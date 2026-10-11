#!/usr/bin/env python3
"""sc_dualboot.py — 스택짱 2호기에 UIFlow2(스트림덱)와 전화기 펌웨어를 함께 두기 위한 이미지 도구.
설계서 docs/exec-plans/2026-10-08-phone-3way-design.md §11 (TW-D14). 기기에는 손대지 않는다 — 파일만 읽고 만든다(기기 쪽은 tools/sc_install.sh).

  python3 sc_dualboot.py check <통째 백업.bin> <새 partition-table.bin>
      새 파티션 표가 UIFlow2 의 표와 "바꿔도 되는 것만" 다른지 본다: nvs·phy_init·factory·sys·storage 는 자리·크기·종류가 같아야 하고,
      vfs 는 같은 자리에서 줄어들기만 하고, 새 칸(ota_0·otadata·nvs_phone)은 줄어든 vfs 가 내준 자리 안에 있어야 한다.
  python3 sc_dualboot.py vfs <옛 vfs 구역.bin> <새 vfs 이미지.bin> [--size 0x140000]
      옛 vfs(littlefs) 안의 파일을 전부 꺼내 새 크기의 littlefs 이미지를 만들고, 다시 열어 파일마다 SHA256 이 같은지 확인한다.
      옛 구역이 통째 백업이면 --from-backup 을 붙인다(표에서 vfs 자리를 찾아 잘라 쓴다).
  python3 sc_dualboot.py ls <vfs 이미지.bin | 통째 백업.bin --from-backup>
      안의 파일 목록(경로·크기·SHA256 앞 10자).

왜 "줄이기"가 아니라 "다시 만들기"인가: littlefs 는 쓰인 블록이 칸 전체에 흩어져 있고(백업 실측: 1216 블록 중 174개가 1178번까지) 초블록에 블록 수를 적어 둔다
  → 표의 크기만 줄이면 마운트가 실패한다. 파일을 꺼내 새 블록 수로 다시 만든다.
필요: littlefs-python (`pip install littlefs-python` — tools/sc_install.sh 가 out/venv 에 설치한다).
"""
import argparse
import hashlib
import struct
import sys
from pathlib import Path

PT_OFFSET, PT_MAX = 0x8000, 0xC00
BLOCK = 4096
FLASH_SIZE = 0x1000000
KEEP = ("nvs", "phy_init", "factory", "sys", "storage")     # UIFlow2 가 쓰는 그대로여야 하는 칸
NEW = ("ota_0", "otadata", "nvs_phone")                      # 전화기 쪽 새 칸


def parse_ptable(blob):
    """파티션 표(0xC00 바이트 이하) → (항목 목록, md5 가 맞는가 | None). 항목 = dict(name, type, sub, off, size, flags)."""
    out, md5_ok = [], None
    for i in range(0, min(len(blob), PT_MAX), 32):
        e = blob[i:i + 32]
        if len(e) < 32:
            break
        if e[:2] == b"\xaa\x50":
            off, size = struct.unpack("<II", e[4:12])
            out.append({"name": e[12:28].split(b"\0")[0].decode("ascii", "replace"), "type": e[2], "sub": e[3],
                        "off": off, "size": size, "flags": struct.unpack("<I", e[28:32])[0]})
        elif e[:2] == b"\xeb\xeb":
            md5_ok = e[16:32] == hashlib.md5(blob[:i]).digest()
            break
        else:
            break
    return out, md5_ok


