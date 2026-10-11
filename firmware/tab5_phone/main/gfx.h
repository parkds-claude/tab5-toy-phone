// gfx.h — 화면 합성·출력. 1280×720 RGB565 버퍼 한 장에 그린 뒤 present() 로 한 번에 내보낸다.
// 출처: firmware/tab5_snapbooth/main/main.cpp 의 합성 방식(카메라·버퍼 교대는 걷어냄). docs/design/README.md §2 "버퍼에 합성 후 한 번에 출력".
// 규칙: 그리기 함수와 present() 는 화면 태스크 하나에서만 부른다(잠금 없음). screenshot_jpeg() 만 다른 태스크에서 불러도 된다.
// 나눔(2026-10-08 토이폰 테마):
//   raster.h      픽셀 합성의 순수 부분(둥근 도형·글자 마스크·색 그림) — 하드웨어 무관
//   gfx_draw.cpp  아래 그리기 함수들 — raster.h 로만 그린다. 기기와 맥의 화면 미리보기(tools/sim)가 같은 파일을 쓴다
//   gfx.cpp       기기 쪽: 합성 버퍼·PPA 출력·스크린샷·내장 글꼴 글자(text*)
//   색과 버튼 모양(테마)은 여기 없다 — toy.h 가 정한다.
#pragma once
#include <cstddef>
#include <cstdint>

#include "geom.h"           // SCREEN_W, SCREEN_H, Rect
#include "raster.h"         // raster::Canvas
#include "sprite.h"         // ui_sprite_t (색 그림 자산 — main/ui_sprites.h 는 toy.cpp 만 포함한다)
#include "ui_assets.h"      // tools/gen_ui_assets.py 가 만든 알파 마스크(ui_asset_t, DIG_BIG, DIG_SMALL, lbl_*)

