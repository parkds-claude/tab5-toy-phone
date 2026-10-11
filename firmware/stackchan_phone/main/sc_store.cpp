// sc_store.cpp — 설명은 sc_store.h.
#include "sc_store.h"

#include <cstring>

#include "esp_log.h"
#include "esp_memory_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace scstore {
namespace {

const char *TAG = "sc_store";
const char *PART = "nvs_phone", *NS = "phone";
constexpr int WORKER_STACK = 8 * 1024, WORKER_PRIO = 3, WORKER_CORE = 0;   // 가장 깊은 일 = 스트림덱으로 갈 때의 앱 이미지 검사(esp_ota_set_boot_partition). 여유는 STAT stk_flash 로 본다
bool s_ready = false;

struct Job { bool (*fn)(void *); void *arg; bool result; TaskHandle_t waiter; };
QueueHandle_t s_jobs = nullptr;
TaskHandle_t s_worker = nullptr;
SemaphoreHandle_t s_one = nullptr;                  // 한 번에 한 일만(일꾼의 답을 받는 통지가 섞이지 않게)

void worker(void *)
{
    Job *j = nullptr;
    while (true) {
        if (xQueueReceive(s_jobs, &j, portMAX_DELAY) != pdTRUE || !j) continue;
        j->result = j->fn(j->arg);
        xTaskNotifyGive(j->waiter);
    }
}

bool on_internal_stack() { int here = 0; return esp_ptr_internal(&here); }

struct Rw { const char *key; uint8_t value; bool write, erase; };
bool do_rw(void *p)
{
    Rw *r = (Rw *)p;
    nvs_handle_t h;
    if (nvs_open_from_partition(PART, NS, (r->write || r->erase) ? NVS_READWRITE : NVS_READONLY, &h) != ESP_OK) return false;   // 한 번도 쓴 적이 없으면 읽기용으로는 열리지 않는다
    bool ok;
    if (r->erase) { const esp_err_t e = nvs_erase_key(h, r->key); ok = (e == ESP_OK || e == ESP_ERR_NVS_NOT_FOUND) && nvs_commit(h) == ESP_OK; }
    else if (r->write) ok = nvs_set_u8(h, r->key, r->value) == ESP_OK && nvs_commit(h) == ESP_OK;
    else ok = nvs_get_u8(h, r->key, &r->value) == ESP_OK;
    nvs_close(h);
    return ok;
}

}  // namespace

bool init()
{
    static StaticSemaphore_t one_buf;
    if (s_jobs) return s_ready;
    s_one = xSemaphoreCreateMutexStatic(&one_buf);
    s_jobs = xQueueCreate(1, sizeof(Job *));
    if (!s_jobs || xTaskCreatePinnedToCore(worker, "flash", WORKER_STACK, nullptr, WORKER_PRIO, &s_worker, WORKER_CORE) != pdPASS) {
        ESP_LOGE(TAG, "flash worker not started");
        if (s_jobs) { vQueueDelete(s_jobs); s_jobs = nullptr; }
    }
    esp_err_t e = nvs_flash_init_partition(PART);
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "%s unusable (%s) -> erase", PART, esp_err_to_name(e));
        nvs_flash_erase_partition(PART);
        e = nvs_flash_init_partition(PART);
    }
    if (e != ESP_OK) { ESP_LOGE(TAG, "%s init failed: %s", PART, esp_err_to_name(e)); return false; }
    s_ready = true;
    return true;
}

bool ready() { return s_ready; }
unsigned stack_free() { return s_worker ? (unsigned)uxTaskGetStackHighWaterMark(s_worker) : 0; }

bool run(bool (*fn)(void *), void *arg)
{
    if (!fn) return false;
    if (on_internal_stack()) return fn(arg);        // 그 자리에서(main 태스크의 부팅 길, 일꾼 자신)
    if (!s_jobs) return false;
    xSemaphoreTake(s_one, portMAX_DELAY);
    Job j = { fn, arg, false, xTaskGetCurrentTaskHandle() };
    Job *p = &j;                                    // j 는 이 태스크의 (PSRAM) 스택에 있다 — 일꾼이 읽고 쓰는 것은 캐시가 켜져 있을 때뿐이다(fn 안의 플래시 구간 밖)
    ulTaskNotifyTake(pdTRUE, 0);                    // 남아 있던 통지를 비운다
    xQueueSend(s_jobs, &p, portMAX_DELAY);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    xSemaphoreGive(s_one);
    return j.result;
}

uint8_t get_u8(const char *key, uint8_t fallback)
{
    if (!s_ready) return fallback;
    Rw r = { key, fallback, false, false };
    return run(do_rw, &r) ? r.value : fallback;
}

bool set_u8(const char *key, uint8_t value)
{
    if (!s_ready) return false;
    Rw r = { key, value, true, false };
    return run(do_rw, &r);
}

bool erase(const char *key)
{
    if (!s_ready) return false;
    Rw r = { key, 0, false, true };
    return run(do_rw, &r);
}

}  // namespace scstore
