// app.cpp — 통화 진행 태스크. 설명은 app.h.
//
// app 태스크가 20ms 마다: 교환기 사건 꺼내기 → 터치 꺼내기 → call_fsm 에 넣기 → 할 일 실행 → 그릴 내용이 바뀌었으면 다시 그리기.
// call_fsm 은 app 태스크에서만 만진다(잠금 불필요). 다른 태스크가 보는 것은 STAT 용 사본(s_stat)뿐이다.
// AP-D1 터치는 "닿은 순간"(wasPressed)으로 받는다. 스냅부스의 "뗀 순간"(wasClicked)은 0.5초 넘게 누르거나 8화소 미끄러지면 놓쳐서,
//       받기·끊기처럼 꾹 누르기 쉬운 버튼에 맞지 않는다. 대신 한 번 누름이 여러 번 잡힐 수 있어 TapGuard(시간)와 버튼 배치로 막는다.
// AP-D2 터치는 별도 태스크(touch)가 20ms 마다 읽어 대기열에 넣는다. 화면을 내보내는 동안(약 60~80ms) app 태스크는 멈춰 있는데,
//       M5Unified 의 wasPressed 는 닿은 뒤 첫 update 한 번에만 참이라 그 사이에 닿았다 뗀 짧은 터치를 통째로 놓치기 때문이다
//       (통화 중에는 시계 때문에 매초 다시 그린다 → 끊기 버튼이 안 눌린 것처럼 보일 수 있다).
//       비용: 태스크 하나(4KB). M5.update() 는 touch 태스크에서만 부른다. 탈출구: touch_task 의 내용을 app_task 루프 머리로 되돌린다.
// 수신거부 메시지·벨 끄기(docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md §5.6):
//       메시지 고르기 화면의 열림(s_picker)은 화면 쪽 상태다 — call_fsm 은 INCOMING 그대로라 벨 제한·상대 취소·부재중이 그대로 돈다(DM-D8).
//       그래서 call_fsm 에 사건을 넣을 때마다(feed) s_picker.sync 로 맞춘다: INCOMING 을 떠나면 닫힌다.
//       비용: 기억이 두 군데(Fsm::bell_muted / s_picker). 탈출구: 고르기 열림도 Fsm 에 넣는다.
// 신호음(Phase 3a, docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md): apply() 가 할 일의 A_TONE_* 를 tones::from_acts 로 소리 하나로 바꿔
//       toneout::play 에 넘긴다. PL3a-D13: 신호 보내기보다 먼저 — 보내기가 Wi-Fi 때문에 최대 2초 막혀도(RL-D6) 벨은 누른 즉시 멈춘다.
//       탈출구: 소리 명령을 FSM 줄 printf 뒤로 옮긴다. (Phase 3b: "TONE …" 줄은 이 태스크가 다음 바퀴에 찍는다 — PL3b-D1.)
// 목소리(Phase 3b, docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md PL3b-D13): 같은 할 일의 A_VOICE_START·A_VOICE_STOP 을 voicecore::from_acts 로
//       켜기/끄기로 바꿔 voice::set 에 넘긴다. 소리 명령 다음, 신호 보내기보다 먼저 — 끊기를 누르면 보내기가 막혀 있어도 마이크는 그 자리에서 닫힌다.
//       상태가 실제로 바뀌었을 때만 "VOICE on|off" 한 줄을 찍는다(그 원인인 "FSM …" 줄보다 먼저 찍힌다 — 같은 태스크라 순서가 늘 같다).
//       받기 전에는 켜지지 않는다(PH-D6): 상태 기계가 A_VOICE_START 를 IN_CALL 로 들어갈 때만 낸다(맥 시험 test_voice_core.cpp 가 전수로 고정).
// 수신 무음 감시(Phase 3c, docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D10): 통화 중 상대의 소리 프레임이 5초째 오지 않으면(voicecore::RxWatch)
//       상태 기계에 VOICE_LOST 를 넣는다 → 끊기와 같은 길로 끝나고 "연결이 끊어졌습니다"가 뜬다(FSM 줄: IN_CALL -> ENDING acts=0x248 notice=LOST).
//       감시는 목소리가 실제로 켜졌을 때(VOICE on)만 시작한다 — 소리 장치가 죽은 기기는 받는 프레임을 세지 못하므로 감시하지 않는다.
// 음량 단계(P4V, 2026-10-05 "작게, 크게" — 판단은 vol_steps.h, 자리는 ui_layout.h, 그리기는 ui.cpp draw_volume):
//       화면의 작게/크게와 시리얼 volup·voldn·volstep 이 같은 대기열로 들어와 vol_do 하나를 탄다(P4V-D3). 코덱 값은 audioout::set_volume — 지금까지 'vol' 명령이 쓰던 길 그대로라
//       통화 소리·벨·연결음에 함께 걸린다. 누른 그 바퀴에 적용하고, 화면은 음량 칸만 다시 그리고(P4V-D4), 저장(NVS "phone"/"volstep")은 3초 뒤·통화 밖에서 한 번 한다(P4V-D2).
//       부팅 때는 start 가 저장된 단계를 읽어 태스크를 띄우기 전에 건다(그 앞에는 소리를 낼 태스크가 없다 — 신호음은 app, 시리얼은 그 뒤에 뜬다).
// 전화기 3대 — 신호 규약 v2 의 이음(docs/exec-plans/2026-10-08-phone-3way-design.md §6·§8): 교환기 글자의 기기 ID 와 상태 기계의 상대 slot 을 여기서 잇는다.
//       판단은 순수 헤더가 한다(roster.h·relay_proto.h·call_fsm.h) — 맥 시험 tools/test_roster.cpp 의 "이어 보기"가 아래와 같은 순서로 돈다.
//       받을 때: peer·ring 의 ID(Signal::peer_id) → roster::route_slot → 상태 기계의 peer 인자. 바꿀 수 없는 신호(ID 없는 v1 모양·모르는 ID·내 ID)는 버리고 "SIGDROP …" 한 줄(RS-D4).
//       걸 때:   Out.peer → roster::id_of_slot → {"t":"call","to":"<id>"}(relayproto::send_text_for — RP-D4: ID 를 못 구하면 보내지 않는다).
//       새 기록 줄(ID ↔ slot 을 바꾼 자리마다 한 줄 — 기기 시험이 "누구에게·누가"를 이 줄로 본다): "PEER slot=<n> id=<기기ID> online=<0|1>" · "RING slot=<n> from=<기기ID>" ·
//             "CALL slot=<n> to=<기기ID>" · "SIGDROP ev=<peer|ring> id=<기기ID|->". FSM 줄은 글자 하나 바꾸지 않았다(tools/test_phone_ctl.py 가 고정한다).
// AP-D3 화면이 상대 둘을 그린다(설계서 §9, 배치는 ui_layout.h UL-D4·UL-D5): 그릴 내용(Model)에 상대별 접속 상태(s_fsm.online(slot))와 지금·직전 통화의 상대(s_fsm.call_peer)를 싣는다.
//       대기 화면의 왼쪽 통화 키캡(Tap::CALL) = slot 0 에게, 오른쪽(Tap::CALL_B) = slot 1 에게 걸기 — 둘 다 TAP_CALL 에 slot 을 실어 넣는다(uilayout::call_slot_of). 꺼진 상대의 키캡도 넣는다:
//       상태 기계가 걸지 않고 안내(PEER_OFF)를 낸다. 거는 중·전화 옴·통화 중 화면의 "상대" 캐릭터는 call_peer 의 것이다(누가 걸었나·누구에게 걸었나).
//       그 slot 이 몇 번 기기인가는 화면이 roster 에서 구한다(UL-D5) — 여기서는 번호(s_unit)와 slot 만 넘긴다.
//       STAT: 옛 칸은 글자 하나 그대로(peer= 는 slot 0 의 상태), 줄 맨 끝에 " peer1=<0|1> call_peer=<slot>" 을 더했다 — 스택짱 펌웨어(sc_app.cpp)의 STAT 과 같은 이름·같은 순서다
//       (스택짱은 그 뒤에 제 칸을 더 붙인다). tools/test_phone_ctl.py 가 "앞의 글자는 그대로, 꼬리만 더한다"를 고정한다.
//       비용: same() 이 견주는 칸이 늘었다(빠뜨리면 상대가 켜져도 화면이 다시 그려지지 않는다 — 무엇을 언제 견주는지는 uilayout::peers_same 한 곳, 맥 시험이 고정한다).
//       탈출구: 화면을 slot 0 만 그리던 때로 돌리려면 ui.cpp draw_idle 과 Tap::CALL_B 를 지운다.
// AP-D4 벨 화면이 대기로 바뀐 직후의 통화 키캡은 더 오래 받지 않는다(ui_layout.h UL-D7): 화면이 바뀔 때 "그 앞 화면"(s_prev_shown)을 기억해 두고, 터치마다 uilayout::screen_guard_for 로
//       가드의 길이를 정한다. 통화 키캡이 받기·거절 자리와 겹치기 때문이다 — 받으려는 순간 전화가 끊기면 그 터치가 엉뚱한 상대에게 거는 것이 된다.
//       앞 화면은 uilayout::guard_prev 가 고른다: 버튼이 없는 지나가는 화면(연결 중·끊는 중)은 세지 않는다 → 거절을 누른 뒤(전화 옴 → 끊는 중 → 대기)의 둘째 터치도 막는다.
//       비용: 기억이 둘 늘었다(s_shown·s_prev_shown — refresh 에서 화면 이름이 바뀔 때만 만진다). 탈출구: on_tap 의 allow 에서 셋째 인자를 뺀다.
#include "app.h"

