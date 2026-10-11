// test_roster.cpp — roster.h 단위 시험:
//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_roster.cpp -o /tmp/t_roster && /tmp/t_roster
// 설계서 docs/exec-plans/2026-10-08-phone-3way-design.md §8(공통 헤더)·TW-D4(기기 ID·번호).
// 앞쪽은 표와 slot 규칙, 뒤쪽("이어 보기")은 app.cpp 가 하는 일 — 받은 글자 → parse → route_slot → Fsm::on → send_text_for — 을 맥에서 그대로 이어 본다
// (기기 셋의 눈으로: 1번 Tab5 는 2번에게, 2번 Tab5 는 1번에게 거는 지금 동작이 v2 글자로 그대로 되는가).
#include "roster.h"

#include "call_fsm.h"
#include "relay_proto.h"

#include <climits>
#include <cstdio>
#include <cstring>
#include <string>
using namespace roster;
static int ok = 0, bad = 0;
#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)

static bool is(const char *a, const char *b) { return a && b && strcmp(a, b) == 0; }

// ── 이어 보기용: 전화기 한 대(app.cpp 의 통화 진행을 순수 로직만으로) ─────────────────────────────
struct Phone {
    int unit = 0;
    callfsm::Fsm fsm;
    std::string sent;          // 마지막으로 교환기에 보낸 글자("" = 보낸 것 없음)
    int dropped = 0;           // 버린 신호 수(상대를 알 수 없는 peer·ring)
    int unsendable = 0;        // 글자를 만들 수 없어 보내지 않은 할 일 수

    callfsm::Out apply(const callfsm::Out &o)                    // app.cpp apply() 의 "보내기" 부분
    {
        sent.clear();
        for (uint16_t bit : { callfsm::A_SEND_CALL, callfsm::A_SEND_ACCEPT, callfsm::A_SEND_REJECT, callfsm::A_SEND_HANGUP }) {
            if (!(o.acts & bit)) continue;
            char buf[relayproto::SEND_BUF];
            const char *text = relayproto::send_text_for(bit, buf, sizeof buf, id_of_slot(unit, o.peer), o.msg);
            if (text) sent = text; else ++unsendable;
        }
        return o;
    }
    callfsm::Out rx(const std::string &text, int64_t now)        // app.cpp app_task 의 "교환기 사건" 부분
    {
        const relayproto::Signal s = relayproto::parse(text.data(), text.size());
        if (s.kind != relayproto::Kind::EVENT) return callfsm::Out{};
        const int slot = route_slot(unit, relayproto::about_peer(s), s.peer_id);
        if (slot < 0) { ++dropped; return callfsm::Out{}; }
        return apply(fsm.on(s.ev, now, s.why, s.flag, s.msg, (uint8_t)slot));
    }
    callfsm::Out tap_call(int slot, int64_t now) { return apply(fsm.on(callfsm::Ev::TAP_CALL, now, callfsm::Why::NONE, false, 0, (uint8_t)slot)); }
    callfsm::Out tap(callfsm::Ev ev, int64_t now) { return apply(fsm.on(ev, now)); }
};

static Phone phone(int unit)                                     // 교환기에 막 붙은 전화기(상대 상태는 아직 모른다)
{
    Phone p; p.unit = unit;
    p.fsm.on(callfsm::Ev::LINK_UP, 1000);
    return p;
}

