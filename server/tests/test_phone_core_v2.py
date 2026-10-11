"""phone_core 신호 규약 v2 시험 — 등록 기기 2~8대, 거는 상대 지정, v1·v2 섞임, busy, 통화 여러 건.
설계: docs/exec-plans/2026-10-08-phone-3way-design.md §6(규약)·§7(교환기)·§12(시험), 결정 TW-D7·D8·D9·D10.
옛 규약(v1, 2대)의 계약은 tests/test_phone_core.py 가 그대로 고정한다 — 이 파일은 v2 에서 새로 생긴 것만 본다.
실행: cd server && python3 -m pytest tests/test_phone_core_v2.py -q
"""
import json
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import phone_core as pc
from test_phone_core import logs, sends

T1, T2, SC, D4 = "tab5-1", "tab5-2", "stackchan-2", "desk-4"      # 등록 순서 = 옛 짝(T1·T2) + 스택짱 + 넷째
TRIO, QUAD = (T1, T2, SC), (T1, T2, SC, D4)
ACCEPT, REJECT, HANGUP = '{"t":"accept"}', '{"t":"reject"}', '{"t":"hangup"}'
RINGING_MSG, CONNECTED = {"t": "ringing"}, {"t": "connected"}


def peer(dev, on):
    """v2 연결이 받는 상태 알림."""
    return {"t": "peer", "id": dev, "online": on}


def peer_v1(on):
    """v1 연결이 받는 상태 알림(옛 짝 하나뿐이라 id 가 없다)."""
    return {"t": "peer", "online": on}


def call(to=None):
    return json.dumps({"t": "call"} if to is None else {"t": "call", "to": to})


def ring(frm=None):
    return {"t": "ring"} if frm is None else {"t": "ring", "from": frm}


def ended(why, **extra):
    return {"t": "ended", "why": why, **extra}


def online(*specs, ids=TRIO, legacy_pair=None):
    """교환기를 만들고 (기기, 규약) 을 적힌 순서대로 붙인다. 연결 객체는 "c-<기기>"."""
    ex = pc.Exchange(ids, legacy_pair)
    for dev, proto in specs:
        ex.attach(dev, "c-" + dev, 0.0, proto=proto)
    return ex


def all_v2(ids=TRIO):
    return online(*[(d, 2) for d in ids], ids=ids)


def leave(ex, dev, now):
    """그 기기의 지금 연결이 끊긴다(detach 는 연결을 "같은 객체인지"로 가린다 — 붙어 있는 객체를 그대로 넘긴다)."""
    return ex.detach(dev, ex.conn_of(dev), now)


def spec_of(n):
    return ",".join(f"dev-{i}:{chr(ord('a') + i) * 16}" for i in range(n))


# ── 등록: 2~8대, 옛 짝 해석 (§6.1) ──────────────────────────────────────────
@pytest.mark.parametrize("n", [2, 3, 8])
def test_parse_tokens_accepts_two_to_eight_devices(n):
    assert list(pc.parse_tokens(spec_of(n))) == [f"dev-{i}" for i in range(n)]


@pytest.mark.parametrize("n", [0, 1, 9, 12])
def test_parse_tokens_rejects_fewer_than_two_or_more_than_eight(n):
    with pytest.raises(ValueError):
        pc.parse_tokens(spec_of(n))


def test_parse_tokens_keeps_the_written_order():
    """옛 짝의 기본값이 "적힌 순서의 앞 두 대"라서 순서가 뜻을 가진다 — 정렬하지 않는다."""
    toks = pc.parse_tokens(f"{T2}:{'b' * 16},{SC}:{'c' * 16},{T1}:{'a' * 16}")
    assert list(toks) == [T2, SC, T1]


def test_parse_tokens_error_never_contains_a_token_even_with_three_devices():
    secrets = ["FIRST-TOKEN-0123456789", "SECOND-TOKEN-0123456789", "THIRD-SHORT"]      # 셋째가 16자 미만 → 오류
    with pytest.raises(ValueError) as e:
        pc.parse_tokens(f"{T1}:{secrets[0]},{T2}:{secrets[1]},{SC}:{secrets[2]}")
    assert not any(s in str(e.value) for s in secrets)
    with pytest.raises(ValueError) as e:                       # 9대(개수 오류)일 때도
        pc.parse_tokens(",".join(f"dev-{i}:SECRET-{i}-0123456789abcdef" for i in range(9)))
    assert "SECRET" not in str(e.value)


def test_parse_legacy_pair_reads_two_registered_ids():
    assert pc.parse_legacy_pair("tab5-1,tab5-2", TRIO) == (T1, T2)
    assert pc.parse_legacy_pair(" tab5-2 , stackchan-2 ", TRIO) == (T2, SC)       # 공백은 다듬고, 어느 두 대든 된다


@pytest.mark.parametrize("empty", ["", "   ", None])
def test_parse_legacy_pair_empty_means_the_first_two_registered(empty):
    assert pc.parse_legacy_pair(empty, TRIO) == (T1, T2)
    assert pc.parse_legacy_pair(empty, (SC, T2, T1)) == (SC, T2)                  # "앞 두 대" = 적힌 순서(이름순이 아니다)


@pytest.mark.parametrize("spec", [
    "tab5-1,tab5-9",              # 미등록 ID
    "tab5-1,tab5-1",              # 같은 ID 둘
    "tab5-1",                     # 개수 틀림(하나)
    "tab5-1,tab5-2,stackchan-2",  # 개수 틀림(셋)
    "tab5-1,",                    # 빈 칸
    ",",
    "TAB5-1,tab5-2",              # 대소문자가 다르면 다른 ID
])
def test_parse_legacy_pair_rejects_wrong_values(spec):
    with pytest.raises(ValueError):
        pc.parse_legacy_pair(spec, TRIO)


