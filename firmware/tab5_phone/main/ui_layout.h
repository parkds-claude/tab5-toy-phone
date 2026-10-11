// ui_layout.h — 통화 화면의 배치와 판정(순수 로직, 맥에서 단위 시험: tools/test_ui_layout.cpp).
//   - 어떤 화면을 그릴지(screen_for), 터치 좌표가 어느 버튼인지(hit), 터치를 받아들일지(TapGuard).
//   - 그리기(ui.cpp)는 여기의 상수대로 그린다 → 시험이 "버튼이 겹치지 않는다·96px 이상이다"를 고정한다.
// 설계서 §10, docs/design/README.md §2·§5(토이폰 테마 — 버튼 모양은 toy.h. 키캡은 그림자까지 여기의 Rect 안에 그리므로 좌표는 테마와 무관하다).
// UL-D1: 화면이 바뀐 직후 같은 자리를 또 눌러도 다른 버튼이 눌리지 않게 "받기/거절"과 "끊기", "통화"와 "취소"를 겹치지 않게 배치했다.
//        Phase 0 에서 세 번 누른 터치가 7회로 기록된 일이 있어(원인 미확정), 배치 + 시간 가드(TapGuard) 두 겹으로 막는다.
//        비용: 받기/거절 버튼이 화면 위쪽으로 올라간다. 탈출구: 원인이 터치 칩 읽기 오류로 확정되면 가드만 남기고 배치를 내려도 된다.
// 수신거부 메시지·벨 끄기(docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md §5.4): 벨 화면 아래 줄에 벨 끄기·메시지로 거절,
//        메시지 고르기 화면(Screen::DECLINE)은 통화 상태 INCOMING 위에 얹는 화면 쪽 상태다(DeclinePicker, DM-D8).
//        신호를 보내는 새 버튼은 문구 셋뿐이고, 통화 쪽 어느 화면의 버튼과도 겹치지 않게 위쪽 한 줄에 둔다(DM-D3 — 시험이 고정한다).
// 음량 버튼(P4V-D4, 2026-10-05 "작게, 크게"): 대기 화면과 통화 화면의 오른쪽 세로 줄에 크게(위) / 단계 막대 / 작게(아래). 두 화면에서 같은 자리다.
//        그 줄(VOL_AREA)은 어느 화면의 어느 버튼과도 겹치지 않는다(아래 static_assert) → 누르는 사이 화면이 바뀌어도 다른 버튼이 눌리지 않고, 그 칸만 다시 그려도 된다.
//        벨 화면에는 두지 않는다(받기·거절·벨 끄기만 — 거절 옆에서 잘못 누르지 않게). 비용: 벨이 울리는 동안에는 못 줄인다(벨 끄기가 있다). 탈출구: volume_visible 에 화면을 더한다.
//        단계 판단은 vol_steps.h, 그리기는 ui.cpp draw_volume — 화면을 다시 디자인할 때는 BTN_VOL_*·VOL_BARS·VOL_AREA 좌표와 그리기 함수만 바꾼다.
// 전화기 3대(docs/exec-plans/2026-10-08-phone-3way-design.md §9, docs/design/README.md §5.4·§5.5): 대기 화면에 상대 둘이 나란히 서고, 걸고 싶은 상대의 통화 키캡을 누른다.
// UL-D4: 통화 키캡 둘 — 왼쪽 BTN_CALL = Tap::CALL = slot 0, 오른쪽 BTN_CALL_B = Tap::CALL_B = slot 1 (slot 의 순서는 roster.h RS-D1: 번호가 작은 상대가 왼쪽).
//        각 상대는 위에서부터 이름 알약(PEER_NAME) · 구름 위의 캐릭터(PEER_BODY) · 통화 키캡 — 한 세로줄이라 "이 친구에게 거는 버튼"이 그림으로 읽힌다.
//        자리: UL-D1 을 두 키캡 모두 지킨다 — 다른 화면의 취소·끊기(x 464~824, y 536~)·벨 끄기/뒤로·메시지로 거절(y 568~)·문구 셋(y ~232)·Wi-Fi·음량 칸과 겹치지 않는다
//              (아래 static_assert + 시험이 원의 점 하나까지 본다). 그 조건을 채우는 곳은 y 232~567 의 띠이고, 그 안에서도 y 536 아래로 내려가려면 x 가 464 의 왼쪽이거나 824 의
//              오른쪽이어야 한다. 캐릭터가 설 높이를 얻으려고 띠의 맨 아래에 두었다 → y 384~560, 가운데 x 368·912(화면 가운데에서 좌우 272), 반지름 88.
//        이유: 전화가 끊기는 순간(벨 → 대기) 손가락이 내려오던 자리가 통화 키캡이면 엉뚱한 상대에게 전화가 걸린다. 상대가 둘이 되어 "엉뚱한"의 값이 커졌다.
//        비용: ① 지름이 240 → 176 으로 줄었다(주요 버튼 160 이상 규칙 안 — 설계서 §9 가 정한 아래 한계). ② 캐릭터가 키캡 위의 하늘에 0.7배로 선다(동산에 설 높이가 없다).
//              ③ 받기·거절(y 328~528)과는 겹친다 — 그 둘까지 피하면 176 짜리 원 둘이 들어갈 자리가 화면에 없다. 예전 통화 버튼(가운데, 지름 240)도 그 둘의 가장자리와 겹쳤지만,
//                 지금은 왼쪽 키캡이 받기의 한가운데 쪽, 오른쪽 키캡이 거절의 한가운데 쪽에 있다 → 이 두 쌍은 배치로 막지 못하고 시간 가드로만 막는다(UL-D7 이 한 방향을 더 길게 막는다).
//        탈출구: 상대가 넷 이상이 되면 키캡 표(BTN_CALLS)가 아니라 화면 구성을 다시 잡아야 한다(아래 static_assert 가 MAX_PEERS == 2 를 묶어 둔다).
// UL-D7: 벨 화면(전화 옴)이 대기 화면으로 바뀐 직후에는 통화 키캡만 더 오래(TAP_AFTER_BELL_GUARD_MS 1.5초) 받지 않는다 — screen_guard_for(터치, 앞 화면), 앞 화면은 guard_prev 가 고른다.
//        이유: UL-D4 의 ③. 받으려는(거절하려는) 순간 건 쪽이 끊거나 벨 30초가 다 되면 화면이 대기로 바뀐다. 그 뒤 0.6초가 지난 터치는 그 자리의 통화 키캡을 눌러
//              (받기 자리 = slot 0, 거절 자리 = slot 1) 건 쪽이 아닌 상대의 벨을 울릴 수 있다. 어린아이가 화면이 바뀐 것을 0.6초 안에 알아채고 손을 멈춘다고 볼 수 없다.
//        범위: 다른 화면의 버튼 가운데 통화 키캡과 겹치는 것은 "받기 ↔ 왼쪽 키캡"·"거절 ↔ 오른쪽 키캡" 둘뿐이다(시험이 모든 화면의 모든 점으로 고정한다) → 앞 화면이 전화 옴일 때의 통화 터치에만 건다.
//              버튼이 없는 지나가는 화면(연결 중·끊는 중 — passing_screen)은 앞 화면으로 세지 않는다: 거절을 누르면 전화 옴 → 끊는 중 → 대기로, 받기를 누른 순간 건 쪽이 끊으면
//              전화 옴 → 연결 중 → 대기로 간다. 그 사이 화면에는 누를 것이 없어 손가락은 여전히 받기·거절 자리에 있다 — "안 눌렸나" 하고 같은 자리를 또 누르면 통화 키캡이다.
//              그래서 그 길로 온 대기 화면에도 1.5초를 건다. 문구로 거절한 길(전화 옴 → 메시지 고르기 → 끊는 중 → 대기)은 앞 화면이 메시지 고르기라 0.6초다(그 화면의 버튼은 통화 키캡과 겹치지 않는다).
//        비용: 부재중 직후·내가 거절한 직후 1.5초 안에 바로 거는 터치는 먹지 않는다(한 번 더 누른다). 자동 시험에서 벨 화면이 끝난 직후 통화 키캡을 누르는 단계 앞에는 sleep:2 를 둔다.
//        하지 않은 것(반대 방향 — 대기 → 벨): 통화 키캡을 누르려는 순간 전화가 오면, 0.6초가 지난 터치는 왼쪽이면 받기·오른쪽이면 거절이 된다. 이쪽 가드는 늘리지 않았다:
//              받기·거절은 벨이 울리자마자 누르는 버튼이고, 기기 시험의 받기·거절 주입(tap:404:428·876:428 — 좌표가 그대로다)이 "벨 뒤 0.6초"에 맞춰져 있어 기기 없이 늘리면 그 흐름을 깨뜨릴 수 있다
//              (스택짱 화면 sc_ui_layout.h 도 이 방향은 0.6초 가드에 기댄다). 기기에서 받기 흐름의 여유를 잰 뒤 정한다.
//        탈출구: 반대 방향도 막으려면 screen_guard_for 에 "앞 화면이 대기이고 터치가 받기·거절"을 더한다. 이 가드를 없애려면 TAP_AFTER_BELL_GUARD_MS 를 TAP_SCREEN_GUARD_MS 와 같게 둔다.
// UL-D5: 그릴 내용(Model)에는 "누가 켜져 있나"(peer_online[slot])와 "지금 통화의 상대"(call_peer)만 싣고, 그 slot 이 몇 번 기기인지는 roster 에서 그때그때 구한다(peer_unit·call_unit).
//        이유: 번호 ↔ slot 의 정본은 roster.h 하나다 — Model 에 사본을 두면 unit 과 어긋난 값을 그릴 수 있다. 화면은 번호만 알면 되고, 번호 ↔ 캐릭터는 toy.cpp 가 정한다.
//        비용: 그릴 때마다 작은 계산(상수 시간). 탈출구: Model 에 peer_unit[MAX_PEERS] 를 싣고 app 이 채운다(스택짱 화면 sc_ui_layout.h 는 그 방식이다).
// UL-D6: 안내 알약의 자리(NOTICE_BAND)를 맨 윗줄로 올렸다(예전 y 92~164, 높이 72 → y 14~78, 높이 64 — 내 이름표와 같은 가운데 높이. 모든 화면에서 같은 자리).
//        이유: 대기 화면의 이름 알약 줄이 예전 안내 자리에 온다(위 UL-D4 의 띠 때문에 상대 줄이 y 92 에서 시작해야 한다). 맨 윗줄 가운데는 안내가 보이는 모든 화면에서 비어 있다
//              (예전에 그 자리에 있던 "상대 전화기 켜짐" 알약이 이름 알약으로 옮겨 갔다). 화면마다 자리를 달리하면 안내가 뜬 채 화면이 바뀔 때 알약이 뛴다.
//              높이를 8 줄인 것은 이름 알약 줄(y 92~)과의 틈(14px) 때문이다 — "통화 중" 알약과 같은 높이가 됐다(글자는 36px 그대로).
//        비용: 거는 중·통화 중·연결 중 화면에서도 안내가 한 줄 위에, 조금 낮은 알약으로 뜬다(구성은 그대로다). 탈출구: ui.cpp draw_notice 가 화면에 따라 다른 띠를 쓰게 한다.
#pragma once
#include <cstddef>
#include <cstdint>

