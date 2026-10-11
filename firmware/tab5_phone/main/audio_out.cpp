// audio_out.cpp — 소리 장치(스피커 출력 + 마이크 입력). 설명은 audio_out.h.
//
// 근거(줄 단위 대조, 사용자 지시 2026-10-03): 이 기기에서 실제로 마이크·스피커를 함께 돌린 Phase 0 시험 펌웨어
//   firmware/tab5_phone_probe/main/board_audio.cpp 와 그 원본 xiaozhi-esp32 main/boards/m5stack/tab5/tab5_audio_codec.cc (MIT).
//   시험 펌웨어 그대로인 것: 핀(MCLK30/BCLK27/WS29/DOUT26/DIN28)·주소(ES8388 0x10, ES7210 0x40, 앰프 = 확장칩 0x43 의 P1)·
//   코덱 제어 인터페이스(M5.In_I2C + 잠금)·I2S 를 송수신 한 번에 만드는 것(i2s_setup: 송신 표준 I2S, 수신 4칸 TDM)·ES8388 설정(DAC,
//   따라가는 쪽, pa -1, 5.0V/3.3V)·ES7210 설정(MIC1~4)·여는 순서(출력 → 입력 → 마이크 증폭 → 음량 → 앰프)·여는 형식(출력 16비트 1채널,
//   입력 16비트 4채널 mask 0x0F, 16kHz)·증폭을 거는 호출(esp_codec_dev_set_in_channel_gain)·읽기/쓰기 호출.
//   Phase 3b: 3a 의 "송신 채널만"(PL3a-D7 — 이 기기에서 소리를 낸 적이 없던 조합)을 Phase 0 에서 실제로 소리가 났던 위 설정으로 바꿨다.
//   시험 펌웨어와 다른 점(전부 계획서 "확인하지 못한 것"에 있다):
//   PL3a-D8 DMA 한 칸을 256표본으로 적는다(시험 펌웨어는 기본 240 → 송신은 드라이버가 64바이트 맞춤으로 256 으로 바꾸며 경고, task4.log:177·185.
//          수신 4칸은 240 그대로였다). 쓰기·읽기 단위(BLOCK)가 DMA 한 칸과 같다 = 16ms 박자가 칸과 맞는다. 수신 한 칸 = 4칸 × 2바이트 × 256 = 2048 B.
//   PL3b-D4 DMA 칸 수 6 → 3. 출력은 늘 (칸 수 − 1) × 16ms 앞서 쓰여 있다(i2s_common.c: 빈 칸 대기열이 칸 수 − 1) → 6칸이면 80ms 가
//          그대로 통화 지연이 된다. 3칸이면 32ms. 비용: 출력 태스크가 48ms 넘게 늦으면 스피커가 끊긴다(6칸이면 96ms) — 늦은 횟수를 센다(aud_late).
//          마이크 쪽도 3칸이라 캡처 태스크가 48ms 넘게 못 읽으면 그만큼 잃는다. 탈출구: DMA_DESC 를 4(48ms)나 6으로.
//   PL3b-D10 마이크 증폭 24dB(시험 펌웨어의 기본은 30dB, 24dB 는 gain 명령으로 재 봤다 — 음량 100 에서 에코 포화 0%). 되먹임 칸(MIC3)은 0dB 를
//          명시해서 건다. 건 뒤에 칩의 증폭 레지스터를 되읽어 확인한다 — esp_codec_dev_set_in_channel_gain 은 쓰기가 실패해도 OK 를
//          돌려주기 때문이다(esp_codec_dev.c:477-493). 이 줄이 빠지면 마이크가 24dB 작게 잡힌다(Phase 0 에서 겪은 것).
//   PL3a-D2 음량 100(0dB) 고정 — 크기는 공급자가 디지털로 정한다(tones.h, voice_core.h). 시험 펌웨어는 50 으로 열고 명령으로 바꿨다.
//   PL3a-D4 앰프는 시작 때 한 번 켠다. 코덱을 연 뒤 30ms 쉬고 켠다(코덱 출력이 자리 잡은 뒤 — 켤 때 퍽 소리를 줄이려는 것, 효과는 귀로만 확인).
//          Phase 3b: start 의 amp_on 이 거짓이면 켜지 않고(조용한 시험 — PL3b-D14), amp() 로 켜고 끌 수 있다.
//   AB-D1 (2026-10-08, 전화기 3대 — 설계서 2026-10-08-phone-3way-design.md TW-D11): 이 파일을 스택짱(CoreS3) 전화기도 함께 쓴다. 기판마다 다른 것 —
//          핀 · 스피커 쪽 칩(주소·대답 확인·코덱 객체) · 앰프 켜고 끄기 · 재생 음량 거는 법 — 은 audio_board.h 의 함수 뒤로 옮겼다(Tab5 = audio_board_tab5.cpp,
//          옮긴 코드는 글자 그대로다). 여기 남은 것은 두 기판이 같은 것: 코덱 제어 인터페이스, I2S 송수신(송신 표준·수신 4칸 TDM), 마이크 칩(ES7210)과 그 증폭,
//          여는 순서, 출력 태스크, 통계. 위 머리말의 "핀·주소·ES8388 설정·앰프" 설명은 Tab5 의 것이다.
#include "audio_out.h"

