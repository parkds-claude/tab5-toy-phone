// test_voice_core.cpp — voice_core.h 단위 시험:
//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_voice_core.cpp -o /tmp/t_voice_core && /tmp/t_voice_core
//   /tmp/t_voice_core --info   → 시험 뒤에 지연 시뮬레이션의 숫자(계획서 지연 예산 표의 "맥 시뮬레이션" 칸)를 찍는다
// 목소리 통화의 판단 로직을 고정한다(계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md PL3b-D2·D5~D13, Review Focus 1~4):
//   받기 전에는 켜지지 않는다 / 어떤 길로 끝나도 꺼진다 / 신호음과 겹치는 순간 / 보내는 프레임 / 밀리면 버린다 / 빠진 프레임 메우기 /
//   받는 길(지터 버퍼 → 링버퍼 → 16ms 묶음) / 오래·거칠게(손실 없는 TCP 의 멈춤·몰림, 시계 어긋남) / 시험음·지연 재기.
#include "voice_core.h"
#include "board_profile.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
using namespace voicecore;
using namespace callfsm;
static int ok = 0, bad = 0;
#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)

typedef std::vector<int16_t> Pcm;
static const double PI = 3.14159265358979323846;
struct NoLock { void lock() {} void unlock() {} };
typedef Receiver<NoLock> Rx;
static NoLock g_nolock;

static int max_jump(const Pcm &v, int prev = 0) { int m = 0; for (int16_t s : v) { m = std::max(m, std::abs((int)s - prev)); prev = s; } return m; }
static int peak(const Pcm &v, size_t from = 0, size_t to = SIZE_MAX) { int m = 0; for (size_t i = from; i < v.size() && i < to; ++i) m = std::max(m, std::abs((int)v[i])); return m; }
static uint32_t g_seed = 1;
static uint32_t rnd(uint32_t m) { g_seed = g_seed * 1664525u + 1013904223u; return (g_seed >> 8) % m; }

// ── 가짜 코덱: 프레임 = [그 프레임 첫 표본의 번호 4바이트][채움 56바이트]. 해제하면 그 번호부터의 PCM 을 다시 만든다(표본 번호 g 의 함수).
//    SINE = 317Hz 사인(이어 붙는 자리의 계단을 볼 때 — 20ms 프레임과 주기가 맞지 않아 프레임마다 모양이 다르다),
//    RAMP = 표본 번호 그 자체(어느 표본이 언제 나왔는지 볼 때).
enum class Sig { SINE, RAMP };
static Sig g_sig = Sig::SINE;
static const int SIG_AMP = 6000, SIG_HZ = 317, RAMP_MOD = 20000;
static int16_t sig(int64_t g) { return g_sig == Sig::SINE ? (int16_t)std::lround(SIG_AMP * std::sin(2 * PI * SIG_HZ * (double)g / RATE)) : (int16_t)(g % RAMP_MOD); }
static const int SIG_STEP = (int)std::ceil(SIG_AMP * 2 * PI * SIG_HZ / RATE) + 2;       // 사인의 이웃 표본 차이 상한
static const int JOIN_STEP = SIG_STEP + 2 * SIG_AMP / FADE + 4;                          // 5ms 에 걸쳐 넘겨 받는 자리의 상한(두 소리의 차이 ≤ 2 × 진폭을 80 걸음에)
static size_t frame_bytes(uint32_t first, uint16_t seq, uint8_t *out)
{
    uint8_t opus[60] = {};
    opus[0] = (uint8_t)first; opus[1] = (uint8_t)(first >> 8); opus[2] = (uint8_t)(first >> 16); opus[3] = (uint8_t)(first >> 24);
    return relayproto::pack_voice(out, 64, seq, opus, sizeof opus);
}
static int g_dec_calls = 0, g_fresh_calls = 0; static bool g_dec_fail = false;
static bool dec(const uint8_t *d, size_t len, int16_t *pcm)
{
    ++g_dec_calls;
    if (g_dec_fail || len != 60) return false;
    const uint32_t first = d[0] | d[1] << 8 | d[2] << 16 | (uint32_t)d[3] << 24;
    for (int i = 0; i < FRAME; ++i) pcm[i] = sig((int64_t)first + i);
    return true;
}
static void fresh() { ++g_fresh_calls; }
// idx 번째 프레임(표본 idx × 320 부터)이 도착한다. 기기에서는 도착할 때마다 해제 태스크를 깨운다(링버퍼가 한 묶음보다 적을 때)
static void feed(Rx &rx, uint32_t idx) { uint8_t f[64]; rx.push(f, frame_bytes(idx * FRAME, (uint16_t)idx, f)); if (rx.want_pump()) rx.pump(dec, fresh); }
// 출력 태스크 + 해제 태스크의 한 바퀴: 한 묶음 가져가고, 모자라면 채운다
static bool pull(Rx &rx, int16_t *out) { const bool s = rx.fill(out, BLOCK); if (rx.want_pump()) rx.pump(dec, fresh); return s; }
static Pcm pull_n(Rx &rx, int blocks) { Pcm all; for (int i = 0; i < blocks; ++i) { int16_t b[BLOCK]; pull(rx, b); all.insert(all.end(), b, b + BLOCK); } return all; }
// 기기의 박자로 돌린다: 16ms 묶음 blocks 개를 가져가며, 다섯 묶음에 네 프레임 꼴(평균 20ms 마다)로 다음 프레임을 넣는다
struct Flow { uint32_t next = 0; int b = 0; };
static Pcm flow(Rx &rx, Flow &fl, int blocks)
{
    Pcm all;
    for (int i = 0; i < blocks; ++i, ++fl.b) { if (fl.b % 5 != 4) feed(rx, fl.next++); int16_t x[BLOCK]; pull(rx, x); all.insert(all.end(), x, x + BLOCK); }
    return all;
}
// v 안에서 "표본 번호 first 부터의 신호"가 시작하는 자리를 찾는다(앞 5ms 는 크기가 바뀌므로 그 뒤 400표본으로 맞춘다). 없으면 -1.
static long align(const Pcm &v, int64_t first)
{
    for (size_t a = 0; a + FADE + 400 <= v.size(); ++a) {
        bool same = true;
        for (int i = FADE; i < FADE + 400 && same; ++i) if (v[a + (size_t)i] != sig(first + i)) same = false;
        if (same) return (long)a;
    }
    return -1;
}
// a 에서 시작한 신호가 v 의 끝까지 표본 하나 빠짐없이 이어지는가
static bool exact_from(const Pcm &v, long a, int64_t first) { if (a < 0) return false; for (size_t i = (size_t)a + FADE; i < v.size(); ++i) if (v[i] != sig(first + (int64_t)(i - (size_t)a))) return false; return true; }

// 상태 기계를 그 상태로 데려간다(교환기에 붙고 상대도 켜진 채)
static Fsm in_state(State s)
{
    Fsm f;
    if (s == State::OFFLINE) return f;
    f.on(Ev::LINK_UP, 1000); f.on(Ev::PEER, 1000, Why::NONE, true);
    switch (s) {
    case State::DIALING:   f.on(Ev::TAP_CALL, 1100); break;
    case State::RINGBACK:  f.on(Ev::TAP_CALL, 1100); f.on(Ev::RINGING, 1200); break;
    case State::INCOMING:  f.on(Ev::RING, 1100); break;
    case State::ANSWERING: f.on(Ev::RING, 1100); f.on(Ev::TAP_ACCEPT, 1200); break;
    case State::IN_CALL:   f.on(Ev::RING, 1100); f.on(Ev::TAP_ACCEPT, 1200); f.on(Ev::CONNECTED, 1300); break;
    case State::ENDING:    f.on(Ev::RING, 1100); f.on(Ev::TAP_REJECT, 1200); break;
    default: break;
    }
    return f;
}
static const State STATES[] = {State::OFFLINE, State::IDLE, State::DIALING, State::RINGBACK, State::INCOMING, State::ANSWERING, State::IN_CALL, State::ENDING};
static const Ev EVENTS[] = {Ev::LINK_UP, Ev::LINK_DOWN, Ev::PEER, Ev::RINGING, Ev::RING, Ev::CONNECTED, Ev::ENDED, Ev::TAP_CALL, Ev::TAP_CANCEL,
                            Ev::TAP_ACCEPT, Ev::TAP_REJECT, Ev::TAP_HANGUP, Ev::TAP_MUTE, Ev::TICK};
static const Why WHYS[] = {Why::NONE, Why::HANGUP, Why::REJECT, Why::TIMEOUT, Why::OFFLINE, Why::LOST};

// ── 신호음 + 목소리를 기기처럼 잇는다: app.cpp 의 apply(할 일 → 소리·목소리) + audio_out 의 출력 한 묶음(pcm::mix_sources) ──
static tones::Player g_tone; static bool g_tone_has = false; static tones::Tone g_tone_cmd = tones::Tone::NONE;
static Rx *g_rx = nullptr; static bool g_voice_on = false;       // g_voice_on = app.cpp 가 기억하는 켜짐(VOICE 줄을 한 번만 찍으려고)
static bool tone_src(int16_t *out, int n) { const tones::Block b = tones::next_block(g_tone, g_tone_has, g_tone_cmd, out, n); g_tone_has = false; return b.sound; }
static bool voice_src(int16_t *out, int n) { const bool s = g_rx->fill(out, n); if (g_rx->want_pump()) g_rx->pump(dec, fresh); return s; }
static void apply_acts(uint16_t acts)
{
    tones::Tone t;
    if (tones::from_acts(acts, &t)) { g_tone_cmd = t; g_tone_has = true; }      // 소리 먼저(PL3a-D13)
    bool on;
    if (from_acts(acts, &on) && on != g_voice_on) { if (on) g_rx->start(); else g_rx->stop(); g_voice_on = on; }
}
static int mix_block(int16_t *out) { static const pcm::source_fn SRC[2] = {tone_src, voice_src}; int16_t tmp[BLOCK]; return pcm::mix_sources(SRC, 2, out, tmp, BLOCK); }

