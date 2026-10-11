// test_ui_layout.cpp — ui_layout.h 단위 시험:
//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_ui_layout.cpp -o /tmp/t_ui && /tmp/t_ui
// 어떤 화면을 그릴지와 터치가 어느 버튼인지(순수 로직)를 고정한다. 설계서 §10, docs/design/README.md §2(터치 96px, 주요 버튼 160px).
#include "ui_layout.h"
#include <cstdio>
#include <initializer_list>
#include <set>
#include <string>
using namespace uilayout;
using callfsm::State;
static int ok = 0, bad = 0;
#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)

static Model M(State s, bool wifi = true, bool relay = true, bool peer = true)
{
    Model m; m.fsm = s; m.wifi_connected = wifi; m.relay_up = relay; m.peer_online[0] = m.peer_online[1] = peer;
    m.unit = 1;                                                 // 전화기 3대: 상대가 있어야 통화 키캡이 있다(UL-D4) — 미등록(0)은 맨 아래 절에서 따로 본다
    m.wifi_phase = wifi ? netwifi::Phase::CONNECTED : netwifi::Phase::SEARCHING;
    return m;
}
static bool overlap(const Rect &a, const Rect &b) { return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h; }
static Rect box(const Circle &c) { return Rect{ c.cx - c.r, c.cy - c.r, 2 * c.r, 2 * c.r }; }
static bool inside_screen(const Rect &r) { return r.x >= 0 && r.y >= 0 && r.x + r.w <= SCREEN_W && r.y + r.h <= SCREEN_H; }
// app.cpp refresh() 가 하는 일 그대로: 화면이 path 의 순서로 바뀌었을 때 터치 가드가 보는 "앞 화면"(UL-D7 guard_prev). 처음 화면 앞에는 연결 중(BOOT)이 있었다고 본다
static Screen prev_after(std::initializer_list<Screen> path)
{
    Screen shown = Screen::BOOT, prev = Screen::BOOT;
    for (Screen next : path) { if (next == shown) continue; prev = guard_prev(prev, shown); shown = next; }
    return prev;
}

