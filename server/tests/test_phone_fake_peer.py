"""phone_fake_peer(가짜 상대 도구) 시험 — 단계 문법, 받은 신호 판정, 그리고 시험용 중계(같은 프로세스)에 run() 을 붙인 실제 주고받기.
실행: cd server && python3 -m pytest tests/test_phone_fake_peer.py -q
수신거부 메시지(docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md §5.7, DM-D14): send:reject:<n>, expect:ended:reject:<n>(0 = msg 가 없어야 함).
"""
import asyncio
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
import phone_fake_peer as fp
from test_phone_relay import A, B, TOK, connect, relay, rx  # noqa: F401 — relay 는 fixture


# ── 단계 문법 ───────────────────────────────────────────────────────────────
def test_send_steps():
    assert fp.parse_step("send:call") == ("send", "call", None)
    assert fp.parse_step("send:reject") == ("send", "reject", None)
    assert fp.parse_step("send:reject:2") == ("send", "reject", 2)
    for n in (0, 100, -1):                                   # 범위 검사는 교환기 몫 — 틀린 번호도 보낼 수 있어야 교환기를 시험한다
        assert fp.parse_step(f"send:reject:{n}") == ("send", "reject", n)


@pytest.mark.parametrize("step", ["send:call:2", "send:accept:1", "send:hangup:1", "send:dance", "send:", "send:reject:", "send:reject:x",
                                  "send:reject:2.0", "send:reject:1:2", "send:reject:1_0", "send:reject: 2"])
def test_send_step_errors(step):
    """번호는 reject 에만 붙고, 정수가 아니면 단계 오류다."""
    with pytest.raises(ValueError):
        fp.parse_step(step)


def test_expect_steps():
    assert fp.parse_step("expect:ring") == ("expect", "ring", None, None, 10.0)
    assert fp.parse_step("expect:peer") == ("expect", "peer", None, None, 10.0)
    assert fp.parse_step("expect:ended:hangup") == ("expect", "ended", "hangup", None, 10.0)
    assert fp.parse_step("expect:ended:timeout@40") == ("expect", "ended", "timeout", None, 40.0)
    assert fp.parse_step("expect:ended:reject") == ("expect", "ended", "reject", None, 10.0)
    assert fp.parse_step("expect:ended:reject:2") == ("expect", "ended", "reject", 2, 10.0)
    assert fp.parse_step("expect:ended:reject:0@15") == ("expect", "ended", "reject", 0, 15.0)
    assert fp.parse_step("expect:ended:reject:2@15") == ("expect", "ended", "reject", 2, 15.0)


@pytest.mark.parametrize("step", ["expect:", "expect:ended:reject:two", "expect:ended:reject:", "expect:ended:reject:2.5", "expect:ended:hangup:2",
                                  "expect:ring:x:1", "expect:ended:reject:1:2"])
def test_expect_step_errors(step):
    with pytest.raises(ValueError):
        fp.parse_step(step)


def test_other_steps_unchanged():
    assert fp.parse_step("sleep:1.5") == ("sleep", 1.5)
    assert fp.parse_step("voice:3") == ("voice", 3)
    with pytest.raises(ValueError):
        fp.parse_step("dance:1")


# ── 받은 신호 판정 ──────────────────────────────────────────────────────────
def test_matches():
    with_msg, plain = {"t": "ended", "why": "reject", "msg": 2}, {"t": "ended", "why": "reject"}
    assert fp.matches(with_msg, "ended", "reject") and fp.matches(plain, "ended", "reject")       # 번호를 안 쓰면 보지 않는다(지금까지의 동작)
    assert fp.matches(with_msg, "ended", None) and fp.matches({"t": "ring"}, "ring", None)
    assert fp.matches(with_msg, "ended", "reject", 2) and not fp.matches(with_msg, "ended", "reject", 3)
    assert not fp.matches(plain, "ended", "reject", 2)                                             # 번호를 기다리는데 번호 없는 거절이 오면 맞지 않는다
    assert fp.matches(plain, "ended", "reject", 0) and not fp.matches(with_msg, "ended", "reject", 0)   # 0 = msg 가 없어야 한다
    assert not fp.matches({"t": "ended", "why": "reject", "msg": "2"}, "ended", "reject", 2)       # 글자 "2" 는 2 가 아니다
    assert not fp.matches({"t": "ended", "why": "reject", "msg": True}, "ended", "reject", 1)      # 참은 1 이 아니다
    assert not fp.matches(with_msg, "ended", "hangup", 2) and not fp.matches("x", "ended", None) and not fp.matches(None, "ended", None, 0)


