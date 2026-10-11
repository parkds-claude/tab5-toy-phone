// wifi_store.h — Wi-Fi 후보 목록 순수 로직 (D11·D12). ESP-IDF 비의존 → tools/test_wifi_store.cpp 로 맥에서 단위테스트.
//   - 저장소 = NVS 목록(화면에서 연결 성공한 것, 최근 사용 순, 최대 MAX_SAVED) + 코드 고정 목록(secrets.h WIFI_NETWORKS)
//   - 자동 연결 = 스캔에서 보이는 후보 중 신호(RSSI) 센 순서
#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace wifistore {

static const size_t MAX_SAVED = 8;

struct Cred { std::string ssid, pass; bool fixed = false; };
struct Ap   { std::string ssid; int rssi = -127; bool secured = true; };

// NVS 저장 형식: "ssid\0pass\0ssid\0pass\0..." (SSID·비번에 NUL 없음 보장: 802.11 SSID 는 NUL 가능하나 UI 입력은 불가)
inline std::string pack(const std::vector<Cred> &v)
{
    std::string out;
    for (auto &c : v) { out += c.ssid; out.push_back('\0'); out += c.pass; out.push_back('\0'); }
    return out;
}
inline std::vector<Cred> unpack(const std::string &blob)
{
    std::vector<Cred> v; size_t i = 0;
    while (i < blob.size()) {
        size_t a = blob.find('\0', i); if (a == std::string::npos) break;
        size_t b = blob.find('\0', a + 1); if (b == std::string::npos) break;
        Cred c; c.ssid = blob.substr(i, a - i); c.pass = blob.substr(a + 1, b - a - 1);
        if (!c.ssid.empty() && v.size() < MAX_SAVED) v.push_back(c);
        i = b + 1;
    }
    return v;
}

// 저장 목록 맨 앞에 넣기(같은 SSID 는 비번 갱신 후 맨 앞으로), 최대 MAX_SAVED
inline void upsert(std::vector<Cred> &saved, const std::string &ssid, const std::string &pass)
{
    saved.erase(std::remove_if(saved.begin(), saved.end(), [&](const Cred &c) { return c.ssid == ssid; }), saved.end());
    Cred c; c.ssid = ssid; c.pass = pass;
    saved.insert(saved.begin(), c);
    if (saved.size() > MAX_SAVED) saved.resize(MAX_SAVED);
}

// 알려진 후보 = 저장 목록(최근 순) + 코드 고정(저장 목록에 같은 SSID 가 있으면 저장 쪽 비번 우선)
inline std::vector<Cred> merge(const std::vector<Cred> &saved, const std::vector<Cred> &fixed)
{
    std::vector<Cred> out = saved;
    for (auto &f : fixed) {
        bool dup = false; for (auto &o : out) if (o.ssid == f.ssid) { dup = true; break; }
        if (!dup && !f.ssid.empty()) { Cred c = f; c.fixed = true; out.push_back(c); }
    }
    return out;
}

// 스캔 결과 정리: 빈 SSID 제거, 같은 SSID(여러 AP) 는 가장 센 신호 하나로, RSSI 내림차순
inline std::vector<Ap> dedupe(const std::vector<Ap> &raw)
{
    std::vector<Ap> out;
    for (auto &a : raw) {
        if (a.ssid.empty()) continue;
        auto it = std::find_if(out.begin(), out.end(), [&](const Ap &o) { return o.ssid == a.ssid; });
        if (it == out.end()) out.push_back(a); else if (a.rssi > it->rssi) *it = a;
    }
    std::stable_sort(out.begin(), out.end(), [](const Ap &x, const Ap &y) { return x.rssi > y.rssi; });
    return out;
}

// 보이는 알려진 후보의 known 인덱스를 신호 센 순서로 (D12)
inline std::vector<int> rank(const std::vector<Cred> &known, const std::vector<Ap> &aps)
{
    std::vector<std::pair<int, int>> hit;   // (rssi, idx)
    for (int i = 0; i < (int)known.size(); ++i) {
        int best = -1000;
        for (auto &a : aps) if (a.ssid == known[i].ssid) best = std::max(best, a.rssi);
        if (best > -1000) hit.push_back({best, i});
    }
    std::stable_sort(hit.begin(), hit.end(), [](const std::pair<int, int> &x, const std::pair<int, int> &y) { return x.first > y.first; });
    std::vector<int> out; for (auto &h : hit) out.push_back(h.second);
    return out;
}

inline int find(const std::vector<Cred> &known, const std::string &ssid)
{
    for (int i = 0; i < (int)known.size(); ++i) if (known[i].ssid == ssid) return i;
    return -1;
}

// 신호 막대 칸수 (1~4)
inline int bars(int rssi) { return rssi > -55 ? 4 : rssi > -65 ? 3 : rssi > -75 ? 2 : 1; }

}  // namespace wifistore
