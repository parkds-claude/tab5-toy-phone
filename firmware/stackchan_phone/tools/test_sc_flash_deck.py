"""sc_flash_deck.py — 블록 나누기·REPL 문장·답 읽기·쓰는 순서·실패했을 때 남는 상태. 기기 없이 돈다(가짜 기기와 가짜 시리얼).

  /opt/homebrew/bin/python3 -m pytest -q -p no:cacheprovider tools/test_sc_flash_deck.py
"""
import base64
import hashlib
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sc_flash_deck as fd  # noqa: E402


# ── 순수한 부분 ──────────────────────────────────────────────────────────────

def test_blocks_pad_the_last_one_with_ff():
    image = bytes(range(256)) * 20 + b"\x01\x02\x03"            # 5123 B = 한 블록 + 1027 B
    bl = fd.blocks(image)
    assert [len(b) for b in bl] == [4096, 4096]
    assert b"".join(bl)[:len(image)] == image
    assert bl[1][1027:] == b"\xff" * (4096 - 1027)


def test_blocks_exact_multiple_adds_nothing():
    assert fd.blocks(b"\xe9" * 8192) == [b"\xe9" * 4096] * 2
    assert fd.blocks(b"") == []


def test_write_order_puts_the_image_head_last():
    assert fd.write_order(4) == [1, 2, 3, 0]
    assert fd.write_order(1) == [0]
    assert fd.write_order(0) == []


def test_block_steps_rebuild_the_block_and_write_once_at_the_end():
    block = bytes((i * 7) & 0xFF for i in range(4096))
    steps = fd.block_steps(5, block)
    assert len(steps) == 4096 // fd.CHUNK
    buf = bytearray(4096)
    for s in steps:
        m = re.match(r"b\[(\d+):(\d+)\]=a2b\('([A-Za-z0-9+/=]+)'\)", s)
        assert m, s
        buf[int(m.group(1)):int(m.group(2))] = base64.b64decode(m.group(3))
    assert bytes(buf) == block
    assert [s.count("p.writeblocks") for s in steps] == [0] * (len(steps) - 1) + [1]
    assert steps[-1].endswith(";p.writeblocks(5,b)")


def test_block_steps_reject_a_short_block():
    with pytest.raises(ValueError):
        fd.block_steps(0, b"\x00" * 100)


# ── 답 읽기 ──────────────────────────────────────────────────────────────────

def test_parse_reply_takes_the_output_and_refuses_errors():
    assert fd.parse_reply(b"OK12451840 3604480 factory\r\n\x04\x04>") == "12451840 3604480 factory"
    assert fd.parse_reply(b"OK\x04\x04>") == ""
    assert fd.parse_reply(b"OKa\x04b\r\n\x04\x04>") == "a\x04b"          # 출력에 섞인 \x04 는 출력으로 남는다
    with pytest.raises(RuntimeError, match="기기 오류"):
        fd.parse_reply(b"OK\x04Traceback (most recent call last):\r\nOSError: 5\r\n\x04>")
    with pytest.raises(RuntimeError, match="받지 않았다"):
        fd.parse_reply(b"raw REPL; CTRL-B to exit\r\n>OK1\r\n\x04\x04>")   # 앞에 다른 글자가 끼면 믿지 않는다


class FakeSerial:
    """쓰기를 받아 적고, Ctrl-D(\x04)로 끝나는 문장마다 미리 정한 답을 읽기 쪽에 넣는다."""

    def __init__(self, replies, noise=b""):
        self.written, self.replies, self.rx, self.closed = b"", list(replies), bytearray(noise), False

    def write(self, data):
        self.written += data
        if data.endswith(b"\x04") and self.replies:
            self.rx += self.replies.pop(0)

    @property
    def in_waiting(self):
        return len(self.rx)

    def read(self, n):
        out = bytes(self.rx[:n])
        del self.rx[:n]
        return out

    def close(self):
        self.closed = True


