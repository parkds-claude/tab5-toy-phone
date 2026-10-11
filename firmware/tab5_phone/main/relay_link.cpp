// relay_link.cpp — 교환기 연결. 설명은 relay_link.h.
//
// esp_websocket_client 1.8.0 을 소스로 확인하고 정한 것(2026-10-04, 기기 검증은 Phase 2 실행 때):
//   RL-D1 생존 확인은 두 겹. 교환기의 "45초 무소식이면 끊음"이 세는 것은 글자·이진 프레임뿐이라 JSON ping 을 15초마다 직접 보낸다.
//         부품의 제어 PING(5초 간격, 15초 무응답이면 끊음)은 기기 쪽에서 죽은 연결을 빨리 알아채는 용도다.
//   RL-D2 교환기가 닫기 프레임으로 끊으면(같은 ID 재접속으로 밀려남, 45초 무소식) 기본 설정의 부품은 멈춘다 → enable_close_reconnect.
//   RL-D3 재연결 간격은 이벤트 핸들러에서 set_reconnect_timeout 으로 정한다. 연결이 60초 넘게 유지됐을 때만 간격을 처음으로 되돌린다
//         — 같은 ID 를 쓰는 다른 접속(phone_smoke.py 등)과 1초마다 서로 밀어내는 것을 막는다. 비용: 금방 끊긴 뒤의 재접속이 느려진다.
//   RL-D4 토큰 거절(401)은 "접속 단계에서 끊겼을 때"만 본다. 부품이 상태 코드를 지우지 않아, 통화 중 끊김에도 옛 401 이 남아 있기 때문이다.
//   RL-D5 "끊고 다시 붙기"(A_RECONNECT)는 stop → start. 이때 부품은 끊김 이벤트를 내지 않으므로 여기서 LINK_DOWN 을 직접 올린다.
//         stop 은 이벤트 핸들러 안에서 부를 수 없어 전용 태스크(relay)에서 처리한다.
//   RL-D6 보내기 시간 제한은 2초. 이 안에 소켓이 뚫리지 않으면 부품이 연결을 끊는다 — 짧게 주면 Wi-Fi 가 잠깐 막힐 때마다 통화가 끊긴다.
//         비용: Wi-Fi 가 막힌 순간에 신호를 보내면 부르는 쪽(화면 태스크)이 최대 2초 멈춘다.
//   RL-D7 붙자마자 JSON ping 을 한 번 보낸다. 교환기는 접속 응답 바로 뒤에 peer 신호를 붙여 보내는데, 둘이 한 번에 도착하면 peer 가
//         전송 계층의 내부 버퍼에 남고, 부품은 "읽을 것이 있나"를 소켓으로만 봐서 다음 수신(최대 5초 뒤의 제어 PONG)까지 꺼내지 않는다
//         (transport_ws.c 의 poll_read). 즉시 보낸 ping 의 pong 이 소켓을 깨워 그 지연을 없앤다.
//   RL-D8 esp_websocket_client_start() 가 ESP_FAIL(전송 객체·태스크 생성 실패, esp_websocket_client.c:1469-1479)이면 relay_task 가 5초마다 다시 부른다
//         (이 태스크에서만 — stop/start 와 send_text 의 경합을 넓히지 않는다). stop 을 부르기 전에 s_phase 를 IDLE 로 내려 stop 이 끝날 때까지
//         up() 이 거짓말하지 않게 하고, stop 이 끝난 뒤에도 한 번 더 내린다(N-1: 접속 중에 stop 이 걸리면 stop 도중 핸들러가 UP 으로 올릴 수 있다).
//         이유: 반환값을 버리면 한 번의 실패가 재부팅 전까지 영구 불통이 된다(M-2).
//         비용: 실패가 계속되면 5초마다 로그 한 줄. 탈출구: 재시도 블록(start_pending)을 지우면 이전 동작(한 번만 시도)이다.
//   RL-D9 토큰이 든 접속 요청 전문을 오류 로그로 찍는 transport_ws 를 막는 장치는 esp_log_level_set 이고, 그것은 CONFIG_LOG_DYNAMIC_LEVEL_CONTROL=y 에 기댄다
//         → sdkconfig.defaults 에 고정하고 꺼지면 빌드가 실패하게 한다(M-4). 탈출구: 둘 다 지우면 Kconfig 기본값에 다시 기댄다.
//   RL-D10 (Phase 3b) 보내기(send_text·send_voice)와 "끊고 다시 붙기"의 stop → start 를 같은 잠금(s_tx_lock)으로 묶는다.
//         이유: esp_websocket_client_start() 는 전송 객체 목록을 지우고 새로 만든다(esp_websocket_client.c 의 create_transport —
//         부품의 잠금을 쥐지 않는다). 그때 다른 태스크가 send 안에서 옛 전송 객체를 쓰고 있으면 해제된 메모리를 쓴다.
//         3b 부터 음성 송신 태스크가 20ms 마다 보내므로 실제로 겹친다. 보내는 쪽은 잠금을 쥔 뒤에 연결 상태를 다시 본다.
//         신호는 잠금을 2초까지(지금의 RL-D6 과 같은 크기), 음성은 20ms 까지만 기다린다 — 음성은 밀리면 버린다.
//         비용: 음성 송신이 Wi-Fi 때문에 소켓에서 막혀 있으면(최대 2초) 그동안 신호(끊기 등)도 잠금을 기다린다. 2초 안에 못 보내면
//         send_text 가 false → 상태 기계가 3초 뒤 연결을 끊고 다시 붙어 양쪽 통화를 끝낸다(FSM-D1). 탈출구: 잠금을 빼면 이전 동작.
//   RL-D11 (3a 최종 검수 M-8) stop 이 도는 동안 접속이 완료되면 핸들러가 LINK_UP 을 대기열에 넣는다. stop 뒤에 그 흔적(UP)이 보이면
//         LINK_DOWN 을 뒤따라 올려 상태 기계가 "붙었다 → 끊겼다"로 읽게 한다(전에는 LINK_UP 만 남아 잠깐 대기 화면이 됐다).
//         fix 1 F2: 재접속 블록 첫머리의 "UP 이었나" 읽기와 IDLE 쓰기도 exchange 한 번으로 묶었다(따로면 그 사이 UP 이 된 것을 놓쳤다).
//   RL-D12 (Phase 3c, 계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D6, 3b 최종 검수 M-4) 교환기 소켓의 TCP_NODELAY 를 실행 중에 켜고 끈다(set_nodelay).
//         왜: 전송 계층은 프레임마다 머리(6바이트)와 몸통(62바이트)을 두 번에 나눠 쓴다(tcp_transport/transport_ws.c:420·428). 기본 동작(Nagle)에서는 몸통이 앞 조각의 확인 응답이 올 때까지
//             잡혀 있어 프레임이 뭉쳐 나가고, 조각 하나를 잃으면 뒤 조각이 안 나가 빠른 재전송이 걸리지 않는다(재전송 시한까지 0.5초 넘게 멈춘다).
//         길: 부품에는 설정 칸이 없다(esp_websocket_client 1.8.0 의 설정 구조체·tcp_transport·esp-tls 어디에도 TCP_NODELAY 가 없다 — 검색 0건). 소켓 번호를 얻는 공식 길은
//             전송 객체를 직접 만들어 넘기는 것(cfg.ext_transport + esp_transport_get_socket)인데, 그러면 접속 경로(주소·토큰 헤더·제어 프레임 설정)를 우리가 다시 짜야 하고
//             틀리면 교환기에 붙지 못한다(기기에서만 드러난다). 그래서 접속 경로는 건드리지 않고, 붙은 뒤에 lwIP 의 소켓 번호 범위(LWIP_SOCKET_OFFSET 부터 CONFIG_LWIP_MAX_SOCKETS 개,
//             lwip/port/include/lwipopts.h:986)를 훑어 "연결된 TCP 소켓"에 옵션을 건다. 이 펌웨어의 TCP 연결은 교환기 하나뿐이다(HTTP·MQTT 를 쓰지 않는다).
//         비용: 소켓을 "찾아서" 건다는 점(다른 TCP 연결이 생기면 거기에도 걸린다 — 해가 되지는 않는다). 작은 조각이 그대로 나가 패킷 수가 는다(프레임당 2개).
//         언제: set_nodelay 가 그 자리에서 한 번, 그리고 다시 붙을 때마다(CONNECTED) 지금 값으로 다시. 보내기와 같은 잠금(s_tx_lock) 안에서 건다.
//         탈출구: 기본값 0(끔)이면 3b 와 같다. 효과가 없으면 손잡이째 지운다. 더 엄밀하게 하려면 ext_transport 로.
//
// Phase 4 — 주소가 wss:// 면 TLS 로 붙는다(집 ↔ 사무실: Cloudflare 터널 뒤의 맥미니 교환기). ws:// 는 그대로 된다(LAN 개발·되돌리기용).
// 부품 소스로 확인하고 정한 것(2026-10-05, ESP-IDF 5.5.4 · esp_websocket_client 1.8.0 — 기기 검증은 합친 뒤 컨트롤러가):
//   P4N-D1 서버 검증은 인증서 묶음 전체(esp_crt_bundle_attach)로 한다. 묶음을 걸면 esp-tls 가 검증 필수로 놓고(esp-tls/esp_tls_mbedtls.c:944-948),
//         호스트 이름도 맞춰 본다(같은 파일 878-890). 묶음도 다른 검증 수단도 없으면 esp-tls 는 붙지 않고 실패한다(982-988) — 검증 없이 붙는 길이 없다.
//         유효 기간은 보지 않는다(CONFIG_MBEDTLS_HAVE_TIME_DATE 끔 — mbedtls/library/x509.c:1097-1110). 기기에 시계가 없어(1970년) 켜면 전부 "아직 유효하지 않음"이 된다.
//         왜 전체 묶음: Cloudflare 가 발급 기관을 바꿔도 붙는다(플래시는 남는다 — 묶음은 플래시 → PSRAM 에 놓이고 내부 메모리를 쓰지 않는다). Phase 0 시험 펌웨어가 같은 설정으로 20회 붙었다.
//         비용: 만료된 인증서도 받는다(서명 사슬·이름은 본다). 탈출구: 시각 동기(SNTP)를 넣은 뒤 HAVE_TIME_DATE 를 켜고 아래 #error 를 지운다.
//   P4N-D2 mbedTLS 의 할당은 전부 PSRAM(CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC — mbedtls/port/esp_mem.c:18-19). 받는 버퍼 16.7KB·보내는 버퍼 4.4KB·악수 중 인증서·큰 수가 내부 메모리에서 빠진다.
//         왜: 내부 메모리 최저가 106,859(기준 100,000)인데 내부 할당이면 평소 25KB·악수 때 35KB 안팎을 더 쓴다(Phase 0: 161KB → 116KB). 레코드 한도(받기 16384)는 줄이지 않는다
//             — Cloudflare 는 조각 크기 협상을 받지 않아, 줄이면 인증서 사슬이 한 레코드에 안 들어가는 날 악수가 깨진다. 동적 버퍼는 앞 단계 결정대로 끈다.
//         비용: 그래도 내부에 남는 것 — esp_tls 구조체(calloc, esp-tls/esp_tls.c:177)와 lwIP 버퍼 몇 KB. 열쇠가 PSRAM 에 놓인다(이 기기는 플래시도 암호화하지 않는다).
//         탈출구: sdkconfig.defaults 의 그 줄을 지우면 내부 할당(아래 #error 도 함께).
//   P4N-D3 웹소켓 태스크 스택은 8192 그대로(내부 메모리 — 부품이 xTaskCreatePinnedToCore 로 만든다, esp_websocket_client.c:1475). TLS 악수가 이 스택에서 돈다.
//         근거: 평문에서 2,064 를 쓴다(stk_ws 6,128). mbedTLS 의 큰 것은 힙에서 나오고, IDF 의 TLS 예제·mqtt(6144)가 같은 깊이로 악수한다. 늘리면 그만큼 내부 메모리가 준다.
//         판정선(기기): wss 로 붙은 뒤 stk_ws ≥ 1,536. 모자라면 relay_diag.h 의 WS_TASK_STACK 을 12288 로.
//   P4N-D4 시간 값은 그대로다. 연결·악수·접속 응답 각각 10초(첫 TLS 연결 실측 1.4~1.9초의 5배), 제어 PING 5초·JSON ping 15초는 교환기의 45초 규칙과 Cloudflare 의 100초 규칙 안쪽이다.
//         TLS 에서도 RL-D7 은 필요하다: 전송 계층은 mbedTLS 에 남은 것은 보지만(tcp_transport/transport_ssl.c:173) 웹소켓 층의 내부 버퍼는 여전히 보지 않는다(transport_ws.c:682-686).
//         RL-D12 도 그대로 선다: TLS 소켓도 lwIP 의 TCP 소켓이고(esp-tls/esp_tls.c:229), 프레임은 여전히 두 번에 나뉘어 나간다(레코드 둘).
//   P4N-D5 접속이 실패한 까닭을 가른다(relay_diag.h classify). 부품은 접속 응답의 상태 코드를 지우지 않아(transport_ws.c:314) DNS·TLS 실패에도 묵은 401 이 보인다 →
//         전송 계층 오류(esp-tls 코드)가 있으면 그것으로, 없을 때만 상태 코드로 정한다. 토큰 거절 표시는 "이번 시도"의 까닭으로 다시 정한다(전에는 한 번 401 이면 붙을 때까지 남았다).
//         까닭은 "RELAYERR stage=…" 한 줄로 남긴다(숫자뿐 — 주소·토큰 없음). 비용: RL-D4 의 60초 간격이 401 이 아닌 실패에서는 풀린다(1→2→4…30초).
//         한계: "401 + 전송 오류 없음"은 토큰 거절로 둔다(진짜 401 이 이어질 때 60초 간격을 지키는 쪽이 먼저다). 401 뒤의 시도가 접속 응답을 기다리다 시간 초과로 끝나면
//         전송 계층이 아무것도 남기지 않아(tcp_transport/transport_ssl.c:266-272) 묵은 401 로 읽힌다 — 부품에는 "이번 시도에 응답을 받았는가"를 알려 주는 신호가 없다.
//   P4N-D7 (수정 1회차, 검수 Important 1·2) 전송 오류는 ERROR 사건에서 보관해 두었다가 DISCONNECTED 가 이어받는다(relay_diag.h ErrStash).
//         왜: 부품은 접속 실패 때 ERROR 를 먼저, DISCONNECTED 를 뒤에 낸다(esp_websocket_client.c:1213·1218 → 1220). 사건을 낼 때 직전 실패가 전송 오류였으면 esp-tls 오류를
//             읽고 지운다(:227-233, 표시는 :282 에서 켜지고 붙었을 때 :1251 에서만 꺼진다). 그래서 두 번째 연속 실패부터는 ERROR 가 오류를 가져가고 DISCONNECTED 에는 0 이 와서,
//             DNS·TLS 실패가 묵은 상태 코드로 판정됐다(묵은 401 이면 "기기 등록 오류"). 사건은 부르는 태스크에서 바로 실행된다(부품의 사건 루프에는 태스크가 없다 — :791-793,
//             내고 곧바로 돌린다 :239·246) → ERROR → DISCONNECTED 순서는 한 태스크 안에서 고정이다.
//         비용: 정적 변수 하나(핸들러에서만 만진다 — 핸들러는 부품의 잠금 아래 차례로 불린다). 탈출구: ERROR 덩이와 take 호출을 지우면 수정 전 동작.
//   P4N-D6 TLS 의 AES·SHA 는 소프트웨어로 한다(CONFIG_MBEDTLS_HARDWARE_AES·SHA 끔). 왜: P4N-D2 로 버퍼가 PSRAM 에 있으면 하드웨어 경로가 달라진다 — GCM 은 통째 가속을 못 쓰고
//         (mbedtls/port/aes/esp_aes_gcm.c:593-614) AES·SHA 가 PSRAM 을 DMA 로 직접 읽는 경로(…/aes/dma/esp_aes_dma_core.c:526-580, …/sha/core/sha.c:280-285)가 된다.
//         그 경로는 이 기기에서 돈 적이 없고(Phase 0 은 내부 버퍼), 레코드마다 내부 메모리에서 DMA 기술자를 잡았다 놓는다(통화 중 초당 200번쯤). 음성 프레임은 68바이트라 소프트웨어가 더 가볍다.
//         표는 플래시에 있다(MBEDTLS_AES_ROM_TABLES, port/include/mbedtls/esp_config.h:263 — 내부 메모리 0). 큰 수·타원곡선 가속은 레지스터 방식이라 그대로 둔다.
//         비용: 플래시 약간, 악수 때 해시 계산이 스택을 조금 더 쓴다(SHA-512 한 묶음 0.7KB). 탈출구: sdkconfig.defaults 의 두 줄과 아래 #error 의 해당 조건을 지운다.
//
// 전화기 3대 — 신호 규약 v2(설계서 docs/exec-plans/2026-10-08-phone-3way-design.md §6.1, 결정 TW-D7):
//   RL-D13 접속 요청에 헤더 X-Phone-Proto: 2 를 더 싣는다. 교환기는 이 헤더가 "2" 인 연결에만 v2 글자(peer 의 id, ring 의 from)를 보낸다. call 의 to 는 어느 연결의 것이든 읽되, 헤더가 없는(v1) 연결이 옛 짝이 아닌 기기를 부르면 offline 으로 끝낸다(server/phone_core.py — 설계서 TW-D23).
//         헤더 글자는 relay_proto.h 의 connect_headers 가 만든다(RP-D5 — 맥 시험이 줄 끝·버전 값을 고정한다. 스택짱 펌웨어도 이 파일을 끌어 쓸 계획이다 — TW-D11).
//         이 모듈은 받은 신호의 기기 ID(Signal::peer_id)를 그대로 대기열에 싣기만 한다 — ID 를 상대 slot 으로 바꾸고 모르는 ID 를 버리는 것은 app 이다(roster.h RS-D4).
//         비용: v1 교환기에 이 펌웨어가 붙으면 헤더는 무시되고 ID 없는 글자가 와서 app 이 peer·ring 을 버린다(상대가 계속 꺼진 것으로 보인다) → 교환기를 먼저 v2 로 올린다.
//         탈출구: relay_proto.h 의 PROTO_VERSION 을 1 로 내리면 v1 연결이 된다(그때는 roster.h route_slot 의 탈출구도 함께 — ID 없는 신호를 slot 0 으로).
#include "relay_link.h"

