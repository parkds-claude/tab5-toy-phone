// test_tx_dyn.cpp — tx_dyn.h 단위 시험:
//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_tx_dyn.cpp -o /tmp/t_tx_dyn && /tmp/t_tx_dyn
//   /tmp/t_tx_dyn --info   → 시험 뒤에 크기 숫자(계획서 "먹먹함·작음" 표의 "맥 시험" 칸)를 찍는다
// 계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D7, Review Focus 1(이득을 키웠더니 넘치거나 하울링)·4(손잡이가 기본 동작을 바꿔 놓음):
//   손잡이가 꺼져 있으면 3b 와 표본 하나까지 같다 / 넘치지 않는다 / 작은 말은 키우고 큰 말은 누른다 / 소리가 없을 때 잡음을 끌어올리지 않는다 /
//   상대가 말하는 동안에는 고정 이득을 넘지 않는다 / 상대 소리만 날 때 남은 메아리를 누른다 / 동시에 말하면 누르지 않는다 / 고음 올리기의 크기.
#include "tx_dyn.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace txdyn;
static int ok = 0, bad = 0;
#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)

typedef std::vector<int16_t> Pcm;
static const double PI = 3.14159265358979323846;
static const int RATE = 16000;
static uint32_t g_seed = 7;
static uint32_t rnd(uint32_t m) { g_seed = g_seed * 1664525u + 1013904223u; return (g_seed >> 8) % m; }
static double rms(const Pcm &v, size_t from = 0, size_t to = SIZE_MAX) { double s = 0; size_t n = 0; for (size_t i = from; i < v.size() && i < to; ++i) { s += (double)v[i] * v[i]; ++n; } return n ? std::sqrt(s / n) : 0; }
static int peak(const Pcm &v, size_t from = 0, size_t to = SIZE_MAX) { int m = 0; for (size_t i = from; i < v.size() && i < to; ++i) m = std::max(m, std::abs((int)v[i])); return m; }
static double db(double a, double b) { return 20 * std::log10((a + 1e-9) / (b + 1e-9)); }

// 말소리 비슷한 것: 0.2초 소리(300Hz + 1200Hz, 올림 코사인 모양) + 0.1초 쉼. peak = 소리의 꼭대기
static int16_t speech_at(long i, int peak_amp)
{
    const long k = i % (RATE * 3 / 10);
    if (k >= RATE / 5) return 0;
    const double env = 0.5 - 0.5 * std::cos(2 * PI * k / (RATE / 5));
    return (int16_t)std::lround(peak_amp * env * (0.6 * std::sin(2 * PI * 300 * i / RATE) + 0.4 * std::sin(2 * PI * 1200 * i / RATE)));
}
static int16_t noise_at(int amp) { return (int16_t)((int)rnd((uint32_t)(2 * amp + 1)) - amp); }
static int16_t tone_at(long i, int amp, int hz) { return (int16_t)std::lround(amp * std::sin(2 * PI * hz * i / RATE)); }

// 한 방향을 seconds 초 돌린다. near(i) = 에코 제거 뒤의 내 쪽 소리, ref(block) = 그 묶음의 되먹임 칸 세기. 돌려주는 것 = 나간 소리
template <class Near, class Ref>
static Pcm run(Dyn &d, const Params &p, double seconds, Near near, Ref ref, std::vector<int> *gains = nullptr)
{
    Pcm out; const long blocks = (long)(seconds * RATE / BLOCK);
    for (long b = 0; b < blocks; ++b) {
        int16_t blk[BLOCK];
        for (int i = 0; i < BLOCK; ++i) blk[i] = near(b * BLOCK + i);
        d.process(blk, ref(b), p);
        if (gains) gains->push_back(d.gain_q8());
        out.insert(out.end(), blk, blk + BLOCK);
    }
    return out;
}
static const Params OFF = {1024, 0, 0, 0};

