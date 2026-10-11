"""phone_ctl 의 순수 로직 시험 — 기기도 직렬 포트도 쓰지 않는다(가짜 포트 목록·가짜 줄 공급원·가짜 시계).
실행: cd <이 폴더> && python3 -m pytest -q -p no:cacheprovider test_phone_ctl.py"""
import base64
import re

import pytest

import phone_ctl as pc


@pytest.fixture(autouse=True)
def no_real_ports(monkeypatch):
    """안전장치: 시험 중에는 실제 포트 목록 조회도, 포트 열기도 일어나면 안 된다(두 기기는 다른 펌웨어로 쓰이는 중)."""
    def boom(*a, **k):
        raise AssertionError("시험이 실제 직렬 포트에 손을 댐")
    monkeypatch.setattr(pc, "list_usb_ports", boom)
    monkeypatch.setattr(pc, "SerialLink", boom)


# ── 기기 번호 → 포트 ─────────────────────────────────────────────────────────
class FakePort:
    def __init__(self, device, serial_number):
        self.device, self.serial_number = device, serial_number


BOTH = [FakePort("/dev/cu.usbmodem2101", "AA:BB:CC:DD:EE:01"), FakePort("/dev/cu.usbmodem1101", "AA:BB:CC:DD:EE:02")]


def test_units_match_contract():
    assert pc.UNITS == {"1": "AA:BB:CC:DD:EE:01", "2": "AA:BB:CC:DD:EE:02"}


@pytest.mark.parametrize("ports", [BOTH, BOTH[::-1]], ids=["1번 먼저", "2번 먼저"])
def test_find_port_picks_by_mac_not_by_order(ports):
    """두 대가 함께 꽂혀 있어도 번호(=MAC)로 고른다 — 엉뚱한 기기를 건드리는 사고 방지."""
    assert pc.find_port("1", ports) == "/dev/cu.usbmodem2101"
    assert pc.find_port("2", ports) == "/dev/cu.usbmodem1101"


def test_find_port_ignores_other_usb_devices_and_letter_case():
    ports = [FakePort("/dev/cu.Bluetooth-Incoming-Port", None), FakePort("/dev/cu.usbserial-10", "A50285BI"),
             FakePort("/dev/cu.usbmodem1101", "aa:bb:cc:dd:ee:02")]
    assert pc.find_port("2", ports) == "/dev/cu.usbmodem1101"


def test_find_port_fails_when_unit_absent():
    with pytest.raises(SystemExit) as e:
        pc.find_port("2", BOTH[:1])
    assert "2번" in str(e.value)


def test_find_port_rejects_unknown_unit():
    with pytest.raises(SystemExit):
        pc.find_port("3", BOTH)


def test_open_links_finds_every_unit_before_opening_any(monkeypatch):
    """한 대라도 안 보이면 아무 포트도 열지 않는다. 둘 다 보이면 번호에 맞는 포트만 연다."""
    opened = []
    monkeypatch.setattr(pc, "SerialLink", lambda device: opened.append(device) or device)
    monkeypatch.setattr(pc, "list_usb_ports", lambda: BOTH[:1])
    with pytest.raises(SystemExit):
        pc.open_links(("1", "2"))
    assert opened == []
    monkeypatch.setattr(pc, "list_usb_ports", lambda: BOTH[::-1])
    assert pc.open_links(("1", "2")) == {"1": "/dev/cu.usbmodem2101", "2": "/dev/cu.usbmodem1101"}
    assert opened == ["/dev/cu.usbmodem2101", "/dev/cu.usbmodem1101"]


# ── STAT 줄 ──────────────────────────────────────────────────────────────────
def stat(fsm="IDLE", unit=2, peer=1, missed=0, screen=None):
    """시리얼 규약(README.md "시리얼 명령")의 STAT 줄 모양."""
    return (f"STAT up=120 unit={unit} fsm={fsm} screen={screen or fsm} wifi=1 relay=1 relay_code=101 peer={peer} "
            f"missed={missed} touch=3 heap_int=412000 heap_int_min=398000 heap_psram=30100000")


def test_parse_stat_reads_word_values_and_number_values():
    s = pc.parse_stat(stat())
    assert s["fsm"] == "IDLE" and s["screen"] == "IDLE"
    assert s["up"] == 120 and s["unit"] == 2 and s["relay_code"] == 101 and s["heap_psram"] == 30100000
    assert all(type(s[k]) is int for k in s if k not in ("fsm", "screen"))


@pytest.mark.parametrize("name", ["OFFLINE", "IDLE", "DIALING", "RINGBACK", "INCOMING", "ANSWERING", "IN_CALL", "ENDING"])
def test_parse_stat_reads_every_state_name(name):
    assert pc.parse_stat(stat(fsm=name))["fsm"] == name


def test_parse_stat_reads_screen_names_with_underscore():
    assert pc.parse_stat(stat(fsm="IDLE", screen="WIFI_LIST"))["screen"] == "WIFI_LIST"


def test_parse_stat_keeps_the_number_forms_of_probe_ctl():
    s = pc.parse_stat("STAT load_pct=34.8 aec_us=5200/7100 rssi=-61 id=tab5-2 ver=1.2.3")
    assert s == {"load_pct": 34.8, "aec_us": (5200, 7100), "rssi": -61, "id": "tab5-2", "ver": "1.2.3"}


@pytest.mark.parametrize("line", ["I (1) phone: hello fsm=IDLE", "STATUS fsm=IDLE", "FSM IDLE -> DIALING acts=0x1 notice=NONE", ""])
def test_parse_stat_rejects_other_lines(line):
    assert pc.parse_stat(line) is None


# ── 스크린샷 묶음 ────────────────────────────────────────────────────────────
JPEG = b"\xff\xd8" + bytes(range(256)) * 40 + b"\xff\xd9"       # 10244 B → B64 네 줄(3072×3 + 1028)


def shot_lines(data, chunk=3072, size=None, checksum=None):
    out = [f"SHOT BEGIN {len(data) if size is None else size}"]
    out += ["B64:" + base64.b64encode(data[i:i + chunk]).decode() for i in range(0, len(data), chunk)]
    out.append(f"SHOT END {sum(data) & 0xFFFFFFFF if checksum is None else checksum}")
    return out


def lossy(lines, lost_chars=128):
    """B64 한 줄의 가운데가 빠진 모양. 128글자 = 원본 96바이트(시험 펌웨어에서 실제로 겪은 유실 크기)."""
    out = list(lines)
    out[2] = out[2][:404] + out[2][404 + lost_chars:]
    return out


def test_parse_shot_ok():
    assert len(shot_lines(JPEG)) == 6
    assert pc.parse_shot(iter(["I (123) phone: 딴 로그", *shot_lines(JPEG)])) == JPEG


def test_parse_shot_detects_size_mismatch():
    with pytest.raises(ValueError, match="크기"):
        pc.parse_shot(shot_lines(JPEG, size=len(JPEG) + 2))


def test_parse_shot_detects_checksum_mismatch():
    with pytest.raises(ValueError, match="검사합"):
        pc.parse_shot(shot_lines(JPEG, checksum=1))


def test_parse_shot_detects_changed_bytes_of_same_size():
    lines = shot_lines(JPEG)
    lines[2] = "B64:" + base64.b64encode(b"\x00" * 3072).decode()
    with pytest.raises(ValueError, match="검사합"):
        pc.parse_shot(lines)


def test_parse_shot_detects_missing_end():
    with pytest.raises(ValueError, match="끝"):
        pc.parse_shot(shot_lines(JPEG)[:-1])


def test_parse_shot_reports_no_answer_at_all():
    with pytest.raises(ValueError, match="응답"):
        pc.parse_shot(["I (1) phone: 딴 로그", stat()])


@pytest.mark.parametrize("lines", [["SHOT FAIL no-mem"], ["SHOT BEGIN 10244", "B64:AAAA", "SHOT FAIL jpeg-enc"]],
                         ids=["바로 거절", "보내다 실패"])
def test_parse_shot_reports_device_failure_with_reason(lines):
    with pytest.raises(ValueError, match="기기.*(no-mem|jpeg-enc)"):
        pc.parse_shot(lines)


def test_parse_shot_ignores_whole_log_lines_between_b64_lines():
    """다른 로그 줄이 B64 줄 사이에 통째로 끼어든 경우: 그 줄은 무시되고 데이터는 온전하다."""
    lines = shot_lines(JPEG)
    lines.insert(2, "W (9012) task_wdt: Task watchdog got triggered")
    lines.insert(4, "FSM IDLE -> INCOMING acts=0x20 notice=NONE")
    assert pc.parse_shot(lines) == JPEG


def test_parse_shot_rejects_log_text_that_cut_into_a_b64_line():
    """다른 로그가 B64 줄 한가운데를 끊고 들어온 경우: 데이터가 온전하지 않으므로 거절한다."""
    lines = shot_lines(JPEG)
    b = lines[2]
    lines[2:3] = [b[:1000] + "W (9012) task_wdt: 끼어든 경고", b[1000:]]
    with pytest.raises(ValueError):
        pc.parse_shot(lines)


def test_parse_shot_rejects_a_b64_line_with_foreign_characters_even_if_the_rest_adds_up():
    """PC-D3: 검사합이 단순 합이라 약하다 — 줄 안에 base64 가 아닌 글자가 보이면, 나머지가 맞아 보여도 받지 않는다(다시 받는다)."""
    lines = shot_lines(JPEG)
    lines[2] = lines[2][:2000] + " " + lines[2][2000:]
    with pytest.raises(ValueError, match="B64"):
        pc.parse_shot(lines)


@pytest.mark.parametrize("lost_chars", [128, 100, 97], ids=["96바이트 유실", "4배수 유실", "어긋난 유실"])
def test_parse_shot_rejects_lost_bytes(lost_chars):
    with pytest.raises(ValueError):
        pc.parse_shot(lossy(shot_lines(JPEG), lost_chars))


def test_parse_shot_restarts_at_a_new_begin_and_ignores_leftovers():
    """앞 전송의 찌꺼기(B64·SHOT END)나 끊긴 SHOT BEGIN 뒤에 새 전송이 오면 새 것만 쓴다."""
    lines = ["B64:AAAA", "SHOT END 5", "SHOT BEGIN 999", "B64:AAAA", *shot_lines(JPEG)]
    assert pc.parse_shot(lines) == JPEG


@pytest.mark.parametrize("lines", [["SHOT BEGIN"], ["SHOT BEGIN abc"], ["SHOT BEGIN 0", "SHOT END 0"],
                                   ["SHOT BEGIN 3", "B64:AAEC", "SHOT END"], ["SHOT BEGIN 3", "B64:AAEC", "SHOT END xyz"]])
def test_parse_shot_rejects_broken_marker_lines(lines):
    with pytest.raises(ValueError):
        pc.parse_shot(lines)


def test_shot_complete():
    full = shot_lines(JPEG)
    assert pc.shot_complete(full) and pc.shot_complete(["SHOT FAIL no-mem"])
    assert not pc.shot_complete(full[:-1]) and not pc.shot_complete([])
    assert not pc.shot_complete(["B64:AAAA", "SHOT END 5", *full[:-1]])     # 앞 전송의 끝 표시는 끝이 아니다


# ── 단계 문법 ────────────────────────────────────────────────────────────────
def test_parse_until():
    assert pc.parse_until("until:stat:^STAT .*fsm=IDLE.*peer=1:60") == ("stat", "^STAT .*fsm=IDLE.*peer=1", 60.0)
    assert pc.parse_until("until:wifi:^WIFI ok=1 ssid=a:b:90") == ("wifi", "^WIFI ok=1 ssid=a:b", 90.0)      # 정규식 안의 ':'
    assert pc.parse_until("until:t 640 380:^OK t:2.5") == ("t 640 380", "^OK t", 2.5)                        # 명령 안의 빈칸


@pytest.mark.parametrize("step", ["until:stat", "until:stat:^STAT", "until:stat:^STAT:abc", "until::^STAT:5", "until:stat::5"])
def test_parse_until_rejects_broken_steps(step):
    with pytest.raises(ValueError):
        pc.parse_until(step)


@pytest.mark.parametrize("step, want", [
    ("1:tap:640:380", ("1", "tap:640:380")),
    ("2:wait:^FSM .* -> INCOMING:10", ("2", "wait:^FSM .* -> INCOMING:10")),
    ("both:until:stat:^STAT .*fsm=IDLE.*peer=1:60", ("both", "until:stat:^STAT .*fsm=IDLE.*peer=1:60")),
    ("1:stat", ("1", "stat")),
    ("2:1:5", ("2", "1:5")),                          # 접두사는 맨 앞 하나만 뗀다
    ("sleep:1", (None, "sleep:1")),                   # 뒤의 1 은 초 — 기기 번호가 아니다
    ("sleep:2", (None, "sleep:2")),
    ("wait:1:5", (None, "wait:1:5")),
    ("tap:1:2", (None, "tap:1:2")),
    ("stat", (None, "stat")),
    ("t 640 380", (None, "t 640 380")),
    ("3:stat", (None, "3:stat")),                     # 숫자로 시작하지만 기기 번호가 아니다
    ("12:stat", (None, "12:stat")),
    ("10:tap:1:2", (None, "10:tap:1:2")),
    ("1 :stat", (None, "1 :stat")),
    ("1", (None, "1")),
    ("both", (None, "both")),
    ("BOTH:stat", (None, "BOTH:stat")),
    ("", (None, "")),
])
def test_split_unit(step, want):
    assert pc.split_unit(step) == want


@pytest.mark.parametrize("step, want", [
    ("until:stat:^STAT .*fsm=IDLE.*peer=1:60", ("until", ("stat", "^STAT .*fsm=IDLE.*peer=1", 60.0))),
    ("wait:^FSM .* -> INCOMING:10", ("wait", ("^FSM .* -> INCOMING", 10.0))),
    ("wait:^RELAY up=1 code=101:x:2.5", ("wait", ("^RELAY up=1 code=101:x", 2.5))),
    ("sleep:1", ("sleep", (1.0,))),
    ("shot:/tmp/a b/in-call.jpg", ("shot", ("/tmp/a b/in-call.jpg",))),
    ("tap:640:380", ("tap", (640, 380))),
    ("tap:0:719", ("tap", (0, 719))),
    ("stat", ("send", ("stat",))),
    ("t 640 380", ("send", ("t 640 380",))),
    ("reboot", ("send", ("reboot",))),
])
def test_parse_step(step, want):
    assert pc.parse_step(step) == want


@pytest.mark.parametrize("step", [
    "tap:640", "tap:640:380:1", "tap:a:b", "tap:640.5:380", "tap:1280:10", "tap:10:720", "tap:-1:5",      # 화면은 1280×720
    "wait:^FSM", "wait:^FSM:abc", "wait:(:3", "wait::3", "wait:^FSM:-1",
    "sleep:", "sleep:abc", "sleep:-1", "shot:", "until:stat:^STAT", "until:stat:(:5", "",
])
def test_parse_step_rejects_broken_steps(step):
    with pytest.raises(ValueError):
        pc.parse_step(step)


