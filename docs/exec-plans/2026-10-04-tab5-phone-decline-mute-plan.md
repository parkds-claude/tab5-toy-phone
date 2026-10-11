# Tab5 음성 전화기 — 수신거부 메시지·벨 끄기 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> (실행하는 에이전트에게: 위 두 스킬 중 하나로 과제를 하나씩 수행하고, 단계는 체크박스로 추적한다.)

- **작성일**: 2026-10-04
- **상태**: 구현·기기 검증 완료(2026-10-04, 커밋 `113097c` 빌드 기준) — 그 뒤 최종 검수의 수정 빌드 `066cf57` 도 두 기기에서 같은 시험을 다시 통과했고(16:12~16:23) 수정 뒤 재검수는 96/100(게이트 통과)이다. 사용자 손 조작도 20:54~21:04 에 했다("좋아. 모두 좋아." — 벨 끄기 등 일부는 손으로 미확인). 결과는 설계서 `2026-10-04-tab5-phone-decline-mute.md` §12 와 이 문서 진행 로그. (이전 상태: 사용자 검토 대기)
- **작성자**: Claude (맥북 세션)
- **넣는 자리**: Phase 2 계획(`docs/exec-plans/2026-10-04-tab5-phone-phase2.md`)의 Task 8 뒤·Task 9 앞. 이 계획이 끝나면 Task 9(손 조작·독립 검수)가 새 기능까지 한 번에 본다.

**Goal:** 전화가 왔을 때 받는 쪽이 ① **벨 끄기**로 그 전화의 벨만 멈추고 ② **메시지로 거절** → 문구 1·2·3 중 하나를 눌러 거절하면 거는 쪽 화면에 "상대: <문구>"가 10초 뜨게 하며, 번호 없는 거절과 그 밖의 통화 흐름은 지금과 똑같이 동작하게 한다(기존 시험을 고치지 않고 전부 통과).

**Architecture:** 판단은 지금처럼 하드웨어와 무관한 헤더(`call_fsm.h`·`relay_proto.h`·`ui_layout.h`)와 교환기의 순수 로직(`phone_core.py`)에 두고 맥에서 시험을 먼저 쓴다. 문구는 번호(1~3)로만 오간다 — 교환기는 `reject` 의 `msg` 를 `ended` 에 실어 건네기만 한다. 벨 꺼짐은 상태 기계가, 메시지 고르기 화면의 열림은 화면 쪽(`DeclinePicker`)이 기억하고, `ui.cpp`·`app.cpp` 는 그것을 그리고 잇기만 한다.

**Tech Stack:** ESP-IDF 5.5.4 (esp32p4) · M5Unified 0.2.23 / M5GFX 0.2.30 · esp_hosted 2.11.7 · esp_websocket_client 1.8.0 · Python 3.11 + aiohttp 3.14 + pytest 9 + pytest-asyncio 1.3 + Pillow 10.2 + pyserial · clang++ (맥 단위 시험)

**Spec:** `docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md` (승인 2026-10-04) — 실행자는 설계서와 이 계획을 함께 읽는다. 배경·화면 배치의 이유·결정 기록(DM-D1~D14)·오류 사례는 설계서에 있고 여기서는 되풀이하지 않는다.

## 한눈에 보기 (사용자용)

| 과제 | 하는 일 | 기기 | 사용자가 할 일 | 걸리는 시간(어림) |
|---|---|---|---|---|
| Task 1 | 교환기가 거절 신호에 실린 문구 번호를 상대에게 건네게 한다(시험 먼저) | 없음 | 없음 | 5분 |
| Task 2 | 전화기의 판단 로직 세 군데(통화 상태·신호 읽기·화면 배치)에 벨 끄기·메시지 거절을 넣는다(시험 먼저) | 없음 | 없음 | 10분 |
| Task 3 | 맥에서 상대 전화기를 흉내 내는 도구가 문구 번호를 다루게 한다 | 없음 | 없음 | 5분 |
| Task 4 | 새 글자 11개("벨 끄기", 문구 셋 등)를 Pretendard 로 굽는다 | 없음 | 없음 | 5분 |
| Task 5 | 화면 그리기·진행 코드를 고치고 펌웨어를 빌드한다 | 없음(빌드만) | 없음 | 10분 |
| Task 6 | 교환기를 새 판으로 다시 띄운다 → **2번에 먼저** 올려 옛 판과 섞여도 통화가 되는지 본다 → **1번에도** 올려 벨 끄기·메시지 거절을 자동으로 시험하고, 예전 시험(걸기·받기·거절·취소·무응답)을 다시 돌리고, 화면 사진을 찍는다 | **1번·2번**(각 5분 안팎 다시 올림) | 시험하는 약 45분 동안 **두 기기 화면을 만지지 않기**. 끝나면 화면 사진 6장을 보고 모양·색 의견 주기 | 45분 |
| Task 7 | 결과를 설계서에 기록하고, 앞 계획의 문서 과제(Task 10)로 넘길 목록을 정리한다 | 없음 | 없음 | 5분 |

- **소리는 아직 없다.** 벨 끄기는 버튼·화면·내부 동작까지 만든다. 실제 소리가 꺼지는지는 Phase 3(소리)에서 확인한다(설계서 §7.4).
- **직접 눌러 보기는 이 계획 다음**이다 — Phase 2 계획 Task 9 에서 새 버튼까지 한 번에 눌러 본다(설계서 §7.3).
- 두 기기는 시험 뒤에도 전화기로 남는다. 되돌리는 법은 아래 "롤백"(각 5분).
- **실행자에게 — 코드를 옮겨 적지 않는다.** 이 계획에 실린 패치와 도구는 맥북의 `~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-decline-mute-plan/draft/`(이하 `D`, git 제외, 맥북에만 있음, **읽기 전용**)에 그대로 있다. 계획서의 코드 블록과 글자 단위로 같다(조립 스크립트가 끼워 넣었다 — 목록과 검사합은 문서 끝 "초안 목록").
  - 기존 파일의 수정은 `D/patches/TN-MM-….patch` 를 저장소 루트에서 `git apply` 로 적용하고, 적용한 뒤의 파일을 `D/files/<저장소 경로>` 와 `cmp` 로 대조한다(출력 없음 = 같음).
  - **순서를 지킨다**: 시험 패치 → 실패 확인 → 구현 패치 → 통과 확인 → 커밋. 한꺼번에 적용하지 않는다.
  - 적용한 파일을 고치지 않는다. Expected 와 다르면 스스로 고치지 말고 멈춰서 보고한다(실제 출력 첨부).
  - `D/files/…/main/ui_assets.h.sha256` 은 검사합뿐이다 — `ui_assets.h` 는 Task 4 에서 생성기를 돌려 만든다. `D/verify/` 의 도구는 저장소에 넣지 않고 그 자리에서 실행한다.
  - 그 폴더가 없거나 내용이 다르면 계획서의 코드 블록이 정본이다(블록을 그대로 파일로 옮겨 `git apply --check` 부터).
- 이 계획에 실린 코드는 작성 시점(2026-10-04, 저장소 `10f1934`)에 저장소 밖 사본에서 실제로 돌려 확인했다: 깨끗한 사본에 패치 14개를 순서대로 적용하며 실패 → 통과를 확인(맥 단위 시험 73·46·59 → 105·61·86, 교환기 pytest 87 → 137, 검증 도구 150 유지), 글자 자산 86개 재생성(기존 자산 그대로), 펌웨어 전체 빌드 성공(0x2312b0, 앱 파티션의 63% 여유, 우리 코드 경고 0). **기기에 올려 본 적은 없다** — `ui.cpp`·`app.cpp` 의 동작은 Task 6 에서 처음 확인된다.

## Global Constraints

설계서와 Phase 2 계획에서 온 전 과제 공통 요구사항. 모든 과제는 이 절을 암묵적으로 포함한다.

- **기기 번호**: 1번 = MAC `AA:BB:CC:DD:EE:01`(교환기 ID `tab5-1`), 2번 = MAC `AA:BB:CC:DD:EE:02`(`tab5-2`).
- **기기는 번호(MAC)로 고른다**: 포트는 항상 `python3 tools/phone_ctl.py port <1|2>` 로 얻고, 플래시 직전에 `read_mac` 으로 다시 확인한다. `/dev/cu.usbmodem*` 를 직접 고르지 않는다.
- **코드 과제(Task 1~5)는 기기·시리얼 포트·교환기를 건드리지 않는다.** 기기는 Task 6 에서만.
- **칩·빌드**: ESP-IDF 5.5.4, 대상 `esp32p4`, 두 기기 모두 P4 rev v1.3(`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`, `CONFIG_ESP32P4_REV_MIN_100=y` — 그대로 둔다). **`esp_hosted` 2.11.7 고정**, Wi-Fi 칩(C6) 펌웨어 교체 금지, `amcchord/M5Tab5-VideoLink` 플래시 금지, `esp_audio_codec` 2.5.0. `sdkconfig.defaults`·`main/idf_component.yml`·`main/CMakeLists.txt` 를 고치지 않는다.
- **화면**: 가로 1280×720, `setRotation(3)`. 터치 대상 96px 이상, 8px 격자. 색은 `docs/design/README.md` §1 토큰(`gfx.h` 의 `C_*`)만, 고정 문구는 Pretendard 로 미리 구운 자산만.
- **범위**: 설계서 §2·§3 그대로. `relay_link.*`·`phone_relay.py`·`phone_main.cpp`·`gfx.*`·`ui_wifi.*`·`net_wifi.*`·`server/tools/phone_relay_dev.sh` 를 고치지 않는다. `phone_ctl.py` 는 머리 주석 한 줄만. **기존 시험은 한 글자도 고치지 않는다**(시험 패치는 추가만).
- **비밀**: `secrets.h`·`server/.env.phone` 을 `cat`/Read 로 열지 않는다. 그 내용(Wi-Fi 비밀번호, 토큰, 교환기 주소)을 화면·로그·보고·커밋에 내지 않는다.
- **이 맥의 함정 세 가지**: ① 맥 단위 시험은 `clang++ … -isysroot "$(xcrun --sdk macosx --show-sdk-path)"` 가 있어야 링크된다. ② `source ~/esp/esp-idf-5.5.4/export.sh` 를 한 셸의 `python3` 에는 Pillow·pytest 가 없다 → 파이썬 도구·시험은 export 하지 **않은** 호출에서, 빌드·esptool 은 export 와 **같은 호출 안에서**(Bash 도구는 호출마다 새 셸이다). ③ 명령을 `sh -c '…'` 로 감싸지 않는다.
- **main 에서 직접 작업**(브랜치·worktree 없음). devsync 가 5분마다 `git add -A` 로 자동 커밋한다 → `git commit` 이 "nothing to commit" 이면 devsync 가 먼저 커밋한 것이라 정상이다(보고에 적는다). `git add` 는 그 과제의 파일만(`git add -A` 금지). 임시 파일은 저장소 밖(`/tmp`)이나 git 제외 폴더 `firmware/tab5_phone/out/` 에만 둔다.
- **교환기는 전용 스크립트로만 켜고 끈다**: `bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh start|stop|restart|start-wrong-token|status` — 정확히 이 모양(`bash` + 절대 경로 + 낱말 하나), 한 번의 호출에 이 명령 하나만. `phone_relay.py` 를 직접 실행하지 않는다. 스크립트를 고치지 않는다(이 맥의 안전 검사가 다른 방법을 막고, 사용자가 이 스크립트 하나만 허용했다). `start`·`restart` 는 로그 `/tmp/phone_relay_dev.log` 를 덮어쓴다 — 필요한 줄은 그 전에 `out/` 으로 복사한다.
- **두 기기에는 같은 바이너리를 올린다**: 빌드는 Task 5 에서 한 번만 하고, 플래시는 `idf.py flash`(HEAD 가 움직이면 다시 빌드해 바이너리가 달라진다) 대신 빌드 폴더에서 `python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash @flash_args` 를 쓴다(직전에 `read_mac` 으로 대상 확인). 부팅 로그의 `App version`·`Compile time` 이 두 기기에서 같은지 기록한다.
- **멈추라는 조건에 걸리면 멈춘다**: BLOCKED 로 보고하고 기다린다. 원인을 찾았고 더 나은 방법이 떠올라도 실행하지 말고 제안으로만 적는다. 특히 기기에 쓰는 일·교환기 재시작 앞의 정지 조건은 실행 담당이 스스로 풀 수 없다.
- **사람이 자판을 쓰는 중일 수 있는 화면(`screen=WIFI_PASS`)에서는 스크린샷을 찍지 않는다** — `shot` 앞에는 항상 `until:stat` 으로 기대하는 화면인지 확인한다(아래 명령에 이미 들어 있다).
- **터치 대조**: 기기 시험의 각 실행은 `both:until:stat` 으로 시작하고 끝난다. 끝나면 `python3 ~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase2/touch_audit.py <로그>` 로 주입한 탭 수와 `touch=` 증가를 대조한다(사용자가 화면을 만진 일이 있었다).
- 이 계획에는 통화 중에 교환기를 끄는 시험이 없다. 결함 조사로 다시 돌리게 되면 기대 패턴은 순서 무관으로 쓴다: `^FSM IN_CALL -> (OFFLINE|IDLE) acts=0x240 notice=LOST`.
- **참고 구현 대조 우선**: 기기에서 이상한 동작이 나오면 추측으로 고치지 않는다(Task 6 "결함이 나오면").
- **진행 안내·보고는 전부 한국어.**

## Review Focus

설계서(§5.8·§10)가 암시하지만 과제의 시험이 놓치기 쉬운 입력·고장 다섯 가지 — 일어나기 쉬운 것부터. 각 줄의 시험은 해당 과제에 들어 있다.

1. **메시지를 고르는 중에 통화가 끝난다**(상대가 취소, 30초 경과, 연결 끊김) — 고르기 화면이 닫혀야 하고, 그 순간 누른 문구·뒤로가 대기 화면의 통화 버튼을 눌러 되걸면 안 되며, 한 바퀴 안에 `ended` → `ring` 이 와도 다음 전화가 고르기 화면으로 뜨면 안 된다. → Task 2 `test_ui_layout.cpp` "(2) message buttons overlap no button of any call screen…"·"(2) mute/open-picker/back do not overlap the call button…"·"picker: closes when the call leaves INCOMING"·"picker: ended then ring -> the next call starts on the bell screen", Task 5 Step 2(`feed()` 가 사건마다 `sync`), Task 6 시나리오 F·G
2. **눌러도 화면이 안 바뀌거나, 끈 벨이 다음 전화에 남는다**(`same()` 에 새 칸 비교 누락, `bell_muted` 를 안 지움) → Task 2 `test_call_fsm.cpp` "next call rings again"·"ended then ring in one batch: the new call is not muted"·"muted bell stuck 40s: reconnect, missed, flag cleared"·"muted call + link down", Task 5 Step 2(`same()` 세 칸), Task 6 시나리오 A·B
3. **틀린 문구 번호**(0, 4~99, 100, 음수, 소수, 글자, 참·거짓, null, 2^32+1, 거절이 아닌 종료에 붙은 번호) — 거절은 되고 번호만 빠져야 한다. 통화가 안 끝나거나 엉뚱한 문구가 뜨면 안 된다. → Task 1 `test_reject_with_wrong_msg_is_a_plain_reject`·`test_msg_on_other_signals_is_not_forwarded`, Task 2 `test_relay_proto.cpp` "wrong msg = plain reject, call still ends"·"msg on a non-reject ended is dropped", `test_call_fsm.cpp` "caller: reject with an unknown number = plain REJECTED"·"reject with an out-of-range number still rejects, without a number"
4. **한 번 누른 터치가 두 번 잡히거나, 누른 순간 신호가 엇갈린다** — "메시지로 거절"을 누른 터치가 문구까지 누르거나, "뒤로"가 받기·거절을 누르거나, 문구를 누른 순간 상대가 취소한다. → Task 2 `test_ui_layout.cpp` "(1) open-picker overlaps no picker button…"·"(3) back overlaps no accept/reject/open-picker…"·"incoming + muted: the mute position is no longer a button", `test_call_fsm.cpp` "message reject crossed with the caller's cancel: idle, not missed, silent"·"message tap after the call already ended (idle): nothing sent", Task 6 시나리오 B(이미 끈 벨을 또 누름)·E(고르기 화면에서 받기 자리)·F(대기로 바뀐 뒤 문구 자리)
5. **옛 판과 섞이거나, 통화 기록에 번호가 남는다** — 새 교환기 + 옛 펌웨어, 새 펌웨어 한 대 + 옛 펌웨어 한 대에서도 통화·거절이 되어야 하고, 교환기 기록은 글자 하나 달라지면 안 된다. → Task 1 `test_reject_msg_is_not_logged`·`test_relay_reject_with_msg`, Task 2 `test_relay_proto.cpp` "ended reject without msg: 0 (old relay)", Task 6 Step 2(새 교환기 + 옛 펌웨어 두 대)·Step 4(시나리오 H)·Step 10(기록에 `msg` 0건)

---

## 비목표·영향 범위

설계서 §2·§3 과 같다. 이 계획에서 더 좁힌 것: **통화 설계서(`2026-10-03-tab5-phone.md` §9.4·§10·§8)와 `firmware/tab5_phone/README.md` 는 고치지 않는다** — Phase 2 계획의 Task 10 이 한 번에 고친다(넘길 목록은 Task 7). `phone_ctl.py` 의 `shot` 에 Wi-Fi 자판 화면 가드를 넣는 일도 하지 않는다(절차로 막는다 — Global Constraints).

정정(2026-10-04): 이 판단("절차로 막는다")은 최종 전체 검수(93점)로 뒤집혔다 — 비밀번호가 든 자판 화면이 `shot` 으로 찍힐 수 있는 길이 절차뿐이었고 Task 8 에서 실제로 1회 일어났다. 펌웨어가 입력이 든 비밀번호 자판 화면의 스크린샷을 거절(`SHOT FAIL private`)하도록 고쳤다(커밋 `a26d237`), 도구는 그 응답을 다시 받지 않고 실패로 끝낸다(`99466b9`). 도구가 `screen=WIFI_PASS` 를 통째로 거절하는 가드는 끝내 넣지 않았다(빈 자판 화면은 찍혀야 하고 막는 일은 펌웨어가 프레임 단위로 한다).

- 바뀌는 저장소 파일 15개 + 다시 만드는 `main/ui_assets.h`: 아래 "초안 목록"의 패치가 건드리는 것이 전부다.
- 기기: Task 6 에서 2번·1번 순서로 다시 플래시(각 5분 안팎, 저장된 Wi-Fi 목록·SD 카드는 그대로). 교환기: 맥북의 개발용만 Task 6 에서 한 번 다시 띄운다. 과금 없음.
- 새 산출물은 git 제외 폴더 `firmware/tab5_phone/out/` 에만 생긴다(`dm_*.log`, `dm_*.jpg`, `fw_prev/`).

## 롤백

- **코드**: 이 기능의 커밋(교환기 / 판단 헤더 / 도구 / 자산 / 화면·진행)을 `git revert`. 부분 되돌리기는 설계서 §9(교환기만·기기만 되돌려도 통화는 된다).
  - 정정(2026-10-04): 커밋을 따로따로 되돌릴 수 있는 것은 교환기(`6ff6108`)와 가짜 상대 도구(`ff90e5b`)뿐이다. 판단 헤더(`618eebf`)·글자 자산(`5971d4c`)·화면·진행(`113097c`)은 **함께** 되돌린다 — 중간 상태(헤더만 되돌리거나 화면·진행만 되돌림)는 `-Werror=switch` 로 펌웨어가 빌드되지 않는다(DMP-D1). 설계서 §9 의 "화면만 문제면 화면·진행 커밋만 되돌린다"도 같은 정정을 받았다.
- **기기 → 이 기능 직전의 펌웨어**(Task 8 을 통과한 `a909f7d` 빌드, Task 5 Step 3 이 `out/fw_prev/` 에 보관): `cd ~/tab5-toy-phone/firmware/tab5_phone && source ~/esp/esp-idf-5.5.4/export.sh && PORT=$(python3 tools/phone_ctl.py port 2) && python -m esptool --chip esp32p4 -p "$PORT" read_mac | grep -i "aa:bb:cc:dd:ee:02" && cd out/fw_prev && python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash @flash_args` (1번은 `port 1`, `aa:bb:cc:dd:ee:01`).
- **교환기**: 코드를 되돌린 뒤 `bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh restart`.

## 이 계획에서 내린 결정 (DMP-D)

설계서의 결정(DM-D1~D14)은 그대로 따른다. 여기 것은 계획을 쓰며 새로 정한 것뿐이다.

| ID | 결정 | 이유 | 비용 | 탈출구 |
|---|---|---|---|---|
| DMP-D1 | Task 2 의 커밋부터 Task 5 의 커밋 전까지 **펌웨어 전체 빌드는 깨진 채로 둔다** | 설계서의 커밋 나누기(판단 헤더 / 자산 / 화면·진행)와 "시험 먼저"를 지키려는 것. 헤더의 새 열거값을 `ui.cpp`·`app.cpp` 가 아직 다루지 않아 `-Werror=switch` 로 실패한다(사본에서 확인: `ui.cpp:20`, `ui.cpp:133`, `app.cpp:120`) | 그 사이의 커밋은 기기에 올릴 수 없다(맥 시험·pytest 는 커밋마다 통과) | Task 2·4·5 를 한 커밋으로 합친다 |
| DMP-D2 | Task 5 에서 다시 빌드하기 전에, 지금 두 기기에 올라가 있는 빌드를 `out/fw_prev/` 에 보관한다 | 저장소의 `build/` 를 덮어쓰면 롤백 때 같은 바이트를 올릴 수 없다(다시 빌드하면 버전 문자열·시각이 달라진다) | 2.3MB 복사(git 제외) | `a909f7d` 를 체크아웃해 다시 빌드 |
| DMP-D3 | 기기 확인은 전부 Task 6 으로 모은다(새 교환기 + 옛 펌웨어 확인 포함) | 코드 과제 묶음(Task 1~5)이 기기·교환기를 건드리지 않게 — 한 담당이 이어서 적용하고 한 번에 검수한다(컨트롤러 지시) | 교환기 변경이 기기에서 확인되는 시점이 늦어진다(그 사이 가동 중인 교환기는 옛 코드 그대로라 영향 없음) | Task 1 끝에 Task 6 Step 2 를 먼저 돌린다 |
| DMP-D4 | 설계서에 없는 안전장치 셋을 더한다: `ui_layout.h` 의 `static_assert` 둘(`Tap::MSG1..MSG3` 의 수와 `BTN_MSG` 의 수가 `MSG_COUNT` 와 같다), 가짜 상대의 `expect` 번호도 `ended:reject` 에만 허용, 번호 글자는 `-?\d+` 만 정수로 받는다(`1_0`, ` 2` 는 오류) | 문구 수를 늘릴 때 빠뜨리기 쉬운 자리를 빌드에서 잡고, 단계 글의 오타가 조용히 다른 번호가 되지 않게 | 없음에 가깝다 | 세 줄을 지운다 |
| DMP-D5 | 문서 과제는 이 기능의 설계서와 이 계획서만 고친다(컨트롤러 지시) | 통화 설계서·README 는 Phase 2 계획 Task 10 이 어차피 고친다 — 두 번 고치지 않는다 | 그때까지 통화 설계서 §9.4·§10 이 실제와 다르다 | Task 7 의 넘김 목록을 바로 반영한다 |

---

### Task 1: 교환기 — 거절의 문구 번호를 건넨다 (시험 먼저)

**Files:**
- Modify: `server/phone_core.py`
- Test: `server/tests/test_phone_core.py`, `server/tests/test_phone_relay.py` (추가만)

**Interfaces:**
- Consumes: `phone_core.Exchange.on_text(dev, raw, now)`·`_end(why, now)`, 시험 도우미 `sends`·`logs`·`both_online`(`test_phone_core.py`), `relay`·`pair`·`rx`(`test_phone_relay.py`).
- Produces (뒤 과제가 이 모양을 그대로 쓴다):
  - 신호: 기기 → 교환기 `{"t":"reject","msg":N}` → 교환기 → 양쪽 `{"t":"ended","why":"reject","msg":N}` — N 이 **정수(참·거짓 제외) 1~99** 일 때만. 그 밖의 값은 번호 없는 `{"t":"ended","why":"reject"}`(통화는 끝난다, `bad_signal` 은 그대로 0).
  - `phone_core.REJECT_MSG_MIN = 1`, `REJECT_MSG_MAX = 99`, `Exchange._reject_msg(msg) -> int | None`, `Exchange._end(why, now, msg=None)`.
  - 그대로인 것: `reject` 가 유효한 조건(벨 중 + 받는 쪽), 통화 기록(`call_end` 에 `msg` 없음), 다른 종료(`hangup`·`timeout`·`offline`·`lost`)의 `ended`.

- [x] **Step 1: 기준선을 확인하고 시험 패치를 적용한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-decline-mute-plan/draft && (cd server && PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider 2>&1 | tail -1) && git apply "$D/patches/T1-01-relay-tests.patch" && git diff --numstat -- server/tests
```
Expected: `87 passed in …s`, 이어서 `61	0	server/tests/test_phone_core.py`, `26	0	server/tests/test_phone_relay.py` (삭제 줄 0).

```diff
--- a/server/tests/test_phone_core.py
+++ b/server/tests/test_phone_core.py
@@ -1,4 +1,5 @@
 """phone_core 순수 로직 시험. 실행: cd server && python3 -m pytest tests/test_phone_core.py -q"""
+import json
 import sys
 from pathlib import Path
 
@@ -305,3 +306,63 @@
     allowed = {"ev", "dev", "caller", "callee", "why", "ring_s", "talk_s"}
     for rec in logs(acts):
         assert set(rec) <= allowed
