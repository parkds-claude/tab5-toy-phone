// test_tones.cpp — tones.h 단위 시험:
//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_tones.cpp -o /tmp/t_tones && /tmp/t_tones
// 신호음의 박자·주파수·크기, 딸깍 방지(5ms), 멈춤 약속(수신거부 설계서 §5.3), 상태 기계의 할 일 → 소리를 고정한다
// (계획 docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md PL3a-D1·D2·D5·D12, Review Focus 1~5).
#include "tones.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
using namespace tones;
using namespace callfsm;
static int ok = 0, bad = 0;
#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)

typedef std::vector<int16_t> Pcm;
static const double PI = 3.14159265358979323846;

static Pcm play(Player &p, int samples) { Pcm v((size_t)samples); p.render(v.data(), samples); return v; }
static Pcm cat(Pcm a, const Pcm &b) { a.insert(a.end(), b.begin(), b.end()); return a; }
static int peak(const Pcm &v, size_t from = 0, size_t to = SIZE_MAX)
{
    int m = 0;
    for (size_t i = from; i < v.size() && i < to; ++i) m = std::max(m, std::abs((int)v[i]));
    return m;
}
static bool quiet(const Pcm &v, size_t from = 0, size_t to = SIZE_MAX) { return peak(v, from, to) == 0; }
static int max_jump(const Pcm &v)                      // 이웃 표본 차이의 최댓값(첫 표본은 0 에서 시작한 것으로 본다)
{
    int m = 0, prev = 0;
    for (int16_t s : v) { m = std::max(m, std::abs((int)s - prev)); prev = s; }
    return m;
}
static int jump_bound(int amp, int hz) { return (int)std::ceil(amp * 2 * PI * hz / RATE) + amp / FADE + 4; }   // 사인 기울기 + 크기 옮김 + 반올림
static double ideal(const Seg &s, int amp, uint32_t off)
{
    const double t = (double)off / RATE;
    const double w = s.f2 ? (std::sin(2 * PI * s.f1 * t) + std::sin(2 * PI * s.f2 * t)) / 2 : std::sin(2 * PI * s.f1 * t);
    return amp * w;
}
static Seg seg_at_ms(Tone t, uint32_t ms) { Where w = locate(pattern(t), (uint64_t)ms * PER_MS); return w.seg < 0 ? Seg{0, 0, 0} : pattern(t).seg[w.seg]; }
static bool sounding_at_ms(Tone t, uint32_t ms) { Where w = locate(pattern(t), (uint64_t)ms * PER_MS); return w.seg >= 0 && pattern(t).seg[w.seg].f1 != 0; }
static Pcm fresh(Tone t, int samples) { Player p; p.command(t); return play(p, samples); }

// 상태 기계 + 소리를 기기처럼 잇는다(app.cpp 의 apply + tone_out.cpp 의 공급자):
//   사건마다 from_acts → 길이 1 우편함(마지막 명령만 남는다) / 16ms(256표본)마다 next_block 과 TICK / 찍힐 TONE 줄을 log 에 모은다.
typedef std::vector<std::string> Log;
static std::string last(const Log &l) { return l.empty() ? std::string() : l.back(); }
struct Rig {
    Fsm f; Player p; int64_t now = 1000;
    bool has = false; Tone box = Tone::NONE;
    Log log;
    Rig() { f.on(Ev::LINK_UP, now); f.on(Ev::PEER, now, Why::NONE, true); }
    void apply(const Out &o) { Tone t; if (from_acts(o.acts, &t)) { box = t; has = true; } }
    void ev(Ev e, Why w = Why::NONE, uint8_t msg = 0) { apply(f.on(e, now, w, false, msg)); }
    Pcm run(int ms)
    {
        Pcm all;
        all.reserve((size_t)ms * PER_MS + 256);
        for (int done = 0; done < ms; done += 16) {
            int16_t buf[256] = {};
            const Block b = next_block(p, has, box, buf, 256);
            has = false;
            if (b.off != Tone::NONE) log.push_back(std::string(tone_name(b.off)) + " off");
            if (b.on != Tone::NONE) log.push_back(std::string(tone_name(b.on)) + " on");
            all.insert(all.end(), buf, buf + 256);               // 낼 것이 없는 묶음은 0(기기에서는 DMA 가 0 을 낸다)
            now += 16; apply(f.on(Ev::TICK, now));
        }
        return all;
    }
};

static int tone_bits(uint16_t acts) { return !!(acts & A_TONE_RINGBACK) + !!(acts & A_TONE_BELL) + !!(acts & A_TONE_END) + !!(acts & A_TONE_STOP); }

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

