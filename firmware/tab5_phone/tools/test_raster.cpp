// test_raster.cpp — 픽셀 합성의 순수 부분(main/raster.h) 맥 단위 시험.
//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --show-sdk-path)" -I../main test_raster.cpp -o /tmp/t_raster && /tmp/t_raster
// 고정하는 것: 캔버스 밖에 쓰지 않는다 / 둥근 사각형·원의 모양(대칭·넓이·가장자리만 섞임) / 색 그림(RLE) 풀기와 좌우 뒤집기 / 동산 채우기 / 굵은 곡선.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "raster.h"

using namespace raster;

static int g_pass = 0, g_fail = 0;
#define CHECK(name, cond) do { if (cond) ++g_pass; else { ++g_fail; printf("FAIL: %s\n", name); } } while (0)

namespace {

const uint16_t BG = 0x1234, GUARD = 0xBEEF;

// 캔버스 사방에 띠를 두른 버퍼: 그리기가 캔버스 밖을 건드리면 띠가 깨진다.
struct Board {
    int w, h, pad;
    std::vector<uint16_t> mem;
    Board(int w_, int h_, int pad_ = 8) : w(w_), h(h_), pad(pad_), mem((size_t)(w_ + 2 * pad_) * (h_ + 2 * pad_), GUARD) { clear(); }
    // 주의: 캔버스의 줄 간격은 폭과 같아야 한다 → 띠는 위아래에만 실제로 둔다(좌우는 "다음 줄로 넘어갔는가"로 본다).
    std::vector<uint16_t> px;
    void clear() { px.assign((size_t)w * (h + 2 * pad), GUARD); for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) at(x, y) = BG; }
    uint16_t &at(int x, int y) { return px[(size_t)(y + pad) * w + x]; }
    Canvas cv() { return Canvas{ px.data() + (size_t)pad * w, w, h }; }
    bool guards_ok() const
    {
        for (int i = 0; i < pad * w; ++i) if (px[i] != GUARD || px[px.size() - 1 - i] != GUARD) return false;
        return true;
    }
    int count(uint16_t v) { int n = 0; for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) n += at(x, y) == v; return n; }
    int changed() { return w * h - count(BG); }
};

const uint32_t RED = 0xFF0000u;
const uint16_t RED565 = 0xF800;

}  // namespace

