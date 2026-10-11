// voice_core.h — 목소리 통화의 판단 로직. 하드웨어·시계·OS 와 무관한 순수 코드(맥에서 단위 시험: tools/test_voice_core.cpp).
//
// 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md 의 결정 PL3b-D2·D5~D13. 기기 전용 코드(voice.cpp)는 여기 것을 태스크에 잇기만 한다.
//   보내는 길: 마이크 4칸 → split_slots → (에코 제거 — 기기) → pick(무엇을 보낼지) → Uplink(16ms 묶음 → 20ms 프레임, 순번)
//             → (Opus 압축 — 기기) → relayproto::pack_voice → TxFifo(밀리면 오래된 것부터 버린다) → 교환기
//   받는 길:  교환기 → Receiver::push(지터 버퍼) → Receiver::pump(프레임 하나씩 꺼내 해제 — 빠진 것은 Concealer 가 메운다 — 링버퍼에)
//             → Receiver::fill(출력 태스크가 16ms 묶음으로 가져간다. audio_out 의 둘째 공급자)
//   켜고 끄기: 상태 기계의 A_VOICE_START·A_VOICE_STOP → from_acts. 받기 전에는 켜지지 않는다(PH-D6 — 시험이 모든 상태 × 사건으로 고정).
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "call_fsm.h"
#include "jitter_buffer.h"
#include "pcm.h"
#include "relay_proto.h"
#include "tones.h"

