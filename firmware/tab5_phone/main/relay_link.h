// relay_link.h — 교환기(WebSocket) 연결. 붙고, 끊기면 다시 붙고(1→2→4…30초, 토큰 거절은 60초), 15초마다 생존 확인을 보낸다.
// 설계서 §9.3·§9.4·§9.5·§9.7. 부품: espressif/esp_websocket_client 1.8.0.
//   - 받은 신호는 사건 대기열에 쌓인다 → 통화 진행 태스크(app)가 poll() 로 꺼낸다. 이 모듈은 통화 상태를 모른다.
//   - 신호 규약 v2(전화기 3대 — relay_link.cpp RL-D13): 접속 때 X-Phone-Proto: 2 를 싣는다. 받은 peer·ring 의 기기 ID 는 sig.peer_id 에 그대로 실려 나간다
//     — 그 ID 를 상대 slot 으로 바꾸는 것은 app 의 일이다(roster.h route_slot). 이 모듈은 내 번호도 모른다.
//   - 음성 프레임(Phase 3)은 대기열을 거치지 않고 등록한 함수로 바로 넘긴다(웹소켓 태스크에서 불린다 — 복사만 하고 바로 돌아올 것).
#pragma once
#include <cstddef>
#include <cstdint>

#include "relay_proto.h"

namespace relaylink {

constexpr unsigned URL_BUF = 160;   // 교환기 주소를 담는 버퍼 크기(끝 NUL 포함). 이보다 긴 RELAY_URL 은 phone_main.cpp 의 static_assert 가 빌드에서 막는다(M-9)

struct In {
    enum class Kind : uint8_t { LINK_UP, LINK_DOWN, SIGNAL } kind;
    relayproto::Signal sig;         // kind == SIGNAL 일 때만. peer·ring 이면 sig.peer_id = 그 상대의 기기 ID(v1 모양이면 빈 글자)
};

// 한 번만 부른다. 태스크를 띄우고 바로 돌아온다. Wi-Fi 가 붙으면 접속을 시작한다.
// url: "ws://192.168.0.10:5094/ws"(맥북 교환기) 또는 "wss://…/ws"(운영). dev_id·token 은 접속 요청 헤더로만 나간다.
void start(const char *url, const char *dev_id, const char *token);

bool up();                          // 교환기에 붙어 있나
int  http_code();                   // 교환기가 접속을 거절한 코드(401 = 토큰 거절). 거절이 아니면 0
int  attempt();                     // 연속 실패 횟수(붙으면 0)
uint32_t change_seq();              // 위 값이 바뀔 때마다 1 증가

bool poll(In *out);                 // 받은 사건 하나를 꺼낸다. 없으면 false. 통화 진행 태스크에서만 부른다

// 보내기와 reconnect 는 같은 잠금으로 묶여 있다(relay_link.cpp RL-D10) — 어느 태스크에서 불러도 된다.
bool send_text(const char *text);                       // 신호 보내기. 붙어 있지 않거나 실패하면 false. 다른 보내기·재접속이 끝나기를 2초까지 기다린다
bool send_voice(const uint8_t *frame, size_t len);      // 음성 프레임 [순번 2][Opus] 보내기. 전용 송신 태스크에서(소켓이 막히면 최대 2초 멈춘다). 잠금은 20ms 만 기다린다 — 못 얻으면 그 프레임은 버린다

void reconnect();                   // 지금 연결을 끊고 처음부터 다시 붙는다(call_fsm 의 A_RECONNECT). 바로 돌아온다

// 이 모듈이 쓰는 두 태스크(relay, relay_ws)의 스택 최저 여유(바이트). 태스크가 없으면 0. STAT 줄에 싣는다.
void stack_free(unsigned *relay_task, unsigned *ws_task);

typedef void (*voice_sink_t)(const uint8_t *frame, size_t len);
void set_voice_sink(voice_sink_t fn);                   // Phase 3: 받은 음성 프레임을 넘겨받을 함수

// Phase 3c(relay_link.cpp RL-D12): 교환기 소켓의 TCP_NODELAY. 값은 기억해 두고 다시 붙을 때마다 건다. 저장하지 않는다(부팅 값 = voicetune::DEFAULT_NODELAY).
int  set_nodelay(bool on);          // 돌려주는 값 = 지금 옵션을 건 소켓 수(붙어 있지 않으면 0 — 값만 기억), 잠금을 2초 안에 못 얻으면 -1
bool nodelay();                     // 기억한 값
int  nodelay_socks();               // 마지막으로 옵션을 건 소켓 수(-1 = 아직 건 적 없음)

}  // namespace relaylink