def test_plan_steps_for_pair():
    plan = pc.plan_steps(["both:until:stat:^STAT:60", "1:tap:640:380", "2:wait:^FSM .* -> INCOMING:10", "sleep:1",
                          "both:stat", "2:shot:a.jpg"], pair=True)
    assert [(u, k) for u, k, _ in plan] == [(("1", "2"), "until"), (("1",), "tap"), (("2",), "wait"), (("1", "2"), "sleep"),
                                            (("1", "2"), "send"), (("2",), "shot")]
    assert plan[1][2] == (640, 380) and plan[2][2] == ("^FSM .* -> INCOMING", 10.0)


@pytest.mark.parametrize("step, why", [
    ("stat", "기기 번호"),                 # 누구에게 보낼지 모른다
    ("wait:^FSM:3", "기기 번호"),
    ("3:stat", "기기 번호"),               # 없는 번호 — 그대로 보내지 않고 알린다
    ("both:shot:a.jpg", "both"),          # 두 기기가 같은 파일에 쓰게 된다
    ("1:tap:640", "tap"),
])
def test_plan_steps_for_pair_rejects(step, why):
    with pytest.raises(ValueError, match=why):
        pc.plan_steps(["both:until:stat:^STAT:5", step], pair=True)


def test_plan_steps_for_single_unit():
    plan = pc.plan_steps(["until:stat:^STAT:30", "tap:640:380", "sleep:1", "stat"], pair=False, unit="2")
    assert [(u, k) for u, k, _ in plan] == [(("2",), "until"), (("2",), "tap"), (("2",), "sleep"), (("2",), "send")]


@pytest.mark.parametrize("step", ["1:stat", "both:stat", "2:tap:640:380"])
def test_plan_steps_for_single_unit_rejects_unit_prefix(step):
    with pytest.raises(ValueError, match="--unit"):
        pc.plan_steps([step], pair=False, unit="2")


def test_plan_steps_rejects_empty_plan():
    with pytest.raises(ValueError):
        pc.plan_steps([], pair=True)


# ── 줄 자르기·대기열 ─────────────────────────────────────────────────────────
def test_split_lines_joins_partial_reads_and_strips_cr():
    lines, rest = pc.split_lines(b"", b"PHONE READY\r\nSTAT up=1 fs")
    assert lines == ["PHONE READY"] and rest == b"STAT up=1 fs"
    lines, rest = pc.split_lines(rest, b"m=IDLE\r\n\r\n")
    assert lines == ["STAT up=1 fsm=IDLE", ""] and rest == b""


def test_split_lines_survives_garbage_and_split_hangul():
    han = "한글 줄\n".encode()
    lines, rest = pc.split_lines(b"", b"\xff\xfe boot noise\n" + han[:2])       # 한글 한 글자(3바이트)가 읽기 경계에서 잘림
    assert len(lines) == 1 and lines[0].endswith("boot noise")
    assert pc.split_lines(rest, han[2:]) == (["한글 줄"], b"")


def queue_of(*lines):
    q = pc.LineQueue()
    for ln in lines:
        q.feed(ln)
    return q


def test_queue_finds_a_line_that_already_passed():
    """다른 기기의 단계를 처리하는 사이에 이미 지나간 줄 — 뒤늦게 기다려도 대기열에서 찾는다."""
    q = queue_of("RELAY up=1 code=101 attempt=1", "FSM IDLE -> INCOMING acts=0x20 notice=NONE", "SCREEN INCOMING")
    assert q.take(r"^FSM .* -> INCOMING") == "FSM IDLE -> INCOMING acts=0x20 notice=NONE"
    assert q.lines == ["SCREEN INCOMING"]               # 찾은 줄과 그 앞은 소비되고, 뒤는 남는다


def test_queue_keeps_everything_when_nothing_matches():
    q = queue_of("a", "b")
    assert q.take("zzz") is None and q.lines == ["a", "b"]


def test_queue_consumes_in_order_so_a_line_is_never_found_twice():
    """두 번째 통화의 wait 가 첫 통화 때의 줄에 속지 않는다."""
    q = queue_of("FSM IDLE -> INCOMING n=1", "FSM INCOMING -> IDLE", "FSM IDLE -> INCOMING n=2")
    assert q.take("-> INCOMING").endswith("n=1")
    assert q.take("-> INCOMING").endswith("n=2")
    assert q.take("-> INCOMING") is None


def test_queue_mark_limits_the_search_to_lines_after_it():
    q = queue_of("OK t 1 1")
    m = q.mark()
    assert q.take("^OK t", start=m) is None             # 명령을 보내기 전에 있던 묵은 응답은 보지 않는다
    q.feed("FSM IDLE -> DIALING")
    q.feed("OK t 640 380")
    assert q.take("^OK t", start=m, through=False) == "OK t 640 380"
    assert q.lines == ["OK t 1 1", "FSM IDLE -> DIALING"]       # through=False: 그 줄만 꺼낸다


def test_queue_pull_takes_only_the_chosen_lines():
    q = queue_of("B64:old", "SHOT BEGIN 3", "FSM A -> B", "B64:AAEC", "SHOT END 3")
    assert q.pull(lambda ln: ln.startswith(("SHOT ", "B64:")), start=1) == ["SHOT BEGIN 3", "B64:AAEC", "SHOT END 3"]
    assert q.lines == ["B64:old", "FSM A -> B"]


# ── 가짜 줄 공급원 ───────────────────────────────────────────────────────────
class Clock:
    def __init__(self):
        self.t = 0.0

    def __call__(self):
        return self.t


class FakeLink:
    """직렬 포트 대역. read() 한 번에 가짜 시계가 0.05초 간다(실제 시간은 쓰지 않는다). respond 로 기기의 응답을 흉내 낸다."""

    def __init__(self, clock, respond=None):
        self.clock, self.respond = clock, respond
        self.later, self.sent = [], []                  # later: (나올 시각, 바이트) / sent: (보낸 시각, 명령)

    def say(self, *lines, at=0.0):
        self.later += [(at, (ln + "\r\n").encode()) for ln in lines]

    def read(self):
        self.clock.t += 0.05
        due = b"".join(d for t, d in self.later if t <= self.clock.t)
        self.later = [(t, d) for t, d in self.later if t > self.clock.t]
        return due

    def write(self, data):
        assert data.endswith(b"\n") and data.count(b"\n") == 1
        cmd = data.decode()[:-1]
        self.sent.append((self.clock.t, cmd))
        if self.respond:
            self.respond(self, cmd)

    @property
    def cmds(self):
        return [c for _, c in self.sent]


def make_hub(units=("1", "2")):
    clock, out = Clock(), []
    links = {u: FakeLink(clock) for u in units}
    return pc.Hub(links, clock=clock, out=out.append), links, clock, out


def test_pair_output_is_tagged_and_b64_lines_are_hidden():
    hub, links, clock, out = make_hub()
    links["1"].say("PHONE READY", "B64:AAAA")
    links["2"].say("PHONE READY")
    hub.pump(0.3)
    assert out == ["[1] PHONE READY", "[2] PHONE READY"]
    assert hub.queues["1"].lines == ["PHONE READY", "B64:AAAA"]


def test_single_unit_output_has_no_tag():
    hub, links, clock, out = make_hub(("2",))
    links["2"].say("PHONE READY")
    hub.pump(0.2)
    assert out == ["PHONE READY"]


def test_wait_finds_a_line_that_passed_while_the_other_unit_was_tapped():
    """요구사항의 예: '1:tap' 직후 '2:wait:…INCOMING' — 2번의 그 줄은 tap 을 처리하는 동안 이미 지나갔다."""
    hub, links, clock, out = make_hub()

    def respond(link, cmd):
        links["1"].say("OK " + cmd, "FSM IDLE -> DIALING acts=0x1 notice=NONE", at=clock.t)
        links["2"].say("FSM IDLE -> INCOMING acts=0x20 notice=NONE", "SCREEN INCOMING", at=clock.t)
    links["1"].respond = respond
    assert hub.tap(("1",), 640, 380)
    assert links["2"].later == [] and "[2] FSM IDLE -> INCOMING acts=0x20 notice=NONE" in out     # 이미 읽혀 화면에 찍혔다
    t = clock.t
    assert hub.wait(("2",), r"^FSM .* -> INCOMING", 10)
    assert clock.t == t                                 # 대기열에서 바로 찾았다 — 포트를 더 기다리지 않았다
    assert hub.queues["2"].lines == ["SCREEN INCOMING"]


def test_other_units_log_keeps_being_read_while_one_unit_is_waited_on():
    hub, links, clock, out = make_hub()
    for i in range(300):
        links["2"].say(f"I ({i}) phone: 2번의 수다", at=i * 0.01)
    links["1"].say("FSM RINGBACK -> IN_CALL acts=0x180 notice=NONE", at=3.0)
    assert hub.wait(("1",), r"-> IN_CALL", 10)
    assert links["2"].later == []                       # 2번 쪽에 밀린 것이 없다(계속 읽었다)
    assert len(hub.queues["2"].lines) == 300 and out.count("[2] I (299) phone: 2번의 수다") == 1


def test_wait_fails_after_the_timeout_and_names_the_unit():
    hub, links, clock, out = make_hub()
    assert not hub.wait(("2",), r"^FSM .* -> INCOMING", 3)
    assert 3.0 <= clock.t <= 3.3
    assert out[-1].startswith("!! [2] wait") and "INCOMING" in out[-1]


def test_both_wait_needs_the_line_on_each_unit():
    hub, links, clock, out = make_hub()
    links["1"].say("FSM RINGBACK -> IN_CALL acts=0x180 notice=NONE", at=0.5)
    links["2"].say("FSM ANSWERING -> IN_CALL acts=0x180 notice=NONE", at=2.0)
    assert hub.wait(("1", "2"), r"^FSM .* -> IN_CALL", 5)
    assert 2.0 <= clock.t <= 2.3

    links["1"].say("FSM IN_CALL -> ENDING acts=0x248 notice=NONE", at=clock.t)
    assert not hub.wait(("1", "2"), r"^FSM IN_CALL -> ", 2)
    fails = [ln for ln in out if ln.startswith("!!")]
    assert len(fails) == 1 and fails[0].startswith("!! [2] wait")       # 안 나온 기기만 알린다


def test_until_resends_every_two_seconds_until_the_answer_comes():
    hub, links, clock, out = make_hub(("2",))
    links["2"].respond = lambda link, cmd: len(link.sent) >= 3 and link.say(stat(), at=clock.t)
    assert hub.until(("2",), "stat", r"^STAT .*fsm=IDLE.*peer=1", 20)
    assert links["2"].cmds == ["stat"] * 3
    t = [t for t, _ in links["2"].sent]
    assert 1.9 <= t[1] - t[0] <= 2.2 and 1.9 <= t[2] - t[1] <= 2.2


def test_until_only_trusts_lines_that_came_after_its_command():
    hub, links, clock, out = make_hub(("2",))
    links["2"].say(stat(fsm="IDLE"))                    # 예전에 받아 대기열에 남아 있는 응답
    hub.pump(0.2)
    links["2"].respond = lambda link, cmd: link.say(stat(fsm="OFFLINE"), at=clock.t)
    assert not hub.until(("2",), "stat", r"^STAT .*fsm=IDLE", 5)
    assert out[-1].startswith("!! until") and "fsm=IDLE" in out[-1]


def test_until_is_a_sync_point_older_lines_are_consumed():
    """until 로 상태를 맞춘 뒤의 wait 는 그 전의 줄(부팅 때의 '-> IDLE')에 속지 않는다."""
    hub, links, clock, out = make_hub(("1",))
    links["1"].say("PHONE BOOT unit=1 id=tab5-1", "FSM OFFLINE -> IDLE acts=0x0 notice=NONE", "PHONE READY")
    links["1"].respond = lambda link, cmd: link.say(stat(unit=1), at=clock.t)
    assert hub.until(("1",), "stat", r"^STAT .*fsm=IDLE", 10)
    assert hub.queues["1"].lines == []
    assert not hub.wait(("1",), r"^FSM .* -> IDLE", 1)


def test_both_until_resends_only_to_the_unit_still_waiting():
    hub, links, clock, out = make_hub()
    links["1"].respond = lambda link, cmd: link.say(stat(unit=1), at=clock.t)
    links["2"].respond = lambda link, cmd: len(link.sent) >= 3 and link.say(stat(unit=2), at=clock.t)
    assert hub.until(("1", "2"), "stat", r"^STAT .*fsm=IDLE.*peer=1", 60)
    assert links["1"].cmds == ["stat"] and links["2"].cmds == ["stat"] * 3


def test_both_until_fails_when_one_unit_never_answers():
    hub, links, clock, out = make_hub()
    links["1"].respond = lambda link, cmd: link.say(stat(unit=1), at=clock.t)
    assert not hub.until(("1", "2"), "stat", r"^STAT ", 5)
    fails = [ln for ln in out if ln.startswith("!!")]
    assert len(fails) == 1 and fails[0].startswith("!! [2] until")


def test_tap_sends_t_waits_for_ok_and_rests_half_a_second():
    hub, links, clock, out = make_hub(("1",))
    links["1"].respond = lambda link, cmd: link.say("OK " + cmd, at=clock.t)
    assert hub.tap(("1",), 640, 380) and hub.tap(("1",), 640, 580)
    (t1, c1), (t2, c2) = links["1"].sent
    assert (c1, c2) == ("t 640 380", "t 640 580")
    assert t2 - t1 >= 0.5                               # 터치 주입 대기열(8칸)이 가득 차지 않게 간격을 둔다
    assert hub.queues["1"].lines == []                  # 'OK t' 줄은 소비됐다
    assert ">> t 640 380" in out


def test_tap_fails_without_ok():
    hub, links, clock, out = make_hub()
    assert not hub.tap(("2",), 400, 520)
    assert links["2"].cmds == ["t 400 520"] and links["1"].cmds == []      # 다시 보내지 않는다(두 번 눌릴 수 있다)
    assert pc.TAP_ACK_S <= clock.t <= pc.TAP_ACK_S + 0.5
    assert out[-1].startswith("!! [2] tap")


def test_tap_does_not_accept_a_stale_or_wrong_ok():
    hub, links, clock, out = make_hub(("1",))
    links["1"].say("OK t 640 380")                      # 앞서 남은 응답
    hub.pump(0.2)
    links["1"].respond = lambda link, cmd: link.say("OK t 640 38", at=clock.t)      # 좌표가 다르게 읽혔다
    assert not hub.tap(("1",), 640, 380)


