// tones.h — 신호음(벨·연결음·종료음)의 박자와 소리 만들기. 하드웨어·시계와 무관한 순수 로직(맥에서 단위 시험: tools/test_tones.cpp).
//
// 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md 의 결정 PL3a-D1(모양)·D2(크기)·D5(딸깍 방지)·D11(기록 줄)·D12(할 일 → 소리).
//   - 상태 기계(call_fsm.h)가 낸 할 일 비트(A_TONE_*)를 from_acts() 가 "지금 내야 할 소리" 하나로 바꾼다. 소리 쪽은 그것만 안다.
//   - Player 는 명령을 받아 16kHz 모노 PCM 을 만든다. 기기에서는 출력 태스크(tone_out.cpp 의 공급자)만 부른다 — 잠금이 없다.
//     공급자가 하는 일(명령 받기 → 한 묶음 만들기 → 찍을 기록 줄 고르기)도 여기(next_block)에 있다 — 기기 전용 코드는 우편함과 printf 뿐이다.
//   - 약속(수신거부 설계서 docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md §5.3): 멈춘(NONE) 뒤에는 새 명령이 올 때까지
//     스스로 다시 울리지 않는다. 이미 멈춘 뒤의 멈춤은 아무 일도 하지 않는다.
#pragma once
#include <cmath>
#include <cstdint>

#include "call_fsm.h"

