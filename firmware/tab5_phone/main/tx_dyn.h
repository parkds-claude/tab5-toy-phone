// tx_dyn.h — 보내는 소리의 크기 다듬기: 말소리 평탄화(작은 말은 키우고 큰 말은 누른다) + 남은 메아리 누르기. 하드웨어 무관(맥 시험: tools/test_tx_dyn.cpp).
//
// 계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md 의 결정 PL3c-D7. 에코 제거(esp-sr)를 거친 16ms 묶음을 받아 제자리에서 고친다.
//   왜: 2026-10-05 아침 실측 — 말소리가 에코 제거 뒤 RMS 70~279(−53~−41dBFS)로 작고, 3b 의 고정 4배로는 −41~−29dBFS 다(통화에 알맞은 크기는 −26~−20dBFS).
//       고정 이득을 더 올리면 큰 소리가 찢어지고 메아리도 그만큼 커진다. 1번 기기가 상대에게 돌려보내는 메아리는 −10.9dB 로 크다(2번은 −20.5dB).
//   세 손잡이(셋 다 0 이면 3b 의 "고정 이득 × 넘치면 자르기"와 표본 하나까지 같다 — 시험이 고정한다):
//     lvl  0 = 끔 / 6~16 = 말소리에 거는 이득의 상한(배). 말소리의 꼭대기를 TARGET_PEAK(−6dBFS)로 맞춘다. 이득은 1배 ~ 상한 사이에서 천천히 오르고 빨리 내린다.
//     tilt 0 = 끔 / 1·2 = 고음을 올린다(1차 필터 y = x + a·(x − 앞 표본), a = 0.5·1.0 → 1kHz +0.5·+1.1dB, 2kHz +1.6·+3.4dB, 4kHz +4.0·+7.0dB).
//          "먹먹하다"에 대한 손잡이다: 작은 스피커·먼 마이크·에코 제거가 깎는 고음을 보내는 쪽에서 미리 올린다. 값: 고음 쪽 잡음도 같이 오른다.
//     res  0 = 끔 / 6·12·18 = 상대 소리만 나고 내 말이 없을 때 보내는 소리를 이만큼(dB) 누른다.
//     guard 0 = 끔(771b9fe 의 동작) / 1~16 = 메아리 지킴(P4H-D1): 지난 0.2초의 되먹임 칸 세기 × guard ÷ 4 보다 큰 묶음만 "내 말"로 쳐서 키운다.
//   P4H-D1 (2026-10-05 다른 방 통화의 "약한 하울링" — tools/test_howl_loop.cpp 의 두 기기 고리 모형):
//     왜: lvl 은 "상대 소리가 난다"를 되먹임 칸 세기 FAR_ON(150) 이상으로만 보았다. 그 아래로 작게 나는 소리(상대의 작은 말, 돌아온 내 목소리의 꼬리)의 남은 메아리가
//         말소리 문턱을 넘으면 내 말로 보고 lvl 배까지 키웠다 → 말이 끝난 뒤에도 고리에 남은 소리를 계속 키운다(모형: 말이 끝나고 1.5~2초 동안 이득이 고정 이득 위에 머문다).
//     어떻게: 스피커에서 나고 있는 소리의 세기(되먹임 칸)에 견줘 충분히 큰 묶음만 내 말이다. 남은 메아리는 되먹임 칸에 비례하므로(남은 메아리 = k × 되먹임 칸),
//         k < guard ÷ 4 인 한 메아리만으로는 키우기가 켜지지 않는다 → 아무도 말하지 않을 때의 고리 이득은 고정 이득(3b)과 같다.
//         말하는 동안에는 돌아오는 내 목소리(되먹임 칸)가 내 말의 4 ÷ guard 배를 넘지 않는 데까지만 키운다 = 한 바퀴 돈 소리가 원래의 k ÷ (guard ÷ 4) 배 아래다.
//     비용: 상대 기기가 내 목소리를 크게 되돌려 보내면(그 기기의 메아리 제거가 나쁘면) 내 말이 덜 커진다(고정 이득까지 — 3b 의 크기 아래로는 가지 않는다).
//           상대 쪽이 시끄러우면(되먹임 칸이 내 말의 4 ÷ guard 배를 넘으면) 내 작은 말이 커지지 않는다. 판정이 묶음마다 갈려 이득이 오르내릴 수 있다(말 끊김 유지 0.2초가 메운다).
//     탈출구: 실행 중 `guard 0`(771b9fe 와 표본까지 같다 — 시험이 고정) · 부팅 값은 voice_tune.h 의 GUARD_DEFAULT 한 곳.
//   지키는 것(시험이 하나씩 고정한다):
//     · 넘치지 않는다 — lvl 이 켜져 있으면 KNEE 위는 부드럽게 눌러 ±32767 안에 둔다(돌지 않고, 딱 잘리지도 않는다).
//     · 소리가 없을 때 잡음을 끌어올리지 않는다 — 말소리가 아닌 묶음의 이득은 고정 이득(base)을 넘지 않는다.
//     · 상대가 말하는 동안에는 이득이 고정 이득을 넘지 않는다 → 스피커 → 마이크 고리의 이득이 3b 보다 커지는 일이 없다(하울링 여유는 그대로이거나 res 만큼 는다).
//       단, 상대 소리가 시작되는 첫 묶음(16ms)은 배운 이득에서 고정 이득으로 내려오는 중이다(표본마다 내려가고 오르지 않는다 — 한 번에 내리면 딸깍 소리가 난다).
//       둘째 묶음부터는 어느 표본도 넘지 않는다(1덩어리 검수 Minor 1 — 시험 "inside that one block (16 ms)").
//     · 동시에 말하기 — 상대 소리가 나는 중에도 내 말이 "남은 메아리의 어림 × 2.5"보다 크면 누르지 않는다. 누르던 중이면 한 묶음(16ms) 안에 푼다.
//   비용: 판단이 한 묶음(16ms) 늦다(말의 첫 16ms 는 앞 묶음의 이득으로 나간다). 상대 소리가 나는 동안의 작은 말소리(남은 메아리 어림의 2.5배 아래)는 res 만큼 눌린다.
//         에코 제거가 아직 자리 잡지 못한 동안(통화 첫 1~2초, 기기를 옮긴 직후)에는 남은 메아리가 어림보다 커서 "내 말"로 보인다 → 누르지 않는다(3b 와 같은 소리).
//   되돌리기: lvl 0 · tilt 0 · res 0.
#pragma once
#include <cstdint>

