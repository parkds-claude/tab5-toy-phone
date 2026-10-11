// test_jitter_buffer.cpp — jitter_buffer.h 단위 시험:
//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_jitter_buffer.cpp -o /tmp/t_jb && /tmp/t_jb
// 설계서 §9.6: 60ms 로 시작해 40~200ms 사이에서 조절, 빈 프레임은 손실 은닉, 300ms 를 넘으면 오래된 것부터 버려 지연이 쌓이지 않게.
// 뒤쪽의 "오래·거칠게" 시험은 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md Review Focus 3(손실·순서 뒤바뀜·몰림·오래 통화).
#include "jitter_buffer.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
using namespace jitterbuf;
static int ok = 0, bad = 0;
#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)

// 프레임 내용 = 순번의 아래 바이트를 len 번 반복 (꺼낸 것이 어느 프레임인지 확인용)
static void put(Buffer &b, uint16_t seq, size_t len = 60)
{
    uint8_t d[MAX_PAYLOAD];
    memset(d, (uint8_t)seq, len);
    b.push(seq, d, len);
}
static bool run_dry(Buffer &b) { for (int i = 0; i < 1000; ++i) if (b.pop().kind == Kind::SILENCE) return true; return false; }   // 무음이 나올 때까지 꺼낸다(고장 나도 끝난다)
static bool is(const Pop &p, uint16_t seq, size_t len = 60) { return p.kind == Kind::PLAY && p.len == len && p.data[0] == (uint8_t)seq && p.data[len - 1] == (uint8_t)seq; }

