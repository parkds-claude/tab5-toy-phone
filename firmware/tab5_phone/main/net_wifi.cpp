// net_wifi.cpp — Wi-Fi 매니저 구현.
// 출처: firmware/tab5_snapbooth/main/main.cpp 의 Wi-Fi 매니저(D11·D12·D13, 2호기에서 실기 검증).
//   그대로 둔 동작: 저장 목록(NVS "snapwifi"/"list") + secrets.h 고정 목록 병합, 스캔에서 보이는 것 중 신호 센 순으로 연결,
//                  부팅 직후 첫 스캔 0개면 1.5초 뒤 다시(최대 2번), 같은 후보에서 3번 끊기면 다음 후보, 후보가 떨어지면 3초 뒤 재스캔,
//                  대상이 없을 때는 재연결하지 않음(s_have_target), 아는 망이 안 보이면 20초마다 재스캔(설정 화면이 열려 있으면 멈춤),
//                  수동 연결 15초 제한·인증 실패 2번이면 "비밀번호 틀림"·성공한 것만 저장, 비밀번호는 로그에 남기지 않음.
//   걷어낸 것: 스냅부스 서버 관련(fetch_styles, 서버 주소 전환), 화면 호출(ui_invalidate, s_state 참조), nvs_flash_init(main 이 부른다),
//             시리얼 전용 요청(wauto).
//   바꾼 것: 화면에 알리는 방법 = change_seq() 증가(화면이 읽어 간다). 화면과 나누는 값은 s_lock 또는 원자 변수로 보호.
//
// 결정
//   N-D1 초기화 실패는 재부팅(ESP_ERROR_CHECK) 대신 Phase::INIT_FAILED 로 알린다.
//        이유: 화면이 "Wi-Fi 칩을 켜지 못했어요. 전원을 다시 넣어 주세요" 를 보여 줄 수 있어야 한다(설계서 §9.7).
//        비용: 스스로 회복하지 않는다. 탈출구: wifi_task 의 실패 지점에서 esp_restart() 를 부르면 원본처럼 된다.
//   N-D2 "연결" 요청을 받는 즉시 Manual::CONNECTING 으로 바꾼다(원본은 매니저 태스크가 요청을 꺼낼 때 바꿨다).
//        이유: 매니저가 스캔 중이면 2~3초 동안 화면이 반응 없는 것처럼 보이고, 그 사이 "연결"이 여러 번 눌릴 수 있다.
//   N-D3 신호 세기는 매니저 태스크가 10초마다 읽어 둔다. rssi() 는 그 값을 돌려줄 뿐이다.
//        이유: esp_wifi_sta_get_ap_info 는 C6 까지 다녀오는 호출이라 화면 태스크에서 직접 부르면 화면이 멈출 수 있다.
//        change_seq 는 신호 막대 칸수(wifistore::bars)가 바뀔 때만 올린다(1dBm 흔들릴 때마다 화면을 다시 그리지 않게).
#include "net_wifi.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>

#include "esp_event.h"
#include "esp_hosted.h"
extern "C" {
#include "esp_hosted_transport_config.h"   // C 헤더에 extern "C" 가드가 없다 → C++ 이름 맹글링 방지
}
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

#include "secrets.h"   // WIFI_NETWORKS (git 제외). 주의: WIFI_SSID·WIFI_PASS 는 매크로 이름이라 변수·열거 이름으로 쓰면 빌드가 깨진다

#ifndef WIFI_NETWORKS
#error "secrets.h 에 WIFI_NETWORKS 가 없습니다. 예: #define WIFI_NETWORKS { {\"ssid\", \"pass\"}, {\"ssid2\", \"pass2\"} }"
#endif

