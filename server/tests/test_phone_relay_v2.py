"""phone_relay 신호 규약 v2 시험 — 가짜 기기 3~4대(WebSocket 클라이언트)로 실제 중계를 돌려 본다.
설계: docs/exec-plans/2026-10-08-phone-3way-design.md §6·§7. 접속 헤더 X-Phone-Proto, 환경변수 PHONE_LEGACY_PAIR, 고른 상대에게만 벨, busy, v1·v2 섞임.
2대(v1)의 계약은 tests/test_phone_relay.py 가 그대로 고정한다.
실행: cd server && python3 -m pytest tests/test_phone_relay_v2.py -q
"""
import asyncio
import logging
import sys
from contextlib import asynccontextmanager
from pathlib import Path

import pytest
import pytest_asyncio
from aiohttp import WSMsgType, WSServerHandshakeError
from aiohttp.test_utils import TestClient, TestServer

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import phone_core
import phone_relay
from test_phone_relay import VOICE, FakeClock, assert_silent, clear_env, closed_with, rx

T1, T2, SC, D4 = "tab5-1", "tab5-2", "stackchan-2", "desk-4"
TOK3 = {T1: "tokenA-0123456789abcdef", T2: "tokenB-0123456789abcdef", SC: "tokenC-0123456789abcdef"}
TOK4 = dict(TOK3, **{D4: "tokenD-0123456789abcdef"})
V2 = phone_core.PROTO_V2


@asynccontextmanager
async def relay_for(tokens, legacy_pair=None):
    """그 토큰들로 시험용 중계를 띄운다 → (client, clock, app)."""
    clock = FakeClock()
    app = phone_relay.create_app(tokens, clock=clock, tick_interval=0.01, legacy_pair=legacy_pair)
    async with TestClient(TestServer(app)) as client:
        yield client, clock, app


@pytest_asyncio.fixture
async def relay3():
    """등록 기기 3대(옛 짝 = 앞 두 대 tab5-1·tab5-2)."""
    async with relay_for(TOK3) as r:
        yield r


def hdr(dev, proto=None, tokens=TOK4):
    h = {"X-Device-Id": dev, "X-Device-Token": tokens[dev]}
    if proto is not None:
        h["X-Phone-Proto"] = str(proto)
    return h


def peer(dev, on):
    return {"t": "peer", "id": dev, "online": on}


async def join_all(client, app, specs):
    """(기기, 규약 헤더 값 또는 None) 을 적힌 순서대로 붙인다 → {기기: 소켓}.
    붙일 때마다 순수 로직(같은 구성의 그림자 교환기)이 말한 상태 알림이 실제 소켓으로 글자까지 그대로 오는지 확인하며 읽어 낸다."""
    ex = app[phone_relay.EXCHANGE]
    shadow = phone_core.Exchange(ex.devices, ex.legacy_pair)
    wss = {}
    for dev, proto in specs:
        wss[dev] = await client.ws_connect("/ws", headers=hdr(dev, proto))
        acts = shadow.attach(dev, object(), 0, proto=phone_core.proto_from_header(None if proto is None else str(proto)))
        for act in acts:
            if act[0] == "send":
                assert await rx(wss[act[1]]) == act[2]
    return wss


# ── 접속 헤더 X-Phone-Proto (§6.1) ──────────────────────────────────────────
async def test_proto_header_two_gets_the_state_of_every_other_device(relay3):
    client, _, app = relay3
    b = await client.ws_connect("/ws", headers=hdr(T2, 2))
    assert await rx(b) == {"t": "peer", "id": "tab5-1", "online": False}
    assert await rx(b) == {"t": "peer", "id": "stackchan-2", "online": False}
    await assert_silent(b)
    assert app[phone_relay.EXCHANGE].proto[T2] == V2
    c = await client.ws_connect("/ws", headers=hdr(SC, 2))
    assert await rx(c) == {"t": "peer", "id": "tab5-1", "online": False}
    assert await rx(c) == {"t": "peer", "id": "tab5-2", "online": True}
    assert await rx(b) == {"t": "peer", "id": "stackchan-2", "online": True}


