"""phone_relay 시험 — 가짜 기기 2대(WebSocket 클라이언트)로 실제 중계를 돌려 본다.
실행: cd server && python3 -m pytest tests/test_phone_relay.py -q
"""
import asyncio
import logging
import os
import sys
from pathlib import Path

import pytest
import pytest_asyncio
from aiohttp import WSMsgType, WSServerHandshakeError
from aiohttp.test_utils import TestClient, TestServer

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import phone_core
import phone_relay

A, B = "tab5-1", "tab5-2"
TOK = {A: "tokenA-0123456789abcdef", B: "tokenB-0123456789abcdef"}
VOICE = b"\x00\x01" + b"OPUSPAYLOAD-" * 5          # 순번 2바이트 + 60바이트


class FakeClock:
    """시험이 시간을 직접 넘긴다(30초 벨 제한·45초 무소식을 기다리지 않으려고)."""

    def __init__(self):
        self.t = 1000.0

    def __call__(self):
        return self.t


@pytest_asyncio.fixture
async def relay():
    clock = FakeClock()
    app = phone_relay.create_app(TOK, clock=clock, tick_interval=0.01)
    async with TestClient(TestServer(app)) as client:
        yield client, clock, app


def hdr(dev, token=None):
    return {"X-Device-Id": dev, "X-Device-Token": TOK[dev] if token is None else token}


async def connect(client, dev):
    return await client.ws_connect("/ws", headers=hdr(dev))


async def rx(ws):
    return await ws.receive_json(timeout=2)


async def assert_silent(ws, secs=0.15):
    with pytest.raises(asyncio.TimeoutError):
        await ws.receive(timeout=secs)


async def pair(client):
    """두 기기를 붙이고 서로의 접속 알림까지 읽어 낸다."""
    a = await connect(client, A)
    assert await rx(a) == {"t": "peer", "online": False}
    b = await connect(client, B)
    assert await rx(b) == {"t": "peer", "online": True}
    assert await rx(a) == {"t": "peer", "online": True}
    return a, b


async def in_call(client):
    a, b = await pair(client)
    await a.send_json({"t": "call"})
    assert await rx(a) == {"t": "ringing"}
    assert await rx(b) == {"t": "ring"}
    await b.send_json({"t": "accept"})
    assert await rx(a) == {"t": "connected"}
    assert await rx(b) == {"t": "connected"}
    return a, b


# ── 생존 확인·인증 ──────────────────────────────────────────────────────────
async def test_healthz_reveals_nothing_but_ok(relay):
    client, _, _ = relay
    r = await client.get("/healthz")
    assert r.status == 200 and await r.json() == {"ok": True}


@pytest.mark.parametrize("headers", [
    {},                                                       # 헤더 없음
    {"X-Device-Id": A},                                       # 토큰 없음
    {"X-Device-Id": A, "X-Device-Token": "wrong-token-0123456789"},
    {"X-Device-Id": A, "X-Device-Token": TOK[B]},             # 남의 토큰
    {"X-Device-Id": "tab5-9", "X-Device-Token": TOK[A]},      # 미등록 기기
])
async def test_bad_credentials_get_401(relay, headers):
    client, _, app = relay
    with pytest.raises(WSServerHandshakeError) as e:
        await client.ws_connect("/ws", headers=headers)
    assert e.value.status == 401
    assert app[phone_relay.EXCHANGE].snapshot()["online"] == []


async def test_token_in_query_string_is_not_accepted(relay):
    """검토 초점 3: 주소창 매개변수로 온 토큰은 읽지 않는다(로그에 남는 자리)."""
    client, _, _ = relay
    with pytest.raises(WSServerHandshakeError) as e:
        await client.ws_connect(f"/ws?id={A}&token={TOK[A]}&X-Device-Id={A}&X-Device-Token={TOK[A]}")
    assert e.value.status == 401


