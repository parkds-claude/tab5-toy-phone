// voice.h — 목소리 통화 경로(기기 전용). 판단은 voice_core.h(맥 시험)에 있고, 여기는 그것을 태스크·코덱·교환기에 잇기만 한다.
// 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md PL3b-D5~D16. Phase 3c(docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md): 통화 품질 손잡이(PL3c-D3~D9).
//   보내는 길(캡처 태스크): 마이크 4칸 읽기 → 에코 제거(esp-sr) → Opus 압축 → 프레임 포장 → 보내기 대기열 → (송신 태스크) 교환기
//   받는 길: (웹소켓 태스크) 프레임 → 지터 버퍼 → (해제 태스크) Opus 해제·메우기 → 링버퍼 → (출력 태스크) audio_out 의 둘째 공급자
//   통화 밖에서는 세 태스크가 모두 잠들어 있고 마이크를 읽지 않는다(PH-D6: 받기 전에는 마이크가 열리지 않는다).
#pragma once
#include <cstdint>

#include "voice_core.h"
#include "voice_tune.h"

namespace voice {

// 부팅 때 한 번(audioout::start·toneout::start 뒤, app::start 앞). 에코 제거·Opus·버퍼를 만들고 태스크 셋을 띄운다(잠든 채),
// audio_out 에 공급자로, relay_link 에 음성 받는 함수로 등록한다. 소리 장치가 없으면(audioout::ready 가 거짓) 아무것도 하지 않고 false.
// 준비가 실패하면 audioout::fail("voice") 로 알리고 false — 전화기는 소리 없이 계속 돈다.
bool start();

// 통화가 열렸다(true) / 끝났다(false). app 태스크가 상태 기계의 A_VOICE_START·A_VOICE_STOP 에서 부른다. 바로 돌아온다(기다리지 않는다).
// 돌려주는 값 = 상태가 실제로 바뀌었나(app 이 "VOICE on|off" 줄을 찍을지). 준비가 안 됐으면 늘 false.
bool set(bool on);
bool on();
uint32_t rx_frames();                       // 이번 통화에서 받은 음성 프레임 수(잠금 없이 — app 태스크의 수신 무음 감시가 20ms 마다 읽는다, PL3c-D10). 준비가 안 됐으면 0

// ── 진단(시리얼 명령 — 사람이 켠다) ──
bool set_source(voicecore::Src s);          // 통화 중에만: 무엇을 보낼지(aec = 평소). 통화가 끝나면 aec 로 돌아간다(스피커가 켜져 있으면 mic·ref 는 쓰이지 않고 aec 로 간다 — PL3b-D22). 통화 중이 아니면 false
// 실행 중 조절(PL3b-D23): 보내는 쪽 디지털 이득 1~4배. 저장하지 않는다(재부팅하면 4배). 통화 안팎 어느 때나. 범위 밖이면 false 와 값은 그대로.
bool set_tx_gain(int n);
int tx_gain();

// ── Phase 3c 통화 품질 손잡이(PL3c-D3). 저장하지 않는다(재부팅하면 voice_tune.h 의 DEFAULT_*). 통화 안팎 어느 때나. 범위 밖이면 false 와 값은 그대로 ──
bool set_kbps(int kbps);                    // Opus 비트율 12~40kbps. 캡처 태스크가 다음 묶음에서 압축기에 건다(통화 밖이면 다음 통화의 첫 묶음에서). 압축기가 거절하면 앞 값으로 되돌아간다
bool set_lvl(int max_gain);                 // 보내는 크기 평탄화: 0 = 끔(고정 이득), 6~16 = 말소리 이득의 상한(배) — tx_dyn.h
bool set_tilt(int n);                       // 보내는 소리의 고음 올리기: 0 = 끔, 1·2 — tx_dyn.h
bool set_res(int db);                       // 남은 메아리 누르기: 0·6·12·18dB — tx_dyn.h
bool set_guard(int n);                      // P4H-D1 메아리 지킴: 0 = 끔, 1~16 = 되먹임 칸 세기의 n ÷ 4 배보다 커야 내 말로 쳐서 키운다 — tx_dyn.h
int guard();                                // 지금 값(TUNE 줄에는 싣지 않는다 — `guard` 명령이 답한다)
// P5A (2026-10-08, voice_tune.h 의 설명): 셋 다 guard 와 같은 방식의 손잡이다 — 저장하지 않고, TUNE·STAT 줄에 싣지 않고, 같은 이름의 명령이 지금 값을 답한다.
bool set_rest(int db);                      // 쉼 누르기: 0·6·12·18dB — 상대 소리도 내 말도 없을 때 보내는 소리를 누른다(tx_dyn.h)
int rest();
bool set_dt(int db);                        // 겹침 누르기: 0·6·12dB — 상대 소리가 나는 동안에는 내 말로 보이는 것도 누른다(tx_dyn.h)
int dt();
bool set_rxgain(int percent);               // 받는 목소리의 재생 이득 100~300%(꼭대기는 부드럽게 누른다 — voice_core.h Receiver). 준비가 안 됐으면 false
int rxgain();
bool set_hpf(int on);                       // 저음 걷어내기(보내는 소리의 200Hz 아래): 0·1 — tx_dyn.h
int hpf();
bool set_jb(int cap);                       // 지터 버퍼 규칙: 0 = 3b 그대로, 3~10 = 새 규칙 + 목표 상한(프레임). 통화 중에도 바로 바뀐다 — jitter_buffer.h JB-D3
bool set_vtone(int hz);                     // 시험음(vsrc tone·probe)의 주파수 300~7000Hz. selftest 의 시험음은 늘 1kHz
struct Tune {                               // TUNE 줄에 싣는 것(phone_main 이 음량·마이크 증폭·nodelay·절전 상태를 더해 찍는다)
    int kbps, lvl, tilt, res_db, jb, vtone; // 손잡이의 지금 값
    int gain_q8;                            // 평탄화가 지금 걸고 있는 이득(256 = 1배). 손잡이(lvl·res)가 둘 다 꺼져 있으면 0
    uint32_t supp, far;                     // 이번(또는 직전) 통화에서: 남은 메아리를 누른 묶음 수 / 상대 소리가 나던 묶음 수(묶음 = 16ms)
    uint32_t rxgap_ms;                      // 이번(또는 직전) 통화에서 받은 프레임 사이의 가장 긴 틈(ms) — 망이 멈췄다 풀린 길이
    uint32_t mic_clip;                      // 이번(또는 직전) 통화에서 마이크 칸이 꼭대기에 닿은 표본 수(스피커 소리가 마이크에서 넘친다 → mic 손잡이를 내릴 근거)
};
void tune(Tune *out);

bool loop(bool on);                         // 통화 밖에서만: 되돌림 시험 — 통화의 소리 길을 상대 없이 돌린다(보낼 프레임이 교환기 대신 내 받는 길로 간다 — voice.cpp VL-D1). 통화가 시작되면 저절로 끝난다. 통화 중이거나 이미 되돌림 중에 켜면 false, 되돌림 중이 아닌데 끄면 false
bool looping();
bool ping();                                // 통화 중에만: 지연 재기 한 번. 결과는 print_pending 이 "VPING rt_ms=…" 또는 "VPING timeout" 으로 찍는다
struct SelfTest { uint32_t mic, ref, aec; };
bool selftest(SelfTest *out);               // 통화 밖에서만: 시험음을 0.64초 내며 마이크 칸(mic)·되먹임 칸(ref)·에코 제거 뒤(aec)의 세기를 잰다. 약 0.7초 기다린다

void print_pending();                       // app 태스크가 20ms 바퀴마다: 지연 재기의 결과 줄("VPING rt_ms=N jb=깊이/목표" 또는 "VPING timeout")을 찍는다(다른 태스크는 찍지 않는다)

// STAT 줄의 목소리 칸. 셈과 시간은 "이번(또는 직전) 통화"의 것이다 — 통화가 열릴 때 0 으로 돌아간다.
struct Stat {
    bool ready, on; voicecore::Src src;
    uint32_t cap;                           // 마이크에서 읽은 묶음 수(부팅 뒤 누적 — 통화 밖에서는 늘지 않아야 한다)
    uint32_t tx, tx_drop, tx_fail;          // 보낸 프레임 / 밀려서 버린 프레임 / 보내기 실패
    voicecore::RxStats rx;
    uint32_t aec_us, aec_max, enc_us, enc_max, dec_us, dec_max;   // 묶음(16ms)·프레임(20ms) 하나를 처리한 시간의 평균·최대(µs)
    uint32_t mic_rms, ref_rms, aec_rms;     // 최근 1초의 세기: 마이크 칸 / 되먹임 칸 / 에코 제거 뒤
    uint32_t rd_err;                        // 마이크 읽기 실패
    unsigned stk_cap, stk_rx, stk_tx;       // 태스크 스택 최저 여유(바이트)
    uint32_t under_ms;                      // PL3b-D25: 이번 통화에서 첫 v_under 가 난 때(통화 시작 뒤 ms). 없었으면 0
    int tx_gain;                            // PL3b-D23: 지금의 보내는 이득(배)
    unsigned stk_enc;                       // (voice.cpp VB-D1) 압축 태스크 스택의 최저 여유(바이트). 압축을 따로 떼지 않는 기판(Tab5)에서는 0
    uint32_t enc_drop;                      // (VB-D1) 압축 태스크가 밀려 버린 16ms 묶음 수(이번 통화). Tab5 에서는 늘 0
};
void stat(Stat *out);

}  // namespace voice
