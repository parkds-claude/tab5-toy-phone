// audio_board_cores3.cpp — 스택짱(M5Stack CoreS3 기반)의 소리 기판 부분. 인터페이스는 ../../tab5_phone/main/audio_board.h.
// 근거: 이 기기에서 실제로 돌려 본 관문 시험 펌웨어 firmware/stackchan_phone_probe/main/board_audio.cpp (2026-10-08 — 소리 남, 오류 0, 메아리 감소 32~43dB).
//   그대로 가져온 것: 핀(MCLK0/BCLK34/WS33/DOUT13/DIN14)·주소(AW88298 0x36)·앰프 리셋(확장칩 AW9523 0x58 레지스터 0x02 의 2번 비트)·
//   AW88298 드라이버 설정(reset_pin -1, 5.0V/3.3V, pa_gain 1 — xiaozhi 의 CoreS3 값)·수신 4칸일 때 앰프 I2S 설정(레지스터 0x06 의 [5:4] = 10, 프레임당 BCLK 64).
//   시험 펌웨어와 다른 점:
//   CB-D1 음량을 드라이버(esp_codec_dev_set_out_vol)로 걸지 않고 앰프 레지스터 0x0C 에 직접 쓴다. 이유: 드라이버는 윗바이트에 0.5dB 단위 값을 그대로 쓰는데
//         실측한 크기는 "윗 4비트 = 6dB 단위, 아랫 4비트 = 0.5dB 단위(0~11)"와 맞았다(음량 80 이 90 보다 컸다 — 관문 실험 진행 로그 17:08).
//         비용: 드라이버가 기억하는 음량 값과 실제가 다르다(드라이버의 음량 함수를 부르지 않으므로 문제없다). 탈출구: 드라이버가 고쳐지면 volume_set 을 한 줄로.
//   CB-D2 100% = 앰프 −6dB (TW-D13). 이유: 0dB 에서는 에코 제거 뒤 남는 메아리가 방 잡음보다 13dB 위였고 −6dB·−12dB 에서는 잡음에 묻혔다. 사용자는 −12dB 를 "잘 들림".
//         1% = 0.3dB 로 낮춘다(Tab5 는 0.5dB): 음량 단계표(vol_steps.h 52·62·72·82·100)가 −20.4·−17.4·−14.4·−11.4·−6dB 가 된다 — 작은 스피커라 범위를 좁혔다.
//         비용: 맨 위 단계가 낼 수 있는 가장 큰 소리가 아니다 · 단계 간격이 Tab5(5dB)와 다르다. 탈출구: TOP_HALF_DB·PCT_TENTH_DB 두 값.
//   CB-D3 스피커 켜고 끄기 = 앰프의 무음 비트(esp_codec_dev_set_out_mute). 장치를 열기 전(out == nullptr)에는 할 일이 없다 — 앰프가 아직 꺼져 있다(PWDN).
#include "audio_board.h"

#include <M5Unified.h>

#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "freertos/task.h"

