# Tab5 음성 전화기 — Phase 3a 신호음(벨·연결음·종료음) 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> (실행하는 에이전트에게: 위 두 스킬 중 하나로 과제를 하나씩 수행하고, 단계는 체크박스로 추적한다.)

- **작성일**: 2026-10-04
- **상태**: 구현·기기 자동 검증·최종 검수(96점) 완료 — **사용자 귀 확인 대기** (2026-10-04 밤. 두 기기에 `5d9935f` 올라가 있음. 스피커에서 실제로 소리가 나는지는 아직 확인되지 않았다 — 아래 진행 로그 "실행 결과") — **2026-10-05 갱신**: 3b(목소리)가 소리 장치를 송수신으로 바꾼 뒤(두 기기 `5935ddd`) 스피커에서 실제로 소리가 난다는 것은 기록(`SELFTEST amp=1` 의 `mic`)과 사용자의 통화로 확인됐다. **신호음의 음량·잡음의 느낌, 벨 끄기로 실제 소리가 그치는지는 여전히 사용자 답이 없다**(아래 진행 로그 "2026-10-05 — 귀 확인 칸").
- **작성자**: Claude (맥북 세션)
- **넣는 자리**: 통화 설계서 §5 의 Phase 3(소리 경로)을 둘로 나눈 앞쪽. 사용자 지시(2026-10-04 21:1x "가자 가자! 속도를 내자!")로 **3a = 신호음만** 먼저 하고, 3b = 목소리 통화(마이크·에코 제거·Opus·지터 버퍼)는 따로 계획한다.

**Goal:** 전화가 오면 받는 쪽에서 **벨**이 울리고, 거는 쪽에서는 **연결음**이 들리고, 통화가 끝나면 **종료음**이 나며, "벨 끄기"를 누르면 **실제 벨소리가 멈춘다** — 받기·거절·상대 취소·시간 초과·교환기 끊김 어느 길로 벨이 끝나도 소리가 남지 않고, 다음 전화에서는 다시 울린다. 통화 흐름·화면·교환기·기존 시리얼 줄은 지금과 똑같다(기존 시험을 고치지 않고 전부 통과).

**Architecture:** 판단(박자·주파수·크기, 5ms 페이드, 멈춤 약속, 할 일 → 소리, 기록 줄 고르기)은 하드웨어와 무관한 헤더 `tones.h` 에 두고 맥에서 시험을 먼저 쓴다. 기기 전용 코드는 둘뿐이다: `audio_out`(ES8388 코덱과 I2S 송신을 열고, 공급자에게 16ms 씩 PCM 을 받아 내는 출력 태스크 — 무엇을 내는지는 모른다)과 `tone_out`(신호음 공급자 — 길이 1 우편함으로 명령을 받고 `TONE` 줄을 찍는다). `app.cpp` 는 상태 기계가 Phase 2 부터 내고 있던 `A_TONE_*` 를 `tones::from_acts` 로 "지금 내야 할 소리" 하나로 바꿔 넘기기만 한다. 3b 의 목소리는 같은 출력 모듈의 둘째 공급자가 된다.

**Tech Stack:** ESP-IDF 5.5.4 (esp32p4) · M5Unified 0.2.23 / M5GFX 0.2.30 · esp_codec_dev 1.6.2(ES8388) + esp_driver_i2s(표준 I2S 송신) · clang++ (맥 단위 시험) · Python 3 + pytest + pyserial

**Spec:** 통화 설계서 `docs/exec-plans/2026-10-03-tab5-phone.md` §5(Phase 3)·§9.5(모듈 표의 `board_audio`·`tones`)·§9.6·§8 "Phase 3 으로 넘기는 것", 수신거부 설계서 `docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md` §5.3(소리 모듈과의 약속)·§7.4(소리 단계에서 확인할 다섯 가지). 3a 는 별도 설계서 없이 **이 계획서가 설계를 겸한다** — 정한 것은 아래 "이 계획에서 내린 결정"(PL3a-D1~D16)에 이유·비용·되돌리는 법과 함께 있다.

## 한눈에 보기 (사용자용)

| 과제 | 하는 일 | 기기 | 사용자가 할 일 | 걸리는 시간(어림) |
|---|---|---|---|---|
| Task 1 | 신호음의 박자·음 높이·크기와 "멈추면 다시 울리지 않는다"를 정하는 판단 로직을 넣는다(시험 먼저) | 없음 | 없음 | 5분 |
| Task 2 | 스피커로 소리를 내보내는 모듈과 신호음 모듈을 넣는다(아직 통화 흐름에 잇지 않는다) | 없음 | 없음 | 5분 |
| Task 3 | 통화 흐름에 소리를 잇고, Phase 2 검수가 넘긴 작은 정리 셋(I2C 잠금 자리, 표시 한 줄, 결정 번호 중복)을 함께 넣는다 | 없음 | 없음 | 10분 |
| Task 4 | 지금 기기에 올라가 있는 판(소리 없는 `066cf57`)을 보관하고 새로 빌드한다 | 없음(빌드만) | 없음 | 5분 |
| Task 5 | 두 기기에 올려 자동으로 걸고·받고·거절하고·벨을 끄며 소리 상태를 기록으로 확인한다 → 이어서 **사용자가 직접 귀로 확인**한다 | **1번·2번**(각 5분 안팎 다시 올림) | 자동 시험 약 10분 동안 **두 기기 화면을 만지지 않기**(이때 벨·연결음이 실제로 여러 번 울린다 — 무응답 시험은 30초 동안). 끝나면 직접 걸어 보며 점검표 10문항에 답하기 | 25분 |
| Task 6 | 결과를 문서에 적는다(README, 통화 설계서, 이 계획서) | 없음 | 없음 | 10분 |

- **목소리는 아직 없다.** 받으면 벨·연결음이 그치고 조용하다. 목소리는 3b 다.
- 소리의 모양(지금 정한 값 — 귀로 들어 보고 바꿀 수 있다): **벨** = 따르르릉 1초(800·1000Hz 를 번갈아) + 2초 쉼 / **연결음** = 뚜— 1초(440+480Hz) + 2초 쉼 / **종료음** = 높은 음 → 낮은 음, 0.5초에 한 번. **음량 조절은 아직 없다**(고정값 — Phase 0 에서 사용자가 "적당"이라 한 크기에 맞췄다. 조절 화면은 Phase 5).
- 벨 끄기를 누르면 **최대 0.1초 안에** 그친다(이미 스피커 쪽으로 내보낸 소리만큼).
- 두 기기는 시험 뒤에도 전화기로 남는다. 되돌리는 법은 아래 "롤백"(각 5분).
- **실행자에게 — 코드를 옮겨 적지 않는다.** 이 계획에 실린 패치와 도구는 맥북의 `~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3a-tones/draft/`(이하 `D`, git 제외, 맥북에만 있음, **읽기 전용**)에 그대로 있다. 계획서의 코드 블록과 글자 단위로 같다(조립 스크립트가 끼워 넣었다 — 목록과 검사합은 문서 끝 "초안 목록").
  - 수정·새 파일은 `D/patches/TN-MM-….patch` 를 저장소 루트에서 `git apply` 로 적용하고, 적용한 뒤의 파일을 `D/files/<저장소 경로>` 와 `cmp` 로 대조한다(출력 없음 = 같음).
  - **순서를 지킨다**: 시험 패치 → 실패 확인 → 구현 패치 → 통과 확인 → 커밋. 한꺼번에 적용하지 않는다.
  - 적용한 파일을 고치지 않는다. Expected 와 다르면 스스로 고치지 말고 멈춰서 보고한다(실제 출력 첨부).
  - `D/verify/` 의 도구(`touch_audit.py`·`secret_scan.py` — Phase 2 에서 쓰던 것 그대로)는 저장소에 넣지 않고 그 자리에서 실행한다.
  - 그 폴더가 없거나 내용이 다르면 계획서의 코드 블록이 정본이다(블록을 그대로 파일로 옮겨 `git apply --check` 부터).
- 이 계획에 실린 코드는 작성 시점(2026-10-04, 저장소 `61860f1`)에 저장소 밖 사본에서 실제로 돌려 확인했다: 깨끗한 사본에 패치 12개를 과제 순서대로 적용하며 실패 → 통과를 확인(맥 단위 시험 16·105·61·9·91 유지 + 새 `tones` **84**, 검증 도구 pytest 152 → **186**, 교환기 pytest 137 유지), 커밋 넷 가운데 Task 2·정리·연결 뒤 세 곳에서 펌웨어 전체 빌드 성공(0x231280 → **0x242230**, 앱 파티션의 62% 여유, 우리 코드 경고 0, 부품 판 그대로. Task 1 의 커밋은 아직 아무도 포함하지 않는 헤더와 시험뿐이다), 구현을 일부러 망가뜨린 22가지를 새 시험이 전부 잡음. **기기에 올려 본 적은 없다** — 실제로 소리가 나는지, 어떻게 들리는지는 Task 5 에서 처음 확인된다(끝의 "진행 로그 — 확인하지 못한 것").

## Global Constraints

통화 설계서·Phase 2 계획에서 온 전 과제 공통 요구사항. 모든 과제는 이 절을 암묵적으로 포함한다.

- **기기 번호**: 1번 = MAC `AA:BB:CC:DD:EE:01`(교환기 ID `tab5-1`), 2번 = MAC `AA:BB:CC:DD:EE:02`(`tab5-2`).
- **기기는 번호(MAC)로 고른다**: 포트는 항상 `python3 tools/phone_ctl.py port <1|2>` 로 얻고, 플래시 직전에 `read_mac` 으로 다시 확인한다. `/dev/cu.usbmodem*` 를 직접 고르지 않는다.
- **코드 과제(Task 1~4)는 기기·시리얼 포트·교환기를 건드리지 않는다.** 기기는 Task 5 에서만.
- **칩·빌드**: ESP-IDF 5.5.4, 대상 `esp32p4`, 두 기기 모두 P4 rev v1.3(`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`, `CONFIG_ESP32P4_REV_MIN_100=y` — 그대로 둔다). **`esp_hosted` 2.11.7 고정**, Wi-Fi 칩(C6) 펌웨어 교체 금지, `amcchord/M5Tab5-VideoLink` 플래시 금지, `esp_audio_codec` 2.5.0, `esp_codec_dev` 1.6.2. `sdkconfig.defaults`·`main/idf_component.yml` 을 고치지 않는다(소리 부품은 Phase 2 때 이미 들어 있다). `main/CMakeLists.txt` 는 소스 두 줄만 더한다.
- **범위**: 고치는 파일은 아래 "초안 목록"의 패치가 건드리는 것이 전부다. 교환기(`server/`), 화면(`ui.cpp`·`ui_layout.h`·`ui_assets.h`·`gen_ui_assets.py`), 판단 헤더(`call_fsm.h`·`relay_proto.h`), 도구(`phone_ctl.py`·`phone_fake_peer.py`), `server/tools/phone_relay_dev.sh` 를 고치지 않는다. **기존 시험은 한 글자도 고치지 않는다**(시험 패치는 추가만). **기존 시리얼 줄의 모양은 한 글자도 바꾸지 않는다** — `STAT` 의 새 칸은 줄 끝에 더한다.
- **비밀**: `secrets.h`·`server/.env.phone` 을 `cat`/Read 로 열지 않는다. 그 내용(Wi-Fi 비밀번호, 토큰, 교환기 주소)을 화면·로그·보고·커밋에 내지 않는다.
- **이 맥의 함정 네 가지**: ① 맥 단위 시험은 `clang++ … -isysroot "$(xcrun --sdk macosx --show-sdk-path)"` 가 있어야 링크된다. ② `source ~/esp/esp-idf-5.5.4/export.sh` 를 한 셸의 `python3` 에는 Pillow·pytest 가 없다 → 파이썬 도구·시험은 export 하지 **않은** 호출에서, 빌드·esptool 은 export 와 **같은 호출 안에서**(Bash 도구는 호출마다 새 셸이다). ③ 명령을 `sh -c '…'` 로 감싸지 않는다. ④ Bash 도구의 `grep` 은 ugrep 이다 — 파일 여러 개를 한 번에 주면 줄이 나오는 순서가 실행마다 다르다. 이 계획의 명령은 그래서 파일을 하나씩 돌거나 `sort` 한다(적힌 그대로 실행하면 된다). 오래 걸리는 명령(빌드, 30초 무응답 시나리오)은 Bash 의 `timeout` 을 600000 으로 준다.
- **main 에서 직접 작업**(브랜치·worktree 없음). devsync 가 5분마다 `git add -A` 로 자동 커밋한다 → `git commit` 이 "nothing to commit" 이면 devsync 가 먼저 커밋한 것이라 정상이다(보고에 적는다). `git add` 는 그 과제의 파일만(`git add -A` 금지). 임시 파일은 저장소 밖(`/tmp`)이나 git 제외 폴더 `firmware/tab5_phone/out/` 에만 둔다.
- **교환기**: 지금 맥북에서 켜져 있고 사용자가 쓰고 있다. 이 계획은 교환기 코드를 바꾸지 않으므로 **다시 띄우지 않는다** — `bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh status` 로 켜져 있는지만 본다(정확히 이 모양, 한 번의 호출에 이 명령 하나만). 꺼져 있으면 켜지 말고 멈춰서 보고한다(켜는 것은 컨트롤러).
- **두 기기에는 같은 바이너리를 올린다**: 빌드는 Task 4 에서 한 번만 하고, 플래시는 `idf.py flash`(HEAD 가 움직이면 다시 빌드해 바이너리가 달라진다) 대신 빌드 폴더에서 `python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash @flash_args` 를 쓴다(직전에 `read_mac` 으로 대상 확인). 부팅 로그의 `App version`·`Compile time` 이 두 기기에서 같은지 기록한다.
- **멈추라는 조건에 걸리면 멈춘다**: BLOCKED 로 보고하고 기다린다. 원인을 찾았고 더 나은 방법이 떠올라도 실행하지 말고 제안으로만 적는다. 특히 기기에 쓰는 일 앞의 정지 조건은 실행 담당이 스스로 풀 수 없다. **기기 시험에서 결함이 나오면 고치지 않는다** — 진단해서 보고한다(Task 5 "결함이 나오면").
- **터치 대조**: 기기 시험의 각 실행은 `both:until:stat` 으로 시작하고 끝난다. 끝나면 `D/verify/touch_audit.py` 로 주입한 탭 수와 `touch=` 증가를 대조한다. 화면이 바뀐 뒤 같은 기기의 다음 탭 앞에는 1초 이상 쉰다(화면 가드 0.6초).
- **`TONE` 줄의 순서**: `TONE` 줄은 출력 태스크가, `FSM` 줄은 app 태스크가 찍는다 — 둘 사이의 앞뒤는 정해져 있지 않다(PL3a-D16). 기기 명령은 `TONE` 줄을 `wait` 로 기다리지 않는다: 소리 상태는 `until:stat` 의 `tone=` 로, 순서는 실행이 끝난 뒤 기기별 `TONE` 줄만 뽑아 본다.
- **참고 구현 대조 우선(사용자 지시 2026-10-03)**: 기기에서 이상한 동작이 나오면 추측하지 않는다. 이 기기에서 실제로 소리를 낸 Phase 0 시험 펌웨어(`firmware/tab5_phone_probe/main/board_audio.cpp`, 부팅 로그 `firmware/tab5_phone_probe/out/task4.log`)와 줄 단위로 대조한다.
- **진행 안내·보고는 전부 한국어.**

## Review Focus

이 계획이 암시하지만 과제의 시험이 놓치기 쉬운 입력·고장 다섯 가지 — 일어나기 쉬운 것부터. 각 줄의 시험은 해당 과제에 들어 있다.

1. **벨을 끈 뒤 같은 통화에서 다시 울리거나, 끈 것이 다음 전화에 남는다**(엇갈려 온 `ring`, 끈 벨을 또 누름, 끈 채 30초 뒤 부재중) → Task 1 `test_tones.cpp` "muted: stays silent for the rest of that call (stray ring, second mute, 27 s)"·"muted call becomes missed: silent, no end tone"·"the next call rings again"·"after a stop it never rings again by itself (60 s)"·"state machine: nothing while ringing (muted or not) restarts a bell or ringback", Task 5 시나리오 F
2. **벨이 끝나는 여러 길 가운데 하나에서 소리가 남는다**(받기·거절·메시지 거절·상대 취소·30초·상대 사라짐·**벨 도중 교환기 끊김**·받지 않았는데 온 `connected`·40초 가드) → Task 1 "bell ends every way (…): silent in 5 ms, stays silent"·"bell stuck 40 s (relay silent): the guard stops it"·"ringing when the relay link drops: silent"·"a stop after the stop is harmless", Task 5 시나리오 A~E
3. **소리가 바뀌거나 겹치는 순간**(종료음이 울리는 중에 다음 전화, 연결음 → 종료음, 한 바퀴 안의 멈춤 → 벨) — 섞이거나, 새 벨이 삼켜지거나, 딸깍 소리가 난다 → Task 1 "end tone then an immediate new call: the bell takes over within 5 ms"·"... and it is exactly the bell alone (not swallowed, not mixed)"·"end replaces the ringback after a 5 ms fade"·"ended then ring in one batch: …"·"no clicks: …"·"stop mid-tone: no click (fades out)", Task 5 Step 11(귀: 딸깍·찢어지는 소리)
4. **아주 짧은 통화가 연달아 온다** — 소리가 남거나, 뒤의 벨이 안 울리거나, 기록 줄의 짝이 안 맞는다 → Task 1 "very short calls in a row: ends silent, never louder than one bell, no clicks"·"short calls in a row: each one rings, none is left sounding"·"bell and stop before the output task looked: …"·"state machine: never two tone bits at once (every state x event)", Task 5 시나리오 G
5. **소리 장치가 안 열리거나 쓰기가 실패한다 / 소리가 화면·터치·교환기 연결을 늦춘다** — 전화기는 소리 없이 그대로 돌아야 한다 → Task 2 Step 2(실패 경로 대조: `AUDIO FAIL`, 쓰기 실패 때 16ms 쉼), Task 3 `test_phone_ctl.py` 의 `test_audio_off_is_visible_in_the_stat`, Task 5 Step 10(`aud_us`·`stk_audio`·`heap_int_min`·터치 대조·통화 회귀). 기기에서 실패를 일부러 만들 수는 없어 실패 경로는 코드 대조로만 확인한다(진행 로그 "확인하지 못한 것").

---

## 비목표·영향 범위

**넣지 않은 것**(한 줄씩 — 대부분 3b 로):

- 마이크·에코 제거·Opus·음성 프레임 송수신·지터 버퍼·`A_VOICE_*` 실행 → 3b.
- `relay_link` 의 `reconnect()` stop/start 와 보내기 사이 잠금 → 3b(음성 송신을 붙이기 전 필수).
- 터치 시각 기준(M-5), `app` 판단의 순수 구조체화(M-7), 버튼 겹침(M-1), `LINK_UP` 가지 시험, 보내기가 화면 태스크를 멈추는 영향 측정(RL-D6) → 3b 이후.
- 음량 조절(화면·시리얼 명령) → Phase 5. 지금은 상수(`tones.h` 의 `*_AMP`, `audio_out.cpp` 의 `VOLUME`).
- 이어폰 감지, 대기 중 앰프·코덱 끄기(절전) → Phase 5.
- 출력의 객관 측정(마이크 칩의 에코 기준 칸으로 스피커 소리 크기를 재는 것) → 3b 에서 마이크를 열 때. 3a 의 "소리가 실제로 난다"는 사용자 귀가 판정한다.
- 기기에서 "벨 도중 교환기 끊김"·"소리 장치 초기화 실패"를 일부러 만드는 시험 → 맥 시험과 코드 대조로만(교환기는 사용자가 쓰는 중이라 끄지 않는다).
- 새 펌웨어 한 대 + 옛 펌웨어 한 대의 섞인 상태 시험 → 신호 규약이 바뀌지 않아 하지 않는다.
- 화면 사진 → 화면이 바뀌지 않아 찍지 않는다.
- 메시지를 고르는 동안 벨을 함께 끄는 것(수신거부 설계서 §7.4 다섯째) → 귀 확인에서 거슬린다고 하면 따로 한다.

**영향 범위**

- 저장소 파일: 새 파일 6개(`main/tones.h`·`audio_out.h/.cpp`·`tone_out.h/.cpp`, `tools/test_tones.cpp`), 고치는 파일 11개(`main/CMakeLists.txt`·`app.h`·`app.cpp`·`phone_main.cpp`·`relay_link.cpp`·`gfx.h`·`gfx.cpp`·`ui_wifi.cpp`, `tools/test_phone_ctl.py`, `README.md`, 통화 설계서) + 이 계획서.
- 기기: Task 5 에서 2번·1번 순서로 다시 플래시(각 5분 안팎, 저장된 Wi-Fi 목록·SD 카드는 그대로). 그 뒤로는 전화가 오면 **실제로 벨이 울린다**.
- 교환기: 건드리지 않는다. 과금 없음.
- 새 산출물은 git 제외 폴더 `firmware/tab5_phone/out/` 에만 생긴다(`p3a_*.log`·`p3a_*.txt`·`p3a_fw_build.sha256`, `fw_prev_066cf57/`).
- 내부 메모리: 출력 태스크 스택 6KB + I2S DMA 약 3KB + 코덱 드라이버 → 대기 중 `heap_int` 가 10~20KB 줄 것으로 본다(Phase 2 최저 262539 B, 기준 100000 B). 실제 값은 Task 5 Step 10.

## 롤백

- **코드**: 커밋을 뒤에서부터 `git revert` 한다. ① 연결 커밋(Task 3 Step 5)만 되돌리면 소리 모듈은 남지만 쓰이지 않는다(빌드되고, 소리 없는 Phase 2 동작). ② 그다음 출력 모듈 커밋(Task 2), ③ 그다음 `tones.h` 커밋(Task 1). 정리 커밋(Task 3 Step 2, N-1·N-2)은 소리와 무관하므로 남겨도 된다. 순서를 건너뛰면(예: Task 2 만 되돌림) 빌드되지 않는다.
- **기기 → 소리 없는 직전 펌웨어**(`066cf57`, Task 4 Step 1 이 `out/fw_prev_066cf57/build/` 에 보관): `cd ~/tab5-toy-phone/firmware/tab5_phone && source ~/esp/esp-idf-5.5.4/export.sh && PORT=$(python3 tools/phone_ctl.py port 2) && python -m esptool --chip esp32p4 -p "$PORT" read_mac | grep -i "aa:bb:cc:dd:ee:02" && cd out/fw_prev_066cf57/build && python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash @flash_args` (1번은 `port 1`, `aa:bb:cc:dd:ee:01`). 이 명령은 실행해 본 적이 없다(형태는 Phase 2 에서 `build/` 로 올린 명령과 같다).
- **소리만 급히 끄고 싶을 때**(다시 올리지 않고): 방법이 없다 — 음량·소리 끄기 명령은 넣지 않았다. 벨은 전화마다 "벨 끄기"로 끈다.
- **교환기**: 바뀌지 않는다.

## 이 계획에서 내린 결정 (PL3a-D)

통화 설계서의 결정(PH-D1~D9)과 수신거부 설계서의 약속(§5.3)은 그대로 따른다. 여기 것은 3a 를 계획하며 정한 것이다.

| ID | 결정 | 이유 | 비용 | 탈출구 |
|---|---|---|---|---|
| PL3a-D1 | **신호음 모양**: 벨 = 800·1000Hz 를 50ms 씩 번갈아 1초 → 2초 쉼(반복) / 연결음 = 440+480Hz 를 함께 1초 → 2초 쉼(반복) / 종료음 = 660Hz 160ms → 쉼 80ms → 440Hz 280ms(한 번, 0.52초) | 벨은 전자식 전화벨의 떨림이라 "전화"로 바로 알아듣고, 3초 주기라 교환기의 30초 벨 동안 10번 울린다. 연결음은 일반 전화 연결음의 모양(널리 쓰이는 값 — 규격 문서와 대조하지는 않았다). 종료음은 내려가는 두 음이라 "통화 중" 신호(뚜-뚜-)와 헷갈리지 않는다 | 떨리는 벨이 날카롭게 들릴 수 있다(귀 확인 항목) | `tones.h` 의 `BELL_SEGS`·`RINGBACK_SEGS`·`END_SEGS` 표만 바꾼다(맥 시험의 박자 시험도 함께) |
| PL3a-D2 | **크기**: 코덱 음량은 100(0dB) 하나로 고정하고, 소리마다 디지털 최고 크기를 둔다 — 벨 8000, 연결음 5300, 종료음 3700 | Phase 0 에서 1kHz·진폭 8000 을 음량 100 으로 낸 "삐"를 사용자가 "적당"이라 했다(`tab5_phone_probe/out/task4.log` 61~66) → 벨은 그 크기 그대로(방 건너에서도 들려야 한다). 연결음·종료음은 귀 가까이에서 듣는 소리라, Phase 0 시험 음성(말소리 구간 RMS −16.9dBFS)을 음량 90 으로 냈을 때("적당")와 RMS 를 맞췄다(≈ −21.9dB) | 3b 의 목소리도 같은 코덱 음량(100)을 쓰게 된다 — 목소리 크기는 디지털로 정하거나, `VOLUME` 을 바꾸면 신호음도 같이 바뀐다 | `tones.h` 의 `BELL_AMP`·`RINGBACK_AMP`·`END_AMP`, `audio_out.cpp` 의 `VOLUME` |
| PL3a-D3 | **출력 구조**: `audio_out` 은 "공급자에게 PCM 을 받아 낸다"(16ms 마다 `source_fn` 을 불러 받은 것을 합쳐 쓴다). 신호음은 그 공급자 하나(`tone_out`), 공급자는 둘까지 | 3b 가 같은 출력 경로에 목소리를 얹는다 — 출력 모듈이 "무엇을 내는지" 모르면 그대로 둘째 공급자를 더하면 된다 | 둘을 합치는 코드(더하고 넘치면 자름)는 3a 에서 한 번도 돌지 않는다(공급자가 하나) — 3b 에서 처음 쓰일 때 맥 시험으로 옮긴다 | `MAX_SOURCES` 를 1 로, 또는 `out_task` 가 `tones` 를 직접 부른다 |
| PL3a-D4 | **무음일 때**: 앰프는 시작 때 한 번 켜고 끄지 않는다. I2S 송신은 계속 돌고, 낼 소리가 없으면 출력 태스크는 **쓰지 않고 잠든다** — 드라이버가 보낸 칸을 0 으로 지우므로(`auto_clear_after_cb`) 0 이 나간다. 앰프는 코덱을 연 30ms 뒤에 켠다 | 앰프를 켜고 끌 때마다 퍽 소리가 나고 I2C 를 쓴다. 0 을 계속 쓰면 대기 중에도 16ms 마다 깨어난다. 3b 에서 마이크(수신)는 송신 클럭이 살아 있어야 한다 | 대기 중 앰프의 바닥 잡음(쉬 소리)이 들릴 수 있다(귀 확인 항목), 앰프 전력. 부팅 때 한 번의 퍽 소리는 남을 수 있다 | `tone_out` 이 소리 시작 전에 앰프를 켜고 끝난 뒤 끈다(I2C 쓰기 둘, 잠금 안에서) |
| PL3a-D5 | **딸깍 방지**: 울리는 칸마다 처음 5ms 동안 커지고 마지막 5ms 동안 작아진다. 울리는 중에 멈춤·다른 소리가 오면 5ms 동안 줄인 뒤 바꾼다 | 사인파를 중간에서 자르면 딸깍 소리가 난다 | 멈춤이 5ms 늦다(들리지 않는 길이) | `tones.h` 의 `FADE` |
| PL3a-D6 | **명령 전달**: app → 출력 태스크는 길이 1 우편함(`xQueueOverwrite`) — 마지막 명령만 남는다 | 명령 하나가 "지금 내야 할 소리"를 통째로 말하므로 마지막 것만 맞으면 된다. 대기열이 가득 차 멈춤 명령을 잃는 일이 없다 | 출력 태스크가 꺼내기 전(16ms 안)에 벨 → 멈춤이 연달아 오면 그 벨은 한 표본도 나지 않고 `TONE BELL on` 줄도 없다 | 길이 8 대기열 |
| PL3a-D7 | **I2S 는 송신 채널 하나만** 연다(표준 I2S, 16비트 모노). 마이크 칩(ES7210)과 수신 채널은 3b | 3a 의 범위(소리 내기만). Phase 0 부팅 로그에서 수신을 열기 전의 송신이 지금과 같은 설정(`STD: TX, data_bit: 16, slot_bit: 16, … MONO, slot_mask: 0x1`)으로 열린 것을 확인했다(`task4.log` 178) | **이 설정 그대로 소리를 낸 적은 없다** — Phase 0 은 수신까지 연 뒤(슬롯 32비트) 소리를 냈다. 3b 가 마이크를 붙일 때 송수신을 한 번에 만드는 모양으로 바꿔야 한다 | Phase 0 시험 펌웨어 `board_audio.cpp` 의 `i2s_setup`·`init` 을 그대로 가져온다(소리가 안 나면 1순위 가설) |
| PL3a-D8 | **쓰기 단위 = DMA 한 칸 = 256표본(16ms)**, DMA 6칸(시험 펌웨어·xiaozhi 와 같은 칸 수) | P4 는 DMA 칸을 64바이트에 맞춰야 해서 기본 240표본을 256 으로 바꾸며 경고를 낸다(`task4.log` 177) → 처음부터 256 으로 적는다. 쓰기 단위가 칸과 같아야 쉬었다 다시 쓸 때 칸 중간부터 이어 쓰지 않는다 | 출력이 최대 6칸(96ms) 앞서 쓰인다 → 벨 끄기 뒤 최대 약 0.1초 뒤에 그친다 | `audio_out.cpp` 의 `DMA_DESC`(칸 수를 줄이면 빨리 그치지만 끊길 여유도 준다) |
| PL3a-D9 | **출력 태스크**: 코어 1, 우선순위 6, 스택 6KB | 화면·터치와 같은 코어(Wi-Fi·교환기 연결은 코어 0), app(4)·touch(5)보다 위라 화면을 내보내는 60~80ms 동안에도 16ms 박자를 지킨다. 한 묶음(16ms 분량) 계산은 맥에서 3µs — P4 에서는 1ms 에 못 미칠 것으로 어림한다(실제 값은 `STAT` 의 `aud_us`). Phase 0 은 에코 제거·Opus 까지 얹고 30~37% 였다 | 태스크 하나(내부 메모리 6KB) | 상수 `TASK_CORE`·`TASK_PRIO`·`TASK_STACK` |
| PL3a-D10 | **소리 실패는 전화기를 멈추지 않는다**: 시작 실패 → `AUDIO FAIL <단계>` 한 줄, `STAT audio=0`, 재시작하지 않는다. 쓰기 실패 → 횟수만 세고(`aud_err`) 16ms 쉰다 | 벨이 안 울려도 화면으로 전화를 받을 수 있다. 쉬지 않으면 우선순위가 높은 출력 태스크가 화면·터치를 굶긴다 | 소리가 안 나는 것을 화면에서는 알 수 없다(시리얼로만). 통화 설계서 §9.7 "소리 초기화 실패 → 부팅 화면에 오류·재시작 가드"와 다르다 | `phone_main.cpp` 에서 `boot_guard(audioout::start(...), "audio")` |
| PL3a-D11 | **시리얼 줄**: `AUDIO ready …`/`AUDIO FAIL …`(부팅), `TONE <이름> on\|off`(출력 태스크가 소리를 만든 뒤에), `STAT` 끝에 `audio= tone= aud_us=<평균>/<최대> aud_err= stk_audio=` | 소리 상태를 기기 로그로 본다. 기존 줄은 그대로라 도구·앞 계획의 정규식이 그대로 맞는다(시험으로 고정) | `TONE … off` 는 만든 소리가 0 이 된 때다 — 스피커는 최대 0.1초 늦게 그친다 | 줄을 지운다 |
| PL3a-D12 | **할 일 → 소리**: `A_TONE_END`·`A_TONE_BELL`·`A_TONE_RINGBACK` 은 그 소리를, `A_TONE_STOP` 은 멈춤을. 함께 오면 시작이 멈춤보다 앞선다(종료음 > 벨 > 연결음) | 상태 기계는 지금 소리 비트를 둘 이상 함께 내지 않는다(모든 상태 × 사건을 시험으로 고정) — 만일을 위한 순서 | 없음 | `tones.h` 의 `from_acts` |
| PL3a-D13 | **소리 명령은 신호 보내기보다 먼저** 낸다(`app.cpp` `apply()` 맨 앞) | 보내기는 Wi-Fi 가 막히면 최대 2초 멈춘다(RL-D6) — 받기·벨 끄기를 누르면 벨은 그와 무관하게 바로 멈춰야 한다 | `TONE` 줄이 그 원인이 된 `FSM` 줄보다 먼저 찍힐 수 있다 | 명령을 `FSM` 줄 `printf` 뒤로 옮긴다 |
| PL3a-D14 | **I2C 잠금은 `phone_main` 이 한 번 만든다**(정적 — 만들기가 실패하지 않는다). `app`(터치)과 `audio_out`(코덱·앰프)이 받아 쓴다 | 터치·코덱(0x10)·앰프 확장칩(0x43)이 같은 `M5.In_I2C` 버스다(Phase 0 시험 펌웨어와 M5GFX 소스로 확인). 전에는 `app::start` 안에서 만들어 화면이 실패하면 잠금이 없었다(Phase 2 최종 검수 권고 5) | `app::start` 의 인자가 하나 늘고 `app::i2c_lock()` 이 없어진다(부르는 곳이 없었다) | `app::i2c_lock()` 을 되살린다 |
| PL3a-D15 | **빌드는 Task 4 에서 한 번만**, 그 앞에 지금 `build/` 를 `out/fw_prev_066cf57/build/` 로 보관한다. 과제 1~3 의 커밋 넷은 사본에서 빌드해 확인했다(Task 2·정리·연결 뒤 세 번 — Task 1 은 아직 아무도 포함하지 않는 헤더뿐). `app.h`·`app.cpp`·`phone_main.cpp` 는 **한 커밋**이다 | 저장소의 `build/` 가 지금 기기에 올라가 있는 바이너리다 — 덮어쓰기 전에 보관해야 같은 바이트로 되돌릴 수 있다. 중간 커밋이 빌드 불가였던 지난번(DMP-D1)을 되풀이하지 않는다 | 실행 담당은 중간 커밋의 빌드를 직접 보지 않는다(사본 기록을 믿는다) | Task 2·3 끝에 `idf.py build` 를 더한다(반드시 보관 뒤에) |
| PL3a-D16 | **기기 시험은 `TONE` 줄을 `wait` 로 기다리지 않는다** — 소리 상태는 `until:stat` 의 `tone=`, 순서는 끝난 뒤 기기별 `TONE` 줄만 뽑아 본다 | `TONE` 줄(출력 태스크)과 `FSM` 줄(app 태스크)의 앞뒤가 정해져 있지 않고, 도구의 `wait` 는 찾은 줄 앞의 줄을 모두 소비한다 | 종료음(0.52초)이 "울리는 중"인 것은 `STAT` 으로 잡지 않는다(`TONE END on`/`off` 줄로만) | 소리 명령을 `FSM` 줄 뒤로 옮기고(PL3a-D13 의 탈출구) `wait` 로 본다 |