namespace voicecore {

constexpr int RATE = 16000;                 // PH-D5
constexpr int BLOCK = 256;                  // 마이크·에코 제거·스피커 출력의 한 묶음(16ms) — audio_out 의 BLOCK, esp-sr VOIP 방식의 묶음과 같다
constexpr int FRAME = 320;                  // Opus 한 프레임(20ms)
constexpr int FADE = 5 * RATE / 1000;       // 5ms(80표본): 목소리가 시작·끝·이어 붙을 때 크기를 옮기는 길이(딸깍 방지 — tones.h 의 FADE 와 같은 값)
constexpr int IN_SLOTS = 4, MIC_SLOT = 0, REF_SLOT = 1;   // 마이크 칩(ES7210)의 칸: 0 = 마이크, 1 = 스피커 되먹임(에코 기준). Phase 0 실측

// PL3b-D9 통화가 열린 뒤 마이크의 앞 8묶음(128ms)은 보내지 않는다: 그 안에는 받기 전에 DMA 에 쌓여 있던 소리(3칸 = 48ms)와
//   아직 스피커에서 나고 있는 연결음 꼬리(멈춤 5ms + 앞서 쓴 32ms)가 들어 있다. 참고 구현(xiaozhi)은 120ms 를 버린다.
constexpr int DISCARD_BLOCKS = 8;
// PL3b-D10 크기: 마이크 증폭은 24dB(audio_out.cpp)로 낮추고 에코 제거 뒤에 디지털로 4배(+12dB) 한다. 재생은 그대로(1배).
//   Phase 0: 음량 100·마이크 24dB 에서 에코 포화 0%·잔향 감소 41.8dB / 30dB 에서는 꼭대기 포화 0.05%. 사용자 말소리는 30dB 에서 −28dBFS 안팎
//   → 24dB + 4배면 −22dBFS = 사용자가 "적당"이라 한 재생 크기(시험 음성 −16.9dBFS 를 음량 90 으로).
// PL3b-D23 (fix 2) 보내는 쪽 디지털 이득은 이제 실행 중에 1~4배로 바꿀 수 있다(시리얼 `txgain <1..4>`, TxGain). 기본은 위의 4배 그대로다.
//   이유: 귀로 듣고 "크다/메아리가 난다"는 답이 나왔을 때 다시 빌드·플래시하지 않고 그 자리에서 낮춰 보려는 것(최종 검수 M-3 / 판정 P3B-R12).
//   비용: 저장하지 않는다(재부팅하면 기본 4배) · 읽는 쪽(캡처 태스크)과 쓰는 쪽(시리얼 태스크)이 달라 atomic. 되돌리기: 기본값이 옛 상수와 같으므로
//   명령을 안 쓰면 옛 동작 그대로다. TX_GAIN_Q8 은 "기본값"의 Q8 표현으로 남겨 둔다(기존 시험·pick 의 기본 인자).
constexpr int TX_GAIN_MIN = 1, TX_GAIN_MAX = 4, TX_GAIN_DEFAULT = 4;
constexpr int TX_GAIN_Q8 = TX_GAIN_DEFAULT * 256;
constexpr int PLAY_GAIN_Q8 = 256;
// P5A-D3 받는 목소리를 키울 때의 꼭대기: PLAY_KNEE 위는 부드럽게 눌러 PLAY_CEIL 을 넘지 않는다.
//   천장 26000(−2dBFS)의 근거(2026-10-08 조사 — Tab5 회로도·ES8388 자료): 코덱 풀스케일 1Vrms × 앰프 이득 약 3.5배 = 3.5Vrms 는 5V 전원과 1W 스피커를 넘는다
//   (1W 는 약 −1.9dBFS). 넘친 소리의 찌그러짐은 되먹임 칸(코덱 출력에서 갈라진다) 뒤에서 생겨 에코 제거가 지우지 못한다 → 메아리로 상대에게 간다.
//   앰프 이득 식은 2차 출처이고 기기에서 재지는 않았다. 탈출구: 이 두 값.
constexpr int PLAY_KNEE = 16000, PLAY_CEIL = 26000;
// PL3b-D8 보내기 대기열: 10프레임(200ms — 설계서 §9.6). 가득 차면 오래된 것부터 버리고, 200ms 넘게 묵은 것은 보내지 않는다.
constexpr int TX_QUEUE = 10;
constexpr uint32_t TX_STALE_MS = 200;
constexpr size_t MAX_TX_FRAME = 200;        // [순번 2][Opus] 한 프레임의 상한(24kbps·20ms = 60바이트, 압축기의 권장 출력 버퍼 160바이트)
constexpr int RING_CAP = 1024;              // 해제한 PCM 링버퍼(64ms). 평소에는 256~575 표본이 들어 있다
// PL3b-D11·D12 시험음과 지연 재기
constexpr int TEST_HZ = 1000;
constexpr int TEST_AMP = 2000;              // 시험음(vsrc tone, selftest)의 크기 — 벨(8000)보다 12dB 작다
constexpr int PING_AMP = 4000;              // 지연 재기의 삑
constexpr int PING_LEAD = 400 * RATE / 1000, PING_BURST = 100 * RATE / 1000, PING_TAIL = 500 * RATE / 1000;   // 쉼 0.4초 → 삑 0.1초 → 쉼 0.5초
constexpr int PING_DETECT = 1500;           // 돌아온 소리에서 이 크기를 넘는 첫 표본을 삑의 시작으로 본다
// PL3b-D14 혼자 하는 점검(selftest): 시험음을 0.64초 내며 마이크 칩의 두 칸을 잰다. 앞 8묶음(묵은 DMA)은 버리고 24묶음(0.38초)을 잰다.
//   되먹임 칸(ref)은 앰프를 꺼도 잡힌다(Phase 0: 1kHz·진폭 8000·음량 100 에서 2370 → 진폭 2000 이면 약 590). 300 이상이면 "코덱이 소리를 내고 있다".
constexpr int SELFTEST_TONE = 640 * RATE / 1000;
constexpr int SELFTEST_SKIP_BLOCKS = DISCARD_BLOCKS, SELFTEST_BLOCKS = 24;
// 자가 점검의 되먹임 칸 판정선(SELFTEST_REF_MIN)은 기판마다 달라 board_profile.h 에 있다(2026-10-09).

// ── 켜고 끄기 ────────────────────────────────────────────────────────────────
// 상태 기계의 할 일 → 목소리 경로를 켤지 끌지. 목소리에 관한 비트가 없으면 false(지금 상태 그대로).
// 끄기가 켜기보다 앞선다 — 둘이 함께 오면(지금 상태 기계는 그러지 않는다. 시험으로 고정) 마이크를 닫는 쪽을 고른다(PH-D6).
inline bool from_acts(uint16_t acts, bool *on)
{
    if (acts & callfsm::A_VOICE_STOP) *on = false;
    else if (acts & callfsm::A_VOICE_START) *on = true;
    else return false;
    return true;
}

// ── 보내는 길 ────────────────────────────────────────────────────────────────
// 마이크 칩에서 읽은 4칸 묶음(칸0,칸1,칸2,칸3,칸0,…)에서 마이크와 에코 기준을 떼어 낸다.
inline void split_slots(const int16_t *slots, int frames, int16_t *mic, int16_t *ref)
{
    for (int i = 0; i < frames; ++i) { mic[i] = slots[i * IN_SLOTS + MIC_SLOT]; ref[i] = slots[i * IN_SLOTS + REF_SLOT]; }
}

// 시험음(위상을 이어 간다). 기본 1kHz. PL3c-D4: 주파수를 바꿀 수 있다(set_hz — 시리얼 `vtone <Hz>`) → 여러 주파수로 보내 "어느 대역이 어디서 깎이는가"를 사람 없이 잰다.
class TestTone {
public:
    void reset() { ph_ = 0; }
    void set_hz(int hz) { step_ = tones::phase_step((uint16_t)hz); }       // 위상은 그대로 이어 간다(바꾸는 자리에 계단이 없다)
    void render(int16_t *out, int n, int amp)
    {
        for (int i = 0; i < n; ++i) { out[i] = (int16_t)(tones::sine_q15(ph_) * amp / 32767); ph_ += step_; }
    }

private:
    uint32_t ph_ = 0, step_ = tones::phase_step(TEST_HZ);
};

// PL3b-D11 무엇을 보낼까. AEC = 평소(에코 제거 결과). 나머지는 사람이 시리얼로 켜는 진단용이고 통화가 끝나면 AEC 로 돌아간다.
//   MIC = 에코 제거를 거치지 않은 마이크, REF = 스피커 되먹임 칸(이 기기가 내고 있는 소리 — 앰프를 꺼도 잡힌다), TONE = 1kHz 시험음.
//   PL3c-D4 PROBE = 마이크 자리에 시험음(PROBE_AMP)을 넣어 에코 제거를 "거치게" 한 것. 보내는 것은 에코 제거 결과(평소와 같은 길)다 →
//     STAT 의 v_mic(넣은 크기)와 v_aec(나온 크기)의 비가 "에코 제거가 그 주파수의 가까운 소리를 얼마나 깎는가"다. 진짜 마이크 소리는 쓰이지 않는다(고리가 닫히지 않는다).
enum class Src : uint8_t { AEC, MIC, REF, TONE, PROBE };
constexpr int PROBE_AMP = 500;              // 말소리가 마이크에 잡히는 크기 근처(2026-10-05 실측 v_mic 204~554). 4배로 보내면 상대에게는 시험음(2000)과 같은 크기다
inline const char *src_name(Src s)
{
    switch (s) { case Src::AEC: return "aec"; case Src::MIC: return "mic"; case Src::REF: return "ref"; case Src::TONE: return "tone"; case Src::PROBE: return "probe"; }
    return "?";
}
inline bool parse_src(const char *text, Src *out)
{
    static const Src ALL[] = {Src::AEC, Src::MIC, Src::REF, Src::TONE, Src::PROBE};
    for (Src s : ALL) if (text && !strcmp(text, src_name(s))) { *out = s; return true; }
    return false;
}
// PL3b-D22 (fix 2) 진단 출처 mic·ref 는 스피커 앰프가 꺼져 있을 때만 쓴다. 이유: 양쪽이 ref(또는 mic)면 메아리 제거를 거치지 않는 고리가 닫혀
//   (ref 는 공기도 거치지 않는다) 한 길마다 이득이 +4.5dB 이상이라 2초 안에 최대 크기가 된다(최종 검수 M-2). aec·tone 은 늘 허용.
//   비용: 스피커를 켠 채 마이크 원음·되먹임 칸을 보내 보는 진단은 못 한다(조용한 모드에서 하면 된다). 되돌리기: src_allowed 를 늘 true 로.
inline bool src_allowed(Src s, bool amp_on) { return !amp_on || s == Src::AEC || s == Src::TONE || s == Src::PROBE; }   // PROBE 는 마이크를 쓰지 않아 고리가 닫히지 않는다 → 늘 허용
// 통화 도중 앰프가 켜졌으면(quiet off) 이미 골라 둔 mic·ref 는 aec 로 되돌린다. 캡처 태스크가 묶음마다 이 값을 쓴다(경합이 있어도 한 묶음 안에 닫힌다).
inline Src effective_src(Src s, bool amp_on) { return src_allowed(s, amp_on) ? s : Src::AEC; }
// PL3c-D7 보내는 크기 다듬기(tx_dyn.h — 평탄화·남은 메아리 누르기)는 "에코 제거를 거친 소리"에만 건다(aec·probe).
//   진단 출처 mic·ref 는 3b 의 고정 이득 그대로다 → 지연 재기(vping — 상대가 ref 를 돌려보낸다)의 삑 크기가 손잡이에 따라 달라지지 않는다. tone 은 원래 이득이 없다.
inline bool uses_dyn(Src s) { return s == Src::AEC || s == Src::PROBE; }

// 시리얼 명령의 숫자 인자 읽기: 숫자(0~9)만 1~4글자, lo..hi 안. 부호·공백·소수점·글자·빈 글자·범위 밖은 거절하고 *out 을 건드리지 않는다.
inline bool parse_uint(const char *text, int lo, int hi, int *out)
{
    if (!text || !*text) return false;
    int v = 0, digits = 0;
    for (const char *p = text; *p; ++p, ++digits) {
        if (*p < '0' || *p > '9' || digits >= 4) return false;
        v = v * 10 + (*p - '0');
    }
    if (v < lo || v > hi) return false;
    *out = v;
    return true;
}

// 보내는 쪽 이득(1~4배). 쓰는 쪽 = 시리얼 태스크, 읽는 쪽 = 캡처 태스크.
class TxGain {
public:
    bool set(int n)
    {
        if (n < TX_GAIN_MIN || n > TX_GAIN_MAX) return false;
        n_.store(n);
        return true;
    }
    int get() const { return n_.load(); }
    int q8() const { return n_.load() * 256; }

private:
    std::atomic<int> n_{TX_GAIN_DEFAULT};
};

// 보낼 한 묶음(BLOCK 표본)을 out 에 만든다. 마이크에서 온 소리에는 gain_q8 을 곱하고(넘치면 +32767/−32768 에서 자른다 — 돌지 않는다), 시험음은 정한 크기 그대로.
// gain_q8 의 기본은 TX_GAIN_Q8(4배) — 기기는 TxGain::q8() 을 넘긴다.
inline void pick(Src src, const int16_t *clean, const int16_t *mic, const int16_t *ref, TestTone &tone, int16_t *out, int gain_q8 = TX_GAIN_Q8)
{
    if (src == Src::TONE) { tone.render(out, BLOCK, TEST_AMP); return; }
    memcpy(out, src == Src::MIC ? mic : src == Src::REF ? ref : clean, BLOCK * sizeof(int16_t));
    pcm::gain_q8(out, BLOCK, gain_q8);
}

// PL3b-D27 (fix 2) 캡처 태스크의 "방금 켜졌다" 판단. 통화가 열릴 때마다 오르는 세대 번호(app 태스크의 set(true))를 함께 본다:
//   모드만 보면 한 묶음(16ms) 안의 켜기→끄기→켜기가 같은 모드로 보여 start()(순번 0·앞 128ms 버림)가 돌지 않았다(최종 검수 M-8).
//   수신 쪽의 epoch 와 같은 모양(바뀌었는지만 본다 — 몇 칸 올랐는지는 상관없다). 비용: 없음(정수 하나). 되돌리기: mode_changed 만 쓰면 옛 동작.
class GenWatch {
public:
    bool fresh(uint32_t gen, bool mode_changed)
    {
        const bool f = mode_changed || gen != seen_;
        seen_ = gen;
        return f;
    }
    // Phase 3c(fix 2 재검수 Minor 6): 마이크 한 묶음을 읽는 16ms 사이에 세대가 올랐나 — 올랐다면 방금 읽은 묶음은 앞 통화(또는 앞 점검)의 것이다.
    //   부르는 쪽은 그 묶음을 버리고 처음으로 돌아간다(다음 바퀴의 fresh 가 새 통화로 다시 시작한다). seen_ 은 건드리지 않는다.
    bool stale(uint32_t gen) const { return gen != seen_; }
    uint32_t seen() const { return seen_; }   // 지금 통화의 세대(마지막 fresh 가 본 값). 스택짱의 캡처 태스크가 압축 태스크로 넘기는 묶음에 싣는다(voice.cpp VB-D1)

private:
    uint32_t seen_ = 0;
};

// 16ms 묶음을 20ms 프레임으로 다시 자르고 순번을 매긴다. 통화마다 start() 로 비운다(순번 0 부터, 앞 DISCARD_BLOCKS 묶음은 버린다 — PL3b-D9).
class Uplink {
public:
    void start() { len_ = 0; seq_ = 0; skip_ = DISCARD_BLOCKS; discarded_ = 0; }
    void push(const int16_t *block)                 // BLOCK 표본
    {
        if (skip_ > 0) { --skip_; ++discarded_; return; }
        if (len_ + BLOCK > CAP) { memmove(buf_, buf_ + BLOCK, (size_t)(len_ - BLOCK) * sizeof(int16_t)); len_ -= BLOCK; }   // pop 을 안 부르면 오래된 것부터 버린다
        memcpy(buf_ + len_, block, BLOCK * sizeof(int16_t));
        len_ += BLOCK;
    }
    bool pop(int16_t *frame, uint16_t *seq)         // 한 프레임(FRAME 표본)이 찼으면 꺼낸다
    {
        if (len_ < FRAME) return false;
        memcpy(frame, buf_, FRAME * sizeof(int16_t));
        len_ -= FRAME;
        memmove(buf_, buf_ + FRAME, (size_t)len_ * sizeof(int16_t));
        *seq = seq_++;
        return true;
    }
    uint32_t discarded() const { return discarded_; }
    int pending() const { return len_; }

private:
    static constexpr int CAP = 4 * BLOCK;
    int16_t buf_[CAP];
    int len_ = 0, skip_ = 0;
    uint16_t seq_ = 0;
    uint32_t discarded_ = 0;
};

// PL3b-D8 보내기 대기열. 넣는 쪽(캡처 태스크)과 꺼내는 쪽(송신 태스크)이 다르다 — Lock 으로 감싼다(기기: FreeRTOS 뮤텍스, 시험: 빈 잠금).
//   음성은 밀리면 버린다(쌓지 않는다): 가득 차면 가장 오래된 것을 버리고 새것을 넣는다. 꺼낼 때 TX_STALE_MS 넘게 묵은 것은 건너뛴다.
struct TxFrame { uint32_t ms; uint16_t len; uint8_t data[MAX_TX_FRAME]; };
inline bool stale(uint32_t now_ms, uint32_t stamp_ms) { return (uint32_t)(now_ms - stamp_ms) > TX_STALE_MS; }   // 시계가 한 바퀴 돌아도 맞는다
template <class Lock>
class TxFifo {
public:
    explicit TxFifo(Lock &lock) : lock_(lock) {}
    bool put(const uint8_t *frame, size_t len, uint32_t now_ms)     // 규격 밖(빈 것·너무 큰 것)은 넣지 않고 false
    {
        if (!frame || len == 0 || len > MAX_TX_FRAME) return false;
        lock_.lock();
        if (count_ == TX_QUEUE) { head_ = (head_ + 1) % TX_QUEUE; --count_; ++dropped_; }
        TxFrame &f = q_[(head_ + count_) % TX_QUEUE];
        f.ms = now_ms; f.len = (uint16_t)len; memcpy(f.data, frame, len);
        ++count_;
        lock_.unlock();
        return true;
    }
    bool take(TxFrame *out, uint32_t now_ms)
    {
        bool got = false;
        lock_.lock();
        while (count_ > 0 && !got) {
            const TxFrame &f = q_[head_];
            if (stale(now_ms, f.ms)) ++dropped_;
            else { *out = f; got = true; }
            head_ = (head_ + 1) % TX_QUEUE; --count_;
        }
        lock_.unlock();
        return got;
    }
    int flush()                                     // 전부 버린다(통화 시작·끝). 돌려주는 값 = 버린 수(dropped 에는 세지 않는다)
    {
        lock_.lock();
        const int n = count_;
        count_ = 0; head_ = 0;
        lock_.unlock();
        return n;
    }
    int size() const { return count_; }
    uint32_t dropped() const { return dropped_; }   // 밀려서 버린 프레임(가득 참 + 묵음). 통화마다 clear_dropped
    void clear_dropped() { lock_.lock(); dropped_ = 0; lock_.unlock(); }

private:
    Lock &lock_;
    TxFrame q_[TX_QUEUE];
    int head_ = 0, count_ = 0;
    uint32_t dropped_ = 0;
};

// PL3c-D4 마이크 묶음에서 꼭대기에 닿은(넘친) 표본 수. 스피커 소리가 마이크에서 넘치면 그 메아리는 에코 제거가 지우지 못한다 — 1번 기기는 제 스피커를 2.3~2.7배로 줍는다
//   (2026-10-05: 상대 소리 v_spk=5904 일 때 v_mic=19777). 통화마다 세어 TUNE 줄의 mclip= 으로 낸다 → 마이크 증폭(mic 손잡이)을 내릴지의 근거.
inline uint32_t clipped(const int16_t *s, int n)
{
    uint32_t c = 0;
    for (int i = 0; i < n; ++i) if (s[i] >= 32767 || s[i] <= -32768) ++c;
    return c;
}

// PL3c-D4 받은 프레임 사이의 가장 긴 틈(ms) — "망이 얼마나 오래 멈췄다 풀렸나"를 기기에서 바로 본다(TUNE 줄의 rxgap=). 통화마다 reset.
//   쓰는 쪽 = 웹소켓 태스크(프레임이 올 때마다 hit), 읽는 쪽 = 시리얼 태스크. 시계(ms)가 한 바퀴 돌아도 맞는다.
class ArrivalGap {
public:
    void reset() { have_ = false; max_.store(0); }
    void hit(uint32_t now_ms)
    {
        if (have_.load()) { const uint32_t gap = now_ms - last_.load(); if (gap > max_.load()) max_.store(gap); }
        last_.store(now_ms); have_.store(true);
    }
    uint32_t max_ms() const { return max_.load(); }

private:
    std::atomic<uint32_t> last_{0}, max_{0};
    std::atomic<bool> have_{false};
};

// ── 받는 길 ──────────────────────────────────────────────────────────────────
// PL3b-D7 빠진 프레임 메우기. esp_audio_codec 2.5.0 의 Opus 해제기는 손실 은닉을 해 주지 않는다(계획서의 근거) → 직전 프레임을 줄여 가며 되풀이한다.
//   lost    직전 소리를 거울처럼 되짚으며(끝 표본에서 거꾸로 — 이어 붙는 자리에 계단이 없다) MAX_CONCEAL 프레임(60ms)에 걸쳐 0 까지 줄인다
//   silence 채우는 중(무음). 직전에 소리가 남아 있었으면 5ms 에 걸쳐 0 으로(그때만 true)
//   good    정상 프레임. 직전 정상 프레임의 바로 다음 순번이 아니면(은닉·무음·건너뜀 뒤) 앞 5ms 를 직전 소리에서 넘겨 받는다(딸깍 방지)
class Concealer {
public:
    void reset() { have_ = false; good_ = false; left_ = 0; }
    void good(int16_t *frame, uint16_t seq)
    {
        if (!(good_ && (uint16_t)(seq - seq_) == 1)) {
            const int32_t g = level();
            for (int i = 0; i < FADE; ++i) {
                const int32_t tail = have_ && g ? walk() * g / Q : 0;
                frame[i] = (int16_t)((tail * (FADE - i) + (int32_t)frame[i] * i) / FADE);
            }
        }
        memcpy(last_, frame, sizeof last_);
        have_ = true; good_ = true; seq_ = seq; left_ = jitterbuf::MAX_CONCEAL; pos_ = FRAME - 2; dir_ = -1;
    }
    void lost(int16_t *out)
    {
        const int32_t g0 = level();
        if (left_ > 0) --left_;
        const int32_t g1 = level();
        for (int i = 0; i < FRAME; ++i) {
            const int32_t g = g0 + (g1 - g0) * (i + 1) / FRAME;
            out[i] = have_ && g0 ? (int16_t)(walk() * g / Q) : 0;
        }
        good_ = false;
    }
    bool silence(int16_t *out)                      // 돌려주는 값 = 줄어드는 꼬리를 썼나(false 면 out 은 전부 0)
    {
        const int32_t g = level();
        const bool tail = have_ && g;
        memset(out, 0, FRAME * sizeof(int16_t));
        if (tail) for (int i = 0; i < FADE; ++i) out[i] = (int16_t)(walk() * (g * (FADE - 1 - i) / FADE) / Q);
        left_ = 0; good_ = false;
        return tail;
    }

private:
    static constexpr int32_t Q = 32768;
    int32_t level() const { return Q * left_ / jitterbuf::MAX_CONCEAL; }
    int32_t walk()                                  // 직전 프레임을 끝에서 거꾸로, 처음에 닿으면 다시 앞으로
    {
        const int32_t v = last_[pos_];
        pos_ += dir_;
        if (pos_ < 0) { pos_ = 1; dir_ = 1; }
        else if (pos_ >= FRAME) { pos_ = FRAME - 2; dir_ = -1; }
        return v;
    }
    int16_t last_[FRAME];
    bool have_ = false, good_ = false;
    uint16_t seq_ = 0;
    int left_ = 0, pos_ = 0, dir_ = -1;
};

// 받는 쪽 통계 — 통화 하나(start 부터). STAT 에 싣는다.
struct RxStats {
    uint32_t frames = 0;        // 교환기에서 받은 프레임
    uint32_t bad = 0;           // 규격 밖(짧거나 긴 것)
    uint32_t played = 0;        // 해제해서 낸 프레임
    uint32_t concealed = 0;     // 메운 프레임(빠졌거나 해제 실패)
    uint32_t dec_fail = 0;      // 해제 실패
    uint32_t under = 0;         // 출력 태스크가 가져가려는데 링버퍼가 비어 있던 횟수(해제 태스크가 굶었다)
    uint32_t play_rms = 0;      // 최근 1초 동안 낸 목소리의 세기
    int depth = 0, target = 0;  // 지터 버퍼에 든 프레임 / 목표
    jitterbuf::Stats jb;
};

// PL3b-D5·D6 받는 길 전체. 세 태스크가 부른다:
//   push  웹소켓 태스크 — 받은 프레임을 지터 버퍼에 넣는다(복사만)
//   pump  해제 태스크   — 링버퍼가 한 묶음(BLOCK)보다 적으면 프레임을 꺼내 PCM 으로 만들어 넣는다. 출력 태스크가 한 묶음 가져갈 때마다,
//                        그리고 프레임이 도착할 때마다 깨운다(want_pump 가 참일 때 — 채우는 중에 프레임이 다 모이면 다음 묶음부터 바로 소리가 난다)
//   fill  출력 태스크   — 16ms 묶음을 가져간다(멈추지 않는다: 링버퍼에서 복사만)
// 지터 버퍼는 Lock 으로, 링버퍼는 잠금 없이(쓰는 쪽 = pump, 읽는 쪽 = fill) 지킨다. start(통화가 열림)·stop(끝남)은 app 태스크가 부른다:
// start 는 지터 버퍼만 그 자리에서 비우고, 은닉·해제기·링버퍼는 주인 태스크가 다음 차례에 비운다(epoch).
// 닫혀 있는 동안에는 push 가 버리고 pump 가 아무것도 하지 않는다 — 통화 밖에서 온 프레임은 한 표본도 나지 않는다.
template <class Lock>
class Receiver {
public:
    explicit Receiver(Lock &lock) : lock_(lock) {}

