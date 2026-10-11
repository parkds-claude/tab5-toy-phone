// voice.cpp — 목소리 통화 경로. 설명은 voice.h, 판단 로직은 voice_core.h(맥 시험 tools/test_voice_core.cpp).
//
// 근거(줄 단위 대조, 사용자 지시 2026-10-03): 이 기기에서 마이크·에코 제거·Opus 를 실제로 돌린 Phase 0 시험 펌웨어
//   firmware/tab5_phone_probe/main/probe_main.cpp 의 audio_task. 그대로 가져온 것: aec_create(16000, 4, 1, AEC_MODE_VOIP_HIGH_PERF) 와
//   묶음 크기 확인(aec_get_chunksize = 256), aec_process(aec, mic, ref, clean), 에코 제거 버퍼의 16바이트 정렬 내부 메모리,
//   Opus 압축기 설정(16kHz 모노, 24000bps, 20ms, VOIP, 복잡도 0, FEC·DTX·VBR 끔)과 esp_opus_enc_open → get_frame_size(입력 640바이트 확인)
//   → esp_opus_enc_process, 해제기 설정(16kHz 모노 20ms)과 esp_opus_dec_open → esp_opus_dec_decode(출력 버퍼 640바이트),
//   Opus 를 부르는 태스크의 스택 48KB(내부 메모리), 그리고 에코 제거·Opus 를 "그 48KB 태스크 안에서 만든다"는 것(codec_setup — 부팅 태스크의
//   스택은 8KB 이고 aec_create·esp_opus_*_open 이 스택을 얼마나 쓰는지 모른다. 거기서 넘치면 boot_guard 가 세지 못하는 재부팅 고리가 된다).
//   AEC_MODE_FD_HIGH_PERF 는 결과가 깨져 쓰지 않는다(Phase 0).
//   시험 펌웨어와 다른 점(전부 계획서 "확인하지 못한 것"에 있다):
//   PL3b-D15 태스크를 셋으로 나눴다(시험 펌웨어는 한 태스크가 읽기·에코 제거·압축·해제·쓰기를 다 했다):
//          v_cap(코어 1, 우선순위 7, 스택 32KB 내부 메모리 — 3b 는 48KB, PL3c-D1) 마이크 → 에코 제거 → 압축 / v_rx(코어 1, 7, 48KB PSRAM) 해제 / v_tx(코어 0, 5, 8KB PSRAM — PL3c-D2) 교환기로 보내기.
//          이유: 재생은 3a 의 출력 태스크가 끌어가는 구조이고, 보내기는 Wi-Fi 가 막히면 2초까지 멈춘다(그동안 마이크·재생이 멎으면 안 된다).
//          스택 48KB 는 시험 펌웨어가 세 가지(에코 제거·압축·해제)를 차례로 부르던 한 태스크의 값이다 — 둘로 나눈 각각에 그대로 준다.
//          무거운 쪽(v_cap — 묶음마다 약 5ms)은 시험 펌웨어처럼 내부 메모리 스택이다(Phase 0 의 부하 30~37% 가 그 조건에서 잰 값).
//          가벼운 쪽(v_rx — 프레임마다 약 0.3ms)의 스택은 PSRAM 에 둔다: 내부 메모리가 빠듯하다(계획서 "CPU·메모리 예산" — 에코 제거·Opus 가
//          내부 메모리를 65~80KB 쓰는 것으로 역산된다). 이 설정(CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=y, XIP from PSRAM)에서 PSRAM 스택은
//          쓸 수 있지만 이 기기에서 돌려 본 적은 없다. 탈출구: RX_STACK_CAPS 를 MALLOC_CAP_INTERNAL 로(그리고 RX_STACK 을 24KB 로 — 참고 구현
//          xiaozhi 는 Opus 압축·해제를 24KB 태스크 하나로 돌린다). 내부 메모리가 더 모자라면 CAP 쪽도 PSRAM 으로(그때는 v_aec_us·v_enc_us 를 Phase 0 과 대조).
//   PL3b-D7 해제기의 손실 은닉을 쓰지 않는다(esp_opus_dec_decode 는 frame_recover 를 읽지 않고 빈 입력을 거절한다) → voicecore::Concealer.
//          통화가 열릴 때 해제기는 esp_opus_dec_reset 으로 처음 상태로(참고 구현 xiaozhi 가 같은 호출을 쓴다). 압축기는 다시 맞추지 않는다.
//   PL3b-D11·D12·D14 진단: 보낼 소리 고르기(vsrc), 지연 재기(vping), 혼자 하는 점검(selftest). 목소리 PCM·압축 프레임을 시리얼·파일로
//          내보내는 기능은 없다 — 세기(RMS) 숫자와 셈만 나간다.
//   VB-D1 (2026-10-08, 전화기 3대 — 설계서 2026-10-08-phone-3way-design.md TW-D11·D12): 이 파일을 스택짱(ESP32-S3) 전화기도 함께 쓴다. 칩에 따라 달라야 하는 것은
//          board_profile.h 한 곳에서 가른다: 에코 제거 방식(스택짱 = VOIP_LOW_COST), 태스크의 코어·스택, 그리고 "압축을 다른 코어의 태스크로 떼기"(SPLIT_ENC).
//          Tab5 는 전과 똑같다 — 캡처 태스크가 압축까지 한다(Encoder 를 그 자리에서 부른다). 스택짱은 캡처 태스크(코어 1)가 에코 제거까지만 하고, 16ms 묶음을
//          대기열(8칸 = 128ms, PSRAM)로 압축 태스크 v_enc(코어 0)에 넘긴다. 근거: 관문 실험 — S3 는 처리 속도가 P4 의 3분의 1 이라 한 코어에 다 올리면 80~93% 였다.
//          묶음에는 통화의 세대 번호를 실어, 압축 태스크가 "새 통화"(프레임 모으기·순번을 처음부터)와 "끝난 통화의 찌꺼기"(버린다)를 가린다.
//          비용: 스택짱에서 보내는 길이 대기열 한 번(한 묶음 = 512바이트 복사)을 더 거친다 — 압축 태스크가 밀리면 묶음을 버린다(STAT v_encdrop). 탈출구: board_profile.h 의 SPLIT_ENC.
#include "voice.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>

#include "esp_aec.h"
#include "esp_audio_dec.h"
#include "esp_audio_enc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_opus_dec.h"
#include "esp_opus_enc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "audio_out.h"
#include "board_profile.h"
#include "relay_link.h"
#include "tx_dyn.h"

static_assert(voicecore::RATE == audioout::RATE && voicecore::BLOCK == audioout::BLOCK && voicecore::IN_SLOTS == audioout::IN_SLOTS,
              "voice_core.h and audio_out.h must agree on rate, block size and mic slots");
static_assert(txdyn::BLOCK == voicecore::BLOCK, "tx_dyn.h works on the same 16 ms block");
static_assert(voicetune::TXGAIN_MIN == voicecore::TX_GAIN_MIN && voicetune::TXGAIN_MAX == voicecore::TX_GAIN_MAX && voicetune::DEFAULT_TXGAIN == voicecore::TX_GAIN_DEFAULT,
              "voice_tune.h and voice_core.h must agree on the send gain (the 3b tests pin TX_GAIN_DEFAULT = 4)");