async def test_token_in_query_with_id_in_header_is_not_accepted(relay):
    """2차 검수 2: 기기 ID 는 헤더로 맞게 오고 토큰만 주소창에 실려 와도 받지 않는다."""
    client, _, app = relay
    with pytest.raises(WSServerHandshakeError) as e:
        await client.ws_connect(f"/ws?token={TOK[A]}&X-Device-Token={TOK[A]}", headers={"X-Device-Id": A})
    assert e.value.status == 401
    assert app[phone_relay.EXCHANGE].snapshot()["online"] == []


async def test_auth_fail_log_survives_garbage_ip_header(relay, caplog):
    """2차 검수 3: 인증 전 헤더(CF-Connecting-IP)가 깨진 글자·긴 값이어도 거절 기록은 깨끗한 한 줄로 남는다."""
    client, _, _ = relay
    caplog.set_level(logging.WARNING)
    reader, writer = await asyncio.open_connection(client.server.host, client.server.port)
    writer.write(b"GET /ws HTTP/1.1\r\nHost: x\r\nCF-Connecting-IP: \xff\xfe" + b"A" * 300 + b"\r\n"
                 b"X-Device-Id: tab5-1\r\nX-Device-Token: wrong-token-0123456789\r\n\r\n")
    await writer.drain()
    status = await asyncio.wait_for(reader.readline(), 2)
    writer.close()
    assert status.startswith(b"HTTP/1.1 401"), status
    msgs = [r.getMessage() for r in caplog.records if "auth_fail" in r.getMessage()]
    assert len(msgs) == 1 and msgs[0].isascii() and len(msgs[0]) < 100, msgs


async def test_undecodable_token_header_gets_401_not_500(relay):
    """검수 #1: 토큰 헤더가 깨진 바이트여도 서버 오류(500)가 아니라 401 로 거절한다."""
    client, _, _ = relay
    reader, writer = await asyncio.open_connection(client.server.host, client.server.port)
    writer.write(b"GET /ws HTTP/1.1\r\nHost: x\r\nX-Device-Id: tab5-1\r\nX-Device-Token: \xff\xfe\r\n"
                 b"Connection: Upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 13\r\n"
                 b"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n")
    await writer.drain()
    status = await asyncio.wait_for(reader.readline(), 2)
    writer.close()
    assert status.startswith(b"HTTP/1.1 401"), status


async def test_compression_is_not_negotiated(relay):
    """Opus 60바이트 프레임은 압축 이득이 없다 — 기기가 압축을 제안해도 받지 않는다."""
    client, _, _ = relay
    ws = await client.ws_connect("/ws", headers=hdr(A), compress=15)
    assert not ws.compress


# ── 걸기·받기·거절·무응답·동시 발신 ─────────────────────────────────────────
async def test_call_accept_voice_hangup(relay):
    client, _, _ = relay
    a, b = await in_call(client)
    await a.send_bytes(VOICE)
    m = await b.receive(timeout=2)
    assert m.type == WSMsgType.BINARY and m.data == VOICE     # 내용을 건드리지 않고 그대로
    await b.send_bytes(VOICE[::-1])
    assert (await a.receive(timeout=2)).data == VOICE[::-1]
    await a.send_json({"t": "hangup"})
    assert await rx(a) == {"t": "ended", "why": "hangup"}
    assert await rx(b) == {"t": "ended", "why": "hangup"}


async def test_reject(relay):
    client, _, _ = relay
    a, b = await pair(client)
    await a.send_json({"t": "call"})
    await rx(a), await rx(b)
    await b.send_json({"t": "reject"})
    assert await rx(a) == {"t": "ended", "why": "reject"}
    assert await rx(b) == {"t": "ended", "why": "reject"}


