"""phone_smoke 스트림 모드(터널을 거친 통화 확인 도구) 시험 — 로컬 시험 교환기(임시 포트)에 붙인다. 진짜 기기·진짜 토큰 파일은 쓰지 않는다.
실행: cd server && python3 -m pytest tests/test_phone_smoke.py -q
"""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
import phone_smoke as ps
from test_phone_relay import A, B, TOK, relay  # noqa: F401 — relay 는 fixture


def test_percentile_and_summarize_numbers():
    s = ps.summarize([10.0, 20.0, 30.0, 40.0, 100.0])
    assert s == {"n": 5, "median": 30.0, "p90": 100.0, "max": 100.0}
    assert ps.summarize([]) == {"n": 0, "median": None, "p90": None, "max": None}


def test_count_loss_and_reorder():
    # 보낸 0..9 중 3·7 이 안 오고, 5 가 4 보다 먼저 왔다
    arrivals = [0, 1, 2, 5, 4, 6, 8, 9]
    r = ps.count_flow(sent=10, arrived_seqs=arrivals)
    assert r == {"sent": 10, "received": 8, "lost": 2, "reordered": 1, "duplicated": 0}
    assert ps.count_flow(sent=3, arrived_seqs=[0, 1, 1, 2])["duplicated"] == 1


@pytest.mark.parametrize("raw,http,ws", [
    ("http://127.0.0.1:5094", "http://127.0.0.1:5094", "http://127.0.0.1:5094/ws"),
    ("wss://phone.example.com/ws", "https://phone.example.com", "https://phone.example.com/ws"),
    ("wss://phone.example.com/", "https://phone.example.com", "https://phone.example.com/ws"),
    ("https://phone.example.com", "https://phone.example.com", "https://phone.example.com/ws"),
    ("ws://10.0.0.1:5094/ws", "http://10.0.0.1:5094", "http://10.0.0.1:5094/ws"),
])
def test_normalize_base(raw, http, ws):
    assert ps.normalize_base(raw) == (http, ws)


async def test_stream_through_local_relay_reports_numbers(relay):
    client, _, app = relay
    base = f"http://{client.server.host}:{client.server.port}"
    lines = []
    res = await ps.run_stream(base, seconds=0.8, tokens=TOK, out=lines.append)
    assert res["a_to_b"]["sent"] >= 20 and res["b_to_a"]["sent"] >= 20
    for d in ("a_to_b", "b_to_a"):
        assert res[d]["lost"] == 0 and res[d]["reordered"] == 0 and res[d]["duplicated"] == 0
        assert res[d]["latency"]["n"] == res[d]["received"] and res[d]["latency"]["median"] < 500
    assert res["ping"]["n"] >= 1
    text = "\n".join(lines)
    assert "중앙값" in text and "손실" in text and "순서" in text
    for secret in (TOK[A], TOK[B]):
        assert secret not in text
    assert app[phone_relay_exchange()].snapshot()["state"] == "IDLE"      # 끊고 나면 교환기는 대기 상태


def phone_relay_exchange():
    import phone_relay
    return phone_relay.EXCHANGE


async def test_stream_sizes_look_like_voice(relay):
    """음성 크기(수십~백여 바이트)의 바이너리를 보낸다 — 교환기의 3~400바이트 규칙 안."""
    sizes = ps.frame_sizes(200)
    assert min(sizes) >= 40 and max(sizes) <= 130 and len(set(sizes)) > 3


# ════════════════════════════════════════════════════════════════════════════
# 신호 규약 v2 (docs/exec-plans/2026-10-08-phone-3way-design.md §7·§12): 등록 기기가 3대여도 2대 시나리오는 옛 짝으로 그대로 돌고,
# 3대 시나리오(--three)가 "고른 상대에게만 벨 · busy · v1/v2 섞임"을 실제 소켓으로 본다.
# ════════════════════════════════════════════════════════════════════════════
import asyncio  # noqa: E402

import phone_core  # noqa: E402
import phone_relay  # noqa: E402
from test_phone_relay_v2 import SC, T1, T2, TOK3, relay3, relay_for  # noqa: E402,F401 — relay3 는 fixture


def base_of(client):
    return f"http://{client.server.host}:{client.server.port}"


async def test_two_device_stream_uses_the_legacy_pair_when_three_devices_are_registered(relay3):
    """3대가 등록된 교환기에서도 2대 시나리오는 옛 짝(tab5-1·tab5-2)으로, 옛 규약(v1) 그대로 돈다 — 스택짱 토큰으로는 붙지 않는다."""
    client, _, app = relay3
    lines = []
    res = await ps.run_stream(base_of(client), seconds=0.4, tokens=TOK3, out=lines.append)
    for d in ("a_to_b", "b_to_a"):
        assert res[d]["sent"] >= 10 and res[d]["lost"] == 0
    text = "\n".join(lines)
    assert "tab5-1→tab5-2" in text and "tab5-2→tab5-1" in text and "stackchan" not in text
    assert not any(t in text for t in TOK3.values())


