// ui_wifi.cpp — Wi-Fi 설정 화면(망 목록 + 비밀번호 자판) 구현.
// 출처: firmware/tab5_snapbooth/main/main.cpp 의 NET_LIST / NET_PASS 화면(D13·D14)과 터치 처리(wifi_list_tap, wifi_pass_tap).
//   그대로 둔 것: 목록에서 고르면 자판 화면으로 — 아는 망·열린 망은 고르는 즉시 저장된 비밀번호로 연결, 연결 중에는 입력 잠금,
//                다시 입력하면 이전 오류 문구가 지워짐, 연결 성공 1.5초 뒤 자동으로 닫힘, 자판 배열(11열×5행, 영문/기호 두 벌, 대문자 전환).
//                "비밀번호는 8자 이상" 검사는 netwifi::request_connect 로 옮겼다(결과는 같게 Manual::SHORT_PASS 로 돌아온다).
//   바꾼 것(디자인 규칙 "터치 대상 96px 이상", "고정 문구는 글자 자산"):
//     - 목록 한 줄 72 → 96px (한 쪽에 6줄 → 5줄), 하단 버튼 높이 80 → 96px, 보기/숨기기 64 → 96px(비밀번호 칸도 96px)
//     - 자판 시작 y 200 → 208 (글쇠 109×98 → 109×96, 여전히 96 이상)
//     - 고정 문구는 lbl_wf_* 자산. 망 이름·비밀번호·쪽 번호·글쇠 글자만 gfx::text(내장 글꼴)
//     - 상태 문구를 비밀번호 칸 아래에서 맨 윗줄 오른쪽으로 옮김(96px 칸을 넣을 자리 확보 + "상태 문구는 상단" 규칙)
//   걷어낸 것: 카메라 잠금(s_proc_mutex·s_paused·s_frozen_valid), 화면 상태(State) 전환 — 열림/닫힘은 is_open() 으로 알린다.
//
// 결정
//   W-D1 그릴 때 쓴 스캔 목록을 s_list 에 붙잡아 두고 터치 판정도 그 목록으로 한다.
//        이유: 그린 뒤 스캔 결과가 바뀌면 "눈에 보인 줄"과 "눌린 줄"이 달라질 수 있다(원본은 매번 최신 목록을 읽었다).
//   W-D2 수동 연결 상태(netwifi::manual_state)를 화면이 지우지 않고, "여기까지 봤다"는 시각(s_ack_ms)만 기억해 가린다.
//        이유: 상태의 주인은 매니저 태스크다. 화면이 지우면 매니저가 막 써넣은 결과를 덮어쓸 수 있다.
//        탈출구: net_wifi 에 manual_clear() 를 추가하고 ack_manual() 을 그 호출로 바꾼다.
//   W-D3 토이폰 테마(2026-10-08, docs/design/README.md §5): 배치·판정은 그대로 두고 모양만 바꿨다 — 누르는 것(목록 줄·버튼·글쇠)은 키캡,
//        보여 주는 것(비밀번호 칸·상태 문구)은 납작한 판·알약. 바탕은 하늘색 한 색(빽빽한 화면이라 동산·구름은 두지 않는다).
//        키캡은 그림자까지 터치 영역 안에 그리므로 글자는 윗면 가운데(toy::keycap_cy)에 놓는다.
#include "ui_wifi.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "esp_log.h"
#include "esp_timer.h"

#include "gfx.h"
#include "net_wifi.h"
#include "toy.h"
#include "ui_layout.h"