@pytest.mark.parametrize("value", [None, "1", "3", "2.0", "v2", "22", "02"])
async def test_missing_or_other_proto_header_is_a_v1_connection(relay3, value):
    """헤더가 없거나 정확히 "2" 가 아니면 v1: 옛 짝의 상태 하나만, 지금과 같은 모양(id 없음)으로 받는다."""
    client, _, app = relay3
    a = await client.ws_connect("/ws", headers=hdr(T1, value))
    assert await rx(a) == {"t": "peer", "online": False}
    await assert_silent(a)
    assert app[phone_relay.EXCHANGE].proto[T1] == phone_core.PROTO_V1


async def test_undecodable_proto_header_is_v1_not_a_server_error(relay3):
    """헤더 값이 깨진 바이트여도 서버 오류가 아니라 v1 접속이 된다(인증은 맞다)."""
    client, _, app = relay3
    reader, writer = await asyncio.open_connection(client.server.host, client.server.port)
    writer.write(b"GET /ws HTTP/1.1\r\nHost: x\r\nX-Device-Id: tab5-1\r\nX-Device-Token: " + TOK3[T1].encode() + b"\r\n"
                 b"X-Phone-Proto: \xff\xfe2\r\nConnection: Upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 13\r\n"
                 b"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n")
    await writer.drain()
    status = await asyncio.wait_for(reader.readline(), 2)
    assert status.startswith(b"HTTP/1.1 101"), status
    await asyncio.sleep(0.05)
    assert app[phone_relay.EXCHANGE].proto[T1] == phone_core.PROTO_V1
    writer.close()


async def test_proto_header_does_not_replace_authentication(relay3):
    """규약 헤더가 있어도 토큰이 틀리면 401 — 인증보다 먼저 읽히지 않는다."""
    client, _, app = relay3
    with pytest.raises(WSServerHandshakeError) as e:
        await client.ws_connect("/ws", headers={"X-Device-Id": SC, "X-Device-Token": "wrong-token-0123456789", "X-Phone-Proto": "2"})
    assert e.value.status == 401 and app[phone_relay.EXCHANGE].snapshot()["online"] == []


# ── 고른 상대에게만 벨 · from · 음성 (§6.3·§6.4) ────────────────────────────
async def test_call_rings_only_the_chosen_device_and_voice_stays_in_that_call(relay3):
    client, _, app = relay3
    ws = await join_all(client, app, [(T1, 2), (T2, 2), (SC, 2)])
    await ws[SC].send_json({"t": "call", "to": T2})
    assert await rx(ws[SC]) == {"t": "ringing"}
    assert await rx(ws[T2]) == {"t": "ring", "from": "stackchan-2"}
    await assert_silent(ws[T1])                                # 고르지 않은 기기에는 아무것도 가지 않는다
    await ws[T2].send_json({"t": "accept"})
    assert await rx(ws[SC]) == {"t": "connected"}
    assert await rx(ws[T2]) == {"t": "connected"}
    await ws[SC].send_bytes(VOICE)
    m = await ws[T2].receive(timeout=2)
    assert m.type == WSMsgType.BINARY and m.data == VOICE
    await ws[T2].send_bytes(VOICE[::-1])
    assert (await ws[SC].receive(timeout=2)).data == VOICE[::-1]
    await ws[T1].send_bytes(VOICE)                             # 통화에 없는 기기의 음성은 누구에게도 가지 않는다
    await ws[SC].send_json({"t": "hangup"})
    assert await rx(ws[SC]) == {"t": "ended", "why": "hangup"}
    assert await rx(ws[T2]) == {"t": "ended", "why": "hangup"}
    await assert_silent(ws[T1])
    assert app[phone_relay.EXCHANGE].stats["voice_fwd"] == 2 and app[phone_relay.EXCHANGE].stats["voice_drop_state"] == 1


async def test_busy_over_the_socket(relay3):
    """TW-D9: 벨이 울리는 상대·통화 중인 상대에게 걸면 거는 쪽만 ended:busy 를 받는다."""
    client, _, app = relay3
    ws = await join_all(client, app, [(T1, 2), (T2, 2), (SC, 2)])
    await ws[SC].send_json({"t": "call", "to": T2})
    assert await rx(ws[SC]) == {"t": "ringing"}
    assert await rx(ws[T2]) == {"t": "ring", "from": SC}
    await ws[T1].send_json({"t": "call", "to": T2})            # 벨 중인 상대
    assert await rx(ws[T1]) == {"t": "ended", "why": "busy"}
    await ws[T2].send_json({"t": "accept"})
    assert await rx(ws[SC]) == {"t": "connected"}
    assert await rx(ws[T2]) == {"t": "connected"}
    await ws[T1].send_json({"t": "call", "to": SC})            # 통화 중인 상대
    assert await rx(ws[T1]) == {"t": "ended", "why": "busy"}
    await assert_silent(ws[SC])                                # 통화 중인 쪽에는 알리지 않는다
    await assert_silent(ws[T2])
    assert app[phone_relay.EXCHANGE].state_of(SC) == phone_core.IN_CALL


