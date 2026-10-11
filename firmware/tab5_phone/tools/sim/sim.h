// sim.h — 맥의 화면 미리보기(tools/render_screens.py). 기기와 같은 그리기 코드(main/gfx_draw.cpp·toy.cpp·ui.cpp·ui_wifi.cpp)를
// 맥에서 돌려 화면을 그림 파일로 만든다. 기기 없이 디자인을 확인하고, 캐릭터·버튼이 겹치지 않는지 눈으로 본다.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace sim {

// 내장 글꼴로 쓰는 "바뀌는 글자"(Wi-Fi 이름·자판 글쇠 등)는 맥에서 그리지 못한다 → 어디에 무엇을 쓰는지만 적어 둔다.
struct Text { std::string s; int x, y; uint32_t rgb; float size; bool center; };

extern std::vector<Text> texts;     // 지금 프레임에 쓴 글자(gfx::clear 가 비운다)
extern bool is_private;             // 지금 프레임이 비공개인가(gfx::set_private)

// 지금 합성 버퍼를 <base>.rgb565(1280×720, 리틀엔디언)와 <base>.txt(글자 목록)로 쓴다.
bool save(const std::string &base);

// 지금 합성 버퍼(1280×720 RGB565). 두 번 그린 결과가 같은지 비교할 때 쓴다.
const uint16_t *pixels();

}  // namespace sim
