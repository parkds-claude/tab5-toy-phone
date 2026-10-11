// gfx.cpp — 화면 합성·출력의 기기 쪽: 합성 버퍼·PPA 출력·스크린샷·내장 글꼴 글자. 도형·글자 자산·색 그림은 gfx_draw.cpp(raster.h)가 그린다.
//   2026-10-08 토이폰 테마: 픽셀 합성(rgb565·blend565·알파 도형·글자 자산·숫자)을 raster.h + gfx_draw.cpp 로 옮겼다 — 맥의 화면 미리보기(tools/sim)가
//   같은 코드로 그리게 하려는 것. 둥근 도형도 스프라이트(fillSmooth*) 대신 raster.h 가 그린다(R-D1). 스프라이트(s_ov)는 내장 글꼴 글자에만 쓴다.
// 출처: firmware/tab5_snapbooth/main/main.cpp (실기 검증된 합성기)에서 카메라와 얽힌 부분을 걷어낸 것.
//   가져온 것: 픽셀 합성(rgb565·blend565·알파 사각형/둥근 사각형/원·글자 자산 블릿·숫자), 스프라이트 묶기와 바이트 순서 자가 검증,
//             PPA 로 세로 패널 프레임버퍼에 한 번에 회전 출력, 하드웨어 JPEG 인코더(화면 캡처).
//   걷어낸 것: 합성 버퍼 2장 교대(swap_compose_buffer), 비동기 PPA 와 완료 콜백(ppa_done_cb), 정지 프레임(s_frozen),
//             카메라 복사용 PPA(ppa_copy_frame), JPEG 디코더, s_proc_mutex.
//
// 결정
//   G-D4(I-1) 비밀번호가 든 자판 화면은 스크린샷으로 내보내지 않는다. 프레임마다 "비공개" 표시를 두고(clear() 가 거짓, 그리는 쪽이 set_private),
//        present() 가 s_lock 안에서 그 값을 "내보낸 화면의 값"으로 확정한다. screenshot_jpeg() 의 두 길 모두 그 값이 참이면 인코딩 없이 0 + is_private.
//        비용: 비밀번호 입력 중에는 화면을 못 찍는다. 탈출구: ui_layout.h is_private_frame() 이 false 만 돌려주게 한다.
//   G-D1 합성 버퍼는 1장, present() 는 동기(끝날 때까지 기다림).
//        이유: 전화기 화면은 카메라처럼 초당 수십 장을 내지 않는다. 버퍼 교대가 없으면 "지금 그리는 버퍼 = 내보낼 버퍼" 하나라 단순하다.
//        비용: present() 한 번(약 60~80ms) 동안 화면 태스크가 멈춘다. 탈출구: 원본의 2장 교대 + PPA_TRANS_MODE_NON_BLOCKING 으로 되돌린다.
//   G-D2 화면 캡처는 "마지막으로 내보낸 화면"만 찍는다 — 그리는 도중의 반쪽 화면을 찍지 않는다.
//        방법: 그리기 함수가 s_dirty 를 켜고 present() 가 끈다. 캡처 요청이 왔을 때 s_dirty 가 꺼져 있으면 그 자리에서 인코딩하고,
//              켜져 있으면(또는 인코딩 도중 켜지면) 곧 올 present() 가 내보낸 직후 대신 인코딩해 준다(최대 SHOT_WAIT_MS 기다림).
//        이유: 그리기 함수에는 잠금이 없다(화면 태스크 전용). 버퍼를 한 장 더 두고 매번 복사하면 present 마다 1.8MB 복사가 든다.
//        비용: 그리기 함수마다 원자 변수 쓰기 1회. 탈출구: s_dirty 검사를 지우면 "잠금만 있는" 단순한 캡처가 된다.
//   G-D3 PPA 회전 각도는 M5.Display.getRotation() 에서 정한다(3 → 90°, 1 → 270°). 원본의 SNAP_ROT180 매크로와 같은 대응.
#include "gfx.h"