def test_tap_keeps_event_lines_that_raced_ahead_of_the_ok():
    hub, links, clock, out = make_hub(("1",))
    links["1"].respond = lambda link, cmd: link.say("FSM IDLE -> DIALING acts=0x1 notice=NONE", "OK " + cmd, at=clock.t)
    assert hub.tap(("1",), 640, 380)
    assert hub.wait(("1",), r"^FSM IDLE -> DIALING", 0)


def shooter(clock, *variants):
    """shot 명령에 차례대로 다른 응답을 내는 기기. 마지막 응답은 되풀이한다."""
    def respond(link, cmd):
        if cmd == "shot":
            link.say(*variants[min(len(link.sent), len(variants)) - 1], at=clock.t)
    return respond


def test_shot_saves_the_jpeg(tmp_path):
    hub, links, clock, out = make_hub(("1",))
    links["1"].respond = shooter(clock, shot_lines(JPEG))
    path = tmp_path / "idle.jpg"
    assert hub.shot("1", str(path))
    assert path.read_bytes() == JPEG and links["1"].cmds == ["shot"]
    assert not any("B64:" in ln for ln in out) and hub.queues["1"].lines == []


def test_shot_retries_when_bytes_were_lost(tmp_path):
    hub, links, clock, out = make_hub(("1",))
    links["1"].respond = shooter(clock, lossy(shot_lines(JPEG)), shot_lines(JPEG))
    path = tmp_path / "a.jpg"
    assert hub.shot("1", str(path))
    assert path.read_bytes() == JPEG and links["1"].cmds == ["shot", "shot"]
    assert any(ln.startswith("!! shot") and "크기" in ln for ln in out)


def test_shot_gives_up_after_three_retries(tmp_path):
    hub, links, clock, out = make_hub(("1",))
    links["1"].respond = shooter(clock, lossy(shot_lines(JPEG)))
    path = tmp_path / "a.jpg"
    assert not hub.shot("1", str(path))
    assert pc.SHOT_RETRIES == 3 and links["1"].cmds == ["shot"] * 4        # 처음 1번 + 다시 3번
    assert not path.exists()                            # 깨진 그림을 남기지 않는다


def test_shot_shows_the_reason_the_device_gave(tmp_path):
    hub, links, clock, out = make_hub()
    links["2"].respond = shooter(clock, ["SHOT FAIL no-mem"])
    assert not hub.shot("2", str(tmp_path / "a.jpg"))
    assert any(ln.startswith("!! [2] shot") and "no-mem" in ln for ln in out)


def test_shot_private_is_not_retried_and_ends_the_step(tmp_path):
    """I-1: 비밀번호가 든 자판 화면이라 기기가 거절(SHOT FAIL private)하면 다시 받지 않고 그 단계를 실패로 끝낸다."""
    hub, links, clock, out = make_hub(("1",))
    links["1"].respond = shooter(clock, ["SHOT FAIL private"])
    path = tmp_path / "a.jpg"
    assert not hub.shot("1", str(path))
    assert links["1"].cmds == ["shot"] and not path.exists()           # 재시도 0회
    assert any(ln.startswith("!!") and "shot" in ln and "비밀번호" in ln and "private" in ln for ln in out)


def test_shot_other_fail_is_still_retried(tmp_path):
    hub, links, clock, out = make_hub(("1",))
    links["1"].respond = shooter(clock, ["SHOT FAIL no-mem"], shot_lines(JPEG))
    path = tmp_path / "a.jpg"
    assert hub.shot("1", str(path)) and path.read_bytes() == JPEG
    assert links["1"].cmds == ["shot", "shot"]


def test_shot_fails_fast_when_the_device_is_silent(tmp_path):
    hub, links, clock, out = make_hub(("1",))
    assert not hub.shot("1", str(tmp_path / "a.jpg"))
    assert clock.t <= (1 + pc.SHOT_RETRIES) * pc.SHOT_START_S + 2       # 전송 한도(60초)를 다 기다리지 않는다
    assert any("응답" in ln for ln in out) and not (tmp_path / "a.jpg").exists()


def test_shot_leaves_other_log_lines_for_a_later_wait(tmp_path):
    """캡처를 받는 십수 초 사이에 나온 FSM 줄을 잃지 않는다(B64 줄 사이에 끼어든 줄)."""
    hub, links, clock, out = make_hub(("1",))
    lines = shot_lines(JPEG)
    lines.insert(3, "FSM IN_CALL -> IDLE acts=0x240 notice=CALL_ENDED")
    links["1"].respond = shooter(clock, lines)
    path = tmp_path / "a.jpg"
    assert hub.shot("1", str(path)) and path.read_bytes() == JPEG
    assert hub.queues["1"].lines == ["FSM IN_CALL -> IDLE acts=0x240 notice=CALL_ENDED"]
    assert hub.wait(("1",), r"^FSM IN_CALL -> IDLE", 0)


def test_shot_reports_an_unwritable_path(tmp_path):
    hub, links, clock, out = make_hub(("1",))
    links["1"].respond = shooter(clock, shot_lines(JPEG))
    assert not hub.shot("1", str(tmp_path / "없는폴더" / "a.jpg"))
    assert links["1"].cmds == ["shot"] and out[-1].startswith("!! shot")      # 전송 문제가 아니므로 다시 받지 않는다


# ── 단계 실행 ────────────────────────────────────────────────────────────────
def test_run_steps_stops_at_the_first_failure():
    hub, links, clock, out = make_hub()
    assert not pc.run_steps(hub, pc.plan_steps(["1:wait:^never:1", "2:stat"], pair=True))
    assert links["2"].cmds == []


def test_run_steps_sends_plain_commands_to_the_named_units():
    hub, links, clock, out = make_hub()
    assert pc.run_steps(hub, pc.plan_steps(["1:stat", "both:reboot", "sleep:1"], pair=True))
    assert links["1"].cmds == ["stat", "reboot"] and links["2"].cmds == ["reboot"]
    assert ">> [1] stat" in out and ">> [2] reboot" in out
    assert clock.t >= 1.0


def test_run_steps_for_a_single_unit():
    hub, links, clock, out = make_hub(("2",))
    links["2"].respond = lambda link, cmd: cmd == "stat" and link.say(stat(), at=clock.t)
    plan = pc.plan_steps(["until:stat:^STAT .*fsm=IDLE:5", "stat", "wait:^STAT .*peer=1:5", "sleep:0.5"], pair=False, unit="2")
    assert pc.run_steps(hub, plan)
    assert links["2"].cmds == ["stat", "stat"] and ">> stat" in out


class FakePair:
    """시리얼 규약(README.md "시리얼 명령")의 줄 모양대로 답하는 가짜 전화기 두 대 + 교환기 흉내.
    도구의 흐름(누구에게 보내고 누구의 줄을 기다리는가)을 보려는 것이지 펌웨어를 검증하는 것이 아니다."""
    SCREEN = {"RINGBACK": "DIALING"}

    def __init__(self, clock, ring=True):
        self.clock, self.ring = clock, ring
        self.fsm = {"1": "IDLE", "2": "IDLE"}
        self.links = {u: FakeLink(clock, lambda link, cmd, u=u: self.on_cmd(u, cmd)) for u in ("1", "2")}

    def go(self, u, new, notice="NONE"):
        old, self.fsm[u] = self.fsm[u], new
        self.links[u].say(f"FSM {old} -> {new} acts=0x0 notice={notice}", f"SCREEN {self.SCREEN.get(new, new)}", at=self.clock.t)

    def on_cmd(self, u, cmd):
        me, peer, now = self.links[u], "2" if u == "1" else "1", self.clock.t
        if cmd == "stat":
            me.say(stat(fsm=self.fsm[u], unit=u, screen=self.SCREEN.get(self.fsm[u], self.fsm[u])), at=now)
        elif cmd == "shot":
            me.say(*shot_lines(JPEG), at=now)
        elif cmd.startswith("t "):
            me.say("OK " + cmd, at=now)
            if (self.fsm[u], cmd) == ("IDLE", "t 368 472"):             # 왼쪽 통화 키캡 = slot 0(Tab5 두 대는 서로의 slot 0 — main/ui_layout.h BTN_CALL)
                self.go(u, "DIALING")
                if self.ring:
                    self.go(u, "RINGBACK")
                    self.go(peer, "INCOMING")
            elif (self.fsm[u], cmd) == ("INCOMING", "t 404 428"):       # 받기
                self.go(u, "ANSWERING")
                self.go(u, "IN_CALL")
                self.go(peer, "IN_CALL")
            elif (self.fsm[u], cmd) == ("IN_CALL", "t 644 606"):        # 끊기
                self.go(u, "ENDING")
                self.go(u, "IDLE")
                self.go(peer, "IDLE", "CALL_ENDED")


def documented_scenario(tmp_path):
    """phone_ctl.py 머리 독스트링의 '예시' 단계를 그대로 꺼낸다(캡처 파일만 시험 폴더로 돌린다)."""
    block = pc.__doc__.split("예시 —")[1].split("순서대로")[0]
    steps = re.findall(r"^\s+'(.+)'", block, re.M)
    return [re.sub(r"shot:.*/", lambda m: f"shot:{tmp_path}/", s) for s in steps]


def test_documented_call_scenario_runs_against_contract_shaped_phones(tmp_path):
    clock, out = Clock(), []
    pair = FakePair(clock)
    steps = documented_scenario(tmp_path)
    assert steps[0] == "both:until:stat:^STAT .*fsm=IDLE.* peer=1:60"                                      # ' peer=1': 빈칸이 없으면 줄 끝의 call_peer=1 에도 맞는다(전화기 3대)
    assert [s for s in steps if ":tap:" in s] == ["1:tap:368:472", "2:tap:404:428", "1:tap:644:606"]      # main/ui_layout.h 버튼 가운데(통화 = 왼쪽 키캡 BTN_CALL)
    assert pc.run_steps(pc.Hub(pair.links, clock=clock, out=out.append), pc.plan_steps(steps, pair=True)), "\n".join(out)
    shots = sorted(tmp_path.glob("*.jpg"))
    assert shots and all(p.read_bytes() == JPEG for p in shots)
    assert pair.fsm == {"1": "IDLE", "2": "IDLE"}
    assert pair.links["1"].cmds.count("t 644 606") == 1 and "t 404 428" in pair.links["2"].cmds


def test_documented_call_scenario_fails_when_the_bell_never_rings(tmp_path):
    clock, out = Clock(), []
    pair = FakePair(clock, ring=False)
    plan = pc.plan_steps(documented_scenario(tmp_path), pair=True)
    assert not pc.run_steps(pc.Hub(pair.links, clock=clock, out=out.append), plan)
    assert [ln for ln in out if ln.startswith("!!")][0].startswith("!! [2] wait")
    assert pair.links["2"].cmds == ["stat"]             # 실패한 뒤의 단계(받기 터치)는 보내지 않았다


# ── 명령줄 ───────────────────────────────────────────────────────────────────
@pytest.mark.parametrize("argv, why", [
    (["pair", "1:tap:640:380", "stat"], "기기 번호"),
    (["pair"], "단계"),
    (["run", "stat"], "--unit"),
    (["--unit", "3", "run", "stat"], "--unit"),
    (["--unit", "2", "run", "1:stat"], "--unit"),
    (["--unit", "2", "run", "tap:9999:1"], "tap"),
])
def test_main_rejects_bad_input_before_touching_any_port(argv, why, capsys):
    """단계가 틀렸으면 포트 목록조차 보지 않고 끝난다(안전장치 fixture 가 손대면 시험을 깨뜨린다)."""
    assert pc.main(argv) == 2
    assert why in capsys.readouterr().err


def test_main_port_prints_the_device_path_without_opening_it(monkeypatch, capsys):
    monkeypatch.setattr(pc, "list_usb_ports", lambda: BOTH)
    assert pc.main(["port", "1"]) == 0
    assert pc.main(["--unit", "2", "port"]) == 0
    assert capsys.readouterr().out.split() == ["/dev/cu.usbmodem2101", "/dev/cu.usbmodem1101"]


def test_main_opens_the_right_units_and_returns_the_verdict(monkeypatch, capsys):
    clock, opened = Clock(), []
    pair = FakePair(clock)
    real_hub = pc.Hub
    monkeypatch.setattr(pc, "open_links", lambda units: opened.append(units) or {u: pair.links[u] for u in units})
    monkeypatch.setattr(pc, "Hub", lambda links: real_hub(links, clock=clock))
    assert pc.main(["pair", "both:until:stat:^STAT .*fsm=IDLE:5"]) == 0
    assert pc.main(["pair", "1:wait:^never:1"]) == 1
    assert pc.main(["--unit", "2", "run", "until:stat:^STAT .*fsm=IDLE:5"]) == 0
    assert opened == [("1", "2"), ("1", "2"), ("2",)]
    assert "[1] STAT " in capsys.readouterr().out


# ── Phase 3a(신호음): STAT 끝에 소리 칸이 붙고 TONE 줄이 생겨도 도구와 앞 계획의 정규식이 그대로 맞는다 ──────────
#    계획 docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md PL3a-D11. 도구(phone_ctl.py)는 고치지 않는다 — 이 시험들이 그것을 고정한다.
def stat3a(fsm="IDLE", unit=2, peer=1, tone="NONE", audio=1, screen=None, touch=3, missed=0):
    """Phase 3a 펌웨어의 STAT 줄 모양(main/app.cpp print_stat): 앞의 칸은 Phase 2 그대로, 끝에 audio= tone= aud_us=평균/최대 aud_err= stk_audio=."""
    return (f"STAT up=157 unit={unit} fsm={fsm} screen={screen or fsm} wifi=1 relay=1 relay_code=0 peer={peer} missed={missed} touch={touch} "
            f"heap_int=262000 heap_int_min=255000 heap_psram=24788704 stk_app=10280 stk_touch=3284 stk_relay=6108 stk_ws=6128 "
            f"audio={audio} tone={tone} aud_us=61/840 aud_err=0 stk_audio=4200")


def test_parse_stat_reads_the_sound_fields_and_the_old_fields_as_before():
    s = pc.parse_stat(stat3a(fsm="INCOMING", tone="BELL"))
    assert s["tone"] == "BELL" and s["audio"] == 1 and s["aud_us"] == (61, 840) and s["aud_err"] == 0 and s["stk_audio"] == 4200
    assert s["fsm"] == "INCOMING" and s["screen"] == "INCOMING" and s["touch"] == 3 and s["stk_ws"] == 6128 and s["heap_int_min"] == 255000
    assert all(type(s[k]) is int for k in s if k not in ("fsm", "screen", "tone", "aud_us"))


@pytest.mark.parametrize("tone", ["NONE", "BELL", "RINGBACK", "END"])
def test_parse_stat_reads_every_tone_name(tone):
    assert pc.parse_stat(stat3a(tone=tone))["tone"] == tone


