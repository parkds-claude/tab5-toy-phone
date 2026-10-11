// test_howl_loop.cpp — 두 기기 고리 모형: tx_dyn.h 를 양쪽에 그대로 넣고 스피커 → 마이크 → 상대 스피커 → 상대 마이크 → … 를 닫는다.
//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_howl_loop.cpp -o /tmp/t_howl_loop && /tmp/t_howl_loop
//   /tmp/t_howl_loop --info   → 훑은 칸마다의 결과(· 조용함 / H 울림 / x 고정 이득도 울림)를 찍는다
// 왜: 2026-10-05 14:1x 다른 방 통화에서 "약한 하울링"(771b9fe · lvl 12). 옛 빌드(고정 4배)에서는 보고가 없었다.
//     평탄화(lvl)는 "상대 소리가 난다"를 되먹임 칸 세기 150 이상으로만 보았다 — 그 아래로 작게 나는 상대 소리의 남은 메아리가
//     말소리 문턱(세기 40)을 넘으면 내 말로 보고 12배까지 키운다. 그 구간에서만 고리 이득이 고정 4배 때보다 +9.5dB(한쪽) 오른다.
// 모형이 쓰는 숫자와 그 근거(로그가 뒷받침하는 것 / 가정인 것):
//   · 되먹임 칸 = 재생 PCM 의 0.415배                       — 로그 p3d_erle: v_ref 551 / v_spk 1372, 569 / 1358
//   · 스피커 → 마이크 결합(마이크 ÷ 재생 PCM): 1번 2.33배, 2번 0.855배 — 로그 p3d_erle: v_mic 3193 / v_spk 1372, 1168 / 1366
//     (고정 4배를 곱하면 한쪽 +19.4dB · +10.7dB = 인계문의 "한쪽 결합 +19dB · +10.7dB")
//   · 방의 잡음(에코 제거 뒤) 세기 약 17                     — 로그 p3q_lvl_speech: 말 없는 구간 v_aec 17~20
//   · 큰 소리에서 메아리 제거 28~36dB                         — 로그 p3d_erle(고른 시험음 · 되먹임 칸 550 근처)
//   · 한쪽 지연 150~300ms                                     — 계획서의 어림(VPING 왕복 ÷ 2)
//   · [가정] 작은 소리(되먹임 칸 150 아래)에서는 메아리 제거가 10~25dB 뿐이다. 150~550 사이는 dB 로 곧게 잇는다.
//   · 큰 소리에서도 자리 잡기 전에는 덜 지운다 — 로그 p3q_r6 의 selftest(시험음 0.64초 · 되먹임 칸 569 · 592): mic 3421 → aec 665(14.2dB), 1305 → 202(16.2dB).
//     그래서 "큰 소리의 감소"를 셋으로 훑는다: 28dB · 36dB(자리 잡은 뒤) · 작은 소리와 같음(자리 잡기 전·말소리 — 얼마나 오래 이런지는 [가정]).
//   · [가정] 남은 메아리는 같은 묶음의 재생 소리를 줄인 것이다(실제로는 모양이 다르다 — 고리 이득을 따지는 데는 세기면 된다).
// 판정 셋(칸마다 — 칸 = 두 기기의 작은 소리 감소 × 큰 소리 감소 × 지연 × 말하는 쪽):
//   ① 끝 1초(말이 끝나고 5~6초 뒤)에 스피커로 나가는 소리가 "방 잡음 × 고정 이득"의 2배 아래면 조용하다. 고정 이득(lvl 0)에서도 조용하지 않은 칸은
//      평탄화의 탓이 아니다(고리 자체가 1을 넘는다) — 훑는 범위에서 뺀다.
//   ② 말이 끝나고 0.5초 뒤부터는(말 끊김 유지 0.2초가 지난 뒤) 누구의 이득도 고정 이득 위에 있지 않다 = 고리에 남은 소리(메아리·잡음)를 키우지 않는다
//      = 아무도 말하지 않을 때의 고리 이득이 고정 4배 때와 같다. ← 771b9fe 가 어기는 것(이 시험이 먼저 실패로 보인 것)
//   ③ 말이 끝나고 1~2초 뒤의 스피커 세기가 고정 이득 때의 3.2배(+10dB = 키운 말의 메아리가 그만큼 큰 것까지)와 판정선 가운데 큰 쪽을 넘지 않는다.
// 이 모형이 보이지 못한 것: 771b9fe 에서도 끝 1초까지 이어지는 울림은 훑은 어느 칸에서도 나오지 않았다(①은 고치기 전에도 통과).
//   모형이 보인 것은 "말이 끝난 뒤 1.5~2초 동안 남은 소리를 12배 가까이로 계속 키운다"(②③)까지다 — 사용자가 들은 "약한 하울링"이 이것인지는 기기에서 확인해야 한다.
#include "tx_dyn.h"
#include "voice_tune.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace txdyn;
static int ok = 0, bad = 0;
#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)

