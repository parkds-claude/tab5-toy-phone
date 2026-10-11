// sc_boot.h — 스트림덱(UIFlow2)과 전화기 펌웨어 사이를 오가는 부팅 선택. 설계서 2026-10-08-phone-3way-design.md §11.2 (TW-D14·D15).
//   한 플래시에 UIFlow2(factory 칸)와 이 펌웨어(ota_0 칸)가 함께 있고, otadata 칸이 "다음에 부팅할 쪽"을 기억한다.
//   스트림덱 쪽에서는 MicroPython 의 esp32.Partition('ota_0').set_boot() 로 이쪽을 고른다(~/Deskbot/firmware/stackdeck/main.py 의 "전화기" 버튼).
#pragma once

namespace scboot {

// 부팅 직후 한 번(main 태스크에서 — 플래시를 읽는다): 지금 도는 앱을 "정상"으로 표시한다. UIFlow2 의 부트로더가 되돌리기(rollback)를 켜 두었다면 이 표시가 없을 때 다음 부팅에 스트림덱으로 돌아가 버린다
// — 켜 두지 않았다면 아무 일도 하지 않는다(어느 쪽이든 안전하다). "SCBOOT running=<칸 이름> deck=<0|1>" 한 줄을 찍는다.
void mark_running_ok();

// USB(USB-Serial/JTAG) 단자 설정을 전원 켜질 때의 값으로 되돌린다(SB-D7 — 스트림덱이 남긴 설정 때문에 이 펌웨어의 USB 가 맥에 잡히지 않는 것을 막는다).
//   app_main 맨 앞에서 한 번. 플래시를 만지지 않고 금방 돌아온다. 스트림덱에서 넘어온 부팅이면 "SCBOOT usb restored (came from the deck)" 한 줄을 찍는다.
void usb_restore();

// 연달아 죽는 고리에서 빠져나오기(SB-D5). mark_running_ok 바로 뒤, 다른 어떤 초기화보다 먼저 한 번(main 태스크):
//   이번 부팅이 "죽어서 다시 켜진 것"(패닉·감시 타이머)이면 횟수를 올리고, 세 번 연달아면 스트림덱으로 돌아간다(돌아오지 않는다).
//   "SCBOOT reset=<이유> crashes=<n>" 한 줄을 찍는다. 횟수는 재시작에도 남는 메모리(RTC)에 둔다 — 전원을 껐다 켜면 0.
void crash_guard();
// 살아 있음 알리기와 감시(SB-D6). start_watch 는 태스크를 다 띄운 뒤 한 번, alive 는 app 태스크가 바퀴마다 부른다.
//   alive 가 20초째 없으면 멈춘 것으로 보고 일부러 죽는다(→ 다시 켜지고 crash_guard 가 센다). 60초를 무사히 돌면 죽은 횟수를 지운다.
void start_watch();
void alive();

bool has_deck();            // 스트림덱(factory 칸의 앱)이 있나 — 없으면 "스트림덱" 버튼을 흐리게 한다. mark_running_ok 가 확인해 둔 값(그 전에는 false)
// 다음 부팅을 스트림덱으로 정하고 재시작한다. 성공하면 돌아오지 않는다. 실패(칸이 없음·쓰기 실패)하면 false — 지금 펌웨어가 그대로 돈다.
bool to_deck();

}  // namespace scboot
