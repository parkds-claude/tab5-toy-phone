// vol_steps.h — 화면의 "작게 / 크게": 음량 단계 ↔ 코덱 재생 음량, 올림·내림, 저장 값 검증, 저장 미루기, 시리얼 명령 읽기.
// 하드웨어와 무관한 순수 코드(맥에서 단위 시험: tools/test_vol_steps.cpp). 그리기는 ui.cpp, 적용·저장은 app.cpp.
//
// 사용자 요구(2026-10-05): "소리/볼륨을 조절(작게, 크게)하는 기능이 필요함" — 어린아이가 쓴다.
// 결정
//   P4V-D1 단계는 5개, 간격은 %가 아니라 dB(귀에 고르게): 맨 위 = 100(0dB, 지금 값·부팅 기본), 한 단계 = 5dB, 맨 아래 = 52(-20dB).
//        이유: 코덱 곡선이 가파르다(1% = 0.5dB — 아래 codec_reg). %로 고르게 나누면(100·80·60·40·20) 한 단계가 10dB 라 두 번 누르면 거의 안 들린다.
//              맨 위는 100 을 넘지 않는다(하울링 — 더 키우지 않는다). 맨 아래도 무음이 아니다(벨을 들어야 한다): -20dB = 소리 크기 1/10, 귀에는 1/4쯤.
//              (Phase 0 시험 펌웨어의 시작 음량이 50 = -21dB 였다 — audio_out.cpp 머리말 PL3a-D2. 그 값에서 벨이 들리는지는 기록에 없다 → 맨 아래 단계의 벨은 기기에서 귀로 확인해야 한다.)
//        비용: 맨 아래에서도 더 줄일 수는 없다. 탈출구: 아래 VOL_OF_STEP 한 줄(시험이 "5dB 고른 간격·맨 위 100"을 고정한다 — 간격을 바꾸면 STEP_HALF_DB 도).
//   P4V-D2 저장은 미룬다: 마지막으로 바뀐 뒤 3초 동안 더 안 바뀌고, 통화·벨 중이 아닐 때 한 번(Saver).
//        이유: 연타마다 플래시에 쓰지 않는다. 플래시에 쓰는 동안은 두 코어가 잠깐 멈춘다(드물게 NVS 쪽 지우기 수십 ms) → 통화 소리가 끊길 수 있다.
//        비용: 통화 중에 바꾸고 통화가 끝나기 전에 전원이 꺼지면 그 값은 남지 않는다(앞에 저장된 값으로 켜진다). 탈출구: app.cpp 가 should_write 에 늘 true 를 넘긴다.
//   P4V-D3 화면 버튼과 시리얼 명령은 같은 요청(Req)으로 바뀌어 같은 함수(apply)를 탄다 — 기기에서 사람 없이 확인할 수 있게.
//        시리얼: volup | voldn | volstep <1..5> | volstep(지금 값). 응답은 한 줄 "VOLSTEP step=<1..5> max=5 vol=<0..100> saved=<0|1>"(format_line).
//        기존 'vol <0..100>'(PL3b-D24, 저장 안 됨, 진단용)은 그대로다 — 그 명령이 코덱 값을 옮기면 vol= 이 단계의 값과 달라 보인다(단계를 다시 누르면 맞춰진다).
//   P4V-D6 selftest·vping 의 판정은 "음량 100"이 전제다(voice_tune.h meas_valid). 맨 위 단계가 아니면 펌웨어가 이미 TUNE … meas=0 한 줄을 뒤에 붙여 드러낸다(PL3c-D3) —
//        재는 동안 100 으로 올렸다 내리지 않는다(스피커가 켜져 있으면 사용자가 줄여 둔 소리가 갑자기 커진다). 재는 절차는 도구 쪽에 못 박았다(tools/phone_ctl.py 설명글 — 수정 1회차):
//        'volstep' 으로 지금 단계를 읽어 둔다 → 'volstep 5' → 재기 → 원래 단계로 되돌린다.
//        비용: 줄여 둔 기기의 SELFTEST 줄은 ok=0 으로 보인다(바로 뒤 TUNE 줄의 meas=0 을 함께 읽어야 한다). 탈출구: phone_main.cpp 의 selftest 가 앰프가 꺼져 있을 때만 잠깐 100 으로 올린다.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace volsteps {

