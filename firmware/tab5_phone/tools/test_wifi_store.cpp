// test_wifi_store.cpp — wifi_store.h 단위테스트:  clang++ -std=c++17 -I../main test_wifi_store.cpp -o /tmp/t && /tmp/t
#include "wifi_store.h"
#include <cstdio>
using namespace wifistore;
static int ok = 0, bad = 0;
#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)
static Cred C(const char *s, const char *p) { Cred c; c.ssid = s; c.pass = p; return c; }
static Ap A(const char *s, int r) { Ap a; a.ssid = s; a.rssi = r; return a; }

int main()
{
    // pack/unpack 왕복 (한글 SSID, 기호 비번)
    std::vector<Cred> v = { C("집와이파이", "p@ss w0rd!"), C("office", "12345678") };
    auto u = unpack(pack(v));
    CHECK("roundtrip size", u.size() == 2);
    CHECK("roundtrip korean ssid", u[0].ssid == "집와이파이" && u[0].pass == "p@ss w0rd!");
    CHECK("unpack truncated blob ignored", unpack(std::string("abc\0de", 6)).empty());
    CHECK("unpack empty", unpack("").empty());

    // upsert: 맨 앞, 중복 갱신, 상한
    std::vector<Cred> s;
    upsert(s, "a", "1"); upsert(s, "b", "2"); upsert(s, "a", "9");
    CHECK("upsert moves to front", s.size() == 2 && s[0].ssid == "a" && s[0].pass == "9" && s[1].ssid == "b");
    for (int i = 0; i < 12; ++i) upsert(s, ("n" + std::to_string(i)).c_str(), "x");
    CHECK("upsert cap MAX_SAVED", s.size() == MAX_SAVED && s[0].ssid == "n11");

    // merge: 저장 우선, 고정은 뒤에 fixed=true, 중복 제거
    auto m = merge({ C("home", "h"), C("office", "NEW") }, { C("office", "OLD"), C("hotspot", "hs") });
    CHECK("merge size", m.size() == 3);
    CHECK("merge saved pass wins", m[1].ssid == "office" && m[1].pass == "NEW" && !m[1].fixed);
    CHECK("merge fixed appended", m[2].ssid == "hotspot" && m[2].fixed);

    // dedupe: 빈 SSID 제거, 같은 SSID 는 최대 RSSI, 내림차순
    auto d = dedupe({ A("x", -80), A("", -30), A("y", -50), A("x", -60) });
    CHECK("dedupe size", d.size() == 2);
    CHECK("dedupe order+max", d[0].ssid == "y" && d[1].ssid == "x" && d[1].rssi == -60);

    // rank: 보이는 알려진 후보만, 신호 센 순
    auto r = rank(m, { A("hotspot", -40), A("cafe", -30), A("home", -70) });
    CHECK("rank visible only", r.size() == 2);
    CHECK("rank by rssi", m[r[0]].ssid == "hotspot" && m[r[1]].ssid == "home");
    CHECK("rank none visible", rank(m, { A("cafe", -30) }).empty());

    CHECK("bars", bars(-40) == 4 && bars(-60) == 3 && bars(-70) == 2 && bars(-90) == 1);
    CHECK("find", find(m, "home") == 0 && find(m, "zzz") == -1);
    printf("\n%d passed, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