static const double PI = 3.14159265358979323846;
static const int RATE = 16000;
static const double REF_RATIO = 0.415;                  // 되먹임 칸 ÷ 재생 PCM
static const double COUPLE_1 = 2.33, COUPLE_2 = 0.855;  // 마이크 ÷ 재생 PCM (1번 · 2번 기기)
static const double REF_SMALL = 150, REF_BIG = 550;     // [가정] 이 아래는 "작은 소리"의 감소, 이 위는 "큰 소리"의 감소
static const int NOISE_AMP = 30;                        // 고른 잡음 ±30 → 세기 약 17.6
static const double NOISE_RMS = 17.6, QUIET_SPK = 2 * 4 * NOISE_RMS;   // 조용함의 판정선(재생 PCM 세기 약 141)

static int16_t speech_at(long i, int peak_amp)          // test_tx_dyn.cpp 와 같은 말소리 비슷한 것: 0.2초 소리 + 0.1초 쉼
{
    const long k = i % (RATE * 3 / 10);
    if (k >= RATE / 5) return 0;
    const double env = 0.5 - 0.5 * std::cos(2 * PI * k / (RATE / 5));
    return (int16_t)std::lround(peak_amp * env * (0.6 * std::sin(2 * PI * 300 * i / RATE) + 0.4 * std::sin(2 * PI * 1200 * i / RATE)));
}
static int16_t sat(double v) { return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : std::lround(v)); }

struct Cfg {
    double erle_lo[2];      // 작은 소리에서의 메아리 감소(dB): [0] = 1번 기기, [1] = 2번 기기
    double erle_hi;         // 큰 소리에서의 감소(dB). 0 = 작은 소리와 같다(크기와 무관하게 덜 지운다)
    int delay;              // 한쪽 지연(묶음 — 16ms)
    int talker;             // 말하는 쪽: 0 = 1번, 1 = 2번
    int peak;               // 말소리의 꼭대기(에코 제거 뒤)
    double vol;             // 재생 이득(1.0 = vol 100)
};
struct Out {
    double tail[2];         // 마지막 1초의 재생 PCM 세기(기기마다)
    double heard;           // 말하는 동안(끝 1.5초) 듣는 쪽 스피커의 세기
    double late;            // 말이 끝나고 1~2초 뒤의 스피커 세기(두 기기 중 큰 쪽)
    int late_gain;          // 말이 끝나고 0.5초 뒤부터 끝까지 걸린 가장 큰 이득(Q8, 두 기기 중 큰 쪽)
    int talk_gain;          // 말하는 동안 말하는 쪽에 걸린 가장 큰 이득(Q8)
    bool quiet() const { return tail[0] < QUIET_SPK && tail[1] < QUIET_SPK; }
};
static const double T_TALK0 = 1.0, T_TALK1 = 4.0, T_END = 10.0;   // 1초 잡음만 → 3초 말 → 6초 말 없음

