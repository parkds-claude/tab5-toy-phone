# Tab5 음성 전화기 Phase 0~1 구현 계획 (가정 검증 + 교환기)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> (실행하는 에이전트에게: 위 두 스킬 중 하나로 과제를 하나씩 수행하고, 단계는 체크박스로 추적한다.)

- **작성일**: 2026-10-03
- **작성자**: Claude (맥북 세션)
- **상태**: 완료(2026-10-03) — Task 1~11 모두 끝. Phase 0 가정 H1·H2·H4 통과, H3 은 망 왕복 어림만 확인(미확인: 입→귀 지연, 말소리 부하, 30분 뒤 터치, 사용자 관찰 3문항 — 설계서 §8), Phase 1 교환기 완료(독립 검수 96점). **게이트: Phase 2~3 상세 계획 착수는 사용자 결정 대기.** 진행 기록: `.superpowers/sdd/2026-10-03-tab5-phone-phase0-1/progress.md`
- **관련 이슈/PR**: -

**Goal:** 전화기 설계의 미검증 가정 4개(H1~H4)를 2번 기기에서 숫자로 확인해 사용자에게 보고하고(Phase 0), 맥미니에 올릴 교환기(신호·음성 중계 서버)를 맥북에서 시험까지 끝낸다(Phase 1).

**Architecture:** Phase 0 은 버리는 시험 펌웨어 `firmware/tab5_phone_probe/` 한 개와 맥 쪽 측정 도구 두 개로 구성한다 — 화면·터치는 M5Unified, 소리는 esp_codec_dev 를 `M5.In_I2C` 위에 얹어 같은 버스를 쓰고, 시리얼 명령으로 시험을 돌려 결과를 맥으로 꺼낸다. Phase 1 은 판단을 전부 맡는 순수 로직 `phone_core.py`(네트워크·시계 없음)와 그것을 WebSocket 에 연결만 하는 `phone_relay.py`(aiohttp) 두 파일이다.

**Tech Stack:** ESP-IDF 5.5.4 (esp32p4) · M5Unified 0.2.23 / M5GFX 0.2.30 · esp_hosted 2.11.7 · esp_codec_dev 1.6.2 · esp-sr 2.5.5(에코 제거) · esp_audio_codec 2.5.0(Opus) · Python 3.11 · aiohttp 3.14 · pytest 9 + pytest-asyncio 1.3 · numpy · pyserial

**Spec:** `docs/exec-plans/2026-10-03-tab5-phone.md` (승인 2026-10-03). 이 계획은 그 설계서를 근거로 하므로 실행자는 두 문서를 함께 읽는다.

## 한눈에 보기 (사용자용)

| 묶음 | 하는 일 | 기기 | 사용자가 할 일 | 걸리는 시간(어림) |
|---|---|---|---|---|
| Task 1 | 준비(무시 규칙, 기기 확인) | 2번 연결 확인 | 2번 기기 전원·케이블 확인 | 5분 |
| Task 2~3 | 측정 도구·시험 펌웨어 작성, 빌드 | 없음 | 없음 | 20분(첫 빌드 내려받기 포함) |
| Task 4 | 2번에 올려 소리 장치 확인 | **2번** | "삐" 소리 들림 확인, 화면 한 번 터치 | 10분 |
| Task 5 | 에코 제거·처리 여유 측정(H2·H4) | **2번** | 조용한 방, 녹음 두 개 듣고 판정, 한 번은 말하기 | 20분 |
| Task 6 | 망 왕복 시간 어림(H3) | **2번** | 없음 | 5분 |
| Task 7 | 30분 동시 가동(H1) | **2번** | 시작·끝에 화면 세 번씩 터치 | 35분 |
| Task 8 | 2번 복구 + 결과 보고 → **게이트(사용자 결정)** | **2번** | 보고서 읽고 진행 여부 결정 | 15분 |
| Task 9~11 | 교환기 작성·시험(맥북) | 없음 | 없음 | 20분 |

- **1번 기기(사진 롤링·공룡 점프)는 이 계획에서 전혀 건드리지 않는다** — 플래시도, 리셋도 없다.
- Task 9~11(교환기)은 기기가 필요 없어 Task 2~8 과 순서를 바꿔도 된다.
- 이 계획에 실린 코드는 작성 시점(2026-10-03)에 임시 폴더에서 실제로 돌려 확인했다: 교환기 시험 76개 통과(10회 반복 흔들림 없음), 측정 도구 시험 26개 통과, 시험 펌웨어 빌드 성공(2.3MB, 기본·대체 설정 3종). **기기에 올려 본 적은 없다** — 그것이 Phase 0 이 할 일이다.

## Global Constraints

설계서의 전 과제 공통 요구사항. 모든 과제는 이 절을 암묵적으로 포함한다.

- **기기 번호(사용자 지정 2026-10-03)**: 1번 = 사진 롤링·공룡 점프(포토 액자), MAC `AA:BB:CC:DD:EE:01`. 2번 = 스냅부스, MAC `AA:BB:CC:DD:EE:02`. Phase 0 은 **2번 1대로만** 한다.
- **기기는 번호(MAC)로 고른다**: 두 대가 함께 꽂혀 있으므로 `/dev/cu.usbmodem*` "첫 번째"를 쓰는 도구(`snap_ctl.py`, `serial_ctl.py`)는 이 계획에서 쓰지 않는다. 포트는 항상 `python3 tools/probe_ctl.py port 2` 로 얻는다.
- **칩·빌드**: ESP-IDF 5.5.4, 대상 `esp32p4`, 두 기기 모두 P4 rev v1.3 → `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`, `CONFIG_ESP32P4_REV_MIN_100=y`. `esp_hosted` 는 **2.11.x 고정**(2.11.7) — 다른 부품 때문에 이 고정을 풀지 않는다. Wi-Fi 칩(C6) 펌웨어 교체 금지, `amcchord/M5Tab5-VideoLink` 플래시 금지.
- **라이브러리 판(설계서 §4 정정 포함)**: `esp_codec_dev` 1.6.2(안정판 1.x 최신), `esp-sr` 2.5.5, **`esp_audio_codec` 2.5.0** — 설계서의 2.6.2 는 P4 rev 3.0 이상에서만 빌드되도록 막혀 있어(부품 CMakeLists 의 검사) rev 1.3 인 우리 기기에 못 쓴다. `esp_websocket_client` 는 Phase 2 부터.
- **음성 형식**: 16kHz 모노, Opus 20ms, 약 24kbps (PH-D5).
- **화면**: 가로 1280×720, 회전 3(삼각대 나사가 화면 아래).
- **교환기**: `127.0.0.1` 에만 연다(기본값), 포트 후보 5094, 설정은 `server/.env.phone`(`PHONE_TOKENS`, `PORT`). 인증은 헤더 `X-Device-Id`·`X-Device-Token`(주소창 매개변수 금지), `hmac.compare_digest`, 실패 시 401, 등록된 2대 외 거절, 같은 ID 재접속 시 옛 연결 끊기.
- **신호 규약 수치**: 벨 제한 30초, 기기 ping 15초, 무소식 45초면 끊음, 음성 이진 프레임 `[순번 2바이트][Opus]` 400바이트 초과 폐기, 통화 중일 때만 건넴.
- **기록·비밀**: 교환기 기록은 시각·기기·통화 길이·종료 사유만. 음성은 어디에도 쓰지 않는다. `secrets.h`·`.env.phone` 내용은 화면·로그·git 에 출력하지 않는다.
- **산출물은 .gitignore 먼저**: devsync 가 5분마다 `add -A` 로 자동 커밋한다 → 새 산출물은 무시 규칙을 **커밋·푸시하고 맥미니에 반영된 뒤** 만든다(Task 1). 임시 파일은 저장소 밖(스크래치패드)에 둔다.
- **브랜치**: 이 저장소는 devsync 가 main 에 직접 커밋하는 흐름이라 별도 브랜치·worktree 를 쓰지 않는다. 단계의 `git commit` 이 "커밋할 것 없음"으로 끝나면 devsync 가 먼저 커밋한 것이므로 정상이다.
- **외부 영향 없음**: 맥미니 배포·터널 호스트 추가·Bot Dashboard 등록은 Phase 4 다. 이 계획은 맥미니에서 `git pull` 확인(읽기) 외에 아무것도 실행하지 않는다. 과금 없음(유료 API 미사용).
- **진행 안내·보고는 전부 한국어.**

## Review Focus

설계서가 암시하지만 놓치기 쉬운 입력·고장 다섯 가지. 각 줄의 시험은 해당 과제에 들어 있다.

1. **같은 기기가 재부팅해 다시 붙는데 옛 연결이 반쯤 죽어 있는 경우** — 새 연결은 즉시 쓸 수 있어야 하고, 옛 연결의 뒤늦은 이탈 처리가 새 연결을 쫓아내면 안 된다. → Task 9 `test_late_detach_of_replaced_connection_is_ignored`, Task 10 `test_same_id_reconnect_replaces_old_connection`
2. **기기가 깨진 JSON·모르는 신호·빈 이진 프레임·통화 전 음성을 보내는 경우** — 교환기가 죽거나 상태가 바뀌거나 상대에게 새면 안 된다. → Task 9 `test_garbage_text_is_ignored`, Task 10 `test_garbage_frames_do_not_break_the_connection`·`test_voice_outside_call_is_not_forwarded`
3. **토큰이 주소창에 실려 오거나 기록에 남는 경우** — 주소창 토큰은 거절하고, 틀린 토큰·맞는 토큰·음성 내용 어느 것도 로그에 없어야 한다. → Task 9 `test_parse_tokens_error_does_not_leak_token`, Task 10 `test_token_in_query_string_is_not_accepted`·`test_logs_contain_no_token_and_no_voice`
4. **상대 쪽 망이 느려 음성이 밀리는 경우(핫스팟)** — 교환기가 쌓아 두면 지연이 계속 늘어난다. 한도를 넘으면 새 프레임을 버려야 한다. → Task 9 `test_should_drop_voice_when_peer_backlogged`, Task 10 `test_voice_is_dropped_when_peer_is_backlogged`
5. **측정이 조용히 틀리는 경우** — 두 대가 꽂혀 있을 때 엉뚱한 기기를 고름, 시리얼 전송이 중간에 잘림, 기준 신호 칸을 잘못 골라 무음과 비교함. 도구는 수치를 내지 말고 이유를 말해야 한다. → Task 2 `test_find_port_picks_by_mac_not_by_order`·`test_parse_pcm_dump_detects_*`·`test_load_pcm_rejects_truncated_file`·`test_erle_rejects_silent_reference`

---

## 비목표 (이번에 건드리지 않는 것)

- 1번 기기(포토 액자) — 플래시·리셋·백업 모두 하지 않는다(1번 백업은 Phase 2 계획의 첫 과제).
- 본 펌웨어 `firmware/tab5_phone/`, 화면 6종, 통화 상태 기계, 지터 버퍼(Phase 2~3).
- 맥미니 배포, launchd, 터널 호스트 `phone.example.com`, Bot Dashboard 등록(Phase 4).
- 스냅부스·포토 액자 펌웨어와 서버의 수정. 스냅부스 펌웨어는 Task 8 에서 **그대로 다시 올리기만** 한다.
- 실제 통화 지연 측정(Phase 4), 이어폰 전환·음소거·절전(Phase 5).

## 영향 범위

- **신규 파일**: `firmware/tab5_phone_probe/**`, `server/phone_core.py`, `server/phone_relay.py`, `server/tests/**`, `server/tools/phone_smoke.py`, `server/pytest.ini`, `server/requirements-phone.txt`.
- **수정 파일**: `.gitignore`(무시 규칙 5줄 추가), `docs/exec-plans/2026-10-03-tab5-phone.md`(결과 기록·상태 줄).
- **기기**: 2번은 Task 4~8 동안 스냅부스로 쓸 수 없다(Task 8 끝에 복구). 2번에 저장된 Wi-Fi 목록(NVS)은 지우지 않는다 — 시험 펌웨어는 같은 파티션 표를 쓰고 읽기만 한다.
- **외부 시스템**: 없음. H3 측정은 기존 주소 `https://snapbooth.example.com/healthz` 에 GET 21회를 보낼 뿐이다.
- **맥북**: Task 11 에서 `127.0.0.1:5094` 를 잠깐 연다(시험 후 종료).

## 롤백

- **2번 기기**(약 5분): `cd ~/tab5-toy-phone/firmware/tab5_snapbooth && source ~/esp/esp-idf-5.5.4/export.sh && idf.py -p "$(python3 ../tab5_phone_probe/tools/probe_ctl.py port 2)" flash`. 그래도 안 되면 `backups/tab5-2_factory_aabbccddee02_2026-09-26.bin` 을 `esptool.py -p <포트> write_flash 0x0 <파일>` 로 복원(공장 상태 — 이 경우 저장된 Wi-Fi 목록은 사라진다).
- **코드**: 전부 신규 파일이라 `firmware/tab5_phone_probe/`, `server/phone_*.py`, `server/tests/`, `server/tools/phone_smoke.py`, `server/pytest.ini`, `server/requirements-phone.txt` 삭제로 끝난다. `.gitignore` 추가 5줄은 남겨도 무해.
- **맥북**: `server/.env.phone` 삭제.

## 이 계획에서 내린 결정 (PL-D)

설계서(PH-D1~D9)에 없던 세부를 정한 것. 설계서와 다른 점은 Task 8·11 에서 설계서에도 옮겨 적는다.

| ID | 결정 | 이유 | 비용 | 탈출구 |
|---|---|---|---|---|
| PL-D1 | 통화가 끝나면 `ended` 를 **양쪽 모두**(끊은 쪽 포함)에게 보낸다 | 기기가 "끝났다"를 한 군데(교환기 응답)에서만 판단하면 상태가 어긋날 일이 없다 | 끊은 쪽은 자기가 아는 사실을 한 번 더 받는다 | `_end()` 에서 보낸 쪽을 빼면 된다(시험 3줄 수정) |
| PL-D2 | 벨 울리는 중 한쪽이 사라지면 남은 쪽에 `ended: offline`, 통화 중이면 `ended: lost` | 설계서는 통화 중만 정했다. 벨 중 이탈은 "상대 기기 꺼짐" 안내가 맞다 | 사유가 하나 더 갈린다 | `detach()` 의 한 줄 |
| PL-D3 | 교환기도 밀린 음성을 버린다(상대 송신 대기열 4096바이트 초과 시) | 설계서는 기기 쪽 폐기만 정했지만, 교환기가 쌓으면 지연이 누적되고 메모리가 는다 | 망이 나쁠 때 소리가 끊긴다(쌓여서 늦는 것보다 낫다). **한계(검수 2026-10-03)**: 이 감지는 교환기 프로그램 안의 대기열만 본다. 운영에서는 교환기의 상대가 같은 맥미니의 터널 프로그램이라 망 정체가 여기까지 거의 올라오지 않는다 → 교환기 메모리 보호일 뿐, 지연 누적의 실질 방어선은 기기 쪽 수신 대기열 300ms 폐기(설계서 §9.6, Phase 3)다. Phase 4 에서 실측 | `MAX_PEER_BUFFER_BYTES` 값 조정 |
| PL-D4 | 수신 주소는 환경변수 `BIND`(기본 `127.0.0.1`) | Phase 2~3 에서 기기 2대가 맥북 교환기에 LAN 으로 붙어야 한다 | 잘못 설정하면 LAN 에 열린다(토큰 인증은 그대로) | 기본값이 설계서대로 내부 전용 |
| PL-D5 | `/healthz` 는 `{"ok": true}` 만 돌려준다 | 터널로 공개되면 누구나 볼 수 있다 — 접속·통화 여부는 사생활 | 대시보드에서 통화 상태를 못 본다 | 토큰을 요구하는 `/stats` 추가 |
| PL-D6 | 에코 측정은 **한 번의 녹음**에서 원음과 결과를 함께 저장해 비교한다(설계서의 "켬/끔 두 벌"을 한 번에) | 두 번 따로 녹음하면 방 소음·위치가 달라 비교가 흐려진다. 에코 제거 전 원음이 곧 "끔"이다 | 메모리 720KB(PSRAM) | 명령 `aec on/off` 로 따로 녹음도 가능 |
| PL-D7 | 시험 음성은 맥의 `say` 로 만들어 펌웨어에 통째로 넣는다 | 시리얼로 올리는 규약을 만들 필요가 없다. 실제 말소리라 삐 소리보다 현실적 | 펌웨어 240KB 증가, 문장을 바꾸면 다시 빌드 | SD 카드에서 읽기 |
| PL-D8 | **잠정 합격선**을 이 계획이 제안한다(설계서에 수치 없음): H2 잔향 감소 25dB 이상 통과 / 15~25 경계 / 미만 미달. H3 왕복 중앙값 150ms 이하 통과 / 150~300 경계. H4 처리 부하 50% 이하·내부 메모리 최저 100KB 이상 | 숫자 없이 "통화 가능한 수준"은 판정할 수 없다. 설계서 지연 예산(망 60~150ms)과 일반적인 스피커폰 기준에서 잡았다 | 선이 틀렸을 수 있다 | **게이트에서 사용자가 귀로 최종 판정** — 수치는 참고 |
| PL-D9 | Phase 0 이 끝나면 2번을 스냅부스로 되돌린다 | 시험 펌웨어는 쓸모가 없고, 다음 단계 시작 전 기기 상태가 분명해야 한다 | 5분 | Task 8 Step 1 을 건너뛴다 |

## 파일 구조

```
firmware/tab5_phone_probe/            ← Phase 0 전용, 버리는 코드
  CMakeLists.txt                      프로젝트 + esp_hosted 패치 훅
  sdkconfig.defaults                  스냅부스 설정에서 카메라·USB 호스트 제거
  partitions.csv                      스냅부스 것 복사(같은 NVS 위치 → 저장된 Wi-Fi 유지)
  README.md                           목적·명령·복구
  main/
    CMakeLists.txt  idf_component.yml 부품 판 고정
    probe_main.cpp                    부팅, 소리 태스크(에코 제거·Opus·루프백·녹음), 화면·터치, 시리얼 명령
    board_audio.h / .cpp              코덱 제어(M5.In_I2C)·I2S 동시 송수신·앰프·음량
    net_probe.h / .cpp                Wi-Fi 연결, 왕복 시간 측정
    wifi_store.h                      스냅부스 것 복사(저장 목록 형식)
    secrets.h                         스냅부스 것 복사 — git 제외
    test_voice.pcm                    시험 음성 — 생성물, git 제외
  tools/
    patch_esp_hosted.py               스냅부스 것 복사
    make_test_voice.sh                시험 음성 생성
    probe_ctl.py                      시리얼 도구(기기 번호로 포트 선택, 덤프, 장시간 가동)
    aec_metrics.py                    잔향 감소량 계산
    test_aec_metrics.py               위 둘의 단위 시험
  out/                                녹음·로그 — git 제외
server/
  phone_core.py                       교환기 순수 로직
  phone_relay.py                      aiohttp 중계
  pytest.ini  requirements-phone.txt
  tests/test_phone_core.py  tests/test_phone_relay.py
  tools/phone_smoke.py                떠 있는 교환기에 가짜 기기 2대로 실제 통화 확인
  .env.phone                          토큰 — git 제외
```

---

# Phase 0 — 가정 검증

### Task 1: 준비 — 무시 규칙, 기기 확인, 복구 준비물

**Files:**
- Modify: `.gitignore` (끝에 추가)

**Interfaces:**
- Consumes: 없음
- Produces: 이후 과제가 만드는 산출물(`firmware/tab5_phone_probe/out/`, `main/test_voice.pcm`, `server/.env.phone`, `.pytest_cache/`)이 자동 커밋되지 않는다는 보장.

- [x] **Step 1: .gitignore 끝에 추가**

```gitignore

# Tab5 전화기 (2026-10-03): 시험 산출물·비밀·캐시 — 파일을 만들기 전에 먼저 등록
.pytest_cache/
server/.env.phone
server/phone_relay.log
firmware/tab5_phone_probe/out/
firmware/tab5_phone_probe/main/test_voice.pcm
```

- [x] **Step 2: 무시 규칙이 먹는지 확인**

Run:
```bash
cd ~/tab5-toy-phone && git check-ignore -v server/.env.phone server/phone_relay.log firmware/tab5_phone_probe/out/x.pcm firmware/tab5_phone_probe/main/test_voice.pcm firmware/tab5_phone_probe/main/secrets.h firmware/tab5_phone_probe/build/x firmware/tab5_phone_probe/sdkconfig server/.pytest_cache/x | wc -l
```
Expected: `8` (여덟 경로 모두 무시됨. `secrets.h`·`build/`·`sdkconfig` 는 기존 규칙이 잡는다)

- [x] **Step 3: 커밋·푸시하고 맥미니에 반영됐는지 확인**

```bash
cd ~/tab5-toy-phone && git add .gitignore && git commit -m "chore(phone): 전화기 시험 산출물 무시 규칙 추가" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" ; git push
ssh macmini 'cd ~/tab5-toy-phone && git pull -q && grep -c "tab5_phone_probe" .gitignore'
```
Expected: 마지막 줄 `2`. (ssh 가 Tailscale 재인증을 요구하면 출력된 링크를 사용자에게 알리고 기다린다. 맥미니에 반영되기 전에는 Step 4 이후로 가지 않는다.)

- [x] **Step 4: 2번 기기가 USB 에 보이는지 확인(포트를 열지 않는다 → 리셋 없음)**

Run:
```bash
python3 -c "
from serial.tools import list_ports
for p in list_ports.comports():
    if p.serial_number: print(p.device, p.serial_number)"
```
Expected: `AA:BB:CC:DD:EE:02` 가 있는 줄이 보인다(1번 `AA:BB:CC:DD:EE:01` 도 꽂혀 있으면 함께 보인다).
보이지 않으면: **여기서 멈추고** 사용자에게 "2번 기기 전원이 켜져 있는지, 데이터 케이블인지" 확인을 요청한다. (계획 작성 시점 2026-10-03 19:20 에는 1번만 보였다.)

- [x] **Step 5: 복구 준비물 확인**

Run:
```bash
cd ~/tab5-toy-phone && shasum -a 256 -c backups/tab5-2_factory_aabbccddee02_2026-09-26.bin.sha256 && ls -la firmware/tab5_snapbooth/build/tab5_snapbooth.bin firmware/tab5_snapbooth/main/secrets.h | awk '{print $5, $9}'
```
Expected: `backups/tab5-2_factory_aabbccddee02_2026-09-26.bin: OK` + 두 파일의 크기와 경로(내용은 출력하지 않는다). 하나라도 없으면 멈추고 보고.

---

### Task 2: 맥 쪽 측정 도구 (순수 로직, 시험 먼저)

**Files:**
- Create: `firmware/tab5_phone_probe/tools/aec_metrics.py`
- Create: `firmware/tab5_phone_probe/tools/probe_ctl.py`
- Test: `firmware/tab5_phone_probe/tools/test_aec_metrics.py`

**Interfaces:**
- Consumes: Task 3 펌웨어가 내는 시리얼 줄 — `PCM BEGIN <이름> <바이트수>` / `B64:<base64, 한 줄에 원본 3072바이트>` / `PCM END <모든 바이트의 합 mod 2^32>` / `PCM FAIL <사유>`, `STAT k=v …`(값은 정수·소수·`평균/최대`), `PROBE BOOT audio=<0|1>`.
- Produces:
  - `probe_ctl.find_port(unit: str, ports=None) -> str` — 기기 번호 `"1"|"2"` → 포트 경로, 없으면 `SystemExit`
  - `probe_ctl.parse_pcm_dump(lines) -> (name: str, data: bytes)` — 어긋나면 `ValueError`
  - `probe_ctl.parse_stat(line: str) -> dict | None`
  - `probe_ctl.parse_until(step: str) -> (cmd, pattern, secs)`
  - `probe_ctl.soak_verdict(first: dict, last: dict, reboots: int) -> (ok: bool, why: list[str])`
  - CLI `probe_ctl.py port <1|2>` / `--unit 2 run <단계>…` / `--unit 2 soak <분> <로그파일>`
  - `aec_metrics.erle(mic, aec, ref) -> dict(erle_db, active_frames, total_frames, mic_dbfs, aec_dbfs, ref_dbfs)` — 못 재면 `ValueError`
  - `aec_metrics.load_pcm(path)`, `delay_ms(ref, mic)`, `clip_ratio(x)`, `verdict(db) -> "통과"|"경계"|"미달"`, CLI `aec_metrics.py <폴더>` (종료 코드 0 정상 / 2 측정 불가 / 3 마이크 포화)

- [x] **Step 1: 실패하는 시험을 먼저 쓴다** — `firmware/tab5_phone_probe/tools/test_aec_metrics.py`