namespace {

const char *TAG = "netwifi";

constexpr int MANUAL_LIMIT_MS = 15000;      // 수동 연결 제한
constexpr int RESCAN_MS       = 20000;      // 아는 망이 안 보일 때 재스캔 간격
constexpr int RSSI_PERIOD_MS  = 10000;      // N-D3

struct FixedCred { const char *ssid; const char *pass; };
const FixedCred FIXED[] = WIFI_NETWORKS;    // secrets.h 고정 후보 (D11)
const int FIXED_N = sizeof(FIXED) / sizeof(FIXED[0]);

enum class Req : uint8_t { NEXT, SCAN, CONNECT, FORGET };
struct Msg { Req type; bool use_saved; char ssid[33]; char pass[65]; };

QueueHandle_t      s_q = nullptr;           // → 매니저 태스크
EventGroupHandle_t s_eg = nullptr;          // 연결 결과
SemaphoreHandle_t  s_lock = nullptr;        // 아래 "s_lock 보호" 값들
const EventBits_t  B_OK = BIT0, B_FAIL = BIT1;

std::atomic<bool> s_started{false};         // esp_wifi_start 까지 끝남
std::atomic<bool> s_init_failed{false};
std::atomic<bool> s_ok{false};              // IP 를 받은 상태
std::atomic<bool> s_none{false};            // 아는 망이 하나도 안 보임
std::atomic<bool> s_suppress{false};        // 수동 스캔·연결 중: 이벤트 핸들러의 자동 재시도를 막는다(원본 s_manual)
std::atomic<bool> s_have_target{false};     // 매니저가 연결 대상을 정했을 때만 끊김 뒤 재시도(없으면 이전 설정으로 헛도는 것을 막는다)
std::atomic<bool> s_scanning{false};
std::atomic<bool> s_setup_open{false};
std::atomic<int>  s_scan_pending{0};        // 요청됐지만 아직 끝나지 않은 스캔 수
std::atomic<int>  s_disc_reason{0};
std::atomic<int>  s_auto_fails{0};
std::atomic<int>  s_rssi{-127};
std::atomic<int>  s_ps{-1};                  // N-D5
std::atomic<uint32_t> s_seq{0};

// ── s_lock 보호 (화면 태스크가 읽는다) ──
std::vector<wifistore::Ap> s_scan;          // 마지막 스캔(신호 센 순)
std::vector<std::string>   s_known_ssids;   // 아는 망 이름
char s_ssid[33] = "";                       // 연결된 망
char s_connecting[33] = "";                 // 지금 붙으려는 망
netwifi::Manual s_manual = netwifi::Manual::IDLE;
int64_t s_manual_ms = 0;

// ── 매니저 태스크 전용 ──
std::vector<wifistore::Cred> s_saved, s_known;
std::vector<int> s_rank;
int s_rank_pos = 0;

struct Lock {
    Lock()  { if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY); }
    ~Lock() { if (s_lock) xSemaphoreGive(s_lock); }
};

int64_t now_ms() { return esp_timer_get_time() / 1000; }
void bump() { s_seq.fetch_add(1); }

void set_manual(netwifi::Manual m)
{
    { Lock l; s_manual = m; s_manual_ms = now_ms(); }
    bump();
}

bool post(Req t, const char *ssid = nullptr, const char *pass = nullptr, bool use_saved = false)
{
    if (!s_q) return false;
    Msg m = {};
    m.type = t; m.use_saved = use_saved;
    if (ssid) strlcpy(m.ssid, ssid, sizeof m.ssid);
    if (pass) strlcpy(m.pass, pass, sizeof m.pass);
    if (xQueueSend(s_q, &m, 0) != pdTRUE) { ESP_LOGW(TAG, "request queue full (type %d)", (int)t); return false; }
    return true;
}