async def test_relay_reject_with_msg(relay, caplog):
    """수신거부 메시지: 실제 WebSocket 으로 양쪽이 문구 번호를 받고, 통화 기록 줄에는 번호가 없다."""
    client, _, _ = relay
    caplog.set_level(logging.INFO)
    a, b = await pair(client)
    await a.send_json({"t": "call"})
    await rx(a), await rx(b)
    await b.send_json({"t": "reject", "msg": 2})
    assert await rx(a) == {"t": "ended", "why": "reject", "msg": 2}
    assert await rx(b) == {"t": "ended", "why": "reject", "msg": 2}
    ends = [r.getMessage() for r in caplog.records if "call_end" in r.getMessage()]
    assert len(ends) == 1 and '"why":"reject"' in ends[0] and "msg" not in ends[0]


async def test_relay_reject_with_wrong_msg_is_plain(relay):
    """번호 자리에 글자를 실어 보내도 건네지 않는다(DM-D1: 문구는 번호로만) — 거절은 그대로 된다."""
    client, _, app = relay
    a, b = await pair(client)
    await a.send_json({"t": "call"})
    await rx(a), await rx(b)
    await b.send_json({"t": "reject", "msg": "회의 중이에요"})
    assert await rx(a) == {"t": "ended", "why": "reject"}
    assert await rx(b) == {"t": "ended", "why": "reject"}
    assert app[phone_relay.EXCHANGE].stats["bad_signal"] == 0


async def test_no_answer_times_out_after_30s(relay):
    client, clock, _ = relay
    a, b = await pair(client)
    await a.send_json({"t": "call"})
    await rx(a), await rx(b)
    clock.t += 29.0
    await assert_silent(a)
    clock.t += 1.5
    assert await rx(a) == {"t": "ended", "why": "timeout"}
    assert await rx(b) == {"t": "ended", "why": "timeout"}


async def test_simultaneous_call_connects(relay):
    client, _, _ = relay
    a, b = await pair(client)
    await a.send_json({"t": "call"})
    assert await rx(a) == {"t": "ringing"}
    assert await rx(b) == {"t": "ring"}
    await b.send_json({"t": "call"})                          # 벨 울리는 쪽도 걸었다 → 바로 연결
    assert await rx(a) == {"t": "connected"}
    assert await rx(b) == {"t": "connected"}


async def test_call_to_offline_peer(relay):
    client, _, _ = relay
    a = await connect(client, A)
    assert await rx(a) == {"t": "peer", "online": False}
    await a.send_json({"t": "call"})
    assert await rx(a) == {"t": "ended", "why": "offline"}


async def test_ping_pong(relay):
    client, _, _ = relay
    a = await connect(client, A)
    await rx(a)
    await a.send_json({"t": "ping", "n": 42})
    assert await rx(a) == {"t": "pong", "n": 42}


# ── 음성 폐기 ───────────────────────────────────────────────────────────────
async def test_voice_outside_call_is_not_forwarded(relay):
    client, _, _ = relay
    a, b = await pair(client)
    await a.send_bytes(VOICE)
    await assert_silent(b)


async def test_oversized_voice_frame_is_dropped(relay):
    client, _, app = relay
    a, b = await in_call(client)
    await a.send_bytes(b"\x00\x02" + b"x" * 399)              # 401바이트 → 버림
    await a.send_bytes(VOICE)                                 # 다음 정상 프레임은 통과
    assert (await b.receive(timeout=2)).data == VOICE
    assert app[phone_relay.EXCHANGE].stats["voice_drop_size"] == 1


async def test_voice_is_dropped_when_peer_is_backlogged(relay, monkeypatch):
    """검토 초점 4: 상대 쪽 송신 대기열이 한도를 넘으면 쌓지 않고 버린다(지연 누적 방지)."""
    client, _, app = relay
    a, b = await in_call(client)
    monkeypatch.setattr(phone_core, "MAX_PEER_BUFFER_BYTES", -1)   # 대기열 0바이트도 "밀림"으로 보게 해 경로를 시험
    await a.send_bytes(VOICE)
    await assert_silent(b)
    assert app[phone_relay.EXCHANGE].stats["voice_drop_backlog"] == 1
    monkeypatch.undo()
    await a.send_bytes(VOICE)
    assert (await b.receive(timeout=2)).data == VOICE


