// sc_main.cpp — 스택짱 2호기 전화기 펌웨어. 부팅, 태스크 기동, 시리얼 명령만 한다. 설계서 docs/exec-plans/2026-10-08-phone-3way-design.md §10·§11.
//
// 이 펌웨어는 스트림덱(UIFlow2)과 한 플래시에 함께 산다(이중 부팅 — TW-D14): 스트림덱의 "전화기" 버튼이 이쪽을 고르고, 화면의 "스트림덱" 키캡(또는 시리얼 'deck')이 돌아간다.
// 통화의 판단·소리 경로·교환기 연결은 Tab5 전화기(../../tab5_phone/main)와 같은 소스다(TW-D11 — main/CMakeLists.txt 가 경로로 끌어 쓴다).
// 스택짱 전용은 sc_* 와 audio_board_cores3.cpp 뿐이다.
// 태스크(코어): audio 스피커 출력(1) / v_cap 마이크 → 에코 제거(1) / v_enc Opus 압축(0) / v_rx Opus 해제(0) / v_tx 음성 보내기(0) / app 화면·통화 진행(1) / touch(1)
//              / wifi(0) / relay·relay_ws 교환기 연결(0) / flash 플래시 일꾼(0) / sercmd 시리얼 명령(0)
//   관문 실험 조건 1: 마이크·에코 제거·스피커는 코어 1, Opus·교환기 연결은 코어 0 — 한 코어에 모으면 돌지 않는다.
//
// 시리얼 명령(USB-Serial/JTAG, 줄 단위) — tools/sc_ctl.py 가 쓴다. Tab5 전화기와 같은 이름·같은 응답 줄이다:
//   stat | t <x> <y> | shot | reboot | deck (스트림덱으로 — 대기·연결 중에만) | mem | cpu [초]
//   quiet on|off (스피커 끔 — 저장된다) | volup | voldn | volstep [1..5]
//   소리·통화 품질(Tab5 와 같은 모듈 voice_cmd.cpp): selftest | vsrc … | txgain | vol | vping | tune | lvl · tilt · res · guard · rest · dt · rxgain · hpf · opus · jb · vtone · nodelay · mic
//   shot 은 JPEG 이 아니라 화면 그대로(RGB565 320×240)를 base64 로 보낸다: "SHOT RAW565 320 240" → "SHOT BEGIN <바이트>" → "B64:…" 줄들 → "SHOT END <검사합>".
#include <M5Unified.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"

#include "audio_out.h"
#include "device_id.h"
#include "net_wifi.h"
#include "relay_link.h"
#include "sc_app.h"
#include "sc_boot.h"
#include "sc_gfx.h"
#include "sc_store.h"
#include "sc_ui_layout.h"
#include "secrets.h"
#include "tone_out.h"
#include "voice.h"
#include "voice_cmd.h"
#include "vol_steps.h"

static const char *TAG = "sc_phone";

// 소리 장치가 안 열리면 다시 켜 본다. 횟수는 nvs_phone 에 적어 3번까지만 — 끝없는 재부팅 방지(Tab5 phone_main.cpp 의 boot_guard 와 같은 규칙, PL3b-D2).
// 그래도 안 되면 소리 없이 계속 돈다(화면·통화 신호는 된다, STAT audio=0).
static void boot_guard(bool ok, const char *what)
{
    char key[16]; snprintf(key, sizeof key, "rst_%s", what);
    const uint8_t n = scstore::get_u8(key, 0);
    if (!ok && scstore::ready() && n < 3) {
        if (scstore::set_u8(key, (uint8_t)(n + 1))) {
            printf("PHONE %s-failed restart %d\n", what, n + 1);
            fflush(stdout); vTaskDelay(pdMS_TO_TICKS(200));
            esp_restart();
        }
        printf("PHONE FAIL %s (restart count not saved)\n", what);
        return;
    }
    if (ok && n) scstore::set_u8(key, 0);
    if (!ok) printf("PHONE FAIL %s\n", what);
}