def test_parse_legacy_pair_error_does_not_echo_the_given_value():
    """PHONE_TOKENS 값을 잘못 붙여 넣었을 수 있다 — 받은 글자를 오류 문구(= 기록)에 되풀이하지 않는다."""
    with pytest.raises(ValueError) as e:
        pc.parse_legacy_pair("tab5-1:PASTED-TOKEN-0123456789,tab5-2:OTHER-TOKEN-0123456789", TRIO)
    assert "PASTED" not in str(e.value) and "OTHER" not in str(e.value)


def test_proto_header_only_exact_two_is_v2():
    """§6.1: 헤더 X-Phone-Proto 가 정확히 "2" 일 때만 v2. 없거나 그 밖은 v1."""
    assert pc.proto_from_header("2") == pc.PROTO_V2 == 2
    for other in (None, "", "1", "3", "02", "2.0", "v2", " 2", "22", 2):
        assert pc.proto_from_header(other) == pc.PROTO_V1 == 1, other


def test_exchange_takes_three_devices_and_the_first_two_are_the_legacy_pair():
    ex = pc.Exchange(TRIO)
    assert ex.devices == TRIO
    assert ex.legacy_peer_of(T1) == T2 and ex.legacy_peer_of(T2) == T1
    assert ex.legacy_peer_of(SC) is None                       # 옛 짝이 없는 기기


def test_exchange_takes_an_explicit_legacy_pair():
    ex = pc.Exchange(TRIO, (SC, T1))
    assert ex.legacy_peer_of(SC) == T1 and ex.legacy_peer_of(T1) == SC and ex.legacy_peer_of(T2) is None


@pytest.mark.parametrize("ids", [(T1,), (T1, T1, T2), tuple(f"dev-{i}" for i in range(9)), ()])
def test_exchange_rejects_one_nine_or_duplicate_devices(ids):
    with pytest.raises(ValueError):
        pc.Exchange(ids)


@pytest.mark.parametrize("pair", [(T1, "tab5-9"), (T1, T1), (T1,), (T1, T2, SC), "tab5-1,tab5-2"])
def test_exchange_rejects_a_wrong_legacy_pair(pair):
    with pytest.raises(ValueError):
        pc.Exchange(TRIO, pair)


def test_exchange_with_eight_devices_works():
    ids = tuple(f"dev-{i}" for i in range(8))
    ex = all_v2(ids)
    assert sends(ex.on_text(ids[7], call(ids[3]), 1)) == [(ids[7], RINGING_MSG), (ids[3], ring(ids[7]))]


def test_attach_rejects_an_unknown_proto_number():
    with pytest.raises(ValueError):
        pc.Exchange(TRIO).attach(T1, "c", 0, proto=3)


# ── 상태 알림 (§6.3) ────────────────────────────────────────────────────────
def test_v2_attach_receives_the_state_of_every_other_device():
    """v2 연결은 접속 직후 다른 기기 전부(꺼진 것 포함)의 상태를 등록 순서로 하나씩 받는다."""
    ex = pc.Exchange(TRIO)
    assert sends(ex.attach(T2, "c2", 0, proto=2)) == [(T2, peer(T1, False)), (T2, peer(SC, False))]
    assert sends(ex.attach(SC, "c3", 1, proto=2)) == [(SC, peer(T1, False)), (SC, peer(T2, True)), (T2, peer(SC, True))]
    assert sends(ex.attach(T1, "c1", 2, proto=2)) == [(T1, peer(T2, True)), (T1, peer(SC, True)),
                                                       (T2, peer(T1, True)), (SC, peer(T1, True))]


def test_v2_attach_with_four_devices_gets_three_states():
    ex = online((D4, 2), ids=QUAD)
    assert sends(ex.attach(T1, "c1", 1, proto=2)) == [(T1, peer(T2, False)), (T1, peer(SC, False)), (T1, peer(D4, True)),
                                                       (D4, peer(T1, True))]


def test_v1_attach_receives_only_its_legacy_peer_state_without_id():
    """v1 연결은 옛 짝의 상태 하나만, 지금과 같은 모양(id 없음)으로 받는다 — 스택짱이 켜져 있어도 모른다."""
    ex = online((SC, 2))
    assert sends(ex.attach(T1, "c1", 1, proto=1)) == [(T1, peer_v1(False))]
    assert sends(ex.attach(T2, "c2", 2, proto=1)) == [(T2, peer_v1(True)), (T1, peer_v1(True))]


def test_attach_defaults_to_v1():
    """proto 를 안 주면 v1 — 옛 호출(attach(dev, conn, now))이 그대로 옛 규약으로 동작한다."""
    ex = pc.Exchange(TRIO)
    assert sends(ex.attach(T1, "c1", 0)) == [(T1, peer_v1(False))]
    assert ex.proto[T1] == pc.PROTO_V1


def test_v1_device_is_never_told_about_devices_outside_its_legacy_pair():
    ex = online((T1, 1))
    assert sends(ex.attach(SC, "c3", 1, proto=2), T1) == []
    assert sends(leave(ex, SC, 2), T1) == []


def test_v1_device_looks_offline_to_a_v2_device_that_is_not_its_legacy_peer():
    """TW-D8: v1 상대는 옛 짝이 아닌 v2 에게 꺼짐으로 보인다 — 먼저 붙어 있든 나중에 붙든."""
    ex = online((SC, 2))
    assert sends(ex.attach(T1, "c1", 1, proto=1), SC) == []            # 스택짱에게는 아무 변화도 없다(계속 꺼짐)
    ex = online((T1, 1))
    assert sends(ex.attach(SC, "c3", 1, proto=2)) == [(SC, peer(T1, False)), (SC, peer(T2, False))]


def test_v1_device_looks_online_to_its_v2_legacy_peer_and_the_v1_sees_it_too():
    ex = online((T1, 1))
    assert sends(ex.attach(T2, "c2", 1, proto=2)) == [(T2, peer(T1, True)), (T2, peer(SC, False)), (T1, peer_v1(True))]