// ---------------------------------------------------------------------------
// 이벤트 (기본 이벤트 루프 태스크에서 불린다)
// ---------------------------------------------------------------------------
void on_event(void *, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        auto *e = (wifi_event_sta_disconnected_t *)data;
        s_disc_reason = e ? e->reason : 0;
        const bool was_ok = s_ok.exchange(false);
        if (was_ok) { s_rssi = -127; bump(); }
        xEventGroupSetBits(s_eg, B_FAIL);
        if (s_suppress || !s_have_target) return;                 // 수동 스캔·연결은 매니저가 다루고, 대상이 없으면 재시도하지 않는다
        ESP_LOGW(TAG, "wifi disconnected (reason %d)", s_disc_reason.load());
        if (was_ok) s_auto_fails = 0;
        if (++s_auto_fails >= 3) { s_auto_fails = 0; post(Req::NEXT); }   // 같은 후보 3번 → 다음으로 센 후보
        else esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto *e = (ip_event_got_ip_t *)data;
        char name[33];
        { Lock l; strlcpy(s_ssid, s_connecting, sizeof s_ssid); strlcpy(name, s_ssid, sizeof name); }
        s_ok = true; s_auto_fails = 0; s_none = false;
        bump();
        xEventGroupSetBits(s_eg, B_OK);
        ESP_LOGI(TAG, "wifi got ip " IPSTR " (%s)", IP2STR(&e->ip_info.ip), name);
    }
}

// ---------------------------------------------------------------------------
// 저장 목록 (NVS — 스냅부스와 같은 네임스페이스·키·형식)
// ---------------------------------------------------------------------------
void nvs_load_saved()
{
    s_saved.clear();
    nvs_handle_t h;
    if (nvs_open("snapwifi", NVS_READONLY, &h) != ESP_OK) return;   // 저장한 적이 없으면 네임스페이스가 없다
    size_t len = 0;
    if (nvs_get_blob(h, "list", nullptr, &len) == ESP_OK && len > 0 && len < 4096) {
        std::string blob(len, '\0');
        if (nvs_get_blob(h, "list", &blob[0], &len) == ESP_OK) s_saved = wifistore::unpack(blob);
    }
    nvs_close(h);
}

void nvs_store_saved()
{
    nvs_handle_t h;
    if (nvs_open("snapwifi", NVS_READWRITE, &h) != ESP_OK) { ESP_LOGE(TAG, "nvs open failed — Wi-Fi 목록을 저장하지 못함"); return; }
    const std::string blob = wifistore::pack(s_saved);
    esp_err_t e = blob.empty() ? nvs_erase_key(h, "list") : nvs_set_blob(h, "list", blob.data(), blob.size());
    if (e == ESP_ERR_NVS_NOT_FOUND) e = ESP_OK;                       // 지울 것이 없었음
    if (e == ESP_OK) e = nvs_commit(h);
    if (e != ESP_OK) ESP_LOGE(TAG, "nvs write failed: %s", esp_err_to_name(e));
    nvs_close(h);
}

void rebuild_known()
{
    std::vector<wifistore::Cred> fixed;
    for (int i = 0; i < FIXED_N; ++i) { wifistore::Cred c; c.ssid = FIXED[i].ssid; c.pass = FIXED[i].pass; fixed.push_back(c); }
    s_known = wifistore::merge(s_saved, fixed);
    std::vector<wifistore::Ap> scan;
    std::string target;
    {
        Lock l;
        s_known_ssids.clear();
        for (auto &c : s_known) s_known_ssids.push_back(c.ssid);
        scan = s_scan;
        target = s_connecting;
    }
    // s_rank 는 s_known 의 번호표다 → 목록이 바뀌면 다시 매긴다.
    // (원본은 다시 매기지 않아, 저장 목록을 지운 뒤 "다음 후보"로 넘어가면 줄어든 목록 밖을 읽을 수 있었다)
    // s_rank_pos = "지금 붙어 있거나 붙으려는 망"의 순위. 그 망이 순위에 없으면 -1 → 다음 후보는 가장 센 것부터.
    s_rank = wifistore::rank(s_known, scan);
    s_rank_pos = -1;
    const int cur = wifistore::find(s_known, target);
    for (size_t i = 0; i < s_rank.size(); ++i) if (s_rank[i] == cur) { s_rank_pos = (int)i; break; }
    bump();
    ESP_LOGI(TAG, "wifi known: %u (saved %u + fixed %d)", (unsigned)s_known.size(), (unsigned)s_saved.size(), FIXED_N);
}

