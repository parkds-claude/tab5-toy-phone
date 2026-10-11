#!/usr/bin/env python3
"""gen_sc_assets.py — 스택짱 전화기 화면(320×240)의 글자·그림 자산을 굽는다.

Tab5 의 굽는 도구를 그대로 쓴다(복사하지 않는다): 글자는 ../../tab5_phone/tools/gen_ui_assets.py 의 주아체 렌더, 그림은 gen_toy_sprites.py 의
SVG → 줄 묶음(RLE), 그림 원본은 toy_art.py (토이폰 테마 — docs/design/README.md §5). 여기에는 "스택짱 화면이 쓰는 크기 표"만 있다.

출력: main/sc_assets.h  — 글자 알파 마스크(sc_mask_t sc_lbl_*, SC_DIG_BIG[10], SC_DIG_SMALL[10])
      main/sc_sprites.h — 색 그림(ui_sprite_t sc_spr_*, 모양은 Tab5 의 sprite.h)

실행(ESP-IDF 를 export 하지 않은 셸에서 — Pillow·rsvg-convert 필요):
    python3 tools/gen_sc_assets.py [--preview out/assets.png] [--sizes]
"""
import argparse
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
TAB5_TOOLS = ROOT.parent / "tab5_phone" / "tools"
sys.path.insert(0, str(TAB5_TOOLS))
import gen_toy_sprites as spr  # noqa: E402
import gen_ui_assets as txt    # noqa: E402
import toy_art                 # noqa: E402

BODY, KEY, BIG, SMALL = 20, 26, 28, 20      # 글자 크기(px): 본문·안내 / 키캡 글자 / 화면의 한 줄 / 작은 숫자
DIGIT_BIG = 52                              # 통화 시간

# (C 식별자, 문구, 크기 px)
LABELS = [
    ("sc_lbl_wifi_starting",   "Wi-Fi 켜는 중...",              22),
    ("sc_lbl_wifi_searching",  "Wi-Fi 연결 중...",              22),
    ("sc_lbl_wifi_none",       "아는 Wi-Fi가 안 보여요",          22),
    ("sc_lbl_wifi_fail",       "Wi-Fi를 켜지 못했어요",           22),
    ("sc_lbl_relay_connecting", "교환기 연결 중...",             22),
    ("sc_lbl_relay_retry",     "교환기에 다시 연결하는 중...",     22),
    ("sc_lbl_auth_fail",       "기기 등록 오류 - 1분 뒤 다시",     22),
    ("sc_lbl_unregistered",    "등록되지 않은 기기예요",           22),
    ("sc_lbl_unit1",           "1번 전화기",                    BODY),
    ("sc_lbl_unit2",           "2번 전화기",                    BODY),
    ("sc_lbl_unit3",           "3번 전화기",                    BODY),
    ("sc_lbl_p1",              "1번",                           KEY),
    ("sc_lbl_p2",              "2번",                           KEY),
    ("sc_lbl_p3",              "3번",                           KEY),
    ("sc_lbl_deck",            "스트림덱",                       22),
    ("sc_lbl_deck_again",      "한 번 더!",                      22),
    ("sc_lbl_to_deck",         "스트림덱으로 가는 중...",          22),
    ("sc_lbl_missed",          "부재중",                        BODY),
    ("sc_lbl_count_unit",      "건",                            BODY),
    ("sc_lbl_dialing",         "거는 중...",                     BIG),
    ("sc_lbl_ringback",        "벨이 울리고 있어요",               BODY),
    ("sc_lbl_cancel",          "취소",                           KEY),
    ("sc_lbl_incoming",        "전화 왔어요!",                    BIG),
    ("sc_lbl_accept",          "받기",                           KEY),
    ("sc_lbl_reject",          "거절",                           KEY),
    ("sc_lbl_answering",       "연결 중...",                     BIG),
    ("sc_lbl_hangup",          "끊기",                           KEY),
    ("sc_lbl_ending",          "끊는 중...",                     BIG),
    ("sc_lbl_vol_dn",          "작게",                           22),
    ("sc_lbl_vol_up",          "크게",                           22),
    ("sc_lbl_colon",           ":",                             DIGIT_BIG),
    ("sc_lbl_n_peer_off",      "상대 전화기가 꺼져 있어요",         BODY),
    ("sc_lbl_n_no_answer",     "받지 않았어요",                   BODY),
    ("sc_lbl_n_rejected",      "상대가 받지 않고 끊었어요",         BODY),
    ("sc_lbl_n_lost",          "연결이 끊어졌습니다",              BODY),
    ("sc_lbl_n_ended",         "통화 종료",                      BODY),
    ("sc_lbl_n_missed",        "부재중 전화가 있어요",             BODY),
    ("sc_lbl_n_relay_silent",  "교환기 응답이 없어 다시 연결해요",   BODY),
    ("sc_lbl_n_busy",          "통화 중이에요",                   BODY),
]
# 거절 문구는 Tab5 의 목록을 그대로 쓴다(번호와 문구의 짝이 한 곳에만 있다 — gen_ui_assets.py DM-D13). Tab5 가 문구와 함께 거절하면 스택짱에도 뜬다.
LABELS += [(f"sc_lbl_n_msg{n}", "상대: " + text, BODY) for n, text in enumerate(txt.REJECT_MSGS, 1)]

