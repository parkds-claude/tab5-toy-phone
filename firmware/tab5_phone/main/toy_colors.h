// toy_colors.h — 토이폰 테마의 색(정본: docs/design/README.md §5.1). 하드웨어·화면 크기와 무관 — Tab5(toy.h)와 스택짱(sc_ui.cpp)이 함께 쓴다.
//   toy.h 에서 색만 떼어 낸 것이다(2026-10-08, 설계서 2026-10-08-phone-3way-design.md TW-D18: 세 기기가 한 식구로 보이게 같은 색을 쓴다).
#pragma once
#include <cstdint>

namespace toy {

// 색(RGB888)
constexpr uint32_t SKY   = 0xBFE8FFu;   // 바탕(하늘)
constexpr uint32_t CLOUD = 0xFFFFFFu;
constexpr uint32_t HILL  = 0x7ED99Au;   // 동산
constexpr uint32_t INK   = 0x2A1F3Du;   // 테두리·글자(짙은 자주빛 남색)
constexpr uint32_t WHITE = 0xFFFFFFu;   // 보통 버튼의 윗면, 판
constexpr uint32_t GO    = 0x3DDC84u;   // 통화·받기·켜짐
constexpr uint32_t STOP  = 0xFF5C5Cu;   // 끊기·거절·오류
constexpr uint32_t WARN  = 0xFFD93Du;   // 알림·부재중·음량 버튼
constexpr uint32_t VOL   = 0x4D9FFFu;   // 음량 막대·입력 커서
constexpr uint32_t PALE  = 0xE6E1F2u;   // 꺼진 것·빈 막대·보조 글쇠
constexpr uint32_t MUTED = 0x625977u;   // 흐린 글자(하늘·흰색·PALE 바탕에서 명도 대비 4.5:1 을 넘는 가장 옅은 값)
constexpr uint32_t FAINT = 0xB4ADC6u;   // 더 못 가는 기호(음량 끝)
constexpr uint32_t GO_SOFT = 0xC9F5DDu; // 지금 연결된 줄(목록)

}  // namespace toy