namespace tones {

constexpr int RATE = 16000;                 // 표본율 — 통화 음성과 같다(PH-D5)
constexpr int PER_MS = RATE / 1000;         // 1ms = 16표본
constexpr int FADE = 5 * PER_MS;            // PL3a-D5: 울림 칸의 시작·끝과 멈춤·바꿈 때 5ms(80표본)에 걸쳐 크기를 0↔최대로 옮긴다(딸깍 소리 방지)

enum class Tone : uint8_t { NONE, BELL, RINGBACK, END };

inline const char *tone_name(Tone t)
{
    switch (t) {
    case Tone::NONE: return "NONE"; case Tone::BELL: return "BELL"; case Tone::RINGBACK: return "RINGBACK"; case Tone::END: return "END";
    }
    return "?";
}

struct Seg { uint16_t f1, f2, ms; };        // 한 칸: 주파수 둘(f2 = 0 이면 한 음), f1 = 0 이면 쉼. 길이(ms)
struct Pattern { const Seg *seg; uint8_t n; bool loop; int16_t amp; };   // amp = 최고 크기(16비트 표본). loop = 끝나면 처음부터

// PL3a-D1 신호음 모양. 바꿀 때는 이 표만 고친다(맥 시험의 박자 시험도 함께).
//   벨: 800·1000Hz 를 50ms 씩 번갈아 1초(전자식 전화벨의 떨림) → 2초 쉼, 반복. 3초 주기라 교환기의 30초 벨 동안 10번 울린다
//   연결음: 440+480Hz 를 함께 1초 → 2초 쉼, 반복(일반 전화 연결음의 모양)
//   종료음: 660Hz 160ms → 쉼 80ms → 440Hz 280ms, 한 번(0.52초). 내려가는 두 음 — "통화 중" 신호(뚜-뚜-)와 헷갈리지 않게
inline constexpr Seg BELL_SEGS[] = {
    {800, 0, 50}, {1000, 0, 50}, {800, 0, 50}, {1000, 0, 50}, {800, 0, 50}, {1000, 0, 50}, {800, 0, 50}, {1000, 0, 50},
    {800, 0, 50}, {1000, 0, 50}, {800, 0, 50}, {1000, 0, 50}, {800, 0, 50}, {1000, 0, 50}, {800, 0, 50}, {1000, 0, 50},
    {800, 0, 50}, {1000, 0, 50}, {800, 0, 50}, {1000, 0, 50}, {0, 0, 2000},
};
inline constexpr Seg RINGBACK_SEGS[] = { {440, 480, 1000}, {0, 0, 2000} };
inline constexpr Seg END_SEGS[] = { {660, 0, 160}, {0, 0, 80}, {440, 0, 280} };

// PL3a-D2 크기 — 코덱 음량 100(0dB, audio_out.cpp)에서의 최고 크기.
//   벨 8000: Phase 0 에서 1kHz·진폭 8000 을 음량 100 으로 낸 "삐"를 사용자가 "적당"이라 했다(tab5_phone_probe/out/task4.log 61~66) — 그 크기 그대로.
//   연결음 5300·종료음 3700: Phase 0 시험 음성(말소리 구간 RMS −16.9dBFS)을 음량 90(−5dB)으로 낸 크기(≈ −21.9dB)와 RMS 를 맞췄다 —
//   귀 가까이에서 듣는 소리라서. 두 음을 함께 내는 연결음의 RMS = 최고 크기/2, 한 음인 종료음은 최고 크기/√2.
inline constexpr int16_t BELL_AMP = 8000, RINGBACK_AMP = 5300, END_AMP = 3700;

inline const Pattern &pattern(Tone t)
{
    static constexpr Pattern NONE_P{nullptr, 0, false, 0};
    static constexpr Pattern BELL_P{BELL_SEGS, sizeof BELL_SEGS / sizeof BELL_SEGS[0], true, BELL_AMP};
    static constexpr Pattern RINGBACK_P{RINGBACK_SEGS, sizeof RINGBACK_SEGS / sizeof RINGBACK_SEGS[0], true, RINGBACK_AMP};
    static constexpr Pattern END_P{END_SEGS, sizeof END_SEGS / sizeof END_SEGS[0], false, END_AMP};
    switch (t) {
    case Tone::BELL: return BELL_P;
    case Tone::RINGBACK: return RINGBACK_P;
    case Tone::END: return END_P;
    default: return NONE_P;
    }
}

inline uint32_t seg_len(const Seg &s) { return (uint32_t)s.ms * PER_MS; }                // 칸 길이(표본)
inline uint32_t cycle_len(const Pattern &p)                                              // 한 바퀴 길이(표본)
{
    uint32_t n = 0;
    for (int i = 0; i < p.n; ++i) n += seg_len(p.seg[i]);
    return n;
}

// 박자 계산: 소리를 시작한 지 pos 표본 뒤에 어느 칸의 몇째 표본인가. 한 번만 울리는 소리가 끝났거나 빈 패턴이면 seg = -1.
struct Where { int seg; uint32_t off, len; };
inline Where locate(const Pattern &p, uint64_t pos)
{
    const uint32_t total = cycle_len(p);
    if (total == 0 || (!p.loop && pos >= total)) return Where{-1, 0, 0};
    uint32_t r = (uint32_t)(pos % total);
    for (int i = 0; i < p.n; ++i) {
        const uint32_t len = seg_len(p.seg[i]);
        if (r < len) return Where{i, r, len};
        r -= len;
    }
    return Where{-1, 0, 0};
}

// 사인 표(257칸, 끝 = 처음). 위상(32비트)의 위 8비트로 칸을, 다음 8비트로 칸 사이를 잇는다(직선 보간 — 오차는 몇 LSB).
struct SineTable {
    int16_t v[257];
    SineTable() { for (int i = 0; i <= 256; ++i) v[i] = (int16_t)std::lround(32767.0 * std::sin(i * (2.0 * 3.14159265358979323846 / 256.0))); }
};
inline const SineTable &sine_table() { static const SineTable t; return t; }   // 처음 부를 때 만든다(기기에서는 tone_out::start 가 미리 부른다)
inline int32_t sine_q15(uint32_t phase)
{
    const SineTable &t = sine_table();
    const uint32_t i = phase >> 24, frac = (phase >> 16) & 0xFF;
    return t.v[i] + ((int32_t)t.v[i + 1] - t.v[i]) * (int32_t)frac / 256;
}
inline uint32_t phase_step(uint16_t hz) { return (uint32_t)(((uint64_t)hz << 32) / RATE); }   // 표본 하나에 나아가는 위상

// 명령을 받아 PCM 을 만든다. 한 곳(출력 태스크)에서만 쓴다.
class Player {
public:
    // 새 소리(BELL·RINGBACK·END) 또는 멈춤(NONE). 울리는 중이면 FADE 표본 안에 줄인 뒤 바꾼다(그 사이에 또 오면 마지막 것으로).
    // 조용할 때(아무것도 안 내거나 박자의 쉼)는 바로 바꾼다. 같은 소리가 와도 처음부터 다시 시작한다.
    void command(Tone t)
    {
        if (cur_ == Tone::NONE || gain_ == 0) begin(t);
        else { next_ = t; releasing_ = true; }
    }
    void render(int16_t *out, int n) { for (int i = 0; i < n; ++i) out[i] = step(); }
    Tone current() const { return cur_; }          // 내고 있는 소리(박자의 쉼 동안에도 그 소리). 완전히 조용해지면 NONE
    bool idle() const { return cur_ == Tone::NONE; }
    uint32_t starts() const { return starts_; }    // 소리를 새로 시작한 횟수 — 같은 소리를 다시 시작한 것도 기록 줄로 알리려고(transitions)
    uint64_t pos() const { return pos_; }          // 지금 소리를 시작한 뒤 지난 표본 수

private:
    Tone cur_ = Tone::NONE, next_ = Tone::NONE;
    bool releasing_ = false;
    int gain_ = 0;                                 // 0..FADE
    uint64_t pos_ = 0;
    uint32_t ph1_ = 0, ph2_ = 0, starts_ = 0;