+
+
+# ── 거절 문구 번호 (수신거부 메시지 — docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md §5.2, DM-D6) ──
+def ringing():
+    """A 가 걸어 B 에 벨이 울리는 중."""
+    ex = both_online()
+    ex.on_text(A, '{"t":"call"}', 1)
+    return ex
+
+
+@pytest.mark.parametrize("n", [1, 2, 3, 4, 99])
+def test_reject_msg_reaches_both(n):
+    """번호는 1~99 의 정수면 뜻을 묻지 않고 양쪽 ended 에 그대로 싣는다(4·99 는 기기가 모르는 번호 — 그래도 건넨다)."""
+    ex = ringing()
+    acts = ex.on_text(B, json.dumps({"t": "reject", "msg": n}), 4)
+    want = {"t": "ended", "why": "reject", "msg": n}
+    assert sends(acts) == [(A, want), (B, want)]
+    assert ex.state == pc.IDLE
+
+
+@pytest.mark.parametrize("raw", ["0", "-1", "100", "1000000000000000000000000000000", "2.0", "1e0", '"2"', "true", "false", "null",
+                                 "[2]", '{"n":2}', '""'])
+def test_reject_with_wrong_msg_is_a_plain_reject(raw):
+    """검토 초점 1: 번호가 틀려도 거절은 맞는 신호다 — 통화는 끝나고, 번호만 빠진다. bad_signal 로 세지 않는다."""
+    ex = ringing()
+    acts = ex.on_text(B, '{"t":"reject","msg":%s}' % raw, 4)
+    plain = {"t": "ended", "why": "reject"}
+    assert sends(acts) == [(A, plain), (B, plain)]
+    assert ex.state == pc.IDLE and ex.stats["bad_signal"] == 0
+
+
+def test_reject_msg_is_not_logged():
+    """기록은 번호가 없을 때와 글자까지 같다(§9.3 "기록: 시각·기기·통화 길이·종료 사유만")."""
+    with_msg = logs(ringing().on_text(B, '{"t":"reject","msg":2}', 4))
+    without = logs(ringing().on_text(B, '{"t":"reject"}', 4))
+    assert with_msg == without == [{"ev": "call_end", "caller": A, "callee": B, "why": "reject", "ring_s": 3.0, "talk_s": 0}]
+
+
+def test_reject_msg_from_the_wrong_side_or_state_is_ignored():
+    """reject 가 유효한 조건(벨 중 + 받는 쪽)은 번호가 있어도 그대로다."""
+    ex = ringing()
+    assert ex.on_text(A, '{"t":"reject","msg":2}', 2) == [] and ex.state == pc.RINGING    # 거는 쪽은 거절할 수 없다
+    ex.on_text(B, '{"t":"accept"}', 3)
+    assert ex.on_text(B, '{"t":"reject","msg":2}', 4) == [] and ex.state == pc.IN_CALL    # 통화 중
+    idle = both_online()
+    assert idle.on_text(B, '{"t":"reject","msg":2}', 1) == [] and idle.state == pc.IDLE   # 대기 중(상대가 이미 취소한 뒤의 거절)
+    assert ex.stats["bad_signal"] == 0 and idle.stats["bad_signal"] == 0
+
+
+def test_msg_on_other_signals_is_not_forwarded():
+    """msg 는 reject 에서만 읽는다 — hangup·call·accept 에 붙어 와도 건네지 않고, 다른 종료의 ended 는 글자 하나 달라지지 않는다."""
+    ex = ringing()
+    acts = ex.on_text(B, '{"t":"hangup","msg":2}', 4)
+    assert sends(acts) == [(A, {"t": "ended", "why": "hangup"}), (B, {"t": "ended", "why": "hangup"})]
+    ex = both_online()
+    assert sends(ex.on_text(A, '{"t":"call","msg":2}', 1)) == [(A, {"t": "ringing"}), (B, {"t": "ring"})]
+    assert sends(ex.on_text(B, '{"t":"accept","msg":2}', 2)) == [(A, {"t": "connected"}), (B, {"t": "connected"})]
+    ex = ringing()
+    ex.on_text(A, '{"t":"reject","msg":2}', 2)                                            # 무시된 거절의 번호가 남아 있다가
+    assert sends(ex.tick(31.0)) == [(A, {"t": "ended", "why": "timeout"}), (B, {"t": "ended", "why": "timeout"})]   # 뒤의 종료에 묻어 나오지 않는다
--- a/server/tests/test_phone_relay.py
+++ b/server/tests/test_phone_relay.py
@@ -171,8 +171,34 @@
     await a.send_json({"t": "call"})
     await rx(a), await rx(b)
     await b.send_json({"t": "reject"})
+    assert await rx(a) == {"t": "ended", "why": "reject"}
+    assert await rx(b) == {"t": "ended", "why": "reject"}
+
+
+async def test_relay_reject_with_msg(relay, caplog):
+    """수신거부 메시지: 실제 WebSocket 으로 양쪽이 문구 번호를 받고, 통화 기록 줄에는 번호가 없다."""
+    client, _, _ = relay
+    caplog.set_level(logging.INFO)
+    a, b = await pair(client)
+    await a.send_json({"t": "call"})
+    await rx(a), await rx(b)
+    await b.send_json({"t": "reject", "msg": 2})
+    assert await rx(a) == {"t": "ended", "why": "reject", "msg": 2}
+    assert await rx(b) == {"t": "ended", "why": "reject", "msg": 2}
+    ends = [r.getMessage() for r in caplog.records if "call_end" in r.getMessage()]
+    assert len(ends) == 1 and '"why":"reject"' in ends[0] and "msg" not in ends[0]
+
+
+async def test_relay_reject_with_wrong_msg_is_plain(relay):
+    """번호 자리에 글자를 실어 보내도 건네지 않는다(DM-D1: 문구는 번호로만) — 거절은 그대로 된다."""
+    client, _, app = relay
+    a, b = await pair(client)
+    await a.send_json({"t": "call"})
+    await rx(a), await rx(b)
+    await b.send_json({"t": "reject", "msg": "회의 중이에요"})
     assert await rx(a) == {"t": "ended", "why": "reject"}
     assert await rx(b) == {"t": "ended", "why": "reject"}
+    assert app[phone_relay.EXCHANGE].stats["bad_signal"] == 0
 
 
 async def test_no_answer_times_out_after_30s(relay):
```

- [x] **Step 2: 실패를 확인한다**

Run: `cd ~/tab5-toy-phone/server && PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider 2>&1 | grep -E "^FAILED|^[0-9]+ (passed|failed)" | sed 's/ - .*//'`
Expected:
```
FAILED tests/test_phone_core.py::test_reject_msg_reaches_both[1]
FAILED tests/test_phone_core.py::test_reject_msg_reaches_both[2]
FAILED tests/test_phone_core.py::test_reject_msg_reaches_both[3]
FAILED tests/test_phone_core.py::test_reject_msg_reaches_both[4]
FAILED tests/test_phone_core.py::test_reject_msg_reaches_both[99]
FAILED tests/test_phone_relay.py::test_relay_reject_with_msg
6 failed, 104 passed in …s
```
(나머지 새 시험 17개는 지금도 통과한다 — 지금의 교환기가 틀린 번호를 그냥 거절로 다루기 때문이다. 구현이 이 동작을 깨뜨리지 못하게 고정하는 시험이다.)

- [x] **Step 3: 구현 패치를 적용하고 초안과 대조한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-decline-mute-plan/draft && git apply "$D/patches/T1-02-relay-core.patch" && for f in server/phone_core.py server/tests/test_phone_core.py server/tests/test_phone_relay.py; do cmp "$D/files/$f" "$f" && echo "같음: $f"; done
```
Expected: `같음:` 3줄.

```diff
--- a/server/phone_core.py
+++ b/server/phone_core.py
@@ -6,6 +6,8 @@
       ("send", 기기ID, dict)   그 기기에 글자 프레임(JSON) 보내기
       ("close", conn)          그 연결 닫기
       ("log", dict)            기록 남기기 (시각·기기·통화 길이·종료 사유만 — 음성·토큰은 절대 넣지 않는다)
+  - 수신거부 메시지(docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md §5.2): reject 에 실린 문구 번호 msg 를
+    ended 에 실어 양쪽에 건넨다. 교환기는 번호의 뜻을 모르고(DM-D1), 기록에도 넣지 않는다.
 """
 import hmac
 import json
@@ -18,6 +20,11 @@
 MAX_TEXT_BYTES = 256           # 신호(JSON)는 수십 바이트 — 이보다 길면 무시
 MAX_PEER_BUFFER_BYTES = 4096   # PL-D3: 상대 쪽 송신 대기열이 이보다 쌓이면 음성을 버린다(약 1초 분량)
 MIN_TOKEN_CHARS = 16
+# DM-D6: 거절 문구 번호는 이 범위의 정수만 건넨다(뜻은 기기가 안다). 범위 밖·정수가 아닌 값은 버리지 않고 "번호 없는 거절"로 낮춘다.
+#   이유: 문구를 늘려도 교환기를 고치지 않고, 번호가 틀렸다고 거절(맞는 신호)까지 무시되면 통화가 안 끝난다.
+#   비용: 틀린 번호는 조용히 사라진다(bad_signal 에도 안 잡힌다). 탈출구: 이 두 값을 고치거나, _reject_msg 에서 bad_signal 을 올린다.
+REJECT_MSG_MIN = 1
+REJECT_MSG_MAX = 99
 
 
 def parse_tokens(spec):
@@ -149,7 +156,7 @@
                 return self._connect(now)
         elif t == "reject":
             if self.state == RINGING and dev == self.callee:
-                return self._end("reject", now)
+                return self._end("reject", now, self._reject_msg(msg))
         elif t == "hangup":
             if self.state != IDLE and dev in (self.caller, self.callee):
                 return self._end("hangup", now)
@@ -169,6 +176,14 @@
             return None
         return msg
 
+    @staticmethod
+    def _reject_msg(msg):
+        """reject 에 실린 문구 번호. 정수(참·거짓 제외)이고 범위 안일 때만 돌려준다 — 아니면 None(번호 없는 거절, DM-D6)."""
+        n = msg.get("msg")
+        if isinstance(n, bool) or not isinstance(n, int) or not REJECT_MSG_MIN <= n <= REJECT_MSG_MAX:
+            return None
+        return n
+
     def _on_call(self, dev, now):
         if self.state == IDLE:
             peer = self.peer_of(dev)
@@ -186,13 +201,14 @@
         self.state, self.call_started = IN_CALL, now
         return [("send", self.caller, {"t": "connected"}), ("send", self.callee, {"t": "connected"})]
 
-    def _end(self, why, now):
-        """통화(또는 벨)를 끝낸다. 아직 붙어 있는 양쪽 모두에 ended 를 보낸다(PL-D1)."""
+    def _end(self, why, now, msg=None):
+        """통화(또는 벨)를 끝낸다. 아직 붙어 있는 양쪽 모두에 ended 를 보낸다(PL-D1). msg = 거절 문구 번호(없으면 None)."""
         caller, callee = self.caller, self.callee
         talk = round(now - self.call_started, 1) if self.state == IN_CALL else 0
         ring = round((self.call_started if self.state == IN_CALL else now) - self.ring_started, 1)
         self.state, self.caller, self.callee = IDLE, None, None
-        acts = [("send", d, {"t": "ended", "why": why}) for d in (caller, callee) if self.is_online(d)]
+        extra = {} if msg is None else {"msg": msg}       # 번호는 기기에만 간다 — 아래 기록(log)에는 넣지 않는다(§9.3)
+        acts = [("send", d, {"t": "ended", "why": why, **extra}) for d in (caller, callee) if self.is_online(d)]
         acts.append(("log", {"ev": "call_end", "caller": caller, "callee": callee, "why": why, "ring_s": ring, "talk_s": talk}))
         return acts
 
```

- [x] **Step 4: 통과를 확인한다**

Run: `cd ~/tab5-toy-phone/server && PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider 2>&1 | tail -1`
Expected: `110 passed in …s` (기존 87 + 새 23)

- [x] **Step 5: 커밋**

```bash
cd ~/tab5-toy-phone && git add server/phone_core.py server/tests/test_phone_core.py server/tests/test_phone_relay.py && git commit -m "feat(phone): 교환기 — 거절 문구 번호(msg)를 ended 에 실어 건넴(수신거부 메시지)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
(가동 중인 개발용 교환기는 건드리지 않는다 — 옛 코드로 계속 돈다. 새 코드로 바꾸는 것은 Task 6 Step 2.)

---

### Task 2: 판단 헤더 3개 — 통화 상태·신호 읽기·화면 배치 (시험 먼저)

**Files:**
- Modify: `firmware/tab5_phone/main/call_fsm.h`, `main/relay_proto.h`, `main/ui_layout.h`
- Test: `firmware/tab5_phone/tools/test_call_fsm.cpp`, `tools/test_relay_proto.cpp`, `tools/test_ui_layout.cpp` (추가만)

**Interfaces:**
- Consumes: Task 1 의 신호 모양(`ended` 의 `msg`).
- Produces (Task 5 가 이 이름·서명을 그대로 쓴다):
  - `call_fsm.h`: `const uint8_t callfsm::MSG_COUNT = 3`, `inline bool callfsm::msg_valid(uint64_t n)`(1~3 만 참), `Ev::TAP_MUTE`(`TAP_HANGUP` 뒤·`TICK` 앞), `Notice::REJECT_MSG`(맨 끝, 이름 `"REJECT_MSG"`), `struct Out { uint16_t acts; Notice notice; uint8_t msg; }`, 공개 필드 `Fsm::bell_muted`, `Out Fsm::on(Ev ev, int64_t now_ms, Why why = Why::NONE, bool flag = false, uint8_t msg = 0)`. 메시지 거절 = `on(Ev::TAP_REJECT, now, Why::NONE, false, 번호)`.
  - `relay_proto.h`: `Signal::msg`(`uint8_t`, `ended`+`reject` 일 때만 1~3, 그 밖 0), `inline int relayproto::reject_text(char *buf, size_t cap, uint8_t msg)`(글자 수, 안 들어가면 0. 번호가 1~3 이 아니면 `{"t":"reject"}`).
  - `ui_layout.h`: `Screen::DECLINE`(맨 끝, 이름 `"DECLINE"`), `Tap::MUTE`·`DECLINE_OPEN`·`DECLINE_BACK`·`MSG1`·`MSG2`·`MSG3`, `Model::bell_muted`·`decline_open`·`notice_msg`, 버튼 `BTN_MUTE {224,568,360,112}`·`BTN_DECLINE {696,568,360,112}`·`BTN_MSG[3] = {48,104,384,128},{448,104,384,128},{848,104,384,128}`·`BTN_BACK {224,568,360,112}`, `NOTICE_MS = 4000`·`NOTICE_MSG_MS = 10000`, `int64_t notice_ms(callfsm::Notice)`, `uint8_t msg_of(Tap)`, `bool notice_visible(const Model &)`, `struct DeclinePicker { bool open; void on_tap(Tap); void sync(callfsm::State); }`.
  - 주입 좌표(버튼 가운데): 벨 끄기 `404,624` · 메시지로 거절 `876,624` · 문구 1·2·3 `240,168`·`640,168`·`1040,168` · 뒤로 `404,624`.
- **주의(DMP-D1)**: 이 과제의 커밋부터 Task 5 전까지 펌웨어 전체 빌드는 실패한다. 그 사이에 `idf.py build`·플래시를 하지 않는다.

맥 단위 시험의 컴파일 명령은 모두 같은 모양이다(`-isysroot` 가 없으면 이 맥에서 링크되지 않는다). 실행 파일은 `/tmp` 에 둔다.

- [x] **Step 1: 통화 상태 기계 — 시험 패치를 적용하고 실패를 확인한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-decline-mute-plan/draft && git apply "$D/patches/T2-01-call-fsm-tests.patch" && git diff --numstat -- firmware/tab5_phone/tools/test_call_fsm.cpp && cd firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_call_fsm.cpp -o /tmp/t_dm_call_fsm 2>&1 | grep -m1 "error:"
```
Expected: `150	0	firmware/tab5_phone/tools/test_call_fsm.cpp`, 이어서 `test_call_fsm.cpp:301:26: error: no member named 'TAP_MUTE' in 'callfsm::Ev'`

```diff
--- a/firmware/tab5_phone/tools/test_call_fsm.cpp
+++ b/firmware/tab5_phone/tools/test_call_fsm.cpp
@@ -290,7 +290,157 @@
     // ── 로그에 찍는 이름 ────────────────────────────────────────────────────
     CHECK("names for the serial log", std::string(state_name(State::RINGBACK)) == "RINGBACK" && std::string(state_name(State::IN_CALL)) == "IN_CALL"
           && std::string(notice_name(Notice::RELAY_SILENT)) == "RELAY_SILENT" && std::string(notice_name(Notice::NONE)) == "NONE");
+
+
+    // ════ 수신거부 메시지·벨 끄기 (docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md §5.3) ════
+    const uint16_t SENDS = A_SEND_CALL | A_SEND_ACCEPT | A_SEND_REJECT | A_SEND_HANGUP;
+
+    // ── 벨 끄기: 이번 전화의 벨만 멈춘다. 신호는 나가지 않고 상태·시간 제한은 그대로 ──
+    {
+        Fsm f = idle(); f.on(Ev::RING, 100);
+        Out o = f.on(Ev::TAP_MUTE, 500);
+        CHECK("mute: stays incoming, only tone-stop, flag set", f.state == State::INCOMING && o.acts == A_TONE_STOP && o.notice == Notice::NONE && o.msg == 0 && f.bell_muted);
+        CHECK("mute never sends a signal", (o.acts & SENDS) == 0);
+        o = f.on(Ev::TAP_MUTE, 900);
+        CHECK("mute twice: second is a no-op", o.acts == 0 && o.notice == Notice::NONE && f.state == State::INCOMING && f.bell_muted);
+        o = f.on(Ev::RING, 950);                                 // Phase 3 약속: 끈 벨을 다시 시작시키는 할 일이 나오지 않는다
+        CHECK("muted: a stray ring does not restart the bell", o.acts == 0 && f.state == State::INCOMING && f.bell_muted);
+        o = f.on(Ev::TICK, 40099);                               // 시간 제한은 벨이 온 시각(100) 기준 그대로 — 끈 시각으로 다시 세지 않는다
+        CHECK("mute keeps the bell time limit", o.acts == 0 && f.state == State::INCOMING);
+        o = f.on(Ev::TICK, 40100);
+        CHECK("muted bell stuck 40s: reconnect, missed, flag cleared", f.state == State::OFFLINE && has(o, A_RECONNECT | A_TONE_STOP) && f.missed == 1 && !f.bell_muted);
+    }
+    {
+        Fsm f = idle(); f.on(Ev::RING, 100);
+        Out o = f.on(Ev::RING, 200);
+        CHECK("bell starts once: a second ring while incoming does nothing", o.acts == 0 && f.state == State::INCOMING && !f.bell_muted);
+    }
+    {
+        Fsm f = idle(); f.on(Ev::RING, 100); f.on(Ev::TAP_MUTE, 200);
+        Out o = f.on(Ev::TAP_ACCEPT, 300);
+        CHECK("mute then accept", f.state == State::ANSWERING && o.acts == (A_SEND_ACCEPT | A_TONE_STOP) && !f.bell_muted);
+        Fsm g = idle(); g.on(Ev::RING, 100); g.on(Ev::TAP_MUTE, 200);
+        o = g.on(Ev::TAP_REJECT, 300);
+        CHECK("mute then plain reject", g.state == State::ENDING && o.acts == (A_SEND_REJECT | A_TONE_STOP) && o.msg == 0 && !g.bell_muted);
+        Fsm h = idle(); h.on(Ev::RING, 100); h.on(Ev::TAP_MUTE, 200);
+        o = h.on(Ev::TAP_REJECT, 300, Why::NONE, false, 2);
+        CHECK("mute then message reject", h.state == State::ENDING && o.acts == (A_SEND_REJECT | A_TONE_STOP) && o.msg == 2 && !h.bell_muted);
+    }
+    {   // 벨 끄기는 다음 전화에 남지 않는다
+        Fsm f = idle(); f.on(Ev::RING, 100); f.on(Ev::TAP_MUTE, 200);
+        Out o = f.on(Ev::ENDED, 300, Why::HANGUP);
+        CHECK("muted call canceled by caller: missed as before", f.state == State::IDLE && f.missed == 1 && o.acts == A_TONE_STOP && o.notice == Notice::MISSED && !f.bell_muted);
+        o = f.on(Ev::RING, 400);
+        CHECK("next call rings again", f.state == State::INCOMING && has(o, A_TONE_BELL) && !f.bell_muted);
+        f.on(Ev::TAP_MUTE, 500);
+        f.on(Ev::ENDED, 600, Why::TIMEOUT); o = f.on(Ev::RING, 600);    // 한 바퀴(같은 시각) 안에 ended → ring
+        CHECK("ended then ring in one batch: the new call is not muted", f.state == State::INCOMING && has(o, A_TONE_BELL) && !f.bell_muted && f.missed == 2);
+        o = f.on(Ev::TAP_MUTE, 650);
+        CHECK("the new call can be muted again", o.acts == A_TONE_STOP && f.bell_muted);
+        o = f.on(Ev::LINK_DOWN, 800);
+        CHECK("muted call + link down", f.state == State::OFFLINE && has(o, A_TONE_STOP) && o.notice == Notice::MISSED && f.missed == 3 && !f.bell_muted);
+    }
+    {
+        bool all = true;
+        Fsm a;                                                      all = all && a.on(Ev::TAP_MUTE, 1).acts == 0 && a.state == State::OFFLINE && !a.bell_muted;
+        Fsm b = idle();                                             all = all && b.on(Ev::TAP_MUTE, 1).acts == 0 && b.state == State::IDLE && !b.bell_muted;
+        Fsm c = idle(); c.on(Ev::TAP_CALL, 1);                      all = all && c.on(Ev::TAP_MUTE, 2).acts == 0 && c.state == State::DIALING && !c.bell_muted;
+        c.on(Ev::RINGING, 3);                                       all = all && c.on(Ev::TAP_MUTE, 4).acts == 0 && c.state == State::RINGBACK && !c.bell_muted;
+        c.on(Ev::CONNECTED, 5);                                     all = all && c.on(Ev::TAP_MUTE, 6).acts == 0 && c.state == State::IN_CALL && !c.bell_muted;
+        c.on(Ev::TAP_HANGUP, 7);                                    all = all && c.on(Ev::TAP_MUTE, 8).acts == 0 && c.state == State::ENDING && !c.bell_muted;
+        Fsm d = idle(); d.on(Ev::RING, 1); d.on(Ev::TAP_ACCEPT, 2); all = all && d.on(Ev::TAP_MUTE, 3).acts == 0 && d.state == State::ANSWERING && !d.bell_muted;
+        CHECK("mute outside INCOMING is ignored in every state", all);
+    }
+
+    // ── 메시지 거절(받는 쪽): TAP_REJECT 에 문구 번호를 실어 넣는다(DM-D7) ──
+    {
+        bool all = true;
+        for (uint8_t n = 1; n <= MSG_COUNT; ++n) {
+            Fsm f = idle(); f.on(Ev::RING, 100);
+            Out o = f.on(Ev::TAP_REJECT, 900, Why::NONE, false, n);
+            all = all && f.state == State::ENDING && o.acts == (A_SEND_REJECT | A_TONE_STOP) && o.msg == n && o.notice == Notice::NONE;
+            o = f.on(Ev::TAP_CALL, 910);                                    // ended 를 받기 전에는 걸지 않는다
+            all = all && !has(o, A_SEND_CALL) && f.state == State::ENDING;
+            o = f.on(Ev::ENDED, 950, Why::REJECT, false, n);                // 교환기는 번호를 거절한 쪽에도 돌려준다
+            all = all && f.state == State::IDLE && f.missed == 0 && o.notice == Notice::NONE && o.acts == 0 && o.msg == 0;
+        }
+        CHECK("message reject 1..3: ending + reject + number; my own ended shows nothing", all);
+    }
+    {
+        Fsm f = idle(); f.on(Ev::RING, 1);
+        Out o = f.on(Ev::TAP_REJECT, 2, Why::NONE, false, 0);
+        CHECK("reject with number 0 = plain reject", f.state == State::ENDING && o.acts == (A_SEND_REJECT | A_TONE_STOP) && o.msg == 0);
+        Fsm g = idle(); g.on(Ev::RING, 1);
+        o = g.on(Ev::TAP_REJECT, 2, Why::NONE, false, 4);
+        Fsm h = idle(); h.on(Ev::RING, 1);
+        Out o2 = h.on(Ev::TAP_REJECT, 2, Why::NONE, false, 255);
+        CHECK("reject with an out-of-range number still rejects, without a number",
+              g.state == State::ENDING && has(o, A_SEND_REJECT) && o.msg == 0 && h.state == State::ENDING && has(o2, A_SEND_REJECT) && o2.msg == 0);
+        Fsm k = idle(); k.on(Ev::RING, 1); k.on(Ev::TAP_REJECT, 1000, Why::NONE, false, 1);
+        o = k.on(Ev::TICK, 3999);
+        const bool waits = k.state == State::ENDING && o.acts == 0;
+        o = k.on(Ev::TICK, 4000);
+        CHECK("message reject unanswered 3s: reconnect", waits && k.state == State::OFFLINE && has(o, A_RECONNECT));
+        Fsm q = idle();
+        o = q.on(Ev::TAP_REJECT, 5, Why::NONE, false, 2);
+        CHECK("message tap after the call already ended (idle): nothing sent", q.state == State::IDLE && o.acts == 0 && o.msg == 0 && o.notice == Notice::NONE);
+        Fsm x = idle(); x.on(Ev::RING, 1); x.on(Ev::TAP_REJECT, 2, Why::NONE, false, 2);
+        o = x.on(Ev::ENDED, 3, Why::HANGUP);                               // 문구를 누른 순간 거는 쪽이 취소 — 교환기는 내 reject 를 무시한다
+        CHECK("message reject crossed with the caller's cancel: idle, not missed, silent", x.state == State::IDLE && x.missed == 0 && o.acts == 0 && o.notice == Notice::NONE);
+    }
 
+    // ── 거는 쪽: 문구와 함께 거절당하면 REJECT_MSG + 번호 ──
+    {
+        bool all = true;
+        for (uint8_t n = 1; n <= MSG_COUNT; ++n) {
+            Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2);
+            Out o = f.on(Ev::ENDED, 900, Why::REJECT, false, n);
+            all = all && f.state == State::IDLE && o.notice == Notice::REJECT_MSG && o.msg == n && o.acts == A_TONE_END;
+            Fsm d = idle(); d.on(Ev::TAP_CALL, 1);                          // ringing 이 오기 전(DIALING)에 거절당해도 같다
+            o = d.on(Ev::ENDED, 5, Why::REJECT, false, n);
+            all = all && d.state == State::IDLE && o.notice == Notice::REJECT_MSG && o.msg == n && o.acts == A_TONE_END;
+        }
+        CHECK("caller: rejected with message 1..3 -> REJECT_MSG + number + end tone", all);
+        Fsm f = idle(); f.on(Ev::TAP_CALL, 1); f.on(Ev::RINGING, 2);
+        Out o = f.on(Ev::ENDED, 900, Why::REJECT);
+        CHECK("caller: reject without a number = REJECTED as before", f.state == State::IDLE && o.notice == Notice::REJECTED && o.msg == 0 && o.acts == A_TONE_END);
+        bool plain = true;
+        for (int n : { 0, 4, 99, 200, 255 }) {
+            Fsm g = idle(); g.on(Ev::TAP_CALL, 1); g.on(Ev::RINGING, 2);
+            Out x = g.on(Ev::ENDED, 900, Why::REJECT, false, (uint8_t)n);
+            plain = plain && g.state == State::IDLE && x.notice == Notice::REJECTED && x.msg == 0 && x.acts == A_TONE_END;
+        }
+        CHECK("caller: reject with an unknown number = plain REJECTED", plain);
+        bool gate = true;
+        const Why whys[] = { Why::HANGUP, Why::TIMEOUT, Why::OFFLINE, Why::LOST };
+        const Notice want[] = { Notice::REJECTED, Notice::NO_ANSWER, Notice::PEER_OFF, Notice::LOST };
+        for (int i = 0; i < 4; ++i) {
+            Fsm g = idle(); g.on(Ev::TAP_CALL, 1); g.on(Ev::RINGING, 2);
+            Out x = g.on(Ev::ENDED, 900, whys[i], false, 2);
+            gate = gate && g.state == State::IDLE && x.notice == want[i] && x.msg == 0;
+        }
+        CHECK("caller: a number on a non-reject ended is ignored", gate);
+        Fsm r = idle(); r.on(Ev::TAP_CALL, 1); r.on(Ev::RINGING, 2); r.on(Ev::ENDED, 900, Why::REJECT, false, 3);
+        o = r.on(Ev::TAP_CALL, 1000);                                       // 안내(10초)가 떠 있는 동안에도 다시 걸 수 있다
+        CHECK("caller: can call again right after a message reject", r.state == State::DIALING && o.acts == A_SEND_CALL);
+    }
+
+    // ── 번호가 든 ended 가 다른 상태에 오면: 지금과 같다(번호는 무시) ──
+    {
+        Fsm f = idle();
+        Out o = f.on(Ev::ENDED, 10, Why::REJECT, false, 2);
+        CHECK("ended+number in idle ignored", f.state == State::IDLE && o.acts == 0 && o.notice == Notice::NONE && o.msg == 0 && f.missed == 0);
+        f.on(Ev::RING, 20);
+        o = f.on(Ev::ENDED, 30, Why::REJECT, false, 2);
+        CHECK("ended+number during my bell = missed", f.state == State::IDLE && o.notice == Notice::MISSED && o.msg == 0 && f.missed == 1);
+        Fsm g = idle(); g.on(Ev::TAP_CALL, 1); g.on(Ev::RINGING, 2); g.on(Ev::CONNECTED, 3);
+        o = g.on(Ev::ENDED, 50, Why::REJECT, false, 2);
+        CHECK("ended+number in call = call ended", g.state == State::IDLE && o.notice == Notice::CALL_ENDED && o.msg == 0);
+    }
+    CHECK("notice name REJECT_MSG (does not start with REJECTED)", std::string(notice_name(Notice::REJECT_MSG)) == "REJECT_MSG" && std::string(notice_name(Notice::REJECTED)) == "REJECTED"
+          && std::string(notice_name(Notice::REJECT_MSG)).rfind("REJECTED", 0) != 0);
+    CHECK("msg_valid: 1..3 only (0, 4, 2^32+1 are not)", MSG_COUNT == 3 && !msg_valid(0) && msg_valid(1) && msg_valid(2) && msg_valid(3) && !msg_valid(4) && !msg_valid(255) && !msg_valid(4294967297ull));
+
     printf("\n%d ok, %d failed\n", ok, bad);
     return bad ? 1 : 0;
 }
```

