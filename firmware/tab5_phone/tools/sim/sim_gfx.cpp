// sim_gfx.cpp — gfx.h 의 기기 쪽(gfx.cpp)을 대신하는 맥 구현. 합성 버퍼는 메모리 한 장, present·스크린샷은 하는 일이 없다.
// 도형·글자 자산·색 그림은 기기와 같은 main/gfx_draw.cpp(raster.h)가 그린다 → 픽셀이 기기와 같다.
// 다른 것은 내장 글꼴(efontKR_16_b) 글자뿐이다: 폭은 그 글꼴의 칸 폭(ASCII 8px, 한글 등 16px × 배율)으로 계산하고,
// 그림에는 render_screens.py 가 비슷한 글꼴로 얹는다(자리·폭은 맞고 글자 모양만 다르다).
#include <cstdio>

#include "gfx.h"
#include "sim.h"

namespace {

uint16_t s_px[SCREEN_W * SCREEN_H];
raster::Canvas s_cv = { s_px, SCREEN_W, SCREEN_H };

}  // namespace

namespace sim {

std::vector<Text> texts;
bool is_private = false;

const uint16_t *pixels() { return s_px; }

bool save(const std::string &base)
{
    FILE *f = fopen((base + ".rgb565").c_str(), "wb");
    if (!f) return false;
    const bool ok = fwrite(s_px, sizeof s_px, 1, f) == 1;
    fclose(f);
    f = fopen((base + ".txt").c_str(), "w");
    if (!f) return false;
    if (is_private) fprintf(f, "private\n");
    for (const Text &t : texts) fprintf(f, "%s\t%d\t%d\t%.2f\t%06x\t%s\n", t.center ? "center" : "left", t.x, t.y, t.size, (unsigned)t.rgb, t.s.c_str());
    fclose(f);
    return ok;
}

}  // namespace sim

namespace gfx {

bool init() { return true; }
void present() {}
void set_private(bool on) { sim::is_private = on; }
size_t screenshot_jpeg(const uint8_t **out, bool *is_private) { if (out) *out = nullptr; if (is_private) *is_private = sim::is_private; return 0; }

raster::Canvas *detail::target() { return &s_cv; }
void detail::frame_begin() { sim::texts.clear(); sim::is_private = false; }

int text_w(const char *s, float size)
{
    if (!s) return 0;
    int cells = 0;                                                // 8px 칸의 수
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        if ((*p & 0xC0) == 0x80) continue;                        // 여러 바이트 글자의 뒷바이트
        cells += *p < 0x80 ? 1 : 2;
    }
    return (int)((float)(cells * 8) * size);
}

int text(const char *s, int x, int y, uint32_t rgb, float size)
{
    if (!s) return 0;
    sim::texts.push_back({ s, x, y, rgb, size, false });
    return text_w(s, size);
}

void text_center(const char *s, int cx, int cy, uint32_t rgb, float size)
{
    if (s) sim::texts.push_back({ s, cx, cy, rgb, size, true });
}

}  // namespace gfx
