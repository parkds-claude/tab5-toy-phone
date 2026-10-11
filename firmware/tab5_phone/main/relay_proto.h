// relay_proto.h — 교환기와 주고받는 글자·음성 프레임의 모양(순수 로직, 맥에서 단위 시험: tools/test_relay_proto.cpp).
// 설계서 §9.4 신호 규약. 교환기 원본: server/phone_core.py.
//   - 받은 글자(JSON 한 겹)를 call_fsm 사건으로 바꾼다. 깨진·모르는·너무 긴 입력은 Kind::NONE 으로 버린다(죽지 않는다).
//   - JSON 라이브러리를 쓰지 않는다: 신호의 열쇠가 몇 개뿐이고(v1: t, online, why, n, msg 다섯 — v2 에서 id, from 이 더해져 일곱), 맥에서 그대로 시험하려는 것(RP-D1).
//     비용: 겹친 객체·배열은 읽지 못한다(규약에 없음). 탈출구: 규약이 커지면 cJSON 으로 교체, 시험은 그대로 쓴다.
// 신호 규약 v2 — 전화기 3대(docs/exec-plans/2026-10-08-phone-3way-design.md §6, 결정 TW-D7·D8·D9). 이 헤더는 Tab5 와 스택짱의 펌웨어가 함께 쓴다(TW-D11):
//   - RP-D2 기기 ID 를 읽는다: peer 의 "id", ring 의 "from" → Signal::peer_id(열쇠가 7개가 됐다 — RP-D1 의 판단은 그대로다). 다른 신호에 붙은 id·from 은 버린다.
//     ID 가 없는 v1 모양({"t":"peer","online":true}, {"t":"ring"})도 지금처럼 읽히고 peer_id 가 빈 글자다 — 그 신호를 받을지 버릴지는 호출자가 정한다
//     (이 헤더는 "내 번호"를 모른다. ID → 상대 slot 은 roster.h 의 route_slot, 호출은 app).
//     ID 의 모양은 id_ok 하나가 정한다(읽기·쓰기 공통): 1~15자, 눈에 보이는 ASCII, 따옴표·역슬래시 없음. 틀린 ID(길다·이스케이프·공백·숫자·true·null)는 "ID 없음"이 된다
//     — 잘라서 담지 않는다(잘린 ID 가 다른 기기의 ID 와 같아질 수 있다). 신호 자체는 살아 있다(기존 msg 의 규칙과 같다: 틀린 값 하나로 통화 진행을 버리지 않는다).
//     비용: 16자 이상의 ID 를 쓰는 기기는 없는 기기처럼 된다. 탈출구: ID_CAP 을 키운다(Signal 이 그만큼 커진다 — relay_link 의 사건 대기열 16칸에 곱해진다).
//   - RP-D3 why:"busy" → Why::BUSY(TW-D9). 그 밖의 모르는 사유는 지금처럼 HANGUP.
//   - RP-D4 걸기 글자는 대상을 싣는다: call_text → {"t":"call","to":"<id>"}. ID 로 글자를 만들 수 없으면 0 을 돌려주고 아무 글자도 남기지 않는다.
//     send_text_for 는 그때 nullptr 을 돌려준다 — 옛 모양 {"t":"call"}(text_for)로 떨어지지 않는다: 그 글자는 "옛 짝에게 건다"(§6.2)라 엉뚱한 기기의 벨이 울린다.
//     비용: 대상 ID 를 못 구하면(호출자의 버그) call 이 나가지 않아 상태 기계가 5초 뒤 다시 붙는다(FSM-D1). 탈출구: send_text_for 의 걸기 갈래를 text_for 로 되돌린다.
//     text_for(A_SEND_CALL) 은 그대로 둔다(v1 모양 — 옛 시험과, v1 교환기로 되돌릴 때의 길).
//   - RP-D5 접속 헤더도 여기서 만든다(connect_headers): X-Device-Id · X-Device-Token · X-Phone-Proto: 2 (§6.1 — 교환기는 이 헤더가 "2" 일 때만 v2 로 대한다, TW-D7).
//     이유: 두 기기가 같은 글자를 내야 하고, 줄 끝(\r\n)·버전 값을 맥 시험이 고정한다. 값의 글자는 검사하지 않는다(ID·토큰은 secrets.h 에서 온다 — 길이만 본다).
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "call_fsm.h"