---

### Task 1: 신호음 판단 헤더 `tones.h` — 박자·소리·멈춤 약속 (시험 먼저)

**Files:**
- Create: `firmware/tab5_phone/main/tones.h`
- Test: `firmware/tab5_phone/tools/test_tones.cpp` (새 파일)

**Interfaces:**
- Consumes: `call_fsm.h` 의 할 일 비트 `callfsm::A_TONE_RINGBACK`(0x10)·`A_TONE_BELL`(0x20)·`A_TONE_END`(0x40)·`A_TONE_STOP`(0x80).
- Produces (Task 2·3 이 이 이름·서명을 그대로 쓴다) — 모두 `namespace tones`:
  - `constexpr int RATE = 16000`, `PER_MS = 16`, `FADE = 80`(5ms).
  - `enum class Tone : uint8_t { NONE, BELL, RINGBACK, END }`, `const char *tone_name(Tone)` → `"NONE"`·`"BELL"`·`"RINGBACK"`·`"END"`.
  - `struct Seg { uint16_t f1, f2, ms; }`, `struct Pattern { const Seg *seg; uint8_t n; bool loop; int16_t amp; }`, `const Pattern &pattern(Tone)`, `BELL_AMP = 8000`·`RINGBACK_AMP = 5300`·`END_AMP = 3700`.
  - 박자 계산: `struct Where { int seg; uint32_t off, len; }`, `Where locate(const Pattern &, uint64_t pos)`(끝났거나 빈 패턴이면 `seg = -1`), `seg_len`, `cycle_len`.
  - `class Player { void command(Tone); void render(int16_t *out, int n); Tone current() const; bool idle() const; uint32_t starts() const; uint64_t pos() const; }`.
  - `struct Block { bool sound; Tone off, on; }`, `Block next_block(Player &, bool has_cmd, Tone cmd, int16_t *out, int n)` — 공급자가 16ms 마다 하는 일 전부(명령 받기 → 한 묶음 → 찍을 기록 줄).
  - `void transitions(Tone before, uint32_t sb, Tone after, uint32_t sa, Tone *off, Tone *on)`, `bool from_acts(uint16_t acts, Tone *out)`, `const SineTable &sine_table()`.

맥 단위 시험의 컴파일 명령은 모두 같은 모양이다(`-isysroot` 가 없으면 이 맥에서 링크되지 않는다). 실행 파일은 `/tmp` 에 둔다.

- [x] **Step 1: 기준선을 확인하고, 시험 패치를 적용해 실패를 확인한다**

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone/tools && SDK="$(xcrun --sdk macosx --show-sdk-path)" && for T in wifi_store call_fsm relay_proto device_id ui_layout; do clang++ -std=c++17 -Wall -Wextra -isysroot "$SDK" -I../main test_$T.cpp -o /tmp/t_p3a_$T 2> /tmp/t_p3a_$T.err; echo "$T: $(/tmp/t_p3a_$T | tail -1) (경고 $(wc -l < /tmp/t_p3a_$T.err | tr -d ' ')줄)"; done
```
Expected: `wifi_store: 16 passed, 0 failed (경고 0줄)` · `call_fsm: 105 ok, 0 failed (경고 0줄)` · `relay_proto: 61 ok, 0 failed (경고 0줄)` · `device_id: 9 ok, 0 failed (경고 0줄)` · `ui_layout: 91 ok, 0 failed (경고 0줄)`.

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3a-tones/draft && git apply "$D/patches/T1-01-tones-tests.patch" && wc -l < firmware/tab5_phone/tools/test_tones.cpp && cd firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_tones.cpp -o /tmp/t_p3a_tones 2>&1 | grep -m1 "error:"
```
Expected: `     439`, 이어서 `test_tones.cpp:5:10: fatal error: 'tones.h' file not found`

```diff
--- /dev/null
+++ b/firmware/tab5_phone/tools/test_tones.cpp
@@ -0,0 +1,439 @@
+// test_tones.cpp — tones.h 단위 시험:
+//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_tones.cpp -o /tmp/t_tones && /tmp/t_tones
+// 신호음의 박자·주파수·크기, 딸깍 방지(5ms), 멈춤 약속(수신거부 설계서 §5.3), 상태 기계의 할 일 → 소리를 고정한다
+// (계획 docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md PL3a-D1·D2·D5·D12, Review Focus 1~5).
+#include "tones.h"
+#include <cmath>
+#include <cstdint>
+#include <cstdio>
+#include <cstdlib>
+#include <string>
+#include <vector>
+using namespace tones;
+using namespace callfsm;
+static int ok = 0, bad = 0;
+#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)
+
+typedef std::vector<int16_t> Pcm;
+static const double PI = 3.14159265358979323846;
+
+static Pcm play(Player &p, int samples) { Pcm v((size_t)samples); p.render(v.data(), samples); return v; }
+static Pcm cat(Pcm a, const Pcm &b) { a.insert(a.end(), b.begin(), b.end()); return a; }
+static int peak(const Pcm &v, size_t from = 0, size_t to = SIZE_MAX)
+{
+    int m = 0;
+    for (size_t i = from; i < v.size() && i < to; ++i) m = std::max(m, std::abs((int)v[i]));
+    return m;
+}
+static bool quiet(const Pcm &v, size_t from = 0, size_t to = SIZE_MAX) { return peak(v, from, to) == 0; }
+static int max_jump(const Pcm &v)                      // 이웃 표본 차이의 최댓값(첫 표본은 0 에서 시작한 것으로 본다)
+{
+    int m = 0, prev = 0;
+    for (int16_t s : v) { m = std::max(m, std::abs((int)s - prev)); prev = s; }
+    return m;
+}
+static int jump_bound(int amp, int hz) { return (int)std::ceil(amp * 2 * PI * hz / RATE) + amp / FADE + 4; }   // 사인 기울기 + 크기 옮김 + 반올림
+static double ideal(const Seg &s, int amp, uint32_t off)
+{
+    const double t = (double)off / RATE;
+    const double w = s.f2 ? (std::sin(2 * PI * s.f1 * t) + std::sin(2 * PI * s.f2 * t)) / 2 : std::sin(2 * PI * s.f1 * t);
+    return amp * w;
+}
+static Seg seg_at_ms(Tone t, uint32_t ms) { Where w = locate(pattern(t), (uint64_t)ms * PER_MS); return w.seg < 0 ? Seg{0, 0, 0} : pattern(t).seg[w.seg]; }
+static bool sounding_at_ms(Tone t, uint32_t ms) { Where w = locate(pattern(t), (uint64_t)ms * PER_MS); return w.seg >= 0 && pattern(t).seg[w.seg].f1 != 0; }
+static Pcm fresh(Tone t, int samples) { Player p; p.command(t); return play(p, samples); }
+
+// 상태 기계 + 소리를 기기처럼 잇는다(app.cpp 의 apply + tone_out.cpp 의 공급자):
+//   사건마다 from_acts → 길이 1 우편함(마지막 명령만 남는다) / 16ms(256표본)마다 next_block 과 TICK / 찍힐 TONE 줄을 log 에 모은다.
+typedef std::vector<std::string> Log;
+static std::string last(const Log &l) { return l.empty() ? std::string() : l.back(); }
+struct Rig {
+    Fsm f; Player p; int64_t now = 1000;
+    bool has = false; Tone box = Tone::NONE;
+    Log log;
+    Rig() { f.on(Ev::LINK_UP, now); f.on(Ev::PEER, now, Why::NONE, true); }
+    void apply(const Out &o) { Tone t; if (from_acts(o.acts, &t)) { box = t; has = true; } }
+    void ev(Ev e, Why w = Why::NONE, uint8_t msg = 0) { apply(f.on(e, now, w, false, msg)); }
+    Pcm run(int ms)
+    {
+        Pcm all;
+        all.reserve((size_t)ms * PER_MS + 256);
+        for (int done = 0; done < ms; done += 16) {
+            int16_t buf[256] = {};
+            const Block b = next_block(p, has, box, buf, 256);
+            has = false;
+            if (b.off != Tone::NONE) log.push_back(std::string(tone_name(b.off)) + " off");
+            if (b.on != Tone::NONE) log.push_back(std::string(tone_name(b.on)) + " on");
+            all.insert(all.end(), buf, buf + 256);               // 낼 것이 없는 묶음은 0(기기에서는 DMA 가 0 을 낸다)
+            now += 16; apply(f.on(Ev::TICK, now));
+        }
+        return all;
+    }
+};
+
+static int tone_bits(uint16_t acts) { return !!(acts & A_TONE_RINGBACK) + !!(acts & A_TONE_BELL) + !!(acts & A_TONE_END) + !!(acts & A_TONE_STOP); }
+
+// 상태 기계를 그 상태로 데려간다(교환기에 붙고 상대도 켜진 채)
+static Fsm in_state(State s)
+{
+    Fsm f;
+    if (s == State::OFFLINE) return f;
+    f.on(Ev::LINK_UP, 1000); f.on(Ev::PEER, 1000, Why::NONE, true);
+    switch (s) {
+    case State::DIALING:   f.on(Ev::TAP_CALL, 1100); break;
+    case State::RINGBACK:  f.on(Ev::TAP_CALL, 1100); f.on(Ev::RINGING, 1200); break;
+    case State::INCOMING:  f.on(Ev::RING, 1100); break;
+    case State::ANSWERING: f.on(Ev::RING, 1100); f.on(Ev::TAP_ACCEPT, 1200); break;
+    case State::IN_CALL:   f.on(Ev::RING, 1100); f.on(Ev::TAP_ACCEPT, 1200); f.on(Ev::CONNECTED, 1300); break;
+    case State::ENDING:    f.on(Ev::RING, 1100); f.on(Ev::TAP_REJECT, 1200); break;
+    default: break;
+    }
+    return f;
+}
+
+int main()
+{
+    // ════ 박자 계산 (PL3a-D1) ════
+    CHECK("bell: 800 Hz at 0 ms, 1000 Hz at 50 ms, alternating every 50 ms",
+          seg_at_ms(Tone::BELL, 0).f1 == 800 && seg_at_ms(Tone::BELL, 49).f1 == 800 && seg_at_ms(Tone::BELL, 50).f1 == 1000
+          && seg_at_ms(Tone::BELL, 100).f1 == 800 && seg_at_ms(Tone::BELL, 950).f1 == 1000 && seg_at_ms(Tone::BELL, 999).f1 == 1000);
+    CHECK("bell: rings 1 s then 2 s quiet, then again (3 s cycle)",
+          sounding_at_ms(Tone::BELL, 999) && !sounding_at_ms(Tone::BELL, 1000) && !sounding_at_ms(Tone::BELL, 2999)
+          && sounding_at_ms(Tone::BELL, 3000) && seg_at_ms(Tone::BELL, 3000).f1 == 800 && seg_at_ms(Tone::BELL, 3050).f1 == 1000
+          && sounding_at_ms(Tone::BELL, 30999) && !sounding_at_ms(Tone::BELL, 31000));
+    CHECK("ringback: 440+480 Hz together for 1 s, then 2 s quiet, repeating",
+          seg_at_ms(Tone::RINGBACK, 0).f1 == 440 && seg_at_ms(Tone::RINGBACK, 0).f2 == 480 && sounding_at_ms(Tone::RINGBACK, 999)
+          && !sounding_at_ms(Tone::RINGBACK, 1000) && !sounding_at_ms(Tone::RINGBACK, 2999) && sounding_at_ms(Tone::RINGBACK, 3000));
+    CHECK("end: 660 Hz 160 ms, 80 ms gap, 440 Hz 280 ms, once (finished at 520 ms)",
+          seg_at_ms(Tone::END, 0).f1 == 660 && seg_at_ms(Tone::END, 159).f1 == 660 && !sounding_at_ms(Tone::END, 160)
+          && !sounding_at_ms(Tone::END, 239) && seg_at_ms(Tone::END, 240).f1 == 440 && seg_at_ms(Tone::END, 519).f1 == 440
+          && locate(pattern(Tone::END), 520 * PER_MS).seg == -1 && locate(pattern(Tone::END), 100000 * PER_MS).seg == -1);
+    CHECK("cycle lengths: bell 3 s, ringback 3 s, end 0.52 s; none is empty",
+          cycle_len(pattern(Tone::BELL)) == 3000u * PER_MS && cycle_len(pattern(Tone::RINGBACK)) == 3000u * PER_MS
+          && cycle_len(pattern(Tone::END)) == 520u * PER_MS && pattern(Tone::NONE).n == 0 && locate(pattern(Tone::NONE), 0).seg == -1);
+    CHECK("16 kHz, and the fade is 5 ms = 80 samples (PL3a-D5)", RATE == 16000 && PER_MS == 16 && FADE == 80);
+    CHECK("every sounding segment is longer than two fades (fade in + fade out fit)", [] {
+        for (Tone t : {Tone::BELL, Tone::RINGBACK, Tone::END})
+            for (int i = 0; i < pattern(t).n; ++i) if (pattern(t).seg[i].f1 && seg_len(pattern(t).seg[i]) <= 2u * FADE) return false;
+        return true; }());
+    CHECK("levels (PL3a-D2): bell 8000, ringback 5300, end 3700",
+          pattern(Tone::BELL).amp == 8000 && pattern(Tone::RINGBACK).amp == 5300 && pattern(Tone::END).amp == 3700);
+    CHECK("names for the serial log", std::string(tone_name(Tone::NONE)) == "NONE" && std::string(tone_name(Tone::BELL)) == "BELL"
+          && std::string(tone_name(Tone::RINGBACK)) == "RINGBACK" && std::string(tone_name(Tone::END)) == "END");
+
+    // ════ 소리 만들기: 주파수·크기·5ms 페이드 (PL3a-D5) ════
+    for (Tone t : {Tone::BELL, Tone::RINGBACK, Tone::END}) {
+        const Pattern &p = pattern(t);
+        const Pcm v = fresh(t, (int)cycle_len(p));
+        bool formula = true, envelope = true, edges = true, gaps = true;
+        uint32_t start = 0;
+        for (int i = 0; i < p.n; ++i) {
+            const Seg &s = p.seg[i]; const uint32_t len = seg_len(s);
+            for (uint32_t off = 0; off < len; ++off) {
+                const int x = v[start + off];
+                if (!s.f1) { if (x) gaps = false; continue; }
+                const uint32_t g = off < (uint32_t)FADE ? off + 1 : off + FADE >= len ? len - 1 - off : (uint32_t)FADE;   // 크기 단계
+                if (std::abs(x) > (int)(p.amp * g / FADE) + 1) envelope = false;
+                if (g == (uint32_t)FADE && std::fabs(x - ideal(s, p.amp, off)) > 4) formula = false;
+            }
+            if (s.f1 && (v[start] == 0 ? false : std::abs((int)v[start]) > p.amp / FADE + 1)) edges = false;   // 칸의 첫 표본은 아주 작다
+            if (s.f1 && v[start + len - 1] != 0) edges = false;                                               // 칸의 마지막 표본은 0
+            start += len;
+        }
+        const std::string n = tone_name(t);
+        CHECK((n + ": samples follow the sine formula (frequency and level), within 4 of 32767").c_str(), formula);
+        CHECK((n + ": 5 ms fade in and out at every segment edge").c_str(), envelope && edges);
+        CHECK((n + ": quiet segments are exactly zero").c_str(), gaps);
+        CHECK((n + ": peak = its level (no louder, at most 1% under)").c_str(), peak(v) <= p.amp && peak(v) >= p.amp * 99 / 100);
+    }
+    CHECK("no clicks: neighbouring samples never jump more than a sine at the highest frequency allows",
+          max_jump(fresh(Tone::BELL, 6000 * PER_MS)) <= jump_bound(BELL_AMP, 1000)
+          && max_jump(fresh(Tone::RINGBACK, 6000 * PER_MS)) <= jump_bound(RINGBACK_AMP, 480)
+          && max_jump(fresh(Tone::END, 600 * PER_MS)) <= jump_bound(END_AMP, 660));
+
+    // ════ 멈춤 약속 (수신거부 설계서 §5.3) ════
+    {
+        Player p; p.command(Tone::BELL);
+        const Pcm a = play(p, 525 * PER_MS);                   // 525ms = 1000Hz 칸 한가운데(크기 최대)
+        CHECK("bell mid-tone is at full level before the stop", std::abs((int)a.back()) > 0 && p.current() == Tone::BELL);
+        p.command(Tone::NONE);
+        const Pcm b = play(p, FADE);
+        CHECK("stop mid-tone: silent within 5 ms and the bell is off", b.back() == 0 && p.current() == Tone::NONE);
+        CHECK("stop mid-tone: no click (fades out)", max_jump(cat(a, b)) <= jump_bound(BELL_AMP, 1000));
+        CHECK("after a stop it never rings again by itself (60 s)", quiet(play(p, 60000 * PER_MS)) && p.current() == Tone::NONE);
+        p.command(Tone::NONE); p.command(Tone::NONE);
+        CHECK("a stop after the stop is harmless", quiet(play(p, 1000 * PER_MS)) && p.current() == Tone::NONE && p.starts() == 1);
+    }
+    {
+        Player p;
+        p.command(Tone::NONE);
+        CHECK("stop with nothing playing: nothing happens", quiet(play(p, 100 * PER_MS)) && p.idle() && p.starts() == 0);
+        p.command(Tone::BELL); play(p, 1500 * PER_MS);         // 벨의 2초 쉼 한가운데
+        p.command(Tone::NONE);
+        CHECK("stop during the bell's quiet gap: off at once", p.current() == Tone::NONE && quiet(play(p, 3000 * PER_MS)));
+    }
+    {
+        Player p; p.command(Tone::END);
+        const Pcm a = play(p, 520 * PER_MS);
+        CHECK("end tone is on while it plays", p.current() == Tone::END && peak(a) > 0);
+        const Pcm b = play(p, 2 * PER_MS);
+        CHECK("end tone plays once and goes off by itself at 0.52 s", p.current() == Tone::NONE && quiet(b));
+        CHECK("end tone does not come back (10 s)", quiet(play(p, 10000 * PER_MS)) && p.starts() == 1);
+    }
+
+    // ════ 바꿈·겹침 (Review Focus 3·5) ════
+    {
+        Player p; p.command(Tone::RINGBACK);
+        const Pcm a = play(p, 500 * PER_MS);                   // 연결음 한가운데
+        p.command(Tone::END);
+        const Pcm b = play(p, FADE);
+        CHECK("end replaces the ringback after a 5 ms fade", p.current() == Tone::END && p.pos() == 0 && b.back() == 0);
+        const Pcm c = play(p, 600 * PER_MS);
+        CHECK("after the switch the end tone is exactly the end tone alone", c == fresh(Tone::END, 600 * PER_MS));
+        CHECK("ringback -> end: no click", max_jump(cat(cat(a, b), c)) <= jump_bound(RINGBACK_AMP, 660));
+    }
+    {
+        Player p; p.command(Tone::END);
+        const Pcm a = play(p, 100 * PER_MS);                   // 종료음이 울리는 중에 다음 전화
+        p.command(Tone::BELL);
+        const Pcm b = play(p, FADE);
+        const Pcm c = play(p, 3000 * PER_MS);
+        CHECK("end tone then an immediate new call: the bell takes over within 5 ms", p.current() == Tone::BELL && p.starts() == 2);
+        CHECK("... and it is exactly the bell alone (not swallowed, not mixed)", c == fresh(Tone::BELL, 3000 * PER_MS));
+        CHECK("end -> bell: no click and never louder than the bell", max_jump(cat(cat(a, b), c)) <= jump_bound(BELL_AMP, 1000) && peak(cat(cat(a, b), c)) <= BELL_AMP);
+    }
+    {
+        Player p; p.command(Tone::BELL); play(p, 525 * PER_MS);
+        p.command(Tone::NONE); p.command(Tone::BELL);          // 한 바퀴 안에 멈춤 → 벨(ended → ring)
+        play(p, FADE);
+        CHECK("stop then bell in one batch: the bell (latest command wins), restarted", p.current() == Tone::BELL && p.starts() == 2 && p.pos() == 0);
+        p.command(Tone::BELL); p.command(Tone::NONE);
+        play(p, FADE + 1);
+        CHECK("bell then stop in one batch: silent", p.current() == Tone::NONE && quiet(play(p, 3000 * PER_MS)));
+    }
+    {
+        Player p; Pcm all;
+        for (int i = 0; i < 6; ++i) {                          // 아주 짧은 전화가 연달아(울림 10ms → 멈춤 → …)
+            p.command(Tone::BELL); all = cat(all, play(p, 10 * PER_MS));
+            p.command(Tone::NONE); all = cat(all, play(p, 3 * PER_MS));
+        }
+        all = cat(all, play(p, 100 * PER_MS));
+        CHECK("very short calls in a row: ends silent, never louder than one bell, no clicks",
+              p.idle() && quiet(all, all.size() - 50 * PER_MS) && peak(all) <= BELL_AMP && max_jump(all) <= jump_bound(BELL_AMP, 1000));
+    }
+
+    // ════ 기록 줄: TONE <이름> on|off (PL3a-D11) ════
+    {
+        Tone off, on;
+        transitions(Tone::NONE, 0, Tone::BELL, 1, &off, &on);
+        CHECK("log: nothing -> bell = 'BELL on' only", off == Tone::NONE && on == Tone::BELL);
+        transitions(Tone::BELL, 1, Tone::NONE, 1, &off, &on);
+        CHECK("log: bell -> silent = 'BELL off' only", off == Tone::BELL && on == Tone::NONE);
+        transitions(Tone::RINGBACK, 1, Tone::END, 2, &off, &on);
+        CHECK("log: ringback -> end = 'RINGBACK off' then 'END on'", off == Tone::RINGBACK && on == Tone::END);
+        transitions(Tone::BELL, 1, Tone::BELL, 2, &off, &on);
+        CHECK("log: the bell restarted = 'BELL off' then 'BELL on'", off == Tone::BELL && on == Tone::BELL);
+        transitions(Tone::BELL, 3, Tone::BELL, 3, &off, &on);
+        CHECK("log: still the same bell = nothing", off == Tone::NONE && on == Tone::NONE);
+        transitions(Tone::NONE, 3, Tone::NONE, 3, &off, &on);
+        CHECK("log: still silent = nothing", off == Tone::NONE && on == Tone::NONE);
+    }
+
+    // ════ 할 일 → 소리 (PL3a-D12) ════
+    {
+        Tone t = Tone::BELL;
+        CHECK("from_acts: bell / ringback / end / stop", from_acts(A_TONE_BELL, &t) && t == Tone::BELL && from_acts(A_TONE_RINGBACK, &t) && t == Tone::RINGBACK
+              && from_acts(A_TONE_END, &t) && t == Tone::END && from_acts(A_TONE_STOP, &t) && t == Tone::NONE);
+        t = Tone::END;
+        CHECK("from_acts: no tone bit = leave the sound alone", !from_acts(0, &t) && !from_acts(A_SEND_CALL | A_SEND_ACCEPT | A_SEND_REJECT | A_SEND_HANGUP | A_VOICE_START | A_VOICE_STOP | A_RECONNECT, &t) && t == Tone::END);
+        CHECK("from_acts: real combinations (accept, connect, hang up in call)", from_acts(A_SEND_ACCEPT | A_TONE_STOP, &t) && t == Tone::NONE
+              && from_acts(A_TONE_STOP | A_VOICE_START, &t) && t == Tone::NONE && from_acts(A_SEND_HANGUP | A_VOICE_STOP | A_TONE_END, &t) && t == Tone::END);
+        CHECK("from_acts: a start bit wins over stop (end > bell > ringback)", from_acts(A_TONE_STOP | A_TONE_BELL, &t) && t == Tone::BELL
+              && from_acts(A_TONE_STOP | A_TONE_END | A_TONE_BELL, &t) && t == Tone::END && from_acts(A_TONE_RINGBACK | A_TONE_BELL, &t) && t == Tone::BELL);
+    }
+    {
+        const State states[] = { State::OFFLINE, State::IDLE, State::DIALING, State::RINGBACK, State::INCOMING, State::ANSWERING, State::IN_CALL, State::ENDING };
+        const Ev evs[] = { Ev::LINK_UP, Ev::LINK_DOWN, Ev::PEER, Ev::RINGING, Ev::RING, Ev::CONNECTED, Ev::ENDED, Ev::TAP_CALL, Ev::TAP_CANCEL,
+                           Ev::TAP_ACCEPT, Ev::TAP_REJECT, Ev::TAP_HANGUP, Ev::TAP_MUTE, Ev::TICK };
+        const Why whys[] = { Why::NONE, Why::HANGUP, Why::REJECT, Why::TIMEOUT, Why::OFFLINE, Why::LOST };
+        int cases = 0, two_bits = 0, bell_wrong = 0, bell_missing = 0, ring_in_incoming = 0;
+        for (State s : states) for (bool muted : {false, true}) for (Ev e : evs) for (Why w : whys) for (bool flag : {false, true})
+            for (uint8_t msg : {(uint8_t)0, (uint8_t)2}) for (int64_t at : {(int64_t)1400, (int64_t)100000}) {
+                if (muted && s != State::INCOMING) continue;
+                Fsm f = in_state(s);
+                if (muted) f.on(Ev::TAP_MUTE, 1300);
+                const State before = f.state;
+                const Out o = f.on(e, at, w, flag, msg);
+                ++cases;
+                if (tone_bits(o.acts) > 1) ++two_bits;
+                if ((o.acts & A_TONE_BELL) && !(before == State::IDLE && f.state == State::INCOMING)) ++bell_wrong;
+                if (before != State::INCOMING && f.state == State::INCOMING && !(o.acts & A_TONE_BELL)) ++bell_missing;
+                if (before == State::INCOMING && (o.acts & (A_TONE_BELL | A_TONE_RINGBACK))) ++ring_in_incoming;
+            }
+        printf("       (state x event x why x flag x msg x time: %d cases)\n", cases);
+        CHECK("state machine: never two tone bits at once (every state x event)", cases > 5000 && two_bits == 0);
+        CHECK("state machine: bell starts only on IDLE -> INCOMING, and always then", bell_wrong == 0 && bell_missing == 0);
+        CHECK("state machine: nothing while ringing (muted or not) restarts a bell or ringback", ring_in_incoming == 0);
+    }
+
+    // ════ 출력 한 묶음(next_block): 명령 → 소리 → 기록 줄 ════
+    {
+        Player p; int16_t buf[256]; for (int16_t &x : buf) x = 77;
+        Block b = next_block(p, false, Tone::NONE, buf, 256);
+        CHECK("block: nothing to play = no sound, buffer untouched, no log line", !b.sound && buf[0] == 77 && buf[255] == 77 && b.off == Tone::NONE && b.on == Tone::NONE);
+        b = next_block(p, true, Tone::NONE, buf, 256);
+        CHECK("block: a stop while silent = still nothing", !b.sound && buf[0] == 77 && b.off == Tone::NONE && b.on == Tone::NONE);
+        b = next_block(p, true, Tone::BELL, buf, 256);
+        CHECK("block: bell command = sound in this very block, 'BELL on'", b.sound && buf[0] == 0 && peak(Pcm(buf, buf + 256)) > 0 && b.off == Tone::NONE && b.on == Tone::BELL);
+        b = next_block(p, false, Tone::NONE, buf, 256);
+        CHECK("block: bell goes on, no new line", b.sound && b.off == Tone::NONE && b.on == Tone::NONE);
+        b = next_block(p, true, Tone::NONE, buf, 256);
+        CHECK("block: stop = the fade-out is still sound, 'BELL off', then silence", b.sound && buf[255] == 0 && b.off == Tone::BELL && b.on == Tone::NONE
+              && !next_block(p, false, Tone::NONE, buf, 256).sound);
+    }
+
+    // ════ 상태 기계 + 소리 (Review Focus 1~5) ════
+    {
+        Rig r; r.ev(Ev::RING);
+        const Pcm a = r.run(1000);
+        CHECK("call comes in: the bell rings", r.p.current() == Tone::BELL && peak(a) >= BELL_AMP * 99 / 100);
+        r.ev(Ev::TAP_MUTE);
+        const Pcm b = r.run(16);
+        CHECK("mute: silent within 5 ms, still ringing state", quiet(b, FADE) && r.p.idle() && r.f.state == State::INCOMING);
+        r.ev(Ev::RING);                                          // 엇갈려 온 ring — 벨을 다시 켜지 않는다
+        r.ev(Ev::TAP_MUTE);                                      // 끈 벨을 또 누름
+        const Pcm c = r.run(27000);
+        CHECK("muted: stays silent for the rest of that call (stray ring, second mute, 27 s)", quiet(c) && r.p.idle() && r.f.state == State::INCOMING);
+        r.ev(Ev::ENDED, Why::TIMEOUT);                           // 교환기 30초 → 부재중(멈춤 한 번 더)
+        const Pcm d = r.run(1000);
+        CHECK("muted call becomes missed: silent, no end tone", quiet(d) && r.p.idle() && r.f.state == State::IDLE && r.f.missed == 1);
+        CHECK("log of a muted call: 'BELL on', 'BELL off' — nothing more", r.log == (Log{"BELL on", "BELL off"}));
+        r.ev(Ev::RING);
+        const Pcm e = r.run(500);
+        CHECK("the next call rings again", peak(e) > 0 && r.p.current() == Tone::BELL && last(r.log) == "BELL on");
+    }
+    {
+        struct End { const char *name; Ev e; Why w; uint8_t msg; };
+        const End ends[] = { {"accept", Ev::TAP_ACCEPT, Why::NONE, 0}, {"reject", Ev::TAP_REJECT, Why::NONE, 0}, {"message reject", Ev::TAP_REJECT, Why::NONE, 2},
+                             {"caller cancels", Ev::ENDED, Why::HANGUP, 0}, {"relay timeout", Ev::ENDED, Why::TIMEOUT, 0}, {"caller vanished", Ev::ENDED, Why::OFFLINE, 0},
+                             {"relay link drops", Ev::LINK_DOWN, Why::NONE, 0}, {"connected without accept", Ev::CONNECTED, Why::NONE, 0} };
+        bool all = true;
+        for (const End &x : ends) {
+            Rig r; r.ev(Ev::RING); r.run(528);                   // 528ms = 1000Hz 칸 한가운데(크기 최대)
+            r.ev(x.e, x.w, x.msg);
+            const Pcm a = r.run(32);
+            const Pcm b = r.run(5000);
+            if (!(quiet(a, FADE) && quiet(b) && r.p.idle() && r.log == (Log{"BELL on", "BELL off"}))) { all = false; printf("       bell left sounding after: %s\n", x.name); }
+        }
+        CHECK("bell ends every way (accept, reject, message reject, cancel, timeout, offline, link down, odd connected): silent in 5 ms, stays silent", all);
+        Rig g; g.ev(Ev::RING);
+        const Pcm a = g.run(41000);                              // 교환기가 끝내지 않을 때: 40초 가드
+        CHECK("bell stuck 40 s (relay silent): the guard stops it", g.f.state == State::OFFLINE && g.p.idle() && quiet(a, a.size() - 900 * PER_MS)
+              && g.log == (Log{"BELL on", "BELL off"}));
+    }
+    {
+        Rig r; r.ev(Ev::RING); r.run(528);
+        r.ev(Ev::LINK_DOWN);
+        const Pcm a = r.run(1000);
+        CHECK("ringing when the relay link drops: silent", quiet(a, FADE) && r.p.idle());
+        r.ev(Ev::LINK_UP); r.ev(Ev::PEER); r.ev(Ev::RING);
+        const Pcm b = r.run(500);
+        CHECK("... and after reconnecting the next call rings", peak(b) > 0 && r.p.current() == Tone::BELL);
+    }
+    {
+        Rig r; r.ev(Ev::TAP_CALL); r.ev(Ev::RINGING);
+        const Pcm a = r.run(1000);
+        CHECK("caller: ringback while the other side rings", peak(a) > 0 && r.p.current() == Tone::RINGBACK);
+        r.ev(Ev::ENDED, Why::REJECT);
+        const Pcm b = r.run(608);
+        const Pcm c = r.run(3000);
+        CHECK("caller rejected: ringback stops, end tone once, then silent", r.p.idle() && peak(b) > 0 && quiet(c)
+              && r.log == (Log{"RINGBACK on", "RINGBACK off", "END on", "END off"}));
+        r.ev(Ev::TAP_CALL); r.ev(Ev::RINGING); r.run(500);
+        r.ev(Ev::CONNECTED);
+        const Pcm d = r.run(1000);
+        CHECK("caller connected: ringback stops, no end tone", quiet(d, FADE) && r.p.idle() && r.f.state == State::IN_CALL && last(r.log) == "RINGBACK off");
+        r.ev(Ev::TAP_HANGUP);
+        const Pcm e = r.run(304);
+        CHECK("hang up in a call: end tone", r.p.current() == Tone::END && peak(e) > 0 && last(r.log) == "END on");
+        r.ev(Ev::ENDED, Why::HANGUP);                            // 내 hangup 의 ended — 종료음은 끊기지 않고 끝까지
+        const Pcm g = r.run(96);
+        CHECK("my hang-up's ended does not cut the end tone", r.p.current() == Tone::END && r.f.state == State::IDLE && peak(g) > 0);
+        r.run(304);
+        CHECK("... which then ends by itself", r.p.idle() && last(r.log) == "END off");
+    }
+    {
+        Rig r; r.ev(Ev::RING); r.run(528);
+        r.ev(Ev::ENDED, Why::TIMEOUT); r.ev(Ev::RING);           // 한 바퀴 안에 ended → ring: 우편함에는 벨만 남는다
+        const Pcm a = r.run(16);
+        CHECK("ended then ring in one batch: after a 5 ms fade the new call's bell rings from its start",
+              r.p.current() == Tone::BELL && r.p.starts() == 2 && Pcm(a.begin() + FADE, a.end()) == fresh(Tone::BELL, (int)a.size() - FADE)
+              && r.log == (Log{"BELL on", "BELL off", "BELL on"}));
+    }
+    {
+        Rig r; r.ev(Ev::RING); r.ev(Ev::ENDED, Why::HANGUP);     // 출력 태스크가 꺼내기도 전에 벨 → 멈춤(PL3a-D6 의 비용)
+        const Pcm a = r.run(1000);
+        CHECK("bell and stop before the output task looked: not a sample, not a log line, missed call counted", quiet(a) && r.log.empty() && r.f.missed == 1);
+    }
+    {
+        Rig r; bool each_rang = true;
+        for (int i = 0; i < 5; ++i) {                            // 아주 짧은 전화가 연달아: 울림 48ms → 상대 취소 → 16ms 뒤 다시
+            r.ev(Ev::RING);
+            if (peak(r.run(48)) == 0) each_rang = false;
+            r.ev(Ev::ENDED, Why::HANGUP);
+            r.run(16);
+        }
+        const Pcm tail = r.run(3000);
+        CHECK("short calls in a row: each one rings, none is left sounding", each_rang && r.p.idle() && quiet(tail) && r.f.missed == 5 && r.p.starts() == 5
+              && r.log == (Log{"BELL on", "BELL off", "BELL on", "BELL off", "BELL on", "BELL off", "BELL on", "BELL off", "BELL on", "BELL off"}));
+    }
+    {
+        Rig r; r.ev(Ev::RING); r.run(96); r.ev(Ev::TAP_ACCEPT); r.ev(Ev::CONNECTED); r.run(2000);
+        r.ev(Ev::ENDED, Why::HANGUP);                            // 상대가 끊음 → 종료음
+        r.run(96);
+        r.ev(Ev::RING);                                          // 종료음이 울리는 중에 다음 전화
+        r.run(16);
+        const Pcm a = r.run(500);
+        CHECK("call ends and the next call comes during the end tone: bell, not end", r.p.current() == Tone::BELL && peak(a) > 0
+              && r.log == (Log{"BELL on", "BELL off", "END on", "END off", "BELL on"}));
+    }
+
+    // ════ 기기 시험(계획 Task 5)의 기대 줄: 시나리오별로 한 기기가 찍는 TONE 줄 ════
+    {
+        Rig callee; callee.ev(Ev::RING); callee.run(1008); callee.ev(Ev::TAP_ACCEPT); callee.run(32); callee.ev(Ev::CONNECTED); callee.run(2000);
+        callee.ev(Ev::ENDED, Why::HANGUP); callee.run(1008);
+        Rig caller; caller.ev(Ev::TAP_CALL); caller.run(32); caller.ev(Ev::RINGING); caller.run(1008); caller.ev(Ev::CONNECTED); caller.run(2000);
+        caller.ev(Ev::TAP_HANGUP); caller.run(48); caller.ev(Ev::ENDED, Why::HANGUP); caller.run(1008);
+        CHECK("scenario call+accept+hangup: callee 'BELL on/off, END on/off', caller 'RINGBACK on/off, END on/off'",
+              callee.log == (Log{"BELL on", "BELL off", "END on", "END off"}) && caller.log == (Log{"RINGBACK on", "RINGBACK off", "END on", "END off"})
+              && callee.p.idle() && caller.p.idle());
+    }
+    {
+        Rig callee; callee.ev(Ev::RING); callee.run(1008); callee.ev(Ev::TAP_REJECT, Why::NONE, 2); callee.run(48); callee.ev(Ev::ENDED, Why::REJECT, 2); callee.run(1008);
+        Rig caller; caller.ev(Ev::TAP_CALL); caller.ev(Ev::RINGING); caller.run(1008); caller.ev(Ev::ENDED, Why::REJECT, 2); caller.run(1008);
+        CHECK("scenario reject (plain or with a message): callee 'BELL on/off' only, caller 'RINGBACK on/off, END on/off'",
+              callee.log == (Log{"BELL on", "BELL off"}) && caller.log == (Log{"RINGBACK on", "RINGBACK off", "END on", "END off"}));
+    }
+    {
+        Rig caller; caller.ev(Ev::TAP_CALL); caller.ev(Ev::RINGING); caller.run(2000); caller.ev(Ev::TAP_CANCEL); caller.run(48); caller.ev(Ev::ENDED, Why::HANGUP); caller.run(1008);
+        Rig callee; callee.ev(Ev::RING); callee.run(2000); callee.ev(Ev::ENDED, Why::HANGUP); callee.run(1008);
+        CHECK("scenario cancel: caller 'RINGBACK on/off' (no end tone), callee 'BELL on/off'",
+              caller.log == (Log{"RINGBACK on", "RINGBACK off"}) && callee.log == (Log{"BELL on", "BELL off"}) && callee.f.missed == 1);
+    }
+    {
+        Rig caller; caller.ev(Ev::TAP_CALL); caller.ev(Ev::RINGING); caller.run(30000); caller.ev(Ev::ENDED, Why::TIMEOUT); caller.run(1008);
+        Rig callee; callee.ev(Ev::RING); callee.run(30000); callee.ev(Ev::ENDED, Why::TIMEOUT); callee.run(1008);
+        CHECK("scenario no answer (30 s): caller 'RINGBACK on/off, END on/off', callee 'BELL on/off'",
+              caller.log == (Log{"RINGBACK on", "RINGBACK off", "END on", "END off"}) && callee.log == (Log{"BELL on", "BELL off"}));
+    }
+    {
+        Rig callee; callee.ev(Ev::RING); callee.run(1008); callee.ev(Ev::TAP_MUTE); callee.run(5008); callee.ev(Ev::TAP_MUTE); callee.run(1008);
+        callee.ev(Ev::TAP_ACCEPT); callee.run(32); callee.ev(Ev::CONNECTED); callee.run(2000); callee.ev(Ev::TAP_HANGUP); callee.run(48);
+        callee.ev(Ev::ENDED, Why::HANGUP); callee.run(1008);
+        CHECK("scenario mute, mute again, accept, hang up: 'BELL on, BELL off, END on, END off' (accept adds no line)",
+              callee.log == (Log{"BELL on", "BELL off", "END on", "END off"}));
+    }
+
+    printf("\n%d ok, %d failed\n", ok, bad);
+    return bad ? 1 : 0;
+}
```