# 그림: (이름, SVG 조각, viewBox, 배율, 대칭축 x, 바닥 y)
BODY_H, HEAD_H, TAG_H, ICON = 104, 60, 30, 30
CHARS = (("bear", toy_art.bear, 260, 246, toy_art.head_bear, 48),
         ("rabbit", toy_art.rabbit, 300, 286, toy_art.head_rabbit, 56),
         ("robot", toy_art.robot, 260, 246, toy_art.head_robot, 52))
SPRITES = []
for name, fn, h, base, head, head_h in CHARS:
    s = BODY_H / h
    SPRITES += [
        (f"sc_spr_{name}_wave",   fn("wave"),   (-12, 0, 264, h), s, 120, base),
        (f"sc_spr_{name}_cup",    fn("cup"),    (-12, 0, 264, h), s, 120, base),
        (f"sc_spr_{name}_caller", fn("caller"), (-90, 0, 420, h), s, 120, base + 3),
        (f"sc_spr_{name}_head",       head(False), (0, 0, 48, head_h), HEAD_H / head_h, 24, head_h),
        (f"sc_spr_{name}_head_sleep", head(True),  (0, 0, 48, head_h), HEAD_H / head_h, 24, head_h),
        (f"sc_spr_{name}_tag",        head(False), (0, 0, 48, head_h), TAG_H / head_h, 24, head_h),
    ]
SPRITES += [
    ("sc_spr_chick",    toy_art.chick("idle"), (-6, 0, 162, 160), 0.6, 75, 151),
    ("sc_spr_ico_call", toy_art.handset(-135), (0, 0, 64, 64), ICON / 64, 32, 64),
    ("sc_spr_ico_hang", toy_art.handset(0),    (0, 0, 64, 64), ICON / 64, 32, 64),
]


def build_text(fonts):
    out = ["// 자동 생성: tools/gen_sc_assets.py — 수정 금지. 주아체(Jua) 알파 마스크(8bit, 0=투명 255=불투명). 스택짱 전화기 화면(320×240)용.",
           f"// 글꼴: Jua Regular (SIL OFL 1.1)  sha256={fonts.sha256('bold')}",
           "#pragma once", "#include <stdint.h>",
           "typedef struct { uint16_t w, h; const uint8_t *a; } sc_mask_t;", ""]
    body, images = [], []
    keys = [k for k, *_ in LABELS]
    if len(set(keys)) != len(keys):
        raise txt.AssetError("자산 이름이 겹칩니다")
    for key, text, px in LABELS:
        fonts.check_glyphs(text, "bold", key)
        img = txt.render(fonts, text, px, "bold")
        txt.emit(key, img, body, f'"{text}" {px}px')
        images.append((key, img))
    for prefix, px in (("sc_dig_big", DIGIT_BIG), ("sc_dig_small", SMALL)):
        vbox = txt.digit_vbox(fonts, px, "bold")
        for d in "0123456789":
            img = txt.render(fonts, d, px, "bold", vbox, tight_x=True)
            txt.emit(f"{prefix}_{d}", img, body, f'"{d}" {px}px (숫자 10개 높이 같음)')
            images.append((f"{prefix}_{d}", img))
    out += [line.replace("ui_asset_t", "sc_mask_t") for line in body]
    out.append("static const sc_mask_t *const SC_DIG_BIG[10] = {" + ",".join(f"&sc_dig_big_{d}" for d in "0123456789") + "};")
    out.append("static const sc_mask_t *const SC_DIG_SMALL[10] = {" + ",".join(f"&sc_dig_small_{d}" for d in "0123456789") + "};")
    return out, images