#include <M5Unified.h>
#include <atomic>
#include <cstdio>
#include <cstring>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"

#include "audio_out.h"
#include "call_fsm.h"
#include "gfx.h"
#include "net_wifi.h"
#include "relay_link.h"
#include "relay_proto.h"
#include "roster.h"
#include "tone_out.h"
#include "tones.h"
#include "ui.h"
#include "voice.h"
#include "ui_layout.h"
#include "ui_wifi.h"
#include "vol_steps.h"

using namespace callfsm;
using uilayout::Model;
using uilayout::Tap;

namespace app {
namespace {

const char *TAG = "app";

struct TapEv { int16_t x, y; uint8_t vol_req, vol_arg; };   // vol_req 가 0 이 아니면 터치가 아니라 음량 요청이다(volsteps::Req — 시리얼 volup·voldn·volstep, P4V-D3)

Fsm s_fsm;
Model s_model, s_drawn;
bool s_drawn_valid = false;
uilayout::TapGuard s_guard;
const char *s_screen = "BOOT";
int64_t s_screen_since = 0;
uilayout::Screen s_shown = uilayout::Screen::BOOT, s_prev_shown = uilayout::Screen::BOOT;   // AP-D4: 지금 통화 화면과 그 바로 앞의 것(Wi-Fi 설정 화면은 그 밑의 통화 화면으로 센다)
Notice s_notice = Notice::NONE;
uint8_t s_notice_msg = 0;                       // s_notice == REJECT_MSG 일 때 상대가 보낸 문구 번호
int64_t s_notice_until = 0;
uilayout::DeclinePicker s_picker;               // 메시지 고르기 화면이 열려 있나(app 태스크에서만 만진다)
int s_unit = 0;
voicecore::RxWatch s_rx_watch;                  // PL3c-D10 (app 태스크에서만 만진다)
int s_vol_step = volsteps::STEP_DEFAULT;        // P4V: 음량 단계 1~5. start 가 저장된 값을 넣고, 그 뒤로는 app 태스크에서만 만진다
volsteps::Saver s_vol_saver;                    // P4V-D2: 저장 미루기

SemaphoreHandle_t s_i2c = nullptr, s_stat_lock = nullptr;
QueueHandle_t s_tap_q = nullptr;                // touch 태스크·시리얼 주입 → app 태스크
TaskHandle_t s_app_task = nullptr, s_touch_task = nullptr;
Model s_stat;                                   // STAT 용 사본(s_stat_lock 으로 보호)
const char *s_stat_screen = "BOOT";
std::atomic<uint32_t> s_touches{0};

int64_t now_ms() { return esp_timer_get_time() / 1000; }

void apply(const Out &o, State before, int64_t now)
{
    tones::Tone tone;
    if (tones::from_acts(o.acts, &tone)) toneout::play(tone);   // PL3a-D13: 보내기보다 먼저. 소리 비트가 없으면 지금 소리를 그대로 둔다
    bool talk = false;                                           // PL3b-D13: 목소리도 보내기보다 먼저(끊으면 마이크가 바로 닫힌다)
    if (voicecore::from_acts(o.acts, &talk) && voice::set(talk)) {
        printf("VOICE %s\n", talk ? "on" : "off");
        if (talk) s_rx_watch.start((uint32_t)now);               // PL3c-D10: 이 통화의 수신 무음 감시를 처음부터
    }
    static const uint16_t SENDS[] = { A_SEND_CALL, A_SEND_ACCEPT, A_SEND_REJECT, A_SEND_HANGUP };
    for (uint16_t bit : SENDS) {
        if (!(o.acts & bit)) continue;
        char buf[relayproto::SEND_BUF];                          // 걸기는 대상의 기기 ID 를, 거절은 문구 번호를 싣는다. send_text 가 호출 안에서 다 보내므로 스택 버퍼로 충분하다(ping 과 같은 방식)
        const char *to = roster::id_of_slot(s_unit, o.peer);     // v2: 상대 slot(Out.peer) → 교환기 기기 ID. 걸기 글자에만 실린다
        const char *text = relayproto::send_text_for(bit, buf, sizeof buf, to, o.msg);
        if (!text) {                                             // RP-D4: 걸 대상의 ID 를 못 구했다(올 수 없는 길) — 옛 짝에게 거는 글자로 떨어지지 않는다. 상태 기계가 5초 뒤 다시 붙는다(FSM-D1)
            ESP_LOGE(TAG, "signal 0x%x not sent: no device id for peer slot %u (unit %d)", bit, (unsigned)o.peer, s_unit);
            continue;
        }
        if (bit == A_SEND_CALL) printf("CALL slot=%u to=%s\n", (unsigned)o.peer, to);   // 누구에게 거는가(ID 는 비밀이 아니다). 바로 뒤에 "FSM IDLE -> DIALING acts=0x1 …"
        if (!relaylink::send_text(text)) ESP_LOGW(TAG, "signal 0x%x not sent (link down)", bit);
    }
    if (o.acts & A_RECONNECT) relaylink::reconnect();
    if (o.notice != Notice::NONE) { s_notice = o.notice; s_notice_msg = o.msg; s_notice_until = now + uilayout::notice_ms(o.notice); }   // 거절 문구는 10초, 그 밖은 4초
    if (s_fsm.state != before || o.acts || o.notice != Notice::NONE) {
        char tail[12] = "";                                      // 문구 번호가 있을 때만 줄 끝에 " msg=N"(기존 줄의 글자는 그대로 — DM-D9). 한 번의 printf 로 찍는다(줄이 쪼개지지 않게)
        if (o.msg) snprintf(tail, sizeof tail, " msg=%u", (unsigned)o.msg);
        printf("FSM %s -> %s acts=0x%x notice=%s%s\n", state_name(before), state_name(s_fsm.state), (unsigned)o.acts, notice_name(o.notice), tail);
    }
}

void feed(Ev ev, int64_t now, Why why = Why::NONE, bool flag = false, uint8_t msg = 0, uint8_t peer = 0)   // peer = 상대 slot(call_fsm.h FSM-D3 — PEER·TAP_CALL·RING 만 읽는다). 주지 않으면 slot 0
{
    const State before = s_fsm.state;
    apply(s_fsm.on(ev, now, why, flag, msg, peer), before, now);
    s_picker.sync(s_fsm.state);                                  // 사건마다 맞춘다 — refresh() 에서만 맞추면 한 바퀴 안에 ended → ring 이 연달아 올 때 다음 전화가 고르기 화면으로 뜬다
}

// ── 음량 단계(P4V) ──────────────────────────────────────────────────────────
// 저장: NVS "phone" 의 "volstep"(u8, 1~5). phone_main 의 quiet 와 같은 자리·같은 방식이다. 읽은 값이 범위 밖이거나 없으면 기본(맨 위 = 지금까지의 음량).
const char *VOL_KEY = "volstep";
int vol_load()
{
    nvs_handle_t h; uint8_t v = 0;
    if (nvs_open("phone", NVS_READONLY, &h) != ESP_OK) return volsteps::STEP_DEFAULT;
    const esp_err_t e = nvs_get_u8(h, VOL_KEY, &v);
    nvs_close(h);
    return e == ESP_OK ? volsteps::from_saved(v) : volsteps::STEP_DEFAULT;
}
bool vol_store(int step)
{
    nvs_handle_t h;
    if (nvs_open("phone", NVS_READWRITE, &h) != ESP_OK) return false;
    const bool ok = nvs_set_u8(h, VOL_KEY, (uint8_t)step) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}
void vol_line()                                                  // 응답·상태 줄(모양은 vol_steps.h format_line — 맥 시험이 고정한다)
{
    char line[64];
    volsteps::format_line(line, sizeof line, s_vol_step, audioout::volume(), s_vol_saver.is_saved(s_vol_step));
    printf("%s\n", line);
}
// 요청 하나를 처리한다 — 화면의 작게/크게와 시리얼 명령이 모두 여기로 온다(P4V-D3).
//   코덱 쓰기(레지스터 둘 — I2C 잠금은 audio_out 이 스스로 잡는다)가 성공했을 때만 단계를 바꾼다 → 화면의 막대가 실제 소리와 어긋나지 않는다.
//   같은 단계를 다시 골라도 코덱 값이 다르면(진단용 'vol' 이 옮겨 놓았다) 다시 맞춘다. 확인음은 내지 않는다(통화 중 방해). 저장은 미룬다(P4V-D2).
void vol_do(volsteps::Req req, int arg, int64_t now)
{
    if (req != volsteps::Req::SHOW) {
        const int target = volsteps::apply(s_vol_step, req, arg);
        if (audioout::volume() != volsteps::vol_of(target)) {
            const audioout::VolResult r = audioout::set_volume(volsteps::vol_of(target));
            if (r != audioout::VolResult::OK) {                  // 소리 장치가 없거나 코덱이 대답하지 않는다 → 단계도 화면도 그대로
                printf("VOLSTEP FAIL %s step=%d\n", r == audioout::VolResult::NO_AUDIO ? "no-audio" : r == audioout::VolResult::I2C ? "i2c" : "range", s_vol_step);
                return;
            }
        }
        s_vol_step = target;
        s_vol_saver.changed(target, now);
    }
    vol_line();
}

// 음량 단계를 뺀 나머지가 같은가 — 다르면 화면을 통째로 다시 그린다. 음량 단계만 다르면 그 칸만 다시 그린다(app_task 6)
bool same(const Model &a, const Model &b)
{
    return a.fsm == b.fsm && uilayout::peers_same(a, b)         // AP-D3: 통화 상대, 그리고 (대기 화면에서만) 상대별 켜짐·꺼짐
        && a.missed == b.missed && a.talk_seconds == b.talk_seconds
        && a.notice == b.notice && a.wifi_phase == b.wifi_phase && a.wifi_connected == b.wifi_connected && !strcmp(a.ssid, b.ssid)
        && wifistore::bars(a.rssi) == wifistore::bars(b.rssi) && a.relay_up == b.relay_up && a.relay_code == b.relay_code
        && (a.relay_attempt >= 2) == (b.relay_attempt >= 2) && a.unit == b.unit
        && a.bell_muted == b.bell_muted && a.decline_open == b.decline_open && a.notice_msg == b.notice_msg;   // 빠뜨리면 눌러도 화면이 다시 그려지지 않는다
}

// 지금 상태로 "그릴 내용"을 만들고, 화면이 바뀌었으면 기록한다(터치 가드의 기준 시각이 된다).
void refresh(int64_t now)
{
    Model m;
    m.fsm = s_fsm.state; m.missed = s_fsm.missed;
    for (int slot = 0; slot < uilayout::MAX_PEERS; ++slot) m.peer_online[slot] = s_fsm.online(slot);   // AP-D3: 상대별 접속 상태(대기 화면의 두 줄)
    m.call_peer = s_fsm.call_peer;                               //        지금·직전 통화의 상대 slot(거는 중·전화 옴·통화 중 화면의 "상대")
    m.talk_seconds = s_fsm.state == State::IN_CALL ? (int)((now - s_fsm.call_start_ms) / 1000) : 0;
    if (s_notice != Notice::NONE && now >= s_notice_until) { s_notice = Notice::NONE; s_notice_msg = 0; }
    m.notice = s_notice; m.notice_msg = s_notice_msg;
    m.bell_muted = s_fsm.bell_muted; m.decline_open = s_picker.open;
    m.wifi_phase = netwifi::phase(); m.wifi_connected = netwifi::connected();
    netwifi::ssid(m.ssid, sizeof m.ssid); m.rssi = netwifi::rssi();
    m.relay_up = relaylink::up(); m.relay_code = relaylink::http_code(); m.relay_attempt = relaylink::attempt();
    m.unit = s_unit;
    m.vol_step = (uint8_t)s_vol_step;
    s_model = m;

    const char *screen = uiwifi::is_open() ? (uiwifi::on_keyboard() ? "WIFI_PASS" : "WIFI_LIST") : uilayout::screen_name(uilayout::screen_for(s_model));
    if (strcmp(screen, s_screen) != 0) {
        s_prev_shown = uilayout::guard_prev(s_prev_shown, s_shown);          // AP-D4: 앞 화면을 기억한다(터치 가드의 길이를 정한다 — 지나가는 화면은 건너뛴다)
        s_shown = uilayout::screen_for(s_model);
        s_screen = screen; s_screen_since = now; printf("SCREEN %s\n", screen);
    }
}

void on_tap(int x, int y, int64_t now)
{
    ++s_touches;
    if (uiwifi::is_open()) {                                     // 설정 화면이 떠 있으면 그쪽이 처리한다(좌표는 로그에 남기지 않는다)
        if (!s_guard.allow(now, s_screen_since, uilayout::KEY_SCREEN_GUARD_MS, uilayout::KEY_REPEAT_GUARD_MS)) return;   // 글쇠가 두 번 눌리지 않게(짧은 가드)
        if (!uiwifi::tap(x, y)) s_drawn_valid = false;           // 닫혔다 → 통화 화면을 다시 그린다
        return;
    }
    const Tap t = uilayout::hit(s_model, x, y);
    if (t == Tap::NONE) return;                                  // 빗나간 터치는 가드를 쓰지 않는다(바로 뒤의 정확한 터치를 살린다)
    if (!s_guard.allow(now, s_screen_since, uilayout::screen_guard_for(t, s_prev_shown))) return;   // AP-D4: 벨 화면이 닫힌 직후의 통화 키캡은 1.5초, 그 밖은 0.6초
    switch (t) {
    case Tap::CALL:                                                                      // 왼쪽 키캡 = slot 0, 오른쪽 = slot 1 에게 건다(AP-D3). 꺼진 상대면 상태 기계가 안내를 낸다
    case Tap::CALL_B: feed(Ev::TAP_CALL, now, Why::NONE, false, 0, (uint8_t)uilayout::call_slot_of(t)); break;
    case Tap::CANCEL: feed(Ev::TAP_CANCEL, now); break;
    case Tap::ACCEPT: feed(Ev::TAP_ACCEPT, now); break;
    case Tap::REJECT: feed(Ev::TAP_REJECT, now); break;
    case Tap::HANGUP: feed(Ev::TAP_HANGUP, now); break;
    case Tap::MUTE:   feed(Ev::TAP_MUTE, now); break;                                    // 벨만 끈다(신호 없음)
    case Tap::DECLINE_OPEN:
    case Tap::DECLINE_BACK: s_picker.on_tap(t); break;                                   // 화면만 바뀐다 — call_fsm 에는 넣지 않는다. 화면 이름이 바뀌어 600ms 화면 가드가 걸린다
    case Tap::MSG1: case Tap::MSG2: case Tap::MSG3:
        feed(Ev::TAP_REJECT, now, Why::NONE, false, uilayout::msg_of(t)); break;         // 메시지 거절 = 문구 번호를 실은 거절(DM-D7)
    case Tap::WIFI:   uiwifi::open(); break;
    case Tap::VOL_DN:
    case Tap::VOL_UP: vol_do(uilayout::vol_req_of(t), 0, now); break;                    // 음량만 바뀐다 — call_fsm 에는 넣지 않는다(P4V)
    case Tap::NONE:   break;
    }
}

void touch_task(void *)                                          // AP-D2
{
    while (true) {
        xSemaphoreTake(s_i2c, portMAX_DELAY); M5.update(); xSemaphoreGive(s_i2c);
        auto t = M5.Touch.getDetail();
        if (t.wasPressed()) { TapEv e{ (int16_t)t.x, (int16_t)t.y, 0, 0 }; xQueueSend(s_tap_q, &e, 0); }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_task(void *)
{
    bool wifi_ui_drawn = false;
    while (true) {
        const int64_t now = now_ms();

        toneout::print_pending();                                // 0) 출력 태스크가 남긴 TONE 줄을 찍는다(PL3b-D1 — 출력 태스크는 찍지 않는다)
        voice::print_pending();                                  //    지연 재기(vping)의 결과 줄도

        relaylink::In in;                                        // 1) 교환기 사건
        while (relaylink::poll(&in)) {
            if (in.kind == relaylink::In::Kind::LINK_UP) feed(Ev::LINK_UP, now);
            else if (in.kind == relaylink::In::Kind::LINK_DOWN) feed(Ev::LINK_DOWN, now);
            else {                                               //    v2: peer·ring 은 "누구의 것인가"(기기 ID)를 상대 slot 으로 바꿔 넣는다. 못 바꾸면 버린다(roster.h RS-D4)
                const relayproto::Signal &sg = in.sig;
                const int slot = roster::route_slot(s_unit, relayproto::about_peer(sg), sg.peer_id);
                if (slot < 0) { printf("SIGDROP ev=%s id=%s\n", sg.ev == Ev::PEER ? "peer" : "ring", sg.peer_id[0] ? sg.peer_id : "-"); continue; }   // id=- 가 이어지면 교환기가 아직 v1 이다
                if (sg.ev == Ev::PEER) printf("PEER slot=%d id=%s online=%d\n", slot, roster::id_of_slot(s_unit, slot), (int)sg.flag);
                else if (sg.ev == Ev::RING) printf("RING slot=%d from=%s\n", slot, roster::id_of_slot(s_unit, slot));
                feed(sg.ev, now, sg.why, sg.flag, sg.msg, (uint8_t)slot);
            }
        }
        refresh(now);                                            //    방금 바뀐 화면을 기준으로 터치를 판정한다

        TapEv tap;                                               // 2) 터치(실제 터치와 시리얼로 주입된 것이 같은 길로 온다)
        while (xQueueReceive(s_tap_q, &tap, 0) == pdTRUE) {
            if (tap.vol_req) vol_do((volsteps::Req)tap.vol_req, tap.vol_arg, now);       //    시리얼의 음량 요청(P4V-D3) — 화면의 작게/크게가 on_tap 에서 부르는 것과 같은 함수
            else on_tap(tap.x, tap.y, now);
            refresh(now);
        }

        feed(Ev::TICK, now);                                     // 3) 시간 제한
        if (s_fsm.state == State::IN_CALL && voice::on() && s_rx_watch.check((uint32_t)now, voice::rx_frames()))
            feed(Ev::VOICE_LOST, now);                           //    PL3c-D10: 상대의 소리가 5초째 없다 → 통화를 끝낸다(FSM-D2)
        // P4V-D2: 음량 단계 저장 — 마지막으로 바뀐 뒤 3초가 지났고, 통화·벨·신호음 중이 아닐 때 한 번(플래시에 쓰는 동안은 두 코어가 잠깐 멈춘다 → 소리가 끊길 수 있다)
        if (s_vol_saver.should_write(now, (s_fsm.state == State::IDLE || s_fsm.state == State::OFFLINE) && toneout::now() == tones::Tone::NONE)) {
            const bool ok = vol_store(s_vol_step);
            s_vol_saver.wrote(s_vol_step, ok);
            if (!ok) ESP_LOGW(TAG, "volume step %d not saved (nvs write failed) - it is kept until the next reboot only", s_vol_step);
            vol_line();                                          //    저장됐으면 saved=1, 실패면 saved=0 그대로
        }

        if (uiwifi::is_open() && s_fsm.state != State::IDLE && s_fsm.state != State::OFFLINE) {   // 4) 전화가 오면 설정 화면을 닫는다
            uiwifi::close(); s_drawn_valid = false;
        }

        refresh(now);                                            // 5) 그릴 내용
        xSemaphoreTake(s_stat_lock, portMAX_DELAY); s_stat = s_model; s_stat_screen = s_screen; xSemaphoreGive(s_stat_lock);

        if (uiwifi::is_open()) {                                 // 6) 그리기(바뀌었을 때만). 그리기와 present 는 항상 짝으로
            if (!wifi_ui_drawn || uiwifi::needs_redraw()) { uiwifi::draw(); gfx::present(); wifi_ui_drawn = true; }   // needs_redraw 가 화면을 스스로 닫을 수 있다 → 다음 바퀴에 is_open 을 다시 본다
            s_drawn_valid = false;
        } else {
            wifi_ui_drawn = false;
            if (!s_drawn_valid || !same(s_model, s_drawn)) { ui::draw(s_model); gfx::present(); s_drawn = s_model; s_drawn_valid = true; }
            else if (s_model.vol_step != s_drawn.vol_step) {     //    음량 단계만 바뀌었다 → 그 칸만 다시 그린다(P4V-D4. 음량 칸이 없는 화면이면 그릴 것도 내보낼 것도 없다)
                if (uilayout::volume_visible(s_model)) { ui::draw_volume(s_model); gfx::present(); }
                s_drawn = s_model;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

}  // namespace

void start(int unit, SemaphoreHandle_t i2c_lock)
{
    s_unit = unit;
    s_i2c = i2c_lock;                                            // PL3a-D14: 잠금은 phone_main 이 만든다(audio_out 과 함께 쓴다)
    s_stat_lock = xSemaphoreCreateMutex();
    s_tap_q = xQueueCreate(8, sizeof(TapEv));
    // P4V: 저장된 음량 단계를 태스크를 띄우기 전에 건다(이 앞에는 소리를 낼 태스크가 없다). 소리 장치가 죽었으면 단계만 기억한다(막대는 저장된 값대로 보인다).
    s_vol_step = vol_load();
    s_vol_saver.boot(s_vol_step);
    if (audioout::ready() && audioout::volume() != volsteps::vol_of(s_vol_step) && audioout::set_volume(volsteps::vol_of(s_vol_step)) != audioout::VolResult::OK)
        ESP_LOGW(TAG, "saved volume step %d not applied (codec write failed) - playing at %d", s_vol_step, audioout::volume());
    vol_line();                                                  // "VOLSTEP step=… vol=… saved=1" — 부팅 때 한 줄(기기 시험이 재부팅 뒤 값 유지를 이 줄로 본다)
    xTaskCreatePinnedToCore(app_task, "app", 12 * 1024, nullptr, 4, &s_app_task, 1);
    xTaskCreatePinnedToCore(touch_task, "touch", 4 * 1024, nullptr, 5, &s_touch_task, 1);
}

void inject_tap(int x, int y)
{
    if (!s_tap_q) return;
    TapEv e{ (int16_t)x, (int16_t)y, 0, 0 };
    xQueueSend(s_tap_q, &e, 0);
}

// P4V-D7: app::start 보다 먼저, 소리 장치를 연 직후에 불린다(phone_main) — 이때는 app 의 다른 상태(s_vol_step·대기열)를 건드리지 않는다. 판단은 vol_steps.h boot_needs_write.
void vol_apply_saved()
{
    const int step = vol_load();
    if (!volsteps::boot_needs_write(audioout::ready(), audioout::volume(), step)) return;
    if (audioout::set_volume(volsteps::vol_of(step)) == audioout::VolResult::OK) ESP_LOGI(TAG, "saved volume step %d applied before the sound tasks start (codec %d)", step, audioout::volume());
    else ESP_LOGW(TAG, "saved volume step %d not applied yet (codec write failed) - app::start tries again", step);
}

bool vol_request(int req, int arg)
{
    if (!s_tap_q || !volsteps::req_valid(req)) return false;
    TapEv e{ 0, 0, (uint8_t)req, (uint8_t)arg };
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
    // PL3b-D25 (fix 2): 맨 끝의 칸 열 개는 원인을 가르려고 나눈 것이다. v_drop(앞쪽, 뜻 그대로)은 v_ovf+v_catch+v_late+v_dup+v_bad 의 합이고, v_urun 은 지터 버퍼가 다 떨어진 횟수
    //   (v_lost 와 같은 때 늘지만 은닉 프레임 수가 아니라 "떨어진 사건"), aud_gap 은 출력 쓰기 간격 최대(µs, 정상 16000, 48000 이상 = 끊김), v_under_ms 는 이번 통화의 첫 v_under 의 때(없으면 0).
    // PL3a-D11·PL3b-D16: 새 칸은 줄 끝에 더한다(앞의 칸은 글자 하나 바꾸지 않는다 — phone_ctl.py·touch_audit.py 와 앞 계획의 정규식이 그대로 맞는다).
    //   전화기 3대(AP-D3): 맨 끝의 peer1 = slot 1 의 상대가 켜져 있나, call_peer = 지금·직전 통화의 상대 slot. (peer= 는 slot 0 — 옛 뜻 그대로)
    //   목소리 칸(v_…)의 셈·시간은 이번(또는 직전) 통화의 것이다. v_cap 만 부팅 뒤 누적(통화 밖에서 늘면 마이크가 열려 있는 것).
    printf("STAT up=%u unit=%d fsm=%s screen=%s wifi=%d relay=%d relay_code=%d peer=%d missed=%u touch=%u heap_int=%u heap_int_min=%u heap_psram=%u"
           " stk_app=%u stk_touch=%u stk_relay=%u stk_ws=%u audio=%d tone=%s aud_us=%u/%u aud_err=%u stk_audio=%u"
           " amp=%d aud_late=%u voice=%d vsrc=%s v_cap=%u v_tx=%u v_txdrop=%u v_txfail=%u v_rx=%u v_play=%u v_lost=%u v_drop=%u v_under=%u v_jb=%d/%d"
           " v_aec_us=%u/%u v_enc_us=%u/%u v_dec_us=%u/%u v_mic=%u v_ref=%u v_aec=%u v_spk=%u v_rderr=%u stk_vcap=%u stk_vrx=%u stk_vtx=%u"
           " v_urun=%u v_ovf=%u v_catch=%u v_late=%u v_dup=%u v_bad=%u aud_gap=%u v_under_ms=%u txgain=%d vol=%d"
           " peer1=%d call_peer=%u\n",
           (unsigned)(esp_timer_get_time() / 1000000), m.unit, state_name(m.fsm), screen, (int)m.wifi_connected, (int)m.relay_up, m.relay_code,
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
           (int)m.peer_online[1], (unsigned)m.call_peer);
}

}  // namespace app
