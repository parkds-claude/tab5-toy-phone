// ui.cpp — 통화 화면 그리기. 배치·판정은 ui_layout.h(맥에서 시험), 여기는 그리기만 한다.
// 설계서 §10, docs/design/README.md §5 토이폰 테마(2026-10-08): 하늘과 동산 위에 캐릭터가 서 있고, 누르는 것은 키캡이다.
//   - 곰 = 1번 전화기, 토끼 = 2번 전화기, 로봇 = 3번 전화기(스택짱). 번호 ↔ 캐릭터는 toy::who_of_unit 한 곳에서 정한다 — 이 파일은 번호만 넘긴다.
//   - 내 캐릭터는 왼쪽, 상대는 오른쪽(오른쪽은 좌우를 뒤집어 그린다). 거는 중·전화 옴·통화 중의 "상대"는 그 통화의 상대다(call_unit — call_fsm 의 call_peer).
//   - 대기 화면(전화기 3대 — docs/exec-plans/2026-10-08-phone-3way-design.md §9, ui_layout.h UL-D4): 내 캐릭터는 서지 않고 왼쪽 위 이름표로만 남는다.
//     상대 둘이 나란히 선다 — 각자 위에서부터 이름 알약 · 구름 위의 캐릭터 · 둥근 통화 키캡. 왼쪽이 slot 0(번호가 작은 쪽).
//   - 캐릭터가 상태를 보여 준다: 상대가 꺼지면 잠들고, 통화가 되면 둘이 종이컵 전화를 줄로 잇는다. 병아리는 알림(부재중)이 있을 때 나온다.
//   - 버튼 자리는 ui_layout.h 그대로다. 키캡은 그림자까지 그 Rect 안에 그린다(toy.h 규칙 ②) → 터치 판정·시험이 그대로다.
//   - 아래 "자리" 상수는 그리기 전용이다(터치와 무관). 캐릭터는 어느 화면에서도 버튼·음량 칸과 겹치지 않게 잡았다 — 화면 미리보기(tools/render_screens.py)로 확인한다.
#include "ui.h"

#include <cstdio>
#include <cstring>

#include "gfx.h"
#include "toy.h"
#include "wifi_store.h"

using namespace uilayout;
using callfsm::Notice;
using toy::Chick;
using toy::Icon;
using toy::Pose;
using toy::Scene;