#include "pcm.h"

namespace txdyn {

constexpr int BLOCK = 256;                      // 한 묶음(16ms @16kHz) — voicecore::BLOCK 과 같다(voice.cpp 의 static_assert)
constexpr int TARGET_PEAK = 16000;              // 말소리 꼭대기의 목표(−6dBFS). 말소리의 RMS 는 꼭대기의 1/4~1/5 → 약 −19dBFS
constexpr int KNEE = 20000, CEIL = 32767;       // KNEE 위는 부드럽게 눌러 CEIL 을 넘지 않는다
constexpr uint32_t FAR_ON = 150;                // 되먹임 칸의 세기가 이 이상이면 "상대 소리가 나고 있다"(되먹임 칸 = 코덱 출력의 0.42배 — 시험음 2000 이 590)
constexpr int FAR_HOLD = 12;                    // 상대 소리가 그친 뒤 이만큼(약 0.2초)은 방의 울림이 남아 있다고 본다
constexpr int HANG = 12;                        // 말이 끊긴 뒤 이만큼(약 0.2초)은 말소리 이득을 유지한다(낱말 끝이 잘리지 않게)
constexpr uint32_t RHO_INIT = 4096, RHO_MAX = 16384;    // 남은 메아리 ÷ 되먹임 칸(Q16). 시작 0.0625(−24dB — 실측 0.037~0.063), 위 한계 0.25
constexpr uint32_t DT_MARGIN_Q8 = 640;          // 내 말로 보려면 남은 메아리 어림의 2.5배
constexpr int GUARD_WIN = FAR_HOLD;               // P4H-D1: 되먹임 칸의 "지난 0.2초 중 가장 큰 세기"를 보는 창(묶음) — 상대 소리 유지(FAR_HOLD)와 같은 길이
constexpr uint32_t GATE_MIN = 40, NOISE_INIT = 20, NOISE_MAX = 400;   // 말소리로 치는 세기의 바닥 / 바닥 잡음 어림의 처음 값·위 한계(에코 제거 뒤의 RMS)

struct Params {
    int base_q8;        // 고정 이득(txgain × 256)
    int lvl_max;        // 0 = 평탄화 끔, 6~16 = 말소리 이득의 상한(배)
    int res_db;         // 0 = 끔, 6·12·18
    int tilt;           // 0 = 끔, 1·2 = 고음 올리기
    int guard = 0;      // P4H-D1 메아리 지킴: 0 = 끔(771b9fe), 1~16 = 되먹임 칸 세기의 guard ÷ 4 배보다 커야 내 말(값을 주지 않으면 0 — 옛 시험은 옛 동작을 그대로 본다)
    int rest_db = 0;    // P5A-D1 쉼 누르기: 0 = 끔 / 6·12 = 상대 소리도 내 말도 없을 때 보내는 소리를 이만큼(dB) 누른다(값을 주지 않으면 0 = 옛 동작)
    int dt_db = 0;      // P5A-D2 겹침 누르기: 0 = 끔 / 6·12 = 상대 소리가 나는 동안에는 "내 말"로 보이는 것도 이만큼(dB) 누른다(값을 주지 않으면 0 = 옛 동작)
    int hpf = 0;        // P5A-D4 저음 걷어내기: 0 = 끔 / 1 = 200Hz 아래를 걷어낸다(2차 고역 통과 — 작은 스피커가 못 내는 저음·퍽 소리·웅 소리. 값을 주지 않으면 0 = 옛 동작)
};
constexpr int tilt_a_q8(int tilt) { return tilt <= 0 ? 0 : tilt == 1 ? 128 : 256; }       // 필터의 a(Q8): 0, 0.5, 1.0
// P5A-D4 저음 걷어내기: 2차 버터워스 고역 통과, 꺾이는 곳 200Hz @16kHz (100Hz −12dB, 200Hz −3dB, 300Hz −0.8dB, 500Hz 위는 0.1dB 안쪽).
//   y = B0·x + B1·x1 + B2·x2 − A1·y1 − A2·y2. 계수: w = 2π·200/16000, Q = 0.7071 (RBJ 쿡북 식)
constexpr float HPF_B0 = 0.945979f, HPF_B1 = -1.891957f, HPF_B2 = 0.945979f, HPF_A1 = -1.889033f, HPF_A2 = 0.894874f;

// |v| 가 KNEE 를 넘으면 부드럽게 눌러 CEIL 아래에 둔다(기울기가 KNEE 에서 1 로 이어진다 — 꺾이는 자리에 계단이 없다). 부호는 그대로.
inline int16_t soft_clip(int32_t v)
{
    const int32_t a = v < 0 ? -v : v;
    if (a <= KNEE) return (int16_t)v;
    const int64_t room = CEIL - KNEE, d = (int64_t)a - KNEE;
    const int32_t y = KNEE + (int32_t)(room * d / (d + room));
    return (int16_t)(v < 0 ? -y : y);
}

class Dyn {
public:
    Dyn() { reset(); }
    void reset()                                 // 통화가 열릴 때
    {
        g_q8_ = 0; env_ = 0; rho_ = RHO_INIT; noise_q4_ = NOISE_INIT * 16; ref_env_ = 0; prev_x_ = 0;
        far_hold_ = 0; hang_ = 0; suppressed_ = 0; far_blocks_ = 0; speech_blocks_ = 0; rested_ = 0;
        own_ = 0; win_at_ = 0; for (uint32_t &v : ref_win_) v = 0;
        hx1_ = hx2_ = hy1_ = hy2_ = 0;
    }