int main()
{
    // ── 어떤 화면인가 ───────────────────────────────────────────────────────
    CHECK("no wifi: boot screen", screen_for(M(State::OFFLINE, false, false)) == Screen::BOOT);
    CHECK("wifi but no relay: boot screen", screen_for(M(State::OFFLINE, true, false)) == Screen::BOOT);
    CHECK("idle", screen_for(M(State::IDLE)) == Screen::IDLE);
    CHECK("dialing and ringback share one screen", screen_for(M(State::DIALING)) == Screen::DIALING && screen_for(M(State::RINGBACK)) == Screen::DIALING);
    CHECK("incoming", screen_for(M(State::INCOMING)) == Screen::INCOMING);
    CHECK("answering", screen_for(M(State::ANSWERING)) == Screen::ANSWERING);
    CHECK("in call", screen_for(M(State::IN_CALL)) == Screen::IN_CALL);
    CHECK("ending", screen_for(M(State::ENDING)) == Screen::ENDING);
    CHECK("screen names", std::string(screen_name(Screen::BOOT)) == "BOOT" && std::string(screen_name(Screen::IN_CALL)) == "IN_CALL" && std::string(screen_name(Screen::DIALING)) == "DIALING");

    // ── 크기 규칙: 터치 96px 이상, 주요 버튼 160px 이상, 통화 키캡(상대마다 하나) 지름 176px 이상 ──
    CHECK("call buttons diameter >= 176 (two of them since the 3-phone screen; still a main action >= 160)", 2 * BTN_CALL.r >= 176 && 2 * BTN_CALL_B.r >= 176 && 2 * BTN_CALL.r >= 160);
    CHECK("accept/reject >= 160", BTN_ACCEPT.w >= 160 && BTN_ACCEPT.h >= 160 && BTN_REJECT.w >= 160 && BTN_REJECT.h >= 160);
    CHECK("hangup/cancel are main actions too: >= 160 both ways", BTN_HANGUP.h >= 160 && BTN_HANGUP.w >= 160 && BTN_CANCEL.h >= 160 && BTN_CANCEL.w >= 160);
    CHECK("wifi badge >= 96 tall", BTN_WIFI.h >= 96 && BTN_WIFI.w >= 96);
    CHECK("all buttons inside the screen", inside_screen(box(BTN_CALL)) && inside_screen(BTN_ACCEPT) && inside_screen(BTN_REJECT) && inside_screen(BTN_HANGUP) && inside_screen(BTN_CANCEL) && inside_screen(BTN_WIFI));
    CHECK("layout on 8px grid", BTN_ACCEPT.x % 8 == 0 && BTN_ACCEPT.y % 8 == 0 && BTN_REJECT.x % 8 == 0 && BTN_HANGUP.x % 8 == 0 && BTN_HANGUP.y % 8 == 0 && BTN_CANCEL.x % 8 == 0 && BTN_CANCEL.y % 8 == 0);

    // ── 화면이 바뀌어도 같은 자리의 두 번째 터치가 다른 버튼을 누르지 않는다 ──
    CHECK("accept does not overlap hangup (answer then instant hangup)", !overlap(BTN_ACCEPT, BTN_HANGUP));
    CHECK("reject does not overlap hangup", !overlap(BTN_REJECT, BTN_HANGUP));
    CHECK("call does not overlap cancel (call then instant cancel)", !overlap(box(BTN_CALL), BTN_CANCEL));
    CHECK("call does not overlap hangup", !overlap(box(BTN_CALL), BTN_HANGUP));
    CHECK("accept and reject apart by >= 96px", BTN_REJECT.x - (BTN_ACCEPT.x + BTN_ACCEPT.w) >= 96);
    CHECK("wifi badge away from call/cancel/hangup", !overlap(BTN_WIFI, box(BTN_CALL)) && !overlap(BTN_WIFI, BTN_CANCEL) && !overlap(BTN_WIFI, BTN_HANGUP));

    // ── 터치 → 버튼 ─────────────────────────────────────────────────────────
    {
        Model m = M(State::IDLE);
        CHECK("idle: center of call button", hit(m, BTN_CALL.cx, BTN_CALL.cy) == Tap::CALL);
        CHECK("idle: just outside the circle corner is not a call", hit(m, BTN_CALL.cx + BTN_CALL.r, BTN_CALL.cy + BTN_CALL.r) == Tap::NONE);
        CHECK("idle: wifi badge", hit(m, BTN_WIFI.x + 10, BTN_WIFI.y + 10) == Tap::WIFI);
        CHECK("idle: empty area", hit(m, 1200, 60) == Tap::NONE);
        TapGuard g;                                             // 빗나간 터치는 가드를 쓰지 않는다 → 바로 뒤의 정확한 터치가 살아야 한다
        const bool miss_counts = hit(m, 1200, 60) != Tap::NONE && g.allow(1000, 0);
        CHECK("a miss must not consume the guard (caller checks hit first)", !miss_counts && g.allow(1100, 0));
        CHECK("idle: hangup position does nothing", hit(m, BTN_HANGUP.x + 5, BTN_HANGUP.y + BTN_HANGUP.h - 5) == Tap::NONE);
        Model off = M(State::IDLE, true, true, false);
        CHECK("idle + peer off: call button still reports CALL (fsm answers with the notice)", hit(off, BTN_CALL.cx, BTN_CALL.cy) == Tap::CALL);
    }
    {
        Model m = M(State::DIALING);
        CHECK("dialing: cancel", hit(m, BTN_CANCEL.x + 1, BTN_CANCEL.y + 1) == Tap::CANCEL);
        CHECK("dialing: call button position does nothing", hit(m, BTN_CALL.cx, BTN_CALL.cy) == Tap::NONE);
        CHECK("ringback: cancel too", hit(M(State::RINGBACK), BTN_CANCEL.x + 1, BTN_CANCEL.y + 1) == Tap::CANCEL);
        CHECK("dialing: wifi badge hidden", hit(m, BTN_WIFI.x + 10, BTN_WIFI.y + 10) == Tap::NONE);
    }
    {
        Model m = M(State::INCOMING);
        CHECK("incoming: accept", hit(m, BTN_ACCEPT.x + BTN_ACCEPT.w / 2, BTN_ACCEPT.y + BTN_ACCEPT.h / 2) == Tap::ACCEPT);
        CHECK("incoming: reject", hit(m, BTN_REJECT.x + BTN_REJECT.w / 2, BTN_REJECT.y + BTN_REJECT.h / 2) == Tap::REJECT);
        CHECK("incoming: gap between the two", hit(m, 640, BTN_ACCEPT.y + 10) == Tap::NONE);
    }
    {
        CHECK("answering: no buttons", hit(M(State::ANSWERING), BTN_ACCEPT.x + 5, BTN_ACCEPT.y + 5) == Tap::NONE && hit(M(State::ANSWERING), BTN_HANGUP.x + 5, BTN_HANGUP.y + 5) == Tap::NONE);
        CHECK("in call: hangup", hit(M(State::IN_CALL), BTN_HANGUP.x + 5, BTN_HANGUP.y + 5) == Tap::HANGUP);
        CHECK("in call: accept position does nothing", hit(M(State::IN_CALL), BTN_ACCEPT.x + 5, BTN_ACCEPT.y + 5) == Tap::NONE);
        CHECK("ending: no buttons", hit(M(State::ENDING), BTN_HANGUP.x + 5, BTN_HANGUP.y + 5) == Tap::NONE);
    }
    {
        Model m = M(State::OFFLINE, false, false);
        m.wifi_phase = netwifi::Phase::NO_KNOWN;
        CHECK("boot + no known wifi: badge opens setup", hit(m, BTN_WIFI.x + 10, BTN_WIFI.y + 10) == Tap::WIFI);
        m.wifi_phase = netwifi::Phase::SEARCHING;
        CHECK("boot + searching: badge works too (user may want to pick another)", hit(m, BTN_WIFI.x + 10, BTN_WIFI.y + 10) == Tap::WIFI);
        CHECK("boot: call position does nothing", hit(m, BTN_CALL.cx, BTN_CALL.cy) == Tap::NONE);
    }
    CHECK("off-screen coordinates", hit(M(State::IDLE), -5, -5) == Tap::NONE && hit(M(State::IDLE), 5000, 5000) == Tap::NONE);

    // ── 터치 가드: 화면이 바뀐 직후 600ms, 직전 터치 후 400ms 는 버린다 ─────
    {
        TapGuard g;
        CHECK("guard: first tap passes", g.allow(10000, 5000));
        CHECK("guard: second tap within 400ms dropped", !g.allow(10300, 5000));
        CHECK("guard: after 400ms passes", g.allow(10400, 5000));
        CHECK("guard: within 600ms of a screen change dropped", !g.allow(20500, 20000));
        CHECK("guard: 600ms after a screen change passes", g.allow(20600, 20000));
        CHECK("guard: a dropped tap does not extend the wait", (g.allow(30000, 0), !g.allow(30100, 0)) && g.allow(30400, 0));
    }
    {   // Wi-Fi 자판은 빨리 치므로 짧은 가드(화면 300ms·반복 150ms)를 쓴다
        TapGuard g;
        CHECK("keyboard guard: first key", g.allow(5000, 0, KEY_SCREEN_GUARD_MS, KEY_REPEAT_GUARD_MS));
        CHECK("keyboard guard: bounce within 150ms dropped", !g.allow(5100, 0, KEY_SCREEN_GUARD_MS, KEY_REPEAT_GUARD_MS));
        CHECK("keyboard guard: next key after 150ms accepted", g.allow(5150, 0, KEY_SCREEN_GUARD_MS, KEY_REPEAT_GUARD_MS));
        CHECK("keyboard guard: 300ms after the screen opens", !g.allow(9200, 9000, KEY_SCREEN_GUARD_MS, KEY_REPEAT_GUARD_MS) && g.allow(9300, 9000, KEY_SCREEN_GUARD_MS, KEY_REPEAT_GUARD_MS));
        CHECK("keyboard guard is shorter than the call-button guard", KEY_REPEAT_GUARD_MS < TAP_REPEAT_GUARD_MS && KEY_SCREEN_GUARD_MS < TAP_SCREEN_GUARD_MS);
    }

    // ── 통화 시간 표시 ──────────────────────────────────────────────────────
    {
        int mm = -1, ss = -1;
        talk_clock(0, &mm, &ss);        CHECK("clock 00:00", mm == 0 && ss == 0);
        talk_clock(75, &mm, &ss);       CHECK("clock 01:15", mm == 1 && ss == 15);
        talk_clock(5999, &mm, &ss);     CHECK("clock 99:59", mm == 99 && ss == 59);
        talk_clock(6000, &mm, &ss);     CHECK("clock caps at 99:59", mm == 99 && ss == 59);
        talk_clock(-5, &mm, &ss);       CHECK("clock negative -> 00:00", mm == 0 && ss == 0);
    }


    // ════ 수신거부 메시지·벨 끄기 (설계서 decline-mute §5.4) ════
    using callfsm::Notice;
    const Model inc = M(State::INCOMING);
    Model pick = M(State::INCOMING); pick.decline_open = true;
    const State others[] = { State::OFFLINE, State::IDLE, State::DIALING, State::RINGBACK, State::ANSWERING, State::IN_CALL, State::ENDING };

    // ── 메시지 고르기 화면은 INCOMING 위에 얹는 화면 쪽 상태 ──
    CHECK("incoming + picker open = DECLINE screen", screen_for(inc) == Screen::INCOMING && screen_for(pick) == Screen::DECLINE && std::string(screen_name(Screen::DECLINE)) == "DECLINE");
    {
        bool all = true;
        for (State s : others) { Model a = M(s), b = M(s); b.decline_open = true; all = all && screen_for(a) == screen_for(b) && screen_for(b) != Screen::DECLINE; }
        CHECK("picker flag means nothing outside INCOMING", all);
    }

    // ── 새 버튼의 크기·격자·겹침 ──
    {
        const Rect news[] = { BTN_MUTE, BTN_DECLINE, BTN_MSG[0], BTN_MSG[1], BTN_MSG[2], BTN_BACK };
        bool geo = true;
        for (const Rect &r : news) geo = geo && r.w >= 96 && r.h >= 96 && r.x % 8 == 0 && r.y % 8 == 0 && r.w % 8 == 0 && r.h % 8 == 0 && inside_screen(r);
        CHECK("(4) new buttons: >= 96px, on the 8px grid, inside the screen", geo && sizeof BTN_MSG / sizeof BTN_MSG[0] == callfsm::MSG_COUNT);
        CHECK("(4) bell screen: the four buttons do not overlap", !overlap(BTN_MUTE, BTN_ACCEPT) && !overlap(BTN_MUTE, BTN_REJECT) && !overlap(BTN_DECLINE, BTN_ACCEPT)
              && !overlap(BTN_DECLINE, BTN_REJECT) && !overlap(BTN_MUTE, BTN_DECLINE) && BTN_DECLINE.x - (BTN_MUTE.x + BTN_MUTE.w) >= 96);
        CHECK("(4) picker screen: the four buttons do not overlap", !overlap(BTN_MSG[0], BTN_MSG[1]) && !overlap(BTN_MSG[1], BTN_MSG[2]) && !overlap(BTN_MSG[0], BTN_MSG[2])
              && !overlap(BTN_BACK, BTN_MSG[0]) && !overlap(BTN_BACK, BTN_MSG[1]) && !overlap(BTN_BACK, BTN_MSG[2]));
        bool r1 = !overlap(BTN_DECLINE, BTN_BACK), r2 = true;
        for (int i = 0; i < callfsm::MSG_COUNT; ++i) {
            r1 = r1 && !overlap(BTN_DECLINE, BTN_MSG[i]);
            r2 = r2 && !overlap(BTN_MSG[i], box(BTN_CALL)) && !overlap(BTN_MSG[i], BTN_WIFI) && !overlap(BTN_MSG[i], BTN_ACCEPT) && !overlap(BTN_MSG[i], BTN_REJECT)
                    && !overlap(BTN_MSG[i], BTN_MUTE) && !overlap(BTN_MSG[i], BTN_DECLINE) && !overlap(BTN_MSG[i], BTN_CANCEL) && !overlap(BTN_MSG[i], BTN_HANGUP);
        }
        CHECK("(1) open-picker overlaps no picker button (open then instant message)", r1);
        CHECK("(2) message buttons overlap no button of any call screen (picking while the caller cancels)", r2 && BTN_MSG[0].y + BTN_MSG[0].h <= BTN_CALL.cy - BTN_CALL.r);
        CHECK("(2) mute/open-picker/back do not overlap the call button (screen turns idle under the finger)", !overlap(BTN_MUTE, box(BTN_CALL)) && !overlap(BTN_DECLINE, box(BTN_CALL)) && !overlap(BTN_BACK, box(BTN_CALL)));
        CHECK("(3) back overlaps no accept/reject/open-picker (back then instant accept or reject)", !overlap(BTN_BACK, BTN_ACCEPT) && !overlap(BTN_BACK, BTN_REJECT) && !overlap(BTN_BACK, BTN_DECLINE));
    }

    // ── 터치 → 버튼 (가운데 좌표는 기기 시험이 주입하는 값과 같다) ──
    {
        CHECK("incoming: accept, reject, mute, open-picker", hit(inc, 404, 428) == Tap::ACCEPT && hit(inc, 876, 428) == Tap::REJECT && hit(inc, 404, 624) == Tap::MUTE && hit(inc, 876, 624) == Tap::DECLINE_OPEN);
        CHECK("incoming: gap between mute and open-picker", hit(inc, 640, 624) == Tap::NONE);
        Model muted = inc; muted.bell_muted = true;
        CHECK("incoming + muted: the mute position is no longer a button", hit(muted, 404, 624) == Tap::NONE && hit(muted, BTN_MUTE.x + 1, BTN_MUTE.y + 1) == Tap::NONE);
        CHECK("incoming + muted: the other three still work", hit(muted, 404, 428) == Tap::ACCEPT && hit(muted, 876, 428) == Tap::REJECT && hit(muted, 876, 624) == Tap::DECLINE_OPEN);
        CHECK("incoming: message positions do nothing", hit(inc, 240, 168) == Tap::NONE && hit(inc, 640, 168) == Tap::NONE && hit(inc, 1040, 168) == Tap::NONE);
        CHECK("picker: three messages and back", hit(pick, 240, 168) == Tap::MSG1 && hit(pick, 640, 168) == Tap::MSG2 && hit(pick, 1040, 168) == Tap::MSG3 && hit(pick, 404, 624) == Tap::DECLINE_BACK);
        CHECK("picker: gaps between messages", hit(pick, 440, 168) == Tap::NONE && hit(pick, 840, 168) == Tap::NONE && hit(pick, 640, 100) == Tap::NONE && hit(pick, 640, 232) == Tap::NONE);
        CHECK("picker: accept/reject/open-picker positions do nothing", hit(pick, 404, 428) == Tap::NONE && hit(pick, 876, 428) == Tap::NONE && hit(pick, 876, 624) == Tap::NONE);
        Model pm = pick; pm.bell_muted = true;
        CHECK("picker + muted: back still works", hit(pm, 404, 624) == Tap::DECLINE_BACK && hit(pm, 640, 168) == Tap::MSG2);
        bool none = true;
        const int pts[][2] = { { 240, 168 }, { 640, 168 }, { 1040, 168 }, { 404, 624 }, { 876, 624 } };
        for (State s : { State::IDLE, State::ANSWERING, State::ENDING, State::OFFLINE }) {
            Model m = M(s); m.decline_open = true; m.bell_muted = true;           // 남아 있는 화면 쪽 값이 있어도
            for (const auto &p : pts) none = none && hit(m, p[0], p[1]) == Tap::NONE;
        }
        for (int i = 0; i < 3; ++i) none = none && hit(M(State::IN_CALL), pts[i][0], pts[i][1]) == Tap::NONE && hit(M(State::DIALING), pts[i][0], pts[i][1]) == Tap::NONE;
        CHECK("idle/in-call/ending and the rest: new positions do nothing", none);
    }

    // ── 순수 로직 넷: msg_of, DeclinePicker, notice_ms, notice_visible ──
    CHECK("msg_of", msg_of(Tap::MSG1) == 1 && msg_of(Tap::MSG2) == 2 && msg_of(Tap::MSG3) == 3 && msg_of(Tap::REJECT) == 0 && msg_of(Tap::NONE) == 0 && msg_of(Tap::DECLINE_BACK) == 0 && msg_of(Tap::MUTE) == 0);
    {
        DeclinePicker p;
        p.on_tap(Tap::ACCEPT); p.on_tap(Tap::REJECT); p.on_tap(Tap::MUTE); p.on_tap(Tap::MSG1); p.on_tap(Tap::DECLINE_BACK);
        CHECK("picker: other taps do not open it", !p.open);
        p.on_tap(Tap::DECLINE_OPEN); p.sync(State::INCOMING); p.on_tap(Tap::MUTE);
        CHECK("picker: opens, stays open while INCOMING", p.open);
        p.on_tap(Tap::DECLINE_BACK);
        CHECK("picker: back closes", !p.open);
        bool closes = true;
        for (State s : others) { DeclinePicker q; q.on_tap(Tap::DECLINE_OPEN); q.sync(s); closes = closes && !q.open; }
        CHECK("picker: closes when the call leaves INCOMING", closes);
        p.on_tap(Tap::DECLINE_OPEN); p.sync(State::IDLE); p.sync(State::INCOMING);          // 한 바퀴 안에 ended → ring
        CHECK("picker: ended then ring -> the next call starts on the bell screen", !p.open);
    }
    CHECK("notice time: message 10s, others 4s", notice_ms(Notice::REJECT_MSG) == 10000 && notice_ms(Notice::REJECTED) == 4000 && notice_ms(Notice::MISSED) == 4000
          && notice_ms(Notice::PEER_OFF) == 4000 && notice_ms(Notice::NO_ANSWER) == 4000 && notice_ms(Notice::LOST) == 4000 && notice_ms(Notice::CALL_ENDED) == 4000 && notice_ms(Notice::RELAY_SILENT) == 4000);
    {
        Model on_bell = inc; on_bell.notice = Notice::REJECT_MSG; on_bell.notice_msg = 2;
        Model on_pick = pick; on_pick.notice = Notice::REJECT_MSG; on_pick.notice_msg = 2;
        Model on_idle = M(State::IDLE); on_idle.notice = Notice::MISSED; on_idle.decline_open = true;
        // UL-D3(토이폰 테마): 벨 화면에서도 가린다 — 안내 자리에 전화 건 캐릭터가 선다
        CHECK("notice box hidden on the bell and picker screens", !notice_visible(on_bell) && !notice_visible(on_pick) && notice_visible(on_idle) && !notice_visible(inc) && !notice_visible(pick));
    }
    {   // I-1: 비밀번호가 한 글자라도 든 자판 화면은 스크린샷으로 내보내지 않는다(빈 자판과 그 밖의 화면은 찍힌다)
        CHECK("private frame: not keyboard, empty -> public", !is_private_frame(false, 0));
        CHECK("private frame: not keyboard, text left over -> public", !is_private_frame(false, 5));
        CHECK("private frame: keyboard, empty -> public", !is_private_frame(true, 0));
        CHECK("private frame: keyboard, 1 char -> private", is_private_frame(true, 1));
        CHECK("private frame: keyboard, long input -> private", is_private_frame(true, 63));
    }

    // ════ 음량 버튼 (P4V — 대기·통화 화면의 오른쪽 세로 줄: 크게 / 단계 막대 / 작게) ════
    {
        const Rect vols[] = { BTN_VOL_UP, BTN_VOL_DN };
        bool geo = true;
        for (const Rect &r : vols) geo = geo && r.w >= 144 && r.h >= 144 && r.x % 8 == 0 && r.y % 8 == 0 && r.w % 8 == 0 && r.h % 8 == 0 && inside_screen(r);
        CHECK("vol: both buttons >= 144px (a child's finger), on the 8px grid, inside the screen", geo);
        CHECK("vol: louder is above quieter, the bars sit between, the two are >= 96px apart", BTN_VOL_UP.y + BTN_VOL_UP.h <= VOL_BARS.y && VOL_BARS.y + VOL_BARS.h <= BTN_VOL_DN.y
              && BTN_VOL_DN.y - (BTN_VOL_UP.y + BTN_VOL_UP.h) >= 96 && !overlap(BTN_VOL_UP, BTN_VOL_DN) && !overlap(VOL_BARS, BTN_VOL_UP) && !overlap(VOL_BARS, BTN_VOL_DN));
        const Rect olds[] = { box(BTN_CALL), box(BTN_CALL_B), BTN_WIFI, BTN_CANCEL, BTN_ACCEPT, BTN_REJECT, BTN_HANGUP, BTN_MUTE, BTN_DECLINE, BTN_MSG[0], BTN_MSG[1], BTN_MSG[2], BTN_BACK };
        bool apart = true;
        for (const Rect &o : olds) apart = apart && !overlap(VOL_AREA, o);
        CHECK("vol: the whole cell overlaps no button of any screen (the screen may turn under the finger)", apart);
        const Rect parts[] = { BTN_VOL_UP, VOL_BARS, BTN_VOL_DN };
        bool holds = inside_screen(VOL_AREA);
        for (const Rect &p : parts) holds = holds && p.x >= VOL_AREA.x && p.y >= VOL_AREA.y && p.x + p.w <= VOL_AREA.x + VOL_AREA.w && p.y + p.h <= VOL_AREA.y + VOL_AREA.h;
        CHECK("vol: the redraw cell holds both buttons and the bars", holds);
        CHECK("vol: the cell is below the notice band (top row, y 14..78) and right of the clock/hangup -> a partial redraw paints over nothing else", VOL_AREA.y >= 168 && VOL_AREA.y >= NOTICE_BAND.y + NOTICE_BAND.h && VOL_AREA.x >= BTN_HANGUP.x + BTN_HANGUP.w && VOL_AREA.x >= 896);
    }
    {
        const int ux = 1168, uy = 312, dx = 1168, dy = 608;      // 기기 시험이 주입하는 좌표 = 버튼 가운데
        CHECK("vol: the injected coordinates are the button centers", ux == BTN_VOL_UP.x + BTN_VOL_UP.w / 2 && uy == BTN_VOL_UP.y + BTN_VOL_UP.h / 2 && dx == BTN_VOL_DN.x + BTN_VOL_DN.w / 2 && dy == BTN_VOL_DN.y + BTN_VOL_DN.h / 2);
        CHECK("vol: idle screen has louder and quieter", hit(M(State::IDLE), ux, uy) == Tap::VOL_UP && hit(M(State::IDLE), dx, dy) == Tap::VOL_DN);
        CHECK("vol: call screen has them at the same place", hit(M(State::IN_CALL), ux, uy) == Tap::VOL_UP && hit(M(State::IN_CALL), dx, dy) == Tap::VOL_DN);
        CHECK("vol: idle with the peer off too", hit(M(State::IDLE, true, true, false), ux, uy) == Tap::VOL_UP && hit(M(State::IDLE, true, true, false), dx, dy) == Tap::VOL_DN);
        CHECK("vol: button edges", hit(M(State::IDLE), BTN_VOL_UP.x, BTN_VOL_UP.y) == Tap::VOL_UP && hit(M(State::IDLE), BTN_VOL_UP.x + BTN_VOL_UP.w - 1, BTN_VOL_UP.y + BTN_VOL_UP.h - 1) == Tap::VOL_UP
              && hit(M(State::IDLE), BTN_VOL_UP.x - 1, uy) == Tap::NONE && hit(M(State::IDLE), ux, BTN_VOL_UP.y - 1) == Tap::NONE && hit(M(State::IDLE), ux, BTN_VOL_DN.y + BTN_VOL_DN.h) == Tap::NONE);
        CHECK("vol: the bars between the two are not a button", hit(M(State::IDLE), ux, VOL_BARS.y + VOL_BARS.h / 2) == Tap::NONE && hit(M(State::IN_CALL), ux, VOL_BARS.y + VOL_BARS.h / 2) == Tap::NONE);
        bool none = true;
        Model pick2 = M(State::INCOMING); pick2.decline_open = true;
        for (const Model &m : { M(State::OFFLINE, false, false), M(State::DIALING), M(State::RINGBACK), M(State::INCOMING), pick2, M(State::ANSWERING), M(State::ENDING) })
            none = none && hit(m, ux, uy) == Tap::NONE && hit(m, dx, dy) == Tap::NONE && !volume_visible(m);
        CHECK("vol: boot/dialing/bell/picker/answering/ending have no volume buttons", none);
        CHECK("vol: shown exactly where it can be tapped", volume_visible(M(State::IDLE)) && volume_visible(M(State::IN_CALL)));
        Model top = M(State::IN_CALL), low = M(State::IN_CALL); top.vol_step = volsteps::STEP_MAX; low.vol_step = volsteps::STEP_MIN;
        CHECK("vol: at either end the buttons still report the tap (the step logic stops, like CALL with the peer off)", hit(top, ux, uy) == Tap::VOL_UP && hit(low, dx, dy) == Tap::VOL_DN);
        CHECK("vol: the old buttons still work beside them", hit(M(State::IDLE), BTN_CALL.cx, BTN_CALL.cy) == Tap::CALL && hit(M(State::IDLE), BTN_WIFI.x + 10, BTN_WIFI.y + 10) == Tap::WIFI
              && hit(M(State::IN_CALL), BTN_HANGUP.x + 5, BTN_HANGUP.y + 5) == Tap::HANGUP && hit(M(State::IN_CALL), 644, 606) == Tap::HANGUP);
        CHECK("vol: tap -> request", vol_req_of(Tap::VOL_UP) == volsteps::Req::UP && vol_req_of(Tap::VOL_DN) == volsteps::Req::DOWN && vol_req_of(Tap::CALL) == volsteps::Req::NONE
              && vol_req_of(Tap::HANGUP) == volsteps::Req::NONE && vol_req_of(Tap::NONE) == volsteps::Req::NONE && vol_req_of(Tap::MSG3) == volsteps::Req::NONE);
        CHECK("vol: the new taps are appended after MSG3 (older numbers unchanged)", (int)Tap::VOL_DN == (int)Tap::MSG3 + 1 && (int)Tap::VOL_UP == (int)Tap::MSG3 + 2 && msg_of(Tap::VOL_DN) == 0 && msg_of(Tap::VOL_UP) == 0);
        CHECK("vol: a fresh model shows the default step (top)", Model().vol_step == volsteps::STEP_DEFAULT);
        DeclinePicker p; p.on_tap(Tap::VOL_UP); p.on_tap(Tap::VOL_DN);
        CHECK("vol: volume taps do not open the message picker", !p.open);
    }

    // ════ 전화기 3대 — 대기 화면의 상대 둘과 통화 키캡 둘 (설계서 2026-10-08-phone-3way-design.md §9, ui_layout.h UL-D4) ════
    {
        const Circle calls[] = { BTN_CALL, BTN_CALL_B };
        const Rect screen{ 0, 0, SCREEN_W, SCREEN_H };
        // ── 크기 · 화면 안 · 서로 ──
        CHECK("3way: the table of call buttons is the two named buttons, one per peer slot", MAX_PEERS == 2 && sizeof BTN_CALLS / sizeof BTN_CALLS[0] == MAX_PEERS
              && BTN_CALLS[0].cx == BTN_CALL.cx && BTN_CALLS[0].cy == BTN_CALL.cy && BTN_CALLS[0].r == BTN_CALL.r
              && BTN_CALLS[1].cx == BTN_CALL_B.cx && BTN_CALLS[1].cy == BTN_CALL_B.cy && BTN_CALLS[1].r == BTN_CALL_B.r);
        CHECK("3way: both call buttons are the same size, on one line, on the 8px grid, inside the screen", BTN_CALL.r == BTN_CALL_B.r && BTN_CALL.cy == BTN_CALL_B.cy
              && BTN_CALL.cx % 8 == 0 && BTN_CALL.cy % 8 == 0 && BTN_CALL.r % 8 == 0 && BTN_CALL_B.cx % 8 == 0 && inside_screen(box(BTN_CALL)) && inside_screen(box(BTN_CALL_B)));
        CHECK("3way: slot 0 is on the left, slot 1 on the right, mirrored about the middle of the screen", BTN_CALL.cx < BTN_CALL_B.cx && BTN_CALL.cx + BTN_CALL_B.cx == SCREEN_W);
        CHECK("3way: the two call buttons are far apart (a wrong friend is not one slip away): >= 96px", !overlap(box(BTN_CALL), box(BTN_CALL_B)) && (BTN_CALL_B.cx - BTN_CALL_B.r) - (BTN_CALL.cx + BTN_CALL.r) >= 96);
        // ── UL-D1 을 둘 다 지킨다: 화면이 손가락 밑에서 바뀌어도 다른 화면의 버튼이 아니다 ──
        bool apart = true, below_msgs = true;
        const Rect others_btns[] = { BTN_CANCEL, BTN_HANGUP, BTN_MUTE, BTN_DECLINE, BTN_BACK, BTN_MSG[0], BTN_MSG[1], BTN_MSG[2], BTN_WIFI, VOL_AREA };
        for (const Circle &c : calls) {
            for (const Rect &o : others_btns) apart = apart && !overlap(box(c), o);
            below_msgs = below_msgs && BTN_MSG[0].y + BTN_MSG[0].h <= c.cy - c.r;
        }
        CHECK("3way: neither call button overlaps cancel/hangup (call then instant cancel), mute/back/open-picker/messages (the bell screen turns idle under the finger), wifi, volume", apart && below_msgs);
        {   // 상자가 아니라 실제 원으로도: 통화 키캡 안의 어떤 점도 다른 화면의 그 버튼들 안에 있지 않다(가장자리 한 점까지)
            bool clean = true;
            for (const Circle &c : calls)
                for (int y = c.cy - c.r; y <= c.cy + c.r; ++y)
                    for (int x = c.cx - c.r; x <= c.cx + c.r; ++x) {
                        if (!in_circle(c, x, y)) continue;
                        for (const Rect &o : others_btns) clean = clean && !o.hit(x, y);
                    }
            CHECK("3way: pixel by pixel, no point of a call button lies in those buttons", clean);
        }
        // ── 상대 줄의 칸(그리기용 자리): 이름 알약 · 캐릭터 · 통화 키캡이 위에서부터 한 줄 ──
        bool column = true, clear = true;
        for (int s = 0; s < MAX_PEERS; ++s) {
            const Rect &n = PEER_NAME[s], &b = PEER_BODY[s]; const Rect k = box(BTN_CALLS[s]);
            column = column && n.y + n.h <= b.y && b.y + b.h <= k.y                                   // 위에서부터: 이름 → 캐릭터 → 키캡
                  && n.x + n.w / 2 == BTN_CALLS[s].cx && b.x + b.w / 2 == BTN_CALLS[s].cx            // 같은 세로줄
                  && inside_screen(n) && inside_screen(b) && n.h >= 48 && b.w >= 2 * BTN_CALLS[s].r;  // 캐릭터 칸이 키캡보다 좁지 않다
            const Rect keep[] = { NOTICE_BAND, UNIT_TAG, BTN_WIFI, MISSED_AREA, VOL_AREA, Rect{ VOL_AREA.x - 16, VOL_AREA.y - 16, VOL_AREA.w + 32, VOL_AREA.h + 32 } };
            for (const Rect &o : keep) clear = clear && !overlap(n, o) && !overlap(b, o) && !overlap(k, o);
        }
        CHECK("3way: each peer column reads top-down: name pill, character, call button - on one axis", column);
        CHECK("3way: the peer columns stay clear of the notice band, my tag, wifi, the missed-call sign, the volume panel", clear);
        CHECK("3way: the two columns do not touch each other", !overlap(PEER_NAME[0], PEER_NAME[1]) && !overlap(PEER_BODY[0], PEER_BODY[1]) && PEER_BODY[0].x + PEER_BODY[0].w <= PEER_BODY[1].x - 96);
        CHECK("3way: the notice band is the top row, right of my tag, above everything of the idle screen", rect_holds(screen, NOTICE_BAND) && rect_holds(screen, UNIT_TAG) && !overlap(NOTICE_BAND, UNIT_TAG)
              && NOTICE_BAND.x + NOTICE_BAND.w / 2 == SCREEN_W / 2 && NOTICE_BAND.y + NOTICE_BAND.h <= PEER_NAME[0].y && NOTICE_BAND.y + NOTICE_BAND.h <= BTN_MSG[0].y);
        CHECK("3way: the missed-call sign keeps its row (below the call buttons, between wifi and volume)", rect_holds(screen, MISSED_AREA) && !overlap(MISSED_AREA, BTN_WIFI) && !overlap(MISSED_AREA, VOL_AREA)
              && MISSED_AREA.x + MISSED_AREA.w / 2 <= SCREEN_W / 2 && MISSED_AREA.y >= BTN_CALL.cy + BTN_CALL.r);
    }
    {   // ── 터치 → 어느 상대 ──
        const Model m = M(State::IDLE);
        CHECK("3way: left button = CALL = slot 0, right button = CALL_B = slot 1", hit(m, BTN_CALL.cx, BTN_CALL.cy) == Tap::CALL && hit(m, BTN_CALL_B.cx, BTN_CALL_B.cy) == Tap::CALL_B
              && call_slot_of(Tap::CALL) == 0 && call_slot_of(Tap::CALL_B) == 1);
        CHECK("3way: the injected coordinates are the button centers", hit(m, 368, 472) == Tap::CALL && hit(m, 912, 472) == Tap::CALL_B && BTN_CALL.cx == 368 && BTN_CALL.cy == 472 && BTN_CALL_B.cx == 912 && BTN_CALL_B.cy == 472);
        bool none = true;
        for (Tap t : { Tap::NONE, Tap::CANCEL, Tap::ACCEPT, Tap::REJECT, Tap::HANGUP, Tap::WIFI, Tap::MUTE, Tap::DECLINE_OPEN, Tap::DECLINE_BACK, Tap::MSG1, Tap::MSG2, Tap::MSG3, Tap::VOL_DN, Tap::VOL_UP })
            none = none && call_slot_of(t) == -1;
        CHECK("3way: no other tap is a call", none);
        CHECK("3way: CALL_B is appended after VOL_UP (older numbers unchanged)", (int)Tap::CALL_B == (int)Tap::VOL_UP + 1 && (int)Tap::CALL == 1 && msg_of(Tap::CALL_B) == 0 && vol_req_of(Tap::CALL_B) == volsteps::Req::NONE);
        CHECK("3way: circle edges", hit(m, BTN_CALL_B.cx + BTN_CALL_B.r, BTN_CALL_B.cy) == Tap::CALL_B && hit(m, BTN_CALL_B.cx + BTN_CALL_B.r + 1, BTN_CALL_B.cy) == Tap::NONE
              && hit(m, BTN_CALL.cx, BTN_CALL.cy - BTN_CALL.r) == Tap::CALL && hit(m, BTN_CALL.cx, BTN_CALL.cy - BTN_CALL.r - 1) == Tap::NONE
              && hit(m, BTN_CALL_B.cx - BTN_CALL_B.r, BTN_CALL_B.cy - BTN_CALL_B.r) == Tap::NONE);
        CHECK("3way: the middle of the screen (the old call button) and the gap between the two do nothing", hit(m, 640, 360) == Tap::NONE && hit(m, 640, 472) == Tap::NONE);
        CHECK("3way: the name pills and the characters are not buttons", hit(m, PEER_NAME[0].x + PEER_NAME[0].w / 2, PEER_NAME[0].y + PEER_NAME[0].h / 2) == Tap::NONE
              && hit(m, PEER_BODY[0].x + PEER_BODY[0].w / 2, PEER_BODY[0].y + PEER_BODY[0].h / 2) == Tap::NONE
              && hit(m, PEER_NAME[1].x + PEER_NAME[1].w / 2, PEER_NAME[1].y + PEER_NAME[1].h / 2) == Tap::NONE
              && hit(m, PEER_BODY[1].x + PEER_BODY[1].w / 2, PEER_BODY[1].y + PEER_BODY[1].h / 2) == Tap::NONE);
        // 꺼진 상대의 키캡도 눌린다 — 안내("꺼져 있어요")는 상태 기계가 낸다. 네 조합 모두
        bool off_ok = true;
        for (int mask = 0; mask < 4; ++mask) {
            Model o = M(State::IDLE); o.peer_online[0] = (mask & 1) != 0; o.peer_online[1] = (mask & 2) != 0;
            off_ok = off_ok && hit(o, BTN_CALL.cx, BTN_CALL.cy) == Tap::CALL && hit(o, BTN_CALL_B.cx, BTN_CALL_B.cy) == Tap::CALL_B && peer_on(o, 0) == ((mask & 1) != 0) && peer_on(o, 1) == ((mask & 2) != 0);
        }
        CHECK("3way: a sleeping peer's button still reports its tap, in all four on/off combinations", off_ok);
        CHECK("3way: peer_on outside the slots is false", !peer_on(m, -1) && !peer_on(m, MAX_PEERS) && !peer_on(m, 200));
    }
    {   // ── 누가 어디에 서나: 왼쪽 = slot 0 = 번호가 작은 쪽 (roster.h RS-D1 을 화면이 그대로 쓴다) ──
        bool order = true, table = true;
        const int want[4][2] = { { 0, 0 }, { 2, 3 }, { 1, 3 }, { 1, 2 } };
        for (int u = 1; u <= 3; ++u) {
            Model m = M(State::IDLE); m.unit = u;
            order = order && peer_unit(m, 0) < peer_unit(m, 1) && peer_unit(m, 0) != u && peer_unit(m, 1) != u && peer_unit(m, 0) >= 1 && peer_unit(m, 1) <= 3;
            table = table && peer_unit(m, 0) == want[u][0] && peer_unit(m, 1) == want[u][1] && peer_unit(m, 2) == 0 && peer_unit(m, -1) == 0;
        }
        CHECK("3way: on every unit the left peer has the smaller number and neither is me", order);
        CHECK("3way: unit 1 sees 2 and 3, unit 2 sees 1 and 3, unit 3 sees 1 and 2", table);
        bool who = true;
        for (int u = 1; u <= 3; ++u)
            for (int s = 0; s < MAX_PEERS; ++s) { Model m = M(State::INCOMING); m.unit = u; m.call_peer = (uint8_t)s; who = who && call_unit(m) == want[u][s] && call_unit(m) == peer_unit(m, s); }
        CHECK("3way: the peer of the call (bell, dialing, talking screens) is the unit in that slot", who);
        Model stray = M(State::IN_CALL); stray.call_peer = 7;
        CHECK("3way: a slot out of range has no unit (the screen draws no stranger)", call_unit(stray) == 0);
    }
    {   // ── 미등록 기기(번호 0): 상대가 없다 → 대기 화면에 통화 키캡이 없다(그리지 않는 것은 눌리지 않는다) ──
        Model m = M(State::IDLE); m.unit = 0;
        CHECK("3way: an unregistered unit has no peers and no call buttons", peer_unit(m, 0) == 0 && peer_unit(m, 1) == 0 && !peer_shown(m, 0) && !peer_shown(m, 1)
              && hit(m, BTN_CALL.cx, BTN_CALL.cy) == Tap::NONE && hit(m, BTN_CALL_B.cx, BTN_CALL_B.cy) == Tap::NONE);
        CHECK("3way: ... but wifi and volume still work there", hit(m, BTN_WIFI.x + 10, BTN_WIFI.y + 10) == Tap::WIFI && hit(m, 1168, 312) == Tap::VOL_UP);
        Model r = M(State::IDLE); r.unit = 3;
        CHECK("3way: units 1..3 show both peers", peer_shown(r, 0) && peer_shown(r, 1) && peer_shown(M(State::IDLE), 0) && peer_shown(M(State::IDLE), 1) && !peer_shown(r, 2) && !peer_shown(r, -1));
    }
    {   // ── 화면마다 그 화면의 버튼만 눌린다: 화면 전체를 훑어, 나온 버튼이 그 화면의 것인지 본다(내 번호 1·2·3, 벨 끔·고르기 열림 포함) ──
        bool only = true; int calls_seen = 0, calls_b_seen = 0;
        const State all[] = { State::OFFLINE, State::IDLE, State::DIALING, State::RINGBACK, State::INCOMING, State::ANSWERING, State::IN_CALL, State::ENDING };
        for (int u = 1; u <= 3; ++u)
            for (State st : all)
                for (int variant = 0; variant < 4; ++variant) {
                    Model m = M(st); m.unit = u; m.decline_open = (variant & 1) != 0; m.bell_muted = (variant & 2) != 0; m.peer_online[1] = false; m.call_peer = (uint8_t)(variant & 1);
                    const Screen sc = screen_for(m);
                    for (int y = 0; y < SCREEN_H; y += 4)
                        for (int x = 0; x < SCREEN_W; x += 4) {
                            const Tap t = hit(m, x, y);
                            bool fits = t == Tap::NONE;
                            switch (sc) {
                            case Screen::BOOT:     fits = fits || t == Tap::WIFI; break;
                            case Screen::IDLE:     fits = fits || t == Tap::CALL || t == Tap::CALL_B || t == Tap::WIFI || t == Tap::VOL_UP || t == Tap::VOL_DN; break;
                            case Screen::DIALING:  fits = fits || t == Tap::CANCEL; break;
                            case Screen::INCOMING: fits = fits || t == Tap::ACCEPT || t == Tap::REJECT || t == Tap::DECLINE_OPEN || (t == Tap::MUTE && !m.bell_muted); break;
                            case Screen::DECLINE:  fits = fits || t == Tap::MSG1 || t == Tap::MSG2 || t == Tap::MSG3 || t == Tap::DECLINE_BACK; break;
                            case Screen::IN_CALL:  fits = fits || t == Tap::HANGUP || t == Tap::VOL_UP || t == Tap::VOL_DN; break;
                            case Screen::ANSWERING:
                            case Screen::ENDING:   break;
                            }
                            only = only && fits;
                            if (t == Tap::CALL) ++calls_seen;
                            if (t == Tap::CALL_B) ++calls_b_seen;
                        }
                }
        CHECK("3way: every screen answers only with its own buttons (whole screen scanned, units 1..3)", only);
        CHECK("3way: both call buttons were found by that scan, and equally often (same size)", calls_seen > 0 && calls_seen == calls_b_seen);
    }
    CHECK("3way: the busy notice stays up as long as the other short notices", notice_ms(Notice::BUSY) == 4000);
    {
        Model busy = M(State::IDLE); busy.notice = Notice::BUSY;
        Model on_bell = M(State::INCOMING); on_bell.notice = Notice::BUSY;
        CHECK("3way: the busy notice shows on the idle screen, not on the bell screen", notice_visible(busy) && !notice_visible(on_bell));
    }
    CHECK("3way: a fresh model has no peer on and points at slot 0", !Model().peer_online[0] && !Model().peer_online[1] && Model().call_peer == 0 && Model().unit == 0);

    // ════ 화면이 손가락 밑에서 바뀔 때: 같은 자리가 다른 버튼이 되는 쌍을 전부 찾아 고정한다 (UL-D1·UL-D4·UL-D7) ════
    //   화면 둘을 고르고 모든 점을 눌러 본다. 두 화면 모두에서 버튼이고 그 둘이 서로 다른 버튼이면 "겹치는 쌍"이다. 이 표가 달라지면(새 겹침·사라진 겹침) 이 시험이 깨진다
    //   → 새 버튼을 놓을 때 "어느 화면의 무엇 위에 놓였나"를 알고 정하게 된다.
    {
        static const char *const TAP_NAME[] = { "NONE", "CALL", "CANCEL", "ACCEPT", "REJECT", "HANGUP", "WIFI", "MUTE", "DECLINE_OPEN", "DECLINE_BACK", "MSG1", "MSG2", "MSG3", "VOL_DN", "VOL_UP", "CALL_B" };
        CHECK("swap: the tap name table covers the whole enum", sizeof TAP_NAME / sizeof TAP_NAME[0] == (size_t)Tap::CALL_B + 1);
        Model picker = M(State::INCOMING); picker.decline_open = true;
        const Model views[] = { M(State::OFFLINE, false, false), M(State::IDLE), M(State::DIALING), M(State::INCOMING), picker, M(State::ANSWERING), M(State::IN_CALL), M(State::ENDING) };
        const size_t NV = sizeof views / sizeof views[0];
        std::set<std::string> found;
        std::set<Screen> over_call[MAX_PEERS];                   // 통화 키캡 자리에 다른 버튼을 둔 화면
        std::set<Tap> under_call[MAX_PEERS];                     // 그 버튼
        std::set<Screen> distinct;
        for (size_t a = 0; a < NV; ++a) distinct.insert(screen_for(views[a]));
        for (size_t a = 0; a < NV; ++a)
            for (size_t b = a + 1; b < NV; ++b)
                for (int y = 0; y < SCREEN_H; ++y)
                    for (int x = 0; x < SCREEN_W; ++x) {
                        const Tap ta = hit(views[a], x, y), tb = hit(views[b], x, y);
                        if (ta == Tap::NONE || tb == Tap::NONE || ta == tb) continue;
                        found.insert(std::string(screen_name(screen_for(views[a]))) + "." + TAP_NAME[(int)ta] + " <-> " + screen_name(screen_for(views[b])) + "." + TAP_NAME[(int)tb]);
                        if (call_slot_of(ta) >= 0) { over_call[call_slot_of(ta)].insert(screen_for(views[b])); under_call[call_slot_of(ta)].insert(tb); }
                        if (call_slot_of(tb) >= 0) { over_call[call_slot_of(tb)].insert(screen_for(views[a])); under_call[call_slot_of(tb)].insert(ta); }
                    }
        const std::set<std::string> expected = {
            // 전화기 3대 전부터 있던 쌍(자리가 그대로다): 같은 뜻이거나(취소 ↔ 끊기, 벨 끄기 ↔ 뒤로 = 같은 칸), 잘못 눌려도 통화를 걸거나 받지 않는다
            "BOOT.WIFI <-> INCOMING.MUTE", "BOOT.WIFI <-> DECLINE.DECLINE_BACK", "IDLE.WIFI <-> INCOMING.MUTE", "IDLE.WIFI <-> DECLINE.DECLINE_BACK",
            "DIALING.CANCEL <-> INCOMING.MUTE", "DIALING.CANCEL <-> INCOMING.DECLINE_OPEN", "DIALING.CANCEL <-> DECLINE.DECLINE_BACK", "DIALING.CANCEL <-> IN_CALL.HANGUP",
            "INCOMING.MUTE <-> DECLINE.DECLINE_BACK", "INCOMING.MUTE <-> IN_CALL.HANGUP", "INCOMING.DECLINE_OPEN <-> IN_CALL.HANGUP", "DECLINE.DECLINE_BACK <-> IN_CALL.HANGUP",
            // 전화기 3대의 통화 키캡(UL-D4 의 ③): 받기·거절과는 겹친다 — 배치로 못 막아 시간 가드로 막는다(UL-D7)
            "IDLE.CALL <-> INCOMING.ACCEPT", "IDLE.CALL_B <-> INCOMING.REJECT",
        };
        CHECK("swap: every screen is in the scan", distinct.size() == NV);
        CHECK("swap: the table of cross-screen overlaps is exactly the known one (a new overlap must be put here on purpose)", found == expected);
        CHECK("swap: a call button shares its place with no button of another screen except accept (left) / reject (right) on the bell screen",
              over_call[0] == std::set<Screen>{ Screen::INCOMING } && over_call[1] == std::set<Screen>{ Screen::INCOMING }
              && under_call[0] == std::set<Tap>{ Tap::ACCEPT } && under_call[1] == std::set<Tap>{ Tap::REJECT });
        // UL-D7: 그 겹침이 있는 화면(전화 옴)이 대기로 바뀐 직후의 통화 터치에만 긴 가드
        bool long_where_needed = true, short_elsewhere = true;
        const Screen all_sc[] = { Screen::BOOT, Screen::IDLE, Screen::DIALING, Screen::INCOMING, Screen::ANSWERING, Screen::IN_CALL, Screen::ENDING, Screen::DECLINE };
        for (Screen prev : all_sc)
            for (int t = 0; t <= (int)Tap::CALL_B; ++t) {
                const int slot = call_slot_of((Tap)t);
                const bool need = slot >= 0 && over_call[slot].count(prev) != 0;
                if (need) long_where_needed = long_where_needed && screen_guard_for((Tap)t, prev) == TAP_AFTER_BELL_GUARD_MS;
                else      short_elsewhere = short_elsewhere && screen_guard_for((Tap)t, prev) == TAP_SCREEN_GUARD_MS;
            }
        CHECK("guard: a call tap right after a screen that had another button on its place gets the long guard", long_where_needed
              && screen_guard_for(Tap::CALL, Screen::INCOMING) == TAP_AFTER_BELL_GUARD_MS && screen_guard_for(Tap::CALL_B, Screen::INCOMING) == TAP_AFTER_BELL_GUARD_MS);
        CHECK("guard: every other tap after every other screen keeps the 600ms guard (accept/reject after idle too - see UL-D7)", short_elsewhere
              && screen_guard_for(Tap::ACCEPT, Screen::IDLE) == TAP_SCREEN_GUARD_MS && screen_guard_for(Tap::REJECT, Screen::IDLE) == TAP_SCREEN_GUARD_MS
              && screen_guard_for(Tap::CALL, Screen::ENDING) == TAP_SCREEN_GUARD_MS && screen_guard_for(Tap::CALL, Screen::DECLINE) == TAP_SCREEN_GUARD_MS
              && screen_guard_for(Tap::WIFI, Screen::INCOMING) == TAP_SCREEN_GUARD_MS && screen_guard_for(Tap::VOL_UP, Screen::INCOMING) == TAP_SCREEN_GUARD_MS);
        CHECK("guard: the long guard is longer than the usual one and shorter than a notice", TAP_AFTER_BELL_GUARD_MS >= 2 * TAP_SCREEN_GUARD_MS && TAP_AFTER_BELL_GUARD_MS < NOTICE_MS);
    }
    {   // 벨이 그친 순간(10000)에 화면이 대기로 바뀌었다. 받기 자리(= 왼쪽 통화 키캡)를 누르던 손가락이 늦게 내려온다
        const int64_t flip = 10000;
        TapGuard g;
        const Model idle_now = M(State::IDLE);
        CHECK("guard: the accept position is the left call button on the idle screen (that is the hazard)", hit(idle_now, 404, 428) == Tap::CALL && hit(idle_now, 876, 428) == Tap::CALL_B
              && hit(M(State::INCOMING), 404, 428) == Tap::ACCEPT && hit(M(State::INCOMING), 876, 428) == Tap::REJECT);
        CHECK("guard: 0.3s after the bell screen closed - dropped (as before)", !g.allow(flip + 300, flip, screen_guard_for(Tap::CALL, Screen::INCOMING)));
        CHECK("guard: 0.7s after - still dropped (the old 600ms guard would have dialed)", !g.allow(flip + 700, flip, screen_guard_for(Tap::CALL, Screen::INCOMING)) && TapGuard().allow(flip + 700, flip));
        CHECK("guard: 1.4s after - still dropped, and the right button too", !g.allow(flip + 1400, flip, screen_guard_for(Tap::CALL, Screen::INCOMING)) && !g.allow(flip + 1400, flip, screen_guard_for(Tap::CALL_B, Screen::INCOMING)));
        CHECK("guard: wifi and volume on that same idle screen answer after the usual 600ms", g.allow(flip + 700, flip, screen_guard_for(Tap::WIFI, Screen::INCOMING)));
        CHECK("guard: 1.5s after - a call tap is taken (the dropped ones did not extend the wait)", g.allow(flip + 1500, flip, screen_guard_for(Tap::CALL, Screen::INCOMING)));
        // 내가 거절을 눌러 떠난 길: 전화 옴 → 끊는 중 → 대기. "안 눌렸나" 하고 거절 자리(= 오른쪽 통화 키캡)를 또 누른다 — 끊는 중은 지나가는 화면이라 앞 화면은 여전히 전화 옴이다
        TapGuard h;
        const Screen after_reject = prev_after({ Screen::IDLE, Screen::INCOMING, Screen::ENDING, Screen::IDLE });
        CHECK("guard: after my own reject (bell -> ending -> idle) the screen before is still the bell screen", after_reject == Screen::INCOMING);
        CHECK("guard: ... so tapping the reject position again 0.9s later does not dial the right peer; 1.5s later it does", !h.allow(flip + 900, flip, screen_guard_for(Tap::CALL_B, after_reject))
              && h.allow(flip + 1500, flip, screen_guard_for(Tap::CALL_B, after_reject)));
        TapGuard k;                                             // 반대 방향(대기 → 벨)은 늘리지 않았다(UL-D7 "하지 않은 것") — 기기 시험의 받기 주입이 이 길이에 맞춰져 있다
        CHECK("guard: accept right after the bell screen appears keeps the 600ms guard (documented, not extended)", !k.allow(flip + 500, flip, screen_guard_for(Tap::ACCEPT, Screen::IDLE)) && k.allow(flip + 600, flip, screen_guard_for(Tap::ACCEPT, Screen::IDLE)));
    }
    {   // ── 터치 가드가 보는 "앞 화면": 버튼이 없는 지나가는 화면은 세지 않는다(UL-D7 guard_prev) ──
        Model picker = M(State::INCOMING); picker.decline_open = true;
        const Model views[] = { M(State::OFFLINE, false, false), M(State::IDLE), M(State::DIALING), M(State::INCOMING), picker, M(State::ANSWERING), M(State::IN_CALL), M(State::ENDING) };
        bool rule = true;
        for (const Model &v : views) {                          // 지나가는 화면 = 버튼이 하나도 없는 화면, 그리고 그 반대
            bool any = false;
            for (int y = 0; y < SCREEN_H && !any; y += 4) for (int x = 0; x < SCREEN_W && !any; x += 4) any = hit(v, x, y) != Tap::NONE;
            rule = rule && passing_screen(screen_for(v)) == !any;
        }
        CHECK("prev: the passing screens are exactly the screens without a button (answering, ending)", rule && passing_screen(Screen::ANSWERING) && passing_screen(Screen::ENDING)
              && !passing_screen(Screen::IDLE) && !passing_screen(Screen::INCOMING) && !passing_screen(Screen::DECLINE) && !passing_screen(Screen::BOOT));
        CHECK("prev: caller hangs up or the bell times out (bell -> idle)", prev_after({ Screen::IDLE, Screen::INCOMING, Screen::IDLE }) == Screen::INCOMING);
        CHECK("prev: I reject (bell -> ending -> idle)", prev_after({ Screen::IDLE, Screen::INCOMING, Screen::ENDING, Screen::IDLE }) == Screen::INCOMING);
        CHECK("prev: I accept just as the caller gives up (bell -> answering -> idle)", prev_after({ Screen::IDLE, Screen::INCOMING, Screen::ANSWERING, Screen::IDLE }) == Screen::INCOMING);
        CHECK("prev: a real call in between resets it (bell -> answering -> in call -> idle / -> ending -> idle)", prev_after({ Screen::INCOMING, Screen::ANSWERING, Screen::IN_CALL, Screen::IDLE }) == Screen::IN_CALL
              && prev_after({ Screen::INCOMING, Screen::ANSWERING, Screen::IN_CALL, Screen::ENDING, Screen::IDLE }) == Screen::IN_CALL);
        CHECK("prev: rejecting with a message goes through the picker (bell -> picker -> ending -> idle): the picker's buttons are not under a call button", prev_after({ Screen::INCOMING, Screen::DECLINE, Screen::ENDING, Screen::IDLE }) == Screen::DECLINE
              && screen_guard_for(Tap::CALL, Screen::DECLINE) == TAP_SCREEN_GUARD_MS);
        CHECK("prev: back from the picker, then reject (bell -> picker -> bell -> ending -> idle)", prev_after({ Screen::INCOMING, Screen::DECLINE, Screen::INCOMING, Screen::ENDING, Screen::IDLE }) == Screen::INCOMING);
        CHECK("prev: my own call cancelled or hung up is not a bell (dialing -> ending -> idle, in call -> ending -> idle)", prev_after({ Screen::IDLE, Screen::DIALING, Screen::ENDING, Screen::IDLE }) == Screen::DIALING
              && prev_after({ Screen::IDLE, Screen::DIALING, Screen::IN_CALL, Screen::ENDING, Screen::IDLE }) == Screen::IN_CALL);
        CHECK("prev: the link dropping in between resets it (bell -> boot -> idle, bell -> ending -> boot -> idle)", prev_after({ Screen::INCOMING, Screen::BOOT, Screen::IDLE }) == Screen::BOOT
              && prev_after({ Screen::INCOMING, Screen::ENDING, Screen::BOOT, Screen::IDLE }) == Screen::BOOT);
        CHECK("prev: on the bell screen itself the screen before is idle (accept/reject keep the usual guard)", prev_after({ Screen::IDLE, Screen::INCOMING }) == Screen::IDLE
              && prev_after({ Screen::IDLE, Screen::DIALING, Screen::INCOMING }) == Screen::DIALING);
        bool only_bell = true;                                  // 어떤 길로 오든: 긴 가드가 걸리는 것은 앞 화면이 전화 옴일 때뿐
        const Screen all_sc[] = { Screen::BOOT, Screen::IDLE, Screen::DIALING, Screen::INCOMING, Screen::ANSWERING, Screen::IN_CALL, Screen::ENDING, Screen::DECLINE };
        for (Screen before : all_sc) for (Screen leaving : all_sc) {
            const Screen p = guard_prev(before, leaving);
            only_bell = only_bell && p == (passing_screen(leaving) ? before : leaving)
                     && (screen_guard_for(Tap::CALL, p) == TAP_AFTER_BELL_GUARD_MS) == (p == Screen::INCOMING);
        }
        CHECK("prev: for every pair of screens, the long guard follows only from the bell screen", only_bell);
    }
    {   // ── 다시 그릴지: 상대 부분(app.cpp same() 이 쓴다) ──
        Model a = M(State::IDLE), b = M(State::IDLE);
        CHECK("redraw: same peers on the idle screen -> no redraw", peers_same(a, b));
        b.peer_online[1] = false;
        CHECK("redraw: a peer going off on the idle screen -> redraw (slot 1)", !peers_same(a, b) && !peers_same(b, a));
        b = a; b.peer_online[0] = false;
        CHECK("redraw: ... and slot 0", !peers_same(a, b));
        Model c = M(State::IN_CALL), d = M(State::IN_CALL); d.peer_online[0] = false; d.peer_online[1] = false;
        CHECK("redraw: during a call the other peers coming and going do not repaint the screen", peers_same(c, d) && peers_same(d, c));
        bool calls_quiet = true;
        for (State st : { State::OFFLINE, State::DIALING, State::RINGBACK, State::INCOMING, State::ANSWERING, State::IN_CALL, State::ENDING }) {
            Model e = M(st), f = M(st); f.peer_online[1] = !e.peer_online[1];
            calls_quiet = calls_quiet && peers_same(e, f);
        }
        CHECK("redraw: only the idle screen shows who is on (every other screen ignores it)", calls_quiet);
        d = c; d.call_peer = 1;
        CHECK("redraw: a different peer of the call always repaints (the character changes)", !peers_same(c, d));
        Model i0 = M(State::IDLE), i1 = M(State::IDLE); i1.call_peer = 1;
        CHECK("redraw: ... on the idle screen too (cheap, and it never happens without a state change)", !peers_same(i0, i1));
    }

    printf("\n%d ok, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
