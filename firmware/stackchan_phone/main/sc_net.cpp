// sc_net.cpp — 스택짱 전화기의 Wi-Fi. 인터페이스는 Tab5 의 ../../tab5_phone/main/net_wifi.h 를 그대로 쓴다(네임스페이스 netwifi) —
//   교환기 연결(relay_link.cpp)이 Tab5 와 같은 소스이고 그것이 netwifi::connected() 를 부른다.
// 근거: 이 기기에서 실제로 붙은 관문 시험 펌웨어 firmware/stackchan_phone_probe/main/net_probe.cpp (2026-10-08 — 10분 동시 가동, 교환기 왕복 40여 회).
//   그대로 가져온 것: esp_wifi 만으로 붙기(S3 는 Wi-Fi 가 칩 안에 있다 — Tab5 의 esp_hosted 없음), 설정을 RAM 에만(WIFI_STORAGE_RAM), 절전 끔(WIFI_PS_NONE — Tab5 N-D4),
//   스캔 → 아는 망 중 신호 센 순서로 붙어 보기(wifi_store.h — Tab5 와 같은 순수 로직), 끊기면 다시 붙기, 태스크 스택을 PSRAM 에.
//   시험 펌웨어와 다른 점:
//   SN-D1 Wi-Fi 설정 화면이 없다 — 아는 망은 secrets.h 의 고정 목록(WIFI_NETWORKS)뿐이다. net_wifi.h 의 "설정 화면용" 함수들은 구현하지 않는다(부르면 링크 오류).
//         이유: 320×240 화면에 글자판을 넣지 않는다(설계서 TW-D12). 사무실 Wi-Fi 는 이미 고정 목록에 있다(같은 목록으로 Tab5 2번과 관문 시험이 붙었다).
//         비용: 다른 곳으로 옮기면 secrets.h 를 고쳐 다시 올려야 한다. 탈출구: 저장 목록을 nvs_phone 에 두고 시리얼 명령으로 넣는다.
//   SN-D2 Wi-Fi 가 NVS 를 쓰지 않는다(sdkconfig: ESP_WIFI_NVS_ENABLED=n, PHY 보정 저장 끔). 이유: 기본 nvs 칸은 스트림덱(UIFlow2)의 것이다 —
//         거기에 우리 Wi-Fi 설정·보정 값을 섞어 쓰지 않는다. 비용: 부팅마다 PHY 전체 보정(수십 ms 더). 탈출구: sdkconfig 두 줄.
//   SN-D3 신호 세기·절전 상태를 10초마다 읽어 둔다(Tab5 와 같은 주기). 막대 칸수가 바뀔 때만 change_seq 를 올린다.
#include "net_wifi.h"

#include <atomic>
#include <cstdio>
#include <cstring>

#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "secrets.h"            // WIFI_NETWORKS — git 제외. 내용을 로그·화면에 내지 않는다(망 이름만 낸다)

namespace netwifi {
namespace {

const char *TAG = "sc_net";

struct FixedCred { const char *ssid; const char *pass; };
const FixedCred FIXED[] = WIFI_NETWORKS;
const int FIXED_N = sizeof(FIXED) / sizeof(FIXED[0]);

constexpr int TASK_CORE = 0, TASK_PRIO = 4, TASK_STACK = 8 * 1024;
constexpr uint32_t CONNECT_TIMEOUT_MS = 15000, RETRY_MS = 5000, POLL_MS = 10000;

EventGroupHandle_t s_eg = nullptr;
const EventBits_t B_OK = BIT0, B_FAIL = BIT1;
SemaphoreHandle_t s_name_lock = nullptr;        // s_ssid 를 읽고 쓰는 동안
std::atomic<Phase> s_phase{Phase::STARTING};
std::atomic<int> s_rssi{-127}, s_ps{-1};
std::atomic<uint32_t> s_seq{0};
char s_ssid[33] = "", s_try[33] = "";

void set_phase(Phase p) { if (s_phase.exchange(p) != p) ++s_seq; }

void on_event(void *, esp_event_base_t base, int32_t id, void *)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) xEventGroupSetBits(s_eg, B_FAIL);
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) xEventGroupSetBits(s_eg, B_OK);
}