- [x] **Step 2: 구현 패치를 적용하고 통과를 확인한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3a-tones/draft && git apply "$D/patches/T1-02-tones.patch" && for f in main/tones.h tools/test_tones.cpp; do cmp "$D/files/firmware/tab5_phone/$f" "firmware/tab5_phone/$f" && echo "같음: $f"; done && cd firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_tones.cpp -o /tmp/t_p3a_tones 2> /tmp/t_p3a_tones.err; echo "경고 $(wc -l < /tmp/t_p3a_tones.err | tr -d ' ')줄"; /tmp/t_p3a_tones | grep -c "^  FAIL"; /tmp/t_p3a_tones | tail -1
```
Expected: `같음:` 2줄, `경고 0줄`, `0`, `84 ok, 0 failed`

```diff
--- /dev/null
+++ b/firmware/tab5_phone/main/tones.h
@@ -0,0 +1,192 @@
+// tones.h — 신호음(벨·연결음·종료음)의 박자와 소리 만들기. 하드웨어·시계와 무관한 순수 로직(맥에서 단위 시험: tools/test_tones.cpp).
+//
+// 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md 의 결정 PL3a-D1(모양)·D2(크기)·D5(딸깍 방지)·D11(기록 줄)·D12(할 일 → 소리).
+//   - 상태 기계(call_fsm.h)가 낸 할 일 비트(A_TONE_*)를 from_acts() 가 "지금 내야 할 소리" 하나로 바꾼다. 소리 쪽은 그것만 안다.
+//   - Player 는 명령을 받아 16kHz 모노 PCM 을 만든다. 기기에서는 출력 태스크(tone_out.cpp 의 공급자)만 부른다 — 잠금이 없다.
+//     공급자가 하는 일(명령 받기 → 한 묶음 만들기 → 찍을 기록 줄 고르기)도 여기(next_block)에 있다 — 기기 전용 코드는 우편함과 printf 뿐이다.
+//   - 약속(수신거부 설계서 docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md §5.3): 멈춘(NONE) 뒤에는 새 명령이 올 때까지
+//     스스로 다시 울리지 않는다. 이미 멈춘 뒤의 멈춤은 아무 일도 하지 않는다.
+#pragma once
+#include <cmath>
+#include <cstdint>
+
+#include "call_fsm.h"
+
+namespace tones {
+
+constexpr int RATE = 16000;                 // 표본율 — 통화 음성과 같다(PH-D5)
+constexpr int PER_MS = RATE / 1000;         // 1ms = 16표본
+constexpr int FADE = 5 * PER_MS;            // PL3a-D5: 울림 칸의 시작·끝과 멈춤·바꿈 때 5ms(80표본)에 걸쳐 크기를 0↔최대로 옮긴다(딸깍 소리 방지)
+
+enum class Tone : uint8_t { NONE, BELL, RINGBACK, END };
+
+inline const char *tone_name(Tone t)
+{
+    switch (t) {
+    case Tone::NONE: return "NONE"; case Tone::BELL: return "BELL"; case Tone::RINGBACK: return "RINGBACK"; case Tone::END: return "END";
+    }
+    return "?";
+}
+
+struct Seg { uint16_t f1, f2, ms; };        // 한 칸: 주파수 둘(f2 = 0 이면 한 음), f1 = 0 이면 쉼. 길이(ms)
+struct Pattern { const Seg *seg; uint8_t n; bool loop; int16_t amp; };   // amp = 최고 크기(16비트 표본). loop = 끝나면 처음부터
+
+// PL3a-D1 신호음 모양. 바꿀 때는 이 표만 고친다(맥 시험의 박자 시험도 함께).
+//   벨: 800·1000Hz 를 50ms 씩 번갈아 1초(전자식 전화벨의 떨림) → 2초 쉼, 반복. 3초 주기라 교환기의 30초 벨 동안 10번 울린다
+//   연결음: 440+480Hz 를 함께 1초 → 2초 쉼, 반복(일반 전화 연결음의 모양)
+//   종료음: 660Hz 160ms → 쉼 80ms → 440Hz 280ms, 한 번(0.52초). 내려가는 두 음 — "통화 중" 신호(뚜-뚜-)와 헷갈리지 않게
+inline constexpr Seg BELL_SEGS[] = {
+    {800, 0, 50}, {1000, 0, 50}, {800, 0, 50}, {1000, 0, 50}, {800, 0, 50}, {1000, 0, 50}, {800, 0, 50}, {1000, 0, 50},
+    {800, 0, 50}, {1000, 0, 50}, {800, 0, 50}, {1000, 0, 50}, {800, 0, 50}, {1000, 0, 50}, {800, 0, 50}, {1000, 0, 50},
+    {800, 0, 50}, {1000, 0, 50}, {800, 0, 50}, {1000, 0, 50}, {0, 0, 2000},
+};
+inline constexpr Seg RINGBACK_SEGS[] = { {440, 480, 1000}, {0, 0, 2000} };
+inline constexpr Seg END_SEGS[] = { {660, 0, 160}, {0, 0, 80}, {440, 0, 280} };
+
+// PL3a-D2 크기 — 코덱 음량 100(0dB, audio_out.cpp)에서의 최고 크기.
+//   벨 8000: Phase 0 에서 1kHz·진폭 8000 을 음량 100 으로 낸 "삐"를 사용자가 "적당"이라 했다(tab5_phone_probe/out/task4.log 61~66) — 그 크기 그대로.
+//   연결음 5300·종료음 3700: Phase 0 시험 음성(말소리 구간 RMS −16.9dBFS)을 음량 90(−5dB)으로 낸 크기(≈ −21.9dB)와 RMS 를 맞췄다 —
+//   귀 가까이에서 듣는 소리라서. 두 음을 함께 내는 연결음의 RMS = 최고 크기/2, 한 음인 종료음은 최고 크기/√2.
+inline constexpr int16_t BELL_AMP = 8000, RINGBACK_AMP = 5300, END_AMP = 3700;
+
+inline const Pattern &pattern(Tone t)
+{
+    static constexpr Pattern NONE_P{nullptr, 0, false, 0};
+    static constexpr Pattern BELL_P{BELL_SEGS, sizeof BELL_SEGS / sizeof BELL_SEGS[0], true, BELL_AMP};
+    static constexpr Pattern RINGBACK_P{RINGBACK_SEGS, sizeof RINGBACK_SEGS / sizeof RINGBACK_SEGS[0], true, RINGBACK_AMP};
+    static constexpr Pattern END_P{END_SEGS, sizeof END_SEGS / sizeof END_SEGS[0], false, END_AMP};
+    switch (t) {
+    case Tone::BELL: return BELL_P;
+    case Tone::RINGBACK: return RINGBACK_P;
+    case Tone::END: return END_P;
+    default: return NONE_P;
+    }
+}
+
+inline uint32_t seg_len(const Seg &s) { return (uint32_t)s.ms * PER_MS; }                // 칸 길이(표본)
+inline uint32_t cycle_len(const Pattern &p)                                              // 한 바퀴 길이(표본)
+{
+    uint32_t n = 0;
+    for (int i = 0; i < p.n; ++i) n += seg_len(p.seg[i]);
+    return n;
+}
+
+// 박자 계산: 소리를 시작한 지 pos 표본 뒤에 어느 칸의 몇째 표본인가. 한 번만 울리는 소리가 끝났거나 빈 패턴이면 seg = -1.
+struct Where { int seg; uint32_t off, len; };
+inline Where locate(const Pattern &p, uint64_t pos)
+{
+    const uint32_t total = cycle_len(p);
+    if (total == 0 || (!p.loop && pos >= total)) return Where{-1, 0, 0};
+    uint32_t r = (uint32_t)(pos % total);
+    for (int i = 0; i < p.n; ++i) {
+        const uint32_t len = seg_len(p.seg[i]);
+        if (r < len) return Where{i, r, len};
+        r -= len;
+    }
+    return Where{-1, 0, 0};
+}
+
+// 사인 표(257칸, 끝 = 처음). 위상(32비트)의 위 8비트로 칸을, 다음 8비트로 칸 사이를 잇는다(직선 보간 — 오차는 몇 LSB).
+struct SineTable {
+    int16_t v[257];
+    SineTable() { for (int i = 0; i <= 256; ++i) v[i] = (int16_t)std::lround(32767.0 * std::sin(i * (2.0 * 3.14159265358979323846 / 256.0))); }
+};
+inline const SineTable &sine_table() { static const SineTable t; return t; }   // 처음 부를 때 만든다(기기에서는 tone_out::start 가 미리 부른다)
+inline int32_t sine_q15(uint32_t phase)
+{
+    const SineTable &t = sine_table();
+    const uint32_t i = phase >> 24, frac = (phase >> 16) & 0xFF;
+    return t.v[i] + ((int32_t)t.v[i + 1] - t.v[i]) * (int32_t)frac / 256;
+}
+inline uint32_t phase_step(uint16_t hz) { return (uint32_t)(((uint64_t)hz << 32) / RATE); }   // 표본 하나에 나아가는 위상
+
+// 명령을 받아 PCM 을 만든다. 한 곳(출력 태스크)에서만 쓴다.
+class Player {
+public:
+    // 새 소리(BELL·RINGBACK·END) 또는 멈춤(NONE). 울리는 중이면 FADE 표본 안에 줄인 뒤 바꾼다(그 사이에 또 오면 마지막 것으로).
+    // 조용할 때(아무것도 안 내거나 박자의 쉼)는 바로 바꾼다. 같은 소리가 와도 처음부터 다시 시작한다.
+    void command(Tone t)
+    {
+        if (cur_ == Tone::NONE || gain_ == 0) begin(t);
+        else { next_ = t; releasing_ = true; }
+    }
+    void render(int16_t *out, int n) { for (int i = 0; i < n; ++i) out[i] = step(); }
+    Tone current() const { return cur_; }          // 내고 있는 소리(박자의 쉼 동안에도 그 소리). 완전히 조용해지면 NONE
+    bool idle() const { return cur_ == Tone::NONE; }
+    uint32_t starts() const { return starts_; }    // 소리를 새로 시작한 횟수 — 같은 소리를 다시 시작한 것도 기록 줄로 알리려고(transitions)
+    uint64_t pos() const { return pos_; }          // 지금 소리를 시작한 뒤 지난 표본 수
+
+private:
+    Tone cur_ = Tone::NONE, next_ = Tone::NONE;
+    bool releasing_ = false;
+    int gain_ = 0;                                 // 0..FADE
+    uint64_t pos_ = 0;
+    uint32_t ph1_ = 0, ph2_ = 0, starts_ = 0;
+
+    void begin(Tone t)
+    {
+        cur_ = pattern(t).n ? t : Tone::NONE; next_ = Tone::NONE; releasing_ = false;
+        gain_ = 0; pos_ = 0; ph1_ = ph2_ = 0;
+        if (cur_ != Tone::NONE) ++starts_;
+    }
+
+    int16_t step()
+    {
+        if (cur_ == Tone::NONE) return 0;
+        const Pattern &p = pattern(cur_);
+        const Where w = locate(p, pos_);
+        if (w.seg < 0) { begin(Tone::NONE); return 0; }                    // 한 번만 울리는 소리(종료음)가 끝났다
+        const Seg &s = p.seg[w.seg];
+        const bool up = !releasing_ && s.f1 != 0 && w.off + FADE < w.len;  // 칸의 마지막 FADE 표본은 줄인다 → 칸 끝에서 크기 0
+        if (up) { if (gain_ < FADE) ++gain_; }
+        else if (gain_ > 0) { --gain_; }
+        int32_t v = 0;
+        if (gain_ > 0) {
+            const int32_t wave = s.f2 ? (sine_q15(ph1_) + sine_q15(ph2_)) / 2 : sine_q15(ph1_);
+            v = wave * p.amp / 32767 * gain_ / FADE;
+        }
+        ph1_ += phase_step(s.f1); ph2_ += phase_step(s.f2);
+        if (w.off + 1 == w.len) ph1_ = ph2_ = 0;                            // 다음 칸은 위상 0 에서(칸 끝의 크기가 0 이라 끊김 없음)
+        ++pos_;
+        if (releasing_ && gain_ == 0) begin(next_);                         // 다 줄였다 → 다음 소리(멈춤이면 조용)
+        return (int16_t)v;
+    }
+};
+
+// 기록 줄(TONE <이름> on|off)에 무엇을 찍을지: 출력 한 묶음 전(before, 시작 횟수 sb)과 뒤(after, sa)를 비교한다.
+// 바뀌었거나(다른 소리·멈춤) 같은 소리를 다시 시작했으면 off = 앞의 소리, on = 새 소리. 찍을 것이 없으면 NONE.
+inline void transitions(Tone before, uint32_t sb, Tone after, uint32_t sa, Tone *off, Tone *on)
+{
+    const bool changed = before != after || sb != sa;
+    *off = changed ? before : Tone::NONE;
+    *on = changed ? after : Tone::NONE;
+}
+
+// 출력 한 묶음(tone_out.cpp 의 공급자가 16ms 마다 부른다): 우편함에 명령이 있었으면(has_cmd) 먼저 넣고 n 표본을 만든다.
+//   sound = out 에 소리를 썼나(false 면 낼 것이 없다 — out 은 건드리지 않는다. 줄어들어 멈춘 묶음은 그 줄어드는 소리가 있으므로 true)
+//   off·on = 이번 묶음에서 찍을 기록 줄: "TONE <off> off" 다음 "TONE <on> on"(없으면 NONE)
+struct Block { bool sound; Tone off, on; };
+inline Block next_block(Player &p, bool has_cmd, Tone cmd, int16_t *out, int n)
+{
+    const Tone before = p.current();
+    const uint32_t sb = p.starts();
+    if (has_cmd) p.command(cmd);
+    Block b{!p.idle(), Tone::NONE, Tone::NONE};
+    if (b.sound) p.render(out, n);
+    transitions(before, sb, p.current(), p.starts(), &b.off, &b.on);
+    return b;
+}
+
+// 상태 기계의 할 일 → 지금 내야 할 소리. 소리에 관한 비트가 없으면 false(지금 소리를 그대로 둔다).
+// PL3a-D12: 시작(종료음 > 벨 > 연결음)이 멈춤보다 앞선다. 지금 상태 기계는 소리 비트를 둘 이상 함께 내지 않는다(시험으로 고정).
+inline bool from_acts(uint16_t acts, Tone *out)
+{
+    if (acts & callfsm::A_TONE_END) *out = Tone::END;
+    else if (acts & callfsm::A_TONE_BELL) *out = Tone::BELL;
+    else if (acts & callfsm::A_TONE_RINGBACK) *out = Tone::RINGBACK;
+    else if (acts & callfsm::A_TONE_STOP) *out = Tone::NONE;
+    else return false;
+    return true;
+}
+
+}  // namespace tones
```

- [x] **Step 3: 커밋** (이 커밋은 헤더와 시험뿐이라 펌웨어 빌드에 영향이 없다 — 아직 아무도 `tones.h` 를 포함하지 않는다)

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/main/tones.h firmware/tab5_phone/tools/test_tones.cpp && git commit -m "feat(phone): 신호음 판단 헤더 tones.h — 벨·연결음·종료음의 박자·크기, 5ms 페이드, 멈춤 약속(맥 시험 84)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: 출력 모듈 `audio_out` 과 신호음 공급자 `tone_out` (기기 전용 코드 — 아직 잇지 않는다)

**Files:**
- Create: `firmware/tab5_phone/main/audio_out.h`, `main/audio_out.cpp`, `main/tone_out.h`, `main/tone_out.cpp`
- Modify: `firmware/tab5_phone/main/CMakeLists.txt` (소스 두 줄)

**Interfaces:**
- Consumes: Task 1 의 `tones::Tone`·`Player`·`next_block`·`tone_name`·`sine_table`, `M5.In_I2C`(M5Unified), `esp_codec_dev` 1.6.2(`es8388_codec_new`·`esp_codec_dev_open`·`esp_codec_dev_write`·`esp_codec_dev_set_out_vol`), `driver/i2s_std.h`.
- Produces (Task 3 이 이 이름·서명을 그대로 쓴다):
  - `namespace audioout`: `constexpr int RATE = 16000`, `BLOCK = 256`, `typedef bool (*source_fn)(int16_t *out, int n)`, `bool start(SemaphoreHandle_t i2c_lock)`, `bool add_source(source_fn)`, `void wake()`, `bool ready()`, `void block_us(uint32_t *avg, uint32_t *max)`, `uint32_t write_errors()`, `unsigned stack_free()`.
  - `namespace toneout`: `void start()`, `void play(tones::Tone)`, `tones::Tone now()`.
  - 시리얼 줄(이 과제의 코드가 찍는다): `AUDIO ready rate=16000 block=256 vol=100` 또는 `AUDIO FAIL <lock|i2c|i2s|codec|amp|task|tones>`, `TONE <BELL|RINGBACK|END> on`, `TONE <BELL|RINGBACK|END> off`.
- 이 과제의 커밋은 빌드된다(사본에서 확인: 새 파일 둘이 컴파일되고, 아직 부르는 곳이 없어 바이너리 크기는 그대로 0x231280). 실행 담당은 여기서 빌드하지 않는다(PL3a-D15 — 빌드는 Task 4 에서, 보관 뒤에).

- [x] **Step 1: 패치 셋을 적용하고 초안과 대조한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3a-tones/draft && git apply "$D/patches/T2-01-audio-out.patch" && git apply "$D/patches/T2-02-tone-out.patch" && git apply "$D/patches/T2-03-cmake.patch" && for f in audio_out.h audio_out.cpp tone_out.h tone_out.cpp CMakeLists.txt; do cmp "$D/files/firmware/tab5_phone/main/$f" "firmware/tab5_phone/main/$f" && echo "같음: $f"; done; git diff --numstat -- firmware/tab5_phone/main/CMakeLists.txt
```
Expected: `같음:` 5줄, `2	0	firmware/tab5_phone/main/CMakeLists.txt`

