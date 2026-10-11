// sc_store.h — 전화기 전용 설정 칸(nvs_phone)의 작은 값들 + "플래시를 만지는 일은 내부 메모리 스택에서" 를 지키는 일꾼.
//   설계서 TW-D16: 스트림덱(UIFlow2)의 nvs 칸은 열지도 쓰지도 않는다.
//   열쇠: "volstep"(음량 단계 1~5) · "quiet"(조용한 모드) · "rst_<무엇>"(다시 켜 본 횟수 — sc_main.cpp boot_guard).
//   SS-D1 플래시를 읽고 쓰는 동안 ESP32-S3 는 캐시를 끈다 → 그동안 PSRAM 을 못 읽는다 → 스택이 PSRAM 에 있는 태스크가 플래시 함수를 부르면 IDF 가 멈춘다
//         (cache_utils.c 의 assert(esp_task_stack_is_sane_cache_disabled())). 스택짱은 내부 메모리가 빠듯해 app·시리얼 태스크의 스택을 PSRAM 에 두므로,
//         플래시를 만지는 일(이 파일의 읽기·쓰기, sc_boot 의 부팅 대상 바꾸기)은 내부 메모리 스택을 가진 일꾼 태스크가 대신 한다(run).
//         부르는 쪽의 스택이 내부 메모리면 그 자리에서 바로 한다. 비용: 일꾼 태스크 하나(내부 메모리 8KB — 9MB 앱 이미지 검사가 이 스택에서 돈다. 여유는 STAT stk_flash). 탈출구: app·시리얼 태스크의 스택을 내부 메모리로(그러면 run 이 늘 바로 한다).
#pragma once
#include <cstdint>

namespace scstore {

// app_main 에서 한 번(main 태스크 — 내부 메모리 스택). 칸이 비었거나 형식이 다르면 지우고 다시 만든다(이 칸은 전화기 것뿐이라 지워도 잃는 것은 음량 단계뿐이다).
// 일꾼 태스크도 여기서 띄운다. 실패하면 false — 그 뒤의 읽기는 기본값, 쓰기는 실패.
bool init();
bool ready();
unsigned stack_free();              // 일꾼 태스크 스택의 최저 여유(바이트, 없으면 0 — STAT stk_flash=)

uint8_t get_u8(const char *key, uint8_t fallback);
bool set_u8(const char *key, uint8_t value);
bool erase(const char *key);                        // 없던 열쇠를 지우는 것도 성공이다

// fn(arg) 를 내부 메모리 스택에서 돌리고 그 결과를 돌려준다(SS-D1). 끝날 때까지 기다린다. 일꾼이 없으면(초기화 전·실패) 부르는 쪽이 내부 메모리 스택일 때만 돌리고, 아니면 false.
bool run(bool (*fn)(void *), void *arg);

}  // namespace scstore