```python
"""aec_metrics·probe_ctl 의 순수 로직 시험. 실행: cd firmware/tab5_phone_probe/tools && python3 -m pytest -q -p no:cacheprovider"""
import base64

import numpy as np
import pytest

import aec_metrics as am
import probe_ctl as pc

SR = am.SR


def speechlike(seconds=6.0, seed=1):
    """말소리처럼 켜졌다 꺼지는 잡음(0.4초 소리 / 0.2초 쉼)."""
    rng = np.random.default_rng(seed)
    n = int(seconds * SR)
    x = rng.normal(0, 6000, n)
    gate = (np.arange(n) % int(0.6 * SR)) < int(0.4 * SR)
    return x * gate


def echo_of(ref, delay=160, gain=0.5, seed=2):
    """스피커 소리가 delay 표본 늦게, gain 배로 마이크에 들어온 모양 + 작은 방 잡음."""
    rng = np.random.default_rng(seed)
    mic = np.zeros_like(ref)
    mic[delay:] = ref[:-delay] * gain
    return mic + rng.normal(0, 5, len(ref))


# ── ERLE ────────────────────────────────────────────────────────────────────
def test_erle_matches_known_attenuation():
    ref = speechlike(); mic = echo_of(ref)
    res = am.erle(mic, mic * 10 ** (-30 / 20), ref)
    assert abs(res["erle_db"] - 30.0) < 0.5
    assert res["active_frames"] >= 50


def test_erle_zero_when_nothing_removed():
    ref = speechlike(); mic = echo_of(ref)
    assert abs(am.erle(mic, mic.copy(), ref)["erle_db"]) < 0.01


def test_erle_capped_when_output_is_silent():
    ref = speechlike(); mic = echo_of(ref)
    assert am.erle(mic, np.zeros_like(mic), ref)["erle_db"] == 99.0      # 0 으로 나누지 않는다


def test_erle_rejects_silent_reference():
    """검토 초점 5: 기준 칸을 잘못 골랐거나 음량 0 이면 수치를 내지 않고 이유를 말한다."""
    ref = np.zeros(SR * 6); mic = speechlike()
    with pytest.raises(ValueError, match="기준 신호"):
        am.erle(mic, mic, ref)


def test_erle_rejects_length_mismatch():
    ref = speechlike()
    with pytest.raises(ValueError, match="길이"):
        am.erle(ref, ref[:-320], ref)


def test_erle_rejects_too_short_recording():
    x = speechlike(0.5)
    with pytest.raises(ValueError):
        am.erle(x, x, x)


def test_verdict_thresholds():
    assert [am.verdict(v) for v in (30, 25, 24.9, 15, 14.9)] == ["통과", "통과", "경계", "경계", "미달"]


def test_delay_ms_finds_echo_lag():
    ref = speechlike(3.0)
    assert abs(am.delay_ms(ref, echo_of(ref, delay=160)) - 10.0) < 0.2    # 160표본 = 10ms


def test_clip_ratio():
    x = np.array([0, 100, 32767, -32768, 5], dtype=float)
    assert am.clip_ratio(x) == pytest.approx(0.4)


# ── 파일 읽기 ───────────────────────────────────────────────────────────────
def test_load_pcm_roundtrip(tmp_path):
    f = tmp_path / "a.pcm"
    np.array([1, -2, 32767, -32768], dtype="<i2").tofile(f)
    assert am.load_pcm(f).tolist() == [1, -2, 32767, -32768]


@pytest.mark.parametrize("raw", [b"", b"\x01\x02\x03"])
def test_load_pcm_rejects_truncated_file(tmp_path, raw):
    """검토 초점 5: 시리얼 전송이 잘린 파일(빈 파일·홀수 바이트)은 조용히 넘어가지 않는다."""
    f = tmp_path / "a.pcm"
    f.write_bytes(raw)
    with pytest.raises(ValueError):
        am.load_pcm(f)


def test_main_reports_and_writes_wavs(tmp_path, capsys):
    ref = speechlike(); mic = echo_of(ref)
    for name, x in (("mic", mic), ("ref", ref), ("aec", mic * 10 ** (-28 / 20))):
        np.clip(x, -32768, 32767).astype("<i2").tofile(tmp_path / f"{name}.pcm")
    assert am.main(["aec_metrics.py", str(tmp_path)]) == 0
    out = capsys.readouterr().out
    assert "통과" in out and (tmp_path / "aec.wav").exists() and (tmp_path / "mic.wav").exists()


def test_main_returns_2_when_files_missing(tmp_path, capsys):
    assert am.main(["aec_metrics.py", str(tmp_path)]) == 2
    assert "측정 불가" in capsys.readouterr().out


# ── probe_ctl: 시리얼로 온 PCM 묶음 풀기 ────────────────────────────────────
def pcm_lines(data, name="mic", chunk=3072, size=None, checksum=None):
    yield f"PCM BEGIN {name} {len(data) if size is None else size}"
    for i in range(0, len(data), chunk):
        yield "B64:" + base64.b64encode(data[i:i + chunk]).decode()
    yield f"PCM END {sum(data) & 0xFFFFFFFF if checksum is None else checksum}"


def test_parse_pcm_dump_ok():
    data = bytes(range(256)) * 40
    lines = ["I (123) probe: 딴 로그", *pcm_lines(data)]
    assert pc.parse_pcm_dump(iter(lines)) == ("mic", data)


def test_parse_pcm_dump_detects_size_mismatch():
    data = bytes(range(256)) * 4
    with pytest.raises(ValueError, match="크기"):
        pc.parse_pcm_dump(pcm_lines(data, size=len(data) + 2))


def test_parse_pcm_dump_detects_checksum_mismatch():
    data = bytes(range(256)) * 4
    with pytest.raises(ValueError, match="검사합"):
        pc.parse_pcm_dump(pcm_lines(data, checksum=1))


def test_parse_pcm_dump_detects_missing_end():
    data = bytes(range(256)) * 4
    lines = list(pcm_lines(data))[:-1]
    with pytest.raises(ValueError, match="끝"):
        pc.parse_pcm_dump(iter(lines))


def test_parse_pcm_dump_reports_device_failure():
    with pytest.raises(ValueError, match="기기"):
        pc.parse_pcm_dump(iter(["PCM FAIL not-ready"]))


# ── probe_ctl: STAT 줄 읽기·기기 번호 → 포트 ─────────────────────────────────
STAT = ("STAT up=120 audio=1 loop=1 aec=1 codec=1 vol=40 chunks=3700 i2c_err=0 rd_err=0 wr_err=0 touch=3 wifi=1 "
        "heap_int=412000 heap_int_min=398000 heap_psram=30100000 aec_us=5200/7100 enc_us=3100/4000 dec_us=600/900 load_pct=34.8")


def test_parse_stat():
    s = pc.parse_stat(STAT)
    assert s["up"] == 120 and s["i2c_err"] == 0 and s["load_pct"] == 34.8
    assert s["aec_us"] == (5200, 7100)


def test_parse_stat_rejects_other_lines():
    assert pc.parse_stat("I (1) probe: hello") is None


def test_parse_until():
    assert pc.parse_until("until:stat:^STAT .*audio=1:40") == ("stat", "^STAT .*audio=1", 40.0)
    assert pc.parse_until("until:wifi:^WIFI ok=1 ssid=a:b:90") == ("wifi", "^WIFI ok=1 ssid=a:b", 90.0)


class FakePort:
    def __init__(self, device, serial_number):
        self.device, self.serial_number = device, serial_number


def test_find_port_picks_by_mac_not_by_order():
    """두 대가 함께 꽂혀 있어도 번호(=MAC)로 고른다 — 엉뚱한 기기에 올리는 사고 방지."""
    ports = [FakePort("/dev/cu.usbmodem2101", "AA:BB:CC:DD:EE:01"), FakePort("/dev/cu.usbmodem1101", "AA:BB:CC:DD:EE:02")]
    assert pc.find_port("2", ports) == "/dev/cu.usbmodem1101"
    assert pc.find_port("1", ports) == "/dev/cu.usbmodem2101"


def test_find_port_fails_when_unit_absent():
    ports = [FakePort("/dev/cu.usbmodem2101", "AA:BB:CC:DD:EE:01")]
    with pytest.raises(SystemExit):
        pc.find_port("2", ports)


def test_find_port_rejects_unknown_unit():
    with pytest.raises(SystemExit):
        pc.find_port("3", [])


def test_soak_verdict():
    first = pc.parse_stat(STAT)
    last = dict(first, up=first["up"] + 1800, chunks=first["chunks"] + 56000)
    assert pc.soak_verdict(first, last, reboots=0) == (True, [])
    bad = dict(last, i2c_err=2, wr_err=1)
    ok, why = pc.soak_verdict(first, bad, reboots=1)
    assert not ok and len(why) == 3
    stalled = dict(last, chunks=first["chunks"])
    assert pc.soak_verdict(first, stalled, reboots=0) == (False, ["소리 묶음 수가 늘지 않음(소리 태스크 멈춤)"])
```

- [x] **Step 2: 실패를 확인**

Run: `cd ~/tab5-toy-phone/firmware/tab5_phone_probe/tools && python3 -m pytest -q`
Expected: 수집 단계에서 FAIL — `ModuleNotFoundError: No module named 'aec_metrics'`

- [x] **Step 3: `aec_metrics.py` 작성**

```python
#!/usr/bin/env python3
"""aec_metrics.py — 에코 제거 효과 측정 (H2). 기기에서 꺼낸 세 갈래 녹음을 비교한다.

  python3 aec_metrics.py out/rec1        # 폴더 안의 mic.pcm ref.pcm aec.pcm (16kHz 모노 16비트, 머리말 없음)
    mic = 마이크 원음(에코 제거 끔과 같음), ref = 스피커로 나간 소리(기준), aec = 에코 제거 결과(켬)
  결과: 잔향 감소량(ERLE, dB) + 귀로 들어 볼 mic.wav / aec.wav 저장.

판정(잠정 — 설계서에 수치가 없어 이 계획에서 제안한 기준. 최종 판정은 사용자 귀):
  25dB 이상 = 통과 / 15~25dB = 경계(PH-D7 후퇴 ① 음량 낮춤 후 재측정) / 15dB 미만 = 미달(후퇴 ② 반이중 검토)
"""
import sys
import wave
from pathlib import Path

import numpy as np

SR = 16000
FRAME = 320                 # 20ms
PASS_DB, MARGINAL_DB = 25.0, 15.0


def load_pcm(path):
    raw = Path(path).read_bytes()
    if len(raw) == 0 or len(raw) % 2:
        raise ValueError(f"{path}: 길이가 0 이거나 홀수({len(raw)}바이트) — 전송이 잘렸을 수 있음")
    return np.frombuffer(raw, dtype="<i2").astype(np.float64)


def dbfs(x):
    rms = np.sqrt(np.mean(np.square(x))) if len(x) else 0.0
    return 20 * np.log10(rms / 32768.0) if rms > 0 else -120.0


def clip_ratio(x):
    return float(np.mean(np.abs(x) >= 32700)) if len(x) else 0.0


def delay_ms(ref, mic, max_ms=100):
    """기준 신호가 마이크에 몇 ms 늦게 나타나는지(교차상관 최대점). 에코 제거 필터가 감당할 범위인지 보는 참고값."""
    n = min(len(ref), len(mic), SR * 3)
    r, m = ref[:n] - np.mean(ref[:n]), mic[:n] - np.mean(mic[:n])
    max_lag = SR * max_ms // 1000
    best, best_lag = -1.0, 0
    for lag in range(0, max_lag + 1):
        c = abs(float(np.dot(r[: n - lag], m[lag:])))
        if c > best:
            best, best_lag = c, lag
    return 1000.0 * best_lag / SR


def erle(mic, aec, ref, skip_s=1.0, gate_db=-45.0):
    """상대 말소리가 나오는 구간(기준 신호가 있는 20ms 묶음)에서 마이크 원음 대비 에코 제거 결과가 몇 dB 줄었나."""
    if not (len(mic) == len(aec) == len(ref)):
        raise ValueError(f"세 녹음의 길이가 다름: mic {len(mic)}, aec {len(aec)}, ref {len(ref)}")
    start = int(skip_s * SR)             # 에코 제거가 방을 익히는 첫 1초는 뺀다
    nf = (len(mic) - start) // FRAME
    if nf <= 0:
        raise ValueError("녹음이 너무 짧음")
    cut = lambda x: x[start:start + nf * FRAME].reshape(nf, FRAME)
    m, a, r = cut(mic), cut(aec), cut(ref)
    r_db = 10 * np.log10(np.mean(np.square(r), axis=1) / 32768.0 ** 2 + 1e-12)
    active = r_db > gate_db
    if int(active.sum()) < 50:           # 1초 미만
        raise ValueError(f"기준 신호가 거의 없음(유효 묶음 {int(active.sum())}개) — 기준 칸 번호나 음량을 확인")
    pm, pa = float(np.sum(np.square(m[active]))), float(np.sum(np.square(a[active])))
    if pm <= 0:
        raise ValueError("마이크 원음이 무음 — 마이크 칸 번호를 확인")
    value = 99.0 if pa <= 0 else min(99.0, 10 * np.log10(pm / pa))
    return {"erle_db": float(value), "active_frames": int(active.sum()), "total_frames": int(nf),
            "mic_dbfs": float(dbfs(m[active].ravel())), "aec_dbfs": float(dbfs(a[active].ravel())), "ref_dbfs": float(dbfs(r[active].ravel()))}


def verdict(erle_db):
    return "통과" if erle_db >= PASS_DB else "경계" if erle_db >= MARGINAL_DB else "미달"


def save_wav(path, x):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(np.clip(x, -32768, 32767).astype("<i2").tobytes())


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__)
    d = Path(argv[1])
    try:
        mic, ref, aec = (load_pcm(d / f"{n}.pcm") for n in ("mic", "ref", "aec"))
        res = erle(mic, aec, ref)
    except (ValueError, OSError) as e:
        print(f"측정 불가: {e}")
        return 2
    clip = clip_ratio(mic)
    print(f"녹음 길이 {len(mic) / SR:.1f}초, 상대 말소리 구간 {res['active_frames'] * 0.02:.1f}초")
    print(f"크기(dBFS)  기준 {res['ref_dbfs']:.1f}  마이크 원음 {res['mic_dbfs']:.1f}  에코 제거 후 {res['aec_dbfs']:.1f}")
    print(f"스피커→마이크 지연 약 {delay_ms(ref, mic):.0f}ms, 마이크 포화 비율 {clip * 100:.2f}%")
    print(f"잔향 감소량(ERLE) {res['erle_db']:.1f} dB → {verdict(res['erle_db'])} (통과 {PASS_DB:.0f}dB 이상, 경계 {MARGINAL_DB:.0f}~{PASS_DB:.0f}dB)")
    if clip > 0.01:
        print("주의: 마이크가 1% 넘게 포화됨 — 음량을 낮춰 다시 재야 수치를 믿을 수 있음")
    save_wav(d / "mic.wav", mic); save_wav(d / "aec.wav", aec)
    print(f"들어 보기: afplay {d / 'mic.wav'}  /  afplay {d / 'aec.wav'}")
    return 0 if clip <= 0.01 else 3


if __name__ == "__main__":
    sys.exit(main(sys.argv))
```

- [x] **Step 4: 다시 실행 — 이번엔 두 번째 모듈이 없어 실패**

Run: `cd ~/tab5-toy-phone/firmware/tab5_phone_probe/tools && python3 -m pytest -q`
Expected: FAIL — `ModuleNotFoundError: No module named 'probe_ctl'`

- [x] **Step 5: `probe_ctl.py` 작성**

```python
#!/usr/bin/env python3
"""probe_ctl.py — tab5_phone_probe 시리얼 검증 도구. 기기는 번호(=MAC)로 고른다: 두 대가 함께 꽂혀 있어도 안전.

  python3 probe_ctl.py port 2                         2번 기기의 포트 경로만 출력 (idf.py -p 에 넘길 때)
  python3 probe_ctl.py --unit 2 run <단계> ...
      until:<명령>:<정규식>:<초>  명령을 2초마다 다시 보내며 정규식 응답을 기다림(기기가 부팅 중이든 이미 켜져 있든 통한다)
      wait:<정규식>:<초>      로그에 정규식이 나올 때까지 대기(없으면 실패)
      sleep:<초>              로그를 보며 대기
      dump:<mic|ref|aec>:<파일>   녹음 한 갈래를 파일로 저장(크기·검사합 확인)
      그 외                   그대로 한 줄 명령 전송 (stat, chan, rec, vol 40, rtt 20 …)
  python3 probe_ctl.py --unit 2 soak <분> <로그파일>   H1 장시간 가동: 루프·에코제거·압축을 켜고 1분마다 stat 기록, 끝에 합격/불합격
포트를 열면 기기가 한 번 리셋될 수 있다 → 한 시험은 한 번의 실행(세션) 안에서 끝낸다. DTR 은 기본값(켜짐)으로 둔다.
"""
import argparse
import base64
import re
import sys
import time

UNITS = {"1": "AA:BB:CC:DD:EE:01",   # 1번 = 사진 롤링·공룡 점프(포토 액자)
         "2": "AA:BB:CC:DD:EE:02"}   # 2번 = 스냅부스 → Phase 0 시험 대상


def find_port(unit, ports=None):
    """기기 번호 → 시리얼 포트 경로. USB 일련번호(=MAC)로 찾으므로 꽂은 순서와 무관하다. 포트를 열지 않는다."""
    if unit not in UNITS:
        sys.exit(f"모르는 기기 번호: {unit} (1 또는 2)")
    if ports is None:
        from serial.tools import list_ports
        ports = list_ports.comports()
    for p in ports:
        if (p.serial_number or "").upper() == UNITS[unit]:
            return p.device
    sys.exit(f"{unit}번 기기({UNITS[unit]})가 USB 에 보이지 않음 — 전원이 켜져 있는지, 데이터 케이블인지 확인")


def parse_pcm_dump(lines):
    """'PCM BEGIN 이름 바이트수' … 'B64:…' … 'PCM END 검사합' 을 읽어 (이름, 바이트) 로. 어긋나면 ValueError."""
    name, size, parts = None, None, []
    for ln in lines:
        if ln.startswith("PCM FAIL"):
            raise ValueError(f"기기가 거절함: {ln}")
        if ln.startswith("PCM BEGIN"):
            _, _, name, size = ln.split()
            size, parts = int(size), []
        elif ln.startswith("B64:") and size is not None:
            parts.append(ln[4:])
        elif ln.startswith("PCM END") and size is not None:
            data = base64.b64decode("".join(parts))
            if len(data) != size:
                raise ValueError(f"크기 불일치: 받은 {len(data)} / 보낸다던 {size}")
            if (sum(data) & 0xFFFFFFFF) != int(ln.split()[2]):
                raise ValueError("검사합 불일치 — 전송 중 깨짐")
            return name, data
    raise ValueError("끝 표시(PCM END)를 받지 못함 — 시간 초과 또는 연결 끊김")


def parse_stat(line):
    """'STAT k=v …' → dict. 숫자는 int/float, 'a/b' 는 (a, b). STAT 줄이 아니면 None."""
    if not line.startswith("STAT "):
        return None
    out = {}
    for k, v in re.findall(r"(\w+)=(\S+)", line):
        if "/" in v:
            out[k] = tuple(int(x) for x in v.split("/"))
        elif "." in v:
            out[k] = float(v)
        else:
            out[k] = int(v)
    return out


def parse_until(step):
    """'until:<명령>:<정규식>:<초>' → (명령, 정규식, 초). 정규식 안의 ':' 는 허용(명령에는 없음)."""
    body = step[len("until:"):]
    cmd, rest = body.split(":", 1)
    pat, secs = rest.rsplit(":", 1)
    return cmd, pat, float(secs)


def soak_verdict(first, last, reboots):
    """장시간 가동 합격 여부: 재부팅 0, I2C·읽기·쓰기 오류 증가 0, 소리 묶음 수 증가."""
    why = []
    if reboots:
        why.append(f"도중 재부팅 {reboots}회")
    for key, label in (("i2c_err", "I2C 오류"), ("rd_err", "소리 읽기 오류"), ("wr_err", "소리 쓰기 오류")):
        d = last[key] - first[key]
        if d > 0:
            why.append(f"{label} {d}건")
    if last["chunks"] <= first["chunks"]:
        why.append("소리 묶음 수가 늘지 않음(소리 태스크 멈춤)")
    return (not why), why


class Session:
    def __init__(self, unit):
        import serial
        self.s = serial.Serial(find_port(unit), 115200, timeout=0.2)
        self.buf = b""
        self.boots = 0

    def lines(self, secs):
        t0 = time.time()
        while time.time() - t0 < secs:
            d = self.s.read(65536)
            if d:
                self.buf += d
            while b"\n" in self.buf:
                ln, self.buf = self.buf.split(b"\n", 1)
                ln = ln.decode("utf-8", "replace").rstrip("\r")
                if ln.startswith("PROBE BOOT"):
                    self.boots += 1
                yield ln

    def pump(self, secs, pattern=None, echo=True):
        rx = re.compile(pattern) if pattern else None
        for ln in self.lines(secs):
            if echo and not ln.startswith("B64:"):
                print(ln)
            if rx and rx.search(ln):
                return ln
        return None

    def send(self, cmd):
        self.s.write((cmd + "\n").encode())
        self.s.flush()

    def ask(self, cmd, pattern, secs, echo=True):
        self.send(cmd)
        return self.pump(secs, pattern, echo)

    def until(self, cmd, pattern, secs, echo=True):
        """응답이 올 때까지 명령을 2초마다 다시 보낸다. 포트를 열 때 기기가 리셋됐든 아니든 같은 방식으로 맞춘다."""
        t0 = time.time()
        while time.time() - t0 < secs:
            ln = self.ask(cmd, pattern, 2.0, echo)
            if ln:
                return ln
        return None

    def dump(self, name, path, secs=120):
        self.send(f"dump {name}")
        got, data = parse_pcm_dump(self.lines(secs))
        with open(path, "wb") as f:
            f.write(data)
        print(f"saved {path} ({got}, {len(data)} B)")


def run_steps(ses, steps):
    for step in steps:
        if step.startswith("until:"):
            cmd, pat, secs = parse_until(step)
            if ses.until(cmd, pat, secs) is None:
                print(f"!! until failed: {cmd} → {pat}")
                return False
        elif step.startswith("wait:"):
            pat, secs = step[5:step.rfind(":")], float(step[step.rfind(":") + 1:])
            if ses.pump(secs, pat) is None:
                print(f"!! wait failed: {pat}")
                return False
        elif step.startswith("sleep:"):
            ses.pump(float(step[6:]))
        elif step.startswith("dump:"):
            _, name, path = step.split(":", 2)
            try:
                ses.dump(name, path)
            except ValueError as e:
                print(f"!! dump failed: {e}")
                return False
        else:
            print(f">> {step}")
            ses.send(step)
    ses.pump(0.5)
    return True


def soak(ses, minutes, logpath):
    if ses.until("stat", r"^STAT .*audio=1", 60, echo=False) is None:
        print("SOAK FAIL: 기기가 준비되지 않음(STAT audio=1 응답 없음)")
        return False
    for cmd in ("vol 40", "aec on", "codec on", "loop on"):
        ses.ask(cmd, r"^OK ", 5)
    first = parse_stat(ses.ask("stat", r"^STAT ", 5) or "")
    if first is None:
        print("SOAK FAIL: stat 응답 없음")
        return False
    boots0, last, t0, n = ses.boots, first, time.time(), 0
    with open(logpath, "a", encoding="utf-8") as log:
        while time.time() - t0 < minutes * 60:
            n += 1
            ses.ask("vol 45" if n % 2 else "vol 40", r"^OK ", 10, echo=False)     # 10초마다 코덱에 I2C 쓰기(버스 경합 유발)
            ses.pump(10, echo=False)
            if n % 6 == 0:                                                        # 1분마다 기록
                line = ses.ask("stat", r"^STAT ", 10, echo=False)
                st = parse_stat(line or "")
                if st is None:
                    print("SOAK FAIL: stat 응답 없음(기기 멈춤?)")
                    return False
                last = st
                log.write(f"{time.strftime('%H:%M:%S')} {line}\n")
                log.flush()
                print(f"[{int((time.time() - t0) / 60):3d}분] i2c_err={st['i2c_err']} rd_err={st['rd_err']} wr_err={st['wr_err']} load={st['load_pct']}% heap_min={st['heap_int_min']}")
    ok, why = soak_verdict(first, last, ses.boots - boots0)
    print("SOAK PASS" if ok else "SOAK FAIL: " + "; ".join(why))
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--unit", default="2", help="기기 번호 1|2 (기본 2)")
    ap.add_argument("cmd", choices=["port", "run", "soak"])
    ap.add_argument("args", nargs="*")
    a = ap.parse_args()
    if a.cmd == "port":
        print(find_port(a.args[0] if a.args else a.unit))
        return 0
    ses = Session(a.unit)
    if a.cmd == "run":
        return 0 if run_steps(ses, a.args) else 1
    if len(a.args) != 2:
        sys.exit("사용법: probe_ctl.py --unit 2 soak <분> <로그파일>")
    return 0 if soak(ses, float(a.args[0]), a.args[1]) else 1


if __name__ == "__main__":
    sys.exit(main())
```

- [x] **Step 6: 통과 확인**

Run: `cd ~/tab5-toy-phone/firmware/tab5_phone_probe/tools && python3 -m pytest -q`
Expected: `26 passed`

- [x] **Step 7: 포트 조회가 번호대로 되는지 실기 확인(포트를 열지 않음)**

Run: `cd ~/tab5-toy-phone/firmware/tab5_phone_probe/tools && python3 probe_ctl.py port 2`
Expected: `/dev/cu.usbmodem…` 한 줄. 2번이 안 보이면 "2번 기기(AA:BB:CC:DD:EE:02)가 USB 에 보이지 않음 …" 과 종료 코드 1.

- [x] **Step 8: 커밋**

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone_probe/tools/aec_metrics.py firmware/tab5_phone_probe/tools/probe_ctl.py firmware/tab5_phone_probe/tools/test_aec_metrics.py && git commit -m "feat(phone-probe): 맥 쪽 측정 도구(잔향 감소량, 시리얼 덤프, 기기 번호로 포트 선택)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: 시험 펌웨어 작성·빌드 (의존성 잠금)

**Files:**
- Create: `firmware/tab5_phone_probe/CMakeLists.txt`, `sdkconfig.defaults`, `README.md`
- Create: `firmware/tab5_phone_probe/main/CMakeLists.txt`, `main/idf_component.yml`, `main/board_audio.h`, `main/board_audio.cpp`, `main/net_probe.h`, `main/net_probe.cpp`, `main/probe_main.cpp`
- Create: `firmware/tab5_phone_probe/tools/make_test_voice.sh`
- Copy(수정 없이): `partitions.csv`, `tools/patch_esp_hosted.py`, `main/wifi_store.h`, `main/secrets.h` ← `firmware/tab5_snapbooth/` 의 같은 파일

**Interfaces:**
- Consumes: 스냅부스의 `wifi_store.h`(`wifistore::unpack/merge/dedupe/rank`), NVS 네임스페이스 `snapwifi` 키 `list`(읽기 전용), `secrets.h` 의 `WIFI_NETWORKS`.
- Produces: 시리얼 규약(115200, 줄 단위). Task 2 도구와 Task 4~7 이 이것에 의존한다.