async def test_glare_with_to_connects_over_the_socket(relay3):
    client, _, app = relay3
    ws = await join_all(client, app, [(T1, 2), (T2, 2), (SC, 2)])
    await ws[T1].send_json({"t": "call", "to": SC})
    assert await rx(ws[T1]) == {"t": "ringing"}
    assert await rx(ws[SC]) == {"t": "ring", "from": T1}
    await ws[SC].send_json({"t": "call", "to": T1})            # 벨 울리는 쪽도 그 상대에게 걸었다 → 바로 연결
    assert await rx(ws[T1]) == {"t": "connected"}
    assert await rx(ws[SC]) == {"t": "connected"}
    await assert_silent(ws[T2])


@pytest.mark.parametrize("frame", [{"t": "call", "to": "tab5-9"}, {"t": "call", "to": SC}, {"t": "call", "to": 3},
                                   {"t": "call", "to": ""}, {"t": "call", "to": None}, {"t": "call"}])
async def test_call_to_self_unregistered_broken_or_missing_target_rings_nobody(relay3, frame):
    """스택짱(옛 짝 없음)이 자기 자신·미등록·깨진 to·to 없이 건다 → 응답도 벨도 없다. 연결은 멀쩡하다."""
    client, _, app = relay3
    ws = await join_all(client, app, [(T1, 2), (T2, 2), (SC, 2)])
    await ws[SC].send_json(frame)
    await ws[SC].send_json({"t": "ping", "n": 5})
    assert await rx(ws[SC]) == {"t": "pong", "n": 5}
    await assert_silent(ws[T1], 0.1)
    await assert_silent(ws[T2], 0.1)
    assert app[phone_relay.EXCHANGE].state == phone_core.IDLE


# ── v1·v2 섞임 (TW-D7·D8) ───────────────────────────────────────────────────
async def test_stackchan_calling_an_old_firmware_tab5_gets_offline_and_the_tab5_does_not_ring(relay3):
    client, _, app = relay3
    ws = await join_all(client, app, [(T1, None), (T2, 2), (SC, 2)])       # 1번만 옛 펌웨어(헤더 없음)
    await ws[SC].send_json({"t": "call", "to": T1})
    assert await rx(ws[SC]) == {"t": "ended", "why": "offline"}
    await assert_silent(ws[T1])


async def test_new_firmware_tab5_calls_the_old_firmware_tab5_and_it_rings_exactly_like_before(relay3):
    client, _, app = relay3
    ws = await join_all(client, app, [(T1, None), (T2, 2), (SC, 2)])
    await ws[T2].send_json({"t": "call", "to": T1})
    assert await rx(ws[T2]) == {"t": "ringing"}
    assert await rx(ws[T1]) == {"t": "ring"}                   # v1 이 받는 벨에는 from 이 없다(글자까지 지금과 같다)
    await ws[T1].send_json({"t": "accept"})
    assert await rx(ws[T2]) == {"t": "connected"}
    assert await rx(ws[T1]) == {"t": "connected"}
    await ws[T1].send_bytes(VOICE)
    assert (await ws[T2].receive(timeout=2)).data == VOICE
    await ws[T1].send_json({"t": "hangup"})
    assert await rx(ws[T2]) == {"t": "ended", "why": "hangup"}
    assert await rx(ws[T1]) == {"t": "ended", "why": "hangup"}
    await assert_silent(ws[SC])


