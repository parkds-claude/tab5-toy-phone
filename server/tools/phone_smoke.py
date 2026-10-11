#!/usr/bin/env python3
"""phone_smoke.py — 떠 있는 교환기에 가짜 기기 2~3대로 붙어 걸기→받기→음성→끊기를 실제 소켓으로 확인한다.

  python3 tools/phone_smoke.py                       # http://127.0.0.1:5094 — 짧은 확인(음성 50프레임 왕복 echo)
  python3 tools/phone_smoke.py wss://phone.example.com/ws --stream      # Phase 4: 터널을 거친 30초 통화 흉내
  python3 tools/phone_smoke.py wss://phone.example.com/ws --stream 60   # 60초
  python3 tools/phone_smoke.py http://127.0.0.1:5095 --three                # 3대 시나리오(신호 규약 v2)
주소는 http(s)://·ws(s)://, 끝의 /ws 는 있어도 없어도 된다. 토큰·옛 짝은 server/.env.phone(PHONE_TOKENS·PHONE_LEGACY_PAIR)에서 읽는다(출력하지 않음).

2대 시나리오(기본·--stream)는 **옛 짝** 두 대가 **옛 규약(v1, 헤더 없음)** 으로 붙는다 — 등록 기기가 3대여도 그대로 돌고,
  그 자체로 "교환기를 v2 로 올린 뒤에도 옛 펌웨어끼리 통화된다"(설계서 2026-10-08-phone-3way-design.md TW-D7)의 확인이다.

--three (설계서 §7·§12): 옛 짝 둘 + 옛 짝에 들지 않은 첫 등록 기기(스택짱)로 붙어 차례로 본다.
  ① v2 연결 셋이 접속 직후 다른 기기 전부의 상태를 기기 ID 와 함께 받는다  ② 고른 상대에게만 벨이 간다(건 기기가 from 에, 나머지는 조용)
  ③ 벨 중·통화 중인 상대에게 걸면 busy  ④ 음성은 그 통화의 둘 사이에서만, ended 도 둘에게만
  ⑤ v1/v2 섞임: 옛 짝의 첫째가 헤더 없이(v1) 다시 붙으면 셋째에게 꺼짐으로 보이고(걸면 offline), 옛 짝끼리는 옛 모양({"t":"ring"})으로 통화된다.
  등록 기기가 3대 미만이면 실행하지 않는다. 끝에 THREE OK.

--stream (P4N-D7): 가짜 기기 둘이 걸고 받은 뒤 20ms 간격으로 음성 크기(40~130바이트)의 바이너리 묶음을 양쪽으로 동시에 흘리고
  끊는다. 숫자로 낸다: 방향별 보낸/받은/손실/순서 뒤바뀜/중복, 편도 지연(중앙값·90%·최대 — 같은 프로세스 시계라 정확),
  ping 왕복(중앙값·90%·최대 — 터널·교환기 왕복). 입→귀 지연의 망 몫 어림으로 쓴다(기기 내부 버퍼는 따로 더해야 함).

!! 주의: 진짜 기기 ID(tab5-1·tab5-2, --three 는 stackchan-2 까지)·토큰으로 붙는다. 그 교환기에 이미 붙어 있는 진짜 기기는 "같은 ID 재접속"
   규칙으로 쫓겨나고, 통화 중이면 lost 로 끝난다(쫓겨난 기기가 몇 초 뒤 다시 붙으면 이번에는 가짜 기기가 밀려 시나리오가 깨진다).
   → **기기가 그 교환기에 붙어 있지 않을 때만** 쓴다(기기 전원을 끄거나 아직 그 주소로 안 옮긴 때).
   운영 교환기를 바꾸기 전의 예행연습은 새 코드를 빈 포트에 따로 띄워 거기에 한다(server/deploy/README.md "교환기 v2 로 올리기").
"""
import asyncio
import contextlib
import json
import os
import random
import statistics
import sys
import time
from pathlib import Path
from urllib.parse import urlsplit

import aiohttp

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import phone_core
import phone_relay


