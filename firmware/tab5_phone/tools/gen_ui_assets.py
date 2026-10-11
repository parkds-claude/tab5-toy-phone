#!/usr/bin/env python3
"""UI 글자 자산 생성기 — 주아체(Jua)로 안티앨리어싱 렌더한 알파(8bit) 비트맵을 C 헤더로.

왜: Tab5 화면에서 내장 비트맵 글꼴(efontKR)을 확대하면 계단이 생긴다(docs/design/README.md §2 "내장 비트맵 폰트 확대 금지").
    맥에서 주아체로 한 번 그려 알파 마스크로 굽고, 기기는 합성 버퍼에 색을 알파 블렌딩만 한다(gfx::blit_asset).
    글꼴은 토이폰 테마(docs/design/README.md §5, 2026-10-08)에서 Pretendard → 주아체로 바꿨다: 둥글고 굵은 한 벌(굵기 하나)이라 장난감 전화기 화면에 맞는다.
    주아체에는 말줄임표·줄표·가운뎃점이 없다 → 문구에는 "..." 과 "-" 를 쓴다(없는 글자는 아래 검사가 오류로 잡는다).
출처: firmware/tab5_snapbooth/tools/gen_ui_assets.py. 바꾼 것:
  - 글꼴이 없으면 다른 글꼴로 넘어가지 않고 오류로 끝난다(원본은 조용히 AppleSDGothicNeo 로 대체했다).
  - 글꼴에 없는 글자가 문구에 있어도 오류로 끝난다(빈 네모가 구워지는 것을 막는다).
  - 스냅부스 스타일 이름 부분을 걷어냈다.
  - 숫자 10개는 세로 범위를 하나로 맞추고(높이가 같아 시계 숫자가 바뀔 때 위아래로 흔들리지 않는다),
    가로는 잉크에 딱 맞게 자른다(원본은 글꼴 고유의 좌우 여백이 붙어 있었다 — "1" 은 좌우가 달라 칸 안에서 쏠린다).
입력: 아래 LABELS (통화 화면 문구 + Wi-Fi 설정 화면용 lbl_wf_* + 수신거부 문구 REJECT_MSGS). 이름은 main/ui.cpp·main/ui_wifi.cpp 가 쓰는 것과 같아야 한다
출력: main/ui_assets.h (ui_asset_t, lbl_*, DIG_BIG[10], DIG_SMALL[10])

실행(ESP-IDF 를 export 하지 않은 셸에서 — export 한 셸의 python3 에는 Pillow 가 없다):
    python3 tools/gen_ui_assets.py                       # tools/fonts/ 의 Jua-Regular.ttf 사용
    python3 tools/gen_ui_assets.py --font-dir <폴더> --preview /tmp/assets.png
글꼴: https://github.com/google/fonts/tree/main/ofl/jua (SIL OFL 1.1) 의 Jua-Regular.ttf 를 tools/fonts/ 에 둔다
      (시스템 글꼴 폴더에 설치하지 않는다. tools/fonts/ 는 git 에 올리지 않는다).
"""
import argparse
import hashlib
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUT = ROOT / "main" / "ui_assets.h"
DEFAULT_FONT_DIR = Path(__file__).resolve().parent / "fonts"
# 주아체는 굵기가 하나뿐이다. 문구 표의 굵기 칸(bold·xbold)은 그대로 두고 둘 다 같은 파일을 가리킨다(굵기가 여럿인 글꼴로 바꿀 때를 위해).
FONT_FAMILY = "Jua"
FONT_FILES = {"bold": ("Jua-Regular.ttf", "Regular"), "xbold": ("Jua-Regular.ttf", "Regular")}
FONT_URL = "https://github.com/google/fonts/raw/main/ofl/jua/Jua-Regular.ttf"

