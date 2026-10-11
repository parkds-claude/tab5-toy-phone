// sc_ui.h — 스택짱 전화기 화면 그리기(320×240). "그릴 내용"(sclayout::Model)만 받아 캔버스에 그린다 — 통화 상태를 바꾸지 않는다.
//   하드웨어와 무관하다(raster.h 로만 그린다) → 기기(sc_gfx.cpp 의 캔버스)와 맥의 화면 미리보기(tools/sim)가 이 파일을 함께 쓴다.
//   자리는 sc_ui_layout.h, 색은 Tab5 와 같은 toy_colors.h, 그림·글자는 tools/gen_sc_assets.py 가 구운 sc_sprites.h·sc_assets.h.
#pragma once
#include "raster.h"
#include "sc_ui_layout.h"

namespace scui {

void draw(const raster::Canvas &cv, const sclayout::Model &m);          // 화면 한 장 전체
// 통화 중: 시계 조각만 다시 그린다(sclayout::CLOCK_AREA — 바탕부터 다시 깐다). 관문 실험 조건 3: 통화 중 화면은 가볍게.
void draw_clock(const raster::Canvas &cv, const sclayout::Model &m);
// 통화 중: 음량 표시 조각만 다시 그린다(sclayout::VOL_PIPS).
void draw_volume(const raster::Canvas &cv, const sclayout::Model &m);

}  // namespace scui
