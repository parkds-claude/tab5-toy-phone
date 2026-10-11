# esp_websocket_client 가 교환기 계약을 만족하는지 — 소스 확인 결과

## 결론
**쓸 수 있습니다.** 헤더 2개, 401 구분, 압축 미제안, 텍스트·이진 송신, wss 인증서 검증 모두 소스로 확인했습니다. 다만 기본 설정 그대로 두면 계약과 어긋나는 곳이 3군데 있어 설정·코드로 메워야 합니다(아래 "위험" A1~A3).

**확인 방식의 한계**: 전부 소스 읽기입니다. 빌드·플래시·기기 실행은 하지 않았습니다. `~/tab5-toy-phone` 는 건드리지 않았습니다(git status 깨끗함).

읽은 판:
- 부품 1.8.0: 꼬리표 `websocket-v1.8.0`, 커밋 70bf122f. 위치 `<scratchpad>/ref/esp-protocols/components/esp_websocket_client/` (아래 "부품:줄" = 이 폴더의 `esp_websocket_client.c`)
- 실제 접속 요청·프레임 처리: 로컬 IDF v5.5.4 의 `~/esp/esp-idf-5.5.4/components/tcp_transport/transport_ws.c` (아래 "ws:줄")

읽기 외에 한 일: 사용자 공개 주소(snapbooth·carbon·example.com)에 GET 4회와 인증서 조회 2회(헤더 크기, `Host: 이름:443`, 인증서 사슬 확인용).

---

## 항목별 답

### 1. 판 번호
- 최신 공개 판은 **1.8.0** (2026-07-22). 의존성은 `idf >=5.0` 뿐이고 칩 제한이 없어 5.5.4·esp32p4·esp_hosted 2.11.7 고정과 충돌하지 않습니다. 근거: 등록소 API, https://components.espressif.com/components/espressif/esp_websocket_client/versions/1.8.0
- `idf_component.yml` 에 적을 줄(프로젝트의 정확 고정 방식대로): `espressif/esp_websocket_client: '1.8.0'`
- 1.8.0 이어야 하는 이유: 닫기 코드 노출(`close_status_code`)과 재연결 대기 중 간격 변경이 이 판에서 들어왔습니다(CHANGELOG.md 1.8.0).
- IDF 5.5.4 에서는 `WEBSOCKET_EVENT_HEADER_RECEIVED` 이벤트가 없습니다(IDF 6.0 이상 전용, 헤더 파일 25~30줄).

### 2. 헤더 두 개 붙이기
- 방법 1: `cfg.headers = "X-Device-Id: tab5-1\r\nX-Device-Token: …\r\n"`. **줄마다 `\r\n` 으로 끝나야** 합니다. 가공 없이 그대로 요청에 붙습니다(ws:273-282). `init` 이 복사하므로 원본 버퍼는 지워도 됩니다(부품:433-437).
- 방법 2: `esp_websocket_client_append_header(client, key, value)` 를 `init` 뒤, `start` 앞에 호출(부품:1031-1074).
- 길이 제한: 부품에는 없습니다. 요청 전체가 `CONFIG_WS_BUFFER_SIZE`(기본 1024) 안에 들어가야 하고(ws:238-289), 고정부 약 200바이트 + 우리 헤더 약 80~100바이트라 여유 있습니다.
- 주소창 매개변수는 쓰지 않습니다. 경로는 `/ws` 만 나갑니다.

### 3. 401 구분
- 가능합니다. 경로: 서버가 401 → `Sec-WebSocket-Accept` 없음 → 접속 실패(ws:314-331) → 부품이 상태 코드를 읽어 둠(부품:1210) → `WEBSOCKET_EVENT_ERROR`, 이어서 `WEBSOCKET_EVENT_DISCONNECTED` 가 오고 둘 다 `error_handle.esp_ws_handshake_status_code == 401`.
- 함정 세 가지:
  - `error_type` 은 `WEBSOCKET_ERROR_TYPE_HANDSHAKE` 가 **아니라** `TCP_TRANSPORT` 입니다. HANDSHAKE 값은 정의만 있고 대입하는 곳이 없습니다.
  - 상태 코드는 **지워지지 않습니다**. 전송 계층은 응답을 받았을 때만 갱신하고(ws:314), 부품은 접속 실패 때만 복사합니다(부품:1210). 그래서 401 뒤에 망 장애로 실패해도 401 로 보이고, 401 → 성공 → 나중의 통화 중 끊김 이벤트에도 401 이 남습니다.
  - ERROR 이벤트의 `esp_tls_*` 필드는 `error_type` 이 TCP_TRANSPORT 일 때만 채워집니다(부품:227-234). 첫 시도의 ERROR 에서는 쓰레기 값입니다.
