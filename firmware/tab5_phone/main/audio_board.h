// audio_board.h — 소리 장치에서 "기판마다 다른 부분": 핀, 스피커 쪽 칩, 앰프 켜고 끄기, 재생 음량.
// audio_out.cpp(출력 태스크·공급자 섞기·마이크 읽기·통계 — 기판과 무관)가 이 함수들만 불러 기판에 닿는다.
//   구현은 기판마다 하나: Tab5 = main/audio_board_tab5.cpp(ES8388 + 확장칩 앰프), 스택짱(CoreS3) = ../stackchan_phone/main/audio_board_cores3.cpp(AW88298).
//   마이크 칩은 두 기판 모두 ES7210 이고 칸 배치도 같다(칸0 = 마이크, 칸1 = 스피커 되먹임 — Tab5 Phase 0·스택짱 관문 실험에서 각각 실측) → 마이크 쪽은 audio_out.cpp 에 그대로 둔다.
// AB-D1 (2026-10-08, 설계서 2026-10-08-phone-3way-design.md TW-D11): 기판 차이를 #if 로 audio_out.cpp 에 흩지 않고 이 여섯 함수 뒤에 모은다.
//   이유: 출력 태스크의 박자·늦음 감시·실패 처리는 Tab5 에서 검증된 코드다 — 스택짱이 그것을 복사하지 않고 함께 쓴다.
//   비용: 함수 호출 한 겹(부팅 때와 음량·앰프를 바꿀 때만 — 소리 묶음마다 부르는 것은 없다). 탈출구: 구현 파일의 내용을 audio_out.cpp 로 되돌려 넣는다.
#pragma once
#include <cstdint>

#include "audio_codec_ctrl_if.h"
#include "audio_codec_gpio_if.h"
#include "audio_codec_if.h"
#include "driver/gpio.h"
#include "esp_codec_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace audioboard {

struct Pins { gpio_num_t mclk, bclk, ws, dout, din; };

constexpr uint32_t I2C_FREQ = 400000;       // 코덱·앰프·확장칩이 물린 M5.In_I2C 의 속도(두 기판 공통)
constexpr uint8_t  MIC_CHIP_ADDR = 0x40;    // ES7210(두 기판 공통)

Pins    pins();                             // I2S 핀
uint8_t speaker_addr();                     // 스피커 쪽 칩의 I2C 주소(코덱 제어 인터페이스가 쓴다)
const char *name();                         // 기록에 찍을 기판 이름("tab5", "cores3")

// 스피커 쪽 칩과 마이크 칩이 I2C 에 대답하나(레지스터 하나씩 읽기). lock = M5.In_I2C 잠금 — 안에서 잡는다.
// 칩이 대답하려면 먼저 해 둘 일(스택짱: 앰프의 리셋 풀기)도 여기서 한다.
bool answer(SemaphoreHandle_t lock);

// 스피커 쪽 코덱 객체를 만든다(esp_codec_dev). ctrl = 그 칩의 제어 인터페이스(audio_out.cpp 가 M5.In_I2C 로 만든 것).
const audio_codec_if_t *new_speaker_codec(const audio_codec_ctrl_if_t *ctrl, const audio_codec_gpio_if_t *gpio);

// 출력·입력 장치를 다 연 뒤 한 번. 할 일이 없으면 true(Tab5). 스택짱: 수신이 4칸이면 송신도 프레임당 BCLK 64 로 나가므로 앰프의 설정을 거기에 맞춘다.
bool after_open(esp_codec_dev_handle_t out);

// 스피커를 켜거나 끈다. out 은 아직 열리지 않았으면 nullptr(부팅 직후의 "먼저 끄기" — 그때는 기판이 할 수 있는 만큼만). lock 안에서 잡는다. 돌려주는 값 = 성공.
bool amp_set(SemaphoreHandle_t lock, esp_codec_dev_handle_t out, bool on);

// 재생 음량(0~100%). 1% = 0.5dB, 100% = 그 기판에서 쓰는 가장 큰 소리(Tab5 는 0dB, 스택짱은 앰프 −6dB — TW-D13). 돌려주는 값 = 호출이 성공했나
// (코덱 쓰기가 실제로 됐는지는 audio_out.cpp 가 I2C 실패 횟수로 한 번 더 본다).
bool volume_set(esp_codec_dev_handle_t out, int percent);

}  // namespace audioboard
