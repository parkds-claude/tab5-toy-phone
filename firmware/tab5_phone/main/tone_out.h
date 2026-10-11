// tone_out.h — 신호음 공급자. app 이 상태 기계의 할 일에서 고른 소리(tones::from_acts)를 받아, 출력 태스크(audio_out)에 PCM 으로 건넨다.
// 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md PL3a-D3·D6·D11. 박자·크기·멈춤 약속은 tones.h(맥 시험)에 있다.
// Phase 3b(docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md PL3b-D1): 기록 줄 "TONE …" 은 출력 태스크가 찍지 않는다 —
//   출력 태스크는 찍을 것을 대기열에 남기고, app 태스크가 print_pending() 으로 찍는다(출력 태스크가 시리얼에 막혀 소리가 끊기지 않게).
#pragma once
#include "tones.h"

namespace toneout {

bool start();                   // audio_out 에 공급자로 등록한다. audioout::start 뒤, app::start 앞에 한 번(소리 장치가 없어도 불러도 된다).
                                // 실패(메모리 부족·자리 없음)하면 audioout::fail("tones") 로 알리고 false — STAT 의 audio= 가 0 이 된다
void play(tones::Tone t);       // 이 소리를 낸다(NONE = 멈춤). app 태스크에서. 바로 돌아온다 — 마지막 명령만 남는다(PL3a-D6)
tones::Tone now();              // 지금 내는 소리(STAT tone=). 출력 태스크가 바꾼다
void print_pending();           // 출력 태스크가 남긴 "TONE <이름> on|off" 줄을 찍는다. app 태스크가 20ms 바퀴마다 부른다(다른 태스크에서 부르지 않는다)

}  // namespace toneout
