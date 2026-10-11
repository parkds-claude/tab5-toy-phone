#!/usr/bin/env python3
"""render_screens.py — 기기 없이 화면을 그림으로 만든다(화면 미리보기).

기기와 같은 그리기 코드(main/gfx_draw.cpp · toy.cpp · ui.cpp · ui_wifi.cpp, 픽셀 합성은 main/raster.h)를 맥에서 컴파일해
tools/sim/sim_main.cpp 의 장면들을 그리고 PNG 로 바꾼다. 도형·구운 글자·캐릭터는 기기와 픽셀까지 같다.
다른 것은 하나: 내장 글꼴로 쓰는 "바뀌는 글자"(Wi-Fi 이름·자판 글쇠·쪽 번호)는 맥의 비슷한 글꼴로 얹는다(자리·폭은 맞고 모양만 다르다).

    python3 tools/render_screens.py                 # out/screens/*.png + _sheet.png(전부를 한 장에) + _sheet_3way.png(전화기 3대: 대기 9조합·전화 옴 3·통화 중 안내)
    python3 tools/render_screens.py --out /tmp/s    # 다른 폴더로
쓰임: 화면을 고친 뒤 기기에 올리기 전에 본다 — 캐릭터·버튼이 겹치지 않는지, 글자가 칸을 넘지 않는지.
      비밀번호가 든 자판 화면(비공개 프레임)은 그림 위쪽 가운데에 "PRIVATE" 딱지를 얹어 기기에서는 찍히지 않는 화면임을 알린다.
ESP-IDF 를 export 하지 않은 셸에서 실행한다(Pillow 필요). out/ 은 git 에 올리지 않는다.
"""
import argparse
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
MAIN, SIM = ROOT / "main", ROOT / "tools" / "sim"
SOURCES = [MAIN / "gfx_draw.cpp", MAIN / "toy.cpp", MAIN / "ui.cpp", MAIN / "ui_wifi.cpp", SIM / "sim_gfx.cpp", SIM / "sim_main.cpp"]
W, H = 1280, 720
# 전화기 3대를 한눈에 보는 장(_sheet_3way.png)에 넣을 장면 — 이름의 앞머리로 고른다(장면은 tools/sim/sim_main.cpp):
#   대기 = 내 번호 1·2·3 × 상대 켜짐/꺼짐(둘 다 켜짐 · 하나만 · 둘 다 꺼짐), 전화 옴 = 건 쪽이 곰·토끼·로봇, 안내 = "통화 중이에요"
THREE_WAY = ("10_", "11_", "12_", "13_", "14_", "15_", "16_", "17_", "18_", "40_", "41_", "42_", "20_")
TEXT_FONTS = ["/System/Library/Fonts/AppleSDGothicNeo.ttc", "/System/Library/Fonts/Supplemental/AppleGothic.ttf"]


def build(exe: Path) -> None:
    # --sdk macosx: 맥북의 기본 SDK(CommandLineTools)는 링크가 깨진다("unknown architecture arm64e.x1") — 스택짱 쪽 render_sc_screens.py 와 같은 모양
    sdk = subprocess.run(["xcrun", "--sdk", "macosx", "--show-sdk-path"], capture_output=True, text=True, check=True).stdout.strip()
    cmd = ["clang++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-isysroot", sdk, "-I", str(SIM / "stub"), "-I", str(SIM), "-I", str(MAIN),
           *map(str, SOURCES), "-o", str(exe)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.stderr.strip():
        print(r.stderr, file=sys.stderr)                 # 경고도 그대로 보여 준다(경고 0 이 기준)
    if r.returncode != 0:
        raise SystemExit("화면 미리보기 빌드 실패")


def text_font(px: int) -> ImageFont.FreeTypeFont:
    for path in TEXT_FONTS:
        if Path(path).is_file():
            try:
                return ImageFont.truetype(path, px, index=6 if path.endswith(".ttc") else 0)   # 6 = Bold
            except OSError:
                return ImageFont.truetype(path, px)
    return ImageFont.load_default()


def to_png(raw: Path) -> Image.Image:
    data = raw.read_bytes()
    if len(data) != W * H * 2:
        raise SystemExit(f"{raw.name}: 크기가 {W}x{H} RGB565 가 아닙니다")
    img = Image.frombytes("RGB", (W, H), data, "raw", "BGR;16")
    draw = ImageDraw.Draw(img)
    private = False
    notes = raw.with_suffix(".txt")
    if notes.is_file():
        for line in notes.read_text(encoding="utf-8").splitlines():
            if line == "private":
                private = True
                continue
            kind, x, y, size, rgb, s = line.split("\t", 5)
            font = text_font(round(16 * float(size)))
            draw.text((int(x), int(y)), s, font=font, fill="#" + rgb, anchor="mm" if kind == "center" else "lm")
    if private:
        draw.rectangle((W // 2 - 190, 0, W // 2 + 190, 20), fill="#2A1F3D")     # 가운데 위의 작은 딱지(이 화면의 그 자리는 비어 있다)
        draw.text((W // 2, 10), "PRIVATE - 기기에서는 스크린샷이 거절되는 화면", font=text_font(14), fill="#FFFFFF", anchor="mm")
    return img


def sheet(images, path: Path, cols: int = 3) -> None:
    tw, th, pad, cap = W // 3, H // 3, 12, 22
    rows = (len(images) + cols - 1) // cols
    out = Image.new("RGB", (pad + cols * (tw + pad), pad + rows * (th + cap + pad)), "#FFFFFF")
    draw = ImageDraw.Draw(out)
    font = text_font(15)
    for i, (name, img) in enumerate(images):
        x, y = pad + (i % cols) * (tw + pad), pad + (i // cols) * (th + cap + pad)
        out.paste(img.resize((tw, th), Image.LANCZOS), (x, y + cap))
        draw.text((x, y + cap // 2), name, font=font, fill="#2A1F3D", anchor="lm")
    out.save(path)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="기기 없이 화면을 그림으로(화면 미리보기)")
    ap.add_argument("--out", type=Path, default=ROOT / "out" / "screens", help="출력 폴더 (기본: out/screens)")
    args = ap.parse_args(argv)
    args.out.mkdir(parents=True, exist_ok=True)
    exe = args.out / "_sim"
    build(exe)
    for old in list(args.out.glob("*.rgb565")) + list(args.out.glob("*.txt")):
        old.unlink()
    for old in args.out.glob("[0-9][0-9]_*.png"):        # 앞 실행이 만든 장면 그림 — 장면 이름이 바뀌면 옛 이름의 그림이 남아 지금 화면처럼 보인다
        old.unlink()
    r = subprocess.run([str(exe), str(args.out)], capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout + r.stderr, file=sys.stderr)
        return 1
    images = []
    for raw in sorted(args.out.glob("*.rgb565")):
        img = to_png(raw)
        img.save(raw.with_suffix(".png"))
        images.append((raw.stem, img))
        raw.unlink()
        raw.with_suffix(".txt").unlink(missing_ok=True)
    exe.unlink(missing_ok=True)
    sheet(images, args.out / "_sheet.png")
    by_prefix = {name[:3]: (name, img) for name, img in images}
    three = [by_prefix[p] for p in THREE_WAY if p in by_prefix]
    if len(three) != len(THREE_WAY):
        print(f"[render_screens] 전화기 3대 장면이 모자랍니다({len(three)}/{len(THREE_WAY)}) — sim_main.cpp 의 장면 이름을 확인하세요", file=sys.stderr)
        return 1
    sheet(three, args.out / "_sheet_3way.png", cols=3)
    print(f"[render_screens] {len(images)} screens → {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
