// test_call_fsm.cpp — call_fsm.h 단위 시험:  clang++ -std=c++17 -Wall -Wextra -I../main test_call_fsm.cpp -o /tmp/t_fsm && /tmp/t_fsm
// 설계서 §9.4 "교환기-기기 계약", §9.5 call_fsm 필수 시험 ①~④ 를 고정한다.
#include "call_fsm.h"
#include <cstdio>
#include <string>
using namespace callfsm;
static int ok = 0, bad = 0;
#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)

static bool has(const Out &o, uint16_t a) { return (o.acts & a) == a; }

// 교환기에 붙고 상대도 켜진 대기 상태의 기계
static Fsm idle(int64_t t = 1000)
{
    Fsm f;
    f.on(Ev::LINK_UP, t);
    f.on(Ev::PEER, t, Why::NONE, true);
    return f;
}

int main()
{
    // ── 접속 ────────────────────────────────────────────────────────────────
    {
        Fsm f;
        CHECK("boot: offline", f.state == State::OFFLINE && !f.peer_online);
        Out o = f.on(Ev::TAP_CALL, 10);
        CHECK("offline: call tap does nothing", f.state == State::OFFLINE && o.acts == 0);
        f.on(Ev::LINK_UP, 20);
        CHECK("link up -> idle, peer unknown = off", f.state == State::IDLE && !f.peer_online);
        o = f.on(Ev::TAP_CALL, 30);
        CHECK("idle + peer off: no call sent, notice peer-off", f.state == State::IDLE && o.acts == 0 && o.notice == Notice::PEER_OFF);
        f.on(Ev::PEER, 40, Why::NONE, true);
        CHECK("peer online flag", f.peer_online);
        f.on(Ev::PEER, 50, Why::NONE, false);
        CHECK("peer offline flag", !f.peer_online);
    }

    // ── 걸기 → 벨 → 받음 → 통화 → 끊기(내가) ────────────────────────────────
    {
        Fsm f = idle();
        Out o = f.on(Ev::TAP_CALL, 2000);
        CHECK("call: dialing + send call", f.state == State::DIALING && has(o, A_SEND_CALL));
        o = f.on(Ev::RINGING, 2100);
        CHECK("ringing: ringback tone", f.state == State::RINGBACK && has(o, A_TONE_RINGBACK));
        o = f.on(Ev::CONNECTED, 5000);
        CHECK("connected: in call, tone stop, voice start", f.state == State::IN_CALL && has(o, A_TONE_STOP | A_VOICE_START));
        CHECK("call start time recorded", f.call_start_ms == 5000);
        o = f.on(Ev::TAP_HANGUP, 9000);
        CHECK("hangup: ending, send hangup, voice stop, end tone", f.state == State::ENDING && has(o, A_SEND_HANGUP | A_VOICE_STOP | A_TONE_END));
        CHECK("talk time kept", f.last_talk_ms == 4000);
        o = f.on(Ev::ENDED, 9100, Why::HANGUP);
        CHECK("ended after my hangup: idle, no extra tone", f.state == State::IDLE && o.acts == 0);
    }

    // ── 상대가 끊음 / 통화 중 끊김 ──────────────────────────────────────────
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2); f.on(Ev::CONNECTED, 3);
        Out o = f.on(Ev::ENDED, 10003, Why::HANGUP);
        CHECK("peer hangup: idle, voice stop, end tone, notice", f.state == State::IDLE && has(o, A_VOICE_STOP | A_TONE_END) && o.notice == Notice::CALL_ENDED);
        CHECK("peer hangup: talk time", f.last_talk_ms == 10000);
    }
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2); f.on(Ev::CONNECTED, 3);
        Out o = f.on(Ev::ENDED, 500, Why::LOST);
        CHECK("ended lost in call: notice lost", f.state == State::IDLE && o.notice == Notice::LOST && has(o, A_VOICE_STOP | A_TONE_END));
    }
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2); f.on(Ev::CONNECTED, 3);
        Out o = f.on(Ev::LINK_DOWN, 500);
        CHECK("link down in call: offline, voice stop, end tone, notice lost", f.state == State::OFFLINE && has(o, A_VOICE_STOP | A_TONE_END) && o.notice == Notice::LOST);
        CHECK("link down clears peer flag", !f.peer_online);
    }

    // ── 거는 쪽이 받는 종료 사유들 ──────────────────────────────────────────
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2);
        Out o = f.on(Ev::ENDED, 30002, Why::TIMEOUT);
        CHECK("caller timeout: notice no-answer + end tone", f.state == State::IDLE && o.notice == Notice::NO_ANSWER && has(o, A_TONE_END));
    }
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2);
        Out o = f.on(Ev::ENDED, 900, Why::REJECT);
        CHECK("caller rejected: notice rejected", f.state == State::IDLE && o.notice == Notice::REJECTED);
        f.on(Ev::TAP_CALL, 1000); f.on(Ev::RINGING, 1001);
        o = f.on(Ev::ENDED, 1500, Why::HANGUP);
        CHECK("callee hung up during bell = rejected", o.notice == Notice::REJECTED);
    }
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1);
        Out o = f.on(Ev::ENDED, 50, Why::OFFLINE);
        CHECK("call to offline peer: notice peer-off (no ringing first)", f.state == State::IDLE && o.notice == Notice::PEER_OFF);
    }
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2);
        Out o = f.on(Ev::ENDED, 900, Why::OFFLINE);
        CHECK("peer vanished during bell: notice peer-off, tone end", f.state == State::IDLE && o.notice == Notice::PEER_OFF && has(o, A_TONE_END));
    }

    // ── 거는 쪽 취소 ────────────────────────────────────────────────────────
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2);
        Out o = f.on(Ev::TAP_CANCEL, 500);
        CHECK("cancel: ending + send hangup + tone stop", f.state == State::ENDING && has(o, A_SEND_HANGUP | A_TONE_STOP));
        Fsm g = idle(); g.on(Ev::TAP_CALL, 1);
        o = g.on(Ev::TAP_CANCEL, 5);
        CHECK("cancel before ringing arrives also sends hangup", g.state == State::ENDING && has(o, A_SEND_HANGUP));
    }

    // ── 받는 쪽: 받기 / 거절 / 부재중 ───────────────────────────────────────
    {
        Fsm f = idle();
        Out o = f.on(Ev::RING, 100);
        CHECK("ring: incoming + bell", f.state == State::INCOMING && has(o, A_TONE_BELL));
        o = f.on(Ev::TAP_ACCEPT, 2000);
        CHECK("accept: answering, send accept, bell stop", f.state == State::ANSWERING && has(o, A_SEND_ACCEPT | A_TONE_STOP));
        o = f.on(Ev::CONNECTED, 2100);
        CHECK("connected after accept: in call + voice start", f.state == State::IN_CALL && has(o, A_VOICE_START));
        CHECK("accepted call is not a missed call", f.missed == 0);
    }
    {
        Fsm f = idle(); f.on(Ev::RING, 100);
        Out o = f.on(Ev::TAP_REJECT, 900);
        CHECK("reject: ending, send reject, bell stop", f.state == State::ENDING && has(o, A_SEND_REJECT | A_TONE_STOP));
        o = f.on(Ev::ENDED, 950, Why::REJECT);
        CHECK("ended after my reject: idle, not missed", f.state == State::IDLE && f.missed == 0 && o.notice == Notice::NONE);
    }
    {   // 필수 시험 ③: 벨 중 ended hangup·timeout = 부재중
        Fsm f = idle(); f.on(Ev::RING, 100);
        Out o = f.on(Ev::ENDED, 30100, Why::TIMEOUT);
        CHECK("(3) bell timeout = missed", f.state == State::IDLE && f.missed == 1 && f.missed_at_ms == 30100 && has(o, A_TONE_STOP) && o.notice == Notice::MISSED);
        f.on(Ev::RING, 40000);
        o = f.on(Ev::ENDED, 41000, Why::HANGUP);
        CHECK("(3) caller canceled during bell = missed", f.missed == 2 && f.missed_at_ms == 41000 && o.notice == Notice::MISSED);
        f.on(Ev::RING, 50000);
        f.on(Ev::ENDED, 51000, Why::OFFLINE);
        CHECK("caller vanished during bell = missed", f.missed == 3);
        f.clear_missed();
        CHECK("clear missed", f.missed == 0);
    }
    {
        Fsm f = idle(); f.on(Ev::RING, 100);
        Out o = f.on(Ev::LINK_DOWN, 500);
        CHECK("link down during bell: offline, bell stop, missed", f.state == State::OFFLINE && has(o, A_TONE_STOP) && f.missed == 1);
    }
    {   // 받기를 눌렀는데 그 순간 거는 쪽이 취소 → 부재중이 아니다(내가 받으려 했음)
        Fsm f = idle(); f.on(Ev::RING, 100); f.on(Ev::TAP_ACCEPT, 200);
        Out o = f.on(Ev::ENDED, 250, Why::HANGUP);
        CHECK("accept raced with cancel: idle, not missed, notice ended", f.state == State::IDLE && f.missed == 0 && o.notice == Notice::CALL_ENDED);
    }

    {   // PH-D6: 받기를 누르지 않았는데 connected 가 와도 마이크를 열지 않는다
        Fsm f = idle(); f.on(Ev::RING, 100);
        Out o = f.on(Ev::CONNECTED, 200);
        CHECK("connected without my accept never opens the mic", !has(o, A_VOICE_START) && f.state != State::IN_CALL && has(o, A_RECONNECT | A_TONE_STOP));
    }

    // ── 동시 발신(§9.4): 내 call 이 가는 중에 ring 이 오면 그대로 기다린다 → connected ──
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1);
        Out o = f.on(Ev::RING, 5);
        CHECK("glare: ring while dialing keeps dialing, no bell", f.state == State::DIALING && o.acts == 0);
        o = f.on(Ev::CONNECTED, 20);
        CHECK("glare: connected -> in call", f.state == State::IN_CALL && has(o, A_VOICE_START));
    }

    // ── 필수 시험 ①: 끊은 직후 다시 걸기 ────────────────────────────────────
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2); f.on(Ev::CONNECTED, 3);
        f.on(Ev::TAP_HANGUP, 1000);
        Out o = f.on(Ev::TAP_CALL, 1010);
        CHECK("(1) no call while waiting for ended", f.state == State::ENDING && !has(o, A_SEND_CALL));
        f.on(Ev::ENDED, 1050, Why::HANGUP);
        o = f.on(Ev::TAP_CALL, 1100);
        CHECK("(1) after ended: call goes out", f.state == State::DIALING && has(o, A_SEND_CALL));
        f.on(Ev::RINGING, 1150);
        CHECK("(1) new call survives (no stale ended consumed)", f.state == State::RINGBACK);
    }
    {   // ended 가 끝내 안 오면 3초 뒤 연결을 끊고 다시 붙는다 → 옛 연결의 뒤늦은 ended 는 교환기가 버린다
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2); f.on(Ev::TAP_CANCEL, 1000);
        Out o = f.on(Ev::TICK, 3999);
        CHECK("(1) ending: not yet timed out", f.state == State::ENDING && o.acts == 0);
        o = f.on(Ev::TICK, 4000);
        CHECK("(1) ending timeout 3s: reconnect, offline", f.state == State::OFFLINE && has(o, A_RECONNECT));
        o = f.on(Ev::ENDED, 4100, Why::HANGUP);
        CHECK("(1) late ended while offline is dropped", f.state == State::OFFLINE && o.acts == 0 && o.notice == Notice::NONE);
    }

    // ── 필수 시험 ②: 대기 상태에서 받은 ended 무시 ──────────────────────────
    {
        Fsm f = idle();
        Out o = f.on(Ev::ENDED, 10, Why::HANGUP);
        CHECK("(2) ended in idle ignored", f.state == State::IDLE && o.acts == 0 && o.notice == Notice::NONE && f.missed == 0);
        o = f.on(Ev::CONNECTED, 20);
        CHECK("connected in idle ignored", f.state == State::IDLE && o.acts == 0);
        o = f.on(Ev::RINGING, 30);
        CHECK("ringing in idle ignored", f.state == State::IDLE && o.acts == 0);
    }

    // ── 필수 시험 ④: 응답 없는 신호에 멈추지 않음 ───────────────────────────
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1000);
        Out o = f.on(Ev::TICK, 5999);
        CHECK("(4) dialing: waits up to 5s", f.state == State::DIALING && o.acts == 0);
        o = f.on(Ev::TICK, 6000);
        CHECK("(4) dialing no answer from relay: reconnect + notice", f.state == State::OFFLINE && has(o, A_RECONNECT) && o.notice == Notice::RELAY_SILENT);
    }
    {
        Fsm f = idle(); f.on(Ev::RING, 0); f.on(Ev::TAP_ACCEPT, 1000);
        Out o = f.on(Ev::TICK, 6000);
        CHECK("(4) accept unanswered 5s: reconnect", f.state == State::OFFLINE && has(o, A_RECONNECT));
    }
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 0); f.on(Ev::RINGING, 100);
        Out o = f.on(Ev::TICK, 40099);
        CHECK("(4) ringback: relay ends it at 30s, we wait 40s", f.state == State::RINGBACK && o.acts == 0);
        o = f.on(Ev::TICK, 40100);
        CHECK("(4) ringback stuck 40s: reconnect + tone stop", f.state == State::OFFLINE && has(o, A_RECONNECT | A_TONE_STOP));
    }
    {
        Fsm f = idle(); f.on(Ev::RING, 100);
        Out o = f.on(Ev::TICK, 40100);
        CHECK("(4) bell stuck 40s: reconnect, bell stop, missed", f.state == State::OFFLINE && has(o, A_RECONNECT | A_TONE_STOP) && f.missed == 1);
    }
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2); f.on(Ev::CONNECTED, 3);
        Out o = f.on(Ev::TICK, 99999999);
        CHECK("in call: no time limit", f.state == State::IN_CALL && o.acts == 0);
        Fsm g = idle();
        o = g.on(Ev::TICK, 99999999);
        CHECK("idle: tick does nothing", g.state == State::IDLE && o.acts == 0);
    }

    // ── 상태에 안 맞는 화면 입력은 아무것도 보내지 않는다 ───────────────────
    {
        Fsm f = idle();
        CHECK("idle: accept/reject/hangup/cancel taps ignored",
              f.on(Ev::TAP_ACCEPT, 1).acts == 0 && f.on(Ev::TAP_REJECT, 2).acts == 0 && f.on(Ev::TAP_HANGUP, 3).acts == 0 && f.on(Ev::TAP_CANCEL, 4).acts == 0 && f.state == State::IDLE);
        f.on(Ev::RING, 10);
        CHECK("incoming: call tap ignored (no second call)", f.on(Ev::TAP_CALL, 11).acts == 0 && f.state == State::INCOMING);
        f.on(Ev::TAP_ACCEPT, 12); f.on(Ev::CONNECTED, 13);
        CHECK("in call: call/accept taps ignored", f.on(Ev::TAP_CALL, 14).acts == 0 && f.on(Ev::TAP_ACCEPT, 15).acts == 0 && f.state == State::IN_CALL);
        Out o = f.on(Ev::RING, 16);
        CHECK("in call: stray ring ignored", o.acts == 0 && f.state == State::IN_CALL);
    }
    {   // 내가 끊는 순간 상대가 받아 connected 가 먼저 도착해도 통화로 들어가지 않는다(곧 ended 가 온다)
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2); f.on(Ev::TAP_CANCEL, 500);
        Out o = f.on(Ev::CONNECTED, 510);
        CHECK("connected while ending ignored (no voice start)", f.state == State::ENDING && o.acts == 0);
    }

    // ── 연결이 끊겼다 다시 붙기 ────────────────────────────────────────────
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1);
        Out o = f.on(Ev::LINK_DOWN, 100);
        CHECK("link down while dialing: offline + notice lost", f.state == State::OFFLINE && o.notice == Notice::LOST && has(o, A_TONE_STOP));
        o = f.on(Ev::LINK_DOWN, 200);
        CHECK("second link down is a no-op", f.state == State::OFFLINE && o.acts == 0 && o.notice == Notice::NONE);
        f.on(Ev::LINK_UP, 300);
        CHECK("link up again: idle, peer unknown", f.state == State::IDLE && !f.peer_online);
        Fsm g = idle(); g.on(Ev::RING, 1); g.on(Ev::TAP_REJECT, 2);
        o = g.on(Ev::LINK_DOWN, 3);
        CHECK("link down while ending: offline, silent", g.state == State::OFFLINE && o.acts == 0 && o.notice == Notice::NONE);
        o = g.on(Ev::LINK_UP, 4);
        CHECK("link up in a call state cannot happen but must not crash", g.state == State::IDLE);
    }

    // ── 시각이 큰 값이어도(수개월 가동) 시간 제한이 맞게 돈다 ───────────────
    {
        const int64_t big = 400LL * 24 * 3600 * 1000;      // 400일
        Fsm f = idle(big); f.on(Ev::TAP_CALL, big);
        CHECK("large clock: not timed out early", f.on(Ev::TICK, big + 4999).acts == 0);
        CHECK("large clock: times out at 5s", has(f.on(Ev::TICK, big + 5000), A_RECONNECT));
    }

    // ── 규약의 구멍(신호에 통화 번호가 없다): 내가 끊는 순간 상대가 먼저 끊고 곧바로 다시 걸면, 교환기는 내 옛 hangup 으로 상대의 새 통화를 끝낸다.
    //    내 쪽에는 ended → ring → ended 가 온다. 가짜 부재중 1건은 생기지만 멈추지는 않는다.
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2); f.on(Ev::CONNECTED, 3);
        f.on(Ev::TAP_HANGUP, 1000);
        f.on(Ev::ENDED, 1010, Why::HANGUP);
        Out o = f.on(Ev::RING, 1020);
        CHECK("race: ring right after ended is a new incoming call", f.state == State::INCOMING && has(o, A_TONE_BELL));
        o = f.on(Ev::ENDED, 1030, Why::HANGUP);
        CHECK("race: stale hangup ends it -> idle (counts as missed), never stuck", f.state == State::IDLE && f.missed == 1 && has(o, A_TONE_STOP));
        o = f.on(Ev::TAP_CALL, 2000);
        CHECK("race: can call again afterwards", f.state == State::DIALING && has(o, A_SEND_CALL));
    }

    // ── 로그에 찍는 이름 ────────────────────────────────────────────────────
    CHECK("names for the serial log", std::string(state_name(State::RINGBACK)) == "RINGBACK" && std::string(state_name(State::IN_CALL)) == "IN_CALL"
          && std::string(notice_name(Notice::RELAY_SILENT)) == "RELAY_SILENT" && std::string(notice_name(Notice::NONE)) == "NONE");


    // ════ 수신거부 메시지·벨 끄기 (docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md §5.3) ════
    const uint16_t SENDS = A_SEND_CALL | A_SEND_ACCEPT | A_SEND_REJECT | A_SEND_HANGUP;

    // ── 벨 끄기: 이번 전화의 벨만 멈춘다. 신호는 나가지 않고 상태·시간 제한은 그대로 ──
    {
        Fsm f = idle(); f.on(Ev::RING, 100);
        Out o = f.on(Ev::TAP_MUTE, 500);
        CHECK("mute: stays incoming, only tone-stop, flag set", f.state == State::INCOMING && o.acts == A_TONE_STOP && o.notice == Notice::NONE && o.msg == 0 && f.bell_muted);
        CHECK("mute never sends a signal", (o.acts & SENDS) == 0);
        o = f.on(Ev::TAP_MUTE, 900);
        CHECK("mute twice: second is a no-op", o.acts == 0 && o.notice == Notice::NONE && f.state == State::INCOMING && f.bell_muted);
        o = f.on(Ev::RING, 950);                                 // Phase 3 약속: 끈 벨을 다시 시작시키는 할 일이 나오지 않는다
        CHECK("muted: a stray ring does not restart the bell", o.acts == 0 && f.state == State::INCOMING && f.bell_muted);
        o = f.on(Ev::TICK, 40099);                               // 시간 제한은 벨이 온 시각(100) 기준 그대로 — 끈 시각으로 다시 세지 않는다
        CHECK("mute keeps the bell time limit", o.acts == 0 && f.state == State::INCOMING);
        o = f.on(Ev::TICK, 40100);
        CHECK("muted bell stuck 40s: reconnect, missed, flag cleared", f.state == State::OFFLINE && has(o, A_RECONNECT | A_TONE_STOP) && f.missed == 1 && !f.bell_muted);
    }
    {
        Fsm f = idle(); f.on(Ev::RING, 100);
        Out o = f.on(Ev::RING, 200);
        CHECK("bell starts once: a second ring while incoming does nothing", o.acts == 0 && f.state == State::INCOMING && !f.bell_muted);
    }
    {
        Fsm f = idle(); f.on(Ev::RING, 100); f.on(Ev::TAP_MUTE, 200);
        Out o = f.on(Ev::TAP_ACCEPT, 300);
        CHECK("mute then accept", f.state == State::ANSWERING && o.acts == (A_SEND_ACCEPT | A_TONE_STOP) && !f.bell_muted);
        Fsm g = idle(); g.on(Ev::RING, 100); g.on(Ev::TAP_MUTE, 200);
        o = g.on(Ev::TAP_REJECT, 300);
        CHECK("mute then plain reject", g.state == State::ENDING && o.acts == (A_SEND_REJECT | A_TONE_STOP) && o.msg == 0 && !g.bell_muted);
        Fsm h = idle(); h.on(Ev::RING, 100); h.on(Ev::TAP_MUTE, 200);
        o = h.on(Ev::TAP_REJECT, 300, Why::NONE, false, 2);
        CHECK("mute then message reject", h.state == State::ENDING && o.acts == (A_SEND_REJECT | A_TONE_STOP) && o.msg == 2 && !h.bell_muted);
    }
    {   // 벨 끄기는 다음 전화에 남지 않는다
        Fsm f = idle(); f.on(Ev::RING, 100); f.on(Ev::TAP_MUTE, 200);
        Out o = f.on(Ev::ENDED, 300, Why::HANGUP);
        CHECK("muted call canceled by caller: missed as before", f.state == State::IDLE && f.missed == 1 && o.acts == A_TONE_STOP && o.notice == Notice::MISSED && !f.bell_muted);
        o = f.on(Ev::RING, 400);
        CHECK("next call rings again", f.state == State::INCOMING && has(o, A_TONE_BELL) && !f.bell_muted);
        f.on(Ev::TAP_MUTE, 500);
        f.on(Ev::ENDED, 600, Why::TIMEOUT); o = f.on(Ev::RING, 600);    // 한 바퀴(같은 시각) 안에 ended → ring
        CHECK("ended then ring in one batch: the new call is not muted", f.state == State::INCOMING && has(o, A_TONE_BELL) && !f.bell_muted && f.missed == 2);
        o = f.on(Ev::TAP_MUTE, 650);
        CHECK("the new call can be muted again", o.acts == A_TONE_STOP && f.bell_muted);
        o = f.on(Ev::LINK_DOWN, 800);
        CHECK("muted call + link down", f.state == State::OFFLINE && has(o, A_TONE_STOP) && o.notice == Notice::MISSED && f.missed == 3 && !f.bell_muted);
    }
    {
        bool all = true;
        Fsm a;                                                      all = all && a.on(Ev::TAP_MUTE, 1).acts == 0 && a.state == State::OFFLINE && !a.bell_muted;
        Fsm b = idle();                                             all = all && b.on(Ev::TAP_MUTE, 1).acts == 0 && b.state == State::IDLE && !b.bell_muted;
        Fsm c = idle(); c.on(Ev::TAP_CALL, 1);                      all = all && c.on(Ev::TAP_MUTE, 2).acts == 0 && c.state == State::DIALING && !c.bell_muted;
        c.on(Ev::RINGING, 3);                                       all = all && c.on(Ev::TAP_MUTE, 4).acts == 0 && c.state == State::RINGBACK && !c.bell_muted;
        c.on(Ev::CONNECTED, 5);                                     all = all && c.on(Ev::TAP_MUTE, 6).acts == 0 && c.state == State::IN_CALL && !c.bell_muted;
        c.on(Ev::TAP_HANGUP, 7);                                    all = all && c.on(Ev::TAP_MUTE, 8).acts == 0 && c.state == State::ENDING && !c.bell_muted;
        Fsm d = idle(); d.on(Ev::RING, 1); d.on(Ev::TAP_ACCEPT, 2); all = all && d.on(Ev::TAP_MUTE, 3).acts == 0 && d.state == State::ANSWERING && !d.bell_muted;
        CHECK("mute outside INCOMING is ignored in every state", all);
    }

    // ── 메시지 거절(받는 쪽): TAP_REJECT 에 문구 번호를 실어 넣는다(DM-D7) ──
    {
        bool all = true;
        for (uint8_t n = 1; n <= MSG_COUNT; ++n) {
            Fsm f = idle(); f.on(Ev::RING, 100);
            Out o = f.on(Ev::TAP_REJECT, 900, Why::NONE, false, n);
            all = all && f.state == State::ENDING && o.acts == (A_SEND_REJECT | A_TONE_STOP) && o.msg == n && o.notice == Notice::NONE;
            o = f.on(Ev::TAP_CALL, 910);                                    // ended 를 받기 전에는 걸지 않는다
            all = all && !has(o, A_SEND_CALL) && f.state == State::ENDING;
            o = f.on(Ev::ENDED, 950, Why::REJECT, false, n);                // 교환기는 번호를 거절한 쪽에도 돌려준다
            all = all && f.state == State::IDLE && f.missed == 0 && o.notice == Notice::NONE && o.acts == 0 && o.msg == 0;
        }
        CHECK("message reject 1..3: ending + reject + number; my own ended shows nothing", all);
    }
    {
        Fsm f = idle(); f.on(Ev::RING, 1);
        Out o = f.on(Ev::TAP_REJECT, 2, Why::NONE, false, 0);
        CHECK("reject with number 0 = plain reject", f.state == State::ENDING && o.acts == (A_SEND_REJECT | A_TONE_STOP) && o.msg == 0);
        Fsm g = idle(); g.on(Ev::RING, 1);
        o = g.on(Ev::TAP_REJECT, 2, Why::NONE, false, 4);
        Fsm h = idle(); h.on(Ev::RING, 1);
        Out o2 = h.on(Ev::TAP_REJECT, 2, Why::NONE, false, 255);
        CHECK("reject with an out-of-range number still rejects, without a number",
              g.state == State::ENDING && has(o, A_SEND_REJECT) && o.msg == 0 && h.state == State::ENDING && has(o2, A_SEND_REJECT) && o2.msg == 0);
        Fsm k = idle(); k.on(Ev::RING, 1); k.on(Ev::TAP_REJECT, 1000, Why::NONE, false, 1);
        o = k.on(Ev::TICK, 3999);
        const bool waits = k.state == State::ENDING && o.acts == 0;
        o = k.on(Ev::TICK, 4000);
        CHECK("message reject unanswered 3s: reconnect", waits && k.state == State::OFFLINE && has(o, A_RECONNECT));
        Fsm q = idle();
        o = q.on(Ev::TAP_REJECT, 5, Why::NONE, false, 2);
        CHECK("message tap after the call already ended (idle): nothing sent", q.state == State::IDLE && o.acts == 0 && o.msg == 0 && o.notice == Notice::NONE);
        Fsm x = idle(); x.on(Ev::RING, 1); x.on(Ev::TAP_REJECT, 2, Why::NONE, false, 2);
        o = x.on(Ev::ENDED, 3, Why::HANGUP);                               // 문구를 누른 순간 거는 쪽이 취소 — 교환기는 내 reject 를 무시한다
        CHECK("message reject crossed with the caller's cancel: idle, not missed, silent", x.state == State::IDLE && x.missed == 0 && o.acts == 0 && o.notice == Notice::NONE);
    }

    // ── 거는 쪽: 문구와 함께 거절당하면 REJECT_MSG + 번호 ──
    {
        bool all = true;
        for (uint8_t n = 1; n <= MSG_COUNT; ++n) {
            Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2);
            Out o = f.on(Ev::ENDED, 900, Why::REJECT, false, n);
            all = all && f.state == State::IDLE && o.notice == Notice::REJECT_MSG && o.msg == n && o.acts == A_TONE_END;
            Fsm d = idle(); d.on(Ev::TAP_CALL, 1);                          // ringing 이 오기 전(DIALING)에 거절당해도 같다
            o = d.on(Ev::ENDED, 5, Why::REJECT, false, n);
            all = all && d.state == State::IDLE && o.notice == Notice::REJECT_MSG && o.msg == n && o.acts == A_TONE_END;
        }
        CHECK("caller: rejected with message 1..3 -> REJECT_MSG + number + end tone", all);
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2);
        Out o = f.on(Ev::ENDED, 900, Why::REJECT);
        CHECK("caller: reject without a number = REJECTED as before", f.state == State::IDLE && o.notice == Notice::REJECTED && o.msg == 0 && o.acts == A_TONE_END);
        bool plain = true;
        for (int n : { 0, 4, 99, 200, 255 }) {
            Fsm g = idle(); g.on(Ev::TAP_CALL, 1); g.on(Ev::RINGING, 2);
            Out x = g.on(Ev::ENDED, 900, Why::REJECT, false, (uint8_t)n);
            plain = plain && g.state == State::IDLE && x.notice == Notice::REJECTED && x.msg == 0 && x.acts == A_TONE_END;
        }
        CHECK("caller: reject with an unknown number = plain REJECTED", plain);
        bool gate = true;
        const Why whys[] = { Why::HANGUP, Why::TIMEOUT, Why::OFFLINE, Why::LOST };
        const Notice want[] = { Notice::REJECTED, Notice::NO_ANSWER, Notice::PEER_OFF, Notice::LOST };
        for (int i = 0; i < 4; ++i) {
            Fsm g = idle(); g.on(Ev::TAP_CALL, 1); g.on(Ev::RINGING, 2);
            Out x = g.on(Ev::ENDED, 900, whys[i], false, 2);
            gate = gate && g.state == State::IDLE && x.notice == want[i] && x.msg == 0;
        }
        CHECK("caller: a number on a non-reject ended is ignored", gate);
        Fsm r = idle(); r.on(Ev::TAP_CALL, 1); r.on(Ev::RINGING, 2); r.on(Ev::ENDED, 900, Why::REJECT, false, 3);
        o = r.on(Ev::TAP_CALL, 1000);                                       // 안내(10초)가 떠 있는 동안에도 다시 걸 수 있다
        CHECK("caller: can call again right after a message reject", r.state == State::DIALING && o.acts == A_SEND_CALL);
    }

    // ── 번호가 든 ended 가 다른 상태에 오면: 지금과 같다(번호는 무시) ──
    {
        Fsm f = idle();
        Out o = f.on(Ev::ENDED, 10, Why::REJECT, false, 2);
        CHECK("ended+number in idle ignored", f.state == State::IDLE && o.acts == 0 && o.notice == Notice::NONE && o.msg == 0 && f.missed == 0);
        f.on(Ev::RING, 20);
        o = f.on(Ev::ENDED, 30, Why::REJECT, false, 2);
        CHECK("ended+number during my bell = missed", f.state == State::IDLE && o.notice == Notice::MISSED && o.msg == 0 && f.missed == 1);
        Fsm g = idle(); g.on(Ev::TAP_CALL, 1); g.on(Ev::RINGING, 2); g.on(Ev::CONNECTED, 3);
        o = g.on(Ev::ENDED, 50, Why::REJECT, false, 2);
        CHECK("ended+number in call = call ended", g.state == State::IDLE && o.notice == Notice::CALL_ENDED && o.msg == 0);
    }
    CHECK("notice name REJECT_MSG (does not start with REJECTED)", std::string(notice_name(Notice::REJECT_MSG)) == "REJECT_MSG" && std::string(notice_name(Notice::REJECTED)) == "REJECTED"
          && std::string(notice_name(Notice::REJECT_MSG)).rfind("REJECTED", 0) != 0);
    CHECK("msg_valid: 1..3 only (0, 4, 2^32+1 are not)", MSG_COUNT == 3 && !msg_valid(0) && msg_valid(1) && msg_valid(2) && msg_valid(3) && !msg_valid(4) && !msg_valid(255) && !msg_valid(4294967297ull));


    // ── Phase 3c (FSM-D2, 계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D10): 통화 중 상대의 소리가 끊겼다(VOICE_LOST) ──
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1100); f.on(Ev::RINGING, 1200); f.on(Ev::CONNECTED, 2000);
        Out o = f.on(Ev::VOICE_LOST, 9000);
        CHECK("voice lost in a call: hang up, close the voice path, end tone — one batch (0x248), like pressing hang-up", o.acts == (A_SEND_HANGUP | A_VOICE_STOP | A_TONE_END) && o.acts == 0x248);
        CHECK("voice lost in a call: the notice is 'connection lost', and the phone waits for the relay's 'ended' (3 s)", o.notice == Notice::LOST && f.state == State::ENDING && f.deadline_ms == 9000 + ENDING_WAIT_MS);
        CHECK("voice lost in a call: the talk time is recorded", f.last_talk_ms == 7000);
        o = f.on(Ev::TAP_CALL, 9100);
        CHECK("... no new call until the relay answered (contract 9.4)", f.state == State::ENDING && o.acts == 0);
        o = f.on(Ev::ENDED, 9200, Why::HANGUP);
        CHECK("... the relay's 'ended' brings it back to idle quietly (the notice was already shown)", f.state == State::IDLE && o.acts == 0 && o.notice == Notice::NONE);
        Fsm g = idle(); g.on(Ev::RING, 1100); g.on(Ev::TAP_ACCEPT, 1200); g.on(Ev::CONNECTED, 1300); g.on(Ev::VOICE_LOST, 8000);
        o = g.on(Ev::TICK, 8000 + ENDING_WAIT_MS - 1);
        const bool waits = o.acts == 0 && g.state == State::ENDING;
        o = g.on(Ev::TICK, 8000 + ENDING_WAIT_MS);
        CHECK("... if the relay is silent too, reconnect after 3 s (FSM-D1) — the call cannot stay half open", waits && has(o, A_RECONNECT) && g.state == State::OFFLINE);
        Fsm h = idle(); h.on(Ev::RING, 1100); h.on(Ev::TAP_ACCEPT, 1200); h.on(Ev::CONNECTED, 1300); h.on(Ev::VOICE_LOST, 8000);
        o = h.on(Ev::VOICE_LOST, 8100);
        CHECK("... a second 'voice lost' while ending does nothing", o.acts == 0 && o.notice == Notice::NONE && h.state == State::ENDING && h.deadline_ms == 8000 + ENDING_WAIT_MS);
    }
    {
        // 통화 중이 아니면 아무 일도 없다(어느 상태에서든): 신호도 소리도 안내도 없고 상태·시간 제한이 그대로다
        bool nothing = true;
        Fsm a;                                                              { const Out o = a.on(Ev::VOICE_LOST, 5); if (o.acts || o.notice != Notice::NONE || a.state != State::OFFLINE) nothing = false; }
        Fsm b = idle();                                                     { const Out o = b.on(Ev::VOICE_LOST, 5); if (o.acts || o.notice != Notice::NONE || b.state != State::IDLE) nothing = false; }
        Fsm c = idle(); c.on(Ev::TAP_CALL, 10);                             { const int64_t d = c.deadline_ms; const Out o = c.on(Ev::VOICE_LOST, 20); if (o.acts || o.notice != Notice::NONE || c.state != State::DIALING || c.deadline_ms != d) nothing = false; }
        c.on(Ev::RINGING, 30);                                              { const int64_t d = c.deadline_ms; const Out o = c.on(Ev::VOICE_LOST, 40); if (o.acts || o.notice != Notice::NONE || c.state != State::RINGBACK || c.deadline_ms != d) nothing = false; }
        Fsm e = idle(); e.on(Ev::RING, 10);                                 { const int64_t d = e.deadline_ms; const Out o = e.on(Ev::VOICE_LOST, 20); if (o.acts || o.notice != Notice::NONE || e.state != State::INCOMING || e.deadline_ms != d || e.missed) nothing = false; }
        e.on(Ev::TAP_ACCEPT, 30);                                           { const int64_t d = e.deadline_ms; const Out o = e.on(Ev::VOICE_LOST, 40); if (o.acts || o.notice != Notice::NONE || e.state != State::ANSWERING || e.deadline_ms != d) nothing = false; }
        Fsm g = idle(); g.on(Ev::RING, 10); g.on(Ev::TAP_REJECT, 20);       { const int64_t d = g.deadline_ms; const Out o = g.on(Ev::VOICE_LOST, 30); if (o.acts || o.notice != Notice::NONE || g.state != State::ENDING || g.deadline_ms != d) nothing = false; }
        CHECK("voice lost outside a call does nothing in any state (offline, idle, dialing, ringback, incoming, answering, ending)", nothing);
        CHECK("the new event was added at the end — the values of the old ones did not move", (int)Ev::TICK == 13 && (int)Ev::VOICE_LOST == 14 && (int)Ev::LINK_UP == 0);
    }


    // ════ 전화기 3대 — 상대가 여럿 (설계서 docs/exec-plans/2026-10-08-phone-3way-design.md §8, call_fsm.h FSM-D3~D7) ════
    // 위의 시험은 한 줄도 고치지 않았다: peer 인자를 주지 않으면 slot 0 = 2대 시절의 "상대"다.
    const auto peer_ev   = [](Fsm &f, int64_t t, int slot, bool on) { return f.on(Ev::PEER, t, Why::NONE, on, 0, (uint8_t)slot); };
    const auto call_to   = [](Fsm &f, int64_t t, int slot) { return f.on(Ev::TAP_CALL, t, Why::NONE, false, 0, (uint8_t)slot); };
    const auto ring_from = [](Fsm &f, int64_t t, int slot) { return f.on(Ev::RING, t, Why::NONE, false, 0, (uint8_t)slot); };
    // FSM-D4: peer_online(2대 시절의 이름)은 언제나 slot 0 의 상태와 같고, 표(online_mask)에는 slot 0·1 의 비트만 선다
    const auto mirror    = [](const Fsm &f) { return f.peer_online == f.online(0) && f.online(0) == ((f.online_mask & 1) != 0) && f.online(1) == ((f.online_mask & 2) != 0) && (f.online_mask & ~3) == 0; };

    // ── slot 별 접속 상태 ──
    {
        Fsm f; f.on(Ev::LINK_UP, 10);
        CHECK("3way: two peer slots", MAX_PEERS == 2);
        CHECK("3way: after link up nobody is online", !f.online(0) && !f.online(1) && f.online_mask == 0 && mirror(f));
        peer_ev(f, 20, 1, true);
        CHECK("3way: peer slot 1 online changes only slot 1", !f.online(0) && f.online(1) && f.online_mask == 0x2 && !f.peer_online && mirror(f));
        peer_ev(f, 30, 0, true);
        CHECK("3way: peer slot 0 online", f.online(0) && f.online(1) && f.online_mask == 0x3 && f.peer_online && mirror(f));
        peer_ev(f, 40, 1, false);
        CHECK("3way: peer slot 1 offline leaves slot 0 alone", f.online(0) && !f.online(1) && f.online_mask == 0x1 && mirror(f));
        peer_ev(f, 50, 0, false);
        CHECK("3way: everybody off again", f.online_mask == 0 && mirror(f));
        f.on(Ev::PEER, 60, Why::NONE, true);
        CHECK("3way: PEER without a slot means slot 0 (the two-phone form)", f.online(0) && !f.online(1) && f.peer_online && mirror(f));
        Out o = peer_ev(f, 70, 1, true);
        CHECK("3way: a peer signal has no output and does not touch the call state", o.acts == 0 && o.notice == Notice::NONE && f.state == State::IDLE && f.deadline_ms == 0);
    }
    {
        Fsm f;
        peer_ev(f, 5, 0, true); peer_ev(f, 5, 1, true);
        CHECK("3way: peer signals while offline are ignored for every slot", f.online_mask == 0 && mirror(f) && f.state == State::OFFLINE);
        f.on(Ev::LINK_UP, 10); peer_ev(f, 11, 0, true); peer_ev(f, 11, 1, true);
        f.on(Ev::LINK_DOWN, 20);
        CHECK("3way: link down clears every slot", f.online_mask == 0 && mirror(f) && f.state == State::OFFLINE);
        f.on(Ev::LINK_UP, 30); peer_ev(f, 31, 0, true); peer_ev(f, 31, 1, true);
        f.on(Ev::LINK_UP, 40);
        CHECK("3way: a second link up clears every slot (the relay tells them again)", f.online_mask == 0 && mirror(f) && f.state == State::IDLE);
        peer_ev(f, 41, 1, true); call_to(f, 50, 1); f.on(Ev::TICK, 50 + SIGNAL_WAIT_MS);
        CHECK("3way: a relay-silent timeout (reconnect) clears every slot", f.state == State::OFFLINE && f.online_mask == 0 && mirror(f));
    }

    // ── 걸기: 그 slot 이 켜져 있을 때만, 대상 slot 은 Out.peer 에 ──
    {
        Fsm f; f.on(Ev::LINK_UP, 10); peer_ev(f, 11, 1, true);
        Out o = call_to(f, 100, 0);
        CHECK("3way: call slot 0 while only slot 1 is on: peer-off notice for slot 0, nothing sent", f.state == State::IDLE && o.acts == 0 && o.notice == Notice::PEER_OFF && o.peer == 0);
        o = call_to(f, 200, 1);
        CHECK("3way: call slot 1: dialing, send call, the target slot rides in Out.peer", f.state == State::DIALING && o.acts == A_SEND_CALL && o.peer == 1 && f.call_peer == 1 && f.deadline_ms == 200 + SIGNAL_WAIT_MS);
        o = f.on(Ev::RINGING, 300);
        CHECK("3way: ringback for slot 1", f.state == State::RINGBACK && has(o, A_TONE_RINGBACK) && o.peer == 1);
        o = f.on(Ev::CONNECTED, 400);
        CHECK("3way: in call with slot 1", f.state == State::IN_CALL && has(o, A_VOICE_START) && o.peer == 1 && f.call_peer == 1);
        o = f.on(Ev::TAP_HANGUP, 900);
        CHECK("3way: hang up the call with slot 1", f.state == State::ENDING && has(o, A_SEND_HANGUP | A_VOICE_STOP) && o.peer == 1);
        f.on(Ev::ENDED, 950, Why::HANGUP);
        CHECK("3way: call_peer keeps the last call's slot after it ended", f.state == State::IDLE && f.call_peer == 1);
        peer_ev(f, 960, 0, true);
        o = call_to(f, 1000, 0);
        CHECK("3way: the next call, to slot 0, moves call_peer", f.state == State::DIALING && o.acts == A_SEND_CALL && o.peer == 0 && f.call_peer == 0);
    }
    {
        Fsm f; f.on(Ev::LINK_UP, 10); peer_ev(f, 11, 0, true);
        call_to(f, 100, 0); f.on(Ev::RINGING, 110); f.on(Ev::ENDED, 200, Why::TIMEOUT);          // 직전 통화 = slot 0
        Out o = call_to(f, 300, 1);
        CHECK("3way: a refused tap (slot 1 is off) names the tapped slot and is not a call — call_peer stays", o.notice == Notice::PEER_OFF && o.peer == 1 && o.acts == 0 && f.call_peer == 0 && f.state == State::IDLE);
        Fsm a = f, b = f;
        const Out x = a.on(Ev::TAP_CALL, 400), y = call_to(b, 400, 0);
        CHECK("3way: TAP_CALL without a slot = TAP_CALL to slot 0", x.acts == A_SEND_CALL && y.acts == A_SEND_CALL && x.peer == 0 && y.peer == 0 && a.state == b.state && a.call_peer == 0 && b.call_peer == 0 && a.deadline_ms == b.deadline_ms);
    }
    {
        Fsm f; f.on(Ev::LINK_UP, 10); peer_ev(f, 11, 0, true); peer_ev(f, 11, 1, true);
        call_to(f, 100, 1); f.on(Ev::RINGING, 110);
        Out o = peer_ev(f, 120, 1, false);
        CHECK("3way: the callee's peer-off alone does not end the call (the relay's 'ended' does)", f.state == State::RINGBACK && o.acts == 0 && o.notice == Notice::NONE && !f.online(1) && f.online(0));
        o = f.on(Ev::ENDED, 130, Why::OFFLINE);
        CHECK("3way: ... then ended:offline names slot 1", f.state == State::IDLE && o.notice == Notice::PEER_OFF && o.peer == 1 && has(o, A_TONE_END));
        o = call_to(f, 200, 1);
        CHECK("3way: ... and slot 1 cannot be called until it is back", o.notice == Notice::PEER_OFF && o.acts == 0 && f.state == State::IDLE);
        o = call_to(f, 700, 0);
        CHECK("3way: ... while slot 0 still can", f.state == State::DIALING && o.acts == A_SEND_CALL && o.peer == 0);
    }
    {   // 안내에도 상대 slot 이 실린다(화면이 "누가"를 그릴 수 있게)
        Fsm f; f.on(Ev::LINK_UP, 10); peer_ev(f, 11, 1, true);
        call_to(f, 100, 1); f.on(Ev::RINGING, 110);
        Out o = f.on(Ev::ENDED, 30110, Why::TIMEOUT);
        const bool no_answer = o.notice == Notice::NO_ANSWER && o.peer == 1;
        call_to(f, 31000, 1); f.on(Ev::RINGING, 31010);
        o = f.on(Ev::ENDED, 31500, Why::REJECT, false, 3);
        const bool reject_msg = o.notice == Notice::REJECT_MSG && o.msg == 3 && o.peer == 1;
        call_to(f, 32000, 1); f.on(Ev::RINGING, 32010); f.on(Ev::CONNECTED, 32500);
        o = f.on(Ev::LINK_DOWN, 33000);
        const bool lost = o.notice == Notice::LOST && o.peer == 1 && f.call_peer == 1;
        f.on(Ev::LINK_UP, 34000); peer_ev(f, 34010, 1, true); call_to(f, 34100, 1);
        o = f.on(Ev::TICK, 34100 + SIGNAL_WAIT_MS);
        CHECK("3way: notices carry the slot they are about (no answer, message reject, lost, relay silent)", no_answer && reject_msg && lost && o.notice == Notice::RELAY_SILENT && o.peer == 1);
    }

    // ── 전화 옴: 건 쪽 slot 을 기억한다 ──
    {
        Fsm f; f.on(Ev::LINK_UP, 10);
        Out o = ring_from(f, 100, 1);
        CHECK("3way: ring from slot 1: incoming + bell, the caller is remembered", f.state == State::INCOMING && o.acts == A_TONE_BELL && o.peer == 1 && f.call_peer == 1 && f.deadline_ms == 100 + RING_GUARD_MS);
        CHECK("3way: ... even though that slot was never reported online (the relay decides who may ring)", !f.online(1));
        o = ring_from(f, 200, 0);
        CHECK("3way: a second ring, from another slot, while ringing changes nothing", o.acts == 0 && f.state == State::INCOMING && f.call_peer == 1 && f.deadline_ms == 100 + RING_GUARD_MS);
        o = f.on(Ev::ENDED, 300, Why::HANGUP);
        CHECK("3way: missed call from slot 1", f.state == State::IDLE && o.notice == Notice::MISSED && o.peer == 1 && f.missed == 1 && f.call_peer == 1);
        o = ring_from(f, 400, 0);
        CHECK("3way: the next ring, from slot 0", f.state == State::INCOMING && f.call_peer == 0 && o.peer == 0 && has(o, A_TONE_BELL));
        o = f.on(Ev::TAP_ACCEPT, 500);
        const bool answering = f.state == State::ANSWERING && o.acts == (A_SEND_ACCEPT | A_TONE_STOP) && o.peer == 0;
        o = f.on(Ev::CONNECTED, 600);
        CHECK("3way: accept and talk with slot 0", answering && f.state == State::IN_CALL && has(o, A_VOICE_START) && o.peer == 0 && f.call_peer == 0);
        o = ring_from(f, 700, 1);
        CHECK("3way: a stray ring from the other slot during a call is ignored (the caller is told 'busy' by the relay)", o.acts == 0 && f.state == State::IN_CALL && f.call_peer == 0);
    }
    {
        Fsm f; f.on(Ev::LINK_UP, 10);
        ring_from(f, 100, 1);
        Out o = f.on(Ev::TAP_REJECT, 200, Why::NONE, false, 2);
        CHECK("3way: message reject to slot 1", f.state == State::ENDING && o.acts == (A_SEND_REJECT | A_TONE_STOP) && o.msg == 2 && o.peer == 1);
        o = ring_from(f, 250, 0);
        CHECK("3way: a ring while waiting for 'ended' is ignored (as before)", o.acts == 0 && f.state == State::ENDING && f.call_peer == 1);
    }

    // ── FSM-D6: 거는 중에 온 ring — 같은 상대면 동시 발신(그대로 기다린다), 다른 상대면 내 call 이 졌다(벨) ──
    {
        Fsm f; f.on(Ev::LINK_UP, 10); peer_ev(f, 11, 0, true); peer_ev(f, 11, 1, true);
        call_to(f, 100, 1);
        Out o = ring_from(f, 105, 1);
        CHECK("3way: ring from the SAME slot while dialing is glare: keep dialing, no bell (as before)", f.state == State::DIALING && o.acts == 0 && f.call_peer == 1 && f.deadline_ms == 100 + SIGNAL_WAIT_MS);
        o = f.on(Ev::CONNECTED, 120);
        CHECK("3way: ... then connected", f.state == State::IN_CALL && has(o, A_VOICE_START) && o.peer == 1);
    }
    {
        Fsm f; f.on(Ev::LINK_UP, 10); peer_ev(f, 11, 0, true); peer_ev(f, 11, 1, true);
        call_to(f, 100, 0);
        Out o = ring_from(f, 105, 1);
        CHECK("3way: ring from ANOTHER slot while dialing: my call lost the race -> bell, incoming from that slot",
              f.state == State::INCOMING && o.acts == A_TONE_BELL && o.notice == Notice::NONE && o.peer == 1 && f.call_peer == 1 && f.deadline_ms == 105 + RING_GUARD_MS);
        CHECK("3way: ... nothing is sent for the lost call (the relay ignored it — no hangup is owed)", (o.acts & SENDS) == 0);
        o = f.on(Ev::TICK, 100 + SIGNAL_WAIT_MS);
        CHECK("3way: ... the dialing time limit no longer applies (the bell's does)", f.state == State::INCOMING && o.acts == 0);
        o = f.on(Ev::TAP_ACCEPT, 6000);
        const bool answering = f.state == State::ANSWERING && o.acts == (A_SEND_ACCEPT | A_TONE_STOP) && o.peer == 1;
        o = f.on(Ev::CONNECTED, 6100);
        CHECK("3way: ... accept it and talk with slot 1", answering && f.state == State::IN_CALL && has(o, A_VOICE_START) && f.call_peer == 1 && f.missed == 0);
    }
    {
        Fsm f; f.on(Ev::LINK_UP, 10); peer_ev(f, 11, 0, true); peer_ev(f, 11, 1, true);
        call_to(f, 100, 1); ring_from(f, 105, 0);
        Out o = f.on(Ev::ENDED, 900, Why::HANGUP);
        CHECK("3way: ... or the other caller gives up: missed call from slot 0, idle", f.state == State::IDLE && o.notice == Notice::MISSED && o.peer == 0 && f.missed == 1 && has(o, A_TONE_STOP));
        o = call_to(f, 2000, 1);
        CHECK("3way: ... and I can call again", f.state == State::DIALING && o.acts == A_SEND_CALL && o.peer == 1);
    }
    {
        Fsm f; f.on(Ev::LINK_UP, 10); peer_ev(f, 11, 0, true); peer_ev(f, 11, 1, true);
        call_to(f, 100, 0); f.on(Ev::RINGING, 110);
        Out o = ring_from(f, 120, 1);
        CHECK("3way: a ring during ringback is a stray (the relay already holds me as the caller): ignored", f.state == State::RINGBACK && o.acts == 0 && f.call_peer == 0 && f.deadline_ms == 110 + RING_GUARD_MS);
        o = ring_from(f, 130, 0);
        CHECK("3way: ... from either slot", f.state == State::RINGBACK && o.acts == 0 && f.call_peer == 0);
    }

    // ── FSM-D7: 통화 중·벨 중인 상대에게 걸었다(ended: busy) ──
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1);
        Out o = f.on(Ev::ENDED, 50, Why::BUSY);
        CHECK("busy while dialing: notice busy + end tone, back to idle", f.state == State::IDLE && o.notice == Notice::BUSY && o.acts == A_TONE_END && o.msg == 0 && f.deadline_ms == 0);
        f.on(Ev::TAP_CALL, 100); f.on(Ev::RINGING, 110);
        o = f.on(Ev::ENDED, 150, Why::BUSY);
        CHECK("busy during ringback: the same", f.state == State::IDLE && o.notice == Notice::BUSY && o.acts == A_TONE_END);
        o = f.on(Ev::TAP_CALL, 200);
        CHECK("busy: I can call again right away", f.state == State::DIALING && o.acts == A_SEND_CALL);
        o = f.on(Ev::ENDED, 250, Why::BUSY, false, 2);
        CHECK("busy with a stray message number: the number is ignored", f.state == State::IDLE && o.notice == Notice::BUSY && o.msg == 0);
        CHECK("busy is not counted as a missed call", f.missed == 0);
    }
    {
        // 다른 상태의 busy 는 모르는 사유(= 그냥 끝남)와 같다: 같은 자리에서 hangup 을 받은 것과 결과가 같아야 한다
        const auto like_hangup = [](const Fsm &base) {
            Fsm a = base, b = base;
            const Out x = a.on(Ev::ENDED, 5000, Why::BUSY), y = b.on(Ev::ENDED, 5000, Why::HANGUP);
            return x.acts == y.acts && x.notice == y.notice && x.notice != Notice::BUSY && x.msg == y.msg && x.peer == y.peer
                && a.state == b.state && a.missed == b.missed && a.deadline_ms == b.deadline_ms && a.last_talk_ms == b.last_talk_ms;
        };
        Fsm off;
        Fsm inc = idle(); inc.on(Ev::RING, 100);
        Fsm ans = idle(); ans.on(Ev::RING, 100); ans.on(Ev::TAP_ACCEPT, 200);
        Fsm talk = idle(); talk.on(Ev::TAP_CALL, 100); talk.on(Ev::RINGING, 110); talk.on(Ev::CONNECTED, 200);
        Fsm end = idle(); end.on(Ev::RING, 100); end.on(Ev::TAP_REJECT, 200);
        CHECK("busy outside dialing/ringback = an unknown reason, handled like hangup (offline, idle, incoming, answering, in call, ending)",
              like_hangup(off) && like_hangup(idle()) && like_hangup(inc) && like_hangup(ans) && like_hangup(talk) && like_hangup(end));
        inc.on(Ev::ENDED, 300, Why::BUSY);
        CHECK("busy during my bell = missed, like any other end", inc.state == State::IDLE && inc.missed == 1);
    }
    CHECK("new values were added at the end — the old ones did not move",
          (int)Why::LOST == 5 && (int)Why::BUSY == 6 && (int)Notice::REJECT_MSG == 8 && (int)Notice::BUSY == 9 && (int)Ev::VOICE_LOST == 14 && (int)State::ENDING == 7 && A_RECONNECT == (1 << 10));
    CHECK("notice name BUSY", std::string(notice_name(Notice::BUSY)) == "BUSY" && std::string(notice_name(Notice::REJECT_MSG)) == "REJECT_MSG");

    // ── FSM-D5: 범위 밖 slot 은 버린다(죽지 않는다) ──
    {
        Fsm f; f.on(Ev::LINK_UP, 10); peer_ev(f, 11, 0, true);
        bool kept = true;
        for (int s : { 2, 3, 5, 7, 8, 9, 64, 128, 255 }) {
            peer_ev(f, 20, s, true);  kept = kept && f.online_mask == 0x1 && mirror(f) && !f.online(s);
            peer_ev(f, 21, s, false); kept = kept && f.online_mask == 0x1 && mirror(f);
        }
        CHECK("out-of-range slot: PEER is ignored (on and off)", kept && !f.online(-1) && !f.online(2) && !f.online(1000) && !f.online(-1000));
        bool quiet = true;
        for (int s : { 2, 5, 8, 255 }) {
            Out o = call_to(f, 100, s);
            quiet = quiet && f.state == State::IDLE && o.acts == 0 && o.notice == Notice::NONE && f.call_peer == 0 && f.deadline_ms == 0;
            o = ring_from(f, 200, s);
            quiet = quiet && f.state == State::IDLE && o.acts == 0 && o.notice == Notice::NONE && f.missed == 0 && f.call_peer == 0;
        }
        CHECK("out-of-range slot: a call tap and a ring do nothing at all (no call, no bell, no notice)", quiet);
        call_to(f, 300, 0);
        Out o = ring_from(f, 305, 5);
        CHECK("out-of-range slot: a ring while dialing does not turn the call into a bell", f.state == State::DIALING && o.acts == 0 && f.call_peer == 0);
        f.on(Ev::RINGING, 310, Why::NONE, false, 0, 5);
        o = f.on(Ev::ENDED, 400, Why::TIMEOUT, false, 0, 200);
        CHECK("other events never read the slot: ringing and ended work with any value there", f.state == State::IDLE && o.notice == Notice::NO_ANSWER && o.peer == 0 && f.call_peer == 0);
    }

    // ── 마구잡이 사건 20만 개: 죽지 않고, 약속이 늘 지켜진다 ──
    {
        uint32_t seed = 20261008u;
        const auto rnd = [&seed](uint32_t n) { seed = seed * 1664525u + 1013904223u; return (seed >> 8) % n; };
        Fsm f;
        int64_t now = 0;
        bool sound = true;
        long calls = 0, bells = 0;
        for (int i = 0; i < 200000 && sound; ++i) {
            const Ev ev = (Ev)rnd(15);
            const uint8_t slot = (uint8_t)(rnd(4) ? rnd(2) : rnd(256));              // 대개 0·1, 넷에 한 번은 아무 값
            const bool was_on = f.online(slot);
            const State before = f.state;
            const uint8_t cp = f.call_peer;
            now += rnd(3000);
            const Out o = f.on(ev, now, (Why)rnd(7), rnd(2) != 0, (uint8_t)rnd(6), slot);
            sound = mirror(f) && f.call_peer < MAX_PEERS && o.peer < MAX_PEERS && (int)f.state <= (int)State::ENDING
                 && (f.state != State::OFFLINE || f.online_mask == 0);
            if (o.acts & A_SEND_CALL) { ++calls; sound = sound && ev == Ev::TAP_CALL && before == State::IDLE && slot < MAX_PEERS && was_on && o.peer == slot && f.call_peer == slot && f.state == State::DIALING; }
            if (o.acts & A_TONE_BELL) { ++bells; sound = sound && ev == Ev::RING && slot < MAX_PEERS && o.peer == slot && f.call_peer == slot && f.state == State::INCOMING && (before == State::IDLE || (before == State::DIALING && cp != slot)); }
            if (o.acts & A_VOICE_START) sound = sound && f.state == State::IN_CALL && ev == Ev::CONNECTED && (before == State::DIALING || before == State::RINGBACK || before == State::ANSWERING);
            if (slot >= MAX_PEERS && (ev == Ev::PEER || ev == Ev::TAP_CALL || ev == Ev::RING)) sound = sound && o.acts == 0 && o.notice == Notice::NONE && f.state == before && f.call_peer == cp;
            if (!sound) printf("       step %d: ev=%d slot=%d before=%s after=%s acts=0x%x\n", i, (int)ev, (int)slot, state_name(before), state_name(f.state), (unsigned)o.acts);
        }
        CHECK("random walk (200k events, any slot value): invariants hold, calls only to an online slot, bells only from a valid slot", sound && calls > 100 && bells > 100);
    }

    printf("\n%d ok, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
