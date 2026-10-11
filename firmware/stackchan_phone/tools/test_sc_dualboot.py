"""sc_dualboot.py 의 단위 시험(기기 없이). littlefs-python 이 필요하다.

  cd firmware/stackchan_phone && <venv>/bin/python -m pytest -q -p no:cacheprovider tools/test_sc_dualboot.py
"""
import hashlib
import struct
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sc_dualboot as sd  # noqa: E402

littlefs = pytest.importorskip("littlefs")


def entry(name, typ, sub, off, size, flags=0):
    return {"name": name, "type": typ, "sub": sub, "off": off, "size": size, "flags": flags}


UIFLOW = [entry("nvs", 1, 0x02, 0x9000, 0x6000), entry("phy_init", 1, 0x01, 0xF000, 0x1000), entry("factory", 0, 0x00, 0x10000, 0x990000),
          entry("sys", 1, 0x81, 0x9A0000, 0x100000), entry("vfs", 1, 0x81, 0xAA0000, 0x4C0000), entry("storage", 1, 0x82, 0xF60000, 0xA0000)]


def dual():
    return [dict(e) for e in UIFLOW[:4]] + [entry("vfs", 1, 0x81, 0xAA0000, 0x140000), entry("ota_0", 0, 0x10, 0xBE0000, 0x370000),
                                            entry("otadata", 1, 0x00, 0xF50000, 0x2000), entry("nvs_phone", 1, 0x02, 0xF52000, 0x6000), dict(UIFLOW[5])]


def pack(entries, md5=True):
    blob = b""
    for e in entries:
        blob += b"\xaa\x50" + bytes([e["type"], e["sub"]]) + struct.pack("<II", e["off"], e["size"]) + e["name"].encode().ljust(16, b"\0") + struct.pack("<I", e["flags"])
    if md5:
        blob += b"\xeb\xeb" + b"\xff" * 14 + hashlib.md5(blob).digest()
    return blob.ljust(0xC00, b"\xff")


# ── 파티션 표 읽기 ──────────────────────────────────────────────────────────────
def test_parse_ptable_roundtrip_and_md5():
    got, ok = sd.parse_ptable(pack(UIFLOW))
    assert got == UIFLOW and ok is True


def test_parse_ptable_detects_a_wrong_md5():
    blob = bytearray(pack(UIFLOW))
    blob[4] ^= 0x10                       # 첫 항목의 자리를 한 비트 바꾼다
    assert sd.parse_ptable(bytes(blob))[1] is False


def test_parse_ptable_without_md5_reports_none():
    assert sd.parse_ptable(pack(UIFLOW, md5=False))[1] is None


# ── 표 대조: UIFlow2 의 칸은 그대로여야 한다 ────────────────────────────────────
def test_the_designed_layout_is_accepted():
    assert sd.check_layout(UIFLOW, dual()) == []


@pytest.mark.parametrize("name", ["nvs", "phy_init", "factory", "sys", "storage"])
def test_moving_or_resizing_a_uiflow_partition_is_refused(name):
    n = dual()
    next(e for e in n if e["name"] == name)["size"] -= 0x1000
    assert any(name in b for b in sd.check_layout(UIFLOW, n))


def test_a_missing_uiflow_partition_is_refused():
    assert any("sys" in b for b in sd.check_layout(UIFLOW, [e for e in dual() if e["name"] != "sys"]))


def test_growing_vfs_is_refused():
    n = dual()
    next(e for e in n if e["name"] == "vfs")["size"] = 0x4D0000
    assert any("커졌다" in b for b in sd.check_layout(UIFLOW, n))


def test_a_new_partition_outside_the_freed_space_is_refused():
    n = dual()
    next(e for e in n if e["name"] == "ota_0")["off"] = 0xAB0000        # 줄어든 vfs 안으로 파고든다
    bad = sd.check_layout(UIFLOW, n)
    assert any("ota_0" in b and "밖" in b for b in bad) or any("겹친다" in b for b in bad)


