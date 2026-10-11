// device_id.cpp — 칩의 MAC 을 읽어 secrets.h 의 기기 표에서 이 기기를 찾는다. 찾는 로직은 device_id.h(맥에서 시험).
#include "device_id.h"

#include "esp_mac.h"

#include "secrets.h"

namespace deviceid {

static const Entry TABLE[] = PHONE_DEVICES;

const Info &get()
{
    static Info info;
    static bool done = false;
    if (!done) {                                                  // app_main 이 태스크를 띄우기 전에 처음 부른다
        uint8_t m[6] = {};
        char s[18] = "";
        if (esp_efuse_mac_get_default(m) == ESP_OK) format_mac(m, s);
        info = lookup(s, TABLE, (int)(sizeof TABLE / sizeof TABLE[0]));
        done = true;
    }
    return info;
}

}  // namespace deviceid
