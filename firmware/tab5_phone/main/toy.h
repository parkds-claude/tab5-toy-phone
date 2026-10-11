// toy.h — 토이폰 테마의 색과 부품(장난감 전화기 + 캐릭터). 정본은 docs/design/README.md §5.
//   화면(ui.cpp, ui_wifi.cpp)은 여기의 부품으로만 그린다 → 모양을 바꿀 때는 이 파일(과 toy.cpp)만 본다.
//   규칙 둘:
//     ① 누르는 것은 키캡(굵은 테두리 + 아래로 두꺼운 그림자), 보여 주기만 하는 것은 납작한 알약·판. 그림자가 있으면 눌린다.
//     ② 키캡은 그림자까지 터치 영역(Rect) 안에 그린다 — 윗면은 Rect 보다 SHADOW 만큼 낮다. 그래서 ui_layout.h 의 좌표와 시험이 그대로다.
//   하드웨어 무관(gfx.h 의 그리기 함수만 쓴다) — 맥의 화면 미리보기(tools/sim)도 이 파일을 쓴다.
#pragma once
#include <cstdint>

#include "gfx.h"
#include "toy_colors.h"

namespace toy {

// 색은 toy_colors.h 에 있다(스택짱 전화기 화면과 함께 쓴다 — 2026-10-08). 이름·값은 그대로: toy::SKY, toy::INK, …
static_assert(SKY == gfx::BOOT_FILL, "the first frame gfx::init shows is the theme background");

constexpr int OUTLINE = 5;              // 키캡 테두리
constexpr int SHADOW  = 8;              // 키캡 그림자(아래로)
constexpr int PILL_OUTLINE = 4;         // 알약·판 테두리

// 바탕: 하늘 + (동산) + (구름·꽃). 프레임의 시작이다(gfx::clear 를 부른다).
enum class Scene : uint8_t {
    FLAT,       // 하늘만 — 설정 화면(목록·자판)처럼 빽빽한 화면
    HILL,       // 하늘 + 동산
    IDLE,       // + 구름(연결 중. 전화기 3대 전에는 대기 화면도 이 바탕이었다)
    INCOMING,   // + 가운데 위의 큰 구름(전화 건 쪽이 올라선다)
    CALL,       // + 구름(거는 중·통화 중)
    PEERS,      // + 구름(대기 — 상대 둘의 줄과 맨 윗줄을 비켜 둔다. 상대가 올라서는 구름 둘은 화면(ui.cpp)이 캐릭터 밑에 그린다)
};
void background(Scene s);
// 바탕의 하늘과 동산만 r 안에 다시 깐다(구름·꽃은 그리지 않는다 → 그것들이 없는 자리에만 쓴다). 화면의 한 조각만 다시 그릴 때,
// 그 조각의 가장자리(섞이는 픽셀)가 거듭 그려져 진해지지 않게 먼저 부른다.
void background_patch(const Rect &r);
void cloud(int x, int y, int scale_pct);                    // 구름 하나. (x, y) = 왼쪽 위, 100% = 약 148×80
void cloud_wide(int cx, int cy);                            // 캐릭터가 올라설 넓은 구름. (cx, cy) = 한가운데, 약 208×68(위아래로 34씩)

// 누르는 것
void keycap(const Rect &r, uint32_t face, int radius);      // 테두리·그림자·윗면. 전부 r 안에 그린다
int  keycap_cy(const Rect &r);                              // 글자·아이콘을 놓을 세로 가운데(= 윗면의 가운데)
void gloss(const Rect &r);                                  // 색이 있는 큰 키캡 윗면의 광택 줄
// 글자 하나(와 왼쪽 아이콘)를 올린 키캡. enabled=false 면 그림자 없는 납작한 판에 흐린 글자(눌리지 않는다는 표시).
void button(const Rect &r, const ui_asset_t *label, bool enabled = true, uint32_t face = WHITE, const ui_sprite_t *icon = nullptr);
void round_button(int cx, int cy, int r, uint32_t face);    // 둥근 키캡(통화 버튼). 반지름 r 인 원 안에 그린다
constexpr int ROUND_DROP = 12;                              // 둥근 키캡의 그림자(아래로)
constexpr int round_face_cy(int cy) { return cy - ROUND_DROP / 2; }      // 둥근 키캡의 글자·아이콘을 놓을 세로 가운데(= 윗면 원의 가운데)
constexpr int round_face_r(int r) { return r - ROUND_DROP; }             // 윗면 원의 반지름(테두리 안쪽)

// 보여 주는 것
void pill(int cx, int cy, int w, int h, uint32_t fill);     // 가운데 기준 알약(테두리 4)
void panel(const Rect &r, uint32_t fill, int radius);       // 테두리 있는 납작한 판(음량 칸, 입력 칸)
void label_pill(const ui_asset_t *label, int cx, int cy, int h, uint32_t fill, uint32_t fg = INK);   // 글자 폭에 맞춘 알약
int  label_pill_w(const ui_asset_t *label, int h);          // label_pill 이 그리는 알약의 폭(화면 미리보기가 "띠를 넘지 않는다"를 볼 때 쓴다)

// 캐릭터. 자리는 "대칭축 x, 발 밑 바닥 y". mirror = 좌우 뒤집기(화면 오른쪽에 설 때).
// 번호 ↔ 캐릭터의 정본은 who_of_unit 하나다(전화기 3대 — 설계서 2026-10-08-phone-3way-design.md TW-D3). "상대"의 캐릭터는 그 상대의 번호로 구한다
// (예전의 other(Who) 는 상대가 하나라는 전제였다 — 지웠다. 번호는 ui_layout.h 의 peer_unit·call_unit 이 준다).
enum class Who : uint8_t { BEAR, RABBIT, ROBOT };
enum class Pose : uint8_t { WAVE, SLEEP, CUP, CHEER, CALLER, HI };   // HI = WAVE 를 0.7배로(대기 화면에서 구름 위에 선 상대). SLEEP 도 그 크기다. 새 자세는 맨 끝에
Who  who_of_unit(int unit);                                 // 1번 = 곰, 2번 = 토끼, 3번 = 로봇(스택짱). 그 밖의 번호는 곰(그릴 일이 없다 — 미등록 기기는 대기 화면까지 오지 않는다)
void character(Who who, Pose pose, int axis_x, int base_y, bool mirror = false);
Rect character_box(Who who, Pose pose, int axis_x, int base_y, bool mirror = false);   // character 가 칠하는 사각형(화면 미리보기가 "칸을 넘지 않는다"를 볼 때 쓴다)
int  head(Who who, bool asleep, int x, int cy);             // 작은 얼굴. x = 왼쪽, cy = 세로 가운데. 돌려주는 값 = 폭
int  head_w(Who who);
enum class Chick : uint8_t { BIG, IDLE, JUMP, HUM };
void chick(Chick pose, int axis_x, int base_y);

// 아이콘(가운데 기준)
enum class Icon : uint8_t { CALL, HANG, SPEAKER, NOTE, GLOSS };   // CALL = 든 수화기(통화·받기), HANG = 엎은 수화기(끊기·거절), GLOSS = 둥근 통화 키캡의 광택
const ui_sprite_t *icon_sprite(Icon i);
void icon(Icon i, int cx, int cy, uint8_t opacity = 255);

// 종이컵 전화: cup 자세(배율 0.88)의 컵 끝 자리. 줄을 이을 때 쓴다.
constexpr int CUP_DX = 92, CUP_DY = 86;                     // 대칭축에서 컵 끝까지(가로), 바닥에서 컵 끝까지(위로)

}  // namespace toy