async def test_old_firmware_call_without_to_reaches_its_legacy_peer_and_never_the_stackchan(relay3):
    client, _, app = relay3
    ws = await join_all(client, app, [(T1, None), (T2, 2), (SC, 2)])
    await ws[T1].send_json({"t": "call"})
    assert await rx(ws[T1]) == {"t": "ringing"}
    assert await rx(ws[T2]) == {"t": "ring", "from": T1}       # 받는 쪽이 v2 면 건 기기가 실린다
    await assert_silent(ws[SC])
    await ws[T2].send_json({"t": "reject", "msg": 2})
    assert await rx(ws[T1]) == {"t": "ended", "why": "reject", "msg": 2}
    assert await rx(ws[T2]) == {"t": "ended", "why": "reject", "msg": 2}


async def test_two_old_firmware_tab5s_talk_as_before_while_three_devices_are_registered(relay3):
    """교환기만 v2 로 올리고 기기는 둘 다 옛 펌웨어인 날: 3대가 등록돼 있어도 Tab5 끼리는 글자 하나 다르지 않게 통화한다."""
    client, _, app = relay3
    a = await client.ws_connect("/ws", headers=hdr(T1))
    assert await rx(a) == {"t": "peer", "online": False}
    b = await client.ws_connect("/ws", headers=hdr(T2))
    assert await rx(b) == {"t": "peer", "online": True}
    assert await rx(a) == {"t": "peer", "online": True}
    await a.send_json({"t": "call"})
    assert await rx(a) == {"t": "ringing"}
    assert await rx(b) == {"t": "ring"}
    await b.send_json({"t": "call"})                           # 서로 동시에 걸기(옛 모양)
    assert await rx(a) == {"t": "connected"}
    assert await rx(b) == {"t": "connected"}
    await a.send_json({"t": "hangup"})
    assert await rx(a) == {"t": "ended", "why": "hangup"}
    assert await rx(b) == {"t": "ended", "why": "hangup"}


async def test_reconnecting_with_the_v2_header_makes_the_device_visible_to_the_stackchan(relay3):
    """1번이 새 펌웨어로 다시 붙는 순간: 스택짱에게 켜짐이 가고, 옛 짝(2번)은 꺼짐→켜짐, 옛 연결은 닫힌다."""
    client, _, app = relay3
    ws = await join_all(client, app, [(T1, None), (T2, 2), (SC, 2)])
    a2 = await client.ws_connect("/ws", headers=hdr(T1, 2))
    assert await rx(ws[T2]) == peer(T1, False)
    assert await rx(ws[T2]) == peer(T1, True)
    assert await rx(ws[SC]) == peer(T1, True)
    assert await rx(a2) == peer(T2, True)
    assert await rx(a2) == peer(SC, True)
    assert (await ws[T1].receive(timeout=2)).type in (WSMsgType.CLOSE, WSMsgType.CLOSING, WSMsgType.CLOSED)
    await ws[SC].send_json({"t": "call", "to": T1})            # 이제 걸린다
    assert await rx(ws[SC]) == {"t": "ringing"}
    assert await rx(a2) == {"t": "ring", "from": SC}


async def test_explicit_legacy_pair_decides_whom_a_call_without_to_reaches():
    async with relay_for(TOK3, legacy_pair=(T2, SC)) as (client, _, app):
        ws = await join_all(client, app, [(T1, 2), (T2, None), (SC, None)])
        await ws[SC].send_json({"t": "call"})
        assert await rx(ws[SC]) == {"t": "ringing"}
        assert await rx(ws[T2]) == {"t": "ring"}
        await assert_silent(ws[T1])


# ── 통화 두 건이 동시에 (TW-D10, 4대 등록) ──────────────────────────────────
async def two_calls(client, app):
    ws = await join_all(client, app, [(T1, 2), (T2, 2), (SC, 2), (D4, 2)])
    for caller, callee in ((T1, T2), (SC, D4)):
        await ws[caller].send_json({"t": "call", "to": callee})
        assert await rx(ws[caller]) == {"t": "ringing"}
        assert await rx(ws[callee]) == {"t": "ring", "from": caller}
        await ws[callee].send_json({"t": "accept"})
        assert await rx(ws[caller]) == {"t": "connected"}
        assert await rx(ws[callee]) == {"t": "connected"}
    return ws


