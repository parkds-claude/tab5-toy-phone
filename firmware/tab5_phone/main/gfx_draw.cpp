// gfx_draw.cpp — gfx.h 의 그리기 함수들. raster.h 로만 그린다(하드웨어 무관) → 기기와 맥의 화면 미리보기(tools/sim)가 이 파일을 함께 쓴다.
// 합성 버퍼를 얻는 길은 gfx::detail::target() 하나다(기기: gfx.cpp, 맥: tools/sim/sim_gfx.cpp). 준비 전이면 아무것도 그리지 않는다.
#include "gfx.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace gfx {

using raster::Canvas;

void clear(uint32_t rgb)
{
    Canvas *cv = detail::target();
    if (!cv) return;
    detail::frame_begin();                                    // G-D4: 프레임은 clear() 로 시작한다 → 기본값은 공개
    raster::fill(*cv, rgb);
}

void fill_rect(int x, int y, int w, int h, uint32_t rgb) { if (Canvas *cv = detail::target()) raster::fill_rect(*cv, x, y, w, h, rgb); }
void fill_rrect(int x, int y, int w, int h, int r, uint32_t rgb) { if (Canvas *cv = detail::target()) raster::fill_rrect(*cv, x, y, w, h, r, rgb); }
void fill_circle(int cx, int cy, int r, uint32_t rgb) { if (Canvas *cv = detail::target()) raster::fill_circle(*cv, cx, cy, r, rgb); }
void fill_alpha_rect(int x, int y, int w, int h, uint32_t rgb, uint8_t a) { if (Canvas *cv = detail::target()) raster::fill_rect(*cv, x, y, w, h, rgb, a); }
void fill_alpha_rrect(int x, int y, int w, int h, int r, uint32_t rgb, uint8_t a) { if (Canvas *cv = detail::target()) raster::fill_rrect(*cv, x, y, w, h, r, rgb, a); }
void fill_alpha_circle(int cx, int cy, int r, uint32_t rgb, uint8_t a) { if (Canvas *cv = detail::target()) raster::fill_circle(*cv, cx, cy, r, rgb, a); }
void fill_below(const uint16_t *top_q4, uint32_t rgb, int x0, int x1, int y_end) { if (Canvas *cv = detail::target()) raster::fill_below(*cv, top_q4, x0, x1, rgb, y_end); }

void stroke_quad(int x0, int y0, int cx, int cy, int x1, int y1, int width, uint32_t rgb)
{
    if (Canvas *cv = detail::target()) raster::stroke_quad(*cv, (float)x0, (float)y0, (float)cx, (float)cy, (float)x1, (float)y1, (float)width, rgb);
}

int blit_asset(const ui_asset_t *as, int x, int y, uint32_t rgb)
{
    Canvas *cv = as ? detail::target() : nullptr;
    if (!cv) return 0;
    raster::blit_mask(*cv, as->a, as->w, as->h, x, y, rgb);
    return as->w;
}

void blit_asset_center(const ui_asset_t *as, int cx, int cy, uint32_t rgb)
{
    if (as) blit_asset(as, cx - as->w / 2, cy - as->h / 2, rgb);
}

void blit_sprite(const ui_sprite_t *s, int x, int y, bool mirror, uint8_t opacity)
{
    Canvas *cv = s ? detail::target() : nullptr;
    if (cv) raster::blit_rle(*cv, *s, x, y, mirror, opacity);
}

void blit_sprite_center(const ui_sprite_t *s, int cx, int cy, uint8_t opacity)
{
    if (s) blit_sprite(s, cx - s->w / 2, cy - s->h / 2, false, opacity);
}

// 뒤집으면 대칭축이 그림의 반대쪽으로 간다: 그림 안의 열 c 는 화면에서 x + (w - 1 - c).
void blit_sprite_at(const ui_sprite_t *s, int axis_x, int base_y, bool mirror)
{
    if (s) blit_sprite(s, mirror ? axis_x - (s->w - 1 - s->ax) : axis_x - s->ax, base_y - s->by, mirror);
}

namespace {

constexpr int GAP_BIG = 14, GAP_SMALL = 2;                    // 숫자 자산은 잉크에 맞춰 잘려 있다(좌우 1px) → 글꼴 본래 간격만큼 띄운다

int digits_of(int n, char *s, size_t cap) { return snprintf(s, cap, "%d", std::max(0, n)); }

}  // namespace

int number_w(int n, bool big)
{
    char s[12];
    digits_of(n, s, sizeof s);
    const ui_asset_t *const *set = big ? DIG_BIG : DIG_SMALL;
    const int gap = big ? GAP_BIG : GAP_SMALL;
    int total = 0;
    for (const char *q = s; *q; ++q) total += set[*q - '0']->w + gap;
    return total - gap;
}

void draw_number(int n, int cx, int cy, bool big, uint32_t rgb)
{
    char s[12];
    digits_of(n, s, sizeof s);
    const ui_asset_t *const *set = big ? DIG_BIG : DIG_SMALL;
    const int gap = big ? GAP_BIG : GAP_SMALL;
    int h = 0;
    for (const char *q = s; *q; ++q) h = std::max<int>(h, set[*q - '0']->h);
    int x = cx - number_w(n, big) / 2;
    for (const char *q = s; *q; ++q) { const ui_asset_t *d = set[*q - '0']; blit_asset(d, x, cy - h / 2 + (h - d->h) / 2, rgb); x += d->w + gap; }
}

// "MM:SS". 숫자마다 폭이 다르므로(1 이 좁다) 가장 넓은 숫자 폭의 칸에 가운데로 넣는다 → 초가 바뀌어도 글자가 좌우로 흔들리지 않는다.
// 전체 폭 = 칸 4개 + 숫자 사이 10×2 + 쌍점 + 쌍점 좌우 6×2 (자산이 바뀌면 달라진다 — 주아체 160px 에서 약 435px). 분이 100 이상이면 분 자리만 늘어난다.
// 간격은 주아체에 맞춘 값이다(둥글고 넓은 숫자라 Pretendard 때의 20·12 는 벌어져 보였다).
void draw_clock(int minutes, int seconds, int cx, int cy, uint32_t rgb)
{
    minutes = std::max(0, minutes);
    seconds = std::min(59, std::max(0, seconds));
    char mm[12];
    snprintf(mm, sizeof mm, "%02d", minutes);
    const int nm = (int)strlen(mm);
    int cell = 0, h = 0;
    for (int i = 0; i < 10; ++i) { cell = std::max<int>(cell, DIG_BIG[i]->w); h = std::max<int>(h, DIG_BIG[i]->h); }
    const int gap = 10, colon_pad = 6;
    const int total = (nm + 2) * cell + nm * gap + 2 * colon_pad + lbl_colon.w;   // 숫자 사이 틈: 분 (nm-1)개 + 초 1개
    int x = cx - total / 2;
    auto digit = [&](int v) {
        const ui_asset_t *d = DIG_BIG[v];
        blit_asset(d, x + (cell - d->w) / 2, cy - h / 2 + (h - d->h) / 2, rgb);
        x += cell;
    };
    for (int i = 0; i < nm; ++i) { if (i) x += gap; digit(mm[i] - '0'); }
    x += colon_pad;
    blit_asset(&lbl_colon, x, cy - lbl_colon.h / 2, rgb);
    x += lbl_colon.w + colon_pad;
    digit(seconds / 10);
    x += gap;
    digit(seconds % 10);
}

}  // namespace gfx
