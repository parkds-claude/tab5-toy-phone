# Tab5 음성 전화기 — Phase 3b 목소리 통화 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> (실행하는 에이전트에게: 위 두 스킬 중 하나로 과제를 하나씩 수행하고, 단계는 체크박스로 추적한다.)

- **작성일**: 2026-10-05
- **상태**: 계획·초안 검증 완료 — 실행 대기 (기기에는 올리지 않았다) → **2026-10-05 갱신**: 구현(Task 1~7)·두 기기 검증(Task 8·8b)·최종 검수(95점, 수정 재검수 96점 잠정)·fix wave 2·fix 3(Wi-Fi 절전 끄기) 완료, 두 기기에 `5935ddd`. **Task 9(사용자와 함께)는 부분 수행** — 사용자 귀 판정은 일부만("동시에 말해도 잘 됨. 다만 통화품질은 좀 아쉽긴 함"). 검증 기준 가운데 `[ ]` 로 남은 것: 내부 메모리(`heap_int_min`)·지연(아침)·아침 손실·점검표 14문항. 다음 = 3c(품질 다듬기) `docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md`(작성 중 — 2026-10-05). **→ 갱신: 3c 계획서는 작성됐고 첫 묶음은 2026-10-05 에 실행됐다(두 기기 `771b9fe`) — 진행 로그 맨 끝 "다른 방 통화의 귀 판정과 3c 첫 묶음".** 결과는 아래 진행 로그 "2026-10-05 — 실행 결과 (Task 1~10)".
- **작성자**: Claude (맥북 세션)
- **넣는 자리**: 통화 설계서 §5 의 Phase 3(소리 경로)을 둘로 나눈 뒤쪽. 3a(신호음)는 `2026-10-04-tab5-phone-phase3a-tones.md` 로 끝났고(두 기기 `5d9935f`, 사용자 귀 확인만 대기), 이 계획이 **3b = 받으면 양쪽이 말하고 듣는 통화**다. 사용자 지시: "가자 가자! 속도를 내자!", "묻지 말고 모든 것을 최적, 최선의 결정으로 진행할 것".

**Goal:** 전화를 받으면 두 기기가 **스피커폰으로 동시에** 목소리를 주고받는다 — 마이크 → 에코 제거 → Opus 압축 → 교환기 → 상대 기기 → 지터 버퍼 → Opus 해제 → 스피커. 받기 전에는 마이크가 열리지 않고, 끊으면(어느 길로 끝나든) 그 자리에서 닫히고 종료음이 난다. 벨·연결음·종료음·통화 흐름·화면·기존 시리얼 줄은 지금과 같다(기존 시험을 고치지 않고 전부 통과).

**Architecture:** 판단(목소리를 켤지, 16ms 묶음 ↔ 20ms 프레임, 순번, 밀리면 버리기, 지터 버퍼, 빠진 프레임 메우기, 신호음과 섞기, 시험음·지연 재기)은 하드웨어와 무관한 헤더 셋(`pcm.h`·`jitter_buffer.h`·`voice_core.h`)에 두고 맥에서 시험을 먼저 쓴다. 기기 전용 코드는 둘이다: `audio_out`(코덱 둘과 I2S 송수신을 열고, 공급자에게 16ms 씩 PCM 을 받아 내고, 마이크 칩의 4칸을 읽어 준다 — 3a 의 "송신 채널만"을 Phase 0 에서 실제로 소리가 났던 송수신 설정으로 바꾼다)과 `voice`(태스크 셋 — 캡처·해제·송신 — 과 에코 제거·Opus 호출, `audio_out` 의 둘째 공급자). `app.cpp` 는 상태 기계가 Phase 2 부터 내고 있던 `A_VOICE_START`·`A_VOICE_STOP` 을 `voicecore::from_acts` 로 켜기/끄기로 바꿔 넘기기만 한다. 교환기는 바꾸지 않는다(통화 중의 이진 프레임을 그대로 건넨다 — 이미 그렇게 돼 있다).

**Tech Stack:** ESP-IDF 5.5.4 (esp32p4) · M5Unified 0.2.23 / M5GFX 0.2.30 · esp_codec_dev 1.6.2(ES8388 + ES7210) + esp_driver_i2s(송신 표준 I2S + 수신 4칸 TDM) · esp-sr 2.5.5(에코 제거 `AEC_MODE_VOIP_HIGH_PERF`) · esp_audio_codec 2.5.0(Opus) · esp_websocket_client 1.8.0 · clang++ (맥 단위 시험) · Python 3 + pytest + pyserial

**Spec:** 통화 설계서 `docs/exec-plans/2026-10-03-tab5-phone.md` §4·§5(Phase 3)·§6·§9.2(PH-D5·D6·D7)·§9.4(교환기 계약)·§9.5·**§9.6(소리 경로·지연 예산·정정 3건)**·§9.7·§8("Phase 0 가정 검증 결과", "3b(목소리)로 넘기는 것" 12항). 출발점은 3a 최종 검수의 "다음 단계(목소리)가 이 코드 위에 설 때 바꿔야 하는 것" 11항과 Minor M-1~M-9(`.superpowers/sdd/2026-10-04-tab5-phone-phase3a-tones/final-review-report.md`)다. 3b 는 별도 설계서 없이 **이 계획서가 설계를 겸한다** — 정한 것은 아래 "지연 예산"·"CPU·메모리 예산"·"이 계획에서 내린 결정"(PL3b-D1~D20)에 이유·비용·되돌리는 법과 함께 있다.

## 한눈에 보기 (사용자용)

| 과제 | 하는 일 | 기기 | 사용자가 할 일 | 걸리는 시간(어림) |
|---|---|---|---|---|
| Task 1 | 3a 검수가 넘긴 정리: 소리 내는 태스크가 글자를 찍지 않게, 교환기 다시 붙기와 보내기가 부딪히지 않게, 소리 장치가 안 켜지면 3번까지 다시 켜 보게 | 없음 | 없음 | 10분 |
| Task 2 | 소리를 섞고 세기를 재는 작은 계산을 넣는다(시험 먼저) | 없음 | 없음 | 5분 |
| Task 3 | 소리 장치를 "스피커만"에서 "스피커 + 마이크"로 바꾼다(Phase 0 에서 실제로 돌았던 설정 그대로) | 없음 | 없음 | 5분 |
| Task 4 | 들쭉날쭉 도착하는 목소리를 고르게 내주는 버퍼를 넣는다(시험 먼저) | 없음 | 없음 | 5분 |
| Task 5 | 목소리 통화의 판단 로직을 넣는다(시험 먼저 — 받기 전에는 마이크가 안 열린다, 끊으면 닫힌다, 빠진 소리 메우기 등) | 없음 | 없음 | 10분 |
| Task 6 | 목소리 모듈을 넣고 통화 흐름에 잇는다 | 없음 | 없음 | 10분 |
| Task 7 | 지금 기기에 올라가 있는 판(`5d9935f`)을 보관하고 새로 빌드한다 | 없음(빌드만) | 없음 | 5분 |
| Task 8 | 두 기기에 올려 **스피커를 끈 채로** 자동 검증한다: 통화가 열리면 목소리 자료가 양쪽으로 흐르는지, 끊으면 멈추는지, 지연·끊김·CPU·메모리 | **1번·2번**(각 5분 안팎 다시 올림) | 약 25분 동안 **두 기기 화면을 만지지 않기**(소리는 나지 않는다 — 다시 올린 직후 부팅 때 "퍽" 소리 한 번은 날 수 있다). 자리에 없어도 된다 | 35분 |
| Task 9 | **사용자와 함께** 스피커를 켜고 실제로 통화해 본다(점검표) | 1번·2번(다시 올리지 않는다) | 두 기기를 다른 방에 두고 통화하며 점검표에 답하기. 3a 의 귀 확인(벨·연결음·종료음)도 이때 함께 | 20분 |
| Task 10 | 결과를 문서에 적는다(README, 통화 설계서, 이 계획서) | 없음 | 없음 | 10분 |

- **이번에 생기는 것**: 받으면 양쪽이 동시에 말하고 듣는다. 끊으면 종료음.
- **지연**(말한 뒤 상대 스피커에서 나기까지): 같은 Wi-Fi 에서 약 0.12초(어림 — 아래 "지연 예산"). 맥미니·터널을 거치는 Phase 4 에서는 약 0.18~0.2초로 예상한다. 설계서의 합격선은 0.5초다.
- **음량 조절은 아직 없다**(고정값 — Phase 0 에서 "적당"이라 하신 크기에 맞췄다. 조절 화면은 Phase 5). 크거나 작으면 Task 9 에서 말씀해 주시면 상수 한두 개를 바꾼다.
- **Task 8 은 사용자 없이도 돌릴 수 있다**: 스피커 앰프를 끄는 "조용한 모드"를 넣어, 소리를 내지 않고도 소리 경로를 기록으로 확인한다(밤에 벨이 울리지 않는다 — 진행 기록의 판정 P3A-R11). 실제로 어떻게 들리는지는 Task 9 에서 사용자의 귀가 판정한다.
- **3a 의 "소리가 정말 나는가"도 이번에 닫힌다**: 3a 는 이 기기에서 소리를 낸 적 없는 설정(송신 채널만)이었고 귀 확인을 아직 못 했다. Task 3 이 그 설정을 Phase 0 에서 실제로 소리가 났던 설정으로 바꾼다 — 귀 확인에서 "소리가 안 난다"가 나와도 고칠 곳이 이 과제다. Task 8 은 스피커 직전의 되먹임 칸으로 "코덱이 소리를 내고 있다"를 기록으로 보고, Task 9 가 귀로 확인한다.
- 두 기기는 시험 뒤에도 전화기로 남는다. 되돌리는 법은 아래 "롤백"(각 5분).
- **실행자에게 — 코드를 옮겨 적지 않는다.** 이 계획에 실린 패치와 도구는 맥북의 `~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft/`(이하 `D`, git 제외, 맥북에만 있음, **읽기 전용**)에 그대로 있다. 계획서의 코드 블록과 글자 단위로 같다(조립 스크립트가 끼워 넣었다 — 목록과 검사합은 문서 끝 "초안 목록").
  - 수정·새 파일은 `D/patches/TN-MM-….patch` 를 저장소 루트에서 `git apply` 로 적용하고, 적용한 뒤의 파일을 `D/files/<저장소 경로>` 와 `cmp` 로 대조한다(출력 없음 = 같음). 같은 파일이 두 과제에서 바뀌는 것이 넷 있다(`audio_out.h/.cpp` = Task 1·3, `app.h/.cpp`·`phone_main.cpp` = Task 1·6) — 앞 과제에서는 `cmp` 대신 그 Step 에 적힌 줄 수·`grep` 으로 확인하고, 마지막으로 바뀌는 과제에서 `cmp` 한다.
  - **순서를 지킨다**: 시험 패치 → 실패 확인 → 구현 패치 → 통과 확인 → 커밋. 한꺼번에 적용하지 않는다.
  - 적용한 파일을 고치지 않는다. Expected 와 다르면 스스로 고치지 말고 멈춰서 보고한다(실제 출력 첨부).
  - `D/verify/` 의 도구(`touch_audit.py`·`secret_scan.py` — Phase 2 부터 쓰던 것 그대로, `voice_check.py` — 이번에 더한 것)는 저장소에 넣지 않고 그 자리에서 실행한다.
  - 그 폴더가 없거나 내용이 다르면 계획서의 코드 블록이 정본이다(블록을 그대로 파일로 옮겨 `git apply --check` 부터).
- 이 계획에 실린 코드는 작성 시점(2026-10-05, 저장소 `4ace824`)에 저장소 밖 사본에서 실제로 돌려 확인했다: 깨끗한 사본에 패치 16개를 과제 순서대로 적용하며 실패 → 통과를 확인(맥 단위 시험 16·105·61·9·91 유지, `tones` 84 → **89**, 새 `pcm` **37** · `jitter_buffer` **65** · `voice_core` **117**, 검증 도구 pytest 186 → **218**, 교환기 pytest 137 유지), **커밋 경계마다** 펌웨어 전체 빌드 성공(0x242230 → 0x2423b0 → 0x242680 → 0x244660 → 0x244660 → **0x287720**, 앱 파티션의 58% 여유, 우리 코드 경고 0, 부품 판 그대로), 구현을 일부러 망가뜨린 97가지를 새 시험이 전부 잡음. 통화 한쪽 방향을 맥에서 시뮬레이션해(손실 없는 망의 멈춤·몰림, 두 기기의 시계 어긋남, 1~2시간) 지연이 쌓이지 않음을 봤고, 그 과정에서 **미리 써 둔 지터 버퍼 초안의 결함 하나를 찾아 고쳤다**(PL3b-D5). **기기에 올려 본 적은 없다** — 마이크·에코 제거·Opus 가 이 펌웨어에서 실제로 도는지, 어떻게 들리는지는 Task 8·9 에서 처음 확인된다(끝의 "진행 로그 — 확인하지 못한 것", 위험이 큰 순서).

## Global Constraints

통화 설계서·앞 계획들에서 온 전 과제 공통 요구사항. 모든 과제는 이 절을 암묵적으로 포함한다.

- **기기 번호**: 1번 = MAC `AA:BB:CC:DD:EE:01`(교환기 ID `tab5-1`), 2번 = MAC `AA:BB:CC:DD:EE:02`(`tab5-2`).
- **기기는 번호(MAC)로 고른다**: 포트는 항상 `python3 tools/phone_ctl.py port <1|2>` 로 얻고, 플래시 직전에 `read_mac` 으로 다시 확인한다. `/dev/cu.usbmodem*` 를 직접 고르지 않는다.
- **코드 과제(Task 1~7)는 기기·시리얼 포트·교환기를 건드리지 않는다.** 기기는 Task 8·9 에서만.
- **칩·빌드**: ESP-IDF 5.5.4, 대상 `esp32p4`, 두 기기 모두 P4 rev v1.3(`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`, `CONFIG_ESP32P4_REV_MIN_100=y` — 그대로 둔다). **`esp_hosted` 2.11.7 고정**, Wi-Fi 칩(C6) 펌웨어 교체 금지, `amcchord/M5Tab5-VideoLink` 플래시 금지, `esp_audio_codec` 2.5.0, `esp-sr` 2.5.5, `esp_codec_dev` 1.6.2. `sdkconfig.defaults`·`main/idf_component.yml` 을 고치지 않는다(소리 부품은 Phase 2 때 이미 들어 있다). `main/CMakeLists.txt` 는 소스 한 줄만 더한다. `dependencies.lock` 은 바뀌지 않아야 한다.
- **범위**: 고치는 파일은 아래 "초안 목록"의 패치가 건드리는 것이 전부다. 교환기(`server/`), 화면(`ui.cpp`·`ui_layout.h`·`ui_assets.h`·`gen_ui_assets.py`), 판단 헤더(`call_fsm.h`·`relay_proto.h`·`tones.h`), 도구(`phone_ctl.py`·`phone_fake_peer.py`), `server/tools/phone_relay_dev.sh` 를 고치지 않는다. **기존 시험은 한 글자도 고치지 않는다**(시험 패치는 추가만). **기존 시리얼 줄의 모양은 한 글자도 바꾸지 않는다** — `STAT` 과 `AUDIO ready` 의 새 칸은 줄 끝에 더한다(도구 시험이 펌웨어 소스의 형식 문자열을 읽어 고정한다).
- **비밀**: `secrets.h`·`server/.env.phone` 을 `cat`/Read 로 열지 않는다. 그 내용(Wi-Fi 비밀번호, 토큰, 교환기 주소)을 화면·로그·보고·커밋에 내지 않는다. **목소리 자료(PCM·압축 프레임)를 시리얼·파일로 내보내는 기능은 없다** — 새 줄에 나오는 것은 세기(RMS) 숫자와 셈뿐이다.
- **이 맥의 함정 네 가지**: ① 맥 단위 시험은 `clang++ … -isysroot "$(xcrun --sdk macosx --show-sdk-path)"` 가 있어야 링크된다. ② `source ~/esp/esp-idf-5.5.4/export.sh` 를 한 셸의 `python3` 에는 Pillow·pytest 가 없다 → 파이썬 도구·시험은 export 하지 **않은** 호출에서, 빌드·esptool 은 export 와 **같은 호출 안에서**(Bash 도구는 호출마다 새 셸이다). ③ 명령을 `sh -c '…'` 로 감싸지 않는다. ④ Bash 도구의 `grep` 은 ugrep 이다 — 파일 여러 개를 한 번에 주면 줄이 나오는 순서가 실행마다 다르다. 이 계획의 명령은 그래서 파일을 하나씩 돌거나 `sort` 한다(적힌 그대로 실행하면 된다). 오래 걸리는 명령(빌드, 3분 통화)은 Bash 의 `timeout` 을 600000 으로 준다. 셸 글에서 `$변수` 바로 뒤에 한글을 붙이지 않는다(`${변수}` 로).
- **main 에서 직접 작업**(브랜치·worktree 없음). devsync 가 5분마다 `git add -A` 로 자동 커밋한다 → `git commit` 이 "nothing to commit" 이면 devsync 가 먼저 커밋한 것이라 정상이다(보고에 적는다). `git add` 는 그 과제의 파일만(`git add -A` 금지). 임시 파일은 저장소 밖(`/tmp`)이나 git 제외 폴더 `firmware/tab5_phone/out/` 에만 둔다.
- **교환기**: 지금 맥북에서 켜져 있고 사용자가 쓰고 있다. 이 계획은 교환기 코드를 바꾸지 않으므로 **다시 띄우지 않는다** — `bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh status` 로 켜져 있는지만 본다(정확히 이 모양, 한 번의 호출에 이 명령 하나만). 꺼져 있으면 켜지 말고 멈춰서 보고한다(켜는 것은 컨트롤러).
- **두 기기에는 같은 바이너리를 올린다**: 빌드는 Task 7 에서 한 번만 하고, 플래시는 `idf.py flash`(HEAD 가 움직이면 다시 빌드해 바이너리가 달라진다) 대신 빌드 폴더에서 `python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash @flash_args` 를 쓴다(직전에 `read_mac` 으로 대상 확인). 부팅 로그의 `App version`·`Compile time` 이 두 기기에서 같은지 기록한다.
- **소리**: Task 8 은 **조용한 모드**(`quiet on` — 스피커 앰프를 끈다, 재부팅 뒤에도 유지)에서만 돌린다. 새 펌웨어를 올린 직후(Step 3·4) 기기마다 `quiet on` 을 걸고, `STAT` 의 `amp=0` 을 확인한 뒤에만 통화 시험으로 간다(그 전에는 전화를 걸지 않는다 — 옛 펌웨어에는 조용한 모드가 없다). **`quiet off` 는 Task 9 에서 컨트롤러가 사용자가 자리에 있을 때만** 건다(P3A-R11: 사용자 없이 벨·목소리가 나는 시험을 돌리지 않는다). Task 8 이 끝나도 조용한 모드를 풀지 않는다.
- **멈추라는 조건에 걸리면 멈춘다**: BLOCKED 로 보고하고 기다린다. 원인을 찾았고 더 나은 방법이 떠올라도 실행하지 말고 제안으로만 적는다. 특히 기기에 쓰는 일 앞의 정지 조건은 실행 담당이 스스로 풀 수 없다. **기기 시험에서 결함이 나오면 고치지 않는다** — 진단해서 보고한다(Task 8 "결함이 나오면").
- **터치 대조**: 기기 시험의 각 실행은 `both:until:stat`(또는 `until:stat`)으로 시작하고 끝난다. 끝나면 `D/verify/touch_audit.py` 로 주입한 탭 수와 `touch=` 증가를 대조한다. 화면이 바뀐 뒤 같은 기기의 다음 탭 앞에는 1초 이상 쉰다(화면 가드 0.6초).
- **줄의 순서**: `VOICE on|off` 는 app 태스크가 그 원인인 `FSM` 줄 **바로 앞**에 찍는다(늘 같은 순서 — `wait` 로 차례로 기다려도 된다). `TONE` 줄은 app 태스크가 다음 바퀴에 찍으므로 대개 `FSM` 줄 뒤지만 앞뒤를 믿지 않는다(3a 와 같다): 기기 명령은 `TONE` 줄을 `wait` 로 기다리지 않고, 소리 상태는 `until:stat` 의 `tone=` 로, 순서는 실행이 끝난 뒤 기기별 `TONE` 줄만 뽑아 본다.
- **참고 구현 대조 우선(사용자 지시 2026-10-03)**: 기기에서 이상한 동작이 나오면 추측하지 않는다. 이 기기에서 실제로 마이크·에코 제거·Opus 를 돌린 Phase 0 시험 펌웨어(`firmware/tab5_phone_probe/main/board_audio.cpp`·`probe_main.cpp`, 부팅·측정 로그 `firmware/tab5_phone_probe/out/task4.log`·`task5.log`)와 줄 단위로 대조한다.
- **진행 안내·보고는 전부 한국어.**

## Review Focus

이 계획이 암시하지만 과제의 시험이 놓치기 쉬운 입력·고장 다섯 가지 — 일어나기 쉬운 것부터. 각 줄의 시험은 해당 과제에 들어 있다.

1. **통화가 끝났는데 마이크·송신·재생이 남는다**(끊기를 눌렀는데 교환기 응답 전까지 열려 있음, 상대가 끊음, 교환기 끊김, 끊김을 못 본 채 다시 붙음, 상대 재부팅, 끝난 뒤 뒤늦게 온 프레임이 재생됨, 다음 통화에 앞 통화의 소리가 섞임) → Task 5 `test_voice_core.cpp` "the call ends every way (…): voice is off"·"hanging up: voice goes off in the same batch as the hangup signal (0x248), not after the relay answers"·"3 million random events with time passing: voice on <=> IN_CALL after every one"·"rx: frames that arrive after the call ended are dropped, nothing is decoded any more"·"rx: a stray wake-up of the decoder task after the call ended decodes nothing"·"rx: the next call starts silent — nothing of the previous call is played"·"tx queue: flush empties it …", Task 8 Step 10(상대 재부팅)·Step 11(연달아 통화, 통화 밖에서 `v_cap` 이 늘지 않는다)·Step 13(`VOICE` 켜기·끄기의 짝)
2. **받지 않았는데 마이크가 열린다 / 소리가 나간다**(벨이 울리는 동안, 받기를 누른 직후 `connected` 전, 받지 않았는데 온 `connected`, 통화 밖에서 온 프레임) → Task 5 "never open before the call is accepted: …"·"a 'connected' that arrives while still ringing (not answered) does not open the mic"·"state machine: start only comes together with entering IN_CALL"·"rx: before any call there is no sound … frames that arrive outside a call are not even stored"·"uplink: the first 8 blocks (128 ms) after the call opens are thrown away", Task 8 Step 6·8·9(벨이 울리는 동안 `voice=0`, `VOICE on` 이 `IN_CALL` 줄 앞에만), Step 12(거절·취소·무응답에는 `VOICE` 줄이 없다)
3. **프레임이 늦거나 몰려 오거나 빠질 때 소리가 깨지거나 지연이 쌓인다**(Wi-Fi 가 잠깐 멈췄다 풀림 — TCP 라 잃지 않고 한꺼번에 온다, 두 기기의 시계가 조금 다름, 여유가 0 이 된 뒤 끝없이 끊김, 순번이 65535 를 넘어감) → Task 4 `test_jitter_buffer.cpp` "receiver clock 0.1% fast for 2 hours: one concealed frame per slip, never a run of them"·"stall then burst (40 ms .. 10 s): back near the target within 1.5 s — the delay never piles up"·"resume after a gap: it starts from the newest 'target' frames …"·"loss 0-50% + duplicates + reordering, 16 min each: …"·"5 million random pushes / pops / resets: …", Task 5 "sim stall 60 ms .. 4 s then a burst: 2 s after it clears the delay is back under 140 ms"·"sim 1 h, sender clock 200 ppm slow: …"·"sim 1 h, sender clock 200 ppm fast: …"·"conceal: …"(딸깍 없이 메우고 이어 붙임), Task 8 Step 9(3분 통화의 `v_lost`·`v_drop`·`v_under`·`v_jb`)
4. **신호음과 목소리가 겹치는 순간**(거는 쪽: 통화가 열릴 때 연결음 멈춤과 목소리 켜기가 **한 묶음**으로 온다 — 연결음 꼬리 위에 첫 목소리, 연결음 꼬리가 마이크로 들어가 상대에게 감 / 끝: 목소리 끄기와 종료음이 한 묶음 — 딸깍, 넘침, 끝난 통화의 목소리가 종료음에 섞임) → Task 2 `test_pcm.cpp` "mix: tone + voice -> the sum of both (nobody wins)"·"mix: loud + loud clips at 32767 (no wrap-around)", Task 5 "overlap: the ringback is gone within 5 ms, without a click, …"·"overlap: the first voice comes at least 2 blocks (32 ms) after the ringback stopped …"·"overlap: nothing captured in the first 96 ms after the call opens is sent …"·"call end: that block = the voice fading out (5 ms) + the end tone starting; …"·"a bell right after: exactly the bell, no leftover voice mixed in", Task 9(귀: 통화가 열리고 끝나는 순간의 소리)
5. **CPU·메모리·Wi-Fi 가 빠듯할 때 화면·터치·교환기 연결이 굶거나, 보낼 음성이 쌓이거나, 다시 붙는 도중의 보내기가 메모리를 깬다** → Task 5 "tx queue: when the sender is stuck it keeps the newest 10 frames (200 ms) and drops the oldest"·"tx queue: 2 million frames with a sender that stalls now and then: …"·"rx: the output task never waits — if the decoder task is starved it plays zeros and counts it", Task 2 "gap: 48 ms (the whole DMA ring) counts as late", Task 1 Step 2(`relay_link` 잠금 — 맥 시험이 없다, 코드 대조), Task 8 Step 8·9·13(`voice_check.py`: 부하 ≤ 50%, `heap_int_min` ≥ 100000, `stk_*` ≥ 1024, `aud_late`=0, `v_under`=0, 터치 대조, `RELAY up=0` 0줄), Task 9 Step 4(말하는 동안의 같은 숫자). Wi-Fi 를 일부러 막는 시험은 기기에서 하지 않는다(진행 로그 "확인하지 못한 것").

---

## 비목표·영향 범위

**넣지 않은 것**(한 줄씩):

- 맥미니 배포·터널·`wss://`·토큰 교체 → Phase 4. 서로 다른 망에서의 지연 실측(§6 의 500ms 기준)도 Phase 4.
- 음량·음소거·이어폰 화면, 이어폰 감지 → Phase 5. 지금 음량은 상수(`voice_core.h` 의 `TX_GAIN_Q8`·`PLAY_GAIN_Q8`, `audio_out.cpp` 의 `VOLUME`·`MIC_GAIN_DB`).
- 소리 장치가 죽었을 때의 화면 표시("소리 꺼짐") → Phase 5(지금은 3번 다시 켜 본 뒤 조용히 돈다 — PL3b-D2).
- 터치 시각 기준(Phase 2 검수 M-5), `app` 판단의 순수 구조체화(M-7 의 app 쪽), 버튼 겹침(M-1 화면) → 뒤로. 신호 보내기가 화면 태스크를 최대 2초 멈추는 것(RL-D6)은 그대로다 — 음성은 app 태스크가 아닌 전용 태스크가 보내므로 음성 때문에 화면이 멈추는 일은 없다.
- 잡음 억제·자동 음량·음성 감지, Opus 의 가변 비트율·무음 구간 생략(DTX)·오류 정정(FEC) → 넣지 않는다(Phase 0 에서 잰 설정 그대로 — PL3b-D17).
- 에코 제거 품질이 모자랄 때의 후퇴(반이중 등, PH-D7) → Task 9 에서 사용자가 "메아리가 거슬린다"고 하면 따로 한다.
- Wi-Fi 를 일부러 막거나 교환기를 끄는 기기 시험 → 하지 않는다(교환기는 사용자가 쓰는 중). 맥 시뮬레이션으로만.
- 새 펌웨어 한 대 + 옛 펌웨어 한 대의 섞인 상태 → 신호 규약이 그대로라 통화 흐름은 된다. 옛 쪽은 목소리를 보내지도 내지도 않는다(시험하지 않는다).
- 목소리 녹음·내보내기(진단용 포함) → 넣지 않는다.

**영향 범위**

- 저장소 파일: 새 파일 8개(`main/pcm.h`·`jitter_buffer.h`·`voice_core.h`·`voice.h`·`voice.cpp`, `tools/test_pcm.cpp`·`test_jitter_buffer.cpp`·`test_voice_core.cpp`), 고치는 파일 13개(`main/CMakeLists.txt`·`app.h`·`app.cpp`·`phone_main.cpp`·`audio_out.h`·`audio_out.cpp`·`tone_out.h`·`tone_out.cpp`·`relay_link.h`·`relay_link.cpp`, `tools/test_tones.cpp`·`test_phone_ctl.py`, `README.md`) + 통화 설계서·3a 계획서(Task 10 에서 글만) + 이 계획서.
- 기기: Task 8 에서 2번·1번 순서로 다시 플래시(각 5분 안팎, 저장된 Wi-Fi 목록·SD 카드는 그대로). NVS(`phone`)에 열쇠 셋이 새로 생길 수 있다: `quiet`(조용한 모드), `f_audio`(실패 주입 — 시험 뒤 스스로 지워진다), `rst_audio`(다시 켜 본 횟수).
- 교환기: 건드리지 않는다. 과금 없음. 통화 중 음성은 한쪽 방향 약 43kbps(프레임 62바이트 + WebSocket·TCP·IP 머리, 20ms 마다).
- 새 산출물은 git 제외 폴더 `firmware/tab5_phone/out/` 에만 생긴다(`p3b_*.log`·`p3b_*.txt`·`p3b_fw_build.sha256`, `fw_prev_5d9935f/`).
- 내부 메모리·CPU: 아래 "CPU·메모리 예산".

## 롤백

- **코드**: 커밋을 뒤에서부터 `git revert` 한다. ① 연결 커밋(Task 6 Step 6)만 되돌리면 목소리 모듈은 남지만 쓰이지 않는다(빌드되고, 통화는 3a 처럼 조용하다 — 소리 장치는 송수신으로 열린 채). ② 그다음 목소리 모듈 커밋(Task 6 Step 2). ③ `voice_core.h`(Task 5)·`jitter_buffer.h`(Task 4)는 아무도 쓰지 않게 되므로 남겨도 된다. ④ 소리 장치 송수신 커밋(Task 3)을 되돌리면 3a 의 "송신 채널만"으로 돌아간다. ⑤ Task 1 의 세 커밋(시험 보강, `relay_link` 잠금, 소리 실패·`TONE` 줄)은 목소리와 무관하게 남겨도 된다. 순서를 건너뛰면(예: Task 3 만 되돌림) 빌드되지 않는다(`voice.cpp` 가 `audioout::read_mic` 을 쓴다).
- **기기 → 직전 펌웨어**(`5d9935f` = 신호음만, Task 7 Step 1 이 `out/fw_prev_5d9935f/build/` 에 보관): `cd ~/tab5-toy-phone/firmware/tab5_phone && source ~/esp/esp-idf-5.5.4/export.sh && PORT=$(python3 tools/phone_ctl.py port 2) && python -m esptool --chip esp32p4 -p "$PORT" read_mac | grep -i "aa:bb:cc:dd:ee:02" && cd out/fw_prev_5d9935f/build && python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash @flash_args` (1번은 `port 1`, `aa:bb:cc:dd:ee:01`). 이 명령은 실행해 본 적이 없다(형태는 3a 에서 `build/` 로 올린 명령과 같다). 더 앞의 판은 `out/fw_prev_066cf57/`(소리 없음).
- **소리만 급히 끄고 싶을 때**(다시 올리지 않고): 시리얼로 `quiet on` — 스피커 앰프가 꺼지고 재부팅 뒤에도 유지된다. 풀 때는 `quiet off`.
- **교환기**: 바뀌지 않는다.

## 지연 예산 (입 → 귀)

지연이 이 제품이다. 버퍼 깊이를 먼저 정하고(결정 PL3b-D4·D5·D6·D8·D9) 그 합을 적는다. "맥 시뮬레이션" 칸은 `test_voice_core.cpp --info` 가 찍는 값이다(보내는 쪽 16ms 묶음 → 20ms 프레임 → 망 → 지터 버퍼 → 링버퍼 → 받는 쪽 16ms 묶음을 그대로 돌린 것 — Task 5 Step 3).

| 구간 | 지연(ms) | 무엇이 정하나 | 근거 |
|---|---|---|---|
| ① 마이크 표본이 캡처 묶음(16ms)에 담겨 읽힌다 | 0~16 (평균 8) | 에코 제거의 묶음 = 256표본(바꿀 수 없다) | Phase 0 실측(묶음 256), 드라이버 소스(DMA 한 칸 = 한 묶음) |
| ② 에코 제거 안에서의 지연 | **모름**(0~16 으로 어림) | esp-sr(닫힌 라이브러리) | 재지 못한다 — Task 9 의 귀(대화가 겹치는가)로만 |
| ③ 16ms 묶음이 20ms 프레임으로 찬다 | 0~32 (평균 16) | Opus 프레임 20ms(PH-D5) | 맥 시험 "uplink: 4 frames per 5 blocks, in the beat 0,1,1,1,1" |
| ④ Opus 의 알고리즘 지연 | 약 6.5 | libopus 의 일반 값 | 이 부품에서 확인하지 않았다 |
| ⑤ 보내기 대기열 → 소켓 | 평소 0 (막히면 최대 200 + TCP 버퍼) | PL3b-D8 | 맥 시험 "tx queue: …" |
| ⑥ 망 + 교환기 | 같은 Wi-Fi·맥북 교환기 3~7(어림) / 터널(Phase 4) 한쪽 길 약 46 | 망 | Phase 0: 터널 왕복 중앙값 92.6ms |
| ⑦ 지터 버퍼 | 40~60 (끊긴 뒤 최대 200, 1초 뒤 되돌아옴) | PL3b-D5: 시작 60ms, 최소 40 | 맥 시험 "rx: the jitter buffer holds 2..3 frames in a steady stream" |
| ⑧ 링버퍼(20ms 프레임 → 16ms 묶음) | 0~20 | PL3b-D6: 한 묶음 미만일 때만 한 프레임 푼다, 채우는 중의 무음은 넣지 않는다 | 맥 시험 "rx: it never decodes further ahead than one block + one frame" |
| ⑨ 출력 DMA 가 앞서 쥔 양 | **32** (3a 는 80) | PL3b-D4: DMA 6칸 → 3칸 | 드라이버 소스 `i2s_common.c`(빈 칸 대기열 = 칸 수 − 1) |
| ⑩ DAC·앰프·스피커·공기(1m) | 약 1~3 | 물리 | — |
| **①+③+⑥+⑦+⑧ (같은 Wi-Fi)** | **가운데 75** (통화를 여는 순간에 따라 67~85, 두 기기의 시계가 0.02% 어긋나 흘러가는 동안 54~126) | | **맥 시뮬레이션**(64회 × 60초, 1시간 × 2) |
| **합 — 같은 Wi-Fi(Phase 3b)** | **약 115** (95~170) + ② | 위 + ④ 6.5 + ⑨ 32 + ⑩ 2 | 어림. 기기에서는 Task 8 Step 7 의 지연 재기(`vping`)로 잰다 |
| 합 — 터널 경유(Phase 4 예상) | 약 175~195 + ② | ⑥ 이 45~75 일 때 시뮬레이션 133 + 6.5 + 32 + 2 | 어림 |
| 통화 설계서 §9.6 의 추정 / §6 의 합격선 | 200~350 / 500 | | 추정치 안쪽이다 |

- 가장 큰 덩어리는 ⑦ 지터 버퍼(40~60)와 ⑨ 출력 DMA(32)다. ⑨ 를 3a 그대로(6칸) 두면 합이 약 165ms 가 된다 — 3칸으로 줄이는 것이 이 계획의 첫 결정이다(3a 최종 검수의 권고).
- ⑦ 을 40ms 로 시작하면 20ms 를 더 줄일 수 있지만 설계서 값(60ms 로 시작)을 따랐다 — 기기에서 `v_jb`·`v_lost` 를 본 뒤 줄일지 정한다(`jitter_buffer.h` 의 `START_FRAMES`).
- **기기에서 재는 법**(Task 8 Step 7): 한쪽이 마이크 대신 "0.1초 삑"을 보내고, 상대는 자기 스피커 되먹임 칸(앰프를 꺼도 잡힌다)을 그대로 돌려보낸다. 삑을 보낸 때부터 돌아온 삑을 해제한 때까지 = `VPING rt_ms`. 여기에는 ③④⑤⑥⑦⑧⑨ 가 한 번, 상대의 ①③④⑤⑥ 과 이쪽의 ⑦ 이 한 번 더 들어 있다 → **한쪽 길(입 → 귀) ≈ rt_ms ÷ 2 + 25ms + ②**. 예상 `rt_ms` 는 약 200(160~260).
- **실측(2026-10-05, 위 "합"과 나란히 — 상세와 조건은 통화 설계서 §9.6 정정)**: 위 예산의 한쪽 길 합 약 115ms(95~170) + ② 는 **낙관적이었다**. `VPING` 왕복 → 한쪽 길 어림(÷ 2 + 25ms): **밤**(`d77ae98`, 스피커 끔) 1번 방향 253ms → 약 151ms · 2번 방향 285ms → 약 167ms(최종 검수 판독 150~185ms, 합격선 왕복 80~320 충족), `v_jb` 깊이 2~4 / 목표 2~3. **아침**(`5935ddd`, 절전 끈 뒤) 왕복 540~573ms → 약 0.3초(합격선 **미달**), `jb=8~10/8`. 사용자 답: "메아리·지연"을 아쉬운 점으로 고름. 재지 못함: ② 에코 제거 안의 지연 · ④ Opus 알고리즘 지연 · ⑩ 공기. 가장 큰 덩어리로 예상한 ⑦ 지터 버퍼는 실제로 아침에 목표가 8 까지(160ms) 올라갔다 — 3c 의 재료.

## CPU·메모리 예산

**CPU** (한 코어 기준, Phase 0 실측에서 출발)

| 일 | 어디서 | 시간 | 몫 | 근거 |
|---|---|---|---|---|
| 에코 제거 한 묶음 | `v_cap`(코어 1, 우선순위 7) | 평균 2795µs / 최대 3330µs, 16ms 마다 | 17.5% | Phase 0 H4 |
| Opus 압축 한 프레임(복잡도 0) | 같은 태스크 | 평균 2173µs, 20ms 마다 | 10.9% | Phase 0 H4 (복잡도 5 는 6863µs — 쓰지 않는다) |
| Opus 해제 한 프레임 | `v_rx`(코어 1, 7) | 평균 306µs, 20ms 마다 | 1.5% | Phase 0 H4 |
| 출력 한 묶음(신호음 + 섞기) | `audio`(코어 1, 6) | 평균 약 300µs, 16ms 마다 | 약 2% | 3a 기기 실측(`aud_us` 315/603) |
| **합** | 코어 1 | | **약 32%**(Phase 0 은 방 잡음 입력에서 34~37% 까지 봤다) | 화면·교환기와 함께일 때는 Task 8 Step 8(조용한 방), 말소리 입력일 때는 Task 9 Step 4 가 처음 잰다 |
| 보내기·받기(소켓) | `v_tx`·`relay_ws`(코어 0) | 프레임당 수백 µs(어림) | 몇 % | 재지 않았다 |

- 합격 기준(Task 8, `voice_check.py`): 부하 = `v_aec_us` 평균 ÷ 16000 + (`v_enc_us` 평균 + `v_dec_us` 평균) ÷ 20000 ≤ **50%**(Phase 0 의 합격선 그대로), 최대값이 한 묶음을 넘지 않는다(`v_aec_us` 최대 < 16000, `v_enc_us`·`v_dec_us` 최대 < 20000), `aud_late` = 0(출력이 48ms 넘게 늦은 적 없음), `v_under` = 0(해제 태스크가 굶은 적 없음), 터치 대조 일치, 통화 중 `RELAY up=0` 0줄.

**내부 메모리** (3a 기기 실측: 대기 중 `heap_int` 약 257KB, 최저 253,875 B. 기준 `heap_int_min` ≥ 100000)

| 3b 가 더 쓰는 것 | 크기 | 어디 | 근거 |
|---|---|---|---|
| `v_cap` 스택 | 48KB | 내부 | Phase 0 시험 펌웨어의 소리 태스크와 같은 값(에코 제거·압축이 여기서 돈다) |
| `v_tx` 스택 | 8KB | 내부 | 교환기 끊김 핸들러(와 Phase 4 의 TLS 쓰기)가 이 스택에서 돌 수 있다 — `relay` 태스크와 같은 값 |
| `v_rx` 스택 | 48KB | **PSRAM** | 내부 메모리를 아끼려고(PL3b-D15) |
| 정적 변수(.data·.bss·내부 코드) | +5.4KB | 내부 | 사본 빌드의 `idf.py size`: DIRAM 144,134 → 149,556 B |
| 마이크 4칸·에코 제거 버퍼(16바이트 정렬) | 3.6KB | 내부 | 시험 펌웨어와 같은 할당 |
| I2S 수신 DMA 3칸 × 2048 B(송신은 칸 수가 반으로 줄지만 입력을 열 때 슬롯이 32비트로 바뀌어 크기는 비슷하다) | 약 +6KB | 내부(DMA) | 드라이버 소스 |
| 대기열·뮤텍스·코덱 객체 | 약 1.5KB | 내부 | 어림 |
| 에코 제거·Opus 압축기·해제기의 상태 | **약 65~80KB(역산)** | 내부(일부는 PSRAM) | Phase 0 시험 펌웨어의 `heap_int` 는 약 162KB, 3a 펌웨어는 약 257KB 다(둘 다 Wi-Fi 에 붙은 뒤). 두 펌웨어의 태스크 스택·DMA·버퍼 차이를 맞춰 보면 이만큼이 남는다(구성이 달라 ±15KB) — 닫힌 라이브러리라 따로 재지 못했다 |
| 지터 버퍼·링버퍼(15KB), 보내기 대기열(2KB) | 17KB | PSRAM | `voice.cpp` 가 PSRAM 에 만든다 |
| **대기 중 `heap_int` 예상** | **약 105~120KB**(최저는 이보다 3~5KB 낮다) | | **기준 100000 에 빠듯하다** |

- 그래서 해제 태스크(`v_rx`)의 스택을 PSRAM 에 뒀다(내부에 두면 예상이 57~72KB 로 기준 아래다). 무거운 쪽(`v_cap`)은 Phase 0 에서 잰 조건 그대로 내부 메모리다.
- Task 8 에서 `heap_int_min` 이 100000 아래로 나오면: 멈추지 않고 값을 보고에 적는다(`voice_check.py` 가 `??` 줄과 `check rc=3` 으로 알린다. 전화기는 돈다 — Phase 0 시험 펌웨어는 TLS 중 116KB 였다). 60000 아래면 멈춘다. 미리 정한 탈출구는 ① `v_cap` 스택도 PSRAM 으로(`voice.cpp` 의 `xTaskCreatePinnedToCore` 를 `…WithCaps` 로 — 약 +48KB. 그때는 `v_aec_us`·`v_enc_us` 를 Phase 0 값 2795·2173µs 와 대조한다) ② Opus 의 할당을 PSRAM 으로(부품의 `media_lib_module_calloc` 등이 약한(weak) 함수라 펌웨어에서 다시 정의할 수 있다). 어느 쪽이든 다시 빌드·플래시가 필요하므로 컨트롤러가 정한다.
- Phase 4 에서 `wss://`(TLS)를 쓰면 연결이 살아 있는 동안 내부 메모리를 약 25~35KB 더 쓴다(Phase 2 조사) — 그때 위 탈출구 ①이 필요할 수 있다.
- 각 스택의 합격 기준: `stk_vcap`·`stk_vrx`·`stk_vtx`·`stk_audio`·나머지 ≥ 1024. `stk_vcap`·`stk_vrx` 의 실제 값은 Phase 5 에서 스택을 줄이는 근거가 된다(보고에 적는다).

## 이 계획에서 내린 결정 (PL3b-D)

통화 설계서의 결정(PH-D1~D9), 3a 의 결정(PL3a-D1~D16) 가운데 바꾼 것은 여기에 적었다(D2 → PL3a-D10, D4 → PL3a-D7·D8). 나머지는 그대로 따른다.

| ID | 결정 | 이유 | 비용 | 탈출구 |
|---|---|---|---|---|
| PL3b-D1 | **`TONE` 줄은 app 태스크가 찍는다.** 출력 태스크는 찍을 것을 길이 16 대기열에 남기기만 한다(3a 최종 검수 M-1) | 출력 태스크(우선순위 6)의 `printf` 는 시리얼이 느리거나 `shot` 의 긴 줄과 겹치면 50ms~0.36초 멈춘다 — 신호음에는 무해했지만 목소리는 그대로 끊긴다 | `TONE` 줄이 한 바퀴(20ms, 화면을 내보내는 중이면 60~80ms) 늦게 찍힌다. app 이 오래 멈추면 줄이 빠질 수 있다(그때 `TONE ? lost=N` 한 줄) | `tone_out.cpp` 의 `fill` 안에서 다시 `printf` |
| PL3b-D2 | **소리 장치가 안 열리면 3번까지 재시작해 다시 켜 보고**(`boot_guard(…, "audio")`), 그래도 안 되면 소리 없이 계속 돈다. 소리 경로의 어느 조각(`audio_out`·`tone_out`·`voice`)이 실패해도 `audioout::fail()` 한 곳으로 모여 `AUDIO FAIL <단계>` + `STAT audio=0` 이 된다. 실패 뒤의 정리는 **앰프를 끄는 것까지만**. 소리 장치는 Wi-Fi·교환기보다 **먼저** 연다. 실패를 일부러 내 보는 시리얼 명령 `fault audio` 를 둔다. 재시작 직전의 줄(`PHONE audio-failed restart N`)은 시리얼로 나간 뒤에 재시작한다(0.2초 — 기기 시험이 그 줄을 센다)(3a 최종 검수 M-2·M-3, 판정 P3A-R10) | 통화 설계서 §9.7("소리 초기화 실패 → 재시작 가드")의 뜻. 3a 의 "다시 켜 보지 않는다"(PL3a-D10)는 사용자에게 물은 적이 없었다. I2S·코덱을 닫는 코드는 이 기기에서 돈 적이 없고, 닫다가 죽으면 `boot_guard` 가 세지 못하는 재부팅 고리가 된다. 실패 경로가 기기에서 한 번도 돈 적이 없었다 | 코덱이 영영 죽은 기기는 부팅 때 3번 재시작(약 20초) 뒤에야 조용히 돈다. 화면에는 표시가 없다(Phase 5). `fault audio` 는 사람이 켜면 그 기기의 소리를 한 번 죽인다(네 번째 부팅에서 스스로 풀린다) | `phone_main.cpp` 에서 `boot_guard(audioout::ready(), "audio")` 한 줄을 지우면 3a 동작. `fault audio` 는 명령 한 줄과 함수 둘을 지운다 |
| PL3b-D3 | **신호음과 목소리는 더한다(넘치면 ±32767 에서 자른다). 누가 이기는 규칙은 없다.** 섞기는 순수 함수 `pcm::mix_sources`(3a 최종 검수 M-7) | 통화 중에는 신호음이 없고, 겹치는 것은 통화가 열리고 끝나는 순간의 몇 ms 뿐이다(PL3b-D13 이 그 구간을 짧게 만든다). 에코 제거의 기준은 하드웨어 되먹임(마이크 칩의 칸 1)이라 섞인 뒤의 소리가 그대로 기준이 된다 — 소프트웨어로 기준을 만들 필요가 없다 | 둘 다 큰 순간에는 잘려서 찌그러질 수 있다(지금 크기에서는 종료음 3700 + 목소리라 넘치지 않는다) | 목소리가 있을 때 신호음을 줄이는(덕킹) 규칙을 `mix_sources` 에 넣는다 |
| PL3b-D4 | **출력 DMA 6칸 → 3칸**(I2S 송수신 공통, 한 칸 = 256표본). 출력은 늘 (칸 수 − 1) × 16 = **32ms** 앞서 쓰여 있다. 늦은 횟수를 센다(`aud_late`: 앞 묶음 뒤 48ms 넘어서야 썼다) | 3a 의 6칸은 80ms 를 통화 지연에 그대로 더한다(3a 최종 검수의 첫 권고). 벨 끄기 뒤 소리가 그치는 것도 0.1초 → 0.03초가 된다 | 출력 태스크가 48ms 넘게 늦으면 스피커가 끊긴다(6칸이면 96ms 까지 버틴다). 마이크도 3칸이라 캡처 태스크가 48ms 넘게 못 읽으면 그만큼 잃는다. **이 칸 수로 이 기기에서 돌려 본 적은 없다**(Phase 0·3a 는 6칸) | `audio_out.cpp` 의 `DMA_DESC` 를 4(48ms)나 6으로 |
| PL3b-D5 | **지터 버퍼는 설계서 §9.6 값 그대로**(60ms 로 시작, 40~200ms, 300ms 를 넘으면 오래된 것부터 버림, 다 떨어지면 3프레임 메우고 무음) — Phase 2 때 써 둔 초안(`jitter_buffer.h`, 시험 42개)을 저장소로 옮기되 **결함 하나를 고쳤다(JB-D2)**: 다 떨어져서 메울 때 순번을 넘기지 않는다. `pop` 은 20ms 마다가 아니라 "출력이 한 묶음 가져가 링버퍼가 모자랄 때" 부른다 | 초안은 여유가 0 이 된 뒤 "메움 3번 → 그 3프레임이 뒤늦게 와 버려짐 → 다시 여유 0"을 끝없이 되풀이했다(받는 쪽 시계가 0.1% 빠르기만 해도 2시간에 4만 5천 프레임을 잃는다 — 긴 시뮬레이션이 잡았다). 고친 뒤에는 20초마다 한 프레임만 메운다 | 메운 만큼 재생이 한 프레임씩 뒤로 밀린다(지연이 는다) → 깊어지면 1초 뒤 목표 깊이로 줄이는 기존 규칙이 되돌린다. 순서 뒤바뀜·중복 처리(전송이 TCP 라 실제로는 오지 않는다)에 13KB(PSRAM) | `jitter_buffer.h` 의 상수(`START_FRAMES`·`MIN_FRAMES`·`OVERFLOW_FRAMES`·`MAX_CONCEAL`) |
| PL3b-D6 | **20ms 프레임 ↔ 16ms 묶음 사이는 잠금 없는 링버퍼**(쓰는 쪽 = 해제 태스크, 읽는 쪽 = 출력 태스크). 해제 태스크는 "링버퍼가 한 묶음(256)보다 적을 때"만 한 프레임을 풀며, 출력 태스크가 한 묶음 가져갈 때마다 + 프레임이 도착할 때마다 깨운다. 채우는 중(통화 시작·끊긴 뒤)의 무음은 링버퍼에 넣지 않는다 | 공급자는 출력 태스크 안에서 멈추면 안 된다(복사만). 미리 많이 풀어 두면 그만큼이 지연이다(링버퍼에 256~575표본 = 0~20ms). 무음을 넣으면 그 무음이 첫 목소리 앞에 줄을 선다(시뮬레이션에서 지연 109 → 85ms) | 해제 태스크가 16ms 안에 못 돌면 출력이 0 을 낸다(`v_under` 로 센다) | `voice_core.h` 의 `pump`(문턱을 두 묶음으로 올리면 여유 +16ms, 지연 +16ms) |
| PL3b-D7 | **빠진 프레임은 직접 메운다**: 직전 프레임을 끝에서 거꾸로 되짚으며(거울 — 이어 붙는 자리에 계단이 없다) 3프레임(60ms)에 걸쳐 0 까지 줄인다. 메운 뒤·건너뛴 뒤의 첫 프레임은 앞 5ms 를 넘겨 받는다. 통화가 열릴 때 해제기는 `esp_opus_dec_reset` 으로 처음 상태로 | **esp_audio_codec 2.5.0 의 Opus 해제기는 손실 은닉을 해 주지 않는다**: ① README 는 은닉(PLC)을 SBC·LC3 에만 적는다(`README.md:195, 204`) ② `esp_opus_dec.h` 에 `enable_plc` 같은 설정이 없다(SBC·LC3 헤더에는 있다) ③ `esp_opus_dec_decode` 를 역어셈블하면 `raw->frame_recover` 를 읽지 않고(`raw` 에서 읽는 것은 `buffer`·`len` 뿐), `raw->buffer` 가 NULL 이면 `ESP_AUDIO_ERR_INVALID_PARAMETER` 로 돌아간다 — libopus 의 은닉 진입(`data == NULL`)에 닿을 길이 공식 사용법에 없다. 설계서 §9.6 의 정정("안 되면 직전 프레임을 감쇠시켜 반복")대로 | 은닉 품질은 libopus 것보다 못하다(전송이 TCP 라 실제 손실은 없고, 망이 멈췄을 때·시계가 어긋날 때만 쓰인다). 해제기는 빠진 프레임을 모른 채 다음 프레임을 풀어 한두 프레임 소리가 어긋날 수 있다 | 길이 0 인 입력을 주면 래퍼가 그대로 `opus_decode_native(len = 0)` 을 부른다(같은 역어셈블) — libopus 라면 은닉이 돈다. **기기에서 확인하지 않았다** |
| PL3b-D8 | **음성은 전용 태스크(`v_tx`)가 보내고, 밀리면 버린다**: 보내기 대기열 10프레임(200ms — 설계서 §9.6), 가득 차면 가장 오래된 것을 버리고, 200ms 넘게 묵은 것은 보내지 않고, 보내기가 실패하면 쌓인 것도 버린다. 소켓 쓰기 시간 제한은 신호와 같은 2초. `relay_link` 의 보내기와 "끊고 다시 붙기"를 같은 잠금으로 묶는다(RL-D10 — Phase 2 최종 검수가 "음성 송신 전 필수"라 한 것) + stop 도중의 `LINK_UP` 뒤에 `LINK_DOWN` 을 올린다(RL-D11, 3a 최종 검수 M-8) | 보내기는 Wi-Fi 가 막히면 멈춘다 — 캡처 태스크가 보내면 마이크·에코 제거가 멎고, app 이 보내면 화면이 멎는다. 시간 제한이 짧으면 부품이 연결을 끊는다(Wi-Fi 가 잠깐 막힐 때마다 통화가 끊긴다). 부품의 `start()` 는 전송 객체를 지우고 새로 만든다 — 그때 다른 태스크가 보내는 중이면 해제된 메모리를 쓴다 | 음성 보내기가 소켓에서 막혀 있으면(최대 2초) 끊기 신호도 그 잠금을 기다린다. 2초 안에 못 보내면 상태 기계가 3초 뒤 연결을 끊고 다시 붙어 양쪽 통화를 끝낸다(FSM-D1 — **끊기는 어느 쪽이든 5초 안에 닿는다**). 마이크는 끊기를 누른 그 자리에서 닫힌다(PL3b-D13). TCP 송신 버퍼(약 5.7KB ≈ 1초 분량)에 이미 들어간 것은 버리지 못한다 — 망이 풀리면 한꺼번에 가고, 받는 쪽 지터 버퍼가 버린다 | `voice_core.h` 의 `TX_QUEUE`·`TX_STALE_MS`. 잠금을 빼면 3a 동작 |
| PL3b-D9 | **통화가 열린 뒤 마이크의 앞 8묶음(128ms)은 보내지 않는다** | 그 안에 ① 받기 전에 DMA 에 쌓여 있던 소리(3칸 = 48ms — 받기 전의 소리가 새면 안 된다, PH-D6) ② 아직 스피커에서 나고 있는 연결음 꼬리(멈춤 5ms + 앞서 쓴 32ms)가 들어 있다. 참고 구현(xiaozhi)은 120ms 를 버린다 | 통화가 열린 직후 0.13초의 말은 가지 않는다(상대의 지터 버퍼도 그동안 채우는 중이다) | `voice_core.h` 의 `DISCARD_BLOCKS` |
| PL3b-D10 | **크기**: 코덱 음량 100 그대로(PL3a-D2) · 마이크 증폭 **24dB** · 보내는 쪽에서 에코 제거 뒤 **4배(+12dB)** · 재생은 그대로(1배) | Phase 0: 음량 100 에서 마이크 30dB 는 에코 꼭대기가 포화(0.05%)하고 24dB 는 포화 0%·잔향 감소 41.8dB 였다 — 포화한 에코는 에코 제거가 지우지 못한다. Phase 0 의 겹쳐 말하기 녹음에서 사용자 말소리는 30dB 에서 약 −28dBFS → 24dB + 4배면 약 −22dBFS = 사용자가 "적당"이라 한 재생 크기(시험 음성 −16.9dBFS 를 음량 90 으로) | 한 사람·한 거리의 녹음 하나에서 나온 값이다 — 사람에 따라 작거나 클 수 있다(Task 9 에서 묻는다). 4배는 마이크 잡음과 남은 에코도 4배로 키운다. 큰 소리는 잘린다 | `audio_out.cpp` 의 `MIC_GAIN_DB`(3dB 단위), `voice_core.h` 의 `TX_GAIN_Q8`·`PLAY_GAIN_Q8`(256 = 1배) |
| PL3b-D11 | **진단용 소리 고르기 `vsrc aec\|mic\|ref\|tone`**. 통화 중에만 듣고, 통화가 끝나면 `aec`(평소)로 돌아간다. `mic` = 에코 제거를 거치지 않은 마이크, `ref` = 스피커 되먹임 칸, `tone` = 1kHz 시험음(크기 2000) | 사람 없이 재려면(의뢰의 권고): 시험음을 보내면 "보내는 쪽 압축 → 교환기 → 받는 쪽 해제 → 코덱 출력"이 받는 쪽 `v_spk`·`v_ref` 로 확인된다. `ref` 를 돌려보내면 지연이 앰프를 끈 채로도 재진다(PL3b-D12) | 진단 코드가 제품에 들어 있다(시리얼로만 닿는다). **두 기기를 모두 `mic` 로 두고 스피커를 켜면 하울링이 난다** — 시험은 한쪽만 | 명령과 `voice_core.h` 의 `Src`·`pick` 을 지운다 |
| PL3b-D12 | **지연 재기 `vping`**: 마이크 대신 "쉼 0.4초 → 삑 0.1초(1kHz, 크기 4000) → 쉼 0.5초"를 보내고, 돌아온 소리에서 크기 1500 을 넘는 첫 표본을 풀 때까지의 시간을 `VPING rt_ms=` 로 찍는다(3초 안에 못 찾으면 `VPING timeout`). 상대는 `vsrc ref`(전기적으로 돌려보냄) 또는 `vsrc mic`(스피커 → 공기 → 마이크)로 둔다 | 입 → 귀 지연을 기기에서 숫자로 본다(귀로는 0.1초와 0.2초를 가르기 어렵다). 삑을 "마이크에 들어온 소리"로 치므로 보내는 길 전체가 들어간다 | 재는 것은 왕복이고 마이크 묶음 대기·에코 제거 내부 지연·공기는 빠진다(한쪽 길 ≈ ÷ 2 + 25ms 는 어림). 재는 1초 동안 그 기기의 목소리는 가지 않는다. 되먹임 칸의 잡음이 크면 잘못 잡는다(80ms 미만이면 `voice_check.py` 가 잘못 잡은 것으로 친다) | 명령과 `voice.cpp` 의 지연 재기 줄, `voice_core.h` 의 `PingGen`·`onset` 을 지운다 |
| PL3b-D13 | **통화가 열리고 끝나는 순간의 순서.** `apply()` 는 한 묶음의 할 일에서 ① 신호음 명령 ② 목소리 켜기/끄기 ③ 신호 보내기 순으로 한다. **열릴 때**(거는 쪽은 `0x180` 한 묶음에 연결음 멈춤 + 목소리 켜기): 연결음은 5ms 에 줄어 멎고 이미 DMA 에 든 32ms 는 그대로 난다. 목소리 공급자는 그 묶음부터 소리를 내지만 지터 버퍼가 60ms 를 채울 때까지 무음이고 첫 프레임은 5ms 에 걸쳐 커진다 → 실제로 겹치는 것은 "연결음 꼬리 + 무음"뿐. 보내는 쪽은 앞 128ms 를 버린다(PL3b-D9). **끝날 때**(`0x240`·`0x248` 한 묶음에 목소리 끄기 + 종료음): 목소리는 그 묶음에서 5ms 에 걸쳐 줄이고 남은 것(링버퍼·지터 버퍼)은 버린다. 종료음은 같은 묶음에서 제 모양대로 시작한다 — 겹치는 5ms 는 더해진다. 이미 DMA 에 든 목소리 32ms 는 그대로 난 뒤 종료음 | 받기·끊기를 누르면 보내기가 막혀 있어도 벨·마이크가 그 자리에서 멈춰야 한다(PL3a-D13 과 같은 이유). 연결음 꼬리가 첫 목소리에 섞이거나 상대에게 가지 않게, 끝날 때 딸깍 소리가 나지 않게 | 끝날 때 목소리의 마지막 32ms(이미 내보낸 것)와 종료음의 첫 32ms 는 스피커에서 이어서 난다(겹치지는 않는다). 통화가 열린 직후 약 0.1초는 양쪽 다 조용하다 | 순서는 `app.cpp` `apply()` 의 세 줄. 길이는 `voice_core.h` 의 `FADE`·`DISCARD_BLOCKS` |
| PL3b-D14 | **조용한 모드(`quiet on\|off`)와 혼자 하는 점검(`selftest`).** `quiet on` 은 스피커 앰프를 끄고 NVS 에 남긴다(재부팅 뒤에도 유지). `selftest` 는 통화 밖에서 시험음(1kHz, 크기 2000, 0.64초)을 내며 마이크 칸·되먹임 칸·에코 제거 뒤의 세기를 잰다. **기기 검증을 둘로 나눈다**: Task 8 = 조용한 모드에서 기록으로(사용자 없이 가능), Task 9 = 소리를 켜고 사용자와 | Phase 0 실측: 앰프를 꺼도 되먹임 칸에는 내는 소리가 그대로 잡히고(1kHz·8000·음량 100 에서 2370) 마이크 칸은 조용하다(3~4). 즉 **소리를 내지 않고도 "코덱까지 소리가 나갔다"를 기록으로 볼 수 있다** — 3a 가 귀에만 맡겼던 것. 밤에 사용자 없이 벨이 울리는 시험을 돌리지 않기로 했다(P3A-R11) | 켜 둔 채 잊으면 벨이 울리지 않는다(`STAT amp=0`, 부팅 줄 `AUDIO ready … amp=0` 으로 보인다 — Task 9 가 푼다). 다시 올린 직후의 첫 부팅은 앰프가 켜진 채다("퍽" 소리 한 번) | 명령과 NVS 열쇠 `quiet` 를 지운다 |
| PL3b-D15 | **태스크 셋**: `v_cap`(코어 1, 우선순위 7, 스택 48KB 내부) 마이크 → 에코 제거 → 압축 / `v_rx`(코어 1, 7, 48KB **PSRAM**) 해제 / `v_tx`(코어 0, 5, 8KB 내부) 보내기. 출력 태스크 `audio`(코어 1, 6)는 3a 그대로. 에코 제거·Opus 는 `v_cap` 태스크가 자기 스택에서 만든다(시험 펌웨어도 48KB 소리 태스크 안에서 만들었다 — 부팅 태스크의 스택은 8KB 다) | Phase 0 은 한 태스크(코어 1, 48KB, 화면 태스크보다 위)가 다 했다 — 그 배치를 유지하되, 재생은 3a 의 "끌어가는" 출력 태스크에 맞추고 보내기는 막힐 수 있어 떼었다. 소리 태스크들은 화면(app 4)·터치(5)보다 위라 화면을 내보내는 60~80ms 동안에도 박자를 지킨다. 한 묶음의 일(약 5ms)이 16ms 보다 짧아 화면을 굶기지 않는다. `v_rx` 스택을 PSRAM 에 둔 이유는 "CPU·메모리 예산" | **PSRAM 스택의 태스크를 이 기기에서 돌려 본 적이 없다**(설정상 가능 — `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=y`, 코드도 PSRAM 에서 실행 중이다). 태스크 사이에 대기열·통지가 생겼다 | `voice.cpp` 의 `RX_STACK_CAPS` 를 `MALLOC_CAP_INTERNAL` 로(`RX_STACK` 은 24KB 로 — 참고 구현 xiaozhi 는 Opus 압축·해제를 24KB 태스크 하나로 돌린다. 내부 메모리 −24KB). 그 밖의 상수 `*_CORE`·`*_PRIO`·`*_STACK` |
| PL3b-D16 | **시리얼 줄**: `VOICE on\|off`(상태가 실제로 바뀔 때, 그 원인인 `FSM` 줄 바로 앞), `VOICE ready …`(부팅), `AUDIO ready` 끝에 ` dma= mic= amp=`, `STAT` 끝에 ` amp= aud_late= voice= vsrc= v_cap= v_tx= v_txdrop= v_txfail= v_rx= v_play= v_lost= v_drop= v_under= v_jb= v_aec_us= v_enc_us= v_dec_us= v_mic= v_ref= v_aec= v_spk= v_rderr= stk_vcap= stk_vrx= stk_vtx=`, `SELFTEST …`, `VPING …`. `v_*` 의 셈·시간은 이번(또는 직전) 통화의 것이고 `v_cap` 만 부팅 뒤 누적 | 목소리 경로를 기기 로그로 본다. 기존 줄은 그대로라 도구·앞 계획의 정규식이 그대로 맞는다(도구 시험이 펌웨어 소스의 형식 문자열을 읽어 고정 — 3a 는 손으로 만든 줄이었다) | `STAT` 한 줄이 약 560자가 됐다 | 칸을 지운다 |
| PL3b-D17 | **에코 제거·Opus 설정은 Phase 0 에서 잰 것 그대로**: `AEC_MODE_VOIP_HIGH_PERF`·필터 길이 4·묶음 256 / Opus 16kHz 모노·20ms·**24kbps 고정 비트율**·VOIP·복잡도 0, 오류 정정(FEC)·무음 생략(DTX)·가변 비트율 끔 | CPU 30~37% 와 잔향 감소 29~42dB 가 이 설정에서 잰 값이다. `FD_HIGH_PERF` 는 결과가 깨졌다. DTX 를 끄면 프레임이 20ms 마다 빠짐없이 와서 "안 온 프레임 = 늦은 것"이 분명하다 | 조용할 때도 43kbps 가 흐른다. 복잡도 0 의 음질은 사용자 귀 판정을 받은 적이 없다(Task 9) | `voice.cpp` 의 상수 |
| PL3b-D18 | **빌드도 올리기도 한 번만**: 빌드는 Task 7 에서, 그 앞에 지금 `build/`(`5d9935f`)를 `out/fw_prev_5d9935f/build/` 로 보관한다. 기기 검증은 빌드 단계로 나누지 않고 한 바이너리로 하되, Task 8 안에서 불확실한 순서(장치가 열리나 → 코덱이 소리를 내나 → 시험음으로 디지털 경로 → 진짜 마이크와 에코 제거)로 확인한다. 과제 1~6 의 커밋 아홉 가운데 펌웨어 소스가 바뀌는 다섯 곳 모두 사본에서 빌드해 확인했다. `app.h`·`app.cpp`·`phone_main.cpp` 는 Task 1 에서도 Task 6 에서도 **한 커밋**이다 | 저장소의 `build/` 가 지금 기기에 올라가 있는 바이너리다. 중간 커밋이 빌드되지 않으면 되돌리기·둘로 나눠 찾기를 못 한다. 두 번 올리면(장치만 먼저) 기기마다 5분씩 더 들고, 실패한 단계의 이름(`AUDIO FAIL <단계>`)이 어느 쪽 문제인지를 이미 가른다 | 실행 담당은 중간 커밋의 빌드를 직접 보지 않는다(사본 기록을 믿는다). 첫 부팅이 실패하면 소리 장치 탓인지 목소리 모듈 탓인지를 로그로 갈라야 한다 | Task 3 의 커밋(장치 + 신호음만, 사본 0x244660)을 따로 빌드해 먼저 올려 본다(반드시 보관 뒤에) |
| PL3b-D19 | **교환기 코드를 바꾸지 않는다** | 통화 중의 이진 프레임(3~400바이트)을 상대에게 그대로 건네는 것, 통화 밖의 프레임을 버리는 것, 밀린 상대(송신 대기 4KB 초과)에게는 쌓지 않고 버리는 것이 이미 있고 시험돼 있다(`phone_core.py` `route_voice`·`should_drop_voice`). 우리 프레임은 62바이트다 | 교환기는 음성의 빈도를 제한하지 않는다(고장 난 기기가 쏟아내면 그대로 건넨다 — 받는 쪽 지터 버퍼가 300ms 에서 버린다) | — |
| PL3b-D20 | **"마이크가 열리지 않는다" = 통화 밖에서는 마이크를 읽지 않는다.** 마이크 칩과 I2S 수신은 부팅 때 한 번 열어 두고 닫지 않는다(Phase 0 과 같다). 캡처 태스크는 통화가 열리면 읽기 시작하고 끝나면 읽던 묶음(최대 16ms)을 끝으로 잠든다 | 입력 장치를 닫았다 열면 `esp_codec_dev` 가 송신을 다시 설정하고(그동안 벨·연결음이 10ms 씩 버려진다) 마이크 증폭이 지워진다 — 통화마다 그 길을 타는 것은 이 기기에서 해 본 적이 없다. 읽지 않으면 그 소리는 어디에도 가지 않는다(DMA 에서 덮여 사라지고, 다음 통화의 앞 128ms 는 버린다) | 마이크 칩과 ADC 가 늘 켜져 있다(전력). "하드웨어로 꺼져 있다"고는 말할 수 없다 | 통화 밖에서 `esp_codec_dev_set_in_mute(…, true)`(I2C 쓰기 둘, 잠금 안에서), 또는 입력 장치를 닫는다(다시 열 때 `mic_gain_apply`) |
| PL3b-D21 | **앰프를 코드가 쥔다**: `M5.begin()` 직후 앰프를 끄는 `amp_off_at_boot` + 소리 장치 실패(`fail`)는 잠금이 있으면 조건 없이 앰프를 끈다(`STAT amp=0` 이 실제와 같게) | M5GFX 가 부팅마다 확장칩에 써서 앰프를 켠다(`M5GFX.cpp:3137·3146`). **같은 `M5.begin` 안에서 M5Unified 의 `Power.begin()` 이 다시 끈다(`Power_Class.inl:153·175`) — 소스 판독으로 켜져 있는 시간은 0.3~1초(화면 초기화 동안)** 이므로 우리 코드가 직접 한 번 더 끄는 것은 이중 안전장치다. 처음 적은 이유("약 2초 켜져 있고 실패 경로에서는 켜진 채 남는다" — 최종 검수 지적 M-1, 검수 보고서의 Minor 번호)는 재검수가 과장으로 판독했다. **실제로 "퍽" 소리가 났는지는 누구도 확인하지 못했다(소스 판독만, 기기 미확인 — 레지스터를 읽는 코드가 없다).** 기기 증거는 부팅 로그 `amp forced off at boot` 뿐 | `app_main` 순서가 바뀜(I2C 잠금 생성을 `M5.begin()` 바로 뒤로), 시작 함수에 인자 하나 | `amp_off_at_boot` 호출 한 줄 삭제 + `fail` 의 조건 복원 |
| PL3b-D22 | **`vsrc mic\|ref` 는 스피커 앰프가 꺼져 있을 때만**(켜져 있으면 `ERR vsrc speaker-on`). `quiet off` 를 받으면 통화 중이던 `mic`·`ref` 출처는 `aec` 로 돌아간다 | 양쪽이 `ref`/`mic` 면 메아리 제거를 안 거치는 고리가 2초 안에 최대 크기가 된다(최종 검수 M-2) | 스피커를 켠 채 마이크 원음 진단이 안 된다(조용한 모드에서 하면 됨) | `voicecore::src_allowed` 를 늘 true 로 |
| PL3b-D23 | **`txgain <1..4>`**: 보내는 쪽 디지털 이득을 실행 중에 바꾼다(기본 4, 인자 없으면 지금 값) | 귀 판정 뒤 재빌드·재플래시 없이 조절한다(최종 검수 M-3: 고리 이득에 안전망이 없었다) | 저장되지 않는다 — 재부팅하면 4 | 명령을 안 쓰면 옛 4배와 같다 |
| PL3b-D24 | **`vol <0..100>`**: 코덱 재생 음량을 실행 중에 바꾼다(기본 100, 인자 없으면 지금 값, 100 을 넘기지 못한다) | 같은 이유 — 벨 크기는 `txgain` 으로 못 바꾼다. 낮추는 방향은 메아리를 줄이는 쪽이라 안전하다 | 저장되지 않는다 — 재부팅하면 100. 음량을 바꾸면 Phase 0 의 메아리 실측 조건(100)에서 벗어난다 | 명령을 안 쓰면 100 |
| PL3b-D25 | **`STAT` 맨 끝에 열 칸 + `VPING` 끝에 ` jb=깊이/목표`**: `v_urun v_ovf v_catch v_late v_dup v_bad aud_gap v_under_ms txgain vol` | `v_drop` 이 다섯 셈의 합이라 원인을 못 가렸다(최종 검수 M-6). `v_drop` = `v_ovf + v_catch + v_late + v_dup + v_bad` 는 한 줄 안에서 늘 같다 | 줄 길이 약 +110자. 옛 모양의 `VPING` 줄(끝을 `$` 로 묶은 정규식)은 맞지 않게 됐다 | 꼬리 한 literal 삭제(도구 시험 `STAT3B2_TAIL` 도 같이) |
| PL3b-D26 | **"굶음"(`v_under`) 셈은 `buffering_` 을 링 읽기 앞에서 뜬다** | 읽기와 판정 사이에 해제 태스크가 끼어드는 오탐 창을 닫는다(최종 검수 M-7). 맥 시험으로 고정하지 못했다(한 스레드 시험에 그 틈이 없다 — 주석으로만) | 없음 | 두 줄의 순서를 되돌린다 |
| PL3b-D27 | **캡처 시작은 세대 번호(`GenWatch`)로 판단한다** | 16ms 안에 켜기→끄기→켜기가 연달아 오면 `start()` 가 누락되는 경합(최종 검수 M-8) | 없음 | `mode != was` 만 쓰던 옛 동작 |
| N-D4 | **Wi-Fi 절전 끄기**: `esp_wifi_start()` 성공 직후 `esp_wifi_set_ps(WIFI_PS_NONE)`(`net_wifi.cpp`, 커밋 `5935ddd`). 성공하면 로그 `netwifi: wifi power save off`, 실패하면 `… FAILED: <이름> (0x..) — continuing`(부팅은 계속) | 기본값(모뎀 절전)이면 공유기가 절전 중인 기기에게 줄 자료를 약 0.3초 주기(DTIM)로 모아서 줘, 통화 중 받는 음성이 뭉쳐 왔다(2026-10-05 아침 30초 통화에서 받은 프레임의 13~18% 가 넘쳐 버려졌다). 이 조사는 Phase 2 때 조사 보고 `…-research/websocket-client.md` 가 "Phase 3 에서 확인할 만합니다"라고 적어 두었으나 3b 계획의 과제로 옮겨지지 않아 늦었다(원인 분류: 절차 부재). 효과는 로그가 지지하는 것만 단정: 통화 중 버림 비율 10~18% → 0%(1번)·2.6%(2번), `v_urun` 13~26 → 5~6. **크게 줄었으나 완전 해결은 아님**(수정 뒤에도 통화 중 `VPING` 285~573ms, 2번 `v_ovf` 약 4%) | Wi-Fi 칩이 늘 깨어 있어 전력을 더 쓴다(USB 전원 탁상기라 수용). 미확인: 칩(C6)이 혼자 리셋될 때 설정이 유지되는지 | 그 블록을 지우거나 `WIFI_PS_MIN_MODEM`, 배터리로 쓰면 통화 중에만 끈다 |

---

과제 묶음(검수가 들어가는 자리): **묶음 1 = Task 1~3**(정리 + 소리 장치), **묶음 2 = Task 4~7**(목소리 경로 + 빌드). 기기는 Task 8·9.

### Task 1: 3a 검수가 넘긴 정리 — 시험 보강, `relay_link` 잠금, 소리 실패 동작, `TONE` 줄

**Files:**
- Modify: `firmware/tab5_phone/main/relay_link.h`, `main/relay_link.cpp`, `main/audio_out.h`, `main/audio_out.cpp`, `main/tone_out.h`, `main/tone_out.cpp`, `main/app.h`, `main/app.cpp`, `main/phone_main.cpp`
- Test: `firmware/tab5_phone/tools/test_tones.cpp` (추가만)

**Interfaces:**
- Consumes: 지금 저장소의 펌웨어(코드는 `5d9935f` 와 같다), 3a 최종 검수의 M-1~M-8.
- Produces (뒤 과제가 이 이름·서명을 그대로 쓴다):
  - `relaylink::send_text`·`send_voice` — 서명 그대로. 이제 어느 태스크에서 불러도 된다(RL-D10: 보내기와 다시 붙기가 같은 잠금). `send_voice` 는 잠금을 20ms 만 기다린다.
  - `void audioout::fail(const char *stage)` — `AUDIO FAIL <stage>` 한 줄, `ready()` 는 false, 앰프를 끈다. `bool toneout::start()`, `void toneout::print_pending()`.
  - 시리얼: 명령 `fault audio` → `OK fault audio`. 부팅 때 소리 장치가 실패하면 `AUDIO FAIL <단계>` → `PHONE audio-failed restart <1~3>`(재시작) → 네 번째 부팅에서 `PHONE FAIL audio` 뒤 소리 없이 계속. `TONE` 줄의 모양은 그대로(찍는 태스크만 app 으로), 밀려서 빠지면 `TONE ? lost=<n>`.
  - 시작 순서: 화면 → **소리 장치** → Wi-Fi → 교환기 → app (전에는 Wi-Fi·교환기가 소리 장치보다 먼저였다).
- 커밋 셋. 모두 빌드된다(사본에서 확인: 기준선 0x242230 → 잠금 뒤 0x2423b0 → 소리 실패·`TONE` 줄 뒤 0x242680, 경고 0). 실행 담당은 여기서 빌드하지 않는다(PL3b-D18).
- `T1-05`(app·phone_main)는 `T1-03`·`T1-04` 와 **함께 적용하고 한 커밋**으로 한다 — `phone_main.cpp` 가 `audioout::fail` 을, `app.cpp` 가 `toneout::print_pending` 을 부른다.

- [x] **Step 1: 기준선을 확인하고, 시험 보강 패치(M-4)를 적용한다** — 이 시험들은 지금 코드에서 바로 통과한다(상태 기계가 이미 그렇게 동작한다 — 열거로만 있던 것을 전수·무작위로 고정하는 시험이다)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone/tools && SDK="$(xcrun --sdk macosx --show-sdk-path)" && for T in wifi_store call_fsm relay_proto device_id ui_layout tones; do clang++ -std=c++17 -Wall -Wextra -isysroot "$SDK" -I../main test_$T.cpp -o /tmp/t_p3b_$T 2> /tmp/t_p3b_$T.err; echo "$T: $(/tmp/t_p3b_$T | tail -1) (경고 $(wc -l < /tmp/t_p3b_$T.err | tr -d ' ')줄)"; done; PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider test_phone_ctl.py 2>&1 | tail -1; cd ~/tab5-toy-phone/server && PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider 2>&1 | tail -1
```
Expected: `wifi_store: 16 passed, 0 failed (경고 0줄)` · `call_fsm: 105 ok, 0 failed (경고 0줄)` · `relay_proto: 61 ok, 0 failed (경고 0줄)` · `device_id: 9 ok, 0 failed (경고 0줄)` · `ui_layout: 91 ok, 0 failed (경고 0줄)` · `tones: 84 ok, 0 failed (경고 0줄)` · `186 passed in …s` · `137 passed in …s`. 하나라도 다르면 멈춘다(BLOCKED — 출발점이 계획과 다르다).

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft && git apply "$D/patches/T1-01-tones-tests.patch" && git diff --numstat -- firmware/tab5_phone/tools/test_tones.cpp && cmp "$D/files/firmware/tab5_phone/tools/test_tones.cpp" firmware/tab5_phone/tools/test_tones.cpp && echo "같음: test_tones.cpp" && cd firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_tones.cpp -o /tmp/t_p3b_tones 2> /tmp/t_p3b_tones.err; echo "경고 $(wc -l < /tmp/t_p3b_tones.err | tr -d ' ')줄"; /tmp/t_p3b_tones | grep -c "^  FAIL"; /tmp/t_p3b_tones | tail -1
```
Expected: `48	0	firmware/tab5_phone/tools/test_tones.cpp`(삭제 줄 0), `같음: test_tones.cpp`, `경고 0줄`, `0`, `89 ok, 0 failed` (기존 84 + 새 5: 벨·연결음 상태를 떠나는 모든 길에 멈춤이나 종료음이 있다 — 모든 상태 × 사건 × 사유 × 시각, 그리고 무작위 사건 200만 개 동안 "벨은 `INCOMING` 밖에서 울리지 않는다, 연결음은 `RINGBACK` 밖에서 나지 않는다, 통화 중에는 신호음이 없다, 끈 벨은 다시 울리지 않는다").
이 시험이 실제로 잡는지는 계획 때 `call_fsm.h` 를 일부러 망가뜨려 확인했다(벨·연결음 도중 교환기가 끊겨도 멈춤을 내지 않는다 / 40초 가드가 벨을 멈추지 않는다 / 통화가 열릴 때 연결음을 멈추지 않는다 → 새 시험이 각각 1~3개 실패).

```diff
--- a/firmware/tab5_phone/tools/test_tones.cpp
+++ b/firmware/tab5_phone/tools/test_tones.cpp
@@ -432,6 +432,54 @@
         callee.ev(Ev::ENDED, Why::HANGUP); callee.run(1008);
         CHECK("scenario mute, mute again, accept, hang up: 'BELL on, BELL off, END on, END off' (accept adds no line)",
               callee.log == (Log{"BELL on", "BELL off", "END on", "END off"}));
+    }
+
+    // ════ Phase 3b 에서 더한 것(3a 최종 검수 M-4): 벨·연결음 상태를 "떠나는 모든 길"에 멈춤이 온다 — 열거가 아니라 전수로 ════
+    {
+        const State states[] = { State::OFFLINE, State::IDLE, State::DIALING, State::RINGBACK, State::INCOMING, State::ANSWERING, State::IN_CALL, State::ENDING };
+        const Ev evs[] = { Ev::LINK_UP, Ev::LINK_DOWN, Ev::PEER, Ev::RINGING, Ev::RING, Ev::CONNECTED, Ev::ENDED, Ev::TAP_CALL, Ev::TAP_CANCEL,
+                           Ev::TAP_ACCEPT, Ev::TAP_REJECT, Ev::TAP_HANGUP, Ev::TAP_MUTE, Ev::TICK };
+        const Why whys[] = { Why::NONE, Why::HANGUP, Why::REJECT, Why::TIMEOUT, Why::OFFLINE, Why::LOST };
+        int left_bell = 0, left_ringback = 0, bell_no_stop = 0, ringback_no_stop = 0;
+        for (State s : states) for (bool muted : {false, true}) for (Ev e : evs) for (Why w : whys) for (bool flag : {false, true})
+            for (uint8_t msg : {(uint8_t)0, (uint8_t)2}) for (int64_t at : {(int64_t)1400, (int64_t)100000}) {
+                if (muted && s != State::INCOMING) continue;
+                Fsm f = in_state(s);
+                if (muted) f.on(Ev::TAP_MUTE, 1300);
+                const State before = f.state;
+                const Out o = f.on(e, at, w, flag, msg);
+                const bool stops = (o.acts & (A_TONE_STOP | A_TONE_END)) != 0;
+                if (before == State::INCOMING && !muted && f.state != State::INCOMING) { ++left_bell; if (!stops) ++bell_no_stop; }
+                if (before == State::RINGBACK && f.state != State::RINGBACK) { ++left_ringback; if (!stops) ++ringback_no_stop; }
+            }
+        CHECK("state machine: every way out of INCOMING (bell not muted) carries a stop or the end tone", left_bell > 20 && bell_no_stop == 0);
+        CHECK("state machine: every way out of RINGBACK carries a stop or the end tone", left_ringback > 20 && ringback_no_stop == 0);
+    }
+    {
+        // 무작위로 오래(시간도 흐른다): 할 일을 from_acts 로 따라간 "지금 내야 할 소리"가 상태와 늘 맞는다
+        const Ev evs[] = { Ev::LINK_UP, Ev::LINK_DOWN, Ev::PEER, Ev::RINGING, Ev::RING, Ev::CONNECTED, Ev::ENDED, Ev::TAP_CALL, Ev::TAP_CANCEL,
+                           Ev::TAP_ACCEPT, Ev::TAP_REJECT, Ev::TAP_HANGUP, Ev::TAP_MUTE, Ev::TICK };
+        const Why whys[] = { Why::NONE, Why::HANGUP, Why::REJECT, Why::TIMEOUT, Why::OFFLINE, Why::LOST };
+        uint32_t seed = 20261004; auto rnd = [&seed](uint32_t m) { seed = seed * 1664525u + 1013904223u; return (seed >> 8) % m; };
+        Fsm f; Tone tone = Tone::NONE; int64_t now = 1000;
+        long bell_outside = 0, ringback_outside = 0, bell_state_silent = 0, ringback_state_silent = 0, tone_in_call = 0, muted_rings = 0, rings = 0, dials = 0;
+        for (int i = 0; i < 2000000; ++i) {
+            now += rnd(8) == 0 ? rnd(9000) : rnd(40);
+            const State before = f.state;
+            const Out o = f.on(evs[rnd(14)], now, whys[rnd(6)], rnd(4) != 0, (uint8_t)rnd(5));
+            from_acts(o.acts, &tone);
+            if (before != State::INCOMING && f.state == State::INCOMING) ++rings;
+            if (before != State::RINGBACK && f.state == State::RINGBACK) ++dials;
+            if (tone == Tone::BELL && f.state != State::INCOMING) ++bell_outside;
+            if (tone == Tone::RINGBACK && f.state != State::RINGBACK) ++ringback_outside;
+            if (f.state == State::INCOMING && !f.bell_muted && tone != Tone::BELL) ++bell_state_silent;
+            if (f.state == State::RINGBACK && tone != Tone::RINGBACK) ++ringback_state_silent;
+            if (f.state == State::IN_CALL && tone != Tone::NONE) ++tone_in_call;
+            if (f.state == State::INCOMING && f.bell_muted && tone == Tone::BELL) ++muted_rings;
+        }
+        CHECK("2 million random events: the bell never sounds outside INCOMING, the ringback never outside RINGBACK", rings > 1000 && dials > 1000 && bell_outside == 0 && ringback_outside == 0);
+        CHECK("2 million random events: while ringing (not muted) the bell is on, while RINGBACK the ringback is on", bell_state_silent == 0 && ringback_state_silent == 0);
+        CHECK("2 million random events: no tone during a call, and a muted bell never rings again", tone_in_call == 0 && muted_rings == 0);
     }
 
     printf("\n%d ok, %d failed\n", ok, bad);
```

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/tools/test_tones.cpp && git commit -m "test(phone): 신호음 시험 보강 — 벨·연결음을 떠나는 모든 길에 멈춤·종료음(전수 + 무작위 200만, 3a 검수 M-4)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [x] **Step 2: `relay_link` 잠금 패치를 적용하고 대조한 뒤 따로 커밋한다** (RL-D10·RL-D11 — 이 파일은 맥 시험이 없다. 읽어서 확인하고 보고에 적는다)

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft && git apply "$D/patches/T1-02-relay-link-lock.patch" && for f in relay_link.h relay_link.cpp; do cmp "$D/files/firmware/tab5_phone/main/$f" "firmware/tab5_phone/main/$f" && echo "같음: $f"; done; cd firmware/tab5_phone/main && grep -n "s_tx_lock\|send_frame(\|rose" relay_link.cpp | cut -c1-110
```
Expected: `같음:` 2줄, 이어서 15줄:
```
24://   RL-D10 (Phase 3b) 보내기(send_text·send_voice)와 "끊고 다시 붙기"의 stop → start 를 같은 잠금(s_tx_lock)으로 묶는다.
66:SemaphoreHandle_t s_tx_lock = nullptr;                           // RL-D10
187:            xSemaphoreTake(s_tx_lock, portMAX_DELAY);            // RL-D10: 보내는 중인 것이 끝나기를 기다린다(보내기 한 번은 최
189:            const bool rose = s_phase.exchange(Phase::IDLE) == Phase::UP;   // N-1 + RL-D11: stop 이 도는 동안 
190:            if (was_up || rose) post(In::Kind::LINK_DOWN);
193:            xSemaphoreGive(s_tx_lock);
200:            xSemaphoreTake(s_tx_lock, portMAX_DELAY);            // RL-D10: start 가 전송 객체를 새로 만든다
202:            xSemaphoreGive(s_tx_lock);
223:bool send_frame(bool text, const char *data, int len, TickType_t lock_wait)
225:    if (!s_ws || !s_tx_lock || s_phase != Phase::UP) return false;            // 붙어 있지 않으면 잠금을 기다리지도 않는다
226:    if (xSemaphoreTake(s_tx_lock, lock_wait) != pdTRUE) return false;
233:    xSemaphoreGive(s_tx_lock);
243:    s_tx_lock = xSemaphoreCreateMutexStatic(&tx_lock_buf);
269:    return send_frame(true, text, (int)strlen(text), TEXT_LOCK_WAIT);
275:    return send_frame(false, (const char *)frame, (int)len, VOICE_LOCK_WAIT);
```

| 확인할 것 | 코드 |
|---|---|
| 다시 붙기는 잠금을 쥔 채 `stop` → `start` 를 한다(그 사이에 보내는 태스크가 없다). 잠금을 쥐기 **전에** 상태를 `IDLE` 로 내려, 새로 보내려는 쪽이 잠금을 기다리지 않고 돌아선다 | `relay_link.cpp:186~193` |
| 보내는 쪽은 잠금을 쥔 **뒤에** 상태(`UP`)를 다시 본다 — 잠금을 기다리는 사이 다시 붙기가 내렸으면 보내지 않는다 | `send_frame` `228` |
| 잠금 순서가 한 방향이다: 잠금을 쥔 채 부르는 것은 부품의 `send`·`stop`·`start` 뿐이고, 부품의 이벤트 핸들러(웹소켓 태스크)는 이 잠금을 잡지 않는다 → 서로 기다리는 고리가 없다 | `on_ws_event` 에 `s_tx_lock` 이 없다(위 `grep` 의 15줄에 핸들러 줄이 없다) |
| 신호는 잠금을 2초까지, 음성은 20ms 까지만 기다린다 | `TEXT_LOCK_WAIT`·`VOICE_LOCK_WAIT`(`67`) |
| `stop` 이 도는 동안 접속이 완료돼 핸들러가 `LINK_UP` 을 올렸으면 그 뒤에 `LINK_DOWN` 을 올린다(M-8) | `189~190` |
| 시작 재시도(5초마다)의 `start` 도 잠금 안이다 | `200~202` |
| 잠금은 정적으로 만든다(실패하지 않는다) | `242~243` |

```diff
--- a/firmware/tab5_phone/main/relay_link.h
+++ b/firmware/tab5_phone/main/relay_link.h
@@ -28,8 +28,9 @@
 
 bool poll(In *out);                 // 받은 사건 하나를 꺼낸다. 없으면 false. 통화 진행 태스크에서만 부른다
 
-bool send_text(const char *text);                       // 신호 보내기. 붙어 있지 않거나 실패하면 false
-bool send_voice(const uint8_t *frame, size_t len);      // 음성 프레임 [순번 2][Opus] 보내기(Phase 3). 전용 송신 태스크에서만
+// 보내기와 reconnect 는 같은 잠금으로 묶여 있다(relay_link.cpp RL-D10) — 어느 태스크에서 불러도 된다.
+bool send_text(const char *text);                       // 신호 보내기. 붙어 있지 않거나 실패하면 false. 다른 보내기·재접속이 끝나기를 2초까지 기다린다
+bool send_voice(const uint8_t *frame, size_t len);      // 음성 프레임 [순번 2][Opus] 보내기. 전용 송신 태스크에서(소켓이 막히면 최대 2초 멈춘다). 잠금은 20ms 만 기다린다 — 못 얻으면 그 프레임은 버린다
 
 void reconnect();                   // 지금 연결을 끊고 처음부터 다시 붙는다(call_fsm 의 A_RECONNECT). 바로 돌아온다
 
--- a/firmware/tab5_phone/main/relay_link.cpp
+++ b/firmware/tab5_phone/main/relay_link.cpp
@@ -21,8 +21,15 @@
 //         비용: 실패가 계속되면 5초마다 로그 한 줄. 탈출구: 재시도 블록(start_pending)을 지우면 이전 동작(한 번만 시도)이다.
 //   RL-D9 토큰이 든 접속 요청 전문을 오류 로그로 찍는 transport_ws 를 막는 장치는 esp_log_level_set 이고, 그것은 CONFIG_LOG_DYNAMIC_LEVEL_CONTROL=y 에 기댄다
 //         → sdkconfig.defaults 에 고정하고 꺼지면 빌드가 실패하게 한다(M-4). 탈출구: 둘 다 지우면 Kconfig 기본값에 다시 기댄다.
-//   주의(Phase 3): 다른 태스크가 send_voice 를 부르는 도중에 reconnect 의 stop → start 가 돌면 부품이 전송 객체를 새로 만들어
-//         쓰던 것이 해제될 수 있다. 음성 송신 태스크를 붙일 때는 reconnect 와 send_voice 를 같은 잠금으로 묶을 것.
+//   RL-D10 (Phase 3b) 보내기(send_text·send_voice)와 "끊고 다시 붙기"의 stop → start 를 같은 잠금(s_tx_lock)으로 묶는다.
+//         이유: esp_websocket_client_start() 는 전송 객체 목록을 지우고 새로 만든다(esp_websocket_client.c 의 create_transport —
+//         부품의 잠금을 쥐지 않는다). 그때 다른 태스크가 send 안에서 옛 전송 객체를 쓰고 있으면 해제된 메모리를 쓴다.
+//         3b 부터 음성 송신 태스크가 20ms 마다 보내므로 실제로 겹친다. 보내는 쪽은 잠금을 쥔 뒤에 연결 상태를 다시 본다.
+//         신호는 잠금을 2초까지(지금의 RL-D6 과 같은 크기), 음성은 20ms 까지만 기다린다 — 음성은 밀리면 버린다.
+//         비용: 음성 송신이 Wi-Fi 때문에 소켓에서 막혀 있으면(최대 2초) 그동안 신호(끊기 등)도 잠금을 기다린다. 2초 안에 못 보내면
+//         send_text 가 false → 상태 기계가 3초 뒤 연결을 끊고 다시 붙어 양쪽 통화를 끝낸다(FSM-D1). 탈출구: 잠금을 빼면 이전 동작.
+//   RL-D11 (3a 최종 검수 M-8) stop 이 도는 동안 접속이 완료되면 핸들러가 LINK_UP 을 대기열에 넣는다. stop 뒤에 그 흔적(UP)이 보이면
+//         LINK_DOWN 을 뒤따라 올려 상태 기계가 "붙었다 → 끊겼다"로 읽게 한다(전에는 LINK_UP 만 남아 잠깐 대기 화면이 됐다).
 #include "relay_link.h"
 
 #include <atomic>
@@ -34,6 +41,7 @@
 #include "esp_websocket_client.h"
 #include "freertos/FreeRTOS.h"
 #include "freertos/queue.h"
+#include "freertos/semphr.h"
 #include "freertos/task.h"
 
 #include "net_wifi.h"
@@ -55,6 +63,8 @@
 std::atomic<voice_sink_t> s_voice_sink{nullptr};
 int64_t s_up_since_us = 0;
 TaskHandle_t s_relay_task = nullptr;
+SemaphoreHandle_t s_tx_lock = nullptr;                           // RL-D10
+constexpr TickType_t TEXT_LOCK_WAIT = pdMS_TO_TICKS(2000), VOICE_LOCK_WAIT = pdMS_TO_TICKS(20);
 
 char s_url[URL_BUF], s_headers[200];
 constexpr int64_t START_RETRY_US = 5LL * 1000000;                // RL-D8
@@ -173,19 +183,23 @@
 
         if (s_reconnect_req.exchange(false)) {                   // RL-D5
             const bool was_up = s_phase == Phase::UP;
-            s_phase = Phase::IDLE;                               // RL-D8: stop 이 끝날 때까지 UP 으로 남지 않게 먼저 내린다
+            s_phase = Phase::IDLE;                               // RL-D8: stop 이 끝날 때까지 UP 으로 남지 않게 먼저 내린다(새로 보내려는 쪽도 여기서 돌아선다)
+            xSemaphoreTake(s_tx_lock, portMAX_DELAY);            // RL-D10: 보내는 중인 것이 끝나기를 기다린다(보내기 한 번은 최대 2초)
             esp_websocket_client_stop(s_ws);                     // 태스크가 끝날 때까지 기다린다(끊김 이벤트는 오지 않는다)
-            s_phase = Phase::IDLE;                               // N-1: stop 이 도는 동안 핸들러가 올린 값(CONNECTING·UP)이 남지 않게 한 번 더
-            if (was_up) post(In::Kind::LINK_DOWN);
+            const bool rose = s_phase.exchange(Phase::IDLE) == Phase::UP;   // N-1 + RL-D11: stop 이 도는 동안 핸들러가 올린 값이 남지 않게, 그리고 UP 까지 갔었는지 본다
+            if (was_up || rose) post(In::Kind::LINK_DOWN);
             printf("RELAY up=0 code=0 attempt=%d\n", s_fails.load());
             start_pending = esp_websocket_client_start(s_ws) != ESP_OK;
+            xSemaphoreGive(s_tx_lock);
             next_start_us = esp_timer_get_time() + START_RETRY_US;
             if (start_pending) ESP_LOGW(TAG, "websocket restart failed — retry in 5s");
             continue;
         }
 
         if (start_pending && esp_timer_get_time() >= next_start_us) {   // RL-D8
+            xSemaphoreTake(s_tx_lock, portMAX_DELAY);            // RL-D10: start 가 전송 객체를 새로 만든다
             start_pending = esp_websocket_client_start(s_ws) != ESP_OK;
+            xSemaphoreGive(s_tx_lock);
             next_start_us = esp_timer_get_time() + START_RETRY_US;
             if (start_pending) ESP_LOGW(TAG, "websocket start retry failed"); else ESP_LOGI(TAG, "websocket start retry ok");
         }
@@ -202,7 +216,22 @@
             last_ping_us = now;
         }
         up_was = is_up;
+    }
+}
+
+// RL-D10: 잠금을 쥐고, 붙어 있을 때만 보낸다. 잠금을 lock_wait 안에 못 얻으면 보내지 않는다.
+bool send_frame(bool text, const char *data, int len, TickType_t lock_wait)
+{
+    if (!s_ws || !s_tx_lock || s_phase != Phase::UP) return false;            // 붙어 있지 않으면 잠금을 기다리지도 않는다
+    if (xSemaphoreTake(s_tx_lock, lock_wait) != pdTRUE) return false;
+    bool ok = false;
+    if (s_phase == Phase::UP) {                                               // 잠금을 쥔 뒤에 다시 본다 — 그 사이 reconnect 가 내렸을 수 있다
+        const int sent = text ? esp_websocket_client_send_text(s_ws, data, len, pdMS_TO_TICKS(2000))    // RL-D6
+                              : esp_websocket_client_send_bin(s_ws, data, len, pdMS_TO_TICKS(2000));
+        ok = sent == len;
     }
+    xSemaphoreGive(s_tx_lock);
+    return ok;
 }
 
 }  // namespace
@@ -210,6 +239,8 @@
 void start(const char *url, const char *dev_id, const char *token)
 {
     if (s_in_q) return;
+    static StaticSemaphore_t tx_lock_buf;                         // RL-D10: 정적이라 만들기가 실패하지 않는다
+    s_tx_lock = xSemaphoreCreateMutexStatic(&tx_lock_buf);
     s_in_q = xQueueCreate(16, sizeof(In));
     strlcpy(s_url, url, sizeof s_url);
     const int n = snprintf(s_headers, sizeof s_headers, "X-Device-Id: %s\r\nX-Device-Token: %s\r\n", dev_id, token);   // 줄마다 \r\n 이 있어야 한다
@@ -234,15 +265,14 @@
 
 bool send_text(const char *text)
 {
-    if (!s_ws || s_phase != Phase::UP || !text) return false;
-    const int len = (int)strlen(text);
-    return esp_websocket_client_send_text(s_ws, text, len, pdMS_TO_TICKS(2000)) == len;                     // RL-D6
+    if (!text) return false;
+    return send_frame(true, text, (int)strlen(text), TEXT_LOCK_WAIT);
 }
 
 bool send_voice(const uint8_t *frame, size_t len)
 {
-    if (!s_ws || s_phase != Phase::UP || !frame || len < relayproto::MIN_VOICE_FRAME || len > relayproto::MAX_VOICE_FRAME) return false;
-    return esp_websocket_client_send_bin(s_ws, (const char *)frame, (int)len, pdMS_TO_TICKS(2000)) == (int)len;
+    if (!frame || len < relayproto::MIN_VOICE_FRAME || len > relayproto::MAX_VOICE_FRAME) return false;
+    return send_frame(false, (const char *)frame, (int)len, VOICE_LOCK_WAIT);
 }
 
 void reconnect() { s_reconnect_req = true; }
```

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/main/relay_link.h firmware/tab5_phone/main/relay_link.cpp && git commit -m "fix(phone): relay_link — 보내기와 다시 붙기를 같은 잠금으로(RL-D10, 음성 송신 전 필수), stop 도중의 LINK_UP 뒤에 LINK_DOWN(RL-D11, 3a 검수 M-8)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [x] **Step 3: 소리 실패 동작·`TONE` 줄 패치 셋을 함께 적용하고 대조한다** (M-1·M-2·M-3·M-5·M-6)

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft && git apply "$D/patches/T1-03-audio-out-fail.patch" && git apply "$D/patches/T1-04-tone-out-log.patch" && git apply "$D/patches/T1-05-app-main.patch" && for f in tone_out.h tone_out.cpp; do cmp "$D/files/firmware/tab5_phone/main/$f" "firmware/tab5_phone/main/$f" && echo "같음: $f"; done; cd firmware/tab5_phone/main && wc -l audio_out.h audio_out.cpp app.h app.cpp phone_main.cpp | head -5
```
Expected: `같음:` 2줄, `      36 audio_out.h`, `     241 audio_out.cpp`, `      17 app.h`, `     248 app.cpp`, `     184 phone_main.cpp` (이 다섯 파일은 Task 3·6 에서 다시 바뀌므로 여기서는 줄 수로 본다).

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone/main && for f in audio_out.cpp tone_out.cpp app.cpp phone_main.cpp; do grep -Hn "^void fail(\|if (stage) { fail(stage)\|audioout::fail(\"tones\")\|xQueueSend(s_log\|toneout::print_pending();\|printf(\"TONE \|boot_guard(audioout::ready(), \"audio\")\|audioout::fail(\"injected\")\|fault_audio_set(false)\|cmd == \"fault audio\"\|netwifi::start();" "$f" | cut -c1-100; done; echo "출력 태스크 안의 printf: $(sed -n '/^bool fill(/,/^}/p' tone_out.cpp | grep -c printf)"
```
Expected:
```
audio_out.cpp:204:    if (stage) { fail(stage); return false; }   // PL3b-D2: phone_main 의 boot_guar
audio_out.cpp:224:void fail(const char *stage)
tone_out.cpp:44:        if (!s_log || xQueueSend(s_log, &l, 0) != pdTRUE) ++s_log_lost;
tone_out.cpp:58:    if (!s_box || !s_log || !audioout::add_source(fill)) { audioout::fail("tones"); 
tone_out.cpp:75:        if (l.off != tones::Tone::NONE) printf("TONE %s off\n", tones::tone_name(l.o
tone_out.cpp:76:        if (l.on != tones::Tone::NONE) printf("TONE %s on\n", tones::tone_name(l.on)
tone_out.cpp:79:    if (lost) printf("TONE ? lost=%u\n", (unsigned)lost);    // 대기열이 넘쳤다(app 이 오래 멈췄
app.cpp:176:        toneout::print_pending();                                // 0) 출력 태스크가 남긴 TONE 줄
phone_main.cpp:113:    else if (cmd == "fault audio") printf(fault_audio_set(true) ? "OK fault audio
phone_main.cpp:172:        if (injected) audioout::fail("injected");
phone_main.cpp:175:        boot_guard(audioout::ready(), "audio");
phone_main.cpp:176:        if (injected) fault_audio_set(false);   // 여기까지 왔으면 세 번 다시 켜 보고 포기한 것이다 —
phone_main.cpp:179:    netwifi::start();
출력 태스크 안의 printf: 0
```

| 확인할 것 | 코드 |
|---|---|
| 출력 태스크(`fill`)는 찍지 않는다 — 찍을 것을 길이 16 대기열에 넣기만 하고(기다리지 않는다, 가득 차면 버리고 센다), `printf` 는 `print_pending`(app 태스크) 안에만 있다 | `tone_out.cpp:42~45`, `71~80`, 위의 `출력 태스크 안의 printf: 0` |
| app 태스크는 20ms 바퀴의 맨 앞에서 `print_pending` 을 부른다 | `app.cpp:176` |
| 소리 장치 시작이 어느 단계에서 실패해도 `fail()` 한 곳을 지난다: `AUDIO FAIL <단계>`, `ready()` false, 앰프 끔. 신호음 공급자의 준비 실패도 같은 곳으로 온다(`audio=1` 로 남지 않는다 — M-3) | `audio_out.cpp:204`, `224~230`, `tone_out.cpp:58` |
| 실패 뒤 정리는 앰프를 끄는 것까지만(I2S·코덱을 닫지 않는다 — PL3b-D2) | `audio_out.cpp:221~230` 의 주석과 본문 |
| `phone_main` 은 소리 장치를 Wi-Fi·교환기보다 먼저 열고, 결과를 `boot_guard(…, "audio")` 에 넘긴다. `boot_guard` 는 횟수를 NVS(`rst_audio`)에 적을 수 있을 때만 재시작하고 3번을 넘기지 않는다(기존 함수 — 화면과 같은 규칙) | `phone_main.cpp:167~177`, `40~59` |
| 재시작 직전의 줄(`PHONE audio-failed restart N`)이 시리얼로 나간 뒤에 재시작한다(`fflush` + 0.2초 — `reboot` 명령과 같은 방식. 화면 실패의 재시작에도 같이 적용된다) | `phone_main.cpp:50~52` |
| `fault audio` 는 NVS 에 표시만 한다(그 자리에서는 아무것도 죽이지 않는다). 다음 부팅부터 소리 장치를 열지 않고 `fail("injected")`. 네 번째 부팅(재시작 3번을 다 쓴 뒤)에서 표시를 지운다 | `phone_main.cpp:62~81`, `113`, `170~176` |
| 처리 시간 합계는 64비트이고 출력 태스크만 쓴다. 다른 태스크에는 평균값만 내준다(M-5) | `audio_out.cpp` 의 `s_sum_us`·`s_avg_us` |
| `tones::RATE` 와 `audioout::RATE` 가 다르면 빌드가 실패한다(M-6) | `tone_out.cpp:23` |

```diff
--- a/firmware/tab5_phone/main/audio_out.h
+++ b/firmware/tab5_phone/main/audio_out.h
@@ -3,6 +3,7 @@
 //   - 출력 태스크가 BLOCK 표본(16ms)마다 등록된 공급자(source_fn)에게 PCM 을 받아 합쳐 코덱에 쓴다. 신호음(tone_out)이 공급자 하나이고,
 //     3b 의 목소리가 둘째가 된다. 낼 것이 없으면 쓰지 않고 잠든다 — I2S 는 계속 돌며 0 을 낸다(앰프는 켜 둔다, PL3a-D4).
 //   - 실패해도(코덱이 대답하지 않음 등) 전화기는 소리 없이 계속 돈다: start 가 false 와 "AUDIO FAIL <단계>" 한 줄(PL3a-D10).
+//     Phase 3b(PL3b-D2): 부르는 쪽(phone_main)이 boot_guard 로 3번까지 다시 켜 본 뒤에 그렇게 된다. 실패하면 앰프를 끈 채로 둔다.
 #pragma once
 #include <cstdint>
 
@@ -23,7 +24,11 @@
 bool add_source(source_fn fn);              // 공급자 등록(최대 2). start 앞뒤 어느 때나. 자리가 없으면 false
 void wake();                                // 공급자에게 낼 것이 생겼다 — 잠든 출력 태스크를 깨운다(어느 태스크에서나, 소리 장치가 없으면 아무 일도 안 함)
 
-bool ready();                               // start 가 성공했나(STAT audio=)
+// 소리 경로의 어느 단계가 실패했음을 알린다: "AUDIO FAIL <stage>" 한 줄, ready() 는 false, 앰프는 끈다. start 안에서도, 공급자 모듈(tone_out·voice)의
+// 준비가 실패했을 때도 부른다 — 그래서 "AUDIO FAIL tones 인데 STAT 은 audio=1" 같은 어긋남이 없다(3a 최종 검수 M-3).
+void fail(const char *stage);
+
+bool ready();                               // 소리 경로 전체가 준비됐나(STAT audio=). fail 이 한 번이라도 불렸으면 false
 void block_us(uint32_t *avg, uint32_t *max);   // 소리를 낸 묶음 하나를 만드는 데(공급자 호출 + 합치기) 걸린 시간의 평균·최대(µs, 부팅 뒤). 묶음 하나 = 16000µs(STAT aud_us=평균/최대)
 uint32_t write_errors();                    // 코덱 쓰기 실패 횟수(STAT aud_err=)
 unsigned stack_free();                      // 출력 태스크 스택 최저 여유(바이트, 태스크가 없으면 0 — STAT stk_audio=)
--- a/firmware/tab5_phone/main/audio_out.cpp
+++ b/firmware/tab5_phone/main/audio_out.cpp
@@ -47,8 +47,11 @@
 TaskHandle_t s_task = nullptr;
 std::atomic<bool> s_ready{false};
 std::atomic<source_fn> s_src[MAX_SOURCES];
-std::atomic<uint32_t> s_max_us{0}, s_sum_us{0}, s_blocks{0}, s_wr_err{0}, s_i2c_err{0};
+std::atomic<bool> s_failed{false}, s_amp{false};
+std::atomic<uint32_t> s_max_us{0}, s_avg_us{0}, s_wr_err{0}, s_i2c_err{0};
 int16_t s_mix[BLOCK], s_tmp[BLOCK];             // 출력 태스크에서만 쓴다
+uint64_t s_sum_us = 0;                          // 출력 태스크에서만 쓴다. 3a 최종 검수 M-5: 32비트 합계는 소리 낸 누적 63시간에 넘친다(통화는 계속 소리를 낸다)
+uint32_t s_blocks = 0;
 
 // ── 코덱 제어 인터페이스: M5.In_I2C 로 레지스터 읽기·쓰기(시험 펌웨어 board_audio.cpp 그대로) ──────────────────
 struct m5_ctrl_t { audio_codec_ctrl_if_t base; uint8_t addr; bool opened; };
@@ -146,11 +149,13 @@
     return s_i2c_err == 0;                      // 음량 설정은 코덱 쓰기가 실패해도 OK 를 돌려준다(esp_codec_dev.c:376~378) → 쓰기 실패 횟수로 확인한다
 }
 
-bool amp_on()                                   // 스피커 앰프 전원(확장칩 0x43 의 P1). M5Unified 가 부팅 때 꺼 둔다
+bool amp_set(bool on)                           // 스피커 앰프 전원(확장칩 0x43 의 P1). M5Unified 가 부팅 때 꺼 둔다. 시험 펌웨어 board_audio.cpp 의 amp() 그대로
 {
     xSemaphoreTake(s_lock, portMAX_DELAY);
-    const bool ok = M5.In_I2C.bitOn(PI4IO1_ADDR, 0x05, 0b00000010, I2C_FREQ);   // 읽고-고쳐-쓰기(트랜잭션 둘) — 잠금 안에서
+    const bool ok = on ? M5.In_I2C.bitOn(PI4IO1_ADDR, 0x05, 0b00000010, I2C_FREQ)    // 읽고-고쳐-쓰기(트랜잭션 둘) — 잠금 안에서
+                       : M5.In_I2C.bitOff(PI4IO1_ADDR, 0x05, 0b00000010, I2C_FREQ);
     xSemaphoreGive(s_lock);
+    if (ok) s_amp = on;
     return ok;
 }
 
@@ -169,9 +174,10 @@
             }
         }
         if (!any) { ulTaskNotifyTake(pdTRUE, portMAX_DELAY); continue; }    // PL3a-D4: 낼 것이 없으면 쓰지 않고 잠든다(DMA 가 0 을 낸다)
-        const uint32_t us = (uint32_t)(esp_timer_get_time() - t0);          // 소리를 낸 묶음만 잰다(기록 줄을 찍은 묶음은 printf 시간이 든다 → 최댓값)
+        const uint32_t us = (uint32_t)(esp_timer_get_time() - t0);          // 소리를 낸 묶음만 잰다
         if (us > s_max_us.load()) s_max_us = us;
         s_sum_us += us; ++s_blocks;
+        s_avg_us = (uint32_t)(s_sum_us / s_blocks);
         if (esp_codec_dev_write(s_out, s_mix, (int)sizeof s_mix) != ESP_CODEC_DEV_OK) {             // 정상이면 DMA 칸이 빌 때까지 기다린다 = 16ms 박자
             ++s_wr_err;
             vTaskDelay(pdMS_TO_TICKS(16));      // 쓰기가 바로 실패해도 박자를 지킨다 — 쉬지 않고 돌면 우선순위가 낮은 화면·터치 태스크가 굶는다(PL3a-D10)
@@ -183,23 +189,19 @@
 
 bool start(SemaphoreHandle_t i2c_lock)
 {
-    if (s_ready || s_tx) return s_ready;        // 한 번만(두 번째 호출은 첫 결과)
+    if (s_ready || s_failed || s_tx) return s_ready;   // 한 번만(두 번째 호출은 첫 결과)
     s_lock = i2c_lock;
-    const char *fail = nullptr;
-    if (!s_lock) fail = "lock";
-    else if (!codec_answers()) fail = "i2c";
-    else if (!i2s_setup()) fail = "i2s";
-    else if (!codec_open()) fail = "codec";
+    const char *stage = nullptr;                // 실패한 단계
+    if (!s_lock) stage = "lock";
+    else if (!codec_answers()) stage = "i2c";
+    else if (!i2s_setup()) stage = "i2s";
+    else if (!codec_open()) stage = "codec";
     else {
         vTaskDelay(pdMS_TO_TICKS(30));          // PL3a-D4: 코덱 출력이 자리 잡은 뒤 앰프를 켠다
-        if (!amp_on()) fail = "amp";
-        else if (xTaskCreatePinnedToCore(out_task, "audio", TASK_STACK, nullptr, TASK_PRIO, &s_task, TASK_CORE) != pdPASS) fail = "task";
+        if (!amp_set(true)) stage = "amp";
+        else if (xTaskCreatePinnedToCore(out_task, "audio", TASK_STACK, nullptr, TASK_PRIO, &s_task, TASK_CORE) != pdPASS) stage = "task";
     }
-    if (fail) {                                 // PL3a-D10: 소리 없이 계속 — 재시작하지 않는다
-        ESP_LOGE(TAG, "audio init failed at '%s' -> the phone runs without sound", fail);
-        printf("AUDIO FAIL %s\n", fail);
-        return false;
-    }
+    if (stage) { fail(stage); return false; }   // PL3b-D2: phone_main 의 boot_guard 가 3번까지 재시작하고, 그래도 안 되면 소리 없이 계속 돈다
     s_ready = true;
     printf("AUDIO ready rate=%d block=%d vol=%d\n", RATE, BLOCK, VOLUME);
     return true;
@@ -216,11 +218,21 @@
 
 void wake() { if (s_task) xTaskNotifyGive(s_task); }
 
+// PL3b-D2 실패 뒤 정리는 "앰프를 끈다"까지만 한다. I2S 채널·코덱 객체는 닫지 않는다: 닫는 코드는 이 기기에서 한 번도 돈 적이 없고,
+//   반쯤 열린 객체를 닫다 죽으면 boot_guard 가 세지 못하는 재부팅 고리가 된다. 남은 채널은 0 만 내보낸다(앰프가 꺼져 있어 들리지 않는다).
+//   출력 태스크가 이미 떠 있으면 그대로 둔다(공급자가 소리를 주면 쓰지만 앰프가 꺼져 있다).
+void fail(const char *stage)
+{
+    s_failed = true; s_ready = false;
+    ESP_LOGE(TAG, "audio failed at '%s' -> the phone runs without sound", stage);
+    printf("AUDIO FAIL %s\n", stage);
+    if (s_lock && s_amp) amp_set(false);
+}
+
 bool ready() { return s_ready; }
 void block_us(uint32_t *avg, uint32_t *max)
 {
-    const uint32_t n = s_blocks.load();
-    if (avg) *avg = n ? s_sum_us.load() / n : 0;
+    if (avg) *avg = s_avg_us.load();
     if (max) *max = s_max_us.load();
 }
 uint32_t write_errors() { return s_wr_err; }
```

```diff
--- a/firmware/tab5_phone/main/tone_out.h
+++ b/firmware/tab5_phone/main/tone_out.h
@@ -1,12 +1,16 @@
 // tone_out.h — 신호음 공급자. app 이 상태 기계의 할 일에서 고른 소리(tones::from_acts)를 받아, 출력 태스크(audio_out)에 PCM 으로 건넨다.
 // 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md PL3a-D3·D6·D11. 박자·크기·멈춤 약속은 tones.h(맥 시험)에 있다.
+// Phase 3b(docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md PL3b-D1): 기록 줄 "TONE …" 은 출력 태스크가 찍지 않는다 —
+//   출력 태스크는 찍을 것을 대기열에 남기고, app 태스크가 print_pending() 으로 찍는다(출력 태스크가 시리얼에 막혀 소리가 끊기지 않게).
 #pragma once
 #include "tones.h"
 
 namespace toneout {
 
-void start();                   // audio_out 에 공급자로 등록한다. audioout::start 뒤, app::start 앞에 한 번(소리 장치가 없어도 불러도 된다)
+bool start();                   // audio_out 에 공급자로 등록한다. audioout::start 뒤, app::start 앞에 한 번(소리 장치가 없어도 불러도 된다).
+                                // 실패(메모리 부족·자리 없음)하면 audioout::fail("tones") 로 알리고 false — STAT 의 audio= 가 0 이 된다
 void play(tones::Tone t);       // 이 소리를 낸다(NONE = 멈춤). app 태스크에서. 바로 돌아온다 — 마지막 명령만 남는다(PL3a-D6)
 tones::Tone now();              // 지금 내는 소리(STAT tone=). 출력 태스크가 바꾼다
+void print_pending();           // 출력 태스크가 남긴 "TONE <이름> on|off" 줄을 찍는다. app 태스크가 20ms 바퀴마다 부른다(다른 태스크에서 부르지 않는다)
 
 }  // namespace toneout
--- a/firmware/tab5_phone/main/tone_out.cpp
+++ b/firmware/tab5_phone/main/tone_out.cpp
@@ -3,9 +3,13 @@
 // PL3a-D6 명령은 길이 1 우편함(xQueueOverwrite)으로 건넨다 — 마지막 명령만 남는다. 명령 하나가 "지금 내야 할 소리"를 통째로 말하므로
 //         마지막 것만 맞으면 된다(맥 시험 "stop then bell in one batch"). 대기열이 가득 차 멈춤 명령을 잃는 일이 없다.
 //         비용: 출력 태스크가 꺼내기 전에 벨 → 멈춤이 연달아 오면 그 벨은 한 표본도 나지 않고 "TONE BELL on" 줄도 없다. 탈출구: 길이 8 대기열.
-// PL3a-D11 기록 줄 "TONE <이름> on|off" 는 출력 태스크가 소리를 실제로 만든 뒤에 찍는다(무엇을 찍을지는 tones::next_block — 맥 시험).
-//         off = 만든 PCM 이 0 이 된 때. 스피커에서 소리가 그치는 것은 그보다 DMA 에 앞서 쓴 만큼(최대 6칸 = 96ms) 늦다.
-//         이 줄은 출력 태스크가 찍으므로 원인이 된 "FSM …" 줄(app 태스크)과의 앞뒤는 정해져 있지 않다 — 기기 시험은 기기별 TONE 줄의 순서만 본다.
+// PL3a-D11 기록 줄 "TONE <이름> on|off" 는 출력 태스크가 소리를 실제로 만든 뒤에 나온다(무엇을 찍을지는 tones::next_block — 맥 시험).
+//         off = 만든 PCM 이 0 이 된 때. 스피커에서 소리가 그치는 것은 그보다 DMA 에 앞서 쓴 만큼(audio_out.cpp 의 DMA_DESC) 늦다.
+// PL3b-D1 그 줄을 출력 태스크(우선순위 6)가 직접 찍지 않는다(3a 최종 검수 M-1): printf 는 stdout 잠금과 USB 직렬 드라이버를 거쳐,
+//         읽는 쪽이 느리거나 다른 태스크가 긴 줄(shot 의 B64 한 줄 약 0.36초)을 찍는 중이면 그동안 출력 태스크가 멈춘다 — 목소리가 끊긴다.
+//         출력 태스크는 찍을 것을 길이 16 대기열에 넣기만 하고(가득 차면 버리고 센다) app 태스크가 20ms 바퀴마다 꺼내 찍는다.
+//         줄의 모양·기기별 순서는 그대로다. "FSM …" 줄과의 앞뒤는 전처럼 정해져 있지 않다(app 이 다음 바퀴에 찍으므로 대개 FSM 줄 뒤).
+//         비용: TONE 줄이 최대 한 바퀴(20ms, 화면을 내보내는 중이면 60~80ms) 늦게 찍힌다. 탈출구: fill 안에서 다시 printf.
 #include "tone_out.h"
 
 #include <atomic>
@@ -16,32 +20,43 @@
 
 #include "audio_out.h"
 
+static_assert(tones::RATE == audioout::RATE, "tones.h and audio_out.h must use the same sample rate");   // 3a 최종 검수 M-6: 한쪽만 바꾸면 음 높이·박자가 조용히 틀린다
+
 namespace toneout {
 namespace {
 
+struct Line { tones::Tone off, on; };                   // 찍을 줄: "TONE <off> off" 다음 "TONE <on> on"(NONE 은 찍지 않는다)
+constexpr int LOG_LEN = 16;
+
 QueueHandle_t s_box = nullptr;                          // 길이 1 우편함
+QueueHandle_t s_log = nullptr;                          // 출력 태스크 → app 태스크: 찍을 줄(PL3b-D1)
 tones::Player s_player;                                 // 출력 태스크에서만 만진다
 std::atomic<tones::Tone> s_now{tones::Tone::NONE};
+std::atomic<uint32_t> s_log_lost{0};
 
 bool fill(int16_t *out, int n)                          // audioout 의 공급자 — 출력 태스크에서 16ms 마다
 {
     tones::Tone cmd = tones::Tone::NONE;
     const bool has_cmd = s_box && xQueueReceive(s_box, &cmd, 0) == pdTRUE;
     const tones::Block b = tones::next_block(s_player, has_cmd, cmd, out, n);   // 명령 → 한 묶음 → 찍을 줄(맥 시험으로 고정된 로직)
-    if (b.off != tones::Tone::NONE) printf("TONE %s off\n", tones::tone_name(b.off));
-    if (b.on != tones::Tone::NONE) printf("TONE %s on\n", tones::tone_name(b.on));
+    if (b.off != tones::Tone::NONE || b.on != tones::Tone::NONE) {              // PL3b-D1: 찍지 않고 남긴다(기다리지 않는다)
+        const Line l{b.off, b.on};
+        if (!s_log || xQueueSend(s_log, &l, 0) != pdTRUE) ++s_log_lost;
+    }
     s_now = s_player.current();
     return b.sound;
 }
 
 }  // namespace
 
-void start()
+bool start()
 {
-    if (s_box) return;
+    if (s_box) return true;
     s_box = xQueueCreate(1, sizeof(tones::Tone));
+    s_log = xQueueCreate(LOG_LEN, sizeof(Line));
     tones::sine_table();                                // 사인 표를 지금 만든다(출력 태스크의 첫 묶음이 늦지 않게)
-    if (!s_box || !audioout::add_source(fill)) printf("AUDIO FAIL tones\n");
+    if (!s_box || !s_log || !audioout::add_source(fill)) { audioout::fail("tones"); return false; }   // 3a 최종 검수 M-3: audio= 도 0 이 된다
+    return true;
 }
 
 void play(tones::Tone t)
@@ -53,4 +68,15 @@
 
 tones::Tone now() { return s_now; }
 
+void print_pending()
+{
+    Line l;
+    while (s_log && xQueueReceive(s_log, &l, 0) == pdTRUE) {
+        if (l.off != tones::Tone::NONE) printf("TONE %s off\n", tones::tone_name(l.off));
+        if (l.on != tones::Tone::NONE) printf("TONE %s on\n", tones::tone_name(l.on));
+    }
+    const uint32_t lost = s_log_lost.exchange(0);
+    if (lost) printf("TONE ? lost=%u\n", (unsigned)lost);    // 대기열이 넘쳤다(app 이 오래 멈췄다) — 줄 몇 개가 빠졌음을 알린다
+}
+
 }  // namespace toneout
```

```diff
--- a/firmware/tab5_phone/main/app.h
+++ b/firmware/tab5_phone/main/app.h
@@ -7,7 +7,7 @@
 
 namespace app {
 
-// 화면·통화 진행 태스크를 띄운다. gfx::init, netwifi::start, relaylink::start, audioout::start, toneout::start 뒤에 한 번. unit = 이 기기의 번호(미등록이면 0).
+// 화면·통화 진행 태스크를 띄운다. gfx::init, audioout::start, toneout::start, netwifi::start, relaylink::start 뒤에 한 번. unit = 이 기기의 번호(미등록이면 0).
 // i2c_lock = M5.In_I2C 를 쓰는 코드가 함께 잡는 잠금. 터치 읽기(M5.update)와 코덱·앰프 제어(audio_out)가 같은 버스를 쓴다 — phone_main 이 만든다(PL3a-D14).
 void start(int unit, SemaphoreHandle_t i2c_lock);
 
--- a/firmware/tab5_phone/main/app.cpp
+++ b/firmware/tab5_phone/main/app.cpp
@@ -173,6 +173,8 @@
     while (true) {
         const int64_t now = now_ms();
 
+        toneout::print_pending();                                // 0) 출력 태스크가 남긴 TONE 줄을 찍는다(PL3b-D1 — 출력 태스크는 찍지 않는다)
+
         relaylink::In in;                                        // 1) 교환기 사건
         while (relaylink::poll(&in)) {
             if (in.kind == relaylink::In::Kind::LINK_UP) feed(Ev::LINK_UP, now);
--- a/firmware/tab5_phone/main/phone_main.cpp
+++ b/firmware/tab5_phone/main/phone_main.cpp
@@ -5,7 +5,7 @@
 //        / relay_ws(부품의 웹소켓 태스크) / sercmd(시리얼 명령, 코어0)
 //
 // 시리얼 명령(115200, 줄 단위) — tools/phone_ctl.py 가 쓴다:
-//   stat | t <x> <y> | shot | reboot
+//   stat | t <x> <y> | shot | reboot | fault audio
 #include <M5Unified.h>
 #include <cstdio>
 #include <cstdlib>
@@ -48,6 +48,7 @@
         nvs_close(h);
         if (counted) {                                           // 횟수를 적지 못했으면 재시작하지 않는다(세지 못하면 끝없이 재시작한다)
             printf("PHONE %s-failed restart %d\n", what, n + 1);
+            fflush(stdout); vTaskDelay(pdMS_TO_TICKS(200));      // PL3b-D2: 이 줄이 시리얼로 나간 뒤에 재시작한다('reboot' 명령과 같은 방식 — 기기 시험이 이 줄을 센다)
             esp_restart();
         }
         printf("PHONE FAIL %s (restart count not saved)\n", what);
@@ -57,6 +58,27 @@
     if (!ok) printf("PHONE FAIL %s\n", what);
 }
 
+// PL3b-D2 소리 장치 실패를 일부러 내 보는 스위치(3a 최종 검수 M-3: 실패 경로가 기기에서 한 번도 돈 적이 없었다).
+//   시리얼 'fault audio' → NVS 에 표시 → 다음 부팅부터 소리 장치를 열지 않고 실패로 친다 → boot_guard 가 3번 재시작 →
+//   네 번째 부팅은 소리 없이 계속 돈다(STAT audio=0) → 그때 표시를 지운다(다음 재부팅은 정상). 사람이 켠 것만, 한 번의 시험만.
+static const char *FAULT_KEY = "f_audio";
+static bool fault_audio_armed()
+{
+    nvs_handle_t h; uint8_t v = 0;
+    if (nvs_open("phone", NVS_READONLY, &h) != ESP_OK) return false;
+    nvs_get_u8(h, FAULT_KEY, &v);
+    nvs_close(h);
+    return v == 1;
+}
+static bool fault_audio_set(bool on)
+{
+    nvs_handle_t h;
+    if (nvs_open("phone", NVS_READWRITE, &h) != ESP_OK) return false;
+    const bool ok = (on ? nvs_set_u8(h, FAULT_KEY, 1) : nvs_erase_key(h, FAULT_KEY)) == ESP_OK && nvs_commit(h) == ESP_OK;
+    nvs_close(h);
+    return ok;
+}
+
 // 화면을 JPEG 로 찍어 시리얼로 내보낸다. 받는 쪽(phone_ctl.py)이 크기와 검사합을 확인한다.
 static void dump_shot()
 {
@@ -88,6 +110,7 @@
     }
     else if (cmd == "shot") dump_shot();
     else if (cmd == "reboot") { printf("OK reboot\n"); fflush(stdout); vTaskDelay(pdMS_TO_TICKS(200)); esp_restart(); }
+    else if (cmd == "fault audio") printf(fault_audio_set(true) ? "OK fault audio\n" : "ERR fault audio\n");   // 다음 부팅부터(PL3b-D2). 'reboot' 로 시작한다
     else printf("ERR unknown\n");
     fflush(stdout);
 }
@@ -141,13 +164,21 @@
     static StaticSemaphore_t i2c_lock_buf;
     SemaphoreHandle_t i2c_lock = xSemaphoreCreateMutexStatic(&i2c_lock_buf);
 
-    netwifi::start();
-    if (me.unit && gfx_ok) relaylink::start(RELAY_URL, me.id, me.token);   // M-3: 화면이 없으면(app 태스크 없음) 교환기에 "켜짐"으로 보이지 않게 붙지 않는다
-    else if (me.unit) ESP_LOGE(TAG, "gfx init failed -> relay not started");
+    // PL3b-D2 소리 장치는 Wi-Fi·교환기보다 먼저 연다(Phase 0 시험 펌웨어와 같은 순서 — 다시 켜 보는 재시작이 망을 건드리기 전에 끝난다).
+    //   실패하면 "AUDIO FAIL <단계>" → boot_guard 가 3번까지 재시작 → 그래도 안 되면 "PHONE FAIL audio" 를 찍고 소리 없이 계속 돈다(STAT audio=0).
+    //   통화 설계서 §9.7 "소리 초기화 실패 → 재시작 가드"(화면 표시는 Phase 5). 3a 의 PL3a-D10(재시작 없음)을 대신한다.
     if (gfx_ok) {
-        audioout::start(i2c_lock);      // PL3a-D10: 실패하면 "AUDIO FAIL <단계>" 한 줄 — 전화기는 소리 없이 계속 돈다
+        const bool injected = fault_audio_armed();
+        if (injected) audioout::fail("injected");
+        else audioout::start(i2c_lock);
         toneout::start();
-        app::start(me.unit, i2c_lock);
+        boot_guard(audioout::ready(), "audio");
+        if (injected) fault_audio_set(false);   // 여기까지 왔으면 세 번 다시 켜 보고 포기한 것이다 — 시험은 한 번으로 끝. 다음 부팅은 정상
     }
+
+    netwifi::start();
+    if (me.unit && gfx_ok) relaylink::start(RELAY_URL, me.id, me.token);   // M-3: 화면이 없으면(app 태스크 없음) 교환기에 "켜짐"으로 보이지 않게 붙지 않는다
+    else if (me.unit) ESP_LOGE(TAG, "gfx init failed -> relay not started");
+    if (gfx_ok) app::start(me.unit, i2c_lock);
     xTaskCreatePinnedToCore(serial_task, "sercmd", 8 * 1024, nullptr, 3, nullptr, 0);
 }
```

- [x] **Step 4: 맥 시험·도구 시험이 그대로인지 다시 돌리고 커밋한다**

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone/tools && SDK="$(xcrun --sdk macosx --show-sdk-path)" && for T in wifi_store call_fsm relay_proto device_id ui_layout tones; do clang++ -std=c++17 -Wall -Wextra -isysroot "$SDK" -I../main test_$T.cpp -o /tmp/t_p3b_$T 2> /tmp/t_p3b_$T.err; echo "$T: $(/tmp/t_p3b_$T | tail -1) (경고 $(wc -l < /tmp/t_p3b_$T.err | tr -d ' ')줄)"; done; PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider test_phone_ctl.py 2>&1 | tail -1
```
Expected: `wifi_store: 16 passed, 0 failed (경고 0줄)` · `call_fsm: 105 ok, 0 failed (경고 0줄)` · `relay_proto: 61 ok, 0 failed (경고 0줄)` · `device_id: 9 ok, 0 failed (경고 0줄)` · `ui_layout: 91 ok, 0 failed (경고 0줄)` · `tones: 89 ok, 0 failed (경고 0줄)` · `186 passed in …s`

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/main/audio_out.h firmware/tab5_phone/main/audio_out.cpp firmware/tab5_phone/main/tone_out.h firmware/tab5_phone/main/tone_out.cpp firmware/tab5_phone/main/app.h firmware/tab5_phone/main/app.cpp firmware/tab5_phone/main/phone_main.cpp && git commit -m "fix(phone): 소리 실패 동작 — 3번까지 다시 켜 보기(boot_guard)·실패 표시 한 곳(audioout::fail)·실패 주입(fault audio), TONE 줄은 app 태스크가 찍는다(3a 검수 M-1·M-2·M-3·M-5·M-6)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: 소리 계산 헤더 `pcm.h` (시험 먼저)

**Files:**
- Create: `firmware/tab5_phone/main/pcm.h`
- Test: `firmware/tab5_phone/tools/test_pcm.cpp`

**Interfaces:**
- Consumes: 없음(표준 라이브러리뿐).
- Produces (`namespace pcm` — Task 3 의 `audio_out.cpp`, Task 5 의 `voice_core.h` 가 쓴다):
  - `int16_t sat16(int32_t)`, `void mix_add(int16_t *dst, const int16_t *src, int n)`(더하고 넘치면 ±32767 에서 자른다), `void gain_q8(int16_t *buf, int n, int gain)`(256 = 1배).
  - `typedef bool (*source_fn)(int16_t *out, int n)`, `int mix_sources(const source_fn *fns, int count, int16_t *mix, int16_t *tmp, int n)` — 소리가 있는 공급자의 수를 돌려준다. 0 이면 `mix` 를 건드리지 않는다. 첫 공급자는 복사, 둘째부터는 더한다(PL3b-D3).
  - `uint32_t isqrt64(uint64_t)`, `struct Rms { add(buf, n); value(); take(); }`(세기 — 합계 64비트).
  - `template <unsigned CAP> class Ring { size(); space(); write(src, n); read(dst, n); drain(); }` — 쓰는 태스크 하나·읽는 태스크 하나일 때 잠금 없이 안전(`drain` 은 읽는 쪽에서만).
  - `struct GapWatch { rest(); wrote(now_us, limit_us); late; max_us; }` — 묶음 사이가 `limit_us` 이상 벌어진 횟수(PL3b-D4).

- [x] **Step 1: 시험 패치만 적용하고 실패를 확인한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft && git apply "$D/patches/T2-01-pcm-tests.patch" && wc -l < firmware/tab5_phone/tools/test_pcm.cpp && cd firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_pcm.cpp -o /tmp/t_p3b_pcm 2>&1 | grep -m1 "error:"
```
Expected: `     167`, 이어서 `test_pcm.cpp:5:10: fatal error: 'pcm.h' file not found`

```diff
--- /dev/null
+++ b/firmware/tab5_phone/tools/test_pcm.cpp
@@ -0,0 +1,167 @@
+// test_pcm.cpp — pcm.h 단위 시험:
+//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_pcm.cpp -o /tmp/t_pcm && /tmp/t_pcm
+// 섞기(목소리 + 신호음), 크기 바꾸기, 세기, 태스크 사이 링버퍼, 출력 박자 감시를 고정한다
+// (계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md PL3b-D3·D4·D6, Review Focus 3·5).
+#include "pcm.h"
+#include <cmath>
+#include <cstdio>
+#include <cstdlib>
+#include <thread>
+#include <vector>
+using namespace pcm;
+static int ok = 0, bad = 0;
+#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)
+
+// 공급자 흉내(함수 포인터라 상태는 전역에 둔다)
+static int g_calls[3];
+static bool g_on[3];
+static int16_t g_val[3];
+template <int K> static bool src(int16_t *out, int n) { ++g_calls[K]; if (!g_on[K]) return false; for (int i = 0; i < n; ++i) out[i] = g_val[K]; return true; }
+static void set(int k, bool on, int16_t v) { g_on[k] = on; g_val[k] = v; }
+static bool all(const int16_t *b, int n, int16_t v) { for (int i = 0; i < n; ++i) if (b[i] != v) return false; return true; }
+
+static uint32_t lcg(uint32_t &s) { s = s * 1664525u + 1013904223u; return s >> 8; }
+
+int main()
+{
+    // ════ 더하기·크기 ════
+    {
+        int16_t a[6] = {100, -100, 32000, -32000, 32767, -32768}, b[6] = {23, -23, 1000, -1000, 1, -1};
+        mix_add(a, b, 6);
+        CHECK("mix_add: plain sum, and clips at +32767 / -32768 instead of wrapping",
+              a[0] == 123 && a[1] == -123 && a[2] == 32767 && a[3] == -32768 && a[4] == 32767 && a[5] == -32768);
+        int16_t g[5] = {1000, -1000, 20000, -20000, 3};
+        gain_q8(g, 5, 512);
+        CHECK("gain_q8: x2 doubles and clips", g[0] == 2000 && g[1] == -2000 && g[2] == 32767 && g[3] == -32768 && g[4] == 6);
+        int16_t h[3] = {1000, -1001, 7};
+        gain_q8(h, 3, 128);
+        CHECK("gain_q8: x0.5 halves (rounds toward zero)", h[0] == 500 && h[1] == -500 && h[2] == 3);
+        int16_t k[2] = {12345, -32768};
+        gain_q8(k, 2, 256);
+        CHECK("gain_q8: 256 leaves the samples untouched", k[0] == 12345 && k[1] == -32768);
+        CHECK("sat16: limits", sat16(40000) == 32767 && sat16(-40000) == -32768 && sat16(-7) == -7);
+    }
+
+    // ════ 출력 한 묶음 섞기 (PL3b-D3 — 3a 에서 한 번도 돌지 않은 코드, 검수 M-7) ════
+    {
+        const source_fn fns[3] = {src<0>, src<1>, nullptr};
+        int16_t mix[256], tmp[256];
+        for (int16_t &v : mix) v = 77;
+        set(0, false, 0); set(1, false, 0);
+        CHECK("mix: nobody has sound -> 0, and the block is left untouched", mix_sources(fns, 3, mix, tmp, 256) == 0 && all(mix, 256, 77));
+        set(0, true, 1200);
+        CHECK("mix: tone only -> exactly the tone", mix_sources(fns, 3, mix, tmp, 256) == 1 && all(mix, 256, 1200));
+        set(0, false, 0); set(1, true, -900);
+        CHECK("mix: voice only -> exactly the voice", mix_sources(fns, 3, mix, tmp, 256) == 1 && all(mix, 256, -900));
+        set(0, true, 1200);
+        CHECK("mix: tone + voice -> the sum of both (nobody wins)", mix_sources(fns, 3, mix, tmp, 256) == 2 && all(mix, 256, 300));
+        set(0, true, 30000); set(1, true, 30000);
+        CHECK("mix: loud + loud clips at 32767 (no wrap-around)", mix_sources(fns, 3, mix, tmp, 256) == 2 && all(mix, 256, 32767));
+        set(0, true, -30000); set(1, true, -30000);
+        CHECK("mix: clips at -32768 on the negative side", mix_sources(fns, 3, mix, tmp, 256) == 2 && all(mix, 256, -32768));
+        g_calls[0] = g_calls[1] = 0;
+        mix_sources(fns, 3, mix, tmp, 256);
+        CHECK("mix: every source is asked exactly once per block, in registration order", g_calls[0] == 1 && g_calls[1] == 1);
+        const source_fn swapped[2] = {src<1>, src<0>};
+        set(0, true, 5); set(1, true, 9);
+        int16_t m2[8], t2[8];
+        CHECK("mix: order of the sources does not change the result", mix_sources(swapped, 2, m2, t2, 8) == 2 && all(m2, 8, 14));
+        const source_fn none[2] = {nullptr, nullptr};
+        CHECK("mix: empty slots are skipped", mix_sources(none, 2, m2, t2, 8) == 0 && all(m2, 8, 14));
+    }
+
+    // ════ 세기 ════
+    {
+        CHECK("isqrt64: exact squares and floor", isqrt64(0) == 0 && isqrt64(1) == 1 && isqrt64(15) == 3 && isqrt64(16) == 4
+              && isqrt64(1073676289ull) == 32767 && isqrt64(18446744073709551615ull) == 4294967295u);
+        Rms r;
+        CHECK("rms: empty = 0", r.value() == 0 && r.take() == 0);
+        std::vector<int16_t> s(16000);
+        for (int i = 0; i < 16000; ++i) s[i] = (int16_t)std::lround(8000 * std::sin(2 * 3.14159265358979 * 1000 * i / 16000.0));
+        r.add(s.data(), 16000);
+        CHECK("rms: a sine of amplitude 8000 reads 5656 (8000 / sqrt 2)", r.value() >= 5655 && r.value() <= 5657);
+        CHECK("rms: take returns the value and clears", r.take() >= 5655 && r.value() == 0 && r.n == 0);
+        int16_t full[4] = {-32768, -32768, -32768, -32768};
+        r.add(full, 4);
+        CHECK("rms: full scale does not overflow", r.take() == 32768);
+        int16_t dc[3] = {100, 100, 100};
+        for (int i = 0; i < 4000000; ++i) r.add(dc, 3);                 // 12,000,000 표본(12분 30초 분량)
+        CHECK("rms: long accumulation stays exact", r.value() == 100 && r.n == 12000000u);
+    }
+
+    // ════ 링버퍼 (PL3b-D6) ════
+    {
+        Ring<8> q;
+        int16_t in[8] = {1, 2, 3, 4, 5, 6, 7, 8}, out[8] = {};
+        CHECK("ring: empty at first", q.size() == 0 && q.space() == 8 && q.read(out, 4) == 0);
+        CHECK("ring: write 5 -> size 5", q.write(in, 5) == 5 && q.size() == 5 && q.space() == 3);
+        CHECK("ring: a write that does not fit puts in what fits and reports it", q.write(in + 5, 5) == 3 && q.size() == 8 && q.space() == 0);
+        CHECK("ring: full -> nothing more goes in", q.write(in, 1) == 0 && q.size() == 8);
+        CHECK("ring: reads come out in order", q.read(out, 3) == 3 && out[0] == 1 && out[1] == 2 && out[2] == 3 && q.size() == 5);
+        CHECK("ring: wraps around the end", q.write(in, 3) == 3 && q.read(out, 8) == 8 && out[0] == 4 && out[4] == 8 && out[5] == 1 && out[7] == 3);
+        CHECK("ring: a read of more than there is returns what there is", q.write(in, 2) == 2 && q.read(out, 8) == 2 && out[1] == 2 && q.size() == 0);
+        q.write(in, 6);
+        q.drain();
+        CHECK("ring: drain empties it (reader side)", q.size() == 0 && q.read(out, 8) == 0 && q.write(in, 8) == 8);
+    }
+    {
+        Ring<1024> q;                                                   // 20ms 프레임을 넣고 16ms 묶음으로 꺼낸다 — 오래 돌려도 표본이 빠지거나 겹치지 않는다
+        uint32_t seed = 7; int16_t next_in = 0, next_out = 0; bool order = true; size_t peak = 0;
+        for (int step = 0; step < 2000000; ++step) {
+            if (lcg(seed) & 1) { int16_t f[320]; if (q.space() >= 320) { for (int16_t &v : f) v = next_in++; q.write(f, 320); } }
+            else { int16_t b[256]; const size_t n = q.read(b, 256); for (size_t i = 0; i < n; ++i) if (b[i] != next_out++) order = false; }
+            if (q.size() > peak) peak = q.size();
+        }
+        CHECK("ring: 2 million random 320-in / 256-out steps (index wraps 2^16 many times): no sample lost, repeated or reordered", order && peak <= 1024);
+    }
+    {
+        Ring<1024> q;                                                   // 진짜 두 스레드: 쓰는 쪽 하나, 읽는 쪽 하나(읽는 쪽이 가끔 drain)
+        const int TOTAL = 3000000;
+        std::atomic<bool> done{false};
+        bool order = true; long got = 0, drains = 0; const long GIVE_UP = 400000000;      // 고장 나서 끝나지 않을 때를 위한 상한
+        std::thread writer([&] {
+            int16_t f[320]; int v = 0; long spins = 0;
+            while (v < TOTAL && ++spins < GIVE_UP) { const int n = TOTAL - v < 320 ? TOTAL - v : 320; for (int i = 0; i < n; ++i) f[i] = (int16_t)(v + i); v += (int)q.write(f, (size_t)n); }
+            done = true;
+        });
+        std::thread reader([&] {
+            int16_t b[256]; int16_t expect = 0; bool synced = true; uint32_t seed = 3; long spins = 0;
+            while ((!done || q.size()) && ++spins < GIVE_UP) {
+                if ((lcg(seed) & 0x3FF) == 0) { q.drain(); synced = false; ++drains; continue; }
+                const size_t n = q.read(b, 256);
+                for (size_t i = 0; i < n; ++i) {
+                    if (synced && b[i] != expect) order = false;        // drain 뒤 첫 표본에서 다시 맞춘다(그 뒤로는 연속이어야 한다)
+                    expect = (int16_t)(b[i] + 1); synced = true;
+                }
+                got += (long)n;
+            }
+        });
+        writer.join(); reader.join();
+        CHECK("ring: two real threads, 3 million samples, reader drains now and then: what is read is always consecutive", order && got > 0 && drains > 0 && q.size() == 0);
+    }
+
+    // ════ 출력 박자 감시 (PL3b-D4) ════
+    {
+        GapWatch w;
+        const int64_t LIMIT = 3 * 16000;                                // DMA 3칸 = 48ms
+        int64_t t = 1000000;
+        w.wrote(t, LIMIT);
+        CHECK("gap: the first block after a rest is never late", w.late == 0 && w.max_us == 0);
+        for (int i = 0; i < 1000; ++i) { t += 16000; w.wrote(t, LIMIT); }
+        CHECK("gap: a steady 16 ms beat is not late", w.late == 0 && w.max_us == 16000);
+        t += 47999; w.wrote(t, LIMIT);
+        CHECK("gap: 47.999 ms is still inside what the DMA holds", w.late == 0 && w.max_us == 47999);
+        t += 48000; w.wrote(t, LIMIT);
+        CHECK("gap: 48 ms (the whole DMA ring) counts as late", w.late == 1 && w.max_us == 48000);
+        t += 400000; w.wrote(t, LIMIT);
+        CHECK("gap: a long stall counts once, and the longest gap is kept", w.late == 2 && w.max_us == 400000);
+        w.rest();
+        t += 9000000; w.wrote(t, LIMIT);
+        CHECK("gap: sleeping (nothing to play) is not a late block", w.late == 2 && w.max_us == 400000);
+        t += 16000; w.wrote(t, LIMIT);
+        CHECK("gap: and the beat is watched again afterwards", w.late == 2);
+    }
+
+    printf("\n%d ok, %d failed\n", ok, bad);
+    return bad ? 1 : 0;
+}
```

- [x] **Step 2: 구현 패치를 적용하고 통과를 확인한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft && git apply "$D/patches/T2-02-pcm.patch" && for f in main/pcm.h tools/test_pcm.cpp; do cmp "$D/files/firmware/tab5_phone/$f" "firmware/tab5_phone/$f" && echo "같음: $f"; done && cd firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_pcm.cpp -o /tmp/t_p3b_pcm 2> /tmp/t_p3b_pcm.err; echo "경고 $(wc -l < /tmp/t_p3b_pcm.err | tr -d ' ')줄"; /tmp/t_p3b_pcm | grep -c "^  FAIL"; /tmp/t_p3b_pcm | tail -1
```
Expected: `같음:` 2줄, `경고 0줄`, `0`, `37 ok, 0 failed` (몇 초 걸린다 — 진짜 두 스레드로 300만 표본을 주고받는 링버퍼 시험이 들어 있다).

```diff
--- /dev/null
+++ b/firmware/tab5_phone/main/pcm.h
@@ -0,0 +1,126 @@
+// pcm.h — 16비트 PCM 묶음에 하는 작은 계산: 섞기, 크기 바꾸기, 세기(RMS), 태스크 사이 링버퍼, 출력 박자 감시.
+// 하드웨어·시계와 무관한 순수 로직(맥에서 단위 시험: tools/test_pcm.cpp).
+// 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md 의 결정 PL3b-D3(섞기)·D4(앞서 쓰는 양)·D6(링버퍼).
+#pragma once
+#include <atomic>
+#include <cstddef>
+#include <cstdint>
+#include <cstring>
+
+namespace pcm {
+
+inline int16_t sat16(int32_t v) { return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }
+
+// acc += add (넘치면 ±32767 에서 자른다). 목소리 + 신호음처럼 두 소리를 함께 낼 때.
+inline void mix_add(int16_t *acc, const int16_t *add, int n)
+{
+    for (int i = 0; i < n; ++i) acc[i] = sat16((int32_t)acc[i] + add[i]);
+}
+
+// buf *= gain_q8 / 256 (넘치면 자른다). 256 = 그대로, 512 = 2배(+6dB).
+inline void gain_q8(int16_t *buf, int n, int gain)
+{
+    if (gain == 256) return;
+    for (int i = 0; i < n; ++i) buf[i] = sat16(((int32_t)buf[i] * gain) / 256);
+}
+
+// PL3b-D3 출력 한 묶음: 공급자들을 등록된 순서로 불러, 소리가 있는 것만 합친다(첫 소리는 그대로, 다음부터는 더하고 넘치면 자른다).
+//   공급자 = bool fn(int16_t *out, int n): 낼 소리가 있으면 out 에 n 표본을 쓰고 true, 없으면 false(out 은 쓰지 않아도 된다).
+//   돌려주는 값 = 소리를 낸 공급자 수. 0 이면 mix 는 건드리지 않는다(부르는 쪽은 쓰지 않고 잠든다).
+//   "누가 이기는가"는 없다 — 신호음과 목소리는 겹치는 동안 그냥 더해진다. 겹치는 구간을 짧게 만드는 것은 각 공급자의 몫이다
+//   (신호음은 5ms 에 줄어 멎고, 목소리는 5ms 에 걸쳐 커지고 줄어든다 — tones.h, voice_core.h).
+typedef bool (*source_fn)(int16_t *out, int n);
+inline int mix_sources(const source_fn *src, int count, int16_t *mix, int16_t *tmp, int n)
+{
+    int sounding = 0;
+    for (int i = 0; i < count; ++i) {
+        if (!src[i] || !src[i](tmp, n)) continue;
+        if (sounding == 0) memcpy(mix, tmp, (size_t)n * sizeof(int16_t));
+        else mix_add(mix, tmp, n);
+        ++sounding;
+    }
+    return sounding;
+}
+
+inline uint32_t isqrt64(uint64_t v)                 // 버림 제곱근
+{
+    uint64_t r = 0, bit = (uint64_t)1 << 62;
+    while (bit > v) bit >>= 2;
+    while (bit) {
+        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
+        else r >>= 1;
+        bit >>= 2;
+    }
+    return (uint32_t)r;
+}
+
+// 세기(RMS)를 모은다. add 로 표본을 넣고 take 로 값을 꺼내며 비운다(표본이 없었으면 0).
+struct Rms {
+    uint64_t sum = 0;
+    uint32_t n = 0;
+    void add(const int16_t *s, int count)
+    {
+        for (int i = 0; i < count; ++i) sum += (uint64_t)((int32_t)s[i] * s[i]);
+        n += (uint32_t)count;
+    }
+    uint32_t value() const { return n ? isqrt64(sum / n) : 0; }
+    uint32_t take() { const uint32_t v = value(); sum = 0; n = 0; return v; }
+};
+
+// PL3b-D6 태스크 둘 사이의 PCM 링버퍼: 쓰는 태스크 하나, 읽는 태스크 하나(잠금 없음 — 읽는 쪽이 출력 태스크라 멈추면 안 된다).
+//   CAP 은 2의 거듭제곱. write 는 자리가 모자라면 들어가는 만큼만 넣는다(나머지는 버린다 — 돌려주는 값으로 안다).
+//   drain 은 읽는 쪽에서만 부른다(쓰는 쪽이 한창 쓰는 중이어도 안전하다 — 읽는 위치만 옮긴다).
+template <unsigned CAP>
+class Ring {
+    static_assert(CAP >= 2 && (CAP & (CAP - 1)) == 0, "CAP must be a power of two");
+
+public:
+    size_t size() const { return (size_t)(w_.load(std::memory_order_acquire) - r_.load(std::memory_order_acquire)); }
+    size_t space() const { return CAP - size(); }
+
+    size_t write(const int16_t *src, size_t n)      // 쓰는 태스크에서만
+    {
+        const uint32_t w = w_.load(std::memory_order_relaxed);
+        const size_t room = CAP - (size_t)(w - r_.load(std::memory_order_acquire));
+        if (n > room) n = room;
+        for (size_t i = 0; i < n; ++i) buf_[(w + i) & (CAP - 1)] = src[i];
+        w_.store(w + (uint32_t)n, std::memory_order_release);
+        return n;
+    }
+
+    size_t read(int16_t *dst, size_t n)             // 읽는 태스크에서만
+    {
+        const uint32_t r = r_.load(std::memory_order_relaxed);
+        const size_t have = (size_t)(w_.load(std::memory_order_acquire) - r);
+        if (n > have) n = have;
+        for (size_t i = 0; i < n; ++i) dst[i] = buf_[(r + i) & (CAP - 1)];
+        r_.store(r + (uint32_t)n, std::memory_order_release);
+        return n;
+    }
+
+    void drain() { r_.store(w_.load(std::memory_order_acquire), std::memory_order_release); }   // 읽는 태스크에서만
+
+private:
+    int16_t buf_[CAP];
+    std::atomic<uint32_t> w_{0}, r_{0};
+};
+
+// PL3b-D4 출력 박자 감시: 한 묶음을 다 쓴 시각을 넣으면, 앞 묶음과의 간격이 limit_us 이상일 때 "늦음"으로 센다.
+//   출력 태스크는 정상이면 16ms 마다 한 묶음을 쓴다. 간격이 DMA 가 앞서 쥔 양(칸 수 × 16ms)을 넘으면 그 사이 스피커에 낼 것이 바닥난 것이다.
+//   쉬었다가(낼 것이 없어 잠들었다가) 다시 쓰는 첫 묶음은 세지 않는다 — rest() 로 알린다.
+struct GapWatch {
+    int64_t last_us = 0;
+    uint32_t late = 0, max_us = 0;
+    void rest() { last_us = 0; }
+    void wrote(int64_t now_us, int64_t limit_us)
+    {
+        if (last_us) {
+            const int64_t gap = now_us - last_us;
+            if (gap > (int64_t)max_us) max_us = (uint32_t)(gap > 0xFFFFFFFFll ? 0xFFFFFFFFll : gap);
+            if (gap >= limit_us) ++late;
+        }
+        last_us = now_us;
+    }
+};
+
+}  // namespace pcm
```

- [x] **Step 3: 커밋** (헤더와 시험뿐이다 — 아직 아무도 `pcm.h` 를 포함하지 않아 펌웨어 빌드에 영향이 없다)

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/main/pcm.h firmware/tab5_phone/tools/test_pcm.cpp && git commit -m "feat(phone): 소리 계산 헤더 pcm.h — 섞기(더하고 넘치면 자름)·크기·세기·태스크 사이 링버퍼·출력 박자 감시(맥 시험 37)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: 소리 장치를 송수신으로 — 마이크(ES7210)·I2S 수신, DMA 3칸, 섞기는 `pcm::mix_sources`

**Files:**
- Modify: `firmware/tab5_phone/main/audio_out.h`, `main/audio_out.cpp`

**Interfaces:**
- Consumes: Task 1 의 `audioout::fail`, Task 2 의 `pcm::mix_sources`·`GapWatch`, `esp_codec_dev` 1.6.2(`es7210_codec_new`·`esp_codec_dev_set_in_channel_gain`·`esp_codec_dev_read`), `driver/i2s_tdm.h`.
- Produces (Task 6 이 이 이름·서명을 그대로 쓴다):
  - `constexpr int audioout::IN_SLOTS = 4`
  - `bool audioout::start(SemaphoreHandle_t i2c_lock, bool amp_on = true)`(인자 하나 늘었다 — 기본값이 있어 지금의 호출 `start(i2c_lock)` 은 그대로 빌드된다)
  - `bool read_mic(int16_t *dst, int frames)`(4칸 × frames 표본, 한 태스크에서만), `bool amp(bool on)`, `bool amp_is_on()`, `uint32_t read_errors()`, `uint32_t late_blocks()`
  - 부팅 줄: `AUDIO ready rate=16000 block=256 vol=100 dma=3 mic=24 amp=<0|1>`(앞의 칸은 그대로, 끝에 셋이 붙는다)
  - 공급자 함수의 조건(`audio_out.h` 의 `source_fn` 주석): 기다리지 않는다, 스택을 크게 쓰지 않는다, 잠에서 깬 직후에는 3묶음을 연달아 요구받는다.
- **이 과제가 3a 의 "송신 채널만"(PL3a-D7 — 이 기기에서 소리를 낸 적이 없는 조합)을 Phase 0 에서 실제로 소리가 났던 설정으로 바꾼다.** 3a 의 귀 확인에서 "소리가 안 난다"가 나와도 고칠 곳이 이 과제다.
- 이 커밋은 빌드된다(사본: 0x244660, 경고 0). 아직 마이크를 읽는 곳은 없다(Task 6).

- [x] **Step 1: 패치를 적용하고 초안과 대조한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft && git apply "$D/patches/T3-01-audio-duplex.patch" && for f in audio_out.h audio_out.cpp; do cmp "$D/files/firmware/tab5_phone/main/$f" "firmware/tab5_phone/main/$f" && echo "같음: $f"; done
```
Expected: `같음:` 2줄.

```diff
--- a/firmware/tab5_phone/main/audio_out.h
+++ b/firmware/tab5_phone/main/audio_out.h
@@ -1,7 +1,8 @@
-// audio_out.h — 스피커 출력(ES8388 코덱 + I2S0 송신). "PCM 을 받아 낸다" — 무엇을 낼지는 모른다.
-// 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md PL3a-D3·D4·D7~D10.
-//   - 출력 태스크가 BLOCK 표본(16ms)마다 등록된 공급자(source_fn)에게 PCM 을 받아 합쳐 코덱에 쓴다. 신호음(tone_out)이 공급자 하나이고,
-//     3b 의 목소리가 둘째가 된다. 낼 것이 없으면 쓰지 않고 잠든다 — I2S 는 계속 돌며 0 을 낸다(앰프는 켜 둔다, PL3a-D4).
+// audio_out.h — 소리 장치: 스피커 출력(ES8388 코덱 + I2S0 송신)과 마이크 입력(ES7210 + I2S0 수신, Phase 3b).
+// 출력은 "PCM 을 받아 낸다" — 무엇을 낼지는 모른다. 입력은 "마이크 칩의 4칸을 읽어 준다" — 무엇에 쓸지는 모른다(voice.cpp 가 쓴다).
+// 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md PL3a-D3·D4·D8~D10, docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md PL3b-D2~D4·D10.
+//   - 출력 태스크가 BLOCK 표본(16ms)마다 등록된 공급자(source_fn)에게 PCM 을 받아 합쳐(pcm::mix_sources) 코덱에 쓴다. 신호음(tone_out)과
+//     목소리(voice)가 공급자다. 낼 것이 없으면 쓰지 않고 잠든다 — I2S 는 계속 돌며 0 을 낸다(PL3a-D4. 마이크가 같은 클럭을 쓰므로 송신을 끄지 않는다).
 //   - 실패해도(코덱이 대답하지 않음 등) 전화기는 소리 없이 계속 돈다: start 가 false 와 "AUDIO FAIL <단계>" 한 줄(PL3a-D10).
 //     Phase 3b(PL3b-D2): 부르는 쪽(phone_main)이 boot_guard 로 3번까지 다시 켜 본 뒤에 그렇게 된다. 실패하면 앰프를 끈 채로 둔다.
 #pragma once
@@ -15,12 +16,17 @@
 constexpr int RATE = 16000;                 // 표본율(PH-D5). 16비트 모노
 constexpr int BLOCK = 256;                  // 한 번에 내는 표본 수 = I2S DMA 한 칸(16ms, PL3a-D8)
 
+constexpr int IN_SLOTS = 4;                 // 마이크 칩이 한 번에 내는 칸 수(TDM): 칸0 = 마이크, 칸1 = 스피커 되먹임(에코 기준), 칸2·3 = 안 쓴다(Phase 0 실측)
+
 // 공급자: 출력 태스크가 BLOCK 마다 부른다. 낼 소리가 있으면 out 에 n 표본을 쓰고 true, 없으면 false(out 은 쓰지 않아도 된다).
-// 출력 태스크에서 불리므로 오래 걸리면 안 된다(묶음 하나 = 16ms).
+// 공급자의 조건(3a 최종 검수 5항): 출력 태스크(우선순위 6, 스택 6KB) 안에서 불린다 → 기다리지 않는다(잠금 대기·printf·코덱 해제 금지),
+//   스택을 크게 쓰지 않는다, 1ms 안에 끝낸다. 목소리처럼 무거운 일(Opus 해제)은 다른 태스크가 하고 공급자는 링버퍼에서 복사만 한다.
+//   출력 태스크는 잠에서 깬 직후 DMA 가 찰 때까지 DMA 칸 수만큼(3묶음)을 연달아 요구한다 — 그때 줄 것이 없으면 무음을 준다.
 typedef bool (*source_fn)(int16_t *out, int n);
 
-// 코덱·I2S 를 열고 앰프를 켜고 출력 태스크를 띄운다. 한 번만. i2c_lock = 터치와 함께 쓰는 M5.In_I2C 잠금(phone_main 이 만든다).
-bool start(SemaphoreHandle_t i2c_lock);
+// 코덱 둘과 I2S 송수신을 열고, 마이크 증폭을 걸고, 앰프를 켜고(amp_on 이 참일 때), 출력 태스크를 띄운다. 한 번만.
+// i2c_lock = 터치와 함께 쓰는 M5.In_I2C 잠금(phone_main 이 만든다).
+bool start(SemaphoreHandle_t i2c_lock, bool amp_on = true);
 bool add_source(source_fn fn);              // 공급자 등록(최대 2). start 앞뒤 어느 때나. 자리가 없으면 false
 void wake();                                // 공급자에게 낼 것이 생겼다 — 잠든 출력 태스크를 깨운다(어느 태스크에서나, 소리 장치가 없으면 아무 일도 안 함)
 
@@ -28,9 +34,21 @@
 // 준비가 실패했을 때도 부른다 — 그래서 "AUDIO FAIL tones 인데 STAT 은 audio=1" 같은 어긋남이 없다(3a 최종 검수 M-3).
 void fail(const char *stage);
 
+// 마이크 칩의 4칸을 frames 표본만큼 읽는다(dst 크기 = frames × IN_SLOTS, 칸0,칸1,칸2,칸3,칸0,…). frames = BLOCK 이면 DMA 한 칸이 찰 때까지(16ms) 기다린다.
+// 실패하면(장치 없음, 1초 안에 못 읽음) false 와 읽기 실패 횟수 +1. 한 태스크에서만 부른다(voice 의 캡처 태스크).
+// 읽지 않는 동안에도 I2S 수신은 돈다(오래된 칸은 드라이버가 버린다) — 다시 읽기 시작하면 앞의 DMA 칸 수만큼은 묵은 소리다(부르는 쪽이 버린다).
+bool read_mic(int16_t *dst, int frames);
+
+// 스피커 앰프를 켜거나 끈다(어느 태스크에서나 — I2C 잠금 안에서 한다). 끄면 스피커는 조용하지만 코덱은 그대로 돌아 되먹임 칸(칸1)에는 소리가 잡힌다.
+// 소리 경로가 실패한 뒤에는 켜지지 않는다. 돌려주는 값 = 성공.
+bool amp(bool on);
+bool amp_is_on();
+
 bool ready();                               // 소리 경로 전체가 준비됐나(STAT audio=). fail 이 한 번이라도 불렸으면 false
 void block_us(uint32_t *avg, uint32_t *max);   // 소리를 낸 묶음 하나를 만드는 데(공급자 호출 + 합치기) 걸린 시간의 평균·최대(µs, 부팅 뒤). 묶음 하나 = 16000µs(STAT aud_us=평균/최대)
 uint32_t write_errors();                    // 코덱 쓰기 실패 횟수(STAT aud_err=)
+uint32_t read_errors();                     // 마이크 읽기 실패 횟수
+uint32_t late_blocks();                     // 출력이 늦은 횟수: 앞 묶음을 쓴 뒤 DMA 가 쥔 양(3칸 = 48ms) 넘게 지나서야 다음 묶음을 썼다 = 그 사이 스피커가 끊겼다(STAT aud_late=)
 unsigned stack_free();                      // 출력 태스크 스택 최저 여유(바이트, 태스크가 없으면 0 — STAT stk_audio=)
 
 }  // namespace audioout
--- a/firmware/tab5_phone/main/audio_out.cpp
+++ b/firmware/tab5_phone/main/audio_out.cpp
@@ -1,17 +1,24 @@
-// audio_out.cpp — 스피커 출력. 설명은 audio_out.h.
+// audio_out.cpp — 소리 장치(스피커 출력 + 마이크 입력). 설명은 audio_out.h.
 //
-// 근거(줄 단위 대조, 사용자 지시 2026-10-03): 이 기기에서 실제로 소리를 낸 Phase 0 시험 펌웨어 firmware/tab5_phone_probe/main/board_audio.cpp 와
-//   그 원본 xiaozhi-esp32 main/boards/m5stack/tab5/tab5_audio_codec.cc (MIT). 핀(MCLK30/BCLK27/WS29/DOUT26)·주소(ES8388 0x10, 앰프 = 확장칩 0x43 의 P1)·
-//   코덱 제어 인터페이스(M5.In_I2C + 잠금)·ES8388 설정(DAC, 따라가는 쪽, pa -1, 5.0V/3.3V)·여는 형식(16비트 1채널 16kHz)은 시험 펌웨어 그대로다.
-//   다른 점:
-//   PL3a-D7 송신 채널 하나만 만든다(마이크 칩 ES7210·수신 채널은 3b). Phase 0 부팅 로그(tab5_phone_probe/out/task4.log 177~181)에서
-//          수신을 열기 전의 송신이 지금과 같은 설정(STD TX data 16, slot 16, MONO, mask 0x1)으로 열린 것을 확인했다.
-//          비용: 3b 가 마이크를 붙일 때 i2s_new_channel 을 송수신 한 번에 만드는 모양(시험 펌웨어 i2s_setup)으로 바꿔야 한다 — 그때 송신 슬롯이
-//          32비트로 바뀌는 것도 Phase 0 와 같다. 탈출구: 시험 펌웨어 board_audio.cpp 의 i2s_setup·init 을 그대로 가져온다.
-//   PL3a-D8 DMA 한 칸을 256표본으로 적는다(시험 펌웨어는 기본 240 → 드라이버가 64바이트 맞춤으로 256 으로 바꾸며 경고, task4.log:177).
-//          쓰기 단위(BLOCK)를 DMA 칸과 같게 해야 쉬었다 다시 쓸 때 칸 중간부터 이어 쓰지 않는다(i2s_channel_write 의 rw_pos).
-//   PL3a-D2 음량 100(0dB) 고정 — 크기는 공급자가 디지털로 정한다(tones.h). 시험 펌웨어는 50 으로 열고 명령으로 바꿨다.
-//   PL3a-D4 앰프는 시작 때 한 번 켜고 끄지 않는다. 코덱을 연 뒤 30ms 쉬고 켠다(코덱 출력이 자리 잡은 뒤 — 켤 때 퍽 소리를 줄이려는 것, 효과는 귀로만 확인).
+// 근거(줄 단위 대조, 사용자 지시 2026-10-03): 이 기기에서 실제로 마이크·스피커를 함께 돌린 Phase 0 시험 펌웨어
+//   firmware/tab5_phone_probe/main/board_audio.cpp 와 그 원본 xiaozhi-esp32 main/boards/m5stack/tab5/tab5_audio_codec.cc (MIT).
+//   시험 펌웨어 그대로인 것: 핀(MCLK30/BCLK27/WS29/DOUT26/DIN28)·주소(ES8388 0x10, ES7210 0x40, 앰프 = 확장칩 0x43 의 P1)·
+//   코덱 제어 인터페이스(M5.In_I2C + 잠금)·I2S 를 송수신 한 번에 만드는 것(i2s_setup: 송신 표준 I2S, 수신 4칸 TDM)·ES8388 설정(DAC,
+//   따라가는 쪽, pa -1, 5.0V/3.3V)·ES7210 설정(MIC1~4)·여는 순서(출력 → 입력 → 마이크 증폭 → 음량 → 앰프)·여는 형식(출력 16비트 1채널,
+//   입력 16비트 4채널 mask 0x0F, 16kHz)·증폭을 거는 호출(esp_codec_dev_set_in_channel_gain)·읽기/쓰기 호출.
+//   Phase 3b: 3a 의 "송신 채널만"(PL3a-D7 — 이 기기에서 소리를 낸 적이 없던 조합)을 Phase 0 에서 실제로 소리가 났던 위 설정으로 바꿨다.
+//   시험 펌웨어와 다른 점(전부 계획서 "확인하지 못한 것"에 있다):
+//   PL3a-D8 DMA 한 칸을 256표본으로 적는다(시험 펌웨어는 기본 240 → 송신은 드라이버가 64바이트 맞춤으로 256 으로 바꾸며 경고, task4.log:177·185.
+//          수신 4칸은 240 그대로였다). 쓰기·읽기 단위(BLOCK)가 DMA 한 칸과 같다 = 16ms 박자가 칸과 맞는다. 수신 한 칸 = 4칸 × 2바이트 × 256 = 2048 B.
+//   PL3b-D4 DMA 칸 수 6 → 3. 출력은 늘 (칸 수 − 1) × 16ms 앞서 쓰여 있다(i2s_common.c: 빈 칸 대기열이 칸 수 − 1) → 6칸이면 80ms 가
+//          그대로 통화 지연이 된다. 3칸이면 32ms. 비용: 출력 태스크가 48ms 넘게 늦으면 스피커가 끊긴다(6칸이면 96ms) — 늦은 횟수를 센다(aud_late).
+//          마이크 쪽도 3칸이라 캡처 태스크가 48ms 넘게 못 읽으면 그만큼 잃는다. 탈출구: DMA_DESC 를 4(48ms)나 6으로.
+//   PL3b-D10 마이크 증폭 24dB(시험 펌웨어의 기본은 30dB, 24dB 는 gain 명령으로 재 봤다 — 음량 100 에서 에코 포화 0%). 되먹임 칸(MIC3)은 0dB 를
+//          명시해서 건다. 건 뒤에 칩의 증폭 레지스터를 되읽어 확인한다 — esp_codec_dev_set_in_channel_gain 은 쓰기가 실패해도 OK 를
+//          돌려주기 때문이다(esp_codec_dev.c:477-493). 이 줄이 빠지면 마이크가 24dB 작게 잡힌다(Phase 0 에서 겪은 것).
+//   PL3a-D2 음량 100(0dB) 고정 — 크기는 공급자가 디지털로 정한다(tones.h, voice_core.h). 시험 펌웨어는 50 으로 열고 명령으로 바꿨다.
+//   PL3a-D4 앰프는 시작 때 한 번 켠다. 코덱을 연 뒤 30ms 쉬고 켠다(코덱 출력이 자리 잡은 뒤 — 켤 때 퍽 소리를 줄이려는 것, 효과는 귀로만 확인).
+//          Phase 3b: start 의 amp_on 이 거짓이면 켜지 않고(조용한 시험 — PL3b-D14), amp() 로 켜고 끌 수 있다.
 #include "audio_out.h"
 
 #include <M5Unified.h>
@@ -20,38 +27,46 @@
 #include <cstring>
 
 #include "driver/i2s_std.h"
+#include "driver/i2s_tdm.h"
 #include "esp_codec_dev.h"
 #include "esp_codec_dev_defaults.h"
 #include "esp_log.h"
 #include "esp_timer.h"
 #include "freertos/task.h"
 
+#include "pcm.h"
+
 namespace audioout {
 namespace {
 
 const char *TAG = "audio_out";
 
-constexpr uint8_t ES8388_ADDR = 0x10, PI4IO1_ADDR = 0x43;
+constexpr uint8_t ES8388_ADDR = 0x10, ES7210_ADDR = 0x40, PI4IO1_ADDR = 0x43;
 constexpr uint32_t I2C_FREQ = 400000;
-constexpr gpio_num_t PIN_MCLK = GPIO_NUM_30, PIN_BCLK = GPIO_NUM_27, PIN_WS = GPIO_NUM_29, PIN_DOUT = GPIO_NUM_26;
+constexpr gpio_num_t PIN_MCLK = GPIO_NUM_30, PIN_BCLK = GPIO_NUM_27, PIN_WS = GPIO_NUM_29, PIN_DOUT = GPIO_NUM_26, PIN_DIN = GPIO_NUM_28;
 constexpr int VOLUME = 100;                     // PL3a-D2
-constexpr int DMA_DESC = 6;                     // DMA 칸 수 — 시험 펌웨어·xiaozhi 와 같다(6 × 16ms = 96ms 앞서 쓴다)
-constexpr int MAX_SOURCES = 2;                  // 신호음 + (3b) 목소리
+constexpr int DMA_DESC = 3;                     // PL3b-D4: DMA 칸 수. 출력은 (3 − 1) × 16ms = 32ms 앞서 쓰여 있다
+constexpr int64_t LATE_US = (int64_t)DMA_DESC * BLOCK * 1000000 / RATE;   // 48ms: 앞 묶음 뒤 이만큼 지나서야 쓰면 그 사이 DMA 가 바닥났다
+constexpr int MIC_GAIN_DB = 24;                 // PL3b-D10: 마이크(MIC1 = 칸0)의 증폭. 3dB 단위, 30 이하
+constexpr uint8_t ES7210_MIC1_GAIN_REG = 0x43, ES7210_MIC3_GAIN_REG = 0x45;   // 아래 4비트 = 증폭 ÷ 3dB (es7210.c: _es7210_set_channel_gain)
+static_assert(MIC_GAIN_DB % 3 == 0 && MIC_GAIN_DB >= 0 && MIC_GAIN_DB <= 30, "ES7210 gain steps are 3 dB up to 30 dB");
+constexpr int MAX_SOURCES = 2;                  // 신호음 + 목소리
 // PL3a-D9 출력 태스크: 코어 1(화면·터치와 같은 코어, Wi-Fi·교환기 연결은 코어 0), 우선순위 6(app 4·touch 5 보다 위 — 화면을 내보내는
 //   60~80ms 동안에도 16ms 박자를 지킨다. 한 묶음 계산은 1ms 에 못 미쳐 화면을 늦추지 않는다 — 어림이고 실제 값은 STAT aud_us), 스택 6KB(printf 한 줄 + 코덱 쓰기. STAT stk_audio 로 확인).
 constexpr int TASK_CORE = 1, TASK_PRIO = 6, TASK_STACK = 6 * 1024;
 
 SemaphoreHandle_t s_lock = nullptr;
-i2s_chan_handle_t s_tx = nullptr;
-esp_codec_dev_handle_t s_out = nullptr;
+i2s_chan_handle_t s_tx = nullptr, s_rx = nullptr;
+esp_codec_dev_handle_t s_out = nullptr, s_in = nullptr;
 TaskHandle_t s_task = nullptr;
 std::atomic<bool> s_ready{false};
 std::atomic<source_fn> s_src[MAX_SOURCES];
 std::atomic<bool> s_failed{false}, s_amp{false};
-std::atomic<uint32_t> s_max_us{0}, s_avg_us{0}, s_wr_err{0}, s_i2c_err{0};
+std::atomic<uint32_t> s_max_us{0}, s_avg_us{0}, s_wr_err{0}, s_rd_err{0}, s_i2c_err{0}, s_late{0};
 int16_t s_mix[BLOCK], s_tmp[BLOCK];             // 출력 태스크에서만 쓴다
 uint64_t s_sum_us = 0;                          // 출력 태스크에서만 쓴다. 3a 최종 검수 M-5: 32비트 합계는 소리 낸 누적 63시간에 넘친다(통화는 계속 소리를 낸다)
 uint32_t s_blocks = 0;
+pcm::GapWatch s_watch;                          // 출력 태스크에서만 쓴다(PL3b-D4)
 
 // ── 코덱 제어 인터페이스: M5.In_I2C 로 레지스터 읽기·쓰기(시험 펌웨어 board_audio.cpp 그대로) ──────────────────
 struct m5_ctrl_t { audio_codec_ctrl_if_t base; uint8_t addr; bool opened; };
@@ -93,24 +108,38 @@
     m.addr = addr; m.opened = true;             // 코덱 드라이버는 "이미 열린" 인터페이스를 요구한다
     return m;
 }
-m5_ctrl_t s_ctrl_out = make_ctrl(ES8388_ADDR);
+m5_ctrl_t s_ctrl_out = make_ctrl(ES8388_ADDR), s_ctrl_in = make_ctrl(ES7210_ADDR);
 
-bool codec_answers()                            // 코덱이 I2C 에 대답하나(레지스터 하나 읽기 — 시험 펌웨어 health_check 의 ES8388 쪽)
+bool codecs_answer()                            // 두 코덱이 I2C 에 대답하나(레지스터 하나씩 읽기 — 시험 펌웨어 health_check 그대로)
 {
-    uint8_t v = 0;
+    uint8_t a = 0, b = 0;
     xSemaphoreTake(s_lock, portMAX_DELAY);
-    const bool ok = M5.In_I2C.readRegister(ES8388_ADDR, 0x19, &v, 1, I2C_FREQ);
+    const bool ok1 = M5.In_I2C.readRegister(ES8388_ADDR, 0x19, &a, 1, I2C_FREQ);
+    const bool ok2 = M5.In_I2C.readRegister(ES7210_ADDR, 0x40, &b, 1, I2C_FREQ);
     xSemaphoreGive(s_lock);
-    return ok;
+    return ok1 && ok2;
 }
 
-bool i2s_setup()                                // PL3a-D7·D8: 송신 채널 하나(표준 I2S), DMA 6칸 × 256표본
+// 시험 펌웨어 board_audio.cpp 의 i2s_setup 그대로(PROBE_TX_TDM=0 쪽) + DMA 칸 수·크기를 적는다(PL3a-D8, PL3b-D4).
+// 송신과 수신을 한 번의 i2s_new_channel 로 만든다: 5.5.4 드라이버는 한 번에 만들 때만 둘을 한 쌍으로 표시한다(i2s_common.c:1064-1066).
+// 따로 만들면 설정이 완전히 같은 표준 모드일 때만 묶이는데(i2s_std.c:244-262) 이 기판은 송신 표준 + 수신 4칸 TDM 이라 클럭을 나누지 못한다.
+bool i2s_setup()
 {
     i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
     chan_cfg.dma_desc_num = DMA_DESC;
     chan_cfg.dma_frame_num = BLOCK;
     chan_cfg.auto_clear_after_cb = true;        // PL3a-D4: 보낸 칸은 0 으로 지운다 → 쓰지 않으면 무음(잡음 대신)
-    if (i2s_new_channel(&chan_cfg, &s_tx, nullptr) != ESP_OK) return false;
+    if (i2s_new_channel(&chan_cfg, &s_tx, &s_rx) != ESP_OK) return false;
+
+    const i2s_tdm_slot_mask_t all4 = (i2s_tdm_slot_mask_t)(I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3);
+    i2s_tdm_config_t rx_cfg = {
+        .clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(RATE),
+        .slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, all4),
+        .gpio_cfg = {
+            .mclk = PIN_MCLK, .bclk = PIN_BCLK, .ws = PIN_WS, .dout = I2S_GPIO_UNUSED, .din = PIN_DIN,
+            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
+        },
+    };
     i2s_std_config_t tx_cfg = {
         .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(RATE),
         .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
@@ -119,13 +148,43 @@
             .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
         },
     };
-    return i2s_channel_init_std_mode(s_tx, &tx_cfg) == ESP_OK && i2s_channel_enable(s_tx) == ESP_OK;
+    if (i2s_channel_init_std_mode(s_tx, &tx_cfg) != ESP_OK) return false;
+    if (i2s_channel_init_tdm_mode(s_rx, &rx_cfg) != ESP_OK) return false;
+    return i2s_channel_enable(s_tx) == ESP_OK && i2s_channel_enable(s_rx) == ESP_OK;
 }
 
-bool codec_open()                               // ES8388 을 DAC 로 열고 음량을 건다(시험 펌웨어 init 의 출력 쪽)
+uint8_t es7210_reg(uint8_t reg, bool *ok)       // 마이크 칩 레지스터 하나 읽기(codecs_answer 와 같은 읽기)
 {
+    uint8_t v = 0;
+    xSemaphoreTake(s_lock, portMAX_DELAY);
+    *ok = M5.In_I2C.readRegister(ES7210_ADDR, reg, &v, 1, I2C_FREQ);
+    xSemaphoreGive(s_lock);
+    return v;
+}
+
+// PL3b-D10 마이크 증폭은 입력 장치를 "연 뒤에" 건다: es7210 드라이버가 장치를 켜면서(es7210_start → mic_select) 증폭을 드라이버 기본값으로
+// 덮어쓴다(Phase 0 에서 이 줄이 없어 마이크가 29dB 작게 잡혔다). 입력 0(MIC1) = 칸0 = 마이크, 입력 2(MIC3) = 칸1 = 되먹임(0dB — 30dB 면 포화).
+// 입력 장치를 닫았다 열거나 껐다 켜면 드라이버가 다시 지우므로 그때마다 다시 불러야 한다(지금은 부팅 때 한 번 열고 닫지 않는다).
+bool mic_gain_apply()
+{
+    if (esp_codec_dev_set_in_channel_gain(s_in, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), (float)MIC_GAIN_DB) != ESP_CODEC_DEV_OK) return false;
+    if (esp_codec_dev_set_in_channel_gain(s_in, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(2), 0.0f) != ESP_CODEC_DEV_OK) return false;
+    bool ok1 = false, ok3 = false;              // 위 호출은 쓰기가 실패해도 OK 를 돌려준다 → 칩에서 되읽는다
+    const uint8_t g1 = es7210_reg(ES7210_MIC1_GAIN_REG, &ok1), g3 = es7210_reg(ES7210_MIC3_GAIN_REG, &ok3);
+    if (!ok1 || !ok3 || (g1 & 0x0F) != MIC_GAIN_DB / 3 || (g3 & 0x0F) != 0) {
+        ESP_LOGE(TAG, "mic gain not applied: read %d/%d reg43=0x%02x reg45=0x%02x (want low nibble %d and 0)", (int)ok1, (int)ok3, g1, g3, MIC_GAIN_DB / 3);
+        return false;
+    }
+    return true;
+}
+
+// 시험 펌웨어 board_audio.cpp 의 init 그대로: 데이터 인터페이스(송수신) → ES8388(DAC) → ES7210(MIC1~4) → 출력 열기 → 입력 열기 → 증폭 → 음량.
+// 입력을 열 때 esp_codec_dev 가 송신을 슬롯 32비트로 다시 설정한다(task4.log:182~187) — 그동안의 쓰기는 10ms 쉬고 버려진다
+// (audio_codec_data_i2s.c:735-738). 그래서 여기서 전부 연 "뒤에" 출력 태스크를 띄운다(start).
+bool codec_open()
+{
     audio_codec_i2s_cfg_t i2s_cfg = {};
-    i2s_cfg.port = I2S_NUM_0; i2s_cfg.rx_handle = nullptr; i2s_cfg.tx_handle = s_tx;
+    i2s_cfg.port = I2S_NUM_0; i2s_cfg.rx_handle = s_rx; i2s_cfg.tx_handle = s_tx;
     const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);
     const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
     if (!data_if || !gpio_if) return false;
@@ -139,13 +198,27 @@
     const audio_codec_if_t *out_codec = es8388_codec_new(&out_cfg);
     if (!out_codec) return false;
 
+    es7210_codec_cfg_t in_cfg = {};
+    in_cfg.ctrl_if = &s_ctrl_in.base;
+    in_cfg.mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3 | ES7210_SEL_MIC4;
+    const audio_codec_if_t *in_codec = es7210_codec_new(&in_cfg);
+    if (!in_codec) return false;
+
     esp_codec_dev_cfg_t dev_cfg = {};
     dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_OUT; dev_cfg.codec_if = out_codec; dev_cfg.data_if = data_if;
     s_out = esp_codec_dev_new(&dev_cfg);
-    if (!s_out) return false;
-    esp_codec_dev_sample_info_t fs = {};
-    fs.bits_per_sample = 16; fs.channel = 1; fs.channel_mask = 0; fs.sample_rate = RATE;
-    if (esp_codec_dev_open(s_out, &fs) != ESP_CODEC_DEV_OK || esp_codec_dev_set_out_vol(s_out, VOLUME) != ESP_CODEC_DEV_OK) return false;
+    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN; dev_cfg.codec_if = in_codec;
+    s_in = esp_codec_dev_new(&dev_cfg);
+    if (!s_out || !s_in) return false;
+
+    esp_codec_dev_sample_info_t fs_out = {};
+    fs_out.bits_per_sample = 16; fs_out.channel = 1; fs_out.channel_mask = 0; fs_out.sample_rate = RATE;
+    if (esp_codec_dev_open(s_out, &fs_out) != ESP_CODEC_DEV_OK) return false;
+    esp_codec_dev_sample_info_t fs_in = {};
+    fs_in.bits_per_sample = 16; fs_in.channel = IN_SLOTS; fs_in.channel_mask = 0x0F; fs_in.sample_rate = RATE;
+    if (esp_codec_dev_open(s_in, &fs_in) != ESP_CODEC_DEV_OK) return false;
+    if (!mic_gain_apply()) return false;
+    if (esp_codec_dev_set_out_vol(s_out, VOLUME) != ESP_CODEC_DEV_OK) return false;
     return s_i2c_err == 0;                      // 음량 설정은 코덱 쓰기가 실패해도 OK 를 돌려준다(esp_codec_dev.c:376~378) → 쓰기 실패 횟수로 확인한다
 }
 
@@ -163,17 +236,14 @@
 {
     while (true) {
         const int64_t t0 = esp_timer_get_time();
-        bool any = false;
-        for (int i = 0; i < MAX_SOURCES; ++i) {
-            const source_fn fn = s_src[i].load();
-            if (!fn || !fn(s_tmp, BLOCK)) continue;
-            if (!any) { memcpy(s_mix, s_tmp, sizeof s_mix); any = true; continue; }
-            for (int k = 0; k < BLOCK; ++k) {   // 둘째 공급자부터는 더한다(넘치면 자른다) — 3b 의 목소리 + 신호음
-                const int32_t v = (int32_t)s_mix[k] + s_tmp[k];
-                s_mix[k] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
-            }
+        source_fn fns[MAX_SOURCES];
+        for (int i = 0; i < MAX_SOURCES; ++i) fns[i] = s_src[i].load();
+        // PL3b-D3: 섞기는 순수 함수(pcm.h — 맥 시험 "mix: …")다. 신호음과 목소리가 함께 소리를 내면 더하고 넘치면 자른다.
+        if (pcm::mix_sources(fns, MAX_SOURCES, s_mix, s_tmp, BLOCK) == 0) {
+            s_watch.rest();
+            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);                        // PL3a-D4: 낼 것이 없으면 쓰지 않고 잠든다(DMA 가 0 을 낸다)
+            continue;
         }
-        if (!any) { ulTaskNotifyTake(pdTRUE, portMAX_DELAY); continue; }    // PL3a-D4: 낼 것이 없으면 쓰지 않고 잠든다(DMA 가 0 을 낸다)
         const uint32_t us = (uint32_t)(esp_timer_get_time() - t0);          // 소리를 낸 묶음만 잰다
         if (us > s_max_us.load()) s_max_us = us;
         s_sum_us += us; ++s_blocks;
@@ -182,31 +252,46 @@
             ++s_wr_err;
             vTaskDelay(pdMS_TO_TICKS(16));      // 쓰기가 바로 실패해도 박자를 지킨다 — 쉬지 않고 돌면 우선순위가 낮은 화면·터치 태스크가 굶는다(PL3a-D10)
         }
+        s_watch.wrote(esp_timer_get_time(), LATE_US);                       // PL3b-D4: 48ms 넘게 걸려서야 다음 묶음을 썼으면 그 사이 스피커가 끊겼다
+        s_late = s_watch.late;
     }
 }
 
 }  // namespace
 
-bool start(SemaphoreHandle_t i2c_lock)
+bool start(SemaphoreHandle_t i2c_lock, bool amp_on)
 {
     if (s_ready || s_failed || s_tx) return s_ready;   // 한 번만(두 번째 호출은 첫 결과)
     s_lock = i2c_lock;
     const char *stage = nullptr;                // 실패한 단계
     if (!s_lock) stage = "lock";
-    else if (!codec_answers()) stage = "i2c";
+    else if (!codecs_answer()) stage = "i2c";
     else if (!i2s_setup()) stage = "i2s";
     else if (!codec_open()) stage = "codec";
     else {
         vTaskDelay(pdMS_TO_TICKS(30));          // PL3a-D4: 코덱 출력이 자리 잡은 뒤 앰프를 켠다
-        if (!amp_set(true)) stage = "amp";
-        else if (xTaskCreatePinnedToCore(out_task, "audio", TASK_STACK, nullptr, TASK_PRIO, &s_task, TASK_CORE) != pdPASS) stage = "task";
+        if (amp_on && !amp_set(true)) stage = "amp";
+        else if (xTaskCreatePinnedToCore(out_task, "audio", TASK_STACK, nullptr, TASK_PRIO, &s_task, TASK_CORE) != pdPASS) stage = "task";   // 전부 연 뒤에 띄운다
     }
     if (stage) { fail(stage); return false; }   // PL3b-D2: phone_main 의 boot_guard 가 3번까지 재시작하고, 그래도 안 되면 소리 없이 계속 돈다
     s_ready = true;
-    printf("AUDIO ready rate=%d block=%d vol=%d\n", RATE, BLOCK, VOLUME);
+    printf("AUDIO ready rate=%d block=%d vol=%d dma=%d mic=%d amp=%d\n", RATE, BLOCK, VOLUME, DMA_DESC, MIC_GAIN_DB, (int)s_amp.load());
     return true;
 }
 
+bool read_mic(int16_t *dst, int frames)         // 시험 펌웨어 board_audio.cpp 의 read_slots 그대로
+{
+    if (!s_in || !s_ready || esp_codec_dev_read(s_in, dst, frames * IN_SLOTS * (int)sizeof(int16_t)) != ESP_CODEC_DEV_OK) { ++s_rd_err; return false; }
+    return true;
+}
+
+bool amp(bool on)
+{
+    if (!s_lock || !s_out || (on && s_failed)) return false;
+    return amp_set(on);
+}
+bool amp_is_on() { return s_amp; }
+
 bool add_source(source_fn fn)
 {
     for (auto &slot : s_src) {
@@ -236,6 +321,8 @@
     if (max) *max = s_max_us.load();
 }
 uint32_t write_errors() { return s_wr_err; }
+uint32_t read_errors() { return s_rd_err; }
+uint32_t late_blocks() { return s_late; }
 unsigned stack_free() { return s_task ? (unsigned)uxTaskGetStackHighWaterMark(s_task) : 0; }
 
 }  // namespace audioout
```

- [x] **Step 2: 이 기기에서 마이크·스피커를 함께 돌린 시험 펌웨어와 줄 단위로 대조한다** (이 파일은 맥 시험이 없다 — 대조 결과를 보고에 적는다)

```bash
cd ~/tab5-toy-phone/firmware && for k in "ES8388_ADDR = 0x10, ES7210_ADDR = 0x40, PI4IO1_ADDR = 0x43" "I2C_FREQ = 400000" "PIN_MCLK = GPIO_NUM_30, PIN_BCLK = GPIO_NUM_27, PIN_WS = GPIO_NUM_29, PIN_DOUT = GPIO_NUM_26, PIN_DIN = GPIO_NUM_28" "info->i2c.port = 1" "auto_clear_after_cb = true" "i2s_new_channel(&chan_cfg, &s_tx, &s_rx)" "I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3" "I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, all4)" ".mclk = PIN_MCLK, .bclk = PIN_BCLK, .ws = PIN_WS, .dout = I2S_GPIO_UNUSED, .din = PIN_DIN" "I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)" ".mclk = PIN_MCLK, .bclk = PIN_BCLK, .ws = PIN_WS, .dout = PIN_DOUT, .din = I2S_GPIO_UNUSED" "i2s_channel_init_tdm_mode(s_rx, &rx_cfg)" "i2s_cfg.port = I2S_NUM_0; i2s_cfg.rx_handle = s_rx; i2s_cfg.tx_handle = s_tx;" "out_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC" "out_cfg.master_mode = false" "out_cfg.pa_pin = -1" "out_cfg.hw_gain.pa_voltage = 5.0; out_cfg.hw_gain.codec_dac_voltage = 3.3" "in_cfg.mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3 | ES7210_SEL_MIC4" "dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN; dev_cfg.codec_if = in_codec;" "fs_in.channel_mask = 0x0F" "esp_codec_dev_set_in_channel_gain(s_in, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(" "esp_codec_dev_read(s_in, dst, frames * IN_SLOTS * (int)sizeof(int16_t))" "M5.In_I2C.readRegister(ES7210_ADDR, 0x40, &b, 1, I2C_FREQ)" "M5.In_I2C.bitOff(PI4IO1_ADDR, 0x05, 0b00000010, I2C_FREQ)"; do echo "$(grep -cF -- "$k" tab5_phone_probe/main/board_audio.cpp) $(grep -cF -- "$k" tab5_phone/main/audio_out.cpp)  $k"; done
```
Expected: 24줄. 모두 `1 1  <글자>` 이고 한 줄만 `1 2  esp_codec_dev_set_in_channel_gain(s_in, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(` 다 — 앞 숫자는 시험 펌웨어, 뒤 숫자는 새 파일에서 그 글자가 나온 횟수다(둘 다 1 = 같은 값을 쓴다. 증폭을 거는 호출만 새 파일에 둘 — 마이크 칸과 되먹임 칸).

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone/main && grep -n "esp_codec_dev_open(s_out\|esp_codec_dev_open(s_in\|if (!mic_gain_apply())\|esp_codec_dev_set_out_vol(s_out, VOLUME)\|amp_on && !amp_set(true)\|xTaskCreatePinnedToCore(out_task\|DMA_DESC = \|MIC_GAIN_DB = \|pcm::mix_sources(\|s_watch.wrote(\|chan_cfg.dma_" audio_out.cpp | cut -c1-100
```
Expected (여는 순서가 줄 번호 순서다: 출력 열기 → 입력 열기 → 마이크 증폭 → 음량 → 앰프 → **그 뒤에** 출력 태스크):
```
48:constexpr int DMA_DESC = 3;                     // PL3b-D4: DMA 칸 수. 출력은 (3 − 1) × 16ms = 32ms 앞서
50:constexpr int MIC_GAIN_DB = 24;                 // PL3b-D10: 마이크(MIC1 = 칸0)의 증폭. 3dB 단위, 30 이하
129:    chan_cfg.dma_desc_num = DMA_DESC;
130:    chan_cfg.dma_frame_num = BLOCK;
216:    if (esp_codec_dev_open(s_out, &fs_out) != ESP_CODEC_DEV_OK) return false;
219:    if (esp_codec_dev_open(s_in, &fs_in) != ESP_CODEC_DEV_OK) return false;
220:    if (!mic_gain_apply()) return false;
221:    if (esp_codec_dev_set_out_vol(s_out, VOLUME) != ESP_CODEC_DEV_OK) return false;
242:        if (pcm::mix_sources(fns, MAX_SOURCES, s_mix, s_tmp, BLOCK) == 0) {
255:        s_watch.wrote(esp_timer_get_time(), LATE_US);                       // PL3b-D4: 48ms 넘게 
273:        if (amp_on && !amp_set(true)) stage = "amp";
274:        else if (xTaskCreatePinnedToCore(out_task, "audio", TASK_STACK, nullptr, TASK_PRIO, &s_t
```

같은 것(위 24줄 + 여는 순서)과 **다른 것 여섯**을 읽어서 확인한다. 다른 것은 전부 "확인하지 못한 것"에 올라 있다:

| 항목 | 시험 펌웨어 `tab5_phone_probe/main/board_audio.cpp` | `audio_out.cpp` | 판단 |
|---|---|---|---|
| DMA | 기본값: 6칸 × 240표본(송신은 드라이버가 256 으로 바꿈, 수신 4칸은 240 그대로) | `dma_desc_num = 3`, `dma_frame_num = 256`(송수신 공통 — 한 번에 만드는 설정이라 나눌 수 없다) | PL3b-D4·PL3a-D8. 수신 한 칸 = 4칸 × 2바이트 × 256 = 2048 B(드라이버 한도 4092 B 안) |
| 마이크 증폭 | 연 뒤 MIC1 에 30dB(24dB 는 `gain` 명령으로 재 봄). MIC3(되먹임)은 건드리지 않음(드라이버가 켜면서 0dB 로 둔 그대로) | 연 뒤 MIC1 에 24dB, MIC3 에 0dB 를 **명시해서** 걸고, 칩의 증폭 레지스터(0x43·0x45 의 아래 4비트 = dB ÷ 3)를 되읽어 확인 — 어긋나면 `AUDIO FAIL codec` | PL3b-D10. 되읽기는 `esp_codec_dev_set_in_channel_gain` 이 쓰기 실패에도 OK 를 돌려주기 때문(의뢰: "반환값 확인"). 레지스터 뜻은 부품 소스 `es7210.c` `_es7210_set_channel_gain`·`get_db` 로 확인했다 |
| 음량 | 50 으로 열고 명령으로 바꿈 | 100 고정 | PL3a-D2 |
| 앰프 | 코덱을 연 직후 켬 | 30ms 쉰 뒤 켬. `amp_on` 이 거짓이면 켜지 않는다. `amp()` 로 켜고 끈다 | PL3a-D4, PL3b-D14 |
| 쓰기·읽기 단위 | 256표본(에코 제거 묶음) | 256표본 = DMA 한 칸 | 같다 |
| 실패했을 때 | 그 자리에서 `false`(시험 펌웨어는 소리 없이 계속) | `fail(<단계>)` → Task 1 의 재시작 가드 | PL3b-D2 |

| 확인할 것 | 코드 |
|---|---|
| 송신·수신을 **한 번의** `i2s_new_channel` 로 만든다(따로 만들면 이 기판에서는 클럭을 나누지 못한다 — 검수가 드라이버 소스로 확인) | `i2s_setup()` `132` |
| 입력을 여는 동안 `esp_codec_dev` 가 송신을 슬롯 32비트로 다시 설정한다(그동안의 쓰기는 10ms 쉬고 버려진다) → **전부 연 뒤에** 출력 태스크를 띄운다 | `start()` `270~274`: `codec_open()` 이 끝난 뒤 `xTaskCreatePinnedToCore(out_task …)` |
| 마이크 증폭은 입력 장치를 **연 뒤에** 건다(드라이버가 켜면서 덮어쓴다 — Phase 0 에서 이 줄이 없어 29dB 작게 잡혔다) | `codec_open()` `219~220` |
| 에코 기준은 하드웨어 되먹임(칸 1)이다 — 소프트웨어로 기준을 만드는 코드가 없다. 신호음과 목소리를 섞은 **뒤의** 소리가 코덱으로 나가고 그것이 그대로 칸 1 에 잡힌다 | `out_task()`: `mix_sources` → `esp_codec_dev_write` 한 곳뿐 |
| 섞는 코드는 순수 함수 호출 하나다(맥 시험 "mix: …" 9개가 고정 — M-7) | `out_task()` `242` |
| 낼 것이 없으면 쓰지 않고 잠든다. 잠들 때 박자 감시를 쉰다(잠은 늦은 것이 아니다) | `out_task()` `242~246` |
| `read_mic` 은 장치가 없거나 읽기가 실패하면 `false` 와 실패 횟수 +1(멈추지 않는다) | `read_mic()` |
| 소리 경로가 실패한 뒤에는 앰프가 켜지지 않는다 | `amp()` 의 `(on && s_failed)` |

- [x] **Step 3: 커밋**

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/main/audio_out.h firmware/tab5_phone/main/audio_out.cpp && git commit -m "feat(phone): 소리 장치 송수신 — ES7210 마이크 + I2S 수신 4칸(Phase 0 설정 그대로), DMA 3칸(32ms), 섞기는 pcm::mix_sources, 앰프 켜고 끄기, 늦은 묶음 세기" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

**묶음 1 끝 — 여기서 검수가 들어간다**(Task 1~3 의 커밋 다섯: 시험 보강 / `relay_link` 잠금 / 소리 실패·`TONE` 줄 / `pcm.h` / 소리 장치 송수신).

---

### Task 4: 지터 버퍼 `jitter_buffer.h` (시험 먼저)

**Files:**
- Create: `firmware/tab5_phone/main/jitter_buffer.h`
- Test: `firmware/tab5_phone/tools/test_jitter_buffer.cpp`

**Interfaces:**
- Consumes: 없음. Phase 2 때 써 둔 초안(`.superpowers/sdd/2026-10-04-tab5-phone-phase2-3/draft/main/jitter_buffer.h`, 시험 42개)을 옮기되 아래 셋을 바꿨다.
- Produces (`namespace jitterbuf` — Task 5 가 쓴다):
  - 상수 `START_FRAMES = 3`(60ms), `MIN_FRAMES = 2`, `MAX_FRAMES = 10`, `OVERFLOW_FRAMES = 15`(300ms), `MAX_CONCEAL = 3`, `MAX_PAYLOAD = 398`
  - `enum class Kind { SILENCE, PLAY, LOST }`, `struct Pop { Kind kind; uint16_t seq; uint16_t len; uint8_t data[MAX_PAYLOAD]; }`, `struct Stats { played, lost, late, dup, overflow_drop, catchup_drop, underruns, resync, bad }`
  - `class Buffer { reset(); push(seq, data, len); Pop pop(); depth(); target_frames(); buffering(); stats(); }` — 잠금이 없다(부르는 쪽이 감싼다).
- 초안과 다른 것: ① **JB-D2** — 다 떨어져서 메울 때 기다리던 순번을 넘기지 않는다(PL3b-D5) ② `Pop.seq`(꺼낸 프레임의 순번 — 메운 뒤 이어 붙일 때 쓴다) ③ `buffering()`.

- [x] **Step 1: 시험 패치만 적용하고 실패를 확인한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft && git apply "$D/patches/T4-01-jitter-tests.patch" && wc -l < firmware/tab5_phone/tools/test_jitter_buffer.cpp && cd firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_jitter_buffer.cpp -o /tmp/t_p3b_jitter_buffer 2>&1 | grep -m1 "error:"
```
Expected: `     355`, 이어서 `test_jitter_buffer.cpp:5:10: fatal error: 'jitter_buffer.h' file not found`

```diff
--- /dev/null
+++ b/firmware/tab5_phone/tools/test_jitter_buffer.cpp
@@ -0,0 +1,355 @@
+// test_jitter_buffer.cpp — jitter_buffer.h 단위 시험:
+//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_jitter_buffer.cpp -o /tmp/t_jb && /tmp/t_jb
+// 설계서 §9.6: 60ms 로 시작해 40~200ms 사이에서 조절, 빈 프레임은 손실 은닉, 300ms 를 넘으면 오래된 것부터 버려 지연이 쌓이지 않게.
+// 뒤쪽의 "오래·거칠게" 시험은 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md Review Focus 3(손실·순서 뒤바뀜·몰림·오래 통화).
+#include "jitter_buffer.h"
+#include <cstdio>
+#include <cstring>
+#include <map>
+#include <vector>
+using namespace jitterbuf;
+static int ok = 0, bad = 0;
+#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)
+
+// 프레임 내용 = 순번의 아래 바이트를 len 번 반복 (꺼낸 것이 어느 프레임인지 확인용)
+static void put(Buffer &b, uint16_t seq, size_t len = 60)
+{
+    uint8_t d[MAX_PAYLOAD];
+    memset(d, (uint8_t)seq, len);
+    b.push(seq, d, len);
+}
+static bool run_dry(Buffer &b) { for (int i = 0; i < 1000; ++i) if (b.pop().kind == Kind::SILENCE) return true; return false; }   // 무음이 나올 때까지 꺼낸다(고장 나도 끝난다)
+static bool is(const Pop &p, uint16_t seq, size_t len = 60) { return p.kind == Kind::PLAY && p.len == len && p.data[0] == (uint8_t)seq && p.data[len - 1] == (uint8_t)seq; }
+
+int main()
+{
+    // ── 처음: 목표 깊이(3프레임 = 60ms)가 찰 때까지 무음, 그 뒤 순서대로 ─────
+    {
+        Buffer b;
+        CHECK("empty: silence", b.pop().kind == Kind::SILENCE);
+        put(b, 100); put(b, 101);
+        CHECK("below target: still silence", b.pop().kind == Kind::SILENCE && b.depth() == 2);
+        put(b, 102);
+        CHECK("target reached: plays first frame", is(b.pop(), 100));
+        CHECK("then in order", is(b.pop(), 101) && is(b.pop(), 102));
+        CHECK("start target is 60ms", b.target_frames() == 3 && START_FRAMES * FRAME_MS == 60);
+        CHECK("limits are 40..200ms and 300ms", MIN_FRAMES * FRAME_MS == 40 && MAX_FRAMES * FRAME_MS == 200 && OVERFLOW_FRAMES * FRAME_MS == 300);
+    }
+
+    // ── 순서가 뒤바뀌어 와도 순번대로 꺼낸다 ────────────────────────────────
+    {
+        Buffer b;
+        put(b, 10); put(b, 12); put(b, 11);
+        CHECK("reorder: 10, 11, 12", is(b.pop(), 10) && is(b.pop(), 11) && is(b.pop(), 12));
+    }
+
+    // ── 한 프레임이 빠졌고 뒤 프레임은 와 있다 → 손실 은닉(LOST) 한 번, 뒤는 그대로 ──
+    {
+        Buffer b;
+        put(b, 1); put(b, 2); put(b, 4); put(b, 5);
+        CHECK("gap: 1, 2", is(b.pop(), 1) && is(b.pop(), 2));
+        CHECK("gap: missing 3 -> LOST", b.pop().kind == Kind::LOST);
+        CHECK("gap: 4 follows", is(b.pop(), 4));
+        CHECK("gap: counted once", b.stats().lost == 1 && b.stats().played == 3);
+    }
+
+    // ── 같은 프레임이 두 번, 이미 지나간 프레임 ─────────────────────────────
+    {
+        Buffer b;
+        put(b, 1); put(b, 1); put(b, 2); put(b, 3);
+        CHECK("duplicate dropped", b.depth() == 3 && b.stats().dup == 1);
+        b.pop(); b.pop();
+        put(b, 1);
+        CHECK("late frame dropped", b.depth() == 1 && b.stats().late == 1);
+        CHECK("late frame did not disturb order", is(b.pop(), 3));
+    }
+
+    // ── 다 떨어졌을 때(망이 잠깐 막힘): 3번까지 은닉, 그 뒤엔 무음으로 다시 채우기를 기다린다 ──
+    {
+        Buffer b;
+        put(b, 1); put(b, 2); put(b, 3);
+        b.pop(); b.pop(); b.pop();
+        CHECK("underrun: conceal 1", b.pop().kind == Kind::LOST);
+        CHECK("underrun: conceal 2", b.pop().kind == Kind::LOST);
+        CHECK("underrun: conceal 3", b.pop().kind == Kind::LOST);
+        CHECK("underrun: then silence (rebuffering)", b.pop().kind == Kind::SILENCE && b.pop().kind == Kind::SILENCE);
+        CHECK("underrun counted once, target grows by one", b.stats().underruns == 1 && b.target_frames() == 4);
+        // 막혔던 프레임이 한꺼번에 온다(4~20, 17개): 메우는 동안 순번을 넘기지 않았으므로(JB-D2) 늦은 것은 없다.
+        // 300ms 를 넘는 순간 가장 새 프레임 '목표 깊이'만큼만 남긴다 — 밀린 만큼 지연이 쌓이지 않게.
+        for (uint16_t s = 4; s <= 20; ++s) put(b, s);
+        CHECK("burst: nothing is late (the concealed frames were still being waited for)", b.stats().late == 0);
+        CHECK("burst: older frames dropped and counted, at most target + 1 kept", b.stats().overflow_drop == (uint32_t)(16 - b.target_frames()) && b.depth() == b.target_frames() + 1);
+        Pop p = b.pop();
+        CHECK("burst: resumes with the newest frames (delay does not pile up)", p.kind == Kind::PLAY && p.data[0] == (uint8_t)(20 - b.target_frames()) && b.depth() == b.target_frames());
+    }
+
+    // ── 끊겼다가(무음) 다시 틀 때: 그 사이 몰려온 것이 300ms 이하여도 목표 깊이만 남기고 버린다 ──
+    {
+        Buffer b;
+        put(b, 1); put(b, 2); put(b, 3);
+        run_dry(b);                // 다 꺼냄 → 메움 3번 → 무음(목표 4)
+        for (uint16_t s = 4; s <= 13; ++s) put(b, s);           // 10개(200ms)가 한꺼번에 — 넘치지는 않는다
+        CHECK("resume after a gap: 10 frames waiting, nothing dropped yet", b.depth() == 10 && b.stats().overflow_drop == 0 && b.target_frames() == 4);
+        const Pop p = b.pop();
+        CHECK("resume after a gap: it starts from the newest 'target' frames, the older ones are dropped (no 200 ms of extra delay)",
+              p.kind == Kind::PLAY && p.seq == 10 && b.depth() == 3 && b.stats().overflow_drop == 6);
+        Buffer c;
+        put(c, 1); put(c, 2); put(c, 3);
+        run_dry(c);
+        for (uint16_t s = 4; s <= 8; ++s) put(c, s);            // 5개: 목표(4)보다 하나 많다 — 한 프레임 여유는 둔다
+        CHECK("resume after a gap: one frame over the target is kept", c.pop().seq == 4 && c.stats().overflow_drop == 0);
+    }
+
+    // ── 꺼내는 쪽이 느려 쌓이기만 할 때: 300ms(15프레임)를 넘으면 오래된 것부터 버린다 ──
+    {
+        Buffer b;
+        for (uint16_t s = 0; s < 15; ++s) put(b, s);
+        CHECK("15 frames = 300ms is still allowed", b.depth() == 15 && b.stats().overflow_drop == 0);
+        put(b, 15);
+        CHECK("16th frame: cut back to target", b.depth() == b.target_frames() && b.stats().overflow_drop == (uint32_t)(16 - b.target_frames()));
+        CHECK("oldest were dropped, newest kept", is(b.pop(), (uint16_t)(16 - 3)));
+    }
+
+    // ── 목표 깊이 조절: 끊김마다 +1(최대 200ms), 오래 무사하면 -1(최소 40ms) ──
+    {
+        Buffer b;
+        uint16_t s = 0;
+        for (int round = 0; round < 12; ++round) {              // 끊김 12번
+            while (b.depth() < b.target_frames()) put(b, s++);
+            run_dry(b);            // 다 꺼냄 → 은닉 3번 → 무음(끊김 1회)
+        }
+        CHECK("target capped at 200ms", b.target_frames() == MAX_FRAMES && b.stats().underruns == 12);
+    }
+    {
+        Buffer b;
+        uint16_t s = 0;
+        for (int i = 0; i < 3; ++i) put(b, s++);
+        int silence = 0;
+        for (int i = 0; i < CALM_POPS * 3; ++i) { put(b, s++); if (b.pop().kind != Kind::PLAY) ++silence; }   // 한 프레임 넣고 한 프레임 꺼내기를 오래 반복
+        CHECK("steady stream: never silent or concealed", silence == 0 && b.stats().lost == 0 && b.stats().underruns == 0);
+        CHECK("steady stream: target shrinks to 40ms", b.target_frames() == MIN_FRAMES);
+        CHECK("steady stream: depth follows the target down", b.depth() <= MIN_FRAMES + 1);
+    }
+
+    // ── 틀고 있는 중에 프레임이 몰려와 깊어지면(300ms 이하), 1초 지켜본 뒤 목표 깊이로 줄인다 ──
+    {
+        Buffer b;
+        uint16_t s = 0;
+        for (int i = 0; i < 3; ++i) put(b, s++);
+        b.pop();                                                // 재생 시작
+        for (int i = 0; i < 8; ++i) put(b, s++);                // 몰려옴: 깊이 10 (200ms)
+        CHECK("burst while playing: kept for now", b.depth() == 10 && b.stats().catchup_drop == 0);
+        int not_play = 0;
+        for (int i = 0; i < CATCHUP_POPS - 1; ++i) { put(b, s++); if (b.pop().kind != Kind::PLAY) ++not_play; }
+        CHECK("burst while playing: not trimmed before 1s", b.stats().catchup_drop == 0 && b.depth() == 10);
+        for (int i = 0; i < 5; ++i) { put(b, s++); if (b.pop().kind != Kind::PLAY) ++not_play; }
+        CHECK("burst while playing: trimmed back near the target after 1s", b.depth() <= b.target_frames() + 1 && b.stats().catchup_drop >= 6);
+        CHECK("burst while playing: playback never paused", not_play == 0);
+    }
+
+    // ── 순번이 65535 → 0 으로 넘어가도 이어진다 ─────────────────────────────
+    {
+        Buffer b;
+        put(b, 65534); put(b, 65535); put(b, 0); put(b, 1);
+        CHECK("wrap: 65534, 65535, 0, 1", is(b.pop(), 65534) && is(b.pop(), 65535) && is(b.pop(), 0) && is(b.pop(), 1));
+        CHECK("wrap: nothing lost", b.stats().lost == 0 && b.stats().late == 0);
+    }
+
+    // ── 순번이 크게 튄다(상대가 재부팅·새 통화): 버퍼를 비우고 새 순번에서 다시 시작 ──
+    {
+        Buffer b;
+        put(b, 500); put(b, 501); put(b, 502);
+        b.pop();
+        put(b, 9000);
+        CHECK("jump: resync counted, old frames discarded", b.stats().resync == 1 && b.depth() == 1);
+        put(b, 9001); put(b, 9002);
+        Pop p = b.pop();
+        CHECK("jump: plays from the new sequence", p.kind == Kind::PLAY && p.data[0] == (uint8_t)9000);
+    }
+    {   // 뒤로 크게 튀는 것도 같은 처리(늦은 프레임으로 영원히 버리지 않는다)
+        Buffer b;
+        put(b, 9000); put(b, 9001); put(b, 9002);
+        b.pop();
+        for (uint16_t s = 10; s < 14; ++s) put(b, s);
+        CHECK("backward jump: resync instead of dropping forever", b.stats().resync == 1 && b.depth() == 4);
+    }
+
+    // ── 규격 밖 프레임 ──────────────────────────────────────────────────────
+    {
+        Buffer b;
+        uint8_t d[MAX_PAYLOAD + 8] = {0};
+        b.push(1, d, 0);
+        b.push(2, d, MAX_PAYLOAD + 1);
+        b.push(3, nullptr, 10);
+        CHECK("empty / oversized / null payload rejected", b.depth() == 0 && b.stats().bad == 3);
+        b.push(4, d, MAX_PAYLOAD);
+        CHECK("largest payload (398) accepted", b.depth() == 1 && MAX_PAYLOAD == 398);
+    }
+
+    // ── 새 통화: reset 하면 처음 상태 ───────────────────────────────────────
+    {
+        Buffer b;
+        put(b, 1); put(b, 2); put(b, 3); b.pop();
+        b.reset();
+        CHECK("reset: empty, silence, stats cleared, target back to start", b.depth() == 0 && b.pop().kind == Kind::SILENCE && b.stats().played == 0 && b.target_frames() == START_FRAMES);
+        put(b, 700); put(b, 701); put(b, 702);
+        CHECK("reset: accepts any new starting sequence", is(b.pop(), 700));
+    }
+
+    // ════ 꺼낸 프레임의 순번, 채우는 중인지 ═══════════════════════════════════
+    {
+        Buffer b;
+        CHECK("buffering at first", b.buffering());
+        put(b, 40); put(b, 41); put(b, 42);
+        Pop p = b.pop();
+        CHECK("a played frame carries its sequence number", p.kind == Kind::PLAY && p.seq == 40 && !b.buffering());
+        CHECK("next ones too", b.pop().seq == 41 && b.pop().seq == 42);
+        b.pop(); b.pop(); b.pop();
+        CHECK("still not buffering while concealing", !b.buffering());
+        b.pop();
+        CHECK("buffering again once it gave up concealing", b.buffering());
+        for (uint16_t s = 46; s < 46 + 16; ++s) put(b, s);          // 16개: 넘쳐서 오래된 것을 버린다
+        p = b.pop();
+        CHECK("after dropping old frames the sequence jumps (the caller can see the gap)", p.kind == Kind::PLAY && p.seq == (uint16_t)(46 + 16 - b.target_frames()));
+    }
+
+    // ════ 오래·거칠게 (Review Focus 3) ════════════════════════════════════════
+    // 기기의 박자: 보내는 쪽은 16ms 묶음을 모아 20ms 프레임이 찰 때마다 한 개(간격 16·16·16·32ms), 받는 쪽은 16ms 마다 한 묶음을 가져가고
+    // 링버퍼가 한 묶음보다 적어지면 pop 한다(역시 16·16·16·32ms). 아래 Beat 가 그 박자를 센다.
+    struct Beat { int acc = 0; int tick() { acc += 256; int n = 0; while (acc >= 320) { acc -= 320; ++n; } return n; } };
+    struct Pull { int ring = 0; int tick() { ring -= 256; int n = 0; while (ring < 256) { ring += 320; ++n; } return n; } };
+
+    {   // 망이 고르면(같은 박자, 어긋난 위상 5가지) 한 시간 동안 한 번도 끊기지 않는다
+        bool clean = true; int worst_depth = 0;
+        for (int phase = 0; phase < 5; ++phase) {
+            Buffer b; Beat tx; Pull rx; uint16_t s = 0; long plays = 0;
+            for (int i = 0; i < phase; ++i) tx.tick();
+            for (int t = 0; t < 225000; ++t) {                      // 16ms × 225000 = 1시간
+                for (int n = tx.tick(); n > 0; --n) put(b, s++);
+                for (int n = rx.tick(); n > 0; --n) { const Pop p = b.pop(); if (p.kind == Kind::PLAY) ++plays; }
+                if (b.depth() > worst_depth) worst_depth = b.depth();
+            }
+            if (b.stats().lost || b.stats().underruns || b.stats().overflow_drop || b.stats().catchup_drop || plays < 179990) clean = false;
+        }
+        CHECK("steady 16/16/16/32 ms beat on both sides, 1 hour x 5 phases: never concealed, never dropped", clean);
+        CHECK("... and it never holds more than 4 frames (80 ms)", worst_depth <= 4);
+    }
+    {   // 순번이 65535 를 여러 번 넘어가는 긴 통화(2시간)에서 내용이 그대로 나온다
+        Buffer b; Beat tx; Pull rx; uint16_t s = 65000; uint32_t sent = 0, got = 0; bool same = true;
+        for (int t = 0; t < 450000; ++t) {
+            for (int n = tx.tick(); n > 0; --n) { uint8_t d[60]; memset(d, (uint8_t)(sent * 7), sizeof d); b.push(s++, d, sizeof d); ++sent; }
+            for (int n = rx.tick(); n > 0; --n) { const Pop p = b.pop(); if (p.kind == Kind::PLAY) { if (p.data[0] != (uint8_t)(got * 7) || p.len != 60) same = false; ++got; } }
+        }
+        CHECK("2-hour call (sequence wraps 5 times): every frame comes out once, in order, unchanged", same && sent == 360000 && got >= sent - 4 && b.stats().lost == 0);
+    }
+    {   // 보내는 쪽 시계가 0.1% 빠르다(실제 수정 발진기 오차의 10배 이상): 쌓이지 않고 가끔 한 프레임씩 버려 따라간다
+        Buffer b; Beat tx; Pull rx; uint16_t s = 0; int worst = 0;
+        for (int t = 0; t < 450000; ++t) {
+            for (int n = tx.tick() + (t % 1250 == 0 ? 1 : 0); n > 0; --n) put(b, s++);     // 20초(1000프레임)마다 한 프레임 더
+            for (int n = rx.tick(); n > 0; --n) b.pop();
+            if (b.depth() > worst) worst = b.depth();
+        }
+        CHECK("sender clock 0.1% fast for 2 hours: depth stays bounded (delay does not pile up)", worst <= 6 && b.depth() <= 4);
+        CHECK("... by dropping a frame now and then, never by going silent", b.stats().catchup_drop >= 100 && b.stats().underruns == 0);
+    }
+    {   // 받는 쪽 시계가 0.1% 빠르다: 20초마다 한 번 다 떨어진다 — 한 프레임만 메우고 이어 간다(JB-D2. 초안은 여기서 끝없이 끊겼다)
+        Buffer b; Beat tx; Pull rx; uint16_t s = 0; int worst_target = 0; long plays = 0, silent = 0;
+        for (int t = 0; t < 450000; ++t) {
+            for (int n = tx.tick(); n > 0; --n) put(b, s++);
+            for (int n = rx.tick() + (t % 1250 == 0 ? 1 : 0); n > 0; --n) {                       // 20초(1000프레임)마다 한 번 더 꺼낸다
+                const Pop p = b.pop();
+                if (p.kind == Kind::PLAY) ++plays; else if (p.kind == Kind::SILENCE && t > 10) ++silent;
+            }
+            if (b.target_frames() > worst_target) worst_target = b.target_frames();
+        }
+        CHECK("receiver clock 0.1% fast for 2 hours: one concealed frame per slip, never a run of them", b.stats().lost >= 300 && b.stats().lost <= 400 && b.stats().underruns == b.stats().lost);
+        CHECK("... no frame is thrown away for it, playback never goes silent, the target does not creep up",
+              b.stats().late == 0 && silent == 0 && plays >= 359990 && worst_target <= 4 && b.target_frames() <= 3);
+    }
+    {   // 한 프레임이 통째로 사라진다(20초마다 — UDP 계열): 뒤 프레임이 와 있으므로 그 순번만 메우고 넘어간다
+        Buffer b; Beat tx; Pull rx; uint32_t sent = 0; long plays = 0;
+        for (int t = 0; t < 450000; ++t) {
+            for (int n = tx.tick(); n > 0; --n) { if (sent % 1000 != 999) put(b, (uint16_t)sent); ++sent; }
+            for (int n = rx.tick(); n > 0; --n) if (b.pop().kind == Kind::PLAY) ++plays;
+        }
+        CHECK("a frame missing every 20 s for 2 hours: each gap is concealed once and skipped",
+              b.stats().lost == 359 && b.stats().underruns == 0 && b.stats().late == 0 && plays >= 359630);   // 360번째 빈자리는 맨 끝 프레임이라 아직 차례가 안 왔다
+    }
+    {   // 망이 멈췄다 풀린다(TCP: 잃지 않고 한꺼번에 온다). 멈춘 길이별로 — 풀린 뒤 1.5초 안에 깊이가 목표 근처로 돌아온다
+        bool recovered = true, bounded = true; int worst = 0;
+        for (int stall_ms : {40, 100, 200, 300, 500, 1000, 3000, 10000}) {
+            Buffer b; Beat tx; Pull rx; uint16_t s = 0; std::vector<uint16_t> held;
+            for (int t = 0; t < 2000; ++t) {                        // 32초
+                const bool stalled = t >= 500 && t < 500 + stall_ms / 16;
+                for (int n = tx.tick(); n > 0; --n) held.push_back(s++);
+                if (!stalled) { for (uint16_t q : held) put(b, q); held.clear(); }
+                for (int n = rx.tick(); n > 0; --n) b.pop();
+                if (b.depth() > worst) worst = b.depth();
+                if (t == 500 + stall_ms / 16 + 94 && b.depth() > b.target_frames() + 1) recovered = false;   // 풀리고 1.5초 뒤
+            }
+            if (b.depth() > b.target_frames() + 1) recovered = false;
+            if (worst > OVERFLOW_FRAMES) bounded = false;
+        }
+        CHECK("stall then burst (40 ms .. 10 s): back near the target within 1.5 s — the delay never piles up", recovered);
+        CHECK("... and it never holds more than 300 ms", bounded && worst <= OVERFLOW_FRAMES);
+    }
+    {   // 손실·중복·순서 뒤바뀜이 섞인 망(UDP 계열로 바꿔도 — JB-D1): 꺼낸 것은 늘 순번이 커지는 쪽이고, 넣은 내용 그대로다
+        uint32_t seed = 20261004; auto rnd = [&seed](uint32_t m) { seed = seed * 1664525u + 1013904223u; return (seed >> 8) % m; };
+        bool forward = true, intact = true, bounded = true; long plays = 0, losts = 0;
+        for (int loss_pct : {0, 1, 5, 20, 50}) {
+            Buffer b; Beat tx; Pull rx; uint16_t s = 0; bool have = false; uint16_t last = 0; std::vector<uint16_t> late;
+            for (int t = 0; t < 60000; ++t) {                       // 16분
+                for (int n = tx.tick(); n > 0; --n) {
+                    const uint16_t q = s++;
+                    if ((int)rnd(100) < loss_pct) continue;         // 잃음
+                    if (rnd(20) == 0) { late.push_back(q); continue; }   // 늦게(순서 뒤바뀜)
+                    uint8_t d[60]; memset(d, (uint8_t)q, sizeof d); b.push(q, d, 1 + q % 60);
+                    if (rnd(30) == 0) b.push(q, d, 1 + q % 60);     // 중복
+                }
+                if (!late.empty() && rnd(3) == 0) { const uint16_t q = late.front(); late.erase(late.begin()); uint8_t d[60]; memset(d, (uint8_t)q, sizeof d); b.push(q, d, 1 + q % 60); }
+                for (int n = rx.tick(); n > 0; --n) {
+                    const Pop p = b.pop();
+                    if (p.kind == Kind::LOST) ++losts;
+                    if (p.kind != Kind::PLAY) continue;
+                    ++plays;
+                    if (have && (int16_t)(uint16_t)(p.seq - last) <= 0) forward = false;
+                    if (p.len != (size_t)(1 + p.seq % 60) || p.data[0] != (uint8_t)p.seq || p.data[p.len - 1] != (uint8_t)p.seq) intact = false;
+                    have = true; last = p.seq;
+                }
+                if (b.depth() > OVERFLOW_FRAMES || b.depth() < 0 || b.target_frames() < MIN_FRAMES || b.target_frames() > MAX_FRAMES) bounded = false;
+            }
+        }
+        CHECK("loss 0-50% + duplicates + reordering, 16 min each: played sequence numbers only move forward (nothing twice)", forward && plays > 100000 && losts > 1000);
+        CHECK("... every played frame is byte-for-byte what was pushed for that number", intact);
+        CHECK("... depth 0..15 and target 2..10 at every step", bounded);
+    }
+    {   // 아무렇게나: 무작위 순번·길이·reset 을 500만 번 — 죽지 않고, 안의 셈이 맞는다
+        uint32_t seed = 99; auto rnd = [&seed](uint32_t m) { seed = seed * 1664525u + 1013904223u; return (seed >> 8) % m; };
+        Buffer b; std::map<uint16_t, uint8_t> content; bool sane = true; uint16_t base = 0; uint8_t d[MAX_PAYLOAD + 4];
+        for (int i = 0; i < 5000000 && sane; ++i) {
+            const uint32_t op = rnd(100);
+            if (op < 55) {
+                const uint16_t q = (uint16_t)(rnd(50) == 0 ? rnd(65536) : base + rnd(40) - 4);
+                const size_t len = rnd(40) == 0 ? rnd(MAX_PAYLOAD + 4) : 1 + rnd(120);
+                const uint8_t fill = (uint8_t)rnd(256);
+                memset(d, fill, sizeof d);
+                const uint32_t before = b.stats().dup + b.stats().late + b.stats().bad;
+                b.push(q, d, len);
+                if (b.stats().dup + b.stats().late + b.stats().bad == before) content[q] = fill;   // 받아들여졌다
+                if (rnd(8) == 0) ++base;
+            } else if (op < 99) {
+                const Pop p = b.pop();
+                if (p.kind == Kind::PLAY) {
+                    auto it = content.find(p.seq);
+                    if (it == content.end() || p.len == 0 || p.len > MAX_PAYLOAD || p.data[0] != it->second || p.data[p.len - 1] != it->second) sane = false;
+                    base = (uint16_t)(p.seq + 1);
+                } else if (p.len != 0) sane = false;
+            } else if (rnd(50) == 0) { b.reset(); content.clear(); }
+            if (b.depth() < 0 || b.depth() > OVERFLOW_FRAMES || b.target_frames() < MIN_FRAMES || b.target_frames() > MAX_FRAMES) sane = false;
+        }
+        CHECK("5 million random pushes / pops / resets: never out of bounds, every played frame is one that was accepted", sane);
+    }
+
+    printf("\n%d ok, %d failed\n", ok, bad);
+    return bad ? 1 : 0;
+}
```

- [x] **Step 2: 구현 패치를 적용하고 통과를 확인한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft && git apply "$D/patches/T4-02-jitter.patch" && for f in main/jitter_buffer.h tools/test_jitter_buffer.cpp; do cmp "$D/files/firmware/tab5_phone/$f" "firmware/tab5_phone/$f" && echo "같음: $f"; done && cd firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_jitter_buffer.cpp -o /tmp/t_p3b_jitter_buffer 2> /tmp/t_p3b_jitter_buffer.err; echo "경고 $(wc -l < /tmp/t_p3b_jitter_buffer.err | tr -d ' ')줄"; /tmp/t_p3b_jitter_buffer | grep -c "^  FAIL"; /tmp/t_p3b_jitter_buffer | tail -1
```
Expected: `같음:` 2줄, `경고 0줄`, `0`, `65 ok, 0 failed` (1초쯤 걸린다 — 1~2시간 분량의 통화를 여러 번 돌린다: 두 기기의 시계가 0.1% 다를 때, 20초마다 한 프레임이 빠질 때, 40ms~10초 멈췄다 몰려올 때, 손실·중복·순서 뒤바뀜 0~50%, 무작위 500만 번).

```diff
--- /dev/null
+++ b/firmware/tab5_phone/main/jitter_buffer.h
@@ -0,0 +1,154 @@
+// jitter_buffer.h — 들쭉날쭉 도착하는 음성 프레임을 재생 박자에 맞춰 고르게 꺼내 준다. 하드웨어 무관(맥에서 단위 시험: tools/test_jitter_buffer.cpp).
+// 설계서 docs/exec-plans/2026-10-03-tab5-phone.md §9.6, 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md PL3b-D5:
+//   - 60ms(3프레임)로 시작해 40~200ms 사이에서 조절한다: 다 떨어져 끊길 때마다 +1프레임, 10초 무사하면 -1프레임.
+//   - 프레임이 비면 LOST 를 돌려준다 → 부르는 쪽이 메운다(voice_core.h 의 Concealer). 3번 연속 비면 무음으로 두고 다시 찰 때까지 기다린다.
+//     JB-D2: 다 떨어져서 메울 때는 순번을 넘기지 않는다 — 늦게 온 그 프레임을 그대로 튼다(재생이 한 프레임만큼 뒤로 밀려 여유가 다시 생긴다).
+//            순번을 넘기던 초안은 여유가 0 이 된 뒤 "메움 3번 → 그 3프레임은 늦었다고 버림 → 다시 여유 0"을 끝없이 되풀이했다
+//            (받는 쪽 시계가 조금 빠르기만 해도 생긴다 — 긴 시험 "receiver clock fast" 가 잡았다). 비용: 메운 만큼 지연이 늘어난다
+//            → 깊어지면 아래의 "1초 뒤 목표로 줄임"이 되돌린다. 뒤 프레임은 와 있는데 그 순번만 빠진 경우(진짜 손실)는 전처럼 넘긴다.
+//   - 300ms(15프레임)를 넘게 쌓이면 오래된 것부터 버린다. 다시 틀기 시작할 때와, 1초 넘게 목표보다 깊을 때도 목표 깊이로 줄인다
+//     → TCP 가 막혔다 풀려 프레임이 몰려와도 지연이 쌓이지 않는다.
+// 쓰는 법: 받는 쪽 태스크가 push(), 재생 쪽 태스크가 프레임 하나(20ms 분량)가 필요할 때마다 pop(). 두 태스크가 다르면 부르는 쪽이 잠금으로 감싼다
+//   (이 클래스는 잠그지 않는다 — voice_core.h 의 Receiver 가 감싼다). pop 은 정확히 20ms 마다가 아니어도 된다: 출력이 16ms 묶음이라
+//   16·16·16·32ms 간격으로 불린다(평균 20ms). 10초·1초 규칙은 pop 횟수(500·50)로 센다.
+// JB-D1: 전송이 TCP(WebSocket)라 순서 뒤바뀜·중복은 실제로는 오지 않는다. 그래도 순번으로 정렬·중복 제거를 하는 것은
+//        교환기를 UDP 계열(LiveKit, PH-D1 탈출구)로 바꿔도 이 파일을 그대로 쓰기 위해서다. 비용: 칸 32개 × 398바이트 = 약 13KB.
+#pragma once
+#include <cstddef>
+#include <cstdint>
+#include <cstring>
+
+namespace jitterbuf {
+
+const int    FRAME_MS        = 20;
+const int    START_FRAMES    = 3;      // 60ms
+const int    MIN_FRAMES      = 2;      // 40ms
+const int    MAX_FRAMES      = 10;     // 200ms
+const int    OVERFLOW_FRAMES = 15;     // 300ms — 이보다 깊어지면 오래된 것부터 버린다
+const int    MAX_CONCEAL     = 3;      // 다 떨어졌을 때 손실 은닉으로 버티는 횟수(60ms)
+const int    CALM_POPS       = 500;    // 10초 동안 끊김이 없으면 목표를 한 프레임 줄인다
+const int    CATCHUP_POPS    = 50;     // 1초 동안 목표보다 2프레임 넘게 깊으면 목표로 줄인다
+const int    SLOTS           = 32;     // 순번 창(2의 거듭제곱)
+const size_t MAX_PAYLOAD     = 398;    // 음성 프레임 400바이트 - 순번 2바이트(§9.4)
+
+enum class Kind : uint8_t { SILENCE, PLAY, LOST };
+
+struct Pop {
+    Kind   kind = Kind::SILENCE;
+    uint16_t seq = 0;             // PLAY 일 때: 그 프레임의 순번(이어지는 프레임인지 부르는 쪽이 본다 — 오래된 것을 버린 뒤에는 건너뛴다)
+    size_t len = 0;
+    uint8_t data[MAX_PAYLOAD];
+};
+
+struct Stats {
+    uint32_t played = 0;          // 꺼내 준 프레임
+    uint32_t lost = 0;            // 손실 은닉으로 메운 횟수
+    uint32_t late = 0;            // 이미 지나간 순번이라 버림
+    uint32_t dup = 0;             // 같은 순번이 또 와서 버림
+    uint32_t bad = 0;             // 규격 밖(빈 것·너무 큰 것)
+    uint32_t overflow_drop = 0;   // 300ms 초과·다시 틀기 시작할 때 버린 프레임
+    uint32_t catchup_drop = 0;    // 틀던 중 깊어져 줄이느라 버린 프레임
+    uint32_t underruns = 0;       // 다 떨어진 횟수
+    uint32_t resync = 0;          // 순번이 크게 튀어 다시 맞춘 횟수
+};
+
+class Buffer {
+public:
+    Buffer() { reset(); }
+
+    void reset()
+    {
+        for (int i = 0; i < SLOTS; ++i) slot_[i].used = false;
+        count_ = 0; started_ = false; buffering_ = true;
+        target_ = START_FRAMES; conceal_run_ = 0; calm_ = 0; deep_ = 0;
+        next_ = 0; stats_ = Stats();
+    }
+
+    int depth() const { return count_; }
+    bool buffering() const { return buffering_; }               // 채우는 중(무음) — 통화 시작 직후와 끊긴 뒤
+    int target_frames() const { return target_; }
+    const Stats &stats() const { return stats_; }
+
+    void push(uint16_t seq, const uint8_t *data, size_t len)
+    {
+        if (!data || len == 0 || len > MAX_PAYLOAD) { ++stats_.bad; return; }
+        if (!started_) { started_ = true; next_ = seq; }
+        int d = (int16_t)(uint16_t)(seq - next_);                // 다음에 꺼낼 순번에서 얼마나 떨어졌나(음수 = 이미 지나감)
+        if (d >= SLOTS || d <= -SLOTS) {                         // 크게 튐: 상대가 다시 시작했다 → 비우고 새 순번에서
+            clear_slots(); next_ = seq; d = 0;
+            buffering_ = true; conceal_run_ = 0; deep_ = 0; ++stats_.resync;
+        }
+        if (d < 0) { ++stats_.late; return; }
+        Slot &s = slot_[seq & (SLOTS - 1)];
+        if (s.used) { ++stats_.dup; return; }                    // 창 안에서 같은 칸 = 같은 순번
+        s.used = true; s.len = (uint16_t)len; memcpy(s.data, data, len);
+        ++count_;
+        if (count_ > OVERFLOW_FRAMES) stats_.overflow_drop += trim_to(target_);
+    }
+
+    Pop pop()
+    {
+        Pop out;
+        if (buffering_) {
+            if (count_ < target_) return out;                    // 아직 덜 찼다 → 무음
+            if (count_ - target_ >= 2) stats_.overflow_drop += trim_to(target_);   // 밀려 있던 만큼은 버리고 가장 새 프레임부터(한 프레임 여유는 둔다)
+            buffering_ = false; conceal_run_ = 0; calm_ = 0; deep_ = 0;
+        }
+
+        if (count_ - target_ >= 2) {                             // 틀던 중 깊어짐: 1초 지켜본 뒤 목표로
+            if (++deep_ >= CATCHUP_POPS) { stats_.catchup_drop += trim_to(target_); deep_ = 0; }
+        } else {
+            deep_ = 0;
+        }
+
+        Slot &s = slot_[next_ & (SLOTS - 1)];
+        if (s.used) {
+            out.kind = Kind::PLAY; out.seq = next_; out.len = s.len; memcpy(out.data, s.data, s.len);
+            s.used = false; --count_; ++next_;
+            conceal_run_ = 0; ++stats_.played;
+            if (++calm_ >= CALM_POPS) { calm_ = 0; if (target_ > MIN_FRAMES) --target_; }
+            return out;
+        }
+
+        if (count_ > 0) {                                        // 이 순번만 빠졌고 뒤는 와 있다 → 은닉하고 넘어간다
+            ++next_; ++stats_.lost; out.kind = Kind::LOST;
+            return out;
+        }
+
+        // 다 떨어졌다 — 기다리던 순번(next_)은 그대로 둔다(JB-D2)
+        if (conceal_run_ == 0) { ++stats_.underruns; calm_ = 0; if (target_ < MAX_FRAMES) ++target_; }
+        if (conceal_run_ < MAX_CONCEAL) {
+            ++conceal_run_; ++stats_.lost; out.kind = Kind::LOST;
+            return out;
+        }
+        buffering_ = true;                                       // 더 은닉하지 않고 다시 찰 때까지 무음
+        return out;
+    }
+
+private:
+    struct Slot { bool used; uint16_t len; uint8_t data[MAX_PAYLOAD]; };
+
+    void clear_slots() { for (int i = 0; i < SLOTS; ++i) slot_[i].used = false; count_ = 0; }
+
+    // 가장 새 프레임 keep 개만 남기고 오래된 것부터 버린다. 돌려주는 값 = 버린 프레임 수.
+    uint32_t trim_to(int keep)
+    {
+        uint32_t dropped = 0;
+        while (count_ > keep) {
+            Slot &s = slot_[next_ & (SLOTS - 1)];
+            if (s.used) { s.used = false; --count_; ++dropped; }
+            ++next_;
+        }
+        while (count_ > 0 && !slot_[next_ & (SLOTS - 1)].used) ++next_;   // 맨 앞의 빈 순번은 건너뛴다
+        return dropped;
+    }
+
+    Slot     slot_[SLOTS];
+    int      count_;
+    bool     started_, buffering_;
+    int      target_, conceal_run_, calm_, deep_;
+    uint16_t next_;
+    Stats    stats_;
+};
+
+}  // namespace jitterbuf
```

- [x] **Step 3: 커밋**

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/main/jitter_buffer.h firmware/tab5_phone/tools/test_jitter_buffer.cpp && git commit -m "feat(phone): 지터 버퍼 jitter_buffer.h — 60ms 로 시작·40~200ms·300ms 넘으면 버림, 다 떨어져 메울 때 순번을 넘기지 않는다(JB-D2) (맥 시험 65)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: 목소리 판단 헤더 `voice_core.h` (시험 먼저)

**Files:**
- Create: `firmware/tab5_phone/main/voice_core.h`
- Test: `firmware/tab5_phone/tools/test_voice_core.cpp`

**Interfaces:**
- Consumes: `call_fsm.h`(`A_VOICE_START` = 0x100, `A_VOICE_STOP` = 0x200), `relay_proto.h`(`pack_voice`·`unpack_voice`), `tones.h`(사인 표), Task 2 의 `pcm.h`, Task 4 의 `jitter_buffer.h`.
- Produces (`namespace voicecore` — Task 6 의 `voice.cpp`·`app.cpp`·`phone_main.cpp` 가 쓴다):
  - 상수: `RATE 16000`, `BLOCK 256`, `FRAME 320`, `FADE 80`, `IN_SLOTS 4`, `DISCARD_BLOCKS 8`, `TX_GAIN_Q8 1024`, `PLAY_GAIN_Q8 256`, `TX_QUEUE 10`, `TX_STALE_MS 200`, `MAX_TX_FRAME 200`, `RING_CAP 1024`, `TEST_AMP 2000`, `PING_AMP 4000`, `PING_DETECT 1500`, `SELFTEST_REF_MIN 300`
  - `bool from_acts(uint16_t acts, bool *on)` — 목소리 비트가 없으면 false. 끄기가 켜기보다 앞선다.
  - 보내는 길: `split_slots(slots, frames, mic, ref)`, `enum class Src { AEC, MIC, REF, TONE }`·`src_name`·`parse_src`, `pick(src, clean, mic, ref, tone, out)`, `class Uplink { start(); push(block); bool pop(frame, &seq); }`, `struct TxFrame`, `template <class Lock> class TxFifo { put(frame, len, now_ms); take(&out, now_ms); flush(); dropped(); clear_dropped(); }`
  - 받는 길: `class Concealer`, `struct RxStats`, `template <class Lock> class Receiver { start(); stop(); push(frame, len); want_pump(); pump(dec, fresh); bool fill(out, n); stats(); }`
  - 시험음·지연 재기: `class TestTone`, `class Burst`, `struct PingGen`, `struct SelfTestTone`, `int onset(pcm, n, threshold)`
- `Lock` 은 `lock()`·`unlock()` 이 있는 아무 형(기기: FreeRTOS 뮤텍스, 맥 시험: 빈 잠금).

- [x] **Step 1: 시험 패치만 적용하고 실패를 확인한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft && git apply "$D/patches/T5-01-voice-core-tests.patch" && wc -l < firmware/tab5_phone/tools/test_voice_core.cpp && cd firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_voice_core.cpp -o /tmp/t_p3b_voice_core 2>&1 | grep -m1 "error:"
```
Expected: `     686`, 이어서 `test_voice_core.cpp:7:10: fatal error: 'voice_core.h' file not found`

```diff
--- /dev/null
+++ b/firmware/tab5_phone/tools/test_voice_core.cpp
@@ -0,0 +1,686 @@
+// test_voice_core.cpp — voice_core.h 단위 시험:
+//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_voice_core.cpp -o /tmp/t_voice_core && /tmp/t_voice_core
+//   /tmp/t_voice_core --info   → 시험 뒤에 지연 시뮬레이션의 숫자(계획서 지연 예산 표의 "맥 시뮬레이션" 칸)를 찍는다
+// 목소리 통화의 판단 로직을 고정한다(계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md PL3b-D2·D5~D13, Review Focus 1~4):
+//   받기 전에는 켜지지 않는다 / 어떤 길로 끝나도 꺼진다 / 신호음과 겹치는 순간 / 보내는 프레임 / 밀리면 버린다 / 빠진 프레임 메우기 /
+//   받는 길(지터 버퍼 → 링버퍼 → 16ms 묶음) / 오래·거칠게(손실 없는 TCP 의 멈춤·몰림, 시계 어긋남) / 시험음·지연 재기.
+#include "voice_core.h"
+#include <algorithm>
+#include <cmath>
+#include <cstdio>
+#include <cstdlib>
+#include <string>
+#include <vector>
+using namespace voicecore;
+using namespace callfsm;
+static int ok = 0, bad = 0;
+#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)
+
+typedef std::vector<int16_t> Pcm;
+static const double PI = 3.14159265358979323846;
+struct NoLock { void lock() {} void unlock() {} };
+typedef Receiver<NoLock> Rx;
+static NoLock g_nolock;
+
+static int max_jump(const Pcm &v, int prev = 0) { int m = 0; for (int16_t s : v) { m = std::max(m, std::abs((int)s - prev)); prev = s; } return m; }
+static int peak(const Pcm &v, size_t from = 0, size_t to = SIZE_MAX) { int m = 0; for (size_t i = from; i < v.size() && i < to; ++i) m = std::max(m, std::abs((int)v[i])); return m; }
+static uint32_t g_seed = 1;
+static uint32_t rnd(uint32_t m) { g_seed = g_seed * 1664525u + 1013904223u; return (g_seed >> 8) % m; }
+
+// ── 가짜 코덱: 프레임 = [그 프레임 첫 표본의 번호 4바이트][채움 56바이트]. 해제하면 그 번호부터의 PCM 을 다시 만든다(표본 번호 g 의 함수).
+//    SINE = 317Hz 사인(이어 붙는 자리의 계단을 볼 때 — 20ms 프레임과 주기가 맞지 않아 프레임마다 모양이 다르다),
+//    RAMP = 표본 번호 그 자체(어느 표본이 언제 나왔는지 볼 때).
+enum class Sig { SINE, RAMP };
+static Sig g_sig = Sig::SINE;
+static const int SIG_AMP = 6000, SIG_HZ = 317, RAMP_MOD = 20000;
+static int16_t sig(int64_t g) { return g_sig == Sig::SINE ? (int16_t)std::lround(SIG_AMP * std::sin(2 * PI * SIG_HZ * (double)g / RATE)) : (int16_t)(g % RAMP_MOD); }
+static const int SIG_STEP = (int)std::ceil(SIG_AMP * 2 * PI * SIG_HZ / RATE) + 2;       // 사인의 이웃 표본 차이 상한
+static const int JOIN_STEP = SIG_STEP + 2 * SIG_AMP / FADE + 4;                          // 5ms 에 걸쳐 넘겨 받는 자리의 상한(두 소리의 차이 ≤ 2 × 진폭을 80 걸음에)
+static size_t frame_bytes(uint32_t first, uint16_t seq, uint8_t *out)
+{
+    uint8_t opus[60] = {};
+    opus[0] = (uint8_t)first; opus[1] = (uint8_t)(first >> 8); opus[2] = (uint8_t)(first >> 16); opus[3] = (uint8_t)(first >> 24);
+    return relayproto::pack_voice(out, 64, seq, opus, sizeof opus);
+}
+static int g_dec_calls = 0, g_fresh_calls = 0; static bool g_dec_fail = false;
+static bool dec(const uint8_t *d, size_t len, int16_t *pcm)
+{
+    ++g_dec_calls;
+    if (g_dec_fail || len != 60) return false;
+    const uint32_t first = d[0] | d[1] << 8 | d[2] << 16 | (uint32_t)d[3] << 24;
+    for (int i = 0; i < FRAME; ++i) pcm[i] = sig((int64_t)first + i);
+    return true;
+}
+static void fresh() { ++g_fresh_calls; }
+// idx 번째 프레임(표본 idx × 320 부터)이 도착한다. 기기에서는 도착할 때마다 해제 태스크를 깨운다(링버퍼가 한 묶음보다 적을 때)
+static void feed(Rx &rx, uint32_t idx) { uint8_t f[64]; rx.push(f, frame_bytes(idx * FRAME, (uint16_t)idx, f)); if (rx.want_pump()) rx.pump(dec, fresh); }
+// 출력 태스크 + 해제 태스크의 한 바퀴: 한 묶음 가져가고, 모자라면 채운다
+static bool pull(Rx &rx, int16_t *out) { const bool s = rx.fill(out, BLOCK); if (rx.want_pump()) rx.pump(dec, fresh); return s; }
+static Pcm pull_n(Rx &rx, int blocks) { Pcm all; for (int i = 0; i < blocks; ++i) { int16_t b[BLOCK]; pull(rx, b); all.insert(all.end(), b, b + BLOCK); } return all; }
+// 기기의 박자로 돌린다: 16ms 묶음 blocks 개를 가져가며, 다섯 묶음에 네 프레임 꼴(평균 20ms 마다)로 다음 프레임을 넣는다
+struct Flow { uint32_t next = 0; int b = 0; };
+static Pcm flow(Rx &rx, Flow &fl, int blocks)
+{
+    Pcm all;
+    for (int i = 0; i < blocks; ++i, ++fl.b) { if (fl.b % 5 != 4) feed(rx, fl.next++); int16_t x[BLOCK]; pull(rx, x); all.insert(all.end(), x, x + BLOCK); }
+    return all;
+}
+// v 안에서 "표본 번호 first 부터의 신호"가 시작하는 자리를 찾는다(앞 5ms 는 크기가 바뀌므로 그 뒤 400표본으로 맞춘다). 없으면 -1.
+static long align(const Pcm &v, int64_t first)
+{
+    for (size_t a = 0; a + FADE + 400 <= v.size(); ++a) {
+        bool same = true;
+        for (int i = FADE; i < FADE + 400 && same; ++i) if (v[a + (size_t)i] != sig(first + i)) same = false;
+        if (same) return (long)a;
+    }
+    return -1;
+}
+// a 에서 시작한 신호가 v 의 끝까지 표본 하나 빠짐없이 이어지는가
+static bool exact_from(const Pcm &v, long a, int64_t first) { if (a < 0) return false; for (size_t i = (size_t)a + FADE; i < v.size(); ++i) if (v[i] != sig(first + (int64_t)(i - (size_t)a))) return false; return true; }
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
+static const State STATES[] = {State::OFFLINE, State::IDLE, State::DIALING, State::RINGBACK, State::INCOMING, State::ANSWERING, State::IN_CALL, State::ENDING};
+static const Ev EVENTS[] = {Ev::LINK_UP, Ev::LINK_DOWN, Ev::PEER, Ev::RINGING, Ev::RING, Ev::CONNECTED, Ev::ENDED, Ev::TAP_CALL, Ev::TAP_CANCEL,
+                            Ev::TAP_ACCEPT, Ev::TAP_REJECT, Ev::TAP_HANGUP, Ev::TAP_MUTE, Ev::TICK};
+static const Why WHYS[] = {Why::NONE, Why::HANGUP, Why::REJECT, Why::TIMEOUT, Why::OFFLINE, Why::LOST};
+
+// ── 신호음 + 목소리를 기기처럼 잇는다: app.cpp 의 apply(할 일 → 소리·목소리) + audio_out 의 출력 한 묶음(pcm::mix_sources) ──
+static tones::Player g_tone; static bool g_tone_has = false; static tones::Tone g_tone_cmd = tones::Tone::NONE;
+static Rx *g_rx = nullptr; static bool g_voice_on = false;       // g_voice_on = app.cpp 가 기억하는 켜짐(VOICE 줄을 한 번만 찍으려고)
+static bool tone_src(int16_t *out, int n) { const tones::Block b = tones::next_block(g_tone, g_tone_has, g_tone_cmd, out, n); g_tone_has = false; return b.sound; }
+static bool voice_src(int16_t *out, int n) { const bool s = g_rx->fill(out, n); if (g_rx->want_pump()) g_rx->pump(dec, fresh); return s; }
+static void apply_acts(uint16_t acts)
+{
+    tones::Tone t;
+    if (tones::from_acts(acts, &t)) { g_tone_cmd = t; g_tone_has = true; }      // 소리 먼저(PL3a-D13)
+    bool on;
+    if (from_acts(acts, &on) && on != g_voice_on) { if (on) g_rx->start(); else g_rx->stop(); g_voice_on = on; }
+}
+static int mix_block(int16_t *out) { static const pcm::source_fn SRC[2] = {tone_src, voice_src}; int16_t tmp[BLOCK]; return pcm::mix_sources(SRC, 2, out, tmp, BLOCK); }
+
+// ── 통화 한쪽 방향의 시뮬레이션(시각은 µs): 보내는 쪽 16ms 묶음 → Uplink → 가짜 코덱 → 망(TCP: 잃지 않고 순서대로, 늦을 수는 있다)
+//    → Receiver → 받는 쪽이 16ms 마다 한 묶음. RAMP 신호라 나온 표본의 값이 곧 "언제 마이크에 들어온 표본인가"다.
+struct Sim {
+    double tx_ppm = 0;                  // 보내는 쪽 시계가 얼마나 빠른가(백만분율)
+    int64_t net_base = 3000, net_jitter = 4000;         // 망 지연 = base + [0, jitter) µs
+    int64_t stall_at = -1, stall_len = 0;               // 이 시각부터 이만큼 망이 멈춘다(그동안 보낸 것은 풀릴 때 한꺼번에 도착)
+    int64_t rx_phase = 5000;            // 받는 쪽 16ms 박자가 보내는 쪽보다 얼마나 늦게 시작하나
+    // 결과
+    std::vector<int> delay_ms;          // 묶음마다: 그 묶음의 첫 표본이 마이크에 들어온 때부터 출력 태스크가 가져간 때까지(이어진 묶음만)
+    long blocks = 0, broken = 0;        // 가져간 묶음 / 그 가운데 표본이 이어지지 않은 묶음(메웠거나 건너뛴 자리)
+    RxStats st;
+    int worst_after = 0;                // settle 뒤의 최대 지연
+    bool framed_ok = false;             // 보내는 쪽 프레임이 표본·순번 모두 이어졌나
+    void run(int seconds, int64_t settle_us = 0)
+    {
+        g_sig = Sig::RAMP;
+        static Rx rx(g_nolock);
+        rx.start();
+        Uplink up; up.start();
+        struct Pkt { int64_t at; uint8_t f[64]; size_t n; };
+        std::vector<Pkt> wire; size_t head = 0;
+        const double tx_period = 16000.0 / (1.0 + tx_ppm / 1e6);
+        double tx_next = tx_period; int64_t rx_next = rx_phase + 16000, last_arrive = 0; int64_t g = 0; uint32_t sent = 0; bool coded = true;
+        const int64_t end = (int64_t)seconds * 1000000;
+        while (rx_next < end) {
+            if (tx_next <= (double)rx_next) {                                   // 보내는 쪽: 한 묶음이 찼다
+                const int64_t now = (int64_t)tx_next;
+                int16_t blk[BLOCK]; for (int i = 0; i < BLOCK; ++i) blk[i] = sig(g + i);
+                up.push(blk); g += BLOCK;
+                int16_t fr[FRAME]; uint16_t seq;
+                while (up.pop(fr, &seq)) {
+                    const uint32_t first = (uint32_t)(DISCARD_BLOCKS * BLOCK) + sent * FRAME;           // 이 프레임의 첫 표본 번호
+                    if (fr[0] != sig(first) || fr[FRAME - 1] != sig((int64_t)first + FRAME - 1) || seq != (uint16_t)sent) coded = false;
+                    Pkt p; p.n = frame_bytes(first, seq, p.f); ++sent;
+                    int64_t at = now + net_base + (net_jitter ? (int64_t)rnd((uint32_t)net_jitter) : 0);
+                    if (stall_at >= 0 && at >= stall_at && at < stall_at + stall_len) at = stall_at + stall_len;
+                    if (at < last_arrive) at = last_arrive;                     // TCP: 순서대로
+                    last_arrive = p.at = at; wire.push_back(p);
+                }
+                tx_next += tx_period;
+                continue;
+            }
+            const int64_t now = rx_next;                                        // 받는 쪽: 한 묶음을 가져간다
+            while (head < wire.size() && wire[head].at <= now) { rx.push(wire[head].f, wire[head].n); ++head; if (rx.want_pump()) rx.pump(dec, fresh); }
+            int16_t out[BLOCK]; pull(rx, out);
+            ++blocks;
+            bool run = true; for (int i = 1; i < BLOCK; ++i) if ((out[i] - out[i - 1] + RAMP_MOD) % RAMP_MOD != 1) { run = false; break; }
+            if (!run) { if (peak(Pcm(out, out + BLOCK))) ++broken; }
+            else {
+                // out[0] = (보낸 쪽 표본 번호) mod 20000. 그 표본이 마이크에 들어온 시각 = 번호 × 62.5µs × (보내는 쪽 시계 보정)
+                const double sample_us = 1e6 / RATE / (1.0 + tx_ppm / 1e6);
+                const int64_t newest = (int64_t)((double)now / sample_us);       // 지금 마이크에 들어오고 있는 표본 번호
+                int64_t k = newest - ((newest - out[0]) % RAMP_MOD + RAMP_MOD) % RAMP_MOD;   // out[0] 과 값이 같은, 지금보다 이른 가장 가까운 번호
+                const int d = (int)(((double)now - (double)k * sample_us) / 1000.0);
+                delay_ms.push_back(d);
+                if (now >= settle_us && d > worst_after) worst_after = d;
+            }
+            rx_next += 16000;
+        }
+        st = rx.stats();
+        framed_ok = coded;
+        g_sig = Sig::SINE;
+    }
+    int pct(int p) const { if (delay_ms.empty()) return -1; std::vector<int> v = delay_ms; std::sort(v.begin(), v.end()); return v[(v.size() - 1) * (size_t)p / 100]; }
+};
+
+int main(int argc, char **argv)
+{
+    const bool info = argc > 1 && std::string(argv[1]) == "--info";
+
+    // ════ 켜고 끄기 (PH-D6, Review Focus 1·2) ════
+    {
+        bool on = true;
+        CHECK("acts: voice start turns it on", from_acts(A_VOICE_START, &on) && on);
+        CHECK("acts: voice stop turns it off", from_acts(A_VOICE_STOP, &on) && !on);
+        on = true;
+        CHECK("acts: stop wins when both come together (when in doubt the mic stays closed)", from_acts(A_VOICE_START | A_VOICE_STOP, &on) && !on);
+        on = true;
+        CHECK("acts: nothing about voice -> leave it as it is", !from_acts(A_SEND_CALL | A_TONE_BELL | A_TONE_STOP | A_TONE_END | A_RECONNECT, &on) && on);
+        on = false;
+        CHECK("acts: the tone and send bits beside it do not matter", from_acts(A_TONE_STOP | A_VOICE_START, &on) && on && from_acts(A_SEND_HANGUP | A_VOICE_STOP | A_TONE_END, &on) && !on);
+    }
+    {
+        // 모든 상태 × 사건 × 사유 × flag: 사건 하나 뒤에 "목소리가 켜져 있다 ⇔ 통화 중(IN_CALL)" 이 늘 맞다
+        bool iff = true, never_both = true, start_only_into_call = true; int opens = 0, closes = 0;
+        for (State s : STATES) for (Ev e : EVENTS) for (Why w : WHYS) for (int flag = 0; flag < 2; ++flag) for (int64_t now : {(int64_t)2000, (int64_t)100000}) {
+            Fsm f = in_state(s);
+            bool voice = s == State::IN_CALL;
+            const Out o = f.on(e, now, w, flag != 0, 0);
+            if ((o.acts & A_VOICE_START) && (o.acts & A_VOICE_STOP)) never_both = false;
+            if ((o.acts & A_VOICE_START) && f.state != State::IN_CALL) start_only_into_call = false;
+            const bool was = voice;
+            from_acts(o.acts, &voice);
+            if (voice != (f.state == State::IN_CALL)) iff = false;
+            if (!was && voice) ++opens;
+            if (was && !voice) ++closes;
+        }
+        CHECK("state machine: after any single event, voice is on exactly when the state is IN_CALL (every state x event x reason)", iff && opens > 0 && closes > 0);
+        CHECK("state machine: start and stop never come in the same batch", never_both);
+        CHECK("state machine: start only comes together with entering IN_CALL", start_only_into_call);
+    }
+    {
+        // 받기 전에는 마이크가 열리지 않는다: 대기·거는 중·벨·받는 중 어디서도, connected 가 아닌 사건으로는 켜지지 않는다
+        bool closed = true;
+        for (State s : {State::OFFLINE, State::IDLE, State::INCOMING, State::ENDING}) for (Ev e : EVENTS) for (Why w : WHYS) {
+            Fsm f = in_state(s); bool voice = false;
+            from_acts(f.on(e, 2000, w, true, 0).acts, &voice);
+            if (voice) closed = false;
+        }
+        for (State s : {State::DIALING, State::RINGBACK, State::ANSWERING}) for (Ev e : EVENTS) for (Why w : WHYS) {
+            Fsm f = in_state(s); bool voice = false;
+            from_acts(f.on(e, 2000, w, true, 0).acts, &voice);
+            if (voice != (e == Ev::CONNECTED)) closed = false;
+        }
+        CHECK("never open before the call is accepted: IDLE / INCOMING (ringing, not answered) / ENDING / OFFLINE never turn it on, whatever arrives", closed);
+        Fsm f = in_state(State::INCOMING); bool voice = false;
+        const Out o = f.on(Ev::CONNECTED, 2000);
+        from_acts(o.acts, &voice);
+        CHECK("a 'connected' that arrives while still ringing (not answered) does not open the mic", !voice && f.state == State::OFFLINE && !(o.acts & A_VOICE_START));
+    }
+    {
+        // 통화가 끝나는 모든 길: 끊기, 상대 끊김(사유 다섯), 교환기 끊김, 끊김을 못 본 채 다시 붙음
+        bool all_stop = true; int ways = 0;
+        struct Way { Ev e; Why w; };
+        const Way ways_out[] = {{Ev::TAP_HANGUP, Why::NONE}, {Ev::ENDED, Why::HANGUP}, {Ev::ENDED, Why::REJECT}, {Ev::ENDED, Why::TIMEOUT}, {Ev::ENDED, Why::OFFLINE},
+                                {Ev::ENDED, Why::LOST}, {Ev::ENDED, Why::NONE}, {Ev::LINK_DOWN, Why::NONE}, {Ev::LINK_UP, Why::NONE}};
+        for (const Way &w : ways_out) {
+            Fsm f = in_state(State::IN_CALL); bool voice = true;
+            const Out o = f.on(w.e, 5000, w.w);
+            from_acts(o.acts, &voice);
+            if (voice || f.state == State::IN_CALL || !(o.acts & A_VOICE_STOP)) all_stop = false;
+            ++ways;
+        }
+        CHECK("the call ends every way (hang up, peer hangs up x6 reasons, relay link drops, link comes up again unseen): voice is off", all_stop && ways == 9);
+        bool stays = true;
+        for (Ev e : {Ev::PEER, Ev::RINGING, Ev::RING, Ev::CONNECTED, Ev::TAP_CALL, Ev::TAP_CANCEL, Ev::TAP_ACCEPT, Ev::TAP_REJECT, Ev::TAP_MUTE, Ev::TICK}) {
+            Fsm f = in_state(State::IN_CALL); bool voice = true;
+            from_acts(f.on(e, 99999999, Why::NONE, false).acts, &voice);
+            if (!voice || f.state != State::IN_CALL) stays = false;
+        }
+        CHECK("nothing else ends a call (stray signals, other buttons, the peer going offline flag, hours passing)", stays);
+    }
+    {
+        // 무작위로 오래: 사건 300만 개(시간도 흐른다 — 시간 제한·재연결 포함). 사건마다 "켜짐 ⇔ IN_CALL".
+        g_seed = 20261004; Fsm f; bool voice = false, iff = true; int64_t now = 1000; long calls = 0;
+        for (int i = 0; i < 3000000 && iff; ++i) {
+            now += rnd(8) == 0 ? rnd(9000) : rnd(40);
+            const State before = f.state;
+            const Out o = f.on(EVENTS[rnd(14)], now, WHYS[rnd(6)], rnd(4) != 0, (uint8_t)rnd(5));
+            from_acts(o.acts, &voice);
+            if (voice != (f.state == State::IN_CALL)) iff = false;
+            if (before != State::IN_CALL && f.state == State::IN_CALL) ++calls;
+        }
+        CHECK("3 million random events with time passing: voice on <=> IN_CALL after every one", iff && calls > 1000);
+    }
+    {
+        // 한 묶음의 할 일에서 소리와 목소리를 함께 읽는다(app.cpp apply 의 순서)
+        tones::Tone t = tones::Tone::BELL; bool on = false;
+        const Out open = in_state(State::RINGBACK).on(Ev::CONNECTED, 2000);
+        CHECK("call opens on the caller: one batch (0x180) says 'stop the ringback' and 'voice on'",
+              open.acts == (A_TONE_STOP | A_VOICE_START) && tones::from_acts(open.acts, &t) && t == tones::Tone::NONE && from_acts(open.acts, &on) && on);
+        Fsm a = in_state(State::INCOMING);
+        const Out acc = a.on(Ev::TAP_ACCEPT, 2000); on = false;
+        CHECK("on the callee the bell stops one step earlier (0x82), voice only at 'connected'",
+              (acc.acts & A_TONE_STOP) && !from_acts(acc.acts, &on) && from_acts(a.on(Ev::CONNECTED, 2100).acts, &on) && on);
+        const Out end = in_state(State::IN_CALL).on(Ev::ENDED, 5000, Why::HANGUP); on = true;
+        CHECK("call ends: one batch (0x240) says 'voice off' and 'end tone'",
+              end.acts == (A_VOICE_STOP | A_TONE_END) && tones::from_acts(end.acts, &t) && t == tones::Tone::END && from_acts(end.acts, &on) && !on);
+        const Out hang = in_state(State::IN_CALL).on(Ev::TAP_HANGUP, 5000); on = true;
+        CHECK("hanging up: voice goes off in the same batch as the hangup signal (0x248), not after the relay answers",
+              hang.acts == (A_SEND_HANGUP | A_VOICE_STOP | A_TONE_END) && from_acts(hang.acts, &on) && !on);
+    }
+
+    // ════ 보내는 길 ════
+    {
+        int16_t slots[8 * IN_SLOTS], mic[8], ref[8];
+        for (int i = 0; i < 8; ++i) { slots[i * 4] = (int16_t)(100 + i); slots[i * 4 + 1] = (int16_t)(200 + i); slots[i * 4 + 2] = 7777; slots[i * 4 + 3] = -7777; }
+        split_slots(slots, 8, mic, ref);
+        bool right = IN_SLOTS == 4 && MIC_SLOT == 0 && REF_SLOT == 1;
+        for (int i = 0; i < 8; ++i) if (mic[i] != 100 + i || ref[i] != 200 + i) right = false;
+        CHECK("mic chip slots: slot 0 = microphone, slot 1 = speaker loopback (echo reference), slots 2 and 3 unused", right);
+    }
+    {
+        TestTone tone; Pcm v(RATE);
+        tone.render(v.data(), RATE, TEST_AMP);
+        int crossings = 0; for (int i = 1; i < RATE; ++i) if (v[i - 1] < 0 && v[i] >= 0) ++crossings;
+        bool formula = true; for (int i = 0; i < RATE; ++i) if (std::abs(v[i] - TEST_AMP * std::sin(2 * PI * TEST_HZ * i / RATE)) > 3) formula = false;
+        CHECK("test tone: 1 kHz at the test level (2000), exact sine", TEST_HZ == 1000 && TEST_AMP == 2000 && crossings >= 999 && crossings <= 1000 && formula && peak(v) <= TEST_AMP && peak(v) >= TEST_AMP - 2);
+        Pcm a(100), b(100); TestTone t2; t2.render(a.data(), 100, 1000); t2.render(b.data(), 100, 1000);
+        TestTone t3; Pcm c(200); t3.render(c.data(), 200, 1000);
+        CHECK("test tone: the phase carries over from block to block (no click)", std::equal(a.begin(), a.end(), c.begin()) && std::equal(b.begin(), b.end(), c.begin() + 100));
+    }
+    {
+        int16_t clean[BLOCK], mic[BLOCK], ref[BLOCK], out[BLOCK]; TestTone tone;
+        for (int i = 0; i < BLOCK; ++i) { clean[i] = (int16_t)(100 + i % 7); mic[i] = (int16_t)(-300 - i % 5); ref[i] = 9000; }
+        pick(Src::AEC, clean, mic, ref, tone, out);
+        bool aec = true; for (int i = 0; i < BLOCK; ++i) if (out[i] != clean[i] * 4) aec = false;
+        CHECK("what to send, normally: the echo-cancelled signal, x4 (PL3b-D10: mic gain 24 dB + 12 dB digital)", aec && TX_GAIN_Q8 == 1024);
+        pick(Src::MIC, clean, mic, ref, tone, out);
+        bool raw = true; for (int i = 0; i < BLOCK; ++i) if (out[i] != mic[i] * 4) raw = false;
+        CHECK("diagnostic 'mic': the raw microphone, same gain", raw);
+        pick(Src::REF, clean, mic, ref, tone, out);
+        CHECK("diagnostic 'ref': the loopback slot; the gain clips at 32767 instead of wrapping", out[0] == 32767 && out[BLOCK - 1] == 32767);
+        pick(Src::TONE, clean, mic, ref, tone, out);
+        CHECK("diagnostic 'tone': the 1 kHz test tone at its own level (no gain), nothing from the mic", peak(Pcm(out, out + BLOCK)) <= TEST_AMP && peak(Pcm(out, out + BLOCK)) >= TEST_AMP - 2 && out[0] == 0);
+        Src s = Src::TONE;
+        CHECK("source names for the serial command", parse_src("aec", &s) && s == Src::AEC && parse_src("mic", &s) && s == Src::MIC && parse_src("ref", &s) && s == Src::REF
+              && parse_src("tone", &s) && s == Src::TONE && !parse_src("AEC", &s) && !parse_src("", &s) && !parse_src(nullptr, &s) && !parse_src("tone ", &s) && s == Src::TONE
+              && std::string(src_name(Src::AEC)) == "aec" && std::string(src_name(Src::REF)) == "ref");
+    }
+    {
+        // 16ms 묶음 → 20ms 프레임. 표본에 번호를 매겨 넣고, 나온 프레임의 표본이 빠짐·겹침 없이 이어지는지 본다
+        Uplink up; up.start();
+        int16_t blk[BLOCK], fr[FRAME]; uint16_t seq = 99; int16_t g = 0; bool none = true;
+        for (int b = 0; b < DISCARD_BLOCKS; ++b) { for (int16_t &v : blk) v = g++; up.push(blk); if (up.pop(fr, &seq)) none = false; }
+        CHECK("uplink: the first 8 blocks (128 ms) after the call opens are thrown away (stale DMA + the ringback tail in the room)", DISCARD_BLOCKS == 8 && none && up.discarded() == 8 && up.pending() == 0);
+        std::vector<int> per_block; bool contiguous = true, numbered = true; int16_t expect = (int16_t)(DISCARD_BLOCKS * BLOCK); uint16_t want_seq = 0; long frames = 0;
+        for (int b = 0; b < 250000; ++b) {                              // 약 67분
+            for (int16_t &v : blk) v = g++;
+            up.push(blk);
+            int n = 0;
+            while (up.pop(fr, &seq)) { ++n; ++frames; if (seq != want_seq++) numbered = false; for (int i = 0; i < FRAME; ++i) if (fr[i] != expect++) contiguous = false; }
+            if (b < 10) per_block.push_back(n);
+        }
+        CHECK("uplink: the first frame starts exactly at sample 2048 (nothing older leaks out)", contiguous);
+        CHECK("uplink: 67 minutes of blocks -> frames with no sample lost, repeated or reordered", contiguous && frames == 250000L * BLOCK / FRAME);
+        CHECK("uplink: sequence numbers count up from 0 and wrap at 65536", numbered && want_seq == (uint16_t)(frames));
+        CHECK("uplink: 4 frames per 5 blocks, in the beat 0,1,1,1,1 (16/16/16/32 ms)", per_block == std::vector<int>({0, 1, 1, 1, 1, 0, 1, 1, 1, 1}));
+        for (int16_t &v : blk) v = 1234;
+        up.push(blk);
+        up.start();
+        CHECK("uplink: a new call starts clean (numbers from 0, leftovers dropped, discards again)", up.pending() == 0 && up.discarded() == 0 && !up.pop(fr, &seq));
+        for (int b = 0; b < DISCARD_BLOCKS + 2; ++b) up.push(blk);
+        CHECK("uplink: ... first frame of the new call is number 0", up.pop(fr, &seq) && seq == 0 && fr[0] == 1234);
+        Uplink lazy; lazy.start();
+        for (int b = 0; b < DISCARD_BLOCKS + 40; ++b) { for (int16_t &v : blk) v = (int16_t)b; lazy.push(blk); }
+        CHECK("uplink: if nobody takes the frames it keeps only the newest audio (never grows)", lazy.pending() <= 4 * BLOCK && lazy.pop(fr, &seq) && fr[0] >= DISCARD_BLOCKS + 36);
+    }
+    {
+        // 프레임 포장(relay_proto.h — 교환기 규약 [순번 2][Opus])과 함께: 압축기 출력이 크든 작든 규격 안
+        uint8_t buf[MAX_TX_FRAME + 8]; uint8_t opus[400] = {}; uint16_t seq = 0; const uint8_t *p = nullptr; size_t n = 0;
+        opus[0] = 0xAB; opus[59] = 0xCD;
+        const size_t len = relayproto::pack_voice(buf, sizeof buf, 0xFFFE, opus, 60);
+        CHECK("frame: [sequence 2 bytes, big end first][opus] and back", len == 62 && buf[0] == 0xFF && buf[1] == 0xFE && buf[2] == 0xAB && buf[61] == 0xCD
+              && relayproto::unpack_voice(buf, len, &seq, &p, &n) && seq == 0xFFFE && n == 60 && p[0] == 0xAB);
+        CHECK("frame: our frames always fit the relay limit (400) and our queue slot (200)", MAX_TX_FRAME <= relayproto::MAX_VOICE_FRAME && 160 + 2 <= MAX_TX_FRAME
+              && relayproto::pack_voice(buf, MAX_TX_FRAME, 1, opus, MAX_TX_FRAME - 1) == 0 && relayproto::pack_voice(buf, MAX_TX_FRAME, 1, opus, MAX_TX_FRAME - 2) == MAX_TX_FRAME);
+    }
+    {
+        // 보내기 대기열: 밀리면 버린다(쌓지 않는다)
+        TxFifo<NoLock> q(g_nolock); TxFrame f; uint8_t d[MAX_TX_FRAME + 1] = {};
+        CHECK("tx queue: empty at first", q.size() == 0 && !q.take(&f, 0) && TX_QUEUE == 10 && TX_STALE_MS == 200);
+        for (int i = 0; i < 3; ++i) { d[0] = (uint8_t)i; q.put(d, 62, 1000 + 20 * i); }
+        CHECK("tx queue: comes out in order with its length", q.size() == 3 && q.take(&f, 1050) && f.data[0] == 0 && f.len == 62 && q.take(&f, 1050) && f.data[0] == 1 && q.size() == 1);
+        q.flush();
+        for (int i = 0; i < 25; ++i) { d[0] = (uint8_t)i; q.put(d, 62, 2000 + 20 * i); }
+        CHECK("tx queue: when the sender is stuck it keeps the newest 10 frames (200 ms) and drops the oldest", q.size() == 10 && q.dropped() == 15 && q.take(&f, 2500) && f.data[0] == 15);
+        CHECK("tx queue: frames older than 200 ms are skipped when taken", q.take(&f, 2000 + 20 * 20 + 201) && f.data[0] == 21 && q.dropped() == 15 + 5 && q.size() == 3);
+        CHECK("tx queue: everything stale -> nothing to send", !q.take(&f, 99999) && q.size() == 0 && q.dropped() == 23);
+        d[0] = 7; q.put(d, 62, 0xFFFFFFF0u);
+        CHECK("tx queue: the millisecond clock wrapping around does not make a fresh frame stale", !stale(0x00000010u, 0xFFFFFFF0u) && stale(0x000000C9u, 0xFFFFFFFFu)
+              && !stale(0xFFFFFFF8u, 0xFFFFFFF0u) && stale(0xFFFFFFFFu, 0xFFFFFF00u) && q.take(&f, 0x00000010u) && f.data[0] == 7);
+        q.put(d, 62, 5000); q.put(d, 62, 5020);
+        CHECK("tx queue: flush empties it and says how many (call start, call end, send failure)", q.flush() == 2 && q.size() == 0 && !q.take(&f, 5030) && q.flush() == 0);
+        CHECK("tx queue: empty, oversized and null frames are refused", !q.put(d, 0, 1) && !q.put(d, MAX_TX_FRAME + 1, 1) && !q.put(nullptr, 10, 1) && q.put(d, MAX_TX_FRAME, 1) && q.size() == 1);
+        q.clear_dropped();
+        CHECK("tx queue: the drop counter is cleared per call", q.dropped() == 0);
+        uint32_t t = 0; long sent = 0; g_seed = 5; TxFifo<NoLock> r(g_nolock); bool order = true; int last = -1; int worst = 0;
+        for (int i = 0; i < 2000000; ++i) {                             // 넣는 쪽 20ms 박자, 꺼내는 쪽은 가끔 오래 막힌다
+            t += 20; d[0] = (uint8_t)(i & 0xFF); d[1] = (uint8_t)(i >> 8); d[2] = (uint8_t)(i >> 16); r.put(d, 62, t);
+            if (r.size() > worst) worst = r.size();
+            if (rnd(50) == 0) { i += (int)rnd(40); t += 0; continue; }
+            while (r.take(&f, t)) { const int id = f.data[0] | f.data[1] << 8 | f.data[2] << 16; if (id <= last) order = false; if (stale(t, f.ms)) order = false; last = id; ++sent; }
+        }
+        CHECK("tx queue: 2 million frames with a sender that stalls now and then: never more than 10 queued, nothing stale or out of order is sent", order && worst <= TX_QUEUE && sent > 1000000);
+    }
+
+    // ════ 빠진 프레임 메우기 (PL3b-D7, Review Focus 3) ════
+    {
+        g_sig = Sig::SINE;
+        auto frame = [](uint32_t idx) { Pcm f(FRAME); for (int i = 0; i < FRAME; ++i) f[i] = sig((int64_t)idx * FRAME + i); return f; };
+        Concealer c; Pcm all;
+        Pcm f0 = frame(0); const Pcm orig0 = f0;
+        c.good(f0.data(), 0);
+        bool fade_in = f0[0] == 0 && std::equal(f0.begin() + FADE, f0.end(), orig0.begin() + FADE);
+        for (int i = 0; i < FADE; ++i) if (std::abs((int)f0[i]) > std::abs((int)orig0[i]) * i / FADE + 1) fade_in = false;
+        CHECK("conceal: the first frame of a call fades in over 5 ms, the rest is untouched", fade_in);
+        all = f0;
+        Pcm f1 = frame(1), f2 = frame(2);
+        c.good(f1.data(), 1); c.good(f2.data(), 2);
+        CHECK("conceal: frames that follow each other are passed through bit for bit", f1 == frame(1) && f2 == frame(2));
+        all.insert(all.end(), f1.begin(), f1.end()); all.insert(all.end(), f2.begin(), f2.end());
+        Pcm l1(FRAME), l2(FRAME), l3(FRAME), l4(FRAME);
+        c.lost(l1.data()); c.lost(l2.data()); c.lost(l3.data()); c.lost(l4.data());
+        for (const Pcm *p : {&l1, &l2, &l3, &l4}) all.insert(all.end(), p->begin(), p->end());
+        CHECK("conceal: a lost frame continues from the last sample with no step (mirrored replay)", max_jump(all) <= SIG_STEP);
+        CHECK("conceal: it fades over 3 frames (60 ms): about 2/3, 1/3, then exactly silent at the end", peak(l1) > SIG_AMP * 2 / 3 - 100 && peak(l1) <= SIG_AMP && peak(l2) <= SIG_AMP * 2 / 3 + 1
+              && peak(l2) > SIG_AMP / 3 - 100 && peak(l3) <= SIG_AMP / 3 + 1 && l3[FRAME - 1] == 0 && jitterbuf::MAX_CONCEAL == 3);
+        CHECK("conceal: after that, more lost frames are pure silence", peak(l4) == 0);
+        Pcm f7 = frame(7);
+        c.good(f7.data(), 7);
+        all.insert(all.end(), f7.begin(), f7.end());
+        CHECK("conceal: the next real frame fades in (no click after silence)", f7[0] == 0 && max_jump(all) <= JOIN_STEP && std::equal(f7.begin() + FADE, f7.end(), frame(7).begin() + FADE));
+        Pcm l5(FRAME);
+        c.lost(l5.data());                                              // 한 프레임만 빠짐
+        Pcm f9 = frame(9);
+        c.good(f9.data(), 9);
+        all.insert(all.end(), l5.begin(), l5.end()); all.insert(all.end(), f9.begin(), f9.end());
+        CHECK("conceal: one lost frame then a real one — cross-faded over 5 ms, no step anywhere", max_jump(all) <= JOIN_STEP && std::equal(f9.begin() + FADE, f9.end(), frame(9).begin() + FADE) && f9[0] != 0);
+        Pcm f20 = frame(20);
+        c.good(f20.data(), 20);                                         // 순번이 건너뜀(오래된 프레임을 버린 뒤) — LOST 표시 없이
+        all.insert(all.end(), f20.begin(), f20.end());
+        CHECK("conceal: a jump in the sequence (old frames were dropped) is joined the same way", max_jump(all) <= JOIN_STEP && f20 != frame(20) && std::equal(f20.begin() + FADE, f20.end(), frame(20).begin() + FADE));
+        Pcm s1(FRAME), s2(FRAME);
+        c.silence(s1.data()); c.silence(s2.data());
+        all.insert(all.end(), s1.begin(), s1.end());
+        CHECK("conceal: going silent while sound was playing fades out in 5 ms (no click), then exact zeros", max_jump(all) <= JOIN_STEP && peak(s1, FADE) == 0 && peak(s1, 0, FADE) > 0 && peak(s2) == 0);
+        Pcm f21 = frame(21);
+        c.good(f21.data(), 21);
+        CHECK("conceal: a frame right after silence fades in even if its number follows on", f21[0] == 0);
+        c.reset();
+        Pcm l6(FRAME, 5);
+        c.lost(l6.data());
+        CHECK("conceal: nothing to repeat yet (new call) -> a lost frame is silence", peak(l6) == 0);
+        Pcm w0 = frame(65535); c.good(w0.data(), 65535); Pcm w1 = frame(65536); c.good(w1.data(), 0);
+        CHECK("conceal: sequence 65535 -> 0 counts as consecutive", w1 == frame(65536));
+        // 큰 소리(꼭대기까지)여도 넘치지 않는다
+        Concealer big; Pcm top(FRAME); for (int i = 0; i < FRAME; ++i) top[i] = i % 2 ? 32767 : -32768;
+        big.good(top.data(), 0); Pcm t1 = top; big.good(t1.data(), 1);
+        Pcm lost(FRAME); big.lost(lost.data()); Pcm t2 = top; big.good(t2.data(), 3);
+        CHECK("conceal: full-scale input never wraps around", peak(lost) <= 32768 && peak(t2) <= 32768 && t1 == top);
+    }
+
+    // ════ 받는 길 (PL3b-D5·D6·D13) ════
+    {
+        g_sig = Sig::SINE; g_dec_calls = g_fresh_calls = 0;
+        static Rx rx(g_nolock);
+        int16_t out[BLOCK]; for (int16_t &v : out) v = 55;
+        feed(rx, 0); feed(rx, 1); feed(rx, 2); feed(rx, 3);
+        CHECK("rx: before any call there is no sound and the block is not touched — frames that arrive outside a call are not even stored",
+              !rx.open() && !rx.fill(out, BLOCK) && out[0] == 55 && out[BLOCK - 1] == 55 && rx.stats().frames == 0 && rx.stats().depth == 0 && !rx.want_pump() && rx.pump(dec, fresh) == 0);
+        rx.start();
+        Pcm quiet = pull_n(rx, 10);
+        RxStats s = rx.stats();
+        CHECK("rx: call open, nothing received yet -> the voice source plays silence (keeps the 16 ms beat going)", peak(quiet) == 0 && s.frames == 0 && s.played == 0);
+        CHECK("rx: waiting for the first frames is not counted as an underrun or a concealed frame", s.under == 0 && s.concealed == 0 && s.depth == 0 && s.target == jitterbuf::START_FRAMES);
+        CHECK("rx: the decoder is reset once at the start of the call", rx.open() && g_fresh_calls == 1 && g_dec_calls == 0);
+        feed(rx, 0); feed(rx, 1);
+        CHECK("rx: two frames (40 ms) are not enough to start (target 60 ms)", peak(pull_n(rx, 2)) == 0 && rx.stats().played == 0 && rx.stats().depth == 2);
+        feed(rx, 2);
+        Flow fl; fl.next = 3;
+        Pcm v = flow(rx, fl, 130);                                      // 약 2초
+        const long a = align(v, 0);
+        CHECK("rx: then the decoded audio comes out in order, sample for sample (no gap, no repeat between 20 ms frames and 16 ms blocks)", a >= 0 && v.size() == 130u * BLOCK && exact_from(v, a, 0));
+        CHECK("rx: the first voice sample is taken within one block (16 ms) of the third frame arriving — no silence queued ahead of it", a >= 0 && a < BLOCK);
+        CHECK("rx: it starts with a 5 ms fade-in and never clicks", a >= 0 && peak(v, 0, (size_t)a + 1) == 0 && max_jump(v) <= JOIN_STEP);
+        s = rx.stats();
+        CHECK("rx: counters: received = fed, played, nothing lost, nothing bad", s.frames == fl.next && s.played >= fl.next - 4 && s.concealed == 0 && s.bad == 0 && s.dec_fail == 0 && s.under == 0 && s.jb.late == 0);
+        CHECK("rx: the jitter buffer holds 2..3 frames in a steady stream", s.depth >= 2 && s.depth <= 3 && s.jb.overflow_drop == 0 && s.jb.catchup_drop == 0);
+        const long ahead = (long)s.played * FRAME - ((long)v.size() - a);            // 해제는 했지만 아직 출력 태스크가 가져가지 않은 표본(링버퍼에 든 것)
+        CHECK("rx: it never decodes further ahead than one block + one frame (the ring adds at most 36 ms)", a >= 0 && ahead >= 0 && ahead < BLOCK + FRAME);
+        CHECK("rx: it reports the level of what it plays (amplitude 6000 -> RMS about 4243)", s.play_rms > 4150 && s.play_rms < 4330);
+
+        // 통화가 끝난다: 남은 소리를 5ms 에 걸쳐 줄이고 끝 — 그 뒤로는 소리 없음(남은 프레임은 버린다)
+        const int16_t last = v.back();
+        int16_t tail[BLOCK];
+        rx.stop();
+        const bool tail_sound = rx.fill(tail, BLOCK);
+        Pcm t(tail, tail + BLOCK);
+        CHECK("rx: voice off -> one last block that fades the remaining sound out in 5 ms (no click), zeros after", tail_sound && max_jump(t, last) <= JOIN_STEP && peak(t, FADE) == 0 && peak(t, 0, FADE) > 0 && t[FADE - 1] == 0);
+        for (int16_t &x : tail) x = 55;
+        CHECK("rx: ... and then no sound at all (the output task may sleep), the block is not touched", !rx.fill(tail, BLOCK) && tail[0] == 55 && !rx.fill(tail, BLOCK));
+        const uint32_t frames_at_end = rx.stats().frames; const int dec_at_end = g_dec_calls; const int left_in_jb = rx.stats().depth;
+        CHECK("rx: a stray wake-up of the decoder task after the call ended decodes nothing (frames were still waiting in the jitter buffer)",
+              left_in_jb >= 2 && rx.pump(dec, fresh) == 0 && rx.pump(dec, fresh) == 0 && g_dec_calls == dec_at_end && !rx.want_pump());
+        for (uint32_t i = fl.next; i < fl.next + 5; ++i) feed(rx, i);   // 끝난 뒤 뒤늦게 온 프레임
+        CHECK("rx: frames that arrive after the call ended are dropped, nothing is decoded any more", rx.stats().frames == frames_at_end && g_dec_calls == dec_at_end && !rx.fill(tail, BLOCK));
+
+        // 다음 통화: 앞 통화의 찌꺼기(지터 버퍼·링버퍼·통계)가 한 표본도 나오지 않는다
+        const int fresh_before = g_fresh_calls;
+        rx.start();
+        Pcm next = pull_n(rx, 12);
+        s = rx.stats();
+        CHECK("rx: the next call starts silent — nothing of the previous call is played", peak(next) == 0 && s.depth == 0);
+        CHECK("rx: ... with fresh counters and a fresh decoder", s.frames == 0 && s.played == 0 && s.jb.played == 0 && s.play_rms == 0 && g_fresh_calls == fresh_before + 1);
+        Flow f2; f2.next = 5000;
+        Pcm again = flow(rx, f2, 20);
+        CHECK("rx: it accepts whatever sequence number the new call starts with", exact_from(again, align(again, 5000LL * FRAME), 5000LL * FRAME));
+    }
+    {
+        // 해제 태스크가 굶으면(출력 태스크가 가져가려는데 비어 있다) 무음으로 메우고 센다. 채우는 중(통화 시작)은 세지 않는다
+        g_sig = Sig::SINE;
+        static Rx rx(g_nolock); rx.start();
+        for (uint32_t i = 0; i < 6; ++i) feed(rx, i);
+        pull_n(rx, 4);
+        int16_t out[BLOCK];
+        rx.fill(out, BLOCK); rx.fill(out, BLOCK); rx.fill(out, BLOCK);                          // pump 를 부르지 않는다
+        CHECK("rx: the output task never waits — if the decoder task is starved it plays zeros and counts it", rx.stats().under >= 1 && rx.stats().under <= 3 && peak(Pcm(out, out + BLOCK)) == 0);
+        const uint32_t u = rx.stats().under;
+        rx.pump(dec, fresh);
+        Pcm v = pull_n(rx, 2);
+        CHECK("rx: it carries on afterwards", rx.stats().under == u && peak(v) > 0);
+    }
+    {
+        // 규격 밖 프레임·해제 실패
+        g_sig = Sig::SINE;
+        static Rx rx(g_nolock); rx.start();
+        uint8_t f[512] = {};
+        rx.push(f, 2); rx.push(f, 401); rx.push(nullptr, 10); rx.push(f, 0);
+        CHECK("rx: frames shorter than 3 or longer than 400 bytes are counted and dropped", rx.stats().bad == 4 && rx.stats().frames == 0 && rx.stats().depth == 0);
+        rx.push(f, 3); rx.push(f, 400);
+        CHECK("rx: 3 and 400 bytes are accepted", rx.stats().frames == 2 && rx.stats().bad == 4);
+        rx.start();
+        for (uint32_t i = 0; i < 3; ++i) feed(rx, i);
+        pull_n(rx, 2);
+        g_dec_fail = true;
+        for (uint32_t i = 3; i < 6; ++i) feed(rx, i);
+        Pcm v = pull_n(rx, 4);
+        g_dec_fail = false;
+        CHECK("rx: a frame the decoder rejects is concealed like a lost one (no crash, no garbage)", rx.stats().dec_fail >= 1 && rx.stats().concealed == rx.stats().dec_fail && max_jump(v, sig(0)) <= SIG_AMP);
+    }
+
+    // ════ 신호음과 목소리가 겹치는 순간 (PL3b-D13) ════
+    {
+        g_sig = Sig::SINE;
+        static Rx rx(g_nolock); g_rx = &rx; g_voice_on = false; g_tone = tones::Player(); g_tone_has = false;
+        // 거는 쪽: 연결음이 울리는 중(1초 울림 구간의 한가운데)에 통화가 열린다 — 한 묶음(0x180)에 "연결음 멈춤 + 목소리 켜기"
+        Fsm f = in_state(State::RINGBACK);
+        apply_acts(A_TONE_RINGBACK);
+        Pcm before; int16_t blk[BLOCK];
+        for (int i = 0; i < 20; ++i) { mix_block(blk); before.insert(before.end(), blk, blk + BLOCK); }         // 320ms: 연결음이 최대 크기
+        CHECK("overlap: ringback alone before the call opens", peak(before) > tones::RINGBACK_AMP * 9 / 10 && !g_voice_on);
+        apply_acts(f.on(Ev::CONNECTED, 5000).acts);
+        Uplink up; up.start();                                          // 같은 순간 보내는 쪽도 시작한다
+        CHECK("overlap: the batch opened the voice and commanded the ringback to stop", g_voice_on && g_tone_has && g_tone_cmd == tones::Tone::NONE);
+        const int n1 = mix_block(blk);
+        Pcm first(blk, blk + BLOCK);
+        CHECK("overlap: in the very first block both sources sound: the ringback fading out + the voice source (still silent)", n1 == 2);
+        CHECK("overlap: the ringback is gone within 5 ms, without a click, and the rest of that block is exact silence",
+              max_jump(first, before.back()) <= (int)std::ceil(tones::RINGBACK_AMP * 2 * PI * 480 / RATE) + tones::RINGBACK_AMP / tones::FADE + 4
+              && peak(first, tones::FADE) == 0 && peak(first, 0, tones::FADE) > 0);
+        // 상대의 첫 프레임들이 도착한다(20ms 마다). 지터 버퍼가 60ms 를 채울 때까지는 무음 — 연결음 꼬리와 목소리가 섞여 나가는 일이 없다
+        Pcm opening; int blocks_until_voice = -1;
+        for (int i = 0; i < 12; ++i) {
+            if (i % 5 != 4) feed(rx, (uint32_t)(i - i / 5));
+            const int n = mix_block(blk);
+            if (n != 1) blocks_until_voice = -100;                      // 연결음 공급자는 더는 소리가 없어야 한다(목소리 공급자만)
+            if (blocks_until_voice == -1 && peak(Pcm(blk, blk + BLOCK))) blocks_until_voice = i;
+            opening.insert(opening.end(), blk, blk + BLOCK);
+        }
+        CHECK("overlap: the first voice comes at least 2 blocks (32 ms) after the ringback stopped — its tail in the DMA has played out by then", blocks_until_voice >= 2);
+        CHECK("overlap: and it fades in (no click on top of the silence)", max_jump(opening) <= JOIN_STEP && peak(opening) > SIG_AMP / 2);
+        // 보내는 쪽: 연결음 꼬리가 방에 남아 있는 동안의 마이크는 상대에게 가지 않는다
+        int16_t mic[BLOCK]; for (int16_t &v : mic) v = 3000; int16_t fr[FRAME]; uint16_t seq; bool leaked = false;
+        for (int i = 0; i < 6; ++i) { up.push(mic); if (up.pop(fr, &seq)) leaked = true; }                      // 96ms = 멈춤 5ms + DMA 에 이미 든 32ms + 여유
+        CHECK("overlap: nothing captured in the first 96 ms after the call opens is sent (the ringback tail stays out of the uplink)", !leaked && DISCARD_BLOCKS * 16 >= 96 + 32);
+
+        // 통화 중: 신호음 공급자는 조용하다 → 섞은 것 = 목소리 그대로
+        Pcm talk = opening; int16_t solo[BLOCK]; int n_call = 1; uint32_t nextf = 10;
+        for (int i = 12; i < 60; ++i) { if (i % 5 != 4) feed(rx, nextf++); if (mix_block(blk) != 1) n_call = 0; talk.insert(talk.end(), blk, blk + BLOCK); }
+        CHECK("in call: only the voice source sounds, and the mix is the decoded voice sample for sample (the silent tone source adds nothing)",
+              n_call == 1 && exact_from(talk, align(talk, 0), 0));
+
+        // 끝: 한 묶음(0x240)에 "목소리 끄기 + 종료음". 목소리는 5ms 에 걸쳐 줄고, 종료음은 처음부터 제 모양 그대로 — 겹치는 5ms 는 더해진다
+        const int16_t last = blk[BLOCK - 1];
+        Fsm c = in_state(State::IN_CALL);
+        apply_acts(c.on(Ev::ENDED, 9000, Why::HANGUP).acts);
+        CHECK("call end: the batch closed the voice and commanded the end tone", !g_voice_on && g_tone_has && g_tone_cmd == tones::Tone::END);
+        const int n_end = mix_block(blk);
+        Pcm endblk(blk, blk + BLOCK);
+        tones::Player ref_p; ref_p.command(tones::Tone::END); int16_t ref_end[BLOCK]; ref_p.render(ref_end, BLOCK);
+        bool tone_exact = true; for (int i = FADE; i < BLOCK; ++i) if (endblk[i] != ref_end[i]) tone_exact = false;
+        bool sum_ok = true; for (int i = 0; i < FADE; ++i) if (std::abs(endblk[i] - ref_end[i]) > SIG_AMP) sum_ok = false;
+        CHECK("call end: that block = the voice fading out (5 ms) + the end tone starting; after 5 ms it is the end tone alone, sample for sample", n_end == 2 && tone_exact && sum_ok);
+        CHECK("call end: no click at the hand-over, no clipping", max_jump(endblk, last) <= JOIN_STEP + tones::END_AMP / tones::FADE + 300 && peak(endblk) < 32767);
+        Pcm rest; int sounding = 0;
+        for (int i = 0; i < 40; ++i) { const int n = mix_block(blk); if (n) { ++sounding; rest.insert(rest.end(), blk, blk + BLOCK); } if (n > 1) sounding = -1000; }
+        tones::Player ref_q; ref_q.command(tones::Tone::END); Pcm ref_all(rest.size() + BLOCK); ref_q.render(ref_all.data(), (int)ref_all.size());
+        CHECK("call end: from then on only the end tone (0.52 s), then nobody has sound — the output task sleeps", sounding > 25 && sounding < 35
+              && std::equal(rest.begin(), rest.end(), ref_all.begin() + BLOCK) && mix_block(solo) == 0 && mix_block(solo) == 0);
+        for (uint32_t i = nextf; i < nextf + 8; ++i) feed(rx, i);       // 끝난 뒤 뒤늦게 온 프레임
+        CHECK("call end: frames that arrive late are not played", mix_block(solo) == 0);
+        // 끊자마자 다음 전화: 벨만 난다(앞 통화의 목소리가 섞이지 않는다)
+        apply_acts(A_TONE_BELL);
+        tones::Player ref_b; ref_b.command(tones::Tone::BELL); int16_t ref_bell[BLOCK]; ref_b.render(ref_bell, BLOCK);
+        CHECK("a bell right after: exactly the bell, no leftover voice mixed in", mix_block(blk) == 1 && std::equal(blk, blk + BLOCK, ref_bell));
+        g_rx = nullptr;
+    }
+
+    // ════ 오래·거칠게: 한 방향 통화 시뮬레이션 (Review Focus 3·4) ════
+    Sim lan, slow_tx, fast_tx, wan;
+    {
+        g_seed = 11; lan.run(600);                                      // 10분, 고른 망(3~7ms)
+        CHECK("sim LAN 10 min: once started every block continues the previous one — no sample lost or repeated", lan.broken <= 1 && lan.st.concealed == 0 && lan.st.under == 0);
+        CHECK("sim LAN: delay from microphone sample to the output task stays within 60..125 ms (capture block + framing + jitter buffer + ring)", lan.pct(0) >= 60 && lan.pct(100) <= 125);
+        CHECK("sim LAN: and it does not drift (spread under 2 frames)", lan.pct(100) - lan.pct(0) <= 40);
+        g_seed = 12; slow_tx.tx_ppm = -200; slow_tx.run(3600);          // 한 시간, 보내는 쪽 시계가 0.02% 느리다(수정 발진기 오차 ±50ppm 두 개의 2배)
+        CHECK("sim 1 h, sender clock 200 ppm slow: a handful of single concealed frames, never a long gap", slow_tx.st.concealed >= 1 && slow_tx.st.concealed <= 60 && slow_tx.st.jb.underruns == slow_tx.st.concealed);
+        CHECK("sim 1 h, sender slow: the delay stays bounded", slow_tx.pct(100) <= 150);
+        g_seed = 13; fast_tx.tx_ppm = 200; fast_tx.run(3600);
+        CHECK("sim 1 h, sender clock 200 ppm fast: frames are dropped now and then to keep up, (almost) nothing concealed", fast_tx.st.jb.catchup_drop >= 1 && fast_tx.st.jb.catchup_drop <= 80 && fast_tx.st.concealed <= 6);
+        CHECK("sim 1 h, sender fast: the delay does not pile up", fast_tx.pct(100) <= 150);
+        g_seed = 14; wan.net_base = 45000; wan.net_jitter = 30000; wan.run(600);   // 터널 경유(Phase 4): 45~75ms
+        CHECK("sim tunnel-like network (45..75 ms, jitter 30 ms) 10 min: plays through, delay under 250 ms", wan.pct(100) <= 250 && wan.st.played > 29000);
+    }
+    {
+        // 망이 멈췄다 풀린다(통화 5초째). 풀리고 2초 뒤부터는 지연이 평소로 돌아와 있다 — 쌓이지 않는다
+        bool back = true, bounded = true; std::string detail;
+        for (int stall_ms : {60, 150, 300, 600, 1500, 4000}) {
+            Sim s; g_seed = 100 + (uint32_t)stall_ms; s.stall_at = 5000000; s.stall_len = (int64_t)stall_ms * 1000;
+            s.run(20, 5000000 + (int64_t)stall_ms * 1000 + 2000000);
+            if (s.worst_after > 140) back = false;
+            if (s.st.depth > jitterbuf::OVERFLOW_FRAMES) bounded = false;
+            if (info) detail += "  stall " + std::to_string(stall_ms) + " ms: delay after recovery max " + std::to_string(s.worst_after) + " ms, concealed " + std::to_string(s.st.concealed)
+                              + ", dropped " + std::to_string(s.st.jb.overflow_drop + s.st.jb.catchup_drop) + ", underruns " + std::to_string(s.st.jb.underruns) + "\n";
+        }
+        CHECK("sim stall 60 ms .. 4 s then a burst: 2 s after it clears the delay is back under 140 ms (old audio is dropped, not queued)", back && bounded);
+        if (info) printf("%s", detail.c_str());
+    }
+
+    // ════ 지연 재기 (PL3b-D12) ════
+    {
+        PingGen p; Pcm all; int at = -1, at_block = -1; int16_t blk[BLOCK];
+        CHECK("ping: idle until started", !p.active());
+        p.start();
+        for (int b = 0; p.active() && b < 200; ++b) { const int a = p.render(blk, BLOCK); if (a >= 0) { at = a; at_block = b; } all.insert(all.end(), blk, blk + BLOCK); }
+        const int start = at_block * BLOCK + at;
+        CHECK("ping: 0.4 s of silence, then the beep starts exactly at sample 6400", start == PING_LEAD && PING_LEAD == 6400 && peak(all, 0, (size_t)PING_LEAD) == 0);
+        CHECK("ping: a 0.1 s 1 kHz beep at level 4000 with 5 ms edges (no click)", peak(all, (size_t)PING_LEAD, (size_t)(PING_LEAD + PING_BURST)) >= PING_AMP - 2 && peak(all) <= PING_AMP
+              && max_jump(all) <= (int)std::ceil(PING_AMP * 2 * PI * 1000 / RATE) + PING_AMP / FADE + 4);
+        CHECK("ping: then silence for 0.5 s and it ends by itself (1 s in all)", peak(all, (size_t)(PING_LEAD + PING_BURST)) == 0 && !p.active() && all.size() >= 16000 && all.size() < 16000 + BLOCK);
+        CHECK("onset: finds the first sample of the returned beep above the threshold, -1 if there is none",
+              onset(all.data(), PING_LEAD, PING_DETECT) == -1 && onset(all.data() + PING_LEAD, BLOCK, PING_DETECT) > 0 && onset(all.data() + PING_LEAD, BLOCK, PING_DETECT) < FADE
+              && onset(all.data(), (int)all.size(), 32767) == -1);
+        int16_t neg[4] = {0, -1499, -1500, 9};
+        CHECK("onset: negative swings count too", onset(neg, 4, PING_DETECT) == 2);
+        p.start();
+        CHECK("ping: can be started again", p.active() && p.render(blk, BLOCK) == -1);
+
+        SelfTestTone st; Pcm tone; int blocks = 0, first_at = -2;
+        CHECK("selftest tone: idle until started (renders silence)", !st.active() && st.render(blk, BLOCK) == -1 && peak(Pcm(blk, blk + BLOCK)) == 0);
+        st.start();
+        while (st.active() && blocks < 100) { const int a = st.render(blk, BLOCK); if (blocks == 0) first_at = a; tone.insert(tone.end(), blk, blk + BLOCK); ++blocks; }
+        CHECK("selftest tone: 0.64 s (40 blocks) of 1 kHz at the test level, starting at once, fading in and out, then it stops by itself",
+              first_at == 0 && blocks == 40 && SELFTEST_TONE == 40 * BLOCK && peak(tone) <= TEST_AMP && peak(tone) >= TEST_AMP - 2 && tone.front() == 0 && tone.back() == 0
+              && max_jump(tone) <= (int)std::ceil(TEST_AMP * 2 * PI * 1000 / RATE) + TEST_AMP / FADE + 4);
+        CHECK("selftest: the tone covers the stale blocks thrown away + the measuring window + the DMA write-ahead",
+              SELFTEST_TONE >= (SELFTEST_SKIP_BLOCKS + SELFTEST_BLOCKS + 3) * BLOCK && SELFTEST_SKIP_BLOCKS >= 3 && SELFTEST_REF_MIN == 300);
+        pcm::Rms r; r.add(tone.data() + SELFTEST_SKIP_BLOCKS * BLOCK, SELFTEST_BLOCKS * BLOCK);
+        CHECK("selftest: the tone reads RMS 1414 in the measuring window (what the loopback slot is compared against)", r.value() >= 1410 && r.value() <= 1416);
+        st.start(); st.render(blk, BLOCK); st.stop();
+        CHECK("selftest tone: stop cuts it (a call opening during the test)", !st.active() && st.render(blk, BLOCK) == -1 && peak(Pcm(blk, blk + BLOCK)) == 0);
+    }
+
+    if (info) {
+        printf("\n-- delay simulation (ms from a sample entering the microphone to the output task taking it; add DMA write-ahead + codec/AEC internals) --\n");
+        const struct { const char *name; const Sim *s; } rows[] = {{"LAN 3-7 ms, 10 min", &lan}, {"sender 200 ppm slow, 1 h", &slow_tx}, {"sender 200 ppm fast, 1 h", &fast_tx}, {"tunnel-like 45-75 ms, 10 min", &wan}};
+        for (const auto &r : rows)
+            printf("  %-30s min %3d  median %3d  p95 %3d  max %3d   concealed %u  dropped %u  underruns %u  jb depth/target %d/%d\n", r.name, r.s->pct(0), r.s->pct(50), r.s->pct(95), r.s->pct(100),
+                   (unsigned)r.s->st.concealed, (unsigned)(r.s->st.jb.overflow_drop + r.s->st.jb.catchup_drop), (unsigned)r.s->st.jb.underruns, r.s->st.depth, r.s->st.target);
+        int lo = 9999, hi = 0; long sum = 0, n = 0;                     // 고른 망에서, 두 기기의 16ms 박자가 어긋난 정도(0~15ms)와 통화가 열린 순간을 바꿔 가며 60초씩
+        for (int phase = 0; phase < 16; ++phase) for (uint32_t seed = 1; seed <= 4; ++seed) {
+            Sim s; g_seed = seed * 977 + (uint32_t)phase; s.rx_phase = phase * 1000 + (int64_t)rnd(1000); s.run(60);
+            const int m = s.pct(50); lo = std::min(lo, m); hi = std::max(hi, m); sum += m; ++n;
+        }
+        printf("  %-30s median delay over 64 runs: min %d  mean %ld  max %d\n", "LAN, start phase swept", lo, sum / n, hi);
+    }
+    printf("\n%d ok, %d failed\n", ok, bad);
+    return bad ? 1 : 0;
+}
```

- [x] **Step 2: 구현 패치를 적용하고 통과를 확인한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft && git apply "$D/patches/T5-02-voice-core.patch" && for f in main/voice_core.h tools/test_voice_core.cpp; do cmp "$D/files/firmware/tab5_phone/$f" "firmware/tab5_phone/$f" && echo "같음: $f"; done && cd firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_voice_core.cpp -o /tmp/t_p3b_voice_core 2> /tmp/t_p3b_voice_core.err; echo "경고 $(wc -l < /tmp/t_p3b_voice_core.err | tr -d ' ')줄"; /tmp/t_p3b_voice_core | grep -c "^  FAIL"; /tmp/t_p3b_voice_core | tail -1
```
Expected: `같음:` 2줄, `경고 0줄`, `0`, `117 ok, 0 failed` (1~2초).

```diff
--- /dev/null
+++ b/firmware/tab5_phone/main/voice_core.h
@@ -0,0 +1,431 @@
+// voice_core.h — 목소리 통화의 판단 로직. 하드웨어·시계·OS 와 무관한 순수 코드(맥에서 단위 시험: tools/test_voice_core.cpp).
+//
+// 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md 의 결정 PL3b-D2·D5~D13. 기기 전용 코드(voice.cpp)는 여기 것을 태스크에 잇기만 한다.
+//   보내는 길: 마이크 4칸 → split_slots → (에코 제거 — 기기) → pick(무엇을 보낼지) → Uplink(16ms 묶음 → 20ms 프레임, 순번)
+//             → (Opus 압축 — 기기) → relayproto::pack_voice → TxFifo(밀리면 오래된 것부터 버린다) → 교환기
+//   받는 길:  교환기 → Receiver::push(지터 버퍼) → Receiver::pump(프레임 하나씩 꺼내 해제 — 빠진 것은 Concealer 가 메운다 — 링버퍼에)
+//             → Receiver::fill(출력 태스크가 16ms 묶음으로 가져간다. audio_out 의 둘째 공급자)
+//   켜고 끄기: 상태 기계의 A_VOICE_START·A_VOICE_STOP → from_acts. 받기 전에는 켜지지 않는다(PH-D6 — 시험이 모든 상태 × 사건으로 고정).
+#pragma once
+#include <atomic>
+#include <cstddef>
+#include <cstdint>
+#include <cstring>
+
+#include "call_fsm.h"
+#include "jitter_buffer.h"
+#include "pcm.h"
+#include "relay_proto.h"
+#include "tones.h"
+
+namespace voicecore {
+
+constexpr int RATE = 16000;                 // PH-D5
+constexpr int BLOCK = 256;                  // 마이크·에코 제거·스피커 출력의 한 묶음(16ms) — audio_out 의 BLOCK, esp-sr VOIP 방식의 묶음과 같다
+constexpr int FRAME = 320;                  // Opus 한 프레임(20ms)
+constexpr int FADE = 5 * RATE / 1000;       // 5ms(80표본): 목소리가 시작·끝·이어 붙을 때 크기를 옮기는 길이(딸깍 방지 — tones.h 의 FADE 와 같은 값)
+constexpr int IN_SLOTS = 4, MIC_SLOT = 0, REF_SLOT = 1;   // 마이크 칩(ES7210)의 칸: 0 = 마이크, 1 = 스피커 되먹임(에코 기준). Phase 0 실측
+
+// PL3b-D9 통화가 열린 뒤 마이크의 앞 8묶음(128ms)은 보내지 않는다: 그 안에는 받기 전에 DMA 에 쌓여 있던 소리(3칸 = 48ms)와
+//   아직 스피커에서 나고 있는 연결음 꼬리(멈춤 5ms + 앞서 쓴 32ms)가 들어 있다. 참고 구현(xiaozhi)은 120ms 를 버린다.
+constexpr int DISCARD_BLOCKS = 8;
+// PL3b-D10 크기: 마이크 증폭은 24dB(audio_out.cpp)로 낮추고 에코 제거 뒤에 디지털로 4배(+12dB) 한다. 재생은 그대로(1배).
+//   Phase 0: 음량 100·마이크 24dB 에서 에코 포화 0%·잔향 감소 41.8dB / 30dB 에서는 꼭대기 포화 0.05%. 사용자 말소리는 30dB 에서 −28dBFS 안팎
+//   → 24dB + 4배면 −22dBFS = 사용자가 "적당"이라 한 재생 크기(시험 음성 −16.9dBFS 를 음량 90 으로).
+constexpr int TX_GAIN_Q8 = 4 * 256;
+constexpr int PLAY_GAIN_Q8 = 256;
+// PL3b-D8 보내기 대기열: 10프레임(200ms — 설계서 §9.6). 가득 차면 오래된 것부터 버리고, 200ms 넘게 묵은 것은 보내지 않는다.
+constexpr int TX_QUEUE = 10;
+constexpr uint32_t TX_STALE_MS = 200;
+constexpr size_t MAX_TX_FRAME = 200;        // [순번 2][Opus] 한 프레임의 상한(24kbps·20ms = 60바이트, 압축기의 권장 출력 버퍼 160바이트)
+constexpr int RING_CAP = 1024;              // 해제한 PCM 링버퍼(64ms). 평소에는 256~575 표본이 들어 있다
+// PL3b-D11·D12 시험음과 지연 재기
+constexpr int TEST_HZ = 1000;
+constexpr int TEST_AMP = 2000;              // 시험음(vsrc tone, selftest)의 크기 — 벨(8000)보다 12dB 작다
+constexpr int PING_AMP = 4000;              // 지연 재기의 삑
+constexpr int PING_LEAD = 400 * RATE / 1000, PING_BURST = 100 * RATE / 1000, PING_TAIL = 500 * RATE / 1000;   // 쉼 0.4초 → 삑 0.1초 → 쉼 0.5초
+constexpr int PING_DETECT = 1500;           // 돌아온 소리에서 이 크기를 넘는 첫 표본을 삑의 시작으로 본다
+// PL3b-D14 혼자 하는 점검(selftest): 시험음을 0.64초 내며 마이크 칩의 두 칸을 잰다. 앞 8묶음(묵은 DMA)은 버리고 24묶음(0.38초)을 잰다.
+//   되먹임 칸(ref)은 앰프를 꺼도 잡힌다(Phase 0: 1kHz·진폭 8000·음량 100 에서 2370 → 진폭 2000 이면 약 590). 300 이상이면 "코덱이 소리를 내고 있다".
+constexpr int SELFTEST_TONE = 640 * RATE / 1000;
+constexpr int SELFTEST_SKIP_BLOCKS = DISCARD_BLOCKS, SELFTEST_BLOCKS = 24;
+constexpr uint32_t SELFTEST_REF_MIN = 300;
+
+// ── 켜고 끄기 ────────────────────────────────────────────────────────────────
+// 상태 기계의 할 일 → 목소리 경로를 켤지 끌지. 목소리에 관한 비트가 없으면 false(지금 상태 그대로).
+// 끄기가 켜기보다 앞선다 — 둘이 함께 오면(지금 상태 기계는 그러지 않는다. 시험으로 고정) 마이크를 닫는 쪽을 고른다(PH-D6).
+inline bool from_acts(uint16_t acts, bool *on)
+{
+    if (acts & callfsm::A_VOICE_STOP) *on = false;
+    else if (acts & callfsm::A_VOICE_START) *on = true;
+    else return false;
+    return true;
+}
+
+// ── 보내는 길 ────────────────────────────────────────────────────────────────
+// 마이크 칩에서 읽은 4칸 묶음(칸0,칸1,칸2,칸3,칸0,…)에서 마이크와 에코 기준을 떼어 낸다.
+inline void split_slots(const int16_t *slots, int frames, int16_t *mic, int16_t *ref)
+{
+    for (int i = 0; i < frames; ++i) { mic[i] = slots[i * IN_SLOTS + MIC_SLOT]; ref[i] = slots[i * IN_SLOTS + REF_SLOT]; }
+}
+
+// 1kHz 시험음(위상을 이어 간다).
+class TestTone {
+public:
+    void reset() { ph_ = 0; }
+    void render(int16_t *out, int n, int amp)
+    {
+        const uint32_t step = tones::phase_step(TEST_HZ);
+        for (int i = 0; i < n; ++i) { out[i] = (int16_t)(tones::sine_q15(ph_) * amp / 32767); ph_ += step; }
+    }
+
+private:
+    uint32_t ph_ = 0;
+};
+
+// PL3b-D11 무엇을 보낼까. AEC = 평소(에코 제거 결과). 나머지는 사람이 시리얼로 켜는 진단용이고 통화가 끝나면 AEC 로 돌아간다.
+//   MIC = 에코 제거를 거치지 않은 마이크, REF = 스피커 되먹임 칸(이 기기가 내고 있는 소리 — 앰프를 꺼도 잡힌다), TONE = 1kHz 시험음.
+enum class Src : uint8_t { AEC, MIC, REF, TONE };
+inline const char *src_name(Src s)
+{
+    switch (s) { case Src::AEC: return "aec"; case Src::MIC: return "mic"; case Src::REF: return "ref"; case Src::TONE: return "tone"; }
+    return "?";
+}
+inline bool parse_src(const char *text, Src *out)
+{
+    static const Src ALL[] = {Src::AEC, Src::MIC, Src::REF, Src::TONE};
+    for (Src s : ALL) if (text && !strcmp(text, src_name(s))) { *out = s; return true; }
+    return false;
+}
+// 보낼 한 묶음(BLOCK 표본)을 out 에 만든다. 마이크에서 온 소리에는 TX_GAIN 을 곱하고(넘치면 자른다), 시험음은 정한 크기 그대로.
+inline void pick(Src src, const int16_t *clean, const int16_t *mic, const int16_t *ref, TestTone &tone, int16_t *out)
+{
+    if (src == Src::TONE) { tone.render(out, BLOCK, TEST_AMP); return; }
+    memcpy(out, src == Src::MIC ? mic : src == Src::REF ? ref : clean, BLOCK * sizeof(int16_t));
+    pcm::gain_q8(out, BLOCK, TX_GAIN_Q8);
+}
+
+// 16ms 묶음을 20ms 프레임으로 다시 자르고 순번을 매긴다. 통화마다 start() 로 비운다(순번 0 부터, 앞 DISCARD_BLOCKS 묶음은 버린다 — PL3b-D9).
+class Uplink {
+public:
+    void start() { len_ = 0; seq_ = 0; skip_ = DISCARD_BLOCKS; discarded_ = 0; }
+    void push(const int16_t *block)                 // BLOCK 표본
+    {
+        if (skip_ > 0) { --skip_; ++discarded_; return; }
+        if (len_ + BLOCK > CAP) { memmove(buf_, buf_ + BLOCK, (size_t)(len_ - BLOCK) * sizeof(int16_t)); len_ -= BLOCK; }   // pop 을 안 부르면 오래된 것부터 버린다
+        memcpy(buf_ + len_, block, BLOCK * sizeof(int16_t));
+        len_ += BLOCK;
+    }
+    bool pop(int16_t *frame, uint16_t *seq)         // 한 프레임(FRAME 표본)이 찼으면 꺼낸다
+    {
+        if (len_ < FRAME) return false;
+        memcpy(frame, buf_, FRAME * sizeof(int16_t));
+        len_ -= FRAME;
+        memmove(buf_, buf_ + FRAME, (size_t)len_ * sizeof(int16_t));
+        *seq = seq_++;
+        return true;
+    }
+    uint32_t discarded() const { return discarded_; }
+    int pending() const { return len_; }
+
+private:
+    static constexpr int CAP = 4 * BLOCK;
+    int16_t buf_[CAP];
+    int len_ = 0, skip_ = 0;
+    uint16_t seq_ = 0;
+    uint32_t discarded_ = 0;
+};
+
+// PL3b-D8 보내기 대기열. 넣는 쪽(캡처 태스크)과 꺼내는 쪽(송신 태스크)이 다르다 — Lock 으로 감싼다(기기: FreeRTOS 뮤텍스, 시험: 빈 잠금).
+//   음성은 밀리면 버린다(쌓지 않는다): 가득 차면 가장 오래된 것을 버리고 새것을 넣는다. 꺼낼 때 TX_STALE_MS 넘게 묵은 것은 건너뛴다.
+struct TxFrame { uint32_t ms; uint16_t len; uint8_t data[MAX_TX_FRAME]; };
+inline bool stale(uint32_t now_ms, uint32_t stamp_ms) { return (uint32_t)(now_ms - stamp_ms) > TX_STALE_MS; }   // 시계가 한 바퀴 돌아도 맞는다
+template <class Lock>
+class TxFifo {
+public:
+    explicit TxFifo(Lock &lock) : lock_(lock) {}
+    bool put(const uint8_t *frame, size_t len, uint32_t now_ms)     // 규격 밖(빈 것·너무 큰 것)은 넣지 않고 false
+    {
+        if (!frame || len == 0 || len > MAX_TX_FRAME) return false;
+        lock_.lock();
+        if (count_ == TX_QUEUE) { head_ = (head_ + 1) % TX_QUEUE; --count_; ++dropped_; }
+        TxFrame &f = q_[(head_ + count_) % TX_QUEUE];
+        f.ms = now_ms; f.len = (uint16_t)len; memcpy(f.data, frame, len);
+        ++count_;
+        lock_.unlock();
+        return true;
+    }
+    bool take(TxFrame *out, uint32_t now_ms)
+    {
+        bool got = false;
+        lock_.lock();
+        while (count_ > 0 && !got) {
+            const TxFrame &f = q_[head_];
+            if (stale(now_ms, f.ms)) ++dropped_;
+            else { *out = f; got = true; }
+            head_ = (head_ + 1) % TX_QUEUE; --count_;
+        }
+        lock_.unlock();
+        return got;
+    }
+    int flush()                                     // 전부 버린다(통화 시작·끝). 돌려주는 값 = 버린 수(dropped 에는 세지 않는다)
+    {
+        lock_.lock();
+        const int n = count_;
+        count_ = 0; head_ = 0;
+        lock_.unlock();
+        return n;
+    }
+    int size() const { return count_; }
+    uint32_t dropped() const { return dropped_; }   // 밀려서 버린 프레임(가득 참 + 묵음). 통화마다 clear_dropped
+    void clear_dropped() { lock_.lock(); dropped_ = 0; lock_.unlock(); }
+
+private:
+    Lock &lock_;
+    TxFrame q_[TX_QUEUE];
+    int head_ = 0, count_ = 0;
+    uint32_t dropped_ = 0;
+};
+
+// ── 받는 길 ──────────────────────────────────────────────────────────────────
+// PL3b-D7 빠진 프레임 메우기. esp_audio_codec 2.5.0 의 Opus 해제기는 손실 은닉을 해 주지 않는다(계획서의 근거) → 직전 프레임을 줄여 가며 되풀이한다.
+//   lost    직전 소리를 거울처럼 되짚으며(끝 표본에서 거꾸로 — 이어 붙는 자리에 계단이 없다) MAX_CONCEAL 프레임(60ms)에 걸쳐 0 까지 줄인다
+//   silence 채우는 중(무음). 직전에 소리가 남아 있었으면 5ms 에 걸쳐 0 으로(그때만 true)
+//   good    정상 프레임. 직전 정상 프레임의 바로 다음 순번이 아니면(은닉·무음·건너뜀 뒤) 앞 5ms 를 직전 소리에서 넘겨 받는다(딸깍 방지)
+class Concealer {
+public:
+    void reset() { have_ = false; good_ = false; left_ = 0; }
+    void good(int16_t *frame, uint16_t seq)
+    {
+        if (!(good_ && (uint16_t)(seq - seq_) == 1)) {
+            const int32_t g = level();
+            for (int i = 0; i < FADE; ++i) {
+                const int32_t tail = have_ && g ? walk() * g / Q : 0;
+                frame[i] = (int16_t)((tail * (FADE - i) + (int32_t)frame[i] * i) / FADE);
+            }
+        }
+        memcpy(last_, frame, sizeof last_);
+        have_ = true; good_ = true; seq_ = seq; left_ = jitterbuf::MAX_CONCEAL; pos_ = FRAME - 2; dir_ = -1;
+    }
+    void lost(int16_t *out)
+    {
+        const int32_t g0 = level();
+        if (left_ > 0) --left_;
+        const int32_t g1 = level();
+        for (int i = 0; i < FRAME; ++i) {
+            const int32_t g = g0 + (g1 - g0) * (i + 1) / FRAME;
+            out[i] = have_ && g0 ? (int16_t)(walk() * g / Q) : 0;
+        }
+        good_ = false;
+    }
+    bool silence(int16_t *out)                      // 돌려주는 값 = 줄어드는 꼬리를 썼나(false 면 out 은 전부 0)
+    {
+        const int32_t g = level();
+        const bool tail = have_ && g;
+        memset(out, 0, FRAME * sizeof(int16_t));
+        if (tail) for (int i = 0; i < FADE; ++i) out[i] = (int16_t)(walk() * (g * (FADE - 1 - i) / FADE) / Q);
+        left_ = 0; good_ = false;
+        return tail;
+    }
+
+private:
+    static constexpr int32_t Q = 32768;
+    int32_t level() const { return Q * left_ / jitterbuf::MAX_CONCEAL; }
+    int32_t walk()                                  // 직전 프레임을 끝에서 거꾸로, 처음에 닿으면 다시 앞으로
+    {
+        const int32_t v = last_[pos_];
+        pos_ += dir_;
+        if (pos_ < 0) { pos_ = 1; dir_ = 1; }
+        else if (pos_ >= FRAME) { pos_ = FRAME - 2; dir_ = -1; }
+        return v;
+    }
+    int16_t last_[FRAME];
+    bool have_ = false, good_ = false;
+    uint16_t seq_ = 0;
+    int left_ = 0, pos_ = 0, dir_ = -1;
+};
+
+// 받는 쪽 통계 — 통화 하나(start 부터). STAT 에 싣는다.
+struct RxStats {
+    uint32_t frames = 0;        // 교환기에서 받은 프레임
+    uint32_t bad = 0;           // 규격 밖(짧거나 긴 것)
+    uint32_t played = 0;        // 해제해서 낸 프레임
+    uint32_t concealed = 0;     // 메운 프레임(빠졌거나 해제 실패)
+    uint32_t dec_fail = 0;      // 해제 실패
+    uint32_t under = 0;         // 출력 태스크가 가져가려는데 링버퍼가 비어 있던 횟수(해제 태스크가 굶었다)
+    uint32_t play_rms = 0;      // 최근 1초 동안 낸 목소리의 세기
+    int depth = 0, target = 0;  // 지터 버퍼에 든 프레임 / 목표
+    jitterbuf::Stats jb;
+};
+
+// PL3b-D5·D6 받는 길 전체. 세 태스크가 부른다:
+//   push  웹소켓 태스크 — 받은 프레임을 지터 버퍼에 넣는다(복사만)
+//   pump  해제 태스크   — 링버퍼가 한 묶음(BLOCK)보다 적으면 프레임을 꺼내 PCM 으로 만들어 넣는다. 출력 태스크가 한 묶음 가져갈 때마다,
+//                        그리고 프레임이 도착할 때마다 깨운다(want_pump 가 참일 때 — 채우는 중에 프레임이 다 모이면 다음 묶음부터 바로 소리가 난다)
+//   fill  출력 태스크   — 16ms 묶음을 가져간다(멈추지 않는다: 링버퍼에서 복사만)
+// 지터 버퍼는 Lock 으로, 링버퍼는 잠금 없이(쓰는 쪽 = pump, 읽는 쪽 = fill) 지킨다. start(통화가 열림)·stop(끝남)은 app 태스크가 부른다:
+// start 는 지터 버퍼만 그 자리에서 비우고, 은닉·해제기·링버퍼는 주인 태스크가 다음 차례에 비운다(epoch).
+// 닫혀 있는 동안에는 push 가 버리고 pump 가 아무것도 하지 않는다 — 통화 밖에서 온 프레임은 한 표본도 나지 않는다.
+template <class Lock>
+class Receiver {
+public:
+    explicit Receiver(Lock &lock) : lock_(lock) {}
+
+    void start()
+    {
+        lock_.lock();
+        jb_.reset();
+        lock_.unlock();
+        frames_ = 0; bad_ = 0; played_ = 0; concealed_ = 0; dec_fail_ = 0; under_ = 0; play_rms_ = 0;
+        buffering_ = true;
+        epoch_.fetch_add(1);
+        open_ = true;
+    }
+    void stop() { open_ = false; }
+    bool open() const { return open_.load(); }
+
+    void push(const uint8_t *frame, size_t len)     // [순번 2][Opus]
+    {
+        if (!open_.load()) return;
+        uint16_t seq = 0; const uint8_t *opus = nullptr; size_t n = 0;
+        if (!relayproto::unpack_voice(frame, len, &seq, &opus, &n)) { ++bad_; return; }
+        ++frames_;
+        lock_.lock();
+        jb_.push(seq, opus, n);
+        lock_.unlock();
+    }
+
+    bool want_pump() const { return open_.load() && ring_.size() < (size_t)BLOCK; }
+
+    // dec(data, len, pcm) → FRAME 표본을 pcm 에 풀고 true(실패면 false). fresh() → 새 통화의 첫 pump 에서 한 번(해제기를 처음 상태로).
+    // 돌려주는 값 = 만든 프레임 수.
+    template <class Dec, class Fresh>
+    int pump(Dec &&dec, Fresh &&fresh)
+    {
+        if (!open_.load()) return 0;
+        const uint32_t e = epoch_.load();
+        if (e != pump_epoch_) { pump_epoch_ = e; plc_.reset(); rms_ = pcm::Rms(); rms_frames_ = 0; fresh(); }
+        int made = 0;
+        while (ring_.size() < (size_t)BLOCK && made < 4) {
+            lock_.lock();
+            const jitterbuf::Pop p = jb_.pop();
+            buffering_ = jb_.buffering();
+            lock_.unlock();
+            int16_t pcm[FRAME];
+            if (p.kind == jitterbuf::Kind::PLAY && dec(p.data, p.len, pcm)) { plc_.good(pcm, p.seq); ++played_; }
+            else if (p.kind == jitterbuf::Kind::SILENCE) {
+                // 채우는 중(통화 시작·끊긴 뒤): 무음을 링버퍼에 넣지 않는다 — 넣으면 그 무음이 첫 목소리 앞에 줄을 서서 지연이 된다(PL3b-D6).
+                // 비어 있으면 fill 이 0 을 낸다. 직전 소리의 꼬리가 남아 있을 때만 그 5ms 를 넣는다.
+                if (!plc_.silence(pcm)) break;
+            }
+            else { if (p.kind == jitterbuf::Kind::PLAY) ++dec_fail_; plc_.lost(pcm); ++concealed_; }
+            pcm::gain_q8(pcm, FRAME, PLAY_GAIN_Q8);
+            rms_.add(pcm, FRAME);
+            if (++rms_frames_ >= RATE / FRAME) { play_rms_ = rms_.take(); rms_frames_ = 0; }
+            ring_.write(pcm, FRAME);
+            ++made;
+        }
+        return made;
+    }
+
+    // 출력 한 묶음. 돌려주는 값 = 소리가 있나(false 면 out 을 건드리지 않는다).
+    //   열려 있는 동안에는 늘 true(받은 것이 없으면 무음을 낸다 — 출력 태스크가 16ms 박자를 지켜야 pump 가 깨어난다).
+    //   닫힌 뒤의 첫 묶음은 남은 소리를 5ms 에 걸쳐 줄인 것이고(PL3b-D13: 딸깍 방지, 나머지는 버린다) 그다음부터 false.
+    bool fill(int16_t *out, int n)
+    {
+        const bool on = open_.load();
+        const uint32_t e = epoch_.load();
+        if (e != fill_epoch_) { fill_epoch_ = e; ring_.drain(); playing_ = false; hold_ = 0; }   // 새 통화: 앞 통화의 찌꺼기를 버린다(읽는 쪽에서)
+        if (on) {
+            const size_t got = ring_.read(out, (size_t)n);
+            if (got < (size_t)n) {
+                memset(out + got, 0, ((size_t)n - got) * sizeof(int16_t));
+                if (!buffering_.load()) ++under_;
+            }
+            hold_ = out[n - 1]; playing_ = true;
+            return true;
+        }
+        if (!playing_) return false;
+        const int m = n < FADE ? n : FADE;
+        int16_t tail[FADE];
+        const size_t got = ring_.read(tail, (size_t)m);
+        memset(out, 0, (size_t)n * sizeof(int16_t));
+        for (int i = 0; i < m; ++i) {
+            const int16_t s = (size_t)i < got ? tail[i] : hold_;
+            hold_ = s;
+            out[i] = (int16_t)((int32_t)s * (m - 1 - i) / m);
+        }
+        playing_ = false; hold_ = 0;                // 링버퍼에 남은 것은 다음 통화가 열릴 때 버려진다(위의 epoch) — 닫혀 있는 동안에는 아무도 꺼내지 않는다
+        return true;
+    }
+
+    RxStats stats()
+    {
+        RxStats s;
+        s.frames = frames_; s.bad = bad_; s.played = played_; s.concealed = concealed_; s.dec_fail = dec_fail_; s.under = under_; s.play_rms = play_rms_;
+        lock_.lock();
+        s.depth = jb_.depth(); s.target = jb_.target_frames(); s.jb = jb_.stats();
+        lock_.unlock();
+        return s;
+    }
+
+private:
+    Lock &lock_;
+    jitterbuf::Buffer jb_;                          // lock_ 안에서만
+    Concealer plc_;                                 // pump 에서만
+    pcm::Rms rms_;                                  // pump 에서만
+    int rms_frames_ = 0;
+    uint32_t pump_epoch_ = 0;
+    pcm::Ring<RING_CAP> ring_;                      // 쓰기 = pump, 읽기 = fill
+    uint32_t fill_epoch_ = 0;                       // fill 에서만
+    bool playing_ = false;
+    int16_t hold_ = 0;
+    std::atomic<uint32_t> epoch_{0}, frames_{0}, bad_{0}, played_{0}, concealed_{0}, dec_fail_{0}, under_{0}, play_rms_{0};
+    std::atomic<bool> buffering_{true}, open_{false};
+};
+
+// ── 시험음 한 토막, 지연 재기 (PL3b-D12·D14) ─────────────────────────────────
+// 쉼 lead → 1kHz 소리 len → 쉼 tail(표본 수)을 차례로 만든다. 소리의 처음·끝 5ms 는 크기를 옮긴다(딸깍 방지). 끝나면 active 가 거짓.
+class Burst {
+public:
+    Burst(int lead, int len, int tail, int amp) : lead_(lead), len_(len), tail_(tail), amp_(amp) {}
+    void start() { pos_ = 0; active_ = true; tone_.reset(); }
+    void stop() { active_ = false; }
+    bool active() const { return active_; }
+    // 한 묶음(n 표본)을 만든다. 돌려주는 값 = 이 묶음에서 소리가 시작한 표본 위치(0..n-1), 아니면 -1.
+    int render(int16_t *out, int n)
+    {
+        int at = -1;
+        for (int i = 0; i < n; ++i, ++pos_) {
+            const int k = pos_ - lead_;                                      // 소리 안에서의 위치
+            int16_t v = 0;
+            if (active_ && k >= 0 && k < len_) {
+                if (k == 0) at = i;
+                tone_.render(&v, 1, amp_);
+                const int edge = k < FADE ? k : len_ - 1 - k < FADE ? len_ - 1 - k : FADE;
+                v = (int16_t)((int32_t)v * edge / FADE);
+            }
+            out[i] = v;
+        }
+        if (pos_ >= lead_ + len_ + tail_) active_ = false;
+        return at;
+    }
+
+private:
+    TestTone tone_;
+    int lead_, len_, tail_, amp_, pos_ = 0;
+    bool active_ = false;
+};
+// 지연 재기: 재는 쪽이 마이크 대신 "쉼 0.4초 → 삑 0.1초 → 쉼 0.5초"를 보내고, 상대가 돌려보낸 소리에서 삑의 시작(onset)을 찾아 걸린 시간을 잰다.
+// 상대는 vsrc ref(스피커 되먹임 — 앰프를 꺼도 된다) 또는 vsrc mic(스피커 → 공기 → 마이크)로 둔다.
+struct PingGen : Burst { PingGen() : Burst(PING_LEAD, PING_BURST, PING_TAIL, PING_AMP) {} };
+// 혼자 하는 점검의 시험음(통화 밖에서 이 기기의 스피커로 0.64초)
+struct SelfTestTone : Burst { SelfTestTone() : Burst(0, SELFTEST_TONE, 0, TEST_AMP) {} };
+// 받은 PCM 에서 |x| 가 threshold 이상인 첫 표본의 위치(없으면 -1).
+inline int onset(const int16_t *pcm, int n, int threshold)
+{
+    for (int i = 0; i < n; ++i) if (pcm[i] >= threshold || pcm[i] <= -threshold) return i;
+    return -1;
+}
+
+}  // namespace voicecore
```

- [x] **Step 3: 지연 시뮬레이션의 숫자를 보고에 적는다** (계획서 "지연 예산" 표의 근거 — 같은 숫자가 나와야 한다)

```bash
/tmp/t_p3b_voice_core --info | grep -v "^  ok" | grep -v "^$"
```
Expected:
```
  stall 60 ms: delay after recovery max 105 ms, concealed 1, dropped 0, underruns 1
  stall 150 ms: delay after recovery max 129 ms, concealed 3, dropped 2, underruns 1
  stall 300 ms: delay after recovery max 133 ms, concealed 3, dropped 9, underruns 1
  stall 600 ms: delay after recovery max 137 ms, concealed 3, dropped 24, underruns 1
  stall 1500 ms: delay after recovery max 113 ms, concealed 3, dropped 70, underruns 1
  stall 4000 ms: delay after recovery max 125 ms, concealed 3, dropped 195, underruns 1
-- delay simulation (ms from a sample entering the microphone to the output task taking it; add DMA write-ahead + codec/AEC internals) --
  LAN 3-7 ms, 10 min             min  85  median  85  p95  85  max  85   concealed 0  dropped 0  underruns 0  jb depth/target 1/2
  sender 200 ppm slow, 1 h       min  54  median  70  p95  82  max  86   concealed 36  dropped 0  underruns 36  jb depth/target 0/2
  sender 200 ppm fast, 1 h       min  63  median  98  p95 120  max 126   concealed 2  dropped 36  underruns 2  jb depth/target 2/2
  tunnel-like 45-75 ms, 10 min   min 133  median 133  p95 133  max 133   concealed 0  dropped 0  underruns 0  jb depth/target 0/2
  LAN, start phase swept         median delay over 64 runs: min 67  mean 75  max 84
117 ok, 0 failed
```
읽는 법: 망이 60ms~4초 멈췄다 풀려도 2초 뒤의 지연은 140ms 아래로 돌아온다(쌓이지 않는다 — 몰려온 것은 버린다). 두 기기의 시계가 0.02% 달라도 100초에 한 프레임을 메우거나 버릴 뿐이다. 여기의 `underruns` 는 "지터 버퍼가 바닥났다"는 셈이고, 기기 `STAT` 의 `v_under`(출력 태스크가 가져가려는데 해제한 소리가 없었다 = 해제 태스크가 굶었다)와는 다른 것이다 — 이 시뮬레이션 전부에서 뒤의 것은 0 이다.

- [x] **Step 4: 커밋**

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/main/voice_core.h firmware/tab5_phone/tools/test_voice_core.cpp && git commit -m "feat(phone): 목소리 판단 헤더 voice_core.h — 켜고 끄기(받기 전에는 안 열린다)·16ms↔20ms·밀리면 버리기·빠진 프레임 메우기·겹치는 순간·시험음과 지연 재기(맥 시험 117)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: 목소리 모듈 `voice` 와 통화 흐름에 잇기 + 도구 시험

**Files:**
- Create: `firmware/tab5_phone/main/voice.h`, `main/voice.cpp`
- Modify: `firmware/tab5_phone/main/CMakeLists.txt`(소스 한 줄), `main/app.h`, `main/app.cpp`, `main/phone_main.cpp`
- Test: `firmware/tab5_phone/tools/test_phone_ctl.py` (추가만)

**Interfaces:**
- Consumes: Task 3 의 `audioout::read_mic`·`add_source`·`wake`·`fail`·`amp`·`amp_is_on`·`late_blocks`·`read_errors`, Task 5 의 `voicecore::*`, Task 1 의 `relaylink::send_voice`(잠금), 기존 `relaylink::set_voice_sink`, esp-sr 2.5.5(`aec_create`·`aec_process`), esp_audio_codec 2.5.0(`esp_opus_enc_*`·`esp_opus_dec_*`).
- Produces:
  - `namespace voice`: `bool start()`, `bool set(bool on)`(상태가 실제로 바뀌었으면 true), `bool on()`, `bool set_source(voicecore::Src)`, `bool ping()`, `bool selftest(SelfTest *out)`, `void print_pending()`, `void stat(Stat *out)`
  - 시리얼 줄: `VOICE ready chunk=256 opus_in=640 opus_out_max=160 bitrate=24000 aec=VOIP_HIGH_PERF`(부팅 — 숫자는 Phase 0 로그 `PROBE AUDIO chunk=256 opus_in=640 opus_out_max=160 … aec_mode=VOIP_HIGH_PERF` 와 같은 호출이 돌려준 값), `VOICE on` / `VOICE off`, `VPING rt_ms=<n>` / `VPING timeout`
  - 시리얼 명령: `quiet on|off` → `OK quiet on amp=0` / `OK quiet off amp=1`(실패 `ERR quiet …`), `selftest` → `SELFTEST amp=<0|1> mic=<n> ref=<n> aec=<n> ok=<0|1>` 또는 `SELFTEST FAIL busy-or-no-audio`, `vsrc <aec|mic|ref|tone>` → `OK vsrc <이름>` / `ERR vsrc`, `vping` → `OK vping` / `ERR vping`
  - `STAT` 줄: 앞의 칸은 그대로, 끝에 ` amp= aud_late= voice= vsrc= v_cap= v_tx= v_txdrop= v_txfail= v_rx= v_play= v_lost= v_drop= v_under= v_jb= v_aec_us= v_enc_us= v_dec_us= v_mic= v_ref= v_aec= v_spk= v_rderr= stk_vcap= stk_vrx= stk_vtx=`
- 커밋 둘. 첫 커밋(모듈만)은 빌드되고 바이너리 크기가 그대로다(사본: 0x244660 — 컴파일되지만 아직 부르는 곳이 없다). 둘째 커밋(연결)에서 **0x287720**.
- `T6-03`(app·phone_main)은 한 커밋이다.

- [x] **Step 1: 목소리 모듈 패치를 적용하고 초안과 대조한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft && git apply "$D/patches/T6-01-voice.patch" && for f in voice.h voice.cpp CMakeLists.txt; do cmp "$D/files/firmware/tab5_phone/main/$f" "firmware/tab5_phone/main/$f" && echo "같음: $f"; done; git diff --numstat -- firmware/tab5_phone/main/CMakeLists.txt
```
Expected: `같음:` 3줄, `1	0	firmware/tab5_phone/main/CMakeLists.txt`

```diff
--- /dev/null
+++ b/firmware/tab5_phone/main/voice.h
@@ -0,0 +1,44 @@
+// voice.h — 목소리 통화 경로(기기 전용). 판단은 voice_core.h(맥 시험)에 있고, 여기는 그것을 태스크·코덱·교환기에 잇기만 한다.
+// 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md PL3b-D5~D16.
+//   보내는 길(캡처 태스크): 마이크 4칸 읽기 → 에코 제거(esp-sr) → Opus 압축 → 프레임 포장 → 보내기 대기열 → (송신 태스크) 교환기
+//   받는 길: (웹소켓 태스크) 프레임 → 지터 버퍼 → (해제 태스크) Opus 해제·메우기 → 링버퍼 → (출력 태스크) audio_out 의 둘째 공급자
+//   통화 밖에서는 세 태스크가 모두 잠들어 있고 마이크를 읽지 않는다(PH-D6: 받기 전에는 마이크가 열리지 않는다).
+#pragma once
+#include <cstdint>
+
+#include "voice_core.h"
+
+namespace voice {
+
+// 부팅 때 한 번(audioout::start·toneout::start 뒤, app::start 앞). 에코 제거·Opus·버퍼를 만들고 태스크 셋을 띄운다(잠든 채),
+// audio_out 에 공급자로, relay_link 에 음성 받는 함수로 등록한다. 소리 장치가 없으면(audioout::ready 가 거짓) 아무것도 하지 않고 false.
+// 준비가 실패하면 audioout::fail("voice") 로 알리고 false — 전화기는 소리 없이 계속 돈다.
+bool start();
+
+// 통화가 열렸다(true) / 끝났다(false). app 태스크가 상태 기계의 A_VOICE_START·A_VOICE_STOP 에서 부른다. 바로 돌아온다(기다리지 않는다).
+// 돌려주는 값 = 상태가 실제로 바뀌었나(app 이 "VOICE on|off" 줄을 찍을지). 준비가 안 됐으면 늘 false.
+bool set(bool on);
+bool on();
+
+// ── 진단(시리얼 명령 — 사람이 켠다) ──
+bool set_source(voicecore::Src s);          // 통화 중에만: 무엇을 보낼지(aec = 평소). 통화가 끝나면 aec 로 돌아간다. 통화 중이 아니면 false
+bool ping();                                // 통화 중에만: 지연 재기 한 번. 결과는 print_pending 이 "VPING rt_ms=…" 또는 "VPING timeout" 으로 찍는다
+struct SelfTest { uint32_t mic, ref, aec; };
+bool selftest(SelfTest *out);               // 통화 밖에서만: 시험음을 0.64초 내며 마이크 칸(mic)·되먹임 칸(ref)·에코 제거 뒤(aec)의 세기를 잰다. 약 0.7초 기다린다
+
+void print_pending();                       // app 태스크가 20ms 바퀴마다: 지연 재기의 결과 줄을 찍는다(다른 태스크는 찍지 않는다)
+
+// STAT 줄의 목소리 칸. 셈과 시간은 "이번(또는 직전) 통화"의 것이다 — 통화가 열릴 때 0 으로 돌아간다.
+struct Stat {
+    bool ready, on; voicecore::Src src;
+    uint32_t cap;                           // 마이크에서 읽은 묶음 수(부팅 뒤 누적 — 통화 밖에서는 늘지 않아야 한다)
+    uint32_t tx, tx_drop, tx_fail;          // 보낸 프레임 / 밀려서 버린 프레임 / 보내기 실패
+    voicecore::RxStats rx;
+    uint32_t aec_us, aec_max, enc_us, enc_max, dec_us, dec_max;   // 묶음(16ms)·프레임(20ms) 하나를 처리한 시간의 평균·최대(µs)
+    uint32_t mic_rms, ref_rms, aec_rms;     // 최근 1초의 세기: 마이크 칸 / 되먹임 칸 / 에코 제거 뒤
+    uint32_t rd_err;                        // 마이크 읽기 실패
+    unsigned stk_cap, stk_rx, stk_tx;       // 태스크 스택 최저 여유(바이트)
+};
+void stat(Stat *out);
+
+}  // namespace voice
--- /dev/null
+++ b/firmware/tab5_phone/main/voice.cpp
@@ -0,0 +1,387 @@
+// voice.cpp — 목소리 통화 경로. 설명은 voice.h, 판단 로직은 voice_core.h(맥 시험 tools/test_voice_core.cpp).
+//
+// 근거(줄 단위 대조, 사용자 지시 2026-10-03): 이 기기에서 마이크·에코 제거·Opus 를 실제로 돌린 Phase 0 시험 펌웨어
+//   firmware/tab5_phone_probe/main/probe_main.cpp 의 audio_task. 그대로 가져온 것: aec_create(16000, 4, 1, AEC_MODE_VOIP_HIGH_PERF) 와
+//   묶음 크기 확인(aec_get_chunksize = 256), aec_process(aec, mic, ref, clean), 에코 제거 버퍼의 16바이트 정렬 내부 메모리,
+//   Opus 압축기 설정(16kHz 모노, 24000bps, 20ms, VOIP, 복잡도 0, FEC·DTX·VBR 끔)과 esp_opus_enc_open → get_frame_size(입력 640바이트 확인)
+//   → esp_opus_enc_process, 해제기 설정(16kHz 모노 20ms)과 esp_opus_dec_open → esp_opus_dec_decode(출력 버퍼 640바이트),
+//   Opus 를 부르는 태스크의 스택 48KB(내부 메모리), 그리고 에코 제거·Opus 를 "그 48KB 태스크 안에서 만든다"는 것(codec_setup — 부팅 태스크의
+//   스택은 8KB 이고 aec_create·esp_opus_*_open 이 스택을 얼마나 쓰는지 모른다. 거기서 넘치면 boot_guard 가 세지 못하는 재부팅 고리가 된다).
+//   AEC_MODE_FD_HIGH_PERF 는 결과가 깨져 쓰지 않는다(Phase 0).
+//   시험 펌웨어와 다른 점(전부 계획서 "확인하지 못한 것"에 있다):
+//   PL3b-D15 태스크를 셋으로 나눴다(시험 펌웨어는 한 태스크가 읽기·에코 제거·압축·해제·쓰기를 다 했다):
+//          v_cap(코어 1, 우선순위 7, 스택 48KB 내부 메모리) 마이크 → 에코 제거 → 압축 / v_rx(코어 1, 7, 48KB PSRAM) 해제 / v_tx(코어 0, 5, 8KB 내부) 교환기로 보내기.
+//          이유: 재생은 3a 의 출력 태스크가 끌어가는 구조이고, 보내기는 Wi-Fi 가 막히면 2초까지 멈춘다(그동안 마이크·재생이 멎으면 안 된다).
+//          스택 48KB 는 시험 펌웨어가 세 가지(에코 제거·압축·해제)를 차례로 부르던 한 태스크의 값이다 — 둘로 나눈 각각에 그대로 준다.
+//          무거운 쪽(v_cap — 묶음마다 약 5ms)은 시험 펌웨어처럼 내부 메모리 스택이다(Phase 0 의 부하 30~37% 가 그 조건에서 잰 값).
+//          가벼운 쪽(v_rx — 프레임마다 약 0.3ms)의 스택은 PSRAM 에 둔다: 내부 메모리가 빠듯하다(계획서 "CPU·메모리 예산" — 에코 제거·Opus 가
+//          내부 메모리를 65~80KB 쓰는 것으로 역산된다). 이 설정(CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=y, XIP from PSRAM)에서 PSRAM 스택은
+//          쓸 수 있지만 이 기기에서 돌려 본 적은 없다. 탈출구: RX_STACK_CAPS 를 MALLOC_CAP_INTERNAL 로(그리고 RX_STACK 을 24KB 로 — 참고 구현
+//          xiaozhi 는 Opus 압축·해제를 24KB 태스크 하나로 돌린다). 내부 메모리가 더 모자라면 CAP 쪽도 PSRAM 으로(그때는 v_aec_us·v_enc_us 를 Phase 0 과 대조).
+//   PL3b-D7 해제기의 손실 은닉을 쓰지 않는다(esp_opus_dec_decode 는 frame_recover 를 읽지 않고 빈 입력을 거절한다) → voicecore::Concealer.
+//          통화가 열릴 때 해제기는 esp_opus_dec_reset 으로 처음 상태로(참고 구현 xiaozhi 가 같은 호출을 쓴다). 압축기는 다시 맞추지 않는다.
+//   PL3b-D11·D12·D14 진단: 보낼 소리 고르기(vsrc), 지연 재기(vping), 혼자 하는 점검(selftest). 목소리 PCM·압축 프레임을 시리얼·파일로
+//          내보내는 기능은 없다 — 세기(RMS) 숫자와 셈만 나간다.
+#include "voice.h"
+
+#include <atomic>
+#include <cstdio>
+#include <cstring>
+#include <new>
+
+#include "esp_aec.h"
+#include "esp_audio_dec.h"
+#include "esp_audio_enc.h"
+#include "esp_heap_caps.h"
+#include "esp_log.h"
+#include "esp_opus_dec.h"
+#include "esp_opus_enc.h"
+#include "esp_timer.h"
+#include "freertos/FreeRTOS.h"
+#include "freertos/idf_additions.h"
+#include "freertos/semphr.h"
+#include "freertos/task.h"
+
+#include "audio_out.h"
+#include "relay_link.h"
+
+static_assert(voicecore::RATE == audioout::RATE && voicecore::BLOCK == audioout::BLOCK && voicecore::IN_SLOTS == audioout::IN_SLOTS,
+              "voice_core.h and audio_out.h must agree on rate, block size and mic slots");
+
+namespace voice {
+namespace {
+
+using namespace voicecore;
+
+const char *TAG = "voice";
+
+constexpr int AEC_FILTER_LEN = 4;                               // esp-sr 권장값(시험 펌웨어와 같다)
+constexpr aec_mode_t AEC_MODE = AEC_MODE_VOIP_HIGH_PERF;        // Phase 0: 잔향 감소 29~42dB, 묶음 256표본. FD_HIGH_PERF 는 쓰지 않는다
+constexpr int OPUS_BITRATE = 24000;                             // PH-D5
+constexpr int OPUS_COMPLEXITY = 0;                              // Phase 0: 5 는 부하 53.6% 로 초과, 0 은 30~37%
+constexpr int CAP_CORE = 1, CAP_PRIO = 7, CAP_STACK = 48 * 1024;   // PL3b-D15
+constexpr int RX_CORE = 1, RX_PRIO = 7, RX_STACK = 48 * 1024;
+constexpr UBaseType_t RX_STACK_CAPS = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;   // 해제 태스크의 스택은 PSRAM(내부 메모리 48KB 를 아낀다)
+constexpr int TX_CORE = 0, TX_PRIO = 5, TX_STACK = 8 * 1024;       // 보내기 실패 때 교환기 연결의 끊김 핸들러가 이 스택에서 돈다(relay 태스크와 같은 크기)
+constexpr uint32_t PING_TIMEOUT_MS = 3000;
+constexpr uint32_t SELFTEST_HOLD_MS = 720;                         // 시험음(0.64초)이 스스로 줄어 끝난 뒤에 점검을 닫는다(중간에 끊으면 딸깍 소리)
+
+enum class Mode : uint8_t { OFF, CALL, TEST };
+struct Mutex {                                                  // voice_core.h 의 Lock: FreeRTOS 뮤텍스
+    SemaphoreHandle_t h = nullptr;
+    void lock() { xSemaphoreTake(h, portMAX_DELAY); }
+    void unlock() { xSemaphoreGive(h); }
+};
+struct Lap {                                                    // 처리 시간(통화마다 reset). 쓰는 태스크 하나, 읽는 쪽은 STAT
+    std::atomic<uint32_t> n{0}, sum{0}, max{0};
+    void add(int64_t us) { const uint32_t u = (uint32_t)us; ++n; sum += u; if (u > max.load()) max = u; }   // 합계는 µs 로 약 71분에 넘친다 → 통화마다 reset, 넘치기 전에 반으로
+    uint32_t avg() const { const uint32_t k = n.load(); return k ? sum.load() / k : 0; }
+    void reset() { n = 0; sum = 0; max = 0; }
+    void halve_if_big() { if (sum.load() > 0x7FFFFFFFu) { sum = sum.load() / 2; n = n.load() / 2; } }
+};
+
+std::atomic<bool> s_ready{false};
+std::atomic<Mode> s_mode{Mode::OFF};
+std::atomic<Src> s_src{Src::AEC};
+Mutex s_jb_lock, s_txq_lock;
+Receiver<Mutex> *s_rx = nullptr;                                // 지터 버퍼(약 13KB)가 들어 있어 PSRAM 에 둔다
+TxFifo<Mutex> *s_txq = nullptr;
+aec_handle_t *s_aec = nullptr;
+void *s_enc = nullptr, *s_dec = nullptr;
+int s_enc_out = 0;                                              // 압축기가 권하는 출력 버퍼 크기
+int16_t *s_slots = nullptr, *s_mic = nullptr, *s_ref = nullptr, *s_clean = nullptr;   // 캡처 태스크에서만
+TaskHandle_t s_cap_task = nullptr, s_rx_task = nullptr, s_tx_task = nullptr;
+SemaphoreHandle_t s_codec_done = nullptr;                       // 캡처 태스크가 codec_setup 을 끝냈다(start 가 기다린다)
+const char *s_codec_fail = nullptr;                             // 실패한 단계(성공이면 nullptr)
+constexpr uint32_t CODEC_SETUP_WAIT_MS = 10000;
+
+std::atomic<uint32_t> s_cap_blocks{0}, s_tx_ok{0}, s_tx_fail{0}, s_enc_fail{0}, s_mic_rms{0}, s_ref_rms{0}, s_aec_rms{0};
+Lap s_t_aec, s_t_enc, s_t_dec;
+
+// 지연 재기(PL3b-D12): 캡처 태스크가 삑을 보내며 시각을 적고, 해제 태스크가 돌아온 삑을 찾고, app 태스크가 결과를 찍는다
+std::atomic<bool> s_ping_req{false}, s_ping_armed{false};
+std::atomic<uint32_t> s_ping_t0_ms{0};                          // 삑의 첫 표본이 "마이크에 들어온" 시각(ms)
+std::atomic<int32_t> s_ping_result{-1};                        // 찍을 결과: -1 없음, -2 시간 초과, 그 밖 = ms
+
+// 혼자 하는 점검(PL3b-D14)
+std::atomic<bool> s_test_start{false}, s_test_done{false};
+SelfTestTone s_test_tone;                                       // 출력 태스크에서만
+SelfTest s_test_result;
+
+int16_t *alloc16(int samples) { return (int16_t *)heap_caps_aligned_calloc(16, (size_t)samples, sizeof(int16_t), MALLOC_CAP_INTERNAL); }
+uint32_t now_ms() { return (uint32_t)(esp_timer_get_time() / 1000); }
+
+// ── 받는 길 ──────────────────────────────────────────────────────────────────
+// audio_out 의 공급자(출력 태스크 안 — 기다리지 않는다: 링버퍼 복사와 통지뿐)
+bool fill(int16_t *out, int n)
+{
+    if (s_test_start.exchange(false)) s_test_tone.start();
+    if (s_test_tone.active()) {
+        if (s_mode.load() != Mode::TEST) s_test_tone.stop();    // 점검 중에 통화가 열렸다 — 시험음을 끊는다
+        else { s_test_tone.render(out, n); return true; }
+    }
+    const bool sound = s_rx->fill(out, n);
+    if (s_rx->want_pump()) xTaskNotifyGive(s_rx_task);          // 링버퍼가 한 묶음보다 적다 → 해제 태스크를 깨운다
+    return sound;
+}
+
+// relay_link 가 받은 음성 프레임을 넘긴다(웹소켓 태스크 — 복사만 하고 바로 돌아온다). 통화 밖에서 온 것은 Receiver 가 버린다
+void on_frame(const uint8_t *frame, size_t len)
+{
+    s_rx->push(frame, len);
+    if (s_rx->want_pump()) xTaskNotifyGive(s_rx_task);
+}
+
+bool decode(const uint8_t *data, size_t len, int16_t *pcm)     // 시험 펌웨어의 esp_opus_dec_decode 호출 그대로
+{
+    esp_audio_dec_in_raw_t raw = {}; raw.buffer = (uint8_t *)data; raw.len = (uint32_t)len;
+    esp_audio_dec_out_frame_t frame = {}; frame.buffer = (uint8_t *)pcm; frame.len = FRAME * 2;
+    esp_audio_dec_info_t info = {};
+    const int64_t t0 = esp_timer_get_time();
+    const bool ok = esp_opus_dec_decode(s_dec, &raw, &frame, &info) == ESP_AUDIO_ERR_OK && frame.decoded_size == (uint32_t)FRAME * 2;
+    s_t_dec.add(esp_timer_get_time() - t0);
+    if (ok && s_ping_armed.load()) {                            // 돌아온 삑의 시작을 찾는다(해제한 때 = 링버퍼에 넣는 때. 스피커로는 그 뒤 링버퍼 + DMA 만큼 더 걸린다)
+        const int at = onset(pcm, FRAME, PING_DETECT);
+        if (at >= 0 && s_ping_armed.exchange(false)) s_ping_result = (int32_t)(now_ms() - s_ping_t0_ms.load());
+    }
+    return ok;
+}
+
+void rx_task(void *)
+{
+    while (true) {
+        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
+        s_rx->pump(decode, [] { esp_opus_dec_reset(s_dec); });  // 새 통화의 첫 차례에 해제기를 처음 상태로(PL3b-D7)
+        s_t_dec.halve_if_big();
+    }
+}
+
+// ── 보내는 길 ────────────────────────────────────────────────────────────────
+void tx_task(void *)
+{
+    static TxFrame f;                                           // 이 태스크에서만 쓴다
+    while (true) {
+        if (!s_txq->take(&f, now_ms())) { ulTaskNotifyTake(pdTRUE, portMAX_DELAY); continue; }
+        if (s_mode.load() != Mode::CALL) continue;              // 통화가 끝났다 — 남은 것은 보내지 않는다
+        if (relaylink::send_voice(f.data, f.len)) ++s_tx_ok;
+        else { ++s_tx_fail; s_txq->flush(); }                   // 못 보냈다(끊김·막힘) → 쌓인 것도 버린다(묵은 소리를 몰아 보내지 않는다)
+    }
+}
+
+// 에코 제거·Opus 압축기·해제기와 캡처 버퍼를 만든다. 캡처 태스크(스택 48KB)의 첫 일로 한 번 — 시험 펌웨어 audio_task 의 앞부분 그대로.
+// 실패하면 그 단계의 이름(성공이면 nullptr).
+const char *codec_setup()
+{
+    s_aec = aec_create(RATE, AEC_FILTER_LEN, 1, AEC_MODE);
+    if (!s_aec) return "aec";
+    const int chunk = aec_get_chunksize(s_aec);
+    if (chunk != BLOCK) { ESP_LOGE(TAG, "aec chunk %d != %d", chunk, BLOCK); return "aec-chunk"; }
+
+    esp_opus_enc_config_t ecfg = ESP_OPUS_ENC_CONFIG_DEFAULT();
+    ecfg.sample_rate = ESP_AUDIO_SAMPLE_RATE_16K; ecfg.channel = ESP_AUDIO_MONO; ecfg.bitrate = OPUS_BITRATE;
+    ecfg.frame_duration = ESP_OPUS_ENC_FRAME_DURATION_20_MS; ecfg.application_mode = ESP_OPUS_ENC_APPLICATION_VOIP; ecfg.complexity = OPUS_COMPLEXITY;
+    int enc_in = 0;
+    if (esp_opus_enc_open(&ecfg, sizeof ecfg, &s_enc) != ESP_AUDIO_ERR_OK || esp_opus_enc_get_frame_size(s_enc, &enc_in, &s_enc_out) != ESP_AUDIO_ERR_OK) return "opus-enc";
+    if (enc_in != FRAME * 2 || s_enc_out <= 0 || (size_t)s_enc_out + 2 > MAX_TX_FRAME) { ESP_LOGE(TAG, "opus frame %d/%d", enc_in, s_enc_out); return "opus-size"; }
+    esp_opus_dec_cfg_t dcfg = ESP_OPUS_DEC_CONFIG_DEFAULT();
+    dcfg.sample_rate = ESP_AUDIO_SAMPLE_RATE_16K; dcfg.channel = ESP_AUDIO_MONO; dcfg.frame_duration = ESP_OPUS_DEC_FRAME_DURATION_20_MS;
+    if (esp_opus_dec_open(&dcfg, sizeof dcfg, &s_dec) != ESP_AUDIO_ERR_OK) return "opus-dec";
+
+    s_slots = alloc16(BLOCK * IN_SLOTS); s_mic = alloc16(BLOCK); s_ref = alloc16(BLOCK); s_clean = alloc16(BLOCK);
+    if (!s_slots || !s_mic || !s_ref || !s_clean) return "buffers";
+    return nullptr;
+}
+
+void cap_task(void *)
+{
+    s_codec_fail = codec_setup();
+    xSemaphoreGive(s_codec_done);
+    if (s_codec_fail) { vTaskDelete(nullptr); return; }         // 실패: start 가 알리고(AUDIO FAIL voice) 이 태스크는 사라진다
+
+    static int16_t blk[BLOCK], frame[FRAME];
+    static uint8_t pkt[MAX_TX_FRAME];
+    Uplink up; TestTone tone; PingGen ping;
+    pcm::Rms r_mic, r_ref, r_aec, t_mic, t_ref, t_aec;
+    int rms_blocks = 0, test_blocks = 0;
+    Mode was = Mode::OFF;
+    while (true) {
+        const Mode mode = s_mode.load();
+        if (mode == Mode::OFF) { was = Mode::OFF; ulTaskNotifyTake(pdTRUE, portMAX_DELAY); continue; }   // 통화 밖: 마이크를 읽지 않는다(PH-D6)
+        if (mode != was) {                                      // 방금 켜졌다(통화 또는 점검)
+            was = mode;
+            up.start(); tone.reset(); ping.stop();
+            r_mic.take(); r_ref.take(); r_aec.take(); t_mic.take(); t_ref.take(); t_aec.take();
+            rms_blocks = 0; test_blocks = 0;
+        }
+        if (!audioout::read_mic(s_slots, BLOCK)) { vTaskDelay(pdMS_TO_TICKS(16)); continue; }   // 실패해도 박자를 지킨다(쉬지 않고 돌면 화면이 굶는다)
+        ++s_cap_blocks;
+        split_slots(s_slots, BLOCK, s_mic, s_ref);
+        int64_t t0 = esp_timer_get_time();
+        aec_process(s_aec, s_mic, s_ref, s_clean);
+        s_t_aec.add(esp_timer_get_time() - t0);
+        s_t_aec.halve_if_big();
+        r_mic.add(s_mic, BLOCK); r_ref.add(s_ref, BLOCK); r_aec.add(s_clean, BLOCK);
+        if (++rms_blocks >= RATE / BLOCK) { s_mic_rms = r_mic.take(); s_ref_rms = r_ref.take(); s_aec_rms = r_aec.take(); rms_blocks = 0; }   // 약 1초마다
+
+        if (mode == Mode::TEST) {                               // 혼자 하는 점검: 보내지 않고 세기만 잰다
+            if (s_test_done.load()) continue;
+            if (++test_blocks <= SELFTEST_SKIP_BLOCKS) continue;
+            t_mic.add(s_mic, BLOCK); t_ref.add(s_ref, BLOCK); t_aec.add(s_clean, BLOCK);
+            if (test_blocks >= SELFTEST_SKIP_BLOCKS + SELFTEST_BLOCKS) {
+                s_test_result.mic = t_mic.take(); s_test_result.ref = t_ref.take(); s_test_result.aec = t_aec.take();
+                s_test_done = true;
+            }
+            continue;
+        }
+
+        if (s_ping_req.exchange(false)) ping.start();           // 지연 재기: 마이크 대신 "쉼 → 삑 → 쉼"
+        if (ping.active()) {
+            const int at = ping.render(blk, BLOCK);
+            if (at >= 0) {                                      // 삑의 첫 표본이 "마이크에 들어온" 시각 = 이 묶음의 끝에서 (BLOCK − at) 표본 전
+                s_ping_t0_ms = now_ms() - (uint32_t)((BLOCK - at) * 1000 / RATE);
+                s_ping_armed = true;
+            }
+        } else {
+            pick(s_src.load(), s_clean, s_mic, s_ref, tone, blk);
+        }
+        up.push(blk);
+        uint16_t seq = 0;
+        while (up.pop(frame, &seq)) {                           // 시험 펌웨어의 esp_opus_enc_process 호출 그대로
+            esp_audio_enc_in_frame_t in = {}; in.buffer = (uint8_t *)frame; in.len = FRAME * 2;
+            esp_audio_enc_out_frame_t out = {}; out.buffer = pkt + 2; out.len = (uint32_t)s_enc_out;
+            t0 = esp_timer_get_time();
+            const bool ok = esp_opus_enc_process(s_enc, &in, &out) == ESP_AUDIO_ERR_OK && out.encoded_bytes > 0;
+            s_t_enc.add(esp_timer_get_time() - t0);
+            s_t_enc.halve_if_big();
+            const size_t n = ok ? relayproto::pack_voice(pkt, sizeof pkt, seq, pkt + 2, out.encoded_bytes) : 0;
+            if (!n) { ++s_enc_fail; continue; }
+            if (s_txq->put(pkt, n, now_ms())) xTaskNotifyGive(s_tx_task);
+        }
+    }
+}
+
+// 만들 것을 차례로 만든다. 실패하면 그 단계의 이름(성공이면 nullptr)
+const char *setup()
+{
+    static StaticSemaphore_t jb_buf, txq_buf;
+    s_jb_lock.h = xSemaphoreCreateMutexStatic(&jb_buf);
+    s_txq_lock.h = xSemaphoreCreateMutexStatic(&txq_buf);
+    void *rx_mem = heap_caps_malloc(sizeof(Receiver<Mutex>), MALLOC_CAP_SPIRAM);
+    void *txq_mem = heap_caps_malloc(sizeof(TxFifo<Mutex>), MALLOC_CAP_SPIRAM);
+    if (!rx_mem || !txq_mem) return "memory";
+    s_rx = new (rx_mem) Receiver<Mutex>(s_jb_lock);
+    s_txq = new (txq_mem) TxFifo<Mutex>(s_txq_lock);
+
+    // 에코 제거·Opus 는 캡처 태스크가 자기 스택(48KB)에서 만든다 — 끝날 때까지 여기서 기다린다. 통화 밖이라 그 뒤 태스크는 잠든다.
+    static StaticSemaphore_t done_buf;
+    s_codec_done = xSemaphoreCreateBinaryStatic(&done_buf);
+    if (xTaskCreatePinnedToCore(cap_task, "v_cap", CAP_STACK, nullptr, CAP_PRIO, &s_cap_task, CAP_CORE) != pdPASS) return "task-cap";
+    if (xSemaphoreTake(s_codec_done, pdMS_TO_TICKS(CODEC_SETUP_WAIT_MS)) != pdTRUE) return "codec-timeout";
+    if (s_codec_fail) { s_cap_task = nullptr; return s_codec_fail; }
+
+    if (xTaskCreatePinnedToCoreWithCaps(rx_task, "v_rx", RX_STACK, nullptr, RX_PRIO, &s_rx_task, RX_CORE, RX_STACK_CAPS) != pdPASS) return "task-rx";
+    if (xTaskCreatePinnedToCore(tx_task, "v_tx", TX_STACK, nullptr, TX_PRIO, &s_tx_task, TX_CORE) != pdPASS) return "task-tx";
+    if (!audioout::add_source(fill)) return "source";
+    relaylink::set_voice_sink(on_frame);
+    return nullptr;
+}
+
+}  // namespace
+
+bool start()
+{
+    if (s_ready) return true;
+    if (!audioout::ready()) return false;                       // 소리 장치가 없다 — 통화 흐름은 그대로, 목소리만 없다
+    const char *stage = setup();
+    if (stage) {
+        ESP_LOGE(TAG, "voice setup failed at '%s'", stage);
+        audioout::fail("voice");                                // STAT audio=0, boot_guard 가 다시 켜 본다(PL3b-D2)
+        return false;
+    }
+    s_ready = true;
+    printf("VOICE ready chunk=%d opus_in=%d opus_out_max=%d bitrate=%d aec=%s\n", BLOCK, FRAME * 2, s_enc_out, OPUS_BITRATE, aec_get_mode_string(AEC_MODE));
+    return true;
+}
+
+bool set(bool on)
+{
+    if (!s_ready) return false;
+    if (on) {
+        if (s_mode.load() == Mode::CALL) return false;
+        s_src = Src::AEC; s_ping_req = false; s_ping_armed = false; s_ping_result = -1;
+        s_txq->flush(); s_txq->clear_dropped();
+        s_tx_ok = 0; s_tx_fail = 0; s_enc_fail = 0;
+        s_t_aec.reset(); s_t_enc.reset(); s_t_dec.reset();
+        s_rx->start();                                          // 지터 버퍼를 비우고 받기 시작한다
+        s_mode = Mode::CALL;
+        xTaskNotifyGive(s_cap_task);                            // 마이크를 연다(캡처 태스크가 읽기 시작한다 — 앞 128ms 는 버린다)
+        audioout::wake();                                       // 출력 태스크가 목소리 공급자를 부르기 시작한다
+        return true;
+    }
+    if (s_mode.load() != Mode::CALL) return false;
+    s_mode = Mode::OFF;                                         // 캡처 태스크는 읽던 묶음(최대 16ms)을 끝으로 잠든다. 송신 태스크는 남은 것을 버린다
+    s_rx->stop();                                               // 받는 쪽: 더 받지 않는다. 출력은 5ms 에 걸쳐 줄이고 끝난다
+    s_txq->flush();
+    s_src = Src::AEC; s_ping_armed = false;
+    audioout::wake();
+    return true;
+}
+
+bool on() { return s_mode.load() == Mode::CALL; }
+
+bool set_source(Src s)
+{
+    if (s_mode.load() != Mode::CALL) return false;
+    s_src = s;
+    return true;
+}
+
+bool ping()
+{
+    if (s_mode.load() != Mode::CALL || s_ping_armed.load()) return false;
+    s_ping_result = -1;
+    s_ping_req = true;
+    return true;
+}
+
+bool selftest(SelfTest *out)
+{
+    Mode expect = Mode::OFF;
+    if (!s_ready || !out || !s_mode.compare_exchange_strong(expect, Mode::TEST)) return false;   // 통화 중에는 하지 않는다
+    const uint32_t t0 = now_ms();
+    s_test_done = false; s_test_start = true;
+    xTaskNotifyGive(s_cap_task);
+    audioout::wake();
+    for (int i = 0; i < 100 && !s_test_done.load() && s_mode.load() == Mode::TEST; ++i) vTaskDelay(pdMS_TO_TICKS(20));   // 최대 2초
+    const bool done = s_test_done.load();
+    while (done && s_mode.load() == Mode::TEST && (uint32_t)(now_ms() - t0) < SELFTEST_HOLD_MS) vTaskDelay(pdMS_TO_TICKS(20));
+    expect = Mode::TEST;
+    s_mode.compare_exchange_strong(expect, Mode::OFF);          // 그 사이 통화가 열렸으면 건드리지 않는다
+    if (done) *out = s_test_result;
+    return done;
+}
+
+void print_pending()
+{
+    int32_t r = s_ping_result.exchange(-1);
+    if (r == -1 && s_ping_armed.load() && (uint32_t)(now_ms() - s_ping_t0_ms.load()) > PING_TIMEOUT_MS && s_ping_armed.exchange(false)) r = -2;
+    if (r >= 0) printf("VPING rt_ms=%d\n", (int)r);
+    else if (r == -2) printf("VPING timeout\n");
+}
+
+void stat(Stat *o)
+{
+    *o = Stat{};
+    o->ready = s_ready; o->on = on(); o->src = s_src.load();
+    if (!s_ready) return;
+    o->cap = s_cap_blocks; o->tx = s_tx_ok; o->tx_drop = s_txq->dropped(); o->tx_fail = s_tx_fail.load() + s_enc_fail.load();
+    o->rx = s_rx->stats();
+    o->aec_us = s_t_aec.avg(); o->aec_max = s_t_aec.max; o->enc_us = s_t_enc.avg(); o->enc_max = s_t_enc.max; o->dec_us = s_t_dec.avg(); o->dec_max = s_t_dec.max;
+    o->mic_rms = s_mic_rms; o->ref_rms = s_ref_rms; o->aec_rms = s_aec_rms;
+    o->rd_err = audioout::read_errors();
+    o->stk_cap = (unsigned)uxTaskGetStackHighWaterMark(s_cap_task);
+    o->stk_rx = (unsigned)uxTaskGetStackHighWaterMark(s_rx_task);
+    o->stk_tx = (unsigned)uxTaskGetStackHighWaterMark(s_tx_task);
+}
+
+}  // namespace voice
--- a/firmware/tab5_phone/main/CMakeLists.txt
+++ b/firmware/tab5_phone/main/CMakeLists.txt
@@ -10,6 +10,7 @@
                            "ui_wifi.cpp"      # Wi-Fi 설정 화면
                            "audio_out.cpp"    # 스피커 출력(ES8388 + I2S 송신) — 공급자에게 PCM 을 받아 낸다(Phase 3a)
                            "tone_out.cpp"     # 신호음 공급자(벨·연결음·종료음, 박자는 tones.h)
+                           "voice.cpp"        # 목소리 통화 경로(마이크 → 에코 제거 → Opus → 교환기 → 지터 버퍼 → Opus → 스피커, 판단은 voice_core.h) — Phase 3b
                        INCLUDE_DIRS ".")
 
 # B-D1: -Wliteral-suffix 한 종류만 끈다(main 의 C++ 파일에만).
```

- [x] **Step 2: 이 기기에서 에코 제거·Opus 를 돌린 시험 펌웨어와 줄 단위로 대조하고, 모듈만 커밋한다** (`voice.cpp` 는 맥 시험이 없다 — 대조 결과를 보고에 적는다)

```bash
cd ~/tab5-toy-phone/firmware && for k in "heap_caps_aligned_calloc(16, (size_t)samples, sizeof(int16_t), MALLOC_CAP_INTERNAL)" "AEC_FILTER_LEN, 1, " "aec_get_chunksize(" "esp_opus_enc_config_t ecfg = ESP_OPUS_ENC_CONFIG_DEFAULT();" "ecfg.sample_rate = ESP_AUDIO_SAMPLE_RATE_16K; ecfg.channel = ESP_AUDIO_MONO; ecfg.bitrate = OPUS_BITRATE;" "ecfg.frame_duration = ESP_OPUS_ENC_FRAME_DURATION_20_MS; ecfg.application_mode = ESP_OPUS_ENC_APPLICATION_VOIP; ecfg.complexity = OPUS_COMPLEXITY;" "esp_opus_dec_cfg_t dcfg = ESP_OPUS_DEC_CONFIG_DEFAULT();" "dcfg.sample_rate = ESP_AUDIO_SAMPLE_RATE_16K; dcfg.channel = ESP_AUDIO_MONO; dcfg.frame_duration = ESP_OPUS_DEC_FRAME_DURATION_20_MS;" "esp_audio_enc_in_frame_t in = {}; in.buffer = (uint8_t *)" "== ESP_AUDIO_ERR_OK && out.encoded_bytes > 0;" "esp_audio_dec_in_raw_t raw = {}; raw.buffer = " "esp_audio_dec_out_frame_t frame = {}; frame.buffer = (uint8_t *)" "48 * 1024"; do echo "$(grep -cF -- "$k" tab5_phone_probe/main/probe_main.cpp) $(grep -cF -- "$k" tab5_phone/main/voice.cpp)  $k"; done; grep -n "define AEC_FILTER_LEN\|define OPUS_BITRATE\|define OPUS_COMPLEXITY\|define PROBE_AEC_MODE" tab5_phone_probe/main/probe_main.cpp | cut -c1-60; grep -n "constexpr int AEC_FILTER_LEN\|constexpr aec_mode_t AEC_MODE\|constexpr int OPUS_BITRATE\|constexpr int OPUS_COMPLEXITY" tab5_phone/main/voice.cpp | cut -c1-64
```
Expected: 13줄이 모두 `1 1  <글자>` 이고 마지막만 `1 2  48 * 1024`(스택 48KB — 새 파일에서는 캡처·해제 태스크 둘), 이어서 값 여덟 줄:
```
50:#define AEC_FILTER_LEN     4            // esp-sr 권장값
51:#define PROBE_AEC_MODE     AEC_MODE_VOIP_HIGH_PERF   // 실
52:#define OPUS_BITRATE       24000        // PH-D5
53:#define OPUS_COMPLEXITY    0            // 0(가벼움)~10. 실측:
58:constexpr int AEC_FILTER_LEN = 4;                            
59:constexpr aec_mode_t AEC_MODE = AEC_MODE_VOIP_HIGH_PERF;     
60:constexpr int OPUS_BITRATE = 24000;                          
61:constexpr int OPUS_COMPLEXITY = 0;                           
```

시험 펌웨어와 **다른 것 다섯**(전부 "확인하지 못한 것"에 올라 있다):

| 항목 | 시험 펌웨어 `probe_main.cpp` `audio_task` | `voice.cpp` | 판단 |
|---|---|---|---|
| 태스크 | 하나(코어 1, 우선순위 10, 스택 48KB)가 읽기·에코 제거·압축·해제·쓰기를 다 함 | 셋: `v_cap`(1·7·48KB 내부) / `v_rx`(1·7·48KB **PSRAM**) / `v_tx`(0·5·8KB) + 3a 의 출력 태스크 | PL3b-D15 |
| 에코 제거·Opus 를 만드는 자리 | 48KB 소리 태스크 안 | 같은 조건: `v_cap`(48KB) 태스크의 첫 일(`codec_setup`). 부팅 태스크(스택 8KB)는 끝나기를 기다리기만 한다 | 같다(부팅 태스크에서 만들면 스택이 넘쳐 세지 못하는 재부팅 고리가 될 수 있다) |
| 해제기 다시 맞추기 | 없음(한 번 열고 계속) | 통화가 열릴 때 `esp_opus_dec_reset` | PL3b-D7 |
| 빠진 프레임 | 없음(같은 태스크 안에서 바로 풀었다) | `voicecore::Concealer`(해제기에 빈 입력을 주지 않는다) | PL3b-D7 |
| 압축한 것을 담는 곳 | 힙(`enc_out` 바이트) | 정적 배열 200바이트(앞 2바이트 = 순번, 권장 출력 160 + 2 ≤ 200 을 부팅 때 확인 — 아니면 `AUDIO FAIL voice`) | 같다 |

| 확인할 것 | 코드(`voice.cpp`) |
|---|---|
| 통화 밖에서는 캡처 태스크가 잠들어 있고 마이크를 읽지 않는다 | `cap_task` 의 `if (mode == Mode::OFF) { … ulTaskNotifyTake(pdTRUE, portMAX_DELAY); continue; }` |
| 끄면 그 자리에서: 방식을 `OFF` 로 → 받는 쪽 닫기 → 보내기 대기열 비우기 → 출력 태스크 깨우기(5ms 줄임을 내게). 기다리지 않고 돌아온다 | `set(false)` |
| 송신 태스크는 통화가 끝났으면 꺼낸 프레임도 보내지 않는다. 보내기가 실패하면 쌓인 것을 버린다 | `tx_task` |
| 공급자(`fill`)는 출력 태스크 안에서 링버퍼 복사와 통지만 한다(잠금·`printf`·해제 없음) | `fill` |
| 웹소켓 태스크(`on_frame`)는 지터 버퍼에 복사만 한다(짧은 뮤텍스). 통화 밖의 프레임은 `Receiver::push` 가 버린다 | `on_frame` |
| 마이크 읽기가 실패하면 16ms 쉰다(쉬지 않고 돌면 화면이 굶는다) | `cap_task` 의 `if (!audioout::read_mic(…)) { vTaskDelay(pdMS_TO_TICKS(16)); continue; }` |
| 준비가 실패하면 `audioout::fail("voice")` → `STAT audio=0`, `boot_guard` 가 다시 켜 본다 | `start()` |
| 목소리 PCM·압축 프레임을 찍는 `printf` 가 없다 | `grep -c "printf" voice.cpp` → 3줄(`VOICE ready`, `VPING rt_ms`, `VPING timeout`) |
| 혼자 하는 점검은 통화 밖에서만 시작하고, 도중에 통화가 열리면 물러난다 | `selftest` 의 `compare_exchange_strong`, `fill` 의 `if (s_mode.load() != Mode::TEST) s_test_tone.stop();` |

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/main/voice.h firmware/tab5_phone/main/voice.cpp firmware/tab5_phone/main/CMakeLists.txt && git commit -m "feat(phone): 목소리 모듈 voice — 캡처(마이크·에코 제거·Opus)·해제·송신 태스크 셋, 진단(vsrc·vping·selftest) — 아직 잇지 않음" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [x] **Step 3: 도구 시험 패치를 적용하고 실패를 확인한다** — 새 `STAT` 모양을 **펌웨어 소스의 형식 문자열에서 읽어** 고정하는 시험이라, 연결 패치 전에는 실패한다

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft && git apply "$D/patches/T6-02-phone-ctl-tests.patch" && git diff --numstat -- firmware/tab5_phone/tools/test_phone_ctl.py && cmp "$D/files/firmware/tab5_phone/tools/test_phone_ctl.py" firmware/tab5_phone/tools/test_phone_ctl.py && echo "같음: test_phone_ctl.py" && cd firmware/tab5_phone/tools && PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider test_phone_ctl.py 2>&1 | tail -1
```
Expected: `156	0	firmware/tab5_phone/tools/test_phone_ctl.py`(삭제 줄 0), `같음: test_phone_ctl.py`, `11 failed, 207 passed in …s` (실패 11 = 새 `STAT` 칸을 보는 시험들 — `test_firmware_stat_keeps_every_old_field_and_only_appends` 등. 나머지 새 시험 21개는 도구를 고치지 않고도 진단 명령·`VOICE`·`VPING`·`SELFTEST` 줄을 다룬다는 것을 고정하며 바로 통과한다.)

```diff
--- a/firmware/tab5_phone/tools/test_phone_ctl.py
+++ b/firmware/tab5_phone/tools/test_phone_ctl.py
@@ -861,3 +861,159 @@
     assert hub.shot("2", str(path)) and path.read_bytes() == JPEG
     assert hub.queues["2"].lines == ["TONE BELL off", "FSM INCOMING -> INCOMING acts=0x80 notice=NONE", "TONE END on"]
     assert hub.wait(("2",), r"^TONE END on$", 0)
+
+
+# ── Phase 3b: 목소리 통화의 새 줄(STAT 끝의 칸, VOICE·VPING·SELFTEST, 진단 명령) — 기존 도구가 고치지 않고 읽는다 ─────────────────
+# 3a 의 stat3a() 는 손으로 만든 줄이었다(3a 최종 검수의 지적). 여기서는 펌웨어 소스(main/app.cpp)의 형식 문자열을 읽어 줄을 만든다
+# → 펌웨어가 STAT 의 칸을 바꾸면 이 시험들이 따라 바뀐 줄을 보게 된다.
+from pathlib import Path
+
+APP_CPP = Path(__file__).resolve().parent.parent / "main" / "app.cpp"
+STAT3A_FORMAT = ("STAT up=%u unit=%d fsm=%s screen=%s wifi=%d relay=%d relay_code=%d peer=%d missed=%u touch=%u heap_int=%u heap_int_min=%u heap_psram=%u"
+                 " stk_app=%u stk_touch=%u stk_relay=%u stk_ws=%u audio=%d tone=%s aud_us=%u/%u aud_err=%u stk_audio=%u")   # 5d9935f(기기에 올라가 있던 판)의 줄
+STAT3B_TAIL = (" amp=%d aud_late=%u voice=%d vsrc=%s v_cap=%u v_tx=%u v_txdrop=%u v_txfail=%u v_rx=%u v_play=%u v_lost=%u v_drop=%u v_under=%u v_jb=%d/%d"
+               " v_aec_us=%u/%u v_enc_us=%u/%u v_dec_us=%u/%u v_mic=%u v_ref=%u v_aec=%u v_spk=%u v_rderr=%u stk_vcap=%u stk_vrx=%u stk_vtx=%u")
+
+
+def firmware_stat_format():
+    """main/app.cpp 의 printf("STAT …") 형식 문자열(이어 쓴 조각을 붙인 것, 끝의 \\n 은 뗀다)."""
+    src = APP_CPP.read_text(encoding="utf-8")
+    m = re.search(r'printf\(((?:\s*"(?:[^"\\]|\\.)*")+)\s*,', src[src.index('printf("STAT up='):])
+    return "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1))).replace("\\n", "")
+
+
+def stat3b(**over):
+    """펌웨어의 형식 문자열에 값을 채운 STAT 줄. 값을 주지 않은 칸은 대기 중인 기기의 값."""
+    vals = {"up": 157, "unit": 2, "fsm": "IDLE", "screen": None, "wifi": 1, "relay": 1, "relay_code": 0, "peer": 1, "missed": 0, "touch": 3,
+            "heap_int": 140000, "heap_int_min": 131000, "heap_psram": 24700000, "stk_app": 10280, "stk_touch": 3284, "stk_relay": 6108, "stk_ws": 6128,
+            "audio": 1, "tone": "NONE", "aud_us": "61/840", "aud_err": 0, "stk_audio": 4200, "amp": 1, "aud_late": 0, "voice": 0, "vsrc": "aec",
+            "v_cap": 0, "v_tx": 0, "v_txdrop": 0, "v_txfail": 0, "v_rx": 0, "v_play": 0, "v_lost": 0, "v_drop": 0, "v_under": 0, "v_jb": "0/3",
+            "v_aec_us": "0/0", "v_enc_us": "0/0", "v_dec_us": "0/0", "v_mic": 0, "v_ref": 0, "v_aec": 0, "v_spk": 0, "v_rderr": 0,
+            "stk_vcap": 46000, "stk_vrx": 47000, "stk_vtx": 7000}
+    vals.update(over)
+    if vals["screen"] is None:
+        vals["screen"] = vals["fsm"]
+    out = []
+    for word in firmware_stat_format().split(" "):
+        key, eq, spec = word.partition("=")
+        out.append(f"{key}={vals[key]}" if eq else word)
+        assert not eq or key in vals, f"펌웨어 STAT 에 이 시험이 모르는 칸이 있다: {word}"
+    return " ".join(out)
+
+
+def test_firmware_stat_keeps_every_old_field_and_only_appends():
+    """기존 줄 모양은 한 글자도 바꾸지 않는다: 펌웨어의 형식 문자열은 3a 의 것으로 시작하고, 3b 의 칸은 그 뒤에만 붙는다."""
+    fmt = firmware_stat_format()
+    assert fmt.startswith(STAT3A_FORMAT)
+    assert fmt == STAT3A_FORMAT + STAT3B_TAIL
+
+
+def test_stat3b_is_built_from_the_firmware_source():
+    line = stat3b()
+    assert line.startswith("STAT up=157 unit=2 fsm=IDLE screen=IDLE wifi=1 relay=1 relay_code=0 peer=1 missed=0 touch=3 heap_int=140000 ")
+    assert " audio=1 tone=NONE aud_us=61/840 aud_err=0 stk_audio=4200 amp=1 aud_late=0 voice=0 vsrc=aec v_cap=0 " in line and line.endswith(" stk_vcap=46000 stk_vrx=47000 stk_vtx=7000")
+    assert "%" not in line and "\\" not in line
+
+
+def test_parse_stat_reads_the_voice_fields_and_the_old_fields_as_before():
+    s = pc.parse_stat(stat3b(fsm="IN_CALL", voice=1, vsrc="tone", v_cap=1500, v_tx=1196, v_rx=1190, v_play=1187, v_lost=1, v_drop=2, v_jb="2/3",
+                             v_aec_us="2810/3400", v_enc_us="2200/3900", v_dec_us="310/450", v_mic=52, v_ref=590, v_aec=9, v_spk=1410, amp=0, aud_late=0))
+    assert s["voice"] == 1 and s["vsrc"] == "tone" and s["v_cap"] == 1500 and s["v_tx"] == 1196 and s["v_rx"] == 1190 and s["v_play"] == 1187
+    assert s["v_lost"] == 1 and s["v_drop"] == 2 and s["v_under"] == 0 and s["v_jb"] == (2, 3) and s["amp"] == 0 and s["aud_late"] == 0
+    assert s["v_aec_us"] == (2810, 3400) and s["v_enc_us"] == (2200, 3900) and s["v_dec_us"] == (310, 450)
+    assert s["v_mic"] == 52 and s["v_ref"] == 590 and s["v_aec"] == 9 and s["v_spk"] == 1410 and s["v_rderr"] == 0
+    assert s["fsm"] == "IN_CALL" and s["screen"] == "IN_CALL" and s["touch"] == 3 and s["tone"] == "NONE" and s["aud_us"] == (61, 840) and s["stk_ws"] == 6128
+    assert all(type(s[k]) is int for k in s if k not in ("fsm", "screen", "tone", "vsrc", "aud_us", "v_jb", "v_aec_us", "v_enc_us", "v_dec_us"))
+
+
+def test_touch_audit_pattern_reads_the_phase3b_stat():
+    rx = r"^STAT up=(\d+) .*?fsm=(\w+) screen=(\w+).*?peer=(\d).*?touch=(\d+)"
+    assert re.match(rx, stat3b(fsm="INCOMING", screen="DECLINE", touch=41)).groups() == ("157", "INCOMING", "DECLINE", "1", "41")
+
+
+@pytest.mark.parametrize("pattern, kwargs", [
+    (r"^STAT ", {}),
+    (r"^STAT .*fsm=IDLE.*peer=1", {}),
+    (r"^STAT .*fsm=IDLE.*relay=1.*peer=1.*missed=0", {}),
+    (r"^STAT .*fsm=IDLE screen=IDLE.*peer=1", {}),
+    (r"^STAT .*fsm=IN_CALL.*screen=IN_CALL", {"fsm": "IN_CALL"}),
+    (r"^STAT .*fsm=INCOMING screen=DECLINE", {"fsm": "INCOMING", "screen": "DECLINE"}),
+    (r"^STAT .*fsm=RINGBACK screen=DIALING", {"fsm": "RINGBACK", "screen": "DIALING"}),
+    (r"^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE", {}),
+    (r"^STAT .*fsm=INCOMING.*tone=BELL", {"fsm": "INCOMING", "tone": "BELL"}),
+    (r"^STAT .*fsm=IN_CALL.*tone=NONE", {"fsm": "IN_CALL"}),
+    (r"^STAT .*fsm=IDLE.*peer=1.*tone=NONE", {}),
+])
+def test_stat_patterns_of_the_readme_and_earlier_plans_match_the_phase3b_stat(pattern, kwargs):
+    assert re.search(pattern, stat3b(**kwargs))
+
+
+def test_margin_grep_picks_up_the_three_voice_task_stacks():
+    got = re.findall(r"heap_int_min=[0-9]+|stk_[a-z]+=[0-9]+", stat3b())
+    assert got == ["heap_int_min=131000", "stk_app=10280", "stk_touch=3284", "stk_relay=6108", "stk_ws=6128", "stk_audio=4200",
+                   "stk_vcap=46000", "stk_vrx=47000", "stk_vtx=7000"]
+
+
+@pytest.mark.parametrize("pattern, kwargs, want", [
+    (r"^STAT .*fsm=IN_CALL.*tone=NONE.* voice=1 ", {"fsm": "IN_CALL", "voice": 1}, True),
+    (r"^STAT .*fsm=IN_CALL.*tone=NONE.* voice=1 ", {"fsm": "IN_CALL", "voice": 0}, False),
+    (r"^STAT .*fsm=IDLE.*peer=1.*tone=NONE.* voice=0 ", {}, True),
+    (r"^STAT .*fsm=IDLE.*peer=1.*tone=NONE.* voice=0 ", {"voice": 1}, False),
+    (r"^STAT .*fsm=INCOMING.*tone=BELL.* voice=0 ", {"fsm": "INCOMING", "tone": "BELL"}, True),
+    (r"^STAT .*audio=1 .* amp=0 ", {"amp": 0}, True),
+    (r"^STAT .*audio=1 .* amp=0 ", {"amp": 1}, False),
+    (r"^STAT .*audio=1 .* amp=1 ", {"amp": 1}, True),
+    (r"^STAT .*audio=0 ", {"audio": 0, "amp": 0}, True),
+    (r"^STAT .* voice=1 vsrc=tone ", {"fsm": "IN_CALL", "voice": 1, "vsrc": "tone"}, True),
+    (r"^STAT .* voice=1 vsrc=tone ", {"fsm": "IN_CALL", "voice": 1, "vsrc": "aec"}, False),
+    (r"^STAT .* voice=0 vsrc=aec ", {}, True),
+])
+def test_voice_patterns_of_the_phase3b_device_task(pattern, kwargs, want):
+    assert bool(re.search(pattern, stat3b(**kwargs))) is want
+
+
+def test_diagnostic_commands_are_sent_as_plain_lines():
+    """진단 명령(quiet·selftest·vsrc·vping·fault)은 도구가 모르는 낱말이라 그대로 한 줄로 나간다 — 도구를 고치지 않는다."""
+    steps = ["both:quiet on", "2:selftest", "1:vsrc tone", "2:vsrc ref", "1:vping", "2:fault audio", "both:quiet off"]
+    plan = pc.plan_steps(steps, pair=True)
+    assert [(units, kind, args) for units, kind, args in plan] == [
+        (("1", "2"), "send", ("quiet on",)), (("2",), "send", ("selftest",)), (("1",), "send", ("vsrc tone",)), (("2",), "send", ("vsrc ref",)),
+        (("1",), "send", ("vping",)), (("2",), "send", ("fault audio",)), (("1", "2"), "send", ("quiet off",))]
+    hub, links, clock, out = make_hub()
+    assert pc.run_steps(hub, plan)
+    assert links["1"].cmds == ["quiet on", "vsrc tone", "vping", "quiet off"] and links["2"].cmds == ["quiet on", "selftest", "vsrc ref", "fault audio", "quiet off"]
+
+
+def test_selftest_and_vping_answers_are_found_by_wait():
+    hub, links, clock, out = make_hub()
+
+    def respond(link, cmd):
+        if cmd == "selftest":
+            link.say("SELFTEST amp=0 mic=41 ref=588 aec=12 ok=1", at=clock.t + 0.8)
+        elif cmd == "vping":
+            link.say("OK vping", at=clock.t)
+            link.say("VPING rt_ms=212", at=clock.t + 0.9)
+        elif cmd.startswith("vsrc "):
+            link.say("OK " + cmd, at=clock.t)
+        elif cmd.startswith("quiet "):
+            link.say(f"OK {cmd} amp={0 if cmd.endswith('on') else 1}", at=clock.t)
+    links["1"].respond = links["2"].respond = respond
+    plan = pc.plan_steps(["both:quiet on", r"both:wait:^OK quiet on amp=0$:3", "2:selftest", r"2:wait:^SELFTEST amp=0 mic=\d+ ref=\d+ aec=\d+ ok=1$:5",
+                          "2:vsrc ref", r"2:wait:^OK vsrc ref$:3", "1:vping", r"1:wait:^OK vping$:3", r"1:wait:^VPING rt_ms=\d+$:5"], pair=True)
+    assert pc.run_steps(hub, plan), "\n".join(out)
+    assert "[1] VPING rt_ms=212" in out and "[2] SELFTEST amp=0 mic=41 ref=588 aec=12 ok=1" in out
+
+
+def test_voice_lines_do_not_disturb_a_tap_and_can_be_waited_for_in_order():
+    """VOICE 줄은 app 태스크가 그 원인인 FSM 줄 바로 앞에 찍는다 — tap 은 자기 줄(OK t)만 꺼내고, 뒤의 wait 가 VOICE → FSM 순서로 찾는다."""
+    hub, links, clock, out = make_hub(("1",))
+    links["1"].respond = lambda link, cmd: link.say("OK " + cmd, "TONE END on", "VOICE off", "FSM IN_CALL -> ENDING acts=0x248 notice=NONE", at=clock.t)
+    assert hub.tap(("1",), 644, 606)
+    assert hub.queues["1"].lines == ["TONE END on", "VOICE off", "FSM IN_CALL -> ENDING acts=0x248 notice=NONE"]
+    assert hub.wait(("1",), r"^VOICE off$", 0) and hub.wait(("1",), r"^FSM IN_CALL -> ENDING acts=0x248 notice=NONE$", 0)
+
+
+def test_a_failed_selftest_or_vping_is_visible():
+    assert not re.search(r"^SELFTEST amp=\d mic=\d+ ref=\d+ aec=\d+ ok=1$", "SELFTEST amp=0 mic=40 ref=12 aec=3 ok=0")
+    assert not re.search(r"^SELFTEST amp=\d mic=\d+ ref=\d+ aec=\d+ ok=1$", "SELFTEST FAIL busy-or-no-audio")
+    assert not re.search(r"^VPING rt_ms=\d+$", "VPING timeout")
```

- [x] **Step 4: 연결 패치를 적용하고 통과를 확인한다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft && git apply "$D/patches/T6-03-app-main.patch" && for f in app.h app.cpp phone_main.cpp; do cmp "$D/files/firmware/tab5_phone/main/$f" "firmware/tab5_phone/main/$f" && echo "같음: $f"; done; cd firmware/tab5_phone/tools && PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider test_phone_ctl.py 2>&1 | tail -1
```
Expected: `같음:` 3줄, `218 passed in …s` (기존 186 을 고치지 않고 + 32).

```diff
--- a/firmware/tab5_phone/main/app.h
+++ b/firmware/tab5_phone/main/app.h
@@ -7,7 +7,7 @@
 
 namespace app {
 
-// 화면·통화 진행 태스크를 띄운다. gfx::init, audioout::start, toneout::start, netwifi::start, relaylink::start 뒤에 한 번. unit = 이 기기의 번호(미등록이면 0).
+// 화면·통화 진행 태스크를 띄운다. gfx::init, audioout::start, toneout::start, voice::start, netwifi::start, relaylink::start 뒤에 한 번. unit = 이 기기의 번호(미등록이면 0).
 // i2c_lock = M5.In_I2C 를 쓰는 코드가 함께 잡는 잠금. 터치 읽기(M5.update)와 코덱·앰프 제어(audio_out)가 같은 버스를 쓴다 — phone_main 이 만든다(PL3a-D14).
 void start(int unit, SemaphoreHandle_t i2c_lock);
 
--- a/firmware/tab5_phone/main/app.cpp
+++ b/firmware/tab5_phone/main/app.cpp
@@ -14,8 +14,11 @@
 //       비용: 기억이 두 군데(Fsm::bell_muted / s_picker). 탈출구: 고르기 열림도 Fsm 에 넣는다.
 // 신호음(Phase 3a, docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md): apply() 가 할 일의 A_TONE_* 를 tones::from_acts 로 소리 하나로 바꿔
 //       toneout::play 에 넘긴다. PL3a-D13: 신호 보내기보다 먼저 — 보내기가 Wi-Fi 때문에 최대 2초 막혀도(RL-D6) 벨은 누른 즉시 멈춘다.
-//       비용: 기록 줄 "TONE …" 이 원인이 된 "FSM …" 줄보다 먼저 찍힐 수 있다(다른 태스크가 찍는다 — 원래도 앞뒤가 정해져 있지 않다).
-//       탈출구: 소리 명령을 FSM 줄 printf 뒤로 옮긴다.
+//       탈출구: 소리 명령을 FSM 줄 printf 뒤로 옮긴다. (Phase 3b: "TONE …" 줄은 이 태스크가 다음 바퀴에 찍는다 — PL3b-D1.)
+// 목소리(Phase 3b, docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md PL3b-D13): 같은 할 일의 A_VOICE_START·A_VOICE_STOP 을 voicecore::from_acts 로
+//       켜기/끄기로 바꿔 voice::set 에 넘긴다. 소리 명령 다음, 신호 보내기보다 먼저 — 끊기를 누르면 보내기가 막혀 있어도 마이크는 그 자리에서 닫힌다.
+//       상태가 실제로 바뀌었을 때만 "VOICE on|off" 한 줄을 찍는다(그 원인인 "FSM …" 줄보다 먼저 찍힌다 — 같은 태스크라 순서가 늘 같다).
+//       받기 전에는 켜지지 않는다(PH-D6): 상태 기계가 A_VOICE_START 를 IN_CALL 로 들어갈 때만 낸다(맥 시험 test_voice_core.cpp 가 전수로 고정).
 #include "app.h"
 
 #include <M5Unified.h>
@@ -38,6 +41,7 @@
 #include "tone_out.h"
 #include "tones.h"
 #include "ui.h"
+#include "voice.h"
 #include "ui_layout.h"
 #include "ui_wifi.h"
 
@@ -77,6 +81,8 @@
 {
     tones::Tone tone;
     if (tones::from_acts(o.acts, &tone)) toneout::play(tone);   // PL3a-D13: 보내기보다 먼저. 소리 비트가 없으면 지금 소리를 그대로 둔다
+    bool talk = false;                                           // PL3b-D13: 목소리도 보내기보다 먼저(끊으면 마이크가 바로 닫힌다)
+    if (voicecore::from_acts(o.acts, &talk) && voice::set(talk)) printf("VOICE %s\n", talk ? "on" : "off");
     static const uint16_t SENDS[] = { A_SEND_CALL, A_SEND_ACCEPT, A_SEND_REJECT, A_SEND_HANGUP };
     for (uint16_t bit : SENDS) {
         if (!(o.acts & bit)) continue;
@@ -86,7 +92,6 @@
         if (!relaylink::send_text(text)) ESP_LOGW(TAG, "signal 0x%x not sent (link down)", bit);
     }
     if (o.acts & A_RECONNECT) relaylink::reconnect();
-    // 목소리(A_VOICE_*)는 3b 에서 잇는다 — 지금은 아래 FSM 줄에 acts 로만 남는다.
     if (o.notice != Notice::NONE) { s_notice = o.notice; s_notice_msg = o.msg; s_notice_until = now + uilayout::notice_ms(o.notice); }   // 거절 문구는 10초, 그 밖은 4초
     if (s_fsm.state != before || o.acts || o.notice != Notice::NONE) {
         char tail[12] = "";                                      // 문구 번호가 있을 때만 줄 끝에 " msg=N"(기존 줄의 글자는 그대로 — DM-D9). 한 번의 printf 로 찍는다(줄이 쪼개지지 않게)
@@ -174,6 +179,7 @@
         const int64_t now = now_ms();
 
         toneout::print_pending();                                // 0) 출력 태스크가 남긴 TONE 줄을 찍는다(PL3b-D1 — 출력 태스크는 찍지 않는다)
+        voice::print_pending();                                  //    지연 재기(vping)의 결과 줄도
 
         relaylink::In in;                                        // 1) 교환기 사건
         while (relaylink::poll(&in)) {
@@ -233,16 +239,26 @@
     relaylink::stack_free(&stk_relay, &stk_ws);
     uint32_t aud_avg = 0, aud_max = 0;
     audioout::block_us(&aud_avg, &aud_max);
-    // PL3a-D11: 소리 칸은 줄 끝에 더한다(앞의 칸은 글자 하나 바꾸지 않는다 — phone_ctl.py·touch_audit.py 의 정규식이 그대로 맞는다)
+    voice::Stat v;
+    voice::stat(&v);
+    // PL3a-D11·PL3b-D16: 새 칸은 줄 끝에 더한다(앞의 칸은 글자 하나 바꾸지 않는다 — phone_ctl.py·touch_audit.py 와 앞 계획의 정규식이 그대로 맞는다).
+    //   목소리 칸(v_…)의 셈·시간은 이번(또는 직전) 통화의 것이다. v_cap 만 부팅 뒤 누적(통화 밖에서 늘면 마이크가 열려 있는 것).
     printf("STAT up=%u unit=%d fsm=%s screen=%s wifi=%d relay=%d relay_code=%d peer=%d missed=%u touch=%u heap_int=%u heap_int_min=%u heap_psram=%u"
-           " stk_app=%u stk_touch=%u stk_relay=%u stk_ws=%u audio=%d tone=%s aud_us=%u/%u aud_err=%u stk_audio=%u\n",
+           " stk_app=%u stk_touch=%u stk_relay=%u stk_ws=%u audio=%d tone=%s aud_us=%u/%u aud_err=%u stk_audio=%u"
+           " amp=%d aud_late=%u voice=%d vsrc=%s v_cap=%u v_tx=%u v_txdrop=%u v_txfail=%u v_rx=%u v_play=%u v_lost=%u v_drop=%u v_under=%u v_jb=%d/%d"
+           " v_aec_us=%u/%u v_enc_us=%u/%u v_dec_us=%u/%u v_mic=%u v_ref=%u v_aec=%u v_spk=%u v_rderr=%u stk_vcap=%u stk_vrx=%u stk_vtx=%u\n",
            (unsigned)(esp_timer_get_time() / 1000000), m.unit, state_name(m.fsm), screen, (int)m.wifi_connected, (int)m.relay_up, m.relay_code,
            (int)m.peer_online, (unsigned)m.missed, (unsigned)s_touches.load(),
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
            s_app_task ? (unsigned)uxTaskGetStackHighWaterMark(s_app_task) : 0, s_touch_task ? (unsigned)uxTaskGetStackHighWaterMark(s_touch_task) : 0,
            stk_relay, stk_ws, (int)audioout::ready(), tones::tone_name(toneout::now()), (unsigned)aud_avg, (unsigned)aud_max,
-           (unsigned)audioout::write_errors(), audioout::stack_free());
+           (unsigned)audioout::write_errors(), audioout::stack_free(),
+           (int)audioout::amp_is_on(), (unsigned)audioout::late_blocks(), (int)v.on, voicecore::src_name(v.src), (unsigned)v.cap,
+           (unsigned)v.tx, (unsigned)v.tx_drop, (unsigned)v.tx_fail, (unsigned)v.rx.frames, (unsigned)v.rx.played, (unsigned)v.rx.concealed,
+           (unsigned)(v.rx.jb.overflow_drop + v.rx.jb.catchup_drop + v.rx.jb.late + v.rx.jb.dup + v.rx.bad), (unsigned)v.rx.under, v.rx.depth, v.rx.target,
+           (unsigned)v.aec_us, (unsigned)v.aec_max, (unsigned)v.enc_us, (unsigned)v.enc_max, (unsigned)v.dec_us, (unsigned)v.dec_max,
+           (unsigned)v.mic_rms, (unsigned)v.ref_rms, (unsigned)v.aec_rms, (unsigned)v.rx.play_rms, (unsigned)v.rd_err, v.stk_cap, v.stk_rx, v.stk_tx);
 }
 
 }  // namespace app
--- a/firmware/tab5_phone/main/phone_main.cpp
+++ b/firmware/tab5_phone/main/phone_main.cpp
@@ -1,11 +1,12 @@
-// phone_main.cpp — Tab5 음성 전화기(Phase 3a: 벨·연결음·종료음까지 — 목소리는 3b). 부팅, 태스크 기동, 시리얼 명령만 한다.
+// phone_main.cpp — Tab5 음성 전화기(Phase 3b: 벨·연결음·종료음 + 목소리 통화). 부팅, 태스크 기동, 시리얼 명령만 한다.
 //
 // 설계: docs/exec-plans/2026-10-03-tab5-phone.md §9.5. 두 대에 같은 펌웨어를 올리고, 기기 번호는 MAC 으로 정한다(device_id).
-// 태스크: app(화면·통화 진행, 코어1) / touch(터치 읽기, 코어1) / audio(스피커 출력, 코어1) / wifi(매니저, 코어0) / relay(교환기 연결 관리, 코어0)
-//        / relay_ws(부품의 웹소켓 태스크) / sercmd(시리얼 명령, 코어0)
+// 태스크: app(화면·통화 진행, 코어1) / touch(터치 읽기, 코어1) / audio(스피커 출력, 코어1) / v_cap(마이크 → 에코 제거 → 압축, 코어1) / v_rx(해제, 코어1)
+//        / v_tx(음성 보내기, 코어0) / wifi(매니저, 코어0) / relay(교환기 연결 관리, 코어0) / relay_ws(부품의 웹소켓 태스크) / sercmd(시리얼 명령, 코어0)
 //
 // 시리얼 명령(115200, 줄 단위) — tools/phone_ctl.py 가 쓴다:
-//   stat | t <x> <y> | shot | reboot | fault audio
+//   stat | t <x> <y> | shot | reboot
+//   진단(사람이 켠다): fault audio | quiet on|off | selftest | vsrc aec|mic|ref|tone | vping     (계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md PL3b-D2·D11·D12·D14)
 #include <M5Unified.h>
 #include <cstdio>
 #include <cstdlib>
@@ -30,6 +31,7 @@
 #include "relay_link.h"
 #include "secrets.h"
 #include "tone_out.h"
+#include "voice.h"
 
 static const char *TAG = "phone";
 
@@ -79,6 +81,28 @@
     return ok;
 }
 
+// PL3b-D14 조용한 모드: 스피커 앰프를 끈 채로 둔다(시리얼 'quiet on' — NVS 에 남아 재부팅 뒤에도 유지, 'quiet off' 로 푼다).
+//   벨·연결음·목소리가 전부 조용하다. 코덱은 그대로 돌아 마이크 칩의 되먹임 칸에는 소리가 잡힌다 → 밤이나 사용자 없이도 소리 경로를 기록으로 확인한다.
+//   사람이 켠 것만. 켜 둔 채 잊으면 벨이 안 울린다 — STAT 의 amp=0 과 부팅 줄 "AUDIO ready … amp=0" 으로 보인다.
+static const char *QUIET_KEY = "quiet";
+static bool quiet_get()
+{
+    nvs_handle_t h; uint8_t v = 0;
+    if (nvs_open("phone", NVS_READONLY, &h) != ESP_OK) return false;
+    nvs_get_u8(h, QUIET_KEY, &v);
+    nvs_close(h);
+    return v == 1;
+}
+static bool quiet_set(bool on)
+{
+    nvs_handle_t h;
+    if (nvs_open("phone", NVS_READWRITE, &h) != ESP_OK) return false;
+    const esp_err_t e = on ? nvs_set_u8(h, QUIET_KEY, 1) : nvs_erase_key(h, QUIET_KEY);
+    const bool ok = (e == ESP_OK || (!on && e == ESP_ERR_NVS_NOT_FOUND)) && nvs_commit(h) == ESP_OK;
+    nvs_close(h);
+    return ok;
+}
+
 // 화면을 JPEG 로 찍어 시리얼로 내보낸다. 받는 쪽(phone_ctl.py)이 크기와 검사합을 확인한다.
 static void dump_shot()
 {
@@ -111,6 +135,23 @@
     else if (cmd == "shot") dump_shot();
     else if (cmd == "reboot") { printf("OK reboot\n"); fflush(stdout); vTaskDelay(pdMS_TO_TICKS(200)); esp_restart(); }
     else if (cmd == "fault audio") printf(fault_audio_set(true) ? "OK fault audio\n" : "ERR fault audio\n");   // 다음 부팅부터(PL3b-D2). 'reboot' 로 시작한다
+    else if (cmd == "quiet on" || cmd == "quiet off") {         // PL3b-D14: 저장하고 앰프를 그 자리에서 끄거나 켠다
+        const bool q = cmd == "quiet on";
+        const bool saved = quiet_set(q), set = audioout::amp(!q);
+        printf("%s quiet %s amp=%d\n", saved && set ? "OK" : "ERR", q ? "on" : "off", (int)audioout::amp_is_on());
+    }
+    else if (cmd == "selftest") {                               // PL3b-D14: 통화 밖에서만. 시험음 0.64초(조용한 모드면 소리 없이)
+        voice::SelfTest r;
+        if (voice::selftest(&r)) printf("SELFTEST amp=%d mic=%u ref=%u aec=%u ok=%d\n", (int)audioout::amp_is_on(), (unsigned)r.mic, (unsigned)r.ref, (unsigned)r.aec,
+                                        (int)(r.ref >= voicecore::SELFTEST_REF_MIN));
+        else printf("SELFTEST FAIL busy-or-no-audio\n");
+    }
+    else if (cmd.rfind("vsrc ", 0) == 0) {                      // PL3b-D11: 통화 중에만, 통화가 끝나면 aec 로 돌아간다
+        voicecore::Src s;
+        if (voicecore::parse_src(cmd.c_str() + 5, &s) && voice::set_source(s)) printf("OK vsrc %s\n", voicecore::src_name(s));
+        else printf("ERR vsrc\n");
+    }
+    else if (cmd == "vping") printf(voice::ping() ? "OK vping\n" : "ERR vping\n");   // PL3b-D12: 결과는 "VPING rt_ms=…" 줄로(1초쯤 뒤)
     else printf("ERR unknown\n");
     fflush(stdout);
 }
@@ -170,8 +211,9 @@
     if (gfx_ok) {
         const bool injected = fault_audio_armed();
         if (injected) audioout::fail("injected");
-        else audioout::start(i2c_lock);
+        else audioout::start(i2c_lock, !quiet_get());           // PL3b-D14: 조용한 모드면 앰프를 켜지 않는다
         toneout::start();
+        voice::start();                                         // 에코 제거·Opus·태스크 셋(잠든 채). 실패하면 "AUDIO FAIL voice"
         boot_guard(audioout::ready(), "audio");
         if (injected) fault_audio_set(false);   // 여기까지 왔으면 세 번 다시 켜 보고 포기한 것이다 — 시험은 한 번으로 끝. 다음 부팅은 정상
     }
```

- [x] **Step 5: 이은 자리를 코드와 하나씩 대조한다** (맥 시험이 없는 파일들이다 — 읽어서 확인하고 보고에 적는다)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone/main && for f in app.cpp phone_main.cpp voice.cpp; do grep -Hn "toneout::play(tone);\|voicecore::from_acts(o.acts, &talk) && voice::set(talk)\|relaylink::send_text(text)\|printf(\"FSM \|voice::print_pending();\|audioout::start(i2c_lock, !quiet_get())\|toneout::start();\|voice::start();\|boot_guard(audioout::ready(), \"audio\")\|if (mode == Mode::OFF) { was = Mode::OFF;\|if (s_mode.load() != Mode::CALL) continue;\|s_mode = Mode::OFF; \|s_rx->stop();\|relaylink::set_voice_sink(on_frame);\|s_codec_fail = codec_setup();" "$f" | cut -c1-100; done; echo "A_VOICE_ 를 직접 보는 줄: $(grep -c 'A_VOICE_' app.cpp)"
```
Expected:
```
app.cpp:83:    if (tones::from_acts(o.acts, &tone)) toneout::play(tone);   // PL3a-D13: 보내기보다 먼저. 소리
app.cpp:85:    if (voicecore::from_acts(o.acts, &talk) && voice::set(talk)) printf("VOICE %s\n", tal
app.cpp:92:        if (!relaylink::send_text(text)) ESP_LOGW(TAG, "signal 0x%x not sent (link down)"
app.cpp:99:        printf("FSM %s -> %s acts=0x%x notice=%s%s\n", state_name(before), state_name(s_f
app.cpp:182:        voice::print_pending();                                  //    지연 재기(vping)의 결과 
phone_main.cpp:214:        else audioout::start(i2c_lock, !quiet_get());           // PL3b-D14: 조용한 
phone_main.cpp:215:        toneout::start();
phone_main.cpp:216:        voice::start();                                         // 에코 제거·Opus·태스크
phone_main.cpp:217:        boot_guard(audioout::ready(), "audio");
voice.cpp:165:        if (s_mode.load() != Mode::CALL) continue;              // 통화가 끝났다 — 남은 것은 보내지
voice.cpp:197:    s_codec_fail = codec_setup();
voice.cpp:209:        if (mode == Mode::OFF) { was = Mode::OFF; ulTaskNotifyTake(pdTRUE, portMAX_DEL
voice.cpp:285:    relaylink::set_voice_sink(on_frame);
voice.cpp:322:    s_mode = Mode::OFF;                                         // 캡처 태스크는 읽던 묶음(최대 16
voice.cpp:323:    s_rx->stop();                                               // 받는 쪽: 더 받지 않는다. 출력은
A_VOICE_ 를 직접 보는 줄: 2
```

| 확인할 것 | 코드 |
|---|---|
| `apply()` 의 순서 = ① 신호음 ② 목소리 ③ 신호 보내기 ④ `FSM` 줄(PL3b-D13). 그래서 `VOICE` 줄은 늘 그 원인인 `FSM` 줄 바로 앞이고, 끊기를 누르면 보내기가 막혀 있어도 마이크가 먼저 닫힌다 | `app.cpp:83` → `85` → `92` → `99` |
| 목소리를 켜고 끄는 것은 `from_acts` 한 곳으로만 정한다(`app.cpp` 에 `A_VOICE_` 를 직접 보는 코드가 없다 — 위의 `2` 는 머리 주석 두 줄) | `grep -n "A_VOICE_" app.cpp` |
| `VOICE on\|off` 는 상태가 **실제로 바뀌었을 때만** 찍는다(`voice::set` 의 돌려주는 값). 소리 장치가 없으면 찍지 않는다 | `app.cpp:85`, `voice.cpp` `set()` 의 `if (!s_ready) return false;` |
| `STAT` 의 앞 칸은 글자 하나 바뀌지 않고 끝에 붙는다. 넘기는 인자 순서 = 형식 순서 | `app.cpp` `print_stat` — 도구 시험 `test_firmware_stat_keeps_every_old_field_and_only_appends` 가 형식을, 이 표가 인자 순서를 본다: `amp` `aud_late` `voice` `vsrc` `v_cap` `v_tx` `v_txdrop` `v_txfail` `v_rx` `v_play` `v_lost` `v_drop` `v_under` `v_jb`(깊이·목표) `v_aec_us`(평균·최대) `v_enc_us` `v_dec_us` `v_mic` `v_ref` `v_aec` `v_spk` `v_rderr` `stk_vcap` `stk_vrx` `stk_vtx` |
| `v_drop` = 넘쳐 버림 + 따라잡느라 버림 + 늦게 옴 + 중복 + 규격 밖 | `app.cpp` 의 `v.rx.jb.overflow_drop + v.rx.jb.catchup_drop + v.rx.jb.late + v.rx.jb.dup + v.rx.bad` |
| 시작 순서: `audioout::start` → `toneout::start` → `voice::start` → `boot_guard(…, "audio")` → Wi-Fi → 교환기 → app. 조용한 모드면 앰프를 켜지 않고 연다 | `phone_main.cpp:214~217` |
| 진단 명령은 시리얼 태스크에서만 닿는다. `vsrc`·`vping` 은 통화 중이 아니면 `ERR`, `selftest` 는 통화 중이면 `SELFTEST FAIL …` | `phone_main.cpp` `serial_cmd` 의 해당 가지, `voice.cpp` `set_source`·`ping`·`selftest` |
| `quiet on\|off` 는 NVS 에 적고 앰프를 그 자리에서 끄거나 켠다. 답에 지금 앰프 상태가 실린다 | `phone_main.cpp` 의 `quiet_set`·`audioout::amp(!q)` |

- [x] **Step 6: 전체 맥 시험·도구 시험·교환기 시험을 돌리고 커밋한다**

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone/tools && SDK="$(xcrun --sdk macosx --show-sdk-path)" && for T in wifi_store call_fsm relay_proto device_id ui_layout tones pcm jitter_buffer voice_core; do clang++ -std=c++17 -Wall -Wextra -isysroot "$SDK" -I../main test_$T.cpp -o /tmp/t_p3b_$T 2> /tmp/t_p3b_$T.err; echo "$T: $(/tmp/t_p3b_$T | tail -1) (경고 $(wc -l < /tmp/t_p3b_$T.err | tr -d ' ')줄)"; done; PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider test_phone_ctl.py 2>&1 | tail -1; cd ~/tab5-toy-phone/server && PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider 2>&1 | tail -1
```
Expected: `wifi_store: 16 passed, 0 failed (경고 0줄)` · `call_fsm: 105 ok, 0 failed (경고 0줄)` · `relay_proto: 61 ok, 0 failed (경고 0줄)` · `device_id: 9 ok, 0 failed (경고 0줄)` · `ui_layout: 91 ok, 0 failed (경고 0줄)` · `tones: 89 ok, 0 failed (경고 0줄)` · `pcm: 37 ok, 0 failed (경고 0줄)` · `jitter_buffer: 65 ok, 0 failed (경고 0줄)` · `voice_core: 117 ok, 0 failed (경고 0줄)` · `218 passed in …s` · `137 passed in …s`

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/tools/test_phone_ctl.py firmware/tab5_phone/main/app.h firmware/tab5_phone/main/app.cpp firmware/tab5_phone/main/phone_main.cpp && git commit -m "feat(phone): 목소리 연결 — A_VOICE_START·STOP 실행(받으면 열리고 끊으면 닫힌다), STAT 끝에 목소리 칸, 조용한 모드·혼자 하는 점검·지연 재기 명령 + 도구 시험(새 STAT 모양을 펌웨어 소스에서 읽어 고정)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: 지금 기기의 빌드를 보관하고, 새로 빌드해 "올릴 바이너리"를 확정한다

**Files:**
- Create(git 제외): `firmware/tab5_phone/out/fw_prev_5d9935f/build/`(지금 기기에 올라간 빌드의 보관본), `out/p3b_build.log`, `out/p3b_fw_build.sha256`, `out/p3b_fw_version.txt`

**Interfaces:**
- Consumes: Task 1~6 의 커밋 아홉(저장소 HEAD), 저장소의 `firmware/tab5_phone/build/`(지금은 두 기기에 올라가 있는 `5d9935f` 빌드), 3a 가 적어 둔 검사합 `out/p3a_fw_build.sha256`.
- Produces: Task 8 이 올릴 `build/`, 그 검사합 `out/p3b_fw_build.sha256`, 버전 `out/p3b_fw_version.txt`, 롤백용 `out/fw_prev_5d9935f/build/`(기존 `out/fw_prev_066cf57/build/` 와 같은 구조).

- [x] **Step 1: 지금 기기에 올라가 있는 빌드를 보관한다** (PL3b-D18 — 다음 Step 이 `build/` 를 덮어쓴다)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && grep '"project_version"' build/project_description.json && mkdir -p out/fw_prev_5d9935f/build/bootloader out/fw_prev_5d9935f/build/partition_table && cp build/flash_args build/tab5_phone.bin out/fw_prev_5d9935f/build/ && cp build/bootloader/bootloader.bin out/fw_prev_5d9935f/build/bootloader/ && cp build/partition_table/partition-table.bin out/fw_prev_5d9935f/build/partition_table/ && shasum -a 256 out/fw_prev_5d9935f/build/tab5_phone.bin | cut -c1-64 && shasum -a 256 -c out/p3a_fw_build.sha256 && git status --short | grep -c "fw_prev"
```
Expected: `    "project_version":    "5d9935f",`, `4d15d9d1a8cbf4fdd5738b2d710d9e6415103e2c472c902c3e5f515bf56a80fa`, `build/tab5_phone.bin: OK` 등 `OK` 3줄(3a 빌드를 두 기기에 올릴 때 적어 둔 검사합과 같다), `0`(git 에 보이지 않는다). **버전이 `5d9935f` 가 아니거나 검사합이 다르면 멈춘다(BLOCKED)** — 누군가 그 뒤에 다시 빌드한 것이라 보관본이 "기기에 올라간 것"이라는 보장이 없다(이때는 아무것도 지우지 말고 보고한다).

- [x] **Step 2: 전체 빌드** (export 와 빌드를 같은 호출에. 1~3분)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && shasum -a 256 dependencies.lock | cut -c1-64 > /tmp/p3b_lock_before.txt && source ~/esp/esp-idf-5.5.4/export.sh > /dev/null 2>&1 && idf.py build > out/p3b_build.log 2>&1; echo "rc=$?"; grep -E "tab5_phone.bin binary size|Project build complete" out/p3b_build.log; echo "우리 코드 경고: $(grep 'warning:' out/p3b_build.log | grep -c 'tab5_phone/main/')"; echo "오류: $(grep -c 'error:' out/p3b_build.log)"; for c in esp_hosted:2.11.7 esp_audio_codec:2.5.0 esp_codec_dev:1.6.2 esp-sr:2.5.5 esp_websocket_client:1.8.0; do n=${c%%:*}; v=${c##*:}; echo "$n $v: $(sed -n "/^  espressif\/$n:/,/^  [a-z]/p" dependencies.lock | grep -c "^    version: $v$")"; done; shasum -a 256 dependencies.lock | cut -c1-64 | cmp - /tmp/p3b_lock_before.txt && echo "dependencies.lock 그대로"
```
Expected(사본에서 이 명령 그대로 돌린 출력 — 자리 값 `secrets.h`):
```
rc=0
tab5_phone.bin binary size 0x287720 bytes. Smallest app partition is 0x600000 bytes. 0x3788e0 bytes (58%) free.
Project build complete. To flash, run:
우리 코드 경고: 0
오류: 0
esp_hosted 2.11.7: 1
esp_audio_codec 2.5.0: 1
esp_codec_dev 1.6.2: 1
esp-sr 2.5.5: 1
esp_websocket_client 1.8.0: 1
dependencies.lock 그대로
```
크기 줄의 두 숫자만 저장소에서 조금 다르다: `secrets.h` 가 달라 사본보다 0x3e0 쯤 크게 나온다(3a 때 사본 0x242230 ↔ 저장소 0x242610). **0x287000~0x289000 사이면 정상**, 벗어나면 보고에 적는다. `(58%) free` 와 나머지 줄은 같아야 한다.
`rc` 가 0 이 아니면 `grep -m5 "error:" out/p3b_build.log` 를 보고에 붙이고 멈춘다(코드를 고치지 않는다).

- [x] **Step 3: 검사합과 버전을 적어 둔다**

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && shasum -a 256 build/tab5_phone.bin build/bootloader/bootloader.bin build/partition_table/partition-table.bin > out/p3b_fw_build.sha256 && grep '"project_version"' build/project_description.json | tee out/p3b_fw_version.txt && git log --oneline -1 && cut -c1-12 out/p3b_fw_build.sha256 && cmp -s build/tab5_phone.bin out/fw_prev_5d9935f/build/tab5_phone.bin; echo "보관본과 다른가(1 이어야 한다): $?"
```
Expected: `"project_version":    "<지금 HEAD 의 짧은 SHA>"`(바로 아래 `git log` 줄의 SHA 와 같다. 끝에 `-dirty` 가 붙으면 작업 트리에 커밋 안 된 변경이 있다는 뜻 — `git status --short` 를 보고에 적는다. 올리는 데는 지장 없다), 검사합 세 줄, `보관본과 다른가(1 이어야 한다): 1`. **이 뒤로 Task 9 가 끝날 때까지 `idf.py build`·`idf.py flash` 를 돌리지 않는다**(바이너리가 달라진다).

**묶음 2 끝 — 여기서 검수가 들어간다**(Task 4~6 의 커밋 넷 + Task 7 의 빌드 기록). 검수가 끝난 뒤에 Task 8 로 간다.

---

### Task 8: 기기 검증 A — 두 대에 올리고, **스피커를 끈 채로** 소리 경로를 기록으로 확인한다

> 2026-10-05 기록: 이 과제는 `d77ae98` 로 밤에 수행했다(보고 `task-8-report.md`). 그 뒤 올릴 빌드가 `e40d097`(Task 8b — 점검 R0~R7)과 `5935ddd`(fix 3)로 바뀌어 두 기기에 두 번 더 올렸다. 각 Step 의 결과와 다시 돌린 횟수는 진행 로그.

**Files:**
- Create(git 제외): `firmware/tab5_phone/out/p3b_*.log`, `out/p3b_flash_unit*.txt`

**Interfaces:**
- Consumes: Task 7 의 `build/`·`out/p3b_fw_build.sha256`·`out/p3b_fw_version.txt`·`out/fw_prev_5d9935f/`, `tools/phone_ctl.py`, `D/verify/voice_check.py`·`touch_audit.py`·`secret_scan.py`, 켜져 있는 개발용 교환기(바꾸지 않는다).
- Produces: 부팅 줄(`AUDIO ready … amp=0`·`VOICE ready …`)과 혼자 하는 점검, 소리 장치 실패 경로, 시험음 통화, 지연(왕복 `VPING`), 30초·3분 통화의 손실·부하·여유, 상대 재부팅, 연달아 통화, 3a 시나리오 회귀의 통과 기록.

모든 명령은 ESP-IDF 를 export 하지 **않은** 호출에서 돌린다(플래시 Step 만 export). 좌표: 통화 `640,360` · 취소 `640,604` · 받기 `404,428` · 거절 `876,428` · 끊기 `644,606` · 벨 끄기 `404,624` · 메시지로 거절 `876,624` · 문구 2 `640,168`.
각 실행의 통과 조건은 공통이다: **`rc=0` 이고 `!!` 줄이 없고(첫 숫자 `0`), 요약 줄이 Expected 와 글자까지 같고, `voice_check` 가 `check rc=0`** (`check rc=3` 은 내부 메모리만 기준 아래라는 뜻이다 — 멈추지 않고 그 `??` 줄을 보고에 적는다. "CPU·메모리 예산"의 탈출구를 컨트롤러가 정한다). 같은 실행이 실패하면 로그로 원인을 확인하고 그 실행만 다시 돌린다(3회까지. 세 번 모두 실패하면 멈춘다).
`I`·`Z` 는 실행의 처음·끝에 두 기기가 "대기 중, 서로 보임, 신호음 없음, 스피커 꺼짐, 마이크 닫힘"인지 보는 단계다(각 명령의 앞에 적혀 있다).
**이 과제 동안 스피커는 꺼져 있다** — Step 3·4 에서 조용한 모드를 건 뒤에는 벨도 목소리도 나지 않는다(다시 올린 직후의 첫 부팅 때 앰프가 켜지는 "퍽" 소리 한 번은 날 수 있다). 조용한 모드는 이 과제가 끝나도 풀지 않는다.

- [x] **Step 1: 사전 확인** — 컨트롤러가 사용자에게 "약 25분 동안 두 기기 화면을 만지지 말아 달라, 소리는 나지 않는다"고 알린 뒤에(자리에 없으면 그대로) 시작한다

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && shasum -a 256 -c out/p3b_fw_build.sha256 && cat out/p3b_fw_version.txt && shasum -a 256 out/fw_prev_5d9935f/build/tab5_phone.bin | cut -c1-12 && P1=$(python3 tools/phone_ctl.py port 1) && P2=$(python3 tools/phone_ctl.py port 2) && echo "1번 $P1 / 2번 $P2" && lsof "$P1" "$P2" 2>/dev/null | grep -c usbmodem
```
Expected: `build/tab5_phone.bin: OK` 등 `OK` 3줄, 버전 한 줄, `4d15d9d1a8cb`(롤백용 보관본이 있다), 포트 경로 두 개, `0`(두 포트를 열고 있는 다른 프로그램 없음). 하나라도 다르면 멈춘다(BLOCKED) — 검사합이 `FAILED` 면 Task 7 Step 3 뒤에 누군가 다시 빌드한 것이다.

Run(단독 호출): `bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh status`
Expected: `relay: listening on port 5094, healthz={"ok": true}`. 꺼져 있으면 켜지 말고 멈춘다(BLOCKED — 컨트롤러가 켠다).

- [x] **Step 2: 올리기 전의 값을 남긴다** (목소리 없는 `5d9935f` 의 메모리 — Step 13 에서 비교한다)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1:60' 'sleep:3' 'both:until:stat:^STAT .*fsm=IDLE:10' > out/p3b_before.log 2>&1; echo "rc=$?"; for u in 1 2; do grep -E "^\[$u\] STAT " out/p3b_before.log | tail -1 | grep -oE '^\[.\]|heap_int=[0-9]+|heap_int_min=[0-9]+|stk_[a-z]+=[0-9]+' | tr '\n' ' '; echo; done
```
Expected: `rc=0`, 기기마다 한 줄(`[1] heap_int=… heap_int_min=… stk_app=… stk_touch=… stk_relay=… stk_ws=… stk_audio=…`). 값을 보고에 적는다(3a 때: `heap_int` 약 257000). `rc=1` 이면(두 기기가 대기 상태가 아니다 — 사용자가 쓰는 중이거나 교환기에 못 붙음) 멈춘다.

- [x] **Step 3: 2번에 새 펌웨어를 올리고, 조용한 모드를 걸고, 부팅 줄과 혼자 하는 점검을 본다**

올리기(export 한 호출):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && source ~/esp/esp-idf-5.5.4/export.sh > /dev/null 2>&1 && PORT=$(python3 tools/phone_ctl.py port 2) && echo "2번 포트: $PORT" && python -m esptool --chip esp32p4 -p "$PORT" read_mac 2>&1 | tee out/p3b_flash_unit2.txt | grep -i "aa:bb:cc:dd:ee:02" && cd build && python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash @flash_args 2>&1 | tee -a ../out/p3b_flash_unit2.txt | grep -E "Hash of data verified|Hard resetting|rror"
```
Expected: `2번 포트: /dev/cu.usbmodem…`, `MAC: aa:bb:cc:dd:ee:02`, `Hash of data verified.` 3줄, `Hard resetting via RTS pin...`. MAC 줄이 안 나오면 아무것도 쓰이지 않는다(명령이 거기서 끝난다) — 멈춘다.

조용한 모드 → 재부팅 → 부팅 줄 → 점검(export 하지 않은 호출):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run 'until:stat:^STAT :60' 'until:quiet on:^OK quiet on amp=0$:10' 'reboot' 'wait:^PHONE BOOT unit=2 id=tab5-2:30' 'wait:^AUDIO (ready|FAIL):20' 'wait:^VOICE ready:10' 'until:stat:^STAT .*fsm=IDLE.*relay=1.*audio=1 .* amp=0 :120' 'selftest' 'wait:^SELFTEST :5' 'until:stat:^STAT .*fsm=IDLE:10' > out/p3b_boot_unit2.log 2>&1; echo "rc=$?"; grep -E "App version|Compile time|ELF file SHA256|^PHONE BOOT|^PHONE FAIL|^PHONE audio-failed|^AUDIO|^VOICE ready|^SELFTEST|^OK quiet|^ERR quiet" out/p3b_boot_unit2.log | sed 's/^.*app_init: //' | tail -8; grep -E "I2S_IF|dma frame num|ES8388|ES7210|Adev_Codec" out/p3b_boot_unit2.log | sed 's/^.*) //' | tail -12; grep "^STAT" out/p3b_boot_unit2.log | tail -1 | grep -oE 'fsm=[A-Z_]+|relay=[01]|audio=[01]|tone=[A-Z]+|amp=[01]|voice=[01]|v_cap=[0-9]+|heap_int=[0-9]+|heap_int_min=[0-9]+|stk_[a-z]+=[0-9]+' | tr '\n' ' '; echo
```
Expected: `rc=0`, 이어서 여덟 줄:
```
OK quiet on amp=0
App version:      <out/p3b_fw_version.txt 의 버전>
Compile time:     …
ELF file SHA256:  …
PHONE BOOT unit=2 id=tab5-2
AUDIO ready rate=16000 block=256 vol=100 dma=3 mic=24 amp=0
VOICE ready chunk=256 opus_in=640 opus_out_max=160 bitrate=24000 aec=VOIP_HIGH_PERF
SELFTEST amp=0 mic=<작은 수> ref=<300~1200> aec=<작은 수> ok=1
```
마지막 줄: `fsm=IDLE relay=1 heap_int=… heap_int_min=… stk_app=… stk_touch=… stk_relay=… stk_ws=… audio=1 tone=NONE stk_audio=… amp=0 voice=0 v_cap=<수십> stk_vcap=… stk_vrx=… stk_vtx=…` — `stk_*` 가 모두 1024 이상인지 보고, **`heap_int`·`heap_int_min`·`stk_vcap`·`stk_vrx`·`SELFTEST` 의 `mic`(조용한 값 — Task 9 에서 비교한다)·`ref` 를 보고에 적는다**. `heap_int` 의 예산은 약 105~120KB 다("CPU·메모리 예산"). 100000 아래여도 60000 이상이면 계속한다(보고에 적는다).
- 읽는 법 — `AUDIO ready … dma=3 mic=24 amp=0`: 스피커와 마이크 장치가 열렸고, 마이크 증폭 24dB 가 칩에서 되읽혔고, 앰프는 꺼져 있다. `VOICE ready …`: 에코 제거(묶음 256)와 Opus(입력 640바이트)가 Phase 0 과 같은 값으로 열렸다. `SELFTEST … ref=<300~1200> ok=1`: 시험음(1kHz·크기 2000)을 내는 동안 스피커 직전의 되먹임 칸에 그 소리가 잡혔다 = **코덱이 실제로 소리를 내고 있다**(Phase 0 실측 비율로 어림 590. 앰프가 꺼져 있어 스피커에서는 나지 않고 `mic` 는 방의 잡음뿐이다).
- 가운데의 부품 로그는 참고다(로그 수준에 따라 안 보일 수 있다 — 없어도 실패가 아니다). 보이면 Phase 0 로그(`tab5_phone_probe/out/task4.log` 177~187)와 같은 모양이어야 한다: 송신이 처음에는 `I2S_IF: STD: TX, data_bit: 16, slot_bit: 16 …`, 입력을 열 때 `slot_bit: 32` 로 한 번 다시 설정된다, `I2S_IF: TDM: RX … slot_mask: 0xf`, `ES8388: Start on mode:2`, `Adev_Codec: Open codec device OK` 두 번. `dma frame num is adjusted` 줄은 **없어야** 한다(있으면 보고에 적는다).
- **멈추는 경우(BLOCKED — 1번에는 올리지 않는다)**: `AUDIO FAIL <단계>`(이때 `PHONE audio-failed restart …` 줄이 따라온다)·`PHONE FAIL`·`ERR quiet`·`SELFTEST FAIL`·`ok=0` 이 보이거나, `PHONE BOOT` 가 되풀이되거나(재부팅 고리), `rc` 가 0 이 아니거나, `heap_int_min` 이 60000 아래일 때. 보고에 `<단계>` 와 그 앞뒤 30줄을 붙인다("결함이 나오면" 참조).

- [x] **Step 4: 1번에 같은 바이너리를 올리고 같은 확인을 한다** (다시 빌드하지 않는다)

올리기(export 한 호출):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && shasum -a 256 -c out/p3b_fw_build.sha256 && source ~/esp/esp-idf-5.5.4/export.sh > /dev/null 2>&1 && PORT=$(python3 tools/phone_ctl.py port 1) && echo "1번 포트: $PORT" && python -m esptool --chip esp32p4 -p "$PORT" read_mac 2>&1 | tee out/p3b_flash_unit1.txt | grep -i "aa:bb:cc:dd:ee:01" && cd build && python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash @flash_args 2>&1 | tee -a ../out/p3b_flash_unit1.txt | grep -E "Hash of data verified|Hard resetting|rror"
```
Expected: `OK` 3줄, `1번 포트: …`, `MAC: aa:bb:cc:dd:ee:01`, `Hash of data verified.` 3줄, `Hard resetting via RTS pin...`.

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py --unit 1 run 'until:stat:^STAT :60' 'until:quiet on:^OK quiet on amp=0$:10' 'reboot' 'wait:^PHONE BOOT unit=1 id=tab5-1:30' 'wait:^AUDIO (ready|FAIL):20' 'wait:^VOICE ready:10' 'until:stat:^STAT .*fsm=IDLE.*relay=1.*audio=1 .* amp=0 :120' 'selftest' 'wait:^SELFTEST :5' 'until:stat:^STAT .*fsm=IDLE:10' > out/p3b_boot_unit1.log 2>&1; echo "rc=$?"; for u in 1 2; do grep -hE "^AUDIO|^VOICE ready|^SELFTEST" out/p3b_boot_unit$u.log | tail -3; done; grep -hE "App version|Compile time|ELF file SHA256" out/p3b_boot_unit1.log out/p3b_boot_unit2.log | sed 's/^.*app_init: //' | sort -u; grep "^STAT" out/p3b_boot_unit1.log | tail -1 | grep -oE 'heap_int=[0-9]+|heap_int_min=[0-9]+|stk_[a-z]+=[0-9]+|amp=[01]' | tr '\n' ' '; echo
```
Expected: `rc=0`, 두 기기 각각 `AUDIO ready rate=16000 block=256 vol=100 dma=3 mic=24 amp=0` · `VOICE ready chunk=256 opus_in=640 opus_out_max=160 bitrate=24000 aec=VOIP_HIGH_PERF` · `SELFTEST amp=0 … ok=1`(모두 여섯 줄), 그리고 **정확히 세 줄**(`App version: …`, `Compile time: …`, `ELF file SHA256: …` 가 한 줄씩 = 두 기기의 값이 같다. 네 줄 이상이면 두 기기의 바이너리가 다르다 — 멈춘다), 1번의 여유 한 줄. 버전 세 줄과 1번의 값을 보고에 적는다. 멈추는 경우는 Step 3 과 같다.

- [x] **Step 5: 소리 장치가 안 켜질 때** — 일부러 실패를 내 본다(2번): 세 번 다시 켜 보고, 소리 없이 계속 돌고, 그 상태로도 통화 흐름은 되고, 재부팅하면 돌아온다 (약 2분)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run 'until:stat:^STAT .*fsm=IDLE.*relay=1.*audio=1 .* amp=0 :60' 'fault audio' 'wait:^OK fault audio$:5' 'reboot' 'wait:^PHONE audio-failed restart 3$:60' 'wait:^PHONE FAIL audio$:30' 'until:stat:^STAT .*fsm=IDLE.*relay=1.*audio=0 :120' > out/p3b_fault.log 2>&1; echo "rc=$?"; echo "주입 뒤: PHONE BOOT $(awk '/^OK fault audio/{f=1} f' out/p3b_fault.log | grep -c '^PHONE BOOT')번, AUDIO FAIL injected $(grep -c '^AUDIO FAIL injected' out/p3b_fault.log)번"; grep -E "^PHONE (audio-failed|FAIL)" out/p3b_fault.log | paste -sd, -; grep "^STAT" out/p3b_fault.log | tail -1 | grep -oE 'fsm=[A-Z_]+|relay=[01]|audio=[01]|tone=[A-Z]+|amp=[01]|voice=[01]|stk_vcap=[0-9]+' | tr '\n' ' '; echo
```
Expected: `rc=0`, `주입 뒤: PHONE BOOT 4번, AUDIO FAIL injected 4번`, `PHONE audio-failed restart 1,PHONE audio-failed restart 2,PHONE audio-failed restart 3,PHONE FAIL audio`, `fsm=IDLE relay=1 audio=0 tone=NONE amp=0 voice=0 stk_vcap=0`. (네 번 부팅했고, 네 번 모두 소리 장치를 열지 않았고, 세 번 재시작한 뒤 포기했고, 그래도 교환기에 붙어 대기 중이다.)

소리 없는 채로 통화 한 번(1번이 걸고 2번이 받고 1번이 끊는다):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair '1:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE.* amp=0 .* voice=0 :60' '2:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=0 :60' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '1:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' 'sleep:2' \
  '2:tap:404:428' '2:wait:^FSM INCOMING -> ANSWERING acts=0x82 notice=NONE$:10' '2:wait:^FSM ANSWERING -> IN_CALL acts=0x180 notice=NONE$:10' '1:wait:^VOICE on$:10' '1:wait:^FSM RINGBACK -> IN_CALL acts=0x180 notice=NONE$:10' 'sleep:3' \
  '1:until:stat:^STAT .*fsm=IN_CALL.*tone=NONE.* voice=1 :10' '2:until:stat:^STAT .*fsm=IN_CALL.*audio=0 .* voice=0 :10' \
  '1:tap:644:606' '1:wait:^VOICE off$:10' '1:wait:^FSM IN_CALL -> ENDING acts=0x248 notice=NONE$:10' '2:wait:^FSM IN_CALL -> IDLE acts=0x240 notice=CALL_ENDED$:10' \
  'sleep:2' 'both:until:stat:^STAT .*fsm=IDLE.*peer=1.* voice=0 :15' > out/p3b_fault_call.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3b_fault_call.log; echo "2번의 VOICE·TONE 줄: $(grep -cE '^\[2\] (VOICE|TONE) ' out/p3b_fault_call.log)"; grep -E "^\[1\] STAT " out/p3b_fault_call.log | tail -1 | grep -oE 'v_tx=[0-9]+|v_rx=[0-9]+|v_under=[0-9]+' | tr '\n' ' '; echo
```
Expected: `rc=0`, `0`, `2번의 VOICE·TONE 줄: 0`(소리 장치가 죽은 기기는 목소리도 신호음도 켜지 않는다 — 통화 흐름만 돈다), `v_tx=<100 이상> v_rx=0 v_under=0`(1번은 보냈고, 2번이 아무것도 보내지 않아 받은 것은 없다).
첫 단계에서 실패하면(2번이 `audio=1` 로 돌아와 있다 = 포트를 열 때 재시작됐다) 위의 주입 명령부터 다시 한다.

재부팅하면 돌아온다(조용한 모드도 그대로):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run 'until:stat:^STAT :30' 'reboot' 'wait:^PHONE BOOT unit=2 id=tab5-2:30' 'wait:^AUDIO (ready|FAIL):20' 'wait:^VOICE ready:10' 'until:stat:^STAT .*fsm=IDLE.*relay=1.*audio=1 .* amp=0 :120' > out/p3b_fault_recover.log 2>&1; echo "rc=$?"; grep -E "^AUDIO|^VOICE ready|^PHONE (FAIL|audio-failed)" out/p3b_fault_recover.log | tail -3
```
Expected: `rc=0`, `AUDIO ready rate=16000 block=256 vol=100 dma=3 mic=24 amp=0`, `VOICE ready chunk=256 opus_in=640 opus_out_max=160 bitrate=24000 aec=VOIP_HIGH_PERF` 두 줄뿐(`PHONE FAIL`·`audio-failed` 줄이 없다 = 실패 표시가 스스로 지워졌다).

- [x] **Step 6: 시험음 통화** — 마이크 대신 1kHz 시험음을 양쪽에서 보낸다: 압축 → 교환기 → 해제 → 코덱 출력까지를 숫자로 본다(1번이 걸고 2번이 받고 1번이 끊는다)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft; I='both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE.* amp=0 .* voice=0 :60'; Z='both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE.* voice=0 vsrc=aec :15'; python3 tools/phone_ctl.py pair "$I" \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '1:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' \
  '2:until:stat:^STAT .*fsm=INCOMING.*tone=BELL.* voice=0 :5' 'sleep:2' \
  '2:tap:404:428' '2:wait:^FSM INCOMING -> ANSWERING acts=0x82 notice=NONE$:10' '2:wait:^VOICE on$:10' '2:wait:^FSM ANSWERING -> IN_CALL acts=0x180 notice=NONE$:10' \
  '1:wait:^VOICE on$:10' '1:wait:^FSM RINGBACK -> IN_CALL acts=0x180 notice=NONE$:10' \
  'both:until:vsrc tone:^OK vsrc tone$:5' 'sleep:8' 'both:until:stat:^STAT .*fsm=IN_CALL.*tone=NONE.* voice=1 vsrc=tone :10' \
  '1:tap:644:606' '1:wait:^VOICE off$:10' '1:wait:^FSM IN_CALL -> ENDING acts=0x248 notice=NONE$:10' '2:wait:^VOICE off$:10' '2:wait:^FSM IN_CALL -> IDLE acts=0x240 notice=CALL_ENDED$:10' \
  'sleep:2' "$Z" > out/p3b_tone.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3b_tone.log; python3 "$D/verify/voice_check.py" out/p3b_tone.log --during-call --voice 1 --amp 0 --min-frames 250 --spk 1100:1700 --ref 300:1200; echo "check rc=$?"
```
Expected: `rc=0`, `0`, 기기마다 요약 한 줄(`[1] fsm=IN_CALL voice=1 vsrc=tone amp=0 cap=… tx=<400 안팎> rx=<400 안팎> play=… lost=0 drop=… txdrop=0 under=0 late=0 jb=…/… mic=… ref=<590 안팎> aec=… spk=<1414 안팎> … load=…% heap_min=… stk_min=…`), `voice_check: 기준에 맞음`, `check rc=0`.
- 읽는 법: `VOICE on` 은 받는 쪽에서 `INCOMING -> ANSWERING` **뒤**, 양쪽에서 `… -> IN_CALL` **바로 앞**에만 나온다(`wait` 가 그 순서로 찾는다 — 순서가 다르면 `rc=1`). 벨이 울리는 동안의 `STAT` 은 `voice=0` 이다(받기 전에는 마이크가 열리지 않는다). `spk` = 상대가 보낸 시험음(크기 2000 → 세기 1414)이 압축·교환기·해제를 거쳐 그대로 나온 것, `ref` = 그 소리가 코덱 출력에 실제로 실린 것(앰프는 꺼져 있다). `under=0`·`late=0` = 16ms 박자를 놓친 적이 없다. 끊은 뒤 `vsrc=aec`(진단 설정이 다음 통화에 남지 않는다).
- **`load`(부하 %)·`us(aec/enc/dec)`·`heap_min`·`stk_min` 을 보고에 적는다.**

- [x] **Step 7: 지연 재기** — 한쪽이 "삑"을 보내고 상대가 자기 스피커 되먹임을 돌려보낸다(2번이 걸고 1번이 받는다. 방향마다 다섯 번. 약 40초)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft; I='both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE.* amp=0 .* voice=0 :60'; Z='both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE.* voice=0 vsrc=aec :15'; P1=(); P2=(); for i in 1 2 3 4 5; do P1+=('1:vping' '1:wait:^VPING :5' 'sleep:1'); P2+=('2:vping' '2:wait:^VPING :5' 'sleep:1'); done; python3 tools/phone_ctl.py pair "$I" \
  '2:tap:640:360' '1:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '2:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' 'sleep:2' \
  '1:tap:404:428' '1:wait:^FSM ANSWERING -> IN_CALL acts=0x180 notice=NONE$:10' '2:wait:^FSM RINGBACK -> IN_CALL acts=0x180 notice=NONE$:10' 'sleep:2' \
  '2:until:vsrc ref:^OK vsrc ref$:5' 'sleep:1' "${P1[@]}" \
  '2:until:vsrc aec:^OK vsrc aec$:5' '1:until:vsrc ref:^OK vsrc ref$:5' 'sleep:1' "${P2[@]}" \
  '1:until:vsrc aec:^OK vsrc aec$:5' 'both:until:stat:^STAT .*fsm=IN_CALL.*tone=NONE.* voice=1 vsrc=aec :10' \
  '2:tap:644:606' '2:wait:^FSM IN_CALL -> ENDING acts=0x248 notice=NONE$:10' '1:wait:^FSM IN_CALL -> IDLE acts=0x240 notice=CALL_ENDED$:10' \
  'sleep:2' "$Z" > out/p3b_ping.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3b_ping.log; python3 "$D/verify/voice_check.py" out/p3b_ping.log --pings 320; echo "check rc=$?"
```
Expected: `rc=0`, `0`, `[1] VPING 5회 rt_ms=[…] 가운데 값 <160~260> / timeout 0회 → 한쪽 길(입→귀) 어림 <가운데 값 ÷ 2 + 25>ms + 에코 제거 내부 지연`, `[2] VPING 5회 …`, `voice_check: 기준에 맞음`, `check rc=0`.
- 예상 가운데 값은 약 200ms 다("지연 예산": 보내는 쪽 프레임 채우기 16 + 압축 6.5 + 망 5 + 상대의 지터 버퍼·링버퍼 46 + 출력 DMA 32 + 상대의 묶음·프레임 채우기 24 + 압축 6.5 + 망 5 + 이쪽 지터 버퍼 40~50). 기준은 320ms 이하·80ms 이상·`timeout` 0.
- **두 방향의 가운데 값과 "한쪽 길 어림"을 보고에 적는다** — 통화 설계서 §9.6 에 실측으로 들어간다(Task 10). 이 값에는 마이크 묶음 대기(평균 8ms)·에코 제거 내부 지연·스피커와 공기(약 2ms)가 빠져 있다.
- `VPING timeout` 이 나오면: 상대의 `vsrc ref` 가 걸렸는지(`OK vsrc ref`), 상대 `STAT` 의 `v_ref`(되먹임 칸에 소리가 잡히는가)를 본다. 80ms 미만이 나오면 삑이 아닌 소리를 잡은 것이다(되먹임 칸의 잡음이 큰 것 — 그 `v_ref` 값을 보고에).

- [x] **Step 8: 30초 통화(진짜 마이크)** — 3a 의 시나리오 B 모양 그대로: 2번이 걸고 1번이 받고, 받은 쪽(1번)이 끊는다(약 45초)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft; I='both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE.* amp=0 .* voice=0 :60'; Z='both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE.* voice=0 vsrc=aec :15'; C='both:until:stat:^STAT .*fsm=IN_CALL.*tone=NONE.* voice=1 :10'; python3 tools/phone_ctl.py pair "$I" \
  '2:tap:640:360' '1:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '2:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' \
  '1:until:stat:^STAT .*fsm=INCOMING.*tone=BELL.* voice=0 :5' '2:until:stat:^STAT .*fsm=RINGBACK.*tone=RINGBACK.* voice=0 :5' 'sleep:3' \
  '1:tap:404:428' '1:wait:^FSM INCOMING -> ANSWERING acts=0x82 notice=NONE$:10' '1:wait:^VOICE on$:10' '1:wait:^FSM ANSWERING -> IN_CALL acts=0x180 notice=NONE$:10' '2:wait:^VOICE on$:10' '2:wait:^FSM RINGBACK -> IN_CALL acts=0x180 notice=NONE$:10' \
  "$C" 'sleep:30' "$C" \
  '1:tap:644:606' '1:wait:^VOICE off$:10' '1:wait:^FSM IN_CALL -> ENDING acts=0x248 notice=NONE$:10' '2:wait:^VOICE off$:10' '2:wait:^FSM IN_CALL -> IDLE acts=0x240 notice=CALL_ENDED$:10' \
  'sleep:2' "$Z" > out/p3b_call30.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3b_call30.log; for u in 1 2; do echo "[$u] $(grep -E "^\[$u\] TONE " out/p3b_call30.log | sed 's/^\[.\] TONE //' | paste -sd, -) / VOICE $(grep -E "^\[$u\] VOICE (on|off)$" out/p3b_call30.log | sed 's/^\[.\] VOICE //' | paste -sd, -)"; done; python3 "$D/verify/voice_check.py" out/p3b_call30.log --during-call --voice 1 --amp 0 --min-frames 1400; echo "check rc=$?"
```
Expected: `rc=0`, `0`, `[1] BELL on,BELL off,END on,END off / VOICE on,off`, `[2] RINGBACK on,RINGBACK off,END on,END off / VOICE on,off`(신호음의 순서는 3a 그대로), 기기마다 요약 한 줄(`tx`·`rx`·`play` 약 1500~1650, `under=0 late=0`), `voice_check: 기준에 맞음`, `check rc=0`.
**`load`·`us(aec/enc/dec)` 와 `max`·`lost`·`drop`·`jb`·`heap_min` 을 보고에 적는다**(방이 조용하면 `mic`·`spk` 는 작다 — 판정하지 않는다).

- [x] **Step 9: 3분 통화** — 3a 의 시나리오 A 모양: 1번이 걸고 2번이 받고 1번이 끊는다. 30초마다 상태를 읽는다(약 3분 30초 — Bash 의 `timeout` 을 600000 으로)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft; I='both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE.* amp=0 .* voice=0 :60'; Z='both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE.* voice=0 vsrc=aec :15'; S=(); for i in 1 2 3 4 5 6; do S+=('sleep:30' 'both:until:stat:^STAT .*fsm=IN_CALL.*tone=NONE.* voice=1 :10'); done; python3 tools/phone_ctl.py pair "$I" \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '1:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' \
  '2:until:stat:^STAT .*fsm=INCOMING.*tone=BELL.* voice=0 :5' '1:until:stat:^STAT .*fsm=RINGBACK.*tone=RINGBACK.* voice=0 :5' 'sleep:3' \
  '2:tap:404:428' '2:wait:^FSM INCOMING -> ANSWERING acts=0x82 notice=NONE$:10' '2:wait:^VOICE on$:10' '2:wait:^FSM ANSWERING -> IN_CALL acts=0x180 notice=NONE$:10' '1:wait:^VOICE on$:10' '1:wait:^FSM RINGBACK -> IN_CALL acts=0x180 notice=NONE$:10' \
  "${S[@]}" \
  '1:tap:644:606' '1:wait:^VOICE off$:10' '1:wait:^FSM IN_CALL -> ENDING acts=0x248 notice=NONE$:10' '2:wait:^VOICE off$:10' '2:wait:^FSM IN_CALL -> IDLE acts=0x240 notice=CALL_ENDED$:10' \
  'sleep:2' "$Z" > out/p3b_call180.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3b_call180.log; for u in 1 2; do echo "[$u] $(grep -E "^\[$u\] TONE " out/p3b_call180.log | sed 's/^\[.\] TONE //' | paste -sd, -) / VOICE $(grep -E "^\[$u\] VOICE (on|off)$" out/p3b_call180.log | sed 's/^\[.\] VOICE //' | paste -sd, -)"; done; for u in 1 2; do grep -E "^\[$u\] STAT .* voice=1 " out/p3b_call180.log | sed -E "s#^\[.\] STAT up=([0-9]+) .* heap_int=([0-9]+) .* v_rx=([0-9]+) v_play=([0-9]+) v_lost=([0-9]+) v_drop=([0-9]+) v_under=([0-9]+) v_jb=([0-9/]+) .*#[$u] up=\1 heap_int=\2 v_rx=\3 v_lost=\5 v_drop=\6 v_under=\7 v_jb=\8#"; done; python3 "$D/verify/voice_check.py" out/p3b_call180.log --during-call --voice 1 --amp 0 --min-frames 8500; echo "check rc=$?"
```
Expected: `rc=0`, `0`, `[1] RINGBACK on,RINGBACK off,END on,END off / VOICE on,off`, `[2] BELL on,BELL off,END on,END off / VOICE on,off`, 기기마다 30초 간격의 여섯 줄(`[1] up=… heap_int=… v_rx=<약 1500 씩 늘어난다> v_lost=… v_drop=… v_under=0 v_jb=<1~3>/<2~3>`), 요약 두 줄(`tx`·`rx` 약 9000), `voice_check: 기준에 맞음`, `check rc=0`.
- 읽는 법: `v_rx` 가 30초마다 약 1500 씩 늘어난다(20ms 마다 한 프레임이 빠짐없이 온다). `v_jb` 의 목표가 2~3 에 머문다(지터 버퍼가 40~60ms 에서 안정 — 목표가 5 이상으로 올라가 있으면 Wi-Fi 가 자주 멈춘 것이다. 보고에 적는다). `v_lost`·`v_drop` 은 두 기기의 시계 차이만큼 드물게 는다(3분에 0~몇 개 — 기준은 받은 것의 2% 이하). `heap_int` 가 여섯 줄에 걸쳐 줄어들지 않는다(새는 메모리가 없다 — ±2KB 흔들림은 정상).
- **여섯 줄 전부와 요약을 보고에 적는다.**

- [x] **Step 10: 통화 중 상대 재부팅** — 2번을 통화 중에 재부팅한다: 1번은 목소리를 끄고 종료음을 내며 대기로 돌아온다(약 1분)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && I='both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE.* amp=0 .* voice=0 :60'; python3 tools/phone_ctl.py pair "$I" \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' 'sleep:2' '2:tap:404:428' '2:wait:^FSM ANSWERING -> IN_CALL acts=0x180 notice=NONE$:10' '1:wait:^FSM RINGBACK -> IN_CALL acts=0x180 notice=NONE$:10' \
  'sleep:8' 'both:until:stat:^STAT .*fsm=IN_CALL.*tone=NONE.* voice=1 :10' \
  '2:reboot' '2:wait:^PHONE BOOT unit=2 id=tab5-2:30' '1:wait:^VOICE off$:60' '1:wait:^FSM IN_CALL -> IDLE acts=0x240 notice=LOST$:5' \
  '2:wait:^AUDIO (ready|FAIL):20' '2:until:stat:^STAT .*fsm=IDLE.*relay=1.*audio=1 .* amp=0 .* voice=0 :120' 'sleep:3' \
  'both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE.* voice=0 vsrc=aec :30' > out/p3b_reboot.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3b_reboot.log; for u in 1 2; do echo "[$u] $(grep -E "^\[$u\] TONE " out/p3b_reboot.log | sed 's/^\[.\] TONE //' | paste -sd, -) / VOICE $(grep -E "^\[$u\] VOICE (on|off)$" out/p3b_reboot.log | sed 's/^\[.\] VOICE //' | paste -sd, -)"; done; grep -E "^\[1\] STAT " out/p3b_reboot.log | tail -1 | grep -oE 'fsm=[A-Z_]+|voice=[01]|v_rx=[0-9]+|v_lost=[0-9]+|v_under=[0-9]+' | tr '\n' ' '; echo; grep -E "^\[2\] AUDIO" out/p3b_reboot.log | tail -1
```
Expected: `rc=0`, `0`, `[1] RINGBACK on,RINGBACK off,END on,END off / VOICE on,off`, `[2] BELL on,BELL off / VOICE on`(2번은 통화 중에 꺼졌다 — 끄는 줄이 없다), `fsm=IDLE voice=0 v_rx=<400 안팎> v_lost=<3 안팎> v_under=0`, `[2] AUDIO ready rate=16000 block=256 vol=100 dma=3 mic=24 amp=0`.
읽는 법: 상대가 사라지면 1번은 60ms 를 메우고(`v_lost` 3) 조용해지며, 교환기가 알려 줄 때(2번이 다시 붙을 때 — 재부팅 뒤 약 10~25초. 늦어도 교환기의 무소식 45초) `VOICE off` → `FSM IN_CALL -> IDLE acts=0x240 notice=LOST`. 그동안 `v_under` 는 늘지 않는다(받을 것이 없는 것은 굶은 것이 아니다).

- [x] **Step 11: 연달아 통화 세 번, 그리고 통화 밖에서는 마이크가 닫혀 있다** (약 1분)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft; I='both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE.* amp=0 .* voice=0 :60'; Z='both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE.* voice=0 vsrc=aec :15'; K=(); for i in 1 2 3; do K+=('1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' 'sleep:1' '2:tap:404:428' '1:wait:^FSM RINGBACK -> IN_CALL acts=0x180 notice=NONE$:10' '2:wait:^FSM ANSWERING -> IN_CALL acts=0x180 notice=NONE$:10' 'sleep:2' 'both:until:stat:^STAT .*fsm=IN_CALL.*tone=NONE.* voice=1 :10' '1:tap:644:606' '2:wait:^FSM IN_CALL -> IDLE acts=0x240 notice=CALL_ENDED$:10' '1:wait:^FSM ENDING -> IDLE acts=0x0 notice=NONE$:10' 'sleep:2'); done; python3 tools/phone_ctl.py pair "$I" "${K[@]}" "$Z" 'sleep:5' "$Z" > out/p3b_three.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3b_three.log; for u in 1 2; do echo "[$u] VOICE $(grep -E "^\[$u\] VOICE (on|off)$" out/p3b_three.log | sed 's/^\[.\] VOICE //' | paste -sd, -)"; done; python3 "$D/verify/voice_check.py" out/p3b_three.log --idle-cap --voice 0 --amp 0; echo "check rc=$?"
```
Expected: `rc=0`, `0`, `[1] VOICE on,off,on,off,on,off`, `[2] VOICE on,off,on,off,on,off`, `[1] 통화 밖 v_cap <n> → <같은 n> (그대로 = 마이크 닫힘)`, `[2] 통화 밖 v_cap …`, 요약 두 줄, `voice_check: 기준에 맞음`, `check rc=0`.
읽는 법: 통화마다 켜기 한 번·끄기 한 번. 마지막 두 `STAT`(5초 사이)에서 `v_cap`(마이크에서 읽은 묶음 수)이 그대로다 = 통화 밖에서는 마이크를 읽지 않는다. 요약 줄의 `tx`·`rx` 는 **마지막 통화**의 것이다(통화마다 0 에서 다시 센다 — 약 100~200).

- [x] **Step 12: 3a 의 시나리오 C~G 를 다시 돌린다** — 소리 장치를 바꾼 뒤에도 신호음이 그대로인지(시나리오 A·B 는 Step 9·8 이 겸했다. 약 3분, 소리는 나지 않는다)

C. 1번이 걸고 2번이 그냥 거절한다(거는 쪽만 종료음):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && I='both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE.* amp=0 .* voice=0 :60'; Z='both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE.* voice=0 vsrc=aec :15'; python3 tools/phone_ctl.py pair "$I" \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '1:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' 'sleep:2' \
  '2:tap:876:428' '2:wait:^FSM INCOMING -> ENDING acts=0x84 notice=NONE$:10' '1:wait:^FSM RINGBACK -> IDLE acts=0x40 notice=REJECTED$:10' \
  'sleep:2' "$Z" > out/p3b_c.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3b_c.log; for u in 1 2; do echo "[$u] $(grep -E "^\[$u\] TONE " out/p3b_c.log | sed 's/^\[.\] TONE //' | paste -sd, -) / VOICE $(grep -cE "^\[$u\] VOICE (on|off)$" out/p3b_c.log)줄"; done
```
Expected: `rc=0`, `0`, `[1] RINGBACK on,RINGBACK off,END on,END off / VOICE 0줄`, `[2] BELL on,BELL off / VOICE 0줄`.

D. 1번이 걸다가 취소한다(종료음 없음, 2번은 부재중):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && I='both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE.* amp=0 .* voice=0 :60'; Z='both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE.* voice=0 vsrc=aec :15'; python3 tools/phone_ctl.py pair "$I" \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '1:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' 'sleep:2' \
  '1:tap:640:604' '1:wait:^FSM RINGBACK -> ENDING acts=0x88 notice=NONE$:10' '2:wait:^FSM INCOMING -> IDLE acts=0x80 notice=MISSED$:10' \
  'sleep:2' "$Z" > out/p3b_d.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3b_d.log; for u in 1 2; do echo "[$u] $(grep -E "^\[$u\] TONE " out/p3b_d.log | sed 's/^\[.\] TONE //' | paste -sd, -) / VOICE $(grep -cE "^\[$u\] VOICE (on|off)$" out/p3b_d.log)줄"; done
```
Expected: `rc=0`, `0`, `[1] RINGBACK on,RINGBACK off / VOICE 0줄`, `[2] BELL on,BELL off / VOICE 0줄`.

E. 30초 무응답(약 40초 — 벨·연결음 상태가 30초 내내 이어지다가 스스로 그친다. 그동안 마이크는 닫혀 있다):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && I='both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE.* amp=0 .* voice=0 :60'; Z='both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE.* voice=0 vsrc=aec :15'; python3 tools/phone_ctl.py pair "$I" \
  '2:tap:640:360' '1:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '2:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' \
  '1:until:stat:^STAT .*fsm=INCOMING.*tone=BELL.* voice=0 :5' 'sleep:15' '1:until:stat:^STAT .*fsm=INCOMING.*tone=BELL.* voice=0 :5' '2:until:stat:^STAT .*fsm=RINGBACK.*tone=RINGBACK.* voice=0 :5' \
  '2:wait:^FSM RINGBACK -> IDLE acts=0x40 notice=NO_ANSWER$:40' '1:wait:^FSM INCOMING -> IDLE acts=0x80 notice=MISSED$:5' \
  'sleep:2' "$Z" > out/p3b_e.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3b_e.log; for u in 1 2; do echo "[$u] $(grep -E "^\[$u\] TONE " out/p3b_e.log | sed 's/^\[.\] TONE //' | paste -sd, -) / VOICE $(grep -cE "^\[$u\] VOICE (on|off)$" out/p3b_e.log)줄"; done
```
Expected: `rc=0`, `0`, `[1] BELL on,BELL off / VOICE 0줄`, `[2] RINGBACK on,RINGBACK off,END on,END off / VOICE 0줄`.

F. 벨 끄기, 그리고 다음 전화는 다시 울린다(메시지 거절 1건 포함. 약 40초):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && I='both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE.* amp=0 .* voice=0 :60'; Z='both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE.* voice=0 vsrc=aec :15'; python3 tools/phone_ctl.py pair "$I" \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '1:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' '2:until:stat:^STAT .*fsm=INCOMING.*tone=BELL:5' 'sleep:1' \
  '2:tap:404:624' '2:wait:^FSM INCOMING -> INCOMING acts=0x80 notice=NONE$:5' '2:until:stat:^STAT .*fsm=INCOMING.*tone=NONE:5' 'sleep:5' \
  '2:until:stat:^STAT .*fsm=INCOMING.*tone=NONE:5' '1:until:stat:^STAT .*fsm=RINGBACK.*tone=RINGBACK:5' \
  '2:tap:404:624' 'sleep:1' '2:until:stat:^STAT .*fsm=INCOMING.*tone=NONE:5' \
  '2:tap:404:428' '2:wait:^FSM INCOMING -> ANSWERING acts=0x82 notice=NONE$:10' '1:wait:^FSM RINGBACK -> IN_CALL acts=0x180 notice=NONE$:10' 'both:until:stat:^STAT .*fsm=IN_CALL.*tone=NONE.* voice=1 :10' 'sleep:2' \
  '2:tap:644:606' '2:wait:^FSM IN_CALL -> ENDING acts=0x248 notice=NONE$:10' '1:wait:^FSM IN_CALL -> IDLE acts=0x240 notice=CALL_ENDED$:10' 'sleep:2' "$Z" \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '1:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' '2:until:stat:^STAT .*fsm=INCOMING.*tone=BELL:5' 'sleep:1' \
  '2:tap:876:624' '2:wait:^SCREEN DECLINE$:5' 'sleep:1' '2:until:stat:^STAT .*fsm=INCOMING screen=DECLINE.*tone=BELL:5' \
  '2:tap:640:168' '2:wait:^FSM INCOMING -> ENDING acts=0x84 notice=NONE msg=2$:10' '1:wait:^FSM RINGBACK -> IDLE acts=0x40 notice=REJECT_MSG msg=2$:10' \
  'sleep:2' "$Z" > out/p3b_f.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3b_f.log; for u in 1 2; do echo "[$u] $(grep -E "^\[$u\] TONE " out/p3b_f.log | sed 's/^\[.\] TONE //' | paste -sd, -) / VOICE $(grep -E "^\[$u\] VOICE (on|off)$" out/p3b_f.log | sed 's/^\[.\] VOICE //' | paste -sd, -)"; done; grep -c '^\[2\] FSM INCOMING -> INCOMING acts=0x80 notice=NONE$' out/p3b_f.log
```
Expected: `rc=0`, `0`, `[1] RINGBACK on,RINGBACK off,END on,END off,RINGBACK on,RINGBACK off,END on,END off / VOICE on,off`, `[2] BELL on,BELL off,END on,END off,BELL on,BELL off / VOICE on,off`, 마지막 줄 `1`. (읽는 법은 3a 계획서 Task 5 Step 8 과 같다. 더해서: 벨을 끈 통화도 받으면 목소리가 켜진다 — `VOICE on,off` 한 번씩.)

G. 짧은 전화 세 번 연달아(울리자마자 취소 → 곧바로 다시):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && I='both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE.* amp=0 .* voice=0 :60'; Z='both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE.* voice=0 vsrc=aec :15'; G=(); for i in 1 2 3; do G+=('1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '1:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' 'sleep:1' '1:tap:640:604' '2:wait:^FSM INCOMING -> IDLE acts=0x80 notice=MISSED$:10' '1:wait:^FSM ENDING -> IDLE acts=0x0 notice=NONE$:10' 'sleep:1'); done; python3 tools/phone_ctl.py pair "$I" "${G[@]}" 'sleep:1' "$Z" > out/p3b_g.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3b_g.log; for u in 1 2; do echo "[$u] $(grep -E "^\[$u\] TONE " out/p3b_g.log | sed 's/^\[.\] TONE //' | paste -sd, -) / VOICE $(grep -cE "^\[$u\] VOICE (on|off)$" out/p3b_g.log)줄"; done
```
Expected: `rc=0`, `0`, `[1] RINGBACK on,RINGBACK off,RINGBACK on,RINGBACK off,RINGBACK on,RINGBACK off / VOICE 0줄`, `[2] BELL on,BELL off,BELL on,BELL off,BELL on,BELL off / VOICE 0줄`.

- [x] **Step 13: 터치 대조·줄의 짝·여유·목소리가 쓴 메모리, 비밀**

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft && A=out/p3b_all_pair.log && cat out/p3b_fault_call.log out/p3b_tone.log out/p3b_ping.log out/p3b_call30.log out/p3b_call180.log out/p3b_reboot.log out/p3b_three.log out/p3b_c.log out/p3b_d.log out/p3b_e.log out/p3b_f.log out/p3b_g.log > $A && python3 "$D/verify/touch_audit.py" $A; echo "TONE on $(grep -cE '^\[.\] TONE [A-Z]+ on$' $A) / off $(grep -cE '^\[.\] TONE [A-Z]+ off$' $A), VOICE on $(grep -cE '^\[.\] VOICE on$' $A) / off $(grep -cE '^\[.\] VOICE off$' $A), TONE ? $(grep -cE '^\[.\] TONE \?' $A)줄, AUDIO FAIL $(grep -c 'AUDIO FAIL' $A)줄, RELAY up=0 $(grep -c 'RELAY up=0' $A)줄"; for u in 1 2; do grep -E "^\[$u\] STAT " out/p3b_g.log | tail -1 | grep -oE '^\[.\]|heap_int=[0-9]+|heap_int_min=[0-9]+|stk_[a-z]+=[0-9]+|audio=[01]|aud_us=[0-9/]+|aud_err=[0-9]+|aud_late=[0-9]+|amp=[01]|v_rderr=[0-9]+' | tr '\n' ' '; echo; done
```
Expected:
- `== out/p3b_all_pair.log: 주입한 탭 수와 touch 증가가 모두 일치`. 불일치 줄이 있으면 그 시각에 사용자가 화면을 만졌는지 컨트롤러에게 확인을 부탁한다. 재부팅을 건너간 줄에서 `touch` 가 줄어든 것은 재부팅이다(Step 10 의 2번) — 불일치로 세지 않는다.
- `TONE on 54 / off 54, VOICE on 19 / off 18, TONE ? 0줄, AUDIO FAIL 0줄, RELAY up=0 0줄`. (신호음: 소리 없는 통화 2 + 시험음 4 + 지연 4 + 30초 4 + 3분 4 + 재부팅 3 + 연달아 12 + C 3 + D 2 + E 3 + F 7 + G 6 — 켜진 채 남은 소리가 없다. 목소리: 통화 10건 × 두 기기 − 소리 장치가 죽어 있던 2번의 1건 = 켜기 19, 끄기는 통화 중에 재부팅한 2번의 1건이 빠져 18. 통화 중에 교환기 연결이 끊긴 적이 없다.)
- 기기마다 한 줄: `heap_int_min` ≥ 100000(아래면 `voice_check` 의 `??` 줄과 함께 보고에 — 멈추지 않는다. 60000 아래면 멈춘다), `stk_app`·`stk_touch`·`stk_relay`·`stk_ws`·`stk_audio`·**`stk_vcap`·`stk_vrx`·`stk_vtx`** ≥ 1024, `audio=1`, `aud_err=0`, `aud_late=0`, `amp=0`, `v_rderr=0`.
- **목소리가 쓴 것**을 보고에 적는다: ① 내부 메모리 = Step 2 의 `heap_int` − 지금 `heap_int`(예산: 약 135~150KB 를 더 쓴다 — 스택 56KB + 에코 제거·Opus 65~80KB + 나머지 약 15KB) ② `stk_vcap`·`stk_vrx`·`stk_vtx` 의 값(스택을 줄일 근거) ③ Step 8 의 `load`.

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 ~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft/verify/secret_scan.py out/p3b_all_pair.log out/p3b_boot_unit1.log out/p3b_boot_unit2.log out/p3b_before.log out/p3b_fault.log out/p3b_fault_recover.log; echo "secret rc=$?"
```
Expected: 로그마다 `wifi_pass=0 relay_url=0 relay_host=0 token=0`, `secret rc=0`. (`rc=2` 는 "값을 못 읽어 검사하지 못함"이다 — 통과가 아니다.)

**결함이 나오면** (다시 돌려도 같은 `!!` 가 나오거나 요약이 Expected 와 다를 때 — 고치지 않고 진단해서 보고한다)
1. 두 기기의 `FSM`·`VOICE`·`TONE` 줄과 `STAT` 의 목소리 칸을 시간순으로 놓고, 어느 기기의 어느 쪽(보내는 길 / 받는 길 / 신호음 / 통화 흐름)이 기대와 다른지 먼저 확정한다. `FSM` 줄부터 다르면 통화 쪽(Phase 2 회귀)이다.
2. 증상별 1순위로 볼 곳(추측하지 않는다 — 시험 펌웨어 `tab5_phone_probe/` 의 코드·로그와 줄 단위로 대조한다):
   - **부팅 고리**(`PHONE BOOT` 가 `AUDIO` 줄 없이 되풀이): 죽은 자리의 백트레이스와 그 앞 30줄을 붙인다. 1순위 가설 = `v_cap` 태스크의 준비(`aec_create`·Opus 열기)나 PSRAM 스택 태스크(`v_rx`) 만들기. 컨트롤러가 "롤백"으로 되돌린다.
   - `AUDIO FAIL i2c`·`i2s`: 부팅 로그의 `I2S_IF`·`i2c` 줄. `AUDIO FAIL codec`: 바로 앞의 `audio_out: mic gain not applied: read …` 줄(마이크 증폭 되읽기 — PL3b-D10 의 다른 점)이 있는지. 없으면 `ES7210`·`ES8388`·`Adev_Codec` 줄을 `task4.log` 177~187 과 대조. `AUDIO FAIL voice`: 바로 앞의 `voice: voice setup failed at '<단계>'`(`aec`·`aec-chunk`·`opus-enc`·`opus-size`·`opus-dec`·`buffers`·`memory`·`task-*`·`codec-timeout`) — `memory`·`buffers`·`task-*` 면 그때의 `heap_int`.
   - `SELFTEST … ok=0`(`ref` 가 300 아래): 코덱 출력이 안 나간다 — 3a 의 신호음도 안 나고 있다는 뜻이다. `STAT` 의 `aud_err`, 부팅 로그의 `ES8388` 줄. `ref` 가 1200 을 넘으면 되먹임 칸의 증폭이 0dB 가 아니다(레지스터 0x45).
   - 시험음 통화에서 `rx=0`: 상대의 `tx`·`txfail`·`v_cap` 을 본다 — `v_cap` 이 안 늘면 마이크 읽기(`v_rderr`), `v_cap` 은 느는데 `tx=0` 이면 압축·보내기(`txfail`), 상대 `tx` 는 느는데 이쪽 `rx=0` 이면 교환기(로그 `/tmp/phone_relay_dev.log` 의 `voice_drop_*` 를 **읽기만** 한다). `rx` 는 느는데 `spk=0`·`play=0`: 해제 실패. `spk` 는 맞는데 `ref` 가 작다: 출력 태스크가 목소리 공급자를 부르지 않는다(`aud_us`).
   - `under`(= `v_under`)가 0 이 아니다: 해제 태스크(`v_rx` — PSRAM 스택)가 16ms 안에 못 돈 것. `v_dec_us` 의 최대, `load`. `late`(= `aud_late`)가 0 이 아니다: 출력 태스크가 48ms 넘게 밀린 것 — `v_aec_us`·`v_enc_us` 의 최대(캡처 태스크가 코어를 오래 쥐었나), DMA 3칸(PL3b-D4 의 탈출구).
   - `load` 가 50% 를 넘는다: `us(aec/enc/dec)` 를 Phase 0 값(2795·2173·306)과 나란히 적는다.
   - `VOICE` 줄의 순서·짝이 다르다: 맥 시험 `test_voice_core.cpp` 의 "never open before …"·"the call ends every way …" 와 대조하고, `FSM` 줄의 `acts` 값(0x100 = 켜기, 0x200 = 끄기)을 본다.
   - 3a 시나리오의 `TONE` 요약이 다르다: 3a 의 기기 로그(`out/p3a_*.log`)와 나란히 놓는다. `TONE ? lost=` 줄이 있으면 app 태스크가 오래 멈춘 것이다(PL3b-D1 의 비용).
   - 통화 흐름이 달라졌다(걸리지 않음, 화면이 늦음, 터치 대조 불일치, `RELAY up=0`): Step 2 의 `p3b_before.log` 때와 비교하고 `heap_int_min`·`stk_*`·`load` 를 붙인다.
3. 다음 경우는 **멈추고 보고한다(BLOCKED)**: 플래시가 `Hash of data verified.` 없이 끝남 / 부팅 고리 / `PHONE FAIL` / 일부러 내지 않은 `AUDIO FAIL` / `SELFTEST` 실패 / 같은 실행이 세 번 연속 실패 / 터치 대조에서 사람 손이 아닌 불일치가 남음 / `stk_*` 가 1024 아래 / `heap_int_min` 이 60000 아래.
4. 좁혀야 하면(소리 장치 쪽인가 목소리 쪽인가): Task 3 의 커밋은 "송수신 장치 + 신호음만"으로 빌드되는 판이다(사본 0x244660) — 컨트롤러가 그 커밋을 따로 빌드해 올려 보게 할 수 있다(이 계획의 단계는 아니다).
5. 전화기를 쓸 수 없게 됐다고 판단되면 컨트롤러가 "롤백"의 명령으로 두 기기를 `5d9935f` 로 되돌린다 — 실행 담당이 스스로 되돌리지 않는다.

---

### Task 9: 기기 검증 B — **사용자와 함께** 스피커를 켜고 실제로 통화한다 (컨트롤러가 진행)

> 2026-10-05 기록: **부분 수행.** Step 1 만 계획대로 했고(한 기기씩 `quiet off` → `selftest`), Step 2~4 는 `[ ]` 로 두었다. 사유와 사용자의 답은 진행 로그 "Task 9".

**Files:**
- Create(git 제외): `firmware/tab5_phone/out/p3b_loud_*.log`, `out/p3b_ear.log`

**Interfaces:**
- Consumes: Task 8 을 통과한 두 기기(조용한 모드, 새 펌웨어), Task 8 Step 3·4 의 `SELFTEST` 조용한 값, `D/verify/voice_check.py`.
- Produces: 스피커에서 실제로 소리가 난다는 기록(마이크가 들은 세기), 에코가 줄어든 양, 사용자의 입·귀 점검표 답(3a 의 귀 확인 포함), 말하는 동안의 부하·손실 기록.

**사용자가 자리에 있을 때만 시작한다**(P3A-R11). 컨트롤러가 먼저 알린다: "지금부터 두 기기에서 소리가 난다 — 삑 소리(0.6초)가 기기마다 한 번, 이어서 약 10초 동안 '삐—' 하는 시험음이 양쪽에서 난다. 그 뒤 직접 통화해 보시면 된다." 두 기기는 **다른 방**에 둔다(같은 방이면 서로의 스피커 소리가 상대 마이크로 들어간다 — 점검표 13번에서만 같은 방에 둔다).
다시 올리지 않는다. 소리가 그치지 않거나 하울링이 나면 곧바로 `quiet on` 을 건다(아래 Step 1 명령의 `quiet off` 를 `quiet on`, `amp=1` 을 `amp=0` 으로 바꿔서).

- [x] **Step 1: 조용한 모드를 풀고, 소리 나는 점검을 한다** — 스피커에서 난 시험음을 그 기기의 마이크가 듣는가

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 :60' 'both:until:quiet off:^OK quiet off amp=1$:10' 'both:until:stat:^STAT .*audio=1 .* amp=1 :10' 'sleep:1' '1:selftest' '1:wait:^SELFTEST :5' 'sleep:2' '2:selftest' '2:wait:^SELFTEST :5' > out/p3b_loud_selftest.log 2>&1; echo "rc=$?"; grep -E "^\[.\] (SELFTEST|OK quiet)" out/p3b_loud_selftest.log; for u in 1 2; do echo "조용할 때 [$u] $(grep -hE '^SELFTEST' out/p3b_boot_unit$u.log | tail -1)"; done
```
Expected: `rc=0`, `[1] OK quiet off amp=1`, `[2] OK quiet off amp=1`, `[1] SELFTEST amp=1 mic=<M1> ref=<300~1200> aec=… ok=1`, `[2] SELFTEST amp=1 mic=<M2> ref=… ok=1`, 이어서 Task 8 때의 조용한 값 두 줄.
**판정**: `mic` 가 같은 기기의 조용한 값의 **5배 이상**이면 "스피커에서 실제로 소리가 났고 그 기기의 마이크가 들었다"(3a 가 귀에만 맡겼던 것의 기록). 5배가 안 되면 스피커가 울리지 않는 것이다 — 사용자에게 삑 소리가 들렸는지 묻고, 안 들렸으면 멈춘다(앰프 경로 — `STAT` 의 `amp=`, 확장칩 0x43). `ref` 는 조용할 때와 비슷해야 한다(코덱 출력은 앰프와 무관). 네 값을 보고에 적는다. (`aec` 는 참고다 — 0.5초로는 에코 제거가 다 맞춰지지 않는다.)

- [ ] **Step 2: 소리 나는 시험음 통화** — 양쪽 스피커에서 1kHz 가 약 10초 난다. 그 소리를 에코 제거가 얼마나 줄이는가(이때 양쪽 방은 조용해야 한다 — 사용자에게 10초만 말하지 말아 달라고 한다) **[2026-10-05 부분 수행 — `[ ]` 유지]** 양쪽 스피커에서 동시에 내는 시험음 통화는 하지 않았다. 대신 한 번에 한 대의 스피커만 켜고 상대가 1kHz 시험음을 보내는 방식으로 메아리 제거를 쟀다(1번 31~36dB, 2번 약 28dB 감소).

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft; python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE.* amp=1 .* voice=0 :60' \
  '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' '1:wait:^FSM DIALING -> RINGBACK acts=0x10 notice=NONE$:10' 'sleep:3' \
  '2:tap:404:428' '2:wait:^FSM ANSWERING -> IN_CALL acts=0x180 notice=NONE$:10' '1:wait:^FSM RINGBACK -> IN_CALL acts=0x180 notice=NONE$:10' \
  'both:until:vsrc tone:^OK vsrc tone$:5' 'sleep:10' 'both:until:stat:^STAT .*fsm=IN_CALL.*tone=NONE.* voice=1 vsrc=tone :10' \
  '1:tap:644:606' '1:wait:^FSM IN_CALL -> ENDING acts=0x248 notice=NONE$:10' '2:wait:^FSM IN_CALL -> IDLE acts=0x240 notice=CALL_ENDED$:10' \
  'sleep:2' 'both:until:stat:^STAT .*fsm=IDLE.*peer=1.*tone=NONE.* voice=0 vsrc=aec :15' > out/p3b_loud_tone.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3b_loud_tone.log; python3 "$D/verify/voice_check.py" out/p3b_loud_tone.log --during-call --voice 1 --amp 1 --min-frames 300 --spk 1100:1700 --ref 300:1200 --min-erle 20; echo "check rc=$?"
```
Expected: `rc=0`, `0`, 기기마다 요약 한 줄(`amp=1 … mic=<조용할 때보다 훨씬 크다> ref=<590 안팎> aec=<mic 의 1/10 이하> spk=<1414 안팎> erle=<20dB 이상>`), `voice_check: 기준에 맞음`, `check rc=0`.
- 이 통화에서 사용자는 벨(2번)·연결음(1번) 3초, 시험음 약 10초, 종료음을 듣는다 — 3a 의 귀 확인 일부가 여기서 된다.
- `erle` = 마이크에 들어온 스피커 소리(시험음)를 에코 제거가 줄인 양이다. Phase 0 실측은 29~42dB. **20dB 에 못 미치면 멈추지 않고 값을 보고에 적는다**(상대에게 메아리가 들릴 수 있다 — 점검표 8번으로 확인한다). `mic`·`aec`·`erle`·`load` 를 보고에 적는다.

- [ ] **Step 3: 기록 창을 띄우고(10분), 사용자가 직접 통화하며 점검표에 답한다** — 기록 창이 떠 있는 동안에는 다른 `phone_ctl.py` 를 돌리지 않는다(Bash 의 `timeout` 600000, 백그라운드) **[2026-10-05 부분 수행 — `[ ]` 유지]** 이 모양의 10분 기록 창과 14문항 점검은 하지 못했다. 사용자는 같은 책상에서 한 번에 한 대의 스피커로 통화해 보고 일부에만 답했다(진행 로그 "Task 9").

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && S=(); for i in $(seq 1 58); do S+=('both:until:stat:^STAT :10' 'sleep:9'); done; python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*audio=1 .* amp=1 :60' "${S[@]}" 'both:until:stat:^STAT :15' > out/p3b_ear.log 2>&1; echo "rc=$?"; grep -c '!!' out/p3b_ear.log; echo "VOICE on $(grep -cE '^\[.\] VOICE on$' out/p3b_ear.log) / off $(grep -cE '^\[.\] VOICE off$' out/p3b_ear.log)"
```

| # | 사용자가 하는 일 | 들려야 하는 것 | 묻는 것 |
|---|---|---|---|
| 1 | 1번에서 통화 버튼 | 2번에서 **벨**(따르르릉 1초, 2초 쉼), 1번에서 **연결음**(뚜— 1초, 2초 쉼) | 벨이 들리는가? 연결음이 들리는가? |
| 2 | 2번에서 "벨 끄기" | 벨이 곧 그친다. 1번의 연결음은 계속 난다 | 누르면 멈추는가? 그 뒤로 다시 울리지 않는가? |
| 3 | 2번에서 받기 | 1번의 연결음이 그치고 **통화가 열린다** | 받는 순간 딸깍 소리나 연결음 꼬리가 거슬리는가? |
| 4 | 한 사람씩 번갈아 말한다(다른 방에서) | 상대 목소리가 들린다 | **서로 들리는가?** 말이 또렷한가? |
| 5 | (4번을 하며) | — | **끊김·로봇 소리·지직거림**이 있는가? |
| 6 | "하나, 둘, 셋"을 주고받는다 | — | **지연**이 대화에 거슬리는가?(무전기처럼 기다려야 하는가) |
| 7 | (4번을 하며) | — | **음량**: 상대 목소리가 크다/적당/작다? 상대는 내 목소리를 어떻게 듣는가? |
| 8 | 한 사람이 말하고 다른 사람은 가만히 있는다 | 말하는 사람에게 자기 목소리가 돌아오지 않는다 | **내 목소리가 메아리로 돌아오는가?** |
| 9 | 두 사람이 **동시에** 말한다 | 둘 다 상대 목소리가 들린다(조금 줄거나 끊길 수 있다) | 동시에 말해도 되는가? 상대 말이 잘리는가? |
| 10 | 한쪽에서 끊기 | 목소리가 그치고 양쪽에서 **종료음**(높은 음 → 낮은 음) | 종료음이 들리는가? 끊는 순간 딸깍·남는 소리가 있는가? |
| 11 | 걸고 → 받는 쪽에서 거절 / 걸다가 거는 쪽에서 취소 / 걸고 → "메시지로 거절"에서 문구 고르기 | 거절: 거는 쪽에 종료음. 취소: 양쪽 다 조용히. 고르는 동안: 벨이 계속 | 맞게 들리는가? 고르는 동안의 벨이 거슬리는가? |
| 12 | 아무것도 안 하고 10초 귀를 대 본다 / 벨이 시작·끝날 때를 듣는다 | 대기 중에는 조용하다 | **잡음**: 벨·연결음·종료음의 음량은 적당한가? 시작·끝의 딸깍, 찢어지는 소리, 대기 중 쉬~ 소리가 있는가? |
| 13 | 두 기기를 **같은 방**에 두고 통화한다(10초만) | 메아리나 "삐—"(하울링)가 날 수 있다 — 스피커폰의 한계 | 어떻게 되는가?(기록만 한다. 심하면 바로 끊는다) |
| 14 | 통화 중에 화면을 본다 | 통화 화면·끊기 버튼이 평소처럼 움직인다 | 통화 중에 화면이 느려지거나 버튼이 늦게 먹는가? |

1·2·10·11·12 는 3a 가 남긴 귀 확인(10문항)을 겸한다. 답은 Task 10 에서 문서에 적는다. "크다/작다/메아리가 있다/끊긴다"는 답이 나오면 고치는 일은 따로 맡긴다(상수 한두 개 — PL3b-D4·D5·D10·D13 의 탈출구 — 를 바꾸고 Task 7·8·9 의 빌드·플래시·해당 단계를 다시).

- [ ] **Step 4: 기록 창이 끝나면 말하는 동안의 숫자를 확인한다** **[`[ ]` 유지 — 확인 못 함]** 이 명령(`voice_check.py --during-call --amp 1`)을 그대로 돌렸다는 기록은 확인 못 했다. 말하는 동안의 숫자는 아침 통화 기록의 `STAT` 에서 읽었다(부하 2번 약 43%·1번 약 40%, 압축 평균 2번 4.3ms·1번 3.6ms).

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft; python3 "$D/verify/voice_check.py" out/p3b_ear.log --during-call --amp 1; echo "check rc=$?"; for u in 1 2; do grep -E "^\[$u\] STAT .* voice=1 " out/p3b_ear.log | sed -E "s#^\[.\] STAT up=([0-9]+) .* v_under=([0-9]+) v_jb=([0-9/]+) v_aec_us=([0-9/]+) v_enc_us=([0-9/]+) v_dec_us=([0-9/]+) v_mic=([0-9]+) v_ref=([0-9]+) v_aec=([0-9]+) v_spk=[0-9]+ .*#[$u] up=\1 under=\2 jb=\3 aec_us=\4 enc_us=\5 dec_us=\6 mic=\7 ref=\8 aec=\9#" | tail -12; done; for u in 1 2; do grep -E "^\[$u\] STAT " out/p3b_ear.log | tail -1 | grep -oE '^\[.\]|fsm=[A-Z_]+|amp=[01]|voice=[01]|heap_int_min=[0-9]+|stk_v[a-z]+=[0-9]+' | tr '\n' ' '; echo; done
```
Expected: 기기마다 요약 한 줄(마지막으로 통화 중에 읽은 `STAT` — `under=0 late=0`, `load` ≤ 50%), `voice_check: 기준에 맞음`, `check rc=0`(사용자가 통화를 한 번도 하지 않았으면 `!! STAT 줄을 못 찾음 … voice=1 인 것` 과 `check rc=2` — 그때는 "기록 없음"으로 적는다), 통화 중의 줄(기기마다 끝의 12줄까지 — 말하는 동안 `mic` 가 수백~수천, `aec_us`·`enc_us` 의 평균이 Phase 0 값 2795·2173 근처인지), 마지막 줄 `[1] fsm=IDLE heap_int_min=… amp=1 voice=0 stk_vcap=… stk_vrx=… stk_vtx=…`.
**말하는 동안의 `load`·`us(aec/enc/dec)`·`max` 를 보고에 적는다**(조용한 방에서 잰 Task 8 Step 8 의 값과 나란히). 끝난 뒤 두 기기는 `amp=1`(소리 켜짐)·대기 상태로 남는다 — 사용자가 밤에 조용히 두고 싶어 하면 `quiet on` 을 건다.

---

### Task 10: 기록 — README, 통화 설계서, 3a 계획서, 이 계획서

> 2026-10-05 기록: 이 과제는 요구서 `task-10-brief.md` 와 그 뒤의 `task-10-addendum.md`(어긋나면 덧붙임이 이긴다)대로 했다. README 패치는 그대로 붙었으나(자동 동기화가 먼저 담았다) 지금 값으로 손으로 고쳤다. 아래 Step 본문은 쓰기 전의 계획 원문이다 — 실제 결과는 진행 로그와 각 문서에 있다.

**Files:**
- Modify: `firmware/tab5_phone/README.md`, `docs/exec-plans/2026-10-03-tab5-phone.md`(머리의 상태 줄, §5 Phase 3 줄, §8 진행 로그, §9.5 모듈 표, §9.6 지연), `docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md`(귀 확인 칸), `docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md`(이 계획서의 체크박스·상태·진행 로그)

**Interfaces:**
- Consumes: Task 1~9 의 보고(커밋 SHA, 시험 개수, 빌드 크기·버전, 기기 숫자 — 부하·메모리·스택·지연·손실, 사용자 점검표 답).
- Produces: 갱신된 문서 넷, 다음 단계(Phase 4)로 넘기는 목록(보고에 싣는다).

- [x] **Step 1: README 패치를 적용하고 대조한 뒤, 실제 결과로 채운다**

```bash
cd ~/tab5-toy-phone && D=~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft && git apply "$D/patches/T10-01-readme.patch" && cmp "$D/files/firmware/tab5_phone/README.md" firmware/tab5_phone/README.md && echo "같음: README.md"
```
Expected: `같음: README.md`. (그 사이 다른 작업이 README 를 고쳐 패치가 문맥 불일치로 적용되지 않으면, 아래 블록의 `+` 줄을 해당 자리에 손으로 넣고 `-` 줄을 지운다.)

````diff
--- a/firmware/tab5_phone/README.md
+++ b/firmware/tab5_phone/README.md
@@ -1,7 +1,7 @@
 # tab5_phone — Tab5 2대 음성 전화기
 
 두 대에 같은 펌웨어를 올린다. 기기 번호는 칩의 MAC 으로 정해진다(1번 `AA:BB:CC:DD:EE:01`, 2번 `AA:BB:CC:DD:EE:02`).
-지금 단계(Phase 3a)는 걸기 → 벨 화면 → 받기/거절 → 통화 화면 → 끊기에 **신호음**이 붙었다: 받는 쪽 벨, 거는 쪽 연결음, 통화가 끝났을 때 종료음. **목소리는 아직 없다**(Phase 3b).
+지금 단계(Phase 3b)는 걸기 → 벨 화면 → 받기/거절 → 통화 화면 → 끊기에 **신호음**(받는 쪽 벨, 거는 쪽 연결음, 통화가 끝났을 때 종료음)과 **목소리**가 붙었다: 받으면 두 기기가 스피커폰으로 동시에 말하고 듣는다(마이크 → 에코 제거 → Opus → 교환기 → 상대 스피커). 받기 전에는 마이크를 읽지 않고, 끊으면 그 자리에서 닫힌다.
 **귀 확인 대기**: 구현·두 기기 자동 검증(기록으로 확인)·최종 검수(96점)는 끝났지만, 스피커에서 실제로 소리가 나는지·어떻게 들리는지는 **아직 아무도 확인하지 못했다**(사용자 귀 확인 대기 — 2026-10-04 22:26~22:36 기록 창에서 터치 0). 기록으로 확인된 것은 "출력 경로가 기대한 설정으로 열렸고 쓰기 오류가 0"까지다. 아래 "신호음" 절의 소리 모양·크기도 귀 확인 전의 설계 값이다.
 받는 쪽 벨 화면에는 **벨 끄기**(그 전화의 벨소리만 끈다 — 누른 뒤 최대 0.1초 안에 그치도록 만들었다 — 이것은 드라이버 소스로 본 설계 값이고 기기에서 재지 않았다. 기록은 `tone=NONE` 으로 바뀌는 것까지만 보여 주며, 실제 소리는 귀 확인 전)와 **메시지로 거절**(문구 3개 중 하나로 거절 → 건 쪽 대기 화면에 "상대: …" 10초)도 있다.
 
@@ -9,6 +9,7 @@
 - 구현 계획: `docs/exec-plans/2026-10-04-tab5-phone-phase2.md`
 - 수신거부 메시지·벨 끄기: 설계 `docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md`, 계획 `…-decline-mute-plan.md`
 - 신호음(Phase 3a): 계획 `docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md` (결정 PL3a-D1~D16)
+- 목소리 통화(Phase 3b): 계획 `docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md` (결정 PL3b-D1~D20, 지연 예산, CPU·메모리 예산)
 - 참고 구현 조사: `docs/exec-plans/2026-10-04-tab5-phone-research/`
 
 ## 구조
@@ -26,8 +27,12 @@
 | `main/ui_wifi.h/.cpp` | Wi-Fi 설정 화면(목록 + 비밀번호 자판) | 기기 |
 | `main/app.h/.cpp` | 터치·교환기 신호·시간 → 상태 기계 → 할 일 실행 → 다시 그리기 | 기기 |
 | `main/tones.h` | 신호음의 박자·주파수·크기, 딸깍 방지(5ms), 할 일 → 소리, 기록 줄 고르기 | 맥 `tools/test_tones.cpp` |
-| `main/audio_out.h/.cpp` | 스피커 출력(ES8388 + I2S 송신). 공급자에게 PCM 을 받아 낸다 — 무엇을 낼지는 모른다 | 기기(귀) |
+| `main/audio_out.h/.cpp` | 소리 장치: 스피커 출력(ES8388 + I2S 송신)과 마이크 입력(ES7210 + I2S 수신 4칸). 출력은 공급자들에게 PCM 을 받아 섞어 내고, 입력은 4칸을 읽어 준다 — 무엇에 쓰는지는 모른다 | 기기 |
 | `main/tone_out.h/.cpp` | 신호음 공급자: 명령 우편함 + `TONE` 기록 줄(로직은 `tones.h`) | 기기 |
+| `main/pcm.h` | 소리 계산: 섞기(더하고 넘치면 자름), 크기, 세기, 태스크 사이 링버퍼, 출력 박자 감시 | 맥 `tools/test_pcm.cpp` |
+| `main/jitter_buffer.h` | 지터 버퍼: 들쭉날쭉 도착하는 음성 프레임을 20ms 박자로 내준다(60ms 로 시작, 40~200ms) | 맥 `tools/test_jitter_buffer.cpp` |
+| `main/voice_core.h` | 목소리 통화의 판단: 켜고 끄기, 16ms 묶음 ↔ 20ms 프레임, 순번, 밀리면 버리기, 빠진 프레임 메우기, 시험음·지연 재기 | 맥 `tools/test_voice_core.cpp` |
+| `main/voice.h/.cpp` | 목소리 태스크 셋(캡처·해제·송신)과 에코 제거(esp-sr)·Opus 호출. `audio_out` 의 둘째 공급자 | 기기 |
 | `main/phone_main.cpp` | 부팅, 태스크 기동, 시리얼 명령 | 기기 |
 | `tools/phone_ctl.py` | 시리얼 검증 도구(기기 번호로 선택, 두 기기 시나리오, 스크린샷, 터치 주입) | `tools/test_phone_ctl.py` |
 | `tools/gen_ui_assets.py` | 고정 문구를 Pretendard 로 구워 `main/ui_assets.h` 를 만든다 | — |
@@ -40,7 +45,7 @@
 
 ```
 cd tools && SDK="$(xcrun --sdk macosx --show-sdk-path)"
-for T in wifi_store call_fsm relay_proto device_id ui_layout tones; do clang++ -std=c++17 -Wall -Wextra -isysroot "$SDK" -I../main test_$T.cpp -o /tmp/t_$T && /tmp/t_$T | tail -1; done
+for T in wifi_store call_fsm relay_proto device_id ui_layout tones pcm jitter_buffer voice_core; do clang++ -std=c++17 -Wall -Wextra -isysroot "$SDK" -I../main test_$T.cpp -o /tmp/t_$T && /tmp/t_$T | tail -1; done
 python3 -m pytest -q -p no:cacheprovider test_phone_ctl.py      # ESP-IDF 를 export 하지 않은 셸에서
 cd ~/tab5-toy-phone/server && python3 -m pytest -q -p no:cacheprovider   # 교환기 + 가짜 상대 (위의 cd tools 와 별개로 따로 실행)
 ```
@@ -97,7 +102,7 @@
 
 ## 시리얼 명령 (115200, 줄 단위)
 
-기기가 받는 명령: `stat` · `t <x> <y>` · `shot` · `reboot`.
+기기가 받는 명령: `stat` · `t <x> <y>` · `shot` · `reboot`. 진단용(사람이 켠다 — 아래 "목소리 통화"): `quiet on|off` · `selftest` · `vsrc aec|mic|ref|tone` · `vping` · `fault audio`.
 기기가 스스로 내는 줄:
 
 | 줄 | 뜻 |
@@ -106,9 +111,11 @@
 | `FSM <이전> -> <이후> acts=0x… notice=<이름>` (+ 번호가 있을 때만 ` msg=<1~3>`) | 통화 상태가 바뀜. `acts` 는 상태 기계가 낸 할 일 비트 |
 | `SCREEN <이름>` | 화면이 바뀜. `DECLINE` 은 메시지 고르기 화면 |
 | `RELAY up=<0\|1> code=<0\|401> attempt=<n>` | 교환기 연결 변화. `up=1` 의 `attempt` 는 항상 0 |
-| `STAT up=<초> unit= fsm= screen= wifi= relay= relay_code= peer= missed= touch= heap_int= heap_int_min= heap_psram= stk_app= stk_touch= stk_relay= stk_ws= audio= tone= aud_us= aud_err= stk_audio=` | `stat` 의 답. `touch` = 앱 태스크가 처리한 탭 수(실제 + 주입, 재부팅하면 0). 합격선: `heap_int_min` ≥ 100000, `stk_*` ≥ 1024. `stk_ws=0` 은 태스크가 아직 없는 부팅 직후 값. 끝의 다섯 칸은 소리(아래 "신호음") |
-| `AUDIO ready rate=16000 block=256 vol=100` / `AUDIO FAIL <단계>` | 부팅 때 소리 장치를 연 결과. `FAIL` 이어도 전화기는 소리 없이 계속 돈다(`STAT … audio=0` — 단 단계 `tones` 는 예외로 `audio=1` 로 남는 표시 불일치, 최종 검수 M-3). 단계: `lock`·`i2c`·`i2s`·`codec`·`amp`·`task`·`tones` |
-| `TONE <BELL\|RINGBACK\|END> on` / `… off` | 신호음이 시작됨 / 그침. 출력 태스크가 찍는다 — 원인이 된 `FSM` 줄과의 앞뒤는 정해져 있지 않다 |
+| `STAT up=<초> unit= fsm= screen= wifi= relay= relay_code= peer= missed= touch= heap_int= heap_int_min= heap_psram= stk_app= stk_touch= stk_relay= stk_ws= audio= tone= aud_us= aud_err= stk_audio= amp= aud_late= voice= vsrc= v_cap= v_tx= v_txdrop= v_txfail= v_rx= v_play= v_lost= v_drop= v_under= v_jb= v_aec_us= v_enc_us= v_dec_us= v_mic= v_ref= v_aec= v_spk= v_rderr= stk_vcap= stk_vrx= stk_vtx=` | `stat` 의 답. `touch` = 앱 태스크가 처리한 탭 수(실제 + 주입, 재부팅하면 0). 합격선: `heap_int_min` ≥ 100000, `stk_*` ≥ 1024. `stk_ws=0` 은 태스크가 아직 없는 부팅 직후 값. `audio=` 부터 다섯 칸은 소리(아래 "신호음"), `amp=` 부터는 목소리(아래 "목소리(Phase 3b)가 더한 것") |
+| `AUDIO ready rate=16000 block=256 vol=100 dma=3 mic=24 amp=<0\|1>` / `AUDIO FAIL <단계>` | 부팅 때 소리 장치(스피커 + 마이크)를 연 결과. `amp=0` 은 조용한 모드. `FAIL` 이면 3번까지 재시작해 다시 켜 보고(`PHONE audio-failed restart <1~3>`), 그래도 안 되면 `PHONE FAIL audio` 를 찍고 소리 없이 계속 돈다(`STAT … audio=0`). 단계: `lock`·`i2c`·`i2s`·`codec`·`amp`·`task`·`tones`·`voice`·`injected`(일부러 낸 실패) |
+| `VOICE ready chunk=256 opus_in=640 opus_out_max=160 bitrate=24000 aec=VOIP_HIGH_PERF` | 부팅 때 목소리 경로(에코 제거·Opus·태스크 셋)가 준비됨. 실패하면 `AUDIO FAIL voice` |
+| `VOICE on` / `VOICE off` | 통화가 열려 마이크를 읽기 시작함 / 통화가 끝나 닫힘. 그 원인인 `FSM` 줄 **바로 앞**에 찍힌다(같은 태스크 — 순서가 늘 같다) |
+| `TONE <BELL\|RINGBACK\|END> on` / `… off` | 신호음이 시작됨 / 그침. 소리를 만든 출력 태스크가 남기고 app 태스크가 다음 바퀴에 찍는다 — 원인이 된 `FSM` 줄과의 앞뒤는 정해져 있지 않다(대개 뒤). app 이 오래 멈춰 줄이 빠지면 `TONE ? lost=<n>` |
 | `OK t x y` | 주입한 터치가 대기열(8칸)에 들어감. 너무 빨리 보내면 가득 차 버려진다 |
 | `ERR t out-of-range` / `ERR unknown` | 좌표가 화면 밖인 `t` 명령 / 모르는 명령 |
 | `SHOT BEGIN … SHOT END` / `SHOT FAIL private` | 스크린샷. **입력이 든 비밀번호 자판 화면에서는 `SHOT FAIL private`** 로 거절한다(빈 자판·다른 화면은 찍힌다). 도구는 이 응답을 다시 받지 않고 실패로 끝낸다 |
@@ -121,9 +128,18 @@
 신호음(Phase 3a)이 더한 것:
 - `STAT` 끝의 칸: `audio=<0|1>`(소리 장치가 열렸나) · `tone=<NONE|BELL|RINGBACK|END>`(지금 내는 신호음 — 박자의 쉼 동안에도 그 이름) · `aud_us=<평균>/<최대>`(소리 한 묶음 16000µs 를 만드는 데 든 시간 µs. 기록 줄을 찍은 묶음이 최댓값이 된다) · `aud_err=<코덱 쓰기 실패 횟수>` · `stk_audio=<출력 태스크 스택 여유>`. 앞의 칸은 그대로다.
 - 한 통화에서 한 기기가 찍는 `TONE` 줄의 순서(맥 시험 `test_tones.cpp` "scenario …" 가 고정): 받는 쪽 `BELL on` → `BELL off`(받기·거절·벨 끄기·부재중 어느 것이든 한 번) → 통화가 끝나면 `END on` → `END off`. 거는 쪽 `RINGBACK on` → `RINGBACK off` → (거절당함·무응답·통화 뒤 끊김이면) `END on` → `END off`. 내가 걸다 취소하면 종료음이 없다.
-- `TONE … off` 는 만든 소리가 0 이 된 때다. 스피커에서 실제로 그치는 것은 앞서 내보낸 만큼(최대 약 0.1초) 늦다 — 드라이버 소스로 본 값이며 기기에서 재지 않았다.
+- `TONE … off` 는 만든 소리가 0 이 된 때다. 스피커에서 실제로 그치는 것은 앞서 내보낸 만큼(DMA 3칸 가운데 2칸 = 약 0.03초. Phase 3a 때는 6칸이라 약 0.1초) 늦다 — 드라이버 소스로 본 값이며 기기에서 재지 않았다.
 - 벨이 울리는지 볼 때는 `until:stat:^STAT .*fsm=INCOMING.*tone=BELL`, 벨 끄기 뒤에는 `…tone=NONE`. `TONE` 줄을 `wait` 로 기다릴 때는 그 앞뒤의 `FSM` 줄과 순서가 바뀔 수 있으니, 끝난 뒤 로그에서 기기별 `TONE` 줄만 뽑아 순서를 본다.
 
+목소리(Phase 3b)가 더한 것:
+- `STAT` 끝의 칸(앞의 칸은 그대로): `amp=<0|1>`(스피커 앰프 — 0 = 조용한 모드) · `aud_late=<출력이 48ms 넘게 늦어 스피커가 끊긴 횟수>` · `voice=<0|1>`(통화가 열려 마이크를 읽는 중) · `vsrc=<aec|mic|ref|tone>`(보내는 소리 — 평소 `aec`) · `v_cap=<마이크에서 읽은 16ms 묶음 수, 부팅 뒤 누적 — 통화 밖에서 늘면 안 된다>`.
+- 이번(또는 직전) 통화의 셈 — 통화가 열릴 때 0 으로: `v_tx`(보낸 프레임, 20ms 하나) · `v_txdrop`(밀려서 버린 것) · `v_txfail`(보내기·압축 실패) · `v_rx`(받은 프레임) · `v_play`(해제해서 낸 것) · `v_lost`(빠져서 메운 것) · `v_drop`(늦거나 넘쳐 버린 것) · `v_under`(해제 태스크가 굶어 무음을 낸 횟수) · `v_jb=<지터 버퍼에 든 프레임>/<목표>`.
+- 처리 시간 `v_aec_us`·`v_enc_us`·`v_dec_us` = `<평균>/<최대>` µs(에코 제거는 16ms 묶음마다, 압축·해제는 20ms 프레임마다). 부하(%) = 에코 제거 평균 ÷ 160 + (압축 평균 + 해제 평균) ÷ 200. 합격선 50%.
+- 최근 1초의 세기(RMS): `v_mic`(마이크 칸) · `v_ref`(스피커 되먹임 칸 — 코덱이 실제로 내고 있는 소리. 앰프를 꺼도 잡힌다) · `v_aec`(에코 제거 뒤) · `v_spk`(낸 상대 목소리). `v_rderr` = 마이크 읽기 실패. `stk_vcap`·`stk_vrx`·`stk_vtx` = 목소리 태스크 셋의 스택 여유.
+- 한 통화에서 한 기기가 찍는 줄의 순서: 거는 쪽 `VOICE on` → `FSM RINGBACK -> IN_CALL acts=0x180`, 받는 쪽 `FSM INCOMING -> ANSWERING acts=0x82` → `VOICE on` → `FSM ANSWERING -> IN_CALL acts=0x180`. 끊는 쪽 `VOICE off` → `FSM IN_CALL -> ENDING acts=0x248`, 상대 `VOICE off` → `FSM IN_CALL -> IDLE acts=0x240 notice=CALL_ENDED`(끊어졌으면 `notice=LOST`).
+- 진단 명령의 답: `OK quiet on amp=0` / `OK quiet off amp=1`(실패 `ERR quiet …`) · `SELFTEST amp=<0|1> mic=<n> ref=<n> aec=<n> ok=<0|1>`(통화 중이거나 소리 장치가 없으면 `SELFTEST FAIL busy-or-no-audio`) · `OK vsrc <이름>` / `ERR vsrc`(통화 중이 아니거나 모르는 이름) · `OK vping` 뒤 약 0.6초에 `VPING rt_ms=<n>`(3초 안에 못 찾으면 `VPING timeout`) / `ERR vping` · `OK fault audio`.
+- 목소리 자료(PCM·압축 프레임)를 시리얼로 내보내는 명령은 없다 — 나오는 것은 세기 숫자와 셈뿐이다.
+
 ### 주입 좌표 (`tap:<x>:<y>` — 각 버튼 안의 한 점. 대부분 가운데이고 취소·끊기는 실제 시험에서 쓴 값(가운데가 아님). 정본은 `main/ui_layout.h`)
 
 | 화면 | 버튼 | 좌표 |
@@ -174,9 +190,34 @@
 
 - **바꾸는 법**: 모양은 `main/tones.h` 의 `BELL_SEGS`·`RINGBACK_SEGS`·`END_SEGS`(한 칸 = 주파수 둘·길이 ms), 크기는 같은 파일의 `BELL_AMP`·`RINGBACK_AMP`·`END_AMP`(16비트 표본의 최고값, 코덱 음량 100 기준). 고친 뒤 맥 시험 `test_tones.cpp` 의 박자·크기 시험을 함께 고치고 → 빌드 → 두 기기 다시 플래시. 코덱 음량은 `main/audio_out.cpp` 의 `VOLUME`(지금 100 고정 — 음량 조절 화면은 Phase 5).
 - 소리가 없을 때 앰프는 켜진 채이고 I2S 는 0 을 낸다(켜고 끌 때의 퍽 소리를 피하려는 것 — PL3a-D4). 대기 중 쉬 소리가 거슬리면 그 결정의 탈출구를 본다.
-- 소리 장치가 열리지 않으면(`AUDIO FAIL …`) 전화기는 소리 없이 그대로 동작한다. 다시 시도하지 않는다 — 재부팅하면 다시 연다.
-- 스피커 출력은 송신 채널 하나만 연다(마이크는 Phase 3b). 출력 경로는 "공급자에게 PCM 을 받아 낸다"(`audio_out.h` 의 `add_source`)이고 신호음이 그 첫 공급자다.
+- 소리 장치가 열리지 않으면(`AUDIO FAIL …`) 3번까지 재시작해 다시 켜 보고, 그래도 안 되면 소리 없이 그대로 동작한다(화면 표시는 아직 없다 — `STAT … audio=0` 으로만 보인다). 재부팅하면 다시 연다.
+- 소리 장치는 스피커(I2S 송신)와 마이크(수신 4칸)를 함께 연다 — Phase 0 시험 펌웨어에서 실제로 돌았던 설정이다. 출력 경로는 "공급자에게 PCM 을 받아 낸다"(`audio_out.h` 의 `add_source`)이고 신호음이 첫 공급자, 목소리가 둘째다. 둘이 함께 소리를 내면 더한다(넘치면 자른다 — `pcm.h` 의 `mix_sources`).
 
+## 목소리 통화 (Phase 3b)
+
+받으면(`IN_CALL` 로 들어갈 때) 켜지고, 통화가 어느 길로 끝나든 그 자리에서 꺼진다. 받기 전에는 마이크를 읽지 않는다.
+
+```
+보내는 길  마이크 칩(칸0 = 마이크, 칸1 = 스피커 되먹임) → 에코 제거(16ms 묶음) → 4배 → 20ms 프레임 → Opus 24kbps → [순번 2][Opus] → 교환기
+받는 길    교환기 → 지터 버퍼 → Opus 해제(빠진 것은 직전 소리를 줄여 가며 메움) → 링버퍼 → 출력 태스크(신호음과 섞음) → 스피커
+```
+
+| 태스크 | 하는 일 | 코어·우선순위·스택 |
+|---|---|---|
+| `v_cap` | 마이크 읽기 → 에코 제거 → Opus 압축 → 보내기 대기열 | 1 · 7 · 48KB(내부 메모리) |
+| `v_rx` | 지터 버퍼에서 꺼내 Opus 해제 → 링버퍼 | 1 · 7 · 48KB(PSRAM) |
+| `v_tx` | 대기열 → 교환기로 보내기(밀리면 버린다) | 0 · 5 · 8KB |
+| `audio` | 16ms 마다 신호음·목소리를 받아 섞어 코덱에 쓴다(Phase 3a 그대로) | 1 · 6 · 6KB |
+
+- **지연**(말한 뒤 상대 스피커에서 나기까지)의 내역과 각 버퍼의 깊이는 계획서 "지연 예산". 버퍼를 바꾸는 곳: 출력 DMA `main/audio_out.cpp` 의 `DMA_DESC`(3칸 = 32ms 앞서 씀), 지터 버퍼 `main/jitter_buffer.h` 의 `START_FRAMES`·`MIN_FRAMES`·`OVERFLOW_FRAMES`, 보내기 대기열 `main/voice_core.h` 의 `TX_QUEUE`.
+- **크기를 바꾸는 법**(음량 조절 화면은 Phase 5): 내 목소리가 상대에게 작게/크게 들리면 `main/voice_core.h` 의 `TX_GAIN_Q8`(256 = 1배, 지금 1024 = 4배), 상대 목소리가 작게/크게 들리면 같은 파일의 `PLAY_GAIN_Q8`. 마이크 증폭은 `main/audio_out.cpp` 의 `MIC_GAIN_DB`(지금 24dB — 30dB 는 스피커 소리가 마이크에서 포화해 에코 제거가 지우지 못한다). 고친 뒤 맥 시험 → 빌드 → 두 기기 다시 플래시.
+- **조용한 모드** `quiet on`: 스피커 앰프를 끈다(재부팅 뒤에도 유지, `quiet off` 로 푼다). 벨·연결음·목소리가 모두 조용하지만 코덱은 그대로 돌아 `v_ref`·`selftest` 로 소리 경로를 기록으로 볼 수 있다 — 사용자 없이 시험할 때 쓴다. **켜 둔 채 잊으면 벨이 울리지 않는다**(`STAT … amp=0`, 부팅 줄 `AUDIO ready … amp=0`).
+- **혼자 하는 점검** `selftest`(통화 밖에서): 1kHz 시험음을 0.64초 내며 마이크 칸·되먹임 칸·에코 제거 뒤의 세기를 잰다. `ok=1` = 되먹임 칸이 300 이상 = 코덱이 소리를 내고 있다. 앰프가 켜져 있으면 `mic` 가 조용한 모드 때보다 훨씬 크다 = 스피커에서 실제로 소리가 났고 마이크가 들었다.
+- **보내는 소리 고르기** `vsrc aec|mic|ref|tone`(통화 중에만, 통화가 끝나면 `aec` 로 돌아간다): `mic` = 에코 제거를 거치지 않은 마이크, `ref` = 스피커 되먹임 칸, `tone` = 1kHz 시험음. **두 기기를 모두 `mic` 로 두고 스피커를 켜면 하울링이 난다** — 한쪽만 쓴다.
+- **지연 재기** `vping`(통화 중에만): 마이크 대신 "쉼 0.4초 → 삑 0.1초 → 쉼 0.5초"를 보내고, 상대가 돌려보낸 소리에서 삑을 찾을 때까지의 시간을 `VPING rt_ms=` 로 찍는다. 상대를 먼저 `vsrc ref`(전기적으로 돌려보냄 — 조용한 모드에서도 된다) 또는 `vsrc mic`(스피커 → 공기 → 마이크)로 둔다. 한쪽 길(입 → 귀) ≈ `rt_ms` ÷ 2 + 25ms.
+- **소리 장치 실패를 일부러 내 보기** `fault audio` → `reboot`: 다음 부팅부터 소리 장치를 열지 않고 실패로 친다 → 세 번 재시작 → 네 번째 부팅은 소리 없이 돈다(`PHONE FAIL audio`, `STAT … audio=0`) → 표시는 스스로 지워져 그다음 재부팅은 정상이다.
+- 두 기기를 같은 방에 두고 스피커를 켠 채 통화하면 서로의 스피커 소리가 상대 마이크로 들어가 메아리·하울링이 날 수 있다(에코 제거는 자기 스피커 소리만 지운다) — 시험은 다른 방에서 한다.
+
 ## 문구를 바꾸는 법 (메시지로 거절)
 
 `tools/gen_ui_assets.py` 의 `REJECT_MSGS`(순서 = 번호) → 다시 굽기(`python3 tools/gen_ui_assets.py --preview /tmp/tab5_phone_assets.png`, export 하지 않은 셸에서 — 미리보기 그림으로 글자가 깨지지 않았는지 눈으로 본다) → 빌드 → 두 기기 다시 플래시. 문구 **수**를 바꾸려면 `call_fsm.h` 의 `MSG_COUNT`, `ui_layout.h` 의 `Tap::MSG*`·`BTN_MSG`, `ui.cpp` 의 배열 둘을 함께 고친다(어긋나면 `static_assert` 가 빌드를 막는다). 현재 문구: 1 "회의 중이에요", 2 "지금은 받을 수 없어요", 3 "잠시 후 다시 걸게요".
````

그 뒤 손으로 고친다(**실제 값으로** — 못 잰 것은 "확인 못 함 — 이유"):
1. 머리의 "**귀 확인 대기**" 문단 → Task 9 의 결과로 바꿔 쓴다: 소리 나는 점검의 `mic` 값(조용할 때 ↔ 켰을 때), 점검표 14문항의 답 요약. Task 9 를 아직 못 했으면 "귀 확인 대기"를 그대로 두고 "목소리 경로는 스피커를 끈 채 기록으로 확인(Task 8)까지"라고 덧붙인다.
2. "맥 단위 시험" 절의 "지금 값" 두 줄 → Phase 3b 의 값으로: 커밋 SHA, `wifi_store 16 · call_fsm 105 · relay_proto 61 · device_id 9 · ui_layout 91 · tones 89 · pcm 37 · jitter_buffer 65 · voice_core 117`, 도구 pytest 218, 교환기 pytest 137, 빌드 크기(Task 7 Step 2 의 실제 값)·여유 58%. 두 기기에서 잰 것: `heap_int`(목소리 전 → 후)·`heap_int_min`, `stk_vcap`·`stk_vrx`·`stk_vtx`, 부하(조용할 때 / 말할 때), `VPING` 가운데 값과 한쪽 길 어림, 3분 통화의 `v_lost`·`v_drop`.
3. "복구" 절의 "지금 기기에 올라간 빌드와 보관본" 줄 → 지금 올라간 빌드(버전·`Compile time`·ELF 앞자리, `out/p3b_fw_version.txt`·`out/p3b_fw_build.sha256`)와 새 보관본 `out/fw_prev_5d9935f/build/`(신호음만 있는 판 — 되돌리는 명령은 이 계획서 "롤백").
4. 귀 확인으로 크기·지연 상수를 바꿨다면 "목소리 통화" 절의 해당 숫자.

- [x] **Step 2: 통화 설계서를 갱신한다** (`docs/exec-plans/2026-10-03-tab5-phone.md` — 원문은 지우지 않고 덧붙인다)

1. 머리의 `- **상태**:` 아래 "갱신" 줄 하나: Phase 3b(목소리)가 어디까지 됐는지(구현·기기 검증 A·B·사용자 확인).
2. §5 의 `- [ ] **Phase 3 — 소리 경로**` 아래에 들여 쓴 줄: "3b 목소리: 구현·기기 검증 완료(날짜) — 계획 `2026-10-04-tab5-phone-phase3b-voice.md`". Task 9 의 점검표에서 "서로 들린다·메아리가 거슬리지 않는다"는 답을 받았으면 Phase 3 의 체크박스를 `[x]` 로 바꾼다(아니면 그대로 두고 남은 것을 적는다).
3. §9.5 모듈 표 아래에 정정 한 줄: "`board_audio` 는 `audio_out`(스피커 + 마이크 장치)으로, `voice` 는 순수 헤더 `voice_core.h`·`pcm.h`·`jitter_buffer.h`(맥 시험)와 기기 연결 `voice.cpp`(태스크 셋)로 나뉘었다."
4. §9.6 아래에 **실측 지연** 절: 이 계획서 "지연 예산" 표의 구간별 값 옆에 기기에서 잰 것 — `VPING` 가운데 값(방향마다), 한쪽 길 어림(÷ 2 + 25ms), `v_jb` 의 깊이/목표, 사용자의 답(점검표 6번). 재지 못한 것(에코 제거 내부 지연, Opus 알고리즘 지연, 공기)은 "재지 못함"으로. 그리고 정정 세 건의 결과: 에코 제거 묶음 16ms(그대로), Opus 손실 은닉(부품이 해 주지 않는다 — PL3b-D7), 재생 DMA(6칸 80ms → 3칸 32ms — PL3b-D4).
5. §8 진행 로그 끝에 `### <날짜> — Phase 3b 목소리 결과 (계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md)` 절:
   - 커밋 아홉의 짧은 SHA, 시험 개수(맥 16·105·61·9·91·89·37·65·117, 도구 218, 교환기 137)와 실제 값이 다르면 그 값.
   - 빌드: 크기, 우리 코드 경고 수, 버전, 두 기기의 `App version`·`Compile time`·`ELF file SHA256` 이 같았는지, `AUDIO ready`·`VOICE ready` 줄.
   - Task 8 의 단계별 결과와 다시 돌린 횟수·이유: 혼자 하는 점검(`ref`), 실패 주입(4번 부팅), 시험음 통화(`spk`·`ref`), `VPING`, 30초·3분 통화(`lost`·`drop`·`jb`·`load`), 상대 재부팅, 연달아 통화, 3a 시나리오 C~G, 줄의 짝(`TONE` 54/54, `VOICE` 19/18).
   - 목소리가 쓴 것: `heap_int` 전 → 후, `heap_int_min`(기준 100000 과의 관계 — 미달이면 고른 탈출구), `stk_vcap`·`stk_vrx`·`stk_vtx`, 부하(조용할 때·말할 때), 터치 대조·비밀 결과.
   - Task 9: 소리 나는 점검의 `mic`(조용할 때 ↔ 켰을 때), `erle`, **사용자 점검표 14문항의 답**(그대로), 그에 따라 바꾼 것이 있으면 무엇을.
   - 설계서와 다르게 한 것: 이 계획서 진행 로그의 "설계서와 다르게 한 곳"을 가리킨다.
   - "3b(목소리)로 넘기는 것" 목록의 각 항에 "(3b 에서 처리 — PL3b-D…)" 또는 "남음"을 붙인다.

- [x] **Step 3: 3a 계획서의 귀 확인 칸을 채운다**

`docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md` 의 진행 로그에서 "사용자 귀 확인 대기"로 남은 곳 아래에 덧붙인다(원문은 지우지 않는다): Task 9 점검표 1·2·10·11·12 번의 답(= 3a 의 10문항에 해당), 소리 나는 점검의 `mic` 값, 그리고 "3b 가 소리 장치를 송수신으로 바꾼 뒤(PL3a-D7 → Phase 0 설정)의 확인이다 — 3a 의 송신 전용 설정으로 소리가 났는지는 끝내 확인하지 않았다"는 한 줄.

- [x] **Step 4: 이 계획서의 체크박스·상태·진행 로그를 갱신하고 커밋한다**

진행 로그에 `### <날짜> — 실행 결과 (Task 1~10)` 절을 더한다(커밋, Expected 와 달랐던 곳, 다시 돌린 실행, 기기 숫자, 점검표 요약). "확인하지 못한 것"의 각 줄에 "확인됨(값)" 또는 "남음"을 붙인다.

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/README.md docs/exec-plans/2026-10-03-tab5-phone.md docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md && git commit -m "docs(phone): Phase 3b 목소리 통화 구현·기기 검증·사용자 확인 결과 기록 — 실측 지연·부하·메모리, 3a 귀 확인 칸" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [x] **Step 5: 다음 단계로 넘기는 목록을 보고에 그대로 싣는다**

1. **Phase 4 에서 다시 잴 것**: 터널·`wss://` 를 거친 지연(`VPING` — 예상 한쪽 길 175~195ms), TLS 가 내부 메모리를 25~35KB 더 쓴 뒤의 `heap_int_min`(지금 값에서 빼 본다 — 모자라면 "CPU·메모리 예산"의 탈출구 ①), 서로 다른 망에서의 손실·지터(`v_jb` 의 목표가 얼마까지 오르는가).
2. **Wi-Fi 가 막힐 때**: 음성이 TCP 송신 버퍼에 최대 약 1초 쌓였다가 한꺼번에 가는 것(받는 쪽이 버린다), 끊기 신호가 최대 2초 + 3초 늦는 것(PL3b-D8) — 기기에서 일부러 만들어 보지 않았다.
3. 음량·음소거·이어폰(Phase 5): 지금은 상수 넷(`VOLUME`·`MIC_GAIN_DB`·`TX_GAIN_Q8`·`PLAY_GAIN_Q8`). 소리 장치가 죽었을 때의 화면 표시도 Phase 5.
4. 스택을 줄일 근거: `stk_vcap`·`stk_vrx`·`stk_vtx` 의 실제 여유(48KB·48KB·8KB 가운데).
5. 진단 명령(`vsrc`·`vping`·`selftest`·`fault audio`·`quiet`)을 제품에 남길지 — 지금은 시리얼로만 닿는다.
6. 점검표에서 나온 것(메아리·겹쳐 말하기·음량·지연에 대한 답)과 그에 따라 바꾼 상수. 메아리가 거슬린다는 답이면 PH-D7 의 후퇴(반이중 등)를 따로 정한다.
7. Phase 2 가 넘긴 것 가운데 남은 것: 터치 시각 기준(M-5), `app` 판단의 순수 구조체화(M-7), 버튼 겹침(M-1 화면), RL-D6(신호 보내기가 화면 태스크를 최대 2초 멈춤).
8. 맥북의 프로젝트 메모리(전화기 진행 상태)에 한 줄 — 컨트롤러 몫.

---

## 검증 기준 (이 계획의 완료 조건)

- [x] 맥 단위 시험: 기존 여섯 개가 **수정 없이** 통과하고(16·105·61·9·91, `tones` 84 → 89 는 추가만) 새 `pcm` `37 ok`·`jitter_buffer` `65 ok`·`voice_core` `117 ok`, 컴파일 경고 0. → Task 1·2·4·5, Task 6 Step 6 **[2026-10-05 확인 — 이 줄의 117 은 계획 당시 값이다. Task 1~7 에서 117 통과, 수정 묶음 2 이후 지금 `voice_core` **138**(기존 117 은 수정 없이 통과), 나머지 16·105·61·9·91·89·37·65 동일, 경고 0 — 이 기록을 쓰며 직접 다시 돌려 확인]**
- [x] 검증 도구 pytest `218 passed`(기존 186 을 고치지 않고 + 32), 교환기 pytest `137 passed`(그대로 — 교환기 코드는 바뀌지 않았다). → Task 6 Step 3·4·6 **[지금 **223**(수정 묶음 2 에서 +5), 교환기 137 — 직접 다시 돌려 확인]**
- [x] 펌웨어 전체 빌드 성공, 우리 코드 경고 0, `dependencies.lock` 그대로(부품 판 다섯 확인), 앱 파티션 58% 여유. → Task 7 Step 2 **[Task 7 첫 빌드 0x287af0 → 지금 `5935ddd` 0x2887a0(2,656,160 B), 매 빌드 경고 0·`dependencies.lock` 불변·58% 여유]**
- [x] 지금 기기의 빌드(`5d9935f`, 검사합 `4d15d9d1a8cb…`)가 `out/fw_prev_5d9935f/build/` 에 보관됐다. → Task 7 Step 1 **[`out/fw_prev_5d9935f/build/` 검사합 OK 3줄(그 뒤 `fw_prev_d77ae98`·`fw_prev_e40d097` 보관본도 있다)]**
- [x] 두 기기에 같은 바이너리, 둘 다 `AUDIO ready rate=16000 block=256 vol=100 dma=3 mic=24 amp=0` · `VOICE ready chunk=256 opus_in=640 opus_out_max=160 bitrate=24000 aec=VOIP_HIGH_PERF` · `SELFTEST … ok=1`. → Task 8 Step 3·4 **[`d77ae98`(Task 8)·`e40d097`(8b)·`5935ddd`(fix 3) 각각 두 기기의 `App version`·`Compile time`·ELF 가 같았고 `SELFTEST ok=1`. 부팅 줄의 `amp=` 는 조용한 모드일 때 0]**
- [x] 소리 장치 실패를 일부러 냈을 때: 부팅 4번·재시작 3번·`PHONE FAIL audio`·`audio=0` 으로 계속 돈다, 그 상태로 통화 흐름이 된다, 재부팅하면 돌아온다. → Task 8 Step 5 **[Task 8 Step 5 — `out/p3b_fault*.log`]**
- [x] **받기 전에는 마이크가 열리지 않는다**: 벨이 울리는 동안 `voice=0`, `VOICE on` 은 `… -> IN_CALL` 줄 바로 앞에만, 거절·취소·무응답에는 `VOICE` 줄이 없다. → Task 8 Step 6·8·9·12 **[최종 검수가 로그에서 확인: STAT 143줄에서 `voice=1 ⇔ fsm=IN_CALL`, `VOICE on` 21줄 전부 `-> IN_CALL` 바로 앞]**
- [x] **끝나면 닫힌다**: 끊는 쪽 `VOICE off` → `IN_CALL -> ENDING`, 상대 `VOICE off` → `IN_CALL -> IDLE`, 상대가 재부팅해도 `VOICE off` → `notice=LOST`, 통화 밖에서 `v_cap` 이 늘지 않는다, 켜기·끄기의 수가 맞는다(19/18). → Task 8 Step 8~11·13 **[Task 8: `VOICE on 19 / off 18`·`TONE 54/54`(로그 전체 합은 21 / 20 — 하나 많은 `on` 은 통화 중 재부팅된 2번의 것이고 켜진 채 끝난 통화는 없다), 통화 밖 `v_cap` 불변]**
- [x] 시험음 통화: 양쪽 `spk` 1100~1700·`ref` 300~1200(압축 → 교환기 → 해제 → 코덱 출력), `under=0`·`late=0`. → Task 8 Step 6 **[**1차 시도 실패**(1번 `v_under=1`, 원인 미확인·재현 안 됨), 2차 시도 통과: `spk` 1354·1358, `ref` 545·568, `under=0 late=0`]**
- [ ] 지연: `VPING` 가운데 값 80~320ms(두 방향), 한쪽 길 어림을 적었다. → Task 8 Step 7 **[**못 미침 — 조건 따라 다름**: 밤(`d77ae98`, 스피커 끔, 지터 버퍼 목표 2~3) 253·285ms 로 충족, **아침(`5935ddd`, 절전 끈 뒤, 목표 8) 540~573ms 로 미달**. 한쪽 길 어림은 적었다(약 151·167ms / 약 0.3초). 처리: 3c]**
- [ ] 30초·3분 통화: 프레임이 빠짐없이 흐르고(약 1500 / 9000), 메운 것·버린 것 2% 이하, `under=0`·`late=0`, 부하 50% 이하, `heap_int` 가 줄어들지 않는다. → Task 8 Step 8·9 **[**일부 미달**: 밤 3분 통화는 충족(받은 약 9,070 프레임, 메운 12 + 버린 22~23 = 약 0.37%, `under=0 late=0`, 부하 약 34%). 그러나 `heap_int` 가 통화 뒤 7~9KB 낮게 굳었고(누수는 아님), **아침 30초 통화는 2번이 2% 초과**(`5935ddd`: 메운 14 + 버린 38 = 52 ÷ 2번이 받은 1,460 프레임 = 약 3.6%, 1번은 메운 9 ÷ 받은 1,456 = 약 0.6%, 둘 다 `out/p3d_call30.log` 마지막 `STAT`). 처리: 3c]**
- [x] 3a 시나리오 A~G 의 `TONE` 줄 순서가 3a 와 같다(`on` 54 = `off` 54). → Task 8 Step 8·9·12·13 **[Task 8: `TONE` on 54 = off 54]**
- [ ] 주입한 탭 수와 `touch=` 증가가 일치, 비밀 0건, `stk_*`(새 `stk_vcap`·`stk_vrx`·`stk_vtx` 포함) ≥ 1024, `heap_int_min` ≥ 100000(아래면 값과 고른 탈출구를 적었다), `aud_err=0`·`aud_late=0`·`v_rderr=0`. 목소리가 쓴 메모리·CPU 를 적었다. → Task 8 Step 13 **[**`heap_int_min` ≥ 100000 이 못 미침**: 밤 최저 102,011 → 아침 1번 90,395·2번 80,463(멈춤선 60,000 위). 나머지(탭 수 일치·비밀 0건·`stk_*` ≥ 1024·`aud_err=0`·`aud_late=0`·`v_rderr=0`)는 충족. 값과 처리: 목소리가 쓴 메모리 약 142KB, 통화 뒤 내부 메모리가 1,668바이트 배수 단위로 줄어 굳음(최종 검수 판독: Wi-Fi 버퍼 풀 — 확인된 것은 아님) → 3c 의 메모리 되찾기]**
- [x] 스피커를 켰을 때 `SELFTEST` 의 `mic` 가 조용할 때의 5배 이상(스피커에서 실제로 소리가 난다), 시험음 통화의 `erle` 를 적었다. → Task 9 Step 1·2 **[`mic`(빌드 `e40d097`) 1번 3280 ↔ 12(약 270배)·2번 1211 ↔ 26(약 47배). `erle`: 한 번에 한 대의 스피커만 켜고 상대가 시험음을 보내는 방식으로 쟀다 — 1번 35.9·31.3dB, 2번 27.5·28.6dB]**
- [ ] **사용자 입·귀 점검표 14문항**에 답을 받았다(서로 들린다 / 메아리 / 동시에 말하기 / 끊김 / 지연 / 음량 / 신호음 / 하울링). → Task 9 Step 3 **[**일부만 답**: 4 "2번은 잘 들림"·9 "동시에 말해도 잘 됨. 다만 통화품질은 좀 아쉽긴 함"·5/6/7/8 은 아쉬운 점으로 "먹먹함·작음", "메아리·지연"(끊김·뭉개짐, 찢어지는 소리는 고르지 않음). 1·2·3·10·11·12·14 미답, 13 미수행. 상세는 통화 설계서 §8]**
- [x] `git status --short` 에 `out/`·`build/`·`secrets.h` 가 없다. **[2026-10-05 확인 — 이 기록을 쓰기 직전 `git status --short` 에 `out/`·`build/`·`secrets.h` 없음(수정한 문서 파일만 나왔다)]**
- [x] README·통화 설계서(§5·§8·§9.5·§9.6 실측 지연)·3a 계획서(귀 확인 칸)·이 계획서가 갱신됐고, 다음 단계로 넘기는 목록이 컨트롤러에게 갔다. → Task 10 **[2026-10-05 주: 이 줄을 처음 `[x]` 로 표시한 시점에는 보고서 `task-10-report.md` 가 아직 전달되기 전이었다. 지금은 보고서(넘기는 목록 전문 포함)가 전달됐고 검수 지적 반영 수정 라운드 1 도 끝나 조건이 충족돼 `[x]` 를 유지한다] **[이 과제(Task 10) 결과 — README·통화 설계서(§5·§8·§9.5·§9.6)·3a 계획서·수신거부 설계서·이 계획서가 갱신됐고 넘기는 목록은 보고서에 있다]**
- 이 계획 밖: 독립 검수(컨트롤러가 과제 묶음마다 맡긴다 — 묶음 1 = Task 1~3, 묶음 2 = Task 4~7), 통화 설계서 §6 의 검증 기준(서로 다른 망·지연 500ms — Phase 4 이후).

## 초안 목록

패치는 저장소 루트 기준 경로의 통합 diff 다. `D` = `~/tab5-toy-phone/.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/draft`. 같은 파일이 두 패치에 든 것이 다섯 있다(`audio_out.h/.cpp` = T1-03 → T3-01, `app.h/.cpp`·`phone_main.cpp` = T1-05 → T6-03 — 뒤 패치는 앞 패치를 적용한 모습에서 출발한다). 패치를 모두 적용한 뒤의 파일 전문은 `D/files/<저장소 경로>`(21개)에 있다. `verify/` 의 `touch_audit.py`·`secret_scan.py` 는 Phase 2·3a 에서 쓰던 것과 바이트까지 같고, `voice_check.py` 는 이번에 더했다(셋 다 저장소에 넣지 않고 그 자리에서 실행한다 — 본문에는 싣지 않았다).

| 초안 파일(`draft/` 기준) | 줄 수 | sha256 앞 12자 |
|---|---|---|
| `patches/T1-01-tones-tests.patch` | 57 | `2d65429cb532` |
| `patches/T1-02-relay-link-lock.patch` | 130 | `fa8d8aea7997` |
| `patches/T1-03-audio-out-fail.patch` | 123 | `73c57695ed85` |
| `patches/T1-04-tone-out-log.patch` | 104 | `7c408e173339` |
| `patches/T1-05-app-main.patch` | 104 | `a7fa67118b83` |
| `patches/T2-01-pcm-tests.patch` | 170 | `bdd3a3e466e8` |
| `patches/T2-02-pcm.patch` | 129 | `61b6b285fb82` |
| `patches/T3-01-audio-duplex.patch` | 363 | `ea41517fe21d` |
| `patches/T4-01-jitter-tests.patch` | 358 | `33d6ab1a73b1` |
| `patches/T4-02-jitter.patch` | 157 | `fe05977c1e94` |
| `patches/T5-01-voice-core-tests.patch` | 689 | `77ce800d5b2f` |
| `patches/T5-02-voice-core.patch` | 434 | `4a1ca543b847` |
| `patches/T6-01-voice.patch` | 447 | `129bf2dc7a72` |
| `patches/T6-02-phone-ctl-tests.patch` | 162 | `220137c10a83` |
| `patches/T6-03-app-main.patch` | 181 | `2bc324ecdccf` |
| `patches/T10-01-readme.patch` | 123 | `d2ca4c5a0d87` |
| `verify/voice_check.py` | 201 | `8e61a338460a` |
| `verify/touch_audit.py` | 38 | `fc39218464c8` |
| `verify/secret_scan.py` | 79 | `8350672ef27a` |

같은 작업 폴더(`D` 의 한 칸 위)에 계획을 만들고 검증한 도구가 있다(실행 담당은 쓰지 않는다): `make_draft.py`·`make_stage40.py`(단계별 사본 → 패치), `make_plan.py`(뼈대 + 패치 → 이 문서), `verify_sequence.sh`·`verify_rest.sh`(깨끗한 사본에 과제 순서대로 적용·시험·빌드), `mutate.py`·`dryrun/mut_fsm.py`(구현을 일부러 망가뜨려 시험이 잡는지), `dryrun/`(Task 1~7 의 명령을 임시 저장소에서, Task 8·9 의 명령을 가짜 전화기 두 대에 글자 그대로 돌려 본 도구), `research/`(Opus 해제기 역어셈블), `logs/`(그 기록).

## 진행 로그

### 2026-10-05 — 계획 작성

- 의뢰(`.superpowers/sdd/2026-10-04-tab5-phone-phase2-3/phase3b-plan-brief.md`)대로 계획서와 검증된 초안을 만들었다. 저장소의 코드·기기·시리얼·교환기·`build/`·`out/`·`secrets.h` 는 건드리지 않았다(이 계획서 파일 하나만 썼다. `build/` 의 바이너리 넷과 `out/p3a_fw_build.sha256` 은 보관 명령을 예행연습하려고 **읽어서** 저장소 밖으로 복사만 했다).
- 먼저 대조한 것(사용자 지시 "참고 구현 대조 우선"): 이 기기에서 마이크·에코 제거·Opus 를 돌린 Phase 0 시험 펌웨어(`tab5_phone_probe/main/board_audio.cpp`·`probe_main.cpp`)와 그 로그(`out/task4.log`·`task5.log`), 3a 최종 검수 보고서(M-1~M-9, "다음 단계가 바꿔야 하는 것" 11항), 실제로 빌드에 잡히는 부품 소스 — `esp_codec_dev` 1.6.2(`es7210.c` 의 증폭 레지스터, `esp_codec_dev.c`, `audio_codec_data_i2s.c`), ESP-IDF 5.5.4 의 I2S 드라이버(`i2s_common.c` — 빈 칸 대기열 = 칸 수 − 1), `esp_websocket_client` 1.8.0(`start` 가 전송 객체를 다시 만든다), `esp_audio_codec` 2.5.0(`esp_opus_dec.h`·README·라이브러리 역어셈블 — 손실 은닉 미지원, 할당 함수가 약한 기호), 교환기 `phone_core.py`·`phone_relay.py`(통화 중 이진 프레임 전달, 밀린 상대에게는 버림, 무소식 45초).
- 검증(저장소 밖 사본, 저장소 `4ace824` 기준. 기록은 작업 폴더 `logs/`):
  - 깨끗한 사본(`git archive HEAD`)에 패치 16개를 과제 순서대로 `git apply` 하며 실패 → 통과를 실제로 봤다. 실패 문구는 각 Step 의 Expected 그대로다(`'pcm.h' file not found` · `'jitter_buffer.h' file not found` · `'voice_core.h' file not found` · 도구 pytest `11 failed, 207 passed`). 최종 파일은 `D/files/` 와 21개 모두 같았다. 패치 16개 모두 지금 저장소에서 `git apply --check` 통과(누적).
  - 맥 단위 시험 16·105·61·9·91 **유지**, `tones` 84 → **89**, 새 `pcm` **37** · `jitter_buffer` **65** · `voice_core` **117**(전부 경고 0), 검증 도구 pytest 186 → **218**, 교환기 pytest **137** 유지. 기존 시험은 한 줄도 고치지 않았다(시험 패치의 삭제 줄 0).
  - 펌웨어 전체 빌드(자리 값 `secrets.h`)를 기준선과 **펌웨어 소스가 바뀌는 커밋 다섯 곳 모두**에서: 0x242230 → `relay_link` 잠금 0x2423b0 → 소리 실패·`TONE` 줄 0x242680 → 소리 장치 송수신 0x244660 → 목소리 모듈(아직 안 이음) 0x244660 → 연결 **0x287720**(+283KB — 에코 제거·Opus 라이브러리. 앱 파티션의 58% 여유). 모두 우리 코드 경고 0, `dependencies.lock` 은 저장소 것과 바이트까지 같다. 정적 내부 메모리(DIRAM)는 144,134 → 149,556 B(+5.4KB).
  - 새 시험이 실제로 잡아내는지 구현을 일부러 망가뜨려 확인: `pcm.h`·`jitter_buffer.h`·`voice_core.h` 87가지(넘칠 때 자르지 않고 돈다, 섞을 때 한쪽을 버린다, 링버퍼 색인, 받기 전에 목소리를 켠다, 끄기보다 켜기를 앞세운다, 앞 128ms 를 버리지 않는다, 대기열이 가득 차면 새것을 버린다, 묵은 것을 보낸다, 메울 때 줄이지 않는다, 닫힌 뒤에도 푼다, 다음 통화에 앞 통화의 소리를 남긴다, 채우는 중의 무음을 쌓는다, 순번을 넘긴다 등)와 `call_fsm.h` 10가지 — **전부 1개 이상 실패**했다. 처음에 안 잡힌 넷은 시험을 더해 잡았다(끊긴 뒤 이어 받을 때의 정리, 시계가 한 바퀴 돌 때의 "묵음" 판정, 닫힌 뒤의 헛깨움, 링버퍼에 미리 푸는 양의 상한).
  - 길게 돌린 것: 지터 버퍼 — 1시간 × 5 박자, 2시간(순번 5바퀴), 보내는 쪽·받는 쪽 시계 0.1% 어긋남 2시간, 20초마다 한 프레임 빠짐, 40ms~10초 멈춤 뒤 몰림, 손실·중복·순서 뒤바뀜 0~50% 16분씩, 무작위 500만 번. 받는 길 전체(지터 버퍼 → 메우기 → 링버퍼 → 16ms 묶음) — 같은 Wi-Fi 10분, 시계 ±0.02% 1시간, 터널 닮은 망 10분, 60ms~4초 멈춤, 통화를 여는 순간 64가지. 상태 기계 → 목소리 켜고 끄기 — 모든 상태 × 사건 × 사유 × 시각, 무작위 300만 사건. 링버퍼 — 무작위 200만 번 + 진짜 두 스레드 300만 표본.
  - **그 과정에서 찾아 고친 결함**: 미리 써 둔 지터 버퍼 초안은 받는 쪽 시계가 조금만 빨라도(0.1%) "여유 0 → 3프레임 메움 → 그 3프레임이 뒤늦게 와 버려짐 → 다시 여유 0"을 끝없이 되풀이했다(2시간에 45,180프레임을 잃음). 다 떨어져서 메울 때 순번을 넘기지 않게 고쳤다(JB-D2 — 고친 뒤 같은 조건에서 20초마다 한 프레임만 메운다). 초안의 시험 42개 가운데 이 동작을 고정하던 것은 새 동작에 맞게 고쳐 옮겼다.
  - Task 1~7 의 명령(bash 블록 34개)은 임시 저장소(`git archive HEAD` + `git init`)에서 **계획서의 글자 그대로**, 실행 담당과 같은 셸(Bash 도구의 zsh + ugrep)로 차례로 돌려 Expected 와 같음을 확인했다 — 커밋 아홉, 지금 `build/` 의 보관(검사합 `4d15d9d1a8cb…` 일치), 그 저장소에서의 전체 빌드(0x287720, 경고 0)까지.
  - Task 8·9 의 `phone_ctl.py` 명령은 도구의 실제 문법 검사를 통과했고, 플래시·교환기 확인·비밀 검사를 뺀 명령 전부(`pair` 와 `--unit run`, 후처리 `voice_check.py`·`touch_audit.py` 포함)를 **가짜 전화기 두 대**(진짜 도구의 단계 해석·대기열 코드 + 기기 로그의 `FSM` 줄 모양 + 펌웨어 소스에서 읽은 `STAT` 형식 + 새 `VOICE`·`AUDIO`·`SELFTEST`·`VPING` 줄, 재부팅·실패 주입 포함)에 계획서의 글자 그대로 돌려 봤다 — `TONE` 줄이 `FSM` 줄보다 먼저 올 때와 뒤에 올 때 둘 다 `rc=0`, 요약 줄과 `TONE` 54/54·`VOICE` 19/18, 터치 대조 일치가 Expected 와 같았다. 가짜 전화기의 숫자(세기·시간·메모리)는 꾸민 값이다 — 범위 판정이 도는지만 봤다.
- **설계서와 다르게 한 곳**
  - 통화 설계서 §9.5 의 모듈: `board_audio` → `audio_out`(3a 의 이름을 이어 스피커 + 마이크 장치), `voice`(음성 태스크 하나) → 순수 헤더 셋(`pcm.h`·`jitter_buffer.h`·`voice_core.h`) + `voice.cpp` 의 태스크 **셋**(캡처·해제·송신, PL3b-D15).
  - §9.6 "Opus 손실 은닉" → 부품이 해 주지 않아 직접 메운다(PL3b-D7 — 설계서의 정정이 예고한 후퇴 그대로). "재생 쪽 DMA 지연" → 6칸에서 3칸으로 줄였다(PL3b-D4). "벨 음량과 통화 음량은 따로" → 코덱 음량 하나(100)에 소리마다 디지털 크기, 마이크 증폭은 30dB 가 아니라 24dB + 디지털 4배(PL3b-D10).
  - §9.7 "소리 초기화 실패 → 부팅 화면에 오류 + 재시작 가드" → 재시작 가드는 넣었고(PL3b-D2), 화면 표시는 넣지 않았다(Phase 5).
  - PH-D6 "받기 전에는 마이크가 열리지 않는다" → 마이크 **장치**는 부팅 때 열어 두고 통화 밖에서는 읽지 않는 것으로 지켰다(PL3b-D20 — 하드웨어로 꺼져 있다고는 말할 수 없다).
  - 3a 계획의 PL3a-D7(송신 채널만)·D10(다시 켜 보지 않음)을 바꿨고(의뢰의 지시), D8(DMA 한 칸 256표본)은 수신에도 적용했다.
  - 의뢰의 "기기 검증을 두 번으로 나눌지" → 빌드 단계가 아니라 **소리를 내는가**로 나눴다(Task 8 조용히 / Task 9 사용자와). 올리는 것은 한 번이고, Task 8 안에서 불확실한 순서(장치가 열리나 → 코덱이 소리를 내나 → 디지털 경로 → 진짜 마이크)로 확인한다. 중간 커밋(Task 3 = 장치 + 신호음만)이 빌드되므로 좁혀 볼 길은 남아 있다.
  - 의뢰에 없던 것을 더했다: 조용한 모드(`quiet`)·혼자 하는 점검(`selftest`)·지연 재기(`vping`)·실패 주입(`fault audio`), `boot_guard` 가 재시작 전에 줄을 내보내는 0.2초, 소리 장치를 Wi-Fi 보다 먼저 여는 순서.
- **확인하지 못한 것** (기기에서만 보이는 것 — 위험이 큰 순서. Task 8·9 가 처음 본다)
  1. **어떻게 들리는가 전부** — 두 기기가 서로 물린 양방향 통화의 메아리, 겹쳐 말할 때 상대 말이 잘리는 정도, 음량(한 사람·한 거리의 Phase 0 녹음 하나에서 정한 값), 복잡도 0 의 음질, 통화가 열리고 끝나는 순간의 소리, 같은 방 하울링. 에코 제거는 Phase 0 에서 한 기기·녹음된 소리·잔향 감소 수치로만 봤다. 귀로만 판정된다(Task 9). → **[2026-10-05 상태] **일부 확인됨(사용자 귀) / 일부 남음** — "2번은 잘 들림", "동시에 말해도 잘 됨. 다만 통화품질은 좀 아쉽긴 함", 아쉬운 점 "먹먹함·작음"·"메아리·지연". **남음**: 같은 방 하울링, 통화가 열리고 끝나는 순간의 소리, 다른 방에서 양쪽 스피커를 켠 통화의 느낌(→ 2026-10-05 09시대 답 받음: "먹먹하게 작은 소리로, 아주 멀리있는 것처럼 들림."), A/B 비교(미답)**
  2. **내부 메모리가 기준(`heap_int_min` ≥ 100000)에 빠듯하다** — 예상 100~117KB. 에코 제거·Opus 가 내부 메모리를 얼마나 쓰는지는 재지 못하고 Phase 0 시험 펌웨어와의 차이에서 거꾸로 셈했다(65~80KB, 두 펌웨어의 구성이 달라 ±15KB). 기준 아래로 나오면 탈출구(캡처 스택을 PSRAM 으로)는 다시 빌드·플래시가 필요하다. → **[2026-10-05 상태] **확인됨(빠듯함이 현실) / 남음** — 부팅 직후 약 115.2K(약 142KB 사용), 밤 최저 `heap_int_min` 102,011, **아침 1번 90,395·2번 80,463 으로 기준 아래**(멈춤선 60,000 위). 탈출구(캡처 스택을 줄이는 것)는 3c**
  3. **소리 장치 설정에서 시험 펌웨어와 다른 곳**이 이 기기에서 돈 적이 없다 — DMA 3칸 × 256표본(수신 4칸 포함. 시험 펌웨어는 6칸 × 240), 마이크 증폭 24dB 를 부팅 때 바로 거는 것과 칩 레지스터 되읽기(판정이 틀리면 첫 부팅에서 `AUDIO FAIL codec` → 재시작 3번으로 드러난다), 되먹임 칸 0dB 를 명시해서 쓰는 것, 조용한 모드로 앰프를 켜지 않고 여는 것. → **[2026-10-05 상태] **확인됨** — 두 기기 모두 `AUDIO ready … dma=3 mic=24` 로 첫 부팅에서 `AUDIO FAIL` 없이 열렸고, `SELFTEST ok=1`, 스피커를 켠 `mic`(빌드 `e40d097`) 3280·1211, 조용한 모드의 앰프 끄기 `amp=0` 도 동작**
  4. **태스크 셋으로 나눈 구조의 박자** — 시험 펌웨어는 한 태스크였다. 코어 1 에 화면·터치·출력·캡처·해제가 함께 있을 때 16ms 박자가 지켜지는지(`v_under`·`aud_late`), PSRAM 에 둔 해제 태스크 스택(이 기기에서 PSRAM 스택 태스크를 돌려 본 적이 없다)과 그때의 Opus 해제 시간, 에코 제거·Opus 를 캡처 태스크 안에서 만드는 것이 부팅을 얼마나 늦추는지. → **[2026-10-05 상태] **확인됨(부팅 지연만 확인 못 함)** — 밤 3분 통화 `v_under=0`·`aud_late=0`(1차 시험음 통화에서 `v_under=1` 이 한 번, 원인 미확인), PSRAM 스택 `v_rx` 정상(`stk_vrx` 40644, 해제 최대 2478·3016µs). 에코 제거·Opus 를 만드는 것이 부팅을 얼마나 늦추는지는 확인 못 함 — 기록에 수치 없음**
  5. **부하** — Phase 0 의 30~37% 는 한 태스크·방 잡음에서 잰 값이다. 말소리·화면 갱신·교환기 송수신과 함께일 때는 Task 8(조용한 방)·Task 9(말할 때)가 처음 잰다. 송신 태스크(코어 0)의 부하는 어림도 없다. → **[2026-10-05 상태] **확인됨(송신 태스크만 확인 못 함)** — 부하 조용할 때 33.7~33.9%, 시험음 37%, 말소리 2번 약 43%·1번 약 40%(합격선 50%). 송신 태스크(코어 0)의 부하는 확인 못 함 — 그것을 재는 `STAT` 칸이 없다**
  6. **지연의 못 재는 조각** — 에코 제거 안의 지연(닫힌 라이브러리), 이 부품의 Opus 알고리즘 지연. `VPING` 은 그 둘과 마이크 묶음 대기·공기를 빼고 잰다. DMA 가 정말 (칸 수 − 1) × 16ms 만 앞서 쓰는지는 드라이버 소스로만 봤다(`VPING` 의 값이 예상 200ms 와 크게 다르면 여기다). → **[2026-10-05 상태] **일부 확인됨** — `VPING` 으로 왕복을 쟀다(밤 253·285ms → 한쪽 약 151·167ms, 아침 540~573ms → 약 0.3초). 에코 제거 안의 지연·Opus 알고리즘 지연·공기는 재지 못함. DMA 가 (칸 수 − 1) × 16ms 만 앞서 쓰는지는 소스로만 봄(`aud_late=0` 만 기록)**
  7. **Opus 해제기** — `esp_opus_dec_reset` 을 이 기기에서 불러 본 적이 없다. 프레임을 건너뛴 뒤(메운 뒤) 해제기 상태가 어긋나 나는 소리. 길이 0 입력으로 라이브러리의 은닉을 쓰는 탈출구는 역어셈블로만 봤다. → **[2026-10-05 상태] **확인됨(기록)** — 통화 여러 번(연달아 3번·재부팅 뒤 통화)에서 `esp_opus_dec_reset` 호출이 문제를 일으키지 않았고 `v_bad` 0. 소리가 어긋나는지는 사용자 귀에서 "끊김·뭉개짐"을 고르지 않은 것이 전부**
  8. **`relay_link` 잠금과 Wi-Fi 정체** — 실제 재연결과 음성 송신이 겹치는 상황, 소켓 쓰기가 2초 막히는 상황을 기기에서 만들지 않는다(교환기를 끄지 않는다). 코드 대조와 부품 소스로만. TCP 송신 버퍼에 쌓인 음성이 망이 풀릴 때 한꺼번에 가는 양. → **[2026-10-05 상태] **일부 남음** — 아침(`e40d097`)에 통화 중 2번의 웹소켓 쓰기 실패 → `RELAY up=0` → `LOST` → 자동 재접속이 **저절로 한 번** 일어나 통화가 끊기고 복구됐다. 그러나 재접속과 음성 송신이 겹치는 상황·소켓이 2초 막히는 상황을 일부러 만들어 보지는 않았다(최종 검수 지적 M-10: 그런 시험이 없었다 — 3c)**
  9. **통화가 열릴 때 버리는 128ms 가 충분한가** — 연결음 꼬리가 상대에게 가지 않는지, 받기 전의 소리가 새지 않는지는 계산(DMA 48ms + 멈춤 5ms + 앞서 쓴 32ms)과 맥 시험으로만. 실제 소리는 귀(점검표 3번). → **[2026-10-05 상태] **남음** — 연결음 꼬리가 상대에게 가는지·받기 전의 소리가 새는지는 계산·맥 시험뿐이고 사용자 귀(점검표 3번)는 미답**
  10. 시험음·되먹임 칸의 비율(0.42 — Phase 0 의 측정 한 번)에 기댄 판정 범위(`ref` 300~1200, 지연 재기의 검출 문턱 1500), 조용한 방에서 되먹임 칸의 잡음 크기. → **[2026-10-05 상태] **확인됨(한 가지는 약점)** — `ref` 545~592(범위 300~1200 안), 조용한 방의 되먹임 칸 잡음 `v_ref` 3~5. 지연 재기의 검출 문턱 1500 은 아침에 삑이 아닌 소리를 잡은 무효 값(29·44ms)이 둘 있어 간섭에 약하다**
  11. 정리한 것들의 기기 동작 — `TONE` 줄을 app 이 찍을 때의 실제 순서와 누락, 재시작 직전 줄이 시리얼에 보이는지(0.2초를 넣었다), `fault audio`·`quiet` 의 NVS 경로, 소리 장치를 Wi-Fi 보다 먼저 여는 순서가 부팅 시간에 주는 영향. → **[2026-10-05 상태] **확인됨(부팅 시간만 확인 못 함)** — `TONE` 줄 on 54 / off 54 가 `FSM` 줄과의 앞뒤 둘 다에서 맞았고, 재시작 직전 줄(`PHONE audio-failed restart 1,2,3`)이 시리얼에 보였고, `fault audio`·`quiet` 의 NVS 경로가 동작(재부팅 뒤 `amp=0` 유지). 소리 장치를 Wi-Fi 보다 먼저 여는 순서가 부팅 시간에 주는 영향은 확인 못 함**
  12. Task 8·9 의 명령 — 가짜 전화기에서만 돌려 봤다. 실제 기기에서의 시간(재부팅 뒤 다시 붙기까지, 상대 재부팅을 교환기가 알기까지), esptool·`lsof`·교환기 `status` 명령은 돌려 보지 못했다(형태는 3a 에서 실제로 쓴 명령 그대로). `secret_scan.py` 는 3a 때의 확인(가짜 값·모의 로그)에 기댄다 — 진짜 `secrets.h` 는 열지 않았다. → **[2026-10-05 상태] **확인됨** — 실제 기기에서 Task 8 의 명령이 전부 돌았다(시험음 통화 1회 재시도 외 첫 시도 통과). `esptool`·`lsof`·교환기 `status` 도 실제로 쓰였다. `secret_scan.py` 로 로그 6개 이상 검사 0건**
  13. 실제 `secrets.h` 로 빌드한 크기(어림 사본 + 0x3e0), 부품 로그 줄이 전화기 펌웨어의 로그 수준에서 보이는지. → **[2026-10-05 상태] **확인됨** — 실제 `secrets.h` 로 빌드한 크기(0x287af0 → 0x2887a0)와 부품 로그 줄(TDM RX·ES7210·`Adev_Codec: Open codec device OK`)이 보였다**
  14. Phase 4 의 조건(터널·TLS·서로 다른 망)에서의 지연·손실·메모리 — 이 계획 밖이다(맥 시뮬레이션의 어림만 있다). → **[2026-10-05 상태] **남음** — Phase 4 의 몫**

### 2026-10-05 — 실행 결과 (Task 1~10)

**먼저 구별해 둔다.** "기록으로 확인"은 로그·맥 시험·코드 읽기, "사용자 귀"는 사용자가 직접 한 말이다. **Task 9 는 부분 수행**이라 사용자 귀 확인은 일부뿐이다. 같은 내용의 통화 설계서 쪽 요약은 `2026-10-03-tab5-phone.md` §8 "Phase 3b 목소리 결과".

- **커밋(`4939aa6..5935ddd`)**: Task 1 `cc48926`·`b645f0d`·`eb9ece1` / Task 2 `c2ea96c` / Task 3 `682cdd1` / Task 4 `dc51862` / Task 5 `f1c6d23` / Task 6 `0ee869a`·`82142cc` / Task 7 은 커밋 없음(보관·빌드) / 과제 검수 반영 `d77ae98` / 최종 검수 수정 묶음 `cf002e4`·`3db643d`·`e40d097` / fix 3 `5935ddd`. 기기는 Task 8(`d77ae98`, 밤)·Task 8b(`e40d097`, 아침)·fix 3(`5935ddd`, 아침) 세 번 올렸다.
- **Expected 와 달랐던 곳**: ① 연결 커밋(Task 6 Step 6)을 자동 동기화가 먼저 담아 제목이 `sync:` 인 `82142cc` 가 됐다(내용은 패치와 `cmp` 동일). 수정 묶음 2 의 기기 코드도 `3db643d`(`sync:`)에 들어갔다. ② 시험음 통화(Task 8 Step 6) 1차 시도에서 1번 `v_under=1` 로 실패, 같은 실행을 다시 해 통과 — 원인 미확인·재현 안 됨. ③ `VPING` 2번 방향 285ms 가 예상 범위(160~260)를 넘었다(합격선 320 이내). ④ 통화 뒤 대기 중 `heap_int` 가 7~9KB 낮게 굳었다(누수 아님으로 판독). ⑤ 계획에 없던 수정이 둘 생겼다 — 최종 검수 수정 묶음(fix wave 2, PL3b-D21~D27)과 Wi-Fi 절전 끄기(fix 3, N-D4). ⑥ 합격선의 시험 개수(`voice_core` 117·도구 218)가 지금 값(138·223)과 다르다. ⑦ Task 9 는 계획대로 하지 못했다.
- **다시 돌린 실행**: 시험음 통화 1회(위 ②). Task 8b 점검에서 `VPING`·30초 통화가 기준에 못 미쳐, 진단으로 20초 통화를 두 번 더 돌렸다(`out/p3c_retest1.log`·`retest2.log` — 버림 약 18%). Wi-Fi 절전을 끈 뒤 같은 방법으로 30초 통화·`VPING`·메아리 제거 실측을 한 번씩 다시 했다(`out/p3d_call30.log`·`p3d_ping.log`·`p3d_erle_unit1/2.log`).
- **기기 숫자(요지)**: 부하 33.7~33.9%(조용할 때)·말소리 2번 약 43%·1번 약 40%, `heap_int` 약 257.3K → 약 115.2K(부팅 직후), `heap_int_min` 밤 102,011 → 아침 80,463~90,395(기준 아래), `stk_vcap` 25204·`stk_vrx` 40644·`stk_vtx` 7176, 3분 통화 메운 12·버린 22~23(약 0.37%), `VPING` 밤 253·285ms / 아침 540~573ms. 전체 값과 출처는 통화 설계서 §8 와 README "맥 단위 시험".
- **Task 9 — 부분 수행**: 기록으로 확인한 것(`SELFTEST amp=1` 의 `mic`(빌드 `e40d097`) 1번 3280 ↔ 12·2번 1211 ↔ 26, 메아리 제거 1번 31~36dB·2번 약 28dB, 한쪽 고리 이득 1번 약 9.3배·2번 약 3.4배, 말소리 통화 부하 2번 약 43%·1번 약 40%). 사용자 귀: "2번은 잘 들림. 1번은 안들림"(그때 1번 스피커는 꺼져 있었다) · "동시에 말해도 잘 됨. 다만 통화품질은 좀 아쉽긴 함" · 아쉬운 점 "먹먹함·작음", "메아리·지연"(끊김·뭉개짐, 찢어지는 소리는 고르지 않음). **미답/미수행**: A/B 비교, 신호음의 음량·잡음(귀 확인 창 07:59~08:04 에 사용자 조작 없음), 다른 방 양쪽 스피커 통화의 느낌(→ 2026-10-05 09시대 답 받음: "먹먹하게 작은 소리로, 아주 멀리있는 것처럼 들림." — 아래 맨 끝 로그), 같은 방 하울링. 점검표 14문항별 답은 통화 설계서 §8.
- **기록 창(2026-10-05 08:27경 ~ 약 20분, 2번 기기 `stat` 만 10초마다 — 컨트롤러 판정 P3B-R16, 진행 기록의 판정 번호)**: **기록 창(약 20분) 동안 통화 없음 — 양쪽 스피커를 켠 다른 방 통화의 기록·느낌은 아직 없다.** `out/p3e_tworoom_unit2.log`(`rc=0`): 2번의 `STAT` 121줄(`up` 1208 → 2402초)이 모두 `fsm=IDLE`·`voice=0`·`relay=1`·`peer=1`(1번은 다른 방에서 접속 중)이고, `touch`(10)·`v_rx`(1890)·`v_play`(1792)·`v_cap`·`heap_int_min`(80,463)이 처음 줄과 끝 줄에서 같았다(목소리 칸의 셈은 "직전 통화"의 값이 그대로 남은 것이다). 사용자의 느낌은 받지 못했다.
- **fix 3 의 검수(범위 한정, 8줄)**: Spec ✅ · Approved · Critical 0 · Important 0 · Minor(가벼운 지적) 3건. 명령이 Wi-Fi 칩까지 가는 길 확인(`esp_wifi_with_remote.c:110-113` → `esp_wifi_remote_with_hosted.h:98-101` → `esp_hosted_api.c:355-358` → `rpc_wrap.c:1524-1535`), 호스트 쪽에 설정이 사라질 길 없음. **미확인**: 칩(C6)이 혼자 리셋될 때 설정이 유지되는지. 미룬 Minor: 주석 문구 낮추기·`esp_wifi_get_ps` 값을 `STAT` 에 싣는 진단·`set_ps` 실패 시 재시도.
- **최종 검수(opus) 95/100**(Critical 0 · Important 0 · Minor 13)과 수정 재검수 96(잠정 — 기기 첫 실행 점검 통과 전까지는 95): 재검수가 점검한 수정 일곱 항목(검수 번호 G1~G7)이 전부 반영됨(ADDRESSED). 재검수의 사실 정정 — M5GFX 가 켠 앰프는 같은 `M5.begin` 안에서 M5Unified 가 다시 끈다(PL3b-D21 이유 정정).
- **정정(이 과제)**: ① PL3b-D21 이유(위 결정 표) — **실제로 "퍽" 소리가 났는지는 누구도 확인하지 못했다(소스 판독만, 기기 미확인).** ② 지연 예산 표: 계획의 한쪽 115ms 는 낙관적 → 실측 밤 150~185ms·아침 약 300ms. ③ 최종 검수가 "멈춤의 출처 미확정"이라 한 밤의 멈춤 네 번쯤: Wi-Fi 절전이었을 **가능성**(확인된 것은 아님). ④ 마이크 결합 어림: 최종 검수의 어림(M ≈ 1100, 3.3배)보다 1번 실측이 약 3배 크다(9.3배, +19dB).
- **설계서와 다르게 한 곳**: 계획 작성 때의 목록(위 "2026-10-05 — 계획 작성")에 더해 — 앰프를 코드가 쥔다(D21)·`vsrc mic|ref` 는 스피커가 꺼져 있을 때만(D22)·실행 중 조절 `txgain`·`vol`(D23·D24)·STAT 칸 분리(D25)·Wi-Fi 절전 끄기(N-D4).
- **작업 순서의 반성(원인 분류 — 절차 부재)**: Phase 2 조사 보고가 "Wi-Fi 절전은 Phase 3 에서 확인할 만하다"고 적어 둔 것이 3b 계획의 과제로 옮겨지지 않았다. 재발 방지는 3c 계획 작성 때 조사 보고의 "확인할 것" 목록을 과제로 옮기는 단계를 넣는 것(이 계획서에서는 고치지 않는다).
- **이 계획서의 남은 `[ ]`**: 검증 기준 넷(`VPING` 80~320ms · 30초·3분 통화 · `heap_int_min` ≥ 100000 · 점검표 14문항)과 Task 9 Step 2~4. 모두 값과 다음 처리(3c)를 해당 줄에 적었다.

### 2026-10-05 — 다른 방 통화의 귀 판정과 3c 첫 묶음 (이 계획서의 맺음 덧붙임)

- **사용자 귀 판정**(2026-10-05 09시대, 양쪽 스피커·서로 다른 방, 빌드 `5935ddd`, `txgain=4 vol=100`): "먹먹하게 작은 소리로, 아주 멀리있는 것처럼 들림." 메아리·지연은 이번에는 말하지 않았다(없다는 뜻인지는 묻지 않았다).
- **통화 기록**: `out/p3e_tworoom_unit2_b.log`(2번 기준, 09:01쯤) 약 4.8초 — `v_tx=240`·`v_rx=241`·`v_play=232`·`v_lost=7`(2.9%)·`v_drop=0`·`v_under=0`, 통화 중 `v_spk=1410`(끝 줄 447). `out/p3e_tworoom_unit1.log` 첫 `STAT` 줄(1번 기준, 약 33.5초 통화) — `v_rx=1678`·`v_play=1670`·`v_lost=7`(0.4%)·`v_drop=0`·`v_under=0`. 서로 다른 통화다.
- **컨트롤러의 판독(사용자의 말이 아님)**: 불만은 망 손실 탓이 아니라 크기·처리 쪽으로 보인다(0.4% 손실, `v_spk` 가 말소리로는 작음). 점검표에서는 이 한 문장을 7번(음량)의 답으로만 센다 — 6번(지연)·8번(메아리)은 미답 유지(상세는 통화 설계서 §8).
- **다음 = 3c 계획서** `docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md` — **첫 묶음은 2026-10-05 에 실행됨**(두 기기 `771b9fe`: 부팅 값 `lvl` 0→12 의 말소리 평탄화 + 새 손잡이 명령 + `TUNE` 줄, 되돌릴 판 `out/fw_prev_5935ddd/build/`). 점검 결과·열린 것(새 소리의 귀 판정 없음, 1번 메아리 가능성, `opus` 미사용, 캡처 스택 여유 8,820/32KB)과 R4 값에 붙는 조건(같은 맥북에서 시험 빌드가 돌았음)은 통화 설계서 §8 와 README "통화 품질 손잡이" 절에 있다. 이 계획서의 `[ ]` 넷(지연·손실·메모리·점검표)은 그대로이며, 메모리 항목은 3c 점검에서 `heap_int_min` 이 116,527·121,751 로 회복됐다(기준 100,000 위 — 단 3b 의 검증 조건과 같은 점검은 아니므로 이 박스는 `[ ]` 로 둔다).

- **검수 반영 덧붙임(2026-10-05)**: ① 메모리: 위 "3c 점검에서 `heap_int_min` 116,527·121,751 로 회복"은 통화를 거친 뒤의 값이고, 그 뒤 듣기 시도 통화 뒤 2번은 106,859(기준 위, `out/p3q_listen_ab.log`)이다. 올리기 전 값과의 비교는 같은 조건이 아니다(올리기 전 99,279·91,631 은 `5935ddd` 가 통화를 여러 번 거친 뒤의 대기값 — 같은 조건 비교는 부팅 직후 약 115.6K → 약 140K). ② 듣기 시도(무효)의 "1번 메아리 가능성"은 근거가 약하다 — 1번의 자기 기록(`out/p3q_after_listen_unit1.log`)에서 1번은 꺼진 적이 없고 마이크 세기(`v_mic=9630`, `v_ref=943`)가 제 스피커 결합으로 설명되지 않아 맥북 소리를 직접 들었을 가능성이 크다(컨트롤러 판독, 메아리 누출은 미확인; `mclip` 203/약 54초 ≈ 0.02%). ③ 합성 음성 측정(`out/p3q_lvl_speech.log`): `lvl 0` 에서 1번 `v_spk` 286·246·223·253 대 `lvl 12`(`g_q8=3072`) 에서 344→626→635→828, 말이 그친 뒤 42 대 45 — 평탄화가 작은 말소리 같은 입력을 올리고 잡음은 올리지 않음(숫자만, 귀 판정·사람 말소리 아님). 자세한 것은 통화 설계서 §8.