def test_touch_audit_pattern_reads_the_new_stat():
    """touch_audit.py(저장소 밖 검증 도구)의 정규식을 글자 그대로 옮겼다 — 터치 대조가 새 STAT 에서도 같은 칸을 읽는다."""
    rx = r"^STAT up=(\d+) .*?fsm=(\w+) screen=(\w+).*?peer=(\d).*?touch=(\d+)"
    assert re.match(rx, stat3a(fsm="INCOMING", screen="DECLINE", touch=41)).groups() == ("157", "INCOMING", "DECLINE", "1", "41")


@pytest.mark.parametrize("pattern, kwargs", [
    (r"^STAT ", {}),
    (r"^STAT .*fsm=IDLE", {}),
    (r"^STAT .*fsm=IDLE.*peer=1", {}),
    (r"^STAT .*fsm=IDLE.*relay=1", {}),
    (r"^STAT .*fsm=IDLE.*relay=1.*peer=1", {}),
    (r"^STAT .*fsm=IDLE.*relay=1.*peer=1.*missed=0", {}),
    (r"^STAT .*fsm=IDLE.*screen=IDLE.*missed=0", {}),
    (r"^STAT .*fsm=IDLE screen=IDLE.*peer=1", {}),
    (r"^STAT .*screen=IDLE.*missed=1", {"missed": 1}),
    (r"^STAT .*fsm=IN_CALL.*screen=IN_CALL", {"fsm": "IN_CALL"}),
    (r"^STAT .*fsm=INCOMING screen=INCOMING", {"fsm": "INCOMING"}),
    (r"^STAT .*fsm=INCOMING screen=DECLINE", {"fsm": "INCOMING", "screen": "DECLINE"}),
    (r"^STAT .*fsm=RINGBACK screen=DIALING", {"fsm": "RINGBACK", "screen": "DIALING"}),
])
def test_stat_patterns_of_the_readme_and_earlier_plans_still_match(pattern, kwargs):
    assert re.search(pattern, stat3a(**kwargs))


def test_margin_grep_picks_up_the_new_task_stack_too():
    """기기 과제의 여유 확인(grep -oE 'heap_int_min=[0-9]+|stk_[a-z]+=[0-9]+')이 새 태스크의 스택도 집는다."""
    got = re.findall(r"heap_int_min=[0-9]+|stk_[a-z]+=[0-9]+", stat3a())
    assert got == ["heap_int_min=255000", "stk_app=10280", "stk_touch=3284", "stk_relay=6108", "stk_ws=6128", "stk_audio=4200"]


@pytest.mark.parametrize("pattern, fsm, tone, want", [
    (r"^STAT .*fsm=INCOMING.*tone=BELL", "INCOMING", "BELL", True),
    (r"^STAT .*fsm=INCOMING.*tone=BELL", "INCOMING", "NONE", False),
    (r"^STAT .*fsm=INCOMING.*tone=NONE", "INCOMING", "NONE", True),
    (r"^STAT .*fsm=INCOMING.*tone=NONE", "INCOMING", "BELL", False),
    (r"^STAT .*fsm=RINGBACK.*tone=RINGBACK", "RINGBACK", "RINGBACK", True),
    (r"^STAT .*fsm=IN_CALL.*tone=NONE", "IN_CALL", "NONE", True),
    (r"^STAT .*fsm=IN_CALL.*tone=NONE", "IN_CALL", "RINGBACK", False),
    (r"^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE", "IDLE", "NONE", True),
    (r"^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE", "IDLE", "END", False),
    (r"^STAT .*fsm=IDLE.*peer=1.*tone=NONE", "IDLE", "NONE", True),
    (r"^STAT .*fsm=IDLE.*peer=1.*tone=NONE", "IDLE", "BELL", False),
])
def test_tone_patterns_of_the_phase3a_device_task(pattern, fsm, tone, want):
    assert bool(re.search(pattern, stat3a(fsm=fsm, tone=tone))) is want


def test_audio_off_is_visible_in_the_stat():
    """소리 장치 초기화가 실패한 기기(audio=0)는 'audio=1 tone=' 을 기대하는 단계에서 걸린다."""
    assert not re.search(r"^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE", stat3a(audio=0))
    assert pc.parse_stat(stat3a(audio=0))["audio"] == 0


def test_until_stat_keeps_asking_until_the_bell_is_off():
    """벨 끄기 뒤의 확인: until 은 tone=NONE 인 STAT 이 올 때까지 2초마다 다시 묻는다. 그 사이의 TONE 줄은 여느 줄처럼 지나간다."""
    hub, links, clock, out = make_hub(("2",))
    answers = [stat3a("INCOMING", tone="BELL"), stat3a("INCOMING", tone="NONE")]

    def respond(link, cmd):
        link.say(answers[min(len(link.sent), len(answers)) - 1], at=clock.t)
        if len(link.sent) == 1:
            link.say("TONE BELL off", at=clock.t + 0.5)
    links["2"].respond = respond
    assert hub.until(("2",), "stat", r"^STAT .*fsm=INCOMING.*tone=NONE", 10)
    assert links["2"].cmds == ["stat", "stat"] and "TONE BELL off" in out


def test_tone_lines_do_not_disturb_a_tap_or_a_screenshot(tmp_path):
    """TONE 줄은 출력 태스크가 찍어 FSM 줄·OK 줄·B64 줄 사이 어디에나 올 수 있다 — tap 과 shot 은 자기 줄만 꺼내고 TONE 줄은 남긴다."""
    hub, links, clock, out = make_hub(("2",))
    links["2"].respond = lambda link, cmd: link.say("TONE BELL off", "OK " + cmd, "FSM INCOMING -> INCOMING acts=0x80 notice=NONE", at=clock.t)
    assert hub.tap(("2",), 404, 624)
    assert hub.queues["2"].lines == ["TONE BELL off", "FSM INCOMING -> INCOMING acts=0x80 notice=NONE"]
    lines = shot_lines(JPEG)
    lines.insert(2, "TONE END on")
    links["2"].respond = shooter(clock, lines)
    path = tmp_path / "a.jpg"
    assert hub.shot("2", str(path)) and path.read_bytes() == JPEG
    assert hub.queues["2"].lines == ["TONE BELL off", "FSM INCOMING -> INCOMING acts=0x80 notice=NONE", "TONE END on"]
    assert hub.wait(("2",), r"^TONE END on$", 0)


# ── Phase 3b: 목소리 통화의 새 줄(STAT 끝의 칸, VOICE·VPING·SELFTEST, 진단 명령) — 기존 도구가 고치지 않고 읽는다 ─────────────────
# 3a 의 stat3a() 는 손으로 만든 줄이었다(3a 최종 검수의 지적). 여기서는 펌웨어 소스(main/app.cpp)의 형식 문자열을 읽어 줄을 만든다
# → 펌웨어가 STAT 의 칸을 바꾸면 이 시험들이 따라 바뀐 줄을 보게 된다.
from pathlib import Path

APP_CPP = Path(__file__).resolve().parent.parent / "main" / "app.cpp"
STAT3A_FORMAT = ("STAT up=%u unit=%d fsm=%s screen=%s wifi=%d relay=%d relay_code=%d peer=%d missed=%u touch=%u heap_int=%u heap_int_min=%u heap_psram=%u"
                 " stk_app=%u stk_touch=%u stk_relay=%u stk_ws=%u audio=%d tone=%s aud_us=%u/%u aud_err=%u stk_audio=%u")   # 5d9935f(기기에 올라가 있던 판)의 줄
STAT3B_TAIL = (" amp=%d aud_late=%u voice=%d vsrc=%s v_cap=%u v_tx=%u v_txdrop=%u v_txfail=%u v_rx=%u v_play=%u v_lost=%u v_drop=%u v_under=%u v_jb=%d/%d"
               " v_aec_us=%u/%u v_enc_us=%u/%u v_dec_us=%u/%u v_mic=%u v_ref=%u v_aec=%u v_spk=%u v_rderr=%u stk_vcap=%u stk_vrx=%u stk_vtx=%u")
# fix 2 (PL3b-D25): 원인을 가르는 칸 열 개. 위 두 상수의 글자는 그대로 두고 맨 끝에만 덧붙였다.
STAT3B2_TAIL = " v_urun=%u v_ovf=%u v_catch=%u v_late=%u v_dup=%u v_bad=%u aud_gap=%u v_under_ms=%u txgain=%d vol=%d"
# 전화기 3대(2026-10-08, main/app.cpp AP-D3): slot 1 의 상대 상태와 지금·직전 통화의 상대 slot. 위 세 상수의 글자는 그대로 두고 맨 끝에만 덧붙였다.
# 스택짱 펌웨어(firmware/stackchan_phone/main/sc_app.cpp)의 STAT 도 vol= 바로 뒤에 같은 이름·같은 순서의 두 칸을 쓴다(그 뒤에 제 칸이 더 붙는다).
STAT3WAY_TAIL = " peer1=%d call_peer=%u"


def firmware_stat_format():
    """main/app.cpp 의 printf("STAT …") 형식 문자열(이어 쓴 조각을 붙인 것, 끝의 \\n 은 뗀다)."""
    src = APP_CPP.read_text(encoding="utf-8")
    m = re.search(r'printf\(((?:\s*"(?:[^"\\]|\\.)*")+)\s*,', src[src.index('printf("STAT up='):])
    return "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1))).replace("\\n", "")


def stat3b(**over):
    """펌웨어의 형식 문자열에 값을 채운 STAT 줄. 값을 주지 않은 칸은 대기 중인 기기의 값."""
    vals = {"up": 157, "unit": 2, "fsm": "IDLE", "screen": None, "wifi": 1, "relay": 1, "relay_code": 0, "peer": 1, "missed": 0, "touch": 3,
            "heap_int": 140000, "heap_int_min": 131000, "heap_psram": 24700000, "stk_app": 10280, "stk_touch": 3284, "stk_relay": 6108, "stk_ws": 6128,
            "audio": 1, "tone": "NONE", "aud_us": "61/840", "aud_err": 0, "stk_audio": 4200, "amp": 1, "aud_late": 0, "voice": 0, "vsrc": "aec",
            "v_cap": 0, "v_tx": 0, "v_txdrop": 0, "v_txfail": 0, "v_rx": 0, "v_play": 0, "v_lost": 0, "v_drop": 0, "v_under": 0, "v_jb": "0/3",
            "v_aec_us": "0/0", "v_enc_us": "0/0", "v_dec_us": "0/0", "v_mic": 0, "v_ref": 0, "v_aec": 0, "v_spk": 0, "v_rderr": 0,
            "stk_vcap": 46000, "stk_vrx": 47000, "stk_vtx": 7000,
            "v_urun": 0, "v_ovf": 0, "v_catch": 0, "v_late": 0, "v_dup": 0, "v_bad": 0, "aud_gap": 16100, "v_under_ms": 0, "txgain": 4, "vol": 100,
            "peer1": 1, "call_peer": 0}
    vals.update(over)
    if vals["screen"] is None:
        vals["screen"] = vals["fsm"]
    out = []
    for word in firmware_stat_format().split(" "):
        key, eq, spec = word.partition("=")
        out.append(f"{key}={vals[key]}" if eq else word)
        assert not eq or key in vals, f"펌웨어 STAT 에 이 시험이 모르는 칸이 있다: {word}"
    return " ".join(out)


def test_firmware_stat_keeps_every_old_field_and_only_appends():
    """기존 줄 모양은 한 글자도 바꾸지 않는다: 펌웨어의 형식 문자열은 3a 의 것으로 시작하고, 3b 의 칸은 그 뒤에만 붙는다."""
    fmt = firmware_stat_format()
    assert fmt.startswith(STAT3A_FORMAT)
    assert fmt.startswith(STAT3A_FORMAT + STAT3B_TAIL)
    assert fmt.startswith(STAT3A_FORMAT + STAT3B_TAIL + STAT3B2_TAIL)
    assert fmt == STAT3A_FORMAT + STAT3B_TAIL + STAT3B2_TAIL + STAT3WAY_TAIL


def test_stat3b_is_built_from_the_firmware_source():
    line = stat3b()
    assert line.startswith("STAT up=157 unit=2 fsm=IDLE screen=IDLE wifi=1 relay=1 relay_code=0 peer=1 missed=0 touch=3 heap_int=140000 ")
    assert " audio=1 tone=NONE aud_us=61/840 aud_err=0 stk_audio=4200 amp=1 aud_late=0 voice=0 vsrc=aec v_cap=0 " in line and " stk_vcap=46000 stk_vrx=47000 stk_vtx=7000 v_urun=0 " in line and line.endswith(" aud_gap=16100 v_under_ms=0 txgain=4 vol=100 peer1=1 call_peer=0")
    assert "%" not in line and "\\" not in line


def test_parse_stat_reads_the_voice_fields_and_the_old_fields_as_before():
    s = pc.parse_stat(stat3b(fsm="IN_CALL", voice=1, vsrc="tone", v_cap=1500, v_tx=1196, v_rx=1190, v_play=1187, v_lost=1, v_drop=2, v_jb="2/3",
                             v_aec_us="2810/3400", v_enc_us="2200/3900", v_dec_us="310/450", v_mic=52, v_ref=590, v_aec=9, v_spk=1410, amp=0, aud_late=0))
    assert s["voice"] == 1 and s["vsrc"] == "tone" and s["v_cap"] == 1500 and s["v_tx"] == 1196 and s["v_rx"] == 1190 and s["v_play"] == 1187
    assert s["v_lost"] == 1 and s["v_drop"] == 2 and s["v_under"] == 0 and s["v_jb"] == (2, 3) and s["amp"] == 0 and s["aud_late"] == 0
    assert s["v_aec_us"] == (2810, 3400) and s["v_enc_us"] == (2200, 3900) and s["v_dec_us"] == (310, 450)
    assert s["v_mic"] == 52 and s["v_ref"] == 590 and s["v_aec"] == 9 and s["v_spk"] == 1410 and s["v_rderr"] == 0
    assert s["fsm"] == "IN_CALL" and s["screen"] == "IN_CALL" and s["touch"] == 3 and s["tone"] == "NONE" and s["aud_us"] == (61, 840) and s["stk_ws"] == 6128
    assert all(type(s[k]) is int for k in s if k not in ("fsm", "screen", "tone", "vsrc", "aud_us", "v_jb", "v_aec_us", "v_enc_us", "v_dec_us"))


def test_touch_audit_pattern_reads_the_phase3b_stat():
    rx = r"^STAT up=(\d+) .*?fsm=(\w+) screen=(\w+).*?peer=(\d).*?touch=(\d+)"
    assert re.match(rx, stat3b(fsm="INCOMING", screen="DECLINE", touch=41)).groups() == ("157", "INCOMING", "DECLINE", "1", "41")