#include <M5Unified.h>
#include <atomic>
#include <cstdio>
#include <cstring>

#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"

#include "audio_board.h"
#include "pcm.h"
#include "voice_tune.h"

namespace audioout {
namespace {

const char *TAG = "audio_out";

constexpr uint8_t ES7210_ADDR = audioboard::MIC_CHIP_ADDR;   // 마이크 칩은 두 기판이 같다. 스피커 쪽 칩·핀·앰프는 audio_board.h(AB-D1)
constexpr uint32_t I2C_FREQ = audioboard::I2C_FREQ;
constexpr int VOLUME = voicetune::DEFAULT_VOL;  // PL3a-D2. Phase 3c: 부팅 값은 voice_tune.h 한 곳에 둔다(PL3c-D3) — 지금 100
constexpr int DMA_DESC = 3;                     // PL3b-D4: DMA 칸 수. 출력은 (3 − 1) × 16ms = 32ms 앞서 쓰여 있다
constexpr int64_t LATE_US = (int64_t)DMA_DESC * BLOCK * 1000000 / RATE;   // 48ms: 앞 묶음 뒤 이만큼 지나서야 쓰면 그 사이 DMA 가 바닥났다
constexpr int MIC_GAIN_DB = voicetune::BOOT_MIC_DB;      // PL3b-D10: 마이크(MIC1 = 칸0)의 증폭. 3dB 단위, 30 이하. 부팅 값(voice_tune.h — Tab5 24, 스택짱 12), 실행 중에는 set_mic_gain(PL3c-D9)
constexpr uint8_t ES7210_MIC1_GAIN_REG = 0x43, ES7210_MIC3_GAIN_REG = 0x45;   // 아래 4비트 = 증폭 ÷ 3dB (es7210.c: _es7210_set_channel_gain)
static_assert(MIC_GAIN_DB % 3 == 0 && MIC_GAIN_DB >= 0 && MIC_GAIN_DB <= 30, "ES7210 gain steps are 3 dB up to 30 dB");
constexpr int MAX_SOURCES = 2;                  // 신호음 + 목소리
// PL3a-D9 출력 태스크: 코어 1(화면·터치와 같은 코어, Wi-Fi·교환기 연결은 코어 0), 우선순위 6(app 4·touch 5 보다 위 — 화면을 내보내는
//   60~80ms 동안에도 16ms 박자를 지킨다. 한 묶음 계산은 1ms 에 못 미쳐 화면을 늦추지 않는다 — 어림이고 실제 값은 STAT aud_us), 스택 6KB(printf 한 줄 + 코덱 쓰기. STAT stk_audio 로 확인).
constexpr int TASK_CORE = 1, TASK_PRIO = 6, TASK_STACK = 6 * 1024;

SemaphoreHandle_t s_lock = nullptr;
i2s_chan_handle_t s_tx = nullptr, s_rx = nullptr;
esp_codec_dev_handle_t s_out = nullptr, s_in = nullptr;
TaskHandle_t s_task = nullptr;
std::atomic<bool> s_ready{false};
std::atomic<source_fn> s_src[MAX_SOURCES];
std::atomic<bool> s_failed{false}, s_amp{false};
std::atomic<uint32_t> s_max_us{0}, s_avg_us{0}, s_wr_err{0}, s_rd_err{0}, s_i2c_err{0}, s_late{0}, s_gap_max{0};
std::atomic<int> s_vol{VOLUME};                  // 코덱 재생 음량(%): 시작 때 VOLUME, 실행 중 vol 명령(PL3b-D24)이 바꾼다. 쓰기가 성공한 값만 담는다
std::atomic<int> s_mic_db{MIC_GAIN_DB};          // 마이크 증폭(dB): 시작 때 MIC_GAIN_DB, 실행 중 mic 명령(PL3c-D9)이 바꾼다. 칩에서 되읽어 확인한 값만 담는다
int16_t s_mix[BLOCK], s_tmp[BLOCK];             // 출력 태스크에서만 쓴다
uint64_t s_sum_us = 0;                          // 출력 태스크에서만 쓴다. 3a 최종 검수 M-5: 32비트 합계는 소리 낸 누적 63시간에 넘친다(통화는 계속 소리를 낸다)
uint32_t s_blocks = 0;
pcm::GapWatch s_watch;                          // 출력 태스크에서만 쓴다(PL3b-D4)

// ── 코덱 제어 인터페이스: M5.In_I2C 로 레지스터 읽기·쓰기(시험 펌웨어 board_audio.cpp 그대로) ──────────────────
struct m5_ctrl_t { audio_codec_ctrl_if_t base; uint8_t addr; bool opened; };

int ctrl_open(const audio_codec_ctrl_if_t *c, void *, int) { ((m5_ctrl_t *)c)->opened = true; return ESP_CODEC_DEV_OK; }
bool ctrl_is_open(const audio_codec_ctrl_if_t *c) { return ((const m5_ctrl_t *)c)->opened; }
int ctrl_close(const audio_codec_ctrl_if_t *c) { ((m5_ctrl_t *)c)->opened = false; return ESP_CODEC_DEV_OK; }
int ctrl_read(const audio_codec_ctrl_if_t *c, int reg, int reg_len, void *data, int data_len)
{
    auto *m = (const m5_ctrl_t *)c;
    if (reg_len != 1 || !data || data_len <= 0) return ESP_CODEC_DEV_INVALID_ARG;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const bool ok = M5.In_I2C.readRegister(m->addr, (uint8_t)reg, (uint8_t *)data, (size_t)data_len, I2C_FREQ);
    xSemaphoreGive(s_lock);
    if (!ok) { ++s_i2c_err; return ESP_CODEC_DEV_READ_FAIL; }
    return ESP_CODEC_DEV_OK;
}
int ctrl_write(const audio_codec_ctrl_if_t *c, int reg, int reg_len, void *data, int data_len)
{
    auto *m = (const m5_ctrl_t *)c;
    if (reg_len != 1 || !data || data_len <= 0) return ESP_CODEC_DEV_INVALID_ARG;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const bool ok = M5.In_I2C.writeRegister(m->addr, (uint8_t)reg, (const uint8_t *)data, (size_t)data_len, I2C_FREQ);
    xSemaphoreGive(s_lock);
    if (!ok) { ++s_i2c_err; return ESP_CODEC_DEV_WRITE_FAIL; }
    return ESP_CODEC_DEV_OK;
}
int ctrl_info(const audio_codec_ctrl_if_t *c, audio_codec_ctrl_info_t *info)
{
    if (!info) return ESP_CODEC_DEV_INVALID_ARG;
    info->type = AUDIO_CODEC_CTRL_I2C; info->i2c.addr = ((const m5_ctrl_t *)c)->addr; info->i2c.port = 1;
    return ESP_CODEC_DEV_OK;
}
m5_ctrl_t make_ctrl(uint8_t addr)
{
    m5_ctrl_t m = {};
    m.base.open = ctrl_open; m.base.is_open = ctrl_is_open; m.base.read_reg = ctrl_read; m.base.write_reg = ctrl_write;
    m.base.get_info = ctrl_info; m.base.close = ctrl_close;
    m.addr = addr; m.opened = true;             // 코덱 드라이버는 "이미 열린" 인터페이스를 요구한다
    return m;
}
m5_ctrl_t s_ctrl_out = make_ctrl(0), s_ctrl_in = make_ctrl(ES7210_ADDR);   // 스피커 쪽 칩의 주소는 codec_open 이 넣는다(audioboard::speaker_addr — AB-D1)

bool codecs_answer() { return audioboard::answer(s_lock); }   // 두 코덱이 I2C 에 대답하나(기판마다 스피커 쪽 칩이 다르다 — AB-D1)

// 시험 펌웨어 board_audio.cpp 의 i2s_setup 그대로(PROBE_TX_TDM=0 쪽) + DMA 칸 수·크기를 적는다(PL3a-D8, PL3b-D4).
// 송신과 수신을 한 번의 i2s_new_channel 로 만든다: 5.5.4 드라이버는 한 번에 만들 때만 둘을 한 쌍으로 표시한다(i2s_common.c:1064-1066).
// 따로 만들면 설정이 완전히 같은 표준 모드일 때만 묶이는데(i2s_std.c:244-262) 이 기판은 송신 표준 + 수신 4칸 TDM 이라 클럭을 나누지 못한다.
bool i2s_setup()
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = DMA_DESC;
    chan_cfg.dma_frame_num = BLOCK;
    chan_cfg.auto_clear_after_cb = true;        // PL3a-D4: 보낸 칸은 0 으로 지운다 → 쓰지 않으면 무음(잡음 대신)
    if (i2s_new_channel(&chan_cfg, &s_tx, &s_rx) != ESP_OK) return false;
    const audioboard::Pins pin = audioboard::pins();
    const gpio_num_t PIN_MCLK = pin.mclk, PIN_BCLK = pin.bclk, PIN_WS = pin.ws, PIN_DOUT = pin.dout, PIN_DIN = pin.din;   // AB-D1

