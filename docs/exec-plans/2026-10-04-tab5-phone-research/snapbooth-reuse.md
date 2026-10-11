# tab5_phone 재사용 조사 보고 (읽기만 함 — 빌드·플래시·시리얼·시험 실행 없음)

## 결론 먼저

- **파일째 그대로 복사 가능한 것은 6개뿐**: `wifi_store.h`, `test_wifi_store.cpp`, `patch_esp_hosted.py`, `partitions.csv`, 시험 펌웨어의 `sdkconfig.defaults`, 시험 펌웨어의 `idf_component.yml`(여기에 websocket 추가).
- **Wi-Fi 매니저·설정 화면·그리기 함수·시리얼 훅은 전부 스냅부스 `main.cpp` 안의 static 함수 + 파일 전역 변수**라서, 줄 범위로 잘라 새 파일로 옮겨야 한다. 카메라용 잠금(`s_proc_mutex`·`s_paused`·`s_frozen_valid`)과 `fetch_styles()` 가 끼어 있다.
- **부팅 가드(패널 감지 실패 재시작)는 스냅부스에 없다.** ESP-IDF 판은 시험 펌웨어 `probe_main.cpp:323-331` 하나뿐이다.
- **터치 디바운스는 어디에도 없다.** 스냅부스는 "뗄 때"(`wasClicked`), 시험 펌웨어는 "닿을 때"(`wasPressed`)를 쓴다.
- **맥 단위 시험 관례는 있다**: 헤더 전용 순수 로직 + `tools/test_*.cpp` 를 `clang++` 한 줄로 빌드(프레임워크·CMake 없음).
- **펌웨어에서 MAC 을 읽어 기기 번호를 정하는 코드는 아직 없다**(설계서에만 있음).

기준 경로(아래 `파일:줄` 은 이 파일들을 가리킨다):
- `~/tab5-toy-phone/firmware/tab5_snapbooth/main/main.cpp` (1,901줄) → 이하 `main.cpp`
- `~/tab5-toy-phone/firmware/tab5_snapbooth/main/wifi_store.h`
- `~/tab5-toy-phone/firmware/tab5_snapbooth/tools/{snap_ctl.py, test_wifi_store.cpp, patch_esp_hosted.py, gen_ui_assets.py}`
- `~/tab5-toy-phone/firmware/tab5_phone_probe/main/{probe_main.cpp, net_probe.cpp, board_audio.cpp}`
- `~/tab5-toy-phone/firmware/tab5_phone_probe/tools/probe_ctl.py`
- `~/tab5-toy-phone/docs/design/README.md`, `~/tab5-toy-phone/docs/exec-plans/2026-10-03-tab5-phone.md`, `~/tab5-toy-phone/docs/DEVICE-SPEC.md`, `~/tab5-toy-phone/docs/daily/2026-09-26/claude.md`

---

## 1. Wi-Fi

### 1-1. esp_hosted 초기화 순서 — `main.cpp:1286-1316` (`wifi_task`)
1. `nvs_flash_init()` — NO_FREE_PAGES/NEW_VERSION 이면 지우고 재시도 (1288-1290)
2. `esp_netif_init()` → `esp_event_loop_create_default()` (1291-1292)
3. `INIT_DEFAULT_HOST_SDIO_CONFIG()` 에 핀 CLK12/CMD13/D0 11/D1 10/D2 9/D3 8/RST15 → `esp_hosted_sdio_set_config()` (1296-1303)
4. `esp_hosted_init()` → `esp_hosted_connect_to_slave()` (1304-1305), C6 펌웨어 판 로그 (1306)
5. `esp_netif_create_default_wifi_sta()` → `esp_wifi_init()` → 이벤트 핸들러 2개 등록 → `esp_wifi_set_mode(STA)` → `esp_wifi_start()` (1308-1314)
6. `wifi_manager_loop()` (1315, 돌아오지 않음)

- 전제: C6 전원은 `M5.begin()` 이 켠다(1294). 생성자 자동초기화는 패치로 꺼 둔다(6절).
- 헤더: `main.cpp:56-63`. `esp_hosted_transport_config.h` 는 `extern "C"` 로 감싸야 한다(61-63).
- 태스크: 스택 10KB, 우선순위 5, 코어 0 (`main.cpp:1822`).
- 같은 순서를 별도 파일로 뺀 예: `net_probe.cpp:109-127`. 단, 매니저·수동 연결·NVS 쓰기가 없는 축소판이고 설계서 §3(29줄)이 "시험 펌웨어는 복사하지 않는다"고 정했다. 파일 나누는 모양 참고용.

### 1-2. `wifi_store.h` — 네임스페이스 `wifistore`, ESP-IDF 비의존, 그대로 복사 가능
스냅부스와 시험 펌웨어의 두 사본은 `diff` 결과 동일하다.