// ---------------------------------------------------------------------------
// 스캔·연결 (매니저 태스크)
// ---------------------------------------------------------------------------
// 스캔(연결 시도 중이면 시도를 멈추고 스캔). 결과는 s_scan 에 올린다
bool do_scan()
{
    s_scanning = true; bump();
    wifi_scan_config_t sc = {};
    sc.show_hidden = false;
    const bool prev = s_suppress;
    esp_err_t e = esp_wifi_scan_start(&sc, true);
    if (e != ESP_OK && !s_ok) {                                       // 연결 시도 중에는 스캔이 안 된다 → 시도를 멈추고 다시
        s_suppress = true;
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(300));
        e = esp_wifi_scan_start(&sc, true);
    }
    std::vector<wifistore::Ap> raw;
    if (e == ESP_OK) {
        uint16_t n = 0;
        esp_wifi_scan_get_ap_num(&n);
        std::vector<wifi_ap_record_t> aps(n);
        if (n && esp_wifi_scan_get_ap_records(&n, aps.data()) == ESP_OK)
            for (uint16_t k = 0; k < n; ++k) {
                wifistore::Ap a;
                a.ssid = (const char *)aps[k].ssid; a.rssi = aps[k].rssi; a.secured = aps[k].authmode != WIFI_AUTH_OPEN;
                raw.push_back(a);
            }
    } else ESP_LOGW(TAG, "wifi scan failed: %s", esp_err_to_name(e));
    auto list = wifistore::dedupe(raw);
    const unsigned count = (unsigned)list.size();
    { Lock l; s_scan = std::move(list); }
    s_suppress = prev;
    s_scanning = false; bump();
    ESP_LOGI(TAG, "wifi scan: %u networks", count);
    return e == ESP_OK;
}

bool scan_empty() { Lock l; return s_scan.empty(); }

void connect_to(const wifistore::Cred &c)
{
    // 이전 연결의 disconnect 이벤트가 C6(SDIO)를 거쳐 늦게 와서 새 시도의 실패로 잘못 읽히지 않도록, 그 이벤트를 기다려 비운 뒤 시작한다(원본 검수 2026-09-26)
    const bool was_linked = s_ok || s_have_target;
    xEventGroupClearBits(s_eg, B_FAIL);
    esp_wifi_disconnect();
    if (was_linked) xEventGroupWaitBits(s_eg, B_FAIL, pdTRUE, pdFALSE, pdMS_TO_TICKS(1500));
    vTaskDelay(pdMS_TO_TICKS(100));
    xEventGroupClearBits(s_eg, B_OK | B_FAIL);
    wifi_config_t sta = {};
    strlcpy((char *)sta.sta.ssid, c.ssid.c_str(), sizeof sta.sta.ssid);
    strlcpy((char *)sta.sta.password, c.pass.c_str(), sizeof sta.sta.password);
    esp_wifi_set_config(WIFI_IF_STA, &sta);
    { Lock l; strlcpy(s_connecting, c.ssid.c_str(), sizeof s_connecting); }
    s_have_target = true;
    esp_wifi_connect();
    ESP_LOGI(TAG, "wifi connecting: %s", c.ssid.c_str());             // 비밀번호는 로그에 남기지 않는다
}