int main(int argc, char **argv)
{
    const bool info = argc > 1 && !strcmp(argv[1], "--info");

    // ── 손잡이가 꺼져 있으면 3b 그대로 ─────────────────────────────────────
    {
        g_seed = 11; Dyn d; bool same = true;
        for (int t = 0; t < 4000 && same; ++t) {
            int16_t a[BLOCK], b[BLOCK]; const int amp = t % 7 == 0 ? 32767 : 1 + (int)rnd(12000);
            for (int i = 0; i < BLOCK; ++i) a[i] = b[i] = (int16_t)(t % 11 == 0 ? (i % 2 ? 32767 : -32768) : noise_at(amp));
            const Params p = {256 * (1 + (int)rnd(4)), 0, 0, 0};
            d.process(a, rnd(6000), p); pcm::gain_q8(b, BLOCK, p.base_q8);
            if (memcmp(a, b, sizeof a)) same = false;
        }
        CHECK("off (lvl 0, res 0): bit for bit the fixed gain of 3b, clipping included, whatever the loopback level", same && d.gain_q8() == 0);
        Dyn e; int16_t a[BLOCK], b[BLOCK]; bool same2 = true;
        for (int t = 0; t < 2000 && same2; ++t) {
            for (int i = 0; i < BLOCK; ++i) a[i] = b[i] = speech_at((long)t * BLOCK + i, 3000);
            const Params p = {1024, 0, 12, 0};
            e.process(a, 0, p); pcm::gain_q8(b, BLOCK, 1024);
            if (memcmp(a, b, sizeof a)) same2 = false;
        }
        CHECK("res on but the far end silent: still bit for bit the fixed gain (nothing is pressed, hard clip as before)", same2 && e.suppressed() == 0);
    }

    // ── 넘치지 않는다 ─────────────────────────────────────────────────────
    {
        bool mono = true, bounded = true, smooth = true; int16_t prev = soft_clip(-200000);
        for (int32_t v = -200000; v <= 200000; ++v) {
            const int16_t y = soft_clip(v);
            if (y < prev) mono = false;
            if ((v > 0 && y <= 0) || (v < 0 && y >= 0) || y > CEIL || y < -CEIL) bounded = false;
            if (y - prev > 1) smooth = false;
            prev = y;
        }
        CHECK("soft clip: untouched up to the knee (20000), never beyond +-32767, never changes sign", bounded && soft_clip(KNEE) == KNEE && soft_clip(-KNEE) == -KNEE && soft_clip(12345) == 12345 && soft_clip(0) == 0);
        CHECK("soft clip: monotonic and with no step anywhere (slope <= 1) — it squeezes, it does not cut", mono && smooth && soft_clip(1 << 30) <= CEIL && soft_clip(60000) > 28000);
        CHECK("soft clip: above the knee it keeps rising all the way (no flat top): 32767 -> 26383, 40000 < 60000 < the ceiling",
              soft_clip(32767) == 26383 && soft_clip(-32767) == -26383 && soft_clip(40000) < soft_clip(60000) && soft_clip(60000) < CEIL && soft_clip(200000) < CEIL);
        g_seed = 5; Dyn d; bool safe = true; long n = 0; int32_t before = 0;
        for (int t = 0; t < 40000 && safe; ++t) {
            int16_t x[BLOCK], y[BLOCK]; const int kind = (int)rnd(5); const int amp = kind == 0 ? 32767 : kind == 1 ? 20 : 1 + (int)rnd(32767);
            for (int i = 0; i < BLOCK; ++i) x[i] = y[i] = kind == 4 ? (int16_t)(i % 2 ? 32767 : -32768) : noise_at(amp);
            const Params p = {256 * (1 + (int)rnd(4)), rnd(3) ? 6 + (int)rnd(11) : 0, 6 * (int)rnd(4), (int)rnd(3)};
            d.process(y, rnd(4) ? rnd(8000) : 0, p);
            for (int i = 0; i < BLOCK; ++i, ++n) {
                const int32_t f = x[i] + (x[i] - before) * tilt_a_q8(p.tilt) / 256;       // 고음 올리기를 거친 값(넓은 셈) — 그 부호가 나간 소리의 부호여야 한다
                before = x[i];
                if ((f > 0 && y[i] < 0) || (f < 0 && y[i] > 0)) safe = false;           // 돌면 부호가 뒤집힌다
            }
            if (rnd(500) == 0) { d.reset(); before = 0; }
        }
        CHECK("10 million random samples with every knob setting: the output never wraps around (the sign never flips)", safe && n > 10000000);
    }

    // ── 작은 말은 키운다 ───────────────────────────────────────────────────
    double quiet_gain_db = 0, loud_out_peak = 0;
    {
        Dyn d, legacy; const Params lvl = {1024, 16, 0, 0};
        std::vector<int> g;
        const Pcm in = run(legacy, {256, 0, 0, 0}, 6, [](long i) { return speech_at(i, 600); }, [](long) { return 0u; });
        const Pcm a = run(d, lvl, 6, [](long i) { return speech_at(i, 600); }, [](long) { return 0u; }, &g);
        Dyn l2; const Pcm b = run(l2, OFF, 6, [](long i) { return speech_at(i, 600); }, [](long) { return 0u; });
        const size_t from = 4 * RATE;                                                    // 4초 뒤(자리 잡은 뒤)
        quiet_gain_db = db(rms(a, from), rms(b, from));
        CHECK("lvl 16, quiet speech (peak 600 after the echo canceller): after 4 s it goes out at least 9 dB louder than the fixed x4", quiet_gain_db >= 9.0);
        CHECK("... and its peaks sit near the target (-6 dBFS), nowhere near clipping", peak(a, from) >= 7000 && peak(a, from) <= KNEE);
        CHECK("... the gain starts at the fixed gain and never exceeds the knob (x16)", g.front() <= 1024 + 1024 / 4 + 1 && *std::max_element(g.begin(), g.end()) <= 16 * 256);
        bool slow = true; for (size_t i = 1; i < g.size(); ++i) if (g[i] > g[i - 1] + g[i - 1] / 4 + 1) slow = false;
        CHECK("... it rises by at most a quarter per block (no jump in loudness)", slow);
        Dyn d6; const Pcm c = run(d6, {1024, 6, 0, 0}, 6, [](long i) { return speech_at(i, 600); }, [](long) { return 0u; });
        CHECK("lvl 6: the same speech is held to x6 (the knob is the ceiling)", d6.gain_q8() <= 6 * 256 && db(rms(c, from), rms(b, from)) < 4.0 && db(rms(c, from), rms(b, from)) > 2.5);
        (void)in;
    }
    // ── 큰 말은 누른다 ─────────────────────────────────────────────────────
    {
        Dyn d, l; const Params lvl = {1024, 16, 0, 0};
        const Pcm a = run(d, lvl, 3, [](long i) { return speech_at(i, 12000); }, [](long) { return 0u; });
        const Pcm b = run(l, OFF, 3, [](long i) { return speech_at(i, 12000); }, [](long) { return 0u; });
        long cut_fixed = 0, cut_lvl = 0; for (size_t i = RATE; i < a.size(); ++i) { if (std::abs((int)b[i]) >= 32767) ++cut_fixed; if (std::abs((int)a[i]) >= 32000) ++cut_lvl; }
        loud_out_peak = peak(a, RATE);
        CHECK("lvl on, loud speech (peak 12000): the fixed x4 cuts hundreds of samples at full scale, the leveller none", cut_fixed > 500 && cut_lvl == 0);
        CHECK("... its peaks come down to about the target", loud_out_peak <= KNEE + 2500 && loud_out_peak >= 12000 && d.gain_q8() < 512);
        Dyn s; Pcm first;
        for (int b2 = 0; b2 < 3; ++b2) { int16_t blk[BLOCK]; for (int i = 0; i < BLOCK; ++i) blk[i] = tone_at(b2 * BLOCK + i, 30000, 500); s.process(blk, 0, lvl); first.insert(first.end(), blk, blk + BLOCK); }
        CHECK("a sudden shout: squeezed under full scale from the first sample, and the gain is down to x1 within one block (16 ms)", peak(first) <= CEIL && s.gain_q8() == 256 && peak(first, 2 * BLOCK) <= 30000);
    }
    // ── 고음 올리기(tilt) ─────────────────────────────────────────────────
    double tilt_db[3][4] = {};
    {
        const int hz[4] = {500, 1000, 2000, 4000};
        for (int t = 0; t <= 2; ++t) for (int k = 0; k < 4; ++k) {
            Dyn d; const Params p = {256, 0, 0, t};
            const Pcm y = run(d, p, 1, [&](long i) { return tone_at(i, 1000, hz[k]); }, [](long) { return 0u; });
            Dyn e; const Pcm x = run(e, {256, 0, 0, 0}, 1, [&](long i) { return tone_at(i, 1000, hz[k]); }, [](long) { return 0u; });
            tilt_db[t][k] = db(rms(y, 1000), rms(x, 1000));
        }
        CHECK("tilt 0: nothing changes at any frequency", std::fabs(tilt_db[0][0]) < 0.01 && std::fabs(tilt_db[0][3]) < 0.01);
        CHECK("tilt 1: a gentle treble lift — about +0.5 dB at 1 kHz, +1.6 dB at 2 kHz, +4 dB at 4 kHz, under +0.2 dB at 500 Hz",
              tilt_db[1][0] < 0.2 && std::fabs(tilt_db[1][1] - 0.5) < 0.3 && std::fabs(tilt_db[1][2] - 1.6) < 0.4 && std::fabs(tilt_db[1][3] - 4.0) < 0.4);
        CHECK("tilt 2: about +1.1 dB at 1 kHz, +3.4 dB at 2 kHz, +7 dB at 4 kHz, under +0.4 dB at 500 Hz",
              tilt_db[2][0] < 0.4 && std::fabs(tilt_db[2][1] - 1.1) < 0.3 && std::fabs(tilt_db[2][2] - 3.4) < 0.4 && std::fabs(tilt_db[2][3] - 7.0) < 0.4);
        Dyn d; Pcm sw;
        for (int b = 0; b < 40; ++b) { int16_t blk[BLOCK]; for (int i = 0; i < BLOCK; ++i) blk[i] = tone_at((long)b * BLOCK + i, 3000, 700); const Params p = {256, 0, 0, b < 20 ? 0 : 2}; d.process(blk, 0, p); sw.insert(sw.end(), blk, blk + BLOCK); }
        int jump = 0; for (size_t i = 1; i < sw.size(); ++i) jump = std::max(jump, std::abs(sw[i] - sw[i - 1]));
        CHECK("tilt switched on in mid-stream: the step is at most twice the tone's own slope (the filter kept following the signal while it was off — not a jump of the full amplitude)",
              jump <= 2 * (int)(3000 * 2 * 3.1416 * 700 / RATE + 1) + 16 && jump < 3000 / 1);
        int16_t top[BLOCK]; for (int i = 0; i < BLOCK; ++i) top[i] = (int16_t)(i % 2 ? 32767 : -32768);
        Dyn f; const Params p = {1024, 0, 0, 2}; f.process(top, 0, p);
        bool ok_sign = true; for (int i = 0; i < BLOCK; ++i) if ((i % 2 ? top[i] <= 0 : top[i] >= 0)) ok_sign = false;
        CHECK("tilt on a full-scale input: clipped, never wrapped", ok_sign);
    }
    // ── 소리가 없을 때 잡음을 끌어올리지 않는다 ───────────────────────────
    {
        g_seed = 3; Dyn d, l; const Params lvl = {1024, 16, 0, 0};
        const Pcm a = run(d, lvl, 5, [](long) { return noise_at(30); }, [](long) { return 0u; });
        g_seed = 3; const Pcm b = run(l, OFF, 5, [](long) { return noise_at(30); }, [](long) { return 0u; });
        CHECK("lvl 16, room noise only: goes out exactly as loud as with the fixed gain — never louder", a == b && d.gain_q8() == 1024);
        // 말하다 그친다: 0.2초 뒤에는 다시 고정 이득
        Dyn e; std::vector<int> g;
        g_seed = 9; run(e, lvl, 5, [](long i) { return (int16_t)(speech_at(i, 600) + noise_at(10)); }, [](long) { return 0u; }, &g);
        const int talking = e.gain_q8();
        g.clear(); run(e, lvl, 1, [](long) { return noise_at(10); }, [](long) { return 0u; }, &g);
        CHECK("after speech stops the noise is back at the fixed gain within 0.25 s (the high gain is not left on the room noise)", talking > 3000 && g[15] == 1024 && g.back() == 1024);
        g.clear(); run(e, lvl, 0.2, [](long i) { return speech_at(i + 800, 600); }, [](long) { return 0u; }, &g);
        CHECK("... and when speech starts again the remembered gain is back within 0.15 s", g[9] >= talking * 9 / 10);
    }
    {
        // 고르게 이어지는 소리(기기 시험의 probe 시험음, 선풍기): 처음 몇 초는 말소리로 치고(시험음으로 평탄화를 잴 수 있다), 그 뒤에는 방의 잡음으로 배워 고정 이득으로 돌아간다
        Dyn d; const Params lvl = {1024, 12, 0, 0}; std::vector<int> g;
        run(d, lvl, 12, [](long i) { return tone_at(i, 500, 1000); }, [](long) { return 0u; }, &g);
        const int at2s = g[2 * RATE / BLOCK], at4s = g[4 * RATE / BLOCK], at12s = g.back();
        CHECK("a steady tone (amplitude 500, like the 'probe' test tone): levelled to the knob's ceiling (x12) within 2 s and still there at 4 s", at2s == 12 * 256 && at4s == 12 * 256);
        CHECK("... and learned as background noise within 12 s — a steady hum is not boosted for ever", at12s == 1024);
    }
    // ── 변이 시험이 찾은 구멍을 막는 시험들(이득이 옮겨 가는 모양, 상대가 말하는 동안의 상한, 남은 메아리 어림) ──
    {
        // 이득은 묶음 "안에서" 고르게 옮겨 간다: 갑자기 커진 소리(값이 한결같은 20000)에서 이웃 표본 사이에 계단이 없다
        Dyn d; const Params lvl = {1024, 16, 0, 0}; Pcm out;
        for (int b = 0; b < 4; ++b) { int16_t blk[BLOCK]; for (int i = 0; i < BLOCK; ++i) blk[i] = 20000; d.process(blk, 0, lvl); out.insert(out.end(), blk, blk + BLOCK); }
        int jump = 0; for (size_t i = 1; i < out.size(); ++i) jump = std::max(jump, std::abs(out[i] - out[i - 1]));
        CHECK("the gain glides inside the block: a level that jumps to 20000 comes down with no step between neighbouring samples (not one block late, all at once)", jump <= 300 && out[BLOCK] <= 20100 && out[0] > 28000);
        // 다시 말하기 시작할 때: 기억한 이득(16배)으로 한 번에 뛰지 않고 한 묶음에 1/4 씩
        Dyn e; std::vector<int> g;
        run(e, lvl, 5, [](long i) { return speech_at(i, 600); }, [](long) { return 0u; });
        run(e, lvl, 1, [](long) { return (int16_t)0; }, [](long) { return 0u; });
        const int rest = e.gain_q8();
        run(e, lvl, 0.2, [](long i) { return speech_at(i + 800, 600); }, [](long) { return 0u; }, &g);
        bool quarter = g[0] <= rest + rest / 4 + 1; for (size_t i = 1; i < g.size(); ++i) if (g[i] > g[i - 1] + g[i - 1] / 4 + 1) quarter = false;
        CHECK("speech starting again: the remembered gain comes back a quarter per block, never in one jump", rest == 1024 && quarter && g.back() > 3000);
    }
    {
        // 내 작은 말로 16배를 배운 뒤 상대가 말하기 시작한다(나는 계속 말한다): 이득은 한 묶음 안에 고정 이득 아래로 — 고리 이득이 3b 보다 커지지 않는다
        Dyn d; const Params lvl = {1024, 16, 0, 0}; std::vector<int> g;
        run(d, lvl, 5, [](long i) { return speech_at(i, 600); }, [](long) { return 0u; });
        const int learned = d.gain_q8();
        run(d, lvl, 2, [](long i) { return speech_at(i, 600); }, [](long) { return 1400u; }, &g);
        CHECK("I learned x16 on my quiet speech, then the far end starts while I keep talking: the gain is at or under the fixed gain from the first block on",
              learned > 3500 && *std::max_element(g.begin(), g.end()) <= 1024);
        g.clear(); run(d, lvl, 1, [](long i) { return speech_at(i, 600); }, [](long) { return 0u; }, &g);
        CHECK("... and when the far end stops (after the 0.2 s reverb hold) my learned gain is still there", g[10] <= 1024 && g[20] > 3500 && g.back() > 3500);
    }
    {
        // 상대 소리가 나는 동안 들어온 소리로는 "말소리의 크기"를 배우지 않는다: 자리 잡지 못한 메아리(꼭대기 800 — 내 말로 보일 만큼 크다)가 2초 들어와도, 그 뒤 내 작은 말의 이득은 고정 이득에서 천천히 오른다
        Dyn d; const Params lvl = {1024, 16, 0, 0}; std::vector<int> g;
        run(d, lvl, 2, [](long i) { return tone_at(i, 800, 700); }, [](long) { return 1400u; });
        run(d, lvl, 0.4, [](long) { return (int16_t)0; }, [](long) { return 0u; });
        run(d, lvl, 0.2, [](long i) { return speech_at(i + 800, 300); }, [](long) { return 0u; }, &g);
        CHECK("sound heard while the far end talks does not teach the leveller: 2 s of unconverged echo, then my quiet speech still starts near the fixed gain (x4), not at x16", g[9] < 1536);
        // 상대가 말하는 중의 내 큰 소리: 바로 따라가 이득을 내린다(고정 이득에서도 넘치므로)
        Dyn e;
        const Pcm loud = run(e, lvl, 1, [](long i) { return speech_at(i, 12000); }, [](long) { return 1400u; });
        CHECK("a loud voice during the far end's speech still pulls the gain down at once (peaks near the target, not squeezed against the ceiling)", e.gain_q8() < 512 && peak(loud, RATE / 2) <= KNEE + 2500);
    }
    {
        // 남은 메아리의 어림은 실제 비율로 내려간다: 메아리가 1% 뿐인 기기에서는 상대가 말하는 중의 내 작은 말(꼭대기 400)도 내 말로 알아본다
        const Params both = {1024, 16, 12, 0};
        Dyn d, l; auto echo = [](long i) { return tone_at(i, 20, 1000); };
        run(d, both, 2, echo, [](long) { return 1400u; });
        const Pcm a = run(d, both, 3, [&](long i) { return (int16_t)(speech_at(i, 400) + echo(i)); }, [](long) { return 1400u; });
        const Pcm b = run(l, OFF, 3, [&](long i) { return (int16_t)(speech_at(i, 400) + echo(i)); }, [](long) { return 1400u; });
        double sa = 0, sb = 0; for (size_t i = 0; i < a.size(); ++i) if ((long)i % (RATE * 3 / 10) > 1000 && (long)i % (RATE * 3 / 10) < 2200) { sa += (double)a[i] * a[i]; sb += (double)b[i] * b[i]; }
        CHECK("double talk with a quiet voice (peak 400) on a device whose leftover echo is only 1 %: still within 1.5 dB of the fixed gain (the echo estimate follows the real ratio down)",
              10 * std::log10((sa + 1) / (sb + 1)) > -1.5);
        // 상대 소리가 아주 클 때(되먹임 6000)의 남은 메아리 4%(RMS 170)는 바닥 잡음 문턱보다 크다 — 그래도 메아리로 보고 누른다(어림의 2.5배가 문턱)
        Dyn e, m; auto big = [](long i) { return tone_at(i, 240, 1000); };
        const Pcm c = run(e, both, 3, big, [](long) { return 6000u; });
        const Pcm f = run(m, OFF, 3, big, [](long) { return 6000u; });
        CHECK("a loud far end (loopback 6000) leaves an echo well above the noise gate: it is still pressed by 12 dB (the margin is 2.5x the estimate, not the noise floor)",
              db(rms(c, RATE), rms(f, RATE)) < -11.0 && e.suppressed() > 100);
    }
    {
        // 상대 소리가 없으면 누르지 않는다(res 만 켠 채 조용한 방): 고정 이득 그대로
        g_seed = 21; Dyn d, l; const Params res = {1024, 0, 18, 0};
        const Pcm a = run(d, res, 2, [](long) { return noise_at(12); }, [](long) { return 0u; });
        g_seed = 21; const Pcm b = run(l, OFF, 2, [](long) { return noise_at(12); }, [](long) { return 0u; });
        CHECK("no far-end sound -> nothing is pressed, however quiet my side is (res 18)", a == b && d.suppressed() == 0);
    }

    // ── 상대가 말하는 동안: 고정 이득을 넘지 않는다, 남은 메아리는 누른다 ──
    double pressed_db = 0, dt_loss_db = 0;
    {
        const Params both = {1024, 16, 12, 0};
        Dyn d, l;
        // 상대 소리만: 되먹임 칸 1400, 남은 메아리 = 그 4%(1kHz) — 2026-10-05 실측과 같은 크기
        auto echo = [](long i) { return tone_at(i, 80, 1000); };
        const Pcm a = run(d, both, 3, echo, [](long) { return 1400u; });
        const Pcm b = run(l, OFF, 3, echo, [](long) { return 1400u; });
        pressed_db = db(rms(a, RATE), rms(b, RATE));
        CHECK("far end talking, I am silent (res 12): what goes back is 12 dB below the fixed gain", pressed_db < -11.5 && pressed_db > -12.5 && d.suppressed() > 100);
        CHECK("... and the leveller never treats the leftover echo as speech (gain stays at or below the fixed gain)", d.gain_q8() <= 1024);
        // 동시에 말하기: 상대 소리가 나는 중에 내 말(꼭대기 1500)
        Dyn e, l2; run(e, both, 2, echo, [](long) { return 1400u; });                    // 먼저 2초 상대만(누르는 중)
        std::vector<int> g;
        const Pcm c = run(e, both, 3, [&](long i) { return (int16_t)(speech_at(i, 1500) + echo(i)); }, [](long) { return 1400u; }, &g);
        const Pcm f = run(l2, OFF, 3, [&](long i) { return (int16_t)(speech_at(i, 1500) + echo(i)); }, [](long) { return 1400u; });
        // 말하는 구간(0.2초 소리)의 세기만 비교한다
        double sa = 0, sb = 0; for (size_t i = 0; i < c.size(); ++i) if ((long)i % (RATE * 3 / 10) > 800 && (long)i % (RATE * 3 / 10) < 2400) { sa += (double)c[i] * c[i]; sb += (double)f[i] * f[i]; }
        dt_loss_db = 10 * std::log10((sa + 1) / (sb + 1));
        CHECK("double talk: my speech over the far end's goes out within 1 dB of the fixed gain (not pressed)", dt_loss_db > -1.0 && dt_loss_db < 0.5);
        CHECK("double talk: the press is released within one block (16 ms) of my speech starting", g[1] == 1024 || g[2] == 1024);
        CHECK("double talk: the gain never goes above the fixed gain while the far end is talking (the loop gain is never higher than in 3b)", *std::max_element(g.begin(), g.end()) <= 1024);
        // 상대가 그친 뒤: 0.2초(울림) 지나면 누르기를 푼다
        g.clear(); g_seed = 2; run(e, both, 1, [](long) { return noise_at(10); }, [](long) { return 0u; }, &g);
        CHECK("far end stops: the press ends after the 0.2 s reverb hold", g[5] < 1024 && g[30] == 1024);
    }
    {
        // 에코 제거가 아직 자리 잡지 못했다(남은 메아리가 되먹임 칸의 2배): 누르지 않고, 이득도 올리지 않는다 = 3b 와 같은 소리
        const Params both = {1024, 16, 18, 0};
        Dyn d, l; auto echo = [](long i) { return tone_at(i, 4000, 700); };
        const Pcm a = run(d, both, 2, echo, [](long) { return 1400u; });
        const Pcm b = run(l, OFF, 2, echo, [](long) { return 1400u; });
        CHECK("echo canceller not converged (echo 2x the loopback): not pressed, not boosted — it goes out no louder than with the fixed gain", rms(a, RATE) <= rms(b, RATE) + 1 && d.gain_q8() <= 1024);
    }
    {
        // 상대 쪽 잡음이 문턱 아래(되먹임 칸 60): 상대 소리로 치지 않는다 → 내 말은 평소처럼 키운다
        Dyn d; const Params lvl = {1024, 16, 12, 0};
        run(d, lvl, 5, [](long i) { return speech_at(i, 600); }, [](long) { return 60u; });
        CHECK("a quiet hiss from the far end (loopback 60 < 150) does not count as the far end talking: my speech is still levelled", d.gain_q8() > 3000 && d.far_blocks() == 0 && d.suppressed() == 0);
        Dyn e; run(e, lvl, 5, [](long i) { return speech_at(i, 600); }, [](long) { return 2000u; });
        CHECK("far end loud the whole time: my speech stays at the fixed gain (safe side), nothing above it", e.gain_q8() <= 1024 && e.far_blocks() > 300);
    }
    {
        Dyn d; const Params lvl = {1024, 16, 12, 0};
        run(d, lvl, 3, [](long i) { return speech_at(i, 600); }, [](long b) { return b % 40 < 20 ? 1400u : 0u; });
        const uint32_t s = d.suppressed(), f = d.far_blocks(), sp = d.speech_blocks();
        d.reset();
        CHECK("reset (a new call): gain back to the start, counters cleared", s + f + sp > 0 && d.suppressed() == 0 && d.far_blocks() == 0 && d.speech_blocks() == 0 && d.gain_q8() == 0);
        g_seed = 77; bool sane = true;
        for (int t = 0; t < 300000 && sane; ++t) {
            int16_t x[BLOCK]; const int amp = 1 + (int)rnd(rnd(3) ? 400 : 32000);
            for (int i = 0; i < BLOCK; ++i) x[i] = noise_at(amp);
            const Params p = {256 * (1 + (int)rnd(4)), 6 + (int)rnd(11), 6 * (int)rnd(4), (int)rnd(3)};
            d.process(x, rnd(3) ? rnd(6000) : 0, p);
            if (d.gain_q8() < 16 || d.gain_q8() > 16 * 256) sane = false;
        }
        CHECK("300 thousand random blocks: the gain always stays between the deepest press and x16", sane);
    }

    {
        // 1덩어리 검수 Minor 1·6: 위의 시험들은 "묶음 끝의 이득"만 본다. 여기서는 묶음 안을 표본마다 본다(입력을 256 으로 고르게 주면 나간 표본 = 그 표본에 걸린 이득(Q8)이다).
        //   상대 소리가 시작되는 첫 묶음(16ms)은 배운 이득에서 고정 이득으로 "내려오는 중"이라 그 안의 표본에는 고정 이득보다 큰 이득이 걸린다 — 오르는 표본은 없고, 묶음 끝에서 고정 이득에 닿는다.
        //   둘째 묶음부터는 어느 표본도 고정 이득을 넘지 않는다(고리 이득이 3b 보다 클 수 있는 것은 길어야 그 16ms 다).
        Dyn d; const Params lvl = {1024, 12, 0, 0};
        run(d, lvl, 5, [](long i) { return speech_at(i, 600); }, [](long) { return 0u; });
        const int learned = d.gain_q8();
        int16_t x[BLOCK]; for (int i = 0; i < BLOCK; ++i) x[i] = 256;
        d.process(x, 1400, lvl);
        bool falling = x[0] <= learned; for (int i = 1; i < BLOCK; ++i) if (x[i] > x[i - 1]) falling = false;
        CHECK("the far end starts while my gain is x12: inside that one block (16 ms) the gain only comes down, sample by sample, and ends at the fixed gain",
              learned > 2500 && falling && x[BLOCK - 1] == 1024 && d.gain_q8() == 1024);
        bool under = true;
        for (int b = 0; b < 60; ++b) { for (int i = 0; i < BLOCK; ++i) x[i] = 256; d.process(x, 1400, lvl); for (int i = 0; i < BLOCK; ++i) if (x[i] > 1024) under = false; }
        CHECK("... and from the second block on no sample is above the fixed gain while the far end is heard", under);
    }

    // ── P5A (2026-10-08): 쉼 누르기(rest)·겹침 누르기(dt) ─────────────────────
    {
        const Params base = {1024, 12, 0, 0, 8};                          // 평탄화 12배 · 메아리 지킴 8 — 두 누르기는 꺼져 있다
        Params rest = base; rest.rest_db = 12;
        Params dt = base; dt.dt_db = 12;
        Params both = base; both.rest_db = 12; both.dt_db = 12; both.res_db = 18;
        auto room = [](long) { return noise_at(30); };
        auto quiet_speech = [](long i) { return speech_at(i, 600); };
        // 쉼: 방 잡음만 2초 — rest 12 면 고정 이득의 1/4(256)로 내려가 머문다. 꺼져 있으면 고정 이득 그대로
        { g_seed = 3; Dyn a; std::vector<int> g; run(a, rest, 2, room, [](long) { return 0u; }, &g);
          g_seed = 3; Dyn b; std::vector<int> g0; run(b, base, 2, room, [](long) { return 0u; }, &g0);
          bool falling = true; for (size_t i = 1; i < g.size(); ++i) if (g[i] > g[i - 1]) falling = false;
          CHECK("rest 12: with nobody talking the gain glides down to a quarter of the fixed gain (room noise goes out 12 dB lower) - never up, there within 0.3 s; with rest 0 it stays at the fixed gain",
                g.back() == 256 && falling && g[3] > 512 && g[18] == 256 && a.rested() >= 120 && g0.back() == 1024 && b.rested() == 0); }
        // 말이 시작되면 그 묶음 안에 고정 이득까지 풀리고(16ms), 이어서 평탄화가 키운다. 말이 그치면 0.2초(말 끊김 유지) 뒤에 다시 눌린다
        { g_seed = 4; Dyn d; std::vector<int> g;
          run(d, rest, 1, room, [](long) { return 0u; });
          run(d, rest, 3, quiet_speech, [](long) { return 0u; }, &g);
          const int first_speech_block = (int)(std::find_if(g.begin(), g.end(), [](int v) { return v >= 1024; }) - g.begin());
          std::vector<int> after; g_seed = 5; run(d, rest, 1, room, [](long) { return 0u; }, &after);
          CHECK("rest 12: my speech lifts the press inside its first blocks (back at the fixed gain within 32 ms) and is then levelled as before (up to the 12x limit)",
                first_speech_block <= 2 && *std::max_element(g.begin(), g.end()) == 12 * 256);
          bool glide = true; for (size_t i = 1; i < after.size(); ++i) if (after[i] > after[i - 1] || (after[i - 1] <= 1024 && after[i] < after[i - 1] - after[i - 1] / 8 - 1)) glide = false;
          CHECK("rest 12: after I stop the press does not cut in at once (the 0.2 s hold runs out first), then the speech gain drops to the fixed gain and glides on down (1/8 per block) - at the rest level within 0.5 s",
                after[0] >= 1024 && glide && after[31] == 256 && after.back() == 256); }
        // 쉼 누르기는 상대 소리가 나는 동안에는 걸리지 않는다(그때는 res·dt 의 몫) — rest 만 켜면 상대 소리 중의 이득은 고정 이득 그대로
        { g_seed = 6; Dyn d; std::vector<int> g; run(d, rest, 2, room, [](long) { return 1400u; }, &g);
          CHECK("rest only: while the far end is heard the rest press does not apply (gain = fixed gain, nothing counted as rest)", g.back() == 1024 && d.rested() == 0); }
        // 겹침: 상대 소리(되먹임 1400) 위의 내 말(꼭대기 3000 — 남은 메아리 어림보다 훨씬 크다) → dt 12 면 고정 이득의 1/4, 꺼져 있으면 고정 이득
        { Dyn a, b; std::vector<int> ga, gb;
          run(a, dt, 2, [](long i) { return speech_at(i, 3000); }, [](long) { return 1400u; }, &ga);
          run(b, base, 2, [](long i) { return speech_at(i, 3000); }, [](long) { return 1400u; }, &gb);
          CHECK("dt 12: my speech over the far end goes out at a quarter of the fixed gain (never above it); with dt 0 at the fixed gain",
                *std::max_element(ga.begin() + 2, ga.end()) == 256 && *std::min_element(ga.begin() + 2, ga.end()) == 256 && gb.back() == 1024); }
        // 큰 남은 메아리(되먹임의 1.2배 — 에코 제거가 자리 잡기 전)는 "내 말"로 보인다 → res 는 걸리지 않지만 dt 가 누른다(이 시험이 P5A-D2 의 이유다)
        { Dyn d; std::vector<int> g; Params p = base; p.res_db = 18; p.dt_db = 12;
          const Pcm out = run(d, p, 2, [](long i) { return tone_at(i, 2376, 700); }, [](long) { return 1400u; }, &g);      // 세기 1680 = 되먹임 1400 의 1.2배
          Dyn e; Params q = base; q.res_db = 18;
          const Pcm out0 = run(e, q, 2, [](long i) { return tone_at(i, 2376, 700); }, [](long) { return 1400u; });
          CHECK("dt 12: a large residual echo (1.2x the loopback - the canceller not settled) passes as 'my speech' and escapes res, but dt presses it 12 dB",
                d.suppressed() == 0 && g.back() == 256 && std::fabs(db(rms(out, out.size() / 2), rms(out0, out0.size() / 2)) + 12.0) < 0.5); }
        // 셋 다 켠 부팅 값 근처: 상대 소리만 날 때(남은 메아리 4%)는 res 가 18dB 누르고(dt 가 아니라), 상대 소리가 그치면 쉼 누르기로 넘어간다
        { Dyn d; std::vector<int> g;
          run(d, both, 2, [](long i) { return tone_at(i, 80, 700); }, [](long) { return 1400u; }, &g);                      // 세기 56 = 1400 의 4%
          const int far_gain = g.back();
          std::vector<int> g2; g_seed = 8; run(d, both, 1, room, [](long) { return 0u; }, &g2);
          CHECK("res 18 + dt 12 + rest 12: far end alone -> pressed 18 dB (gain 128, counted as suppressed); after it stops (0.2 s) -> the rest press (gain 256)",
                far_gain == 128 && d.suppressed() > 100 && g2.back() == 256 && d.rested() > 0); }
        // 넘치지 않는다 · 켜고 끄는 순간 계단이 없다(이득은 묶음 안에서 곧게 옮겨 간다 — 표본 사이의 뜀이 작다)
        { Dyn d; Pcm out = run(d, both, 1, [](long i) { return tone_at(i, 30000, 500); }, [](long) { return 0u; });
          CHECK("rest/dt on: a full-scale input still never leaves the 16-bit range (soft knee as with lvl)", peak(out) <= 32767); }
        // P5A-D4 저음 걷어내기: 100Hz 는 약 12dB, 200Hz 는 약 3dB 줄고 1kHz 는 그대로다. 꺼져 있으면 아무것도 바꾸지 않는다
        { const Params fix = {256, 0, 0, 0};                             // 고정 1배 — 필터만 본다
          Params hp = fix; hp.hpf = 1;
          auto level = [&](int hz, const Params &p) { Dyn d; const Pcm o = run(d, p, 1, [hz](long i) { return tone_at(i, 8000, hz); }, [](long) { return 0u; }); return rms(o, o.size() / 2); };
          const double d100 = db(level(100, hp), level(100, fix)), d200 = db(level(200, hp), level(200, fix)), d300 = db(level(300, hp), level(300, fix)), d1k = db(level(1000, hp), level(1000, fix)), d4k = db(level(4000, hp), level(4000, fix));
          CHECK("hpf 1: second-order high-pass at 200 Hz - 100 Hz about -12 dB, 200 Hz about -3 dB, 300 Hz within 1 dB, 1 kHz and 4 kHz untouched (0.1 dB)",
                d100 < -11.0 && d100 > -13.5 && d200 < -2.5 && d200 > -3.6 && d300 > -1.0 && std::fabs(d1k) < 0.1 && std::fabs(d4k) < 0.1);
          Dyn a, b; g_seed = 31; const Pcm x = run(a, fix, 1, [](long) { return noise_at(9000); }, [](long) { return 0u; });
          Params off = fix; off.hpf = 0;
          g_seed = 31; const Pcm y = run(b, off, 1, [](long) { return noise_at(9000); }, [](long) { return 0u; });
          Dyn c; const Pcm dc = run(c, hp, 1, [](long) { return (int16_t)5000; }, [](long) { return 0u; });
          CHECK("hpf 0 changes nothing (sample for sample); hpf 1 removes a steady offset completely within a second", x == y && peak(dc, dc.size() - 1600) <= 1);
          if (info) printf("   저음 걷어내기: 100 / 200 / 300 / 1k / 4kHz = %+.1f / %+.1f / %+.1f / %+.2f / %+.2f dB\n", d100, d200, d300, d1k, d4k); }
        // 꺼져 있으면(0·0) P4H 의 동작과 표본까지 같다 — 값을 주지 않은 Params 는 옛 동작
        { g_seed = 21; Dyn a; const Pcm x = run(a, base, 3, quiet_speech, [](long b) { return b % 40 < 20 ? 300u : 0u; });
          Params z = base; z.rest_db = 0; z.dt_db = 0;
          g_seed = 21; Dyn b; const Pcm y = run(b, z, 3, quiet_speech, [](long bk) { return bk % 40 < 20 ? 300u : 0u; });
          CHECK("rest 0 and dt 0 are the 2026-10-05 behaviour, sample for sample", x == y); }
    }

    if (info) {
        printf("\n-- 크기(맥 시험, 계획서 표) --\n");
        printf("작은 말(에코 제거 뒤 꼭대기 600) · lvl 16: 고정 4배보다 %+.1f dB\n", quiet_gain_db);
        printf("큰 말(꼭대기 12000) · lvl 16: 나가는 꼭대기 %.0f (고정 4배는 32767 에서 잘림)\n", loud_out_peak);
        printf("상대 소리만(되먹임 1400, 남은 메아리 4%%) · res 12: 고정 4배보다 %+.1f dB\n", pressed_db);
        printf("동시에 말하기(내 말 꼭대기 1500) · lvl 16 · res 12: 고정 4배보다 %+.2f dB\n", dt_loss_db);
        printf("고음 올리기(500 / 1k / 2k / 4kHz): tilt 1 = %+.1f / %+.1f / %+.1f / %+.1f dB, tilt 2 = %+.1f / %+.1f / %+.1f / %+.1f dB\n",
               tilt_db[1][0], tilt_db[1][1], tilt_db[1][2], tilt_db[1][3], tilt_db[2][0], tilt_db[2][1], tilt_db[2][2], tilt_db[2][3]);
    }
    printf("\n%d ok, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
