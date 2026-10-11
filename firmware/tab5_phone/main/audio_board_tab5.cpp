// audio_board_tab5.cpp — Tab5 의 소리 기판 부분. 인터페이스는 audio_board.h.
// audio_out.cpp 에 있던 Tab5 전용 코드를 그대로 옮긴 것이다(2026-10-08 — 스택짱이 audio_out.cpp 를 함께 쓰게 되어 기판별 부분을 떼었다. 동작은 한 줄도 바꾸지 않았다).
//   근거·결정은 audio_out.cpp 의 머리말 그대로: 핀(MCLK30/BCLK27/WS29/DOUT26/DIN28) · ES8388(0x10, DAC, 따라가는 쪽, pa -1, 5.0V/3.3V) ·
//   앰프 = 확장칩(PI4IOE5V6408 0x43)의 P1(출력 레지스터 0x05 의 비트1 — 읽고-고쳐-쓰기를 I2C 잠금 안에서, PL3b-D21) · 음량은 esp_codec_dev 의 곡선 그대로(PL3b-D24, vol_steps.h codec_reg).
#include "audio_board.h"

#include <M5Unified.h>

#include "esp_codec_dev_defaults.h"

namespace audioboard {
namespace {

constexpr uint8_t ES8388_ADDR = 0x10, PI4IO1_ADDR = 0x43;

}  // namespace

Pins pins() { return Pins{ GPIO_NUM_30, GPIO_NUM_27, GPIO_NUM_29, GPIO_NUM_26, GPIO_NUM_28 }; }
uint8_t speaker_addr() { return ES8388_ADDR; }
const char *name() { return "tab5"; }

bool answer(SemaphoreHandle_t lock)             // 두 코덱이 I2C 에 대답하나(레지스터 하나씩 읽기 — 시험 펌웨어 health_check 그대로)
{
    uint8_t a = 0, b = 0;
    xSemaphoreTake(lock, portMAX_DELAY);
    const bool ok1 = M5.In_I2C.readRegister(ES8388_ADDR, 0x19, &a, 1, I2C_FREQ);
    const bool ok2 = M5.In_I2C.readRegister(MIC_CHIP_ADDR, 0x40, &b, 1, I2C_FREQ);
    xSemaphoreGive(lock);
    return ok1 && ok2;
}

const audio_codec_if_t *new_speaker_codec(const audio_codec_ctrl_if_t *ctrl, const audio_codec_gpio_if_t *gpio)
{
    es8388_codec_cfg_t out_cfg = {};
    out_cfg.ctrl_if = ctrl; out_cfg.gpio_if = gpio;
    out_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
    out_cfg.master_mode = false;                // 클럭은 P4 가 낸다(칩은 따라가는 쪽)
    out_cfg.pa_pin = -1;                        // 앰프는 확장칩으로 직접 켠다
    out_cfg.hw_gain.pa_voltage = 5.0; out_cfg.hw_gain.codec_dac_voltage = 3.3;
    return es8388_codec_new(&out_cfg);
}

bool after_open(esp_codec_dev_handle_t) { return true; }   // Tab5 는 연 뒤에 더 할 일이 없다

// 스피커 앰프 전원(확장칩 0x43 의 P1, 출력 레지스터 0x05 의 비트1). 읽고-고쳐-쓰기(트랜잭션 둘)를 I2C 잠금 안에서. out 은 쓰지 않는다(코덱을 열기 전에도 끌 수 있다 — PL3b-D21).
bool amp_set(SemaphoreHandle_t lock, esp_codec_dev_handle_t, bool on)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    const bool ok = on ? M5.In_I2C.bitOn(PI4IO1_ADDR, 0x05, 0b00000010, I2C_FREQ)
                       : M5.In_I2C.bitOff(PI4IO1_ADDR, 0x05, 0b00000010, I2C_FREQ);
    xSemaphoreGive(lock);
    return ok;
}

bool volume_set(esp_codec_dev_handle_t out, int percent) { return esp_codec_dev_set_out_vol(out, percent) == ESP_CODEC_DEV_OK; }

}  // namespace audioboard
