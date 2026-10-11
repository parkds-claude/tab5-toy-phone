// audio_out.h — 소리 장치: 스피커 출력(ES8388 코덱 + I2S0 송신)과 마이크 입력(ES7210 + I2S0 수신, Phase 3b).
// 출력은 "PCM 을 받아 낸다" — 무엇을 낼지는 모른다. 입력은 "마이크 칩의 4칸을 읽어 준다" — 무엇에 쓸지는 모른다(voice.cpp 가 쓴다).
// 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md PL3a-D3·D4·D8~D10, docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md PL3b-D2~D4·D10.
//   - 출력 태스크가 BLOCK 표본(16ms)마다 등록된 공급자(source_fn)에게 PCM 을 받아 합쳐(pcm::mix_sources) 코덱에 쓴다. 신호음(tone_out)과
//     목소리(voice)가 공급자다. 낼 것이 없으면 쓰지 않고 잠든다 — I2S 는 계속 돌며 0 을 낸다(PL3a-D4. 마이크가 같은 클럭을 쓰므로 송신을 끄지 않는다).
//   - 실패해도(코덱이 대답하지 않음 등) 전화기는 소리 없이 계속 돈다: start 가 false 와 "AUDIO FAIL <단계>" 한 줄(PL3a-D10).
//     Phase 3b(PL3b-D2): 부르는 쪽(phone_main)이 boot_guard 로 3번까지 다시 켜 본 뒤에 그렇게 된다.
//   - 앰프(PL3b-D21): M5GFX 가 부팅마다(M5.begin 안에서) 앰프를 켠다. 그래서 phone_main 이 M5.begin 직후 amp_off_at_boot 로 먼저 끄고, 실패 경로(fail)는 잠금이 있으면 늘 끈다
//     → 소리 장치가 실패한 채로 도는 전화기는 앰프도 꺼져 있다(STAT amp=0 이 실제와 같다).
#pragma once
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace audioout {

constexpr int RATE = 16000;                 // 표본율(PH-D5). 16비트 모노
constexpr int BLOCK = 256;                  // 한 번에 내는 표본 수 = I2S DMA 한 칸(16ms, PL3a-D8)

constexpr int IN_SLOTS = 4;                 // 마이크 칩이 한 번에 내는 칸 수(TDM): 칸0 = 마이크, 칸1 = 스피커 되먹임(에코 기준), 칸2·3 = 안 쓴다(Phase 0 실측)

// 공급자: 출력 태스크가 BLOCK 마다 부른다. 낼 소리가 있으면 out 에 n 표본을 쓰고 true, 없으면 false(out 은 쓰지 않아도 된다).
// 공급자의 조건(3a 최종 검수 5항): 출력 태스크(우선순위 6, 스택 6KB) 안에서 불린다 → 기다리지 않는다(잠금 대기·printf·코덱 해제 금지),
//   스택을 크게 쓰지 않는다, 1ms 안에 끝낸다. 목소리처럼 무거운 일(Opus 해제)은 다른 태스크가 하고 공급자는 링버퍼에서 복사만 한다.
//   출력 태스크는 잠에서 깬 직후 DMA 가 찰 때까지 DMA 칸 수만큼(3묶음)을 연달아 요구한다 — 그때 줄 것이 없으면 무음을 준다.
typedef bool (*source_fn)(int16_t *out, int n);

// 코덱 둘과 I2S 송수신을 열고, 마이크 증폭을 걸고, 앰프를 켜고(amp_on 이 참일 때), 출력 태스크를 띄운다. 한 번만.
// i2c_lock = 터치와 함께 쓰는 M5.In_I2C 잠금(phone_main 이 만든다).
// inject_fail 이 참이면 장치를 열지 않고 "injected" 단계로 실패한다(실패 주입 — PL3b-D2). 잠금을 가진 채 실패하므로 앰프가 꺼진다(PL3b-D21).
bool start(SemaphoreHandle_t i2c_lock, bool amp_on = true, bool inject_fail = false);

// app_main 에서 M5.begin() 바로 뒤, 다른 어떤 소리 초기화보다 먼저: 잠금을 안에서 잡고 앰프를 한 번 끈다(PL3b-D21). 실패해도 부팅을 멈추지 않는다(로그 한 줄 — start 가 같은 쓰기를 다시 한다).
void amp_off_at_boot(SemaphoreHandle_t i2c_lock);
bool add_source(source_fn fn);              // 공급자 등록(최대 2). start 앞뒤 어느 때나. 자리가 없으면 false
void wake();                                // 공급자에게 낼 것이 생겼다 — 잠든 출력 태스크를 깨운다(어느 태스크에서나, 소리 장치가 없으면 아무 일도 안 함)

// 소리 경로의 어느 단계가 실패했음을 알린다: "AUDIO FAIL <stage>" 한 줄, ready() 는 false, 앰프는 끈다(I2C 잠금이 있으면 — 앰프가 켜져 있다고 기록되어 있든 아니든 조건 없이). start 안에서도, 공급자 모듈(tone_out·voice)의
// 준비가 실패했을 때도 부른다 — 그래서 "AUDIO FAIL tones 인데 STAT 은 audio=1" 같은 어긋남이 없다(3a 최종 검수 M-3).
void fail(const char *stage);

// 마이크 칩의 4칸을 frames 표본만큼 읽는다(dst 크기 = frames × IN_SLOTS, 칸0,칸1,칸2,칸3,칸0,…). frames = BLOCK 이면 DMA 한 칸이 찰 때까지(16ms) 기다린다.
// 실패하면(장치 없음, 1초 안에 못 읽음) false 와 읽기 실패 횟수 +1. 한 태스크에서만 부른다(voice 의 캡처 태스크).
// 읽지 않는 동안에도 I2S 수신은 돈다(오래된 칸은 드라이버가 버린다) — 다시 읽기 시작하면 앞의 DMA 칸 수만큼은 묵은 소리다(부르는 쪽이 버린다).
bool read_mic(int16_t *dst, int frames);

// 스피커 앰프를 켜거나 끈다(어느 태스크에서나 — I2C 잠금 안에서 한다). 끄면 스피커는 조용하지만 코덱은 그대로 돌아 되먹임 칸(칸1)에는 소리가 잡힌다.
// 소리 경로가 실패한 뒤에는 켜지지 않는다. 돌려주는 값 = 성공.
bool amp(bool on);
bool amp_is_on();                           // 마지막으로 성공한 쓰기의 값(부팅 직후 amp_off_at_boot 가 성공했다면 실제와 같다)

// PL3b-D24 코덱 재생 음량(0~100%) — 실행 중에만 유효하다(저장하지 않는다. 재부팅하면 시작 값 100). 통화 중에도 된다.
enum class VolResult : uint8_t { OK, RANGE, NO_AUDIO, I2C };
VolResult set_volume(int percent);          // RANGE = 0~100 밖, NO_AUDIO = 소리 장치가 안 열렸다, I2C = 코덱 쓰기 실패(값은 그대로)
int volume();                               // 지금 값(STAT vol=)
// PL3c-D9 마이크 증폭(12~24dB, 3dB 단위) — 실행 중에만 유효하다(저장하지 않는다. 재부팅하면 시작 값). 통화 중에도 된다(바꾸는 순간 딸깍 소리가 날 수 있다).
VolResult set_mic_gain(int db);             // RANGE = 범위·단위 밖, NO_AUDIO = 소리 장치가 안 열렸다, I2C = 칩에서 되읽은 값이 다르다(앞 값으로 되돌린다)
int mic_gain();                             // 지금 값(TUNE mic=)

bool ready();                               // 소리 경로 전체가 준비됐나(STAT audio=). fail 이 한 번이라도 불렸으면 false
void block_us(uint32_t *avg, uint32_t *max);   // 소리를 낸 묶음 하나를 만드는 데 걸린 시간의 평균·최대(µs, 부팅 뒤). 묶음 하나 = 16000µs(STAT aud_us=평균/최대).
                                               // 공급자 호출 + 합치기 + 그 사이에 끼어든 다른 태스크(캡처·해제)가 CPU 를 쓴 시간까지 들어간다 → 출력 태스크의 일이 아니라 "밀린 시간"의 상한이다(PL3b-D25)
uint32_t write_errors();                    // 코덱 쓰기 실패 횟수(STAT aud_err=)
uint32_t read_errors();                     // 마이크 읽기 실패 횟수
uint32_t gap_max_us();                      // 출력 쓰기 사이 간격의 최대(µs, 부팅 뒤, 쉬었다 다시 쓴 첫 묶음은 제외 — STAT aud_gap=). 16000 이 정상, DMA 가 쥔 양 48000 이상이면 끊김
uint32_t late_blocks();                     // 출력이 늦은 횟수: 앞 묶음을 쓴 뒤 DMA 가 쥔 양(3칸 = 48ms) 넘게 지나서야 다음 묶음을 썼다 = 그 사이 스피커가 끊겼다(STAT aud_late=)
unsigned stack_free();                      // 출력 태스크 스택 최저 여유(바이트, 태스크가 없으면 0 — STAT stk_audio=)

}  // namespace audioout