def test_repl_enter_drops_everything_before_the_sync_mark(monkeypatch):
    monkeypatch.setattr(fd, "_sleep", lambda s: None)
    ser = FakeSerial([b"OKSC-SYNC\r\n\x04\x04>", b"OK7\r\n\x04\x04>"], noise=b"KeyboardInterrupt\r\nraw REPL; CTRL-B to exit\r\n>")
    r = fd.Repl("x", ser=ser)
    r.enter()
    assert r.run("print(7)") == "7"
    assert ser.written == b"\x03\x03\x01print('SC-SYNC')\x04print(7)\x04"      # 빈 Ctrl-D(= 소프트 재시작)를 보내지 않는다


def test_repl_run_times_out_instead_of_hanging(monkeypatch):
    clock = iter(range(0, 1000))
    monkeypatch.setattr(fd, "_now", lambda: next(clock))
    r = fd.Repl("x", ser=FakeSerial([b"OK"]))                   # 끝 표시가 오지 않는다
    with pytest.raises(RuntimeError, match="답하지 않는다"):
        r.run("print(1)", timeout=5)


# ── 가짜 기기로 전체 흐름 ────────────────────────────────────────────────────

class FakeDevice:
    """sc_flash_deck 이 보내는 문장만 알아듣는 스택짱. 전화기 칸(ota_0)과 부팅 대상을 든다."""

    def __init__(self, addr=fd.APP_ADDR, size=fd.APP_MAX, running="factory", boot="ota_0", corrupt_block=None, die_at_block=None):
        self.addr, self.size, self.running, self.boot = addr, size, running, boot
        self.part = bytearray(b"\xe9" + b"\xaa" * 8191)         # 예전 앱(머리 0xE9)이 들어 있다
        self.buf = bytearray(4096)
        self.corrupt_block, self.die_at_block = corrupt_block, die_at_block
        self.log, self.resets, self.entered, self.closed = [], 0, False, False

    def enter(self):
        self.entered = True

    def close(self):
        self.closed = True

    def send(self, code):
        assert code == fd.RESET
        self.resets += 1
        self.log.append("reset")

    def _write(self, n):
        if self.die_at_block == n:
            raise OSError("device disconnected")
        data = bytes(self.buf)
        if self.corrupt_block == n:
            data = b"\x00" + data[1:]
        if len(self.part) < (n + 1) * 4096:
            self.part.extend(b"\xff" * ((n + 1) * 4096 - len(self.part)))
        self.part[n * 4096:(n + 1) * 4096] = data
        self.log.append(f"write{n}")

    def run(self, code, timeout=15.0):
        if code == fd.SETUP:
            return f"{self.addr} {self.size} {self.running}"
        if code == fd.PIN_DECK:
            self.boot = "factory"
            self.log.append("pin")
            return "boot=deck"
        if code == fd.WIPE_HEAD:
            self.part[0:4096] = b"\xff" * 4096
            self.log.append("wipe")
            return ""
        if code == fd.GO_PHONE:
            if self.part[0] != 0xE9:
                raise RuntimeError("기기 오류: OSError: ESP_ERR_OTA_VALIDATE_FAILED")
            self.boot = "ota_0"
            self.log.append("go")
            return "boot=ota_0"
        m = re.fullmatch(r"b\[(\d+):(\d+)\]=a2b\('([A-Za-z0-9+/=]+)'\)(?:;p\.writeblocks\((\d+),b\))?", code)
        if m:
            self.buf[int(m.group(1)):int(m.group(2))] = base64.b64decode(m.group(3))
            if m.group(4) is not None:
                self._write(int(m.group(4)))
            return ""
        m = re.match(r"h=uhashlib\.sha256\(\);n=(\d+);k=0\n", code)
        if m:
            self.log.append("hash")
            return hashlib.sha256(bytes(self.part[:int(m.group(1))])).hexdigest()
        raise AssertionError(f"모르는 문장: {code[:60]!r}")


IMAGE = b"\xe9" + bytes((i * 13 + 5) & 0xFF for i in range(4096 * 3 + 100))      # 4블록(마지막은 101 B)


