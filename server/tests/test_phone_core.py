"""phone_core 순수 로직 시험. 실행: cd server && python3 -m pytest tests/test_phone_core.py -q"""
import json
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import phone_core as pc

A, B = "tab5-1", "tab5-2"
TOK = {A: "a" * 32, B: "b" * 32}


def sends(acts, dev=None):
    """할 일 목록에서 send 만 (기기, 메시지) 로 뽑는다."""
    return [(a[1], a[2]) for a in acts if a[0] == "send" and (dev is None or a[1] == dev)]


def logs(acts):
    return [a[1] for a in acts if a[0] == "log"]


def both_online(now=0.0):
    ex = pc.Exchange([A, B])
    ex.attach(A, "connA", now)
    ex.attach(B, "connB", now)
    return ex


# ── 토큰 ────────────────────────────────────────────────────────────────────
def test_parse_tokens_ok():
    assert pc.parse_tokens(f"{A}:{'x' * 16}, {B}:{'y' * 16}") == {A: "x" * 16, B: "y" * 16}


@pytest.mark.parametrize("spec", ["", f"{A}:{'x' * 16}", f"{A}:short,{B}:{'y' * 16}", f"{A}{'x' * 16},{B}:{'y' * 16}",
                                  f"{A}:{'x' * 16},{A}:{'y' * 16}", ",".join(f"tab5-{i}:{'z' * 16}" for i in range(1, 10))])
def test_parse_tokens_rejects_bad_spec(spec):
    """마지막 값: 기기 9대(§6.1 의 2~8대를 넘음). 신호 규약 v2 전에는 "3대"가 이 자리의 틀린 값이었다 — 이제 3대는 맞는 설정이다."""
    with pytest.raises(ValueError):
        pc.parse_tokens(spec)


def test_parse_tokens_error_does_not_leak_token():
    secret = "SECRETSECRET"                          # 16자 미만 → 오류
    with pytest.raises(ValueError) as e:
        pc.parse_tokens(f"{A}:{secret},{B}:{'y' * 16}")
    assert secret not in str(e.value)


def test_check_token():
    assert pc.check_token(TOK, A, "a" * 32)
    assert not pc.check_token(TOK, A, "b" * 32)       # 남의 토큰
    assert not pc.check_token(TOK, "tab5-9", "a" * 32)  # 미등록 기기
    assert not pc.check_token(TOK, A, "")
    assert not pc.check_token(TOK, None, None)        # 헤더 없음


def test_check_token_survives_undecodable_header_value():
    """검수 #1: 헤더가 깨진 UTF-8 이면 aiohttp 가 짝 없는 대리 문자(\\udcff)로 넘긴다 — 예외 없이 거절해야 한다."""
    assert not pc.check_token(TOK, A, "\udcff")
    assert not pc.check_token(TOK, "tab5-9", "\udcff")
    assert not pc.check_token(TOK, "\udcff", "a" * 32)
    assert not pc.check_token(TOK, A, "\ud800")       # 2차 검수 4: 어떤 글자가 와도 예외 없이 거절


def test_exchange_needs_two_distinct_devices():
    with pytest.raises(ValueError):
        pc.Exchange([A])
    with pytest.raises(ValueError):
        pc.Exchange([A, A])


# ── 접속·이탈 ───────────────────────────────────────────────────────────────
def test_attach_reports_peer_state_both_ways():
    ex = pc.Exchange([A, B])
    assert sends(ex.attach(A, "connA", 0)) == [(A, {"t": "peer", "online": False})]
    assert sends(ex.attach(B, "connB", 1)) == [(B, {"t": "peer", "online": True}), (A, {"t": "peer", "online": True})]


def test_attach_unknown_device_raises():
    with pytest.raises(ValueError):
        pc.Exchange([A, B]).attach("tab5-9", "c", 0)


def test_detach_tells_peer_offline():
    ex = both_online()
    assert sends(ex.detach(A, "connA", 5)) == [(B, {"t": "peer", "online": False})]
    assert not ex.is_online(A)


def test_reattach_same_id_closes_old_connection():
    ex = both_online()
    acts = ex.attach(A, "connA2", 5)
    assert ("close", "connA") in acts
    assert ex.conn_of(A) == "connA2"


def test_late_detach_of_replaced_connection_is_ignored():
    """검토 초점 1: 재접속 뒤 옛 연결의 뒤늦은 이탈이 새 연결을 쫓아내면 안 된다."""
    ex = both_online()
    ex.attach(A, "connA2", 5)
    assert ex.detach(A, "connA", 6) == []
    assert ex.conn_of(A) == "connA2"