#include "call_fsm.h"
#include "geom.h"
#include "net_wifi.h"
#include "roster.h"
#include "vol_steps.h"

namespace uilayout {

enum class Screen : uint8_t { BOOT, IDLE, DIALING, INCOMING, ANSWERING, IN_CALL, ENDING, DECLINE };
// CALL = slot 0 에게 걸기(대기 화면의 왼쪽 키캡), CALL_B = slot 1(오른쪽 — UL-D4). MSG1~3 은 연달아 둔다. 새 것은 맨 끝에 더한다
enum class Tap : uint8_t { NONE, CALL, CANCEL, ACCEPT, REJECT, HANGUP, WIFI, MUTE, DECLINE_OPEN, DECLINE_BACK, MSG1, MSG2, MSG3, VOL_DN, VOL_UP, CALL_B };
static_assert((int)Tap::MSG3 - (int)Tap::MSG1 + 1 == callfsm::MSG_COUNT, "Tap::MSG1..MSG3 must match callfsm::MSG_COUNT");

constexpr int MAX_PEERS = callfsm::MAX_PEERS;                   // 화면이 그리는 상대 수 = 상대 slot 의 개수
static_assert(MAX_PEERS == 2, "the idle screen is laid out for exactly two peers (BTN_CALL / BTN_CALL_B, Tap::CALL / Tap::CALL_B) - UL-D4");

struct Circle { int cx, cy, r; };

// 그릴 내용 전부. 통화 진행 태스크(app)가 채우고, 그리기·판정 함수는 이것만 본다.
struct Model {
    callfsm::State  fsm = callfsm::State::OFFLINE;
    bool            peer_online[MAX_PEERS] = { false, false };  // slot 별: 그 상대가 켜져 있나(call_fsm 의 online(slot)). 2대 시절의 peer_online 은 [0] 이다
    uint8_t         call_peer = 0;                              // 지금 통화·벨의 상대 slot, 대기 중에는 직전 것(call_fsm 의 call_peer) — 거는 중·전화 옴·통화 중 화면의 "상대"
    uint16_t        missed = 0;
    int             talk_seconds = 0;                       // 통화 시간(초)
    callfsm::Notice notice = callfsm::Notice::NONE;         // 잠깐 띄울 안내
    netwifi::Phase  wifi_phase = netwifi::Phase::STARTING;
    bool            wifi_connected = false;
    char            ssid[33] = "";
    int             rssi = -127;
    bool            relay_up = false;
    int             relay_code = 0;                         // 교환기가 거절한 HTTP 코드(401 = 기기 등록 오류), 없으면 0
    int             relay_attempt = 0;                      // 교환기 연결 연속 실패 횟수
    int             unit = 0;                               // 내 번호 1~3(roster.h), 미등록이면 0
    bool            bell_muted = false;                     // 이번 전화의 벨을 껐다(call_fsm 의 값)
    bool            decline_open = false;                   // 메시지 고르기 화면이 열려 있다(INCOMING 일 때만 뜻이 있다)
    uint8_t         notice_msg = 0;                         // notice == REJECT_MSG 일 때 보여 줄 문구 번호
    uint8_t         vol_step = volsteps::STEP_DEFAULT;      // 음량 단계 1~5(P4V — app 이 채운다)
};

// 상대의 번호(UL-D5): slot → 그 상대의 기기 번호(나를 뺀 나머지를 번호 순으로 — roster.h RS-D1). 상대가 없으면(내가 미등록, slot 이 범위 밖) 0.
inline int  peer_unit(const Model &m, int slot) { return roster::peer_unit(m.unit, slot); }
inline int  call_unit(const Model &m) { return peer_unit(m, m.call_peer); }                       // 지금(직전) 통화·벨의 상대 번호
inline bool peer_shown(const Model &m, int slot) { return peer_unit(m, slot) != 0; }              // 대기 화면에 그 상대의 줄(과 통화 키캡)이 있나
inline bool peer_on(const Model &m, int slot) { return slot >= 0 && slot < MAX_PEERS && m.peer_online[slot]; }

// 버튼 위치(1280×720, 8px 격자)
constexpr Circle BTN_CALL   = { 368, 472, 88 };                 // 대기 화면 왼쪽: slot 0 에게 걸기. 지름 176 (UL-D4)
constexpr Circle BTN_CALL_B = { 912, 472, 88 };                 // 대기 화면 오른쪽: slot 1 에게 걸기. 화면 가운데(640)에서 좌우로 272
constexpr Circle BTN_CALLS[MAX_PEERS] = { BTN_CALL, BTN_CALL_B };   // slot 순서
constexpr Tap    CALL_TAPS[MAX_PEERS] = { Tap::CALL, Tap::CALL_B };
constexpr Rect   BTN_WIFI   = {  32, 600, 240,  96 };
constexpr Rect   BTN_CANCEL = { 480, 536, 320, 160 };          // 주요 버튼은 160px 이상(취소·끊기도 그 화면의 주요 버튼이다)
constexpr Rect   BTN_ACCEPT = { 224, 328, 360, 200 };
constexpr Rect   BTN_REJECT = { 696, 328, 360, 200 };
constexpr Rect   BTN_HANGUP = { 464, 536, 360, 160 };
constexpr Rect   BTN_MUTE    = { 224, 568, 360, 112 };         // 벨 화면 아래 줄 왼쪽(받기 아래). 끈 뒤에는 "벨 꺼짐" 표시일 뿐 버튼이 아니다
constexpr Rect   BTN_DECLINE = { 696, 568, 360, 112 };         // 벨 화면 아래 줄 오른쪽(거절 아래): 메시지 고르기 화면을 연다
constexpr Rect   BTN_MSG[]   = { { 48, 104, 384, 128 }, { 448, 104, 384, 128 }, { 848, 104, 384, 128 } };   // 고르기 화면 위쪽 한 줄: 문구 1·2·3
constexpr Rect   BTN_BACK    = { 224, 568, 360, 112 };         // 고르기 화면 아래 왼쪽(벨 끄기와 같은 자리)
static_assert(sizeof BTN_MSG / sizeof BTN_MSG[0] == callfsm::MSG_COUNT, "BTN_MSG must have callfsm::MSG_COUNT entries");
// 음량(P4V-D4): 오른쪽 세로 줄. 주입 좌표(가운데) = 크게 (1168,312), 작게 (1168,608)
constexpr Rect   BTN_VOL_UP = { 1088, 240, 160, 144 };     // 크게(＋)
constexpr Rect   VOL_BARS   = { 1088, 400, 160, 120 };     // 단계 막대(버튼이 아니다)
constexpr Rect   BTN_VOL_DN = { 1088, 536, 160, 144 };     // 작게(−)
constexpr Rect   VOL_AREA   = { 1088, 240, 160, 440 };     // 위 셋을 담는 칸 = 음량만 바뀌었을 때 다시 그리는 범위

// 그리기용 자리(누르는 것이 아니다 — 그리는 쪽 ui.cpp 가 이 칸 안에만 그린다. 시험이 "서로·버튼과 겹치지 않는다"를 고정하고, 화면 미리보기가 그림이 칸을 넘지 않는지 본다)
constexpr Rect   UNIT_TAG    = {  24,  12, 216,  72 };          // 왼쪽 위: 내 이름표(작은 얼굴 + "N번 전화기" 또는 "등록되지 않은 기기")
constexpr Rect   NOTICE_BAND = { 240,  14, 800,  64 };          // 맨 윗줄 가운데: 잠깐 뜨는 안내 알약(가운데 정렬, 폭은 문구에 따라, 높이는 이 띠의 높이 — UL-D6)
constexpr Rect   PEER_NAME[MAX_PEERS] = { { 248,  92, 240,  52 }, { 792,  92, 240,  52 } };   // 대기: 상대의 이름 알약("N번 전화기" + 켜짐 불빛)
constexpr Rect   PEER_BODY[MAX_PEERS] = { { 256, 148, 224, 232 }, { 800, 148, 224, 232 } };   // 대기: 구름 위의 상대 캐릭터(구름·잠든 그림의 Z 까지)
constexpr int    PEER_FEET_Y = 352;                             // 대기: 상대 캐릭터의 발밑(대칭축은 그 줄의 통화 키캡 cx). 귀가 가장 긴 토끼(0.7배, 키 200)의 귀 끝이 y 152
constexpr int    PEER_LAMP   = 22;                              // 이름 알약 안의 켜짐 불빛(지름)
constexpr int    peer_name_w(int label_w) { return 24 + label_w + 12 + PEER_LAMP + 18; }   // 이름 알약의 폭 = 여백 24 · 글자 · 틈 12 · 불빛 · 여백 18
constexpr int    UNIT_TAG_PAD = 8, UNIT_TAG_GAP = 10;           // 내 이름표 안: 왼쪽 여백 · 작은 얼굴 · 틈 · 글자
constexpr Rect   MISSED_AREA = { 352, 588, 448, 128 };          // 아래 가운데: 병아리 + "부재중 n건" 알약(알약의 가운데는 화면 가운데, 병아리가 그 왼쪽 — 다섯 자리 건수까지 이 칸 안이다)

constexpr bool rects_overlap(const Rect &a, const Rect &b) { return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h; }
constexpr bool rect_holds(const Rect &o, const Rect &i) { return i.x >= o.x && i.y >= o.y && i.x + i.w <= o.x + o.w && i.y + i.h <= o.y + o.h; }
constexpr Rect circle_box(const Circle &c) { return Rect{ c.cx - c.r, c.cy - c.r, 2 * c.r, 2 * c.r }; }
// UL-D4: 통화 키캡 하나가 다른 화면의 버튼들과 떨어져 있는가(상자로 본다 — 원은 그 안에 있다). 원의 맨 아래 점(cy + r)까지 벨 끄기 줄(y 568)에 닿지 않게 상자의 아랫변으로 견준다
constexpr bool call_btn_clear(const Circle &c)
{
    const Rect b = circle_box(c);
    return 2 * c.r >= 176 && rect_holds(Rect{ 0, 0, SCREEN_W, SCREEN_H }, b)
        && !rects_overlap(b, BTN_CANCEL) && !rects_overlap(b, BTN_HANGUP) && !rects_overlap(b, BTN_MUTE) && !rects_overlap(b, BTN_DECLINE) && !rects_overlap(b, BTN_BACK)
        && !rects_overlap(b, BTN_MSG[0]) && !rects_overlap(b, BTN_MSG[1]) && !rects_overlap(b, BTN_MSG[2]) && !rects_overlap(b, BTN_WIFI) && !rects_overlap(b, VOL_AREA)
        && c.cy + c.r < BTN_MUTE.y && c.cy + c.r < BTN_DECLINE.y;
}
static_assert(call_btn_clear(BTN_CALL) && call_btn_clear(BTN_CALL_B), "UL-D4: a call button is >= 176px, inside the screen and overlaps no button of another screen (cancel, hangup, mute, back, open-picker, messages, wifi, volume)");
static_assert(BTN_CALL.r == BTN_CALL_B.r && BTN_CALL.cy == BTN_CALL_B.cy && BTN_CALL.cx + BTN_CALL_B.cx == SCREEN_W && BTN_CALL.cx < BTN_CALL_B.cx
              && (BTN_CALL_B.cx - BTN_CALL_B.r) - (BTN_CALL.cx + BTN_CALL.r) >= 96, "UL-D4: slot 0 is the left call button, slot 1 the right one - same size, mirrored, far apart");
static_assert(BTN_VOL_UP.w >= 144 && BTN_VOL_UP.h >= 144 && BTN_VOL_DN.w >= 144 && BTN_VOL_DN.h >= 144, "volume buttons are big enough for a child's finger");
static_assert(BTN_VOL_DN.y - (BTN_VOL_UP.y + BTN_VOL_UP.h) >= 96, "louder and quieter are far apart");
static_assert(rect_holds(VOL_AREA, BTN_VOL_UP) && rect_holds(VOL_AREA, VOL_BARS) && rect_holds(VOL_AREA, BTN_VOL_DN) && rect_holds(Rect{ 0, 0, SCREEN_W, SCREEN_H }, VOL_AREA),
              "the volume cell holds its parts and is inside the screen");
static_assert(!rects_overlap(BTN_VOL_UP, VOL_BARS) && !rects_overlap(VOL_BARS, BTN_VOL_DN) && !rects_overlap(BTN_VOL_UP, BTN_VOL_DN), "volume parts do not overlap each other");
static_assert(!rects_overlap(VOL_AREA, circle_box(BTN_CALL)) && !rects_overlap(VOL_AREA, circle_box(BTN_CALL_B)) && !rects_overlap(VOL_AREA, BTN_WIFI) && !rects_overlap(VOL_AREA, BTN_CANCEL) && !rects_overlap(VOL_AREA, BTN_ACCEPT)
              && !rects_overlap(VOL_AREA, BTN_REJECT) && !rects_overlap(VOL_AREA, BTN_HANGUP) && !rects_overlap(VOL_AREA, BTN_MUTE) && !rects_overlap(VOL_AREA, BTN_DECLINE)
              && !rects_overlap(VOL_AREA, BTN_MSG[0]) && !rects_overlap(VOL_AREA, BTN_MSG[1]) && !rects_overlap(VOL_AREA, BTN_MSG[2]) && !rects_overlap(VOL_AREA, BTN_BACK),
              "the volume cell overlaps no button of any screen");

const int64_t TAP_SCREEN_GUARD_MS = 600;                    // 화면이 바뀐 직후 이만큼은 터치를 버린다
const int64_t TAP_AFTER_BELL_GUARD_MS = 1500;               // 벨 화면이 대기로 바뀐 직후의 통화 키캡은 이만큼(UL-D7)
const int64_t TAP_REPEAT_GUARD_MS = 400;                    // 직전에 받아들인 터치 뒤 이만큼은 버린다
const int64_t KEY_SCREEN_GUARD_MS = 300;                    // Wi-Fi 설정 화면(목록·자판)용 — 글쇠를 빨리 치므로 짧게
const int64_t KEY_REPEAT_GUARD_MS = 150;
const int64_t NOTICE_MS     = 4000;                         // 안내를 띄워 두는 시간
const int64_t NOTICE_MSG_MS = 10000;                        // 상대가 보낸 거절 문구는 읽을 시간을 더 준다(DM-D10)

inline int64_t notice_ms(callfsm::Notice n) { return n == callfsm::Notice::REJECT_MSG ? NOTICE_MSG_MS : NOTICE_MS; }

inline Screen screen_for(const Model &m)
{
    switch (m.fsm) {
    case callfsm::State::OFFLINE:   return Screen::BOOT;    // Wi-Fi 또는 교환기에 아직 못 붙음
    case callfsm::State::IDLE:      return Screen::IDLE;
    case callfsm::State::DIALING:
    case callfsm::State::RINGBACK:  return Screen::DIALING;
    case callfsm::State::INCOMING:  return m.decline_open ? Screen::DECLINE : Screen::INCOMING;   // 다른 상태에서는 decline_open 을 보지 않는다
    case callfsm::State::ANSWERING: return Screen::ANSWERING;
    case callfsm::State::IN_CALL:   return Screen::IN_CALL;
    case callfsm::State::ENDING:    return Screen::ENDING;
    }
    return Screen::BOOT;
}

inline const char *screen_name(Screen s)
{
    switch (s) {
    case Screen::BOOT: return "BOOT"; case Screen::IDLE: return "IDLE"; case Screen::DIALING: return "DIALING";
    case Screen::INCOMING: return "INCOMING"; case Screen::ANSWERING: return "ANSWERING"; case Screen::IN_CALL: return "IN_CALL";
    case Screen::ENDING: return "ENDING"; case Screen::DECLINE: return "DECLINE";
    }
    return "?";
}

inline bool in_circle(const Circle &c, int x, int y)
{
    const int64_t dx = x - c.cx, dy = y - c.cy;
    return dx * dx + dy * dy <= (int64_t)c.r * c.r;
}

inline Tap hit(const Model &m, int x, int y)
{
    switch (screen_for(m)) {
    case Screen::BOOT:
        if (BTN_WIFI.hit(x, y)) return Tap::WIFI;
        break;
    case Screen::IDLE:
        for (int s = 0; s < MAX_PEERS; ++s)                 // 상대가 꺼져 있어도 그 키캡의 터치를 돌려준다 — 안내는 call_fsm 이 낸다. 상대가 없는 자리(미등록 기기)에는 키캡이 없다
            if (peer_shown(m, s) && in_circle(BTN_CALLS[s], x, y)) return CALL_TAPS[s];
        if (BTN_WIFI.hit(x, y)) return Tap::WIFI;
        if (BTN_VOL_UP.hit(x, y)) return Tap::VOL_UP;       // 맨 위·맨 아래에서도 돌려준다 — 멈추는 것은 vol_steps.h 가 한다
        if (BTN_VOL_DN.hit(x, y)) return Tap::VOL_DN;
        break;
    case Screen::DIALING:
        if (BTN_CANCEL.hit(x, y)) return Tap::CANCEL;
        break;
    case Screen::INCOMING:
        if (BTN_ACCEPT.hit(x, y)) return Tap::ACCEPT;
        if (BTN_REJECT.hit(x, y)) return Tap::REJECT;
        if (BTN_MUTE.hit(x, y)) return m.bell_muted ? Tap::NONE : Tap::MUTE;      // 이미 껐으면 버튼이 아니다(상태 표시)
        if (BTN_DECLINE.hit(x, y)) return Tap::DECLINE_OPEN;
        break;
    case Screen::DECLINE:                                   // 문구 셋과 뒤로만 버튼이다(받으려면 뒤로)
        for (int i = 0; i < callfsm::MSG_COUNT; ++i) if (BTN_MSG[i].hit(x, y)) return (Tap)((int)Tap::MSG1 + i);
        if (BTN_BACK.hit(x, y)) return Tap::DECLINE_BACK;
        break;
    case Screen::IN_CALL:
        if (BTN_HANGUP.hit(x, y)) return Tap::HANGUP;
        if (BTN_VOL_UP.hit(x, y)) return Tap::VOL_UP;
        if (BTN_VOL_DN.hit(x, y)) return Tap::VOL_DN;
        break;
    case Screen::ANSWERING:
    case Screen::ENDING:
        break;
    }
    return Tap::NONE;
}

// 문구 버튼 → 문구 번호(1~MSG_COUNT). 문구 버튼이 아니면 0.
inline uint8_t msg_of(Tap t) { return (t >= Tap::MSG1 && t <= Tap::MSG3) ? (uint8_t)((int)t - (int)Tap::MSG1 + 1) : 0; }

// 통화 키캡 → 걸 상대의 slot(UL-D4 — 표 CALL_TAPS 를 거꾸로 읽는다). 통화 키캡이 아니면 -1.
constexpr int call_slot_of(Tap t)
{
    for (int s = 0; s < MAX_PEERS; ++s) if (CALL_TAPS[s] == t) return s;
    return -1;
}

// 이 터치에 걸 화면 가드의 길이(UL-D7). prev = 터치 가드가 보는 앞 화면(아래 guard_prev). 벨 화면이 대기로 바뀐 직후의 통화 키캡만 길다.
constexpr int64_t screen_guard_for(Tap t, Screen prev)
{
    return call_slot_of(t) >= 0 && prev == Screen::INCOMING ? TAP_AFTER_BELL_GUARD_MS : TAP_SCREEN_GUARD_MS;
}

// 버튼이 없는 지나가는 화면인가(연결 중·끊는 중 — hit 가 아무것도 돌려주지 않는 화면. 시험이 그 둘뿐임을 고정한다).
constexpr bool passing_screen(Screen s) { return s == Screen::ANSWERING || s == Screen::ENDING; }
// 화면이 바뀔 때 터치 가드가 "앞 화면"으로 볼 것(UL-D7). leaving = 방금 떠난 화면, before = 그때까지의 앞 화면.
//   지나가는 화면을 떠날 때는 그 앞의 것을 그대로 둔다 → 전화 옴 → 끊는 중 → 대기에서 앞 화면은 여전히 전화 옴이다.
constexpr Screen guard_prev(Screen before, Screen leaving) { return passing_screen(leaving) ? before : leaving; }

// 그릴 내용의 "상대" 부분이 같은가 — 다르면 다시 그린다(app.cpp same() 이 쓴다).
//   통화 상대(call_peer)는 늘 견준다. 상대별 켜짐·꺼짐은 대기 화면만 그리므로 그 화면에서만 견준다
//   → 통화 중에 다른 상대가 켜지고 꺼져도 화면을 통째로 다시 그리지 않는다(다시 그리기는 60~80ms 를 쓰고, 통화 중에는 소리가 먼저다).
//   대기 화면으로 돌아올 때는 상태(fsm)가 달라 어차피 통째로 그린다 — 그때의 값이 그려진다.
inline bool peers_same(const Model &a, const Model &b)
{
    if (a.call_peer != b.call_peer) return false;
    if (a.fsm != callfsm::State::IDLE || b.fsm != callfsm::State::IDLE) return true;
    for (int s = 0; s < MAX_PEERS; ++s) if (a.peer_online[s] != b.peer_online[s]) return false;
    return true;
}

// 음량 칸을 그리는(= 누를 수 있는) 화면인가: 대기와 통화(P4V-D4). hit 의 두 가지와 같아야 한다 — 시험이 고정한다.
inline bool volume_visible(const Model &m) { const Screen s = screen_for(m); return s == Screen::IDLE || s == Screen::IN_CALL; }
// 음량 버튼 → 요청. 음량 버튼이 아니면 NONE.
inline volsteps::Req vol_req_of(Tap t) { return t == Tap::VOL_UP ? volsteps::Req::UP : t == Tap::VOL_DN ? volsteps::Req::DOWN : volsteps::Req::NONE; }

// 안내 상자를 그릴까. 고르기 화면에서는 그리지 않는다(DM-D12 — 안내 시간은 계속 간다. 그때는 문구 버튼 줄을, 안내가 맨 윗줄로 온 지금(UL-D6)은 그 화면의 안내 문구를 가린다).
// 벨 화면에서도 그리지 않는다(토이폰 테마 UL-D3, 2026-10-08): 안내 자리(위쪽 가운데)에 전화 건 캐릭터가 서고, 벨 화면에서는 받을지 말지가 먼저다.
//        비용: 직전 통화의 안내(예: 상대의 거절 문구 10초)가 떠 있는 동안 전화가 오면 그 안내는 벨이 끝날 때까지 가려진다(시간은 계속 간다).
//        탈출구: 아래 조건에서 INCOMING 을 빼고 ui.cpp draw_incoming 의 캐릭터 자리를 옮긴다.
inline bool notice_visible(const Model &m)
{
    const Screen s = screen_for(m);
    return m.notice != callfsm::Notice::NONE && s != Screen::DECLINE && s != Screen::INCOMING;
}

// 이 프레임을 스크린샷으로 내보내도 되는가의 반대 — Wi-Fi 비밀번호 자판 화면에 입력이 한 글자라도 있으면 비공개다.
// UL-D2(I-1): "보기"면 평문, 숨김이면 ● 개수로 길이가 드러나므로 둘 다 막는다. 빈 자판과 그 밖의 화면은 공개(디자인 검토용으로 찍을 수 있다).
//        비용: 비밀번호를 입력하는 동안은 화면을 찍을 수 없다. 탈출구: 이 함수가 false 만 돌려주게 하면 막지 않는다.
inline bool is_private_frame(bool on_keyboard, size_t pw_len) { return on_keyboard && pw_len > 0; }

// 메시지 고르기 화면의 열림 상태(화면 쪽 상태 — 통화 상태 기계는 INCOMING 그대로다).
struct DeclinePicker {
    bool open = false;
    void on_tap(Tap t) { if (t == Tap::DECLINE_OPEN) open = true; else if (t == Tap::DECLINE_BACK) open = false; }
    // INCOMING 을 떠나면 닫는다. call_fsm 에 사건을 넣을 때마다 부른다 — 화면을 그릴 때만 맞추면 한 바퀴 안에 ended → ring 이 연달아 올 때
    // 다음 전화가 고르기 화면으로 뜬다.
    void sync(callfsm::State s) { if (s != callfsm::State::INCOMING) open = false; }
};

struct TapGuard {
    int64_t last_ms = -1000000;
    // 이 터치를 받아들일까. screen_since_ms = 지금 화면이 뜬 시각.
    bool allow(int64_t now_ms, int64_t screen_since_ms, int64_t screen_guard_ms = TAP_SCREEN_GUARD_MS, int64_t repeat_guard_ms = TAP_REPEAT_GUARD_MS)
    {
        if (now_ms - screen_since_ms < screen_guard_ms) return false;
        if (now_ms - last_ms < repeat_guard_ms) return false;
        last_ms = now_ms;
        return true;
    }
};

// 통화 시간(초) → 분·초. 99:59 에서 멈춘다.
inline void talk_clock(int seconds, int *mm, int *ss)
{
    if (seconds < 0) seconds = 0;
    if (seconds > 5999) seconds = 5999;
    *mm = seconds / 60; *ss = seconds % 60;
}

}  // namespace uilayout