    void start()
    {
        lock_.lock();
        jb_.reset();
        lock_.unlock();
        frames_ = 0; bad_ = 0; played_ = 0; concealed_ = 0; dec_fail_ = 0; under_ = 0; play_rms_ = 0;
        buffering_ = true;
        epoch_.fetch_add(1);
        open_ = true;
    }
    void stop() { open_ = false; }
    bool open() const { return open_.load(); }
    // P5A-D3 받는 목소리의 재생 이득(Q8 — 256 = 그대로). 은닉으로 메운 소리에도 같이 걸린다. 통화 중에도 바뀐다(다음 프레임부터). 재는 세기(play_rms)는 이득 뒤의 값이다.
    void set_play_gain_q8(int q8) { play_gain_q8_ = q8 < 0 ? 0 : q8; }
    int play_gain_q8() const { return play_gain_q8_.load(); }
    uint32_t frames() const { return frames_.load(); }  // 이번 통화에서 받은 프레임 수(잠금 없이 읽는다 — app 태스크의 수신 무음 감시용. stats() 는 지터 버퍼 잠금을 잡는다)
    // PL3c-D5 지터 버퍼의 규칙을 바꾼다(시리얼 `jb` — 통화 중에도 된다, 통화가 끝나도 남는다)
    void set_policy(const jitterbuf::Policy &p)
    {
        lock_.lock();
        jb_.set_policy(p);
        lock_.unlock();
    }
    uint32_t under() const { return under_.load(); }   // 굶음 횟수(잠금 없이 읽는다 — 출력 태스크용. stats() 는 지터 버퍼 잠금을 잡는다)