async def test_two_calls_at_once_do_not_mix_voice():
    async with relay_for(TOK4) as (client, _, app):
        ws = await two_calls(client, app)
        frames = {d: bytes([0, i]) + d.encode() * 4 for i, d in enumerate((T1, T2, SC, D4))}     # 기기마다 다른 내용
        for d in (T1, T2, SC, D4):
            await ws[d].send_bytes(frames[d])
        for me, other in ((T1, T2), (T2, T1), (SC, D4), (D4, SC)):
            m = await ws[me].receive(timeout=2)
            assert m.type == WSMsgType.BINARY and m.data == frames[other]      # 내 통화 상대의 것만 온다
            await assert_silent(ws[me], 0.1)
        assert app[phone_relay.EXCHANGE].stats["voice_fwd"] == 4


async def test_one_side_leaving_ends_only_its_own_call():
    async with relay_for(TOK4) as (client, _, app):
        ws = await two_calls(client, app)
        await ws[T2].close()
        assert await rx(ws[T1]) == {"t": "ended", "why": "lost"}
        assert await rx(ws[T1]) == peer(T2, False)
        assert await rx(ws[SC]) == peer(T2, False)             # 다른 통화의 기기는 상태 알림만 받는다
        assert await rx(ws[D4]) == peer(T2, False)
        await ws[SC].send_bytes(VOICE)                         # 그 통화는 이어진다
        assert (await ws[D4].receive(timeout=2)).data == VOICE
        ex = app[phone_relay.EXCHANGE]
        assert ex.state_of(SC) == ex.state_of(D4) == phone_core.IN_CALL and ex.state_of(T1) == phone_core.IDLE


async def test_ring_timeout_ends_only_the_call_that_rang_for_30s():
    async with relay_for(TOK4) as (client, clock, app):
        ws = await join_all(client, app, [(T1, 2), (T2, 2), (SC, 2), (D4, 2)])
        await ws[T1].send_json({"t": "call", "to": T2})
        assert await rx(ws[T1]) == {"t": "ringing"}
        assert await rx(ws[T2]) == {"t": "ring", "from": T1}
        clock.t += 20.0
        await ws[SC].send_json({"t": "call", "to": D4})
        assert await rx(ws[SC]) == {"t": "ringing"}
        assert await rx(ws[D4]) == {"t": "ring", "from": SC}
        clock.t += 10.5                                        # 첫 벨만 30초를 넘겼다
        assert await rx(ws[T1]) == {"t": "ended", "why": "timeout"}
        assert await rx(ws[T2]) == {"t": "ended", "why": "timeout"}
        await assert_silent(ws[SC])
        assert app[phone_relay.EXCHANGE].state_of(D4) == phone_core.RINGING


# ── 그대로인 것: healthz·기록·종료 (§6.5·§7) ────────────────────────────────
async def test_healthz_reveals_nothing_but_ok_even_with_three_devices_in_use(relay3):
    client, _, app = relay3
    ws = await join_all(client, app, [(T1, 2), (T2, 2), (SC, 2)])
    await ws[SC].send_json({"t": "call", "to": T2})
    await rx(ws[SC]), await rx(ws[T2])
    r = await client.get("/healthz")
    assert r.status == 200 and await r.json() == {"ok": True}
    assert await r.text() == '{"ok": true}'


async def test_logs_contain_no_token_and_no_voice_with_three_devices(relay3, caplog):
    client, clock, app = relay3
    caplog.set_level(logging.DEBUG)
    ws = await join_all(client, app, [(T1, None), (T2, 2), (SC, 2)])
    await ws[SC].send_json({"t": "call", "to": T1})            # offline
    await rx(ws[SC])
    await ws[SC].send_json({"t": "call", "to": T2})
    await rx(ws[SC]), await rx(ws[T2])
    await ws[T1].send_json({"t": "call"})                      # busy (옛 짝이 벨 중)
    assert await rx(ws[T1]) == {"t": "ended", "why": "busy"}
    await ws[T2].send_json({"t": "accept"})
    await rx(ws[SC]), await rx(ws[T2])
    await ws[SC].send_bytes(VOICE)
    await ws[T2].receive(timeout=2)
    clock.t += 7.0
    await ws[T2].send_json({"t": "hangup"})
    await rx(ws[SC]), await rx(ws[T2])
    text = caplog.text
    for secret in (*TOK3.values(), "OPUSPAYLOAD"):
        assert secret not in text
    assert '"why":"offline"' in text and '"why":"busy"' in text and '"why":"hangup"' in text and '"talk_s":7.0' in text
    assert '"caller":"stackchan-2","callee":"tab5-2"' in text


