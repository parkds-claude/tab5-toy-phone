// sim_main.cpp — 화면 미리보기의 장면 목록. 장면마다 "그릴 내용"(uilayout::Model)을 만들어 기기와 같은 ui::draw / uiwifi::draw 로 그리고 저장한다.
//   사용: sim <출력 폴더>   (보통 tools/render_screens.py 가 빌드·실행·PNG 변환까지 한다)
//   Wi-Fi 매니저(netwifi)는 여기의 가짜가 대신한다 — 망 목록과 연결 상태를 장면이 정한다. 비밀·실제 망 이름은 쓰지 않는다.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "esp_timer.h"
#include "geom.h"
#include "net_wifi.h"
#include "sim.h"
#include "toy.h"
#include "ui.h"
#include "ui_wifi.h"

using callfsm::Notice;
using callfsm::State;
using uilayout::Model;

// ── 가짜 Wi-Fi 매니저 ────────────────────────────────────────────────────────
namespace {
bool s_linked = true;
netwifi::Manual s_manual = netwifi::Manual::IDLE;
int64_t s_manual_ms = 0;
std::vector<wifistore::Ap> s_scan;
}  // namespace

int64_t esp_timer_get_time() { return 100000000; }

namespace netwifi {
void start() {}
Phase phase() { return s_linked ? Phase::CONNECTED : Phase::SEARCHING; }
bool connected() { return s_linked; }
void ssid(char *out, size_t cap) { snprintf(out, cap, "%s", s_linked ? "우리집" : ""); }
int rssi() { return -58; }
int ps() { return 0; }
uint32_t change_seq() { return 1; }
void set_setup_open(bool) {}
void request_scan() {}
bool scan_busy() { return false; }
std::vector<wifistore::Ap> scan_results() { return s_scan; }
bool is_known(const std::string &name) { return name == "우리집" || name == "사무실-2G"; }
void request_connect(const char *, const char *, bool) { s_manual = Manual::CONNECTING; s_manual_ms = 1; }
Manual manual_state() { return s_manual; }
int64_t manual_since_ms() { return s_manual_ms; }
void request_forget(const char *) {}
}  // namespace netwifi