```diff
--- /dev/null
+++ b/firmware/tab5_phone/main/audio_out.h
@@ -0,0 +1,31 @@
+// audio_out.h — 스피커 출력(ES8388 코덱 + I2S0 송신). "PCM 을 받아 낸다" — 무엇을 낼지는 모른다.
+// 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md PL3a-D3·D4·D7~D10.
+//   - 출력 태스크가 BLOCK 표본(16ms)마다 등록된 공급자(source_fn)에게 PCM 을 받아 합쳐 코덱에 쓴다. 신호음(tone_out)이 공급자 하나이고,
+//     3b 의 목소리가 둘째가 된다. 낼 것이 없으면 쓰지 않고 잠든다 — I2S 는 계속 돌며 0 을 낸다(앰프는 켜 둔다, PL3a-D4).
+//   - 실패해도(코덱이 대답하지 않음 등) 전화기는 소리 없이 계속 돈다: start 가 false 와 "AUDIO FAIL <단계>" 한 줄(PL3a-D10).
+#pragma once
+#include <cstdint>
+
+#include "freertos/FreeRTOS.h"
+#include "freertos/semphr.h"
+
+namespace audioout {
+
+constexpr int RATE = 16000;                 // 표본율(PH-D5). 16비트 모노
+constexpr int BLOCK = 256;                  // 한 번에 내는 표본 수 = I2S DMA 한 칸(16ms, PL3a-D8)
+
+// 공급자: 출력 태스크가 BLOCK 마다 부른다. 낼 소리가 있으면 out 에 n 표본을 쓰고 true, 없으면 false(out 은 쓰지 않아도 된다).
+// 출력 태스크에서 불리므로 오래 걸리면 안 된다(묶음 하나 = 16ms).
+typedef bool (*source_fn)(int16_t *out, int n);
+
+// 코덱·I2S 를 열고 앰프를 켜고 출력 태스크를 띄운다. 한 번만. i2c_lock = 터치와 함께 쓰는 M5.In_I2C 잠금(phone_main 이 만든다).
+bool start(SemaphoreHandle_t i2c_lock);
+bool add_source(source_fn fn);              // 공급자 등록(최대 2). start 앞뒤 어느 때나. 자리가 없으면 false
+void wake();                                // 공급자에게 낼 것이 생겼다 — 잠든 출력 태스크를 깨운다(어느 태스크에서나, 소리 장치가 없으면 아무 일도 안 함)
+
+bool ready();                               // start 가 성공했나(STAT audio=)
+void block_us(uint32_t *avg, uint32_t *max);   // 소리를 낸 묶음 하나를 만드는 데(공급자 호출 + 합치기) 걸린 시간의 평균·최대(µs, 부팅 뒤). 묶음 하나 = 16000µs(STAT aud_us=평균/최대)
+uint32_t write_errors();                    // 코덱 쓰기 실패 횟수(STAT aud_err=)
+unsigned stack_free();                      // 출력 태스크 스택 최저 여유(바이트, 태스크가 없으면 0 — STAT stk_audio=)
+
+}  // namespace audioout
--- /dev/null
+++ b/firmware/tab5_phone/main/audio_out.cpp
@@ -0,0 +1,229 @@
+// audio_out.cpp — 스피커 출력. 설명은 audio_out.h.
+//
+// 근거(줄 단위 대조, 사용자 지시 2026-10-03): 이 기기에서 실제로 소리를 낸 Phase 0 시험 펌웨어 firmware/tab5_phone_probe/main/board_audio.cpp 와
+//   그 원본 xiaozhi-esp32 main/boards/m5stack/tab5/tab5_audio_codec.cc (MIT). 핀(MCLK30/BCLK27/WS29/DOUT26)·주소(ES8388 0x10, 앰프 = 확장칩 0x43 의 P1)·
+//   코덱 제어 인터페이스(M5.In_I2C + 잠금)·ES8388 설정(DAC, 따라가는 쪽, pa -1, 5.0V/3.3V)·여는 형식(16비트 1채널 16kHz)은 시험 펌웨어 그대로다.
+//   다른 점:
+//   PL3a-D7 송신 채널 하나만 만든다(마이크 칩 ES7210·수신 채널은 3b). Phase 0 부팅 로그(tab5_phone_probe/out/task4.log 177~181)에서
+//          수신을 열기 전의 송신이 지금과 같은 설정(STD TX data 16, slot 16, MONO, mask 0x1)으로 열린 것을 확인했다.
+//          비용: 3b 가 마이크를 붙일 때 i2s_new_channel 을 송수신 한 번에 만드는 모양(시험 펌웨어 i2s_setup)으로 바꿔야 한다 — 그때 송신 슬롯이
+//          32비트로 바뀌는 것도 Phase 0 와 같다. 탈출구: 시험 펌웨어 board_audio.cpp 의 i2s_setup·init 을 그대로 가져온다.
+//   PL3a-D8 DMA 한 칸을 256표본으로 적는다(시험 펌웨어는 기본 240 → 드라이버가 64바이트 맞춤으로 256 으로 바꾸며 경고, task4.log:177).
+//          쓰기 단위(BLOCK)를 DMA 칸과 같게 해야 쉬었다 다시 쓸 때 칸 중간부터 이어 쓰지 않는다(i2s_channel_write 의 rw_pos).
+//   PL3a-D2 음량 100(0dB) 고정 — 크기는 공급자가 디지털로 정한다(tones.h). 시험 펌웨어는 50 으로 열고 명령으로 바꿨다.
+//   PL3a-D4 앰프는 시작 때 한 번 켜고 끄지 않는다. 코덱을 연 뒤 30ms 쉬고 켠다(코덱 출력이 자리 잡은 뒤 — 켤 때 퍽 소리를 줄이려는 것, 효과는 귀로만 확인).
+#include "audio_out.h"
+
+#include <M5Unified.h>
+#include <atomic>
+#include <cstdio>
+#include <cstring>
+
+#include "driver/i2s_std.h"
+#include "esp_codec_dev.h"
+#include "esp_codec_dev_defaults.h"
+#include "esp_log.h"
+#include "esp_timer.h"
+#include "freertos/task.h"
+
+namespace audioout {
+namespace {
+
+const char *TAG = "audio_out";
+
+constexpr uint8_t ES8388_ADDR = 0x10, PI4IO1_ADDR = 0x43;
+constexpr uint32_t I2C_FREQ = 400000;
+constexpr gpio_num_t PIN_MCLK = GPIO_NUM_30, PIN_BCLK = GPIO_NUM_27, PIN_WS = GPIO_NUM_29, PIN_DOUT = GPIO_NUM_26;
+constexpr int VOLUME = 100;                     // PL3a-D2
+constexpr int DMA_DESC = 6;                     // DMA 칸 수 — 시험 펌웨어·xiaozhi 와 같다(6 × 16ms = 96ms 앞서 쓴다)
+constexpr int MAX_SOURCES = 2;                  // 신호음 + (3b) 목소리
+// PL3a-D9 출력 태스크: 코어 1(화면·터치와 같은 코어, Wi-Fi·교환기 연결은 코어 0), 우선순위 6(app 4·touch 5 보다 위 — 화면을 내보내는
+//   60~80ms 동안에도 16ms 박자를 지킨다. 한 묶음 계산은 1ms 에 못 미쳐 화면을 늦추지 않는다 — 어림이고 실제 값은 STAT aud_us), 스택 6KB(printf 한 줄 + 코덱 쓰기. STAT stk_audio 로 확인).
+constexpr int TASK_CORE = 1, TASK_PRIO = 6, TASK_STACK = 6 * 1024;
+
+SemaphoreHandle_t s_lock = nullptr;
+i2s_chan_handle_t s_tx = nullptr;
+esp_codec_dev_handle_t s_out = nullptr;
+TaskHandle_t s_task = nullptr;
+std::atomic<bool> s_ready{false};
+std::atomic<source_fn> s_src[MAX_SOURCES];
+std::atomic<uint32_t> s_max_us{0}, s_sum_us{0}, s_blocks{0}, s_wr_err{0}, s_i2c_err{0};
+int16_t s_mix[BLOCK], s_tmp[BLOCK];             // 출력 태스크에서만 쓴다
+
+// ── 코덱 제어 인터페이스: M5.In_I2C 로 레지스터 읽기·쓰기(시험 펌웨어 board_audio.cpp 그대로) ──────────────────
+struct m5_ctrl_t { audio_codec_ctrl_if_t base; uint8_t addr; bool opened; };
+
+int ctrl_open(const audio_codec_ctrl_if_t *c, void *, int) { ((m5_ctrl_t *)c)->opened = true; return ESP_CODEC_DEV_OK; }
+bool ctrl_is_open(const audio_codec_ctrl_if_t *c) { return ((const m5_ctrl_t *)c)->opened; }
+int ctrl_close(const audio_codec_ctrl_if_t *c) { ((m5_ctrl_t *)c)->opened = false; return ESP_CODEC_DEV_OK; }
+int ctrl_read(const audio_codec_ctrl_if_t *c, int reg, int reg_len, void *data, int data_len)
+{
+    auto *m = (const m5_ctrl_t *)c;
+    if (reg_len != 1 || !data || data_len <= 0) return ESP_CODEC_DEV_INVALID_ARG;
+    xSemaphoreTake(s_lock, portMAX_DELAY);
+    const bool ok = M5.In_I2C.readRegister(m->addr, (uint8_t)reg, (uint8_t *)data, (size_t)data_len, I2C_FREQ);
+    xSemaphoreGive(s_lock);
+    if (!ok) { ++s_i2c_err; return ESP_CODEC_DEV_READ_FAIL; }
+    return ESP_CODEC_DEV_OK;
+}
+int ctrl_write(const audio_codec_ctrl_if_t *c, int reg, int reg_len, void *data, int data_len)
+{
+    auto *m = (const m5_ctrl_t *)c;
+    if (reg_len != 1 || !data || data_len <= 0) return ESP_CODEC_DEV_INVALID_ARG;
+    xSemaphoreTake(s_lock, portMAX_DELAY);
+    const bool ok = M5.In_I2C.writeRegister(m->addr, (uint8_t)reg, (const uint8_t *)data, (size_t)data_len, I2C_FREQ);
+    xSemaphoreGive(s_lock);
+    if (!ok) { ++s_i2c_err; return ESP_CODEC_DEV_WRITE_FAIL; }
+    return ESP_CODEC_DEV_OK;
+}
+int ctrl_info(const audio_codec_ctrl_if_t *c, audio_codec_ctrl_info_t *info)
+{
+    if (!info) return ESP_CODEC_DEV_INVALID_ARG;
+    info->type = AUDIO_CODEC_CTRL_I2C; info->i2c.addr = ((const m5_ctrl_t *)c)->addr; info->i2c.port = 1;
+    return ESP_CODEC_DEV_OK;
+}
+m5_ctrl_t make_ctrl(uint8_t addr)
+{
+    m5_ctrl_t m = {};
+    m.base.open = ctrl_open; m.base.is_open = ctrl_is_open; m.base.read_reg = ctrl_read; m.base.write_reg = ctrl_write;
+    m.base.get_info = ctrl_info; m.base.close = ctrl_close;
+    m.addr = addr; m.opened = true;             // 코덱 드라이버는 "이미 열린" 인터페이스를 요구한다
+    return m;
+}
+m5_ctrl_t s_ctrl_out = make_ctrl(ES8388_ADDR);
+
+bool codec_answers()                            // 코덱이 I2C 에 대답하나(레지스터 하나 읽기 — 시험 펌웨어 health_check 의 ES8388 쪽)
+{
+    uint8_t v = 0;
+    xSemaphoreTake(s_lock, portMAX_DELAY);
+    const bool ok = M5.In_I2C.readRegister(ES8388_ADDR, 0x19, &v, 1, I2C_FREQ);
+    xSemaphoreGive(s_lock);
+    return ok;
+}
+
+bool i2s_setup()                                // PL3a-D7·D8: 송신 채널 하나(표준 I2S), DMA 6칸 × 256표본
+{
+    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
+    chan_cfg.dma_desc_num = DMA_DESC;
+    chan_cfg.dma_frame_num = BLOCK;
+    chan_cfg.auto_clear_after_cb = true;        // PL3a-D4: 보낸 칸은 0 으로 지운다 → 쓰지 않으면 무음(잡음 대신)
+    if (i2s_new_channel(&chan_cfg, &s_tx, nullptr) != ESP_OK) return false;
+    i2s_std_config_t tx_cfg = {
+        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(RATE),
+        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
+        .gpio_cfg = {
+            .mclk = PIN_MCLK, .bclk = PIN_BCLK, .ws = PIN_WS, .dout = PIN_DOUT, .din = I2S_GPIO_UNUSED,
+            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
+        },
+    };
+    return i2s_channel_init_std_mode(s_tx, &tx_cfg) == ESP_OK && i2s_channel_enable(s_tx) == ESP_OK;
+}
+
+bool codec_open()                               // ES8388 을 DAC 로 열고 음량을 건다(시험 펌웨어 init 의 출력 쪽)
+{
+    audio_codec_i2s_cfg_t i2s_cfg = {};
+    i2s_cfg.port = I2S_NUM_0; i2s_cfg.rx_handle = nullptr; i2s_cfg.tx_handle = s_tx;
+    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);
+    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
+    if (!data_if || !gpio_if) return false;
+
+    es8388_codec_cfg_t out_cfg = {};
+    out_cfg.ctrl_if = &s_ctrl_out.base; out_cfg.gpio_if = gpio_if;
+    out_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
+    out_cfg.master_mode = false;                // 클럭은 P4 가 낸다(칩은 따라가는 쪽)
+    out_cfg.pa_pin = -1;                        // 앰프는 확장칩으로 직접 켠다
+    out_cfg.hw_gain.pa_voltage = 5.0; out_cfg.hw_gain.codec_dac_voltage = 3.3;
+    const audio_codec_if_t *out_codec = es8388_codec_new(&out_cfg);
+    if (!out_codec) return false;
+
+    esp_codec_dev_cfg_t dev_cfg = {};
+    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_OUT; dev_cfg.codec_if = out_codec; dev_cfg.data_if = data_if;
+    s_out = esp_codec_dev_new(&dev_cfg);
+    if (!s_out) return false;
+    esp_codec_dev_sample_info_t fs = {};
+    fs.bits_per_sample = 16; fs.channel = 1; fs.channel_mask = 0; fs.sample_rate = RATE;
+    if (esp_codec_dev_open(s_out, &fs) != ESP_CODEC_DEV_OK || esp_codec_dev_set_out_vol(s_out, VOLUME) != ESP_CODEC_DEV_OK) return false;
+    return s_i2c_err == 0;                      // 음량 설정은 코덱 쓰기가 실패해도 OK 를 돌려준다(esp_codec_dev.c:376~378) → 쓰기 실패 횟수로 확인한다
+}
+
+bool amp_on()                                   // 스피커 앰프 전원(확장칩 0x43 의 P1). M5Unified 가 부팅 때 꺼 둔다
+{
+    xSemaphoreTake(s_lock, portMAX_DELAY);
+    const bool ok = M5.In_I2C.bitOn(PI4IO1_ADDR, 0x05, 0b00000010, I2C_FREQ);   // 읽고-고쳐-쓰기(트랜잭션 둘) — 잠금 안에서
+    xSemaphoreGive(s_lock);
+    return ok;
+}
+
+void out_task(void *)
+{
+    while (true) {
+        const int64_t t0 = esp_timer_get_time();
+        bool any = false;
+        for (int i = 0; i < MAX_SOURCES; ++i) {
+            const source_fn fn = s_src[i].load();
+            if (!fn || !fn(s_tmp, BLOCK)) continue;
+            if (!any) { memcpy(s_mix, s_tmp, sizeof s_mix); any = true; continue; }
+            for (int k = 0; k < BLOCK; ++k) {   // 둘째 공급자부터는 더한다(넘치면 자른다) — 3b 의 목소리 + 신호음
+                const int32_t v = (int32_t)s_mix[k] + s_tmp[k];
+                s_mix[k] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
+            }
+        }
+        if (!any) { ulTaskNotifyTake(pdTRUE, portMAX_DELAY); continue; }    // PL3a-D4: 낼 것이 없으면 쓰지 않고 잠든다(DMA 가 0 을 낸다)
+        const uint32_t us = (uint32_t)(esp_timer_get_time() - t0);          // 소리를 낸 묶음만 잰다(기록 줄을 찍은 묶음은 printf 시간이 든다 → 최댓값)
+        if (us > s_max_us.load()) s_max_us = us;
+        s_sum_us += us; ++s_blocks;
+        if (esp_codec_dev_write(s_out, s_mix, (int)sizeof s_mix) != ESP_CODEC_DEV_OK) {             // 정상이면 DMA 칸이 빌 때까지 기다린다 = 16ms 박자
+            ++s_wr_err;
+            vTaskDelay(pdMS_TO_TICKS(16));      // 쓰기가 바로 실패해도 박자를 지킨다 — 쉬지 않고 돌면 우선순위가 낮은 화면·터치 태스크가 굶는다(PL3a-D10)
+        }
+    }
+}
+
+}  // namespace
+
+bool start(SemaphoreHandle_t i2c_lock)
+{
+    if (s_ready || s_tx) return s_ready;        // 한 번만(두 번째 호출은 첫 결과)
+    s_lock = i2c_lock;
+    const char *fail = nullptr;
+    if (!s_lock) fail = "lock";
+    else if (!codec_answers()) fail = "i2c";
+    else if (!i2s_setup()) fail = "i2s";
+    else if (!codec_open()) fail = "codec";
+    else {
+        vTaskDelay(pdMS_TO_TICKS(30));          // PL3a-D4: 코덱 출력이 자리 잡은 뒤 앰프를 켠다
+        if (!amp_on()) fail = "amp";
+        else if (xTaskCreatePinnedToCore(out_task, "audio", TASK_STACK, nullptr, TASK_PRIO, &s_task, TASK_CORE) != pdPASS) fail = "task";
+    }
+    if (fail) {                                 // PL3a-D10: 소리 없이 계속 — 재시작하지 않는다
+        ESP_LOGE(TAG, "audio init failed at '%s' -> the phone runs without sound", fail);
+        printf("AUDIO FAIL %s\n", fail);
+        return false;
+    }
+    s_ready = true;
+    printf("AUDIO ready rate=%d block=%d vol=%d\n", RATE, BLOCK, VOLUME);
+    return true;
+}
+
+bool add_source(source_fn fn)
+{
+    for (auto &slot : s_src) {
+        source_fn empty = nullptr;
+        if (slot.compare_exchange_strong(empty, fn)) { wake(); return true; }
+    }
+    return false;
+}
+
+void wake() { if (s_task) xTaskNotifyGive(s_task); }
+
+bool ready() { return s_ready; }
+void block_us(uint32_t *avg, uint32_t *max)
+{
+    const uint32_t n = s_blocks.load();
+    if (avg) *avg = n ? s_sum_us.load() / n : 0;
+    if (max) *max = s_max_us.load();
+}
+uint32_t write_errors() { return s_wr_err; }
+unsigned stack_free() { return s_task ? (unsigned)uxTaskGetStackHighWaterMark(s_task) : 0; }
+
+}  // namespace audioout
```

```diff
--- /dev/null
+++ b/firmware/tab5_phone/main/tone_out.h
@@ -0,0 +1,12 @@
+// tone_out.h — 신호음 공급자. app 이 상태 기계의 할 일에서 고른 소리(tones::from_acts)를 받아, 출력 태스크(audio_out)에 PCM 으로 건넨다.
+// 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md PL3a-D3·D6·D11. 박자·크기·멈춤 약속은 tones.h(맥 시험)에 있다.
+#pragma once
+#include "tones.h"
+
+namespace toneout {
+
+void start();                   // audio_out 에 공급자로 등록한다. audioout::start 뒤, app::start 앞에 한 번(소리 장치가 없어도 불러도 된다)
+void play(tones::Tone t);       // 이 소리를 낸다(NONE = 멈춤). app 태스크에서. 바로 돌아온다 — 마지막 명령만 남는다(PL3a-D6)
+tones::Tone now();              // 지금 내는 소리(STAT tone=). 출력 태스크가 바꾼다
+
+}  // namespace toneout
--- /dev/null
+++ b/firmware/tab5_phone/main/tone_out.cpp
@@ -0,0 +1,56 @@
+// tone_out.cpp — 신호음 공급자. 설명은 tone_out.h.
+//
+// PL3a-D6 명령은 길이 1 우편함(xQueueOverwrite)으로 건넨다 — 마지막 명령만 남는다. 명령 하나가 "지금 내야 할 소리"를 통째로 말하므로
+//         마지막 것만 맞으면 된다(맥 시험 "stop then bell in one batch"). 대기열이 가득 차 멈춤 명령을 잃는 일이 없다.
+//         비용: 출력 태스크가 꺼내기 전에 벨 → 멈춤이 연달아 오면 그 벨은 한 표본도 나지 않고 "TONE BELL on" 줄도 없다. 탈출구: 길이 8 대기열.
+// PL3a-D11 기록 줄 "TONE <이름> on|off" 는 출력 태스크가 소리를 실제로 만든 뒤에 찍는다(무엇을 찍을지는 tones::next_block — 맥 시험).
+//         off = 만든 PCM 이 0 이 된 때. 스피커에서 소리가 그치는 것은 그보다 DMA 에 앞서 쓴 만큼(최대 6칸 = 96ms) 늦다.
+//         이 줄은 출력 태스크가 찍으므로 원인이 된 "FSM …" 줄(app 태스크)과의 앞뒤는 정해져 있지 않다 — 기기 시험은 기기별 TONE 줄의 순서만 본다.
+#include "tone_out.h"
+
+#include <atomic>
+#include <cstdio>
+
+#include "freertos/FreeRTOS.h"
+#include "freertos/queue.h"
+
+#include "audio_out.h"
+
+namespace toneout {
+namespace {
+
+QueueHandle_t s_box = nullptr;                          // 길이 1 우편함
+tones::Player s_player;                                 // 출력 태스크에서만 만진다
+std::atomic<tones::Tone> s_now{tones::Tone::NONE};
+
+bool fill(int16_t *out, int n)                          // audioout 의 공급자 — 출력 태스크에서 16ms 마다
+{
+    tones::Tone cmd = tones::Tone::NONE;
+    const bool has_cmd = s_box && xQueueReceive(s_box, &cmd, 0) == pdTRUE;
+    const tones::Block b = tones::next_block(s_player, has_cmd, cmd, out, n);   // 명령 → 한 묶음 → 찍을 줄(맥 시험으로 고정된 로직)
+    if (b.off != tones::Tone::NONE) printf("TONE %s off\n", tones::tone_name(b.off));
+    if (b.on != tones::Tone::NONE) printf("TONE %s on\n", tones::tone_name(b.on));
+    s_now = s_player.current();
+    return b.sound;
+}
+
+}  // namespace
+
+void start()
+{
+    if (s_box) return;
+    s_box = xQueueCreate(1, sizeof(tones::Tone));
+    tones::sine_table();                                // 사인 표를 지금 만든다(출력 태스크의 첫 묶음이 늦지 않게)
+    if (!s_box || !audioout::add_source(fill)) printf("AUDIO FAIL tones\n");
+}
+
+void play(tones::Tone t)
+{
+    if (!s_box) return;
+    xQueueOverwrite(s_box, &t);
+    audioout::wake();
+}
+
+tones::Tone now() { return s_now; }
+
+}  // namespace toneout
```

```diff
--- a/firmware/tab5_phone/main/CMakeLists.txt
+++ b/firmware/tab5_phone/main/CMakeLists.txt
@@ -8,6 +8,8 @@
                            "gfx.cpp"          # 화면 합성·출력·스크린샷
                            "net_wifi.cpp"     # Wi-Fi 매니저
                            "ui_wifi.cpp"      # Wi-Fi 설정 화면
+                           "audio_out.cpp"    # 스피커 출력(ES8388 + I2S 송신) — 공급자에게 PCM 을 받아 낸다(Phase 3a)
+                           "tone_out.cpp"     # 신호음 공급자(벨·연결음·종료음, 박자는 tones.h)
                        INCLUDE_DIRS ".")
 
 # B-D1: -Wliteral-suffix 한 종류만 끈다(main 의 C++ 파일에만).
```

- [x] **Step 2: 이 기기에서 소리를 낸 시험 펌웨어와 줄 단위로 대조한다** (이 두 파일은 맥 시험이 없다 — 대조 결과를 보고에 적는다)

```bash
cd ~/tab5-toy-phone/firmware && for k in "ES8388_ADDR = 0x10" "PI4IO1_ADDR = 0x43" "I2C_FREQ = 400000" "PIN_MCLK = GPIO_NUM_30, PIN_BCLK = GPIO_NUM_27, PIN_WS = GPIO_NUM_29, PIN_DOUT = GPIO_NUM_26" "info->i2c.port = 1" "auto_clear_after_cb = true" "I2S_STD_CLK_DEFAULT_CONFIG(" "I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)" ".mclk = PIN_MCLK, .bclk = PIN_BCLK, .ws = PIN_WS, .dout = PIN_DOUT, .din = I2S_GPIO_UNUSED" "out_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC" "out_cfg.master_mode = false" "out_cfg.pa_pin = -1" "out_cfg.hw_gain.pa_voltage = 5.0; out_cfg.hw_gain.codec_dac_voltage = 3.3" "dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_OUT; dev_cfg.codec_if = out_codec; dev_cfg.data_if = data_if" "bitOn(PI4IO1_ADDR, 0x05, 0b00000010, I2C_FREQ)"; do echo "$(grep -cF -- "$k" tab5_phone_probe/main/board_audio.cpp) $(grep -cF -- "$k" tab5_phone/main/audio_out.cpp)  $k"; done
```
Expected: 15줄 모두 `1 1  <글자>` — 앞 숫자는 시험 펌웨어, 뒤 숫자는 새 파일에서 그 글자가 나온 횟수다(둘 다 1 = 같은 값을 쓴다).

같은 것(위 15줄)과 **다른 것 넷**을 읽어서 확인한다:

| 항목 | 시험 펌웨어 `tab5_phone_probe/main/board_audio.cpp` | `audio_out.cpp` | 판단 |
|---|---|---|---|
| I2S 채널 | 송수신 한 쌍 `i2s_new_channel(&chan_cfg, &s_tx, &s_rx)` + 수신 4칸 TDM | 송신만 `i2s_new_channel(&chan_cfg, &s_tx, nullptr)`, 수신·ES7210 없음 | PL3a-D7 — 소리가 안 나면 1순위 가설 |
| DMA | 기본값(6칸 × 240표본 → 드라이버가 256 으로 바꿈) | `dma_desc_num = 6`, `dma_frame_num = BLOCK`(256) | PL3a-D8 |
| 음량 | `set_volume(50)` 뒤 명령으로 80·100 | `esp_codec_dev_set_out_vol(s_out, 100)` 한 번 | PL3a-D2 |
| 앰프 | 코덱을 연 직후 `amp(true)` | 30ms 쉰 뒤 `amp_on()`, 끄는 함수 없음 | PL3a-D4 |

실패 경로(Review Focus 5)를 읽어서 확인한다 — `audio_out.cpp` 에서:

| 확인할 것 | 코드 |
|---|---|
| 시작이 어느 단계에서 실패해도 `false` 를 돌려주고 `AUDIO FAIL <단계>` 한 줄만 찍는다(재시작·`abort` 없음) | `start()` 의 `if (fail) { … printf("AUDIO FAIL %s\n", fail); return false; }` |
| 코덱 쓰기(I2C)가 조용히 실패한 것을 잡는다 | `codec_open()` 끝의 `return s_i2c_err == 0;`(`ctrl_read`·`ctrl_write` 가 센다) |
| 쓰기가 실패해도 쉬지 않고 돌지 않는다 | `out_task()` 의 `++s_wr_err; vTaskDelay(pdMS_TO_TICKS(16));` |
| 낼 것이 없으면 쓰지 않고 잠든다(깨우는 것은 `wake()` 뿐) | `out_task()` 의 `if (!any) { ulTaskNotifyTake(pdTRUE, portMAX_DELAY); continue; }` |
| 소리 장치가 없어도 `play()`·`wake()` 는 아무 일 없이 돌아온다 | `tone_out.cpp` `play()` 의 `xQueueOverwrite` + `audioout::wake()`(`s_task` 가 없으면 아무 일도 안 함) |
| I2C 를 만지는 곳은 모두 잠금 안이다 | `ctrl_read`·`ctrl_write`·`codec_answers`·`amp_on` 의 `xSemaphoreTake(s_lock, …)` … `xSemaphoreGive(s_lock)` |

- [x] **Step 3: 커밋**

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/main/audio_out.h firmware/tab5_phone/main/audio_out.cpp firmware/tab5_phone/main/tone_out.h firmware/tab5_phone/main/tone_out.cpp firmware/tab5_phone/main/CMakeLists.txt && git commit -m "feat(phone): 스피커 출력 audio_out(ES8388·I2S 송신) + 신호음 공급자 tone_out — 아직 잇지 않음" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: 통화 흐름에 잇기 + Phase 2 가 넘긴 정리(N-1·N-2·I2C 잠금) + 도구 시험

**Files:**
- Modify: `firmware/tab5_phone/main/app.h`, `main/app.cpp`, `main/phone_main.cpp`(연결·I2C 잠금), `main/relay_link.cpp`(N-1), `main/gfx.h`·`main/gfx.cpp`·`main/ui_wifi.cpp`(N-2 — 주석만)
- Test: `firmware/tab5_phone/tools/test_phone_ctl.py` (추가만)

**Interfaces:**
- Consumes: Task 1 의 `tones::from_acts`·`tone_name`, Task 2 의 `audioout::start`·`ready`·`block_us`·`write_errors`·`stack_free`, `toneout::start`·`play`·`now`.
- Produces:
  - `void app::start(int unit, SemaphoreHandle_t i2c_lock)`(인자 하나 늘었다), `app::i2c_lock()` 은 없어진다.
  - `STAT` 줄: 앞의 칸은 그대로, 끝에 ` audio=<0|1> tone=<NONE|BELL|RINGBACK|END> aud_us=<평균>/<최대> aud_err=<n> stk_audio=<바이트>`.
  - 소리가 나는 때(상태 기계의 할 일 그대로): `IDLE → INCOMING`(0x20) 벨 / `DIALING → RINGBACK`(0x10) 연결음 / `acts` 에 0x40 이 든 전이(거절당함·무응답·통화 뒤 끊김·통화 중 끊어짐) 종료음 / 0x80 이 든 전이 멈춤.
- **`T3-04`(app)와 `T3-05`(phone_main)는 함께 적용하고 한 커밋으로 한다** — 하나만 적용하면 빌드되지 않는다(사본에서 확인: `main/phone_main.cpp:139:27: error: too few arguments to function 'void app::start(int, SemaphoreHandle_t)'`).

- [x] **Step 1: 도구 시험 패치를 적용한다 — 도구를 고치지 않아도 새 줄 모양이 맞는다는 것을 고정한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3a-tones/draft && git apply "$D/patches/T3-01-phone-ctl-tests.patch" && git diff --numstat -- firmware/tab5_phone/tools/test_phone_ctl.py && cmp "$D/files/firmware/tab5_phone/tools/test_phone_ctl.py" firmware/tab5_phone/tools/test_phone_ctl.py && cd firmware/tab5_phone/tools && PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider test_phone_ctl.py 2>&1 | tail -1
```
Expected: `104	0	firmware/tab5_phone/tools/test_phone_ctl.py`(삭제 줄 0), `186 passed in …s` (기존 152 + 새 34. 새 시험은 지금 도구로 바로 통과한다 — `phone_ctl.py` 를 고치지 않고도 `STAT` 끝의 새 칸과 `TONE` 줄을 그대로 다룬다는 것, 앞 계획·README 의 정규식이 새 `STAT` 에도 맞는다는 것을 고정하는 시험이다. ESP-IDF 를 export 하지 않은 호출에서 돌린다.)

저장소 밖 검증 도구 `touch_audit.py` 도 새 `STAT` 모양을 읽는지 가짜 로그로 확인한다(둘째 구간에 주입 없는 터치 2개를 일부러 넣었다):

```bash
D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3a-tones/draft && printf '%s\n' '[2] STAT up=100 unit=2 fsm=IDLE screen=IDLE wifi=1 relay=1 relay_code=0 peer=1 missed=0 touch=3 heap_int=258000 heap_int_min=251000 heap_psram=24788704 stk_app=10280 stk_touch=3284 stk_relay=6108 stk_ws=6128 audio=1 tone=NONE aud_us=0/0 aud_err=0 stk_audio=4312' '[2] OK t 404 624' '[2] TONE BELL off' '[2] STAT up=103 unit=2 fsm=INCOMING screen=INCOMING wifi=1 relay=1 relay_code=0 peer=1 missed=0 touch=4 heap_int=258000 heap_int_min=251000 heap_psram=24788704 stk_app=10280 stk_touch=3284 stk_relay=6108 stk_ws=6128 audio=1 tone=NONE aud_us=58/912 aud_err=0 stk_audio=4312' '[2] STAT up=105 unit=2 fsm=INCOMING screen=INCOMING wifi=1 relay=1 relay_code=0 peer=1 missed=0 touch=6 heap_int=258000 heap_int_min=251000 heap_psram=24788704 stk_app=10280 stk_touch=3284 stk_relay=6108 stk_ws=6128 audio=1 tone=NONE aud_us=58/912 aud_err=0 stk_audio=4312' > /tmp/p3a_touch_fake.log && python3 "$D/verify/touch_audit.py" /tmp/p3a_touch_fake.log; echo "rc=$?"
```
Expected: `== /tmp/p3a_touch_fake.log: 불일치 1건`, `  줄 5 기기 2 up=105: touch 4→6 (+2), 주입 0 → 설명 안 되는 터치 +2  [INCOMING/INCOMING peer=1]`, `rc=1` (첫 구간 3→4 는 주입 1과 맞아 조용하고, 일부러 넣은 둘째 구간만 잡는다 = 새 `STAT` 에서도 같은 칸을 읽는다)

```diff
--- a/firmware/tab5_phone/tools/test_phone_ctl.py
+++ b/firmware/tab5_phone/tools/test_phone_ctl.py
@@ -757,3 +757,107 @@
     assert pc.main(["--unit", "2", "run", "until:stat:^STAT .*fsm=IDLE:5"]) == 0
     assert opened == [("1", "2"), ("1", "2"), ("2",)]
     assert "[1] STAT " in capsys.readouterr().out
+
+
+# ── Phase 3a(신호음): STAT 끝에 소리 칸이 붙고 TONE 줄이 생겨도 도구와 앞 계획의 정규식이 그대로 맞는다 ──────────
+#    계획 docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md PL3a-D11. 도구(phone_ctl.py)는 고치지 않는다 — 이 시험들이 그것을 고정한다.
+def stat3a(fsm="IDLE", unit=2, peer=1, tone="NONE", audio=1, screen=None, touch=3, missed=0):
+    """Phase 3a 펌웨어의 STAT 줄 모양(main/app.cpp print_stat): 앞의 칸은 Phase 2 그대로, 끝에 audio= tone= aud_us=평균/최대 aud_err= stk_audio=."""
+    return (f"STAT up=157 unit={unit} fsm={fsm} screen={screen or fsm} wifi=1 relay=1 relay_code=0 peer={peer} missed={missed} touch={touch} "
+            f"heap_int=262000 heap_int_min=255000 heap_psram=24788704 stk_app=10280 stk_touch=3284 stk_relay=6108 stk_ws=6128 "
+            f"audio={audio} tone={tone} aud_us=61/840 aud_err=0 stk_audio=4200")
+
+
+def test_parse_stat_reads_the_sound_fields_and_the_old_fields_as_before():
+    s = pc.parse_stat(stat3a(fsm="INCOMING", tone="BELL"))
+    assert s["tone"] == "BELL" and s["audio"] == 1 and s["aud_us"] == (61, 840) and s["aud_err"] == 0 and s["stk_audio"] == 4200
+    assert s["fsm"] == "INCOMING" and s["screen"] == "INCOMING" and s["touch"] == 3 and s["stk_ws"] == 6128 and s["heap_int_min"] == 255000
+    assert all(type(s[k]) is int for k in s if k not in ("fsm", "screen", "tone", "aud_us"))
+
+
+@pytest.mark.parametrize("tone", ["NONE", "BELL", "RINGBACK", "END"])
+def test_parse_stat_reads_every_tone_name(tone):
+    assert pc.parse_stat(stat3a(tone=tone))["tone"] == tone
+
+
+def test_touch_audit_pattern_reads_the_new_stat():
+    """touch_audit.py(저장소 밖 검증 도구)의 정규식을 글자 그대로 옮겼다 — 터치 대조가 새 STAT 에서도 같은 칸을 읽는다."""
+    rx = r"^STAT up=(\d+) .*?fsm=(\w+) screen=(\w+).*?peer=(\d).*?touch=(\d+)"
+    assert re.match(rx, stat3a(fsm="INCOMING", screen="DECLINE", touch=41)).groups() == ("157", "INCOMING", "DECLINE", "1", "41")
+
+
+@pytest.mark.parametrize("pattern, kwargs", [
+    (r"^STAT ", {}),
+    (r"^STAT .*fsm=IDLE", {}),
+    (r"^STAT .*fsm=IDLE.*peer=1", {}),
+    (r"^STAT .*fsm=IDLE.*relay=1", {}),
+    (r"^STAT .*fsm=IDLE.*relay=1.*peer=1", {}),
+    (r"^STAT .*fsm=IDLE.*relay=1.*peer=1.*missed=0", {}),
+    (r"^STAT .*fsm=IDLE.*screen=IDLE.*missed=0", {}),
+    (r"^STAT .*fsm=IDLE screen=IDLE.*peer=1", {}),
+    (r"^STAT .*screen=IDLE.*missed=1", {"missed": 1}),
+    (r"^STAT .*fsm=IN_CALL.*screen=IN_CALL", {"fsm": "IN_CALL"}),
+    (r"^STAT .*fsm=INCOMING screen=INCOMING", {"fsm": "INCOMING"}),
+    (r"^STAT .*fsm=INCOMING screen=DECLINE", {"fsm": "INCOMING", "screen": "DECLINE"}),
+    (r"^STAT .*fsm=RINGBACK screen=DIALING", {"fsm": "RINGBACK", "screen": "DIALING"}),
+])
+def test_stat_patterns_of_the_readme_and_earlier_plans_still_match(pattern, kwargs):
+    assert re.search(pattern, stat3a(**kwargs))
+
+
+def test_margin_grep_picks_up_the_new_task_stack_too():
+    """기기 과제의 여유 확인(grep -oE 'heap_int_min=[0-9]+|stk_[a-z]+=[0-9]+')이 새 태스크의 스택도 집는다."""
+    got = re.findall(r"heap_int_min=[0-9]+|stk_[a-z]+=[0-9]+", stat3a())
+    assert got == ["heap_int_min=255000", "stk_app=10280", "stk_touch=3284", "stk_relay=6108", "stk_ws=6128", "stk_audio=4200"]
+
+
+@pytest.mark.parametrize("pattern, fsm, tone, want", [
+    (r"^STAT .*fsm=INCOMING.*tone=BELL", "INCOMING", "BELL", True),
+    (r"^STAT .*fsm=INCOMING.*tone=BELL", "INCOMING", "NONE", False),
+    (r"^STAT .*fsm=INCOMING.*tone=NONE", "INCOMING", "NONE", True),
+    (r"^STAT .*fsm=INCOMING.*tone=NONE", "INCOMING", "BELL", False),
+    (r"^STAT .*fsm=RINGBACK.*tone=RINGBACK", "RINGBACK", "RINGBACK", True),
+    (r"^STAT .*fsm=IN_CALL.*tone=NONE", "IN_CALL", "NONE", True),
+    (r"^STAT .*fsm=IN_CALL.*tone=NONE", "IN_CALL", "RINGBACK", False),
+    (r"^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE", "IDLE", "NONE", True),
+    (r"^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE", "IDLE", "END", False),
+    (r"^STAT .*fsm=IDLE.*peer=1.*tone=NONE", "IDLE", "NONE", True),
+    (r"^STAT .*fsm=IDLE.*peer=1.*tone=NONE", "IDLE", "BELL", False),
+])
+def test_tone_patterns_of_the_phase3a_device_task(pattern, fsm, tone, want):
+    assert bool(re.search(pattern, stat3a(fsm=fsm, tone=tone))) is want
+
+
+def test_audio_off_is_visible_in_the_stat():
+    """소리 장치 초기화가 실패한 기기(audio=0)는 'audio=1 tone=' 을 기대하는 단계에서 걸린다."""
+    assert not re.search(r"^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE", stat3a(audio=0))
+    assert pc.parse_stat(stat3a(audio=0))["audio"] == 0
+
+
+def test_until_stat_keeps_asking_until_the_bell_is_off():
+    """벨 끄기 뒤의 확인: until 은 tone=NONE 인 STAT 이 올 때까지 2초마다 다시 묻는다. 그 사이의 TONE 줄은 여느 줄처럼 지나간다."""
+    hub, links, clock, out = make_hub(("2",))
+    answers = [stat3a("INCOMING", tone="BELL"), stat3a("INCOMING", tone="NONE")]
+
+    def respond(link, cmd):
+        link.say(answers[min(len(link.sent), len(answers)) - 1], at=clock.t)
+        if len(link.sent) == 1:
+            link.say("TONE BELL off", at=clock.t + 0.5)
+    links["2"].respond = respond
+    assert hub.until(("2",), "stat", r"^STAT .*fsm=INCOMING.*tone=NONE", 10)
+    assert links["2"].cmds == ["stat", "stat"] and "TONE BELL off" in out
+
+
+def test_tone_lines_do_not_disturb_a_tap_or_a_screenshot(tmp_path):
+    """TONE 줄은 출력 태스크가 찍어 FSM 줄·OK 줄·B64 줄 사이 어디에나 올 수 있다 — tap 과 shot 은 자기 줄만 꺼내고 TONE 줄은 남긴다."""
+    hub, links, clock, out = make_hub(("2",))
+    links["2"].respond = lambda link, cmd: link.say("TONE BELL off", "OK " + cmd, "FSM INCOMING -> INCOMING acts=0x80 notice=NONE", at=clock.t)
+    assert hub.tap(("2",), 404, 624)
+    assert hub.queues["2"].lines == ["TONE BELL off", "FSM INCOMING -> INCOMING acts=0x80 notice=NONE"]
+    lines = shot_lines(JPEG)
+    lines.insert(2, "TONE END on")
+    links["2"].respond = shooter(clock, lines)
+    path = tmp_path / "a.jpg"
+    assert hub.shot("2", str(path)) and path.read_bytes() == JPEG
+    assert hub.queues["2"].lines == ["TONE BELL off", "FSM INCOMING -> INCOMING acts=0x80 notice=NONE", "TONE END on"]
+    assert hub.wait(("2",), r"^TONE END on$", 0)
```

- [x] **Step 2: 정리 패치 둘(N-1·N-2)을 적용하고 대조한 뒤 따로 커밋한다** (소리와 무관한 정리다 — 되돌릴 때 따로 남길 수 있게 커밋을 나눈다. 이 커밋도 빌드된다)

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3a-tones/draft && git apply "$D/patches/T3-02-relay-link-n1.patch" && git apply "$D/patches/T3-03-gfx-n2.patch" && for f in relay_link.cpp gfx.h gfx.cpp ui_wifi.cpp; do cmp "$D/files/firmware/tab5_phone/main/$f" "firmware/tab5_phone/main/$f" && echo "같음: $f"; done; cd firmware/tab5_phone/main && grep -c "s_phase = Phase::IDLE;" relay_link.cpp; echo "G-D4 $(cat gfx.cpp gfx.h ui_wifi.cpp | grep -c 'G-D4')곳, G-D3 $(cat gfx.cpp gfx.h ui_wifi.cpp | grep -c 'G-D3')곳"; grep -n "G-D3" gfx.cpp | cut -c1-60
```
Expected: `같음:` 4줄, `4`(끊김 핸들러 둘 + stop 앞 + **stop 뒤** — 전에는 3), `G-D4 7곳, G-D3 2곳`, 이어서 G-D3 두 줄이 모두 화면 회전(`20://   G-D3 PPA 회전 각도는 …`, `158:    switch (M5.Display.getRotation()) { … // G-D3`)인지 본다 — 비공개 프레임 결정은 G-D4 로 옮겨 갔다.