// 스캔 결과로 순위를 다시 매기고, 끊겨 있으면 가장 센 후보에 붙는다 (D12)
void auto_connect(bool rescan)
{
    if (rescan) {
        s_suppress = true;
        do_scan();
        for (int retry = 0; retry < 2 && scan_empty(); ++retry) {     // 부팅 직후에는 C6 가 준비 전이라 0개가 나온다 → 잠시 뒤 다시
            vTaskDelay(pdMS_TO_TICKS(1500));
            do_scan();
        }
        s_suppress = false;
    }
    std::vector<wifistore::Ap> scan;
    { Lock l; scan = s_scan; }
    s_rank = wifistore::rank(s_known, scan);
    s_rank_pos = 0;
    if (s_rank.empty()) {
        if (!s_ok) { s_have_target = false; esp_wifi_disconnect(); }
        s_none = !s_ok;
        bump();
        ESP_LOGW(TAG, "wifi: no known network visible (%u seen)", (unsigned)scan.size());
        return;
    }
    if (s_none.exchange(false)) bump();
    std::string order;
    for (int i : s_rank) { if (!order.empty()) order += ", "; order += s_known[i].ssid; }
    ESP_LOGI(TAG, "wifi: known visible by signal: %s", order.c_str());
    if (!s_ok) { s_auto_fails = 0; connect_to(s_known[s_rank[0]]); }
}

bool is_auth_reason(int r) { return r == 2 || r == 14 || r == 15 || r == 202 || r == 204; }

// 화면에서 요청한 연결: 15초 안에 IP 를 받으면 저장, 인증 실패·시간 초과는 상태로 알리고 자동 연결로 돌아간다
void manual_connect(const Msg &m)
{
    wifistore::Cred c;
    c.ssid = m.ssid; c.pass = m.pass;
    if (m.use_saved) { int k = wifistore::find(s_known, c.ssid); if (k >= 0) c.pass = s_known[k].pass; }
    s_suppress = true;
    connect_to(c);
    const int64_t t0 = now_ms();
    int auth_fails = 0;
    bool ok = false;
    netwifi::Manual fail = netwifi::Manual::FAIL_TIMEOUT;
    while (now_ms() - t0 < MANUAL_LIMIT_MS) {
        EventBits_t b = xEventGroupWaitBits(s_eg, B_OK | B_FAIL, pdTRUE, pdFALSE, pdMS_TO_TICKS(500));
        if (b & B_OK) { ok = true; break; }
        if (b & B_FAIL) {
            const int reason = s_disc_reason;
            ESP_LOGW(TAG, "manual connect: disconnected (reason %d)", reason);
            if (is_auth_reason(reason) && ++auth_fails >= 2) { fail = netwifi::Manual::FAIL_AUTH; break; }
            vTaskDelay(pdMS_TO_TICKS(300));
            esp_wifi_connect();
        }
    }
    s_suppress = false;
    if (ok) {
        if (!c.pass.empty() || wifistore::find(s_known, c.ssid) < 0) { wifistore::upsert(s_saved, c.ssid, c.pass); nvs_store_saved(); rebuild_known(); }
        ESP_LOGI(TAG, "manual connect OK: %s (saved)", c.ssid.c_str());
        set_manual(netwifi::Manual::OK);
    } else {
        ESP_LOGW(TAG, "manual connect FAILED: %s (%s)", c.ssid.c_str(), fail == netwifi::Manual::FAIL_AUTH ? "auth" : "timeout");
        esp_wifi_disconnect();
        set_manual(fail);
        auto_connect(false);                                          // 원래 쓰던 망으로 돌아가 본다
    }
}

void refresh_rssi()
{
    wifi_ap_record_t ap = {};
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) return;
    const int prev = s_rssi.exchange(ap.rssi);
    if (!s_ok) { s_rssi = -127; return; }                             // 읽는 사이 끊김 → 끊김 처리가 넣은 -127 을 덮어쓰지 않게 되돌린다
    if (prev == -127 || wifistore::bars(prev) != wifistore::bars(ap.rssi)) bump();
    wifi_ps_type_t ps = WIFI_PS_NONE;                                 // N-D5
    s_ps = esp_wifi_get_ps(&ps) == ESP_OK ? (int)ps : -1;
}

