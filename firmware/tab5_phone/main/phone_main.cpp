// phone_main.cpp — Tab5 음성 전화기(Phase 3b: 벨·연결음·종료음 + 목소리 통화). 부팅, 태스크 기동, 시리얼 명령만 한다.
//
// 설계: docs/exec-plans/2026-10-03-tab5-phone.md §9.5. 두 대에 같은 펌웨어를 올리고, 기기 번호는 MAC 으로 정한다(device_id).
// 태스크: app(화면·통화 진행, 코어1) / touch(터치 읽기, 코어1) / audio(스피커 출력, 코어1) / v_cap(마이크 → 에코 제거 → 압축, 코어1) / v_rx(해제, 코어1)
//        / v_tx(음성 보내기, 코어0) / wifi(매니저, 코어0) / relay(교환기 연결 관리, 코어0) / relay_ws(부품의 웹소켓 태스크) / sercmd(시리얼 명령, 코어0)
//
// 시리얼 명령(115200, 줄 단위) — tools/phone_ctl.py 가 쓴다:
//   stat | t <x> <y> | shot | reboot
//   진단(사람이 켠다): fault audio | quiet on|off | selftest | vsrc aec|mic|ref|tone | vping     (계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md PL3b-D2·D11·D12·D14)
//   실행 중 조절(저장하지 않는다 — 재부팅하면 기본값. PL3b-D23·D24): txgain [1..4] (보내는 이득, 기본 4) | vol [0..100] (재생 음량, 기본 100). 인자 없이 보내면 지금 값
//   음량 단계(P4V — 화면의 작게/크게와 같은 길, 저장된다. 판단은 vol_steps.h): volup | voldn | volstep [1..5]. 답은 app 이 "VOLSTEP step=<1..5> max=5 vol=<0..100> saved=<0|1>" 한 줄로
//   통화 품질 손잡이(Phase 3c — 계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D3. 저장하지 않는다, 부팅 값은 voice_tune.h. 인자 없이 보내면 지금 값):
//     guard [0..16] 메아리 지킴(P4H-D1: 되먹임 칸 세기의 n÷4 배보다 큰 소리만 키운다, 0 = 끔) |
//     rest [0|6|12|18] 쉼 누르기 | dt [0|6|12] 겹침 누르기 | rxgain [100..300] 받는 목소리의 재생 이득(%) | hpf [0|1] 저음 걷어내기 — P5A(2026-10-08, voice_tune.h) |
//     lvl [0|6..16] 보내는 크기 평탄화 | tilt [0..2] 고음 올리기 | res [0|6|12|18] 남은 메아리 누르기 | mic [12..24 3dB 단위] 마이크 증폭 | opus [12..40] 비트율(kbps)
//     | jb [0|3..10] 지터 버퍼 규칙 | nodelay [0|1] TCP_NODELAY | vtone [300..7000] 시험음 주파수 | vsrc probe (에코 제거를 거치는 시험음) | tune (손잡이·측정값 한 줄)
#include <M5Unified.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "app.h"
#include "audio_out.h"
#include "device_id.h"
#include "gfx.h"
#include "net_wifi.h"
#include "relay_link.h"
#include "secrets.h"
#include "tone_out.h"
#include "voice.h"
#include "voice_cmd.h"
#include "vol_steps.h"

static const char *TAG = "phone";

#define DISPLAY_ROTATION 3              // 삼각대 나사가 화면 아래(정상 가로). 터치 좌표도 이 방향 기준