#include <atomic>
#include <cstring>
#include <type_traits>

#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_tls_errors.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "esp_heap_caps.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

#include "board_profile.h"
#include "net_wifi.h"
#include "relay_diag.h"
#include "voice_tune.h"

namespace relaylink {
namespace {

const char *TAG = "relay";

// Phase 4 설정 지킴이 — sdkconfig 는 git 밖이라(기기마다 다시 만든다) 묵은 sdkconfig 나 menuconfig 의 손길이 아래 약속을 깨면 빌드를 멈춘다.
#if !CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#error "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y 가 꺼지면 wss 의 서버 검증 수단이 없다(P4N-D1). sdkconfig 를 지우고 다시 빌드할 것"
#endif
#if CONFIG_ESP_TLS_INSECURE || CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY
#error "TLS 서버 검증을 끄는 설정은 쓰지 않는다(P4N-D1)"
#endif
#if CONFIG_MBEDTLS_HAVE_TIME_DATE
#error "기기에 시계가 없다 — 유효 기간 검사를 켜면 모든 인증서가 떨어진다. 시각 동기(SNTP)를 넣은 뒤에 이 줄을 지울 것(P4N-D1)"
#endif
#if !CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC || CONFIG_MBEDTLS_HARDWARE_AES || CONFIG_MBEDTLS_HARDWARE_SHA
#error "TLS 메모리는 PSRAM, AES·SHA 는 소프트웨어여야 한다(P4N-D2·D6). sdkconfig.defaults 를 고친 뒤에는 sdkconfig 를 지우고 다시 빌드할 것"
#endif

// relay_diag.h 가 값으로 든 오류 코드가 진짜 esp-tls 값과 같은가(P4N-D5)
static_assert(relaydiag::ERR_DNS == ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME, "relay_diag.h ERR_DNS");
static_assert(relaydiag::ERR_SOCKET == ESP_ERR_ESP_TLS_CANNOT_CREATE_SOCKET, "relay_diag.h ERR_SOCKET");
static_assert(relaydiag::ERR_FAMILY == ESP_ERR_ESP_TLS_UNSUPPORTED_PROTOCOL_FAMILY, "relay_diag.h ERR_FAMILY");
static_assert(relaydiag::ERR_CONNECT == ESP_ERR_ESP_TLS_FAILED_CONNECT_TO_HOST, "relay_diag.h ERR_CONNECT");
static_assert(relaydiag::ERR_SETOPT == ESP_ERR_ESP_TLS_SOCKET_SETOPT_FAILED, "relay_diag.h ERR_SETOPT");
static_assert(relaydiag::ERR_TIMEOUT == ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT, "relay_diag.h ERR_TIMEOUT");
static_assert(relaydiag::ERR_CLOSED_FIN == ESP_ERR_ESP_TLS_TCP_CLOSED_FIN, "relay_diag.h ERR_CLOSED_FIN");
static_assert(relaydiag::ERR_TLS_FIRST == ESP_ERR_MBEDTLS_CERT_PARTLY_OK, "relay_diag.h ERR_TLS_FIRST");
static_assert(relaydiag::ERR_TLS_SETUP == ESP_ERR_MBEDTLS_SSL_SETUP_FAILED, "relay_diag.h ERR_TLS_SETUP");
static_assert(relaydiag::ERR_TLS_HANDSHAKE == ESP_ERR_MBEDTLS_SSL_HANDSHAKE_FAILED, "relay_diag.h ERR_TLS_HANDSHAKE");
static_assert(relaydiag::ERR_TLS_LAST == ESP_ERR_MBEDTLS_SSL_READ_FAILED, "relay_diag.h ERR_TLS_LAST");

enum class Phase : uint8_t { IDLE, CONNECTING, UP };

esp_websocket_client_handle_t s_ws = nullptr;
QueueHandle_t s_in_q = nullptr;
std::atomic<Phase> s_phase{Phase::IDLE};
std::atomic<bool> s_auth_rejected{false};
std::atomic<int> s_fails{0};
std::atomic<uint32_t> s_seq{0};
std::atomic<bool> s_reconnect_req{false};
std::atomic<voice_sink_t> s_voice_sink{nullptr};
int64_t s_up_since_us = 0;
TaskHandle_t s_relay_task = nullptr;
SemaphoreHandle_t s_tx_lock = nullptr;                           // RL-D10
constexpr TickType_t TEXT_LOCK_WAIT = pdMS_TO_TICKS(2000), VOICE_LOCK_WAIT = pdMS_TO_TICKS(20);

relaydiag::ErrStash s_err_stash;                                 // P4N-D7: ERROR 사건이 가져간 전송 오류(핸들러에서만 만진다)

std::atomic<bool> s_nodelay{voicetune::DEFAULT_NODELAY != 0};    // RL-D12
std::atomic<int> s_nodelay_socks{-1};                            // 마지막으로 옵션을 건 소켓 수(-1 = 아직 건 적 없음)

char s_url[URL_BUF], s_headers[200];
constexpr int64_t START_RETRY_US = 5LL * 1000000;                // RL-D8

// 받는 프레임 재조립(작은 프레임도 여러 이벤트로 쪼개져 올 수 있다). 규약: 음성 400바이트 이하, 신호 256바이트 이하
uint8_t s_rx[512];
int s_rx_len = 0;
uint8_t s_rx_op = 0;
bool s_rx_drop = false;

// RL-D12 연결된 TCP 소켓 전부에 TCP_NODELAY 를 건다(또는 푼다). 돌려주는 값 = 건 소켓 수. 부르는 쪽이 s_tx_lock 을 쥐고 있거나, 아직 아무도 보내지 않는 때(CONNECTED 핸들러)다.
int apply_nodelay(bool on)
{
    int n = 0;
    const int v = on ? 1 : 0;
    for (int fd = LWIP_SOCKET_OFFSET; fd < LWIP_SOCKET_OFFSET + CONFIG_LWIP_MAX_SOCKETS; ++fd) {
        int type = 0; socklen_t len = sizeof type;
        if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &len) != 0 || type != SOCK_STREAM) continue;     // 소켓이 아니거나 TCP 가 아니다
        struct sockaddr_storage peer; socklen_t plen = sizeof peer;
        if (getpeername(fd, (struct sockaddr *)&peer, &plen) != 0) continue;                            // 아직 연결되지 않았다
        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &v, sizeof v) == 0) ++n;
    }
    s_nodelay_socks = n;
    return n;
}