namespace {

const char *TAG = "uiwifi";

using netwifi::Manual;

// ── 목록 화면 배치 ──────────────────────────────────────────────────────────
constexpr int HEAD_CY = 36;                                   // 맨 윗줄(제목·쪽 번호·상태)의 세로 가운데
constexpr int WL_ROWS = 5, WL_X = 40, WL_Y = 72, WL_W = SCREEN_W - 80, WL_H = 96, WL_GAP = 8;
constexpr int WB_W = 270, WB_H = 96, WB_GAP = 24, WB_Y = SCREEN_H - 24 - WB_H;
Rect wl_row(int i) { return Rect{WL_X, WL_Y + i * (WL_H + WL_GAP), WL_W, WL_H}; }
Rect wl_btn(int i) { const int x0 = (SCREEN_W - (4 * WB_W + 3 * WB_GAP)) / 2; return Rect{x0 + i * (WB_W + WB_GAP), WB_Y, WB_W, WB_H}; }

// ── 자판 화면 배치 (D14: 11열×5행, 영문/기호 두 벌) ──────────────────────────
enum KAct : uint8_t { KA_CHAR, KA_BACK, KA_SHIFT, KA_SYM, KA_SPACE, KA_CANCEL, KA_OK };
struct PKey { uint8_t row, col, span; const char *lo, *sym; KAct act; };
const PKey PKEYS[] = {
    {0,0,1,"1","1",KA_CHAR},{0,1,1,"2","2",KA_CHAR},{0,2,1,"3","3",KA_CHAR},{0,3,1,"4","4",KA_CHAR},{0,4,1,"5","5",KA_CHAR},
    {0,5,1,"6","6",KA_CHAR},{0,6,1,"7","7",KA_CHAR},{0,7,1,"8","8",KA_CHAR},{0,8,1,"9","9",KA_CHAR},{0,9,1,"0","0",KA_CHAR},{0,10,1,"","",KA_BACK},
    {1,0,1,"q","!",KA_CHAR},{1,1,1,"w","@",KA_CHAR},{1,2,1,"e","#",KA_CHAR},{1,3,1,"r","$",KA_CHAR},{1,4,1,"t","%",KA_CHAR},
    {1,5,1,"y","^",KA_CHAR},{1,6,1,"u","&",KA_CHAR},{1,7,1,"i","*",KA_CHAR},{1,8,1,"o","(",KA_CHAR},{1,9,1,"p",")",KA_CHAR},{1,10,1,"@","_",KA_CHAR},
    {2,0,1,"a","-",KA_CHAR},{2,1,1,"s","+",KA_CHAR},{2,2,1,"d","=",KA_CHAR},{2,3,1,"f","[",KA_CHAR},{2,4,1,"g","]",KA_CHAR},
    {2,5,1,"h","{",KA_CHAR},{2,6,1,"j","}",KA_CHAR},{2,7,1,"k","\\",KA_CHAR},{2,8,1,"l","|",KA_CHAR},{2,9,1,".",";",KA_CHAR},{2,10,1,"-",":",KA_CHAR},
    {3,0,1,"","",KA_SHIFT},{3,1,1,"z","'",KA_CHAR},{3,2,1,"x","\"",KA_CHAR},{3,3,1,"c",",",KA_CHAR},{3,4,1,"v",".",KA_CHAR},
    {3,5,1,"b","<",KA_CHAR},{3,6,1,"n",">",KA_CHAR},{3,7,1,"m","/",KA_CHAR},{3,8,1,"_","?",KA_CHAR},{3,9,1,"!","`",KA_CHAR},{3,10,1,"?","~",KA_CHAR},
    {4,0,2,"","",KA_SYM},{4,2,5,"","",KA_SPACE},{4,7,2,"","",KA_CANCEL},{4,9,2,"","",KA_OK},
};
constexpr int NPKEYS = sizeof(PKEYS) / sizeof(PKEYS[0]);
constexpr int KB_Y = 208, KB_MARGIN = 8, KB_GAP = 6, KB_COLS = 11, KB_ROWS = 5;
constexpr int KPX = (SCREEN_W - 2 * KB_MARGIN + KB_GAP) / KB_COLS;    // 115 → 글쇠 폭 109
constexpr int KPY = (SCREEN_H - KB_Y) / KB_ROWS;                      // 102 → 글쇠 높이 96
static_assert(KPX - KB_GAP >= 96 && KPY - KB_GAP >= 96, "자판 글쇠는 96px 이상이어야 한다");
static_assert(WL_H >= 96 && WB_H >= 96, "목록 줄·버튼은 96px 이상이어야 한다");
static_assert(WL_Y + WL_ROWS * (WL_H + WL_GAP) - WL_GAP <= WB_Y, "목록이 하단 버튼과 겹친다");
Rect pkey_rect(const PKey &k) { return Rect{KB_MARGIN + k.col * KPX, KB_Y + k.row * KPY + KB_GAP / 2, k.span * KPX - KB_GAP, KPY - KB_GAP}; }
constexpr Rect PW_FIELD = {40, 88, 1000, 96}, PW_SHOW = {1056, 88, 184, 96};
constexpr int PW_PAD = 24;                                            // 비밀번호 칸 안쪽 여백
constexpr size_t PW_MAX = 63;                                         // WPA 비밀번호 최대 길이
constexpr int OK_CLOSE_MS = 1500;                                     // "연결됨" 을 보여 준 뒤 닫히기까지
constexpr int ROW_R = 26, KEY_R = 20, FIELD_R = 26;                   // 키캡·판의 모서리(W-D3)

// ── 상태 (화면 태스크 전용) ─────────────────────────────────────────────────
enum class View : uint8_t { LIST, PASS };
bool s_open = false;
View s_view = View::LIST;
int  s_page = 0;
std::vector<wifistore::Ap> s_list;                                    // 마지막으로 그린 목록 (W-D1)
std::string s_sel, s_pw;
bool s_pw_show = false, s_shift = false, s_sym = false;
bool s_dirty = false;                                                 // 터치로 화면 내용이 바뀜
uint32_t s_seen_seq = 0;                                              // 마지막으로 그릴 때의 netwifi::change_seq()
int64_t  s_ack_ms = -1;                                               // (이 시각, 이 상태)의 수동 연결 결과는 이미 본 것으로 친다 (W-D2)
Manual   s_ack_state = Manual::IDLE;

int64_t now_ms() { return esp_timer_get_time() / 1000; }

// 화면에 보여 줄 수동 연결 상태. 이미 본 결과는 IDLE 로 가린다.
// 시각과 상태를 함께 비교한다 — 시각만 보면 같은 밀리초에 상태가 또 바뀐 경우 새 상태까지 가려진다.
Manual manual()
{
    const Manual st = netwifi::manual_state();
    return (st == s_ack_state && netwifi::manual_since_ms() == s_ack_ms) ? Manual::IDLE : st;
}
void ack_manual()
{
    const Manual st = netwifi::manual_state();
    if (st == Manual::CONNECTING) return;                             // 진행 중인 시도는 가리지 않는다(입력 잠금이 풀리면 안 된다)
    s_ack_state = st;
    s_ack_ms = netwifi::manual_since_ms();
}

void wipe_password()
{
    std::fill(s_pw.begin(), s_pw.end(), '\0');
    s_pw.clear();
    s_pw_show = false; s_shift = false; s_sym = false;
}

int pages() { return std::max<int>(1, ((int)s_list.size() + WL_ROWS - 1) / WL_ROWS); }

// 폭에 맞지 않으면 끝을 "..." 로 줄인다(UTF-8 글자 단위)
std::string fit(const std::string &s, int max_w, float size)
{
    if (gfx::text_w(s.c_str(), size) <= max_w) return s;
    std::string t = s;
    while (!t.empty()) {
        size_t i = t.size() - 1;
        while (i > 0 && ((unsigned char)t[i] & 0xC0) == 0x80) --i;    // 여러 바이트 글자의 첫 바이트까지
        t.erase(i);
        const std::string u = t + "...";
        if (gfx::text_w(u.c_str(), size) <= max_w) return u;
    }
    return "...";
}

// ── 작은 그림(원본과 같은 모양) ──────────────────────────────────────────────
void draw_bars(int x, int cy, int n)
{
    for (int k = 0; k < 4; ++k) { int h = 10 + k * 8; gfx::fill_rrect(x + k * 12, cy + 18 - h, 8, h, 2, k < n ? toy::INK : toy::PALE); }
}

void draw_lock(int x, int cy, uint32_t c)
{
    gfx::fill_rect(x, cy - 2, 18, 14, c);                                                          // 몸통
    gfx::fill_rect(x + 3, cy - 11, 3, 10, c); gfx::fill_rect(x + 12, cy - 11, 3, 10, c); gfx::fill_rect(x + 3, cy - 13, 12, 3, c);   // 고리
}

// 지우기(⌫): 왼쪽 화살 모양 + 가위표
void draw_backspace(int cx, int cy, uint32_t fg, uint32_t bg)
{
    for (int dy = -14; dy <= 14; ++dy) {                              // 왼쪽 삼각형: 꼭짓점 (cx-26, cy), 밑변 x = cx-10
        const int len = 16 * (14 - std::abs(dy)) / 14;
        gfx::fill_rect(cx - 10 - len, cy + dy, len, 1, fg);
    }
    gfx::fill_rect(cx - 10, cy - 14, 34, 29, fg);
    for (int i = 0; i <= 14; ++i) {                                   // 가위표(2px 굵기)
        gfx::fill_rect(cx - 2 + i, cy - 7 + i, 2, 1, bg);
        gfx::fill_rect(cx - 2 + i, cy + 7 - i, 2, 1, bg);
    }
}

// 대문자(⇧): 위쪽 삼각형 + 기둥
void draw_shift(int cx, int cy, uint32_t fg)
{
    for (int yy = -20; yy <= 2; ++yy) {                               // 꼭짓점 (cx, cy-20), 밑변 y = cy+2 (좌우 18)
        const int half = 18 * (yy + 20) / 22;
        gfx::fill_rect(cx - half, cy + yy, 2 * half + 1, 1, fg);
    }
    gfx::fill_rect(cx - 7, cy + 2, 14, 14, fg);
}

// 현재 벌의 글자(영문 대문자는 ⇧)
std::string pkey_char(const PKey &k)
{
    std::string c = s_sym ? k.sym : k.lo;
    if (!s_sym && s_shift && c.size() == 1 && c[0] >= 'a' && c[0] <= 'z') c[0] = (char)(c[0] - 'a' + 'A');
    return c;
}

// ── 목록 화면 ───────────────────────────────────────────────────────────────
void draw_list()
{
    s_list = netwifi::scan_results();                                 // W-D1
    const bool busy = netwifi::scan_busy();
    const bool linked = netwifi::connected();
    char cur[33];
    netwifi::ssid(cur, sizeof cur);
    const int np = pages();
    if (s_page >= np) s_page = np - 1;

    // 맨 윗줄: 제목(왼쪽) · 쪽 번호(가운데) · 현재 연결(오른쪽)
    gfx::blit_asset(&lbl_wf_title, WL_X, HEAD_CY - lbl_wf_title.h / 2, toy::INK);
    char pg[32];
    snprintf(pg, sizeof pg, "%d / %d", s_page + 1, np);
    gfx::text_center(pg, SCREEN_W / 2, HEAD_CY, toy::MUTED, 1.5f);
    const int right = SCREEN_W - WL_X;
    if (linked) {
        const std::string name = fit(cur, 420, 1.5f);                 // 가운데 쪽 번호와 겹치지 않는 폭
        const int nw = gfx::text_w(name.c_str(), 1.5f);
        gfx::text(name.c_str(), right - nw, HEAD_CY, toy::INK, 1.5f);
        gfx::blit_asset(&lbl_wf_connected, right - nw - 12 - lbl_wf_connected.w, HEAD_CY - lbl_wf_connected.h / 2, toy::INK);
    } else {
        gfx::blit_asset(&lbl_wf_disconnected, right - lbl_wf_disconnected.w, HEAD_CY - lbl_wf_disconnected.h / 2, toy::MUTED);
    }

    // 목록
    if (s_list.empty()) {
        const int cy = WL_Y + (WB_Y - 16 - WL_Y) / 2;
        gfx::blit_asset_center(busy ? &lbl_wf_scanning : &lbl_wf_empty, SCREEN_W / 2, cy, toy::MUTED);
    }
    for (int i = 0; i < WL_ROWS; ++i) {
        const int idx = s_page * WL_ROWS + i;
        if (idx >= (int)s_list.size()) break;
        const auto &a = s_list[idx];
        const Rect r = wl_row(i);
        const int cy = toy::keycap_cy(r);
        const bool is_cur = linked && a.ssid == cur;
        const bool known = netwifi::is_known(a.ssid);
        toy::keycap(r, is_cur ? toy::GO_SOFT : toy::WHITE, ROW_R);   // 지금 연결된 줄은 연두
        const int rx = r.x + r.w - 28 - 44;                           // 신호 막대(폭 44)의 왼쪽
        draw_bars(rx, cy, wifistore::bars(a.rssi));
        if (a.secured) draw_lock(rx - 44, cy, toy::MUTED);
        int text_right = rx - 64;                                     // 자물쇠 왼쪽
        const ui_asset_t *tag = is_cur ? &lbl_wf_connected : known ? &lbl_wf_saved : nullptr;
        if (tag) {
            gfx::blit_asset(tag, text_right - tag->w, cy - tag->h / 2, is_cur ? toy::INK : toy::MUTED);
            text_right -= tag->w;
        }
        const int tx = r.x + 28;
        gfx::text(fit(a.ssid, text_right - 24 - tx, 2.0f).c_str(), tx, cy, toy::INK, 2.0f);
    }

    // 하단 버튼
    toy::button(wl_btn(0), &lbl_wf_prev, s_page > 0);
    toy::button(wl_btn(1), &lbl_wf_next, s_page + 1 < np);
    toy::button(wl_btn(2), busy ? &lbl_wf_rescan_busy : &lbl_wf_rescan, !busy);
    toy::button(wl_btn(3), &lbl_wf_close);
}

void tap_list(int tx, int ty)
{
    const int np = pages();
    if (wl_btn(0).hit(tx, ty)) { if (s_page > 0) --s_page; }
    else if (wl_btn(1).hit(tx, ty)) { if (s_page + 1 < np) ++s_page; }
    else if (wl_btn(2).hit(tx, ty)) { if (!netwifi::scan_busy()) netwifi::request_scan(); }
    else if (wl_btn(3).hit(tx, ty)) { uiwifi::close(); return; }
    else for (int i = 0; i < WL_ROWS; ++i) {
        const int idx = s_page * WL_ROWS + i;
        if (idx >= (int)s_list.size() || !wl_row(i).hit(tx, ty)) continue;
        const auto &a = s_list[idx];
        const bool connect_saved = netwifi::is_known(a.ssid) || !a.secured;   // 아는 망·열린 망은 비밀번호를 묻지 않고 바로 연결
        s_sel = a.ssid;
        wipe_password();
        ack_manual();
        s_view = View::PASS;
        ESP_LOGI(TAG, "wifi setup: selected %s (%s)", s_sel.c_str(), connect_saved ? "saved/open → connect" : "needs password");
        if (connect_saved) netwifi::request_connect(s_sel.c_str(), "", true);
        break;
    }
    s_dirty = true;
}

// ── 자판 화면 ───────────────────────────────────────────────────────────────
void draw_pass()
{
    const Manual st = manual();

    // 맨 윗줄 오른쪽: 상태 문구. 안내(힌트)는 흐린 글자, 진행·결과는 알약(연결 중 흰색, 연결됨 초록, 실패 빨강)
    const ui_asset_t *msg = &lbl_wf_hint;
    uint32_t fill = 0;                                                // 0 = 알약 없음
    switch (st) {
    case Manual::CONNECTING:   msg = &lbl_wf_connecting;   fill = toy::WHITE; break;
    case Manual::OK:           msg = &lbl_wf_connected;    fill = toy::GO;    break;
    case Manual::FAIL_AUTH:    msg = &lbl_wf_fail_auth;    fill = toy::STOP;  break;
    case Manual::FAIL_TIMEOUT: msg = &lbl_wf_fail_timeout; fill = toy::STOP;  break;
    case Manual::SHORT_PASS:   msg = &lbl_wf_short_pass;   fill = toy::STOP;  break;
    default: break;
    }
    const int pill_h = 48, pill_w = msg->w + pill_h + 2 * toy::PILL_OUTLINE;
    const int msg_x = SCREEN_W - 40 - (fill ? pill_w : msg->w);       // 문구(또는 알약)의 왼쪽 끝 — 망 이름은 이 앞에서 줄인다
    if (fill) toy::label_pill(msg, msg_x + pill_w / 2, HEAD_CY, pill_h, fill);
    else gfx::blit_asset(msg, msg_x, HEAD_CY - msg->h / 2, toy::MUTED);

    // 맨 윗줄 왼쪽: "<망 이름> 에 연결" (이름이 길면 상태 문구 앞에서 줄인다)
    const int name_max = msg_x - 32 - lbl_wf_join.w - 12 - 40;
    const int nw = gfx::text(fit(s_sel, name_max, 2.0f).c_str(), 40, HEAD_CY, toy::INK, 2.0f);
    gfx::blit_asset(&lbl_wf_join, 40 + nw + 12, HEAD_CY - lbl_wf_join.h / 2, toy::INK);

    // 비밀번호 칸 + 보기/숨기기
    toy::panel(PW_FIELD, toy::WHITE, FIELD_R);                        // 입력 칸은 누르는 것이 아니다 → 납작한 판
    {
        const int max_w = PW_FIELD.w - 2 * PW_PAD - 8;                // 커서 자리 포함
        auto make = [&](size_t from) {
            std::string v;
            if (s_pw_show) v = s_pw.substr(from);
            else for (size_t i = from; i < s_pw.size(); ++i) v += "●";
            return v;
        };
        size_t from = 0;                                              // 칸을 넘치면 앞쪽을 잘라 끝(커서)이 보이게
        std::string shown = make(0);
        while (from < s_pw.size() && gfx::text_w(shown.c_str(), 2.0f) > max_w) shown = make(++from);
        const int cy = PW_FIELD.y + PW_FIELD.h / 2;
        const int tw = gfx::text(shown.c_str(), PW_FIELD.x + PW_PAD, cy, toy::INK, 2.0f);
        gfx::fill_rect(PW_FIELD.x + PW_PAD + tw + 2, cy - 24, 4, 48, toy::VOL);   // 커서
    }
    toy::button(PW_SHOW, s_pw_show ? &lbl_wf_hide : &lbl_wf_show);

    // 자판
    for (int i = 0; i < NPKEYS; ++i) {
        const PKey &k = PKEYS[i];
        const Rect r = pkey_rect(k);
        const int cx = r.x + r.w / 2, cy = toy::keycap_cy(r);
        switch (k.act) {                                              // 글자 글쇠는 흰색, 기능 글쇠는 옅은 보라, 연결은 초록, 켜진 대문자는 노랑
        case KA_CHAR:
            toy::keycap(r, toy::WHITE, KEY_R);
            gfx::text_center(pkey_char(k).c_str(), cx, cy, toy::INK, 2.0f);
            break;
        case KA_BACK:
            toy::keycap(r, toy::PALE, KEY_R);
            draw_backspace(cx, cy, toy::INK, toy::PALE);
            break;
        case KA_SHIFT: {
            const bool on = s_shift && !s_sym;
            toy::keycap(r, on ? toy::WARN : toy::PALE, KEY_R);
            if (!s_sym) draw_shift(cx, cy, toy::INK);                // 기호 벌에서는 대문자 전환이 없다
            break;
        }
        case KA_SYM:
            toy::keycap(r, toy::PALE, KEY_R);
            gfx::text_center(s_sym ? "ABC" : "#+=", cx, cy, toy::INK, 2.0f);
            break;
        case KA_SPACE:
            toy::keycap(r, toy::WHITE, KEY_R);
            gfx::fill_rrect(cx - r.w / 4, cy - 3, r.w / 2, 6, 3, toy::MUTED);
            break;
        case KA_CANCEL:
            toy::keycap(r, toy::PALE, KEY_R);
            gfx::blit_asset_center(&lbl_wf_cancel, cx, cy, toy::INK);
            break;
        case KA_OK:
            toy::keycap(r, st == Manual::CONNECTING ? toy::PALE : toy::GO, KEY_R);
            gfx::blit_asset_center(&lbl_wf_connect, cx, cy, st == Manual::CONNECTING ? toy::MUTED : toy::INK);
            break;
        }
    }
}

void tap_pass(int tx, int ty)
{
    const Manual st = manual();
    // 연결 시도 중에는 입력 잠금(원본). 성공 표시 중(1.5초)에도 잠근다 — 원본은 이때 누르면 자동 닫힘이 취소돼 자판 화면에 남았다
    if (st == Manual::CONNECTING || st == Manual::OK) return;
    bool do_connect = false, cancel = false;
    if (PW_SHOW.hit(tx, ty)) s_pw_show = !s_pw_show;
    else for (int i = 0; i < NPKEYS; ++i) {
        const PKey &k = PKEYS[i];
        if (!pkey_rect(k).hit(tx, ty)) continue;
        switch (k.act) {
        case KA_CHAR:   if (s_pw.size() < PW_MAX) s_pw += pkey_char(k); break;
        case KA_BACK:   if (!s_pw.empty()) s_pw.pop_back(); break;
        case KA_SHIFT:  if (!s_sym) s_shift = !s_shift; break;
        case KA_SYM:    s_sym = !s_sym; break;
        case KA_SPACE:  if (s_pw.size() < PW_MAX) s_pw += ' '; break;
        case KA_CANCEL: cancel = true; break;
        case KA_OK:     do_connect = true; break;
        }
        break;
    }
    s_dirty = true;
    if (!do_connect) ack_manual();                                    // 다시 입력하면 이전 오류 문구를 지운다
    if (cancel) { wipe_password(); s_view = View::LIST; return; }
    if (do_connect) netwifi::request_connect(s_sel.c_str(), s_pw.c_str(), false);   // 8자 미만이면 SHORT_PASS 로 돌아온다. 비밀번호는 로그에 남기지 않는다
}

}  // namespace