# (C 식별자, 문구, 크기 px, 굵기)
LABELS = [
    # ── 전화기 화면 (main/ui.cpp 가 쓴다) ──
    ("lbl_wifi_connecting",  "Wi-Fi 연결 중...",                                  40, "bold"),
    ("lbl_relay_connecting", "교환기 연결 중...",                                  40, "bold"),
    ("lbl_wifi_none",        "Wi-Fi가 없어요 - 아래 버튼을 눌러 주세요",                32, "bold"),
    ("lbl_wifi_init_fail",   "Wi-Fi 칩을 켜지 못했어요. 전원을 다시 넣어 주세요",    32, "bold"),
    ("lbl_relay_fail",       "교환기에 연결할 수 없어요 - 다시 시도하는 중",          32, "bold"),
    ("lbl_auth_fail",        "기기 등록 오류 - 1분 뒤 다시 시도해요",            32, "bold"),
    ("lbl_unit1",            "1번 전화기",                                       28, "bold"),
    ("lbl_unit2",            "2번 전화기",                                       28, "bold"),
    ("lbl_unit0",            "등록되지 않은 기기",                                28, "bold"),
    ("lbl_unreg_help",       "등록되지 않은 기기예요. 펌웨어의 기기 표를 확인해 주세요", 32, "bold"),
    ("lbl_wifi_setup",       "Wi-Fi 설정",                                      32, "bold"),   # 누르는 배지 위의 글자 → 버튼 라벨 기준(32px 이상)
    ("lbl_peer_on",         "상대 전화기 켜짐",                                   32, "bold"),
    ("lbl_peer_off",         "상대 전화기 꺼짐",                             32, "bold"),
    ("lbl_call",             "통화",                                            56, "xbold"),
    ("lbl_missed",           "부재중",                                          32, "bold"),
    ("lbl_count_unit",       "건",                                              32, "bold"),
    ("lbl_dialing",          "거는 중...",                                        56, "xbold"),
    ("lbl_ringback",         "상대 벨이 울리고 있어요",                           32, "bold"),
    ("lbl_cancel",           "취소",                                            40, "bold"),
    ("lbl_incoming",         "따르릉! 전화 왔어요",                                  56, "xbold"),
    ("lbl_accept",           "받기",                                            48, "xbold"),
    ("lbl_reject",           "거절",                                            48, "xbold"),
    ("lbl_answering",        "연결 중...",                                        56, "xbold"),
    ("lbl_in_call",          "통화 중",                                         40, "bold"),
    ("lbl_hangup",           "끊기",                                            48, "xbold"),
    ("lbl_ending",           "끊는 중...",                                        40, "bold"),
    ("lbl_colon",            ":",                                              160, "xbold"),
    ("lbl_n_peer_off",       "상대 전화기가 꺼져 있어 걸 수 없어요",                  36, "bold"),
    ("lbl_n_no_answer",      "받지 않았어요",                                    36, "bold"),
    ("lbl_n_rejected",       "상대가 받지 않고 끊었어요",                         36, "bold"),
    ("lbl_n_lost",           "연결이 끊어졌습니다",                               36, "bold"),
    ("lbl_n_ended",          "통화 종료",                                       36, "bold"),
    ("lbl_n_missed",         "부재중 전화가 있어요",                              36, "bold"),
    ("lbl_n_relay_silent",   "교환기 응답이 없어 다시 연결합니다",                  36, "bold"),
    # ── Wi-Fi 설정 화면 (main/ui_wifi.cpp). 버튼 36px, 안내 28~32px, 제목 40px ──
    ("lbl_wf_title",         "Wi-Fi 선택",                                      40, "bold"),
    ("lbl_wf_connected",     "연결됨",                                          28, "bold"),
    ("lbl_wf_disconnected",  "연결 안 됨",                                       28, "bold"),
    ("lbl_wf_saved",         "저장됨",                                          28, "bold"),
    ("lbl_wf_scanning",      "주변 Wi-Fi를 찾는 중...",                            32, "bold"),
    ("lbl_wf_empty",         "보이는 2.4GHz Wi-Fi가 없어요 - [다시 검색]을 눌러 주세요", 32, "bold"),
    ("lbl_wf_prev",          "이전",                                            36, "bold"),
    ("lbl_wf_next",          "다음",                                            36, "bold"),
    ("lbl_wf_rescan",        "다시 검색",                                       36, "bold"),
    ("lbl_wf_rescan_busy",   "검색 중...",                                        36, "bold"),
    ("lbl_wf_close",         "닫기",                                            36, "bold"),
    ("lbl_wf_join",          "에 연결",                                         32, "bold"),
    ("lbl_wf_show",          "보기",                                            36, "bold"),
    ("lbl_wf_hide",          "숨기기",                                          36, "bold"),
    ("lbl_wf_hint",          "영문, 숫자, 기호를 입력하고 [연결]을 누르세요",          28, "bold"),
    ("lbl_wf_connecting",    "연결 중...",                                        28, "bold"),
    ("lbl_wf_fail_auth",     "비밀번호가 맞지 않습니다",                          28, "bold"),
    ("lbl_wf_fail_timeout",  "연결하지 못했어요 - 신호와 비밀번호를 확인해 주세요", 28, "bold"),
    ("lbl_wf_short_pass",    "비밀번호는 8자 이상입니다",                         28, "bold"),
    ("lbl_wf_cancel",        "취소",                                            36, "bold"),
    ("lbl_wf_connect",       "연결",                                            36, "bold"),
]
# ── 수신거부 메시지·벨 끄기 (docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md §5.5). 전부 Bold 36px ──
# DM-D13: 거절 문구는 여기 한 곳에만 적는다(순서 = 문구 번호 1·2·3). 버튼용(lbl_msgN)과 거는 쪽 안내용(lbl_n_msgN, "상대: …")을 함께 만든다.
#   이유: 번호와 문구의 짝이 한 곳에만 있다. 개수는 main/call_fsm.h 의 MSG_COUNT 와 같아야 하고 main/ui.cpp 의 static_assert 가 빌드에서 잡는다.
#   비용: 목록과 아래 두 줄. 탈출구: LABELS 에 여섯 줄을 직접 적는다.
# 기존 자산 뒤에 덧붙인다 → 다시 구워도 ui_assets.h 의 기존 줄은 한 줄도 달라지지 않는다(추가만).
REJECT_MSGS = ["회의 중이에요", "지금은 받을 수 없어요", "잠시 후 다시 걸게요"]
LABELS += [
    ("lbl_mute",             "벨 끄기",                                          36, "bold"),   # 벨 화면 아래 줄 왼쪽
    ("lbl_mute_done",        "벨 꺼짐",                                          36, "bold"),   #   끈 뒤(버튼이 아니라 상태 표시)
    ("lbl_decline",          "메시지로 거절",                                     36, "bold"),   # 벨 화면 아래 줄 오른쪽
    ("lbl_back",             "뒤로",                                             36, "bold"),   # 메시지 고르기 화면
    ("lbl_decline_hint",     "보낼 메시지를 누르면 거절됩니다",                      36, "bold"),
]
LABELS += [(f"lbl_msg{n}", text, 36, "bold") for n, text in enumerate(REJECT_MSGS, 1)]
LABELS += [(f"lbl_n_msg{n}", "상대: " + text, 36, "bold") for n, text in enumerate(REJECT_MSGS, 1)]
# ── 전화기 3대 (docs/exec-plans/2026-10-08-phone-3way-design.md §9, docs/design/README.md §5.4·§5.5) ──
# 기존 자산 뒤에 덧붙인다 → 다시 구워도 ui_assets.h 의 기존 줄은 한 줄도 달라지지 않는다(추가만).
# "상대 전화기 켜짐/꺼짐"(lbl_peer_on·lbl_peer_off)은 대기 화면이 상대별 이름 알약으로 바뀌어 이제 쓰이지 않는다 — 이름·순서를 지키려고 남겨 둔다(쓰이지 않는 자산은 바이너리에 들어가지 않는다).
LABELS += [
    ("lbl_unit3",            "3번 전화기",                                       28, "bold"),   # 스택짱(로봇)의 이름. lbl_unit1·2 와 같은 크기 — 내 이름표와 대기 화면의 이름 알약이 함께 쓴다
    ("lbl_n_busy",           "통화 중이에요",                                    36, "bold"),   # 안내 BUSY: 건 상대가 다른 통화·벨 중이다(설계서 TW-D9)
    ("lbl_call_s",           "통화",                                            48, "xbold"),  # 대기 화면의 통화 키캡(상대마다 하나, 지름 176). 주요 버튼의 글자 크기 48(디자인 규칙 §5.2) — lbl_call 56px 는 예전 지름 240 짜리의 글자
]
# 대기 화면의 이름 알약(상대마다 하나): "N번 전화기" 를 상태 글자 크기(32)로 — 예전 "상대 전화기 켜짐" 알약과 같은 크기다. 내 이름표(왼쪽 위)는 lbl_unitN(28) 그대로.
LABELS += [(f"lbl_name{n}", f"{n}번 전화기", 32, "bold") for n in (1, 2, 3)]
DIGIT_BIG = (160, "xbold")    # 통화 시간 "MM:SS" (gfx::draw_clock), 큰 숫자
DIGIT_SMALL = (32, "bold")    # 부재중 건수 등