| 줄 | 시그니처 | 하는 일 |
|---|---|---|
| 11 | `static const size_t MAX_SAVED = 8;` | 저장 최대 개수 |
| 13 | `struct Cred { std::string ssid, pass; bool fixed = false; };` | |
| 14 | `struct Ap { std::string ssid; int rssi = -127; bool secured = true; };` | |
| 17 | `inline std::string pack(const std::vector<Cred> &v)` | `"ssid\0pass\0…"` 로 묶기 |
| 23 | `inline std::vector<Cred> unpack(const std::string &blob)` | 풀기(빈 SSID 제거, 8개 상한, 잘린 꼬리 무시) |
| 37 | `inline void upsert(std::vector<Cred> &saved, const std::string &ssid, const std::string &pass)` | 맨 앞에 넣기, 같은 SSID 는 갱신, 8개 상한 |
| 46 | `inline std::vector<Cred> merge(const std::vector<Cred> &saved, const std::vector<Cred> &fixed)` | 저장 + 고정(중복이면 저장 쪽 비번 우선, 고정은 `fixed=true`) |
| 57 | `inline std::vector<Ap> dedupe(const std::vector<Ap> &raw)` | 같은 SSID 는 가장 센 것 하나, RSSI 내림차순 |
| 70 | `inline std::vector<int> rank(const std::vector<Cred> &known, const std::vector<Ap> &aps)` | 보이는 후보의 known 인덱스를 신호 센 순으로 |
| 83 | `inline int find(const std::vector<Cred> &known, const std::string &ssid)` | 없으면 -1 |
| 90 | `inline int bars(int rssi)` | 1~4칸 (-55/-65/-75 경계) |

- **NVS**: 네임스페이스 `"snapwifi"`, 키 `"list"`(blob), 길이 4096 미만만 읽음 — 읽기 `main.cpp:1132-1142`, 쓰기 1143-1149(비면 키 삭제). 비번은 평문(`main.cpp:21-22`).
- 고정 목록: `secrets.h` 의 `WIFI_NETWORKS` → `FixedCred{ssid, pass}` 배열 (`main.cpp:183-185`), 합치기 `rebuild_known()` 1150-1159.
- 같은 네임스페이스·같은 파티션 표를 쓰면 2번 기기는 스냅부스에서 저장한 목록을 그대로 물려받는다(시험 펌웨어가 읽기만 한 선례: `net_probe.cpp:61`).

### 1-3. 신호 센 순 자동 연결
- `auto_connect(bool rescan)` — `main.cpp:1206-1224`. 스캔 → `wifistore::rank` → 끊겨 있으면 1순위에 `connect_to()`.
- `connect_to()` — 1186-1203. 이전 연결의 늦은 끊김 이벤트를 기다려 소진한 뒤 비트를 지운다(1188-1194). 비번은 로그에 안 남긴다(1202).
- 같은 후보 3회 끊김 → `WReq::NEXT` (1053-1055) → 다음 후보, 다 떨어지면 3초 뒤 재스캔 (1269-1273).
- 아는 망이 안 보이면 `s_wifi_none = true` (1219), 20초마다 재스캔. 설정 화면이 열려 있으면 건너뜀 (1279).
- 매니저 루프 `wifi_manager_loop()` 1258-1284, 요청 종류 `WReq { AUTO, NEXT, SCAN, CONNECT, FORGET }` (196), 큐 `s_wq`·`wifi_post()` (198, 216-226).

### 1-4. "부팅 직후 첫 스캔 0개" 재시도
- `main.cpp:1210-1214` — 비어 있으면 1.5초 쉬고 최대 2번 더.
- 시험 펌웨어 판: `net_probe.cpp:77-85` (3회 시도).
- 실측 기록: `docs/daily/2026-09-26/claude.md:119`.

### 1-5. 끊김 재연결 (`s_have_target`)
- 선언 `main.cpp:1042`, 검사 1051(`s_manual || !s_have_target` 이면 재시도 안 함), 켜기 1200, 끄기 1219.
- 이유: 대상이 없는데 끊김 이벤트마다 재연결하면 이전 설정으로 헛돈다(reason 201) — `docs/daily/2026-09-26/claude.md:123`.
- 이벤트 핸들러 전체: `main.cpp:1043-1066`.

### 1-6. 설정 화면 (NET_LIST / NET_PASS)

| 조각 | 줄 범위 |
|---|---|
| 상태·전역 (`WifiUi`, `s_scan`, `s_sel_ssid`, `s_pw`, `s_badge_rect` …) | `main.cpp:194-214`, `wui_set` 227 |
| Wi-Fi 배지(누르면 설정 진입) | 472-497 |
| 목록 화면 그리기 (`wl_row`, `wl_btn`, `draw_bars`, `draw_lock`, `ui_button`, `ui_wifi_list`) | 593-643 |
| 비번 자판 (`PKEYS` 11열×5행, `pkey_rect`, `pkey_char`, `ui_wifi_pass`) | 645-712 |
| 열기/닫기 (`open_wifi_setup`, `close_wifi_setup`) | 1437-1446, 1471-1477 |
| 터치 처리 (`wifi_list_tap`, `wifi_pass_tap`) | 1479-1504, 1506-1533 |
| 수동 연결(15초 제한, 인증 실패 2회면 "비번 틀림", 성공한 것만 저장) | 1229-1256, 인증 사유 코드 1226 |
| `ui_task` 안의 분기 | 1587-1593 |
| 시리얼 검증 명령 `w`·`wscan`·`wsel`·`wauto`·`wforget`·`wlist` | 1703-1719 |