constexpr int COUNT = 5;
constexpr int STEP_MIN = 1, STEP_MAX = COUNT, STEP_DEFAULT = STEP_MAX;   // 저장된 값이 없으면 맨 위 = 지금까지의 동작
constexpr int VOL_OF_STEP[COUNT] = { 52, 62, 72, 82, 100 };              // 단계 1~5 의 코덱 재생 음량(%) — -20, -15, -10, -5, 0dB
constexpr int STEP_HALF_DB = 10;                                         // 이웃한 단계의 간격(0.5dB 단위) = 5dB
constexpr int VOL_TOP = 100;                                             // 재생 최대(지금 값). 이보다 크게 하지 않는다
static_assert(COUNT >= 2 && COUNT <= 9, "the serial command reads the step as one digit");
static_assert(VOL_OF_STEP[COUNT - 1] == VOL_TOP, "the top step is today's volume (100) - never louder");
static_assert(VOL_OF_STEP[0] > 0, "the bottom step is not mute - the bell must be heard");

constexpr bool ok_step(int s) { return s >= STEP_MIN && s <= STEP_MAX; }
constexpr int clamp(int s) { return s < STEP_MIN ? STEP_MIN : s > STEP_MAX ? STEP_MAX : s; }
constexpr int vol_of(int step) { return VOL_OF_STEP[clamp(step) - 1]; }      // 깨진 값이 와도 표 밖을 읽지 않는다
constexpr int up(int s) { return clamp(clamp(s) + 1); }                      // 끝에서 멈춘다(돌아가지 않는다)
constexpr int down(int s) { return clamp(clamp(s) - 1); }
constexpr int from_saved(int raw) { return ok_step(raw) ? raw : STEP_DEFAULT; }   // 저장소에서 읽은 값: 범위 밖(없음·지워진 플래시·다른 판)이면 기본
constexpr int step_of_vol(int vol)                                           // 코덱 값 → 단계. 표에 없는 값이면 0(진단용 vol 명령이 옮겼다)
{
    for (int i = 0; i < COUNT; ++i) if (VOL_OF_STEP[i] == vol) return i + 1;
    return 0;
}

// 코덱 음량(%) → ES8388 DAC 음량 레지스터(0 = 0dB, 한 칸 = -0.5dB, 0xC0 = -96dB). 부품 소스(managed_components/espressif__esp_codec_dev)의 식을 정수로 옮긴 것:
//   esp_codec_dev.c:79-90·92-115  기본 곡선: 0% = -50dB … 100% = 0dB 직선(1% = 0.5dB), 0 만 -96dB
//   es8388.c:462·352              거기에 3.61dB 를 더한다(hw_gain = 20·log10(DAC 3.3V / 앰프 5.0V) — audio_out.cpp 의 설정) → 93~100% 는 모두 0dB 로 잘린다
//   esp_codec_dev_vol.c:9-23      레지스터 = (int)(-2 × dB)
//   → 1~92% 는 (92 − %)칸, 즉 -(92 − %)/2 dB. 재검수 기록과 맞는다: 88 = -2dB(selftest 가 턱걸이), 74 = -9dB(vping 이 못 찾음).
// 시험이 부품의 실수 식과 0~100 전부를 견준다(test_vol_steps.cpp ref_reg).
constexpr int codec_reg(int vol) { return vol <= 0 ? 184 : vol >= 93 ? 0 : 92 - vol; }
constexpr int step_half_db(int step) { return -codec_reg(vol_of(step)); }    // 단계의 크기(0.5dB 단위, 맨 위 = 0)

// P4V-D6: selftest·vping 의 판정이 유효한 단계인가(= 코덱 값이 voicetune::LEGACY_VOL 100 인 단계 — 시험이 voice_tune.h 의 meas_valid 와 견준다)
constexpr bool meas_ok_at(int step) { return vol_of(step) == VOL_TOP; }

