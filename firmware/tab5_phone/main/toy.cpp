// toy.cpp — 토이폰 테마의 부품 그리기. 설명은 toy.h, 정본은 docs/design/README.md §5.
// 색 그림(ui_sprites.h — 큰 표)은 이 파일만 포함한다: 다른 파일이 포함하면 쓰는 그림이 번역 단위마다 한 벌씩 더 들어간다.
#include "toy.h"

#include <algorithm>

#include "ui_sprites.h"

namespace toy {
namespace {

// 줄 = Who, 칸 = Pose (둘 다 enum 순서 그대로 — tools/toy_art.py 의 _char 목록과 같은 순서다)
const ui_sprite_t *const CHAR[3][6] = {
    { &spr_bear_wave,   &spr_bear_sleep,   &spr_bear_cup,   &spr_bear_cheer,   &spr_bear_caller,   &spr_bear_hi },
    { &spr_rabbit_wave, &spr_rabbit_sleep, &spr_rabbit_cup, &spr_rabbit_cheer, &spr_rabbit_caller, &spr_rabbit_hi },
    { &spr_robot_wave,  &spr_robot_sleep,  &spr_robot_cup,  &spr_robot_cheer,  &spr_robot_caller,  &spr_robot_hi },
};
const ui_sprite_t *const HEAD[3][2] = { { &spr_head_bear, &spr_head_bear_sleep }, { &spr_head_rabbit, &spr_head_rabbit_sleep }, { &spr_head_robot, &spr_head_robot_sleep } };
static_assert(sizeof CHAR / sizeof CHAR[0] == (size_t)Who::ROBOT + 1 && sizeof CHAR[0] / sizeof CHAR[0][0] == (size_t)Pose::HI + 1 && sizeof HEAD / sizeof HEAD[0] == (size_t)Who::ROBOT + 1,
              "one sprite row per character (Who) and one column per pose (Pose)");
const ui_sprite_t *const CHICK[4] = { &spr_chick_big, &spr_chick_idle, &spr_chick_jump, &spr_chick_hum };
const ui_sprite_t *const ICON[] = { &spr_ico_call, &spr_ico_hang, &spr_ico_speaker, &spr_ico_note, &spr_gloss };
static_assert(sizeof ICON / sizeof ICON[0] == (size_t)Icon::GLOSS + 1, "one sprite per Icon, in enum order");

// 키캡 모서리: 높이에 맞춰 둥글게(96 → 26, 112 → 31, 160 이상 → 44)
int radius_for(int h) { return std::min(44, std::max(18, h * 28 / 100)); }

struct Puff { int16_t x, y; uint8_t pct; };
struct Dot { int16_t x, y; uint8_t r; };
const Puff CLOUDS_IDLE[]     = { { 70, 118, 100 }, { 884, 132, 72 }, { 1104, 70, 86 }, { 316, 204, 56 } };
const Puff CLOUDS_INCOMING[] = { { 210, 84, 80 }, { 1016, 96, 90 } };
const Puff CLOUDS_CALL[]     = { { 96, 104, 100 }, { 900, 76, 80 }, { 372, 48, 60 } };
// 대기(전화기 3대): 상대 둘의 줄(x 248~488 · 792~1032, y 92~560)과 맨 윗줄(안내 띠 y 14~78), 음량 판(x 1072~, y 224~)을 비켜 둔다 — 두 줄 사이의 하늘과 양옆에만
const Puff CLOUDS_PEERS[]    = { { 570, 206, 90 }, { 668, 330, 52 }, { 52, 232, 84 }, { 1104, 96, 80 } };
const Dot FLOWERS_IDLE[]     = { { 312, 664, 6 }, { 352, 690, 5 }, { 792, 642, 6 }, { 866, 688, 5 }, { 1010, 654, 6 }, { 604, 700, 5 } };
const Dot FLOWERS_INCOMING[] = { { 640, 700, 5 }, { 1076, 700, 6 }, { 1250, 640, 5 }, { 206, 704, 5 } };
const Dot FLOWERS_CALL[]     = { { 230, 690, 6 }, { 420, 664, 5 }, { 880, 676, 6 }, { 1000, 702, 5 } };
// 대기(전화기 3대): 통화 키캡 아래의 풀밭에만. Wi-Fi 키캡(x 32~272, y 600~696)·부재중 자리(x 368~776, y 588~)·음량 판을 비켜 둔다
const Dot FLOWERS_PEERS[]    = { { 304, 640, 6 }, { 336, 694, 5 }, { 832, 630, 6 }, { 904, 684, 5 }, { 1016, 650, 6 }, { 800, 704, 5 } };

template <size_t N> void clouds(const Puff (&list)[N]) { for (const Puff &p : list) cloud(p.x, p.y, p.pct); }
template <size_t N> void flowers(const Dot (&list)[N]) { for (const Dot &d : list) gfx::fill_circle(d.x, d.y, d.r, WHITE); }

}  // namespace

void cloud(int x, int y, int pct)
{
    auto s = [pct](int v) { return v * pct / 100; };
    gfx::fill_circle(x + s(34), y + s(50), s(30), CLOUD);
    gfx::fill_circle(x + s(74), y + s(36), s(36), CLOUD);
    gfx::fill_circle(x + s(116), y + s(52), s(28), CLOUD);
    gfx::fill_rrect(x + s(34), y + s(50), s(82), s(30), s(15), CLOUD);
}

void cloud_wide(int cx, int cy)                              // 약 208×68
{
    const int top = cy - 34;
    gfx::fill_circle(cx - 76, top + 42, 26, CLOUD);
    gfx::fill_circle(cx - 30, top + 32, 32, CLOUD);
    gfx::fill_circle(cx + 26, top + 34, 30, CLOUD);
    gfx::fill_circle(cx + 76, top + 44, 24, CLOUD);
    gfx::fill_rrect(cx - 76, top + 40, 152, 28, 14, CLOUD);
}

void background(Scene s)
{
    gfx::clear(SKY);
    if (s == Scene::FLAT) return;
    switch (s) {                                             // 구름은 동산 뒤
    case Scene::IDLE:     clouds(CLOUDS_IDLE); break;
    case Scene::INCOMING: clouds(CLOUDS_INCOMING); cloud_wide(SCREEN_W / 2, 182); break;
    case Scene::CALL:     clouds(CLOUDS_CALL); break;
    case Scene::PEERS:    clouds(CLOUDS_PEERS); break;
    case Scene::HILL:
    case Scene::FLAT:     break;
    }
    gfx::fill_below(HILL_TOP_Q4, HILL);
    switch (s) {
    case Scene::IDLE:     flowers(FLOWERS_IDLE); break;
    case Scene::INCOMING: flowers(FLOWERS_INCOMING); break;
    case Scene::CALL:     flowers(FLOWERS_CALL); break;
    case Scene::PEERS:    flowers(FLOWERS_PEERS); break;
    case Scene::HILL:
    case Scene::FLAT:     break;
    }
}

void background_patch(const Rect &r)
{
    gfx::fill_rect(r.x, r.y, r.w, r.h, SKY);
    gfx::fill_below(HILL_TOP_Q4, HILL, r.x, r.x + r.w, r.y + r.h);
}

// 테두리 색의 둥근 사각형 한 장(= 윗면 테두리 + 아래 그림자가 합쳐진 모양) 위에 윗면을 SHADOW 만큼 짧게 얹는다.
void keycap(const Rect &r, uint32_t face, int radius)
{
    gfx::fill_rrect(r.x, r.y, r.w, r.h, radius, INK);
    gfx::fill_rrect(r.x + OUTLINE, r.y + OUTLINE, r.w - 2 * OUTLINE, r.h - SHADOW - 2 * OUTLINE, std::max(0, radius - OUTLINE), face);
}

int keycap_cy(const Rect &r) { return r.y + (r.h - SHADOW) / 2; }

void gloss(const Rect &r)
{
    const int inset = r.w >= 300 ? 28 : 18, h = r.w >= 300 ? 14 : 12;
    gfx::fill_alpha_rrect(r.x + inset, r.y + OUTLINE + 6, r.w - 2 * inset, h, h / 2, WHITE, 110);
}

void button(const Rect &r, const ui_asset_t *label, bool enabled, uint32_t face, const ui_sprite_t *icon)
{
    const int radius = radius_for(r.h), cx = r.x + r.w / 2, cy = keycap_cy(r);
    if (!enabled) {                                          // 그림자 없는 납작한 판 = 눌리지 않는다
        gfx::fill_rrect(r.x, r.y, r.w, r.h - SHADOW, radius, PALE);
        gfx::blit_asset_center(label, cx, cy, MUTED);
        return;
    }
    keycap(r, face, radius);
    if (face != WHITE && r.h >= 150) gloss(r);               // 색이 있는 큰 키캡에만 광택 줄
    const int gap = 18, lw = label ? label->w : 0, total = (icon ? icon->w + gap : 0) + lw;
    int x = cx - total / 2;
    if (icon) { gfx::blit_sprite(icon, x, cy - icon->h / 2); x += icon->w + gap; }
    if (label) gfx::blit_asset(label, x, cy - label->h / 2, INK);
}

// 원 둘(윗면과 그림자)을 세로로 어긋나게 합친 모양 = 알약. 윗면 원은 그 위쪽에 얹는다.
void round_button(int cx, int cy, int r, uint32_t face)
{
    const int rr = r - ROUND_DROP / 2;                       // 테두리 바깥 반지름
    gfx::fill_rrect(cx - rr, cy - r, 2 * rr, 2 * r, rr, INK);
    gfx::fill_circle(cx, round_face_cy(cy), round_face_r(r), face);
}

void pill(int cx, int cy, int w, int h, uint32_t fill)
{
    gfx::fill_rrect(cx - w / 2, cy - h / 2, w, h, h / 2, INK);
    gfx::fill_rrect(cx - w / 2 + PILL_OUTLINE, cy - h / 2 + PILL_OUTLINE, w - 2 * PILL_OUTLINE, h - 2 * PILL_OUTLINE, h / 2 - PILL_OUTLINE, fill);
}

void panel(const Rect &r, uint32_t fill, int radius)
{
    gfx::fill_rrect(r.x, r.y, r.w, r.h, radius, INK);
    gfx::fill_rrect(r.x + OUTLINE, r.y + OUTLINE, r.w - 2 * OUTLINE, r.h - 2 * OUTLINE, std::max(0, radius - OUTLINE), fill);
}

int label_pill_w(const ui_asset_t *label, int h) { return label ? label->w + h + 2 * PILL_OUTLINE : 0; }

void label_pill(const ui_asset_t *label, int cx, int cy, int h, uint32_t fill, uint32_t fg)
{
    if (!label) return;
    pill(cx, cy, label_pill_w(label, h), h, fill);
    gfx::blit_asset_center(label, cx, cy, fg);
}

Who who_of_unit(int unit) { return unit == 2 ? Who::RABBIT : unit == 3 ? Who::ROBOT : Who::BEAR; }

void character(Who who, Pose pose, int axis_x, int base_y, bool mirror)
{
    gfx::blit_sprite_at(CHAR[(int)who][(int)pose], axis_x, base_y, mirror);
}

Rect character_box(Who who, Pose pose, int axis_x, int base_y, bool mirror)       // gfx::blit_sprite_at 과 같은 계산
{
    const ui_sprite_t *s = CHAR[(int)who][(int)pose];
    return Rect{ mirror ? axis_x - (s->w - 1 - s->ax) : axis_x - s->ax, base_y - s->by, s->w, s->h };
}

int head_w(Who who) { return HEAD[(int)who][0]->w; }

int head(Who who, bool asleep, int x, int cy)
{
    const ui_sprite_t *s = HEAD[(int)who][asleep ? 1 : 0];
    gfx::blit_sprite(s, x, cy - s->h / 2);
    return s->w;
}

void chick(Chick pose, int axis_x, int base_y) { gfx::blit_sprite_at(CHICK[(int)pose], axis_x, base_y); }

const ui_sprite_t *icon_sprite(Icon i) { return ICON[(int)i]; }
void icon(Icon i, int cx, int cy, uint8_t opacity) { gfx::blit_sprite_center(ICON[(int)i], cx, cy, opacity); }

}  // namespace toy