class AssetError(Exception):
    pass


class Fonts:
    """주아체만 쓴다. 파일이 없거나 주아체가 아니면 AssetError (대체 글꼴 없음)."""

    def __init__(self, font_dir: Path):
        self.paths = {}
        for weight, (fname, style) in FONT_FILES.items():
            p = font_dir / fname
            if not p.is_file():
                raise AssetError(f"글꼴이 없습니다: {p}\n  → {FONT_URL} 을 {font_dir}/ 에 넣어 주세요 "
                                 f"(다른 글꼴로 대체하지 않습니다).")
            family, got_style = ImageFont.truetype(str(p), 16).getname()
            if family != FONT_FAMILY or got_style != style:
                raise AssetError(f"{p} 는 {FONT_FAMILY} {style} 가 아닙니다 (읽은 이름: {family} {got_style}).")
            self.paths[weight] = p
        self._cache = {}

    def get(self, px: int, weight: str) -> ImageFont.FreeTypeFont:
        key = (px, weight)
        if key not in self._cache:
            self._cache[key] = ImageFont.truetype(str(self.paths[weight]), px)
        return self._cache[key]

    def sha256(self, weight: str) -> str:
        return hashlib.sha256(self.paths[weight].read_bytes()).hexdigest()

    def check_glyphs(self, text: str, weight: str, where: str):
        """글꼴에 없는 글자는 '없는 글자' 그림(.notdef)으로 그려진다 → 그 그림과 같으면 오류."""
        f = self.get(48, weight)
        notdef = f.getmask("￾")
        nd = (notdef.size, bytes(notdef))
        for ch in sorted(set(text)):
            if ch.isspace():
                continue
            m = f.getmask(ch)
            if (m.size, bytes(m)) == nd:
                raise AssetError(f"{where}: 글꼴({FONT_FILES[weight][0]})에 없는 글자 {ch!r} (U+{ord(ch):04X})")


