// sc_gfx.cpp — 설명은 sc_gfx.h.
//   SG-D1 합성 버퍼에 그린 뒤 한 번에 내보낸다(Tab5 gfx.cpp 와 같은 방식 — docs/design/README.md §2). 이유: 화면에 직접 그리면 그리는 순서가 깜빡임으로 보인다.
//         비용: PSRAM 154KB × 2(화면 찍기용 사본 포함). 탈출구: 없음(스택짱의 PSRAM 은 8MB).
//   SG-D2 조각 내보내기는 M5GFX 의 자르기 범위(setClipRect)에 버퍼 전체를 밀어 넣는 방식이다 — M5GFX 가 범위 밖을 보내지 않는다.
//         이유: 버퍼의 줄 간격(320)을 그대로 쓴다(조각을 따로 복사하지 않는다). 탈출구: 조각을 줄마다 writePixels 로.
//   SG-D3 색 순서: 버퍼는 CPU 순서의 RGB565(raster.h)다 → lgfx::rgb565_t 로 넘겨 M5GFX 가 패널 순서로 바꾸게 한다(setSwapBytes 설정에 기대지 않는다).
//   SG-D4 화면 찍기는 present 때마다 사본을 뜨지 않고, 찍을 때 잠금 안에서 버퍼를 한 번 복사한다(1ms 쯤). 그리기와 present 가 한 잠금 구간이라
//         버퍼는 늘 "마지막으로 내보낸 화면"과 같다.
#include "sc_gfx.h"

#include <M5Unified.h>
#include <cstring>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "toy_colors.h"

namespace scgfx {
namespace {

const char *TAG = "sc_gfx";
constexpr int W = sclayout::W, H = sclayout::H;
constexpr size_t BYTES = (size_t)W * H * 2;

raster::Canvas s_cv = { nullptr, W, H };
uint16_t *s_shot = nullptr;
SemaphoreHandle_t s_lock = nullptr;

}  // namespace

bool init()
{
    static StaticSemaphore_t lock_buf;
    if (s_cv.px) return true;
    if (M5.Display.width() != W || M5.Display.height() != H) {
        M5.Display.setRotation(1);                                       // CoreS3 의 기본(가로 320×240)
        if (M5.Display.width() != W || M5.Display.height() != H) { ESP_LOGE(TAG, "init fail: panel %dx%d", (int)M5.Display.width(), (int)M5.Display.height()); return false; }
    }
    s_lock = xSemaphoreCreateMutexStatic(&lock_buf);
    s_cv.px = (uint16_t *)heap_caps_malloc(BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_shot = (uint16_t *)heap_caps_malloc(BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_cv.px || !s_shot) { ESP_LOGE(TAG, "init fail: buffers"); heap_caps_free(s_cv.px); heap_caps_free(s_shot); s_cv.px = nullptr; s_shot = nullptr; return false; }
    raster::fill(s_cv, toy::SKY);
    present();
    return true;
}

const raster::Canvas &frame_begin()
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    return s_cv;
}

void present()
{
    if (!s_cv.px) return;
    M5.Display.pushImage(0, 0, W, H, (const lgfx::rgb565_t *)s_cv.px);  // SG-D3
}

void present(const sclayout::Rect &r)
{
    if (!s_cv.px) return;
    M5.Display.setClipRect(r.x, r.y, r.w, r.h);                          // SG-D2
    M5.Display.pushImage(0, 0, W, H, (const lgfx::rgb565_t *)s_cv.px);
    M5.Display.clearClipRect();
}

void frame_end() { xSemaphoreGive(s_lock); }

size_t snapshot(const uint8_t **out)
{
    if (!s_cv.px || !out) return 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);                               // SG-D4
    memcpy(s_shot, s_cv.px, BYTES);
    xSemaphoreGive(s_lock);
    *out = (const uint8_t *)s_shot;
    return BYTES;
}

}  // namespace scgfx