def run_main(monkeypatch, tmp_path, dev, *args, kind="deck"):
    app = tmp_path / "app.bin"
    app.write_bytes(IMAGE)
    monkeypatch.setattr(fd.sc_ctl, "which", lambda: (kind, "/dev/fake"))
    return fd.main(["--app", str(app), *args], repl_factory=lambda port: dev)


def test_go_writes_head_last_checks_then_switches_and_restarts(monkeypatch, tmp_path):
    dev = FakeDevice()
    assert run_main(monkeypatch, tmp_path, dev, "--go") == 0
    assert dev.log == ["pin", "wipe", "write1", "write2", "write3", "write0", "hash", "go", "reset"]
    assert bytes(dev.part[:len(IMAGE)]) == IMAGE and dev.boot == "ota_0" and dev.resets == 1 and dev.closed


def test_without_go_it_stays_on_the_deck(monkeypatch, tmp_path):
    dev = FakeDevice(boot="ota_0")                              # 부팅 대상이 전화기로 남아 있던 기기라도
    assert run_main(monkeypatch, tmp_path, dev) == 0
    assert "go" not in dev.log and dev.boot == "factory" and dev.resets == 1


def test_mismatch_leaves_nothing_bootable_and_restarts_the_deck(monkeypatch, tmp_path, capsys):
    dev = FakeDevice(corrupt_block=2)
    assert run_main(monkeypatch, tmp_path, dev, "--go") == 1
    assert "기기에 쓰인 것이 다르다" in capsys.readouterr().err
    assert dev.log[-3:] == ["hash", "wipe", "reset"] and "go" not in dev.log
    assert dev.boot == "factory" and dev.part[0] == 0xFF and dev.resets == 1


def test_port_drop_mid_write_leaves_no_image_head_and_still_tries_to_restart(monkeypatch, tmp_path, capsys):
    dev = FakeDevice(die_at_block=2)
    assert run_main(monkeypatch, tmp_path, dev, "--go") == 1
    assert "중단" in capsys.readouterr().err
    assert dev.boot == "factory" and dev.part[0] == 0xFF          # 머리가 없으니 부팅할 것이 없다
    assert dev.resets == 1 and dev.closed


@pytest.mark.parametrize("kw, text", [({"size": 0x100000}, "전화기 칸이 기대와 다르다"), ({"addr": 0x10000}, "전화기 칸이 기대와 다르다"),
                                      ({"running": "ota_0"}, "스트림덱 칸이 아니다")])
def test_refuses_an_unexpected_partition_before_writing_and_restarts(monkeypatch, tmp_path, capsys, kw, text):
    dev = FakeDevice(**kw)
    before = bytes(dev.part)
    assert run_main(monkeypatch, tmp_path, dev, "--go") == 1
    assert text in capsys.readouterr().err
    assert bytes(dev.part) == before and dev.log == ["reset"]     # 아무것도 쓰지 않았고, 멈춘 스트림덱은 다시 켠다


def test_refuses_when_not_deck_without_touching_the_port(monkeypatch, tmp_path, capsys):
    for kind in ("phone", "none"):
        dev = FakeDevice()
        assert run_main(monkeypatch, tmp_path, dev, kind=kind) == 1
        assert "스트림덱이 돌 때만" in capsys.readouterr().err
        assert not dev.entered and dev.resets == 0


def test_refuses_a_file_that_is_not_an_app_image(monkeypatch, tmp_path):
    monkeypatch.setattr(fd.sc_ctl, "which", lambda: (_ for _ in ()).throw(AssertionError("기기를 보기 전에 거절해야 한다")))
    bad = tmp_path / "bad.bin"
    bad.write_bytes(b"\x00" * 64)
    assert fd.main(["--app", str(bad)]) == 1
    big = tmp_path / "big.bin"
    big.write_bytes(b"\xe9" + b"\x00" * fd.APP_MAX)
    assert fd.main(["--app", str(big)]) == 1
