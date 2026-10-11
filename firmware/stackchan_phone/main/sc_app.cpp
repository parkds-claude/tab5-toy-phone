// sc_app.cpp — 설명은 sc_app.h. 흐름은 Tab5 app.cpp 와 같다:
//   app 태스크가 20ms 마다: 교환기 사건 꺼내기 → 터치 꺼내기 → call_fsm 에 넣기 → 할 일 실행(소리 → 목소리 → 신호 보내기) → 그릴 내용이 바뀌었으면 다시 그리기.
//   call_fsm 은 app 태스크에서만 만진다. 다른 태스크가 보는 것은 STAT 용 사본(s_stat)뿐이다.
// 스택짱에서 다른 점:
//   SA-D1 화면은 바뀐 만큼만 다시 그린다: 통화 시간만 바뀌었으면 시계 조각(4ms), 음량 단계만 바뀌었으면 음량 조각, 그 밖에는 통째(35ms).
//         이유: 관문 실험 조건 3 — 통화 중 화면은 가볍게(숫자 몇 줄을 1초마다 고쳐 그리는 것만으로 코어 1 의 12% 였다). 조각 그리기가 통째 그리기와 픽셀까지 같다는 것은
//         맥의 화면 미리보기(tools/sim/sc_sim.cpp)가 견준다.
//   SA-D2 상대가 둘이다: 통화 키캡의 slot 을 그대로 TAP_CALL 의 peer 인자로 넣는다(call_fsm.h FSM-D3). 받은 peer·ring 의 기기 ID → slot 은 roster::route_slot(Tab5 와 같은 길).
//   SA-D3 스트림덱으로 가기(sc_ui_layout.h SL-D2): 대기·연결 중 화면에서만, 두 번 눌러야. 가기 전에 "스트림덱으로 가는 중" 한 장을 그리고 교환기 연결은 그대로 둔 채 재시작한다
//         (교환기는 연결이 끊긴 것으로 본다 — 상대 화면에 "잠듦"). 부팅 대상 바꾸기가 실패하면 그 장을 지우고 대기 화면으로 돌아온다.
//   SA-D4 이 태스크의 스택은 PSRAM 에 둔다(내부 메모리가 빠듯하다 — 관문 실험 조건 4). 그래서 플래시를 만지는 일(음량 단계 저장, 부팅 대상 바꾸기)은 scstore 의 일꾼이 한다(sc_store.h SS-D1).
//         비용: 그리기가 조금 느리다(스택 접근이 캐시를 거친다). 탈출구: APP_STACK_CAPS 를 MALLOC_CAP_INTERNAL 로.
#include "sc_app.h"

#include <M5Unified.h>
#include <atomic>
#include <cstdio>
#include <cstring>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "audio_out.h"
#include "call_fsm.h"
#include "net_wifi.h"
#include "relay_link.h"
#include "relay_proto.h"
#include "roster.h"
#include "sc_boot.h"
#include "sc_gfx.h"
#include "sc_store.h"
#include "sc_ui.h"
#include "sc_ui_layout.h"
#include "tone_out.h"
#include "tones.h"
#include "voice.h"
#include "vol_steps.h"

using namespace callfsm;
using sclayout::Model;
using sclayout::Tap;

static_assert(sclayout::MAX_PEERS == roster::MAX_PEERS, "the idle screen has one call key per peer slot");