- N-1(재검수 `fix-1-rereview.md`): `reconnect` 처리에서 `esp_websocket_client_stop()` **앞에서만** `s_phase` 를 IDLE 로 내려, stop 이 도는 동안 핸들러가 올린 값(UP)이 stop 뒤에 남을 수 있었다(표시만 틀림) → stop 뒤에도 한 번 더 내린다.
- N-2: 결정 번호 G-D3 이 둘(스크린샷 비공개 / 화면 회전)이었다 → 비공개 쪽을 G-D4 로(주석 7곳, 코드 변화 없음).

```diff
--- a/firmware/tab5_phone/main/relay_link.cpp
+++ b/firmware/tab5_phone/main/relay_link.cpp
@@ -16,7 +16,8 @@
 //         (transport_ws.c 의 poll_read). 즉시 보낸 ping 의 pong 이 소켓을 깨워 그 지연을 없앤다.
 //   RL-D8 esp_websocket_client_start() 가 ESP_FAIL(전송 객체·태스크 생성 실패, esp_websocket_client.c:1469-1479)이면 relay_task 가 5초마다 다시 부른다
 //         (이 태스크에서만 — stop/start 와 send_text 의 경합을 넓히지 않는다). stop 을 부르기 전에 s_phase 를 IDLE 로 내려 stop 이 끝날 때까지
-//         up() 이 거짓말하지 않게 한다. 이유: 반환값을 버리면 한 번의 실패가 재부팅 전까지 영구 불통이 된다(M-2).
+//         up() 이 거짓말하지 않게 하고, stop 이 끝난 뒤에도 한 번 더 내린다(N-1: 접속 중에 stop 이 걸리면 stop 도중 핸들러가 UP 으로 올릴 수 있다).
+//         이유: 반환값을 버리면 한 번의 실패가 재부팅 전까지 영구 불통이 된다(M-2).
 //         비용: 실패가 계속되면 5초마다 로그 한 줄. 탈출구: 재시도 블록(start_pending)을 지우면 이전 동작(한 번만 시도)이다.
 //   RL-D9 토큰이 든 접속 요청 전문을 오류 로그로 찍는 transport_ws 를 막는 장치는 esp_log_level_set 이고, 그것은 CONFIG_LOG_DYNAMIC_LEVEL_CONTROL=y 에 기댄다
 //         → sdkconfig.defaults 에 고정하고 꺼지면 빌드가 실패하게 한다(M-4). 탈출구: 둘 다 지우면 Kconfig 기본값에 다시 기댄다.
@@ -174,6 +175,7 @@
             const bool was_up = s_phase == Phase::UP;
             s_phase = Phase::IDLE;                               // RL-D8: stop 이 끝날 때까지 UP 으로 남지 않게 먼저 내린다
             esp_websocket_client_stop(s_ws);                     // 태스크가 끝날 때까지 기다린다(끊김 이벤트는 오지 않는다)
+            s_phase = Phase::IDLE;                               // N-1: stop 이 도는 동안 핸들러가 올린 값(CONNECTING·UP)이 남지 않게 한 번 더
             if (was_up) post(In::Kind::LINK_DOWN);
             printf("RELAY up=0 code=0 attempt=%d\n", s_fails.load());
             start_pending = esp_websocket_client_start(s_ws) != ESP_OK;
```

```diff
--- a/firmware/tab5_phone/main/gfx.h
+++ b/firmware/tab5_phone/main/gfx.h
@@ -68,7 +68,7 @@
 //   - 화면 태스크가 다음 화면을 그리는 도중이면 반쪽 화면을 찍지 않고, 곧 올 present() 직후의 화면을 찍는다(최대 1초 기다림,
 //     그 안에 present() 가 없으면 0). 그래서 "그리기 → present()" 를 짝지어 부를 것 — 그려 놓고 present 를 미루면 캡처가 실패한다.
 //   - 품질 85, 1280×720. 내부 버퍼는 1MB.
-//   - G-D3(I-1): 비공개 프레임이면 0 을 돌려주고 *is_private 를 true 로 한다(is_private 는 null 이어도 된다). "비공개인가"는 present() 가
+//   - G-D4(I-1): 비공개 프레임이면 0 을 돌려주고 *is_private 를 true 로 한다(is_private 는 null 이어도 된다). "비공개인가"는 present() 가
 //     s_lock 안에서 실제로 내보낸 프레임에 붙이는 값이라 촬영과 화면 사이에 틈이 없다. 두 길(바로 찍기·present() 의 대리 인코딩) 모두 인코딩 자체를 하지 않는다.
 size_t screenshot_jpeg(const uint8_t **out, bool *is_private = nullptr);
 
--- a/firmware/tab5_phone/main/gfx.cpp
+++ b/firmware/tab5_phone/main/gfx.cpp
@@ -6,7 +6,7 @@
 //             카메라 복사용 PPA(ppa_copy_frame), JPEG 디코더, s_proc_mutex.
 //
 // 결정
-//   G-D3(I-1) 비밀번호가 든 자판 화면은 스크린샷으로 내보내지 않는다. 프레임마다 "비공개" 표시를 두고(clear() 가 거짓, 그리는 쪽이 set_private),
+//   G-D4(I-1) 비밀번호가 든 자판 화면은 스크린샷으로 내보내지 않는다. 프레임마다 "비공개" 표시를 두고(clear() 가 거짓, 그리는 쪽이 set_private),
 //        present() 가 s_lock 안에서 그 값을 "내보낸 화면의 값"으로 확정한다. screenshot_jpeg() 의 두 길 모두 그 값이 참이면 인코딩 없이 0 + is_private.
 //        비용: 비밀번호 입력 중에는 화면을 못 찍는다. 탈출구: ui_layout.h is_private_frame() 이 false 만 돌려주게 한다.
 //   G-D1 합성 버퍼는 1장, present() 는 동기(끝날 때까지 기다림).
@@ -62,7 +62,7 @@
 uint32_t s_shot_len = 0;                                      // s_lock 보호
 bool s_shot_priv = false;                                     // s_lock 보호 — 대리 인코딩이 비공개라서 건너뛰었다
 bool s_priv_draft = false;                                    // 지금 그리는 프레임의 비공개 표시(화면 태스크 전용 — clear() 가 거짓으로, set_private 가 설정)
-bool s_priv_shown = false;                                    // 마지막으로 present() 한 프레임의 비공개 표시(s_lock 보호) — G-D3
+bool s_priv_shown = false;                                    // 마지막으로 present() 한 프레임의 비공개 표시(s_lock 보호) — G-D4
 
 jpeg_encoder_handle_t s_jenc = nullptr;
 uint8_t *s_jenc_out = nullptr;
@@ -211,7 +211,7 @@
 {
     if (!s_fb) return;
     mark();
-    s_priv_draft = false;                                     // G-D3: 프레임은 clear() 로 시작한다 → 기본값은 공개
+    s_priv_draft = false;                                     // G-D4: 프레임은 clear() 로 시작한다 → 기본값은 공개
     std::fill_n((uint16_t *)s_buf, (size_t)SCREEN_W * SCREEN_H, rgb565(rgb));
 }
 
@@ -237,7 +237,7 @@
     esp_err_t err = ppa_do_scale_rotate_mirror(s_ppa, &op);
     if (err != ESP_OK) ESP_LOGW(TAG, "ppa srm failed: %s", esp_err_to_name(err));
     s_dirty.store(false);                                     // 이제 합성 버퍼 = 화면
-    s_priv_shown = s_priv_draft;                              // G-D3: 방금 내보낸 프레임의 비공개 값을 여기서 확정한다
+    s_priv_shown = s_priv_draft;                              // G-D4: 방금 내보낸 프레임의 비공개 값을 여기서 확정한다
     if (s_shot_req.exchange(false)) {                         // G-D2: 캡처가 기다리고 있으면 지금(그리기가 멈춘 순간) 인코딩
         s_shot_priv = s_priv_shown;
         s_shot_len = s_shot_priv ? 0 : encode_locked();
@@ -431,7 +431,7 @@
     xSemaphoreTake(s_lock, portMAX_DELAY);
     bool clean = !s_dirty.load();
     if (clean) {                                              // 합성 버퍼 = 마지막으로 내보낸 화면 → 바로 찍는다
-        if (s_priv_shown) {                                   // G-D3: 그 화면이 비공개면 인코딩하지 않는다
+        if (s_priv_shown) {                                   // G-D4: 그 화면이 비공개면 인코딩하지 않는다
             xSemaphoreGive(s_lock);
             if (is_private) *is_private = true;
             return 0;
--- a/firmware/tab5_phone/main/ui_wifi.cpp
+++ b/firmware/tab5_phone/main/ui_wifi.cpp
@@ -397,7 +397,7 @@
     s_dirty = false;
     gfx::clear(C_BG);
     if (s_view == View::LIST) draw_list(); else draw_pass();
-    gfx::set_private(uilayout::is_private_frame(s_view == View::PASS, s_pw.size()));   // I-1: 입력이 든 자판 화면은 스크린샷 거절(gfx G-D3). 프레임 시작(clear)에서 공개로 되돌아간다
+    gfx::set_private(uilayout::is_private_frame(s_view == View::PASS, s_pw.size()));   // I-1: 입력이 든 자판 화면은 스크린샷 거절(gfx G-D4). 프레임 시작(clear)에서 공개로 되돌아간다
 }
 
 bool tap(int x, int y)
```

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/tools/test_phone_ctl.py firmware/tab5_phone/main/relay_link.cpp firmware/tab5_phone/main/gfx.h firmware/tab5_phone/main/gfx.cpp firmware/tab5_phone/main/ui_wifi.cpp && git commit -m "fix(phone): 재검수 Minor 정리 — relay_link stop 뒤에도 IDLE(N-1), gfx 결정 번호 G-D4(N-2) + 도구 시험(새 STAT·TONE 줄 모양 고정)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [x] **Step 3: 연결 패치 둘을 함께 적용하고 초안과 대조한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3a-tones/draft && git apply "$D/patches/T3-04-app.patch" && git apply "$D/patches/T3-05-phone-main.patch" && for f in app.h app.cpp phone_main.cpp; do cmp "$D/files/firmware/tab5_phone/main/$f" "firmware/tab5_phone/main/$f" && echo "같음: $f"; done
```
Expected: `같음:` 3줄.

```diff
--- a/firmware/tab5_phone/main/app.h
+++ b/firmware/tab5_phone/main/app.h
@@ -7,13 +7,11 @@
 
 namespace app {
 
-// 화면·통화 진행 태스크를 띄운다. gfx::init, netwifi::start, relaylink::start 뒤에 한 번. unit = 이 기기의 번호(미등록이면 0).
-void start(int unit);
+// 화면·통화 진행 태스크를 띄운다. gfx::init, netwifi::start, relaylink::start, audioout::start, toneout::start 뒤에 한 번. unit = 이 기기의 번호(미등록이면 0).
+// i2c_lock = M5.In_I2C 를 쓰는 코드가 함께 잡는 잠금. 터치 읽기(M5.update)와 코덱·앰프 제어(audio_out)가 같은 버스를 쓴다 — phone_main 이 만든다(PL3a-D14).
+void start(int unit, SemaphoreHandle_t i2c_lock);
 
 void inject_tap(int x, int y);      // 시리얼 't x y' — 실제 터치와 같은 대기열(8칸)에 넣는다. 화면 태스크가 다음 바퀴에 처리한다
 void print_stat();                  // 시리얼 'stat' — STAT 한 줄을 찍는다(어느 태스크에서 불러도 된다)
 
-// M5.In_I2C 를 쓰는 코드가 함께 잡는 잠금. 터치 읽기(M5.update)와 Phase 3 의 코덱·앰프 제어가 같은 버스를 쓴다.
-SemaphoreHandle_t i2c_lock();
-
 }  // namespace app
--- a/firmware/tab5_phone/main/app.cpp
+++ b/firmware/tab5_phone/main/app.cpp
@@ -12,6 +12,10 @@
 //       메시지 고르기 화면의 열림(s_picker)은 화면 쪽 상태다 — call_fsm 은 INCOMING 그대로라 벨 제한·상대 취소·부재중이 그대로 돈다(DM-D8).
 //       그래서 call_fsm 에 사건을 넣을 때마다(feed) s_picker.sync 로 맞춘다: INCOMING 을 떠나면 닫힌다.
 //       비용: 기억이 두 군데(Fsm::bell_muted / s_picker). 탈출구: 고르기 열림도 Fsm 에 넣는다.
+// 신호음(Phase 3a, docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md): apply() 가 할 일의 A_TONE_* 를 tones::from_acts 로 소리 하나로 바꿔
+//       toneout::play 에 넘긴다. PL3a-D13: 신호 보내기보다 먼저 — 보내기가 Wi-Fi 때문에 최대 2초 막혀도(RL-D6) 벨은 누른 즉시 멈춘다.
+//       비용: 기록 줄 "TONE …" 이 원인이 된 "FSM …" 줄보다 먼저 찍힐 수 있다(다른 태스크가 찍는다 — 원래도 앞뒤가 정해져 있지 않다).
+//       탈출구: 소리 명령을 FSM 줄 printf 뒤로 옮긴다.
 #include "app.h"
 
 #include <M5Unified.h>
@@ -25,11 +29,14 @@
 #include "freertos/queue.h"
 #include "freertos/task.h"
 
+#include "audio_out.h"
 #include "call_fsm.h"
 #include "gfx.h"
 #include "net_wifi.h"
 #include "relay_link.h"
 #include "relay_proto.h"
+#include "tone_out.h"
+#include "tones.h"
 #include "ui.h"
 #include "ui_layout.h"
 #include "ui_wifi.h"
@@ -68,6 +75,8 @@
 
 void apply(const Out &o, State before, int64_t now)
 {
+    tones::Tone tone;
+    if (tones::from_acts(o.acts, &tone)) toneout::play(tone);   // PL3a-D13: 보내기보다 먼저. 소리 비트가 없으면 지금 소리를 그대로 둔다
     static const uint16_t SENDS[] = { A_SEND_CALL, A_SEND_ACCEPT, A_SEND_REJECT, A_SEND_HANGUP };
     for (uint16_t bit : SENDS) {
         if (!(o.acts & bit)) continue;
@@ -77,7 +86,7 @@
         if (!relaylink::send_text(text)) ESP_LOGW(TAG, "signal 0x%x not sent (link down)", bit);
     }
     if (o.acts & A_RECONNECT) relaylink::reconnect();
-    // 소리: Phase 3 에서 tones·voice_pipeline 을 여기에 잇는다(A_TONE_*, A_VOICE_*). Phase 2 는 아래 FSM 줄에 acts 로만 남는다.
+    // 목소리(A_VOICE_*)는 3b 에서 잇는다 — 지금은 아래 FSM 줄에 acts 로만 남는다.
     if (o.notice != Notice::NONE) { s_notice = o.notice; s_notice_msg = o.msg; s_notice_until = now + uilayout::notice_ms(o.notice); }   // 거절 문구는 10초, 그 밖은 4초
     if (s_fsm.state != before || o.acts || o.notice != Notice::NONE) {
         char tail[12] = "";                                      // 문구 번호가 있을 때만 줄 끝에 " msg=N"(기존 줄의 글자는 그대로 — DM-D9). 한 번의 printf 로 찍는다(줄이 쪼개지지 않게)
@@ -197,10 +206,10 @@
 
 }  // namespace
 
-void start(int unit)
+void start(int unit, SemaphoreHandle_t i2c_lock)
 {
     s_unit = unit;
-    s_i2c = xSemaphoreCreateMutex();
+    s_i2c = i2c_lock;                                            // PL3a-D14: 잠금은 phone_main 이 만든다(audio_out 과 함께 쓴다)
     s_stat_lock = xSemaphoreCreateMutex();
     s_tap_q = xQueueCreate(8, sizeof(TapEv));
     xTaskCreatePinnedToCore(app_task, "app", 12 * 1024, nullptr, 4, &s_app_task, 1);
@@ -220,16 +229,18 @@
     if (s_stat_lock) { xSemaphoreTake(s_stat_lock, portMAX_DELAY); m = s_stat; screen = s_stat_screen; xSemaphoreGive(s_stat_lock); }
     unsigned stk_relay = 0, stk_ws = 0;
     relaylink::stack_free(&stk_relay, &stk_ws);
+    uint32_t aud_avg = 0, aud_max = 0;
+    audioout::block_us(&aud_avg, &aud_max);
+    // PL3a-D11: 소리 칸은 줄 끝에 더한다(앞의 칸은 글자 하나 바꾸지 않는다 — phone_ctl.py·touch_audit.py 의 정규식이 그대로 맞는다)
     printf("STAT up=%u unit=%d fsm=%s screen=%s wifi=%d relay=%d relay_code=%d peer=%d missed=%u touch=%u heap_int=%u heap_int_min=%u heap_psram=%u"
-           " stk_app=%u stk_touch=%u stk_relay=%u stk_ws=%u\n",
+           " stk_app=%u stk_touch=%u stk_relay=%u stk_ws=%u audio=%d tone=%s aud_us=%u/%u aud_err=%u stk_audio=%u\n",
            (unsigned)(esp_timer_get_time() / 1000000), m.unit, state_name(m.fsm), screen, (int)m.wifi_connected, (int)m.relay_up, m.relay_code,
            (int)m.peer_online, (unsigned)m.missed, (unsigned)s_touches.load(),
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
            s_app_task ? (unsigned)uxTaskGetStackHighWaterMark(s_app_task) : 0, s_touch_task ? (unsigned)uxTaskGetStackHighWaterMark(s_touch_task) : 0,
-           stk_relay, stk_ws);
+           stk_relay, stk_ws, (int)audioout::ready(), tones::tone_name(toneout::now()), (unsigned)aud_avg, (unsigned)aud_max,
+           (unsigned)audioout::write_errors(), audioout::stack_free());
 }
 
-SemaphoreHandle_t i2c_lock() { return s_i2c; }
-
 }  // namespace app
```

```diff
--- a/firmware/tab5_phone/main/phone_main.cpp
+++ b/firmware/tab5_phone/main/phone_main.cpp
@@ -1,7 +1,8 @@
-// phone_main.cpp — Tab5 음성 전화기(Phase 2: 소리 없이 벨·받기·거절·끊기). 부팅, 태스크 기동, 시리얼 명령만 한다.
+// phone_main.cpp — Tab5 음성 전화기(Phase 3a: 벨·연결음·종료음까지 — 목소리는 3b). 부팅, 태스크 기동, 시리얼 명령만 한다.
 //
 // 설계: docs/exec-plans/2026-10-03-tab5-phone.md §9.5. 두 대에 같은 펌웨어를 올리고, 기기 번호는 MAC 으로 정한다(device_id).
-// 태스크: app(화면·통화 진행, 코어1) / touch(터치 읽기, 코어1) / wifi(매니저, 코어0) / relay(교환기 연결 관리, 코어0) / relay_ws(부품의 웹소켓 태스크) / sercmd(시리얼 명령, 코어0)
+// 태스크: app(화면·통화 진행, 코어1) / touch(터치 읽기, 코어1) / audio(스피커 출력, 코어1) / wifi(매니저, 코어0) / relay(교환기 연결 관리, 코어0)
+//        / relay_ws(부품의 웹소켓 태스크) / sercmd(시리얼 명령, 코어0)
 //
 // 시리얼 명령(115200, 줄 단위) — tools/phone_ctl.py 가 쓴다:
 //   stat | t <x> <y> | shot | reboot
@@ -22,11 +23,13 @@
 #include "nvs_flash.h"
 
 #include "app.h"
+#include "audio_out.h"
 #include "device_id.h"
 #include "gfx.h"
 #include "net_wifi.h"
 #include "relay_link.h"
 #include "secrets.h"
+#include "tone_out.h"
 
 static const char *TAG = "phone";
 
@@ -108,7 +111,7 @@
 extern "C" void app_main(void)
 {
     auto cfg = M5.config();
-    cfg.internal_spk = false;           // 소리는 Phase 3 에서 esp_codec_dev 로 직접 다룬다(PH-D4) — M5Unified 가 I2S0·코덱을 잡지 않게
+    cfg.internal_spk = false;           // 소리는 audio_out 이 esp_codec_dev 로 직접 다룬다(PH-D4) — M5Unified 가 I2S0·코덱을 잡지 않게
     cfg.internal_mic = false;
     M5.begin(cfg);
 
@@ -133,9 +136,18 @@
     const bool gfx_ok = gfx::init();
     boot_guard(gfx_ok, "gfx");
 
+    // PL3a-D14: M5.In_I2C 잠금은 여기서 한 번 만든다(정적 — 만들기가 실패하지 않는다). 터치(app)와 코덱·앰프(audio_out)가 같은 버스를 쓴다.
+    //   전에는 app::start 안에서 만들어, 화면이 실패해 app 이 없으면 잠금도 없었다(Phase 2 최종 검수 권고 5).
+    static StaticSemaphore_t i2c_lock_buf;
+    SemaphoreHandle_t i2c_lock = xSemaphoreCreateMutexStatic(&i2c_lock_buf);
+
     netwifi::start();
     if (me.unit && gfx_ok) relaylink::start(RELAY_URL, me.id, me.token);   // M-3: 화면이 없으면(app 태스크 없음) 교환기에 "켜짐"으로 보이지 않게 붙지 않는다
     else if (me.unit) ESP_LOGE(TAG, "gfx init failed -> relay not started");
-    if (gfx_ok) app::start(me.unit);
+    if (gfx_ok) {
+        audioout::start(i2c_lock);      // PL3a-D10: 실패하면 "AUDIO FAIL <단계>" 한 줄 — 전화기는 소리 없이 계속 돈다
+        toneout::start();
+        app::start(me.unit, i2c_lock);
+    }
     xTaskCreatePinnedToCore(serial_task, "sercmd", 8 * 1024, nullptr, 3, nullptr, 0);
 }
```

- [x] **Step 4: 이은 자리를 코드와 하나씩 대조한다** (이 세 파일은 맥 시험이 없다 — 읽어서 확인하고 보고에 적는다)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone/main && for f in phone_main.cpp app.cpp relay_link.cpp; do grep -Hn "toneout::play(tone)\|s_i2c = i2c_lock\|xSemaphoreCreateMutexStatic\|audioout::start(i2c_lock)\|toneout::start();\|app::start(me.unit, i2c_lock)\|N-1: stop 이 도는 동안\|aud_us=%u/%u" "$f" | cut -c1-110; done; echo "i2c_lock() 남은 곳: $(cat app.cpp app.h | grep -c 'i2c_lock()')"
```
Expected:
```
phone_main.cpp:142:    SemaphoreHandle_t i2c_lock = xSemaphoreCreateMutexStatic(&i2c_lock_buf);
phone_main.cpp:148:        audioout::start(i2c_lock);      // PL3a-D10: 실패하면 "AUDIO FAIL <단계>" 한 줄 — 전화기는 소리 없
phone_main.cpp:149:        toneout::start();
phone_main.cpp:150:        app::start(me.unit, i2c_lock);
app.cpp:79:    if (tones::from_acts(o.acts, &tone)) toneout::play(tone);   // PL3a-D13: 보내기보다 먼저. 소리 비트가 없으면 지
app.cpp:212:    s_i2c = i2c_lock;                                            // PL3a-D14: 잠금은 phone_main 이 만든다
app.cpp:236:           " stk_app=%u stk_touch=%u stk_relay=%u stk_ws=%u audio=%d tone=%s aud_us=%u/%u aud_err=
relay_link.cpp:178:            s_phase = Phase::IDLE;                               // N-1: stop 이 도는 동안 핸들러가 
i2c_lock() 남은 곳: 0
```

| 확인할 것 | 코드 |
|---|---|
| 소리 명령은 `apply()` 의 **맨 앞**(신호 보내기 `for` 앞)이고, 소리 비트가 없으면 `play` 를 부르지 않는다(지금 소리 그대로) | `app.cpp:78~79` |
| 소리 명령은 `from_acts` 한 곳으로만 정한다(`app.cpp` 에 `A_TONE_` 을 직접 보는 코드가 없다) | `grep -n "A_TONE_" app.cpp` → 머리 주석 한 줄(`15:`)뿐 |
| `STAT` 의 앞 칸은 글자 하나 바뀌지 않고 끝에 다섯 칸이 붙는다, 넘기는 인자 순서 = 형식 순서(`audio` `tone` `aud_us` 평균·최대 `aud_err` `stk_audio`) | `app.cpp:235~243` |
| I2C 잠금: `phone_main` 이 정적으로 한 번 만들고(`xSemaphoreCreateMutexStatic`), `audioout::start` 와 `app::start` 에 **같은 것**을 넘긴다. `app::start` 는 더 이상 만들지 않는다 | `phone_main.cpp:141~142`, `148`, `150`, `app.cpp:212` |
| 시작 순서: `audioout::start` → `toneout::start` → `app::start`(app 태스크가 뜨기 전에 소리 쪽이 준비된다). 화면이 실패하면(`gfx_ok` 거짓) 셋 다 부르지 않는다 — 전과 같이 교환기에도 붙지 않는다 | `phone_main.cpp:147~151` |
| `audioout::start` 의 결과를 보지 않는다(실패해도 다음 줄로 — PL3a-D10) | `phone_main.cpp:148` |

- [x] **Step 5: 맥 시험·도구 시험이 그대로인지 다시 돌리고 커밋한다**

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone/tools && SDK="$(xcrun --sdk macosx --show-sdk-path)" && for T in wifi_store call_fsm relay_proto device_id ui_layout tones; do clang++ -std=c++17 -Wall -Wextra -isysroot "$SDK" -I../main test_$T.cpp -o /tmp/t_p3a_$T 2> /tmp/t_p3a_$T.err; echo "$T: $(/tmp/t_p3a_$T | tail -1) (경고 $(wc -l < /tmp/t_p3a_$T.err | tr -d ' ')줄)"; done; PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider test_phone_ctl.py 2>&1 | tail -1; cd ~/tab5-toy-phone/server && PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider 2>&1 | tail -1
```
Expected: `wifi_store: 16 passed, 0 failed (경고 0줄)` · `call_fsm: 105 ok, 0 failed (경고 0줄)` · `relay_proto: 61 ok, 0 failed (경고 0줄)` · `device_id: 9 ok, 0 failed (경고 0줄)` · `ui_layout: 91 ok, 0 failed (경고 0줄)` · `tones: 84 ok, 0 failed (경고 0줄)` · `186 passed in …s` · `137 passed in …s`

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/main/app.h firmware/tab5_phone/main/app.cpp firmware/tab5_phone/main/phone_main.cpp && git commit -m "feat(phone): 신호음 연결 — A_TONE_* 실행(벨·연결음·종료음·멈춤), I2C 잠금을 phone_main 으로, STAT 끝에 소리 칸" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: 지금 기기의 빌드를 보관하고, 새로 빌드해 "올릴 바이너리"를 확정한다

**Files:**
- Create(git 제외): `firmware/tab5_phone/out/fw_prev_066cf57/build/`(지금 기기에 올라간 빌드의 보관본), `out/p3a_build.log`, `out/p3a_fw_build.sha256`, `out/p3a_fw_version.txt`

**Interfaces:**
- Consumes: Task 1~3 의 커밋 넷(저장소 HEAD), 저장소의 `firmware/tab5_phone/build/`(지금은 두 기기에 올라가 있는 `066cf57` 빌드).
- Produces: Task 5 가 올릴 `build/`, 그 검사합 `out/p3a_fw_build.sha256`, 버전 `out/p3a_fw_version.txt`, 롤백용 `out/fw_prev_066cf57/build/`(기존 `out/fw_prev_113097c/build/` 와 같은 구조).

- [x] **Step 1: 지금 기기에 올라가 있는 빌드를 보관한다** (PL3a-D15 — 다음 Step 이 `build/` 를 덮어쓴다)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && grep '"project_version"' build/project_description.json && mkdir -p out/fw_prev_066cf57/build/bootloader out/fw_prev_066cf57/build/partition_table && cp build/flash_args build/tab5_phone.bin out/fw_prev_066cf57/build/ && cp build/bootloader/bootloader.bin out/fw_prev_066cf57/build/bootloader/ && cp build/partition_table/partition-table.bin out/fw_prev_066cf57/build/partition_table/ && shasum -a 256 out/fw_prev_066cf57/build/tab5_phone.bin | cut -c1-64 && shasum -a 256 -c out/fx_fw_build.sha256 && git status --short | grep -c "fw_prev"
```
Expected: `    "project_version":    "066cf57",`, `c86233366b294d5e89750b82e64f3a6082b507f8c141d91826f2e100e9095786`, `build/tab5_phone.bin: OK` 등 `OK` 3줄(Phase 2 수정 빌드를 두 기기에 올릴 때 적어 둔 검사합과 같다), `0`(git 에 보이지 않는다). **버전이 `066cf57` 이 아니거나 검사합이 다르면 멈춘다(BLOCKED)** — 누군가 그 뒤에 다시 빌드한 것이라 보관본이 "기기에 올라간 것"이라는 보장이 없다(이때는 아무것도 지우지 말고 보고한다).

