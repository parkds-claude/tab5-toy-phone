// ui_wifi.h — Wi-Fi 설정 화면(망 목록 + 비밀번호 자판).
// 출처: firmware/tab5_snapbooth/main/main.cpp 의 NET_LIST / NET_PASS 화면. 기능은 그대로, 터치 영역만 디자인 규칙(96px 이상)에 맞춘다.
// 규칙: 모든 함수는 화면 태스크에서만 부른다. 자판 화면에서는 터치 좌표를 로그에 남기지 않는다(비밀번호 복원 방지).
#pragma once

namespace uiwifi {

void open();            // 설정 화면을 연다(스캔 시작). 이미 열려 있으면 아무것도 안 한다
bool is_open();
void close();

// 현재 화면(목록 또는 자판)을 gfx 합성 버퍼에 그린다. present() 는 부르는 쪽이 한다.
void draw();

// 터치 한 번을 처리한다. 화면이 닫혔으면(닫기 버튼, 연결 성공 후) false 를 돌려준다.
bool tap(int x, int y);

// 마지막 draw() 이후 다시 그려야 할 일이 생겼으면 true(스캔 끝남, 수동 연결 상태 변화, 안내 문구 시간 경과 등).
// 주의: 연결에 성공하면 "연결됨" 을 1.5초 보여 준 뒤 이 함수가 화면을 닫고 false 를 돌려준다(터치 없이 닫히는 유일한 경우).
//       그래서 부르는 쪽은 매 바퀴 is_open() 을 다시 확인해, 닫혔으면 원래 화면을 다시 그린다.
//       열려 있는 동안에는 매 바퀴 불러야 한다(부르지 않으면 자동으로 닫히지 않는다).
bool needs_redraw();

// 지금 비밀번호 자판이 떠 있나(터치 좌표 로그를 막을 때 쓴다)
bool on_keyboard();

}  // namespace uiwifi
