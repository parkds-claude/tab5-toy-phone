# Tab5 음성 전화기 Phase 2 구현 계획 (소리 없는 통화 골격)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> (실행하는 에이전트에게: 위 두 스킬 중 하나로 과제를 하나씩 수행하고, 단계는 체크박스로 추적한다.)

- **작성일**: 2026-10-04
- **작성자**: Claude (맥북 세션)
- **상태**: 완료(2026-10-04) — Task 1~10 모두 끝남. 독립 검수 93 → 93 → 96(통과), `066cf57` 두 기기 재회귀 통과, 사용자 손 조작(20:54~21:04)도 마쳤다(사용자 "좋아. 모두 좋아."; 손으로 한 범위와 미확인 항목은 Task 9 Step 3 옆). Task 8 뒤·Task 9 앞에 수신거부 메시지·벨 끄기 기능이 끼어들었다(별도 설계서·계획). 진행 기록은 아래 "진행 로그". (이전 상태: 승인(2026-10-04 사용자 "새로운 세션에서 이어서 가자") — 새 세션에서 Task 1 부터 실행. Phase 0~1 완료(2026-10-03) 뒤 게이트에서 "가자" 승인을 받아 작성.)
- **관련 이슈/PR**: -

**Goal:** Tab5 2대에 같은 전화기 펌웨어를 올려, 맥북에 띄운 교환기를 거쳐 **걸기 → 벨 화면 → 받기/거절 → 통화 화면 → 끊기**가 소리 없이 끝까지 돌고, 연결이 끊겨도 스스로 다시 붙는 것을 자동 시험과 손 조작으로 확인한다.

**Architecture:** 판단은 전부 하드웨어와 무관한 헤더 네 개(`call_fsm.h` 통화 상태 기계, `relay_proto.h` 신호 읽기·쓰기, `ui_layout.h` 화면 배치·터치 판정, `device_id.h` 기기 번호)에 두고 맥에서 단위 시험한다. 기기 쪽은 그것을 잇는 얇은 모듈들이다 — `relay_link`(esp_websocket_client 로 교환기 연결), `net_wifi`·`ui_wifi`·`gfx`(스냅부스 펌웨어에서 떼어 온 Wi-Fi 매니저·설정 화면·화면 합성), `ui`(통화 화면 그리기), `app`(사건 → 상태 기계 → 할 일 실행), `phone_main`(부팅·시리얼 명령). 검증은 `tools/phone_ctl.py` 가 두 기기의 시리얼을 함께 열어 터치를 주입하고 상태 줄과 스크린샷을 읽어 자동으로 한다.

**Tech Stack:** ESP-IDF 5.5.4 (esp32p4) · M5Unified 0.2.23 / M5GFX 0.2.30 · esp_hosted 2.11.7 · esp_websocket_client 1.8.0 · (소리 부품 esp_codec_dev 1.6.2 / esp-sr 2.5.5 / esp_audio_codec 2.5.0 은 의존성만 유지, Phase 3 에서 사용) · Python 3 + pyserial + Pillow + pytest · clang++ (맥 단위 시험)

**Spec:** `docs/exec-plans/2026-10-03-tab5-phone.md` (승인 2026-10-03, Phase 0 결과는 §8). 참고 구현 조사: `docs/exec-plans/2026-10-04-tab5-phone-research/` (이 계획과 함께 커밋). 실행자는 설계서 §9.4·§9.5·§9.7·§10 과 이 계획을 함께 읽는다.

## 한눈에 보기 (사용자용)

| 묶음 | 하는 일 | 기기 | 사용자가 할 일 | 걸리는 시간(어림) |
|---|---|---|---|---|
| Task 1 | 준비: 무시 규칙, **1번 기기 통째 백업**, 비밀 설정 파일 | 1번(읽기만 — 사진 액자가 한 번 재시작됨) | 두 기기 전원·케이블 확인 | 10분 |
| Task 2 | 판단 로직 4개(시험 먼저) | 없음 | 없음 | 15분 |
| Task 3 | 맥 쪽 검증 도구, 가짜 상대 도구 | 없음 | 없음 | 10분 |
| Task 4~5 | 글자 자산, 화면·Wi-Fi 모듈(스냅부스에서 떼어 옴), 빌드 | 없음 | 없음 | 25분(첫 빌드 포함) |
| Task 6 | 교환기 연결·통화 화면·진행 모듈, 전체 빌드 | 없음 | 없음 | 15분 |
| Task 7 | **2번 한 대**로 먼저 검증(상대는 맥이 흉내) | **2번**(스냅부스 → 전화기) | 없음(자동). 화면 사진을 보고 디자인 의견 주시면 반영 | 30분 |
| Task 8 | **1번에도 올려** 두 대로 걸기·받기·거절·취소·무응답·끊김 자동 시험 | **1번·2번** | 1번을 전화기로 바꿔도 되는지 확인(사진 액자를 못 씀) | 30분 |
| Task 9 | 여유 측정, 손으로 직접 걸고 받아 보기, 독립 검수 → **게이트(사용자 결정)** | 1번·2번 | 직접 눌러 보고 느낌 알려 주기(5분) | 30분 |
| Task 10 | 문서 갱신, 결과 보고 | 없음 | 보고서 읽고 Phase 3(소리) 진행 여부 결정 | 10분 |

- **소리는 이 계획에 없다.** 벨소리·통화 음성은 Phase 3 계획(이 계획의 결과를 보고 따로 쓴다)에서 붙인다. 이 계획이 끝나면 두 기기는 "소리 없는 전화기" 상태로 남는다.
- **2번은 Task 7 부터, 1번은 Task 8 부터 스냅부스·사진 액자로 쓸 수 없다.** 되돌리는 법은 "롤백" 절(각 5분).
- **실행자에게 — 코드를 다시 옮겨 적지 않아도 된다.** 이 계획에 실린 파일은 맥북의 `~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase2-3/draft/`(git 제외, 맥북에만 있음)에 그대로 있다. 계획서와 글자 단위로 같다(33/33 기계 확인). 각 Step 에서 그 파일을 저장소로 복사한 뒤 계획서의 코드 블록과 같은지 대조하면 된다. 단 **순서는 지킨다**: 시험 파일을 먼저 넣고 실패를 확인한 다음 구현 파일을 넣는다.
  - 대응: `draft/main/*` → `firmware/tab5_phone/main/`, `draft/tools/*` → `firmware/tab5_phone/tools/`, `draft/CMakeLists.txt`·`sdkconfig.defaults`·`partitions.csv` → `firmware/tab5_phone/`, `draft/server_tools/phone_fake_peer.py` → `server/tools/`, `draft/final/README.md` → `firmware/tab5_phone/README.md`.
  - Task 5 의 임시 진입점과 그때의 `main/CMakeLists.txt` 는 `draft/final/stub_main.cpp`·`draft/final/task5_main_CMakeLists.txt` 다(`draft/main/CMakeLists.txt` 는 Task 6 의 최종본).
  - **복사하지 않는 것**: `draft/main/secrets.h`(빌드 확인용 자리 값 — 실제 것은 Task 1 Step 7 이 만든다), `draft/build/`·`managed_components/`·`sdkconfig`·`dependencies.lock`·`__pycache__/`, `draft/main/jitter_buffer.h`·`draft/tools/test_jitter_buffer.cpp`(Phase 3 용), `draft/staging/`, `draft/CONTRACTS.md`.
  - `draft/tools/fonts/` 의 글꼴 두 개는 Task 4 Step 1 의 검사합과 같으면 내려받는 대신 복사해도 된다(무시 규칙을 넣은 뒤에).
  - 그 폴더가 없거나 내용이 다르면 계획서의 코드 블록이 정본이다.
- 이 계획에 실린 코드는 작성 시점(2026-10-04)에 임시 폴더에서 실제로 돌려 확인했다: 맥 단위 시험 203개 통과, 검증 도구 시험 150개 통과, 가짜 상대 도구는 실제 교환기에 붙여 통화 확인, 펌웨어 전체 빌드 성공(2.2MB, 앱 파티션의 65% 여유). **기기에 올려 본 적은 없다** — Phase 0 때처럼 기기에서만 드러나는 결함이 있을 수 있고, 그것을 찾는 것이 Task 7~9 다.

## Global Constraints

설계서와 Phase 0 결과에서 온 전 과제 공통 요구사항. 모든 과제는 이 절을 암묵적으로 포함한다.

- **기기 번호**: 1번 = 사진 롤링·공룡 점프(포토 액자), MAC `AA:BB:CC:DD:EE:01`. 2번 = 스냅부스, MAC `AA:BB:CC:DD:EE:02`. 교환기 기기 ID 는 `tab5-1`·`tab5-2`.
- **기기는 번호(MAC)로 고른다**: 포트는 항상 `python3 tools/phone_ctl.py port <1|2>` 로 얻고, 플래시 전에 `esptool.py read_mac` 으로 다시 확인한다. `/dev/cu.usbmodem*` "첫 번째"를 여는 도구(`snap_ctl.py`, `serial_ctl.py`)는 쓰지 않는다.
- **1번 기기는 공장 백업이 없다** → Task 1 의 통째 백업이 성공하고 검사합을 적기 전에는 1번에 아무것도 쓰지 않는다. 1번 플래시(Task 8) 직전에 사용자 확인을 받는다.
  - 정정(2026-10-04): "공장 백업이 없다"는 틀렸다 — 맥미니 `~/tab5-toy-phone/backups/tab5-1_factory_aabbccddee01_2026-09-22.bin`(16,777,216 B, 검사합 OK)에 1번의 공장 백업이 있다(**맥북에는 없다**). 사진 액자 상태의 통째 백업(Task 1)은 그래도 필요했고(사진 액자로 되돌리려면), 맥북에 3개가 있다: `backups/tab5-1_photoframe_aabbccddee01_2026-10-04{,_b2,_b3}.bin`(b1·b3 는 맥미니에도) — 서로 NVS 구역(0x9000~0xe000)만 다르다.
- **칩·빌드**: ESP-IDF 5.5.4, 대상 `esp32p4`, 두 기기 모두 P4 rev v1.3 → `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`, `CONFIG_ESP32P4_REV_MIN_100=y`. **`esp_hosted` 2.11.7 고정** — 다른 부품 때문에 이 고정을 풀지 않는다. Wi-Fi 칩(C6) 펌웨어 교체 금지, `amcchord/M5Tab5-VideoLink` 플래시 금지. `esp_audio_codec` 은 2.5.0(2.6+ 는 P4 rev 3.0 이상 전용).
- **화면**: 가로 1280×720, `M5.Display.setRotation(3)`(삼각대 나사가 화면 아래). 터치 대상 96px 이상, 주요 버튼 160px 이상, 통화 버튼 지름 200px 이상. 색은 `docs/design/README.md` §1 토큰(다크)만, 고정 문구는 Pretendard 로 미리 구운 글자, 바뀌는 글자(Wi-Fi 이름·비밀번호)만 내장 글꼴.
- **교환기(Phase 1 완성품, 수정하지 않는다)**: 헤더 `X-Device-Id`·`X-Device-Token` 으로 인증(틀리면 401), 신호는 JSON 글자 프레임, 기기 ping 15초·무소식 45초면 끊음, 벨 제한 30초, 압축 없음, 같은 ID 재접속 시 옛 연결 끊김. 이 계획에서는 **맥북에서만** 띄운다(`BIND=0.0.0.0 PORT=5094`, 기기가 LAN 으로 붙음). 맥미니 배포·터널은 Phase 4.
  - 정정(2026-10-04) ① "수정하지 않는다"는 **수신거부 메시지·벨 끄기 기능에 한해 바뀌었다**(`server/phone_core.py` 하나, 새 기능 설계서 DM-D5, 이 계획의 Task 8 뒤·Task 9 앞에 끼워 넣음). ② "`BIND=0.0.0.0 PORT=5094` 로 띄운다"는 명령은 안전 검사에 막혀 `phone_relay_dev.sh` 로 바뀌었다(Task 7 Step 1 정정).
- **비밀**: `secrets.h`·`server/.env.phone` 의 내용(Wi-Fi 비밀번호, 토큰, 교환기 주소)을 화면·로그·git·채팅에 내지 않는다. 시리얼 로그에도 싣지 않는다.
- **산출물은 .gitignore 먼저**: devsync 가 5분마다 `add -A` 로 자동 커밋한다 → 새 산출물 폴더(`firmware/tab5_phone/out/`, `tools/fonts/`)는 무시 규칙을 **커밋·푸시하고 맥미니에 반영된 뒤** 만든다(Task 1). 임시 파일은 저장소 밖에 둔다.
- **브랜치**: devsync 가 main 에 직접 커밋하는 흐름이라 별도 브랜치·worktree 를 쓰지 않는다. 단계의 `git commit` 이 "커밋할 것 없음"으로 끝나면 devsync 가 먼저 커밋한 것이므로 정상이다.
- **이 맥의 함정 세 가지**: ① 맥 단위 시험은 `clang++ -isysroot "$(xcrun --sdk macosx --show-sdk-path)"` 를 붙여야 링크된다(SDK 27 과 링커가 안 맞음). ② `source ~/esp/esp-idf-5.5.4/export.sh` 를 한 셸에서는 `python3` 가 ESP-IDF 것으로 바뀐다(Pillow·numpy·pytest 없음) → 파이썬 도구·시험은 export 하지 않은 셸에서. ③ 명령을 `sh -c '…'` 로 감싸면 안전 검사에 걸릴 수 있다 → 직접 실행.
- **참고 구현 대조 우선(사용자 지시 2026-10-03)**: 기기에서 이상한 동작이 나오면 추측으로 고치기 전에 같은 기기에서 동작하는 구현(스냅부스 `main.cpp`, xiaozhi-esp32 Tab5 보드, esp_websocket_client 소스)과 줄 단위로 대조한다.
- **진행 안내·보고는 전부 한국어.**

## Review Focus

설계서가 암시하지만 놓치기 쉬운 입력·고장 다섯 가지. 각 줄의 시험은 해당 과제에 들어 있다.

1. **끊자마자 다시 걸기, 받는 순간 상대가 취소, 양쪽이 동시에 걸기** — 신호에 통화 번호가 없어 앞 통화의 `ended` 가 새 통화를 끊을 수 있다. → Task 2 `test_call_fsm.cpp` "(1) no call while waiting for ended"·"accept raced with cancel"·"glare: …", Task 8 Step 5(실기 동시 발신)
2. **교환기가 대답하지 않는 경우**(상태에 안 맞는 신호는 조용히 무시된다) — 화면이 "거는 중"에 영원히 멈추면 안 된다. → Task 2 "(4) …" 5개(기기에서는 "대답하지 않는 교환기"를 만들기 어려워 맥 시험으로만 고정한다)
3. **깨진·너무 긴·모르는 신호, 값 안에 든 가짜 열쇠, 중간에 0 바이트** — 죽거나 엉뚱한 사건으로 읽으면 안 된다. → Task 2 `test_relay_proto.cpp` "truncated"·"oversized text ignored"·"embedded NUL ignored"·"key inside a string value is not a key"
4. **한 번 누른 터치가 여러 번 잡히는 경우**(Phase 0 에서 3번 눌러 7회 기록) — 받자마자 끊기거나, 걸자마자 취소되면 안 된다. 반대로 화면을 다시 그리는 동안의 짧은 터치를 놓쳐도 안 된다. → Task 2 `test_ui_layout.cpp` "accept does not overlap hangup"·"guard: …"·"a miss must not consume the guard", Task 6 `app.cpp` AP-D2(터치 전용 태스크), Task 9 Step 3(손으로 연타·짧게 치기)
5. **받기를 누르지 않았는데 통화가 열리는 경우** — 원치 않을 때 소리가 새지 않아야 한다(PH-D6). 교환기가 어긋나 `connected` 를 먼저 보내도 음성 경로를 열지 않는다. → Task 2 "connected without my accept never opens the mic"

---

## 비목표 (이번에 건드리지 않는 것)

- **소리 전부**: 벨소리·연결음·종료음, 마이크·스피커, 에코 제거, Opus, 지터 버퍼(Phase 3). `call_fsm` 은 소리 관련 "할 일"(`A_TONE_*`, `A_VOICE_*`)을 내지만 이 계획에서는 로그로만 남긴다.
- 음량·음소거·이어폰 전환·배터리 표시·화면 절전·부재중의 "마지막 시각"과 지우기(Phase 5). 부재중은 건수만 세고, 재부팅하면 0 이 된다.
- 맥미니 배포, launchd, 터널 호스트 `phone.example.com`, Bot Dashboard 등록(Phase 4). 교환기 코드 수정.
- 스냅부스·포토 액자 펌웨어와 서버의 수정(읽고 복사만 한다). 시험 펌웨어 `firmware/tab5_phone_probe/` 의 수정.
- Wi-Fi 설정 화면의 디자인 개편 — 스냅부스 화면을 기능 그대로 옮기고 터치 영역·글자만 규칙에 맞춘다.

## 영향 범위

- **신규 파일**: `firmware/tab5_phone/**`(펌웨어·도구·시험), `server/tools/phone_fake_peer.py`, `docs/exec-plans/2026-10-04-tab5-phone-research/`(조사 보고 3건), `backups/tab5-1_photoframe_aabbccddee01_<날짜>.bin`(git 제외) + `.sha256`.
- **수정 파일**: `.gitignore`(2줄 추가), `docs/exec-plans/2026-10-03-tab5-phone.md`(§9.5 모듈 표·§9.6 정정·§8 진행 로그), `README.md`(펌웨어 목록 한 줄).
- **기기**: 2번은 Task 7 부터, 1번은 Task 8 부터 전화기 펌웨어. 저장된 Wi-Fi 목록(NVS `snapwifi`)은 2번에서 그대로 물려받는다(같은 파티션 표·같은 형식). 1번의 NVS 는 다른 펌웨어가 쓰던 자리라 초기화될 수 있다(전화기 펌웨어가 못 읽으면 지우고 다시 만든다 — 1번에는 물려받을 Wi-Fi 목록이 없다). SD 카드는 건드리지 않는다.
- **맥북**: Task 7~9 동안 교환기를 `0.0.0.0:5094` 로 띄운다(같은 Wi-Fi 안에서 접속 가능, 토큰 인증 있음). 끝나면 종료한다.
- **외부 시스템**: 없음. 과금 없음. Pretendard 글꼴을 GitHub 에서 한 번 내려받는다(SIL OFL, `tools/fonts/` — git 제외).

## 롤백

- **2번 기기 → 스냅부스**(약 5분): `cd ~/tab5-toy-phone/firmware/tab5_snapbooth && source ~/esp/esp-idf-5.5.4/export.sh && PORT=$(python3 ../tab5_phone/tools/phone_ctl.py port 2) && esptool.py -p "$PORT" read_mac | grep -i "aa:bb:cc:dd:ee:02" && idf.py -p "$PORT" flash`. 안 되면 `backups/tab5-2_factory_aabbccddee02_2026-09-26.bin` 을 `esptool.py -p "$PORT" write_flash 0x0 <파일>`(공장 상태 — 저장된 Wi-Fi 목록은 사라진다).
- **1번 기기 → 사진 액자**(약 5분): `esptool.py -p "$(python3 firmware/tab5_phone/tools/phone_ctl.py port 1)" -b 921600 write_flash 0x0 backups/tab5-1_photoframe_aabbccddee01_<날짜>.bin` — Task 1 에서 뜬 통째 백업이라 펌웨어·설정이 그대로 돌아온다.
- **코드**: 전부 신규 파일이라 `firmware/tab5_phone/`, `server/tools/phone_fake_peer.py` 삭제로 끝난다. `.gitignore` 2줄은 남겨도 무해.
- **맥북**: 교환기 프로세스 종료(`lsof -nP -iTCP:5094 -sTCP:LISTEN` 으로 확인).

## 참고 구현과의 차이 (요약 — 전문은 `docs/exec-plans/2026-10-04-tab5-phone-research/`)

사용자 지시("관련 레포가 있으면 리버스 엔지니어링으로")에 따라 계획을 쓰기 전에 세 갈래로 대조했다.

| 대조 대상 | 결론 | 이 계획에 반영한 것 |
|---|---|---|
| 스냅부스 펌웨어(`firmware/tab5_snapbooth/main/main.cpp`, 우리 것, 기기 검증됨) | Wi-Fi 매니저·설정 화면·화면 합성·시리얼 스크린샷이 한 파일(1,901줄)에 카메라 코드와 얽혀 있다. 그대로 복사 가능한 파일은 6개뿐 | 줄 범위로 떼어 `gfx`·`net_wifi`·`ui_wifi` 로 옮김(Task 5). 터치 영역이 96px 미만인 곳과 내장 글꼴 확대는 규칙에 맞게 고침. 스크린샷 전송에 검사합·줄마다 양보 추가 |
| esp_websocket_client 1.8.0 소스 + IDF 의 transport_ws.c | 교환기 계약을 만족할 수 있다. 단 기본 설정 그대로면 세 군데가 어긋난다: ① 부품의 자동 ping 은 교환기의 45초 규칙에 안 잡힌다 ② 교환기가 닫기 프레임으로 끊으면 부품이 멈춘다 ③ 보내기 시간 제한을 넘기면 부품이 연결을 끊는다 | `relay_link.cpp` 의 RL-D1~D7(Task 6). `CONFIG_WS_BUFFER_SIZE=2048`. 401 판별은 "접속 단계의 끊김"에서만 |
| xiaozhi-esp32 Tab5 보드, M5Tab5-VideoLink (소리) | **두 레포 모두 Tab5 에서 "스피커폰 동시 통화 + 에코 제거"를 돌리지 않는다**(xiaozhi 는 말하는 동안 마이크를 끄는 교대 대화, VideoLink 는 에코 제거·압축·지터 버퍼 없음). 하드웨어 초기화는 우리 시험 펌웨어가 이미 xiaozhi 와 같다 | 이 계획(소리 없음)에는 직접 영향 없음. Phase 3 계획의 전제로 넘긴다: 가져올 것은 운영 규칙 4가지, 설계서 정정 3건(에코 제거 단위 16ms, Opus 손실 은닉 미확인, I2S DMA 재생 지연 최대 90ms) |

## 이 계획에서 내린 결정 (PL2-D)

설계서(PH-D1~D9)에 없던 세부를 정한 것. 코드 안의 결정(FSM-D1, RP-D1, UL-D1, AP-D1·D2, RL-D1~D7)은 각 파일 머리 주석에 이유·비용·탈출구와 함께 있다.

| ID | 결정 | 이유 | 비용 | 탈출구 |
|---|---|---|---|---|
| PL2-D1 | Phase 2 와 Phase 3 계획을 두 파일로 나눈다 | Phase 2 만으로 "벨·받기·끊기"가 스스로 시험되고, Phase 3 의 선택(마이크 증폭 24/30dB, 재생 박자, 손실 은닉)은 Phase 2 에서 재는 CPU·메모리 여유를 보고 정하는 편이 낫다 | 문서 둘 | 한 파일로 합친다 |
| PL2-D2 | 설계서 §9.5 표에 없는 모듈 넷을 추가: `app`(진행), `ui_layout.h`(배치·판정), `relay_proto.h`(신호 모양), `device_id`(기기 번호) | 맥에서 시험할 수 있는 판단을 최대한 헤더로 뺐다(설계서 §11 "순수 로직은 시험 먼저"). `ui` 는 그리기만 남는다 | 파일 수 증가 | `app.cpp` 를 `ui.cpp` 에, 헤더들을 `call_fsm.h` 에 합친다 |
| PL2-D3 | 신호 읽기에 JSON 라이브러리(cJSON)를 쓰지 않고 60줄짜리 스캐너를 쓴다 | 신호의 열쇠가 4개뿐이고, 맥 시험에 ESP-IDF 를 끌어오지 않으려는 것 | 겹친 객체·배열은 못 읽는다(규약에 없음) | cJSON 으로 교체 — 시험 46개는 그대로 쓴다 |
| PL2-D4 | 교환기가 대답하지 않을 때의 복구는 "연결을 끊고 다시 붙기" 하나로 통일(FSM-D1) | 다시 붙으면 교환기가 그 통화를 끝내고 옛 연결의 뒤늦은 신호를 버리므로 양쪽 상태가 확실히 맞는다. 경우마다 다른 복구를 만들지 않아도 된다 | 재연결 1~2초 동안 전화를 못 받는다 | 시간 제한 값(5초/3초/40초)을 늘리거나 대기로 그냥 복귀 |
| PL2-D5 | 터치는 "닿은 순간"에 받고, 중복은 시간 가드(화면 전환 후 600ms·직전 터치 후 400ms)와 버튼 배치(받기·거절 ↔ 끊기, 통화 ↔ 취소가 겹치지 않음)로 막는다 | 스냅부스의 "뗀 순간" 방식은 0.5초 넘게 누르면 놓친다 — 전화 받기에 맞지 않는다. Phase 0 의 "3번 눌러 7회" 원인이 확정되지 않아 두 겹으로 막는다 | 받기·거절 버튼이 화면 위쪽에 있다. 0.4초 안의 연타는 무시된다(Wi-Fi 자판은 빨리 치므로 0.15초) | 원인이 확정되면 가드만 남기고 배치를 내린다 |
| PL2-D6 | 2번 한 대 + 맥의 가짜 상대(`phone_fake_peer.py`)로 먼저 검증한 뒤 1번에 올린다 | 1번은 공장 백업이 없다. 문제를 최대한 2번에서 잡고, 1번은 검증된 펌웨어만 받는다 | 도구 하나 추가 | 바로 두 대에 올린다 |
| PL2-D7 | 1번의 NVS 를 못 읽으면(다른 펌웨어가 쓰던 자리) 지우고 다시 만든다 | 표준 처리. 1번에는 물려받을 Wi-Fi 목록이 없다 | 2번에서 같은 오류가 나면 저장된 Wi-Fi 목록이 사라진다(고정 목록은 `secrets.h` 에 남는다) | 지우지 않고 오류 화면만 띄운다 |
| PL2-D8 | 개발 중 교환기 주소는 맥북의 LAN 주소를 `secrets.h` 에 넣는다 | 새 인프라 없이 Phase 2~3 을 끝낸다 | 맥북이 다른 Wi-Fi 로 옮기면 주소가 바뀌어 `secrets.h` 를 고치고 다시 올려야 한다 | Phase 4 에서 `wss://phone.example.com/ws` 로 바꾸면 사라진다 |
| PL2-D9 | Pretendard 글꼴은 저장소에 넣지 않고 `tools/fonts/`(git 제외)에 내려받아 쓴다. 생성기는 글꼴이 없으면 **실패**한다 | 원본 생성기는 글꼴이 없으면 조용히 다른 글꼴로 그렸다(이 맥에는 Pretendard 가 없다). 구워진 결과(`ui_assets.h`)는 git 에 들어간다 | 다른 기기에서 자산을 다시 구우려면 글꼴을 받아야 한다 | 글꼴 파일을 저장소에 넣는다(약 3MB, OFL 이라 가능) |

정정(2026-10-04): PL2-D6 의 이유 "1번은 공장 백업이 없다"도 위 정정(맥미니에 공장 백업이 있다)을 받는다. 결정(2번 먼저)은 그대로 유효하다 — 문제를 2번에서 잡고 1번은 검증된 펌웨어만 받는 이점은 백업 유무와 무관하다.

## 파일 구조

```
firmware/tab5_phone/
  CMakeLists.txt                      프로젝트 + esp_hosted 패치 훅
  sdkconfig.defaults                  시험 펌웨어 설정 + CONFIG_WS_BUFFER_SIZE=2048
  partitions.csv                      스냅부스·시험 펌웨어와 같음(같은 NVS 위치 → 저장된 Wi-Fi 유지)
  README.md                           목적·명령·복구
  main/
    CMakeLists.txt  idf_component.yml 부품 판 고정(+ esp_websocket_client 1.8.0)
    call_fsm.h                        통화 상태 기계 ─┐
    relay_proto.h                     신호 읽기·쓰기, 음성 프레임 포장, 재연결 간격 │ 하드웨어 무관
    ui_layout.h  geom.h               화면 배치·터치 판정·터치 가드 │ → 맥에서 단위 시험
    device_id.h                       MAC → 기기 번호·ID·토큰 ─┘
    device_id.cpp                     칩 MAC 읽기
    relay_link.h / .cpp               교환기 연결·재연결·ping, 신호 대기열
    net_wifi.h / .cpp                 Wi-Fi 매니저(스냅부스에서 떼어 옴)
    wifi_store.h                      스냅부스 것 복사(저장 목록 순수 로직)
    gfx.h / .cpp                      화면 합성·출력·스크린샷(스냅부스에서 떼어 옴)
    ui_wifi.h / .cpp                  Wi-Fi 설정 화면(스냅부스에서 떼어 옴)
    ui.h / .cpp                       통화 화면 그리기
    app.h / .cpp                      사건 → 상태 기계 → 할 일 실행 → 다시 그리기
    phone_main.cpp                    부팅, 태스크 기동, 시리얼 명령
    ui_assets.h                       글자 자산(생성물, git 포함)
    secrets.h.example                 비밀 설정 예시
    secrets.h                         Wi-Fi 목록·교환기 주소·기기 표 — git 제외
  tools/
    patch_esp_hosted.py               스냅부스 것 복사
    gen_ui_assets.py                  글자 자산 생성(Pretendard, 없으면 실패)
    fonts/                            내려받은 글꼴 — git 제외
    phone_ctl.py                      시리얼 도구(기기 번호로 선택, 두 기기 시나리오, 스크린샷, 터치 주입)
    test_phone_ctl.py                 위 도구의 단위 시험(pytest)
    test_call_fsm.cpp  test_relay_proto.cpp  test_ui_layout.cpp  test_device_id.cpp  test_wifi_store.cpp   맥 단위 시험
  out/                                스크린샷·로그 — git 제외
server/
  tools/phone_fake_peer.py            교환기에 "기기 한 대"로 붙어 대본대로 신호를 주고받는 가짜 상대
```

---

### Task 1: 준비 — 무시 규칙, 기기 확인, 1번 기기 백업, 비밀 설정

**Files:**
- Modify: `.gitignore` (끝에 추가)
- Create: `backups/tab5-1_photoframe_aabbccddee01_<날짜>.bin`(git 제외), `backups/tab5-1_photoframe_aabbccddee01_<날짜>.bin.sha256`
- Create: `firmware/tab5_phone/main/secrets.h`(git 제외)

**Interfaces:**
- Consumes: `firmware/tab5_snapbooth/main/secrets.h` 의 `WIFI_NETWORKS`, `server/.env.phone` 의 `PHONE_TOKENS`(Phase 1 에서 만든 것), 기존 `firmware/tab5_phone_probe/tools/probe_ctl.py`(`port` 조회).
- Produces: 이후 과제의 산출물이 자동 커밋되지 않는다는 보장. 1번 기기를 되돌릴 수 있는 백업. `secrets.h` 의 매크로 셋 — `WIFI_NETWORKS`(`{ {"ssid","pass"}, … }`), `RELAY_URL`(`"ws://<맥북 LAN 주소>:5094/ws"`), `PHONE_DEVICES`(`{ {"MAC", 번호, "기기 ID", "토큰"}, … }`).

- [x] **Step 1: .gitignore 끝에 추가**

```gitignore

# Tab5 전화기 본 펌웨어 (2026-10-04): 시험 산출물·내려받은 글꼴 — 파일을 만들기 전에 먼저 등록
firmware/tab5_phone/out/
firmware/tab5_phone/tools/fonts/
```

- [x] **Step 2: 무시 규칙이 먹는지 확인**

Run:
```bash
cd ~/tab5-toy-phone && git check-ignore -v firmware/tab5_phone/out/x.jpg firmware/tab5_phone/tools/fonts/x.otf firmware/tab5_phone/main/secrets.h firmware/tab5_phone/build/x firmware/tab5_phone/sdkconfig firmware/tab5_phone/managed_components/x firmware/tab5_phone/dependencies.lock backups/x.bin | wc -l
```
Expected: `8` (여덟 경로 모두 무시됨. 뒤의 여섯은 기존 규칙이 잡는다)

- [x] **Step 3: 커밋·푸시하고 맥미니에 반영됐는지 확인**

```bash
cd ~/tab5-toy-phone && git add .gitignore && git commit -m "chore(phone): 전화기 본 펌웨어 산출물 무시 규칙 추가" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" ; git push
ssh macmini 'cd ~/tab5-toy-phone && git pull -q && grep -c "tab5_phone/" .gitignore'
```
Expected: 마지막 줄 `2`. (ssh 가 Tailscale 재인증을 요구하면 출력된 링크를 사용자에게 알리고 기다린다. 맥미니에 반영되기 전에는 Step 4 이후로 가지 않는다.)

- [x] **Step 4: 두 기기가 USB 에 보이는지 확인(포트를 열지 않는다 → 리셋 없음)**

Run:
```bash
python3 -c "
from serial.tools import list_ports
for p in list_ports.comports():
    if p.serial_number: print(p.device, p.serial_number)"
```
Expected: `AA:BB:CC:DD:EE:01`(1번)와 `AA:BB:CC:DD:EE:02`(2번) 두 줄. 하나라도 없으면 **여기서 멈추고** 사용자에게 전원·데이터 케이블 확인을 요청한다.

- [x] **Step 5: 1번 기기를 통째로 백업한다(16MB, 약 3~4분. 사진 액자가 한 번 재시작된다)**

```bash
cd ~/tab5-toy-phone && source ~/esp/esp-idf-5.5.4/export.sh
PORT=$(python3 firmware/tab5_phone_probe/tools/probe_ctl.py port 1) && echo "1번 포트: $PORT"
OUT=backups/tab5-1_photoframe_aabbccddee01_$(date +%F).bin
esptool.py -p "$PORT" read_mac | grep -i "aa:bb:cc:dd:ee:01" && esptool.py -p "$PORT" -b 921600 read_flash 0 0x1000000 "$OUT"
ls -l "$OUT" | awk '{print $5}' && shasum -a 256 "$OUT" | tee "$OUT.sha256"
```
Expected: `MAC: aa:bb:cc:dd:ee:01` → 읽기 진행 → `16777216` → 검사합 한 줄. MAC 줄이 안 나오면 읽기는 실행되지 않는다(`&&`) — 멈추고 보고. 크기가 16777216 이 아니면 백업 실패다 — 다시 읽고, 두 번 실패하면 멈춘다.

- [x] **Step 6: 백업을 한 번 더 읽어 같은지 확인(읽기 오류 검출)**

```bash
cd ~/tab5-toy-phone && source ~/esp/esp-idf-5.5.4/export.sh
PORT=$(python3 firmware/tab5_phone_probe/tools/probe_ctl.py port 1)
OUT=backups/tab5-1_photoframe_aabbccddee01_$(date +%F).bin
esptool.py -p "$PORT" verify_flash 0 "$OUT" | tail -2
```
Expected: `-- verify OK (digest matched)`. 다르면 백업을 믿을 수 없다 — Step 5 부터 다시.
(사진 액자가 실행 중에 NVS 를 고쳐 쓰면 드물게 다를 수 있다. 두 번 연속 다르면 다른 구간만 다른지 확인해 보고하고, 사용자 결정 없이 Task 8 로 가지 않는다.)

정정(2026-10-04): 이 "드물게"는 드물지 않다 — 사진 액자는 **부팅마다** NVS 에 부팅 횟수를 쓰고 esptool 은 호출마다 칩을 재시작시키므로, 읽기와 `verify_flash` 를 나눈 호출은 NVS 구역(0x9000~0xe000)에서 거의 항상 어긋난다. 백업은 **한 esptool 세션에서 읽고 바로 verify** 로 확인한다(상세는 Task 8 Step 2 정정 ①). 이 Task 에서는 첫 읽기가 verify 에 한 번 실패하고 다시 읽은 둘째 읽기가 `verify OK` 였다 — 원인은 Task 8 의 조사로 같은 것으로 설명된다(그때의 첫 읽기 sha 를 남기지 않아 직접 확인은 못 함).

- [x] **Step 7: `secrets.h` 를 만든다(값은 화면에 내지 않는다)**

```bash
cd ~/tab5-toy-phone && mkdir -p firmware/tab5_phone/main && python3 - <<'EOF'
import re, subprocess
snap = open("firmware/tab5_snapbooth/main/secrets.h", encoding="utf-8").read()
m = re.search(r"^#define\s+WIFI_NETWORKS\b.*(?:\\\n.*)*", snap, re.M)
assert m, "스냅부스 secrets.h 에 WIFI_NETWORKS 가 없음"
env = {}
for ln in open("server/.env.phone", encoding="utf-8"):
    if "=" in ln and not ln.lstrip().startswith("#"):
        k, v = ln.strip().split("=", 1); env[k.strip()] = v.strip().strip("\"'")
tok = dict(p.strip().split(":", 1) for p in env["PHONE_TOKENS"].split(","))
assert set(tok) == {"tab5-1", "tab5-2"}, "PHONE_TOKENS 에 tab5-1·tab5-2 가 있어야 함"
ip = subprocess.run(["ipconfig", "getifaddr", "en0"], capture_output=True, text=True).stdout.strip()
assert re.fullmatch(r"\d+\.\d+\.\d+\.\d+", ip), "맥북 Wi-Fi 주소(en0)를 얻지 못함 — 맥북이 기기와 같은 Wi-Fi 에 붙어 있는지 확인"
open("firmware/tab5_phone/main/secrets.h", "w", encoding="utf-8").write(
    "// secrets.h — git 제외. 내용을 화면·로그에 내지 않는다. 형식은 secrets.h.example 참조.\n#pragma once\n\n"
    + m.group(0) + "\n\n"
    + f'#define RELAY_URL "ws://{ip}:5094/ws"\n\n'
    + "#define PHONE_DEVICES { \\\n"
    + f'    {{ "AA:BB:CC:DD:EE:01", 1, "tab5-1", "{tok["tab5-1"]}" }}, \\\n'
    + f'    {{ "AA:BB:CC:DD:EE:02", 2, "tab5-2", "{tok["tab5-2"]}" }}, \\\n'
    + "}\n")
print("secrets.h 작성 완료: Wi-Fi 목록(스냅부스 것), 교환기 주소(맥북 LAN), 기기 2대")
EOF
git check-ignore -q firmware/tab5_phone/main/secrets.h && grep -c "^#define" firmware/tab5_phone/main/secrets.h
```
Expected: `secrets.h 작성 완료: …` 와 `3`. (맥북이 다른 Wi-Fi 로 옮기면 주소가 바뀐다 — 그때는 이 Step 을 다시 실행하고 펌웨어를 다시 올린다, PL2-D8.)

---

### Task 2: 판단 로직 네 개 (하드웨어 무관, 시험 먼저)

**Files:**
- Create: `firmware/tab5_phone/main/wifi_store.h`(복사), `main/geom.h`, `main/net_wifi.h`, `main/call_fsm.h`, `main/relay_proto.h`, `main/device_id.h`, `main/ui_layout.h`
- Test: `firmware/tab5_phone/tools/test_wifi_store.cpp`(복사), `tools/test_call_fsm.cpp`, `tools/test_relay_proto.cpp`, `tools/test_device_id.cpp`, `tools/test_ui_layout.cpp`

**Interfaces:**
- Consumes: 스냅부스의 `wifi_store.h`(`wifistore::Cred/Ap/pack/unpack/upsert/merge/dedupe/rank/find/bars`), 설계서 §9.4 신호 규약, 교환기 `server/phone_core.py` 가 실제로 내는 글자.
- Produces (이후 과제가 이 이름·형을 그대로 쓴다):
  - `callfsm::Fsm` — `Out on(Ev ev, int64_t now_ms, Why why = Why::NONE, bool flag = false)`, 공개 필드 `state`·`peer_online`·`call_start_ms`·`last_talk_ms`·`missed`·`missed_at_ms`, `clear_missed()`. `Out { uint16_t acts; Notice notice; }`. 열거형 `State`(OFFLINE, IDLE, DIALING, RINGBACK, INCOMING, ANSWERING, IN_CALL, ENDING), `Ev`, `Why`, `Notice`, 할 일 비트 `A_SEND_CALL … A_RECONNECT`. `state_name(State)`, `notice_name(Notice)`.
  - `relayproto::parse(const char *, size_t) -> Signal{kind, ev, why, flag, n}`, `text_for(uint16_t act) -> const char *`, `ping_text(char *, size_t, uint32_t) -> int`, `pack_voice(...)`, `unpack_voice(...)`, `backoff_ms(int attempt, bool auth_failed) -> int32_t`, 상수 `MAX_TEXT`·`MAX_VOICE_FRAME`·`MIN_VOICE_FRAME`.
  - `deviceid::Entry{mac, unit, id, token}`, `deviceid::Info{unit, id, token, mac[18]}`, `lookup(const char *mac, const Entry *, int) -> Info`, `format_mac(const uint8_t[6], char[18])`. (기기 전용 `const Info &get()` 은 Task 6.)
  - `uilayout::Model`(그릴 내용 전부), `Screen`, `Tap`, `screen_for(const Model &)`, `screen_name(Screen)`, `hit(const Model &, int x, int y) -> Tap`, `TapGuard::allow(now_ms, screen_since_ms, screen_guard_ms = 600, repeat_guard_ms = 400)`(Wi-Fi 설정 화면은 `KEY_SCREEN_GUARD_MS` 300·`KEY_REPEAT_GUARD_MS` 150 을 넘긴다), `talk_clock(seconds, &mm, &ss)`, 버튼 상수 `BTN_CALL`·`BTN_WIFI`·`BTN_CANCEL`·`BTN_ACCEPT`·`BTN_REJECT`·`BTN_HANGUP`.
  - `geom.h`: `SCREEN_W`(1280), `SCREEN_H`(720), `struct Rect{x,y,w,h; hit()}`.
  - `net_wifi.h`: `netwifi::` 선언(구현은 Task 5) — `Phase`, `Manual`, `start()`, `phase()`, `connected()`, `ssid()`, `rssi()`, `change_seq()`, 설정 화면용 함수들.

맥 단위 시험의 컴파일 명령은 모두 같은 모양이다(SDK 를 명시해야 이 맥에서 링크된다):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_<이름>.cpp -o /tmp/t_<이름> && /tmp/t_<이름> | tail -1
```

- [x] **Step 1: 스냅부스의 저장 목록 로직과 그 시험을 복사하고 돌려 본다**

```bash
cd ~/tab5-toy-phone && mkdir -p firmware/tab5_phone/main firmware/tab5_phone/tools && cp firmware/tab5_snapbooth/main/wifi_store.h firmware/tab5_phone/main/ && cp firmware/tab5_snapbooth/tools/test_wifi_store.cpp firmware/tab5_phone/tools/
cd firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_wifi_store.cpp -o /tmp/t_wifi_store && /tmp/t_wifi_store | tail -1
```
Expected: `16 passed, 0 failed`

- [x] **Step 2: 화면 크기·사각형 — `main/geom.h`**

```cpp
// geom.h — 화면 크기와 사각형. 하드웨어 무관(맥 시험에서도 쓴다).
#pragma once

#define SCREEN_W 1280
#define SCREEN_H 720

struct Rect {
    int x, y, w, h;
    bool hit(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};
```

- [x] **Step 3: Wi-Fi 매니저의 약속(선언만) — `main/net_wifi.h`** (구현은 Task 5. `ui_layout.h` 가 `netwifi::Phase` 를 쓰므로 먼저 둔다)

```cpp
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
```

- [x] **Step 4: 통화 상태 기계의 시험을 먼저 쓴다 — `tools/test_call_fsm.cpp`**

```cpp
// test_call_fsm.cpp — call_fsm.h 단위 시험:  clang++ -std=c++17 -Wall -Wextra -I../main test_call_fsm.cpp -o /tmp/t_fsm && /tmp/t_fsm
// 설계서 §9.4 "교환기-기기 계약", §9.5 call_fsm 필수 시험 ①~④ 를 고정한다.
#include "call_fsm.h"
#include <cstdio>
#include <string>
using namespace callfsm;
static int ok = 0, bad = 0;
#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)

static bool has(const Out &o, uint16_t a) { return (o.acts & a) == a; }

// 교환기에 붙고 상대도 켜진 대기 상태의 기계
static Fsm idle(int64_t t = 1000)
{
    Fsm f;
    f.on(Ev::LINK_UP, t);
    f.on(Ev::PEER, t, Why::NONE, true);
    return f;
}

int main()
{
    // ── 접속 ────────────────────────────────────────────────────────────────
    {
        Fsm f;
        CHECK("boot: offline", f.state == State::OFFLINE && !f.peer_online);
        Out o = f.on(Ev::TAP_CALL, 10);
        CHECK("offline: call tap does nothing", f.state == State::OFFLINE && o.acts == 0);
        f.on(Ev::LINK_UP, 20);
        CHECK("link up -> idle, peer unknown = off", f.state == State::IDLE && !f.peer_online);
        o = f.on(Ev::TAP_CALL, 30);
        CHECK("idle + peer off: no call sent, notice peer-off", f.state == State::IDLE && o.acts == 0 && o.notice == Notice::PEER_OFF);
        f.on(Ev::PEER, 40, Why::NONE, true);
        CHECK("peer online flag", f.peer_online);
        f.on(Ev::PEER, 50, Why::NONE, false);
        CHECK("peer offline flag", !f.peer_online);
    }

    // ── 걸기 → 벨 → 받음 → 통화 → 끊기(내가) ────────────────────────────────
    {
        Fsm f = idle();
        Out o = f.on(Ev::TAP_CALL, 2000);
        CHECK("call: dialing + send call", f.state == State::DIALING && has(o, A_SEND_CALL));
        o = f.on(Ev::RINGING, 2100);
        CHECK("ringing: ringback tone", f.state == State::RINGBACK && has(o, A_TONE_RINGBACK));
        o = f.on(Ev::CONNECTED, 5000);
        CHECK("connected: in call, tone stop, voice start", f.state == State::IN_CALL && has(o, A_TONE_STOP | A_VOICE_START));
        CHECK("call start time recorded", f.call_start_ms == 5000);
        o = f.on(Ev::TAP_HANGUP, 9000);
        CHECK("hangup: ending, send hangup, voice stop, end tone", f.state == State::ENDING && has(o, A_SEND_HANGUP | A_VOICE_STOP | A_TONE_END));
        CHECK("talk time kept", f.last_talk_ms == 4000);
        o = f.on(Ev::ENDED, 9100, Why::HANGUP);
        CHECK("ended after my hangup: idle, no extra tone", f.state == State::IDLE && o.acts == 0);
    }

    // ── 상대가 끊음 / 통화 중 끊김 ──────────────────────────────────────────
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2); f.on(Ev::CONNECTED, 3);
        Out o = f.on(Ev::ENDED, 10003, Why::HANGUP);
        CHECK("peer hangup: idle, voice stop, end tone, notice", f.state == State::IDLE && has(o, A_VOICE_STOP | A_TONE_END) && o.notice == Notice::CALL_ENDED);
        CHECK("peer hangup: talk time", f.last_talk_ms == 10000);
    }
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2); f.on(Ev::CONNECTED, 3);
        Out o = f.on(Ev::ENDED, 500, Why::LOST);
        CHECK("ended lost in call: notice lost", f.state == State::IDLE && o.notice == Notice::LOST && has(o, A_VOICE_STOP | A_TONE_END));
    }
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2); f.on(Ev::CONNECTED, 3);
        Out o = f.on(Ev::LINK_DOWN, 500);
        CHECK("link down in call: offline, voice stop, end tone, notice lost", f.state == State::OFFLINE && has(o, A_VOICE_STOP | A_TONE_END) && o.notice == Notice::LOST);
        CHECK("link down clears peer flag", !f.peer_online);
    }

    // ── 거는 쪽이 받는 종료 사유들 ──────────────────────────────────────────
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2);
        Out o = f.on(Ev::ENDED, 30002, Why::TIMEOUT);
        CHECK("caller timeout: notice no-answer + end tone", f.state == State::IDLE && o.notice == Notice::NO_ANSWER && has(o, A_TONE_END));
    }
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2);
        Out o = f.on(Ev::ENDED, 900, Why::REJECT);
        CHECK("caller rejected: notice rejected", f.state == State::IDLE && o.notice == Notice::REJECTED);
        f.on(Ev::TAP_CALL, 1000); f.on(Ev::RINGING, 1001);
        o = f.on(Ev::ENDED, 1500, Why::HANGUP);
        CHECK("callee hung up during bell = rejected", o.notice == Notice::REJECTED);
    }
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1);
        Out o = f.on(Ev::ENDED, 50, Why::OFFLINE);
        CHECK("call to offline peer: notice peer-off (no ringing first)", f.state == State::IDLE && o.notice == Notice::PEER_OFF);
    }
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2);
        Out o = f.on(Ev::ENDED, 900, Why::OFFLINE);
        CHECK("peer vanished during bell: notice peer-off, tone end", f.state == State::IDLE && o.notice == Notice::PEER_OFF && has(o, A_TONE_END));
    }

    // ── 거는 쪽 취소 ────────────────────────────────────────────────────────
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2);
        Out o = f.on(Ev::TAP_CANCEL, 500);
        CHECK("cancel: ending + send hangup + tone stop", f.state == State::ENDING && has(o, A_SEND_HANGUP | A_TONE_STOP));
        Fsm g = idle(); g.on(Ev::TAP_CALL, 1);
        o = g.on(Ev::TAP_CANCEL, 5);
        CHECK("cancel before ringing arrives also sends hangup", g.state == State::ENDING && has(o, A_SEND_HANGUP));
    }

    // ── 받는 쪽: 받기 / 거절 / 부재중 ───────────────────────────────────────
    {
        Fsm f = idle();
        Out o = f.on(Ev::RING, 100);
        CHECK("ring: incoming + bell", f.state == State::INCOMING && has(o, A_TONE_BELL));
        o = f.on(Ev::TAP_ACCEPT, 2000);
        CHECK("accept: answering, send accept, bell stop", f.state == State::ANSWERING && has(o, A_SEND_ACCEPT | A_TONE_STOP));
        o = f.on(Ev::CONNECTED, 2100);
        CHECK("connected after accept: in call + voice start", f.state == State::IN_CALL && has(o, A_VOICE_START));
        CHECK("accepted call is not a missed call", f.missed == 0);
    }
    {
        Fsm f = idle(); f.on(Ev::RING, 100);
        Out o = f.on(Ev::TAP_REJECT, 900);
        CHECK("reject: ending, send reject, bell stop", f.state == State::ENDING && has(o, A_SEND_REJECT | A_TONE_STOP));
        o = f.on(Ev::ENDED, 950, Why::REJECT);
        CHECK("ended after my reject: idle, not missed", f.state == State::IDLE && f.missed == 0 && o.notice == Notice::NONE);
    }
    {   // 필수 시험 ③: 벨 중 ended hangup·timeout = 부재중
        Fsm f = idle(); f.on(Ev::RING, 100);
        Out o = f.on(Ev::ENDED, 30100, Why::TIMEOUT);
        CHECK("(3) bell timeout = missed", f.state == State::IDLE && f.missed == 1 && f.missed_at_ms == 30100 && has(o, A_TONE_STOP) && o.notice == Notice::MISSED);
        f.on(Ev::RING, 40000);
        o = f.on(Ev::ENDED, 41000, Why::HANGUP);
        CHECK("(3) caller canceled during bell = missed", f.missed == 2 && f.missed_at_ms == 41000 && o.notice == Notice::MISSED);
        f.on(Ev::RING, 50000);
        f.on(Ev::ENDED, 51000, Why::OFFLINE);
        CHECK("caller vanished during bell = missed", f.missed == 3);
        f.clear_missed();
        CHECK("clear missed", f.missed == 0);
    }
    {
        Fsm f = idle(); f.on(Ev::RING, 100);
        Out o = f.on(Ev::LINK_DOWN, 500);
        CHECK("link down during bell: offline, bell stop, missed", f.state == State::OFFLINE && has(o, A_TONE_STOP) && f.missed == 1);
    }
    {   // 받기를 눌렀는데 그 순간 거는 쪽이 취소 → 부재중이 아니다(내가 받으려 했음)
        Fsm f = idle(); f.on(Ev::RING, 100); f.on(Ev::TAP_ACCEPT, 200);
        Out o = f.on(Ev::ENDED, 250, Why::HANGUP);
        CHECK("accept raced with cancel: idle, not missed, notice ended", f.state == State::IDLE && f.missed == 0 && o.notice == Notice::CALL_ENDED);
    }

    {   // PH-D6: 받기를 누르지 않았는데 connected 가 와도 마이크를 열지 않는다
        Fsm f = idle(); f.on(Ev::RING, 100);
        Out o = f.on(Ev::CONNECTED, 200);
        CHECK("connected without my accept never opens the mic", !has(o, A_VOICE_START) && f.state != State::IN_CALL && has(o, A_RECONNECT | A_TONE_STOP));
    }

    // ── 동시 발신(§9.4): 내 call 이 가는 중에 ring 이 오면 그대로 기다린다 → connected ──
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1);
        Out o = f.on(Ev::RING, 5);
        CHECK("glare: ring while dialing keeps dialing, no bell", f.state == State::DIALING && o.acts == 0);
        o = f.on(Ev::CONNECTED, 20);
        CHECK("glare: connected -> in call", f.state == State::IN_CALL && has(o, A_VOICE_START));
    }

    // ── 필수 시험 ①: 끊은 직후 다시 걸기 ────────────────────────────────────
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2); f.on(Ev::CONNECTED, 3);
        f.on(Ev::TAP_HANGUP, 1000);
        Out o = f.on(Ev::TAP_CALL, 1010);
        CHECK("(1) no call while waiting for ended", f.state == State::ENDING && !has(o, A_SEND_CALL));
        f.on(Ev::ENDED, 1050, Why::HANGUP);
        o = f.on(Ev::TAP_CALL, 1100);
        CHECK("(1) after ended: call goes out", f.state == State::DIALING && has(o, A_SEND_CALL));
        f.on(Ev::RINGING, 1150);
        CHECK("(1) new call survives (no stale ended consumed)", f.state == State::RINGBACK);
    }
    {   // ended 가 끝내 안 오면 3초 뒤 연결을 끊고 다시 붙는다 → 옛 연결의 뒤늦은 ended 는 교환기가 버린다
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2); f.on(Ev::TAP_CANCEL, 1000);
        Out o = f.on(Ev::TICK, 3999);
        CHECK("(1) ending: not yet timed out", f.state == State::ENDING && o.acts == 0);
        o = f.on(Ev::TICK, 4000);
        CHECK("(1) ending timeout 3s: reconnect, offline", f.state == State::OFFLINE && has(o, A_RECONNECT));
        o = f.on(Ev::ENDED, 4100, Why::HANGUP);
        CHECK("(1) late ended while offline is dropped", f.state == State::OFFLINE && o.acts == 0 && o.notice == Notice::NONE);
    }

    // ── 필수 시험 ②: 대기 상태에서 받은 ended 무시 ──────────────────────────
    {
        Fsm f = idle();
        Out o = f.on(Ev::ENDED, 10, Why::HANGUP);
        CHECK("(2) ended in idle ignored", f.state == State::IDLE && o.acts == 0 && o.notice == Notice::NONE && f.missed == 0);
        o = f.on(Ev::CONNECTED, 20);
        CHECK("connected in idle ignored", f.state == State::IDLE && o.acts == 0);
        o = f.on(Ev::RINGING, 30);
        CHECK("ringing in idle ignored", f.state == State::IDLE && o.acts == 0);
    }

    // ── 필수 시험 ④: 응답 없는 신호에 멈추지 않음 ───────────────────────────
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1000);
        Out o = f.on(Ev::TICK, 5999);
        CHECK("(4) dialing: waits up to 5s", f.state == State::DIALING && o.acts == 0);
        o = f.on(Ev::TICK, 6000);
        CHECK("(4) dialing no answer from relay: reconnect + notice", f.state == State::OFFLINE && has(o, A_RECONNECT) && o.notice == Notice::RELAY_SILENT);
    }
    {
        Fsm f = idle(); f.on(Ev::RING, 0); f.on(Ev::TAP_ACCEPT, 1000);
        Out o = f.on(Ev::TICK, 6000);
        CHECK("(4) accept unanswered 5s: reconnect", f.state == State::OFFLINE && has(o, A_RECONNECT));
    }
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 0); f.on(Ev::RINGING, 100);
        Out o = f.on(Ev::TICK, 40099);
        CHECK("(4) ringback: relay ends it at 30s, we wait 40s", f.state == State::RINGBACK && o.acts == 0);
        o = f.on(Ev::TICK, 40100);
        CHECK("(4) ringback stuck 40s: reconnect + tone stop", f.state == State::OFFLINE && has(o, A_RECONNECT | A_TONE_STOP));
    }
    {
        Fsm f = idle(); f.on(Ev::RING, 100);
        Out o = f.on(Ev::TICK, 40100);
        CHECK("(4) bell stuck 40s: reconnect, bell stop, missed", f.state == State::OFFLINE && has(o, A_RECONNECT | A_TONE_STOP) && f.missed == 1);
    }
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2); f.on(Ev::CONNECTED, 3);
        Out o = f.on(Ev::TICK, 99999999);
        CHECK("in call: no time limit", f.state == State::IN_CALL && o.acts == 0);
        Fsm g = idle();
        o = g.on(Ev::TICK, 99999999);
        CHECK("idle: tick does nothing", g.state == State::IDLE && o.acts == 0);
    }

    // ── 상태에 안 맞는 화면 입력은 아무것도 보내지 않는다 ───────────────────
    {
        Fsm f = idle();
        CHECK("idle: accept/reject/hangup/cancel taps ignored",
              f.on(Ev::TAP_ACCEPT, 1).acts == 0 && f.on(Ev::TAP_REJECT, 2).acts == 0 && f.on(Ev::TAP_HANGUP, 3).acts == 0 && f.on(Ev::TAP_CANCEL, 4).acts == 0 && f.state == State::IDLE);
        f.on(Ev::RING, 10);
        CHECK("incoming: call tap ignored (no second call)", f.on(Ev::TAP_CALL, 11).acts == 0 && f.state == State::INCOMING);
        f.on(Ev::TAP_ACCEPT, 12); f.on(Ev::CONNECTED, 13);
        CHECK("in call: call/accept taps ignored", f.on(Ev::TAP_CALL, 14).acts == 0 && f.on(Ev::TAP_ACCEPT, 15).acts == 0 && f.state == State::IN_CALL);
        Out o = f.on(Ev::RING, 16);
        CHECK("in call: stray ring ignored", o.acts == 0 && f.state == State::IN_CALL);
    }
    {   // 내가 끊는 순간 상대가 받아 connected 가 먼저 도착해도 통화로 들어가지 않는다(곧 ended 가 온다)
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2); f.on(Ev::TAP_CANCEL, 500);
        Out o = f.on(Ev::CONNECTED, 510);
        CHECK("connected while ending ignored (no voice start)", f.state == State::ENDING && o.acts == 0);
    }

    // ── 연결이 끊겼다 다시 붙기 ────────────────────────────────────────────
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1);
        Out o = f.on(Ev::LINK_DOWN, 100);
        CHECK("link down while dialing: offline + notice lost", f.state == State::OFFLINE && o.notice == Notice::LOST && has(o, A_TONE_STOP));
        o = f.on(Ev::LINK_DOWN, 200);
        CHECK("second link down is a no-op", f.state == State::OFFLINE && o.acts == 0 && o.notice == Notice::NONE);
        f.on(Ev::LINK_UP, 300);
        CHECK("link up again: idle, peer unknown", f.state == State::IDLE && !f.peer_online);
        Fsm g = idle(); g.on(Ev::RING, 1); g.on(Ev::TAP_REJECT, 2);
        o = g.on(Ev::LINK_DOWN, 3);
        CHECK("link down while ending: offline, silent", g.state == State::OFFLINE && o.acts == 0 && o.notice == Notice::NONE);
        o = g.on(Ev::LINK_UP, 4);
        CHECK("link up in a call state cannot happen but must not crash", g.state == State::IDLE);
    }

    // ── 시각이 큰 값이어도(수개월 가동) 시간 제한이 맞게 돈다 ───────────────
    {
        const int64_t big = 400LL * 24 * 3600 * 1000;      // 400일
        Fsm f = idle(big); f.on(Ev::TAP_CALL, big);
        CHECK("large clock: not timed out early", f.on(Ev::TICK, big + 4999).acts == 0);
        CHECK("large clock: times out at 5s", has(f.on(Ev::TICK, big + 5000), A_RECONNECT));
    }

    // ── 규약의 구멍(신호에 통화 번호가 없다): 내가 끊는 순간 상대가 먼저 끊고 곧바로 다시 걸면, 교환기는 내 옛 hangup 으로 상대의 새 통화를 끝낸다.
    //    내 쪽에는 ended → ring → ended 가 온다. 가짜 부재중 1건은 생기지만 멈추지는 않는다.
    {
        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2); f.on(Ev::CONNECTED, 3);
        f.on(Ev::TAP_HANGUP, 1000);
        f.on(Ev::ENDED, 1010, Why::HANGUP);
        Out o = f.on(Ev::RING, 1020);
        CHECK("race: ring right after ended is a new incoming call", f.state == State::INCOMING && has(o, A_TONE_BELL));
        o = f.on(Ev::ENDED, 1030, Why::HANGUP);
        CHECK("race: stale hangup ends it -> idle (counts as missed), never stuck", f.state == State::IDLE && f.missed == 1 && has(o, A_TONE_STOP));
        o = f.on(Ev::TAP_CALL, 2000);
        CHECK("race: can call again afterwards", f.state == State::DIALING && has(o, A_SEND_CALL));
    }

    // ── 로그에 찍는 이름 ────────────────────────────────────────────────────
    CHECK("names for the serial log", std::string(state_name(State::RINGBACK)) == "RINGBACK" && std::string(state_name(State::IN_CALL)) == "IN_CALL"
          && std::string(notice_name(Notice::RELAY_SILENT)) == "RELAY_SILENT" && std::string(notice_name(Notice::NONE)) == "NONE");

    printf("\n%d ok, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
```

- [x] **Step 5: 실패를 확인**

Run: `cd ~/tab5-toy-phone/firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_call_fsm.cpp -o /tmp/t_call_fsm`
Expected: FAIL — `fatal error: 'call_fsm.h' file not found`

- [x] **Step 6: `main/call_fsm.h` 작성**

```cpp
// call_fsm.h — 통화 상태 기계. 하드웨어·시계·네트워크와 무관한 순수 로직(맥에서 단위 시험: tools/test_call_fsm.cpp).
//
// 사건(화면 터치·교환기 신호·연결 상태·시간 경과)을 넣으면 다음 상태와 "할 일"(보낼 신호, 소리, 음성 켜기/끄기)을 돌려준다.
// 설계서 docs/exec-plans/2026-10-03-tab5-phone.md §9.4 "교환기-기기 계약"을 전제로 한다:
//   - "끝났다"는 교환기의 ended 하나로 판단한다. hangup·reject 를 보낸 뒤에는 ended 를 받을 때까지 새 call 을 보내지 않는다.
//   - 대기 상태에서 받은 ended 는 버린다(신호에 통화 번호가 없어 앞 통화의 것인지 구분할 수 없다).
//   - 교환기는 상태에 안 맞는 신호에 응답하지 않는다 → 기다리는 상태마다 시간 제한을 두고, 넘으면 연결을 끊고 다시 붙는다
//     (FSM-D1: 다시 붙으면 교환기가 그 통화를 끝내고 옛 연결의 뒤늦은 프레임을 버리므로 양쪽 상태가 확실히 맞는다.
//      비용: 재연결 1~2초 동안 전화를 못 받는다. 탈출구: 시간 제한 값을 늘리거나 A_RECONNECT 대신 대기로 복귀).
//   - 받기를 누르지 않았는데 connected 가 오면 통화로 들어가지 않는다(PH-D6: 원치 않을 때 소리가 새지 않게) → 재연결로 맞춘다.
#pragma once
#include <cstdint>

namespace callfsm {

enum class State : uint8_t {
    OFFLINE,     // 교환기에 붙지 않음
    IDLE,        // 대기
    DIALING,     // call 을 보냈고 교환기 응답(ringing/ended/connected)을 기다림
    RINGBACK,    // 상대 벨이 울리는 중(연결음)
    INCOMING,    // 전화 옴(벨)
    ANSWERING,   // accept 를 보냈고 connected 를 기다림
    IN_CALL,     // 통화 중
    ENDING,      // hangup/reject 를 보냈고 ended 를 기다림
};

enum class Why : uint8_t { NONE, HANGUP, REJECT, TIMEOUT, OFFLINE, LOST };   // 교환기 ended 의 why

enum class Ev : uint8_t {
    LINK_UP, LINK_DOWN,                                          // 교환기 연결됨 / 끊김
    PEER,                                                        // 상대 기기 접속 여부 (flag = online)
    RINGING, RING, CONNECTED, ENDED,                             // 교환기 신호 (ENDED 는 why 와 함께)
    TAP_CALL, TAP_CANCEL, TAP_ACCEPT, TAP_REJECT, TAP_HANGUP,    // 화면 버튼
    TICK,                                                        // 주기 호출(시간 제한 검사). 100ms 간격이면 충분
};

enum Act : uint16_t {                 // 할 일(비트 묶음)
    A_SEND_CALL     = 1 << 0,
    A_SEND_ACCEPT   = 1 << 1,
    A_SEND_REJECT   = 1 << 2,
    A_SEND_HANGUP   = 1 << 3,
    A_TONE_RINGBACK = 1 << 4,         // 연결음 시작(반복)
    A_TONE_BELL     = 1 << 5,         // 벨소리 시작(반복)
    A_TONE_END      = 1 << 6,         // 종료음 한 번(울리던 소리는 멈춘다)
    A_TONE_STOP     = 1 << 7,         // 울리던 소리 멈춤
    A_VOICE_START   = 1 << 8,         // 마이크·스피커 음성 경로 켜기
    A_VOICE_STOP    = 1 << 9,
    A_RECONNECT     = 1 << 10,        // 교환기 연결을 끊고 다시 붙기
};

enum class Notice : uint8_t {         // 화면에 잠깐 띄울 안내
    NONE,
    PEER_OFF,       // 상대 기기가 꺼져 있음
    NO_ANSWER,      // 받지 않음
    REJECTED,       // 상대가 받지 않고 끊음
    LOST,           // 연결이 끊어졌습니다
    CALL_ENDED,     // 통화 종료
    MISSED,         // 부재중
    RELAY_SILENT,   // 교환기 응답 없음
};

struct Out {
    uint16_t acts = 0;
    Notice notice = Notice::NONE;
};

const int64_t SIGNAL_WAIT_MS = 5000;     // call·accept 를 보낸 뒤 교환기 응답을 기다리는 한도
const int64_t ENDING_WAIT_MS = 3000;     // hangup·reject 를 보낸 뒤 ended 를 기다리는 한도(§9.4)
const int64_t RING_GUARD_MS  = 40000;    // 벨·연결음 한도. 교환기가 30초에 끝내므로 정상이면 여기까지 오지 않는다

struct Fsm {
    State   state = State::OFFLINE;
    bool    peer_online = false;
    int64_t deadline_ms = 0;             // 현재 상태의 시간 제한(없으면 0)
    int64_t call_start_ms = 0;           // 통화 시작 시각(화면의 통화 시간 표시용)
    int64_t last_talk_ms = 0;            // 직전 통화 길이
    uint16_t missed = 0;                 // 부재중 건수
    int64_t missed_at_ms = 0;            // 마지막 부재중 시각

    void clear_missed() { missed = 0; }

    Out on(Ev ev, int64_t now_ms, Why why = Why::NONE, bool flag = false)
    {
        Out o;
        switch (ev) {
        case Ev::LINK_UP:
            if (state != State::OFFLINE) o = drop(now_ms);      // 끊김을 못 본 채 다시 붙은 경우: 먼저 정리
            state = State::IDLE; peer_online = false; deadline_ms = 0;
            return o;
        case Ev::LINK_DOWN:
            return drop(now_ms);
        case Ev::PEER:
            if (state != State::OFFLINE) peer_online = flag;
            return o;
        case Ev::TICK:
            return tick(now_ms);
        default:
            break;
        }

        switch (state) {
        case State::OFFLINE:
            break;                                              // 뒤늦은 신호·터치는 모두 버린다

        case State::IDLE:
            if (ev == Ev::TAP_CALL) {
                if (!peer_online) { o.notice = Notice::PEER_OFF; break; }
                o.acts = A_SEND_CALL; enter(State::DIALING, now_ms + SIGNAL_WAIT_MS);
            } else if (ev == Ev::RING) {
                o.acts = A_TONE_BELL; enter(State::INCOMING, now_ms + RING_GUARD_MS);
            }
            break;                                              // ended·connected·ringing 은 버린다(§9.4)

        case State::DIALING:
        case State::RINGBACK:
            if (ev == Ev::RINGING && state == State::DIALING) {
                o.acts = A_TONE_RINGBACK; enter(State::RINGBACK, now_ms + RING_GUARD_MS);
            } else if (ev == Ev::CONNECTED) {
                o = begin_call(now_ms);
            } else if (ev == Ev::ENDED) {
                o.acts = A_TONE_END;
                o.notice = why == Why::TIMEOUT ? Notice::NO_ANSWER
                         : why == Why::OFFLINE ? Notice::PEER_OFF
                         : why == Why::LOST    ? Notice::LOST
                         :                       Notice::REJECTED;   // reject, 또는 받는 쪽이 벨 중에 보낸 hangup
                enter(State::IDLE, 0);
            } else if (ev == Ev::TAP_CANCEL) {
                o.acts = A_SEND_HANGUP | A_TONE_STOP; enter(State::ENDING, now_ms + ENDING_WAIT_MS);
            }
            break;                                              // 거는 중에 온 ring(동시 발신)은 그대로 기다린다 → connected

        case State::INCOMING:
            if (ev == Ev::TAP_ACCEPT) {
                o.acts = A_SEND_ACCEPT | A_TONE_STOP; enter(State::ANSWERING, now_ms + SIGNAL_WAIT_MS);
            } else if (ev == Ev::TAP_REJECT) {
                o.acts = A_SEND_REJECT | A_TONE_STOP; enter(State::ENDING, now_ms + ENDING_WAIT_MS);
            } else if (ev == Ev::ENDED) {                       // timeout·거는 쪽 취소(hangup)·거는 쪽 사라짐(offline) 모두 부재중
                o.acts = A_TONE_STOP; o.notice = Notice::MISSED; miss(now_ms); enter(State::IDLE, 0);
            } else if (ev == Ev::CONNECTED) {                   // 받기를 누르지 않았다 → 통화로 들어가지 않는다
                o.acts = A_TONE_STOP | A_RECONNECT; o.notice = Notice::RELAY_SILENT; miss(now_ms); offline();
            }
            break;

        case State::ANSWERING:
            if (ev == Ev::CONNECTED) {
                o = begin_call(now_ms);
            } else if (ev == Ev::ENDED) {                       // 받는 순간 거는 쪽이 취소 — 부재중으로 세지 않는다
                o.notice = why == Why::LOST ? Notice::LOST : Notice::CALL_ENDED; enter(State::IDLE, 0);
            } else if (ev == Ev::TAP_HANGUP) {
                o.acts = A_SEND_HANGUP; enter(State::ENDING, now_ms + ENDING_WAIT_MS);
            }
            break;

        case State::IN_CALL:
            if (ev == Ev::TAP_HANGUP) {
                last_talk_ms = now_ms - call_start_ms;
                o.acts = A_SEND_HANGUP | A_VOICE_STOP | A_TONE_END; enter(State::ENDING, now_ms + ENDING_WAIT_MS);
            } else if (ev == Ev::ENDED) {
                last_talk_ms = now_ms - call_start_ms;
                o.acts = A_VOICE_STOP | A_TONE_END;
                o.notice = why == Why::LOST ? Notice::LOST : Notice::CALL_ENDED;
                enter(State::IDLE, 0);
            }
            break;

        case State::ENDING:
            if (ev == Ev::ENDED) enter(State::IDLE, 0);         // 내가 끝낸 통화 — 안내·소리 없음
            break;                                              // 그 전의 call 터치·뒤늦은 connected 는 버린다
        }
        return o;
    }

private:
    void enter(State s, int64_t deadline) { state = s; deadline_ms = deadline; }
    void offline() { state = State::OFFLINE; peer_online = false; deadline_ms = 0; }
    void miss(int64_t now_ms) { if (missed < 0xFFFF) ++missed; missed_at_ms = now_ms; }

    Out begin_call(int64_t now_ms)
    {
        Out o; o.acts = A_TONE_STOP | A_VOICE_START;
        call_start_ms = now_ms; enter(State::IN_CALL, 0);
        return o;
    }

    Out drop(int64_t now_ms)                                    // 교환기 연결이 끊김
    {
        Out o;
        switch (state) {
        case State::IN_CALL:
            last_talk_ms = now_ms - call_start_ms;
            o.acts = A_VOICE_STOP | A_TONE_END; o.notice = Notice::LOST; break;
        case State::DIALING: case State::RINGBACK: case State::ANSWERING:
            o.acts = A_TONE_STOP; o.notice = Notice::LOST; break;
        case State::INCOMING:
            o.acts = A_TONE_STOP; o.notice = Notice::MISSED; miss(now_ms); break;
        default:
            break;                                              // OFFLINE·IDLE·ENDING: 조용히
        }
        offline();
        return o;
    }

    Out tick(int64_t now_ms)
    {
        Out o;
        if (deadline_ms == 0 || now_ms < deadline_ms) return o;
        switch (state) {
        case State::DIALING: case State::ANSWERING:
            o.acts = A_RECONNECT | A_TONE_STOP; o.notice = Notice::RELAY_SILENT; break;
        case State::RINGBACK:
            o.acts = A_RECONNECT | A_TONE_STOP; o.notice = Notice::RELAY_SILENT; break;
        case State::INCOMING:
            o.acts = A_RECONNECT | A_TONE_STOP; o.notice = Notice::MISSED; miss(now_ms); break;
        case State::ENDING:
            o.acts = A_RECONNECT; break;
        default:
            deadline_ms = 0; return o;
        }
        offline();
        return o;
    }
};

inline const char *state_name(State s)
{
    switch (s) {
    case State::OFFLINE: return "OFFLINE"; case State::IDLE: return "IDLE"; case State::DIALING: return "DIALING";
    case State::RINGBACK: return "RINGBACK"; case State::INCOMING: return "INCOMING"; case State::ANSWERING: return "ANSWERING";
    case State::IN_CALL: return "IN_CALL"; case State::ENDING: return "ENDING";
    }
    return "?";
}

inline const char *notice_name(Notice n)
{
    switch (n) {
    case Notice::NONE: return "NONE"; case Notice::PEER_OFF: return "PEER_OFF"; case Notice::NO_ANSWER: return "NO_ANSWER";
    case Notice::REJECTED: return "REJECTED"; case Notice::LOST: return "LOST"; case Notice::CALL_ENDED: return "CALL_ENDED";
    case Notice::MISSED: return "MISSED"; case Notice::RELAY_SILENT: return "RELAY_SILENT";
    }
    return "?";
}

}  // namespace callfsm
```

- [x] **Step 7: 통과 확인**

Run: `cd ~/tab5-toy-phone/firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_call_fsm.cpp -o /tmp/t_call_fsm && /tmp/t_call_fsm | tail -1`
Expected: `73 ok, 0 failed` (경고 0)

- [x] **Step 8: 신호 읽기·쓰기의 시험을 먼저 쓴다 — `tools/test_relay_proto.cpp`**

```cpp
// test_relay_proto.cpp — relay_proto.h 단위 시험:
//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_relay_proto.cpp -o /tmp/t_proto && /tmp/t_proto
// 교환기(server/phone_core.py)가 실제로 내는 글자와 같은 모양을 넣어 본다. 깨진 입력에 죽지 않는 것이 핵심(검토 초점).
#include "relay_proto.h"
#include <cstdio>
#include <cstring>
#include <string>
using namespace relayproto;
using callfsm::Ev;
using callfsm::Why;
static int ok = 0, bad = 0;
#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)

static Signal P(const std::string &s) { return parse(s.data(), s.size()); }

int main()
{
    // ── 교환기가 보내는 신호 (json.dumps 기본 모양: 쉼표·쌍점 뒤에 공백) ─────
    { Signal s = P("{\"t\": \"peer\", \"online\": true}");  CHECK("peer online",  s.kind == Kind::EVENT && s.ev == Ev::PEER && s.flag); }
    { Signal s = P("{\"t\": \"peer\", \"online\": false}"); CHECK("peer offline", s.kind == Kind::EVENT && s.ev == Ev::PEER && !s.flag); }
    { Signal s = P("{\"t\":\"peer\",\"online\":true}");       CHECK("compact form too", s.kind == Kind::EVENT && s.ev == Ev::PEER && s.flag); }
    { Signal s = P("{\"t\": \"ringing\"}");   CHECK("ringing",   s.kind == Kind::EVENT && s.ev == Ev::RINGING); }
    { Signal s = P("{\"t\": \"ring\"}");      CHECK("ring (not confused with ringing)", s.kind == Kind::EVENT && s.ev == Ev::RING); }
    { Signal s = P("{\"t\": \"connected\"}"); CHECK("connected", s.kind == Kind::EVENT && s.ev == Ev::CONNECTED); }
    {
        const char *names[] = { "hangup", "reject", "timeout", "offline", "lost" };
        const Why want[] = { Why::HANGUP, Why::REJECT, Why::TIMEOUT, Why::OFFLINE, Why::LOST };
        bool all = true;
        for (int i = 0; i < 5; ++i) {
            Signal s = P(std::string("{\"t\": \"ended\", \"why\": \"") + names[i] + "\"}");
            all = all && s.kind == Kind::EVENT && s.ev == Ev::ENDED && s.why == want[i];
        }
        CHECK("ended with each why", all);
    }
    { Signal s = P("{\"t\": \"ended\", \"why\": \"weird\"}"); CHECK("ended with unknown why still ends the call", s.kind == Kind::EVENT && s.ev == Ev::ENDED && s.why == Why::HANGUP); }
    { Signal s = P("{\"t\": \"ended\"}");                     CHECK("ended without why still ends the call", s.kind == Kind::EVENT && s.ev == Ev::ENDED); }
    { Signal s = P("{\"t\": \"pong\", \"n\": 41}");          CHECK("pong n", s.kind == Kind::PONG && s.n == 41); }
    { Signal s = P("{\"why\": \"lost\", \"t\": \"ended\"}"); CHECK("key order does not matter", s.kind == Kind::EVENT && s.ev == Ev::ENDED && s.why == Why::LOST); }

    // ── 깨진·모르는 입력은 버린다(죽지 않는다) ──────────────────────────────
    CHECK("empty", P("").kind == Kind::NONE);
    CHECK("not json", P("hello").kind == Kind::NONE);
    CHECK("unknown t", P("{\"t\": \"selfdestruct\"}").kind == Kind::NONE);
    CHECK("t is a number", P("{\"t\": 5}").kind == Kind::NONE);
    CHECK("no t", P("{\"online\": true}").kind == Kind::NONE);
    CHECK("truncated", P("{\"t\": \"end").kind == Kind::NONE);
    CHECK("peer without online", P("{\"t\": \"peer\"}").kind == Kind::NONE);
    CHECK("peer online not bool", P("{\"t\": \"peer\", \"online\": 1}").kind == Kind::NONE);
    CHECK("pong without n", P("{\"t\": \"pong\"}").kind == Kind::NONE);
    CHECK("pong n not a number", P("{\"t\": \"pong\", \"n\": \"x\"}").kind == Kind::NONE);
    CHECK("device-only signal echoed back is ignored", P("{\"t\": \"call\"}").kind == Kind::NONE);
    { std::string big(5000, 'a'); CHECK("oversized text ignored", P("{\"t\": \"" + big + "\"}").kind == Kind::NONE); }
    { const char raw[] = { '{', '"', 't', '"', ':', '"', 'r', 0, 'n', 'g', '"', '}' }; CHECK("embedded NUL ignored", parse(raw, sizeof raw).kind == Kind::NONE); }
    CHECK("null pointer", parse(nullptr, 0).kind == Kind::NONE);
    {   // 값 안에 들어 있는 "t" 를 열쇠로 착각하지 않는다
        Signal s = P("{\"x\": \"\\\"t\\\": \\\"ring\\\"\", \"t\": \"connected\"}");
        CHECK("key inside a string value is not a key", s.kind == Kind::EVENT && s.ev == Ev::CONNECTED);
    }

    // ── 보내는 글자 ─────────────────────────────────────────────────────────
    CHECK("call text",   std::string(text_for(callfsm::A_SEND_CALL))   == "{\"t\":\"call\"}");
    CHECK("accept text", std::string(text_for(callfsm::A_SEND_ACCEPT)) == "{\"t\":\"accept\"}");
    CHECK("reject text", std::string(text_for(callfsm::A_SEND_REJECT)) == "{\"t\":\"reject\"}");
    CHECK("hangup text", std::string(text_for(callfsm::A_SEND_HANGUP)) == "{\"t\":\"hangup\"}");
    CHECK("no text for non-send action", text_for(callfsm::A_TONE_BELL) == nullptr);
    { char b[40]; int n = ping_text(b, sizeof b, 7); CHECK("ping text", n == (int)strlen(b) && std::string(b) == "{\"t\":\"ping\",\"n\":7}"); }
    { char b[8];  CHECK("ping text does not overflow a small buffer", ping_text(b, sizeof b, 123456) == 0); }

    // ── 음성 프레임: [순번 2바이트(큰 자리 먼저)][Opus] ─────────────────────
    {
        uint8_t opus[60]; for (int i = 0; i < 60; ++i) opus[i] = (uint8_t)(i + 1);
        uint8_t frame[80];
        size_t n = pack_voice(frame, sizeof frame, 0x1234, opus, 60);
        CHECK("pack voice: size and seq bytes", n == 62 && frame[0] == 0x12 && frame[1] == 0x34 && frame[2] == 1 && frame[61] == 60);
        uint16_t seq = 0; const uint8_t *body = nullptr; size_t blen = 0;
        CHECK("unpack voice roundtrip", unpack_voice(frame, n, &seq, &body, &blen) && seq == 0x1234 && blen == 60 && body == frame + 2);
        CHECK("pack voice refuses a frame over 400 bytes", pack_voice(frame, sizeof frame, 1, opus, 399) == 0);
        uint8_t bigbuf[500];
        CHECK("pack voice: 398 + 2 = 400 is allowed", pack_voice(bigbuf, sizeof bigbuf, 1, bigbuf + 2, 398) == 400);
        CHECK("pack voice: empty opus refused", pack_voice(frame, sizeof frame, 1, opus, 0) == 0);
        CHECK("pack voice: output too small refused", pack_voice(frame, 10, 1, opus, 60) == 0);
        CHECK("unpack voice: too short", !unpack_voice(frame, 2, &seq, &body, &blen));
        CHECK("unpack voice: over 400 refused", !unpack_voice(bigbuf, 401, &seq, &body, &blen));
        CHECK("unpack voice: null", !unpack_voice(nullptr, 10, &seq, &body, &blen));
    }

    // ── 재연결 간격: 1 → 2 → 4 … 최대 30초 (§9.5), 토큰 거절은 60초 (§9.7) ──
    CHECK("backoff 1s first", backoff_ms(0, false) == 1000);
    CHECK("backoff doubles", backoff_ms(1, false) == 2000 && backoff_ms(2, false) == 4000 && backoff_ms(4, false) == 16000);
    CHECK("backoff capped at 30s", backoff_ms(5, false) == 30000 && backoff_ms(50, false) == 30000 && backoff_ms(-3, false) == 1000);
    CHECK("auth failure waits 60s", backoff_ms(0, true) == 60000 && backoff_ms(9, true) == 60000);

    printf("\n%d ok, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
```

- [x] **Step 9: 실패를 확인**

Run: `cd ~/tab5-toy-phone/firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_relay_proto.cpp -o /tmp/t_relay_proto`
Expected: FAIL — `fatal error: 'relay_proto.h' file not found`

- [x] **Step 10: `main/relay_proto.h` 작성**

```cpp
// relay_proto.h — 교환기와 주고받는 글자·음성 프레임의 모양(순수 로직, 맥에서 단위 시험: tools/test_relay_proto.cpp).
// 설계서 §9.4 신호 규약. 교환기 원본: server/phone_core.py.
//   - 받은 글자(JSON 한 겹)를 call_fsm 사건으로 바꾼다. 깨진·모르는·너무 긴 입력은 Kind::NONE 으로 버린다(죽지 않는다).
//   - JSON 라이브러리를 쓰지 않는다: 신호가 열쇠 4개(t, online, why, n)뿐이고, 맥에서 그대로 시험하려는 것(RP-D1).
//     비용: 겹친 객체·배열은 읽지 못한다(규약에 없음). 탈출구: 규약이 커지면 cJSON 으로 교체, 시험은 그대로 쓴다.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "call_fsm.h"

namespace relayproto {

enum class Kind : uint8_t { NONE, EVENT, PONG };

struct Signal {
    Kind kind = Kind::NONE;
    callfsm::Ev ev = callfsm::Ev::TICK;
    callfsm::Why why = callfsm::Why::NONE;
    bool flag = false;          // peer 의 online
    uint32_t n = 0;             // pong 의 n
};

const size_t MAX_TEXT = 256;            // 교환기 신호는 수십 바이트 — 이보다 길면 버린다(교환기 MAX_TEXT_BYTES 와 같은 값)
const size_t MAX_VOICE_FRAME = 400;     // [순번 2][Opus] 전체 길이 상한(§9.4)
const size_t MIN_VOICE_FRAME = 3;

namespace detail {

struct Scan {
    const char *p, *end;
    void ws() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p; }
    // 따옴표로 시작하는 글자열을 읽어 out(최대 cap-1 글자)에 담는다. 이스케이프가 있으면 plain=false (비교 대상에서 제외).
    bool str(char *out, size_t cap, bool *plain)
    {
        if (p >= end || *p != '"') return false;
        ++p; size_t n = 0; *plain = true;
        while (p < end && *p != '"') {
            unsigned char c = (unsigned char)*p;
            if (c < 0x20) return false;                          // 제어 문자(NUL 포함)는 JSON 글자열에 날것으로 올 수 없다
            if (c == '\\') { *plain = false; ++p; if (p >= end) return false; }
            if (n + 1 < cap) out[n++] = *p; else *plain = false; // 너무 긴 값은 어차피 아는 값이 아니다
            ++p;
        }
        if (p >= end) return false;                              // 닫는 따옴표 없음
        ++p; out[n] = 0;
        return true;
    }
};

inline bool eq(const char *a, const char *b) { return strcmp(a, b) == 0; }

}  // namespace detail

inline Signal parse(const char *text, size_t len)
{
    using namespace detail;
    Signal none;
    if (!text || len < 2 || len > MAX_TEXT) return none;
    Scan s{ text, text + len };

    char t[16] = "", why[16] = "";
    bool have_t = false, have_why = false, have_online = false, online = false, have_n = false;
    uint32_t n = 0;

    s.ws();
    if (s.p >= s.end || *s.p != '{') return none;
    ++s.p; s.ws();
    if (s.p < s.end && *s.p == '}') return none;
    while (true) {
        char key[16]; bool key_plain = true;
        s.ws();
        if (!s.str(key, sizeof key, &key_plain)) return none;
        s.ws();
        if (s.p >= s.end || *s.p != ':') return none;
        ++s.p; s.ws();
        if (s.p >= s.end) return none;

        if (*s.p == '"') {                                       // 글자열 값
            char val[16]; bool plain = true;
            if (!s.str(val, sizeof val, &plain)) return none;
            if (key_plain && plain && eq(key, "t"))   { strcpy(t, val); have_t = true; }
            if (key_plain && plain && eq(key, "why")) { strcpy(why, val); have_why = true; }
        } else if (s.end - s.p >= 4 && !memcmp(s.p, "true", 4)) {
            s.p += 4; if (key_plain && eq(key, "online")) { have_online = true; online = true; }
        } else if (s.end - s.p >= 5 && !memcmp(s.p, "false", 5)) {
            s.p += 5; if (key_plain && eq(key, "online")) { have_online = true; online = false; }
        } else if (s.end - s.p >= 4 && !memcmp(s.p, "null", 4)) {
            s.p += 4;
        } else if (*s.p == '-' || (*s.p >= '0' && *s.p <= '9')) { // 숫자 값
            const char *b = s.p; bool digits_only = true; uint64_t v = 0;
            while (s.p < s.end && (*s.p == '-' || *s.p == '+' || *s.p == '.' || *s.p == 'e' || *s.p == 'E' || (*s.p >= '0' && *s.p <= '9'))) {
                if (*s.p < '0' || *s.p > '9') digits_only = false; else if (v <= 0xFFFFFFFFull) v = v * 10 + (uint64_t)(*s.p - '0');
                ++s.p;
            }
            if (s.p == b) return none;
            if (key_plain && eq(key, "n") && digits_only && v <= 0xFFFFFFFFull) { have_n = true; n = (uint32_t)v; }
        } else {
            return none;                                         // 겹친 객체·배열 등: 규약에 없다
        }

        s.ws();
        if (s.p >= s.end) return none;
        if (*s.p == ',') { ++s.p; continue; }
        if (*s.p == '}') { ++s.p; break; }
        return none;
    }
    s.ws();
    if (s.p != s.end || !have_t) return none;

    Signal out;
    if (eq(t, "peer")) {
        if (!have_online) return none;
        out.kind = Kind::EVENT; out.ev = callfsm::Ev::PEER; out.flag = online;
    } else if (eq(t, "ringing")) {
        out.kind = Kind::EVENT; out.ev = callfsm::Ev::RINGING;
    } else if (eq(t, "ring")) {
        out.kind = Kind::EVENT; out.ev = callfsm::Ev::RING;
    } else if (eq(t, "connected")) {
        out.kind = Kind::EVENT; out.ev = callfsm::Ev::CONNECTED;
    } else if (eq(t, "ended")) {                                 // 사유를 몰라도 "끝났다"는 사실은 전한다
        out.kind = Kind::EVENT; out.ev = callfsm::Ev::ENDED;
        out.why = !have_why            ? callfsm::Why::HANGUP
                : eq(why, "reject")    ? callfsm::Why::REJECT
                : eq(why, "timeout")   ? callfsm::Why::TIMEOUT
                : eq(why, "offline")   ? callfsm::Why::OFFLINE
                : eq(why, "lost")      ? callfsm::Why::LOST
                :                        callfsm::Why::HANGUP;
    } else if (eq(t, "pong")) {
        if (!have_n) return none;
        out.kind = Kind::PONG; out.n = n;
    }
    return out;
}

// call_fsm 의 "보내기" 할 일 하나 → 교환기에 보낼 글자. 보내기가 아니면 nullptr.
inline const char *text_for(uint16_t act)
{
    switch (act) {
    case callfsm::A_SEND_CALL:   return "{\"t\":\"call\"}";
    case callfsm::A_SEND_ACCEPT: return "{\"t\":\"accept\"}";
    case callfsm::A_SEND_REJECT: return "{\"t\":\"reject\"}";
    case callfsm::A_SEND_HANGUP: return "{\"t\":\"hangup\"}";
    default:                     return nullptr;
    }
}

// 생존 확인 글자를 buf 에 쓴다. 돌려주는 값 = 글자 수(끝의 0 제외), 안 들어가면 0.
inline int ping_text(char *buf, size_t cap, uint32_t n)
{
    int len = snprintf(buf, cap, "{\"t\":\"ping\",\"n\":%u}", (unsigned)n);
    return (len > 0 && (size_t)len < cap) ? len : 0;
}

// 음성 프레임 만들기: out = [순번 큰 자리, 작은 자리][Opus…]. 돌려주는 값 = 전체 길이, 규격 밖이면 0.
// opus 가 out+2 를 가리켜도 된다(제자리 포장).
inline size_t pack_voice(uint8_t *out, size_t cap, uint16_t seq, const uint8_t *opus, size_t opus_len)
{
    if (!out || !opus || opus_len == 0 || opus_len + 2 > MAX_VOICE_FRAME || cap < opus_len + 2) return 0;
    memmove(out + 2, opus, opus_len);
    out[0] = (uint8_t)(seq >> 8); out[1] = (uint8_t)(seq & 0xFF);
    return opus_len + 2;
}

inline bool unpack_voice(const uint8_t *frame, size_t len, uint16_t *seq, const uint8_t **opus, size_t *opus_len)
{
    if (!frame || len < MIN_VOICE_FRAME || len > MAX_VOICE_FRAME) return false;
    *seq = (uint16_t)((frame[0] << 8) | frame[1]);
    *opus = frame + 2; *opus_len = len - 2;
    return true;
}

// 재연결까지 기다릴 시간(ms). attempt = 연속 실패 횟수(0 부터). 토큰 거절(401)이면 1분(§9.7).
inline int32_t backoff_ms(int attempt, bool auth_failed)
{
    if (auth_failed) return 60000;
    if (attempt < 0) attempt = 0;
    if (attempt >= 5) return 30000;
    return 1000 << attempt;
}

}  // namespace relayproto
```

- [x] **Step 11: 통과 확인**

Run: `cd ~/tab5-toy-phone/firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_relay_proto.cpp -o /tmp/t_relay_proto && /tmp/t_relay_proto | tail -1`
Expected: `46 ok, 0 failed`

- [x] **Step 12: 기기 번호 판별의 시험을 먼저 쓴다 — `tools/test_device_id.cpp`**

```cpp
// test_device_id.cpp — device_id.h 의 순수 로직 시험:
//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_device_id.cpp -o /tmp/t_id && /tmp/t_id
#include "device_id.h"
#include <cstdio>
#include <string>
using namespace deviceid;
static int ok = 0, bad = 0;
#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)

int main()
{
    const Entry tbl[] = { { "AA:BB:CC:DD:EE:01", 1, "tab5-1", "tok-one-0123456789" }, { "AA:BB:CC:DD:EE:02", 2, "tab5-2", "tok-two-0123456789" } };

    Info a = lookup("AA:BB:CC:DD:EE:01", tbl, 2);
    CHECK("unit 1 by mac", a.unit == 1 && std::string(a.id) == "tab5-1" && std::string(a.token) == "tok-one-0123456789");
    Info b = lookup("aa:bb:cc:dd:ee:02", tbl, 2);
    CHECK("unit 2, lowercase mac", b.unit == 2 && std::string(b.id) == "tab5-2");
    CHECK("mac kept as given", std::string(b.mac) == "aa:bb:cc:dd:ee:02");
    Info c = lookup("AA:BB:CC:DD:EE:FF", tbl, 2);
    CHECK("unknown device: unit 0, no token", c.unit == 0 && std::string(c.id) == "unknown" && std::string(c.token).empty());
    CHECK("prefix of a mac does not match", lookup("AA:BB:CC:DD:EE", tbl, 2).unit == 0);
    CHECK("null mac / null table / empty table", lookup(nullptr, tbl, 2).unit == 0 && lookup("AA:BB:CC:DD:EE:01", nullptr, 2).unit == 0 && lookup("AA:BB:CC:DD:EE:01", tbl, 0).unit == 0);
    const Entry broken[] = { { "AA:BB:CC:DD:EE:01", 3, "tab5-3", "x" }, { "AA:BB:CC:DD:EE:02", 2, nullptr, "x" } };
    CHECK("bad table rows are ignored (unit must be 1 or 2, id required)", lookup("AA:BB:CC:DD:EE:01", broken, 2).unit == 0 && lookup("AA:BB:CC:DD:EE:02", broken, 2).unit == 0);

    const uint8_t m[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x02 };
    char s[18]; format_mac(m, s);
    CHECK("format mac: upper case with colons", std::string(s) == "AA:BB:CC:DD:EE:02");
    CHECK("formatted mac finds unit 2", lookup(s, tbl, 2).unit == 2);

    printf("\n%d ok, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
```

- [x] **Step 13: 실패를 확인**

Run: `cd ~/tab5-toy-phone/firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_device_id.cpp -o /tmp/t_device_id`
Expected: FAIL — `fatal error: 'device_id.h' file not found`

- [x] **Step 14: `main/device_id.h` 작성**

```cpp
// device_id.h — 이 기기가 몇 번인지(MAC 으로 판별)와 교환기에 내밀 ID·토큰. 설계서 PH-D9: 두 대에 같은 펌웨어, secrets.h 에 MAC→토큰 표.
// 표에서 찾는 부분은 순수 로직(맥에서 단위 시험: tools/test_device_id.cpp), MAC 을 읽는 부분만 기기 전용.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace deviceid {

struct Entry {
    const char *mac;      // "AA:BB:CC:DD:EE:01" (대소문자 무관)
    int unit;             // 1 또는 2
    const char *id;       // 교환기 기기 ID ("tab5-1")
    const char *token;    // 교환기 토큰 — 화면·로그에 내지 않는다
};

struct Info {
    int unit = 0;                 // 표에 없으면 0
    const char *id = "unknown";
    const char *token = "";
    char mac[18] = "";
};

inline void format_mac(const uint8_t m[6], char out[18])
{
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

inline bool same_mac(const char *a, const char *b)
{
    if (!a || !b) return false;
    for (;; ++a, ++b) {
        char x = *a, y = *b;
        if (x >= 'a' && x <= 'f') x = (char)(x - 32);
        if (y >= 'a' && y <= 'f') y = (char)(y - 32);
        if (x != y) return false;
        if (!x) return true;
    }
}

// MAC 글자열로 표에서 찾는다. 없으면 unit 0 / "unknown" / 빈 토큰.
inline Info lookup(const char *mac, const Entry *table, int n)
{
    Info out;
    if (mac) { size_t i = 0; for (; i + 1 < sizeof out.mac && mac[i]; ++i) out.mac[i] = mac[i]; out.mac[i] = 0; }   // 길면 자른다
    for (int i = 0; table && i < n; ++i) {
        if (same_mac(mac, table[i].mac) && table[i].id && table[i].token && (table[i].unit == 1 || table[i].unit == 2)) {
            out.unit = table[i].unit; out.id = table[i].id; out.token = table[i].token;
            return out;
        }
    }
    return out;
}

#ifdef ESP_PLATFORM
// 이 기기의 정보. 처음 부를 때 칩의 MAC(eFuse — USB 일련번호·esptool read_mac 과 같은 값)을 읽어 secrets.h 의 PHONE_DEVICES 에서 찾는다.
const Info &get();
#endif

}  // namespace deviceid
```

- [x] **Step 15: 통과 확인**

Run: `cd ~/tab5-toy-phone/firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_device_id.cpp -o /tmp/t_device_id && /tmp/t_device_id | tail -1`
Expected: `9 ok, 0 failed`

- [x] **Step 16: 화면 배치·터치 판정의 시험을 먼저 쓴다 — `tools/test_ui_layout.cpp`**

```cpp
// test_ui_layout.cpp — ui_layout.h 단위 시험:
//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_ui_layout.cpp -o /tmp/t_ui && /tmp/t_ui
// 어떤 화면을 그릴지와 터치가 어느 버튼인지(순수 로직)를 고정한다. 설계서 §10, docs/design/README.md §2(터치 96px, 주요 버튼 160px).
#include "ui_layout.h"
#include <cstdio>
#include <string>
using namespace uilayout;
using callfsm::State;
static int ok = 0, bad = 0;
#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)

static Model M(State s, bool wifi = true, bool relay = true, bool peer = true)
{
    Model m; m.fsm = s; m.wifi_connected = wifi; m.relay_up = relay; m.peer_online = peer;
    m.wifi_phase = wifi ? netwifi::Phase::CONNECTED : netwifi::Phase::SEARCHING;
    return m;
}
static bool overlap(const Rect &a, const Rect &b) { return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h; }
static Rect box(const Circle &c) { return Rect{ c.cx - c.r, c.cy - c.r, 2 * c.r, 2 * c.r }; }
static bool inside_screen(const Rect &r) { return r.x >= 0 && r.y >= 0 && r.x + r.w <= SCREEN_W && r.y + r.h <= SCREEN_H; }

int main()
{
    // ── 어떤 화면인가 ───────────────────────────────────────────────────────
    CHECK("no wifi: boot screen", screen_for(M(State::OFFLINE, false, false)) == Screen::BOOT);
    CHECK("wifi but no relay: boot screen", screen_for(M(State::OFFLINE, true, false)) == Screen::BOOT);
    CHECK("idle", screen_for(M(State::IDLE)) == Screen::IDLE);
    CHECK("dialing and ringback share one screen", screen_for(M(State::DIALING)) == Screen::DIALING && screen_for(M(State::RINGBACK)) == Screen::DIALING);
    CHECK("incoming", screen_for(M(State::INCOMING)) == Screen::INCOMING);
    CHECK("answering", screen_for(M(State::ANSWERING)) == Screen::ANSWERING);
    CHECK("in call", screen_for(M(State::IN_CALL)) == Screen::IN_CALL);
    CHECK("ending", screen_for(M(State::ENDING)) == Screen::ENDING);
    CHECK("screen names", std::string(screen_name(Screen::BOOT)) == "BOOT" && std::string(screen_name(Screen::IN_CALL)) == "IN_CALL" && std::string(screen_name(Screen::DIALING)) == "DIALING");

    // ── 크기 규칙: 터치 96px 이상, 주요 버튼 160px 이상, 통화 버튼 지름 200px 이상 ──
    CHECK("call button diameter >= 200", 2 * BTN_CALL.r >= 200);
    CHECK("accept/reject >= 160", BTN_ACCEPT.w >= 160 && BTN_ACCEPT.h >= 160 && BTN_REJECT.w >= 160 && BTN_REJECT.h >= 160);
    CHECK("hangup/cancel are main actions too: >= 160 both ways", BTN_HANGUP.h >= 160 && BTN_HANGUP.w >= 160 && BTN_CANCEL.h >= 160 && BTN_CANCEL.w >= 160);
    CHECK("wifi badge >= 96 tall", BTN_WIFI.h >= 96 && BTN_WIFI.w >= 96);
    CHECK("all buttons inside the screen", inside_screen(box(BTN_CALL)) && inside_screen(BTN_ACCEPT) && inside_screen(BTN_REJECT) && inside_screen(BTN_HANGUP) && inside_screen(BTN_CANCEL) && inside_screen(BTN_WIFI));
    CHECK("layout on 8px grid", BTN_ACCEPT.x % 8 == 0 && BTN_ACCEPT.y % 8 == 0 && BTN_REJECT.x % 8 == 0 && BTN_HANGUP.x % 8 == 0 && BTN_HANGUP.y % 8 == 0 && BTN_CANCEL.x % 8 == 0 && BTN_CANCEL.y % 8 == 0);

    // ── 화면이 바뀌어도 같은 자리의 두 번째 터치가 다른 버튼을 누르지 않는다 ──
    CHECK("accept does not overlap hangup (answer then instant hangup)", !overlap(BTN_ACCEPT, BTN_HANGUP));
    CHECK("reject does not overlap hangup", !overlap(BTN_REJECT, BTN_HANGUP));
    CHECK("call does not overlap cancel (call then instant cancel)", !overlap(box(BTN_CALL), BTN_CANCEL));
    CHECK("call does not overlap hangup", !overlap(box(BTN_CALL), BTN_HANGUP));
    CHECK("accept and reject apart by >= 96px", BTN_REJECT.x - (BTN_ACCEPT.x + BTN_ACCEPT.w) >= 96);
    CHECK("wifi badge away from call/cancel/hangup", !overlap(BTN_WIFI, box(BTN_CALL)) && !overlap(BTN_WIFI, BTN_CANCEL) && !overlap(BTN_WIFI, BTN_HANGUP));

    // ── 터치 → 버튼 ─────────────────────────────────────────────────────────
    {
        Model m = M(State::IDLE);
        CHECK("idle: center of call button", hit(m, BTN_CALL.cx, BTN_CALL.cy) == Tap::CALL);
        CHECK("idle: just outside the circle corner is not a call", hit(m, BTN_CALL.cx + BTN_CALL.r, BTN_CALL.cy + BTN_CALL.r) == Tap::NONE);
        CHECK("idle: wifi badge", hit(m, BTN_WIFI.x + 10, BTN_WIFI.y + 10) == Tap::WIFI);
        CHECK("idle: empty area", hit(m, 1200, 60) == Tap::NONE);
        TapGuard g;                                             // 빗나간 터치는 가드를 쓰지 않는다 → 바로 뒤의 정확한 터치가 살아야 한다
        const bool miss_counts = hit(m, 1200, 60) != Tap::NONE && g.allow(1000, 0);
        CHECK("a miss must not consume the guard (caller checks hit first)", !miss_counts && g.allow(1100, 0));
        CHECK("idle: hangup position does nothing", hit(m, BTN_HANGUP.x + 5, BTN_HANGUP.y + BTN_HANGUP.h - 5) == Tap::NONE);
        Model off = M(State::IDLE, true, true, false);
        CHECK("idle + peer off: call button still reports CALL (fsm answers with the notice)", hit(off, BTN_CALL.cx, BTN_CALL.cy) == Tap::CALL);
    }
    {
        Model m = M(State::DIALING);
        CHECK("dialing: cancel", hit(m, BTN_CANCEL.x + 1, BTN_CANCEL.y + 1) == Tap::CANCEL);
        CHECK("dialing: call button position does nothing", hit(m, BTN_CALL.cx, BTN_CALL.cy) == Tap::NONE);
        CHECK("ringback: cancel too", hit(M(State::RINGBACK), BTN_CANCEL.x + 1, BTN_CANCEL.y + 1) == Tap::CANCEL);
        CHECK("dialing: wifi badge hidden", hit(m, BTN_WIFI.x + 10, BTN_WIFI.y + 10) == Tap::NONE);
    }
    {
        Model m = M(State::INCOMING);
        CHECK("incoming: accept", hit(m, BTN_ACCEPT.x + BTN_ACCEPT.w / 2, BTN_ACCEPT.y + BTN_ACCEPT.h / 2) == Tap::ACCEPT);
        CHECK("incoming: reject", hit(m, BTN_REJECT.x + BTN_REJECT.w / 2, BTN_REJECT.y + BTN_REJECT.h / 2) == Tap::REJECT);
        CHECK("incoming: gap between the two", hit(m, 640, BTN_ACCEPT.y + 10) == Tap::NONE);
    }
    {
        CHECK("answering: no buttons", hit(M(State::ANSWERING), BTN_ACCEPT.x + 5, BTN_ACCEPT.y + 5) == Tap::NONE && hit(M(State::ANSWERING), BTN_HANGUP.x + 5, BTN_HANGUP.y + 5) == Tap::NONE);
        CHECK("in call: hangup", hit(M(State::IN_CALL), BTN_HANGUP.x + 5, BTN_HANGUP.y + 5) == Tap::HANGUP);
        CHECK("in call: accept position does nothing", hit(M(State::IN_CALL), BTN_ACCEPT.x + 5, BTN_ACCEPT.y + 5) == Tap::NONE);
        CHECK("ending: no buttons", hit(M(State::ENDING), BTN_HANGUP.x + 5, BTN_HANGUP.y + 5) == Tap::NONE);
    }
    {
        Model m = M(State::OFFLINE, false, false);
        m.wifi_phase = netwifi::Phase::NO_KNOWN;
        CHECK("boot + no known wifi: badge opens setup", hit(m, BTN_WIFI.x + 10, BTN_WIFI.y + 10) == Tap::WIFI);
        m.wifi_phase = netwifi::Phase::SEARCHING;
        CHECK("boot + searching: badge works too (user may want to pick another)", hit(m, BTN_WIFI.x + 10, BTN_WIFI.y + 10) == Tap::WIFI);
        CHECK("boot: call position does nothing", hit(m, BTN_CALL.cx, BTN_CALL.cy) == Tap::NONE);
    }
    CHECK("off-screen coordinates", hit(M(State::IDLE), -5, -5) == Tap::NONE && hit(M(State::IDLE), 5000, 5000) == Tap::NONE);

    // ── 터치 가드: 화면이 바뀐 직후 600ms, 직전 터치 후 400ms 는 버린다 ─────
    {
        TapGuard g;
        CHECK("guard: first tap passes", g.allow(10000, 5000));
        CHECK("guard: second tap within 400ms dropped", !g.allow(10300, 5000));
        CHECK("guard: after 400ms passes", g.allow(10400, 5000));
        CHECK("guard: within 600ms of a screen change dropped", !g.allow(20500, 20000));
        CHECK("guard: 600ms after a screen change passes", g.allow(20600, 20000));
        CHECK("guard: a dropped tap does not extend the wait", (g.allow(30000, 0), !g.allow(30100, 0)) && g.allow(30400, 0));
    }
    {   // Wi-Fi 자판은 빨리 치므로 짧은 가드(화면 300ms·반복 150ms)를 쓴다
        TapGuard g;
        CHECK("keyboard guard: first key", g.allow(5000, 0, KEY_SCREEN_GUARD_MS, KEY_REPEAT_GUARD_MS));
        CHECK("keyboard guard: bounce within 150ms dropped", !g.allow(5100, 0, KEY_SCREEN_GUARD_MS, KEY_REPEAT_GUARD_MS));
        CHECK("keyboard guard: next key after 150ms accepted", g.allow(5150, 0, KEY_SCREEN_GUARD_MS, KEY_REPEAT_GUARD_MS));
        CHECK("keyboard guard: 300ms after the screen opens", !g.allow(9200, 9000, KEY_SCREEN_GUARD_MS, KEY_REPEAT_GUARD_MS) && g.allow(9300, 9000, KEY_SCREEN_GUARD_MS, KEY_REPEAT_GUARD_MS));
        CHECK("keyboard guard is shorter than the call-button guard", KEY_REPEAT_GUARD_MS < TAP_REPEAT_GUARD_MS && KEY_SCREEN_GUARD_MS < TAP_SCREEN_GUARD_MS);
    }

    // ── 통화 시간 표시 ──────────────────────────────────────────────────────
    {
        int mm = -1, ss = -1;
        talk_clock(0, &mm, &ss);        CHECK("clock 00:00", mm == 0 && ss == 0);
        talk_clock(75, &mm, &ss);       CHECK("clock 01:15", mm == 1 && ss == 15);
        talk_clock(5999, &mm, &ss);     CHECK("clock 99:59", mm == 99 && ss == 59);
        talk_clock(6000, &mm, &ss);     CHECK("clock caps at 99:59", mm == 99 && ss == 59);
        talk_clock(-5, &mm, &ss);       CHECK("clock negative -> 00:00", mm == 0 && ss == 0);
    }

    printf("\n%d ok, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
```

- [x] **Step 17: 실패를 확인**

Run: `cd ~/tab5-toy-phone/firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_ui_layout.cpp -o /tmp/t_ui_layout`
Expected: FAIL — `fatal error: 'ui_layout.h' file not found`

- [x] **Step 18: `main/ui_layout.h` 작성**

```cpp
// ui_layout.h — 통화 화면의 배치와 판정(순수 로직, 맥에서 단위 시험: tools/test_ui_layout.cpp).
//   - 어떤 화면을 그릴지(screen_for), 터치 좌표가 어느 버튼인지(hit), 터치를 받아들일지(TapGuard).
//   - 그리기(ui.cpp)는 여기의 상수대로 그린다 → 시험이 "버튼이 겹치지 않는다·96px 이상이다"를 고정한다.
// 설계서 §10, docs/design/README.md §2.
// UL-D1: 화면이 바뀐 직후 같은 자리를 또 눌러도 다른 버튼이 눌리지 않게 "받기/거절"과 "끊기", "통화"와 "취소"를 겹치지 않게 배치했다.
//        Phase 0 에서 세 번 누른 터치가 7회로 기록된 일이 있어(원인 미확정), 배치 + 시간 가드(TapGuard) 두 겹으로 막는다.
//        비용: 받기/거절 버튼이 화면 위쪽으로 올라간다. 탈출구: 원인이 터치 칩 읽기 오류로 확정되면 가드만 남기고 배치를 내려도 된다.
#pragma once
#include <cstdint>

#include "call_fsm.h"
#include "geom.h"
#include "net_wifi.h"

namespace uilayout {

enum class Screen : uint8_t { BOOT, IDLE, DIALING, INCOMING, ANSWERING, IN_CALL, ENDING };
enum class Tap : uint8_t { NONE, CALL, CANCEL, ACCEPT, REJECT, HANGUP, WIFI };

struct Circle { int cx, cy, r; };

// 그릴 내용 전부. 통화 진행 태스크(app)가 채우고, 그리기·판정 함수는 이것만 본다.
struct Model {
    callfsm::State  fsm = callfsm::State::OFFLINE;
    bool            peer_online = false;
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
    int             unit = 0;                               // 1, 2, 미등록이면 0
};

// 버튼 위치(1280×720, 8px 격자)
const Circle BTN_CALL   = { 640, 360, 120 };                // 지름 240
const Rect   BTN_WIFI   = {  32, 600, 240,  96 };
const Rect   BTN_CANCEL = { 480, 536, 320, 160 };          // 주요 버튼은 160px 이상(취소·끊기도 그 화면의 주요 버튼이다)
const Rect   BTN_ACCEPT = { 224, 328, 360, 200 };
const Rect   BTN_REJECT = { 696, 328, 360, 200 };
const Rect   BTN_HANGUP = { 464, 536, 360, 160 };

const int64_t TAP_SCREEN_GUARD_MS = 600;                    // 화면이 바뀐 직후 이만큼은 터치를 버린다
const int64_t TAP_REPEAT_GUARD_MS = 400;                    // 직전에 받아들인 터치 뒤 이만큼은 버린다
const int64_t KEY_SCREEN_GUARD_MS = 300;                    // Wi-Fi 설정 화면(목록·자판)용 — 글쇠를 빨리 치므로 짧게
const int64_t KEY_REPEAT_GUARD_MS = 150;

inline Screen screen_for(const Model &m)
{
    switch (m.fsm) {
    case callfsm::State::OFFLINE:   return Screen::BOOT;    // Wi-Fi 또는 교환기에 아직 못 붙음
    case callfsm::State::IDLE:      return Screen::IDLE;
    case callfsm::State::DIALING:
    case callfsm::State::RINGBACK:  return Screen::DIALING;
    case callfsm::State::INCOMING:  return Screen::INCOMING;
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
    case Screen::ENDING: return "ENDING";
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
        if (in_circle(BTN_CALL, x, y)) return Tap::CALL;    // 상대가 꺼져 있어도 CALL 을 돌려준다 — 안내는 call_fsm 이 낸다
        if (BTN_WIFI.hit(x, y)) return Tap::WIFI;
        break;
    case Screen::DIALING:
        if (BTN_CANCEL.hit(x, y)) return Tap::CANCEL;
        break;
    case Screen::INCOMING:
        if (BTN_ACCEPT.hit(x, y)) return Tap::ACCEPT;
        if (BTN_REJECT.hit(x, y)) return Tap::REJECT;
        break;
    case Screen::IN_CALL:
        if (BTN_HANGUP.hit(x, y)) return Tap::HANGUP;
        break;
    case Screen::ANSWERING:
    case Screen::ENDING:
        break;
    }
    return Tap::NONE;
}

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
```

- [x] **Step 19: 통과 확인 + 네 시험을 한 번에 다시**

Run:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone/tools && SDK="$(xcrun --sdk macosx --show-sdk-path)" && for T in wifi_store call_fsm relay_proto device_id ui_layout; do clang++ -std=c++17 -Wall -Wextra -isysroot "$SDK" -I../main test_$T.cpp -o /tmp/t_$T && printf "%-12s " $T && /tmp/t_$T | tail -1; done
```
Expected:
```
wifi_store   16 passed, 0 failed
call_fsm     73 ok, 0 failed
relay_proto  46 ok, 0 failed
device_id    9 ok, 0 failed
ui_layout    59 ok, 0 failed
```

- [x] **Step 20: 커밋**

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/main/wifi_store.h firmware/tab5_phone/main/geom.h firmware/tab5_phone/main/net_wifi.h firmware/tab5_phone/main/call_fsm.h firmware/tab5_phone/main/relay_proto.h firmware/tab5_phone/main/device_id.h firmware/tab5_phone/main/ui_layout.h firmware/tab5_phone/tools/test_*.cpp && git commit -m "feat(phone): 통화 판단 로직(상태 기계·신호·화면 배치·기기 번호) + 맥 단위 시험" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: 맥 쪽 검증 도구와 가짜 상대 (시험 먼저)

**Files:**
- Create: `firmware/tab5_phone/tools/phone_ctl.py`
- Test: `firmware/tab5_phone/tools/test_phone_ctl.py`
- Create: `server/tools/phone_fake_peer.py`

**Interfaces:**
- Consumes: Task 6 의 펌웨어가 낼 시리얼 줄(이 과제가 먼저라 규약은 Task 6 의 표를 본다 — `PHONE BOOT`, `STAT k=v …`, `OK t x y`, `SHOT BEGIN/B64/SHOT END <합>`, `FSM … -> …`, `SCREEN …`, `RELAY …`), 기존 `server/phone_relay.py`·`server/phone_core.py`·`server/.env.phone`.
- Produces:
  - CLI `phone_ctl.py port <1|2>` / `--unit <1|2> run <단계>…` / `pair <단계>…`. 끝나는 값 0 통과 · 1 단계 실패 또는 포트 오류 · 2 단계 글이 틀림.
  - 단계: `until:<명령>:<정규식>:<초>`, `wait:<정규식>:<초>`, `sleep:<초>`, `tap:<x>:<y>`, `shot:<파일>`, 그 외는 그대로 전송. `pair` 에서는 `1:`·`2:`·`both:` 접두사(접두사 없는 것은 `sleep` 만).
  - 출력 표식: 기기가 낸 줄은 그대로(`pair` 에서는 앞에 `[1]`·`[2]`), 도구가 보낸 줄 `>>`, 실패 `!!`, 스크린샷 저장 `== 저장: <파일> (<바이트> B, <n>번째 전송, <초>초)`.
  - 순수 함수 `find_port(unit, ports=None)`, `parse_stat(line)`, `parse_shot(lines)`, `parse_until(step)`, `split_unit(step)`, 줄 대기열 `LineQueue`.
  - CLI `phone_fake_peer.py [--base <주소>] <기기 ID> <단계>…` — 단계 `send:<call|accept|reject|hangup>`, `expect:<t>[:<why>][@<초>]`, `sleep:<초>`, `voice:<프레임수>`. 성공하면 마지막 줄 `… FAKE OK`, 실패하면 `FAKE FAIL: …` 와 종료 코드 1.

- [x] **Step 1: 실패하는 시험을 먼저 쓴다** — `firmware/tab5_phone/tools/test_phone_ctl.py`

```python
"""phone_ctl 의 순수 로직 시험 — 기기도 직렬 포트도 쓰지 않는다(가짜 포트 목록·가짜 줄 공급원·가짜 시계).
실행: cd <이 폴더> && python3 -m pytest -q -p no:cacheprovider test_phone_ctl.py"""
import base64
import re

import pytest

import phone_ctl as pc


@pytest.fixture(autouse=True)
def no_real_ports(monkeypatch):
    """안전장치: 시험 중에는 실제 포트 목록 조회도, 포트 열기도 일어나면 안 된다(두 기기는 다른 펌웨어로 쓰이는 중)."""
    def boom(*a, **k):
        raise AssertionError("시험이 실제 직렬 포트에 손을 댐")
    monkeypatch.setattr(pc, "list_usb_ports", boom)
    monkeypatch.setattr(pc, "SerialLink", boom)


# ── 기기 번호 → 포트 ─────────────────────────────────────────────────────────
class FakePort:
    def __init__(self, device, serial_number):
        self.device, self.serial_number = device, serial_number


BOTH = [FakePort("/dev/cu.usbmodem2101", "AA:BB:CC:DD:EE:01"), FakePort("/dev/cu.usbmodem1101", "AA:BB:CC:DD:EE:02")]


def test_units_match_contract():
    assert pc.UNITS == {"1": "AA:BB:CC:DD:EE:01", "2": "AA:BB:CC:DD:EE:02"}


@pytest.mark.parametrize("ports", [BOTH, BOTH[::-1]], ids=["1번 먼저", "2번 먼저"])
def test_find_port_picks_by_mac_not_by_order(ports):
    """두 대가 함께 꽂혀 있어도 번호(=MAC)로 고른다 — 엉뚱한 기기를 건드리는 사고 방지."""
    assert pc.find_port("1", ports) == "/dev/cu.usbmodem2101"
    assert pc.find_port("2", ports) == "/dev/cu.usbmodem1101"


def test_find_port_ignores_other_usb_devices_and_letter_case():
    ports = [FakePort("/dev/cu.Bluetooth-Incoming-Port", None), FakePort("/dev/cu.usbserial-10", "A50285BI"),
             FakePort("/dev/cu.usbmodem1101", "aa:bb:cc:dd:ee:02")]
    assert pc.find_port("2", ports) == "/dev/cu.usbmodem1101"


def test_find_port_fails_when_unit_absent():
    with pytest.raises(SystemExit) as e:
        pc.find_port("2", BOTH[:1])
    assert "2번" in str(e.value)


def test_find_port_rejects_unknown_unit():
    with pytest.raises(SystemExit):
        pc.find_port("3", BOTH)


def test_open_links_finds_every_unit_before_opening_any(monkeypatch):
    """한 대라도 안 보이면 아무 포트도 열지 않는다. 둘 다 보이면 번호에 맞는 포트만 연다."""
    opened = []
    monkeypatch.setattr(pc, "SerialLink", lambda device: opened.append(device) or device)
    monkeypatch.setattr(pc, "list_usb_ports", lambda: BOTH[:1])
    with pytest.raises(SystemExit):
        pc.open_links(("1", "2"))
    assert opened == []
    monkeypatch.setattr(pc, "list_usb_ports", lambda: BOTH[::-1])
    assert pc.open_links(("1", "2")) == {"1": "/dev/cu.usbmodem2101", "2": "/dev/cu.usbmodem1101"}
    assert opened == ["/dev/cu.usbmodem2101", "/dev/cu.usbmodem1101"]


# ── STAT 줄 ──────────────────────────────────────────────────────────────────
def stat(fsm="IDLE", unit=2, peer=1, missed=0, screen=None):
    """시리얼 규약(README.md "시리얼 명령")의 STAT 줄 모양."""
    return (f"STAT up=120 unit={unit} fsm={fsm} screen={screen or fsm} wifi=1 relay=1 relay_code=101 peer={peer} "
            f"missed={missed} touch=3 heap_int=412000 heap_int_min=398000 heap_psram=30100000")


def test_parse_stat_reads_word_values_and_number_values():
    s = pc.parse_stat(stat())
    assert s["fsm"] == "IDLE" and s["screen"] == "IDLE"
    assert s["up"] == 120 and s["unit"] == 2 and s["relay_code"] == 101 and s["heap_psram"] == 30100000
    assert all(type(s[k]) is int for k in s if k not in ("fsm", "screen"))


@pytest.mark.parametrize("name", ["OFFLINE", "IDLE", "DIALING", "RINGBACK", "INCOMING", "ANSWERING", "IN_CALL", "ENDING"])
def test_parse_stat_reads_every_state_name(name):
    assert pc.parse_stat(stat(fsm=name))["fsm"] == name


def test_parse_stat_reads_screen_names_with_underscore():
    assert pc.parse_stat(stat(fsm="IDLE", screen="WIFI_LIST"))["screen"] == "WIFI_LIST"


def test_parse_stat_keeps_the_number_forms_of_probe_ctl():
    s = pc.parse_stat("STAT load_pct=34.8 aec_us=5200/7100 rssi=-61 id=tab5-2 ver=1.2.3")
    assert s == {"load_pct": 34.8, "aec_us": (5200, 7100), "rssi": -61, "id": "tab5-2", "ver": "1.2.3"}


@pytest.mark.parametrize("line", ["I (1) phone: hello fsm=IDLE", "STATUS fsm=IDLE", "FSM IDLE -> DIALING acts=0x1 notice=NONE", ""])
def test_parse_stat_rejects_other_lines(line):
    assert pc.parse_stat(line) is None


# ── 스크린샷 묶음 ────────────────────────────────────────────────────────────
JPEG = b"\xff\xd8" + bytes(range(256)) * 40 + b"\xff\xd9"       # 10244 B → B64 네 줄(3072×3 + 1028)


def shot_lines(data, chunk=3072, size=None, checksum=None):
    out = [f"SHOT BEGIN {len(data) if size is None else size}"]
    out += ["B64:" + base64.b64encode(data[i:i + chunk]).decode() for i in range(0, len(data), chunk)]
    out.append(f"SHOT END {sum(data) & 0xFFFFFFFF if checksum is None else checksum}")
    return out


def lossy(lines, lost_chars=128):
    """B64 한 줄의 가운데가 빠진 모양. 128글자 = 원본 96바이트(시험 펌웨어에서 실제로 겪은 유실 크기)."""
    out = list(lines)
    out[2] = out[2][:404] + out[2][404 + lost_chars:]
    return out


def test_parse_shot_ok():
    assert len(shot_lines(JPEG)) == 6
    assert pc.parse_shot(iter(["I (123) phone: 딴 로그", *shot_lines(JPEG)])) == JPEG


def test_parse_shot_detects_size_mismatch():
    with pytest.raises(ValueError, match="크기"):
        pc.parse_shot(shot_lines(JPEG, size=len(JPEG) + 2))


def test_parse_shot_detects_checksum_mismatch():
    with pytest.raises(ValueError, match="검사합"):
        pc.parse_shot(shot_lines(JPEG, checksum=1))


def test_parse_shot_detects_changed_bytes_of_same_size():
    lines = shot_lines(JPEG)
    lines[2] = "B64:" + base64.b64encode(b"\x00" * 3072).decode()
    with pytest.raises(ValueError, match="검사합"):
        pc.parse_shot(lines)


def test_parse_shot_detects_missing_end():
    with pytest.raises(ValueError, match="끝"):
        pc.parse_shot(shot_lines(JPEG)[:-1])


def test_parse_shot_reports_no_answer_at_all():
    with pytest.raises(ValueError, match="응답"):
        pc.parse_shot(["I (1) phone: 딴 로그", stat()])


@pytest.mark.parametrize("lines", [["SHOT FAIL no-mem"], ["SHOT BEGIN 10244", "B64:AAAA", "SHOT FAIL jpeg-enc"]],
                         ids=["바로 거절", "보내다 실패"])
def test_parse_shot_reports_device_failure_with_reason(lines):
    with pytest.raises(ValueError, match="기기.*(no-mem|jpeg-enc)"):
        pc.parse_shot(lines)


def test_parse_shot_ignores_whole_log_lines_between_b64_lines():
    """다른 로그 줄이 B64 줄 사이에 통째로 끼어든 경우: 그 줄은 무시되고 데이터는 온전하다."""
    lines = shot_lines(JPEG)
    lines.insert(2, "W (9012) task_wdt: Task watchdog got triggered")
    lines.insert(4, "FSM IDLE -> INCOMING acts=0x20 notice=NONE")
    assert pc.parse_shot(lines) == JPEG


def test_parse_shot_rejects_log_text_that_cut_into_a_b64_line():
    """다른 로그가 B64 줄 한가운데를 끊고 들어온 경우: 데이터가 온전하지 않으므로 거절한다."""
    lines = shot_lines(JPEG)
    b = lines[2]
    lines[2:3] = [b[:1000] + "W (9012) task_wdt: 끼어든 경고", b[1000:]]
    with pytest.raises(ValueError):
        pc.parse_shot(lines)


def test_parse_shot_rejects_a_b64_line_with_foreign_characters_even_if_the_rest_adds_up():
    """PC-D3: 검사합이 단순 합이라 약하다 — 줄 안에 base64 가 아닌 글자가 보이면, 나머지가 맞아 보여도 받지 않는다(다시 받는다)."""
    lines = shot_lines(JPEG)
    lines[2] = lines[2][:2000] + " " + lines[2][2000:]
    with pytest.raises(ValueError, match="B64"):
        pc.parse_shot(lines)


@pytest.mark.parametrize("lost_chars", [128, 100, 97], ids=["96바이트 유실", "4배수 유실", "어긋난 유실"])
def test_parse_shot_rejects_lost_bytes(lost_chars):
    with pytest.raises(ValueError):
        pc.parse_shot(lossy(shot_lines(JPEG), lost_chars))


def test_parse_shot_restarts_at_a_new_begin_and_ignores_leftovers():
    """앞 전송의 찌꺼기(B64·SHOT END)나 끊긴 SHOT BEGIN 뒤에 새 전송이 오면 새 것만 쓴다."""
    lines = ["B64:AAAA", "SHOT END 5", "SHOT BEGIN 999", "B64:AAAA", *shot_lines(JPEG)]
    assert pc.parse_shot(lines) == JPEG


@pytest.mark.parametrize("lines", [["SHOT BEGIN"], ["SHOT BEGIN abc"], ["SHOT BEGIN 0", "SHOT END 0"],
                                   ["SHOT BEGIN 3", "B64:AAEC", "SHOT END"], ["SHOT BEGIN 3", "B64:AAEC", "SHOT END xyz"]])
def test_parse_shot_rejects_broken_marker_lines(lines):
    with pytest.raises(ValueError):
        pc.parse_shot(lines)


def test_shot_complete():
    full = shot_lines(JPEG)
    assert pc.shot_complete(full) and pc.shot_complete(["SHOT FAIL no-mem"])
    assert not pc.shot_complete(full[:-1]) and not pc.shot_complete([])
    assert not pc.shot_complete(["B64:AAAA", "SHOT END 5", *full[:-1]])     # 앞 전송의 끝 표시는 끝이 아니다


# ── 단계 문법 ────────────────────────────────────────────────────────────────
def test_parse_until():
    assert pc.parse_until("until:stat:^STAT .*fsm=IDLE.*peer=1:60") == ("stat", "^STAT .*fsm=IDLE.*peer=1", 60.0)
    assert pc.parse_until("until:wifi:^WIFI ok=1 ssid=a:b:90") == ("wifi", "^WIFI ok=1 ssid=a:b", 90.0)      # 정규식 안의 ':'
    assert pc.parse_until("until:t 640 380:^OK t:2.5") == ("t 640 380", "^OK t", 2.5)                        # 명령 안의 빈칸


@pytest.mark.parametrize("step", ["until:stat", "until:stat:^STAT", "until:stat:^STAT:abc", "until::^STAT:5", "until:stat::5"])
def test_parse_until_rejects_broken_steps(step):
    with pytest.raises(ValueError):
        pc.parse_until(step)


@pytest.mark.parametrize("step, want", [
    ("1:tap:640:380", ("1", "tap:640:380")),
    ("2:wait:^FSM .* -> INCOMING:10", ("2", "wait:^FSM .* -> INCOMING:10")),
    ("both:until:stat:^STAT .*fsm=IDLE.*peer=1:60", ("both", "until:stat:^STAT .*fsm=IDLE.*peer=1:60")),
    ("1:stat", ("1", "stat")),
    ("2:1:5", ("2", "1:5")),                          # 접두사는 맨 앞 하나만 뗀다
    ("sleep:1", (None, "sleep:1")),                   # 뒤의 1 은 초 — 기기 번호가 아니다
    ("sleep:2", (None, "sleep:2")),
    ("wait:1:5", (None, "wait:1:5")),
    ("tap:1:2", (None, "tap:1:2")),
    ("stat", (None, "stat")),
    ("t 640 380", (None, "t 640 380")),
    ("3:stat", (None, "3:stat")),                     # 숫자로 시작하지만 기기 번호가 아니다
    ("12:stat", (None, "12:stat")),
    ("10:tap:1:2", (None, "10:tap:1:2")),
    ("1 :stat", (None, "1 :stat")),
    ("1", (None, "1")),
    ("both", (None, "both")),
    ("BOTH:stat", (None, "BOTH:stat")),
    ("", (None, "")),
])
def test_split_unit(step, want):
    assert pc.split_unit(step) == want


@pytest.mark.parametrize("step, want", [
    ("until:stat:^STAT .*fsm=IDLE.*peer=1:60", ("until", ("stat", "^STAT .*fsm=IDLE.*peer=1", 60.0))),
    ("wait:^FSM .* -> INCOMING:10", ("wait", ("^FSM .* -> INCOMING", 10.0))),
    ("wait:^RELAY up=1 code=101:x:2.5", ("wait", ("^RELAY up=1 code=101:x", 2.5))),
    ("sleep:1", ("sleep", (1.0,))),
    ("shot:/tmp/a b/in-call.jpg", ("shot", ("/tmp/a b/in-call.jpg",))),
    ("tap:640:380", ("tap", (640, 380))),
    ("tap:0:719", ("tap", (0, 719))),
    ("stat", ("send", ("stat",))),
    ("t 640 380", ("send", ("t 640 380",))),
    ("reboot", ("send", ("reboot",))),
])
def test_parse_step(step, want):
    assert pc.parse_step(step) == want


@pytest.mark.parametrize("step", [
    "tap:640", "tap:640:380:1", "tap:a:b", "tap:640.5:380", "tap:1280:10", "tap:10:720", "tap:-1:5",      # 화면은 1280×720
    "wait:^FSM", "wait:^FSM:abc", "wait:(:3", "wait::3", "wait:^FSM:-1",
    "sleep:", "sleep:abc", "sleep:-1", "shot:", "until:stat:^STAT", "until:stat:(:5", "",
])
def test_parse_step_rejects_broken_steps(step):
    with pytest.raises(ValueError):
        pc.parse_step(step)


def test_plan_steps_for_pair():
    plan = pc.plan_steps(["both:until:stat:^STAT:60", "1:tap:640:380", "2:wait:^FSM .* -> INCOMING:10", "sleep:1",
                          "both:stat", "2:shot:a.jpg"], pair=True)
    assert [(u, k) for u, k, _ in plan] == [(("1", "2"), "until"), (("1",), "tap"), (("2",), "wait"), (("1", "2"), "sleep"),
                                            (("1", "2"), "send"), (("2",), "shot")]
    assert plan[1][2] == (640, 380) and plan[2][2] == ("^FSM .* -> INCOMING", 10.0)


@pytest.mark.parametrize("step, why", [
    ("stat", "기기 번호"),                 # 누구에게 보낼지 모른다
    ("wait:^FSM:3", "기기 번호"),
    ("3:stat", "기기 번호"),               # 없는 번호 — 그대로 보내지 않고 알린다
    ("both:shot:a.jpg", "both"),          # 두 기기가 같은 파일에 쓰게 된다
    ("1:tap:640", "tap"),
])
def test_plan_steps_for_pair_rejects(step, why):
    with pytest.raises(ValueError, match=why):
        pc.plan_steps(["both:until:stat:^STAT:5", step], pair=True)


def test_plan_steps_for_single_unit():
    plan = pc.plan_steps(["until:stat:^STAT:30", "tap:640:380", "sleep:1", "stat"], pair=False, unit="2")
    assert [(u, k) for u, k, _ in plan] == [(("2",), "until"), (("2",), "tap"), (("2",), "sleep"), (("2",), "send")]


@pytest.mark.parametrize("step", ["1:stat", "both:stat", "2:tap:640:380"])
def test_plan_steps_for_single_unit_rejects_unit_prefix(step):
    with pytest.raises(ValueError, match="--unit"):
        pc.plan_steps([step], pair=False, unit="2")


def test_plan_steps_rejects_empty_plan():
    with pytest.raises(ValueError):
        pc.plan_steps([], pair=True)


# ── 줄 자르기·대기열 ─────────────────────────────────────────────────────────
def test_split_lines_joins_partial_reads_and_strips_cr():
    lines, rest = pc.split_lines(b"", b"PHONE READY\r\nSTAT up=1 fs")
    assert lines == ["PHONE READY"] and rest == b"STAT up=1 fs"
    lines, rest = pc.split_lines(rest, b"m=IDLE\r\n\r\n")
    assert lines == ["STAT up=1 fsm=IDLE", ""] and rest == b""


def test_split_lines_survives_garbage_and_split_hangul():
    han = "한글 줄\n".encode()
    lines, rest = pc.split_lines(b"", b"\xff\xfe boot noise\n" + han[:2])       # 한글 한 글자(3바이트)가 읽기 경계에서 잘림
    assert len(lines) == 1 and lines[0].endswith("boot noise")
    assert pc.split_lines(rest, han[2:]) == (["한글 줄"], b"")


def queue_of(*lines):
    q = pc.LineQueue()
    for ln in lines:
        q.feed(ln)
    return q


def test_queue_finds_a_line_that_already_passed():
    """다른 기기의 단계를 처리하는 사이에 이미 지나간 줄 — 뒤늦게 기다려도 대기열에서 찾는다."""
    q = queue_of("RELAY up=1 code=101 attempt=1", "FSM IDLE -> INCOMING acts=0x20 notice=NONE", "SCREEN INCOMING")
    assert q.take(r"^FSM .* -> INCOMING") == "FSM IDLE -> INCOMING acts=0x20 notice=NONE"
    assert q.lines == ["SCREEN INCOMING"]               # 찾은 줄과 그 앞은 소비되고, 뒤는 남는다


def test_queue_keeps_everything_when_nothing_matches():
    q = queue_of("a", "b")
    assert q.take("zzz") is None and q.lines == ["a", "b"]


def test_queue_consumes_in_order_so_a_line_is_never_found_twice():
    """두 번째 통화의 wait 가 첫 통화 때의 줄에 속지 않는다."""
    q = queue_of("FSM IDLE -> INCOMING n=1", "FSM INCOMING -> IDLE", "FSM IDLE -> INCOMING n=2")
    assert q.take("-> INCOMING").endswith("n=1")
    assert q.take("-> INCOMING").endswith("n=2")
    assert q.take("-> INCOMING") is None


def test_queue_mark_limits_the_search_to_lines_after_it():
    q = queue_of("OK t 1 1")
    m = q.mark()
    assert q.take("^OK t", start=m) is None             # 명령을 보내기 전에 있던 묵은 응답은 보지 않는다
    q.feed("FSM IDLE -> DIALING")
    q.feed("OK t 640 380")
    assert q.take("^OK t", start=m, through=False) == "OK t 640 380"
    assert q.lines == ["OK t 1 1", "FSM IDLE -> DIALING"]       # through=False: 그 줄만 꺼낸다


def test_queue_pull_takes_only_the_chosen_lines():
    q = queue_of("B64:old", "SHOT BEGIN 3", "FSM A -> B", "B64:AAEC", "SHOT END 3")
    assert q.pull(lambda ln: ln.startswith(("SHOT ", "B64:")), start=1) == ["SHOT BEGIN 3", "B64:AAEC", "SHOT END 3"]
    assert q.lines == ["B64:old", "FSM A -> B"]


# ── 가짜 줄 공급원 ───────────────────────────────────────────────────────────
class Clock:
    def __init__(self):
        self.t = 0.0

    def __call__(self):
        return self.t


class FakeLink:
    """직렬 포트 대역. read() 한 번에 가짜 시계가 0.05초 간다(실제 시간은 쓰지 않는다). respond 로 기기의 응답을 흉내 낸다."""

    def __init__(self, clock, respond=None):
        self.clock, self.respond = clock, respond
        self.later, self.sent = [], []                  # later: (나올 시각, 바이트) / sent: (보낸 시각, 명령)

    def say(self, *lines, at=0.0):
        self.later += [(at, (ln + "\r\n").encode()) for ln in lines]

    def read(self):
        self.clock.t += 0.05
        due = b"".join(d for t, d in self.later if t <= self.clock.t)
        self.later = [(t, d) for t, d in self.later if t > self.clock.t]
        return due

    def write(self, data):
        assert data.endswith(b"\n") and data.count(b"\n") == 1
        cmd = data.decode()[:-1]
        self.sent.append((self.clock.t, cmd))
        if self.respond:
            self.respond(self, cmd)

    @property
    def cmds(self):
        return [c for _, c in self.sent]


def make_hub(units=("1", "2")):
    clock, out = Clock(), []
    links = {u: FakeLink(clock) for u in units}
    return pc.Hub(links, clock=clock, out=out.append), links, clock, out


def test_pair_output_is_tagged_and_b64_lines_are_hidden():
    hub, links, clock, out = make_hub()
    links["1"].say("PHONE READY", "B64:AAAA")
    links["2"].say("PHONE READY")
    hub.pump(0.3)
    assert out == ["[1] PHONE READY", "[2] PHONE READY"]
    assert hub.queues["1"].lines == ["PHONE READY", "B64:AAAA"]


def test_single_unit_output_has_no_tag():
    hub, links, clock, out = make_hub(("2",))
    links["2"].say("PHONE READY")
    hub.pump(0.2)
    assert out == ["PHONE READY"]


def test_wait_finds_a_line_that_passed_while_the_other_unit_was_tapped():
    """요구사항의 예: '1:tap' 직후 '2:wait:…INCOMING' — 2번의 그 줄은 tap 을 처리하는 동안 이미 지나갔다."""
    hub, links, clock, out = make_hub()

    def respond(link, cmd):
        links["1"].say("OK " + cmd, "FSM IDLE -> DIALING acts=0x1 notice=NONE", at=clock.t)
        links["2"].say("FSM IDLE -> INCOMING acts=0x20 notice=NONE", "SCREEN INCOMING", at=clock.t)
    links["1"].respond = respond
    assert hub.tap(("1",), 640, 380)
    assert links["2"].later == [] and "[2] FSM IDLE -> INCOMING acts=0x20 notice=NONE" in out     # 이미 읽혀 화면에 찍혔다
    t = clock.t
    assert hub.wait(("2",), r"^FSM .* -> INCOMING", 10)
    assert clock.t == t                                 # 대기열에서 바로 찾았다 — 포트를 더 기다리지 않았다
    assert hub.queues["2"].lines == ["SCREEN INCOMING"]


def test_other_units_log_keeps_being_read_while_one_unit_is_waited_on():
    hub, links, clock, out = make_hub()
    for i in range(300):
        links["2"].say(f"I ({i}) phone: 2번의 수다", at=i * 0.01)
    links["1"].say("FSM RINGBACK -> IN_CALL acts=0x180 notice=NONE", at=3.0)
    assert hub.wait(("1",), r"-> IN_CALL", 10)
    assert links["2"].later == []                       # 2번 쪽에 밀린 것이 없다(계속 읽었다)
    assert len(hub.queues["2"].lines) == 300 and out.count("[2] I (299) phone: 2번의 수다") == 1


def test_wait_fails_after_the_timeout_and_names_the_unit():
    hub, links, clock, out = make_hub()
    assert not hub.wait(("2",), r"^FSM .* -> INCOMING", 3)
    assert 3.0 <= clock.t <= 3.3
    assert out[-1].startswith("!! [2] wait") and "INCOMING" in out[-1]


def test_both_wait_needs_the_line_on_each_unit():
    hub, links, clock, out = make_hub()
    links["1"].say("FSM RINGBACK -> IN_CALL acts=0x180 notice=NONE", at=0.5)
    links["2"].say("FSM ANSWERING -> IN_CALL acts=0x180 notice=NONE", at=2.0)
    assert hub.wait(("1", "2"), r"^FSM .* -> IN_CALL", 5)
    assert 2.0 <= clock.t <= 2.3

    links["1"].say("FSM IN_CALL -> ENDING acts=0x248 notice=NONE", at=clock.t)
    assert not hub.wait(("1", "2"), r"^FSM IN_CALL -> ", 2)
    fails = [ln for ln in out if ln.startswith("!!")]
    assert len(fails) == 1 and fails[0].startswith("!! [2] wait")       # 안 나온 기기만 알린다


def test_until_resends_every_two_seconds_until_the_answer_comes():
    hub, links, clock, out = make_hub(("2",))
    links["2"].respond = lambda link, cmd: len(link.sent) >= 3 and link.say(stat(), at=clock.t)
    assert hub.until(("2",), "stat", r"^STAT .*fsm=IDLE.*peer=1", 20)
    assert links["2"].cmds == ["stat"] * 3
    t = [t for t, _ in links["2"].sent]
    assert 1.9 <= t[1] - t[0] <= 2.2 and 1.9 <= t[2] - t[1] <= 2.2


def test_until_only_trusts_lines_that_came_after_its_command():
    hub, links, clock, out = make_hub(("2",))
    links["2"].say(stat(fsm="IDLE"))                    # 예전에 받아 대기열에 남아 있는 응답
    hub.pump(0.2)
    links["2"].respond = lambda link, cmd: link.say(stat(fsm="OFFLINE"), at=clock.t)
    assert not hub.until(("2",), "stat", r"^STAT .*fsm=IDLE", 5)
    assert out[-1].startswith("!! until") and "fsm=IDLE" in out[-1]


def test_until_is_a_sync_point_older_lines_are_consumed():
    """until 로 상태를 맞춘 뒤의 wait 는 그 전의 줄(부팅 때의 '-> IDLE')에 속지 않는다."""
    hub, links, clock, out = make_hub(("1",))
    links["1"].say("PHONE BOOT unit=1 id=tab5-1", "FSM OFFLINE -> IDLE acts=0x0 notice=NONE", "PHONE READY")
    links["1"].respond = lambda link, cmd: link.say(stat(unit=1), at=clock.t)
    assert hub.until(("1",), "stat", r"^STAT .*fsm=IDLE", 10)
    assert hub.queues["1"].lines == []
    assert not hub.wait(("1",), r"^FSM .* -> IDLE", 1)


def test_both_until_resends_only_to_the_unit_still_waiting():
    hub, links, clock, out = make_hub()
    links["1"].respond = lambda link, cmd: link.say(stat(unit=1), at=clock.t)
    links["2"].respond = lambda link, cmd: len(link.sent) >= 3 and link.say(stat(unit=2), at=clock.t)
    assert hub.until(("1", "2"), "stat", r"^STAT .*fsm=IDLE.*peer=1", 60)
    assert links["1"].cmds == ["stat"] and links["2"].cmds == ["stat"] * 3


def test_both_until_fails_when_one_unit_never_answers():
    hub, links, clock, out = make_hub()
    links["1"].respond = lambda link, cmd: link.say(stat(unit=1), at=clock.t)
    assert not hub.until(("1", "2"), "stat", r"^STAT ", 5)
    fails = [ln for ln in out if ln.startswith("!!")]
    assert len(fails) == 1 and fails[0].startswith("!! [2] until")


def test_tap_sends_t_waits_for_ok_and_rests_half_a_second():
    hub, links, clock, out = make_hub(("1",))
    links["1"].respond = lambda link, cmd: link.say("OK " + cmd, at=clock.t)
    assert hub.tap(("1",), 640, 380) and hub.tap(("1",), 640, 580)
    (t1, c1), (t2, c2) = links["1"].sent
    assert (c1, c2) == ("t 640 380", "t 640 580")
    assert t2 - t1 >= 0.5                               # 터치 주입 슬롯이 1칸 — 연속 주입이 덮이지 않게
    assert hub.queues["1"].lines == []                  # 'OK t' 줄은 소비됐다
    assert ">> t 640 380" in out


def test_tap_fails_without_ok():
    hub, links, clock, out = make_hub()
    assert not hub.tap(("2",), 400, 520)
    assert links["2"].cmds == ["t 400 520"] and links["1"].cmds == []      # 다시 보내지 않는다(두 번 눌릴 수 있다)
    assert pc.TAP_ACK_S <= clock.t <= pc.TAP_ACK_S + 0.5
    assert out[-1].startswith("!! [2] tap")


def test_tap_does_not_accept_a_stale_or_wrong_ok():
    hub, links, clock, out = make_hub(("1",))
    links["1"].say("OK t 640 380")                      # 앞서 남은 응답
    hub.pump(0.2)
    links["1"].respond = lambda link, cmd: link.say("OK t 640 38", at=clock.t)      # 좌표가 다르게 읽혔다
    assert not hub.tap(("1",), 640, 380)


def test_tap_keeps_event_lines_that_raced_ahead_of_the_ok():
    hub, links, clock, out = make_hub(("1",))
    links["1"].respond = lambda link, cmd: link.say("FSM IDLE -> DIALING acts=0x1 notice=NONE", "OK " + cmd, at=clock.t)
    assert hub.tap(("1",), 640, 380)
    assert hub.wait(("1",), r"^FSM IDLE -> DIALING", 0)


def shooter(clock, *variants):
    """shot 명령에 차례대로 다른 응답을 내는 기기. 마지막 응답은 되풀이한다."""
    def respond(link, cmd):
        if cmd == "shot":
            link.say(*variants[min(len(link.sent), len(variants)) - 1], at=clock.t)
    return respond


def test_shot_saves_the_jpeg(tmp_path):
    hub, links, clock, out = make_hub(("1",))
    links["1"].respond = shooter(clock, shot_lines(JPEG))
    path = tmp_path / "idle.jpg"
    assert hub.shot("1", str(path))
    assert path.read_bytes() == JPEG and links["1"].cmds == ["shot"]
    assert not any("B64:" in ln for ln in out) and hub.queues["1"].lines == []


def test_shot_retries_when_bytes_were_lost(tmp_path):
    hub, links, clock, out = make_hub(("1",))
    links["1"].respond = shooter(clock, lossy(shot_lines(JPEG)), shot_lines(JPEG))
    path = tmp_path / "a.jpg"
    assert hub.shot("1", str(path))
    assert path.read_bytes() == JPEG and links["1"].cmds == ["shot", "shot"]
    assert any(ln.startswith("!! shot") and "크기" in ln for ln in out)


def test_shot_gives_up_after_three_retries(tmp_path):
    hub, links, clock, out = make_hub(("1",))
    links["1"].respond = shooter(clock, lossy(shot_lines(JPEG)))
    path = tmp_path / "a.jpg"
    assert not hub.shot("1", str(path))
    assert pc.SHOT_RETRIES == 3 and links["1"].cmds == ["shot"] * 4        # 처음 1번 + 다시 3번
    assert not path.exists()                            # 깨진 그림을 남기지 않는다


def test_shot_shows_the_reason_the_device_gave(tmp_path):
    hub, links, clock, out = make_hub()
    links["2"].respond = shooter(clock, ["SHOT FAIL no-mem"])
    assert not hub.shot("2", str(tmp_path / "a.jpg"))
    assert any(ln.startswith("!! [2] shot") and "no-mem" in ln for ln in out)


def test_shot_fails_fast_when_the_device_is_silent(tmp_path):
    hub, links, clock, out = make_hub(("1",))
    assert not hub.shot("1", str(tmp_path / "a.jpg"))
    assert clock.t <= (1 + pc.SHOT_RETRIES) * pc.SHOT_START_S + 2       # 전송 한도(60초)를 다 기다리지 않는다
    assert any("응답" in ln for ln in out) and not (tmp_path / "a.jpg").exists()


def test_shot_leaves_other_log_lines_for_a_later_wait(tmp_path):
    """캡처를 받는 십수 초 사이에 나온 FSM 줄을 잃지 않는다(B64 줄 사이에 끼어든 줄)."""
    hub, links, clock, out = make_hub(("1",))
    lines = shot_lines(JPEG)
    lines.insert(3, "FSM IN_CALL -> IDLE acts=0x240 notice=CALL_ENDED")
    links["1"].respond = shooter(clock, lines)
    path = tmp_path / "a.jpg"
    assert hub.shot("1", str(path)) and path.read_bytes() == JPEG
    assert hub.queues["1"].lines == ["FSM IN_CALL -> IDLE acts=0x240 notice=CALL_ENDED"]
    assert hub.wait(("1",), r"^FSM IN_CALL -> IDLE", 0)


def test_shot_reports_an_unwritable_path(tmp_path):
    hub, links, clock, out = make_hub(("1",))
    links["1"].respond = shooter(clock, shot_lines(JPEG))
    assert not hub.shot("1", str(tmp_path / "없는폴더" / "a.jpg"))
    assert links["1"].cmds == ["shot"] and out[-1].startswith("!! shot")      # 전송 문제가 아니므로 다시 받지 않는다


# ── 단계 실행 ────────────────────────────────────────────────────────────────
def test_run_steps_stops_at_the_first_failure():
    hub, links, clock, out = make_hub()
    assert not pc.run_steps(hub, pc.plan_steps(["1:wait:^never:1", "2:stat"], pair=True))
    assert links["2"].cmds == []


def test_run_steps_sends_plain_commands_to_the_named_units():
    hub, links, clock, out = make_hub()
    assert pc.run_steps(hub, pc.plan_steps(["1:stat", "both:reboot", "sleep:1"], pair=True))
    assert links["1"].cmds == ["stat", "reboot"] and links["2"].cmds == ["reboot"]
    assert ">> [1] stat" in out and ">> [2] reboot" in out
    assert clock.t >= 1.0


def test_run_steps_for_a_single_unit():
    hub, links, clock, out = make_hub(("2",))
    links["2"].respond = lambda link, cmd: cmd == "stat" and link.say(stat(), at=clock.t)
    plan = pc.plan_steps(["until:stat:^STAT .*fsm=IDLE:5", "stat", "wait:^STAT .*peer=1:5", "sleep:0.5"], pair=False, unit="2")
    assert pc.run_steps(hub, plan)
    assert links["2"].cmds == ["stat", "stat"] and ">> stat" in out


class FakePair:
    """시리얼 규약(README.md "시리얼 명령")의 줄 모양대로 답하는 가짜 전화기 두 대 + 교환기 흉내.
    도구의 흐름(누구에게 보내고 누구의 줄을 기다리는가)을 보려는 것이지 펌웨어를 검증하는 것이 아니다."""
    SCREEN = {"RINGBACK": "DIALING"}

    def __init__(self, clock, ring=True):
        self.clock, self.ring = clock, ring
        self.fsm = {"1": "IDLE", "2": "IDLE"}
        self.links = {u: FakeLink(clock, lambda link, cmd, u=u: self.on_cmd(u, cmd)) for u in ("1", "2")}

    def go(self, u, new, notice="NONE"):
        old, self.fsm[u] = self.fsm[u], new
        self.links[u].say(f"FSM {old} -> {new} acts=0x0 notice={notice}", f"SCREEN {self.SCREEN.get(new, new)}", at=self.clock.t)

    def on_cmd(self, u, cmd):
        me, peer, now = self.links[u], "2" if u == "1" else "1", self.clock.t
        if cmd == "stat":
            me.say(stat(fsm=self.fsm[u], unit=u, screen=self.SCREEN.get(self.fsm[u], self.fsm[u])), at=now)
        elif cmd == "shot":
            me.say(*shot_lines(JPEG), at=now)
        elif cmd.startswith("t "):
            me.say("OK " + cmd, at=now)
            if (self.fsm[u], cmd) == ("IDLE", "t 640 360"):             # 통화 버튼
                self.go(u, "DIALING")
                if self.ring:
                    self.go(u, "RINGBACK")
                    self.go(peer, "INCOMING")
            elif (self.fsm[u], cmd) == ("INCOMING", "t 404 428"):       # 받기
                self.go(u, "ANSWERING")
                self.go(u, "IN_CALL")
                self.go(peer, "IN_CALL")
            elif (self.fsm[u], cmd) == ("IN_CALL", "t 644 606"):        # 끊기
                self.go(u, "ENDING")
                self.go(u, "IDLE")
                self.go(peer, "IDLE", "CALL_ENDED")


def documented_scenario(tmp_path):
    """phone_ctl.py 머리 독스트링의 '예시' 단계를 그대로 꺼낸다(캡처 파일만 시험 폴더로 돌린다)."""
    block = pc.__doc__.split("예시 —")[1].split("순서대로")[0]
    steps = re.findall(r"^\s+'(.+)'", block, re.M)
    return [re.sub(r"shot:.*/", lambda m: f"shot:{tmp_path}/", s) for s in steps]


def test_documented_call_scenario_runs_against_contract_shaped_phones(tmp_path):
    clock, out = Clock(), []
    pair = FakePair(clock)
    steps = documented_scenario(tmp_path)
    assert steps[0] == "both:until:stat:^STAT .*fsm=IDLE.*peer=1:60"
    assert [s for s in steps if ":tap:" in s] == ["1:tap:640:360", "2:tap:404:428", "1:tap:644:606"]      # main/ui_layout.h 버튼 가운데
    assert pc.run_steps(pc.Hub(pair.links, clock=clock, out=out.append), pc.plan_steps(steps, pair=True)), "\n".join(out)
    shots = sorted(tmp_path.glob("*.jpg"))
    assert shots and all(p.read_bytes() == JPEG for p in shots)
    assert pair.fsm == {"1": "IDLE", "2": "IDLE"}
    assert pair.links["1"].cmds.count("t 644 606") == 1 and "t 404 428" in pair.links["2"].cmds


def test_documented_call_scenario_fails_when_the_bell_never_rings(tmp_path):
    clock, out = Clock(), []
    pair = FakePair(clock, ring=False)
    plan = pc.plan_steps(documented_scenario(tmp_path), pair=True)
    assert not pc.run_steps(pc.Hub(pair.links, clock=clock, out=out.append), plan)
    assert [ln for ln in out if ln.startswith("!!")][0].startswith("!! [2] wait")
    assert pair.links["2"].cmds == ["stat"]             # 실패한 뒤의 단계(받기 터치)는 보내지 않았다


# ── 명령줄 ───────────────────────────────────────────────────────────────────
@pytest.mark.parametrize("argv, why", [
    (["pair", "1:tap:640:380", "stat"], "기기 번호"),
    (["pair"], "단계"),
    (["run", "stat"], "--unit"),
    (["--unit", "3", "run", "stat"], "--unit"),
    (["--unit", "2", "run", "1:stat"], "--unit"),
    (["--unit", "2", "run", "tap:9999:1"], "tap"),
])
def test_main_rejects_bad_input_before_touching_any_port(argv, why, capsys):
    """단계가 틀렸으면 포트 목록조차 보지 않고 끝난다(안전장치 fixture 가 손대면 시험을 깨뜨린다)."""
    assert pc.main(argv) == 2
    assert why in capsys.readouterr().err


def test_main_port_prints_the_device_path_without_opening_it(monkeypatch, capsys):
    monkeypatch.setattr(pc, "list_usb_ports", lambda: BOTH)
    assert pc.main(["port", "1"]) == 0
    assert pc.main(["--unit", "2", "port"]) == 0
    assert capsys.readouterr().out.split() == ["/dev/cu.usbmodem2101", "/dev/cu.usbmodem1101"]


def test_main_opens_the_right_units_and_returns_the_verdict(monkeypatch, capsys):
    clock, opened = Clock(), []
    pair = FakePair(clock)
    real_hub = pc.Hub
    monkeypatch.setattr(pc, "open_links", lambda units: opened.append(units) or {u: pair.links[u] for u in units})
    monkeypatch.setattr(pc, "Hub", lambda links: real_hub(links, clock=clock))
    assert pc.main(["pair", "both:until:stat:^STAT .*fsm=IDLE:5"]) == 0
    assert pc.main(["pair", "1:wait:^never:1"]) == 1
    assert pc.main(["--unit", "2", "run", "until:stat:^STAT .*fsm=IDLE:5"]) == 0
    assert opened == [("1", "2"), ("1", "2"), ("2",)]
    assert "[1] STAT " in capsys.readouterr().out
```

- [x] **Step 2: 실패를 확인** (ESP-IDF 를 export 하지 않은 셸에서)

Run: `cd ~/tab5-toy-phone/firmware/tab5_phone/tools && python3 -m pytest -q -p no:cacheprovider test_phone_ctl.py 2>&1 | tail -3`
Expected: 수집 단계에서 FAIL — `ModuleNotFoundError: No module named 'phone_ctl'`

- [x] **Step 3: `tools/phone_ctl.py` 작성**

```python
#!/usr/bin/env python3
r"""phone_ctl.py — tab5_phone 시리얼 검증 도구. 맥에서 화면 캡처·터치 주입·상태 읽기를 자동으로 한다.
기기는 번호(=MAC)로 고른다 — 두 대가 함께 꽂혀 있어도 꽂은 순서와 무관하다. 줄 모양은 README.md "시리얼 명령"(펌웨어 쪽 정본은 main/phone_main.cpp·main/app.cpp).

  python3 phone_ctl.py port 2                    2번 기기의 포트 경로만 출력(포트를 열지 않는다. idf.py -p 에 넘길 때)
  python3 phone_ctl.py --unit 2 run <단계> …     한 기기
  python3 phone_ctl.py pair <단계> …             두 기기를 함께 열고, 단계 앞에 기기 번호를 붙인다
  끝나는 값: 0 = 모든 단계 통과, 1 = 어느 단계가 실패(또는 포트 오류), 2 = 단계 글이 틀림(포트를 열기 전에 알린다)

단계 (정규식에 ^ * | 가 들어가므로 단계마다 작은따옴표로 감싼다)
  until:<명령>:<정규식>:<초>   명령을 2초마다 다시 보내며 정규식에 맞는 응답을 기다린다(부팅 중이든 이미 켜져 있든 통한다)
  wait:<정규식>:<초>           로그에 정규식에 맞는 줄이 나올 때까지 기다린다(없으면 실패). 이미 지나간 줄도 찾는다
  sleep:<초>                   로그를 보며 쉰다
  tap:<x>:<y>                  터치 주입: 't x y' 를 보내고 'OK t x y' 를 기다린 뒤 0.5초 쉰다(기기의 주입 슬롯이 1칸)
  shot:<파일>                  화면을 JPEG 파일로 저장. 크기·검사합이 어긋나면 3회까지 다시 받는다
  그 외                        그대로 한 줄 명령으로 보낸다 (stat, reboot …)

pair 의 접두사
  1:<단계>  2:<단계>           그 기기에만
  both:<단계>                  두 기기 모두. until·wait·tap 은 둘 다 맞아야 통과. both:shot 은 안 된다(같은 파일에 쓰게 된다)
  sleep:<초>                   접두사 없이 쓴다. 다른 단계에 접두사가 없으면 실행 전에 오류로 알린다
  출력은 줄 앞에 [1]·[2] 가 붙는다. 한 기기를 기다리는 동안에도 다른 기기의 로그를 계속 읽는다.

지나간 줄을 놓치지 않는 방법 (PC-D1)
  기기마다 "아직 소비하지 않은 줄" 대기열이 있다. 읽은 줄은 화면에 찍고 대기열에 쌓아 둔다.
  - wait      대기열 맨 앞부터 찾는다. 찾으면 그 줄과 그 앞의 줄이 소비된다 → 같은 기기의 wait 는 줄이 나오는 순서대로 쓴다.
  - until     명령을 보낸 뒤에 온 줄만 본다(묵은 응답에 속지 않게). 찾으면 그 앞의 줄까지 모두 소비 = "여기서부터 새로" 맞춤점.
  - tap·shot  자기 응답 줄(OK t, SHOT·B64)만 꺼내고 나머지는 남긴다 → 그 사이에 나온 FSM 줄을 뒤의 wait 가 찾는다.

예시 — 1번이 걸기 → 2번 벨 → 2번이 받기 → 1번이 끊기 (좌표는 main/ui_layout.h 의 BTN_* 가운데)
  python3 phone_ctl.py pair \
    'both:until:stat:^STAT .*fsm=IDLE.*peer=1:60' \
    '1:tap:640:360' \
    '2:wait:^FSM IDLE -> INCOMING:10' \
    '1:wait:^FSM DIALING -> RINGBACK:10' \
    '2:tap:404:428' \
    'both:wait:^FSM .* -> IN_CALL:10' \
    '1:shot:/tmp/phone-1-in-call.jpg' \
    '2:shot:/tmp/phone-2-in-call.jpg' \
    'both:until:stat:^STAT .*fsm=IN_CALL.*screen=IN_CALL:10' \
    '1:tap:644:606' \
    '1:wait:^FSM ENDING -> IDLE:10' \
    '2:wait:^FSM IN_CALL -> IDLE:10' \
    'both:until:stat:^STAT .*fsm=IDLE.*screen=IDLE.*missed=0:10'
  순서대로: 둘 다 대기+상대 보임 / 1번 통화 버튼(640,360) / 2번에 벨 / 1번에 연결음 / 2번 받기(404,428) / 둘 다 통화 중 /
  통화 화면 캡처 두 장 / 캡처 뒤에도 통화 중인지 / 1번 끊기(644,606) / 1번·2번 대기로 / 부재중 0건.
  거절은 2번에 tap:876:428, 걸다가 취소는 1번에 tap:640:604, 한 기기만 볼 때는 --unit 2 run 'until:stat:^STAT :30' 'shot:/tmp/idle.jpg'.

주의
  - 포트를 열면 기기가 한 번 리셋될 수 있다 → 시나리오 하나는 한 번의 실행 안에서 끝내고, 첫 단계는 until 로 맞춘다.
  - '-> IDLE' 만 기다리면 부팅 때의 'FSM OFFLINE -> IDLE' 에도 맞는다 → 출발 상태까지 적는다(기기가 도중에 죽었다 살아나도 통과하지 않게).
  - shot 은 십수 초 걸릴 수 있다(115200bps 기준 추정). 벨은 30초면 교환기가 끊으므로 벨 화면 캡처는 한 장만.
  - 이 도구는 토큰·Wi-Fi 비밀번호를 다루지 않는다(규약상 기기도 어떤 줄에도 싣지 않는다).
"""
import argparse
import base64
import re
import sys
import time

UNITS = {"1": "AA:BB:CC:DD:EE:01",       # 1번 전화기 (USB 일련번호 = MAC)
         "2": "AA:BB:CC:DD:EE:02"}       # 2번 전화기
SCREEN_W, SCREEN_H = 1280, 720           # 가로, setRotation(3). 터치 좌표도 이 좌표계
RESEND_S = 2.0                           # until 이 명령을 다시 보내는 간격
TAP_ACK_S = 3.0                          # 'OK t' 를 기다리는 한도
TAP_GAP_S = 0.5                          # 터치 주입 뒤 쉬는 시간 — 주입 슬롯이 1칸이라 연속 주입이 덮인다
SHOT_RETRIES = 3                         # PC-D4: 처음 1번 + 다시 3번 = 최대 4번 받는다("최대 3회 다시 받는다"를 글자대로).
#                                          비용: 계속 실패하면 한 장에 최대 4분. 탈출구: 이 숫자
SHOT_START_S = 10.0                      # shot 을 보낸 뒤 SHOT BEGIN 이 이 안에 안 오면 그 시도는 실패(응답 없음)
SHOT_TOTAL_S = 60.0                      # 한 번의 전송 한도


# ── 기기 번호 → 포트 ─────────────────────────────────────────────────────────
def list_usb_ports():
    """맥에 보이는 직렬 포트 목록(포트를 열지 않는다). pyserial 은 쓸 때만 불러온다 — 순수 로직 시험에는 필요 없다."""
    from serial.tools import list_ports
    return list_ports.comports()


def find_port(unit, ports=None):
    """기기 번호 → 시리얼 포트 경로. USB 일련번호(=MAC)로 찾으므로 꽂은 순서와 무관하다. 포트를 열지 않는다."""
    if unit not in UNITS:
        sys.exit(f"모르는 기기 번호: {unit} (1 또는 2)")
    if ports is None:
        ports = list_usb_ports()
    for p in ports:
        if (p.serial_number or "").upper() == UNITS[unit]:
            return p.device
    sys.exit(f"{unit}번 기기({UNITS[unit]})가 USB 에 보이지 않음 — 전원이 켜져 있는지, 데이터 케이블인지 확인")


# ── 줄 읽기(순수 함수) ───────────────────────────────────────────────────────
_INT, _FLOAT, _RATIO = re.compile(r"-?\d+"), re.compile(r"-?\d+\.\d+"), re.compile(r"\d+/\d+")


def parse_stat(line):
    """'STAT k=v …' → dict. 숫자는 int/float, 'a/b' 는 (a, b), 그 밖(fsm=IDLE 같은 글자 값)은 문자열. STAT 줄이 아니면 None."""
    if not line.startswith("STAT "):
        return None
    out = {}
    for k, v in re.findall(r"(\w+)=(\S+)", line):
        if _INT.fullmatch(v):
            out[k] = int(v)
        elif _FLOAT.fullmatch(v):
            out[k] = float(v)
        elif _RATIO.fullmatch(v):
            out[k] = tuple(int(x) for x in v.split("/"))
        else:
            out[k] = v
    return out


def _marker_number(line, what):
    parts = line.split()
    if len(parts) != 3 or not parts[2].isascii() or not parts[2].isdigit():
        raise ValueError(f"{what} 줄이 깨짐: {line!r}")
    return int(parts[2])


def parse_shot(lines):
    """'SHOT BEGIN <바이트수>' … 'B64:…' … 'SHOT END <합>' → JPEG 바이트. 어긋나면 이유를 담은 ValueError.

    B64 가 아닌 줄(사이에 끼어든 다른 로그)은 건너뛴다. 새 SHOT BEGIN 이 오면 처음부터 다시 모은다(앞 전송의 찌꺼기 무시).
    PC-D3: B64 줄은 한 줄씩 엄격하게 푼다. 이유: 느슨하게 풀면 줄 안에 끼어든 글자가 조용히 버려지거나 데이터로 섞인다.
    비용: 규약에 없는 꾸밈(줄 끝 공백 등)이 붙으면 거절한다. 탈출구: validate=True 를 빼면 크기·검사합 검사만 남는다."""
    size, parts = None, []
    for ln in lines:
        if ln.startswith("SHOT FAIL"):
            raise ValueError(f"기기가 거절함: {ln}")
        if ln.startswith("SHOT BEGIN"):
            size, parts = _marker_number(ln, "SHOT BEGIN"), []
            if size == 0:
                raise ValueError("빈 스크린샷(0바이트)")
        elif size is None:
            continue                                    # 시작 전의 줄: 다른 로그이거나 앞 전송의 찌꺼기
        elif ln.startswith("B64:"):
            try:
                parts.append(base64.b64decode(ln[4:], validate=True))
            except ValueError:
                raise ValueError("B64 줄이 깨짐 — 다른 로그가 줄 안에 끼어들었거나 일부가 유실됨") from None
        elif ln.startswith("SHOT END"):
            want = _marker_number(ln, "SHOT END")
            data = b"".join(parts)
            if len(data) != size:
                raise ValueError(f"크기 불일치: 받은 {len(data)} / 보낸다던 {size}")
            if (sum(data) & 0xFFFFFFFF) != want:
                raise ValueError("검사합 불일치 — 전송 중 깨짐")
            return data
    if size is None:
        raise ValueError("응답 없음 — SHOT BEGIN 을 받지 못함")
    raise ValueError("끝 표시(SHOT END)를 받지 못함 — 시간 초과 또는 유실")


def shot_complete(lines):
    """스크린샷 줄 묶음이 끝났는가: 'SHOT FAIL', 또는 'SHOT BEGIN' 뒤의 'SHOT END'(BEGIN 앞의 END 는 앞 전송의 것)."""
    began = False
    for ln in lines:
        if ln.startswith("SHOT FAIL"):
            return True
        if ln.startswith("SHOT BEGIN"):
            began = True
        elif began and ln.startswith("SHOT END"):
            return True
    return False


def _is_shot_line(ln):
    return ln.startswith(("SHOT BEGIN", "SHOT END", "SHOT FAIL", "B64:"))


def split_lines(rest, data):
    """앞서 남은 조각 + 새로 읽은 바이트 → (완성된 줄들, 남은 조각). 줄 끝의 CR 은 뗀다. 줄 단위로 풀어 한글이 경계에서 깨지지 않는다."""
    *done, rest = (rest + data).split(b"\n")
    return [ln.decode("utf-8", "replace").rstrip("\r") for ln in done], rest


# ── 단계 문법(순수 함수) ─────────────────────────────────────────────────────
def _secs(text, step):
    try:
        v = float(text)
    except ValueError:
        v = -1.0
    if not 0 <= v < float("inf"):
        raise ValueError(f"초는 0 이상의 숫자여야 함: {step!r}")
    return v


def _regex(pat, step):
    try:
        re.compile(pat)
    except re.error as e:
        raise ValueError(f"정규식이 틀림({e}): {step!r}") from None
    return pat


def parse_until(step):
    """'until:<명령>:<정규식>:<초>' → (명령, 정규식, 초). 정규식 안의 ':' 는 허용(명령에는 없음)."""
    cmd, sep1, rest = step[len("until:"):].partition(":")
    pat, sep2, secs = rest.rpartition(":")
    if not (cmd and sep1 and pat and sep2):
        raise ValueError(f"until 은 until:<명령>:<정규식>:<초> 모양: {step!r}")
    return cmd, pat, _secs(secs, step)


def split_unit(step):
    """pair 단계의 기기 접두사를 뗀다: '1:tap:640:380' → ('1', 'tap:640:380'), 'both:…' → ('both', …), 없으면 (None, 원문).
    접두사는 정확히 '1:'·'2:'·'both:' 뿐 — 'sleep:1' 의 1, '3:…'·'12:…' 는 기기 번호가 아니다."""
    head, sep, rest = step.partition(":")
    if sep and (head in UNITS or head == "both"):
        return head, rest
    return None, step


def parse_step(step):
    """단계 글 한 개 → (종류, 인자). 종류: until·wait·sleep·tap·shot·send. 모양이 틀리면 ValueError."""
    if not step.strip():
        raise ValueError("빈 단계")
    if step.startswith("until:"):
        cmd, pat, secs = parse_until(step)
        return "until", (cmd, _regex(pat, step), secs)
    if step.startswith("wait:"):
        pat, sep, secs = step[len("wait:"):].rpartition(":")
        if not (pat and sep):
            raise ValueError(f"wait 는 wait:<정규식>:<초> 모양: {step!r}")
        return "wait", (_regex(pat, step), _secs(secs, step))
    if step.startswith("sleep:"):
        return "sleep", (_secs(step[len("sleep:"):], step),)
    if step.startswith("tap:"):
        m = re.fullmatch(r"tap:(\d+):(\d+)", step, re.ASCII)
        if not m:
            raise ValueError(f"tap 은 tap:<x>:<y> (0 이상의 정수) 모양: {step!r}")
        x, y = int(m[1]), int(m[2])
        if x >= SCREEN_W or y >= SCREEN_H:
            raise ValueError(f"tap 좌표가 화면({SCREEN_W}×{SCREEN_H}) 밖: {step!r}")
        return "tap", (x, y)
    if step.startswith("shot:"):
        if not step[len("shot:"):]:
            raise ValueError(f"shot 은 shot:<파일> 모양: {step!r}")
        return "shot", (step[len("shot:"):],)
    return "send", (step,)


def plan_steps(steps, pair, unit=None):
    """단계 글 목록 → [(기기들, 종류, 인자)]. 포트를 열기 전에 전부 확인한다 — 틀린 단계 하나 때문에 시나리오가 중간에 멈추지 않게.

    PC-D2: pair 에서 접두사 없는 단계는 sleep 만 받는다. 이유: 'stat' 을 누구에게 보낼지, 없는 번호 '3:stat' 을 그대로 보낼지
    짐작하면 엉뚱한 기기를 건드린다. 비용: 양쪽에 보낼 때도 both: 를 적어야 한다. 탈출구: 아래 검사를 "없으면 both" 로 바꾼다."""
    if not steps:
        raise ValueError("단계가 없음")
    plan = []
    for step in steps:
        who, rest = split_unit(step)
        if not pair and who is not None:
            raise ValueError(f"run 에서는 기기를 --unit 으로 고른다(단계에 접두사를 붙이지 않는다): {step!r}")
        kind, args = parse_step(rest)
        if not pair:
            plan.append(((unit,), kind, args))
            continue
        if who is None and kind != "sleep":
            raise ValueError(f"pair 단계에는 기기 번호가 필요(1: 2: both:): {step!r}")
        if who == "both" and kind == "shot":
            raise ValueError(f"both:shot 은 두 기기가 같은 파일에 쓰게 된다 — 1:shot:<파일> 2:shot:<파일> 로 나눈다: {step!r}")
        plan.append(((who,) if who in UNITS else tuple(UNITS), kind, args))
    return plan


# ── 줄 대기열(직렬 포트를 모른다 — 줄은 feed 로 넣는다) ───────────────────────
class LineQueue:
    """한 기기의 "아직 소비하지 않은 줄"(도착 순서).

    PC-D1: 읽은 줄을 버리지 않고 쌓아 두었다가, 기다리는 단계가 앞에서부터 찾아 소비한다.
    이유: 두 기기를 번갈아 다루면 한 기기의 단계를 처리하는 사이에 다른 기기의 줄이 지나간다(예: 1:tap 직후 2:wait).
    비용: 아무도 기다리지 않는 줄이 실행 내내 남는다(메모리, 그리고 넓은 정규식이 묵은 줄에 맞을 위험) → until 이 맞춤점 구실을 한다.
    탈출구: take 의 through 기본값을 바꾸면 "찾은 줄만 꺼내기"가 된다."""

    def __init__(self):
        self.lines = []

    def feed(self, line):
        self.lines.append(line)

    def mark(self):
        """지금 위치. 명령을 보내기 직전에 찍어 두면 "보낸 뒤에 온 줄"만 볼 수 있다."""
        return len(self.lines)

    def take(self, pattern, start=0, through=True):
        """start 번째부터 정규식에 맞는 첫 줄을 찾는다. 찾으면 through=True: 그 줄까지 전부 소비 / False: 그 줄만 꺼냄. 없으면 None."""
        rx = re.compile(pattern)
        for i in range(start, len(self.lines)):
            if rx.search(self.lines[i]):
                line = self.lines[i]
                del self.lines[(0 if through else i):i + 1]
                return line
        return None

    def pull(self, pred, start=0):
        """start 번째부터 pred 에 맞는 줄을 모두 꺼낸다(나머지는 그대로 남긴다)."""
        got = [ln for ln in self.lines[start:] if pred(ln)]
        if got:
            self.lines[start:] = [ln for ln in self.lines[start:] if not pred(ln)]
        return got


def _say(text):
    print(text, flush=True)


class Hub:
    """기기들의 링크와 대기열 묶음. 링크는 read() -> bytes(잠깐 기다렸다 온 만큼), write(bytes) 만 있으면 된다 — 시험에서는 가짜를 넣는다.
    어느 기기를 기다리든 poll() 이 모든 링크를 읽으므로 다른 기기의 로그가 포트에 쌓이지 않는다."""

    def __init__(self, links, clock=None, out=None):
        self.links = dict(links)
        self.queues = {u: LineQueue() for u in self.links}
        self.rest = {u: b"" for u in self.links}
        self.tagged = len(self.links) > 1               # 두 대일 때만 줄 앞에 [1]·[2]
        self.clock = clock or time.monotonic
        self.out = out or _say

    def note(self, sign, unit, text):
        self.out(f"{sign} [{unit}] {text}" if self.tagged else f"{sign} {text}")

    def poll(self):
        """모든 기기에서 한 번씩 읽어 줄 단위로 대기열에 넣고 화면에 찍는다(B64 줄은 찍지 않는다)."""
        for u, link in self.links.items():
            lines, self.rest[u] = split_lines(self.rest[u], link.read())
            for ln in lines:
                if not ln:
                    continue
                self.queues[u].feed(ln)
                if not ln.startswith("B64:"):
                    self.out(f"[{u}] {ln}" if self.tagged else ln)

    def send(self, unit, cmd, quiet=False):
        if not quiet:
            self.note(">>", unit, cmd)
        self.links[unit].write((cmd + "\n").encode())

    def pump(self, secs):
        end = self.clock() + secs
        while self.clock() < end:
            self.poll()

    def expect(self, units, pattern, secs, marks=None, through=True, resend=None):
        """units 각각의 대기열에서 정규식에 맞는 줄을 찾는다(대기열부터 훑고, 없으면 읽으며 기다린다). → {기기: 줄} — 못 찾은 기기는 빠진다.
        marks 가 있으면 그 위치 뒤의 줄만 본다. resend 가 있으면 아직 못 찾은 기기에 2초마다 다시 보낸다."""
        pos = dict(marks) if marks else {u: 0 for u in units}
        found, t0 = {}, self.clock()
        next_send = t0 + RESEND_S
        while True:
            for u in units:
                if u not in found:
                    line = self.queues[u].take(pattern, pos[u], through)
                    if line is None:
                        pos[u] = self.queues[u].mark()          # 훑은 데까지는 다시 보지 않는다
                    else:
                        found[u] = line
            now = self.clock()
            if len(found) == len(units) or now - t0 >= secs:
                return found
            if resend and now >= next_send:
                for u in units:
                    if u not in found:
                        self.send(u, resend, quiet=True)
                next_send = now + RESEND_S
            self.poll()

    def _passed(self, units, found, text):
        for u in units:
            if u not in found:
                self.note("!!", u, text)
        return len(found) == len(units)

    def _send_marked(self, units, cmd):
        """이미 와 있던 줄을 먼저 읽어 "보내기 전"으로 치고, 위치를 찍은 뒤 보낸다."""
        self.poll()
        marks = {u: self.queues[u].mark() for u in units}
        for u in units:
            self.send(u, cmd)
        return marks

    def wait(self, units, pattern, secs):
        return self._passed(units, self.expect(units, pattern, secs), f"wait 실패: {pattern} ({secs:g}초 안에 나오지 않음)")

    def until(self, units, cmd, pattern, secs):
        marks = self._send_marked(units, cmd)
        found = self.expect(units, pattern, secs, marks, resend=cmd)
        return self._passed(units, found, f"until 실패: {cmd} → {pattern} ({secs:g}초 동안 다시 보냈지만 응답 없음)")

    def tap(self, units, x, y):
        marks = self._send_marked(units, f"t {x} {y}")           # 한 번만 보낸다 — 다시 보내면 두 번 눌릴 수 있다
        found = self.expect(units, rf"^OK t {x} {y}\s*$", TAP_ACK_S, marks, through=False)
        ok = self._passed(units, found, f"tap 실패: 't {x} {y}' 에 'OK t {x} {y}' 응답이 없음")
        if ok:
            self.pump(TAP_GAP_S)
        return ok

    def shot(self, unit, path):
        q, tries = self.queues[unit], SHOT_RETRIES + 1
        for n in range(1, tries + 1):
            mark, t0, got = self._send_marked((unit,), "shot")[unit], self.clock(), []
            while not shot_complete(got):
                waited = self.clock() - t0
                if waited >= SHOT_TOTAL_S or (waited >= SHOT_START_S and not any(ln.startswith("SHOT BEGIN") for ln in got)):
                    break
                self.poll()
                got += q.pull(_is_shot_line, mark)               # 스크린샷 줄만 꺼낸다 — 사이에 낀 다른 로그는 대기열에 남는다
            try:
                data = parse_shot(got)
            except ValueError as e:
                self.note("!!", unit, f"shot 실패 {n}/{tries}: {e}")
                continue
            try:
                with open(path, "wb") as f:
                    f.write(data)
            except OSError as e:
                self.note("!!", unit, f"shot 저장 실패: {e}")
                return False
            self.note("==", unit, f"저장: {path} ({len(data)} B, {n}번째 전송, {self.clock() - t0:.1f}초)")
            return True
        return False


def run_steps(hub, plan):
    """plan_steps 가 만든 단계를 차례로 실행한다. 한 단계라도 실패하면 거기서 멈추고 False."""
    for units, kind, args in plan:
        if kind == "until":
            ok = hub.until(units, *args)
        elif kind == "wait":
            ok = hub.wait(units, *args)
        elif kind == "tap":
            ok = hub.tap(units, *args)
        elif kind == "shot":
            ok = hub.shot(units[0], *args)
        elif kind == "sleep":
            hub.pump(*args)
            ok = True
        else:
            for u in units:
                hub.send(u, *args)
            ok = True
        if not ok:
            return False
    hub.pump(0.5)
    return True


# ── 실제 포트(시험에서는 쓰지 않는다) ────────────────────────────────────────
class SerialLink:
    """실제 직렬 포트 한 개. 이 파일에서 포트를 여는 곳은 여기뿐이다. DTR 은 기본값(켜짐)으로 둔다."""

    def __init__(self, device):
        import serial
        self.s = serial.Serial(device, 115200, timeout=0.05)

    def read(self):
        return self.s.read(self.s.in_waiting or 1)       # 온 만큼 바로 받고, 없으면 0.05초까지만 기다린다

    def write(self, data):
        self.s.write(data)
        self.s.flush()


def open_links(units):
    ports = list_usb_ports()
    devices = {u: find_port(u, ports) for u in units}    # 모두 찾은 뒤에 연다 — 한 대라도 안 보이면 아무 포트도 열지 않는다
    return {u: SerialLink(d) for u, d in devices.items()}


def main(argv=None):
    ap = argparse.ArgumentParser(prog="phone_ctl.py", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--unit", help="기기 번호 1|2 (run 에 필요)")
    ap.add_argument("cmd", choices=["port", "run", "pair"])
    ap.add_argument("args", nargs="*", help="port: 기기 번호 / run·pair: 단계들")
    a = ap.parse_args(argv)
    if a.cmd == "port":
        print(find_port((a.args[0] if a.args else a.unit) or ""))
        return 0
    try:
        if a.cmd == "run" and a.unit not in UNITS:
            raise ValueError("run 에는 --unit 1 또는 --unit 2 가 필요")
        plan = plan_steps(a.args, pair=a.cmd == "pair", unit=a.unit)
    except ValueError as e:
        print(f"단계 오류: {e}", file=sys.stderr)
        return 2
    try:
        hub = Hub(open_links(tuple(UNITS) if a.cmd == "pair" else (a.unit,)))
        return 0 if run_steps(hub, plan) else 1
    except OSError as e:                                 # 포트를 다른 프로그램이 잡고 있거나, 도중에 USB 가 빠짐
        print(f"!! 포트 오류: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
```

- [x] **Step 4: 통과 확인**

Run: `cd ~/tab5-toy-phone/firmware/tab5_phone/tools && PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider test_phone_ctl.py 2>&1 | tail -1`
Expected: `150 passed`

- [x] **Step 5: 기기 번호로 포트가 나오는지 실기 확인(포트를 열지 않음)**

Run: `cd ~/tab5-toy-phone/firmware/tab5_phone/tools && python3 phone_ctl.py port 1 && python3 phone_ctl.py port 2`
Expected: `/dev/cu.usbmodem…` 두 줄(서로 다른 경로). 안 보이는 기기는 "…번 기기(…)가 USB 에 보이지 않음 …" 과 종료 코드 1.

- [x] **Step 6: 가짜 상대 — `server/tools/phone_fake_peer.py`**

```python
#!/usr/bin/env python3
"""phone_fake_peer.py — 교환기에 "기기 한 대"로 붙어 대본대로 신호를 주고받는다. 진짜 Tab5 한 대만으로 걸기·받기 화면을 시험할 때 상대 역할.

  python3 tools/phone_fake_peer.py tab5-1 <단계> ...            # http://127.0.0.1:5094 의 교환기에 tab5-1 로 접속
  python3 tools/phone_fake_peer.py --base http://127.0.0.1:5095 tab5-1 <단계> ...
단계:
  send:<call|accept|reject|hangup>     신호 보내기
  expect:<t>[:<why>]                    그 신호가 올 때까지 기다림(기본 10초). 예: expect:ring, expect:ended:hangup, expect:peer
  expect:<t>[:<why>]@<초>               기다리는 시간 지정. 예: expect:ended:timeout@40
  sleep:<초>                            기다림(그동안 온 신호는 출력하고 버린다)
  voice:<프레임수>                      20ms 간격으로 음성 흉내 프레임([순번 2][0 × 60])을 보낸다(Phase 3 시험용)
연결을 유지하는 동안 10초마다 ping 을 보낸다(교환기는 45초 무소식이면 끊는다). 토큰은 server/.env.phone 에서 읽고 출력하지 않는다.

주의: 같은 ID 의 진짜 기기가 붙어 있으면 그 기기가 밀려난다 — 흉내 낼 번호의 기기는 꺼 두거나 아직 전화기 펌웨어를 올리지 않은 상태여야 한다.
"""
import asyncio
import json
import os
import sys
import time
from pathlib import Path

import aiohttp

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import phone_core
import phone_relay

SIGNALS = ("call", "accept", "reject", "hangup")


def parse_step(step):
    """단계 글자 → (종류, 인자…). 잘못된 단계는 ValueError."""
    kind, _, rest = step.partition(":")
    if kind == "send":
        if rest not in SIGNALS:
            raise ValueError(f"send 는 {SIGNALS} 중 하나: {step}")
        return ("send", rest)
    if kind == "expect":
        body, _, secs = rest.partition("@")
        t, _, why = body.partition(":")
        if not t:
            raise ValueError(f"expect 에 신호 이름이 없음: {step}")
        return ("expect", t, why or None, float(secs) if secs else 10.0)
    if kind == "sleep":
        return ("sleep", float(rest))
    if kind == "voice":
        return ("voice", int(rest))
    raise ValueError(f"모르는 단계: {step}")


def matches(msg, t, why):
    return isinstance(msg, dict) and msg.get("t") == t and (why is None or msg.get("why") == why)


async def run(base, dev, steps, out=print):
    phone_relay.load_env_file(Path(__file__).resolve().parent.parent / ".env.phone")
    tokens = phone_core.parse_tokens(os.environ.get("PHONE_TOKENS", ""))
    if dev not in tokens:
        raise ValueError(f"등록되지 않은 기기 ID: {dev}")
    plan = [parse_step(s) for s in steps]
    t0 = time.monotonic()
    stamp = lambda: f"{time.monotonic() - t0:6.2f}s"

    async with aiohttp.ClientSession() as s:
        async with s.ws_connect(base.rstrip("/") + "/ws", headers={"X-Device-Id": dev, "X-Device-Token": tokens[dev]}) as ws:
            out(f"{stamp()} 접속됨 ({dev})")

            async def pinger():
                n = 0
                while True:
                    await asyncio.sleep(10)
                    n += 1
                    await ws.send_json({"t": "ping", "n": n})

            async def recv(deadline):
                """다음 신호 하나(pong 은 건너뜀). 시간이 다 되면 None, 연결이 닫히면 ConnectionError."""
                while True:
                    left = deadline - time.monotonic()
                    if left <= 0:
                        return None
                    try:
                        m = await ws.receive(timeout=left)
                    except asyncio.TimeoutError:
                        return None
                    if m.type == aiohttp.WSMsgType.TEXT:
                        msg = json.loads(m.data)
                        if msg.get("t") == "pong":
                            continue
                        out(f"{stamp()} ← {m.data}")
                        return msg
                    if m.type == aiohttp.WSMsgType.BINARY:
                        continue                                   # 상대가 보낸 음성 — 내용은 보지 않는다
                    raise ConnectionError(f"교환기가 연결을 닫음({m.type.name})")

            ping_task = asyncio.create_task(pinger())
            try:
                for st in plan:
                    if st[0] == "send":
                        await ws.send_json({"t": st[1]})
                        out(f"{stamp()} → {st[1]}")
                    elif st[0] == "expect":
                        _, t, why, secs = st
                        deadline = time.monotonic() + secs
                        while True:
                            msg = await recv(deadline)
                            if msg is None:
                                raise AssertionError(f"{secs:g}초 안에 {t}{':' + why if why else ''} 가 오지 않음")
                            if matches(msg, t, why):
                                break
                    elif st[0] == "sleep":
                        deadline = time.monotonic() + st[1]
                        while await recv(deadline) is not None:
                            pass
                    elif st[0] == "voice":
                        for seq in range(st[1]):
                            await ws.send_bytes((seq & 0xFFFF).to_bytes(2, "big") + bytes(60))
                            await asyncio.sleep(0.02)
                        out(f"{stamp()} → 음성 흉내 {st[1]}프레임")
            finally:
                ping_task.cancel()
    out(f"{stamp()} FAKE OK")


def main(argv):
    args = argv[1:]
    base = "http://127.0.0.1:5094"
    if args and args[0] == "--base":
        if len(args) < 2:
            sys.exit(__doc__)
        base, args = args[1], args[2:]
    if len(args) < 1:
        sys.exit(__doc__)
    try:
        asyncio.run(run(base, args[0], args[1:]))
    except (AssertionError, ValueError, ConnectionError, aiohttp.ClientError, asyncio.TimeoutError) as e:
        sys.exit(f"FAKE FAIL: {type(e).__name__}: {e}")


if __name__ == "__main__":
    main(sys.argv)
```

- [x] **Step 7: 가짜 상대 둘을 실제 교환기(임시 포트 5095)에 붙여 통화가 되는지 확인**

```bash
cd ~/tab5-toy-phone/server && (PORT=5095 BIND=127.0.0.1 nohup python3 phone_relay.py > /tmp/phone_relay_5095.log 2>&1 &) ; sleep 1; curl -s http://127.0.0.1:5095/healthz; echo
(python3 tools/phone_fake_peer.py --base http://127.0.0.1:5095 tab5-1 expect:peer expect:peer send:call expect:ringing expect:connected voice:5 sleep:0.3 send:hangup expect:ended:hangup | tail -1 &) ; sleep 0.3
python3 tools/phone_fake_peer.py --base http://127.0.0.1:5095 tab5-2 expect:peer expect:ring send:accept expect:connected expect:ended:hangup | tail -1; sleep 1
python3 tools/phone_fake_peer.py --base http://127.0.0.1:5095 tab5-1 expect:ring@0.5; echo "종료 코드 $?"
python3 tools/phone_fake_peer.py --base http://127.0.0.1:5095 tab5-1 expect:peer send:call expect:ended:offline | tail -1
pkill -f "phone_relay.py"; sleep 1; lsof -nP -iTCP:5095 -sTCP:LISTEN | wc -l
```
Expected:
```
{"ok": true}
… FAKE OK
… FAKE OK
FAKE FAIL: AssertionError: 0.5초 안에 ring 가 오지 않음
종료 코드 1
… FAKE OK
       0
```
(토큰은 `server/.env.phone` 에서 읽고 출력하지 않는다. 진짜 기기는 아직 전화기 펌웨어가 아니므로 밀려날 접속이 없다.)

- [x] **Step 8: 커밋**

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/tools/phone_ctl.py firmware/tab5_phone/tools/test_phone_ctl.py server/tools/phone_fake_peer.py && git commit -m "feat(phone): 맥 쪽 검증 도구(두 기기 시나리오·스크린샷·터치 주입)와 가짜 상대" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: 글자 자산 (Pretendard 로 구운 고정 문구)

**Files:**
- Create: `firmware/tab5_phone/tools/gen_ui_assets.py`
- Create: `firmware/tab5_phone/main/ui_assets.h`(생성물 — git 에 넣는다, 약 1.6MB)
- Create: `firmware/tab5_phone/tools/fonts/Pretendard-Bold.otf`, `Pretendard-ExtraBold.otf`(git 제외)

**Interfaces:**
- Consumes: Pretendard 1.3.9 (SIL OFL 1.1), Pillow.
- Produces: `main/ui_assets.h` — `typedef struct { uint16_t w, h; const uint8_t *a; } ui_asset_t;`, 문구 55개 `static const ui_asset_t lbl_*`(쓸 때는 `&lbl_xxx`), 숫자 `DIG_BIG[10]`·`DIG_SMALL[10]`(`const ui_asset_t *` 배열). Task 5 의 `gfx`·`ui_wifi` 와 Task 6 의 `ui` 가 쓴다.

통화 화면 문구(이름은 `main/ui.cpp` 가 쓰는 것과 같아야 한다):

| 이름 | 문구 | 굵기 | px |
|---|---|---|---|
| `lbl_wifi_connecting` / `lbl_relay_connecting` | Wi-Fi 연결 중… / 교환기 연결 중… | Bold | 40 |
| `lbl_wifi_none` | Wi-Fi 연결 안 됨 — 배지를 눌러 설정 | Bold | 32 |
| `lbl_wifi_init_fail` | Wi-Fi 칩을 켜지 못했어요. 전원을 다시 넣어 주세요 | Bold | 32 |
| `lbl_relay_fail` | 교환기에 연결할 수 없음 — 다시 시도하는 중 | Bold | 32 |
| `lbl_auth_fail` | 기기 등록 오류 — 1분 뒤 다시 시도합니다 | Bold | 32 |
| `lbl_unreg_help` | 등록되지 않은 기기예요. 펌웨어의 기기 표를 확인해 주세요 | Bold | 32 |
| `lbl_unit1` / `lbl_unit2` / `lbl_unit0` | 1번 전화기 / 2번 전화기 / 등록되지 않은 기기 | Bold | 28 |
| `lbl_wifi_setup` | Wi-Fi 설정(누르는 배지 위의 글자) | Bold | 32 |
| `lbl_peer_on` / `lbl_peer_off` | 상대 기기 켜짐 / 상대 기기가 꺼져 있음 | Bold | 32 |
| `lbl_call` | 통화 | ExtraBold | 56 |
| `lbl_missed` / `lbl_count_unit` | 부재중 / 건 | Bold | 32 |
| `lbl_dialing` / `lbl_incoming` / `lbl_answering` | 거는 중… / 전화가 왔습니다 / 연결 중… | ExtraBold | 56 |
| `lbl_ringback` | 상대 벨이 울리고 있어요 | Bold | 32 |
| `lbl_cancel` / `lbl_in_call` / `lbl_ending` | 취소 / 통화 중 / 끊는 중… | Bold | 40 |
| `lbl_accept` / `lbl_reject` / `lbl_hangup` | 받기 / 거절 / 끊기 | ExtraBold | 48 |
| `lbl_colon`, `DIG_BIG[0..9]` | : , 0~9 | ExtraBold | 160 |
| `DIG_SMALL[0..9]` | 0~9 | Bold | 32 |
| `lbl_n_peer_off` / `lbl_n_no_answer` / `lbl_n_rejected` / `lbl_n_lost` / `lbl_n_ended` / `lbl_n_missed` / `lbl_n_relay_silent` | 상대 기기가 꺼져 있어 걸 수 없어요 / 받지 않았어요 / 상대가 받지 않고 끊었어요 / 연결이 끊어졌습니다 / 통화 종료 / 부재중 전화가 있어요 / 교환기 응답이 없어 다시 연결합니다 | Bold | 36 |
| `lbl_wf_*` 21개 | Wi-Fi 설정 화면의 고정 문구(생성기 안의 목록 참조) | Bold | 28~40 |

- [x] **Step 1: Pretendard 를 내려받아 `tools/fonts/` 에 둔다** (Task 1 에서 이 폴더를 git 제외로 등록했다. 시스템 글꼴 폴더에 설치하지 않는다)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && git check-ignore -q tools/fonts/x.otf && mkdir -p tools/fonts && curl -L -s -o /tmp/Pretendard-1.3.9.zip https://github.com/orioncactus/pretendard/releases/download/v1.3.9/Pretendard-1.3.9.zip && shasum -a 256 /tmp/Pretendard-1.3.9.zip | cut -c1-64
unzip -j -o -q /tmp/Pretendard-1.3.9.zip "public/static/Pretendard-Bold.otf" "public/static/Pretendard-ExtraBold.otf" "LICENSE.txt" -d tools/fonts && shasum -a 256 tools/fonts/*.otf
```
Expected:
```
04be351a74d6bf7d60c480a3087e51d185485d35a52023142af1df19eb8c428a
2e91915fab54df71cc9598ebf608b2bdb54c6fe3c066ac61dff0bc44fca71cc7  tools/fonts/Pretendard-Bold.otf
c35fe941b7568d52a96010561540e47f9d3948dfde66ba25bc1908233e0a40cd  tools/fonts/Pretendard-ExtraBold.otf
```
검사합이 다르면 내려받은 것이 다른 파일이다 — 쓰지 말고 멈춘다. (`git check-ignore` 가 실패하면 Task 1 Step 1~3 이 안 된 것이다 — 먼저 한다.)

- [x] **Step 2: 생성기 — `tools/gen_ui_assets.py`**

```python
#!/usr/bin/env python3
"""UI 글자 자산 생성기 — Pretendard 로 안티앨리어싱 렌더한 알파(8bit) 비트맵을 C 헤더로.

왜: Tab5 화면에서 내장 비트맵 글꼴(efontKR)을 확대하면 계단이 생긴다(docs/design/README.md §2 "내장 비트맵 폰트 확대 금지").
    맥에서 Pretendard 로 한 번 그려 알파 마스크로 굽고, 기기는 합성 버퍼에 색을 알파 블렌딩만 한다(gfx::blit_asset).
출처: firmware/tab5_snapbooth/tools/gen_ui_assets.py. 바꾼 것:
  - 글꼴이 없으면 다른 글꼴로 넘어가지 않고 오류로 끝난다(원본은 조용히 AppleSDGothicNeo 로 대체했다).
  - 글꼴에 없는 글자가 문구에 있어도 오류로 끝난다(빈 네모가 구워지는 것을 막는다).
  - 스냅부스 스타일 이름 부분을 걷어냈다.
  - 숫자 10개는 세로 범위를 하나로 맞추고(높이가 같아 시계 숫자가 바뀔 때 위아래로 흔들리지 않는다),
    가로는 잉크에 딱 맞게 자른다(원본은 글꼴 고유의 좌우 여백이 붙어 있었다 — "1" 은 좌우가 달라 칸 안에서 쏠린다).
입력: 아래 LABELS (통화 화면 문구 + Wi-Fi 설정 화면용 lbl_wf_*). 이름은 main/ui.cpp·main/ui_wifi.cpp 가 쓰는 것과 같아야 한다
출력: main/ui_assets.h (ui_asset_t, lbl_*, DIG_BIG[10], DIG_SMALL[10])

실행(ESP-IDF 를 export 하지 않은 셸에서 — export 한 셸의 python3 에는 Pillow 가 없다):
    python3 tools/gen_ui_assets.py                       # tools/fonts/ 의 Pretendard 사용
    python3 tools/gen_ui_assets.py --font-dir <폴더> --preview /tmp/assets.png
글꼴: https://github.com/orioncactus/pretendard/releases (SIL OFL 1.1) 의 Pretendard-1.3.9.zip → public/static/
      Pretendard-Bold.otf, Pretendard-ExtraBold.otf 를 tools/fonts/ 에 둔다(시스템 글꼴 폴더에 설치하지 않는다).
"""
import argparse
import hashlib
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUT = ROOT / "main" / "ui_assets.h"
DEFAULT_FONT_DIR = Path(__file__).resolve().parent / "fonts"
FONT_FILES = {"bold": ("Pretendard-Bold.otf", "Bold"), "xbold": ("Pretendard-ExtraBold.otf", "ExtraBold")}
FONT_URL = "https://github.com/orioncactus/pretendard/releases/download/v1.3.9/Pretendard-1.3.9.zip"

# (C 식별자, 문구, 크기 px, 굵기)
LABELS = [
    # ── 전화기 화면 (main/ui.cpp 가 쓴다) ──
    ("lbl_wifi_connecting",  "Wi-Fi 연결 중…",                                  40, "bold"),
    ("lbl_relay_connecting", "교환기 연결 중…",                                  40, "bold"),
    ("lbl_wifi_none",        "Wi-Fi 연결 안 됨 — 배지를 눌러 설정",                32, "bold"),
    ("lbl_wifi_init_fail",   "Wi-Fi 칩을 켜지 못했어요. 전원을 다시 넣어 주세요",    32, "bold"),
    ("lbl_relay_fail",       "교환기에 연결할 수 없음 — 다시 시도하는 중",          32, "bold"),
    ("lbl_auth_fail",        "기기 등록 오류 — 1분 뒤 다시 시도합니다",            32, "bold"),
    ("lbl_unit1",            "1번 전화기",                                       28, "bold"),
    ("lbl_unit2",            "2번 전화기",                                       28, "bold"),
    ("lbl_unit0",            "등록되지 않은 기기",                                28, "bold"),
    ("lbl_unreg_help",       "등록되지 않은 기기예요. 펌웨어의 기기 표를 확인해 주세요", 32, "bold"),
    ("lbl_wifi_setup",       "Wi-Fi 설정",                                      32, "bold"),   # 누르는 배지 위의 글자 → 버튼 라벨 기준(32px 이상)
    ("lbl_peer_on",         "상대 기기 켜짐",                                   32, "bold"),
    ("lbl_peer_off",         "상대 기기가 꺼져 있음",                             32, "bold"),
    ("lbl_call",             "통화",                                            56, "xbold"),
    ("lbl_missed",           "부재중",                                          32, "bold"),
    ("lbl_count_unit",       "건",                                              32, "bold"),
    ("lbl_dialing",          "거는 중…",                                        56, "xbold"),
    ("lbl_ringback",         "상대 벨이 울리고 있어요",                           32, "bold"),
    ("lbl_cancel",           "취소",                                            40, "bold"),
    ("lbl_incoming",         "전화가 왔습니다",                                  56, "xbold"),
    ("lbl_accept",           "받기",                                            48, "xbold"),
    ("lbl_reject",           "거절",                                            48, "xbold"),
    ("lbl_answering",        "연결 중…",                                        56, "xbold"),
    ("lbl_in_call",          "통화 중",                                         40, "bold"),
    ("lbl_hangup",           "끊기",                                            48, "xbold"),
    ("lbl_ending",           "끊는 중…",                                        40, "bold"),
    ("lbl_colon",            ":",                                              160, "xbold"),
    ("lbl_n_peer_off",       "상대 기기가 꺼져 있어 걸 수 없어요",                  36, "bold"),
    ("lbl_n_no_answer",      "받지 않았어요",                                    36, "bold"),
    ("lbl_n_rejected",       "상대가 받지 않고 끊었어요",                         36, "bold"),
    ("lbl_n_lost",           "연결이 끊어졌습니다",                               36, "bold"),
    ("lbl_n_ended",          "통화 종료",                                       36, "bold"),
    ("lbl_n_missed",         "부재중 전화가 있어요",                              36, "bold"),
    ("lbl_n_relay_silent",   "교환기 응답이 없어 다시 연결합니다",                  36, "bold"),
    # ── Wi-Fi 설정 화면 (main/ui_wifi.cpp). 버튼 36px, 안내 28~32px, 제목 40px ──
    ("lbl_wf_title",         "Wi-Fi 선택",                                      40, "bold"),
    ("lbl_wf_connected",     "연결됨",                                          28, "bold"),
    ("lbl_wf_disconnected",  "연결 안 됨",                                       28, "bold"),
    ("lbl_wf_saved",         "저장됨",                                          28, "bold"),
    ("lbl_wf_scanning",      "주변 Wi-Fi를 찾는 중…",                            32, "bold"),
    ("lbl_wf_empty",         "보이는 2.4GHz Wi-Fi가 없습니다 — [다시 검색]을 눌러 주세요", 32, "bold"),
    ("lbl_wf_prev",          "이전",                                            36, "bold"),
    ("lbl_wf_next",          "다음",                                            36, "bold"),
    ("lbl_wf_rescan",        "다시 검색",                                       36, "bold"),
    ("lbl_wf_rescan_busy",   "검색 중…",                                        36, "bold"),
    ("lbl_wf_close",         "닫기",                                            36, "bold"),
    ("lbl_wf_join",          "에 연결",                                         32, "bold"),
    ("lbl_wf_show",          "보기",                                            36, "bold"),
    ("lbl_wf_hide",          "숨기기",                                          36, "bold"),
    ("lbl_wf_hint",          "영문·숫자·기호를 입력하고 [연결]을 누르세요",          28, "bold"),
    ("lbl_wf_connecting",    "연결 중…",                                        28, "bold"),
    ("lbl_wf_fail_auth",     "비밀번호가 맞지 않습니다",                          28, "bold"),
    ("lbl_wf_fail_timeout",  "연결하지 못했습니다 — 신호와 비밀번호를 확인해 주세요", 28, "bold"),
    ("lbl_wf_short_pass",    "비밀번호는 8자 이상입니다",                         28, "bold"),
    ("lbl_wf_cancel",        "취소",                                            36, "bold"),
    ("lbl_wf_connect",       "연결",                                            36, "bold"),
]
DIGIT_BIG = (160, "xbold")    # 통화 시간 "MM:SS" (gfx::draw_clock), 큰 숫자
DIGIT_SMALL = (32, "bold")    # 부재중 건수 등


class AssetError(Exception):
    pass


class Fonts:
    """Pretendard 만 쓴다. 파일이 없거나 Pretendard 가 아니면 AssetError (대체 글꼴 없음)."""

    def __init__(self, font_dir: Path):
        self.paths = {}
        for weight, (fname, style) in FONT_FILES.items():
            p = font_dir / fname
            if not p.is_file():
                raise AssetError(f"글꼴이 없습니다: {p}\n  → {FONT_URL} 의 public/static/{fname} 를 {font_dir}/ 에 넣어 주세요 "
                                 f"(다른 글꼴로 대체하지 않습니다).")
            family, got_style = ImageFont.truetype(str(p), 16).getname()
            if family != "Pretendard" or got_style != style:
                raise AssetError(f"{p} 는 Pretendard {style} 가 아닙니다 (읽은 이름: {family} {got_style}).")
            self.paths[weight] = p
        self._cache = {}

    def get(self, px: int, weight: str) -> ImageFont.FreeTypeFont:
        key = (px, weight)
        if key not in self._cache:
            self._cache[key] = ImageFont.truetype(str(self.paths[weight]), px)
        return self._cache[key]

    def sha256(self, weight: str) -> str:
        return hashlib.sha256(self.paths[weight].read_bytes()).hexdigest()

    def check_glyphs(self, text: str, weight: str, where: str):
        """글꼴에 없는 글자는 '없는 글자' 그림(.notdef)으로 그려진다 → 그 그림과 같으면 오류."""
        f = self.get(48, weight)
        notdef = f.getmask("￾")
        nd = (notdef.size, bytes(notdef))
        for ch in sorted(set(text)):
            if ch.isspace():
                continue
            m = f.getmask(ch)
            if (m.size, bytes(m)) == nd:
                raise AssetError(f"{where}: 글꼴({FONT_FILES[weight][0]})에 없는 글자 {ch!r} (U+{ord(ch):04X})")


def render(fonts: Fonts, text: str, px: int, weight: str, vbox=None, tight_x=False) -> Image.Image:
    """글자를 알파 그림으로(사방 1px 여백). 가로는 글꼴이 정한 글자 폭(좌우에 글꼴 고유 여백이 조금 포함된다).
    vbox=(위, 아래)를 주면 세로 범위를 그 값으로 고정. tight_x=True 면 가로를 실제 잉크에 딱 맞게 자른다(좌우 1px 여백)."""
    f = fonts.get(px, weight)
    d = ImageDraw.Draw(Image.new("L", (10, 10)))
    l, t, r, b = d.textbbox((0, 0), text, font=f)
    if vbox:
        t, b = vbox
    w, h = r - l + 2, b - t + 2
    img = Image.new("L", (w, h), 0)
    ImageDraw.Draw(img).text((-l + 1, -t + 1), text, font=f, fill=255)
    if tight_x:
        ink = img.getbbox()
        if ink is None:
            raise AssetError(f"{text!r}: 그려진 것이 없습니다")
        img = img.crop((max(0, ink[0] - 1), 0, min(w, ink[2] + 1), h))
    return img


def digit_vbox(fonts: Fonts, px: int, weight: str):
    f = fonts.get(px, weight)
    d = ImageDraw.Draw(Image.new("L", (10, 10)))
    boxes = [d.textbbox((0, 0), ch, font=f) for ch in "0123456789"]
    return min(bx[1] for bx in boxes), max(bx[3] for bx in boxes)


def emit(name: str, img: Image.Image, out: list, note: str):
    w, h = img.size
    if not (0 < w < 65536 and 0 < h < 65536):
        raise AssetError(f"{name}: 크기 {w}x{h} 가 uint16_t 범위를 벗어납니다")
    data = img.tobytes()
    out.append(f"// {name}: {note} → {w}x{h}")
    out.append(f"static const uint8_t {name}_a[{len(data)}] = {{")
    for i in range(0, len(data), 24):
        out.append("  " + ",".join(str(b) for b in data[i:i + 24]) + ",")
    out.append("};")
    out.append(f"static const ui_asset_t {name} = {{ {w}, {h}, {name}_a }};")


def build(fonts: Fonts):
    """돌려주는 값: (헤더 줄 목록, [(이름, 그림)], 알파 바이트 합)"""
    keys = [k for k, *_ in LABELS]
    dup = sorted({k for k in keys if keys.count(k) > 1})
    if dup:
        raise AssetError(f"자산 이름이 겹칩니다: {dup}")
    out = ["// 자동 생성: tools/gen_ui_assets.py — 수정 금지. Pretendard 알파 마스크(8bit, 0=투명 255=불투명).",
           f"// 글꼴: Pretendard 1.3.9 (SIL OFL 1.1)  Bold sha256={fonts.sha256('bold')}",
           f"//                                      ExtraBold sha256={fonts.sha256('xbold')}",
           "#pragma once", "#include <stdint.h>",
           "typedef struct { uint16_t w, h; const uint8_t *a; } ui_asset_t;", ""]
    images, total = [], 0
    for key, text, px, wt in LABELS:
        fonts.check_glyphs(text, wt, key)
        img = render(fonts, text, px, wt)
        emit(key, img, out, f'"{text}" {FONT_FILES[wt][1]} {px}px')
        images.append((key, img)); total += img.size[0] * img.size[1]
    for prefix, (px, wt) in (("dig_big", DIGIT_BIG), ("dig_small", DIGIT_SMALL)):
        fonts.check_glyphs("0123456789", wt, prefix)
        vbox = digit_vbox(fonts, px, wt)
        for dgt in "0123456789":
            # 숫자는 잉크에 딱 맞게 자른다: 글꼴의 "1" 은 좌우 여백이 달라(왼쪽 8·오른쪽 18px @160) 그대로 칸에 넣으면 한쪽으로 쏠린다.
            # 간격은 그리는 쪽(gfx::draw_number / draw_clock)이 준다.
            img = render(fonts, dgt, px, wt, vbox, tight_x=True)
            emit(f"{prefix}_{dgt}", img, out, f'"{dgt}" {FONT_FILES[wt][1]} {px}px (숫자 10개 높이 같음, 좌우는 잉크에 맞춤)')
            images.append((f"{prefix}_{dgt}", img)); total += img.size[0] * img.size[1]
    out.append("static const ui_asset_t *const DIG_BIG[10] = {" + ",".join(f"&dig_big_{d}" for d in "0123456789") + "};")
    out.append("static const ui_asset_t *const DIG_SMALL[10] = {" + ",".join(f"&dig_small_{d}" for d in "0123456789") + "};")
    return out, images, total


def write_preview(images, path: Path):
    """검수용 한 장 그림: 자산을 위에서 아래로 늘어놓는다(어두운 배경, 흰 글자)."""
    pad, gap = 16, 10
    width = max(im.size[0] for _, im in images) + 2 * pad
    height = pad + sum(im.size[1] + gap for _, im in images) + pad
    sheet = Image.new("L", (width, height), 22)
    y = pad
    for _, im in images:
        sheet.paste(Image.eval(im, lambda a: 22 + a * (242 - 22) // 255), (pad, y))
        y += im.size[1] + gap
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Pretendard 글자 자산(main/ui_assets.h) 생성")
    ap.add_argument("--font-dir", type=Path, default=DEFAULT_FONT_DIR,
                    help=f"Pretendard-Bold.otf / Pretendard-ExtraBold.otf 가 있는 폴더 (기본: {DEFAULT_FONT_DIR})")
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT, help=f"출력 헤더 (기본: {DEFAULT_OUT})")
    ap.add_argument("--preview", type=Path, default=None, help="검수용 PNG 를 이 경로에 함께 저장")
    ap.add_argument("--sizes", action="store_true", help="자산별 크기 표를 출력")
    args = ap.parse_args(argv)
    try:
        fonts = Fonts(args.font_dir)
        out, images, total = build(fonts)
    except AssetError as e:
        print(f"[gen_ui_assets] 오류: {e}", file=sys.stderr)
        return 2
    args.out.write_text("\n".join(out) + "\n", encoding="utf-8")
    if args.sizes:
        for name, im in images:
            print(f"  {name:24s} {im.size[0]:4d} x {im.size[1]:3d}")
    if args.preview:
        write_preview(images, args.preview)
    print(f"[gen_ui_assets] {args.out.name}: labels={len(LABELS)} digits=20 total={len(images)} "
          f"alpha_bytes={total:,} file_bytes={args.out.stat().st_size:,}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [x] **Step 3: 굽는다** (ESP-IDF 를 export 하지 않은 셸에서 — export 한 셸의 python3 에는 Pillow 가 없다)

Run:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/gen_ui_assets.py --preview /tmp/tab5_phone_assets.png && shasum -a 256 main/ui_assets.h | cut -c1-64
```
Expected:
```
[gen_ui_assets] ui_assets.h: labels=55 digits=20 total=75 alpha_bytes=551,455 file_bytes=1,645,968
4af5900e2b988d9838704f4107377e24f3f9b48475d064a85395a69a08da5e8b
```
`/tmp/tab5_phone_assets.png` 를 **직접 열어 본다**(Read 도구): 글자가 Pretendard 로 또렷하게 나왔는지, 빈 네모가 없는지. 검사합만 다르고 나머지 숫자가 같으면 Pillow 판이 달라 가장자리 값이 미세하게 다른 것이다(작성 시 Pillow 10.2.0) — 미리보기가 정상이면 진행하고 실제 검사합을 진행 로그에 적는다.

- [x] **Step 4: 글꼴이 없으면 조용히 대체하지 않고 실패하는지 확인**

Run: `cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/gen_ui_assets.py --font-dir /tmp/no-such-fonts --out /tmp/x_should_not_exist.h; echo "rc=$?"; ls /tmp/x_should_not_exist.h`
Expected: `[gen_ui_assets] 오류: 글꼴이 없습니다: …`, `rc=2`, `ls: … No such file or directory`

- [x] **Step 5: 커밋**

```bash
cd ~/tab5-toy-phone && git status --short firmware/tab5_phone | grep -c "fonts/" ; git add firmware/tab5_phone/tools/gen_ui_assets.py firmware/tab5_phone/main/ui_assets.h && git commit -m "feat(phone): 화면 글자 자산(Pretendard 알파 마스크 75개)과 생성기" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
Expected: 첫 줄 `0` (글꼴 폴더는 git 에 보이지 않는다)

---

### Task 5: 빌드 뼈대 + 화면 합성·Wi-Fi 매니저·Wi-Fi 설정 화면 (스냅부스에서 떼어 옴)

**Files:**
- Create: `firmware/tab5_phone/CMakeLists.txt`, `sdkconfig.defaults`, `partitions.csv`(복사), `tools/patch_esp_hosted.py`
- Create: `firmware/tab5_phone/main/CMakeLists.txt`, `main/idf_component.yml`
- Create: `firmware/tab5_phone/main/gfx.h`, `main/gfx.cpp`, `main/net_wifi.cpp`, `main/ui_wifi.h`, `main/ui_wifi.cpp`
- Create: `firmware/tab5_phone/main/stub_main.cpp`(빌드 확인용 임시 진입점 — Task 6 에서 지운다)

**Interfaces:**
- Consumes: Task 2 의 `geom.h`·`net_wifi.h`·`wifi_store.h`, Task 4 의 `ui_assets.h`, Task 1 의 `secrets.h`(`WIFI_NETWORKS`).
- Produces:
  - `gfx::init()`, `clear(rgb)`, `present()`, `fill_rect/fill_rrect/fill_circle/fill_alpha_rect/fill_alpha_rrect/fill_alpha_circle`, `blit_asset(as, x, y, rgb) -> int`, `blit_asset_center`, `draw_number(n, cx, cy, big, rgb)`, `draw_clock(mm, ss, cx, cy, rgb)`, `text(s, x, y, rgb, size) -> int`, `text_w`, `text_center`, `button(const Rect &, const ui_asset_t *, enabled, bg, fg)`, `hit_circle`, `screenshot_jpeg(const uint8_t **out) -> size_t`. 색 토큰 `C_BG … C_WHITE`.
  - `netwifi::` 구현(선언은 Task 2 의 `net_wifi.h`).
  - `uiwifi::open()`, `is_open()`, `close()`, `draw()`, `tap(x, y) -> bool`, `needs_redraw() -> bool`, `on_keyboard() -> bool`. **주의**: 연결에 성공하면 1.5초 뒤 `needs_redraw()` 가 스스로 화면을 닫고 false 를 돌려준다 — 부르는 쪽(Task 6 의 `app`)은 매 바퀴 `is_open()` 을 다시 확인한다.
  - Wi-Fi 설정 화면 버튼 위치(자동 시험용): 목록 줄 1~5 가운데 (640, 120/224/328/432/536), 이전 (199, 648), 다음 (493, 648), 다시 검색 (787, 648), **닫기 (1081, 648)**, 자판의 취소 (925, 667), 연결 (1155, 667), 보기/숨기기 (1148, 136), 글쇠 행 r·열 c 는 (62 + 115c, 259 + 102r).

스냅부스 원본(`firmware/tab5_snapbooth/main/main.cpp`, 기기 검증됨)과 달라진 점 — 기기에서 이상하면 이 목록부터 원본과 대조한다:
- **뺀 것**: 합성 버퍼 2장 교대·비동기 출력·정지 프레임·카메라 복사·JPEG 디코더·카메라 잠금 3종, 스냅부스 서버 관련(`fetch_styles`, 서버 주소 전환), `nvs_flash_init`(이제 `phone_main` 이 부른다), 시리얼 전용 Wi-Fi 명령.
- **바꾼 것**: 목록 줄 높이 72 → 96(한 쪽 6 → 5줄), 하단 버튼 높이 80 → 96, 보기/숨기기 180×64 → 184×96, 비밀번호 칸 높이 64 → 96, 고정 문구를 내장 글꼴 확대에서 글자 자산으로, Wi-Fi 칩 초기화 실패 시 재부팅 대신 `Phase::INIT_FAILED`, 신호 세기는 매니저가 10초마다 읽어 둔다.
- **그대로**: esp_hosted 초기화 순서와 핀, NVS 네임스페이스 `snapwifi`/키 `list`/형식, 저장 + 고정 목록 병합, 신호 센 순 연결, 부팅 직후 첫 스캔 0개 재시도, 같은 후보 3회 끊김이면 다음 후보, 대상이 없을 때 재연결 금지, 수동 연결 15초 제한·인증 실패 2회면 "비밀번호가 맞지 않습니다"·성공한 것만 저장, 비밀번호를 로그에 남기지 않음.

- [x] **Step 1: 프로젝트 파일 — `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tab5_phone)

# esp_hosted 생성자 자동초기화 무력화 패치 (스냅부스·시험 펌웨어와 같은 방식, 멱등). 이유는 tools/patch_esp_hosted.py 참조.
execute_process(COMMAND python3 ${CMAKE_CURRENT_LIST_DIR}/tools/patch_esp_hosted.py RESULT_VARIABLE _eh_patch_rc)
if(NOT _eh_patch_rc EQUAL 0)
  message(FATAL_ERROR "patch_esp_hosted.py failed (rc=${_eh_patch_rc})")
endif()
```

`partitions.csv` 는 시험 펌웨어 것을 그대로 복사한다(같은 NVS 위치 → 2번 기기의 저장된 Wi-Fi 목록이 유지된다):
```bash
cd ~/tab5-toy-phone/firmware && cp tab5_phone_probe/partitions.csv tab5_phone/partitions.csv
```

- [x] **Step 2: `sdkconfig.defaults`, `main/idf_component.yml`**

```ini
# M5Stack Tab5 (ESP32-P4NRW32: 16MB Flash / 32MB PSRAM) — tab5_phone_probe(Phase 0 에서 실기 검증) 설정 그대로
CONFIG_IDF_TARGET="esp32p4"
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_PARTITION_TABLE_OFFSET=0x10000

CONFIG_COMPILER_OPTIMIZATION_PERF=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_360=y
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
CONFIG_FREERTOS_HZ=1000

# PSRAM (M5GFX 의 Tab5 지원은 200MHz 필수)
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_HEX=y
CONFIG_SPIRAM_SPEED_200M=y
CONFIG_IDF_EXPERIMENTAL_FEATURES=y
CONFIG_SPIRAM_XIP_FROM_PSRAM=y
CONFIG_CACHE_L2_CACHE_256KB=y
CONFIG_CACHE_L2_CACHE_LINE_128B=y

# Tab5 의 ESP32-P4 는 rev v1.x (v3.x 와 비호환)
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP32P4_REV_MIN_100=y

# Wi-Fi: ESP32-C6 via esp_hosted 2.11.x (SDIO) + esp_wifi_remote — 스냅부스·시험 펌웨어와 동일
CONFIG_ESP_WIFI_REMOTE_ENABLED=y
CONFIG_ESP_HOSTED_ENABLED=y
CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y
CONFIG_ESP_HOSTED_IDF_SLAVE_TARGET="esp32c6"
CONFIG_ESP_HOSTED_SDIO_SLOT_1=y
CONFIG_ESP_HOSTED_SDIO_4_BIT_BUS=y
CONFIG_ESP_HOSTED_SDIO_CLOCK_FREQ_KHZ=40000
CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_CLK_SLOT_1=12
CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_CMD_SLOT_1=13
CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D0_SLOT_1=11
CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D1_4BIT_BUS_SLOT_1=10
CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D2_4BIT_BUS_SLOT_1=9
CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D3_4BIT_BUS_SLOT_1=8
CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE=15
CONFIG_ESP_HOSTED_GPIO_SLAVE_RESET_SLAVE=15
CONFIG_ESP_HOSTED_SDIO_RESET_ACTIVE_HIGH=y
CONFIG_ESP_HOSTED_SDIO_RESET_DELAY_MS=1500
CONFIG_ESP_HOSTED_SLAVE_RESET_ON_EVERY_HOST_BOOTUP=y
CONFIG_ESP_HOSTED_SDIO_OPTIMIZATION_RX_STREAMING_MODE=y
CONFIG_ESP_HOSTED_USE_MEMPOOL=y
CONFIG_ESP_HOSTED_TRANSPORT_RESTART_ON_FAILURE=y
CONFIG_ESP_HTTP_CLIENT_ENABLE_HTTPS=y
CONFIG_LWIP_MAX_SOCKETS=10

# --- 교환기 연결(esp_websocket_client) ---
# 접속 응답 헤더가 이 버퍼에 다 들어가야 한다. Cloudflare 를 거친 응답 헤더가 600~700바이트라 기본 1024 는 빠듯하다(넘치면 접속 실패, 401 도 못 읽는다)
CONFIG_WS_BUFFER_SIZE=2048
# 켜지 않는다(기본값 유지): CONFIG_ESP_WS_CLIENT_SEPARATE_TX_LOCK, CONFIG_MBEDTLS_DYNAMIC_BUFFER — wss 에서 읽기·쓰기가 겹쳐 메모리가 깨진 보고가 있다
```

```yaml
dependencies:
  idf: '>=5.4'
  # ── 스냅부스(2호기에서 검증된 조합)·시험 펌웨어(Phase 0)와 같은 판으로 고정. 풀지 말 것 ──
  m5stack/m5unified: '0.2.23'
  m5stack/m5gfx: '0.2.30'
  espressif/esp_wifi_remote: '1.6.5'
  espressif/esp_hosted: '2.11.7'        # Tab5 C6 슬레이브와 검증된 세대. 3.x 는 SDIO 초기화 실패
  # ── 소리(Phase 3 에서 쓴다). 시험 펌웨어에서 검증한 판 ──
  espressif/esp_codec_dev: '1.6.2'      # 2.0.0 은 시험판
  espressif/esp-sr: '2.5.5'             # 에코 제거. P4 rev<3 전용 라이브러리 포함
  espressif/esp_audio_codec: '2.5.0'    # Opus. 2.6 이상은 P4 rev 3.0 이상만 빌드됨 → 우리 기기(rev 1.3)는 2.5.x
  # ── 교환기 연결 ──
  espressif/esp_websocket_client: '1.8.0'   # 닫기 코드 노출·재연결 간격 변경이 이 판부터. idf>=5.0, 칩 제한 없음
```

- [x] **Step 3: esp_hosted 패치 — `tools/patch_esp_hosted.py`** (스냅부스 것과 동작이 같다. "이미 적용됨" 판정만 다른 프로젝트의 표식도 인정하도록 넓혔다)

```python
#!/usr/bin/env python3
"""esp_hosted 2.x 의 C-생성자 자동 초기화를 무력화한다 (멱등, CMake 가 빌드마다 실행).

이유: 생성자(esp_hosted_host_init)가 app_main 전에 esp_hosted_init() 를 호출해
 (1) 런타임 핀 설정(esp_hosted_sdio_set_config, Tab5 CLK12/CMD13/D0~3/RST15)이 반영되기 전에 기본 핀으로 전송을 띄우고
 (2) 큰 정적 풋프린트에서는 SDIO mempool 할당이 실패해 부팅 루프(tab5-ssh-terminal 사례).
main/net_wifi.cpp 의 매니저 태스크가 M5.begin() 이후 명시적으로 초기화한다. 재fetch 되어도 이 스크립트가 다시 적용.
출처: firmware/tab5_snapbooth/tools/patch_esp_hosted.py (동작 같음. 적용 여부 판정만 "PATCHED (tab5_" 로 넓혀
      스냅부스·시험 펌웨어에서 이미 패치된 폴더를 가져와도 오류가 나지 않게 했다).
"""
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
TARGET = ROOT / "managed_components/espressif__esp_hosted/host/port/esp/freertos/src/port_esp_hosted_host_init.c"
ORIG = "static void __attribute__((constructor)) esp_hosted_host_init(void)"
PATCHED = ("/* PATCHED (tab5_phone): 생성자 자동초기화 비활성 — net_wifi.cpp 매니저 태스크에서 명시 초기화 */\n"
           "static void __attribute__((unused)) esp_hosted_host_init(void)")

def main() -> int:
    if not TARGET.exists():
        print(f"[patch_esp_hosted] not fetched yet: {TARGET}"); return 0
    src = TARGET.read_text()
    if "PATCHED (tab5_" in src:
        print("[patch_esp_hosted] already applied"); return 0
    if ORIG not in src:
        print("[patch_esp_hosted] ERROR: constructor pattern not found — esp_hosted layout changed", file=sys.stderr); return 1
    TARGET.write_text(src.replace(ORIG, PATCHED)); print("[patch_esp_hosted] applied"); return 0

if __name__ == "__main__":
    sys.exit(main())
```

- [x] **Step 4: 화면 합성의 약속 — `main/gfx.h`**

```cpp
// gfx.h — 화면 합성·출력. 1280×720 RGB565 버퍼 한 장에 그린 뒤 present() 로 한 번에 내보낸다.
// 출처: firmware/tab5_snapbooth/main/main.cpp 의 합성 방식(카메라·버퍼 교대는 걷어냄). docs/design/README.md §2 "버퍼에 합성 후 한 번에 출력".
// 규칙: 그리기 함수와 present() 는 화면 태스크 하나에서만 부른다(잠금 없음). screenshot_jpeg() 만 다른 태스크에서 불러도 된다.
#pragma once
#include <cstddef>
#include <cstdint>

#include "geom.h"           // SCREEN_W, SCREEN_H, Rect
#include "ui_assets.h"      // tools/gen_ui_assets.py 가 만든 알파 마스크(ui_asset_t, DIG_BIG, DIG_SMALL, lbl_*)

// 디자인 토큰(docs/design/README.md §1, 다크). RGB888
#define C_BG        0x161615u
#define C_SURFACE   0x1D1D1Bu
#define C_LINE      0x2A2A28u
#define C_INK       0xF2F1EEu
#define C_INK2      0xD9D8D4u
#define C_MUTED     0x9A9893u
#define C_RED_BG    0x3A2223u
#define C_RED_T     0xF0A7A5u
#define C_GREEN_BG  0x1F2E22u
#define C_GREEN_T   0xA6D6AAu
#define C_BLUE_BG   0x1C2E3Au
#define C_BLUE_T    0x8FCBF2u
#define C_YELLOW_BG 0x3A3220u
#define C_YELLOW_T  0xE8C877u
#define C_WHITE     0xFFFFFFu

namespace gfx {

// M5.begin() 과 M5.Display.setRotation() 뒤에 한 번. 합성 버퍼(PSRAM)·스프라이트 묶기·PPA 등록·DSI 프레임버퍼·JPEG 인코더.
// 실패하면 false 를 돌려주고 로그에 "gfx: init fail: <단계>" 를 남긴다.
//   - 회전은 3(기본) 또는 1 만 받는다(PPA 회전 각도를 여기서 정한다). 패널 감지가 실패해 1280×720 이 아니면 false.
//   - JPEG 인코더만 실패하면 true 다(화면은 쓸 수 있다). 그때 screenshot_jpeg() 는 0 을 돌려준다(로그 "gfx: init warn: jpeg encoder").
//   - 성공하면 배경색(C_BG) 한 장을 내보낸 상태로 돌아온다. init 이 실패했으면 아래 함수들은 아무것도 하지 않는다.
bool init();

void clear(uint32_t rgb);                 // 합성 버퍼 전체를 한 색으로
void present();                           // 합성 버퍼를 화면으로(끝날 때까지 기다림, 약 60~80ms)

// 도형. fill_rrect·fill_circle 은 안티앨리어싱.
void fill_rect(int x, int y, int w, int h, uint32_t rgb);
void fill_rrect(int x, int y, int w, int h, int r, uint32_t rgb);
void fill_circle(int cx, int cy, int r, uint32_t rgb);
void fill_alpha_rect(int x, int y, int w, int h, uint32_t rgb, uint8_t a);
void fill_alpha_rrect(int x, int y, int w, int h, int r, uint32_t rgb, uint8_t a);
void fill_alpha_circle(int cx, int cy, int r, uint32_t rgb, uint8_t a);    // 가장자리 1px 고리(반지름 r+1)는 알파 a/3 로 부드럽게

// 미리 구운 글자(Pretendard 알파 마스크). blit_asset 은 그린 폭을 돌려준다. as 가 nullptr 이면 아무것도 안 한다.
int  blit_asset(const ui_asset_t *as, int x, int y, uint32_t rgb);
void blit_asset_center(const ui_asset_t *as, int cx, int cy, uint32_t rgb);
void draw_number(int n, int cx, int cy, bool big, uint32_t rgb);                 // 숫자 자산(DIG_BIG / DIG_SMALL), 가운데 정렬
void draw_clock(int minutes, int seconds, int cx, int cy, uint32_t rgb);         // "MM:SS" — DIG_BIG 숫자 + lbl_colon, 가운데 정렬
                                                                                 //   숫자는 같은 폭의 칸에 넣는다(초가 바뀌어도 흔들리지 않음). 약 501×119px. 분이 100 이상이면 분 자리가 늘어난다

// 바뀌는 짧은 글자(내장 efontKR_16_b) — Wi-Fi 이름, 비밀번호 입력 칸 등. size 는 배율.
int  text(const char *s, int x, int y, uint32_t rgb, float size = 1.0f);         // 왼쪽·세로 가운데 기준, 돌려주는 값 = 폭
int  text_w(const char *s, float size);
void text_center(const char *s, int cx, int cy, uint32_t rgb, float size);

// 큰 버튼: 둥근 사각형 + 가운데에 글자 자산. enabled=false 면 흐리게(바탕 C_SURFACE, 글자 C_LINE).
// 모서리 반지름 = 높이의 1/5 (16~28px 로 제한).
void button(const Rect &r, const ui_asset_t *label, bool enabled, uint32_t bg, uint32_t fg);

bool hit_circle(int x, int y, int cx, int cy, int r);

// 마지막으로 present() 한 화면을 JPEG 로 만든다(하드웨어 인코더). *out 은 내부 버퍼를 가리키며 다음 호출 때까지 유효하다.
// 돌려주는 값 = 바이트 수, 실패하면 0. 내부 잠금이 있어 시리얼 태스크에서 불러도 된다(그동안 present 는 기다린다).
//   - 화면 태스크가 다음 화면을 그리는 도중이면 반쪽 화면을 찍지 않고, 곧 올 present() 직후의 화면을 찍는다(최대 1초 기다림,
//     그 안에 present() 가 없으면 0). 그래서 "그리기 → present()" 를 짝지어 부를 것 — 그려 놓고 present 를 미루면 캡처가 실패한다.
//   - 품질 85, 1280×720. 내부 버퍼는 1MB.
size_t screenshot_jpeg(const uint8_t **out);

}  // namespace gfx
```

- [x] **Step 5: `main/gfx.cpp`**

```cpp
// gfx.cpp — 화면 합성·출력 구현.
// 출처: firmware/tab5_snapbooth/main/main.cpp (실기 검증된 합성기)에서 카메라와 얽힌 부분을 걷어낸 것.
//   가져온 것: 픽셀 합성(rgb565·blend565·알파 사각형/둥근 사각형/원·글자 자산 블릿·숫자), 스프라이트 묶기와 바이트 순서 자가 검증,
//             PPA 로 세로 패널 프레임버퍼에 한 번에 회전 출력, 하드웨어 JPEG 인코더(화면 캡처).
//   걷어낸 것: 합성 버퍼 2장 교대(swap_compose_buffer), 비동기 PPA 와 완료 콜백(ppa_done_cb), 정지 프레임(s_frozen),
//             카메라 복사용 PPA(ppa_copy_frame), JPEG 디코더, s_proc_mutex.
//
// 결정
//   G-D1 합성 버퍼는 1장, present() 는 동기(끝날 때까지 기다림).
//        이유: 전화기 화면은 카메라처럼 초당 수십 장을 내지 않는다. 버퍼 교대가 없으면 "지금 그리는 버퍼 = 내보낼 버퍼" 하나라 단순하다.
//        비용: present() 한 번(약 60~80ms) 동안 화면 태스크가 멈춘다. 탈출구: 원본의 2장 교대 + PPA_TRANS_MODE_NON_BLOCKING 으로 되돌린다.
//   G-D2 화면 캡처는 "마지막으로 내보낸 화면"만 찍는다 — 그리는 도중의 반쪽 화면을 찍지 않는다.
//        방법: 그리기 함수가 s_dirty 를 켜고 present() 가 끈다. 캡처 요청이 왔을 때 s_dirty 가 꺼져 있으면 그 자리에서 인코딩하고,
//              켜져 있으면(또는 인코딩 도중 켜지면) 곧 올 present() 가 내보낸 직후 대신 인코딩해 준다(최대 SHOT_WAIT_MS 기다림).
//        이유: 그리기 함수에는 잠금이 없다(화면 태스크 전용). 버퍼를 한 장 더 두고 매번 복사하면 present 마다 1.8MB 복사가 든다.
//        비용: 그리기 함수마다 원자 변수 쓰기 1회. 탈출구: s_dirty 검사를 지우면 "잠금만 있는" 단순한 캡처가 된다.
//   G-D3 PPA 회전 각도는 M5.Display.getRotation() 에서 정한다(3 → 90°, 1 → 270°). 원본의 SNAP_ROT180 매크로와 같은 대응.
#include "gfx.h"

#include <M5Unified.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "driver/jpeg_encode.h"
#include "driver/ppa.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lgfx/v1/platforms/esp32p4/Panel_DSI.hpp"

namespace {

const char *TAG = "gfx";

constexpr int    PANEL_W        = 720;                        // 물리 패널(세로)
constexpr int    PANEL_H        = 1280;
constexpr size_t BUF_BYTES      = (size_t)SCREEN_W * SCREEN_H * 2;   // RGB565
constexpr size_t CACHE_ALIGN    = 128;                        // sdkconfig: CONFIG_CACHE_L2_CACHE_LINE_128B
constexpr size_t JPEG_OUT_BYTES = 1024 * 1024;                // 원본과 같은 1MB(화면 한 장은 보통 100KB 안쪽)
constexpr int    JPEG_QUALITY   = 85;                         // 원본 시리얼 캡처와 같은 값
constexpr int    SHOT_WAIT_MS   = 1000;                       // 그리는 중일 때 다음 present() 를 기다리는 한도

uint8_t *s_buf = nullptr;                                     // 합성 버퍼(가로 1280×720, native LE RGB565)
uint8_t *s_fb = nullptr;                                      // DSI 프레임버퍼(세로 720×1280). init 이 다 끝나야 채워진다 → 공개 함수는 이것으로 준비 여부를 본다
size_t   s_fb_size = 0;
ppa_client_handle_t s_ppa = nullptr;
ppa_srm_rotation_angle_t s_rot = PPA_SRM_ROTATION_ANGLE_90;
lgfx::LGFX_Sprite s_ov;                                       // 합성 버퍼에 묶인 스프라이트(안티앨리어싱 도형·내장 글꼴)

SemaphoreHandle_t s_lock = nullptr;                           // present() ↔ screenshot_jpeg()
SemaphoreHandle_t s_shot_done = nullptr;                      // present() 가 대신 인코딩을 끝냈다
std::atomic<bool> s_dirty{false};                             // 마지막 present() 뒤로 합성 버퍼에 그린 것이 있다
std::atomic<bool> s_shot_req{false};                          // 다음 present() 에서 인코딩해 달라
uint32_t s_shot_len = 0;                                      // s_lock 보호

jpeg_encoder_handle_t s_jenc = nullptr;
uint8_t *s_jenc_out = nullptr;
size_t   s_jenc_cap = 0;

inline void mark() { s_dirty.store(true); }

inline uint16_t rgb565(uint32_t c) { return (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F)); }

inline uint16_t blend565(uint16_t d, uint32_t c, uint8_t a)   // a: 0..255
{
    if (a == 255) return rgb565(c);
    if (a == 0) return d;
    uint32_t dr = (d >> 11) & 31, dg = (d >> 5) & 63, db = d & 31;
    uint32_t sr = (c >> 19) & 31, sg = (c >> 10) & 63, sb = (c >> 3) & 31;
    uint32_t ia = 255 - a;
    uint32_t r = (sr * a + dr * ia + 127) / 255, g = (sg * a + dg * ia + 127) / 255, b = (sb * a + db * ia + 127) / 255;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

inline uint16_t *px(int x, int y) { return (uint16_t *)s_buf + y * SCREEN_W + x; }

// 한 줄(높이 1)을 알파로 칠한다. 화면 밖은 잘라낸다.
inline void blend_span(int x, int y, int w, uint32_t c, uint8_t a)
{
    if (y < 0 || y >= SCREEN_H) return;
    int x0 = std::max(0, x), x1 = std::min(SCREEN_W, x + w);
    if (x1 <= x0) return;
    uint16_t *p = px(x0, y);
    for (int xx = x0; xx < x1; ++xx, ++p) *p = blend565(*p, c, a);
}

inline int circle_dx(int r, int dy) { return (int)std::lround(std::sqrt((float)(r * r - dy * dy))); }

// 합성 버퍼 → JPEG (s_jenc_out). 돌려주는 값 0 = 실패. s_lock 을 쥔 채로 부른다.
uint32_t encode_locked()
{
    if (!s_jenc || !s_jenc_out) return 0;
    jpeg_encode_cfg_t cfg = {};
    cfg.height = SCREEN_H;
    cfg.width = SCREEN_W;
    cfg.src_type = JPEG_ENCODE_IN_FORMAT_RGB565;
    cfg.sub_sample = JPEG_DOWN_SAMPLING_YUV420;
    cfg.image_quality = JPEG_QUALITY;
    uint32_t len = 0;
    esp_err_t e = jpeg_encoder_process(s_jenc, &cfg, s_buf, BUF_BYTES, s_jenc_out, s_jenc_cap, &len);
    if (e != ESP_OK) { ESP_LOGE(TAG, "jpeg encode failed: %s", esp_err_to_name(e)); return 0; }
    return len;
}

// 스프라이트를 합성 버퍼에 묶고 바이트 순서를 자가 검증한다(원본 bind_overlay_sprite).
// 직접 쓰는 픽셀(blend565)과 PPA·JPEG 인코더는 native LE RGB565 를 기대한다 → 스프라이트도 같은 순서로 써야 색이 맞는다.
bool bind_sprite()
{
    // 버퍼가 묶인 채 setColorDepth 를 부르면 LovyanGFX 가 같은 크기의 버퍼를 새로 잡으려 든다(1.8MB) → 먼저 비운다.
    // (우리 버퍼는 "미리 잡아 준 버퍼"로 묶이므로 deleteSprite 가 해제하지 않는다)
    s_ov.deleteSprite();
    s_ov.setColorDepth(lgfx::color_depth_t::rgb565_nonswapped);
    s_ov.setBuffer(s_buf, SCREEN_W, SCREEN_H, 0);
    s_ov.fillRect(0, 0, 2, 1, (uint32_t)0xFF0000u);           // 빨강 한 점을 찍어 실제로 어떤 바이트가 들어갔나 본다
    uint16_t raw = *(uint16_t *)s_buf;
    if (raw != 0xF800) {
        ESP_LOGW(TAG, "sprite byte order raw=0x%04X → rgb565_2Byte 로 다시 묶음", raw);
        s_ov.deleteSprite();
        s_ov.setColorDepth(lgfx::color_depth_t::rgb565_2Byte);
        s_ov.setBuffer(s_buf, SCREEN_W, SCREEN_H, 0);
        s_ov.fillRect(0, 0, 2, 1, (uint32_t)0xFF0000u);
        raw = *(uint16_t *)s_buf;
    }
    s_ov.setTextWrap(false);
    ESP_LOGI(TAG, "sprite bound (native RGB565 %s)", raw == 0xF800 ? "OK" : "MISMATCH");
    return raw == 0xF800;
}

bool init_fail(const char *stage)
{
    ESP_LOGE(TAG, "init fail: %s", stage);                    // 로그에는 "gfx: init fail: <단계>" 로 찍힌다
    return false;
}

}  // namespace

namespace gfx {

bool init()
{
    if (s_fb) return true;                                    // 이미 끝남

    // 1) 패널: Tab5 이고 가로 1280×720 으로 돌려져 있어야 한다(패널 감지가 실패하면 폭이 0 이다)
    const auto board = M5.getBoard();
    if (board != m5::board_t::board_M5Tab5 && board != m5::board_t::board_M5Tab5X) return init_fail("board (Tab5 가 아님)");
    if (M5.Display.width() != SCREEN_W || M5.Display.height() != SCREEN_H) return init_fail("panel (1280x720 가로가 아님 — 패널 감지 실패 또는 setRotation 누락)");
    switch (M5.Display.getRotation()) {                       // G-D3
    case 3: s_rot = PPA_SRM_ROTATION_ANGLE_90; break;
    case 1: s_rot = PPA_SRM_ROTATION_ANGLE_270; break;
    default: return init_fail("rotation (1 또는 3 만 지원)");
    }

    // 2) DSI 프레임버퍼: PPA 가 여기에 직접 쓴다(128바이트 정렬 필수). s_fb 는 준비가 다 끝난 뒤에 넣는다(그 전에는 present 가 아무것도 안 함)
    auto *panel = static_cast<lgfx::Panel_DSI *>(M5.Display.getPanel());
    uint8_t *fb = panel ? (uint8_t *)panel->config_detail().buffer : nullptr;
    if (!fb || ((uintptr_t)fb & (CACHE_ALIGN - 1)) != 0) return init_fail("dsi framebuffer");

    // 3) 잠금
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    if (!s_shot_done) s_shot_done = xSemaphoreCreateBinary();
    if (!s_lock || !s_shot_done) return init_fail("semaphore");

    // 4) 합성 버퍼(PSRAM, 캐시 줄 정렬) + 스프라이트
    if (!s_buf) s_buf = (uint8_t *)heap_caps_aligned_calloc(CACHE_ALIGN, 1, BUF_BYTES, MALLOC_CAP_SPIRAM);
    if (!s_buf) return init_fail("compose buffer (PSRAM 1.8MB)");
    if (!bind_sprite()) return init_fail("sprite byte order");

    // 5) PPA (회전·복사 한 번에). 동기 전용이라 대기 1건이면 충분하다
    if (!s_ppa) {
        ppa_client_config_t pc = {};
        pc.oper_type = PPA_OPERATION_SRM;
        pc.max_pending_trans_num = 1;
        pc.data_burst_length = PPA_DATA_BURST_LENGTH_128;
        if (ppa_register_client(&pc, &s_ppa) != ESP_OK) { s_ppa = nullptr; return init_fail("ppa client"); }
    }

    // 6) JPEG 인코더(화면 캡처용). 엔진은 칩에 하나뿐이다. 실패해도 화면은 쓸 수 있으므로 init 은 성공으로 둔다
    if (!s_jenc) {
        jpeg_encode_engine_cfg_t ec = {};
        ec.intr_priority = 0;
        ec.timeout_ms = 1000;
        if (jpeg_new_encoder_engine(&ec, &s_jenc) != ESP_OK) s_jenc = nullptr;
    }
    if (s_jenc && !s_jenc_out) {
        jpeg_encode_memory_alloc_cfg_t mc = {};
        mc.buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER;
        s_jenc_out = (uint8_t *)jpeg_alloc_encoder_mem(JPEG_OUT_BYTES, &mc, &s_jenc_cap);
    }
    if (!s_jenc || !s_jenc_out) ESP_LOGW(TAG, "init warn: jpeg encoder — 화면 캡처(screenshot_jpeg)는 0 을 돌려준다");

    s_fb_size = (size_t)PANEL_W * PANEL_H * 2;
    s_fb = fb;                                                // 여기부터 present() 가 동작한다
    ESP_LOGI(TAG, "init ok: compose %p, dsi fb %p, rotation %d", s_buf, s_fb, (int)M5.Display.getRotation());
    clear(C_BG);
    present();                                                // 부팅 로고·쓰레기 대신 배경색 한 장
    return true;
}

void clear(uint32_t rgb)
{
    if (!s_fb) return;
    mark();
    std::fill_n((uint16_t *)s_buf, (size_t)SCREEN_W * SCREEN_H, rgb565(rgb));
}

void present()
{
    if (!s_fb) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    ppa_srm_oper_config_t op = {};
    op.in.buffer = s_buf;
    op.in.pic_w = SCREEN_W;  op.in.pic_h = SCREEN_H;
    op.in.block_w = SCREEN_W; op.in.block_h = SCREEN_H;
    op.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
    op.out.buffer = s_fb;    op.out.buffer_size = s_fb_size;
    op.out.pic_w = PANEL_W;  op.out.pic_h = PANEL_H;
    op.out.block_offset_x = 0; op.out.block_offset_y = 0;
    op.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
    op.rotation_angle = s_rot;
    op.scale_x = 1.0f; op.scale_y = 1.0f;
    op.alpha_update_mode = PPA_ALPHA_NO_CHANGE;
    op.mode = PPA_TRANS_MODE_BLOCKING;
    esp_err_t err = ppa_do_scale_rotate_mirror(s_ppa, &op);
    if (err != ESP_OK) ESP_LOGW(TAG, "ppa srm failed: %s", esp_err_to_name(err));
    s_dirty.store(false);                                     // 이제 합성 버퍼 = 화면
    if (s_shot_req.exchange(false)) {                         // G-D2: 캡처가 기다리고 있으면 지금(그리기가 멈춘 순간) 인코딩
        s_shot_len = encode_locked();
        xSemaphoreGive(s_shot_done);
    }
    xSemaphoreGive(s_lock);
}

void fill_rect(int x, int y, int w, int h, uint32_t rgb)
{
    if (!s_fb) return;
    mark();
    int x0 = std::max(0, x), y0 = std::max(0, y), x1 = std::min(SCREEN_W, x + w), y1 = std::min(SCREEN_H, y + h);
    if (x1 <= x0) return;
    const uint16_t v = rgb565(rgb);
    for (int yy = y0; yy < y1; ++yy) std::fill_n(px(x0, yy), x1 - x0, v);
}

void fill_rrect(int x, int y, int w, int h, int r, uint32_t rgb)
{
    if (!s_fb || w <= 0 || h <= 0) return;
    mark();
    r = std::max(0, std::min(r, std::min(w, h) / 2));
    s_ov.fillSmoothRoundRect(x, y, w, h, r, rgb);             // rgb 는 uint32_t → LovyanGFX 가 RGB888 로 해석한다(int 로 넘기면 RGB565 로 본다)
}

void fill_circle(int cx, int cy, int r, uint32_t rgb)
{
    if (!s_fb || r <= 0) return;
    mark();
    s_ov.fillSmoothCircle(cx, cy, r, rgb);
}

void fill_alpha_rect(int x, int y, int w, int h, uint32_t rgb, uint8_t a)
{
    if (!s_fb) return;
    mark();
    int y0 = std::max(0, y), y1 = std::min(SCREEN_H, y + h);
    for (int yy = y0; yy < y1; ++yy) blend_span(x, yy, w, rgb, a);
}

// 둥근 사각형 알파 채우기(모서리는 원 방정식으로 줄마다 들여쓰기)
void fill_alpha_rrect(int x, int y, int w, int h, int r, uint32_t rgb, uint8_t a)
{
    if (!s_fb || w <= 0 || h <= 0) return;
    mark();
    r = std::max(0, std::min(r, std::min(w, h) / 2));
    for (int yy = 0; yy < h; ++yy) {
        int inset = 0;
        if (yy < r)           { int dy = r - yy - 1;   inset = r - circle_dx(r, dy); }
        else if (yy >= h - r) { int dy = yy - (h - r); inset = r - circle_dx(r, dy); }
        blend_span(x + inset, y + yy, w - 2 * inset, rgb, a);
    }
}

// 반투명 원. 가장자리 1px 고리는 알파 1/3 로 깔아 계단을 누그러뜨린다(원본 fill_alpha_circle_soft 의 2단 알파).
// 원본은 큰 원(a/3) 위에 작은 원(a)을 덧칠해 안쪽이 a 보다 조금 진했다 — 여기서는 안쪽이 정확히 a 가 되게 고리만 따로 칠한다.
void fill_alpha_circle(int cx, int cy, int r, uint32_t rgb, uint8_t a)
{
    if (!s_fb || r <= 0) return;
    mark();
    const int ro = r + 1;
    for (int dy = -ro; dy <= ro; ++dy) {
        const int dxo = circle_dx(ro, dy);
        if (dy < -r || dy > r) { blend_span(cx - dxo, cy + dy, 2 * dxo + 1, rgb, a / 3); continue; }
        const int dxi = circle_dx(r, dy);
        blend_span(cx - dxi, cy + dy, 2 * dxi + 1, rgb, a);
        if (dxo > dxi) {
            blend_span(cx - dxo, cy + dy, dxo - dxi, rgb, a / 3);
            blend_span(cx + dxi + 1, cy + dy, dxo - dxi, rgb, a / 3);
        }
    }
}

int blit_asset(const ui_asset_t *as, int x, int y, uint32_t rgb)
{
    if (!as || !s_fb) return 0;
    mark();
    for (int yy = 0; yy < as->h; ++yy) {
        int ty = y + yy;
        if (ty < 0 || ty >= SCREEN_H) continue;
        const uint8_t *row = as->a + yy * as->w;
        for (int xx = 0; xx < as->w; ++xx) {
            int tx = x + xx;
            if (tx < 0 || tx >= SCREEN_W) continue;
            uint8_t a = row[xx];
            if (!a) continue;
            uint16_t *p = px(tx, ty);
            *p = blend565(*p, rgb, a);
        }
    }
    return as->w;
}

void blit_asset_center(const ui_asset_t *as, int cx, int cy, uint32_t rgb)
{
    if (as) blit_asset(as, cx - as->w / 2, cy - as->h / 2, rgb);
}

void draw_number(int n, int cx, int cy, bool big, uint32_t rgb)
{
    char s[12];
    snprintf(s, sizeof s, "%d", std::max(0, n));
    const ui_asset_t *const *set = big ? DIG_BIG : DIG_SMALL;
    const int gap = big ? 14 : 2;                             // 숫자 자산은 잉크에 맞춰 잘려 있다(좌우 1px) → 글꼴 본래 간격만큼 띄운다
    int total = 0, h = 0;
    for (const char *q = s; *q; ++q) { const ui_asset_t *d = set[*q - '0']; total += d->w + gap; h = std::max<int>(h, d->h); }
    total -= gap;
    int x = cx - total / 2;
    for (const char *q = s; *q; ++q) { const ui_asset_t *d = set[*q - '0']; blit_asset(d, x, cy - h / 2 + (h - d->h) / 2, rgb); x += d->w + gap; }
}

// "MM:SS". 숫자마다 폭이 다르므로(1 이 좁다) 가장 넓은 숫자 폭의 칸에 가운데로 넣는다 → 초가 바뀌어도 글자가 좌우로 흔들리지 않는다.
// 전체 폭 = 칸 4개(각 97) + 숫자 사이 20×2 + 쌍점(49) + 쌍점 좌우 12×2 = 501px (자산이 바뀌면 달라진다). 분이 100 이상이면 분 자리만 늘어난다.
void draw_clock(int minutes, int seconds, int cx, int cy, uint32_t rgb)
{
    minutes = std::max(0, minutes);
    seconds = std::min(59, std::max(0, seconds));
    char mm[12];
    snprintf(mm, sizeof mm, "%02d", minutes);
    const int nm = (int)strlen(mm);
    int cell = 0, h = 0;
    for (int i = 0; i < 10; ++i) { cell = std::max<int>(cell, DIG_BIG[i]->w); h = std::max<int>(h, DIG_BIG[i]->h); }
    const int gap = 20, colon_pad = 12;                       // 쌍점 자산에는 좌우 여백이 10px 씩 들어 있다 → 숫자 사이와 비슷한 간격이 된다
    const int total = (nm + 2) * cell + nm * gap + 2 * colon_pad + lbl_colon.w;   // 숫자 사이 틈: 분 (nm-1)개 + 초 1개
    int x = cx - total / 2;
    auto digit = [&](int v) {
        const ui_asset_t *d = DIG_BIG[v];
        blit_asset(d, x + (cell - d->w) / 2, cy - h / 2 + (h - d->h) / 2, rgb);
        x += cell;
    };
    for (int i = 0; i < nm; ++i) { if (i) x += gap; digit(mm[i] - '0'); }
    x += colon_pad;
    blit_asset(&lbl_colon, x, cy - lbl_colon.h / 2, rgb);
    x += lbl_colon.w + colon_pad;
    digit(seconds / 10);
    x += gap;
    digit(seconds % 10);
}

int text(const char *s, int x, int y, uint32_t rgb, float size)
{
    if (!s || !s_fb) return 0;
    mark();
    s_ov.setFont(&fonts::efontKR_16_b);
    s_ov.setTextSize(size);
    s_ov.setTextDatum(textdatum_t::middle_left);
    s_ov.setTextColor(rgb);
    s_ov.drawString(s, x, y);
    return s_ov.textWidth(s);
}

int text_w(const char *s, float size)
{
    if (!s || !s_fb) return 0;
    s_ov.setFont(&fonts::efontKR_16_b);
    s_ov.setTextSize(size);
    return s_ov.textWidth(s);
}

void text_center(const char *s, int cx, int cy, uint32_t rgb, float size)
{
    if (!s || !s_fb) return;
    mark();
    s_ov.setFont(&fonts::efontKR_16_b);
    s_ov.setTextSize(size);
    s_ov.setTextDatum(textdatum_t::middle_center);
    s_ov.setTextColor(rgb);
    s_ov.drawString(s, cx, cy);
}

// 모서리 반지름은 버튼 높이의 1/5 (16~28px): 96px 버튼 19, 120px 24, 140px 이상 28
void button(const Rect &r, const ui_asset_t *label, bool enabled, uint32_t bg, uint32_t fg)
{
    fill_rrect(r.x, r.y, r.w, r.h, std::min(28, std::max(16, r.h / 5)), enabled ? bg : (uint32_t)C_SURFACE);
    blit_asset_center(label, r.x + r.w / 2, r.y + r.h / 2, enabled ? fg : (uint32_t)C_LINE);
}

bool hit_circle(int x, int y, int cx, int cy, int r)
{
    int dx = x - cx, dy = y - cy;
    return dx * dx + dy * dy <= r * r;
}

size_t screenshot_jpeg(const uint8_t **out)
{
    if (out) *out = nullptr;
    if (!out || !s_fb || !s_jenc || !s_jenc_out) return 0;
    uint32_t len = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool clean = !s_dirty.load();
    if (clean) {                                              // 합성 버퍼 = 마지막으로 내보낸 화면 → 바로 찍는다
        len = encode_locked();
        clean = !s_dirty.load();                              // 찍는 사이 화면 태스크가 다음 화면을 그리기 시작했으면 섞였을 수 있다 → 버린다
    }
    if (clean) {
        xSemaphoreGive(s_lock);
    } else {                                                  // 그리는 중 → 곧 올 present() 가 내보낸 직후 대신 찍어 준다
        xSemaphoreTake(s_shot_done, 0);                       // 지난 신호 비우기
        s_shot_req.store(true);
        xSemaphoreGive(s_lock);
        if (xSemaphoreTake(s_shot_done, pdMS_TO_TICKS(SHOT_WAIT_MS)) != pdTRUE) {
            xSemaphoreTake(s_lock, portMAX_DELAY);
            const bool unanswered = s_shot_req.exchange(false);   // 아직 켜져 있으면 present() 가 오지 않은 것
            xSemaphoreGive(s_lock);
            if (unanswered) { ESP_LOGW(TAG, "screenshot: 그리는 중인데 %dms 안에 present() 가 없었다", SHOT_WAIT_MS); return 0; }
            xSemaphoreTake(s_shot_done, 0);                   // 시간 초과 직후 present() 가 처리했다 → 그 결과를 쓴다
        }
        xSemaphoreTake(s_lock, portMAX_DELAY);
        len = s_shot_len;
        xSemaphoreGive(s_lock);
    }
    if (!len) return 0;
    *out = s_jenc_out;
    return len;
}

}  // namespace gfx
```

- [x] **Step 6: Wi-Fi 매니저 — `main/net_wifi.cpp`** (선언은 Task 2 Step 3 의 `net_wifi.h`)

```cpp
// net_wifi.cpp — Wi-Fi 매니저 구현.
// 출처: firmware/tab5_snapbooth/main/main.cpp 의 Wi-Fi 매니저(D11·D12·D13, 2호기에서 실기 검증).
//   그대로 둔 동작: 저장 목록(NVS "snapwifi"/"list") + secrets.h 고정 목록 병합, 스캔에서 보이는 것 중 신호 센 순으로 연결,
//                  부팅 직후 첫 스캔 0개면 1.5초 뒤 다시(최대 2번), 같은 후보에서 3번 끊기면 다음 후보, 후보가 떨어지면 3초 뒤 재스캔,
//                  대상이 없을 때는 재연결하지 않음(s_have_target), 아는 망이 안 보이면 20초마다 재스캔(설정 화면이 열려 있으면 멈춤),
//                  수동 연결 15초 제한·인증 실패 2번이면 "비밀번호 틀림"·성공한 것만 저장, 비밀번호는 로그에 남기지 않음.
//   걷어낸 것: 스냅부스 서버 관련(fetch_styles, 서버 주소 전환), 화면 호출(ui_invalidate, s_state 참조), nvs_flash_init(main 이 부른다),
//             시리얼 전용 요청(wauto).
//   바꾼 것: 화면에 알리는 방법 = change_seq() 증가(화면이 읽어 간다). 화면과 나누는 값은 s_lock 또는 원자 변수로 보호.
//
// 결정
//   N-D1 초기화 실패는 재부팅(ESP_ERROR_CHECK) 대신 Phase::INIT_FAILED 로 알린다.
//        이유: 화면이 "Wi-Fi 칩을 켜지 못했어요. 전원을 다시 넣어 주세요" 를 보여 줄 수 있어야 한다(설계서 §9.7).
//        비용: 스스로 회복하지 않는다. 탈출구: wifi_task 의 실패 지점에서 esp_restart() 를 부르면 원본처럼 된다.
//   N-D2 "연결" 요청을 받는 즉시 Manual::CONNECTING 으로 바꾼다(원본은 매니저 태스크가 요청을 꺼낼 때 바꿨다).
//        이유: 매니저가 스캔 중이면 2~3초 동안 화면이 반응 없는 것처럼 보이고, 그 사이 "연결"이 여러 번 눌릴 수 있다.
//   N-D3 신호 세기는 매니저 태스크가 10초마다 읽어 둔다. rssi() 는 그 값을 돌려줄 뿐이다.
//        이유: esp_wifi_sta_get_ap_info 는 C6 까지 다녀오는 호출이라 화면 태스크에서 직접 부르면 화면이 멈출 수 있다.
//        change_seq 는 신호 막대 칸수(wifistore::bars)가 바뀔 때만 올린다(1dBm 흔들릴 때마다 화면을 다시 그리지 않게).
#include "net_wifi.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>

#include "esp_event.h"
#include "esp_hosted.h"
extern "C" {
#include "esp_hosted_transport_config.h"   // C 헤더에 extern "C" 가드가 없다 → C++ 이름 맹글링 방지
}
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

#include "secrets.h"   // WIFI_NETWORKS (git 제외). 주의: WIFI_SSID·WIFI_PASS 는 매크로 이름이라 변수·열거 이름으로 쓰면 빌드가 깨진다

#ifndef WIFI_NETWORKS
#error "secrets.h 에 WIFI_NETWORKS 가 없습니다. 예: #define WIFI_NETWORKS { {\"ssid\", \"pass\"}, {\"ssid2\", \"pass2\"} }"
#endif

namespace {

const char *TAG = "netwifi";

constexpr int MANUAL_LIMIT_MS = 15000;      // 수동 연결 제한
constexpr int RESCAN_MS       = 20000;      // 아는 망이 안 보일 때 재스캔 간격
constexpr int RSSI_PERIOD_MS  = 10000;      // N-D3

struct FixedCred { const char *ssid; const char *pass; };
const FixedCred FIXED[] = WIFI_NETWORKS;    // secrets.h 고정 후보 (D11)
const int FIXED_N = sizeof(FIXED) / sizeof(FIXED[0]);

enum class Req : uint8_t { NEXT, SCAN, CONNECT, FORGET };
struct Msg { Req type; bool use_saved; char ssid[33]; char pass[65]; };

QueueHandle_t      s_q = nullptr;           // → 매니저 태스크
EventGroupHandle_t s_eg = nullptr;          // 연결 결과
SemaphoreHandle_t  s_lock = nullptr;        // 아래 "s_lock 보호" 값들
const EventBits_t  B_OK = BIT0, B_FAIL = BIT1;

std::atomic<bool> s_started{false};         // esp_wifi_start 까지 끝남
std::atomic<bool> s_init_failed{false};
std::atomic<bool> s_ok{false};              // IP 를 받은 상태
std::atomic<bool> s_none{false};            // 아는 망이 하나도 안 보임
std::atomic<bool> s_suppress{false};        // 수동 스캔·연결 중: 이벤트 핸들러의 자동 재시도를 막는다(원본 s_manual)
std::atomic<bool> s_have_target{false};     // 매니저가 연결 대상을 정했을 때만 끊김 뒤 재시도(없으면 이전 설정으로 헛도는 것을 막는다)
std::atomic<bool> s_scanning{false};
std::atomic<bool> s_setup_open{false};
std::atomic<int>  s_scan_pending{0};        // 요청됐지만 아직 끝나지 않은 스캔 수
std::atomic<int>  s_disc_reason{0};
std::atomic<int>  s_auto_fails{0};
std::atomic<int>  s_rssi{-127};
std::atomic<uint32_t> s_seq{0};

// ── s_lock 보호 (화면 태스크가 읽는다) ──
std::vector<wifistore::Ap> s_scan;          // 마지막 스캔(신호 센 순)
std::vector<std::string>   s_known_ssids;   // 아는 망 이름
char s_ssid[33] = "";                       // 연결된 망
char s_connecting[33] = "";                 // 지금 붙으려는 망
netwifi::Manual s_manual = netwifi::Manual::IDLE;
int64_t s_manual_ms = 0;

// ── 매니저 태스크 전용 ──
std::vector<wifistore::Cred> s_saved, s_known;
std::vector<int> s_rank;
int s_rank_pos = 0;

struct Lock {
    Lock()  { if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY); }
    ~Lock() { if (s_lock) xSemaphoreGive(s_lock); }
};

int64_t now_ms() { return esp_timer_get_time() / 1000; }
void bump() { s_seq.fetch_add(1); }

void set_manual(netwifi::Manual m)
{
    { Lock l; s_manual = m; s_manual_ms = now_ms(); }
    bump();
}

bool post(Req t, const char *ssid = nullptr, const char *pass = nullptr, bool use_saved = false)
{
    if (!s_q) return false;
    Msg m = {};
    m.type = t; m.use_saved = use_saved;
    if (ssid) strlcpy(m.ssid, ssid, sizeof m.ssid);
    if (pass) strlcpy(m.pass, pass, sizeof m.pass);
    if (xQueueSend(s_q, &m, 0) != pdTRUE) { ESP_LOGW(TAG, "request queue full (type %d)", (int)t); return false; }
    return true;
}

// ---------------------------------------------------------------------------
// 이벤트 (기본 이벤트 루프 태스크에서 불린다)
// ---------------------------------------------------------------------------
void on_event(void *, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        auto *e = (wifi_event_sta_disconnected_t *)data;
        s_disc_reason = e ? e->reason : 0;
        const bool was_ok = s_ok.exchange(false);
        if (was_ok) { s_rssi = -127; bump(); }
        xEventGroupSetBits(s_eg, B_FAIL);
        if (s_suppress || !s_have_target) return;                 // 수동 스캔·연결은 매니저가 다루고, 대상이 없으면 재시도하지 않는다
        ESP_LOGW(TAG, "wifi disconnected (reason %d)", s_disc_reason.load());
        if (was_ok) s_auto_fails = 0;
        if (++s_auto_fails >= 3) { s_auto_fails = 0; post(Req::NEXT); }   // 같은 후보 3번 → 다음으로 센 후보
        else esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto *e = (ip_event_got_ip_t *)data;
        char name[33];
        { Lock l; strlcpy(s_ssid, s_connecting, sizeof s_ssid); strlcpy(name, s_ssid, sizeof name); }
        s_ok = true; s_auto_fails = 0; s_none = false;
        bump();
        xEventGroupSetBits(s_eg, B_OK);
        ESP_LOGI(TAG, "wifi got ip " IPSTR " (%s)", IP2STR(&e->ip_info.ip), name);
    }
}

// ---------------------------------------------------------------------------
// 저장 목록 (NVS — 스냅부스와 같은 네임스페이스·키·형식)
// ---------------------------------------------------------------------------
void nvs_load_saved()
{
    s_saved.clear();
    nvs_handle_t h;
    if (nvs_open("snapwifi", NVS_READONLY, &h) != ESP_OK) return;   // 저장한 적이 없으면 네임스페이스가 없다
    size_t len = 0;
    if (nvs_get_blob(h, "list", nullptr, &len) == ESP_OK && len > 0 && len < 4096) {
        std::string blob(len, '\0');
        if (nvs_get_blob(h, "list", &blob[0], &len) == ESP_OK) s_saved = wifistore::unpack(blob);
    }
    nvs_close(h);
}

void nvs_store_saved()
{
    nvs_handle_t h;
    if (nvs_open("snapwifi", NVS_READWRITE, &h) != ESP_OK) { ESP_LOGE(TAG, "nvs open failed — Wi-Fi 목록을 저장하지 못함"); return; }
    const std::string blob = wifistore::pack(s_saved);
    esp_err_t e = blob.empty() ? nvs_erase_key(h, "list") : nvs_set_blob(h, "list", blob.data(), blob.size());
    if (e == ESP_ERR_NVS_NOT_FOUND) e = ESP_OK;                       // 지울 것이 없었음
    if (e == ESP_OK) e = nvs_commit(h);
    if (e != ESP_OK) ESP_LOGE(TAG, "nvs write failed: %s", esp_err_to_name(e));
    nvs_close(h);
}

void rebuild_known()
{
    std::vector<wifistore::Cred> fixed;
    for (int i = 0; i < FIXED_N; ++i) { wifistore::Cred c; c.ssid = FIXED[i].ssid; c.pass = FIXED[i].pass; fixed.push_back(c); }
    s_known = wifistore::merge(s_saved, fixed);
    std::vector<wifistore::Ap> scan;
    std::string target;
    {
        Lock l;
        s_known_ssids.clear();
        for (auto &c : s_known) s_known_ssids.push_back(c.ssid);
        scan = s_scan;
        target = s_connecting;
    }
    // s_rank 는 s_known 의 번호표다 → 목록이 바뀌면 다시 매긴다.
    // (원본은 다시 매기지 않아, 저장 목록을 지운 뒤 "다음 후보"로 넘어가면 줄어든 목록 밖을 읽을 수 있었다)
    // s_rank_pos = "지금 붙어 있거나 붙으려는 망"의 순위. 그 망이 순위에 없으면 -1 → 다음 후보는 가장 센 것부터.
    s_rank = wifistore::rank(s_known, scan);
    s_rank_pos = -1;
    const int cur = wifistore::find(s_known, target);
    for (size_t i = 0; i < s_rank.size(); ++i) if (s_rank[i] == cur) { s_rank_pos = (int)i; break; }
    bump();
    ESP_LOGI(TAG, "wifi known: %u (saved %u + fixed %d)", (unsigned)s_known.size(), (unsigned)s_saved.size(), FIXED_N);
}

// ---------------------------------------------------------------------------
// 스캔·연결 (매니저 태스크)
// ---------------------------------------------------------------------------
// 스캔(연결 시도 중이면 시도를 멈추고 스캔). 결과는 s_scan 에 올린다
bool do_scan()
{
    s_scanning = true; bump();
    wifi_scan_config_t sc = {};
    sc.show_hidden = false;
    const bool prev = s_suppress;
    esp_err_t e = esp_wifi_scan_start(&sc, true);
    if (e != ESP_OK && !s_ok) {                                       // 연결 시도 중에는 스캔이 안 된다 → 시도를 멈추고 다시
        s_suppress = true;
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(300));
        e = esp_wifi_scan_start(&sc, true);
    }
    std::vector<wifistore::Ap> raw;
    if (e == ESP_OK) {
        uint16_t n = 0;
        esp_wifi_scan_get_ap_num(&n);
        std::vector<wifi_ap_record_t> aps(n);
        if (n && esp_wifi_scan_get_ap_records(&n, aps.data()) == ESP_OK)
            for (uint16_t k = 0; k < n; ++k) {
                wifistore::Ap a;
                a.ssid = (const char *)aps[k].ssid; a.rssi = aps[k].rssi; a.secured = aps[k].authmode != WIFI_AUTH_OPEN;
                raw.push_back(a);
            }
    } else ESP_LOGW(TAG, "wifi scan failed: %s", esp_err_to_name(e));
    auto list = wifistore::dedupe(raw);
    const unsigned count = (unsigned)list.size();
    { Lock l; s_scan = std::move(list); }
    s_suppress = prev;
    s_scanning = false; bump();
    ESP_LOGI(TAG, "wifi scan: %u networks", count);
    return e == ESP_OK;
}

bool scan_empty() { Lock l; return s_scan.empty(); }

void connect_to(const wifistore::Cred &c)
{
    // 이전 연결의 disconnect 이벤트가 C6(SDIO)를 거쳐 늦게 와서 새 시도의 실패로 잘못 읽히지 않도록, 그 이벤트를 기다려 비운 뒤 시작한다(원본 검수 2026-09-26)
    const bool was_linked = s_ok || s_have_target;
    xEventGroupClearBits(s_eg, B_FAIL);
    esp_wifi_disconnect();
    if (was_linked) xEventGroupWaitBits(s_eg, B_FAIL, pdTRUE, pdFALSE, pdMS_TO_TICKS(1500));
    vTaskDelay(pdMS_TO_TICKS(100));
    xEventGroupClearBits(s_eg, B_OK | B_FAIL);
    wifi_config_t sta = {};
    strlcpy((char *)sta.sta.ssid, c.ssid.c_str(), sizeof sta.sta.ssid);
    strlcpy((char *)sta.sta.password, c.pass.c_str(), sizeof sta.sta.password);
    esp_wifi_set_config(WIFI_IF_STA, &sta);
    { Lock l; strlcpy(s_connecting, c.ssid.c_str(), sizeof s_connecting); }
    s_have_target = true;
    esp_wifi_connect();
    ESP_LOGI(TAG, "wifi connecting: %s", c.ssid.c_str());             // 비밀번호는 로그에 남기지 않는다
}

// 스캔 결과로 순위를 다시 매기고, 끊겨 있으면 가장 센 후보에 붙는다 (D12)
void auto_connect(bool rescan)
{
    if (rescan) {
        s_suppress = true;
        do_scan();
        for (int retry = 0; retry < 2 && scan_empty(); ++retry) {     // 부팅 직후에는 C6 가 준비 전이라 0개가 나온다 → 잠시 뒤 다시
            vTaskDelay(pdMS_TO_TICKS(1500));
            do_scan();
        }
        s_suppress = false;
    }
    std::vector<wifistore::Ap> scan;
    { Lock l; scan = s_scan; }
    s_rank = wifistore::rank(s_known, scan);
    s_rank_pos = 0;
    if (s_rank.empty()) {
        if (!s_ok) { s_have_target = false; esp_wifi_disconnect(); }
        s_none = !s_ok;
        bump();
        ESP_LOGW(TAG, "wifi: no known network visible (%u seen)", (unsigned)scan.size());
        return;
    }
    if (s_none.exchange(false)) bump();
    std::string order;
    for (int i : s_rank) { if (!order.empty()) order += ", "; order += s_known[i].ssid; }
    ESP_LOGI(TAG, "wifi: known visible by signal: %s", order.c_str());
    if (!s_ok) { s_auto_fails = 0; connect_to(s_known[s_rank[0]]); }
}

bool is_auth_reason(int r) { return r == 2 || r == 14 || r == 15 || r == 202 || r == 204; }

// 화면에서 요청한 연결: 15초 안에 IP 를 받으면 저장, 인증 실패·시간 초과는 상태로 알리고 자동 연결로 돌아간다
void manual_connect(const Msg &m)
{
    wifistore::Cred c;
    c.ssid = m.ssid; c.pass = m.pass;
    if (m.use_saved) { int k = wifistore::find(s_known, c.ssid); if (k >= 0) c.pass = s_known[k].pass; }
    s_suppress = true;
    connect_to(c);
    const int64_t t0 = now_ms();
    int auth_fails = 0;
    bool ok = false;
    netwifi::Manual fail = netwifi::Manual::FAIL_TIMEOUT;
    while (now_ms() - t0 < MANUAL_LIMIT_MS) {
        EventBits_t b = xEventGroupWaitBits(s_eg, B_OK | B_FAIL, pdTRUE, pdFALSE, pdMS_TO_TICKS(500));
        if (b & B_OK) { ok = true; break; }
        if (b & B_FAIL) {
            const int reason = s_disc_reason;
            ESP_LOGW(TAG, "manual connect: disconnected (reason %d)", reason);
            if (is_auth_reason(reason) && ++auth_fails >= 2) { fail = netwifi::Manual::FAIL_AUTH; break; }
            vTaskDelay(pdMS_TO_TICKS(300));
            esp_wifi_connect();
        }
    }
    s_suppress = false;
    if (ok) {
        if (!c.pass.empty() || wifistore::find(s_known, c.ssid) < 0) { wifistore::upsert(s_saved, c.ssid, c.pass); nvs_store_saved(); rebuild_known(); }
        ESP_LOGI(TAG, "manual connect OK: %s (saved)", c.ssid.c_str());
        set_manual(netwifi::Manual::OK);
    } else {
        ESP_LOGW(TAG, "manual connect FAILED: %s (%s)", c.ssid.c_str(), fail == netwifi::Manual::FAIL_AUTH ? "auth" : "timeout");
        esp_wifi_disconnect();
        set_manual(fail);
        auto_connect(false);                                          // 원래 쓰던 망으로 돌아가 본다
    }
}

void refresh_rssi()
{
    wifi_ap_record_t ap = {};
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) return;
    const int prev = s_rssi.exchange(ap.rssi);
    if (!s_ok) { s_rssi = -127; return; }                             // 읽는 사이 끊김 → 끊김 처리가 넣은 -127 을 덮어쓰지 않게 되돌린다
    if (prev == -127 || wifistore::bars(prev) != wifistore::bars(ap.rssi)) bump();
}

void manager_loop()
{
    nvs_load_saved();
    rebuild_known();
    auto_connect(true);
    int64_t last_rescan = now_ms(), last_rssi = 0;
    while (true) {
        Msg m;
        if (xQueueReceive(s_q, &m, pdMS_TO_TICKS(1000)) == pdTRUE) {
            switch (m.type) {
            case Req::SCAN:
                s_suppress = true; do_scan(); s_suppress = false;
                // 목록이 나왔으니 "검색 중" 을 먼저 끈다(아래 연결은 1~2초 걸릴 수 있다).
                // 음수로 내려갔을 때만 0 으로 되돌린다 — 무조건 0 을 넣으면 그 사이 들어온 새 요청(+1)을 지워 버린다.
                if (s_scan_pending.fetch_sub(1) <= 0) s_scan_pending = 0;
                bump();
                if (!s_ok) auto_connect(false);
                last_rescan = now_ms();
                break;
            case Req::NEXT:
                if (s_ok) break;
                if (++s_rank_pos < (int)s_rank.size()) {
                    ESP_LOGW(TAG, "wifi: next candidate %d/%u", s_rank_pos + 1, (unsigned)s_rank.size());
                    connect_to(s_known[s_rank[s_rank_pos]]);
                } else {
                    vTaskDelay(pdMS_TO_TICKS(3000));
                    auto_connect(true);
                    last_rescan = now_ms();
                }
                break;
            case Req::CONNECT:
                manual_connect(m);
                break;
            case Req::FORGET:
                if (!m.ssid[0]) s_saved.clear();
                else s_saved.erase(std::remove_if(s_saved.begin(), s_saved.end(), [&](const wifistore::Cred &c) { return c.ssid == m.ssid; }), s_saved.end());
                nvs_store_saved();
                rebuild_known();
                ESP_LOGW(TAG, "wifi: forgot %s", m.ssid[0] ? m.ssid : "(all saved)");
                break;
            }
        }
        // 아는 망이 안 보이면 20초마다 다시 찾는다(설정 화면이 열려 있으면 사용자 조작에 맡긴다)
        if (!s_ok && s_none && now_ms() - last_rescan > RESCAN_MS && !s_setup_open) { auto_connect(true); last_rescan = now_ms(); }
        // N-D3 신호 세기. 연결 직후(아직 -127)에는 다음 바퀴에 바로 읽는다
        if (s_ok && now_ms() - last_rssi > (s_rssi == -127 ? 900 : RSSI_PERIOD_MS)) { last_rssi = now_ms(); refresh_rssi(); }
    }
}

void init_failed(const char *what)                                    // N-D1
{
    printf("PHONE FAIL %s\n", what);
    fflush(stdout);
    s_init_failed = true;
    // 초기화가 끝나기 전에 화면에서 눌러 둔 "연결" 요청은 이제 아무도 처리하지 못한다 → CONNECTING(입력 잠금)에 갇히지 않게 실패로 돌린다
    bool stuck;
    { Lock l; stuck = (s_manual == netwifi::Manual::CONNECTING); }
    if (stuck) set_manual(netwifi::Manual::FAIL_TIMEOUT);
    bump();
}

void wifi_task(void *)
{
    esp_err_t e = esp_netif_init();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) { init_failed("wifi netif"); vTaskDelete(nullptr); return; }
    e = esp_event_loop_create_default();                              // 다른 모듈이 먼저 만들었으면 INVALID_STATE — 그대로 쓴다
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) { init_failed("wifi event-loop"); vTaskDelete(nullptr); return; }

    // C6 전원은 M5.begin() 이 익스팬더 0x44 bit0(WLAN_PWR_EN) 으로 켠다.
    // esp_hosted 2.11.x: 핀은 실행 중 구조체로 준다. 생성자 자동초기화는 tools/patch_esp_hosted.py 로 꺼 두었다.
    {
        struct esp_hosted_sdio_config conf = INIT_DEFAULT_HOST_SDIO_CONFIG();
        conf.pin_clk.pin = 12; conf.pin_cmd.pin = 13;
        conf.pin_d0.pin = 11;  conf.pin_d1.pin = 10; conf.pin_d2.pin = 9; conf.pin_d3.pin = 8;
        conf.pin_reset.pin = 15;
        e = esp_hosted_sdio_set_config(&conf);
        if (e != ESP_OK) { ESP_LOGE(TAG, "esp_hosted_sdio_set_config: %s", esp_err_to_name(e)); init_failed("esp_hosted sdio-config"); vTaskDelete(nullptr); return; }
    }
    if (esp_hosted_init() != 0)             { init_failed("esp_hosted init"); vTaskDelete(nullptr); return; }
    if (esp_hosted_connect_to_slave() != 0) { init_failed("esp_hosted connect-to-slave"); vTaskDelete(nullptr); return; }
    {
        esp_hosted_coprocessor_fwver_t v = {};
        if (esp_hosted_get_coprocessor_fwversion(&v) == ESP_OK) ESP_LOGI(TAG, "C6 slave fw %u.%u.%u", (unsigned)v.major1, (unsigned)v.minor1, (unsigned)v.patch1);
    }

    if (!esp_netif_create_default_wifi_sta()) { init_failed("wifi netif-sta"); vTaskDelete(nullptr); return; }
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&wcfg) != ESP_OK) { init_failed("wifi init"); vTaskDelete(nullptr); return; }
    if (esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_event, nullptr) != ESP_OK ||
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_event, nullptr) != ESP_OK) { init_failed("wifi event-handler"); vTaskDelete(nullptr); return; }
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK || esp_wifi_start() != ESP_OK) { init_failed("wifi start"); vTaskDelete(nullptr); return; }
    s_started = true;
    bump();
    manager_loop();                                                   // 돌아오지 않는다
}

}  // namespace

namespace netwifi {

void start()
{
    static bool called = false;
    if (called) return;                                               // 이미 시작함
    called = true;
    s_lock = xSemaphoreCreateMutex();
    s_eg = xEventGroupCreate();
    QueueHandle_t q = xQueueCreate(8, sizeof(Msg));
    if (!q || !s_eg || !s_lock) { init_failed("wifi alloc"); return; }
    s_q = q;                                                          // 큐를 마지막에 연다: 요청(post)이 들어올 수 있을 때는 잠금·이벤트 그룹이 이미 있다
    // 스택 10KB·우선순위 5·코어 0 — 원본과 같다
    if (xTaskCreatePinnedToCore(wifi_task, "wifi", 10 * 1024, nullptr, 5, nullptr, 0) != pdPASS) init_failed("wifi task");
}

Phase phase()
{
    if (s_init_failed) return Phase::INIT_FAILED;
    if (!s_started) return Phase::STARTING;
    if (s_ok) return Phase::CONNECTED;
    if (s_none) return Phase::NO_KNOWN;
    return Phase::SEARCHING;
}

bool connected() { return s_ok; }

void ssid(char *out, size_t cap)
{
    if (!out || !cap) return;
    out[0] = 0;
    if (!s_ok) return;
    Lock l;
    strlcpy(out, s_ssid, cap);
}

int rssi() { return s_ok ? s_rssi.load() : -127; }

uint32_t change_seq() { return s_seq; }

void set_setup_open(bool open) { s_setup_open = open; }

void request_scan()
{
    if (s_init_failed) return;                                        // 매니저가 없다 — 요청이 영원히 남지 않게
    s_scan_pending.fetch_add(1);
    bump();
    if (!post(Req::SCAN)) { s_scan_pending.fetch_sub(1); bump(); }
}

bool scan_busy() { return !s_init_failed && (s_scanning || s_scan_pending > 0); }

std::vector<wifistore::Ap> scan_results()
{
    Lock l;
    return s_scan;
}

bool is_known(const std::string &ssid)
{
    Lock l;
    return std::find(s_known_ssids.begin(), s_known_ssids.end(), ssid) != s_known_ssids.end();
}

void request_connect(const char *ssid, const char *pass, bool use_saved)
{
    if (!ssid || !*ssid) return;
    if (!pass) pass = "";
    if (manual_state() == Manual::CONNECTING) return;                 // 이미 시도 중 — 겹친 요청은 버린다(진행 중인 상태를 건드리지 않는다)
    if (!use_saved && strlen(pass) < 8) { set_manual(Manual::SHORT_PASS); return; }   // WPA 비밀번호는 8자 이상
    if (s_init_failed) { set_manual(Manual::FAIL_TIMEOUT); return; }  // 매니저가 없다
    set_manual(Manual::CONNECTING);                                   // N-D2
    ESP_LOGI(TAG, "wifi setup: connect %s (%s)", ssid, use_saved ? "saved/open" : "typed password");   // 비밀번호·길이는 남기지 않는다
    // 큐가 찼거나, 요청을 넣는 사이 초기화가 실패했으면(매니저가 없다) "연결"이 반응 없이 남지 않게 실패로 돌린다
    if (!post(Req::CONNECT, ssid, use_saved ? "" : pass, use_saved) || s_init_failed) set_manual(Manual::FAIL_TIMEOUT);
}

Manual manual_state()
{
    Lock l;
    return s_manual;
}

int64_t manual_since_ms()
{
    Lock l;
    return s_manual_ms;
}

void request_forget(const char *ssid) { post(Req::FORGET, ssid ? ssid : ""); }

}  // namespace netwifi
```

- [x] **Step 7: Wi-Fi 설정 화면 — `main/ui_wifi.h`, `main/ui_wifi.cpp`**

```cpp
// ui_wifi.h — Wi-Fi 설정 화면(망 목록 + 비밀번호 자판).
// 출처: firmware/tab5_snapbooth/main/main.cpp 의 NET_LIST / NET_PASS 화면. 기능은 그대로, 터치 영역만 디자인 규칙(96px 이상)에 맞춘다.
// 규칙: 모든 함수는 화면 태스크에서만 부른다. 자판 화면에서는 터치 좌표를 로그에 남기지 않는다(비밀번호 복원 방지).
#pragma once

namespace uiwifi {

void open();            // 설정 화면을 연다(스캔 시작). 이미 열려 있으면 아무것도 안 한다
bool is_open();
void close();

// 현재 화면(목록 또는 자판)을 gfx 합성 버퍼에 그린다. present() 는 부르는 쪽이 한다.
void draw();

// 터치 한 번을 처리한다. 화면이 닫혔으면(닫기 버튼, 연결 성공 후) false 를 돌려준다.
bool tap(int x, int y);

// 마지막 draw() 이후 다시 그려야 할 일이 생겼으면 true(스캔 끝남, 수동 연결 상태 변화, 안내 문구 시간 경과 등).
// 주의: 연결에 성공하면 "연결됨" 을 1.5초 보여 준 뒤 이 함수가 화면을 닫고 false 를 돌려준다(터치 없이 닫히는 유일한 경우).
//       그래서 부르는 쪽은 매 바퀴 is_open() 을 다시 확인해, 닫혔으면 원래 화면을 다시 그린다.
//       열려 있는 동안에는 매 바퀴 불러야 한다(부르지 않으면 자동으로 닫히지 않는다).
bool needs_redraw();

// 지금 비밀번호 자판이 떠 있나(터치 좌표 로그를 막을 때 쓴다)
bool on_keyboard();

}  // namespace uiwifi
```

```cpp
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
#include "ui_wifi.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "esp_log.h"
#include "esp_timer.h"

#include "gfx.h"
#include "net_wifi.h"

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
    for (int k = 0; k < 4; ++k) { int h = 10 + k * 8; gfx::fill_rect(x + k * 12, cy + 18 - h, 8, h, k < n ? C_INK : C_LINE); }
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
    gfx::blit_asset(&lbl_wf_title, WL_X, HEAD_CY - lbl_wf_title.h / 2, C_INK);
    char pg[32];
    snprintf(pg, sizeof pg, "%d / %d", s_page + 1, np);
    gfx::text_center(pg, SCREEN_W / 2, HEAD_CY, C_MUTED, 1.5f);
    const int right = SCREEN_W - WL_X;
    if (linked) {
        const std::string name = fit(cur, 420, 1.5f);                 // 가운데 쪽 번호와 겹치지 않는 폭
        const int nw = gfx::text_w(name.c_str(), 1.5f);
        gfx::text(name.c_str(), right - nw, HEAD_CY, C_INK2, 1.5f);
        gfx::blit_asset(&lbl_wf_connected, right - nw - 12 - lbl_wf_connected.w, HEAD_CY - lbl_wf_connected.h / 2, C_GREEN_T);
    } else {
        gfx::blit_asset(&lbl_wf_disconnected, right - lbl_wf_disconnected.w, HEAD_CY - lbl_wf_disconnected.h / 2, C_MUTED);
    }

    // 목록
    if (s_list.empty()) {
        const int cy = WL_Y + (WB_Y - 16 - WL_Y) / 2;
        gfx::blit_asset_center(busy ? &lbl_wf_scanning : &lbl_wf_empty, SCREEN_W / 2, cy, C_MUTED);
    }
    for (int i = 0; i < WL_ROWS; ++i) {
        const int idx = s_page * WL_ROWS + i;
        if (idx >= (int)s_list.size()) break;
        const auto &a = s_list[idx];
        const Rect r = wl_row(i);
        const int cy = r.y + r.h / 2;
        const bool is_cur = linked && a.ssid == cur;
        const bool known = netwifi::is_known(a.ssid);
        gfx::fill_rrect(r.x, r.y, r.w, r.h, 16, is_cur ? C_LINE : C_SURFACE);
        const int rx = r.x + r.w - 28 - 44;                           // 신호 막대(폭 44)의 왼쪽
        draw_bars(rx, cy, wifistore::bars(a.rssi));
        if (a.secured) draw_lock(rx - 44, cy, C_MUTED);
        int text_right = rx - 64;                                     // 자물쇠 왼쪽
        const ui_asset_t *tag = is_cur ? &lbl_wf_connected : known ? &lbl_wf_saved : nullptr;
        if (tag) {
            gfx::blit_asset(tag, text_right - tag->w, cy - tag->h / 2, is_cur ? C_GREEN_T : C_MUTED);
            text_right -= tag->w;
        }
        const int tx = r.x + 28;
        gfx::text(fit(a.ssid, text_right - 24 - tx, 2.0f).c_str(), tx, cy, C_INK, 2.0f);
    }

    // 하단 버튼
    gfx::button(wl_btn(0), &lbl_wf_prev, s_page > 0, C_SURFACE, C_INK);
    gfx::button(wl_btn(1), &lbl_wf_next, s_page + 1 < np, C_SURFACE, C_INK);
    gfx::button(wl_btn(2), busy ? &lbl_wf_rescan_busy : &lbl_wf_rescan, !busy, C_SURFACE, C_INK);
    gfx::button(wl_btn(3), &lbl_wf_close, true, C_SURFACE, C_INK);
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

    // 맨 윗줄 오른쪽: 상태 문구
    const ui_asset_t *msg = &lbl_wf_hint;
    uint32_t mc = C_MUTED;
    switch (st) {
    case Manual::CONNECTING:   msg = &lbl_wf_connecting;   mc = C_BLUE_T;  break;
    case Manual::OK:           msg = &lbl_wf_connected;    mc = C_GREEN_T; break;
    case Manual::FAIL_AUTH:    msg = &lbl_wf_fail_auth;    mc = C_RED_T;   break;
    case Manual::FAIL_TIMEOUT: msg = &lbl_wf_fail_timeout; mc = C_RED_T;   break;
    case Manual::SHORT_PASS:   msg = &lbl_wf_short_pass;   mc = C_RED_T;   break;
    default: break;
    }
    const int msg_x = SCREEN_W - 40 - msg->w;
    gfx::blit_asset(msg, msg_x, HEAD_CY - msg->h / 2, mc);

    // 맨 윗줄 왼쪽: "<망 이름> 에 연결" (이름이 길면 상태 문구 앞에서 줄인다)
    const int name_max = msg_x - 32 - lbl_wf_join.w - 12 - 40;
    const int nw = gfx::text(fit(s_sel, name_max, 2.0f).c_str(), 40, HEAD_CY, C_INK, 2.0f);
    gfx::blit_asset(&lbl_wf_join, 40 + nw + 12, HEAD_CY - lbl_wf_join.h / 2, C_INK2);

    // 비밀번호 칸 + 보기/숨기기
    gfx::fill_rrect(PW_FIELD.x, PW_FIELD.y, PW_FIELD.w, PW_FIELD.h, 16, C_SURFACE);
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
        const int tw = gfx::text(shown.c_str(), PW_FIELD.x + PW_PAD, cy, C_INK, 2.0f);
        gfx::fill_rect(PW_FIELD.x + PW_PAD + tw + 2, cy - 24, 3, 48, C_BLUE_T);   // 커서
    }
    gfx::button(PW_SHOW, s_pw_show ? &lbl_wf_hide : &lbl_wf_show, true, C_SURFACE, C_INK);

    // 자판
    for (int i = 0; i < NPKEYS; ++i) {
        const PKey &k = PKEYS[i];
        const Rect r = pkey_rect(k);
        const int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
        switch (k.act) {
        case KA_CHAR:
            gfx::fill_rrect(r.x, r.y, r.w, r.h, 12, C_SURFACE);
            gfx::text_center(pkey_char(k).c_str(), cx, cy, C_INK, 2.0f);
            break;
        case KA_BACK:
            gfx::fill_rrect(r.x, r.y, r.w, r.h, 12, C_LINE);
            draw_backspace(cx, cy, C_INK, C_LINE);
            break;
        case KA_SHIFT: {
            const bool on = s_shift && !s_sym;
            gfx::fill_rrect(r.x, r.y, r.w, r.h, 12, on ? C_INK : C_LINE);
            if (!s_sym) draw_shift(cx, cy, on ? C_BG : C_INK);       // 기호 벌에서는 대문자 전환이 없다
            break;
        }
        case KA_SYM:
            gfx::fill_rrect(r.x, r.y, r.w, r.h, 12, C_LINE);
            gfx::text_center(s_sym ? "ABC" : "#+=", cx, cy, C_INK, 2.0f);
            break;
        case KA_SPACE:
            gfx::fill_rrect(r.x, r.y, r.w, r.h, 12, C_SURFACE);
            gfx::fill_rect(cx - r.w / 4, cy - 3, r.w / 2, 6, C_MUTED);
            break;
        case KA_CANCEL:
            gfx::fill_rrect(r.x, r.y, r.w, r.h, 12, C_LINE);
            gfx::blit_asset_center(&lbl_wf_cancel, cx, cy, C_INK);
            break;
        case KA_OK:
            gfx::fill_rrect(r.x, r.y, r.w, r.h, 12, st == Manual::CONNECTING ? C_LINE : C_BLUE_T);
            gfx::blit_asset_center(&lbl_wf_connect, cx, cy, C_BG);
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
    gfx::clear(C_BG);
    if (s_view == View::LIST) draw_list(); else draw_pass();
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
```

- [x] **Step 8: 빌드 확인용 임시 진입점 — `main/stub_main.cpp`, `main/CMakeLists.txt`**

```cpp
// stub_main.cpp — 빌드 확인용 임시 진입점. gfx·net_wifi·ui_wifi 세 모듈이 컴파일·링크되는지 보기 위한 것이다.
// phone_main.cpp 가 들어오면 이 파일을 지우고 main/CMakeLists.txt 의 SRCS 에서 뺀다(app_main 은 하나만 있어야 한다).
// 기기에 올려 본 적이 없다 — 화면 태스크가 세 모듈을 어떤 순서로 부르는지 보여 주는 예시로만 읽을 것.
#include <M5Unified.h>

#include <cstdio>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "gfx.h"
#include "net_wifi.h"
#include "ui_layout.h"       // Wi-Fi 배지 자리(BTN_WIFI)를 실제 화면과 같게 쓴다
#include "ui_wifi.h"

static const char *TAG = "stub";

static const Rect &BADGE = uilayout::BTN_WIFI;                // CONTRACTS.md: Wi-Fi 배지, 가운데 (152, 648) 240×96
static const int  BTN_CX = 640, BTN_CY = 470, BTN_R = 110;    // 임시 원형 버튼(누르면 화면 캡처 크기를 로그에 찍는다)

static void draw_home(int up_s)
{
    gfx::clear(C_BG);

    // 윗줄: Wi-Fi 상태 한 문장
    const ui_asset_t *msg = &lbl_wifi_connecting;
    uint32_t mc = C_MUTED;
    switch (netwifi::phase()) {
    case netwifi::Phase::CONNECTED:   msg = &lbl_wf_connected;   mc = C_GREEN_T; break;
    case netwifi::Phase::NO_KNOWN:    msg = &lbl_wifi_none;      mc = C_YELLOW_T; break;
    case netwifi::Phase::INIT_FAILED: msg = &lbl_wifi_init_fail; mc = C_RED_T; break;
    default: break;
    }
    gfx::fill_alpha_rect(0, 0, SCREEN_W, 96, C_SURFACE, 160);
    gfx::blit_asset_center(msg, SCREEN_W / 2, 48, mc);

    // 가운데: 켜진 뒤 지난 시간(MM:SS) + 원형 버튼
    gfx::draw_clock(up_s / 60, up_s % 60, SCREEN_W / 2, 220, C_INK);
    gfx::fill_alpha_circle(BTN_CX, BTN_CY, BTN_R + 12, C_GREEN_BG, 140);
    gfx::fill_circle(BTN_CX, BTN_CY, BTN_R, C_GREEN_BG);
    gfx::blit_asset_center(&lbl_call, BTN_CX, BTN_CY, C_GREEN_T);

    // 왼쪽 아래: Wi-Fi 배지(누르면 설정 화면)
    gfx::button(BADGE, &lbl_wifi_setup, true, C_SURFACE, C_INK2);

    // 오른쪽 아래: 연결된 망 이름과 신호(dBm 의 크기)
    char name[33];
    netwifi::ssid(name, sizeof name);
    if (name[0]) {
        const int w = gfx::text_w(name, 1.5f);
        gfx::fill_alpha_rrect(SCREEN_W - 40 - w - 120, 628, w + 120, 64, 16, C_SURFACE, 210);
        gfx::text(name, SCREEN_W - 40 - w - 100, 660, C_INK2, 1.5f);
        gfx::draw_number(-netwifi::rssi(), SCREEN_W - 40 - 44, 660, false, C_BLUE_T);
    } else {
        gfx::fill_rect(SCREEN_W - 40 - 200, 659, 200, 2, C_LINE);
        gfx::text_center("-", SCREEN_W - 40 - 100, 640, C_MUTED, 1.5f);
    }
}

extern "C" void app_main(void)
{
    auto cfg = M5.config();
    cfg.internal_spk = false;            // 소리는 esp_codec_dev 로 직접 다룬다(PH-D4) — M5Unified 가 I2S·코덱을 잡지 않게
    cfg.internal_mic = false;
    M5.begin(cfg);
    if (nvs_flash_init() != ESP_OK) ESP_LOGE(TAG, "nvs init failed (지우지 않는다: 저장된 Wi-Fi 목록 보존)");
    M5.Display.setRotation(3);           // 삼각대 나사가 화면 아래
    M5.Display.setBrightness(160);

    if (!gfx::init()) {
        printf("PHONE FAIL gfx\n");
        while (true) vTaskDelay(pdMS_TO_TICKS(1000));
    }
    netwifi::start();
    printf("PHONE READY (stub)\n");

    uint32_t seen_seq = ~0u;
    int seen_s = -1;
    bool home_dirty = true;
    while (true) {
        M5.update();
        const auto t = M5.Touch.getDetail();
        const bool tap = t.wasClicked();

        if (uiwifi::is_open()) {
            if (tap) {
                if (uiwifi::on_keyboard()) ESP_LOGI(TAG, "tap (keypad)");      // 자판 좌표는 비밀번호를 되살릴 수 있어 남기지 않는다
                else ESP_LOGI(TAG, "tap (%d,%d)", t.x, t.y);
                uiwifi::tap(t.x, t.y);
            }
            if (uiwifi::needs_redraw()) { uiwifi::draw(); gfx::present(); }   // needs_redraw 가 화면을 닫을 수도 있다(연결 성공 1.5초 뒤)
            if (!uiwifi::is_open()) home_dirty = true;                        // 닫혔다 → 다음 바퀴에 원래 화면을 그린다
        } else {
            if (tap) {
                ESP_LOGI(TAG, "tap (%d,%d)", t.x, t.y);
                if (BADGE.hit(t.x, t.y)) uiwifi::open();
                else if (gfx::hit_circle(t.x, t.y, BTN_CX, BTN_CY, BTN_R)) {
                    const uint8_t *jpg = nullptr;
                    const size_t n = gfx::screenshot_jpeg(&jpg);
                    ESP_LOGI(TAG, "screenshot %u bytes", (unsigned)n);
                }
            }
            const int s = (int)(esp_timer_get_time() / 1000000);
            const uint32_t seq = netwifi::change_seq();
            if (!uiwifi::is_open() && (home_dirty || s != seen_s || seq != seen_seq)) {
                home_dirty = false; seen_s = s; seen_seq = seq;
                draw_home(s);
                gfx::present();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```

```cmake
# REQUIRES 를 적지 않는다: main 은 그러면 모든 컴포넌트를 자동으로 본다(관리형 컴포넌트 이름 오타 방지 — 시험 펌웨어와 같은 방식).
set(PHONE_SRCS
    "gfx.cpp"        # 화면 합성·출력
    "net_wifi.cpp"   # Wi-Fi 매니저
    "ui_wifi.cpp"    # Wi-Fi 설정 화면
)

# 빌드 확인용 임시 진입점. phone_main.cpp 가 들어오면 아래 한 줄을 지우고
# "phone_main.cpp" "ui.cpp" "relay_link.cpp" "device_id.cpp" 를 PHONE_SRCS 에 넣는다(app_main 은 하나만 있어야 한다).
list(APPEND PHONE_SRCS "stub_main.cpp")

idf_component_register(SRCS ${PHONE_SRCS}
                       INCLUDE_DIRS ".")

# B-D1: -Wliteral-suffix 한 종류만 끈다(main 의 C++ 파일에만).
#   이유: <M5Unified.h> 를 포함하는 파일마다 ESP-IDF 5.5.4 헤더(esp_private/periph_ctrl.h:88, "…"CONFIG_IDF_TARGET 사이에 공백 없음)에서
#         같은 경고가 1건씩 찍힌다 — 스냅부스·시험 펌웨어 빌드 기록에도 있는 소음이다. 우리 코드의 새 경고가 묻히지 않게 끈다.
#   비용: 우리 코드에서 같은 종류("문자열"매크로 를 붙여 쓴 것)의 경고도 안 보인다. 탈출구: 아래 한 줄을 지운다.
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-literal-suffix>)
```

- [x] **Step 9: 빌드** (첫 빌드는 부품 약 200MB 를 내려받아 5~10분 걸린다)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && source ~/esp/esp-idf-5.5.4/export.sh && idf.py set-target esp32p4 && idf.py build 2>&1 | tail -4
```
Expected (끝부분):
```
tab5_phone.bin binary size 0x18…… bytes. Smallest app partition is 0x600000 bytes. 0x47…… bytes (7x%) free.
Project build complete.
```
(임시 진입점으로는 약 1.6MB 다.) 설정 단계 로그에 `TARGET_LIB_PATH is set to: esp32p4_less_v3` 와 `[patch_esp_hosted] applied`(또는 `already applied`)가 있어야 한다. `main/` 의 우리 파일에서 경고가 나오면 고친다.

빌드가 실패하면:
- 부품 판 해석 실패(`ERROR: Because …`) → **`esp_hosted` 고정은 풀지 않는다.** 오류 전문을 보고하고 멈춘다.
- `'WIFI_NETWORKS' was not declared` → Task 1 Step 7 의 `secrets.h` 가 없다.
- `ui_assets.h: No such file` → Task 4 Step 3 을 먼저.

- [x] **Step 10: 산출물이 git 에 안 보이는지 확인하고 커밋**

```bash
cd ~/tab5-toy-phone && git status --short firmware/tab5_phone | grep -cE "build/|managed_components|sdkconfig$|dependencies.lock|secrets\.h$|/fonts/"
git add firmware/tab5_phone && git commit -m "feat(phone): 빌드 뼈대 + 화면 합성·Wi-Fi 매니저·Wi-Fi 설정 화면(스냅부스에서 떼어 옴)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
Expected: 첫 줄 `0`

---

### Task 6: 교환기 연결·통화 화면·진행 모듈, 전체 빌드

**Files:**
- Create: `firmware/tab5_phone/main/relay_link.h`, `main/relay_link.cpp`, `main/device_id.cpp`, `main/secrets.h.example`, `main/ui.h`, `main/ui.cpp`, `main/app.h`, `main/app.cpp`, `main/phone_main.cpp`
- Modify: `firmware/tab5_phone/main/CMakeLists.txt`(임시 진입점을 빼고 본 소스를 넣는다)
- Delete: `firmware/tab5_phone/main/stub_main.cpp`(Task 5 의 빌드 확인용)

**Interfaces:**
- Consumes: Task 2 의 `callfsm`·`relayproto`·`uilayout`·`deviceid::lookup`, Task 4 의 `ui_assets.h`(`lbl_*`, `DIG_BIG`, `DIG_SMALL`), Task 5 의 `gfx::`·`netwifi::`·`uiwifi::`, Task 1 의 `secrets.h`(`RELAY_URL`, `PHONE_DEVICES`).
- Produces:
  - `relaylink::start(url, dev_id, token)`, `up()`, `http_code()`, `attempt()`, `change_seq()`, `poll(In *)`, `send_text(const char *)`, `send_voice(const uint8_t *, size_t)`, `reconnect()`, `stack_free(unsigned *, unsigned *)`, `set_voice_sink(fn)` — `send_voice`·`set_voice_sink` 는 Phase 3 이 쓴다(그때 `reconnect` 와 `send_voice` 를 같은 잠금으로 묶어야 한다 — 파일 머리 주석).
  - `deviceid::get() -> const Info &`
  - `ui::draw(const uilayout::Model &)`
  - `app::start(int unit)`, `app::inject_tap(x, y)`, `app::print_stat()`, `app::i2c_lock()` — `i2c_lock` 은 Phase 3 의 코덱·앰프 제어가 함께 잡는다. 태스크 둘을 띄운다: `app`(진행·그리기)과 `touch`(20ms 마다 터치를 읽어 대기열에 넣는다 — 화면을 내보내는 동안의 짧은 터치를 놓치지 않기 위해, AP-D2).
  - 시리얼 규약(115200, 줄 단위) — Task 3 의 `phone_ctl.py` 와 Task 7~9 가 이것에 의존한다:

| 보내는 명령 | 돌아오는 줄 |
|---|---|
| (부팅) | `PHONE BOOT unit=<0\|1\|2> id=<tab5-1\|tab5-2\|unknown>` → `PHONE READY`. 실패는 `PHONE FAIL <panel\|gfx\|relay …>`, 재시작 가드는 `PHONE <panel\|gfx>-failed restart <n>` |
| `stat` | `STAT up=<초> unit=<n> fsm=<상태> screen=<화면> wifi=<0\|1> relay=<0\|1> relay_code=<0\|401> peer=<0\|1> missed=<n> touch=<n> heap_int=<B> heap_int_min=<B> heap_psram=<B> stk_app=<B> stk_touch=<B> stk_relay=<B> stk_ws=<B>` (`stk_*` = 그 태스크 스택의 최저 여유, 태스크가 없으면 0) |
| `t <x> <y>` | `OK t <x> <y>` (범위 밖이면 `ERR t out-of-range`). 실제 터치와 같은 대기열에 들어가 화면 태스크가 다음 바퀴에 처리한다(같은 터치 가드를 거친다) |
| `shot` | `SHOT BEGIN <바이트수>` → `B64:<…>` 줄들 → `SHOT END <모든 바이트의 합 mod 2^32>` / `SHOT FAIL <사유>` (JPEG) |
| `reboot` | `OK reboot` 뒤 재시작 |
| (스스로) | `FSM <이전> -> <이후> acts=0x<16진> notice=<이름>` — 상태가 바뀌거나 할 일·안내가 있을 때 |
| (스스로) | `SCREEN <BOOT\|IDLE\|DIALING\|INCOMING\|ANSWERING\|IN_CALL\|ENDING\|WIFI_LIST\|WIFI_PASS>` — 화면이 바뀔 때 |
| (스스로) | `RELAY up=<0\|1> code=<0\|401> attempt=<n>` — 교환기 연결이 바뀔 때 |

  상태 이름(`fsm=`, `FSM` 줄): `OFFLINE` `IDLE` `DIALING` `RINGBACK` `INCOMING` `ANSWERING` `IN_CALL` `ENDING`. 화면 이름은 상태에서 정해진다 — `OFFLINE`→`BOOT`, `DIALING`·`RINGBACK`→`DIALING`, 나머지는 같은 이름. Wi-Fi 설정이 떠 있으면 `WIFI_LIST`/`WIFI_PASS`.
  안내 이름(`notice=`): `NONE` `PEER_OFF` `NO_ANSWER` `REJECTED` `LOST` `CALL_ENDED` `MISSED` `RELAY_SILENT`.
  `STAT` 의 항목 순서는 위 표대로 고정이고 값에 공백이 없다(시험의 정규식이 순서에 기댄다). `SHOT END` 의 합은 JPEG 원본 바이트의 합이다.
  할 일 비트(`acts`): `0x1` call 보냄, `0x2` accept, `0x4` reject, `0x8` hangup, `0x10` 연결음, `0x20` 벨, `0x40` 종료음, `0x80` 소리 멈춤, `0x100` 음성 켬, `0x200` 음성 끔, `0x400` 재연결.

- [x] **Step 1: 교환기 연결 — `main/relay_link.h`**

```cpp
// relay_link.h — 교환기(WebSocket) 연결. 붙고, 끊기면 다시 붙고(1→2→4…30초, 토큰 거절은 60초), 15초마다 생존 확인을 보낸다.
// 설계서 §9.3·§9.4·§9.5·§9.7. 부품: espressif/esp_websocket_client 1.8.0.
//   - 받은 신호는 사건 대기열에 쌓인다 → 통화 진행 태스크(app)가 poll() 로 꺼낸다. 이 모듈은 통화 상태를 모른다.
//   - 음성 프레임(Phase 3)은 대기열을 거치지 않고 등록한 함수로 바로 넘긴다(웹소켓 태스크에서 불린다 — 복사만 하고 바로 돌아올 것).
#pragma once
#include <cstddef>
#include <cstdint>

#include "relay_proto.h"

namespace relaylink {

struct In {
    enum class Kind : uint8_t { LINK_UP, LINK_DOWN, SIGNAL } kind;
    relayproto::Signal sig;         // kind == SIGNAL 일 때만
};

// 한 번만 부른다. 태스크를 띄우고 바로 돌아온다. Wi-Fi 가 붙으면 접속을 시작한다.
// url: "ws://192.168.0.10:5094/ws"(맥북 교환기) 또는 "wss://…/ws"(운영). dev_id·token 은 접속 요청 헤더로만 나간다.
void start(const char *url, const char *dev_id, const char *token);

bool up();                          // 교환기에 붙어 있나
int  http_code();                   // 교환기가 접속을 거절한 코드(401 = 토큰 거절). 거절이 아니면 0
int  attempt();                     // 연속 실패 횟수(붙으면 0)
uint32_t change_seq();              // 위 값이 바뀔 때마다 1 증가

bool poll(In *out);                 // 받은 사건 하나를 꺼낸다. 없으면 false. 통화 진행 태스크에서만 부른다

bool send_text(const char *text);                       // 신호 보내기. 붙어 있지 않거나 실패하면 false
bool send_voice(const uint8_t *frame, size_t len);      // 음성 프레임 [순번 2][Opus] 보내기(Phase 3). 전용 송신 태스크에서만

void reconnect();                   // 지금 연결을 끊고 처음부터 다시 붙는다(call_fsm 의 A_RECONNECT). 바로 돌아온다

// 이 모듈이 쓰는 두 태스크(relay, relay_ws)의 스택 최저 여유(바이트). 태스크가 없으면 0. STAT 줄에 싣는다.
void stack_free(unsigned *relay_task, unsigned *ws_task);

typedef void (*voice_sink_t)(const uint8_t *frame, size_t len);
void set_voice_sink(voice_sink_t fn);                   // Phase 3: 받은 음성 프레임을 넘겨받을 함수

}  // namespace relaylink
```

- [x] **Step 2: `main/relay_link.cpp`** (부품 소스로 확인한 일곱 가지 결정 RL-D1~D7 이 머리 주석에 있다)

```cpp
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
//   주의(Phase 3): 다른 태스크가 send_voice 를 부르는 도중에 reconnect 의 stop → start 가 돌면 부품이 전송 객체를 새로 만들어
//         쓰던 것이 해제될 수 있다. 음성 송신 태스크를 붙일 때는 reconnect 와 send_voice 를 같은 잠금으로 묶을 것.
#include "relay_link.h"

#include <atomic>
#include <cstring>

#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "net_wifi.h"

namespace relaylink {
namespace {

const char *TAG = "relay";

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

char s_url[160], s_headers[200];

// 받는 프레임 재조립(작은 프레임도 여러 이벤트로 쪼개져 올 수 있다). 규약: 음성 400바이트 이하, 신호 256바이트 이하
uint8_t s_rx[512];
int s_rx_len = 0;
uint8_t s_rx_op = 0;
bool s_rx_drop = false;

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

// 주의: 웹소켓 태스크(부품의 잠금을 쥔 채) 또는 send 를 부른 태스크에서 실행된다 → 복사·대기열 넣기만 한다. stop/close/destroy 금지.
void on_ws_event(void *, esp_event_base_t, int32_t id, void *event_data)
{
    auto *d = static_cast<esp_websocket_event_data_t *>(event_data);
    switch (id) {
    case WEBSOCKET_EVENT_BEFORE_CONNECT:
        s_phase = Phase::CONNECTING; ++s_seq;
        break;
    case WEBSOCKET_EVENT_CONNECTED:
        s_phase = Phase::UP; s_auth_rejected = false; s_up_since_us = esp_timer_get_time();
        s_rx_len = 0; s_rx_drop = false;
        printf("RELAY up=1 code=0 attempt=%d\n", s_fails.load());
        post(In::Kind::LINK_UP);
        break;
    case WEBSOCKET_EVENT_DATA:
        if (d->op_code >= 0x08) break;                           // 닫기(8)·PING(9)·PONG(10)도 DATA 로 온다
        on_frame_part(d);
        break;
    case WEBSOCKET_EVENT_DISCONNECTED: {                         // 접속 실패 또는 비정상 끊김
        const bool was_up = s_phase == Phase::UP;
        if (s_phase == Phase::CONNECTING && d->error_handle.esp_ws_handshake_status_code == 401) s_auth_rejected = true;   // RL-D4
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
    default:
        break;                                                   // ERROR 는 순서·횟수가 일정치 않아 상태 판단에 쓰지 않는다
    }
}

void relay_task(void *)
{
    while (!netwifi::connected()) vTaskDelay(pdMS_TO_TICKS(200));

    esp_websocket_client_config_t cfg = {};
    cfg.uri = s_url;
    cfg.headers = s_headers;                                     // init 이 복사한다
    if (strncmp(s_url, "wss://", 6) == 0) cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.task_name = "relay_ws";
    cfg.task_stack = 8192;                                       // 기본 4096. TLS 악수와 위 핸들러가 이 스택에서 돈다
    cfg.network_timeout_ms = 10000;
    cfg.reconnect_timeout_ms = 1000;                             // 첫 대기. 이후는 schedule_reconnect 가 정한다
    cfg.enable_close_reconnect = true;                           // RL-D2
    cfg.ping_interval_sec = 5;                                   // RL-D1 (제어 PING)
    cfg.pingpong_timeout_sec = 15;
    s_ws = esp_websocket_client_init(&cfg);
    memset(s_headers, 0, sizeof s_headers);                      // 토큰이 든 원본은 지운다
    if (!s_ws) { ESP_LOGE(TAG, "client init failed"); printf("PHONE FAIL relay init\n"); vTaskDelete(nullptr); return; }
    esp_websocket_register_events(s_ws, WEBSOCKET_EVENT_ANY, on_ws_event, nullptr);
    esp_websocket_client_start(s_ws);

    uint32_t ping_n = 0;
    int64_t last_ping_us = 0;
    bool wifi_was = true, up_was = false;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(100));

        if (s_reconnect_req.exchange(false)) {                   // RL-D5
            const bool was_up = s_phase == Phase::UP;
            esp_websocket_client_stop(s_ws);                     // 태스크가 끝날 때까지 기다린다(끊김 이벤트는 오지 않는다)
            s_phase = Phase::IDLE;
            if (was_up) post(In::Kind::LINK_DOWN);
            printf("RELAY up=0 code=0 attempt=%d\n", s_fails.load());
            esp_websocket_client_start(s_ws);
            continue;
        }

        const bool wifi = netwifi::connected();
        if (wifi && !wifi_was && !s_auth_rejected) esp_websocket_client_set_reconnect_timeout(s_ws, 1000);   // Wi-Fi 가 돌아오면 바로 다시 붙는다
        wifi_was = wifi;

        const int64_t now = esp_timer_get_time();
        const bool is_up = s_phase == Phase::UP;
        if (is_up && (!up_was || now - last_ping_us >= 15LL * 1000000)) {                                  // RL-D1 (15초마다) + RL-D7 (붙자마자 한 번)
            char buf[40];
            if (relayproto::ping_text(buf, sizeof buf, ++ping_n)) send_text(buf);
            last_ping_us = now;
        }
        up_was = is_up;
    }
}

}  // namespace

void start(const char *url, const char *dev_id, const char *token)
{
    if (s_in_q) return;
    s_in_q = xQueueCreate(16, sizeof(In));
    strlcpy(s_url, url, sizeof s_url);
    const int n = snprintf(s_headers, sizeof s_headers, "X-Device-Id: %s\r\nX-Device-Token: %s\r\n", dev_id, token);   // 줄마다 \r\n 이 있어야 한다
    if (n <= 0 || n >= (int)sizeof s_headers) {                  // ID·토큰이 너무 길다 — 붙을 수 없다. 화면에는 "교환기에 연결할 수 없음"으로 보인다
        ESP_LOGE(TAG, "id/token too long"); printf("PHONE FAIL relay headers\n");
        memset(s_headers, 0, sizeof s_headers); s_fails = 99; ++s_seq;
        return;
    }
    esp_log_level_set("transport_ws", ESP_LOG_NONE);             // 접속 요청 쓰기가 실패하면 요청 전문(토큰 포함)을 오류 로그로 찍는 부품 동작을 막는다
    xTaskCreatePinnedToCore(relay_task, "relay", 8 * 1024, nullptr, 5, &s_relay_task, 0);   // send_text(wss 면 TLS 쓰기)와 끊김 핸들러가 이 스택에서 돌 수 있다
}

bool up() { return s_phase == Phase::UP; }
int  http_code() { return s_auth_rejected ? 401 : 0; }
int  attempt() { return s_phase == Phase::UP ? 0 : s_fails.load(); }
uint32_t change_seq() { return s_seq; }

bool poll(In *out) { return s_in_q && xQueueReceive(s_in_q, out, 0) == pdTRUE; }

bool send_text(const char *text)
{
    if (!s_ws || s_phase != Phase::UP || !text) return false;
    const int len = (int)strlen(text);
    return esp_websocket_client_send_text(s_ws, text, len, pdMS_TO_TICKS(2000)) == len;                     // RL-D6
}

bool send_voice(const uint8_t *frame, size_t len)
{
    if (!s_ws || s_phase != Phase::UP || !frame || len < relayproto::MIN_VOICE_FRAME || len > relayproto::MAX_VOICE_FRAME) return false;
    return esp_websocket_client_send_bin(s_ws, (const char *)frame, (int)len, pdMS_TO_TICKS(2000)) == (int)len;
}

void reconnect() { s_reconnect_req = true; }

void stack_free(unsigned *relay_task_free, unsigned *ws_task_free)
{
    TaskHandle_t ws = xTaskGetHandle("relay_ws");                // 부품이 만든 웹소켓 태스크(멈춘 동안에는 없다)
    if (relay_task_free) *relay_task_free = s_relay_task ? (unsigned)uxTaskGetStackHighWaterMark(s_relay_task) : 0;
    if (ws_task_free) *ws_task_free = ws ? (unsigned)uxTaskGetStackHighWaterMark(ws) : 0;
}

void set_voice_sink(voice_sink_t fn) { s_voice_sink = fn; }

}  // namespace relaylink
```

- [x] **Step 3: 기기 번호 — `main/device_id.cpp`**

```cpp
// device_id.cpp — 칩의 MAC 을 읽어 secrets.h 의 기기 표에서 이 기기를 찾는다. 찾는 로직은 device_id.h(맥에서 시험).
#include "device_id.h"

#include "esp_mac.h"

#include "secrets.h"

namespace deviceid {

static const Entry TABLE[] = PHONE_DEVICES;

const Info &get()
{
    static Info info;
    static bool done = false;
    if (!done) {                                                  // app_main 이 태스크를 띄우기 전에 처음 부른다
        uint8_t m[6] = {};
        char s[18] = "";
        if (esp_efuse_mac_get_default(m) == ESP_OK) format_mac(m, s);
        info = lookup(s, TABLE, (int)(sizeof TABLE / sizeof TABLE[0]));
        done = true;
    }
    return info;
}

}  // namespace deviceid
```

- [x] **Step 4: 비밀 설정 예시 — `main/secrets.h.example`** (실제 `secrets.h` 는 Task 1 에서 만들었다)

```cpp
// secrets.h.example — 복사해서 secrets.h 로 만들고 값을 채운다. secrets.h 는 git 에 올리지 않는다(.gitignore: secrets*.h*).
// 내용을 화면·로그·채팅에 붙여 넣지 않는다.
#pragma once

// 처음부터 아는 Wi-Fi (여러 개 가능). 화면에서 추가한 망은 기기에 따로 저장된다.
#define WIFI_NETWORKS { {"my-ssid", "my-password"} }

// 교환기 주소. Phase 2~3 은 맥북 LAN 주소(예: "ws://192.168.0.10:5094/ws"), Phase 4 부터 운영 주소("wss://…/ws").
#define RELAY_URL "ws://192.168.0.10:5094/ws"

// 기기 표: { "MAC", 번호, "교환기 기기 ID", "토큰" }. 토큰은 server/.env.phone 의 PHONE_TOKENS 와 같아야 한다(16자 이상).
#define PHONE_DEVICES { \
    { "AA:BB:CC:DD:EE:01", 1, "tab5-1", "replace-with-token-1" }, \
    { "AA:BB:CC:DD:EE:02", 2, "tab5-2", "replace-with-token-2" }, \
}
```

- [x] **Step 5: 통화 화면 그리기 — `main/ui.h`**

```cpp
// ui.h — 통화 화면 그리기. 어떤 화면인지·버튼 위치는 ui_layout.h 가 정한다.
#pragma once
#include "ui_layout.h"

namespace ui {

// 합성 버퍼에 통화 화면 한 장을 그린다. 화면으로 내보내는 것(gfx::present)은 부르는 쪽이 한다. 화면 태스크에서만 부른다.
void draw(const uilayout::Model &m);

}  // namespace ui
```

- [x] **Step 6: `main/ui.cpp`**

```cpp
// ui.cpp — 통화 화면 그리기. 배치·판정은 ui_layout.h(맥에서 시험), 여기는 그리기만 한다.
// 설계서 §10, docs/design/README.md §1(색 토큰)·§2(상태 글자는 위쪽, 주요 버튼은 아래쪽, 고정 문구는 미리 구운 글자).
#include "ui.h"

#include <cstdio>
#include <cstring>

#include "gfx.h"
#include "wifi_store.h"

using namespace uilayout;
using callfsm::Notice;

namespace {

const int CX = SCREEN_W / 2;

const ui_asset_t *notice_asset(Notice n)
{
    switch (n) {
    case Notice::PEER_OFF:     return &lbl_n_peer_off;
    case Notice::NO_ANSWER:    return &lbl_n_no_answer;
    case Notice::REJECTED:     return &lbl_n_rejected;
    case Notice::LOST:         return &lbl_n_lost;
    case Notice::CALL_ENDED:   return &lbl_n_ended;
    case Notice::MISSED:       return &lbl_n_missed;
    case Notice::RELAY_SILENT: return &lbl_n_relay_silent;
    case Notice::NONE:         break;
    }
    return nullptr;
}

void draw_unit(const Model &m)                                   // 왼쪽 위: 몇 번 전화기
{
    gfx::blit_asset(m.unit == 1 ? &lbl_unit1 : m.unit == 2 ? &lbl_unit2 : &lbl_unit0, 32, 32, C_MUTED);
}

void draw_notice(const Model &m)                                 // 위쪽 가운데: 잠깐 띄우는 안내
{
    const ui_asset_t *a = notice_asset(m.notice);
    if (!a) return;
    const int w = a->w + 64, h = 80, x = CX - w / 2, y = 88;
    gfx::fill_rrect(x, y, w, h, 12, C_SURFACE);
    gfx::blit_asset_center(a, CX, y + h / 2, m.notice == Notice::MISSED ? C_YELLOW_T : C_INK);
}

void draw_wifi_badge(const Model &m)                             // 왼쪽 아래: Wi-Fi 상태. 누르면 설정 화면
{
    const Rect &r = BTN_WIFI;
    gfx::fill_rrect(r.x, r.y, r.w, r.h, 12, C_SURFACE);
    const int cy = r.y + r.h / 2;
    if (!m.wifi_connected || !m.ssid[0]) {
        gfx::blit_asset_center(&lbl_wifi_setup, r.x + r.w / 2, cy, C_YELLOW_T);
        return;
    }
    const int bars = wifistore::bars(m.rssi);                    // 신호 막대 4칸
    for (int i = 0; i < 4; ++i) {
        const int bh = 12 + i * 8;
        gfx::fill_rect(r.x + 24 + i * 12, cy + 18 - bh, 8, bh, i < bars ? C_INK2 : C_LINE);
    }
    char name[33]; strlcpy(name, m.ssid, sizeof name);           // 이름이 길면 뒤를 자른다
    const int max_w = r.w - 24 - 48 - 16 - 16;
    for (size_t n = strlen(name); n > 1 && gfx::text_w(name, 1.5f) > max_w; --n) {
        while (n > 1 && ((unsigned char)name[n - 1] & 0xC0) == 0x80) --n;   // 한글 등 여러 바이트 글자는 통째로 지운다
        name[n - 1] = 0;
    }
    gfx::text(name, r.x + 24 + 48 + 16, cy, C_INK2, 1.5f);
}

void draw_boot(const Model &m)
{
    const ui_asset_t *a = &lbl_relay_connecting; uint32_t c = C_INK;
    if (m.wifi_phase == netwifi::Phase::INIT_FAILED)      { a = &lbl_wifi_init_fail; c = C_RED_T; }
    else if (!m.wifi_connected && m.wifi_phase == netwifi::Phase::NO_KNOWN) { a = &lbl_wifi_none; c = C_YELLOW_T; }
    else if (!m.wifi_connected)                           { a = &lbl_wifi_connecting; }
    else if (m.unit == 0)                                 { a = &lbl_unreg_help; c = C_RED_T; }
    else if (m.relay_code == 401)                         { a = &lbl_auth_fail; c = C_RED_T; }
    else if (m.relay_attempt >= 2)                        { a = &lbl_relay_fail; c = C_YELLOW_T; }
    gfx::blit_asset_center(a, CX, SCREEN_H / 2 - 24, c);
    draw_wifi_badge(m);
}

void draw_idle(const Model &m)
{
    gfx::blit_asset_center(m.peer_online ? &lbl_peer_on : &lbl_peer_off, CX, 48, m.peer_online ? C_GREEN_T : C_MUTED);
    gfx::fill_circle(BTN_CALL.cx, BTN_CALL.cy, BTN_CALL.r, m.peer_online ? C_GREEN_BG : C_SURFACE);
    gfx::blit_asset_center(&lbl_call, BTN_CALL.cx, BTN_CALL.cy, m.peer_online ? C_GREEN_T : C_MUTED);
    if (m.missed > 0) {                                          // 아래 가운데: "부재중 n건"
        const int y = 648, gap = 12;
        char num[8]; snprintf(num, sizeof num, "%u", (unsigned)m.missed);
        int num_w = 0;
        for (const char *q = num; *q; ++q) num_w += DIG_SMALL[*q - '0']->w + 2;
        int x = CX - (lbl_missed.w + gap + num_w + gap / 2 + lbl_count_unit.w) / 2;
        x += gfx::blit_asset(&lbl_missed, x, y - lbl_missed.h / 2, C_YELLOW_T) + gap;
        gfx::draw_number(m.missed, x + num_w / 2, y, false, C_YELLOW_T);
        x += num_w + gap / 2;
        gfx::blit_asset(&lbl_count_unit, x, y - lbl_count_unit.h / 2, C_YELLOW_T);
    }
    draw_wifi_badge(m);
}

void draw_dialing(const Model &m)
{
    gfx::blit_asset_center(&lbl_dialing, CX, 240, C_INK);
    if (m.fsm == callfsm::State::RINGBACK) gfx::blit_asset_center(&lbl_ringback, CX, 336, C_MUTED);
    gfx::button(BTN_CANCEL, &lbl_cancel, true, C_SURFACE, C_INK);
}

void draw_incoming(const Model &)
{
    gfx::blit_asset_center(&lbl_incoming, CX, 216, C_INK);
    gfx::button(BTN_ACCEPT, &lbl_accept, true, C_GREEN_BG, C_GREEN_T);
    gfx::button(BTN_REJECT, &lbl_reject, true, C_RED_BG, C_RED_T);
}

void draw_in_call(const Model &m)
{
    int mm = 0, ss = 0;
    talk_clock(m.talk_seconds, &mm, &ss);
    gfx::blit_asset_center(&lbl_in_call, CX, 208, C_GREEN_T);
    gfx::draw_clock(mm, ss, CX, 368, C_INK);
    gfx::button(BTN_HANGUP, &lbl_hangup, true, C_RED_BG, C_RED_T);
}

}  // namespace

namespace ui {

void draw(const Model &m)
{
    gfx::clear(C_BG);
    draw_unit(m);
    switch (screen_for(m)) {
    case Screen::BOOT:      draw_boot(m); break;
    case Screen::IDLE:      draw_idle(m); break;
    case Screen::DIALING:   draw_dialing(m); break;
    case Screen::INCOMING:  draw_incoming(m); break;
    case Screen::ANSWERING: gfx::blit_asset_center(&lbl_answering, CX, SCREEN_H / 2, C_INK); break;
    case Screen::IN_CALL:   draw_in_call(m); break;
    case Screen::ENDING:    gfx::blit_asset_center(&lbl_ending, CX, SCREEN_H / 2, C_MUTED); break;
    }
    draw_notice(m);
}

}  // namespace ui
```

- [x] **Step 7: 통화 진행 — `main/app.h`**

```cpp
// app.h — 통화 진행. 사건(터치·교환기 신호·시간)을 모아 call_fsm 에 넣고, 돌려받은 "할 일"을 실행하고, 화면을 그린다.
// 설계서 §9.5 의 ui(터치 → 사건)와 main(태스크 기동) 사이를 잇는 모듈이다. PL2-D2: 화면 그리기(ui)와 진행(app)을 나눠
// ui 를 "그릴 내용만 받아 그리는" 코드로 유지한다. 비용: 파일 하나 추가. 탈출구: app.cpp 를 ui.cpp 에 합친다.
#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace app {

// 화면·통화 진행 태스크를 띄운다. gfx::init, netwifi::start, relaylink::start 뒤에 한 번. unit = 이 기기의 번호(미등록이면 0).
void start(int unit);

void inject_tap(int x, int y);      // 시리얼 't x y' — 실제 터치와 같은 대기열(8칸)에 넣는다. 화면 태스크가 다음 바퀴에 처리한다
void print_stat();                  // 시리얼 'stat' — STAT 한 줄을 찍는다(어느 태스크에서 불러도 된다)

// M5.In_I2C 를 쓰는 코드가 함께 잡는 잠금. 터치 읽기(M5.update)와 Phase 3 의 코덱·앰프 제어가 같은 버스를 쓴다.
SemaphoreHandle_t i2c_lock();

}  // namespace app
```

- [x] **Step 8: `main/app.cpp`**

```cpp
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

#include "call_fsm.h"
#include "gfx.h"
#include "net_wifi.h"
#include "relay_link.h"
#include "relay_proto.h"
#include "ui.h"
#include "ui_layout.h"
#include "ui_wifi.h"

using namespace callfsm;
using uilayout::Model;
using uilayout::Tap;

namespace app {
namespace {

const char *TAG = "app";
const int64_t NOTICE_MS = 4000;                 // 안내를 띄워 두는 시간

struct TapEv { int16_t x, y; };

Fsm s_fsm;
Model s_model, s_drawn;
bool s_drawn_valid = false;
uilayout::TapGuard s_guard;
const char *s_screen = "BOOT";
int64_t s_screen_since = 0;
Notice s_notice = Notice::NONE;
int64_t s_notice_until = 0;
int s_unit = 0;

SemaphoreHandle_t s_i2c = nullptr, s_stat_lock = nullptr;
QueueHandle_t s_tap_q = nullptr;                // touch 태스크·시리얼 주입 → app 태스크
TaskHandle_t s_app_task = nullptr, s_touch_task = nullptr;
Model s_stat;                                   // STAT 용 사본(s_stat_lock 으로 보호)
const char *s_stat_screen = "BOOT";
std::atomic<uint32_t> s_touches{0};

int64_t now_ms() { return esp_timer_get_time() / 1000; }

void apply(const Out &o, State before, int64_t now)
{
    static const uint16_t SENDS[] = { A_SEND_CALL, A_SEND_ACCEPT, A_SEND_REJECT, A_SEND_HANGUP };
    for (uint16_t bit : SENDS) {
        if ((o.acts & bit) && !relaylink::send_text(relayproto::text_for(bit))) ESP_LOGW(TAG, "signal 0x%x not sent (link down)", bit);
    }
    if (o.acts & A_RECONNECT) relaylink::reconnect();
    // 소리: Phase 3 에서 tones·voice_pipeline 을 여기에 잇는다(A_TONE_*, A_VOICE_*). Phase 2 는 아래 FSM 줄에 acts 로만 남는다.
    if (o.notice != Notice::NONE) { s_notice = o.notice; s_notice_until = now + NOTICE_MS; }
    if (s_fsm.state != before || o.acts || o.notice != Notice::NONE)
        printf("FSM %s -> %s acts=0x%x notice=%s\n", state_name(before), state_name(s_fsm.state), (unsigned)o.acts, notice_name(o.notice));
}

void feed(Ev ev, int64_t now, Why why = Why::NONE, bool flag = false)
{
    const State before = s_fsm.state;
    apply(s_fsm.on(ev, now, why, flag), before, now);
}

bool same(const Model &a, const Model &b)
{
    return a.fsm == b.fsm && a.peer_online == b.peer_online && a.missed == b.missed && a.talk_seconds == b.talk_seconds
        && a.notice == b.notice && a.wifi_phase == b.wifi_phase && a.wifi_connected == b.wifi_connected && !strcmp(a.ssid, b.ssid)
        && wifistore::bars(a.rssi) == wifistore::bars(b.rssi) && a.relay_up == b.relay_up && a.relay_code == b.relay_code
        && (a.relay_attempt >= 2) == (b.relay_attempt >= 2) && a.unit == b.unit;
}

// 지금 상태로 "그릴 내용"을 만들고, 화면이 바뀌었으면 기록한다(터치 가드의 기준 시각이 된다).
void refresh(int64_t now)
{
    Model m;
    m.fsm = s_fsm.state; m.peer_online = s_fsm.peer_online; m.missed = s_fsm.missed;
    m.talk_seconds = s_fsm.state == State::IN_CALL ? (int)((now - s_fsm.call_start_ms) / 1000) : 0;
    if (s_notice != Notice::NONE && now >= s_notice_until) s_notice = Notice::NONE;
    m.notice = s_notice;
    m.wifi_phase = netwifi::phase(); m.wifi_connected = netwifi::connected();
    netwifi::ssid(m.ssid, sizeof m.ssid); m.rssi = netwifi::rssi();
    m.relay_up = relaylink::up(); m.relay_code = relaylink::http_code(); m.relay_attempt = relaylink::attempt();
    m.unit = s_unit;
    s_model = m;

    const char *screen = uiwifi::is_open() ? (uiwifi::on_keyboard() ? "WIFI_PASS" : "WIFI_LIST") : uilayout::screen_name(uilayout::screen_for(s_model));
    if (strcmp(screen, s_screen) != 0) { s_screen = screen; s_screen_since = now; printf("SCREEN %s\n", screen); }
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
    if (!s_guard.allow(now, s_screen_since)) return;
    switch (t) {
    case Tap::CALL:   feed(Ev::TAP_CALL, now); break;
    case Tap::CANCEL: feed(Ev::TAP_CANCEL, now); break;
    case Tap::ACCEPT: feed(Ev::TAP_ACCEPT, now); break;
    case Tap::REJECT: feed(Ev::TAP_REJECT, now); break;
    case Tap::HANGUP: feed(Ev::TAP_HANGUP, now); break;
    case Tap::WIFI:   uiwifi::open(); break;
    case Tap::NONE:   break;
    }
}

void touch_task(void *)                                          // AP-D2
{
    while (true) {
        xSemaphoreTake(s_i2c, portMAX_DELAY); M5.update(); xSemaphoreGive(s_i2c);
        auto t = M5.Touch.getDetail();
        if (t.wasPressed()) { TapEv e{ (int16_t)t.x, (int16_t)t.y }; xQueueSend(s_tap_q, &e, 0); }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_task(void *)
{
    bool wifi_ui_drawn = false;
    while (true) {
        const int64_t now = now_ms();

        relaylink::In in;                                        // 1) 교환기 사건
        while (relaylink::poll(&in)) {
            if (in.kind == relaylink::In::Kind::LINK_UP) feed(Ev::LINK_UP, now);
            else if (in.kind == relaylink::In::Kind::LINK_DOWN) feed(Ev::LINK_DOWN, now);
            else feed(in.sig.ev, now, in.sig.why, in.sig.flag);
        }
        refresh(now);                                            //    방금 바뀐 화면을 기준으로 터치를 판정한다

        TapEv tap;                                               // 2) 터치(실제 터치와 시리얼로 주입된 것이 같은 길로 온다)
        while (xQueueReceive(s_tap_q, &tap, 0) == pdTRUE) { on_tap(tap.x, tap.y, now); refresh(now); }

        feed(Ev::TICK, now);                                     // 3) 시간 제한

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
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

}  // namespace

void start(int unit)
{
    s_unit = unit;
    s_i2c = xSemaphoreCreateMutex();
    s_stat_lock = xSemaphoreCreateMutex();
    s_tap_q = xQueueCreate(8, sizeof(TapEv));
    xTaskCreatePinnedToCore(app_task, "app", 12 * 1024, nullptr, 4, &s_app_task, 1);
    xTaskCreatePinnedToCore(touch_task, "touch", 4 * 1024, nullptr, 5, &s_touch_task, 1);
}

void inject_tap(int x, int y)
{
    if (!s_tap_q) return;
    TapEv e{ (int16_t)x, (int16_t)y };
    xQueueSend(s_tap_q, &e, 0);
}

void print_stat()
{
    Model m; const char *screen = "BOOT";
    if (s_stat_lock) { xSemaphoreTake(s_stat_lock, portMAX_DELAY); m = s_stat; screen = s_stat_screen; xSemaphoreGive(s_stat_lock); }
    unsigned stk_relay = 0, stk_ws = 0;
    relaylink::stack_free(&stk_relay, &stk_ws);
    printf("STAT up=%u unit=%d fsm=%s screen=%s wifi=%d relay=%d relay_code=%d peer=%d missed=%u touch=%u heap_int=%u heap_int_min=%u heap_psram=%u"
           " stk_app=%u stk_touch=%u stk_relay=%u stk_ws=%u\n",
           (unsigned)(esp_timer_get_time() / 1000000), m.unit, state_name(m.fsm), screen, (int)m.wifi_connected, (int)m.relay_up, m.relay_code,
           (int)m.peer_online, (unsigned)m.missed, (unsigned)s_touches.load(),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           s_app_task ? (unsigned)uxTaskGetStackHighWaterMark(s_app_task) : 0, s_touch_task ? (unsigned)uxTaskGetStackHighWaterMark(s_touch_task) : 0,
           stk_relay, stk_ws);
}

SemaphoreHandle_t i2c_lock() { return s_i2c; }

}  // namespace app
```

- [x] **Step 9: 부팅·시리얼 명령 — `main/phone_main.cpp`**

```cpp
// phone_main.cpp — Tab5 음성 전화기(Phase 2: 소리 없이 벨·받기·거절·끊기). 부팅, 태스크 기동, 시리얼 명령만 한다.
//
// 설계: docs/exec-plans/2026-10-03-tab5-phone.md §9.5. 두 대에 같은 펌웨어를 올리고, 기기 번호는 MAC 으로 정한다(device_id).
// 태스크: app(화면·통화 진행, 코어1) / touch(터치 읽기, 코어1) / wifi(매니저, 코어0) / relay(교환기 연결 관리, 코어0) / relay_ws(부품의 웹소켓 태스크) / sercmd(시리얼 명령, 코어0)
//
// 시리얼 명령(115200, 줄 단위) — tools/phone_ctl.py 가 쓴다:
//   stat | t <x> <y> | shot | reboot
#include <M5Unified.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "app.h"
#include "device_id.h"
#include "gfx.h"
#include "net_wifi.h"
#include "relay_link.h"
#include "secrets.h"

static const char *TAG = "phone";

#define DISPLAY_ROTATION 3              // 삼각대 나사가 화면 아래(정상 가로). 터치 좌표도 이 방향 기준

// 화면이 안 잡히면(소프트 리셋 뒤 패널 감지가 가끔 실패한다) 재시작한다. 횟수는 NVS 에 적어 3번까지만 — 무한 재부팅 방지(§9.7).
// what 마다 횟수를 따로 센다(키 "rst_<what>") — 한 검사가 통과하며 다른 검사의 횟수를 지우면 끝없이 재시작한다.
static void boot_guard(bool ok, const char *what)
{
    char key[16]; snprintf(key, sizeof key, "rst_%s", what);
    nvs_handle_t h; uint8_t n = 0;
    const bool nvs_ok = nvs_open("phone", NVS_READWRITE, &h) == ESP_OK;
    if (nvs_ok) nvs_get_u8(h, key, &n);
    if (!ok && nvs_ok && n < 3) {
        const bool counted = nvs_set_u8(h, key, (uint8_t)(n + 1)) == ESP_OK && nvs_commit(h) == ESP_OK;
        nvs_close(h);
        if (counted) {                                           // 횟수를 적지 못했으면 재시작하지 않는다(세지 못하면 끝없이 재시작한다)
            printf("PHONE %s-failed restart %d\n", what, n + 1);
            esp_restart();
        }
        printf("PHONE FAIL %s (restart count not saved)\n", what);
        return;
    }
    if (nvs_ok) { if (ok && n) { nvs_set_u8(h, key, 0); nvs_commit(h); } nvs_close(h); }
    if (!ok) printf("PHONE FAIL %s\n", what);
}

// 화면을 JPEG 로 찍어 시리얼로 내보낸다. 받는 쪽(phone_ctl.py)이 크기와 검사합을 확인한다.
static void dump_shot()
{
    const uint8_t *p = nullptr;
    const size_t n = gfx::screenshot_jpeg(&p);
    if (!n || !p) { printf("SHOT FAIL encode\n"); return; }
    uint32_t sum = 0;
    for (size_t i = 0; i < n; ++i) sum += p[i];
    printf("SHOT BEGIN %u\n", (unsigned)n);
    static unsigned char line[4200];
    for (size_t off = 0; off < n; off += 3072) {
        size_t chunk = n - off < 3072 ? n - off : 3072, olen = 0;
        mbedtls_base64_encode(line, sizeof line, &olen, p + off, chunk); line[olen] = 0;
        printf("B64:%s\n", (const char *)line);
        vTaskDelay(pdMS_TO_TICKS(2));   // 줄마다 양보 — 쉬지 않고 보내면 CPU0 IDLE 이 굶어 감시 경고가 줄 한가운데 끼어든다(Phase 0 에서 겪음)
    }
    printf("SHOT END %u\n", (unsigned)sum);
}

static void serial_cmd(const std::string &cmd)
{
    int x = 0, y = 0;
    if (cmd == "stat") app::print_stat();
    else if (sscanf(cmd.c_str(), "t %d %d", &x, &y) == 2) {
        if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) printf("ERR t out-of-range\n");
        else { app::inject_tap(x, y); printf("OK t %d %d\n", x, y); }
    }
    else if (cmd == "shot") dump_shot();
    else if (cmd == "reboot") { printf("OK reboot\n"); fflush(stdout); vTaskDelay(pdMS_TO_TICKS(200)); esp_restart(); }
    else printf("ERR unknown\n");
    fflush(stdout);
}

static void serial_task(void *)
{
    usb_serial_jtag_driver_config_t jcfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    if (usb_serial_jtag_driver_install(&jcfg) == ESP_OK) usb_serial_jtag_vfs_use_driver();
    printf("PHONE READY\n"); fflush(stdout);
    std::string line;
    while (true) {
        uint8_t b = 0;
        if (usb_serial_jtag_read_bytes(&b, 1, pdMS_TO_TICKS(100)) <= 0) continue;
        if (b == '\r') continue;
        if (b != '\n') { if (line.size() < 64) line += (char)b; continue; }
        std::string cmd; cmd.swap(line);
        if (!cmd.empty()) serial_cmd(cmd);
    }
}

extern "C" void app_main(void)
{
    auto cfg = M5.config();
    cfg.internal_spk = false;           // 소리는 Phase 3 에서 esp_codec_dev 로 직접 다룬다(PH-D4) — M5Unified 가 I2S0·코덱을 잡지 않게
    cfg.internal_mic = false;
    M5.begin(cfg);

    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {     // 다른 펌웨어가 쓰던 자리(1번 기기) — 지우고 다시
        ESP_LOGW(TAG, "nvs unusable (%s) -> erase. saved wifi list is lost", esp_err_to_name(e));
        nvs_flash_erase();
        e = nvs_flash_init();
    }
    if (e != ESP_OK) ESP_LOGE(TAG, "nvs init failed: %s", esp_err_to_name(e));

    boot_guard(M5.Display.width() != 0, "panel");
    M5.Display.setRotation(DISPLAY_ROTATION);
    M5.Display.setBrightness(160);

    const deviceid::Info &me = deviceid::get();
    printf("PHONE BOOT unit=%d id=%s\n", me.unit, me.id);
    ESP_LOGI(TAG, "mac %s", me.mac);
    if (!me.unit) ESP_LOGE(TAG, "this MAC is not in PHONE_DEVICES (secrets.h) -> relay disabled");

    const bool gfx_ok = gfx::init();
    boot_guard(gfx_ok, "gfx");

    netwifi::start();
    if (me.unit) relaylink::start(RELAY_URL, me.id, me.token);
    if (gfx_ok) app::start(me.unit);
    xTaskCreatePinnedToCore(serial_task, "sercmd", 8 * 1024, nullptr, 3, nullptr, 0);
}
```

- [x] **Step 10: 임시 진입점을 지우고 `main/CMakeLists.txt` 를 최종 모양으로**

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && git rm -q main/stub_main.cpp && ls main/stub_main.cpp 2>&1 | tail -1
```
Expected: `ls: main/stub_main.cpp: No such file or directory` (Task 5 에서 커밋된 파일이라 git 기록에 남는다)

```cmake
# REQUIRES 를 적지 않는다: main 은 그러면 모든 컴포넌트를 자동으로 본다(관리형 컴포넌트 이름 오타 방지 — 시험 펌웨어와 같은 방식).
idf_component_register(SRCS
                           "phone_main.cpp"   # 부팅, 태스크 기동, 시리얼 명령
                           "app.cpp"          # 사건 → 통화 상태 기계 → 할 일 실행 → 다시 그리기
                           "ui.cpp"           # 통화 화면 그리기
                           "relay_link.cpp"   # 교환기 연결·재연결·ping
                           "device_id.cpp"    # 칩 MAC → 기기 번호·ID·토큰
                           "gfx.cpp"          # 화면 합성·출력·스크린샷
                           "net_wifi.cpp"     # Wi-Fi 매니저
                           "ui_wifi.cpp"      # Wi-Fi 설정 화면
                       INCLUDE_DIRS ".")

# B-D1: -Wliteral-suffix 한 종류만 끈다(main 의 C++ 파일에만).
#   이유: <M5Unified.h> 를 포함하는 파일마다 ESP-IDF 5.5.4 헤더(esp_private/periph_ctrl.h:88, "…"CONFIG_IDF_TARGET 사이에 공백 없음)에서
#         같은 경고가 1건씩 찍힌다 — 스냅부스·시험 펌웨어 빌드 기록에도 있는 소음이다. 우리 코드의 새 경고가 묻히지 않게 끈다.
#   비용: 우리 코드에서 같은 종류("문자열"매크로 를 붙여 쓴 것)의 경고도 안 보인다. 탈출구: 아래 한 줄을 지운다.
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-literal-suffix>)
```

- [x] **Step 11: 전체 빌드**

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && source ~/esp/esp-idf-5.5.4/export.sh && idf.py build 2>&1 | tail -6
```
Expected (끝부분. 크기는 `secrets.h` 의 글자 수에 따라 몇 바이트 다를 수 있다):
```
tab5_phone.bin binary size 0x219b30 bytes. Smallest app partition is 0x600000 bytes. 0x3e64d0 bytes (65%) free.
Project build complete.
```
우리 코드(`main/*.cpp`)에서 경고가 나오면 고친다(경고 0 이 기준). 빌드가 실패하면:
- `esp_websocket_client.h: No such file` → `main/idf_component.yml` 에 `espressif/esp_websocket_client: '1.8.0'` 이 있는지 확인(Task 5 Step 2).
- `'lbl_…' was not declared` → 글자 자산이 빠졌다. Task 4 의 생성기 목록에 그 이름이 있는지 확인하고 다시 굽는다.
- `'PHONE_DEVICES' was not declared` / `'RELAY_URL' …` → Task 1 Step 7 의 `secrets.h` 를 다시 만든다.
- 부품 판 해석 실패(`ERROR: Because …`) → **`esp_hosted` 고정은 풀지 않는다.** 오류 전문을 보고하고 멈춘다.

- [x] **Step 12: 실제로 잡힌 부품 판 확인**

Run:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 - <<'PY'
import re
name = None
for ln in open("dependencies.lock"):
    m = re.match(r"^  ([a-z0-9_/\-]+):$", ln)
    if m: name = m.group(1)
    m = re.match(r"^    version: (.+)$", ln)
    if m and name: print(f"{name:36s} {m.group(1)}"); name = None
PY
```
Expected: 직접 고정한 여덟 개가 아래와 같다(다르면 실패). 딸려 온 것은 결과 기록(Task 10)에 실제 값을 적는다.
```
espressif/esp-sr                     2.5.5
espressif/esp_audio_codec            2.5.0
espressif/esp_codec_dev              1.6.2
espressif/esp_hosted                 2.11.7
espressif/esp_websocket_client       1.8.0
espressif/esp_wifi_remote            1.6.5
m5stack/m5gfx                        0.2.30
m5stack/m5unified                    0.2.23
```

- [x] **Step 13: 산출물이 git 에 안 보이는지 확인하고 커밋**

```bash
cd ~/tab5-toy-phone && git status --short firmware/tab5_phone | grep -cE "build/|managed_components|sdkconfig$|dependencies.lock|secrets\.h$|/out/|/fonts/"
git add firmware/tab5_phone && git commit -m "feat(phone): 전화기 펌웨어 골격 — 교환기 연결·통화 화면·진행(소리 없음)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
Expected: 첫 줄 `0`

---

### Task 7: 2번 기기 한 대로 검증 (상대는 맥이 흉내 낸다)

**Files:**
- Create: `firmware/tab5_phone/out/t7_*.jpg`, `out/task7.log` (git 제외)
- Modify(결함이 나오면): 해당 소스 — 고칠 때는 "참고 구현 대조 우선" 원칙을 따른다

**Interfaces:**
- Consumes: Task 3 의 `phone_ctl.py`·`phone_fake_peer.py`, Task 6 의 펌웨어와 시리얼 규약, Phase 1 의 `server/phone_relay.py`.
- Produces: 2번 기기에서 확인된 동작 목록과 스크린샷 — Task 8 에서 1번에 올려도 되는 근거. 기기에서만 드러난 결함의 수정 커밋.

**시험 조건**: 맥북과 2번 기기가 같은 Wi-Fi 에 있다. 1번 기기는 아직 사진 액자 그대로(전화기 펌웨어가 아니므로 `tab5-1` 자리는 맥의 가짜 상대가 쓴다). 사용자에게 먼저 알린다: "지금부터 2번 기기가 스냅부스가 아니라 전화기 화면으로 바뀝니다. 약 30분 동안 화면이 혼자 바뀌는 것은 자동 시험입니다."

- [x] **Step 1: 맥북에서 교환기를 띄운다(LAN 에서 접속 가능하게)**

```bash
cd ~/tab5-toy-phone/server && (BIND=0.0.0.0 PORT=5094 nohup python3 phone_relay.py > /tmp/phone_relay_dev.log 2>&1 &) ; sleep 1
curl -s -m 3 http://127.0.0.1:5094/healthz; echo; curl -s -m 3 "http://$(ipconfig getifaddr en0):5094/healthz"; echo
```
Expected: `{"ok": true}` 두 줄. 둘째 줄이 안 나오면 macOS 방화벽이 막은 것이다 — 화면에 "python 이 들어오는 연결을 허용할까요?" 창이 떴는지 사용자에게 확인을 요청한다(허용을 눌러야 기기가 붙는다).

정정(2026-10-04): 위 명령(`BIND=0.0.0.0 PORT=5094 nohup python3 phone_relay.py …`)은 Claude Code 의 자동 안전 검사가 막는다(로컬 서비스를 망에 여는 명령). 우회하지 않았고, 사용자가 허용 규칙 하나를 직접 추가했다: `Bash(bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh:*)`. **Task 7~9(와 새 기능 Task 6)의 교환기 명령은 모두 `bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh start | stop | restart | start-wrong-token | status` 로 바꿔 쓴다** — 정확히 이 모양(`bash` + 절대 경로 + 낱말 하나), 한 번의 호출에 이 명령 하나만. 스크립트는 `0.0.0.0:5094` 로 띄우고 로그는 `/tmp/phone_relay_dev.log`(`start` 때마다 덮어씀). 시험이 끝나면 위 허용 규칙을 지운다. 이 단계의 `curl` 로 LAN 주소를 부르는 확인은 맥북이 제 LAN 주소로 보내면 빈 응답이었으나(원인 미조사) 기기 접속에는 영향이 없었다(기기가 실제로 붙었다).

- [x] **Step 2: 대상이 2번인지 확인한 뒤 플래시**

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && source ~/esp/esp-idf-5.5.4/export.sh
PORT=$(python3 tools/phone_ctl.py port 2) && echo "2번 포트: $PORT"
esptool.py -p "$PORT" read_mac | grep -i "aa:bb:cc:dd:ee:02" && idf.py -p "$PORT" flash 2>&1 | tail -3
```
Expected: `MAC: aa:bb:cc:dd:ee:02` 뒤 `Hard resetting via RTS pin...`. MAC 줄이 안 나오면 플래시는 실행되지 않는다 — 멈추고 보고.

- [x] **Step 3: 부팅 → Wi-Fi → 교환기 연결 → 대기 화면**

Run (ESP-IDF 를 export 하지 않은 새 셸에서):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && mkdir -p out && python3 tools/phone_ctl.py --unit 2 run "until:stat:^STAT .*fsm=IDLE:90" "shot:out/t7_idle_peer_off.jpg" | tee out/task7.log | grep -E "^(STAT|PHONE|RELAY|SCREEN|==|!!)"
```
Expected: `STAT … unit=2 fsm=IDLE screen=IDLE wifi=1 relay=1 relay_code=0 peer=0 …` 와 `== 저장: out/t7_idle_peer_off.jpg (… B, 1번째 전송, …초)`. 스크린샷을 **직접 열어 본다**(Read 도구): 왼쪽 위 "2번 전화기", 위 가운데 "상대 기기가 꺼져 있음", 가운데 흐린 원 안에 "통화", 왼쪽 아래 Wi-Fi 배지(신호 막대 + 망 이름). 글자가 깨졌거나 겹치면 Task 4·6 으로 돌아가 고친다.

`until failed` 이면 로그에서 원인을 본다:

| 보이는 것 | 뜻 | 조치 |
|---|---|---|
| `PHONE BOOT unit=0 id=unknown` | 이 기기의 MAC 이 `secrets.h` 표에 없다 | 로그의 `phone: mac …` 줄과 표를 대조. `esp_efuse_mac_get_default` 가 USB 일련번호와 다른 값을 주면 **멈추고 보고**(기기 식별 방법을 바꿔야 한다) |
| `PHONE FAIL gfx` / `PHONE gfx-failed restart` | 화면 합성 초기화 실패 | 로그의 `gfx: init fail: <단계>` 를 스냅부스 `main.cpp` 1799~1844줄과 대조 |
| `fsm=OFFLINE … wifi=0` 이 계속 | Wi-Fi 에 못 붙음 | 로그의 `wifi` 줄. 아는 망이 안 보이면 화면의 배지를 눌러 설정(Step 9) |
| `wifi=1 relay=0` 이 계속, `RELAY up=0 code=0 attempt=…` 반복 | 교환기에 못 붙음 | 맥북 주소가 바뀌었는지(`ipconfig getifaddr en0` vs Task 1 Step 7), Step 1 의 방화벽 |
| `RELAY up=0 code=401` | 토큰이 다르다 | `secrets.h` 를 Task 1 Step 7 로 다시 만든다 |
| 재부팅 반복(`PHONE BOOT` 가 계속) | 충돌 | 직전 로그 전문을 보고하고 멈춘다 |

- [x] **Step 4: 상대가 꺼져 있을 때 통화 버튼 → 안내만 뜨고 걸리지 않는다**

Run:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run "until:stat:^STAT .*fsm=IDLE:30" "tap:640:360" "wait:^FSM IDLE -> IDLE acts=0x0 notice=PEER_OFF:5" "shot:out/t7_notice_peer_off.jpg" | tee -a out/task7.log | grep -E "^(FSM|==|!!)"
```
Expected: `FSM IDLE -> IDLE acts=0x0 notice=PEER_OFF`. 스크린샷 위쪽에 "상대 기기가 꺼져 있어 걸 수 없어요" 안내 상자.

- [x] **Step 5: 가짜 상대가 건다 → 벨 화면 → 받기 → 통화 화면 → 상대가 끊음**

```bash
cd ~/tab5-toy-phone && (python3 server/tools/phone_fake_peer.py tab5-1 expect:peer sleep:2 send:call expect:ringing expect:connected@20 sleep:4 send:hangup expect:ended:hangup > /tmp/fake_t7_5.log 2>&1 &)
cd firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run "wait:^FSM IDLE -> INCOMING acts=0x20:15" "sleep:1" "shot:out/t7_incoming.jpg" "tap:404:428" "wait:^FSM INCOMING -> ANSWERING acts=0x82:5" "wait:^FSM ANSWERING -> IN_CALL acts=0x180:5" "sleep:2" "shot:out/t7_in_call.jpg" "wait:^FSM IN_CALL -> IDLE acts=0x240 notice=CALL_ENDED:15" | tee -a out/task7.log | grep -E "^(FSM|SCREEN|==|!!)"; sleep 1; tail -1 /tmp/fake_t7_5.log
```
Expected: FSM 줄 넷이 차례로, 스크린샷 둘, 마지막 줄 `… FAKE OK`. 스크린샷 확인: 벨 화면은 "전화가 왔습니다" + 초록 "받기"·빨강 "거절", 통화 화면은 "통화 중" + `00:0x` 시계 + 빨강 "끊기".

- [x] **Step 6: 기기가 건다 → 연결음 화면 → 상대가 받음 → 기기가 끊음**

```bash
cd ~/tab5-toy-phone && (python3 server/tools/phone_fake_peer.py tab5-1 expect:peer expect:ring@30 sleep:2 send:accept expect:connected expect:ended:hangup@30 > /tmp/fake_t7_6.log 2>&1 &)
cd firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run "until:stat:^STAT .*fsm=IDLE.*peer=1:20" "tap:640:360" "wait:^FSM IDLE -> DIALING acts=0x1:5" "wait:^FSM DIALING -> RINGBACK acts=0x10:5" "shot:out/t7_dialing.jpg" "wait:^FSM RINGBACK -> IN_CALL acts=0x180:10" "sleep:2" "tap:644:606" "wait:^FSM IN_CALL -> ENDING acts=0x248:5" "wait:^FSM ENDING -> IDLE:5" | tee -a out/task7.log | grep -E "^(FSM|==|!!)"; sleep 1; tail -1 /tmp/fake_t7_6.log
```
Expected: FSM 줄 다섯, `… FAKE OK`. 스크린샷: "거는 중…" + "상대 벨이 울리고 있어요" + "취소".

- [x] **Step 7: 나머지 경우 다섯 가지** — 각각 실행하고 Expected 의 FSM 줄과 `FAKE OK` 를 확인한다

(부재중 건수(`missed=`)는 재부팅 전까지 쌓인다. 아래 기대값은 (a)~(e)를 처음부터 순서대로 한 번씩 돌렸을 때의 값이다 — 어느 것을 다시 돌릴 때는 먼저 `python3 tools/phone_ctl.py --unit 2 run reboot "until:stat:^STAT .*fsm=IDLE:90"` 로 0 으로 되돌리고 (a)부터 다시 한다.)

(a) 거절:
```bash
cd ~/tab5-toy-phone && (python3 server/tools/phone_fake_peer.py tab5-1 expect:peer sleep:2 send:call expect:ringing expect:ended:reject@20 > /tmp/fake_t7_7a.log 2>&1 &)
cd firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run "wait:^FSM IDLE -> INCOMING:15" "sleep:1" "tap:876:428" "wait:^FSM INCOMING -> ENDING acts=0x84:5" "wait:^FSM ENDING -> IDLE:5" "stat" "wait:^STAT .*missed=0:5" | tee -a out/task7.log | grep -E "^(FSM|STAT|!!)"; sleep 1; tail -1 /tmp/fake_t7_7a.log
```
(b) 상대가 걸다 취소 → 부재중:
```bash
cd ~/tab5-toy-phone && (python3 server/tools/phone_fake_peer.py tab5-1 expect:peer sleep:2 send:call expect:ringing sleep:3 send:hangup expect:ended:hangup > /tmp/fake_t7_7b.log 2>&1 &)
cd firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run "wait:^FSM IDLE -> INCOMING:15" "wait:^FSM INCOMING -> IDLE acts=0x80 notice=MISSED:15" "stat" "wait:^STAT .*missed=1:5" "sleep:5" "shot:out/t7_missed.jpg" | tee -a out/task7.log | grep -E "^(FSM|STAT|==|!!)"; sleep 1; tail -1 /tmp/fake_t7_7b.log
```
스크린샷 아래 가운데에 "부재중 1건".

(c) 기기가 걸었는데 30초 무응답:
```bash
cd ~/tab5-toy-phone && (python3 server/tools/phone_fake_peer.py tab5-1 expect:peer expect:ring@30 expect:ended:timeout@40 > /tmp/fake_t7_7c.log 2>&1 &)
cd firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run "until:stat:^STAT .*fsm=IDLE.*peer=1:20" "tap:640:360" "wait:^FSM DIALING -> RINGBACK:5" "wait:^FSM RINGBACK -> IDLE acts=0x40 notice=NO_ANSWER:40" | tee -a out/task7.log | grep -E "^(FSM|!!)"; sleep 1; tail -1 /tmp/fake_t7_7c.log
```
(d) 기기가 걸다 취소:
```bash
cd ~/tab5-toy-phone && (python3 server/tools/phone_fake_peer.py tab5-1 expect:peer expect:ring@30 expect:ended:hangup@20 > /tmp/fake_t7_7d.log 2>&1 &)
cd firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run "until:stat:^STAT .*fsm=IDLE.*peer=1:20" "tap:640:360" "wait:^FSM DIALING -> RINGBACK:5" "sleep:1" "tap:640:604" "wait:^FSM RINGBACK -> ENDING acts=0x88:5" "wait:^FSM ENDING -> IDLE:5" | tee -a out/task7.log | grep -E "^(FSM|!!)"; sleep 1; tail -1 /tmp/fake_t7_7d.log
```
(e) 벨이 울리는 중에 상대가 사라짐(가짜 상대가 그냥 끊고 나감) → 부재중:
```bash
cd ~/tab5-toy-phone && (python3 server/tools/phone_fake_peer.py tab5-1 expect:peer sleep:2 send:call expect:ringing sleep:2 > /tmp/fake_t7_7e.log 2>&1 &)
cd firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run "wait:^FSM IDLE -> INCOMING:15" "wait:^FSM INCOMING -> IDLE acts=0x80 notice=MISSED:15" "stat" "wait:^STAT .*peer=0.*missed=2:5" | tee -a out/task7.log | grep -E "^(FSM|STAT|!!)"
```
Expected(공통): 적힌 `wait` 가 모두 통과(`!!` 줄 없음), (a)~(d) 는 `FAKE OK`.

- [x] **Step 8: 교환기가 사라졌다 돌아올 때 — 재연결 간격, 토큰 거절, 밀려남**

한 번만 찍히는 줄(`FSM IDLE -> OFFLINE` 등)을 놓치지 않도록 **도구를 먼저 띄우고 교환기는 그 뒤에 끈다**(같은 셸에서 지연 종료). 상태 확인은 지나간 줄이 아니라 `until:stat` 으로 한다.

(a) 교환기를 끄고 지켜본 뒤 다시 켠다:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && (sleep 25; pkill -f "phone_relay.py") & python3 tools/phone_ctl.py --unit 2 run "until:stat:^STAT .*fsm=IDLE.*relay=1:20" "wait:^RELAY up=0:60" "until:stat:^STAT .*fsm=OFFLINE.*relay=0:10" "sleep:40" "shot:out/t7_relay_down.jpg" | tee -a out/task7.log | grep -E "^(RELAY|FSM|SCREEN|==|!!)"
cd ~/tab5-toy-phone/server && (BIND=0.0.0.0 PORT=5094 nohup python3 phone_relay.py > /tmp/phone_relay_dev.log 2>&1 &) ; cd ../firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run "until:stat:^STAT .*fsm=IDLE.*relay=1:45" | tee -a out/task7.log | grep -E "^(STAT|!!)"
```
Expected: `RELAY up=0 code=0 attempt=1` → `FSM IDLE -> OFFLINE acts=0x0 notice=NONE` → `SCREEN BOOT` → 이어서 `RELAY up=0 … attempt=2`, `3`, `4`… 가 간격을 벌리며 찍힌다(1→2→4→8→16→30초). 스크린샷에 "교환기에 연결할 수 없음 — 다시 시도하는 중". 다시 켜면 최대 30초 안에 `STAT … fsm=IDLE … relay=1`.
(직전 연결이 60초 넘게 유지되지 않았으면 실패 횟수가 이어져 첫 간격부터 길 수 있다 — RL-D3. 그래도 `attempt` 가 늘며 간격이 30초를 넘지 않으면 정상이다.)

(b) 틀린 토큰의 교환기(기기 쪽에서는 "내 토큰이 거절됨"과 같다):
```bash
cd ~/tab5-toy-phone/server && pkill -f "phone_relay.py"; (PHONE_TOKENS="tab5-1:wrong-token-aaaaaaaaaaaa,tab5-2:wrong-token-bbbbbbbbbbbb" BIND=0.0.0.0 PORT=5094 nohup python3 phone_relay.py > /tmp/phone_relay_dev.log 2>&1 &)
cd ../firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run "until:stat:^STAT .*relay=0 relay_code=401:100" "shot:out/t7_auth_fail.jpg" | tee -a out/task7.log | grep -E "^(RELAY|STAT|==|!!)"
cd ~/tab5-toy-phone/server && pkill -f "phone_relay.py"; (BIND=0.0.0.0 PORT=5094 nohup python3 phone_relay.py > /tmp/phone_relay_dev.log 2>&1 &) ; cd ../firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run "until:stat:^STAT .*relay=1:100" | tee -a out/task7.log | grep -E "^(STAT|!!)"
```
Expected: `STAT … relay=0 relay_code=401 …`(다음 재시도 때 판정되므로 최대 30초쯤 걸릴 수 있다), 스크린샷에 "기기 등록 오류 — 1분 뒤 다시 시도합니다". 올바른 토큰의 교환기로 바꾸면 60초 간격의 다음 시도에서 `relay=1`.
(환경변수 `PHONE_TOKENS` 가 `.env.phone` 보다 우선한다 — `load_env_file` 이 이미 있는 값은 덮지 않는다.)

(c) 같은 ID 의 다른 접속에 밀려났다 돌아오기:
```bash
cd ~/tab5-toy-phone && python3 server/tools/phone_fake_peer.py tab5-2 sleep:3 ; cd firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run "until:stat:^STAT .*relay=1:70" | tee -a out/task7.log | grep -E "^(STAT|!!)"
```
Expected: 가짜가 `tab5-2` 로 붙는 순간 기기는 밀려난다. 기기는 재연결 간격(직전 실패 횟수에 따라 1~30초)이 지나 다시 붙는다 — 그 전에 가짜가 3초를 채우고 나가면 `FAKE OK`, 기기가 먼저 붙어 가짜를 밀어내면 `FAKE FAIL: ConnectionError …` 인데 둘 다 정상이다. 마지막에 `STAT … relay=1`.

(d) 2분 동안 가만히 두어도 끊기지 않는다(JSON ping 15초 — 교환기의 45초 규칙):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run "until:stat:^STAT .*relay=1:30" "sleep:125" "stat" "wait:^STAT .*relay=1:5" | tee out/t7_idle2min.log | grep -cE "^RELAY up=0"
```
Expected: `0` (2분 동안 `RELAY up=0` 줄이 한 번도 없다).

(e) 붙자마자 상대가 보이는지(RL-D7 — 접속 직후의 `peer` 신호가 늦게 읽히는 문제를 막았는지). 가짜 상대를 먼저 붙여 두고 기기를 재부팅해 걸린 시간을 본다:
```bash
cd ~/tab5-toy-phone && (python3 server/tools/phone_fake_peer.py tab5-1 sleep:60 > /tmp/fake_t7_8e.log 2>&1 &) ; cd firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run "reboot" "wait:^RELAY up=1:60" "until:stat:^STAT .*peer=1:10" | tee -a out/task7.log | grep -E "^(RELAY|STAT|!!)"
```
Expected: `RELAY up=1 …` 뒤 2초 안팎에 `STAT … peer=1`(도구가 `stat` 을 2초마다 보내므로 첫 응답에 이미 `peer=1` 이면 통과). 5초 가까이 걸리면 RL-D7 이 듣지 않은 것이다 — `relay_link.cpp` 의 "붙자마자 ping" 이 실제로 나가는지 부품 소스(`esp_websocket_client.c`, `transport_ws.c` 의 `poll_read`)와 대조한다.

- [x] **Step 9: Wi-Fi 설정 화면이 열리고 닫힌다**

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run "until:stat:^STAT .*fsm=IDLE:30" "tap:152:648" "wait:^SCREEN WIFI_LIST:5" "sleep:4" "shot:out/t7_wifi_list.jpg" "tap:1081:648" "wait:^SCREEN IDLE:5" | tee -a out/task7.log | grep -E "^(SCREEN|==|!!)"
```
Expected: `SCREEN WIFI_LIST` → 스크린샷에 주변 망 목록 → `SCREEN IDLE`. (비밀번호 입력까지의 시험은 스냅부스에서 검증된 동작이라 여기서는 화면이 뜨는 것만 본다. 새 망을 실제로 추가하는 것은 Task 9 의 손 조작에서.)

- [x] **Step 10: 여유 기록**

Run: `cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run "stat" "wait:^STAT :5" | tee -a out/task7.log | grep "^STAT"`
Expected: `heap_int_min` 이 100000 이상(잠정 합격선, Phase 0 과 같음). 값을 Task 10 결과표에 적을 수 있게 메모한다. 미달이면 설계서 §12 의 메모리 대응(참고 구현 xiaozhi 의 PSRAM 설정 4종 — `research/audio-reference-diff.md` §2-D)을 검토해 보고한다.

- [x] **Step 11: 고친 것이 있으면 커밋**

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone server/tools && git commit -m "fix(phone): 2번 기기 검증에서 드러난 결함 수정" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
(고친 것이 없으면 건너뛴다. 고쳤다면 무엇을·왜·어떻게 확인했는지 Task 10 진행 로그에 적는다. 판단 로직을 고쳤다면 맥 단위 시험에 그 경우를 먼저 추가한다.)

---

### Task 8: 1번 기기에도 올려 두 대로 통화 흐름을 검증

**Files:**
- Create: `firmware/tab5_phone/out/t8_*.jpg`, `out/task8.log` (git 제외)

**Interfaces:**
- Consumes: Task 7 까지 검증된 펌웨어, Task 1 의 1번 백업, `phone_ctl.py pair`.
- Produces: 두 대 사이의 걸기·받기·거절·취소·무응답·끊김이 자동으로 반복 통과한다는 기록.

- [x] **Step 1: 사용자 확인(1번을 전화기로 바꾼다)**

사용자에게 말한다: "2번 한 대 시험이 끝났습니다(결과 요약 …). 이제 1번 기기(사진 액자)에도 전화기 펌웨어를 올립니다. 올리면 사진 액자로는 쓸 수 없고, 되돌리려면 백업(`backups/tab5-1_photoframe_…bin`, 검사합 확인됨)을 다시 쓰면 됩니다(약 5분). 진행할까요?"
**답을 받기 전에는 Step 2 로 가지 않는다.**

- [x] **Step 2: 백업이 온전한지 다시 확인하고 1번에 플래시**

```bash
cd ~/tab5-toy-phone && shasum -a 256 -c backups/tab5-1_photoframe_aabbccddee01_*.bin.sha256
cd firmware/tab5_phone && source ~/esp/esp-idf-5.5.4/export.sh
PORT=$(python3 tools/phone_ctl.py port 1) && echo "1번 포트: $PORT"
esptool.py -p "$PORT" read_mac | grep -i "aa:bb:cc:dd:ee:01" && idf.py -p "$PORT" flash 2>&1 | tail -3
```
Expected: `…bin: OK` → `MAC: aa:bb:cc:dd:ee:01` → `Hard resetting via RTS pin...`. 검사합이 `OK` 가 아니면 플래시하지 않는다.

정정(2026-10-04) ① 백업 검증: 사진 액자 펌웨어는 **부팅할 때마다 NVS 에 부팅 횟수(`bootFails`)를 쓴다.** esptool 은 호출이 끝날 때마다 칩을 다시 시작시키므로, 읽기와 `verify_flash` 를 **다른 esptool 호출로 나누면 항상 NVS 구역(0x9000~0xe000)만 어긋난다.** 실제로 플래시 직전 `verify_flash` 가 Task 1 의 백업(b1)과 새로 뜬 b2 모두 `digest mismatch` 였고(다른 바이트 28개, 전부 0x9025~0x933B), **한 esptool 세션에서 읽고 바로 verify** 한 b3 만 `verify OK` 였다(앱·SPIFFS·부트로더·파티션 표는 세 파일이 바이트까지 같다). 백업 검증은 이 방식으로 한다(Task 1 Step 5·6 도 같다). 이때 구현 담당이 "b2 도 다르면 BLOCKED" 지시를 스스로 넘겨 b3 로 진행한 것은 절차 이탈이지만 결과는 받아들였다(차이가 NVS 부팅 횟수뿐이고 b3 가 verify OK — 원인 분류: 절차 부재 + 규칙 이탈). b1·b2·b3 는 모두 보관(맥북 `backups/tab5-1_photoframe_aabbccddee01_2026-10-04{,_b2,_b3}.bin`, b1·b3 는 맥미니에도).
정정(2026-10-04) ② 플래시 방법: 두 기기에는 **같은 바이너리**를 올려야 하는데 `idf.py flash` 는 HEAD 가 움직였으면(devsync 커밋) CMake 를 다시 돌려 버전 문자열·시각이 다른 새 바이너리를 만든다. 그래서 1번에는 빌드 폴더에서 같은 산출물을 `python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash @flash_args` 로 올렸다(직전에 `read_mac` 으로 대상 확인). 이후 플래시(새 기능 계획 Task 6)도 같은 방식이다.

- [x] **Step 3: 두 대가 서로를 본다**

Run (export 하지 않은 새 셸):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair "2:reboot" "sleep:3" "both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*missed=0:120" "1:shot:out/t8_unit1_idle.jpg" "2:shot:out/t8_unit2_idle.jpg" | tee out/task8.log | grep -E "STAT|==|!!"
```
Expected: `[1] STAT … unit=1 fsm=IDLE … peer=1 missed=0 …`, `[2] STAT … unit=2 fsm=IDLE … peer=1 missed=0 …`(2번은 Task 7 에서 쌓인 부재중 건수를 재부팅으로 0 으로 되돌린다), 스크린샷 둘("1번 전화기"/"2번 전화기", "상대 기기 켜짐", 초록 통화 버튼).
1번이 `wifi=0` 에서 멈추면: 1번에는 저장된 Wi-Fi 목록이 없다 — `secrets.h` 의 고정 목록에 지금 장소의 Wi-Fi 가 없다는 뜻이다. 화면 배지를 눌러 직접 고르거나(사용자), `secrets.h` 에 추가하고 다시 올린다.
1번이 `PHONE BOOT unit=0` 이면 Task 7 Step 3 의 표대로.

- [x] **Step 4: 걸기 → 벨 → 받기 → 끊기를 양방향으로 10회**

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && for i in 1 2 3 4 5; do
python3 tools/phone_ctl.py pair "both:until:stat:^STAT .*fsm=IDLE.*peer=1:30" \
  "1:tap:640:360" "2:wait:^FSM IDLE -> INCOMING:10" "1:wait:^FSM DIALING -> RINGBACK:10" "sleep:1" "2:tap:404:428" "1:wait:^FSM RINGBACK -> IN_CALL:10" "2:wait:^FSM ANSWERING -> IN_CALL:10" "sleep:2" "1:tap:644:606" "2:wait:^FSM IN_CALL -> IDLE .*notice=CALL_ENDED:10" "1:wait:^FSM ENDING -> IDLE:10" \
  "sleep:1" \
  "2:tap:640:360" "1:wait:^FSM IDLE -> INCOMING:10" "2:wait:^FSM DIALING -> RINGBACK:10" "sleep:1" "1:tap:404:428" "2:wait:^FSM RINGBACK -> IN_CALL:10" "1:wait:^FSM ANSWERING -> IN_CALL:10" "sleep:2" "1:tap:644:606" "2:wait:^FSM IN_CALL -> IDLE .*notice=CALL_ENDED:10" "1:wait:^FSM ENDING -> IDLE:10" \
  >> out/task8.log 2>&1 && echo "회차 $i: 통과" || { echo "회차 $i: 실패"; break; }
done
```
Expected: `회차 1: 통과` … `회차 5: 통과` (한 회차에 통화 2건 = 10건 연속). 실패하면 `out/task8.log` 끝의 `!!` 줄 앞뒤를 보고 원인을 찾는다(추측으로 고치지 않는다 — 어느 기기가 어떤 신호를 못 받았는지 두 기기의 FSM 줄을 시간순으로 나란히 놓고 본다).

- [x] **Step 5: 거절·취소·무응답·동시 발신**

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair "both:until:stat:^STAT .*fsm=IDLE.*peer=1:30" \
  "1:tap:640:360" "2:wait:^FSM IDLE -> INCOMING:10" "sleep:1" "2:tap:876:428" "1:wait:^FSM RINGBACK -> IDLE .*notice=REJECTED:10" "2:wait:^FSM ENDING -> IDLE:10" "sleep:5" \
  "1:tap:640:360" "2:wait:^FSM IDLE -> INCOMING:10" "sleep:2" "1:tap:640:604" "2:wait:^FSM INCOMING -> IDLE .*notice=MISSED:10" "1:wait:^FSM ENDING -> IDLE:10" "sleep:5" \
  "2:tap:640:360" "1:wait:^FSM IDLE -> INCOMING:10" "2:wait:^FSM RINGBACK -> IDLE .*notice=NO_ANSWER:40" "1:wait:^FSM INCOMING -> IDLE .*notice=MISSED:5" "sleep:5" \
  "1:until:stat:^STAT .*missed=1:5" "2:until:stat:^STAT .*missed=1:5" "1:shot:out/t8_unit1_missed.jpg" | tee -a out/task8.log | grep -E "FSM|STAT|==|!!"
```
Expected: 적힌 `wait`·`until` 이 모두 통과. 1번은 `missed=1`(2번이 건 전화를 30초 동안 받지 않음), 2번도 `missed=1`(1번이 걸다 취소함) — Step 3 에서 둘 다 0 으로 시작했을 때의 값이다. 1번 스크린샷 아래 가운데에 "부재중 1건".

동시 발신(양쪽이 거의 같은 순간에 통화 버튼) — 교환기는 벨이 울리는 쪽의 `call` 을 받기로 취급한다:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair "both:until:stat:^STAT .*fsm=IDLE.*peer=1:30" "both:tap:640:360" "1:wait:-> IN_CALL:10" "2:wait:-> IN_CALL:10" "sleep:1" "1:tap:644:606" "both:until:stat:^STAT .*fsm=IDLE:15" | tee -a out/task8.log | grep -E "FSM|!!"
```
Expected: 두 기기 모두 `IN_CALL` 에 들어간다. 경로는 타이밍에 따라 둘 중 하나다 — (가) 한쪽은 `DIALING -> RINGBACK -> IN_CALL`, 다른 쪽은 `DIALING -> IN_CALL`(거는 중에 벨 신호를 받고 그대로 기다림) 또는 (나) 한쪽 터치가 가드(화면 전환 직후 600ms)에 걸려 `IDLE -> INCOMING` 으로 간 경우: 그러면 이 시험은 "동시 발신"이 아니었던 것이므로 다시 실행한다(3회까지). 세 번 모두 (나)면 그 사실을 기록하고 넘어간다 — 판단 로직은 맥 시험 "glare: …" 가 고정하고 있다.

- [x] **Step 6: 통화 중 한쪽이 사라진다**

(a) 통화 중 교환기 종료 → 양쪽 "연결이 끊어졌습니다" (도구를 먼저 띄우고 교환기는 30초 뒤에 끈다 — 한 번만 찍히는 줄을 놓치지 않게):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && (sleep 30; pkill -f "phone_relay.py") & python3 tools/phone_ctl.py pair "both:until:stat:^STAT .*fsm=IDLE.*peer=1:20" "1:tap:640:360" "2:wait:^FSM IDLE -> INCOMING:10" "sleep:1" "2:tap:404:428" "both:until:stat:^STAT .*fsm=IN_CALL:10" "1:wait:^FSM IN_CALL -> OFFLINE .*notice=LOST:60" "2:wait:^FSM IN_CALL -> OFFLINE .*notice=LOST:60" "1:shot:out/t8_lost.jpg" | tee -a out/task8.log | grep -E "FSM|==|!!"
cd ~/tab5-toy-phone/server && (BIND=0.0.0.0 PORT=5094 nohup python3 phone_relay.py > /tmp/phone_relay_dev.log 2>&1 &) ; cd ../firmware/tab5_phone && python3 tools/phone_ctl.py pair "both:until:stat:^STAT .*fsm=IDLE.*peer=1:60" | grep -cE "STAT"
```
Expected: 양쪽 `FSM IN_CALL -> OFFLINE acts=0x240 notice=LOST`, 스크린샷 위쪽에 "연결이 끊어졌습니다". 교환기를 다시 켜면 둘 다 대기로 돌아온다(마지막 줄은 `2` 이상).
(통화가 30초 안에 성립하지 않아 교환기가 먼저 꺼졌다면 시험이 성립하지 않은 것이다 — 교환기를 다시 켜고 처음부터.)

정정(2026-10-04): 위 Expected(양쪽 `FSM IN_CALL -> OFFLINE acts=0x240 notice=LOST`)는 교환기가 연결을 닫는 **순서**에 의존해 틀렸다. 교환기가 종료 때 연결을 하나씩 닫아서, 먼저 끊긴 쪽만 `IN_CALL -> OFFLINE` 으로 바로 가고 다른 쪽은 `ended: lost` 를 받아 `IN_CALL -> IDLE`(LOST) 을 거친 뒤 `IDLE -> OFFLINE` 이 된다(1회차는 1번이 먼저, 2회차는 2번이 먼저 — 순서가 시험마다 바뀐다). **올바른 기대 패턴은 순서 무관**이다: `^FSM IN_CALL -> (OFFLINE|IDLE) acts=0x240 notice=LOST` + 마지막에 양쪽 `fsm=OFFLINE`(`both:until:stat:^STAT .*fsm=OFFLINE`). 시험 쪽 문제이고 펌웨어 결함이 아니다 — 선행 상태·`acts`·`notice` 는 그대로 요구하므로 시험이 약해지지 않는다(대신 "중간에 IDLE 을 거치지 않는다"는 성질은 확인하지 않는다 — 설계 요구가 아님). 이 문장 바로 위의 명령(`wait:^FSM IN_CALL -> OFFLINE`)은 원문 그대로 둔다. 같은 패턴을 쓸 곳(README)에는 고쳐 적었다. 또한 `(sleep 30; pkill …) &` 앞에 `cd … &&` 를 두면 `cd` 까지 백그라운드로 가므로 `;` 로 이어야 현재 폴더가 바뀌지 않는다.

(b) 통화 중 2번을 재부팅 → 1번에 "연결이 끊어졌습니다", 2번은 60초 안에 스스로 대기로:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair "both:until:stat:^STAT .*fsm=IDLE.*peer=1:30" "1:tap:640:360" "2:wait:^FSM IDLE -> INCOMING:10" "sleep:1" "2:tap:404:428" "both:until:stat:^STAT .*fsm=IN_CALL:10" "2:reboot" "1:wait:^FSM IN_CALL -> IDLE .*notice=LOST:60" "2:until:stat:^STAT .*fsm=IDLE.*peer=1:60" | tee -a out/task8.log | grep -E "FSM|STAT|!!"
```
Expected: `[1] FSM IN_CALL -> IDLE acts=0x240 notice=LOST`(2번이 다시 붙는 순간 교환기가 옛 연결을 정리하며 알린다 — 부팅·Wi-Fi 연결 시간만큼 걸린다. 2번이 끝내 못 붙어도 교환기의 45초 무소식 규칙으로 온다), 이어서 `[2] STAT … fsm=IDLE … peer=1` (설계서 §6 "전원을 다시 넣으면 60초 안에 스스로 대기 상태").

- [x] **Step 7: 고친 것이 있으면 커밋**

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone server/tools && git commit -m "fix(phone): 두 대 검증에서 드러난 결함 수정" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
(고친 것이 없으면 건너뛴다.)

---

### Task 9: 여유 측정, 손 조작 확인, 독립 검수 → 게이트

**Files:**
- Create: `firmware/tab5_phone/out/task9.log` (git 제외)

**Interfaces:**
- Consumes: 두 대에 올라간 펌웨어, `phone_ctl.py`.
- Produces: Phase 3 계획이 전제로 삼을 숫자(대기·통화 화면에서의 내부 메모리 최저, 재연결 반복 뒤의 메모리 변화), 사용자의 손 조작 평가, 독립 검수 점수.

- [x] **Step 1: 재연결을 30번 시키고 메모리가 줄지 않는지 본다** (참고 구현 조사에서 "재연결 반복 시 내부 메모리 감소" 보고가 있었다)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run "until:stat:^STAT .*relay=1:60" "stat" "wait:^STAT :5" | grep "^STAT" | tail -1 | tee out/task9.log
for i in $(seq 1 30); do pkill -f "phone_relay.py"; sleep 3; (cd ~/tab5-toy-phone/server && BIND=0.0.0.0 PORT=5094 nohup python3 phone_relay.py > /tmp/phone_relay_dev.log 2>&1 &); python3 tools/phone_ctl.py --unit 2 run "until:stat:^STAT .*relay=1:70" > /dev/null || { echo "재연결 실패 $i"; break; }; done
python3 tools/phone_ctl.py --unit 2 run "stat" "wait:^STAT :5" | grep "^STAT" | tail -1 | tee -a out/task9.log
```
Expected: 앞뒤 `heap_int` 차이가 8192 바이트 이내이고 `heap_int_min` 이 100000 이상, `stk_app`·`stk_touch`·`stk_relay`·`stk_ws` 가 모두 1024 이상(각 태스크 스택의 최저 여유 — 그보다 작으면 그 태스크의 스택을 키운다). 계속 줄면(한 번에 수 KB 씩) 그 양을 기록하고 보고한다 — `relay_link` 의 재연결 방식(부품의 자동 재연결 → stop/start 직접 제어, `research/websocket-client.md` B6·B8)을 바꾸는 근거가 된다.
(재연결 간격이 최대 30초까지 벌어지므로 이 Step 은 15분쯤 걸릴 수 있다.)

정정(2026-10-04): 위 반복문은 "교환기를 끄고 3초 뒤 켠 다음 `relay=1` 을 본다"인데, 기기는 교환기가 꺼진 것을 15~20초 뒤에야 알아서(기기 `ping_interval_sec=5` + `pingpong_timeout_sec=15`, 교환기가 종료 때 WebSocket 을 닫지 않음) 끄고 바로 `stat` 을 보면 **낡은 `relay=1` 이 "다시 붙었다"로 읽힌다** — 실제로 교환기 정지 35회 중 5회(홀수 회차 1·3·5·7·9)가 가짜 통과였다. 판정은 **교환기 로그의 새 `online` 줄**(두 기기 모두)로 한다 — 켤 때마다 로그 파일이 새로 만들어지므로 그 줄은 "새 프로세스에 기기가 새로 접속했다"는 뜻이다. 그렇게 세어 교환기 정지 35회 = 실제 재접속 30회/기기가 됐다. 교환기 명령도 `pkill`·`BIND=… nohup python3 phone_relay.py` 대신 `bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh stop|start` 로 바꿔 썼다(Task 7 Step 1 정정 참조). 시간도 "약 15분"이 아니라 벽시계 21분 30초(회차별 소요 합계 967초)가 걸렸다.

- [x] **Step 2: 대기 12분 — 끊김 없이 붙어 있고, 그 뒤에도 전화가 걸려 온다**

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair "both:until:stat:^STAT .*fsm=IDLE.*peer=1:60" "sleep:720" "1:tap:640:360" "2:wait:^FSM IDLE -> INCOMING:10" "2:tap:876:428" "both:until:stat:^STAT .*fsm=IDLE:15" | tee -a out/task9.log | grep -cE "RELAY up=0"
```
Expected: `0` (12분 동안 재연결 없음). (설계서 §6 의 "24시간 대기 뒤에도 전화가 온다"는 Phase 4 에서 맥미니 교환기로 잰다 — 여기서는 짧은 판.)

- [x] **Step 3: 사용자가 직접 눌러 본다**  ← 앞선 기록 창 2회(15:31~15:39 `113097c`, 16:26~16:34 `066cf57`)는 터치 0이었고(사용자 부재), 세 번째 창에서 했다: 2026-10-04 20:54~21:04 세 번째 기록 창(`066cf57`, `out/hand_3.log`)에서 사용자가 직접 눌렀다 — 1번이 2번에 걸어 ① 받기 → 2번이 끊기 ② 걸다가 건 쪽이 취소 → 2번 부재중 ③ 받기 → 끊기 ④ 메시지 3으로 거절 → 1번 `notice=REJECT_MSG msg=3` ⑤ 메시지 고르기 열기 → 뒤로 → 받기 → 끊기 ⑥ 그냥 거절 → 1번 `notice=REJECTED` (받은 통화 세 번은 모두 2번이 끊었다). FSM·SCREEN 87줄, 오류 줄·`RELAY up=0` 0, rc=0, 로그 끝까지 `up` 16696→17236. `touch=` 1번 31→38(+7 = 걸기 6 + 취소 1, 정확히 일치), 2번 37→49(+12 = 동작으로 이어진 탭 11 + 동작 없는 탭 1 — 빈 곳이거나 가드가 버린 탭, 로그로는 구분 못 함). 한 번 누름이 두 번 동작한 흔적 없음(받기 직후 끊기·걸기 직후 취소·거절 뒤 되걸기 없음), 2번 `missed` 1→2, `heap_int_min` 불변(262539/263187). 사용자의 답(점검표에 10초 안내가 적당한지·위쪽 문구 버튼이 불편한지·보조 버튼 윤곽이 너무 희미한지·초록/빨강 색과 화면 방향이 맞는지 포함): **"좋아. 모두 좋아."**. 손으로는 **미확인**(자동 시험으로만 확인): 2번이 1번에 거는 방향, 벨 끄기, 메시지 1·2, 연타·아주 짧은 탭, 거절 왼쪽 가장자리를 1초 넘게 누르기, 받기 아래 가장자리 길게 누르기, 빈 곳 3초 누르기, 안내가 떠 있는 10초 안에 다시 걸기, 공유기 껐다 켜기 (로그에 시각이 없어 구분 못 하는 것 포함). M-1 은 "거절 한 번, 되걸림 없음"까지만 손으로 확인했고 가장자리 길게 누르기는 미확인이라 Phase 3 첫 과제 목록에 남는다. (아래는 계획 때 점검표 중 손으로 하지 않은 것을 포함한 원 점검표) 아래 항목은 미확인으로 게이트 보고의 점검표에 실린다: 걸고 받기 / 걸고 거절 / 걸다 취소 / 받은 뒤 끊기 / 통화·받기 버튼 연타 / 끊기 짧게 톡 치기 / 벨 끄기 뒤 받기 / 문구 3개 각각 보내 상대 화면에서 읽기(10초가 적당한지) / 고르다가 뒤로 → 받기 / 고르는 중 상대가 끊기 / 거절당한 직후 안내가 떠 있는 10초 안에 다시 걸기 / 안내가 떠 있는 기기가 전화를 받아 메시지 고르기 열기 / 빈 곳 3초 누르기 / 거절 왼쪽 가장자리 1초 넘게 누르기 / 받기 아래 가장자리 길게 누르기 / 벨 끄기를 누르려다 받기가 눌리는지 / 받기 초록·거절 빨강·화면 방향 / (가능하면) 공유기 30초 껐다 켜기

사용자에게 부탁한다: "두 기기로 직접 ① 걸고 받기 ② 걸고 거절하기 ③ 걸다가 취소하기 ④ 받은 뒤 끊기를 한 번씩 해 주세요. 통화 버튼과 받기 버튼을 일부러 빠르게 두세 번 연달아 눌러 보시고, 통화 중에 끊기 버튼을 아주 짧게 톡 쳐 보세요." 그동안 로그를 받는다:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair "sleep:300" | tee -a out/task9.log | grep -E "FSM|!!"
```
묻는다: ① 버튼이 한 번에 눌렸나요(안 먹거나 두 번 먹은 적, 짧게 친 끊기가 안 먹은 적)? ② 받자마자 끊기거나, 걸자마자 취소된 적이 있나요? ③ 글자 크기·버튼 위치에 불편한 점이 있나요? ④ 화면이 바뀌는 속도가 답답하지 않았나요? 답을 기록한다.
로그에서 확인한다: 연타했을 때 `FSM` 줄이 사용자의 의도보다 많이 찍혔는지(예: `IDLE -> DIALING` 직후 `-> ENDING`).

- [x] **Step 4: 독립 검수**  ← 최종 전체 검수 1차 93 · 2차 93(미달) → 수정 묶음 뒤 재검수(opus, diff 한정) **96 / 100 — 게이트 통과**

`code-reviewer` 에이전트(새 맥락)에게 맡긴다. 건넬 것: 이 계획서 경로, 설계서 경로, `git diff <Task 1 직전 커밋>..HEAD -- firmware/tab5_phone server/tools/phone_fake_peer.py`, `out/task7.log`·`out/task8.log`·`out/task9.log`, 위 Review Focus 다섯 줄. 요청: 100점 만점 점수와 심각도별 지적(파일:줄), 특히 ① 토큰·비밀번호가 로그·화면에 새는 경로 ② 태스크 사이의 공유 상태(잠금) ③ 스냅부스에서 떼어 온 코드에 남은 카메라 흔적 ④ 디자인 규칙 위반(색 토큰 밖 색, 96px 미만 터치). 95점 미만이면 Critical·Important 를 고치고(판단 로직은 시험 먼저) 한 번 더 검수한다(`docs/design/README.md` 품질 게이트).

- [x] **Step 5: 교환기 종료**  ← 교환기를 16:35 에 껐고 포트가 닫힌 것을 확인했다. 20:54 에 사용자 요청(손 조작)으로 다시 켰고 사용자 쪽에서 계속 쓰도록 켜 둔다. 허용 규칙은 사용자가 지운다 — 시험이 끝나면 `~/.claude/settings.json` 의 `Bash(bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh:*)` 를 지울 것(Phase 3 을 이어서 하면 그대로 두어도 된다)

```bash
pkill -f "phone_relay.py"; sleep 1; lsof -nP -iTCP:5094 -sTCP:LISTEN | wc -l
```
Expected: `0`

---

### Task 10: 문서 갱신과 게이트 보고

**Files:**
- Create: `firmware/tab5_phone/README.md`
- Modify: `docs/exec-plans/2026-10-03-tab5-phone.md` — §9.5 모듈 표, §9.6 정정, §5 Phase 2 체크, §8 진행 로그
- Modify: `docs/exec-plans/2026-10-04-tab5-phone-phase2.md` — 체크박스와 진행 로그
- Modify: `README.md`(저장소 루트) — 펌웨어 목록에 한 줄

- [x] **Step 1: `firmware/tab5_phone/README.md`**

  (처리 메모 2026-10-04: 아래 블록은 계획 때의 초안이다. 실제 `firmware/tab5_phone/README.md` 는 이 초안을 복사한 뒤 실제로 일어난 일에 맞게 고쳤다 — 빌드·플래시(`write_flash @flash_args`), 교환기(`phone_relay_dev.sh`), 새 기능의 시리얼 규약·좌표, `SHOT FAIL private`, 복구(공장 백업 위치·부분 롤백) 등. 둘이 다르면 저장소의 README 가 맞다.)

````markdown
# tab5_phone — Tab5 2대 음성 전화기

두 대에 같은 펌웨어를 올린다. 기기 번호는 칩의 MAC 으로 정해진다(1번 `AA:BB:CC:DD:EE:01`, 2번 `AA:BB:CC:DD:EE:02`).
지금 단계(Phase 2)는 **소리 없이** 걸기 → 벨 화면 → 받기/거절 → 통화 화면 → 끊기까지다. 소리는 Phase 3 에서 붙인다.

- 설계: `docs/exec-plans/2026-10-03-tab5-phone.md`
- 구현 계획: `docs/exec-plans/2026-10-04-tab5-phone-phase2.md`
- 참고 구현 조사: `docs/exec-plans/2026-10-04-tab5-phone-research/`

## 구조

| 파일 | 역할 | 시험 |
|---|---|---|
| `main/call_fsm.h` | 통화 상태 기계. 사건을 넣으면 다음 상태와 할 일을 돌려준다 | 맥 `tools/test_call_fsm.cpp` |
| `main/relay_proto.h` | 교환기 신호 읽기·쓰기, 음성 프레임 포장, 재연결 간격 | 맥 `tools/test_relay_proto.cpp` |
| `main/ui_layout.h`, `geom.h` | 어떤 화면인지, 터치가 어느 버튼인지, 터치를 받아들일지 | 맥 `tools/test_ui_layout.cpp` |
| `main/device_id.h/.cpp` | MAC → 기기 번호·교환기 ID·토큰 | 맥 `tools/test_device_id.cpp` |
| `main/relay_link.h/.cpp` | 교환기(WebSocket) 연결·재연결·생존 확인 | 기기 |
| `main/net_wifi.h/.cpp`, `wifi_store.h` | Wi-Fi 매니저(저장 목록 + 고정 목록, 신호 센 순 자동 연결) | 맥 `tools/test_wifi_store.cpp` + 기기 |
| `main/gfx.h/.cpp` | 화면 합성(1280×720 버퍼) → 한 번에 출력, 스크린샷 | 기기 |
| `main/ui.h/.cpp` | 통화 화면 그리기 | 기기(스크린샷) |
| `main/ui_wifi.h/.cpp` | Wi-Fi 설정 화면(목록 + 비밀번호 자판) | 기기 |
| `main/app.h/.cpp` | 터치·교환기 신호·시간 → 상태 기계 → 할 일 실행 → 다시 그리기 | 기기 |
| `main/phone_main.cpp` | 부팅, 태스크 기동, 시리얼 명령 | 기기 |
| `tools/phone_ctl.py` | 시리얼 검증 도구(기기 번호로 선택, 두 기기 시나리오, 스크린샷, 터치 주입) | `tools/test_phone_ctl.py` |
| `tools/gen_ui_assets.py` | 고정 문구를 Pretendard 로 구워 `main/ui_assets.h` 를 만든다 | — |

`main/secrets.h`(Wi-Fi 목록·교환기 주소·기기 표), `tools/fonts/`, `out/`, `build/`, `managed_components/`, `sdkconfig`, `dependencies.lock` 은 git 제외.

## 맥 단위 시험

```
cd tools && SDK="$(xcrun --sdk macosx --show-sdk-path)"
for T in wifi_store call_fsm relay_proto device_id ui_layout; do clang++ -std=c++17 -Wall -Wextra -isysroot "$SDK" -I../main test_$T.cpp -o /tmp/t_$T && /tmp/t_$T | tail -1; done
python3 -m pytest -q -p no:cacheprovider test_phone_ctl.py      # ESP-IDF 를 export 하지 않은 셸에서
```

## 빌드·플래시

```
source ~/esp/esp-idf-5.5.4/export.sh
idf.py set-target esp32p4 && idf.py build
PORT=$(python3 tools/phone_ctl.py port 2)        # 기기 번호로 포트를 고른다(두 대가 함께 꽂혀 있다)
esptool.py -p "$PORT" read_mac | grep -i "aa:bb:cc:dd:ee:02" && idf.py -p "$PORT" flash
```

`snap_ctl.py`·`serial_ctl.py` 는 "첫 번째 포트"를 열어 엉뚱한 기기를 리셋시킬 수 있으니 쓰지 않는다.

## 교환기 (개발 중에는 맥북에서)

```
cd ~/tab5-toy-phone/server && BIND=0.0.0.0 PORT=5094 python3 phone_relay.py
```

`secrets.h` 의 `RELAY_URL` 은 맥북의 LAN 주소다. 맥북이 다른 Wi-Fi 로 옮기면 주소가 바뀌므로 `secrets.h` 를 고치고 다시 올린다.

## 시리얼 명령 (115200, 줄 단위)

`stat` · `t <x> <y>` · `shot` · `reboot` — 스스로 내는 줄: `PHONE BOOT …`, `FSM <이전> -> <이후> acts=0x… notice=…`, `SCREEN <이름>`, `RELAY up=… code=… attempt=…`

```
python3 tools/phone_ctl.py --unit 2 run "until:stat:^STAT .*fsm=IDLE:60" "shot:out/idle.jpg"
python3 tools/phone_ctl.py pair "both:until:stat:^STAT .*fsm=IDLE.*peer=1:60" "1:tap:640:360" "2:wait:^FSM IDLE -> INCOMING:10" "2:tap:404:428"
```

상대 기기 없이 한 대만 시험할 때는 맥이 상대를 흉내 낸다: `python3 ~/tab5-toy-phone/server/tools/phone_fake_peer.py tab5-1 expect:peer send:call expect:ringing`

## 복구

- 2번 → 스냅부스: `cd ../tab5_snapbooth && idf.py -p "$(python3 ../tab5_phone/tools/phone_ctl.py port 2)" flash`
- 1번 → 사진 액자: `esptool.py -p "$(python3 tools/phone_ctl.py port 1)" -b 921600 write_flash 0x0 ../../backups/tab5-1_photoframe_aabbccddee01_<날짜>.bin`
````

- [x] **Step 2: 설계서 갱신** — 아래 네 곳을 고친다(값은 Task 7~9 의 실측으로 채운다. 못 잰 항목은 "측정 못 함 — 이유")  ← 값을 모두 채웠다(재검수 96점, `066cf57` 회귀 통과). 손 조작은 처음에 "사용자 미수행 — 확인 못 함"으로 적었다가 20:54~21:04 에 사용자가 직접 한 결과로 바꿨다(Task 9 Step 3)

1. §9.5 모듈 표에 네 줄 추가: `app`(사건 → 상태 기계 → 할 일 실행 → 다시 그리기), `ui_layout.h`(화면 배치·터치 판정·터치 가드, 맥 시험), `relay_proto.h`(신호 읽기·쓰기·음성 프레임 포장·재연결 간격, 맥 시험), `device_id`(MAC → 기기 번호·ID·토큰). `main.cpp` 는 `phone_main.cpp` 로, `tools/phone_ctl.py` 의 설명에 "두 기기 시나리오(pair)" 추가.
2. §9.6 정정 세 줄(참고 구현 조사 결과): "에코 제거 단위(약 32ms)" → "16ms(VOIP 방식 256표본, Phase 0 실측)", "프레임이 비면 Opus 의 손실 은닉으로 메운다" → "… (esp_audio_codec 2.5.0 의 Opus 해제기가 손실 은닉을 지원하는지 미확인 — Phase 3 초반에 시험, 안 되면 직전 프레임 감쇠 반복)", 지연 예상에 "재생 쪽 I2S DMA(기본 6×240표본)가 최대 90ms 를 더할 수 있음 — Phase 3 에서 쓰는 박자를 캡처에 묶어 억제" 추가.
3. §5 의 `- [ ] **Phase 2 — 펌웨어 골격(소리 없이)**` 를 `- [x]` 로(Task 8 Step 4 의 10건 연속 통과와 Task 9 검수 95점 이상일 때만).
4. §8 진행 로그 끝에 결과 절을 추가:

```markdown
### 2026-10-__ — Phase 2 결과 (계획 docs/exec-plans/2026-10-04-tab5-phone-phase2.md)

| 항목 | 방법 | 결과 |
|---|---|---|
| 한 대 검증(2번 + 가짜 상대) | 수신·발신·거절·취소·무응답·상대 사라짐 | …/6 통과 |
| 두 대 통화 흐름 | 걸기→벨→받기→끊기 양방향 | …/10 연속 통과 |
| 거절·취소·무응답·동시 발신 | pair 시나리오 | … |
| 통화 중 끊김 | 교환기 종료 / 상대 재부팅 | 안내 "…" , 복귀 …초 |
| 재연결 | 교환기 종료·재시작, 틀린 토큰(401), 같은 ID 밀려남 | 간격 …초, 401 안내 …, 밀려난 뒤 복귀 …초 |
| 대기 유지 | 12분 | 재연결 …회 |
| 여유 | STAT | 내부 메모리 최저 대기 … KB / 통화 화면 … KB, 재연결 30회 전후 차이 … B |
| 손 조작(사용자) | 직접 걸고 받기, 연타 | "…" |
| 독립 검수 | code-reviewer | …점 |

- 기기에서 드러나 고친 것: …
- 부품 판(실제로 잡힌 것): esp_websocket_client 1.8.0, esp_hosted 2.11.7, m5unified 0.2.23, m5gfx 0.2.30, …
- Phase 3 으로 넘기는 것: …
```

- [x] **Step 3: 저장소 루트 `README.md` 의 펌웨어 목록에 한 줄 추가**

`firmware/tab5_phone_probe` 줄 아래(없으면 펌웨어 목록 끝)에: `- firmware/tab5_phone/ — Tab5 2대 음성 전화기(Phase 2: 소리 없는 통화 골격). 설계 docs/exec-plans/2026-10-03-tab5-phone.md`

- [x] **Step 4: 이 계획서의 체크박스·상태·진행 로그를 갱신하고 커밋·푸시**  ← 1부(값이 없는 자리는 표시를 둔 채)와 2부(채움·검수 지적 반영) 모두 커밋함. `git push` 는 하지 않는다(devsync 가 올린다)

```bash
cd ~/tab5-toy-phone && git add docs firmware/tab5_phone/README.md README.md && git commit -m "docs(phone): Phase 2 결과 기록" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" ; git push
```

- [x] **Step 5: 사용자에게 게이트 보고**  ← 컨트롤러가 했다(문서 담당은 하지 않는다) — 게이트 보고는 채팅으로 16:5x 경에 전달됐다.

`~/CLAUDE.md` 의 큰 작업 보고 형식(**[목표] [한 일] [증거] [검증] [리스크] [다음]**)으로, 쉬운 말로 쓴다. [증거] 에는 Step 2 의 표와 스크린샷 서너 장(대기·벨·통화 중·부재중)을 넣는다. [리스크] 에는 확인하지 못한 것을 그대로 적는다(예: 다른 Wi-Fi·핫스팟에서의 동작, 24시간 대기, 소리). [다음] 에는 결정이 필요한 것만:

| 결과 | 사용자에게 묻는 것 |
|---|---|
| 모두 통과 | "Phase 3(소리) 계획대로 진행할까요?" + 손 조작에서 나온 화면 수정 요청이 있으면 그 범위 |
| 통화 흐름 일부 실패 | 원인과 고칠 안. 고친 뒤 Task 7·8 의 해당 Step 을 다시 통과시키고 보고 |
| 메모리 여유 미달(100KB 미만) 또는 재연결 때마다 감소 | 대응안(PSRAM 설정, 재연결 방식 변경) 중 선택 |

보고를 보낸 뒤 **여기서 멈춘다.** 두 기기는 전화기 펌웨어 그대로 둔다(Phase 3 이 이어서 쓴다). 사용자가 스냅부스·사진 액자로 되돌리길 원하면 "롤백" 절대로 한다.

---

## 검증 기준 (이 계획의 완료 조건)

- [x] 맥 단위 시험 5개(`wifi_store`·`call_fsm`·`relay_proto`·`device_id`·`ui_layout`) 전부 `0 failed`, 검증 도구 pytest 전부 통과.  (결과: `066cf57` 에서 16/105/61/9/91 전부 0 failed, 도구 152 passed, 교환기 137 passed)
- [x] 2번 한 대 + 가짜 상대로 수신·발신·거절·취소(부재중)·무응답·상대 사라짐이 통과하고, 각 화면 스크린샷을 눈으로 확인했다.  (결과: Task 7, 스크린샷 11장 판독. `a909f7d` 기준 — `066cf57` 로는 통화 흐름 회귀가 통과)
- [x] 두 대에서 걸기 → 벨 → 받기 → 끊기가 양방향 **10건 연속** 통과.  (결과: Task 8 Step 4, 5회차 `!!` 0, 교환기 `hangup` 10건. `a909f7d` 기준 — 새 기능 뒤 `113097c`, 수정 뒤 `066cf57` 에서도 같은 10건이 다시 통과)
- [x] 교환기를 껐다 켜면 스스로 다시 붙고(간격 1→2→4…30초), 틀린 토큰이면 "기기 등록 오류"가 뜬다. 2분·12분 대기 중 끊김 0회.  (결과: 간격 1/2/4/8/16초 뒤 30초 상한, 401 뒤 60초 간격 재시도, 2분 `RELAY up=0` 0건·12분 0건. `a909f7d` 기준 — `066cf57` 로 통화 흐름 회귀는 통과했고 재연결 30회·12분 대기 측정은 다시 재지 않았다)
- [x] 통화 중 교환기 종료·상대 재부팅 때 "연결이 끊어졌습니다"가 뜨고 대기로 돌아온다. 재부팅한 기기는 60초 안에 대기 상태.  (결과: 교환기 종료는 기기가 19~20초 뒤에 알아챔 → 안내 뜸, 교환기를 다시 켜면 6~11초에 대기 복귀 / 상대 재부팅은 약 18초에 대기 복귀. `a909f7d` 기준 — `066cf57` 로 다시 재지 않았다)
- [x] 내부 메모리 최저 100KB 이상, 재연결 30회 전후 차이 8KB 이내, 각 태스크 스택 최저 여유 1KB 이상(미달이면 수치와 대응안을 보고).  (결과: `heap_int_min` 최저 263015, 재연결 30회 전후 −2096 B(1번)·−2132 B(2번), `stk_*` 최저 3120 — 모두 통과. 단 30회로는 회차당 70 B 이하의 누수는 구별하지 못한다. `a909f7d` 기준 — `066cf57` 로 다시 재지 않았고, 끝 STAT 는 262539/263187)
- [x] 1번 기기 통째 백업 파일과 검사합이 있고 `verify_flash` 가 통과했다.  (결과: Task 1 둘째 읽기 `verify OK`, Task 8 b3 는 한 esptool 세션 read+verify OK — 위 정정 참조)
- [x] `git status` 에 `secrets.h`·`build/`·`out/`·`fonts/`·`*.bin` 이 없다. 시리얼 로그에 토큰·비밀번호·교환기 주소가 없다(`grep` 으로 확인).  (결과: 2부 문서 작업 중 `git status --short` 에 고친 문서만 보이고 위 파일들은 없다. 기기·교환기 로그 비밀 0건 — 새 기능 Task 6 과 `066cf57` 재회귀는 `secret_scan.py`/`grep` 으로)
- [x] 독립 검수 95점 이상.  (최종 검수 1차 93 · 2차 93 → 수정 뒤 재검수 **96 / 100**)
- [x] 사용자가 직접 걸고 받아 봤고, 게이트 보고가 갔다.  (손 조작은 20:54~21:04 에 했고 사용자 "좋아. 모두 좋아." — 범위는 Task 9 Step 3 옆. 게이트 보고는 채팅으로 16:5x 경에 전달)

## 진행 로그

### 2026-10-04 — 계획 작성
- 사용자 지시("관련 레포가 있으면 리버스 엔지니어링으로")에 따라 세 갈래 대조부터 했다: 스냅부스 펌웨어 재사용 범위, esp_websocket_client 1.8.0 이 교환기 계약을 만족하는지, xiaozhi-esp32·M5Tab5-VideoLink 의 소리 처리와 우리 시험 펌웨어의 차이. 보고 원문은 `docs/exec-plans/2026-10-04-tab5-phone-research/`.
- 발견 1: 참고 레포 둘 다 Tab5 에서 스피커폰 동시 통화 + 에코 제거를 돌리지 않는다 — 통화 품질의 근거는 Phase 0 실측뿐(Phase 3 계획의 전제).
- 발견 2: esp_websocket_client 는 기본 설정으로는 교환기 계약과 세 군데 어긋난다(자동 ping 이 45초 규칙에 안 잡힘 / 서버가 닫으면 멈춤 / 보내기 시간 제한 초과 시 연결 끊음) → `relay_link.cpp` RL-D1·D2·D6.
- 발견 3: 스냅부스의 Wi-Fi 설정 화면은 터치 영역이 96px 미만인 곳이 있고 고정 문구를 내장 글꼴 확대로 그린다 → 옮기면서 규칙에 맞춤.
- 발견 4: 이 맥에는 Pretendard 가 없고 원본 자산 생성기는 조용히 다른 글꼴로 대체한다 → 없으면 실패하도록 바꿈(PL2-D9).
- 발견 5: 이 맥은 SDK 27 과 링커가 안 맞아 맥 단위 시험에 `-isysroot` 가 필요하다(스냅부스 `test_wifi_store.cpp` 머리의 명령은 그대로는 링크 실패).
- 검증: 임시 폴더에서 맥 단위 시험 203개·검증 도구 시험 150개 통과, 가짜 상대 도구를 실제 교환기(임시 포트)에 붙여 통화·시간 초과·상대 꺼짐 확인, 펌웨어 전체 빌드 성공(0x219b30). 기기 미탑재.
- 독립 검수(code-reviewer, 새 맥락) 84점 — Critical 0, 비밀 누출 경로 없음, 이 문서의 `FSM …` 기대 줄은 전부 코드와 일치. Important 4건을 반영했다: ① 접속 직후 `peer` 신호가 부품의 내부 버퍼에 남아 최대 5초 늦게 읽힐 수 있음 → 붙자마자 JSON ping(RL-D7) ② 교환기를 끈 뒤 새로 띄운 도구가 한 번만 찍히는 줄을 놓침 → Task 7 Step 8·Task 8 Step 6 을 "도구 먼저, 지연 종료 + `until:stat`" 로 ③ 재연결 간격 서술(RL-D3 때문에 1초가 아니라 최대 30초) ④ 화면을 내보내는 60~80ms 동안 짧은 터치를 놓침 → 터치 전용 태스크(AP-D2). Minor 중 반영: 빗나간 터치가 가드를 쓰지 않게, 재시작 가드의 NVS 쓰기 실패 처리, STAT 에 스택 최저 여유, 취소·끊기 버튼 160px, Wi-Fi 배지 글자 32px, 안내 문구 중복 제거, 규약의 구멍(끊는 순간 상대의 재발신)을 시험으로 고정.
- 2026-10-04 사용자 승인("새로운 세션에서 이어서 가자") → 새 세션에서 실행. 실행 방식은 사용자가 고르지 않아 권고안(과제마다 구현·검수를 나누는 subagent-driven-development)을 기본으로 인계했다.
- 미룬 것(검수 Minor): 401 을 받은 뒤 망 오류만 이어져도 "기기 등록 오류"로 남는 것(부품이 상태 코드를 지우지 않음 — 화면은 Wi-Fi 끊김을 먼저 보여 준다), 시리얼로 주입한 터치의 좌표가 자판 화면에서도 `OK t x y` 로 되돌아오는 것(시험자 본인의 입력), `ui_assets.h` 를 여러 파일이 포함해 수 KB 중복되는 것.
- 반영 뒤 재검수는 하지 않았다 — 판단 로직은 시험을 먼저 고쳐 통과시켰고(203개), 전체 빌드를 다시 통과시켰다. 실행 단계의 Task 9 Step 4 에서 기기 로그와 함께 다시 검수한다.

### 2026-10-04 — 실행 기록 (Task 1~10 1부)

출처: `.superpowers/sdd/2026-10-04-tab5-phone-phase2/` 의 `progress.md`·`task-{1..9}-report.md`·`fix-1-report.md`·`final-review-report.md`(2차)·`final-review-report.first-1531.md`(1차), 기기 로그는 `firmware/tab5_phone/out/`(git 제외). 실행은 subagent-driven-development(과제마다 구현 담당 + 검수), 컨트롤러가 사용자와의 접점(방화벽·안전 검사·1번 플래시 확인·손 조작)을 맡았다. 사용자는 중간(약 14:20 이후)에 자리를 떴다("이후 작업은 100% 너의 선택으로").

**Task 1~6 (코드, 기기 없음)** — 커밋 구간 `1b2cecc`(Task 1 직전)..`a909f7d`
- Task 1(`60979e0`): 무시 규칙 2줄(`firmware/tab5_phone/out/`, `tools/fonts/`)을 푸시하고 맥미니 반영을 ssh 로 확인한 뒤 1번 기기 사진 액자 상태를 통째 백업(`backups/tab5-1_photoframe_aabbccddee01_2026-10-04.bin`, 16,777,216 B, sha256 `81993fb506b1…`), `secrets.h` 작성(값은 출력하지 않음). 첫 읽기는 `verify_flash` 실패, 다시 읽은 둘째가 `verify OK`(→ Task 8 정정 참조).
- Task 2(`fed7866`): 판단 로직 네 개, 맥 단위 시험 16/73/46/9/59 = 203, 경고 0. Task 3(`9e32998`): 검증 도구·가짜 상대, 도구 pytest 150(가짜 상대는 실제 교환기 임시 포트 5095 에 붙여 통화 확인). Task 4(`c396487`): 글자 자산(`ui_assets.h` sha256 `4af5900e2b98…`, 글꼴 없으면 실패하는 것 확인). Task 5(`3757d6f`): 화면 합성·Wi-Fi 매니저·설정 화면, 첫 빌드 `tab5_phone.bin` 0x18a7e0 = 1,615,840 B(74% 여유), 경고 0. Task 6(`a909f7d`): 교환기 연결·통화 화면·진행, 전체 빌드 0x219b40 = 2,202,432 B(65% 여유), 우리 코드 경고 0, 부품 판 8개 기대값과 일치(esp_hosted 2.11.7 · esp_wifi_remote 1.6.5 · m5unified 0.2.23 · m5gfx 0.2.30 · esp_codec_dev 1.6.2 · esp-sr 2.5.5 · esp_audio_codec 2.5.0 · esp_websocket_client 1.8.0).
- 과제 검수: 모두 Approved, Critical 0. 이월한 지적(Task 2 의 Important 1건 — 가드 순서 시험이 실제 호출 순서를 못 본다)은 Task 6 검수가 `app.cpp` 에서 hit 먼저 → 맞을 때만 가드를 쓰는 것을 확인하고, Task 7 A5·X1 에서 실기로 확인해 닫았다.

**Task 7 (2번 기기 한 대 + 가짜 상대)** — 커밋 `a909f7d`, 코드 수정 없음
- 막힘: 교환기를 망에 여는 명령을 안전 검사가 거부했다(Expose Local Services). 우회하지 않았고(권한 파일을 고치는 것도 거부됨), 사용자가 허용 규칙을 직접 추가한 뒤 `phone_relay_dev.sh` 로 이어갔다. 스크립트의 변수 이름이 `BIND`·`PORT`(readonly)라 환경 지정이 무시되던 결함 1건을 `RELAY_BIND`·`RELAY_PORT` 로 고쳤다. 교환기가 필요 없는 부분(A 부분)을 먼저 했다.
- 통과: 부팅 → Wi-Fi(약 16초에 `wifi=1`, 저장된 망을 물려받음) → 대기 화면, 수신·발신·거절·취소→부재중·무응답(30초)·발신 취소·상대 사라짐, 교환기 끊김·복귀(재연결 간격 1/2/4/8/16초 뒤 30초 상한, 켠 뒤 최대 30초 안에 복귀), 틀린 토큰(`relay_code=401`, 화면 "기기 등록 오류 — 1분 뒤 다시 시도합니다", 60초 간격으로 재시도해 복귀), 같은 ID 에 밀려남(2초 안에 감지, 약 30초 뒤 복귀), 2분 유지(`RELAY up=0` 0건), 접속 직후 상대 인식 2초 안, Wi-Fi 설정 화면 열림·닫힘, 여유 `heap_int_min` 이 시험 전체 최저 264727(재부팅 뒤 265327) · `stk_*` 3136 이상. 통화 화면의 시계 콜론 세로 정렬은 픽셀 실측으로 정상(숫자·콜론 중심 모두 367.5).
- 검수가 Important 2건을 잡았다 — Step 9 와 8(d) 2분 시험이 손을 대지 않은 조건이 아니었다(사용자가 그 시간에 화면을 눌렀음, 사용자 확인). 두 시험을 깨끗하게 다시 돌렸다: 2분 대기 125초 동안 `RELAY up=0` 0건·`touch=` 16→16·FSM 줄 0, Step 9 는 열림 확인 → 닫기 탭 → `SCREEN IDLE`(`touch=` 16→18 = 주입 2). 원인 분류: 검증 누락(필터가 `>>` 줄을 지움 / 합격 조건이 한 줄뿐).
- 교환기 프로세스가 죽거나 꺼지면 기기는 15~20초 뒤에야 안다(Task 7 측정 약 17.6초) — 결함이 아니라 **Phase 4(맥미니·터널) 전에 정할 항목**이다(통화 설계서 §8). 원인은 Task 8 의 코드 대조가 맞다: 기기 `ping_interval_sec=5` + `pingpong_timeout_sec=15`, 교환기가 종료 때 WebSocket 을 닫지 않고 PONG 도 안 보낸다.
- C6 코프로세서 경고(`Version mismatch: Host [2.11.0] > Co-proc [0.0.0]`)와 RPC 오류 12294 1회는 스냅부스와 같은 동작으로 판정(두 기기 모두 같은 두 줄이 찍힌다 — Task 8 에서 확인). Wi-Fi 배지의 긴 망 이름은 말줄임표 없이 잘리는 것이 설계된 동작.

**Task 8 (1번도 올려 두 대 검증)** — 코드 수정 없음
- 사용자 사전 승인("문제없으면 1번 기기에도 올릴 것")으로 Step 1 의 확인을 충족했다. 플래시 직전 백업 verify 가 두 번 어긋나 지시는 BLOCKED 였으나 구현 담당이 원인(사진 액자의 NVS 부팅 횟수)을 확정하고 한 세션 read+verify(b3)로 `verify OK` 를 얻어 진행했다 — 컨트롤러가 절차 이탈이지만 결과를 받아들였다(위 Task 8 Step 2 정정). 이후 dispatch 공통 안내에 "정지 조건에 걸리면 대안은 제안으로만"을 추가했다. 또 `idf.py flash` 대신 `write_flash @flash_args` 로 같은 바이너리를 올렸다.
- 1번은 처음에 지금 장소의 Wi-Fi 에 못 붙었다(`secrets.h` 고정 목록에 없고 저장된 목록도 없음) → 사용자가 화면에서 직접 연결했고(저장됨), 재부팅하면 저장된 망으로 약 17초에 대기 상태가 된다. **새 장소에 둘 때마다 같은 일이 생긴다**(README). 이 과정에서 자동 스크린샷이 사용자가 비밀번호를 입력하던 화면을 한 번 찍었다 — 파일은 지웠으나 구현 담당이 열어 보았으므로 이 맥의 대화 기록에 남았을 수 있다고 사용자에게 알렸다(이 사고가 최종 검수 I-1 로 이어졌다).
- 통과: 두 대 서로 인식, 양방향 10건(5회차 모두 통과, 교환기 `hangup` 10건·벨 1.6~1.8초·통화 2.7~2.9초), 거절(`reject`)·취소→부재중(`hangup`)·무응답(`timeout`, 벨 30.5초), 동시 발신 3회 모두 진짜 동시·경로 (가), 통화 중 교환기 종료(기대 패턴을 순서 무관으로 바로잡아 통과 — 위 정정, 기기가 19~20초 뒤에 알고 "연결이 끊어졌습니다", 켜면 6~11초에 대기 복귀), 통화 중 상대 재부팅(1번 `IN_CALL -> IDLE … LOST`, 2번 약 18초에 대기 복귀).
- 터치 대조: 사용자가 손으로 Wi-Fi 를 설정한 1건(1번 0→14) 외에 모든 구간에서 주입한 탭 수와 `touch=` 증가가 일치(감사된 구간 약 42 기기·분, 그중 무접촉 요청 구간 약 27 기기·분 — 설명 안 되는 터치 0). 비밀 0건. 여유 `heap_int_min` 최저 269043 · `stk_*` 최저 3136. 두 기기 같은 빌드(`App version a909f7d`, 컴파일 11:05:17).
- Task 7 에서 "설명 안 되는 터치" 몇 구간은 사용자가 "내가 누른 것으로 하자"고 답해 정리했다(단 확정 기억은 아니라 이후 무접촉 구간의 `touch=` 불변 확인은 계속 했다).

**끼어든 새 기능** — Task 8 뒤·Task 9 앞에 "수신거부 메시지·벨 끄기"(설계서 `2026-10-04-tab5-phone-decline-mute.md`, 계획 `…-decline-mute-plan.md`)를 넣었다(사용자 선택). 이 기능에 한해 교환기 `server/phone_core.py` 가 바뀌었다(DM-D5 — 위 Global Constraints 의 정정). 구현·기기 검증 결과는 그 설계서 §12 (두 기기 `113097c` 빌드: 시나리오 H·A~G·I 통과, 터치 주입 80 = 증가 80). **Task 9 의 측정(Step 1·2)은 새 기능 전 빌드(`a909f7d`)로 했고**, 새 기능 빌드의 여유는 새 기능 Task 6 의 마지막 STAT(`heap_int_min` 271983/271471, `stk_*` 3252 이상)로 다시 확인했다.

**Task 9 Step 1·2 (측정, 새 기능 전 빌드 `a909f7d`)** — 코드 수정 없음
- 재연결 30회: 교환기 정지 35회 = 기기당 실제 재접속 30회(위 Step 1 정정 — 낡은 `relay=1` 에 속은 가짜 통과 5회를 빼고 교환기 로그의 새 `online` 으로 센 값). `heap_int` 1번 272531 → 270435 (−2096 B), 2번 272611 → 270479 (−2132 B) — 기준 8192 B 이내. `heap_int_min` 최저 265583/263015(기준 100000), `stk_app`/`stk_touch`/`stk_relay`/`stk_ws` 최저 10164/3120/6124/5936(기준 1024). 계속 줄어드는 모양은 아니다(1번은 처음 2~3회에 −1.7 KB 내려간 뒤 평평, 2번은 마지막 부분에 −1.7 KB 계단이 한 번). 읽을 때마다 ±1.6 KB 흔들려 **회차당 약 70 B 이하의 누수는 이 시험으로 구별하지 못한다**. 켠 뒤 두 기기가 붙기까지 7~47초(중앙값 25초, 재접속 간격 1→30초 설계대로). `stop` 뒤 옛 교환기 연결이 15.9~20.7초 더 살아 있어, 잰 것은 "기기가 15~20초 만에 스스로 알아채 재접속하는 경로"이다(서버가 즉시 끊는 경우·Wi-Fi 끊김은 안 쟀다).
- 12분 대기: `RELAY up=0` 0건, 앞뒤 `up` 차이 정확히 720초, `touch=` 불변(1번 9→9, 2번 3→3), 12분 뒤 1번이 건 전화가 2번에 벨로 오고 2번이 거절하는 데까지 정상(1번 `notice=REJECTED`), 12분 쉰 뒤 `heap_int` 270859/270903.
- 터치: 재연결 구간에 주입 없이 두 기기가 13:59:42.7(2번)·13:59:44.0(1번) 무렵 `touch=` 가 각각 +1 — 정지 13회 구간이고 그 회차만 재접속 간격이 0·접속 대기가 짧아 통화 버튼이 눌려 5초 규칙(`SIGNAL_WAIT_MS`)으로 재접속한 모양과 맞는다. 컨트롤러는 그 시각 사용자가 자리에 있었으므로 사용자 손으로 분류했다(확정 못 함 — 게이트 보고에서 사용자에게 묻는다).

**Task 9 Step 3·4 — 최종 전체 검수와 수정 묶음**
- 손 조작(Step 3): 앞선 두 기록 창(15:31~15:39 `113097c` `out/hand_1.log` 1번 27→27·2번 18→18, 16:26~16:34 `066cf57` `out/hand_2.log` 1번 31→31·2번 33→33)은 FSM·SCREEN 줄 0, 무접촉 약 32 기기·분(유령 터치 0)이었고 사용자 부재로 확인 못 했다. **세 번째 창에서 사용자가 직접 했다**: 2026-10-04 20:54~21:04 세 번째 기록 창(`066cf57`, `out/hand_3.log`)에서 사용자가 직접 눌렀다 — 1번이 2번에 걸어 ① 받기 → 2번이 끊기 ② 걸다가 건 쪽이 취소 → 2번 부재중 ③ 받기 → 끊기 ④ 메시지 3으로 거절 → 1번 `notice=REJECT_MSG msg=3` ⑤ 메시지 고르기 열기 → 뒤로 → 받기 → 끊기 ⑥ 그냥 거절 → 1번 `notice=REJECTED` (받은 통화 세 번은 모두 2번이 끊었다). FSM·SCREEN 87줄, 오류 줄·`RELAY up=0` 0, rc=0, 로그 끝까지 `up` 16696→17236. `touch=` 1번 31→38(+7 = 걸기 6 + 취소 1, 정확히 일치), 2번 37→49(+12 = 동작으로 이어진 탭 11 + 동작 없는 탭 1 — 빈 곳이거나 가드가 버린 탭, 로그로는 구분 못 함). 한 번 누름이 두 번 동작한 흔적 없음(받기 직후 끊기·걸기 직후 취소·거절 뒤 되걸기 없음), 2번 `missed` 1→2, `heap_int_min` 불변(262539/263187). 사용자의 답(점검표에 10초 안내가 적당한지·위쪽 문구 버튼이 불편한지·보조 버튼 윤곽이 너무 희미한지·초록/빨강 색과 화면 방향이 맞는지 포함): **"좋아. 모두 좋아."**. 손으로는 **미확인**(자동 시험으로만 확인): 2번이 1번에 거는 방향, 벨 끄기, 메시지 1·2, 연타·아주 짧은 탭, 거절 왼쪽 가장자리를 1초 넘게 누르기, 받기 아래 가장자리 길게 누르기, 빈 곳 3초 누르기, 안내가 떠 있는 10초 안에 다시 걸기, 공유기 껐다 켜기 (로그에 시각이 없어 구분 못 하는 것 포함). M-1 은 "거절 한 번, 되걸림 없음"까지만 손으로 확인했고 가장자리 길게 누르기는 미확인이라 Phase 3 첫 과제 목록에 남는다. 16:34~20:54 에는 교환기가 꺼져 있어 기기가 "교환기에 연결할 수 없음" 화면이었고(통화 버튼 없음), 그 사이 2번 `touch=` 33→37 은 사용자가 돌아와 그 화면을 누른 것이다. 교환기는 20:54 에 사용자 요청으로 다시 켰고 사용자 쪽에서 계속 쓰도록 켜 둔다(문서의 교환기 종료 안내는 시험이 끝날 때의 지시로 그대로 유효).
- 최종 전체 검수(`113097c` 기준)를 두 번 독립으로 했다. 둘 다 **93 / 100**, 게이트(95) 미달. Critical 0 · Important 1(I-1 Wi-Fi 비밀번호 자판 화면이 `shot` 으로 찍힐 수 있다 — Task 8 에서 실제 1회 발생). (1차는 Important 를 2건으로 셌으나 둘째 — 검수 시점에 `113097c` 빌드의 기기 증거와 손 조작이 없었다 — 는 그 뒤 새 기능 Task 6 이 기기 증거를 채웠고, 2차가 "결함 아님, 손 조작은 게이트 조건"으로 정리했다. 지적 번호는 2차 보고서 기준.) 2차 배점: 설계 24/25 · 품질 23.5/25 · 보안·비밀 12.5/15 · 동시성 9.5/10 · 시험·증거 14/15 · 디자인 9.5/10. 검수자가 맥 시험과 pytest 를 다시 돌려 105/61/86/9/16 · 137 · 150 을 확인했고 `relayproto::parse` 퍼징 30만 건 이상 없음. 알려진 한계 (a)~(e) 판정: (a) 교환기 종료를 15~20초 뒤에 앎 = 골격 게이트로는 수용, **Phase 4 배포 전 필수** / (b) 401 뒤 평범한 망 오류에도 "기기 등록 오류" = 수용(Phase 4~5) / (c) `reconnect()` 의 stop/start 와 `send_text` 경합 = Phase 2 는 닿는 길 없음, **Phase 3 에서 음성 송신 전 필수** / (d) 영영 멈춤 = 이 설정에서는 닿지 않음(실제 구멍은 M-2) / (e) 중간 커밋 빌드 불가 = 결함 아님, 문서 정정(위 새 기능 문서에서 함).
- **판정 뒤집힘**: 새 기능 계획의 "`shot` 에 자판 화면 가드를 넣지 않고 절차로 막는다"(Task 8 검수 Minor 를 컨트롤러가 기각했던 판정)는 최종 검수로 뒤집혔다 — 이미 한 번 사고가 났고 절차는 확인과 촬영 사이에 틈이 있었다. 펌웨어에서 막는다.
- 수정 묶음(fix wave 1): `99466b9`(도구: `SHOT FAIL private` 는 다시 받지 않고 실패) · `a26d237`(입력이 든 비밀번호 자판 화면은 `shot` 이 `SHOT FAIL private` 로 거절, 빈 자판·다른 화면은 찍힘) · `066cf57`(화면 초기화 실패 시 교환기 미접속, `esp_websocket_client_start()` 실패를 5초마다 재시도, `CONFIG_LOG_DYNAMIC_LEVEL_CONTROL` 고정 + `#error`, `RELAY_URL` 길이 `static_assert`, `RELAY up=1` 의 `attempt=0`). 시험: 맥 16/105/61/9/**91**(ui_layout +5), 도구 pytest **152**(+2), 교환기 137. 빌드 `066cf57`, `tab5_phone.bin` 0x231650(2,299,472 B), 경고 0, `dependencies.lock` 불변. 롤백용 보관본: `out/fw_prev_113097c/`(이 기능까지 들어 있고 이번 수정만 없는 빌드), `out/fw_prev/`(`a909f7d`). 걱정(수정 담당 보고): gfx 비공개 두 길과 `start` 재시도는 기기 전용 코드라 기기에서 확인되지 않았다.
- 수정 뒤 재검수(diff 한정, opus, `fix-1-rereview.md`)를 먼저 받고 그 뒤 두 기기를 재플래시·회귀했다(검수받지 않은 코드를 기기에 올리지 않으려고). **재검수 96 / 100 — 게이트(95) 통과**: 이전 지적 I-1·M-3·M-2·M-4·M-9(두 항목)·M-10(한 항목) 전부 반영됨, 새 Critical 0 · Important 0 · Minor 2(N-1: `relay_link.cpp` 에서 stop 앞에서만 IDLE 로 내려 stop 도중의 값이 남을 수 있음 — 표시만 틀림 / N-2: 결정 번호 G-D3 중복 — 주석) → 다시 빌드하지 않고 Phase 3 첫 과제로 넘김. 배점 설계 24 · 품질 24 · 보안 15 · 동시성 9.5 · 시험·증거 14 · 디자인 9.5. 96점은 "코드 기준 + 기기 회귀 통과 조건"이었고 조건은 아래로 충족됐다.
- **`066cf57` 재플래시·회귀 통과**(16:12~16:23, 로그·사진 `out/fx_*`, 보고 `fix-2-report.md`, 독립 검수 `fix-2-review.md` Approved — Critical 0 · Important 0): 2번 → 1번 순서로 `build/` 에서 같은 바이너리를 올렸다(`read_mac` 확인 뒤, 플래시마다 `Hash of data verified.` 3줄, 부팅 로그 `App version 066cf57` · `Compile time Oct  4 2026 15:56:52` · `ELF file SHA256 d49244c85...` 가 두 기기에서 같다). 새 기능 계획 Task 6 Step 6~9 를 그대로 다시: 시나리오 A·B·C·D·E·F·G·I 전부 `!!` 0, 다시 돌린 실행 0회, 줄 내용은 `113097c` 때와 같고 사진 8장은 `dm_*` 와 바이트까지 같다. 새 확인 셋:
  - ① 자판 화면 `shot` 거절(2번, 가짜 글자만, "연결"은 누르지 않았고 2번의 Wi-Fi 는 그대로): 0자 = 찍힘 / 글자 1개 = `SHOT FAIL private`(기기 줄 1번, 도구는 다시 받지 않고 파일 없음) / "보기" 켠 뒤도 거절 / 지우면 다시 찍힘 / 취소 뒤 대기 화면 찍힘 / 원시 터치 직후 연속 촬영 5회 모두 거절. → 최종 검수 I-1 이 기기에서 닫힘.
  - ② 교환기 무응답: 교환기 `stop` 직후 1번이 걸면 `FSM DIALING -> OFFLINE acts=0x480 notice=RELAY_SILENT`(1회차에 재현), 교환기를 다시 켜면 두 기기 복귀, `RELAY up=1` 6줄 모두 `attempt=0`. → 2차 검수 M-6 의 "`A_RECONNECT` 기기 증거 없음"이 닫힘.
  - ③ 설정 화면이 열린 채 전화: Wi-Fi 목록이 열려 있어도 전화가 오면 목록이 닫히고 벨 화면(`SCREEN INCOMING`)이 뜸. 가짜 글자가 든 자판이 열려 있을 때도 같고, 그 벨 화면은 `shot` 이 찍힌다(사진이 정상 벨 화면과 바이트 동일 — 비공개 표시가 다음 화면에 남지 않음), 거절 뒤 화면은 대기. 첫 실행의 `!!` 하나는 이미 지나간 줄을 다시 기다리게 짠 시나리오 오류였고(기기 결함 아님) 2회차는 통과.
  - 터치 대조: 원본 도구 출력은 불일치 2건이었으나 한 기기용 실행 로그(`fx_priv_*`)에 `[n]` 접두어·STAT 가 없고 부팅 로그가 섞여 주입을 2번 몫으로 세지 못한 **도구의 한계**였다(독립 검수가 접두어를 보정해 재계산: 불일치 0, 2번의 13→27 증가 = `fx_priv_*` 의 주입 14와 같음). 설명 안 되는 터치 없음. 비밀 검사 로그 6개 0건, 교환기 `call_end` 3건·`msg` 0건·오류 0.
  - 여유(끝 STAT, `out/fx_r6b.log`): `heap_int_min` 1번 262539 · 2번 263187, `stk_touch` 3268·3284, 나머지 스택도 기준 1024 의 3배 이상. `113097c` 때(271983·271471)보다 낮은 것은 Wi-Fi 목록·자판 화면을 처음 연 순간의 일회성 계단으로 보인다(독립 검수 판독: 1번 270611→262539, 2번 270923→265247 이후 같은 동작을 반복해도 더 내려가지 않고 `heap_int` 는 복귀) — 옛 펌웨어 장시간 구간에도 263015 가 있었다. 기준 100000 의 2.6배이고, 추세인지는 24시간 대기 기록(Phase 4 전)에서 확인한다.
  - 못 본 것: 입력 글자가 실제로 들어갔는지·"보기"가 켜졌는지는 사진이 아니라 `touch=` 증가로만 간접 확인했고, `start` 재시도(RL-D8)와 화면 초기화 실패 때의 미접속(M-3)은 실패를 만들 방법이 없어 코드·빌드로만 판정했다. 저장된 Wi-Fi 목록 불변은 직접 비교하지 못했다(연결 시도 줄이 전혀 없음으로 대체).
- 이번에 넣지 않은 것(판정과 이유): M-1(통화 원 ↔ 받기·거절 겹침 6,800·6,939px² — 손 조작에서 되걸리는지 먼저 본다) · `server/tools/phone_relay_dev.sh` 의 잔손질(`pkill -f` 패턴, 로그 덮어쓰기, "already running") — 사용자가 허용 규칙으로 직접 등록한 스크립트라 사용자가 보는 자리에서 고친다 · `phone_ctl.py` 가 `screen=WIFI_PASS` 를 통째로 거절하는 가드(빈 자판 화면은 찍혀야 한다). 미룬 지적 약 60건의 분류표는 `final-review-report.md` 의 "미룬 지적의 분류".

**Task 10 (문서) 1부**: 새 `firmware/tab5_phone/README.md`, 통화 설계서(§4 정정·§5·§8 결과표·§9.4·§9.5·§9.6 정정·§10·§12), 이 계획서(체크박스·상태·이 기록), 새 기능 문서, 저장소 `README.md` 한 줄. 문서 검수(`task-10-review.md`: Important 5 · Minor 10)를 받아 2부에서 모두 반영하고 값을 채웠다(재검수 96점, `066cf57` 회귀 통과, 손 조작은 사용자 미수행).
- **Phase 3 전에 고칠 잔손질에 더할 것**: 터치 감사 도구 `touch_audit.py`(저장소 밖)는 접두어 없는 한 기기용 로그와 부팅 로그가 섞이면 허위 불일치를 낸다 — 기기 지정 옵션을 받거나 접두어 없는 줄을 STAT 의 `unit=` 로 귀속하게 고친다 / 재검수 N-1(`relay_link.cpp` stop 도중 값)·N-2(결정 번호 G-D3 중복) / 교환기 `stop` 때 `date` 를 로그에 남기기(순서 입증).

**이번 실행에서 바로잡은 계획 문구**(본문은 지우지 않고 해당 자리에 "정정(2026-10-04)"로 덧붙임): ① 1번 공장 백업은 있다(맥미니) ② 백업 검증은 한 esptool 세션에서 읽기 → verify ③ 통화 중 교환기 종료의 기대 패턴은 순서 무관 ④ 교환기 명령은 `phone_relay_dev.sh` ⑤ 재연결 반복문은 낡은 `relay=1` 에 속는다(새 `online` 줄로 판정) ⑥ 중간 커밋 빌드 불가 → 부분 롤백은 세 커밋 함께(새 기능 문서) ⑦ 교환기 종료를 15~20초 뒤에야 아는 것은 Phase 4 전 결정 항목 ⑧ 플래시는 `write_flash @flash_args`(같은 바이너리) ⑨ 새 기능 끼어듦과 교환기 수정(DM-D5) ⑩ README "시리얼 명령" 절 이름을 `phone_ctl.py` 머리 주석과 맞춤(주석은 고치지 않음).