// 화면이 안 잡히면(소프트 리셋 뒤 패널 감지가 가끔 실패한다) 재시작한다. 횟수는 NVS 에 적어 3번까지만 — 무한 재부팅 방지(§9.7).
// what 마다 횟수를 따로 센다(키 "rst_<what>") — 한 검사가 통과하며 다른 검사의 횟수를 지우면 끝없이 재시작한다.
static void boot_guard(bool ok, const char *what)
{
    char key[16]; snprintf(key, sizeof key, "rst_%s", what);
    nvs_handle_t h; uint8_t n = 0;
    const bool nvs_ok = nvs_open("phone", NVS_READWRITE, &h) == ESP_OK;
    if (nvs_ok) nvs_get_u8(h, key, &n);
    if (!ok && nvs_ok && n < 3) {
        const bool counted = nvs_set_u8(h, key, (uint8_t)(n + 1)) == ESP_OK && nvs_commit(h) == ESP_OK;
        nvs_close(h);
        if (counted) {                                           // 횟수를 적지 못했으면 재시작하지 않는다(세지 못하면 끝없이 재시작한다)
            printf("PHONE %s-failed restart %d\n", what, n + 1);
            fflush(stdout); vTaskDelay(pdMS_TO_TICKS(200));      // PL3b-D2: 이 줄이 시리얼로 나간 뒤에 재시작한다('reboot' 명령과 같은 방식 — 기기 시험이 이 줄을 센다)
            esp_restart();
        }
        printf("PHONE FAIL %s (restart count not saved)\n", what);
        return;
    }
    if (nvs_ok) { if (ok && n) { nvs_set_u8(h, key, 0); nvs_commit(h); } nvs_close(h); }
    if (!ok) printf("PHONE FAIL %s\n", what);
}

// PL3b-D2 소리 장치 실패를 일부러 내 보는 스위치(3a 최종 검수 M-3: 실패 경로가 기기에서 한 번도 돈 적이 없었다).
//   시리얼 'fault audio' → NVS 에 표시 → 다음 부팅부터 소리 장치를 열지 않고 실패로 친다 → boot_guard 가 3번 재시작 →
//   네 번째 부팅은 소리 없이 계속 돈다(STAT audio=0) → 그때 표시를 지운다(다음 재부팅은 정상). 사람이 켠 것만, 한 번의 시험만.
static const char *FAULT_KEY = "f_audio";
static bool fault_audio_armed()
{
    nvs_handle_t h; uint8_t v = 0;
    if (nvs_open("phone", NVS_READONLY, &h) != ESP_OK) return false;
    nvs_get_u8(h, FAULT_KEY, &v);
    nvs_close(h);
    return v == 1;
}
static bool fault_audio_set(bool on)
{
    nvs_handle_t h;
    if (nvs_open("phone", NVS_READWRITE, &h) != ESP_OK) return false;
    const bool ok = (on ? nvs_set_u8(h, FAULT_KEY, 1) : nvs_erase_key(h, FAULT_KEY)) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}

// PL3b-D14 조용한 모드: 스피커 앰프를 끈 채로 둔다(시리얼 'quiet on' — NVS 에 남아 재부팅 뒤에도 유지, 'quiet off' 로 푼다).
//   벨·연결음·목소리가 전부 조용하다. 코덱은 그대로 돌아 마이크 칩의 되먹임 칸에는 소리가 잡힌다 → 밤이나 사용자 없이도 소리 경로를 기록으로 확인한다.
//   사람이 켠 것만. 켜 둔 채 잊으면 벨이 안 울린다 — STAT 의 amp=0 과 부팅 줄 "AUDIO ready … amp=0" 으로 보인다.
static const char *QUIET_KEY = "quiet";
static bool quiet_get()
{
    nvs_handle_t h; uint8_t v = 0;
    if (nvs_open("phone", NVS_READONLY, &h) != ESP_OK) return false;
    nvs_get_u8(h, QUIET_KEY, &v);
    nvs_close(h);
    return v == 1;
}
static bool quiet_set(bool on)
{
    nvs_handle_t h;
    if (nvs_open("phone", NVS_READWRITE, &h) != ESP_OK) return false;
    const esp_err_t e = on ? nvs_set_u8(h, QUIET_KEY, 1) : nvs_erase_key(h, QUIET_KEY);
    const bool ok = (e == ESP_OK || (!on && e == ESP_ERR_NVS_NOT_FOUND)) && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}