@pytest.mark.parametrize("pattern, kwargs", [
    (r"^STAT ", {}),
    (r"^STAT .*fsm=IDLE.*peer=1", {}),
    (r"^STAT .*fsm=IDLE.*relay=1.*peer=1.*missed=0", {}),
    (r"^STAT .*fsm=IDLE screen=IDLE.*peer=1", {}),
    (r"^STAT .*fsm=IN_CALL.*screen=IN_CALL", {"fsm": "IN_CALL"}),
    (r"^STAT .*fsm=INCOMING screen=DECLINE", {"fsm": "INCOMING", "screen": "DECLINE"}),
    (r"^STAT .*fsm=RINGBACK screen=DIALING", {"fsm": "RINGBACK", "screen": "DIALING"}),
    (r"^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE", {}),
    (r"^STAT .*fsm=INCOMING.*tone=BELL", {"fsm": "INCOMING", "tone": "BELL"}),
    (r"^STAT .*fsm=IN_CALL.*tone=NONE", {"fsm": "IN_CALL"}),
    (r"^STAT .*fsm=IDLE.*peer=1.*tone=NONE", {}),
])
def test_stat_patterns_of_the_readme_and_earlier_plans_match_the_phase3b_stat(pattern, kwargs):
    assert re.search(pattern, stat3b(**kwargs))


def test_margin_grep_picks_up_the_three_voice_task_stacks():
    got = re.findall(r"heap_int_min=[0-9]+|stk_[a-z]+=[0-9]+", stat3b())
    assert got == ["heap_int_min=131000", "stk_app=10280", "stk_touch=3284", "stk_relay=6108", "stk_ws=6128", "stk_audio=4200",
                   "stk_vcap=46000", "stk_vrx=47000", "stk_vtx=7000"]


@pytest.mark.parametrize("pattern, kwargs, want", [
    (r"^STAT .*fsm=IN_CALL.*tone=NONE.* voice=1 ", {"fsm": "IN_CALL", "voice": 1}, True),
    (r"^STAT .*fsm=IN_CALL.*tone=NONE.* voice=1 ", {"fsm": "IN_CALL", "voice": 0}, False),
    (r"^STAT .*fsm=IDLE.*peer=1.*tone=NONE.* voice=0 ", {}, True),
    (r"^STAT .*fsm=IDLE.*peer=1.*tone=NONE.* voice=0 ", {"voice": 1}, False),
    (r"^STAT .*fsm=INCOMING.*tone=BELL.* voice=0 ", {"fsm": "INCOMING", "tone": "BELL"}, True),
    (r"^STAT .*audio=1 .* amp=0 ", {"amp": 0}, True),
    (r"^STAT .*audio=1 .* amp=0 ", {"amp": 1}, False),
    (r"^STAT .*audio=1 .* amp=1 ", {"amp": 1}, True),
    (r"^STAT .*audio=0 ", {"audio": 0, "amp": 0}, True),
    (r"^STAT .* voice=1 vsrc=tone ", {"fsm": "IN_CALL", "voice": 1, "vsrc": "tone"}, True),
    (r"^STAT .* voice=1 vsrc=tone ", {"fsm": "IN_CALL", "voice": 1, "vsrc": "aec"}, False),
    (r"^STAT .* voice=0 vsrc=aec ", {}, True),
])
def test_voice_patterns_of_the_phase3b_device_task(pattern, kwargs, want):
    assert bool(re.search(pattern, stat3b(**kwargs))) is want


def test_diagnostic_commands_are_sent_as_plain_lines():
    """진단 명령(quiet·selftest·vsrc·vping·fault)은 도구가 모르는 낱말이라 그대로 한 줄로 나간다 — 도구를 고치지 않는다."""
    steps = ["both:quiet on", "2:selftest", "1:vsrc tone", "2:vsrc ref", "1:vping", "2:fault audio", "both:quiet off"]
    plan = pc.plan_steps(steps, pair=True)
    assert [(units, kind, args) for units, kind, args in plan] == [
        (("1", "2"), "send", ("quiet on",)), (("2",), "send", ("selftest",)), (("1",), "send", ("vsrc tone",)), (("2",), "send", ("vsrc ref",)),
        (("1",), "send", ("vping",)), (("2",), "send", ("fault audio",)), (("1", "2"), "send", ("quiet off",))]
    hub, links, clock, out = make_hub()
    assert pc.run_steps(hub, plan)
    assert links["1"].cmds == ["quiet on", "vsrc tone", "vping", "quiet off"] and links["2"].cmds == ["quiet on", "selftest", "vsrc ref", "fault audio", "quiet off"]


def test_selftest_and_vping_answers_are_found_by_wait():
    hub, links, clock, out = make_hub()

    def respond(link, cmd):
        if cmd == "selftest":
            link.say("SELFTEST amp=0 mic=41 ref=588 aec=12 ok=1", at=clock.t + 0.8)
        elif cmd == "vping":
            link.say("OK vping", at=clock.t)
            link.say("VPING rt_ms=212", at=clock.t + 0.9)
        elif cmd.startswith("vsrc "):
            link.say("OK " + cmd, at=clock.t)
        elif cmd.startswith("quiet "):
            link.say(f"OK {cmd} amp={0 if cmd.endswith('on') else 1}", at=clock.t)
    links["1"].respond = links["2"].respond = respond
    plan = pc.plan_steps(["both:quiet on", r"both:wait:^OK quiet on amp=0$:3", "2:selftest", r"2:wait:^SELFTEST amp=0 mic=\d+ ref=\d+ aec=\d+ ok=1$:5",
                          "2:vsrc ref", r"2:wait:^OK vsrc ref$:3", "1:vping", r"1:wait:^OK vping$:3", r"1:wait:^VPING rt_ms=\d+$:5"], pair=True)
    assert pc.run_steps(hub, plan), "\n".join(out)
    assert "[1] VPING rt_ms=212" in out and "[2] SELFTEST amp=0 mic=41 ref=588 aec=12 ok=1" in out


def test_voice_lines_do_not_disturb_a_tap_and_can_be_waited_for_in_order():
    """VOICE 줄은 app 태스크가 그 원인인 FSM 줄 바로 앞에 찍는다 — tap 은 자기 줄(OK t)만 꺼내고, 뒤의 wait 가 VOICE → FSM 순서로 찾는다."""
    hub, links, clock, out = make_hub(("1",))
    links["1"].respond = lambda link, cmd: link.say("OK " + cmd, "TONE END on", "VOICE off", "FSM IN_CALL -> ENDING acts=0x248 notice=NONE", at=clock.t)
    assert hub.tap(("1",), 644, 606)
    assert hub.queues["1"].lines == ["TONE END on", "VOICE off", "FSM IN_CALL -> ENDING acts=0x248 notice=NONE"]
    assert hub.wait(("1",), r"^VOICE off$", 0) and hub.wait(("1",), r"^FSM IN_CALL -> ENDING acts=0x248 notice=NONE$", 0)


def test_a_failed_selftest_or_vping_is_visible():
    assert not re.search(r"^SELFTEST amp=\d mic=\d+ ref=\d+ aec=\d+ ok=1$", "SELFTEST amp=0 mic=40 ref=12 aec=3 ok=0")
    assert not re.search(r"^SELFTEST amp=\d mic=\d+ ref=\d+ aec=\d+ ok=1$", "SELFTEST FAIL busy-or-no-audio")
    assert not re.search(r"^VPING rt_ms=\d+$", "VPING timeout")


# ── fix 2 (PL3b-D23~D25): 새 칸, 새 명령의 응답, VPING 의 새 꼬리 ────────────────────────────────────────────────────────────────────────────
class _Sources:
    """시리얼 명령의 소스 = phone_main.cpp(기기별: stat·t·shot·quiet·volstep…) + voice_cmd.cpp(소리·통화 품질 명령 — 2026-10-08 에 떼어 스택짱 전화기와 함께 쓴다).
    아래 시험들은 "펌웨어의 명령 소스가 이 모양이다"를 고정한다 — 두 파일을 이어 붙여 읽는다. 순서는 명령을 보는 순서와 같다: voice_cmd(…·vol·…) → phone_main 의 뒷부분(volstep → ERR unknown)."""

    def __init__(self, *paths):
        self.paths = paths
        self.parent = paths[0].parent

    def read_text(self, encoding="utf-8"):
        return "\n".join(p.read_text(encoding=encoding) for p in self.paths)


PHONE_MAIN_CPP = _Sources(Path(__file__).resolve().parent.parent / "main" / "voice_cmd.cpp", Path(__file__).resolve().parent.parent / "main" / "phone_main.cpp")
VOICE_CPP = Path(__file__).resolve().parent.parent / "main" / "voice.cpp"


def test_parse_stat_reads_the_split_counters_and_the_runtime_values():
    s = pc.parse_stat(stat3b(fsm="IN_CALL", voice=1, v_drop=7, v_urun=2, v_ovf=3, v_catch=1, v_late=1, v_dup=1, v_bad=1, aud_gap=48512, v_under_ms=3250, txgain=2, vol=60))
    assert s["v_urun"] == 2 and s["v_ovf"] == 3 and s["v_catch"] == 1 and s["v_late"] == 1 and s["v_dup"] == 1 and s["v_bad"] == 1
    assert s["aud_gap"] == 48512 and s["v_under_ms"] == 3250 and s["txgain"] == 2 and s["vol"] == 60
    assert s["v_drop"] == 7 and s["v_jb"] == (0, 3) and s["stk_vtx"] == 7000 and s["amp"] == 1      # 앞의 칸은 그대로 읽힌다
    assert all(type(s[k]) is int for k in ("v_urun", "v_ovf", "v_catch", "v_late", "v_dup", "v_bad", "aud_gap", "v_under_ms", "txgain", "vol"))


def test_the_old_patterns_still_match_a_line_with_the_new_tail_and_the_new_fields_can_be_found():
    line = stat3b(fsm="IN_CALL", voice=1, txgain=3, vol=80, v_under_ms=1200)
    assert re.search(r"^STAT .*fsm=IN_CALL.*tone=NONE.* voice=1 ", line) and re.search(r"^STAT .*audio=1 .* amp=1 ", line)
    assert re.search(r"stk_vtx=(\d+)", line).group(1) == "7000"                                    # 마지막 칸이 아니게 되어도 칸 이름으로 읽는다
    assert re.search(r" txgain=(\d) vol=(\d+) ", line).groups() == ("3", "80") and re.search(r" v_under_ms=(\d+) ", line).group(1) == "1200"   # vol= 은 이제 마지막 칸이 아니다(전화기 3대의 꼬리가 붙었다) — 끝을 묶지 않는다
    assert re.findall(r"heap_int_min=[0-9]+|stk_[a-z]+=[0-9]+", line)[-3:] == ["stk_vcap=46000", "stk_vrx=47000", "stk_vtx=7000"]
    assert not re.search(r"\bv_late=.*\baud_late=", line)                                         # 이름이 겹치는 aud_late 와 v_late 는 따로 읽힌다
    assert re.search(r" aud_late=(\d+) ", line).group(1) == "0" and re.search(r" v_late=(\d+) ", line).group(1) == "0"


def vping_line_from_firmware(rt, depth, target):
    """voice.cpp 의 printf("VPING rt_ms=…") 형식 문자열에 값을 채운 줄."""
    m = re.search(r'printf\("(VPING rt_ms=%d[^"]*)\\n", \(int\)r, rs\.depth, rs\.target\)', VOICE_CPP.read_text(encoding="utf-8"))
    assert m, "voice.cpp 에서 VPING 형식을 찾지 못함"
    return m.group(1).replace("%d", "{}").format(rt, depth, target)


def test_vping_line_grows_a_tail_at_the_end_only():
    line = vping_line_from_firmware(212, 3, 5)
    assert line == "VPING rt_ms=212 jb=3/5"
    assert line.startswith("VPING rt_ms=212")                                                        # 앞의 글자는 그대로
    assert re.search(r"^VPING rt_ms=(\d+)", line).group(1) == "212"                                   # 끝을 묶지 않는 옛 식은 그대로 맞는다
    assert re.search(r"^VPING rt_ms=(\d+) jb=(\d+)/(\d+)$", line).groups() == ("212", "3", "5")
    # 알아 둘 것: 끝을 $ 로 묶은 식(^VPING rt_ms=\d+$ — 작업 폴더의 voice_check.py 와 기기 과제의 wait)은 새 줄에 맞지 않는다 → 그 식은 (?: jb=\d+/\d+)? 를 붙여야 한다
    assert not re.search(r"^VPING rt_ms=\d+$", line) and re.search(r"^VPING rt_ms=\d+(?: jb=\d+/\d+)?$", line)
    assert re.search(r"^VPING rt_ms=\d+(?: jb=\d+/\d+)?$", "VPING rt_ms=212")                          # 옛 줄(d77ae98)도 같은 식으로 읽힌다
    assert not re.search(r"^VPING rt_ms=\d+(?: jb=\d+/\d+)?$", "VPING timeout")
    assert 'printf("VPING timeout\\n")' in VOICE_CPP.read_text(encoding="utf-8")                      # 시간 초과 줄은 그대로


def test_the_runtime_command_answers_have_the_documented_shape():
    src = PHONE_MAIN_CPP.read_text(encoding="utf-8")
    for want in ('"OK txgain %d\\n"', '"ERR txgain 1-%d\\n"', '"OK vol %d\\n"', '"ERR vol 0-100\\n"', '"ERR vol no-audio\\n"', '"ERR vol i2c\\n"', '"ERR vsrc speaker-on\\n"', '"ERR vsrc\\n"', '"OK vsrc %s\\n"'):
        assert want in src, want
    assert re.search(r"^OK txgain [1-4]$", "OK txgain 2") and re.search(r"^ERR txgain 1-4$", "ERR txgain 1-4") and re.search(r"^OK vol (?:100|[1-9]?\d)$", "OK vol 60")


def test_runtime_commands_are_sent_as_plain_lines_and_the_answers_can_be_waited_for():
    hub, links, clock, out = make_hub(("1",))

    def respond(link, cmd):
        if cmd.startswith("txgain "):
            link.say("OK txgain " + cmd.split()[1] if cmd.split()[1] in "1234" else "ERR txgain 1-4", at=clock.t)
        elif cmd.startswith("vol "):
            link.say("OK vol " + cmd.split()[1], at=clock.t)
        elif cmd.startswith("vsrc "):
            link.say("ERR vsrc speaker-on", at=clock.t)
    links["1"].respond = respond
    plan = pc.plan_steps(["1:txgain 2", r"1:wait:^OK txgain 2$:3", "1:txgain 9", r"1:wait:^ERR txgain 1-4$:3", "1:vol 60", r"1:wait:^OK vol 60$:3",
                          "1:vsrc ref", r"1:wait:^ERR vsrc speaker-on$:3"], pair=True)
    assert pc.run_steps(hub, plan), "\n".join(out)
    assert links["1"].cmds == ["txgain 2", "txgain 9", "vol 60", "vsrc ref"]