- [x] **Step 2: 전체 빌드** (export 와 빌드를 같은 호출에. 1~3분)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && shasum -a 256 dependencies.lock | cut -c1-64 > /tmp/p3a_lock_before.txt && source ~/esp/esp-idf-5.5.4/export.sh > /dev/null 2>&1 && idf.py build > out/p3a_build.log 2>&1; echo "rc=$?"; grep -E "tab5_phone.bin binary size|Project build complete" out/p3a_build.log; echo "우리 코드 경고: $(grep 'warning:' out/p3a_build.log | grep -c 'tab5_phone/main/')"; echo "오류: $(grep -c 'error:' out/p3a_build.log)"; grep -c "version: 2.11.7" dependencies.lock; shasum -a 256 dependencies.lock | cut -c1-64 | cmp - /tmp/p3a_lock_before.txt && echo "dependencies.lock 그대로"
```
Expected(사본에서 이 명령 그대로 돌린 출력 — 자리 값 `secrets.h`):
```
rc=0
tab5_phone.bin binary size 0x242230 bytes. Smallest app partition is 0x600000 bytes. 0x3bddd0 bytes (62%) free.
Project build complete. To flash, run:
우리 코드 경고: 0
오류: 0
1
dependencies.lock 그대로
```
크기 줄의 두 숫자만 저장소에서 조금 다르다: `secrets.h` 가 달라 사본보다 0x3d0 쯤 크게 나온다(Phase 2 때 사본 0x231280 ↔ 저장소 0x231650 — 그대로면 0x242600 근처). **0x242000~0x243000 사이면 정상**, 벗어나면 보고에 적는다. `(62%) free` 와 나머지 줄은 같아야 한다.
`rc` 가 0 이 아니면 `grep -m5 "error:" out/p3a_build.log` 를 보고에 붙이고 멈춘다(코드를 고치지 않는다).

- [x] **Step 3: 검사합과 버전을 적어 둔다**

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && shasum -a 256 build/tab5_phone.bin build/bootloader/bootloader.bin build/partition_table/partition-table.bin > out/p3a_fw_build.sha256 && grep '"project_version"' build/project_description.json | tee out/p3a_fw_version.txt && git log --oneline -1 && cut -c1-12 out/p3a_fw_build.sha256 && cmp -s build/tab5_phone.bin out/fw_prev_066cf57/build/tab5_phone.bin; echo "보관본과 다른가(1 이어야 한다): $?"
```
Expected: `"project_version":    "<지금 HEAD 의 짧은 SHA>"`(바로 아래 `git log` 줄의 SHA 와 같다. 끝에 `-dirty` 가 붙으면 작업 트리에 커밋 안 된 변경이 있다는 뜻 — `git status --short` 를 보고에 적는다. 올리는 데는 지장 없다), 검사합 세 줄, `보관본과 다른가(1 이어야 한다): 1`. **이 뒤로 Task 5 가 끝날 때까지 `idf.py build`·`idf.py flash` 를 돌리지 않는다**(바이너리가 달라진다).

---

### Task 5: 기기 검증 — 두 대에 올리고, 소리 상태를 기록으로 확인한 뒤, 사용자가 귀로 확인한다

**Files:**
- Create(git 제외): `firmware/tab5_phone/out/p3a_*.log`, `out/p3a_flash_unit*.txt`

**Interfaces:**
- Consumes: Task 4 의 `build/`·`out/p3a_fw_build.sha256`·`out/p3a_fw_version.txt`·`out/fw_prev_066cf57/`, `tools/phone_ctl.py pair`, `D/verify/touch_audit.py`·`D/verify/secret_scan.py`, 켜져 있는 개발용 교환기(바꾸지 않는다).
- Produces: 시나리오 A~G 의 통과 기록(기기별 `TONE` 줄 순서 포함), 터치 대조·비밀·여유·`aud_us` 기록, 사용자 귀 확인 결과(점검표 10문항).

모든 명령은 ESP-IDF 를 export 하지 **않은** 호출에서 돌린다(플래시 Step 만 export). 좌표: 통화 `640,360` · 취소 `640,604` · 받기 `404,428` · 거절 `876,428` · 끊기 `644,606` · 벨 끄기 `404,624` · 메시지로 거절 `876,624` · 문구 2 `640,168`.
각 실행의 통과 조건은 공통이다: **`rc=0` 이고 `!!` 줄이 없고, 끝의 기기별 `TONE` 요약 두 줄이 Expected 와 글자까지 같다.** `FSM` 줄의 `acts` 값은 Phase 2 기기 로그(`out/fx_*.log`)와 같고, `TONE` 줄의 순서는 맥 시험 `test_tones.cpp` "scenario …" 가 고정한 것이다. 같은 실행이 실패하면 로그로 원인을 확인하고 그 실행만 다시 돌린다(3회까지. 세 번 모두 실패하면 멈춘다).
**이 과제 동안 두 기기에서 실제로 소리가 난다** — 컨트롤러가 사용자에게 미리 알린다.

- [x] **Step 1: 사전 확인** — 컨트롤러가 사용자에게 "약 10분 동안 두 기기 화면을 만지지 말아 달라, 그동안 벨·연결음이 여러 번 울린다(한 번은 30초)"고 알린 뒤에 시작한다

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && shasum -a 256 -c out/p3a_fw_build.sha256 && cat out/p3a_fw_version.txt && shasum -a 256 out/fw_prev_066cf57/build/tab5_phone.bin | cut -c1-12 && P1=$(python3 tools/phone_ctl.py port 1) && P2=$(python3 tools/phone_ctl.py port 2) && echo "1번 $P1 / 2번 $P2" && lsof "$P1" "$P2" 2>/dev/null | grep -c usbmodem
```
Expected: `build/tab5_phone.bin: OK` 등 `OK` 3줄, 버전 한 줄, `c86233366b29`(롤백용 보관본이 있다), 포트 경로 두 개, `0`(두 포트를 열고 있는 다른 프로그램 없음). 하나라도 다르면 멈춘다(BLOCKED) — 검사합이 `FAILED` 면 Task 4 Step 3 뒤에 누군가 다시 빌드한 것이다.

Run(단독 호출): `bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh status`
Expected: `relay: listening on port 5094, healthz={"ok": true}`. 꺼져 있으면 켜지 말고 멈춘다(BLOCKED — 컨트롤러가 켠다).

- [x] **Step 2: 올리기 전의 값을 남긴다** (소리 없는 `066cf57` 의 메모리 — Step 10 에서 비교한다)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1:60' 'sleep:3' 'both:until:stat:^STAT .*fsm=IDLE:10' > out/p3a_before.log 2>&1; echo "rc=$?"; for u in 1 2; do grep -E "^\[$u\] STAT " out/p3a_before.log | tail -1 | grep -oE '^\[.\]|heap_int=[0-9]+|heap_int_min=[0-9]+|stk_[a-z]+=[0-9]+' | tr '\n' ' '; echo; done
```
Expected: `rc=0`, 기기마다 한 줄(`[1] heap_int=… heap_int_min=… stk_app=… stk_touch=… stk_relay=… stk_ws=…`). 값을 보고에 적는다. `rc=1` 이면(두 기기가 대기 상태가 아니다 — 사용자가 쓰는 중이거나 교환기에 못 붙음) 멈춘다.

- [x] **Step 3: 2번에 새 펌웨어를 올린다** (export 한 호출)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && source ~/esp/esp-idf-5.5.4/export.sh > /dev/null 2>&1 && PORT=$(python3 tools/phone_ctl.py port 2) && echo "2번 포트: $PORT" && python -m esptool --chip esp32p4 -p "$PORT" read_mac 2>&1 | tee out/p3a_flash_unit2.txt | grep -i "aa:bb:cc:dd:ee:02" && cd build && python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash @flash_args 2>&1 | tee -a ../out/p3a_flash_unit2.txt | grep -E "Hash of data verified|Hard resetting|rror"
```
Expected: `2번 포트: /dev/cu.usbmodem…`, `MAC: aa:bb:cc:dd:ee:02`, `Hash of data verified.` 3줄, `Hard resetting via RTS pin...`. MAC 줄이 안 나오면 아무것도 쓰이지 않는다(명령이 거기서 끝난다) — 멈춘다.

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run 'until:stat:^STAT :60' 'reboot' 'wait:^PHONE BOOT unit=2 id=tab5-2:30' 'wait:^AUDIO (ready|FAIL):20' 'until:stat:^STAT .*fsm=IDLE.*relay=1:120' > out/p3a_boot_unit2.log 2>&1; echo "rc=$?"; grep -E "App version|Compile time|ELF file SHA256|PHONE BOOT|PHONE FAIL|^AUDIO" out/p3a_boot_unit2.log | tail -5; grep -E "I2S_IF: STD: TX|dma frame num|ES8388|Adev_Codec" out/p3a_boot_unit2.log | sed 's/^.*) //' | tail -6; grep "^STAT" out/p3a_boot_unit2.log | tail -1 | grep -oE 'fsm=[A-Z_]+|relay=[01]|audio=[01]|tone=[A-Z]+|aud_us=[0-9/]+|aud_err=[0-9]+|stk_audio=[0-9]+' | tr '\n' ' '; echo
```
Expected: `rc=0`, `App version:      <out/p3a_fw_version.txt 의 버전>`, `Compile time: …`, `ELF file SHA256: …`, `PHONE BOOT unit=2 id=tab5-2`, **`AUDIO ready rate=16000 block=256 vol=100`**, 마지막 줄 `fsm=IDLE relay=1 audio=1 tone=NONE aud_us=0/0 aud_err=0 stk_audio=<1024 이상>`.
가운데의 부품 로그는 참고다(로그 수준에 따라 안 보일 수 있다 — 없어도 실패가 아니다). 보이면 Phase 0 로그(`tab5_phone_probe/out/task4.log` 178~181)와 같은 모양이어야 한다: `I2S_IF: STD: TX, data_bit: 16, slot_bit: 16, ws_width: 16, slot_mode: MONO, slot_mask: 0x1` · `I2S_IF: STD: TX, sample_rate_hz: 16000, mclk_multiple: 256, clk_src: 0` · `ES8388: Start on mode:2` · `Adev_Codec: Open codec device OK`. `dma frame num is adjusted` 줄은 **없어야** 한다(PL3a-D8 — 있으면 보고에 적는다).
**`AUDIO FAIL <단계>` 가 나오거나 `PHONE FAIL` 줄이 있거나 `STAT` 이 없으면 멈춘다(BLOCKED)** — 1번에는 올리지 않는다. 보고에 `<단계>` 와 그 앞뒤 20줄을 붙인다("결함이 나오면" 참조).

- [x] **Step 4: 1번에 같은 바이너리를 올린다** (export 한 호출. 다시 빌드하지 않는다)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && shasum -a 256 -c out/p3a_fw_build.sha256 && source ~/esp/esp-idf-5.5.4/export.sh > /dev/null 2>&1 && PORT=$(python3 tools/phone_ctl.py port 1) && echo "1번 포트: $PORT" && python -m esptool --chip esp32p4 -p "$PORT" read_mac 2>&1 | tee out/p3a_flash_unit1.txt | grep -i "aa:bb:cc:dd:ee:01" && cd build && python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash @flash_args 2>&1 | tee -a ../out/p3a_flash_unit1.txt | grep -E "Hash of data verified|Hard resetting|rror"
```
Expected: `OK` 3줄, `1번 포트: …`, `MAC: aa:bb:cc:dd:ee:01`, `Hash of data verified.` 3줄, `Hard resetting via RTS pin...`.

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py --unit 1 run 'until:stat:^STAT :60' 'reboot' 'wait:^PHONE BOOT unit=1 id=tab5-1:30' 'wait:^AUDIO (ready|FAIL):20' 'until:stat:^STAT .*fsm=IDLE.*relay=1:120' > out/p3a_boot_unit1.log 2>&1; echo "rc=$?"; grep -h "^AUDIO" out/p3a_boot_unit1.log out/p3a_boot_unit2.log; grep -hE "App version|Compile time|ELF file SHA256" out/p3a_boot_unit1.log out/p3a_boot_unit2.log | sed 's/^.*app_init: //' | sort -u
```
Expected: `rc=0`, `AUDIO ready rate=16000 block=256 vol=100` **2줄**(두 기기), 그리고 **정확히 세 줄**(`App version: …`, `Compile time: …`, `ELF file SHA256: …` 가 한 줄씩 = 두 기기의 값이 같다. 네 줄 이상이면 두 기기의 바이너리가 다르다 — 멈춘다). 그 세 줄을 보고에 적는다.

- [x] **Step 5: 시나리오 A·B — 양방향 통화 2건**(걸기 → 벨·연결음 → 받기 → 통화 → 끊기 → 종료음)

A. 1번이 걸고 2번이 받고 1번이 끊는다:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE:60' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '1:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' \
  '2:until:stat:^STAT .*fsm=INCOMING.*tone=BELL:5' '1:until:stat:^STAT .*fsm=RINGBACK.*tone=RINGBACK:5' 'sleep:3' \
  '2:tap:404:428' '2:wait:^FSM INCOMING -> ANSWERING acts=0x82 notice=NONE$:10' '1:wait:^FSM RINGBACK -> IN_CALL acts=0x180 notice=NONE$:10' \
  'both:until:stat:^STAT .*fsm=IN_CALL.*tone=NONE:10' 'sleep:2' \
  '1:tap:644:606' '1:wait:^FSM IN_CALL -> ENDING acts=0x248 notice=NONE$:10' '2:wait:^FSM IN_CALL -> IDLE acts=0x240 notice=CALL_ENDED$:10' \
  'sleep:2' 'both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE:15' > out/p3a_a.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3a_a.log; for u in 1 2; do echo "[$u] $(grep -E "^\[$u\] TONE " out/p3a_a.log | sed 's/^\[.\] TONE //' | paste -sd, -)"; done
```
Expected: `rc=0`, `0`, `[1] RINGBACK on,RINGBACK off,END on,END off`, `[2] BELL on,BELL off,END on,END off`. (벨이 울리는 동안 2번 `tone=BELL`, 1번 `tone=RINGBACK`, 받은 뒤 둘 다 `tone=NONE`.)

B. 2번이 걸고 1번이 받고, 받은 쪽(1번)이 끊는다:
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE:60' \
  '2:tap:640:360' '1:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '2:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' \
  '1:until:stat:^STAT .*fsm=INCOMING.*tone=BELL:5' '2:until:stat:^STAT .*fsm=RINGBACK.*tone=RINGBACK:5' 'sleep:3' \
  '1:tap:404:428' '1:wait:^FSM INCOMING -> ANSWERING acts=0x82 notice=NONE$:10' '2:wait:^FSM RINGBACK -> IN_CALL acts=0x180 notice=NONE$:10' \
  'both:until:stat:^STAT .*fsm=IN_CALL.*tone=NONE:10' 'sleep:2' \
  '1:tap:644:606' '1:wait:^FSM IN_CALL -> ENDING acts=0x248 notice=NONE$:10' '2:wait:^FSM IN_CALL -> IDLE acts=0x240 notice=CALL_ENDED$:10' \
  'sleep:2' 'both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE:15' > out/p3a_b.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3a_b.log; for u in 1 2; do echo "[$u] $(grep -E "^\[$u\] TONE " out/p3a_b.log | sed 's/^\[.\] TONE //' | paste -sd, -)"; done
```
Expected: `rc=0`, `0`, `[1] BELL on,BELL off,END on,END off`, `[2] RINGBACK on,RINGBACK off,END on,END off`.

- [x] **Step 6: 시나리오 C·D — 거절, 걸다가 취소**

C. 1번이 걸고 2번이 그냥 거절한다(거는 쪽만 종료음):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE:60' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '1:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' 'sleep:2' \
  '2:tap:876:428' '2:wait:^FSM INCOMING -> ENDING acts=0x84 notice=NONE$:10' '1:wait:^FSM RINGBACK -> IDLE acts=0x40 notice=REJECTED$:10' \
  'sleep:2' 'both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE:15' > out/p3a_c.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3a_c.log; for u in 1 2; do echo "[$u] $(grep -E "^\[$u\] TONE " out/p3a_c.log | sed 's/^\[.\] TONE //' | paste -sd, -)"; done
```
Expected: `rc=0`, `0`, `[1] RINGBACK on,RINGBACK off,END on,END off`, `[2] BELL on,BELL off`.

D. 1번이 걸다가 취소한다(종료음 없음, 2번은 부재중):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE:60' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '1:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' 'sleep:2' \
  '1:tap:640:604' '1:wait:^FSM RINGBACK -> ENDING acts=0x88 notice=NONE$:10' '2:wait:^FSM INCOMING -> IDLE acts=0x80 notice=MISSED$:10' \
  'sleep:2' 'both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE:15' > out/p3a_d.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3a_d.log; for u in 1 2; do echo "[$u] $(grep -E "^\[$u\] TONE " out/p3a_d.log | sed 's/^\[.\] TONE //' | paste -sd, -)"; done
```
Expected: `rc=0`, `0`, `[1] RINGBACK on,RINGBACK off`, `[2] BELL on,BELL off`.

- [x] **Step 7: 시나리오 E — 30초 무응답**(약 40초. 벨이 30초 동안 계속 울리다가 스스로 그친다)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE:60' \
  '2:tap:640:360' '1:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '2:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' \
  '1:until:stat:^STAT .*fsm=INCOMING.*tone=BELL:5' 'sleep:15' '1:until:stat:^STAT .*fsm=INCOMING.*tone=BELL:5' '2:until:stat:^STAT .*fsm=RINGBACK.*tone=RINGBACK:5' \
  '2:wait:^FSM RINGBACK -> IDLE acts=0x40 notice=NO_ANSWER$:40' '1:wait:^FSM INCOMING -> IDLE acts=0x80 notice=MISSED$:5' \
  'sleep:2' 'both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE:15' > out/p3a_e.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3a_e.log; for u in 1 2; do echo "[$u] $(grep -E "^\[$u\] TONE " out/p3a_e.log | sed 's/^\[.\] TONE //' | paste -sd, -)"; done
```
Expected: `rc=0`, `0`, `[1] BELL on,BELL off`, `[2] RINGBACK on,RINGBACK off,END on,END off`. (15초 뒤에도 1번 `tone=BELL`, 2번 `tone=RINGBACK` — 한 번 시작한 소리가 30초 내내 이어진다.)

- [x] **Step 8: 시나리오 F — 벨 끄기, 그리고 다음 전화는 다시 울린다**(메시지 거절 1건 포함. 약 40초)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE:60' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '1:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' '2:until:stat:^STAT .*fsm=INCOMING.*tone=BELL:5' 'sleep:1' \
  '2:tap:404:624' '2:wait:^FSM INCOMING -> INCOMING acts=0x80 notice=NONE$:5' '2:until:stat:^STAT .*fsm=INCOMING.*tone=NONE:5' 'sleep:5' \
  '2:until:stat:^STAT .*fsm=INCOMING.*tone=NONE:5' '1:until:stat:^STAT .*fsm=RINGBACK.*tone=RINGBACK:5' \
  '2:tap:404:624' 'sleep:1' '2:until:stat:^STAT .*fsm=INCOMING.*tone=NONE:5' \
  '2:tap:404:428' '2:wait:^FSM INCOMING -> ANSWERING acts=0x82 notice=NONE$:10' '1:wait:^FSM RINGBACK -> IN_CALL acts=0x180 notice=NONE$:10' 'both:until:stat:^STAT .*fsm=IN_CALL.*tone=NONE:10' 'sleep:2' \
  '2:tap:644:606' '2:wait:^FSM IN_CALL -> ENDING acts=0x248 notice=NONE$:10' '1:wait:^FSM IN_CALL -> IDLE acts=0x240 notice=CALL_ENDED$:10' 'sleep:2' 'both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE:15' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '1:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' '2:until:stat:^STAT .*fsm=INCOMING.*tone=BELL:5' 'sleep:1' \
  '2:tap:876:624' '2:wait:^SCREEN DECLINE$:5' 'sleep:1' '2:until:stat:^STAT .*fsm=INCOMING screen=DECLINE.*tone=BELL:5' \
  '2:tap:640:168' '2:wait:^FSM INCOMING -> ENDING acts=0x84 notice=NONE msg=2$:10' '1:wait:^FSM RINGBACK -> IDLE acts=0x40 notice=REJECT_MSG msg=2$:10' \
  'sleep:2' 'both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE:15' > out/p3a_f.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3a_f.log; for u in 1 2; do echo "[$u] $(grep -E "^\[$u\] TONE " out/p3a_f.log | sed 's/^\[.\] TONE //' | paste -sd, -)"; done; grep -c '^\[2\] FSM INCOMING -> INCOMING acts=0x80 notice=NONE$' out/p3a_f.log
```
Expected: `rc=0`, `0`, `[1] RINGBACK on,RINGBACK off,END on,END off,RINGBACK on,RINGBACK off,END on,END off`, `[2] BELL on,BELL off,END on,END off,BELL on,BELL off`, 마지막 줄 `1`.
읽는 법: 첫 전화 — 벨 끄기 → `tone=NONE`, **5초 뒤에도 `fsm=INCOMING` 인 채 `tone=NONE`**(끈 벨은 다시 울리지 않는다), 그동안 1번의 연결음은 그대로(`tone=RINGBACK` — 거는 쪽은 모른다). 끈 벨 자리를 또 눌러도 아무 일 없다(벨 끄기 `FSM` 줄이 한 번뿐 = 마지막 줄 `1`). 받을 때는 2번에 `TONE` 줄이 더 생기지 않는다(이미 꺼져 있다 — `BELL off` 가 한 번뿐). 둘째 전화 — **다시 `tone=BELL`**(끈 것이 다음 전화에 남지 않는다), 메시지를 고르는 화면에서도 벨은 계속 울리고(`screen=DECLINE … tone=BELL`), 문구를 누르면 그친다.

- [x] **Step 9: 시나리오 G — 짧은 전화 세 번 연달아**(울리자마자 취소 → 곧바로 다시)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE:60' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '1:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' 'sleep:1' '1:tap:640:604' '2:wait:^FSM INCOMING -> IDLE acts=0x80 notice=MISSED$:10' '1:wait:^FSM ENDING -> IDLE acts=0x0 notice=NONE$:10' 'sleep:1' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '1:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' 'sleep:1' '1:tap:640:604' '2:wait:^FSM INCOMING -> IDLE acts=0x80 notice=MISSED$:10' '1:wait:^FSM ENDING -> IDLE acts=0x0 notice=NONE$:10' 'sleep:1' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '1:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' 'sleep:1' '1:tap:640:604' '2:wait:^FSM INCOMING -> IDLE acts=0x80 notice=MISSED$:10' '1:wait:^FSM ENDING -> IDLE acts=0x0 notice=NONE$:10' \
  'sleep:2' 'both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE:15' > out/p3a_g.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3a_g.log; for u in 1 2; do echo "[$u] $(grep -E "^\[$u\] TONE " out/p3a_g.log | sed 's/^\[.\] TONE //' | paste -sd, -)"; done
```
Expected: `rc=0`, `0`, `[1] RINGBACK on,RINGBACK off,RINGBACK on,RINGBACK off,RINGBACK on,RINGBACK off`, `[2] BELL on,BELL off,BELL on,BELL off,BELL on,BELL off` — 전화마다 `on` 한 번·`off` 한 번, 끝은 조용.

- [x] **Step 10: 터치 대조·소리 줄의 짝·여유·소리가 쓴 CPU와 메모리, 비밀**

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3a-tones/draft && cat out/p3a_a.log out/p3a_b.log out/p3a_c.log out/p3a_d.log out/p3a_e.log out/p3a_f.log out/p3a_g.log > out/p3a_all_pair.log && python3 "$D/verify/touch_audit.py" out/p3a_all_pair.log; echo "TONE 줄 $(grep -cE '^\[.\] TONE ' out/p3a_all_pair.log)개: on $(grep -cE '^\[.\] TONE [A-Z]+ on$' out/p3a_all_pair.log) / off $(grep -cE '^\[.\] TONE [A-Z]+ off$' out/p3a_all_pair.log), AUDIO FAIL $(grep -c 'AUDIO FAIL' out/p3a_all_pair.log)줄"; for u in 1 2; do grep -E "^\[$u\] STAT " out/p3a_g.log | tail -1 | grep -oE '^\[.\]|heap_int=[0-9]+|heap_int_min=[0-9]+|stk_[a-z]+=[0-9]+|audio=[01]|aud_us=[0-9/]+|aud_err=[0-9]+' | tr '\n' ' '; echo; done
```
Expected:
- `== out/p3a_all_pair.log: 주입한 탭 수와 touch 증가가 모두 일치`. 불일치 줄이 있으면 그 시각에 사용자가 화면을 만졌는지 컨트롤러에게 확인을 부탁한다. 재부팅을 건너간 줄에서 `touch` 가 줄어든 것은 재부팅이다 — 불일치로 세지 않는다.
- `TONE 줄 58개: on 29 / off 29, AUDIO FAIL 0줄` (A 8 + B 8 + C 6 + D 4 + E 6 + F 14 + G 12. `on` 과 `off` 의 수가 같다 = 켜진 채 남은 소리가 없다.)
- 기기마다 한 줄: `heap_int_min` ≥ 100000, `stk_app`·`stk_touch`·`stk_relay`·`stk_ws`·**`stk_audio`** ≥ 1024, `audio=1`, `aud_err=0`.
- **소리가 쓴 것**을 보고에 적는다: ① 메모리 = Step 2 의 `heap_int` 에서 지금 `heap_int` 를 뺀 값(어림 10~20KB) ② CPU = `aud_us` 의 평균 ÷ 16000(소리를 내는 동안 출력 태스크가 쓴 몫. 어림 몇 %) — **평균이 1600(10%)을 넘거나 최대가 16000(묶음 하나)을 넘으면 보고에 따로 적는다**(최대는 기록 줄을 찍은 묶음의 `printf` 시간이라 수백~수천 µs 일 수 있다).

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 ~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3a-tones/draft/verify/secret_scan.py out/p3a_all_pair.log out/p3a_boot_unit1.log out/p3a_boot_unit2.log out/p3a_before.log; echo "secret rc=$?"
```
Expected: 로그마다 `wifi_pass=0 relay_url=0 relay_host=0 token=0`, `secret rc=0`. (`rc=2` 는 "값을 못 읽어 검사하지 못함"이다 — 통과가 아니다.)

- [ ] **Step 11: 사용자 귀 확인** — 컨트롤러가 진행한다(실행 담당은 Step 10 까지 보고하고 멈춘다) — **대기**: 2026-10-04 22:26~22:36 기록 창을 한 번 열었으나 사용자가 만지지 않았다(터치 0, `out/p3a_ear.log`). 점검표 10문항의 답은 아직 없다. **[2026-10-05 갱신 — 원문은 그대로]** 10문항 답은 **여전히 없다**(아래 진행 로그 "2026-10-05 — 귀 확인 칸": 2026-10-05 07:59~08:04 귀 확인 창 `out/p3c_ear_tones.log` 에도 사용자 조작 없음, FSM·TONE 줄 0). 그래서 이 칸은 `[ ]` 로 둔다.

컨트롤러는 아래 기록 창을 띄워 두고(10분, 백그라운드), 사용자에게 직접 걸고 받아 보며 점검표에 답해 달라고 한다. 기록 창이 떠 있는 동안에는 다른 `phone_ctl.py` 를 돌리지 않는다.

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*audio=1 tone=NONE:60' 'sleep:600' 'both:until:stat:^STAT :15' > out/p3a_ear.log 2>&1; echo "rc=$?"; grep -cE '^\[.\] TONE ' out/p3a_ear.log; grep -c '!!' out/p3a_ear.log
```

| # | 사용자가 하는 일 | 들려야 하는 것 | 묻는 것 |
|---|---|---|---|
| 1 | 1번에서 통화 버튼 | 2번에서 **벨**(따르르릉 1초, 2초 쉼), 1번에서 **연결음**(뚜— 1초, 2초 쉼) | 벨이 들리는가? 연결음이 들리는가? |
| 2 | 2번에서 "벨 끄기" | 벨이 곧(0.1초 안) 그친다. 1번의 연결음은 계속 난다 | 누르면 멈추는가? 그 뒤로 다시 울리지 않는가? |
| 3 | 2번에서 받기 | 1번의 연결음이 그치고 양쪽이 조용하다(목소리는 아직 없다) | 받으면 멈추는가? |
| 4 | 한쪽에서 끊기 | 양쪽에서 **종료음**(높은 음 → 낮은 음, 0.5초에 한 번) | 종료음이 들리는가? 뜻이 통하는가? |
| 5 | 다시 걸어, 벨을 끄지 않고 받기 | 받는 순간 벨이 그친다 | 받으면 벨이 멈추는가? |
| 6 | 걸고 → 받는 쪽에서 거절 | 받는 쪽 벨이 그치고, 거는 쪽은 연결음이 그친 뒤 종료음 | 맞게 들리는가? |
| 7 | 걸다가 거는 쪽에서 취소 | 양쪽 다 조용히 끝난다(종료음 없음) | 남는 소리가 없는가? |
| 8 | (들은 것을 떠올려) | — | **음량**: 벨이 옆방에서도 들릴 만한가, 너무 크지 않은가? 연결음·종료음은 적당한가? (크다/적당/작다) |
| 9 | 아무것도 안 하고 10초 귀를 대 본다 / 벨이 시작·끝날 때를 듣는다 | 대기 중에는 조용하다 | **잡음**: 소리 시작·끝의 딸깍, 찢어지는 소리, 대기 중 쉬~ 소리가 있는가? |
| 10 | 걸고 → 받는 쪽에서 "메시지로 거절"을 눌러 문구를 고른다 | 고르는 동안 벨이 계속 울린다 | 고르는 동안 울리는 벨이 거슬리는가?(거슬리면 "메시지로 거절"을 누를 때 벨도 끄게 바꾼다) |

답은 Task 6 에서 문서에 적는다. "크다/작다/거슬린다/잡음이 있다"는 답이 나오면 고치는 일은 따로 맡긴다(상수 한두 개 — PL3a-D1·D2·D4 의 탈출구 — 를 바꾸고 Task 4·5 의 빌드·플래시·해당 시나리오를 다시).

**결함이 나오면** (다시 돌려도 같은 `!!` 가 나오거나 `TONE` 요약이 Expected 와 다를 때 — 고치지 않고 진단해서 보고한다)
1. 두 기기의 `FSM`·`TONE` 줄과 `STAT` 의 `tone=` 을 시간순으로 놓고, 어느 기기의 어느 소리가 기대와 다른지 먼저 확정한다. `FSM` 줄은 맞는데 `TONE` 만 다르면 소리 쪽, `FSM` 줄부터 다르면 통화 쪽(Phase 2 회귀)이다.
2. 증상별 1순위로 볼 곳:
   - `AUDIO FAIL i2c`·`codec`: 부팅 로그의 `ES8388`·`I2S_IF`·`Adev_Codec`·`i2c` 줄을 보고에 붙인다. 시험 펌웨어와 주소·속도·핀이 같은지는 Task 2 Step 2 에서 이미 대조했다.
   - `AUDIO ready` 인데 **소리가 전혀 안 난다**(귀): 3a 가 시험 펌웨어와 다른 하드웨어 설정은 "송신 채널만 연 것"(PL3a-D7) 하나다 — 그 결정의 탈출구(시험 펌웨어의 `i2s_setup` 을 그대로)가 1순위 가설이다. 부팅 로그의 `I2S_IF: STD: TX …` 줄을 `task4.log` 178~187 과 대조한다.
   - 소리가 **끊기거나 찢어진다**: `aud_err`(쓰기 실패), `aud_us` 의 최대(16000 을 넘으면 묶음을 제때 못 만든 것), 그 시각의 `stk_audio`.
   - `TONE` 줄의 **짝이 안 맞는다**(`on` ≠ `off`) 또는 순서가 다르다: 맥 시험 `test_tones.cpp` 의 "scenario …" 와 대조하고, 16ms 안에 벨 → 멈춤이 겹친 경우(PL3a-D6 의 비용 — `BELL on` 줄이 통째로 없다)인지 `FSM` 줄의 간격으로 본다.
   - 통화 흐름이 달라졌다(걸리지 않음, 화면이 늦음, 터치 대조 불일치): Step 2 의 `p3a_before.log` 때와 비교하고, `heap_int_min`·`stk_*`·`aud_us` 를 붙인다.