# ── 시험용 중계에 붙여 실제로 주고받는다 ────────────────────────────────────
def start_fake(client, monkeypatch, dev, steps):
    """가짜 상대를 시험용 중계에 dev 로 붙인다. 토큰은 시험용 값만 쓴다(server/.env.phone 을 읽지 않게 막는다)."""
    monkeypatch.setattr(fp.phone_relay, "load_env_file", lambda path: None)
    monkeypatch.setenv("PHONE_TOKENS", f"{A}:{TOK[A]},{B}:{TOK[B]}")
    lines = []
    base = str(client.server.make_url("")).rstrip("/")
    return asyncio.ensure_future(fp.run(base, dev, steps, out=lines.append)), lines


@pytest.mark.parametrize("tail, want", [
    (["send:reject:2", "expect:ended:reject:2"], {"t": "ended", "why": "reject", "msg": 2}),     # 번호가 든 거절이 양쪽에 간다
    (["send:reject:100", "expect:ended:reject:0"], {"t": "ended", "why": "reject"}),             # 틀린 번호 → 교환기가 번호 없는 거절로 낮춘다
    (["send:reject", "expect:ended:reject:0"], {"t": "ended", "why": "reject"}),                 # 그냥 거절(지금까지의 동작)
])
async def test_fake_peer_rejects_through_the_relay(relay, monkeypatch, tail, want):
    client, _, _ = relay
    a = await connect(client, A)
    assert await rx(a) == {"t": "peer", "online": False}
    fake, lines = start_fake(client, monkeypatch, B, ["expect:ring"] + tail)
    assert await rx(a) == {"t": "peer", "online": True}
    await a.send_json({"t": "call"})
    assert await rx(a) == {"t": "ringing"}
    assert await rx(a) == want                                # 진짜 기기 자리(A)가 받는 종료 신호
    await asyncio.wait_for(fake, 5)
    assert lines[-1].endswith("FAKE OK")
    assert not any(TOK[A] in ln or TOK[B] in ln for ln in lines)          # 토큰은 출력하지 않는다


async def test_fake_peer_expect_with_a_number_fails_on_a_plain_reject(relay, monkeypatch):
    """기대 판정이 느슨하면 기기 시험이 거짓 통과한다 — 번호 2 를 기다리는데 번호 없는 거절이 오면 실패해야 한다."""
    client, _, _ = relay
    a = await connect(client, A)
    assert await rx(a) == {"t": "peer", "online": False}
    fake, _ = start_fake(client, monkeypatch, B, ["send:call", "expect:ringing", "expect:ended:reject:2@1"])
    assert await rx(a) == {"t": "peer", "online": True}
    assert await rx(a) == {"t": "ring"}
    await a.send_json({"t": "reject"})                        # 번호 없는 거절
    with pytest.raises(AssertionError, match="ended:reject:2"):
        await asyncio.wait_for(fake, 5)


async def test_fake_peer_expect_zero_fails_when_a_number_arrives(relay, monkeypatch):
    client, _, _ = relay
    a = await connect(client, A)
    assert await rx(a) == {"t": "peer", "online": False}
    fake, _ = start_fake(client, monkeypatch, B, ["send:call", "expect:ringing", "expect:ended:reject:0@1"])
    assert await rx(a) == {"t": "peer", "online": True}
    assert await rx(a) == {"t": "ring"}
    await a.send_json({"t": "reject", "msg": 3})
    with pytest.raises(AssertionError, match="ended:reject:0"):
        await asyncio.wait_for(fake, 5)