# ── 걸기·받기·거절·끊기 ─────────────────────────────────────────────────────
def test_call_rings_peer():
    ex = both_online()
    acts = ex.on_text(A, '{"t":"call"}', 1)
    assert sends(acts) == [(A, {"t": "ringing"}), (B, {"t": "ring"})]
    assert ex.state == pc.RINGING


def test_call_when_peer_offline_ends_offline():
    ex = pc.Exchange([A, B])
    ex.attach(A, "connA", 0)
    acts = ex.on_text(A, '{"t":"call"}', 1)
    assert sends(acts) == [(A, {"t": "ended", "why": "offline"})]
    assert ex.state == pc.IDLE


def test_accept_connects_both():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    acts = ex.on_text(B, '{"t":"accept"}', 4)
    assert sends(acts) == [(A, {"t": "connected"}), (B, {"t": "connected"})]
    assert ex.state == pc.IN_CALL


def test_reject_ends_both_with_reject():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    acts = ex.on_text(B, '{"t":"reject"}', 4)
    assert sends(acts) == [(A, {"t": "ended", "why": "reject"}), (B, {"t": "ended", "why": "reject"})]
    assert ex.state == pc.IDLE


def test_hangup_in_call_ends_both_and_logs_length():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    ex.on_text(B, '{"t":"accept"}', 4)
    acts = ex.on_text(B, '{"t":"hangup"}', 64.5)
    assert sends(acts) == [(A, {"t": "ended", "why": "hangup"}), (B, {"t": "ended", "why": "hangup"})]
    assert logs(acts) == [{"ev": "call_end", "caller": A, "callee": B, "why": "hangup", "ring_s": 3.0, "talk_s": 60.5}]


def test_caller_cancel_while_ringing():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    acts = ex.on_text(A, '{"t":"hangup"}', 3)
    assert sends(acts) == [(A, {"t": "ended", "why": "hangup"}), (B, {"t": "ended", "why": "hangup"})]


def test_ring_timeout_after_30s():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    ex.on_text(A, '{"t":"ping","n":1}', 20)          # 생존 확인은 벨 제한과 무관
    ex.on_text(B, '{"t":"ping","n":1}', 20)
    assert sends(ex.tick(30.9)) == []
    acts = ex.tick(31.0)
    assert sends(acts) == [(A, {"t": "ended", "why": "timeout"}), (B, {"t": "ended", "why": "timeout"})]
    assert ex.state == pc.IDLE


def test_glare_call_from_callee_is_accept():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    acts = ex.on_text(B, '{"t":"call"}', 1.2)
    assert sends(acts) == [(A, {"t": "connected"}), (B, {"t": "connected"})]


# ── 상태에 안 맞는 신호 ─────────────────────────────────────────────────────
@pytest.mark.parametrize("sig", ["accept", "reject", "hangup"])
def test_signals_ignored_when_idle(sig):
    ex = both_online()
    assert ex.on_text(A, '{"t":"%s"}' % sig, 1) == []
    assert ex.state == pc.IDLE


def test_caller_cannot_accept_or_reject_own_call():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    assert ex.on_text(A, '{"t":"accept"}', 2) == []
    assert ex.on_text(A, '{"t":"reject"}', 2) == []
    assert ex.on_text(A, '{"t":"call"}', 2) == []      # 거듭 걸기
    assert ex.state == pc.RINGING


def test_call_during_call_ignored():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    ex.on_text(B, '{"t":"accept"}', 2)
    assert ex.on_text(A, '{"t":"call"}', 3) == []
    assert ex.on_text(B, '{"t":"call"}', 3) == []
    assert ex.state == pc.IN_CALL


@pytest.mark.parametrize("raw", ["", "not json", "[]", "123", '{"x":1}', '{"t":5}', '{"t":"dance"}',
                                 '{"t":"ping"}', '{"t":"ping","n":"1"}', '{"t":"ping","n":true}',
                                 '{"t":"call","pad":"' + "x" * 300 + '"}'])
def test_garbage_text_is_ignored(raw):
    """검토 초점 2: 깨진·엉뚱한·너무 긴 신호에 죽지 않고, 상태도 안 바뀐다."""
    ex = both_online()
    assert ex.on_text(A, raw, 1) == []
    assert ex.state == pc.IDLE
    assert ex.stats["bad_signal"] == 1