namespace relayproto {

enum class Kind : uint8_t { NONE, EVENT, PONG };

const int    PROTO_VERSION = 2;         // 신호 규약 판(§6). 접속 헤더 X-Phone-Proto 에 싣는다(RP-D5)
const size_t ID_CAP = 16;               // 기기 ID 를 담는 칸(끝의 0 포함) = ID 는 15자까지(RP-D2). roster.h 의 ID_MAX + 1 — 시험(test_roster.cpp)이 묶는다
const size_t SEND_BUF = 48;             // 보낼 신호 글자를 만드는 버퍼의 넉넉한 크기: 가장 긴 것이 걸기 35자({"t":"call","to":"<15자>"}), 거절은 22자

struct Signal {
    Kind kind = Kind::NONE;
    callfsm::Ev ev = callfsm::Ev::TICK;
    callfsm::Why why = callfsm::Why::NONE;
    bool flag = false;          // peer 의 online
    uint32_t n = 0;             // pong 의 n
    uint8_t msg = 0;            // ended(why=reject) 의 거절 문구 번호(1~callfsm::MSG_COUNT). 없거나 범위 밖이면 0
    char peer_id[ID_CAP] = "";  // (v2, RP-D2) peer 의 "id" · ring 의 "from": 그 상대의 교환기 기기 ID. 없거나 모양이 틀리면 빈 글자, 비어 있지 않으면 늘 id_ok 다
};

const size_t MAX_TEXT = 256;            // 교환기 신호는 수십 바이트 — 이보다 길면 버린다(교환기 MAX_TEXT_BYTES 와 같은 값)
const size_t MAX_VOICE_FRAME = 400;     // [순번 2][Opus] 전체 길이 상한(§9.4)
const size_t MIN_VOICE_FRAME = 3;

// 기기 ID 로 쓸 수 있는 글자인가(RP-D2 — 읽기·쓰기가 함께 쓰는 규칙): 1~15자, 눈에 보이는 ASCII(0x21~0x7E)만, 따옴표·역슬래시 없음.
// → JSON 글자열에 이스케이프 없이 그대로 실리고, 기록 줄에 찍어도 줄이 깨지지 않는다. 공백·제어 문자·DEL·한글 등은 안 된다.
inline bool id_ok(const char *id)
{
    if (!id || !*id) return false;
    size_t n = 0;
    for (; id[n]; ++n) {
        const unsigned char c = (unsigned char)id[n];
        if (n + 1 >= ID_CAP || c <= 0x20 || c >= 0x7F || c == '"' || c == '\\') return false;
    }
    return true;
}

namespace detail {

struct Scan {
    const char *p, *end;
    void ws() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p; }
    // 따옴표로 시작하는 글자열을 읽어 out(최대 cap-1 글자)에 담는다. 이스케이프가 있으면 plain=false (비교 대상에서 제외).
    bool str(char *out, size_t cap, bool *plain)
    {
        if (p >= end || *p != '"') return false;
        ++p; size_t n = 0; *plain = true;
        while (p < end && *p != '"') {
            unsigned char c = (unsigned char)*p;
            if (c < 0x20) return false;                          // 제어 문자(NUL 포함)는 JSON 글자열에 날것으로 올 수 없다
            if (c == '\\') { *plain = false; ++p; if (p >= end) return false; }
            if (n + 1 < cap) out[n++] = *p; else *plain = false; // 너무 긴 값은 어차피 아는 값이 아니다
            ++p;
        }
        if (p >= end) return false;                              // 닫는 따옴표 없음
        ++p; out[n] = 0;
        return true;
    }
};

inline bool eq(const char *a, const char *b) { return strcmp(a, b) == 0; }

}  // namespace detail