async def test_garbage_frames_do_not_break_the_connection(relay):
    """검토 초점 2: 깨진 글자·빈 이진 프레임 뒤에도 연결과 상태가 멀쩡하다."""
    client, _, app = relay
    a, b = await pair(client)
    await a.send_str("not json at all")
    await a.send_str('{"t":"dance"}')
    await a.send_bytes(b"")
    await a.send_json({"t": "ping", "n": 1})
    assert await rx(a) == {"t": "pong", "n": 1}
    await assert_silent(b)
    stats = app[phone_relay.EXCHANGE].stats
    assert stats["bad_signal"] == 2 and stats["voice_drop_size"] == 1 and stats["voice_fwd"] == 0


# ── 연결 끊김·재접속·무소식 ─────────────────────────────────────────────────
async def test_disconnect_in_call_tells_peer_lost(relay):
    client, _, _ = relay
    a, b = await in_call(client)
    await b.close()
    assert await rx(a) == {"t": "ended", "why": "lost"}
    assert await rx(a) == {"t": "peer", "online": False}


async def test_same_id_reconnect_replaces_old_connection(relay):
    """검토 초점 1: 재접속하면 옛 연결은 닫히고, 새 연결은 바로 쓸 수 있다."""
    client, _, app = relay
    a, b = await pair(client)
    a2 = await connect(client, A)
    assert await rx(b) == {"t": "peer", "online": False}
    assert await rx(a2) == {"t": "peer", "online": True}
    assert await rx(b) == {"t": "peer", "online": True}
    assert (await a.receive(timeout=2)).type in (WSMsgType.CLOSE, WSMsgType.CLOSING, WSMsgType.CLOSED)
    await asyncio.sleep(0.05)                                 # 옛 연결의 뒤늦은 이탈 처리가 끝나도
    assert app[phone_relay.EXCHANGE].snapshot()["online"] == [A, B]   # 새 연결은 그대로
    await a2.send_json({"t": "call"})
    assert await rx(a2) == {"t": "ringing"}
    assert await rx(b) == {"t": "ring"}


async def test_frames_from_replaced_connection_are_ignored(relay, monkeypatch):
    """검수 #3: 재접속으로 밀려난 옛 연결이 아직 안 닫혔어도, 거기서 온 신호는 그 기기의 것으로 치지 않는다."""
    client, _, app = relay

    async def never_close(conn):
        pass

    monkeypatch.setattr(phone_relay, "_safe_close", never_close)   # 옛 연결 닫기가 늦어지는 상황(반쯤 죽은 소켓) 흉내
    a, b = await pair(client)
    a2 = await connect(client, A)
    assert await rx(b) == {"t": "peer", "online": False}
    assert await rx(a2) == {"t": "peer", "online": True}
    assert await rx(b) == {"t": "peer", "online": True}
    await a.send_json({"t": "call"})                               # 옛 연결에서 온 신호
    await assert_silent(b)                                         # B 에 벨이 울리면 안 된다
    assert app[phone_relay.EXCHANGE].state == phone_core.IDLE


async def test_failure_during_attach_leaves_no_ghost_connection(relay, monkeypatch):
    """검수 #8: 접속 직후 전송이 터져도 그 기기가 '접속 중'으로 남지 않는다(유령 연결 없음)."""
    client, _, app = relay

    async def boom(conn, text):
        raise OSError("boom")

    monkeypatch.setattr(phone_relay, "_safe_send", boom)
    try:
        ws = await client.ws_connect("/ws", headers=hdr(A))
        await ws.receive(timeout=2)
    except Exception:                                              # noqa: BLE001 — 연결이 어떤 식으로 끊기든 상관없다
        pass
    await asyncio.sleep(0.05)
    assert app[phone_relay.EXCHANGE].snapshot()["online"] == []