namespace audioboard {
namespace {

const char *TAG = "audio_board";
constexpr uint8_t AW88298_ADDR = 0x36, AW9523_ADDR = 0x58;
constexpr int AW_REG_ID = 0x00, AW_REG_I2SCTRL = 0x06, AW_REG_VOL = 0x0C;
constexpr int TOP_HALF_DB = 12;             // CB-D2: 100% 일 때의 감쇠(0.5dB 단위) = −6dB
constexpr int PCT_TENTH_DB = 3;             // CB-D2: 1% = 0.3dB

}  // namespace

Pins pins() { return Pins{ GPIO_NUM_0, GPIO_NUM_34, GPIO_NUM_33, GPIO_NUM_13, GPIO_NUM_14 }; }
uint8_t speaker_addr() { return AW88298_ADDR; }
const char *name() { return "cores3"; }

bool answer(SemaphoreHandle_t lock)
{
    uint8_t id[2] = {0, 0}, b = 0;
    xSemaphoreTake(lock, portMAX_DELAY);
    // 앰프 리셋 풀기. M5GFX 가 부팅 때 이미 올려 두지만(P0 = 0b111) 소리를 M5Unified 에 맡기지 않으므로 여기서 한 번 더 확실히 한다.
    const bool rst = M5.In_I2C.bitOn(AW9523_ADDR, 0x02, 0b00000100, I2C_FREQ);
    xSemaphoreGive(lock);
    if (!rst) { ESP_LOGE(TAG, "amp reset release failed (AW9523 0x58)"); return false; }
    vTaskDelay(pdMS_TO_TICKS(50));
    xSemaphoreTake(lock, portMAX_DELAY);
    const bool ok1 = M5.In_I2C.readRegister(AW88298_ADDR, AW_REG_ID, id, 2, I2C_FREQ);      // 칩 ID (0x1852)
    const bool ok2 = M5.In_I2C.readRegister(MIC_CHIP_ADDR, 0x40, &b, 1, I2C_FREQ);
    xSemaphoreGive(lock);
    return ok1 && ok2;
}

const audio_codec_if_t *new_speaker_codec(const audio_codec_ctrl_if_t *ctrl, const audio_codec_gpio_if_t *gpio)
{
    aw88298_codec_cfg_t cfg = {};
    cfg.ctrl_if = ctrl; cfg.gpio_if = gpio;
    cfg.reset_pin = -1;                         // 리셋은 확장칩으로 직접 풀었다(answer)
    cfg.hw_gain.pa_voltage = 5.0; cfg.hw_gain.codec_dac_voltage = 3.3; cfg.hw_gain.pa_gain = 1;
    return aw88298_codec_new(&cfg);
}

bool after_open(esp_codec_dev_handle_t out)
{
    // 수신 4칸(16비트 × 4 = 프레임당 BCLK 64)이면 같은 포트의 송신도 프레임당 64 로 나간다(관문 실험 부팅 로그: "STD: TX data 16, slot 32").
    // 드라이버는 16비트 재생이면 앰프를 프레임당 32 로 맞춰 두므로 64 로 고친다([5:4] = 10). 자료 폭([7:6] = 00, 16비트)은 그대로.
    int r = 0;
    if (esp_codec_dev_read_reg(out, AW_REG_I2SCTRL, &r) != ESP_CODEC_DEV_OK) return false;
    const int want = (r & ~0x30) | 0x20;
    if (esp_codec_dev_write_reg(out, AW_REG_I2SCTRL, want) != ESP_CODEC_DEV_OK) return false;
    int back = 0;
    if (esp_codec_dev_read_reg(out, AW_REG_I2SCTRL, &back) != ESP_CODEC_DEV_OK || back != want) { ESP_LOGE(TAG, "amp i2s setting not applied: 0x%04x (want 0x%04x)", back, want); return false; }
    ESP_LOGI(TAG, "aw88298 reg06=0x%04x (bclk 64 per frame)", back);
    return true;
}

bool amp_set(SemaphoreHandle_t, esp_codec_dev_handle_t out, bool on)
{
    if (!out) return !on;                       // CB-D3: 열기 전에는 앰프가 꺼져 있다 — "끄기"는 이미 된 것, "켜기"는 못 한다
    return esp_codec_dev_set_out_mute(out, !on) == ESP_CODEC_DEV_OK;
}

// CB-D1·D2: % → 감쇠(0.5dB 단위) → 레지스터 0x0C 의 윗바이트(윗 4비트 = 6dB 단위, 아랫 4비트 = 0.5dB 단위 0~11). 0% 는 가장 작게(−96dB 근처).
int volume_code(int percent)
{
    if (percent <= 0) return 0xFB;              // 15 × 6dB + 11 × 0.5dB = −95.5dB
    if (percent > 100) percent = 100;
    const int half_db = TOP_HALF_DB + ((100 - percent) * PCT_TENTH_DB + 2) / 5;   // 0.1dB → 0.5dB 단위로 반올림
    const int hi = half_db / 12, lo = half_db % 12;
    return hi > 15 ? 0xFB : (hi << 4) | lo;
}

bool volume_set(esp_codec_dev_handle_t out, int percent)
{
    if (!out) return false;
    return esp_codec_dev_write_reg(out, AW_REG_VOL, (volume_code(percent) << 8) | 0x64) == ESP_CODEC_DEV_OK;   // 아랫바이트 0x64 는 드라이버·M5Unified 가 쓰는 값 그대로
}

}  // namespace audioboard