static_assert(std::is_trivially_copyable<In>::value, "In is copied byte-for-byte through the FreeRTOS queue (Signal carries the peer id as a plain char array)");

void post(In::Kind k, const relayproto::Signal *sig = nullptr)
{
    In in{}; in.kind = k;
    if (sig) in.sig = *sig;
    if (xQueueSend(s_in_q, &in, 0) != pdTRUE) ESP_LOGW(TAG, "event queue full (kind %d)", (int)k);
    ++s_seq;
}

void schedule_reconnect(esp_websocket_client_handle_t c, bool was_up)
{
    if (was_up && esp_timer_get_time() - s_up_since_us >= 60LL * 1000000) s_fails = 0;        // RL-D3
    const int ms = relayproto::backoff_ms(s_fails.load(), s_auth_rejected.load());
    if (s_fails < 30) ++s_fails;
    esp_websocket_client_set_reconnect_timeout(c, ms);
    ESP_LOGI(TAG, "reconnect in %d ms (fails %d)", ms, s_fails.load());
}

void on_frame_part(const esp_websocket_event_data_t *d)
{
    if (d->payload_offset == 0) {
        s_rx_len = 0; s_rx_op = d->op_code;
        s_rx_drop = d->payload_len > (int)sizeof s_rx || !d->fin || d->op_code == 0x00;      // 너무 큰 것·이어지는 메시지는 버린다
    }
    if (s_rx_drop || d->payload_offset != s_rx_len || s_rx_len + d->data_len > (int)sizeof s_rx) { s_rx_drop = true; return; }
    memcpy(s_rx + s_rx_len, d->data_ptr, d->data_len);
    s_rx_len += d->data_len;
    if (s_rx_len < d->payload_len) return;

    if (s_rx_op == 0x02) {                                       // 음성 [순번 2][Opus]
        voice_sink_t fn = s_voice_sink.load();
        if (fn) fn(s_rx, (size_t)s_rx_len);
    } else if (s_rx_op == 0x01) {                                // 신호(JSON, 끝에 0 이 없다)
        relayproto::Signal sig = relayproto::parse((const char *)s_rx, (size_t)s_rx_len);
        if (sig.kind == relayproto::Kind::EVENT) post(In::Kind::SIGNAL, &sig);
    }
}

