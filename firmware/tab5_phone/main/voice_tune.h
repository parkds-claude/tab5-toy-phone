// voice_tune.h — 통화 품질 손잡이: 기본값(부팅 값), 범위, 값 읽기. 하드웨어와 무관한 순수 코드(맥에서 단위 시험: tools/test_voice_tune.cpp).
//
// 계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md 의 결정 PL3c-D3.
//   손잡이 = 실행 중에 시리얼 명령으로 바꿔 들어 보는 값(저장하지 않는다 — 재부팅하면 아래 기본값). 3b 의 txgain·vol 과 같은 방식이다.
//   이유: "먹먹하다·작다·메아리·지연"의 원인 후보마다 다시 빌드·플래시하지 않고 그 자리에서 바꿔 듣고 재려는 것(판정 P3B-R17).
//   기본값은 손잡이를 넣기 전(5935ddd)의 동작과 같게 시작한다(LEGACY_*) — 회귀와 개선을 가를 수 있게.
//   굳히기 과제는 아래 DEFAULT_* 아홉 줄의 숫자만 고친다(draft/verify/freeze_tune.py 가 범위를 확인하며 고친다) — 굳힌 뒤에도 시리얼로 LEGACY_* 값을 주면 5935ddd 의 동작이 그대로 나온다.
//   비용: 진단 명령이 여덟 개 늘었다(Phase 4 의 "진단 명령 묶기"에 함께 넣는다). 되돌리기: DEFAULT_* 를 LEGACY_* 로.
#pragma once
#include "board_profile.h"
#include <cstdint>

