// sc_ui_layout.h — 스택짱 전화기 화면(320×240)의 배치와 "무엇을 눌렀나". 하드웨어와 무관한 순수 로직(맥 시험: tools/test_sc_ui_layout.cpp).
// 설계서 docs/exec-plans/2026-10-08-phone-3way-design.md §10.2, 디자인 규칙 docs/design/README.md §5.10(스택짱 화면).
//   Tab5 의 ui_layout.h(1280×720)와 같은 역할이다. 따로 둔 이유: 화면이 16분의 1 넓이라 자리를 줄여 옮길 수 없다(버튼이 손가락보다 작아진다) — 구성부터 다르다.
//   함께 쓰는 것: 통화 상태(call_fsm.h), 음량 단계(vol_steps.h). 터치 가드의 시간 값은 Tab5 와 같다.
//
//   SL-D1 누르는 것의 크기: 화면의 주요 버튼(통화·받기·거절·끊기·취소)은 가로·세로 모두 72px(9.2mm) 이상, 보조 버튼(음량·스트림덱)은 짧은 변 56px(7.1mm) 이상.
//         이유: 2인치 화면의 화소 간격은 0.1275mm 다. 72px 을 전부에 요구하면 대기 화면에 상대 둘 + 스트림덱이 들어가지 않는다.
//         비용: 보조 버튼은 주요 버튼보다 누르기 어렵다(통화 중에 급히 누르는 것은 끊기 하나 — 그것은 152×72). 탈출구: 아래 Rect 만 바꾼다(시험이 겹침·크기·화면 안을 고정한다).
//   SL-D2 스트림덱으로 가기는 두 번 눌러야 한다(DeckArm): 첫 번째는 "한 번 더!"로 바뀌고 3초 안에 다시 누르면 간다.
//         이유: 가면 전화를 못 받는다(상대 화면에 "잠듦") — 잘못 눌러 넘어가면 돌아오는 데 스트림덱 부팅 8초 + 버튼 한 번이 든다.
//         비용: 한 번 더 누른다. 탈출구: sc_app.cpp 가 DeckArm 을 거치지 않고 바로 부른다.
//   SL-D3 상대 자리(slot)는 왼쪽이 0, 오른쪽이 1 — roster.h 의 번호 오름차순 그대로다(3번인 스택짱에게는 왼쪽 = 1번 곰, 오른쪽 = 2번 토끼).
#pragma once
#include <cstdint>

#include "call_fsm.h"
#include "vol_steps.h"