// 사건 자료의 esp-tls 칸을 옮겨 담는다. 부르는 쪽이 error_type == TCP_TRANSPORT 인 것을 확인한 뒤에만 부른다(그 밖에는 부품이 칸을 채우지 않는다 — esp_websocket_client.c:227-234)
relaydiag::TransportErr transport_err(const esp_websocket_error_codes_t &e)
{
    relaydiag::TransportErr t;
    t.esp = (int)e.esp_tls_last_esp_err; t.tls = e.esp_tls_stack_err; t.flags = e.esp_tls_cert_verify_flags; t.sock = e.esp_transport_sock_errno;
    return t;
}

// 주의: 웹소켓 태스크(부품의 잠금을 쥔 채) 또는 send 를 부른 태스크에서 실행된다 → 복사·대기열 넣기만 한다. stop/close/destroy 금지.
void on_ws_event(void *, esp_event_base_t, int32_t id, void *event_data)
{
    auto *d = static_cast<esp_websocket_event_data_t *>(event_data);
    switch (id) {
    case WEBSOCKET_EVENT_BEFORE_CONNECT:
        s_phase = Phase::CONNECTING; ++s_seq;
        s_err_stash.clear();                                     // P4N-D7: 접속 시도마다 빈 보관함으로 시작한다
        break;
    case WEBSOCKET_EVENT_CONNECTED:
        s_phase = Phase::UP; s_auth_rejected = false; s_up_since_us = esp_timer_get_time();
        s_rx_len = 0; s_rx_drop = false;
        s_err_stash.clear();
        if (s_nodelay.load()) apply_nodelay(true);               // RL-D12: 새 소켓이다 — 켜 둔 손잡이를 다시 건다(끈 상태면 건드리지 않는다 = 3b 그대로)
        printf("RELAY up=1 code=0 attempt=%d\n", 0);          // 붙었으니 0 (s_fails 는 아직 묵은 실패 횟수일 수 있다 — 모양은 그대로)
        post(In::Kind::LINK_UP);
        break;
    case WEBSOCKET_EVENT_DATA:
        if (d->op_code >= 0x08) break;                           // 닫기(8)·PING(9)·PONG(10)도 DATA 로 온다
        on_frame_part(d);
        break;
    case WEBSOCKET_EVENT_DISCONNECTED: {                         // 접속 실패 또는 비정상 끊김
        const bool was_up = s_phase == Phase::UP;
        if (s_phase == Phase::CONNECTING) {                      // RL-D4 + P4N-D5: 접속 단계에서 끊겼을 때만, 이번 시도의 까닭으로 정한다
            const esp_websocket_error_codes_t &e = d->error_handle;
            const bool tr = e.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT;   // 부품은 이 종류일 때만 esp-tls 칸을 채운다 — 아니면 초기화되지 않은 값이다
            const relaydiag::TransportErr te = s_err_stash.take(tr, tr ? transport_err(e) : relaydiag::TransportErr{});   // P4N-D7: 자기 값이 비었으면 앞선 ERROR 가 가져간 것을 쓴다
            const relaydiag::Fail why = relaydiag::classify(e.esp_ws_handshake_status_code, te.esp, te.flags);
            s_auth_rejected = relaydiag::is_auth(why);
            printf("RELAYERR stage=%s esp=0x%x tls=-0x%x flags=0x%x errno=%d http=%d attempt=%d\n", relaydiag::fail_name(why), (unsigned)te.esp, (unsigned)te.tls, (unsigned)te.flags, te.sock,
                   relaydiag::shown_http(why, e.esp_ws_handshake_status_code), s_fails.load());
        }
        s_phase = Phase::IDLE;
        schedule_reconnect(d->client, was_up);
        printf("RELAY up=0 code=%d attempt=%d\n", s_auth_rejected.load() ? 401 : 0, s_fails.load());
        if (was_up) post(In::Kind::LINK_DOWN); else ++s_seq;
        break;
    }
    case WEBSOCKET_EVENT_CLOSED: {                               // 교환기가 닫기 프레임으로 끊음(RL-D2)
        const bool was_up = s_phase == Phase::UP;
        s_phase = Phase::IDLE;
        schedule_reconnect(d->client, was_up);
        printf("RELAY up=0 code=0 attempt=%d\n", s_fails.load());
        if (was_up) post(In::Kind::LINK_DOWN); else ++s_seq;
        break;
    }
    case WEBSOCKET_EVENT_ERROR:                                  // 상태 전이에는 쓰지 않는다(끊김은 DISCONNECTED·CLOSED 가 알린다). 전송 오류만 보관한다(P4N-D7) —
        // 부품이 이 사건을 내면서 esp-tls 오류를 읽고 지워, 뒤따르는 DISCONNECTED 에는 0 이 온다
        if (d->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) s_err_stash.on_error(true, transport_err(d->error_handle));
        break;
    default:
        break;
    }
}