def test_detach_notifies_only_the_devices_that_saw_it_online():
    """꺼짐 알림은 "켜짐으로 알고 있던" 기기에게만 간다 — v1 을 원래 꺼짐으로 보던 스택짱에게는 다시 보내지 않는다."""
    ex = online((T1, 1), (T2, 2), (SC, 2))
    assert sends(leave(ex, T1, 5)) == [(T2, peer(T1, False))]
    assert sends(leave(ex, SC, 6)) == [(T2, peer(SC, False))]


def test_v1_device_without_a_legacy_peer_gets_no_peer_frames():
    """옛 짝이 없는 기기(스택짱)가 v1 로 붙으면 알릴 상대가 없다 — 상태 알림을 보내지 않고, 다른 v2 에게는 꺼짐으로 보인다."""
    ex = online((T2, 2))
    assert sends(ex.attach(SC, "c3", 1, proto=1)) == []
    assert sends(ex.attach(T1, "c1", 2, proto=2), T1) == [(T1, peer(T2, True)), (T1, peer(SC, False))]


# ── 같은 기기가 다른 규약으로 다시 붙음 (v1 펌웨어 → v2 펌웨어) ─────────────
def test_reattach_from_v1_to_v2_updates_every_viewer():
    """1번이 새 펌웨어로 다시 붙으면: 옛 짝(2번)은 꺼짐→켜짐, 스택짱은 (원래 꺼짐이었으니) 켜짐만 받는다. 새 연결은 전부의 상태를 받는다."""
    ex = online((T1, 1), (T2, 2), (SC, 2))
    acts = ex.attach(T1, "c1-new", 5, proto=2)
    assert ("close", "c-" + T1) in acts
    assert sends(acts, T2) == [(T2, peer(T1, False)), (T2, peer(T1, True))]
    assert sends(acts, SC) == [(SC, peer(T1, True))]
    assert sends(acts, T1) == [(T1, peer(T2, True)), (T1, peer(SC, True))]
    assert ex.proto[T1] == pc.PROTO_V2
    assert sends(ex.on_text(SC, call(T1), 6)) == [(SC, RINGING_MSG), (T1, ring(SC))]      # 이제 스택짱이 1번에게 걸 수 있다


def test_reattach_from_v2_to_v1_hides_it_from_devices_outside_its_legacy_pair():
    """거꾸로(옛 펌웨어로 되돌림): 스택짱에게는 꺼짐으로 바뀌고 다시 켜지지 않는다. 옛 짝은 꺼짐→켜짐. 새 연결(v1)은 옛 짝 상태만 받는다."""
    ex = all_v2()
    acts = ex.attach(T1, "c1-old-fw", 5, proto=1)
    assert sends(acts, SC) == [(SC, peer(T1, False))]
    assert sends(acts, T2) == [(T2, peer(T1, False)), (T2, peer(T1, True))]
    assert sends(acts, T1) == [(T1, peer_v1(True))]
    assert sends(ex.on_text(SC, call(T1), 6)) == [(SC, ended("offline"))]


def test_reattach_with_the_same_proto_flaps_offline_then_online():
    ex = all_v2()
    acts = ex.attach(SC, "c3-new", 5, proto=2)
    assert sends(acts, T1) == [(T1, peer(SC, False)), (T1, peer(SC, True))]
    assert sends(acts, T2) == [(T2, peer(SC, False)), (T2, peer(SC, True))]
    assert ex.conn_of(SC) == "c3-new"


def test_late_detach_of_a_replaced_connection_keeps_the_new_proto():
    ex = online((T1, 1), (T2, 2), (SC, 2))
    old_conn = ex.conn_of(T1)
    ex.attach(T1, "c1-new", 5, proto=2)
    assert ex.detach(T1, old_conn, 6) == []                    # 옛 연결의 뒤늦은 이탈
    assert ex.is_online(T1) and ex.proto[T1] == pc.PROTO_V2


# ── 걸기 판정 (§6.4) ────────────────────────────────────────────────────────
def test_call_rings_only_the_chosen_device():
    """6번 줄: 고른 상대에게만 벨이 가고, 나머지 기기에는 아무것도 가지 않는다."""
    ex = all_v2()
    acts = ex.on_text(SC, call(T2), 1)
    assert sends(acts) == [(SC, RINGING_MSG), (T2, ring(SC))]
    assert sends(acts, T1) == []
    assert ex.state_of(SC) == ex.state_of(T2) == pc.RINGING and ex.state_of(T1) == pc.IDLE
    assert ex.call_of[SC] is ex.call_of[T2] and T1 not in ex.call_of      # 두 기기가 같은 통화 객체를 가리킨다(§7)


def test_ring_names_the_caller_for_each_possible_pair():
    for caller, callee in ((T1, T2), (T2, T1), (T1, SC), (SC, T1), (T2, SC), (SC, T2)):
        ex = all_v2()
        assert sends(ex.on_text(caller, call(callee), 1)) == [(caller, RINGING_MSG), (callee, ring(caller))]


def test_call_without_to_goes_to_the_legacy_peer_for_v1_and_v2_callers():
    """§6.2: to 없는 call 은 옛 짝에게. v1 호환이고, v2 연결이 to 없이 보내도 같은 규칙이다."""
    ex = online((T1, 1), (T2, 1), (SC, 2))
    assert sends(ex.on_text(T1, call(), 1)) == [(T1, RINGING_MSG), (T2, ring())]               # v1 → v1: 지금과 글자까지 같다
    ex = online((T1, 1), (T2, 2), (SC, 2))
    assert sends(ex.on_text(T1, call(), 1)) == [(T1, RINGING_MSG), (T2, ring(T1))]             # v1 → v2 옛 짝: from 이 실린다
    ex = all_v2()
    acts = ex.on_text(T2, call(), 1)
    assert sends(acts) == [(T2, RINGING_MSG), (T1, ring(T2))] and sends(acts, SC) == []        # v2 가 to 없이 → 옛 짝


