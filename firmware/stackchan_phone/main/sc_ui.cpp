// sc_ui.cpp — 설명은 sc_ui.h. 디자인 규칙 docs/design/README.md §5.10 (토이폰 테마의 스택짱 화면).
//   규칙(Tab5 와 같다): 누르는 것은 키캡(굵은 테두리 + 아래로 두꺼운 그림자, 그림자까지 터치 영역 안), 보여 주기만 하는 것은 납작한 알약.
//   SU-D1 테두리 3 · 그림자 5 · 알약 테두리 2 (Tab5 는 5 · 8 · 4). 이유: 화소 간격이 Tab5(0.085mm)의 1.5배라 같은 굵기로 보이려면 0.67배. 탈출구: 아래 세 상수.
//   SU-D2 바탕은 하늘 + 화면 아래의 낮은 동산(원호) + 구름 둘. 동산은 버튼 뒤에 깔린다 — 화면마다 같은 자리라 조각만 다시 그릴 때 그대로 다시 깔 수 있다(sky_patch·band).
//   SU-D3 받기·거절은 색만으로 가르지 않는다: 자리(왼쪽 = 받기) + 수화기 그림(든 것 / 엎은 것) + 글자.
#include "sc_ui.h"

#include <algorithm>

#include "sc_assets.h"
#include "sc_sprites.h"
#include "toy_colors.h"