inline Signal parse(const char *text, size_t len)
{
    using namespace detail;
    Signal none;
    if (!text || len < 2 || len > MAX_TEXT) return none;
    Scan s{ text, text + len };

    char t[ID_CAP] = "", why[ID_CAP] = "";                      // 값을 담는 칸은 모두 ID_CAP 이다(아래 val 에서 통째로 옮긴다 — 칸 크기가 어긋나면 넘친다)
    char id[ID_CAP] = "", from[ID_CAP] = "";                     // (v2) peer 의 "id", ring 의 "from". 모양이 틀리면 빈 글자로 둔다(RP-D2)
    bool have_t = false, have_why = false, have_online = false, online = false, have_n = false;
    uint32_t n = 0;
    uint8_t msg = 0;

    s.ws();
    if (s.p >= s.end || *s.p != '{') return none;
    ++s.p; s.ws();
    if (s.p < s.end && *s.p == '}') return none;
    while (true) {
        char key[16]; bool key_plain = true;
        s.ws();
        if (!s.str(key, sizeof key, &key_plain)) return none;
        s.ws();
        if (s.p >= s.end || *s.p != ':') return none;
        ++s.p; s.ws();
        if (s.p >= s.end) return none;

        // (v2) id·from 은 열쇠가 나올 때마다 새로 정한다: 글자열이 아니거나 모양이 틀린 값이면 "ID 없음" — 같은 열쇠가 두 번 오면 마지막 것이 이긴다(msg 와 같은 규칙)
        char *const id_slot = !key_plain ? nullptr : eq(key, "id") ? id : eq(key, "from") ? from : nullptr;
        if (id_slot) id_slot[0] = 0;

        if (*s.p == '"') {                                       // 글자열 값
            char val[ID_CAP]; bool plain = true;                 // 15자를 넘는 값은 잘려 담기고 plain 이 꺼진다 → 아는 값도, ID 도 아니다
            if (!s.str(val, sizeof val, &plain)) return none;
            if (key_plain && plain && eq(key, "t"))   { strcpy(t, val); have_t = true; }
            if (key_plain && plain && eq(key, "why")) { strcpy(why, val); have_why = true; }
            if (id_slot && plain && id_ok(val)) strcpy(id_slot, val);
        } else if (s.end - s.p >= 4 && !memcmp(s.p, "true", 4)) {
            s.p += 4; if (key_plain && eq(key, "online")) { have_online = true; online = true; }
        } else if (s.end - s.p >= 5 && !memcmp(s.p, "false", 5)) {
            s.p += 5; if (key_plain && eq(key, "online")) { have_online = true; online = false; }
        } else if (s.end - s.p >= 4 && !memcmp(s.p, "null", 4)) {
            s.p += 4;
        } else if (*s.p == '-' || (*s.p >= '0' && *s.p <= '9')) { // 숫자 값
            const char *b = s.p; bool digits_only = true; uint64_t v = 0;
            while (s.p < s.end && (*s.p == '-' || *s.p == '+' || *s.p == '.' || *s.p == 'e' || *s.p == 'E' || (*s.p >= '0' && *s.p <= '9'))) {
                if (*s.p < '0' || *s.p > '9') digits_only = false; else if (v <= 0xFFFFFFFFull) v = v * 10 + (uint64_t)(*s.p - '0');
                ++s.p;
            }
            if (s.p == b) return none;
            if (key_plain && eq(key, "n") && digits_only && v <= 0xFFFFFFFFull) { have_n = true; n = (uint32_t)v; }
            if (key_plain && eq(key, "msg")) msg = (digits_only && callfsm::msg_valid(v)) ? (uint8_t)v : 0;   // 틀린 번호는 0(번호 없는 거절). v 를 64비트 그대로 비교한다
        } else {
            return none;                                         // 겹친 객체·배열 등: 규약에 없다
        }

        s.ws();
        if (s.p >= s.end) return none;
        if (*s.p == ',') { ++s.p; continue; }
        if (*s.p == '}') { ++s.p; break; }
        return none;
    }
    s.ws();
    if (s.p != s.end || !have_t) return none;

    Signal out;
    if (eq(t, "peer")) {
        if (!have_online) return none;
        out.kind = Kind::EVENT; out.ev = callfsm::Ev::PEER; out.flag = online;
        strcpy(out.peer_id, id);                                  // 누구의 상태인가(v1 모양이면 빈 글자)
    } else if (eq(t, "ringing")) {
        out.kind = Kind::EVENT; out.ev = callfsm::Ev::RINGING;
    } else if (eq(t, "ring")) {
        out.kind = Kind::EVENT; out.ev = callfsm::Ev::RING;
        strcpy(out.peer_id, from);                                // 누가 걸었나(v1 모양이면 빈 글자)
    } else if (eq(t, "connected")) {
        out.kind = Kind::EVENT; out.ev = callfsm::Ev::CONNECTED;
    } else if (eq(t, "ended")) {                                 // 사유를 몰라도 "끝났다"는 사실은 전한다
        out.kind = Kind::EVENT; out.ev = callfsm::Ev::ENDED;
        out.why = !have_why            ? callfsm::Why::HANGUP
                : eq(why, "reject")    ? callfsm::Why::REJECT
                : eq(why, "timeout")   ? callfsm::Why::TIMEOUT
                : eq(why, "offline")   ? callfsm::Why::OFFLINE
                : eq(why, "lost")      ? callfsm::Why::LOST
                : eq(why, "busy")      ? callfsm::Why::BUSY      // RP-D3
                :                        callfsm::Why::HANGUP;
        if (out.why == callfsm::Why::REJECT) out.msg = msg;       // 문구 번호는 거절에만 붙는다(다른 종료에 붙어 온 번호는 버린다)
    } else if (eq(t, "pong")) {
        if (!have_n) return none;
        out.kind = Kind::PONG; out.n = n;
    }
    return out;
}