**의존성(옮길 때 같이 가져가거나 바꿔야 하는 것)**
- 그리기: `s_ov` 스프라이트(172), `sp_text`/`sp_text_w`/`sp_text_center`(443-447, 464-469), `sp_fill_smooth_rrect`(439), `fill_alpha_rect`/`fill_alpha_rrect`(386-402), `Rect::hit`(131), 색 상수(110-121), `lbl_wifi_wait` 자산(491).
- 상태: `set_state`(832-837), `s_state`, `s_dirty`/`ui_invalidate`(140, 230), `s_ui_mutex`(149), `now_ms`(229).
- **카메라에 얽힌 부분(걷어내야 함)**: 열기/닫기가 `s_proc_mutex`·`s_paused`·`s_frozen_valid` 를 만진다(1439-1444, 1473-1476). 매니저 루프의 `fetch_styles()`·`styles_ok` (1262, 1274, 1281-1282).
- 이름 충돌 주의: `secrets.h` 매크로 `WIFI_SSID`·`WIFI_PASS` 는 식별자로 못 쓴다 (`main.cpp:24`, `net_probe.cpp:27`).
- 자판 화면에서는 터치 좌표를 로그에 남기지 않는다(비번 복원 방지) — `main.cpp:1543-1544`. 옮길 때 유지 필요.

**디자인 규칙과 어긋나는 곳(검수 95점 게이트 대비)**
- 목록 줄 높이 72 (`main.cpp:593`), 하단 버튼 270×80 (595), 보기/숨기기 180×64 (663), 배지 히트 높이 76 (474, 482) — 모두 터치 96px 미만. 자판 키는 109×98 로 통과 (660-662).
- 버튼 글자는 efontKR_16_b × 1.6 = 약 26px (610) — "버튼 라벨 32px 이상"·"내장 글꼴 확대 금지"(`docs/design/README.md:33-34`)와 다르다.

---

## 2. 화면 그리기 관례

### 2-1. 구조: 전체 합성 → PPA 로 한 번에 출력 (부분 갱신 없음)
- 설명: `main.cpp:6-10`. 1280×720 RGB565 버퍼에 UI 를 합성한 뒤 PPA 가 회전해 DSI 프레임버퍼에 직접 쓴다. `M5.Display` 에 덧그리지 않는다.
- 합성 버퍼 2장: `jpeg_alloc_decoder_mem` 으로 할당 (1830), 전역 163-172.
- 스프라이트를 버퍼에 묶기: `bind_overlay_sprite()` 1777-1794 (`rgb565_nonswapped`, 바이트 순서 자가 검증).
- 출력: `present()` 331-348, 완료 대기 `ppa_wait_idle()` 328, PPA 등록 1834-1837, DSI 버퍼 얻기 1839-1844.
- 정지 화면은 `s_dirty` 가 켜졌을 때만 다시 그린다: `ui_invalidate()` 230, `render_paused_locked()` 818-827, 호출 1615-1622. 1회 60~80ms (1605 주석).
- 전화기에는 카메라가 없으므로 버퍼 1장으로 줄일 수 있고 `s_frozen`·버퍼 교대(366)는 필요 없다.
- 대조: 시험 펌웨어는 `M5.Display` 에 줄 단위로 직접 그린다(`probe_main.cpp:180-186`). 회전 상태의 전체 화면 `pushSprite` 는 652ms 로 느리다는 실측이 있다(`docs/DEVICE-SPEC.md:165`).

### 2-2. 글꼴
- 고정 문구·숫자: 맥에서 Pretendard 로 구운 8비트 알파 마스크. 생성기 `gen_ui_assets.py:22-40`(라벨 28~40px, 숫자 260/40px), 형식 `ui_asset_t {w, h, a}` (85), 그리기 `blit_asset`/`blit_asset_center`/`draw_number` (`main.cpp:410-436`).
- 바뀌는 글자: `fonts::efontKR_16_b` 에 배율 1.1~2.0 (`main.cpp:443-447, 464-469`). 시험 펌웨어는 `efontKR_24` × 1.5 (`probe_main.cpp:190`). efontKR 은 2350자 범위(`docs/DEVICE-SPEC.md:165`).
- 생성기를 복사할 때 걷어낼 것: 스냅부스 스타일 이름 부분(`gen_ui_assets.py:15, 72-82, 94-103`).
- **주의**: 이 맥의 `~/Library/Fonts`·`/Library/Fonts` 에 Pretendard 가 없다. 생성기는 없으면 경고 없이 AppleSDGothicNeo 로 바꿔 그린다(`gen_ui_assets.py:43-47`).
- `ui_assets.h` 는 git 에 들어간다(무시 규칙 없음, 스냅부스 것은 24,015줄).