namespace sclayout {

constexpr int W = 320, H = 240;

struct Rect {
    int x, y, w, h;
    constexpr bool hit(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

constexpr int MAX_PEERS = 2;

enum class Link : uint8_t {         // 교환기까지 가는 길의 지금 상태(대기 전 화면의 문구를 고른다)
    WIFI_STARTING, WIFI_SEARCHING, WIFI_NONE, WIFI_FAIL,
    RELAY_CONNECTING, RELAY_RETRY,  // 붙는 중 / 두 번 넘게 실패해 다시 붙는 중
    AUTH_FAIL,                      // 교환기가 토큰을 거절(401)
    UNREGISTERED,                   // 이 기기의 MAC 이 기기 표에 없다
    UP,
};

enum class Screen : uint8_t { CONNECTING, IDLE, DIALING, INCOMING, ANSWERING, IN_CALL, ENDING, TO_DECK };

struct Model {
    callfsm::State  fsm = callfsm::State::OFFLINE;
    Link            link = Link::WIFI_STARTING;
    int             unit = 0;                       // 내 번호(1~3, 미등록 0)
    uint8_t         peer_unit[MAX_PEERS] = {0, 0};  // slot → 상대의 번호(없으면 0)
    bool            peer_online[MAX_PEERS] = {false, false};
    uint8_t         call_peer = 0;                  // 지금(또는 직전) 통화·벨의 상대 slot
    uint16_t        missed = 0;
    int             talk_seconds = 0;
    callfsm::Notice notice = callfsm::Notice::NONE;
    uint8_t         notice_msg = 0;
    uint8_t         vol_step = volsteps::STEP_DEFAULT;
    bool            deck_ok = false;                // 스트림덱(factory 칸의 앱)이 있다
    bool            deck_armed = false;             // "한 번 더!" 상태(SL-D2)
    bool            to_deck = false;                // 스트림덱으로 가는 중(재시작 직전의 한 장)
};

// ── 자리(4px 격자) ───────────────────────────────────────────────────────────
constexpr int MARGIN = 6;
constexpr Rect TOP_STRIP   = {   0,   0, 320,  44 };                       // 이름표·부재중·안내(누르는 것 아님)
constexpr Rect BTN_CALL[MAX_PEERS] = { { 6, 46, 151, 128 }, { 163, 46, 151, 128 } };   // 대기: 상대 둘(SL-D3)
constexpr Rect BTN_DECK    = {   6, 180, 151,  56 };                       // 대기·연결 중: 스트림덱(보조)
constexpr Rect MISSED_AREA = { 163, 184, 151,  48 };                       // 대기: 부재중 알약(누르는 것 아님)
constexpr Rect BTN_CANCEL  = {  60, 160, 200,  74 };                       // 거는 중
constexpr Rect BTN_ACCEPT  = {   6, 156, 151,  78 };                       // 전화 옴: 왼쪽 = 받기(Tab5 와 같은 쪽)
constexpr Rect BTN_REJECT  = { 163, 156, 151,  78 };
constexpr Rect BTN_VOL_DN  = {   6, 160,  72,  74 };                       // 통화 중: 작게 | 끊기 | 크게
constexpr Rect BTN_HANGUP  = {  84, 160, 152,  74 };
constexpr Rect BTN_VOL_UP  = { 242, 160,  72,  74 };
constexpr Rect VOL_PIPS    = {  84, 132, 152,  20 };                       // 통화 중: 음량 단계 표시(누르는 것 아님)
constexpr Rect CLOCK_AREA  = { 124,  24, 190,  72 };                       // 통화 중: 시계 — 1초마다 이 조각만 다시 그린다
constexpr int  VOLUME_BAND_Y = 156;                                         // 통화 중: 음량이 바뀌면 음량 표시(VOL_PIPS)와 이 줄부터 화면 끝까지(버튼 줄)를 다시 그린다
constexpr Rect VOLUME_DIRTY = { 0, 132, 320, 108 };                        //   → 화면으로 내보낼 범위(VOL_PIPS 의 윗줄 ~ 화면 끝, 폭 전체)
constexpr int  CHAR_AXIS_CALL = 62, CHAR_BASE_CALL = 122;                  // 통화 중·연결 중(받은 뒤): 상대 캐릭터의 대칭축 x·바닥 y
constexpr int  CHAR_AXIS_MID = 160, CHAR_BASE_MID = 112;                   // 거는 중·전화 옴·연결 중(부팅): 가운데
constexpr int  TEXT_MID_Y = 134;                                           // 그 아래 한 줄 글자의 세로 가운데

constexpr int PRIMARY_MIN = 72, SECONDARY_MIN = 56;                        // SL-D1

enum class Tap : uint8_t { NONE, CALL_A, CALL_B, CANCEL, ACCEPT, REJECT, HANGUP, VOL_DN, VOL_UP, DECK };

inline const char *tap_name(Tap t)
{
    switch (t) {
    case Tap::NONE: return "NONE"; case Tap::CALL_A: return "CALL_A"; case Tap::CALL_B: return "CALL_B"; case Tap::CANCEL: return "CANCEL";
    case Tap::ACCEPT: return "ACCEPT"; case Tap::REJECT: return "REJECT"; case Tap::HANGUP: return "HANGUP";
    case Tap::VOL_DN: return "VOL_DN"; case Tap::VOL_UP: return "VOL_UP"; case Tap::DECK: return "DECK";
    }
    return "?";
}

inline Screen screen_for(const Model &m)
{
    if (m.to_deck) return Screen::TO_DECK;
    switch (m.fsm) {
    case callfsm::State::OFFLINE:   return Screen::CONNECTING;
    case callfsm::State::IDLE:      return Screen::IDLE;
    case callfsm::State::DIALING:
    case callfsm::State::RINGBACK:  return Screen::DIALING;
    case callfsm::State::INCOMING:  return Screen::INCOMING;
    case callfsm::State::ANSWERING: return Screen::ANSWERING;
    case callfsm::State::IN_CALL:   return Screen::IN_CALL;
    case callfsm::State::ENDING:    return Screen::ENDING;
    }
    return Screen::CONNECTING;
}

inline const char *screen_name(Screen s)
{
    switch (s) {
    case Screen::CONNECTING: return "CONNECTING"; case Screen::IDLE: return "IDLE"; case Screen::DIALING: return "DIALING";
    case Screen::INCOMING: return "INCOMING"; case Screen::ANSWERING: return "ANSWERING"; case Screen::IN_CALL: return "IN_CALL";
    case Screen::ENDING: return "ENDING"; case Screen::TO_DECK: return "TO_DECK";
    }
    return "?";
}

// 통화 키캡이 보이는가(그 slot 에 상대가 있다). 꺼져 있는 상대도 보이고 눌린다 — 누르면 "꺼져 있어요" 안내가 뜬다(call_fsm 의 PEER_OFF).
constexpr bool call_visible(const Model &m, int slot) { return slot >= 0 && slot < MAX_PEERS && m.peer_unit[slot] != 0; }
// 스트림덱 키캡이 보이는 화면인가 / 눌리는가(스트림덱이 없으면 흐린 판으로만 보인다).
inline bool deck_visible(const Model &m) { const Screen s = screen_for(m); return s == Screen::CONNECTING || s == Screen::IDLE; }
inline bool deck_enabled(const Model &m) { return deck_visible(m) && m.deck_ok; }
inline bool volume_visible(const Model &m) { const Screen s = screen_for(m); return s == Screen::IN_CALL || s == Screen::ANSWERING; }

inline Tap hit(const Model &m, int x, int y)
{
    switch (screen_for(m)) {
    case Screen::CONNECTING:
        return deck_enabled(m) && BTN_DECK.hit(x, y) ? Tap::DECK : Tap::NONE;
    case Screen::IDLE:
        if (call_visible(m, 0) && BTN_CALL[0].hit(x, y)) return Tap::CALL_A;
        if (call_visible(m, 1) && BTN_CALL[1].hit(x, y)) return Tap::CALL_B;
        return deck_enabled(m) && BTN_DECK.hit(x, y) ? Tap::DECK : Tap::NONE;
    case Screen::DIALING:
        return BTN_CANCEL.hit(x, y) ? Tap::CANCEL : Tap::NONE;
    case Screen::INCOMING:
        return BTN_ACCEPT.hit(x, y) ? Tap::ACCEPT : BTN_REJECT.hit(x, y) ? Tap::REJECT : Tap::NONE;
    case Screen::ANSWERING:
    case Screen::IN_CALL:
        return BTN_HANGUP.hit(x, y) ? Tap::HANGUP : BTN_VOL_DN.hit(x, y) ? Tap::VOL_DN : BTN_VOL_UP.hit(x, y) ? Tap::VOL_UP : Tap::NONE;
    case Screen::ENDING:
    case Screen::TO_DECK:
        return Tap::NONE;
    }
    return Tap::NONE;
}

constexpr int slot_of(Tap t) { return t == Tap::CALL_A ? 0 : t == Tap::CALL_B ? 1 : -1; }
constexpr volsteps::Req vol_req_of(Tap t) { return t == Tap::VOL_UP ? volsteps::Req::UP : t == Tap::VOL_DN ? volsteps::Req::DOWN : volsteps::Req::NONE; }

// ── 터치 가드(값은 Tab5 ui_layout.h 와 같다) ──────────────────────────────────
const int64_t TAP_SCREEN_GUARD_MS = 600;        // 화면이 바뀐 직후 이만큼은 터치를 버린다(벨이 오는 순간 대기 화면을 누르던 손가락이 받기를 누르지 않게)
const int64_t TAP_REPEAT_GUARD_MS = 400;        // 직전에 받아들인 터치 뒤 이만큼은 버린다
const int64_t NOTICE_MS = 4000, NOTICE_MSG_MS = 10000;
inline int64_t notice_ms(callfsm::Notice n) { return n == callfsm::Notice::REJECT_MSG ? NOTICE_MSG_MS : NOTICE_MS; }

// SL-D4 벨 화면이 대기 화면으로 바뀐 직후(건 쪽이 끊었다·벨 30초가 다 됐다·내가 거절했다)에는 1.5초 동안 터치를 받지 않는다.
//   이유: "거절"·"받기"를 누르려던 손가락이 바뀐 화면의 통화 키캡(아래쪽 18px 가 겹친다)에 닿으면 엉뚱한 상대에게 전화가 걸린다. Tab5 화면과 같은 규칙·같은 값(ui_layout.h UL-D7).
//   비용: 그 직후 바로 걸려면 1.5초를 기다려야 한다. 탈출구: TAP_AFTER_BELL_GUARD_MS.
const int64_t TAP_AFTER_BELL_GUARD_MS = 1500;
constexpr int64_t screen_guard_for(Screen prev, Screen now) { return (prev == Screen::INCOMING && now == Screen::IDLE) ? TAP_AFTER_BELL_GUARD_MS : TAP_SCREEN_GUARD_MS; }

struct TapGuard {
    int64_t last_ms = -1000000;
    bool allow(int64_t now_ms, int64_t screen_since_ms, int64_t screen_guard_ms = TAP_SCREEN_GUARD_MS)
    {
        if (now_ms - screen_since_ms < screen_guard_ms) return false;
        if (now_ms - last_ms < TAP_REPEAT_GUARD_MS) return false;
        last_ms = now_ms;
        return true;
    }
};

// ── 스트림덱으로 가기: 두 번 누르기(SL-D2) ────────────────────────────────────
const int64_t DECK_ARM_MS = 3000;
struct DeckArm {
    int64_t armed_at = -1;
    bool armed(int64_t now_ms) const { return armed_at >= 0 && now_ms - armed_at < DECK_ARM_MS; }
    // 눌렀다. 돌려주는 값 = 지금 가야 하나(두 번째 누름). 첫 번째면 장전만 한다.
    bool tap(int64_t now_ms)
    {
        if (armed(now_ms)) { armed_at = -1; return true; }
        armed_at = now_ms;
        return false;
    }
    void cancel() { armed_at = -1; }             // 화면이 바뀌었다(전화가 왔다 등) — 장전을 푼다
};

// 통화 시간 → 분·초(분은 99 에서 멈춘다 — 시계 칸이 네 자리다).
constexpr int clock_min(int seconds) { return seconds < 0 ? 0 : seconds / 60 > 99 ? 99 : seconds / 60; }
constexpr int clock_sec(int seconds) { return seconds < 0 ? 0 : seconds / 60 > 99 ? 59 : seconds % 60; }

}  // namespace sclayout