    const i2s_tdm_slot_mask_t all4 = (i2s_tdm_slot_mask_t)(I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3);
    i2s_tdm_config_t rx_cfg = {
        .clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(RATE),
        .slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, all4),
        .gpio_cfg = {
            .mclk = PIN_MCLK, .bclk = PIN_BCLK, .ws = PIN_WS, .dout = I2S_GPIO_UNUSED, .din = PIN_DIN,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    i2s_std_config_t tx_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = PIN_MCLK, .bclk = PIN_BCLK, .ws = PIN_WS, .dout = PIN_DOUT, .din = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    if (i2s_channel_init_std_mode(s_tx, &tx_cfg) != ESP_OK) return false;
    if (i2s_channel_init_tdm_mode(s_rx, &rx_cfg) != ESP_OK) return false;
    return i2s_channel_enable(s_tx) == ESP_OK && i2s_channel_enable(s_rx) == ESP_OK;
}

uint8_t es7210_reg(uint8_t reg, bool *ok)       // 마이크 칩 레지스터 하나 읽기(codecs_answer 와 같은 읽기)
{
    uint8_t v = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *ok = M5.In_I2C.readRegister(ES7210_ADDR, reg, &v, 1, I2C_FREQ);
    xSemaphoreGive(s_lock);
    return v;
}

// PL3b-D10 마이크 증폭은 입력 장치를 "연 뒤에" 건다: es7210 드라이버가 장치를 켜면서(es7210_start → mic_select) 증폭을 드라이버 기본값으로
// 덮어쓴다(Phase 0 에서 이 줄이 없어 마이크가 29dB 작게 잡혔다). 입력 0(MIC1) = 칸0 = 마이크, 입력 2(MIC3) = 칸1 = 되먹임(0dB — 30dB 면 포화).
// 입력 장치를 닫았다 열거나 껐다 켜면 드라이버가 다시 지우므로 그때마다 다시 불러야 한다(지금은 부팅 때 한 번 열고 닫지 않는다).
bool mic_gain_apply(int db = MIC_GAIN_DB)       // db: 3dB 단위, 30 이하(부르는 쪽이 확인한다)
{
    if (esp_codec_dev_set_in_channel_gain(s_in, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), (float)db) != ESP_CODEC_DEV_OK) return false;
    if (esp_codec_dev_set_in_channel_gain(s_in, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(2), 0.0f) != ESP_CODEC_DEV_OK) return false;
    bool ok1 = false, ok3 = false;              // 위 호출은 쓰기가 실패해도 OK 를 돌려준다 → 칩에서 되읽는다
    const uint8_t g1 = es7210_reg(ES7210_MIC1_GAIN_REG, &ok1), g3 = es7210_reg(ES7210_MIC3_GAIN_REG, &ok3);
    if (!ok1 || !ok3 || (g1 & 0x0F) != db / 3 || (g3 & 0x0F) != 0) {
        ESP_LOGE(TAG, "mic gain not applied: read %d/%d reg43=0x%02x reg45=0x%02x (want low nibble %d and 0)", (int)ok1, (int)ok3, g1, g3, db / 3);
        return false;
    }
    return true;
}

// 시험 펌웨어 board_audio.cpp 의 init 그대로: 데이터 인터페이스(송수신) → ES8388(DAC) → ES7210(MIC1~4) → 출력 열기 → 입력 열기 → 증폭 → 음량.
// 입력을 열 때 esp_codec_dev 가 송신을 슬롯 32비트로 다시 설정한다(task4.log:182~187) — 그동안의 쓰기는 10ms 쉬고 버려진다
// (audio_codec_data_i2s.c:735-738). 그래서 여기서 전부 연 "뒤에" 출력 태스크를 띄운다(start).
bool codec_open()
{
    audio_codec_i2s_cfg_t i2s_cfg = {};
    i2s_cfg.port = I2S_NUM_0; i2s_cfg.rx_handle = s_rx; i2s_cfg.tx_handle = s_tx;
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    if (!data_if || !gpio_if) return false;

    s_ctrl_out.addr = audioboard::speaker_addr();
    const audio_codec_if_t *out_codec = audioboard::new_speaker_codec(&s_ctrl_out.base, gpio_if);   // AB-D1: Tab5 = ES8388(DAC), 스택짱 = AW88298
    if (!out_codec) return false;

    es7210_codec_cfg_t in_cfg = {};
    in_cfg.ctrl_if = &s_ctrl_in.base;
    in_cfg.mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3 | ES7210_SEL_MIC4;
    const audio_codec_if_t *in_codec = es7210_codec_new(&in_cfg);
    if (!in_codec) return false;

    esp_codec_dev_cfg_t dev_cfg = {};
    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_OUT; dev_cfg.codec_if = out_codec; dev_cfg.data_if = data_if;
    s_out = esp_codec_dev_new(&dev_cfg);
    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN; dev_cfg.codec_if = in_codec;
    s_in = esp_codec_dev_new(&dev_cfg);
    if (!s_out || !s_in) return false;

    esp_codec_dev_sample_info_t fs_out = {};
    fs_out.bits_per_sample = 16; fs_out.channel = 1; fs_out.channel_mask = 0; fs_out.sample_rate = RATE;
    if (esp_codec_dev_open(s_out, &fs_out) != ESP_CODEC_DEV_OK) return false;
    esp_codec_dev_sample_info_t fs_in = {};
    fs_in.bits_per_sample = 16; fs_in.channel = IN_SLOTS; fs_in.channel_mask = 0x0F; fs_in.sample_rate = RATE;
    if (esp_codec_dev_open(s_in, &fs_in) != ESP_CODEC_DEV_OK) return false;
    if (!mic_gain_apply()) return false;
    if (!audioboard::volume_set(s_out, VOLUME)) return false;
    if (!audioboard::after_open(s_out)) return false;   // AB-D1: 스택짱은 여기서 앰프의 I2S 설정을 맞춘다(Tab5 는 할 일이 없다)
    return s_i2c_err == 0;                      // 음량 설정은 코덱 쓰기가 실패해도 OK 를 돌려준다(esp_codec_dev.c:376~378) → 쓰기 실패 횟수로 확인한다
}

// 스피커 앰프를 켜고 끈다 — 방법은 기판마다 다르다(AB-D1: Tab5 = 확장칩 0x43 의 P1 을 읽고-고쳐-쓰기, 스택짱 = 앰프의 무음 비트). 잠금은 그쪽이 안에서 잡는다.
// 사실(M5GFX.cpp:3137·3146, Power_Class.inl:153·175 — 소스 판독, 기기에서 레지스터를 읽어 확인한 것은 아니다): Tab5 에서는 M5GFX 가 부팅(M5.begin → 화면 자동 감지)마다 그 레지스터에
//   0b01000110 → 0b01110110 을 써서 비트1(앰프)을 켜고, 같은 M5.begin 안에서 M5Unified 의 Power.begin 이 0b01110000 을 써서 다시 끈다. 즉 M5.begin 이 돌아올 때 앰프는
//   꺼져 있고, 켜져 있는 시간은 화면 초기화 동안(0.3~1초쯤)이다. 우리가 M5.begin 직후와 실패 경로에서 한 번 더 끄는 것(PL3b-D21)은 라이브러리 판이 바뀌어도 꺼져 있게 하는
//   이중 안전장치다(Phase 3c 에서 사실 정정 — fix 2 재검수 Minor 1. 3b 의 주석은 "켜진 채 시작해 약 2초 켜 둔다"고 적었다).
bool amp_set(bool on)
{
    const uint32_t err0 = s_i2c_err.load();
    // 코덱 제어 인터페이스를 거치는 기판(스택짱 — esp_codec_dev 의 무음 호출)은 쓰기가 실패해도 OK 를 돌려준다(esp_codec_dev.c:432~435) → 쓰기 실패 횟수가 늘었는지도 본다.
    // Tab5 는 확장칩에 직접 쓰므로 이 횟수가 움직이지 않는다(판정은 전과 같다).
    const bool ok = audioboard::amp_set(s_lock, s_out, on) && s_i2c_err.load() == err0;
    if (ok) s_amp = on;
    return ok;
}

void out_task(void *)
{
    while (true) {
        const int64_t t0 = esp_timer_get_time();
        source_fn fns[MAX_SOURCES];
        for (int i = 0; i < MAX_SOURCES; ++i) fns[i] = s_src[i].load();
        // PL3b-D3: 섞기는 순수 함수(pcm.h — 맥 시험 "mix: …")다. 신호음과 목소리가 함께 소리를 내면 더하고 넘치면 자른다.
        if (pcm::mix_sources(fns, MAX_SOURCES, s_mix, s_tmp, BLOCK) == 0) {
            s_watch.rest();
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);                        // PL3a-D4: 낼 것이 없으면 쓰지 않고 잠든다(DMA 가 0 을 낸다)
            continue;
        }
        const uint32_t us = (uint32_t)(esp_timer_get_time() - t0);          // 소리를 낸 묶음만 잰다
        if (us > s_max_us.load()) s_max_us = us;
        s_sum_us += us; ++s_blocks;
        s_avg_us = (uint32_t)(s_sum_us / s_blocks);
        if (esp_codec_dev_write(s_out, s_mix, (int)sizeof s_mix) != ESP_CODEC_DEV_OK) {             // 정상이면 DMA 칸이 빌 때까지 기다린다 = 16ms 박자
            ++s_wr_err;
            vTaskDelay(pdMS_TO_TICKS(16));      // 쓰기가 바로 실패해도 박자를 지킨다 — 쉬지 않고 돌면 우선순위가 낮은 화면·터치 태스크가 굶는다(PL3a-D10)
        }
        s_watch.wrote(esp_timer_get_time(), LATE_US);                       // PL3b-D4: 48ms 넘게 걸려서야 다음 묶음을 썼으면 그 사이 스피커가 끊겼다
        s_late = s_watch.late;
        s_gap_max = s_watch.max_us;                                         // PL3b-D25: 쓰기 간격의 최대(µs, 쉬었다 다시 쓴 첫 묶음은 제외)
    }
}

}  // namespace