int main()
{
    // ── 표: 번호 ↔ 교환기 ID (TW-D4) ───────────────────────────────────────
    CHECK("three devices, two peers each", MAX_UNITS == 3 && MAX_PEERS == 2);
    CHECK("valid unit: 1..3 only", valid_unit(1) && valid_unit(2) && valid_unit(3) && !valid_unit(0) && !valid_unit(4) && !valid_unit(-1) && !valid_unit(INT_MAX) && !valid_unit(INT_MIN));
    CHECK("id of unit: tab5-1, tab5-2, stackchan-2", is(id_of_unit(1), "tab5-1") && is(id_of_unit(2), "tab5-2") && is(id_of_unit(3), "stackchan-2"));
    CHECK("id of an unknown unit is the empty string, never null", is(id_of_unit(0), "") && is(id_of_unit(4), "") && is(id_of_unit(-1), "") && is(id_of_unit(INT_MAX), "") && is(id_of_unit(INT_MIN), ""));
    CHECK("unit of id", unit_of_id("tab5-1") == 1 && unit_of_id("tab5-2") == 2 && unit_of_id("stackchan-2") == 3);
    CHECK("unit of an unknown id is 0 (null, empty, near misses, other case)",
          unit_of_id(nullptr) == 0 && unit_of_id("") == 0 && unit_of_id("tab5-3") == 0 && unit_of_id("tab5-") == 0 && unit_of_id("tab5-11") == 0 && unit_of_id("tab5-1 ") == 0
          && unit_of_id(" tab5-1") == 0 && unit_of_id("TAB5-1") == 0 && unit_of_id("stackchan-1") == 0 && unit_of_id("stackchan-22") == 0 && unit_of_id("unknown") == 0);
    {
        bool all = true;
        for (int u = 1; u <= MAX_UNITS; ++u) all = all && unit_of_id(id_of_unit(u)) == u;
        CHECK("unit -> id -> unit round trip for every unit", all);
    }
    {   // ID 는 글자로 그대로 실린다: 길이·글자가 규약 안이어야 한다(교환기 글자 + 접속 헤더)
        bool all = true;
        for (int u = 1; u <= MAX_UNITS; ++u) {
            const char *id = id_of_unit(u);
            all = all && strlen(id) >= 1 && strlen(id) <= ID_MAX && relayproto::id_ok(id);
            for (int v = u + 1; v <= MAX_UNITS; ++v) all = all && !is(id, id_of_unit(v));
        }
        CHECK("every id is unique, 1..15 chars and wire-safe", all && ID_MAX + 1 == relayproto::ID_CAP);
    }

    // ── 상대 slot: 나를 뺀 나머지를 번호 오름차순으로 (RS-D1) ──────────────
    CHECK("peer count: 2 for a registered unit, 0 otherwise", peer_count(1) == 2 && peer_count(2) == 2 && peer_count(3) == 2 && peer_count(0) == 0 && peer_count(4) == 0 && peer_count(-1) == 0);
    CHECK("unit 1 sees 2 then 3", peer_unit(1, 0) == 2 && peer_unit(1, 1) == 3);
    CHECK("unit 2 sees 1 then 3", peer_unit(2, 0) == 1 && peer_unit(2, 1) == 3);
    CHECK("unit 3 sees 1 then 2", peer_unit(3, 0) == 1 && peer_unit(3, 1) == 2);
    CHECK("the two Tab5 are each other's slot 0 (the old pair keeps its place)", peer_unit(1, 0) == 2 && peer_unit(2, 0) == 1 && slot_of_unit(1, 2) == 0 && slot_of_unit(2, 1) == 0);
    {
        bool all = true;
        for (int me = 1; me <= MAX_UNITS; ++me) {
            bool seen[MAX_UNITS + 1] = {};
            int last = 0;
            for (int s = 0; s < peer_count(me); ++s) {
                const int u = peer_unit(me, s);
                all = all && valid_unit(u) && u != me && u > last && !seen[u];      // 나는 빠지고, 오름차순이고, 겹치지 않는다
                if (valid_unit(u)) seen[u] = true;
                last = u;
            }
            for (int u = 1; u <= MAX_UNITS; ++u) all = all && (seen[u] == (u != me));   // 나머지는 빠짐없이 있다
        }
        CHECK("slots list every other unit exactly once, in ascending order", all);
    }
    CHECK("no peer in a slot out of range", peer_unit(1, -1) == 0 && peer_unit(1, 2) == 0 && peer_unit(2, 5) == 0 && peer_unit(3, 255) == 0 && peer_unit(2, INT_MAX) == 0 && peer_unit(2, INT_MIN) == 0);
    CHECK("slot of unit", slot_of_unit(1, 2) == 0 && slot_of_unit(1, 3) == 1 && slot_of_unit(2, 1) == 0 && slot_of_unit(2, 3) == 1 && slot_of_unit(3, 1) == 0 && slot_of_unit(3, 2) == 1);
    CHECK("I am not my own peer", slot_of_unit(1, 1) == -1 && slot_of_unit(2, 2) == -1 && slot_of_unit(3, 3) == -1);
    CHECK("an unknown unit has no slot", slot_of_unit(1, 0) == -1 && slot_of_unit(1, 4) == -1 && slot_of_unit(2, -1) == -1 && slot_of_unit(3, INT_MAX) == -1);
    {
        bool all = true;
        for (int me = 1; me <= MAX_UNITS; ++me)
            for (int s = 0; s < MAX_PEERS; ++s) all = all && slot_of_unit(me, peer_unit(me, s)) == s && slot_of_id(me, id_of_slot(me, s)) == s;
        CHECK("slot -> unit -> slot and slot -> id -> slot round trips", all);
    }
    CHECK("slot of id", slot_of_id(2, "tab5-1") == 0 && slot_of_id(2, "stackchan-2") == 1 && slot_of_id(1, "tab5-2") == 0 && slot_of_id(1, "stackchan-2") == 1
          && slot_of_id(3, "tab5-1") == 0 && slot_of_id(3, "tab5-2") == 1);
    CHECK("my own id, an unknown id, an empty id and null have no slot", slot_of_id(2, "tab5-2") == -1 && slot_of_id(1, "tab5-1") == -1 && slot_of_id(3, "stackchan-2") == -1
          && slot_of_id(2, "tab5-9") == -1 && slot_of_id(2, "") == -1 && slot_of_id(2, nullptr) == -1);
    CHECK("id of slot", is(id_of_slot(2, 0), "tab5-1") && is(id_of_slot(2, 1), "stackchan-2") && is(id_of_slot(1, 0), "tab5-2") && is(id_of_slot(3, 1), "tab5-2"));
    CHECK("id of a slot out of range is the empty string", is(id_of_slot(2, 2), "") && is(id_of_slot(2, -1), "") && is(id_of_slot(2, 255), ""));

    // ── 내 번호가 0(미등록)이거나 범위 밖이면: 죽지 않고 "상대 없음" (RS-D3) ──
    {
        bool none = true;
        for (int me : { 0, 4, -1, 99, INT_MAX, INT_MIN }) {
            none = none && peer_count(me) == 0;
            for (int s = -1; s <= 3; ++s) none = none && peer_unit(me, s) == 0 && is(id_of_slot(me, s), "");
            for (int u = -1; u <= 4; ++u) none = none && slot_of_unit(me, u) == -1;
            for (const char *id : { "tab5-1", "tab5-2", "stackchan-2", "", "x" }) none = none && slot_of_id(me, id) == -1 && route_slot(me, true, id) == -1;
            none = none && slot_of_id(me, nullptr) == -1 && route_slot(me, true, nullptr) == -1;
        }
        CHECK("an unregistered phone (unit 0 or out of range) has no peers at all", none);
    }

    // ── 받은 신호의 slot (RS-D4): 상대에 달린 신호(peer·ring)는 ID 로, 그 밖은 ID 를 보지 않는다 ──
    CHECK("route: a peer-bound signal goes to the slot of its id", route_slot(2, true, "tab5-1") == 0 && route_slot(2, true, "stackchan-2") == 1 && route_slot(3, true, "tab5-2") == 1);
    CHECK("route: a peer-bound signal without a usable id is dropped", route_slot(2, true, "") == -1 && route_slot(2, true, nullptr) == -1 && route_slot(2, true, "tab5-2") == -1 && route_slot(2, true, "nobody") == -1);
    CHECK("route: other signals never look at the id", route_slot(2, false, "") == 0 && route_slot(2, false, nullptr) == 0 && route_slot(2, false, "nobody") == 0 && route_slot(2, false, "stackchan-2") == 0
          && route_slot(0, false, nullptr) == 0);

    // 컴파일 때도 쓸 수 있다(화면 쪽이 static_assert 로 표를 묶을 수 있게)
    static_assert(peer_unit(2, 1) == 3 && slot_of_unit(3, 2) == 1 && unit_of_id("stackchan-2") == 3 && slot_of_id(1, "tab5-2") == 0 && route_slot(1, true, "tab5-1") == -1, "roster is usable in constant expressions");
    CHECK("usable in constant expressions", true);


    // ════ 이어 보기: 받은 글자 → slot → 상태 기계 → 보낼 글자 (app.cpp 의 이음 그대로) ════
    using callfsm::Ev;
    using callfsm::Notice;
    using callfsm::State;

    {   // 2번 Tab5: 접속 직후 교환기가 다른 기기 전부의 상태를 하나씩 알린다(§6.3)
        Phone p = phone(2);
        p.rx("{\"t\":\"peer\",\"id\":\"tab5-1\",\"online\":true}", 1100);
        p.rx("{\"t\":\"peer\",\"id\":\"stackchan-2\",\"online\":false}", 1100);
        CHECK("unit 2: peer tab5-1 is slot 0, stackchan-2 is slot 1", p.fsm.online(0) && !p.fsm.online(1) && p.dropped == 0);
        callfsm::Out o = p.tap_call(0, 2000);
        CHECK("unit 2 calls slot 0 -> {\"t\":\"call\",\"to\":\"tab5-1\"}", p.fsm.state == State::DIALING && o.peer == 0 && p.sent == "{\"t\":\"call\",\"to\":\"tab5-1\"}");
        p.rx("{\"t\":\"ringing\"}", 2100);
        o = p.rx("{\"t\":\"ended\",\"why\":\"busy\"}", 2200);
        CHECK("... the peer is on another call: busy notice, back to idle", p.fsm.state == State::IDLE && o.notice == Notice::BUSY && o.peer == 0 && p.sent.empty());
        o = p.tap_call(1, 9000);
        CHECK("unit 2 taps slot 1 while the stackchan is off: nothing is sent", p.fsm.state == State::IDLE && o.notice == Notice::PEER_OFF && o.peer == 1 && p.sent.empty());
        p.rx("{\"t\":\"peer\",\"id\":\"stackchan-2\",\"online\":true}", 9500);
        o = p.tap_call(1, 10000);
        CHECK("unit 2 calls slot 1 -> {\"t\":\"call\",\"to\":\"stackchan-2\"}", p.fsm.state == State::DIALING && o.peer == 1 && p.sent == "{\"t\":\"call\",\"to\":\"stackchan-2\"}");
    }
    {   // 1번 Tab5: 2번에게(지금 동작이 v2 글자로)
        Phone p = phone(1);
        p.rx("{\"t\":\"peer\",\"id\":\"tab5-2\",\"online\":true}", 1100);
        p.tap_call(0, 2000);
        CHECK("unit 1 calls slot 0 -> {\"t\":\"call\",\"to\":\"tab5-2\"}", p.fsm.state == State::DIALING && p.sent == "{\"t\":\"call\",\"to\":\"tab5-2\"}");
        p.rx("{\"t\":\"ringing\"}", 2100);
        p.rx("{\"t\":\"connected\"}", 3000);
        CHECK("... ringing and connected carry no id and still drive the call", p.fsm.state == State::IN_CALL && p.dropped == 0);
        p.tap(Ev::TAP_HANGUP, 5000);
        CHECK("... hang-up text is unchanged", p.sent == "{\"t\":\"hangup\"}");
    }
    {   // 스택짱(3번): slot 0 = 1번, slot 1 = 2번
        Phone p = phone(3);
        p.rx("{\"t\":\"peer\",\"id\":\"tab5-1\",\"online\":true}", 1100);
        p.rx("{\"t\":\"peer\",\"id\":\"tab5-2\",\"online\":true}", 1100);
        p.tap_call(1, 2000);
        CHECK("unit 3 calls slot 1 -> {\"t\":\"call\",\"to\":\"tab5-2\"}", p.sent == "{\"t\":\"call\",\"to\":\"tab5-2\"}");
        Phone q = phone(3);
        q.rx("{\"t\":\"peer\",\"id\":\"tab5-1\",\"online\":true}", 1100);
        q.tap_call(0, 2000);
        CHECK("unit 3 calls slot 0 -> {\"t\":\"call\",\"to\":\"tab5-1\"}", q.sent == "{\"t\":\"call\",\"to\":\"tab5-1\"}");
    }
    {   // 전화 옴: 건 기기를 slot 으로 기억한다
        Phone p = phone(1);
        callfsm::Out o = p.rx("{\"t\":\"ring\",\"from\":\"stackchan-2\"}", 2000);
        CHECK("unit 1: ring from stackchan-2 = incoming from slot 1", p.fsm.state == State::INCOMING && p.fsm.call_peer == 1 && o.peer == 1 && (o.acts & callfsm::A_TONE_BELL));
        p.tap(Ev::TAP_ACCEPT, 3000);
        CHECK("... accept text is unchanged", p.sent == "{\"t\":\"accept\"}");
        Phone q = phone(2);
        q.rx("{\"t\":\"ring\",\"from\":\"tab5-1\"}", 2000);
        CHECK("unit 2: ring from tab5-1 = incoming from slot 0", q.fsm.state == State::INCOMING && q.fsm.call_peer == 0);
        q.apply(q.fsm.on(Ev::TAP_REJECT, 3000, callfsm::Why::NONE, false, 2));
        CHECK("... message reject text is unchanged", q.sent == "{\"t\":\"reject\",\"msg\":2}");
    }
    {   // v1 모양(id 없음)·모르는 id·내 id 의 peer·ring 은 버린다 — v2 교환기는 항상 id 를 싣는다(§6.3)
        Phone p = phone(2);
        p.rx("{\"t\":\"peer\",\"online\":true}", 1100);
        CHECK("v1-shaped peer (no id) is dropped: nobody comes online", !p.fsm.online(0) && !p.fsm.online(1) && p.dropped == 1);
        p.rx("{\"t\":\"ring\"}", 1200);
        CHECK("v1-shaped ring (no from) is dropped: no bell", p.fsm.state == State::IDLE && p.dropped == 2 && p.fsm.missed == 0);
        p.rx("{\"t\":\"peer\",\"id\":\"tab5-9\",\"online\":true}", 1300);
        p.rx("{\"t\":\"ring\",\"from\":\"tab5-9\"}", 1300);
        CHECK("an unregistered id is dropped", !p.fsm.online(0) && !p.fsm.online(1) && p.fsm.state == State::IDLE && p.dropped == 4);
        p.rx("{\"t\":\"peer\",\"id\":\"tab5-2\",\"online\":true}", 1400);
        p.rx("{\"t\":\"ring\",\"from\":\"tab5-2\"}", 1400);
        CHECK("my own id is dropped", !p.fsm.online(0) && !p.fsm.online(1) && p.fsm.state == State::IDLE && p.dropped == 6);
        p.rx("{\"t\":\"peer\",\"id\":\"abcdefghijklmnop\",\"online\":true}", 1500);      // 16자 — 규약보다 긴 id
        p.rx("{\"t\":\"peer\",\"id\":7,\"online\":true}", 1500);
        p.rx("{\"t\":\"peer\",\"from\":\"tab5-1\",\"online\":true}", 1500);             // peer 의 열쇠는 id 다(from 이 아니다)
        p.rx("{\"t\":\"ring\",\"id\":\"tab5-1\"}", 1500);                                // ring 의 열쇠는 from 이다(id 가 아니다)
        CHECK("an over-long id, a number id and the wrong key are dropped", !p.fsm.online(0) && !p.fsm.online(1) && p.fsm.state == State::IDLE && p.dropped == 10);
        callfsm::Out o = p.tap_call(0, 2000);
        CHECK("... so a call tap only shows 'peer is off'", o.notice == Notice::PEER_OFF && p.sent.empty() && p.fsm.state == State::IDLE);
    }
    {   // 미등록 기기(0번): 교환기에 붙지 않지만, 붙었다고 쳐도 상대가 없다
        Phone p = phone(0);
        p.rx("{\"t\":\"peer\",\"id\":\"tab5-1\",\"online\":true}", 1100);
        p.rx("{\"t\":\"ring\",\"from\":\"tab5-1\"}", 1200);
        CHECK("unit 0: every peer and ring is dropped", !p.fsm.online(0) && !p.fsm.online(1) && p.fsm.state == State::IDLE && p.dropped == 2);
        p.fsm.on(Ev::PEER, 1300, callfsm::Why::NONE, true, 0, 0);                        // 상태 기계를 억지로 켜도
        p.tap_call(0, 2000);
        CHECK("unit 0: a call has no id to go to -> nothing is sent (never the old-pair text)", p.sent.empty() && p.unsendable == 1);
    }
    {   // 걸 때마다: 어느 번호·어느 slot 에서든 글자가 만들어지고, 그 ID 는 표의 것이다
        bool all = true;
        for (int me = 1; me <= MAX_UNITS; ++me)
            for (int s = 0; s < MAX_PEERS; ++s) {
                Phone p = phone(me);
                p.rx(std::string("{\"t\":\"peer\",\"id\":\"") + id_of_slot(me, s) + "\",\"online\":true}", 1100);
                p.tap_call(s, 2000);
                all = all && p.fsm.online(s) && !p.fsm.online(1 - s) && p.fsm.call_peer == s && p.unsendable == 0
                      && p.sent == std::string("{\"t\":\"call\",\"to\":\"") + id_of_unit(peer_unit(me, s)) + "\"}";
            }
        CHECK("every unit x slot: peer online -> call text names that peer's id", all);
    }

    printf("\n%d ok, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