| 보내는 명령 | 돌아오는 줄 | 뜻 |
|---|---|---|
| (부팅) | `PROBE BOOT audio=<0\|1>` → `PROBE AUDIO chunk=<n> opus_in=640 opus_out_max=<n> voice=<표본수> aec_mode=<이름>` → `PROBE READY audio=<0\|1>` | 소리 장치 초기화 결과. 실패 단계는 로그 `board_audio: init fail: <단계>` |
| `stat` | `STAT up= audio= loop= aec= codec= vol= chunks= i2c_err= rd_err= wr_err= touch= wifi= heap_int= heap_int_min= heap_psram= aec_us=<평균>/<최대> enc_us= dec_us= load_pct=` | 상태 한 줄. 처리 시간 통계는 **직전 stat 이후 구간**(읽으면 초기화) |
| `chan` | `CHAN amp=0 rms=<칸0> <칸1> <칸2> <칸3>` → `CHAN amp=1 rms=…` → `CHAN DONE` | 1kHz 음을 내며 앰프 끔/켬에서 칸별 크기 |
| `loop on\|off`, `aec on\|off`, `codec on\|off` | `OK <명령>` | 루프백(마이크→0.5초 뒤 스피커) / 루프백에 에코 제거 결과 사용 / 루프백에 Opus 압축·해제 삽입 |
| `vol <0-100>` | `OK vol <n>` | 스피커 음량 |
| `gain <입력 0-3> <dB>` | `OK gain <n> <dB>` | 마이크 칩 입력별 증폭(기본 네 입력 모두 30dB) |
| `rec` | `REC DONE samples=<n>` 또는 `REC FAIL <사유>` | 시험 음성을 틀며 마이크 원음·기준·에코 제거 결과를 동시 녹음(약 8.5초) |
| `dump mic\|ref\|aec` | `PCM BEGIN …` `B64:…` `PCM END <합>` | 녹음 한 갈래 꺼내기 |
| `wifi` | `WIFI ok=<0\|1> ssid= ip= rssi=` | Wi-Fi 상태 |
| `rtt [n]` | `RTT FIRST ms= status=` → `RTT i=<k> ms= status=` ×n → `RTT DONE n= min= med= p90= max=` 또는 `RTT FAIL <사유>` | 왕복 시간(첫 번은 TLS 연결 포함이라 통계에서 제외) |
| (터치) | `TOUCH <x> <y> n=<누적>` | 화면을 누를 때마다 |

- [x] **Step 1: 폴더를 만들고 스냅부스에서 네 파일을 복사한다(내용을 출력하지 않는다)**

```bash
cd ~/tab5-toy-phone/firmware && mkdir -p tab5_phone_probe/main tab5_phone_probe/tools tab5_phone_probe/out
cp tab5_snapbooth/partitions.csv tab5_phone_probe/
cp tab5_snapbooth/tools/patch_esp_hosted.py tab5_phone_probe/tools/
cp tab5_snapbooth/main/wifi_store.h tab5_phone_probe/main/
cp tab5_snapbooth/main/secrets.h tab5_phone_probe/main/
git -C ~/tab5-toy-phone status --short firmware/tab5_phone_probe | grep -c secrets.h
```
Expected: 마지막 줄 `0` (`secrets.h` 는 git 에 보이지 않는다)

- [x] **Step 2: `firmware/tab5_phone_probe/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tab5_phone_probe)

# esp_hosted 생성자 자동초기화 무력화 패치 (스냅부스와 같은 스크립트, 멱등). 이유는 tools/patch_esp_hosted.py 참조.
execute_process(COMMAND python3 ${CMAKE_CURRENT_LIST_DIR}/tools/patch_esp_hosted.py RESULT_VARIABLE _eh_patch_rc)
if(NOT _eh_patch_rc EQUAL 0)
  message(FATAL_ERROR "patch_esp_hosted.py failed (rc=${_eh_patch_rc})")
endif()
```

- [x] **Step 3: `firmware/tab5_phone_probe/sdkconfig.defaults`**

```
# M5Stack Tab5 (ESP32-P4NRW32: 16MB Flash / 32MB PSRAM) — tab5_snapbooth 설정에서 카메라·USB 호스트를 뺀 것
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

# Wi-Fi: ESP32-C6 via esp_hosted 2.11.x (SDIO) + esp_wifi_remote — 스냅부스와 동일
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
```

- [x] **Step 4: `firmware/tab5_phone_probe/main/CMakeLists.txt`**

```cmake
# REQUIRES 를 적지 않는다: main 은 그러면 모든 컴포넌트를 자동으로 본다(관리형 컴포넌트 이름 오타 방지).
idf_component_register(SRCS "probe_main.cpp" "board_audio.cpp" "net_probe.cpp"
                       INCLUDE_DIRS "."
                       EMBED_FILES "test_voice.pcm")
```

- [x] **Step 5: `firmware/tab5_phone_probe/main/idf_component.yml`** — `dependencies.lock` 은 git 제외라 여기 적힌 정확한 판이 곧 잠금이다

```yaml
dependencies:
  idf: '>=5.4'
  # ── 스냅부스(2호기에서 검증된 조합)와 같은 판으로 고정 ──
  m5stack/m5unified: '0.2.23'
  m5stack/m5gfx: '0.2.30'
  espressif/esp_wifi_remote: '1.6.5'
  espressif/esp_hosted: '2.11.7'        # Tab5 C6 슬레이브와 검증된 세대. 3.x 는 SDIO 초기화 실패
  # ── 전화기에서 새로 쓰는 것 ──
  espressif/esp_codec_dev: '1.6.2'      # 2.0.0 은 시험판
  espressif/esp-sr: '2.5.5'             # 에코 제거. P4 rev<3 전용 라이브러리 포함
  espressif/esp_audio_codec: '2.5.0'    # Opus. 2.6 이상은 P4 rev 3.0 이상만 빌드됨 → 우리 기기(rev 1.3)는 2.5.x
```

- [x] **Step 6: `firmware/tab5_phone_probe/main/board_audio.h`**

```cpp
// board_audio.h — Tab5 소리 장치(스피커 ES8388 + 마이크 ES7210) 16kHz 동시 송수신. Phase 0 시험용.
//   코덱 제어는 M5.In_I2C 위에 얹는다(PH-D4: 화면·터치와 I2C 드라이버를 하나로). 호출자가 준 잠금으로 버스를 지킨다.
#pragma once
#include <cstddef>
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace board_audio {

constexpr int SAMPLE_RATE = 16000;
constexpr int IN_SLOTS = 4;                    // 마이크 칩이 한 번에 내는 칸 수(TDM)

bool init(SemaphoreHandle_t i2c_lock);         // 실패하면 false, 로그에 어느 단계인지 남긴다
bool read_slots(int16_t *dst, int frames);     // dst 크기 = frames × IN_SLOTS (칸0,칸1,칸2,칸3,칸0,…)
bool write_mono(const int16_t *src, int frames);
void set_volume(int percent);                  // 0~100
void set_in_gain(int mic_index, float db);     // 마이크 칩 입력 0~3 의 증폭(dB). 드라이버 기본은 네 입력 모두 30dB
void amp(bool on);                             // 스피커 앰프 전원(확장칩 0x43 의 P1)
bool health_check();                           // 두 칩의 레지스터를 한 번씩 읽어 본다(버스 건강 확인)
uint32_t i2c_errors();
uint32_t read_errors();
uint32_t write_errors();

}  // namespace board_audio
```

- [x] **Step 7: `firmware/tab5_phone_probe/main/board_audio.cpp`**

```cpp
// board_audio.cpp — 근거: xiaozhi-esp32 main/boards/m5stack/tab5/tab5_audio_codec.cc (MIT) 의 I2S·코덱 설정,
//   M5Unified 0.2.23 M5Unified.inl 의 Tab5 핀(MCLK30/BCLK27/WS29/DOUT26/DIN28)·주소(ES8388 0x10, ES7210 0x40, 앰프 = 0x43 P1).
#include "board_audio.h"

#include <M5Unified.h>
#include <atomic>
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

// 0 = 선례(xiaozhi)대로 송신은 표준 I2S, 수신만 4칸 TDM.
// 1 = 송신도 4칸 TDM(공장 BSP 방식). `chan` 시험에서 칸 배치가 깨져 보일 때만 1 로 바꿔 다시 시험한다.
#ifndef PROBE_TX_TDM
#define PROBE_TX_TDM 0
#endif

static const char *TAG = "board_audio";

namespace {

constexpr uint8_t ES8388_ADDR = 0x10, ES7210_ADDR = 0x40, PI4IO1_ADDR = 0x43;
constexpr uint32_t I2C_FREQ = 400000;
constexpr gpio_num_t PIN_MCLK = GPIO_NUM_30, PIN_BCLK = GPIO_NUM_27, PIN_WS = GPIO_NUM_29, PIN_DOUT = GPIO_NUM_26, PIN_DIN = GPIO_NUM_28;
constexpr int OUT_SLOTS = PROBE_TX_TDM ? 4 : 1;
constexpr int MAX_FRAMES = 1024;

SemaphoreHandle_t s_lock = nullptr;
std::atomic<uint32_t> s_i2c_err{0}, s_rd_err{0}, s_wr_err{0};
i2s_chan_handle_t s_tx = nullptr, s_rx = nullptr;
esp_codec_dev_handle_t s_out = nullptr, s_in = nullptr;

// ── 코덱 제어 인터페이스: M5.In_I2C 로 레지스터 읽기·쓰기 ──────────────────────────────
struct m5_ctrl_t { audio_codec_ctrl_if_t base; uint8_t addr; bool opened; };

int ctrl_open(const audio_codec_ctrl_if_t *c, void *, int) { ((m5_ctrl_t *)c)->opened = true; return ESP_CODEC_DEV_OK; }
bool ctrl_is_open(const audio_codec_ctrl_if_t *c) { return ((const m5_ctrl_t *)c)->opened; }
int ctrl_close(const audio_codec_ctrl_if_t *c) { ((m5_ctrl_t *)c)->opened = false; return ESP_CODEC_DEV_OK; }
int ctrl_read(const audio_codec_ctrl_if_t *c, int reg, int reg_len, void *data, int data_len)
{
    auto *m = (const m5_ctrl_t *)c;
    if (reg_len != 1 || !data || data_len <= 0) return ESP_CODEC_DEV_INVALID_ARG;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool ok = M5.In_I2C.readRegister(m->addr, (uint8_t)reg, (uint8_t *)data, (size_t)data_len, I2C_FREQ);
    xSemaphoreGive(s_lock);
    if (!ok) { ++s_i2c_err; return ESP_CODEC_DEV_READ_FAIL; }
    return ESP_CODEC_DEV_OK;
}
int ctrl_write(const audio_codec_ctrl_if_t *c, int reg, int reg_len, void *data, int data_len)
{
    auto *m = (const m5_ctrl_t *)c;
    if (reg_len != 1 || !data || data_len <= 0) return ESP_CODEC_DEV_INVALID_ARG;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool ok = M5.In_I2C.writeRegister(m->addr, (uint8_t)reg, (const uint8_t *)data, (size_t)data_len, I2C_FREQ);
    xSemaphoreGive(s_lock);
    if (!ok) { ++s_i2c_err; return ESP_CODEC_DEV_WRITE_FAIL; }
    return ESP_CODEC_DEV_OK;
}
int ctrl_info(const audio_codec_ctrl_if_t *c, audio_codec_ctrl_info_t *info)
{
    if (!info) return ESP_CODEC_DEV_INVALID_ARG;
    info->type = AUDIO_CODEC_CTRL_I2C; info->i2c.addr = ((const m5_ctrl_t *)c)->addr; info->i2c.port = 1;
    return ESP_CODEC_DEV_OK;
}
m5_ctrl_t make_ctrl(uint8_t addr)
{
    m5_ctrl_t m = {};
    m.base.open = ctrl_open; m.base.is_open = ctrl_is_open; m.base.read_reg = ctrl_read; m.base.write_reg = ctrl_write;
    m.base.get_info = ctrl_info; m.base.close = ctrl_close;
    m.addr = addr; m.opened = true;             // 코덱 드라이버는 "이미 열린" 인터페이스를 요구한다
    return m;
}
m5_ctrl_t s_ctrl_out = make_ctrl(ES8388_ADDR), s_ctrl_in = make_ctrl(ES7210_ADDR);

bool i2s_setup()
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear_after_cb = true;        // 보낼 소리가 끊기면 잡음 대신 무음
    if (i2s_new_channel(&chan_cfg, &s_tx, &s_rx) != ESP_OK) { ESP_LOGE(TAG, "init fail: i2s_new_channel"); return false; }

    const i2s_tdm_slot_mask_t all4 = (i2s_tdm_slot_mask_t)(I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3);
    i2s_tdm_config_t rx_cfg = {
        .clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(board_audio::SAMPLE_RATE),
        .slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, all4),
        .gpio_cfg = {
            .mclk = PIN_MCLK, .bclk = PIN_BCLK, .ws = PIN_WS, .dout = I2S_GPIO_UNUSED, .din = PIN_DIN,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
#if PROBE_TX_TDM
    i2s_tdm_config_t tx_cfg = rx_cfg;
    tx_cfg.gpio_cfg.dout = PIN_DOUT; tx_cfg.gpio_cfg.din = I2S_GPIO_UNUSED;
    if (i2s_channel_init_tdm_mode(s_tx, &tx_cfg) != ESP_OK) { ESP_LOGE(TAG, "init fail: tx tdm"); return false; }
#else
    i2s_std_config_t tx_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(board_audio::SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = PIN_MCLK, .bclk = PIN_BCLK, .ws = PIN_WS, .dout = PIN_DOUT, .din = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    if (i2s_channel_init_std_mode(s_tx, &tx_cfg) != ESP_OK) { ESP_LOGE(TAG, "init fail: tx std"); return false; }
#endif
    if (i2s_channel_init_tdm_mode(s_rx, &rx_cfg) != ESP_OK) { ESP_LOGE(TAG, "init fail: rx tdm"); return false; }
    if (i2s_channel_enable(s_tx) != ESP_OK || i2s_channel_enable(s_rx) != ESP_OK) { ESP_LOGE(TAG, "init fail: i2s enable"); return false; }
    return true;
}

}  // namespace

namespace board_audio {

bool init(SemaphoreHandle_t i2c_lock)
{
    s_lock = i2c_lock;
    if (!s_lock) { ESP_LOGE(TAG, "init fail: no i2c lock"); return false; }
    if (!health_check()) { ESP_LOGE(TAG, "init fail: codec not answering on I2C (ES8388 0x10 / ES7210 0x40)"); return false; }
    if (!i2s_setup()) return false;

    audio_codec_i2s_cfg_t i2s_cfg = {};
    i2s_cfg.port = I2S_NUM_0; i2s_cfg.rx_handle = s_rx; i2s_cfg.tx_handle = s_tx;
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    if (!data_if || !gpio_if) { ESP_LOGE(TAG, "init fail: data/gpio if"); return false; }

    es8388_codec_cfg_t out_cfg = {};
    out_cfg.ctrl_if = &s_ctrl_out.base; out_cfg.gpio_if = gpio_if;
    out_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
    out_cfg.master_mode = false;                // 클럭은 P4 가 낸다(칩은 따라가는 쪽)
    out_cfg.pa_pin = -1;                        // 앰프는 확장칩으로 직접 켠다
    out_cfg.hw_gain.pa_voltage = 5.0; out_cfg.hw_gain.codec_dac_voltage = 3.3;
    const audio_codec_if_t *out_codec = es8388_codec_new(&out_cfg);
    if (!out_codec) { ESP_LOGE(TAG, "init fail: es8388_codec_new"); return false; }

    es7210_codec_cfg_t in_cfg = {};
    in_cfg.ctrl_if = &s_ctrl_in.base;
    in_cfg.mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3 | ES7210_SEL_MIC4;
    const audio_codec_if_t *in_codec = es7210_codec_new(&in_cfg);
    if (!in_codec) { ESP_LOGE(TAG, "init fail: es7210_codec_new"); return false; }

    esp_codec_dev_cfg_t dev_cfg = {};
    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_OUT; dev_cfg.codec_if = out_codec; dev_cfg.data_if = data_if;
    s_out = esp_codec_dev_new(&dev_cfg);
    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN; dev_cfg.codec_if = in_codec;
    s_in = esp_codec_dev_new(&dev_cfg);
    if (!s_out || !s_in) { ESP_LOGE(TAG, "init fail: esp_codec_dev_new"); return false; }

    esp_codec_dev_sample_info_t fs_out = {};
    fs_out.bits_per_sample = 16; fs_out.channel = OUT_SLOTS; fs_out.channel_mask = PROBE_TX_TDM ? 0x0F : 0; fs_out.sample_rate = SAMPLE_RATE;
    if (esp_codec_dev_open(s_out, &fs_out) != ESP_CODEC_DEV_OK) { ESP_LOGE(TAG, "init fail: open out"); return false; }
    esp_codec_dev_sample_info_t fs_in = {};
    fs_in.bits_per_sample = 16; fs_in.channel = IN_SLOTS; fs_in.channel_mask = 0x0F; fs_in.sample_rate = SAMPLE_RATE;
    if (esp_codec_dev_open(s_in, &fs_in) != ESP_CODEC_DEV_OK) { ESP_LOGE(TAG, "init fail: open in"); return false; }
    set_volume(50);
    amp(true);
    ESP_LOGI(TAG, "audio ready: 16kHz, out %d slot(s), in %d slots, tx_tdm=%d", OUT_SLOTS, IN_SLOTS, PROBE_TX_TDM);
    return true;
}

bool read_slots(int16_t *dst, int frames)
{
    if (!s_in || esp_codec_dev_read(s_in, dst, frames * IN_SLOTS * (int)sizeof(int16_t)) != ESP_CODEC_DEV_OK) { ++s_rd_err; return false; }
    return true;
}

bool write_mono(const int16_t *src, int frames)
{
    if (!s_out || frames > MAX_FRAMES) { ++s_wr_err; return false; }
#if PROBE_TX_TDM
    static int16_t wide[MAX_FRAMES * 4];
    for (int i = 0; i < frames; ++i) { wide[i * 4] = wide[i * 4 + 1] = wide[i * 4 + 2] = wide[i * 4 + 3] = src[i]; }
    int rc = esp_codec_dev_write(s_out, wide, frames * 4 * (int)sizeof(int16_t));
#else
    int rc = esp_codec_dev_write(s_out, (void *)src, frames * (int)sizeof(int16_t));
#endif
    if (rc != ESP_CODEC_DEV_OK) { ++s_wr_err; return false; }
    return true;
}

void set_volume(int percent)
{
    if (s_out) esp_codec_dev_set_out_vol(s_out, percent < 0 ? 0 : percent > 100 ? 100 : percent);
}

void set_in_gain(int mic_index, float db)
{
    if (s_in && mic_index >= 0 && mic_index < IN_SLOTS) esp_codec_dev_set_in_channel_gain(s_in, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(mic_index), db);
}

void amp(bool on)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool ok = on ? M5.In_I2C.bitOn(PI4IO1_ADDR, 0x05, 0b00000010, I2C_FREQ) : M5.In_I2C.bitOff(PI4IO1_ADDR, 0x05, 0b00000010, I2C_FREQ);
    xSemaphoreGive(s_lock);
    if (!ok) ++s_i2c_err;
}

bool health_check()
{
    uint8_t a = 0, b = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool ok1 = M5.In_I2C.readRegister(ES8388_ADDR, 0x19, &a, 1, I2C_FREQ);
    bool ok2 = M5.In_I2C.readRegister(ES7210_ADDR, 0x40, &b, 1, I2C_FREQ);
    xSemaphoreGive(s_lock);
    if (!ok1) ++s_i2c_err;
    if (!ok2) ++s_i2c_err;
    return ok1 && ok2;
}

uint32_t i2c_errors() { return s_i2c_err; }
uint32_t read_errors() { return s_rd_err; }
uint32_t write_errors() { return s_wr_err; }

}  // namespace board_audio
```

- [x] **Step 8: `firmware/tab5_phone_probe/main/net_probe.h`**

```cpp
// net_probe.h — Wi-Fi 연결(스냅부스와 같은 esp_hosted 초기화) + 교환기까지 왕복 시간 어림(H3). Phase 0 시험용.
#pragma once

namespace net_probe {

void start();                      // Wi-Fi 태스크 시작. nvs_flash_init() 이 끝난 뒤에 부른다
bool connected();
void print_status();               // "WIFI ok=1 ssid=... ip=... rssi=..." 한 줄
void rtt(int n);                   // HTTPS GET 을 n+1 번(첫 번은 TLS 연결 포함) → "RTT ..." 줄들과 "RTT DONE ..." 요약

}  // namespace net_probe
```

- [x] **Step 9: `firmware/tab5_phone_probe/main/net_probe.cpp`**

```cpp
// net_probe.cpp — 근거: tab5_snapbooth main.cpp 의 wifi_task(esp_hosted 2.11 SDIO 핀·초기화 순서), wifi_store.h(저장 목록 형식).
//   자동 재연결 매니저는 넣지 않았다(시험용): 끊기면 5초 뒤 다시 스캔해 가장 센 알려진 Wi-Fi 에 붙는다.
#include "net_probe.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_hosted.h"
extern "C" {
#include "esp_hosted_transport_config.h"   // C 헤더에 extern "C" 가드 없음
}
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "secrets.h"      // WIFI_NETWORKS (git 제외). 주의: WIFI_PASS 는 여기서 매크로라 변수 이름으로 쓰면 안 된다
#include "wifi_store.h"

// H3 근사: 새 인프라 없이, 이미 터널에 물려 있는 주소까지의 왕복을 잰다(설계서 §5 Phase 0)
#define PROBE_HEALTH_URL "https://snapbooth.example.com/healthz"

static const char *TAG = "net_probe";

namespace {

struct FixedCred { const char *ssid; const char *pass; };
const FixedCred FIXED[] = WIFI_NETWORKS;
const int FIXED_N = sizeof(FIXED) / sizeof(FIXED[0]);

EventGroupHandle_t s_eg = nullptr;
const EventBits_t B_OK = BIT0, B_FAIL = BIT1;
std::atomic<bool> s_ok{false};
char s_ssid[33] = "", s_ip[16] = "", s_try[33] = "";

void on_event(void *, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) { s_ok = false; xEventGroupSetBits(s_eg, B_FAIL); }
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto *e = (ip_event_got_ip_t *)data;
        snprintf(s_ip, sizeof s_ip, IPSTR, IP2STR(&e->ip_info.ip)); strlcpy(s_ssid, s_try, sizeof s_ssid);
        s_ok = true; xEventGroupSetBits(s_eg, B_OK);
        ESP_LOGI(TAG, "wifi got ip %s (%s)", s_ip, s_ssid);
    }
}

std::vector<wifistore::Cred> load_known()
{
    std::vector<wifistore::Cred> saved, fixed;
    nvs_handle_t h;
    if (nvs_open("snapwifi", NVS_READONLY, &h) == ESP_OK) {          // 스냅부스가 화면에서 저장해 둔 목록을 읽기만 한다
        size_t len = 0;
        if (nvs_get_blob(h, "list", nullptr, &len) == ESP_OK && len > 0 && len < 4096) {
            std::string blob(len, '\0');
            if (nvs_get_blob(h, "list", &blob[0], &len) == ESP_OK) saved = wifistore::unpack(blob);
        }
        nvs_close(h);
    }
    for (int i = 0; i < FIXED_N; ++i) { wifistore::Cred c; c.ssid = FIXED[i].ssid; c.pass = FIXED[i].pass; fixed.push_back(c); }
    ESP_LOGI(TAG, "wifi known: saved %u + fixed %d", (unsigned)saved.size(), FIXED_N);
    return wifistore::merge(saved, fixed);
}

std::vector<wifistore::Ap> scan()
{
    std::vector<wifistore::Ap> raw;
    for (int attempt = 0; attempt < 3 && raw.empty(); ++attempt) {      // 부팅 직후 첫 스캔은 0개일 수 있다(C6 준비 전)
        if (attempt) vTaskDelay(pdMS_TO_TICKS(1500));
        wifi_scan_config_t sc = {};
        if (esp_wifi_scan_start(&sc, true) != ESP_OK) continue;
        uint16_t n = 0; esp_wifi_scan_get_ap_num(&n);
        std::vector<wifi_ap_record_t> aps(n);
        if (n && esp_wifi_scan_get_ap_records(&n, aps.data()) == ESP_OK)
            for (uint16_t k = 0; k < n; ++k) { wifistore::Ap a; a.ssid = (const char *)aps[k].ssid; a.rssi = aps[k].rssi; a.secured = aps[k].authmode != WIFI_AUTH_OPEN; raw.push_back(a); }
    }
    return wifistore::dedupe(raw);
}

bool try_connect(const wifistore::Cred &c)
{
    esp_wifi_disconnect(); vTaskDelay(pdMS_TO_TICKS(300));
    xEventGroupClearBits(s_eg, B_OK | B_FAIL);
    wifi_config_t sta = {};
    strlcpy((char *)sta.sta.ssid, c.ssid.c_str(), sizeof sta.sta.ssid);
    strlcpy((char *)sta.sta.password, c.pass.c_str(), sizeof sta.sta.password);
    esp_wifi_set_config(WIFI_IF_STA, &sta);
    strlcpy(s_try, c.ssid.c_str(), sizeof s_try);
    ESP_LOGI(TAG, "wifi connecting: %s", c.ssid.c_str());               // 비밀번호는 로그에 남기지 않는다
    esp_wifi_connect();
    const int64_t t0 = esp_timer_get_time();
    while (esp_timer_get_time() - t0 < 15000000) {
        EventBits_t b = xEventGroupWaitBits(s_eg, B_OK | B_FAIL, pdTRUE, pdFALSE, pdMS_TO_TICKS(500));
        if (b & B_OK) return true;
        if (b & B_FAIL) { vTaskDelay(pdMS_TO_TICKS(500)); esp_wifi_connect(); }
    }
    return false;
}

void wifi_task(void *)
{
    s_eg = xEventGroupCreate();
    if (esp_netif_init() != ESP_OK || esp_event_loop_create_default() != ESP_OK) { printf("PROBE FAIL wifi netif/event\n"); vTaskDelete(nullptr); return; }
    {   // C6 전원은 M5.begin() 이 켠다. 핀은 Tab5 배선(CLK12/CMD13/D0 11/D1 10/D2 9/D3 8/RST15)
        struct esp_hosted_sdio_config conf = INIT_DEFAULT_HOST_SDIO_CONFIG();
        conf.pin_clk.pin = 12; conf.pin_cmd.pin = 13;
        conf.pin_d0.pin = 11; conf.pin_d1.pin = 10; conf.pin_d2.pin = 9; conf.pin_d3.pin = 8;
        conf.pin_reset.pin = 15;
        if (esp_hosted_sdio_set_config(&conf) != ESP_OK) { printf("PROBE FAIL esp_hosted sdio config\n"); vTaskDelete(nullptr); return; }
    }
    if (esp_hosted_init() != 0 || esp_hosted_connect_to_slave() != 0) { printf("PROBE FAIL esp_hosted init\n"); vTaskDelete(nullptr); return; }
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&wcfg) != ESP_OK) { printf("PROBE FAIL esp_wifi_init\n"); vTaskDelete(nullptr); return; }
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_event, nullptr);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_event, nullptr);
    esp_wifi_set_mode(WIFI_MODE_STA);
    if (esp_wifi_start() != ESP_OK) { printf("PROBE FAIL esp_wifi_start\n"); vTaskDelete(nullptr); return; }

    const auto known = load_known();
    while (true) {
        if (!s_ok) {
            const auto aps = scan();
            const auto order = wifistore::rank(known, aps);
            if (order.empty()) ESP_LOGW(TAG, "wifi: no known network visible (%u seen)", (unsigned)aps.size());
            for (int idx : order) if (try_connect(known[idx])) break;
            printf("PROBE WIFI ok=%d\n", (int)s_ok.load());
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

}  // namespace

namespace net_probe {

void start() { xTaskCreatePinnedToCore(wifi_task, "wifi", 10 * 1024, nullptr, 5, nullptr, 0); }

bool connected() { return s_ok; }

void print_status()
{
    int rssi = 0;
    wifi_ap_record_t ap = {};
    if (s_ok && esp_wifi_sta_get_ap_info(&ap) == ESP_OK) rssi = ap.rssi;
    printf("WIFI ok=%d ssid=%s ip=%s rssi=%d\n", (int)s_ok.load(), s_ok ? s_ssid : "-", s_ok ? s_ip : "-", rssi);
}

void rtt(int n)
{
    if (!s_ok) { printf("RTT FAIL wifi-not-connected\n"); return; }
    esp_http_client_config_t cfg = {};
    cfg.url = PROBE_HEALTH_URL; cfg.timeout_ms = 8000; cfg.crt_bundle_attach = esp_crt_bundle_attach; cfg.keep_alive_enable = true;
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) { printf("RTT FAIL client-init\n"); return; }
    std::vector<float> ms;
    for (int i = 0; i <= n; ++i) {
        const int64_t t0 = esp_timer_get_time();
        const esp_err_t e = esp_http_client_perform(c);
        const float d = (esp_timer_get_time() - t0) / 1000.0f;
        if (e != ESP_OK) { printf("RTT ERR i=%d err=%s\n", i, esp_err_to_name(e)); vTaskDelay(pdMS_TO_TICKS(500)); continue; }
        const int st = esp_http_client_get_status_code(c);
        if (i == 0) printf("RTT FIRST ms=%.1f status=%d\n", d, st);      // TLS 연결 시간 포함 — 통계에서 뺀다
        else { printf("RTT i=%d ms=%.1f status=%d\n", i, d, st); ms.push_back(d); }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    esp_http_client_cleanup(c);
    if (ms.empty()) { printf("RTT FAIL no-samples\n"); return; }
    std::sort(ms.begin(), ms.end());
    const size_t k = ms.size();
    printf("RTT DONE n=%u min=%.1f med=%.1f p90=%.1f max=%.1f\n", (unsigned)k, ms.front(), ms[k / 2], ms[std::min(k - 1, (k * 9) / 10)], ms.back());
}

}  // namespace net_probe
```

