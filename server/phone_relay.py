#!/usr/bin/env python3
"""phone_relay.py — 전화 교환기 (aiohttp WebSocket 중계). 판단은 전부 phone_core.Exchange 가 한다.

설계: docs/exec-plans/2026-10-08-phone-3way-design.md §6·§7 (신호 규약 v2) — 옛 규약은 2026-10-03-tab5-phone.md §9.3·§9.4
  GET /healthz   생존 확인 ({"ok": true} 만 — 접속·통화 상태는 내보내지 않는다)
  GET /ws        기기 접속. 헤더 X-Device-Id / X-Device-Token (주소창 매개변수는 읽지 않는다)
                 헤더 X-Phone-Proto: 2 를 더 보내면 v2 연결(상대 지정·건 기기 표시), 없거나 그 밖이면 v1 연결(옛 펌웨어) — TW-D7
설정: server/.env.phone (git 제외)
  PHONE_TOKENS=tab5-1:<토큰>,tab5-2:<토큰>,stackchan-2:<토큰>   기기 2~8대. 적힌 순서가 뜻을 가진다(아래)
  PHONE_LEGACY_PAIR=tab5-1,tab5-2   (선택) 옛 짝 = v1 연결끼리의 상대. 없으면 PHONE_TOKENS 에 적힌 순서의 앞 두 대
  PORT=5094  BIND=127.0.0.1
실행: python3 server/phone_relay.py
기록: 시각·기기·통화 길이·종료 사유만. 음성과 토큰은 어디에도 쓰지 않는다.
"""
import asyncio
import json
import logging
import os
import time
from pathlib import Path

from aiohttp import WSCloseCode, WSMsgType, web

import phone_core

log = logging.getLogger("phone_relay")
WS_MAX_MSG_BYTES = 4096      # 이보다 큰 프레임을 보내는 연결은 aiohttp 가 닫는다(정상 음성은 400바이트 이하)
WS_CLOSE_TIMEOUT_S = 2.0     # 죽은 연결을 닫을 때 상대의 응답을 기다리는 상한

EXCHANGE = web.AppKey("exchange", phone_core.Exchange)
TOKENS = web.AppKey("tokens", dict)
CLOCK = web.AppKey("clock", object)
TICK_INTERVAL = web.AppKey("tick_interval", float)
BG_TASKS = web.AppKey("bg_tasks", set)
AUTH_LOG = web.AppKey("auth_log", dict)
AUTH_LOG_WINDOW_S = 10.0     # P4N-D6: 인증 실패 기록은 이 창당 한 줄(터널 앞에서 두드리는 봇이 로그를 채우지 않게). 비용: 창 안의 개별 접속 주소는 남지 않는다 — 건수만 다음 줄에. 탈출구: 0 으로 두면 매번 기록


class Conn:
    """기기 연결 1개: WebSocket 과 그 아래 전송로(송신 대기열 크기를 보려고 함께 쥔다)."""
    __slots__ = ("ws", "transport")

    def __init__(self, ws, transport):
        self.ws, self.transport = ws, transport


def load_env_file(path):
    """KEY=VALUE 줄을 환경변수로 올린다(이미 있으면 그대로). 값은 출력하지 않는다."""
    p = Path(path)
    if not p.exists():
        return
    for line in p.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        v = v.strip()
        if len(v) >= 2 and v[0] == v[-1] and v[0] in "\"'":
            v = v[1:-1]                          # 따옴표로 감싼 값도 같은 값으로 읽는다
        os.environ.setdefault(k.strip(), v)


def _spawn(app, coro):
    """기다리지 않고 실행(죽은 연결 닫기 등). 참조를 쥐고 있어야 도중에 수거되지 않는다."""
    task = asyncio.ensure_future(coro)
    app[BG_TASKS].add(task)
    task.add_done_callback(app[BG_TASKS].discard)


async def _safe_send(conn, text):
    try:
        await conn.ws.send_str(text)
    except (ConnectionError, RuntimeError):
        pass                                     # 닫히는 중인 연결 — 이탈 처리는 수신 루프가 한다


