// ui.h — 통화 화면 그리기. 어떤 화면인지·버튼 위치는 ui_layout.h 가 정한다.
#pragma once
#include "ui_layout.h"

namespace ui {

// 합성 버퍼에 통화 화면 한 장을 그린다. 화면으로 내보내는 것(gfx::present)은 부르는 쪽이 한다. 화면 태스크에서만 부른다.
void draw(const uilayout::Model &m);

// 합성 버퍼의 음량 칸(uilayout::VOL_AREA)만 다시 그린다 — 음량 단계만 바뀌었을 때(P4V-D4). 음량 칸이 없는 화면이면 아무것도 안 한다.
// draw 와 같이 내보내기(gfx::present)는 부르는 쪽이 한다.
void draw_volume(const uilayout::Model &m);

}  // namespace ui