# ── Phase 3c (계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md) ─────────────────────────────────────────────────────────────────
# 기기에서만 드러나는 것 가운데 "소스가 이 모양이다"로 고정할 수 있는 것을 고정한다(동작은 기기 과제가 본다).
def test_phase3c_the_capture_stack_is_32k_internal_and_the_sender_stack_is_in_psram():
    """PL3c-D1·D2: 내부 메모리 24KB 를 되찾는다 — 캡처 태스크 48 → 32KB(내부 그대로), 송신 태스크 8KB 는 PSRAM 으로. 해제 태스크는 3b 그대로 PSRAM."""
    src = VOICE_CPP.read_text(encoding="utf-8")
    assert re.search(r"\bCAP_STACK = 32 \* 1024;", src) and not re.search(r"\bCAP_STACK = 48 \* 1024;", src)
    assert 'xTaskCreatePinnedToCore(cap_task, "v_cap", CAP_STACK, nullptr, CAP_PRIO, &s_cap_task, CAP_CORE)' in src          # 무거운 쪽은 내부 메모리 스택 그대로
    assert re.search(r"\bTX_STACK = 8 \* 1024;", src) and re.search(r"\bTX_STACK_CAPS = MALLOC_CAP_SPIRAM \| MALLOC_CAP_8BIT;", src)
    assert 'xTaskCreatePinnedToCoreWithCaps(tx_task, "v_tx", TX_STACK, nullptr, TX_PRIO, &s_tx_task, TX_CORE, TX_STACK_CAPS)' in src
    assert 'xTaskCreatePinnedToCoreWithCaps(rx_task, "v_rx", RX_STACK, nullptr, RX_PRIO, &s_rx_task, RX_CORE, RX_STACK_CAPS)' in src


# ── Phase 3c: 통화 품질 손잡이(PL3c-D3) — 새 줄 TUNE, 손잡이 명령의 응답. STAT 줄은 바뀌지 않는다 ───────────────────────────────────────────
VOICE_TUNE_H = Path(__file__).resolve().parent.parent / "main" / "voice_tune.h"
VOICE_CORE_H = Path(__file__).resolve().parent.parent / "main" / "voice_core.h"
TUNE3C_FORMAT = ("TUNE dflt=%d legacy=%d meas=%d txgain=%d vol=%d mic=%d kbps=%d lvl=%d tilt=%d res=%d jb=%d nodelay=%d vtone=%d ps=%d nd_socks=%d"
                 " g_q8=%d supp=%u far=%u rxgap=%u mclip=%u")


def firmware_tune_format():
    """main/phone_main.cpp 의 printf("TUNE …") 형식 문자열(끝의 \\n 은 뗀다)."""
    src = PHONE_MAIN_CPP.read_text(encoding="utf-8")
    m = re.search(r'printf\("(TUNE (?:[^"\\]|\\.)*)\\n",', src)
    assert m, "phone_main.cpp 에서 TUNE 형식을 찾지 못함"
    return m.group(1)


def tune_line(**over):
    """펌웨어의 형식 문자열에 값을 채운 TUNE 줄. 값을 주지 않은 칸은 5935ddd 와 같은 설정으로 대기 중인 기기의 값."""
    vals = {"dflt": 1, "legacy": 1, "meas": 1, "txgain": 4, "vol": 100, "mic": 24, "kbps": 24, "lvl": 0, "tilt": 0, "res": 0, "jb": 0, "nodelay": 0, "vtone": 1000,
            "ps": 0, "nd_socks": -1, "g_q8": 0, "supp": 0, "far": 0, "rxgap": 0, "mclip": 0}
    vals.update(over)
    out = []
    for word in firmware_tune_format().split(" "):
        key, eq, spec = word.partition("=")
        assert not eq or key in vals, f"펌웨어 TUNE 에 이 시험이 모르는 칸이 있다: {word}"
        out.append(f"{key}={vals[key]}" if eq else word)
    return " ".join(out)


def parse_kv(line):
    return {k: int(v) for k, v in re.findall(r"(\w+)=(-?\d+)", line)}


def test_phase3c_stat_line_is_exactly_the_5935ddd_line():
    """3c 는 STAT 에 칸을 더하지 않는다(새 값은 TUNE 줄에) — 기존 도구·기록 창·앞 계획의 식이 그대로 맞는다."""
    assert firmware_stat_format() == STAT3A_FORMAT + STAT3B_TAIL + STAT3B2_TAIL + STAT3WAY_TAIL     # (전화기 3대의 두 칸은 그 뒤에 붙었다 — 3c 까지의 글자는 그대로)
    assert stat3b().endswith(" aud_gap=16100 v_under_ms=0 txgain=4 vol=100 peer1=1 call_peer=0")


def test_phase3c_tune_line_format_is_pinned():
    assert firmware_tune_format() == TUNE3C_FORMAT
    line = tune_line()
    assert line == "TUNE dflt=1 legacy=1 meas=1 txgain=4 vol=100 mic=24 kbps=24 lvl=0 tilt=0 res=0 jb=0 nodelay=0 vtone=1000 ps=0 nd_socks=-1 g_q8=0 supp=0 far=0 rxgap=0 mclip=0"
    assert "%" not in line and pc.parse_stat(line) is None                                           # STAT 줄이 아니다 — parse_stat 은 건드리지 않는다
    d = parse_kv(tune_line(legacy=0, lvl=12, g_q8=3072, nd_socks=1, rxgap=262, ps=-1))
    assert d["legacy"] == 0 and d["lvl"] == 12 and d["g_q8"] == 3072 and d["nd_socks"] == 1 and d["rxgap"] == 262 and d["ps"] == -1 and d["mic"] == 24
    assert len(d) == 20


@pytest.mark.parametrize("pattern, kwargs, want", [
    (r"^TUNE dflt=1 legacy=1 meas=1 ", {}, True),                                                    # 손잡이를 건드리지 않은 기기(기본값을 굳히기 전의 빌드)
    (r"^TUNE dflt=1 legacy=1 meas=1 ", {"legacy": 0, "lvl": 12}, False),
    (r"^TUNE .* legacy=1 ", {"dflt": 0}, True),                                                      # 굳힌 빌드에서 시리얼로 옛 값을 준 기기
    (r"^TUNE .* meas=1 ", {"meas": 0, "vol": 80}, False),
    (r"^TUNE .* ps=0 ", {}, True),
    (r"^TUNE .* ps=0 ", {"ps": 1}, False),
    (r"^TUNE .* nodelay=1 .* nd_socks=1 ", {"nodelay": 1, "nd_socks": 1}, True),
    (r"^TUNE .* lvl=12 tilt=0 res=0 ", {"lvl": 12}, True),
])
def test_phase3c_tune_patterns_of_the_device_tasks(pattern, kwargs, want):
    assert bool(re.search(pattern, tune_line(**kwargs))) is want


def tune_header_ints():
    return {k: int(v) for k, v in re.findall(r"\b([A-Z][A-Z_]+[A-Z])\s*= (-?\d+)\b", VOICE_TUNE_H.read_text(encoding="utf-8"))}


def test_phase3c_knob_answers_have_the_documented_shape_and_their_ranges_match_the_header():
    src = PHONE_MAIN_CPP.read_text(encoding="utf-8")
    for want in ('printf("OK %s %d\\n", name, get())', 'printf("ERR %s %s\\n", name, range)', '"OK mic %d\\n"', '"ERR mic 12-24 step 3\\n"', '"ERR mic no-audio\\n"', '"ERR mic i2c\\n"',
                 'knob_cmd(cmd, "lvl", 0, voicetune::LVL_MAX, "0|6-16"', 'knob_cmd(cmd, "tilt", 0, voicetune::TILT_MAX, "0-2"', 'knob_cmd(cmd, "res", 0, voicetune::RES_DB_MAX, "0|6|12|18"',
                 'knob_cmd(cmd, "opus", voicetune::KBPS_MIN, voicetune::KBPS_MAX, "12-40"', 'knob_cmd(cmd, "jb", 0, voicetune::JB_MAX, "0|3-10"',
                 'knob_cmd(cmd, "vtone", voicetune::VTONE_MIN, voicetune::VTONE_MAX, "300-7000"', 'knob_cmd(cmd, "nodelay", 0, 1, "0|1"', 'cmd == "tune"'):
        assert want in src, want
    h = tune_header_ints()                                                                           # 응답에 적힌 범위 = voice_tune.h 의 범위
    assert (h["LVL_MIN"], h["LVL_MAX"], h["TILT_MAX"], h["RES_DB_STEP"], h["RES_DB_MAX"]) == (6, 16, 2, 6, 18)
    assert (h["KBPS_MIN"], h["KBPS_MAX"], h["JB_MIN"], h["JB_MAX"], h["VTONE_MIN"], h["VTONE_MAX"]) == (12, 40, 3, 10, 300, 7000)
    assert (h["MIC_DB_MIN"], h["MIC_DB_MAX"], h["MIC_DB_STEP"]) == (12, 24, 3)
    assert (h["TXGAIN_MIN"], h["TXGAIN_MAX"], h["VOL_MIN"], h["VOL_MAX"]) == (1, 4, 0, 100)                # 3b 의 txgain·vol 과 같은 범위
    assert h["MIC_DB_MAX"] == h["LEGACY_MIC_DB"] == 24                                                # 마이크 증폭은 3b 의 값보다 올리지 못한다(고리 이득)
    assert '"probe"' in VOICE_CORE_H.read_text(encoding="utf-8")                                     # vsrc probe


def test_phase4_echo_guard_knob_is_a_plain_knob_outside_the_tune_line():
    """P4H-D1 메아리 지킴 손잡이: `guard`(지금 값) · `guard <0..16>` → `OK guard <n>` / `ERR guard 0-16`. 저장하지 않는다. TUNE·STAT 줄은 그대로(위의 시험들이 고정한다)."""
    src = PHONE_MAIN_CPP.read_text(encoding="utf-8")
    assert 'knob_cmd(cmd, "guard", 0, voicetune::GUARD_MAX, "0-16", [] { return voice::guard(); }, [](int v) { return voice::set_guard(v); })' in src
    h = tune_header_ints()
    assert h["GUARD_MAX"] == 16 and 0 < h["GUARD_DEFAULT"] <= h["GUARD_MAX"]                          # 부팅 값은 켜져 있다(0 = 771b9fe 의 동작)
    assert "guard=" not in firmware_tune_format() and "guard" not in firmware_stat_format()          # 줄 모양은 건드리지 않았다
    voice_cpp = (PHONE_MAIN_CPP.parent / "voice.cpp").read_text(encoding="utf-8")
    assert "s_guard{voicetune::GUARD_DEFAULT}" in voice_cpp and "s_tilt.load(), s_guard.load()}" in voice_cpp   # 부팅 값 = 헤더 한 곳, 캡처 태스크가 묶음마다 읽는다


def test_p5a_rest_overlap_and_rxgain_knobs_are_plain_knobs_outside_the_tune_line():
    """P5A(2026-10-08) 쉼 누르기 `rest` · 겹침 누르기 `dt` · 받는 목소리 이득 `rxgain`: guard 와 같은 모양의 손잡이다 — 저장하지 않고, TUNE·STAT 줄은 그대로다."""
    src = PHONE_MAIN_CPP.read_text(encoding="utf-8")
    assert 'knob_cmd(cmd, "rest", 0, voicetune::REST_MAX, "0|6|12|18", [] { return voice::rest(); }, [](int v) { return voice::set_rest(v); })' in src
    assert 'knob_cmd(cmd, "dt", 0, voicetune::DT_MAX, "0|6|12", [] { return voice::dt(); }, [](int v) { return voice::set_dt(v); })' in src
    assert 'knob_cmd(cmd, "rxgain", voicetune::RXGAIN_MIN, voicetune::RXGAIN_MAX, "100-300", [] { return voice::rxgain(); }, [](int v) { return voice::set_rxgain(v); })' in src
    assert src.index('knob_cmd(cmd, "res",') < src.index('knob_cmd(cmd, "rest",')              # "res" 는 이름 뒤가 빈칸·끝일 때만 맞는다 → "rest 12" 를 가로채지 않는다(knob_cmd)
    h = tune_header_ints()
    assert (h["REST_STEP"], h["REST_MAX"], h["DT_STEP"], h["DT_MAX"], h["RXGAIN_MIN"], h["RXGAIN_MAX"]) == (6, 18, 6, 12, 100, 300)
    assert 0 < h["REST_DEFAULT"] <= h["REST_MAX"] and 0 < h["DT_DEFAULT"] <= h["DT_MAX"] and 100 < h["RXGAIN_DEFAULT"] <= h["RXGAIN_MAX"]   # 부팅 값은 켜져 있다
    fmt = firmware_tune_format() + firmware_stat_format()
    assert "rest=" not in fmt and "dt=" not in fmt and "rxgain=" not in fmt                      # 줄 모양은 건드리지 않았다
    voice_cpp = (PHONE_MAIN_CPP.parent / "voice.cpp").read_text(encoding="utf-8")
    assert "s_rest{voicetune::REST_DEFAULT}, s_dt{voicetune::DT_DEFAULT}, s_rxgain{voicetune::RXGAIN_DEFAULT}" in voice_cpp      # 부팅 값 = 헤더 한 곳
    assert "dp.rest_db = s_rest.load(); dp.dt_db = s_dt.load();" in voice_cpp                    # 캡처 태스크가 묶음마다 읽는다
    assert "s_rx->set_play_gain_q8(voicetune::RXGAIN_DEFAULT * 256 / 100);" in voice_cpp         # 부팅 때 받는 쪽에 건다
    core = (PHONE_MAIN_CPP.parent / "voice_core.h").read_text(encoding="utf-8")
    assert "g > 256) pcm::gain_soft_q8(pcm, FRAME, g, PLAY_KNEE, PLAY_CEIL);" in core and "else pcm::gain_q8(pcm, FRAME, g);" in core   # 키울 때만 부드럽게 누른다(1배면 예전 그대로)
    assert "constexpr int PLAY_KNEE = 16000, PLAY_CEIL = 26000;" in core                         # 천장은 스피커 한계(약 -2dBFS) 아래
    assert 'knob_cmd(cmd, "hpf", 0, 1, "0|1", [] { return voice::hpf(); }, [](int v) { return voice::set_hpf(v); })' in src
    assert "s_hpf{voicetune::HPF_DEFAULT}" in voice_cpp and "dp.hpf = s_hpf.load();" in voice_cpp and h["HPF_DEFAULT"] in (0, 1)