def test_ping_pong_echoes_n():
    ex = both_online()
    assert sends(ex.on_text(A, '{"t":"ping","n":7}', 1)) == [(A, {"t": "pong", "n": 7})]


# ── 통화 도중 이탈 ──────────────────────────────────────────────────────────
def test_disconnect_in_call_gives_lost_then_peer_offline():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    ex.on_text(B, '{"t":"accept"}', 2)
    acts = ex.detach(B, "connB", 10)
    assert sends(acts) == [(A, {"t": "ended", "why": "lost"}), (A, {"t": "peer", "online": False})]
    assert ex.state == pc.IDLE


def test_disconnect_while_ringing_gives_offline():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    acts = ex.detach(B, "connB", 5)
    assert sends(acts) == [(A, {"t": "ended", "why": "offline"}), (A, {"t": "peer", "online": False})]


def test_reattach_during_call_ends_call_as_lost():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    ex.on_text(B, '{"t":"accept"}', 2)
    acts = ex.attach(B, "connB2", 10)                # B 가 재부팅해 다시 붙음
    assert (A, {"t": "ended", "why": "lost"}) in sends(acts)
    assert ("close", "connB") in acts
    assert sends(acts)[-2:] == [(B, {"t": "peer", "online": True}), (A, {"t": "peer", "online": True})]
    assert ex.state == pc.IDLE


# ── 무소식 45초 ─────────────────────────────────────────────────────────────
def test_silence_45s_closes_and_notifies_peer():
    ex = both_online(0)
    ex.on_text(B, '{"t":"ping","n":1}', 40)
    assert ex.tick(45.0) == []                        # 45초 "넘어야" 끊는다
    acts = ex.tick(45.1)
    assert ("close", "connA") in acts
    assert sends(acts) == [(B, {"t": "peer", "online": False})]
    assert not ex.is_online(A) and ex.is_online(B)


def test_silence_in_call_gives_lost():
    ex = both_online(0)
    ex.on_text(A, '{"t":"call"}', 1)
    ex.on_text(B, '{"t":"accept"}', 2)
    for t in range(5, 46, 5):                         # B 만 계속 소식을 보냄 (음성도 소식), 마지막 45초
        assert ex.route_voice(B, 60, t) == A
    acts = ex.tick(47.0)                              # A 의 마지막 소식 = 1초 → 46초째 무소식
    assert (B, {"t": "ended", "why": "lost"}) in sends(acts)
    assert ("close", "connA") in acts


# ── 음성 ────────────────────────────────────────────────────────────────────
def test_voice_forwarded_only_in_call():
    ex = both_online()
    assert ex.route_voice(A, 60, 1) is None           # 대기 중
    ex.on_text(A, '{"t":"call"}', 1)
    assert ex.route_voice(A, 60, 2) is None           # 벨 울리는 중
    ex.on_text(B, '{"t":"accept"}', 3)
    assert ex.route_voice(A, 60, 4) == B
    assert ex.route_voice(B, 60, 4) == A
    ex.on_text(A, '{"t":"hangup"}', 5)
    assert ex.route_voice(A, 60, 6) is None           # 끊은 뒤
    assert ex.stats["voice_fwd"] == 2 and ex.stats["voice_drop_state"] == 3


@pytest.mark.parametrize("size,ok", [(2, False), (3, True), (400, True), (401, False), (4000, False)])
def test_voice_size_limits(size, ok):
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    ex.on_text(B, '{"t":"accept"}', 2)
    assert (ex.route_voice(A, size, 3) == B) is ok


def test_should_drop_voice_when_peer_backlogged():
    """검토 초점 4: 상대 쪽이 밀려 있으면 쌓지 않고 버린다."""
    assert pc.MAX_PEER_BUFFER_BYTES == 4096           # PL-D3 의 값 자체를 고정
    assert not pc.should_drop_voice(0)
    assert not pc.should_drop_voice(pc.MAX_PEER_BUFFER_BYTES)
    assert pc.should_drop_voice(pc.MAX_PEER_BUFFER_BYTES + 1)


def test_frames_from_detached_device_are_ignored():
    ex = both_online()
    ex.detach(A, "connA", 1)
    assert ex.on_text(A, '{"t":"call"}', 2) == []
    assert ex.route_voice(A, 60, 2) is None