# ════════════════════════════════════════════════════════════════════════════
# 신호 규약 v2 (docs/exec-plans/2026-10-08-phone-3way-design.md §6·§7): --proto 2, send:call:<기기ID>,
# expect:ring:<기기ID>(건 기기 확인), expect:peer:<기기ID>:<on|off>, expect:ended:busy. 위의 옛 문법은 그대로 동작한다(v1 기본).
# ════════════════════════════════════════════════════════════════════════════
from test_phone_relay import assert_silent  # noqa: E402
from test_phone_relay_v2 import SC, TOK3, join_all, relay3  # noqa: E402,F401 — relay3 는 fixture
from test_phone_relay_v2 import T1, T2  # noqa: E402

import phone_relay  # noqa: E402


# ── 단계 문법 (v2) ──────────────────────────────────────────────────────────
def test_send_call_with_a_device_id():
    assert fp.parse_step("send:call:tab5-2") == ("send", "call", "tab5-2")
    assert fp.parse_step("send:call:stackchan-2") == ("send", "call", "stackchan-2")
    assert fp.parse_step("send:call:tab5-9") == ("send", "call", "tab5-9")       # 미등록 ID 도 보낼 수 있어야 교환기를 시험한다
    assert fp.frame_of("call", "tab5-2") == {"t": "call", "to": "tab5-2"}
    assert fp.frame_of("call", None) == {"t": "call"}                            # 옛 모양 그대로
    assert fp.frame_of("reject", 2) == {"t": "reject", "msg": 2} and fp.frame_of("accept", None) == {"t": "accept"}


@pytest.mark.parametrize("step", ["send:call:", "send:call:2", "send:call:-x", "send:call:tab5-2:x", "send:call:tab5 2", "send:call:탭",
                                  "send:accept:tab5-2", "send:hangup:tab5-2", "send:reject:tab5-2"])
def test_send_call_step_errors(step):
    """기기 ID 는 call 에만 붙고, 영문자로 시작하는 영문·숫자·-·_ 여야 한다(숫자만 쓰면 문구 번호를 잘못 붙인 것으로 본다)."""
    with pytest.raises(ValueError):
        fp.parse_step(step)


def test_expect_ring_and_peer_with_a_device_id():
    assert fp.parse_step("expect:ring:tab5-1") == ("expect", "ring", "tab5-1", None, 10.0)
    assert fp.parse_step("expect:ring:stackchan-2@3") == ("expect", "ring", "stackchan-2", None, 3.0)
    assert fp.parse_step("expect:peer:stackchan-2:on") == ("expect", "peer", "stackchan-2", True, 10.0)
    assert fp.parse_step("expect:peer:tab5-1:off@2.5") == ("expect", "peer", "tab5-1", False, 2.5)
    assert fp.parse_step("expect:ended:busy") == ("expect", "ended", "busy", None, 10.0)
    assert fp.parse_step("expect:ended:busy@4") == ("expect", "ended", "busy", None, 4.0)


@pytest.mark.parametrize("step", ["expect:ring:", "expect:ring:2", "expect:ring:tab5-1:on", "expect:peer:", "expect:peer:tab5-1",
                                  "expect:peer:tab5-1:", "expect:peer:tab5-1:maybe", "expect:peer:tab5-1:ON", "expect:peer::on",
                                  "expect:peer:tab5-1:on:x", "expect:peer:on", "expect:ended:busy:1"])
def test_expect_ring_and_peer_step_errors(step):
    """빈 기기 ID(셸 변수가 비어 `expect:ring:` 이 된 경우)를 "아무 벨이나"로 받으면 시험이 거짓 통과한다 — 단계 오류로 멈춘다."""
    with pytest.raises(ValueError):
        fp.parse_step(step)