def test_call_without_to_from_a_device_outside_the_legacy_pair_is_ignored():
    """3번 줄 뒷부분: 옛 짝이 없는 기기(스택짱)의 to 없는 call 은 갈 곳이 없다 — 응답 없이 무시."""
    ex = all_v2()
    assert ex.on_text(SC, call(), 1) == []
    assert ex.state == pc.IDLE and ex.call_of == {}
    assert ex.stats["bad_signal"] == 0                         # 설계서 §6.4 3번: 미등록·자기 자신만 bad_signal, 이 경우는 "무시"만


@pytest.mark.parametrize("to", ["stackchan-2", "tab5-9", "nobody"])
def test_call_to_self_or_unregistered_is_ignored_as_bad_signal(to):
    """3번 줄 앞부분: 자기 자신·미등록에게 걸기 → 응답 없이 무시하고 bad_signal 로 센다."""
    ex = all_v2()
    assert ex.on_text(SC, call(to), 1) == []
    assert ex.state == pc.IDLE and ex.stats["bad_signal"] == 1


@pytest.mark.parametrize("raw_to", ["123", "0", "1.5", "true", "false", "null", "[]", '["tab5-1"]', "{}", '{"id":"tab5-1"}',
                                    '""', '" "', '" tab5-1"', '"TAB5-1"', '"tab5-1 "', '"' + "x" * 64 + '"', '"tab5-1\\u0000"'])
def test_call_with_a_broken_to_is_ignored_and_never_falls_back_to_the_legacy_peer(raw_to):
    """깨진 to(숫자·빈 글자·긴 글자·null·목록…)는 "to 가 없다"가 아니다 — 옛 짝에게 벨이 가면 엉뚱한 사람(집의 1번)을 울린다.
    to 칸이 있으면 등록된 기기 ID 글자와 글자까지 같을 때만 건다. 그 밖은 3번 줄(미등록) = 무시 + bad_signal."""
    ex = all_v2()
    acts = ex.on_text(T2, '{"t":"call","to":%s}' % raw_to, 1)      # T2 의 옛 짝은 T1
    assert acts == []
    assert ex.state == pc.IDLE and ex.call_of == {} and ex.stats["bad_signal"] == 1


def test_call_with_a_to_too_long_for_a_signal_frame_is_ignored():
    """신호 글자 한도(256바이트)를 넘기는 긴 to — 프레임째 버려진다(기존 규칙)."""
    ex = all_v2()
    assert ex.on_text(T2, call("x" * 300), 1) == []
    assert ex.state == pc.IDLE and ex.stats["bad_signal"] == 1


def test_call_to_an_offline_device_ends_offline():
    """4번 줄 앞부분: 대상이 꺼져 있으면 거는 쪽에만 ended:offline. 통화는 생기지 않는다."""
    ex = online((T1, 2), (T2, 2))
    acts = ex.on_text(T1, call(SC), 1)
    assert sends(acts) == [(T1, ended("offline"))]
    assert logs(acts) == [{"ev": "call_end", "caller": T1, "callee": SC, "why": "offline", "ring_s": 0, "talk_s": 0}]
    assert ex.state == pc.IDLE


def test_v2_device_calling_a_v1_device_that_is_not_its_legacy_peer_gets_offline():
    """4번 줄 뒷부분·TW-D8: 스택짱(v2)이 옛 펌웨어의 1번(v1)에게 걸면 offline — 1번에는 아무것도 가지 않는다."""
    ex = online((T1, 1), (T2, 2), (SC, 2))
    acts = ex.on_text(SC, call(T1), 1)
    assert sends(acts) == [(SC, ended("offline"))]
    assert sends(acts, T1) == [] and ex.state == pc.IDLE


def test_offline_wins_over_busy_for_a_v1_device_the_caller_cannot_see():
    """4번이 5번보다 먼저다: 보이지 않는 v1 기기가 통화 중이어도 busy 가 아니라 offline(통화 중이라는 사실을 흘리지 않는다)."""
    ex = online((T1, 1), (T2, 2), (SC, 2))
    ex.on_text(T2, call(T1), 1)
    ex.on_text(T1, ACCEPT, 2)
    assert sends(ex.on_text(SC, call(T1), 3)) == [(SC, ended("offline"))]
    assert ex.state_of(T1) == pc.IN_CALL


def test_v2_legacy_peer_calling_a_v1_device_rings_it_without_from():
    """4·6번 줄: 옛 짝인 v2(새 펌웨어의 2번)가 v1(옛 펌웨어의 1번)에게 걸면 벨이 가고, v1 이 받는 ring 은 지금 모양(from 없음)이다."""
    ex = online((T1, 1), (T2, 2), (SC, 2))
    acts = ex.on_text(T2, call(T1), 1)
    assert sends(acts) == [(T2, RINGING_MSG), (T1, ring())]
    assert sends(ex.on_text(T1, ACCEPT, 2)) == [(T2, CONNECTED), (T1, CONNECTED)]


def test_v1_connection_naming_a_device_outside_its_legacy_pair_gets_offline():
    """TW-D8("v1 기기는 옛 짝하고만 걸고 받는다")의 건 쪽 절반 — 설계서 §6.4 에는 글로 없는 경우(v1 은 to 를 보내지 않는다).
    v1 연결이 to 로 옛 짝 아닌 기기를 부르면 offline. 벨이 간다면 받는 쪽 화면에는 "잠든" 캐릭터가 전화를 건 것으로 뜬다."""
    ex = online((T1, 1), (T2, 2), (SC, 2))
    acts = ex.on_text(T1, call(SC), 1)
    assert sends(acts) == [(T1, ended("offline"))] and ex.state == pc.IDLE
    assert sends(ex.on_text(T1, call(T2), 2)) == [(T1, RINGING_MSG), (T2, ring(T1))]          # 옛 짝은 to 로 불러도 된다