def test_phase3c_boot_defaults_live_on_nine_lines_the_freeze_tool_can_rewrite():
    """굳히기 과제(freeze_tune.py)는 voice_tune.h 의 DEFAULT_* 아홉 줄의 숫자만 고친다 — 그 줄의 모양을 고정한다(값은 고정하지 않는다)."""
    lines = re.findall(r"^constexpr int DEFAULT_([A-Z_]+)\s+= (\d+);", VOICE_TUNE_H.read_text(encoding="utf-8"), re.M)
    assert [k for k, _ in lines] == ["TXGAIN", "VOL", "MIC_DB", "KBPS", "LVL", "TILT", "RES_DB", "JB", "NODELAY"]
    h = tune_header_ints()
    assert (h["LEGACY_TXGAIN"], h["LEGACY_VOL"], h["LEGACY_MIC_DB"], h["LEGACY_KBPS"], h["LEGACY_LVL"], h["LEGACY_TILT"], h["LEGACY_RES_DB"], h["LEGACY_JB"], h["LEGACY_NODELAY"]) == (4, 100, 24, 24, 0, 0, 0, 0, 0)
    assert h["DEFAULT_TXGAIN"] == 4                                                                  # 3b 의 시험이 TX_GAIN_DEFAULT = 4 를 고정한다(평탄화가 그 위를 맡는다) — 굳히기에서 바꾸지 않는다


def test_phase3c_knob_commands_are_sent_as_plain_lines_and_the_answers_can_be_waited_for():
    hub, links, clock, out = make_hub()

    def respond(link, cmd):
        word = cmd.split()
        if cmd == "tune":
            link.say(tune_line(legacy=0, lvl=12, g_q8=1024), at=clock.t)
        elif word[0] in ("lvl", "tilt", "res", "opus", "jb", "vtone", "nodelay", "mic") and len(word) == 2:
            link.say(f"ERR lvl 0|6-16" if cmd == "lvl 3" else f"OK {word[0]} {word[1]}", at=clock.t)
        elif cmd == "vsrc probe":
            link.say("OK vsrc probe", at=clock.t)
    links["1"].respond = links["2"].respond = respond
    steps = ["both:lvl 12", r"both:wait:^OK lvl 12$:3", "1:lvl 3", r"1:wait:^ERR lvl 0\|6-16$:3", "2:tilt 1", r"2:wait:^OK tilt 1$:3", "2:res 12", r"2:wait:^OK res 12$:3",
             "1:mic 18", r"1:wait:^OK mic 18$:3", "1:opus 32", r"1:wait:^OK opus 32$:3", "both:jb 4", r"both:wait:^OK jb 4$:3", "both:nodelay 1", r"both:wait:^OK nodelay 1$:3",
             "1:vtone 3000", r"1:wait:^OK vtone 3000$:3", "1:vsrc probe", r"1:wait:^OK vsrc probe$:3", "both:tune", r"both:wait:^TUNE dflt=\d legacy=0 .* lvl=12 .* g_q8=1024 :3"]
    plan = pc.plan_steps(steps, pair=True)
    assert pc.run_steps(hub, plan), "\n".join(out)
    assert links["1"].cmds == ["lvl 12", "lvl 3", "mic 18", "opus 32", "jb 4", "nodelay 1", "vtone 3000", "vsrc probe", "tune"]
    assert links["2"].cmds == ["lvl 12", "tilt 1", "res 12", "jb 4", "nodelay 1", "tune"]


# ── Phase 3c: 미룬 지적(fix 2 재검수 Minor 2·4·6, 범위 밖 1 / fix 3 검수 Minor ①)과 수신 무음 감시의 연결 — 소스 모양으로 고정 ───────────────────
AUDIO_OUT_CPP = Path(__file__).resolve().parent.parent / "main" / "audio_out.cpp"
NET_WIFI_CPP = Path(__file__).resolve().parent.parent / "main" / "net_wifi.cpp"


def test_phase3c_the_amp_is_forced_off_right_after_m5_begin_and_on_every_failure_path():
    """fix 2 재검수 Minor 4(G1 은 시험이 없다): M5.begin 바로 뒤, NVS·화면·소리 초기화보다 먼저 앰프를 끈다. 실패 경로는 잠금이 있으면 조건 없이 끄고, 못 끄면 알린다(Minor 2)."""
    main = PHONE_MAIN_CPP.read_text(encoding="utf-8")
    begin, off, nvs, guard, start = (main.index(x) for x in ("M5.begin(cfg);", "audioout::amp_off_at_boot(i2c_lock);", "nvs_flash_init()", 'boot_guard(M5.Display.width() != 0, "panel")', "audioout::start(i2c_lock,"))
    assert begin < off < nvs < guard < start
    audio = AUDIO_OUT_CPP.read_text(encoding="utf-8")
    fail = audio[audio.index("void fail(const char *stage)"):]
    fail = fail[:fail.index("\n}\n")]
    assert "if (s_lock && !amp_set(false)) ESP_LOGW(" in fail and "amp could not be switched off after the failure" in fail
    assert "s_amp" not in re.sub(r"//[^\n]*", "", fail)                                              # 끌지 말지를 s_amp(기록된 값)에 묻지 않는다 — 잠금만 있으면 끈다(주석은 빼고 본다)
    boot = audio[audio.index("void amp_off_at_boot("):audio.index("bool start(SemaphoreHandle_t")]
    assert "if (amp_set(false)) ESP_LOGI(" in boot and "amp could not be forced off at boot" in boot


def test_phase3c_a_block_read_across_a_restart_is_dropped_and_selftest_bumps_the_generation():
    """Minor 6: 마이크를 읽는 16ms 사이에 끄기→켜기가 지나가면 그 묶음을 버린다(셈·에코 제거·보내기보다 앞에서). 범위 밖 1: 점검도 세대를 올린다."""
    src = VOICE_CPP.read_text(encoding="utf-8")
    read, stale, count, aec = (src.index(x) for x in ("audioout::read_mic(s_slots, BLOCK)", "if (gen.stale(s_gen.load())) continue;", "++s_cap_blocks;", "aec_process(s_aec,"))
    assert read < stale < count < aec
    assert src.count("s_gen.fetch_add(1);") == 2
    selftest = src[src.index("bool selftest(SelfTest *out)"):]
    selftest = selftest[:selftest.index("\n}\n")]
    assert selftest.index("compare_exchange_strong(expect, Mode::TEST)") < selftest.index("s_gen.fetch_add(1);") < selftest.index("xTaskNotifyGive(s_cap_task);")


def test_phase3c_wifi_power_save_is_switched_off_once_and_read_back_periodically():
    """fix 3 검수 Minor ①: 절전 끄기는 esp_wifi_start 뒤 한 번, 지금 값은 매니저 태스크가 신호 세기와 함께 읽어 TUNE 의 ps= 로 내보낸다."""
    src = NET_WIFI_CPP.read_text(encoding="utf-8")
    assert src.count("esp_wifi_set_ps(WIFI_PS_NONE)") == 1 and src.index("esp_wifi_start()") < src.index("esp_wifi_set_ps(WIFI_PS_NONE)")
    rssi = src[src.index("void refresh_rssi()"):]
    rssi = rssi[:rssi.index("\n}\n")]
    assert "esp_wifi_get_ps(&ps) == ESP_OK ? (int)ps : -1" in rssi and src.count("esp_wifi_get_ps(") == 1
    assert "int ps() { return s_ps.load(); }" in src and "netwifi::ps()" in PHONE_MAIN_CPP.read_text(encoding="utf-8")


def test_phase3c_the_receive_watchdog_runs_only_in_a_call_with_voice_on():
    """PL3c-D10: VOICE_LOST 는 통화 중이고 목소리가 실제로 켜져 있을 때만, 통화가 열릴 때 새로 시작한 감시가 알릴 때만 넣는다."""
    src = APP_CPP.read_text(encoding="utf-8")
    assert "if (s_fsm.state == State::IN_CALL && voice::on() && s_rx_watch.check((uint32_t)now, voice::rx_frames()))" in src
    assert src.count("feed(Ev::VOICE_LOST, now);") == 1 and src.count("s_rx_watch.start((uint32_t)now);") == 1
    assert src.index("feed(Ev::TICK, now);") < src.index("feed(Ev::VOICE_LOST, now);")
    apply = src[src.index("void apply(const Out &o, State before, int64_t now)"):src.index("void feed(Ev ev")]
    assert apply.index('printf("VOICE %s\\n"') < apply.index("s_rx_watch.start((uint32_t)now);") < apply.index("static const uint16_t SENDS[]")
    assert firmware_stat_format() == STAT3A_FORMAT + STAT3B_TAIL + STAT3B2_TAIL + STAT3WAY_TAIL       # app.cpp 를 고쳤어도 STAT 줄은 그대로다(+ 전화기 3대의 꼬리)


# ════ 음량 단계(P4V — 화면의 작게/크게, vol_steps.h·ui_layout.h·app.cpp) ════
VOL_STEPS_H = Path(__file__).resolve().parent.parent / "main" / "vol_steps.h"
UI_LAYOUT_H = Path(__file__).resolve().parent.parent / "main" / "ui_layout.h"
VOLSTEP_RE = r"^VOLSTEP step=([1-5]) max=5 vol=(100|[1-9]?\d) saved=([01])$"