def build_sprites():
    out = ["// 자동 생성: tools/gen_sc_assets.py — 수정 금지. 그림 원본은 ../tab5_phone/tools/toy_art.py (토이폰 테마). 스택짱 전화기 화면(320×240)용.",
           "// 줄 하나 = 묶음의 나열, 묶음 = 16비트 둘: [길이 << 8 | 알파], [RGB565]. row[y]..row[y+1] 이 y 줄의 묶음(단위: 묶음 개수).",
           "#pragma once", '#include "sprite.h"', ""]
    made = []
    for name, inner, vb, scale, axis, base in SPRITES:
        img = spr.rasterize(inner, vb, scale)
        w, h = img.size
        rows, runs = spr.encode(img)
        dec = spr.decode(rows, runs, w, h)
        spr.check_roundtrip(name, img, dec)
        ax, by = round((axis - vb[0]) * scale), round((base - vb[1]) * scale)
        words = []
        for n, a, c in runs:
            words += [(n << 8) | a, c]
        out.append(f"// {name}: {w}x{h}, 대칭축 x={ax}, 바닥 y={by}, 묶음 {len(runs)}")
        out.append(f"static const uint16_t {name}_runs[{len(words)}] = {{")
        out += ["  " + ",".join(str(v) for v in words[i:i + 20]) + "," for i in range(0, len(words), 20)]
        out.append("};")
        out.append(f"static const uint32_t {name}_row[{len(rows)}] = {{")
        out += ["  " + ",".join(str(v) for v in rows[i:i + 20]) + "," for i in range(0, len(rows), 20)]
        out.append("};")
        out.append(f"static const ui_sprite_t {name} = {{ {w}, {h}, {ax}, {by}, {name}_row, {name}_runs }};")
        made.append((name, dec, len(runs)))
    return out, made


def write_preview(images, made, path: Path):
    pad, gap, width = 10, 8, 900
    sheet = Image.new("RGBA", (width, 2400), (191, 232, 255, 255))
    x = y = pad
    row_h = 0
    for _, dec, _ in made:
        w, h = dec.size
        if x + w + pad > width:
            x, y, row_h = pad, y + row_h + gap, 0
        sheet.alpha_composite(dec, (x, y)); x += w + gap; row_h = max(row_h, h)
    y += row_h + gap * 2
    x = pad
    row_h = 0
    for _, im in images:
        w, h = im.size
        if x + w + pad > width:
            x, y, row_h = pad, y + row_h + gap, 0
        ink = Image.new("RGBA", im.size, (42, 31, 61, 255)); ink.putalpha(im)
        sheet.alpha_composite(ink, (x, y)); x += w + gap; row_h = max(row_h, h)
    sheet = sheet.crop((0, 0, width, y + row_h + pad))
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.convert("RGB").save(path)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="스택짱 전화기 화면 자산(main/sc_assets.h, main/sc_sprites.h) 생성")
    ap.add_argument("--font-dir", type=Path, default=TAB5_TOOLS / "fonts")
    ap.add_argument("--out-dir", type=Path, default=ROOT / "main")
    ap.add_argument("--preview", type=Path, default=None)
    ap.add_argument("--sizes", action="store_true")
    a = ap.parse_args(argv)
    try:
        t_out, images = build_text(txt.Fonts(a.font_dir))
        s_out, made = build_sprites()
    except (txt.AssetError, spr.SpriteError) as e:
        print(f"[gen_sc_assets] 오류: {e}", file=sys.stderr)
        return 2
    (a.out_dir / "sc_assets.h").write_text("\n".join(t_out) + "\n", encoding="utf-8")
    (a.out_dir / "sc_sprites.h").write_text("\n".join(s_out) + "\n", encoding="utf-8")
    if a.sizes:
        for name, im in images:
            print(f"  {name:26s} {im.size[0]:4d} x {im.size[1]:3d}")
        for name, dec, n in made:
            print(f"  {name:26s} {dec.size[0]:4d} x {dec.size[1]:3d}  runs={n}")
    if a.preview:
        write_preview(images, made, a.preview)
    print(f"[gen_sc_assets] labels={len(LABELS)} digits=20 sprites={len(made)} "
          f"sc_assets.h={(a.out_dir / 'sc_assets.h').stat().st_size:,}B sc_sprites.h={(a.out_dir / 'sc_sprites.h').stat().st_size:,}B")
    return 0


if __name__ == "__main__":
    sys.exit(main())