void amp_off_at_boot(SemaphoreHandle_t i2c_lock)
{
    s_lock = i2c_lock;                          // start 가 같은 값을 다시 넣는다. 여기서 먼저 넣어야 fail 이 잠금 없이 불리는 길에서도 앰프를 끈다
    if (!s_lock) { ESP_LOGW(TAG, "amp not forced off at boot: no i2c lock"); return; }
    if (amp_set(false)) ESP_LOGI(TAG, "amp forced off at boot (M5GFX turns it on at every boot)");
    else ESP_LOGW(TAG, "amp could not be forced off at boot (i2c write failed) - audioout::start retries the same write");   // 부팅은 멈추지 않는다
}

bool start(SemaphoreHandle_t i2c_lock, bool amp_on, bool inject_fail)
{
    if (s_ready || s_failed || s_tx) return s_ready;   // 한 번만(두 번째 호출은 첫 결과)
    s_lock = i2c_lock;
    const char *stage = nullptr;                // 실패한 단계
    if (inject_fail) stage = "injected";        // PL3b-D21: 실패 주입도 잠금을 가진 채 실패한다 → fail 이 앰프를 끈다
    else if (!s_lock) stage = "lock";
    else if (!codecs_answer()) stage = "i2c";
    else if (!i2s_setup()) stage = "i2s";
    else if (!codec_open()) stage = "codec";
    else {
        vTaskDelay(pdMS_TO_TICKS(30));          // PL3a-D4: 코덱 출력이 자리 잡은 뒤 앰프를 켠다
        if (!amp_set(amp_on)) stage = "amp";    // 조용한 모드(amp_on 거짓)도 끄기를 명시적으로 한다: M5GFX 가 부팅마다 켜 둔다(fix 1 F1, 위 amp_set 의 설명)
        else if (xTaskCreatePinnedToCore(out_task, "audio", TASK_STACK, nullptr, TASK_PRIO, &s_task, TASK_CORE) != pdPASS) stage = "task";   // 전부 연 뒤에 띄운다
    }
    if (stage) { fail(stage); return false; }   // PL3b-D2: phone_main 의 boot_guard 가 3번까지 재시작하고, 그래도 안 되면 소리 없이 계속 돈다
    s_ready = true;
    printf("AUDIO ready rate=%d block=%d vol=%d dma=%d mic=%d amp=%d\n", RATE, BLOCK, VOLUME, DMA_DESC, MIC_GAIN_DB, (int)s_amp.load());
    return true;
}

