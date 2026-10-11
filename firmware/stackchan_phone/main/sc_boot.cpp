// sc_boot.cpp — 설명은 sc_boot.h.
//   SB-D1 스트림덱으로 갈 때는 esp_ota_set_boot_partition(factory) 를 쓴다 — IDF 는 factory 를 고르면 otadata 를 지운다(= "ota 선택 없음 → factory 부팅").
//         이유: otadata 를 직접 지우는 것보다 IDF 의 검사(대상 칸에 올바른 앱 이미지가 있는가)를 거친다. 비용·탈출구: 없음(실패하면 false 로 알리고 그대로 돈다).
//         이유: otadata 를 직접 지우는 것보다 IDF 의 검사를 거친다. 그 검사가 실패하면 SB-D4(아래 set_boot_to_deck)로 간다.
//   SB-D2 재시작 전에 0.2초 쉰다: "SCBOOT to-deck" 줄이 USB 로 나갈 시간(화면의 "스트림덱으로 가는 중"은 부르는 쪽이 먼저 그린다).
//   SB-D5 죽는 고리 막기: 이 펌웨어가 부팅하자마자 죽기를 되풀이하면 사용자는 스트림덱으로 돌아갈 길이 없다(화면의 키캡을 누를 틈이 없다).
//         패닉·감시 타이머로 다시 켜진 것이 세 번 이어지면 스스로 스트림덱으로 돌아간다. 이유: 이 기기는 사무실 책상에서 PC 없이 쓴다 — 벽돌처럼 보이면 안 된다.
//         비용: 전화기 펌웨어에 죽는 버그가 있으면 조용히 스트림덱으로 돌아가 버린다(원인은 USB 로그로만 보인다). 소리 장치 실패의 "다시 켜 보기"(sc_main boot_guard — esp_restart)는
//         죽은 것으로 세지 않는다. 탈출구: CRASH_LIMIT 를 크게.
//         한계: app_main 에 닿기 전에 죽는 경우(PSRAM 초기화 실패 등 시작 코드의 abort)는 세지 못한다 — 그때는 USB 로 다시 올리거나 otadata 를 지워야 한다(README "복구").
//   SB-D6 멈춤 감시: app 태스크(화면·터치·통화 진행)가 20초째 한 바퀴도 못 돌면 화면이 얼어 "스트림덱" 키캡도 못 누른다 → 일부러 죽어 다시 켠다(위 SB-D5 가 센다).
//         감시는 esp_timer 태스크(내부 메모리 스택)에서 1초마다. 비용: app 태스크가 정당하게 20초 넘게 막히는 일이 생기면 재부팅된다(지금 가장 긴 것은 신호 보내기 2초).
//   SB-D3 플래시를 만지는 일(부팅 대상 쓰기, 스트림덱 앱 확인)은 내부 메모리 스택에서 한다(sc_store.h SS-D1): to_deck 은 scstore::run 을 거치고,
//         has_deck 은 부팅 때(mark_running_ok — main 태스크) 한 번 확인해 둔 값을 돌려준다.
//   SB-D7 부팅 때 USB(USB-Serial/JTAG) 단자 설정을 칩이 전원 켜질 때의 값으로 되돌린다(usb_restore). 스트림덱(UIFlow2)은 같은 단자를 USB-OTG 로 쓰면서
//         USB-Serial/JTAG 쪽을 "풀업 끔 고정 + 패드 끔"으로 둔다(2026-10-09 스트림덱에서 읽은 값: USB_SERIAL_JTAG.conf0 = 0x100, RTC_CNTL_USB_CONF = 0x180000).
//         넘어올 때의 machine.reset() 은 CPU 만 다시 켜서(IDF 의 esp_restart — 주변 장치 레지스터가 남는다) 이 값이 그대로 오고, IDF 의 USB 드라이버는
//         단자 선택과 패드만 되돌릴 뿐 풀업 고정은 풀지 않는다 → 맥이 기기를 보지 못한다(첫 설치에서: 전화기는 돌고 교환기에도 붙었는데 USB 에 기기가 없어
//         상태 확인·앱 갱신에 사람 손이 필요했다). 되돌리는 것: 단자 → USB-Serial/JTAG, 풀업·기준 전압 고정 해제, 핀 맞바꾸기 해제, 패드 켬.
//         "전원 켜질 때의 값"은 IDF 의 레지스터 정의(soc/esp32s3/register/soc/usb_serial_jtag_reg.h 의 CONF0 — default: 패드 켬 1, D+ 풀업 1, 나머지 0 = 0x4200)를 따른다.
//         전원을 켜서 뜬 부팅에는 같은 값을 다시 쓰는 것이라 달라지는 것이 없다. 비용: 부팅 때 레지스터 쓰기 몇 번. 판별(from_deck)은 기록용일 뿐이다 —
//         "소프트웨어가 단자를 USB-OTG 로 돌려 놓았다"는 이 펌웨어가 만들지 않는 상태다(우리 재시작 뒤에는 USB-Serial/JTAG 쪽으로 남는다).
//         확인: 2026-10-09 이 기기에서 고치기 전에는 스트림덱에서 넘어온 뒤 USB 에 안 잡혔고, 고친 뒤에는 3초 안에 잡혔다(왕복 두 번).
//         탈출구: 그래도 안 잡히면 RST 를 짧게 한 번(하드웨어 재시작은 전부 지운다).
#include "sc_boot.h"

