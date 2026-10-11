// device_id.h — 이 기기가 몇 번인지(MAC 으로 판별)와 교환기에 내밀 ID·토큰. 설계서 PH-D9: 모든 기기에 같은 펌웨어, secrets.h 에 MAC→토큰 표.
// 표에서 찾는 부분은 순수 로직(맥에서 단위 시험: tools/test_device_id.cpp), MAC 을 읽는 부분만 기기 전용.
// 전화기 3대(docs/exec-plans/2026-10-08-phone-3way-design.md §8·TW-D4): 번호는 1~3 이고, 번호 ↔ 교환기 ID 의 정본은 roster.h 다.
//   DI-D1 secrets.h 의 줄은 "번호와 ID 가 roster.h 의 표와 같을 때"만 받는다(번호 3 = "stackchan-2"). 다르면 그 줄은 없는 줄이다 → 그 기기는 미등록(unit 0)으로 뜬다.
//         이유: 번호 ↔ ID 가 두 곳(roster.h, secrets.h)에 있다(RS-D2). 어긋난 채 붙으면 다른 기기들이 이 기기를 엉뚱한 slot 으로 보거나 모르는 기기로 버리는데,
//               그 증상은 상대 화면에서만 보인다. 부팅 줄 "PHONE BOOT unit=0 id=unknown" 과 미등록 안내로 그 자리에서 드러나는 편이 낫다.
//         비용: secrets.h 의 ID 를 표와 다르게 적으면(오타 포함) 그 기기는 교환기에 붙지 않는다. 탈출구: lookup 의 unit_of_id 비교를 지우면 번호 범위만 본다.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "roster.h"

namespace deviceid {

struct Entry {
    const char *mac;      // "AA:BB:CC:DD:EE:01" (대소문자 무관)
    int unit;             // 1 ~ roster::MAX_UNITS(3)
    const char *id;       // 교환기 기기 ID — roster.h 의 그 번호의 ID 와 같아야 한다("tab5-1"·"tab5-2"·"stackchan-2", DI-D1)
    const char *token;    // 교환기 토큰 — 화면·로그에 내지 않는다
};

struct Info {
    int unit = 0;                 // 표에 없으면 0
    const char *id = "unknown";
    const char *token = "";
    char mac[18] = "";
};

inline void format_mac(const uint8_t m[6], char out[18])
{
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

inline bool same_mac(const char *a, const char *b)
{
    if (!a || !b) return false;
    for (;; ++a, ++b) {
        char x = *a, y = *b;
        if (x >= 'a' && x <= 'f') x = (char)(x - 32);
        if (y >= 'a' && y <= 'f') y = (char)(y - 32);
        if (x != y) return false;
        if (!x) return true;
    }
}

// MAC 글자열로 표에서 찾는다. 없으면 unit 0 / "unknown" / 빈 토큰. 번호·ID 가 roster.h 의 표와 다른 줄은 건너뛴다(DI-D1).
inline Info lookup(const char *mac, const Entry *table, int n)
{
    Info out;
    if (mac) { size_t i = 0; for (; i + 1 < sizeof out.mac && mac[i]; ++i) out.mac[i] = mac[i]; out.mac[i] = 0; }   // 길면 자른다
    for (int i = 0; table && i < n; ++i) {
        if (same_mac(mac, table[i].mac) && table[i].id && table[i].token && roster::valid_unit(table[i].unit) && roster::unit_of_id(table[i].id) == table[i].unit) {
            out.unit = table[i].unit; out.id = table[i].id; out.token = table[i].token;
            return out;
        }
    }
    return out;
}

#ifdef ESP_PLATFORM
// 이 기기의 정보. 처음 부를 때 칩의 MAC(eFuse — USB 일련번호·esptool read_mac 과 같은 값)을 읽어 secrets.h 의 PHONE_DEVICES 에서 찾는다.
const Info &get();
#endif

}  // namespace deviceid