bool read_mic(int16_t *dst, int frames)         // 시험 펌웨어 board_audio.cpp 의 read_slots 그대로
{
    if (!s_in || !s_ready || esp_codec_dev_read(s_in, dst, frames * IN_SLOTS * (int)sizeof(int16_t)) != ESP_CODEC_DEV_OK) { ++s_rd_err; return false; }
    return true;
}

bool amp(bool on)
{
    if (!s_lock || !s_out || (on && s_failed)) return false;
    return amp_set(on);
}
bool amp_is_on() { return s_amp; }

// PL3b-D24 실행 중 음량. 시작 때 음량을 거는 길(codec_open 의 esp_codec_dev_set_out_vol)과 같다 — 코덱 제어 인터페이스가 ctrl_write 안에서 I2C 잠금을 스스로 잡으므로
// 여기서는 잠금을 잡지 않는다(잡으면 같은 뮤텍스를 두 번 잡아 멈춘다. 시험 펌웨어 board_audio.cpp 의 set_volume, 186~189줄도 잠금 없이 부른다).
// esp_codec_dev_set_out_vol 은 쓰기가 실패해도 OK 를 돌려주므로(esp_codec_dev.c:376~378) 쓰기 실패 횟수가 늘었는지로 확인한다 — 실패면 이전 값을 유지.
VolResult set_volume(int percent)
{
    if (percent < 0 || percent > 100) return VolResult::RANGE;
    if (!s_out || !s_ready.load()) return VolResult::NO_AUDIO;
    const uint32_t err0 = s_i2c_err.load();
    if (!audioboard::volume_set(s_out, percent) || s_i2c_err.load() != err0) return VolResult::I2C;   // AB-D1: 거는 법은 기판마다(Tab5 = esp_codec_dev 의 곡선 그대로)
    s_vol = percent;
    return VolResult::OK;
}
int volume() { return s_vol.load(); }