### 2-3. 색 상수 — `main.cpp:110-121` (RGB888, 디자인 토큰 다크 열)
`C_BG 161615`, `C_SURFACE 1D1D1B`, `C_LINE 2A2A28`, `C_INK F2F1EE`, `C_INK2 D9D8D4`, `C_MUTED 9A9893`, `C_RED_BG 3A2223`, `C_RED_T F0A7A5`, `C_GREEN_T A6D6AA`, `C_BLUE_T 8FCBF2`, `C_WHITE`, `C_SHUTTER E36A63`.
- 코드에 없는 토큰: pale-green 배경 `1F2E22`, pale-blue 배경 `1C2E3A`, pale-yellow 쌍. 받기(초록)·거절(빨강) 버튼에 필요하면 추가.
- 565 변환·알파 혼합: `rgb565`/`blend565` 371-382.

### 2-4. 버튼·터치 판정
- `struct Rect { x,y,w,h; bool hit(px,py) }` — `main.cpp:131`
- `ui_button(const Rect&, const char *label, bool enabled, uint32_t bg, uint32_t fg)` — 607-611
- 원 판정 `hit_circle(x,y,cx,cy,r)` — 1434, 전원 버튼 `hit_power_button` — 737(반지름 +16 확장)
- 반투명 원/둥근 사각형: `fill_alpha_circle` 404-407, `fill_alpha_circle_soft` 539-543, `fill_alpha_rrect` 393-402. 큰 원 버튼 예: `ui_shutter` 546-557.
- 확인 창 예(취소/실행 2버튼, 6초 자동 취소): `ui_power_ask` 739-753, 처리 1582-1586.
- 히트 영역은 그림보다 넓게 잡는 관례: 배지 ±16 (482), 다시 찍기 ±16 (1595), 셔터 반지름 +28 (1562).

### 2-5. 상태 enum 과 전환
- `enum class State { IDLE, COUNTDOWN, PROCESSING, RESULT, ERROR, NET_LIST, NET_PASS, POWER_ASK }` — `main.cpp:126`
- `set_state()` 832-837: 상태·진입 시각 기록, `state -> N` 로그(검증 도구의 `wait:` 대상), 다시 그리기 요청.
- `ui_task` 1535-1625: 20ms 루프. `M5.update()` → 터치 → `switch (s_state)` (1547-1602) → 필요 시 렌더. 타이머는 `el = now_ms() - s_state_since_ms` (1545).
- 그리기 분기: `draw_overlay(State, el)` 756-815.
- 250ms 페이드: 771, 792, 재렌더 1606-1613.
- 잠금: UI 자료는 `s_ui_mutex` (149, 823-825), I2C 는 `s_i2c_lock` 으로 `M5.update()` 를 감쌈 (158, 1539). 전화기에서는 코덱 제어와 같은 버스를 쓰므로 같은 잠금이 필요하다(`board_audio.cpp:44-46, 54-56`, `probe_main.cpp:193`).
- 생존 감시: `ui_task` 하트비트가 10초 멈추면 재시작 — 1630-1642.

### 2-6. `docs/design/README.md` §1·§2 필수 규칙 요약
- **색(§1, 13-23줄)**: 토큰만 쓴다. 다크 기본 — bg `#161615`, surface `#1D1D1B`, line `#2A2A28`, ink `#F2F1EE`, ink-2 `#D9D8D4`, muted `#9A9893`, 파스텔 4쌍(blue/yellow/green/red), accent `#E36A63`.
- **서체·모양(25줄)**: 본문 Pretendard, 제목 Newsreader, 라벨 JetBrains Mono. 카드 라운드 12px, 칩 9999px.
- **터치(29줄)**: 96×96px 이상, 주요 액션 160px 이상·하단 중앙. 상태 글자는 상단(손가락 가림).
- **글자 크기(34줄)**: 안내 28px 이상, 버튼 라벨 32px 이상, 큰 숫자 240px 이상. 사전 렌더 알파 마스크, 짧은 동적 문자열만 내장 글꼴.
- **그리기(33줄)**: 화면에 직접 덧그리지 말고 버퍼에 합성 후 한 번에 출력. 도형은 안티앨리어싱. 이미지 위 글자는 surface 80~90% 반투명 패널 위.
- **모션(35줄)**: 200~300ms.
- **오류 문구(36줄)**: 원인 + 다음 행동 한 문장.
- **검수(44-45줄)**: 토큰 외 색 금지, 여백 8px 그리드, 위계 3단 이내, 95점 게이트(7줄).
- "상태는 4가지만"(31줄)·풀블리드(28줄)·스타일 칩(30줄)은 스냅부스 전용. 전화기 화면 6종은 설계서 §10(187-200줄)이 정한다: 통화 버튼 지름 200px 이상, 대기 60초 뒤 밝기 낮춤.

---

## 3. 터치

- 읽기: `M5.update()` 뒤 `M5.Touch.getDetail()` — `main.cpp:1539-1542`, `probe_main.cpp:193-195`. 둘 다 20ms 루프.
- **회전 3 좌표**: `M5.Display.setRotation(3)` 을 걸면 `getDetail()` 좌표가 가로 1280×720 논리 좌표로 나온다. 스냅부스는 화면을 PPA 로 직접 쓰면서도 터치 좌표계를 맞추려고 회전을 건다 (`main.cpp:92, 1805`). `docs/DEVICE-SPEC.md:166`.

