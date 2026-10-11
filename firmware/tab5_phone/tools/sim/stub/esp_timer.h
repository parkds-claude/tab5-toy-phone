// 맥의 화면 미리보기용 대역: ESP-IDF 의 esp_timer.h 자리. 시각은 sim_main.cpp 가 정한다(늘 같은 값 → 그림이 실행마다 같다).
#pragma once
#include <cstdint>
int64_t esp_timer_get_time();