def test_overlapping_partitions_are_refused():
    n = dual()
    next(e for e in n if e["name"] == "otadata")["off"] = 0xF4F000      # ota_0 의 끝과 겹친다
    assert any("겹친다" in b for b in sd.check_layout(UIFLOW, n))


def test_an_app_partition_off_the_64k_grid_is_refused():
    n = dual()
    o = next(e for e in n if e["name"] == "ota_0")
    o["off"] += 0x1000; o["size"] -= 0x1000
    assert any("64KB" in b for b in sd.check_layout(UIFLOW, n))


def test_an_undesigned_extra_partition_is_refused():
    assert any("설계에 없는" in b for b in sd.check_layout(UIFLOW, dual() + [entry("extra", 1, 0x40, 0xF58000, 0x1000)]))


def test_a_table_past_the_flash_end_is_refused():
    n = dual()
    assert any("플래시 크기" in b for b in sd.check_layout(UIFLOW, n, flash_size=0xF00000))


# ── vfs 다시 만들기 ─────────────────────────────────────────────────────────────
def make_fs(files, blocks):
    fs = littlefs.LittleFS(block_size=4096, block_count=blocks)
    for path, body in files.items():
        if "/" in path.strip("/"):
            fs.makedirs("/" + "/".join(path.strip("/").split("/")[:-1]), exist_ok=True)
        with fs.open(path, "wb") as f:
            f.write(body)
    return bytes(fs.context.buffer)


FILES = {"/main.py": b"print('deck')\n" * 900, "/boot.py": b"# boot\n", "/icons/a.png": bytes(range(256)) * 9, "/res/img/x.jpg": b"\xff\xd8" + b"j" * 5000,
         "/empty.txt": b""}


def test_shrink_keeps_every_file_byte_for_byte():
    old = make_fs(FILES, 256)
    new, files = sd.shrink_vfs(old, 96 * 4096)
    assert len(new) == 96 * 4096
    assert {p: b for p, b in sd.list_files(new) if b is not None} == FILES
    assert [p for p, b in files if b is None] == ["/icons/", "/res/", "/res/img/"]


def test_shrunk_image_reports_the_new_block_count():
    new, _ = sd.shrink_vfs(make_fs(FILES, 256), 96 * 4096)
    assert sd._mount(new).fs_stat().block_count == 96


def test_shrink_refuses_when_the_files_do_not_leave_room():
    old = make_fs({"/big.bin": b"x" * 300_000}, 256)
    with pytest.raises(ValueError, match="여유"):
        sd.shrink_vfs(old, 96 * 4096)


def test_shrink_refuses_sizes_off_the_4k_grid():
    with pytest.raises(ValueError, match="4KB"):
        sd.shrink_vfs(make_fs(FILES, 256), 96 * 4096 + 1)


def test_vfs_of_backup_cuts_the_region_named_in_the_table(tmp_path):
    vfs = make_fs(FILES, 64)
    table = [entry("nvs", 1, 2, 0x9000, 0x6000), entry("vfs", 1, 0x81, 0x10000, len(vfs))]
    backup = (b"\xe9" + b"\0" * (0x8000 - 1) + pack(table)).ljust(0x10000, b"\xff") + vfs
    assert sd.vfs_of_backup(backup) == vfs


def test_vfs_of_backup_refuses_a_file_without_a_table():
    with pytest.raises(ValueError, match="vfs 칸"):
        sd.vfs_of_backup(b"\xff" * 0x20000)


def test_cli_check_fails_on_a_table_without_md5(tmp_path, capsys):
    b = tmp_path / "backup.bin"; b.write_bytes(b"\0" * 0x8000 + pack(UIFLOW))
    p = tmp_path / "pt.bin"; p.write_bytes(pack(dual(), md5=False))
    assert sd.main(["check", str(b), str(p)]) == 1
    assert "MD5" in capsys.readouterr().out