// 조용한 모드(PL3b-D14): 스피커를 끈 채로 둔다 — 벨·연결음·목소리가 전부 조용하다. 저장된다(재부팅·스트림덱 왕복 뒤에도 유지). 켜 둔 채 잊으면 벨이 안 울린다(STAT amp=0).
static const char *QUIET_KEY = "quiet";
static bool quiet_get() { return scstore::get_u8(QUIET_KEY, 0) == 1; }
static bool quiet_set(bool on) { return on ? scstore::set_u8(QUIET_KEY, 1) : scstore::erase(QUIET_KEY); }

// 화면 그대로를 시리얼로 내보낸다(받는 쪽 tools/sc_ctl.py 가 크기와 검사합을 확인하고 PNG 로 바꾼다).
static void dump_shot()
{
    const uint8_t *p = nullptr;
    const size_t n = scgfx::snapshot(&p);
    if (!n || !p) { printf("SHOT FAIL no-frame\n"); return; }
    uint32_t sum = 0;
    for (size_t i = 0; i < n; ++i) sum += p[i];
    printf("SHOT RAW565 %d %d\n", sclayout::W, sclayout::H);
    printf("SHOT BEGIN %u\n", (unsigned)n);
    static unsigned char *line = nullptr;                               // 4200바이트 — PSRAM 에 한 번(내부 메모리를 아낀다)
    if (!line) line = (unsigned char *)heap_caps_malloc(4200, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!line) { printf("SHOT FAIL memory\n"); return; }
    for (size_t off = 0; off < n; off += 3072) {
        size_t chunk = n - off < 3072 ? n - off : 3072, olen = 0;
        mbedtls_base64_encode(line, 4200, &olen, p + off, chunk); line[olen] = 0;
        printf("B64:%s\n", (const char *)line);
        vTaskDelay(pdMS_TO_TICKS(2));                                   // 줄마다 양보(Tab5 phone_main.cpp 와 같은 이유 — 쉬지 않고 보내면 IDLE 이 굶는다)
    }
    printf("SHOT END %u\n", (unsigned)sum);
}