def test_call_to_a_device_that_is_being_rung_or_is_ringing_someone_ends_busy():
    """5번 줄·TW-D9: 벨에 묶인 상대(벨이 울리는 쪽, 벨을 걸어 놓고 기다리는 쪽 모두) → 거는 쪽에만 ended:busy."""
    ex = all_v2()
    ex.on_text(SC, call(T2), 1)
    acts = ex.on_text(T1, call(T2), 2)                         # 벨이 울리고 있는 2번에게
    assert sends(acts) == [(T1, ended("busy"))]
    assert logs(acts) == [{"ev": "call_end", "caller": T1, "callee": T2, "why": "busy", "ring_s": 0, "talk_s": 0}]
    assert sends(ex.on_text(T1, call(SC), 3)) == [(T1, ended("busy"))]                         # 걸어 놓고 기다리는 스택짱에게
    assert ex.state_of(T1) == pc.IDLE and ex.state_of(SC) == ex.state_of(T2) == pc.RINGING    # 울리던 벨은 그대로


def test_call_to_a_device_in_a_call_ends_busy_and_does_not_disturb_the_call():
    ex = all_v2()
    ex.on_text(SC, call(T2), 1)
    ex.on_text(T2, ACCEPT, 2)
    for target in (T2, SC):
        acts = ex.on_text(T1, call(target), 3)
        assert sends(acts) == [(T1, ended("busy"))]            # 통화 중인 두 기기에는 아무것도 가지 않는다
    assert ex.state_of(SC) == ex.state_of(T2) == pc.IN_CALL
    assert ex.route_voice(SC, 60, 4) == T2


def test_v1_caller_gets_the_same_busy_frame_when_its_legacy_peer_is_busy():
    """§6.3: ended 의 busy 는 "모두"에게 같은 모양으로 간다 — 옛 펌웨어(v1)가 걸었어도. (v1 화면은 모르는 사유를 "받지 않고 끊음"으로 보여 준다 — 기기 쪽 동작)"""
    ex = online((T1, 1), (T2, 2), (SC, 2))
    ex.on_text(SC, call(T2), 1)                                # 옛 짝(2번)이 스택짱의 벨을 받는 중
    assert sends(ex.on_text(T1, call(), 2)) == [(T1, ended("busy"))]
    ex.on_text(T2, ACCEPT, 3)                                  # 통화 중이 된 뒤에도
    assert sends(ex.on_text(T1, call(), 4)) == [(T1, ended("busy"))]
    assert ex.state_of(T1) == pc.IDLE and ex.state_of(T2) == pc.IN_CALL


def test_caller_that_got_busy_is_free_and_can_call_again_once_the_line_clears():
    ex = all_v2()
    ex.on_text(SC, call(T2), 1)
    ex.on_text(T1, call(T2), 2)                                # busy
    assert T1 not in ex.call_of
    ex.on_text(SC, HANGUP, 3)
    assert sends(ex.on_text(T1, call(T2), 4)) == [(T1, RINGING_MSG), (T2, ring(T1))]


def test_glare_with_to_connects_both():
    """1번 줄: 서로 동시에 걸기 — 벨을 받는 중인 기기가 "나에게 건 기기"를 to 로 부르면 받기로 친다."""
    ex = all_v2()
    ex.on_text(SC, call(T2), 1)
    acts = ex.on_text(T2, call(SC), 1.2)
    assert sends(acts) == [(SC, CONNECTED), (T2, CONNECTED)]
    assert ex.state_of(SC) == ex.state_of(T2) == pc.IN_CALL and ex.state_of(T1) == pc.IDLE


def test_call_without_to_while_being_rung_is_an_accept_even_if_the_caller_is_not_the_legacy_peer():
    """1번 줄: 벨을 받는 중의 to 없는 call 은 "지금 울리는 전화 받기" — 옛 짝(1번)에게 새로 걸지 않는다."""
    ex = all_v2()
    ex.on_text(SC, call(T2), 1)
    acts = ex.on_text(T2, call(), 2)
    assert sends(acts) == [(SC, CONNECTED), (T2, CONNECTED)] and sends(acts, T1) == []


def test_v1_device_being_rung_by_its_v2_legacy_peer_can_answer_with_a_plain_call():
    """1번 줄, 섞인 경우: 옛 펌웨어(v1)의 1번이 벨을 받는 중에 통화 버튼을 누르면({"t":"call"}) 받기가 된다 — 옛 규칙 그대로."""
    ex = online((T1, 1), (T2, 2), (SC, 2))
    ex.on_text(T2, call(T1), 1)
    acts = ex.on_text(T1, call(), 1.5)
    assert sends(acts) == [(T2, CONNECTED), (T1, CONNECTED)] and sends(acts, SC) == []


def test_call_to_a_third_device_while_being_rung_is_ignored():
    """2번 줄: 벨 받는 중에 제3자에게 call → 응답 없이 무시. 제3자에는 벨이 가지 않고, 울리던 벨은 그대로다."""
    ex = all_v2()
    ex.on_text(SC, call(T2), 1)
    assert ex.on_text(T2, call(T1), 2) == []
    assert ex.state_of(T2) == pc.RINGING and ex.state_of(T1) == pc.IDLE and ex.stats["bad_signal"] == 0
    assert sends(ex.on_text(T2, ACCEPT, 3)) == [(SC, CONNECTED), (T2, CONNECTED)]              # 원래 전화는 여전히 받을 수 있다