int main()
{
    // ════ 박자 계산 (PL3a-D1) ════
    CHECK("bell: 800 Hz at 0 ms, 1000 Hz at 50 ms, alternating every 50 ms",
          seg_at_ms(Tone::BELL, 0).f1 == 800 && seg_at_ms(Tone::BELL, 49).f1 == 800 && seg_at_ms(Tone::BELL, 50).f1 == 1000
          && seg_at_ms(Tone::BELL, 100).f1 == 800 && seg_at_ms(Tone::BELL, 950).f1 == 1000 && seg_at_ms(Tone::BELL, 999).f1 == 1000);
    CHECK("bell: rings 1 s then 2 s quiet, then again (3 s cycle)",
          sounding_at_ms(Tone::BELL, 999) && !sounding_at_ms(Tone::BELL, 1000) && !sounding_at_ms(Tone::BELL, 2999)
          && sounding_at_ms(Tone::BELL, 3000) && seg_at_ms(Tone::BELL, 3000).f1 == 800 && seg_at_ms(Tone::BELL, 3050).f1 == 1000
          && sounding_at_ms(Tone::BELL, 30999) && !sounding_at_ms(Tone::BELL, 31000));
    CHECK("ringback: 440+480 Hz together for 1 s, then 2 s quiet, repeating",
          seg_at_ms(Tone::RINGBACK, 0).f1 == 440 && seg_at_ms(Tone::RINGBACK, 0).f2 == 480 && sounding_at_ms(Tone::RINGBACK, 999)
          && !sounding_at_ms(Tone::RINGBACK, 1000) && !sounding_at_ms(Tone::RINGBACK, 2999) && sounding_at_ms(Tone::RINGBACK, 3000));
    CHECK("end: 660 Hz 160 ms, 80 ms gap, 440 Hz 280 ms, once (finished at 520 ms)",
          seg_at_ms(Tone::END, 0).f1 == 660 && seg_at_ms(Tone::END, 159).f1 == 660 && !sounding_at_ms(Tone::END, 160)
          && !sounding_at_ms(Tone::END, 239) && seg_at_ms(Tone::END, 240).f1 == 440 && seg_at_ms(Tone::END, 519).f1 == 440
          && locate(pattern(Tone::END), 520 * PER_MS).seg == -1 && locate(pattern(Tone::END), 100000 * PER_MS).seg == -1);
    CHECK("cycle lengths: bell 3 s, ringback 3 s, end 0.52 s; none is empty",
          cycle_len(pattern(Tone::BELL)) == 3000u * PER_MS && cycle_len(pattern(Tone::RINGBACK)) == 3000u * PER_MS
          && cycle_len(pattern(Tone::END)) == 520u * PER_MS && pattern(Tone::NONE).n == 0 && locate(pattern(Tone::NONE), 0).seg == -1);
    CHECK("16 kHz, and the fade is 5 ms = 80 samples (PL3a-D5)", RATE == 16000 && PER_MS == 16 && FADE == 80);
    CHECK("every sounding segment is longer than two fades (fade in + fade out fit)", [] {
        for (Tone t : {Tone::BELL, Tone::RINGBACK, Tone::END})
            for (int i = 0; i < pattern(t).n; ++i) if (pattern(t).seg[i].f1 && seg_len(pattern(t).seg[i]) <= 2u * FADE) return false;
        return true; }());
    CHECK("levels (PL3a-D2): bell 8000, ringback 5300, end 3700",
          pattern(Tone::BELL).amp == 8000 && pattern(Tone::RINGBACK).amp == 5300 && pattern(Tone::END).amp == 3700);
    CHECK("names for the serial log", std::string(tone_name(Tone::NONE)) == "NONE" && std::string(tone_name(Tone::BELL)) == "BELL"
          && std::string(tone_name(Tone::RINGBACK)) == "RINGBACK" && std::string(tone_name(Tone::END)) == "END");

    // ════ 소리 만들기: 주파수·크기·5ms 페이드 (PL3a-D5) ════
    for (Tone t : {Tone::BELL, Tone::RINGBACK, Tone::END}) {
        const Pattern &p = pattern(t);
        const Pcm v = fresh(t, (int)cycle_len(p));
        bool formula = true, envelope = true, edges = true, gaps = true;
        uint32_t start = 0;
        for (int i = 0; i < p.n; ++i) {
            const Seg &s = p.seg[i]; const uint32_t len = seg_len(s);
            for (uint32_t off = 0; off < len; ++off) {
                const int x = v[start + off];
                if (!s.f1) { if (x) gaps = false; continue; }
                const uint32_t g = off < (uint32_t)FADE ? off + 1 : off + FADE >= len ? len - 1 - off : (uint32_t)FADE;   // 크기 단계
                if (std::abs(x) > (int)(p.amp * g / FADE) + 1) envelope = false;
                if (g == (uint32_t)FADE && std::fabs(x - ideal(s, p.amp, off)) > 4) formula = false;
            }
            if (s.f1 && (v[start] == 0 ? false : std::abs((int)v[start]) > p.amp / FADE + 1)) edges = false;   // 칸의 첫 표본은 아주 작다
            if (s.f1 && v[start + len - 1] != 0) edges = false;                                               // 칸의 마지막 표본은 0
            start += len;
        }
        const std::string n = tone_name(t);
        CHECK((n + ": samples follow the sine formula (frequency and level), within 4 of 32767").c_str(), formula);
        CHECK((n + ": 5 ms fade in and out at every segment edge").c_str(), envelope && edges);
        CHECK((n + ": quiet segments are exactly zero").c_str(), gaps);
        CHECK((n + ": peak = its level (no louder, at most 1% under)").c_str(), peak(v) <= p.amp && peak(v) >= p.amp * 99 / 100);
    }
    CHECK("no clicks: neighbouring samples never jump more than a sine at the highest frequency allows",
          max_jump(fresh(Tone::BELL, 6000 * PER_MS)) <= jump_bound(BELL_AMP, 1000)
          && max_jump(fresh(Tone::RINGBACK, 6000 * PER_MS)) <= jump_bound(RINGBACK_AMP, 480)
          && max_jump(fresh(Tone::END, 600 * PER_MS)) <= jump_bound(END_AMP, 660));

    // ════ 멈춤 약속 (수신거부 설계서 §5.3) ════
    {
        Player p; p.command(Tone::BELL);
        const Pcm a = play(p, 525 * PER_MS);                   // 525ms = 1000Hz 칸 한가운데(크기 최대)
        CHECK("bell mid-tone is at full level before the stop", std::abs((int)a.back()) > 0 && p.current() == Tone::BELL);
        p.command(Tone::NONE);
        const Pcm b = play(p, FADE);
        CHECK("stop mid-tone: silent within 5 ms and the bell is off", b.back() == 0 && p.current() == Tone::NONE);
        CHECK("stop mid-tone: no click (fades out)", max_jump(cat(a, b)) <= jump_bound(BELL_AMP, 1000));
        CHECK("after a stop it never rings again by itself (60 s)", quiet(play(p, 60000 * PER_MS)) && p.current() == Tone::NONE);
        p.command(Tone::NONE); p.command(Tone::NONE);
        CHECK("a stop after the stop is harmless", quiet(play(p, 1000 * PER_MS)) && p.current() == Tone::NONE && p.starts() == 1);
    }
    {
        Player p;
        p.command(Tone::NONE);
        CHECK("stop with nothing playing: nothing happens", quiet(play(p, 100 * PER_MS)) && p.idle() && p.starts() == 0);
        p.command(Tone::BELL); play(p, 1500 * PER_MS);         // 벨의 2초 쉼 한가운데
        p.command(Tone::NONE);
        CHECK("stop during the bell's quiet gap: off at once", p.current() == Tone::NONE && quiet(play(p, 3000 * PER_MS)));
    }
    {
        Player p; p.command(Tone::END);
        const Pcm a = play(p, 520 * PER_MS);
        CHECK("end tone is on while it plays", p.current() == Tone::END && peak(a) > 0);
        const Pcm b = play(p, 2 * PER_MS);
        CHECK("end tone plays once and goes off by itself at 0.52 s", p.current() == Tone::NONE && quiet(b));
        CHECK("end tone does not come back (10 s)", quiet(play(p, 10000 * PER_MS)) && p.starts() == 1);
    }

    // ════ 바꿈·겹침 (Review Focus 3·5) ════
    {
        Player p; p.command(Tone::RINGBACK);
        const Pcm a = play(p, 500 * PER_MS);                   // 연결음 한가운데
        p.command(Tone::END);
        const Pcm b = play(p, FADE);
        CHECK("end replaces the ringback after a 5 ms fade", p.current() == Tone::END && p.pos() == 0 && b.back() == 0);
        const Pcm c = play(p, 600 * PER_MS);
        CHECK("after the switch the end tone is exactly the end tone alone", c == fresh(Tone::END, 600 * PER_MS));
        CHECK("ringback -> end: no click", max_jump(cat(cat(a, b), c)) <= jump_bound(RINGBACK_AMP, 660));
    }
    {
        Player p; p.command(Tone::END);
        const Pcm a = play(p, 100 * PER_MS);                   // 종료음이 울리는 중에 다음 전화
        p.command(Tone::BELL);
        const Pcm b = play(p, FADE);
        const Pcm c = play(p, 3000 * PER_MS);
        CHECK("end tone then an immediate new call: the bell takes over within 5 ms", p.current() == Tone::BELL && p.starts() == 2);
        CHECK("... and it is exactly the bell alone (not swallowed, not mixed)", c == fresh(Tone::BELL, 3000 * PER_MS));
        CHECK("end -> bell: no click and never louder than the bell", max_jump(cat(cat(a, b), c)) <= jump_bound(BELL_AMP, 1000) && peak(cat(cat(a, b), c)) <= BELL_AMP);
    }
    {
        Player p; p.command(Tone::BELL); play(p, 525 * PER_MS);
        p.command(Tone::NONE); p.command(Tone::BELL);          // 한 바퀴 안에 멈춤 → 벨(ended → ring)
        play(p, FADE);
        CHECK("stop then bell in one batch: the bell (latest command wins), restarted", p.current() == Tone::BELL && p.starts() == 2 && p.pos() == 0);
        p.command(Tone::BELL); p.command(Tone::NONE);
        play(p, FADE + 1);
        CHECK("bell then stop in one batch: silent", p.current() == Tone::NONE && quiet(play(p, 3000 * PER_MS)));
    }
    {
        Player p; Pcm all;
        for (int i = 0; i < 6; ++i) {                          // 아주 짧은 전화가 연달아(울림 10ms → 멈춤 → …)
            p.command(Tone::BELL); all = cat(all, play(p, 10 * PER_MS));
            p.command(Tone::NONE); all = cat(all, play(p, 3 * PER_MS));
        }
        all = cat(all, play(p, 100 * PER_MS));
        CHECK("very short calls in a row: ends silent, never louder than one bell, no clicks",
              p.idle() && quiet(all, all.size() - 50 * PER_MS) && peak(all) <= BELL_AMP && max_jump(all) <= jump_bound(BELL_AMP, 1000));
    }

    // ════ 기록 줄: TONE <이름> on|off (PL3a-D11) ════
    {
        Tone off, on;
        transitions(Tone::NONE, 0, Tone::BELL, 1, &off, &on);
        CHECK("log: nothing -> bell = 'BELL on' only", off == Tone::NONE && on == Tone::BELL);
        transitions(Tone::BELL, 1, Tone::NONE, 1, &off, &on);
        CHECK("log: bell -> silent = 'BELL off' only", off == Tone::BELL && on == Tone::NONE);
        transitions(Tone::RINGBACK, 1, Tone::END, 2, &off, &on);
        CHECK("log: ringback -> end = 'RINGBACK off' then 'END on'", off == Tone::RINGBACK && on == Tone::END);
        transitions(Tone::BELL, 1, Tone::BELL, 2, &off, &on);
        CHECK("log: the bell restarted = 'BELL off' then 'BELL on'", off == Tone::BELL && on == Tone::BELL);
        transitions(Tone::BELL, 3, Tone::BELL, 3, &off, &on);
        CHECK("log: still the same bell = nothing", off == Tone::NONE && on == Tone::NONE);
        transitions(Tone::NONE, 3, Tone::NONE, 3, &off, &on);
        CHECK("log: still silent = nothing", off == Tone::NONE && on == Tone::NONE);
    }

    // ════ 할 일 → 소리 (PL3a-D12) ════
    {
        Tone t = Tone::BELL;
        CHECK("from_acts: bell / ringback / end / stop", from_acts(A_TONE_BELL, &t) && t == Tone::BELL && from_acts(A_TONE_RINGBACK, &t) && t == Tone::RINGBACK
              && from_acts(A_TONE_END, &t) && t == Tone::END && from_acts(A_TONE_STOP, &t) && t == Tone::NONE);
        t = Tone::END;
        CHECK("from_acts: no tone bit = leave the sound alone", !from_acts(0, &t) && !from_acts(A_SEND_CALL | A_SEND_ACCEPT | A_SEND_REJECT | A_SEND_HANGUP | A_VOICE_START | A_VOICE_STOP | A_RECONNECT, &t) && t == Tone::END);
        CHECK("from_acts: real combinations (accept, connect, hang up in call)", from_acts(A_SEND_ACCEPT | A_TONE_STOP, &t) && t == Tone::NONE
              && from_acts(A_TONE_STOP | A_VOICE_START, &t) && t == Tone::NONE && from_acts(A_SEND_HANGUP | A_VOICE_STOP | A_TONE_END, &t) && t == Tone::END);
        CHECK("from_acts: a start bit wins over stop (end > bell > ringback)", from_acts(A_TONE_STOP | A_TONE_BELL, &t) && t == Tone::BELL
              && from_acts(A_TONE_STOP | A_TONE_END | A_TONE_BELL, &t) && t == Tone::END && from_acts(A_TONE_RINGBACK | A_TONE_BELL, &t) && t == Tone::BELL);
    }
    {
        const State states[] = { State::OFFLINE, State::IDLE, State::DIALING, State::RINGBACK, State::INCOMING, State::ANSWERING, State::IN_CALL, State::ENDING };
        const Ev evs[] = { Ev::LINK_UP, Ev::LINK_DOWN, Ev::PEER, Ev::RINGING, Ev::RING, Ev::CONNECTED, Ev::ENDED, Ev::TAP_CALL, Ev::TAP_CANCEL,
                           Ev::TAP_ACCEPT, Ev::TAP_REJECT, Ev::TAP_HANGUP, Ev::TAP_MUTE, Ev::TICK };
        const Why whys[] = { Why::NONE, Why::HANGUP, Why::REJECT, Why::TIMEOUT, Why::OFFLINE, Why::LOST };
        int cases = 0, two_bits = 0, bell_wrong = 0, bell_missing = 0, ring_in_incoming = 0;
        for (State s : states) for (bool muted : {false, true}) for (Ev e : evs) for (Why w : whys) for (bool flag : {false, true})
            for (uint8_t msg : {(uint8_t)0, (uint8_t)2}) for (int64_t at : {(int64_t)1400, (int64_t)100000}) {
                if (muted && s != State::INCOMING) continue;
                Fsm f = in_state(s);
                if (muted) f.on(Ev::TAP_MUTE, 1300);
                const State before = f.state;
                const Out o = f.on(e, at, w, flag, msg);
                ++cases;
                if (tone_bits(o.acts) > 1) ++two_bits;
                if ((o.acts & A_TONE_BELL) && !(before == State::IDLE && f.state == State::INCOMING)) ++bell_wrong;
                if (before != State::INCOMING && f.state == State::INCOMING && !(o.acts & A_TONE_BELL)) ++bell_missing;
                if (before == State::INCOMING && (o.acts & (A_TONE_BELL | A_TONE_RINGBACK))) ++ring_in_incoming;
            }
        printf("       (state x event x why x flag x msg x time: %d cases)\n", cases);
        CHECK("state machine: never two tone bits at once (every state x event)", cases > 5000 && two_bits == 0);
        CHECK("state machine: bell starts only on IDLE -> INCOMING, and always then", bell_wrong == 0 && bell_missing == 0);
        CHECK("state machine: nothing while ringing (muted or not) restarts a bell or ringback", ring_in_incoming == 0);
    }

    // ════ 출력 한 묶음(next_block): 명령 → 소리 → 기록 줄 ════
    {
        Player p; int16_t buf[256]; for (int16_t &x : buf) x = 77;
        Block b = next_block(p, false, Tone::NONE, buf, 256);
        CHECK("block: nothing to play = no sound, buffer untouched, no log line", !b.sound && buf[0] == 77 && buf[255] == 77 && b.off == Tone::NONE && b.on == Tone::NONE);
        b = next_block(p, true, Tone::NONE, buf, 256);
        CHECK("block: a stop while silent = still nothing", !b.sound && buf[0] == 77 && b.off == Tone::NONE && b.on == Tone::NONE);
        b = next_block(p, true, Tone::BELL, buf, 256);
        CHECK("block: bell command = sound in this very block, 'BELL on'", b.sound && buf[0] == 0 && peak(Pcm(buf, buf + 256)) > 0 && b.off == Tone::NONE && b.on == Tone::BELL);
        b = next_block(p, false, Tone::NONE, buf, 256);
        CHECK("block: bell goes on, no new line", b.sound && b.off == Tone::NONE && b.on == Tone::NONE);
        b = next_block(p, true, Tone::NONE, buf, 256);
        CHECK("block: stop = the fade-out is still sound, 'BELL off', then silence", b.sound && buf[255] == 0 && b.off == Tone::BELL && b.on == Tone::NONE
              && !next_block(p, false, Tone::NONE, buf, 256).sound);
    }

    // ════ 상태 기계 + 소리 (Review Focus 1~5) ════
    {
        Rig r; r.ev(Ev::RING);
        const Pcm a = r.run(1000);
        CHECK("call comes in: the bell rings", r.p.current() == Tone::BELL && peak(a) >= BELL_AMP * 99 / 100);
        r.ev(Ev::TAP_MUTE);
        const Pcm b = r.run(16);
        CHECK("mute: silent within 5 ms, still ringing state", quiet(b, FADE) && r.p.idle() && r.f.state == State::INCOMING);
        r.ev(Ev::RING);                                          // 엇갈려 온 ring — 벨을 다시 켜지 않는다
        r.ev(Ev::TAP_MUTE);                                      // 끈 벨을 또 누름
        const Pcm c = r.run(27000);
        CHECK("muted: stays silent for the rest of that call (stray ring, second mute, 27 s)", quiet(c) && r.p.idle() && r.f.state == State::INCOMING);
        r.ev(Ev::ENDED, Why::TIMEOUT);                           // 교환기 30초 → 부재중(멈춤 한 번 더)
        const Pcm d = r.run(1000);
        CHECK("muted call becomes missed: silent, no end tone", quiet(d) && r.p.idle() && r.f.state == State::IDLE && r.f.missed == 1);
        CHECK("log of a muted call: 'BELL on', 'BELL off' — nothing more", r.log == (Log{"BELL on", "BELL off"}));
        r.ev(Ev::RING);
        const Pcm e = r.run(500);
        CHECK("the next call rings again", peak(e) > 0 && r.p.current() == Tone::BELL && last(r.log) == "BELL on");
    }
    {
        struct End { const char *name; Ev e; Why w; uint8_t msg; };
        const End ends[] = { {"accept", Ev::TAP_ACCEPT, Why::NONE, 0}, {"reject", Ev::TAP_REJECT, Why::NONE, 0}, {"message reject", Ev::TAP_REJECT, Why::NONE, 2},
                             {"caller cancels", Ev::ENDED, Why::HANGUP, 0}, {"relay timeout", Ev::ENDED, Why::TIMEOUT, 0}, {"caller vanished", Ev::ENDED, Why::OFFLINE, 0},
                             {"relay link drops", Ev::LINK_DOWN, Why::NONE, 0}, {"connected without accept", Ev::CONNECTED, Why::NONE, 0} };
        bool all = true;
        for (const End &x : ends) {
            Rig r; r.ev(Ev::RING); r.run(528);                   // 528ms = 1000Hz 칸 한가운데(크기 최대)
            r.ev(x.e, x.w, x.msg);
            const Pcm a = r.run(32);
            const Pcm b = r.run(5000);
            if (!(quiet(a, FADE) && quiet(b) && r.p.idle() && r.log == (Log{"BELL on", "BELL off"}))) { all = false; printf("       bell left sounding after: %s\n", x.name); }
        }
        CHECK("bell ends every way (accept, reject, message reject, cancel, timeout, offline, link down, odd connected): silent in 5 ms, stays silent", all);
        Rig g; g.ev(Ev::RING);
        const Pcm a = g.run(41000);                              // 교환기가 끝내지 않을 때: 40초 가드
        CHECK("bell stuck 40 s (relay silent): the guard stops it", g.f.state == State::OFFLINE && g.p.idle() && quiet(a, a.size() - 900 * PER_MS)
              && g.log == (Log{"BELL on", "BELL off"}));
    }
    {
        Rig r; r.ev(Ev::RING); r.run(528);
        r.ev(Ev::LINK_DOWN);
        const Pcm a = r.run(1000);
        CHECK("ringing when the relay link drops: silent", quiet(a, FADE) && r.p.idle());
        r.ev(Ev::LINK_UP); r.ev(Ev::PEER); r.ev(Ev::RING);
        const Pcm b = r.run(500);
        CHECK("... and after reconnecting the next call rings", peak(b) > 0 && r.p.current() == Tone::BELL);
    }
    {
        Rig r; r.ev(Ev::TAP_CALL); r.ev(Ev::RINGING);
        const Pcm a = r.run(1000);
        CHECK("caller: ringback while the other side rings", peak(a) > 0 && r.p.current() == Tone::RINGBACK);
        r.ev(Ev::ENDED, Why::REJECT);
        const Pcm b = r.run(608);
        const Pcm c = r.run(3000);
        CHECK("caller rejected: ringback stops, end tone once, then silent", r.p.idle() && peak(b) > 0 && quiet(c)
              && r.log == (Log{"RINGBACK on", "RINGBACK off", "END on", "END off"}));
        r.ev(Ev::TAP_CALL); r.ev(Ev::RINGING); r.run(500);
        r.ev(Ev::CONNECTED);
        const Pcm d = r.run(1000);
        CHECK("caller connected: ringback stops, no end tone", quiet(d, FADE) && r.p.idle() && r.f.state == State::IN_CALL && last(r.log) == "RINGBACK off");
        r.ev(Ev::TAP_HANGUP);
        const Pcm e = r.run(304);
        CHECK("hang up in a call: end tone", r.p.current() == Tone::END && peak(e) > 0 && last(r.log) == "END on");
        r.ev(Ev::ENDED, Why::HANGUP);                            // 내 hangup 의 ended — 종료음은 끊기지 않고 끝까지
        const Pcm g = r.run(96);
        CHECK("my hang-up's ended does not cut the end tone", r.p.current() == Tone::END && r.f.state == State::IDLE && peak(g) > 0);
        r.run(304);
        CHECK("... which then ends by itself", r.p.idle() && last(r.log) == "END off");
    }
    {
        Rig r; r.ev(Ev::RING); r.run(528);
        r.ev(Ev::ENDED, Why::TIMEOUT); r.ev(Ev::RING);           // 한 바퀴 안에 ended → ring: 우편함에는 벨만 남는다
        const Pcm a = r.run(16);
        CHECK("ended then ring in one batch: after a 5 ms fade the new call's bell rings from its start",
              r.p.current() == Tone::BELL && r.p.starts() == 2 && Pcm(a.begin() + FADE, a.end()) == fresh(Tone::BELL, (int)a.size() - FADE)
              && r.log == (Log{"BELL on", "BELL off", "BELL on"}));
    }
    {
        Rig r; r.ev(Ev::RING); r.ev(Ev::ENDED, Why::HANGUP);     // 출력 태스크가 꺼내기도 전에 벨 → 멈춤(PL3a-D6 의 비용)
        const Pcm a = r.run(1000);
        CHECK("bell and stop before the output task looked: not a sample, not a log line, missed call counted", quiet(a) && r.log.empty() && r.f.missed == 1);
    }
    {
        Rig r; bool each_rang = true;
        for (int i = 0; i < 5; ++i) {                            // 아주 짧은 전화가 연달아: 울림 48ms → 상대 취소 → 16ms 뒤 다시
            r.ev(Ev::RING);
            if (peak(r.run(48)) == 0) each_rang = false;
            r.ev(Ev::ENDED, Why::HANGUP);
            r.run(16);
        }
        const Pcm tail = r.run(3000);
        CHECK("short calls in a row: each one rings, none is left sounding", each_rang && r.p.idle() && quiet(tail) && r.f.missed == 5 && r.p.starts() == 5
              && r.log == (Log{"BELL on", "BELL off", "BELL on", "BELL off", "BELL on", "BELL off", "BELL on", "BELL off", "BELL on", "BELL off"}));
    }
    {
        Rig r; r.ev(Ev::RING); r.run(96); r.ev(Ev::TAP_ACCEPT); r.ev(Ev::CONNECTED); r.run(2000);
        r.ev(Ev::ENDED, Why::HANGUP);                            // 상대가 끊음 → 종료음
        r.run(96);
        r.ev(Ev::RING);                                          // 종료음이 울리는 중에 다음 전화
        r.run(16);
        const Pcm a = r.run(500);
        CHECK("call ends and the next call comes during the end tone: bell, not end", r.p.current() == Tone::BELL && peak(a) > 0
              && r.log == (Log{"BELL on", "BELL off", "END on", "END off", "BELL on"}));
    }

    // ════ 기기 시험(계획 Task 5)의 기대 줄: 시나리오별로 한 기기가 찍는 TONE 줄 ════
    {
        Rig callee; callee.ev(Ev::RING); callee.run(1008); callee.ev(Ev::TAP_ACCEPT); callee.run(32); callee.ev(Ev::CONNECTED); callee.run(2000);
        callee.ev(Ev::ENDED, Why::HANGUP); callee.run(1008);
        Rig caller; caller.ev(Ev::TAP_CALL); caller.run(32); caller.ev(Ev::RINGING); caller.run(1008); caller.ev(Ev::CONNECTED); caller.run(2000);
        caller.ev(Ev::TAP_HANGUP); caller.run(48); caller.ev(Ev::ENDED, Why::HANGUP); caller.run(1008);
        CHECK("scenario call+accept+hangup: callee 'BELL on/off, END on/off', caller 'RINGBACK on/off, END on/off'",
              callee.log == (Log{"BELL on", "BELL off", "END on", "END off"}) && caller.log == (Log{"RINGBACK on", "RINGBACK off", "END on", "END off"})
              && callee.p.idle() && caller.p.idle());
    }
    {
        Rig callee; callee.ev(Ev::RING); callee.run(1008); callee.ev(Ev::TAP_REJECT, Why::NONE, 2); callee.run(48); callee.ev(Ev::ENDED, Why::REJECT, 2); callee.run(1008);
        Rig caller; caller.ev(Ev::TAP_CALL); caller.ev(Ev::RINGING); caller.run(1008); caller.ev(Ev::ENDED, Why::REJECT, 2); caller.run(1008);
        CHECK("scenario reject (plain or with a message): callee 'BELL on/off' only, caller 'RINGBACK on/off, END on/off'",
              callee.log == (Log{"BELL on", "BELL off"}) && caller.log == (Log{"RINGBACK on", "RINGBACK off", "END on", "END off"}));
    }
    {
        Rig caller; caller.ev(Ev::TAP_CALL); caller.ev(Ev::RINGING); caller.run(2000); caller.ev(Ev::TAP_CANCEL); caller.run(48); caller.ev(Ev::ENDED, Why::HANGUP); caller.run(1008);
        Rig callee; callee.ev(Ev::RING); callee.run(2000); callee.ev(Ev::ENDED, Why::HANGUP); callee.run(1008);
        CHECK("scenario cancel: caller 'RINGBACK on/off' (no end tone), callee 'BELL on/off'",
              caller.log == (Log{"RINGBACK on", "RINGBACK off"}) && callee.log == (Log{"BELL on", "BELL off"}) && callee.f.missed == 1);
    }
    {
        Rig caller; caller.ev(Ev::TAP_CALL); caller.ev(Ev::RINGING); caller.run(30000); caller.ev(Ev::ENDED, Why::TIMEOUT); caller.run(1008);
        Rig callee; callee.ev(Ev::RING); callee.run(30000); callee.ev(Ev::ENDED, Why::TIMEOUT); callee.run(1008);
        CHECK("scenario no answer (30 s): caller 'RINGBACK on/off, END on/off', callee 'BELL on/off'",
              caller.log == (Log{"RINGBACK on", "RINGBACK off", "END on", "END off"}) && callee.log == (Log{"BELL on", "BELL off"}));
    }
    {
        Rig callee; callee.ev(Ev::RING); callee.run(1008); callee.ev(Ev::TAP_MUTE); callee.run(5008); callee.ev(Ev::TAP_MUTE); callee.run(1008);
        callee.ev(Ev::TAP_ACCEPT); callee.run(32); callee.ev(Ev::CONNECTED); callee.run(2000); callee.ev(Ev::TAP_HANGUP); callee.run(48);
        callee.ev(Ev::ENDED, Why::HANGUP); callee.run(1008);
        CHECK("scenario mute, mute again, accept, hang up: 'BELL on, BELL off, END on, END off' (accept adds no line)",
              callee.log == (Log{"BELL on", "BELL off", "END on", "END off"}));
    }

    // ════ Phase 3b 에서 더한 것(3a 최종 검수 M-4): 벨·연결음 상태를 "떠나는 모든 길"에 멈춤이 온다 — 열거가 아니라 전수로 ════
    {
        const State states[] = { State::OFFLINE, State::IDLE, State::DIALING, State::RINGBACK, State::INCOMING, State::ANSWERING, State::IN_CALL, State::ENDING };
        const Ev evs[] = { Ev::LINK_UP, Ev::LINK_DOWN, Ev::PEER, Ev::RINGING, Ev::RING, Ev::CONNECTED, Ev::ENDED, Ev::TAP_CALL, Ev::TAP_CANCEL,
                           Ev::TAP_ACCEPT, Ev::TAP_REJECT, Ev::TAP_HANGUP, Ev::TAP_MUTE, Ev::TICK };
        const Why whys[] = { Why::NONE, Why::HANGUP, Why::REJECT, Why::TIMEOUT, Why::OFFLINE, Why::LOST };
        int left_bell = 0, left_ringback = 0, bell_no_stop = 0, ringback_no_stop = 0;
        for (State s : states) for (bool muted : {false, true}) for (Ev e : evs) for (Why w : whys) for (bool flag : {false, true})
            for (uint8_t msg : {(uint8_t)0, (uint8_t)2}) for (int64_t at : {(int64_t)1400, (int64_t)100000}) {
                if (muted && s != State::INCOMING) continue;
                Fsm f = in_state(s);
                if (muted) f.on(Ev::TAP_MUTE, 1300);
                const State before = f.state;
                const Out o = f.on(e, at, w, flag, msg);
                const bool stops = (o.acts & (A_TONE_STOP | A_TONE_END)) != 0;
                if (before == State::INCOMING && !muted && f.state != State::INCOMING) { ++left_bell; if (!stops) ++bell_no_stop; }
                if (before == State::RINGBACK && f.state != State::RINGBACK) { ++left_ringback; if (!stops) ++ringback_no_stop; }
            }
        CHECK("state machine: every way out of INCOMING (bell not muted) carries a stop or the end tone", left_bell > 20 && bell_no_stop == 0);
        CHECK("state machine: every way out of RINGBACK carries a stop or the end tone", left_ringback > 20 && ringback_no_stop == 0);
    }
    {
        // 무작위로 오래(시간도 흐른다): 할 일을 from_acts 로 따라간 "지금 내야 할 소리"가 상태와 늘 맞는다
        const Ev evs[] = { Ev::LINK_UP, Ev::LINK_DOWN, Ev::PEER, Ev::RINGING, Ev::RING, Ev::CONNECTED, Ev::ENDED, Ev::TAP_CALL, Ev::TAP_CANCEL,
                           Ev::TAP_ACCEPT, Ev::TAP_REJECT, Ev::TAP_HANGUP, Ev::TAP_MUTE, Ev::TICK };
        const Why whys[] = { Why::NONE, Why::HANGUP, Why::REJECT, Why::TIMEOUT, Why::OFFLINE, Why::LOST };
        uint32_t seed = 20261004; auto rnd = [&seed](uint32_t m) { seed = seed * 1664525u + 1013904223u; return (seed >> 8) % m; };
        Fsm f; Tone tone = Tone::NONE; int64_t now = 1000;
        long bell_outside = 0, ringback_outside = 0, bell_state_silent = 0, ringback_state_silent = 0, tone_in_call = 0, muted_rings = 0, rings = 0, dials = 0;
        for (int i = 0; i < 2000000; ++i) {
            now += rnd(8) == 0 ? rnd(9000) : rnd(40);
            const State before = f.state;
            const Out o = f.on(evs[rnd(14)], now, whys[rnd(6)], rnd(4) != 0, (uint8_t)rnd(5));
            from_acts(o.acts, &tone);
            if (before != State::INCOMING && f.state == State::INCOMING) ++rings;
            if (before != State::RINGBACK && f.state == State::RINGBACK) ++dials;
            if (tone == Tone::BELL && f.state != State::INCOMING) ++bell_outside;
            if (tone == Tone::RINGBACK && f.state != State::RINGBACK) ++ringback_outside;
            if (f.state == State::INCOMING && !f.bell_muted && tone != Tone::BELL) ++bell_state_silent;
            if (f.state == State::RINGBACK && tone != Tone::RINGBACK) ++ringback_state_silent;
            if (f.state == State::IN_CALL && tone != Tone::NONE) ++tone_in_call;
            if (f.state == State::INCOMING && f.bell_muted && tone == Tone::BELL) ++muted_rings;
        }
        CHECK("2 million random events: the bell never sounds outside INCOMING, the ringback never outside RINGBACK", rings > 1000 && dials > 1000 && bell_outside == 0 && ringback_outside == 0);
        CHECK("2 million random events: while ringing (not muted) the bell is on, while RINGBACK the ringback is on", bell_state_silent == 0 && ringback_state_silent == 0);
        CHECK("2 million random events: no tone during a call, and a muted bell never rings again", tone_in_call == 0 && muted_rings == 0);
    }

    printf("\n%d ok, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