static Out loop(const Cfg &c, const Params &p)
{
    const int blocks = (int)(T_END * RATE / BLOCK);
    const double couple[2] = {COUPLE_1, COUPLE_2};
    Dyn dyn[2];
    std::vector<std::vector<int16_t>> sent[2];          // 기기마다 보낸 묶음들
    uint32_t seed[2] = {12345u, 67890u};
    double env[2] = {0, 0}, tail_sq[2] = {0, 0}, late_sq[2] = {0, 0}, heard_sq = 0; long tail_n = 0, heard_n = 0, late_n = 0;
    Out o = {};
    for (int b = 0; b < blocks; ++b) {
        int16_t out[2][BLOCK];
        for (int u = 0; u < 2; ++u) {
            double spk[BLOCK]; double sq = 0;
            const int from = b - c.delay;
            for (int i = 0; i < BLOCK; ++i) { spk[i] = from >= 0 ? sent[1 - u][from][i] * c.vol : 0.0; sq += spk[i] * spk[i]; }
            const double spk_rms = std::sqrt(sq / BLOCK), ref = REF_RATIO * spk_rms;
            env[u] = ref > env[u] ? ref : env[u] * 0.875;                                  // 감소를 고르는 데 쓰는 되먹임 칸의 어림
            const double t = env[u] <= REF_SMALL ? 0 : env[u] >= REF_BIG ? 1 : std::log(env[u] / REF_SMALL) / std::log(REF_BIG / REF_SMALL);
            const double erle = std::pow(10.0, (c.erle_lo[u] + t * ((c.erle_hi > 0 ? c.erle_hi : c.erle_lo[u]) - c.erle_lo[u])) / 20);
            const double t_s = (double)b * BLOCK / RATE;
            const bool talking = u == c.talker && t_s >= T_TALK0 && t_s < T_TALK1;
            for (int i = 0; i < BLOCK; ++i) {
                seed[u] = seed[u] * 1664525u + 1013904223u;
                const int noise = (int)((seed[u] >> 8) % (2 * NOISE_AMP + 1)) - NOISE_AMP;
                out[u][i] = sat(spk[i] * couple[u] / erle + noise + (talking ? speech_at((long)b * BLOCK + i, c.peak) : 0));
            }
            dyn[u].process(out[u], (uint32_t)std::lround(ref), p);
            if (t_s >= T_END - 1) { tail_sq[u] += sq; if (u == 0) tail_n += BLOCK; }
            if (t_s >= T_TALK1 + 1 && t_s < T_TALK1 + 2) { late_sq[u] += sq; if (u == 0) late_n += BLOCK; }
            if (t_s >= T_TALK1 + 0.5 && dyn[u].gain_q8() > o.late_gain) o.late_gain = dyn[u].gain_q8();
            if (talking && dyn[u].gain_q8() > o.talk_gain) o.talk_gain = dyn[u].gain_q8();
            if (u != c.talker && t_s >= T_TALK1 - 1.5 && t_s < T_TALK1) { heard_sq += sq; heard_n += BLOCK; }
        }
        for (int u = 0; u < 2; ++u) sent[u].emplace_back(out[u], out[u] + BLOCK);
    }
    for (int u = 0; u < 2; ++u) o.tail[u] = std::sqrt(tail_sq[u] / (tail_n ? tail_n : 1));
    o.late = std::sqrt(std::max(late_sq[0], late_sq[1]) / (late_n ? late_n : 1));
    o.heard = std::sqrt(heard_sq / (heard_n ? heard_n : 1));
    return o;
}

// 부팅 값 그대로의 손잡이(voice_tune.h) — 이 시험이 지키는 것은 "기기가 켜졌을 때의 값"이다
// 2026-10-05(P4H)의 부팅 값: lvl 12 · res 0 · tilt 0 · guard 6. 아래 P4H 시험들은 그때의 결정(메아리 지킴 guard)을 그때의 값으로 계속 지킨다
//   — 부팅 값이 바뀌어도(2026-10-08 P5A: res 12 · tilt 1 · guard 8 + 쉼·겹침 누르기) 이 시험들이 본 것은 흔들리지 않는다.
static Params p4h(int guard = 6) { return {4 * 256, 12, 0, 0, guard}; }
// 지금의 부팅 값(voice_tune.h) — "기기가 켜졌을 때의 값". 맨 아래 P5A 절이 지킨다
static Params boot()
{
    Params p = {voicetune::DEFAULT_TXGAIN * 256, voicetune::DEFAULT_LVL, voicetune::DEFAULT_RES_DB, voicetune::DEFAULT_TILT, voicetune::GUARD_DEFAULT};
    p.rest_db = voicetune::REST_DEFAULT; p.dt_db = voicetune::DT_DEFAULT; p.hpf = voicetune::HPF_DEFAULT;
    return p;
}
static Params fixed() { return {voicetune::DEFAULT_TXGAIN * 256, 0, 0, 0}; }                // lvl 0 = 고정 4배(5935ddd 의 소리)