- 판별 규칙: **"접속 단계(BEFORE_CONNECT~CONNECTED 사이)의 DISCONNECTED 에서 401 이면 등록 오류, CONNECTED 가 올 때까지 유지"**. 코드는 아래 조각 참조.
- 남는 오차: 401 을 한 번 받은 뒤 망이 끊기면 계속 "기기 등록 오류"로 보입니다. 화면 우선순위를 Wi-Fi 끊김 > 401 로 두면 대부분 가려집니다.

### 4. 압축 제안
- **제안하지 않습니다.** 요청 헤더는 Connection, Host, User-Agent, Upgrade, Sec-WebSocket-Version, Sec-WebSocket-Key 에 선택 항목(Protocol, Authorization, 사용자 헤더)뿐입니다(ws:238-282). 부품과 전송 계층에서 `extensions`/`deflate` 문자열을 검색해 0건이었습니다.

### 5. 송신 API·스레드 안전성·조각
- API: `esp_websocket_client_send_text` / `send_bin(client, data, len, TickType_t timeout)`. 반환은 보낸 바이트 수 또는 -1.
- **스레드 안전합니다.** 송신 전체가 재귀 뮤텍스 `client->lock` 안에서 돌고(부품:720), 수신도 같은 잠금 안이라(부품:1387-1392) 음성 태스크와 신호 태스크가 동시에 불러도 프레임이 섞이지 않습니다. wss 에서 mbedTLS 읽기·쓰기도 겹치지 않습니다.
- `timeout` 의 뜻 두 가지: ① 잠금 대기 상한 — 넘으면 -1 만 반환. ② 소켓 쓰기 대기 상한 — **넘으면 부품이 연결을 끊고 재연결합니다**(부품:740-768). 위험 A3 참조.
- 큰 프레임: `buffer_size`(기본 1024)를 넘으면 WebSocket 연속 프레임으로 쪼개 보냅니다(부품:731-737). 우리는 400바이트 이하라 해당 없습니다.
- 수신(`WEBSOCKET_EVENT_DATA`):
  - `op_code` 1=텍스트, 2=이진. **제어 프레임 8(CLOSE)·9(PING)·10(PONG)도 DATA 이벤트로 옵니다**(부품:580, 1108) → 8 이상은 무시해야 합니다.
  - **작은 프레임도 여러 이벤트로 쪼개져 올 수 있습니다.** 읽기 한 번이 소켓에 도착한 만큼만 돌려주기 때문입니다(ws:464-490, 부품:1084-1111). `payload_offset`/`payload_len` 으로 재조립해야 합니다.
  - `data_ptr` 은 핸들러 안에서만 유효하고 NUL 종료가 아닙니다.
- 핸들러 실행 위치: 보통 ws 태스크에서 잠금을 쥔 채 실행됩니다. 송신 실패 때는 **send 를 부른 태스크**에서 실행됩니다(부품:753-766). 따라서 핸들러는 복사해서 큐에 넣기만 해야 합니다. 핸들러 안에서 stop/close/destroy 는 금지입니다.

### 6. 재연결·끊김 감지, 그리고 ping 의 정체
- **서버가 기대하는 ping 은 JSON 텍스트 `{"t":"ping","n":N}` 입니다.** "소식"으로 치는 것은 텍스트·이진 프레임뿐입니다(`phone_core.py:133,204`). WebSocket 제어 PING 은 aiohttp 가 자동 응답하고 핸들러에 넘기지 않습니다(설치된 aiohttp 3.14.0 `web_ws.py:113` autoping 기본 True, `:674-678`). `phone_relay.py:113` 은 autoping 을 바꾸지 않습니다.
- 그래서 ping 은 두 겹입니다:
  - JSON ping 15초 — 펌웨어가 직접 보냄. 서버의 45초 규칙용.
  - 부품의 제어 PING — 기기 쪽 끊김 감지용. PONG 이 `pingpong_timeout_sec` 동안 없으면 끊고 재연결(부품:1280-1323).