int main()
{
    // ── 처음: 목표 깊이(3프레임 = 60ms)가 찰 때까지 무음, 그 뒤 순서대로 ─────
    {
        Buffer b;
        CHECK("empty: silence", b.pop().kind == Kind::SILENCE);
        put(b, 100); put(b, 101);
        CHECK("below target: still silence", b.pop().kind == Kind::SILENCE && b.depth() == 2);
        put(b, 102);
        CHECK("target reached: plays first frame", is(b.pop(), 100));
        CHECK("then in order", is(b.pop(), 101) && is(b.pop(), 102));
        CHECK("start target is 60ms", b.target_frames() == 3 && START_FRAMES * FRAME_MS == 60);
        CHECK("limits are 40..200ms and 300ms", MIN_FRAMES * FRAME_MS == 40 && MAX_FRAMES * FRAME_MS == 200 && OVERFLOW_FRAMES * FRAME_MS == 300);
    }

    // ── 순서가 뒤바뀌어 와도 순번대로 꺼낸다 ────────────────────────────────
    {
        Buffer b;
        put(b, 10); put(b, 12); put(b, 11);
        CHECK("reorder: 10, 11, 12", is(b.pop(), 10) && is(b.pop(), 11) && is(b.pop(), 12));
    }

    // ── 한 프레임이 빠졌고 뒤 프레임은 와 있다 → 손실 은닉(LOST) 한 번, 뒤는 그대로 ──
    {
        Buffer b;
        put(b, 1); put(b, 2); put(b, 4); put(b, 5);
        CHECK("gap: 1, 2", is(b.pop(), 1) && is(b.pop(), 2));
        CHECK("gap: missing 3 -> LOST", b.pop().kind == Kind::LOST);
        CHECK("gap: 4 follows", is(b.pop(), 4));
        CHECK("gap: counted once", b.stats().lost == 1 && b.stats().played == 3);
    }

    // ── 같은 프레임이 두 번, 이미 지나간 프레임 ─────────────────────────────
    {
        Buffer b;
        put(b, 1); put(b, 1); put(b, 2); put(b, 3);
        CHECK("duplicate dropped", b.depth() == 3 && b.stats().dup == 1);
        b.pop(); b.pop();
        put(b, 1);
        CHECK("late frame dropped", b.depth() == 1 && b.stats().late == 1);
        CHECK("late frame did not disturb order", is(b.pop(), 3));
    }

    // ── 다 떨어졌을 때(망이 잠깐 막힘): 3번까지 은닉, 그 뒤엔 무음으로 다시 채우기를 기다린다 ──
    {
        Buffer b;
        put(b, 1); put(b, 2); put(b, 3);
        b.pop(); b.pop(); b.pop();
        CHECK("underrun: conceal 1", b.pop().kind == Kind::LOST);
        CHECK("underrun: conceal 2", b.pop().kind == Kind::LOST);
        CHECK("underrun: conceal 3", b.pop().kind == Kind::LOST);
        CHECK("underrun: then silence (rebuffering)", b.pop().kind == Kind::SILENCE && b.pop().kind == Kind::SILENCE);
        CHECK("underrun counted once, target grows by one", b.stats().underruns == 1 && b.target_frames() == 4);
        // 막혔던 프레임이 한꺼번에 온다(4~20, 17개): 메우는 동안 순번을 넘기지 않았으므로(JB-D2) 늦은 것은 없다.
        // 300ms 를 넘는 순간 가장 새 프레임 '목표 깊이'만큼만 남긴다 — 밀린 만큼 지연이 쌓이지 않게.
        for (uint16_t s = 4; s <= 20; ++s) put(b, s);
        CHECK("burst: nothing is late (the concealed frames were still being waited for)", b.stats().late == 0);
        CHECK("burst: older frames dropped and counted, at most target + 1 kept", b.stats().overflow_drop == (uint32_t)(16 - b.target_frames()) && b.depth() == b.target_frames() + 1);
        Pop p = b.pop();
        CHECK("burst: resumes with the newest frames (delay does not pile up)", p.kind == Kind::PLAY && p.data[0] == (uint8_t)(20 - b.target_frames()) && b.depth() == b.target_frames());
    }

    // ── 끊겼다가(무음) 다시 틀 때: 그 사이 몰려온 것이 300ms 이하여도 목표 깊이만 남기고 버린다 ──
    {
        Buffer b;
        put(b, 1); put(b, 2); put(b, 3);
        run_dry(b);                // 다 꺼냄 → 메움 3번 → 무음(목표 4)
        for (uint16_t s = 4; s <= 13; ++s) put(b, s);           // 10개(200ms)가 한꺼번에 — 넘치지는 않는다
        CHECK("resume after a gap: 10 frames waiting, nothing dropped yet", b.depth() == 10 && b.stats().overflow_drop == 0 && b.target_frames() == 4);
        const Pop p = b.pop();
        CHECK("resume after a gap: it starts from the newest 'target' frames, the older ones are dropped (no 200 ms of extra delay)",
              p.kind == Kind::PLAY && p.seq == 10 && b.depth() == 3 && b.stats().overflow_drop == 6);
        Buffer c;
        put(c, 1); put(c, 2); put(c, 3);
        run_dry(c);
        for (uint16_t s = 4; s <= 8; ++s) put(c, s);            // 5개: 목표(4)보다 하나 많다 — 한 프레임 여유는 둔다
        CHECK("resume after a gap: one frame over the target is kept", c.pop().seq == 4 && c.stats().overflow_drop == 0);
    }

    // ── 꺼내는 쪽이 느려 쌓이기만 할 때: 300ms(15프레임)를 넘으면 오래된 것부터 버린다 ──
    {
        Buffer b;
        for (uint16_t s = 0; s < 15; ++s) put(b, s);
        CHECK("15 frames = 300ms is still allowed", b.depth() == 15 && b.stats().overflow_drop == 0);
        put(b, 15);
        CHECK("16th frame: cut back to target", b.depth() == b.target_frames() && b.stats().overflow_drop == (uint32_t)(16 - b.target_frames()));
        CHECK("oldest were dropped, newest kept", is(b.pop(), (uint16_t)(16 - 3)));
    }

    // ── 목표 깊이 조절: 끊김마다 +1(최대 200ms), 오래 무사하면 -1(최소 40ms) ──
    {
        Buffer b;
        uint16_t s = 0;
        for (int round = 0; round < 12; ++round) {              // 끊김 12번
            while (b.depth() < b.target_frames()) put(b, s++);
            run_dry(b);            // 다 꺼냄 → 은닉 3번 → 무음(끊김 1회)
        }
        CHECK("target capped at 200ms", b.target_frames() == MAX_FRAMES && b.stats().underruns == 12);
    }
    {
        Buffer b;
        uint16_t s = 0;
        for (int i = 0; i < 3; ++i) put(b, s++);
        int silence = 0;
        for (int i = 0; i < CALM_POPS * 3; ++i) { put(b, s++); if (b.pop().kind != Kind::PLAY) ++silence; }   // 한 프레임 넣고 한 프레임 꺼내기를 오래 반복
        CHECK("steady stream: never silent or concealed", silence == 0 && b.stats().lost == 0 && b.stats().underruns == 0);
        CHECK("steady stream: target shrinks to 40ms", b.target_frames() == MIN_FRAMES);
        CHECK("steady stream: depth follows the target down", b.depth() <= MIN_FRAMES + 1);
    }

    // ── 틀고 있는 중에 프레임이 몰려와 깊어지면(300ms 이하), 1초 지켜본 뒤 목표 깊이로 줄인다 ──
    {
        Buffer b;
        uint16_t s = 0;
        for (int i = 0; i < 3; ++i) put(b, s++);
        b.pop();                                                // 재생 시작
        for (int i = 0; i < 8; ++i) put(b, s++);                // 몰려옴: 깊이 10 (200ms)
        CHECK("burst while playing: kept for now", b.depth() == 10 && b.stats().catchup_drop == 0);
        int not_play = 0;
        for (int i = 0; i < CATCHUP_POPS - 1; ++i) { put(b, s++); if (b.pop().kind != Kind::PLAY) ++not_play; }
        CHECK("burst while playing: not trimmed before 1s", b.stats().catchup_drop == 0 && b.depth() == 10);
        for (int i = 0; i < 5; ++i) { put(b, s++); if (b.pop().kind != Kind::PLAY) ++not_play; }
        CHECK("burst while playing: trimmed back near the target after 1s", b.depth() <= b.target_frames() + 1 && b.stats().catchup_drop >= 6);
        CHECK("burst while playing: playback never paused", not_play == 0);
    }

    // ── 순번이 65535 → 0 으로 넘어가도 이어진다 ─────────────────────────────
    {
        Buffer b;
        put(b, 65534); put(b, 65535); put(b, 0); put(b, 1);
        CHECK("wrap: 65534, 65535, 0, 1", is(b.pop(), 65534) && is(b.pop(), 65535) && is(b.pop(), 0) && is(b.pop(), 1));
        CHECK("wrap: nothing lost", b.stats().lost == 0 && b.stats().late == 0);
    }

    // ── 순번이 크게 튄다(상대가 재부팅·새 통화): 버퍼를 비우고 새 순번에서 다시 시작 ──
    {
        Buffer b;
        put(b, 500); put(b, 501); put(b, 502);
        b.pop();
        put(b, 9000);
        CHECK("jump: resync counted, old frames discarded", b.stats().resync == 1 && b.depth() == 1);
        put(b, 9001); put(b, 9002);
        Pop p = b.pop();
        CHECK("jump: plays from the new sequence", p.kind == Kind::PLAY && p.data[0] == (uint8_t)9000);
    }
    {   // 뒤로 크게 튀는 것도 같은 처리(늦은 프레임으로 영원히 버리지 않는다)
        Buffer b;
        put(b, 9000); put(b, 9001); put(b, 9002);
        b.pop();
        for (uint16_t s = 10; s < 14; ++s) put(b, s);
        CHECK("backward jump: resync instead of dropping forever", b.stats().resync == 1 && b.depth() == 4);
    }

    // ── 규격 밖 프레임 ──────────────────────────────────────────────────────
    {
        Buffer b;
        uint8_t d[MAX_PAYLOAD + 8] = {0};
        b.push(1, d, 0);
        b.push(2, d, MAX_PAYLOAD + 1);
        b.push(3, nullptr, 10);
        CHECK("empty / oversized / null payload rejected", b.depth() == 0 && b.stats().bad == 3);
        b.push(4, d, MAX_PAYLOAD);
        CHECK("largest payload (398) accepted", b.depth() == 1 && MAX_PAYLOAD == 398);
    }

    // ── 새 통화: reset 하면 처음 상태 ───────────────────────────────────────
    {
        Buffer b;
        put(b, 1); put(b, 2); put(b, 3); b.pop();
        b.reset();
        CHECK("reset: empty, silence, stats cleared, target back to start", b.depth() == 0 && b.pop().kind == Kind::SILENCE && b.stats().played == 0 && b.target_frames() == START_FRAMES);
        put(b, 700); put(b, 701); put(b, 702);
        CHECK("reset: accepts any new starting sequence", is(b.pop(), 700));
    }

    // ════ 꺼낸 프레임의 순번, 채우는 중인지 ═══════════════════════════════════
    {
        Buffer b;
        CHECK("buffering at first", b.buffering());
        put(b, 40); put(b, 41); put(b, 42);
        Pop p = b.pop();
        CHECK("a played frame carries its sequence number", p.kind == Kind::PLAY && p.seq == 40 && !b.buffering());
        CHECK("next ones too", b.pop().seq == 41 && b.pop().seq == 42);
        b.pop(); b.pop(); b.pop();
        CHECK("still not buffering while concealing", !b.buffering());
        b.pop();
        CHECK("buffering again once it gave up concealing", b.buffering());
        for (uint16_t s = 46; s < 46 + 16; ++s) put(b, s);          // 16개: 넘쳐서 오래된 것을 버린다
        p = b.pop();
        CHECK("after dropping old frames the sequence jumps (the caller can see the gap)", p.kind == Kind::PLAY && p.seq == (uint16_t)(46 + 16 - b.target_frames()));
    }

    // ════ 오래·거칠게 (Review Focus 3) ════════════════════════════════════════
    // 기기의 박자: 보내는 쪽은 16ms 묶음을 모아 20ms 프레임이 찰 때마다 한 개(간격 16·16·16·32ms), 받는 쪽은 16ms 마다 한 묶음을 가져가고
    // 링버퍼가 한 묶음보다 적어지면 pop 한다(역시 16·16·16·32ms). 아래 Beat 가 그 박자를 센다.
    struct Beat { int acc = 0; int tick() { acc += 256; int n = 0; while (acc >= 320) { acc -= 320; ++n; } return n; } };
    struct Pull { int ring = 0; int tick() { ring -= 256; int n = 0; while (ring < 256) { ring += 320; ++n; } return n; } };

    {   // 망이 고르면(같은 박자, 어긋난 위상 5가지) 한 시간 동안 한 번도 끊기지 않는다
        bool clean = true; int worst_depth = 0;
        for (int phase = 0; phase < 5; ++phase) {
            Buffer b; Beat tx; Pull rx; uint16_t s = 0; long plays = 0;
            for (int i = 0; i < phase; ++i) tx.tick();
            for (int t = 0; t < 225000; ++t) {                      // 16ms × 225000 = 1시간
                for (int n = tx.tick(); n > 0; --n) put(b, s++);
                for (int n = rx.tick(); n > 0; --n) { const Pop p = b.pop(); if (p.kind == Kind::PLAY) ++plays; }
                if (b.depth() > worst_depth) worst_depth = b.depth();
            }
            if (b.stats().lost || b.stats().underruns || b.stats().overflow_drop || b.stats().catchup_drop || plays < 179990) clean = false;
        }
        CHECK("steady 16/16/16/32 ms beat on both sides, 1 hour x 5 phases: never concealed, never dropped", clean);
        CHECK("... and it never holds more than 4 frames (80 ms)", worst_depth <= 4);
    }
    {   // 순번이 65535 를 여러 번 넘어가는 긴 통화(2시간)에서 내용이 그대로 나온다
        Buffer b; Beat tx; Pull rx; uint16_t s = 65000; uint32_t sent = 0, got = 0; bool same = true;
        for (int t = 0; t < 450000; ++t) {
            for (int n = tx.tick(); n > 0; --n) { uint8_t d[60]; memset(d, (uint8_t)(sent * 7), sizeof d); b.push(s++, d, sizeof d); ++sent; }
            for (int n = rx.tick(); n > 0; --n) { const Pop p = b.pop(); if (p.kind == Kind::PLAY) { if (p.data[0] != (uint8_t)(got * 7) || p.len != 60) same = false; ++got; } }
        }
        CHECK("2-hour call (sequence wraps 5 times): every frame comes out once, in order, unchanged", same && sent == 360000 && got >= sent - 4 && b.stats().lost == 0);
    }
    {   // 보내는 쪽 시계가 0.1% 빠르다(실제 수정 발진기 오차의 10배 이상): 쌓이지 않고 가끔 한 프레임씩 버려 따라간다
        Buffer b; Beat tx; Pull rx; uint16_t s = 0; int worst = 0;
        for (int t = 0; t < 450000; ++t) {
            for (int n = tx.tick() + (t % 1250 == 0 ? 1 : 0); n > 0; --n) put(b, s++);     // 20초(1000프레임)마다 한 프레임 더
            for (int n = rx.tick(); n > 0; --n) b.pop();
            if (b.depth() > worst) worst = b.depth();
        }
        CHECK("sender clock 0.1% fast for 2 hours: depth stays bounded (delay does not pile up)", worst <= 6 && b.depth() <= 4);
        CHECK("... by dropping a frame now and then, never by going silent", b.stats().catchup_drop >= 100 && b.stats().underruns == 0);
    }
    {   // 받는 쪽 시계가 0.1% 빠르다: 20초마다 한 번 다 떨어진다 — 한 프레임만 메우고 이어 간다(JB-D2. 초안은 여기서 끝없이 끊겼다)
        Buffer b; Beat tx; Pull rx; uint16_t s = 0; int worst_target = 0; long plays = 0, silent = 0;
        for (int t = 0; t < 450000; ++t) {
            for (int n = tx.tick(); n > 0; --n) put(b, s++);
            for (int n = rx.tick() + (t % 1250 == 0 ? 1 : 0); n > 0; --n) {                       // 20초(1000프레임)마다 한 번 더 꺼낸다
                const Pop p = b.pop();
                if (p.kind == Kind::PLAY) ++plays; else if (p.kind == Kind::SILENCE && t > 10) ++silent;
            }
            if (b.target_frames() > worst_target) worst_target = b.target_frames();
        }
        CHECK("receiver clock 0.1% fast for 2 hours: one concealed frame per slip, never a run of them", b.stats().lost >= 300 && b.stats().lost <= 400 && b.stats().underruns == b.stats().lost);
        CHECK("... no frame is thrown away for it, playback never goes silent, the target does not creep up",
              b.stats().late == 0 && silent == 0 && plays >= 359990 && worst_target <= 4 && b.target_frames() <= 3);
    }
    {   // 한 프레임이 통째로 사라진다(20초마다 — UDP 계열): 뒤 프레임이 와 있으므로 그 순번만 메우고 넘어간다
        Buffer b; Beat tx; Pull rx; uint32_t sent = 0; long plays = 0;
        for (int t = 0; t < 450000; ++t) {
            for (int n = tx.tick(); n > 0; --n) { if (sent % 1000 != 999) put(b, (uint16_t)sent); ++sent; }
            for (int n = rx.tick(); n > 0; --n) if (b.pop().kind == Kind::PLAY) ++plays;
        }
        CHECK("a frame missing every 20 s for 2 hours: each gap is concealed once and skipped",
              b.stats().lost == 359 && b.stats().underruns == 0 && b.stats().late == 0 && plays >= 359630);   // 360번째 빈자리는 맨 끝 프레임이라 아직 차례가 안 왔다
    }
    {   // 망이 멈췄다 풀린다(TCP: 잃지 않고 한꺼번에 온다). 멈춘 길이별로 — 풀린 뒤 1.5초 안에 깊이가 목표 근처로 돌아온다
        bool recovered = true, bounded = true; int worst = 0;
        for (int stall_ms : {40, 100, 200, 300, 500, 1000, 3000, 10000}) {
            Buffer b; Beat tx; Pull rx; uint16_t s = 0; std::vector<uint16_t> held;
            for (int t = 0; t < 2000; ++t) {                        // 32초
                const bool stalled = t >= 500 && t < 500 + stall_ms / 16;
                for (int n = tx.tick(); n > 0; --n) held.push_back(s++);
                if (!stalled) { for (uint16_t q : held) put(b, q); held.clear(); }
                for (int n = rx.tick(); n > 0; --n) b.pop();
                if (b.depth() > worst) worst = b.depth();
                if (t == 500 + stall_ms / 16 + 94 && b.depth() > b.target_frames() + 1) recovered = false;   // 풀리고 1.5초 뒤
            }
            if (b.depth() > b.target_frames() + 1) recovered = false;
            if (worst > OVERFLOW_FRAMES) bounded = false;
        }
        CHECK("stall then burst (40 ms .. 10 s): back near the target within 1.5 s — the delay never piles up", recovered);
        CHECK("... and it never holds more than 300 ms", bounded && worst <= OVERFLOW_FRAMES);
    }
    {   // 손실·중복·순서 뒤바뀜이 섞인 망(UDP 계열로 바꿔도 — JB-D1): 꺼낸 것은 늘 순번이 커지는 쪽이고, 넣은 내용 그대로다
        uint32_t seed = 20261004; auto rnd = [&seed](uint32_t m) { seed = seed * 1664525u + 1013904223u; return (seed >> 8) % m; };
        bool forward = true, intact = true, bounded = true; long plays = 0, losts = 0;
        for (int loss_pct : {0, 1, 5, 20, 50}) {
            Buffer b; Beat tx; Pull rx; uint16_t s = 0; bool have = false; uint16_t last = 0; std::vector<uint16_t> late;
            for (int t = 0; t < 60000; ++t) {                       // 16분
                for (int n = tx.tick(); n > 0; --n) {
                    const uint16_t q = s++;
                    if ((int)rnd(100) < loss_pct) continue;         // 잃음
                    if (rnd(20) == 0) { late.push_back(q); continue; }   // 늦게(순서 뒤바뀜)
                    uint8_t d[60]; memset(d, (uint8_t)q, sizeof d); b.push(q, d, 1 + q % 60);
                    if (rnd(30) == 0) b.push(q, d, 1 + q % 60);     // 중복
                }
                if (!late.empty() && rnd(3) == 0) { const uint16_t q = late.front(); late.erase(late.begin()); uint8_t d[60]; memset(d, (uint8_t)q, sizeof d); b.push(q, d, 1 + q % 60); }
                for (int n = rx.tick(); n > 0; --n) {
                    const Pop p = b.pop();
                    if (p.kind == Kind::LOST) ++losts;
                    if (p.kind != Kind::PLAY) continue;
                    ++plays;
                    if (have && (int16_t)(uint16_t)(p.seq - last) <= 0) forward = false;
                    if (p.len != (size_t)(1 + p.seq % 60) || p.data[0] != (uint8_t)p.seq || p.data[p.len - 1] != (uint8_t)p.seq) intact = false;
                    have = true; last = p.seq;
                }
                if (b.depth() > OVERFLOW_FRAMES || b.depth() < 0 || b.target_frames() < MIN_FRAMES || b.target_frames() > MAX_FRAMES) bounded = false;
            }
        }
        CHECK("loss 0-50% + duplicates + reordering, 16 min each: played sequence numbers only move forward (nothing twice)", forward && plays > 100000 && losts > 1000);
        CHECK("... every played frame is byte-for-byte what was pushed for that number", intact);
        CHECK("... depth 0..15 and target 2..10 at every step", bounded);
    }
    {   // 아무렇게나: 무작위 순번·길이·reset 을 500만 번 — 죽지 않고, 안의 셈이 맞는다
        uint32_t seed = 99; auto rnd = [&seed](uint32_t m) { seed = seed * 1664525u + 1013904223u; return (seed >> 8) % m; };
        Buffer b; std::map<uint16_t, uint8_t> content; bool sane = true; uint16_t base = 0; uint8_t d[MAX_PAYLOAD + 4];
        for (int i = 0; i < 5000000 && sane; ++i) {
            const uint32_t op = rnd(100);
            if (op < 55) {
                const uint16_t q = (uint16_t)(rnd(50) == 0 ? rnd(65536) : base + rnd(40) - 4);
                const size_t len = rnd(40) == 0 ? rnd(MAX_PAYLOAD + 4) : 1 + rnd(120);
                const uint8_t fill = (uint8_t)rnd(256);
                memset(d, fill, sizeof d);
                const uint32_t before = b.stats().dup + b.stats().late + b.stats().bad;
                b.push(q, d, len);
                if (b.stats().dup + b.stats().late + b.stats().bad == before) content[q] = fill;   // 받아들여졌다
                if (rnd(8) == 0) ++base;
            } else if (op < 99) {
                const Pop p = b.pop();
                if (p.kind == Kind::PLAY) {
                    auto it = content.find(p.seq);
                    if (it == content.end() || p.len == 0 || p.len > MAX_PAYLOAD || p.data[0] != it->second || p.data[p.len - 1] != it->second) sane = false;
                    base = (uint16_t)(p.seq + 1);
                } else if (p.len != 0) sane = false;
            } else if (rnd(50) == 0) { b.reset(); content.clear(); }
            if (b.depth() < 0 || b.depth() > OVERFLOW_FRAMES || b.target_frames() < MIN_FRAMES || b.target_frames() > MAX_FRAMES) sane = false;
        }
        CHECK("5 million random pushes / pops / resets: never out of bounds, every played frame is one that was accepted", sane);
    }


    // ════ Phase 3c (JB-D3, 계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D5): 깊이를 정하는 규칙(Policy) ════
    // 틀고 있는 상태로 만든다: 목표만큼 채우고 한 번 꺼낸다. 돌려주는 값 = 다음에 넣을 순번
    auto prime = [](Buffer &b, uint16_t from) { uint16_t q = from; while (b.depth() < b.target_frames()) put(b, q++); b.pop(); return q; };
    // 한 프레임 넣고 한 프레임 꺼내기를 n 번(끊김 없이 흐른다)
    auto steady = [](Buffer &b, uint16_t &q, int n) { for (int i = 0; i < n; ++i) { put(b, q++); b.pop(); } };
    // 다 떨어지게 한 뒤 다시 채워 튼다(끊김 한 번). 돌려주는 값 = 그때의 목표
    auto underrun = [](Buffer &b, uint16_t &q) { while (b.pop().kind != Kind::SILENCE) {} while (b.buffering()) { put(b, q++); b.pop(); } return b.target_frames(); };
    {
        const Policy l = legacy_policy();
        CHECK("policy: the default is the 3b rule (ceiling 200 ms, 10 s calm, 1 s then all at once, no resume trim, no burst rule)",
              l.max_frames == MAX_FRAMES && l.calm_pops == CALM_POPS && l.catchup_pops == CATCHUP_POPS && l.catchup_step == 0 && l.resume_pops == 0 && l.burst_pops == 0);
        Buffer b;
        CHECK("policy: a new buffer starts with it", b.policy().max_frames == 10 && b.policy().calm_pops == 500 && b.policy().catchup_step == 0);
        const Policy t = tuned_policy(4);
        CHECK("policy: tuned(4) = ceiling 80 ms, 5 s calm, 0.5 s then one frame per 0.1 s, trim a burst within 0.5 s of resuming, burst rule 1 s",
              t.max_frames == 4 && t.calm_pops == 250 && t.catchup_pops == 25 && t.catchup_step == 5 && t.resume_pops == 25 && t.burst_pops == 50);
        CHECK("policy: tuned() clamps the ceiling to 3..10 frames (the call still starts at 60 ms)", tuned_policy(1).max_frames == 3 && tuned_policy(3).max_frames == 3 && tuned_policy(10).max_frames == 10 && tuned_policy(99).max_frames == 10 && tuned_policy(-5).max_frames == 3);
    }
    {
        // 기본 규칙을 다시 걸어도 3b 와 글자 그대로 같다: 같은 무작위 입력 200만 번에 두 버퍼가 같은 것을 낸다
        uint32_t seed = 4242; auto rnd = [&seed](uint32_t m) { seed = seed * 1664525u + 1013904223u; return (seed >> 8) % m; };
        Buffer a, b; b.set_policy(legacy_policy()); bool same = true; uint16_t base = 0; uint8_t d[MAX_PAYLOAD];
        for (int i = 0; i < 2000000 && same; ++i) {
            if (rnd(100) < 52) {
                const uint16_t q = (uint16_t)(rnd(200) == 0 ? rnd(65536) : base + rnd(30) - 3); const size_t len = 1 + rnd(100);
                memset(d, (uint8_t)q, sizeof d); a.push(q, d, len); b.push(q, d, len);
                if (rnd(6) == 0) ++base;
            } else {
                const Pop x = a.pop(), y = b.pop();
                if (x.kind != y.kind || x.seq != y.seq || x.len != y.len) same = false;
                if (x.kind == Kind::PLAY) base = (uint16_t)(x.seq + 1);
            }
            if (a.depth() != b.depth() || a.target_frames() != b.target_frames() || a.buffering() != b.buffering()) same = false;
        }
        const Stats &x = a.stats(), &y = b.stats();
        CHECK("policy: setting the legacy rule explicitly changes nothing — 2 million random pushes / pops give identical output and counters",
              same && x.played == y.played && x.lost == y.lost && x.overflow_drop == y.overflow_drop && x.catchup_drop == y.catchup_drop && x.underruns == y.underruns && x.late == y.late && x.played > 100000);
    }
    {
        // ① 상한: 끊김이 드문드문(2초보다 뜸하게) 오면 목표는 상한에서 멈춘다. 3b 는 200ms 까지 올라간다
        Buffer t, l; t.set_policy(tuned_policy(4));
        uint16_t qt = prime(t, 0), ql = prime(l, 0); int top_t = 0, top_l = 0;
        for (int i = 0; i < 12; ++i) { steady(t, qt, 150); steady(l, ql, 150); top_t = underrun(t, qt); top_l = underrun(l, ql); }     // 3초마다 한 번 끊긴다
        CHECK("ceiling: underruns 3 s apart push the 3b rule to its 200 ms ceiling, the tuned rule stops at its own (80 ms)", top_l == 10 && top_t == 4 && t.stats().underruns == 12);
        // ② 5초 무사하면 한 프레임 내려온다(3b 는 10초)
        steady(t, qt, 245); const int before = t.target_frames(); steady(t, qt, 10);
        CHECK("calm: 5 s (250 frames) without an underrun brings the target down one frame; the 3b rule needs 10 s", before == 4 && t.target_frames() == 3);
        steady(l, ql, 260);
        CHECK("... (3b after the same 5 s: still at 200 ms)", l.target_frames() == 10);
        steady(t, qt, 600);
        CHECK("calm: it never goes below 40 ms", t.target_frames() == MIN_FRAMES);
    }
    {
        // 끊김이 1초 안에 세 번 거듭된다(0.3초마다 뭉쳐 오는 망 — 절전이 다시 켜진 경우): 그동안은 3b 처럼 200ms 까지 올라간다
        Buffer t; t.set_policy(tuned_policy(4)); uint16_t q = prime(t, 0); int top = 0;
        steady(t, q, 10); underrun(t, q); steady(t, q, 10); const int second = underrun(t, q);      // 통화 시작에 흔한 것: 끊김 두 번이 붙어 온다
        CHECK("burst rule: two underruns close together (common right after a call opens) do not pass the knob", second == 4);
        for (int i = 0; i < 12; ++i) { steady(t, q, 10); top = underrun(t, q); }                    // 0.2초마다 계속 끊긴다
        CHECK("burst rule: a third and further underruns within 1 s of each other may climb past the knob to the 3b ceiling (a low ceiling would throw most of the audio away)", top == 10);
        steady(t, q, 60); const int hold = underrun(t, q);
        CHECK("burst rule: once underruns are more than 1 s apart again, the knob's ceiling is back (no further rise)", hold == 10 && t.target_frames() == 10);
        steady(t, q, 6 * 250 + 10);
        CHECK("burst rule: ... and 5 s of calm at a time brings it back down under the knob", t.target_frames() <= 4);
    }
    {
        // 거듭 끊기는 동안(마지막 끊김 뒤 1초)에는 깊어져도 3b 처럼 기다린다 — 0.5초 만에 버리기 시작하지 않는다. 끊김이 뜸해지면 새 규칙으로 돌아온다
        Buffer t; t.set_policy(tuned_policy(4)); uint16_t q = prime(t, 0);
        for (int i = 0; i < 6; ++i) { steady(t, q, 10); underrun(t, q); }                           // 0.2초마다 끊긴다 → 거듭 끊기는 망
        for (int i = 0; i < 4; ++i) put(t, q++);                                                    // 뭉쳐 온 것(다음 0.3초치)
        const uint32_t before = t.stats().catchup_drop + t.stats().overflow_drop;
        steady(t, q, 40);
        CHECK("burst rule: while underruns keep coming, a deep buffer is left alone for 0.8 s (that surplus is the next clump's supply)", t.stats().catchup_drop + t.stats().overflow_drop == before);
        steady(t, q, 300);                                                                          // 6초 무사 → 거듭 끊김이 풀렸다
        for (int i = 0; i < 4; ++i) put(t, q++);                                                    // 목표보다 3프레임 깊다(300ms 는 넘지 않는다)
        uint16_t last = 0; bool have = false; int big = 0;
        for (int i = 0; i < 80; ++i) { put(t, q++); const Pop p = t.pop(); if (have) big = std::max(big, (int)(uint16_t)(p.seq - last)); last = p.seq; have = true; }
        CHECK("burst rule: once the underruns have stopped, the tuned catch-up is back (one frame at a time, not the whole surplus in one jump)", big == 2 && t.depth() - t.target_frames() < 2);
    }
    {
        // ③ 깊어졌을 때: 3b 는 1초 뒤 한꺼번에(큰 건너뜀 한 번), 새 규칙은 0.5초 지켜본 뒤 0.1초마다 한 프레임
        Buffer t, l; t.set_policy(tuned_policy(4));
        uint16_t qt = prime(t, 0), ql = prime(l, 0);
        steady(t, qt, 30); steady(l, ql, 30);                                                        // 다시 튼 지 0.6초 — "다시 틀기 직후"가 아니다
        for (int i = 0; i < 9; ++i) { put(t, qt++); put(l, ql++); }                                  // 9프레임이 몰려왔다(180ms 멈췄다 풀림 — 끊기지는 않았다)
        const int deep_t = t.depth(), deep_l = l.depth();
        std::vector<int> jumps_t, jumps_l; uint16_t last_t = 0, last_l = 0; bool have = false; int back_t = -1, back_l = -1;
        for (int i = 0; i < 80; ++i) {
            put(t, qt++); put(l, ql++);
            const Pop a = t.pop(), b = l.pop();
            if (have) { jumps_t.push_back((uint16_t)(a.seq - last_t)); jumps_l.push_back((uint16_t)(b.seq - last_l)); }
            last_t = a.seq; last_l = b.seq; have = true;
            if (back_t < 0 && t.depth() - t.target_frames() < 2) back_t = i;
            if (back_l < 0 && l.depth() - l.target_frames() < 2) back_l = i;
        }
        const int big_t = *std::max_element(jumps_t.begin(), jumps_t.end()), big_l = *std::max_element(jumps_l.begin(), jumps_l.end());
        CHECK("catch-up: both rules start 8 frames (160 ms) too deep", deep_t == deep_l && deep_t - 3 >= 8);
        CHECK("catch-up: the 3b rule plays 1 s late, then skips it all in one jump (8 frames = 160 ms of speech gone at once)", big_l >= 8 && back_l >= 49 && l.stats().catchup_drop >= 7);
        CHECK("catch-up: the tuned rule waits 0.5 s, then drops one frame every 0.1 s — never more than one frame (20 ms) at a time", big_t == 2 && t.stats().catchup_drop >= 6 && back_t >= 24);
        CHECK("catch-up: ... and is back near its target within 1.3 s", back_t >= 0 && back_t <= 65);
    }
    {
        // 다시 틀기 시작한 직후 몰려온 꼬리: 새 규칙은 기다리지 않고 버린다(이미 그만큼 늦은 소리), 3b 는 1초 동안 늦게 튼다
        Buffer t, l; t.set_policy(tuned_policy(4));
        uint16_t qt = prime(t, 0), ql = prime(l, 0);
        steady(t, qt, 200); steady(l, ql, 200);
        while (t.pop().kind != Kind::SILENCE) {} while (l.pop().kind != Kind::SILENCE) {}           // 망이 멈췄다 — 둘 다 다 떨어져 무음
        for (int i = 0; i < 4; ++i) { put(t, qt++); put(l, ql++); }                                  // 풀렸다: 먼저 네 프레임
        t.pop(); l.pop();                                                                            // 다시 틀기 시작
        for (int i = 0; i < 9; ++i) { put(t, qt++); put(l, ql++); }                                  // 꼬리 아홉 프레임이 곧바로 따라온다
        const uint32_t ovf_t = t.stats().overflow_drop, ovf_l = l.stats().overflow_drop;
        put(t, qt++); put(l, ql++); t.pop(); l.pop();
        CHECK("resume: a burst that lands right after playback restarts is trimmed at once by the tuned rule (counted with the restart drops)",
              t.depth() <= t.target_frames() && t.stats().overflow_drop > ovf_t && t.stats().catchup_drop == 0);
        CHECK("resume: ... the 3b rule keeps playing it 180 ms late for a second", l.depth() - l.target_frames() >= 8 && l.stats().overflow_drop == ovf_l);
    }
    {
        // 통화 중에 규칙을 바꾼다
        Buffer b; uint16_t q = prime(b, 0);
        for (int i = 0; i < 9; ++i) { steady(b, q, 150); underrun(b, q); }
        const int high = b.target_frames();
        b.set_policy(tuned_policy(5));
        CHECK("set_policy during a call: a target above the new ceiling comes down to it at once", high == 10 && b.target_frames() == 5);
        bool in_order = true; uint16_t last = 0; bool have = false;
        for (int i = 0; i < 200; ++i) { put(b, q++); const Pop p = b.pop(); if (p.kind == Kind::PLAY) { if (have && (int16_t)(p.seq - last) <= 0) in_order = false; last = p.seq; have = true; } }
        CHECK("set_policy during a call: playback carries on in order, the surplus is shed by the new rule", in_order && b.depth() - b.target_frames() < 2);
        b.reset();
        CHECK("set_policy: the rule survives reset() (a new call keeps the knob)", b.policy().max_frames == 5 && b.policy().catchup_step == 5 && b.target_frames() == START_FRAMES);
        Policy odd = {1, 0, 0, 99, -3, -1};
        b.set_policy(odd);
        CHECK("set_policy: nonsense values are clamped (ceiling >= 40 ms, counts >= 1, step <= wait)", b.policy().max_frames == MIN_FRAMES && b.policy().calm_pops == 1 && b.policy().catchup_pops == 1 && b.policy().catchup_step == 1 && b.policy().resume_pops == 0 && b.policy().burst_pops == 0);
        b.set_policy(legacy_policy());
        CHECK("set_policy: and back to the 3b rule", b.policy().max_frames == MAX_FRAMES && b.policy().catchup_step == 0);
    }
    {
        // 아무렇게나(새 규칙, 상한 3~10): 죽지 않고, 셈이 맞고, 꺼낸 프레임은 전부 받아들인 것
        uint32_t seed = 31337; auto rnd = [&seed](uint32_t m) { seed = seed * 1664525u + 1013904223u; return (seed >> 8) % m; };
        Buffer b; b.set_policy(tuned_policy(4)); std::map<uint16_t, uint8_t> content; bool sane = true; uint16_t base = 0; uint8_t d[MAX_PAYLOAD + 4];
        for (int i = 0; i < 3000000 && sane; ++i) {
            const uint32_t op = rnd(1000);
            if (op < 540) {
                const uint16_t q = (uint16_t)(rnd(50) == 0 ? rnd(65536) : base + rnd(40) - 4);
                const size_t len = rnd(40) == 0 ? rnd(MAX_PAYLOAD + 4) : 1 + rnd(120);
                const uint8_t fill = (uint8_t)rnd(256);
                memset(d, fill, sizeof d);
                const uint32_t before = b.stats().dup + b.stats().late + b.stats().bad;
                b.push(q, d, len);
                if (b.stats().dup + b.stats().late + b.stats().bad == before) content[q] = fill;
                if (rnd(8) == 0) ++base;
            } else if (op < 990) {
                const Pop p = b.pop();
                if (p.kind == Kind::PLAY) {
                    auto it = content.find(p.seq);
                    if (it == content.end() || p.len == 0 || p.len > MAX_PAYLOAD || p.data[0] != it->second || p.data[p.len - 1] != it->second) sane = false;
                    base = (uint16_t)(p.seq + 1);
                } else if (p.len != 0) sane = false;
            } else if (op < 995) b.set_policy(rnd(4) == 0 ? legacy_policy() : tuned_policy(3 + (int)rnd(8)));
            else if (rnd(50) == 0) { b.reset(); content.clear(); }
            if (b.depth() < 0 || b.depth() > OVERFLOW_FRAMES || b.target_frames() < MIN_FRAMES || b.target_frames() > MAX_FRAMES) sane = false;
        }
        CHECK("3 million random pushes / pops / rule changes / resets: never out of bounds, every played frame is one that was accepted", sane);
    }

    printf("\n%d ok, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