int main()
{
    // ── 색 ──
    CHECK("rgb565: pure colors", rgb565(0xFF0000) == 0xF800 && rgb565(0x00FF00) == 0x07E0 && rgb565(0x0000FF) == 0x001F && rgb565(0xFFFFFF) == 0xFFFF && rgb565(0) == 0);
    CHECK("blend565: 0 keeps, 255 replaces", blend565(0x1234, 0xF800, 0) == 0x1234 && blend565(0x1234, 0xF800, 255) == 0xF800 && blend565(0x1234, 0xF800, 999) == 0xF800);
    {
        const uint16_t mid = blend565(0x0000, 0xFFFF, 128);
        CHECK("blend565: half is about half", ((mid >> 11) == 16 || (mid >> 11) == 15) && (((mid >> 5) & 63) == 32 || ((mid >> 5) & 63) == 31));
    }

    // ── 사각형: 모양과 잘라내기 ──
    {
        Board b(40, 30);
        Canvas cv = b.cv();
        fill_rect(cv, 5, 6, 10, 4, RED);
        CHECK("fill_rect: exact area", b.count(RED565) == 40 && b.at(5, 6) == RED565 && b.at(14, 9) == RED565 && b.at(4, 6) == BG && b.at(15, 9) == BG && b.at(5, 10) == BG);
        b.clear();
        fill_rect(cv, -20, -20, 200, 200, RED);
        CHECK("fill_rect: clipped to the canvas, guards intact", b.count(RED565) == 40 * 30 && b.guards_ok());
        b.clear();
        fill_rect(cv, 38, 28, 10, 10, RED); fill_rect(cv, -8, -8, 10, 10, RED); fill_rect(cv, 100, 5, 4, 4, RED); fill_rect(cv, 5, -100, 4, 4, RED);
        CHECK("fill_rect: partial and fully-outside rects", b.count(RED565) == 4 + 4 && b.guards_ok());
        b.clear();
        fill_rect(cv, 3, 3, 0, 5, RED); fill_rect(cv, 3, 3, 5, 0, RED); fill_rect(cv, 3, 3, -4, 5, RED);
        CHECK("fill_rect: empty sizes draw nothing", b.changed() == 0);
    }

    // ── 둥근 사각형 ──
    {
        Board a(64, 48), b(64, 48);
        Canvas ca = a.cv(), cb = b.cv();
        fill_rrect(ca, 4, 5, 30, 20, 0, RED); fill_rect(cb, 4, 5, 30, 20, RED);
        CHECK("fill_rrect: radius 0 equals fill_rect", a.px == b.px);
        a.clear();
        fill_rrect(ca, 8, 8, 40, 24, 8, RED);
        bool sym = true;
        for (int y = 0; y < 24; ++y) for (int x = 0; x < 40; ++x)
            sym = sym && a.at(8 + x, 8 + y) == a.at(8 + 39 - x, 8 + y) && a.at(8 + x, 8 + y) == a.at(8 + x, 8 + 23 - y);
        CHECK("fill_rrect: symmetric left-right and top-bottom", sym);
        CHECK("fill_rrect: corner pixel untouched, edge midpoints solid, inside solid",
              a.at(8, 8) == BG && a.at(47, 31) == BG && a.at(28, 8) == RED565 && a.at(8, 20) == RED565 && a.at(47, 20) == RED565 && a.at(28, 31) == RED565 && a.at(28, 20) == RED565);
        CHECK("fill_rrect: nothing outside its box", a.at(7, 20) == BG && a.at(48, 20) == BG && a.at(28, 7) == BG && a.at(28, 32) == BG && a.guards_ok());
        const int solid = a.count(RED565), touched = a.changed();
        const double want = 40.0 * 24.0 - (4.0 - 3.14159265) * 8.0 * 8.0;      // 사각형 − 모서리 넷의 빈 곳
        CHECK("fill_rrect: area matches the shape (solid <= area <= touched)", solid <= want + 1 && touched >= want - 1 && touched - solid < 60);
        a.clear();
        fill_rrect(ca, 8, 8, 40, 24, 100, RED);                               // 반지름이 너무 크면 짧은 변의 절반으로
        b.clear();
        fill_rrect(cb, 8, 8, 40, 24, 12, RED);
        CHECK("fill_rrect: radius is clamped to half the short side", a.px == b.px);
        a.clear();
        fill_rrect(ca, -30, -30, 200, 200, 40, RED);
        CHECK("fill_rrect: huge shape is clipped, guards intact", a.guards_ok() && a.count(RED565) == 64 * 48);
        a.clear();
        fill_rrect(ca, 50, 36, 40, 40, 16, RED); fill_rrect(ca, -20, -20, 40, 40, 16, RED);
        CHECK("fill_rrect: shapes crossing the corners stay inside", a.guards_ok());
        a.clear();
        fill_rrect(ca, 4, 4, 20, 20, 6, RED, 128);
        const uint16_t half = blend565(BG, RED565, 128);
        CHECK("fill_rrect: overall alpha blends the inside", a.at(14, 14) == half && a.count(RED565) == 0);
    }

    // ── 원 ──
    {
        Board b(96, 96);
        Canvas cv = b.cv();
        fill_circle(cv, 48, 48, 30, RED);
        const int solid = b.count(RED565), touched = b.changed();
        const double area = 3.14159265 * 30 * 30;
        // 가장자리 한 겹(둘레 약 190px)은 섞이는 픽셀이다: 꽉 찬 픽셀 수 <= 넓이 <= 건드린 픽셀 수, 둘 다 넓이에서 5% 안쪽
        CHECK("fill_circle: area brackets the true area within 5%", solid <= area && touched >= area && (touched - area) / area < 0.05 && (area - solid) / area < 0.05);
        CHECK("fill_circle: centre and rim", b.at(48, 48) == RED565 && b.at(48 - 29, 48) == RED565 && b.at(48 + 28, 47) == RED565 && b.at(48 - 31, 48) == BG && b.at(48, 48 + 30) == BG);
        bool sym = true;
        for (int y = 0; y < 96; ++y) for (int x = 0; x < 96; ++x) sym = sym && b.at(x, y) == b.at(95 - x, y) && b.at(x, y) == b.at(x, 95 - y);
        CHECK("fill_circle: symmetric", sym);
        b.clear();
        fill_circle(cv, 10, 10, 0, RED); fill_circle(cv, 10, 10, -3, RED);
        CHECK("fill_circle: radius 0 or less draws nothing", b.changed() == 0);
        fill_circle(cv, 0, 0, 40, RED); fill_circle(cv, 95, 95, 40, RED);
        CHECK("fill_circle: clipped at the corners", b.guards_ok());
    }

    // ── 글자 마스크 ──
    {
        Board b(16, 8);
        Canvas cv = b.cv();
        const uint8_t mask[6] = { 255, 0, 128, 0, 255, 255 };                 // 3×2
        blit_mask(cv, mask, 3, 2, 4, 3, RED);
        CHECK("blit_mask: opaque, transparent and half pixels", b.at(4, 3) == RED565 && b.at(5, 3) == BG && b.at(6, 3) == blend565(BG, RED565, 128) && b.at(4, 4) == BG && b.at(5, 4) == RED565 && b.at(6, 4) == RED565);
        b.clear();
        blit_mask(cv, mask, 3, 2, -1, -1, RED); blit_mask(cv, mask, 3, 2, 14, 7, RED); blit_mask(cv, mask, 3, 2, 40, 2, RED);
        CHECK("blit_mask: clipped on every side", b.guards_ok() && b.at(0, 0) == RED565 && b.at(1, 0) == RED565 && b.at(14, 7) == RED565 && b.at(15, 7) == BG);
    }

    // ── 색 그림(RLE) ──
    {
        // 4×3: 줄0 = [투명1][빨강2 불투명]        (끝의 투명 1 은 빠져 있다)
        //      줄1 = [파랑1 반투명][초록3 불투명]
        //      줄2 = (빈 줄)
        const uint16_t runs[] = { (1 << 8) | 0, 0, (2 << 8) | 255, 0xF800, (1 << 8) | 128, 0x001F, (3 << 8) | 255, 0x07E0 };
        const uint32_t row[] = { 0, 2, 4, 4 };
        const ui_sprite_t s = { 4, 3, 1, 3, row, runs };
        Board b(12, 8);
        Canvas cv = b.cv();
        blit_rle(cv, s, 2, 1);
        CHECK("blit_rle: runs land where they should", b.at(2, 1) == BG && b.at(3, 1) == 0xF800 && b.at(4, 1) == 0xF800 && b.at(5, 1) == BG
              && b.at(2, 2) == blend565(BG, 0x001F, 128) && b.at(3, 2) == 0x07E0 && b.at(5, 2) == 0x07E0 && b.at(6, 2) == BG && b.changed() == 6);
        b.clear();
        blit_rle(cv, s, 2, 1, true);                                           // 뒤집기: 열 c → 2 + (4 - 1 - c)
        CHECK("blit_rle: mirrored", b.at(5, 1) == BG && b.at(4, 1) == 0xF800 && b.at(3, 1) == 0xF800 && b.at(2, 1) == BG
              && b.at(5, 2) == blend565(BG, 0x001F, 128) && b.at(4, 2) == 0x07E0 && b.at(2, 2) == 0x07E0 && b.changed() == 6);
        b.clear();
        blit_rle(cv, s, 2, 1, false, 128);
        CHECK("blit_rle: overall opacity scales every run", b.at(3, 1) == blend565(BG, 0xF800, 128) && b.at(2, 2) == blend565(BG, 0x001F, 64) && b.at(2, 1) == BG);
        b.clear();
        blit_rle(cv, s, -2, -1); blit_rle(cv, s, 10, 6); blit_rle(cv, s, 10, 6, true); blit_rle(cv, s, -2, -1, true); blit_rle(cv, s, 50, 50); blit_rle(cv, s, -50, 2);
        CHECK("blit_rle: clipped on every side, both directions", b.guards_ok());
        b.clear();
        blit_rle(cv, s, -2, 0);                                                // 줄0 의 빨강 둘(열 1·2) → x = -1, 0
        CHECK("blit_rle: a run cut by the left edge keeps its inside part", b.at(0, 0) == 0xF800 && b.at(1, 0) == BG && b.at(0, 1) == 0x07E0 && b.at(1, 1) == 0x07E0 && b.at(2, 1) == BG);
    }

    // ── 동산 ──
    {
        Board b(8, 10);
        Canvas cv = b.cv();
        const uint16_t top[8] = { 3 * 16, 3 * 16 + 8, 0, 9 * 16 + 15, 10 * 16, 200 * 16, 5 * 16, 5 * 16 };
        fill_below(cv, top, 0, 8, RED);
        CHECK("fill_below: column filled from its top line down", b.at(0, 2) == BG && b.at(0, 3) == RED565 && b.at(0, 9) == RED565);
        CHECK("fill_below: the pixel the line crosses is blended by its cover", b.at(1, 3) == blend565(BG, RED565, 8 * 255 / 16) && b.at(1, 4) == RED565);
        CHECK("fill_below: top 0 fills the whole column, tops at or below the bottom fill nothing",
              b.at(2, 0) == RED565 && b.at(2, 9) == RED565 && b.at(4, 9) == BG && b.at(5, 9) == BG && b.at(3, 8) == BG && b.at(3, 9) != RED565 && b.at(3, 9) != BG);
        CHECK("fill_below: guards intact", b.guards_ok());
        b.clear();
        fill_below(cv, top, 0, 3, RED);
        CHECK("fill_below: only the columns asked for", b.at(2, 5) == RED565 && b.at(6, 9) == BG);
        b.clear();
        fill_below(cv, top, 6, 8, RED, 8);                                    // 열 6·7 만, 8번째 줄 앞까지
        CHECK("fill_below: column range and bottom limit", b.at(6, 5) == RED565 && b.at(7, 7) == RED565 && b.at(6, 8) == BG && b.at(7, 9) == BG && b.at(5, 9) == BG && b.at(0, 9) == BG);
        fill_below(cv, top, -5, 50, RED, 500);
        CHECK("fill_below: out-of-range columns and bottom are clipped", b.guards_ok() && b.at(0, 9) == RED565 && b.at(7, 9) == RED565);
        {
            Board once(8, 10), twice(8, 10);
            Canvas c1 = once.cv(), c2 = twice.cv();
            fill_rect(c1, 0, 0, 8, 10, 0x0000FFu); fill_below(c1, top, 0, 8, RED);
            fill_rect(c2, 0, 0, 8, 10, 0x0000FFu); fill_below(c2, top, 0, 8, RED);
            fill_rect(c2, 0, 0, 8, 10, 0x0000FFu); fill_below(c2, top, 0, 8, RED);     // 바탕을 다시 깔고 그리면 몇 번이든 같다
            CHECK("fill_below: repainting over a fresh background gives the same pixels", once.px == twice.px);
        }
    }

    // ── 굵은 곡선 ──
    {
        Board b(80, 40);
        Canvas cv = b.cv();
        stroke_quad(cv, 10, 10, 40, 30, 70, 10, 4, RED);
        CHECK("stroke_quad: both ends and the sagging middle are inked", b.at(10, 10) == RED565 && b.at(69, 10) == RED565 && (b.at(40, 19) == RED565 || b.at(40, 20) == RED565));
        CHECK("stroke_quad: far from the curve stays clean", b.at(40, 5) == BG && b.at(40, 35) == BG && b.at(5, 30) == BG);
        int col = 0;
        for (int y = 0; y < 40; ++y) col += b.at(40, y) != BG;
        CHECK("stroke_quad: about as thick as asked (4px, edges blended)", col >= 4 && col <= 6);
        stroke_quad(cv, -30, -30, 40, 100, 120, -30, 6, RED);
        CHECK("stroke_quad: clipped, guards intact", b.guards_ok());
    }

    printf("test_raster: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