namespace {

const int CX = SCREEN_W / 2;

// 자리(대칭축 x, 바닥 y)
const int PEER_X = 920, PEER_BASE = 586;                          // 거는 중: 상대(동산 위, 손을 흔든다)
const int TALK_ME_X = 320, TALK_PEER_X = 960, TALK_BASE = 590;    // 종이컵을 든 둘(통화 시계의 양옆 — 귀와 시계 15px, 오른쪽 팔과 음량 판 14px)
// 대기 화면의 상대 줄: 가로 가운데는 그 상대의 통화 키캡(BTN_CALLS[slot].cx), 칸은 ui_layout.h 의 PEER_NAME(이름 알약)·PEER_BODY(캐릭터와 구름).
//   캐릭터의 발밑은 PEER_FEET_Y(ui_layout.h). 그림이 칸을 넘지 않는지는 화면 미리보기(tools/sim/sim_main.cpp check_peer_cells)가 캐릭터·자세마다 본다.
const int PEER_CLOUD_CY = PEER_FEET_Y - 12;                       // 발밑의 구름(약 208×68 → y 306~374): 캐릭터(폭 185)보다 넓어 양옆으로 보인다. 통화 키캡(y 384~)과 10px
static_assert(PEER_CLOUD_CY + 34 <= PEER_BODY[0].y + PEER_BODY[0].h && PEER_BODY[0].w >= 208 && PEER_BODY[0].y == PEER_BODY[1].y && PEER_BODY[0].h == PEER_BODY[1].h,
              "the peer's cloud stays inside the body cell");
constexpr Rect VOL_PANEL = { 1072, 224, 192, 472 };                   // 음량 칸을 담는 판(VOL_AREA 보다 사방 16px 크다)
static_assert(rect_holds(VOL_PANEL, VOL_AREA) && rect_holds(Rect{ 0, 0, SCREEN_W, SCREEN_H }, VOL_PANEL), "the volume panel holds the volume cell and is inside the screen");
// 판은 음량 칸이 뜨는 화면(대기·통화)의 어느 버튼과도, 대기 화면의 상대 줄·부재중 자리와도 겹치지 않는다 → 판만 다시 그려도 다른 것이 상하지 않는다
static_assert(!rects_overlap(VOL_PANEL, circle_box(BTN_CALL)) && !rects_overlap(VOL_PANEL, circle_box(BTN_CALL_B)) && !rects_overlap(VOL_PANEL, BTN_WIFI) && !rects_overlap(VOL_PANEL, BTN_HANGUP)
              && !rects_overlap(VOL_PANEL, PEER_NAME[1]) && !rects_overlap(VOL_PANEL, PEER_BODY[1]) && !rects_overlap(VOL_PANEL, MISSED_AREA) && !rects_overlap(VOL_PANEL, NOTICE_BAND),
              "the volume panel overlaps no button of the idle and in-call screens, nor the peer columns");

// 거절 문구(순서 = 문구 번호 1·2·3). 문구 자체는 tools/gen_ui_assets.py 의 REJECT_MSGS 한 곳에만 적는다(DM-D13).
// 크기를 적지 않은 배열 + static_assert: 문구 수가 callfsm::MSG_COUNT 와 어긋나면 빌드가 실패한다.
const ui_asset_t *const MSG_LABEL[]  = { &lbl_msg1, &lbl_msg2, &lbl_msg3 };          // 메시지 고르기 화면의 버튼 글자
const ui_asset_t *const MSG_NOTICE[] = { &lbl_n_msg1, &lbl_n_msg2, &lbl_n_msg3 };    // 거는 쪽 안내 상자("상대: …")
static_assert(sizeof MSG_LABEL / sizeof MSG_LABEL[0] == callfsm::MSG_COUNT && sizeof MSG_NOTICE / sizeof MSG_NOTICE[0] == callfsm::MSG_COUNT,
              "reject message assets must match callfsm::MSG_COUNT");

const ui_asset_t *notice_asset(Notice n, uint8_t msg)            // msg = REJECT_MSG 일 때의 문구 번호
{
    switch (n) {
    case Notice::PEER_OFF:     return &lbl_n_peer_off;
    case Notice::NO_ANSWER:    return &lbl_n_no_answer;
    case Notice::REJECTED:     return &lbl_n_rejected;
    case Notice::LOST:         return &lbl_n_lost;
    case Notice::CALL_ENDED:   return &lbl_n_ended;
    case Notice::MISSED:       return &lbl_n_missed;
    case Notice::RELAY_SILENT: return &lbl_n_relay_silent;
    case Notice::REJECT_MSG:   return callfsm::msg_valid(msg) ? MSG_NOTICE[msg - 1] : &lbl_n_rejected;   // 모르는 번호면 그냥 거절 문구
    case Notice::BUSY:         return &lbl_n_busy;               // "통화 중이에요" — 건 상대가 다른 통화·벨 중이다(전화기 3대, call_fsm.h FSM-D7, 설계서 TW-D9)
    case Notice::NONE:         break;
    }
    return nullptr;
}

Scene scene_of(Screen s)
{
    switch (s) {
    case Screen::BOOT:      return Scene::IDLE;
    case Screen::IDLE:      return Scene::PEERS;                 // 상대 둘의 줄을 비켜 둔 구름(전화기 3대)
    case Screen::INCOMING:  return Scene::INCOMING;
    case Screen::DECLINE:   return Scene::HILL;
    case Screen::DIALING:
    case Screen::ANSWERING:
    case Screen::IN_CALL:
    case Screen::ENDING:    return Scene::CALL;
    }
    return Scene::IDLE;
}

// "N번 전화기". 등록된 번호(roster.h 의 1~3)가 아니면 nullptr. big = 대기 화면의 이름 알약(32px), 아니면 내 이름표(28px)
const ui_asset_t *unit_label(int unit, bool big = false)
{
    static const ui_asset_t *const TAG[]  = { &lbl_unit1, &lbl_unit2, &lbl_unit3 };
    static const ui_asset_t *const NAME[] = { &lbl_name1, &lbl_name2, &lbl_name3 };
    static_assert(sizeof TAG / sizeof TAG[0] == roster::MAX_UNITS && sizeof NAME / sizeof NAME[0] == roster::MAX_UNITS,
                  "one name label per registered unit (tools/gen_ui_assets.py lbl_unitN, lbl_nameN)");
    return roster::valid_unit(unit) ? (big ? NAME : TAG)[unit - 1] : nullptr;
}

void draw_unit(const Model &m)                                   // 왼쪽 위(UNIT_TAG): 내 캐릭터의 얼굴 + 몇 번 전화기. 대기 화면에서는 이것이 "나"의 전부다
{
    const int x = UNIT_TAG.x + UNIT_TAG_PAD, cy = 46;
    const ui_asset_t *a = unit_label(m.unit);
    if (!a) { gfx::blit_asset(&lbl_unit0, x, cy - lbl_unit0.h / 2, toy::INK); return; }
    const int w = toy::head(toy::who_of_unit(m.unit), false, x, cy);
    gfx::blit_asset(a, x + w + UNIT_TAG_GAP, cy - a->h / 2, toy::INK);
}

void draw_notice(const Model &m)                                 // 맨 윗줄 가운데(NOTICE_BAND — ui_layout.h UL-D6): 잠깐 띄우는 안내(알약)
{
    if (!notice_visible(m)) return;                              // 벨·메시지 고르기 화면에서는 그리지 않는다(자리가 없다 — 안내 시간은 계속 간다)
    toy::label_pill(notice_asset(m.notice, m.notice_msg), NOTICE_BAND.x + NOTICE_BAND.w / 2, NOTICE_BAND.y + NOTICE_BAND.h / 2, NOTICE_BAND.h,
                    m.notice == Notice::MISSED ? toy::WARN : toy::WHITE);
}

void draw_wifi_badge(const Model &m)                             // 왼쪽 아래: Wi-Fi 상태. 누르면 설정 화면
{
    const Rect &r = BTN_WIFI;
    const int cy = toy::keycap_cy(r);
    if (!m.wifi_connected || !m.ssid[0]) {
        toy::button(r, &lbl_wifi_setup, true, toy::WARN);
        return;
    }
    toy::keycap(r, toy::WHITE, 28);
    const int bars = wifistore::bars(m.rssi);                    // 신호 막대 4칸
    for (int i = 0; i < 4; ++i) {
        const int bh = 12 + i * 8;
        gfx::fill_rrect(r.x + 25 + i * 13, cy + 18 - bh, 8, bh, 2, i < bars ? toy::INK : toy::PALE);
    }
    char name[33]; strlcpy(name, m.ssid, sizeof name);           // 이름이 길면 뒤를 자른다
    const int tx = r.x + 25 + 48 + 16, max_w = r.x + r.w - 20 - tx;
    for (size_t n = strlen(name); n > 1 && gfx::text_w(name, 1.5f) > max_w; --n) {
        while (n > 1 && ((unsigned char)name[n - 1] & 0xC0) == 0x80) --n;   // 한글 등 여러 바이트 글자는 통째로 지운다
        name[n - 1] = 0;
    }
    gfx::text(name, tx, cy, toy::INK, 1.5f);
}

// 대기 화면의 상대 한 줄(전화기 3대 — ui_layout.h UL-D4): 위에서부터 이름 알약 · 구름 위의 캐릭터 · 둥근 통화 키캡. 가로 가운데는 그 키캡이다.
//   켜짐·꺼짐은 세 가지로 함께 보인다(색만으로 구분하지 않는다 — 디자인 규칙 §5.4): 캐릭터(손 흔들기 ↔ 눈 감고 Z) · 불빛(GO ↔ PALE) · 키캡(GO·광택 ↔ PALE·흐린 그림과 글자).
//   꺼진 상대의 키캡에도 그림자를 남긴다: 눌린다(누르면 상태 기계가 "꺼져 있어 걸 수 없어요"를 낸다 — 디자인 규칙 §5.3 의 예외).
void draw_peer_name(const Rect &cell, int unit, bool on)         // 이름 알약: "N번 전화기" + 켜짐 불빛
{
    const ui_asset_t *a = unit_label(unit, true);
    if (!a) return;                                              // 등록된 번호가 아니다(draw_peer 가 이미 걸렀다)
    const int cx = cell.x + cell.w / 2, cy = cell.y + cell.h / 2, w = peer_name_w(a->w);   // 폭 = 여백 24 · 글자 · 틈 12 · 불빛 · 여백 18 (ui_layout.h)
    toy::pill(cx, cy, w, cell.h, toy::WHITE);
    const int lamp_cx = cx + w / 2 - 18 - PEER_LAMP / 2;
    gfx::blit_asset(a, cx - w / 2 + 24, cy - a->h / 2, toy::INK);
    gfx::fill_circle(lamp_cx, cy, PEER_LAMP / 2, toy::INK);
    gfx::fill_circle(lamp_cx, cy, PEER_LAMP / 2 - 3, on ? toy::GO : toy::PALE);
}

void draw_peer(const Model &m, int slot)
{
    const int unit = peer_unit(m, slot);
    if (!unit) return;                                           // 상대가 없는 자리(미등록 기기) — 터치 판정(hit)도 그 키캡을 돌려주지 않는다
    const toy::Who who = toy::who_of_unit(unit);
    const bool on = peer_on(m, slot);
    const Circle &b = BTN_CALLS[slot];
    draw_peer_name(PEER_NAME[slot], unit, on);
    toy::cloud_wide(b.cx, PEER_CLOUD_CY);                        // 상대는 구름 위에 선다(멀리 있는 친구 — 전화 옴 화면의 건 쪽과 같은 말)
    if (on) toy::character(who, Pose::HI, b.cx, PEER_FEET_Y, b.cx > CX);   // 오른쪽 상대만 뒤집는다 → 둘 다 바깥쪽 손을 든다
    else    toy::character(who, Pose::SLEEP, b.cx, PEER_FEET_Y);           // 잠든 그림은 뒤집지 않는다(Z 가 뒤집힌다)
    toy::round_button(b.cx, b.cy, b.r, on ? toy::GO : toy::PALE);
    const int fy = toy::round_face_cy(b.cy);                     // 윗면의 가운데. 수화기 그림(68) 위, "통화"(48px) 아래 — 둘 다 윗면 원(반지름 76) 안
    if (on) toy::icon(Icon::GLOSS, b.cx - 30, fy - 50);
    toy::icon(Icon::CALL, b.cx, fy - 24, on ? 255 : 110);
    gfx::blit_asset_center(&lbl_call_s, b.cx, fy + 36, on ? toy::INK : toy::MUTED);
}

void draw_missed(const Model &m)                                 // 아래 가운데(MISSED_AREA): 병아리 + "부재중 n건"
{
    if (m.missed == 0) return;
    const int cy = 648, h = 60, gap = 8;
    const int num_w = gfx::number_w(m.missed, false);
    const int inner = lbl_missed.w + gap + num_w + gap / 2 + lbl_count_unit.w;
    const int w = inner + h + 2 * toy::PILL_OUTLINE;
    toy::chick(Chick::IDLE, CX - w / 2 - 72, 707);
    toy::pill(CX, cy, w, h, toy::WARN);
    int x = CX - inner / 2;
    x += gfx::blit_asset(&lbl_missed, x, cy - lbl_missed.h / 2, toy::INK) + gap;
    gfx::draw_number(m.missed, x + num_w / 2, cy, false, toy::INK);
    x += num_w + gap / 2;
    gfx::blit_asset(&lbl_count_unit, x, cy - lbl_count_unit.h / 2, toy::INK);
}

void draw_boot(const Model &m)                                   // 아직 못 붙음: 병아리 + 지금 무엇을 하는지(문제면 노랑·빨강)
{
    const ui_asset_t *a = &lbl_relay_connecting; uint32_t fill = toy::WHITE;
    if (m.wifi_phase == netwifi::Phase::INIT_FAILED)      { a = &lbl_wifi_init_fail; fill = toy::STOP; }
    else if (!m.wifi_connected && m.wifi_phase == netwifi::Phase::NO_KNOWN) { a = &lbl_wifi_none; fill = toy::WARN; }
    else if (!m.wifi_connected)                           { a = &lbl_wifi_connecting; }
    else if (m.unit == 0)                                 { a = &lbl_unreg_help; fill = toy::STOP; }
    else if (m.relay_code == 401)                         { a = &lbl_auth_fail; fill = toy::STOP; }
    else if (m.relay_attempt >= 2)                        { a = &lbl_relay_fail; fill = toy::WARN; }
    toy::cloud_wide(CX, 310);                                    // 병아리가 구름 위에 서서 기다린다
    toy::chick(Chick::BIG, CX, 314);
    toy::label_pill(a, CX, 384, 72, fill);
    draw_wifi_badge(m);
}

void draw_idle(const Model &m)                                   // 내 캐릭터는 서지 않는다(왼쪽 위 이름표뿐) — 상대 둘이 나란히
{
    for (int slot = 0; slot < MAX_PEERS; ++slot) draw_peer(m, slot);
    draw_missed(m);
    draw_wifi_badge(m);
}

void draw_dialing(const Model &m)
{
    toy::character(toy::who_of_unit(m.unit), Pose::CUP, TALK_ME_X, TALK_BASE);
    toy::character(toy::who_of_unit(call_unit(m)), Pose::WAVE, PEER_X, PEER_BASE, true);   // 내가 건 상대
    gfx::blit_asset_center(&lbl_dialing, CX, 240, toy::INK);
    if (m.fsm == callfsm::State::RINGBACK) toy::label_pill(&lbl_ringback, CX, 336, 60, toy::WHITE);
    toy::button(BTN_CANCEL, &lbl_cancel);
}

void draw_incoming(const Model &m)
{
    const toy::Who me = toy::who_of_unit(m.unit);
    toy::character(toy::who_of_unit(call_unit(m)), Pose::CALLER, CX, 189);   // 전화 건 쪽(누가 걸었나 = 이 캐릭터)이 가운데 위 구름에 올라서 있다
    gfx::blit_asset_center(&lbl_incoming, CX, 268, toy::INK);
    toy::button(BTN_ACCEPT, &lbl_accept, true, toy::GO, toy::icon_sprite(Icon::CALL));
    toy::button(BTN_REJECT, &lbl_reject, true, toy::STOP, toy::icon_sprite(Icon::HANG));
    // 아래 줄: 벨 끄기(끈 뒤에는 "벨 꺼짐" — 납작한 판의 상태 표시, 버튼이 아니다), 메시지로 거절
    toy::button(BTN_MUTE, m.bell_muted ? &lbl_mute_done : &lbl_mute, !m.bell_muted);
    toy::button(BTN_DECLINE, &lbl_decline);
    toy::character(me, Pose::CHEER, 112, 686);
    toy::chick(Chick::JUMP, 1172, 678);
}

void draw_decline(const Model &m)                                // 메시지 고르기: 문구를 누르면 그 문구와 함께 거절된다
{
    gfx::blit_asset_center(&lbl_decline_hint, CX, 48, toy::INK);
    for (int i = 0; i < callfsm::MSG_COUNT; ++i) toy::button(BTN_MSG[i], MSG_LABEL[i]);
    toy::cloud_wide(CX, 472);                                    // 벨은 아직 울린다 — 건 쪽이 가운데에서 기다린다
    toy::character(toy::who_of_unit(call_unit(m)), Pose::CALLER, CX, 480);
    toy::button(BTN_BACK, &lbl_back);
}

// 종이컵 전화를 든 둘과 그 사이의 줄(연결 중·통화 중·끊는 중). 줄을 먼저 그려 컵이 줄 끝을 덮게 한다.
void draw_talkers(const Model &m)
{
    const int y = TALK_BASE - toy::CUP_DY;
    gfx::stroke_quad(TALK_ME_X + toy::CUP_DX, y, CX, y + 38, TALK_PEER_X - toy::CUP_DX, y, 4, toy::INK);
    toy::character(toy::who_of_unit(m.unit), Pose::CUP, TALK_ME_X, TALK_BASE);
    toy::character(toy::who_of_unit(call_unit(m)), Pose::CUP, TALK_PEER_X, TALK_BASE, true);   // 이 통화의 상대
}

// 음량 칸(P4V-D4): 흰 판 위에 크게(＋) / 스피커와 단계 막대 / 작게(−). 글자 없이 도형과 스피커 그림만 쓴다.
// 판(VOL_PANEL) 안만 칠한다 → 음량만 바뀌었을 때는 이 함수만 부른다(ui::draw_volume). 판은 음량 칸이 뜨는 화면의 어느 버튼·캐릭터·구름·꽃과도 겹치지 않는다.
// 먼저 판 자리의 바탕(하늘·동산)을 다시 깐다: 판의 둥근 가장자리는 바탕과 섞이는 픽셀이라, 바탕 없이 거듭 그리면 누를 때마다 모서리가 진해진다
// (2026-10-08 검수 I-1). 그래서 몇 번을 그려도 결과가 같다 — tools/sim/sim_main.cpp 가 장면마다 확인한다.
// 어느 화면에 두는지·좌표·단계 판단은 ui_layout.h·vol_steps.h 가 정한다 — 화면을 다시 디자인할 때는 이 함수(와 그 좌표)만 바꾼다.
// 막대는 스피커 옆에 가운데 정렬로 둔다(2026-10-08 사용자 지적: 바닥에 맞춘 계단 막대는 Wi-Fi 신호 표시로 읽힌다).
void draw_volume_cell(const Model &m)
{
    if (!volume_visible(m)) return;
    const Rect &u = BTN_VOL_UP, &d = BTN_VOL_DN, &b = VOL_BARS;
    toy::background_patch(VOL_PANEL);
    toy::panel(VOL_PANEL, toy::WHITE, 44);
    const int step = volsteps::clamp(m.vol_step);
    const uint32_t up_c = step < volsteps::STEP_MAX ? toy::INK : toy::FAINT;   // 끝에 닿으면 기호를 흐리게(더 못 간다 — 눌러도 그대로다)
    const uint32_t dn_c = step > volsteps::STEP_MIN ? toy::INK : toy::FAINT;
    const int arm = 28, thick = 14;                              // 기호의 팔 길이·굵기
    toy::keycap(u, toy::WARN, 32); toy::gloss(u);
    int cx = u.x + u.w / 2, cy = toy::keycap_cy(u);
    gfx::fill_rrect(cx - arm, cy - thick / 2, 2 * arm, thick, thick / 2, up_c);
    gfx::fill_rrect(cx - thick / 2, cy - arm, thick, 2 * arm, thick / 2, up_c);
    toy::keycap(d, toy::WARN, 32); toy::gloss(d);
    cx = d.x + d.w / 2; cy = toy::keycap_cy(d);
    gfx::fill_rrect(cx - arm, cy - thick / 2, 2 * arm, thick, thick / 2, dn_c);
    // 스피커 + 막대: 왼쪽이 낮고 오른쪽이 높다, 지금 단계까지 파랑
    const ui_sprite_t *spk = toy::icon_sprite(Icon::SPEAKER);
    const int bw = 15, gap = 4, sgap = 9, bars_w = volsteps::COUNT * bw + (volsteps::COUNT - 1) * gap;
    const int bcy = b.y + b.h / 2;
    int x = b.x + (b.w - (spk->w + sgap + bars_w)) / 2;
    gfx::blit_sprite(spk, x, bcy - spk->h / 2);
    x += spk->w + sgap;
    for (int i = 0; i < volsteps::COUNT; ++i, x += bw + gap) {
        const int bh = 24 + i * 14;
        gfx::fill_rrect(x, bcy - bh / 2, bw, bh, bw / 2, toy::INK);
        gfx::fill_rrect(x + 3, bcy - bh / 2 + 3, bw - 6, bh - 6, (bw - 6) / 2, i < step ? toy::VOL : toy::PALE);
    }
}

void draw_in_call(const Model &m)
{
    int mm = 0, ss = 0;
    talk_clock(m.talk_seconds, &mm, &ss);
    toy::label_pill(&lbl_in_call, CX, 208, 64, toy::GO);
    gfx::draw_clock(mm, ss, CX, 368, toy::INK);
    draw_talkers(m);
    toy::icon(Icon::NOTE, 572, 468);
    toy::icon(Icon::NOTE, 712, 460);
    toy::button(BTN_HANGUP, &lbl_hangup, true, toy::STOP, toy::icon_sprite(Icon::HANG));
    toy::chick(Chick::HUM, 105, 693);
}

}  // namespace

namespace ui {

void draw(const Model &m)
{
    const Screen sc = screen_for(m);
    toy::background(scene_of(sc));
    draw_unit(m);
    switch (sc) {
    case Screen::BOOT:      draw_boot(m); break;
    case Screen::IDLE:      draw_idle(m); break;
    case Screen::DIALING:   draw_dialing(m); break;
    case Screen::INCOMING:  draw_incoming(m); break;
    case Screen::DECLINE:   draw_decline(m); break;
    case Screen::ANSWERING: draw_talkers(m); gfx::blit_asset_center(&lbl_answering, CX, 300, toy::INK); break;
    case Screen::IN_CALL:   draw_in_call(m); break;
    case Screen::ENDING:    draw_talkers(m); gfx::blit_asset_center(&lbl_ending, CX, 300, toy::MUTED); break;
    }
    draw_volume_cell(m);                                         // 대기·통화 화면에만 그린다(volume_visible)
    draw_notice(m);
}

void draw_volume(const Model &m) { draw_volume_cell(m); }

}  // namespace ui