#include <atomic>
#include <cstdio>

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/usb_serial_jtag_ll.h"
#include "soc/rtc_cntl_struct.h"

#include "sc_store.h"

namespace scboot {
namespace {

const char *TAG = "sc_boot";

bool s_has_deck = false;

constexpr uint32_t CRASH_MAGIC = 0x5C0B0071u, CRASH_LIMIT = 3;         // SB-D5
struct CrashMark { uint32_t magic, count; };
RTC_NOINIT_ATTR CrashMark s_crash;                                     // 재시작(패닉 포함)에는 남고, 전원을 껐다 켜면 쓰레기 값 → magic 으로 가린다
constexpr int64_t STALL_US = 20 * 1000000LL, STABLE_US = 60 * 1000000LL;   // SB-D6
std::atomic<int64_t> s_alive_at{0};
int64_t s_watch_from = 0;
bool s_stable = false;

const esp_partition_t *deck_partition()
{
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);
}

bool set_boot_to_deck(void *)                                          // 내부 메모리 스택에서(SB-D3)
{
    const esp_partition_t *p = deck_partition();
    if (!p) { ESP_LOGE(TAG, "no factory partition - cannot go back to the deck"); return false; }
    const esp_err_t e = esp_ota_set_boot_partition(p);              // SB-D1: 스트림덱 앱(9MB)의 검사합을 확인한 뒤 otadata 를 지운다(1초 안팎)
    if (e == ESP_OK) return true;
    // SB-D4 IDF 의 검사가 실패해도(읽기 오류·이미지 형식 차이) 돌아가는 길을 막지 않는다: otadata 를 직접 지운다 = "고른 것 없음 → factory 부터".
    //   부트로더가 부팅 때 그 앱을 다시 검사하고, 못 쓰면 다음 앱 칸(이 전화기)으로 넘어온다 — 벽돌이 되지 않는다. 비용: IDF 의 사전 검사를 건너뛴다.
    ESP_LOGW(TAG, "set boot partition: %s -> erasing otadata directly", esp_err_to_name(e));
    const esp_partition_t *od = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
    if (!od) { ESP_LOGE(TAG, "no otadata partition"); return false; }
    const esp_err_t e2 = esp_partition_erase_range(od, 0, od->size);
    if (e2 != ESP_OK) { ESP_LOGE(TAG, "otadata erase: %s", esp_err_to_name(e2)); return false; }
    return true;
}

void watch_tick(void *)                                                // esp_timer 태스크에서 1초마다(SB-D6)
{
    const int64_t now = esp_timer_get_time();
    if (!s_stable && now - s_watch_from >= STABLE_US) { s_stable = true; s_crash.count = 0; }   // 60초를 무사히 돌았다
    if (now - s_alive_at.load() >= STALL_US) {
        printf("SCBOOT stall -> restart\n"); fflush(stdout);
        esp_system_abort("app task stalled for 20 s");               // 패닉으로 다시 켜진다 → crash_guard 가 센다
    }
}

}  // namespace