// 이 신호는 "어느 상대의 일인가"에 달린 것인가: peer(그 상대의 접속 상태)와 ring(그 상대가 걸었다). 호출자는 이런 신호의 peer_id 를 상대 slot 으로 바꿔
// 상태 기계에 넣고, 바꿀 수 없으면 버린다(roster.h route_slot — RS-D4). ID 가 없는 v1 모양도 "상대에 달린 신호"다(그래서 버려진다).
inline bool about_peer(const Signal &s) { return s.kind == Kind::EVENT && (s.ev == callfsm::Ev::PEER || s.ev == callfsm::Ev::RING); }

// call_fsm 의 "보내기" 할 일 하나 → 교환기에 보낼 고정 글자(v1 모양 — 걸기에 대상이 없다). 보내기가 아니면 nullptr. 기기는 send_text_for 를 쓴다(RP-D4).
inline const char *text_for(uint16_t act)
{
    switch (act) {
    case callfsm::A_SEND_CALL:   return "{\"t\":\"call\"}";
    case callfsm::A_SEND_ACCEPT: return "{\"t\":\"accept\"}";
    case callfsm::A_SEND_REJECT: return "{\"t\":\"reject\"}";
    case callfsm::A_SEND_HANGUP: return "{\"t\":\"hangup\"}";
    default:                     return nullptr;
    }
}

// 거절 글자를 buf 에 쓴다. 번호가 1~MSG_COUNT 면 {"t":"reject","msg":N}, 아니면 번호 없는 {"t":"reject"}(text_for 와 같은 글자).
// 돌려주는 값 = 글자 수(끝의 0 제외), 안 들어가면 0.
inline int reject_text(char *buf, size_t cap, uint8_t msg)
{
    int len = callfsm::msg_valid(msg) ? snprintf(buf, cap, "{\"t\":\"reject\",\"msg\":%u}", (unsigned)msg)
                                      : snprintf(buf, cap, "{\"t\":\"reject\"}");
    return (len > 0 && (size_t)len < cap) ? len : 0;
}

// (v2, RP-D4) 걸기 글자를 buf 에 쓴다: {"t":"call","to":"<id>"}. 돌려주는 값 = 글자 수(끝의 0 제외).
// id 가 id_ok 가 아니거나(빈 글자·nullptr·16자 이상·따옴표·역슬래시·제어 문자·공백·비 ASCII) 안 들어가면 0 — 그때 buf 에는 아무 글자도 남기지 않는다(빈 글자).
inline int call_text(char *buf, size_t cap, const char *to_id)
{
    if (!buf || cap == 0) return 0;
    buf[0] = 0;
    if (!id_ok(to_id)) return 0;
    const int len = snprintf(buf, cap, "{\"t\":\"call\",\"to\":\"%s\"}", to_id);
    if (len > 0 && (size_t)len < cap) return len;
    buf[0] = 0;
    return 0;
}

