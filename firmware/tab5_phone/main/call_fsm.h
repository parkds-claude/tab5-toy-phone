// call_fsm.h — 통화 상태 기계. 하드웨어·시계·네트워크와 무관한 순수 로직(맥에서 단위 시험: tools/test_call_fsm.cpp).
//
// 사건(화면 터치·교환기 신호·연결 상태·시간 경과)을 넣으면 다음 상태와 "할 일"(보낼 신호, 소리, 음성 켜기/끄기)을 돌려준다.
// 설계서 docs/exec-plans/2026-10-03-tab5-phone.md §9.4 "교환기-기기 계약"을 전제로 한다:
//   - "끝났다"는 교환기의 ended 하나로 판단한다. hangup·reject 를 보낸 뒤에는 ended 를 받을 때까지 새 call 을 보내지 않는다.
//   - 대기 상태에서 받은 ended 는 버린다(신호에 통화 번호가 없어 앞 통화의 것인지 구분할 수 없다).
//   - 교환기는 상태에 안 맞는 신호에 응답하지 않는다 → 기다리는 상태마다 시간 제한을 두고, 넘으면 연결을 끊고 다시 붙는다
//     (FSM-D1: 다시 붙으면 교환기가 그 통화를 끝내고 옛 연결의 뒤늦은 프레임을 버리므로 양쪽 상태가 확실히 맞는다.
//      비용: 재연결 1~2초 동안 전화를 못 받는다. 탈출구: 시간 제한 값을 늘리거나 A_RECONNECT 대신 대기로 복귀).
//   - 받기를 누르지 않았는데 connected 가 오면 통화로 들어가지 않는다(PH-D6: 원치 않을 때 소리가 새지 않게) → 재연결로 맞춘다.
// 수신거부 메시지·벨 끄기(docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md §5.3):
//   - 벨 끄기(TAP_MUTE)는 이번 전화의 벨만 멈춘다(A_TONE_STOP). 신호는 나가지 않고 상태·시간 제한도 그대로다.
//     벨 시작(A_TONE_BELL)은 IDLE → INCOMING 으로 들어갈 때 한 번만 낸다 → 끈 뒤에는 그 통화가 끝날 때까지 벨을 다시 시작시키는 할 일이 나오지 않는다
//     (Phase 3 소리 모듈과의 약속: A_TONE_STOP 을 받으면 스스로 다시 울리지 않는다, 이미 멈춘 뒤의 A_TONE_STOP 도 무해해야 한다).
//   - 메시지 거절은 새 사건이 아니라 TAP_REJECT 에 문구 번호(msg, 1~MSG_COUNT)를 실어 넣는다(DM-D7: 전이·할 일이 그냥 거절과 같고 다른 것은 번호뿐.
//     비용: 번호를 넣으려면 why·flag 자리를 채워 적어야 한다. 탈출구: TAP_REJECT_MSG 사건으로 나눈다).
//   - "벨 꺼짐"은 여기(bell_muted)가, "메시지 고르기 화면 열림"은 화면 쪽(uilayout::DeclinePicker)이 기억한다(DM-D8).
// 수신 무음 감시(Phase 3c, docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D10 — 3b 최종 검수 M-9):
//   - FSM-D2 통화 중인데 상대의 소리 프레임이 한동안 오지 않으면(판단은 voicecore::RxWatch — 5초) app 이 VOICE_LOST 를 넣는다. 통화 중(IN_CALL)에만 뜻이 있다:
//     끊기를 누른 것과 같은 길(hangup 을 보내고 ENDING 에서 ended 를 기다린다 — §9.4 의 계약 그대로)로 끝내되, 안내는 "연결이 끊어졌습니다"(LOST)다.
//     이유: 상대 기기가 꺼지거나 재부팅되면 교환기가 그것을 알 때까지(실측 17초, 계약상 45초) 내 마이크가 열린 채 아무에게도 닿지 않는 소리를 보낸다.
//     무음 구간 생략(DTX)을 쓰지 않으므로 통화 중에는 프레임이 20ms 마다 온다 — 5초 동안 하나도 안 왔다면 상대는 없다.
//     교환기가 살아 있으면 hangup 에 ended 로 답하고(상대가 아직 붙어 있으면 상대에게도 간다), 교환기까지 죽었으면 ENDING 의 3초 제한이 다시 붙게 한다(FSM-D1).
//     비용: 받는 길만 5초 넘게 막혔다 풀리는 망에서는 살아 있는 통화를 끊는다(보내는 길이 2초 막히면 어차피 연결이 끊긴다 — relay_link RL-D6).
//     탈출구: app.cpp 에서 VOICE_LOST 를 넣는 한 줄을 지운다(이 사건을 아무도 넣지 않으면 3b 와 같다).
// 전화기 3대 — 상대가 여럿(docs/exec-plans/2026-10-08-phone-3way-design.md §6·§8, 결정 TW-D4·D9). 이 헤더는 Tab5 와 스택짱의 펌웨어가 함께 쓴다(TW-D11):
//   - FSM-D3 상대를 slot(0 … MAX_PEERS-1 — 순서는 roster.h RS-D1)으로 센다. on() 의 맨 끝 인자 peer 가 "이 사건이 가리키는 상대 slot"이고 기본값은 0 이다
//     → 2대 구성은 slot 0 만 쓰는 것과 같아, 옛 호출·옛 시험이 한 줄도 바뀌지 않는다.
//     peer 를 읽는 사건은 셋뿐이다: PEER(그 slot 의 접속 상태만 바꾼다) · TAP_CALL(대기에서: 그 slot 에게 건다 — 켜져 있을 때만) · RING(대기와 거는 중에서: 건 쪽 slot 을 본다 — FSM-D6).
//     나머지 사건(ringing·connected·ended·다른 버튼·시간)은 peer 를 보지 않는다: 통화는 한 번에 하나이고 그 상대는 call_peer 가 기억한다.
//     call_peer = 지금 통화·벨의 상대 slot, 대기 중에는 직전 것(화면이 "누구에게 걸었나·누가 걸었나"를 그릴 때 읽는다). 걸기가 나갈 때와 벨이 시작될 때만 바뀐다.
//     Out.peer  = 그 결과(할 일·안내)가 가리키는 상대 slot — A_SEND_CALL 의 대상이고, 안내와 함께면 "누구 이야기인가"다. 값은 그때의 call_peer 이고 예외는 하나다:
//                 꺼진 상대를 누른 TAP_CALL(안내 PEER_OFF)은 통화가 아니어서 call_peer 를 바꾸지 않으므로, 누른 slot 을 싣는다.
//     비용: 인자가 여섯으로 늘었다(peer 를 주려면 why·flag·msg 자리를 채워 적어야 한다 — DM-D7 과 같은 비용). 탈출구: 사건을 구조체 하나로 묶는다.
//   - FSM-D4 접속 상태는 online_mask(비트 n = slot n)에 두고, 2대 시절의 이름 peer_online 은 slot 0 의 사본으로 남긴다(옛 화면·옛 시험이 이 이름을 읽는다).
//     둘은 set_online·clear_online 에서만 함께 바뀐다(시험이 마구잡이 사건 20만 개로 "늘 같다"를 고정한다). 새 코드는 online(slot) 을 쓴다.
//     비용: 같은 사실이 두 칸에 있다(밖에서 한쪽만 고쳐 쓰면 어긋난다 — 둘 다 읽기만 할 것). 탈출구: 읽는 쪽을 모두 online(0) 으로 옮긴 뒤 peer_online 을 지운다.
//   - FSM-D5 범위 밖 slot(MAX_PEERS 이상)의 PEER·TAP_CALL·RING 은 버린다: 상태·할 일·안내가 그대로다. slot 0 으로 낮춰 받지 않는다
//     — 모르는 상대의 벨을 "slot 0 의 전화"로 울리면 엉뚱한 캐릭터가 뜬다(roster.h RS-D4 와 같은 판단). 정상 길에서는 오지 않는다(app 이 roster::route_slot 으로 걸러 넣는다).
//     비용: 호출자의 버그로 slot 이 틀리면 벨이 울리지 않는다(교환기는 30초 뒤 끝낸다). 탈출구: step() 의 known 검사를 "0 으로 낮춤"으로 바꾼다.
//   - FSM-D6 거는 중(DIALING)에 온 ring 은 건 쪽을 본다. 내가 걸던 그 상대면 동시 발신 — 교환기가 내 call 을 받기로 쳐서 곧 connected 가 온다(§6.4-1) → 지금처럼 그대로 기다린다.
//     다른 상대면 내 call 이 졌다: 교환기는 내가 이미 그 벨에 묶여 있어 내 call 을 응답 없이 버렸다(§6.4-2) → 벨 화면으로 넘어간다(보낼 것은 없다 — 버려진 call 에는 hangup 을 빚지지 않았다).
//     이유: 그대로 기다리면 5초 뒤 "교환기 응답 없음"으로 다시 붙고(FSM-D1), 그 재접속이 걸어 온 쪽의 벨까지 끝낸다(offline) — 두 통화가 다 실패한다.
//     비용: 통화 버튼을 눌렀는데 벨 화면이 뜬다(먼저 걸려 온 전화가 이긴다). 교환기가 §6.4 와 다르게 내 call 도 받아 버렸다면 그 통화의 신호가 벨 중에 온다
//           — ended 면 부재중으로 세고 대기, connected 면 다시 붙는다(받기 전에는 마이크가 열리지 않는다 — PH-D6 그대로). 멈추지는 않는다.
//     연결음 중(RINGBACK)에는 해당 없다 — 교환기가 나를 거는 쪽으로 묶은 뒤라 남의 벨이 오지 않는다(와도 버린다).
//     탈출구: DIALING 의 RING 갈래를 지우면 2대 시절 동작(어느 ring 이든 그대로 기다린다).
//   - FSM-D7 ended 의 새 사유 busy(TW-D9: 통화 중·벨 중인 상대에게 걸었다) → 거는 중·연결음에서 안내 BUSY + 종료음, 대기로. 그 밖의 상태에서는 모르는 사유와 같다
//     (relay_proto.h 가 모르는 사유를 HANGUP 으로 읽는 것과 같은 결과 — 벨 중이면 부재중, 통화 중이면 통화 종료, 끊는 중이면 조용히, 대기면 버림).
//   - enum 에 값을 더할 때는 맨 끝에만(Why::BUSY, Notice::BUSY). 앞의 값이 움직이면 기록 줄(FSM … acts=0x… notice=…)을 읽는 도구·옛 기록과 어긋난다.
#pragma once
#include <cstdint>