// PL3c-D9 실행 중 마이크 증폭. 부팅 때와 같은 길(mic_gain_apply — 걸고, 칩의 증폭 레지스터를 되읽어 확인한다). 되먹임 칸(MIC3)은 늘 0dB 그대로다.
// 올리는 쪽은 부팅 값의 상한(voicetune::MIC_DB_MAX = 24)까지만: 올리면 스피커 → 마이크 고리 이득이 그만큼 오른다. 코덱 제어 인터페이스가 I2C 잠금을 스스로 잡는다(set_volume 과 같다).
VolResult set_mic_gain(int db)
{
    if (!voicetune::ok_mic_db(db)) return VolResult::RANGE;
    if (!s_in || !s_ready.load()) return VolResult::NO_AUDIO;
    if (!mic_gain_apply(db)) { mic_gain_apply(s_mic_db.load()); return VolResult::I2C; }   // 실패하면 앞 값으로 되돌려 본다(그것도 실패하면 로그가 남는다)
    s_mic_db = db;
    return VolResult::OK;
}
int mic_gain() { return s_mic_db.load(); }

bool add_source(source_fn fn)
{
    for (auto &slot : s_src) {
        source_fn empty = nullptr;
        if (slot.compare_exchange_strong(empty, fn)) { wake(); return true; }
    }
    return false;
}