async def test_frame_over_4096_bytes_closes_the_sender(relay):
    """한도(WS_MAX_MSG_BYTES)를 넘는 프레임을 보낸 연결은 닫히고, 통화 상대는 lost 를 받는다."""
    client, _, _ = relay
    a, b = await in_call(client)
    await a.send_bytes(b"x" * 5000)
    assert await rx(b) == {"t": "ended", "why": "lost"}
    assert await rx(b) == {"t": "peer", "online": False}


async def test_silent_device_is_closed_after_45s(relay):
    client, clock, _ = relay
    a, b = await pair(client)
    clock.t += 40
    await b.send_json({"t": "ping", "n": 1})                  # B 만 소식을 보낸다
    assert await rx(b) == {"t": "pong", "n": 1}
    clock.t += 6                                              # A 는 46초째 무소식
    assert await rx(b) == {"t": "peer", "online": False}
    assert (await a.receive(timeout=2)).type in (WSMsgType.CLOSE, WSMsgType.CLOSING, WSMsgType.CLOSED)


# ── 기록에 비밀이 남지 않는다 ───────────────────────────────────────────────
async def test_logs_contain_no_token_and_no_voice(relay, caplog):
    """검토 초점 3: 토큰·음성 내용은 기록에 없다. 통화 길이·사유는 있다."""
    client, clock, _ = relay
    caplog.set_level(logging.DEBUG)
    with pytest.raises(WSServerHandshakeError):
        await client.ws_connect("/ws", headers=hdr(A, "wrong-token-SHOULD-NOT-APPEAR"))
    a, b = await in_call(client)
    await a.send_bytes(VOICE)
    await b.receive(timeout=2)
    clock.t += 12.0
    await a.send_json({"t": "hangup"})
    await rx(a), await rx(b)
    text = caplog.text
    for secret in (TOK[A], TOK[B], "wrong-token-SHOULD-NOT-APPEAR", "OPUSPAYLOAD"):
        assert secret not in text
    assert '"why":"hangup"' in text and '"talk_s":12.0' in text
    assert "auth_fail" in text


def clear_env(monkeypatch, *names):
    """환경변수를 비우되, 시험이 끝나면 원래대로 돌아가게 한다(load_env_file 이 넣은 값이 다른 시험에 새지 않게)."""
    for n in names:
        monkeypatch.setenv(n, "x")
        monkeypatch.delenv(n)


def test_load_env_file_sets_defaults_only(tmp_path, monkeypatch):
    f = tmp_path / ".env.phone"
    f.write_text("# 주석\nPORT=5094\nPHONE_TOKENS=x:1\n\n잘못된줄\n", encoding="utf-8")
    clear_env(monkeypatch, "PHONE_TOKENS")
    monkeypatch.setenv("PORT", "6000")
    phone_relay.load_env_file(f)
    assert os.environ["PORT"] == "6000"                       # 이미 있는 값은 그대로
    assert os.environ["PHONE_TOKENS"] == "x:1"
    phone_relay.load_env_file(tmp_path / "없는파일")           # 없어도 오류 없음


def test_load_env_file_strips_quotes(tmp_path, monkeypatch):
    """검수 #8: 값을 따옴표로 감싸 적어도 같은 값으로 읽는다(안 벗기면 인증이 전부 실패한다)."""
    f = tmp_path / ".env.phone"
    f.write_text('PHONE_TOKENS="a:1,b:2"\nPORT=\'5094\'\n', encoding="utf-8")
    clear_env(monkeypatch, "PHONE_TOKENS", "PORT")
    phone_relay.load_env_file(f)
    assert os.environ["PHONE_TOKENS"] == "a:1,b:2" and os.environ["PORT"] == "5094"


def run_main(monkeypatch, **env):
    """main() 을 실제 서버 없이 돌려 web.run_app 에 넘긴 설정을 돌려준다."""
    seen = {}
    monkeypatch.setattr(phone_relay, "load_env_file", lambda path: None)
    clear_env(monkeypatch, "BIND", "PORT")
    monkeypatch.setenv("PHONE_TOKENS", f"{A}:{TOK[A]},{B}:{TOK[B]}")
    for k, v in env.items():
        monkeypatch.setenv(k, v)
    monkeypatch.setattr(phone_relay.web, "run_app", lambda app, **kw: seen.update(kw))
    phone_relay.main()
    return seen