- [x] **Step 10: `firmware/tab5_phone_probe/main/probe_main.cpp`**

```cpp
/*
 * tab5_phone_probe — Tab5 음성 전화기 Phase 0 가정 검증 전용. 버리는 코드: 본 펌웨어(tab5_phone)에 복사해 쓰지 않는다.
 * 설계: docs/exec-plans/2026-10-03-tab5-phone.md §4(H1~H4)·§5 Phase 0 / 계획: docs/exec-plans/2026-10-03-tab5-phone-phase0-1.md
 *
 * 하는 일
 *   H1  화면·터치(M5Unified)와 소리(esp_codec_dev)를 같은 I2C 버스·I2S 로 함께 돌린다. 루프백 = 마이크 → 0.5초 뒤 스피커.
 *   H2  시험 음성을 스피커로 틀면서 마이크 원음·기준 신호·에코 제거 결과를 한 번에 녹음해 시리얼로 꺼낸다.
 *   H4  에코 제거와 Opus 압축·해제에 걸린 시간, 남은 메모리를 잰다.
 *   H3  교환기 터널까지 왕복 시간을 어림한다(net_probe).
 *
 * 시리얼 명령(줄 단위, 115200): stat | chan | loop on|off | aec on|off | codec on|off | vol <0-100> | gain <입력 0-3> <dB> | rec | dump mic|ref|aec | wifi | rtt [n]
 * 검증 도구: tools/probe_ctl.py
 */
#include <M5Unified.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "mbedtls/base64.h"

#include "esp_aec.h"
#include "esp_audio_enc.h"
#include "esp_audio_dec.h"
#include "esp_opus_enc.h"
#include "esp_opus_dec.h"

#include "board_audio.h"
#include "net_probe.h"

static const char *TAG = "probe";

#define DISPLAY_ROTATION   3            // 삼각대 나사가 화면 아래(정상 가로)
#define MIC_SLOT           0            // `chan` 시험으로 확인한 뒤 다르면 고친다
#define REF_SLOT           1            // 에코 기준 신호(스피커로 나가는 소리를 칩 안에서 되받은 것)
#define AEC_FILTER_LEN     4            // esp-sr 권장값
#define PROBE_AEC_MODE     AEC_MODE_VOIP_HIGH_PERF   // H2 가 경계·미달이면 AEC_MODE_FD_HIGH_PERF(양방향 동시 통화용)로 바꿔 다시 잰다
#define OPUS_BITRATE       24000        // PH-D5
#define OPUS_COMPLEXITY    5            // 0(가벼움)~10. H4 가 미달이면 0 으로 낮춰 다시 잰다
#define OPUS_SAMPLES       320          // 20ms @16kHz
#define LOOP_DELAY_SAMPLES 8000         // 0.5초
#define REC_TAIL_SAMPLES   16000        // 시험 음성 뒤 1초(잔향)
#define C_BG   0x161615u
#define C_INK  0xF2F1EEu
#define C_MUTE 0x9A9893u
#define C_RED  0xF0A7A5u
#define C_GRN  0xA6D6AAu

extern const uint8_t voice_start[] asm("_binary_test_voice_pcm_start");
extern const uint8_t voice_end[]   asm("_binary_test_voice_pcm_end");

struct Timing {                          // 한 구간(직전 stat 이후)의 처리 시간 통계
    std::atomic<uint32_t> n{0}, sum_us{0}, max_us{0};
    void add(uint32_t us) { ++n; sum_us += us; if (us > max_us) max_us = us; }
    uint32_t avg() const { uint32_t k = n; return k ? sum_us / k : 0; }
    void reset() { n = 0; sum_us = 0; max_us = 0; }
};

struct Fifo {                            // 한 태스크 안에서만 쓰는 고정 크기 대기열(가득 차면 오래된 것부터 버림)
    int16_t *buf = nullptr; int cap = 0, head = 0, len = 0;
    bool init(int c) { buf = (int16_t *)heap_caps_malloc((size_t)c * sizeof(int16_t), MALLOC_CAP_SPIRAM); cap = c; return buf != nullptr; }
    void push(const int16_t *s, int n) { for (int i = 0; i < n; ++i) { if (len == cap) { head = (head + 1) % cap; --len; } buf[(head + len) % cap] = s[i]; ++len; } }
    int pop(int16_t *d, int n) { int k = std::min(n, len); for (int i = 0; i < k; ++i) d[i] = buf[(head + i) % cap]; head = (head + k) % cap; len -= k; return k; }
};

static SemaphoreHandle_t g_i2c_lock = nullptr;
static std::atomic<bool> g_audio_ok{false}, g_loop{false}, g_use_aec{false}, g_codec{false}, g_tone{false};
static std::atomic<int>  g_rec{0};       // 0 대기, 1 녹음 중, 2 끝
static std::atomic<int>  g_rec_pos{0}, g_vol{50}, g_chunk{0};
static std::atomic<uint32_t> g_chunks{0}, g_touch{0}, g_slot_seq{0}, g_slot_rms[board_audio::IN_SLOTS];
static Timing g_t_aec, g_t_enc, g_t_dec;
static int16_t *g_rec_mic = nullptr, *g_rec_ref = nullptr, *g_rec_aec = nullptr;
static int g_rec_len = 0, g_voice_samples = 0;

static int16_t *alloc16(int samples) { return (int16_t *)heap_caps_aligned_calloc(16, (size_t)samples, sizeof(int16_t), MALLOC_CAP_INTERNAL); }

// ---------------------------------------------------------------------------
// 소리 태스크: 마이크 4칸 읽기 → 에코 제거 → (Opus 압축·해제) → 0.5초 지연 → 스피커
// ---------------------------------------------------------------------------
static void audio_task(void *)
{
    aec_handle_t *aec = aec_create(board_audio::SAMPLE_RATE, AEC_FILTER_LEN, 1, PROBE_AEC_MODE);
    if (!aec) { printf("PROBE FAIL aec_create\n"); vTaskDelete(nullptr); return; }
    const int chunk = aec_get_chunksize(aec);

    esp_opus_enc_config_t ecfg = ESP_OPUS_ENC_CONFIG_DEFAULT();
    ecfg.sample_rate = ESP_AUDIO_SAMPLE_RATE_16K; ecfg.channel = ESP_AUDIO_MONO; ecfg.bitrate = OPUS_BITRATE;
    ecfg.frame_duration = ESP_OPUS_ENC_FRAME_DURATION_20_MS; ecfg.application_mode = ESP_OPUS_ENC_APPLICATION_VOIP; ecfg.complexity = OPUS_COMPLEXITY;
    void *enc = nullptr, *dec = nullptr; int enc_in = 0, enc_out = 0;
    esp_opus_dec_cfg_t dcfg = ESP_OPUS_DEC_CONFIG_DEFAULT();
    dcfg.sample_rate = ESP_AUDIO_SAMPLE_RATE_16K; dcfg.channel = ESP_AUDIO_MONO; dcfg.frame_duration = ESP_OPUS_DEC_FRAME_DURATION_20_MS;
    if (esp_opus_enc_open(&ecfg, sizeof ecfg, &enc) != ESP_AUDIO_ERR_OK || esp_opus_enc_get_frame_size(enc, &enc_in, &enc_out) != ESP_AUDIO_ERR_OK
        || esp_opus_dec_open(&dcfg, sizeof dcfg, &dec) != ESP_AUDIO_ERR_OK || enc_in != OPUS_SAMPLES * 2) {
        printf("PROBE FAIL opus open (enc_in=%d)\n", enc_in); vTaskDelete(nullptr); return;
    }

    int16_t *slots = alloc16(chunk * board_audio::IN_SLOTS), *mic = alloc16(chunk), *ref = alloc16(chunk), *clean = alloc16(chunk), *spk = alloc16(chunk);
    int16_t *pcm20 = alloc16(OPUS_SAMPLES), *dec_pcm = alloc16(OPUS_SAMPLES);
    uint8_t *pkt = (uint8_t *)heap_caps_malloc((size_t)enc_out, MALLOC_CAP_INTERNAL);
    Fifo enc_fifo, delay;
    if (!slots || !mic || !ref || !clean || !spk || !pcm20 || !dec_pcm || !pkt || !enc_fifo.init(4096) || !delay.init(LOOP_DELAY_SAMPLES * 3)) {
        printf("PROBE FAIL audio buffers\n"); vTaskDelete(nullptr); return;
    }
    int16_t tone[16]; for (int i = 0; i < 16; ++i) tone[i] = (int16_t)(8000.0f * sinf(2.0f * (float)M_PI * i / 16.0f));   // 1kHz
    uint64_t acc[board_audio::IN_SLOTS] = {}; int acc_n = 0, tone_pos = 0;
    g_chunk = chunk;
    printf("PROBE AUDIO chunk=%d opus_in=%d opus_out_max=%d voice=%d aec_mode=%s\n", chunk, enc_in, enc_out, g_voice_samples, aec_get_mode_string(PROBE_AEC_MODE));

    while (true) {
        if (!board_audio::read_slots(slots, chunk)) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }
        for (int i = 0; i < chunk; ++i) {
            mic[i] = slots[i * board_audio::IN_SLOTS + MIC_SLOT]; ref[i] = slots[i * board_audio::IN_SLOTS + REF_SLOT];
            for (int s = 0; s < board_audio::IN_SLOTS; ++s) { int32_t v = slots[i * board_audio::IN_SLOTS + s]; acc[s] += (uint64_t)(v * v); }
        }
        if (++acc_n >= 16000 / chunk) {                                   // 약 1초마다 칸별 크기(RMS) 게시
            for (int s = 0; s < board_audio::IN_SLOTS; ++s) { g_slot_rms[s] = (uint32_t)sqrt((double)acc[s] / (double)(acc_n * chunk)); acc[s] = 0; }
            acc_n = 0; ++g_slot_seq;
        }

        int64_t t0 = esp_timer_get_time();
        aec_process(aec, mic, ref, clean);
        g_t_aec.add((uint32_t)(esp_timer_get_time() - t0));
        const int16_t *src = g_use_aec ? clean : mic;

        memset(spk, 0, (size_t)chunk * sizeof(int16_t));
        if (g_rec == 1) {                                                 // 녹음: 스피커로는 시험 음성, 세 갈래를 같은 시각으로 저장
            const int pos = g_rec_pos, n = std::min(chunk, g_rec_len - pos);
            memcpy(g_rec_mic + pos, mic, (size_t)n * 2); memcpy(g_rec_ref + pos, ref, (size_t)n * 2); memcpy(g_rec_aec + pos, clean, (size_t)n * 2);
            const int v = std::max(0, std::min(chunk, g_voice_samples - pos));
            if (v > 0) memcpy(spk, voice_start + (size_t)pos * 2, (size_t)v * 2);
            g_rec_pos = pos + n;
            if (pos + n >= g_rec_len) g_rec = 2;
        } else if (g_tone) {
            for (int i = 0; i < chunk; ++i) { spk[i] = tone[tone_pos]; tone_pos = (tone_pos + 1) & 15; }
        } else if (g_loop) {
            if (g_codec) {                                                // 20ms 단위로 압축 → 해제 (에코 제거 단위와 달라 대기열을 둔다)
                enc_fifo.push(src, chunk);
                while (enc_fifo.len >= OPUS_SAMPLES) {
                    enc_fifo.pop(pcm20, OPUS_SAMPLES);
                    esp_audio_enc_in_frame_t in = {}; in.buffer = (uint8_t *)pcm20; in.len = (uint32_t)enc_in;
                    esp_audio_enc_out_frame_t out = {}; out.buffer = pkt; out.len = (uint32_t)enc_out;
                    t0 = esp_timer_get_time();
                    const bool enc_ok = esp_opus_enc_process(enc, &in, &out) == ESP_AUDIO_ERR_OK && out.encoded_bytes > 0;
                    g_t_enc.add((uint32_t)(esp_timer_get_time() - t0));
                    if (!enc_ok) continue;
                    esp_audio_dec_in_raw_t raw = {}; raw.buffer = pkt; raw.len = out.encoded_bytes;
                    esp_audio_dec_out_frame_t frame = {}; frame.buffer = (uint8_t *)dec_pcm; frame.len = OPUS_SAMPLES * 2;
                    esp_audio_dec_info_t info = {};
                    t0 = esp_timer_get_time();
                    const bool dec_ok = esp_opus_dec_decode(dec, &raw, &frame, &info) == ESP_AUDIO_ERR_OK;
                    g_t_dec.add((uint32_t)(esp_timer_get_time() - t0));
                    if (dec_ok) delay.push(dec_pcm, (int)frame.decoded_size / 2);
                }
            } else delay.push(src, chunk);
            if (delay.len >= LOOP_DELAY_SAMPLES + chunk) delay.pop(spk, chunk);
        } else { delay.len = 0; delay.head = 0; enc_fifo.len = 0; enc_fifo.head = 0; }

        board_audio::write_mono(spk, chunk);
        ++g_chunks;
    }
}

// ---------------------------------------------------------------------------
// 화면·터치 태스크: 20ms 마다 터치 읽기(I2C), 1초마다 숫자 갱신 + 코덱 버스 건강 확인
// ---------------------------------------------------------------------------
static void draw_line(int row, uint32_t color, const char *text)
{
    const int y = 40 + row * 64;
    M5.Display.fillRect(0, y, M5.Display.width(), 60, (uint32_t)C_BG);
    M5.Display.setTextColor((uint32_t)color, (uint32_t)C_BG);
    M5.Display.drawString(text, 40, y + 8);
}

static void ui_task(void *)
{
    M5.Display.setFont(&fonts::efontKR_24); M5.Display.setTextSize(1.5f);
    int64_t last = 0; char buf[160];
    while (true) {
        xSemaphoreTake(g_i2c_lock, portMAX_DELAY); M5.update(); xSemaphoreGive(g_i2c_lock);
        auto t = M5.Touch.getDetail();
        if (t.wasPressed()) {
            const uint32_t n = ++g_touch;
            printf("TOUCH %d %d n=%u\n", (int)t.x, (int)t.y, (unsigned)n);
            M5.Display.fillCircle(t.x, t.y, 14, (uint32_t)C_GRN);
        }
        const int64_t now = esp_timer_get_time() / 1000;
        if (now - last >= 1000) {
            last = now;
            const bool bus_ok = g_audio_ok ? board_audio::health_check() : false;
            const uint32_t up = (uint32_t)(now / 1000);
            draw_line(0, C_INK, "전화기 가정 검증 (Phase 0)");
            snprintf(buf, sizeof buf, "소리 장치: %s   버스: %s", g_audio_ok ? "정상" : "실패", bus_ok ? "정상" : "오류");
            draw_line(1, (g_audio_ok && bus_ok) ? C_GRN : C_RED, buf);
            snprintf(buf, sizeof buf, "루프 %s  에코제거 %s  압축 %s  음량 %d", g_loop ? "켬" : "끔", g_use_aec ? "켬" : "끔", g_codec ? "켬" : "끔", (int)g_vol);
            draw_line(2, C_INK, buf);
            snprintf(buf, sizeof buf, "묶음 %u   I2C 오류 %u   읽기 %u   쓰기 %u", (unsigned)g_chunks, (unsigned)board_audio::i2c_errors(), (unsigned)board_audio::read_errors(), (unsigned)board_audio::write_errors());
            draw_line(3, board_audio::i2c_errors() ? C_RED : C_INK, buf);
            snprintf(buf, sizeof buf, "터치 %u회   Wi-Fi %s", (unsigned)g_touch, net_probe::connected() ? "연결됨" : "연결 안 됨");
            draw_line(4, C_INK, buf);
            snprintf(buf, sizeof buf, "가동 %02u:%02u:%02u", (unsigned)(up / 3600), (unsigned)(up / 60 % 60), (unsigned)(up % 60));
            draw_line(5, C_MUTE, buf);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ---------------------------------------------------------------------------
// 시리얼 명령
// ---------------------------------------------------------------------------
static void cmd_stat()
{
    const int chunk = g_chunk;
    const uint32_t a = g_t_aec.avg(), e = g_t_enc.avg(), d = g_t_dec.avg();
    const float load = chunk ? 100.0f * ((float)a / (chunk * 62.5f) + (float)(e + d) / 20000.0f) : 0.0f;   // 한 코어 기준, 16kHz 1표본 = 62.5us
    printf("STAT up=%u audio=%d loop=%d aec=%d codec=%d vol=%d chunks=%u i2c_err=%u rd_err=%u wr_err=%u touch=%u wifi=%d "
           "heap_int=%u heap_int_min=%u heap_psram=%u aec_us=%u/%u enc_us=%u/%u dec_us=%u/%u load_pct=%.1f\n",
           (unsigned)(esp_timer_get_time() / 1000000), (int)g_audio_ok, (int)g_loop, (int)g_use_aec, (int)g_codec, (int)g_vol, (unsigned)g_chunks,
           (unsigned)board_audio::i2c_errors(), (unsigned)board_audio::read_errors(), (unsigned)board_audio::write_errors(), (unsigned)g_touch, (int)net_probe::connected(),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           (unsigned)a, (unsigned)g_t_aec.max_us, (unsigned)e, (unsigned)g_t_enc.max_us, (unsigned)d, (unsigned)g_t_dec.max_us, load);
    g_t_aec.reset(); g_t_enc.reset(); g_t_dec.reset();
}

static bool wait_slot_windows(int windows)                                // 칸별 크기가 새로 windows 번 게시될 때까지(최대 5초)
{
    const uint32_t target = g_slot_seq + (uint32_t)windows;
    for (int i = 0; i < 250 && g_slot_seq < target; ++i) vTaskDelay(pdMS_TO_TICKS(20));
    return g_slot_seq >= target;
}

static void cmd_chan()                                                    // 1kHz 음을 내며 앰프 끔/켬에서 칸별 크기 비교
{
    if (!g_audio_ok || g_rec == 1) { printf("CHAN FAIL busy-or-no-audio\n"); return; }
    const bool was_loop = g_loop; g_loop = false; g_tone = true;
    for (int on = 0; on <= 1; ++on) {
        board_audio::amp(on);
        if (!wait_slot_windows(2)) { printf("CHAN FAIL no-audio-frames\n"); break; }
        printf("CHAN amp=%d rms=%u %u %u %u\n", on, (unsigned)g_slot_rms[0], (unsigned)g_slot_rms[1], (unsigned)g_slot_rms[2], (unsigned)g_slot_rms[3]);
    }
    g_tone = false; board_audio::amp(true); g_loop = was_loop;
    printf("CHAN DONE\n");
}

static void cmd_rec()
{
    if (!g_audio_ok || !g_rec_mic) { printf("REC FAIL no-audio\n"); return; }
    g_rec_pos = 0; g_rec = 1;
    for (int i = 0; i < 1000 && g_rec != 2; ++i) vTaskDelay(pdMS_TO_TICKS(20));     // 최대 20초
    if (g_rec != 2) { g_rec = 0; printf("REC FAIL timeout\n"); return; }
    g_rec = 0;
    printf("REC DONE samples=%d\n", g_rec_len);
}

static void dump_pcm(const char *name, const int16_t *buf, int samples)
{
    const uint8_t *p = (const uint8_t *)buf; const uint32_t bytes = (uint32_t)samples * 2;
    uint32_t sum = 0; for (uint32_t i = 0; i < bytes; ++i) sum += p[i];
    printf("PCM BEGIN %s %u\n", name, (unsigned)bytes);
    static unsigned char line[4200];
    for (uint32_t off = 0; off < bytes; off += 3072) {
        size_t n = std::min<uint32_t>(3072, bytes - off), olen = 0;
        mbedtls_base64_encode(line, sizeof line, &olen, p + off, n); line[olen] = 0;
        printf("B64:%s\n", (const char *)line);
    }
    printf("PCM END %u\n", (unsigned)sum); fflush(stdout);
}

static void serial_cmd(const std::string &cmd)
{
    int v = 0, ch = 0; float db = 0;
    if (cmd == "stat") cmd_stat();
    else if (cmd == "chan") cmd_chan();
    else if (cmd == "loop on" || cmd == "loop off") { g_loop = cmd == "loop on"; printf("OK %s\n", cmd.c_str()); }
    else if (cmd == "aec on" || cmd == "aec off") { g_use_aec = cmd == "aec on"; printf("OK %s\n", cmd.c_str()); }
    else if (cmd == "codec on" || cmd == "codec off") { g_codec = cmd == "codec on"; printf("OK %s\n", cmd.c_str()); }
    else if (sscanf(cmd.c_str(), "vol %d", &v) == 1) { v = std::max(0, std::min(100, v)); g_vol = v; board_audio::set_volume(v); printf("OK vol %d\n", v); }
    else if (sscanf(cmd.c_str(), "gain %d %f", &ch, &db) == 2) { board_audio::set_in_gain(ch, db); printf("OK gain %d %.1f\n", ch, db); }
    else if (cmd == "rec") cmd_rec();
    else if (cmd == "dump mic" || cmd == "dump ref" || cmd == "dump aec") {
        if (!g_rec_mic || g_rec == 1) printf("PCM FAIL not-ready\n");
        else dump_pcm(cmd.c_str() + 5, cmd == "dump mic" ? g_rec_mic : cmd == "dump ref" ? g_rec_ref : g_rec_aec, g_rec_len);
    }
    else if (cmd == "wifi") net_probe::print_status();
    else if (cmd == "rtt" || sscanf(cmd.c_str(), "rtt %d", &v) == 1) net_probe::rtt(v > 0 && v <= 100 ? v : 20);
    else printf("? unknown cmd '%s'\n", cmd.c_str());
    fflush(stdout);
}

static void serial_task(void *)
{
    usb_serial_jtag_driver_config_t jcfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    if (usb_serial_jtag_driver_install(&jcfg) == ESP_OK) usb_serial_jtag_vfs_use_driver();
    printf("PROBE READY audio=%d\n", (int)g_audio_ok); fflush(stdout);
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

// ---------------------------------------------------------------------------
// app_main
// ---------------------------------------------------------------------------
static void panel_guard()                // 패널 감지 실패(폭 0)면 재시작. 횟수는 NVS 에(3번까지) — 무한 재부팅 방지
{
    nvs_handle_t h; uint8_t n = 0;
    const bool nvs_ok = nvs_open("probe", NVS_READWRITE, &h) == ESP_OK;
    if (nvs_ok) nvs_get_u8(h, "rst", &n);
    if (M5.Display.width() == 0 && nvs_ok && n < 3) { nvs_set_u8(h, "rst", n + 1); nvs_commit(h); nvs_close(h); printf("PROBE panel-not-detected restart %d\n", n + 1); esp_restart(); }
    if (nvs_ok) { if (n) { nvs_set_u8(h, "rst", 0); nvs_commit(h); } nvs_close(h); }
    if (M5.Display.width() == 0) printf("PROBE FAIL panel-not-detected\n");
}

extern "C" void app_main(void)
{
    auto cfg = M5.config();
    cfg.internal_spk = false;            // 소리는 esp_codec_dev 로 직접 다룬다(PH-D4) — M5Unified 가 I2S0·코덱을 잡지 않게
    cfg.internal_mic = false;
    M5.begin(cfg);
    if (nvs_flash_init() != ESP_OK) ESP_LOGE(TAG, "nvs init failed (지우지 않는다: 저장된 Wi-Fi 목록 보존)");
    panel_guard();
    M5.Display.setRotation(DISPLAY_ROTATION);
    M5.Display.setBrightness(160);
    M5.Display.fillScreen((uint32_t)C_BG);

    g_i2c_lock = xSemaphoreCreateMutex();
    g_voice_samples = (int)((voice_end - voice_start) / 2);
    g_rec_len = g_voice_samples + REC_TAIL_SAMPLES;
    g_rec_mic = (int16_t *)heap_caps_malloc((size_t)g_rec_len * 2, MALLOC_CAP_SPIRAM);
    g_rec_ref = (int16_t *)heap_caps_malloc((size_t)g_rec_len * 2, MALLOC_CAP_SPIRAM);
    g_rec_aec = (int16_t *)heap_caps_malloc((size_t)g_rec_len * 2, MALLOC_CAP_SPIRAM);
    if (!g_rec_mic || !g_rec_ref || !g_rec_aec) { g_rec_mic = nullptr; printf("PROBE FAIL rec buffers\n"); }

    g_audio_ok = board_audio::init(g_i2c_lock);
    printf("PROBE BOOT audio=%d\n", (int)g_audio_ok);

    net_probe::start();
    xTaskCreatePinnedToCore(ui_task, "ui", 8 * 1024, nullptr, 4, nullptr, 1);
    if (g_audio_ok) xTaskCreatePinnedToCore(audio_task, "audio", 48 * 1024, nullptr, 10, nullptr, 1);   // Opus 는 스택을 많이 쓴다(문서 권장 40KB)
    xTaskCreatePinnedToCore(serial_task, "sercmd", 12 * 1024, nullptr, 3, nullptr, 0);                  // rtt 의 TLS 연결이 이 태스크에서 돈다
}
```