async def test_short_two_device_check_passes_on_a_three_device_relay(relay3):
    client, _, app = relay3
    lines = []
    await ps.run(base_of(client), tokens=TOK3, out=lines.append)
    assert lines[-1].startswith("SMOKE OK") and "중앙값" in lines[-1]
    await asyncio.sleep(0.05)
    assert app[phone_relay.EXCHANGE].snapshot()["online"] == [] and app[phone_relay.EXCHANGE].state == phone_core.IDLE


async def test_two_device_scenario_follows_an_explicit_legacy_pair():
    """PHONE_LEGACY_PAIR 로 옛 짝을 바꿔 둔 교환기라면 2대 시나리오도 그 두 대로 돈다(to 없는 call 이 가는 곳이 옛 짝이므로)."""
    async with relay_for(TOK3, legacy_pair=(T2, SC)) as (client, _, app):
        lines = []
        await ps.run_stream(base_of(client), seconds=0.3, tokens=TOK3, legacy_pair=(T2, SC), out=lines.append)
        assert "tab5-2→stackchan-2" in "\n".join(lines)


async def test_three_device_scenario_passes_on_a_three_device_relay(relay3):
    client, _, app = relay3
    lines = []
    res = await ps.run_three(base_of(client), tokens=TOK3, out=lines.append, quiet_s=0.15)
    assert res == {"legacy_pair": (T1, T2), "third": SC}
    text = "\n".join(lines)
    for word in ("고른 상대에게만", "busy", "v1", "offline"):          # 세 가지를 실제로 지나갔다는 줄이 남는다
        assert word in text, word
    assert not any(t in text for t in TOK3.values())
    await asyncio.sleep(0.05)
    ex = app[phone_relay.EXCHANGE]
    assert ex.snapshot()["online"] == [] and ex.state == phone_core.IDLE
    assert ex.stats["bad_signal"] == 0 and ex.stats["voice_fwd"] >= 2


async def test_three_device_scenario_needs_a_third_registered_device(relay):
    client, _, _ = relay
    with pytest.raises(ValueError, match="3대"):
        await ps.run_three(base_of(client), tokens=TOK)


async def test_three_device_scenario_catches_a_relay_that_rings_everyone(relay3, monkeypatch):
    """시나리오가 정말로 잡아내는지: 벨을 모든 기기에 뿌리는 고장 난 교환기에서는 "조용해야 할 기기"에서 실패해야 한다."""
    client, _, app = relay3
    real = phone_core.Exchange._on_call

    def rings_everyone(self, dev, msg, now):
        acts = real(self, dev, msg, now)
        if any(a[0] == "send" and a[2].get("t") == "ring" for a in acts):
            acts += [("send", d, {"t": "ring", "from": dev}) for d in self.devices if d != dev and d not in self.call_of]
        return acts

    monkeypatch.setattr(phone_core.Exchange, "_on_call", rings_everyone)
    with pytest.raises(AssertionError, match="조용"):
        await ps.run_three(base_of(client), tokens=TOK3, out=lambda line: None, quiet_s=0.15)


async def test_three_device_scenario_catches_a_relay_without_busy(relay3, monkeypatch):
    """busy 를 모르는 교환기(통화 중인 상대에게도 offline 을 주는)에서는 busy 단계에서 실패해야 한다."""
    client, _, app = relay3
    monkeypatch.setattr(phone_core.Exchange, "_refuse", staticmethod(lambda caller, callee, why: [("send", caller, {"t": "ended", "why": "offline"})]))
    with pytest.raises(AssertionError, match="busy"):
        await ps.run_three(base_of(client), tokens=TOK3, out=lambda line: None, quiet_s=0.15)


@pytest.mark.parametrize("args,want", [
    ([], ("http://127.0.0.1:5094", "short", None)),
    (["http://127.0.0.1:5095"], ("http://127.0.0.1:5095", "short", None)),
    (["--stream"], ("http://127.0.0.1:5094", "stream", 30.0)),
    (["wss://phone.example.com/ws", "--stream", "60"], ("wss://phone.example.com/ws", "stream", 60.0)),
    (["--stream", "5", "http://127.0.0.1:5095"], ("http://127.0.0.1:5095", "stream", 5.0)),
    (["--three"], ("http://127.0.0.1:5094", "three", None)),
    (["http://127.0.0.1:5095", "--three"], ("http://127.0.0.1:5095", "three", None)),
])
def test_cli_modes(args, want):
    assert ps.parse_cli(args) == want


@pytest.mark.parametrize("args", [["--three", "--stream"], ["--stream", "10", "--three"], ["--four"], ["a", "b"]])
def test_cli_errors(args):
    with pytest.raises(ValueError):
        ps.parse_cli(args)
