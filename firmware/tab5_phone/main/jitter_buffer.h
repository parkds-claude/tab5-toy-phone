// jitter_buffer.h — 들쭉날쭉 도착하는 음성 프레임을 재생 박자에 맞춰 고르게 꺼내 준다. 하드웨어 무관(맥에서 단위 시험: tools/test_jitter_buffer.cpp).
// 설계서 docs/exec-plans/2026-10-03-tab5-phone.md §9.6, 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md PL3b-D5:
//   - 60ms(3프레임)로 시작해 40~200ms 사이에서 조절한다: 다 떨어져 끊길 때마다 +1프레임, 10초 무사하면 -1프레임.
//   - 프레임이 비면 LOST 를 돌려준다 → 부르는 쪽이 메운다(voice_core.h 의 Concealer). 3번 연속 비면 무음으로 두고 다시 찰 때까지 기다린다.
//     JB-D2: 다 떨어져서 메울 때는 순번을 넘기지 않는다 — 늦게 온 그 프레임을 그대로 튼다(재생이 한 프레임만큼 뒤로 밀려 여유가 다시 생긴다).
//            순번을 넘기던 초안은 여유가 0 이 된 뒤 "메움 3번 → 그 3프레임은 늦었다고 버림 → 다시 여유 0"을 끝없이 되풀이했다
//            (받는 쪽 시계가 조금 빠르기만 해도 생긴다 — 긴 시험 "receiver clock fast" 가 잡았다). 비용: 메운 만큼 지연이 늘어난다
//            → 깊어지면 아래의 "1초 뒤 목표로 줄임"이 되돌린다. 뒤 프레임은 와 있는데 그 순번만 빠진 경우(진짜 손실)는 전처럼 넘긴다.
//   - 300ms(15프레임)를 넘게 쌓이면 오래된 것부터 버린다. 다시 틀기 시작할 때와, 1초 넘게 목표보다 깊을 때도 목표 깊이로 줄인다
//     → TCP 가 막혔다 풀려 프레임이 몰려와도 지연이 쌓이지 않는다.
// 쓰는 법: 받는 쪽 태스크가 push(), 재생 쪽 태스크가 프레임 하나(20ms 분량)가 필요할 때마다 pop(). 두 태스크가 다르면 부르는 쪽이 잠금으로 감싼다
//   (이 클래스는 잠그지 않는다 — voice_core.h 의 Receiver 가 감싼다). pop 은 정확히 20ms 마다가 아니어도 된다: 출력이 16ms 묶음이라
//   16·16·16·32ms 간격으로 불린다(평균 20ms). 10초·1초 규칙은 pop 횟수(500·50)로 센다.
// JB-D1: 전송이 TCP(WebSocket)라 순서 뒤바뀜·중복은 실제로는 오지 않는다. 그래도 순번으로 정렬·중복 제거를 하는 것은
//        교환기를 UDP 계열(LiveKit, PH-D1 탈출구)로 바꿔도 이 파일을 그대로 쓰기 위해서다. 비용: 칸 32개 × 398바이트 = 약 13KB.
// JB-D3 (Phase 3c, 계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D5): 깊이를 정하는 규칙을 Policy 로 묶어 실행 중에 바꿀 수 있게 했다.
//        기본(legacy_policy)은 위의 3b 규칙 그대로다(시험 65개가 고치지 않고 통과). 새 규칙 tuned_policy(상한)은 흔들리는 망에서 지연이 쌓여 머무는 것을 고친다:
//          ① 목표의 상한을 낮춘다(3b 는 200ms): 2026-10-05 아침의 망은 0.15~0.25초 멈춤이 5초에 한 번꼴이라 "10초 무사"가 오지 않아 목표가 상한까지 올라가 머물렀다
//             (양쪽 160~200ms = 왕복 지연 540~570ms 의 절반 이상). 멈춤이 목표보다 길면 깊은 버퍼로도 끊김을 못 막는다 — 지연만 늘어난다.
//             단 끊김이 1초 안에 세 번 거듭되면(절전이 다시 켜진 것 같은 망 — 0.3초마다 뭉쳐 온다) 그동안은 3b 의 규칙으로 돈다(상한 200ms, ③ 도 3b 처럼):
//             낮은 상한을 고집하면 소리의 대부분을 버리게 된다(맥 시뮬레이션: 상한 4 고정이면 33% 만 튼다 → 이 예외로 3b 와 같은 88%).
//             "세 번"인 이유: 통화가 열린 직후에는 끊김이 한두 번 붙어 오는 일이 흔하다(3b 기기 기록) — 그것으로 상한을 넘기지 않으려는 것.
//          ② 5초 무사하면 목표를 줄인다(3b 는 10초).
//          ③ 깊어지면 1초 기다렸다 한꺼번에 버리는 대신(0.2초를 통째로 건너뛴다) 0.5초 지켜본 뒤 0.1초마다 한 프레임씩 버린다(20ms 씩 — 이어 붙는 자리는 Concealer 가 5ms 에 걸쳐 잇는다).
//             다시 틀기 시작한 직후 0.5초 안에 몰려온 꼬리는 기다리지 않고 바로 버린다(이미 그만큼 늦은 소리다).
//        비용: 상한이 낮으면 그보다 긴 멈춤마다 끊긴다(3b 는 200ms 까지 버텼다) — 맥 시뮬레이션(test_voice_core.cpp --info)이 아침 모양의 망에서 두 규칙의 지연·끊김을 나란히 찍는다.
//        되돌리기: legacy_policy(시리얼 `jb 0`).
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace jitterbuf {

const int    FRAME_MS        = 20;
const int    START_FRAMES    = 3;      // 60ms
const int    MIN_FRAMES      = 2;      // 40ms
const int    MAX_FRAMES      = 10;     // 200ms
const int    OVERFLOW_FRAMES = 15;     // 300ms — 이보다 깊어지면 오래된 것부터 버린다
const int    MAX_CONCEAL     = 3;      // 다 떨어졌을 때 손실 은닉으로 버티는 횟수(60ms)
const int    CALM_POPS       = 500;    // 10초 동안 끊김이 없으면 목표를 한 프레임 줄인다
const int    CATCHUP_POPS    = 50;     // 1초 동안 목표보다 2프레임 넘게 깊으면 목표로 줄인다
const int    SLOTS           = 32;     // 순번 창(2의 거듭제곱)
const size_t MAX_PAYLOAD     = 398;    // 음성 프레임 400바이트 - 순번 2바이트(§9.4)

// JB-D3 깊이를 정하는 규칙. 값이 LEGACY 와 같으면 동작이 3b 와 글자 그대로 같다.
struct Policy {
    int max_frames;         // 목표의 상한(프레임). MIN_FRAMES 이상
    int calm_pops;          // 이만큼 끊김 없이 틀면 목표를 한 프레임 줄인다
    int catchup_pops;       // 목표보다 2프레임 넘게 깊은 채 이만큼 지나면 줄인다
    int catchup_step;       // 0 = 그때 목표까지 한꺼번에 줄인다(3b). N = 가장 오래된 한 프레임만 버리고, 계속 깊으면 N pop 뒤에 또 한 프레임
    int resume_pops;        // 다시 틀기 시작한 뒤 이 pop 수 안에 2프레임 넘게 깊어지면 기다리지 않고 바로 목표로 줄인다. 0 = 하지 않는다(3b)
    int burst_pops;         // 앞 끊김 뒤 이 pop 수 안에 또 끊기는 일이 두 번 이어지면(= 세 번째 끊김) "거듭 끊기는 망"으로 본다: 끊김이 이 pop 수보다 뜸해질 때까지 상한은 MAX_FRAMES, 줄이는 규칙은 3b 것. 0 = 보지 않는다
};
const int TUNED_CALM_POPS = 250, TUNED_CATCHUP_POPS = 25, TUNED_CATCHUP_STEP = 5, TUNED_RESUME_POPS = 25, TUNED_BURST_POPS = 50;   // 5초 / 0.5초 / 0.1초 / 0.5초 / 1초
inline Policy legacy_policy() { Policy p = {10, 500, 50, 0, 0, 0}; return p; }
// cap = 목표 상한(프레임). 범위 밖이면 가까운 끝으로 맞춘다(START_FRAMES 아래로는 내리지 않는다 — 통화 시작의 60ms 는 그대로다).
inline Policy tuned_policy(int cap)
{
    Policy p = {cap < 3 ? 3 : cap > 10 ? 10 : cap, TUNED_CALM_POPS, TUNED_CATCHUP_POPS, TUNED_CATCHUP_STEP, TUNED_RESUME_POPS, TUNED_BURST_POPS};
    return p;
}

enum class Kind : uint8_t { SILENCE, PLAY, LOST };

struct Pop {
    Kind   kind = Kind::SILENCE;
    uint16_t seq = 0;             // PLAY 일 때: 그 프레임의 순번(이어지는 프레임인지 부르는 쪽이 본다 — 오래된 것을 버린 뒤에는 건너뛴다)
    size_t len = 0;
    uint8_t data[MAX_PAYLOAD];
};

struct Stats {
    uint32_t played = 0;          // 꺼내 준 프레임
    uint32_t lost = 0;            // 손실 은닉으로 메운 횟수
    uint32_t late = 0;            // 이미 지나간 순번이라 버림
    uint32_t dup = 0;             // 같은 순번이 또 와서 버림
    uint32_t bad = 0;             // 규격 밖(빈 것·너무 큰 것)
    uint32_t overflow_drop = 0;   // 300ms 초과·다시 틀기 시작할 때 버린 프레임
    uint32_t catchup_drop = 0;    // 틀던 중 깊어져 줄이느라 버린 프레임
    uint32_t underruns = 0;       // 다 떨어진 횟수
    uint32_t resync = 0;          // 순번이 크게 튀어 다시 맞춘 횟수
};

class Buffer {
public:
    Buffer() : pol_(legacy_policy()) { reset(); }

    void reset()                                                    // 규칙(Policy)은 남는다 — 통화마다 다시 걸지 않아도 된다
    {
        for (int i = 0; i < SLOTS; ++i) slot_[i].used = false;
        count_ = 0; started_ = false; buffering_ = true;
        target_ = START_FRAMES; conceal_run_ = 0; calm_ = 0; deep_ = 0; resume_left_ = 0; since_urun_ = 0x7FFFFFFF; bursty_ = false; short_run_ = 0;
        next_ = 0; stats_ = Stats();
        if (target_ > pol_.max_frames) target_ = pol_.max_frames;
    }

    // JB-D3 규칙을 바꾼다(통화 중에도 된다). 목표가 새 상한보다 높으면 그 자리에서 상한으로 내린다 — 넘치는 프레임은 규칙대로(한꺼번에 또는 하나씩) 줄어든다.
    void set_policy(const Policy &p)
    {
        pol_ = p;
        if (pol_.max_frames < MIN_FRAMES) pol_.max_frames = MIN_FRAMES;
        if (pol_.max_frames > MAX_FRAMES) pol_.max_frames = MAX_FRAMES;
        if (pol_.calm_pops < 1) pol_.calm_pops = 1;
        if (pol_.catchup_pops < 1) pol_.catchup_pops = 1;
        if (pol_.catchup_step < 0) pol_.catchup_step = 0;
        if (pol_.catchup_step > pol_.catchup_pops) pol_.catchup_step = pol_.catchup_pops;
        if (pol_.resume_pops < 0) pol_.resume_pops = 0;
        if (pol_.burst_pops < 0) pol_.burst_pops = 0;
        if (target_ > pol_.max_frames) target_ = pol_.max_frames;
        calm_ = 0; deep_ = 0; bursty_ = false; short_run_ = 0;
    }
    const Policy &policy() const { return pol_; }

    int depth() const { return count_; }
    bool buffering() const { return buffering_; }               // 채우는 중(무음) — 통화 시작 직후와 끊긴 뒤
    int target_frames() const { return target_; }
    const Stats &stats() const { return stats_; }

    void push(uint16_t seq, const uint8_t *data, size_t len)
    {
        if (!data || len == 0 || len > MAX_PAYLOAD) { ++stats_.bad; return; }
        if (!started_) { started_ = true; next_ = seq; }
        int d = (int16_t)(uint16_t)(seq - next_);                // 다음에 꺼낼 순번에서 얼마나 떨어졌나(음수 = 이미 지나감)
        if (d >= SLOTS || d <= -SLOTS) {                         // 크게 튐: 상대가 다시 시작했다 → 비우고 새 순번에서
            clear_slots(); next_ = seq; d = 0;
            buffering_ = true; conceal_run_ = 0; deep_ = 0; ++stats_.resync;
        }
        if (d < 0) { ++stats_.late; return; }
        Slot &s = slot_[seq & (SLOTS - 1)];
        if (s.used) { ++stats_.dup; return; }                    // 창 안에서 같은 칸 = 같은 순번
        s.used = true; s.len = (uint16_t)len; memcpy(s.data, data, len);
        ++count_;
        if (count_ > OVERFLOW_FRAMES) stats_.overflow_drop += trim_to(target_);
    }

    Pop pop()
    {
        Pop out;
        if (buffering_) {
            if (count_ < target_) return out;                    // 아직 덜 찼다 → 무음
            if (count_ - target_ >= 2) stats_.overflow_drop += trim_to(target_);   // 밀려 있던 만큼은 버리고 가장 새 프레임부터(한 프레임 여유는 둔다)
            buffering_ = false; conceal_run_ = 0; calm_ = 0; deep_ = 0;
            resume_left_ = bursty_ ? 0 : pol_.resume_pops;
        }

        if (bursty_ && since_urun_ > pol_.burst_pops) bursty_ = false;      // 끊김이 뜸해졌다 → 새 규칙으로 돌아간다
        const int wait = bursty_ ? CATCHUP_POPS : pol_.catchup_pops, step = bursty_ ? 0 : pol_.catchup_step;
        if (count_ - target_ >= 2) {                             // 틀던 중 깊어짐
            if (resume_left_ > 0) { stats_.overflow_drop += trim_to(target_); deep_ = 0; }   // JB-D3 ③ 다시 틀기 시작한 직후 몰려온 꼬리: 바로 목표로("다시 틀기 시작할 때 버린 것"으로 센다)
            else if (++deep_ >= wait) {                          // 3b: 1초 지켜본 뒤 목표로 / 새 규칙: 0.5초 지켜본 뒤 0.1초마다 한 프레임
                if (step == 0) { stats_.catchup_drop += trim_to(target_); deep_ = 0; }
                else { stats_.catchup_drop += trim_to(count_ - 1); deep_ = wait - step; }
            }
        } else {
            deep_ = 0;
        }
        if (resume_left_ > 0) --resume_left_;
        if (since_urun_ < 0x7FFFFFFF) ++since_urun_;

        Slot &s = slot_[next_ & (SLOTS - 1)];
        if (s.used) {
            out.kind = Kind::PLAY; out.seq = next_; out.len = s.len; memcpy(out.data, s.data, s.len);
            s.used = false; --count_; ++next_;
            conceal_run_ = 0; ++stats_.played;
            if (++calm_ >= pol_.calm_pops) { calm_ = 0; if (target_ > MIN_FRAMES) --target_; }
            return out;
        }

        if (count_ > 0) {                                        // 이 순번만 빠졌고 뒤는 와 있다 → 은닉하고 넘어간다
            ++next_; ++stats_.lost; out.kind = Kind::LOST;
            return out;
        }

        // 다 떨어졌다 — 기다리던 순번(next_)은 그대로 둔다(JB-D2)
        if (conceal_run_ == 0) {
            ++stats_.underruns; calm_ = 0;
            short_run_ = since_urun_ <= pol_.burst_pops ? short_run_ + 1 : 0;  // 앞 끊김 뒤 금방 또 끊긴 횟수(이어서)
            bursty_ = pol_.burst_pops > 0 && short_run_ >= 2;                  // JB-D3 ①: 세 번째 — 거듭 끊기는 망
            if (target_ < (bursty_ ? MAX_FRAMES : pol_.max_frames)) ++target_;
            since_urun_ = 0;
        }
        if (conceal_run_ < MAX_CONCEAL) {
            ++conceal_run_; ++stats_.lost; out.kind = Kind::LOST;
            return out;
        }
        buffering_ = true;                                       // 더 은닉하지 않고 다시 찰 때까지 무음
        return out;
    }

private:
    struct Slot { bool used; uint16_t len; uint8_t data[MAX_PAYLOAD]; };

    void clear_slots() { for (int i = 0; i < SLOTS; ++i) slot_[i].used = false; count_ = 0; }

    // 가장 새 프레임 keep 개만 남기고 오래된 것부터 버린다. 돌려주는 값 = 버린 프레임 수.
    uint32_t trim_to(int keep)
    {
        uint32_t dropped = 0;
        while (count_ > keep) {
            Slot &s = slot_[next_ & (SLOTS - 1)];
            if (s.used) { s.used = false; --count_; ++dropped; }
            ++next_;
        }
        while (count_ > 0 && !slot_[next_ & (SLOTS - 1)].used) ++next_;   // 맨 앞의 빈 순번은 건너뛴다
        return dropped;
    }

    Slot     slot_[SLOTS];
    int      count_;
    bool     started_, buffering_, bursty_;
    int      target_, conceal_run_, calm_, deep_, resume_left_, since_urun_, short_run_;
    Policy   pol_;
    uint16_t next_;
    Stats    stats_;
};

}  // namespace jitterbuf