def test_call_from_a_device_already_ringing_out_or_in_a_call_is_ignored():
    """2번 줄: 거는 쪽이 이미 벨·통화에 묶여 있으면 누구에게 걸든 무시."""
    ex = all_v2()
    ex.on_text(SC, call(T2), 1)
    assert ex.on_text(SC, call(T1), 2) == [] and ex.on_text(SC, call(T2), 2) == [] and ex.on_text(SC, call(), 2) == []
    ex.on_text(T2, ACCEPT, 3)
    for dev in (SC, T2):
        assert ex.on_text(dev, call(T1), 4) == [] and ex.on_text(dev, call(), 4) == []
    assert ex.state_of(T1) == pc.IDLE and ex.state_of(SC) == pc.IN_CALL


def test_rules_are_applied_top_down_a_bound_caller_is_ignored_before_to_is_examined():
    """판정은 위에서부터: 묶여 있는 기기의 call 은 2번 줄에서 끝난다 — to 가 깨졌어도 3번 줄(bad_signal)까지 가지 않는다."""
    ex = all_v2()
    ex.on_text(SC, call(T2), 1)
    assert ex.on_text(T2, '{"t":"call","to":123}', 2) == [] and ex.on_text(SC, call("tab5-9"), 2) == []
    assert ex.stats["bad_signal"] == 0 and ex.state_of(T2) == pc.RINGING


# ── 그대로인 신호 (§6.2·§6.3 "그대로") ──────────────────────────────────────
def test_ping_pong_ringing_connected_are_the_same_on_v1_and_v2_connections():
    ex = online((T1, 1), (T2, 2), (SC, 2))
    for dev in (T1, T2, SC):
        assert sends(ex.on_text(dev, '{"t":"ping","n":9}', 1)) == [(dev, {"t": "pong", "n": 9})]
    assert sends(ex.on_text(T1, call(), 2), T1) == [(T1, {"t": "ringing"})]                  # v1 이 걸었을 때
    assert sends(ex.on_text(T2, ACCEPT, 3)) == [(T1, {"t": "connected"}), (T2, {"t": "connected"})]
    ex.on_text(T1, HANGUP, 4)
    assert sends(ex.on_text(T2, call(SC), 5), T2) == [(T2, {"t": "ringing"})]                # v2 가 걸었을 때
    assert sends(ex.on_text(SC, ACCEPT, 6)) == [(T2, {"t": "connected"}), (SC, {"t": "connected"})]


# ── 받기·거절·끊기는 그 통화의 당사자만 ─────────────────────────────────────
@pytest.mark.parametrize("sig", [ACCEPT, REJECT, HANGUP, '{"t":"reject","msg":2}'])
def test_a_third_device_cannot_touch_someone_elses_call(sig):
    ex = all_v2()
    ex.on_text(SC, call(T2), 1)
    assert ex.on_text(T1, sig, 2) == []
    assert ex.state_of(SC) == ex.state_of(T2) == pc.RINGING


def test_ended_goes_to_both_parties_of_that_call_and_nobody_else():
    """§6.5: ended 는 그 통화의 양쪽 모두에게 — 그리고 그 둘에게만."""
    for end_by, sig, why in ((T2, REJECT, "reject"), (SC, HANGUP, "hangup"), (T2, HANGUP, "hangup")):
        ex = all_v2()
        ex.on_text(SC, call(T2), 1)
        assert sends(ex.on_text(end_by, sig, 2)) == [(SC, ended(why)), (T2, ended(why))]
    ex = all_v2()
    ex.on_text(SC, call(T2), 1)
    assert sends(ex.on_text(T2, '{"t":"reject","msg":3}', 2)) == [(SC, ended("reject", msg=3)), (T2, ended("reject", msg=3))]


# ── 벨 제한은 통화마다 (§6.4 6번 줄) ────────────────────────────────────────
@pytest.mark.parametrize("first,second", [((SC, T2), (D4, T1)), ((D4, T1), (SC, T2))])
def test_ring_timeout_is_counted_per_call(first, second):
    """벨 제한은 통화마다 따로 센다 — 먼저 건 통화만 30초에 끝나고, 10초 늦게 건 통화는 10초 더 울린다.
    두 순서를 다 본다(먼저 건 쪽이 등록 순서로 앞일 때와 뒤일 때): 한 통화의 시간 초과가 다른 통화를 끌고 가면 어느 한쪽에서 드러난다."""
    ex = all_v2(QUAD)
    ex.on_text(first[0], call(first[1]), 1)
    ex.on_text(second[0], call(second[1]), 11)

    def everyone_pings(t):                                     # 무소식 끊기에 걸리지 않게 모두 소식을 보낸다(벨 제한과는 무관)
        for d in QUAD:
            ex.on_text(d, '{"t":"ping","n":1}', t)

    everyone_pings(20)
    assert sends(ex.tick(30.9)) == []
    acts = ex.tick(31.0)                                       # 첫 통화만 30초가 찼다
    assert sends(acts) == [(first[0], ended("timeout")), (first[1], ended("timeout"))]
    assert logs(acts) == [{"ev": "call_end", "caller": first[0], "callee": first[1], "why": "timeout", "ring_s": 30.0, "talk_s": 0}]
    assert ex.state_of(second[0]) == ex.state_of(second[1]) == pc.RINGING
    everyone_pings(40)
    assert sends(ex.tick(40.9)) == []
    assert sends(ex.tick(41.0)) == [(second[0], ended("timeout")), (second[1], ended("timeout"))]
    assert ex.state == pc.IDLE and ex.call_of == {}


def test_an_answered_call_is_not_timed_out_while_another_still_rings():
    ex = all_v2(QUAD)
    ex.on_text(SC, call(T2), 1)
    ex.on_text(T2, ACCEPT, 2)
    ex.on_text(D4, call(T1), 3)
    for d in QUAD:
        ex.on_text(d, '{"t":"ping","n":1}', 30)
    assert sends(ex.tick(33.0)) == [(D4, ended("timeout")), (T1, ended("timeout"))]
    assert ex.state_of(SC) == pc.IN_CALL


