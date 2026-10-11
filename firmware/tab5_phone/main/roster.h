// roster.h — 등록 기기 표: 번호(unit) ↔ 교환기 기기 ID, 그리고 "나를 뺀 나머지"를 slot 으로 세는 규칙. 순수 로직(맥에서 단위 시험: tools/test_roster.cpp).
// 설계서 docs/exec-plans/2026-10-08-phone-3way-design.md §8(공통 헤더)·TW-D4(tab5-1·tab5-2·stackchan-2 = 1·2·3번).
// 두 가지 기기(Tab5 = ESP32-P4, 스택짱 = ESP32-S3)의 펌웨어가 이 파일을 함께 쓴다(TW-D11) → ESP-IDF·M5 헤더를 끌어오지 않는다.
//   - 여기 있는 것: 번호와 기기 ID. ID 는 비밀이 아니다(교환기 기록·시리얼 줄에 그대로 나온다).
//   - 여기 없는 것: 토큰·MAC(secrets.h 의 PHONE_DEVICES — device_id.h 가 읽는다), 캐릭터(toy.h 의 who_of_unit — 화면 쪽 일이다).
//
// slot 이란: 통화 상태 기계(call_fsm.h)와 화면은 상대를 기기 번호가 아니라 "내 상대 가운데 몇 번째"(slot 0 … MAX_PEERS-1)로 센다.
//   교환기 글자에는 기기 ID 가 실리고(relay_proto.h), ID ↔ slot 을 잇는 것이 이 파일이다:
//       받을 때  slot = roster::route_slot(my_unit, relayproto::about_peer(sig), sig.peer_id);   // -1 이면 그 신호를 버린다
//       걸 때    relayproto::send_text_for(A_SEND_CALL, buf, cap, roster::id_of_slot(my_unit, out.peer), out.msg);
//
// RS-D1 slot 은 나를 뺀 나머지를 번호 오름차순으로 센다(내가 2번이면 slot 0 = 1번, slot 1 = 3번).
//       이유: 세 기기가 같은 규칙으로 줄을 세우고, Tab5 두 대는 서로의 slot 0 이라 2대 시절의 "상대"가 그 자리 그대로다(옛 화면·옛 시험이 slot 0 만 써도 맞는다).
//       비용: 같은 기기가 보는 쪽에 따라 다른 slot 이다(2번은 1번에게 slot 0 이지만 3번에게는 slot 1) → 로그의 slot 은 그 줄을 찍은 기기의 눈으로 읽어야 한다.
//       탈출구: peer_unit·slot_of_unit 두 함수만 바꾸면 순서가 바뀐다(시험이 "빠짐없이·겹침 없이"를 고정한다).
// RS-D2 표를 secrets.h 의 PHONE_DEVICES 와 따로, 이 헤더에 상수로 둔다.
//       이유: secrets.h 는 git 밖이라 맥 시험과 화면 미리보기가 읽을 수 없고, 상대의 ID 는 내 MAC 줄이 아니라 "등록된 전부"를 알아야 나온다.
//       비용: 같은 사실(번호 ↔ ID)이 두 곳에 있다 → device_id.h 의 lookup 이 "PHONE_DEVICES 의 번호·ID 가 이 표와 같은 줄"만 받아 어긋남을 부팅 때 드러낸다(DI-D1).
//       탈출구: 기기를 더하면 DEVICES 에 한 줄(번호는 1 부터 빈틈 없이), secrets.h·교환기 PHONE_TOKENS 에도 같은 ID 로 한 줄. call_fsm.h 의 접속 표가 8비트라 상대는 8까지다.
// RS-D3 내 번호가 0(미등록)이거나 범위 밖이면 상대가 없다: peer_count 0, peer_unit 0, slot -1, ID "". 죽지 않는다(미등록 기기는 교환기에 붙지도 않는다 — phone_main).
// RS-D4 받은 신호의 slot 은 route_slot 한 곳에서 정한다: 상대에 달린 신호(peer·ring)는 ID 로 찾고, 찾지 못하면(-1) 버린다 — ID 가 없는 v1 모양, 표에 없는 ID, 내 ID.
//       이유: v2 교환기는 그 두 신호에 항상 ID 를 싣는다(§6.3). ID 없는 신호를 "아마 slot 0"으로 받으면, 옛 교환기나 깨진 글자 하나로 엉뚱한 캐릭터가 뜨고 엉뚱한 상대가 켜진 것으로 보인다.
//       비용: 옛(v1) 교환기에 붙으면 상대가 계속 꺼진 것으로 보이고 벨도 울리지 않는다 → 교환기를 v2 로 올린 뒤에 이 펌웨어를 올린다.
//             버린 ring 은 교환기에서는 살아 있다(거절을 보내지 않는다): 건 쪽은 30초 동안 연결음을 듣다 "받지 않음"이 되고, 그동안 이 기기가 누구에게 걸면
//             교환기가 응답 없이 버려(§6.4-2) 5초 뒤 "교환기 응답 없음"으로 다시 붙는다. 표가 맞는 세 기기 사이에서는 일어나지 않는다(설정이 어긋났을 때의 모습이다).
//       탈출구: route_slot 에서 "ID 가 빈 글자면 0" 으로 바꾸면 v1 교환기와도 옛 짝끼리 통화된다(Tab5 두 대에서만 뜻이 맞다).
#pragma once
#include <cstddef>