3. 다음 경우는 **멈추고 보고한다(BLOCKED)**: 플래시가 `Hash of data verified.` 없이 끝남 / 기기가 `PHONE BOOT` 를 찍지 않거나 `PHONE FAIL` 을 찍음 / `AUDIO FAIL` / 같은 실행이 세 번 연속 실패 / 터치 대조에서 사람 손이 아닌 불일치가 남음 / `heap_int_min` 이나 `stk_*` 가 기준 아래.
4. 소리 때문에 전화기를 쓸 수 없게 됐다고 판단되면(예: 그치지 않는 소리) 컨트롤러가 "롤백"의 명령으로 두 기기를 `066cf57` 로 되돌린다 — 실행 담당이 스스로 되돌리지 않는다.

---

### Task 6: 기록 — README, 통화 설계서, 이 계획서

**Files:**
- Modify: `firmware/tab5_phone/README.md`, `docs/exec-plans/2026-10-03-tab5-phone.md`(§5 Phase 3 줄, §8 진행 로그, §9.5 모듈 표, 터치 두 건 닫기), `docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md`(§7.4 확인 결과), `docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md`(이 계획서의 체크박스·상태·진행 로그)

**Interfaces:**
- Consumes: Task 1~5 의 보고(커밋 SHA, 시험 개수, 빌드 크기·버전, 시나리오 결과, `heap_int`·`aud_us`, 사용자 귀 확인 답).
- Produces: 갱신된 문서 넷, 3b 계획으로 넘기는 목록(보고에 싣는다).

- [x] **Step 1: README 패치를 적용하고 대조한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3a-tones/draft && git apply "$D/patches/T6-01-readme.patch" && cmp "$D/files/firmware/tab5_phone/README.md" firmware/tab5_phone/README.md && echo "같음: README.md"
```
Expected: `같음: README.md`. 그 뒤, 실제 결과가 패치의 글과 다른 곳만 손으로 고친다: "지금 값" 줄의 시험 개수(계획대로면 그대로), 귀 확인으로 소리 모양·크기를 바꿨다면 "신호음" 절의 표.

````diff
--- a/firmware/tab5_phone/README.md
+++ b/firmware/tab5_phone/README.md
@@ -1,12 +1,13 @@
 # tab5_phone — Tab5 2대 음성 전화기
 
 두 대에 같은 펌웨어를 올린다. 기기 번호는 칩의 MAC 으로 정해진다(1번 `AA:BB:CC:DD:EE:01`, 2번 `AA:BB:CC:DD:EE:02`).
-지금 단계(Phase 2)는 **소리 없이** 걸기 → 벨 화면 → 받기/거절 → 통화 화면 → 끊기까지다. 소리는 Phase 3 에서 붙인다.
-받는 쪽 벨 화면에는 **벨 끄기**(그 전화의 벨만 끈다)와 **메시지로 거절**(문구 3개 중 하나로 거절 → 건 쪽 대기 화면에 "상대: …" 10초)도 있다. 실제 벨소리가 Phase 3 이라, 벨 끄기가 소리를 정말 멈추는지는 그때 확인한다.
+지금 단계(Phase 3a)는 걸기 → 벨 화면 → 받기/거절 → 통화 화면 → 끊기에 **신호음**이 붙었다: 받는 쪽 벨, 거는 쪽 연결음, 통화가 끝났을 때 종료음. **목소리는 아직 없다**(Phase 3b).
+받는 쪽 벨 화면에는 **벨 끄기**(그 전화의 벨소리만 끈다 — 누른 뒤 최대 0.1초 안에 그친다)와 **메시지로 거절**(문구 3개 중 하나로 거절 → 건 쪽 대기 화면에 "상대: …" 10초)도 있다.
 
 - 설계: `docs/exec-plans/2026-10-03-tab5-phone.md` (진행·결과는 §8)
 - 구현 계획: `docs/exec-plans/2026-10-04-tab5-phone-phase2.md`
 - 수신거부 메시지·벨 끄기: 설계 `docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md`, 계획 `…-decline-mute-plan.md`
+- 신호음(Phase 3a): 계획 `docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md` (결정 PL3a-D1~D16)
 - 참고 구현 조사: `docs/exec-plans/2026-10-04-tab5-phone-research/`
 
 ## 구조
@@ -23,6 +24,9 @@
 | `main/ui.h/.cpp` | 통화 화면 그리기 | 기기(스크린샷) |
 | `main/ui_wifi.h/.cpp` | Wi-Fi 설정 화면(목록 + 비밀번호 자판) | 기기 |
 | `main/app.h/.cpp` | 터치·교환기 신호·시간 → 상태 기계 → 할 일 실행 → 다시 그리기 | 기기 |
+| `main/tones.h` | 신호음의 박자·주파수·크기, 딸깍 방지(5ms), 할 일 → 소리, 기록 줄 고르기 | 맥 `tools/test_tones.cpp` |
+| `main/audio_out.h/.cpp` | 스피커 출력(ES8388 + I2S 송신). 공급자에게 PCM 을 받아 낸다 — 무엇을 낼지는 모른다 | 기기(귀) |
+| `main/tone_out.h/.cpp` | 신호음 공급자: 명령 우편함 + `TONE` 기록 줄(로직은 `tones.h`) | 기기 |
 | `main/phone_main.cpp` | 부팅, 태스크 기동, 시리얼 명령 | 기기 |
 | `tools/phone_ctl.py` | 시리얼 검증 도구(기기 번호로 선택, 두 기기 시나리오, 스크린샷, 터치 주입) | `tools/test_phone_ctl.py` |
 | `tools/gen_ui_assets.py` | 고정 문구를 Pretendard 로 구워 `main/ui_assets.h` 를 만든다 | — |
@@ -35,12 +39,12 @@
 
 ```
 cd tools && SDK="$(xcrun --sdk macosx --show-sdk-path)"
-for T in wifi_store call_fsm relay_proto device_id ui_layout; do clang++ -std=c++17 -Wall -Wextra -isysroot "$SDK" -I../main test_$T.cpp -o /tmp/t_$T && /tmp/t_$T | tail -1; done
+for T in wifi_store call_fsm relay_proto device_id ui_layout tones; do clang++ -std=c++17 -Wall -Wextra -isysroot "$SDK" -I../main test_$T.cpp -o /tmp/t_$T && /tmp/t_$T | tail -1; done
 python3 -m pytest -q -p no:cacheprovider test_phone_ctl.py      # ESP-IDF 를 export 하지 않은 셸에서
 cd ~/tab5-toy-phone/server && python3 -m pytest -q -p no:cacheprovider   # 교환기 + 가짜 상대 (위의 cd tools 와 별개로 따로 실행)
 ```
 
-- 지금 값(커밋 `066cf57`): wifi_store 16 · call_fsm 105 · relay_proto 61 · device_id 9 · ui_layout 91, 컴파일 경고 0 / 도구 pytest 152 / 교환기 pytest 137.
+- 지금 값(Phase 3a): wifi_store 16 · call_fsm 105 · relay_proto 61 · device_id 9 · ui_layout 91 · tones 84, 컴파일 경고 0 / 도구 pytest 186 / 교환기 pytest 137.
 - 이 맥의 함정: ① `-isysroot` 가 없으면 링크가 안 된다(SDK 27 과 링커가 안 맞음). ② ESP-IDF 를 export 한 셸의 `python3` 에는 Pillow·pytest 가 없다 → 파이썬 도구·시험은 export 하지 않은 호출에서.
 
 ## 빌드·플래시
@@ -100,7 +104,9 @@
 | `FSM <이전> -> <이후> acts=0x… notice=<이름>` (+ 번호가 있을 때만 ` msg=<1~3>`) | 통화 상태가 바뀜. `acts` 는 상태 기계가 낸 할 일 비트 |
 | `SCREEN <이름>` | 화면이 바뀜. `DECLINE` 은 메시지 고르기 화면 |
 | `RELAY up=<0\|1> code=<0\|401> attempt=<n>` | 교환기 연결 변화. `up=1` 의 `attempt` 는 항상 0 |
-| `STAT up=<초> unit= fsm= screen= wifi= relay= relay_code= peer= missed= touch= heap_int= heap_int_min= heap_psram= stk_app= stk_touch= stk_relay= stk_ws=` | `stat` 의 답. `touch` = 앱 태스크가 처리한 탭 수(실제 + 주입, 재부팅하면 0). 합격선: `heap_int_min` ≥ 100000, `stk_*` ≥ 1024. `stk_ws=0` 은 태스크가 아직 없는 부팅 직후 값 |
+| `STAT up=<초> unit= fsm= screen= wifi= relay= relay_code= peer= missed= touch= heap_int= heap_int_min= heap_psram= stk_app= stk_touch= stk_relay= stk_ws= audio= tone= aud_us= aud_err= stk_audio=` | `stat` 의 답. `touch` = 앱 태스크가 처리한 탭 수(실제 + 주입, 재부팅하면 0). 합격선: `heap_int_min` ≥ 100000, `stk_*` ≥ 1024. `stk_ws=0` 은 태스크가 아직 없는 부팅 직후 값. 끝의 다섯 칸은 소리(아래 "신호음") |
+| `AUDIO ready rate=16000 block=256 vol=100` / `AUDIO FAIL <단계>` | 부팅 때 소리 장치를 연 결과. `FAIL` 이어도 전화기는 소리 없이 계속 돈다(`STAT … audio=0`). 단계: `lock`·`i2c`·`i2s`·`codec`·`amp`·`task`·`tones` |
+| `TONE <BELL\|RINGBACK\|END> on` / `… off` | 신호음이 시작됨 / 그침. 출력 태스크가 찍는다 — 원인이 된 `FSM` 줄과의 앞뒤는 정해져 있지 않다 |
 | `OK t x y` | 주입한 터치가 대기열(8칸)에 들어감. 너무 빨리 보내면 가득 차 버려진다 |
 | `ERR t out-of-range` / `ERR unknown` | 좌표가 화면 밖인 `t` 명령 / 모르는 명령 |
 | `SHOT BEGIN … SHOT END` / `SHOT FAIL private` | 스크린샷. **입력이 든 비밀번호 자판 화면에서는 `SHOT FAIL private`** 로 거절한다(빈 자판·다른 화면은 찍힌다). 도구는 이 응답을 다시 받지 않고 실패로 끝낸다 |
@@ -110,6 +116,12 @@
 - 벨 끄기: `FSM INCOMING -> INCOMING acts=0x80 notice=NONE`. 메시지 거절(받는 쪽): `FSM INCOMING -> ENDING acts=0x84 notice=NONE msg=2`. 거는 쪽: `FSM RINGBACK -> IDLE acts=0x40 notice=REJECT_MSG msg=2`.
 - 그냥 거절만 엄격히 볼 때는 `notice=REJECTED$` 로 쓴다(`REJECT_MSG` 는 `REJECTED` 로 시작하지 않는다).
 
+신호음(Phase 3a)이 더한 것:
+- `STAT` 끝의 칸: `audio=<0|1>`(소리 장치가 열렸나) · `tone=<NONE|BELL|RINGBACK|END>`(지금 내는 신호음 — 박자의 쉼 동안에도 그 이름) · `aud_us=<평균>/<최대>`(소리 한 묶음 16000µs 를 만드는 데 든 시간 µs. 기록 줄을 찍은 묶음이 최댓값이 된다) · `aud_err=<코덱 쓰기 실패 횟수>` · `stk_audio=<출력 태스크 스택 여유>`. 앞의 칸은 그대로다.
+- 한 통화에서 한 기기가 찍는 `TONE` 줄의 순서(맥 시험 `test_tones.cpp` "scenario …" 가 고정): 받는 쪽 `BELL on` → `BELL off`(받기·거절·벨 끄기·부재중 어느 것이든 한 번) → 통화가 끝나면 `END on` → `END off`. 거는 쪽 `RINGBACK on` → `RINGBACK off` → (거절당함·무응답·통화 뒤 끊김이면) `END on` → `END off`. 내가 걸다 취소하면 종료음이 없다.
+- `TONE … off` 는 만든 소리가 0 이 된 때다. 스피커에서 실제로 그치는 것은 앞서 내보낸 만큼(최대 약 0.1초) 늦다.
+- 벨이 울리는지 볼 때는 `until:stat:^STAT .*fsm=INCOMING.*tone=BELL`, 벨 끄기 뒤에는 `…tone=NONE`. `TONE` 줄을 `wait` 로 기다릴 때는 그 앞뒤의 `FSM` 줄과 순서가 바뀔 수 있으니, 끝난 뒤 로그에서 기기별 `TONE` 줄만 뽑아 순서를 본다.
+
 ### 주입 좌표 (`tap:<x>:<y>` — 각 버튼 안의 한 점. 대부분 가운데이고 취소·끊기는 실제 시험에서 쓴 값(가운데가 아님). 정본은 `main/ui_layout.h`)
 
 | 화면 | 버튼 | 좌표 |
@@ -150,6 +162,19 @@
 
 단계: `expect:peer` · `expect:ring` · `expect:ringing` · `expect:connected` · `expect:ended:<사유>` · `send:call|accept|reject|hangup` · `sleep:<초>` · `voice:<프레임수>`(Phase 3 용), 맨 뒤에 `@<초>` 로 기다리는 한도. 수신거부 메시지용으로 `send:reject:<n>`(그 수를 `msg` 로 실어 보냄)과 `expect:ended:reject:<n>`(`0` = 번호가 없어야 함)이 있고, 번호를 쓰지 않으면 `msg` 를 보지 않는다.
 
+## 신호음 (Phase 3a)
+
+| 소리 | 언제 | 모양 | 크기(최고) |
+|---|---|---|---|
+| 벨 `BELL` | 전화가 오는 동안(받는 쪽) | 800·1000Hz 를 50ms 씩 번갈아 1초 → 2초 쉼, 반복 | 8000 |
+| 연결음 `RINGBACK` | 상대 벨이 울리는 동안(거는 쪽) | 440+480Hz 를 함께 1초 → 2초 쉼, 반복 | 5300 |
+| 종료음 `END` | 통화가 끝났을 때 한 번(거절당함·무응답·통화 뒤 끊김. 내가 걸다 취소한 때와 부재중에는 없다) | 660Hz 0.16초 → 쉼 0.08초 → 440Hz 0.28초 | 3700 |
+
+- **바꾸는 법**: 모양은 `main/tones.h` 의 `BELL_SEGS`·`RINGBACK_SEGS`·`END_SEGS`(한 칸 = 주파수 둘·길이 ms), 크기는 같은 파일의 `BELL_AMP`·`RINGBACK_AMP`·`END_AMP`(16비트 표본의 최고값, 코덱 음량 100 기준). 고친 뒤 맥 시험 `test_tones.cpp` 의 박자·크기 시험을 함께 고치고 → 빌드 → 두 기기 다시 플래시. 코덱 음량은 `main/audio_out.cpp` 의 `VOLUME`(지금 100 고정 — 음량 조절 화면은 Phase 5).
+- 소리가 없을 때 앰프는 켜진 채이고 I2S 는 0 을 낸다(켜고 끌 때의 퍽 소리를 피하려는 것 — PL3a-D4). 대기 중 쉬 소리가 거슬리면 그 결정의 탈출구를 본다.
+- 소리 장치가 열리지 않으면(`AUDIO FAIL …`) 전화기는 소리 없이 그대로 동작한다. 다시 시도하지 않는다 — 재부팅하면 다시 연다.
+- 스피커 출력은 송신 채널 하나만 연다(마이크는 Phase 3b). 출력 경로는 "공급자에게 PCM 을 받아 낸다"(`audio_out.h` 의 `add_source`)이고 신호음이 그 첫 공급자다.
+
 ## 문구를 바꾸는 법 (메시지로 거절)
 
 `tools/gen_ui_assets.py` 의 `REJECT_MSGS`(순서 = 번호) → 다시 굽기(`python3 tools/gen_ui_assets.py --preview /tmp/tab5_phone_assets.png`, export 하지 않은 셸에서 — 미리보기 그림으로 글자가 깨지지 않았는지 눈으로 본다) → 빌드 → 두 기기 다시 플래시. 문구 **수**를 바꾸려면 `call_fsm.h` 의 `MSG_COUNT`, `ui_layout.h` 의 `Tap::MSG*`·`BTN_MSG`, `ui.cpp` 의 배열 둘을 함께 고친다(어긋나면 `static_assert` 가 빌드를 막는다). 현재 문구: 1 "회의 중이에요", 2 "지금은 받을 수 없어요", 3 "잠시 후 다시 걸게요".
@@ -160,5 +185,5 @@
 - **2번 → 스냅부스**: `cd ~/tab5-toy-phone/firmware/tab5_snapbooth && source ~/esp/esp-idf-5.5.4/export.sh && PORT=$(python3 ../tab5_phone/tools/phone_ctl.py port 2) && python -m esptool --chip esp32p4 -p "$PORT" read_mac | grep -i "aa:bb:cc:dd:ee:02" && idf.py -p "$PORT" flash`. (Phase 0 끝에 2번을 스냅부스로 복구한 적은 있다 — 통화 설계서 §8 — 그러나 그때 쓴 명령은 기록이 없고, 전화기 펌웨어에서 되돌린 적은 없다.) 안 되면 공장 백업 `backups/tab5-2_factory_aabbccddee02_2026-09-26.bin` 을 아래 1번과 같은 형태로 `write_flash 0x0` (저장된 Wi-Fi 목록은 사라진다).
 - **1번 → 사진 액자**: `cd ~/tab5-toy-phone/firmware/tab5_phone && source ~/esp/esp-idf-5.5.4/export.sh && PORT=$(python3 tools/phone_ctl.py port 1) && python -m esptool --chip esp32p4 -p "$PORT" read_mac | grep -i "aa:bb:cc:dd:ee:01" && python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash 0x0 ../../backups/tab5-1_photoframe_aabbccddee01_2026-10-04.bin` (`-b 460800` 은 이 프로젝트에서 플래시에 쓴 값이다. 백업을 뜰 때 쓴 `-b 921600` 은 읽기에만 썼고 쓰기에는 써 본 적이 없다. 16MB 전체를 쓰므로 시간이 오래 걸린다). 맥북 `backups/` 에 3개가 있다 — `…_2026-10-04.bin`(원본), `…_b2.bin`, `…_b3.bin`. 서로 NVS 구역(0x9000~0xe000)만 다르다(사진 액자 펌웨어가 부팅마다 NVS 에 부팅 횟수를 쓰기 때문). b3 는 한 세션에서 읽고 바로 verify 한 것이다. 원본과 b3 는 맥미니에도 있다.
 - **1번 → 공장 상태**: 공장 백업 `tab5-1_factory_aabbccddee01_2026-09-22.bin`(16,777,216 B, 검사합 OK)은 **맥미니** `~/tab5-toy-phone/backups/` 에만 있다(맥북에는 없다). 맥북으로 가져와 위 "1번 → 사진 액자"와 같은 명령의 파일 이름만 바꿔 쓴다. `backups/` 는 git 제외라 devsync 로 오지 않는다.
-- **지금 기기에 올라간 빌드와 보관본**: 지금 두 기기에는 `066cf57` 빌드가 올라가 있다(`out/fx_*`). 맥북에만 있는(git 제외) 보관본: `out/fw_prev_113097c/build/` = 이번 수정 직전(`113097c`, 새 기능까지 들어 있고 최종 검수 수정 `99466b9`·`a26d237`·`066cf57` 만 없는 빌드), `out/fw_prev/` = 새 기능 전(`a909f7d`). 보관본으로 되돌릴 때는 위 "빌드·플래시"와 같은 명령에서 `cd build` 를 `cd out/fw_prev`(또는 `cd out/fw_prev_113097c/build`)로 바꾼다 — 이것도 실행해 본 적 없다. 소스에서 되돌릴 때: **판단 헤더(`618eebf`)·글자 자산(`5971d4c`)·화면·진행(`113097c`) 세 커밋은 함께** 되돌린다 — 중간 상태는 `-Werror=switch` 로 빌드되지 않는다. 교환기(`6ff6108`)와 가짜 상대 도구(`ff90e5b`)는 따로 되돌려도 된다.
+- **지금 기기에 올라간 빌드와 보관본**: 지금 두 기기에는 Phase 3a(신호음) 빌드가 올라가 있다(버전은 `out/p3a_fw_version.txt`, 검사합 `out/p3a_fw_build.sha256`). 맥북에만 있는(git 제외) 보관본: `out/fw_prev_066cf57/build/` = 신호음 직전(`066cf57`, 소리 없는 Phase 2 최종 빌드 — 되돌리는 명령은 신호음 계획서 "롤백"), `out/fw_prev_113097c/build/` = Phase 2 최종 검수 수정 직전(`113097c`, 수신거부 메시지·벨 끄기까지 들어 있고 최종 검수 수정 `99466b9`·`a26d237`·`066cf57` 만 없는 빌드), `out/fw_prev/` = 새 기능 전(`a909f7d`). 보관본으로 되돌릴 때는 위 "빌드·플래시"와 같은 명령에서 `cd build` 를 `cd out/fw_prev_066cf57/build`(또는 `cd out/fw_prev_113097c/build`·`cd out/fw_prev`)로 바꾼다 — 이것도 실행해 본 적 없다. 소스에서 되돌릴 때: **판단 헤더(`618eebf`)·글자 자산(`5971d4c`)·화면·진행(`113097c`) 세 커밋은 함께** 되돌린다 — 중간 상태는 `-Werror=switch` 로 빌드되지 않는다. 교환기(`6ff6108`)와 가짜 상대 도구(`ff90e5b`)는 따로 되돌려도 된다.
 - **백업을 새로 뜰 때(사진 액자 상태 등)**: 읽기와 `verify_flash` 를 **한 esptool 세션에서** 한다 — 읽기 전 `read_mac` 은 `--after no_reset`, `read_flash` 는 `--before no_reset --after no_reset_stub`, 이어서 `verify_flash` 는 `--before no_reset` 으로, 중간에 칩이 재시작되지 않게. 호출을 나누면 사진 액자가 부팅하며 NVS 를 고쳐 쓰기 때문에 항상 어긋난다.
````

- [x] **Step 2: 통화 설계서를 갱신한다** — 먼저 터치 두 건을 닫는 패치를 적용하고, 이어서 결과를 적는다

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3a-tones/draft && git apply "$D/patches/T6-02-design-touch.patch" && grep -c '사용자가 "눌렀음"이라고 답했다(2026-10-04 21:1x)' docs/exec-plans/2026-10-03-tab5-phone.md && grep -c "사용자 확인 대기" docs/exec-plans/2026-10-03-tab5-phone.md
```
Expected: `1`, `0`. (Phase 2 문서에 "사용자 확인 대기"로 남아 있던 터치 두 건 — 13:59:43 전후, 15:06~15:31 — 을 사용자의 답 "눌렀음"(2026-10-04 21:1x)으로 닫는다. 패치가 문맥 불일치로 적용되지 않으면 아래 블록의 `-` 줄을 `+` 줄로 직접 바꾼다.)

```diff
--- a/docs/exec-plans/2026-10-03-tab5-phone.md
+++ b/docs/exec-plans/2026-10-03-tab5-phone.md
@@ -323,7 +323,7 @@
 - **수정 묶음(fix wave 1)**: `99466b9`(도구가 `SHOT FAIL private` 는 다시 받지 않고 실패) · `a26d237`(입력이 든 비밀번호 자판 화면은 `shot` 이 `SHOT FAIL private` 로 거절, 빈 자판·다른 화면은 찍힘) · `066cf57`(화면 초기화 실패 시 교환기 미접속, `esp_websocket_client_start()` 실패 5초마다 재시도, `CONFIG_LOG_DYNAMIC_LEVEL_CONTROL` 고정 + `#error`, `RELAY_URL` 길이 `static_assert`, `RELAY up=1` 의 `attempt=0`). 시험: 맥 16/105/61/9/91, 도구 pytest 152, 교환기 137. 빌드 `066cf57` `tab5_phone.bin` 0x231650(2,299,472 B), 경고 0. 이로써 이전 판정 "촬영 가드는 넣지 않고 절차로 막는다"가 뒤집혔다. 두 기기 재플래시·재회귀(2번 → 1번, 같은 바이너리 `App version 066cf57` · `Compile time Oct  4 2026 15:56:52` · ELF `d49244c85...`, 16:12~16:23)는 **모두 통과**: 새 기능 Task 6 Step 6~9(시나리오 A·B·C·D·E·F·G·I) 다시 통과·다시 돌린 실행 0회·사진 8장은 `113097c` 때와 바이트 동일 / 자판 화면 `shot` — 글자 1개부터 `SHOT FAIL private`, 지우면 다시 찍힘, 연속 5회 거절 / 교환기 `stop` 직후 걸면 5초 뒤 `DIALING -> OFFLINE acts=0x480 notice=RELAY_SILENT`, 교환기를 켜면 복귀(`RELAY up=1` 6줄 모두 `attempt=0`) / 설정 화면(목록·입력이 든 자판)이 열린 채 전화가 오면 벨 화면으로 바뀌고 `shot` 이 찍힘. 터치 대조는 도구 한계로 원본 출력에 허위 불일치 2건이 났으나 보정해 다시 세면 설명 안 되는 터치 0(약 11분 × 2대). 끝 STAT `heap_int_min` 1번 262539 · 2번 263187, `stk_touch` 3268·3284 — 113097c 때보다 낮은 것은 Wi-Fi 목록·자판 화면을 처음 연 순간의 일회성 계단으로 판독됐고(같은 동작을 반복해도 더 내려가지 않음) 기준 100000 의 2.6배다. 보고 `fix-2-report.md`, 독립 검수 `fix-2-review.md` Approved
 - **기기에서 드러나 고친 것**: 펌웨어·교환기 코드 결함으로 고친 것은 **없다**(Task 7·8·새 기능 Task 6 모두 코드 수정 0). 드러난 것은 절차·도구·환경이었다 — ① 교환기 개발 스크립트 변수 이름(`BIND`·`PORT` readonly)이 환경 지정을 무시해 127.0.0.1 에만 열리던 것(고침) ② 손이 닿은 시험 두 개를 깨끗하게 다시 돌림(검증 누락) ③ 재연결 반복문이 낡은 `relay=1` 에 속아 35회 중 5회가 가짜 통과(판정을 새 `online` 줄로 강화) ④ 통화 중 교환기 종료의 기대 패턴이 닫는 순서에 의존(순서 무관으로 정정) ⑤ 사진 액자 백업 verify 가 읽기/verify 분리 때문에 어긋남(한 세션으로 정정) ⑥ 1번이 지금 장소의 Wi-Fi 를 몰라 사용자가 화면에서 직접 연결(새 장소마다 같은 일 — README) ⑦ 비밀번호 화면 스크린샷 1회(→ 최종 검수 I-1 → 수정 묶음).
 - **부품 판(실제로 잡힌 것)**: esp_websocket_client 1.8.0, esp_hosted 2.11.7, esp_wifi_remote 1.6.5, m5unified 0.2.23, m5gfx 0.2.30, esp_codec_dev 1.6.2, esp-sr 2.5.5, esp_audio_codec 2.5.0 (딸려 온 것: cjson 1.7.19~2, dl_fft 0.8.0, eppp_link 1.1.6, esp-dl 3.3.13, esp_new_jpeg 1.0.2, esp_serial_slave_link 1.1.2, wifi_remote_over_eppp 0.3.3). 두 기기의 C6 코프로세서 경고(`Version mismatch: Host [2.11.0] > Co-proc [0.0.0]`)는 스냅부스와 같은 동작이고 C6 펌웨어는 교체하지 않았다.
-- **터치 관련 사실**: 무접촉을 요청한 구간·사용자 부재 구간에서 주입한 탭 수와 `touch=` 증가가 모두 일치했다(유령 터치 0) — Task 8 약 42 기기·분(그중 무접촉 요청 구간 약 27 기기·분), Task 9 12분 대기 24 기기·분, 새 기능 Task 6 주입 80 = 증가 80, 15:31~15:39 무접촉 16 기기·분. 설명 안 된 터치는 사용자가 있던 시간대뿐이다: 오전 구간(Task 7)은 사용자가 "내가 누른 것으로 하자"고 답했고, **13:59:43 전후 두 기기 각 1회와 15:06~15:31 사이(1번 +2·2번 +5, 그 사이 자동 시험이 없었음)는 사용자 확인 대기** — 게이트 보고에서 묻는다. 손 조작 창(20:54~21:04)의 주입·증가 대조와 한 번 누름이 두 번 동작한 흔적 없음은 위 결과표 "손 조작" 행. 16:34~20:54 에는 교환기가 꺼져 있어 기기가 "교환기에 연결할 수 없음" 화면이었고(통화 버튼 없음), 그 사이 2번 `touch=` 33→37 은 사용자가 돌아와 그 화면을 누른 것이다. 교환기는 20:54 에 사용자 요청으로 다시 켰고 사용자 쪽에서 계속 쓰도록 켜 둔다(문서의 교환기 종료 안내는 시험이 끝날 때의 지시로 그대로 유효). 위 "사용자 확인 대기" 두 질문(13:59:43 전후, 15:06~15:31)은 아직 답이 없다.
+- **터치 관련 사실**: 무접촉을 요청한 구간·사용자 부재 구간에서 주입한 탭 수와 `touch=` 증가가 모두 일치했다(유령 터치 0) — Task 8 약 42 기기·분(그중 무접촉 요청 구간 약 27 기기·분), Task 9 12분 대기 24 기기·분, 새 기능 Task 6 주입 80 = 증가 80, 15:31~15:39 무접촉 16 기기·분. 설명 안 된 터치는 사용자가 있던 시간대뿐이다: 오전 구간(Task 7)은 사용자가 "내가 누른 것으로 하자"고 답했고, 13:59:43 전후 두 기기 각 1회와 15:06~15:31 사이(1번 +2·2번 +5, 그 사이 자동 시험이 없었음)도 **사용자가 "눌렀음"이라고 답했다(2026-10-04 21:1x)** — 설명 안 된 터치는 남지 않는다. 손 조작 창(20:54~21:04)의 주입·증가 대조와 한 번 누름이 두 번 동작한 흔적 없음은 위 결과표 "손 조작" 행. 16:34~20:54 에는 교환기가 꺼져 있어 기기가 "교환기에 연결할 수 없음" 화면이었고(통화 버튼 없음), 그 사이 2번 `touch=` 33→37 은 사용자가 돌아와 그 화면을 누른 것이다. 교환기는 20:54 에 사용자 요청으로 다시 켰고 사용자 쪽에서 계속 쓰도록 켜 둔다(문서의 교환기 종료 안내는 시험이 끝날 때의 지시로 그대로 유효). 위 두 질문(13:59:43 전후, 15:06~15:31)은 사용자의 답("눌렀음", 2026-10-04 21:1x)으로 닫았다.
 - **Phase 3 으로 넘기는 것**
   - **Phase 3 첫 과제**: ⓪ 재검수가 남긴 Minor 둘 — N-1(`relay_link.cpp` 에서 stop 앞에서만 IDLE 로 내려 stop 도중 들어온 값이 남을 수 있음 — 표시만 틀림), N-2(결정 번호 G-D3 중복 — 주석) / ① `relay_link` 의 `reconnect()` stop/start 와 보내기 사이 잠금(**음성 송신을 붙이기 전 필수**) ② 터치 시각 기준을 "처리한 때"가 아니라 "누른 때"로(검수 M-5) ③ `app` 의 판단(hit+guard, `same()`, 안내 시간)을 순수 구조체로 옮겨 맥에서 시험(M-7) ④ `LINK_UP` 비OFFLINE 가지 시험 추가 ⑤ I2C 잠금을 `app::start` 밖으로(화면 초기화가 실패하면 잠금이 없음) ⑥ 신호 보내기가 화면 태스크를 최대 2초 멈추는 영향 측정(RL-D6) ⑦ 새 기능 설계서 §7.4 의 확인 다섯 가지(벨 끄기가 실제 소리를 멈추는지 등)와 §5.3 의 약속(소리 모듈은 `A_TONE_STOP` 뒤 스스로 다시 울리지 않는다, 이미 멈춘 뒤의 `A_TONE_STOP` 은 무해).
   - **Phase 4(맥미니·터널) 전**: ① 교환기 `phone_relay.py` 의 `on_shutdown` 에서 WebSocket 을 닫기 — 지금은 교환기를 **정상 종료(`phone_relay_dev.sh stop`)** 하면 기기가 15~20초 뒤에야 안다(기기 `ping_interval_sec=5` + `pingpong_timeout_sec=15`, 교환기가 종료 때 WebSocket 을 닫지 않음). 프로세스가 죽어 연결이 끊기면(FIN) 부품이 바로 안다 — 코드로만 확인했고 기기에서 재지는 않았다. 안: 교환기 종료 훅에서 닫기 / 기기 pingpong 시간 줄이기(Wi-Fi 순간 끊김 오탐과 맞바꿈) — **결함으로 보지 않았고 배포 전에 정할 항목** ② 개발 중 평문 `ws://` 로 LAN 을 지난 토큰을 새로 만들기 ③ `wss://` 에서 `stk_ws` 재측정 ④ 24시간 대기 기록(메모리 추세·`touch=` 불변) ⑤ 401 뒤 평범한 망 오류에도 "기기 등록 오류"가 남는 것 ⑥ 개발용 교환기 끄기와 허용 규칙(`~/.claude/settings.json` 의 `Bash(bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh:*)`) 지우기.