void mark_running_ok()
{
    const esp_partition_t *run = esp_ota_get_running_partition();
    const esp_err_t e = esp_ota_mark_app_valid_cancel_rollback();      // 되돌리기 대기 상태가 아니면 아무것도 바꾸지 않는다(factory 에서 돌면 ESP_FAIL — 시험용으로 통째 올렸을 때)
    if (e != ESP_OK) ESP_LOGW(TAG, "mark valid: %s", esp_err_to_name(e));
    const esp_partition_t *p = deck_partition();
    esp_app_desc_t desc;
    s_has_deck = p && p != run && esp_ota_get_partition_description(p, &desc) == ESP_OK;   // 그 칸에 읽을 수 있는 앱 이미지가 있고, 그것이 지금 도는 나 자신이 아니다
    printf("SCBOOT running=%s deck=%d\n", run ? run->label : "?", (int)s_has_deck);
}

bool has_deck() { return s_has_deck; }

void usb_restore()                                                     // SB-D7
{
    const bool from_deck = RTCCNTL.usb_conf.sw_hw_usb_phy_sel && RTCCNTL.usb_conf.sw_usb_phy_sel;   // 단자가 USB-OTG(스트림덱의 것)로 돌려져 있었다
    usb_serial_jtag_ll_phy_enable_external(false);                     // 단자(칩 안의 USB PHY) → USB-Serial/JTAG
    USB_SERIAL_JTAG.conf0.dp_pullup = 1;                               // 풀업 값도 전원 켜질 때대로(D+ 만 올림) 맞춘 뒤
    USB_SERIAL_JTAG.conf0.dp_pulldown = 0;
    USB_SERIAL_JTAG.conf0.dm_pullup = 0;
    USB_SERIAL_JTAG.conf0.dm_pulldown = 0;
    usb_serial_jtag_ll_phy_set_pullup_strength(false);
    usb_serial_jtag_ll_phy_disable_pull_override();                    // "끔" 고정을 푼다 — 이것이 남아 있으면 맥이 기기를 보지 못한다
    usb_serial_jtag_ll_phy_disable_vref_override();
    usb_serial_jtag_ll_phy_enable_pin_exchg(false);
    usb_serial_jtag_ll_phy_enable_pad(true);
    if (from_deck) printf("SCBOOT usb restored (came from the deck)\n");
}

void crash_guard()
{
    const esp_reset_reason_t r = esp_reset_reason();
    // RTC 감시 타이머(ESP_RST_WDT)는 세지 않는다: esptool 의 --after watchdog-reset(tools/sc_flash.sh)이 바로 그 이유로 찍힌다 — 세면 연달아 세 번 올렸을 때 스트림덱으로 쫓겨난다.
    const bool crashed = r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT;
    if (s_crash.magic != CRASH_MAGIC) { s_crash.magic = CRASH_MAGIC; s_crash.count = 0; }
    s_crash.count = crashed ? s_crash.count + 1 : 0;
    printf("SCBOOT reset=%d crashes=%u\n", (int)r, (unsigned)s_crash.count);
    if (s_crash.count < CRASH_LIMIT) return;
    s_crash.count = 0;                                                 // 스트림덱에서 다시 넘어오면 처음부터 센다
    if (!s_has_deck) { printf("SCBOOT crash-loop but no deck to go back to\n"); return; }
    printf("SCBOOT crash-loop -> deck\n"); fflush(stdout);
    if (set_boot_to_deck(nullptr)) { vTaskDelay(pdMS_TO_TICKS(200)); esp_restart(); }   // main 태스크(내부 메모리 스택)에서 불린다
}

void start_watch()
{
    static esp_timer_handle_t timer = nullptr;
    if (timer) return;
    s_watch_from = esp_timer_get_time();
    s_alive_at = s_watch_from;
    const esp_timer_create_args_t args = { watch_tick, nullptr, ESP_TIMER_TASK, "sc_watch", true };
    if (esp_timer_create(&args, &timer) != ESP_OK || esp_timer_start_periodic(timer, 1000000) != ESP_OK) ESP_LOGW(TAG, "stall watch not started");
}

void alive() { s_alive_at = esp_timer_get_time(); }

bool to_deck()
{
    if (!s_has_deck || !scstore::run(set_boot_to_deck, nullptr)) return false;
    printf("SCBOOT to-deck\n"); fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(200));                                    // SB-D2
    esp_restart();
    return true;                                                       // 여기까지 오지 않는다
}

}  // namespace scboot