namespace {

std::string s_dir;
int s_made = 0, s_bad = 0;

Model base(int unit)
{
    Model m;
    m.unit = unit;
    m.fsm = State::IDLE;
    m.wifi_phase = netwifi::Phase::CONNECTED;
    m.wifi_connected = true;
    snprintf(m.ssid, sizeof m.ssid, "우리집");
    m.rssi = -58;
    m.relay_up = true;
    m.peer_online[0] = m.peer_online[1] = true;
    m.vol_step = 4;
    return m;
}

Model idle(int unit, bool on0, bool on1) { Model m = base(unit); m.peer_online[0] = on0; m.peer_online[1] = on1; return m; }
Model call(int unit, State st, int peer_slot) { Model m = base(unit); m.fsm = st; m.call_peer = (uint8_t)peer_slot; return m; }   // 그 slot 의 상대와의 통화·벨

bool holds(const Rect &o, const Rect &i) { return uilayout::rect_holds(o, i); }

// 대기 화면의 그림이 제 칸(ui_layout.h)을 넘지 않는가 — 캐릭터 셋 × (손 흔들기 왼쪽·오른쪽, 잠듦) × 두 줄, 이름 알약의 폭, 안내 알약의 폭.
// 칸끼리·칸과 버튼이 겹치지 않는 것은 맥 시험(tools/test_ui_layout.cpp)이 고정한다 → 둘을 합치면 "캐릭터가 버튼·문구와 겹치지 않는다"가 된다.
void check_peer_cells()
{
    using namespace uilayout;
    const toy::Who whos[] = { toy::Who::BEAR, toy::Who::RABBIT, toy::Who::ROBOT };
    for (int s = 0; s < MAX_PEERS; ++s)
        for (toy::Who w : whos) {
            const int cx = BTN_CALLS[s].cx;
            const Rect boxes[] = { toy::character_box(w, toy::Pose::HI, cx, PEER_FEET_Y, cx > SCREEN_W / 2), toy::character_box(w, toy::Pose::SLEEP, cx, PEER_FEET_Y) };
            for (const Rect &b : boxes)
                if (!holds(PEER_BODY[s], b)) { ++s_bad; fprintf(stderr, "대기 화면의 캐릭터가 칸을 넘는다: slot %d who %d (%d,%d %dx%d)\n", s, (int)w, b.x, b.y, b.w, b.h); }
        }
    const ui_asset_t *const names[] = { &lbl_unit1, &lbl_unit2, &lbl_unit3 };       // 내 이름표(28px)
    const ui_asset_t *const pills[] = { &lbl_name1, &lbl_name2, &lbl_name3 };       // 대기 화면의 이름 알약(32px)
    for (const ui_asset_t *a : pills)                            // ui.cpp draw_peer_name 이 그리는 폭(ui_layout.h peer_name_w)
        if (peer_name_w(a->w) > PEER_NAME[0].w || a->h > PEER_NAME[0].h - 2 * toy::PILL_OUTLINE) { ++s_bad; fprintf(stderr, "이름 알약이 칸보다 크다(%d)\n", a->w); }
    const ui_asset_t *const notices[] = { &lbl_n_peer_off, &lbl_n_no_answer, &lbl_n_rejected, &lbl_n_lost, &lbl_n_ended, &lbl_n_missed, &lbl_n_relay_silent, &lbl_n_busy,
                                          &lbl_n_msg1, &lbl_n_msg2, &lbl_n_msg3 };
    for (const ui_asset_t *a : notices)                          // ui.cpp draw_notice 가 그리는 알약의 폭
        if (toy::label_pill_w(a, NOTICE_BAND.h) > NOTICE_BAND.w || a->h > NOTICE_BAND.h - 2 * toy::PILL_OUTLINE) { ++s_bad; fprintf(stderr, "안내 알약이 띠보다 크다(%d)\n", a->w); }
    int head_max = 0;                                            // ui.cpp draw_unit: 여백 · 작은 얼굴 · 틈 · 글자 / 미등록이면 글자만
    for (toy::Who w : whos) head_max = toy::head_w(w) > head_max ? toy::head_w(w) : head_max;
    for (const ui_asset_t *a : names)
        if (UNIT_TAG_PAD + head_max + UNIT_TAG_GAP + a->w > UNIT_TAG.w) { ++s_bad; fprintf(stderr, "내 이름표가 칸보다 넓다(%d)\n", a->w); }
    if (UNIT_TAG_PAD + lbl_unit0.w > UNIT_TAG.w) { ++s_bad; fprintf(stderr, "미등록 이름표가 칸보다 넓다(%d)\n", lbl_unit0.w); }
}

// 음량 칸만 다시 그리기(ui::draw_volume — 기기에서 음량 버튼을 누를 때의 길)를 거듭해도, 다른 단계로 갔다 돌아와도 화면 전체가 처음과 같아야 한다.
// 같지 않으면 그 길이 이웃 픽셀을 상하게 하거나 가장자리가 거듭 섞여 진해지는 것이다(2026-10-08 검수 I-1).
void check_volume_redraw(const char *name, const Model &m)
{
    if (!uilayout::volume_visible(m)) return;
    const size_t n = (size_t)SCREEN_W * SCREEN_H;
    const std::vector<uint16_t> first(sim::pixels(), sim::pixels() + n);
    for (int i = 0; i < 5; ++i) ui::draw_volume(m);
    Model other = m;
    other.vol_step = m.vol_step == 1 ? 5 : 1;
    ui::draw_volume(other);
    ui::draw_volume(m);
    if (memcmp(first.data(), sim::pixels(), n * sizeof(uint16_t)) != 0) { ++s_bad; fprintf(stderr, "음량 칸 다시 그리기가 화면을 바꿨다: %s\n", name); }
}

void shot(const char *name, const Model &m)
{
    ui::draw(m);
    if (sim::save(s_dir + "/" + name)) ++s_made; else { ++s_bad; fprintf(stderr, "저장 실패: %s\n", name); }
    check_volume_redraw(name, m);
}

void shot_wifi(const char *name)
{
    uiwifi::draw();
    if (sim::save(s_dir + "/" + name)) ++s_made; else fprintf(stderr, "저장 실패: %s\n", name);
}

void tap_key(int row, int col) { uiwifi::tap(8 + col * 115 + 54, 208 + row * 102 + 51); }   // 자판 글쇠의 가운데(ui_wifi.cpp 의 배치)

}  // namespace

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "사용: %s <출력 폴더>\n", argv[0]); return 2; }
    s_dir = argv[1];

    // ── 아직 못 붙음(BOOT) ──
    { Model m = base(1); m.fsm = State::OFFLINE; m.relay_up = false; m.wifi_connected = false; m.ssid[0] = 0; m.wifi_phase = netwifi::Phase::SEARCHING; shot("01_boot_wifi", m); }
    { Model m = base(1); m.fsm = State::OFFLINE; m.relay_up = false; m.wifi_connected = false; m.ssid[0] = 0; m.wifi_phase = netwifi::Phase::NO_KNOWN; shot("02_boot_no_wifi", m); }
    { Model m = base(1); m.fsm = State::OFFLINE; m.relay_up = false; shot("03_boot_relay", m); }
    { Model m = base(2); m.fsm = State::OFFLINE; m.relay_up = false; m.relay_attempt = 3; shot("04_boot_relay_fail", m); }
    { Model m = base(1); m.fsm = State::OFFLINE; m.relay_up = false; m.relay_code = 401; m.notice = Notice::RELAY_SILENT; shot("05_boot_auth_notice", m); }
    { Model m = base(0); m.fsm = State::OFFLINE; m.relay_up = false; shot("06_boot_unregistered", m); }
    { Model m = base(1); m.fsm = State::OFFLINE; m.relay_up = false; m.wifi_connected = false; m.ssid[0] = 0; m.wifi_phase = netwifi::Phase::INIT_FAILED; shot("07_boot_wifi_chip_fail", m); }

    // ── 대기(전화기 3대): 내 번호 1·2·3 × 상대 켜짐/꺼짐(둘 다 켜짐 · 하나만 · 둘 다 꺼짐). 왼쪽 = slot 0 = 번호가 작은 상대 ──
    //    내 번호 1 → 왼쪽 토끼(2번)·오른쪽 로봇(3번) / 2 → 곰(1번)·로봇(3번) / 3 → 곰(1번)·토끼(2번). "하나만"은 번호마다 꺼지는 쪽을 달리해 세 캐릭터의 잠든 그림을 양쪽 자리에서 다 본다
    check_peer_cells();
    { Model m = idle(1, true, true); m.missed = 2; shot("10_idle_u1_on_on", m); }
    shot("11_idle_u1_on_off", idle(1, true, false));             // 스택짱이 스트림덱 상태(= 꺼짐)인 평소 모습
    shot("12_idle_u1_off_off", idle(1, false, false));
    shot("13_idle_u2_on_on", idle(2, true, true));
    shot("14_idle_u2_off_on", idle(2, false, true));
    { Model m = idle(2, false, false); m.missed = 1; m.vol_step = 1; shot("15_idle_u2_off_off_vol_min", m); }
    shot("16_idle_u3_on_on", idle(3, true, true));
    shot("17_idle_u3_on_off", idle(3, true, false));
    shot("18_idle_u3_off_off", idle(3, false, false));
    // ── 대기 + 안내 ──
    { Model m = idle(1, true, true); m.notice = Notice::BUSY; shot("20_idle_notice_busy", m); }
    { Model m = idle(1, false, true); m.notice = Notice::PEER_OFF; m.wifi_connected = false; m.ssid[0] = 0; shot("21_idle_notice_peer_off", m); }
    { Model m = idle(1, true, true); m.notice = Notice::REJECT_MSG; m.notice_msg = 2; m.vol_step = 5; shot("22_idle_notice_msg_vol_max", m); }
    { Model m = idle(2, true, false); m.notice = Notice::MISSED; m.missed = 12; snprintf(m.ssid, sizeof m.ssid, "아주긴이름의공유기-2.4G"); m.rssi = -80; shot("23_idle_notice_missed", m); }
    { Model m = idle(3, true, true); m.notice = Notice::RELAY_SILENT; m.missed = 65535; shot("24_idle_notice_long_missed_max", m); }   // 가장 긴 안내 축 + 가장 넓은 부재중 알약

    // ── 거는 중: 왼쪽 나, 오른쪽 = 내가 건 상대 ──
    shot("30_dialing_u1_to_rabbit", call(1, State::DIALING, 0));
    shot("31_ringback_u2_to_bear", call(2, State::RINGBACK, 0));
    shot("32_ringback_u1_to_robot", call(1, State::RINGBACK, 1));
    shot("33_dialing_u3_to_rabbit", call(3, State::DIALING, 1));

    // ── 전화 옴: 구름 위 = 건 쪽(곰·토끼·로봇) ──
    shot("40_incoming_from_bear", call(2, State::INCOMING, 0));
    shot("41_incoming_from_rabbit", call(1, State::INCOMING, 0));
    shot("42_incoming_from_robot", call(1, State::INCOMING, 1));
    { Model m = call(3, State::INCOMING, 1); m.bell_muted = true; shot("43_incoming_u3_from_rabbit_bell_off", m); }
    { Model m = call(1, State::INCOMING, 0); m.decline_open = true; shot("44_decline_from_rabbit", m); }
    { Model m = call(2, State::INCOMING, 1); m.decline_open = true; shot("45_decline_from_robot", m); }

    // ── 연결 중 · 통화 중 · 끊는 중: 왼쪽 나, 오른쪽 = 이 통화의 상대 ──
    shot("50_answering_u1_rabbit", call(1, State::ANSWERING, 0));
    { Model m = call(1, State::IN_CALL, 0); m.talk_seconds = 207; shot("51_in_call_with_rabbit", m); }
    { Model m = call(2, State::IN_CALL, 0); m.talk_seconds = 5999; m.vol_step = 2; shot("52_in_call_with_bear_long", m); }
    { Model m = call(1, State::IN_CALL, 1); m.talk_seconds = 83; shot("53_in_call_with_robot", m); }
    { Model m = call(3, State::IN_CALL, 0); m.talk_seconds = 12; shot("54_in_call_u3_with_bear", m); }
    { Model m = call(1, State::IN_CALL, 0); m.talk_seconds = 71; m.notice = Notice::LOST; shot("55_in_call_notice", m); }
    shot("56_ending_u2_robot", call(2, State::ENDING, 1));

    // ── Wi-Fi 설정 ──
    s_scan = { { "우리집", -48, true }, { "사무실-2G", -61, true }, { "카페 손님용", -67, false }, { "Tab5-test-network-with-a-long-name", -72, true },
               { "iptime", -79, true }, { "옆집", -83, true }, { "KT_GiGA_2G", -88, true } };
    uiwifi::open();
    shot_wifi("60_wifi_list");
    uiwifi::tap(640, 72 + 3 * 104 + 40);                         // 넷째 줄(모르는 잠긴 망) → 자판 화면
    shot_wifi("61_wifi_pass_empty");
    tap_key(1, 4); tap_key(2, 0); tap_key(3, 5); tap_key(0, 4);  // t a b 5
    tap_key(3, 0);                                               // 대문자 켜기
    shot_wifi("62_wifi_pass_typed_private");
    s_manual = netwifi::Manual::FAIL_AUTH; s_manual_ms = 7;
    shot_wifi("63_wifi_pass_fail");
    s_manual = netwifi::Manual::CONNECTING; s_manual_ms = 8;
    shot_wifi("64_wifi_pass_connecting");
    tap_key(4, 0);                                               // (연결 중에는 입력이 잠긴다 — 화면은 그대로)
    uiwifi::close();
    s_linked = false; s_scan.clear();
    uiwifi::open();
    shot_wifi("65_wifi_list_empty");
    uiwifi::close();

    printf("screens=%d bad=%d dir=%s\n", s_made, s_bad, s_dir.c_str());
    return s_bad ? 1 : 0;
}
