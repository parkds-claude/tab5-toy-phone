// 맥의 화면 미리보기용 대역: ESP-IDF 의 esp_log.h 자리. 로그는 찍지 않는다.
#pragma once
#define ESP_LOGE(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGD(tag, ...) ((void)(tag))