void relay_task(void *)
{
    while (!netwifi::connected()) vTaskDelay(pdMS_TO_TICKS(200));

    esp_websocket_client_config_t cfg = {};
    cfg.uri = s_url;
    cfg.headers = s_headers;                                     // init 이 복사한다
    const relaydiag::Scheme scheme = relaydiag::scheme_of(s_url);
    if (scheme == relaydiag::Scheme::WSS) cfg.crt_bundle_attach = esp_crt_bundle_attach;   // P4N-D1: 서버 검증(필수). 이 줄이 빠지면 esp-tls 는 붙지 않고 실패한다
    cfg.task_name = "relay_ws";
    cfg.task_stack = relaydiag::WS_TASK_STACK;                   // 기본 4096. TLS 악수와 위 핸들러가 이 스택에서 돈다(P4N-D3)
    cfg.network_timeout_ms = relaydiag::NET_TIMEOUT_MS;          // P4N-D4
    cfg.reconnect_timeout_ms = 1000;                             // 첫 대기. 이후는 schedule_reconnect 가 정한다
    cfg.enable_close_reconnect = true;                           // RL-D2
    cfg.ping_interval_sec = relaydiag::CTRL_PING_S;              // RL-D1 (제어 PING)
    cfg.pingpong_timeout_sec = relaydiag::PONG_TIMEOUT_S;
    s_ws = esp_websocket_client_init(&cfg);
    memset(s_headers, 0, sizeof s_headers);                      // 토큰이 든 원본은 지운다
    if (!s_ws) { ESP_LOGE(TAG, "client init failed"); printf("PHONE FAIL relay init\n"); vTaskDelete(nullptr); return; }
    esp_websocket_register_events(s_ws, WEBSOCKET_EVENT_ANY, on_ws_event, nullptr);
    bool start_pending = esp_websocket_client_start(s_ws) != ESP_OK;   // RL-D8: 실패하면 아래 루프가 5초마다 다시 시도
    int64_t next_start_us = esp_timer_get_time() + START_RETRY_US;
    if (start_pending) ESP_LOGW(TAG, "websocket start failed — retry in 5s");

    uint32_t ping_n = 0;
    int64_t last_ping_us = 0;
    bool wifi_was = true, up_was = false;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(100));

        if (s_reconnect_req.exchange(false)) {                   // RL-D5
            const bool was_up = s_phase.exchange(Phase::IDLE) == Phase::UP;   // RL-D8: stop 이 끝날 때까지 UP 으로 남지 않게 먼저 내린다(새로 보내려는 쪽도 여기서 돌아선다). 읽기와 쓰기를 한 번에 — 그 사이 핸들러가 UP 으로 올려도 LINK_DOWN 이 따라간다(RL-D11)
            xSemaphoreTake(s_tx_lock, portMAX_DELAY);            // RL-D10: 보내는 중인 것이 끝나기를 기다린다(보내기 한 번은 최대 2초)
            esp_websocket_client_stop(s_ws);                     // 태스크가 끝날 때까지 기다린다(끊김 이벤트는 오지 않는다)
            const bool rose = s_phase.exchange(Phase::IDLE) == Phase::UP;   // N-1 + RL-D11: stop 이 도는 동안 핸들러가 올린 값이 남지 않게, 그리고 UP 까지 갔었는지 본다
            if (was_up || rose) post(In::Kind::LINK_DOWN);
            printf("RELAY up=0 code=0 attempt=%d\n", s_fails.load());
            start_pending = esp_websocket_client_start(s_ws) != ESP_OK;
            xSemaphoreGive(s_tx_lock);
            next_start_us = esp_timer_get_time() + START_RETRY_US;
            if (start_pending) ESP_LOGW(TAG, "websocket restart failed — retry in 5s");
            continue;
        }

        if (start_pending && esp_timer_get_time() >= next_start_us) {   // RL-D8
            xSemaphoreTake(s_tx_lock, portMAX_DELAY);            // RL-D10: start 가 전송 객체를 새로 만든다
            start_pending = esp_websocket_client_start(s_ws) != ESP_OK;
            xSemaphoreGive(s_tx_lock);
            next_start_us = esp_timer_get_time() + START_RETRY_US;
            if (start_pending) ESP_LOGW(TAG, "websocket start retry failed"); else ESP_LOGI(TAG, "websocket start retry ok");
        }

        const bool wifi = netwifi::connected();
        if (wifi && !wifi_was && !s_auth_rejected) esp_websocket_client_set_reconnect_timeout(s_ws, 1000);   // Wi-Fi 가 돌아오면 바로 다시 붙는다
        wifi_was = wifi;

        const int64_t now = esp_timer_get_time();
        const bool is_up = s_phase == Phase::UP;
        if (is_up && (!up_was || now - last_ping_us >= relaydiag::JSON_PING_S * 1000000LL)) {                                  // RL-D1 (15초마다) + RL-D7 (붙자마자 한 번)
            char buf[40];
            if (relayproto::ping_text(buf, sizeof buf, ++ping_n)) send_text(buf);
            last_ping_us = now;
        }
        up_was = is_up;
    }
}