- [x] **Step 11: `firmware/tab5_phone_probe/tools/make_test_voice.sh`** 를 쓰고 실행해 시험 음성을 만든다

```bash
#!/bin/bash
# 시험 음성 만들기: macOS 내장 음성합성(say) → 16kHz 모노 16비트 PCM(머리말 없음) → main/test_voice.pcm
# 펌웨어에 통째로 넣어(EMBED_FILES) 스피커로 튼다. 산출물은 git 제외.
set -euo pipefail
cd "$(dirname "$0")/.."
TMP=$(mktemp -d)
say -v Yuna -r 190 -o "$TMP/v.aiff" "안녕하세요. 전화기 에코 제거 시험입니다. 하나, 둘, 셋, 넷, 다섯. 잠시 후에 다시 걸겠습니다."
afconvert -f WAVE -d LEI16@16000 -c 1 "$TMP/v.aiff" "$TMP/v.wav"
python3 - "$TMP/v.wav" main/test_voice.pcm <<'PY'
import sys, wave
w = wave.open(sys.argv[1], "rb")
assert (w.getframerate(), w.getnchannels(), w.getsampwidth()) == (16000, 1, 2), "형식이 16kHz 모노 16비트가 아님"
pcm = w.readframes(w.getnframes())
assert 3 * 32000 <= len(pcm) <= 8 * 32000, f"길이가 3~8초가 아님: {len(pcm) / 32000:.1f}초"
open(sys.argv[2], "wb").write(pcm)
print(f"test_voice.pcm {len(pcm)} bytes ({len(pcm) / 32000:.1f}초)")
PY
rm -rf "$TMP"
```

Run: `cd ~/tab5-toy-phone/firmware/tab5_phone_probe && chmod +x tools/make_test_voice.sh && ./tools/make_test_voice.sh`
Expected: `test_voice.pcm 239942 bytes (7.5초)` 안팎(맥의 음성 판에 따라 조금 다를 수 있다. 3~9초 밖이면 스크립트가 실패한다)

- [x] **Step 12: `firmware/tab5_phone_probe/README.md`**

````markdown
# tab5_phone_probe — 전화기 Phase 0 가정 검증 (버리는 코드)

Tab5 음성 전화기(설계 `docs/exec-plans/2026-10-03-tab5-phone.md`)의 미검증 가정 H1~H4 를 **2번 기기**에서 확인하는 시험 펌웨어.
본 펌웨어(`firmware/tab5_phone/`)에 복사해 쓰지 않는다. 절차·합격선·판정표는 `docs/exec-plans/2026-10-03-tab5-phone-phase0-1.md`.

## 구조

| 파일 | 역할 |
|---|---|
| `main/probe_main.cpp` | 부팅, 소리 태스크(에코 제거 → Opus 압축·해제 → 0.5초 지연 루프백, 시험 음성 녹음), 화면·터치, 시리얼 명령 |
| `main/board_audio.{h,cpp}` | 스피커 칩 ES8388·마이크 칩 ES7210 을 `M5.In_I2C` 로 제어, I2S0 동시 송수신(수신 4칸 TDM), 앰프·음량 |
| `main/net_probe.{h,cpp}` | Wi-Fi 연결(스냅부스와 같은 esp_hosted 초기화, 저장 목록은 읽기만), 왕복 시간 측정 |
| `main/idf_component.yml` | 부품 판 고정. `esp_audio_codec` 은 **2.5.0** (2.6+ 는 P4 rev 3.0 이상 전용) |
| `tools/probe_ctl.py` | 시리얼 도구. 기기를 번호(MAC)로 고른다 |
| `tools/aec_metrics.py` | 잔향 감소량(ERLE) 계산, 들어 볼 WAV 저장 |
| `tools/make_test_voice.sh` | 시험 음성 `main/test_voice.pcm` 생성(macOS `say`) — git 제외 |

`main/secrets.h`(스냅부스 것 복사), `main/test_voice.pcm`, `out/`, `build/`, `managed_components/`, `sdkconfig`, `dependencies.lock` 은 git 제외.

## 빌드·플래시 (2번 기기만)

```
./tools/make_test_voice.sh
source ~/esp/esp-idf-5.5.4/export.sh
idf.py set-target esp32p4 && idf.py build
PORT=$(python3 tools/probe_ctl.py port 2)
esptool.py -p "$PORT" read_mac | grep -i "aa:bb:cc:dd:ee:02" && idf.py -p "$PORT" flash
```

두 대가 함께 꽂혀 있을 수 있으므로 포트 이름을 직접 쓰지 않는다. `snap_ctl.py`·`serial_ctl.py` 는 "첫 번째 포트"를 열어 1번 기기를 리셋시킬 수 있으니 여기서는 쓰지 않는다.

## 시리얼 명령 (115200, 줄 단위)

`stat` · `chan` · `loop on|off` · `aec on|off` · `codec on|off` · `vol <0-100>` · `gain <입력 0-3> <dB>` · `rec` · `dump mic|ref|aec` · `wifi` · `rtt [n]`

```
python3 tools/probe_ctl.py --unit 2 run "until:stat:^STAT .*audio=1:40" "chan" "wait:CHAN DONE:15"
python3 tools/probe_ctl.py --unit 2 soak 30 out/soak.log
```

## 복구

```
cd ../tab5_snapbooth && idf.py -p "$(python3 ../tab5_phone_probe/tools/probe_ctl.py port 2)" flash
```

시험 펌웨어는 스냅부스와 같은 파티션 표를 쓰고 NVS 를 지우지 않으므로, 화면에서 저장해 둔 Wi-Fi 목록은 그대로 남는다.
````

- [x] **Step 13: 빌드** (첫 빌드는 부품 약 200MB 를 내려받아 5~10분 걸린다)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone_probe && source ~/esp/esp-idf-5.5.4/export.sh && idf.py set-target esp32p4 && idf.py build
```
Expected (끝부분):
```
tab5_phone_probe.bin binary size 0x244… bytes. Smallest app partition is 0x600000 bytes. 0x3bb… bytes (62%) free.
Project build complete.
```
그리고 설정 단계 로그에 `TARGET_LIB_PATH is set to: esp32p4_less_v3` 와 `[patch_esp_hosted] applied`(또는 `already applied`)가 있어야 한다.

빌드가 실패하면:
- `esp_audio_codec (v2.6+) requires ESP32-P4 chip version >= 3.0` → `idf_component.yml` 의 `esp_audio_codec` 이 `2.5.0` 인지 확인.
- 부품 판 해석 실패(`ERROR: Because …`) → **`esp_hosted` 고정은 풀지 않는다.** 오류 전문을 그대로 보고하고 멈춘다(설계서 §12 "라이브러리 버전 충돌").
- `test_voice.pcm` 없음 → Step 11 을 다시 실행.

- [x] **Step 14: 실제로 잡힌 부품 판을 확인**

Run:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone_probe && python3 - <<'EOF'
import re
name = None
for ln in open("dependencies.lock"):
    m = re.match(r"^  ([a-z0-9_/\-]+):$", ln)
    if m: name = m.group(1)
    m = re.match(r"^    version: (.+)$", ln)
    if m and name: print(f"{name:36s} {m.group(1)}"); name = None
EOF
```
Expected (계획 작성 시 임시 빌드에서 확인한 값):
```
espressif/cjson                      1.7.19~2
espressif/dl_fft                     0.8.0
espressif/eppp_link                  1.1.6
espressif/esp-dl                     3.3.13
espressif/esp-sr                     2.5.5
espressif/esp_audio_codec            2.5.0
espressif/esp_codec_dev              1.6.2
espressif/esp_hosted                 2.11.7
espressif/esp_new_jpeg               1.0.2
espressif/esp_serial_slave_link      1.1.2
espressif/esp_wifi_remote            1.6.5
espressif/wifi_remote_over_eppp      0.3.3
idf                                  5.5.4
m5stack/m5gfx                        0.2.30
m5stack/m5unified                    0.2.23
```
직접 고정한 일곱 개(esp-sr, esp_audio_codec, esp_codec_dev, esp_hosted, esp_wifi_remote, m5gfx, m5unified)가 다르면 실패다. 딸려 온 것(esp-dl 등)이 달라졌으면 결과 기록(Task 8)에 실제 값을 적는다.

- [x] **Step 15: 산출물이 git 에 안 보이는지 확인하고 커밋**

```bash
cd ~/tab5-toy-phone && git status --short firmware/tab5_phone_probe | grep -cE "build/|managed_components|sdkconfig$|dependencies.lock|secrets.h|test_voice.pcm"
git add firmware/tab5_phone_probe && git commit -m "feat(phone-probe): Phase 0 가정 검증 펌웨어(화면·터치 + 16kHz 동시 송수신 + 에코 제거 + Opus + 왕복 측정)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
Expected: 첫 줄 `0`

---

### Task 4: 2번 기기에 올려 소리 장치와 칸 배치를 확인 (H1 기본)

**Files:**
- Modify(결과에 따라): `firmware/tab5_phone_probe/main/probe_main.cpp` 의 `MIC_SLOT`·`REF_SLOT`, `main/board_audio.cpp` 의 `PROBE_TX_TDM`
- Create: `firmware/tab5_phone_probe/out/task4.log` (git 제외)

**Interfaces:**
- Consumes: Task 2 `probe_ctl.py`, Task 3 펌웨어.
- Produces: **확정된 칸 번호**(마이크·기준 신호가 4칸 중 몇 번인지)와 송신 방식(`PROBE_TX_TDM` 0 또는 1) — Task 5·7 과 Phase 3 설계가 이 값에 의존한다.

- [x] **Step 1: 대상이 2번인지 확인한 뒤 플래시**

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone_probe && source ~/esp/esp-idf-5.5.4/export.sh
PORT=$(python3 tools/probe_ctl.py port 2) && echo "2번 포트: $PORT"
esptool.py -p "$PORT" read_mac | grep -i "aa:bb:cc:dd:ee:02" && idf.py -p "$PORT" flash
```
Expected: `MAC: aa:bb:cc:dd:ee:02` 줄이 나온 뒤 플래시가 진행되고 `Hard resetting via RTS pin...` 으로 끝난다. MAC 줄이 안 나오면 플래시는 실행되지 않는다(`&&`) — 멈추고 보고.

- [x] **Step 2: 부팅과 소리 장치 초기화 확인**

Run:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone_probe && python3 tools/probe_ctl.py --unit 2 run "until:stat:^STAT :40" | tee out/task4.log
```
Expected: `STAT up=… audio=1 … i2c_err=0 rd_err=0 wr_err=0 …` 줄. 화면에는 "전화기 가정 검증 (Phase 0)" 과 "소리 장치: 정상   버스: 정상" 이 보이고 숫자가 1초마다 바뀐다.

`audio=0` 이면 로그의 `board_audio: init fail: <단계>` 를 보고 아래대로 판단한다.

| 단계 | 뜻 | 조치 |
|---|---|---|
| `codec not answering on I2C` | M5.In_I2C 로 두 칩이 응답하지 않음 | **H1 실패** — 멈추고 보고(PH-D4 탈출구 검토) |
| `i2s_new_channel` / `tx std` / `rx tdm` / `i2s enable` | I2S0 을 못 잡음 | M5Unified 가 I2S 를 잡았는지 확인: `probe_main.cpp` 의 `cfg.internal_spk = false; cfg.internal_mic = false;` 가 `M5.begin` 앞에 있는지. 맞는데도 실패면 **H1 실패** 보고 |
| `es8388_codec_new` / `es7210_codec_new` / `open out` / `open in` | 코덱 드라이버 초기화 실패 | 로그 앞뒤의 `ES8388`/`ES7210` 오류 줄을 보고에 붙이고 멈춘다 |

`until failed` (STAT 응답 자체가 없음) 이면 화면을 본다: 검은 화면 → 로그의 `PROBE panel-not-detected` 확인. 재부팅 반복 → 로그 전문을 보고하고 멈춘다.

- [x] **Step 3: 칸 배치 시험** — 스피커에서 1kHz "삐" 소리가 2초쯤 난다

Run:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone_probe && python3 tools/probe_ctl.py --unit 2 run "until:stat:^STAT .*audio=1:40" "vol 50" "chan" "wait:CHAN DONE:15" | tee -a out/task4.log
```
Expected: 두 줄 `CHAN amp=0 rms=a b c d`, `CHAN amp=1 rms=a b c d` 와 `CHAN DONE`.

읽는 법(숫자는 **예시이며 실측이 아니다**):
```
CHAN amp=0 rms=40 5200 35 20      ← 앰프를 꺼도 큰 칸 = 기준 신호(칩 안에서 되받은 스피커 소리)
CHAN amp=1 rms=2600 5200 2400 25  ← 앰프를 켜야 커지는 칸 = 마이크
```
- **기준 칸** = `amp=0` 에서 300 이상이고 `amp=1` 에서도 거의 같은(±20%) 칸.
- **마이크 칸** = `amp=1` 값이 `amp=0` 값의 3배 이상인 칸(둘 이상이면 번호가 가장 작은 것).

판정:

| 관찰 | 조치 |
|---|---|
| 마이크 = 칸0, 기준 = 칸1 | 가정대로. 고칠 것 없음 |
| 마이크·기준이 다른 번호 | `probe_main.cpp` 의 `#define MIC_SLOT` / `#define REF_SLOT` 을 그 번호로 고치고 `idf.py build && idf.py -p "$PORT" flash`, 이 Step 을 다시 실행 |
| 기준 칸이 20000 이상(찌그러짐) | 어느 입력이 그 칸인지 찾는다: `gain 0 0` → `chan` → 값이 줄어든 칸이 입력 0. `gain 1 0`, `gain 2 0`, `gain 3 0` 도 차례로. 기준 칸에 해당하는 입력은 0dB 로 두고(마이크 입력은 `gain <n> 30` 으로 되돌림) 그 번호를 기록 |
| `amp=0` 에서 큰 칸이 없음(기준 신호 없음), 또는 네 칸이 모두 같은 값 | `board_audio.cpp` 의 `#define PROBE_TX_TDM 0` 을 `1` 로 바꿔 빌드·플래시 후 다시. 그래도 같으면 **H1 실패**(하드웨어 기준 신호를 못 받음) — 멈추고 보고 |
| `amp=1` 에서도 모든 칸이 작음(소리가 안 남) | 사용자에게 "삐 소리가 들렸는지" 확인. 안 들렸으면 `vol 80` 으로 다시. 그래도 안 들리면 **H1 실패** 보고 |

- [x] **Step 4: 사용자 확인 두 가지**

사용자에게 먼저 말한다: ① "방금 2번 기기 스피커에서 '삐' 소리가 들렸나요?" ② "지금 명령을 실행합니다. 화면에 글자가 다시 뜨면 30초 안에 화면 아무 곳이나 한 번 눌러 주세요."
그 뒤 Run:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone_probe && python3 tools/probe_ctl.py --unit 2 run "until:stat:^STAT .*audio=1:40" "sleep:30" "stat" "wait:^STAT :5" | tee -a out/task4.log
```
Expected: `TOUCH <x> <y> n=1` 줄이 보이고 누른 자리에 초록 점이 찍힌다. 마지막 `STAT` 의 `touch=` 가 1 이상, `i2c_err=0`.

- [x] **Step 5: 값을 고쳤으면 커밋**

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone_probe/main && git commit -m "fix(phone-probe): 실측한 칸 배치 반영(MIC_SLOT/REF_SLOT/PROBE_TX_TDM)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
(고친 것이 없으면 건너뛴다.) 확정 값 `MIC_SLOT=…, REF_SLOT=…, PROBE_TX_TDM=…, 기준 입력 증폭=…` 을 Task 8 결과표에 적을 수 있게 메모한다.

---

### Task 5: H2·H4 — 에코 제거 효과와 처리 여유

**Files:**
- Create: `firmware/tab5_phone_probe/out/rec_v40/`, `out/rec_v60/`, `out/rec_v80/`, `out/rec_talk/` (각각 `mic.pcm ref.pcm aec.pcm mic.wav aec.wav`), `out/task5.log` — 모두 git 제외

**Interfaces:**
- Consumes: Task 4 에서 확정한 칸 번호가 반영된 펌웨어, Task 2 `aec_metrics.py`.
- Produces: 음량별 잔향 감소량(dB), 처리 부하(%), 내부 메모리 최저값 — Task 8 보고의 H2·H4 판정 근거.

**시험 조건**: 2번 기기를 평소 쓸 자리(책상 위, 세워서)에 둔다. 방을 조용히 한다(Step 1~2 동안 말하지 않는다). 사용자에게 먼저 알린다: "지금부터 2번 기기에서 여자 목소리 안내가 세 번(약 8초씩) 나옵니다. 그동안 조용히 해 주세요."

- [x] **Step 1: 음량 40·60·80 에서 녹음하고 꺼낸다**

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone_probe && for V in 40 60 80; do mkdir -p out/rec_v$V && python3 tools/probe_ctl.py --unit 2 run "until:stat:^STAT .*audio=1:40" "vol $V" "sleep:1" "rec" "wait:REC DONE:25" "dump:mic:out/rec_v$V/mic.pcm" "dump:ref:out/rec_v$V/ref.pcm" "dump:aec:out/rec_v$V/aec.pcm" | tee -a out/task5.log; done
```
Expected (음량마다): `REC DONE samples=135971` 안팎, `saved out/rec_v40/mic.pcm (mic, 271942 B)` 식의 줄 세 개. `!! wait failed: REC DONE`(기기가 `REC FAIL <사유>` 를 냈거나 응답 없음) 또는 `!! dump failed: 크기 불일치/검사합 불일치/끝 표시…` 가 나오면 그 음량만 다시 실행하고, 두 번 연속 실패하면 로그를 보고하고 멈춘다.

- [x] **Step 2: 잔향 감소량 계산**

Run:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone_probe && for V in 40 60 80; do echo "== 음량 $V =="; python3 tools/aec_metrics.py out/rec_v$V; done | tee -a out/task5.log
```
Expected (형식. 숫자는 실측으로 채워진다):
```
== 음량 60 ==
녹음 길이 8.5초, 상대 말소리 구간 5.x초
크기(dBFS)  기준 -xx.x  마이크 원음 -xx.x  에코 제거 후 -xx.x
스피커→마이크 지연 약 x ms, 마이크 포화 비율 0.00%
잔향 감소량(ERLE) xx.x dB → 통과|경계|미달 (통과 25dB 이상, 경계 15~25dB)
들어 보기: afplay out/rec_v60/mic.wav  /  afplay out/rec_v60/aec.wav
```
- `측정 불가: 기준 신호가 거의 없음` → `REF_SLOT` 이 틀렸다. Task 4 Step 3 으로 돌아간다.
- `주의: 마이크가 1% 넘게 포화됨` → 그 음량의 수치는 버린다(더 낮은 음량 결과만 쓴다). 이 자체가 "최대 음량을 낮춰야 한다"는 근거다(PH-D7 후퇴 ①).

- [x] **Step 3: 귀로 확인(사용자)**

음량 60 의 두 파일을 차례로 튼다:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone_probe && afplay out/rec_v60/mic.wav; afplay out/rec_v60/aec.wav
```
사용자에게 묻는다: "첫 번째는 에코 제거 전, 두 번째는 제거 후입니다. 두 번째에서 안내 목소리가 ① 거의 안 들림 ② 작게 들림 ③ 그대로 들림 중 어느 쪽인가요?" 답을 기록한다.

- [x] **Step 4: 겹쳐 말하기(사용자)** — 상대가 말하는 중에 내 목소리가 살아남는지

사용자에게 부탁한다: "이번엔 안내 목소리가 나오는 동안 기기 앞 50cm 에서 '하나, 둘, 셋 … 열' 을 보통 크기로 세어 주세요."
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone_probe && mkdir -p out/rec_talk && python3 tools/probe_ctl.py --unit 2 run "until:stat:^STAT .*audio=1:40" "vol 60" "sleep:1" "rec" "wait:REC DONE:25" "dump:mic:out/rec_talk/mic.pcm" "dump:ref:out/rec_talk/ref.pcm" "dump:aec:out/rec_talk/aec.pcm" && python3 tools/aec_metrics.py out/rec_talk; afplay out/rec_talk/aec.wav
```
이 녹음의 dB 수치는 **쓰지 않는다**(내 목소리가 섞여 있어 의미가 없다). 사용자에게 묻는다: "방금 들은 소리에서 본인 목소리가 ① 또렷함 ② 중간중간 끊김 ③ 거의 안 들림 중 어느 쪽인가요?"

- [x] **Step 5: (H2 가 경계·미달일 때만) 에코 제거 모드를 바꿔 한 번 더**

`probe_main.cpp` 의 `#define PROBE_AEC_MODE AEC_MODE_VOIP_HIGH_PERF` 를 `AEC_MODE_FD_HIGH_PERF` 로 바꾸고:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone_probe && source ~/esp/esp-idf-5.5.4/export.sh && idf.py build && idf.py -p "$(python3 tools/probe_ctl.py port 2)" flash
```
Step 1~4 를 폴더 이름만 `out/fd_rec_v40` 식으로 바꿔 다시 한다. 두 모드의 결과를 나란히 기록하고, 더 나은 쪽을 펌웨어에 남긴다.

- [x] **Step 6: H4 — 처리 시간과 메모리**

Run:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone_probe && python3 tools/probe_ctl.py --unit 2 run "until:stat:^STAT .*audio=1:40" "vol 40" "aec on" "codec on" "loop on" "sleep:5" "stat" "sleep:60" "stat" "wait:^STAT :5" | tee -a out/task5.log
```
**마지막** `STAT` 줄을 쓴다(첫 stat 은 구간 초기화용). 읽을 값과 잠정 합격선(PL-D8):

| 값 | 뜻 | 합격선 |
|---|---|---|
| `load_pct` | 에코 제거 + Opus 압축·해제가 한 코어에서 차지하는 비율 | 50 이하 |
| `aec_us=평균/최대` | 에코 제거 한 묶음 처리 시간(µs) | 최대 < 묶음 길이(`PROBE AUDIO chunk=n` 의 n × 62.5µs) |
| `enc_us`, `dec_us` | Opus 20ms 한 프레임 압축·해제 시간 | 평균 합 < 10000 |
| `heap_int_min` | 내부 메모리 최저 남은 양(바이트) | 100000 이상 |
| `rd_err`, `wr_err` | 소리 읽기·쓰기 오류 | 0 |

`load_pct` 가 50 을 넘으면: `probe_main.cpp` 의 `#define OPUS_COMPLEXITY 5` 를 `0` 으로 바꿔 빌드·플래시 후 이 Step 을 다시 실행하고 두 값을 모두 기록한다. 0 에서도 넘으면 **H4 실패** 로 기록.

- [x] **Step 7: 설정을 바꿨으면 커밋**

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone_probe/main && git commit -m "chore(phone-probe): H2·H4 측정에서 고른 설정 반영(에코 제거 모드·Opus 복잡도)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
(바꾼 것이 없으면 건너뛴다.)

---

### Task 6: H3 — 교환기까지 왕복 시간 어림

**Files:**
- Create: `firmware/tab5_phone_probe/out/task6.log` (git 제외)

**Interfaces:**
- Consumes: 펌웨어의 `wifi`, `rtt` 명령. 2번 기기 NVS 에 저장된 Wi-Fi 목록 + `secrets.h` 의 `WIFI_NETWORKS`.
- Produces: 왕복 시간 중앙값·90% 값(ms) — H3 판정 근거(근사).

- [x] **Step 1: 대상 주소 상태를 맥에서 먼저 확인**

Run: `curl -s -o /dev/null -m 10 -w "HTTP %{http_code}, %{time_total}s\n" https://snapbooth.example.com/healthz`
- `HTTP 200` → 그대로 진행.
- `HTTP 502` → 맥미니의 스냅부스 서버(:5085)가 꺼져 있다는 뜻이다(계획 작성 시점 2026-10-03 19:30 이 상태였다. 터널 프로그램 cloudflared 는 켜져 있었다). **측정은 그대로 진행한다**: 요청이 Cloudflare → 터널 → 맥미니의 cloudflared 까지 갔다가 돌아오므로 경로는 같고, 맥미니 안의 마지막 1ms 구간만 빠진다. 결과에 "status=502 로 측정"이라고 적고, 사용자에게 스냅부스 서버가 꺼져 있음을 알린다(켜는 것은 이 계획의 범위 밖 — 사용자가 결정).
- 그 외(530, 시간 초과 등) → 터널 자체가 내려가 있다. 멈추고 보고.

- [x] **Step 2: 기기에서 20회 측정(소리 꺼짐 상태)**