async def _safe_close(conn):
    try:
        await conn.ws.close()
    except Exception:                            # noqa: BLE001 — 이미 죽은 연결을 닫다 나는 오류는 버린다
        pass


async def apply_actions(app, actions):
    """phone_core 가 돌려준 할 일 목록을 실제 연결에 반영한다."""
    ex = app[EXCHANGE]
    for act in actions:
        kind = act[0]
        if kind == "send":
            conn = ex.conn_of(act[1])
            if conn is not None:
                await _safe_send(conn, json.dumps(act[2], separators=(",", ":")))
        elif kind == "close":
            _spawn(app, _safe_close(act[1]))
        elif kind == "log":
            log.info("%s", json.dumps(act[1], ensure_ascii=False, separators=(",", ":")))


def _client_ip(request):
    """기록용 접속 주소. 터널 뒤에서는 CF-Connecting-IP — 인증 전 헤더라 위조·깨진 글자·긴 값이 올 수 있어 ASCII 64자로 다듬는다."""
    raw = request.headers.get("CF-Connecting-IP") or request.remote or "-"
    return ascii(raw)[1:-1][:64]


def _log_auth_fail(app, request, t):
    """인증 실패 기록을 창당 한 줄로 묶는다. 창이 지난 뒤 첫 실패 줄에 그동안 생략한 건수를 붙인다."""
    st = app[AUTH_LOG]
    if st["since"] >= 0 and t - st["since"] < AUTH_LOG_WINDOW_S:
        st["skipped"] += 1
        return
    extra = f" (+{st['skipped']} 생략)" if st["skipped"] else ""
    st["since"], st["skipped"] = t, 0
    log.warning("auth_fail ip=%s%s", _client_ip(request), extra)


async def healthz(request):
    return web.json_response({"ok": True})


async def ws_handler(request):
    app = request.app
    ex, now = app[EXCHANGE], app[CLOCK]
    dev = request.headers.get("X-Device-Id")
    token = request.headers.get("X-Device-Token")
    if not phone_core.check_token(app[TOKENS], dev, token):
        _log_auth_fail(app, request, now())
        return web.Response(status=401, text="unauthorized")

    # compress=False: Opus 60바이트 프레임은 압축 이득이 없다(기기가 제안해도 받지 않는다)
    ws = web.WebSocketResponse(max_msg_size=WS_MAX_MSG_BYTES, timeout=WS_CLOSE_TIMEOUT_S, compress=False)
    await ws.prepare(request)
    conn = Conn(ws, request.transport)
    # TW-D7: 규약 버전은 인증을 통과한 뒤에 읽는다(인증 전 헤더로는 아무것도 가르지 않는다). 값은 기록하지 않는다.
    proto = phone_core.proto_from_header(request.headers.get("X-Phone-Proto"))
    try:
        await apply_actions(app, ex.attach(dev, conn, now(), proto))   # try 안: 여기서 터져도 finally 가 접속 기록을 지운다
        async for msg in ws:
            if ex.conn_of(dev) is not conn:
                break                            # 재접속으로 밀려난 옛 연결 — 여기서 온 프레임은 그 기기의 것으로 치지 않는다
            if msg.type == WSMsgType.TEXT:
                await apply_actions(app, ex.on_text(dev, msg.data, now()))
            elif msg.type == WSMsgType.BINARY:
                peer = ex.route_voice(dev, len(msg.data), now())
                peer_conn = ex.conn_of(peer) if peer else None
                if peer_conn is None:
                    continue
                if peer_conn.transport is not None and phone_core.should_drop_voice(peer_conn.transport.get_write_buffer_size()):
                    ex.stats["voice_drop_backlog"] += 1       # PL-D3: 밀린 상대에겐 쌓지 않고 버린다
                    continue
                try:
                    await peer_conn.ws.send_bytes(msg.data)
                except (ConnectionError, RuntimeError):
                    pass
            elif msg.type == WSMsgType.ERROR:
                break
    finally:
        await apply_actions(app, ex.detach(dev, conn, now()))
    return ws