#include "roster.h"

namespace callfsm {

enum class State : uint8_t {
    OFFLINE,     // 교환기에 붙지 않음
    IDLE,        // 대기
    DIALING,     // call 을 보냈고 교환기 응답(ringing/ended/connected)을 기다림
    RINGBACK,    // 상대 벨이 울리는 중(연결음)
    INCOMING,    // 전화 옴(벨)
    ANSWERING,   // accept 를 보냈고 connected 를 기다림
    IN_CALL,     // 통화 중
    ENDING,      // hangup/reject 를 보냈고 ended 를 기다림
};

enum class Why : uint8_t { NONE, HANGUP, REJECT, TIMEOUT, OFFLINE, LOST, BUSY };   // 교환기 ended 의 why. BUSY(상대가 통화 중·벨 중 — FSM-D7)는 맨 끝에 더했다 — 앞의 값은 그대로

enum class Ev : uint8_t {
    LINK_UP, LINK_DOWN,                                          // 교환기 연결됨 / 끊김
    PEER,                                                        // 상대 기기 접속 여부 (flag = online)
    RINGING, RING, CONNECTED, ENDED,                             // 교환기 신호 (ENDED 는 why 와 함께)
    TAP_CALL, TAP_CANCEL, TAP_ACCEPT, TAP_REJECT, TAP_HANGUP,    // 화면 버튼 (TAP_REJECT 는 문구 번호 msg 를 실을 수 있다)
    TAP_MUTE,                                                    // 벨 끄기(이번 전화만)
    TICK,                                                        // 주기 호출(시간 제한 검사). 100ms 간격이면 충분
    VOICE_LOST,                                                  // (Phase 3c) 통화 중인데 상대의 소리 프레임이 5초째 오지 않는다(FSM-D2). 맨 끝에 더했다 — 앞의 값은 그대로
};

enum Act : uint16_t {                 // 할 일(비트 묶음)
    A_SEND_CALL     = 1 << 0,
    A_SEND_ACCEPT   = 1 << 1,
    A_SEND_REJECT   = 1 << 2,
    A_SEND_HANGUP   = 1 << 3,
    A_TONE_RINGBACK = 1 << 4,         // 연결음 시작(반복)
    A_TONE_BELL     = 1 << 5,         // 벨소리 시작(반복)
    A_TONE_END      = 1 << 6,         // 종료음 한 번(울리던 소리는 멈춘다)
    A_TONE_STOP     = 1 << 7,         // 울리던 소리 멈춤
    A_VOICE_START   = 1 << 8,         // 마이크·스피커 음성 경로 켜기
    A_VOICE_STOP    = 1 << 9,
    A_RECONNECT     = 1 << 10,        // 교환기 연결을 끊고 다시 붙기
};

enum class Notice : uint8_t {         // 화면에 잠깐 띄울 안내
    NONE,
    PEER_OFF,       // 상대 기기가 꺼져 있음
    NO_ANSWER,      // 받지 않음
    REJECTED,       // 상대가 받지 않고 끊음
    LOST,           // 연결이 끊어졌습니다
    CALL_ENDED,     // 통화 종료
    MISSED,         // 부재중
    RELAY_SILENT,   // 교환기 응답 없음
    REJECT_MSG,     // 상대가 문구와 함께 거절(Out.msg = 문구 번호)
    BUSY,           // 상대가 통화 중·벨 중이다(FSM-D7). 맨 끝에 더했다 — 앞의 값은 그대로
};

const uint8_t MSG_COUNT = 3;             // 거절 문구 수. 번호는 1~MSG_COUNT, 0 = 문구 없음
inline bool msg_valid(uint64_t n) { return n >= 1 && n <= MSG_COUNT; }   // 64비트로 받는다 — 잘라서 비교하면 2^32+1 이 1 이 된다

const uint8_t MAX_PEERS = roster::MAX_PEERS;   // 상대 slot 의 개수(FSM-D3). slot 은 0 … MAX_PEERS-1

struct Out {
    uint16_t acts = 0;
    Notice notice = Notice::NONE;
    uint8_t msg = 0;                     // A_SEND_REJECT 와 함께: 실어 보낼 문구 번호 / REJECT_MSG 와 함께: 보여 줄 문구 번호. 없으면 0
    uint8_t peer = 0;                    // 이 결과가 가리키는 상대 slot(FSM-D3): A_SEND_CALL 이면 걸 대상, 안내면 "누구 이야기인가". 늘 0 … MAX_PEERS-1
};

const int64_t SIGNAL_WAIT_MS = 5000;     // call·accept 를 보낸 뒤 교환기 응답을 기다리는 한도
const int64_t ENDING_WAIT_MS = 3000;     // hangup·reject 를 보낸 뒤 ended 를 기다리는 한도(§9.4)
const int64_t RING_GUARD_MS  = 40000;    // 벨·연결음 한도. 교환기가 30초에 끝내므로 정상이면 여기까지 오지 않는다

struct Fsm {
    State   state = State::OFFLINE;
    bool    peer_online = false;         // slot 0 의 상대가 켜져 있나 = online(0). 2대 시절의 이름(FSM-D4: online_mask 의 사본 — 읽기만 한다). 새 코드는 online(slot)
    int64_t deadline_ms = 0;             // 현재 상태의 시간 제한(없으면 0)
    int64_t call_start_ms = 0;           // 통화 시작 시각(화면의 통화 시간 표시용)
    int64_t last_talk_ms = 0;            // 직전 통화 길이
    uint16_t missed = 0;                 // 부재중 건수
    int64_t missed_at_ms = 0;            // 마지막 부재중 시각
    bool    bell_muted = false;          // 이번 전화의 벨을 껐다. INCOMING 을 떠나는 모든 길(enter·offline)에서 풀린다
    uint8_t online_mask = 0;             // 상대별 접속 상태: 비트 n = slot n 의 상대가 켜져 있다(FSM-D4). 읽기만 한다 — 바꾸는 것은 PEER 사건뿐
    uint8_t call_peer = 0;               // 지금 통화·벨의 상대 slot, 대기 중에는 직전 것(FSM-D3). 걸기가 나갈 때와 벨이 시작될 때만 바뀐다