async def expect(ws, want):
    got = await ws.receive_json(timeout=5)
    if got != want:
        raise AssertionError(f"기대 {want} / 실제 {got}")


def normalize_base(raw):
    """사용자가 준 주소(ws(s)://·http(s)://, 끝의 /ws 유무) → (healthz 용 http 기준 주소, /ws 주소). aiohttp 는 ws_connect 에 http(s) 주소를 받는다."""
    u = urlsplit(raw)
    scheme = {"ws": "http", "wss": "https"}.get(u.scheme, u.scheme)
    path = u.path.rstrip("/")
    if path.endswith("/ws"):
        path = path[:-3]
    http = f"{scheme}://{u.netloc}{path}"
    return http, http + "/ws"


def _config(tokens=None, legacy_pair=None):
    """시나리오에 쓸 (토큰 표, 옛 짝). tokens 를 안 주면 server/.env.phone·환경변수에서 읽는다(교환기와 같은 규칙 — phone_core).
    tokens 를 직접 주면(시험) 환경은 읽지 않고, legacy_pair 가 없으면 tokens 의 앞 두 대."""
    if tokens is None:
        phone_relay.load_env_file(Path(__file__).resolve().parent.parent / ".env.phone")
        tokens = phone_core.parse_tokens(os.environ.get("PHONE_TOKENS", ""))
        if legacy_pair is None:
            return tokens, phone_core.parse_legacy_pair(os.environ.get("PHONE_LEGACY_PAIR", ""), tokens)
    return tokens, phone_core.parse_legacy_pair(",".join(legacy_pair or ()), tokens)


def _headers(dev, token, proto=1):
    """접속 헤더. proto 2 면 X-Phone-Proto: 2 (§6.1) — 1 이면 보내지 않는다(옛 펌웨어와 같은 모양)."""
    h = {"X-Device-Id": dev, "X-Device-Token": token}
    if proto == 2:
        h["X-Phone-Proto"] = "2"
    return h


async def expect_quiet(ws, secs, who):
    """secs 동안 그 연결에 아무것도 오지 않아야 한다(고르지 않은 기기에 벨·음성·ended 가 새지 않는지)."""
    try:
        m = await ws.receive(timeout=secs)
    except asyncio.TimeoutError:
        return
    raise AssertionError(f"조용해야 할 기기({who})에 프레임이 옴: {m.type.name} {str(m.data)[:80]}")


async def read_states(ws, me, tokens):
    """v2 연결이 접속 직후 받는 상태 알림을 읽어 {기기ID: 켜짐} 으로. "다른 등록 기기 전부에 대해 하나씩"(§6.3)이 아니면 실패."""
    others = [d for d in tokens if d != me]
    seen = {}
    for _ in others:
        m = await ws.receive_json(timeout=5)
        if m.get("t") != "peer" or not isinstance(m.get("id"), str) or not isinstance(m.get("online"), bool):
            raise AssertionError(f"기기 ID 가 실린 상태 알림이 아님({me} 쪽): {m}")
        seen[m["id"]] = m["online"]
    if set(seen) != set(others):
        raise AssertionError(f"접속 직후 상태 알림이 다른 기기 전부를 덮지 않음({me} 쪽): {sorted(seen)}")
    return seen