| | 스냅부스 | 시험 펌웨어 |
|---|---|---|
| 판정 | `t.wasClicked()` (`main.cpp:1542`) | `t.wasPressed()` (`probe_main.cpp:195`) |
| 뜻 | 손을 뗀 순간, 움직이지 않았고 0.5초 안에 뗀 경우만 | 손이 닿은 순간마다 |
| 디바운스 | 없음 | 없음 |
| 보조 장치 | 결과 화면 진입 후 700ms 무시(104, 1595), 연결 중 자판 잠금(1508), 끄는 중 무시(1583) | 없음 |

- 정의 위치(가져온 라이브러리, git 제외): `~/tab5-toy-phone/firmware/tab5_snapbooth/managed_components/m5stack__m5unified/src/utility/Touch_Class.hpp:78-82`, 상태 계산 `Touch_Class.inl:58-124`, 기준값 `_flickThresh = 8`(화소)·`_msecHold = 500`(ms) `Touch_Class.hpp:116-117`.
- **`wasClicked` 의 함정**: 0.5초 넘게 누르면 `hold_end`, 8화소 넘게 미끄러지면 `flick_end` 가 되어 클릭으로 안 잡힌다. 받기·끊기 같은 큰 버튼을 꾹 누르는 사용자는 놓칠 수 있다. 모든 떼기를 잡는 것은 `wasReleased()` (`Touch_Class.hpp:82`).
- **7회 기록 건**: 원인은 기록으로 확정할 수 없다. 남은 것은 누적 횟수뿐이고(`~/tab5-toy-phone/firmware/tab5_phone_probe/out/soak_aborted_2119.log:3` 에 `touch=7`, 4줄에 39) 좌표·시각이 찍힌 `TOUCH` 줄은 로그에 없다.
  - 코드상 가능한 경로(추정): `wasPressed` 는 접촉이 잠깐 끊겼다 이어지면 다시 선다. 터치 칩 드라이버가 I2C 읽기 반환값을 확인하지 않아(`…/managed_components/m5stack__m5gfx/src/lgfx/v1/platforms/esp32p4/Touch_ST7123.inl:132-141`) 읽기 실패 한 번이 "뗌"으로 보일 수 있다.
  - `wasClicked` 로 바꿔도 접촉 끊김에는 똑같이 여러 번 잡힌다. 버튼별 최소 간격(예: 같은 버튼 300ms)이나 상태 진입 직후 무시 구간을 직접 넣어야 한다. 스냅부스의 700ms 가드가 그 선례다.

---

## 4. 자율 검증 훅

### 4-1. 펌웨어 쪽

| 조각 | 줄 범위 | 전화기에 필요한 최소 부분 |
|---|---|---|
| 시리얼 읽기 태스크 | `main.cpp:1724-1751` | 드라이버 설치 1726-1727, 줄 모으기 1728-1743. 한 글자 명령(1735, 1744-1749)은 뺀다. 줄 전용 판은 `probe_main.cpp:304-318` 이 더 단순 |
| 터치 주입 | 전역 `main.cpp:192`, 파싱 1740, 소비 1544 | 세 줄 그대로 |
| 스크린샷 `J` | `dump_screenshot_serial()` 1677-1696 | + JPEG 인코더 905-925, `s_jenc_mutex`(180, 1812), `s_last_presented`(326, 346), `ppa_wait_idle`(328) |
| 줄 명령 분배 | `serial_line_cmd()` 1701-1722 | 틀만 |

- `J` 는 **합성 버퍼가 있어야** 동작한다(마지막으로 화면에 낸 1280×720 버퍼를 하드웨어 JPEG 로 인코딩). 2-1 의 합성 구조를 쓰지 않으면 못 가져온다.
- 빌드 의존: `esp_driver_jpeg`, `mbedtls`, `esp_driver_usb_serial_jtag` (`~/tab5-toy-phone/firmware/tab5_snapbooth/main/CMakeLists.txt:3-5`), 헤더 `main.cpp:51-52, 67-69`.
- 출력 형식: `SHOT BEGIN <바이트>` → `B64:<3072바이트 분량>` 줄들 → `SHOT END` / 실패 `SHOT FAIL` (1686-1694).
- 합성 버퍼 없이 하는 대안(Arduino 선례): 8줄씩 `readRectRGB` 로 읽어 원시 RGB 전송 — `~/tab5-toy-phone/firmware/tab5_hangul_wp/tab5_hangul_wp.ino:264-281`. 2.7MB 라 느리고 유실이 있다(275줄 주석).

### 4-2. `snap_ctl.py` 사용법
- 문서: `snap_ctl.py:2-11`. `python3 snap_ctl.py run "wait:<정규식>:<초>" "J:out.png" "t 640 360" "sleep:1" "wlist"`, `listen <초>`.
- 구현: `Session.lines/pump/send/shot` 20-52, 단계 실행 54-69. `shot` 은 크기만 확인한다(47), 제한 20초(40).
- 명령 표: `~/tab5-toy-phone/firmware/tab5_snapbooth/README.md:40-61`.

