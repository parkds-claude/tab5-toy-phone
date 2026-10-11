"""sc_ctl.py 의 순수 부분 시험(포트를 열지 않는다).
    cd firmware/stackchan_phone/tools && /opt/homebrew/bin/python3 -m pytest -q -p no:cacheprovider test_sc_ctl.py test_sc_dualboot.py"""
import importlib
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
sc = importlib.import_module("sc_ctl")
pc = sc.phone_ctl


def port(serial, device="/dev/cu.x"):
    return SimpleNamespace(serial_number=serial, device=device)


def test_which_tells_the_phone_firmware_from_the_deck_by_the_usb_serial_shape():
    assert sc.which([port("AA:BB:CC:DD:EE:03", "/dev/cu.a")]) == ("phone", "/dev/cu.a")      # 칩 내장 USB: 전화기 펌웨어·다운로드 모드
    assert sc.which([port("aabbccddee030000", "/dev/cu.b")]) == ("deck", "/dev/cu.b")        # UIFlow2 의 USB
    assert sc.which([port("AA:BB:CC:DD:EE:02"), port(None)]) == ("none", None)               # Tab5 만 꽂혀 있다
    assert sc.which([port("AA:BB:CC:DD:EE:04")]) == ("none", None)                           # 스택짱 1호기는 우리 기기가 아니다
    assert sc.which([]) == ("none", None)


def test_find_port_refuses_the_deck_and_keeps_the_tab5_lookup():
    assert sc.find_port("3", [port("AA:BB:CC:DD:EE:03", "/dev/cu.a")]) == "/dev/cu.a"
    with pytest.raises(SystemExit) as e:
        sc.find_port("3", [port("aabbccddee030000")])
    assert "스트림덱" in str(e.value)                                                         # UIFlow2 의 REPL 에 명령을 보내지 않는다
    with pytest.raises(SystemExit):
        sc.find_port("3", [])
    assert sc.find_port("2", [port("AA:BB:CC:DD:EE:02", "/dev/cu.t")]) == "/dev/cu.t"        # Tab5 는 원래 방식 그대로


def test_units_table_and_step_prefixes_accept_unit_3_after_main_sets_it():
    pc.UNITS = {"2": sc.TAB5["2"], sc.SC_UNIT: sc.SC_MAC}
    plan = pc.plan_steps(["3:tap:238:110", "2:wait:^FSM IDLE -> INCOMING:10", "both:until:stat:^STAT :5", "sleep:1"], pair=True)
    assert [(u, k) for u, k, _ in plan] == [(("3",), "tap"), (("2",), "wait"), (("2", "3"), "until"), (("2", "3"), "sleep")]
    with pytest.raises(ValueError):
        pc.plan_steps(["1:stat"], pair=True)                                                 # 표에 없는 1번은 접두사가 아니다 → 기기 번호가 필요하다는 오류


def test_rgb565_to_png_converts_colours_and_rejects_a_wrong_size(tmp_path):
    from PIL import Image
    raw = bytearray(sc.W * sc.H * 2)
    raw[0:2] = (0xF800).to_bytes(2, "little")            # (0,0) 빨강
    raw[2:4] = (0x07E0).to_bytes(2, "little")            # (1,0) 초록
    raw[-2:] = (0x001F).to_bytes(2, "little")            # 마지막 픽셀 파랑
    out = tmp_path / "s.png"
    sc.rgb565_to_png(bytes(raw), out)
    img = Image.open(out)
    assert img.size == (320, 240)
    assert img.getpixel((0, 0)) == (255, 0, 0) and img.getpixel((1, 0)) == (0, 255, 0) and img.getpixel((319, 239)) == (0, 0, 255) and img.getpixel((5, 5)) == (0, 0, 0)
    with pytest.raises(ValueError):
        sc.rgb565_to_png(b"\0" * 100, out)


def test_shot_writes_a_png_for_the_stackchan_and_leaves_tab5_shots_alone(tmp_path):
    notes = []

    class FakeHub:
        def note(self, *a):
            notes.append(a)
    raw = bytes(sc.W * sc.H * 2)
    orig = sc._tab5_shot
    try:
        sc._tab5_shot = lambda self, unit, path: (Path(path).write_bytes(raw if unit == "3" else b"\xff\xd8jpeg"), True)[1]
        p3, p2 = tmp_path / "a.png", tmp_path / "b.jpg"
        assert sc._shot(FakeHub(), "3", str(p3)) and p3.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n"
        assert sc._shot(FakeHub(), "2", str(p2)) and p2.read_bytes() == b"\xff\xd8jpeg"       # Tab5 의 JPEG 은 그대로
        sc._tab5_shot = lambda self, unit, path: False
        assert sc._shot(FakeHub(), "3", str(p3)) is False                                    # 받기가 실패하면 그대로 실패
        sc._tab5_shot = lambda self, unit, path: (Path(path).write_bytes(b"short"), True)[1]
        assert sc._shot(FakeHub(), "3", str(p3)) is False and any("변환 실패" in str(n) for n in notes)
    finally:
        sc._tab5_shot = orig


def test_main_rejects_unknown_commands_and_bad_with(capsys):
    assert sc.main(["frobnicate"]) == 2
    assert sc.main(["pair", "--with", "9", "3:stat"]) == 2
    assert sc.main(["--help"]) == 0