Run:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone_probe && python3 tools/probe_ctl.py --unit 2 run "until:wifi:^WIFI ok=1:90" "rtt 20" "wait:RTT DONE:90" | tee out/task6.log
```
Expected: `WIFI ok=1 ssid=<이름> ip=<주소> rssi=<값>` 뒤에 `RTT FIRST ms=…`, `RTT i=1..20 ms=… status=…`, `RTT DONE n=20 min= med= p90= max=`.
- `until failed: wifi` → 알려진 Wi-Fi 가 안 보이거나 연결 실패. 로그의 `net_probe: wifi known: saved N + fixed M` 과 `no known network visible` 을 보고에 붙이고 멈춘다(사용자에게 어떤 Wi-Fi 를 쓰는지 확인).
- `PROBE FAIL esp_hosted …` → Wi-Fi 칩 초기화 실패. **`esp_hosted` 판을 바꾸지 말고** 로그를 보고하고 멈춘다.

- [x] **Step 3: 소리를 돌리면서 한 번 더(Wi-Fi 와 소리가 서로 방해하지 않는지)**

Run:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone_probe && python3 tools/probe_ctl.py --unit 2 run "until:wifi:^WIFI ok=1:90" "vol 40" "aec on" "codec on" "loop on" "sleep:3" "stat" "rtt 20" "wait:RTT DONE:90" "stat" "wait:^STAT :5" | tee -a out/task6.log
```
Expected: `RTT DONE …` 와, 두 `STAT` 사이에 `rd_err`·`wr_err`·`i2c_err` 가 늘지 않음.

- [x] **Step 4: 판정 메모**

잠정 합격선(PL-D8): 중앙값 150ms 이하 = 통과, 150~300 = 경계, 초과 = 미달(PH-D1 탈출구 검토). 통화 음성의 한쪽 방향 망 지연은 대략 (A 기기 왕복 ÷ 2) + (B 기기 왕복 ÷ 2) 이므로, 두 기기가 비슷한 망에 있으면 이 왕복값과 비슷하다. **이 값은 한 장소(지금 Wi-Fi)의 어림**이고 실제 통화 지연은 Phase 4 에서 잰다 — 보고에 그대로 적는다.

---

### Task 7: H1 — 화면·터치와 소리 30분 동시 가동

**Files:**
- Create: `firmware/tab5_phone_probe/out/soak.log` (git 제외)

**Interfaces:**
- Consumes: `probe_ctl.py soak` (Task 2), 펌웨어의 `loop`/`aec`/`codec`/`vol`/`stat`.
- Produces: `SOAK PASS` 또는 `SOAK FAIL: <사유>` + 1분 간격 기록 — H1 판정 근거.

- [x] **Step 1: 사용자에게 알린다**

"지금부터 30분 동안 2번 기기가 마이크 소리를 0.5초 뒤에 스피커로 되돌려 냅니다(작은 메아리처럼 들립니다). 시작 직후에 화면을 세 번, 28분쯤에 다시 세 번 눌러 주세요. 그 사이엔 자리를 비워도 됩니다. 삑 하는 울림(하울링)이 계속 나면 알려 주세요."

- [x] **Step 2: 30분 가동** (백그라운드로 실행하고 끝나면 결과를 읽는다. 한 번의 실행 안에서 끝내야 한다 — 포트를 다시 열면 기기가 리셋될 수 있다)

Run:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone_probe && python3 tools/probe_ctl.py --unit 2 soak 30 out/soak.log
```
도구가 하는 일: 음량 40·에코 제거·압축·루프백을 켜고, 10초마다 음량을 40↔45 로 바꿔 코덱에 I2C 쓰기를 일으키고(화면 태스크는 20ms 마다 터치를, 1초마다 코덱 레지스터를 같은 버스로 읽는다), 1분마다 `stat` 을 `out/soak.log` 에 남긴다.
Expected: 1분마다 `[ n분] i2c_err=0 rd_err=0 wr_err=0 load=…% heap_min=…`, 마지막 줄 `SOAK PASS`.

- [x] **Step 3: 터치가 처음과 끝에 모두 먹었는지 기록에서 확인** — ⚠️ 기대값 미충족: 첫 줄 touch=0, 터치는 7~10분 구간에만 35회, 종료 무렵 없음(진행 기록의 Ruling 참조)

Run:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone_probe && grep -o "touch=[0-9]*" out/soak.log | sed -n '1p;$p' && grep -o "heap_int_min=[0-9]*" out/soak.log | sed -n '1p;$p'
```
Expected: 첫 줄 `touch=` 가 3 이상, 마지막 줄 `touch=` 가 첫 줄보다 3 이상 큼. `heap_int_min` 은 처음과 끝이 거의 같다(계속 줄면 메모리가 새는 것 — 결과에 적는다).

- [x] **Step 4: 사용자 확인** — ⚠️ 세 문항 모두 답을 받지 못함(게이트 보고에서 다시 물음)

묻는다: ① "30분 뒤에도 화면의 '가동' 시간과 '묶음' 숫자가 올라가고 있었나요?" ② "삑 하는 울림이 있었나요?" ③ "메아리 소리가 끊기거나 지직거렸나요?" 답을 기록한다.

`SOAK FAIL` 이면: 사유 줄과 `out/soak.log` 마지막 5줄을 보고에 붙인다. `도중 재부팅` 이면 재부팅 직전 로그가 필요하므로 `python3 tools/probe_ctl.py --unit 2 run "sleep:600"` 으로 10분간 로그를 그대로 받아 재현을 시도한다. **H1 실패는 게이트 사유**(설계 수정 후 재승인)다.

---

### Task 8: 2번 기기 복구, 결과 기록, 게이트 보고

**Files:**
- Modify: `docs/exec-plans/2026-10-03-tab5-phone.md` — §4 전제(H1~H4 를 결과로 갱신, 라이브러리 판 정정), §5 Phase 0 체크, §8 진행 로그
- Modify: `docs/exec-plans/2026-10-03-tab5-phone-phase0-1.md` — 이 문서의 체크박스와 진행 로그

**Interfaces:**
- Consumes: Task 4~7 의 측정값과 사용자 답.
- Produces: 사용자에게 가는 게이트 보고서. **이 보고 뒤에는 사용자의 결정 없이 Phase 2 로 넘어가지 않는다.**

- [x] **Step 1: 2번 기기를 스냅부스로 되돌린다(PL-D9)**

```bash
cd ~/tab5-toy-phone/firmware/tab5_snapbooth && source ~/esp/esp-idf-5.5.4/export.sh
PORT=$(python3 ../tab5_phone_probe/tools/probe_ctl.py port 2) && esptool.py -p "$PORT" read_mac | grep -i "aa:bb:cc:dd:ee:02" && idf.py -p "$PORT" flash
python3 ../tab5_phone_probe/tools/probe_ctl.py --unit 2 run "wait:snapbooth: wifi known:60"
```
Expected: 플래시 완료 뒤 로그에 `snapbooth: wifi known: N (saved … + fixed …)` 가 나오고 화면에 스냅부스 대기 화면이 뜬다. (맥미니의 스냅부스 서버가 꺼져 있으면 스타일 목록은 기본 1종으로 뜬다 — 기기 문제가 아니다.)

- [x] **Step 2: 설계서 §8 진행 로그 끝에 결과를 적는다** — 아래 틀의 `…` 을 Task 4~7 의 실측값으로 채운다(빈칸으로 두지 않는다. 못 잰 항목은 "측정 못 함 — 이유"로 적는다)

```markdown
### 2026-10-__ — Phase 0 가정 검증 결과 (2번 기기, 계획 docs/exec-plans/2026-10-03-tab5-phone-phase0-1.md)

| 가정 | 방법 | 결과 | 판정 |
|---|---|---|---|
| H1 화면·터치와 소리 공존 | 30분: 루프백+에코 제거+Opus, 10초마다 코덱 I2C 쓰기, 20ms 마다 터치 읽기 | I2C 오류 …건, 소리 읽기/쓰기 오류 …/…건, 재부팅 …회, 터치 처음 …회·끝 …회, 사용자 관찰: … | 통과/실패 |
| H2 에코 제거 | 시험 음성(7.5초) 재생 중 원음·결과 동시 녹음, 음량 40/60/80 | 잔향 감소 …/…/… dB (모드 …), 마이크 포화 …%, 귀 판정: 에코 "…", 겹쳐 말하기 "…" | 통과/경계/미달 |
| H3 망 왕복(근사) | snapbooth.example.com/healthz 20회 (status …) | 중앙값 … ms, 90% … ms, 최대 … ms (Wi-Fi: …, 신호 … dBm). 소리 동작 중: 중앙값 … ms | 통과/경계/미달 |
| H4 실시간 처리 | 60초 구간, Opus 복잡도 … | 부하 …%, 에코 제거 평균/최대 …/… µs(묶음 … ms), Opus 압축/해제 평균 …/… µs, 내부 메모리 최저 … KB | 통과/실패 |

- 확정한 값: 마이크 칸 …, 기준 칸 …, 송신 방식 `PROBE_TX_TDM=…`, 기준 입력 증폭 … dB, 에코 제거 묶음 … 표본.
- 부품 판(실제로 잡힌 것): esp_codec_dev 1.6.2, esp-sr 2.5.5, esp_audio_codec 2.5.0, esp_hosted 2.11.7, esp-dl …
- 정정: 설계서 §4 의 `esp_audio_codec` 2.6.2 → **2.5.0**(2.6 이상은 P4 rev 3.0 이상에서만 빌드됨). Opus 어셈블리 최적화가 없는 판이라 H4 에서 부하를 확인함.
- 합격선은 계획서 PL-D8 의 잠정값. 최종 판정은 사용자.
```

같은 파일에서: §4 "아직 검증 안 된 가정" 의 H1~H4 각 줄 끝에 `→ 결과: …(2026-10-__)` 를 붙이고, §4 "검증된 사실"의 라이브러리 줄에서 `espressif/esp_audio_codec 2.6.2` 를 `2.5.0(2.6+ 는 P4 rev 3.0 이상 전용)` 으로 고친다. §5 의 `- [ ] **Phase 0 — 가정 검증**` 을 `- [x]` 로 바꾼다(네 가정 모두 측정을 마쳤을 때만).

- [x] **Step 3: 이 계획서의 체크박스와 진행 로그를 갱신하고 커밋**

```bash
cd ~/tab5-toy-phone && git add docs/exec-plans && git commit -m "docs(phone): Phase 0 가정 검증 결과 기록" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" ; git push
```

- [x] **Step 4: 사용자에게 게이트 보고**

`~/CLAUDE.md` 의 큰 작업 보고 형식(**[목표] [한 일] [증거] [검증] [리스크] [다음]**)으로, 쉬운 말로 쓴다. [증거] 에는 Step 2 의 표를 그대로 넣는다. [다음] 에는 설계서 §5 게이트 규칙에 따라 **결정이 필요한 것만** 적는다:

| 결과 | 사용자에게 묻는 것 |
|---|---|
| H1 또는 H4 실패 | "설계를 고쳐야 합니다. 원인은 … 이고 고칠 안은 … 입니다. 이 안으로 설계서를 수정해 다시 승인받겠습니다." (Phase 2 계획을 쓰지 않는다) |
| H2 경계·미달 | PH-D7 후퇴 경로 중 고르기: ① 최대 음량을 …까지로 제한(그 음량에서는 … dB) ② 반이중(상대가 말할 때 내 마이크를 줄임) ③ 누르고 말하기 |
| H3 경계·미달 | 그대로 진행하고 Phase 4 에서 실제 지연을 재 볼지, PH-D1 탈출구(LiveKit)를 먼저 검토할지 |
| 모두 통과 | "Phase 2~3 상세 계획을 쓰겠습니다" 에 대한 승인 |

보고를 보낸 뒤 **여기서 멈춘다.**

---

# Phase 1 — 교환기 (맥북에서만 실행)

### Task 9: 교환기 순수 로직 `phone_core` (시험 먼저)

**Files:**
- Create: `server/phone_core.py`
- Create: `server/pytest.ini`, `server/requirements-phone.txt`
- Test: `server/tests/test_phone_core.py`

**Interfaces:**
- Consumes: 없음(표준 라이브러리 `hmac`, `json` 만).
- Produces (Task 10 이 그대로 쓴다):
  - 상수 `IDLE, RINGING, IN_CALL`, `RING_TIMEOUT_S=30.0`, `SILENCE_TIMEOUT_S=45.0`, `MAX_VOICE_BYTES=400`, `MIN_VOICE_BYTES=3`, `MAX_TEXT_BYTES=256`, `MAX_PEER_BUFFER_BYTES=4096`, `MIN_TOKEN_CHARS=16`
  - `parse_tokens(spec: str) -> dict[str, str]` — `"tab5-1:토큰,tab5-2:토큰"`, 정확히 2대·토큰 16자 이상, 아니면 `ValueError`(문구에 토큰 없음)
  - `check_token(tokens: dict, dev, token) -> bool`
  - `should_drop_voice(peer_buffered_bytes: int) -> bool`
  - `class Exchange(device_ids)`: `attach(dev, conn, now) -> list`, `detach(dev, conn, now) -> list`, `on_text(dev, raw: str, now) -> list`, `route_voice(dev, size: int, now) -> str | None`, `tick(now) -> list`, `conn_of(dev)`, `is_online(dev)`, `peer_of(dev)`, `snapshot() -> dict`, 속성 `state`, `stats`(키: `voice_fwd`, `voice_drop_big`, `voice_drop_state`, `voice_drop_backlog`, `bad_signal`)
  - 돌려주는 "할 일" 형식: `("send", 기기ID, dict)` / `("close", conn)` / `("log", dict)`. `now` 는 호출자가 주는 단조 증가 초(float). `conn` 은 불투명 객체(같은지만 `is` 로 본다).
  - 신호 의미(PL-D1·D2 포함): `ended` 는 붙어 있는 양쪽 모두에게. 벨 중 이탈 = `offline`, 통화 중 이탈 = `lost`. `hangup` 은 벨 중·통화 중 어느 쪽이 보내도 끝낸다. `reject`·`accept` 는 벨 받는 쪽만. 벨 받는 쪽의 `call` = `accept`. 상태에 안 맞는 신호·깨진 글자는 응답 없이 무시(`stats["bad_signal"]`).

- [x] **Step 1: 시험 설정 파일 두 개**

`server/pytest.ini`:
```ini
[pytest]
testpaths = tests
asyncio_mode = auto
```
(`server/tools/test_*.py` 는 pytest 용이 아니라 가져오기만 해도 실행되는 스크립트다 — `testpaths` 로 `tests/` 만 모은다. 시험은 항상 `cd server` 한 뒤 실행한다.)

`server/requirements-phone.txt`:
```
# 교환기 실행 (맥미니 배포 때 venv 에 설치 — Phase 4)
aiohttp>=3.14,<3.15
# 시험 (개발 기기에서만)
pytest>=9,<10
pytest-asyncio>=1.3,<2
```

- [x] **Step 2: 실패하는 시험을 먼저 쓴다** — `server/tests/test_phone_core.py`

```python
"""phone_core 순수 로직 시험. 실행: cd server && python3 -m pytest tests/test_phone_core.py -q"""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import phone_core as pc

A, B = "tab5-1", "tab5-2"
TOK = {A: "a" * 32, B: "b" * 32}


def sends(acts, dev=None):
    """할 일 목록에서 send 만 (기기, 메시지) 로 뽑는다."""
    return [(a[1], a[2]) for a in acts if a[0] == "send" and (dev is None or a[1] == dev)]


def logs(acts):
    return [a[1] for a in acts if a[0] == "log"]


def both_online(now=0.0):
    ex = pc.Exchange([A, B])
    ex.attach(A, "connA", now)
    ex.attach(B, "connB", now)
    return ex


# ── 토큰 ────────────────────────────────────────────────────────────────────
def test_parse_tokens_ok():
    assert pc.parse_tokens(f"{A}:{'x' * 16}, {B}:{'y' * 16}") == {A: "x" * 16, B: "y" * 16}


@pytest.mark.parametrize("spec", ["", f"{A}:{'x' * 16}", f"{A}:short,{B}:{'y' * 16}", f"{A}{'x' * 16},{B}:{'y' * 16}",
                                  f"{A}:{'x' * 16},{A}:{'y' * 16}", f"{A}:{'x' * 16},{B}:{'y' * 16},tab5-3:{'z' * 16}"])
def test_parse_tokens_rejects_bad_spec(spec):
    with pytest.raises(ValueError):
        pc.parse_tokens(spec)


def test_parse_tokens_error_does_not_leak_token():
    secret = "SECRETSECRET"                          # 16자 미만 → 오류
    with pytest.raises(ValueError) as e:
        pc.parse_tokens(f"{A}:{secret},{B}:{'y' * 16}")
    assert secret not in str(e.value)


def test_check_token():
    assert pc.check_token(TOK, A, "a" * 32)
    assert not pc.check_token(TOK, A, "b" * 32)       # 남의 토큰
    assert not pc.check_token(TOK, "tab5-9", "a" * 32)  # 미등록 기기
    assert not pc.check_token(TOK, A, "")
    assert not pc.check_token(TOK, None, None)        # 헤더 없음


def test_exchange_needs_two_distinct_devices():
    with pytest.raises(ValueError):
        pc.Exchange([A])
    with pytest.raises(ValueError):
        pc.Exchange([A, A])


# ── 접속·이탈 ───────────────────────────────────────────────────────────────
def test_attach_reports_peer_state_both_ways():
    ex = pc.Exchange([A, B])
    assert sends(ex.attach(A, "connA", 0)) == [(A, {"t": "peer", "online": False})]
    assert sends(ex.attach(B, "connB", 1)) == [(B, {"t": "peer", "online": True}), (A, {"t": "peer", "online": True})]


def test_attach_unknown_device_raises():
    with pytest.raises(ValueError):
        pc.Exchange([A, B]).attach("tab5-9", "c", 0)


def test_detach_tells_peer_offline():
    ex = both_online()
    assert sends(ex.detach(A, "connA", 5)) == [(B, {"t": "peer", "online": False})]
    assert not ex.is_online(A)


def test_reattach_same_id_closes_old_connection():
    ex = both_online()
    acts = ex.attach(A, "connA2", 5)
    assert ("close", "connA") in acts
    assert ex.conn_of(A) == "connA2"


def test_late_detach_of_replaced_connection_is_ignored():
    """검토 초점 1: 재접속 뒤 옛 연결의 뒤늦은 이탈이 새 연결을 쫓아내면 안 된다."""
    ex = both_online()
    ex.attach(A, "connA2", 5)
    assert ex.detach(A, "connA", 6) == []
    assert ex.conn_of(A) == "connA2"


# ── 걸기·받기·거절·끊기 ─────────────────────────────────────────────────────
def test_call_rings_peer():
    ex = both_online()
    acts = ex.on_text(A, '{"t":"call"}', 1)
    assert sends(acts) == [(A, {"t": "ringing"}), (B, {"t": "ring"})]
    assert ex.state == pc.RINGING


def test_call_when_peer_offline_ends_offline():
    ex = pc.Exchange([A, B])
    ex.attach(A, "connA", 0)
    acts = ex.on_text(A, '{"t":"call"}', 1)
    assert sends(acts) == [(A, {"t": "ended", "why": "offline"})]
    assert ex.state == pc.IDLE


def test_accept_connects_both():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    acts = ex.on_text(B, '{"t":"accept"}', 4)
    assert sends(acts) == [(A, {"t": "connected"}), (B, {"t": "connected"})]
    assert ex.state == pc.IN_CALL


def test_reject_ends_both_with_reject():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    acts = ex.on_text(B, '{"t":"reject"}', 4)
    assert sends(acts) == [(A, {"t": "ended", "why": "reject"}), (B, {"t": "ended", "why": "reject"})]
    assert ex.state == pc.IDLE


def test_hangup_in_call_ends_both_and_logs_length():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    ex.on_text(B, '{"t":"accept"}', 4)
    acts = ex.on_text(B, '{"t":"hangup"}', 64.5)
    assert sends(acts) == [(A, {"t": "ended", "why": "hangup"}), (B, {"t": "ended", "why": "hangup"})]
    assert logs(acts) == [{"ev": "call_end", "caller": A, "callee": B, "why": "hangup", "ring_s": 3.0, "talk_s": 60.5}]


def test_caller_cancel_while_ringing():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    acts = ex.on_text(A, '{"t":"hangup"}', 3)
    assert sends(acts) == [(A, {"t": "ended", "why": "hangup"}), (B, {"t": "ended", "why": "hangup"})]


def test_ring_timeout_after_30s():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    ex.on_text(A, '{"t":"ping","n":1}', 20)          # 생존 확인은 벨 제한과 무관
    ex.on_text(B, '{"t":"ping","n":1}', 20)
    assert sends(ex.tick(30.9)) == []
    acts = ex.tick(31.0)
    assert sends(acts) == [(A, {"t": "ended", "why": "timeout"}), (B, {"t": "ended", "why": "timeout"})]
    assert ex.state == pc.IDLE


def test_glare_call_from_callee_is_accept():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    acts = ex.on_text(B, '{"t":"call"}', 1.2)
    assert sends(acts) == [(A, {"t": "connected"}), (B, {"t": "connected"})]


# ── 상태에 안 맞는 신호 ─────────────────────────────────────────────────────
@pytest.mark.parametrize("sig", ["accept", "reject", "hangup"])
def test_signals_ignored_when_idle(sig):
    ex = both_online()
    assert ex.on_text(A, '{"t":"%s"}' % sig, 1) == []
    assert ex.state == pc.IDLE


def test_caller_cannot_accept_or_reject_own_call():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    assert ex.on_text(A, '{"t":"accept"}', 2) == []
    assert ex.on_text(A, '{"t":"reject"}', 2) == []
    assert ex.on_text(A, '{"t":"call"}', 2) == []      # 거듭 걸기
    assert ex.state == pc.RINGING


def test_call_during_call_ignored():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    ex.on_text(B, '{"t":"accept"}', 2)
    assert ex.on_text(A, '{"t":"call"}', 3) == []
    assert ex.on_text(B, '{"t":"call"}', 3) == []
    assert ex.state == pc.IN_CALL


@pytest.mark.parametrize("raw", ["", "not json", "[]", "123", '{"x":1}', '{"t":5}', '{"t":"dance"}',
                                 '{"t":"ping"}', '{"t":"ping","n":"1"}', '{"t":"ping","n":true}',
                                 '{"t":"call","pad":"' + "x" * 300 + '"}'])
def test_garbage_text_is_ignored(raw):
    """검토 초점 2: 깨진·엉뚱한·너무 긴 신호에 죽지 않고, 상태도 안 바뀐다."""
    ex = both_online()
    assert ex.on_text(A, raw, 1) == []
    assert ex.state == pc.IDLE
    assert ex.stats["bad_signal"] == 1


def test_ping_pong_echoes_n():
    ex = both_online()
    assert sends(ex.on_text(A, '{"t":"ping","n":7}', 1)) == [(A, {"t": "pong", "n": 7})]


# ── 통화 도중 이탈 ──────────────────────────────────────────────────────────
def test_disconnect_in_call_gives_lost_then_peer_offline():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    ex.on_text(B, '{"t":"accept"}', 2)
    acts = ex.detach(B, "connB", 10)
    assert sends(acts) == [(A, {"t": "ended", "why": "lost"}), (A, {"t": "peer", "online": False})]
    assert ex.state == pc.IDLE


def test_disconnect_while_ringing_gives_offline():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    acts = ex.detach(B, "connB", 5)
    assert sends(acts) == [(A, {"t": "ended", "why": "offline"}), (A, {"t": "peer", "online": False})]


def test_reattach_during_call_ends_call_as_lost():
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    ex.on_text(B, '{"t":"accept"}', 2)
    acts = ex.attach(B, "connB2", 10)                # B 가 재부팅해 다시 붙음
    assert (A, {"t": "ended", "why": "lost"}) in sends(acts)
    assert ("close", "connB") in acts
    assert sends(acts)[-2:] == [(B, {"t": "peer", "online": True}), (A, {"t": "peer", "online": True})]
    assert ex.state == pc.IDLE


# ── 무소식 45초 ─────────────────────────────────────────────────────────────
def test_silence_45s_closes_and_notifies_peer():
    ex = both_online(0)
    ex.on_text(B, '{"t":"ping","n":1}', 40)
    assert ex.tick(45.0) == []                        # 45초 "넘어야" 끊는다
    acts = ex.tick(45.1)
    assert ("close", "connA") in acts
    assert sends(acts) == [(B, {"t": "peer", "online": False})]
    assert not ex.is_online(A) and ex.is_online(B)


def test_silence_in_call_gives_lost():
    ex = both_online(0)
    ex.on_text(A, '{"t":"call"}', 1)
    ex.on_text(B, '{"t":"accept"}', 2)
    for t in range(5, 60, 5):                         # B 만 계속 소식을 보냄 (음성도 소식)
        assert ex.route_voice(B, 60, t) == A
    acts = ex.tick(47.0)                              # A 의 마지막 소식 = 1초
    assert (B, {"t": "ended", "why": "lost"}) in sends(acts)
    assert ("close", "connA") in acts


# ── 음성 ────────────────────────────────────────────────────────────────────
def test_voice_forwarded_only_in_call():
    ex = both_online()
    assert ex.route_voice(A, 60, 1) is None           # 대기 중
    ex.on_text(A, '{"t":"call"}', 1)
    assert ex.route_voice(A, 60, 2) is None           # 벨 울리는 중
    ex.on_text(B, '{"t":"accept"}', 3)
    assert ex.route_voice(A, 60, 4) == B
    assert ex.route_voice(B, 60, 4) == A
    ex.on_text(A, '{"t":"hangup"}', 5)
    assert ex.route_voice(A, 60, 6) is None           # 끊은 뒤
    assert ex.stats["voice_fwd"] == 2 and ex.stats["voice_drop_state"] == 3


@pytest.mark.parametrize("size,ok", [(2, False), (3, True), (400, True), (401, False), (4000, False)])
def test_voice_size_limits(size, ok):
    ex = both_online()
    ex.on_text(A, '{"t":"call"}', 1)
    ex.on_text(B, '{"t":"accept"}', 2)
    assert (ex.route_voice(A, size, 3) == B) is ok


def test_should_drop_voice_when_peer_backlogged():
    """검토 초점 4: 상대 쪽이 밀려 있으면 쌓지 않고 버린다."""
    assert not pc.should_drop_voice(0)
    assert not pc.should_drop_voice(pc.MAX_PEER_BUFFER_BYTES)
    assert pc.should_drop_voice(pc.MAX_PEER_BUFFER_BYTES + 1)


def test_frames_from_detached_device_are_ignored():
    ex = both_online()
    ex.detach(A, "connA", 1)
    assert ex.on_text(A, '{"t":"call"}', 2) == []
    assert ex.route_voice(A, 60, 2) is None


def test_log_records_never_contain_payload_fields():
    """기록에는 시각·기기·길이·사유만 — 허용된 키만 나온다."""
    ex = both_online()
    acts = ex.on_text(A, '{"t":"call"}', 1) + ex.on_text(B, '{"t":"accept"}', 2) + ex.on_text(A, '{"t":"hangup"}', 9)
    acts += ex.detach(A, "connA", 10)
    allowed = {"ev", "dev", "caller", "callee", "why", "ring_s", "talk_s"}
    for rec in logs(acts):
        assert set(rec) <= allowed
```