async def test_shutdown_closes_all_three_devices_with_going_away(relay3):
    client, _, app = relay3
    ws = await join_all(client, app, [(T1, None), (T2, 2), (SC, 2)])
    await app.shutdown()
    for d in (T1, T2, SC):
        assert await closed_with(ws[d]) == 1001


# ── main(): PHONE_TOKENS 3대 · PHONE_LEGACY_PAIR (§6.1) ─────────────────────
def spec_of(tokens):
    return ",".join(f"{d}:{t}" for d, t in tokens.items())


def run_main(monkeypatch, **env):
    """main() 을 실제 서버 없이 돌려 web.run_app 에 넘긴 설정과 앱을 돌려준다. 환경변수는 넘긴 것만 있다."""
    seen = {}
    monkeypatch.setattr(phone_relay, "load_env_file", lambda path: None)
    clear_env(monkeypatch, "BIND", "PORT", "PHONE_TOKENS", "PHONE_LEGACY_PAIR")
    for k, v in env.items():
        monkeypatch.setenv(k, v)
    monkeypatch.setattr(phone_relay.web, "run_app", lambda app, **kw: seen.update(kw, app=app))
    phone_relay.main()
    return seen


def test_main_starts_with_three_devices_and_the_first_two_as_legacy_pair(monkeypatch):
    seen = run_main(monkeypatch, PHONE_TOKENS=spec_of(TOK3))
    ex = seen["app"][phone_relay.EXCHANGE]
    assert ex.devices == (T1, T2, SC) and ex.legacy_pair == (T1, T2)
    assert seen["host"] == "127.0.0.1" and seen["port"] == 5094 and seen["access_log"] is None      # 나머지 설정은 그대로


def test_main_reads_phone_legacy_pair(monkeypatch):
    seen = run_main(monkeypatch, PHONE_TOKENS=spec_of(TOK3), PHONE_LEGACY_PAIR="tab5-2, stackchan-2")
    assert seen["app"][phone_relay.EXCHANGE].legacy_pair == (T2, SC)


def test_main_treats_an_empty_phone_legacy_pair_as_unset(monkeypatch):
    seen = run_main(monkeypatch, PHONE_TOKENS=spec_of(TOK3), PHONE_LEGACY_PAIR="")
    assert seen["app"][phone_relay.EXCHANGE].legacy_pair == (T1, T2)


@pytest.mark.parametrize("pair", ["tab5-1,tab5-9", "tab5-1", "tab5-1,tab5-1", "tab5-1,tab5-2,stackchan-2"])
def test_main_rejects_a_wrong_legacy_pair_with_a_plain_message(monkeypatch, pair):
    with pytest.raises(SystemExit) as e:
        run_main(monkeypatch, PHONE_TOKENS=spec_of(TOK3), PHONE_LEGACY_PAIR=pair)
    assert "설정 오류" in str(e.value) and "PHONE_LEGACY_PAIR" in str(e.value)


def test_main_error_for_a_pasted_token_in_legacy_pair_does_not_show_it(monkeypatch):
    with pytest.raises(SystemExit) as e:
        run_main(monkeypatch, PHONE_TOKENS=spec_of(TOK3), PHONE_LEGACY_PAIR=spec_of(TOK3))
    assert not any(t in str(e.value) for t in TOK3.values())


def test_main_rejects_nine_devices(monkeypatch):
    nine = ",".join(f"dev-{i}:token-{i}-0123456789abcdef" for i in range(9))
    with pytest.raises(SystemExit) as e:
        run_main(monkeypatch, PHONE_TOKENS=nine)
    assert "PHONE_TOKENS" in str(e.value) and "token-3" not in str(e.value)


def test_main_startup_line_names_devices_and_legacy_pair_but_no_token(monkeypatch, caplog):
    caplog.set_level(logging.INFO)
    run_main(monkeypatch, PHONE_TOKENS=spec_of(TOK3))
    line = [r.getMessage() for r in caplog.records if "listening" in r.getMessage()][0]
    assert "devices=tab5-1,tab5-2,stackchan-2" in line and "legacy_pair=tab5-1,tab5-2" in line
    assert not any(t in caplog.text for t in TOK3.values())