def check_layout(old, new, flash_size=FLASH_SIZE):
    """옛 표(UIFlow2)와 새 표를 견줘 문제 목록을 돌려준다(빈 목록 = 괜찮다)."""
    bad = []
    o, n = {e["name"]: e for e in old}, {e["name"]: e for e in new}
    if len(n) != len(new):
        bad.append("새 표에 같은 이름의 칸이 둘 이상 있다")
    for name in KEEP:
        if name not in o:
            bad.append(f"옛 표에 {name} 칸이 없다(UIFlow2 의 표가 맞는가)")
        elif name not in n:
            bad.append(f"새 표에 {name} 칸이 없다")
        elif any(o[name][k] != n[name][k] for k in ("type", "sub", "off", "size", "flags")):
            bad.append(f"{name} 칸이 달라졌다(자리·크기·종류가 UIFlow2 와 같아야 한다)")
    if "vfs" not in o or "vfs" not in n:
        bad.append("vfs 칸이 옛 표나 새 표에 없다")
        return bad
    ov, nv = o["vfs"], n["vfs"]
    if (nv["off"], nv["type"], nv["sub"]) != (ov["off"], ov["type"], ov["sub"]):
        bad.append("vfs 칸의 시작 자리나 종류가 달라졌다")
    if nv["size"] > ov["size"]:
        bad.append("vfs 칸이 커졌다(줄어들기만 해야 한다)")
    if nv["size"] % BLOCK or nv["size"] < 64 * BLOCK:
        bad.append("vfs 칸의 크기가 4KB 의 배수가 아니거나 너무 작다")
    lo, hi = nv["off"] + nv["size"], ov["off"] + ov["size"]          # 줄어든 vfs 가 내준 자리
    for name in NEW:
        if name not in n:
            bad.append(f"새 표에 {name} 칸이 없다")
            continue
        e = n[name]
        if e["off"] < lo or e["off"] + e["size"] > hi:
            bad.append(f"{name} 칸이 vfs 가 내준 자리(0x{lo:x}~0x{hi:x}) 밖에 있다")
    if "ota_0" in n:
        e = n["ota_0"]
        if (e["type"], e["sub"]) != (0, 0x10) or e["off"] % 0x10000:
            bad.append("ota_0 칸이 앱 칸(type 0, sub 0x10)이 아니거나 64KB 에 맞지 않는다")
    if "otadata" in n and ((n["otadata"]["type"], n["otadata"]["sub"]) != (1, 0x00) or n["otadata"]["size"] != 0x2000):
        bad.append("otadata 칸이 data/ota 가 아니거나 크기가 0x2000 이 아니다")
    for name in n:
        if name not in o and name not in NEW:
            bad.append(f"설계에 없는 새 칸: {name}")
    spans = sorted((e["off"], e["off"] + e["size"], e["name"]) for e in new)
    for (a0, a1, an), (b0, b1, bn) in zip(spans, spans[1:]):
        if b0 < a1:
            bad.append(f"{an} 과 {bn} 칸이 겹친다")
    if spans and spans[-1][1] > flash_size:
        bad.append("표가 플래시 크기를 넘는다")
    return bad