// ── 통화 한쪽 방향의 시뮬레이션(시각은 µs): 보내는 쪽 16ms 묶음 → Uplink → 가짜 코덱 → 망(TCP: 잃지 않고 순서대로, 늦을 수는 있다)
//    → Receiver → 받는 쪽이 16ms 마다 한 묶음. RAMP 신호라 나온 표본의 값이 곧 "언제 마이크에 들어온 표본인가"다.
struct Sim {
    double tx_ppm = 0;                  // 보내는 쪽 시계가 얼마나 빠른가(백만분율)
    int64_t net_base = 3000, net_jitter = 4000;         // 망 지연 = base + [0, jitter) µs
    int64_t stall_at = -1, stall_len = 0;               // 이 시각부터 이만큼 망이 멈춘다(그동안 보낸 것은 풀릴 때 한꺼번에 도착)
    int64_t rx_phase = 5000;            // 받는 쪽 16ms 박자가 보내는 쪽보다 얼마나 늦게 시작하나
    // 결과
    std::vector<int> delay_ms;          // 묶음마다: 그 묶음의 첫 표본이 마이크에 들어온 때부터 출력 태스크가 가져간 때까지(이어진 묶음만)
    long blocks = 0, broken = 0;        // 가져간 묶음 / 그 가운데 표본이 이어지지 않은 묶음(메웠거나 건너뛴 자리)
    RxStats st;
    int worst_after = 0;                // settle 뒤의 최대 지연
    bool framed_ok = false;             // 보내는 쪽 프레임이 표본·순번 모두 이어졌나
    void run(int seconds, int64_t settle_us = 0)
    {
        g_sig = Sig::RAMP;
        static Rx rx(g_nolock);
        rx.start();
        Uplink up; up.start();
        struct Pkt { int64_t at; uint8_t f[64]; size_t n; };
        std::vector<Pkt> wire; size_t head = 0;
        const double tx_period = 16000.0 / (1.0 + tx_ppm / 1e6);
        double tx_next = tx_period; int64_t rx_next = rx_phase + 16000, last_arrive = 0; int64_t g = 0; uint32_t sent = 0; bool coded = true;
        const int64_t end = (int64_t)seconds * 1000000;
        while (rx_next < end) {
            if (tx_next <= (double)rx_next) {                                   // 보내는 쪽: 한 묶음이 찼다
                const int64_t now = (int64_t)tx_next;
                int16_t blk[BLOCK]; for (int i = 0; i < BLOCK; ++i) blk[i] = sig(g + i);
                up.push(blk); g += BLOCK;
                int16_t fr[FRAME]; uint16_t seq;
                while (up.pop(fr, &seq)) {
                    const uint32_t first = (uint32_t)(DISCARD_BLOCKS * BLOCK) + sent * FRAME;           // 이 프레임의 첫 표본 번호
                    if (fr[0] != sig(first) || fr[FRAME - 1] != sig((int64_t)first + FRAME - 1) || seq != (uint16_t)sent) coded = false;
                    Pkt p; p.n = frame_bytes(first, seq, p.f); ++sent;
                    int64_t at = now + net_base + (net_jitter ? (int64_t)rnd((uint32_t)net_jitter) : 0);
                    if (stall_at >= 0 && at >= stall_at && at < stall_at + stall_len) at = stall_at + stall_len;
                    if (at < last_arrive) at = last_arrive;                     // TCP: 순서대로
                    last_arrive = p.at = at; wire.push_back(p);
                }
                tx_next += tx_period;
                continue;
            }
            const int64_t now = rx_next;                                        // 받는 쪽: 한 묶음을 가져간다
            while (head < wire.size() && wire[head].at <= now) { rx.push(wire[head].f, wire[head].n); ++head; if (rx.want_pump()) rx.pump(dec, fresh); }
            int16_t out[BLOCK]; pull(rx, out);
            ++blocks;
            bool run = true; for (int i = 1; i < BLOCK; ++i) if ((out[i] - out[i - 1] + RAMP_MOD) % RAMP_MOD != 1) { run = false; break; }
            if (!run) { if (peak(Pcm(out, out + BLOCK))) ++broken; }
            else {
                // out[0] = (보낸 쪽 표본 번호) mod 20000. 그 표본이 마이크에 들어온 시각 = 번호 × 62.5µs × (보내는 쪽 시계 보정)
                const double sample_us = 1e6 / RATE / (1.0 + tx_ppm / 1e6);
                const int64_t newest = (int64_t)((double)now / sample_us);       // 지금 마이크에 들어오고 있는 표본 번호
                int64_t k = newest - ((newest - out[0]) % RAMP_MOD + RAMP_MOD) % RAMP_MOD;   // out[0] 과 값이 같은, 지금보다 이른 가장 가까운 번호
                const int d = (int)(((double)now - (double)k * sample_us) / 1000.0);
                delay_ms.push_back(d);
                if (now >= settle_us && d > worst_after) worst_after = d;
            }
            rx_next += 16000;
        }
        st = rx.stats();
        framed_ok = coded;
        g_sig = Sig::SINE;
    }
    int pct(int p) const { if (delay_ms.empty()) return -1; std::vector<int> v = delay_ms; std::sort(v.begin(), v.end()); return v[(v.size() - 1) * (size_t)p / 100]; }
};

// ── Phase 3c (계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D5): 여러 번 멈추는 망에서 지터 버퍼의 규칙을 견준다 ──
//    위 Sim 과 같은 길(보내는 쪽 16ms 묶음 → 프레임 → 망 → Receiver → 받는 쪽 16ms 묶음)에 2026-10-05 아침의 도착 모양을 넣는다:
//    흔들림(jitter) + 짧은 멈춤이 몇 초마다(그동안 보낸 것은 풀릴 때 한꺼번에 — TCP) + 가끔 긴 멈춤, 또는 일정한 주기로만 배달(Wi-Fi 절전의 모양).
struct Net3c {
    int64_t base = 5000, jitter = 20000;                    // 망 지연 = base + [0, jitter) µs
    int64_t stall_every = 0, stall_min = 0, stall_max = 0;  // 짧은 멈춤: 평균 간격(±50%)과 길이 범위(µs). 0 = 없음
    int64_t big_every = 0, big_len = 0;                     // 긴 멈춤
    int64_t clump = 0;                                      // > 0: 이 주기의 경계에서만 배달한다
};
struct Run3c { double mean = 0, urun_min = 0, lost_min = 0, drop_min = 0, played_pct = 0, tgt_mean = 0; int p95 = 0, tgt_max = 0; };
static Run3c run3c(const Net3c &net, const jitterbuf::Policy &pol, int seconds, uint32_t seed)
{
    g_seed = seed; g_sig = Sig::RAMP;
    static Rx rx(g_nolock);
    rx.set_policy(pol);
    rx.start();
    Uplink up; up.start();
    struct Pkt { int64_t at; uint8_t f[64]; size_t n; };
    std::vector<Pkt> wire; size_t head = 0;
    const int64_t end = (int64_t)seconds * 1000000;
    std::vector<std::pair<int64_t, int64_t>> stalls;
    if (net.stall_every) for (int64_t t = net.stall_every; t < end;) { stalls.push_back({t, net.stall_min + (int64_t)rnd((uint32_t)(net.stall_max - net.stall_min + 1))}); t += net.stall_every / 2 + (int64_t)rnd((uint32_t)net.stall_every); }
    if (net.big_every) for (int64_t t = net.big_every; t < end; t += net.big_every) stalls.push_back({t, net.big_len});
    double tx_next = 16000; int64_t rx_next = 5000 + 16000, last_arrive = 0, g = 0; uint32_t sent = 0;
    std::vector<int> delay; double tsum = 0; long tn = 0; Run3c r;
    while (rx_next < end) {
        if (tx_next <= (double)rx_next) {
            const int64_t now = (int64_t)tx_next;
            int16_t blk[BLOCK]; for (int i = 0; i < BLOCK; ++i) blk[i] = sig(g + i);
            up.push(blk); g += BLOCK;
            int16_t fr[FRAME]; uint16_t seq;
            while (up.pop(fr, &seq)) {
                Pkt p; p.n = frame_bytes((uint32_t)(DISCARD_BLOCKS * BLOCK) + sent * FRAME, seq, p.f); ++sent;
                int64_t at = now + net.base + (net.jitter ? (int64_t)rnd((uint32_t)net.jitter) : 0);
                for (const auto &s : stalls) if (at >= s.first && at < s.first + s.second) at = s.first + s.second;
                if (net.clump) at = (at / net.clump + 1) * net.clump;
                if (at < last_arrive) at = last_arrive;                         // TCP: 순서대로
                last_arrive = p.at = at; wire.push_back(p);
            }
            tx_next += 16000;
            continue;
        }
        const int64_t now = rx_next;
        while (head < wire.size() && wire[head].at <= now) { rx.push(wire[head].f, wire[head].n); ++head; if (rx.want_pump()) rx.pump(dec, fresh); }
        int16_t out[BLOCK]; pull(rx, out);
        bool run = true; for (int i = 1; i < BLOCK; ++i) if ((out[i] - out[i - 1] + RAMP_MOD) % RAMP_MOD != 1) { run = false; break; }
        if (run) {
            const double sample_us = 1e6 / RATE;
            const int64_t newest = (int64_t)((double)now / sample_us);
            const int64_t k = newest - ((newest - out[0]) % RAMP_MOD + RAMP_MOD) % RAMP_MOD;
            delay.push_back((int)(((double)now - (double)k * sample_us) / 1000.0));
        }
        const RxStats st = rx.stats(); tsum += st.target; ++tn; if (st.target > r.tgt_max) r.tgt_max = st.target;
        rx_next += 16000;
    }
    const RxStats st = rx.stats(); const double min = seconds / 60.0;
    std::sort(delay.begin(), delay.end());
    if (!delay.empty()) { double sum = 0; for (int d : delay) sum += d; r.mean = sum / (double)delay.size(); r.p95 = delay[delay.size() * 95 / 100]; }
    r.urun_min = st.jb.underruns / min; r.lost_min = st.concealed / min; r.drop_min = (st.jb.overflow_drop + st.jb.catchup_drop + st.jb.late) / min;
    r.played_pct = 100.0 * st.played / (st.frames ? st.frames : 1); r.tgt_mean = tn ? tsum / (double)tn : 0;
    rx.stop(); rx.set_policy(jitterbuf::legacy_policy());
    g_sig = Sig::SINE;
    return r;
}
static Net3c net_quiet() { Net3c n; n.base = 3000; n.jitter = 4000; return n; }
static Net3c net_night() { Net3c n; n.stall_every = 45000000; n.stall_min = 150000; n.stall_max = 250000; return n; }                    // 2026-10-05 02시: 0.15~0.25초 멈춤이 3분에 네 번쯤
static Net3c net_morning() { Net3c n; n.jitter = 40000; n.stall_every = 5500000; n.stall_min = 150000; n.stall_max = 250000; n.big_every = 60000000; n.big_len = 800000; return n; }   // 08시: 30초에 5~6번 + 가끔 0.8초
static Net3c net_powersave() { Net3c n; n.jitter = 10000; n.clump = 307200; return n; }                                                 // Wi-Fi 절전(fix 3 전): 0.3초마다 뭉쳐 온다

