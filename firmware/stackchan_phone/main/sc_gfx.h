// sc_gfx.h — 스택짱 화면 출력(기기 쪽). 320×240 RGB565 합성 버퍼 한 장(PSRAM)에 그린 뒤 M5GFX 로 내보낸다.
//   그리기는 sc_ui.cpp(순수 — raster.h)가 이 버퍼에 한다. 이 모듈은 버퍼·내보내기·화면 찍기(시리얼 'shot')만 맡는다.
//   규칙: frame_begin → (그리기) → present → frame_end 를 화면 태스크 하나에서만. snapshot 은 다른 태스크(시리얼)에서 불러도 된다 — 같은 잠금으로 묶여 반쪽 화면을 찍지 않는다.
#pragma once
#include <cstddef>
#include <cstdint>

#include "raster.h"
#include "sc_ui_layout.h"

namespace scgfx {

// M5.begin() 뒤에 한 번. 버퍼를 만들고(실패하면 false) 하늘색 한 장을 내보낸다.
bool init();

const raster::Canvas &frame_begin();                // 잠금을 잡고 합성 버퍼를 돌려준다
void present();                                     // 버퍼 전체를 화면으로(약 35ms — SPI 40MHz)
void present(const sclayout::Rect &r);              // 그 조각만(통화 중 시계: 약 4ms)
void frame_end();                                   // 잠금을 푼다

// 지금 화면(마지막으로 present 한 내용)의 사본을 준다: RGB565 리틀엔디언, W×H×2 바이트. *out 은 내부 버퍼(다음 호출까지 유효). 실패하면 0.
size_t snapshot(const uint8_t **out);

}  // namespace scgfx