# ── 통화 두 건이 동시에 (TW-D10) ────────────────────────────────────────────
def two_calls():
    """4대: T1↔T2 통화 중, SC↔D4 통화 중."""
    ex = all_v2(QUAD)
    ex.on_text(T1, call(T2), 1)
    ex.on_text(T2, ACCEPT, 2)
    ex.on_text(SC, call(D4), 3)
    ex.on_text(D4, ACCEPT, 4)
    return ex


def test_two_calls_progress_independently():
    ex = all_v2(QUAD)
    assert sends(ex.on_text(T1, call(T2), 1)) == [(T1, RINGING_MSG), (T2, ring(T1))]
    assert sends(ex.on_text(SC, call(D4), 2)) == [(SC, RINGING_MSG), (D4, ring(SC))]           # 첫 통화가 벨 중이어도 다른 쌍은 건다
    assert sends(ex.on_text(D4, ACCEPT, 3)) == [(SC, CONNECTED), (D4, CONNECTED)]
    assert ex.state_of(T1) == ex.state_of(T2) == pc.RINGING and ex.state_of(SC) == ex.state_of(D4) == pc.IN_CALL
    assert sends(ex.on_text(T2, REJECT, 4)) == [(T1, ended("reject")), (T2, ended("reject"))]
    assert ex.state_of(SC) == ex.state_of(D4) == pc.IN_CALL
    assert ex.call_of[SC] is ex.call_of[D4] and ex.call_of[SC] is not ex.call_of.get(T1)


def test_voice_of_two_calls_never_mixes():
    """§6.3: 음성은 같은 통화의 상대에게만 건넨다."""
    ex = two_calls()
    assert ex.route_voice(T1, 60, 5) == T2 and ex.route_voice(T2, 60, 5) == T1
    assert ex.route_voice(SC, 60, 5) == D4 and ex.route_voice(D4, 60, 5) == SC
    assert ex.stats["voice_fwd"] == 4 and ex.stats["voice_drop_state"] == 0


def test_hangup_ends_only_its_own_call():
    ex = two_calls()
    acts = ex.on_text(T1, HANGUP, 10)
    assert sends(acts) == [(T1, ended("hangup")), (T2, ended("hangup"))]
    assert logs(acts) == [{"ev": "call_end", "caller": T1, "callee": T2, "why": "hangup", "ring_s": 1.0, "talk_s": 8.0}]
    assert ex.state_of(SC) == ex.state_of(D4) == pc.IN_CALL
    assert ex.route_voice(SC, 60, 11) == D4 and ex.route_voice(T1, 60, 11) is None


def test_device_leaving_one_call_does_not_touch_the_other_call():
    """통화 중 한쪽 이탈: 그 통화의 상대만 lost 를 받는다. 다른 통화의 두 기기는 상태 알림(꺼짐)만 받고 통화는 이어진다."""
    ex = two_calls()
    acts = leave(ex, T2, 10)
    assert sends(acts) == [(T1, ended("lost")), (T1, peer(T2, False)), (SC, peer(T2, False)), (D4, peer(T2, False))]
    assert ex.state_of(T1) == pc.IDLE and ex.state_of(SC) == ex.state_of(D4) == pc.IN_CALL
    assert ex.route_voice(SC, 60, 11) == D4 and ex.route_voice(D4, 60, 11) == SC


def test_summary_state_is_the_most_advanced_call():
    """Exchange.state 는 요약값(통화 하나라도 있으면 IN_CALL, 아니고 벨이 있으면 RINGING) — 2대일 때는 옛 뜻과 같다."""
    ex = all_v2(QUAD)
    assert ex.state == pc.IDLE
    ex.on_text(T1, call(T2), 1)
    assert ex.state == pc.RINGING
    ex.on_text(SC, call(D4), 2)
    ex.on_text(D4, ACCEPT, 3)
    assert ex.state == pc.IN_CALL
    ex.on_text(SC, HANGUP, 4)
    assert ex.state == pc.RINGING
    ex.on_text(T1, HANGUP, 5)
    assert ex.state == pc.IDLE


# ── 이탈·재접속 교체 (§6.5) ─────────────────────────────────────────────────
def test_leaving_while_ringing_gives_offline_to_the_other_party_only():
    ex = all_v2()
    ex.on_text(SC, call(T2), 1)
    acts = leave(ex, SC, 5)                         # 건 쪽이 사라짐
    assert sends(acts) == [(T2, ended("offline")), (T1, peer(SC, False)), (T2, peer(SC, False))]
    assert logs(acts)[-1] == {"ev": "call_end", "caller": SC, "callee": T2, "why": "offline", "ring_s": 4.0, "talk_s": 0}
    assert ex.state == pc.IDLE


def test_leaving_when_not_in_any_call_does_not_end_other_calls():
    ex = all_v2()
    ex.on_text(SC, call(T2), 1)
    ex.on_text(T2, ACCEPT, 2)
    assert sends(leave(ex, T1, 3)) == [(T2, peer(T1, False)), (SC, peer(T1, False))]
    assert ex.state_of(SC) == ex.state_of(T2) == pc.IN_CALL


def test_reattach_closes_the_old_connection_and_ends_only_the_call_it_was_in():
    """같은 ID 가 다시 붙으면 옛 연결을 닫고, 그 기기가 묶여 있던 통화만 끝난다(다른 통화는 그대로)."""
    ex = two_calls()
    acts = ex.attach(SC, "c3-new", 10, proto=2)
    assert ("close", "c-" + SC) in acts and ex.conn_of(SC) == "c3-new"
    assert sends(acts, D4) == [(D4, ended("lost")), (D4, peer(SC, False)), (D4, peer(SC, True))]
    assert sends(acts, SC) == [(SC, peer(T1, True)), (SC, peer(T2, True)), (SC, peer(D4, True))]   # 새 연결은 ended 를 받지 않는다(새로 시작)
    assert ex.state_of(SC) == ex.state_of(D4) == pc.IDLE and ex.state_of(T1) == ex.state_of(T2) == pc.IN_CALL
    assert ex.route_voice(T1, 60, 11) == T2