- [x] **Step 2: 통화 상태 기계 — 구현 패치를 적용하고 통과를 확인한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-decline-mute-plan/draft && git apply "$D/patches/T2-02-call-fsm.patch" && cmp "$D/files/firmware/tab5_phone/main/call_fsm.h" firmware/tab5_phone/main/call_fsm.h && cmp "$D/files/firmware/tab5_phone/tools/test_call_fsm.cpp" firmware/tab5_phone/tools/test_call_fsm.cpp && cd firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_call_fsm.cpp -o /tmp/t_dm_call_fsm && /tmp/t_dm_call_fsm | tail -1
```
Expected: 컴파일 경고 없이 `105 ok, 0 failed` (기존 73 + 새 32)

```diff
--- a/firmware/tab5_phone/main/call_fsm.h
+++ b/firmware/tab5_phone/main/call_fsm.h
@@ -8,6 +8,13 @@
 //     (FSM-D1: 다시 붙으면 교환기가 그 통화를 끝내고 옛 연결의 뒤늦은 프레임을 버리므로 양쪽 상태가 확실히 맞는다.
 //      비용: 재연결 1~2초 동안 전화를 못 받는다. 탈출구: 시간 제한 값을 늘리거나 A_RECONNECT 대신 대기로 복귀).
 //   - 받기를 누르지 않았는데 connected 가 오면 통화로 들어가지 않는다(PH-D6: 원치 않을 때 소리가 새지 않게) → 재연결로 맞춘다.
+// 수신거부 메시지·벨 끄기(docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md §5.3):
+//   - 벨 끄기(TAP_MUTE)는 이번 전화의 벨만 멈춘다(A_TONE_STOP). 신호는 나가지 않고 상태·시간 제한도 그대로다.
+//     벨 시작(A_TONE_BELL)은 IDLE → INCOMING 으로 들어갈 때 한 번만 낸다 → 끈 뒤에는 그 통화가 끝날 때까지 벨을 다시 시작시키는 할 일이 나오지 않는다
+//     (Phase 3 소리 모듈과의 약속: A_TONE_STOP 을 받으면 스스로 다시 울리지 않는다, 이미 멈춘 뒤의 A_TONE_STOP 도 무해해야 한다).
+//   - 메시지 거절은 새 사건이 아니라 TAP_REJECT 에 문구 번호(msg, 1~MSG_COUNT)를 실어 넣는다(DM-D7: 전이·할 일이 그냥 거절과 같고 다른 것은 번호뿐.
+//     비용: 번호를 넣으려면 why·flag 자리를 채워 적어야 한다. 탈출구: TAP_REJECT_MSG 사건으로 나눈다).
+//   - "벨 꺼짐"은 여기(bell_muted)가, "메시지 고르기 화면 열림"은 화면 쪽(uilayout::DeclinePicker)이 기억한다(DM-D8).
 #pragma once
 #include <cstdint>
 
@@ -30,7 +37,8 @@
     LINK_UP, LINK_DOWN,                                          // 교환기 연결됨 / 끊김
     PEER,                                                        // 상대 기기 접속 여부 (flag = online)
     RINGING, RING, CONNECTED, ENDED,                             // 교환기 신호 (ENDED 는 why 와 함께)
-    TAP_CALL, TAP_CANCEL, TAP_ACCEPT, TAP_REJECT, TAP_HANGUP,    // 화면 버튼
+    TAP_CALL, TAP_CANCEL, TAP_ACCEPT, TAP_REJECT, TAP_HANGUP,    // 화면 버튼 (TAP_REJECT 는 문구 번호 msg 를 실을 수 있다)
+    TAP_MUTE,                                                    // 벨 끄기(이번 전화만)
     TICK,                                                        // 주기 호출(시간 제한 검사). 100ms 간격이면 충분
 };
 
@@ -57,11 +65,16 @@
     CALL_ENDED,     // 통화 종료
     MISSED,         // 부재중
     RELAY_SILENT,   // 교환기 응답 없음
+    REJECT_MSG,     // 상대가 문구와 함께 거절(Out.msg = 문구 번호)
 };
 
+const uint8_t MSG_COUNT = 3;             // 거절 문구 수. 번호는 1~MSG_COUNT, 0 = 문구 없음
+inline bool msg_valid(uint64_t n) { return n >= 1 && n <= MSG_COUNT; }   // 64비트로 받는다 — 잘라서 비교하면 2^32+1 이 1 이 된다
+
 struct Out {
     uint16_t acts = 0;
     Notice notice = Notice::NONE;
+    uint8_t msg = 0;                     // A_SEND_REJECT 와 함께: 실어 보낼 문구 번호 / REJECT_MSG 와 함께: 보여 줄 문구 번호. 없으면 0
 };
 
 const int64_t SIGNAL_WAIT_MS = 5000;     // call·accept 를 보낸 뒤 교환기 응답을 기다리는 한도
@@ -76,10 +89,11 @@
     int64_t last_talk_ms = 0;            // 직전 통화 길이
     uint16_t missed = 0;                 // 부재중 건수
     int64_t missed_at_ms = 0;            // 마지막 부재중 시각
+    bool    bell_muted = false;          // 이번 전화의 벨을 껐다. INCOMING 을 떠나는 모든 길(enter·offline)에서 풀린다
 
     void clear_missed() { missed = 0; }
 
-    Out on(Ev ev, int64_t now_ms, Why why = Why::NONE, bool flag = false)
+    Out on(Ev ev, int64_t now_ms, Why why = Why::NONE, bool flag = false, uint8_t msg = 0)
     {
         Out o;
         switch (ev) {
@@ -123,6 +137,7 @@
                          : why == Why::OFFLINE ? Notice::PEER_OFF
                          : why == Why::LOST    ? Notice::LOST
                          :                       Notice::REJECTED;   // reject, 또는 받는 쪽이 벨 중에 보낸 hangup
+                if (why == Why::REJECT && msg_valid(msg)) { o.notice = Notice::REJECT_MSG; o.msg = msg; }   // 문구와 함께 거절. 번호는 reject 에만 뜻이 있다
                 enter(State::IDLE, 0);
             } else if (ev == Ev::TAP_CANCEL) {
                 o.acts = A_SEND_HANGUP | A_TONE_STOP; enter(State::ENDING, now_ms + ENDING_WAIT_MS);
@@ -132,8 +147,10 @@
         case State::INCOMING:
             if (ev == Ev::TAP_ACCEPT) {
                 o.acts = A_SEND_ACCEPT | A_TONE_STOP; enter(State::ANSWERING, now_ms + SIGNAL_WAIT_MS);
-            } else if (ev == Ev::TAP_REJECT) {
-                o.acts = A_SEND_REJECT | A_TONE_STOP; enter(State::ENDING, now_ms + ENDING_WAIT_MS);
+            } else if (ev == Ev::TAP_REJECT) {                  // 번호가 1~MSG_COUNT 면 메시지 거절, 아니면 그냥 거절(번호 0)
+                o.acts = A_SEND_REJECT | A_TONE_STOP; o.msg = msg_valid(msg) ? msg : 0; enter(State::ENDING, now_ms + ENDING_WAIT_MS);
+            } else if (ev == Ev::TAP_MUTE) {                    // 상태·시간 제한은 그대로. 두 번째부터는 아무 일도 없다
+                if (!bell_muted) { bell_muted = true; o.acts = A_TONE_STOP; }
             } else if (ev == Ev::ENDED) {                       // timeout·거는 쪽 취소(hangup)·거는 쪽 사라짐(offline) 모두 부재중
                 o.acts = A_TONE_STOP; o.notice = Notice::MISSED; miss(now_ms); enter(State::IDLE, 0);
             } else if (ev == Ev::CONNECTED) {                   // 받기를 누르지 않았다 → 통화로 들어가지 않는다
@@ -171,8 +188,8 @@
     }
 
 private:
-    void enter(State s, int64_t deadline) { state = s; deadline_ms = deadline; }
-    void offline() { state = State::OFFLINE; peer_online = false; deadline_ms = 0; }
+    void enter(State s, int64_t deadline) { state = s; deadline_ms = deadline; bell_muted = false; }
+    void offline() { state = State::OFFLINE; peer_online = false; deadline_ms = 0; bell_muted = false; }
     void miss(int64_t now_ms) { if (missed < 0xFFFF) ++missed; missed_at_ms = now_ms; }
 
     Out begin_call(int64_t now_ms)
@@ -236,7 +253,7 @@
     switch (n) {
     case Notice::NONE: return "NONE"; case Notice::PEER_OFF: return "PEER_OFF"; case Notice::NO_ANSWER: return "NO_ANSWER";
     case Notice::REJECTED: return "REJECTED"; case Notice::LOST: return "LOST"; case Notice::CALL_ENDED: return "CALL_ENDED";
-    case Notice::MISSED: return "MISSED"; case Notice::RELAY_SILENT: return "RELAY_SILENT";
+    case Notice::MISSED: return "MISSED"; case Notice::RELAY_SILENT: return "RELAY_SILENT"; case Notice::REJECT_MSG: return "REJECT_MSG";
     }
     return "?";
 }
```

- [x] **Step 3: 신호 읽기·쓰기 — 시험 패치를 적용하고 실패를 확인한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-decline-mute-plan/draft && git apply "$D/patches/T2-03-relay-proto-tests.patch" && git diff --numstat -- firmware/tab5_phone/tools/test_relay_proto.cpp && cd firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_relay_proto.cpp -o /tmp/t_dm_relay_proto 2>&1 | grep -m1 "error:"
```
Expected: `62	0	firmware/tab5_phone/tools/test_relay_proto.cpp`, 이어서 `test_relay_proto.cpp:102:134: error: no member named 'msg' in 'relayproto::Signal'`

```diff
--- a/firmware/tab5_phone/tools/test_relay_proto.cpp
+++ b/firmware/tab5_phone/tools/test_relay_proto.cpp
@@ -90,6 +90,68 @@
     CHECK("backoff capped at 30s", backoff_ms(5, false) == 30000 && backoff_ms(50, false) == 30000 && backoff_ms(-3, false) == 1000);
     CHECK("auth failure waits 60s", backoff_ms(0, true) == 60000 && backoff_ms(9, true) == 60000);
 
+
+    // ════ 수신거부 메시지: ended(why=reject) 의 msg, 보낼 때의 reject_text (설계서 decline-mute §5.2) ════
+    {
+        bool all = true;
+        for (int n = 1; n <= 3; ++n) {
+            const std::string d = std::to_string(n);
+            Signal a = P("{\"t\":\"ended\",\"why\":\"reject\",\"msg\":" + d + "}");            // 교환기가 실제로 보내는 모양(공백 없음)
+            Signal b = P("{\"t\": \"ended\", \"why\": \"reject\", \"msg\": " + d + "}");
+            Signal c = P("{\"msg\": " + d + ", \"t\": \"ended\", \"why\": \"reject\"}");
+            for (const Signal &s : { a, b, c }) all = all && s.kind == Kind::EVENT && s.ev == Ev::ENDED && s.why == Why::REJECT && s.msg == n;
+        }
+        CHECK("ended reject with msg 1..3 (compact, spaced, any key order)", all);
+    }
+    { Signal s = P("{\"t\":\"ended\",\"why\":\"reject\"}"); CHECK("ended reject without msg: 0 (old relay)", s.kind == Kind::EVENT && s.ev == Ev::ENDED && s.why == Why::REJECT && s.msg == 0); }
+    {   // 번호가 틀려도 거절은 거절이다 — 통화는 끝나고 번호만 0 이 된다(4~99 는 교환기가 건네는 범위지만 기기가 모르는 번호)
+        bool all = true;
+        for (const char *v : { "0", "4", "99", "100", "-1", "2.0", "1e0", "\"2\"", "true", "null", "4294967297", "4294967298", "99999999999999999999" }) {
+            Signal s = P(std::string("{\"t\":\"ended\",\"why\":\"reject\",\"msg\":") + v + "}");
+            const bool good = s.kind == Kind::EVENT && s.ev == Ev::ENDED && s.why == Why::REJECT && s.msg == 0;
+            if (!good) printf("       msg=%s -> kind=%d msg=%d\n", v, (int)s.kind, (int)s.msg);
+            all = all && good;
+        }
+        CHECK("wrong msg = plain reject, call still ends", all);
+    }
+    {
+        bool all = true;
+        for (const char *why : { "hangup", "timeout", "offline", "lost", "weird" }) {
+            Signal s = P(std::string("{\"t\":\"ended\",\"why\":\"") + why + "\",\"msg\":2}");
+            all = all && s.kind == Kind::EVENT && s.ev == Ev::ENDED && s.why != Why::REJECT && s.msg == 0;
+        }
+        Signal nowhy = P("{\"t\":\"ended\",\"msg\":2}");
+        CHECK("msg on a non-reject ended is dropped", all && nowhy.ev == Ev::ENDED && nowhy.msg == 0);
+    }
+    { Signal s = P("{\"t\":\"ring\",\"msg\":2}");          CHECK("msg on ring is dropped, ring still works", s.kind == Kind::EVENT && s.ev == Ev::RING && s.msg == 0); }
+    { Signal s = P("{\"t\":\"pong\",\"n\":7,\"msg\":2}");  CHECK("pong unaffected", s.kind == Kind::PONG && s.n == 7 && s.msg == 0); }
+    {
+        Signal a = P("{\"t\":\"ended\",\"why\":\"reject\",\"msg\":1,\"msg\":3}");
+        Signal b = P("{\"t\":\"ended\",\"why\":\"reject\",\"msg\":2,\"msg\":9}");
+        CHECK("repeated msg: last one wins", a.msg == 3 && b.ev == Ev::ENDED && b.msg == 0);
+    }
+    CHECK("msg as array/object drops the whole signal (existing rule)",
+          P("{\"t\":\"ended\",\"why\":\"reject\",\"msg\":[2]}").kind == Kind::NONE && P("{\"t\":\"ended\",\"why\":\"reject\",\"msg\":{\"a\":2}}").kind == Kind::NONE);
+    { Signal s = P("{\"t\":\"ended\",\"why\":\"reject\",\"x\":\"\\\"msg\\\":2\"}"); CHECK("msg inside a string value is not a key", s.kind == Kind::EVENT && s.ev == Ev::ENDED && s.why == Why::REJECT && s.msg == 0); }
+    {
+        char b[40]; bool all = true;
+        for (int n = 1; n <= 3; ++n) {
+            const int len = reject_text(b, sizeof b, (uint8_t)n);
+            all = all && len == (int)strlen(b) && std::string(b) == "{\"t\":\"reject\",\"msg\":" + std::to_string(n) + "}";
+        }
+        CHECK("reject text with number 1..3", all);
+        int len = reject_text(b, sizeof b, 0);
+        CHECK("reject text with 0 = plain", len == (int)strlen(b) && std::string(b) == text_for(callfsm::A_SEND_REJECT));
+        reject_text(b, sizeof b, 4); const std::string four = b;
+        len = reject_text(b, sizeof b, 255);
+        CHECK("reject text out-of-range = plain", four == "{\"t\":\"reject\"}" && std::string(b) == "{\"t\":\"reject\"}" && len == 14);
+        char small[10];
+        CHECK("reject text does not overflow a small buffer", reject_text(small, sizeof small, 2) == 0 && reject_text(small, sizeof small, 0) == 0);
+        CHECK("plain reject text unchanged", std::string(text_for(callfsm::A_SEND_REJECT)) == "{\"t\":\"reject\"}");
+        Signal back = P("{\"t\":\"ended\",\"why\":\"reject\",\"msg\":99}");                   // 가장 긴 새 프레임(37바이트)도 상한 256 안
+        CHECK("longest new frame fits the text limit", std::string("{\"t\":\"ended\",\"why\":\"reject\",\"msg\":99}").size() == 37 && back.kind == Kind::EVENT);
+    }
+
     printf("\n%d ok, %d failed\n", ok, bad);
     return bad ? 1 : 0;
 }
```

- [x] **Step 4: 신호 읽기·쓰기 — 구현 패치를 적용하고 통과를 확인한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-decline-mute-plan/draft && git apply "$D/patches/T2-04-relay-proto.patch" && cmp "$D/files/firmware/tab5_phone/main/relay_proto.h" firmware/tab5_phone/main/relay_proto.h && cmp "$D/files/firmware/tab5_phone/tools/test_relay_proto.cpp" firmware/tab5_phone/tools/test_relay_proto.cpp && cd firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_relay_proto.cpp -o /tmp/t_dm_relay_proto && /tmp/t_dm_relay_proto | tail -1
```
Expected: 컴파일 경고 없이 `61 ok, 0 failed` (기존 46 + 새 15)

```diff
--- a/firmware/tab5_phone/main/relay_proto.h
+++ b/firmware/tab5_phone/main/relay_proto.h
@@ -1,7 +1,7 @@
 // relay_proto.h — 교환기와 주고받는 글자·음성 프레임의 모양(순수 로직, 맥에서 단위 시험: tools/test_relay_proto.cpp).
 // 설계서 §9.4 신호 규약. 교환기 원본: server/phone_core.py.
 //   - 받은 글자(JSON 한 겹)를 call_fsm 사건으로 바꾼다. 깨진·모르는·너무 긴 입력은 Kind::NONE 으로 버린다(죽지 않는다).
-//   - JSON 라이브러리를 쓰지 않는다: 신호가 열쇠 4개(t, online, why, n)뿐이고, 맥에서 그대로 시험하려는 것(RP-D1).
+//   - JSON 라이브러리를 쓰지 않는다: 신호가 열쇠 5개(t, online, why, n, msg)뿐이고, 맥에서 그대로 시험하려는 것(RP-D1).
 //     비용: 겹친 객체·배열은 읽지 못한다(규약에 없음). 탈출구: 규약이 커지면 cJSON 으로 교체, 시험은 그대로 쓴다.
 #pragma once
 #include <cstddef>
@@ -21,6 +21,7 @@
     callfsm::Why why = callfsm::Why::NONE;
     bool flag = false;          // peer 의 online
     uint32_t n = 0;             // pong 의 n
+    uint8_t msg = 0;            // ended(why=reject) 의 거절 문구 번호(1~callfsm::MSG_COUNT). 없거나 범위 밖이면 0
 };
 
 const size_t MAX_TEXT = 256;            // 교환기 신호는 수십 바이트 — 이보다 길면 버린다(교환기 MAX_TEXT_BYTES 와 같은 값)
@@ -64,6 +65,7 @@
     char t[16] = "", why[16] = "";
     bool have_t = false, have_why = false, have_online = false, online = false, have_n = false;
     uint32_t n = 0;
+    uint8_t msg = 0;
 
     s.ws();
     if (s.p >= s.end || *s.p != '{') return none;
@@ -97,6 +99,7 @@
             }
             if (s.p == b) return none;
             if (key_plain && eq(key, "n") && digits_only && v <= 0xFFFFFFFFull) { have_n = true; n = (uint32_t)v; }
+            if (key_plain && eq(key, "msg")) msg = (digits_only && callfsm::msg_valid(v)) ? (uint8_t)v : 0;   // 틀린 번호는 0(번호 없는 거절). v 를 64비트 그대로 비교한다
         } else {
             return none;                                         // 겹친 객체·배열 등: 규약에 없다
         }
@@ -128,6 +131,7 @@
                 : eq(why, "offline")   ? callfsm::Why::OFFLINE
                 : eq(why, "lost")      ? callfsm::Why::LOST
                 :                        callfsm::Why::HANGUP;
+        if (out.why == callfsm::Why::REJECT) out.msg = msg;       // 문구 번호는 거절에만 붙는다(다른 종료에 붙어 온 번호는 버린다)
     } else if (eq(t, "pong")) {
         if (!have_n) return none;
         out.kind = Kind::PONG; out.n = n;
@@ -147,6 +151,15 @@
     }
 }
 