static void print_mem()
{
    printf("MEM int_free=%u int_largest=%u int_min=%u psram_free=%u\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

// 태스크별·코어별 CPU 사용률을 secs 초 동안 잰다(관문 실험의 'cpu' 와 같은 방식 — 두 번 찍어 뺀다). 통화 중의 코어 여유를 본다(합격선: 코어당 60% 이하).
static void print_cpu(int secs)
{
    constexpr int MAX_TASKS = 40;
    static TaskStatus_t *a = nullptr, *b = nullptr;
    if (!a) { a = (TaskStatus_t *)heap_caps_malloc(sizeof(TaskStatus_t) * MAX_TASKS * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); b = a ? a + MAX_TASKS : nullptr; }
    if (!a) { printf("CPU FAIL memory\n"); return; }
    configRUN_TIME_COUNTER_TYPE t0 = 0, t1 = 0;
    const UBaseType_t n0 = uxTaskGetSystemState(a, MAX_TASKS, &t0);
    vTaskDelay(pdMS_TO_TICKS(secs * 1000));
    const UBaseType_t n1 = uxTaskGetSystemState(b, MAX_TASKS, &t1);
    const double total = (double)(t1 - t0);
    if (!n0 || !n1 || total <= 0) { printf("CPU FAIL no-data\n"); return; }
    double idle[2] = { 0, 0 };
    for (UBaseType_t i = 0; i < n1; ++i) {
        for (UBaseType_t k = 0; k < n0; ++k) {
            if (a[k].xHandle != b[i].xHandle) continue;
            const double pct = 100.0 * (double)(b[i].ulRunTimeCounter - a[k].ulRunTimeCounter) / total;
            const int core = (int)b[i].xCoreID;
            if (!strncmp(b[i].pcTaskName, "IDLE", 4)) { if (core == 0 || core == 1) idle[core] = pct; }
            else if (pct >= 0.5) printf("CPU task=%s core=%d pct=%.1f\n", b[i].pcTaskName, core > 1 ? -1 : core, pct);
            break;
        }
    }
    printf("CPU DONE secs=%d core0=%.1f core1=%.1f\n", secs, 100.0 - idle[0], 100.0 - idle[1]);
}

static void serial_cmd(const std::string &cmd)
{
    int x = 0, y = 0;
    if (cmd == "stat") scapp::print_stat();
    else if (sscanf(cmd.c_str(), "t %d %d", &x, &y) == 2) {
        if (x < 0 || x >= sclayout::W || y < 0 || y >= sclayout::H) printf("ERR t out-of-range\n");
        else { scapp::inject_tap(x, y); printf("OK t %d %d\n", x, y); }
    }
    else if (cmd == "shot") dump_shot();
    else if (cmd == "reboot") { printf("OK reboot\n"); fflush(stdout); vTaskDelay(pdMS_TO_TICKS(200)); esp_restart(); }
    else if (cmd == "deck") printf(scapp::deck_request() ? "OK deck\n" : "ERR deck\n");    // 가는 것은 app 태스크가 한다("DECK go" → "SCBOOT to-deck", 못 가면 "DECK FAIL <이유>")
    else if (cmd == "mem") print_mem();
    else if (cmd == "cpu") print_cpu(5);
    else if (sscanf(cmd.c_str(), "cpu %d", &x) == 1) print_cpu(x < 1 ? 1 : x > 30 ? 30 : x);
    else if (cmd == "quiet on" || cmd == "quiet off") {
        const bool q = cmd == "quiet on";
        const bool saved = quiet_set(q), set = audioout::amp(!q);
        if (set && !q) voice::set_source(voicecore::Src::AEC);          // PL3b-D22: 스피커가 켜졌다 → 통화 중이던 mic·ref 진단 출처를 aec 로
        printf("%s quiet %s amp=%d\n", saved && set ? "OK" : "ERR", q ? "on" : "off", (int)audioout::amp_is_on());
    }
    else if (voicecmd::handle(cmd)) {}                           // 소리·통화 품질 명령 — Tab5 와 같은 모듈(voice_cmd.cpp)
    else if (const volsteps::Cmd vc = volsteps::parse_cmd(cmd.c_str()); vc.mine) {
        if (!vc.ok) printf("ERR volstep 1-%d\n", volsteps::COUNT);
        else if (!scapp::vol_request((int)vc.req, vc.arg)) printf("ERR volstep busy-or-no-app\n");
    }
    else printf("ERR unknown\n");
    fflush(stdout);
}

static void serial_task(void *)
{
    usb_serial_jtag_driver_config_t jcfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    if (usb_serial_jtag_driver_install(&jcfg) == ESP_OK) usb_serial_jtag_vfs_use_driver();
    printf("PHONE READY\n"); fflush(stdout);
    std::string line;
    while (true) {
        uint8_t b = 0;
        if (usb_serial_jtag_read_bytes(&b, 1, pdMS_TO_TICKS(100)) <= 0) continue;
        if (b == '\r') continue;
        if (b != '\n') { if (line.size() < 64) line += (char)b; continue; }
        std::string cmd; cmd.swap(line);
        if (!cmd.empty()) serial_cmd(cmd);
    }
}

extern "C" void app_main(void)
{
    // 무엇보다 먼저: 지금 도는 앱을 "정상"으로 표시하고(TW-D15), 연달아 죽는 중이면 스트림덱으로 돌아간다(sc_boot.cpp SB-D5 — 여기서 돌아가면 아래는 돌지 않는다).
    scboot::usb_restore();               // USB 단자를 전원 켜질 때의 설정으로(SB-D7 — 스트림덱이 남긴 설정이면 맥에 안 잡힌다). 아래 줄들이 USB 로 나가도록 맨 먼저
    scboot::mark_running_ok();           // "SCBOOT running=ota_0 deck=1"
    scboot::crash_guard();               // "SCBOOT reset=<이유> crashes=<n>"

    auto cfg = M5.config();
    cfg.internal_spk = false;            // 소리는 audio_out 이 esp_codec_dev 로 직접 다룬다 — M5Unified 가 I2S·앰프를 잡지 않게
    cfg.internal_mic = false;
    cfg.internal_imu = false;            // 쓰지 않는 것은 켜지 않는다
    cfg.led_brightness = 0;
    M5.begin(cfg);                       // 화면·터치·전원칩. 서보·LED·PY32(0x6F)에는 아무것도 쓰지 않는다(TW-D6 — 관문 시험 펌웨어와 같은 설정)

    static StaticSemaphore_t i2c_lock_buf;
    SemaphoreHandle_t i2c_lock = xSemaphoreCreateMutexStatic(&i2c_lock_buf);   // 터치(scapp)와 앰프·마이크 칩 제어(audio_out)가 같은 M5.In_I2C 를 쓴다
    audioout::amp_off_at_boot(i2c_lock);                                       // 장치를 열기 전이라 앰프는 꺼져 있다(audio_board_cores3.cpp CB-D3) — 규칙대로 한 번 부른다

    scstore::init();                     // 전화기 전용 설정 칸(nvs_phone)과 플래시 일꾼. 스트림덱의 nvs 는 건드리지 않는다(TW-D16)

    const deviceid::Info &me = deviceid::get();
    printf("PHONE BOOT unit=%d id=%s board=stackchan\n", me.unit, me.id);
    ESP_LOGI(TAG, "mac %s", me.mac);
    if (!me.unit) ESP_LOGE(TAG, "this MAC is not in PHONE_DEVICES (secrets.h) -> relay disabled");

    static_assert(sizeof(RELAY_URL) <= relaylink::URL_BUF, "relay URL does not fit the URL buffer");
    const bool gfx_ok = scgfx::init();   // 하늘색 한 장
    if (!gfx_ok) printf("PHONE FAIL gfx\n");
    xSemaphoreTake(i2c_lock, portMAX_DELAY); M5.Display.setBrightness(140); xSemaphoreGive(i2c_lock);   // 밝기는 전원칩(I2C)으로 건다

    // 소리 장치·에코 제거·Opus 를 Wi-Fi 보다 먼저 연다: 빠른 내부 메모리를 소리 쪽이 먼저 잡는다(관문 실험 조건 4 — 순서를 바꾸면 내부 메모리 여유가 10KB 까지 내려갔다).
    if (gfx_ok) {
        audioout::start(i2c_lock, !quiet_get());
        scapp::vol_apply_saved();        // 저장된 음량 단계를 신호음·목소리보다 먼저 건다(P4V-D7)
        toneout::start();
        voice::start();                  // 에코 제거·Opus·태스크(잠든 채). 실패하면 "AUDIO FAIL voice"
        boot_guard(audioout::ready(), "audio");
    }
    print_mem();

    netwifi::start();
    if (me.unit && gfx_ok) relaylink::start(RELAY_URL, me.id, me.token);   // 화면이 없으면 교환기에 "켜짐"으로 보이지 않게 붙지 않는다(Tab5 M-3)
    else if (me.unit) ESP_LOGE(TAG, "gfx init failed -> relay not started");
    if (gfx_ok) scapp::start(me.unit, i2c_lock);
    if (gfx_ok) scboot::start_watch();   // app 태스크가 20초째 멈추면 다시 켠다(SB-D6). 화면이 없으면 app 태스크도 없으니 감시하지 않는다
    // 시리얼 태스크의 스택도 PSRAM(플래시를 만지는 명령은 scstore 의 일꾼이 한다 — sc_store.h SS-D1)
    if (xTaskCreatePinnedToCoreWithCaps(serial_task, "sercmd", 8 * 1024, nullptr, 3, nullptr, 0, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS)
        xTaskCreatePinnedToCore(serial_task, "sercmd", 8 * 1024, nullptr, 3, nullptr, 0);
}
