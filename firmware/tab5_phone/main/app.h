// app.h — 통화 진행. 사건(터치·교환기 신호·시간)을 모아 call_fsm 에 넣고, 돌려받은 "할 일"을 실행하고, 화면을 그린다.
// 설계서 §9.5 의 ui(터치 → 사건)와 main(태스크 기동) 사이를 잇는 모듈이다. PL2-D2: 화면 그리기(ui)와 진행(app)을 나눠
// ui 를 "그릴 내용만 받아 그리는" 코드로 유지한다. 비용: 파일 하나 추가. 탈출구: app.cpp 를 ui.cpp 에 합친다.
#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace app {

// 화면·통화 진행 태스크를 띄운다. gfx::init, audioout::start, toneout::start, voice::start, netwifi::start, relaylink::start 뒤에 한 번. unit = 이 기기의 번호(미등록이면 0).
// i2c_lock = M5.In_I2C 를 쓰는 코드가 함께 잡는 잠금. 터치 읽기(M5.update)와 코덱·앰프 제어(audio_out)가 같은 버스를 쓴다 — phone_main 이 만든다(PL3a-D14).
void start(int unit, SemaphoreHandle_t i2c_lock);

void inject_tap(int x, int y);      // 시리얼 't x y' — 실제 터치와 같은 대기열(8칸)에 넣는다. 화면 태스크가 다음 바퀴에 처리한다
void print_stat();                  // 시리얼 'stat' — STAT 한 줄을 찍는다(어느 태스크에서 불러도 된다)

// P4V-D3 음량 단계 요청(시리얼 volup·voldn·volstep) — 화면의 작게/크게와 같은 대기열에 넣는다. req = volsteps::Req 의 값, arg = SET 의 단계.
// app 태스크가 다음 바퀴에 처리하고 "VOLSTEP step=… max=… vol=… saved=…" 한 줄로 답한다. 돌려주는 값 = 대기열에 들어갔다(app 이 없거나 가득 찼으면 false).
bool vol_request(int req, int arg);

// P4V-D7 저장된 음량 단계를 코덱에 건다. audioout::start 바로 뒤, 다른 소리 태스크(toneout·voice)를 시작하기 전에 한 번(NVS 초기화 뒤) — 소리 장치가 준비되지 않았으면 아무것도 안 한다.
// 시리얼 줄은 찍지 않는다(부팅 줄 "VOLSTEP …" 은 app::start 가 찍는다).
void vol_apply_saved();

}  // namespace app