- 권장값:

| 설정 | 값 | 이유 |
|---|---|---|
| `ping_interval_sec` | 5 | 죽은 연결을 20초 안에 감지(서버 45초보다 먼저) |
| `pingpong_timeout_sec` | 15 | 위와 한 쌍. 기본 120 은 너무 김 |
| `network_timeout_ms` | 10000 | 기본값. 명시하지 않으면 경고 로그 |
| `reconnect_timeout_ms` | 1000 | 첫 대기. 이후는 핸들러가 조정 |
| `disable_auto_reconnect` | false | 기본값 유지 |
| `enable_close_reconnect` | **true** | 위험 A2 |
| `keep_alive_enable` | false | 제어 PING 이 같은 역할 |

- 1→2→4…30초 간격과 401 의 60초는 DISCONNECTED/CLOSED 핸들러에서 `esp_websocket_client_set_reconnect_timeout()` 으로 구현합니다(부품:1668-1691). 유지보수자가 직접 권한 방식입니다: https://github.com/espressif/esp-protocols/issues/1104
- Wi-Fi 가 돌아오면 같은 함수로 1000 을 넣어 대기를 줄일 수 있습니다(1.8.0 부터 대기 중에도 반영).

### 7. wss 인증서 검증
- `cfg.crt_bundle_attach = esp_crt_bundle_attach;` + `#include "esp_crt_bundle.h"`(부품:660-666). `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE` 이 꺼져 있으면 CA 없이 진행하다 esp-tls 가 접속을 거부합니다(검증 없이 붙지는 않음).
- 시험 펌웨어 `sdkconfig` 에 이미 있는 것: `MBEDTLS_CERTIFICATE_BUNDLE=y`(전체 묶음), `SSL_PROTO_TLS1_2=y`, `ECP_DP_SECP384R1_ENABLED=y`, `WS_TRANSPORT=y`. 날짜 검사는 꺼져 있어(`MBEDTLS_HAVE_TIME_DATE` 미설정) 시각 동기화가 없어도 됩니다.
- 인증서 사슬 대조: example.com ← Let's Encrypt YE1 ← Root YE ← ISRG Root X2 ← ISRG Root X1. X1·X2 가 IDF 5.5.4 묶음에 있습니다(`cacrt_all.pem:1736, 2784`). Phase 0 의 `net_probe.cpp:162` 가 같은 방식으로 같은 인증서에 이미 붙었습니다.
- 새로 넣기를 권하는 것: **`CONFIG_WS_BUFFER_SIZE=2048`** (위험 B5).
- 메모리 단서(추정, 미실측): `MBEDTLS_INTERNAL_MEM_ALLOC=y`, 수신 16384 + 송신 4096 고정 버퍼라 연결이 살아 있는 동안 내부 힙 약 21KB 상주. 여기에 부품 버퍼 2KB, 전송 버퍼 1~2KB, 태스크 스택 8KB. 합쳐 약 35KB → 평소 약 125KB, 악수 순간은 Phase 0 의 116KB 근처나 그 아래로 예상합니다.