namespace voice {
namespace {

using namespace voicecore;

const char *TAG = "voice";

constexpr int AEC_FILTER_LEN = 4;                               // esp-sr 권장값(시험 펌웨어와 같다)
constexpr aec_mode_t AEC_MODE = boardprofile::AEC_LOW_COST ? AEC_MODE_VOIP_LOW_COST : AEC_MODE_VOIP_HIGH_PERF;   // Tab5 = HIGH_PERF(Phase 0: 잔향 감소 29~42dB, 묶음 256표본), 스택짱 = LOW_COST(VB-D1). FD_HIGH_PERF 는 쓰지 않는다
constexpr int OPUS_BITRATE = voicetune::DEFAULT_KBPS * 1000;   // PH-D5 — 부팅 값(voice_tune.h, 지금 24kbps). 실행 중에는 set_kbps(PL3c-D8)
constexpr int OPUS_COMPLEXITY = 0;                              // Phase 0: 5 는 부하 53.6% 로 초과, 0 은 30~37%
// PL3c-D1 캡처 태스크의 스택 48 → 32KB(내부 메모리 16KB 를 되찾는다 — 3b 최종 검수 M-11).
//   근거(기기 실측, STAT stk_vcap = 최저 여유): 만든 직후 46,380(초기화는 2.7KB 만 쓴다) → 에코 제거만 돈 뒤(selftest) 42,964(6.2KB) →
//   시험음 통화 27,716(21.4KB) → 마이크 통화 25,204~25,236(23.9KB — 밤의 3분 통화, 아침의 말소리 통화, 통화 20여 번 동안 그대로).
//   가장 깊이 쓰는 것은 Opus 압축(복잡도 0 · 24kbps)이다. 32KB 면 여유 약 8.8KB(27%).
//   비용: 여유가 24KB → 8.8KB 로 준다. 복잡도를 올리거나 Opus 가 다른 방식으로 넘어가면(64kbps 근처) 스택을 더 쓸 수 있다 — 비트율 손잡이는 40kbps 까지만(PL3c-D8).
//         넘치면 하드웨어 스택 가드가 패닉을 낸다(CONFIG_ESP_SYSTEM_HW_STACK_GUARD=y) — 통화 중에만 도는 태스크라 부팅 고리는 아니다.
//   합격 기준(기기 과제): 말소리 통화 뒤 stk_vcap >= 6000. 되돌리기: 이 숫자를 48 로.
#if defined(CONFIG_IDF_TARGET_ESP32S3)   // 스택짱(VB-D1) — 값과 근거는 board_profile.h
constexpr int CAP_CORE = boardprofile::CAP_CORE, CAP_PRIO = boardprofile::CAP_PRIO, CAP_STACK = boardprofile::CAP_STACK;
constexpr int RX_CORE = boardprofile::RX_CORE, RX_PRIO = 7, RX_STACK = 48 * 1024;
#else
constexpr int CAP_CORE = 1, CAP_PRIO = 7, CAP_STACK = 32 * 1024;   // PL3b-D15, PL3c-D1
constexpr int RX_CORE = 1, RX_PRIO = 7, RX_STACK = 48 * 1024;
#endif
constexpr UBaseType_t RX_STACK_CAPS = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;   // 해제 태스크의 스택은 PSRAM(내부 메모리 48KB 를 아낀다)
// PL3c-D2 송신 태스크의 스택(8KB)도 PSRAM 에 둔다(내부 메모리 8KB 를 되찾는다). 크기는 그대로 — 보내기 실패 때 교환기 연결의 끊김 핸들러가
//   이 스택에서 돌고(relay 태스크와 같은 크기), Phase 4 에서는 TLS 쓰기도 여기서 돈다. 해제 태스크(v_rx)가 같은 방식(PSRAM 스택)으로 3b 내내 돌았다.
//   이 태스크는 20ms 에 한 번 소켓에 60여 바이트를 쓰는 것이 전부라 스택이 느려져도 박자에 영향이 없다(무거운 캡처 태스크는 내부 메모리 그대로).
//   비용: PSRAM 스택의 태스크가 하나 더 는다. 되돌리기: TX_STACK_CAPS 를 MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT 로.
constexpr int TX_CORE = 0, TX_PRIO = 5, TX_STACK = 8 * 1024;
constexpr UBaseType_t TX_STACK_CAPS = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
constexpr uint32_t PING_TIMEOUT_MS = 3000;
constexpr uint32_t SELFTEST_HOLD_MS = 720;                         // 시험음(0.64초)이 스스로 줄어 끝난 뒤에 점검을 닫는다(중간에 끊으면 딸깍 소리)

enum class Mode : uint8_t { OFF, CALL, TEST };
struct Mutex {                                                  // voice_core.h 의 Lock: FreeRTOS 뮤텍스
    SemaphoreHandle_t h = nullptr;
    void lock() { xSemaphoreTake(h, portMAX_DELAY); }
    void unlock() { xSemaphoreGive(h); }
};
struct Lap {                                                    // 처리 시간(통화마다 reset). 쓰는 태스크 하나, 읽는 쪽은 STAT
    std::atomic<uint32_t> n{0}, sum{0}, max{0};
    void add(int64_t us) { const uint32_t u = (uint32_t)us; ++n; sum += u; if (u > max.load()) max = u; }   // 합계는 µs 로 약 71분에 넘친다 → 통화마다 reset, 넘치기 전에 반으로
    uint32_t avg() const { const uint32_t k = n.load(); return k ? sum.load() / k : 0; }
    void reset() { n = 0; sum = 0; max = 0; }
    void halve_if_big() { if (sum.load() > 0x7FFFFFFFu) { sum = sum.load() / 2; n = n.load() / 2; } }
};

std::atomic<bool> s_ready{false};
std::atomic<Mode> s_mode{Mode::OFF};
std::atomic<Src> s_src{Src::AEC};
std::atomic<bool> s_loop{false};                                // VL-D1: 되돌림 시험(loop) — 송신 태스크가 프레임을 교환기 대신 내 받는 길(on_frame)에 넣는다
SemaphoreHandle_t s_mode_lock = nullptr;                        // VL-D2: 통화 열고 닫기(set)·되돌림(loop)·되돌림 중의 프레임 넣기를 한 줄로 세운다. setup 이 만든다
struct ModeLock { ModeLock() { xSemaphoreTake(s_mode_lock, portMAX_DELAY); } ~ModeLock() { xSemaphoreGive(s_mode_lock); } };
TxGain s_tx_gain;                                               // PL3b-D23: 보내는 이득 1~4배(기본 4). 쓰기 = 시리얼 태스크, 읽기 = 캡처 태스크
std::atomic<uint32_t> s_gen{0};                                 // PL3b-D27: 통화가 열릴 때마다 오르는 세대 번호(set(true)) — 캡처 태스크가 "새 통화"를 모드와 별개로 알아본다
// PL3c-D3 통화 품질 손잡이(쓰기 = 시리얼 태스크, 읽기 = 캡처 태스크). 부팅 값은 voice_tune.h
std::atomic<int> s_kbps_want{voicetune::DEFAULT_KBPS}, s_lvl{voicetune::DEFAULT_LVL}, s_tilt{voicetune::DEFAULT_TILT}, s_res_db{voicetune::DEFAULT_RES_DB}, s_jb{voicetune::DEFAULT_JB},
                 s_vtone{voicetune::VTONE_DEFAULT};
std::atomic<int> s_guard{voicetune::GUARD_DEFAULT};             // P4H-D1: 메아리 지킴 손잡이(쓰기 = 시리얼 태스크, 읽기 = 캡처 태스크)
std::atomic<int> s_hpf{voicetune::HPF_DEFAULT};                 // P5A-D4: 저음 걷어내기(캡처 태스크가 읽는다)
std::atomic<int> s_rest{voicetune::REST_DEFAULT}, s_dt{voicetune::DT_DEFAULT}, s_rxgain{voicetune::RXGAIN_DEFAULT};   // P5A: 쉼 누르기·겹침 누르기(캡처 태스크가 읽는다)·받는 목소리 이득(Receiver 가 쥔다)
std::atomic<int> s_dyn_gain_q8{0};                              // 평탄화가 지금 걸고 있는 이득(TUNE g_q8=)
std::atomic<uint32_t> s_dyn_supp{0}, s_dyn_far{0}, s_mic_clip{0};   // s_mic_clip: 이번 통화에서 마이크 칸이 꼭대기에 닿은 표본 수(TUNE mclip=)
ArrivalGap s_rx_gap;                                            // PL3c-D4: 받은 프레임 사이의 가장 긴 틈(웹소켓 태스크가 적는다)
std::atomic<uint32_t> s_call_t0_ms{0}, s_first_under_ms{0};     // PL3b-D25: 이번 통화의 시작 시각 / 첫 굶음(v_under)이 난 시각(통화 시작 뒤 ms, 없으면 0)
Mutex s_jb_lock, s_txq_lock;
Receiver<Mutex> *s_rx = nullptr;                                // 지터 버퍼(약 13KB)가 들어 있어 PSRAM 에 둔다
TxFifo<Mutex> *s_txq = nullptr;
aec_handle_t *s_aec = nullptr;
void *s_enc = nullptr, *s_dec = nullptr;
int s_enc_out = 0;                                              // 압축기가 권하는 출력 버퍼 크기
int16_t *s_slots = nullptr, *s_mic = nullptr, *s_ref = nullptr, *s_clean = nullptr;   // 캡처 태스크에서만
TaskHandle_t s_cap_task = nullptr, s_rx_task = nullptr, s_tx_task = nullptr;
// VB-D1 (스택짱만): 캡처 태스크 → 압축 태스크로 가는 16ms 묶음 대기열. Tab5 에서는 만들지 않는다
struct EncItem { uint32_t gen; int16_t pcm[BLOCK]; };          // gen = 그 묶음이 속한 통화의 세대(s_gen)
constexpr int ENC_QUEUE_LEN = 8;                                // 8묶음 = 128ms. 그보다 밀리면 새 묶음을 버린다(묵은 소리를 몰아 보내지 않는다)
QueueHandle_t s_enc_q = nullptr;
TaskHandle_t s_enc_task = nullptr;
std::atomic<uint32_t> s_enc_drop{0};                            // 압축 태스크가 밀려 버린 묶음 수(통화마다 0 으로 — STAT v_encdrop)
SemaphoreHandle_t s_codec_done = nullptr;                       // 캡처 태스크가 codec_setup 을 끝냈다(start 가 기다린다)
const char *s_codec_fail = nullptr;                             // 실패한 단계(성공이면 nullptr)
constexpr uint32_t CODEC_SETUP_WAIT_MS = 10000;

std::atomic<uint32_t> s_cap_blocks{0}, s_tx_ok{0}, s_tx_fail{0}, s_enc_fail{0}, s_mic_rms{0}, s_ref_rms{0}, s_aec_rms{0};
Lap s_t_aec, s_t_enc, s_t_dec;

// 지연 재기(PL3b-D12): 캡처 태스크가 삑을 보내며 시각을 적고, 해제 태스크가 돌아온 삑을 찾고, app 태스크가 결과를 찍는다
std::atomic<bool> s_ping_req{false}, s_ping_armed{false};
std::atomic<uint32_t> s_ping_t0_ms{0};                          // 삑의 첫 표본이 "마이크에 들어온" 시각(ms)
std::atomic<int32_t> s_ping_result{-1};                        // 찍을 결과: -1 없음, -2 시간 초과, 그 밖 = ms

// 혼자 하는 점검(PL3b-D14)
std::atomic<bool> s_test_start{false}, s_test_done{false};
SelfTestTone s_test_tone;                                       // 출력 태스크에서만
SelfTest s_test_result;

int16_t *alloc16(int samples) { return (int16_t *)heap_caps_aligned_calloc(16, (size_t)samples, sizeof(int16_t), MALLOC_CAP_INTERNAL); }
uint32_t now_ms() { return (uint32_t)(esp_timer_get_time() / 1000); }

// ── 받는 길 ──────────────────────────────────────────────────────────────────
// audio_out 의 공급자(출력 태스크 안 — 기다리지 않는다: 링버퍼 복사와 통지뿐)
bool fill(int16_t *out, int n)
{
    if (s_test_start.exchange(false)) s_test_tone.start();
    if (s_test_tone.active()) {
        if (s_mode.load() != Mode::TEST) s_test_tone.stop();    // 점검 중에 통화가 열렸다 — 시험음을 끊는다
        else { s_test_tone.render(out, n); return true; }
    }
    const bool sound = s_rx->fill(out, n);
    if (s_first_under_ms.load() == 0 && s_rx->open() && s_rx->under() != 0) {   // PL3b-D25: 이번 통화의 첫 굶음의 때를 적는다(잠금 없이 — 읽기와 시계뿐). 닫힌 동안은 앞 통화의 셈이 남아 있어도 보지 않는다
        const uint32_t d = now_ms() - s_call_t0_ms.load();
        s_first_under_ms = d ? d : 1;                           // 0 은 "없음"이라 최소 1
    }
    if (s_rx->want_pump()) xTaskNotifyGive(s_rx_task);          // 링버퍼가 한 묶음보다 적다 → 해제 태스크를 깨운다
    return sound;
}

// relay_link 가 받은 음성 프레임을 넘긴다(웹소켓 태스크 — 복사만 하고 바로 돌아온다). 통화 밖에서 온 것은 Receiver 가 버린다.
// 되돌림 시험(VL-D1) 중에는 송신 태스크가 부른다 — 그때는 통화가 아니라 교환기가 음성을 보내지 않으므로 넣는 쪽은 여전히 하나다.
void on_frame(const uint8_t *frame, size_t len)
{
    if (s_rx->open()) s_rx_gap.hit(now_ms());                   // PL3c-D4: 통화 중에 받은 프레임 사이의 틈(통화 밖의 것은 재지 않는다)
    s_rx->push(frame, len);
    if (s_rx->want_pump()) xTaskNotifyGive(s_rx_task);          // want_pump 는 Ring::size() 를 읽는다. 여기는 쓰는 태스크도 읽는 태스크도 아닌 제3 태스크라 순간적으로 큰 값이 나올 수 있다 —
                                                                // 결과는 깨우기 한 번을 빼먹는 것뿐이고 출력 태스크가 16ms 안에 다시 깨운다(최종 검수 M-8, 동작은 그대로)
}

bool decode(const uint8_t *data, size_t len, int16_t *pcm)     // 시험 펌웨어의 esp_opus_dec_decode 호출 그대로
{
    esp_audio_dec_in_raw_t raw = {}; raw.buffer = (uint8_t *)data; raw.len = (uint32_t)len;
    esp_audio_dec_out_frame_t frame = {}; frame.buffer = (uint8_t *)pcm; frame.len = FRAME * 2;
    esp_audio_dec_info_t info = {};
    const int64_t t0 = esp_timer_get_time();
    const bool ok = esp_opus_dec_decode(s_dec, &raw, &frame, &info) == ESP_AUDIO_ERR_OK && frame.decoded_size == (uint32_t)FRAME * 2;
    s_t_dec.add(esp_timer_get_time() - t0);
    if (ok && s_ping_armed.load()) {                            // 돌아온 삑의 시작을 찾는다(해제한 때 = 링버퍼에 넣는 때. 스피커로는 그 뒤 링버퍼 + DMA 만큼 더 걸린다)
        const int at = onset(pcm, FRAME, PING_DETECT);
        if (at >= 0 && s_ping_armed.exchange(false)) s_ping_result = (int32_t)(now_ms() - s_ping_t0_ms.load());
    }
    return ok;
}

void rx_task(void *)
{
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        s_rx->pump(decode, [] { esp_opus_dec_reset(s_dec); });  // 새 통화의 첫 차례에 해제기를 처음 상태로(PL3b-D7)
        s_t_dec.halve_if_big();
    }
}

// ── 보내는 길 ────────────────────────────────────────────────────────────────
void tx_task(void *)
{
    static TxFrame f;                                           // 이 태스크에서만 쓴다
    while (true) {
        if (!s_txq->take(&f, now_ms())) { ulTaskNotifyTake(pdTRUE, portMAX_DELAY); continue; }
        if (s_mode.load() != Mode::CALL) continue;              // 통화가 끝났다 — 남은 것은 보내지 않는다
        if (s_loop.load()) {                                    // VL-D1: 되돌림 시험 — 교환기로 보내지 않는다. 평소 통화는 이 가지에 들어오지 않는다(잠금도 잡지 않는다)
            ModeLock l;                                         // VL-D2: 닫고 다시 여는 것(set)과 겹치지 않게. 그 사이에 되돌림이 끝났으면 이 프레임은 버린다
            if (s_loop.load() && s_mode.load() == Mode::CALL) { on_frame(f.data, f.len); ++s_tx_ok; }
            continue;
        }
        if (relaylink::send_voice(f.data, f.len)) ++s_tx_ok;
        else { ++s_tx_fail; s_txq->flush(); }                   // 못 보냈다(끊김·막힘) → 쌓인 것도 버린다(묵은 소리를 몰아 보내지 않는다)
    }
}

// 에코 제거·Opus 압축기·해제기와 캡처 버퍼를 만든다. 캡처 태스크(스택 48KB)의 첫 일로 한 번 — 시험 펌웨어 audio_task 의 앞부분 그대로.
// 실패하면 그 단계의 이름(성공이면 nullptr).
const char *codec_setup()
{
    s_aec = aec_create(RATE, AEC_FILTER_LEN, 1, AEC_MODE);
    if (!s_aec) return "aec";
    const int chunk = aec_get_chunksize(s_aec);
    if (chunk != BLOCK) { ESP_LOGE(TAG, "aec chunk %d != %d", chunk, BLOCK); return "aec-chunk"; }

    esp_opus_enc_config_t ecfg = ESP_OPUS_ENC_CONFIG_DEFAULT();
    ecfg.sample_rate = ESP_AUDIO_SAMPLE_RATE_16K; ecfg.channel = ESP_AUDIO_MONO; ecfg.bitrate = OPUS_BITRATE;
    ecfg.frame_duration = ESP_OPUS_ENC_FRAME_DURATION_20_MS; ecfg.application_mode = ESP_OPUS_ENC_APPLICATION_VOIP; ecfg.complexity = OPUS_COMPLEXITY;
    {   // fix 1 F3: 부품이 입출력 버퍼에 요구하는 정렬을 한 번 조회해 16 을 넘으면 알린다(동작은 바꾸지 않는다)
        esp_audio_enc_frame_info_t fi = {};
        if (esp_opus_enc_get_frame_info_by_cfg(&ecfg, &fi) == ESP_AUDIO_ERR_OK) {
            ESP_LOGI(TAG, "opus buffer align in=%u out=%u (buffers are 16-byte aligned)", (unsigned)fi.in_frame_align, (unsigned)fi.out_frame_align);
            if (fi.in_frame_align > 16 || fi.out_frame_align > 16) ESP_LOGW(TAG, "opus wants buffer alignment above 16 (in=%u out=%u)", (unsigned)fi.in_frame_align, (unsigned)fi.out_frame_align);
        }
    }
    int enc_in = 0;
    if (esp_opus_enc_open(&ecfg, sizeof ecfg, &s_enc) != ESP_AUDIO_ERR_OK || esp_opus_enc_get_frame_size(s_enc, &enc_in, &s_enc_out) != ESP_AUDIO_ERR_OK) return "opus-enc";
    // s_enc_out = 압축기가 요구하는 출력 버퍼 크기(프레임 바이트 + 100 — voice_tune.h 의 opus_out_need). 버퍼(OPUS_OUT_CAP)에 들어가야 한다.
    //   3b 는 이것을 보내기 칸(MAX_TX_FRAME)과 견줬다 — 24kbps 에서는 같은 답이지만 비트율을 올리면 요구(40kbps = 200)가 칸보다 커진다. 실제 프레임(40kbps = 100바이트)이
    //   칸에 들어가는지는 pack_voice 가 프레임마다 본다(안 들어가면 그 프레임은 v_txfail 로 센다).
    if (enc_in != FRAME * 2 || s_enc_out <= 0 || s_enc_out > voicetune::OPUS_OUT_CAP) { ESP_LOGE(TAG, "opus frame %d/%d", enc_in, s_enc_out); return "opus-size"; }
    esp_opus_dec_cfg_t dcfg = ESP_OPUS_DEC_CONFIG_DEFAULT();
    dcfg.sample_rate = ESP_AUDIO_SAMPLE_RATE_16K; dcfg.channel = ESP_AUDIO_MONO; dcfg.frame_duration = ESP_OPUS_DEC_FRAME_DURATION_20_MS;
    if (esp_opus_dec_open(&dcfg, sizeof dcfg, &s_dec) != ESP_AUDIO_ERR_OK) return "opus-dec";

    s_slots = alloc16(BLOCK * IN_SLOTS); s_mic = alloc16(BLOCK); s_ref = alloc16(BLOCK); s_clean = alloc16(BLOCK);
    if (!s_slots || !s_mic || !s_ref || !s_clean) return "buffers";
    return nullptr;
}

// PL3c-D8 Opus 비트율을 실행 중에 바꾼다. 캡처 태스크에서만, 압축과 압축 사이에 부른다 — 부품의 이 함수에는 잠금이 없다(esp_opus_enc.h:158-160 의 주의).
//   바꾼 뒤에는 압축기가 요구하는 출력 버퍼 크기를 다시 묻는다: esp_opus_enc_process 는 넘겨준 길이가 그보다 작으면 그 프레임을 압축하지 않는다(역어셈블로 확인 — 계획서 PL3c-D8).
//   인코더를 다시 열지 않는다(복잡도·VBR·FEC 는 열 때만 정해진다 — 손잡이로 두지 않았다).
bool apply_kbps(int kbps)
{
    if (esp_opus_enc_set_bitrate(s_enc, kbps * 1000) != ESP_AUDIO_ERR_OK) return false;
    int in = 0, out = 0;
    if (esp_opus_enc_get_frame_size(s_enc, &in, &out) != ESP_AUDIO_ERR_OK || in != FRAME * 2 || out <= 0 || out > voicetune::OPUS_OUT_CAP) return false;
    s_enc_out = out;
    return true;
}

// 압축 한 벌(VB-D1): 16ms 묶음을 받아 20ms 프레임으로 모으고(Uplink) Opus 로 눌러 보내기 대기열에 넣는다. 캡처 태스크의 뒷부분을 그대로 옮긴 것이다.
//   한 태스크만 쓴다: Tab5 는 캡처 태스크, 스택짱은 압축 태스크(코어 0). 비트율 손잡이(apply_kbps)도 이 태스크가 압축과 압축 사이에 건다(PL3c-D8 의 조건 그대로).
struct Encoder {
    Uplink up;
    int kbps_now = voicetune::DEFAULT_KBPS;                      // 압축기에 지금 걸려 있는 비트율
    void start() { up.start(); }                                 // 새 통화: 프레임 모으기와 순번을 처음부터
    void feed(const int16_t *blk)
    {
        // fix 1 F3: 압축기 입출력은 Phase 0 시험 펌웨어(probe_main.cpp 111~113, 154~155)와 같게 16바이트 정렬 버퍼로 준다. 압축 출력은 따로 받아 pack_voice 가 pkt 에 싣는다.
        alignas(16) static int16_t frame[FRAME];
        alignas(16) static uint8_t enc_out[voicetune::OPUS_OUT_CAP];   // PL3c-D8: 40kbps 의 요구(200)까지 담는다(3b 는 MAX_TX_FRAME = 200 — 24kbps 의 요구 160)
        static uint8_t pkt[MAX_TX_FRAME];
        if (const int want = s_kbps_want.load(); want != kbps_now) {      // PL3c-D8: 비트율 손잡이 — 압축 앞에서 건다
            if (apply_kbps(want)) kbps_now = want;
            else { ESP_LOGW(TAG, "opus bitrate %d kbps refused - back to %d", want, kbps_now); apply_kbps(kbps_now); s_kbps_want = kbps_now; }
        }
        up.push(blk);
        uint16_t seq = 0;
        while (up.pop(frame, &seq)) {                           // 시험 펌웨어의 esp_opus_enc_process 호출 그대로
            esp_audio_enc_in_frame_t in = {}; in.buffer = (uint8_t *)frame; in.len = FRAME * 2;
            esp_audio_enc_out_frame_t out = {}; out.buffer = enc_out; out.len = (uint32_t)s_enc_out;
            const int64_t t0 = esp_timer_get_time();
            const bool ok = esp_opus_enc_process(s_enc, &in, &out) == ESP_AUDIO_ERR_OK && out.encoded_bytes > 0;
            s_t_enc.add(esp_timer_get_time() - t0);
            s_t_enc.halve_if_big();
            const size_t n = ok ? relayproto::pack_voice(pkt, sizeof pkt, seq, enc_out, out.encoded_bytes) : 0;
            if (!n) { ++s_enc_fail; continue; }
            if (s_txq->put(pkt, n, now_ms())) xTaskNotifyGive(s_tx_task);
        }
    }
};
Encoder s_encoder;                                              // 캡처 태스크(Tab5) 또는 압축 태스크(스택짱)에서만 만진다

// VB-D1 (스택짱만): 압축 태스크. 캡처 태스크가 넘긴 묶음을 차례로 누른다. 끝난 통화의 묶음(세대가 다르거나 통화가 닫혔다)은 버린다.
void enc_task(void *)
{
    static EncItem it;                                          // 이 태스크에서만
    uint32_t cur = 0;                                           // 지금 누르고 있는 통화의 세대(0 = 아직 없음 — s_gen 은 첫 통화에서 1 이 된다)
    while (true) {
        if (xQueueReceive(s_enc_q, &it, portMAX_DELAY) != pdTRUE) continue;
        if (s_mode.load() != Mode::CALL || it.gen != s_gen.load()) continue;
        if (it.gen != cur) { cur = it.gen; s_encoder.start(); }
        s_encoder.feed(it.pcm);
    }
}

void cap_task(void *)
{
    s_codec_fail = codec_setup();
    xSemaphoreGive(s_codec_done);
    if (s_codec_fail) { vTaskDelete(nullptr); return; }         // 실패: start 가 알리고(AUDIO FAIL voice) 이 태스크는 사라진다

    alignas(16) static int16_t blk[BLOCK];                       // 보낼 16ms 묶음. 압축(프레임 모으기·Opus·보내기 대기열)은 Encoder 가 한다(VB-D1)
    TestTone tone, probe; PingGen ping;
    static txdyn::Dyn dyn;                                       // PL3c-D7: 보내는 크기 다듬기(손잡이 lvl·tilt·res 가 셋 다 0 이면 3b 의 고정 이득 그대로)
    static EncItem item;                                         // VB-D1 (스택짱만): 압축 태스크로 넘길 묶음
    pcm::Rms r_mic, r_ref, r_aec, t_mic, t_ref, t_aec;
    int rms_blocks = 0, test_blocks = 0;
    Mode was = Mode::OFF;
    GenWatch gen;                                               // PL3b-D27: 모드가 같아 보여도(한 묶음 안의 켜기→끄기→켜기) 세대가 오르면 새 통화다
    while (true) {
        const Mode mode = s_mode.load();
        if (mode == Mode::OFF) { was = Mode::OFF; ulTaskNotifyTake(pdTRUE, portMAX_DELAY); continue; }   // 통화 밖: 마이크를 읽지 않는다(PH-D6)
        if (gen.fresh(s_gen.load(), mode != was)) {             // 방금 켜졌다(통화 또는 점검)
            was = mode;
            if (!boardprofile::SPLIT_ENC) s_encoder.start();    // 스택짱은 압축 태스크가 세대를 보고 스스로 시작한다(VB-D1)
            s_enc_drop = 0;
            tone.reset(); probe.reset(); ping.stop();
            dyn.reset(); s_dyn_gain_q8 = 0; s_dyn_supp = 0; s_dyn_far = 0; s_mic_clip = 0;
            r_mic.take(); r_ref.take(); r_aec.take(); t_mic.take(); t_ref.take(); t_aec.take();
            rms_blocks = 0; test_blocks = 0;
        }
        if (!audioout::read_mic(s_slots, BLOCK)) { vTaskDelay(pdMS_TO_TICKS(16)); continue; }   // 실패해도 박자를 지킨다(쉬지 않고 돌면 화면이 굶는다)
        if (gen.stale(s_gen.load())) continue;                  // Phase 3c(fix 2 재검수 Minor 6): 읽는 16ms 사이에 끄기→켜기가 지나갔다 — 이 묶음은 앞 통화의 것이라 버린다(새 통화에 실리지 않게)
        ++s_cap_blocks;
        split_slots(s_slots, BLOCK, s_mic, s_ref);
        if (const uint32_t c = clipped(s_mic, BLOCK)) s_mic_clip += c;   // PL3c-D4: 마이크가 넘친 표본(진짜 마이크 소리를 센다 — probe 를 넣기 전에)
        const Src src = mode == Mode::CALL ? effective_src(s_src.load(), audioout::amp_is_on()) : Src::AEC;   // PL3b-D22: 스피커가 켜졌으면 mic·ref 는 aec 로
        const int vtone = s_vtone.load();
        tone.set_hz(vtone); probe.set_hz(vtone);
        if (src == Src::PROBE) probe.render(s_mic, BLOCK, PROBE_AMP);   // PL3c-D4: 마이크 자리에 시험음을 넣어 에코 제거를 거치게 한다(진짜 마이크 소리는 쓰이지 않는다)
        int64_t t0 = esp_timer_get_time();
        aec_process(s_aec, s_mic, s_ref, s_clean);
        s_t_aec.add(esp_timer_get_time() - t0);
        s_t_aec.halve_if_big();
        r_mic.add(s_mic, BLOCK); r_ref.add(s_ref, BLOCK); r_aec.add(s_clean, BLOCK);
        if (++rms_blocks >= RATE / BLOCK) { s_mic_rms = r_mic.take(); s_ref_rms = r_ref.take(); s_aec_rms = r_aec.take(); rms_blocks = 0; }   // 약 1초마다

        if (mode == Mode::TEST) {                               // 혼자 하는 점검: 보내지 않고 세기만 잰다
            if (s_test_done.load()) continue;
            if (++test_blocks <= SELFTEST_SKIP_BLOCKS) continue;
            t_mic.add(s_mic, BLOCK); t_ref.add(s_ref, BLOCK); t_aec.add(s_clean, BLOCK);
            if (test_blocks >= SELFTEST_SKIP_BLOCKS + SELFTEST_BLOCKS) {
                s_test_result.mic = t_mic.take(); s_test_result.ref = t_ref.take(); s_test_result.aec = t_aec.take();
                s_test_done = true;
            }
            continue;
        }

        if (s_ping_req.exchange(false)) ping.start();           // 지연 재기: 마이크 대신 "쉼 → 삑 → 쉼"
        if (ping.active()) {
            const int at = ping.render(blk, BLOCK);
            if (at >= 0) {                                      // 삑의 첫 표본이 "마이크에 들어온" 시각 = 이 묶음의 끝에서 (BLOCK − at) 표본 전
                s_ping_t0_ms = now_ms() - (uint32_t)((BLOCK - at) * 1000 / RATE);
                s_ping_armed = true;
            }
        } else if (uses_dyn(src)) {                             // PL3c-D7: 에코 제거를 거친 소리(aec·probe)는 크기 다듬기를 거친다
            pcm::Rms rr; rr.add(s_ref, BLOCK);                  //   이 묶음의 되먹임 칸 세기 = "상대 소리가 지금 나고 있나"
            memcpy(blk, s_clean, sizeof blk);
            txdyn::Params dp = {s_tx_gain.q8(), s_lvl.load(), s_res_db.load(), s_tilt.load(), s_guard.load()};
            dp.rest_db = s_rest.load(); dp.dt_db = s_dt.load();     // P5A-D1·D2
            dp.hpf = s_hpf.load();                                  // P5A-D4
            dyn.process(blk, rr.value(), dp);
            s_dyn_gain_q8 = dyn.gain_q8(); s_dyn_supp = dyn.suppressed(); s_dyn_far = dyn.far_blocks();
        } else {
            pick(src, s_clean, s_mic, s_ref, tone, blk, s_tx_gain.q8());   // 진단 출처(mic·ref·tone): 3b 그대로(PL3b-D23 의 고정 이득)
        }
        if (boardprofile::SPLIT_ENC) {                          // VB-D1: 스택짱 — 압축은 코어 0 의 태스크가 한다. 대기열이 찼으면 이 묶음을 버린다
            item.gen = gen.seen();
            memcpy(item.pcm, blk, sizeof item.pcm);
            if (xQueueSend(s_enc_q, &item, 0) != pdTRUE) ++s_enc_drop;
        } else s_encoder.feed(blk);                             // Tab5: 이 태스크가 그 자리에서 누른다(전과 같다)
    }
}

// 만들 것을 차례로 만든다. 실패하면 그 단계의 이름(성공이면 nullptr)
const char *setup()
{
    static StaticSemaphore_t jb_buf, txq_buf, mode_buf;
    s_mode_lock = xSemaphoreCreateMutexStatic(&mode_buf);        // VL-D2 (정적이라 만들기가 실패하지 않는다)
    s_jb_lock.h = xSemaphoreCreateMutexStatic(&jb_buf);
    s_txq_lock.h = xSemaphoreCreateMutexStatic(&txq_buf);
    void *rx_mem = heap_caps_malloc(sizeof(Receiver<Mutex>), MALLOC_CAP_SPIRAM);
    void *txq_mem = heap_caps_malloc(sizeof(TxFifo<Mutex>), MALLOC_CAP_SPIRAM);
    if (!rx_mem || !txq_mem) return "memory";
    s_rx = new (rx_mem) Receiver<Mutex>(s_jb_lock);
    s_txq = new (txq_mem) TxFifo<Mutex>(s_txq_lock);
    if (voicetune::DEFAULT_JB) s_rx->set_policy(jitterbuf::tuned_policy(voicetune::DEFAULT_JB));   // PL3c-D5: 부팅 값이 0 이면 건드리지 않는다(3b 의 규칙)
    s_rx->set_play_gain_q8(voicetune::RXGAIN_DEFAULT * 256 / 100);                                 // P5A-D3: 받는 목소리의 재생 이득(부팅 값)

    // 에코 제거·Opus 는 캡처 태스크가 자기 스택(48KB)에서 만든다 — 끝날 때까지 여기서 기다린다. 통화 밖이라 그 뒤 태스크는 잠든다.
    static StaticSemaphore_t done_buf;
    s_codec_done = xSemaphoreCreateBinaryStatic(&done_buf);
    if (xTaskCreatePinnedToCore(cap_task, "v_cap", CAP_STACK, nullptr, CAP_PRIO, &s_cap_task, CAP_CORE) != pdPASS) return "task-cap";
    if (xSemaphoreTake(s_codec_done, pdMS_TO_TICKS(CODEC_SETUP_WAIT_MS)) != pdTRUE) return "codec-timeout";
    if (s_codec_fail) { s_cap_task = nullptr; return s_codec_fail; }

    if (boardprofile::SPLIT_ENC) {                              // VB-D1 (스택짱만): 묶음 대기열(PSRAM)과 압축 태스크(스택은 내부 메모리 — 압축이 가장 무거운 일이다)
        static StaticQueue_t enc_q_ctl;                         // 제어 블록(잠금이 든다)은 내부 메모리에, 묶음 저장소(4KB)만 PSRAM 에
        uint8_t *store = (uint8_t *)heap_caps_malloc((size_t)ENC_QUEUE_LEN * sizeof(EncItem), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        s_enc_q = store ? xQueueCreateStatic(ENC_QUEUE_LEN, sizeof(EncItem), store, &enc_q_ctl) : nullptr;
        if (!s_enc_q) return "enc-queue";
        if (xTaskCreatePinnedToCore(enc_task, "v_enc", boardprofile::ENC_STACK, nullptr, boardprofile::ENC_PRIO, &s_enc_task, boardprofile::ENC_CORE) != pdPASS) return "task-enc";
    }
    if (xTaskCreatePinnedToCoreWithCaps(rx_task, "v_rx", RX_STACK, nullptr, RX_PRIO, &s_rx_task, RX_CORE, RX_STACK_CAPS) != pdPASS) return "task-rx";
    if (xTaskCreatePinnedToCoreWithCaps(tx_task, "v_tx", TX_STACK, nullptr, TX_PRIO, &s_tx_task, TX_CORE, TX_STACK_CAPS) != pdPASS) return "task-tx";
    if (!audioout::add_source(fill)) return "source";
    relaylink::set_voice_sink(on_frame);
    return nullptr;
}

}  // namespace

bool start()
{
    if (s_ready) return true;
    if (!audioout::ready()) return false;                       // 소리 장치가 없다 — 통화 흐름은 그대로, 목소리만 없다
    const char *stage = setup();
    if (stage) {
        ESP_LOGE(TAG, "voice setup failed at '%s'", stage);
        audioout::fail("voice");                                // STAT audio=0, boot_guard 가 다시 켜 본다(PL3b-D2)
        return false;
    }
    s_ready = true;
    printf("VOICE ready chunk=%d opus_in=%d opus_out_max=%d bitrate=%d aec=%s\n", BLOCK, FRAME * 2, s_enc_out, OPUS_BITRATE, aec_get_mode_string(AEC_MODE));
    return true;
}

// VL-D1 되돌림 시험(loop): 통화 밖에서 통화의 소리 길 전부(마이크 → 에코 제거 → 압축 → [되돌림] → 지터 버퍼 → 해제 → 스피커)를 상대 없이 돌려 본다.
//   이유: 상대 전화기를 받아 줄 사람이 없을 때도 통화 중의 부하·스택·메모리·밀린 소리를 재려고(2026-10-09 스택짱 첫 설치 — 통화 0회인 채로 올렸다).
//   교환기를 거치지 않으므로 망 쪽 부하(음성 프레임의 TLS 송수신)는 들어 있지 않다. 스피커 소리가 마이크로 되돌아 다시 나가는 고리가 생기니
//   (에코 제거 하나만 고리 안에 있다) 재는 동안은 보통 `vsrc tone`(시험음을 보낸다 — 마이크가 고리에 들지 않는다)과 함께 쓴다.
//   진짜 통화가 시작되면(app 이 set 을 부른다) 되돌림은 조용히 끝나고 통화가 열린다 — 시험 깃발이 통화를 망가뜨리지 않는다.
//   비용: 송신 태스크에 분기 하나. 탈출구: `vloop off`, 또는 재부팅.
// VL-D2 set·loop 는 같은 잠금(s_mode_lock) 안에서 돈다 — 부르는 태스크가 둘이 됐다(app 의 통화 진행 + 시리얼 명령). 되돌림을 닫을 때는 모드를 먼저 닫고
//   깃발을 나중에 내린다(거꾸로면 그 틈에 송신 태스크가 되돌림 때의 프레임을 교환기로 보낸다). 송신 태스크는 되돌림 가지에서만 이 잠금을 잡고 깃발을 다시 본다.
//   남는 틈: 송신 태스크가 프레임을 꺼낸 직후 통째로 "닫고 새 통화 열기"가 끼어들면 그 프레임 하나(20ms)가 새 통화로 나간다 — 통화가 잇달아 열릴 때
//   원래 있던 틈과 같은 모양이고, 되돌림을 켠 채 전화를 받았을 때만 생긴다. 평소 통화의 길(잠금 없이 send_voice)은 그대로 둔다.
static bool set_mode(bool on)
{
    if (!s_ready) return false;
    if (on) {
        if (s_mode.load() == Mode::CALL) return false;
        s_src = Src::AEC; s_ping_req = false; s_ping_armed = false; s_ping_result = -1;
        s_txq->flush(); s_txq->clear_dropped();
        s_tx_ok = 0; s_tx_fail = 0; s_enc_fail = 0;
        s_t_aec.reset(); s_t_enc.reset(); s_t_dec.reset();
        s_call_t0_ms = now_ms(); s_first_under_ms = 0;          // PL3b-D25: rx->start() 앞에서 — 그 뒤에야 open 이 참이 되어 공급자가 첫 굶음을 적는다
        s_rx_gap.reset();                                       // PL3c-D4: 틈은 통화마다 다시 잰다
        s_rx->start();                                          // 지터 버퍼를 비우고 받기 시작한다
        s_gen.fetch_add(1);                                     // PL3b-D27: 모드를 CALL 로 바꾸기 직전에 올린다 — 캡처 태스크가 CALL 을 보면 새 세대도 반드시 보인다
        s_mode = Mode::CALL;
        xTaskNotifyGive(s_cap_task);                            // 마이크를 연다(캡처 태스크가 읽기 시작한다 — 앞 128ms 는 버린다)
        audioout::wake();                                       // 출력 태스크가 목소리 공급자를 부르기 시작한다
        return true;
    }
    if (s_mode.load() != Mode::CALL) return false;
    s_mode = Mode::OFF;                                         // 캡처 태스크는 읽던 묶음(최대 16ms)을 끝으로 잠든다. 송신 태스크는 남은 것을 버린다
    s_rx->stop();                                               // 받는 쪽: 더 받지 않는다. 출력은 5ms 에 걸쳐 줄이고 끝난다
    s_txq->flush();
    s_src = Src::AEC; s_ping_armed = false;
    audioout::wake();
    return true;
}

bool set(bool on)
{
    if (!s_ready) return false;
    ModeLock l;                                                 // VL-D2
    bool closed = false;
    if (s_loop.load()) { closed = set_mode(false); s_loop = false; }   // 되돌림 시험 중이었다 → 모드를 먼저 닫고 깃발을 내린 뒤 요청대로(VL-D1·D2)
    return on ? set_mode(true) : (set_mode(false) || closed);
}

bool loop(bool on)
{
    if (!s_ready) return false;
    ModeLock l;                                                 // VL-D2
    if (on) {
        if (s_mode.load() == Mode::CALL) return false;          // 통화 중이거나 이미 되돌림 중
        s_loop = true;
        if (set_mode(true)) return true;
        s_loop = false;
        return false;
    }
    if (!s_loop.load()) return false;
    const bool closed = set_mode(false);                        // 모드를 먼저 닫고 깃발을 나중에(VL-D2)
    s_loop = false;
    return closed;
}
bool looping() { return s_loop.load(); }

bool on() { return s_mode.load() == Mode::CALL; }
uint32_t rx_frames() { return s_ready ? s_rx->frames() : 0; }

bool set_source(Src s)
{
    if (s_mode.load() != Mode::CALL) return false;
    s_src = s;
    return true;
}

bool set_tx_gain(int n) { return s_tx_gain.set(n); }
int tx_gain() { return s_tx_gain.get(); }

// ── Phase 3c 통화 품질 손잡이(PL3c-D3) ──
bool set_kbps(int kbps) { if (!voicetune::ok_kbps(kbps)) return false; s_kbps_want = kbps; return true; }
bool set_lvl(int max_gain) { if (!voicetune::ok_lvl(max_gain)) return false; s_lvl = max_gain; return true; }
bool set_tilt(int n) { if (!voicetune::ok_tilt(n)) return false; s_tilt = n; return true; }
bool set_res(int db) { if (!voicetune::ok_res_db(db)) return false; s_res_db = db; return true; }
bool set_guard(int n) { if (!voicetune::ok_guard(n)) return false; s_guard = n; return true; }   // P4H-D1
int guard() { return s_guard.load(); }
bool set_rest(int db) { if (!voicetune::ok_rest(db)) return false; s_rest = db; return true; }      // P5A-D1
int rest() { return s_rest.load(); }
bool set_dt(int db) { if (!voicetune::ok_dt(db)) return false; s_dt = db; return true; }            // P5A-D2
int dt() { return s_dt.load(); }
bool set_rxgain(int percent)                                                                       // P5A-D3
{
    if (!voicetune::ok_rxgain(percent) || !s_ready) return false;
    s_rx->set_play_gain_q8(percent * 256 / 100);
    s_rxgain = percent;
    return true;
}
int rxgain() { return s_rxgain.load(); }
bool set_hpf(int on) { if (!voicetune::ok_hpf(on)) return false; s_hpf = on; return true; }           // P5A-D4
int hpf() { return s_hpf.load(); }
bool set_jb(int cap)
{
    if (!voicetune::ok_jb(cap) || !s_ready) return false;
    s_rx->set_policy(cap ? jitterbuf::tuned_policy(cap) : jitterbuf::legacy_policy());     // 지터 버퍼 잠금 안에서 바뀐다(통화 중에도)
    s_jb = cap;
    return true;
}
bool set_vtone(int hz) { if (!voicetune::ok_vtone(hz)) return false; s_vtone = hz; return true; }
void tune(Tune *o)
{
    o->kbps = s_kbps_want.load(); o->lvl = s_lvl.load(); o->tilt = s_tilt.load(); o->res_db = s_res_db.load(); o->jb = s_jb.load(); o->vtone = s_vtone.load();
    o->gain_q8 = s_dyn_gain_q8.load(); o->supp = s_dyn_supp.load(); o->far = s_dyn_far.load(); o->rxgap_ms = s_rx_gap.max_ms(); o->mic_clip = s_mic_clip.load();
}

bool ping()
{
    if (s_mode.load() != Mode::CALL || s_ping_armed.load()) return false;
    s_ping_result = -1;
    s_ping_req = true;
    return true;
}

bool selftest(SelfTest *out)
{
    Mode expect = Mode::OFF;
    if (!s_ready || !out || !s_mode.compare_exchange_strong(expect, Mode::TEST)) return false;   // 통화 중에는 하지 않는다
    s_gen.fetch_add(1);                                         // Phase 3c(fix 2 재검수 범위 밖 1): 점검도 세대를 올린다 — 16ms 안에 연달아 온 둘째 점검이 첫째의 셈(test_blocks)을 이어받지 않게
    const uint32_t t0 = now_ms();
    s_test_done = false; s_test_start = true;
    xTaskNotifyGive(s_cap_task);
    audioout::wake();
    for (int i = 0; i < 100 && !s_test_done.load() && s_mode.load() == Mode::TEST; ++i) vTaskDelay(pdMS_TO_TICKS(20));   // 최대 2초
    const bool done = s_test_done.load();
    while (done && s_mode.load() == Mode::TEST && (uint32_t)(now_ms() - t0) < SELFTEST_HOLD_MS) vTaskDelay(pdMS_TO_TICKS(20));
    expect = Mode::TEST;
    s_mode.compare_exchange_strong(expect, Mode::OFF);          // 그 사이 통화가 열렸으면 건드리지 않는다
    if (done) *out = s_test_result;
    return done;
}

void print_pending()
{
    int32_t r = s_ping_result.exchange(-1);
    if (r == -1 && s_ping_armed.load() && (uint32_t)(now_ms() - s_ping_t0_ms.load()) > PING_TIMEOUT_MS && s_ping_armed.exchange(false)) r = -2;
    if (r >= 0) {                                               // PL3b-D25: 줄 끝에 그때의 지터 버퍼(깊이/목표)를 덧붙인다 — 앞의 글자는 그대로
        const RxStats rs = s_rx->stats();
        printf("VPING rt_ms=%d jb=%d/%d\n", (int)r, rs.depth, rs.target);
    }
    else if (r == -2) printf("VPING timeout\n");
}

void stat(Stat *o)
{
    *o = Stat{};
    o->ready = s_ready; o->on = on(); o->src = s_src.load(); o->tx_gain = s_tx_gain.get();
    if (!s_ready) return;
    o->cap = s_cap_blocks; o->tx = s_tx_ok; o->tx_drop = s_txq->dropped(); o->tx_fail = s_tx_fail.load() + s_enc_fail.load();
    o->rx = s_rx->stats();
    o->under_ms = s_first_under_ms.load();
    o->aec_us = s_t_aec.avg(); o->aec_max = s_t_aec.max; o->enc_us = s_t_enc.avg(); o->enc_max = s_t_enc.max; o->dec_us = s_t_dec.avg(); o->dec_max = s_t_dec.max;
    o->mic_rms = s_mic_rms; o->ref_rms = s_ref_rms; o->aec_rms = s_aec_rms;
    o->rd_err = audioout::read_errors();
    o->stk_cap = (unsigned)uxTaskGetStackHighWaterMark(s_cap_task);
    o->stk_rx = (unsigned)uxTaskGetStackHighWaterMark(s_rx_task);
    o->stk_tx = (unsigned)uxTaskGetStackHighWaterMark(s_tx_task);
    o->stk_enc = s_enc_task ? (unsigned)uxTaskGetStackHighWaterMark(s_enc_task) : 0;   // VB-D1: 압축 태스크는 스택짱에만 있다
    o->enc_drop = s_enc_drop.load();
}

}  // namespace voice