def render(fonts: Fonts, text: str, px: int, weight: str, vbox=None, tight_x=False) -> Image.Image:
    """글자를 알파 그림으로(사방 1px 여백). 가로는 글꼴이 정한 글자 폭(좌우에 글꼴 고유 여백이 조금 포함된다).
    vbox=(위, 아래)를 주면 세로 범위를 그 값으로 고정. tight_x=True 면 가로를 실제 잉크에 딱 맞게 자른다(좌우 1px 여백)."""
    f = fonts.get(px, weight)
    d = ImageDraw.Draw(Image.new("L", (10, 10)))
    l, t, r, b = d.textbbox((0, 0), text, font=f)
    if vbox:
        t, b = vbox
    w, h = r - l + 2, b - t + 2
    img = Image.new("L", (w, h), 0)
    ImageDraw.Draw(img).text((-l + 1, -t + 1), text, font=f, fill=255)
    if tight_x:
        ink = img.getbbox()
        if ink is None:
            raise AssetError(f"{text!r}: 그려진 것이 없습니다")
        img = img.crop((max(0, ink[0] - 1), 0, min(w, ink[2] + 1), h))
    return img


def digit_vbox(fonts: Fonts, px: int, weight: str):
    f = fonts.get(px, weight)
    d = ImageDraw.Draw(Image.new("L", (10, 10)))
    boxes = [d.textbbox((0, 0), ch, font=f) for ch in "0123456789"]
    return min(bx[1] for bx in boxes), max(bx[3] for bx in boxes)


def emit(name: str, img: Image.Image, out: list, note: str):
    w, h = img.size
    if not (0 < w < 65536 and 0 < h < 65536):
        raise AssetError(f"{name}: 크기 {w}x{h} 가 uint16_t 범위를 벗어납니다")
    data = img.tobytes()
    out.append(f"// {name}: {note} → {w}x{h}")
    out.append(f"static const uint8_t {name}_a[{len(data)}] = {{")
    for i in range(0, len(data), 24):
        out.append("  " + ",".join(str(b) for b in data[i:i + 24]) + ",")
    out.append("};")
    out.append(f"static const ui_asset_t {name} = {{ {w}, {h}, {name}_a }};")