    void push(const uint8_t *frame, size_t len)     // [순번 2][Opus]
    {
        if (!open_.load()) return;
        uint16_t seq = 0; const uint8_t *opus = nullptr; size_t n = 0;
        if (!relayproto::unpack_voice(frame, len, &seq, &opus, &n)) { ++bad_; return; }
        ++frames_;
        lock_.lock();
        jb_.push(seq, opus, n);
        lock_.unlock();
    }

    bool want_pump() const { return open_.load() && ring_.size() < (size_t)BLOCK; }

    // dec(data, len, pcm) → FRAME 표본을 pcm 에 풀고 true(실패면 false). fresh() → 새 통화의 첫 pump 에서 한 번(해제기를 처음 상태로).
    // 돌려주는 값 = 만든 프레임 수.
    template <class Dec, class Fresh>
    int pump(Dec &&dec, Fresh &&fresh)
    {
        if (!open_.load()) return 0;
        const uint32_t e = epoch_.load();
        if (e != pump_epoch_) { pump_epoch_ = e; plc_.reset(); rms_ = pcm::Rms(); rms_frames_ = 0; fresh(); }
        int made = 0;
        while (ring_.size() < (size_t)BLOCK && made < 4) {
            lock_.lock();
            const jitterbuf::Pop p = jb_.pop();
            buffering_ = jb_.buffering();
            lock_.unlock();
            alignas(16) int16_t pcm[FRAME];                  // fix 1 F3: 해제기 출력은 Phase 0 의 dec_pcm(probe_main.cpp 112)처럼 16바이트 정렬
            if (p.kind == jitterbuf::Kind::PLAY && dec(p.data, p.len, pcm)) { plc_.good(pcm, p.seq); ++played_; }
            else if (p.kind == jitterbuf::Kind::SILENCE) {
                // 채우는 중(통화 시작·끊긴 뒤): 무음을 링버퍼에 넣지 않는다 — 넣으면 그 무음이 첫 목소리 앞에 줄을 서서 지연이 된다(PL3b-D6).
                // 비어 있으면 fill 이 0 을 낸다. 직전 소리의 꼬리가 남아 있을 때만 그 5ms 를 넣는다.
                if (!plc_.silence(pcm)) break;
            }
            else { if (p.kind == jitterbuf::Kind::PLAY) ++dec_fail_; plc_.lost(pcm); ++concealed_; }
            if (const int g = play_gain_q8_.load(); g > 256) pcm::gain_soft_q8(pcm, FRAME, g, PLAY_KNEE, PLAY_CEIL);   // P5A-D3: 키울 때는 꼭대기를 부드럽게 누른다(스피커 한계 아래)
            else pcm::gain_q8(pcm, FRAME, g);
            rms_.add(pcm, FRAME);
            if (++rms_frames_ >= RATE / FRAME) { play_rms_ = rms_.take(); rms_frames_ = 0; }
            ring_.write(pcm, FRAME);
            ++made;
        }
        return made;
    }