// 화면을 JPEG 로 찍어 시리얼로 내보낸다. 받는 쪽(phone_ctl.py)이 크기와 검사합을 확인한다.
static void dump_shot()
{
    const uint8_t *p = nullptr;
    bool priv = false;
    const size_t n = gfx::screenshot_jpeg(&p, &priv);
    if (priv) { printf("SHOT FAIL private\n"); return; }   // I-1: 비밀번호가 든 자판 화면 — 도구가 이 줄이면 다시 받지 않는다
    if (!n || !p) { printf("SHOT FAIL encode\n"); return; }
    uint32_t sum = 0;
    for (size_t i = 0; i < n; ++i) sum += p[i];
    printf("SHOT BEGIN %u\n", (unsigned)n);
    static unsigned char line[4200];
    for (size_t off = 0; off < n; off += 3072) {
        size_t chunk = n - off < 3072 ? n - off : 3072, olen = 0;
        mbedtls_base64_encode(line, sizeof line, &olen, p + off, chunk); line[olen] = 0;
        printf("B64:%s\n", (const char *)line);
        vTaskDelay(pdMS_TO_TICKS(2));   // 줄마다 양보 — 쉬지 않고 보내면 CPU0 IDLE 이 굶어 감시 경고가 줄 한가운데 끼어든다(Phase 0 에서 겪음)
    }
    printf("SHOT END %u\n", (unsigned)sum);
}

