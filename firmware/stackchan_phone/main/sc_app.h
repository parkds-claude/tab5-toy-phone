// sc_app.h — 스택짱 전화기의 통화 진행. 사건(터치·교환기 신호·시간)을 모아 call_fsm 에 넣고, 돌려받은 "할 일"을 실행하고, 화면을 그린다.
//   Tab5 의 app.h/app.cpp 와 같은 역할·같은 순서다(그 파일의 결정 AP-D1·D2, PL3a-D13, PL3b-D13, PL3c-D10, P4V 를 그대로 따른다). 따로 둔 이유: 화면이 다르고
//   (sc_ui — 상대 둘의 통화 키캡, 스트림덱 키캡), Tab5 쪽에만 있는 것(Wi-Fi 설정 화면, 메시지 거절, 벨 끄기)이 없다(설계서 TW-D5).
//   판단은 전부 Tab5 와 같은 순수 헤더가 한다: call_fsm.h · roster.h · relay_proto.h · vol_steps.h · voice_core.h · tones.h.
#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace scapp {

// 화면·통화 진행 태스크와 터치 태스크를 띄운다. scgfx::init, audioout::start, toneout::start, voice::start, netwifi::start, relaylink::start 뒤에 한 번.
// unit = 이 기기의 번호(미등록이면 0). i2c_lock = M5.In_I2C 를 쓰는 코드가 함께 잡는 잠금(터치 읽기와 코덱·앰프 제어가 같은 버스다).
void start(int unit, SemaphoreHandle_t i2c_lock);

void inject_tap(int x, int y);      // 시리얼 't x y' — 실제 터치와 같은 대기열로
void print_stat();                  // 시리얼 'stat'
bool vol_request(int req, int arg); // 시리얼 volup·voldn·volstep — 화면의 작게/크게와 같은 대기열로. 답은 app 태스크가 "VOLSTEP …" 한 줄로
void vol_apply_saved();             // 저장된 음량 단계를 코덱에 건다. audioout::start 바로 뒤, 다른 소리 태스크를 시작하기 전에 한 번(main 태스크)
// 시리얼 'deck' — 스트림덱으로 간다(화면의 두 번 누르기를 거치지 않는다). 대기·연결 중일 때만. 돌려주는 값 = 요청이 대기열에 들어갔다(가는 것은 app 태스크가 한다)
bool deck_request();

}  // namespace scapp
