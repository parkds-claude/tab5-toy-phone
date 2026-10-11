// pcm.h — 16비트 PCM 묶음에 하는 작은 계산: 섞기, 크기 바꾸기, 세기(RMS), 태스크 사이 링버퍼, 출력 박자 감시.
// 하드웨어·시계와 무관한 순수 로직(맥에서 단위 시험: tools/test_pcm.cpp).
// 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md 의 결정 PL3b-D3(섞기)·D4(앞서 쓰는 양)·D6(링버퍼).
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pcm {

inline int16_t sat16(int32_t v) { return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }

// acc += add (넘치면 +32767 / −32768 에서 자른다 — sat16). 목소리 + 신호음처럼 두 소리를 함께 낼 때.
inline void mix_add(int16_t *acc, const int16_t *add, int n)
{
    for (int i = 0; i < n; ++i) acc[i] = sat16((int32_t)acc[i] + add[i]);
}

// buf *= gain_q8 / 256 (넘치면 +32767 / −32768 에서 자른다 — 돌지 않는다). 256 = 그대로, 512 = 2배(+6dB).
inline void gain_q8(int16_t *buf, int n, int gain)
{
    if (gain == 256) return;
    for (int i = 0; i < n; ++i) buf[i] = sat16(((int32_t)buf[i] * gain) / 256);
}

// 이득을 걸되 넘치는 꼭대기는 부드럽게 누른다(P5A-D3 — 받는 목소리를 키울 때): |v| 가 knee 를 넘으면 ceil 로 휘어 붙는다(기울기가 knee 에서 1 로 이어진다 — 딱 잘리지 않는다).
// tx_dyn.h 의 soft_clip 과 같은 곡선이다(그쪽은 보내는 길). 1배 이하의 이득에는 gain_q8 을 쓴다.
inline int16_t soft_limit(int32_t v, int32_t knee = 20000, int32_t ceil = 32767)
{
    const int32_t a = v < 0 ? -v : v;
    if (a <= knee) return (int16_t)v;
    const int64_t room = ceil - knee, d = (int64_t)a - knee;
    const int32_t y = knee + (int32_t)(room * d / (d + room));
    return (int16_t)(v < 0 ? -y : y);
}
inline void gain_soft_q8(int16_t *buf, int n, int gain, int32_t knee = 20000, int32_t ceil = 32767)
{
    for (int i = 0; i < n; ++i) buf[i] = soft_limit(((int32_t)buf[i] * gain) / 256, knee, ceil);
}

// PL3b-D3 출력 한 묶음: 공급자들을 등록된 순서로 불러, 소리가 있는 것만 합친다(첫 소리는 그대로, 다음부터는 더하고 넘치면 자른다).
//   공급자 = bool fn(int16_t *out, int n): 낼 소리가 있으면 out 에 n 표본을 쓰고 true, 없으면 false(out 은 쓰지 않아도 된다).
//   돌려주는 값 = 소리를 낸 공급자 수. 0 이면 mix 는 건드리지 않는다(부르는 쪽은 쓰지 않고 잠든다).
//   "누가 이기는가"는 없다 — 신호음과 목소리는 겹치는 동안 그냥 더해진다. 겹치는 구간을 짧게 만드는 것은 각 공급자의 몫이다
//   (신호음은 5ms 에 줄어 멎고, 목소리는 5ms 에 걸쳐 커지고 줄어든다 — tones.h, voice_core.h).
typedef bool (*source_fn)(int16_t *out, int n);
inline int mix_sources(const source_fn *src, int count, int16_t *mix, int16_t *tmp, int n)
{
    int sounding = 0;
    for (int i = 0; i < count; ++i) {
        if (!src[i] || !src[i](tmp, n)) continue;
        if (sounding == 0) memcpy(mix, tmp, (size_t)n * sizeof(int16_t));
        else mix_add(mix, tmp, n);
        ++sounding;
    }
    return sounding;
}

inline uint32_t isqrt64(uint64_t v)                 // 버림 제곱근
{
    uint64_t r = 0, bit = (uint64_t)1 << 62;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return (uint32_t)r;
}

// 세기(RMS)를 모은다. add 로 표본을 넣고 take 로 값을 꺼내며 비운다(표본이 없었으면 0).
struct Rms {
    uint64_t sum = 0;
    uint32_t n = 0;
    void add(const int16_t *s, int count)
    {
        for (int i = 0; i < count; ++i) sum += (uint64_t)((int32_t)s[i] * s[i]);
        n += (uint32_t)count;
    }
    uint32_t value() const { return n ? isqrt64(sum / n) : 0; }
    uint32_t take() { const uint32_t v = value(); sum = 0; n = 0; return v; }
};

// PL3b-D6 태스크 둘 사이의 PCM 링버퍼: 쓰는 태스크 하나, 읽는 태스크 하나(잠금 없음 — 읽는 쪽이 출력 태스크라 멈추면 안 된다).
//   CAP 은 2의 거듭제곱. write 는 자리가 모자라면 들어가는 만큼만 넣는다(나머지는 버린다 — 돌려주는 값으로 안다).
//   drain 은 읽는 쪽에서만 부른다(쓰는 쪽이 한창 쓰는 중이어도 안전하다 — 읽는 위치만 옮긴다).
template <unsigned CAP>
class Ring {
    static_assert(CAP >= 2 && (CAP & (CAP - 1)) == 0, "CAP must be a power of two");

public:
    // 쓰는 태스크·읽는 태스크 말고 제3 태스크가 부르면(voice.cpp on_frame 의 want_pump) 두 위치를 읽는 사이에 한쪽이 움직여 순간적으로 큰 값이 나올 수 있다.
    // 그 결과는 해제 태스크를 깨우는 것을 한 번 빼먹는 것뿐이다(출력 태스크가 16ms 안에 다시 깨운다). 그 밖의 용도로 쓰지 말 것.
    size_t size() const { return (size_t)(w_.load(std::memory_order_acquire) - r_.load(std::memory_order_acquire)); }
    size_t space() const { return CAP - size(); }

    size_t write(const int16_t *src, size_t n)      // 쓰는 태스크에서만
    {
        const uint32_t w = w_.load(std::memory_order_relaxed);
        const size_t room = CAP - (size_t)(w - r_.load(std::memory_order_acquire));
        if (n > room) n = room;
        for (size_t i = 0; i < n; ++i) buf_[(w + i) & (CAP - 1)] = src[i];
        w_.store(w + (uint32_t)n, std::memory_order_release);
        return n;
    }

    size_t read(int16_t *dst, size_t n)             // 읽는 태스크에서만
    {
        const uint32_t r = r_.load(std::memory_order_relaxed);
        const size_t have = (size_t)(w_.load(std::memory_order_acquire) - r);
        if (n > have) n = have;
        for (size_t i = 0; i < n; ++i) dst[i] = buf_[(r + i) & (CAP - 1)];
        r_.store(r + (uint32_t)n, std::memory_order_release);
        return n;
    }

    void drain() { r_.store(w_.load(std::memory_order_acquire), std::memory_order_release); }   // 읽는 태스크에서만

private:
    int16_t buf_[CAP];
    std::atomic<uint32_t> w_{0}, r_{0};
};

// PL3b-D4 출력 박자 감시: 한 묶음을 다 쓴 시각을 넣으면, 앞 묶음과의 간격이 limit_us 이상일 때 "늦음"으로 센다.
//   출력 태스크는 정상이면 16ms 마다 한 묶음을 쓴다. 간격이 DMA 가 앞서 쥔 양(칸 수 × 16ms)을 넘으면 그 사이 스피커에 낼 것이 바닥난 것이다.
//   쉬었다가(낼 것이 없어 잠들었다가) 다시 쓰는 첫 묶음은 세지 않는다 — rest() 로 알린다.
struct GapWatch {
    int64_t last_us = 0;
    uint32_t late = 0, max_us = 0;
    void rest() { last_us = 0; }
    void wrote(int64_t now_us, int64_t limit_us)
    {
        if (last_us) {
            const int64_t gap = now_us - last_us;
            if (gap > (int64_t)max_us) max_us = (uint32_t)(gap > 0xFFFFFFFFll ? 0xFFFFFFFFll : gap);
            if (gap >= limit_us) ++late;
        }
        last_us = now_us;
    }
};

}  // namespace pcm
