// tone_out.cpp — 신호음 공급자. 설명은 tone_out.h.
//
// PL3a-D6 명령은 길이 1 우편함(xQueueOverwrite)으로 건넨다 — 마지막 명령만 남는다. 명령 하나가 "지금 내야 할 소리"를 통째로 말하므로
//         마지막 것만 맞으면 된다(맥 시험 "stop then bell in one batch"). 대기열이 가득 차 멈춤 명령을 잃는 일이 없다.
//         비용: 출력 태스크가 꺼내기 전에 벨 → 멈춤이 연달아 오면 그 벨은 한 표본도 나지 않고 "TONE BELL on" 줄도 없다. 탈출구: 길이 8 대기열.
// PL3a-D11 기록 줄 "TONE <이름> on|off" 는 출력 태스크가 소리를 실제로 만든 뒤에 나온다(무엇을 찍을지는 tones::next_block — 맥 시험).
//         off = 만든 PCM 이 0 이 된 때. 스피커에서 소리가 그치는 것은 그보다 DMA 에 앞서 쓴 만큼(audio_out.cpp 의 DMA_DESC) 늦다.
// PL3b-D1 그 줄을 출력 태스크(우선순위 6)가 직접 찍지 않는다(3a 최종 검수 M-1): printf 는 stdout 잠금과 USB 직렬 드라이버를 거쳐,
//         읽는 쪽이 느리거나 다른 태스크가 긴 줄(shot 의 B64 한 줄 약 0.36초)을 찍는 중이면 그동안 출력 태스크가 멈춘다 — 목소리가 끊긴다.
//         출력 태스크는 찍을 것을 길이 16 대기열에 넣기만 하고(가득 차면 버리고 센다) app 태스크가 20ms 바퀴마다 꺼내 찍는다.
//         줄의 모양·기기별 순서는 그대로다. "FSM …" 줄과의 앞뒤는 전처럼 정해져 있지 않다(app 이 다음 바퀴에 찍으므로 대개 FSM 줄 뒤).
//         비용: TONE 줄이 최대 한 바퀴(20ms, 화면을 내보내는 중이면 60~80ms) 늦게 찍힌다. 탈출구: fill 안에서 다시 printf.
#include "tone_out.h"

#include <atomic>
#include <cstdio>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "audio_out.h"

static_assert(tones::RATE == audioout::RATE, "tones.h and audio_out.h must use the same sample rate");   // 3a 최종 검수 M-6: 한쪽만 바꾸면 음 높이·박자가 조용히 틀린다

namespace toneout {
namespace {

struct Line { tones::Tone off, on; };                   // 찍을 줄: "TONE <off> off" 다음 "TONE <on> on"(NONE 은 찍지 않는다)
constexpr int LOG_LEN = 16;

QueueHandle_t s_box = nullptr;                          // 길이 1 우편함
QueueHandle_t s_log = nullptr;                          // 출력 태스크 → app 태스크: 찍을 줄(PL3b-D1)
tones::Player s_player;                                 // 출력 태스크에서만 만진다
std::atomic<tones::Tone> s_now{tones::Tone::NONE};
std::atomic<uint32_t> s_log_lost{0};

bool fill(int16_t *out, int n)                          // audioout 의 공급자 — 출력 태스크에서 16ms 마다
{
    tones::Tone cmd = tones::Tone::NONE;
    const bool has_cmd = s_box && xQueueReceive(s_box, &cmd, 0) == pdTRUE;
    const tones::Block b = tones::next_block(s_player, has_cmd, cmd, out, n);   // 명령 → 한 묶음 → 찍을 줄(맥 시험으로 고정된 로직)
    if (b.off != tones::Tone::NONE || b.on != tones::Tone::NONE) {              // PL3b-D1: 찍지 않고 남긴다(기다리지 않는다)
        const Line l{b.off, b.on};
        if (!s_log || xQueueSend(s_log, &l, 0) != pdTRUE) ++s_log_lost;
    }
    s_now = s_player.current();
    return b.sound;
}

}  // namespace

bool start()
{
    if (s_box) return true;
    s_box = xQueueCreate(1, sizeof(tones::Tone));
    s_log = xQueueCreate(LOG_LEN, sizeof(Line));
    tones::sine_table();                                // 사인 표를 지금 만든다(출력 태스크의 첫 묶음이 늦지 않게)
    if (!s_box || !s_log || !audioout::add_source(fill)) { audioout::fail("tones"); return false; }   // 3a 최종 검수 M-3: audio= 도 0 이 된다
    return true;
}

void play(tones::Tone t)
{
    if (!s_box) return;
    xQueueOverwrite(s_box, &t);
    audioout::wake();
}

tones::Tone now() { return s_now; }

void print_pending()
{
    Line l;
    while (s_log && xQueueReceive(s_log, &l, 0) == pdTRUE) {
        if (l.off != tones::Tone::NONE) printf("TONE %s off\n", tones::tone_name(l.off));
        if (l.on != tones::Tone::NONE) printf("TONE %s on\n", tones::tone_name(l.on));
    }
    const uint32_t lost = s_log_lost.exchange(0);
    if (lost) printf("TONE ? lost=%u\n", (unsigned)lost);    // 대기열이 넘쳤다(app 이 오래 멈췄다) — 줄 몇 개가 빠졌음을 알린다
}

}  // namespace toneout
