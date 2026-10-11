// test_device_id.cpp — device_id.h 의 순수 로직 시험:
//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_device_id.cpp -o /tmp/t_id && /tmp/t_id
#include "device_id.h"
#include <cstdio>
#include <string>
using namespace deviceid;
static int ok = 0, bad = 0;
#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)

int main()
{
    const Entry tbl[] = { { "AA:BB:CC:DD:EE:01", 1, "tab5-1", "tok-one-0123456789" }, { "AA:BB:CC:DD:EE:02", 2, "tab5-2", "tok-two-0123456789" } };

    Info a = lookup("AA:BB:CC:DD:EE:01", tbl, 2);
    CHECK("unit 1 by mac", a.unit == 1 && std::string(a.id) == "tab5-1" && std::string(a.token) == "tok-one-0123456789");
    Info b = lookup("aa:bb:cc:dd:ee:02", tbl, 2);
    CHECK("unit 2, lowercase mac", b.unit == 2 && std::string(b.id) == "tab5-2");
    CHECK("mac kept as given", std::string(b.mac) == "aa:bb:cc:dd:ee:02");
    Info c = lookup("AA:BB:CC:DD:EE:FF", tbl, 2);
    CHECK("unknown device: unit 0, no token", c.unit == 0 && std::string(c.id) == "unknown" && std::string(c.token).empty());
    CHECK("prefix of a mac does not match", lookup("AA:BB:CC:DD:EE", tbl, 2).unit == 0);
    CHECK("null mac / null table / empty table", lookup(nullptr, tbl, 2).unit == 0 && lookup("AA:BB:CC:DD:EE:01", nullptr, 2).unit == 0 && lookup("AA:BB:CC:DD:EE:01", tbl, 0).unit == 0);
    const Entry broken[] = { { "AA:BB:CC:DD:EE:01", 3, "tab5-3", "x" }, { "AA:BB:CC:DD:EE:02", 2, nullptr, "x" } };
    CHECK("bad table rows are ignored (the id must be the roster's id for that unit, id required)", lookup("AA:BB:CC:DD:EE:01", broken, 2).unit == 0 && lookup("AA:BB:CC:DD:EE:02", broken, 2).unit == 0);

    const uint8_t m[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x02 };
    char s[18]; format_mac(m, s);
    CHECK("format mac: upper case with colons", std::string(s) == "AA:BB:CC:DD:EE:02");
    CHECK("formatted mac finds unit 2", lookup(s, tbl, 2).unit == 2);

    // ── 전화기 3대(설계서 2026-10-08-phone-3way-design.md TW-D4): 번호 1~3, 번호와 ID 는 roster.h 의 표와 맞아야 한다(DI-D1) ──
    const Entry three[] = { { "AA:BB:CC:DD:EE:01", 1, "tab5-1", "tok-one-0123456789" }, { "AA:BB:CC:DD:EE:02", 2, "tab5-2", "tok-two-0123456789" },
                            { "AA:BB:CC:DD:EE:03", 3, "stackchan-2", "tok-three-0123456789" } };
    Info t3 = lookup("AA:BB:CC:DD:EE:03", three, 3);
    CHECK("unit 3 (the stackchan) by mac", t3.unit == 3 && std::string(t3.id) == "stackchan-2" && std::string(t3.token) == "tok-three-0123456789");
    CHECK("unit 3, lowercase mac", lookup("aa:bb:cc:dd:ee:03", three, 3).unit == 3);
    CHECK("units 1 and 2 are found as before in a three-row table", lookup("AA:BB:CC:DD:EE:01", three, 3).unit == 1 && lookup("aa:bb:cc:dd:ee:02", three, 3).unit == 2
          && std::string(lookup("AA:BB:CC:DD:EE:02", three, 3).id) == "tab5-2");
    CHECK("a table shorter than its rows: the third row is not read", lookup("AA:BB:CC:DD:EE:03", three, 2).unit == 0);
    {
        const Entry off[] = { { "AA:00:00:00:00:01", 0, "tab5-1", "x" }, { "AA:00:00:00:00:02", 4, "stackchan-2", "x" }, { "AA:00:00:00:00:03", -1, "tab5-2", "x" },
                              { "AA:00:00:00:00:04", 99, "tab5-1", "x" } };
        bool none = true;
        for (const Entry &e : off) none = none && lookup(e.mac, off, 4).unit == 0 && std::string(lookup(e.mac, off, 4).id) == "unknown" && std::string(lookup(e.mac, off, 4).token).empty();
        CHECK("unit numbers outside 1..3 are ignored (0, 4, -1, 99)", none);
    }
    {
        // 번호는 맞는데 ID 가 표와 다르다 → 미등록으로 본다: 다른 기기들이 이 기기를 엉뚱한 자리(또는 모르는 기기)로 보게 되는 것을 부팅 때 드러낸다
        const Entry mixed[] = { { "AA:00:00:00:00:01", 2, "tab5-1", "x" }, { "AA:00:00:00:00:02", 3, "stackchan-1", "x" }, { "AA:00:00:00:00:03", 1, "", "x" },
                                { "AA:00:00:00:00:04", 3, "tab5-3", "x" }, { "AA:00:00:00:00:05", 1, "TAB5-1", "x" } };
        bool none = true;
        for (const Entry &e : mixed) none = none && lookup(e.mac, mixed, 5).unit == 0 && std::string(lookup(e.mac, mixed, 5).token).empty();
        CHECK("a row whose id is not the roster's id for its unit is ignored (swapped, misspelt, empty, other case)", none);
    }
    {
        const Entry later[] = { { "AA:BB:CC:DD:EE:03", 3, "stackchan-9", "bad" }, { "AA:BB:CC:DD:EE:03", 3, "stackchan-2", "good-token-0123456789" } };
        Info g = lookup("AA:BB:CC:DD:EE:03", later, 2);
        CHECK("a bad row does not hide a good row for the same mac", g.unit == 3 && std::string(g.token) == "good-token-0123456789");
    }

    printf("\n%d ok, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