static const double LO[] = {10, 13, 16, 19, 22, 25}, HI[] = {0, 28, 36};                     // 작은 소리의 감소(기기마다) / 큰 소리의 감소(0 = 작은 소리와 같음)
static const int DELAY[] = {9, 14, 19};                                                     // 한쪽 150 · 225 · 300ms
static const int CELLS = 6 * 6 * 3 * 3 * 2;
static std::vector<Cfg> cells()
{
    std::vector<Cfg> v;
    for (double h : HI) for (int d : DELAY) for (int talker = 0; talker < 2; ++talker) for (double l1 : LO) for (double l2 : LO) v.push_back({{l1, l2}, h, d, talker, 600, 1.0});
    return v;
}
struct Sweep { int cases = 0, valid = 0, ring = 0, late_boost = 0, late_loud = 0, boosted = 0; double heard_ratio_min = 1e9, heard_ratio_sum = 0; int worst_gain = 0; Cfg worst_cfg = {}; };
// base = 칸마다의 고정 이득(lvl 0) 결과(한 번만 셈한다). 고정 이득에서도 끝 1초가 조용하지 않은 칸은 평탄화의 탓이 아니므로 뺀다
static Sweep sweep(const std::vector<Cfg> &cs, const std::vector<Out> &base, const Params &p, bool print)
{
    Sweep s;
    for (size_t k = 0; k < cs.size(); ++k) {
        const Cfg &c = cs[k]; const Out &f = base[k];
        if (print && k % 36 == 0) printf("  큰 소리 %s · 지연 %dms · 말하는 쪽 %d번   (가로 = 2번의 작은 소리 감소 10~25dB, 세로 = 1번)\n", c.erle_hi > 0 ? (c.erle_hi > 30 ? "36dB" : "28dB") : "작은 소리와 같음", c.delay * 16, c.talker + 1);
        if (print && k % 6 == 0) printf("    1번 %2.0fdB  ", c.erle_lo[0]);
        ++s.cases;
        if (f.quiet()) {
            ++s.valid;
            const Out o = loop(c, p);
            const bool ring = !o.quiet(), late_boost = o.late_gain > p.base_q8, late_loud = o.late > std::max(QUIET_SPK, 3.2 * f.late);
            if (ring) ++s.ring;
            if (late_boost) { ++s.late_boost; if (o.late_gain > s.worst_gain) { s.worst_gain = o.late_gain; s.worst_cfg = c; } }
            if (late_loud) ++s.late_loud;
            if (o.talk_gain > p.base_q8 * 3 / 2) ++s.boosted;
            s.heard_ratio_min = std::min(s.heard_ratio_min, o.heard / f.heard); s.heard_ratio_sum += o.heard / f.heard;
            if (print) printf("%s ", ring ? "H" : late_boost && late_loud ? "B" : late_boost ? "b" : "·");
        } else if (print) printf("x ");
        if (print && k % 6 == 5) printf("\n");
    }
    return s;
}
static void print_sweep(const char *what, const Sweep &s)
{
    printf("%s: 칸 %d, 고정 이득이 조용한 칸 %d → 끝까지 울림 %d, 말이 끝난 뒤에도 키움 %d(가장 큰 이득 %d/256배), 1~2초 뒤 소리가 큼 %d, 말하는 쪽을 1.5배 넘게 키운 칸 %d, 듣는 크기(고정 대비) 최소 %.2f배 · 평균 %.2f배\n",
           what, s.cases, s.valid, s.ring, s.late_boost, s.worst_gain, s.late_loud, s.boosted, s.heard_ratio_min, s.valid ? s.heard_ratio_sum / s.valid : 0);
    if (s.late_boost) printf("   가장 크게 키운 칸: 1번 %.0fdB · 2번 %.0fdB · 큰 소리 %.0fdB · 지연 %dms · 말하는 쪽 %d번\n",
                             s.worst_cfg.erle_lo[0], s.worst_cfg.erle_lo[1], s.worst_cfg.erle_hi > 0 ? s.worst_cfg.erle_hi : s.worst_cfg.erle_lo[0], s.worst_cfg.delay * 16, s.worst_cfg.talker + 1);
}