namespace roster {

constexpr int    MAX_UNITS = 3;                 // 등록 기기 수 = 가장 큰 번호(번호는 1 부터 빈틈 없이)
constexpr int    MAX_PEERS = MAX_UNITS - 1;     // 한 기기가 보는 상대 수 = slot 의 개수
constexpr size_t ID_MAX    = 15;                // 기기 ID 의 최대 글자 수(relay_proto.h 의 Signal::peer_id 가 16칸 — 시험이 둘을 묶는다)

struct Device { int unit; const char *id; };
inline constexpr Device DEVICES[MAX_UNITS] = {      // inline: 여러 파일이 끌어 써도 표는 하나다
    { 1, "tab5-1" },                            // Tab5 1번
    { 2, "tab5-2" },                            // Tab5 2번
    { 3, "stackchan-2" },                       // 스택짱 2호기 (TW-D4)
};

namespace detail {
constexpr bool same(const char *a, const char *b) { for (; *a && *a == *b; ++a, ++b) {} return *a == *b; }
constexpr size_t len(const char *s) { size_t n = 0; while (s[n]) ++n; return n; }
// 표가 약속을 지키는가: 번호가 자리와 같고, ID 가 1~ID_MAX 글자의 소문자·숫자·'-' 이고(JSON 글자와 접속 헤더에 그대로 실린다), 서로 다르다
constexpr bool table_ok()
{
    for (int i = 0; i < MAX_UNITS; ++i) {
        const char *id = DEVICES[i].id;
        if (DEVICES[i].unit != i + 1 || len(id) < 1 || len(id) > ID_MAX) return false;
        for (const char *p = id; *p; ++p) if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-')) return false;
        for (int j = 0; j < i; ++j) if (same(id, DEVICES[j].id)) return false;
    }
    return true;
}
}  // namespace detail
static_assert(detail::table_ok(), "roster: unit numbers must be 1..MAX_UNITS in order, ids unique, 1..15 chars of [a-z0-9-]");
static_assert(MAX_PEERS >= 1 && MAX_PEERS <= 8, "roster: call_fsm.h keeps one bit per peer slot in 8 bits");

constexpr bool valid_unit(int unit) { return unit >= 1 && unit <= MAX_UNITS; }

// 번호 → 교환기 기기 ID. 모르는 번호면 빈 글자(nullptr 이 아니다 — 그대로 %s 에 넣거나 call_text 에 넘겨도 된다: 빈 ID 로는 글자가 만들어지지 않는다).
constexpr const char *id_of_unit(int unit) { return valid_unit(unit) ? DEVICES[unit - 1].id : ""; }

// 교환기 기기 ID → 번호. 모르는 ID·빈 글자·nullptr 이면 0. 글자가 통째로 같아야 한다(대소문자 구분).
constexpr int unit_of_id(const char *id)
{
    if (!id || !*id) return 0;
    for (int i = 0; i < MAX_UNITS; ++i) if (detail::same(id, DEVICES[i].id)) return DEVICES[i].unit;
    return 0;
}

// 내가 보는 상대 수. 미등록(0)·범위 밖이면 0 (RS-D3).
constexpr int peer_count(int my_unit) { return valid_unit(my_unit) ? MAX_PEERS : 0; }

// slot → 그 상대의 번호. 나를 뺀 나머지를 번호 오름차순으로(RS-D1). 상대가 없으면 0.
constexpr int peer_unit(int my_unit, int slot)
{
    if (!valid_unit(my_unit) || slot < 0 || slot >= MAX_PEERS) return 0;
    return slot + 1 < my_unit ? slot + 1 : slot + 2;             // 내 번호부터는 한 칸씩 밀린다
}

// 번호 → 내가 보는 slot. 나 자신·모르는 번호·내가 미등록이면 -1.
constexpr int slot_of_unit(int my_unit, int unit)
{
    if (!valid_unit(my_unit) || !valid_unit(unit) || unit == my_unit) return -1;
    return unit < my_unit ? unit - 1 : unit - 2;
}

// 교환기 기기 ID → 내가 보는 slot. 내 ID·모르는 ID·빈 글자·nullptr 이면 -1.
constexpr int slot_of_id(int my_unit, const char *id) { return slot_of_unit(my_unit, unit_of_id(id)); }

// slot → 그 상대의 교환기 기기 ID(걸 때 글자에 싣는다). 상대가 없으면 빈 글자.
constexpr const char *id_of_slot(int my_unit, int slot) { return id_of_unit(peer_unit(my_unit, slot)); }

// 받은 신호를 상태 기계에 넣을 때의 상대 slot (RS-D4). 돌려주는 값: 넣을 slot(0 … MAX_PEERS-1), 버려야 하면 -1.
//   about_peer = 그 신호가 "어느 상대의 일인가"에 달린 것인가(peer·ring — relay_proto.h 의 about_peer), id = 신호에 실려 온 기기 ID(Signal::peer_id, 없으면 빈 글자).
//   그 밖의 신호(ringing·connected·ended)는 ID 를 보지 않고 0 을 돌려준다 — 상태 기계는 그 신호들에서 slot 을 읽지 않는다(지금 통화의 상대는 call_peer 가 기억한다).
constexpr int route_slot(int my_unit, bool about_peer, const char *id) { return about_peer ? slot_of_id(my_unit, id) : 0; }

}  // namespace roster