def test_volume_taps_in_the_docstring_are_the_button_centers_and_the_answer_line_matches_the_firmware():
    rects = {n: tuple(int(v) for v in m.split(",")) for n, m in re.findall(r"constexpr Rect\s+(BTN_VOL_UP|BTN_VOL_DN)\s*=\s*\{([^}]*)\}", UI_LAYOUT_H.read_text(encoding="utf-8"))}
    centers = {n: (x + w // 2, y + h // 2) for n, (x, y, w, h) in rects.items()}
    assert centers == {"BTN_VOL_UP": (1168, 312), "BTN_VOL_DN": (1168, 608)}
    assert "크게 = tap:1168:312, 작게 = tap:1168:608" in pc.__doc__
    assert pc.parse_step("tap:1168:312") == ("tap", (1168, 312)) and pc.parse_step("tap:1168:608") == ("tap", (1168, 608))
    fmt = re.search(r'snprintf\(buf, n, "(VOLSTEP [^"]*)"', VOL_STEPS_H.read_text(encoding="utf-8"))[1]
    assert fmt == "VOLSTEP step=%d max=%d vol=%d saved=%d"                                             # 독스트링·보고서가 적은 모양
    assert re.search(VOLSTEP_RE, fmt.replace("%d", "{}").format(3, 5, 72, 0)) and re.search(VOLSTEP_RE, fmt.replace("%d", "{}").format(5, 5, 100, 1))
    assert "'VOLSTEP step=<1..5> max=5 vol=<0..100> saved=<0|1>'" in pc.__doc__


def test_volume_commands_and_taps_share_one_path_in_the_firmware():
    """P4V-D3: 화면 버튼(on_tap)과 시리얼 명령(대기열의 vol_req)이 vol_do 하나를 탄다. 코덱 값은 기존 'vol' 이 쓰던 audioout::set_volume 그대로, 저장은 app 태스크의 한 곳에서만."""
    app = APP_CPP.read_text(encoding="utf-8")
    assert "case Tap::VOL_UP: vol_do(uilayout::vol_req_of(t), 0, now); break;" in app
    assert "if (tap.vol_req) vol_do((volsteps::Req)tap.vol_req, tap.vol_arg, now);" in app
    do = app[app.index("void vol_do(volsteps::Req req, int arg, int64_t now)"):app.index("bool same(const Model &a, const Model &b)")]
    assert do.count("audioout::set_volume(volsteps::vol_of(target))") == 1 and "toneout::play" not in do and "vol_store" not in do     # 확인음 없음, 누를 때 플래시에 쓰지 않는다
    assert do.index("audioout::set_volume") < do.index("s_vol_step = target;") < do.index("s_vol_saver.changed(target, now);")        # 코덱 쓰기가 성공한 뒤에만 단계를 바꾼다
    assert app.count("vol_store(s_vol_step)") == 1 and app.count("nvs_set_u8(h, VOL_KEY") == 1
    assert "if (s_vol_saver.should_write(now, (s_fsm.state == State::IDLE || s_fsm.state == State::OFFLINE) && toneout::now() == tones::Tone::NONE)) {" in app
    start = app[app.index("void start(int unit, SemaphoreHandle_t i2c_lock)"):app.index("void inject_tap(int x, int y)")]
    assert start.index("s_vol_step = vol_load();") < start.index("audioout::set_volume(volsteps::vol_of(s_vol_step))") < start.index("xTaskCreatePinnedToCore(app_task")   # 태스크가 뜨기 전에 건다
    assert "else if (s_model.vol_step != s_drawn.vol_step) {" in app and "ui::draw_volume(s_model); gfx::present();" in app       # 음량만 바뀌면 그 칸만
    main = PHONE_MAIN_CPP.read_text(encoding="utf-8")
    assert "else if (const volsteps::Cmd vc = volsteps::parse_cmd(cmd.c_str()); vc.mine) {" in main
    assert '"ERR volstep 1-%d\\n"' in main and '"ERR volstep busy-or-no-app\\n"' in main and "app::vol_request((int)vc.req, vc.arg)" in main
    assert '"OK vol %d\\n"' in main and 'cmd == "vol" || cmd.rfind("vol ", 0) == 0' in main          # 진단용 vol 은 그대로
    assert main.index('cmd == "vol" || cmd.rfind("vol ", 0) == 0') < main.index("volsteps::parse_cmd(cmd.c_str())") < main.index('printf("ERR unknown\\n")')
    assert firmware_stat_format() == STAT3A_FORMAT + STAT3B_TAIL + STAT3B2_TAIL + STAT3WAY_TAIL       # STAT 줄은 그대로다(음량 단계는 VOLSTEP 줄로 읽는다. + 전화기 3대의 꼬리)


def test_volume_steps_can_be_driven_and_waited_for_with_plain_steps():
    """기기 확인 절차의 모양: 주입 탭·명령 → VOLSTEP 줄 → 3초 뒤 saved=1. 가짜 기기는 펌웨어의 줄 모양대로 답한다."""
    hub, links, clock, out = make_hub(("1",))
    table, state = {1: 52, 2: 62, 3: 72, 4: 82, 5: 100}, {"step": 5, "saved": 5}

    def line(link):
        link.say(f"VOLSTEP step={state['step']} max=5 vol={table[state['step']]} saved={int(state['step'] == state['saved'])}", at=clock.t)

    def respond(link, cmd):
        if cmd == "t 1168 608" or cmd == "voldn":
            state["step"] = max(1, state["step"] - 1)
        elif cmd == "t 1168 312" or cmd == "volup":
            state["step"] = min(5, state["step"] + 1)
        elif re.fullmatch(r"volstep [1-5]", cmd):
            state["step"] = int(cmd[-1])
        elif cmd.startswith("volstep "):
            link.say("ERR volstep 1-5", at=clock.t)
            return
        elif cmd != "volstep":
            return
        if cmd.startswith("t "):
            link.say("OK " + cmd, at=clock.t)
        line(link)
        if cmd != "volstep" and state["step"] != state["saved"]:
            state["saved"] = state["step"]
            link.say(f"VOLSTEP step={state['step']} max=5 vol={table[state['step']]} saved=1", at=clock.t + 3.0)
    links["1"].respond = respond
    steps = ["until:volstep:^VOLSTEP step=5 max=5 vol=100 saved=1$:5", "tap:1168:608", "wait:^VOLSTEP step=4 max=5 vol=82 saved=0$:3", "wait:^VOLSTEP step=4 .*saved=1$:10",
             "until:volstep 2:^VOLSTEP step=2 max=5 vol=62 saved=0$:5", "until:volstep 9:^ERR volstep 1-5$:5", "until:volup:^VOLSTEP step=3 max=5 vol=72 :5",
             "tap:1168:312", "wait:^VOLSTEP step=4 max=5 vol=82 :3"]
    assert pc.run_steps(hub, pc.plan_steps(steps, pair=False, unit="1")), "\n".join(out)
    seen = [ln[ln.index("VOLSTEP"):] for ln in out if re.match(r"(?:\[\d\] )?VOLSTEP step=", ln)]
    assert len(seen) >= 6 and all(re.search(VOLSTEP_RE, ln) for ln in seen)
    assert links["1"].cmds == ["volstep", "t 1168 608", "volstep 2", "volstep 9", "volup", "t 1168 312"]


# ── 수정 1회차(검수 Important 1·2) ──
def test_the_docstring_fixes_the_measuring_procedure_for_a_turned_down_phone():
    """Important 1: 줄여 둔 기기의 SELFTEST ok=0 은 고장이 아니다 — 재기 전에 단계를 읽어 두고 맨 위로 올렸다가 되돌리는 절차를 도구 설명글에 못 박는다."""
    doc = pc.__doc__
    assert "'volstep' 으로 지금 단계 <n> 을 읽어 둔다 → 'volstep 5' → selftest(또는 vping) → 'volstep <n>' 으로 되돌린다" in doc
    assert "줄여 둔 기기의 'SELFTEST … ok=0' 과 그 뒤의 'TUNE … meas=0 … vol=<100 아님>' 은 고장이 아니다" in doc
    assert "'ok=1' 만 기다리는 단계는 위 절차 없이는 실패한다" in doc and "vping 은 두 기기 모두 맨 위 단계여야 한다" in doc and "'quiet on' 인 채로" in doc
    assert "재려면 'vol 100'" not in doc                                                                # 옛 문구(저장 안 되는 진단 명령으로 재기)는 지웠다 — 절차는 하나만
    main = PHONE_MAIN_CPP.read_text(encoding="utf-8")                                                  # 펌웨어 쪽: SELFTEST 줄(성공·FAIL 둘 다) 뒤에 TUNE 줄을 붙이는 조건은 그대로다
    st = main[main.index('else if (cmd == "selftest") {'):main.index('else if (cmd.rfind("vsrc ", 0) == 0) {')]
    assert st.index('printf("SELFTEST amp=%d') < st.index('printf("SELFTEST FAIL busy-or-no-audio\\n")') < st.index("if (!meas_valid_now()) print_tune();") and st.count("print_tune();") == 1
    assert re.search(r'printf\("TUNE dflt=%d legacy=%d meas=%d txgain=%d vol=%d ', main)


def test_the_measuring_procedure_works_as_plain_steps_and_ok1_alone_fails_on_a_turned_down_phone():
    table, state = {1: 52, 2: 62, 3: 72, 4: 82, 5: 100}, {"step": 3}

    def respond(link, cmd):
        if re.fullmatch(r"volstep [1-5]", cmd):
            state["step"] = int(cmd[-1])
        if cmd.startswith("volstep"):
            link.say(f"VOLSTEP step={state['step']} max=5 vol={table[state['step']]} saved=0", at=clock.t)
        elif cmd == "selftest":
            top = state["step"] == 5
            link.say(f"SELFTEST amp=0 mic=40 ref={380 if top else 120} aec=3 ok={int(top)}", at=clock.t)
            if not top:
                link.say(f"TUNE dflt=0 legacy=0 meas=0 txgain=4 vol={table[state['step']]} mic=24 kbps=24 lvl=12 tilt=0 res=0 jb=0 nodelay=0 vtone=1000 ps=0 nd_socks=1 g_q8=0 supp=0 far=0 rxgap=0 mclip=0", at=clock.t)
    hub, links, clock, out = make_hub(("1",))
    links["1"].respond = respond
    naive = pc.plan_steps(["until:selftest:^SELFTEST .*ok=1$:5"], pair=False, unit="1")
    assert not pc.run_steps(hub, naive)                                                                # 줄여 둔 기기: ok=1 만 기다리면 실패로 보인다
    hub, links, clock, out = make_hub(("1",))
    links["1"].respond = respond
    told = pc.plan_steps(["selftest", "wait:^SELFTEST .*ok=0$:3", "wait:^TUNE .*meas=0 .*vol=72 :3"], pair=False, unit="1")
    assert pc.run_steps(hub, told), "\n".join(out)                                                     # 그때 뒤따르는 TUNE 줄이 까닭(meas=0, vol=72)을 말한다
    hub, links, clock, out = make_hub(("1",))
    links["1"].respond = respond
    proc = pc.plan_steps(["until:volstep:^VOLSTEP step=3 :5", "until:volstep 5:^VOLSTEP step=5 max=5 vol=100 :5", "until:selftest:^SELFTEST .*ok=1$:5",
                          "until:volstep 3:^VOLSTEP step=3 max=5 vol=72 :5"], pair=False, unit="1")
    assert pc.run_steps(hub, proc), "\n".join(out)                                                     # 설명글의 절차: 읽어 두고 → 맨 위 → 재기 → 되돌리기
    assert links["1"].cmds == ["volstep", "volstep 5", "selftest", "volstep 3"] and state["step"] == 3


def test_saved_volume_is_applied_right_after_the_sound_device_opens_and_before_the_sound_tasks():
    """Important 2: 저장된 음량은 audioout::start 바로 뒤, 신호음·목소리·Wi-Fi·교환기를 시작하기 전에 건다(그 사이 코덱이 100 인 구간을 없앤다). app::start 의 적용은 그대로 둔다."""
    main = PHONE_MAIN_CPP.read_text(encoding="utf-8")
    order = [main.index(x) for x in ("audioout::start(i2c_lock, !quiet_get(), injected);", "app::vol_apply_saved();", "toneout::start();", "voice::start();", "netwifi::start();", "app::start(me.unit, i2c_lock);")]
    assert order == sorted(order) and main.count("app::vol_apply_saved();") == 1
    assert main.index("nvs_flash_init();") < order[1]                                                  # 저장소는 그 앞에서 이미 열렸다
    app = APP_CPP.read_text(encoding="utf-8")
    fn = app[app.index("void vol_apply_saved()"):]
    fn = fn[:fn.index("\n}\n")]
    assert "const int step = vol_load();" in fn and "if (!volsteps::boot_needs_write(audioout::ready(), audioout::volume(), step)) return;" in fn
    assert "audioout::set_volume(volsteps::vol_of(step))" in fn and "printf(" not in fn             # 새 시리얼 줄을 만들지 않는다(기록은 로그로만)
    load = app[app.index("int vol_load()"):]
    assert "volsteps::from_saved(v)" in load[:load.index("\n}\n")]                                    # 검증은 vol_steps.h 그대로
    start = app[app.index("void start(int unit, SemaphoreHandle_t i2c_lock)"):app.index("void inject_tap(int x, int y)")]
    assert "s_vol_step = vol_load();" in start and "vol_line();" in start                           # app::start 의 같은 코드는 유지 — 단계 기억과 VOLSTEP 부팅 줄
    assert 'printf("AUDIO ready rate=%d block=%d vol=%d dma=%d mic=%d amp=%d\\n", RATE, BLOCK, VOLUME, DMA_DESC, MIC_GAIN_DB, (int)s_amp.load());' in AUDIO_OUT_CPP.read_text(encoding="utf-8")   # 부팅 줄은 그대로
    assert firmware_stat_format() == STAT3A_FORMAT + STAT3B_TAIL + STAT3B2_TAIL + STAT3WAY_TAIL



# ── 전화기 3대(2026-10-08): 통화 키캡 둘 · STAT 꼬리 peer1= call_peer= ──────────────────────────────────────────
UI_LAYOUT_H = Path(__file__).resolve().parent.parent / "main" / "ui_layout.h"


def test_3way_stat_tail_is_read_and_the_old_fields_keep_their_meaning():
    s = pc.parse_stat(stat3b(peer=0, peer1=1, call_peer=1, fsm="INCOMING", screen="INCOMING"))
    assert s["peer"] == 0 and s["peer1"] == 1 and s["call_peer"] == 1                                   # peer= 는 slot 0, peer1= 은 slot 1, call_peer= 는 건 쪽의 slot
    assert s["fsm"] == "INCOMING" and s["touch"] == 3 and s["vol"] == 100 and s["txgain"] == 4
    assert type(s["peer1"]) is int and type(s["call_peer"]) is int
    rx = r"^STAT up=(\d+) .*?fsm=(\w+) screen=(\w+).*?peer=(\d).*?touch=(\d+)"                         # touch_audit.py 의 식: 첫 peer= (slot 0)를 읽는다
    assert re.match(rx, stat3b(peer=0, peer1=1, call_peer=1)).groups() == ("157", "IDLE", "IDLE", "0", "3")


def test_3way_peer_patterns_need_a_leading_space():
    """'call_peer=1' 안에 'peer=1' 이 들어 있다 → 빈칸 없이 쓴 식은 slot 0 이 꺼져 있어도 맞는다. 문서·예시는 ' peer=1' 로 쓴다.
    (이 시험의 첫 줄이 깨지면 칸 이름이 바뀌어 함정이 사라진 것이다 — 그때는 문서의 주의를 지워도 된다.)"""
    line = stat3b(peer=0, peer1=1, call_peer=1)
    assert re.search(r"^STAT .*fsm=IDLE.*peer=1", line)                                                # 함정: slot 0 은 꺼져 있다
    assert not re.search(r"^STAT .*fsm=IDLE.* peer=1", line)                                           # 빈칸을 두면 속지 않는다
    assert re.search(r"^STAT .*fsm=IDLE.* peer=1", stat3b(peer=1, peer1=0, call_peer=0))
    assert re.search(r"^STAT .* peer1=1", line) and not re.search(r"^STAT .* peer1=1", stat3b(peer=1, peer1=0))
    assert ".* peer=1:60'" in pc.__doc__ and ".*peer=1" not in pc.__doc__                              # 머리 주석의 예시에 빈칸 없는 꼴이 남아 있지 않다


def test_3way_documented_tap_coordinates_are_the_layout_constants():
    """머리 주석·README 의 통화 키캡 좌표가 main/ui_layout.h 의 상수와 같다(왼쪽 = slot 0, 오른쪽 = slot 1)."""
    src = UI_LAYOUT_H.read_text(encoding="utf-8")
    a = re.search(r"constexpr Circle BTN_CALL\s*=\s*\{\s*(\d+),\s*(\d+),\s*(\d+)\s*\}", src)
    b = re.search(r"constexpr Circle BTN_CALL_B\s*=\s*\{\s*(\d+),\s*(\d+),\s*(\d+)\s*\}", src)
    assert a and b
    ax, ay, ar = map(int, a.groups()); bx, by, br = map(int, b.groups())
    assert (ax, ay) == (368, 472) and (bx, by) == (912, 472) and ar == br and ax < bx
    assert f"'1:tap:{ax}:{ay}'" in pc.__doc__ and f"왼쪽 {ax},{ay} = slot 0" in pc.__doc__ and f"오른쪽 {bx},{by} = slot 1" in pc.__doc__
    readme = (Path(__file__).resolve().parent.parent / "README.md").read_text(encoding="utf-8")
    assert f"{ax},{ay} · {bx},{by}" in readme and f'"1:tap:{ax}:{ay}"' in readme and "640,360" not in readme and "tap:640:360" not in readme
    app = APP_CPP.read_text(encoding="utf-8")
    assert "case Tap::CALL_B: feed(Ev::TAP_CALL, now, Why::NONE, false, 0, (uint8_t)uilayout::call_slot_of(t)); break;" in app      # 두 키캡이 같은 길로, slot 만 달리


def test_3way_app_wires_the_peer_model_and_the_after_bell_guard():
    """순수 헤더의 판단(ui_layout.h — 맥 시험이 고정한다)을 app.cpp 가 실제로 부르는가. 호출이 빠져도 헤더 시험은 통과하므로 이음을 글자로 묶는다."""
    app = APP_CPP.read_text(encoding="utf-8")
    refresh = app[app.index("void refresh(int64_t now)"):app.index("void on_tap(int x, int y, int64_t now)")]
    on_tap = app[app.index("void on_tap(int x, int y, int64_t now)"):app.index("void touch_task(void *)")]
    # 그릴 내용: 상대별 접속 상태와 통화 상대를 상태 기계에서 그대로
    assert "for (int slot = 0; slot < uilayout::MAX_PEERS; ++slot) m.peer_online[slot] = s_fsm.online(slot);" in refresh
    assert "m.call_peer = s_fsm.call_peer;" in refresh
    assert "uilayout::peers_same(a, b)" in app[app.index("bool same(const Model &a, const Model &b)"):app.index("void refresh(int64_t now)")]
    # 벨 뒤 가드(UL-D7·AP-D4): 화면 이름이 바뀔 때만, 떠나는 화면으로 앞 화면을 갱신한 뒤에 지금 화면을 바꾼다
    change = refresh[refresh.index("if (strcmp(screen, s_screen) != 0) {"):]
    assert change.index("s_prev_shown = uilayout::guard_prev(s_prev_shown, s_shown);") < change.index("s_shown = uilayout::screen_for(s_model);") < change.index("s_screen_since = now;")
    assert app.count("s_prev_shown = ") == 2 and app.count("s_shown = ") == 2            # 선언 한 줄(둘 다 BOOT) + refresh 의 한 곳뿐 — 다른 데서 만지지 않는다
    # 터치: 빗나간 터치는 가드 앞에서 걸러지고, 통화 화면의 터치는 모두 이 한 줄의 가드를 지난다
    assert on_tap.index("if (t == Tap::NONE) return;") < on_tap.index("if (!s_guard.allow(now, s_screen_since, uilayout::screen_guard_for(t, s_prev_shown))) return;") < on_tap.index("switch (t) {")
    assert on_tap.count("s_guard.allow(") == 2                                             # Wi-Fi 설정 화면의 짧은 가드 + 이것
    hdr = UI_LAYOUT_H.read_text(encoding="utf-8")
    assert "const int64_t TAP_AFTER_BELL_GUARD_MS = 1500;" in hdr and "const int64_t TAP_SCREEN_GUARD_MS = 600;" in hdr
    assert "sleep:2" in pc.__doc__ and "sleep:2" in (Path(__file__).resolve().parent.parent / "README.md").read_text(encoding="utf-8")   # 자동 시험 쓰는 사람에게 알린다