### 4-3. 주의점과 출처
- **주입 슬롯 1칸**: `s_inj_tap`·`s_inj_x`·`s_inj_y` 한 벌뿐(`main.cpp:192`). `ui_task` 가 루프당 하나만 소비(1544)하고 루프는 20ms + 렌더 60~80ms(1605, 1623). 그 사이 두 번 넣으면 앞의 것이 덮인다. 실제 터치가 있으면 주입은 다음 루프로 밀린다(`!tap &&`).
- **0.5초 간격·사이에 `J` 금지**: 코드가 강제하지 않는다. 경험 규칙으로만 적혀 있다 — `docs/daily/2026-09-26/claude.md:134`(스크린샷을 겹쳐 보내 주입 1개가 덮여 비번이 9자리만 입력된 사고). `snap_ctl.py` 는 단계 사이에 자동으로 쉬지 않으므로(60-67) 호출자가 `sleep:0.5` 를 넣어야 한다.
  - 기전(코드로 본 추정): `J` 는 시리얼 태스크 안에서 끝까지 돈다(1739). 그동안 온 명령은 256바이트 수신 버퍼에 쌓였다가 한꺼번에 처리된다.
- **포트를 열면 기기가 리셋될 수 있다**: `snap_ctl.py:2`, `probe_ctl.py:12`. 한 시험은 한 번의 실행 안에서. 리셋 여부와 무관하게 맞추는 `until:` 단계는 `probe_ctl.py:73-78, 132-139`.
- **큰 데이터는 검사합 + 재시도**: 스냅부스 `J` 에는 검사합도 줄마다 양보도 없다. 시험 펌웨어는 감시 경고가 데이터 줄에 끼어든 뒤 줄마다 `vTaskDelay(2)` 와 검사합을 넣었다(`probe_main.cpp:268-281`, 받는 쪽 `probe_ctl.py:37-55`). 그 뒤에도 가끔 96바이트 유실 — 설계서 268줄. 전화기 `J` 는 이 방식을 따르는 편이 안전하다.
- **출력 속도**: 콘솔이 115200bps 에 묶여 초당 약 11.5KB(설계서 268줄). 생성된 설정에서 주 콘솔은 UART, USB 는 보조다(`~/tab5-toy-phone/firmware/tab5_snapbooth/sdkconfig:1556-1562`; `sdkconfig.defaults` 에는 지정 없음).
- **두 대 문제**: `snap_ctl.py:15-18` 은 "첫 번째 포트"를 연다. 두 대가 꽂혀 있으면 엉뚱한 기기를 리셋한다(`~/tab5-toy-phone/firmware/tab5_phone_probe/README.md:30`). `phone_ctl.py` 는 `probe_ctl.py:20-34` 의 번호 선택을 써야 한다. 두 포트를 동시에 여는 도구는 저장소에 없다(걸기↔받기 자동화에 필요).

---

## 5. 부팅 가드·회전·밝기

- **스냅부스 `app_main` 에는 패널 가드가 없다** (`main.cpp:1799-1807`).
- ESP-IDF 판: `panel_guard()` — `probe_main.cpp:323-331`. 화면 폭이 0 이면 NVS `"probe"`/`"rst"`(u8)를 올리고 `esp_restart()`, 최대 3회, 성공하면 0 으로.
- 호출 순서 — `probe_main.cpp:335-343`: `M5.begin(cfg)` → `nvs_flash_init()`(실패해도 지우지 않음, 339) → `panel_guard()` → `setRotation(3)` → `setBrightness(160)` → `fillScreen`.
  - 가드가 NVS 를 쓰므로 `nvs_flash_init()` 을 Wi-Fi 태스크가 아니라 `app_main` 앞쪽에서 불러야 한다(스냅부스는 Wi-Fi 태스크 안, `main.cpp:1288`).
  - 소리를 직접 다루려면 `cfg.internal_spk = false; cfg.internal_mic = false;` (`probe_main.cpp:336-337`).
- Arduino 판(같은 생각, 최대 5회): `~/tab5-toy-phone/firmware/tab5_hangul_wp/tab5_hangul_wp.ino:311-325`(이유 20-22줄), `~/tab5-toy-phone/firmware/tab5_photoframe/tab5_photoframe.ino:231-238`. 배경: `docs/DEVICE-SPEC.md:165`(소프트 리셋 뒤 거의 매번 한 번 실패, `RTC_DATA_ATTR` 는 유지 안 됨).
- 스냅부스 화면 설정: `main.cpp:1803-1807` — `setColorDepth(16)`, `setSwapBytes(true)`, `setRotation(DISPLAY_ROTATION)`, `setBrightness(200)`. 회전 정의 `main.cpp:91-92`, `~/tab5-toy-phone/firmware/tab5_snapbooth/main/snap_config.h:3-6`, 시험 펌웨어 `probe_main.cpp:47`.
- 대기 중 밝기를 낮추는 코드는 저장소에 없다(`setBrightness` 호출은 위 두 곳뿐).

---

## 6. 빌드 설정