    void clear_missed() { missed = 0; }
    bool online(int slot) const { return slot >= 0 && slot < MAX_PEERS && ((online_mask >> slot) & 1) != 0; }   // 그 slot 의 상대가 켜져 있나. 범위 밖이면 false

    // 사건 하나를 넣는다. peer = 이 사건이 가리키는 상대 slot — PEER·TAP_CALL·RING 만 읽는다(FSM-D3). 주지 않으면 slot 0.
    Out on(Ev ev, int64_t now_ms, Why why = Why::NONE, bool flag = false, uint8_t msg = 0, uint8_t peer = 0)
    {
        Out o = step(ev, now_ms, why, flag, msg, peer);
        if (!(ev == Ev::TAP_CALL && o.notice == Notice::PEER_OFF)) o.peer = call_peer;   // FSM-D3: 꺼진 상대를 누른 터치만 "누른 slot"(step 이 실었다), 그 밖은 통화의 상대
        return o;
    }

private:
    Out step(Ev ev, int64_t now_ms, Why why, bool flag, uint8_t msg, uint8_t peer)
    {
        Out o;
        const bool known = peer < MAX_PEERS;                    // FSM-D5: 범위 밖 slot 의 PEER·TAP_CALL·RING 은 버린다
        switch (ev) {
        case Ev::LINK_UP:
            if (state != State::OFFLINE) o = drop(now_ms);      // 끊김을 못 본 채 다시 붙은 경우: 먼저 정리
            state = State::IDLE; clear_online(); deadline_ms = 0;   // 상대 상태는 교환기가 접속 직후 전부 다시 알려 준다(§6.3)
            return o;
        case Ev::LINK_DOWN:
            return drop(now_ms);
        case Ev::PEER:
            if (state != State::OFFLINE && known) set_online(peer, flag);   // 그 slot 만 바꾼다. 통화 상태는 건드리지 않는다(끝났다는 것은 ended 가 알린다)
            return o;
        case Ev::TICK:
            return tick(now_ms);
        default:
            break;
        }

        switch (state) {
        case State::OFFLINE:
            break;                                              // 뒤늦은 신호·터치는 모두 버린다

        case State::IDLE:
            if (ev == Ev::TAP_CALL && known) {
                if (!online(peer)) { o.notice = Notice::PEER_OFF; o.peer = peer; break; }   // 통화가 아니다 — call_peer 는 그대로, 안내에는 누른 slot
                call_peer = peer;                               // 대상은 on() 이 Out.peer 에 싣는다
                o.acts = A_SEND_CALL; enter(State::DIALING, now_ms + SIGNAL_WAIT_MS);
            } else if (ev == Ev::RING && known) {               // 건 쪽을 기억한다. 그 slot 이 "꺼짐"으로 보이고 있어도 받는다 — 누가 걸 수 있는지는 교환기가 정한다
                call_peer = peer;
                o.acts = A_TONE_BELL; enter(State::INCOMING, now_ms + RING_GUARD_MS);
            }
            break;                                              // ended·connected·ringing 은 버린다(§9.4)

        case State::DIALING:
        case State::RINGBACK:
            if (ev == Ev::RINGING && state == State::DIALING) {
                o.acts = A_TONE_RINGBACK; enter(State::RINGBACK, now_ms + RING_GUARD_MS);
            } else if (ev == Ev::CONNECTED) {
                o = begin_call(now_ms);
            } else if (ev == Ev::ENDED) {
                o.acts = A_TONE_END;
                o.notice = why == Why::TIMEOUT ? Notice::NO_ANSWER
                         : why == Why::OFFLINE ? Notice::PEER_OFF
                         : why == Why::LOST    ? Notice::LOST
                         : why == Why::BUSY    ? Notice::BUSY        // FSM-D7: 상대가 통화 중·벨 중
                         :                       Notice::REJECTED;   // reject, 또는 받는 쪽이 벨 중에 보낸 hangup
                if (why == Why::REJECT && msg_valid(msg)) { o.notice = Notice::REJECT_MSG; o.msg = msg; }   // 문구와 함께 거절. 번호는 reject 에만 뜻이 있다
                enter(State::IDLE, 0);
            } else if (ev == Ev::TAP_CANCEL) {
                o.acts = A_SEND_HANGUP | A_TONE_STOP; enter(State::ENDING, now_ms + ENDING_WAIT_MS);
            } else if (ev == Ev::RING && state == State::DIALING && known && peer != call_peer) {   // FSM-D6: 다른 상대의 벨이 먼저였다 — 내 call 은 교환기가 버렸다
                call_peer = peer;
                o.acts = A_TONE_BELL; enter(State::INCOMING, now_ms + RING_GUARD_MS);
            }
            break;                                              // 거는 중에 온 "그 상대의" ring(동시 발신)은 그대로 기다린다 → connected. 연결음 중의 ring 은 버린다

        case State::INCOMING:
            if (ev == Ev::TAP_ACCEPT) {
                o.acts = A_SEND_ACCEPT | A_TONE_STOP; enter(State::ANSWERING, now_ms + SIGNAL_WAIT_MS);
            } else if (ev == Ev::TAP_REJECT) {                  // 번호가 1~MSG_COUNT 면 메시지 거절, 아니면 그냥 거절(번호 0)
                o.acts = A_SEND_REJECT | A_TONE_STOP; o.msg = msg_valid(msg) ? msg : 0; enter(State::ENDING, now_ms + ENDING_WAIT_MS);
            } else if (ev == Ev::TAP_MUTE) {                    // 상태·시간 제한은 그대로. 두 번째부터는 아무 일도 없다
                if (!bell_muted) { bell_muted = true; o.acts = A_TONE_STOP; }
            } else if (ev == Ev::ENDED) {                       // timeout·거는 쪽 취소(hangup)·거는 쪽 사라짐(offline) 모두 부재중
                o.acts = A_TONE_STOP; o.notice = Notice::MISSED; miss(now_ms); enter(State::IDLE, 0);
            } else if (ev == Ev::CONNECTED) {                   // 받기를 누르지 않았다 → 통화로 들어가지 않는다
                o.acts = A_TONE_STOP | A_RECONNECT; o.notice = Notice::RELAY_SILENT; miss(now_ms); offline();
            }
            break;

        case State::ANSWERING:
            if (ev == Ev::CONNECTED) {
                o = begin_call(now_ms);
            } else if (ev == Ev::ENDED) {                       // 받는 순간 거는 쪽이 취소 — 부재중으로 세지 않는다
                o.notice = why == Why::LOST ? Notice::LOST : Notice::CALL_ENDED; enter(State::IDLE, 0);
            } else if (ev == Ev::TAP_HANGUP) {
                o.acts = A_SEND_HANGUP; enter(State::ENDING, now_ms + ENDING_WAIT_MS);
            }
            break;

        case State::IN_CALL:
            if (ev == Ev::TAP_HANGUP) {
                last_talk_ms = now_ms - call_start_ms;
                o.acts = A_SEND_HANGUP | A_VOICE_STOP | A_TONE_END; enter(State::ENDING, now_ms + ENDING_WAIT_MS);
            } else if (ev == Ev::ENDED) {
                last_talk_ms = now_ms - call_start_ms;
                o.acts = A_VOICE_STOP | A_TONE_END;
                o.notice = why == Why::LOST ? Notice::LOST : Notice::CALL_ENDED;
                enter(State::IDLE, 0);
            } else if (ev == Ev::VOICE_LOST) {                  // FSM-D2: 끊기를 누른 것과 같은 길 + "연결이 끊어졌습니다"
                last_talk_ms = now_ms - call_start_ms;
                o.acts = A_SEND_HANGUP | A_VOICE_STOP | A_TONE_END; o.notice = Notice::LOST;
                enter(State::ENDING, now_ms + ENDING_WAIT_MS);
            }
            break;

        case State::ENDING:
            if (ev == Ev::ENDED) enter(State::IDLE, 0);         // 내가 끝낸 통화 — 안내·소리 없음
            break;                                              // 그 전의 call 터치·뒤늦은 connected 는 버린다
        }
        return o;
    }