async def _ticker(app):
    ex, now = app[EXCHANGE], app[CLOCK]
    while True:
        await asyncio.sleep(app[TICK_INTERVAL])
        try:
            await apply_actions(app, ex.tick(now()))
        except Exception:                        # noqa: BLE001 — 시계 태스크가 죽으면 벨 제한·무소식 끊기가 멈춘다
            log.exception("tick failed")


async def _ticker_ctx(app):
    task = asyncio.ensure_future(_ticker(app))
    yield
    task.cancel()
    for t in list(app[BG_TASKS]):
        t.cancel()
    await asyncio.gather(task, *app[BG_TASKS], return_exceptions=True)


async def _on_shutdown(app):
    """P4N-D5: 교환기가 내려갈 때 붙어 있는 기기를 닫기(1001 = 서버가 떠남)로 보낸다.
    이유: 안 닫으면 기기가 ping 무응답(15~20초)이 돼서야 알고, 그동안 통화 중이면 말이 허공으로 간다.
    통화 중이던 상대에게는 detach 가 평소처럼 ended:lost 를 보낸다(§9.4 와 같은 신호 — 새 신호를 만들지 않는다).
    비용: 죽은 연결은 WS_CLOSE_TIMEOUT_S 만큼 종료가 늦는다(병렬이라 최대 그만큼). 탈출구: 이 훅을 빼면 예전 동작."""
    async def close(conn):
        try:
            await conn.ws.close(code=WSCloseCode.GOING_AWAY, message=b"relay shutdown")
        except Exception:                        # noqa: BLE001 — 이미 죽은 연결의 오류는 버린다
            pass
    await asyncio.gather(*(close(c) for c in list(app[EXCHANGE].conns.values())))


def create_app(tokens, clock=time.monotonic, tick_interval=1.0, legacy_pair=None):
    """tokens: {기기ID: 토큰}(적힌 순서 유지). legacy_pair: 옛 짝(기기ID 둘) — None 이면 tokens 의 앞 두 대(§6.1)."""
    app = web.Application()
    app[TOKENS] = dict(tokens)
    app[EXCHANGE] = phone_core.Exchange(tokens.keys(), legacy_pair)
    app[CLOCK] = clock
    app[TICK_INTERVAL] = tick_interval
    app[BG_TASKS] = set()
    app[AUTH_LOG] = {"since": -1.0, "skipped": 0}
    app.router.add_get("/healthz", healthz)
    app.router.add_get("/ws", ws_handler)
    app.on_shutdown.append(_on_shutdown)
    app.cleanup_ctx.append(_ticker_ctx)
    return app


def main():
    load_env_file(Path(__file__).resolve().parent / ".env.phone")
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(message)s")
    try:
        tokens = phone_core.parse_tokens(os.environ.get("PHONE_TOKENS", ""))
        legacy_pair = phone_core.parse_legacy_pair(os.environ.get("PHONE_LEGACY_PAIR", ""), tokens)
    except ValueError as e:
        raise SystemExit(f"[phone_relay] 설정 오류: {e}")
    host = os.environ.get("BIND", "127.0.0.1")   # PL-D4: 기본은 내부 전용. LAN 시험 때만 BIND=0.0.0.0
    try:
        port = int(os.environ.get("PORT", "5094"))
    except ValueError:
        raise SystemExit("[phone_relay] 설정 오류: PORT 는 숫자여야 함")
    # 기기 ID 는 적힌 순서대로 찍는다(옛 짝 기본값의 근거) — ID 는 비밀이 아니고, 토큰은 찍지 않는다.
    log.info("listening http://%s:%d devices=%s legacy_pair=%s", host, port, ",".join(tokens), ",".join(legacy_pair))
    web.run_app(create_app(tokens, legacy_pair=legacy_pair), host=host, port=port, access_log=None, print=None)


if __name__ == "__main__":
    main()