def _mount(img):
    from littlefs import LittleFS
    fs = LittleFS(block_size=BLOCK, block_count=len(img) // BLOCK, mount=False)
    fs.context.buffer = bytearray(img)
    fs.mount()
    return fs


def list_files(img):
    """littlefs 이미지 → [(경로, 내용 바이트)] (폴더는 경로가 '/' 로 끝나고 내용은 None). 경로 순."""
    fs, out = _mount(img), []

    def walk(path):
        for name in sorted(fs.listdir(path)):
            p = path.rstrip("/") + "/" + name
            if fs.stat(p).type == 2:
                out.append((p + "/", None))
                walk(p)
            else:
                with fs.open(p, "rb") as f:
                    out.append((p, f.read()))
    walk("/")
    return out


def shrink_vfs(old_img, new_size):
    """옛 vfs 이미지의 파일을 전부 옮긴 new_size 바이트짜리 littlefs 이미지를 만든다. 옮긴 뒤 다시 열어 파일마다 같은지 확인한다."""
    from littlefs import LittleFS
    if len(old_img) % BLOCK or new_size % BLOCK:
        raise ValueError("이미지 크기는 4KB 의 배수여야 한다")
    st = _mount(old_img).fs_stat()
    files = list_files(old_img)
    need = sum(len(b) for _, b in files if b is not None)
    if need > new_size * 6 // 10:
        raise ValueError(f"파일 합계 {need} B 가 새 크기 {new_size} B 의 60% 를 넘는다 — 여유가 모자란다(vfs 를 덜 줄이거나 파일을 지운다)")
    fs = LittleFS(block_size=BLOCK, block_count=new_size // BLOCK, disk_version=st.disk_version, name_max=st.name_max)
    for path, body in files:
        if body is None:
            fs.makedirs(path.rstrip("/"), exist_ok=True)
        else:
            with fs.open(path, "wb") as f:
                f.write(body)
    new_img = bytes(fs.context.buffer)
    if len(new_img) != new_size:
        raise ValueError("만든 이미지의 크기가 다르다")
    back = list_files(new_img)
    if [(p, None if b is None else hashlib.sha256(b).digest()) for p, b in back] != [(p, None if b is None else hashlib.sha256(b).digest()) for p, b in files]:
        raise ValueError("새 이미지를 다시 열어 보니 파일이 옛것과 다르다")
    if _mount(new_img).fs_stat().block_count != new_size // BLOCK:
        raise ValueError("새 이미지의 블록 수가 새 크기와 다르다")
    return new_img, files


def vfs_of_backup(backup):
    """통째 백업에서 vfs 구역을 잘라 낸다(표에서 자리를 찾는다)."""
    entries, md5_ok = parse_ptable(backup[PT_OFFSET:PT_OFFSET + PT_MAX])
    v = next((e for e in entries if e["name"] == "vfs"), None)
    if md5_ok is False or v is None or v["off"] + v["size"] > len(backup):
        raise ValueError("백업에서 vfs 칸을 찾지 못했다(파티션 표가 깨졌거나 통째 백업이 아니다)")
    return backup[v["off"]:v["off"] + v["size"]]


def _print_files(files):
    for path, body in files:
        print(f"  {path:44s} {'' if body is None else len(body):>8} {'' if body is None else hashlib.sha256(body).hexdigest()[:10]}")
    print(f"  파일 {sum(1 for _, b in files if b is not None)}개, 합계 {sum(len(b) for _, b in files if b is not None)} B")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("check"); c.add_argument("backup"); c.add_argument("ptable")
    v = sub.add_parser("vfs"); v.add_argument("src"); v.add_argument("dst"); v.add_argument("--size", type=lambda s: int(s, 0), default=0x140000); v.add_argument("--from-backup", action="store_true")
    l = sub.add_parser("ls"); l.add_argument("src"); l.add_argument("--from-backup", action="store_true")
    a = ap.parse_args(argv)
    try:
        if a.cmd == "check":
            old, old_md5 = parse_ptable(Path(a.backup).read_bytes()[PT_OFFSET:PT_OFFSET + PT_MAX])
            new, new_md5 = parse_ptable(Path(a.ptable).read_bytes())
            bad = check_layout(old, new)
            if old_md5 is False:
                bad.append("백업의 파티션 표 MD5 가 맞지 않는다")
            if new_md5 is not True:
                bad.append("새 파티션 표에 MD5 가 없거나 맞지 않는다(부트로더가 거부한다)")
            for e in new:
                print(f"  {e['name']:10s} type={e['type']} sub=0x{e['sub']:02x} 0x{e['off']:06x} +0x{e['size']:06x}")
            if bad:
                print("문제:\n  - " + "\n  - ".join(bad))
                return 1
            print("표 괜찮음: UIFlow2 의 칸은 그대로이고 새 칸은 vfs 가 내준 자리 안에 있다")
            return 0
        raw = Path(a.src).read_bytes()
        img = vfs_of_backup(raw) if a.from_backup else raw
        if a.cmd == "ls":
            _print_files(list_files(img))
            return 0
        new_img, files = shrink_vfs(img, a.size)
        Path(a.dst).write_bytes(new_img)
        _print_files(files)
        print(f"만듦: {a.dst} ({len(new_img)} B = {len(new_img) // BLOCK} 블록) — 다시 열어 파일 {sum(1 for _, b in files if b is not None)}개가 옛것과 같음을 확인")
        return 0
    except (ValueError, OSError) as e:
        print(f"실패: {e}")
        return 2


if __name__ == "__main__":
    sys.exit(main())