def test_matches_ring_from_and_peer_id_online():
    r2, r1 = {"t": "ring", "from": "stackchan-2"}, {"t": "ring"}
    assert fp.matches(r2, "ring", None) and fp.matches(r1, "ring", None)                      # 기기 ID 를 안 쓰면 from 을 보지 않는다(지금까지의 동작)
    assert fp.matches(r2, "ring", "stackchan-2") and not fp.matches(r2, "ring", "tab5-1")
    assert not fp.matches(r1, "ring", "stackchan-2")                                          # from 이 없는(v1 모양) 벨은 "그 기기가 건 벨"이 아니다
    on, off, v1 = {"t": "peer", "id": "tab5-2", "online": True}, {"t": "peer", "id": "tab5-2", "online": False}, {"t": "peer", "online": True}
    assert fp.matches(on, "peer", None) and fp.matches(v1, "peer", None)
    assert fp.matches(on, "peer", "tab5-2", True) and not fp.matches(off, "peer", "tab5-2", True)
    assert fp.matches(off, "peer", "tab5-2", False) and not fp.matches(on, "peer", "tab5-1", True)
    assert not fp.matches(v1, "peer", "tab5-2", True)                                         # id 없는(v1 모양) 알림은 그 기기의 것으로 치지 않는다
    assert not fp.matches({"t": "peer", "id": "tab5-2", "online": 1}, "peer", "tab5-2", True)  # 1 은 참이 아니다
    assert fp.matches({"t": "ended", "why": "busy"}, "ended", "busy") and not fp.matches({"t": "ended", "why": "offline"}, "ended", "busy")


# ── 명령줄 ──────────────────────────────────────────────────────────────────
def test_cli_defaults_to_v1_and_local_relay():
    assert fp.parse_cli(["tab5-1", "send:call"]) == ("http://127.0.0.1:5094", 1, "tab5-1", ["send:call"])
    assert fp.parse_cli(["--base", "http://127.0.0.1:5095", "tab5-1"]) == ("http://127.0.0.1:5095", 1, "tab5-1", [])


def test_cli_proto_option_in_either_order():
    want = ("http://x:1", 2, "stackchan-2", ["send:call:tab5-2", "expect:ringing"])
    assert fp.parse_cli(["--proto", "2", "--base", "http://x:1", "stackchan-2", "send:call:tab5-2", "expect:ringing"]) == want
    assert fp.parse_cli(["--base", "http://x:1", "--proto", "2", "stackchan-2", "send:call:tab5-2", "expect:ringing"]) == want
    assert fp.parse_cli(["--proto", "1", "tab5-1"])[1] == 1


@pytest.mark.parametrize("args", [[], ["--base"], ["--proto"], ["--proto", "3", "tab5-1"], ["--proto", "two", "tab5-1"],
                                  ["--base", "http://x:1"], ["--speed", "9", "tab5-1"], ["--proto", "2"]])
def test_cli_errors(args):
    with pytest.raises(ValueError):
        fp.parse_cli(args)


def test_main_exits_with_usage_on_a_wrong_proto(capsys):
    with pytest.raises(SystemExit) as e:
        fp.main(["phone_fake_peer.py", "--proto", "3", "tab5-1", "send:call"])
    assert "--proto" in str(e.value)


# ── 3대 교환기에 붙여 실제로 주고받는다 ─────────────────────────────────────
def start_fake3(client, monkeypatch, dev, steps, proto):
    """가짜 기기를 3대 등록 시험 중계에 dev 로 붙인다. 토큰은 시험용 값만 쓴다(server/.env.phone 을 읽지 않게 막는다)."""
    monkeypatch.setattr(fp.phone_relay, "load_env_file", lambda path: None)
    monkeypatch.setenv("PHONE_TOKENS", ",".join(f"{d}:{t}" for d, t in TOK3.items()))
    lines = []
    base = str(client.server.make_url("")).rstrip("/")
    return asyncio.ensure_future(fp.run(base, dev, steps, out=lines.append, proto=proto)), lines