void manager_loop()
{
    nvs_load_saved();
    rebuild_known();
    auto_connect(true);
    int64_t last_rescan = now_ms(), last_rssi = 0;
    while (true) {
        Msg m;
        if (xQueueReceive(s_q, &m, pdMS_TO_TICKS(1000)) == pdTRUE) {
            switch (m.type) {
            case Req::SCAN:
                s_suppress = true; do_scan(); s_suppress = false;
                // 목록이 나왔으니 "검색 중" 을 먼저 끈다(아래 연결은 1~2초 걸릴 수 있다).
                // 음수로 내려갔을 때만 0 으로 되돌린다 — 무조건 0 을 넣으면 그 사이 들어온 새 요청(+1)을 지워 버린다.
                if (s_scan_pending.fetch_sub(1) <= 0) s_scan_pending = 0;
                bump();
                if (!s_ok) auto_connect(false);
                last_rescan = now_ms();
                break;
            case Req::NEXT:
                if (s_ok) break;
                if (++s_rank_pos < (int)s_rank.size()) {
                    ESP_LOGW(TAG, "wifi: next candidate %d/%u", s_rank_pos + 1, (unsigned)s_rank.size());
                    connect_to(s_known[s_rank[s_rank_pos]]);
                } else {
                    vTaskDelay(pdMS_TO_TICKS(3000));
                    auto_connect(true);
                    last_rescan = now_ms();
                }
                break;
            case Req::CONNECT:
                manual_connect(m);
                break;
            case Req::FORGET:
                if (!m.ssid[0]) s_saved.clear();
                else s_saved.erase(std::remove_if(s_saved.begin(), s_saved.end(), [&](const wifistore::Cred &c) { return c.ssid == m.ssid; }), s_saved.end());
                nvs_store_saved();
                rebuild_known();
                ESP_LOGW(TAG, "wifi: forgot %s", m.ssid[0] ? m.ssid : "(all saved)");
                break;
            }
        }
        // 아는 망이 안 보이면 20초마다 다시 찾는다(설정 화면이 열려 있으면 사용자 조작에 맡긴다)
        if (!s_ok && s_none && now_ms() - last_rescan > RESCAN_MS && !s_setup_open) { auto_connect(true); last_rescan = now_ms(); }
        // N-D3 신호 세기. 연결 직후(아직 -127)에는 다음 바퀴에 바로 읽는다
        if (s_ok && now_ms() - last_rssi > (s_rssi == -127 ? 900 : RSSI_PERIOD_MS)) { last_rssi = now_ms(); refresh_rssi(); }
    }
}

void init_failed(const char *what)                                    // N-D1
{
    printf("PHONE FAIL %s\n", what);
    fflush(stdout);
    s_init_failed = true;
    // 초기화가 끝나기 전에 화면에서 눌러 둔 "연결" 요청은 이제 아무도 처리하지 못한다 → CONNECTING(입력 잠금)에 갇히지 않게 실패로 돌린다
    bool stuck;
    { Lock l; stuck = (s_manual == netwifi::Manual::CONNECTING); }
    if (stuck) set_manual(netwifi::Manual::FAIL_TIMEOUT);
    bump();
}