int main(int argc, char **argv)
{
    const bool info = argc > 1 && !strcmp(argv[1], "--info");

    // ── 모형이 로그와 맞는가 ───────────────────────────────────────────────
    {
        // 고리를 끊고(vol 0) 한쪽만 말한다 = p3q_lvl_speech.log 의 조건(상대 스피커 꺼짐): 듣는 쪽이 받는 크기가 lvl 0 의 약 2.8배
        const Cfg open = {{16, 16}, 28, 14, 1, 600, 0.0};
        Cfg heard = open; heard.vol = 1.0;
        Cfg c = heard; c.erle_lo[0] = c.erle_lo[1] = 60; c.erle_hi = 60;                     // 메아리가 사실상 없다(듣는 쪽 스피커는 켜져 있다)
        const double a = loop(c, fixed()).heard, b = loop(c, p4h()).heard;
        CHECK("model vs log (p3q_lvl_speech): with no echo, the boot values deliver quiet speech 2.2x - 3.2x louder than the fixed gain", b / a > 2.2 && b / a < 3.2);
        (void)open;
    }

    // ── 고리: 부팅 값에서, 고정 이득이 조용한 모든 칸 ────────────────────────
    if (info) printf("\n-- 칸마다(부팅 값): · 좋음 / b 말이 끝난 뒤에도 키움 / B 키우고 소리도 큼 / H 끝까지 울림 / x 고정 이득도 울림(범위 밖) --\n");
    const std::vector<Cfg> cs = cells();
    std::vector<Out> base; for (const Cfg &c : cs) base.push_back(loop(c, fixed()));
    const Sweep now = sweep(cs, base, p4h(), info);
    CHECK("the sweep is not empty: the fixed gain (lvl 0) is quiet in at least 250 of the 648 cells (small-signal echo loss 10-25 dB each side; loud 28 dB, 36 dB or no better than small; 150-300 ms; either talker)",
          now.cases == CELLS && (int)cs.size() == CELLS && now.valid >= 250);
    CHECK("boot values: the loop is quiet again 5 s after the talker stops in every cell where the fixed gain is quiet (no cell keeps ringing)", now.ring == 0);
    CHECK("boot values: from 0.5 s after the talker stops nobody's gain is above the fixed gain in any cell - what is left in the loop (echo, room noise) is never boosted", now.late_boost == 0);
    CHECK("boot values: 1-2 s after the talker stops the loudspeakers are no louder than 3.2x the fixed-gain run (or the quiet line) in any cell", now.late_loud == 0);
    if (info || now.ring || now.late_boost || now.late_loud) { printf("\n"); print_sweep("부팅 값", now); }
    if (info) for (int g : {0, 2, 4, 6, 8, 12, 16}) { char name[32]; snprintf(name, sizeof name, "guard %2d", g); print_sweep(name, sweep(cs, base, p4h(g), false)); }

    // ── 손잡이 guard(P4H-D1): 0 은 옛 동작, 부팅 값은 메아리만으로는 키우기를 켜지 않는다 ──
    const Sweep old = sweep(cs, base, p4h(0), false);
    CHECK("guard 0 is the 771b9fe behaviour: in at least 20 cells the gain is still above the fixed gain 0.5 s after the talker stopped (the fault this build fixes - and proof the model can see it)",
          old.late_boost >= 20 && old.worst_gain > 2 * 1024 && old.ring == 0);
    CHECK("guard 2 (half the loopback level) is not enough in this model: some cells still boost what is left in the loop", sweep(cs, base, p4h(2), false).late_boost > 0);
    CHECK("the boot value costs little loudness in the closed loop: the listener still gets the talker at least 0.9x as loud as with guard 0, averaged over the cells (and never below the fixed gain)",
          now.heard_ratio_sum >= 0.9 * old.heard_ratio_sum && now.heard_ratio_min > 0.99 && now.boosted >= 150);

    const Params g0 = p4h(0), gb = p4h();
    auto quiet_speech = [](long i) { return speech_at(i, 600); };
    auto run = [](Dyn &d, const Params &p, double seconds, auto near, auto ref, std::vector<int> *gains = nullptr) {
        std::vector<int16_t> out; const long blocks = (long)(seconds * RATE / BLOCK);
        for (long b = 0; b < blocks; ++b) {
            int16_t blk[BLOCK];
            for (int i = 0; i < BLOCK; ++i) blk[i] = near(b * BLOCK + i);
            d.process(blk, ref(b), p);
            if (gains) gains->push_back(d.gain_q8());
            out.insert(out.end(), blk, blk + BLOCK);
        }
        return out;
    };
    auto rms = [](const std::vector<int16_t> &v) { double q = 0; for (int16_t x : v) q += (double)x * x; return std::sqrt(q / (v.empty() ? 1 : v.size())); };
    auto tone = [](int amp) { return [amp](long i) { return (int16_t)std::lround(amp * std::sin(2 * PI * 700 * i / RATE)); }; };
    {
        // 내 작은 말로 12배를 배운 뒤, 상대의 작은 소리(되먹임 칸 100 — 문턱 150 아래)의 남은 메아리(세기 80 = 되먹임 칸의 0.8배)만 2초 들어온다
        Dyn a, b; std::vector<int> ga, gb2;
        run(a, g0, 5, quiet_speech, [](long) { return 0u; }); run(b, gb, 5, quiet_speech, [](long) { return 0u; });
        const int learned = b.gain_q8();
        run(a, g0, 2, tone(113), [](long) { return 100u; }, &ga); run(b, gb, 2, tone(113), [](long) { return 100u; }, &gb2);
        CHECK("leftover echo of a quiet far end (loopback 100 < 150, echo 0.8x the loopback): guard 0 boosts it like speech (above x10)", *std::max_element(ga.begin() + 13, ga.end()) > 10 * 256);
        CHECK("... the boot value never does: once my own hang (0.2 s) is over, every block goes out at the fixed gain", learned > 10 * 256 && *std::max_element(gb2.begin() + 13, gb2.end()) == 1024);
        // 그 소리가 그친다: 창(0.2초) 안의 꼬리도 키우지 않고, 그 뒤 내 말은 다시 커진다
        std::vector<int> tail, back;
        run(b, gb, 0.128, tone(113), [](long) { return 0u; }, &tail);
        run(b, gb, 0.4, [](long) { return (int16_t)0; }, [](long) { return 0u; });
        run(b, gb, 0.6, [&](long i) { return quiet_speech(i + 800); }, [](long) { return 0u; }, &back);
        CHECK("... its tail (8 blocks after the loopback fell silent) is still not boosted - the 0.2 s window remembers the loudspeaker", *std::max_element(tail.begin(), tail.end()) == 1024);
        CHECK("... and my own quiet speech is boosted again right after (the learned gain is back within 0.25 s)", back[15] >= learned * 9 / 10);
    }
    {
        // 상대 소리가 나다가(되먹임 칸 400, 남은 메아리 300 = 어림의 2.5배를 넘어 "내 말"로 보인다) 작아진다(100): 상대 소리 때의 판정이 남긴 말 끊김 유지가 꼬리를 키우지 않는다
        Dyn a, b; std::vector<int> ga, gb2;
        for (Dyn *d : {&a, &b}) { const Params &p = d == &a ? g0 : gb; run(*d, p, 5, quiet_speech, [](long) { return 0u; }); run(*d, p, 1, tone(424), [](long) { return 400u; }); }
        run(a, g0, 0.5, tone(113), [](long) { return 100u; }, &ga); run(b, gb, 0.5, tone(113), [](long) { return 100u; }, &gb2);
        CHECK("a loud far end fading to a quiet one: guard 0 lets the 'my speech' verdict of the loud part carry the boost onto the tail (above x8), the boot value keeps every block at the fixed gain",
              *std::max_element(ga.begin(), ga.end()) > 8 * 256 && *std::max_element(gb2.begin(), gb2.end()) <= 1024);
    }
    {
        // 지켜야 하는 것 ①: 상대가 조용할 때(되먹임 칸 18 = 상대 방의 잡음 — 로그 p3q_lvl_speech 의 16~18)의 내 작은 말은 옛 판과 표본까지 같게 커진다
        Dyn a, b;
        const std::vector<int16_t> x = run(a, g0, 6, quiet_speech, [](long) { return 18u; }), y = run(b, gb, 6, quiet_speech, [](long) { return 18u; });
        CHECK("kept: with the far end idle (loopback 18, its room noise) my quiet speech goes out bit for bit as with guard 0 - boosted to x12", x == y && b.gain_q8() == 12 * 256);
        // ②: 말 없는 구간의 잡음은 키우지 않는다(상대 잡음이 스피커에서 나고 있어도)
        Dyn n; uint32_t sd = 5; std::vector<int> g;
        run(n, gb, 5, [&](long) { sd = sd * 1664525u + 1013904223u; return (int16_t)((int)((sd >> 8) % 61) - 30); }, [](long) { return 18u; }, &g);
        CHECK("kept: room noise with the far end's noise in the loudspeaker is never boosted (fixed gain on every block)", *std::max_element(g.begin(), g.end()) == 1024 && *std::min_element(g.begin(), g.end()) == 1024);
        // ③: 동시에 말하기 — 상대의 작은 소리(되먹임 칸 100) 위의 내 말은 고정 이득 아래로 눌리지 않는다(뚝 끊기지 않는다). 보통 크기의 말은 그대로 다듬는다
        Dyn q, qf, l, l0;
        const std::vector<int16_t> dq = run(q, gb, 4, quiet_speech, [](long) { return 100u; }, &g), df = run(qf, fixed(), 4, quiet_speech, [](long) { return 100u; });
        CHECK("kept: my quiet speech over a quiet far end (loopback 100) is never pressed under the fixed gain - at worst it is not boosted", rms(dq) >= rms(df) && *std::min_element(g.begin(), g.end()) >= 1024);
        const std::vector<int16_t> dl = run(l, gb, 4, [](long i) { return speech_at(i, 3000); }, [](long) { return 100u; }), d0 = run(l0, g0, 4, [](long i) { return speech_at(i, 3000); }, [](long) { return 100u; });
        CHECK("kept: my normal speech (peak 3000) over a quiet far end is levelled as with guard 0 - within 1 dB (it is far above anything the loudspeaker could leave)",
              std::fabs(20 * std::log10(rms(dl) / rms(d0))) < 1.0 && l.gain_q8() > 1024);
        if (info) printf("   내 보통 말(꼭대기 3000) · 되먹임 칸 100: guard 0 대비 %+.2f dB / 내 작은 말(꼭대기 600) · 되먹임 칸 100: 고정 이득 대비 %+.2f dB\n", 20 * std::log10(rms(dl) / rms(d0)), 20 * std::log10(rms(dq) / rms(df)));
    }
    {
        // 손잡이의 뜻: guard ÷ 4 배. 메아리가 되먹임 칸의 1.3배면 guard 4(1배)는 뚫리고 부팅 값(6 = 1.5배)은 막는다 — 1번 기기의 메아리 감소 13.3dB 에 해당한다(마이크가 되먹임 칸의 6배)
        Dyn a, b; std::vector<int> ga, gb2;
        run(a, p4h(4), 5, quiet_speech, [](long) { return 0u; }); run(b, gb, 5, quiet_speech, [](long) { return 0u; });
        run(a, p4h(4), 1, tone(184), [](long) { return 100u; }, &ga); run(b, gb, 1, tone(184), [](long) { return 100u; }, &gb2);
        CHECK("the knob is in quarters of the loopback level: echo at 1.3x the loopback gets through guard 4 (1x) but not through guard 6 (1.5x - the 2026-10-05 boot value)",
              *std::max_element(ga.begin() + 13, ga.end()) > 8 * 256 && *std::max_element(gb2.begin() + 13, gb2.end()) == 1024);
        CHECK("the knob's range: 0 (off) to 16 (4x), the boot value inside it", voicetune::ok_guard(0) && voicetune::ok_guard(16) && !voicetune::ok_guard(17) && !voicetune::ok_guard(-1) && voicetune::ok_guard(voicetune::GUARD_DEFAULT) && voicetune::GUARD_MAX == 16);
    }

    // ── P5A (2026-10-08 "통화음이 작다 · 다른 건물인데도 하울링이 조금 있다"): 648칸 전부에서 — 고정 이득이 울리는 칸도 빼지 않는다 ──
    //   위의 P4H 시험은 "고정 이득(lvl 0)에서도 울리는 칸은 평탄화의 탓이 아니다"라며 그 칸들을 범위에서 뺐다. 사용자가 여전히 듣는 울림은 바로 그 칸들일 수 있다
    //   (고리 자체가 1 을 넘는다 — 메아리 제거가 약할 때). 그래서 여기서는 전부 센다. 재생 이득(Cfg::vol)은 받는 목소리 키우기(rxgain)다.
    {
        struct All { int ring = 0, strong = 0; double heard = 0; };
        auto all = [&cs](const Params &p, double vol) {
            All a;
            for (Cfg c : cs) {
                c.vol = vol;
                const Out o = loop(c, p);
                const double lim = QUIET_SPK * vol;                              // 조용함의 판정선도 재생 이득만큼 오른다(방 잡음이 그만큼 크게 난다)
                if (o.tail[0] >= lim || o.tail[1] >= lim) ++a.ring;
                if (o.tail[0] >= 1000 || o.tail[1] >= 1000) ++a.strong;           // 크게 울림(벨 크기의 1/8 을 넘는 소리가 끝까지 남는다)
                a.heard += o.heard;
            }
            a.heard /= (double)cs.size();
            return a;
        };
        const double rx = voicetune::RXGAIN_DEFAULT / 100.0;
        const All was = all(p4h(), 1.0), was_rx = all(p4h(), rx), now1 = all(boot(), 1.0), nowx = all(boot(), rx);
        const double louder_db = 20 * std::log10(nowx.heard / was.heard);
        CHECK("P5A: the model sees the complaint - with the 2026-10-05 boot values about half of all 648 cells are still ringing 5 s after the talker stops, some of them loudly",
              (int)cs.size() == CELLS && was.ring >= 300 && was.strong >= 30);
        CHECK("P5A: ... and only turning the playback up (rxgain alone) makes it worse", was_rx.ring > was.ring && was_rx.strong > 2 * was.strong);
        CHECK("P5A: today's boot values at today's playback gain: at most 12 of the 648 cells ring and none rings loudly", nowx.ring <= 12 && nowx.strong == 0);
        CHECK("P5A: with the playback gain back at 1x (rxgain 100) at most 2 cells ring", now1.ring <= 2 && now1.strong == 0);
        CHECK("P5A: the listener hears the talker at least 4.5 dB louder than with the 2026-10-05 boot values (averaged over all cells)", louder_db >= 4.5);
        Params tilted = boot(); tilted.tilt = 1;
        const All a_tilt = all(tilted, rx);
        CHECK("P5A: why tilt stays off at boot - with the treble lift on (tilt 1) the same values ring in at least 100 more cells (broadband noise gains 4 dB per lap)",
              voicetune::DEFAULT_TILT == 0 && a_tilt.ring >= nowx.ring + 100);
        Params no_dt = boot(); no_dt.dt_db = 0;
        Params no_rest = boot(); no_rest.rest_db = 0;
        const All a_dt = all(no_dt, rx), a_rest = all(no_rest, rx);
        CHECK("P5A: both new presses carry weight - without the overlap press (dt 0) or without the rest press (rest 0) clearly more cells ring", a_dt.ring >= nowx.ring + 10 && a_rest.ring >= nowx.ring + 10);
        if (info) {
            printf("\n-- P5A: 648칸 전부(울리는 칸 / 크게 울리는 칸 / 듣는 말소리) --\n");
            printf("   2026-10-05 부팅 값 · 재생 1배: %d / %d / 기준\n", was.ring, was.strong);
            printf("   2026-10-05 부팅 값 · 재생 %.1f배: %d / %d / %+.1f dB\n", rx, was_rx.ring, was_rx.strong, 20 * std::log10(was_rx.heard / was.heard));
            printf("   지금 부팅 값 · 재생 1배: %d / %d / %+.1f dB\n", now1.ring, now1.strong, 20 * std::log10(now1.heard / was.heard));
            printf("   지금 부팅 값 · 재생 %.1f배: %d / %d / %+.1f dB\n", rx, nowx.ring, nowx.strong, louder_db);
            printf("   (겹침 누르기 끔: %d / %d, 쉼 누르기 끔: %d / %d, 고음 올리기 켬: %d / %d)\n", a_dt.ring, a_dt.strong, a_rest.ring, a_rest.strong, a_tilt.ring, a_tilt.strong);
        }
    }

    printf("\n%d ok, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