    // blk = 에코 제거를 거친 한 묶음(BLOCK 표본) → 제자리에서 보낼 소리로. ref_rms = 같은 묶음의 되먹임 칸 세기.
    void process(int16_t *blk, uint32_t ref_rms, const Params &p)
    {
        if (p.hpf) {                                         // P5A-D4: 맨 먼저(고음 올리기·세기 재기 앞) — 저음이 평탄화의 크기 판단을 흔들지 않게
            for (int i = 0; i < BLOCK; ++i) {
                const float x = (float)blk[i];
                const float y = HPF_B0 * x + HPF_B1 * hx1_ + HPF_B2 * hx2_ - HPF_A1 * hy1_ - HPF_A2 * hy2_;
                hx2_ = hx1_; hx1_ = x; hy2_ = hy1_; hy1_ = y;
                blk[i] = pcm::sat16((int32_t)(y < 0 ? y - 0.5f : y + 0.5f));
            }
        } else {
            hx1_ = hx2_ = (float)blk[BLOCK - 1]; hy1_ = hy2_ = 0;   // 꺼져 있는 동안에는 "고른 입력"의 상태로 둔다(켜는 순간 큰 계단이 없다)
        }
        if (const int a = tilt_a_q8(p.tilt)) {              // 고음 올리기: 이득을 걸기 전에(넘치면 여기서는 그냥 자른다 — 에코 제거 뒤의 소리는 작아서 닿지 않는다)
            for (int i = 0; i < BLOCK; ++i) { const int32_t x = blk[i]; blk[i] = pcm::sat16(x + (x - prev_x_) * a / 256); prev_x_ = x; }
        } else {
            prev_x_ = blk[BLOCK - 1];                       // 꺼져 있는 동안에도 앞 표본은 따라간다(켜는 순간 계단이 없다)
        }
        if (p.lvl_max == 0 && p.res_db == 0 && p.rest_db == 0 && p.dt_db == 0) {   // 평탄화·누르기가 다 꺼져 있다 = 3b 의 고정 이득
            pcm::gain_q8(blk, BLOCK, p.base_q8);
            g_q8_ = 0;                                      // 다시 켜면 처음부터(고정 이득에서 시작한다)
            return;
        }
        const int32_t base = p.base_q8;
        if (g_q8_ == 0) { g_q8_ = base; env_ = (uint32_t)((int64_t)TARGET_PEAK * 256 / base); }     // 처음: 고정 이득에서 시작
        // ── 재기 ──
        uint64_t sum = 0; int32_t pk = 0;
        for (int i = 0; i < BLOCK; ++i) { const int32_t a = blk[i] < 0 ? -(int32_t)blk[i] : blk[i]; if (a > pk) pk = a; sum += (uint64_t)((int64_t)a * a); }
        const uint32_t r = pcm::isqrt64(sum / BLOCK);
        // ── 상대 소리가 나고 있나 ──
        const bool far_now = ref_rms >= FAR_ON;
        if (far_now) far_hold_ = FAR_HOLD; else if (far_hold_ > 0) --far_hold_;
        const bool far = far_hold_ > 0;
        ref_env_ = ref_rms > ref_env_ ? ref_rms : ref_env_ - ref_env_ / 8;          // 울림이 남는 동안의 어림(한 묶음에 1/8 씩 준다)
        if (far) ++far_blocks_;
        // P4H-D1: 지난 0.2초(이 묶음 포함)에 스피커에서 난 가장 큰 소리 → 그 guard ÷ 4 배 아래의 묶음은 스피커에서 온 소리일 수 있다(문턱 FAR_ON 아래의 작은 소리도 본다)
        ref_win_[win_at_] = ref_rms; win_at_ = (win_at_ + 1) % GUARD_WIN;
        uint32_t ref_hi = 0; for (const uint32_t v : ref_win_) if (v > ref_hi) ref_hi = v;
        const uint64_t echo_cap = (uint64_t)ref_hi * (uint32_t)(p.guard > 0 ? p.guard : 0) / 4;
        // ── 내 말이 있나 ──
        bool near;
        if (far) {
            if (far_now) {                                                          // 남은 메아리의 비율: 내려갈 때는 바로, 올라갈 때는 천천히(겹쳐 말하는 동안 부풀지 않게)
                const uint64_t q = ((uint64_t)r << 16) / ref_rms;
                if (q < rho_) rho_ = (uint32_t)((rho_ + q) / 2);
                else { rho_ += rho_ / 128 + 1; if (rho_ > RHO_MAX) rho_ = RHO_MAX; }
            }
            const uint64_t echo = ((uint64_t)rho_ * ref_env_) >> 16;                // 지금 남아 있을 메아리의 세기
            const uint32_t noise = noise_q4_ >> 4;
            uint64_t need = echo * DT_MARGIN_Q8 / 256;
            if (need < 3ull * noise) need = 3ull * noise;
            if (need < GATE_MIN) need = GATE_MIN;
            near = r > need;
        } else {
            // 바닥 잡음의 어림(1/16 눈금): 내려갈 때는 바로(말 사이의 쉼에서 방의 잡음으로 내려온다), 올라갈 때는 아주 천천히 — 한 묶음에 1/256(1초에 1.28배).
            //   고르게 이어지는 소리(선풍기·시험음)는 약 7초 뒤에 잡음으로 배운다. 그 전까지는 말소리로 친다(말에는 1초 안에 쉼이 있다).
            const uint32_t r_q4 = r << 4;
            if (r_q4 < noise_q4_) noise_q4_ = (noise_q4_ + r_q4) / 2;
            else { noise_q4_ += noise_q4_ / 256 + 1; if (noise_q4_ > NOISE_MAX * 16) noise_q4_ = NOISE_MAX * 16; }
            const uint32_t noise = noise_q4_ >> 4;
            const uint32_t gate = 3 * noise > GATE_MIN ? 3 * noise : GATE_MIN;
            near = r > gate && r > echo_cap;                                        // P4H-D1: 잡음보다 크고, 스피커에서 올 수 있는 소리보다도 크다(guard 0 이면 echo_cap = 0 — 옛 판정)
        }
        if (near) { hang_ = HANG; ++speech_blocks_; } else if (hang_ > 0) --hang_;
        // P4H-D1: 키우기를 허락하는 남은 묶음 수. 상대 소리가 나는 동안의 "내 말" 판정(남은 메아리 어림의 2.5배)은 메아리가 넘는 일이 잦아 키우기의 근거로 쓰지 않는다 —
        //   그 판정이 채운 말 끊김 유지(hang_)가 상대 소리가 그친 직후의 메아리 꼬리를 키우는 길을 막는다. guard 0 이면 옛 동작(hang_ 그대로).
        if (p.guard <= 0) own_ = hang_;
        else if (far) own_ = 0;
        else if (near) own_ = HANG;
        else if (own_ > 0) --own_;
        // ── 말소리의 꼭대기를 따라간다(상대 소리가 없을 때만 — 메아리를 말소리로 배우지 않는다) ──
        if (near && !far) env_ = (uint32_t)pk > env_ ? (uint32_t)pk : env_ - env_ / 64;       // 말하는 동안 한 묶음에 1/64 씩 내려간다(약 0.7초에 반 — 이득은 천천히 오른다)
        if (near && (uint32_t)pk > env_) env_ = (uint32_t)pk;                        // 큰 소리는 어느 때든 바로 따라간다(이득을 내리는 쪽)
        if (env_ < 1) env_ = 1;
        // ── 이 묶음의 이득 ──
        int32_t speech_q8 = base;
        if (p.lvl_max > 0) {
            const int64_t want = (int64_t)TARGET_PEAK * 256 / env_;
            const int32_t top = p.lvl_max * 256;
            speech_q8 = want > top ? top : want < 256 ? 256 : (int32_t)want;
        }
        const int32_t quiet_q8 = speech_q8 < base ? speech_q8 : base;              // 말소리가 아닌 것·상대가 말하는 동안: 고정 이득을 넘지 않는다
        int32_t target;
        bool pressed = false;
        if (far) {
            target = quiet_q8;
            if (!near && hang_ == 0 && p.res_db > 0) { target >>= p.res_db / 6; pressed = true; }     // 6dB = 반
            else if (p.dt_db > 0) target >>= p.dt_db / 6;                                    // P5A-D2: 내 말로 보여도(겹쳐 말하기 — 또는 자리 잡기 전의 큰 남은 메아리) 조금 누른다
        } else {
            target = own_ > 0 ? speech_q8 : quiet_q8;
            if (own_ == 0 && hang_ == 0 && p.rest_db > 0) { target >>= p.rest_db / 6; ++rested_; }   // P5A-D1: 아무도 말하지 않는 쉼 — 방의 잡음과 고리에 남은 소리를 누른다
        }
        if (pressed) ++suppressed_;
        const bool resting = !far && own_ == 0 && hang_ == 0 && p.rest_db > 0;
        // ── 옮겨 가기: 내릴 때는 이 묶음 안에, 올릴 때는 고정 이득까지는 이 묶음 안에(누르던 것을 푼다), 그 위로는 한 묶음에 1/4 씩 ──
        int32_t next = target;
        if (target > g_q8_) {
            const int32_t fast_to = target < quiet_q8 ? target : quiet_q8;
            int32_t up = g_q8_ < fast_to ? fast_to : g_q8_;
            if (target > up) { up += up / 4 + 1; if (up > target) up = target; }
            next = up;
        } else if (resting && target < g_q8_) {
            // P5A-D1: 쉼으로는 천천히 내려간다 — 고정 이득까지는 바로(말소리 이득을 잡음에 걸어 두지 않는다), 그 아래로는 한 묶음에 1/8 씩(12dB 에 약 0.2초).
            //   한 번에 내리면 말끝마다 방의 잡음이 뚝 끊겨 숨 쉬듯 들린다(고전 스피커폰은 대기로 돌아가는 데 0.3~1초를 쓴다).
            int32_t down = g_q8_ > quiet_q8 ? quiet_q8 : g_q8_;
            down -= down / 8 + 1;
            next = down < target ? target : down;
        }
        const bool soft = p.lvl_max > 0;
        for (int i = 0; i < BLOCK; ++i) {
            const int32_t g = g_q8_ + (int32_t)((int64_t)(next - g_q8_) * (i + 1) / BLOCK);
            const int64_t v = (int64_t)blk[i] * g / 256;
            blk[i] = soft ? soft_clip(v > 0x3FFFFFFF ? 0x3FFFFFFF : v < -0x3FFFFFFF ? -0x3FFFFFFF : (int32_t)v) : pcm::sat16((int32_t)(v > 0x3FFFFFFF ? 0x3FFFFFFF : v < -0x3FFFFFFF ? -0x3FFFFFFF : v));
        }
        g_q8_ = next;
    }

    int gain_q8() const { return g_q8_; }                    // 지금 이득(Q8). 0 = 손잡이가 꺼져 있다
    uint32_t suppressed() const { return suppressed_; }      // 남은 메아리를 누른 묶음 수(통화마다)
    uint32_t far_blocks() const { return far_blocks_; }      // 상대 소리가 나던 묶음 수
    uint32_t speech_blocks() const { return speech_blocks_; }
    uint32_t rested() const { return rested_; }               // 쉼 누르기가 걸린 묶음 수(통화마다)

private:
    int32_t g_q8_, prev_x_;
    uint32_t env_, rho_, noise_q4_, ref_env_;
    int far_hold_, hang_;
    int own_, win_at_;                      // P4H-D1: 키우기를 허락하는 남은 묶음 수 / ref_win_ 의 다음 자리
    uint32_t ref_win_[GUARD_WIN];           // P4H-D1: 지난 GUARD_WIN 묶음의 되먹임 칸 세기
    uint32_t suppressed_, far_blocks_, speech_blocks_, rested_;
    float hx1_, hx2_, hy1_, hy2_;            // P5A-D4: 저음 걷어내기의 앞 표본 둘(입력·출력)
};

}  // namespace txdyn