// RL-D10: 잠금을 쥐고, 붙어 있을 때만 보낸다. 잠금을 lock_wait 안에 못 얻으면 보내지 않는다.
bool send_frame(bool text, const char *data, int len, TickType_t lock_wait)
{
    if (!s_ws || !s_tx_lock || s_phase != Phase::UP) return false;            // 붙어 있지 않으면 잠금을 기다리지도 않는다
    if (xSemaphoreTake(s_tx_lock, lock_wait) != pdTRUE) return false;
    bool ok = false;
    if (s_phase == Phase::UP) {                                               // 잠금을 쥔 뒤에 다시 본다 — 그 사이 reconnect 가 내렸을 수 있다
        const int sent = text ? esp_websocket_client_send_text(s_ws, data, len, pdMS_TO_TICKS(2000))    // RL-D6
                              : esp_websocket_client_send_bin(s_ws, data, len, pdMS_TO_TICKS(2000));
        ok = sent == len;
    }
    xSemaphoreGive(s_tx_lock);
    return ok;
}

}  // namespace

void start(const char *url, const char *dev_id, const char *token)
{
    if (s_in_q) return;
    static StaticSemaphore_t tx_lock_buf;                         // RL-D10: 정적이라 만들기가 실패하지 않는다
    s_tx_lock = xSemaphoreCreateMutexStatic(&tx_lock_buf);
    s_in_q = xQueueCreate(16, sizeof(In));
    strlcpy(s_url, url, sizeof s_url);
    const relaydiag::Scheme scheme = relaydiag::scheme_of(s_url);
    printf("RELAYCFG scheme=%s tls=%d\n", relaydiag::scheme_name(scheme), (int)(scheme == relaydiag::Scheme::WSS));   // 어느 길로 붙는 빌드인가(주소는 싣지 않는다)
    if (scheme == relaydiag::Scheme::NONE) {                     // ws:// 도 wss:// 도 아니다 — 붙을 수 없다. 화면에는 "교환기에 연결할 수 없음"으로 보인다
        ESP_LOGE(TAG, "relay address must start with ws:// or wss://"); printf("PHONE FAIL relay scheme\n");
        s_fails = 99; ++s_seq;
        return;
    }
    const int n = relayproto::connect_headers(s_headers, sizeof s_headers, dev_id, token);   // RL-D13: X-Device-Id · X-Device-Token · X-Phone-Proto: 2 (줄마다 \r\n — relay_proto.h RP-D5)
    if (n <= 0) {                                                // ID·토큰이 너무 길다 — 붙을 수 없다. 화면에는 "교환기에 연결할 수 없음"으로 보인다
        ESP_LOGE(TAG, "id/token too long"); printf("PHONE FAIL relay headers\n");
        memset(s_headers, 0, sizeof s_headers); s_fails = 99; ++s_seq;
        return;
    }
#if !CONFIG_LOG_DYNAMIC_LEVEL_CONTROL
#error "CONFIG_LOG_DYNAMIC_LEVEL_CONTROL=y 가 꺼지면 esp_log_level_set 이 듣지 않아 토큰이 든 접속 요청이 오류 로그로 찍힐 수 있다(RL-D9)"
#endif
    esp_log_level_set("transport_ws", ESP_LOG_NONE);             // 접속 요청 쓰기가 실패하면 요청 전문(토큰 포함)을 오류 로그로 찍는 부품 동작을 막는다
    // RL-D14 (2026-10-08, 전화기 3대): 스택짱(ESP32-S3)은 내부 메모리가 빠듯해 이 태스크의 스택을 PSRAM 에 둔다(board_profile.h RELAY_STACK_PSRAM — 이 태스크는 플래시를 만지지 않는다).
    //   안 되면 내부 메모리로. Tab5 는 전과 같다(내부 메모리).
    if (!boardprofile::RELAY_STACK_PSRAM || xTaskCreatePinnedToCoreWithCaps(relay_task, "relay", 8 * 1024, nullptr, 5, &s_relay_task, 0, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS)
    xTaskCreatePinnedToCore(relay_task, "relay", 8 * 1024, nullptr, 5, &s_relay_task, 0);   // send_text(wss 면 TLS 쓰기)와 끊김 핸들러가 이 스택에서 돌 수 있다
}

bool up() { return s_phase == Phase::UP; }
int  http_code() { return s_auth_rejected ? 401 : 0; }
int  attempt() { return s_phase == Phase::UP ? 0 : s_fails.load(); }
uint32_t change_seq() { return s_seq; }

bool poll(In *out) { return s_in_q && xQueueReceive(s_in_q, out, 0) == pdTRUE; }

bool send_text(const char *text)
{
    if (!text) return false;
    return send_frame(true, text, (int)strlen(text), TEXT_LOCK_WAIT);
}

bool send_voice(const uint8_t *frame, size_t len)
{
    if (!frame || len < relayproto::MIN_VOICE_FRAME || len > relayproto::MAX_VOICE_FRAME) return false;
    return send_frame(false, (const char *)frame, (int)len, VOICE_LOCK_WAIT);
}

void reconnect() { s_reconnect_req = true; }

void stack_free(unsigned *relay_task_free, unsigned *ws_task_free)
{
    TaskHandle_t ws = xTaskGetHandle("relay_ws");                // 부품이 만든 웹소켓 태스크(멈춘 동안에는 없다)
    if (relay_task_free) *relay_task_free = s_relay_task ? (unsigned)uxTaskGetStackHighWaterMark(s_relay_task) : 0;
    if (ws_task_free) *ws_task_free = ws ? (unsigned)uxTaskGetStackHighWaterMark(ws) : 0;
}

void set_voice_sink(voice_sink_t fn) { s_voice_sink = fn; }

int set_nodelay(bool on)                                         // RL-D12
{
    s_nodelay = on;
    if (!s_tx_lock || s_phase != Phase::UP) return 0;            // 붙어 있지 않다 — 값만 기억한다(다음에 붙을 때 건다)
    if (xSemaphoreTake(s_tx_lock, TEXT_LOCK_WAIT) != pdTRUE) return -1;
    const int n = s_phase == Phase::UP ? apply_nodelay(on) : 0;
    xSemaphoreGive(s_tx_lock);
    return n;
}
bool nodelay() { return s_nodelay.load(); }
int nodelay_socks() { return s_nodelay_socks.load(); }

}  // namespace relaylink
