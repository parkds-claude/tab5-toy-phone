// test_voice_tune.cpp — voice_tune.h 단위 시험:
//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_voice_tune.cpp -o /tmp/t_voice_tune && /tmp/t_voice_tune
// 계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D3·D8, Review Focus 4(손잡이가 기본 동작을 바꿔 놓음):
//   손잡이의 범위(소리가 커지는 쪽은 3b 의 값을 넘지 못한다), "부팅 값과 같은가"·"5935ddd 와 같은가"·"selftest·vping 판정이 유효한가", Opus 출력 버퍼 크기.
// 이 시험은 DEFAULT_* 의 값 자체를 고정하지 않는다(마지막 과제가 고친다) — 그 값이 LEGACY_* 와 같은지는 draft/verify/tune_check.py 와 기기의 TUNE 줄(legacy=1)이 본다.
#include "voice_tune.h"
#include <cstdio>
using namespace voicetune;
static int ok = 0, bad = 0;
#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)

int main()
{
    {
        CHECK("legacy = what 5935ddd did: x4, volume 100, mic 24 dB, 24 kbps, leveller off, no treble lift, echo press off, old jitter rule, Nagle on",
              LEGACY_TXGAIN == 4 && LEGACY_VOL == 100 && LEGACY_MIC_DB == 24 && LEGACY_KBPS == 24 && LEGACY_LVL == 0 && LEGACY_TILT == 0 && LEGACY_RES_DB == 0 && LEGACY_JB == 0 && LEGACY_NODELAY == 0);
        const Knobs boot, old = legacy();
        CHECK("a fresh Knobs holds the boot defaults; legacy() holds the 5935ddd values", is_default(boot) && is_legacy(old)
              && boot.txgain == DEFAULT_TXGAIN && boot.vol == DEFAULT_VOL && boot.mic_db == DEFAULT_MIC_DB && boot.kbps == DEFAULT_KBPS
              && boot.lvl == DEFAULT_LVL && boot.tilt == DEFAULT_TILT && boot.res_db == DEFAULT_RES_DB && boot.jb == DEFAULT_JB && boot.nodelay == DEFAULT_NODELAY);
        bool each = true;
        for (int f = 0; f < 9; ++f) {
            Knobs k = legacy();
            int *field[] = {&k.txgain, &k.vol, &k.mic_db, &k.kbps, &k.lvl, &k.tilt, &k.res_db, &k.jb, &k.nodelay};
            *field[f] += 1;
            if (is_legacy(k) || same(k, legacy())) each = false;
        }
        CHECK("changing any one of the nine knobs makes it 'not legacy' (none is forgotten in the comparison)", each && sizeof(Knobs) == 9 * sizeof(int));
        Knobs k = legacy();
        CHECK("selftest / vping verdicts are valid only at send gain x4 and volume 100 ...", meas_valid(k));
        k.txgain = 2; const bool g = meas_valid(k); k.txgain = 4; k.vol = 90; const bool v = meas_valid(k);
        CHECK("... not when either is turned down", !g && !v);
        k = legacy(); k.mic_db = 18; k.lvl = 16; k.tilt = 2; k.res_db = 12; k.jb = 4; k.nodelay = 1; k.kbps = 32;
        CHECK("... and the other knobs do not matter for them (diagnostic sources bypass the leveller)", meas_valid(k) && !is_legacy(k));
    }
    {
        CHECK("txgain 1..4, volume 0..100 (as in 3b)", ok_txgain(1) && ok_txgain(4) && !ok_txgain(0) && !ok_txgain(5) && ok_vol(0) && ok_vol(100) && !ok_vol(101) && !ok_vol(-1));
        CHECK("mic gain: 12..24 dB in 3 dB steps — never above the 3b value (louder would raise the loop gain)",
              ok_mic_db(12) && ok_mic_db(15) && ok_mic_db(18) && ok_mic_db(21) && ok_mic_db(24) && !ok_mic_db(27) && !ok_mic_db(30) && !ok_mic_db(9) && !ok_mic_db(13) && !ok_mic_db(0) && MIC_DB_MAX == LEGACY_MIC_DB);
        CHECK("bitrate: 12..40 kbps", ok_kbps(12) && ok_kbps(24) && ok_kbps(40) && !ok_kbps(11) && !ok_kbps(41) && !ok_kbps(0) && !ok_kbps(24000));
        CHECK("leveller: 0 (off) or a ceiling of x6..x16", ok_lvl(0) && ok_lvl(6) && ok_lvl(16) && !ok_lvl(1) && !ok_lvl(5) && !ok_lvl(17) && !ok_lvl(-1));
        CHECK("treble lift: 0 (off), 1 or 2", ok_tilt(0) && ok_tilt(1) && ok_tilt(2) && !ok_tilt(3) && !ok_tilt(-1));
        CHECK("echo press: 0, 6, 12 or 18 dB", ok_res_db(0) && ok_res_db(6) && ok_res_db(12) && ok_res_db(18) && !ok_res_db(3) && !ok_res_db(24) && !ok_res_db(-6));
        CHECK("jitter rule: 0 (the 3b rule) or a ceiling of 3..10 frames", ok_jb(0) && ok_jb(3) && ok_jb(10) && !ok_jb(1) && !ok_jb(2) && !ok_jb(11) && !ok_jb(-1));
        CHECK("nodelay: 0 or 1; test tone: 300..7000 Hz (below half the sample rate)", ok_nodelay(0) && ok_nodelay(1) && !ok_nodelay(2) && ok_vtone(300) && ok_vtone(1000) && ok_vtone(7000) && !ok_vtone(299) && !ok_vtone(7001) && VTONE_DEFAULT == 1000 && VTONE_MAX < 8000);
        bool defaults_ok = ok_txgain(DEFAULT_TXGAIN) && ok_vol(DEFAULT_VOL) && ok_mic_db(DEFAULT_MIC_DB) && ok_kbps(DEFAULT_KBPS) && ok_lvl(DEFAULT_LVL) && ok_tilt(DEFAULT_TILT) && ok_res_db(DEFAULT_RES_DB) && ok_jb(DEFAULT_JB) && ok_nodelay(DEFAULT_NODELAY);
        CHECK("whatever the boot defaults are, each lies inside its knob's range", defaults_ok);
        CHECK("echo guard (P4H-D1): 0 (off, the 771b9fe behaviour) .. 16 quarters of the loopback level; it boots on (not 0) and inside the range",
              ok_guard(0) && ok_guard(1) && ok_guard(GUARD_MAX) && !ok_guard(GUARD_MAX + 1) && !ok_guard(-1) && GUARD_MAX == 16 && GUARD_DEFAULT > 0 && ok_guard(GUARD_DEFAULT));
    }
    {
        CHECK("encoder output buffer the part asks for: bytes of one 20 ms frame + 100 (160 at 24 kbps — the 'opus_out_max' the device printed)",
              opus_out_need(24) == 160 && opus_out_need(12) == 130 && opus_out_need(32) == 180 && opus_out_need(40) == 200);
        bool fits = true; for (int k = KBPS_MIN; k <= KBPS_MAX; ++k) if (opus_out_need(k) > OPUS_OUT_CAP || k * 1000 / 400 + 2 > 200) fits = false;
        CHECK("every bitrate the knob allows fits the capture task's buffer (256) and the tx queue slot (200 with the 2-byte sequence)", fits && OPUS_OUT_CAP == 256);
    }
    {   // P5A (2026-10-08): 쉼 누르기·겹침 누르기·받는 목소리 이득 — guard 처럼 손잡이 아홉 개 밖에 있다
        CHECK("P5A knobs: ranges - rest 0|6|12|18, dt 0|6|12, rxgain 100-300, the boot values inside them and switched on",
              ok_rest(0) && ok_rest(6) && ok_rest(12) && ok_rest(18) && !ok_rest(24) && !ok_rest(5) && !ok_rest(-6)
              && ok_dt(0) && ok_dt(6) && ok_dt(12) && !ok_dt(18) && !ok_dt(3)
              && ok_rxgain(100) && ok_rxgain(300) && !ok_rxgain(99) && !ok_rxgain(301)
              && ok_rest(REST_DEFAULT) && ok_dt(DT_DEFAULT) && ok_rxgain(RXGAIN_DEFAULT) && REST_DEFAULT > 0 && DT_DEFAULT > 0 && RXGAIN_DEFAULT > 100
              && ok_hpf(0) && ok_hpf(1) && !ok_hpf(2) && !ok_hpf(-1) && ok_hpf(HPF_DEFAULT));
        CHECK("P5A knobs: they are not part of the nine - dflt/legacy/meas do not look at them (selftest and vping do not pass through them)",
              is_default(Knobs()) && meas_valid(Knobs()) && legacy().res_db == 0 && Knobs().res_db == DEFAULT_RES_DB);
    }

    printf("\n%d ok, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