static void serial_cmd(const std::string &cmd)
{
    int x = 0, y = 0;
    if (cmd == "stat") app::print_stat();
    else if (sscanf(cmd.c_str(), "t %d %d", &x, &y) == 2) {
        if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) printf("ERR t out-of-range\n");
        else { app::inject_tap(x, y); printf("OK t %d %d\n", x, y); }
    }
    else if (cmd == "shot") dump_shot();
    else if (cmd == "reboot") { printf("OK reboot\n"); fflush(stdout); vTaskDelay(pdMS_TO_TICKS(200)); esp_restart(); }
    else if (cmd == "fault audio") printf(fault_audio_set(true) ? "OK fault audio\n" : "ERR fault audio\n");   // 다음 부팅부터(PL3b-D2). 'reboot' 로 시작한다
    else if (cmd == "quiet on" || cmd == "quiet off") {         // PL3b-D14: 저장하고 앰프를 그 자리에서 끄거나 켠다
        const bool q = cmd == "quiet on";
        const bool saved = quiet_set(q), set = audioout::amp(!q);
        if (set && !q) voice::set_source(voicecore::Src::AEC);  // PL3b-D22: 스피커가 켜졌다 → 통화 중이던 mic·ref 진단 출처를 aec 로 되돌린다(캡처 태스크도 묶음마다 막는다)
        printf("%s quiet %s amp=%d\n", saved && set ? "OK" : "ERR", q ? "on" : "off", (int)audioout::amp_is_on());
    }
    else if (voicecmd::handle(cmd)) {}                           // 소리·통화 품질 명령(selftest·vsrc·txgain·vol·vping·tune·손잡이들) — voice_cmd.cpp. 스택짱 전화기와 함께 쓴다
    else if (const volsteps::Cmd vc = volsteps::parse_cmd(cmd.c_str()); vc.mine) {   // P4V-D3: volup | voldn | volstep [1..5] — 화면의 작게/크게와 같은 대기열로. 답("VOLSTEP …")은 app 태스크가 찍는다
        if (!vc.ok) printf("ERR volstep 1-%d\n", volsteps::COUNT);
        else if (!app::vol_request((int)vc.req, vc.arg)) printf("ERR volstep busy-or-no-app\n");
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
    auto cfg = M5.config();
    cfg.internal_spk = false;           // 소리는 audio_out 이 esp_codec_dev 로 직접 다룬다(PH-D4) — M5Unified 가 I2S0·코덱을 잡지 않게
    cfg.internal_mic = false;
    M5.begin(cfg);

    // PL3b-D21 M5GFX 가 M5.begin 안에서(화면 자동 감지) 스피커 앰프를 켜 둔다(M5GFX.cpp:3137·3146). 소리 장치를 열기까지 기다리지 않고 바로 끈다 —
    //   I2C 잠금은 여기서 한 번 만든다(정적 — 만들기가 실패하지 않는다). 터치(app)와 코덱·앰프(audio_out)가 같은 버스를 쓴다(PL3a-D14: 전에는 app::start 안에서 만들어
    //   화면이 실패해 app 이 없으면 잠금도 없었다). 터치 태스크는 아직 없지만 규칙대로 잠금 안에서 쓴다. 실패해도 부팅은 계속한다(audioout::start 가 같은 쓰기를 다시 한다).
    static StaticSemaphore_t i2c_lock_buf;
    SemaphoreHandle_t i2c_lock = xSemaphoreCreateMutexStatic(&i2c_lock_buf);
    audioout::amp_off_at_boot(i2c_lock);

    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {     // 다른 펌웨어가 쓰던 자리(1번 기기) — 지우고 다시
        ESP_LOGW(TAG, "nvs unusable (%s) -> erase. saved wifi list is lost", esp_err_to_name(e));
        nvs_flash_erase();
        e = nvs_flash_init();
    }
    if (e != ESP_OK) ESP_LOGE(TAG, "nvs init failed: %s", esp_err_to_name(e));

    boot_guard(M5.Display.width() != 0, "panel");
    M5.Display.setRotation(DISPLAY_ROTATION);
    M5.Display.setBrightness(160);

    const deviceid::Info &me = deviceid::get();
    printf("PHONE BOOT unit=%d id=%s\n", me.unit, me.id);
    ESP_LOGI(TAG, "mac %s", me.mac);
    if (!me.unit) ESP_LOGE(TAG, "this MAC is not in PHONE_DEVICES (secrets.h) -> relay disabled");

    static_assert(sizeof(RELAY_URL) <= relaylink::URL_BUF, "relay URL does not fit the URL buffer");   // M-9: 잘린 주소로 조용히 붙는 것을 빌드에서 막는다(문구에 주소 값을 싣지 않는다)
    const bool gfx_ok = gfx::init();
    boot_guard(gfx_ok, "gfx");

    // PL3b-D2 소리 장치는 Wi-Fi·교환기보다 먼저 연다(Phase 0 시험 펌웨어와 같은 순서 — 다시 켜 보는 재시작이 망을 건드리기 전에 끝난다).
    //   실패하면 "AUDIO FAIL <단계>" → boot_guard 가 3번까지 재시작 → 그래도 안 되면 "PHONE FAIL audio" 를 찍고 소리 없이 계속 돈다(STAT audio=0).
    //   통화 설계서 §9.7 "소리 초기화 실패 → 재시작 가드"(화면 표시는 Phase 5). 3a 의 PL3a-D10(재시작 없음)을 대신한다.
    if (gfx_ok) {
        const bool injected = fault_audio_armed();
        audioout::start(i2c_lock, !quiet_get(), injected);      // PL3b-D14: 조용한 모드면 앰프를 켜지 않는다. PL3b-D21: 주입도 잠금을 가진 채 실패한다 → 앰프가 꺼진 채 남는다
        app::vol_apply_saved();                                 // P4V-D7: 저장된 음량 단계를 여기서 바로 건다(코덱은 100 으로 열린다) — 신호음·목소리·Wi-Fi·교환기를 시작하기 전
        toneout::start();
        voice::start();                                         // 에코 제거·Opus·태스크 셋(잠든 채). 실패하면 "AUDIO FAIL voice"
        boot_guard(audioout::ready(), "audio");
        if (injected) fault_audio_set(false);   // 여기까지 왔으면 세 번 다시 켜 보고 포기한 것이다 — 시험은 한 번으로 끝. 다음 부팅은 정상
    }

    netwifi::start();
    if (me.unit && gfx_ok) relaylink::start(RELAY_URL, me.id, me.token);   // M-3: 화면이 없으면(app 태스크 없음) 교환기에 "켜짐"으로 보이지 않게 붙지 않는다
    else if (me.unit) ESP_LOGE(TAG, "gfx init failed -> relay not started");
    if (gfx_ok) app::start(me.unit, i2c_lock);
    xTaskCreatePinnedToCore(serial_task, "sercmd", 8 * 1024, nullptr, 3, nullptr, 0);
}