std::vector<wifistore::Cred> load_known()
{
    std::vector<wifistore::Cred> saved, fixed;
    for (int i = 0; i < FIXED_N; ++i) { wifistore::Cred c; c.ssid = FIXED[i].ssid; c.pass = FIXED[i].pass; fixed.push_back(c); }
    return wifistore::merge(saved, fixed);
}

std::vector<wifistore::Ap> scan()
{
    std::vector<wifistore::Ap> raw;
    for (int attempt = 0; attempt < 3 && raw.empty(); ++attempt) {
        if (attempt) vTaskDelay(pdMS_TO_TICKS(1500));
        wifi_scan_config_t sc = {};
        if (esp_wifi_scan_start(&sc, true) != ESP_OK) continue;
        uint16_t n = 0; esp_wifi_scan_get_ap_num(&n);
        std::vector<wifi_ap_record_t> aps(n);
        if (n && esp_wifi_scan_get_ap_records(&n, aps.data()) == ESP_OK)
            for (uint16_t k = 0; k < n; ++k) { wifistore::Ap a; a.ssid = (const char *)aps[k].ssid; a.rssi = aps[k].rssi; a.secured = aps[k].authmode != WIFI_AUTH_OPEN; raw.push_back(a); }
        else esp_wifi_clear_ap_list();          // 결과를 가져가지 않으면 드라이버가 쥐고 있다
    }
    return wifistore::dedupe(raw);
}

bool try_connect(const wifistore::Cred &c)
{
    esp_wifi_disconnect(); vTaskDelay(pdMS_TO_TICKS(300));
    xEventGroupClearBits(s_eg, B_OK | B_FAIL);
    wifi_config_t sta = {};
    strlcpy((char *)sta.sta.ssid, c.ssid.c_str(), sizeof sta.sta.ssid);
    strlcpy((char *)sta.sta.password, c.pass.c_str(), sizeof sta.sta.password);
    if (esp_wifi_set_config(WIFI_IF_STA, &sta) != ESP_OK) return false;
    strlcpy(s_try, c.ssid.c_str(), sizeof s_try);
    ESP_LOGI(TAG, "wifi connecting: %s", c.ssid.c_str());           // 비밀번호는 로그에 남기지 않는다
    esp_wifi_connect();
    const int64_t t0 = esp_timer_get_time();
    while (esp_timer_get_time() - t0 < (int64_t)CONNECT_TIMEOUT_MS * 1000) {
        const EventBits_t b = xEventGroupWaitBits(s_eg, B_OK | B_FAIL, pdTRUE, pdFALSE, pdMS_TO_TICKS(500));
        if (b & B_OK) return true;
        if (b & B_FAIL) { vTaskDelay(pdMS_TO_TICKS(500)); esp_wifi_connect(); }
    }
    return false;
}

bool read_link()                                // SN-D3. 돌려주는 값 = 지금 공유기에 붙어 있나(끊김 사건을 놓쳤을 때를 위한 두 번째 확인)
{
    wifi_ap_record_t ap = {};
    const bool linked = esp_wifi_sta_get_ap_info(&ap) == ESP_OK;
    const int now = linked ? ap.rssi : -127;
    const int before = s_rssi.exchange(now);
    wifi_ps_type_t ps = WIFI_PS_NONE;
    const int ps_now = esp_wifi_get_ps(&ps) == ESP_OK ? (int)ps : -1;
    const int ps_before = s_ps.exchange(ps_now);
    if (wifistore::bars(now) != wifistore::bars(before) || ps_now != ps_before) ++s_seq;
    return linked;
}

void fail_init(const char *what)
{
    printf("PHONE FAIL wifi %s\n", what);
    set_phase(Phase::INIT_FAILED);
    while (true) vTaskDelay(portMAX_DELAY);     // 스택을 PSRAM 에 만든 태스크는 스스로 지우지 않는다(IDF: WithCaps 태스크는 다른 태스크가 지워야 한다) — 잠들어 둔다
}