- [x] **Step 3: 실패를 확인**

Run: `cd ~/tab5-toy-phone/server && python3 -m pytest tests/test_phone_core.py -q`
Expected: 수집 단계에서 FAIL — `ModuleNotFoundError: No module named 'phone_core'`

- [x] **Step 4: `server/phone_core.py` 작성**

```python
"""phone_core.py — Tab5 전화 교환기의 순수 로직 (네트워크·시계 없음, pytest 로 바로 시험).

설계: docs/exec-plans/2026-10-03-tab5-phone.md §9.3·§9.4
  - 등록된 기기 2대의 접속·이탈, 통화 상태(IDLE/RINGING/IN_CALL), 30초 벨 제한, 45초 무소식 끊기, 신호 검증.
  - 시각은 호출자가 now(초, 단조 증가)로 넣는다. 결과는 "할 일" 목록으로 돌려준다:
      ("send", 기기ID, dict)   그 기기에 글자 프레임(JSON) 보내기
      ("close", conn)          그 연결 닫기
      ("log", dict)            기록 남기기 (시각·기기·통화 길이·종료 사유만 — 음성·토큰은 절대 넣지 않는다)
"""
import hmac
import json

IDLE, RINGING, IN_CALL = "IDLE", "RINGING", "IN_CALL"
RING_TIMEOUT_S = 30.0          # 벨 제한 (§9.3)
SILENCE_TIMEOUT_S = 45.0       # 무소식 끊기 (§9.4)
MAX_VOICE_BYTES = 400          # 이보다 큰 음성 프레임은 버린다 (§9.4)
MIN_VOICE_BYTES = 3            # 순번 2바이트 + Opus 최소 1바이트
MAX_TEXT_BYTES = 256           # 신호(JSON)는 수십 바이트 — 이보다 길면 무시
MAX_PEER_BUFFER_BYTES = 4096   # PL-D3: 상대 쪽 송신 대기열이 이보다 쌓이면 음성을 버린다(약 1초 분량)
MIN_TOKEN_CHARS = 16


def parse_tokens(spec):
    """'tab5-1:토큰,tab5-2:토큰' → {기기ID: 토큰}. 정확히 2대, 토큰 16자 이상. 오류 문구에 토큰을 넣지 않는다."""
    out = {}
    for part in (spec or "").split(","):
        part = part.strip()
        if not part:
            continue
        dev, sep, tok = part.partition(":")
        dev, tok = dev.strip(), tok.strip()
        if not sep or not dev or not tok:
            raise ValueError("PHONE_TOKENS 형식 오류: '기기ID:토큰,기기ID:토큰'")
        if len(tok) < MIN_TOKEN_CHARS:
            raise ValueError(f"PHONE_TOKENS: {dev} 의 토큰이 너무 짧음({MIN_TOKEN_CHARS}자 이상)")
        if dev in out:
            raise ValueError(f"PHONE_TOKENS: 기기 ID 중복({dev})")
        out[dev] = tok
    if len(out) != 2:
        raise ValueError("PHONE_TOKENS: 기기는 정확히 2대여야 함")
    return out


def check_token(tokens, dev, token):
    """등록된 기기이고 토큰이 맞으면 True. 비교는 hmac.compare_digest (시간차 공격 방지)."""
    if not isinstance(dev, str) or not isinstance(token, str):
        return False
    want = tokens.get(dev)
    if want is None:
        hmac.compare_digest(token.encode(), b"\0" * len(token.encode()))   # 미등록 ID 도 같은 시간 소모
        return False
    return hmac.compare_digest(token.encode(), want.encode())


def should_drop_voice(peer_buffered_bytes):
    """PL-D3: 상대 연결의 송신 대기열이 밀려 있으면 True (지연이 쌓이지 않게 새 음성을 버린다)."""
    return peer_buffered_bytes > MAX_PEER_BUFFER_BYTES


class Exchange:
    """기기 2대 사이의 통화 1건을 관리한다."""

    def __init__(self, device_ids):
        ids = sorted(device_ids)
        if len(ids) != 2 or ids[0] == ids[1]:
            raise ValueError("기기는 서로 다른 2대여야 함")
        self.devices = tuple(ids)
        self.conns = {}            # 기기ID → 현재 연결(불투명 객체)
        self.last_seen = {}        # 기기ID → 마지막으로 프레임을 받은 시각
        self.state = IDLE
        self.caller = None
        self.callee = None
        self.ring_deadline = 0.0
        self.ring_started = 0.0
        self.call_started = 0.0
        self.stats = {"voice_fwd": 0, "voice_drop_big": 0, "voice_drop_state": 0, "voice_drop_backlog": 0, "bad_signal": 0}

    # ── 조회 ────────────────────────────────────────────────────────────────
    def peer_of(self, dev):
        return self.devices[1] if dev == self.devices[0] else self.devices[0]

    def conn_of(self, dev):
        return self.conns.get(dev)

    def is_online(self, dev):
        return dev in self.conns

    def snapshot(self):
        return {"state": self.state, "online": sorted(self.conns), "stats": dict(self.stats)}

    # ── 접속·이탈 ───────────────────────────────────────────────────────────
    def attach(self, dev, conn, now):
        if dev not in self.devices:
            raise ValueError("미등록 기기")
        acts = []
        old = self.conns.get(dev)
        if old is not None:                      # 같은 ID 가 다시 붙음 → 옛 연결 정리 후 교체 (§9.3)
            acts += self.detach(dev, old, now)
            acts.append(("close", old))
        self.conns[dev] = conn
        self.last_seen[dev] = now
        peer = self.peer_of(dev)
        acts.append(("log", {"ev": "online", "dev": dev}))
        acts.append(("send", dev, {"t": "peer", "online": self.is_online(peer)}))
        if self.is_online(peer):
            acts.append(("send", peer, {"t": "peer", "online": True}))
        return acts

    def detach(self, dev, conn, now):
        if self.conns.get(dev) is not conn:      # 이미 교체된 옛 연결의 뒤늦은 이탈 → 무시
            return []
        del self.conns[dev]
        self.last_seen.pop(dev, None)
        acts = [("log", {"ev": "offline", "dev": dev})]
        if self.state != IDLE and dev in (self.caller, self.callee):
            # PL-D2: 통화 중 이탈 = lost, 벨 울리는 중 이탈 = offline
            acts += self._end("lost" if self.state == IN_CALL else "offline", now)
        peer = self.peer_of(dev)
        if self.is_online(peer):
            acts.append(("send", peer, {"t": "peer", "online": False}))
        return acts

    # ── 신호 ────────────────────────────────────────────────────────────────
    def on_text(self, dev, raw, now):
        if not self.is_online(dev):
            return []
        self.last_seen[dev] = now                # 어떤 프레임이든 "소식"으로 친다
        msg = self._parse(raw)
        if msg is None:
            self.stats["bad_signal"] += 1
            return []
        t = msg["t"]
        if t == "ping":
            n = msg.get("n")
            if isinstance(n, bool) or not isinstance(n, int):
                self.stats["bad_signal"] += 1
                return []
            return [("send", dev, {"t": "pong", "n": n})]
        if t == "call":
            return self._on_call(dev, now)
        if t == "accept":
            if self.state == RINGING and dev == self.callee:
                return self._connect(now)
        elif t == "reject":
            if self.state == RINGING and dev == self.callee:
                return self._end("reject", now)
        elif t == "hangup":
            if self.state != IDLE and dev in (self.caller, self.callee):
                return self._end("hangup", now)
        else:
            self.stats["bad_signal"] += 1
        return []                                # 상태에 안 맞는 신호는 조용히 무시

    @staticmethod
    def _parse(raw):
        if not isinstance(raw, str) or len(raw.encode("utf-8", "replace")) > MAX_TEXT_BYTES:
            return None
        try:
            msg = json.loads(raw)
        except ValueError:
            return None
        if not isinstance(msg, dict) or not isinstance(msg.get("t"), str):
            return None
        return msg

    def _on_call(self, dev, now):
        if self.state == IDLE:
            peer = self.peer_of(dev)
            if not self.is_online(peer):
                return [("send", dev, {"t": "ended", "why": "offline"}),
                        ("log", {"ev": "call_end", "caller": dev, "callee": peer, "why": "offline", "ring_s": 0, "talk_s": 0})]
            self.state, self.caller, self.callee = RINGING, dev, peer
            self.ring_started, self.ring_deadline = now, now + RING_TIMEOUT_S
            return [("send", dev, {"t": "ringing"}), ("send", peer, {"t": "ring"})]
        if self.state == RINGING and dev == self.callee:
            return self._connect(now)            # 동시 발신: 벨 울리는 쪽의 call 은 accept 로 취급 (§9.4)
        return []

    def _connect(self, now):
        self.state, self.call_started = IN_CALL, now
        return [("send", self.caller, {"t": "connected"}), ("send", self.callee, {"t": "connected"})]

    def _end(self, why, now):
        """통화(또는 벨)를 끝낸다. 아직 붙어 있는 양쪽 모두에 ended 를 보낸다(PL-D1)."""
        caller, callee = self.caller, self.callee
        talk = round(now - self.call_started, 1) if self.state == IN_CALL else 0
        ring = round((self.call_started if self.state == IN_CALL else now) - self.ring_started, 1)
        self.state, self.caller, self.callee = IDLE, None, None
        acts = [("send", d, {"t": "ended", "why": why}) for d in (caller, callee) if self.is_online(d)]
        acts.append(("log", {"ev": "call_end", "caller": caller, "callee": callee, "why": why, "ring_s": ring, "talk_s": talk}))
        return acts

    # ── 음성 ────────────────────────────────────────────────────────────────
    def route_voice(self, dev, size, now):
        """이진 프레임을 건넬 상대 기기ID, 버려야 하면 None. 내용은 보지 않는다(크기만)."""
        if not self.is_online(dev):
            return None
        self.last_seen[dev] = now
        if size > MAX_VOICE_BYTES or size < MIN_VOICE_BYTES:
            self.stats["voice_drop_big"] += 1
            return None
        if self.state != IN_CALL or dev not in (self.caller, self.callee):
            self.stats["voice_drop_state"] += 1
            return None
        self.stats["voice_fwd"] += 1
        return self.peer_of(dev)

    # ── 시간 경과 ───────────────────────────────────────────────────────────
    def tick(self, now):
        acts = []
        for dev in list(self.conns):
            if now - self.last_seen.get(dev, now) > SILENCE_TIMEOUT_S:
                conn = self.conns[dev]
                acts.append(("log", {"ev": "silence_close", "dev": dev}))
                acts += self.detach(dev, conn, now)
                acts.append(("close", conn))
        if self.state == RINGING and now >= self.ring_deadline:
            acts += self._end("timeout", now)
        return acts
```

- [x] **Step 5: 통과 확인**

Run: `cd ~/tab5-toy-phone/server && python3 -m pytest tests/test_phone_core.py -q`
Expected: `54 passed`

- [x] **Step 6: 커밋**

```bash
cd ~/tab5-toy-phone && git add server/phone_core.py server/pytest.ini server/requirements-phone.txt server/tests/test_phone_core.py && git commit -m "feat(phone): 교환기 순수 로직 phone_core(접속·통화 상태·30초 벨·45초 무소식·신호 검증)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 10: 교환기 중계 `phone_relay` (가짜 기기 2대로 시험)

**Files:**
- Create: `server/phone_relay.py`
- Test: `server/tests/test_phone_relay.py`

**Interfaces:**
- Consumes: Task 9 의 `phone_core` 전부(위 Produces 그대로).
- Produces:
  - `create_app(tokens: dict, clock=time.monotonic, tick_interval=1.0) -> aiohttp.web.Application` — 시험은 `clock` 에 가짜 시계를, `tick_interval` 에 0.01 을 넣는다
  - `load_env_file(path)` — `KEY=VALUE` 줄을 환경변수로(이미 있으면 유지)
  - `AppKey`: `EXCHANGE`, `TOKENS`, `CLOCK`, `TICK_INTERVAL`, `BG_TASKS`
  - HTTP: `GET /healthz` → 200 `{"ok": true}` / `GET /ws` → 헤더 인증 실패 시 401, 성공 시 WebSocket
  - 실행: `python3 server/phone_relay.py` (`server/.env.phone` 의 `PHONE_TOKENS`, `PORT`=5094, `BIND`=127.0.0.1)

- [x] **Step 1: 실패하는 시험을 먼저 쓴다** — `server/tests/test_phone_relay.py`

```python
"""phone_relay 시험 — 가짜 기기 2대(WebSocket 클라이언트)로 실제 중계를 돌려 본다.
실행: cd server && python3 -m pytest tests/test_phone_relay.py -q
"""
import asyncio
import logging
import sys
from pathlib import Path

import pytest
import pytest_asyncio
from aiohttp import WSMsgType, WSServerHandshakeError
from aiohttp.test_utils import TestClient, TestServer

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import phone_core
import phone_relay

A, B = "tab5-1", "tab5-2"
TOK = {A: "tokenA-0123456789abcdef", B: "tokenB-0123456789abcdef"}
VOICE = b"\x00\x01" + b"OPUSPAYLOAD-" * 5          # 순번 2바이트 + 60바이트


class FakeClock:
    """시험이 시간을 직접 넘긴다(30초 벨 제한·45초 무소식을 기다리지 않으려고)."""

    def __init__(self):
        self.t = 1000.0

    def __call__(self):
        return self.t


@pytest_asyncio.fixture
async def relay():
    clock = FakeClock()
    app = phone_relay.create_app(TOK, clock=clock, tick_interval=0.01)
    async with TestClient(TestServer(app)) as client:
        yield client, clock, app


def hdr(dev, token=None):
    return {"X-Device-Id": dev, "X-Device-Token": TOK[dev] if token is None else token}


async def connect(client, dev):
    return await client.ws_connect("/ws", headers=hdr(dev))


async def rx(ws):
    return await ws.receive_json(timeout=2)


async def assert_silent(ws, secs=0.15):
    with pytest.raises(asyncio.TimeoutError):
        await ws.receive(timeout=secs)


async def pair(client):
    """두 기기를 붙이고 서로의 접속 알림까지 읽어 낸다."""
    a = await connect(client, A)
    assert await rx(a) == {"t": "peer", "online": False}
    b = await connect(client, B)
    assert await rx(b) == {"t": "peer", "online": True}
    assert await rx(a) == {"t": "peer", "online": True}
    return a, b


async def in_call(client):
    a, b = await pair(client)
    await a.send_json({"t": "call"})
    assert await rx(a) == {"t": "ringing"}
    assert await rx(b) == {"t": "ring"}
    await b.send_json({"t": "accept"})
    assert await rx(a) == {"t": "connected"}
    assert await rx(b) == {"t": "connected"}
    return a, b


# ── 생존 확인·인증 ──────────────────────────────────────────────────────────
async def test_healthz_reveals_nothing_but_ok(relay):
    client, _, _ = relay
    r = await client.get("/healthz")
    assert r.status == 200 and await r.json() == {"ok": True}


@pytest.mark.parametrize("headers", [
    {},                                                       # 헤더 없음
    {"X-Device-Id": A},                                       # 토큰 없음
    {"X-Device-Id": A, "X-Device-Token": "wrong-token-0123456789"},
    {"X-Device-Id": A, "X-Device-Token": TOK[B]},             # 남의 토큰
    {"X-Device-Id": "tab5-9", "X-Device-Token": TOK[A]},      # 미등록 기기
])
async def test_bad_credentials_get_401(relay, headers):
    client, _, app = relay
    with pytest.raises(WSServerHandshakeError) as e:
        await client.ws_connect("/ws", headers=headers)
    assert e.value.status == 401
    assert app[phone_relay.EXCHANGE].snapshot()["online"] == []


async def test_token_in_query_string_is_not_accepted(relay):
    """검토 초점 3: 주소창 매개변수로 온 토큰은 읽지 않는다(로그에 남는 자리)."""
    client, _, _ = relay
    with pytest.raises(WSServerHandshakeError) as e:
        await client.ws_connect(f"/ws?id={A}&token={TOK[A]}&X-Device-Id={A}&X-Device-Token={TOK[A]}")
    assert e.value.status == 401


# ── 걸기·받기·거절·무응답·동시 발신 ─────────────────────────────────────────
async def test_call_accept_voice_hangup(relay):
    client, _, _ = relay
    a, b = await in_call(client)
    await a.send_bytes(VOICE)
    m = await b.receive(timeout=2)
    assert m.type == WSMsgType.BINARY and m.data == VOICE     # 내용을 건드리지 않고 그대로
    await b.send_bytes(VOICE[::-1])
    assert (await a.receive(timeout=2)).data == VOICE[::-1]
    await a.send_json({"t": "hangup"})
    assert await rx(a) == {"t": "ended", "why": "hangup"}
    assert await rx(b) == {"t": "ended", "why": "hangup"}


async def test_reject(relay):
    client, _, _ = relay
    a, b = await pair(client)
    await a.send_json({"t": "call"})
    await rx(a), await rx(b)
    await b.send_json({"t": "reject"})
    assert await rx(a) == {"t": "ended", "why": "reject"}
    assert await rx(b) == {"t": "ended", "why": "reject"}


async def test_no_answer_times_out_after_30s(relay):
    client, clock, _ = relay
    a, b = await pair(client)
    await a.send_json({"t": "call"})
    await rx(a), await rx(b)
    clock.t += 29.0
    await assert_silent(a)
    clock.t += 1.5
    assert await rx(a) == {"t": "ended", "why": "timeout"}
    assert await rx(b) == {"t": "ended", "why": "timeout"}


async def test_simultaneous_call_connects(relay):
    client, _, _ = relay
    a, b = await pair(client)
    await a.send_json({"t": "call"})
    assert await rx(a) == {"t": "ringing"}
    assert await rx(b) == {"t": "ring"}
    await b.send_json({"t": "call"})                          # 벨 울리는 쪽도 걸었다 → 바로 연결
    assert await rx(a) == {"t": "connected"}
    assert await rx(b) == {"t": "connected"}


async def test_call_to_offline_peer(relay):
    client, _, _ = relay
    a = await connect(client, A)
    assert await rx(a) == {"t": "peer", "online": False}
    await a.send_json({"t": "call"})
    assert await rx(a) == {"t": "ended", "why": "offline"}


async def test_ping_pong(relay):
    client, _, _ = relay
    a = await connect(client, A)
    await rx(a)
    await a.send_json({"t": "ping", "n": 42})
    assert await rx(a) == {"t": "pong", "n": 42}


# ── 음성 폐기 ───────────────────────────────────────────────────────────────
async def test_voice_outside_call_is_not_forwarded(relay):
    client, _, _ = relay
    a, b = await pair(client)
    await a.send_bytes(VOICE)
    await assert_silent(b)


async def test_oversized_voice_frame_is_dropped(relay):
    client, _, app = relay
    a, b = await in_call(client)
    await a.send_bytes(b"\x00\x02" + b"x" * 399)              # 401바이트 → 버림
    await a.send_bytes(VOICE)                                 # 다음 정상 프레임은 통과
    assert (await b.receive(timeout=2)).data == VOICE
    assert app[phone_relay.EXCHANGE].stats["voice_drop_big"] == 1


async def test_voice_is_dropped_when_peer_is_backlogged(relay, monkeypatch):
    """검토 초점 4: 상대 쪽 송신 대기열이 한도를 넘으면 쌓지 않고 버린다(지연 누적 방지)."""
    client, _, app = relay
    a, b = await in_call(client)
    monkeypatch.setattr(phone_core, "MAX_PEER_BUFFER_BYTES", -1)   # 대기열 0바이트도 "밀림"으로 보게 해 경로를 시험
    await a.send_bytes(VOICE)
    await assert_silent(b)
    assert app[phone_relay.EXCHANGE].stats["voice_drop_backlog"] == 1
    monkeypatch.undo()
    await a.send_bytes(VOICE)
    assert (await b.receive(timeout=2)).data == VOICE


async def test_garbage_frames_do_not_break_the_connection(relay):
    """검토 초점 2: 깨진 글자·빈 이진 프레임 뒤에도 연결과 상태가 멀쩡하다."""
    client, _, _ = relay
    a, b = await pair(client)
    await a.send_str("not json at all")
    await a.send_str('{"t":"dance"}')
    await a.send_bytes(b"")
    await a.send_json({"t": "ping", "n": 1})
    assert await rx(a) == {"t": "pong", "n": 1}
    await assert_silent(b)


# ── 연결 끊김·재접속·무소식 ─────────────────────────────────────────────────
async def test_disconnect_in_call_tells_peer_lost(relay):
    client, _, _ = relay
    a, b = await in_call(client)
    await b.close()
    assert await rx(a) == {"t": "ended", "why": "lost"}
    assert await rx(a) == {"t": "peer", "online": False}


async def test_same_id_reconnect_replaces_old_connection(relay):
    """검토 초점 1: 재접속하면 옛 연결은 닫히고, 새 연결은 바로 쓸 수 있다."""
    client, _, app = relay
    a, b = await pair(client)
    a2 = await connect(client, A)
    assert await rx(b) == {"t": "peer", "online": False}
    assert await rx(a2) == {"t": "peer", "online": True}
    assert await rx(b) == {"t": "peer", "online": True}
    assert (await a.receive(timeout=2)).type in (WSMsgType.CLOSE, WSMsgType.CLOSING, WSMsgType.CLOSED)
    await asyncio.sleep(0.05)                                 # 옛 연결의 뒤늦은 이탈 처리가 끝나도
    assert app[phone_relay.EXCHANGE].snapshot()["online"] == [A, B]   # 새 연결은 그대로
    await a2.send_json({"t": "call"})
    assert await rx(a2) == {"t": "ringing"}
    assert await rx(b) == {"t": "ring"}


async def test_silent_device_is_closed_after_45s(relay):
    client, clock, _ = relay
    a, b = await pair(client)
    clock.t += 40
    await b.send_json({"t": "ping", "n": 1})                  # B 만 소식을 보낸다
    assert await rx(b) == {"t": "pong", "n": 1}
    clock.t += 6                                              # A 는 46초째 무소식
    assert await rx(b) == {"t": "peer", "online": False}
    assert (await a.receive(timeout=2)).type in (WSMsgType.CLOSE, WSMsgType.CLOSING, WSMsgType.CLOSED)


# ── 기록에 비밀이 남지 않는다 ───────────────────────────────────────────────
async def test_logs_contain_no_token_and_no_voice(relay, caplog):
    """검토 초점 3: 토큰·음성 내용은 기록에 없다. 통화 길이·사유는 있다."""
    client, clock, _ = relay
    caplog.set_level(logging.DEBUG)
    with pytest.raises(WSServerHandshakeError):
        await client.ws_connect("/ws", headers=hdr(A, "wrong-token-SHOULD-NOT-APPEAR"))
    a, b = await in_call(client)
    await a.send_bytes(VOICE)
    await b.receive(timeout=2)
    clock.t += 12.0
    await a.send_json({"t": "hangup"})
    await rx(a), await rx(b)
    text = caplog.text
    for secret in (TOK[A], TOK[B], "wrong-token-SHOULD-NOT-APPEAR", "OPUSPAYLOAD"):
        assert secret not in text
    assert '"why":"hangup"' in text and '"talk_s":12.0' in text
    assert "auth_fail" in text


def test_load_env_file_sets_defaults_only(tmp_path, monkeypatch):
    f = tmp_path / ".env.phone"
    f.write_text("# 주석\nPORT=5094\nPHONE_TOKENS=x:1\n\n잘못된줄\n", encoding="utf-8")
    monkeypatch.setenv("PORT", "6000")
    monkeypatch.delenv("PHONE_TOKENS", raising=False)
    phone_relay.load_env_file(f)
    import os
    assert os.environ["PORT"] == "6000"                       # 이미 있는 값은 그대로
    assert os.environ["PHONE_TOKENS"] == "x:1"
    phone_relay.load_env_file(tmp_path / "없는파일")           # 없어도 오류 없음
```

- [x] **Step 2: 실패를 확인**

Run: `cd ~/tab5-toy-phone/server && python3 -m pytest tests/test_phone_relay.py -q`
Expected: 수집 단계에서 FAIL — `ModuleNotFoundError: No module named 'phone_relay'`

- [x] **Step 3: `server/phone_relay.py` 작성**

```python
#!/usr/bin/env python3
"""phone_relay.py — Tab5 전화 교환기 (aiohttp WebSocket 중계). 판단은 전부 phone_core.Exchange 가 한다.

설계: docs/exec-plans/2026-10-03-tab5-phone.md §9.3·§9.4
  GET /healthz   생존 확인 ({"ok": true} 만 — 접속·통화 상태는 내보내지 않는다)
  GET /ws        기기 접속. 헤더 X-Device-Id / X-Device-Token (주소창 매개변수는 읽지 않는다)
설정: server/.env.phone (git 제외) — PHONE_TOKENS=tab5-1:<토큰>,tab5-2:<토큰>  PORT=5094  BIND=127.0.0.1
실행: python3 server/phone_relay.py
기록: 시각·기기·통화 길이·종료 사유만. 음성과 토큰은 어디에도 쓰지 않는다.
"""
import asyncio
import json
import logging
import os
import time
from pathlib import Path

from aiohttp import WSMsgType, web

import phone_core

log = logging.getLogger("phone_relay")
WS_MAX_MSG_BYTES = 4096      # 이보다 큰 프레임을 보내는 연결은 aiohttp 가 닫는다(정상 음성은 400바이트 이하)
WS_CLOSE_TIMEOUT_S = 2.0     # 죽은 연결을 닫을 때 상대의 응답을 기다리는 상한