namespace voicetune {

// ── 5935ddd 의 동작(손잡이를 넣기 전의 고정값) ─────────────────────────────────
constexpr int LEGACY_TXGAIN = 4, LEGACY_VOL = 100, LEGACY_MIC_DB = 24, LEGACY_KBPS = 24, LEGACY_LVL = 0, LEGACY_TILT = 0, LEGACY_RES_DB = 0, LEGACY_JB = 0, LEGACY_NODELAY = 0;

// ── 기본값(부팅 값). 굳히기 과제는 이 아홉 줄의 숫자만 고친다 ──────────────────
constexpr int DEFAULT_TXGAIN  = 4;      // 보내는 이득(배) — 평탄화(lvl)가 꺼져 있을 때의 고정 이득이자, 켜져 있을 때 "말소리가 아닌 것"에 거는 이득
constexpr int DEFAULT_VOL     = 100;    // 코덱 재생 음량(%)
constexpr int DEFAULT_MIC_DB  = 24;     // 마이크 증폭(dB)
constexpr int DEFAULT_KBPS    = 24;     // Opus 비트율(kbps)
constexpr int DEFAULT_LVL     = 12;      // 보내는 크기 평탄화: 0 = 끔(고정 이득), 6~16 = 말소리에 거는 이득의 상한(배)
constexpr int DEFAULT_TILT    = 0;      // 보내는 소리의 고음 올리기: 0 = 끔, 1 = 조금(4kHz +4dB), 2 = 많이(4kHz +7dB) — P5A 에서 1 을 재 봤으나 고리 모형의 울리는 칸이 6 → 200 이라 0 그대로
constexpr int DEFAULT_RES_DB  = 18;      // 남은 메아리 누르기(dB): 0 = 끔 — 2026-10-08 P5A: 0 → 18("다른 건물인데도 하울링이 조금 있다")
constexpr int DEFAULT_JB      = 0;      // 지터 버퍼: 0 = 3b 의 규칙 그대로, 3~10 = 새 규칙 + 목표 상한(프레임)
constexpr int DEFAULT_NODELAY = 0;      // 교환기 소켓의 TCP_NODELAY
// 기판마다 부팅 값이 다른 손잡이(board_profile.h — 2026-10-08 전화기 3대): 마이크 증폭. 위의 DEFAULT_MIC_DB 는 Tab5 의 값이고(굳히기 도구가 고치는 줄),
//   스택짱(ESP32-S3)은 12dB 다 — 24dB 는 그 기판에서 넘친다(관문 실험). 부팅 때 거는 값·"전부 부팅 값인가"(is_default)는 BOOT_MIC_DB 를 쓴다. 맥 시험에서는 Tab5 의 값이다.
constexpr int BOOT_MIC_DB = boardprofile::STACKCHAN ? boardprofile::MIC_DB : DEFAULT_MIC_DB;

// ── 범위 ──────────────────────────────────────────────────────────────────────
constexpr int TXGAIN_MIN = 1, TXGAIN_MAX = 4;
constexpr int VOL_MIN = 0, VOL_MAX = 100;
// 마이크 증폭은 3b 의 값(24dB)보다 올리지 못한다: 올리면 스피커 → 마이크 고리 이득이 그만큼 오르고, 1번 기기는 24dB 에서도 큰 소리의 메아리가 마이크에서 넘친다(PL3c-D9).
constexpr int MIC_DB_MIN = 12, MIC_DB_MAX = 24, MIC_DB_STEP = 3;
// Opus 비트율: 부품의 범위는 6~510kbps(esp_opus_enc_set_bitrate). 위를 40 으로 묶는 이유는 PL3c-D8(압축 출력 버퍼·캡처 태스크 스택).
constexpr int KBPS_MIN = 12, KBPS_MAX = 40;
constexpr int LVL_MIN = 6, LVL_MAX = 16;            // 0(끔)은 따로
constexpr int TILT_MAX = 2;                         // 0, 1, 2
constexpr int RES_DB_STEP = 6, RES_DB_MAX = 18;     // 0, 6, 12, 18
constexpr int JB_MIN = 3, JB_MAX = 10;              // 0(옛 규칙)은 따로
constexpr int VTONE_MIN = 300, VTONE_MAX = 7000, VTONE_DEFAULT = 1000;   // 시험음 주파수(Hz). 표본율 16kHz 의 절반(8kHz) 아래
// P4H-D1 메아리 지킴(tx_dyn.h — 평탄화가 고리에 남은 소리를 키우지 않게): 0 = 끔(771b9fe 의 동작), 1~16 = 되먹임 칸 세기의 guard ÷ 4 배보다 큰 묶음만 "내 말"로 쳐서 키운다.
//   부팅 값 6(= 1.5배)의 근거 — tools/test_howl_loop.cpp 의 두 기기 고리 모형(고정 이득이 조용한 306칸): 0 은 27칸 · 2 는 23칸에서 말이 끝난 뒤에도 남은 소리를 키우고, 4 부터 0칸.
//     4(1배)는 1번 기기(마이크가 되먹임 칸의 6배를 줍는다 — 로그 p3d_erle)의 메아리 감소가 15.6dB 아래면 뚫리고, 6 은 12dB 까지 버틴다(selftest 의 자리 잡기 전 값이 14.2dB).
//     단, 모형의 메아리 감소 10~25dB·150~550 구간 직선 연결은 **가정**이다(실측은 selftest 14.2·16.2dB, 고른 시험음 28~36dB) — 사용자 통화로 확인.
//     비용: 모형에서 듣는 쪽이 받는 말소리의 크기(고정 이득 대비, 칸 평균)가 1.61배(0) → 1.54배(4) → 1.47배(6) → 1.36배(8) → 1.16배(12). 상대 스피커가 꺼져 있으면(메아리 없음) 어느 값이든 2.8배 그대로.
//     기기에서: 아직 울리면 `guard 8` → `guard 12`, 소리가 다시 작으면 `guard 4`. 저장되지 않는다(재부팅하면 이 값).
//   손잡이 아홉 개(Knobs)에는 넣지 않았다(vtone 과 같은 자리) — TUNE 의 dflt·legacy 는 이 값을 보지 않는다(lvl 0 이면 이 값은 아무 일도 하지 않는다). 지금 값은 `guard` 명령이 답한다.
//   2026-10-08 P5A: 부팅 값 6 → 8(2배). 재생을 2배로 키우면(RXGAIN_DEFAULT) 돌아오는 소리도 커진다 — 고리 모형(648칸, res 12·rest 12·dt 12·재생 2배)에서 6 은 울리는 칸 28, 8 은 4.
constexpr int GUARD_MAX = 16, GUARD_DEFAULT = 8;

// P5A (2026-10-08 사용자: "통화음이 작은 편이다 · 공간이 아예 달라도(집-사무실) 하울링이 조금 있다") — 계획 docs/exec-plans/2026-10-08-tab5-phone-audio-p5a.md
//   왜 울리나(tools/test_howl_loop.cpp 의 고리 모형, 648칸 전부): 메아리 제거가 약한 칸에서는 고정 4배만으로도 두 기기를 한 바퀴 돈 소리가 1배를 넘는다
//     (2026-10-05 의 부팅 값: 648칸 중 329칸이 말이 끝난 5초 뒤에도 방 잡음의 2배 위 — 그때는 "고정 이득도 울리는 칸"이라 범위에서 뺐던 칸들이다).
//   어떻게: ① 쉼 누르기(rest) — 상대 소리도 내 말도 없을 때 보내는 소리를 누른다 → 아무도 말하지 않을 때의 고리 이득이 양쪽 합쳐 2 × rest dB 낮다(작은 울림이 자라지 못한다)
//           ② 겹침 누르기(dt) — 상대 소리가 나는 동안에는 "내 말"로 보이는 것도 누른다 → 메아리 제거가 자리 잡기 전의 큰 남은 메아리가 내 말로 보여도 고리가 1 을 넘지 않는다
//           ③ 남은 메아리 누르기(res, 위의 DEFAULT_RES_DB 18) ④ 받는 목소리 키우기(rxgain) — 누르기로 번 여유를 듣는 크기에 쓴다. 꼭대기는 부드럽게 눌러 넘치지 않는다.
//   모형의 결과(648칸): 2026-10-05 부팅 값은 울리는 칸 329 · 크게 울리는 칸 48. 그 값에 재생만 2배로 하면 534 · 219. 지금 부팅 값 + 재생 2배는 6 · 0 이고
//     듣는 말소리는 +6.0dB(2026-10-05 부팅 값·재생 1배 대비, 칸 평균). 재생을 1배로 두면 0 · 0 · +2.2dB. 고음 올리기(tilt 1)를 함께 켜면 200 · 105 로 다시 울린다
//     (보내는 소리의 넓은 대역 잡음이 한 바퀴마다 +4dB) — 그래서 tilt 는 0 으로 두고 손잡이로만 남긴다(울림이 없는 것을 귀로 확인한 뒤 `tilt 1` 을 켜 본다).
//   비용: 쉼에는 방의 잡음이 rest dB 작게 간다(말을 시작하면 16ms 안에 풀린다 — 잡음이 숨 쉬듯 들릴 수 있다). 겹쳐 말하면 서로 dt dB 작게 들린다(받는 쪽이 6dB 키우므로 예전보다 약 6dB 작다).
//         모형의 메아리 감소 범위(작은 소리 10~25dB)는 여전히 [가정]이다 — 귀로 확인해야 한다.
//   탈출구: 실행 중 `rest 0` · `dt 0` · `res 0` · `rxgain 100` · `guard 6` · `hpf 0` 이면 2026-10-05 의 소리 그대로. 부팅 값은 아래 네 줄과 위의 DEFAULT_RES_DB·GUARD_DEFAULT.
//   손잡이 아홉 개(Knobs)에는 넣지 않았다(guard 와 같은 자리 — TUNE·STAT 줄은 그대로). 지금 값은 `rest` · `dt` · `rxgain` 명령이 답한다.
constexpr int REST_STEP = 6, REST_MAX = 18, REST_DEFAULT = 12;          // 쉼 누르기(dB): 0 = 끔, 6·12·18
constexpr int DT_STEP = 6, DT_MAX = 12, DT_DEFAULT = 12;                // 겹침 누르기(dB): 0 = 끔, 6·12
constexpr int RXGAIN_MIN = 100, RXGAIN_MAX = 300, RXGAIN_DEFAULT = 200; // 받는 목소리의 재생 이득(%): 100 = 그대로, 200 = 2배(+6dB). 신호음(벨·연결음)에는 걸리지 않는다
constexpr int HPF_DEFAULT = 1;                                          // 저음 걷어내기(보내는 소리, 200Hz 아래): 0 = 끔, 1 = 켬 — 작은 스피커가 못 내는 저음을 버려 또렷하게(P5A-D4)

inline bool ok_txgain(int v) { return v >= TXGAIN_MIN && v <= TXGAIN_MAX; }
inline bool ok_vol(int v) { return v >= VOL_MIN && v <= VOL_MAX; }
inline bool ok_mic_db(int v) { return v >= MIC_DB_MIN && v <= MIC_DB_MAX && v % MIC_DB_STEP == 0; }
inline bool ok_kbps(int v) { return v >= KBPS_MIN && v <= KBPS_MAX; }
inline bool ok_lvl(int v) { return v == 0 || (v >= LVL_MIN && v <= LVL_MAX); }
inline bool ok_tilt(int v) { return v >= 0 && v <= TILT_MAX; }
inline bool ok_res_db(int v) { return v >= 0 && v <= RES_DB_MAX && v % RES_DB_STEP == 0; }
inline bool ok_jb(int v) { return v == 0 || (v >= JB_MIN && v <= JB_MAX); }
inline bool ok_nodelay(int v) { return v == 0 || v == 1; }
inline bool ok_vtone(int v) { return v >= VTONE_MIN && v <= VTONE_MAX; }
inline bool ok_guard(int v) { return v >= 0 && v <= GUARD_MAX; }
inline bool ok_rest(int v) { return v >= 0 && v <= REST_MAX && v % REST_STEP == 0; }
inline bool ok_dt(int v) { return v >= 0 && v <= DT_MAX && v % DT_STEP == 0; }
inline bool ok_rxgain(int v) { return v >= RXGAIN_MIN && v <= RXGAIN_MAX; }
inline bool ok_hpf(int v) { return v == 0 || v == 1; }

// 굳힌 값이 범위를 벗어나면 빌드가 실패한다(잘못 굳힌 판이 기기에 올라가지 않게).
static_assert(TXGAIN_MIN <= DEFAULT_TXGAIN && DEFAULT_TXGAIN <= TXGAIN_MAX, "DEFAULT_TXGAIN out of range");
static_assert(VOL_MIN <= DEFAULT_VOL && DEFAULT_VOL <= VOL_MAX, "DEFAULT_VOL out of range");
static_assert(MIC_DB_MIN <= DEFAULT_MIC_DB && DEFAULT_MIC_DB <= MIC_DB_MAX && DEFAULT_MIC_DB % MIC_DB_STEP == 0, "DEFAULT_MIC_DB out of range");
static_assert(MIC_DB_MIN <= BOOT_MIC_DB && BOOT_MIC_DB <= MIC_DB_MAX && BOOT_MIC_DB % MIC_DB_STEP == 0, "BOOT_MIC_DB out of range");
static_assert(KBPS_MIN <= DEFAULT_KBPS && DEFAULT_KBPS <= KBPS_MAX, "DEFAULT_KBPS out of range");
static_assert(DEFAULT_LVL == 0 || (LVL_MIN <= DEFAULT_LVL && DEFAULT_LVL <= LVL_MAX), "DEFAULT_LVL out of range");
static_assert(DEFAULT_TILT >= 0 && DEFAULT_TILT <= TILT_MAX, "DEFAULT_TILT out of range");
static_assert(DEFAULT_RES_DB >= 0 && DEFAULT_RES_DB <= RES_DB_MAX && DEFAULT_RES_DB % RES_DB_STEP == 0, "DEFAULT_RES_DB out of range");
static_assert(DEFAULT_JB == 0 || (JB_MIN <= DEFAULT_JB && DEFAULT_JB <= JB_MAX), "DEFAULT_JB out of range");
static_assert(DEFAULT_NODELAY == 0 || DEFAULT_NODELAY == 1, "DEFAULT_NODELAY out of range");
static_assert(GUARD_DEFAULT >= 0 && GUARD_DEFAULT <= GUARD_MAX, "GUARD_DEFAULT out of range");
static_assert(REST_DEFAULT >= 0 && REST_DEFAULT <= REST_MAX && REST_DEFAULT % REST_STEP == 0, "REST_DEFAULT out of range");
static_assert(DT_DEFAULT >= 0 && DT_DEFAULT <= DT_MAX && DT_DEFAULT % DT_STEP == 0, "DT_DEFAULT out of range");
static_assert(RXGAIN_MIN <= RXGAIN_DEFAULT && RXGAIN_DEFAULT <= RXGAIN_MAX, "RXGAIN_DEFAULT out of range");

// 손잡이 아홉 개의 지금 값. STAT 이 아니라 TUNE 줄에 싣는다(기존 STAT 줄은 한 글자도 바꾸지 않는다).
struct Knobs {
    int txgain = DEFAULT_TXGAIN, vol = DEFAULT_VOL, mic_db = BOOT_MIC_DB, kbps = DEFAULT_KBPS;
    int lvl = DEFAULT_LVL, tilt = DEFAULT_TILT, res_db = DEFAULT_RES_DB, jb = DEFAULT_JB, nodelay = DEFAULT_NODELAY;
};
inline bool same(const Knobs &a, const Knobs &b)
{
    return a.txgain == b.txgain && a.vol == b.vol && a.mic_db == b.mic_db && a.kbps == b.kbps && a.lvl == b.lvl && a.tilt == b.tilt && a.res_db == b.res_db && a.jb == b.jb && a.nodelay == b.nodelay;
}
inline Knobs legacy()
{
    Knobs k;
    k.txgain = LEGACY_TXGAIN; k.vol = LEGACY_VOL; k.mic_db = LEGACY_MIC_DB; k.kbps = LEGACY_KBPS; k.lvl = LEGACY_LVL; k.tilt = LEGACY_TILT; k.res_db = LEGACY_RES_DB; k.jb = LEGACY_JB; k.nodelay = LEGACY_NODELAY;
    return k;
}
// dflt = 지금 값이 부팅 값과 같은가. legacy = 지금 값이 5935ddd 의 동작과 같은가(기본값을 굳히기 전의 빌드는 부팅 직후 둘 다 1, 굳힌 빌드는 dflt 만 1).
inline bool is_default(const Knobs &k) { return same(k, Knobs()); }
inline bool is_legacy(const Knobs &k) { return same(k, legacy()); }
// selftest 의 합격 판정(되먹임 칸 300 이상)과 vping 의 삑 찾기(1500 이상)는 "보내는 이득 4배 · 음량 100"을 전제로 한다(fix 2 재검수 Minor 3):
//   음량을 낮추면 되먹임 칸이 같이 작아지고(88 아래면 ok=0), 응답하는 기기의 이득이 낮으면 돌아온 삑이 문턱에 못 미친다(VPING timeout).
//   진단 출처(ref·mic)에는 평탄화·메아리 누르기를 걸지 않으므로(voice.cpp) 다른 손잡이는 이 판정을 흔들지 않는다.
inline bool meas_valid(const Knobs &k) { return k.txgain == LEGACY_TXGAIN && k.vol == LEGACY_VOL; }

// Opus 압축기가 요구하는 출력 버퍼 크기(바이트). 부품(esp_audio_codec 2.5.0)의 esp_opus_enc_set_bitrate 가 비트율을 바꿀 때 다시 셈하는 식 그대로다
//   (역어셈블: 프레임 표본 수 × 비트율 ÷ (표본율 × 8) + 100 — 24kbps·20ms·16kHz 에서 160 = 부팅 줄의 opus_out_max).
//   esp_opus_enc_process 는 넘겨준 출력 버퍼 길이가 이 값보다 작으면 그 프레임을 압축하지 않고 오류를 낸다 → 비트율을 올린 뒤에는 이 값을 다시 물어야 한다.
constexpr int opus_out_need(int kbps, int frame_samples = 320, int rate = 16000) { return frame_samples * kbps * 1000 / (rate * 8) + 100; }
constexpr int OPUS_OUT_CAP = 256;                   // 캡처 태스크의 압축 출력 버퍼. 40kbps 의 요구(200)를 넉넉히 담는다
static_assert(opus_out_need(KBPS_MAX) <= OPUS_OUT_CAP, "the encoder output buffer must hold the largest bitrate the knob allows");
static_assert(opus_out_need(24) == 160, "24 kbps / 20 ms / 16 kHz -> 60 + 100 (the value the device printed as opus_out_max)");

}  // namespace voicetune