void wifi_task(void *)
{
    if (esp_netif_init() != ESP_OK || esp_event_loop_create_default() != ESP_OK) { fail_init("netif"); }
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&wcfg) != ESP_OK) { fail_init("init"); }
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_event, nullptr);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_event, nullptr);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);                             // SN-D2
    esp_wifi_set_mode(WIFI_MODE_STA);
    if (esp_wifi_start() != ESP_OK) { fail_init("start"); }
    esp_wifi_set_ps(WIFI_PS_NONE);                                      // 절전 끔: 받는 음성이 0.3초 주기로 뭉치던 원인(Tab5 N-D4)

    const auto known = load_known();
    ESP_LOGI(TAG, "wifi known: fixed %d", FIXED_N);
    set_phase(Phase::SEARCHING);
    while (true) {
        if (s_phase.load() != Phase::CONNECTED) {
            const auto aps = scan();
            const auto order = wifistore::rank(known, aps);
            bool ok = false;
            for (int idx : order) if ((ok = try_connect(known[idx]))) break;
            if (ok) {
                xSemaphoreTake(s_name_lock, portMAX_DELAY);
                strlcpy(s_ssid, s_try, sizeof s_ssid);
                xSemaphoreGive(s_name_lock);
                esp_wifi_set_ps(WIFI_PS_NONE);
                xEventGroupClearBits(s_eg, B_FAIL);
                set_phase(Phase::CONNECTED);
                read_link();
                printf("PHONE WIFI ok ssid=%s rssi=%d\n", s_ssid, s_rssi.load());
            } else {
                if (order.empty()) ESP_LOGW(TAG, "wifi: no known network visible (%u seen)", (unsigned)aps.size());
                set_phase(order.empty() ? Phase::NO_KNOWN : Phase::SEARCHING);
                vTaskDelay(pdMS_TO_TICKS(RETRY_MS));
            }
            continue;
        }
        // 붙어 있는 동안: 끊김을 기다리며 10초마다 신호를 읽는다
        const EventBits_t b = xEventGroupWaitBits(s_eg, B_FAIL, pdTRUE, pdFALSE, pdMS_TO_TICKS(POLL_MS));
        if ((b & B_FAIL) || !read_link()) {     // 끊김 사건이 왔거나, 10초 점검에서 공유기에 붙어 있지 않다(붙는 순간과 겹쳐 사건을 놓친 경우)
            ESP_LOGW(TAG, "wifi lost");
            xSemaphoreTake(s_name_lock, portMAX_DELAY);
            s_ssid[0] = 0;
            xSemaphoreGive(s_name_lock);
            s_rssi = -127;
            set_phase(Phase::SEARCHING);
            printf("PHONE WIFI lost\n");
        }
    }
}

}  // namespace

void start()
{
    static StaticSemaphore_t lock_buf;
    if (s_eg) return;                           // 한 번만
    s_eg = xEventGroupCreate();
    s_name_lock = xSemaphoreCreateMutexStatic(&lock_buf);
    // 스택은 PSRAM 에(관문 실험: 내부 메모리가 빠듯하다 — TLS 연결 중 최저 13KB). 안 되면 내부 메모리로.
    if (xTaskCreatePinnedToCoreWithCaps(wifi_task, "wifi", TASK_STACK, nullptr, TASK_PRIO, nullptr, TASK_CORE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS &&
        xTaskCreatePinnedToCore(wifi_task, "wifi", TASK_STACK, nullptr, TASK_PRIO, nullptr, TASK_CORE) != pdPASS) {
        printf("PHONE FAIL wifi task\n");
        set_phase(Phase::INIT_FAILED);
    }
}

Phase phase() { return s_phase.load(); }
bool connected() { return s_phase.load() == Phase::CONNECTED; }

void ssid(char *out, size_t cap)
{
    if (!out || !cap) return;
    out[0] = 0;
    if (!s_name_lock) return;
    xSemaphoreTake(s_name_lock, portMAX_DELAY);
    strlcpy(out, s_ssid, cap);
    xSemaphoreGive(s_name_lock);
}

int rssi() { return s_rssi.load(); }
int ps() { return s_ps.load(); }
uint32_t change_seq() { return s_seq.load(); }

}  // namespace netwifi
