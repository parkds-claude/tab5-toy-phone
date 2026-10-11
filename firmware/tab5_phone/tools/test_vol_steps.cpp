// test_vol_steps.cpp — vol_steps.h 단위 시험(화면의 "작게 / 크게" — 단계 ↔ 코덱 음량, 올림·내림, 저장 값 검증, 저장 미루기, 시리얼 명령 읽기):
//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_vol_steps.cpp -o /tmp/t_vol_steps && /tmp/t_vol_steps
// 코덱 곡선은 부품 소스(managed_components/espressif__esp_codec_dev)의 식을 그대로 옮겨 견준다 — 줄 번호는 ref_reg 의 주석.
#include "vol_steps.h"
#include "voice_tune.h"

#include <cmath>
#include <cstdio>
#include <string>
using namespace volsteps;
static int ok = 0, bad = 0;
#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)

// 부품이 음량(%) 하나를 ES8388 의 DAC 음량 레지스터 값으로 바꾸는 길을 그대로 옮긴 것(0 = 0dB, 한 칸 = -0.5dB, 0xC0 = -96dB):
//   esp_codec_dev.c:79-90   기본 곡선 = 0 → -50dB, 100 → 0dB (직선, 1% = 0.5dB)
//   esp_codec_dev.c:92-115  _get_vol_db: 0 이면 -96dB, 100 이상이면 0dB, 그 사이는 직선
//   es8388.c:462            hw_gain = 20·log10(DAC 3.3V / 앰프 5.0V) + 앰프 이득 0 = -3.61dB (우리 설정 — audio_out.cpp codec_open)
//   es8388.c:352            db_value -= hw_gain  → 3.61dB 를 더한다(그래서 93~100 은 전부 0dB 로 잘린다)
//   esp_codec_dev_vol.c:9-23 esp_codec_dev_vol_calc_reg: 범위(es8388.c:28-40 — 0xC0 = -96dB, 0 = 0dB)에 직선으로 놓고 (int) 로 자른다
static int ref_reg(int vol)
{
    float db;
    if (vol == 0) db = -96.0f;
    else if (vol >= 100) db = 0.0f;
    else db = -50.0f + (float)vol * ((0.0f - -50.0f) / (100 - 0));
    const float hw_gain = (float)(20 * log10(3.3f / 5.0f) + 0.0f);
    db -= hw_gain;
    if (db >= 0.0f) return 0;
    if (db <= -96.0f) return 0xC0;
    const float ratio = (0 - 0xC0) / (0.0f - -96.0f);
    return (int)((db - -96.0f) * ratio + 0xC0);
}