void wifi_task(void *)
{
    esp_err_t e = esp_netif_init();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) { init_failed("wifi netif"); vTaskDelete(nullptr); return; }
    e = esp_event_loop_create_default();                              // 다른 모듈이 먼저 만들었으면 INVALID_STATE — 그대로 쓴다
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) { init_failed("wifi event-loop"); vTaskDelete(nullptr); return; }

    // C6 전원은 M5.begin() 이 익스팬더 0x44 bit0(WLAN_PWR_EN) 으로 켠다.
    // esp_hosted 2.11.x: 핀은 실행 중 구조체로 준다. 생성자 자동초기화는 tools/patch_esp_hosted.py 로 꺼 두었다.
    {
        struct esp_hosted_sdio_config conf = INIT_DEFAULT_HOST_SDIO_CONFIG();
        conf.pin_clk.pin = 12; conf.pin_cmd.pin = 13;
        conf.pin_d0.pin = 11;  conf.pin_d1.pin = 10; conf.pin_d2.pin = 9; conf.pin_d3.pin = 8;
        conf.pin_reset.pin = 15;
        e = esp_hosted_sdio_set_config(&conf);
        if (e != ESP_OK) { ESP_LOGE(TAG, "esp_hosted_sdio_set_config: %s", esp_err_to_name(e)); init_failed("esp_hosted sdio-config"); vTaskDelete(nullptr); return; }
    }
    if (esp_hosted_init() != 0)             { init_failed("esp_hosted init"); vTaskDelete(nullptr); return; }
    if (esp_hosted_connect_to_slave() != 0) { init_failed("esp_hosted connect-to-slave"); vTaskDelete(nullptr); return; }
    {
        esp_hosted_coprocessor_fwver_t v = {};
        if (esp_hosted_get_coprocessor_fwversion(&v) == ESP_OK) ESP_LOGI(TAG, "C6 slave fw %u.%u.%u", (unsigned)v.major1, (unsigned)v.minor1, (unsigned)v.patch1);
    }

    if (!esp_netif_create_default_wifi_sta()) { init_failed("wifi netif-sta"); vTaskDelete(nullptr); return; }
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&wcfg) != ESP_OK) { init_failed("wifi init"); vTaskDelete(nullptr); return; }
    if (esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_event, nullptr) != ESP_OK ||
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_event, nullptr) != ESP_OK) { init_failed("wifi event-handler"); vTaskDelete(nullptr); return; }
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK || esp_wifi_start() != ESP_OK) { init_failed("wifi start"); vTaskDelete(nullptr); return; }
    // N-D4 Wi-Fi 절전 끄기. 이유: 기본값(모뎀 절전)이면 공유기가 DTIM 마다 모아 둔 자료를 한꺼번에 줘서 통화 중 받는 음성이 0.3초씩 뭉쳐 온다(2026-10-05 실측).
    //      비용: Wi-Fi 칩이 늘 깨어 있어 전력을 더 쓴다 — USB 전원 탁상 전화기라 수용. 탈출구: 이 블록을 지우거나 WIFI_PS_MIN_MODEM; 배터리로 쓰면 통화 중에만 끈다.
    //      실패해도 부팅을 멈추지 않는다(C6 펌웨어가 모를 수 있음). 한 번만 건다: 호스트 쪽에는 이 설정이 사라질 길이 없다(esp_wifi_start·init 은 여기서 각 1회, 재접속·망 전환은
//      connect/disconnect/scan 만 쓴다 — fix 3 검수가 소스로 확인). 단 C6 가 혼자 리셋된 뒤에도 남는지는 확인하지 못했다 → N-D5 가 지금 값을 읽어 TUNE 줄의 ps= 로 보여 준다.
//   N-D5 (Phase 3c, fix 3 검수 Minor ①) 절전 상태를 신호 세기와 같은 박자(10초)로 읽어 둔다: esp_wifi_get_ps → ps(). 0 = 꺼짐(WIFI_PS_NONE), 1·2 = 켜짐, -1 = 못 읽음·아직 안 읽음.
//      이유: 절전이 다시 켜지면 받는 음성이 0.3초씩 뭉쳐 오는데(2026-10-05 아침의 고장) 로그로는 "망이 나쁜 것"과 구별되지 않는다. 화면 태스크가 아니라 매니저 태스크가 읽는다(N-D3 과 같은 이유).
//      비용: 10초마다 C6 까지 다녀오는 호출이 하나 는다. 0 이 아니어도 스스로 다시 걸지는 않는다(값을 보고 정한다 — 계획서 PL3c-D11). 탈출구: refresh_rssi 의 두 줄을 지운다.
    {
        const esp_err_t pe = esp_wifi_set_ps(WIFI_PS_NONE);
        if (pe == ESP_OK) ESP_LOGI(TAG, "wifi power save off");
        else ESP_LOGW(TAG, "wifi power save off FAILED: %s (0x%x) — continuing", esp_err_to_name(pe), (unsigned)pe);
    }
    s_started = true;
    bump();
    manager_loop();                                                   // 돌아오지 않는다
}

}  // namespace