### 8. 태스크·버퍼 기본값, esp_hosted
- 기본값(부품:30-41): 스택 4096, 우선순위 5, 코어 고정 없음, 버퍼 1024(수신·송신 각각), 이벤트 큐 1.
- 권장: `task_stack = 8192`. TLS 악수와 이벤트 핸들러가 이 스택에서 돕니다. 실제 사용량은 Phase 2 에서 `uxTaskGetStackHighWaterMark` 로 잽니다. 버퍼는 1024 유지, 우선순위는 Phase 3 에서 소리 태스크와 함께 정합니다.
- 켜지 말 것:
  - `CONFIG_ESP_WS_CLIENT_SEPARATE_TX_LOCK` — 송신·수신 잠금이 갈려 wss 에서 mbedTLS 읽기·쓰기가 겹칩니다(부품:714-724).
  - `CONFIG_MBEDTLS_DYNAMIC_BUFFER` — 위와 겹치면 메모리 깨짐이 보고됐습니다(https://github.com/espressif/esp-protocols/issues/1111, 작성자가 최신 판에서 해결됐다며 닫음).
- esp_hosted: 이 부품 고유의 문제는 **찾지 못했습니다**(esp-hosted-mcu 이슈 검색). 일반 문제로 "Unrecoverable host sdio state"가 미해결입니다(https://github.com/espressif/esp-hosted-mcu/issues/167, 2.11.6·2.12.13 에서 보고). 나타나면 Wi-Fi 가 통째로 죽으므로 재연결 경로가 방어선입니다.
- 참고(부품 밖): 시험 펌웨어에 `esp_wifi_set_ps` 호출이 없어 Wi-Fi 절전이 기본값입니다. 통화 지연에 영향이 있는지 Phase 3 에서 확인할 만합니다.

### 9. 같은 ID 재접속과 무한 핑퐁
- 밀려난 쪽이 받는 것: 서버의 `ws.close()` → 닫기 프레임(코드 1000). 기기에는 DATA(op 8) → 부품이 닫기 응답 → **`WEBSOCKET_EVENT_CLOSED`**(`close_status_code=1000`). DISCONNECTED 는 오지 않습니다(부품:1150-1160, 1334-1365, 1417-1441).
- **기기 한 대로는 핑퐁이 생기지 않습니다.** 클라이언트 하나는 연결을 하나만 갖고, 재접속 전에 옛 소켓을 버립니다. 서버가 옛 연결을 닫아도 새 연결에는 영향이 없습니다(`phone_core.py:116`, `phone_relay.py:119`).
- **같은 ID 를 쓰는 자동 재연결 클라이언트가 둘이면 무한 핑퐁이 납니다**(`enable_close_reconnect=true` 일 때). 1초 간격으로 서로 밀어내고 그때마다 통화가 `lost` 로 끝납니다. `phone_smoke.py` 는 재접속하지 않아 무한은 아니지만, 기기가 켜져 있으면 시험이 깨집니다.
- 대처: 재연결 간격을 **연결이 60초 이상 유지됐을 때만 1초로 초기화**하면 핑퐁이 30초에 한 번으로 줄어듭니다(아래 코드에 반영).
- 선택 사항(서버 계약 변경): 지금은 밀려남과 45초 무소식이 둘 다 코드 1000 이라 기기가 구분할 수 없습니다. 밀어낼 때 고유 닫기 코드(예 4001)를 주면 "다른 기기가 같은 ID 로 접속" 안내가 가능합니다.

### 10. xiaozhi-esp32
- 자체 라이브러리 `78/esp-ml307`(~3.7.3)의 WebSocket 을 씁니다(`main/protocols/websocket_protocol.cc:91`, `main/idf_component.yml:31`).
- 이유 단서(코드 구조에서 추론): Wi-Fi 소켓과 LTE 모뎀을 한 인터페이스로 묶기 위해서로 보입니다(`include/network_interface.h:11-21`, README 첫 줄).
- 부수 단서: 헤더와 본문을 한 버퍼에 담아 **한 번에** 씁니다(`src/web_socket.cc:216-255`). IDF 전송 계층은 두 번에 나눠 씁니다(위험 B4).

---

## 계약과 안 맞거나 위험한 점

**A. Phase 2 에서 반드시 처리 (기본 설정이면 계약 위반)**

| # | 문제 | 대처 |
|---|---|---|
| A1 | 부품의 자동 ping 은 서버의 45초 규칙에 안 잡힘 → 그것만 믿으면 대기 중 45초마다 끊김 | 펌웨어가 JSON ping 을 15초마다 직접 보냄 |
| A2 | 서버가 닫기 프레임으로 끊으면(밀려남·45초 무소식) 기본 설정의 부품은 **멈추고 다시 붙지 않음**(부품:1436-1441) | `enable_close_reconnect = true` + CLOSED 처리 |
| A3 | send 의 timeout 안에 소켓이 안 뚫리면 부품이 **연결을 끊음** → 음성에 0~수십 ms 를 주면 Wi-Fi 가 잠깐 막힐 때마다 통화가 `lost` | 음성은 전용 송신 태스크 + 큐. 200ms 넘은 프레임은 큐에서 버리고 send 타임아웃은 넉넉히(예 2초). 끊긴 동안은 send 를 부르지 않음(부를 때마다 오류 로그) |

**B. 알고 가야 할 위험 (Phase 3~4 에서 측정 후 결정)**

- **B4. 상향 음성이 묶여 나갈 가능성** (소스에서 추론, 기기 미측정)
  - 사실: 프레임마다 헤더와 본문을 따로 씁니다(ws:420, 428). TCP_NODELAY 를 켜는 코드가 esp-tls·tcp_transport 에 없습니다(검색 0건). lwIP 는 미확인 데이터가 있으면 작은 조각을 붙잡습니다(`tcp_priv.h:100-105`).
  - 추정: 왕복 90ms 망에서 20ms 음성이 약 100ms 단위로 묶여 나가, 상향 지연·지터가 최대 그만큼 늘 수 있습니다. LAN 에서는 잘 안 보여 Phase 4 에서 처음 드러날 수 있습니다.
  - 같은 현상의 현장 보고와 유지보수자 답(외부 전송을 직접 만들어 소켓 옵션 설정): https://github.com/espressif/esp-protocols/issues/636
  - 권고: Phase 2 는 단순 설정으로 가고, Phase 3 에서 교환기 도착 간격을 재서 결정합니다. 필요하면 `cfg.ext_transport` + CONNECTED 에서 `setsockopt(TCP_NODELAY)`. 바뀌는 곳은 `relay_link` 초기화뿐입니다.
  - NODELAY 의 주의점(추론): 프레임당 pbuf 2개가 왕복 시간 동안 남아 lwIP 쓰기 가능 한도(8개, `opt.h:1400-1418`)를 넘습니다. `CONFIG_LWIP_TCP_SND_BUF_DEFAULT` 를 함께 키워야 합니다.
- **B5. 접속 버퍼 1024 의 여유가 작음**
  - 실측: Cloudflare 경유 일반 응답 헤더가 573~595바이트(Report-To 250여 바이트 포함). 101 응답은 600~700바이트로 추정합니다.
  - 넘치면 "Header size exceeded buffer size"로 접속이 실패하고 401 도 읽지 못합니다(ws:305-310).
  - 대처: `CONFIG_WS_BUFFER_SIZE=2048`.
- **B6. 재연결 반복 시 내부 메모리 감소 보고** (미해결, https://github.com/espressif/esp-protocols/issues/1104)
  - 유지보수자 진단은 닫힌 TCP 가 120초간 남는 것, 처방은 간격 늘리기입니다. 보고자는 동의하지 않았습니다.
  - 대처: 1→30초 간격(설계와 동일) + Phase 2 에서 교환기를 수십 번 껐다 켜며 내부 힙을 측정.
- **B7. 토큰이 시리얼 로그에 찍히는 경로**
  - 접속 요청 쓰기가 실패하면 요청 전문(토큰 포함)이 ERROR 수준으로 찍힙니다(ws:292). DEBUG 수준에서는 항상 찍힙니다(ws:274, 290).
  - 대처: 시리얼 기록을 파일로 남기거나 공유할 때 주의. 필요하면 `esp_log_level_set("transport_ws", ESP_LOG_NONE)`.
- **B8. 401 값이 남는 문제** — 항목 3 의 판별 규칙으로 처리.
  - 탈출구(정확한 판별이 필요할 때): `disable_auto_reconnect=true` 로 두고 FINISH → `esp_websocket_client_stop()`(종료 대기) → 대기 → `start()` 를 직접 돌립니다. 시도마다 전송 계층이 새로 만들어져 값이 깨끗합니다. 비용은 시도마다 태스크 생성.

**확인 못 함**
- 기기에서의 실제 동작 전부(스택 사용량, 악수 시간, 메모리 최저점).
- Cloudflare 가 제어 PING/PONG 을 원본까지 넘기는지, 101 응답의 실제 헤더 크기, `X-Device-*` 헤더 통과. 부품이 항상 보내는 `Host: 이름:443` 형식은 carbon.example.com 에서 200 을 확인했습니다.
- 교환기를 정상 종료할 때 기기가 받는 닫기 코드.
- B4 의 실제 지연 크기.

---

## Phase 2 계획에 옮길 코드 조각 (소스 대조만 함, 컴파일·기기 미검증)

`main/idf_component.yml`
```yaml
  espressif/esp_websocket_client: '1.8.0'   # close_status_code·재연결 간격 변경이 이 판부터
```
`sdkconfig.defaults`
```
CONFIG_WS_BUFFER_SIZE=2048
# 켜지 않는다(기본값 유지): CONFIG_ESP_WS_CLIENT_SEPARATE_TX_LOCK, CONFIG_MBEDTLS_DYNAMIC_BUFFER
```
`relay_link.cpp` 뼈대
```cpp
#include <cstring>
#include "esp_websocket_client.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"

enum LinkPhase : uint8_t { LINK_IDLE, LINK_CONNECTING, LINK_UP };
static esp_websocket_client_handle_t s_ws;
static volatile LinkPhase s_phase = LINK_IDLE;
static volatile bool s_auth_rejected;   // 마지막 교환기 응답이 401. CONNECTED 때만 해제
static int     s_fails;
static int64_t s_up_since_us;
static uint8_t s_rx[512];               // 계약: 음성 ≤400, 신호 ≤256
static int     s_rx_len;  static uint8_t s_rx_op;  static bool s_rx_drop;

// 1→2→4→8→16→30초. 401 은 60초(§9.7). 60초 이상 유지된 연결만 간격 초기화(같은 ID 핑퐁 방지)
static void schedule_reconnect(esp_websocket_client_handle_t c, bool was_up) {
    if (was_up && esp_timer_get_time() - s_up_since_us >= 60LL * 1000000) s_fails = 0;
    int ms = s_auth_rejected ? 60000 : (s_fails >= 5 ? 30000 : (1000 << s_fails));
    if (s_fails < 30) s_fails++;
    esp_websocket_client_set_reconnect_timeout(c, ms);
}

// 프레임이 여러 이벤트로 쪼개져 올 수 있다 → payload_offset/len 으로 재조립
static void on_chunk(const esp_websocket_event_data_t *d) {
    if (d->payload_offset == 0) {
        s_rx_len = 0;  s_rx_op = d->op_code;
        s_rx_drop = d->payload_len > (int)sizeof(s_rx) || !d->fin || d->op_code == 0x00;  // 너무 큼·분할 메시지는 버림
    }
    if (s_rx_drop || d->payload_offset != s_rx_len || s_rx_len + d->data_len > (int)sizeof(s_rx)) { s_rx_drop = true; return; }
    memcpy(s_rx + s_rx_len, d->data_ptr, d->data_len);
    s_rx_len += d->data_len;
    if (s_rx_len < d->payload_len) return;
    if (s_rx_op == 0x02)      voice_rx_push(s_rx, s_rx_len);                  // [순번 2][Opus]
    else if (s_rx_op == 0x01) signal_rx_push((const char *)s_rx, s_rx_len);   // JSON, NUL 종료 아님
}

// 주의: ws 태스크(잠금 쥔 채) 또는 send 를 부른 태스크에서 실행된다 → 복사·큐 넣기만. stop/close/destroy 금지
static void on_ws_event(void *, esp_event_base_t, int32_t id, void *event_data) {
    auto *d = static_cast<esp_websocket_event_data_t *>(event_data);
    switch (id) {
    case WEBSOCKET_EVENT_BEFORE_CONNECT:
        s_phase = LINK_CONNECTING;
        break;
    case WEBSOCKET_EVENT_CONNECTED:
        s_phase = LINK_UP;  s_auth_rejected = false;  s_up_since_us = esp_timer_get_time();
        s_rx_len = 0;  s_rx_drop = false;
        relay_post(EV_LINK_UP);
        break;
    case WEBSOCKET_EVENT_DATA:
        if (d->op_code >= 0x08) break;        // CLOSE(8)·PING(9)·PONG(10)도 DATA 로 온다
        on_chunk(d);
        break;
    case WEBSOCKET_EVENT_DISCONNECTED: {      // 접속 실패·비정상 끊김 (시도마다 1번)
        bool was_up = (s_phase == LINK_UP);
        // 401 판별: 접속 단계에서만 본다(값이 지워지지 않아 통화 중 끊김에도 남아 있음). error_type 은 HANDSHAKE 가 아니다
        if (s_phase == LINK_CONNECTING && d->error_handle.esp_ws_handshake_status_code == 401) s_auth_rejected = true;
        s_phase = LINK_IDLE;
        schedule_reconnect(d->client, was_up);
        relay_post(s_auth_rejected ? EV_LINK_AUTH_REJECTED : EV_LINK_DOWN);   // "기기 등록 오류" / "교환기에 연결할 수 없음"
        break;
    }
    case WEBSOCKET_EVENT_CLOSED: {            // 교환기가 닫기 프레임으로 끊음(밀려남·45초 무소식). d->close_status_code = 현재 항상 1000
        bool was_up = (s_phase == LINK_UP);
        s_phase = LINK_IDLE;
        schedule_reconnect(d->client, was_up);
        relay_post(EV_LINK_DOWN);
        break;
    }
    default: break;                           // ERROR 는 기록만 — 순서·횟수가 일정치 않아 상태 전이에 쓰지 않는다
    }
}

esp_err_t relay_link_start(const char *uri, const char *dev_id, const char *token) {
    char hdr[160];
    int n = snprintf(hdr, sizeof(hdr), "X-Device-Id: %s\r\nX-Device-Token: %s\r\n", dev_id, token);  // 줄마다 \r\n 필수
    if (n <= 0 || n >= (int)sizeof(hdr)) return ESP_ERR_INVALID_SIZE;

    esp_websocket_client_config_t cfg = {};
    cfg.uri = uri;                            // 운영 "wss://<호스트>/ws", 개발 "ws://<맥북 LAN IP>:5094/ws"
    cfg.headers = hdr;                        // init 이 복사한다
    if (strncmp(uri, "wss://", 6) == 0) cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.task_name = "relay_ws";
    cfg.task_stack = 8192;                    // 기본 4096. TLS 악수·핸들러가 이 스택에서 돈다
    cfg.network_timeout_ms = 10000;
    cfg.reconnect_timeout_ms = 1000;          // 첫 대기. 이후 schedule_reconnect 가 조정
    cfg.enable_close_reconnect = true;        // 기본(false)이면 교환기가 닫을 때 부품이 멈춘다
    cfg.ping_interval_sec = 5;                // 제어 PING: 기기 쪽 끊김 감지용(교환기 45초 규칙과 무관)
    cfg.pingpong_timeout_sec = 15;

    s_ws = esp_websocket_client_init(&cfg);
    memset(hdr, 0, sizeof(hdr));
    if (!s_ws) return ESP_FAIL;
    esp_websocket_register_events(s_ws, WEBSOCKET_EVENT_ANY, on_ws_event, nullptr);
    return esp_websocket_client_start(s_ws);
}

// 어느 태스크에서든 호출 가능(부품 내부 잠금). timeout 안에 소켓이 안 뚫리면 부품이 연결을 끊고 재연결한다
bool relay_send_signal(const char *json, int len) {
    return s_phase == LINK_UP && esp_websocket_client_send_text(s_ws, json, len, pdMS_TO_TICKS(2000)) == len;
}
bool relay_send_voice(const uint8_t *f, int len) {   // 전용 송신 태스크에서만. 200ms 넘은 프레임은 부르기 전에 버린다
    return s_phase == LINK_UP && esp_websocket_client_send_bin(s_ws, (const char *)f, len, pdMS_TO_TICKS(2000)) == len;
}
// 15초마다 {"t":"ping","n":N} 을 relay_send_signal 로 보낸다 — 교환기의 45초 규칙은 이것(과 음성)만 센다
// Wi-Fi 가 다시 붙으면(IP_EVENT_STA_GOT_IP): 401 상태가 아닐 때 esp_websocket_client_set_reconnect_timeout(s_ws, 1000)
```

Phase 2 시험에 넣을 것: ① 틀린 토큰 → "기기 등록 오류" + 60초 간격 ② 교환기 종료·재시작 → 간격 1→30초 ③ JSON ping 을 멈추면 45초 뒤 CLOSED → 자동 재접속 ④ 기기가 켜진 채 `phone_smoke.py` → 밀려난 뒤 복귀 ⑤ 재연결 30회 전후 내부 힙 ⑥ ws 태스크 스택 최저 여유.