    void begin(Tone t)
    {
        cur_ = pattern(t).n ? t : Tone::NONE; next_ = Tone::NONE; releasing_ = false;
        gain_ = 0; pos_ = 0; ph1_ = ph2_ = 0;
        if (cur_ != Tone::NONE) ++starts_;
    }

    int16_t step()
    {
        if (cur_ == Tone::NONE) return 0;
        const Pattern &p = pattern(cur_);
        const Where w = locate(p, pos_);
        if (w.seg < 0) { begin(Tone::NONE); return 0; }                    // 한 번만 울리는 소리(종료음)가 끝났다
        const Seg &s = p.seg[w.seg];
        const bool up = !releasing_ && s.f1 != 0 && w.off + FADE < w.len;  // 칸의 마지막 FADE 표본은 줄인다 → 칸 끝에서 크기 0
        if (up) { if (gain_ < FADE) ++gain_; }
        else if (gain_ > 0) { --gain_; }
        int32_t v = 0;
        if (gain_ > 0) {
            const int32_t wave = s.f2 ? (sine_q15(ph1_) + sine_q15(ph2_)) / 2 : sine_q15(ph1_);
            v = wave * p.amp / 32767 * gain_ / FADE;
        }
        ph1_ += phase_step(s.f1); ph2_ += phase_step(s.f2);
        if (w.off + 1 == w.len) ph1_ = ph2_ = 0;                            // 다음 칸은 위상 0 에서(칸 끝의 크기가 0 이라 끊김 없음)
        ++pos_;
        if (releasing_ && gain_ == 0) begin(next_);                         // 다 줄였다 → 다음 소리(멈춤이면 조용)
        return (int16_t)v;
    }
};

// 기록 줄(TONE <이름> on|off)에 무엇을 찍을지: 출력 한 묶음 전(before, 시작 횟수 sb)과 뒤(after, sa)를 비교한다.
// 바뀌었거나(다른 소리·멈춤) 같은 소리를 다시 시작했으면 off = 앞의 소리, on = 새 소리. 찍을 것이 없으면 NONE.
inline void transitions(Tone before, uint32_t sb, Tone after, uint32_t sa, Tone *off, Tone *on)
{
    const bool changed = before != after || sb != sa;
    *off = changed ? before : Tone::NONE;
    *on = changed ? after : Tone::NONE;
}

// 출력 한 묶음(tone_out.cpp 의 공급자가 16ms 마다 부른다): 우편함에 명령이 있었으면(has_cmd) 먼저 넣고 n 표본을 만든다.
//   sound = out 에 소리를 썼나(false 면 낼 것이 없다 — out 은 건드리지 않는다. 줄어들어 멈춘 묶음은 그 줄어드는 소리가 있으므로 true)
//   off·on = 이번 묶음에서 찍을 기록 줄: "TONE <off> off" 다음 "TONE <on> on"(없으면 NONE)
struct Block { bool sound; Tone off, on; };
inline Block next_block(Player &p, bool has_cmd, Tone cmd, int16_t *out, int n)
{
    const Tone before = p.current();
    const uint32_t sb = p.starts();
    if (has_cmd) p.command(cmd);
    Block b{!p.idle(), Tone::NONE, Tone::NONE};
    if (b.sound) p.render(out, n);
    transitions(before, sb, p.current(), p.starts(), &b.off, &b.on);
    return b;
}

// 상태 기계의 할 일 → 지금 내야 할 소리. 소리에 관한 비트가 없으면 false(지금 소리를 그대로 둔다).
// PL3a-D12: 시작(종료음 > 벨 > 연결음)이 멈춤보다 앞선다. 지금 상태 기계는 소리 비트를 둘 이상 함께 내지 않는다(시험으로 고정).
inline bool from_acts(uint16_t acts, Tone *out)
{
    if (acts & callfsm::A_TONE_END) *out = Tone::END;
    else if (acts & callfsm::A_TONE_BELL) *out = Tone::BELL;
    else if (acts & callfsm::A_TONE_RINGBACK) *out = Tone::RINGBACK;
    else if (acts & callfsm::A_TONE_STOP) *out = Tone::NONE;
    else return false;
    return true;
}

}  // namespace tones