    void enter(State s, int64_t deadline) { state = s; deadline_ms = deadline; bell_muted = false; }
    void offline() { state = State::OFFLINE; clear_online(); deadline_ms = 0; bell_muted = false; }   // call_peer 는 남긴다(끊긴 통화의 안내가 "누구"를 가리킨다)
    // FSM-D4: 표와 사본(peer_online = slot 0)은 이 둘에서만, 함께 바뀐다. slot 은 부르는 쪽이 범위를 확인했다
    void set_online(uint8_t slot, bool on)
    {
        online_mask = (uint8_t)(on ? (online_mask | (1u << slot)) : (online_mask & ~(1u << slot)));
        peer_online = (online_mask & 1u) != 0;
    }
    void clear_online() { online_mask = 0; peer_online = false; }
    void miss(int64_t now_ms) { if (missed < 0xFFFF) ++missed; missed_at_ms = now_ms; }

    Out begin_call(int64_t now_ms)
    {
        Out o; o.acts = A_TONE_STOP | A_VOICE_START;
        call_start_ms = now_ms; enter(State::IN_CALL, 0);
        return o;
    }

    Out drop(int64_t now_ms)                                    // 교환기 연결이 끊김
    {
        Out o;
        switch (state) {
        case State::IN_CALL:
            last_talk_ms = now_ms - call_start_ms;
            o.acts = A_VOICE_STOP | A_TONE_END; o.notice = Notice::LOST; break;
        case State::DIALING: case State::RINGBACK: case State::ANSWERING:
            o.acts = A_TONE_STOP; o.notice = Notice::LOST; break;
        case State::INCOMING:
            o.acts = A_TONE_STOP; o.notice = Notice::MISSED; miss(now_ms); break;
        default:
            break;                                              // OFFLINE·IDLE·ENDING: 조용히
        }
        offline();
        return o;
    }