// (v2, RP-D4) call_fsm 의 "보내기" 할 일 하나 → 교환기에 보낼 글자. 걸기는 대상의 기기 ID(to_id — roster::id_of_slot(my_unit, Out.peer))를, 거절은 문구 번호(msg)를 싣는다.
// buf 는 SEND_BUF 바이트면 넉넉하다. 돌려주는 값 = 보낼 글자(buf 또는 고정 글자). nullptr 이면 보내지 않는다: 보내기 할 일 하나가 아니거나, 걸기인데 ID 로 글자를 만들 수 없다.
inline const char *send_text_for(uint16_t act, char *buf, size_t cap, const char *to_id, uint8_t msg)
{
    switch (act) {
    case callfsm::A_SEND_CALL:   return call_text(buf, cap, to_id) ? buf : nullptr;          // 옛 모양으로 떨어지지 않는다(RP-D4)
    case callfsm::A_SEND_REJECT: return (buf && reject_text(buf, cap, msg)) ? buf : text_for(act);   // 버퍼가 없거나 작으면 번호 없는 거절 — 거절은 반드시 나간다
    default:                     return text_for(act);
    }
}

// (v2, RP-D5) 접속 요청에 더 실을 헤더를 buf 에 쓴다(§6.1). 줄마다 \r\n 이 있어야 한다(esp_websocket_client 의 headers 칸이 그대로 붙여 보낸다).
// 돌려주는 값 = 글자 수(끝의 0 제외). dev_id·token 이 nullptr 이거나 안 들어가면 0 — 그때 buf 는 빈 글자다(토큰이 반쯤 든 버퍼를 남기지 않는다).
// 값의 글자는 거르지 않는다(줄바꿈이 든 값이면 헤더가 깨진다) — 믿는 입력만 넣는다: secrets.h 의 기기 ID 와 토큰.
inline int connect_headers(char *buf, size_t cap, const char *dev_id, const char *token)
{
    if (!buf || cap == 0) return 0;
    buf[0] = 0;
    if (!dev_id || !token) return 0;
    const int len = snprintf(buf, cap, "X-Device-Id: %s\r\nX-Device-Token: %s\r\nX-Phone-Proto: %d\r\n", dev_id, token, PROTO_VERSION);
    if (len > 0 && (size_t)len < cap) return len;
    memset(buf, 0, cap);
    return 0;
}

// 생존 확인 글자를 buf 에 쓴다. 돌려주는 값 = 글자 수(끝의 0 제외), 안 들어가면 0.
inline int ping_text(char *buf, size_t cap, uint32_t n)
{
    int len = snprintf(buf, cap, "{\"t\":\"ping\",\"n\":%u}", (unsigned)n);
    return (len > 0 && (size_t)len < cap) ? len : 0;
}

// 음성 프레임 만들기: out = [순번 큰 자리, 작은 자리][Opus…]. 돌려주는 값 = 전체 길이, 규격 밖이면 0.
// opus 가 out+2 를 가리켜도 된다(제자리 포장).
inline size_t pack_voice(uint8_t *out, size_t cap, uint16_t seq, const uint8_t *opus, size_t opus_len)
{
    if (!out || !opus || opus_len == 0 || opus_len + 2 > MAX_VOICE_FRAME || cap < opus_len + 2) return 0;
    memmove(out + 2, opus, opus_len);
    out[0] = (uint8_t)(seq >> 8); out[1] = (uint8_t)(seq & 0xFF);
    return opus_len + 2;
}

inline bool unpack_voice(const uint8_t *frame, size_t len, uint16_t *seq, const uint8_t **opus, size_t *opus_len)
{
    if (!frame || len < MIN_VOICE_FRAME || len > MAX_VOICE_FRAME) return false;
    *seq = (uint16_t)((frame[0] << 8) | frame[1]);
    *opus = frame + 2; *opus_len = len - 2;
    return true;
}

// 재연결까지 기다릴 시간(ms). attempt = 연속 실패 횟수(0 부터). 토큰 거절(401)이면 1분(§9.7).
inline int32_t backoff_ms(int attempt, bool auth_failed)
{
    if (auth_failed) return 60000;
    if (attempt < 0) attempt = 0;
    if (attempt >= 5) return 30000;
    return 1000 << attempt;
}

}  // namespace relayproto
