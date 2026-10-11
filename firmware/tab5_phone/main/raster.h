// raster.h — 픽셀 합성의 순수 부분(하드웨어 무관). 기기(gfx.cpp)와 맥의 화면 미리보기(tools/sim)가 같은 코드로 그린다
//            → 맥에서 본 화면과 기기 화면이 픽셀까지 같다(내장 글꼴로 쓰는 바뀌는 글자만 예외). 맥 단위 시험: tools/test_raster.cpp.
//   캔버스는 native RGB565 한 장(줄 간격 = 폭). 모든 함수는 캔버스 밖을 잘라낸다(밖에 쓰지 않는다).
//   R-D1 둥근 모서리·원은 "픽셀 가운데에서 원 중심까지의 거리"로 덮임 정도를 구한다(가장자리 1px 만 섞는다).
//        이유: LovyanGFX 의 fillSmooth* 를 쓰면 맥에서 같은 그림을 못 만든다. 비용: 모서리마다 sqrt (반지름 120 인 원 하나에 약 1만 번,
//        화면 한 장 전체로 수 ms). 탈출구: gfx.cpp 에서 fill_rrect·fill_circle 만 스프라이트(s_ov.fillSmooth*)로 되돌린다.
//   R-D2 색 그림(ui_sprite_t)은 줄 단위 묶음(RLE)을 풀어 얹는다 — 불투명 묶음은 채우기, 반투명 묶음(가장자리)만 섞는다. 모양은 sprite.h.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "sprite.h"

namespace raster {

struct Canvas { uint16_t *px; int w, h; };

inline uint16_t rgb565(uint32_t c) { return (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F)); }

inline uint16_t blend565(uint16_t d, uint16_t s, uint32_t a)      // a: 0..255, s 를 d 위에
{
    if (a >= 255) return s;
    if (a == 0) return d;
    const uint32_t dr = d >> 11, dg = (d >> 5) & 63, db = d & 31;
    const uint32_t sr = s >> 11, sg = (s >> 5) & 63, sb = s & 31;
    const uint32_t ia = 255 - a;
    return (uint16_t)((((sr * a + dr * ia + 127) / 255) << 11) | (((sg * a + dg * ia + 127) / 255) << 5) | ((sb * a + db * ia + 127) / 255));
}

// 한 줄(높이 1)을 칠한다. v = RGB565.
inline void span(const Canvas &cv, int x, int y, int w, uint16_t v, uint32_t a = 255)
{
    if (y < 0 || y >= cv.h || a == 0) return;
    const int x0 = std::max(0, x), x1 = std::min(cv.w, x + w);
    if (x1 <= x0) return;
    uint16_t *p = cv.px + (size_t)y * cv.w + x0;
    if (a >= 255) std::fill_n(p, x1 - x0, v);
    else for (int i = x0; i < x1; ++i, ++p) *p = blend565(*p, v, a);
}

inline void plot(const Canvas &cv, int x, int y, uint16_t v, uint32_t a)
{
    if (x < 0 || x >= cv.w || y < 0 || y >= cv.h || a == 0) return;
    uint16_t *p = cv.px + (size_t)y * cv.w + x;
    *p = blend565(*p, v, a);
}

inline void fill(const Canvas &cv, uint32_t rgb) { std::fill_n(cv.px, (size_t)cv.w * cv.h, rgb565(rgb)); }

inline void fill_rect(const Canvas &cv, int x, int y, int w, int h, uint32_t rgb, uint32_t a = 255)
{
    const uint16_t v = rgb565(rgb);
    const int y0 = std::max(0, y), y1 = std::min(cv.h, y + h);
    for (int yy = y0; yy < y1; ++yy) span(cv, x, yy, w, v, a);
}

// 둥근 사각형(R-D1). r 은 짧은 변의 절반까지. a = 전체 불투명도.
inline void fill_rrect(const Canvas &cv, int x, int y, int w, int h, int r, uint32_t rgb, uint32_t a = 255)
{
    if (w <= 0 || h <= 0) return;
    r = std::max(0, std::min(r, std::min(w, h) / 2));
    const uint16_t v = rgb565(rgb);
    for (int yy = 0; yy < h; ++yy) {
        const int ty = y + yy;
        if (ty < 0 || ty >= cv.h) continue;
        float dy;
        if (yy < r) dy = (float)(r - yy) - 0.5f;
        else if (yy >= h - r) dy = (float)(yy - (h - r)) + 0.5f;
        else { span(cv, x, ty, w, v, a); continue; }
        int xx = 0;
        for (; xx < r; ++xx) {                                    // 모서리: 바깥에서 안으로 — 덮임이 1 이 되면 그 안쪽은 다 찬다
            const float dx = (float)(r - xx) - 0.5f;
            const float cov = (float)r - std::sqrt(dx * dx + dy * dy) + 0.5f;
            if (cov <= 0.0f) continue;
            if (cov >= 1.0f) break;
            const uint32_t aa = (uint32_t)(cov * (float)a + 0.5f);
            plot(cv, x + xx, ty, v, aa);
            plot(cv, x + w - 1 - xx, ty, v, aa);
        }
        span(cv, x + xx, ty, w - 2 * xx, v, a);
    }
}

inline void fill_circle(const Canvas &cv, int cx, int cy, int r, uint32_t rgb, uint32_t a = 255)
{
    if (r > 0) fill_rrect(cv, cx - r, cy - r, 2 * r, 2 * r, r, rgb, a);
}