    // 출력 한 묶음. 돌려주는 값 = 소리가 있나(false 면 out 을 건드리지 않는다).
    //   열려 있는 동안에는 늘 true(받은 것이 없으면 무음을 낸다 — 출력 태스크가 16ms 박자를 지켜야 pump 가 깨어난다).
    //   닫힌 뒤의 첫 묶음은 남은 소리를 5ms 에 걸쳐 줄인 것이고(PL3b-D13: 딸깍 방지, 나머지는 버린다) 그다음부터 false.
    bool fill(int16_t *out, int n)
    {
        const bool on = open_.load();
        const uint32_t e = epoch_.load();
        if (e != fill_epoch_) { fill_epoch_ = e; ring_.drain(); playing_ = false; hold_ = 0; }   // 새 통화: 앞 통화의 찌꺼기를 버린다(읽는 쪽에서)
        if (on) {
            // PL3b-D26 (fix 2) "채우는 중"은 링버퍼를 읽기 **앞에서** 한 번 떠 둔다. 읽은 뒤에 보면 그 사이 해제 태스크가 채우기를 끝내
            //   (프레임이 목표만큼 모인 순간) 정당한 무음 묶음이 굶음으로 세어졌다(최종 검수 M-7, 창 약 1µs). 앞에서 뜨면 "읽을 때 채우는 중이었다"가
            //   그대로 판정에 쓰인다. 비용: 없음. 되돌리기: 이 줄을 아래로. (두 태스크의 끼어드는 순서는 맥에서 흉내 낼 수 없어 시험으로는 못 고정한다 — 읽는 순서만 주석·검토로.)
            const bool was_buffering = buffering_.load();
            const size_t got = ring_.read(out, (size_t)n);
            if (got < (size_t)n) {
                memset(out + got, 0, ((size_t)n - got) * sizeof(int16_t));
                if (!was_buffering) ++under_;
            }
            hold_ = out[n - 1]; playing_ = true;
            return true;
        }
        if (!playing_) return false;
        const int m = n < FADE ? n : FADE;
        int16_t tail[FADE];
        const size_t got = ring_.read(tail, (size_t)m);
        memset(out, 0, (size_t)n * sizeof(int16_t));
        for (int i = 0; i < m; ++i) {
            const int16_t s = (size_t)i < got ? tail[i] : hold_;
            hold_ = s;
            out[i] = (int16_t)((int32_t)s * (m - 1 - i) / m);
        }
        playing_ = false; hold_ = 0;                // 링버퍼에 남은 것은 다음 통화가 열릴 때 버려진다(위의 epoch) — 닫혀 있는 동안에는 아무도 꺼내지 않는다
        return true;
    }