// P4V-D7(수정 1회차, 검수 Important 2): 부팅 때 저장된 단계를 소리 장치를 연 직후에 건다 — 신호음·목소리·Wi-Fi·교환기를 시작하기 전(phone_main → app::vol_apply_saved).
//   이유: 코덱은 100 으로 열린다. 전에는 app::start 에 가서야 저장값을 걸어, 그 사이(다른 태스크들을 띄우는 동안) 코덱이 100 이었다 — 줄여 쓰던 기기가 그 구간에 소리를 내면 가장 크게 난다.
//   지금 쓸 것이 있나: 소리 장치가 준비됐고 코덱 값이 그 단계의 값과 다를 때만(맨 위 단계면 쓸 것이 없다, 준비 안 됐으면 건너뛴다).
//   비용: 부팅 때 NVS 를 한 번 더 읽는다(app::start 도 읽는다 — 그쪽은 단계 기억과 VOLSTEP 부팅 줄을 맡는다). 탈출구: phone_main.cpp 의 호출 한 줄을 지운다(app::start 가 전처럼 건다).
constexpr bool boot_needs_write(bool audio_ready, int codec_vol, int step) { return audio_ready && codec_vol != vol_of(step); }

// ── 요청(P4V-D3): 화면 버튼·시리얼 명령 → 같은 모양 ─────────────────────────
enum class Req : uint8_t { NONE, SHOW, DOWN, UP, SET };                       // SHOW = 바꾸지 않고 지금 값만 알린다
constexpr bool req_valid(int r) { return r >= (int)Req::SHOW && r <= (int)Req::SET; }
constexpr int apply(int step, Req r, int arg = 0)                            // 새 단계. SET 의 값이 범위 밖이면 그대로
{
    return r == Req::UP ? up(step) : r == Req::DOWN ? down(step) : (r == Req::SET && ok_step(arg)) ? arg : clamp(step);
}

// 시리얼 한 줄 → 요청. mine = 음량 단계 명령이다 / ok = 받아들인다(거짓이면 "ERR volstep 1-5").
// 기존 'vol'·'vol <0..100>' 은 여기 것이 아니다(mine=false → phone_main 의 앞 가지가 처리한다).
struct Cmd { bool mine; bool ok; Req req; int arg; };
inline Cmd parse_cmd(const char *s)
{
    if (!strcmp(s, "volup")) return { true, true, Req::UP, 0 };
    if (!strcmp(s, "voldn")) return { true, true, Req::DOWN, 0 };
    if (strncmp(s, "volstep", 7) != 0 || (s[7] != 0 && s[7] != ' ')) return { false, false, Req::NONE, 0 };
    if (s[7] == 0) return { true, true, Req::SHOW, 0 };
    const char *p = s + 8;                                                    // 숫자 한 자리만(부호·공백·뒤 글자 없음)
    if (p[0] >= '0' && p[0] <= '9' && p[1] == 0 && ok_step(p[0] - '0')) return { true, true, Req::SET, p[0] - '0' };
    return { true, false, Req::NONE, 0 };
}

// 응답·상태 줄(줄바꿈 없이): "VOLSTEP step=3 max=5 vol=72 saved=0". vol = 코덱의 지금 값, saved = 이 단계가 저장소에 들어 있다.
inline int format_line(char *buf, size_t n, int step, int vol, bool saved)
{
    return snprintf(buf, n, "VOLSTEP step=%d max=%d vol=%d saved=%d", step, COUNT, vol, (int)saved);
}

// ── 저장 미루기(P4V-D2). app 태스크 하나에서만 쓴다 ──────────────────────────
constexpr int64_t SAVE_DELAY_MS = 3000;
struct Saver {
    int     saved = STEP_DEFAULT;       // 저장소에 들어 있다고 아는 값(부팅 때 읽은 값 → 그 뒤 성공한 쓰기). 저장된 것이 없으면 기본값과 같다고 본다
    int64_t due_ms = -1;                // 쓸 때. -1 = 쓸 것 없음

    void boot(int step) { saved = step; due_ms = -1; }
    void changed(int step, int64_t now_ms) { due_ms = step == saved ? -1 : now_ms + SAVE_DELAY_MS; }   // 누를 때마다 뒤로 민다. 저장된 값으로 돌아오면 쓸 것이 없다
    bool pending() const { return due_ms >= 0; }
    bool is_saved(int step) const { return step == saved; }
    // 지금 쓸까. idle = 통화·벨·연결음 중이 아니다(app 이 통화 상태로 정한다)
    bool should_write(int64_t now_ms, bool idle) const { return due_ms >= 0 && idle && now_ms >= due_ms; }
    // 쓰기를 마쳤다. 실패하면 되풀이하지 않는다(20ms 바퀴마다 플래시를 두드리지 않게) — 다음에 바꿀 때 다시 쓴다
    void wrote(int step, bool ok) { if (ok) saved = step; due_ms = -1; }
};

}  // namespace volsteps