namespace scui {
namespace {

using raster::Canvas;
using sclayout::Model;
using sclayout::Rect;
using sclayout::Screen;
using namespace toy;

constexpr int OUTLINE = 3, SHADOW = 5, PILL_OUTLINE = 2;                 // SU-D1
constexpr int HILL_CX = 160, HILL_CY = 620, HILL_R = 424;                // SU-D2: 가운데 윗선 y = 196, 양 끝 y ≈ 227

struct Cast { const ui_sprite_t *wave, *cup, *caller, *head, *head_sleep, *tag; const sc_mask_t *name, *num; };
const Cast CAST[3] = {
    { &sc_spr_bear_wave,   &sc_spr_bear_cup,   &sc_spr_bear_caller,   &sc_spr_bear_head,   &sc_spr_bear_head_sleep,   &sc_spr_bear_tag,   &sc_lbl_unit1, &sc_lbl_p1 },
    { &sc_spr_rabbit_wave, &sc_spr_rabbit_cup, &sc_spr_rabbit_caller, &sc_spr_rabbit_head, &sc_spr_rabbit_head_sleep, &sc_spr_rabbit_tag, &sc_lbl_unit2, &sc_lbl_p2 },
    { &sc_spr_robot_wave,  &sc_spr_robot_cup,  &sc_spr_robot_caller,  &sc_spr_robot_head,  &sc_spr_robot_head_sleep,  &sc_spr_robot_tag,  &sc_lbl_unit3, &sc_lbl_p3 },
};
const Cast *cast_of(int unit) { return unit >= 1 && unit <= 3 ? &CAST[unit - 1] : nullptr; }

// ── 부품 ────────────────────────────────────────────────────────────────────
void mask(const Canvas &cv, const sc_mask_t *m, int x, int y, uint32_t rgb) { if (m) raster::blit_mask(cv, m->a, m->w, m->h, x, y, rgb); }
void mask_center(const Canvas &cv, const sc_mask_t *m, int cx, int cy, uint32_t rgb) { if (m) mask(cv, m, cx - m->w / 2, cy - m->h / 2, rgb); }
void sprite(const Canvas &cv, const ui_sprite_t *s, int x, int y, bool mirror = false) { if (s) raster::blit_rle(cv, *s, x, y, mirror); }
void sprite_at(const Canvas &cv, const ui_sprite_t *s, int axis_x, int base_y) { if (s) sprite(cv, s, axis_x - s->ax, base_y - s->by); }

void cloud(const Canvas &cv, int x, int y, int pct)                      // Tab5 toy::cloud 와 같은 모양(100% = 약 148×80)
{
    auto s = [pct](int v) { return v * pct / 100; };
    raster::fill_circle(cv, x + s(34), y + s(50), s(30), CLOUD);
    raster::fill_circle(cv, x + s(74), y + s(36), s(36), CLOUD);
    raster::fill_circle(cv, x + s(116), y + s(52), s(28), CLOUD);
    raster::fill_rrect(cv, x + s(34), y + s(50), s(82), s(30), s(15), CLOUD);
}

// 바탕. 하늘 + (구름) + 동산. 구름의 자리는 화면의 구성마다 다르다 — 다시 그리는 조각(시계·음량 표시)과 겹치지 않게 둔다.
enum class Sky : uint8_t { PLAIN, MID, CALL };       // PLAIN = 구름 없음(대기) / MID = 캐릭터가 가운데(연결 중·거는 중·전화 옴) / CALL = 캐릭터가 왼쪽(통화 중)
void background(const Canvas &cv, Sky sky)
{
    raster::fill(cv, SKY);
    if (sky == Sky::MID) { cloud(cv, 8, 58, 36); cloud(cv, 258, 20, 36); }    // 왼쪽 가운데·오른쪽 위 — 위 띠의 이름표(왼쪽 위)와 가운데 캐릭터 밖
    if (sky == Sky::CALL) { cloud(cv, 2, 0, 26); cloud(cv, 244, 98, 38); }   // 시계(y 24~96)·음량 표시(y 132~152) 조각 밖
    raster::fill_circle(cv, HILL_CX, HILL_CY, HILL_R, HILL);
}
// 조각만 다시 그릴 때 먼저 바탕을 다시 깐다(둥근 모서리의 섞이는 픽셀이 거듭 그려져 진해지지 않게).
//   sky_patch: 동산에 닿지 않는 조각(하늘만). band: 화면 폭 전체의 가로 띠(하늘 + 동산) — 동산의 곡선은 가로로 자를 수 없어 띠째로 다시 깐다.
void sky_patch(const Canvas &cv, const Rect &r) { raster::fill_rect(cv, r.x, r.y, r.w, r.h, SKY); }
void band(const Canvas &cv, int y, int h)
{
    raster::fill_rect(cv, 0, y, cv.w, h, SKY);
    const int y0 = std::max(0, y), y1 = std::min(cv.h, y + h);
    if (y1 <= y0) return;
    const Canvas rows = { cv.px + (size_t)y0 * cv.w, cv.w, y1 - y0 };       // 그 줄들만 가진 캔버스 = 세로 자르기
    raster::fill_circle(rows, HILL_CX, HILL_CY - y0, HILL_R, HILL);
}
static_assert(sclayout::CLOCK_AREA.y + sclayout::CLOCK_AREA.h <= HILL_CY - HILL_R && sclayout::VOL_PIPS.y + sclayout::VOL_PIPS.h <= HILL_CY - HILL_R,
              "the clock and volume patches are repainted with sky only - they must stay above the hill");

void keycap(const Canvas &cv, const Rect &r, uint32_t face, int radius)  // 테두리·그림자·윗면 전부 r 안
{
    raster::fill_rrect(cv, r.x, r.y, r.w, r.h, radius, INK);
    raster::fill_rrect(cv, r.x + OUTLINE, r.y + OUTLINE, r.w - 2 * OUTLINE, r.h - SHADOW - 2 * OUTLINE, std::max(0, radius - OUTLINE), face);
}
int keycap_cy(const Rect &r) { return r.y + (r.h - SHADOW) / 2; }
void flat(const Canvas &cv, const Rect &r, int radius) { raster::fill_rrect(cv, r.x, r.y, r.w, r.h - SHADOW, radius, PALE); }   // 눌리지 않는 판(그림자 없음)

void pill(const Canvas &cv, int cx, int cy, int w, int h, uint32_t fill)
{
    raster::fill_rrect(cv, cx - w / 2, cy - h / 2, w, h, h / 2, INK);
    raster::fill_rrect(cv, cx - w / 2 + PILL_OUTLINE, cy - h / 2 + PILL_OUTLINE, w - 2 * PILL_OUTLINE, h - 2 * PILL_OUTLINE, h / 2 - PILL_OUTLINE, fill);
}

// 그림(왼쪽) + 글자 한 줄을 가운데 맞춰 놓는다.
void icon_label(const Canvas &cv, const ui_sprite_t *icon, const sc_mask_t *label, int cx, int cy, uint32_t fg, int gap = 8)
{
    const int total = (icon ? icon->w + gap : 0) + (label ? label->w : 0);
    int x = cx - total / 2;
    if (icon) { sprite(cv, icon, x, cy - icon->h / 2); x += icon->w + gap; }
    if (label) mask(cv, label, x, cy - label->h / 2, fg);
}

void button(const Canvas &cv, const Rect &r, uint32_t face, const ui_sprite_t *icon, const sc_mask_t *label)
{
    keycap(cv, r, face, 18);
    icon_label(cv, icon, label, r.x + r.w / 2, keycap_cy(r), INK);
}

int number_w(int n)
{
    int w = 0;
    do { w += SC_DIG_SMALL[n % 10]->w + 1; n /= 10; } while (n);
    return w - 1;
}
void number(const Canvas &cv, int n, int x, int cy, uint32_t rgb)        // 왼쪽 기준, 작은 숫자
{
    char d[6]; int k = 0;
    do { d[k++] = (char)(n % 10); n /= 10; } while (n && k < 6);
    while (k--) { const sc_mask_t *g = SC_DIG_SMALL[(int)d[k]]; mask(cv, g, x, cy - g->h / 2, rgb); x += g->w + 1; }
}

const sc_mask_t *notice_label(callfsm::Notice n, uint8_t msg)
{
    using callfsm::Notice;
    switch (n) {
    case Notice::PEER_OFF:     return &sc_lbl_n_peer_off;
    case Notice::NO_ANSWER:    return &sc_lbl_n_no_answer;
    case Notice::REJECTED:     return &sc_lbl_n_rejected;
    case Notice::LOST:         return &sc_lbl_n_lost;
    case Notice::CALL_ENDED:   return &sc_lbl_n_ended;
    case Notice::MISSED:       return &sc_lbl_n_missed;
    case Notice::RELAY_SILENT: return &sc_lbl_n_relay_silent;
    case Notice::BUSY:         return &sc_lbl_n_busy;
    case Notice::REJECT_MSG:   return msg == 1 ? &sc_lbl_n_msg1 : msg == 2 ? &sc_lbl_n_msg2 : msg == 3 ? &sc_lbl_n_msg3 : &sc_lbl_n_rejected;
    case Notice::NONE:         break;
    }
    return nullptr;
}

const sc_mask_t *link_label(sclayout::Link l)
{
    using sclayout::Link;
    switch (l) {
    case Link::WIFI_STARTING:    return &sc_lbl_wifi_starting;
    case Link::WIFI_SEARCHING:   return &sc_lbl_wifi_searching;
    case Link::WIFI_NONE:        return &sc_lbl_wifi_none;
    case Link::WIFI_FAIL:        return &sc_lbl_wifi_fail;
    case Link::RELAY_CONNECTING: return &sc_lbl_relay_connecting;
    case Link::RELAY_RETRY:      return &sc_lbl_relay_retry;
    case Link::AUTH_FAIL:        return &sc_lbl_auth_fail;
    case Link::UNREGISTERED:     return &sc_lbl_unregistered;
    case Link::UP:               return &sc_lbl_relay_connecting;        // 붙었고 대기로 넘어가기 직전의 한 바퀴
    }
    return nullptr;
}

// ── 화면의 조각들 ────────────────────────────────────────────────────────────
// 위 띠: 안내가 있으면 안내(노란 알약, 화면 폭), 없으면 내 이름표(작은 얼굴 + "N번 전화기").
void top_strip(const Canvas &cv, const Model &m)
{
    if (const sc_mask_t *n = notice_label(m.notice, m.notice_msg)) {
        pill(cv, sclayout::W / 2, 22, std::min(sclayout::W - 8, n->w + 36), 34, WARN);
        mask_center(cv, n, sclayout::W / 2, 22, INK);
        return;
    }
    const Cast *me = cast_of(m.unit);
    if (!me) return;
    const int w = me->tag->w + 6 + me->name->w + 22;
    pill(cv, 6 + w / 2, 22, w, 34, WHITE);
    sprite(cv, me->tag, 6 + 9, 22 - me->tag->h / 2);
    mask(cv, me->name, 6 + 9 + me->tag->w + 6, 22 - me->name->h / 2, INK);
}

void deck_key(const Canvas &cv, const Model &m)
{
    if (!m.deck_ok) { flat(cv, sclayout::BTN_DECK, 16); mask_center(cv, &sc_lbl_deck, sclayout::BTN_DECK.x + sclayout::BTN_DECK.w / 2, keycap_cy(sclayout::BTN_DECK), MUTED); return; }
    keycap(cv, sclayout::BTN_DECK, m.deck_armed ? WARN : WHITE, 16);
    mask_center(cv, m.deck_armed ? &sc_lbl_deck_again : &sc_lbl_deck, sclayout::BTN_DECK.x + sclayout::BTN_DECK.w / 2, keycap_cy(sclayout::BTN_DECK), INK);
}

// 대기 화면의 통화 키캡 하나: 위 = 상대 얼굴(흰 동그라미 안), 아래 = 수화기 + "N번". 켜져 있으면 초록, 꺼져 있으면 옅은 색에 잠든 얼굴.
void call_key(const Canvas &cv, const Model &m, int slot)
{
    const Cast *c = cast_of(m.peer_unit[slot]);
    if (!c) return;
    const Rect &r = sclayout::BTN_CALL[slot];
    const bool on = m.peer_online[slot];
    keycap(cv, r, on ? GO : PALE, 22);
    const int cx = r.x + r.w / 2;
    raster::fill_circle(cv, cx, r.y + 46, 37, INK);
    raster::fill_circle(cv, cx, r.y + 46, 37 - OUTLINE, WHITE);
    const ui_sprite_t *face = on ? c->head : c->head_sleep;
    sprite(cv, face, cx - face->ax, r.y + 46 - face->h / 2 - 1);
    icon_label(cv, &sc_spr_ico_call, c->num, cx, r.y + 102, on ? INK : MUTED, 6);
}

void missed_pill(const Canvas &cv, const Model &m)
{
    if (!m.missed) return;
    const int n = m.missed > 99 ? 99 : m.missed;
    const int w = sc_lbl_missed.w + 6 + number_w(n) + 2 + sc_lbl_count_unit.w;
    const int cx = sclayout::MISSED_AREA.x + sclayout::MISSED_AREA.w / 2, cy = sclayout::MISSED_AREA.y + sclayout::MISSED_AREA.h / 2 - 2;
    pill(cv, cx, cy, w + 26, 36, WARN);
    int x = cx - w / 2;
    mask(cv, &sc_lbl_missed, x, cy - sc_lbl_missed.h / 2, INK); x += sc_lbl_missed.w + 6;
    number(cv, n, x, cy, INK); x += number_w(n) + 2;
    mask(cv, &sc_lbl_count_unit, x, cy - sc_lbl_count_unit.h / 2, INK);
}

void clock_face(const Canvas &cv, const Model &m)
{
    const int mm = sclayout::clock_min(m.talk_seconds), ss = sclayout::clock_sec(m.talk_seconds);
    const int cell = 32, colon = sc_lbl_colon.w + 6;                     // 숫자는 같은 폭의 칸에 넣는다(초가 바뀌어도 흔들리지 않게)
    const Rect &a = sclayout::CLOCK_AREA;
    int x = a.x + (a.w - (4 * cell + colon)) / 2;
    const int cy = a.y + a.h / 2;
    const int digits[4] = { mm / 10, mm % 10, ss / 10, ss % 10 };
    for (int i = 0; i < 4; ++i) {
        if (i == 2) { mask_center(cv, &sc_lbl_colon, x + colon / 2, cy, INK); x += colon; }
        const sc_mask_t *g = SC_DIG_BIG[digits[i]];
        mask(cv, g, x + (cell - g->w) / 2, cy - g->h / 2, INK);
        x += cell;
    }
}

void vol_pips(const Canvas &cv, const Model &m)
{
    const Rect &a = sclayout::VOL_PIPS;
    const int n = volsteps::COUNT, gap = 6, w = (a.w - (n - 1) * gap) / n;
    for (int i = 0; i < n; ++i) {
        const int x = a.x + i * (w + gap);
        raster::fill_rrect(cv, x, a.y, w, a.h, 7, INK);
        raster::fill_rrect(cv, x + PILL_OUTLINE, a.y + PILL_OUTLINE, w - 2 * PILL_OUTLINE, a.h - 2 * PILL_OUTLINE, 5, i < m.vol_step ? VOL : PALE);
    }
}

void call_buttons(const Canvas &cv, const Model &m)
{
    const bool lo = m.vol_step <= volsteps::STEP_MIN, hi = m.vol_step >= volsteps::STEP_MAX;   // 끝에서는 흐린 글자(더 못 간다) — 키캡은 그대로(눌러도 무해)
    keycap(cv, sclayout::BTN_VOL_DN, WARN, 18);
    mask_center(cv, &sc_lbl_vol_dn, sclayout::BTN_VOL_DN.x + sclayout::BTN_VOL_DN.w / 2, keycap_cy(sclayout::BTN_VOL_DN), lo ? MUTED : INK);
    button(cv, sclayout::BTN_HANGUP, STOP, &sc_spr_ico_hang, &sc_lbl_hangup);
    keycap(cv, sclayout::BTN_VOL_UP, WARN, 18);
    mask_center(cv, &sc_lbl_vol_up, sclayout::BTN_VOL_UP.x + sclayout::BTN_VOL_UP.w / 2, keycap_cy(sclayout::BTN_VOL_UP), hi ? MUTED : INK);
}

const Cast *peer_cast(const Model &m)
{
    return m.call_peer < sclayout::MAX_PEERS ? cast_of(m.peer_unit[m.call_peer]) : nullptr;
}

}  // namespace

void draw(const Canvas &cv, const Model &m)
{
    const Cast *peer = peer_cast(m), *me = cast_of(m.unit);
    switch (sclayout::screen_for(m)) {
    case Screen::CONNECTING:
        background(cv, Sky::MID);
        top_strip(cv, m);
        sprite_at(cv, &sc_spr_chick, sclayout::CHAR_AXIS_MID, sclayout::CHAR_BASE_MID + 16);
        mask_center(cv, link_label(m.unit ? m.link : sclayout::Link::UNREGISTERED), sclayout::W / 2, sclayout::TEXT_MID_Y + 20, INK);
        deck_key(cv, m);
        break;
    case Screen::IDLE:
        background(cv, Sky::PLAIN);
        top_strip(cv, m);
        call_key(cv, m, 0);
        call_key(cv, m, 1);
        deck_key(cv, m);
        missed_pill(cv, m);
        break;
    case Screen::DIALING:
        background(cv, Sky::MID);
        if (peer) sprite_at(cv, peer->cup, sclayout::CHAR_AXIS_MID, sclayout::CHAR_BASE_MID);
        mask_center(cv, m.fsm == callfsm::State::RINGBACK ? &sc_lbl_ringback : &sc_lbl_dialing, sclayout::W / 2, sclayout::TEXT_MID_Y, INK);
        button(cv, sclayout::BTN_CANCEL, STOP, &sc_spr_ico_hang, &sc_lbl_cancel);
        break;
    case Screen::INCOMING:
        background(cv, Sky::MID);
        if (peer) sprite_at(cv, peer->caller, sclayout::CHAR_AXIS_MID, sclayout::CHAR_BASE_MID - 4);
        mask_center(cv, &sc_lbl_incoming, sclayout::W / 2, sclayout::TEXT_MID_Y - 2, INK);
        button(cv, sclayout::BTN_ACCEPT, GO, &sc_spr_ico_call, &sc_lbl_accept);     // SU-D3
        button(cv, sclayout::BTN_REJECT, STOP, &sc_spr_ico_hang, &sc_lbl_reject);
        break;
    case Screen::ANSWERING:
        background(cv, Sky::CALL);
        if (peer) sprite_at(cv, peer->cup, sclayout::CHAR_AXIS_CALL, sclayout::CHAR_BASE_CALL);
        mask_center(cv, &sc_lbl_answering, sclayout::CLOCK_AREA.x + sclayout::CLOCK_AREA.w / 2, sclayout::CLOCK_AREA.y + sclayout::CLOCK_AREA.h / 2, INK);
        vol_pips(cv, m);
        call_buttons(cv, m);
        break;
    case Screen::IN_CALL:
        background(cv, Sky::CALL);
        if (peer) sprite_at(cv, peer->cup, sclayout::CHAR_AXIS_CALL, sclayout::CHAR_BASE_CALL);
        clock_face(cv, m);
        vol_pips(cv, m);
        call_buttons(cv, m);
        break;
    case Screen::ENDING:
        background(cv, Sky::MID);
        if (peer) sprite_at(cv, peer->wave, sclayout::CHAR_AXIS_MID, sclayout::CHAR_BASE_MID);
        mask_center(cv, &sc_lbl_ending, sclayout::W / 2, sclayout::TEXT_MID_Y + 6, INK);
        break;
    case Screen::TO_DECK:
        background(cv, Sky::MID);
        if (me) sprite_at(cv, me->wave, sclayout::CHAR_AXIS_MID, sclayout::CHAR_BASE_MID + 12);
        mask_center(cv, &sc_lbl_to_deck, sclayout::W / 2, sclayout::TEXT_MID_Y + 22, INK);
        break;
    }
}

void draw_clock(const Canvas &cv, const Model &m)
{
    sky_patch(cv, sclayout::CLOCK_AREA);
    clock_face(cv, m);
}

void draw_volume(const Canvas &cv, const Model &m)
{
    sky_patch(cv, sclayout::VOL_PIPS);
    band(cv, sclayout::VOLUME_BAND_Y, sclayout::H - sclayout::VOLUME_BAND_Y);   // 버튼 줄은 띠째로(끝 단계의 흐린 글자가 함께 바뀐다)
    vol_pips(cv, m);
    call_buttons(cv, m);
}

}  // namespace scui