- **`sdkconfig.defaults`**
  - 스냅부스 `~/tab5-toy-phone/firmware/tab5_snapbooth/sdkconfig.defaults:1-68`. 필수: rev 1.x (29-30), PSRAM 200MHz (15-21), esp_hosted SDIO 핀·리셋 (36-56), HTTPS·소켓 10 (58-59). 카메라(61-68)·USB 호스트(24-26)는 전화기에 불필요.
  - 시험 펌웨어 `~/tab5-toy-phone/firmware/tab5_phone_probe/sdkconfig.defaults:1-50` = 위에서 카메라·USB 를 뺀 것. 전화기는 이쪽을 복사하는 편이 맞다.
- **`partitions.csv`** (두 곳 동일, 1-4줄): nvs `0x11000` 크기 `0x6000`, phy_init `0x17000`, factory 앱 `0x20000` 6M. 표 위치 `0x10000`(defaults 7줄). OTA 없음.
- **`CMakeLists.txt`**: 시험 펌웨어 `~/tab5-toy-phone/firmware/tab5_phone_probe/CMakeLists.txt:1-9` 가 최소형(esp_hosted 패치 훅만). 스냅부스 1-16 의 4줄·12-16줄은 카메라 전용.
- **`patch_esp_hosted.py:1-28`** (두 사본 동일)
  - 대상: `managed_components/espressif__esp_hosted/host/port/esp/freertos/src/port_esp_hosted_host_init.c` (12줄)
  - 바꾸는 것: `static void __attribute__((constructor)) esp_hosted_host_init(void)` 의 `constructor` 를 `unused` 로 (13-15줄). `app_main` 전에 자동 초기화되지 않게 한다.
  - 이유(4-7줄): 런타임 핀 설정 전에 기본 핀으로 전송을 띄움, 큰 펌웨어에서 SDIO 메모리 풀 할당 실패로 부팅 루프.
  - 멱등(표식 `PATCHED (tab5_snapbooth)`, 21줄), 아직 안 받아졌으면 0 반환(18-19줄), 패턴이 없으면 1 → CMake 중단(23-24줄).
  - 경로를 스크립트 위치 기준으로 잡아(11줄) `firmware/tab5_phone/tools/` 에 복사하면 그대로 동작. 두 프로젝트 모두 적용돼 있음을 대상 파일 17-18줄에서 확인.
- **`main/CMakeLists.txt`**: 스냅부스는 `REQUIRES` 명시(1-5줄), 시험 펌웨어는 생략해 전 컴포넌트 자동 참조(`~/tab5-toy-phone/firmware/tab5_phone_probe/main/CMakeLists.txt:1-4`).
- **`main/idf_component.yml`**
  - 스냅부스 1-7줄: `m5unified '*'`, `esp_wifi_remote '*'`, `esp_hosted ~2.11.6`, `usb_host_uvc ^2`, `esp_video '*'`.
  - 시험 펌웨어 1-11줄(판 고정): m5unified 0.2.23, m5gfx 0.2.30, esp_wifi_remote 1.6.5, esp_hosted 2.11.7, esp_codec_dev 1.6.2, esp-sr 2.5.5, esp_audio_codec 2.5.0.
  - `esp_websocket_client` 는 어느 yml 에도 없다. 설계서 44줄이 1.8.0 을 적었을 뿐이고 esp_hosted 고정과 함께 빌드된 적이 없다.
- **`secrets.h.example`** (`~/tab5-toy-phone/firmware/tab5_snapbooth/main/secrets.h.example:1-10`, 이름만): `WIFI_SSID`, `WIFI_PASS`, `MDNS_NAME`, `SNAPBOOTH_URL`, `SNAPBOOTH_TOKEN`, `WIFI_NETWORKS`, `SNAPBOOTH_URLS`.
  - 코드가 실제로 쓰는 것: `WIFI_NETWORKS`(`main.cpp:184`), `SNAPBOOTH_URLS`(186), `SNAPBOOTH_TOKEN`(1339, 1664).
  - 시험 펌웨어에는 example 이 없다(스냅부스 `secrets.h` 복사). 교환기 주소·MAC→토큰 표 매크로는 아직 이름이 정해진 곳이 없다.
- **`.gitignore`** (`~/tab5-toy-phone/.gitignore`): `*.bin`(2), `__pycache__/`(3), `firmware/**/secrets.h`(10), `firmware/**/build/`(12), `firmware/**/managed_components/`(13), `firmware/**/sdkconfig`(14), `firmware/**/sdkconfig.old`(15), `firmware/**/dependencies.lock`(16), `.pytest_cache/`(28), 시험 펌웨어 전용 31-32.
  - `git check-ignore` 로 확인: `firmware/tab5_phone/` 의 secrets.h·build·managed_components·sdkconfig·dependencies.lock 은 이미 무시된다. **`firmware/tab5_phone/out/` 은 무시되지 않는다** — 스크린샷·로그 폴더를 만들기 전에 규칙을 먼저 넣어야 한다(설계서 56줄).

---

## 7. 순수 로직을 맥에서 시험하는 관례 — 있음

