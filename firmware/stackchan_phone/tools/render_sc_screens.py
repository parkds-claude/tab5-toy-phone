#!/usr/bin/env python3
"""render_sc_screens.py — 스택짱 전화기 화면 미리보기. 기기에 올리지 않고 맥에서 모든 장면을 그려 본다.

기기와 같은 그리기 코드(main/sc_ui.cpp + Tab5 의 raster.h)를 맥용으로 컴파일해 장면마다 PNG 를 만든다 → 기기 화면과 픽셀까지 같다.
"조각만 다시 그리기"(시계·음량)는 통째로 그린 것과 한 픽셀도 다르지 않아야 한다 — sc_sim 이 그 대조를 하고 다르면 실패한다.

    python3 tools/render_sc_screens.py [--out out/screens] [--scale 2]
출력: <out>/<장면>.png (원래 크기 320×240), <out>/sheet.png (전 장면을 늘어놓은 한 장 — scale 배)
필요: clang++, Pillow. ESP-IDF 를 export 하지 않은 셸에서.
"""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
TAB5_MAIN = ROOT.parent / "tab5_phone" / "main"
W, H = 320, 240

def build(tmp: Path) -> Path:
    exe = tmp / "sc_sim"
    sdk = subprocess.run(["xcrun", "--sdk", "macosx", "--show-sdk-path"], capture_output=True, text=True).stdout.strip()
    cmd = ["clang++", "-std=c++17", "-O1", "-Wall", "-Wextra", "-Werror", "-isysroot", sdk, "-I", str(ROOT / "main"), "-I", str(TAB5_MAIN),
           str(HERE / "sim" / "sc_sim.cpp"), str(ROOT / "main" / "sc_ui.cpp"), "-o", str(exe)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit("빌드 실패")
    return exe


def to_image(raw: bytes) -> Image.Image:
    if len(raw) != W * H * 2:
        raise SystemExit(f"크기가 다르다: {len(raw)}")
    img = Image.new("RGB", (W, H))
    px = img.load()
    for i in range(W * H):
        v = raw[2 * i] | (raw[2 * i + 1] << 8)
        px[i % W, i // W] = ((v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)
    return img


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", type=Path, default=ROOT / "out" / "screens")
    ap.add_argument("--scale", type=int, default=2)
    a = ap.parse_args(argv)
    a.out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        exe = build(tmp)
        r = subprocess.run([str(exe), str(tmp)], capture_output=True, text=True)
        if r.returncode != 0:
            sys.stderr.write(r.stdout + r.stderr)
            return 1
        names = r.stdout.split()
        raws = {n: (tmp / f"{n}.rgb565").read_bytes() for n in names}
    images = {n: to_image(raw) for n, raw in raws.items()}
    for n, img in images.items():
        img.save(a.out / f"{n}.png")
    cols = 4
    rows = (len(names) + cols - 1) // cols
    s, gap = a.scale, 12
    sheet = Image.new("RGB", (cols * (W * s + gap) + gap, rows * (H * s + gap) + gap), (40, 40, 48))
    for i, n in enumerate(names):
        sheet.paste(images[n].resize((W * s, H * s), Image.NEAREST), (gap + (i % cols) * (W * s + gap), gap + (i // cols) * (H * s + gap)))
    sheet.save(a.out / "sheet.png")
    print(f"[render_sc_screens] 장면 {len(names)}개 → {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
