// voice_cmd.h — 소리·통화 품질의 시리얼 명령(진단·손잡이). 기판과 무관한 부분만 모았다 — Tab5(phone_main.cpp)와 스택짱(../stackchan_phone/main/sc_main.cpp)이 함께 쓴다.
//   phone_main.cpp 에 있던 것을 그대로 옮긴 것이다(2026-10-08, 설계서 2026-10-08-phone-3way-design.md TW-D11: 공통 로직은 복사하지 않는다). 줄의 모양은 글자 하나 바꾸지 않았다
//   (tools/phone_ctl.py·tools/test_phone_ctl.py 가 읽는다). 명령의 뜻·범위·결정 ID 는 voice.h·voice_tune.h·audio_out.h 와 각 계획서.
//   여기 없는 것(기판마다 저장 자리·화면이 달라 각 main 이 맡는다): stat · t · shot · reboot · fault audio · quiet on|off · volup|voldn|volstep.
#pragma once
#include <string>

namespace voicecmd {

// 이 모듈의 명령이면 처리하고 true(응답 줄을 찍는다 — "OK …" / "ERR …" / "SELFTEST …" / "TUNE …"). 아니면 아무것도 하지 않고 false.
//   selftest | vsrc aec|mic|ref|tone|probe | txgain [1..4] | vol [0..100] | vping | tune
//   lvl · tilt · res · guard · rest · dt · rxgain · hpf · opus · jb · vtone · nodelay · mic  (인자 없이 보내면 지금 값)
bool handle(const std::string &cmd);

void print_tune();                  // "TUNE dflt=… …" 한 줄(PL3c-D3)

}  // namespace voicecmd