- **C++**: 프레임워크·CMake 호스트 빌드 없이 `clang++` 한 줄.
  - `test_wifi_store.cpp:1-48` — 실행 `clang++ -std=c++17 -I../main test_wifi_store.cpp -o /tmp/t && /tmp/t` (1줄), `CHECK(name, cond)` 매크로(6줄), 실패 시 종료 코드 1(47줄). 16항목.
  - `~/tab5-toy-phone/firmware/tab5_hangul_wp/tools/test_ime.cpp:1-94` — `clang++ -std=c++17 -Wall -Wextra -I.. -o /tmp/test_ime test_ime.cpp && /tmp/test_ime`. 37항목.
  - 전제: 로직을 ESP-IDF 헤더를 넣지 않은 헤더 전용 파일로 분리(`wifi_store.h:1`, `~/tab5-toy-phone/firmware/tab5_hangul_wp/hangul_ime.h:3-4`), 시험은 `tools/` 아래.
  - `call_fsm`·`jitter_buffer` 도 같은 방식으로 맞추면 된다(설계서 158-159, 205줄).
- **파이썬(맥 쪽 도구·서버)**: pytest.
  - `~/tab5-toy-phone/firmware/tab5_phone_probe/tools/test_aec_metrics.py` — 실행 `cd firmware/tab5_phone_probe/tools && python3 -m pytest -q -p no:cacheprovider` (1줄). 가짜 포트로 `find_port` 시험 170-185.
  - `~/tab5-toy-phone/server/tests/test_phone_core.py`, `test_phone_relay.py`, 설정 `~/tab5-toy-phone/server/pytest.ini`.
- CI 는 없다. 통과 수치(16/16, 37/37)는 문서 기록이고 이번에 다시 돌리지 않았다.

---

## 8. 두 기기 구분

- **`probe_ctl.py:20-21`**: `UNITS = {"1": "AA:BB:CC:DD:EE:01", "2": "AA:BB:CC:DD:EE:02"}` — 1번 포토 액자, 2번 스냅부스.
- **포트 선택**: `find_port(unit, ports=None)` 24-34 — USB 일련번호(= MAC, 대문자·콜론)로 찾고 포트는 열지 않는다. CLI `--unit`(기본 2, 210줄), `port <번호>`(214-216줄).
- **플래시 전 확인 관례**: `esptool.py -p "$PORT" read_mac | grep -i <MAC> && idf.py -p "$PORT" flash` — `~/tab5-toy-phone/firmware/tab5_phone_probe/README.md:26-27`.
- **문서**: 설계서 41줄(MAC), 116줄(PH-D9: MAC 으로 `tab5-1`/`tab5-2` + 기기별 토큰, 두 대 같은 펌웨어), 164줄(`secrets.h` 에 MAC→토큰 표). `~/tab5-toy-phone/README.md:38`.
- **교환기 쪽 계약**: 기기 ID `tab5-1`·`tab5-2`, 헤더 `X-Device-Id`·`X-Device-Token` — `~/tab5-toy-phone/server/phone_relay.py:6-7, 106-110`, `~/tab5-toy-phone/server/phone_core.py:23-41`. 가짜 기기 클라이언트 예: `~/tab5-toy-phone/server/tools/phone_smoke.py:33-40`.
- **펌웨어 쪽**: MAC 을 읽거나 기기 번호를 정하는 코드가 없다(`esp_read_mac`·`esp_efuse_mac`·`getEfuseMac` 검색 0건).
  - 확인 필요(추정): `UNITS` 의 MAC 은 P4 칩의 것이다(esptool·USB 일련번호 기준). Wi-Fi 는 C6 가 담당하므로 `esp_wifi_get_mac()` 은 다른 값을 줄 수 있다. Phase 2 에서 부팅 때 찍어 `UNITS` 와 대조해야 한다.

---

## 덧붙임: 옮길 때 걸릴 수 있는 것

- **1번 기기의 NVS(추정, 미확인)**: 1번은 지금 Arduino 펌웨어다. 전화기 파티션 표의 nvs 영역(`0x11000~0x17000`)에 이전 펌웨어 잔재가 있으면 `nvs_flash_init()` 이 실패할 수 있다. 스냅부스는 지우고 재시도(`main.cpp:1288-1290`), 시험 펌웨어는 지우지 않는다(`probe_main.cpp:339`). 어느 쪽을 택할지 정해야 하고, 지우면 2번 기기에 저장된 Wi-Fi 목록도 사라진다.
- **`board_audio`**: 설계서 §9.5 가 같은 이름의 모듈을 요구하지만 §3 은 시험 펌웨어 코드를 복사하지 않는다고 했다. 참고 구현은 `board_audio.cpp`(220줄, I2C 제어 34-74줄). 마이크 증폭을 장치를 연 뒤 다시 거는 것 등 넘겨받을 사항은 설계서 263-269줄.
- **생성된 설정에 `CONFIG_ESP_HOSTED_WIFI_AUTO_CONNECT_ON_STA_START=y`** 가 켜져 있다(`~/tab5-toy-phone/firmware/tab5_snapbooth/sdkconfig:2695`). `s_have_target` 가 막는 "이전 설정으로 헛도는" 동작과 관련 있을 수 있다(확인 안 함).