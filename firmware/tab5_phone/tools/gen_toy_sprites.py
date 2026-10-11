#!/usr/bin/env python3
"""토이폰 테마의 그림 자산 생성기 — tools/toy_art.py 의 SVG 조각을 색 그림으로 구워 C 헤더로.

왜: 캐릭터는 색이 여럿이라 글자 자산(한 색 알파 마스크, gen_ui_assets.py)으로는 못 그린다. 맥에서 한 번 그려 줄 단위로 눌러 담고(RLE),
    기기는 합성 버퍼에 줄을 풀어 얹기만 한다(raster::blit_rle). 평평한 색이 대부분이라 그냥 담는 것의 1/5~1/10 크기다.
입력: tools/toy_art.py 의 SPRITES (이름, SVG 조각, viewBox, 배율, 대칭축, 바닥) 와 HILL_PATH
출력: main/ui_sprites.h — ui_sprite_t spr_* 와 동산 윗선 표 HILL_TOP_Q4[1280]
      줄 하나 = 묶음(run)의 나열, 묶음 하나 = 16비트 둘: [길이(8) << 8 | 알파(8)], [RGB565]. 줄 끝의 투명 묶음은 뺀다.

실행(ESP-IDF 를 export 하지 않은 셸에서):
    python3 tools/gen_toy_sprites.py                      # main/ui_sprites.h
    python3 tools/gen_toy_sprites.py --preview /tmp/sprites.png --sizes
필요한 것: rsvg-convert (brew install librsvg), Pillow. 그림을 바꿀 때만 필요하다 — 구운 헤더는 git 에 있어 빌드에는 필요 없다.
"""
import argparse
import io
import shutil
import subprocess
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import toy_art  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUT = ROOT / "main" / "ui_sprites.h"
SCREEN_W = 1280
MAX_RUN = 255


class SpriteError(Exception):
    pass


def rasterize(inner: str, viewbox, scale: float) -> Image.Image:
    """SVG 조각을 배율대로 그린 RGBA 그림."""
    exe = shutil.which("rsvg-convert")
    if not exe:
        raise SpriteError("rsvg-convert 가 없습니다 → brew install librsvg")
    x, y, w, h = viewbox
    out_w, out_h = round(w * scale), round(h * scale)
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{out_w}" height="{out_h}" viewBox="{x} {y} {w} {h}">{inner}</svg>')
    r = subprocess.run([exe, "-f", "png"], input=svg.encode("utf-8"), capture_output=True)
    if r.returncode != 0:
        raise SpriteError(f"rsvg-convert 실패: {r.stderr.decode('utf-8', 'replace')[:300]}")
    img = Image.open(io.BytesIO(r.stdout)).convert("RGBA")
    if img.size != (out_w, out_h):
        raise SpriteError(f"그려진 크기 {img.size} 가 기대 {(out_w, out_h)} 와 다릅니다")
    return img


def rgb565(r: int, g: int, b: int) -> int:
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def encode(img: Image.Image):
    """RGBA → (줄 시작 표, 묶음 목록). 묶음 = (길이, 알파, 565)."""
    w, h = img.size
    px = img.load()
    rows, runs = [0], []
    for yy in range(h):
        line = []
        for xx in range(w):
            r, g, b, a = px[xx, yy]
            key = (0, 0) if a == 0 else (a, rgb565(r, g, b))
            if line and line[-1][1] == key and line[-1][0] < MAX_RUN:
                line[-1][0] += 1
            else:
                line.append([1, key])
        while line and line[-1][1][0] == 0:               # 줄 끝의 투명 묶음은 뺀다
            line.pop()
        runs.extend((n, a, c) for n, (a, c) in line)
        rows.append(len(runs))
    return rows, runs


def decode(rows, runs, w, h) -> Image.Image:
    """검수용: 구운 것을 다시 풀어 RGBA 로(기기의 raster::blit_rle 과 같은 규칙)."""
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    px = img.load()
    for yy in range(h):
        xx = 0
        for n, a, c in runs[rows[yy]:rows[yy + 1]]:
            if a:
                rgb = ((c >> 11) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31)
                for k in range(n):
                    px[xx + k, yy] = rgb + (a,)
            xx += n
        if xx > w:
            raise SpriteError("줄이 폭을 넘었습니다")
    return img


def check_roundtrip(name: str, src: Image.Image, dec: Image.Image) -> None:
    """구운 것을 다시 풀면 원본과 같아야 한다(색은 RGB565 로 줄인 값, 투명 픽셀의 색은 보지 않는다)."""
    a, b = src.load(), dec.load()
    for yy in range(src.size[1]):
        for xx in range(src.size[0]):
            r, g, bl, al = a[xx, yy]
            r2, g2, b2, al2 = b[xx, yy]
            if al != al2 or (al and rgb565(r, g, bl) != rgb565(r2, g2, b2)):
                raise SpriteError(f"{name}: 굽고 다시 푼 것이 ({xx},{yy})에서 원본과 다릅니다")