    Out tick(int64_t now_ms)
    {
        Out o;
        if (deadline_ms == 0 || now_ms < deadline_ms) return o;
        switch (state) {
        case State::DIALING: case State::ANSWERING:
            o.acts = A_RECONNECT | A_TONE_STOP; o.notice = Notice::RELAY_SILENT; break;
        case State::RINGBACK:
            o.acts = A_RECONNECT | A_TONE_STOP; o.notice = Notice::RELAY_SILENT; break;
        case State::INCOMING:
            o.acts = A_RECONNECT | A_TONE_STOP; o.notice = Notice::MISSED; miss(now_ms); break;
        case State::ENDING:
            o.acts = A_RECONNECT; break;
        default:
            deadline_ms = 0; return o;
        }
        offline();
        return o;
    }
};

inline const char *state_name(State s)
{
    switch (s) {
    case State::OFFLINE: return "OFFLINE"; case State::IDLE: return "IDLE"; case State::DIALING: return "DIALING";
    case State::RINGBACK: return "RINGBACK"; case State::INCOMING: return "INCOMING"; case State::ANSWERING: return "ANSWERING";
    case State::IN_CALL: return "IN_CALL"; case State::ENDING: return "ENDING";
    }
    return "?";
}

inline const char *notice_name(Notice n)
{
    switch (n) {
    case Notice::NONE: return "NONE"; case Notice::PEER_OFF: return "PEER_OFF"; case Notice::NO_ANSWER: return "NO_ANSWER";
    case Notice::REJECTED: return "REJECTED"; case Notice::LOST: return "LOST"; case Notice::CALL_ENDED: return "CALL_ENDED";
    case Notice::MISSED: return "MISSED"; case Notice::RELAY_SILENT: return "RELAY_SILENT"; case Notice::REJECT_MSG: return "REJECT_MSG";
    case Notice::BUSY: return "BUSY";
    }
    return "?";
}

}  // namespace callfsm