#include <M5Unified.h>

#include <atomic>

#include "driver/jpeg_encode.h"
#include "driver/ppa.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lgfx/v1/platforms/esp32p4/Panel_DSI.hpp"

namespace {

const char *TAG = "gfx";

constexpr int    PANEL_W        = 720;                        // 물리 패널(세로)
constexpr int    PANEL_H        = 1280;
constexpr size_t BUF_BYTES      = (size_t)SCREEN_W * SCREEN_H * 2;   // RGB565
constexpr size_t CACHE_ALIGN    = 128;                        // sdkconfig: CONFIG_CACHE_L2_CACHE_LINE_128B
constexpr size_t JPEG_OUT_BYTES = 1024 * 1024;                // 원본과 같은 1MB(화면 한 장은 보통 100KB 안쪽)
constexpr int    JPEG_QUALITY   = 85;                         // 원본 시리얼 캡처와 같은 값
constexpr int    SHOT_WAIT_MS   = 1000;                       // 그리는 중일 때 다음 present() 를 기다리는 한도

uint8_t *s_buf = nullptr;                                     // 합성 버퍼(가로 1280×720, native LE RGB565)
raster::Canvas s_canvas = { nullptr, SCREEN_W, SCREEN_H };    // 같은 버퍼를 gfx_draw.cpp 에 건네는 모양(init 이 채운다)
uint8_t *s_fb = nullptr;                                      // DSI 프레임버퍼(세로 720×1280). init 이 다 끝나야 채워진다 → 공개 함수는 이것으로 준비 여부를 본다
size_t   s_fb_size = 0;
ppa_client_handle_t s_ppa = nullptr;
ppa_srm_rotation_angle_t s_rot = PPA_SRM_ROTATION_ANGLE_90;
lgfx::LGFX_Sprite s_ov;                                       // 합성 버퍼에 묶인 스프라이트(안티앨리어싱 도형·내장 글꼴)

SemaphoreHandle_t s_lock = nullptr;                           // present() ↔ screenshot_jpeg()
SemaphoreHandle_t s_shot_done = nullptr;                      // present() 가 대신 인코딩을 끝냈다
std::atomic<bool> s_dirty{false};                             // 마지막 present() 뒤로 합성 버퍼에 그린 것이 있다
std::atomic<bool> s_shot_req{false};                          // 다음 present() 에서 인코딩해 달라
uint32_t s_shot_len = 0;                                      // s_lock 보호
bool s_shot_priv = false;                                     // s_lock 보호 — 대리 인코딩이 비공개라서 건너뛰었다
bool s_priv_draft = false;                                    // 지금 그리는 프레임의 비공개 표시(화면 태스크 전용 — clear() 가 거짓으로, set_private 가 설정)
bool s_priv_shown = false;                                    // 마지막으로 present() 한 프레임의 비공개 표시(s_lock 보호) — G-D4

jpeg_encoder_handle_t s_jenc = nullptr;
uint8_t *s_jenc_out = nullptr;
size_t   s_jenc_cap = 0;

inline void mark() { s_dirty.store(true); }

// 합성 버퍼 → JPEG (s_jenc_out). 돌려주는 값 0 = 실패. s_lock 을 쥔 채로 부른다.
uint32_t encode_locked()
{
    if (!s_jenc || !s_jenc_out) return 0;
    jpeg_encode_cfg_t cfg = {};
    cfg.height = SCREEN_H;
    cfg.width = SCREEN_W;
    cfg.src_type = JPEG_ENCODE_IN_FORMAT_RGB565;
    cfg.sub_sample = JPEG_DOWN_SAMPLING_YUV420;
    cfg.image_quality = JPEG_QUALITY;
    uint32_t len = 0;
    esp_err_t e = jpeg_encoder_process(s_jenc, &cfg, s_buf, BUF_BYTES, s_jenc_out, s_jenc_cap, &len);
    if (e != ESP_OK) { ESP_LOGE(TAG, "jpeg encode failed: %s", esp_err_to_name(e)); return 0; }
    return len;
}

// 스프라이트를 합성 버퍼에 묶고 바이트 순서를 자가 검증한다(원본 bind_overlay_sprite).
// 직접 쓰는 픽셀(blend565)과 PPA·JPEG 인코더는 native LE RGB565 를 기대한다 → 스프라이트도 같은 순서로 써야 색이 맞는다.
bool bind_sprite()
{
    // 버퍼가 묶인 채 setColorDepth 를 부르면 LovyanGFX 가 같은 크기의 버퍼를 새로 잡으려 든다(1.8MB) → 먼저 비운다.
    // (우리 버퍼는 "미리 잡아 준 버퍼"로 묶이므로 deleteSprite 가 해제하지 않는다)
    s_ov.deleteSprite();
    s_ov.setColorDepth(lgfx::color_depth_t::rgb565_nonswapped);
    s_ov.setBuffer(s_buf, SCREEN_W, SCREEN_H, 0);
    s_ov.fillRect(0, 0, 2, 1, (uint32_t)0xFF0000u);           // 빨강 한 점을 찍어 실제로 어떤 바이트가 들어갔나 본다
    uint16_t raw = *(uint16_t *)s_buf;
    if (raw != 0xF800) {
        ESP_LOGW(TAG, "sprite byte order raw=0x%04X → rgb565_2Byte 로 다시 묶음", raw);
        s_ov.deleteSprite();
        s_ov.setColorDepth(lgfx::color_depth_t::rgb565_2Byte);
        s_ov.setBuffer(s_buf, SCREEN_W, SCREEN_H, 0);
        s_ov.fillRect(0, 0, 2, 1, (uint32_t)0xFF0000u);
        raw = *(uint16_t *)s_buf;
    }
    s_ov.setTextWrap(false);
    ESP_LOGI(TAG, "sprite bound (native RGB565 %s)", raw == 0xF800 ? "OK" : "MISMATCH");
    return raw == 0xF800;
}

bool init_fail(const char *stage)
{
    ESP_LOGE(TAG, "init fail: %s", stage);                    // 로그에는 "gfx: init fail: <단계>" 로 찍힌다
    return false;
}

}  // namespace