def test_reattach_while_being_rung_ends_the_ring_as_offline():
    ex = all_v2()
    ex.on_text(SC, call(T2), 1)
    acts = ex.attach(T2, "c2-new", 3, proto=2)
    assert (SC, ended("offline")) in sends(acts) and ex.state == pc.IDLE


# ── 무소식 45초 (§6.5) ──────────────────────────────────────────────────────
def test_silence_closes_only_the_silent_device_and_ends_only_its_call():
    ex = two_calls()                                           # 마지막 소식: T1=1, T2=2, SC=3, D4=4
    for t in (20, 40):
        for d in (T2, SC, D4):                                 # T1 만 조용하다
            ex.on_text(d, '{"t":"ping","n":1}', t)
    assert ex.tick(46.0) == []                                 # T1 의 마지막 소식 1초 → 45초 "넘어야" 끊는다
    acts = ex.tick(46.1)
    assert ("close", "c-" + T1) in acts
    assert logs(acts) == [{"ev": "silence_close", "dev": T1}, {"ev": "offline", "dev": T1},
                          {"ev": "call_end", "caller": T1, "callee": T2, "why": "lost", "ring_s": 1.0, "talk_s": 44.1}]
    assert sends(acts) == [(T2, ended("lost")), (T2, peer(T1, False)), (SC, peer(T1, False)), (D4, peer(T1, False))]
    assert not ex.is_online(T1) and ex.state_of(SC) == ex.state_of(D4) == pc.IN_CALL


def test_voice_counts_as_news_per_device_with_three_devices():
    ex = all_v2()
    ex.on_text(SC, call(T2), 1)
    ex.on_text(T2, ACCEPT, 2)
    for t in range(5, 61, 5):                                  # 통화 중인 둘은 음성이 소식이다
        assert ex.route_voice(SC, 60, t) == T2 and ex.route_voice(T2, 60, t) == SC
    acts = ex.tick(60.0)                                       # T1 은 접속(0초) 뒤로 조용했다
    assert ("close", "c-" + T1) in acts and ex.state_of(SC) == pc.IN_CALL


# ── 기록·상태 조회 형식 (§6.5) ──────────────────────────────────────────────
def test_log_records_have_only_time_device_length_and_reason_fields():
    """기록에는 기기·통화 길이·사유만 — 토큰·음성·거절 문구 번호·to 에 실려 온 글자는 어떤 경로로도 들어가지 않는다."""
    ex = pc.Exchange(QUAD)
    acts = []
    for d, p in ((T1, 1), (T2, 2), (SC, 2), (D4, 2)):
        acts += ex.attach(d, "c-" + d, 0, proto=p)
    acts += ex.on_text(SC, call(T1), 1)                        # offline (v1 을 볼 수 없다)
    acts += ex.on_text(SC, call(T2), 2) + ex.on_text(D4, call(T2), 3)      # 벨 + busy
    acts += ex.on_text(T2, '{"t":"reject","msg":2}', 4)        # 문구 번호가 든 거절
    acts += ex.on_text(SC, call("LEAK-THIS-ID"), 5) + ex.on_text(SC, '{"t":"call","to":"LEAK","token":"LEAK-TOKEN"}', 5)
    acts += ex.on_text(T2, call(T1), 6) + ex.on_text(T1, ACCEPT, 7)
    ex.route_voice(T2, 60, 8)
    acts += ex.attach(T1, "c1-new", 9, proto=2) + ex.on_text(SC, call(D4), 10) + ex.tick(41) + ex.tick(100)
    allowed = {"ev", "dev", "caller", "callee", "why", "ring_s", "talk_s", "proto"}   # proto = 그 연결의 규약 판(1·2 — 비밀이 아니다, v2 접속 줄에만)
    recs = logs(acts)
    assert {r["ev"] for r in recs} == {"online", "offline", "call_end", "silence_close"}
    assert {r["why"] for r in recs if r["ev"] == "call_end"} == {"offline", "busy", "reject", "lost", "timeout"}
    for rec in recs:
        assert set(rec) <= allowed, rec
        assert all(v in QUAD for k, v in rec.items() if k in ("dev", "caller", "callee")), rec
        assert "LEAK" not in json.dumps(rec) and "msg" not in rec


def test_snapshot_and_stats_keep_their_shape():
    ex = all_v2()
    ex.on_text(SC, call(T2), 1)
    snap = ex.snapshot()
    assert set(snap) == {"state", "online", "stats"}
    assert snap["state"] == pc.RINGING and snap["online"] == sorted(TRIO)
    assert set(snap["stats"]) == {"voice_fwd", "voice_drop_size", "voice_drop_state", "voice_drop_backlog", "bad_signal"}


def test_frames_from_a_device_that_is_not_attached_are_ignored_with_three_devices():
    ex = online((T1, 2), (T2, 2))
    assert ex.on_text(SC, call(T1), 1) == [] and ex.route_voice(SC, 60, 1) is None
    assert ex.state == pc.IDLE


# ── 독립 검수(2026-10-08) Minor 1: 접속 기록의 규약 판 ─────────────────────────
def test_online_log_marks_v2_connections_and_leaves_v1_lines_unchanged():
    import phone_core as pc
    ex = pc.Exchange(["tab5-1", "tab5-2", "stackchan-2"])
    logs1 = [a[1] for a in ex.attach("tab5-1", object(), 0.0) if a[0] == "log"]
    logs2 = [a[1] for a in ex.attach("stackchan-2", object(), 0.0, proto=pc.PROTO_V2) if a[0] == "log"]
    assert logs1 == [{"ev": "online", "dev": "tab5-1"}]                    # v1: 지금까지의 줄과 글자가 같다
    assert logs2 == [{"ev": "online", "dev": "stackchan-2", "proto": 2}]   # v2: 규약 판을 싣는다