def test_log_records_never_contain_payload_fields():
    """기록에는 시각·기기·길이·사유만 — 허용된 키만 나온다."""
    ex = both_online()
    acts = ex.on_text(A, '{"t":"call"}', 1) + ex.on_text(B, '{"t":"accept"}', 2) + ex.on_text(A, '{"t":"hangup"}', 9)
    acts += ex.detach(A, "connA", 10)
    allowed = {"ev", "dev", "caller", "callee", "why", "ring_s", "talk_s"}
    for rec in logs(acts):
        assert set(rec) <= allowed


# ── 거절 문구 번호 (수신거부 메시지 — docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md §5.2, DM-D6) ──
def ringing():
    """A 가 걸어 B 에 벨이 울리는 중."""
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    return ex


@pytest.mark.parametrize("n", [1, 2, 3, 4, 99])
def test_reject_msg_reaches_both(n):
    """번호는 1~99 의 정수면 뜻을 묻지 않고 양쪽 ended 에 그대로 싣는다(4·99 는 기기가 모르는 번호 — 그래도 건넨다)."""
    ex = ringing()
    acts = ex.on_text(B, json.dumps({"t": "reject", "msg": n}), 4)
    want = {"t": "ended", "why": "reject", "msg": n}
    assert sends(acts) == [(A, want), (B, want)]
    assert ex.state == pc.IDLE


@pytest.mark.parametrize("raw", ["0", "-1", "100", "1000000000000000000000000000000", "2.0", "1e0", '"2"', "true", "false", "null",
                                 "[2]", '{"n":2}', '""'])
def test_reject_with_wrong_msg_is_a_plain_reject(raw):
    """검토 초점 1: 번호가 틀려도 거절은 맞는 신호다 — 통화는 끝나고, 번호만 빠진다. bad_signal 로 세지 않는다."""
    ex = ringing()
    acts = ex.on_text(B, '{"t":"reject","msg":%s}' % raw, 4)
    plain = {"t": "ended", "why": "reject"}
    assert sends(acts) == [(A, plain), (B, plain)]
    assert ex.state == pc.IDLE and ex.stats["bad_signal"] == 0


def test_reject_msg_is_not_logged():
    """기록은 번호가 없을 때와 글자까지 같다(§9.3 "기록: 시각·기기·통화 길이·종료 사유만")."""
    with_msg = logs(ringing().on_text(B, '{"t":"reject","msg":2}', 4))
    without = logs(ringing().on_text(B, '{"t":"reject"}', 4))
    assert with_msg == without == [{"ev": "call_end", "caller": A, "callee": B, "why": "reject", "ring_s": 3.0, "talk_s": 0}]


def test_reject_msg_from_the_wrong_side_or_state_is_ignored():
    """reject 가 유효한 조건(벨 중 + 받는 쪽)은 번호가 있어도 그대로다."""
    ex = ringing()
    assert ex.on_text(A, '{"t":"reject","msg":2}', 2) == [] and ex.state == pc.RINGING    # 거는 쪽은 거절할 수 없다
    ex.on_text(B, '{"t":"accept"}', 3)
    assert ex.on_text(B, '{"t":"reject","msg":2}', 4) == [] and ex.state == pc.IN_CALL    # 통화 중
    idle = both_online()
    assert idle.on_text(B, '{"t":"reject","msg":2}', 1) == [] and idle.state == pc.IDLE   # 대기 중(상대가 이미 취소한 뒤의 거절)
    assert ex.stats["bad_signal"] == 0 and idle.stats["bad_signal"] == 0


def test_msg_on_other_signals_is_not_forwarded():
    """msg 는 reject 에서만 읽는다 — hangup·call·accept 에 붙어 와도 건네지 않고, 다른 종료의 ended 는 글자 하나 달라지지 않는다."""
    ex = ringing()
    acts = ex.on_text(B, '{"t":"hangup","msg":2}', 4)
    assert sends(acts) == [(A, {"t": "ended", "why": "hangup"}), (B, {"t": "ended", "why": "hangup"})]
    ex = both_online()
    assert sends(ex.on_text(A, '{"t":"call","msg":2}', 1)) == [(A, {"t": "ringing"}), (B, {"t": "ring"})]
    assert sends(ex.on_text(B, '{"t":"accept","msg":2}', 2)) == [(A, {"t": "connected"}), (B, {"t": "connected"})]
    ex = ringing()
    ex.on_text(A, '{"t":"reject","msg":2}', 2)                                            # 무시된 거절의 번호가 남아 있다가
    assert sends(ex.tick(31.0)) == [(A, {"t": "ended", "why": "timeout"}), (B, {"t": "ended", "why": "timeout"})]   # 뒤의 종료에 묻어 나오지 않는다