EXCHANGE = web.AppKey("exchange", phone_core.Exchange)
TOKENS = web.AppKey("tokens", dict)
CLOCK = web.AppKey("clock", object)
TICK_INTERVAL = web.AppKey("tick_interval", float)
BG_TASKS = web.AppKey("bg_tasks", set)


class Conn:
    """기기 연결 1개: WebSocket 과 그 아래 전송로(송신 대기열 크기를 보려고 함께 쥔다)."""
    __slots__ = ("ws", "transport")

    def __init__(self, ws, transport):
        self.ws, self.transport = ws, transport


def load_env_file(path):
    """KEY=VALUE 줄을 환경변수로 올린다(이미 있으면 그대로). 값은 출력하지 않는다."""
    p = Path(path)
    if not p.exists():
        return
    for line in p.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        os.environ.setdefault(k.strip(), v.strip())


def _spawn(app, coro):
    """기다리지 않고 실행(죽은 연결 닫기 등). 참조를 쥐고 있어야 도중에 수거되지 않는다."""
    task = asyncio.ensure_future(coro)
    app[BG_TASKS].add(task)
    task.add_done_callback(app[BG_TASKS].discard)


async def _safe_send(conn, text):
    try:
        await conn.ws.send_str(text)
    except (ConnectionError, RuntimeError):
        pass                                     # 닫히는 중인 연결 — 이탈 처리는 수신 루프가 한다


async def _safe_close(conn):
    try:
        await conn.ws.close()
    except Exception:                            # noqa: BLE001 — 이미 죽은 연결을 닫다 나는 오류는 버린다
        pass


async def apply_actions(app, actions):
    """phone_core 가 돌려준 할 일 목록을 실제 연결에 반영한다."""
    ex = app[EXCHANGE]
    for act in actions:
        kind = act[0]
        if kind == "send":
            conn = ex.conn_of(act[1])
            if conn is not None:
                await _safe_send(conn, json.dumps(act[2], separators=(",", ":")))
        elif kind == "close":
            _spawn(app, _safe_close(act[1]))
        elif kind == "log":
            log.info("%s", json.dumps(act[1], ensure_ascii=False, separators=(",", ":")))


async def healthz(request):
    return web.json_response({"ok": True})


async def ws_handler(request):
    app = request.app
    ex, now = app[EXCHANGE], app[CLOCK]
    dev = request.headers.get("X-Device-Id")
    token = request.headers.get("X-Device-Token")
    if not phone_core.check_token(app[TOKENS], dev, token):
        log.warning("auth_fail ip=%s", request.headers.get("CF-Connecting-IP") or request.remote)
        return web.Response(status=401, text="unauthorized")

    ws = web.WebSocketResponse(max_msg_size=WS_MAX_MSG_BYTES, timeout=WS_CLOSE_TIMEOUT_S)
    await ws.prepare(request)
    conn = Conn(ws, request.transport)
    await apply_actions(app, ex.attach(dev, conn, now()))
    try:
        async for msg in ws:
            if msg.type == WSMsgType.TEXT:
                await apply_actions(app, ex.on_text(dev, msg.data, now()))
            elif msg.type == WSMsgType.BINARY:
                peer = ex.route_voice(dev, len(msg.data), now())
                peer_conn = ex.conn_of(peer) if peer else None
                if peer_conn is None:
                    continue
                if peer_conn.transport is not None and phone_core.should_drop_voice(peer_conn.transport.get_write_buffer_size()):
                    ex.stats["voice_drop_backlog"] += 1       # PL-D3: 밀린 상대에겐 쌓지 않고 버린다
                    continue
                try:
                    await peer_conn.ws.send_bytes(msg.data)
                except (ConnectionError, RuntimeError):
                    pass
            elif msg.type == WSMsgType.ERROR:
                break
    finally:
        await apply_actions(app, ex.detach(dev, conn, now()))
    return ws


async def _ticker(app):
    ex, now = app[EXCHANGE], app[CLOCK]
    while True:
        await asyncio.sleep(app[TICK_INTERVAL])
        try:
            await apply_actions(app, ex.tick(now()))
        except Exception:                        # noqa: BLE001 — 시계 태스크가 죽으면 벨 제한·무소식 끊기가 멈춘다
            log.exception("tick failed")


async def _ticker_ctx(app):
    task = asyncio.ensure_future(_ticker(app))
    yield
    task.cancel()
    for t in list(app[BG_TASKS]):
        t.cancel()
    await asyncio.gather(task, *app[BG_TASKS], return_exceptions=True)


def create_app(tokens, clock=time.monotonic, tick_interval=1.0):
    app = web.Application()
    app[TOKENS] = dict(tokens)
    app[EXCHANGE] = phone_core.Exchange(tokens.keys())
    app[CLOCK] = clock
    app[TICK_INTERVAL] = tick_interval
    app[BG_TASKS] = set()
    app.router.add_get("/healthz", healthz)
    app.router.add_get("/ws", ws_handler)
    app.cleanup_ctx.append(_ticker_ctx)
    return app


def main():
    load_env_file(Path(__file__).resolve().parent / ".env.phone")
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(message)s")
    try:
        tokens = phone_core.parse_tokens(os.environ.get("PHONE_TOKENS", ""))
    except ValueError as e:
        raise SystemExit(f"[phone_relay] 설정 오류: {e}")
    host = os.environ.get("BIND", "127.0.0.1")   # PL-D4: 기본은 내부 전용. LAN 시험 때만 BIND=0.0.0.0
    port = int(os.environ.get("PORT", "5094"))
    log.info("listening http://%s:%d devices=%s", host, port, ",".join(sorted(tokens)))
    web.run_app(create_app(tokens), host=host, port=port, access_log=None, print=None)


if __name__ == "__main__":
    main()
```

- [x] **Step 4: 전체 통과 확인**

Run: `cd ~/tab5-toy-phone/server && python3 -m pytest -q`
Expected: `76 passed` (순수 로직 54 + 중계 22)

- [x] **Step 5: 흔들리는 시험이 없는지 10회 반복(경고도 오류로)**

Run:
```bash
cd ~/tab5-toy-phone/server && fails=0; for i in 1 2 3 4 5 6 7 8 9 10; do python3 -m pytest -q -W error 2>&1 | tail -1 | grep -q "76 passed" || fails=$((fails+1)); done; echo "실패 $fails / 10"
```
Expected: `실패 0 / 10`

- [x] **Step 6: 커밋**

```bash
cd ~/tab5-toy-phone && git add server/phone_relay.py server/tests/test_phone_relay.py && git commit -m "feat(phone): 교환기 WebSocket 중계 phone_relay(헤더 인증·음성 건네기·밀린 음성 폐기)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 11: 교환기 실제 기동 확인, 취약점 점검, 독립 검수, 문서

**Files:**
- Create: `server/tools/phone_smoke.py`
- Create: `server/.env.phone` (git 제외, 맥북 시험용 토큰)
- Modify: `docs/exec-plans/2026-10-03-tab5-phone.md` — §5 Phase 1 체크, §8 진행 로그, §9.4 에 PL-D1·D2 의미 추가

**Interfaces:**
- Consumes: Task 9~10, Task 1 의 무시 규칙.
- Produces: `python3 server/tools/phone_smoke.py [기본주소]` — 떠 있는 교환기에 가짜 기기 2대로 걸기→받기→음성 50프레임→끊기. 성공 시 `SMOKE OK … 중앙값 x ms`, 실패 시 `SMOKE FAIL: …` 과 종료 코드 1. Phase 4 에서 터널 경유 확인·지연 측정에 그대로 쓴다.

- [x] **Step 1: `server/tools/phone_smoke.py` 작성**

```python
#!/usr/bin/env python3
"""phone_smoke.py — 떠 있는 교환기에 가짜 기기 2대로 붙어 걸기→받기→음성 50프레임→끊기를 실제 소켓으로 확인한다.

  python3 tools/phone_smoke.py                       # http://127.0.0.1:5094 (맥북에서 띄운 교환기)
  python3 tools/phone_smoke.py https://phone.example.com   # Phase 4: 터널을 거친 실제 경로(왕복 지연 측정 겸용)
토큰은 server/.env.phone 에서 읽는다(출력하지 않음). 교환기와 같은 기기에서 실행한다.
"""
import asyncio
import os
import statistics
import sys
import time
from pathlib import Path

import aiohttp

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import phone_core
import phone_relay


async def expect(ws, want):
    got = await ws.receive_json(timeout=5)
    if got != want:
        raise AssertionError(f"기대 {want} / 실제 {got}")


async def run(base):
    phone_relay.load_env_file(Path(__file__).resolve().parent.parent / ".env.phone")
    tokens = phone_core.parse_tokens(os.environ.get("PHONE_TOKENS", ""))
    (a_id, a_tok), (b_id, b_tok) = sorted(tokens.items())
    url = base.rstrip("/") + "/ws"
    async with aiohttp.ClientSession() as s:
        async with s.get(base.rstrip("/") + "/healthz") as r:
            assert r.status == 200, f"healthz {r.status}"
        async with s.ws_connect(url, headers={"X-Device-Id": a_id, "X-Device-Token": a_tok}) as a:
            await expect(a, {"t": "peer", "online": False})
            async with s.ws_connect(url, headers={"X-Device-Id": b_id, "X-Device-Token": b_tok}) as b:
                await expect(b, {"t": "peer", "online": True})
                await expect(a, {"t": "peer", "online": True})
                await a.send_json({"t": "call"})
                await expect(a, {"t": "ringing"})
                await expect(b, {"t": "ring"})
                await b.send_json({"t": "accept"})
                await expect(a, {"t": "connected"})
                await expect(b, {"t": "connected"})
                rtts = []
                for seq in range(50):                                  # 20ms 간격 음성 흉내: A→B 로 보내고 B 가 되돌려 왕복 시간 측정
                    frame = seq.to_bytes(2, "big") + bytes(60)
                    t0 = time.perf_counter()
                    await a.send_bytes(frame)
                    m = await b.receive(timeout=5)
                    assert m.type == aiohttp.WSMsgType.BINARY and m.data == frame, "음성 프레임이 바뀌었거나 오지 않음"
                    await b.send_bytes(m.data)
                    m2 = await a.receive(timeout=5)
                    assert m2.data == frame
                    rtts.append((time.perf_counter() - t0) * 1000)
                    await asyncio.sleep(0.02)
                await a.send_json({"t": "hangup"})
                await expect(a, {"t": "ended", "why": "hangup"})
                await expect(b, {"t": "ended", "why": "hangup"})
    rtts.sort()
    print(f"SMOKE OK  음성 왕복(A→교환기→B→교환기→A) 중앙값 {statistics.median(rtts):.1f}ms, 최대 {rtts[-1]:.1f}ms")


if __name__ == "__main__":
    try:
        asyncio.run(run(sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:5094"))
    except (AssertionError, ValueError, aiohttp.ClientError, asyncio.TimeoutError) as e:
        sys.exit(f"SMOKE FAIL: {type(e).__name__}: {e}")
```

- [x] **Step 2: 맥북용 `.env.phone` 을 만든다(값을 출력하지 않는다)**

```bash
cd ~/tab5-toy-phone && git check-ignore -q server/.env.phone && cd server && python3 - <<'EOF'
import os, secrets
p = ".env.phone"
if os.path.exists(p):
    print("이미 있음 — 그대로 둠")
else:
    fd = os.open(p, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    os.write(fd, f"PHONE_TOKENS=tab5-1:{secrets.token_hex(16)},tab5-2:{secrets.token_hex(16)}\nPORT=5094\n".encode())
    os.close(fd)
    print("만듦: server/.env.phone (권한 600, 값은 출력하지 않음)")
EOF
git -C ~/tab5-toy-phone status --short | grep -c "env.phone"
```
Expected: `만듦: …` (또는 `이미 있음`) 다음 줄 `0`. `git check-ignore` 가 실패하면(무시 규칙 없음) 파일을 만들지 않는다 — Task 1 로 돌아간다.
(이 토큰은 맥북 시험용이다. 기기 `secrets.h` 와 맥미니 `.env.phone` 에 넣을 실제 토큰은 Phase 2·4 에서 따로 만든다.)

- [x] **Step 3: 포트가 비어 있는지 보고 교환기를 띄워 확인**

Run:
```bash
cd ~/tab5-toy-phone/server && lsof -nP -iTCP:5094 -sTCP:LISTEN ; python3 phone_relay.py 2> /tmp/phone_relay_task11.log &
sleep 2
curl -s -w "  → HTTP %{http_code}\n" http://127.0.0.1:5094/healthz
curl -s -o /dev/null -w "토큰 없이 /ws → HTTP %{http_code}\n" -H "Connection: Upgrade" -H "Upgrade: websocket" -H "Sec-WebSocket-Version: 13" -H "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==" http://127.0.0.1:5094/ws
lsof -nP -iTCP:5094 -sTCP:LISTEN | awk 'NR>1{print "수신 주소:", $9}'
python3 tools/phone_smoke.py
kill %1 ; wait 2>/dev/null
cat /tmp/phone_relay_task11.log ; rm -f /tmp/phone_relay_task11.log
```
Expected:
```
{"ok": true}  → HTTP 200
토큰 없이 /ws → HTTP 401
수신 주소: 127.0.0.1:5094
SMOKE OK  음성 왕복(A→교환기→B→교환기→A) 중앙값 0.xms, 최대 x.xms
… phone_relay listening http://127.0.0.1:5094 devices=tab5-1,tab5-2
… phone_relay auth_fail ip=127.0.0.1
… phone_relay {"ev":"online","dev":"tab5-1"}
… phone_relay {"ev":"online","dev":"tab5-2"}
… phone_relay {"ev":"call_end","caller":"tab5-1","callee":"tab5-2","why":"hangup","ring_s":0.0,"talk_s":1.x}
… phone_relay {"ev":"offline","dev":"tab5-2"}
… phone_relay {"ev":"offline","dev":"tab5-1"}
```
첫 `lsof` 가 뭔가를 출력하면(5094 가 이미 쓰이는 중) 멈추고 무엇이 쓰는지 보고한다. 수신 주소가 `*:5094` 나 `0.0.0.0:5094` 면 실패다(기본값은 내부 전용이어야 한다). 로그 파일은 저장소 밖(`/tmp`)에 두고 바로 지운다.

- [x] **Step 4: 음성·토큰이 디스크에 쓰이지 않는지 코드 검사(설계서 §6)**

Run:
```bash
cd ~/tab5-toy-phone/server && grep -nE "open\(|\.write\(|write_text|write_bytes|logging\.FileHandler|print\(" phone_core.py phone_relay.py
```
Expected: `phone_relay.py` 의 `load_env_file` 안 `p.read_text(` 는 이 패턴에 걸리지 않으므로 **출력이 없어야 한다**(파일 쓰기·print 가 한 줄도 없음). 한 줄이라도 나오면 그 줄이 음성·토큰을 다루지 않는지 확인하고 보고에 적는다.

- [x] **Step 5: 의존성 취약점 점검**

Run:
```bash
python3 -m pip install -q pip-audit && cd ~/tab5-toy-phone/server && python3 -m pip_audit -r requirements-phone.txt
```
Expected: `No known vulnerabilities found`. 취약점이 나오면 표의 "Fix Versions" 로 `requirements-phone.txt` 의 범위를 올리고 `python3 -m pytest -q` 를 다시 통과시킨다. (`pip-audit` 는 PyPA 가 관리하는 공식 도구다.)

- [x] **Step 6: 독립 검수(생성자와 평가자 분리)**

`code-reviewer` 에이전트에게 맡긴다. 건넬 것: 파일 `server/phone_core.py`, `server/phone_relay.py`, `server/tests/test_phone_core.py`, `server/tests/test_phone_relay.py`, `server/tools/phone_smoke.py` 와 설계서 §9.3·§9.4·§11, 이 계획의 "Review Focus" 5줄과 PL-D1~D5. 요청: "설계서 대비 빠진 동작, 인증·기록의 보안 구멍, 동시성 문제(재접속·끊김 순서), 시험이 실제로는 아무것도 확인하지 않는 곳을 찾아 100점 만점으로 채점." 결론을 미리 주지 않는다.
지적은 `superpowers:receiving-code-review` 대로 하나씩 사실 확인 후 반영하고, 고칠 때마다 `python3 -m pytest -q` 를 다시 돌린다. 95점 미만이면 고친 뒤 재검수.

- [x] **Step 7: 설계서 갱신**

`docs/exec-plans/2026-10-03-tab5-phone.md`:
- §5 `- [ ] **Phase 1 — 교환기**` → `- [x]`
- §9.4 표 아래에 추가:
  ```markdown
  - (2026-10-__ 구현 확정, 계획 PL-D1·D2) `ended` 는 통화에 참여한 **양쪽 모두**에게 간다(끊은 쪽 포함). 벨 울리는 중 한쪽이 끊기면 남은 쪽은 `ended: offline`, 통화 중이면 `ended: lost` 를 받는다. `hangup` 은 벨 중에도 유효하다(거는 쪽의 취소). 상태에 안 맞는 신호와 깨진 글자는 응답 없이 무시된다.
  - (PL-D3) 교환기는 상대 쪽 송신 대기열이 4096바이트를 넘으면 새 음성 프레임을 버린다.
  ```
- §9.3 `phone_relay.py` 줄에 추가: "`BIND` 환경변수(기본 `127.0.0.1`)로 수신 주소를 바꿀 수 있다 — LAN 시험 전용(PL-D4). `/healthz` 는 `{"ok": true}` 만 돌려준다(PL-D5)."
- §8 진행 로그에 추가:
  ```markdown
  ### 2026-10-__ — Phase 1 교환기 완료
  - `server/phone_core.py`(순수 로직) + `server/phone_relay.py`(aiohttp) + 시험 76개 통과(10회 반복 흔들림 없음), 실제 기동·가짜 기기 2대 통화 확인(`tools/phone_smoke.py`), pip-audit …, 독립 검수 …점.
  - 맥북에서만 실행. 맥미니 배포·터널·Bot Dashboard 는 Phase 4.
  ```

- [x] **Step 8: 커밋·푸시**

```bash
cd ~/tab5-toy-phone && git add server/tools/phone_smoke.py docs/exec-plans && git commit -m "feat(phone): 교환기 실기동 확인 도구 + Phase 1 완료 기록" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" ; git push
git status --short | grep -cE "env.phone|pytest_cache|__pycache__"
```
Expected: 마지막 줄 `0`

- [x] **Step 9: 완료 보고**

`superpowers:verification-before-completion` 으로 위 Expected 를 실제 출력과 대조한 뒤, 큰 작업 보고 형식으로 사용자에게 알린다. [리스크] 에 반드시 적을 것: "교환기는 가짜 기기로만 시험했다 — 실제 Tab5(ESP WebSocket 클라이언트)와의 호환, Cloudflare 터널을 거친 동작은 Phase 2·4 에서 확인한다."

---

## 검증 기준 (이 계획의 완료 조건)

- [x] H1~H4 네 가정 모두 측정값이 설계서 §8 에 기록되고, 사용자에게 게이트 보고가 갔다.
- [x] 2번 기기가 스냅부스로 복구되어 부팅 로그(`snapbooth: wifi known`)가 확인됐다. 1번 기기는 건드리지 않았다.
- [x] `cd server && python3 -m pytest -q` → 87 passed(검수 반영으로 76 → 87), 10회 반복 실패 0.
- [x] `cd firmware/tab5_phone_probe/tools && python3 -m pytest -q` → 26 passed.
- [x] 교환기가 `127.0.0.1:5094` 에서만 열리고, 토큰 없는 접속은 401, 기록에 토큰·음성이 없다.
- [x] `git status` 에 `.env.phone`·`secrets.h`·`build/`·`out/`·`test_voice.pcm`·`.pytest_cache` 가 없다.
- [x] 교환기 코드 독립 검수 95점 이상. (84 → 93 → 96점)

## 진행 로그

### 2026-10-03 — 계획 작성
- 설계서·스냅부스 펌웨어·M5Unified 0.2.23 소스·공개 선례(xiaozhi Tab5 오디오 설정)·라이브러리 원본(esp_codec_dev 1.6.2, esp-sr 2.5.5, esp_audio_codec 2.5.0/2.6.2)을 읽고 작성.
- 발견 1: `esp_audio_codec` 2.6+ 는 P4 rev 3.0 이상에서만 빌드되게 막혀 있음 → 2.5.0 으로 고정(설계서 §4 정정 대상).
- 발견 2: 코덱 드라이버(es8388/es7210)는 "이미 열린" 제어 인터페이스와 `get_info` 구현을 요구함 → `board_audio.cpp` 의 M5.In_I2C 어댑터에 반영.
- 발견 3: M5Unified 는 새 I2S 드라이버(i2s_std)를 쓰므로 esp_codec_dev 와 드라이버 충돌 없음. 단 `cfg.internal_spk/mic = false` 로 I2S0 선점을 막아야 함.
- 발견 4: 맥미니 스냅부스 서버(:5085)가 꺼져 있어 `snapbooth.example.com/healthz` 가 502(터널은 살아 있음) — H3 측정은 가능, Task 6 Step 1 에 처리 방법 기재.
- 발견 5: 작성 시점에 맥북 USB 에는 1번 기기만 보임(2번 미검출) — Task 1 Step 4 에서 확인.
- 검증: 임시 폴더에서 교환기 시험 76개·측정 도구 시험 26개 통과, 시험 펌웨어 빌드 성공(기본 / `AEC_MODE_FD_HIGH_PERF` / `PROBE_TX_TDM=1`). 기기 미탑재.

### 2026-10-03 — 실행 1차 (맥북 세션, 기기 없이 되는 과제)
- Task 1: 무시 규칙 커밋(72ce2a5)·맥미니 반영 확인, 2번 기기 USB 확인(`/dev/cu.usbmodem101`, 1번은 `/dev/cu.usbmodem2101` — 포트 이름은 바뀔 수 있으니 항상 `probe_ctl.py port 2`), 복구용 파일 확인.
- Task 2: 측정 도구 시험 26개 통과(시험 먼저 → 실패 확인 → 구현). 커밋 11e5a19.
- Task 3: 시험 펌웨어 저장소 안에서 빌드 성공(0x244cc0 바이트, 62% 여유, `esp32p4_less_v3`, 우리 코드 경고 0). 부품 판은 Step 14 기대값과 전부 일치. `build/` 가 남아 있어 Task 4 는 바로 플래시 가능.
- Task 9·10: 교환기 시험 76개 통과, 10회 반복 실패 0. 커밋 63b7bcd, 557ad50.
- Task 11 Step 1~5: 실제 기동(`127.0.0.1:5094`)·401·가짜 기기 2대 통화(`SMOKE OK`, 중앙값 0.8ms)·로그 속 토큰 0건·파일 쓰기 검사 깨끗·pip-audit 취약점 없음.

### 2026-10-03 — Task 11 독립 검수 반영
- 1차 검수 84점(Important 4, Minor 5). 시험 먼저 → 수정 순서로 반영, 교환기 시험 76 → 85개(10회 반복 실패 0). 커밋 806f13d.
- 고친 것: 깨진 UTF-8 토큰 헤더에 500 대신 401 / 재접속으로 밀려난 옛 연결에서 온 프레임 무시 / 접속 직후 전송 실패 시 유령 연결 방지 / WebSocket 압축 협상 끔 / `.env` 따옴표·`PORT` 오류 처리 / 통계 이름 `voice_drop_big` → `voice_drop_size` / `phone_smoke.py` 경고.
- **이 문서 Task 9~11 의 서버 코드 블록은 검수 전 판이다. 정본은 저장소의 `server/` 파일.**
- 코드로 고치지 않고 판단한 것(진행 기록의 Ruling): PL-D3 의 한계(위 표에 추가), 끊자마자 다시 걸 때의 `ended`→`ringing` 순서는 펌웨어 `call_fsm` 요구사항으로 넘김.
- 2차 검수 93점 → 주소창 토큰 시험 보강, 인증 실패 기록의 접속 주소 정제, 설계서 §9.4 계약 명문화 등 반영(커밋 b3e1a2a, 시험 87개) → **재채점 96점**. Task 11 완료.
- 남은 과제: Task 4~8(2번 기기 시험·복구·게이트 보고). `firmware/tab5_phone_probe/build/` 가 있어 Task 4 Step 1 부터 바로 진행 가능.


### 2026-10-03 — 실행 2차 (새 세션, Task 4~8 — 2번 기기 시험)
- Task 4: 플래시·소리 장치 초기화 정상. 칸 배치는 가정대로(마이크 칸0, 기준 칸1) — 코드 수정 없음. 터치·스피커는 사용자 확인("소리 적당함", 터치 기록 39회).
- Task 5: 결함 둘을 찾아 고침. ① 녹음 덤프 중 감시 경고(task_wdt)가 데이터 줄에 끼어들어 전송이 깨짐 → `dump_pcm` 줄마다 양보(59ac326). ② **마이크 증폭이 0dB 로 남음** → 참고 구현(xiaozhi Tab5)과 대조해 open 뒤 30dB 재설정(28cb3da). ② 때문에 처음 잰 H2 수치는 무효, 다시 잼: 음량 60/80/100 = 18.5/29.4/35.9dB. H4 는 Opus 복잡도 5 가 53.6% 로 초과 → 0 으로 낮춰 29.9%.
- 계획과 다르게 한 것: H2 음량을 40/60/80 → 60/80/100(사용자가 최대 음량을 "적당"이라 평가) + 마이크 24dB·음량 100 추가. 에코 제거 방식 비교(Step 5)는 음량 80·100 만 — FD 방식은 출력이 상수로 깨져 비교 불가. 귀 판정은 음량 80 겹쳐 말하기 녹음 한 쌍으로 두 질문을 함께. 30분 가동은 사용자 확인 단계와 순서를 바꿔 진행(한 번 중단 후 최종 설정으로 재실행).
- Task 6: 왕복 중앙값 92.6ms(소리 동작 중 86.1ms), status 502(맥미니 스냅부스 서버 꺼짐).
- Task 7: 30분 가동 SOAK PASS(오류·재부팅 0). 종료 무렵 터치는 미확인, 사용자 관찰 3문항은 답 없음.
- 기록 독립 검수 82점: 수치는 원자료와 일치, 판정 표현의 범위 과장 지적을 설계서 §4·§8 에 반영.
- Task 8: 2번 기기 스냅부스 복구 확인, 설계서 §4·§5·§8 갱신, 게이트 보고.
- 이 문서 Task 3 의 펌웨어 코드 블록은 수정 전 판이다. 정본은 저장소의 `firmware/tab5_phone_probe/` 파일.