    RxStats stats()
    {
        RxStats s;
        s.frames = frames_; s.bad = bad_; s.played = played_; s.concealed = concealed_; s.dec_fail = dec_fail_; s.under = under_; s.play_rms = play_rms_;
        lock_.lock();
        s.depth = jb_.depth(); s.target = jb_.target_frames(); s.jb = jb_.stats();
        lock_.unlock();
        return s;
    }

private:
    Lock &lock_;
    jitterbuf::Buffer jb_;                          // lock_ 안에서만
    Concealer plc_;                                 // pump 에서만
    pcm::Rms rms_;                                  // pump 에서만
    int rms_frames_ = 0;
    uint32_t pump_epoch_ = 0;
    pcm::Ring<RING_CAP> ring_;                      // 쓰기 = pump, 읽기 = fill
    uint32_t fill_epoch_ = 0;                       // fill 에서만
    bool playing_ = false;
    int16_t hold_ = 0;
    std::atomic<uint32_t> epoch_{0}, frames_{0}, bad_{0}, played_{0}, concealed_{0}, dec_fail_{0}, under_{0}, play_rms_{0};
    std::atomic<bool> buffering_{true}, open_{false};
    std::atomic<int> play_gain_q8_{PLAY_GAIN_Q8};         // P5A-D3
};

// ── 수신 무음 감시 (Phase 3c, 계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D10 — 3b 최종 검수 M-9) ──
// 통화 중에 상대의 소리 프레임이 RX_SILENCE_MS 동안 하나도 오지 않으면 한 번 알린다 → app 이 상태 기계에 VOICE_LOST 를 넣어 통화를 끝낸다(call_fsm.h FSM-D2).
//   start  통화가 열릴 때(VOICE on)
//   check  app 태스크가 20ms 바퀴마다: 지금 시각과 "이번 통화에서 받은 프레임 수"(Receiver::frames)를 넣는다. 돌려주는 값 = 지금 알릴 것인가(통화마다 한 번만 true)
//   첫 프레임이 오기 전에는 세지 않는다: 소리 장치가 죽은 상대(PL3b-D2 — 소리 없이 통화 흐름만 도는 기기)는 프레임을 한 번도 보내지 않는데, 그 통화를 5초 만에 끊으면
//   "소리 없이도 통화 흐름은 된다"는 3b 의 약속이 깨진다. 비용: 한 프레임도 못 받은 통화는 이 감시가 끝내지 못한다(교환기의 45초에 맡긴다).
//   5초인 이유: 통화 중에는 프레임이 20ms 마다 온다(DTX 없음). 2026-10-05 아침의 혼잡한 망에서 잰 멈춤은 0.15~0.25초(가끔 0.8초)다. 보내는 쪽 소켓이 2초 막히면 연결이 끊긴다(RL-D6).
constexpr uint32_t RX_SILENCE_MS = 5000;
class RxWatch {
public:
    void start(uint32_t now_ms) { last_frames_ = 0; last_ms_ = now_ms; armed_ = false; fired_ = false; }
    bool check(uint32_t now_ms, uint32_t frames)
    {
        if (frames != last_frames_) { last_frames_ = frames; last_ms_ = now_ms; armed_ = frames != 0; return false; }   // 프레임이 왔다(또는 새 통화라 셈이 0 으로 돌아갔다)
        if (!armed_ || fired_) return false;
        if ((uint32_t)(now_ms - last_ms_) < RX_SILENCE_MS) return false;       // 시계(ms)가 한 바퀴 돌아도 맞는다
        fired_ = true;
        return true;
    }
    uint32_t silent_ms(uint32_t now_ms) const { return armed_ ? (uint32_t)(now_ms - last_ms_) : 0; }

private:
    uint32_t last_frames_ = 0, last_ms_ = 0;
    bool armed_ = false, fired_ = false;
};

// ── 시험음 한 토막, 지연 재기 (PL3b-D12·D14) ─────────────────────────────────
// 쉼 lead → 1kHz 소리 len → 쉼 tail(표본 수)을 차례로 만든다. 소리의 처음·끝 5ms 는 크기를 옮긴다(딸깍 방지). 끝나면 active 가 거짓.
class Burst {
public:
    Burst(int lead, int len, int tail, int amp) : lead_(lead), len_(len), tail_(tail), amp_(amp) {}
    void start() { pos_ = 0; active_ = true; tone_.reset(); }
    void stop() { active_ = false; }
    bool active() const { return active_; }
    // 한 묶음(n 표본)을 만든다. 돌려주는 값 = 이 묶음에서 소리가 시작한 표본 위치(0..n-1), 아니면 -1.
    int render(int16_t *out, int n)
    {
        int at = -1;
        for (int i = 0; i < n; ++i, ++pos_) {
            const int k = pos_ - lead_;                                      // 소리 안에서의 위치
            int16_t v = 0;
            if (active_ && k >= 0 && k < len_) {
                if (k == 0) at = i;
                tone_.render(&v, 1, amp_);
                const int edge = k < FADE ? k : len_ - 1 - k < FADE ? len_ - 1 - k : FADE;
                v = (int16_t)((int32_t)v * edge / FADE);
            }
            out[i] = v;
        }
        if (pos_ >= lead_ + len_ + tail_) active_ = false;
        return at;
    }

private:
    TestTone tone_;
    int lead_, len_, tail_, amp_, pos_ = 0;
    bool active_ = false;
};
// 지연 재기: 재는 쪽이 마이크 대신 "쉼 0.4초 → 삑 0.1초 → 쉼 0.5초"를 보내고, 상대가 돌려보낸 소리에서 삑의 시작(onset)을 찾아 걸린 시간을 잰다.
// 상대는 vsrc ref(스피커 되먹임 — 앰프를 꺼도 된다) 또는 vsrc mic(스피커 → 공기 → 마이크)로 둔다.
struct PingGen : Burst { PingGen() : Burst(PING_LEAD, PING_BURST, PING_TAIL, PING_AMP) {} };
// 혼자 하는 점검의 시험음(통화 밖에서 이 기기의 스피커로 0.64초)
struct SelfTestTone : Burst { SelfTestTone() : Burst(0, SELFTEST_TONE, 0, TEST_AMP) {} };
// 받은 PCM 에서 |x| 가 threshold 이상인 첫 표본의 위치(없으면 -1).
inline int onset(const int16_t *pcm, int n, int threshold)
{
    for (int i = 0; i < n; ++i) if (pcm[i] >= threshold || pcm[i] <= -threshold) return i;
    return -1;
}

}  // namespace voicecore
