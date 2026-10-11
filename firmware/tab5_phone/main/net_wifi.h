// net_wifi.h — Wi-Fi 매니저. 저장된 망(NVS "snapwifi"/"list") + secrets.h 고정 목록 중 신호 센 것에 스스로 붙고, 끊기면 다시 붙는다.
// 출처: firmware/tab5_snapbooth/main/main.cpp 의 Wi-Fi 매니저(D11·D12·D13)를 화면·카메라 코드와 떼어 옮긴 것.
//   - NVS 네임스페이스·키·형식을 스냅부스와 같게 둔다 → 2번 기기는 스냅부스에서 저장한 목록을 그대로 물려받는다.
//   - 화면은 이 모듈을 읽기만 한다(change_seq 가 바뀌면 다시 그림). 이 모듈은 화면 코드를 부르지 않는다.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "wifi_store.h"

namespace netwifi {

enum class Phase : uint8_t {
    STARTING,       // Wi-Fi 칩 초기화 중
    SEARCHING,      // 아는 망을 찾는 중(스캔·연결 시도)
    CONNECTED,
    NO_KNOWN,       // 아는 망이 하나도 안 보임 → 화면: "Wi-Fi 연결 안 됨 — 배지를 눌러 설정"
    INIT_FAILED,    // Wi-Fi 칩 초기화 실패(로그에 "PHONE FAIL esp_hosted …" 또는 "PHONE FAIL wifi …"). 스스로 회복하지 않는다(재부팅 필요)
};

enum class Manual : uint8_t { IDLE, CONNECTING, OK, FAIL_AUTH, FAIL_TIMEOUT, SHORT_PASS };   // 설정 화면에서 누른 "연결"의 진행 상태

// nvs_flash_init() 과 M5.begin() 뒤에 한 번. 매니저 태스크를 띄우고 바로 돌아온다.
void start();

Phase phase();
bool  connected();
void  ssid(char *out, size_t cap);          // 연결된 망 이름(없으면 빈 글자)
int   rssi();                               // 연결된 망의 신호(dBm), 없으면 -127. 매니저가 10초마다 읽어 둔 값(연결 직후 1초쯤은 -127)
int   ps();                                 // Wi-Fi 절전 상태(N-D5): 0 = 꺼짐(통화에 필요한 값), 1·2 = 켜짐, -1 = 못 읽음·아직 안 읽음. 매니저가 10초마다 읽어 둔 값
uint32_t change_seq();                      // 위 상태·스캔 결과·수동 연결 상태가 바뀔 때마다 1 증가
                                            //   (신호는 막대 칸수 wifistore::bars 가 바뀔 때만 — 1dBm 흔들림으로 화면을 다시 그리지 않게)

// ── 설정 화면용 ────────────────────────────────────────────────────────────
void set_setup_open(bool open);             // 열려 있는 동안 자동 재스캔을 멈춘다(목록이 흔들리지 않게)
void request_scan();
bool scan_busy();                           // 스캔 중이거나 요청이 아직 처리되지 않았으면 true(request_scan 직후부터 true)
std::vector<wifistore::Ap> scan_results();  // 마지막 스캔(신호 센 순, 같은 이름은 하나)
bool is_known(const std::string &ssid);     // 저장됐거나 고정 목록에 있는 망
// 15초 제한. 성공한 것만 저장한다.
//   - use_saved=true: 아는 망이면 저장된 비밀번호로, 모르는 망이면 비밀번호 없이(열린 망) 붙는다. pass 는 무시.
//   - use_saved=false 이고 pass 가 8자 미만이면 시도하지 않고 바로 Manual::SHORT_PASS.
//   - 부르는 즉시 Manual::CONNECTING 이 된다. 이미 CONNECTING 이면 요청을 버린다.
void request_connect(const char *ssid, const char *pass, bool use_saved);
Manual  manual_state();                     // 다음 request_connect 까지 마지막 결과가 남아 있다(스스로 IDLE 로 돌아가지 않는다)
int64_t manual_since_ms();                  // manual_state 가 마지막으로 바뀐 시각(esp_timer ms)
// 저장 목록에서 지운다(ssid 가 nullptr 또는 "" 이면 저장 목록 전부). secrets.h 고정 목록은 지워지지 않는다.
// 지금 붙어 있는 연결은 끊지 않는다(스냅부스 wforget 과 같다). 그 연결이 끊기면 "같은 후보 3회" 규칙대로 같은 설정으로
// 두 번까지 다시 붙어 보므로 곧바로 떨어지지 않을 수 있다 — 확실히 떼려면 지운 뒤 재부팅한다.
void request_forget(const char *ssid);

}  // namespace netwifi
