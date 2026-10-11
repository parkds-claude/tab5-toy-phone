// test_sc_ui_layout.cpp — main/sc_ui_layout.h 의 맥 단위 시험(배치·터치 판정·두 번 누르기·시계).
//   빌드·실행: clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I main -I ../tab5_phone/main tools/test_sc_ui_layout.cpp -o /tmp/t && /tmp/t
#include <cstdio>
#include <vector>

#include "sc_ui_layout.h"

using namespace sclayout;
using callfsm::State;

static int g_pass = 0, g_fail = 0;
#define CHECK(c) do { if (c) ++g_pass; else { ++g_fail; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static Model base(State st)
{
    Model m; m.fsm = st; m.link = Link::UP; m.unit = 3; m.peer_unit[0] = 1; m.peer_unit[1] = 2; m.peer_online[0] = m.peer_online[1] = true; m.deck_ok = true;
    return m;
}
static bool inside(const Rect &r) { return r.x >= 0 && r.y >= 0 && r.x + r.w <= W && r.y + r.h <= H; }
static bool overlap(const Rect &a, const Rect &b) { return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h; }
static int cx(const Rect &r) { return r.x + r.w / 2; }
static int cy(const Rect &r) { return r.y + r.h / 2; }

int main()
{
    // ── 크기(SL-D1)와 화면 안 ──
    const Rect primary[] = { BTN_CALL[0], BTN_CALL[1], BTN_CANCEL, BTN_ACCEPT, BTN_REJECT, BTN_HANGUP };
    for (const Rect &r : primary) { CHECK(r.w >= PRIMARY_MIN && r.h >= PRIMARY_MIN); CHECK(inside(r)); }
    const Rect secondary[] = { BTN_DECK, BTN_VOL_DN, BTN_VOL_UP };
    for (const Rect &r : secondary) { CHECK((r.w < r.h ? r.w : r.h) >= SECONDARY_MIN); CHECK((r.w > r.h ? r.w : r.h) >= PRIMARY_MIN); CHECK(inside(r)); }
    for (const Rect &r : { TOP_STRIP, MISSED_AREA, VOL_PIPS, CLOCK_AREA, VOLUME_DIRTY }) CHECK(inside(r));

    // ── 화면마다: 누르는 것끼리, 누르는 것과 보여 주는 조각이 겹치지 않는다 ──
    {   // 대기
        const std::vector<Rect> v = { BTN_CALL[0], BTN_CALL[1], BTN_DECK, MISSED_AREA, TOP_STRIP };
        for (size_t i = 0; i < v.size(); ++i) for (size_t j = i + 1; j < v.size(); ++j) CHECK(!overlap(v[i], v[j]));
    }
    CHECK(!overlap(BTN_ACCEPT, BTN_REJECT));
    {   // 통화 중
        const std::vector<Rect> v = { BTN_VOL_DN, BTN_HANGUP, BTN_VOL_UP, VOL_PIPS, CLOCK_AREA };
        for (size_t i = 0; i < v.size(); ++i) for (size_t j = i + 1; j < v.size(); ++j) CHECK(!overlap(v[i], v[j]));
    }
    // 음량이 바뀔 때 화면으로 내보내는 범위가 다시 그리는 것(표시 + 버튼 줄)을 다 담는다
    for (const Rect &r : { VOL_PIPS, BTN_VOL_DN, BTN_HANGUP, BTN_VOL_UP }) CHECK(r.x >= VOLUME_DIRTY.x && r.y >= VOLUME_DIRTY.y && r.x + r.w <= VOLUME_DIRTY.x + VOLUME_DIRTY.w && r.y + r.h <= VOLUME_DIRTY.y + VOLUME_DIRTY.h);
    CHECK(VOLUME_BAND_Y <= BTN_VOL_DN.y && VOLUME_BAND_Y <= BTN_HANGUP.y && VOLUME_BAND_Y >= VOL_PIPS.y + VOL_PIPS.h);
    CHECK(VOLUME_DIRTY.x == 0 && VOLUME_DIRTY.w == W && VOLUME_DIRTY.y + VOLUME_DIRTY.h == H);
    // 받기는 왼쪽, 거절은 오른쪽(Tab5 와 같은 쪽 — 색만으로 가르지 않는다)
    CHECK(cx(BTN_ACCEPT) < cx(BTN_REJECT));
    // 벨이 오는 순간 대기 화면의 통화 키캡을 누르던 손가락: 같은 자리가 받기·거절이 되지 않게 화면 가드가 막는다(아래 TapGuard) — 자리는 겹쳐도 된다

    // ── 화면 고르기 ──
    CHECK(screen_for(base(State::OFFLINE)) == Screen::CONNECTING);
    CHECK(screen_for(base(State::IDLE)) == Screen::IDLE);
    CHECK(screen_for(base(State::DIALING)) == Screen::DIALING && screen_for(base(State::RINGBACK)) == Screen::DIALING);
    CHECK(screen_for(base(State::INCOMING)) == Screen::INCOMING);
    CHECK(screen_for(base(State::ANSWERING)) == Screen::ANSWERING);
    CHECK(screen_for(base(State::IN_CALL)) == Screen::IN_CALL);
    CHECK(screen_for(base(State::ENDING)) == Screen::ENDING);
    { Model m = base(State::IN_CALL); m.to_deck = true; CHECK(screen_for(m) == Screen::TO_DECK); }

    // ── 터치 판정 ──
    {   // 대기: 왼쪽 = slot 0, 오른쪽 = slot 1(SL-D3), 스트림덱
        Model m = base(State::IDLE);
        CHECK(hit(m, cx(BTN_CALL[0]), cy(BTN_CALL[0])) == Tap::CALL_A);
        CHECK(hit(m, cx(BTN_CALL[1]), cy(BTN_CALL[1])) == Tap::CALL_B);
        CHECK(slot_of(Tap::CALL_A) == 0 && slot_of(Tap::CALL_B) == 1 && slot_of(Tap::DECK) == -1 && slot_of(Tap::NONE) == -1);
        CHECK(hit(m, cx(BTN_DECK), cy(BTN_DECK)) == Tap::DECK);
        CHECK(hit(m, cx(MISSED_AREA), cy(MISSED_AREA)) == Tap::NONE);            // 부재중 알약은 누르는 것이 아니다
        CHECK(hit(m, cx(TOP_STRIP), cy(TOP_STRIP)) == Tap::NONE);
        CHECK(hit(m, BTN_CALL[0].x + BTN_CALL[0].w, cy(BTN_CALL[0])) == Tap::NONE);   // 두 키캡 사이의 틈
        CHECK(hit(m, -1, -1) == Tap::NONE && hit(m, W, H) == Tap::NONE && hit(m, 100000, 100000) == Tap::NONE);
        m.peer_online[1] = false;                                                // 꺼진 상대도 눌린다(안내는 call_fsm 이 낸다)
        CHECK(hit(m, cx(BTN_CALL[1]), cy(BTN_CALL[1])) == Tap::CALL_B);
        m.peer_unit[1] = 0;                                                      // 상대가 하나뿐이면 오른쪽 자리는 비어 있다
        CHECK(hit(m, cx(BTN_CALL[1]), cy(BTN_CALL[1])) == Tap::NONE);
        CHECK(call_visible(m, 0) && !call_visible(m, 1) && !call_visible(m, 2) && !call_visible(m, -1));
        m.deck_ok = false;                                                       // 스트림덱이 없으면 눌리지 않는다
        CHECK(hit(m, cx(BTN_DECK), cy(BTN_DECK)) == Tap::NONE);
        CHECK(deck_visible(m) && !deck_enabled(m));
    }
    {   // 연결 중: 스트림덱만
        Model m = base(State::OFFLINE);
        CHECK(hit(m, cx(BTN_DECK), cy(BTN_DECK)) == Tap::DECK);
        CHECK(hit(m, cx(BTN_CALL[0]), cy(BTN_CALL[0])) == Tap::NONE);
    }
    {   // 거는 중: 취소만. 스트림덱·통화 키캡 자리는 죽어 있다
        Model m = base(State::RINGBACK);
        CHECK(hit(m, cx(BTN_CANCEL), cy(BTN_CANCEL)) == Tap::CANCEL);
        CHECK(hit(m, cx(BTN_CALL[0]), BTN_CALL[0].y + 4) == Tap::NONE);
        CHECK(hit(m, BTN_DECK.x + 4, cy(BTN_DECK)) == Tap::NONE);
        CHECK(!deck_visible(m));
    }
    {   // 전화 옴
        Model m = base(State::INCOMING);
        CHECK(hit(m, cx(BTN_ACCEPT), cy(BTN_ACCEPT)) == Tap::ACCEPT);
        CHECK(hit(m, cx(BTN_REJECT), cy(BTN_REJECT)) == Tap::REJECT);
        CHECK(hit(m, cx(BTN_CALL[0]), BTN_CALL[0].y + 10) == Tap::NONE);
    }
    for (State st : { State::IN_CALL, State::ANSWERING }) {   // 통화 중·연결 중: 끊기와 음량
        Model m = base(st);
        CHECK(hit(m, cx(BTN_HANGUP), cy(BTN_HANGUP)) == Tap::HANGUP);
        CHECK(hit(m, cx(BTN_VOL_DN), cy(BTN_VOL_DN)) == Tap::VOL_DN);
        CHECK(hit(m, cx(BTN_VOL_UP), cy(BTN_VOL_UP)) == Tap::VOL_UP);
        CHECK(hit(m, cx(CLOCK_AREA), cy(CLOCK_AREA)) == Tap::NONE);
        CHECK(volume_visible(m));
    }
    CHECK(!volume_visible(base(State::IDLE)));
    CHECK(vol_req_of(Tap::VOL_UP) == volsteps::Req::UP && vol_req_of(Tap::VOL_DN) == volsteps::Req::DOWN && vol_req_of(Tap::HANGUP) == volsteps::Req::NONE);
    {   // 끊는 중·스트림덱으로 가는 중: 아무것도 눌리지 않는다
        Model m = base(State::ENDING);
        for (int y = 0; y < H; y += 6) for (int x = 0; x < W; x += 6) if (hit(m, x, y) != Tap::NONE) { CHECK(false); y = H; break; }
        m = base(State::IDLE); m.to_deck = true;
        CHECK(hit(m, cx(BTN_DECK), cy(BTN_DECK)) == Tap::NONE && hit(m, cx(BTN_CALL[0]), cy(BTN_CALL[0])) == Tap::NONE);
    }
    // 어느 화면에서도 hit 이 돌려주는 것은 그 화면에 있는 버튼뿐이다(전수: 4px 격자)
    for (State st : { State::OFFLINE, State::IDLE, State::DIALING, State::RINGBACK, State::INCOMING, State::ANSWERING, State::IN_CALL, State::ENDING }) {
        const Model m = base(st);
        bool ok = true;
        for (int y = 0; y < H && ok; y += 4) for (int x = 0; x < W && ok; x += 4) {
            const Tap t = hit(m, x, y);
            const bool call = t == Tap::CALL_A || t == Tap::CALL_B;
            if (call && st != State::IDLE) ok = false;
            if (t == Tap::DECK && st != State::IDLE && st != State::OFFLINE) ok = false;
            if ((t == Tap::ACCEPT || t == Tap::REJECT) && st != State::INCOMING) ok = false;
            if (t == Tap::CANCEL && st != State::DIALING && st != State::RINGBACK) ok = false;
            if ((t == Tap::HANGUP || t == Tap::VOL_DN || t == Tap::VOL_UP) && st != State::IN_CALL && st != State::ANSWERING) ok = false;
        }
        CHECK(ok);
    }

    // ── 터치 가드 ──
    {
        TapGuard g;
        CHECK(!g.allow(1000, 500));                 // 화면이 바뀐 지 500ms — 버린다
        CHECK(g.allow(1100, 500));                  // 600ms 지났다
        CHECK(!g.allow(1400, 500));                 // 직전 터치 뒤 300ms — 버린다
        CHECK(g.allow(1500, 500));
        TapGuard h;
        CHECK(!h.allow(599, 0) && h.allow(600, 0)); // 경계
    }

    // ── 벨 화면 직후의 대기 화면은 더 길게 막는다(SL-D4) ──
    {
        CHECK(screen_guard_for(Screen::INCOMING, Screen::IDLE) == TAP_AFTER_BELL_GUARD_MS && TAP_AFTER_BELL_GUARD_MS > TAP_SCREEN_GUARD_MS);
        for (Screen prev : { Screen::CONNECTING, Screen::IDLE, Screen::DIALING, Screen::ANSWERING, Screen::IN_CALL, Screen::ENDING, Screen::TO_DECK })
            CHECK(screen_guard_for(prev, Screen::IDLE) == TAP_SCREEN_GUARD_MS);             // 다른 화면에서 온 대기 화면은 평소대로
        for (Screen now : { Screen::CONNECTING, Screen::DIALING, Screen::INCOMING, Screen::ANSWERING, Screen::IN_CALL, Screen::ENDING })
            CHECK(screen_guard_for(Screen::INCOMING, now) == TAP_SCREEN_GUARD_MS);          // 벨 화면에서 받기를 눌러 넘어간 화면(끊기 등)은 늦추지 않는다
        TapGuard g;
        CHECK(!g.allow(1400, 0, screen_guard_for(Screen::INCOMING, Screen::IDLE)));         // 1.4초: 아직 막힌다
        CHECK(g.allow(1500, 0, screen_guard_for(Screen::INCOMING, Screen::IDLE)));          // 1.5초: 받는다
        // 겹치는 자리가 실제로 있다는 것(그래서 가드가 필요하다): 거절 키캡의 윗부분이 오른쪽 통화 키캡의 아랫부분과 겹친다
        CHECK(overlap(BTN_REJECT, BTN_CALL[1]) && overlap(BTN_ACCEPT, BTN_CALL[0]));
    }

    // ── 스트림덱 두 번 누르기(SL-D2) ──
    {
        DeckArm d;
        CHECK(!d.armed(0));
        CHECK(!d.tap(1000));                        // 첫 번째: 장전만
        CHECK(d.armed(1000) && d.armed(3999) && !d.armed(4000));
        CHECK(d.tap(2000));                         // 3초 안의 두 번째: 간다
        CHECK(!d.armed(2000));                      // 간 뒤에는 풀려 있다
        CHECK(!d.tap(10000));                       // 다시 첫 번째
        CHECK(!d.tap(13000));                       // 3초가 지나 누르면 다시 첫 번째(장전을 새로)
        CHECK(d.armed(13000));
        d.cancel();
        CHECK(!d.armed(13001) && !d.tap(13002));    // 풀린 뒤의 누름은 첫 번째
    }

    // ── 시계 ──
    CHECK(clock_min(0) == 0 && clock_sec(0) == 0);
    CHECK(clock_min(83) == 1 && clock_sec(83) == 23);
    CHECK(clock_min(5999) == 99 && clock_sec(5999) == 59);
    CHECK(clock_min(6000) == 99 && clock_sec(6000) == 59);          // 100분부터는 99:59 에서 멈춘다
    CHECK(clock_min(1 << 30) == 99 && clock_sec(1 << 30) == 59);
    CHECK(clock_min(-5) == 0 && clock_sec(-5) == 0);

    // ── 이름 ──
    CHECK(screen_name(Screen::IDLE)[0] == 'I' && tap_name(Tap::DECK)[0] == 'D');
    CHECK(notice_ms(callfsm::Notice::REJECT_MSG) == 10000 && notice_ms(callfsm::Notice::BUSY) == 4000);

    printf("sc_ui_layout: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