int main(int argc, char **argv)
{
    const bool info = argc > 1 && std::string(argv[1]) == "--info";

    // ════ 켜고 끄기 (PH-D6, Review Focus 1·2) ════
    {
        bool on = true;
        CHECK("acts: voice start turns it on", from_acts(A_VOICE_START, &on) && on);
        CHECK("acts: voice stop turns it off", from_acts(A_VOICE_STOP, &on) && !on);
        on = true;
        CHECK("acts: stop wins when both come together (when in doubt the mic stays closed)", from_acts(A_VOICE_START | A_VOICE_STOP, &on) && !on);
        on = true;
        CHECK("acts: nothing about voice -> leave it as it is", !from_acts(A_SEND_CALL | A_TONE_BELL | A_TONE_STOP | A_TONE_END | A_RECONNECT, &on) && on);
        on = false;
        CHECK("acts: the tone and send bits beside it do not matter", from_acts(A_TONE_STOP | A_VOICE_START, &on) && on && from_acts(A_SEND_HANGUP | A_VOICE_STOP | A_TONE_END, &on) && !on);
    }
    {
        // 모든 상태 × 사건 × 사유 × flag: 사건 하나 뒤에 "목소리가 켜져 있다 ⇔ 통화 중(IN_CALL)" 이 늘 맞다
        bool iff = true, never_both = true, start_only_into_call = true; int opens = 0, closes = 0;
        for (State s : STATES) for (Ev e : EVENTS) for (Why w : WHYS) for (int flag = 0; flag < 2; ++flag) for (int64_t now : {(int64_t)2000, (int64_t)100000}) {
            Fsm f = in_state(s);
            bool voice = s == State::IN_CALL;
            const Out o = f.on(e, now, w, flag != 0, 0);
            if ((o.acts & A_VOICE_START) && (o.acts & A_VOICE_STOP)) never_both = false;
            if ((o.acts & A_VOICE_START) && f.state != State::IN_CALL) start_only_into_call = false;
            const bool was = voice;
            from_acts(o.acts, &voice);
            if (voice != (f.state == State::IN_CALL)) iff = false;
            if (!was && voice) ++opens;
            if (was && !voice) ++closes;
        }
        CHECK("state machine: after any single event, voice is on exactly when the state is IN_CALL (every state x event x reason)", iff && opens > 0 && closes > 0);
        CHECK("state machine: start and stop never come in the same batch", never_both);
        CHECK("state machine: start only comes together with entering IN_CALL", start_only_into_call);
    }
    {
        // 받기 전에는 마이크가 열리지 않는다: 대기·거는 중·벨·받는 중 어디서도, connected 가 아닌 사건으로는 켜지지 않는다
        bool closed = true;
        for (State s : {State::OFFLINE, State::IDLE, State::INCOMING, State::ENDING}) for (Ev e : EVENTS) for (Why w : WHYS) {
            Fsm f = in_state(s); bool voice = false;
            from_acts(f.on(e, 2000, w, true, 0).acts, &voice);
            if (voice) closed = false;
        }
        for (State s : {State::DIALING, State::RINGBACK, State::ANSWERING}) for (Ev e : EVENTS) for (Why w : WHYS) {
            Fsm f = in_state(s); bool voice = false;
            from_acts(f.on(e, 2000, w, true, 0).acts, &voice);
            if (voice != (e == Ev::CONNECTED)) closed = false;
        }
        CHECK("never open before the call is accepted: IDLE / INCOMING (ringing, not answered) / ENDING / OFFLINE never turn it on, whatever arrives", closed);
        Fsm f = in_state(State::INCOMING); bool voice = false;
        const Out o = f.on(Ev::CONNECTED, 2000);
        from_acts(o.acts, &voice);
        CHECK("a 'connected' that arrives while still ringing (not answered) does not open the mic", !voice && f.state == State::OFFLINE && !(o.acts & A_VOICE_START));
    }
    {
        // 통화가 끝나는 모든 길: 끊기, 상대 끊김(사유 다섯), 교환기 끊김, 끊김을 못 본 채 다시 붙음
        bool all_stop = true; int ways = 0;
        struct Way { Ev e; Why w; };
        const Way ways_out[] = {{Ev::TAP_HANGUP, Why::NONE}, {Ev::ENDED, Why::HANGUP}, {Ev::ENDED, Why::REJECT}, {Ev::ENDED, Why::TIMEOUT}, {Ev::ENDED, Why::OFFLINE},
                                {Ev::ENDED, Why::LOST}, {Ev::ENDED, Why::NONE}, {Ev::LINK_DOWN, Why::NONE}, {Ev::LINK_UP, Why::NONE}};
        for (const Way &w : ways_out) {
            Fsm f = in_state(State::IN_CALL); bool voice = true;
            const Out o = f.on(w.e, 5000, w.w);
            from_acts(o.acts, &voice);
            if (voice || f.state == State::IN_CALL || !(o.acts & A_VOICE_STOP)) all_stop = false;
            ++ways;
        }
        CHECK("the call ends every way (hang up, peer hangs up x6 reasons, relay link drops, link comes up again unseen): voice is off", all_stop && ways == 9);
        bool stays = true;
        for (Ev e : {Ev::PEER, Ev::RINGING, Ev::RING, Ev::CONNECTED, Ev::TAP_CALL, Ev::TAP_CANCEL, Ev::TAP_ACCEPT, Ev::TAP_REJECT, Ev::TAP_MUTE, Ev::TICK}) {
            Fsm f = in_state(State::IN_CALL); bool voice = true;
            from_acts(f.on(e, 99999999, Why::NONE, false).acts, &voice);
            if (!voice || f.state != State::IN_CALL) stays = false;
        }
        CHECK("nothing else ends a call (stray signals, other buttons, the peer going offline flag, hours passing)", stays);
    }
    {
        // 무작위로 오래: 사건 300만 개(시간도 흐른다 — 시간 제한·재연결 포함). 사건마다 "켜짐 ⇔ IN_CALL".
        g_seed = 20261004; Fsm f; bool voice = false, iff = true; int64_t now = 1000; long calls = 0;
        for (int i = 0; i < 3000000 && iff; ++i) {
            now += rnd(8) == 0 ? rnd(9000) : rnd(40);
            const State before = f.state;
            const Out o = f.on(EVENTS[rnd(14)], now, WHYS[rnd(6)], rnd(4) != 0, (uint8_t)rnd(5));
            from_acts(o.acts, &voice);
            if (voice != (f.state == State::IN_CALL)) iff = false;
            if (before != State::IN_CALL && f.state == State::IN_CALL) ++calls;
        }
        CHECK("3 million random events with time passing: voice on <=> IN_CALL after every one", iff && calls > 1000);
    }
    {
        // 한 묶음의 할 일에서 소리와 목소리를 함께 읽는다(app.cpp apply 의 순서)
        tones::Tone t = tones::Tone::BELL; bool on = false;
        const Out open = in_state(State::RINGBACK).on(Ev::CONNECTED, 2000);
        CHECK("call opens on the caller: one batch (0x180) says 'stop the ringback' and 'voice on'",
              open.acts == (A_TONE_STOP | A_VOICE_START) && tones::from_acts(open.acts, &t) && t == tones::Tone::NONE && from_acts(open.acts, &on) && on);
        Fsm a = in_state(State::INCOMING);
        const Out acc = a.on(Ev::TAP_ACCEPT, 2000); on = false;
        CHECK("on the callee the bell stops one step earlier (0x82), voice only at 'connected'",
              (acc.acts & A_TONE_STOP) && !from_acts(acc.acts, &on) && from_acts(a.on(Ev::CONNECTED, 2100).acts, &on) && on);
        const Out end = in_state(State::IN_CALL).on(Ev::ENDED, 5000, Why::HANGUP); on = true;
        CHECK("call ends: one batch (0x240) says 'voice off' and 'end tone'",
              end.acts == (A_VOICE_STOP | A_TONE_END) && tones::from_acts(end.acts, &t) && t == tones::Tone::END && from_acts(end.acts, &on) && !on);
        const Out hang = in_state(State::IN_CALL).on(Ev::TAP_HANGUP, 5000); on = true;
        CHECK("hanging up: voice goes off in the same batch as the hangup signal (0x248), not after the relay answers",
              hang.acts == (A_SEND_HANGUP | A_VOICE_STOP | A_TONE_END) && from_acts(hang.acts, &on) && !on);
    }

    // ════ 보내는 길 ════
    {
        int16_t slots[8 * IN_SLOTS], mic[8], ref[8];
        for (int i = 0; i < 8; ++i) { slots[i * 4] = (int16_t)(100 + i); slots[i * 4 + 1] = (int16_t)(200 + i); slots[i * 4 + 2] = 7777; slots[i * 4 + 3] = -7777; }
        split_slots(slots, 8, mic, ref);
        bool right = IN_SLOTS == 4 && MIC_SLOT == 0 && REF_SLOT == 1;
        for (int i = 0; i < 8; ++i) if (mic[i] != 100 + i || ref[i] != 200 + i) right = false;
        CHECK("mic chip slots: slot 0 = microphone, slot 1 = speaker loopback (echo reference), slots 2 and 3 unused", right);
    }
    {
        TestTone tone; Pcm v(RATE);
        tone.render(v.data(), RATE, TEST_AMP);
        int crossings = 0; for (int i = 1; i < RATE; ++i) if (v[i - 1] < 0 && v[i] >= 0) ++crossings;
        bool formula = true; for (int i = 0; i < RATE; ++i) if (std::abs(v[i] - TEST_AMP * std::sin(2 * PI * TEST_HZ * i / RATE)) > 3) formula = false;
        CHECK("test tone: 1 kHz at the test level (2000), exact sine", TEST_HZ == 1000 && TEST_AMP == 2000 && crossings >= 999 && crossings <= 1000 && formula && peak(v) <= TEST_AMP && peak(v) >= TEST_AMP - 2);
        Pcm a(100), b(100); TestTone t2; t2.render(a.data(), 100, 1000); t2.render(b.data(), 100, 1000);
        TestTone t3; Pcm c(200); t3.render(c.data(), 200, 1000);
        CHECK("test tone: the phase carries over from block to block (no click)", std::equal(a.begin(), a.end(), c.begin()) && std::equal(b.begin(), b.end(), c.begin() + 100));
    }
    {
        int16_t clean[BLOCK], mic[BLOCK], ref[BLOCK], out[BLOCK]; TestTone tone;
        for (int i = 0; i < BLOCK; ++i) { clean[i] = (int16_t)(100 + i % 7); mic[i] = (int16_t)(-300 - i % 5); ref[i] = 9000; }
        pick(Src::AEC, clean, mic, ref, tone, out);
        bool aec = true; for (int i = 0; i < BLOCK; ++i) if (out[i] != clean[i] * 4) aec = false;
        CHECK("what to send, normally: the echo-cancelled signal, x4 (PL3b-D10: mic gain 24 dB + 12 dB digital)", aec && TX_GAIN_Q8 == 1024);
        pick(Src::MIC, clean, mic, ref, tone, out);
        bool raw = true; for (int i = 0; i < BLOCK; ++i) if (out[i] != mic[i] * 4) raw = false;
        CHECK("diagnostic 'mic': the raw microphone, same gain", raw);
        pick(Src::REF, clean, mic, ref, tone, out);
        CHECK("diagnostic 'ref': the loopback slot; the gain clips at 32767 instead of wrapping", out[0] == 32767 && out[BLOCK - 1] == 32767);
        pick(Src::TONE, clean, mic, ref, tone, out);
        CHECK("diagnostic 'tone': the 1 kHz test tone at its own level (no gain), nothing from the mic", peak(Pcm(out, out + BLOCK)) <= TEST_AMP && peak(Pcm(out, out + BLOCK)) >= TEST_AMP - 2 && out[0] == 0);
        Src s = Src::TONE;
        CHECK("source names for the serial command", parse_src("aec", &s) && s == Src::AEC && parse_src("mic", &s) && s == Src::MIC && parse_src("ref", &s) && s == Src::REF
              && parse_src("tone", &s) && s == Src::TONE && !parse_src("AEC", &s) && !parse_src("", &s) && !parse_src(nullptr, &s) && !parse_src("tone ", &s) && s == Src::TONE
              && std::string(src_name(Src::AEC)) == "aec" && std::string(src_name(Src::REF)) == "ref");
    }
    {
        // 16ms 묶음 → 20ms 프레임. 표본에 번호를 매겨 넣고, 나온 프레임의 표본이 빠짐·겹침 없이 이어지는지 본다
        Uplink up; up.start();
        int16_t blk[BLOCK], fr[FRAME]; uint16_t seq = 99; int16_t g = 0; bool none = true;
        for (int b = 0; b < DISCARD_BLOCKS; ++b) { for (int16_t &v : blk) v = g++; up.push(blk); if (up.pop(fr, &seq)) none = false; }
        CHECK("uplink: the first 8 blocks (128 ms) after the call opens are thrown away (stale DMA + the ringback tail in the room)", DISCARD_BLOCKS == 8 && none && up.discarded() == 8 && up.pending() == 0);
        std::vector<int> per_block; bool contiguous = true, numbered = true; int16_t expect = (int16_t)(DISCARD_BLOCKS * BLOCK); uint16_t want_seq = 0; long frames = 0;
        for (int b = 0; b < 250000; ++b) {                              // 약 67분
            for (int16_t &v : blk) v = g++;
            up.push(blk);
            int n = 0;
            while (up.pop(fr, &seq)) { ++n; ++frames; if (seq != want_seq++) numbered = false; for (int i = 0; i < FRAME; ++i) if (fr[i] != expect++) contiguous = false; }
            if (b < 10) per_block.push_back(n);
        }
        CHECK("uplink: the first frame starts exactly at sample 2048 (nothing older leaks out)", contiguous);
        CHECK("uplink: 67 minutes of blocks -> frames with no sample lost, repeated or reordered", contiguous && frames == 250000L * BLOCK / FRAME);
        CHECK("uplink: sequence numbers count up from 0 and wrap at 65536", numbered && want_seq == (uint16_t)(frames));
        CHECK("uplink: 4 frames per 5 blocks, in the beat 0,1,1,1,1 (16/16/16/32 ms)", per_block == std::vector<int>({0, 1, 1, 1, 1, 0, 1, 1, 1, 1}));
        for (int16_t &v : blk) v = 1234;
        up.push(blk);
        up.start();
        CHECK("uplink: a new call starts clean (numbers from 0, leftovers dropped, discards again)", up.pending() == 0 && up.discarded() == 0 && !up.pop(fr, &seq));
        for (int b = 0; b < DISCARD_BLOCKS + 2; ++b) up.push(blk);
        CHECK("uplink: ... first frame of the new call is number 0", up.pop(fr, &seq) && seq == 0 && fr[0] == 1234);
        Uplink lazy; lazy.start();
        for (int b = 0; b < DISCARD_BLOCKS + 40; ++b) { for (int16_t &v : blk) v = (int16_t)b; lazy.push(blk); }
        CHECK("uplink: if nobody takes the frames it keeps only the newest audio (never grows)", lazy.pending() <= 4 * BLOCK && lazy.pop(fr, &seq) && fr[0] >= DISCARD_BLOCKS + 36);
    }
    {
        // 프레임 포장(relay_proto.h — 교환기 규약 [순번 2][Opus])과 함께: 압축기 출력이 크든 작든 규격 안
        uint8_t buf[MAX_TX_FRAME + 8]; uint8_t opus[400] = {}; uint16_t seq = 0; const uint8_t *p = nullptr; size_t n = 0;
        opus[0] = 0xAB; opus[59] = 0xCD;
        const size_t len = relayproto::pack_voice(buf, sizeof buf, 0xFFFE, opus, 60);
        CHECK("frame: [sequence 2 bytes, big end first][opus] and back", len == 62 && buf[0] == 0xFF && buf[1] == 0xFE && buf[2] == 0xAB && buf[61] == 0xCD
              && relayproto::unpack_voice(buf, len, &seq, &p, &n) && seq == 0xFFFE && n == 60 && p[0] == 0xAB);
        CHECK("frame: our frames always fit the relay limit (400) and our queue slot (200)", MAX_TX_FRAME <= relayproto::MAX_VOICE_FRAME && 160 + 2 <= MAX_TX_FRAME
              && relayproto::pack_voice(buf, MAX_TX_FRAME, 1, opus, MAX_TX_FRAME - 1) == 0 && relayproto::pack_voice(buf, MAX_TX_FRAME, 1, opus, MAX_TX_FRAME - 2) == MAX_TX_FRAME);
    }
    {
        // 보내기 대기열: 밀리면 버린다(쌓지 않는다)
        TxFifo<NoLock> q(g_nolock); TxFrame f; uint8_t d[MAX_TX_FRAME + 1] = {};
        CHECK("tx queue: empty at first", q.size() == 0 && !q.take(&f, 0) && TX_QUEUE == 10 && TX_STALE_MS == 200);
        for (int i = 0; i < 3; ++i) { d[0] = (uint8_t)i; q.put(d, 62, 1000 + 20 * i); }
        CHECK("tx queue: comes out in order with its length", q.size() == 3 && q.take(&f, 1050) && f.data[0] == 0 && f.len == 62 && q.take(&f, 1050) && f.data[0] == 1 && q.size() == 1);
        q.flush();
        for (int i = 0; i < 25; ++i) { d[0] = (uint8_t)i; q.put(d, 62, 2000 + 20 * i); }
        CHECK("tx queue: when the sender is stuck it keeps the newest 10 frames (200 ms) and drops the oldest", q.size() == 10 && q.dropped() == 15 && q.take(&f, 2500) && f.data[0] == 15);
        CHECK("tx queue: frames older than 200 ms are skipped when taken", q.take(&f, 2000 + 20 * 20 + 201) && f.data[0] == 21 && q.dropped() == 15 + 5 && q.size() == 3);
        CHECK("tx queue: everything stale -> nothing to send", !q.take(&f, 99999) && q.size() == 0 && q.dropped() == 23);
        d[0] = 7; q.put(d, 62, 0xFFFFFFF0u);
        CHECK("tx queue: the millisecond clock wrapping around does not make a fresh frame stale", !stale(0x00000010u, 0xFFFFFFF0u) && stale(0x000000C9u, 0xFFFFFFFFu)
              && !stale(0xFFFFFFF8u, 0xFFFFFFF0u) && stale(0xFFFFFFFFu, 0xFFFFFF00u) && q.take(&f, 0x00000010u) && f.data[0] == 7);
        q.put(d, 62, 5000); q.put(d, 62, 5020);
        CHECK("tx queue: flush empties it and says how many (call start, call end, send failure)", q.flush() == 2 && q.size() == 0 && !q.take(&f, 5030) && q.flush() == 0);
        CHECK("tx queue: empty, oversized and null frames are refused", !q.put(d, 0, 1) && !q.put(d, MAX_TX_FRAME + 1, 1) && !q.put(nullptr, 10, 1) && q.put(d, MAX_TX_FRAME, 1) && q.size() == 1);
        q.clear_dropped();
        CHECK("tx queue: the drop counter is cleared per call", q.dropped() == 0);
        uint32_t t = 0; long sent = 0; g_seed = 5; TxFifo<NoLock> r(g_nolock); bool order = true; int last = -1; int worst = 0;
        for (int i = 0; i < 2000000; ++i) {                             // 넣는 쪽 20ms 박자, 꺼내는 쪽은 가끔 오래 막힌다
            t += 20; d[0] = (uint8_t)(i & 0xFF); d[1] = (uint8_t)(i >> 8); d[2] = (uint8_t)(i >> 16); r.put(d, 62, t);
            if (r.size() > worst) worst = r.size();
            if (rnd(50) == 0) { i += (int)rnd(40); t += 0; continue; }
            while (r.take(&f, t)) { const int id = f.data[0] | f.data[1] << 8 | f.data[2] << 16; if (id <= last) order = false; if (stale(t, f.ms)) order = false; last = id; ++sent; }
        }
        CHECK("tx queue: 2 million frames with a sender that stalls now and then: never more than 10 queued, nothing stale or out of order is sent", order && worst <= TX_QUEUE && sent > 1000000);
    }

    // ════ 빠진 프레임 메우기 (PL3b-D7, Review Focus 3) ════
    {
        g_sig = Sig::SINE;
        auto frame = [](uint32_t idx) { Pcm f(FRAME); for (int i = 0; i < FRAME; ++i) f[i] = sig((int64_t)idx * FRAME + i); return f; };
        Concealer c; Pcm all;
        Pcm f0 = frame(0); const Pcm orig0 = f0;
        c.good(f0.data(), 0);
        bool fade_in = f0[0] == 0 && std::equal(f0.begin() + FADE, f0.end(), orig0.begin() + FADE);
        for (int i = 0; i < FADE; ++i) if (std::abs((int)f0[i]) > std::abs((int)orig0[i]) * i / FADE + 1) fade_in = false;
        CHECK("conceal: the first frame of a call fades in over 5 ms, the rest is untouched", fade_in);
        all = f0;
        Pcm f1 = frame(1), f2 = frame(2);
        c.good(f1.data(), 1); c.good(f2.data(), 2);
        CHECK("conceal: frames that follow each other are passed through bit for bit", f1 == frame(1) && f2 == frame(2));
        all.insert(all.end(), f1.begin(), f1.end()); all.insert(all.end(), f2.begin(), f2.end());
        Pcm l1(FRAME), l2(FRAME), l3(FRAME), l4(FRAME);
        c.lost(l1.data()); c.lost(l2.data()); c.lost(l3.data()); c.lost(l4.data());
        for (const Pcm *p : {&l1, &l2, &l3, &l4}) all.insert(all.end(), p->begin(), p->end());
        CHECK("conceal: a lost frame continues from the last sample with no step (mirrored replay)", max_jump(all) <= SIG_STEP);
        CHECK("conceal: it fades over 3 frames (60 ms): about 2/3, 1/3, then exactly silent at the end", peak(l1) > SIG_AMP * 2 / 3 - 100 && peak(l1) <= SIG_AMP && peak(l2) <= SIG_AMP * 2 / 3 + 1
              && peak(l2) > SIG_AMP / 3 - 100 && peak(l3) <= SIG_AMP / 3 + 1 && l3[FRAME - 1] == 0 && jitterbuf::MAX_CONCEAL == 3);
        CHECK("conceal: after that, more lost frames are pure silence", peak(l4) == 0);
        Pcm f7 = frame(7);
        c.good(f7.data(), 7);
        all.insert(all.end(), f7.begin(), f7.end());
        CHECK("conceal: the next real frame fades in (no click after silence)", f7[0] == 0 && max_jump(all) <= JOIN_STEP && std::equal(f7.begin() + FADE, f7.end(), frame(7).begin() + FADE));
        Pcm l5(FRAME);
        c.lost(l5.data());                                              // 한 프레임만 빠짐
        Pcm f9 = frame(9);
        c.good(f9.data(), 9);
        all.insert(all.end(), l5.begin(), l5.end()); all.insert(all.end(), f9.begin(), f9.end());
        CHECK("conceal: one lost frame then a real one — cross-faded over 5 ms, no step anywhere", max_jump(all) <= JOIN_STEP && std::equal(f9.begin() + FADE, f9.end(), frame(9).begin() + FADE) && f9[0] != 0);
        Pcm f20 = frame(20);
        c.good(f20.data(), 20);                                         // 순번이 건너뜀(오래된 프레임을 버린 뒤) — LOST 표시 없이
        all.insert(all.end(), f20.begin(), f20.end());
        CHECK("conceal: a jump in the sequence (old frames were dropped) is joined the same way", max_jump(all) <= JOIN_STEP && f20 != frame(20) && std::equal(f20.begin() + FADE, f20.end(), frame(20).begin() + FADE));
        Pcm s1(FRAME), s2(FRAME);
        c.silence(s1.data()); c.silence(s2.data());
        all.insert(all.end(), s1.begin(), s1.end());
        CHECK("conceal: going silent while sound was playing fades out in 5 ms (no click), then exact zeros", max_jump(all) <= JOIN_STEP && peak(s1, FADE) == 0 && peak(s1, 0, FADE) > 0 && peak(s2) == 0);
        Pcm f21 = frame(21);
        c.good(f21.data(), 21);
        CHECK("conceal: a frame right after silence fades in even if its number follows on", f21[0] == 0);
        c.reset();
        Pcm l6(FRAME, 5);
        c.lost(l6.data());
        CHECK("conceal: nothing to repeat yet (new call) -> a lost frame is silence", peak(l6) == 0);
        Pcm w0 = frame(65535); c.good(w0.data(), 65535); Pcm w1 = frame(65536); c.good(w1.data(), 0);
        CHECK("conceal: sequence 65535 -> 0 counts as consecutive", w1 == frame(65536));
        // 큰 소리(꼭대기까지)여도 넘치지 않는다
        Concealer big; Pcm top(FRAME); for (int i = 0; i < FRAME; ++i) top[i] = i % 2 ? 32767 : -32768;
        big.good(top.data(), 0); Pcm t1 = top; big.good(t1.data(), 1);
        Pcm lost(FRAME); big.lost(lost.data()); Pcm t2 = top; big.good(t2.data(), 3);
        CHECK("conceal: full-scale input never wraps around", peak(lost) <= 32768 && peak(t2) <= 32768 && t1 == top);
    }

    // ════ 받는 길 (PL3b-D5·D6·D13) ════
    {
        g_sig = Sig::SINE; g_dec_calls = g_fresh_calls = 0;
        static Rx rx(g_nolock);
        int16_t out[BLOCK]; for (int16_t &v : out) v = 55;
        feed(rx, 0); feed(rx, 1); feed(rx, 2); feed(rx, 3);
        CHECK("rx: before any call there is no sound and the block is not touched — frames that arrive outside a call are not even stored",
              !rx.open() && !rx.fill(out, BLOCK) && out[0] == 55 && out[BLOCK - 1] == 55 && rx.stats().frames == 0 && rx.stats().depth == 0 && !rx.want_pump() && rx.pump(dec, fresh) == 0);
        rx.start();
        Pcm quiet = pull_n(rx, 10);
        RxStats s = rx.stats();
        CHECK("rx: call open, nothing received yet -> the voice source plays silence (keeps the 16 ms beat going)", peak(quiet) == 0 && s.frames == 0 && s.played == 0);
        CHECK("rx: waiting for the first frames is not counted as an underrun or a concealed frame", s.under == 0 && s.concealed == 0 && s.depth == 0 && s.target == jitterbuf::START_FRAMES);
        CHECK("rx: the decoder is reset once at the start of the call", rx.open() && g_fresh_calls == 1 && g_dec_calls == 0);
        feed(rx, 0); feed(rx, 1);
        CHECK("rx: two frames (40 ms) are not enough to start (target 60 ms)", peak(pull_n(rx, 2)) == 0 && rx.stats().played == 0 && rx.stats().depth == 2);
        feed(rx, 2);
        Flow fl; fl.next = 3;
        Pcm v = flow(rx, fl, 130);                                      // 약 2초
        const long a = align(v, 0);
        CHECK("rx: then the decoded audio comes out in order, sample for sample (no gap, no repeat between 20 ms frames and 16 ms blocks)", a >= 0 && v.size() == 130u * BLOCK && exact_from(v, a, 0));
        CHECK("rx: the first voice sample is taken within one block (16 ms) of the third frame arriving — no silence queued ahead of it", a >= 0 && a < BLOCK);
        CHECK("rx: it starts with a 5 ms fade-in and never clicks", a >= 0 && peak(v, 0, (size_t)a + 1) == 0 && max_jump(v) <= JOIN_STEP);
        s = rx.stats();
        CHECK("rx: counters: received = fed, played, nothing lost, nothing bad", s.frames == fl.next && s.played >= fl.next - 4 && s.concealed == 0 && s.bad == 0 && s.dec_fail == 0 && s.under == 0 && s.jb.late == 0);
        CHECK("rx: the jitter buffer holds 2..3 frames in a steady stream", s.depth >= 2 && s.depth <= 3 && s.jb.overflow_drop == 0 && s.jb.catchup_drop == 0);
        const long ahead = (long)s.played * FRAME - ((long)v.size() - a);            // 해제는 했지만 아직 출력 태스크가 가져가지 않은 표본(링버퍼에 든 것)
        CHECK("rx: it never decodes further ahead than one block + one frame (the ring adds at most 36 ms)", a >= 0 && ahead >= 0 && ahead < BLOCK + FRAME);
        CHECK("rx: it reports the level of what it plays (amplitude 6000 -> RMS about 4243)", s.play_rms > 4150 && s.play_rms < 4330);

        // 통화가 끝난다: 남은 소리를 5ms 에 걸쳐 줄이고 끝 — 그 뒤로는 소리 없음(남은 프레임은 버린다)
        const int16_t last = v.back();
        int16_t tail[BLOCK];
        rx.stop();
        const bool tail_sound = rx.fill(tail, BLOCK);
        Pcm t(tail, tail + BLOCK);
        CHECK("rx: voice off -> one last block that fades the remaining sound out in 5 ms (no click), zeros after", tail_sound && max_jump(t, last) <= JOIN_STEP && peak(t, FADE) == 0 && peak(t, 0, FADE) > 0 && t[FADE - 1] == 0);
        for (int16_t &x : tail) x = 55;
        CHECK("rx: ... and then no sound at all (the output task may sleep), the block is not touched", !rx.fill(tail, BLOCK) && tail[0] == 55 && !rx.fill(tail, BLOCK));
        const uint32_t frames_at_end = rx.stats().frames; const int dec_at_end = g_dec_calls; const int left_in_jb = rx.stats().depth;
        CHECK("rx: a stray wake-up of the decoder task after the call ended decodes nothing (frames were still waiting in the jitter buffer)",
              left_in_jb >= 2 && rx.pump(dec, fresh) == 0 && rx.pump(dec, fresh) == 0 && g_dec_calls == dec_at_end && !rx.want_pump());
        for (uint32_t i = fl.next; i < fl.next + 5; ++i) feed(rx, i);   // 끝난 뒤 뒤늦게 온 프레임
        CHECK("rx: frames that arrive after the call ended are dropped, nothing is decoded any more", rx.stats().frames == frames_at_end && g_dec_calls == dec_at_end && !rx.fill(tail, BLOCK));

        // 다음 통화: 앞 통화의 찌꺼기(지터 버퍼·링버퍼·통계)가 한 표본도 나오지 않는다
        const int fresh_before = g_fresh_calls;
        rx.start();
        Pcm next = pull_n(rx, 12);
        s = rx.stats();
        CHECK("rx: the next call starts silent — nothing of the previous call is played", peak(next) == 0 && s.depth == 0);
        CHECK("rx: ... with fresh counters and a fresh decoder", s.frames == 0 && s.played == 0 && s.jb.played == 0 && s.play_rms == 0 && g_fresh_calls == fresh_before + 1);
        Flow f2; f2.next = 5000;
        Pcm again = flow(rx, f2, 20);
        CHECK("rx: it accepts whatever sequence number the new call starts with", exact_from(again, align(again, 5000LL * FRAME), 5000LL * FRAME));
    }
    {
        // 해제 태스크가 굶으면(출력 태스크가 가져가려는데 비어 있다) 무음으로 메우고 센다. 채우는 중(통화 시작)은 세지 않는다
        g_sig = Sig::SINE;
        static Rx rx(g_nolock); rx.start();
        for (uint32_t i = 0; i < 6; ++i) feed(rx, i);
        pull_n(rx, 4);
        int16_t out[BLOCK];
        rx.fill(out, BLOCK); rx.fill(out, BLOCK); rx.fill(out, BLOCK);                          // pump 를 부르지 않는다
        CHECK("rx: the output task never waits — if the decoder task is starved it plays zeros and counts it", rx.stats().under >= 1 && rx.stats().under <= 3 && peak(Pcm(out, out + BLOCK)) == 0);
        const uint32_t u = rx.stats().under;
        rx.pump(dec, fresh);
        Pcm v = pull_n(rx, 2);
        CHECK("rx: it carries on afterwards", rx.stats().under == u && peak(v) > 0);
    }
    {
        // 규격 밖 프레임·해제 실패
        g_sig = Sig::SINE;
        static Rx rx(g_nolock); rx.start();
        uint8_t f[512] = {};
        rx.push(f, 2); rx.push(f, 401); rx.push(nullptr, 10); rx.push(f, 0);
        CHECK("rx: frames shorter than 3 or longer than 400 bytes are counted and dropped", rx.stats().bad == 4 && rx.stats().frames == 0 && rx.stats().depth == 0);
        rx.push(f, 3); rx.push(f, 400);
        CHECK("rx: 3 and 400 bytes are accepted", rx.stats().frames == 2 && rx.stats().bad == 4);
        rx.start();
        for (uint32_t i = 0; i < 3; ++i) feed(rx, i);
        pull_n(rx, 2);
        g_dec_fail = true;
        for (uint32_t i = 3; i < 6; ++i) feed(rx, i);
        Pcm v = pull_n(rx, 4);
        g_dec_fail = false;
        CHECK("rx: a frame the decoder rejects is concealed like a lost one (no crash, no garbage)", rx.stats().dec_fail >= 1 && rx.stats().concealed == rx.stats().dec_fail && max_jump(v, sig(0)) <= SIG_AMP);
    }

    // ════ 신호음과 목소리가 겹치는 순간 (PL3b-D13) ════
    {
        g_sig = Sig::SINE;
        static Rx rx(g_nolock); g_rx = &rx; g_voice_on = false; g_tone = tones::Player(); g_tone_has = false;
        // 거는 쪽: 연결음이 울리는 중(1초 울림 구간의 한가운데)에 통화가 열린다 — 한 묶음(0x180)에 "연결음 멈춤 + 목소리 켜기"
        Fsm f = in_state(State::RINGBACK);
        apply_acts(A_TONE_RINGBACK);
        Pcm before; int16_t blk[BLOCK];
        for (int i = 0; i < 20; ++i) { mix_block(blk); before.insert(before.end(), blk, blk + BLOCK); }         // 320ms: 연결음이 최대 크기
        CHECK("overlap: ringback alone before the call opens", peak(before) > tones::RINGBACK_AMP * 9 / 10 && !g_voice_on);
        apply_acts(f.on(Ev::CONNECTED, 5000).acts);
        Uplink up; up.start();                                          // 같은 순간 보내는 쪽도 시작한다
        CHECK("overlap: the batch opened the voice and commanded the ringback to stop", g_voice_on && g_tone_has && g_tone_cmd == tones::Tone::NONE);
        const int n1 = mix_block(blk);
        Pcm first(blk, blk + BLOCK);
        CHECK("overlap: in the very first block both sources sound: the ringback fading out + the voice source (still silent)", n1 == 2);
        CHECK("overlap: the ringback is gone within 5 ms, without a click, and the rest of that block is exact silence",
              max_jump(first, before.back()) <= (int)std::ceil(tones::RINGBACK_AMP * 2 * PI * 480 / RATE) + tones::RINGBACK_AMP / tones::FADE + 4
              && peak(first, tones::FADE) == 0 && peak(first, 0, tones::FADE) > 0);
        // 상대의 첫 프레임들이 도착한다(20ms 마다). 지터 버퍼가 60ms 를 채울 때까지는 무음 — 연결음 꼬리와 목소리가 섞여 나가는 일이 없다
        Pcm opening; int blocks_until_voice = -1;
        for (int i = 0; i < 12; ++i) {
            if (i % 5 != 4) feed(rx, (uint32_t)(i - i / 5));
            const int n = mix_block(blk);
            if (n != 1) blocks_until_voice = -100;                      // 연결음 공급자는 더는 소리가 없어야 한다(목소리 공급자만)
            if (blocks_until_voice == -1 && peak(Pcm(blk, blk + BLOCK))) blocks_until_voice = i;
            opening.insert(opening.end(), blk, blk + BLOCK);
        }
        CHECK("overlap: the first voice comes at least 2 blocks (32 ms) after the ringback stopped — its tail in the DMA has played out by then", blocks_until_voice >= 2);
        CHECK("overlap: and it fades in (no click on top of the silence)", max_jump(opening) <= JOIN_STEP && peak(opening) > SIG_AMP / 2);
        // 보내는 쪽: 연결음 꼬리가 방에 남아 있는 동안의 마이크는 상대에게 가지 않는다
        int16_t mic[BLOCK]; for (int16_t &v : mic) v = 3000; int16_t fr[FRAME]; uint16_t seq; bool leaked = false;
        for (int i = 0; i < 6; ++i) { up.push(mic); if (up.pop(fr, &seq)) leaked = true; }                      // 96ms = 멈춤 5ms + DMA 에 이미 든 32ms + 여유
        CHECK("overlap: nothing captured in the first 96 ms after the call opens is sent (the ringback tail stays out of the uplink)", !leaked && DISCARD_BLOCKS * 16 >= 96 + 32);

        // 통화 중: 신호음 공급자는 조용하다 → 섞은 것 = 목소리 그대로
        Pcm talk = opening; int16_t solo[BLOCK]; int n_call = 1; uint32_t nextf = 10;
        for (int i = 12; i < 60; ++i) { if (i % 5 != 4) feed(rx, nextf++); if (mix_block(blk) != 1) n_call = 0; talk.insert(talk.end(), blk, blk + BLOCK); }
        CHECK("in call: only the voice source sounds, and the mix is the decoded voice sample for sample (the silent tone source adds nothing)",
              n_call == 1 && exact_from(talk, align(talk, 0), 0));

        // 끝: 한 묶음(0x240)에 "목소리 끄기 + 종료음". 목소리는 5ms 에 걸쳐 줄고, 종료음은 처음부터 제 모양 그대로 — 겹치는 5ms 는 더해진다
        const int16_t last = blk[BLOCK - 1];
        Fsm c = in_state(State::IN_CALL);
        apply_acts(c.on(Ev::ENDED, 9000, Why::HANGUP).acts);
        CHECK("call end: the batch closed the voice and commanded the end tone", !g_voice_on && g_tone_has && g_tone_cmd == tones::Tone::END);
        const int n_end = mix_block(blk);
        Pcm endblk(blk, blk + BLOCK);
        tones::Player ref_p; ref_p.command(tones::Tone::END); int16_t ref_end[BLOCK]; ref_p.render(ref_end, BLOCK);
        bool tone_exact = true; for (int i = FADE; i < BLOCK; ++i) if (endblk[i] != ref_end[i]) tone_exact = false;
        bool sum_ok = true; for (int i = 0; i < FADE; ++i) if (std::abs(endblk[i] - ref_end[i]) > SIG_AMP) sum_ok = false;
        CHECK("call end: that block = the voice fading out (5 ms) + the end tone starting; after 5 ms it is the end tone alone, sample for sample", n_end == 2 && tone_exact && sum_ok);
        CHECK("call end: no click at the hand-over, no clipping", max_jump(endblk, last) <= JOIN_STEP + tones::END_AMP / tones::FADE + 300 && peak(endblk) < 32767);
        Pcm rest; int sounding = 0;
        for (int i = 0; i < 40; ++i) { const int n = mix_block(blk); if (n) { ++sounding; rest.insert(rest.end(), blk, blk + BLOCK); } if (n > 1) sounding = -1000; }
        tones::Player ref_q; ref_q.command(tones::Tone::END); Pcm ref_all(rest.size() + BLOCK); ref_q.render(ref_all.data(), (int)ref_all.size());
        CHECK("call end: from then on only the end tone (0.52 s), then nobody has sound — the output task sleeps", sounding > 25 && sounding < 35
              && std::equal(rest.begin(), rest.end(), ref_all.begin() + BLOCK) && mix_block(solo) == 0 && mix_block(solo) == 0);
        for (uint32_t i = nextf; i < nextf + 8; ++i) feed(rx, i);       // 끝난 뒤 뒤늦게 온 프레임
        CHECK("call end: frames that arrive late are not played", mix_block(solo) == 0);
        // 끊자마자 다음 전화: 벨만 난다(앞 통화의 목소리가 섞이지 않는다)
        apply_acts(A_TONE_BELL);
        tones::Player ref_b; ref_b.command(tones::Tone::BELL); int16_t ref_bell[BLOCK]; ref_b.render(ref_bell, BLOCK);
        CHECK("a bell right after: exactly the bell, no leftover voice mixed in", mix_block(blk) == 1 && std::equal(blk, blk + BLOCK, ref_bell));
        g_rx = nullptr;
    }

    // ════ 오래·거칠게: 한 방향 통화 시뮬레이션 (Review Focus 3·4) ════
    Sim lan, slow_tx, fast_tx, wan;
    {
        g_seed = 11; lan.run(600);                                      // 10분, 고른 망(3~7ms)
        CHECK("sim LAN 10 min: once started every block continues the previous one — no sample lost or repeated", lan.broken <= 1 && lan.st.concealed == 0 && lan.st.under == 0);
        CHECK("sim LAN: delay from microphone sample to the output task stays within 60..125 ms (capture block + framing + jitter buffer + ring)", lan.pct(0) >= 60 && lan.pct(100) <= 125);
        CHECK("sim LAN: and it does not drift (spread under 2 frames)", lan.pct(100) - lan.pct(0) <= 40);
        g_seed = 12; slow_tx.tx_ppm = -200; slow_tx.run(3600);          // 한 시간, 보내는 쪽 시계가 0.02% 느리다(수정 발진기 오차 ±50ppm 두 개의 2배)
        CHECK("sim 1 h, sender clock 200 ppm slow: a handful of single concealed frames, never a long gap", slow_tx.st.concealed >= 1 && slow_tx.st.concealed <= 60 && slow_tx.st.jb.underruns == slow_tx.st.concealed);
        CHECK("sim 1 h, sender slow: the delay stays bounded", slow_tx.pct(100) <= 150);
        g_seed = 13; fast_tx.tx_ppm = 200; fast_tx.run(3600);
        CHECK("sim 1 h, sender clock 200 ppm fast: frames are dropped now and then to keep up, (almost) nothing concealed", fast_tx.st.jb.catchup_drop >= 1 && fast_tx.st.jb.catchup_drop <= 80 && fast_tx.st.concealed <= 6);
        CHECK("sim 1 h, sender fast: the delay does not pile up", fast_tx.pct(100) <= 150);
        g_seed = 14; wan.net_base = 45000; wan.net_jitter = 30000; wan.run(600);   // 터널 경유(Phase 4): 45~75ms
        CHECK("sim tunnel-like network (45..75 ms, jitter 30 ms) 10 min: plays through, delay under 250 ms", wan.pct(100) <= 250 && wan.st.played > 29000);
    }
    {
        // 망이 멈췄다 풀린다(통화 5초째). 풀리고 2초 뒤부터는 지연이 평소로 돌아와 있다 — 쌓이지 않는다
        bool back = true, bounded = true; std::string detail;
        for (int stall_ms : {60, 150, 300, 600, 1500, 4000}) {
            Sim s; g_seed = 100 + (uint32_t)stall_ms; s.stall_at = 5000000; s.stall_len = (int64_t)stall_ms * 1000;
            s.run(20, 5000000 + (int64_t)stall_ms * 1000 + 2000000);
            if (s.worst_after > 140) back = false;
            if (s.st.depth > jitterbuf::OVERFLOW_FRAMES) bounded = false;
            if (info) detail += "  stall " + std::to_string(stall_ms) + " ms: delay after recovery max " + std::to_string(s.worst_after) + " ms, concealed " + std::to_string(s.st.concealed)
                              + ", dropped " + std::to_string(s.st.jb.overflow_drop + s.st.jb.catchup_drop) + ", underruns " + std::to_string(s.st.jb.underruns) + "\n";
        }
        CHECK("sim stall 60 ms .. 4 s then a burst: 2 s after it clears the delay is back under 140 ms (old audio is dropped, not queued)", back && bounded);
        if (info) printf("%s", detail.c_str());
    }

    // ════ 지연 재기 (PL3b-D12) ════
    {
        PingGen p; Pcm all; int at = -1, at_block = -1; int16_t blk[BLOCK];
        CHECK("ping: idle until started", !p.active());
        p.start();
        for (int b = 0; p.active() && b < 200; ++b) { const int a = p.render(blk, BLOCK); if (a >= 0) { at = a; at_block = b; } all.insert(all.end(), blk, blk + BLOCK); }
        const int start = at_block * BLOCK + at;
        CHECK("ping: 0.4 s of silence, then the beep starts exactly at sample 6400", start == PING_LEAD && PING_LEAD == 6400 && peak(all, 0, (size_t)PING_LEAD) == 0);
        CHECK("ping: a 0.1 s 1 kHz beep at level 4000 with 5 ms edges (no click)", peak(all, (size_t)PING_LEAD, (size_t)(PING_LEAD + PING_BURST)) >= PING_AMP - 2 && peak(all) <= PING_AMP
              && max_jump(all) <= (int)std::ceil(PING_AMP * 2 * PI * 1000 / RATE) + PING_AMP / FADE + 4);
        CHECK("ping: then silence for 0.5 s and it ends by itself (1 s in all)", peak(all, (size_t)(PING_LEAD + PING_BURST)) == 0 && !p.active() && all.size() >= 16000 && all.size() < 16000 + BLOCK);
        CHECK("onset: finds the first sample of the returned beep above the threshold, -1 if there is none",
              onset(all.data(), PING_LEAD, PING_DETECT) == -1 && onset(all.data() + PING_LEAD, BLOCK, PING_DETECT) > 0 && onset(all.data() + PING_LEAD, BLOCK, PING_DETECT) < FADE
              && onset(all.data(), (int)all.size(), 32767) == -1);
        int16_t neg[4] = {0, -1499, -1500, 9};
        CHECK("onset: negative swings count too", onset(neg, 4, PING_DETECT) == 2);
        p.start();
        CHECK("ping: can be started again", p.active() && p.render(blk, BLOCK) == -1);

        SelfTestTone st; Pcm tone; int blocks = 0, first_at = -2;
        CHECK("selftest tone: idle until started (renders silence)", !st.active() && st.render(blk, BLOCK) == -1 && peak(Pcm(blk, blk + BLOCK)) == 0);
        st.start();
        while (st.active() && blocks < 100) { const int a = st.render(blk, BLOCK); if (blocks == 0) first_at = a; tone.insert(tone.end(), blk, blk + BLOCK); ++blocks; }
        CHECK("selftest tone: 0.64 s (40 blocks) of 1 kHz at the test level, starting at once, fading in and out, then it stops by itself",
              first_at == 0 && blocks == 40 && SELFTEST_TONE == 40 * BLOCK && peak(tone) <= TEST_AMP && peak(tone) >= TEST_AMP - 2 && tone.front() == 0 && tone.back() == 0
              && max_jump(tone) <= (int)std::ceil(TEST_AMP * 2 * PI * 1000 / RATE) + TEST_AMP / FADE + 4);
        CHECK("selftest: the tone covers the stale blocks thrown away + the measuring window + the DMA write-ahead",
              SELFTEST_TONE >= (SELFTEST_SKIP_BLOCKS + SELFTEST_BLOCKS + 3) * BLOCK && SELFTEST_SKIP_BLOCKS >= 3 && boardprofile::SELFTEST_REF_MIN == 300);   // 판정선은 기판마다(board_profile.h) — 맥 시험은 Tab5 의 값을 본다
        pcm::Rms r; r.add(tone.data() + SELFTEST_SKIP_BLOCKS * BLOCK, SELFTEST_BLOCKS * BLOCK);
        CHECK("selftest: the tone reads RMS 1414 in the measuring window (what the loopback slot is compared against)", r.value() >= 1410 && r.value() <= 1416);
        st.start(); st.render(blk, BLOCK); st.stop();
        CHECK("selftest tone: stop cuts it (a call opening during the test)", !st.active() && st.render(blk, BLOCK) == -1 && peak(Pcm(blk, blk + BLOCK)) == 0);
    }

    // ── fix 2 (PL3b-D21~D27): 실행 중 조절(txgain), 진단 출처 제한, 숫자 인자 읽기, 세대 번호, 굶음 셈 접근 ──────────────────────────────────
    {
        TxGain g;
        CHECK("txgain: the default is 4x, the same as the old constant (existing behaviour is unchanged)", g.get() == 4 && g.q8() == TX_GAIN_Q8 && TX_GAIN_Q8 == 1024 && TX_GAIN_DEFAULT == 4);
        bool all_ok = true; for (int n = TX_GAIN_MIN; n <= TX_GAIN_MAX; ++n) { if (!g.set(n) || g.get() != n || g.q8() != n * 256) all_ok = false; }
        CHECK("txgain: 1, 2, 3 and 4 are accepted and read back (q8 = n x 256)", all_ok && TX_GAIN_MIN == 1 && TX_GAIN_MAX == 4);
        g.set(2);
        const int bad_vals[] = {0, 5, -1, -4, 100, 256, 1024, INT32_MAX, INT32_MIN};
        bool kept = true; for (int v : bad_vals) if (g.set(v) || g.get() != 2 || g.q8() != 512) kept = false;
        CHECK("txgain: a value outside 1..4 is refused and the value does not change", kept);
    }
    {
        int16_t clean[BLOCK], mic[BLOCK], ref[BLOCK], out[BLOCK]; TestTone tone;
        for (int i = 0; i < BLOCK; ++i) { clean[i] = (int16_t)(100 + i % 7); mic[i] = (int16_t)(-300 - i % 5); ref[i] = (int16_t)(1000 + i % 3); }
        bool scaled = true;
        for (int n = 1; n <= 4; ++n) {
            pick(Src::AEC, clean, mic, ref, tone, out, n * 256);
            for (int i = 0; i < BLOCK; ++i) if (out[i] != clean[i] * n) scaled = false;
            pick(Src::MIC, clean, mic, ref, tone, out, n * 256);
            for (int i = 0; i < BLOCK; ++i) if (out[i] != mic[i] * n) scaled = false;
            pick(Src::REF, clean, mic, ref, tone, out, n * 256);
            for (int i = 0; i < BLOCK; ++i) if (out[i] != ref[i] * n) scaled = false;
        }
        CHECK("txgain: at 1x, 2x, 3x and 4x the level is exactly that multiple — for the echo-cancelled signal and for the diagnostic sources mic and ref too", scaled);
        pick(Src::AEC, clean, mic, ref, tone, out);
        bool dflt = true; for (int i = 0; i < BLOCK; ++i) if (out[i] != clean[i] * 4) dflt = false;
        CHECK("txgain: pick without a gain argument still multiplies by 4 (the old call sites)", dflt);
        TestTone t1, t2; int16_t a[BLOCK], b[BLOCK];
        pick(Src::TONE, clean, mic, ref, t1, a, 256); pick(Src::TONE, clean, mic, ref, t2, b, 1024);
        CHECK("txgain: the test tone is never multiplied", std::equal(a, a + BLOCK, b) && peak(Pcm(a, a + BLOCK)) <= TEST_AMP);
    }
    {
        int16_t clean[BLOCK], mic[BLOCK], ref[BLOCK], out[BLOCK]; TestTone tone;
        for (int i = 0; i < BLOCK; ++i) { clean[i] = (int16_t)(i % 2 ? 20000 : -20000); mic[i] = (int16_t)(i % 2 ? 32767 : -32768); ref[i] = (int16_t)(i % 2 ? 9000 : -9000); }
        bool ok_clip = true;
        for (int n = 1; n <= 4; ++n) {
            pick(Src::AEC, clean, mic, ref, tone, out, n * 256);
            for (int i = 0; i < BLOCK; ++i) {
                const int want = std::max(-32768, std::min(32767, (i % 2 ? 20000 : -20000) * n));
                if (out[i] != want) ok_clip = false;
            }
            pick(Src::MIC, clean, mic, ref, tone, out, n * 256);
            for (int i = 0; i < BLOCK; ++i) if (out[i] != (i % 2 ? 32767 : -32768)) ok_clip = false;     // 이미 끝값: 몇 배든 끝값에 머문다(돌지 않는다)
            pick(Src::REF, clean, mic, ref, tone, out, n * 256);
            for (int i = 0; i < BLOCK; ++i) { const int want = std::max(-32768, std::min(32767, (i % 2 ? 9000 : -9000) * n)); if (out[i] != want) ok_clip = false; }
        }
        CHECK("txgain: overflow is cut at +32767 / -32768 for every gain and every source (it never wraps around)", ok_clip);
    }
    {
        int v = 77;
        CHECK("parse_uint: plain numbers inside the range", parse_uint("3", 1, 4, &v) && v == 3 && parse_uint("100", 0, 100, &v) && v == 100 && parse_uint("0", 0, 100, &v) && v == 0 && parse_uint("007", 0, 100, &v) && v == 7);
        v = 77;
        const char *bad_text[] = {"", " ", "abc", "-1", "+3", "3x", "x3", " 3", "3 ", "3.5", "1e1", "0x10", "99999999999", "٣"};
        bool refused = true; for (const char *t : bad_text) if (parse_uint(t, 0, 100, &v) || v != 77) refused = false;
        CHECK("parse_uint: empty, signs, letters, spaces, decimals and huge numbers are refused, the output stays", refused && !parse_uint(nullptr, 0, 100, &v) && v == 77);
        CHECK("parse_uint: outside the range is refused (101 for volume, 0 and 5 for txgain)", !parse_uint("101", 0, 100, &v) && !parse_uint("0", 1, 4, &v) && !parse_uint("5", 1, 4, &v) && v == 77);
    }
    {
        const Src all[] = {Src::AEC, Src::MIC, Src::REF, Src::TONE};
        bool allowed_quiet = true, allowed_loud = true;
        for (Src s : all) { if (!src_allowed(s, false)) allowed_quiet = false; }
        for (Src s : all) { const bool want = s == Src::AEC || s == Src::TONE; if (src_allowed(s, true) != want) allowed_loud = false; }
        CHECK("diagnostic sources: with the speaker off all four are allowed; with the speaker on only aec and tone (mic and ref would close an electrical loop)", allowed_quiet && allowed_loud);
        CHECK("diagnostic sources: if the speaker comes on during a call, mic and ref fall back to aec; the others stay",
              effective_src(Src::MIC, true) == Src::AEC && effective_src(Src::REF, true) == Src::AEC && effective_src(Src::AEC, true) == Src::AEC && effective_src(Src::TONE, true) == Src::TONE
              && effective_src(Src::MIC, false) == Src::MIC && effective_src(Src::REF, false) == Src::REF);
    }
    {
        GenWatch w;
        CHECK("capture start: the first look at generation 0 with the mode unchanged does not start", !w.fresh(0, false));
        CHECK("capture start: a mode change starts", w.fresh(0, true));
        CHECK("capture start: nothing changed, nothing starts", !w.fresh(0, false) && !w.fresh(0, false));
        CHECK("capture start: call -> off -> call inside one 16 ms block (mode looks the same, generation went up) starts again", w.fresh(1, false) && !w.fresh(1, false));
        CHECK("capture start: the generation going up by several steps starts once", w.fresh(5, false) && !w.fresh(5, false) && w.fresh(0xFFFFFFFFu, false) && w.fresh(0, false));
    }
    {
        g_sig = Sig::SINE;
        static Rx rx(g_nolock); rx.start();
        CHECK("rx: under() starts at 0 and while filling is not counted", rx.under() == 0);
        int16_t out[BLOCK]; rx.fill(out, BLOCK);
        CHECK("rx: ... still 0 after a block taken while the first frames are awaited", rx.under() == 0 && rx.stats().under == 0);
        for (uint32_t i = 0; i < 6; ++i) feed(rx, i);
        pull_n(rx, 4);
        rx.fill(out, BLOCK); rx.fill(out, BLOCK); rx.fill(out, BLOCK);                          // pump 를 부르지 않는다 → 굶음
        CHECK("rx: under() (a lock-free read for the output task) equals the count in stats()", rx.under() >= 1 && rx.under() == rx.stats().under);
        rx.start();
        CHECK("rx: a new call resets it", rx.under() == 0);
    }

    // ════ Phase 3c: 측정 수단과 지터 버퍼 규칙 (계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D4·D5) ════
    {
        // 시험음의 주파수를 바꾼다(vtone) — 여러 주파수로 "어느 대역이 어디서 깎이는가"를 잰다
        bool exact = true; int cross[3] = {0, 0, 0}; const int hz[3] = {300, 3000, 6000};
        for (int k = 0; k < 3; ++k) {
            TestTone t; t.set_hz(hz[k]); Pcm v(RATE); t.render(v.data(), RATE, TEST_AMP);
            for (int i = 1; i < RATE; ++i) if (v[i - 1] < 0 && v[i] >= 0) ++cross[k];
            for (int i = 0; i < RATE; ++i) if (std::abs(v[i] - TEST_AMP * std::sin(2 * PI * hz[k] * i / RATE)) > 6) exact = false;
        }
        CHECK("test tone: set_hz gives an exact sine at 300, 3000 and 6000 Hz, same level", exact && std::abs(cross[0] - 300) <= 1 && std::abs(cross[1] - 3000) <= 1 && std::abs(cross[2] - 6000) <= 1);
        TestTone a, b; Pcm x(RATE), y(RATE); a.render(x.data(), RATE, TEST_AMP); b.set_hz(TEST_HZ); b.render(y.data(), RATE, TEST_AMP);
        CHECK("test tone: without set_hz it is still 1 kHz (the 3b tone, sample for sample)", x == y);
        TestTone c; Pcm z(400); c.render(z.data(), 200, 4000); c.set_hz(2500); c.render(z.data() + 200, 200, 4000);
        CHECK("test tone: changing the frequency keeps the phase (no click at the switch)", max_jump(z) <= (int)std::ceil(4000 * 2 * PI * 2500 / RATE) + 4);
    }
    {
        // 출처 probe: 마이크 자리에 시험음을 넣어 에코 제거를 거치게 한다. 보내는 것은 에코 제거 결과(평소와 같은 길 — 같은 이득)
        int16_t clean[BLOCK], mic[BLOCK], ref[BLOCK], out[BLOCK], want[BLOCK]; TestTone tone;
        for (int i = 0; i < BLOCK; ++i) { clean[i] = (int16_t)(50 + i % 9); mic[i] = (int16_t)-700; ref[i] = 9000; }
        pick(Src::PROBE, clean, mic, ref, tone, out); pick(Src::AEC, clean, mic, ref, tone, want);
        Src s = Src::AEC;
        CHECK("diagnostic 'probe': what is sent is the echo-cancelled signal with the same gain as normal (the tone goes in before the canceller, on the device)",
              std::equal(out, out + BLOCK, want) && parse_src("probe", &s) && s == Src::PROBE && std::string(src_name(Src::PROBE)) == "probe" && PROBE_AMP == 500 && PROBE_AMP * TX_GAIN_DEFAULT == TEST_AMP);
        CHECK("diagnostic 'probe': allowed with the speaker on (the real microphone is not used, so no loop closes); mic / ref still are not",
              src_allowed(Src::PROBE, true) && src_allowed(Src::PROBE, false) && effective_src(Src::PROBE, true) == Src::PROBE && !src_allowed(Src::MIC, true) && !src_allowed(Src::REF, true));
        CHECK("the leveller / echo press apply only to the echo-cancelled path (aec, probe) — diagnostic mic / ref keep the fixed gain so 'vping' stays comparable",
              uses_dyn(Src::AEC) && uses_dyn(Src::PROBE) && !uses_dyn(Src::MIC) && !uses_dyn(Src::REF) && !uses_dyn(Src::TONE));
    }
    {
        int16_t m[8] = {0, 32767, -32768, 32766, -32767, 100, 32767, -1};
        CHECK("mic clip count: samples sitting at either end of the 16-bit range (an echo that overloads the mic cannot be cancelled)", clipped(m, 8) == 3 && clipped(m, 1) == 0 && clipped(m, 0) == 0);
    }
    {
        ArrivalGap g;
        CHECK("arrival gap: nothing yet -> 0", g.max_ms() == 0);
        g.hit(1000);
        CHECK("arrival gap: one frame alone is not a gap", g.max_ms() == 0);
        g.hit(1020); g.hit(1041); g.hit(1300); g.hit(1320);
        CHECK("arrival gap: the longest pause between two frames (259 ms here)", g.max_ms() == 259);
        g.reset(); g.hit(5000);
        CHECK("arrival gap: reset per call — the pause between two calls is not counted", g.max_ms() == 0);
        ArrivalGap w; w.hit(0xFFFFFFF0u); w.hit(0x00000010u);
        CHECK("arrival gap: the millisecond clock wrapping around is still 32 ms", w.max_ms() == 32);
    }
    {
        // Receiver 가 규칙을 지터 버퍼에 건넨다. 통화가 새로 열려도 남는다
        g_sig = Sig::SINE;
        static Rx rx(g_nolock);
        rx.set_policy(jitterbuf::tuned_policy(4)); rx.start();
        uint32_t idx = 0;
        for (int round = 0; round < 8; ++round) {                               // 3초 틀고 0.4초 멈추기를 여덟 번
            for (int i = 0; i < 150; ++i) { feed(rx, idx++); int16_t b[BLOCK]; pull(rx, b); if (i % 4 == 3) pull(rx, b); }
            pull_n(rx, 25); idx += 20;
        }
        const RxStats capped = rx.stats();
        rx.start();
        for (int i = 0; i < 3; ++i) feed(rx, i);
        rx.set_policy(jitterbuf::legacy_policy()); rx.start(); idx = 0;
        for (int round = 0; round < 8; ++round) {
            for (int i = 0; i < 150; ++i) { feed(rx, idx++); int16_t b[BLOCK]; pull(rx, b); if (i % 4 == 3) pull(rx, b); }
            pull_n(rx, 25); idx += 20;
        }
        const RxStats old = rx.stats();
        CHECK("rx: set_policy reaches the jitter buffer — eight stalls 3 s apart leave the tuned rule at its ceiling (4) and the 3b rule well above it",
              capped.jb.underruns >= 7 && capped.target <= 4 && old.target >= 8);
    }
    Run3c m_old, m_jb4, m_jb6, n_old, n_jb4, q_old, q_jb4, p_old, p_jb4;
    {
        // 아침의 망: 지금 규칙은 목표가 상한(200ms)까지 올라가 머문다. 새 규칙은 상한에서 멈추고 지연이 0.1초 넘게 준다. 값은 끊김이 느는 것
        const jitterbuf::Policy old = jitterbuf::legacy_policy(), jb4 = jitterbuf::tuned_policy(4), jb6 = jitterbuf::tuned_policy(6);
        m_old = run3c(net_morning(), old, 180, 1000); m_jb4 = run3c(net_morning(), jb4, 180, 1000); m_jb6 = run3c(net_morning(), jb6, 180, 1000);
        CHECK("sim, the morning network (stalls of 0.15-0.25 s every 5.5 s): the 3b rule climbs to its 200 ms ceiling and stays (mean delay over 220 ms)",
              m_old.tgt_max == 10 && m_old.tgt_mean > 7.5 && m_old.mean > 220);
        CHECK("sim, the morning network: 'jb 4' holds the target near 80 ms and cuts the mean delay by more than 100 ms", m_jb4.tgt_mean < 4.3 && m_jb4.mean < m_old.mean - 100 && m_jb4.p95 < m_old.p95 - 100);
        CHECK("sim, the morning network: the price — more underruns (each stall longer than the buffer is heard), still over 96 % of the frames played",
              m_jb4.urun_min > m_old.urun_min && m_jb4.urun_min < 16 && m_jb4.played_pct > 96.0 && m_old.played_pct > 98.0);
        CHECK("sim, the morning network: 'jb 6' sits in between (delay and underruns)", m_jb6.mean > m_jb4.mean + 20 && m_jb6.mean < m_old.mean - 50 && m_jb6.played_pct >= m_jb4.played_pct);
        n_old = run3c(net_night(), old, 180, 1000); n_jb4 = run3c(net_night(), jb4, 180, 1000);
        CHECK("sim, the night network (a stall every 45 s): the tuned rule is no worse — same underruns, the delay does not stay high after a stall",
              n_jb4.urun_min <= n_old.urun_min + 0.4 && n_jb4.mean <= n_old.mean + 1 && n_jb4.p95 <= n_old.p95 && n_jb4.played_pct > 99.3);
        q_old = run3c(net_quiet(), old, 120, 1000); q_jb4 = run3c(net_quiet(), jb4, 120, 1000);
        CHECK("sim, a quiet network: no underrun, nothing dropped, the same delay under either rule", q_old.urun_min == 0 && q_jb4.urun_min == 0 && q_jb4.drop_min == 0 && std::abs(q_jb4.mean - q_old.mean) < 2);
        p_old = run3c(net_powersave(), old, 60, 1000); p_jb4 = run3c(net_powersave(), jb4, 60, 1000);
        CHECK("sim, frames arriving in 0.3 s clumps (Wi-Fi power save back on): the burst rule falls back to the 3b behaviour instead of throwing most of the audio away",
              p_jb4.tgt_max == 10 && p_jb4.played_pct > p_old.played_pct - 2.0 && p_old.played_pct > 80.0);
    }

    // ════ Phase 3c: 수신 무음 감시 (계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D10, Review Focus 3) ════
    {
        RxWatch w; w.start(1000);
        bool quiet = true; for (uint32_t t = 1000; t < 1000 + 3600000; t += 20) if (w.check(t, 0)) quiet = false;
        CHECK("rx watch: before the first frame it never fires, however long (a peer whose audio is dead sends none — that call must not be cut)", quiet && RX_SILENCE_MS == 5000 && w.silent_ms(999999) == 0);
        w.start(1000);
        bool early = false; uint32_t frames = 0, t = 1000;
        for (; t < 61000; t += 20) { ++frames; if (w.check(t, frames)) early = true; }                // 1분 동안 20ms 마다 프레임
        CHECK("rx watch: frames keep arriving -> it never fires", !early);
        for (; t < 61000 + 4980; t += 20) if (w.check(t, frames)) early = true;                       // 프레임이 멎었다: 4.98초까지
        CHECK("rx watch: 4.98 s of silence is not enough", !early && w.silent_ms(t) >= 4900);
        bool fired = false; int times = 0;
        for (; t < 61000 + 20000; t += 20) if (w.check(t, frames)) { if (!fired) fired = (t - 60980) >= 5000 && (t - 60980) <= 5020; ++times; }
        CHECK("rx watch: it fires at 5 s of silence, exactly once", fired && times == 1);
        // 멈췄다 풀리는 망: 4.9초 멈춤이 되풀이돼도 끊지 않는다
        RxWatch s; s.start(0); bool cut = false; frames = 0; t = 0;
        for (int round = 0; round < 50; ++round) { for (int i = 0; i < 50; ++i, t += 20) { ++frames; if (s.check(t, frames)) cut = true; } for (int i = 0; i < 245; ++i, t += 20) if (s.check(t, frames)) cut = true; }
        CHECK("rx watch: stalls of 4.9 s, fifty times over, do not cut the call (only 5 s without a single frame does)", !cut);
        // 새 통화: 받은 프레임 수가 0 으로 돌아가고 다시 센다
        RxWatch n; n.start(100); n.check(120, 1); bool f1 = false; for (uint32_t u = 140; u <= 5200; u += 20) if (n.check(u, 1)) f1 = true;
        n.start(9000);
        bool f2 = false; for (uint32_t u = 9000; u < 9000 + 30000; u += 20) if (n.check(u, 0)) f2 = true;
        n.check(40000, 3); bool f3 = false; for (uint32_t u = 40020; u <= 45100; u += 20) if (n.check(u, 3)) f3 = true;
        CHECK("rx watch: a new call starts over — not armed until its own first frame, then fires again after its own 5 s", f1 && !f2 && f3);
        RxWatch c; c.start(0xFFFFF000u); c.check(0xFFFFF100u, 5);
        bool wrapped_early = false; for (uint32_t u = 0xFFFFF120u; u != 0x00000480u; u += 0x20) if (c.check(u, 5)) wrapped_early = true;      // 0x480 = 4.99초 뒤(시계가 0 을 지나간다)
        CHECK("rx watch: the millisecond clock wrapping around neither fires early nor is missed", !wrapped_early && c.check(0xFFFFF100u + 5000u, 5));
    }
    {
        // 상태 기계와 함께: 새 사건(VOICE_LOST)을 넣어도 "목소리가 켜져 있다 ⇔ 통화 중"이 모든 상태 × 사건 × 사유에서 맞는다
        static const Ev EVENTS3C[] = {Ev::LINK_UP, Ev::LINK_DOWN, Ev::PEER, Ev::RINGING, Ev::RING, Ev::CONNECTED, Ev::ENDED, Ev::TAP_CALL, Ev::TAP_CANCEL,
                                      Ev::TAP_ACCEPT, Ev::TAP_REJECT, Ev::TAP_HANGUP, Ev::TAP_MUTE, Ev::TICK, Ev::VOICE_LOST};
        bool iff = true, start_only_into_call = true;
        for (State s : STATES) for (Ev e : EVENTS3C) for (Why w : WHYS) for (int flag = 0; flag < 2; ++flag) {
            Fsm f = in_state(s); bool voice = s == State::IN_CALL;
            const Out o = f.on(e, 100000, w, flag != 0, 0);
            if ((o.acts & A_VOICE_START) && f.state != State::IN_CALL) start_only_into_call = false;
            from_acts(o.acts, &voice);
            if (voice != (f.state == State::IN_CALL)) iff = false;
        }
        CHECK("state machine with the new 'voice lost' event: voice is on exactly when the state is IN_CALL (every state x 15 events x reason)", iff && start_only_into_call);
        Fsm f = in_state(State::IN_CALL); bool voice = true;
        const Out o = f.on(Ev::VOICE_LOST, 9000);
        from_acts(o.acts, &voice);
        CHECK("voice lost: the mic closes in the same batch as the hangup signal (0x248) — not after the relay answers", !voice && o.acts == 0x248 && f.state == State::ENDING);
        bool never = true;
        for (State s : {State::OFFLINE, State::IDLE, State::DIALING, State::RINGBACK, State::INCOMING, State::ANSWERING, State::ENDING}) {
            Fsm g = in_state(s); bool v = false; from_acts(g.on(Ev::VOICE_LOST, 2000).acts, &v); if (v || g.state != s) never = false;
        }
        CHECK("voice lost never opens the mic and never moves any other state", never);
        g_seed = 20261005; Fsm r; bool rv = false, riff = true; int64_t now = 1000; long lost = 0;
        for (int i = 0; i < 2000000 && riff; ++i) {
            now += rnd(8) == 0 ? rnd(9000) : rnd(40);
            const State before = r.state;
            const Out o2 = r.on(EVENTS3C[rnd(15)], now, WHYS[rnd(6)], rnd(4) != 0, (uint8_t)rnd(5));
            from_acts(o2.acts, &rv);
            if (rv != (r.state == State::IN_CALL)) riff = false;
            if (before == State::IN_CALL && r.state == State::ENDING && o2.notice == Notice::LOST) ++lost;
        }
        CHECK("2 million random events including 'voice lost': voice on <=> IN_CALL after every one", riff && lost > 100);
    }
    {
        g_sig = Sig::SINE;
        static Rx rx(g_nolock); rx.start();
        CHECK("rx: frames() is 0 when a call opens", rx.frames() == 0);
        for (uint32_t i = 0; i < 7; ++i) feed(rx, i);
        CHECK("rx: frames() (a lock-free read for the app task's watchdog) equals the count in stats()", rx.frames() == 7 && rx.stats().frames == 7);
        rx.start();
        CHECK("rx: a new call resets it", rx.frames() == 0);
        // 감시와 함께: 프레임이 오다가 멎으면 5초 뒤 한 번
        RxWatch w; w.start(0); uint32_t t = 0; bool fired = false;
        for (uint32_t i = 0; i < 100; ++i, t += 20) { feed(rx, i); if (w.check(t, rx.frames())) fired = true; }
        const bool during = fired;
        for (int i = 0; i < 260; ++i, t += 20) if (w.check(t, rx.frames())) fired = true;
        CHECK("rx + watch: frames flowing -> quiet; the peer vanishes -> one alarm 5 s later", !during && fired);
    }

    {
        // Phase 3c(fix 2 재검수 Minor 6): 읽는 16ms 사이에 세대가 올랐으면 그 묶음은 앞 통화의 것 — 버린다
        GenWatch w;
        const bool first = w.fresh(1, true);
        CHECK("gen watch: right after a call starts the block just read is not stale", first && !w.stale(1));
        CHECK("gen watch: the generation moved on while the mic block was being read -> that block is stale (it belongs to the previous call)", w.stale(2) && w.stale(7));
        CHECK("gen watch: asking does not change anything — the next round still sees the new call as fresh", w.stale(2) && w.fresh(2, false) && !w.stale(2) && !w.fresh(2, false));
    }

    if (info) {
        printf("\n-- delay simulation (ms from a sample entering the microphone to the output task taking it; add DMA write-ahead + codec/AEC internals) --\n");
        const struct { const char *name; const Sim *s; } rows[] = {{"LAN 3-7 ms, 10 min", &lan}, {"sender 200 ppm slow, 1 h", &slow_tx}, {"sender 200 ppm fast, 1 h", &fast_tx}, {"tunnel-like 45-75 ms, 10 min", &wan}};
        for (const auto &r : rows)
            printf("  %-30s min %3d  median %3d  p95 %3d  max %3d   concealed %u  dropped %u  underruns %u  jb depth/target %d/%d\n", r.name, r.s->pct(0), r.s->pct(50), r.s->pct(95), r.s->pct(100),
                   (unsigned)r.s->st.concealed, (unsigned)(r.s->st.jb.overflow_drop + r.s->st.jb.catchup_drop), (unsigned)r.s->st.jb.underruns, r.s->st.depth, r.s->st.target);
        int lo = 9999, hi = 0; long sum = 0, n = 0;                     // 고른 망에서, 두 기기의 16ms 박자가 어긋난 정도(0~15ms)와 통화가 열린 순간을 바꿔 가며 60초씩
        for (int phase = 0; phase < 16; ++phase) for (uint32_t seed = 1; seed <= 4; ++seed) {
            Sim s; g_seed = seed * 977 + (uint32_t)phase; s.rx_phase = phase * 1000 + (int64_t)rnd(1000); s.run(60);
            const int m = s.pct(50); lo = std::min(lo, m); hi = std::max(hi, m); sum += m; ++n;
        }
        printf("  %-30s median delay over 64 runs: min %d  mean %ld  max %d\n", "LAN, start phase swept", lo, sum / n, hi);
    }
    if (info) {
        printf("\n-- Phase 3c: jitter rule vs network (one way, mic sample -> output task; add DMA 32 ms + codec/AEC internals). 180 s each (power save 60 s) --\n");
        printf("  %-34s %-7s %5s %5s %7s %7s %7s %7s %5s %4s\n", "network", "rule", "mean", "p95", "urun/m", "lost/m", "drop/m", "play%", "tgt", "max");
        const struct { const char *name; Net3c net; int sec; } nets[] = {{"quiet (3-7 ms)", net_quiet(), 180}, {"night (stall / 45 s)", net_night(), 180}, {"morning (stall / 5.5 s + 0.8 s / min)", net_morning(), 180}, {"power save (0.3 s clumps)", net_powersave(), 60}};
        const struct { const char *name; jitterbuf::Policy pol; } rules[] = {{"jb 0", jitterbuf::legacy_policy()}, {"jb 4", jitterbuf::tuned_policy(4)}, {"jb 5", jitterbuf::tuned_policy(5)}, {"jb 6", jitterbuf::tuned_policy(6)}, {"jb 8", jitterbuf::tuned_policy(8)}};
        for (const auto &n : nets) for (const auto &r : rules) {
            Run3c a; const int N = 4;
            for (int i = 0; i < N; ++i) { const Run3c x = run3c(n.net, r.pol, n.sec, 1000u + (uint32_t)i * 77u); a.mean += x.mean / N; a.p95 += x.p95; a.urun_min += x.urun_min / N; a.lost_min += x.lost_min / N; a.drop_min += x.drop_min / N; a.played_pct += x.played_pct / N; a.tgt_mean += x.tgt_mean / N; a.tgt_max = std::max(a.tgt_max, x.tgt_max); }
            printf("  %-34s %-7s %5.0f %5d %7.1f %7.1f %7.1f %7.2f %5.1f %4d\n", n.name, r.name, a.mean, a.p95 / N, a.urun_min, a.lost_min, a.drop_min, a.played_pct, a.tgt_mean, a.tgt_max);
        }
        (void)m_old; (void)m_jb4; (void)m_jb6; (void)n_old; (void)n_jb4; (void)q_old; (void)q_jb4; (void)p_old; (void)p_jb4;
    }
    printf("\n%d ok, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