namespace uiwifi {

void open()
{
    if (s_open) return;
    s_open = true;
    s_view = View::LIST;
    s_page = 0;
    s_sel.clear();
    wipe_password();
    ack_manual();
    netwifi::set_setup_open(true);
    netwifi::request_scan();
    s_dirty = true;
}

bool is_open() { return s_open; }

void close()
{
    if (!s_open) return;
    s_open = false;
    wipe_password();
    ack_manual();
    netwifi::set_setup_open(false);
}

void draw()
{
    if (!s_open) return;
    s_seen_seq = netwifi::change_seq();                               // 값을 읽기 전에 적어 둔다 → 그리는 도중의 변화는 다음 needs_redraw() 가 잡는다
    s_dirty = false;
    toy::background(toy::Scene::FLAT);
    if (s_view == View::LIST) draw_list(); else draw_pass();
    gfx::set_private(uilayout::is_private_frame(s_view == View::PASS, s_pw.size()));   // I-1: 입력이 든 자판 화면은 스크린샷 거절(gfx G-D4). 프레임 시작(clear)에서 공개로 되돌아간다
}

bool tap(int x, int y)
{
    if (!s_open) return false;
    if (s_view == View::LIST) tap_list(x, y); else tap_pass(x, y);
    return s_open;
}

bool needs_redraw()
{
    if (!s_open) return false;
    // 연결 성공: "연결됨" 을 1.5초 보여 준 뒤 스스로 닫는다(부르는 쪽은 is_open() 으로 알아챈다)
    if (s_view == View::PASS && manual() == Manual::OK && now_ms() - netwifi::manual_since_ms() > OK_CLOSE_MS) { close(); return false; }
    return s_dirty || netwifi::change_seq() != s_seen_seq;
}

bool on_keyboard() { return s_open && s_view == View::PASS; }

}  // namespace uiwifi