+// 거절 글자를 buf 에 쓴다. 번호가 1~MSG_COUNT 면 {"t":"reject","msg":N}, 아니면 번호 없는 {"t":"reject"}(text_for 와 같은 글자).
+// 돌려주는 값 = 글자 수(끝의 0 제외), 안 들어가면 0.
+inline int reject_text(char *buf, size_t cap, uint8_t msg)
+{
+    int len = callfsm::msg_valid(msg) ? snprintf(buf, cap, "{\"t\":\"reject\",\"msg\":%u}", (unsigned)msg)
+                                      : snprintf(buf, cap, "{\"t\":\"reject\"}");
+    return (len > 0 && (size_t)len < cap) ? len : 0;
+}
+
 // 생존 확인 글자를 buf 에 쓴다. 돌려주는 값 = 글자 수(끝의 0 제외), 안 들어가면 0.
 inline int ping_text(char *buf, size_t cap, uint32_t n)
 {
```

- [x] **Step 5: 화면 배치·판정 — 시험 패치를 적용하고 실패를 확인한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-decline-mute-plan/draft && git apply "$D/patches/T2-05-ui-layout-tests.patch" && git diff --numstat -- firmware/tab5_phone/tools/test_ui_layout.cpp && cd firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_ui_layout.cpp -o /tmp/t_dm_ui_layout 2>&1 | grep -m1 "error:"
```
Expected: `85	0	firmware/tab5_phone/tools/test_ui_layout.cpp`, 이어서 `test_ui_layout.cpp:127:43: error: no member named 'decline_open' in 'uilayout::Model'`

```diff
--- a/firmware/tab5_phone/tools/test_ui_layout.cpp
+++ b/firmware/tab5_phone/tools/test_ui_layout.cpp
@@ -120,6 +120,91 @@
         talk_clock(-5, &mm, &ss);       CHECK("clock negative -> 00:00", mm == 0 && ss == 0);
     }
 
+
+    // ════ 수신거부 메시지·벨 끄기 (설계서 decline-mute §5.4) ════
+    using callfsm::Notice;
+    const Model inc = M(State::INCOMING);
+    Model pick = M(State::INCOMING); pick.decline_open = true;
+    const State others[] = { State::OFFLINE, State::IDLE, State::DIALING, State::RINGBACK, State::ANSWERING, State::IN_CALL, State::ENDING };
+
+    // ── 메시지 고르기 화면은 INCOMING 위에 얹는 화면 쪽 상태 ──
+    CHECK("incoming + picker open = DECLINE screen", screen_for(inc) == Screen::INCOMING && screen_for(pick) == Screen::DECLINE && std::string(screen_name(Screen::DECLINE)) == "DECLINE");
+    {
+        bool all = true;
+        for (State s : others) { Model a = M(s), b = M(s); b.decline_open = true; all = all && screen_for(a) == screen_for(b) && screen_for(b) != Screen::DECLINE; }
+        CHECK("picker flag means nothing outside INCOMING", all);
+    }
+
+    // ── 새 버튼의 크기·격자·겹침 ──
+    {
+        const Rect news[] = { BTN_MUTE, BTN_DECLINE, BTN_MSG[0], BTN_MSG[1], BTN_MSG[2], BTN_BACK };
+        bool geo = true;
+        for (const Rect &r : news) geo = geo && r.w >= 96 && r.h >= 96 && r.x % 8 == 0 && r.y % 8 == 0 && r.w % 8 == 0 && r.h % 8 == 0 && inside_screen(r);
+        CHECK("(4) new buttons: >= 96px, on the 8px grid, inside the screen", geo && sizeof BTN_MSG / sizeof BTN_MSG[0] == callfsm::MSG_COUNT);
+        CHECK("(4) bell screen: the four buttons do not overlap", !overlap(BTN_MUTE, BTN_ACCEPT) && !overlap(BTN_MUTE, BTN_REJECT) && !overlap(BTN_DECLINE, BTN_ACCEPT)
+              && !overlap(BTN_DECLINE, BTN_REJECT) && !overlap(BTN_MUTE, BTN_DECLINE) && BTN_DECLINE.x - (BTN_MUTE.x + BTN_MUTE.w) >= 96);
+        CHECK("(4) picker screen: the four buttons do not overlap", !overlap(BTN_MSG[0], BTN_MSG[1]) && !overlap(BTN_MSG[1], BTN_MSG[2]) && !overlap(BTN_MSG[0], BTN_MSG[2])
+              && !overlap(BTN_BACK, BTN_MSG[0]) && !overlap(BTN_BACK, BTN_MSG[1]) && !overlap(BTN_BACK, BTN_MSG[2]));
+        bool r1 = !overlap(BTN_DECLINE, BTN_BACK), r2 = true;
+        for (int i = 0; i < callfsm::MSG_COUNT; ++i) {
+            r1 = r1 && !overlap(BTN_DECLINE, BTN_MSG[i]);
+            r2 = r2 && !overlap(BTN_MSG[i], box(BTN_CALL)) && !overlap(BTN_MSG[i], BTN_WIFI) && !overlap(BTN_MSG[i], BTN_ACCEPT) && !overlap(BTN_MSG[i], BTN_REJECT)
+                    && !overlap(BTN_MSG[i], BTN_MUTE) && !overlap(BTN_MSG[i], BTN_DECLINE) && !overlap(BTN_MSG[i], BTN_CANCEL) && !overlap(BTN_MSG[i], BTN_HANGUP);
+        }
+        CHECK("(1) open-picker overlaps no picker button (open then instant message)", r1);
+        CHECK("(2) message buttons overlap no button of any call screen (picking while the caller cancels)", r2 && BTN_MSG[0].y + BTN_MSG[0].h <= BTN_CALL.cy - BTN_CALL.r);
+        CHECK("(2) mute/open-picker/back do not overlap the call button (screen turns idle under the finger)", !overlap(BTN_MUTE, box(BTN_CALL)) && !overlap(BTN_DECLINE, box(BTN_CALL)) && !overlap(BTN_BACK, box(BTN_CALL)));
+        CHECK("(3) back overlaps no accept/reject/open-picker (back then instant accept or reject)", !overlap(BTN_BACK, BTN_ACCEPT) && !overlap(BTN_BACK, BTN_REJECT) && !overlap(BTN_BACK, BTN_DECLINE));
+    }
+
+    // ── 터치 → 버튼 (가운데 좌표는 기기 시험이 주입하는 값과 같다) ──
+    {
+        CHECK("incoming: accept, reject, mute, open-picker", hit(inc, 404, 428) == Tap::ACCEPT && hit(inc, 876, 428) == Tap::REJECT && hit(inc, 404, 624) == Tap::MUTE && hit(inc, 876, 624) == Tap::DECLINE_OPEN);
+        CHECK("incoming: gap between mute and open-picker", hit(inc, 640, 624) == Tap::NONE);
+        Model muted = inc; muted.bell_muted = true;
+        CHECK("incoming + muted: the mute position is no longer a button", hit(muted, 404, 624) == Tap::NONE && hit(muted, BTN_MUTE.x + 1, BTN_MUTE.y + 1) == Tap::NONE);
+        CHECK("incoming + muted: the other three still work", hit(muted, 404, 428) == Tap::ACCEPT && hit(muted, 876, 428) == Tap::REJECT && hit(muted, 876, 624) == Tap::DECLINE_OPEN);
+        CHECK("incoming: message positions do nothing", hit(inc, 240, 168) == Tap::NONE && hit(inc, 640, 168) == Tap::NONE && hit(inc, 1040, 168) == Tap::NONE);
+        CHECK("picker: three messages and back", hit(pick, 240, 168) == Tap::MSG1 && hit(pick, 640, 168) == Tap::MSG2 && hit(pick, 1040, 168) == Tap::MSG3 && hit(pick, 404, 624) == Tap::DECLINE_BACK);
+        CHECK("picker: gaps between messages", hit(pick, 440, 168) == Tap::NONE && hit(pick, 840, 168) == Tap::NONE && hit(pick, 640, 100) == Tap::NONE && hit(pick, 640, 232) == Tap::NONE);
+        CHECK("picker: accept/reject/open-picker positions do nothing", hit(pick, 404, 428) == Tap::NONE && hit(pick, 876, 428) == Tap::NONE && hit(pick, 876, 624) == Tap::NONE);
+        Model pm = pick; pm.bell_muted = true;
+        CHECK("picker + muted: back still works", hit(pm, 404, 624) == Tap::DECLINE_BACK && hit(pm, 640, 168) == Tap::MSG2);
+        bool none = true;
+        const int pts[][2] = { { 240, 168 }, { 640, 168 }, { 1040, 168 }, { 404, 624 }, { 876, 624 } };
+        for (State s : { State::IDLE, State::ANSWERING, State::ENDING, State::OFFLINE }) {
+            Model m = M(s); m.decline_open = true; m.bell_muted = true;           // 남아 있는 화면 쪽 값이 있어도
+            for (const auto &p : pts) none = none && hit(m, p[0], p[1]) == Tap::NONE;
+        }
+        for (int i = 0; i < 3; ++i) none = none && hit(M(State::IN_CALL), pts[i][0], pts[i][1]) == Tap::NONE && hit(M(State::DIALING), pts[i][0], pts[i][1]) == Tap::NONE;
+        CHECK("idle/in-call/ending and the rest: new positions do nothing", none);
+    }
+
+    // ── 순수 로직 넷: msg_of, DeclinePicker, notice_ms, notice_visible ──
+    CHECK("msg_of", msg_of(Tap::MSG1) == 1 && msg_of(Tap::MSG2) == 2 && msg_of(Tap::MSG3) == 3 && msg_of(Tap::REJECT) == 0 && msg_of(Tap::NONE) == 0 && msg_of(Tap::DECLINE_BACK) == 0 && msg_of(Tap::MUTE) == 0);
+    {
+        DeclinePicker p;
+        p.on_tap(Tap::ACCEPT); p.on_tap(Tap::REJECT); p.on_tap(Tap::MUTE); p.on_tap(Tap::MSG1); p.on_tap(Tap::DECLINE_BACK);
+        CHECK("picker: other taps do not open it", !p.open);
+        p.on_tap(Tap::DECLINE_OPEN); p.sync(State::INCOMING); p.on_tap(Tap::MUTE);
+        CHECK("picker: opens, stays open while INCOMING", p.open);
+        p.on_tap(Tap::DECLINE_BACK);
+        CHECK("picker: back closes", !p.open);
+        bool closes = true;
+        for (State s : others) { DeclinePicker q; q.on_tap(Tap::DECLINE_OPEN); q.sync(s); closes = closes && !q.open; }
+        CHECK("picker: closes when the call leaves INCOMING", closes);
+        p.on_tap(Tap::DECLINE_OPEN); p.sync(State::IDLE); p.sync(State::INCOMING);          // 한 바퀴 안에 ended → ring
+        CHECK("picker: ended then ring -> the next call starts on the bell screen", !p.open);
+    }
+    CHECK("notice time: message 10s, others 4s", notice_ms(Notice::REJECT_MSG) == 10000 && notice_ms(Notice::REJECTED) == 4000 && notice_ms(Notice::MISSED) == 4000
+          && notice_ms(Notice::PEER_OFF) == 4000 && notice_ms(Notice::NO_ANSWER) == 4000 && notice_ms(Notice::LOST) == 4000 && notice_ms(Notice::CALL_ENDED) == 4000 && notice_ms(Notice::RELAY_SILENT) == 4000);
+    {
+        Model on_bell = inc; on_bell.notice = Notice::REJECT_MSG; on_bell.notice_msg = 2;
+        Model on_pick = pick; on_pick.notice = Notice::REJECT_MSG; on_pick.notice_msg = 2;
+        Model on_idle = M(State::IDLE); on_idle.notice = Notice::MISSED; on_idle.decline_open = true;
+        CHECK("notice box hidden on the picker screen only", notice_visible(on_bell) && !notice_visible(on_pick) && notice_visible(on_idle) && !notice_visible(inc) && !notice_visible(pick));
+    }
+
     printf("\n%d ok, %d failed\n", ok, bad);
     return bad ? 1 : 0;
 }
```

- [x] **Step 6: 화면 배치·판정 — 구현 패치를 적용하고 통과를 확인한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-decline-mute-plan/draft && git apply "$D/patches/T2-06-ui-layout.patch" && cmp "$D/files/firmware/tab5_phone/main/ui_layout.h" firmware/tab5_phone/main/ui_layout.h && cmp "$D/files/firmware/tab5_phone/tools/test_ui_layout.cpp" firmware/tab5_phone/tools/test_ui_layout.cpp && cd firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_ui_layout.cpp -o /tmp/t_dm_ui_layout && /tmp/t_dm_ui_layout | tail -1
```
Expected: 컴파일 경고 없이 `86 ok, 0 failed` (기존 59 + 새 27)

```diff
--- a/firmware/tab5_phone/main/ui_layout.h
+++ b/firmware/tab5_phone/main/ui_layout.h
@@ -5,6 +5,9 @@
 // UL-D1: 화면이 바뀐 직후 같은 자리를 또 눌러도 다른 버튼이 눌리지 않게 "받기/거절"과 "끊기", "통화"와 "취소"를 겹치지 않게 배치했다.
 //        Phase 0 에서 세 번 누른 터치가 7회로 기록된 일이 있어(원인 미확정), 배치 + 시간 가드(TapGuard) 두 겹으로 막는다.
 //        비용: 받기/거절 버튼이 화면 위쪽으로 올라간다. 탈출구: 원인이 터치 칩 읽기 오류로 확정되면 가드만 남기고 배치를 내려도 된다.
+// 수신거부 메시지·벨 끄기(docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md §5.4): 벨 화면 아래 줄에 벨 끄기·메시지로 거절,
+//        메시지 고르기 화면(Screen::DECLINE)은 통화 상태 INCOMING 위에 얹는 화면 쪽 상태다(DeclinePicker, DM-D8).
+//        신호를 보내는 새 버튼은 문구 셋뿐이고, 통화 쪽 어느 화면의 버튼과도 겹치지 않게 위쪽 한 줄에 둔다(DM-D3 — 시험이 고정한다).
 #pragma once
 #include <cstdint>
 
@@ -14,8 +17,9 @@
 
 namespace uilayout {
 
-enum class Screen : uint8_t { BOOT, IDLE, DIALING, INCOMING, ANSWERING, IN_CALL, ENDING };
-enum class Tap : uint8_t { NONE, CALL, CANCEL, ACCEPT, REJECT, HANGUP, WIFI };
+enum class Screen : uint8_t { BOOT, IDLE, DIALING, INCOMING, ANSWERING, IN_CALL, ENDING, DECLINE };
+enum class Tap : uint8_t { NONE, CALL, CANCEL, ACCEPT, REJECT, HANGUP, WIFI, MUTE, DECLINE_OPEN, DECLINE_BACK, MSG1, MSG2, MSG3 };   // MSG1~3 은 연달아 둔다
+static_assert((int)Tap::MSG3 - (int)Tap::MSG1 + 1 == callfsm::MSG_COUNT, "Tap::MSG1..MSG3 must match callfsm::MSG_COUNT");
 
 struct Circle { int cx, cy, r; };
 
@@ -34,6 +38,9 @@
     int             relay_code = 0;                         // 교환기가 거절한 HTTP 코드(401 = 기기 등록 오류), 없으면 0
     int             relay_attempt = 0;                      // 교환기 연결 연속 실패 횟수
     int             unit = 0;                               // 1, 2, 미등록이면 0
+    bool            bell_muted = false;                     // 이번 전화의 벨을 껐다(call_fsm 의 값)
+    bool            decline_open = false;                   // 메시지 고르기 화면이 열려 있다(INCOMING 일 때만 뜻이 있다)
+    uint8_t         notice_msg = 0;                         // notice == REJECT_MSG 일 때 보여 줄 문구 번호
 };
 
 // 버튼 위치(1280×720, 8px 격자)
@@ -43,12 +50,21 @@
 const Rect   BTN_ACCEPT = { 224, 328, 360, 200 };
 const Rect   BTN_REJECT = { 696, 328, 360, 200 };
 const Rect   BTN_HANGUP = { 464, 536, 360, 160 };
+const Rect   BTN_MUTE    = { 224, 568, 360, 112 };         // 벨 화면 아래 줄 왼쪽(받기 아래). 끈 뒤에는 "벨 꺼짐" 표시일 뿐 버튼이 아니다
+const Rect   BTN_DECLINE = { 696, 568, 360, 112 };         // 벨 화면 아래 줄 오른쪽(거절 아래): 메시지 고르기 화면을 연다
+const Rect   BTN_MSG[]   = { { 48, 104, 384, 128 }, { 448, 104, 384, 128 }, { 848, 104, 384, 128 } };   // 고르기 화면 위쪽 한 줄: 문구 1·2·3
+const Rect   BTN_BACK    = { 224, 568, 360, 112 };         // 고르기 화면 아래 왼쪽(벨 끄기와 같은 자리)
+static_assert(sizeof BTN_MSG / sizeof BTN_MSG[0] == callfsm::MSG_COUNT, "BTN_MSG must have callfsm::MSG_COUNT entries");
 
 const int64_t TAP_SCREEN_GUARD_MS = 600;                    // 화면이 바뀐 직후 이만큼은 터치를 버린다
 const int64_t TAP_REPEAT_GUARD_MS = 400;                    // 직전에 받아들인 터치 뒤 이만큼은 버린다
 const int64_t KEY_SCREEN_GUARD_MS = 300;                    // Wi-Fi 설정 화면(목록·자판)용 — 글쇠를 빨리 치므로 짧게
 const int64_t KEY_REPEAT_GUARD_MS = 150;
+const int64_t NOTICE_MS     = 4000;                         // 안내를 띄워 두는 시간
+const int64_t NOTICE_MSG_MS = 10000;                        // 상대가 보낸 거절 문구는 읽을 시간을 더 준다(DM-D10)
 
+inline int64_t notice_ms(callfsm::Notice n) { return n == callfsm::Notice::REJECT_MSG ? NOTICE_MSG_MS : NOTICE_MS; }
+
 inline Screen screen_for(const Model &m)
 {
     switch (m.fsm) {
@@ -56,7 +72,7 @@
     case callfsm::State::IDLE:      return Screen::IDLE;
     case callfsm::State::DIALING:
     case callfsm::State::RINGBACK:  return Screen::DIALING;
-    case callfsm::State::INCOMING:  return Screen::INCOMING;
+    case callfsm::State::INCOMING:  return m.decline_open ? Screen::DECLINE : Screen::INCOMING;   // 다른 상태에서는 decline_open 을 보지 않는다
     case callfsm::State::ANSWERING: return Screen::ANSWERING;
     case callfsm::State::IN_CALL:   return Screen::IN_CALL;
     case callfsm::State::ENDING:    return Screen::ENDING;
@@ -69,7 +85,7 @@
     switch (s) {
     case Screen::BOOT: return "BOOT"; case Screen::IDLE: return "IDLE"; case Screen::DIALING: return "DIALING";
     case Screen::INCOMING: return "INCOMING"; case Screen::ANSWERING: return "ANSWERING"; case Screen::IN_CALL: return "IN_CALL";
-    case Screen::ENDING: return "ENDING";
+    case Screen::ENDING: return "ENDING"; case Screen::DECLINE: return "DECLINE";
     }
     return "?";
 }
@@ -96,7 +112,13 @@
     case Screen::INCOMING:
         if (BTN_ACCEPT.hit(x, y)) return Tap::ACCEPT;
         if (BTN_REJECT.hit(x, y)) return Tap::REJECT;
+        if (BTN_MUTE.hit(x, y)) return m.bell_muted ? Tap::NONE : Tap::MUTE;      // 이미 껐으면 버튼이 아니다(상태 표시)
+        if (BTN_DECLINE.hit(x, y)) return Tap::DECLINE_OPEN;
         break;
+    case Screen::DECLINE:                                   // 문구 셋과 뒤로만 버튼이다(받으려면 뒤로)
+        for (int i = 0; i < callfsm::MSG_COUNT; ++i) if (BTN_MSG[i].hit(x, y)) return (Tap)((int)Tap::MSG1 + i);
+        if (BTN_BACK.hit(x, y)) return Tap::DECLINE_BACK;
+        break;
     case Screen::IN_CALL:
         if (BTN_HANGUP.hit(x, y)) return Tap::HANGUP;
         break;
@@ -107,6 +129,21 @@
     return Tap::NONE;
 }
 
+// 문구 버튼 → 문구 번호(1~MSG_COUNT). 문구 버튼이 아니면 0.
+inline uint8_t msg_of(Tap t) { return (t >= Tap::MSG1 && t <= Tap::MSG3) ? (uint8_t)((int)t - (int)Tap::MSG1 + 1) : 0; }
+
+// 안내 상자를 그릴까. 고르기 화면에서는 문구 버튼 줄(y 104~232)을 가리므로 그리지 않는다(DM-D12 — 안내 시간은 계속 간다).
+inline bool notice_visible(const Model &m) { return m.notice != callfsm::Notice::NONE && screen_for(m) != Screen::DECLINE; }
+
+// 메시지 고르기 화면의 열림 상태(화면 쪽 상태 — 통화 상태 기계는 INCOMING 그대로다).
+struct DeclinePicker {
+    bool open = false;
+    void on_tap(Tap t) { if (t == Tap::DECLINE_OPEN) open = true; else if (t == Tap::DECLINE_BACK) open = false; }
+    // INCOMING 을 떠나면 닫는다. call_fsm 에 사건을 넣을 때마다 부른다 — 화면을 그릴 때만 맞추면 한 바퀴 안에 ended → ring 이 연달아 올 때
+    // 다음 전화가 고르기 화면으로 뜬다.
+    void sync(callfsm::State s) { if (s != callfsm::State::INCOMING) open = false; }
+};
+
 struct TapGuard {
     int64_t last_ms = -1000000;
     // 이 터치를 받아들일까. screen_since_ms = 지금 화면이 뜬 시각.
```

- [x] **Step 7: 다섯 개를 모두 다시 돌린다** (헤더가 서로를 포함하므로 — 기존 개수가 줄지 않았는지)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone/tools && for t in call_fsm relay_proto ui_layout device_id wifi_store; do clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_$t.cpp -o /tmp/t_dm_$t && echo "$t: $(/tmp/t_dm_$t | tail -1)"; done
```
Expected:
```
call_fsm: 105 ok, 0 failed
relay_proto: 61 ok, 0 failed
ui_layout: 86 ok, 0 failed
device_id: 9 ok, 0 failed
wifi_store: 16 passed, 0 failed
```
(경고가 한 줄이라도 찍히면 통과가 아니다.)

- [x] **Step 8: 커밋**

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/main/call_fsm.h firmware/tab5_phone/main/relay_proto.h firmware/tab5_phone/main/ui_layout.h firmware/tab5_phone/tools/test_call_fsm.cpp firmware/tab5_phone/tools/test_relay_proto.cpp firmware/tab5_phone/tools/test_ui_layout.cpp && git commit -m "feat(phone): 판단 로직 — 벨 끄기·메시지 거절(call_fsm·relay_proto·ui_layout)" -m "화면·진행(ui.cpp·app.cpp)은 뒤 커밋에서 맞춘다 — 그 전까지 펌웨어 전체 빌드는 -Werror=switch 로 실패한다." -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: 가짜 상대 도구와 검증 도구 주석

**Files:**
- Modify: `server/tools/phone_fake_peer.py`, `firmware/tab5_phone/tools/phone_ctl.py`(머리 주석 한 줄)
- Create: `server/tests/test_phone_fake_peer.py`

**Interfaces:**
- Consumes: Task 1 의 교환기(같은 프로세스의 시험용 중계에 `run()` 을 붙인다), `test_phone_relay.py` 의 `relay`·`connect`·`rx`·`A`·`B`·`TOK`.
- Produces:
  - 단계 문법: `send:reject:<정수>`(범위를 검사하지 않는다), `expect:ended:reject:<정수>`(`0` = `msg` 가 없어야 함, 번호를 안 쓰면 `msg` 를 보지 않음), `@<초>` 는 맨 뒤 그대로. 번호는 `reject` 에만, 정수(`-?\d+`)가 아니면 `ValueError`.
  - `parse_step()` 이 돌려주는 모양: `("send", 신호, 번호 | None)`, `("expect", t, why | None, 번호 | None, 초)`. `matches(msg, t, why, num=None)`.
  - `phone_ctl.py`: 코드는 그대로, 머리 주석에 새 좌표 한 줄.

- [x] **Step 1: 시험 파일을 만들고 실패를 확인한다** (약 11초 — 기다리다 시간이 다 되는 시험이 있다)

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-decline-mute-plan/draft && git apply "$D/patches/T3-01-fake-peer-tests.patch" && cmp "$D/files/server/tests/test_phone_fake_peer.py" server/tests/test_phone_fake_peer.py && cd server && PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider tests/test_phone_fake_peer.py 2>&1 | grep -E "^[0-9]+ (passed|failed)"
```
Expected: `12 failed, 15 passed in …s` (실패: `test_send_steps`, `test_expect_steps`, `test_expect_step_errors` 6개, `test_matches`, `test_fake_peer_rejects_through_the_relay` 3개)

```diff
--- /dev/null
+++ b/server/tests/test_phone_fake_peer.py
@@ -0,0 +1,123 @@
+"""phone_fake_peer(가짜 상대 도구) 시험 — 단계 문법, 받은 신호 판정, 그리고 시험용 중계(같은 프로세스)에 run() 을 붙인 실제 주고받기.
+실행: cd server && python3 -m pytest tests/test_phone_fake_peer.py -q
+수신거부 메시지(docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md §5.7, DM-D14): send:reject:<n>, expect:ended:reject:<n>(0 = msg 가 없어야 함).
+"""
+import asyncio
+import sys
+from pathlib import Path
+
+import pytest
+
+sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
+sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
+import phone_fake_peer as fp
+from test_phone_relay import A, B, TOK, connect, relay, rx  # noqa: F401 — relay 는 fixture
+
+
+# ── 단계 문법 ───────────────────────────────────────────────────────────────
+def test_send_steps():
+    assert fp.parse_step("send:call") == ("send", "call", None)
+    assert fp.parse_step("send:reject") == ("send", "reject", None)
+    assert fp.parse_step("send:reject:2") == ("send", "reject", 2)
+    for n in (0, 100, -1):                                   # 범위 검사는 교환기 몫 — 틀린 번호도 보낼 수 있어야 교환기를 시험한다
+        assert fp.parse_step(f"send:reject:{n}") == ("send", "reject", n)
+
+
+@pytest.mark.parametrize("step", ["send:call:2", "send:accept:1", "send:hangup:1", "send:dance", "send:", "send:reject:", "send:reject:x",
+                                  "send:reject:2.0", "send:reject:1:2", "send:reject:1_0", "send:reject: 2"])
+def test_send_step_errors(step):
+    """번호는 reject 에만 붙고, 정수가 아니면 단계 오류다."""
+    with pytest.raises(ValueError):
+        fp.parse_step(step)
+
+
+def test_expect_steps():
+    assert fp.parse_step("expect:ring") == ("expect", "ring", None, None, 10.0)
+    assert fp.parse_step("expect:peer") == ("expect", "peer", None, None, 10.0)
+    assert fp.parse_step("expect:ended:hangup") == ("expect", "ended", "hangup", None, 10.0)
+    assert fp.parse_step("expect:ended:timeout@40") == ("expect", "ended", "timeout", None, 40.0)
+    assert fp.parse_step("expect:ended:reject") == ("expect", "ended", "reject", None, 10.0)
+    assert fp.parse_step("expect:ended:reject:2") == ("expect", "ended", "reject", 2, 10.0)
+    assert fp.parse_step("expect:ended:reject:0@15") == ("expect", "ended", "reject", 0, 15.0)
+    assert fp.parse_step("expect:ended:reject:2@15") == ("expect", "ended", "reject", 2, 15.0)
+
+
+@pytest.mark.parametrize("step", ["expect:", "expect:ended:reject:two", "expect:ended:reject:", "expect:ended:reject:2.5", "expect:ended:hangup:2",
+                                  "expect:ring:x:1", "expect:ended:reject:1:2"])
+def test_expect_step_errors(step):
+    with pytest.raises(ValueError):
+        fp.parse_step(step)
+
+
+def test_other_steps_unchanged():
+    assert fp.parse_step("sleep:1.5") == ("sleep", 1.5)
+    assert fp.parse_step("voice:3") == ("voice", 3)
+    with pytest.raises(ValueError):
+        fp.parse_step("dance:1")
+
+
+# ── 받은 신호 판정 ──────────────────────────────────────────────────────────
+def test_matches():
+    with_msg, plain = {"t": "ended", "why": "reject", "msg": 2}, {"t": "ended", "why": "reject"}
+    assert fp.matches(with_msg, "ended", "reject") and fp.matches(plain, "ended", "reject")       # 번호를 안 쓰면 보지 않는다(지금까지의 동작)
+    assert fp.matches(with_msg, "ended", None) and fp.matches({"t": "ring"}, "ring", None)
+    assert fp.matches(with_msg, "ended", "reject", 2) and not fp.matches(with_msg, "ended", "reject", 3)
+    assert not fp.matches(plain, "ended", "reject", 2)                                             # 번호를 기다리는데 번호 없는 거절이 오면 맞지 않는다
+    assert fp.matches(plain, "ended", "reject", 0) and not fp.matches(with_msg, "ended", "reject", 0)   # 0 = msg 가 없어야 한다
+    assert not fp.matches({"t": "ended", "why": "reject", "msg": "2"}, "ended", "reject", 2)       # 글자 "2" 는 2 가 아니다
+    assert not fp.matches({"t": "ended", "why": "reject", "msg": True}, "ended", "reject", 1)      # 참은 1 이 아니다
+    assert not fp.matches(with_msg, "ended", "hangup", 2) and not fp.matches("x", "ended", None) and not fp.matches(None, "ended", None, 0)
+
+
+# ── 시험용 중계에 붙여 실제로 주고받는다 ────────────────────────────────────
+def start_fake(client, monkeypatch, dev, steps):
+    """가짜 상대를 시험용 중계에 dev 로 붙인다. 토큰은 시험용 값만 쓴다(server/.env.phone 을 읽지 않게 막는다)."""
+    monkeypatch.setattr(fp.phone_relay, "load_env_file", lambda path: None)
+    monkeypatch.setenv("PHONE_TOKENS", f"{A}:{TOK[A]},{B}:{TOK[B]}")
+    lines = []
+    base = str(client.server.make_url("")).rstrip("/")
+    return asyncio.ensure_future(fp.run(base, dev, steps, out=lines.append)), lines
+
+
+@pytest.mark.parametrize("tail, want", [
+    (["send:reject:2", "expect:ended:reject:2"], {"t": "ended", "why": "reject", "msg": 2}),     # 번호가 든 거절이 양쪽에 간다
+    (["send:reject:100", "expect:ended:reject:0"], {"t": "ended", "why": "reject"}),             # 틀린 번호 → 교환기가 번호 없는 거절로 낮춘다
+    (["send:reject", "expect:ended:reject:0"], {"t": "ended", "why": "reject"}),                 # 그냥 거절(지금까지의 동작)
+])
+async def test_fake_peer_rejects_through_the_relay(relay, monkeypatch, tail, want):
+    client, _, _ = relay
+    a = await connect(client, A)
+    assert await rx(a) == {"t": "peer", "online": False}
+    fake, lines = start_fake(client, monkeypatch, B, ["expect:ring"] + tail)
+    assert await rx(a) == {"t": "peer", "online": True}
+    await a.send_json({"t": "call"})
+    assert await rx(a) == {"t": "ringing"}
+    assert await rx(a) == want                                # 진짜 기기 자리(A)가 받는 종료 신호
+    await asyncio.wait_for(fake, 5)
+    assert lines[-1].endswith("FAKE OK")
+    assert not any(TOK[A] in ln or TOK[B] in ln for ln in lines)          # 토큰은 출력하지 않는다
+
+
+async def test_fake_peer_expect_with_a_number_fails_on_a_plain_reject(relay, monkeypatch):
+    """기대 판정이 느슨하면 기기 시험이 거짓 통과한다 — 번호 2 를 기다리는데 번호 없는 거절이 오면 실패해야 한다."""
+    client, _, _ = relay
+    a = await connect(client, A)
+    assert await rx(a) == {"t": "peer", "online": False}
+    fake, _ = start_fake(client, monkeypatch, B, ["send:call", "expect:ringing", "expect:ended:reject:2@1"])
+    assert await rx(a) == {"t": "peer", "online": True}
+    assert await rx(a) == {"t": "ring"}
+    await a.send_json({"t": "reject"})                        # 번호 없는 거절
+    with pytest.raises(AssertionError, match="ended:reject:2"):
+        await asyncio.wait_for(fake, 5)
+
+
+async def test_fake_peer_expect_zero_fails_when_a_number_arrives(relay, monkeypatch):
+    client, _, _ = relay
+    a = await connect(client, A)
+    assert await rx(a) == {"t": "peer", "online": False}
+    fake, _ = start_fake(client, monkeypatch, B, ["send:call", "expect:ringing", "expect:ended:reject:0@1"])
+    assert await rx(a) == {"t": "peer", "online": True}
+    assert await rx(a) == {"t": "ring"}
+    await a.send_json({"t": "reject", "msg": 3})
+    with pytest.raises(AssertionError, match="ended:reject:0"):
+        await asyncio.wait_for(fake, 5)
```

- [x] **Step 2: 구현 패치를 적용하고 통과를 확인한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-decline-mute-plan/draft && git apply "$D/patches/T3-02-fake-peer.patch" && cmp "$D/files/server/tools/phone_fake_peer.py" server/tools/phone_fake_peer.py && cd server && PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider tests/test_phone_fake_peer.py 2>&1 | tail -1 && PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider 2>&1 | tail -1
```
Expected: `27 passed in …s`, 이어서 `137 passed in …s` (87 + 23 + 27)

```diff
--- a/server/tools/phone_fake_peer.py
+++ b/server/tools/phone_fake_peer.py
@@ -5,8 +5,10 @@
   python3 tools/phone_fake_peer.py --base http://127.0.0.1:5095 tab5-1 <단계> ...
 단계:
   send:<call|accept|reject|hangup>     신호 보내기
+  send:reject:<정수>                    거절에 문구 번호(msg)를 실어 보낸다. 범위를 검사하지 않는다 — 틀린 번호(0, 100, -1)도 보낼 수 있어야 교환기를 시험한다
   expect:<t>[:<why>]                    그 신호가 올 때까지 기다림(기본 10초). 예: expect:ring, expect:ended:hangup, expect:peer
-  expect:<t>[:<why>]@<초>               기다리는 시간 지정. 예: expect:ended:timeout@40
+  expect:ended:reject:<정수>            msg 가 그 수와 같은 거절 종료를 기다림. 0 은 "msg 가 없어야 한다". 번호를 안 쓰면 msg 를 보지 않는다
+  expect:<t>[:<why>[:<정수>]]@<초>      기다리는 시간 지정(맨 뒤). 예: expect:ended:timeout@40, expect:ended:reject:2@15
   sleep:<초>                            기다림(그동안 온 신호는 출력하고 버린다)
   voice:<프레임수>                      20ms 간격으로 음성 흉내 프레임([순번 2][0 × 60])을 보낸다(Phase 3 시험용)
 연결을 유지하는 동안 10초마다 ping 을 보낸다(교환기는 45초 무소식이면 끊는다). 토큰은 server/.env.phone 에서 읽고 출력하지 않는다.
@@ -16,6 +18,7 @@
 import asyncio
 import json
 import os
+import re
 import sys
 import time
 from pathlib import Path
@@ -33,15 +36,21 @@
     """단계 글자 → (종류, 인자…). 잘못된 단계는 ValueError."""
     kind, _, rest = step.partition(":")
     if kind == "send":
-        if rest not in SIGNALS:
+        sig, sep, num = rest.partition(":")
+        if sig not in SIGNALS:
             raise ValueError(f"send 는 {SIGNALS} 중 하나: {step}")
-        return ("send", rest)
+        if sep and sig != "reject":
+            raise ValueError(f"문구 번호는 reject 에만 붙인다: {step}")
+        return ("send", sig, _msg_num(num, step) if sep else None)
     if kind == "expect":
         body, _, secs = rest.partition("@")
-        t, _, why = body.partition(":")
+        t, _, tail = body.partition(":")
+        why, sep, num = tail.partition(":")
         if not t:
             raise ValueError(f"expect 에 신호 이름이 없음: {step}")
-        return ("expect", t, why or None, float(secs) if secs else 10.0)
+        if sep and (t, why) != ("ended", "reject"):
+            raise ValueError(f"문구 번호는 expect:ended:reject 에만 붙인다: {step}")
+        return ("expect", t, why or None, _msg_num(num, step) if sep else None, float(secs) if secs else 10.0)
     if kind == "sleep":
         return ("sleep", float(rest))
     if kind == "voice":
@@ -49,10 +58,26 @@
     raise ValueError(f"모르는 단계: {step}")
 
 
-def matches(msg, t, why):
-    return isinstance(msg, dict) and msg.get("t") == t and (why is None or msg.get("why") == why)
+def _msg_num(text, step):
+    """문구 번호 글자 → 정수. 정수가 아니면 단계 오류(범위는 보지 않는다)."""
+    if not re.fullmatch(r"-?\d+", text, re.ASCII):
+        raise ValueError(f"문구 번호는 정수: {step}")
+    return int(text)
 
 
+def matches(msg, t, why, num=None):
+    """받은 신호가 기다리던 것인가. num: None = msg 를 보지 않음(지금까지의 동작), 0 = msg 가 없어야 함, 그 밖 = msg 가 그 수와 같아야 함.
+    DM-D14: 판정이 느슨하면(번호를 안 보면) 번호가 빠진 거절에도 기기 시험이 통과해 버린다."""
+    if not (isinstance(msg, dict) and msg.get("t") == t and (why is None or msg.get("why") == why)):
+        return False
+    if num is None:
+        return True
+    if num == 0:
+        return "msg" not in msg
+    got = msg.get("msg")
+    return isinstance(got, int) and not isinstance(got, bool) and got == num
+
+
 async def run(base, dev, steps, out=print):
     phone_relay.load_env_file(Path(__file__).resolve().parent.parent / ".env.phone")
     tokens = phone_core.parse_tokens(os.environ.get("PHONE_TOKENS", ""))
@@ -97,16 +122,16 @@
             try:
                 for st in plan:
                     if st[0] == "send":
-                        await ws.send_json({"t": st[1]})
-                        out(f"{stamp()} → {st[1]}")
+                        await ws.send_json({"t": st[1]} if st[2] is None else {"t": st[1], "msg": st[2]})
+                        out(f"{stamp()} → {st[1]}" + ("" if st[2] is None else f" msg={st[2]}"))
                     elif st[0] == "expect":
-                        _, t, why, secs = st
+                        _, t, why, num, secs = st
                         deadline = time.monotonic() + secs
                         while True:
                             msg = await recv(deadline)
                             if msg is None:
-                                raise AssertionError(f"{secs:g}초 안에 {t}{':' + why if why else ''} 가 오지 않음")
-                            if matches(msg, t, why):
+                                raise AssertionError(f"{secs:g}초 안에 {t}{':' + why if why else ''}{'' if num is None else f':{num}'} 가 오지 않음")
+                            if matches(msg, t, why, num):
                                 break
                     elif st[0] == "sleep":
                         deadline = time.monotonic() + st[1]
```

- [x] **Step 3: `phone_ctl.py` 머리 주석에 새 좌표 한 줄을 넣고, 도구 시험이 그대로인지 확인한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-decline-mute-plan/draft && git apply "$D/patches/T3-03-phone-ctl-comment.patch" && cmp "$D/files/firmware/tab5_phone/tools/phone_ctl.py" firmware/tab5_phone/tools/phone_ctl.py && git diff --numstat -- firmware/tab5_phone/tools/phone_ctl.py && cd firmware/tab5_phone/tools && PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider test_phone_ctl.py 2>&1 | tail -1
```
Expected: `1	0	firmware/tab5_phone/tools/phone_ctl.py`, `150 passed in …s` (주석의 "예시" 블록은 시험이 글자대로 고정한다 — 그 블록 밖에 한 줄만 더했다)

```diff
--- a/firmware/tab5_phone/tools/phone_ctl.py
+++ b/firmware/tab5_phone/tools/phone_ctl.py
@@ -45,6 +45,7 @@
   순서대로: 둘 다 대기+상대 보임 / 1번 통화 버튼(640,360) / 2번에 벨 / 1번에 연결음 / 2번 받기(404,428) / 둘 다 통화 중 /
   통화 화면 캡처 두 장 / 캡처 뒤에도 통화 중인지 / 1번 끊기(644,606) / 1번·2번 대기로 / 부재중 0건.
   거절은 2번에 tap:876:428, 걸다가 취소는 1번에 tap:640:604, 한 기기만 볼 때는 --unit 2 run 'until:stat:^STAT :30' 'shot:/tmp/idle.jpg'.
+  벨 끄기는 받는 쪽에 tap:404:624, 메시지로 거절은 tap:876:624 → 고르기 화면(SCREEN DECLINE)에서 문구 1·2·3 = tap:240:168·tap:640:168·tap:1040:168, 뒤로 = tap:404:624. 화면이 바뀐 뒤의 탭 앞에는 sleep:1(화면 가드 0.6초).
 
 주의
   - 포트를 열면 기기가 한 번 리셋될 수 있다 → 시나리오 하나는 한 번의 실행 안에서 끝내고, 첫 단계는 until 로 맞춘다.
```

- [x] **Step 4: 커밋**

```bash
cd ~/tab5-toy-phone && git add server/tools/phone_fake_peer.py server/tests/test_phone_fake_peer.py firmware/tab5_phone/tools/phone_ctl.py && git commit -m "feat(phone): 가짜 상대 도구 — 거절 문구 번호(send:reject:<n>·expect:ended:reject:<n>)와 그 시험" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: 글자 자산 — 새 문구 11개

**Files:**
- Modify: `firmware/tab5_phone/tools/gen_ui_assets.py`
- Regenerate: `firmware/tab5_phone/main/ui_assets.h`(생성물 — 손으로 고치지 않는다. 1.6MB → 1.9MB, `cat`/Read 로 열지 않는다)

**Interfaces:**
- Consumes: `tools/fonts/Pretendard-Bold.otf`·`Pretendard-ExtraBold.otf`(git 제외, 이미 있음), Pillow.
- Produces: `main/ui_assets.h` 에 `static const ui_asset_t` 11개(전부 Bold 36px, 쓸 때는 `&이름`):

| 이름 | 문구 | 크기 |
|---|---|---|
| `lbl_mute` / `lbl_mute_done` | 벨 끄기 / 벨 꺼짐 | 103×35 |
| `lbl_decline` | 메시지로 거절 | 196×35 |
| `lbl_back` | 뒤로 | 64×35 |
| `lbl_decline_hint` | 보낼 메시지를 누르면 거절됩니다 | 460×35 |
| `lbl_msg1` · `lbl_msg2` · `lbl_msg3` | 회의 중이에요 · 지금은 받을 수 없어요 · 잠시 후 다시 걸게요 | 196 · 305 · 274 ×35 |
| `lbl_n_msg1` · `lbl_n_msg2` · `lbl_n_msg3` | 상대: 회의 중이에요 · 상대: 지금은 받을 수 없어요 · 상대: 잠시 후 다시 걸게요 | 276 · 385 · 354 ×35 |

  문구 셋은 생성기의 `REJECT_MSGS` 한 곳에만 적는다(순서 = 문구 번호).

- [x] **Step 1: 글꼴이 그대로인지 확인하고 생성기 패치를 적용한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-decline-mute-plan/draft && shasum -a 256 firmware/tab5_phone/tools/fonts/*.otf | cut -c1-12 && shasum -a 256 firmware/tab5_phone/main/ui_assets.h | cut -c1-12 && git apply "$D/patches/T4-01-gen-ui-assets.patch" && cmp "$D/files/firmware/tab5_phone/tools/gen_ui_assets.py" firmware/tab5_phone/tools/gen_ui_assets.py && echo "같음"
```
Expected: `2e91915fab54`(Bold), `c35fe941b756`(ExtraBold), `4af5900e2b98`(지금의 `ui_assets.h`), `같음`. 글꼴 검사합이 다르면 멈춘다.

```diff
--- a/firmware/tab5_phone/tools/gen_ui_assets.py
+++ b/firmware/tab5_phone/tools/gen_ui_assets.py
@@ -9,7 +9,7 @@
   - 스냅부스 스타일 이름 부분을 걷어냈다.
   - 숫자 10개는 세로 범위를 하나로 맞추고(높이가 같아 시계 숫자가 바뀔 때 위아래로 흔들리지 않는다),
     가로는 잉크에 딱 맞게 자른다(원본은 글꼴 고유의 좌우 여백이 붙어 있었다 — "1" 은 좌우가 달라 칸 안에서 쏠린다).
-입력: 아래 LABELS (통화 화면 문구 + Wi-Fi 설정 화면용 lbl_wf_*). 이름은 main/ui.cpp·main/ui_wifi.cpp 가 쓰는 것과 같아야 한다
+입력: 아래 LABELS (통화 화면 문구 + Wi-Fi 설정 화면용 lbl_wf_* + 수신거부 문구 REJECT_MSGS). 이름은 main/ui.cpp·main/ui_wifi.cpp 가 쓰는 것과 같아야 한다
 출력: main/ui_assets.h (ui_asset_t, lbl_*, DIG_BIG[10], DIG_SMALL[10])
 
 실행(ESP-IDF 를 export 하지 않은 셸에서 — export 한 셸의 python3 에는 Pillow 가 없다):
@@ -91,6 +91,21 @@
     ("lbl_wf_cancel",        "취소",                                            36, "bold"),
     ("lbl_wf_connect",       "연결",                                            36, "bold"),
 ]
+# ── 수신거부 메시지·벨 끄기 (docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md §5.5). 전부 Bold 36px ──
+# DM-D13: 거절 문구는 여기 한 곳에만 적는다(순서 = 문구 번호 1·2·3). 버튼용(lbl_msgN)과 거는 쪽 안내용(lbl_n_msgN, "상대: …")을 함께 만든다.
+#   이유: 번호와 문구의 짝이 한 곳에만 있다. 개수는 main/call_fsm.h 의 MSG_COUNT 와 같아야 하고 main/ui.cpp 의 static_assert 가 빌드에서 잡는다.
+#   비용: 목록과 아래 두 줄. 탈출구: LABELS 에 여섯 줄을 직접 적는다.
+# 기존 자산 뒤에 덧붙인다 → 다시 구워도 ui_assets.h 의 기존 줄은 한 줄도 달라지지 않는다(추가만).
+REJECT_MSGS = ["회의 중이에요", "지금은 받을 수 없어요", "잠시 후 다시 걸게요"]
+LABELS += [
+    ("lbl_mute",             "벨 끄기",                                          36, "bold"),   # 벨 화면 아래 줄 왼쪽
+    ("lbl_mute_done",        "벨 꺼짐",                                          36, "bold"),   #   끈 뒤(버튼이 아니라 상태 표시)
+    ("lbl_decline",          "메시지로 거절",                                     36, "bold"),   # 벨 화면 아래 줄 오른쪽
+    ("lbl_back",             "뒤로",                                             36, "bold"),   # 메시지 고르기 화면
+    ("lbl_decline_hint",     "보낼 메시지를 누르면 거절됩니다",                      36, "bold"),
+]
+LABELS += [(f"lbl_msg{n}", text, 36, "bold") for n, text in enumerate(REJECT_MSGS, 1)]
+LABELS += [(f"lbl_n_msg{n}", "상대: " + text, 36, "bold") for n, text in enumerate(REJECT_MSGS, 1)]
 DIGIT_BIG = (160, "xbold")    # 통화 시간 "MM:SS" (gfx::draw_clock), 큰 숫자
 DIGIT_SMALL = (32, "bold")    # 부재중 건수 등
 
```

- [x] **Step 2: 굽는다** (ESP-IDF 를 export 하지 않은 셸에서)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/gen_ui_assets.py --preview /tmp/tab5_phone_dm_assets.png && shasum -a 256 -c ~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-decline-mute-plan/draft/files/firmware/tab5_phone/main/ui_assets.h.sha256 && git diff --numstat -- main/ui_assets.h
```
Expected:
```
[gen_ui_assets] ui_assets.h: labels=66 digits=20 total=86 alpha_bytes=646,515 file_bytes=1,923,795
main/ui_assets.h: OK
4010	0	firmware/tab5_phone/main/ui_assets.h
```
(검사합 `c2d923463e385f578265f2fd5265f4b9f67949659b9518895b96e7e618cebc68`. **삭제 줄 0** = 기존 자산 75개는 한 줄도 달라지지 않고 11개만 추가됐다.)
`/tmp/tab5_phone_dm_assets.png` 를 **직접 열어 본다**(Read 도구 — 세로로 긴 그림이다. 아래쪽, Wi-Fi 화면 문구 뒤·큰 숫자 앞의 11줄): 위 표의 문구가 Pretendard 로 또렷한지, 빈 네모가 없는지. 숫자 줄은 같고 검사합만 다르면 Pillow 판이 다른 것이다(작성 시 10.2.0) — 미리보기가 정상이면 진행하고 실제 검사합을 보고에 적는다.

- [x] **Step 3: 커밋**

```bash
cd ~/tab5-toy-phone && git status --short firmware/tab5_phone | grep -c "fonts/"; git add firmware/tab5_phone/tools/gen_ui_assets.py firmware/tab5_phone/main/ui_assets.h && git commit -m "feat(phone): 글자 자산 — 벨 끄기·메시지 고르기 문구 11개(75 → 86)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
Expected: 첫 줄 `0` (글꼴 폴더는 git 에 보이지 않는다)

---

### Task 5: 화면·진행 — `ui.cpp`·`app.cpp`, 전체 빌드

**Files:**
- Modify: `firmware/tab5_phone/main/ui.cpp`, `main/app.cpp`
- Create(git 제외): `firmware/tab5_phone/out/fw_prev/`(지금 기기에 올라간 빌드의 보관본), `out/dm_fw_build.sha256`, `out/dm_fw_version.txt`

**Interfaces:**
- Consumes: Task 2 의 이름 전부(`Out.msg`, `Fsm::bell_muted`, `Ev::TAP_MUTE`, `Notice::REJECT_MSG`, `reject_text`, `Signal::msg`, `Screen::DECLINE`, `Tap::MUTE`…`MSG3`, `Model` 새 칸 셋, `BTN_MUTE`·`BTN_DECLINE`·`BTN_MSG[]`·`BTN_BACK`, `notice_ms`, `msg_of`, `notice_visible`, `DeclinePicker`), Task 4 의 자산 11개, `gfx::button`·`gfx::blit_asset_center`, `relaylink::send_text`(호출 안에서 다 보낸다 → 스택 버퍼로 충분).
- Produces:
  - 시리얼 규약 추가분(기존 줄의 글자는 그대로): `SCREEN DECLINE`(이때 `STAT … fsm=INCOMING screen=DECLINE`), `FSM … notice=REJECT_MSG`, 문구 번호가 있을 때만 `FSM` 줄 끝에 ` msg=<1~3>`. 예: `FSM INCOMING -> INCOMING acts=0x80 notice=NONE`(벨 끄기) · `FSM INCOMING -> ENDING acts=0x84 notice=NONE msg=2` · `FSM RINGBACK -> IDLE acts=0x40 notice=REJECT_MSG msg=2`.
  - Task 6 이 올릴 바이너리: 저장소의 `firmware/tab5_phone/build/`, 그 검사합 `out/dm_fw_build.sha256`, 버전 `out/dm_fw_version.txt`. 롤백용 `out/fw_prev/`.

- [x] **Step 1: 패치 둘을 적용하고 초안과 대조한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-decline-mute-plan/draft && git apply "$D/patches/T5-01-ui.patch" && git apply "$D/patches/T5-02-app.patch" && for f in main/ui.cpp main/app.cpp main/call_fsm.h main/relay_proto.h main/ui_layout.h; do cmp "$D/files/firmware/tab5_phone/$f" "firmware/tab5_phone/$f" && echo "같음: $f"; done
```
Expected: `같음:` 5줄.

```diff
--- a/firmware/tab5_phone/main/ui.cpp
+++ b/firmware/tab5_phone/main/ui.cpp
@@ -15,7 +15,14 @@
 
 const int CX = SCREEN_W / 2;
 
-const ui_asset_t *notice_asset(Notice n)
+// 거절 문구(순서 = 문구 번호 1·2·3). 문구 자체는 tools/gen_ui_assets.py 의 REJECT_MSGS 한 곳에만 적는다(DM-D13).
+// 크기를 적지 않은 배열 + static_assert: 문구 수가 callfsm::MSG_COUNT 와 어긋나면 빌드가 실패한다.
+const ui_asset_t *const MSG_LABEL[]  = { &lbl_msg1, &lbl_msg2, &lbl_msg3 };          // 메시지 고르기 화면의 버튼 글자
+const ui_asset_t *const MSG_NOTICE[] = { &lbl_n_msg1, &lbl_n_msg2, &lbl_n_msg3 };    // 거는 쪽 안내 상자("상대: …")
+static_assert(sizeof MSG_LABEL / sizeof MSG_LABEL[0] == callfsm::MSG_COUNT && sizeof MSG_NOTICE / sizeof MSG_NOTICE[0] == callfsm::MSG_COUNT,
+              "reject message assets must match callfsm::MSG_COUNT");
+
+const ui_asset_t *notice_asset(Notice n, uint8_t msg)            // msg = REJECT_MSG 일 때의 문구 번호
 {
     switch (n) {
     case Notice::PEER_OFF:     return &lbl_n_peer_off;
@@ -25,6 +32,7 @@
     case Notice::CALL_ENDED:   return &lbl_n_ended;
     case Notice::MISSED:       return &lbl_n_missed;
     case Notice::RELAY_SILENT: return &lbl_n_relay_silent;
+    case Notice::REJECT_MSG:   return callfsm::msg_valid(msg) ? MSG_NOTICE[msg - 1] : &lbl_n_rejected;   // 모르는 번호면 그냥 거절 문구
     case Notice::NONE:         break;
     }
     return nullptr;
@@ -37,7 +45,8 @@
 
 void draw_notice(const Model &m)                                 // 위쪽 가운데: 잠깐 띄우는 안내
 {
-    const ui_asset_t *a = notice_asset(m.notice);
+    if (!notice_visible(m)) return;                              // 메시지 고르기 화면에서는 문구 버튼 줄을 가리므로 그리지 않는다(DM-D12)
+    const ui_asset_t *a = notice_asset(m.notice, m.notice_msg);
     if (!a) return;
     const int w = a->w + 64, h = 80, x = CX - w / 2, y = 88;
     gfx::fill_rrect(x, y, w, h, 12, C_SURFACE);
@@ -106,13 +115,24 @@
     gfx::button(BTN_CANCEL, &lbl_cancel, true, C_SURFACE, C_INK);
 }
 
-void draw_incoming(const Model &)
+void draw_incoming(const Model &m)
 {
     gfx::blit_asset_center(&lbl_incoming, CX, 216, C_INK);
     gfx::button(BTN_ACCEPT, &lbl_accept, true, C_GREEN_BG, C_GREEN_T);
     gfx::button(BTN_REJECT, &lbl_reject, true, C_RED_BG, C_RED_T);
+    // 아래 줄: 벨 끄기(끈 뒤에는 "벨 꺼짐" — 흐린 글자의 상태 표시), 메시지로 거절. 보조 버튼이라 짙은 회색 바탕.
+    // "벨 꺼짐"에 gfx::button 의 enabled=false 를 쓰지 않는다 — 그 모양은 글자가 C_LINE 이라 거의 보이지 않는다.
+    gfx::button(BTN_MUTE, m.bell_muted ? &lbl_mute_done : &lbl_mute, true, C_SURFACE, m.bell_muted ? C_MUTED : C_INK);
+    gfx::button(BTN_DECLINE, &lbl_decline, true, C_SURFACE, C_RED_T);
 }
 
+void draw_decline(const Model &)                                 // 메시지 고르기: 문구를 누르면 그 문구와 함께 거절된다
+{
+    gfx::blit_asset_center(&lbl_decline_hint, CX, 48, C_INK);
+    for (int i = 0; i < callfsm::MSG_COUNT; ++i) gfx::button(BTN_MSG[i], MSG_LABEL[i], true, C_SURFACE, C_INK);
+    gfx::button(BTN_BACK, &lbl_back, true, C_SURFACE, C_INK);
+}
+
 void draw_in_call(const Model &m)
 {
     int mm = 0, ss = 0;
@@ -135,6 +155,7 @@
     case Screen::IDLE:      draw_idle(m); break;
     case Screen::DIALING:   draw_dialing(m); break;
     case Screen::INCOMING:  draw_incoming(m); break;
+    case Screen::DECLINE:   draw_decline(m); break;
     case Screen::ANSWERING: gfx::blit_asset_center(&lbl_answering, CX, SCREEN_H / 2, C_INK); break;
     case Screen::IN_CALL:   draw_in_call(m); break;
     case Screen::ENDING:    gfx::blit_asset_center(&lbl_ending, CX, SCREEN_H / 2, C_MUTED); break;
```

```diff
--- a/firmware/tab5_phone/main/app.cpp
+++ b/firmware/tab5_phone/main/app.cpp
@@ -8,6 +8,10 @@
 //       M5Unified 의 wasPressed 는 닿은 뒤 첫 update 한 번에만 참이라 그 사이에 닿았다 뗀 짧은 터치를 통째로 놓치기 때문이다
 //       (통화 중에는 시계 때문에 매초 다시 그린다 → 끊기 버튼이 안 눌린 것처럼 보일 수 있다).
 //       비용: 태스크 하나(4KB). M5.update() 는 touch 태스크에서만 부른다. 탈출구: touch_task 의 내용을 app_task 루프 머리로 되돌린다.
+// 수신거부 메시지·벨 끄기(docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md §5.6):
+//       메시지 고르기 화면의 열림(s_picker)은 화면 쪽 상태다 — call_fsm 은 INCOMING 그대로라 벨 제한·상대 취소·부재중이 그대로 돈다(DM-D8).
+//       그래서 call_fsm 에 사건을 넣을 때마다(feed) s_picker.sync 로 맞춘다: INCOMING 을 떠나면 닫힌다.
+//       비용: 기억이 두 군데(Fsm::bell_muted / s_picker). 탈출구: 고르기 열림도 Fsm 에 넣는다.
 #include "app.h"
 
 #include <M5Unified.h>
@@ -38,7 +42,6 @@
 namespace {
 
 const char *TAG = "app";
-const int64_t NOTICE_MS = 4000;                 // 안내를 띄워 두는 시간
 
 struct TapEv { int16_t x, y; };
 
@@ -49,7 +52,9 @@
 const char *s_screen = "BOOT";
 int64_t s_screen_since = 0;
 Notice s_notice = Notice::NONE;
+uint8_t s_notice_msg = 0;                       // s_notice == REJECT_MSG 일 때 상대가 보낸 문구 번호
 int64_t s_notice_until = 0;
+uilayout::DeclinePicker s_picker;               // 메시지 고르기 화면이 열려 있나(app 태스크에서만 만진다)
 int s_unit = 0;
 
 SemaphoreHandle_t s_i2c = nullptr, s_stat_lock = nullptr;
@@ -65,19 +70,27 @@
 {
     static const uint16_t SENDS[] = { A_SEND_CALL, A_SEND_ACCEPT, A_SEND_REJECT, A_SEND_HANGUP };
     for (uint16_t bit : SENDS) {
-        if ((o.acts & bit) && !relaylink::send_text(relayproto::text_for(bit))) ESP_LOGW(TAG, "signal 0x%x not sent (link down)", bit);
+        if (!(o.acts & bit)) continue;
+        char rej[40];                                            // 거절은 문구 번호를 실을 수 있다. send_text 가 호출 안에서 다 보내므로 스택 버퍼로 충분하다(ping 과 같은 방식)
+        const char *text = relayproto::text_for(bit);
+        if (bit == A_SEND_REJECT && relayproto::reject_text(rej, sizeof rej, o.msg)) text = rej;
+        if (!relaylink::send_text(text)) ESP_LOGW(TAG, "signal 0x%x not sent (link down)", bit);
     }
     if (o.acts & A_RECONNECT) relaylink::reconnect();
     // 소리: Phase 3 에서 tones·voice_pipeline 을 여기에 잇는다(A_TONE_*, A_VOICE_*). Phase 2 는 아래 FSM 줄에 acts 로만 남는다.
-    if (o.notice != Notice::NONE) { s_notice = o.notice; s_notice_until = now + NOTICE_MS; }
-    if (s_fsm.state != before || o.acts || o.notice != Notice::NONE)
-        printf("FSM %s -> %s acts=0x%x notice=%s\n", state_name(before), state_name(s_fsm.state), (unsigned)o.acts, notice_name(o.notice));
+    if (o.notice != Notice::NONE) { s_notice = o.notice; s_notice_msg = o.msg; s_notice_until = now + uilayout::notice_ms(o.notice); }   // 거절 문구는 10초, 그 밖은 4초
+    if (s_fsm.state != before || o.acts || o.notice != Notice::NONE) {
+        char tail[12] = "";                                      // 문구 번호가 있을 때만 줄 끝에 " msg=N"(기존 줄의 글자는 그대로 — DM-D9). 한 번의 printf 로 찍는다(줄이 쪼개지지 않게)
+        if (o.msg) snprintf(tail, sizeof tail, " msg=%u", (unsigned)o.msg);
+        printf("FSM %s -> %s acts=0x%x notice=%s%s\n", state_name(before), state_name(s_fsm.state), (unsigned)o.acts, notice_name(o.notice), tail);
+    }
 }
 
-void feed(Ev ev, int64_t now, Why why = Why::NONE, bool flag = false)
+void feed(Ev ev, int64_t now, Why why = Why::NONE, bool flag = false, uint8_t msg = 0)
 {
     const State before = s_fsm.state;
-    apply(s_fsm.on(ev, now, why, flag), before, now);
+    apply(s_fsm.on(ev, now, why, flag, msg), before, now);
+    s_picker.sync(s_fsm.state);                                  // 사건마다 맞춘다 — refresh() 에서만 맞추면 한 바퀴 안에 ended → ring 이 연달아 올 때 다음 전화가 고르기 화면으로 뜬다
 }
 
 bool same(const Model &a, const Model &b)
@@ -85,7 +98,8 @@
     return a.fsm == b.fsm && a.peer_online == b.peer_online && a.missed == b.missed && a.talk_seconds == b.talk_seconds
         && a.notice == b.notice && a.wifi_phase == b.wifi_phase && a.wifi_connected == b.wifi_connected && !strcmp(a.ssid, b.ssid)
         && wifistore::bars(a.rssi) == wifistore::bars(b.rssi) && a.relay_up == b.relay_up && a.relay_code == b.relay_code
-        && (a.relay_attempt >= 2) == (b.relay_attempt >= 2) && a.unit == b.unit;
+        && (a.relay_attempt >= 2) == (b.relay_attempt >= 2) && a.unit == b.unit
+        && a.bell_muted == b.bell_muted && a.decline_open == b.decline_open && a.notice_msg == b.notice_msg;   // 빠뜨리면 눌러도 화면이 다시 그려지지 않는다
 }
 
 // 지금 상태로 "그릴 내용"을 만들고, 화면이 바뀌었으면 기록한다(터치 가드의 기준 시각이 된다).
@@ -94,8 +108,9 @@
     Model m;
     m.fsm = s_fsm.state; m.peer_online = s_fsm.peer_online; m.missed = s_fsm.missed;
     m.talk_seconds = s_fsm.state == State::IN_CALL ? (int)((now - s_fsm.call_start_ms) / 1000) : 0;
-    if (s_notice != Notice::NONE && now >= s_notice_until) s_notice = Notice::NONE;
-    m.notice = s_notice;
+    if (s_notice != Notice::NONE && now >= s_notice_until) { s_notice = Notice::NONE; s_notice_msg = 0; }
+    m.notice = s_notice; m.notice_msg = s_notice_msg;
+    m.bell_muted = s_fsm.bell_muted; m.decline_open = s_picker.open;
     m.wifi_phase = netwifi::phase(); m.wifi_connected = netwifi::connected();
     netwifi::ssid(m.ssid, sizeof m.ssid); m.rssi = netwifi::rssi();
     m.relay_up = relaylink::up(); m.relay_code = relaylink::http_code(); m.relay_attempt = relaylink::attempt();
@@ -123,6 +138,11 @@
     case Tap::ACCEPT: feed(Ev::TAP_ACCEPT, now); break;
     case Tap::REJECT: feed(Ev::TAP_REJECT, now); break;
     case Tap::HANGUP: feed(Ev::TAP_HANGUP, now); break;
+    case Tap::MUTE:   feed(Ev::TAP_MUTE, now); break;                                    // 벨만 끈다(신호 없음)
+    case Tap::DECLINE_OPEN:
+    case Tap::DECLINE_BACK: s_picker.on_tap(t); break;                                   // 화면만 바뀐다 — call_fsm 에는 넣지 않는다. 화면 이름이 바뀌어 600ms 화면 가드가 걸린다
+    case Tap::MSG1: case Tap::MSG2: case Tap::MSG3:
+        feed(Ev::TAP_REJECT, now, Why::NONE, false, uilayout::msg_of(t)); break;         // 메시지 거절 = 문구 번호를 실은 거절(DM-D7)
     case Tap::WIFI:   uiwifi::open(); break;
     case Tap::NONE:   break;
     }
@@ -148,7 +168,7 @@
         while (relaylink::poll(&in)) {
             if (in.kind == relaylink::In::Kind::LINK_UP) feed(Ev::LINK_UP, now);
             else if (in.kind == relaylink::In::Kind::LINK_DOWN) feed(Ev::LINK_DOWN, now);
-            else feed(in.sig.ev, now, in.sig.why, in.sig.flag);
+            else feed(in.sig.ev, now, in.sig.why, in.sig.flag, in.sig.msg);
         }
         refresh(now);                                            //    방금 바뀐 화면을 기준으로 터치를 판정한다
 
```

- [x] **Step 2: 설계서 §5.6 의 항목을 코드와 하나씩 대조한다** (이 두 파일은 맥 시험이 없다 — 읽어서 확인하고 보고에 적는다. 줄 번호는 패치를 적용한 뒤의 것)

| 설계서 §5.6 | 코드 |
|---|---|
| **`same()` 에 `Model` 새 칸 셋의 비교** — 빠지면 눌러도 화면이 다시 그려지지 않는다 | `app.cpp:102` `a.bell_muted == b.bell_muted && a.decline_open == b.decline_open && a.notice_msg == b.notice_msg` |
| **`feed()` 가 사건마다 `s_picker.sync()`** — `refresh()` 에서만 맞추면 `ended` → `ring` 뒤 다음 전화가 고르기 화면으로 뜬다 | `app.cpp:93` `s_picker.sync(s_fsm.state);` (`feed()` 의 마지막 줄. 교환기 사건·터치·`TICK` 모두 `feed()` 를 지난다) |
| `app_task` 가 `in.sig.msg` 를 넘긴다 | `app.cpp:171` |
| `apply()` 가 `reject_text` 로 번호를 실어 보낸다 | `app.cpp:76` (`bit == A_SEND_REJECT` 일 때만. 번호가 0 이면 `text_for` 와 같은 글자) |
| 안내 시간이 `notice_ms()` 를 쓴다, `NOTICE_MS` 상수는 `ui_layout.h` 로 옮겼다 | `app.cpp:81` (`app.cpp` 에 `NOTICE_MS` 정의 없음) |
| `FSM` 줄 끝의 ` msg=N`(번호가 있을 때만, 한 번의 `printf`) | `app.cpp:84~85` |
| `refresh()` 가 `Model` 의 새 세 칸을 채운다 | `app.cpp:112~113` |
| `on_tap()`: `MUTE` → `TAP_MUTE` / `DECLINE_OPEN`·`DECLINE_BACK` → `s_picker.on_tap`(상태 기계에 넣지 않는다) / `MSG1~3` → `TAP_REJECT` + `msg_of(t)` | `app.cpp:141~145` |
| `SCREEN DECLINE` — `refresh()` 가 `screen_name(screen_for(s_model))` 를 찍는다(코드 변경 없음, `Model::decline_open` 과 `Screen::DECLINE` 으로 나온다). 화면 이름이 바뀌므로 600ms 화면 가드가 그대로 걸린다 | `app.cpp` `refresh()` 끝 두 줄(그대로) |
| `draw_incoming`: 벨 끄기(끈 뒤 `lbl_mute_done` + `C_MUTED`, `enabled=false` 를 쓰지 않는다)·메시지로 거절(`C_RED_T`) | `ui.cpp:125~126` |
| `draw_decline`(신규) + `ui::draw()` 의 `switch` 에 `Screen::DECLINE` | `ui.cpp:129~134`, `ui.cpp:158` |
| 안내 상자: `REJECT_MSG` 면 `MSG_NOTICE[번호 - 1]`(범위 밖이면 `lbl_n_rejected`), `notice_visible(m)` 일 때만 그린다 | `ui.cpp:35`, `ui.cpp:48` |
| `MSG_LABEL[]`·`MSG_NOTICE[]` 는 크기를 적지 않은 배열 + `static_assert`(`MSG_COUNT`) | `ui.cpp:20~23` |

- [x] **Step 3: 지금 기기에 올라가 있는 빌드를 보관한다** (DMP-D2 — 다음 Step 이 `build/` 를 덮어쓴다)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && mkdir -p out/fw_prev/bootloader out/fw_prev/partition_table && cp build/flash_args build/tab5_phone.bin out/fw_prev/ && cp build/bootloader/bootloader.bin out/fw_prev/bootloader/ && cp build/partition_table/partition-table.bin out/fw_prev/partition_table/ && shasum -a 256 out/fw_prev/tab5_phone.bin | cut -c1-64 && git status --short | grep -c "fw_prev"
```
Expected: `e1190c9a4140408031e1bb25941b5343617c7ee62afa569192c0dee2e466e22f`(Phase 2 Task 8 에서 두 기기에 올린 빌드), `0`(git 에 보이지 않는다). 검사합이 다르면 누군가 그 뒤에 다시 빌드한 것이다 — 보관본이 "기기에 올라간 것"이라는 보장이 없다고 보고에 적고 계속한다.

- [x] **Step 4: 전체 빌드** (export 와 빌드를 같은 호출에. 1~3분)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && source ~/esp/esp-idf-5.5.4/export.sh > /dev/null 2>&1 && idf.py build > /tmp/tab5_phone_dm_build.log 2>&1; echo "rc=$?"; grep -E "binary size|Project build complete" /tmp/tab5_phone_dm_build.log; echo "우리 코드 경고: $(grep 'warning:' /tmp/tab5_phone_dm_build.log | grep -c 'tab5_phone/main/')"; echo "오류: $(grep -c 'error:' /tmp/tab5_phone_dm_build.log)"; grep -c "version: 2.11.7" dependencies.lock
```
Expected:
```
rc=0
tab5_phone.bin binary size 0x2312b0 bytes. Smallest app partition is 0x600000 bytes. 0x3ced50 bytes (63%) free.
Project build complete. To flash, run:
우리 코드 경고: 0
오류: 0
1
```
크기는 자리 값 `secrets.h` 로 잰 것이라 실제로는 수십 바이트 다를 수 있다(기준선도 사본 0x219b30 / 저장소 0x219b40 이었다). 0x2312b0 에서 0x100 넘게 다르면 보고에 적는다. 마지막 줄 `1` = `esp_hosted` 가 2.11.7 그대로다.
`rc` 가 0 이 아니면 `grep -m5 "error:" /tmp/tab5_phone_dm_build.log` 를 보고에 붙이고 멈춘다(코드를 고치지 않는다).

- [x] **Step 5: 커밋**

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/main/ui.cpp firmware/tab5_phone/main/app.cpp && git commit -m "feat(phone): 화면·진행 — 벨 끄기 버튼·메시지 고르기 화면·거절 문구 안내(10초)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [x] **Step 6: 커밋 뒤 다시 빌드해 "올릴 바이너리"를 확정하고 검사합을 적어 둔다** (버전 문자열이 방금 커밋을 가리키게 — 1분 안팎)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && source ~/esp/esp-idf-5.5.4/export.sh > /dev/null 2>&1 && idf.py build > /tmp/tab5_phone_dm_build2.log 2>&1; echo "rc=$?"; grep -c "Project build complete" /tmp/tab5_phone_dm_build2.log; echo "우리 코드 경고: $(grep 'warning:' /tmp/tab5_phone_dm_build2.log | grep -c 'tab5_phone/main/')"; shasum -a 256 build/tab5_phone.bin build/bootloader/bootloader.bin build/partition_table/partition-table.bin > out/dm_fw_build.sha256 && grep '"project_version"' build/project_description.json | tee out/dm_fw_version.txt && git log --oneline -1 && cut -c1-12 out/dm_fw_build.sha256
```
Expected: `rc=0`, `1`, `우리 코드 경고: 0`, `"project_version":    "<방금 커밋의 짧은 SHA>"`(끝에 `-dirty` 가 붙으면 작업 트리에 커밋 안 된 변경이 있다는 뜻 — `git status --short` 를 보고에 적는다. 올리는 데는 지장 없다), 검사합 세 줄. **이 뒤로 Task 6 이 끝날 때까지 `idf.py build`·`idf.py flash` 를 돌리지 않는다**(바이너리가 달라진다).

---

### Task 6: 기기 검증 — 섞인 상태 → 두 대 시나리오 A~G → 회귀 → 화면 사진

**Files:**
- Create(git 제외): `firmware/tab5_phone/out/dm_*.log`, `out/dm_*.jpg`, `out/dm_flash_unit*.txt`

**Interfaces:**
- Consumes: Task 1 의 교환기 코드(저장소에 커밋된 것), Task 5 의 `build/`·`out/dm_fw_build.sha256`·`out/dm_fw_version.txt`, `tools/phone_ctl.py pair`, `server/tools/phone_relay_dev.sh`, `…/2026-10-04-tab5-phone-phase2/touch_audit.py`, `D/verify/secret_scan.py`.
- Produces: 설계서 §7.2 시나리오 H·A~G·I 의 통과 기록, 스크린샷 6종(`dm_a_bell.jpg`·`dm_a_muted.jpg`·`dm_c_picker.jpg`·`dm_c_notice1.jpg`·`dm_c_notice2.jpg`·`dm_d_6s.jpg`) + `dm_d_15s.jpg`, 터치 대조·비밀·여유 기록.

모든 명령은 ESP-IDF 를 export 하지 **않은** 호출에서 돌린다(플래시 Step 만 export). 좌표: 통화 `640,360` · 취소 `640,604` · 받기 `404,428` · 거절 `876,428` · 끊기 `644,606` · 벨 끄기 `404,624` · 메시지로 거절 `876,624` · 문구 1·2·3 `240,168`·`640,168`·`1040,168` · 뒤로 `404,624`. 화면이 바뀐 뒤 같은 기기의 다음 탭 앞에는 `sleep:1` 이 있다(화면 가드 0.6초).
각 실행의 통과 조건은 공통이다: **`rc=0` 이고 `!!` 줄이 없다**(= 적힌 `wait`·`until` 이 모두 맞았다). 벨은 30초면 교환기가 끝낸다 — 스크린샷이 다시 받기(재시도)에 들어가 30초를 넘기면 그 실행은 실패한다. 그러면 원인이 그것인지 로그로 확인하고 그 실행만 다시 돌린다(3회까지. 세 번 모두 실패하면 멈춘다).

- [x] **Step 1: 사전 확인** — 컨트롤러가 사용자에게 "약 45분 동안 두 기기 화면을 만지지 말아 달라"고 알린 뒤에 시작한다 (정정 2026-10-04: 이 "약 45분"은 계획 때의 예상이었고 실제 기기 시험은 14:53~15:05 약 12분이었다)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && shasum -a 256 -c out/dm_fw_build.sha256 && cat out/dm_fw_version.txt && grep -c "REJECT_MSG_MAX" ~/tab5-toy-phone/server/phone_core.py && P1=$(python3 tools/phone_ctl.py port 1) && P2=$(python3 tools/phone_ctl.py port 2) && echo "1번 $P1 / 2번 $P2" && lsof "$P1" "$P2" 2>/dev/null | grep -c usbmodem
```
Expected: `build/tab5_phone.bin: OK` 등 `OK` 3줄, 버전 한 줄, `2`(교환기 코드가 새 것), 포트 경로 두 개, `0`(두 포트를 열고 있는 다른 프로그램 없음 — 다른 담당의 시험이 아직 돌고 있으면 0 이 아니다). 하나라도 다르면 멈춘다(BLOCKED) — 특히 검사합이 `FAILED` 면 Task 5 Step 6 뒤에 누군가 다시 빌드한 것이다.

Run(단독 호출): `bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh status`
Expected: `relay: listening on port 5094, healthz={"ok": true}`

- [x] **Step 2: 교환기를 새 코드로 다시 띄우고, 옛 펌웨어 두 대로 그냥 거절 한 번** (새 교환기 + 옛 펌웨어 = 지금과 같아야 한다)

Run: `cp /tmp/phone_relay_dev.log ~/tab5-toy-phone/firmware/tab5_phone/out/dm_relay_before_restart.log`
Run(단독 호출): `bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh restart`
Expected: `relay: stopped`, `relay: started on 0.0.0.0:5094`

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'sleep:30' 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1:120' '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20:10' '1:wait:^FSM DIALING -> RINGBACK:10' 'sleep:1' '2:tap:876:428' '2:wait:^FSM INCOMING -> ENDING acts=0x84 notice=NONE$:10' '1:wait:^FSM RINGBACK -> IDLE acts=0x40 notice=REJECTED$:10' '2:wait:^FSM ENDING -> IDLE:10' 'both:until:stat:^STAT .*fsm=IDLE.*peer=1:15' > out/dm_s2_relay.log 2>&1; echo "rc=$?"; grep -E 'FSM|RELAY|!!' out/dm_s2_relay.log; grep -o '"ev":"online","dev":"tab5-[12]"' /tmp/phone_relay_dev.log | sort -u; grep '"ev":"call_end"' /tmp/phone_relay_dev.log | tail -1 | grep -o '"why":"[a-z]*".*'
```
Expected: `rc=0`, `!!` 없음, `"ev":"online","dev":"tab5-1"`·`"ev":"online","dev":"tab5-2"` 두 줄(두 기기가 **새** 교환기에 붙었다), 마지막 줄 `"why":"reject","ring_s":…,"talk_s":0}`(`msg` 없음).
맨 앞의 `sleep:30` 은 빼지 않는다: 교환기를 다시 띄운 뒤 최대 20초 동안 기기는 죽은 옛 연결을 붙어 있다고 믿는다(`relay=1` 로 보인다) — 그 사이에 걸면 실패한다. `RELAY up=0` → `RELAY up=1` 줄이 그 사이의 재연결이다(포트를 열 때 기기가 재시작됐다면 부팅 로그가 대신 보인다).

- [x] **Step 3: 2번에 새 펌웨어를 올린다** (섞인 상태를 만든다. export 한 호출)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && source ~/esp/esp-idf-5.5.4/export.sh > /dev/null 2>&1 && PORT=$(python3 tools/phone_ctl.py port 2) && echo "2번 포트: $PORT" && python -m esptool --chip esp32p4 -p "$PORT" read_mac 2>&1 | tee out/dm_flash_unit2.txt | grep -i "aa:bb:cc:dd:ee:02" && cd build && python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash @flash_args 2>&1 | tee -a ../out/dm_flash_unit2.txt | grep -E "Hash of data verified|Hard resetting|rror"
```
Expected: `2번 포트: /dev/cu.usbmodem…`, `MAC: aa:bb:cc:dd:ee:02`, `Hash of data verified.` 3줄, `Hard resetting via RTS pin...`. MAC 줄이 안 나오면 아무것도 쓰이지 않는다(명령이 거기서 끝난다) — 멈춘다.

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run 'until:stat:^STAT :60' 'reboot' 'wait:^PHONE BOOT unit=2 id=tab5-2:30' 'until:stat:^STAT .*fsm=IDLE.*relay=1:120' > out/dm_boot_unit2.log 2>&1; echo "rc=$?"; grep -E "App version|Compile time|ELF file SHA256|PHONE BOOT|PHONE FAIL" out/dm_boot_unit2.log | tail -4; grep "^STAT" out/dm_boot_unit2.log | tail -1
```
Expected: `rc=0`, `App version:      <out/dm_fw_version.txt 의 버전>`, `Compile time: …`, `ELF file SHA256: …`, `PHONE BOOT unit=2 id=tab5-2`, `STAT … fsm=IDLE screen=IDLE wifi=1 relay=1 …`. `PHONE FAIL` 줄이 있거나 60초 안에 `STAT` 이 없으면 멈춘다.

- [x] **Step 4: 시나리오 H — 섞인 상태**(2번 = 새 펌웨어, 1번 = 옛 펌웨어)에서도 통화·거절이 된다

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1:60' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20:10' '1:wait:^FSM DIALING -> RINGBACK:10' 'sleep:1' '2:tap:876:624' '2:wait:^SCREEN DECLINE$:5' 'sleep:1' '2:tap:640:168' '2:wait:^FSM INCOMING -> ENDING acts=0x84 notice=NONE msg=2$:10' '1:wait:^FSM RINGBACK -> IDLE acts=0x40 notice=REJECTED$:10' '2:wait:^FSM ENDING -> IDLE:10' 'sleep:5' \
  '2:tap:640:360' '1:wait:^FSM IDLE -> INCOMING acts=0x20:10' '2:wait:^FSM DIALING -> RINGBACK:10' 'sleep:1' '1:tap:876:428' '2:wait:^FSM RINGBACK -> IDLE acts=0x40 notice=REJECTED$:10' '1:wait:^FSM ENDING -> IDLE:10' \
  'both:until:stat:^STAT .*fsm=IDLE.*peer=1:15' > out/dm_h.log 2>&1; echo "rc=$?"; grep -E 'FSM|SCREEN|!!' out/dm_h.log; grep -oE '^\[[12]\] STAT .*missed=[0-9]+' out/dm_h.log | sed -E 's/ STAT .*(missed=[0-9]+)/ \1/' | sort -u
```
Expected: `rc=0`, `!!` 없음, 마지막은 **정확히 두 줄**(기기마다 `missed=` 값이 하나 = 거절은 부재중을 남기지 않는다). 앞 통화: 1번(옛)이 걸고 2번이 문구 2로 거절 → `[2] FSM INCOMING -> ENDING acts=0x84 notice=NONE msg=2`, `[1] FSM RINGBACK -> IDLE acts=0x40 notice=REJECTED`(옛 펌웨어는 번호를 모르므로 그냥 거절로 보인다). 뒤 통화: 2번이 걸고 1번(옛)이 그냥 거절 → `[2] FSM RINGBACK -> IDLE acts=0x40 notice=REJECTED`(끝에 `msg=` 없음).

- [x] **Step 5: 1번에 같은 바이너리를 올린다** (export 한 호출. 다시 빌드하지 않는다)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && shasum -a 256 -c out/dm_fw_build.sha256 && source ~/esp/esp-idf-5.5.4/export.sh > /dev/null 2>&1 && PORT=$(python3 tools/phone_ctl.py port 1) && echo "1번 포트: $PORT" && python -m esptool --chip esp32p4 -p "$PORT" read_mac 2>&1 | tee out/dm_flash_unit1.txt | grep -i "aa:bb:cc:dd:ee:01" && cd build && python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash @flash_args 2>&1 | tee -a ../out/dm_flash_unit1.txt | grep -E "Hash of data verified|Hard resetting|rror"
```
Expected: `OK` 3줄, `1번 포트: …`, `MAC: aa:bb:cc:dd:ee:01`, `Hash of data verified.` 3줄, `Hard resetting via RTS pin...`.

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py --unit 1 run 'until:stat:^STAT :60' 'reboot' 'wait:^PHONE BOOT unit=1 id=tab5-1:30' 'until:stat:^STAT .*fsm=IDLE.*relay=1:120' > out/dm_boot_unit1.log 2>&1; echo "rc=$?"; grep -hE "App version|Compile time|ELF file SHA256" out/dm_boot_unit1.log out/dm_boot_unit2.log | sed 's/^.*app_init: //' | sort -u
```
Expected: `rc=0`, 그리고 **정확히 세 줄**(`App version: …`, `Compile time: …`, `ELF file SHA256: …` 가 한 줄씩 = 두 기기의 값이 같다. 네 줄 이상이면 두 기기의 바이너리가 다르다 — 멈춘다). 그 세 줄을 보고에 적는다. (1번은 저장된 Wi-Fi 목록으로 부팅 뒤 약 17초에 대기 상태가 된다.)

- [x] **Step 6: 시나리오 A·B — 벨 끄기**

A. 벨 끄기 → 받기(화면 사진: 벨 화면, "벨 꺼짐"):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1:60' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20:10' '1:wait:^FSM DIALING -> RINGBACK:10' \
  '2:until:stat:^STAT .*fsm=INCOMING screen=INCOMING:5' '2:shot:out/dm_a_bell.jpg' \
  '2:tap:404:624' '2:wait:^FSM INCOMING -> INCOMING acts=0x80 notice=NONE$:5' '1:until:stat:^STAT .*fsm=RINGBACK screen=DIALING:5' \
  '2:until:stat:^STAT .*fsm=INCOMING screen=INCOMING:5' '2:shot:out/dm_a_muted.jpg' \
  '2:tap:404:428' '2:wait:^FSM INCOMING -> ANSWERING acts=0x82:10' '1:wait:^FSM RINGBACK -> IN_CALL:10' '2:wait:^FSM ANSWERING -> IN_CALL:10' 'sleep:2' \
  '1:tap:644:606' '2:wait:^FSM IN_CALL -> IDLE .*notice=CALL_ENDED:10' '1:wait:^FSM ENDING -> IDLE:10' \
  'both:until:stat:^STAT .*fsm=IDLE.*peer=1:15' > out/dm_a.log 2>&1; echo "rc=$?"; grep -E 'FSM|SCREEN|==|!!' out/dm_a.log
```
Expected: `rc=0`, `!!` 없음, `[2] FSM INCOMING -> INCOMING acts=0x80 notice=NONE`(벨 끄기 — 신호 없이 상태 그대로), 그 뒤에도 1번은 `fsm=RINGBACK`(거는 쪽은 모른다), 이어서 양쪽 `-> IN_CALL`. `== [2] 저장:` 2줄.

B. 끈 벨은 다음 전화에 남지 않는다(끈 벨을 또 눌러도 아무 일 없음 → 1번 취소 → 다시 걸기 → 또 끌 수 있음 → 그냥 거절):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1:60' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20:10' '1:wait:^FSM DIALING -> RINGBACK:10' 'sleep:1' \
  '2:tap:404:624' '2:wait:^FSM INCOMING -> INCOMING acts=0x80 notice=NONE$:5' 'sleep:1' '2:tap:404:624' 'sleep:1' '2:until:stat:^STAT .*fsm=INCOMING screen=INCOMING:5' \
  '1:tap:640:604' '2:wait:^FSM INCOMING -> IDLE acts=0x80 notice=MISSED$:10' '1:wait:^FSM ENDING -> IDLE:10' 'sleep:2' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20:10' '1:wait:^FSM DIALING -> RINGBACK:10' 'sleep:1' \
  '2:tap:404:624' '2:wait:^FSM INCOMING -> INCOMING acts=0x80 notice=NONE$:5' 'sleep:1' \
  '2:tap:876:428' '2:wait:^FSM INCOMING -> ENDING acts=0x84 notice=NONE$:10' '1:wait:^FSM RINGBACK -> IDLE acts=0x40 notice=REJECTED$:10' '2:wait:^FSM ENDING -> IDLE:10' \
  'both:until:stat:^STAT .*fsm=IDLE.*peer=1:15' > out/dm_b.log 2>&1; echo "rc=$?"; grep -E 'FSM|!!' out/dm_b.log; grep -c '^\[2\] FSM INCOMING -> INCOMING acts=0x80 notice=NONE$' out/dm_b.log
```
Expected: `rc=0`, `!!` 없음, 마지막 줄 `2` — 벨 끄기 줄이 전화마다 한 번씩 정확히 두 번이다(`3` 이면 이미 끈 벨이 또 눌린 것, 둘째 전화에서 줄이 안 나와 실패했다면 앞 전화의 "꺼짐"이 남은 것). 그냥 거절은 `[1] … notice=REJECTED`(끝에 `msg=` 없음).

- [x] **Step 7: 시나리오 C·D — 메시지 거절 세 문구, 안내 10초**(화면 사진: 고르기 화면, 거는 쪽 안내 3종, 15초 뒤)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1:60' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20:10' '1:wait:^FSM DIALING -> RINGBACK:10' 'sleep:1' \
  '2:tap:876:624' '2:wait:^SCREEN DECLINE$:5' '2:until:stat:^STAT .*fsm=INCOMING screen=DECLINE:5' '2:shot:out/dm_c_picker.jpg' \
  '2:tap:240:168' '2:wait:^FSM INCOMING -> ENDING acts=0x84 notice=NONE msg=1$:10' '1:wait:^FSM RINGBACK -> IDLE acts=0x40 notice=REJECT_MSG msg=1$:10' \
  '1:until:stat:^STAT .*fsm=IDLE screen=IDLE:5' '1:shot:out/dm_c_notice1.jpg' '2:wait:^FSM ENDING -> IDLE acts=0x0 notice=NONE$:10' 'sleep:12' \
  '2:tap:640:360' '1:wait:^FSM IDLE -> INCOMING acts=0x20:10' '2:wait:^FSM DIALING -> RINGBACK:10' 'sleep:1' \
  '1:tap:876:624' '1:wait:^SCREEN DECLINE$:5' 'sleep:1' \
  '1:tap:640:168' '1:wait:^FSM INCOMING -> ENDING acts=0x84 notice=NONE msg=2$:10' '2:wait:^FSM RINGBACK -> IDLE acts=0x40 notice=REJECT_MSG msg=2$:10' \
  '2:until:stat:^STAT .*fsm=IDLE screen=IDLE:5' '2:shot:out/dm_c_notice2.jpg' '1:wait:^FSM ENDING -> IDLE acts=0x0 notice=NONE$:10' 'sleep:12' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20:10' '1:wait:^FSM DIALING -> RINGBACK:10' 'sleep:1' \
  '2:tap:876:624' '2:wait:^SCREEN DECLINE$:5' 'sleep:1' \
  '2:tap:1040:168' '2:wait:^FSM INCOMING -> ENDING acts=0x84 notice=NONE msg=3$:10' '1:wait:^FSM RINGBACK -> IDLE acts=0x40 notice=REJECT_MSG msg=3$:10' \
  'sleep:6' '1:until:stat:^STAT .*fsm=IDLE screen=IDLE:5' '1:shot:out/dm_d_6s.jpg' 'sleep:6' '1:until:stat:^STAT .*fsm=IDLE screen=IDLE:5' '1:shot:out/dm_d_15s.jpg' \
  '2:wait:^FSM ENDING -> IDLE acts=0x0 notice=NONE$:10' 'both:until:stat:^STAT .*fsm=IDLE.*peer=1:15' > out/dm_c.log 2>&1; echo "rc=$?"; grep -E 'FSM .*msg=|SCREEN DECLINE|==|!!' out/dm_c.log; grep -oE '^\[[12]\] STAT .*missed=[0-9]+' out/dm_c.log | sed -E 's/ STAT .*(missed=[0-9]+)/ \1/' | sort -u
```
Expected: `rc=0`, `!!` 없음, 문구마다 받는 쪽 `FSM INCOMING -> ENDING acts=0x84 notice=NONE msg=N` 과 거는 쪽 `FSM RINGBACK -> IDLE acts=0x40 notice=REJECT_MSG msg=N`(N = 1 → 2 → 3, 둘째 통화는 방향이 반대), `SCREEN DECLINE` 3줄, `== … 저장:` 5줄. 마지막은 **정확히 두 줄**(`[1] missed=0`, `[2] missed=1` — 2번의 1건은 Step 6 B 에서 생긴 것): 기기마다 값이 하나뿐 = 메시지 거절은 어느 쪽에도 부재중을 남기지 않는다(세 줄 이상이면 이 실행 중에 부재중이 늘었다).
`sleep:12` 는 앞 통화의 10초 안내가 다음 사진에 남지 않게 기다리는 것이다. D 의 두 사진은 거절 뒤 약 6초와 15초 이상에 찍힌다.

- [x] **Step 8: 시나리오 E·F·G — 뒤로, 고르는 중 상대 취소, 고르는 중 30초 무응답**

E. 뒤로 → 받기(고르기 화면에서 받기 자리는 버튼이 아니다):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1:60' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20:10' '1:wait:^FSM DIALING -> RINGBACK:10' 'sleep:1' \
  '2:tap:876:624' '2:wait:^SCREEN DECLINE$:5' 'sleep:1' '2:tap:404:428' 'sleep:1' '2:until:stat:^STAT .*fsm=INCOMING screen=DECLINE:5' \
  '2:tap:404:624' '2:wait:^SCREEN INCOMING$:5' 'sleep:1' \
  '2:tap:404:428' '2:wait:^FSM INCOMING -> ANSWERING acts=0x82:10' '1:wait:^FSM RINGBACK -> IN_CALL:10' '2:wait:^FSM ANSWERING -> IN_CALL:10' 'sleep:2' \
  '1:tap:644:606' '2:wait:^FSM IN_CALL -> IDLE .*notice=CALL_ENDED:10' '1:wait:^FSM ENDING -> IDLE:10' \
  'both:until:stat:^STAT .*fsm=IDLE.*peer=1:15' > out/dm_e.log 2>&1; echo "rc=$?"; grep -E 'FSM|SCREEN|!!' out/dm_e.log
```
Expected: `rc=0`, `!!` 없음, `[2] SCREEN DECLINE` → (받기 자리를 눌러도 `fsm=INCOMING screen=DECLINE` 그대로) → `[2] SCREEN INCOMING` → 양쪽 `-> IN_CALL`.

F. 고르는 중에 상대가 취소 → 부재중, 그 뒤의 터치는 아무것도 누르지 않고, 다음 전화는 벨 화면으로 시작한다:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1:60' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20:10' '1:wait:^FSM DIALING -> RINGBACK:10' 'sleep:1' \
  '2:tap:876:624' '2:wait:^SCREEN DECLINE$:5' 'sleep:1' \
  '1:tap:640:604' '2:wait:^FSM INCOMING -> IDLE acts=0x80 notice=MISSED$:10' '2:wait:^SCREEN IDLE$:5' '1:wait:^FSM ENDING -> IDLE:10' 'sleep:1' \
  '2:tap:640:168' '2:tap:404:624' 'sleep:1' '2:until:stat:^STAT .*fsm=IDLE screen=IDLE:5' '1:until:stat:^STAT .*fsm=IDLE screen=IDLE:5' 'sleep:3' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20:10' '2:wait:^SCREEN INCOMING$:5' '2:until:stat:^STAT .*fsm=INCOMING screen=INCOMING:5' 'sleep:1' \
  '1:tap:640:604' '2:wait:^FSM INCOMING -> IDLE acts=0x80 notice=MISSED$:10' '1:wait:^FSM ENDING -> IDLE:10' \
  'both:until:stat:^STAT .*fsm=IDLE.*peer=1:15' > out/dm_f.log 2>&1; echo "rc=$?"; grep -E 'FSM|SCREEN|!!' out/dm_f.log; grep -c '^\[2\] FSM IDLE -> DIALING' out/dm_f.log
```
Expected: `rc=0`, `!!` 없음, `[2] SCREEN DECLINE` → `[2] FSM INCOMING -> IDLE acts=0x80 notice=MISSED` → `[2] SCREEN IDLE` → (문구 2 자리와 뒤로 자리를 눌러도 `fsm=IDLE screen=IDLE`) → 다음 전화에 `[2] SCREEN INCOMING`(고르기 화면이 아니다). 마지막 줄 `0`(2번은 한 번도 걸지 않았다).

G. 고르는 중 30초 무응답(약 35초 걸린다):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1:60' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20:10' '1:wait:^FSM DIALING -> RINGBACK:10' 'sleep:1' \
  '2:tap:876:624' '2:wait:^SCREEN DECLINE$:5' \
  '1:wait:^FSM RINGBACK -> IDLE acts=0x40 notice=NO_ANSWER$:40' '2:wait:^FSM INCOMING -> IDLE acts=0x80 notice=MISSED$:5' '2:wait:^SCREEN IDLE$:5' \
  'both:until:stat:^STAT .*fsm=IDLE screen=IDLE.*peer=1:15' > out/dm_g.log 2>&1; echo "rc=$?"; grep -E 'FSM|SCREEN|!!' out/dm_g.log
```
Expected: `rc=0`, `!!` 없음, `[1] FSM RINGBACK -> IDLE acts=0x40 notice=NO_ANSWER`, `[2] FSM INCOMING -> IDLE acts=0x80 notice=MISSED`, `[2] SCREEN IDLE`(고르기 화면이 닫혔다).

- [x] **Step 9: 시나리오 I — 회귀**(Phase 2 계획 Task 8 Step 4·5 를 그대로 다시. 부재중 건수를 0 으로 맞추려고 두 기기를 먼저 재부팅한다)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT :30' 'both:reboot' 'sleep:3' 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*missed=0:120' > out/dm_i0_reboot.log 2>&1; echo "rc=$?"; grep -E '^\[.\] STAT' out/dm_i0_reboot.log | tail -2
```
Expected: `rc=0`, 두 기기 모두 `fsm=IDLE … relay=1 … peer=1 missed=0`.

걸기 → 벨 → 받기 → 끊기를 양방향으로 10회:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && for i in 1 2 3 4 5; do
python3 tools/phone_ctl.py pair "both:until:stat:^STAT .*fsm=IDLE.*peer=1:30" \
  "1:tap:640:360" "2:wait:^FSM IDLE -> INCOMING:10" "1:wait:^FSM DIALING -> RINGBACK:10" "sleep:1" "2:tap:404:428" "1:wait:^FSM RINGBACK -> IN_CALL:10" "2:wait:^FSM ANSWERING -> IN_CALL:10" "sleep:2" "1:tap:644:606" "2:wait:^FSM IN_CALL -> IDLE .*notice=CALL_ENDED:10" "1:wait:^FSM ENDING -> IDLE:10" \
  "sleep:1" \
  "2:tap:640:360" "1:wait:^FSM IDLE -> INCOMING:10" "2:wait:^FSM DIALING -> RINGBACK:10" "sleep:1" "1:tap:404:428" "2:wait:^FSM RINGBACK -> IN_CALL:10" "1:wait:^FSM ANSWERING -> IN_CALL:10" "sleep:2" "1:tap:644:606" "2:wait:^FSM IN_CALL -> IDLE .*notice=CALL_ENDED:10" "1:wait:^FSM ENDING -> IDLE:10" \
  >> out/dm_i_step4.log 2>&1 && echo "회차 $i: 통과" || { echo "회차 $i: 실패"; break; }
done; python3 tools/phone_ctl.py pair "both:until:stat:^STAT .*fsm=IDLE:15" >> out/dm_i_step4.log 2>&1; grep -c '!!' out/dm_i_step4.log
```
Expected: `회차 1: 통과` … `회차 5: 통과`, 마지막 줄 `0`. (다시 돌릴 때는 `out/dm_i_step4.log` 를 먼저 다른 이름으로 옮긴다 — `>>` 로 이어 쓴다.)

거절·취소·무응답:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair "both:until:stat:^STAT .*fsm=IDLE.*peer=1:30" \
  "1:tap:640:360" "2:wait:^FSM IDLE -> INCOMING:10" "sleep:1" "2:tap:876:428" "1:wait:^FSM RINGBACK -> IDLE .*notice=REJECTED:10" "2:wait:^FSM ENDING -> IDLE:10" "sleep:5" \
  "1:tap:640:360" "2:wait:^FSM IDLE -> INCOMING:10" "sleep:2" "1:tap:640:604" "2:wait:^FSM INCOMING -> IDLE .*notice=MISSED:10" "1:wait:^FSM ENDING -> IDLE:10" "sleep:5" \
  "2:tap:640:360" "1:wait:^FSM IDLE -> INCOMING:10" "2:wait:^FSM RINGBACK -> IDLE .*notice=NO_ANSWER:40" "1:wait:^FSM INCOMING -> IDLE .*notice=MISSED:5" "sleep:5" \
  "1:until:stat:^STAT .*screen=IDLE.*missed=1:5" "2:until:stat:^STAT .*missed=1:5" "1:shot:out/dm_i_unit1_missed.jpg" > out/dm_i_step5.log 2>&1; echo "rc=$?"; grep -E 'FSM|==|!!' out/dm_i_step5.log
```
Expected: `rc=0`, `!!` 없음. 1번 `missed=1`(2번이 건 전화를 30초 동안 받지 않음), 2번 `missed=1`(1번이 걸다 취소함). 스크린샷 아래 가운데에 "부재중 1건".

동시 발신(양쪽이 거의 같은 순간에 통화 버튼):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair "both:until:stat:^STAT .*fsm=IDLE.*peer=1:30" "both:tap:640:360" "1:wait:-> IN_CALL:10" "2:wait:-> IN_CALL:10" "sleep:1" "1:tap:644:606" "both:until:stat:^STAT .*fsm=IDLE:15" > out/dm_i_glare.log 2>&1; echo "rc=$?"; grep -E 'FSM|!!' out/dm_i_glare.log
```
Expected: `rc=0`. 두 기기 모두 `IN_CALL` 에 들어간다 — (가) 한쪽 `DIALING -> RINGBACK -> IN_CALL`, 다른 쪽 `DIALING -> IN_CALL`. (나) 한쪽 터치가 가드에 걸려 `IDLE -> INCOMING` 으로 갔다면 "동시 발신"이 아니었던 것이다 — 다시 돌린다(3회까지. 세 번 모두 (나)면 기록하고 넘어간다 — 판단은 맥 시험 "glare: …" 가 고정한다).

- [x] **Step 10: 터치 대조·비밀·교환기 기록·여유**

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && cp /tmp/phone_relay_dev.log out/dm_relay.log && cat out/dm_s2_relay.log out/dm_h.log out/dm_a.log out/dm_b.log out/dm_c.log out/dm_e.log out/dm_f.log out/dm_g.log out/dm_i0_reboot.log out/dm_i_step4.log out/dm_i_step5.log out/dm_i_glare.log > out/dm_all_pair.log && python3 ~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase2/touch_audit.py out/dm_all_pair.log; python3 ~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-decline-mute-plan/draft/verify/secret_scan.py out/dm_all_pair.log out/dm_boot_unit1.log out/dm_boot_unit2.log out/dm_relay.log; echo "secret rc=$?"; echo "교환기 기록: call_end $(grep -c '"ev":"call_end"' out/dm_relay.log)건, 그중 msg 가 든 줄 $(grep '"ev":"call_end"' out/dm_relay.log | grep -c 'msg')건, 오류 줄 $(grep -ciE 'traceback|tick failed' out/dm_relay.log)건"; for u in 1 2; do grep -E "^\[$u\] STAT " out/dm_i_glare.log | tail -1 | grep -oE '^\[.\]|heap_int_min=[0-9]+|stk_[a-z]+=[0-9]+' | tr '\n' ' '; echo; done
```
Expected:
- `== out/dm_all_pair.log: 주입한 탭 수와 touch 증가가 모두 일치`. 불일치 줄이 있으면 그 시각에 사용자가 화면을 만졌는지 컨트롤러에게 확인을 부탁한다. 재부팅(플래시·`reboot`)을 건너간 줄에서 `touch` 가 줄어든 것(예: `20→0`)은 재부팅이다 — 불일치로 세지 않는다.
- 로그마다 `wifi_pass=0 relay_url=0 relay_host=0 token=0`, `secret rc=0`. (`rc=2` 는 "값을 못 읽어 검사하지 못함"이다 — 통과가 아니다.)
- `그중 msg 가 든 줄 0건`, `오류 줄 0건`.
- 두 기기의 `heap_int_min` ≥ 100000, `stk_app`·`stk_touch`·`stk_relay`·`stk_ws` ≥ 1024 (Phase 2 Task 8 의 최저: 269043 / 3136). 값을 보고에 적는다.

- [x] **Step 11: 화면 사진을 직접 열어 판독하고 보고한다** (Read 도구로 한 장씩)

| 사진 | 보여야 하는 것 |
|---|---|
| `out/dm_a_bell.jpg` | "전화가 왔습니다", 받기(초록)·거절(빨강) 그대로, 그 아래 줄에 **벨 끄기**(짙은 회색 바탕·밝은 글자)와 **메시지로 거절**(짙은 회색 바탕·빨간 글자) |
| `out/dm_a_muted.jpg` | 같은 화면에서 왼쪽 아래가 **벨 꺼짐**(흐린 글자), 나머지 셋은 그대로 |
| `out/dm_c_picker.jpg` | 위에 "보낼 메시지를 누르면 거절됩니다", 그 아래 한 줄에 문구 버튼 셋(회의 중이에요 / 지금은 받을 수 없어요 / 잠시 후 다시 걸게요), 왼쪽 아래 **뒤로**. 가운데는 비어 있다 |
| `out/dm_c_notice1.jpg` · `dm_c_notice2.jpg` · `dm_d_6s.jpg` | 대기 화면 위쪽 안내 상자에 "상대: 회의 중이에요" · "상대: 지금은 받을 수 없어요" · "상대: 잠시 후 다시 걸게요" |
| `out/dm_d_15s.jpg` | 같은 대기 화면에 안내 상자가 **없다**(`dm_d_6s.jpg` 에는 있다 = 안내가 4초가 아니라 10초) |

공통: 글자가 Pretendard 로 또렷한지, 글자·버튼이 겹치거나 잘리지 않는지, 화면이 뒤집히지 않았는지. 사진 6장(위 표의 앞 6개)은 컨트롤러가 사용자에게 보여 준다(새 버튼의 색 — 설계서 §11-1).
보고에는 Step 2~10 의 결과표(시나리오 H·A~G·I 각각 통과/실패, 다시 돌린 횟수와 이유), 두 기기의 버전 세 줄, 터치 대조·비밀·교환기 기록·여유 값, 사진 판독을 적는다.

**결함이 나오면** (다시 돌려도 같은 `!!` 가 나오거나 화면이 표와 다를 때 — 추측으로 고치지 않는다)
1. 두 기기의 `FSM`·`SCREEN` 줄과 교환기 로그의 `call_end` 줄을 시간순으로 나란히 놓고, 어느 쪽이 어떤 신호를 못 받았는지(또는 어떤 터치가 안 먹었는지) 먼저 확정한다.
2. 같은 기기에서 동작하는 구현과 줄 단위로 대조한다: 이 기능 직전의 `ui.cpp`·`app.cpp`(`git show a909f7d:firmware/tab5_phone/main/app.cpp`), 스냅부스 `firmware/tab5_snapbooth/main/main.cpp`, `managed_components/espressif__esp_websocket_client` 소스.
3. 원인이 판단 로직(`call_fsm.h`·`relay_proto.h`·`ui_layout.h`·`phone_core.py`)이면 그 결함을 드러내는 **시험을 먼저** 쓰고(실패 확인) 고친다. `ui.cpp`·`app.cpp` 면 Task 5 Step 2 의 표와 다시 대조한다.
4. 고쳤으면 Task 5 Step 4·6 대로 다시 빌드하고(새 검사합), **두 기기 모두** Step 3·5 의 방법으로 다시 올린 뒤, 실패한 시나리오와 그 앞의 시나리오부터 다시 돌린다. 커밋: `git add <고친 파일> && git commit -m "fix(phone): <무엇을> — 기기 검증에서 드러남" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"`.
5. 다음 경우는 고치려 들지 말고 **멈추고 보고한다(BLOCKED)**: 플래시가 `Hash of data verified.` 없이 끝남 / 기기가 `PHONE BOOT` 를 찍지 않거나 `PHONE FAIL` 을 찍음 / 같은 실행이 세 번 연속 실패하는데 원인을 확정하지 못함 / 터치 대조에서 사람 손이 아닌 불일치가 남음 / 범위 밖 파일(`relay_link`·`phone_relay.py`·`phone_main.cpp`·`gfx`)을 고쳐야 해 보임.

---

### Task 7: 기록 — 설계서 체크·진행 로그, 앞 계획으로 넘길 목록

**Files:**
- Modify: `docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md`(§8 체크, §12 진행 로그, 머리의 상태), `docs/exec-plans/2026-10-04-tab5-phone-decline-mute-plan.md`(이 계획서의 체크박스·상태·진행 로그)

**Interfaces:**
- Consumes: Task 1~6 의 보고(커밋 SHA, 시험 개수, 빌드 크기·버전, 시나리오 결과, 스크린샷 판독).
- Produces: 아래 "Phase 2 계획 Task 10 으로 넘기는 것" 목록(이 계획서에 이미 적혀 있다 — 실행 결과에 맞게 고칠 것만 고친다).

- [x] **Step 1: 이 기능 설계서를 갱신한다**

1. 머리의 `- **상태**:` 를 "구현·기기 자동 검증 완료(날짜) — 손 조작·독립 검수는 Phase 2 계획 Task 9"로.
2. §8 의 체크박스: 맥 단위 시험 / 교환기 pytest / `ui_assets.h` 재생성 / 펌웨어 빌드 / 기기 자동 시험 A~I 다섯 줄을 실제로 통과한 것만 `- [x]` 로. 손 조작·독립 검수·문서 반영 세 줄은 그대로 둔다(Task 9·10 의 몫).
3. §12 진행 로그 끝에 `### <날짜> — 구현·기기 검증 (계획 docs/exec-plans/2026-10-04-tab5-phone-decline-mute-plan.md)` 절을 더하고 다음을 **실제 값으로** 적는다(못 잰 것은 "확인 못 함 — 이유"):
   - 커밋 다섯 개(교환기 / 판단 헤더 / 도구 / 자산 / 화면·진행)의 짧은 SHA, 기기 검증에서 고친 커밋이 있으면 그것도.
   - 시험 개수(맥 105·61·86·9·16, 교환기 pytest 137, 검증 도구 150)와 실제로 나온 값이 다르면 그 값.
   - 빌드: 바이너리 크기, 우리 코드 경고 수, 버전 문자열, 두 기기의 `App version`·`Compile time`·`ELF file SHA256` 이 같았는지.
   - 시나리오 H·A~G·I 각각의 결과와 다시 돌린 횟수·이유, 스크린샷 판독, 안내 10초 확인(D).
   - 터치 대조 결과, 비밀 검사 결과, 교환기 기록의 `msg` 건수, `heap_int_min`·`stk_*` 최저.
   - 설계와 달라진 것(이 계획의 DMP-D4, 기기에서 고친 것)과 남은 것(손 조작 §7.3, Phase 3 로 넘기는 확인 §7.4).

- [x] **Step 2: 이 계획서의 체크박스·상태·진행 로그를 갱신하고 커밋한다**

```bash
cd ~/tab5-toy-phone && git add docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md docs/exec-plans/2026-10-04-tab5-phone-decline-mute-plan.md && git commit -m "docs(phone): 수신거부 메시지·벨 끄기 구현·기기 검증 결과 기록" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [x] **Step 3: 넘김 목록을 보고에 그대로 싣는다** (컨트롤러가 Phase 2 계획 Task 9·10 의 dispatch 에 붙인다)
  - 처리(2026-10-04): 아래 1~4번은 같은 문서 담당이 Phase 2 계획 Task 10 의 문서에 직접 반영했고(통화 설계서 §9.4·§10·§8, `firmware/tab5_phone/README.md`), 5번은 컨트롤러가 Task 9 dispatch 에 붙였으며, 6번은 통화 설계서 §8 의 "Phase 3 으로 넘기는 것"에 실었다. 7번(프로젝트 메모리)은 컨트롤러 몫.

**Phase 2 계획 Task 10(문서) 으로 넘기는 것**

1. 통화 설계서 `docs/exec-plans/2026-10-03-tab5-phone.md` §9.4 신호 표에 두 줄:
   - `| 기기 → 교환기 | {"t":"reject","msg":N} (N = 1~3) | 문구 N 과 함께 거절(수신거부 메시지). 교환기는 1~99 의 정수만 건네고, 그 밖의 값은 번호 없는 거절로 낮춘다 |`
   - `| 교환기 → 기기 | {"t":"ended","why":"reject","msg":N} | 문구 N 과 함께 거절로 끝남(양쪽에 간다). 기기는 1~3 만 문구로 읽고 그 밖은 그냥 거절로 본다. 통화 기록에는 번호를 남기지 않는다 |`
2. 같은 설계서 §10 화면 표: "전화 옴" 줄에 "아래 줄에 벨 끄기(끈 뒤 '벨 꺼짐')·메시지로 거절"을 더하고, 새 줄 `| 메시지 고르기 | 안내 문구 + 문구 버튼 셋(위쪽 한 줄) + 뒤로. 통화 상태는 벨 그대로(30초 제한·상대 취소가 그대로 돈다) |`, "대기" 줄에 "거절 문구 안내 '상대: …' 10초(그 밖의 안내는 4초)".
3. 같은 설계서 §8 진행 로그에 한 절: 이 기능의 설계서·계획서 경로와 결과 요약(이 기능 설계서 §12 의 절을 가리킨다). Phase 2 계획의 진행 로그에 한 줄: "Task 8 뒤·Task 9 앞에 수신거부 메시지·벨 끄기를 넣음(별도 설계서·계획)", 그리고 "교환기(수정하지 않는다)"가 이 기능에 한해 바뀌었다는 것(DM-D5).
4. `firmware/tab5_phone/README.md`(Task 10 이 만든다)에 실을 것:
   - 시리얼 규약 추가분: `SCREEN DECLINE`(이때 `STAT … fsm=INCOMING screen=DECLINE`), 안내 이름 `REJECT_MSG`, `FSM` 줄 끝의 ` msg=<1~3>`(번호가 있을 때만). 그냥 거절을 엄격히 볼 때는 `notice=REJECTED$`.
   - 주입 좌표: 벨 끄기 `404,624` · 메시지로 거절 `876,624` · 문구 1·2·3 `240,168`·`640,168`·`1040,168` · 뒤로 `404,624`. 화면이 바뀐 뒤의 탭 앞에는 `sleep:1`.
   - 가짜 상대의 `send:reject:<n>`·`expect:ended:reject:<n>`(`0` = 번호가 없어야 함).
   - 문구를 바꾸는 법: `tools/gen_ui_assets.py` 의 `REJECT_MSGS` → 다시 굽기 → 두 기기 다시 플래시. 문구 수를 바꾸려면 `call_fsm.h` 의 `MSG_COUNT`, `ui_layout.h` 의 `Tap::MSG*`·`BTN_MSG`, `ui.cpp` 의 배열 둘을 함께(어긋나면 `static_assert` 가 빌드를 막는다).
   - 이 기능 직전 펌웨어의 보관본 `out/fw_prev/`(맥북에만, git 제외)와 그것으로 되돌리는 명령(이 계획서 "롤백").
5. Task 9(손 조작·독립 검수)에 더할 것: 손 조작 목록에 이 기능 설계서 §7.3 의 항목, 독립 검수 범위에 이 계획의 커밋들과 `out/dm_*.log`, 12분 대기의 "touch 불변" 조건은 그대로.
6. Phase 3(소리) 계획으로: 이 기능 설계서 §7.4 의 확인 다섯 가지와 §5.3 의 약속(소리 모듈은 `A_TONE_STOP` 뒤 스스로 다시 울리지 않는다, 이미 멈춘 뒤의 `A_TONE_STOP` 은 무해).
7. 맥북의 프로젝트 메모리(전화기 진행 상태)에 한 줄 — 컨트롤러 몫.

---

## 검증 기준 (이 계획의 완료 조건)

설계서 §8 을 과제와 짝지은 것.

- [x] 맥 단위 시험: 기존 시험이 **수정 없이** 통과하고(73·46·59·9·16 이 줄지 않는다) 설계서 §7.1 의 새 시험이 들어가 통과한다 — `105 ok`·`61 ok`·`86 ok`·`9 ok`·`16 passed`, 컴파일 경고 0. → Task 2 Step 7  (결과: `113097c` 에서 105·61·86·9·16 그대로, 경고 0. 수정 묶음 뒤 `066cf57` 은 ui_layout 91)
- [x] 교환기 pytest: 기존 87개가 수정 없이 통과하고 새 시험이 통과한다(`137 passed`), 검증 도구 `150 passed`. → Task 1 Step 4, Task 3 Step 2·3  (결과: 137 passed, 도구 150 passed. 수정 묶음 뒤 도구 152)
- [x] 교환기 기록의 `call_end` 줄에 `msg` 가 없다. → Task 1 `test_reject_msg_is_not_logged`·`test_relay_reject_with_msg`, Task 6 Step 10  (결과: 기기 시험 중 `call_end` 27건, `msg` 든 줄 0건)
- [x] `ui_assets.h` 재생성: `git diff --numstat` 이 `4010	0`(삭제 줄 0), 자산 86개. → Task 4 Step 2  (결과: `4010 0`, 자산 86개)
- [x] 펌웨어 전체 빌드 성공, 우리 코드 경고 0, 부품 판 그대로. → Task 5 Step 4·6  (결과: `113097c` 0x2312c0, 경고 0, `dependencies.lock` 불변)
- [x] 기기 자동 시험 H·A~G·I 통과, 두 기기에 같은 바이너리. → Task 6  (결과: `113097c` 빌드, 다시 돌린 실행 0회, 두 기기 버전 세 줄 동일)
- [x] 수정 빌드 `066cf57` 을 두 기기에 올려 같은 시험을 다시 통과시켰다. → 시나리오 A·B·C·D·E·F·G·I 전부 `!!` 0, 다시 돌린 실행 0회, 사진 8장은 `113097c` 때와 바이트까지 같다(설계서 §12).
- [ ] 스크린샷 6종을 사용자에게 보여 줬다. → 사진은 찍어 판독했다(설계서 §12). 사용자에게 보여 주는 것은 게이트 보고에서 한다  (※ 위 두 줄은 원래 "기기 자동 시험" 줄의 뒷부분이었으나 나누어 적었다)
- [x] 시험 로그에 비밀 0건, 주입한 탭 수와 `touch=` 증가가 일치, `heap_int_min` ≥ 100000·`stk_*` ≥ 1024. → Task 6 Step 10  (결과: 비밀 0건, 주입 80 = 증가 80, `heap_int_min` 끝 STAT 271983/271471, `stk_*` 3252 이상 — 모두 `113097c` 빌드. `066cf57` 재회귀의 끝 STAT 는 262539/263187)
- [x] `git status --short` 에 `out/`·`build/`·`fonts/`·`secrets.h` 가 없다.  (결과: 2부 문서 작업 중 `git status --short` 에 고친 문서만 보이고 `out/`·`build/`·`fonts/`·`secrets.h` 는 없다. 기기 시험 자체는 `out/` 에만 산출물을 만들었고 그 폴더는 git 제외)
- [x] 이 기능 설계서 §8·§12 와 이 계획서가 갱신됐고, 넘김 목록이 컨트롤러에게 갔다. → Task 7
- 이 계획 밖(Phase 2 계획이 맡는다): 사용자 손 조작(설계서 §7.3) → Task 9 Step 3 / 독립 검수 95점 이상 → Task 9 Step 4 / 통화 설계서 §9.4·§10·§8 과 README → Task 10.

## 초안 목록

패치는 저장소 루트 기준 경로의 통합 diff 다. 파일 하나는 패치 하나에만 들어 있다(패치 = 그 파일의 지금 → 최종). `D` = `~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-decline-mute-plan/draft`. 패치를 모두 적용한 뒤의 파일 전문은 `D/files/<저장소 경로>`(15개)에 있다.

| 초안 파일(`draft/` 기준) | 줄 수 | sha256 앞 12자 |
|---|---|---|
| `patches/T1-01-relay-tests.patch` | 109 | `d30163134e6a` |
| `patches/T1-02-relay-core.patch` | 65 | `5f839c02b93c` |
| `patches/T2-01-call-fsm-tests.patch` | 160 | `5854a33d2146` |
| `patches/T2-02-call-fsm.patch` | 97 | `296c0a5ba56a` |
| `patches/T2-03-relay-proto-tests.patch` | 71 | `80df1eed1321` |
| `patches/T2-04-relay-proto.patch` | 59 | `96564b79968b` |
| `patches/T2-05-ui-layout-tests.patch` | 94 | `20ffef144557` |
| `patches/T2-06-ui-layout.patch` | 110 | `06327f2e0de2` |
| `patches/T3-01-fake-peer-tests.patch` | 126 | `5bb9a4618083` |
| `patches/T3-02-fake-peer.patch` | 99 | `1062787641de` |
| `patches/T3-03-phone-ctl-comment.patch` | 10 | `9dc0b8db23b6` |
| `patches/T4-01-gen-ui-assets.patch` | 33 | `e4c03e469725` |
| `patches/T5-01-ui.patch` | 70 | `145ea30ed249` |
| `patches/T5-02-app.patch` | 108 | `e7bbaa203f1a` |
| `verify/secret_scan.py` | 79 | `8350672ef27a` |

기기 검증 때 쓰는 도구(저장소에 넣지 않는다 — `D/verify/` 에서 그대로 실행한다. 가짜 값으로 시험했다: 깨끗한 로그 `rc=0`, 값이 든 로그 `rc=1`, 값을 못 읽으면 `rc=2`):

```python
#!/usr/bin/env python3
"""secret_scan.py — 기기·교환기 로그에 비밀 값이 실렸는지 "개수만" 센다. 값은 어디에도 출력하지 않는다.

  python3 secret_scan.py <로그 파일>...                                   저장소의 secrets.h 와 server/.env.phone 에서 값을 읽는다
  python3 secret_scan.py --secrets <secrets.h> --env <.env.phone> <로그>...   다른 파일로(이 도구를 시험할 때)

찾는 값: Wi-Fi 비밀번호(WIFI_NETWORKS 의 둘째 값), 교환기 주소(RELAY_URL 전체와 그 안의 호스트), 기기 토큰(PHONE_DEVICES 의 넷째 값과 .env.phone 의 PHONE_TOKENS).
SSID·MAC·기기 ID 는 비밀이 아니라 세지 않는다. 4글자 미만인 값은 우연히 맞기 쉬워 건너뛴다.
끝나는 값: 0 = 모든 로그에서 0건 / 1 = 한 건이라도 있음 / 2 = 읽을 값이 없음(파일이 없거나 모양이 다름 — 검사한 것이 아니다).
"""
import argparse
import re
import sys
from pathlib import Path

REPO = Path("~/tab5-toy-phone")
STR = r'"((?:[^"\\]|\\.)*)"'


def macro_body(text, name):
    """'#define NAME …' 의 몸통(줄 끝 \\ 로 이어진 줄 포함). 없으면 빈 글자."""
    m = re.search(r"^[ \t]*#[ \t]*define[ \t]+" + re.escape(name) + r"\b((?:.*\\\n)*.*)", text, re.M)
    return m.group(1) if m else ""


def unesc(s):
    return re.sub(r"\\(.)", r"\1", s)


def load(secrets_path, env_path):
    """{갈래: 값 집합}. 값은 이 함수 밖으로 '개수'로만 나간다."""
    vals = {"wifi_pass": set(), "relay_url": set(), "relay_host": set(), "token": set()}
    if secrets_path.is_file():
        text = secrets_path.read_text(encoding="utf-8", errors="replace")
        for _ssid, pw in re.findall(r"\{\s*" + STR + r"\s*,\s*" + STR + r"\s*\}", macro_body(text, "WIFI_NETWORKS")):
            vals["wifi_pass"].add(unesc(pw))
        for url in re.findall(STR, macro_body(text, "RELAY_URL")):
            vals["relay_url"].add(unesc(url))
            host = re.sub(r"^\w+://", "", unesc(url)).split("/")[0].rsplit(":", 1)[0]
            vals["relay_host"].add(host)
        for _mac, _unit, _dev, tok in re.findall(r"\{\s*" + STR + r"\s*,\s*(\d+)\s*,\s*" + STR + r"\s*,\s*" + STR + r"\s*\}", macro_body(text, "PHONE_DEVICES")):
            vals["token"].add(unesc(tok))
    if env_path.is_file():
        for line in env_path.read_text(encoding="utf-8", errors="replace").splitlines():
            k, sep, v = line.strip().partition("=")
            if sep and k.strip() == "PHONE_TOKENS":
                for part in v.strip().strip("\"'").split(","):
                    tok = part.partition(":")[2].strip()
                    if tok:
                        vals["token"].add(tok)
    return {k: {v for v in s if len(v) >= 4} for k, s in vals.items()}


def main(argv=None):
    ap = argparse.ArgumentParser(description="로그에 비밀 값이 있는지 개수만 센다")
    ap.add_argument("--secrets", type=Path, default=REPO / "firmware/tab5_phone/main/secrets.h")
    ap.add_argument("--env", type=Path, default=REPO / "server/.env.phone")
    ap.add_argument("logs", nargs="+", type=Path)
    a = ap.parse_args(argv)
    vals = load(a.secrets, a.env)
    empty = [k for k, s in vals.items() if not s]
    if empty:
        print(f"값을 못 읽음: {', '.join(empty)} — 검사하지 못했다", file=sys.stderr)
        return 2
    rc = 0
    for path in a.logs:
        if not path.is_file():
            print(f"{path}: 파일 없음", file=sys.stderr)
            return 2
        text = path.read_text(encoding="utf-8", errors="replace")
        counts = {k: sum(text.count(v) for v in s) for k, s in vals.items()}
        print(f"{path}: " + " ".join(f"{k}={n}" for k, n in counts.items()) + f"  (값 {sum(len(s) for s in vals.values())}개로 검사)")
        if any(counts.values()):
            rc = 1
    return rc


if __name__ == "__main__":
    sys.exit(main())
```

## 진행 로그

### 2026-10-04 — 계획 작성
- 승인된 설계서(§11 의 열린 질문 셋은 제안대로 확정)를 요구사항으로 삼아, 설계서 작성 때의 시제품(헤더 3개·`phone_core.py`·시험 초안)을 출발점으로 다시 만들고 다시 검증했다. 저장소의 코드·기기·시리얼·교환기·`build/`·`out/` 은 건드리지 않았다.
- 검증(저장소 밖 사본, 저장소 `10f1934` 기준):
  - 깨끗한 사본(`git archive HEAD`)에 패치 14개를 과제 순서대로 `git apply` 하며 실패 → 통과를 실제로 봤다. 실패 문구는 각 Step 의 Expected 그대로다. 최종 파일은 `D/files/` 와 15개 모두 같았다. 패치 14개 모두 지금 저장소에서 `git apply --check` 통과.
  - 맥 단위 시험 73·46·59·9·16 → **105·61·86·9·16**(경고 0), 교환기 pytest 87 → **137**(교환기 23 + 가짜 상대 27 추가), 검증 도구 **150** 유지. 기존 시험은 한 줄도 고치지 않았다(시험 패치의 삭제 줄 0).
  - 새 시험이 실제로 잡아내는지 구현을 일부러 망가뜨려 확인(16가지 — 범위 검사 제거, 참·거짓 검사 제거, 기록에 번호 넣기, `hangup` 에 번호 건네기, 가짜 상대가 번호를 안 봄, 32비트로 잘라 비교, 숫자만 검사 제거, 거절이 아닌 종료에 번호, `enter()`·`offline()` 이 벨 꺼짐을 안 지움, 벨 끄기가 시간 제한을 다시 셈, 범위 밖 번호를 실어 보냄, `sync` 무력화, 끈 뒤에도 버튼, 문구 줄을 통화 버튼 높이로, 고르기 화면에서 안내 상자): 전부 1개 이상 실패했다.
  - 글자 자산: `labels=66 digits=20 total=86 alpha_bytes=646,515 file_bytes=1,923,795`, 검사합 `c2d92346…`, `git diff --numstat` `4010 0`. 새 문구 11개를 미리보기로 직접 봤다(Pretendard, 크기는 설계서 §5.5 표와 같다).
  - 펌웨어 전체 빌드(자리 값 `secrets.h`): 변경 전 0x219b30 → 변경 후 **0x2312b0**(+96KB, 앱 파티션의 63% 여유), 우리 코드 경고 0, `dependencies.lock` 은 저장소 것과 바이트까지 같다. 헤더만 바꾼 중간 상태는 `-Werror=switch` 로 실패함도 확인했다(DMP-D1).
  - `ui.cpp`·`app.cpp` 는 설계서 §5.6 의 항목을 코드와 하나씩 대조했다(Task 5 Step 2 의 표).
- 설계서와 다르게 한 곳: DMP-D4 의 안전장치 셋 / 안내가 끝날 때 문구 번호도 함께 지운다(`app.cpp` `refresh()`) / 시험 몇 개 추가("bell starts once…", "message reject crossed with the caller's cancel…", "(2) mute/open-picker/back do not overlap the call button…" 등) / 문서 과제를 줄이고 기기 확인을 Task 6 으로 모음(컨트롤러 지시 — DMP-D3·D5). 설계서 §7.2 의 "선택"(가짜 상대로 틀린 번호를 실기 화면에서 보기)은 넣지 않았다 — 1번 전원을 꺼야 하고, 같은 동작을 교환기·해석기·상태 기계 시험이 고정한다.
- **확인하지 못한 것**: 기기에서의 동작 전부(화면 모양, 터치, 10초 안내, 섞인 상태) — Task 6 이 처음 본다. Task 6 의 `phone_ctl.py` 명령 15개(단계 256개)는 도구의 실제 문법 검사(`plan_steps`)에 넣어 통과시켰고 로그 후처리 명령은 가짜 로그로 돌려 봤지만, **기기에 대고 실행해 보지는 못했다**(기기 사용 금지) — 기대하는 줄은 `app.cpp` 의 `printf` 모양과 상태 기계의 할 일 값에서 계산한 것이다. 실제 `secrets.h` 로 빌드한 크기. 고르는 중 교환기가 끊기는 경우는 기기 시나리오에 없다(맥 시험 "muted call + link down"·"picker: closes when the call leaves INCOMING" 으로만 고정).

### 2026-10-04 — 실행 결과 (Task 1~7) — 자세한 값은 설계서 `2026-10-04-tab5-phone-decline-mute.md` §12
- 실행 순서: 사용자가 설계서를 "제안대로 승인" → 이 계획서 작성 → 사용자 지시("속도를 내자")로 바로 실행. 코드 과제 1~5 를 한 구현 담당이 이어서 적용해 14:23 에 시작했고(과제 묶음 검수 1회), 기기 과제 6 은 Phase 2 계획 Task 9 Step 1·2 측정(14:32 끝)과 겹치지 않게 그 뒤 14:53~15:05 에 돌렸다. 사용자는 이 사이 자리를 떴다.
- Task 1~5: 모든 Step 이 Expected 와 일치(RED 문구·개수 포함). 커밋 `6ff6108` 교환기 · `618eebf` 판단 헤더 · `ff90e5b` 가짜 상대 도구 · `5971d4c` 글자 자산 · `113097c` 화면·진행(사이에 devsync `acd92f0` 가 RED 상태 시험 파일을 먼저 담음). 시험 105·61·86·9·16 / 137 / 150, 경고 0, 빌드 0x2312c0(63% 여유). 검수: 품질 Approved, Critical 0, Important 1(DMP-D1 — 중간 커밋 빌드 불가, 계획이 정한 것), Minor 6.
- Task 6: Step 1~11 모두 첫 시도에 통과(다시 돌린 실행 0회, `!!` 0). 시나리오 H·A~G·I 통과, 두 기기 같은 바이너리, 터치 주입 80 = 증가 80, 비밀 0건, 교환기 `call_end` 27건에 `msg` 0건. 검수: Approved, Critical 0 · Important 0 · Minor 4. 이 결과는 **`113097c` 빌드** 기준이다.
- 계획과 달라진 것: ① 계획의 "결함이 나오면 고친다"를 컨트롤러가 "고치지 말고 진단·보고"로 좁혔다(사용자 부재 중 검수 없이 두 기기에 다시 올리지 않으려고) — 결함이 없어 쓰이지 않았다. ② 롤백 문구 정정(위 "롤백" 절).
- Task 7(이 기록): 설계서 §8 체크·§12 결과·머리 상태를 갱신했고, 넘김 목록 1~4번을 Phase 2 계획 Task 10 의 문서(통화 설계서 §9.4·§10·§8, 새 `firmware/tab5_phone/README.md`)에 직접 반영했다. Phase 2 계획의 진행 로그에 "Task 8 뒤·Task 9 앞에 수신거부 메시지·벨 끄기를 넣음(별도 설계서·계획)"과 "교환기가 이 기능에 한해 바뀜(DM-D5)"을 적었다.
- 그 뒤: 최종 전체 검수 두 번 모두 93점(미달, Important 1 — 비밀번호 자판 화면 스크린샷). 수정 묶음 `99466b9`·`a26d237`·`066cf57`(위 비목표 절의 정정 참조)이 이 기능 코드(`ui_layout.h` 시험 +5 등)에도 닿았다. 수정 빌드 `066cf57` 의 두 기기 재플래시(2번 → 1번, 같은 바이너리 `App version 066cf57`)와 Task 6 Step 6~9 재회귀(A·B·C·D·E·F·G·I 전부 통과, 다시 돌린 실행 0회)·자판 화면 `shot` 거절(가짜 글자 1개부터 거절, 지우면 다시 찍힘)·교환기 무응답 때 `RELAY_SILENT`·설정 화면이 열린 채 전화(벨 화면으로 바뀜)를 모두 통과했다. 수정 뒤 재검수 96/100(opus, diff 한정; 새 Minor 2 — Phase 3 첫 과제). 자세한 값은 설계서 §12
- 손 조작(설계서 §7.3): 앞선 두 창은 터치 0이었고 세 번째 창(20:54~21:04, `066cf57`, `out/hand_3.log`)에서 사용자가 직접 했다 — 메시지 3 거절·고르다 뒤로 → 받기·그냥 거절·걸다 취소·받기/끊기, 오류 0, 사용자 "좋아. 모두 좋아.". 벨 끄기·문구 1·2·연타·거절 가장자리 길게 누르기는 손으로 미확인(자동 시험으로만). 새 보조 버튼 윤곽(1.07:1)은 사용자가 "모두 좋아"로 답해 그대로 둔다. 남은 것: Phase 3 의 소리 확인(설계서 §7.4).