namespace scapp {
namespace {

const char *TAG = "sc_app";

constexpr int APP_CORE = 1, APP_PRIO = 4, APP_STACK = 12 * 1024;          // Tab5 와 같은 코어·우선순위(소리 출력 6·캡처 7 보다 아래)
constexpr int TOUCH_CORE = 1, TOUCH_PRIO = 5, TOUCH_STACK = 4 * 1024;
constexpr UBaseType_t APP_STACK_CAPS = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;   // SA-D4

enum : uint8_t { REQ_TAP = 0, REQ_DECK = 0xFE };                            // TapEv::vol_req 의 특별한 값(1~4 는 volsteps::Req)
struct TapEv { int16_t x, y; uint8_t vol_req, vol_arg; };

Fsm s_fsm;
Model s_model, s_drawn;
bool s_drawn_valid = false;
sclayout::TapGuard s_guard;
sclayout::DeckArm s_deck_arm;
bool s_to_deck = false;
const char *s_screen = "BOOT";
int64_t s_screen_since = 0;
sclayout::Screen s_screen_now = sclayout::Screen::CONNECTING, s_screen_prev = sclayout::Screen::CONNECTING;   // 터치 가드의 길이를 고른다(SL-D4)
Notice s_notice = Notice::NONE;
uint8_t s_notice_msg = 0;
int64_t s_notice_until = 0;
int s_unit = 0;
voicecore::RxWatch s_rx_watch;
int s_vol_step = volsteps::STEP_DEFAULT;
volsteps::Saver s_vol_saver;

SemaphoreHandle_t s_i2c = nullptr, s_stat_lock = nullptr;
QueueHandle_t s_tap_q = nullptr;
TaskHandle_t s_app_task = nullptr, s_touch_task = nullptr;
Model s_stat;
const char *s_stat_screen = "BOOT";
std::atomic<uint32_t> s_touches{0};

int64_t now_ms() { return esp_timer_get_time() / 1000; }

void apply(const Out &o, State before, int64_t now)
{
    tones::Tone tone;
    if (tones::from_acts(o.acts, &tone)) toneout::play(tone);   // PL3a-D13: 보내기보다 먼저
    bool talk = false;                                           // PL3b-D13: 목소리도 보내기보다 먼저(끊으면 마이크가 바로 닫힌다)
    if (voicecore::from_acts(o.acts, &talk) && voice::set(talk)) {
        printf("VOICE %s\n", talk ? "on" : "off");
        if (talk) s_rx_watch.start((uint32_t)now);               // PL3c-D10
    }
    static const uint16_t SENDS[] = { A_SEND_CALL, A_SEND_ACCEPT, A_SEND_REJECT, A_SEND_HANGUP };
    for (uint16_t bit : SENDS) {
        if (!(o.acts & bit)) continue;
        char buf[relayproto::SEND_BUF];
        const char *to = roster::id_of_slot(s_unit, o.peer);     // 상대 slot(Out.peer) → 교환기 기기 ID. 걸기 글자에만 실린다
        const char *text = relayproto::send_text_for(bit, buf, sizeof buf, to, o.msg);
        if (!text) { ESP_LOGE(TAG, "signal 0x%x not sent: no device id for peer slot %u (unit %d)", bit, (unsigned)o.peer, s_unit); continue; }
        if (bit == A_SEND_CALL) printf("CALL slot=%u to=%s\n", (unsigned)o.peer, to);
        if (!relaylink::send_text(text)) ESP_LOGW(TAG, "signal 0x%x not sent (link down)", bit);
    }
    if (o.acts & A_RECONNECT) relaylink::reconnect();
    if (o.notice != Notice::NONE) { s_notice = o.notice; s_notice_msg = o.msg; s_notice_until = now + sclayout::notice_ms(o.notice); }
    if (s_fsm.state != before || o.acts || o.notice != Notice::NONE) {
        char tail[12] = "";
        if (o.msg) snprintf(tail, sizeof tail, " msg=%u", (unsigned)o.msg);
        printf("FSM %s -> %s acts=0x%x notice=%s%s\n", state_name(before), state_name(s_fsm.state), (unsigned)o.acts, notice_name(o.notice), tail);
    }
}

void feed(Ev ev, int64_t now, Why why = Why::NONE, bool flag = false, uint8_t msg = 0, uint8_t peer = 0)
{
    const State before = s_fsm.state;
    apply(s_fsm.on(ev, now, why, flag, msg, peer), before, now);
    if (s_fsm.state != State::IDLE && s_fsm.state != State::OFFLINE) s_deck_arm.cancel();   // 전화가 오가면 "한 번 더!"를 푼다(SL-D2)
}

// ── 음량 단계(Tab5 app.cpp 의 P4V 와 같은 길. 저장 자리만 nvs_phone 칸 — scstore) ───────────────
const char *VOL_KEY = "volstep";
int vol_load() { return volsteps::from_saved(scstore::get_u8(VOL_KEY, 0)); }
void vol_line()
{
    char line[64];
    volsteps::format_line(line, sizeof line, s_vol_step, audioout::volume(), s_vol_saver.is_saved(s_vol_step));
    printf("%s\n", line);
}
void vol_do(volsteps::Req req, int arg, int64_t now)
{
    if (req != volsteps::Req::SHOW) {
        const int target = volsteps::apply(s_vol_step, req, arg);
        if (audioout::volume() != volsteps::vol_of(target)) {
            const audioout::VolResult r = audioout::set_volume(volsteps::vol_of(target));
            if (r != audioout::VolResult::OK) {
                printf("VOLSTEP FAIL %s step=%d\n", r == audioout::VolResult::NO_AUDIO ? "no-audio" : r == audioout::VolResult::I2C ? "i2c" : "range", s_vol_step);
                return;
            }
        }
        s_vol_step = target;
        s_vol_saver.changed(target, now);
    }
    vol_line();
}

sclayout::Link link_now()
{
    using sclayout::Link;
    if (!s_unit) return Link::UNREGISTERED;
    switch (netwifi::phase()) {
    case netwifi::Phase::STARTING:    return Link::WIFI_STARTING;
    case netwifi::Phase::SEARCHING:   return Link::WIFI_SEARCHING;
    case netwifi::Phase::NO_KNOWN:    return Link::WIFI_NONE;
    case netwifi::Phase::INIT_FAILED: return Link::WIFI_FAIL;
    case netwifi::Phase::CONNECTED:   break;
    }
    if (relaylink::up()) return Link::UP;
    if (relaylink::http_code() == 401) return Link::AUTH_FAIL;
    return relaylink::attempt() >= 2 ? Link::RELAY_RETRY : Link::RELAY_CONNECTING;
}

// 통화 시간·음량 단계를 뺀 나머지가 같은가 — 다르면 화면을 통째로 다시 그린다(SA-D1)
bool same_frame(const Model &a, const Model &b)
{
    for (int i = 0; i < sclayout::MAX_PEERS; ++i) if (a.peer_unit[i] != b.peer_unit[i] || a.peer_online[i] != b.peer_online[i]) return false;
    return a.fsm == b.fsm && a.link == b.link && a.unit == b.unit && a.call_peer == b.call_peer && a.missed == b.missed
        && a.notice == b.notice && a.notice_msg == b.notice_msg && a.deck_ok == b.deck_ok && a.deck_armed == b.deck_armed && a.to_deck == b.to_deck;
}

void refresh(int64_t now)
{
    Model m;
    m.fsm = s_fsm.state; m.link = link_now(); m.unit = s_unit;
    for (int i = 0; i < sclayout::MAX_PEERS; ++i) { m.peer_unit[i] = (uint8_t)roster::peer_unit(s_unit, i); m.peer_online[i] = s_fsm.online(i); }
    m.call_peer = s_fsm.call_peer;
    m.missed = s_fsm.missed;
    m.talk_seconds = s_fsm.state == State::IN_CALL ? (int)((now - s_fsm.call_start_ms) / 1000) : 0;
    if (s_notice != Notice::NONE && now >= s_notice_until) { s_notice = Notice::NONE; s_notice_msg = 0; }
    m.notice = s_notice; m.notice_msg = s_notice_msg;
    m.vol_step = (uint8_t)s_vol_step;
    m.deck_ok = scboot::has_deck(); m.deck_armed = s_deck_arm.armed(now); m.to_deck = s_to_deck;
    s_model = m;

    const sclayout::Screen sc = sclayout::screen_for(s_model);
    const char *screen = sclayout::screen_name(sc);
    if (strcmp(screen, s_screen) != 0) { s_screen_prev = s_screen_now; s_screen_now = sc; s_screen = screen; s_screen_since = now; printf("SCREEN %s\n", screen); }
}

void paint()                                                                // 그릴 내용이 바뀐 만큼만(SA-D1). 그리기와 내보내기는 한 잠금 구간이다(화면 찍기가 반쪽을 찍지 않게)
{
    const bool full = !s_drawn_valid || !same_frame(s_model, s_drawn);
    const bool clock = !full && s_model.talk_seconds != s_drawn.talk_seconds && sclayout::screen_for(s_model) == sclayout::Screen::IN_CALL;
    const bool vol = !full && s_model.vol_step != s_drawn.vol_step && sclayout::volume_visible(s_model);
    if (full || clock || vol) {
        const raster::Canvas &cv = scgfx::frame_begin();
        if (full) { scui::draw(cv, s_model); scgfx::present(); }
        else {
            if (clock) { scui::draw_clock(cv, s_model); scgfx::present(sclayout::CLOCK_AREA); }
            if (vol) { scui::draw_volume(cv, s_model); scgfx::present(sclayout::VOLUME_DIRTY); }
        }
        scgfx::frame_end();
    }
    s_drawn = s_model; s_drawn_valid = true;
}

// 스트림덱으로 간다(SA-D3). 대기·연결 중에만. 성공하면 돌아오지 않는다.
void go_deck(int64_t now)
{
    if (s_fsm.state != State::IDLE && s_fsm.state != State::OFFLINE) { printf("DECK FAIL busy\n"); return; }
    if (!scboot::has_deck()) { printf("DECK FAIL no-deck\n"); return; }
    printf("DECK go\n");
    s_to_deck = true; refresh(now); paint();                                // "스트림덱으로 가는 중..." 한 장
    toneout::play(tones::Tone::NONE);
    if (!scboot::to_deck()) {                                               // 실패: 그 장을 지우고 돌아온다
        printf("DECK FAIL set-boot\n");
        s_to_deck = false; s_drawn_valid = false;
    }
}

void on_tap(int x, int y, int64_t now)
{
    ++s_touches;
    const Tap t = sclayout::hit(s_model, x, y);
    if (t == Tap::NONE) return;                                             // 빗나간 터치는 가드를 쓰지 않는다(바로 뒤의 정확한 터치를 살린다)
    if (!s_guard.allow(now, s_screen_since, sclayout::screen_guard_for(s_screen_prev, s_screen_now))) return;   // 벨 화면 직후의 대기 화면은 1.5초(SL-D4), 그 밖은 0.6초
    if (t != Tap::DECK) s_deck_arm.cancel();
    switch (t) {
    case Tap::CALL_A:
    case Tap::CALL_B: feed(Ev::TAP_CALL, now, Why::NONE, false, 0, (uint8_t)sclayout::slot_of(t)); break;   // SA-D2
    case Tap::CANCEL: feed(Ev::TAP_CANCEL, now); break;
    case Tap::ACCEPT: feed(Ev::TAP_ACCEPT, now); break;
    case Tap::REJECT: feed(Ev::TAP_REJECT, now); break;
    case Tap::HANGUP: feed(Ev::TAP_HANGUP, now); break;
    case Tap::VOL_DN:
    case Tap::VOL_UP: vol_do(sclayout::vol_req_of(t), 0, now); break;
    case Tap::DECK:   if (s_deck_arm.tap(now)) go_deck(now); break;        // SL-D2: 두 번째 누름에만 간다
    case Tap::NONE:   break;
    }
}

void touch_task(void *)                                                     // AP-D2: 화면을 내보내는 동안 닿았다 뗀 짧은 터치를 놓치지 않게 따로 읽는다
{
    while (true) {
        xSemaphoreTake(s_i2c, portMAX_DELAY); M5.update(); xSemaphoreGive(s_i2c);
        auto t = M5.Touch.getDetail();
        if (t.wasPressed()) { TapEv e{ (int16_t)t.x, (int16_t)t.y, REQ_TAP, 0 }; xQueueSend(s_tap_q, &e, 0); }   // AP-D1: 닿은 순간
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_task(void *)
{
    while (true) {
        const int64_t now = now_ms();

        toneout::print_pending();
        voice::print_pending();

        relaylink::In in;                                                   // 1) 교환기 사건
        while (relaylink::poll(&in)) {
            if (in.kind == relaylink::In::Kind::LINK_UP) feed(Ev::LINK_UP, now);
            else if (in.kind == relaylink::In::Kind::LINK_DOWN) feed(Ev::LINK_DOWN, now);
            else {
                const relayproto::Signal &sg = in.sig;
                const int slot = roster::route_slot(s_unit, relayproto::about_peer(sg), sg.peer_id);
                if (slot < 0) { printf("SIGDROP ev=%s id=%s\n", sg.ev == Ev::PEER ? "peer" : "ring", sg.peer_id[0] ? sg.peer_id : "-"); continue; }
                if (sg.ev == Ev::PEER) printf("PEER slot=%d id=%s online=%d\n", slot, roster::id_of_slot(s_unit, slot), (int)sg.flag);
                else if (sg.ev == Ev::RING) printf("RING slot=%d from=%s\n", slot, roster::id_of_slot(s_unit, slot));
                feed(sg.ev, now, sg.why, sg.flag, sg.msg, (uint8_t)slot);
            }
        }
        refresh(now);                                                       //    방금 바뀐 화면을 기준으로 터치를 판정한다

        TapEv tap;                                                          // 2) 터치·시리얼 요청
        while (xQueueReceive(s_tap_q, &tap, 0) == pdTRUE) {
            if (tap.vol_req == REQ_DECK) go_deck(now);
            else if (tap.vol_req != REQ_TAP) vol_do((volsteps::Req)tap.vol_req, tap.vol_arg, now);
            else on_tap(tap.x, tap.y, now);
            refresh(now);
        }

        feed(Ev::TICK, now);                                                // 3) 시간 제한
        if (s_fsm.state == State::IN_CALL && voice::on() && s_rx_watch.check((uint32_t)now, voice::rx_frames()))
            feed(Ev::VOICE_LOST, now);                                      //    PL3c-D10
        // P4V-D2: 음량 단계 저장 — 마지막으로 바뀐 뒤 3초가 지났고 통화·벨·신호음 중이 아닐 때 한 번(플래시에 쓰는 동안 소리가 끊길 수 있다)
        if (s_vol_saver.should_write(now, (s_fsm.state == State::IDLE || s_fsm.state == State::OFFLINE) && toneout::now() == tones::Tone::NONE)) {
            const bool ok = scstore::set_u8(VOL_KEY, (uint8_t)s_vol_step);
            s_vol_saver.wrote(s_vol_step, ok);
            if (!ok) ESP_LOGW(TAG, "volume step %d not saved - it is kept until the next reboot only", s_vol_step);
            vol_line();
        }

        refresh(now);                                                       // 4) 그릴 내용 → 5) 그리기
        xSemaphoreTake(s_stat_lock, portMAX_DELAY); s_stat = s_model; s_stat_screen = s_screen; xSemaphoreGive(s_stat_lock);
        paint();
        scboot::alive();                                                    // 한 바퀴 돌았다(sc_boot.cpp SB-D6: 20초째 없으면 다시 켜진다)
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

BaseType_t create(TaskFunction_t fn, const char *name, int stack, int prio, TaskHandle_t *h, int core)
{
    if (xTaskCreatePinnedToCoreWithCaps(fn, name, stack, nullptr, prio, h, core, APP_STACK_CAPS) == pdPASS) return pdPASS;
    ESP_LOGW(TAG, "%s: stack in internal memory (PSRAM stack not available)", name);
    return xTaskCreatePinnedToCore(fn, name, stack, nullptr, prio, h, core);
}

}  // namespace

void start(int unit, SemaphoreHandle_t i2c_lock)
{
    s_unit = unit;
    s_i2c = i2c_lock;
    s_stat_lock = xSemaphoreCreateMutex();
    s_tap_q = xQueueCreate(8, sizeof(TapEv));
    s_vol_step = vol_load();
    s_vol_saver.boot(s_vol_step);
    if (audioout::ready() && audioout::volume() != volsteps::vol_of(s_vol_step) && audioout::set_volume(volsteps::vol_of(s_vol_step)) != audioout::VolResult::OK)
        ESP_LOGW(TAG, "saved volume step %d not applied (amp write failed) - playing at %d", s_vol_step, audioout::volume());
    vol_line();
    if (create(app_task, "app", APP_STACK, APP_PRIO, &s_app_task, APP_CORE) != pdPASS) printf("PHONE FAIL app-task\n");
    if (create(touch_task, "touch", TOUCH_STACK, TOUCH_PRIO, &s_touch_task, TOUCH_CORE) != pdPASS) printf("PHONE FAIL touch-task\n");
}

void inject_tap(int x, int y)
{
    if (!s_tap_q) return;
    TapEv e{ (int16_t)x, (int16_t)y, REQ_TAP, 0 };
    xQueueSend(s_tap_q, &e, 0);
}

void vol_apply_saved()
{
    const int step = vol_load();
    if (!volsteps::boot_needs_write(audioout::ready(), audioout::volume(), step)) return;
    if (audioout::set_volume(volsteps::vol_of(step)) == audioout::VolResult::OK) ESP_LOGI(TAG, "saved volume step %d applied before the sound tasks start", step);
    else ESP_LOGW(TAG, "saved volume step %d not applied yet (amp write failed) - scapp::start tries again", step);
}

bool vol_request(int req, int arg)
{
    if (!s_tap_q || !volsteps::req_valid(req)) return false;
    TapEv e{ 0, 0, (uint8_t)req, (uint8_t)arg };
    return xQueueSend(s_tap_q, &e, 0) == pdTRUE;
}

bool deck_request()
{
    if (!s_tap_q) return false;
    TapEv e{ 0, 0, REQ_DECK, 0 };
    return xQueueSend(s_tap_q, &e, 0) == pdTRUE;
}

void print_stat()
{
    Model m; const char *screen = "BOOT";
    if (s_stat_lock) { xSemaphoreTake(s_stat_lock, portMAX_DELAY); m = s_stat; screen = s_stat_screen; xSemaphoreGive(s_stat_lock); }
    unsigned stk_relay = 0, stk_ws = 0;
    relaylink::stack_free(&stk_relay, &stk_ws);
    uint32_t aud_avg = 0, aud_max = 0;
    audioout::block_us(&aud_avg, &aud_max);
    voice::Stat v;
    voice::stat(&v);
    // 줄의 앞부분(… vol=)은 Tab5 의 STAT 과 칸 이름·순서가 같다 — 같은 도구(tools/phone_ctl.py 의 읽는 부분)로 읽는다. peer= 는 slot 0 의 상태다.
    // 스택짱에만 있는 칸은 줄 끝에 더한다: peer1(slot 1 의 상태) call_peer(지금·직전 통화의 상대 slot) deck(스트림덱이 있나) stk_venc(압축 태스크) stk_flash(플래시 일꾼) link(sclayout::Link 의 값)
    printf("STAT up=%u unit=%d fsm=%s screen=%s wifi=%d relay=%d relay_code=%d peer=%d missed=%u touch=%u heap_int=%u heap_int_min=%u heap_psram=%u"
           " stk_app=%u stk_touch=%u stk_relay=%u stk_ws=%u audio=%d tone=%s aud_us=%u/%u aud_err=%u stk_audio=%u"
           " amp=%d aud_late=%u voice=%d vsrc=%s v_cap=%u v_tx=%u v_txdrop=%u v_txfail=%u v_rx=%u v_play=%u v_lost=%u v_drop=%u v_under=%u v_jb=%d/%d"
           " v_aec_us=%u/%u v_enc_us=%u/%u v_dec_us=%u/%u v_mic=%u v_ref=%u v_aec=%u v_spk=%u v_rderr=%u stk_vcap=%u stk_vrx=%u stk_vtx=%u"
           " v_urun=%u v_ovf=%u v_catch=%u v_late=%u v_dup=%u v_bad=%u aud_gap=%u v_under_ms=%u txgain=%d vol=%d"
           " peer1=%d call_peer=%u deck=%d stk_venc=%u v_encdrop=%u stk_flash=%u link=%d\n",
           (unsigned)(esp_timer_get_time() / 1000000), m.unit, state_name(m.fsm), screen, (int)netwifi::connected(), (int)relaylink::up(), relaylink::http_code(),
           (int)m.peer_online[0], (unsigned)m.missed, (unsigned)s_touches.load(),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           s_app_task ? (unsigned)uxTaskGetStackHighWaterMark(s_app_task) : 0, s_touch_task ? (unsigned)uxTaskGetStackHighWaterMark(s_touch_task) : 0,
           stk_relay, stk_ws, (int)audioout::ready(), tones::tone_name(toneout::now()), (unsigned)aud_avg, (unsigned)aud_max,
           (unsigned)audioout::write_errors(), audioout::stack_free(),
           (int)audioout::amp_is_on(), (unsigned)audioout::late_blocks(), (int)v.on, voicecore::src_name(v.src), (unsigned)v.cap,
           (unsigned)v.tx, (unsigned)v.tx_drop, (unsigned)v.tx_fail, (unsigned)v.rx.frames, (unsigned)v.rx.played, (unsigned)v.rx.concealed,
           (unsigned)(v.rx.jb.overflow_drop + v.rx.jb.catchup_drop + v.rx.jb.late + v.rx.jb.dup + v.rx.bad), (unsigned)v.rx.under, v.rx.depth, v.rx.target,
           (unsigned)v.aec_us, (unsigned)v.aec_max, (unsigned)v.enc_us, (unsigned)v.enc_max, (unsigned)v.dec_us, (unsigned)v.dec_max,
           (unsigned)v.mic_rms, (unsigned)v.ref_rms, (unsigned)v.aec_rms, (unsigned)v.rx.play_rms, (unsigned)v.rd_err, v.stk_cap, v.stk_rx, v.stk_tx,
           (unsigned)v.rx.jb.underruns, (unsigned)v.rx.jb.overflow_drop, (unsigned)v.rx.jb.catchup_drop, (unsigned)v.rx.jb.late, (unsigned)v.rx.jb.dup, (unsigned)v.rx.bad,
           (unsigned)audioout::gap_max_us(), (unsigned)v.under_ms, v.tx_gain, audioout::volume(),
           (int)m.peer_online[1], (unsigned)m.call_peer, (int)m.deck_ok, v.stk_enc, (unsigned)v.enc_drop, scstore::stack_free(), (int)m.link);
}

}  // namespace scapp