void wake() { if (s_task) xTaskNotifyGive(s_task); }

// PL3b-D2·D21 실패 뒤 정리는 "앰프를 끈다"까지만 한다. 잠금이 있으면 s_amp 값과 상관없이 늘 끈다(M5GFX 가 부팅마다 켜 두므로 s_amp=거짓이어도 실제로는 켜져 있다). I2S 채널·코덱 객체는 닫지 않는다: 닫는 코드는 이 기기에서 한 번도 돈 적이 없고,
//   반쯤 열린 객체를 닫다 죽으면 boot_guard 가 세지 못하는 재부팅 고리가 된다. 남은 채널은 0 만 내보낸다(앰프가 꺼져 있어 들리지 않는다).
//   출력 태스크가 이미 떠 있으면 그대로 둔다(공급자가 소리를 주면 쓰지만 앰프가 꺼져 있다).
void fail(const char *stage)
{
    s_failed = true; s_ready = false;
    ESP_LOGE(TAG, "audio failed at '%s' -> the phone runs without sound", stage);
    printf("AUDIO FAIL %s\n", stage);
    // 끄기에 성공하면 s_amp=거짓 → STAT amp= 가 실제와 같다. 잠금이 없으면(stage=lock) 할 수 있는 것이 없다.
    // Phase 3c(fix 2 재검수 Minor 2): 끄기 쓰기가 실패하면 그 사실을 남긴다 — 확장칩이 대답하지 않는 고장에서는 한 번도 끄지 못했는데 STAT 에는 amp=0 으로 보인다.
    if (s_lock && !amp_set(false)) ESP_LOGW(TAG, "amp could not be switched off after the failure (i2c write failed) - STAT amp= may not match the hardware");
}

bool ready() { return s_ready; }
void block_us(uint32_t *avg, uint32_t *max)
{
    if (avg) *avg = s_avg_us.load();
    if (max) *max = s_max_us.load();
}
uint32_t write_errors() { return s_wr_err; }
uint32_t read_errors() { return s_rd_err; }
uint32_t late_blocks() { return s_late; }
uint32_t gap_max_us() { return s_gap_max; }
unsigned stack_free() { return s_task ? (unsigned)uxTaskGetStackHighWaterMark(s_task) : 0; }

}  // namespace audioout