async def test_fake_stackchan_v2_calls_the_chosen_tab5_and_talks(relay3, monkeypatch):
    """--proto 2 + send:call:<기기ID>: 고른 2번에게만 벨이 가고(from = 스택짱), 통화·음성·끊기가 된다."""
    client, _, app = relay3
    ws = await join_all(client, app, [(T1, 2), (T2, 2)])
    fake, lines = start_fake3(client, monkeypatch, SC, ["expect:peer:tab5-1:on", "expect:peer:tab5-2:on", "send:call:tab5-2", "expect:ringing",
                                                         "expect:connected", "voice:3", "expect:ended:hangup"], proto=2)
    for d in (T1, T2):
        assert await rx(ws[d]) == {"t": "peer", "id": SC, "online": True}
    assert await rx(ws[T2]) == {"t": "ring", "from": SC}
    assert app[phone_relay.EXCHANGE].proto[SC] == 2                       # 헤더가 실제로 갔다
    await ws[T2].send_json({"t": "accept"})
    assert await rx(ws[T2]) == {"t": "connected"}
    for seq in range(3):
        assert (await ws[T2].receive(timeout=2)).data[:2] == seq.to_bytes(2, "big")
    await assert_silent(ws[T1])                                           # 고르지 않은 1번에는 벨도 음성도 없다
    await ws[T2].send_json({"t": "hangup"})
    await asyncio.wait_for(fake, 5)
    assert lines[-1].endswith("FAKE OK") and any("to=tab5-2" in ln for ln in lines)
    assert not any(t in ln for ln in lines for t in TOK3.values())        # 토큰은 출력하지 않는다


async def test_fake_stackchan_v2_checks_who_is_calling(relay3, monkeypatch):
    client, _, app = relay3
    ws = await join_all(client, app, [(T1, 2), (T2, 2)])
    fake, lines = start_fake3(client, monkeypatch, SC, ["expect:ring:tab5-2", "send:accept", "expect:connected", "expect:ended:hangup"], proto=2)
    for d in (T1, T2):
        assert await rx(ws[d]) == {"t": "peer", "id": SC, "online": True}
    await ws[T2].send_json({"t": "call", "to": SC})
    assert await rx(ws[T2]) == {"t": "ringing"}
    assert await rx(ws[T2]) == {"t": "connected"}
    await ws[T2].send_json({"t": "hangup"})
    await asyncio.wait_for(fake, 5)
    assert lines[-1].endswith("FAKE OK")


async def test_fake_expect_ring_from_fails_when_another_device_calls(relay3, monkeypatch):
    """1번이 걸기를 기다리는데 2번이 걸었다 — 느슨하게 통과하면 "누가 걸었는지" 시험이 거짓 통과한다."""
    client, _, app = relay3
    ws = await join_all(client, app, [(T1, 2), (T2, 2)])
    fake, _ = start_fake3(client, monkeypatch, SC, ["expect:ring:tab5-1@1"], proto=2)
    for d in (T1, T2):
        assert await rx(ws[d]) == {"t": "peer", "id": SC, "online": True}
    await ws[T2].send_json({"t": "call", "to": SC})
    with pytest.raises(AssertionError, match="ring:tab5-1"):
        await asyncio.wait_for(fake, 5)


async def test_fake_gets_busy_when_the_chosen_device_is_ringing(relay3, monkeypatch):
    client, _, app = relay3
    ws = await join_all(client, app, [(T1, 2), (T2, 2)])
    await ws[T1].send_json({"t": "call", "to": T2})
    assert await rx(ws[T1]) == {"t": "ringing"}
    assert await rx(ws[T2]) == {"t": "ring", "from": T1}
    fake, lines = start_fake3(client, monkeypatch, SC, ["send:call:tab5-2", "expect:ended:busy"], proto=2)
    await asyncio.wait_for(fake, 5)
    assert lines[-1].endswith("FAKE OK")