def build(fonts: Fonts):
    """돌려주는 값: (헤더 줄 목록, [(이름, 그림)], 알파 바이트 합)"""
    keys = [k for k, *_ in LABELS]
    dup = sorted({k for k in keys if keys.count(k) > 1})
    if dup:
        raise AssetError(f"자산 이름이 겹칩니다: {dup}")
    out = ["// 자동 생성: tools/gen_ui_assets.py — 수정 금지. 주아체(Jua) 알파 마스크(8bit, 0=투명 255=불투명).",
           f"// 글꼴: Jua Regular (SIL OFL 1.1)  sha256={fonts.sha256('bold')}",
           "#pragma once", "#include <stdint.h>",
           "typedef struct { uint16_t w, h; const uint8_t *a; } ui_asset_t;", ""]
    images, total = [], 0
    for key, text, px, wt in LABELS:
        fonts.check_glyphs(text, wt, key)
        img = render(fonts, text, px, wt)
        emit(key, img, out, f'"{text}" {FONT_FILES[wt][1]} {px}px')
        images.append((key, img)); total += img.size[0] * img.size[1]
    for prefix, (px, wt) in (("dig_big", DIGIT_BIG), ("dig_small", DIGIT_SMALL)):
        fonts.check_glyphs("0123456789", wt, prefix)
        vbox = digit_vbox(fonts, px, wt)
        for dgt in "0123456789":
            # 숫자는 잉크에 딱 맞게 자른다: 글꼴의 "1" 은 좌우 여백이 달라(왼쪽 8·오른쪽 18px @160) 그대로 칸에 넣으면 한쪽으로 쏠린다.
            # 간격은 그리는 쪽(gfx::draw_number / draw_clock)이 준다.
            img = render(fonts, dgt, px, wt, vbox, tight_x=True)
            emit(f"{prefix}_{dgt}", img, out, f'"{dgt}" {FONT_FILES[wt][1]} {px}px (숫자 10개 높이 같음, 좌우는 잉크에 맞춤)')
            images.append((f"{prefix}_{dgt}", img)); total += img.size[0] * img.size[1]
    out.append("static const ui_asset_t *const DIG_BIG[10] = {" + ",".join(f"&dig_big_{d}" for d in "0123456789") + "};")
    out.append("static const ui_asset_t *const DIG_SMALL[10] = {" + ",".join(f"&dig_small_{d}" for d in "0123456789") + "};")
    return out, images, total


def write_preview(images, path: Path):
    """검수용 한 장 그림: 자산을 위에서 아래로 늘어놓는다(어두운 배경, 흰 글자)."""
    pad, gap = 16, 10
    width = max(im.size[0] for _, im in images) + 2 * pad
    height = pad + sum(im.size[1] + gap for _, im in images) + pad
    sheet = Image.new("L", (width, height), 22)
    y = pad
    for _, im in images:
        sheet.paste(Image.eval(im, lambda a: 22 + a * (242 - 22) // 255), (pad, y))
        y += im.size[1] + gap
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="주아체 글자 자산(main/ui_assets.h) 생성")
    ap.add_argument("--font-dir", type=Path, default=DEFAULT_FONT_DIR,
                    help=f"Jua-Regular.ttf 가 있는 폴더 (기본: {DEFAULT_FONT_DIR})")
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT, help=f"출력 헤더 (기본: {DEFAULT_OUT})")
    ap.add_argument("--preview", type=Path, default=None, help="검수용 PNG 를 이 경로에 함께 저장")
    ap.add_argument("--sizes", action="store_true", help="자산별 크기 표를 출력")
    args = ap.parse_args(argv)
    try:
        fonts = Fonts(args.font_dir)
        out, images, total = build(fonts)
    except AssetError as e:
        print(f"[gen_ui_assets] 오류: {e}", file=sys.stderr)
        return 2
    args.out.write_text("\n".join(out) + "\n", encoding="utf-8")
    if args.sizes:
        for name, im in images:
            print(f"  {name:24s} {im.size[0]:4d} x {im.size[1]:3d}")
    if args.preview:
        write_preview(images, args.preview)
    print(f"[gen_ui_assets] {args.out.name}: labels={len(LABELS)} digits=20 total={len(images)} "
          f"alpha_bytes={total:,} file_bytes={args.out.stat().st_size:,}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