namespace netwifi {

void start()
{
    static bool called = false;
    if (called) return;                                               // 이미 시작함
    called = true;
    s_lock = xSemaphoreCreateMutex();
    s_eg = xEventGroupCreate();
    QueueHandle_t q = xQueueCreate(8, sizeof(Msg));
    if (!q || !s_eg || !s_lock) { init_failed("wifi alloc"); return; }
    s_q = q;                                                          // 큐를 마지막에 연다: 요청(post)이 들어올 수 있을 때는 잠금·이벤트 그룹이 이미 있다
    // 스택 10KB·우선순위 5·코어 0 — 원본과 같다
    if (xTaskCreatePinnedToCore(wifi_task, "wifi", 10 * 1024, nullptr, 5, nullptr, 0) != pdPASS) init_failed("wifi task");
}

Phase phase()
{
    if (s_init_failed) return Phase::INIT_FAILED;
    if (!s_started) return Phase::STARTING;
    if (s_ok) return Phase::CONNECTED;
    if (s_none) return Phase::NO_KNOWN;
    return Phase::SEARCHING;
}

bool connected() { return s_ok; }

void ssid(char *out, size_t cap)
{
    if (!out || !cap) return;
    out[0] = 0;
    if (!s_ok) return;
    Lock l;
    strlcpy(out, s_ssid, cap);
}

int rssi() { return s_ok ? s_rssi.load() : -127; }
int ps() { return s_ps.load(); }

uint32_t change_seq() { return s_seq; }

void set_setup_open(bool open) { s_setup_open = open; }

void request_scan()
{
    if (s_init_failed) return;                                        // 매니저가 없다 — 요청이 영원히 남지 않게
    s_scan_pending.fetch_add(1);
    bump();
    if (!post(Req::SCAN)) { s_scan_pending.fetch_sub(1); bump(); }
}

bool scan_busy() { return !s_init_failed && (s_scanning || s_scan_pending > 0); }

std::vector<wifistore::Ap> scan_results()
{
    Lock l;
    return s_scan;
}

bool is_known(const std::string &ssid)
{
    Lock l;
    return std::find(s_known_ssids.begin(), s_known_ssids.end(), ssid) != s_known_ssids.end();
}

void request_connect(const char *ssid, const char *pass, bool use_saved)
{
    if (!ssid || !*ssid) return;
    if (!pass) pass = "";
    if (manual_state() == Manual::CONNECTING) return;                 // 이미 시도 중 — 겹친 요청은 버린다(진행 중인 상태를 건드리지 않는다)
    if (!use_saved && strlen(pass) < 8) { set_manual(Manual::SHORT_PASS); return; }   // WPA 비밀번호는 8자 이상
    if (s_init_failed) { set_manual(Manual::FAIL_TIMEOUT); return; }  // 매니저가 없다
    set_manual(Manual::CONNECTING);                                   // N-D2
    ESP_LOGI(TAG, "wifi setup: connect %s (%s)", ssid, use_saved ? "saved/open" : "typed password");   // 비밀번호·길이는 남기지 않는다
    // 큐가 찼거나, 요청을 넣는 사이 초기화가 실패했으면(매니저가 없다) "연결"이 반응 없이 남지 않게 실패로 돌린다
    if (!post(Req::CONNECT, ssid, use_saved ? "" : pass, use_saved) || s_init_failed) set_manual(Manual::FAIL_TIMEOUT);
}

Manual manual_state()
{
    Lock l;
    return s_manual;
}

int64_t manual_since_ms()
{
    Lock l;
    return s_manual_ms;
}

void request_forget(const char *ssid) { post(Req::FORGET, ssid ? ssid : ""); }

}  // namespace netwifi
