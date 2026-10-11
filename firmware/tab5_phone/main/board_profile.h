// board_profile.h — 기판(칩)마다 달라야 하는 통화 값들을 한 곳에. 순수 상수(맥 시험에서도 읽힌다 — 그때는 Tab5 의 값).
//   설계서 docs/exec-plans/2026-10-08-phone-3way-design.md TW-D11·D12: 두 기기가 같은 소스(voice.cpp·audio_out.cpp·relay_link.cpp)를 쓰고, 다른 값은 여기서만 가른다.
//   가르는 기준은 칩이다: ESP32-S3 = 스택짱(M5Stack CoreS3 기반), 그 밖 = Tab5(ESP32-P4) — 이 저장소의 전화기는 그 둘뿐이다.
//   스택짱 값의 근거는 전부 관문 실험(docs/exec-plans/2026-10-08-stackchan-phone-probe.md §8 "관문 결과"의 조건 1·2·4·5)의 실측이다.
//   BP-D1 Tab5 의 값은 이 파일을 만들기 전과 한 글자도 다르지 않다(voice.cpp·voice_tune.h·relay_link.cpp 에 적혀 있던 숫자를 옮긴 것) — Tab5 의 동작은 그대로다.
#pragma once

#if defined(__has_include)
#  if __has_include("sdkconfig.h")
#    include "sdkconfig.h"
#  endif
#endif

namespace boardprofile {

#if defined(CONFIG_IDF_TARGET_ESP32S3)
// ── 스택짱(ESP32-S3) ────────────────────────────────────────────────────────
constexpr bool STACKCHAN = true;
constexpr int  MIC_DB = 12;                 // 마이크 증폭. Tab5 의 24dB 는 여기서 넘친다(30dB·음량 50 에서 재생한 것보다 크게 잡혔다) — 12dB 에서 포화 0%, 메아리 감소 32~43dB
constexpr bool AEC_LOW_COST = true;         // 에코 제거 = AEC_MODE_VOIP_LOW_COST: 감소량은 고성능과 같고(34.4/42.9/32.2dB), 말소리가 흐를 때 6.4ms/16ms(고성능은 9.1~9.7ms)
// 압축(Opus)을 캡처 태스크에서 떼어 다른 코어에서 돌린다: 한 코어에 에코 제거 + 압축을 다 올리면 80~93% 였다. 나누면 코어 1 = 55%(에코 제거 + 화면), 코어 0 = 49%(Opus + Wi-Fi).
constexpr bool SPLIT_ENC = true;
constexpr int  CAP_CORE = 1, CAP_PRIO = 7, CAP_STACK = 24 * 1024;   // 마이크 → 에코 제거(+ 에코 제거·Opus 만들기 — codec_setup 이 이 스택에서 돈다). 에코 제거만의 스택 사용과 만들기 경로는 이 칩에서 재지 못했다(실측은 "에코 제거 + Opus 한 태스크 = 19KB, Opus 단독 19~21KB") — 넉넉히 잡고 기기에서 STAT stk_vcap 을 본 뒤 줄인다(독립 검수 2026-10-08 Important 1)
constexpr int  ENC_CORE = 0, ENC_PRIO = 6, ENC_STACK = 28 * 1024;   // Opus 압축. 스택 28KB 는 관문 시험 펌웨어가 돌린 값(사용 19~21KB). 우선순위는 교환기 연결(5)보다 위
constexpr int  RX_CORE = 0;                 // Opus 해제도 코어 0(관문 실험이 잰 배치 — 코어 0 의 49% 에 들어 있다)
constexpr bool RELAY_STACK_PSRAM = true;    // 교환기 연결 태스크의 스택을 PSRAM 에(내부 메모리가 빠듯하다 — TLS 연결 중 최저 13KB 였다). 이 태스크는 플래시를 만지지 않는다
// 자가 점검(selftest)의 되먹임 칸 판정선: 이 기판의 되먹임 칸은 앰프 출력에서 온다 — 음량 맨 위 단계(vol 100)·스피커 켬에서 134(2026-10-09 두 번, 그때 mic 444 · aec 16~22),
// 스피커를 끄면 1 이다. Tab5 와 같은 비율(실측의 절반쯤)로 잡는다. 스피커를 끈 채(quiet on)로는 판정할 수 없다(늘 ok=0).
constexpr unsigned SELFTEST_REF_MIN = 70;
#else
// ── Tab5(ESP32-P4) — 그리고 맥 시험 ─────────────────────────────────────────
constexpr bool STACKCHAN = false;
constexpr int  MIC_DB = 24;                 // PL3b-D10
constexpr unsigned SELFTEST_REF_MIN = 300;  // PL3b-D14: 되먹임 칸은 앰프를 꺼도 잡힌다(진폭 2000 에서 약 590) — 300 이상이면 "코덱이 소리를 내고 있다"
constexpr bool AEC_LOW_COST = false;        // AEC_MODE_VOIP_HIGH_PERF (Phase 0)
constexpr bool SPLIT_ENC = false;           // 캡처 태스크가 에코 제거와 압축을 함께 한다(PL3b-D15)
constexpr int  CAP_CORE = 1, CAP_PRIO = 7, CAP_STACK = 32 * 1024;   // PL3b-D15, PL3c-D1
constexpr int  ENC_CORE = 0, ENC_PRIO = 6, ENC_STACK = 0;           // 쓰지 않는다(SPLIT_ENC 가 거짓)
constexpr int  RX_CORE = 1;                 // PL3b-D15
constexpr bool RELAY_STACK_PSRAM = false;
#endif

}  // namespace boardprofile