def bezier_y_table(path, width: int):
    """3차 곡선 조각들을 x = 0..width-1 의 y 표로(조각마다 x 가 한 방향으로 늘어난다고 본다)."""
    pts = []
    for p0, p1, p2, p3 in path:
        for i in range(4001):
            t = i / 4000
            mt = 1 - t
            x = mt ** 3 * p0[0] + 3 * mt * mt * t * p1[0] + 3 * mt * t * t * p2[0] + t ** 3 * p3[0]
            y = mt ** 3 * p0[1] + 3 * mt * mt * t * p1[1] + 3 * mt * t * t * p2[1] + t ** 3 * p3[1]
            pts.append((x, y))
    table, j = [], 0
    for xx in range(width):
        cx = xx + 0.5
        while j + 1 < len(pts) and pts[j + 1][0] < cx:
            j += 1
        (x0, y0), (x1, y1) = pts[j], pts[min(j + 1, len(pts) - 1)]
        y = y0 if x1 == x0 else y0 + (y1 - y0) * (cx - x0) / (x1 - x0)
        table.append(round(y * 16))
    if any(b[0] < a[0] for a, b in zip(pts, pts[1:])):
        raise SpriteError("동산 곡선의 x 가 한 방향으로 늘지 않습니다(열마다 높이 하나를 정할 수 없다)")
    if not all(0 <= v < 65536 for v in table):
        raise SpriteError("동산 높이가 16비트 범위를 벗어났습니다")
    return table


def build():
    names = [s[0] for s in toy_art.SPRITES]
    dup = sorted({n for n in names if names.count(n) > 1})
    if dup:
        raise SpriteError(f"이름이 겹칩니다: {dup}")
    out = ["// 자동 생성: tools/gen_toy_sprites.py — 수정 금지. 그림 원본은 tools/toy_art.py (토이폰 테마, docs/design/README.md §5).",
           "// 줄 하나 = 묶음의 나열, 묶음 = 16비트 둘: [길이 << 8 | 알파], [RGB565]. row[y]..row[y+1] 이 y 줄의 묶음(단위: 묶음 개수).",
           "#pragma once", '#include "sprite.h"', ""]
    made, total_px, total_runs = [], 0, 0
    for name, inner, vb, scale, axis, base in toy_art.SPRITES:
        img = rasterize(inner, vb, scale)
        w, h = img.size
        rows, runs = encode(img)
        check_roundtrip(name, img, decode(rows, runs, w, h))
        ax, by = round((axis - vb[0]) * scale), round((base - vb[1]) * scale)
        words = []
        for n, a, c in runs:
            words += [(n << 8) | a, c]
        out.append(f"// {name}: {w}x{h}, 대칭축 x={ax}, 바닥 y={by}, 묶음 {len(runs)}")
        out.append(f"static const uint16_t {name}_runs[{len(words)}] = {{")
        for i in range(0, len(words), 20):
            out.append("  " + ",".join(str(v) for v in words[i:i + 20]) + ",")
        out.append("};")
        out.append(f"static const uint32_t {name}_row[{len(rows)}] = {{")
        for i in range(0, len(rows), 20):
            out.append("  " + ",".join(str(v) for v in rows[i:i + 20]) + ",")
        out.append("};")
        out.append(f"static const ui_sprite_t {name} = {{ {w}, {h}, {ax}, {by}, {name}_row, {name}_runs }};")
        made.append((name, img, decode(rows, runs, w, h), len(runs)))
        total_px += w * h
        total_runs += len(runs)
    hill = bezier_y_table(toy_art.HILL_PATH, SCREEN_W)
    out.append("")
    out.append(f"// 동산의 윗선: 열 x 의 높이 × 16 (화면 좌표). toy::background 가 열마다 여기부터 바닥까지 칠한다.")
    out.append(f"static const uint16_t HILL_TOP_Q4[{SCREEN_W}] = {{")
    for i in range(0, SCREEN_W, 20):
        out.append("  " + ",".join(str(v) for v in hill[i:i + 20]) + ",")
    out.append("};")
    return out, made, total_px, total_runs


def write_preview(made, path: Path):
    """검수용 한 장 그림: 구운 것을 다시 풀어 하늘색 바탕에 늘어놓는다."""
    pad, gap, width = 16, 12, 1400
    x = y = pad
    row_h = 0
    places = []
    for name, _, dec, _ in made:
        w, h = dec.size
        if x + w + pad > width:
            x, y, row_h = pad, y + row_h + gap, 0
        places.append((x, y, dec))
        x += w + gap
        row_h = max(row_h, h)
    sheet = Image.new("RGBA", (width, y + row_h + pad), (191, 232, 255, 255))
    for px, py, dec in places:
        sheet.alpha_composite(dec, (px, py))
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.convert("RGB").save(path)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="토이폰 그림 자산(main/ui_sprites.h) 생성")
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT, help=f"출력 헤더 (기본: {DEFAULT_OUT})")
    ap.add_argument("--preview", type=Path, default=None, help="검수용 PNG 를 이 경로에 함께 저장")
    ap.add_argument("--sizes", action="store_true", help="자산별 크기 표를 출력")
    args = ap.parse_args(argv)
    try:
        out, made, total_px, total_runs = build()
    except SpriteError as e:
        print(f"[gen_toy_sprites] 오류: {e}", file=sys.stderr)
        return 2
    args.out.write_text("\n".join(out) + "\n", encoding="utf-8")
    if args.sizes:
        for name, img, _, n in made:
            print(f"  {name:26s} {img.size[0]:4d} x {img.size[1]:3d}  runs={n:6d}")
    if args.preview:
        write_preview(made, args.preview)
    print(f"[gen_toy_sprites] {args.out.name}: sprites={len(made)} pixels={total_px:,} runs={total_runs:,} "
          f"data_bytes={total_runs * 4:,} file_bytes={args.out.stat().st_size:,}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