// 중심이 소수인 작은 원(굵은 선을 찍어 그릴 때 쓴다).
inline void dot(const Canvas &cv, float fx, float fy, float r, uint16_t v)
{
    const int x0 = (int)std::floor(fx - r - 1.0f), x1 = (int)std::ceil(fx + r + 1.0f);
    const int y0 = (int)std::floor(fy - r - 1.0f), y1 = (int)std::ceil(fy + r + 1.0f);
    for (int yy = y0; yy <= y1; ++yy)
        for (int xx = x0; xx <= x1; ++xx) {
            const float dx = (float)xx + 0.5f - fx, dy = (float)yy + 0.5f - fy;
            const float cov = r - std::sqrt(dx * dx + dy * dy) + 0.5f;
            if (cov <= 0.0f) continue;
            if (cov >= 1.0f) { if (xx >= 0 && xx < cv.w && yy >= 0 && yy < cv.h) cv.px[(size_t)yy * cv.w + xx] = v; }
            else plot(cv, xx, yy, v, (uint32_t)(cov * 255.0f + 0.5f));
        }
}

// 굵은 2차 곡선(끝이 둥글다): (x0,y0) → 조절점 (cx,cy) → (x1,y1). 종이컵 전화의 줄.
// 반 픽셀 간격으로 원을 찍는다 — 가장자리 픽셀은 겹쳐 섞여 조금 진해지지만 선 굵기 4px 에서는 보이지 않는다.
inline void stroke_quad(const Canvas &cv, float x0, float y0, float cx, float cy, float x1, float y1, float width, uint32_t rgb)
{
    const uint16_t v = rgb565(rgb);
    const float len = std::fabs(x1 - x0) + std::fabs(y1 - y0) + std::fabs(cx - x0) + std::fabs(cy - y0);
    const int steps = std::max(2, (int)(len * 2.0f));
    float lx = -1e9f, ly = -1e9f;
    for (int i = 0; i <= steps; ++i) {
        const float t = (float)i / (float)steps, mt = 1.0f - t;
        const float px = mt * mt * x0 + 2.0f * mt * t * cx + t * t * x1, py = mt * mt * y0 + 2.0f * mt * t * cy + t * t * y1;
        if (std::fabs(px - lx) + std::fabs(py - ly) < 0.5f && i != steps) continue;
        dot(cv, px, py, width * 0.5f, v);
        lx = px; ly = py;
    }
}

// 알파 마스크(8bit)에 색을 입혀 얹는다 — 글자 자산.
inline void blit_mask(const Canvas &cv, const uint8_t *mask, int w, int h, int x, int y, uint32_t rgb)
{
    const uint16_t v = rgb565(rgb);
    for (int yy = 0; yy < h; ++yy) {
        const int ty = y + yy;
        if (ty < 0 || ty >= cv.h) continue;
        const uint8_t *row = mask + (size_t)yy * w;
        uint16_t *line = cv.px + (size_t)ty * cv.w;
        for (int xx = 0; xx < w; ++xx) {
            const int tx = x + xx;
            const uint8_t a = row[xx];
            if (!a || tx < 0 || tx >= cv.w) continue;
            line[tx] = blend565(line[tx], v, a);
        }
    }
}

// 색 그림(R-D2). (x, y) = 왼쪽 위. mirror = 좌우 뒤집기. opacity = 전체 불투명도(255 = 그대로).
inline void blit_rle(const Canvas &cv, const ui_sprite_t &s, int x, int y, bool mirror = false, uint32_t opacity = 255)
{
    for (int yy = 0; yy < s.h; ++yy) {
        const int ty = y + yy;
        if (ty < 0 || ty >= cv.h) continue;
        const uint16_t *p = s.runs + 2 * (size_t)s.row[yy], *e = s.runs + 2 * (size_t)s.row[yy + 1];
        int at = 0;                                               // 그림 안에서의 x
        for (; p < e; p += 2) {
            const int n = p[0] >> 8;
            uint32_t a = p[0] & 255u;
            if (a) {
                if (opacity < 255) a = (a * opacity + 127) / 255;
                span(cv, mirror ? x + s.w - at - n : x + at, ty, n, p[1], a);
            }
            at += n;
        }
    }
}

// 열 x0..x1-1 마다 "윗선 ~ 아래 끝(y_end, 그 줄은 빼고)"을 칠한다(동산). top_q4[x] = 열 x 의 윗선 높이 × 16 — 표는 열 0 부터 있어야 한다.
// 윗선이 걸친 픽셀은 덮인 만큼만 섞는다 → 같은 자리에 두 번 부르면 그 픽셀이 더 진해진다. 다시 칠할 때는 먼저 바탕색을 깔 것(toy::background_patch).
inline void fill_below(const Canvas &cv, const uint16_t *top_q4, int x0, int x1, uint32_t rgb, int y_end = 1 << 30)
{
    const uint16_t v = rgb565(rgb);
    const int bottom = std::min(cv.h, y_end);
    for (int x = std::max(0, x0); x < std::min(x1, cv.w); ++x) {
        const int y0 = top_q4[x] >> 4, frac = top_q4[x] & 15;
        if (y0 >= bottom) continue;
        plot(cv, x, y0, v, (uint32_t)((16 - frac) * 255 / 16));
        uint16_t *p = cv.px + (size_t)(y0 + 1) * cv.w + x;
        for (int y = y0 + 1; y < bottom; ++y, p += cv.w) *p = v;
    }
}

}  // namespace raster