def test_main_binds_loopback_and_disables_access_log(monkeypatch):
    """검수 #6: 기본 수신 주소는 내부 전용, 접근 로그(요청 줄이 남는 자리)는 꺼져 있다."""
    seen = run_main(monkeypatch)
    assert seen["host"] == "127.0.0.1" and seen["port"] == 5094
    assert seen["access_log"] is None


def test_main_rejects_bad_port_with_a_plain_message(monkeypatch):
    """검수 #8: PORT 가 숫자가 아니면 트레이스백이 아니라 설정 오류 한 줄로 끝난다."""
    with pytest.raises(SystemExit) as e:
        run_main(monkeypatch, PORT="abc")
    assert "PORT" in str(e.value)


# ── 종료 훅 (P4N-D5) ────────────────────────────────────────────────────────
async def closed_with(ws):
    """연결이 닫힐 때까지 읽어 닫기 코드를 돌려준다(그 사이 오는 글자 프레임은 건너뛴다)."""
    while True:
        m = await ws.receive(timeout=3)
        if m.type == WSMsgType.CLOSE:
            return m.data
        assert m.type == WSMsgType.TEXT, m.type


async def test_shutdown_closes_attached_devices_with_going_away(relay):
    """교환기가 내려갈 때 붙어 있는 기기에 닫기(1001)를 보낸다 — 기기가 15~20초 뒤가 아니라 바로 안다."""
    client, _, app = relay
    a, b = await pair(client)
    await app.shutdown()
    assert await closed_with(a) == 1001
    assert await closed_with(b) == 1001


async def test_shutdown_in_call_gives_peer_lost_then_going_away(relay):
    """통화 중 종료: 규약(§9.4)대로 상대에게 ended:lost 가 먼저 가고(통화 중 이탈과 같은 신호), 이어 1001 로 닫힌다."""
    client, _, app = relay
    a, b = await in_call(client)
    await app.shutdown()
    seen = {}
    for name, ws in (("a", a), ("b", b)):
        texts = []
        while True:
            m = await ws.receive(timeout=3)
            if m.type == WSMsgType.CLOSE:
                seen[name] = m.data
                break
            texts.append(m.json())
        # 어느 쪽이든 ended:lost 를 최대 한 번만 받고, 그 밖의 통화 신호는 없다
        assert [t for t in texts if t.get("t") != "peer"] in ([], [{"t": "ended", "why": "lost"}]), texts
    assert seen == {"a": 1001, "b": 1001}
    assert app[phone_relay.EXCHANGE].snapshot()["state"] == phone_core.IDLE


async def test_shutdown_with_nobody_attached_is_quiet(relay):
    client, _, app = relay
    await app.shutdown()


# ── 인증 실패 기록 묶기 (P4N-D6) ────────────────────────────────────────────
async def test_auth_fail_log_is_rate_limited(relay, caplog):
    """인증 실패가 몰려도 기록은 창(10초)당 한 줄 — 나머지는 세어 두었다가 다음 줄에 "(+N 생략)"으로 붙인다."""
    client, clock, _ = relay
    caplog.set_level(logging.WARNING)
    for _ in range(25):
        with pytest.raises(WSServerHandshakeError):
            await client.ws_connect("/ws", headers=hdr(A, "wrong-token-0123456789"))
    lines = [r.getMessage() for r in caplog.records if "auth_fail" in r.getMessage()]
    assert len(lines) == 1, lines
    clock.t += 11.0
    with pytest.raises(WSServerHandshakeError):
        await client.ws_connect("/ws", headers=hdr(A, "wrong-token-0123456789"))
    lines = [r.getMessage() for r in caplog.records if "auth_fail" in r.getMessage()]
    assert len(lines) == 2 and "+24" in lines[1], lines