async def test_fake_stackchan_v2_sees_an_old_firmware_tab5_as_off_and_cannot_call_it(relay3, monkeypatch):
    """TW-D8 을 도구로 본다: 옛 펌웨어(v1)의 1번은 스택짱에게 꺼짐으로 보이고, 걸면 offline."""
    client, _, app = relay3
    ws = await join_all(client, app, [(T1, None), (T2, 2)])
    fake, lines = start_fake3(client, monkeypatch, SC, ["expect:peer:tab5-1:off", "expect:peer:tab5-2:on", "send:call:tab5-1",
                                                         "expect:ended:offline"], proto=2)
    await asyncio.wait_for(fake, 5)
    assert lines[-1].endswith("FAKE OK")
    await assert_silent(ws[T1])                                           # 1번에는 벨이 가지 않았다


async def test_fake_default_is_still_v1_on_a_three_device_relay(relay3, monkeypatch):
    """--proto 를 안 쓰면 헤더를 보내지 않는다(v1) — 옛 대본이 3대 등록 교환기에서도 그대로 돈다: to 없는 call 은 옛 짝에게."""
    client, _, app = relay3
    ws = await join_all(client, app, [(T2, 2)])
    fake, lines = start_fake3(client, monkeypatch, T1, ["expect:peer", "send:call", "expect:ringing", "expect:ended:reject:2"], proto=1)
    assert await rx(ws[T2]) == {"t": "peer", "id": T1, "online": True}
    assert await rx(ws[T2]) == {"t": "ring", "from": T1}
    assert app[phone_relay.EXCHANGE].proto[T1] == 1
    await ws[T2].send_json({"t": "reject", "msg": 2})
    await asyncio.wait_for(fake, 5)
    assert lines[-1].endswith("FAKE OK")


@pytest.mark.parametrize("step", ["expect:ring:tab5-2", "expect:peer:tab5-2:on"])
async def test_fake_v1_run_refuses_expectations_only_a_v2_connection_can_meet(relay3, monkeypatch, step):
    """from·id 는 v2 연결에만 실려 온다 — v1 로 붙어 기다리면 반드시 시간 초과다. 붙기 전에 "--proto 2 가 필요하다"로 멈춘다."""
    client, _, app = relay3
    fake, _ = start_fake3(client, monkeypatch, T1, [step], proto=1)
    with pytest.raises(ValueError, match="--proto 2"):
        await asyncio.wait_for(fake, 5)
    assert app[phone_relay.EXCHANGE].snapshot()["online"] == []           # 접속도 하지 않았다


async def test_fake_expect_peer_timeout_says_when_that_notice_already_went_by(relay3, monkeypatch):
    """기대는 온 순서대로 소비된다 — 접속 직후 상태 알림은 등록 순서(1번 → 2번)로 오는데 대본이 2번을 먼저 기다리면 1번의 알림은 그 사이에 지나간다.
    그냥 "오지 않음"이라고만 하면 교환기가 안 보낸 것으로 읽힌다 — 이미 지나갔다는 것과 순서를 바꾸라는 것을 말해 준다."""
    client, _, app = relay3
    ws = await join_all(client, app, [(T2, 2)])
    fake, lines = start_fake3(client, monkeypatch, SC, ["expect:peer:tab5-2:on", "expect:peer:tab5-1:off@0.5"], proto=2)
    with pytest.raises(AssertionError, match="이미 지나갔다") as e:
        await asyncio.wait_for(fake, 5)
    assert "peer:tab5-1:off" in str(e.value) and "순서" in str(e.value)


async def test_fake_expect_peer_plain_timeout_has_no_hint(relay3, monkeypatch):
    client, _, app = relay3
    ws = await join_all(client, app, [(T2, 2)])
    fake, _ = start_fake3(client, monkeypatch, SC, ["expect:peer:tab5-1:on@0.5"], proto=2)      # 1번은 꺼져 있다 — 정말로 오지 않는다
    with pytest.raises(AssertionError) as e:
        await asyncio.wait_for(fake, 5)
    assert "peer:tab5-1:on" in str(e.value) and "지나갔다" not in str(e.value)