```

같은 문서에 손으로 더한다(**실제 값으로** — 못 잰 것은 "확인 못 함 — 이유"):
1. 머리의 `- **상태**:` 아래 "갱신" 줄 하나: Phase 3 을 3a(신호음)/3b(목소리)로 나눴고 3a 가 어디까지 됐는지.
2. §5 의 `- [ ] **Phase 3 — 소리 경로**` 줄 아래에 들여 쓴 줄: "3a 신호음(벨·연결음·종료음): 구현·기기 검증·사용자 귀 확인 완료(날짜) — 계획 `2026-10-04-tab5-phone-phase3a-tones.md`. 3b 목소리: 계획 전". Phase 3 의 체크박스는 3b 까지 끝나야 `[x]` 다 — 그대로 둔다.
3. §9.5 모듈 표에 정정 한 줄(표를 지우지 않고 아래에 덧붙인다): "`board_audio` 는 3a 에서 출력만 가진 `audio_out`(ES8388 + I2S 송신, 공급자에게 PCM 을 받아 냄)으로 시작했고 마이크·이어폰은 3b·Phase 5 에서 더한다. `tones` 는 순수 헤더 `tones.h`(맥 시험)와 기기 연결 `tone_out` 으로 나뉘었다."
4. §8 진행 로그 끝에 `### <날짜> — Phase 3a 신호음 결과 (계획 docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md)` 절:
   - 커밋 넷(tones.h / 출력 모듈 / 정리 / 연결)의 짧은 SHA, 시험 개수(맥 16·105·61·9·91·84, 도구 186, 교환기 137)와 실제 값이 다르면 그 값.
   - 빌드: 크기, 우리 코드 경고 수, 버전, 두 기기의 `App version`·`Compile time`·`ELF file SHA256` 이 같았는지, `AUDIO ready` 두 줄.
   - 시나리오 A~G 각각의 결과와 다시 돌린 횟수·이유, `TONE` 줄 수(58 = on 29 / off 29).
   - 소리가 쓴 것: `heap_int` 전 → 후, `heap_int_min`, `stk_audio`, `aud_us` 평균/최대, 터치 대조·비밀 결과.
   - **사용자 귀 확인 10문항의 답**(그대로), 그에 따라 바꾼 것이 있으면 무엇을.
   - 설계서와 다르게 한 것: 이 계획서 진행 로그의 "설계서와 다르게 한 곳"을 가리킨다.
   - "Phase 3 으로 넘기는 것" 가운데 3a 가 처리한 것 — ⓪ N-1·N-2, ⑤ I2C 잠금, ⑦ 수신거부 §7.4·§5.3 — 에 "(3a 에서 처리)"를 붙이고, 나머지(①~④·⑥)는 3b 로 남는다고 적는다.

- [x] **Step 3: 수신거부 설계서 §7.4 에 확인 결과를 적는다**

`docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md` 의 "### 7.4 Phase 3(소리)로 넘기는 확인" 문단 아래에 결과 다섯 줄을 덧붙인다(원문은 지우지 않는다): ① 벨 끄기를 누르면 실제 소리가 멈추고 그 통화가 끝날 때까지 다시 울리지 않는가 — 시나리오 F + 귀 확인 2번 ② 다음 전화는 다시 울리는가 — 시나리오 F ③ 벨을 끈 통화가 부재중이 될 때 소리 없이 끝나는가 — 맥 시험 "muted call becomes missed: silent, no end tone"(기기 시나리오에는 없다) ④ 이미 멈춘 뒤의 `A_TONE_STOP` 이 무해한가 — 맥 시험 + 시나리오 F(받을 때 `TONE` 줄이 더 생기지 않음) ⑤ 고르는 동안 벨이 계속 울리는 것이 거슬리는가 — 귀 확인 10번의 답.

- [x] **Step 4: 이 계획서의 체크박스·상태·진행 로그를 갱신하고 커밋한다**

진행 로그에 `### <날짜> — 실행 결과 (Task 1~6)` 절을 더한다(커밋, Expected 와 달랐던 곳, 다시 돌린 실행, 귀 확인 요약).

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/README.md docs/exec-plans/2026-10-03-tab5-phone.md docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md && git commit -m "docs(phone): Phase 3a 신호음 구현·기기 검증·귀 확인 결과 기록, Phase 2 터치 두 건 닫음" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [x] **Step 5: 3b 계획으로 넘기는 목록을 보고에 그대로 싣는다**

1. 출력 경로는 `audio_out` 의 둘째 공급자로 얹는다(`add_source`). 둘을 합치는 코드(더하고 넘치면 자름, `audio_out.cpp` `out_task`)는 3a 에서 한 번도 돌지 않았다 — 3b 에서 맥 시험이 닿는 곳으로 옮긴다.
2. 마이크를 붙일 때 I2S 를 송수신 한 쌍으로 다시 만든다(시험 펌웨어 `board_audio.cpp` 의 `i2s_setup`·`init`) — 그때 송신 슬롯이 32비트로 바뀐다(Phase 0 와 같은 동작). 출력 경로를 다시 확인한다(PL3a-D7). 출력 장치만 따로 닫지 않는다(조사 기록의 운영 규칙).
3. 코덱 음량은 100 고정이다 — 목소리 크기를 어디서 정할지(디지털/코덱)와 신호음 크기의 관계(PL3a-D2).
4. (정정 2026-10-04 밤: 원문의 "신호음은 이미 멈춰 있다"는 틀렸다 — 거는 쪽은 `A_VOICE_START`+`A_TONE_STOP` 이 한 묶음(`acts=0x180`)이고 스피커는 최대 약 96ms 연결음을 더 낸다. 통화 설계서 §8 3b 목록 5번 참조.) 통화가 끝날 때 종료음(`A_TONE_END`)과 목소리 끝이 겹친다 — 둘째 공급자가 멈추는 순서.
5. Phase 2 가 넘긴 것 가운데 남은 것: `relay_link` 의 `reconnect()` 와 보내기 사이 잠금(**음성 송신 전 필수**), 터치 시각 기준(M-5), `app` 판단의 순수 구조체화(M-7), `LINK_UP` 가지 시험, RL-D6 영향 측정, M-1.
6. 귀 확인에서 나온 것(음량·잡음·벨 모양에 대한 답)과 그에 따라 바꾼 상수.
7. 맥북의 프로젝트 메모리(전화기 진행 상태)에 한 줄 — 컨트롤러 몫.

---

## 검증 기준 (이 계획의 완료 조건)

- [x] 맥 단위 시험: 기존 다섯 개가 **수정 없이** 통과하고(16·105·61·9·91) 새 `tones` 가 `84 ok, 0 failed`, 컴파일 경고 0. → Task 1 Step 2, Task 3 Step 5
- [x] 검증 도구 pytest `186 passed`(기존 152 를 고치지 않고 + 34), 교환기 pytest `137 passed`(그대로). → Task 3 Step 1·5
- [x] 펌웨어 전체 빌드 성공, 우리 코드 경고 0, `dependencies.lock` 그대로, 앱 파티션 62% 여유. → Task 4 Step 2
- [x] 지금 기기의 빌드(`066cf57`, 검사합 `c86233366b29…`)가 `out/fw_prev_066cf57/build/` 에 보관됐다. → Task 4 Step 1
- [x] 두 기기에 같은 바이너리, 둘 다 `AUDIO ready rate=16000 block=256 vol=100`. → Task 5 Step 3·4
- [x] 기기 시나리오 A~G 통과: `rc=0`·`!!` 0, 기기별 `TONE` 줄의 순서가 Expected 와 같다, `TONE` 줄 58개(on 29 = off 29). → Task 5 Step 5~10
- [x] 벨 끄기: 누른 뒤 `tone=NONE`, 5초 뒤에도 `fsm=INCOMING` 인 채 `tone=NONE`, 다음 전화는 `tone=BELL`. → Task 5 Step 8
- [x] 주입한 탭 수와 `touch=` 증가가 일치, 비밀 0건, `heap_int_min` ≥ 100000, `stk_*`(새 `stk_audio` 포함) ≥ 1024, `aud_err=0`. 소리가 쓴 메모리·CPU 를 적었다. → Task 5 Step 10
- [ ] **사용자 귀 확인 10문항**에 답을 받았다(벨이 들린다 / 벨 끄기로 멈춘다 / 받으면 멈춘다 / 연결음 / 종료음 / 음량 / 잡음). → Task 5 Step 11 **대기 — 답 없음(사용자 미수행).** 답이 오면 이 칸을 `[x]` 로 하고 진행 로그와 README "신호음" 절을 고친다. **[2026-10-05 갱신]** 10문항 답은 받지 못했다(미답). 다만 "스피커에서 실제로 소리가 난다"는 3b 빌드(송수신 설정)에서 기록과 사용자 통화로 확인됐다 — 3a 의 송신 전용 설정으로 소리가 났는지는 끝내 확인하지 않았다. 그래서 `[ ]` 유지.
- [x] `git status --short` 에 `out/`·`build/`·`secrets.h` 가 없다.
- [x] README·통화 설계서(§5·§8·§9.5, 터치 두 건 닫음)·수신거부 설계서 §7.4·이 계획서가 갱신됐고, 3b 로 넘기는 목록이 컨트롤러에게 갔다. → Task 6
- 이 계획 밖: 목소리 통화(3b), 독립 검수(컨트롤러가 과제 묶음마다 맡긴다), 통화 설계서 §6 의 검증 기준(서로 다른 망·지연·하울링 — Phase 4 이후).

## 초안 목록

패치는 저장소 루트 기준 경로의 통합 diff 다. 파일 하나는 패치 하나에만 들어 있다(패치 = 그 파일의 지금 → 최종). `D` = `~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3a-tones/draft`. 패치를 모두 적용한 뒤의 파일 전문은 `D/files/<저장소 경로>`(17개)에 있다. `verify/` 의 두 도구는 Phase 2 에서 쓰던 것과 바이트까지 같다(본문에는 싣지 않았다 — 코드는 앞 계획서 `2026-10-04-tab5-phone-decline-mute-plan.md` "초안 목록"과 `.superpowers/sdd/2026-10-04-tab5-phone-phase2-3/carryover-from-phase2/`).

| 초안 파일(`draft/` 기준) | 줄 수 | sha256 앞 12자 |
|---|---|---|
| `patches/T1-01-tones-tests.patch` | 442 | `1b8f85fb4141` |
| `patches/T1-02-tones.patch` | 195 | `10d5bd9678f5` |
| `patches/T2-01-audio-out.patch` | 266 | `641d4b58c4d9` |
| `patches/T2-02-tone-out.patch` | 74 | `c3e94f2aa1d9` |
| `patches/T2-03-cmake.patch` | 11 | `a45ca9d130eb` |
| `patches/T3-01-phone-ctl-tests.patch` | 110 | `bac218f7034f` |
| `patches/T3-02-relay-link-n1.patch` | 20 | `9c3791296045` |
| `patches/T3-03-gfx-n2.patch` | 69 | `5630b864c61a` |
| `patches/T3-04-app.patch` | 101 | `f4e34277ec39` |
| `patches/T3-05-phone-main.patch` | 56 | `0568eeb04c2b` |
| `patches/T6-01-readme.patch` | 94 | `a1557abc8feb` |
| `patches/T6-02-design-touch.patch` | 11 | `12b6bb49c092` |
| `verify/touch_audit.py` | 38 | `fc39218464c8` |
| `verify/secret_scan.py` | 79 | `8350672ef27a` |

같은 작업 폴더(`D` 의 한 칸 위)에 계획을 만들고 검증한 도구가 있다(실행 담당은 쓰지 않는다): `make_draft.py`(사본 → 패치), `make_plan.py`(뼈대 + 패치 → 이 문서), `verify_sequence.sh`(깨끗한 사본에 과제 순서대로 적용·시험·빌드), `mutate.py`(구현을 일부러 망가뜨려 시험이 잡는지), `dryrun/`(Task 1~4 의 명령을 임시 저장소에서, Task 5 의 명령을 가짜 전화기 두 대에 글자 그대로 돌려 본 도구와 무작위 시험), `logs/`(그 기록).

## 진행 로그

### 2026-10-04 — 계획 작성

- 사용자 지시("가자 가자! 속도를 내자!")로 Phase 3 을 3a(신호음)/3b(목소리)로 나누고 3a 만 계획했다. 저장소의 코드·기기·시리얼·교환기·`build/`·`out/`·`secrets.h` 는 건드리지 않았다(이 계획서 파일 하나만 썼다).
- 먼저 대조한 것(사용자 지시 "참고 구현 대조 우선"): 이 기기에서 소리를 낸 Phase 0 시험 펌웨어(`tab5_phone_probe/main/board_audio.cpp`·`probe_main.cpp`)와 그 부팅 로그(`out/task4.log`), 원본 xiaozhi-esp32 `tab5_audio_codec.cc`, 실제로 빌드에 잡히는 `esp_codec_dev` 1.6.2 소스(`audio_codec_data_i2s.c`·`esp_codec_dev.c`·`es8388.c`), ESP-IDF 5.5.4 의 I2S 드라이버(`i2s_common.c` — DMA 칸 맞춤, 쓰기 위치, 보낸 칸 지우기)와 USB 직렬 드라이버(읽는 쪽이 없을 때 쓰기가 최대 50ms 만 기다림), M5GFX 의 Tab5 터치 버스.
- 검증(저장소 밖 사본, 저장소 `61860f1` 기준. 기록은 작업 폴더 `logs/`):
  - 깨끗한 사본(`git archive HEAD`)에 패치 12개를 과제 순서대로 `git apply` 하며 실패 → 통과를 실제로 봤다. 실패 문구는 Task 1 Step 1 의 Expected 그대로다. 최종 파일은 `D/files/` 와 17개 모두 같았다. 패치 12개 모두 지금 저장소에서 `git apply --check` 통과.
  - 맥 단위 시험 16·105·61·9·91 **유지** + 새 `tones` **84**(경고 0), 검증 도구 pytest 152 → **186**, 교환기 pytest **137** 유지. 기존 시험은 한 줄도 고치지 않았다(시험 패치의 삭제 줄 0).
  - 새 시험이 실제로 잡아내는지 `tones.h` 를 일부러 망가뜨려 확인(22가지 — 멈춤·바꿈 때 줄이지 않고 자름, 칸 끝에서 줄이지 않음, 페이드 1표본, 울리는 중의 멈춤 무시, 멈춘 뒤 스스로 다시 시작, 줄인 뒤 다음 소리를 버림, 벨이 한 번만 울림, 종료음이 되풀이됨, 주파수·쉼·크기 바꿈, 두 음을 그냥 더함, 사인 표 크기 절반, 끝난 소리를 한 표본 늦게 앎, 할 일의 우선순위·종료음·벨/연결음 바꿔 읽기, 다시 시작을 기록 줄로 알리지 않음, 낼 것이 없어도 소리가 있다고 함, 명령을 한 묶음 늦게 넣음·버림, 위상을 되돌리지 않음): **전부 1개 이상 실패**했다. 따로, 무작위 명령 5만 7천 개(40만 묶음 = 약 1.8시간 분량)로 `Player` 를 흔들어도 크기는 8000 을 넘지 않고, 이웃 표본 차이는 상한 안이고(딸깍 없음), 멈춤 뒤 5ms 가 지나면 0 이고, `on`·`off` 줄의 수가 같았다(`dryrun/fuzz.cpp`).
  - 펌웨어 전체 빌드(자리 값 `secrets.h`)를 기준선과 커밋 경계 세 곳에서: 기준선 0x231280 → Task 2 뒤 0x231280(그대로 — 새 파일은 컴파일되지만 아직 부르는 곳이 없다) → 정리 뒤 0x231280 → 연결 뒤 **0x242230**(+69KB, 앱 파티션의 62% 여유), 모두 우리 코드 경고 0, `dependencies.lock` 은 저장소 것과 바이트까지 같다. `app` 패치만 적용한 중간 상태는 빌드 실패함도 확인했다(그래서 한 커밋 — PL3a-D15).
  - Task 1~4 의 명령(bash 블록 18개)은 임시 저장소(`git archive HEAD` + `git init`)에서 **계획서의 글자 그대로**, 실행 담당과 같은 셸(Bash 도구의 zsh + ugrep)로 차례로 돌려 Expected 와 같음을 확인했다 — 커밋 넷, 지금 `build/` 의 보관(검사합 `c86233366b29…` 일치), 그 저장소에서의 전체 빌드(0x242230, 경고 0)까지.
  - Task 5 의 `phone_ctl.py` 명령 11개(단계 147개)는 도구의 실제 문법 검사(`plan_steps`)를 통과했고, 그 가운데 `pair` 명령 아홉 개(올리기 전 기록, 시나리오 A~G, 귀 확인 기록 창)와 후처리 명령은 **가짜 전화기 두 대**(진짜 도구의 단계 해석·대기열 코드 + Phase 2 기기 로그의 `FSM` 줄 모양 + 새 `TONE`·`STAT` 줄)에 계획서의 글자 그대로 돌려 봤다 — `TONE` 줄이 `FSM` 줄보다 먼저 올 때와 뒤에 올 때 둘 다 `rc=0`, 요약 줄과 `TONE` 58줄(on 29 / off 29), 터치 대조 일치가 Expected 와 같았다.
- **설계서와 다르게 한 곳**
  - 통화 설계서 §9.5 의 모듈 이름: `board_audio`(코덱·송수신·앰프·이어폰·음량) → 3a 는 출력만 가진 `audio_out` 으로 시작(마이크·이어폰은 뒤에). `tones` → 순수 헤더 `tones.h` + 기기 연결 `tone_out` 둘로.
  - §9.6 "벨 음량과 통화 음량은 따로 둔다" → 3a 는 코덱 음량 하나(100)에 소리마다 디지털 크기(PL3a-D2). 통화 음량은 3b.
  - §9.7 "화면·소리 초기화 실패 → 부팅 화면에 오류 + 재시작 가드" → 소리 실패는 재시작하지 않고 소리 없이 계속 돈다, 화면 표시 없음(PL3a-D10 — 의뢰의 지시).
  - §10 "거는 중: '거는 중…' + 연결음" → 연결음은 교환기의 `ringing` 을 받은 뒤(RINGBACK)부터 난다. 그 앞의 DIALING(응답을 기다리는 최대 5초, 보통 0.1초 안)은 조용하다 — 상태 기계의 할 일을 그대로 따랐다.
  - 의뢰의 "`TONE <이름> on|off` 와 `STAT … tone=`" 에 더해 `AUDIO ready/FAIL` 줄과 `STAT` 의 `audio=`·`aud_us=`·`aud_err=`·`stk_audio=` 를 넣었다(기기 과제의 "여유·CPU" 확인용).
  - Phase 2 가 넘긴 "I2C 잠금을 `app::start` 밖으로"는 `phone_main` 이 정적으로 만드는 것으로 했다(만들기가 실패하지 않는다 — PL3a-D14).
- **확인하지 못한 것** (기기에서만 보이는 것 — Task 5 가 처음 본다)
  - **실제로 소리가 나는지**, 어떻게 들리는지 전부: 벨·연결음·종료음의 음색과 크기, 시작·끝의 딸깍, 찢어지는 소리, 대기 중 앰프의 쉬 소리, 부팅 때 앰프를 켜는 순간의 퍽 소리, 벨 끄기 뒤 정말 0.1초 안에 그치는지.
  - **송신 채널만 연 I2S 로 이 기기에서 소리를 낸 적이 없다**(PL3a-D7): Phase 0 은 수신까지 연 뒤(송신 슬롯 32비트) 소리를 냈다. 같은 설정(슬롯 16비트)으로 "열린" 것까지만 Phase 0 로그로 확인했다.
  - DMA 칸을 256 으로 적은 것과 "쓰기 단위 = 칸"의 효과(쉬었다 다시 쓸 때의 순서)는 드라이버 소스로만 확인했다.
  - 소리 장치 초기화 실패(`AUDIO FAIL`)와 코덱 쓰기 실패 뒤에도 전화기가 그대로 도는지 — 기기에서 일부러 실패시킬 수 없어 코드 대조로만(Task 2 Step 2).
  - 출력 태스크가 화면·터치·교환기 연결에 주는 영향, 내부 메모리 감소량, `stk_audio`, `aud_us` 의 실제 값.
  - 시리얼을 읽는 쪽이 없을 때(충전기 전원) 기록 줄 `printf` 가 출력 태스크를 최대 50ms 멈출 수 있는 것 — 소스로만 확인했다(기록 줄은 소리가 바뀔 때만 찍는다).
  - Task 5 의 명령: 가짜 전화기에서만 돌려 봤다. 실제 기기에서의 시간(벨 직후 `STAT` 왕복, 30초 무응답)과, esptool·`lsof`·교환기 `status` 명령, 재부팅이 든 `--unit` 명령 둘은 돌려 보지 못했다(형태는 Phase 2 에서 실제로 쓴 명령 그대로). `secret_scan.py` 는 가짜 값과 모의 로그로만 돌려 봤다(깨끗한 로그 `rc=0`, 주소를 넣은 로그 `rc=1`) — 진짜 `secrets.h` 는 열지 않았다.
  - 실제 `secrets.h` 로 빌드한 크기(어림 0x242600), 부품 로그 줄(`I2S_IF: STD: TX …` 등)이 전화기 펌웨어의 로그 수준에서 보이는지.
  - 벨 도중 교환기가 끊기는 경우와 벨을 끈 통화가 부재중이 되는 경우는 기기 시나리오에 없다(맥 시험으로만 고정).

### 2026-10-04 밤 — 실행 결과 (Task 1~6)

**먼저 구별해 둔다.** 아래 "확인함"은 시리얼 로그·맥 시험·코드 읽기로 확인한 것이다. **스피커에서 실제로 소리가 나는지, 어떻게 들리는지는 아직 아무도 확인하지 못했다.** 계획 작성 때 "확인하지 못한 것" 맨 위에 적은 그 항목은 그대로 남아 있다.

- **커밋**: Task 1 `fc50abb`(tones.h + 시험 84) · Task 2 `697e5da`(audio_out·tone_out, 아직 안 이음) · Task 3 `f8cff1b`(N-1·N-2 정리 + 도구 시험) · `5d9935f`(연결 — `app`·`phone_main` 한 커밋). Task 4·5 는 커밋 없음(빌드·기기). 기준선 `05d3c2e`. Task 6(이 문서들)은 아래 "문서 커밋".
- **Expected 와 달랐던 곳**: 없다. 시험 먼저 순서도 그대로 — RED 는 `test_tones.cpp:5:10: fatal error: 'tones.h' file not found`, GREEN 은 `84 ok, 0 failed`. 맥 시험 16/105/61/9/91/84, 도구 pytest 186, 교환기 137, 경고 0. 빌드 `5d9935f` 0x242610(2,369,040 B), 앱 파티션 62% 여유, 경고 0. 이전 빌드 `066cf57`(검사합 `c86233366b29…`)은 `out/fw_prev_066cf57/build/` 에 보관.
- **다시 돌린 실행**: 0회(시나리오마다 1회에 통과). C 와 D 는 한 Bash 호출에 두 실행을 이어 붙여 돌렸다(각 명령은 계획 그대로).
- **기기(22:21~22:25)**: 두 기기 같은 바이너리(`App version 5d9935f` · `Compile time Oct  4 2026 22:19:00` · ELF `ca67340d0…`), 두 기기 `AUDIO ready rate=16000 block=256 vol=100`. 시나리오 A~G 전부 `rc=0`·`!!` 0, `TONE` 줄 58개(on 29 / off 29), 터치 대조 일치, 비밀 0건. 시나리오별 표와 값 출처는 통화 설계서 §8 "Phase 3a 신호음 결과".
- **소리가 쓴 것**: `heap_int` 270971→257279(1번) · 271051→257311(2번) = 약 13.7KB, `heap_int_min` 253987 · 253875, `stk_audio` 4304, `aud_us` 평균/최대 315/603 · 292/624(CPU 약 2%), `aud_err` 0. 귀 확인 대기 10분(아무도 안 누름) 뒤에는 `heap_int_min` 이 253971 · 252903 으로 1번 −16 B · 2번 −972 B — 원인은 보지 않았고 기준의 2.5배라 24시간 기록(Phase 4 전)에서 추세를 본다.
- **귀 확인**: 10문항 답 **전부 "사용자 미수행 — 귀 확인 대기(2026-10-04 22:26~22:36 기록 창 1회, 터치 0)"**. 그에 따라 바꾼 상수·코드는 없다. 소리가 전혀 안 나면 1순위 가설은 PL3a-D7 의 탈출구(시험 펌웨어의 `i2s_setup`). 사용자가 밤 늦게라 소리 나는 기기 시험을 더 돌리지 않는다(P3A-R11).
- **최종 전체 검수(opus) 96 / 100 — 게이트 통과**(설계 24/25 · 품질 24/25 · 보안 15/15 · 동시성 9.5/10 · 시험·증거 14/15 · 규칙·문서 9.5/10; Critical 0 · Important 0 · Minor 9, `final-review-report.md`). 통과의 조건 ① 귀 확인에서 "소리가 난다"(**미충족 — 대기**) ② Task 6 문서(이 기록) ③ 3b 첫 과제(아래 목록).
- **"설계서와 다르게 한 곳"의 후속**: 통화 설계서에 정정을 덧붙였다 — §9.5 모듈 이름, §9.6 음량·앞서 쓰는 양, §9.7 소리 실패(PL3a-D10: 지금은 화면 표시·재시작 없이 소리 없이 계속 / 3b 첫 빌드에서 `boot_guard` 3번 재시도로 바꿀 예정 — P3A-R10, 사용자에게는 보고하고 이견이 있으면 바꾼다).
- **계획의 "확인하지 못한 것" 아홉 줄 갱신** (최종 검수 표; 위 "계획 작성" 절의 원문은 그대로 둔다):
  - 닫힘: 화면·터치·교환기 영향·메모리·스택·`aud_us`(수치로) / Task 5 명령이 실제 기기에서 도는지 / 실제 `secrets.h` 빌드 크기(0x242610)와 부품 로그 줄이 보이는지.
  - 절반 닫힘: 송신 채널만 연 I2S(PL3a-D7) — 기대한 설정으로 열렸고 쓰기 오류 0, **들리는지는 귀** / DMA 256·쓰기 단위 = 칸 — 조정 경고 없음·드라이버 소스로 칸 512 B, 쉬었다 다시 쓸 때의 소리는 귀.
  - 남음: **실제로 소리가 나는지·어떻게 들리는지**(딸깍·쉬 소리·부팅 퍽 소리·벨 끄기 뒤 0.1초 포함) / `AUDIO FAIL`·쓰기 실패 뒤 동작(읽기로만) / 시리얼을 읽는 쪽이 없을 때 `printf` 50ms(소스로만) / 벨 도중 교환기 끊김·끈 채 부재중(기기 시나리오 없음, 맥 시험으로만).
- **Phase 2 가 넘긴 것 가운데 3a 가 처리한 것**: ⓪ N-2 닫힘 · N-1 은 범위대로 닫힘(잔여 M-8 은 3b) / ⑤ I2C 잠금 / ⑦ 수신거부 §7.4·§5.3 — 로직 닫힘, 실제 소리가 멈추는가·고르는 동안 벨이 거슬리는가는 귀 확인 대기(`decline-mute.md` §7.4 "확인 결과"). ①~④·⑥ 은 3b.
- **최종 검수 Minor 9건의 처리**: M-1 출력 태스크 `printf`(3b 첫 과제, 목소리 전 필수) · M-2 소리 장치 실패 동작(3b 첫 빌드, P3A-R10) · M-3 실패 경로 미실행·`AUDIO FAIL tones` 표시 불일치(M-2 와 함께) · M-4 전수 훑기 두 줄(P3A-R9, 3b 첫 과제) · M-5 통계 64비트(3b) · M-6 `static_assert`(P3A-R9, 3b 첫 과제) · M-7 섞기 순수 함수화·맥 시험(3b) · M-8 N-1 잔여(3b `relay_link` 잠금) · M-9 인계 목록 보강(이 Task 6 에서 반영 — 아래).
- **3b(목소리)로 넘기는 목록**: 정본은 통화 설계서 `2026-10-03-tab5-phone.md` §8 "Phase 3a 신호음 결과"의 "3b(목소리)로 넘기는 것" 12항이다(한 곳에만 둔다 — 여기서 반복하지 않는다). 이 계획서에만 있는 것: 최종 검수 M-9 가 지적한 "원래 Task 6 Step 5 의 1~7번에 빠진 것"은 그 12항에 모두 들어갔고, Task 6 Step 5 본문(위)의 4번 문장은 정정 주석을 붙였다.
- **귀 확인에 달려 `[ ]` 로 둔 자리**: 이 계획서의 Task 5 Step 11 · 검증 기준의 "사용자 귀 확인 10문항" 줄 / 통화 설계서 §5 Phase 3 아래 "3a 사용자 귀 확인" 줄 / 수신거부 설계서 §7.4 의 ①(실제로 소리가 멈추는가)·⑤(고르는 동안 벨이 거슬리는가).


### 2026-10-05 — 귀 확인 칸 (3b 가 채운 것 — 위 원문은 그대로 두고 덧붙임. 소리 켠 `mic` 점검은 빌드 `e40d097`, 같은 점검을 `5935ddd` 에서 1번만 다시 한 값은 아래)

**먼저 구별해 둔다.** "스피커에서 실제로 소리가 난다"는 3b 가 소리 장치를 **송수신(Phase 0 에서 실제로 소리가 났던 설정)** 으로 바꾼 뒤의 확인이다(PL3a-D7 → Phase 0 설정). **3a 의 송신 전용 설정으로 소리가 났는지는 끝내 확인하지 않았다** — 이제 그 설정은 코드에 없다. 아래는 그 확인과, 확인하지 못한 것이다.

- **기록으로 확인됨(소리가 난다)**: 스피커를 켠 `SELFTEST` 의 마이크 칸 세기 `mic`(빌드 `e40d097` 에서 잰 값; 조용한 모드 값은 같은 빌드의 아침 점검 `out/p3c_cmd_unit1.log`·`p3c_cmd_unit2.log`) — 1번 **3280**(조용한 모드 12), 2번 **1211**(조용한 모드 26) = 스피커에서 실제로 소리가 났고 그 기기의 마이크가 들었다(약 270배·47배). 조용한 모드에서도 되먹임 칸 `ref` 569~592 로 코덱이 소리를 내고 있는 것은 밤에 이미 보였다. 로그 `out/p3c_loud_unit1.log`·`p3c_loud_unit2.log`(둘 다 `e40d097`). `5935ddd`(Wi-Fi 절전 끈 빌드)에서 1번을 다시 점검한 값은 `mic=3857`(`out/p3d_swap_1on.log`) — 2번은 `5935ddd` 에서 스피커를 켠 채 다시 재지 않았다(로그에 없음). 소리 경로는 Wi-Fi 절전과 무관하다.
- **사용자의 통화로 확인됨**: 사용자가 통화해 보고 "2번은 잘 들림"(1번 마이크 → 2번 스피커). 이것은 **목소리**이고 신호음(벨·연결음·종료음)에 대한 답이 아니다. 사용자는 이때 연결음·종료음도 들었을 것이나 답은 받지 못했다(추정일 뿐).
- **3a 귀 확인 10문항(계획서 Step 11 표)의 답 — 전부 "미답"**: 1 벨·연결음이 들리는가 — 미답 / 2 벨 끄기로 멈추고 다시 울리지 않는가 — 미답 / 3 받으면 멈추는가 — 미답 / 4 종료음이 들리는가 — 미답 / 5 벨을 끄지 않고 받으면 벨이 멈추는가 — 미답 / 6 거절 때 — 미답 / 7 취소 때 남는 소리 — 미답 / 8 음량(벨·연결음·종료음) — 미답 / 9 잡음·딸깍·쉬 소리 — 미답(통화 품질에 대한 질문에서 "찢어지는 소리"는 고르지 않았으나 신호음 이야기가 아니다) / 10 고르는 동안 벨이 거슬리는가 — 미답. 이유: 2026-10-05 07:59~08:04 에 귀 확인 창(`out/p3c_ear_tones.log`)을 열었으나 사용자 조작이 없었고(FSM·TONE 줄 0) 이후 통화 시험이 목소리 쪽으로 옮겨 갔다. 3b 점검표(14문항)의 1·2·10·11·12 번이 이 10문항을 겸하도록 만들었는데 모두 미답이다.
- **귀 확인 전의 설계 값 그대로인 것**: 신호음의 모양·크기(`BELL_AMP` 8000 · `RINGBACK_AMP` 5300 · `END_AMP` 3700, 코덱 음량 100). 벨 끄기 뒤 스피커에서 그치는 시간은 DMA 가 6칸에서 3칸으로 줄어 설계 값으로 약 0.1초 → 약 0.03초가 됐다(드라이버 소스로 본 값, 기기에서 재지 않았다). 사용자가 음량이 "작음"이라 한 것은 통화 목소리에 대한 말이다.
- **그 밖에 3b 가 닫은 3a 의 숙제**: M-1(출력 태스크 `printf`)·M-2/M-3(소리 장치 실패 동작 — 실패 주입으로 4부팅·재시작 3번·`PHONE FAIL audio` 확인)·M-4·M-5·M-6·M-7·M-8(`relay_link` 잠금)은 3b 에서 처리했다(통화 설계서 §8 "Phase 3b 목소리 결과"의 12항 표).