int main()
{
    // ── 단계 표 ─────────────────────────────────────────────────────────────
    CHECK("five steps, default = top", COUNT == 5 && STEP_MIN == 1 && STEP_MAX == 5 && STEP_DEFAULT == STEP_MAX);
    CHECK("top step = 100 = today's boot volume (never louder than before)", vol_of(STEP_MAX) == 100 && vol_of(STEP_MAX) == voicetune::DEFAULT_VOL && voicetune::VOL_MAX == 100);
    {
        bool rising = true, in_range = true;
        for (int s = STEP_MIN; s <= STEP_MAX; ++s) in_range = in_range && vol_of(s) >= 1 && vol_of(s) <= 100;
        for (int s = STEP_MIN; s < STEP_MAX; ++s) rising = rising && vol_of(s) < vol_of(s + 1);
        CHECK("every step is inside 1..100 and louder than the one below", rising && in_range);
    }
    CHECK("bottom step is not mute: at most 20 dB below the top (the bell must stay audible)", vol_of(STEP_MIN) > 0 && codec_reg(vol_of(STEP_MIN)) <= 40);
    CHECK("the table itself", vol_of(1) == 52 && vol_of(2) == 62 && vol_of(3) == 72 && vol_of(4) == 82 && vol_of(5) == 100);

    // ── 코덱 곡선: 헤더의 정수 모형이 부품의 식과 0~100 전부에서 같다 ─────────
    {
        bool same = true; int first_bad = -1;
        for (int v = 0; v <= 100; ++v) if (codec_reg(v) != ref_reg(v)) { same = false; if (first_bad < 0) first_bad = v; }
        if (!same) printf("       codec_reg(%d)=%d, component formula=%d\n", first_bad, codec_reg(first_bad), ref_reg(first_bad));
        CHECK("codec_reg matches the component's formula for every volume 0..100", same);
        CHECK("the curve as measured before: 93..100 are all 0 dB, 88 = -2 dB, 74 = -9 dB", codec_reg(100) == 0 && codec_reg(93) == 0 && codec_reg(92) == 0 && codec_reg(88) == 4 && codec_reg(74) == 18);
    }
    {
        bool even = true;
        for (int s = STEP_MIN; s < STEP_MAX; ++s) even = even && codec_reg(vol_of(s)) - codec_reg(vol_of(s + 1)) == STEP_HALF_DB;
        CHECK("steps are evenly spaced in dB (5 dB each), not in percent", even && STEP_HALF_DB == 10 && codec_reg(vol_of(STEP_MAX)) == 0);
        CHECK("step_db: 0, -5, -10, -15, -20", step_half_db(5) == 0 && step_half_db(4) == -10 && step_half_db(3) == -20 && step_half_db(2) == -30 && step_half_db(1) == -40);
    }

    // ── 올림·내림·끝에서 멈춤 ───────────────────────────────────────────────
    CHECK("up: one step at a time, stops at the top", up(1) == 2 && up(2) == 3 && up(3) == 4 && up(4) == 5 && up(5) == 5);
    CHECK("down: one step at a time, stops at the bottom", down(5) == 4 && down(4) == 3 && down(3) == 2 && down(2) == 1 && down(1) == 1);
    CHECK("up/down on a broken value land inside the range", up(0) == 2 && up(99) == 5 && down(0) == 1 && down(99) == 4 && up(-7) == 2 && down(-7) == 1);
    CHECK("vol_of clamps a broken step instead of reading outside the table", vol_of(0) == vol_of(1) && vol_of(-3) == vol_of(1) && vol_of(6) == vol_of(5) && vol_of(1000) == 100);
    CHECK("ok_step", ok_step(1) && ok_step(5) && !ok_step(0) && !ok_step(6) && !ok_step(-1) && !ok_step(255));

    // ── 저장 값 검증: 범위 밖(지워진 플래시 0xFF, 0, 옛 판의 값)이면 기본 ─────
    CHECK("from_saved keeps 1..5", from_saved(1) == 1 && from_saved(2) == 2 && from_saved(3) == 3 && from_saved(4) == 4 && from_saved(5) == 5);
    CHECK("from_saved: anything else -> default (top)", from_saved(0) == STEP_DEFAULT && from_saved(6) == STEP_DEFAULT && from_saved(255) == STEP_DEFAULT && from_saved(-1) == STEP_DEFAULT && from_saved(100) == STEP_DEFAULT);
    CHECK("step_of_vol: exact table values only (0 = the diagnostic 'vol' command moved it)", step_of_vol(52) == 1 && step_of_vol(72) == 3 && step_of_vol(100) == 5 && step_of_vol(92) == 0 && step_of_vol(0) == 0 && step_of_vol(80) == 0);

    // ── 요청 하나 → 새 단계 (화면 버튼과 시리얼 명령이 같은 함수를 탄다) ──────
    CHECK("apply: up/down", apply(3, Req::UP) == 4 && apply(5, Req::UP) == 5 && apply(3, Req::DOWN) == 2 && apply(1, Req::DOWN) == 1);
    CHECK("apply: set inside the range", apply(3, Req::SET, 1) == 1 && apply(3, Req::SET, 5) == 5 && apply(1, Req::SET, 1) == 1);
    CHECK("apply: set outside the range keeps the step", apply(3, Req::SET, 0) == 3 && apply(3, Req::SET, 6) == 3 && apply(3, Req::SET, -1) == 3);
    CHECK("apply: show/none change nothing", apply(4, Req::SHOW) == 4 && apply(4, Req::NONE) == 4 && apply(4, Req::SHOW, 2) == 4);
    CHECK("req_valid: only the four known requests travel through the queue", req_valid((int)Req::SHOW) && req_valid((int)Req::DOWN) && req_valid((int)Req::UP) && req_valid((int)Req::SET)
          && !req_valid((int)Req::NONE) && !req_valid(-1) && !req_valid(5) && !req_valid(200));

    // ── 시리얼 명령 읽기 ────────────────────────────────────────────────────
    {
        const Cmd a = parse_cmd("volup"), b = parse_cmd("voldn"), c = parse_cmd("volstep"), d = parse_cmd("volstep 3");
        CHECK("cmd: volup / voldn / volstep / volstep 3", a.mine && a.ok && a.req == Req::UP && b.mine && b.ok && b.req == Req::DOWN
              && c.mine && c.ok && c.req == Req::SHOW && d.mine && d.ok && d.req == Req::SET && d.arg == 3);
        CHECK("cmd: volstep 1 and 5 are the ends", parse_cmd("volstep 1").arg == 1 && parse_cmd("volstep 1").ok && parse_cmd("volstep 5").arg == 5 && parse_cmd("volstep 5").ok);
        bool rej = true;
        for (const char *s : { "volstep 0", "volstep 6", "volstep 9", "volstep x", "volstep 33", "volstep 3 ", "volstep  3", "volstep -1", "volstep +3", "volstep ", "volstep 3x" }) {
            const Cmd r = parse_cmd(s);
            rej = rej && r.mine && !r.ok && r.req == Req::NONE;
        }
        CHECK("cmd: a bad step is ours but refused (ERR volstep 1-5), never applied", rej);
        bool other = true;
        for (const char *s : { "vol", "vol 50", "vol 100", "volume", "volstepx", "volstep3", "volup ", "volup 2", "voldn 1", "voldown", "VOLUP", "", "stat", " volup", "tune" }) other = other && !parse_cmd(s).mine;
        CHECK("cmd: the old 'vol <0..100>' and everything else are not ours", other);
    }

    // ── 응답·상태 줄의 정확한 모양 ──────────────────────────────────────────
    {
        char buf[64];
        const int n = format_line(buf, sizeof buf, 3, 72, false);
        CHECK("line: VOLSTEP step=3 max=5 vol=72 saved=0", std::string(buf) == "VOLSTEP step=3 max=5 vol=72 saved=0" && n == (int)std::string(buf).size());
        format_line(buf, sizeof buf, 5, 100, true);
        CHECK("line: VOLSTEP step=5 max=5 vol=100 saved=1", std::string(buf) == "VOLSTEP step=5 max=5 vol=100 saved=1");
        char tiny[8];
        format_line(tiny, sizeof tiny, 5, 100, true);
        CHECK("line: a short buffer is cut, not overrun", std::string(tiny) == "VOLSTEP");
    }

    // ── 저장 미루기: 연타해도 플래시에는 마지막 값 한 번, 통화·벨 중에는 쓰지 않는다 ──
    {
        Saver s; s.boot(3);
        CHECK("saver: after boot nothing to write", !s.pending() && s.is_saved(3) && !s.is_saved(4) && !s.should_write(1000000, true));
        s.changed(4, 1000);
        CHECK("saver: a change waits SAVE_DELAY_MS", s.pending() && !s.should_write(1000, true) && !s.should_write(1000 + SAVE_DELAY_MS - 1, true) && s.should_write(1000 + SAVE_DELAY_MS, true));
        CHECK("saver: the delay is long enough to swallow a burst of taps (>= 2 s)", SAVE_DELAY_MS >= 2000);
        s.changed(5, 2000); s.changed(4, 2400); s.changed(5, 2800);
        CHECK("saver: every tap pushes the write back -> one write for the burst", !s.should_write(1000 + SAVE_DELAY_MS, true) && !s.should_write(2800 + SAVE_DELAY_MS - 1, true) && s.should_write(2800 + SAVE_DELAY_MS, true));
        CHECK("saver: never while a call or a bell is on (flash writes stall both cores)", !s.should_write(2800 + SAVE_DELAY_MS, false) && !s.should_write(99999999, false) && s.pending());
        CHECK("saver: written as soon as the phone is idle again", s.should_write(99999999, true));
        s.wrote(5, true);
        CHECK("saver: after a good write it is saved and quiet", !s.pending() && s.is_saved(5) && !s.is_saved(3) && !s.should_write(99999999, true));
        s.changed(4, 5000); s.changed(5, 5400);
        CHECK("saver: back to the saved value -> nothing to write", !s.pending() && !s.should_write(99999999, true));
        s.changed(2, 6000);
        s.wrote(2, false);
        CHECK("saver: a failed write is not repeated every loop, and is not reported as saved", !s.pending() && !s.should_write(99999999, true) && !s.is_saved(2) && s.is_saved(5));
        s.changed(2, 7000);
        CHECK("saver: the next change tries again", s.pending() && s.should_write(7000 + SAVE_DELAY_MS, true));
        Saver z;
        CHECK("saver: a fresh one believes the default is stored (first boot writes nothing)", !z.pending() && z.is_saved(STEP_DEFAULT));
    }

    // ── 부팅 때 저장값 걸기(P4V-D7): 소리 장치를 연 직후, 소리 태스크들보다 먼저 ──
    CHECK("boot: a turned-down phone gets its saved step written as soon as the codec is open", boot_needs_write(true, 100, 1) && boot_needs_write(true, 100, 2) && boot_needs_write(true, 100, 3) && boot_needs_write(true, 100, 4));
    CHECK("boot: nothing to write at the top step (the codec opens at 100) or when it already matches", !boot_needs_write(true, 100, STEP_MAX) && !boot_needs_write(true, 72, 3) && !boot_needs_write(true, 52, 1));
    CHECK("boot: skipped when the sound device is not ready", !boot_needs_write(false, 100, 1) && !boot_needs_write(false, 100, 3) && !boot_needs_write(false, 100, 5) && !boot_needs_write(false, 72, 2));

    // ── 점검 도구가 속지 않게: selftest·vping 의 판정은 맨 위 단계에서만 유효하다(TUNE meas=) ──
    {
        bool only_top = true;
        for (int s = STEP_MIN; s <= STEP_MAX; ++s) {
            voicetune::Knobs k; k.vol = vol_of(s);
            only_top = only_top && voicetune::meas_valid(k) == (s == STEP_MAX) && meas_ok_at(s) == (s == STEP_MAX);
        }
        CHECK("meas: TUNE meas=1 at the top step only -> a turned-down phone shows meas=0, not 'broken'", only_top);
    }

    printf("\n%d ok, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