namespace gfx {

constexpr uint32_t BOOT_FILL = 0xBFE8FFu;   // init() 이 내보내는 첫 장의 색(토이폰 하늘색 — toy.h 의 static_assert 가 toy::SKY 와 묶는다)

// M5.begin() 과 M5.Display.setRotation() 뒤에 한 번. 합성 버퍼(PSRAM)·스프라이트 묶기·PPA 등록·DSI 프레임버퍼·JPEG 인코더.
// 실패하면 false 를 돌려주고 로그에 "gfx: init fail: <단계>" 를 남긴다.
//   - 회전은 3(기본) 또는 1 만 받는다(PPA 회전 각도를 여기서 정한다). 패널 감지가 실패해 1280×720 이 아니면 false.
//   - JPEG 인코더만 실패하면 true 다(화면은 쓸 수 있다). 그때 screenshot_jpeg() 는 0 을 돌려준다(로그 "gfx: init warn: jpeg encoder").
//   - 성공하면 BOOT_FILL 한 장을 내보낸 상태로 돌아온다. init 이 실패했으면 아래 함수들은 아무것도 하지 않는다.
bool init();

void clear(uint32_t rgb);                 // 합성 버퍼 전체를 한 색으로(프레임의 시작 — 비공개 표시도 풀린다)
void present();                           // 합성 버퍼를 화면으로(끝날 때까지 기다림, 약 60~80ms)

// 도형. fill_rrect·fill_circle 은 가장자리를 부드럽게 섞는다(raster.h R-D1). alpha 판은 전체를 a(0~255)로 섞는다.
void fill_rect(int x, int y, int w, int h, uint32_t rgb);
void fill_rrect(int x, int y, int w, int h, int r, uint32_t rgb);
void fill_circle(int cx, int cy, int r, uint32_t rgb);
void fill_alpha_rect(int x, int y, int w, int h, uint32_t rgb, uint8_t a);
void fill_alpha_rrect(int x, int y, int w, int h, int r, uint32_t rgb, uint8_t a);
void fill_alpha_circle(int cx, int cy, int r, uint32_t rgb, uint8_t a);
// 열 x0..x1-1 마다 윗선(top_q4[x] = 높이×16, SCREEN_W 칸짜리 표)부터 y_end 앞줄까지 — 동산. 기본값은 화면 전체
void fill_below(const uint16_t *top_q4, uint32_t rgb, int x0 = 0, int x1 = SCREEN_W, int y_end = SCREEN_H);
void stroke_quad(int x0, int y0, int cx, int cy, int x1, int y1, int width, uint32_t rgb);   // 굵은 2차 곡선(끝이 둥글다)

// 미리 구운 글자(주아체 알파 마스크). blit_asset 은 그린 폭을 돌려준다. as 가 nullptr 이면 아무것도 안 한다.
int  blit_asset(const ui_asset_t *as, int x, int y, uint32_t rgb);
void blit_asset_center(const ui_asset_t *as, int cx, int cy, uint32_t rgb);
void draw_number(int n, int cx, int cy, bool big, uint32_t rgb);                 // 숫자 자산(DIG_BIG / DIG_SMALL), 가운데 정렬
int  number_w(int n, bool big);                                                  // draw_number 가 그릴 폭
void draw_clock(int minutes, int seconds, int cx, int cy, uint32_t rgb);         // "MM:SS" — DIG_BIG 숫자 + lbl_colon, 가운데 정렬
                                                                                 //   숫자는 같은 폭의 칸에 넣는다(초가 바뀌어도 흔들리지 않음). 분이 100 이상이면 분 자리가 늘어난다

// 미리 구운 색 그림(캐릭터·아이콘). s 가 nullptr 이면 아무것도 안 한다. mirror = 좌우 뒤집기, opacity = 전체 불투명도.
void blit_sprite(const ui_sprite_t *s, int x, int y, bool mirror = false, uint8_t opacity = 255);            // (x, y) = 왼쪽 위
void blit_sprite_center(const ui_sprite_t *s, int cx, int cy, uint8_t opacity = 255);                        // 그림의 한가운데 기준
void blit_sprite_at(const ui_sprite_t *s, int axis_x, int base_y, bool mirror = false);                      // 그림의 기준점(대칭축 x, 바닥 y) 기준

// 바뀌는 짧은 글자(내장 efontKR_16_b) — Wi-Fi 이름, 비밀번호 입력 칸 등. size 는 배율.
int  text(const char *s, int x, int y, uint32_t rgb, float size = 1.0f);         // 왼쪽·세로 가운데 기준, 돌려주는 값 = 폭
int  text_w(const char *s, float size);
void text_center(const char *s, int cx, int cy, uint32_t rgb, float size);

// 마지막으로 present() 한 화면을 JPEG 로 만든다(하드웨어 인코더). *out 은 내부 버퍼를 가리키며 다음 호출 때까지 유효하다.
// 돌려주는 값 = 바이트 수, 실패하면 0. 내부 잠금이 있어 시리얼 태스크에서 불러도 된다(그동안 present 는 기다린다).
//   - 화면 태스크가 다음 화면을 그리는 도중이면 반쪽 화면을 찍지 않고, 곧 올 present() 직후의 화면을 찍는다(최대 1초 기다림,
//     그 안에 present() 가 없으면 0). 그래서 "그리기 → present()" 를 짝지어 부를 것 — 그려 놓고 present 를 미루면 캡처가 실패한다.
//   - 품질 85, 1280×720. 내부 버퍼는 1MB.
//   - G-D4(I-1): 비공개 프레임이면 0 을 돌려주고 *is_private 를 true 로 한다(is_private 는 null 이어도 된다). "비공개인가"는 present() 가
//     s_lock 안에서 실제로 내보낸 프레임에 붙이는 값이라 촬영과 화면 사이에 틈이 없다. 두 길(바로 찍기·present() 의 대리 인코딩) 모두 인코딩 자체를 하지 않는다.
size_t screenshot_jpeg(const uint8_t **out, bool *is_private = nullptr);

// 지금 그리는 프레임을 비공개로 표시한다(Wi-Fi 비밀번호 자판 화면). 기본값은 공개: clear() 가 프레임 시작에서 거짓으로 되돌리고,
// 그 프레임을 그리는 쪽이 clear() 뒤에 true 를 부른다. 화면 태스크에서만 부른다.
void set_private(bool on);

// gfx_draw.cpp 가 쓰는 연결점. 기기는 gfx.cpp, 맥의 화면 미리보기는 tools/sim/sim_gfx.cpp 가 채운다.
namespace detail {
raster::Canvas *target();                 // 그릴 준비가 됐으면 합성 버퍼를 돌려주고 "그렸다"를 표시한다(스크린샷 G-D2). 준비 전이면 nullptr
void frame_begin();                       // clear() 가 부른다 — 새 프레임이므로 비공개 표시를 푼다(G-D4)
}

}  // namespace gfx