def percentile(sorted_vals, q):
    """가장 가까운 순위 방식 백분위수(작은 표본에서 보간하지 않는다)."""
    i = max(0, min(len(sorted_vals) - 1, -(-len(sorted_vals) * q // 100) - 1))
    return sorted_vals[int(i)]


def summarize(vals):
    if not vals:
        return {"n": 0, "median": None, "p90": None, "max": None}
    v = sorted(vals)
    return {"n": len(v), "median": statistics.median(v), "p90": percentile(v, 90), "max": v[-1]}


def count_flow(sent, arrived_seqs):
    """보낸 수와 도착한 순번 목록(도착 순서)으로 손실·순서 뒤바뀜·중복을 센다. 뒤바뀜 = 이미 본 가장 큰 순번보다 작은 순번이 늦게 온 것."""
    seen, top, reordered, dup = set(), -1, 0, 0
    for q in arrived_seqs:
        if q in seen:
            dup += 1
            continue
        seen.add(q)
        if q < top:
            reordered += 1
        top = max(top, q)
    return {"sent": sent, "received": len(seen), "lost": sent - len(seen), "reordered": reordered, "duplicated": dup}


def frame_sizes(n, seed=7):
    """음성 크기를 흉내 낸 바이트 수(순번 2바이트 포함 40~130). 교환기 규칙(3~400)과 PL-D3 근처의 현실적 값."""
    rnd = random.Random(seed)
    return [rnd.randint(40, 130) for _ in range(n)]


async def run_stream(base, seconds=30.0, interval=0.02, tokens=None, out=print, legacy_pair=None):
    """가짜 기기 둘(옛 짝, v1)이 걸고 받은 뒤 `seconds` 동안 양방향 음성 크기 바이너리를 흘리고 끊는다. 숫자 결과(dict)를 돌려주고 out 으로 한글 요약을 낸다."""
    http, url = normalize_base(base)
    tokens, (a_id, b_id) = _config(tokens, legacy_pair)
    a_tok, b_tok = tokens[a_id], tokens[b_id]
    sent_at = {"a": {}, "b": {}}            # 보낸 쪽 → {순번: 보낸 시각}
    arrived = {"a": [], "b": []}            # 받은 쪽 → 도착한 순번(도착 순서)
    lat = {"a": [], "b": []}                # 받은 쪽 → 편도 지연(ms)
    ping_rtt, pending = [], {}
    texts = {"a": asyncio.Queue(), "b": asyncio.Queue()}
    sizes = frame_sizes(int(seconds / interval) + 50)

    async def reader(name, ws):
        async for m in ws:
            now = time.perf_counter()
            if m.type == aiohttp.WSMsgType.BINARY:
                q = int.from_bytes(m.data[:2], "big")
                src = "b" if name == "a" else "a"
                arrived[name].append(q)
                t0 = sent_at[src].get(q)
                if t0 is not None:
                    lat[name].append((now - t0) * 1000)
            elif m.type == aiohttp.WSMsgType.TEXT:
                msg = json.loads(m.data)
                if msg.get("t") == "pong" and msg.get("n") in pending:
                    ping_rtt.append((now - pending.pop(msg["n"])) * 1000)
                else:
                    texts[name].put_nowait(msg)

    async def sender(name, ws, stop_at):
        nxt, seq = time.perf_counter(), 0
        while nxt < stop_at:
            size = sizes[seq % len(sizes)]
            sent_at[name][seq] = time.perf_counter()
            await ws.send_bytes(seq.to_bytes(2, "big") + bytes(size - 2))
            seq += 1
            nxt += interval
            await asyncio.sleep(max(0.0, nxt - time.perf_counter()))
        return seq

    async def pinger(ws, stop_at):
        n = 0
        while time.perf_counter() < stop_at:
            n += 1
            pending[n] = time.perf_counter()
            await ws.send_json({"t": "ping", "n": n})
            await asyncio.sleep(max(0.0, min(2.0, stop_at - time.perf_counter())))   # 남은 시간보다 오래 자지 않는다(끊기가 늦어지지 않게)

    async def next_text(name, want):
        while True:
            got = await asyncio.wait_for(texts[name].get(), 5)
            if got == want:
                return

    async with aiohttp.ClientSession() as s:
        async with s.get(http + "/healthz") as r:
            assert r.status == 200, f"healthz {r.status}"
        async with s.ws_connect(url, headers={"X-Device-Id": a_id, "X-Device-Token": a_tok}) as a:
            await expect(a, {"t": "peer", "online": False})
            async with s.ws_connect(url, headers={"X-Device-Id": b_id, "X-Device-Token": b_tok}) as b:
                await expect(b, {"t": "peer", "online": True})
                await expect(a, {"t": "peer", "online": True})
                await a.send_json({"t": "call"})
                await expect(a, {"t": "ringing"})
                await expect(b, {"t": "ring"})
                await b.send_json({"t": "accept"})
                await expect(a, {"t": "connected"})
                await expect(b, {"t": "connected"})
                readers = [asyncio.ensure_future(reader("a", a)), asyncio.ensure_future(reader("b", b))]
                try:
                    stop_at = time.perf_counter() + seconds
                    out(f"통화 연결됨 — {seconds:g}초 동안 양방향 음성 흉내 전송")
                    sent_a, sent_b, _ = await asyncio.gather(sender("a", a, stop_at), sender("b", b, stop_at), pinger(a, stop_at))
                    await asyncio.sleep(0.5)                           # 오는 길의 묶음이 도착할 시간
                    await a.send_json({"t": "hangup"})
                    await next_text("a", {"t": "ended", "why": "hangup"})
                    await next_text("b", {"t": "ended", "why": "hangup"})
                finally:
                    for t in readers:
                        t.cancel()
                    await asyncio.gather(*readers, return_exceptions=True)
    res = {"a_to_b": {**count_flow(sent_a, arrived["b"]), "latency": summarize(lat["b"])},
           "b_to_a": {**count_flow(sent_b, arrived["a"]), "latency": summarize(lat["a"])},
           "ping": summarize(ping_rtt)}
    f = lambda v: "-" if v is None else f"{v:.1f}ms"
    for key, label in (("a_to_b", f"{a_id}→{b_id}"), ("b_to_a", f"{b_id}→{a_id}")):
        d = res[key]
        out(f"{label}: 보냄 {d['sent']} 받음 {d['received']} 손실 {d['lost']}({100 * d['lost'] / max(1, d['sent']):.1f}%) "
            f"순서 뒤바뀜 {d['reordered']} 중복 {d['duplicated']} | 편도 중앙값 {f(d['latency']['median'])} 90% {f(d['latency']['p90'])} 최대 {f(d['latency']['max'])}")
    p = res["ping"]
    out(f"ping 왕복({p['n']}회): 중앙값 {f(p['median'])} 90% {f(p['p90'])} 최대 {f(p['max'])}")
    return res


async def run(base, tokens=None, legacy_pair=None, out=print):
    """짧은 확인: 옛 짝 둘(v1)이 걸고 받고 음성 50프레임을 왕복시킨 뒤 끊는다."""
    tokens, (a_id, b_id) = _config(tokens, legacy_pair)
    a_tok, b_tok = tokens[a_id], tokens[b_id]
    base = normalize_base(base)[0]
    url = base.rstrip("/") + "/ws"
    async with aiohttp.ClientSession() as s:
        async with s.get(base.rstrip("/") + "/healthz") as r:
            assert r.status == 200, f"healthz {r.status}"
        async with s.ws_connect(url, headers={"X-Device-Id": a_id, "X-Device-Token": a_tok}) as a:
            await expect(a, {"t": "peer", "online": False})
            async with s.ws_connect(url, headers={"X-Device-Id": b_id, "X-Device-Token": b_tok}) as b:
                await expect(b, {"t": "peer", "online": True})
                await expect(a, {"t": "peer", "online": True})
                await a.send_json({"t": "call"})
                await expect(a, {"t": "ringing"})
                await expect(b, {"t": "ring"})
                await b.send_json({"t": "accept"})
                await expect(a, {"t": "connected"})
                await expect(b, {"t": "connected"})
                rtts = []
                for seq in range(50):                                  # 20ms 간격 음성 흉내: A→B 로 보내고 B 가 되돌려 왕복 시간 측정
                    frame = seq.to_bytes(2, "big") + bytes(60)
                    t0 = time.perf_counter()
                    await a.send_bytes(frame)
                    m = await b.receive(timeout=5)
                    assert m.type == aiohttp.WSMsgType.BINARY and m.data == frame, "음성 프레임이 바뀌었거나 오지 않음"
                    await b.send_bytes(m.data)
                    m2 = await a.receive(timeout=5)
                    assert m2.data == frame
                    rtts.append((time.perf_counter() - t0) * 1000)
                    await asyncio.sleep(0.02)
                await a.send_json({"t": "hangup"})
                await expect(a, {"t": "ended", "why": "hangup"})
                await expect(b, {"t": "ended", "why": "hangup"})
    rtts.sort()
    out(f"SMOKE OK  음성 왕복(A→교환기→B→교환기→A) 중앙값 {statistics.median(rtts):.1f}ms, 최대 {rtts[-1]:.1f}ms")


async def run_three(base, tokens=None, legacy_pair=None, out=print, quiet_s=0.5):
    """3대 시나리오(설계서 §7·§12): 고른 상대에게만 벨 · busy · v1/v2 섞임. 어긋나면 AssertionError, 등록 기기가 3대 미만이면 ValueError.
    quiet_s: "조용해야 할 기기"를 지켜보는 시간. 터널 너머 교환기는 왕복이 0.1초를 넘으니 기본을 0.5초로 둔다(시험은 짧게 준다)."""
    tokens, (l1, l2) = _config(tokens, legacy_pair)
    rest = [d for d in tokens if d not in (l1, l2)]
    if not rest:
        raise ValueError("3대 시나리오에는 등록 기기가 3대 이상이어야 함(옛 짝 둘 + 한 대) — PHONE_TOKENS 를 확인")
    x = rest[0]                                               # 옛 짝에 들지 않은 첫 등록 기기(스택짱)
    http, url = normalize_base(base)
    voice = lambda seq: seq.to_bytes(2, "big") + bytes(60)
    peer = lambda dev, on: {"t": "peer", "id": dev, "online": on}

    async with contextlib.AsyncExitStack() as stack:
        s = await stack.enter_async_context(aiohttp.ClientSession())
        async with s.get(http + "/healthz") as r:
            assert r.status == 200, f"healthz {r.status}"

        async def attach(dev, proto):
            return await stack.enter_async_context(s.ws_connect(url, headers=_headers(dev, tokens[dev], proto)))

        async def states(ws, me, want):
            got = await read_states(ws, me, tokens)
            for dev, on in want.items():
                if got[dev] is not on:
                    raise AssertionError(f"{me} 가 본 {dev} 의 상태: 기대 {'켜짐' if on else '꺼짐'} / 실제 {'켜짐' if got[dev] else '꺼짐'}")

        # ① 접속: 셋 다 새 규약(v2) — 접속 직후 다른 기기 전부의 상태를, 이후에는 바뀐 것만 받는다
        a = await attach(l1, 2)
        await states(a, l1, {l2: False, x: False})
        b = await attach(l2, 2)
        await states(b, l2, {l1: True, x: False})
        await expect(a, peer(l2, True))
        c = await attach(x, 2)
        await states(c, x, {l1: True, l2: True})
        await expect(a, peer(x, True))
        await expect(b, peer(x, True))
        out(f"접속: v2 연결 셋({l1}·{l2}·{x})이 서로의 상태를 기기 ID 와 함께 받음")

        # ② 고른 상대에게만 벨
        await c.send_json({"t": "call", "to": l2})
        await expect(c, {"t": "ringing"})
        await expect(b, {"t": "ring", "from": x})
        await expect_quiet(a, quiet_s, l1)
        out(f"고른 상대에게만 벨: {x} → {l2} (건 기기가 from 에 실림, {l1} 은 조용)")

        # ③ busy: 벨 중인 상대, 통화 중인 상대
        await a.send_json({"t": "call", "to": l2})
        await expect(a, {"t": "ended", "why": "busy"})
        await b.send_json({"t": "accept"})
        await expect(c, {"t": "connected"})
        await expect(b, {"t": "connected"})
        await a.send_json({"t": "call", "to": x})
        await expect(a, {"t": "ended", "why": "busy"})
        out(f"busy: {l1} 이 벨 중인 {l2}·통화 중인 {x} 에게 걸면 통화 중")

        # ④ 음성은 그 통화의 둘 사이에서만, 끊으면 ended 도 둘에게만
        for src, dst, seq in ((c, b, 1), (b, c, 2)):
            await src.send_bytes(voice(seq))
            m = await dst.receive(timeout=5)
            assert m.type == aiohttp.WSMsgType.BINARY and m.data == voice(seq), "음성 프레임이 바뀌었거나 오지 않음"
        await c.send_json({"t": "hangup"})
        await expect(c, {"t": "ended", "why": "hangup"})
        await expect(b, {"t": "ended", "why": "hangup"})
        await expect_quiet(a, quiet_s, l1)
        out(f"음성·끊기: {x}↔{l2} 사이에서만 오가고 {l1} 에는 아무것도 가지 않음")

        # ⑤ v1/v2 섞임: 옛 짝의 첫째가 옛 펌웨어(헤더 없음)로 다시 붙는다
        await a.close()
        await expect(b, peer(l1, False))
        await expect(c, peer(l1, False))
        a = await attach(l1, 1)
        await expect(a, {"t": "peer", "online": True})          # v1 은 옛 짝의 상태 하나만, 옛 모양으로
        await expect(b, peer(l1, True))                         # 옛 짝(v2)에게는 켜짐
        await expect_quiet(c, quiet_s, x)                       # 셋째에게는 계속 꺼짐(TW-D8)
        await c.send_json({"t": "call", "to": l1})
        await expect(c, {"t": "ended", "why": "offline"})
        await b.send_json({"t": "call", "to": l1})
        await expect(b, {"t": "ringing"})
        await expect(a, {"t": "ring"})                          # v1 이 받는 벨은 옛 모양(from 없음)
        await a.send_json({"t": "accept"})
        await expect(b, {"t": "connected"})
        await expect(a, {"t": "connected"})
        await a.send_json({"t": "hangup"})
        await expect(b, {"t": "ended", "why": "hangup"})
        await expect(a, {"t": "ended", "why": "hangup"})
        await a.send_json({"t": "call"})                        # v1 의 to 없는 call → 옛 짝에게
        await expect(a, {"t": "ringing"})
        await expect(b, {"t": "ring", "from": l1})
        await expect_quiet(c, quiet_s, x)
        await b.send_json({"t": "reject"})
        await expect(a, {"t": "ended", "why": "reject"})
        await expect(b, {"t": "ended", "why": "reject"})
        out(f"v1/v2 섞임: v1 로 붙은 {l1} 은 {x} 에게 꺼짐(걸면 offline), 옛 짝 {l2} 와는 옛 모양으로 걸고 받음")
    return {"legacy_pair": (l1, l2), "third": x}


DEFAULT_BASE = "http://127.0.0.1:5094"


def parse_cli(args):
    """명령줄 인자 → (주소, 방식, 초). 방식: "short"(기본) · "stream"(--stream [초], 초를 안 주면 30) · "three"(--three). 틀리면 ValueError."""
    args, mode, seconds = list(args), "short", None
    if "--stream" in args:
        k = args.index("--stream")
        nxt = args[k + 1] if k + 1 < len(args) else ""
        seconds, drop = (float(nxt), 2) if nxt and not nxt.startswith("-") and nxt[0].isdigit() else (30.0, 1)
        args, mode = args[:k] + args[k + drop:], "stream"
    if "--three" in args:
        if mode != "short":
            raise ValueError("--stream 과 --three 는 함께 쓰지 않는다")
        args.remove("--three")
        mode = "three"
    if len(args) > 1 or any(a.startswith("-") for a in args):
        raise ValueError("인자가 틀림")
    return (args[0] if args else DEFAULT_BASE), mode, seconds


def main(argv):
    try:
        base, mode, seconds = parse_cli(argv[1:])
    except ValueError:
        sys.exit(__doc__)
    try:
        if mode == "stream":
            asyncio.run(run_stream(base, seconds=seconds))
            print("STREAM OK")
        elif mode == "three":
            asyncio.run(run_three(base))
            print("THREE OK")
        else:
            asyncio.run(run(base))
    except (AssertionError, ValueError, TypeError, aiohttp.ClientError, asyncio.TimeoutError) as e:   # TypeError: 기대한 글자 대신 닫기 프레임이 온 경우
        sys.exit(f"SMOKE FAIL: {type(e).__name__}: {e}")


if __name__ == "__main__":
    main(sys.argv)
