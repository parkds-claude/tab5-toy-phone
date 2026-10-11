// test_pcm.cpp — pcm.h 단위 시험:
//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_pcm.cpp -o /tmp/t_pcm && /tmp/t_pcm
// 섞기(목소리 + 신호음), 크기 바꾸기, 세기, 태스크 사이 링버퍼, 출력 박자 감시를 고정한다
// (계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md PL3b-D3·D4·D6, Review Focus 3·5).
#include "pcm.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
using namespace pcm;
static int ok = 0, bad = 0;
#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)

// 공급자 흉내(함수 포인터라 상태는 전역에 둔다)
static int g_calls[3];
static bool g_on[3];
static int16_t g_val[3];
template <int K> static bool src(int16_t *out, int n) { ++g_calls[K]; if (!g_on[K]) return false; for (int i = 0; i < n; ++i) out[i] = g_val[K]; return true; }
static void set(int k, bool on, int16_t v) { g_on[k] = on; g_val[k] = v; }
static bool all(const int16_t *b, int n, int16_t v) { for (int i = 0; i < n; ++i) if (b[i] != v) return false; return true; }

static uint32_t lcg(uint32_t &s) { s = s * 1664525u + 1013904223u; return s >> 8; }

int main()
{
    // ════ 더하기·크기 ════
    {
        int16_t a[6] = {100, -100, 32000, -32000, 32767, -32768}, b[6] = {23, -23, 1000, -1000, 1, -1};
        mix_add(a, b, 6);
        CHECK("mix_add: plain sum, and clips at +32767 / -32768 instead of wrapping",
              a[0] == 123 && a[1] == -123 && a[2] == 32767 && a[3] == -32768 && a[4] == 32767 && a[5] == -32768);
        int16_t g[5] = {1000, -1000, 20000, -20000, 3};
        gain_q8(g, 5, 512);
        CHECK("gain_q8: x2 doubles and clips", g[0] == 2000 && g[1] == -2000 && g[2] == 32767 && g[3] == -32768 && g[4] == 6);
        int16_t h[3] = {1000, -1001, 7};
        gain_q8(h, 3, 128);
        CHECK("gain_q8: x0.5 halves (rounds toward zero)", h[0] == 500 && h[1] == -500 && h[2] == 3);
        int16_t k[2] = {12345, -32768};
        gain_q8(k, 2, 256);
        CHECK("gain_q8: 256 leaves the samples untouched", k[0] == 12345 && k[1] == -32768);
        CHECK("sat16: limits", sat16(40000) == 32767 && sat16(-40000) == -32768 && sat16(-7) == -7);
    }

    // ════ 출력 한 묶음 섞기 (PL3b-D3 — 3a 에서 한 번도 돌지 않은 코드, 검수 M-7) ════
    {
        const source_fn fns[3] = {src<0>, src<1>, nullptr};
        int16_t mix[256], tmp[256];
        for (int16_t &v : mix) v = 77;
        set(0, false, 0); set(1, false, 0);
        CHECK("mix: nobody has sound -> 0, and the block is left untouched", mix_sources(fns, 3, mix, tmp, 256) == 0 && all(mix, 256, 77));
        set(0, true, 1200);
        CHECK("mix: tone only -> exactly the tone", mix_sources(fns, 3, mix, tmp, 256) == 1 && all(mix, 256, 1200));
        set(0, false, 0); set(1, true, -900);
        CHECK("mix: voice only -> exactly the voice", mix_sources(fns, 3, mix, tmp, 256) == 1 && all(mix, 256, -900));
        set(0, true, 1200);
        CHECK("mix: tone + voice -> the sum of both (nobody wins)", mix_sources(fns, 3, mix, tmp, 256) == 2 && all(mix, 256, 300));
        set(0, true, 30000); set(1, true, 30000);
        CHECK("mix: loud + loud clips at 32767 (no wrap-around)", mix_sources(fns, 3, mix, tmp, 256) == 2 && all(mix, 256, 32767));
        set(0, true, -30000); set(1, true, -30000);
        CHECK("mix: clips at -32768 on the negative side", mix_sources(fns, 3, mix, tmp, 256) == 2 && all(mix, 256, -32768));
        g_calls[0] = g_calls[1] = 0;
        mix_sources(fns, 3, mix, tmp, 256);
        CHECK("mix: every source is asked exactly once per block, in registration order", g_calls[0] == 1 && g_calls[1] == 1);
        const source_fn swapped[2] = {src<1>, src<0>};
        set(0, true, 5); set(1, true, 9);
        int16_t m2[8], t2[8];
        CHECK("mix: order of the sources does not change the result", mix_sources(swapped, 2, m2, t2, 8) == 2 && all(m2, 8, 14));
        const source_fn none[2] = {nullptr, nullptr};
        CHECK("mix: empty slots are skipped", mix_sources(none, 2, m2, t2, 8) == 0 && all(m2, 8, 14));
    }

    // ════ 세기 ════
    {
        CHECK("isqrt64: exact squares and floor", isqrt64(0) == 0 && isqrt64(1) == 1 && isqrt64(15) == 3 && isqrt64(16) == 4
              && isqrt64(1073676289ull) == 32767 && isqrt64(18446744073709551615ull) == 4294967295u);
        Rms r;
        CHECK("rms: empty = 0", r.value() == 0 && r.take() == 0);
        std::vector<int16_t> s(16000);
        for (int i = 0; i < 16000; ++i) s[i] = (int16_t)std::lround(8000 * std::sin(2 * 3.14159265358979 * 1000 * i / 16000.0));
        r.add(s.data(), 16000);
        CHECK("rms: a sine of amplitude 8000 reads 5656 (8000 / sqrt 2)", r.value() >= 5655 && r.value() <= 5657);
        CHECK("rms: take returns the value and clears", r.take() >= 5655 && r.value() == 0 && r.n == 0);
        int16_t full[4] = {-32768, -32768, -32768, -32768};
        r.add(full, 4);
        CHECK("rms: full scale does not overflow", r.take() == 32768);
        int16_t dc[3] = {100, 100, 100};
        for (int i = 0; i < 4000000; ++i) r.add(dc, 3);                 // 12,000,000 표본(12분 30초 분량)
        CHECK("rms: long accumulation stays exact", r.value() == 100 && r.n == 12000000u);
    }

    // ════ 링버퍼 (PL3b-D6) ════
    {
        Ring<8> q;
        int16_t in[8] = {1, 2, 3, 4, 5, 6, 7, 8}, out[8] = {};
        CHECK("ring: empty at first", q.size() == 0 && q.space() == 8 && q.read(out, 4) == 0);
        CHECK("ring: write 5 -> size 5", q.write(in, 5) == 5 && q.size() == 5 && q.space() == 3);
        CHECK("ring: a write that does not fit puts in what fits and reports it", q.write(in + 5, 5) == 3 && q.size() == 8 && q.space() == 0);
        CHECK("ring: full -> nothing more goes in", q.write(in, 1) == 0 && q.size() == 8);
        CHECK("ring: reads come out in order", q.read(out, 3) == 3 && out[0] == 1 && out[1] == 2 && out[2] == 3 && q.size() == 5);
        CHECK("ring: wraps around the end", q.write(in, 3) == 3 && q.read(out, 8) == 8 && out[0] == 4 && out[4] == 8 && out[5] == 1 && out[7] == 3);
        CHECK("ring: a read of more than there is returns what there is", q.write(in, 2) == 2 && q.read(out, 8) == 2 && out[1] == 2 && q.size() == 0);
        q.write(in, 6);
        q.drain();
        CHECK("ring: drain empties it (reader side)", q.size() == 0 && q.read(out, 8) == 0 && q.write(in, 8) == 8);
    }
    {
        Ring<1024> q;                                                   // 20ms 프레임을 넣고 16ms 묶음으로 꺼낸다 — 오래 돌려도 표본이 빠지거나 겹치지 않는다
        uint32_t seed = 7; int16_t next_in = 0, next_out = 0; bool order = true; size_t peak = 0;
        for (int step = 0; step < 2000000; ++step) {
            if (lcg(seed) & 1) { int16_t f[320]; if (q.space() >= 320) { for (int16_t &v : f) v = next_in++; q.write(f, 320); } }
            else { int16_t b[256]; const size_t n = q.read(b, 256); for (size_t i = 0; i < n; ++i) if (b[i] != next_out++) order = false; }
            if (q.size() > peak) peak = q.size();
        }
        CHECK("ring: 2 million random 320-in / 256-out steps (index wraps 2^16 many times): no sample lost, repeated or reordered", order && peak <= 1024);
    }
    {
        Ring<1024> q;                                                   // 진짜 두 스레드: 쓰는 쪽 하나, 읽는 쪽 하나(읽는 쪽이 가끔 drain)
        const int TOTAL = 3000000;
        std::atomic<bool> done{false};
        bool order = true; long got = 0, drains = 0; const long GIVE_UP = 400000000;      // 고장 나서 끝나지 않을 때를 위한 상한
        std::thread writer([&] {
            int16_t f[320]; int v = 0; long spins = 0;
            while (v < TOTAL && ++spins < GIVE_UP) { const int n = TOTAL - v < 320 ? TOTAL - v : 320; for (int i = 0; i < n; ++i) f[i] = (int16_t)(v + i); v += (int)q.write(f, (size_t)n); }
            done = true;
        });
        std::thread reader([&] {
            int16_t b[256]; int16_t expect = 0; bool synced = true; uint32_t seed = 3; long spins = 0;
            while ((!done || q.size()) && ++spins < GIVE_UP) {
                if ((lcg(seed) & 0x3FF) == 0) { q.drain(); synced = false; ++drains; continue; }
                const size_t n = q.read(b, 256);
                for (size_t i = 0; i < n; ++i) {
                    if (synced && b[i] != expect) order = false;        // drain 뒤 첫 표본에서 다시 맞춘다(그 뒤로는 연속이어야 한다)
                    expect = (int16_t)(b[i] + 1); synced = true;
                }
                got += (long)n;
            }
        });
        writer.join(); reader.join();
        CHECK("ring: two real threads, 3 million samples, reader drains now and then: what is read is always consecutive", order && got > 0 && drains > 0 && q.size() == 0);
    }

    // ════ 출력 박자 감시 (PL3b-D4) ════
    {
        GapWatch w;
        const int64_t LIMIT = 3 * 16000;                                // DMA 3칸 = 48ms
        int64_t t = 1000000;
        w.wrote(t, LIMIT);
        CHECK("gap: the first block after a rest is never late", w.late == 0 && w.max_us == 0);
        for (int i = 0; i < 1000; ++i) { t += 16000; w.wrote(t, LIMIT); }
        CHECK("gap: a steady 16 ms beat is not late", w.late == 0 && w.max_us == 16000);
        t += 47999; w.wrote(t, LIMIT);
        CHECK("gap: 47.999 ms is still inside what the DMA holds", w.late == 0 && w.max_us == 47999);
        t += 48000; w.wrote(t, LIMIT);
        CHECK("gap: 48 ms (the whole DMA ring) counts as late", w.late == 1 && w.max_us == 48000);
        t += 400000; w.wrote(t, LIMIT);
        CHECK("gap: a long stall counts once, and the longest gap is kept", w.late == 2 && w.max_us == 400000);
        w.rest();
        t += 9000000; w.wrote(t, LIMIT);
        CHECK("gap: sleeping (nothing to play) is not a late block", w.late == 2 && w.max_us == 400000);
        t += 16000; w.wrote(t, LIMIT);
        CHECK("gap: and the beat is watched again afterwards", w.late == 2);
    }

    {   // P5A-D3: 받는 목소리를 키울 때 — 꼭대기를 부드럽게 누른다
        using namespace pcm;
        CHECK("soft_limit: below the knee nothing changes, sign kept", soft_limit(0) == 0 && soft_limit(20000) == 20000 && soft_limit(-20000) == -20000 && soft_limit(12345) == 12345 && soft_limit(-1) == -1);
        bool mono = true, inside = true;
        for (int32_t v = 20000; v < 400000; v += 37) { if (soft_limit(v + 37) < soft_limit(v)) mono = false; if (soft_limit(v) > 32767 || soft_limit(-v) < -32767 || soft_limit(-v) != -soft_limit(v)) inside = false; }
        CHECK("soft_limit: above the knee it keeps rising but never passes full scale, symmetric for both signs", mono && inside && soft_limit(1 << 30) <= 32767 && soft_limit(-(1 << 30)) >= -32767);
        CHECK("soft_limit: no step at the knee (within one count, slope about 1 there) and twice the knee lands near 27800", soft_limit(20001) >= 20000 && soft_limit(20001) <= 20001 && soft_limit(20100) > 20090 && soft_limit(40000) > 27000 && soft_limit(40000) < 28200);
        int16_t a[4] = {100, -8000, 16000, -32768}, b[4] = {100, -8000, 16000, -32768};
        gain_soft_q8(a, 4, 512); gain_q8(b, 4, 512);
        CHECK("gain_soft_q8 2x: small samples are exactly doubled, large ones are bent instead of clipped (16000 -> about 26200, -32768 -> above -32767)",
              a[0] == 200 && a[1] == -16000 && b[2] == 32000 && a[2] > 26000 && a[2] < 26400 && b[3] == -32768 && a[3] > -32767 && a[3] < -29000);
        // 받는 목소리의 천장(voice_core.h PLAY_KNEE 16000 · PLAY_CEIL 26000 — 스피커 한계 아래): 2배로 키운 말소리 꼭대기(16000 → 32000)가 약 22150 에 놓이고 어떤 입력도 26000 을 넘지 않는다
        int16_t c[4] = {100, 7000, 16000, 32767};
        gain_soft_q8(c, 4, 512, 16000, 26000);
        bool under = true; for (int32_t v = -200000; v <= 200000; v += 53) if (soft_limit(v, 16000, 26000) > 26000 || soft_limit(v, 16000, 26000) < -26000) under = false;
        CHECK("gain_soft_q8 with the loudspeaker ceiling (knee 16000, ceiling 26000): quiet samples exactly doubled, a doubled speech peak lands near 22150, nothing ever passes 26000",
              c[0] == 200 && c[1] == 14000 && c[2] > 22000 && c[2] < 22300 && c[3] < 26000 && c[3] > 23000 && under);
    }

    printf("\n%d ok, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