namespace gfx {

bool init()
{
    if (s_fb) return true;                                    // 이미 끝남

    // 1) 패널: Tab5 이고 가로 1280×720 으로 돌려져 있어야 한다(패널 감지가 실패하면 폭이 0 이다)
    const auto board = M5.getBoard();
    if (board != m5::board_t::board_M5Tab5 && board != m5::board_t::board_M5Tab5X) return init_fail("board (Tab5 가 아님)");
    if (M5.Display.width() != SCREEN_W || M5.Display.height() != SCREEN_H) return init_fail("panel (1280x720 가로가 아님 — 패널 감지 실패 또는 setRotation 누락)");
    switch (M5.Display.getRotation()) {                       // G-D3
    case 3: s_rot = PPA_SRM_ROTATION_ANGLE_90; break;
    case 1: s_rot = PPA_SRM_ROTATION_ANGLE_270; break;
    default: return init_fail("rotation (1 또는 3 만 지원)");
    }

    // 2) DSI 프레임버퍼: PPA 가 여기에 직접 쓴다(128바이트 정렬 필수). s_fb 는 준비가 다 끝난 뒤에 넣는다(그 전에는 present 가 아무것도 안 함)
    auto *panel = static_cast<lgfx::Panel_DSI *>(M5.Display.getPanel());
    uint8_t *fb = panel ? (uint8_t *)panel->config_detail().buffer : nullptr;
    if (!fb || ((uintptr_t)fb & (CACHE_ALIGN - 1)) != 0) return init_fail("dsi framebuffer");

    // 3) 잠금
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    if (!s_shot_done) s_shot_done = xSemaphoreCreateBinary();
    if (!s_lock || !s_shot_done) return init_fail("semaphore");

    // 4) 합성 버퍼(PSRAM, 캐시 줄 정렬) + 스프라이트
    if (!s_buf) s_buf = (uint8_t *)heap_caps_aligned_calloc(CACHE_ALIGN, 1, BUF_BYTES, MALLOC_CAP_SPIRAM);
    if (!s_buf) return init_fail("compose buffer (PSRAM 1.8MB)");
    if (!bind_sprite()) return init_fail("sprite byte order");
    s_canvas.px = (uint16_t *)s_buf;

    // 5) PPA (회전·복사 한 번에). 동기 전용이라 대기 1건이면 충분하다
    if (!s_ppa) {
        ppa_client_config_t pc = {};
        pc.oper_type = PPA_OPERATION_SRM;
        pc.max_pending_trans_num = 1;
        pc.data_burst_length = PPA_DATA_BURST_LENGTH_128;
        if (ppa_register_client(&pc, &s_ppa) != ESP_OK) { s_ppa = nullptr; return init_fail("ppa client"); }
    }

    // 6) JPEG 인코더(화면 캡처용). 엔진은 칩에 하나뿐이다. 실패해도 화면은 쓸 수 있으므로 init 은 성공으로 둔다
    if (!s_jenc) {
        jpeg_encode_engine_cfg_t ec = {};
        ec.intr_priority = 0;
        ec.timeout_ms = 1000;
        if (jpeg_new_encoder_engine(&ec, &s_jenc) != ESP_OK) s_jenc = nullptr;
    }
    if (s_jenc && !s_jenc_out) {
        jpeg_encode_memory_alloc_cfg_t mc = {};
        mc.buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER;
        s_jenc_out = (uint8_t *)jpeg_alloc_encoder_mem(JPEG_OUT_BYTES, &mc, &s_jenc_cap);
    }
    if (!s_jenc || !s_jenc_out) ESP_LOGW(TAG, "init warn: jpeg encoder — 화면 캡처(screenshot_jpeg)는 0 을 돌려준다");

    s_fb_size = (size_t)PANEL_W * PANEL_H * 2;
    s_fb = fb;                                                // 여기부터 present() 가 동작한다
    ESP_LOGI(TAG, "init ok: compose %p, dsi fb %p, rotation %d", s_buf, s_fb, (int)M5.Display.getRotation());
    clear(BOOT_FILL);
    present();                                                // 부팅 로고·쓰레기 대신 배경색 한 장
    return true;
}

void set_private(bool on) { s_priv_draft = on; }

// gfx_draw.cpp 의 연결점: 그리기 함수마다 한 번 부른다 → "그렸다" 표시(G-D2)가 여기 한 곳에서 켜진다.
raster::Canvas *detail::target()
{
    if (!s_fb) return nullptr;
    mark();
    return &s_canvas;
}

void detail::frame_begin() { s_priv_draft = false; }           // G-D4: 프레임은 clear() 로 시작한다 → 기본값은 공개

void present()
{
    if (!s_fb) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    ppa_srm_oper_config_t op = {};
    op.in.buffer = s_buf;
    op.in.pic_w = SCREEN_W;  op.in.pic_h = SCREEN_H;
    op.in.block_w = SCREEN_W; op.in.block_h = SCREEN_H;
    op.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
    op.out.buffer = s_fb;    op.out.buffer_size = s_fb_size;
    op.out.pic_w = PANEL_W;  op.out.pic_h = PANEL_H;
    op.out.block_offset_x = 0; op.out.block_offset_y = 0;
    op.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
    op.rotation_angle = s_rot;
    op.scale_x = 1.0f; op.scale_y = 1.0f;
    op.alpha_update_mode = PPA_ALPHA_NO_CHANGE;
    op.mode = PPA_TRANS_MODE_BLOCKING;
    esp_err_t err = ppa_do_scale_rotate_mirror(s_ppa, &op);
    if (err != ESP_OK) ESP_LOGW(TAG, "ppa srm failed: %s", esp_err_to_name(err));
    s_dirty.store(false);                                     // 이제 합성 버퍼 = 화면
    s_priv_shown = s_priv_draft;                              // G-D4: 방금 내보낸 프레임의 비공개 값을 여기서 확정한다
    if (s_shot_req.exchange(false)) {                         // G-D2: 캡처가 기다리고 있으면 지금(그리기가 멈춘 순간) 인코딩
        s_shot_priv = s_priv_shown;
        s_shot_len = s_shot_priv ? 0 : encode_locked();
        xSemaphoreGive(s_shot_done);
    }
    xSemaphoreGive(s_lock);
}

int text(const char *s, int x, int y, uint32_t rgb, float size)
{
    if (!s || !s_fb) return 0;
    mark();
    s_ov.setFont(&fonts::efontKR_16_b);
    s_ov.setTextSize(size);
    s_ov.setTextDatum(textdatum_t::middle_left);
    s_ov.setTextColor(rgb);
    s_ov.drawString(s, x, y);
    return s_ov.textWidth(s);
}

int text_w(const char *s, float size)
{
    if (!s || !s_fb) return 0;
    s_ov.setFont(&fonts::efontKR_16_b);
    s_ov.setTextSize(size);
    return s_ov.textWidth(s);
}

void text_center(const char *s, int cx, int cy, uint32_t rgb, float size)
{
    if (!s || !s_fb) return;
    mark();
    s_ov.setFont(&fonts::efontKR_16_b);
    s_ov.setTextSize(size);
    s_ov.setTextDatum(textdatum_t::middle_center);
    s_ov.setTextColor(rgb);
    s_ov.drawString(s, cx, cy);
}

size_t screenshot_jpeg(const uint8_t **out, bool *is_private)
{
    if (out) *out = nullptr;
    if (is_private) *is_private = false;
    if (!out || !s_fb || !s_jenc || !s_jenc_out) return 0;
    uint32_t len = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool clean = !s_dirty.load();
    if (clean) {                                              // 합성 버퍼 = 마지막으로 내보낸 화면 → 바로 찍는다
        if (s_priv_shown) {                                   // G-D4: 그 화면이 비공개면 인코딩하지 않는다
            xSemaphoreGive(s_lock);
            if (is_private) *is_private = true;
            return 0;
        }
        len = encode_locked();
        clean = !s_dirty.load();                              // 찍는 사이 화면 태스크가 다음 화면을 그리기 시작했으면 섞였을 수 있다 → 버린다
    }
    if (clean) {
        xSemaphoreGive(s_lock);
    } else {                                                  // 그리는 중 → 곧 올 present() 가 내보낸 직후 대신 찍어 준다
        xSemaphoreTake(s_shot_done, 0);                       // 지난 신호 비우기
        s_shot_req.store(true);
        xSemaphoreGive(s_lock);
        if (xSemaphoreTake(s_shot_done, pdMS_TO_TICKS(SHOT_WAIT_MS)) != pdTRUE) {
            xSemaphoreTake(s_lock, portMAX_DELAY);
            const bool unanswered = s_shot_req.exchange(false);   // 아직 켜져 있으면 present() 가 오지 않은 것
            xSemaphoreGive(s_lock);
            if (unanswered) { ESP_LOGW(TAG, "screenshot: 그리는 중인데 %dms 안에 present() 가 없었다", SHOT_WAIT_MS); return 0; }
            xSemaphoreTake(s_shot_done, 0);                   // 시간 초과 직후 present() 가 처리했다 → 그 결과를 쓴다
        }
        xSemaphoreTake(s_lock, portMAX_DELAY);
        len = s_shot_len;
        const bool priv = s_shot_priv;
        xSemaphoreGive(s_lock);
        if (priv) { if (is_private) *is_private = true; return 0; }
    }
    if (!len) return 0;
    *out = s_jenc_out;
    return len;
}

}  // namespace gfx
