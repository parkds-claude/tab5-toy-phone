# Tab5 음성 전화기 — Phase 3c 통화 품질 다듬기 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> (실행하는 에이전트에게: 위 두 스킬 중 하나로 과제를 하나씩 수행하고, 단계는 체크박스로 추적한다.)

- **작성일**: 2026-10-05
- **상태**: **1덩어리 실행됨**(2026-10-05 09:2x~) — Task 1~6 커밋(`199f00e`…`771b9fe`)·과제 검수 통과(Critical 0 · Important 0 · Minor 8), 두 기기 = `771b9fe`, 조용한 점검 R0~R5 통과, 스피커 켬. **사용자와 듣기 1회차(09:50)는 무효**(시험 차림의 잘못 — 판정 P3C-R6) → **`lvl`·`tilt`·`res` 를 귀로 고른 값은 아직 없다 = 두 번째 굳히기의 근거가 없다**(부팅 값은 `lvl 12`·나머지 `5935ddd` 그대로). **가장 큰 열린 위험: 받는 목소리가 커진 뒤 1번 기기가 메아리를 흘리는가**(Task 8 Step 4·결정 PL3c-D16). 사용자의 "속도" 지시로 **인계문(`chunk1-handoff.md`)만으로 먼저 실행했고 이 계획서는 그 뒤에 조립됐다**(컨트롤러 판정 P3C-R1) — Task 1~8 의 본문은 인계문과 같은 내용이고 결과는 끝의 "진행 로그"에 있다. Task 6b(시험 보강)도 들어갔다(`d3da36f`). **남은 것: Task 8 의 평소 통화 한 번 + 1번의 메아리 시험(Step 4), Task 7 의 측정 M1~M4, 2덩어리(Task 9~12).**
- **작성자**: Claude (맥북 세션)
- **넣는 자리**: 통화 설계서 §5 의 Phase 3(소리 경로) 뒤, Phase 4(맥미니 배포) 앞. 3b(목소리 통화)는 `2026-10-04-tab5-phone-phase3b-voice.md` 로 끝났고, 이 계획을 쓰기 시작할 때 두 기기에는 `5935ddd` 가 올라가 있었다(지금은 1덩어리의 `771b9fe`). 이 계획이 **3c = 사용자가 "아쉽다"고 한 통화 품질을 고치는 것**이다.
- **사용자 판정(이것이 요구사항이다)**
  - 2026-10-05 아침, 같은 책상·한 번에 한 대의 스피커: "동시에 말해도 잘 됨. 다만 통화품질은 좀 아쉽긴 함" — 아쉬운 점으로 고른 것 **"먹먹함·작음"**, **"메아리·지연"**(고르지 않은 것: 끊김·뭉개짐, 찢어지는 소리).
  - 2026-10-05 09:01 쯤, **두 방·양쪽 스피커**(`5935ddd`, `txgain=4 vol=100`): **"먹먹하게 작은 소리로, 아주 멀리있는 것처럼 들림."** — 아침의 "먹먹함·작음"이 같은 책상 배치 탓이 아니라는 뜻이다. **받는 목소리의 크기와 또렷함이 첫째**, 지연·메아리가 둘째다(이때 메아리·지연은 묻지 않았다 — "없다"는 뜻이 아니다).
  - 2026-10-05 09:18: "가자 가자. 속도를 내서 가자" — **1덩어리(크고 또렷하게)만 먼저** 올리고, 굳히는 일(2덩어리)은 맥미니로 옮길 때 한다.
  - 아직 답이 없는 것: 아침의 A/B(에코 제거를 거친 소리 대 마이크 그대로), 벨·연결음·종료음의 크기·잡음.

**Goal:** 상대의 말이 지금보다 **크게**(말소리의 꼭대기를 −6dBFS 로 — 지금보다 최대 +9.5dB), 넘치지 않게, 들리게 한다. 그리고 "먹먹함·지연·메아리"의 원인 후보마다 **다시 올리지 않고 바꿔 들을 수 있는 손잡이**와 **사람 없이 재는 수단**을 넣어, 사용자와 10분 듣고 고른 값을 부팅 값으로 굳힌다. 동시에 말하기·통화 흐름·기존 시리얼 줄은 지금과 같다(기존 시험을 고치지 않고 전부 통과).

**Architecture:** 판단은 전부 순수 헤더에 두고 맥에서 시험을 먼저 쓴다 — `voice_tune.h`(손잡이의 부팅 값·범위), `tx_dyn.h`(보내는 소리의 평탄화·부드러운 눌림·고음 올리기·남은 메아리 누르기), `jitter_buffer.h` 의 `Policy`(깊이를 정하는 규칙 — 기본은 3b 그대로), `voice_core.h`(시험음 주파수, 에코 제거를 거치는 시험음 `probe`, 도착 틈, 마이크 넘침 셈, 수신 무음 감시), `call_fsm.h`(사건 `VOICE_LOST` 하나). 기기 전용 코드(`voice.cpp`·`audio_out.cpp`·`relay_link.cpp`·`net_wifi.cpp`·`phone_main.cpp`)는 그것을 태스크·코덱·소켓·시리얼 명령에 잇기만 한다. 손잡이의 값은 저장하지 않고(재부팅하면 `voice_tune.h` 의 부팅 값), 새 값은 `STAT` 이 아니라 **새 줄 `TUNE`** 에 싣는다(`STAT` 은 한 글자도 바뀌지 않는다). 교환기는 바꾸지 않는다.

**Tech Stack:** ESP-IDF 5.5.4 (esp32p4) · M5Unified 0.2.23 / M5GFX 0.2.30 · esp_codec_dev 1.6.2(ES8388 + ES7210) · esp-sr 2.5.5(에코 제거 `AEC_MODE_VOIP_HIGH_PERF`) · esp_audio_codec 2.5.0(Opus) · esp_websocket_client 1.8.0 · lwIP 소켓 · clang++ (맥 단위 시험) · Python 3 + pytest + pyserial

**Spec:** 통화 설계서 `docs/exec-plans/2026-10-03-tab5-phone.md` §5·§6·§8("Phase 0 가정 검증 결과")·§9.4·§9.6·§9.7, 3b 계획서 `docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md`(결정 PL3b-D1~D27, "지연 예산"·"CPU·메모리 예산"), 3b 최종 검수의 Minor M-4·M-5·M-9·M-10·M-11 과 "Phase 4 전에 해야 할 것", fix 2 재검수 Minor 2·4·6·범위 밖 1, fix 3 검수 Minor ①(작업 폴더 `.superpowers/sdd/2026-10-04-tab5-phone-phase3b-voice/` 의 `final-review-report.md`·`fix-2-rereview.md`·`fix-3-review.md`·`progress.md`). 3c 는 별도 설계서 없이 **이 계획서가 설계를 겸한다** — 정한 것은 아래 "먹먹함·작음 — 후보별 판독"·"지연 예산"·"CPU·메모리 예산"·"이 계획에서 내린 결정"(PL3c-D1~D15)에 이유·비용·되돌리는 법과 함께 있다.

## 한눈에 보기 (사용자용)

| 과제 | 하는 일 | 기기 | 사용자가 할 일 | 상태(2026-10-05 10시) |
|---|---|---|---|---|
| **1덩어리 — 지금** | | | | |
| Task 1 | 전화기 안의 여유 메모리 24KB 를 되찾는다(새 기능을 얹기 전에) | 없음 | 없음 | 끝남 `199f00e` |
| Task 2 | 바꿔 들을 "손잡이"의 값·범위와, 들쭉날쭉한 망에서 지연을 덜 쌓는 버퍼 규칙을 넣는다(시험 먼저, 기본은 지금 그대로) | 없음 | 없음 | 끝남 `91adf23` |
| Task 3 | 사람 없이 재는 수단을 넣는다(여러 높이의 시험음, 망이 멈춘 길이, 마이크가 넘친 횟수) | 없음 | 없음 | 끝남 `657224a` |
| Task 4 | **작은 말은 키우고 큰 말은 누르는** 계산을 넣는다(시험 먼저). 고음 올리기·남은 메아리 누르기도 함께 | 없음 | 없음 | 끝남 `9de0596`(자동 동기화 커밋에 담김) |
| Task 5 | 위의 것들을 전화기에 잇는다(시리얼 명령 9개, 새 기록 줄 하나). 이 시점의 소리는 지금과 똑같다 | 없음 | 없음 | 끝남 `d285c33` |
| Task 6 | **말소리 키우기를 켠 값으로 굳히고**(한 줄) 지금 기기의 판(`5935ddd`)을 보관한 뒤 새로 빌드한다 | 없음(빌드만) | 없음 | 끝남 `771b9fe`, 검수 통과 |
| Task 6b | 시험을 더 촘촘히 한다(구현은 그대로) — 일부러 망가뜨린 83가지 가운데 처음에 못 잡은 11가지와 검수가 짚은 한 곳 | 없음 | 없음 | 끝남 `d3da36f`(기기는 그대로 `771b9fe`) |
| Task 7 | 두 기기에 올려 **스피커를 끈 채로** 확인한다: 옛 소리와 똑같이도 도는지, 키우기가 실제로 도는지, 메모리 | **1번·2번**(각 5분 안팎 다시 올림) | **두 기기를 맥북에 꽂아 주기**, 약 20분 화면을 만지지 않기(소리는 나지 않는다 — 다시 켜질 때 "퍽" 한 번은 날 수 있다) | 올리기·확인 끝남. 측정 M1~M4 는 **남음** |
| Task 8 | **사용자와 함께** 듣는다: 서로 다른 방에서 "가 / 나 / 같다" 다섯 문항 | 1번·2번(다시 올리지 않는다) | 두 기기를 다른 방에 두고 10분 듣기 | 1회차 무효 — **답 없음**. 다음: 평소처럼 한 통화 + 한 줄 답, 1번의 메아리 시험 |
| **2덩어리 — 맥미니로 옮길 때** | | | | |
| Task 9 | 상대가 사라지면 5초 안에 스스로 끊는다(지금은 17~45초 동안 마이크가 열려 있다) — 초안 검증됨 | 없음 | 없음 | 남음 |
| Task 10 | 검수가 미뤄 둔 작은 지적 넷 — 초안 검증됨 | 없음 | 없음 | 남음 |
| Task 11 | 듣고 고른 값을 부팅 값으로 굳히고 다시 올린다. 통화 중에 연결이 끊겼다 돌아오는 시험 | 1번·2번 | 두 기기를 맥북에 꽂아 주기 | 남음(30분) |
| Task 12 | 결과를 문서에 적는다 | 없음 | 없음 | 남음 |

- **1덩어리를 올리면 바로 달라지는 것**: 상대의 말이 **지금보다 최대 3배(+9.5dB) 크게** 온다. 가까이서 크게 말해 찢어지던 소리는 눌려서 깨끗해진다. 말이 없을 때의 방 잡음은 지금과 같은 크기다.
- **"먹먹하다"·"멀리 있는 것처럼"**: 크기를 올리면 한결 낫지만 전부는 아니다. 고음을 올리는 손잡이를 넣었고(Task 8 에서 들어 보고 정한다), 일부는 마이크와 입의 거리 탓이다 — 기기에서 30~50cm 안에서 말하면 크기가 눈에 띄게 오른다(거리가 반이면 +6dB). 이어폰은 Phase 5.
- **지연**(말한 뒤 상대 스피커에서 나기까지): 아침의 혼잡한 Wi-Fi 에서 약 0.3초였다. 그 절반 이상이 "끊기지 않게 모아 두는 버퍼"다 — 버퍼를 얕게 하면 약 0.19초가 되지만 망이 멈출 때마다 짧게 끊긴다. 어느 쪽이 나은지는 Task 8·11 에서 재고 듣고 정한다(지금 기본은 그대로).
- **메아리**(내 목소리가 돌아오는 것): 상대가 말하지 않을 때 보내는 소리를 더 누르는 손잡이를 넣었다. 1번 기기는 제 스피커 소리를 마이크로 유난히 크게 줍는다 — 목소리가 커진 만큼 넘칠 수 있어 그 횟수를 세고(`mclip`), 넘치면 마이크 증폭을 내린다. **이것이 지금 가장 큰 열린 위험이다** — `mclip` 은 가까이서 크게 말해도 오르므로 "1번 쪽이 조용하고 상대만 말할 때"의 값으로 가른다(Task 8 Step 4).
- 두 기기는 시험 뒤에도 전화기로 남는다. 되돌리는 법은 아래 "롤백"(각 5분). 급하면 시리얼로 `lvl 0`(옛 소리) 또는 `quiet on`(스피커 끔).
- **실행자에게 — 코드를 옮겨 적지 않는다.** 이 계획에 실린 패치와 도구는 맥북의 `~/tab5-toy-phone/.superpowers/sdd/2026-10-05-tab5-phone-phase3c-quality/draft/`(이하 `D`, git 제외, 맥북에만 있음, **읽기 전용**)에 그대로 있다. 계획서의 코드 블록과 글자 단위로 같다(조립 스크립트가 끼워 넣었다 — 목록과 검사합은 문서 끝 "초안 목록").
  - 수정·새 파일은 `D/patches/TN-MM-….patch` 를 저장소 루트에서 `git apply` 로 적용하고, 적용한 뒤의 파일을 `D/files/<저장소 경로>` 와 `cmp` 한다. `D/files` 는 2덩어리까지 적용한 **마지막** 모습이다 — 1덩어리 끝에서 `cmp` 가 같은 파일은 Task 6 Step 2 에 적었다.
  - **Task 1~6·6b 의 패치(T1~T6b, 13개)는 이미 저장소에 들어가 있다** — 다시 적용하지 않는다(적용되지 않는다). 지금 저장소에 적용되는 것은 `T9-01`·`T9-02` → `T10-01`·`T10-02` 네 개다.
  - **순서를 지킨다**: 시험 패치 → 실패 확인 → 구현 패치 → 통과 확인 → 커밋. 한꺼번에 적용하지 않는다. 적용한 파일을 고치지 않는다. Expected 와 다르면 스스로 고치지 말고 멈춰서 보고한다(실제 출력 첨부).
  - `D/verify/` 의 도구(`voice_check.py`·`touch_audit.py`·`secret_scan.py` — 3b 것 그대로, `freeze_tune.py`·`tune_report.py` — 이번에 더한 것)는 저장소에 넣지 않고 그 자리에서 실행한다.
  - 1덩어리만 따로 정리한 인계문: 같은 작업 폴더의 `chunk1-handoff.md`(이 계획서의 Task 1~8 과 같은 내용).
- 이 계획에 실린 코드는 작성 시점(2026-10-05)에 저장소 밖 사본에서 실제로 돌려 확인했다(기준 = 1덩어리를 얹기 전의 `782dc45` — 펌웨어·시험·교환기 코드는 `5935ddd` 와 같다): 깨끗한 사본에 패치 17개를 과제 순서대로 적용하며 실패 → 통과를 확인(맥 단위 시험 16·105·61·9·91·89·37 유지, `jitter_buffer` 65 → **90**(Task 2) → **92**(Task 6b), `voice_core` 138 → **158**(1덩어리) → **176**(2덩어리), `call_fsm` 105 → **114**(2덩어리), 새 `voice_tune` **17** · `tx_dyn` **34**(Task 4) → **46**(Task 6b), 도구 pytest 223 → **237**(1덩어리) → **241**, 교환기 pytest 137 유지), Task 6 까지 적용한 사본이 **저장소에 실제로 커밋된 `771b9fe` 와 파일 하나까지 같음**을 확인, **커밋 경계마다** 펌웨어 전체 빌드 성공(0x288450 → 0x288450 → 0x288760 → 0x288770 → 0x288770 → 0x28a390 → 0x28a390 → 0x28a510 → 0x28a410, 앱 파티션의 58% 여유, 우리 코드 경고 0, `dependencies.lock` 불변), 순수 헤더를 일부러 망가뜨린 83가지를 시험이 전부 잡음(처음에는 11가지가 살아남아 시험 12개를 더 썼다 = Task 6b — 구현은 고치지 않았다). **작성자는 기기를 만지지 않았다.** 1덩어리는 컨트롤러가 실행해 기기에서 확인했다 — 결과와 아직 확인하지 못한 것(위험이 큰 순서)은 끝의 "진행 로그".

## Global Constraints

통화 설계서·앞 계획들에서 온 전 과제 공통 요구사항. 모든 과제는 이 절을 암묵적으로 포함한다.

- **기기 번호**: 1번 = MAC `AA:BB:CC:DD:EE:01`(교환기 ID `tab5-1`), 2번 = MAC `AA:BB:CC:DD:EE:02`(`tab5-2`).
- **기기는 번호(MAC)로 고른다**: 포트는 항상 `python3 tools/phone_ctl.py port <1|2>` 로 얻고(포트를 열지 않는다), 플래시 직전에 `read_mac` 으로 다시 확인한다. `/dev/cu.usbmodem*` 를 직접 고르지 않는다. **어느 기기가 맥북에 꽂혀 있는지는 가정하지 않는다**(2026-10-05 하루에도 세 번 바뀌었다) — 기기 과제의 첫 단계가 `port 1`·`port 2` 로 보이는 기기를 확인하고, 안 보이는 기기는 사용자에게 꽂아 달라고 부탁한다.
- **코드 과제(Task 1~6, 6b, 9, 10)는 기기·시리얼 포트·교환기를 건드리지 않는다.** 기기는 Task 7·8·11 에서만.
- **칩·빌드**: ESP-IDF 5.5.4, 대상 `esp32p4`, 두 기기 모두 P4 rev v1.3. **`esp_hosted` 2.11.7 고정**, Wi-Fi 칩(C6) 펌웨어 교체 금지, `amcchord/M5Tab5-VideoLink` 플래시 금지, `esp_audio_codec` 2.5.0, `esp-sr` 2.5.5, `esp_codec_dev` 1.6.2. `sdkconfig.defaults`·`main/idf_component.yml`·`main/CMakeLists.txt` 를 고치지 않는다(새 파일은 전부 헤더다). `dependencies.lock` 은 바뀌지 않아야 한다.
- **범위**: 고치는 파일은 아래 "초안 목록"의 패치가 건드리는 것이 전부다. 교환기(`server/`), 화면(`ui.cpp`·`ui_layout.h`·`ui_assets.h`), `relay_proto.h`·`tones.h`·`pcm.h`·`tone_out.*`, 도구(`phone_ctl.py`·`phone_fake_peer.py`), `server/tools/phone_relay_dev.sh` 를 고치지 않는다. **기존 시험은 한 글자도 고치지 않는다**(시험 패치는 추가만 — 조립 스크립트가 삭제 줄 0 을 확인했다). **기존 시리얼 줄의 모양은 한 글자도 바꾸지 않는다** — `STAT`·`AUDIO ready`·`VOICE ready`·`SELFTEST`·`VPING` 은 그대로이고 새 값은 새 줄 `TUNE` 에만 싣는다(도구 시험이 펌웨어 소스의 형식 문자열을 읽어 고정한다).
- **비밀**: `secrets.h`·`server/.env.phone` 을 `cat`/Read 로 열지 않는다. 그 내용(Wi-Fi 망 이름·비밀번호, 토큰, 교환기 주소)을 화면·로그·보고·커밋에 내지 않는다. **목소리 자료(PCM·압축 프레임)를 시리얼·파일로 내보내는 기능은 없다** — 새 줄에 나오는 것은 세기·셈·설정값뿐이다.
- **이 맥의 함정**: ① 맥 단위 시험은 `clang++ … -isysroot "$(xcrun --sdk macosx --show-sdk-path)"` 가 있어야 링크된다. ② `source ~/esp/esp-idf-5.5.4/export.sh` 를 한 셸의 `python3` 에는 Pillow·pytest 가 없다 → 파이썬 도구·시험은 export 하지 **않은** 호출에서, 빌드·esptool 은 export 와 **같은 호출 안에서**. ③ 명령을 `sh -c '…'` 로 감싸지 않는다. ④ Bash 도구의 `grep` 은 ugrep 이다 — 파일 여러 개를 한 번에 주면 순서가 실행마다 다르다. 오래 걸리는 명령은 Bash 의 `timeout` 을 600000 으로. 셸 글에서 `$변수` 바로 뒤에 한글을 붙이지 않는다(`${변수}` 로). ⑤ **이 맥북이 교환기다 — 빌드에는 `nice -n 10` 을 붙인다**(통화를 흔들지 않게).
- **main 에서 직접 작업**(브랜치·worktree 없음). devsync 가 5분마다 `git add -A` 로 자동 커밋한다 → `git commit` 이 "nothing to commit" 이면 devsync 가 먼저 커밋한 것이라 정상이다(보고에 적는다). `git add` 는 그 과제의 파일만(`git add -A` 금지). 임시 파일은 저장소 밖(`/tmp`)이나 git 제외 폴더 `firmware/tab5_phone/out/` 에만 둔다.
- **교환기**: 지금 맥북에서 켜져 있고 사용자가 쓰고 있다. 이 계획은 교환기 코드를 바꾸지 않으므로 **다시 띄우지 않는다** — `bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh status` 로 켜져 있는지만 본다(단독 호출). 꺼져 있으면 켜지 말고 멈춰서 보고한다. Task 11 의 "교환기를 내렸다 올리는" 단계만 **컨트롤러가** `restart` 한다.
- **두 기기에는 같은 바이너리를 올린다**: 빌드는 한 번만 하고, 플래시는 `idf.py flash` 대신 빌드 폴더에서 `python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash @flash_args`(직전에 `read_mac`). 2번 → 1번 순서. 기기별로 다른 상수를 굽지 않는다.
- **소리**: 두 기기의 스피커가 지금 켜져 있다(`quiet off`) — 사용자의 아이가 통화 중일 수 있다. 기기 과제는 **사용자에게 알리고, 두 기기가 `fsm=IDLE` 인 것을 본 뒤** 시작하고, **재부팅·플래시 전에 `quiet on`** 을 건다(스피커를 켠 채 재부팅하면 "퍽" 소리가 날 수 있다 — 소스 판독, 기기 미확인). 소리가 나는 시험(Task 8)은 사용자가 자리에 있을 때만, 밤에는 하지 않는다. 스피커가 켜진 동안 `vsrc mic`·`vsrc ref` 는 펌웨어가 거절한다(`ERR vsrc speaker-on`). `selftest`·`vping` 의 판정은 `TUNE` 의 `meas=1`(`txgain=4`·`vol=100`)일 때만 유효하다.
- **멈추라는 조건에 걸리면 멈춘다**: BLOCKED 로 보고하고 기다린다. 기기 시험에서 결함이 나오면 고치지 않는다 — 진단해서 보고한다.
- **하드웨어 상태를 적을 때는 "소스 판독"과 "기기에서 확인"을 구별한다.** 측정값에는 조건(빌드·손잡이 값·같은 방/다른 방·시각)을 붙인다. 측정 출력은 전부 `out/p3q_*` 파일로 남긴다(화면에서 본 값만 적지 않는다 — fix 3 검수의 지적).
- **참고 구현 대조 우선(사용자 지시 2026-10-03)**: 기기에서 이상한 동작이 나오면 추측하지 않는다. Phase 0 시험 펌웨어(`firmware/tab5_phone_probe/`)와 그 로그(`out/task4.log`·`task5.log`)와 줄 단위로 대조한다.
- **진행 안내·보고는 전부 한국어.**

## Review Focus

이 계획이 암시하지만 과제의 시험이 놓치기 쉬운 입력·고장 다섯 가지 — 일어나기 쉬운 것부터. 각 줄의 시험은 해당 과제에 들어 있다.

1. **이득을 키웠더니 넘치거나, 잡음이 커지거나, 고리가 돈다**(가까이서 지르는 소리, 말 없는 방의 잡음, 상대가 말하는 동안의 내 이득, 에코 제거가 아직 자리 잡지 못한 첫 몇 초, 커진 상대 목소리가 1번 기기의 마이크에서 넘침) → Task 4 `test_tx_dyn.cpp` "10 million random samples with every knob setting: the output never wraps around"·"a sudden shout: squeezed under full scale from the first sample, and the gain is down to x1 within one block"·"lvl 16, room noise only: goes out exactly as loud as with the fixed gain — never louder"·"I learned x16 on my quiet speech, then the far end starts while I keep talking: the gain is at or under the fixed gain from the first block on"·"echo canceller not converged (echo 2x the loopback): not pressed, not boosted"·"a steady tone … learned as background noise within 12 s", Task 6b "the far end starts while my gain is x12: inside that one block (16 ms) the gain only comes down, sample by sample, and ends at the fixed gain"·"... and from the second block on no sample is above the fixed gain while the far end is heard"(1덩어리 검수 Minor 1 — 첫 16ms 는 내려오는 중이다), Task 3 "mic clip count", Task 7 R5(상대 소리가 나는 동안 `g_q8 ≤ 1024` — 기기에서 확인됨)·Task 8 Step 3 의 `mclip` 읽는 법·Step 4(1번의 메아리 시험 — **아직 열려 있다**).
2. **손잡이가 기본 동작을 바꿔 놓는다**(손잡이를 넣은 것만으로 소리·줄 모양·지연이 달라짐, 굳힌 값이 범위 밖, 손잡이를 바꾼 채 `selftest`·`vping` 을 돌려 고장처럼 보임) → Task 4 "off (lvl 0, res 0): bit for bit the fixed gain of 3b, clipping included"·"res on but the far end silent: still bit for bit", Task 2 `test_jitter_buffer.cpp` "policy: setting the legacy rule explicitly changes nothing — 2 million random pushes / pops give identical output and counters"·`test_voice_tune.cpp` "whatever the boot defaults are, each lies inside its knob's range"·"selftest / vping verdicts are valid only at send gain x4 and volume 100", Task 5 pytest `test_phase3c_stat_line_is_exactly_the_5935ddd_line`·`test_phase3c_tune_line_format_is_pinned`, `voice_tune.h` 의 `static_assert` 여덟 개, Task 7 R4(같은 바이너리에 `lvl 0` → `TUNE legacy=1` 로 3b 의 회귀 그대로).
3. **지터 버퍼를 얕게 했더니 끊김이 늘거나, 나쁜 망에서 소리의 대부분을 버린다**(0.15~0.25초 멈춤이 5초마다, 0.3초마다 뭉쳐 오는 망 — 절전이 다시 켜진 경우, 통화 시작 직후 붙어 오는 끊김, 통화 중 규칙 바꾸기) → Task 2 "ceiling: underruns 3 s apart …"·"burst rule: two underruns close together (common right after a call opens) do not pass the knob"·"burst rule: a third and further underruns within 1 s … may climb past the knob"·"catch-up: the tuned rule waits 0.5 s, then drops one frame every 0.1 s — never more than one frame (20 ms) at a time"·"3 million random pushes / pops / rule changes / resets", Task 3 `test_voice_core.cpp` "sim, the morning network: … the price — more underruns …, still over 96 % of the frames played"·"sim, frames arriving in 0.3 s clumps (Wi-Fi power save back on): the burst rule falls back to the 3b behaviour", Task 7 M4·Task 11(기기에서 `jb 0` 대 `jb 5` 의 끊김·지연).
4. **무음 감시가 멀쩡한 통화를 끊는다 / 끊어야 할 통화를 못 끊는다**(4.9초 멈췄다 풀리는 망, 소리 장치가 죽어 프레임을 한 번도 안 보내는 상대, 받기 전, 시계가 한 바퀴 도는 순간) → Task 9 `test_voice_core.cpp` "rx watch: before the first frame it never fires, however long"·"rx watch: stalls of 4.9 s, fifty times over, do not cut the call"·"rx watch: it fires at 5 s of silence, exactly once"·"rx watch: the millisecond clock wrapping around …"·"state machine with the new 'voice lost' event: voice is on exactly when the state is IN_CALL (every state x 15 events x reason)", `test_call_fsm.cpp` "voice lost outside a call does nothing in any state", Task 11 Step 4(통화 중 상대 재부팅 → 5~8초 안에 `notice=LOST`).
5. **메모리 되찾기가 스택 넘침·굶음을 만든다**(말소리 통화에서의 Opus 스택, PSRAM 스택의 송신 태스크가 끊김 핸들러를 돌릴 때, 망이 몰릴 때의 `heap_int_min`) → Task 1 pytest `test_phase3c_the_capture_stack_is_32k_internal_and_the_sender_stack_is_in_psram`(모양만 — 동작은 기기), Task 7 R3(`stk_vcap` 약 26,600·`heap_int` +24KB — 확인됨)·R4(`stk_vcap ≥ 6000`, `v_under=0`, `aud_late=0` — 확인됨, 최저 8,820)·Task 8(말소리 통화 뒤 `stk_vcap ≥ 6000`, `heap_int_min ≥ 100000` — 듣기 1회차에서 8,820·106,859), Task 11 Step 3(통화 중 연결 끊김 — `stk_vtx`).

---

## 비목표·영향 범위

**넣지 않은 것**(한 줄씩):

- 맥미니 배포·터널·`wss://`·토큰 교체 → Phase 4. 음량·음소거·이어폰 화면, 화면 모양 변경 → Phase 5.
- 교환기를 유선 기기로 옮기기·맥북을 5GHz 로 바꾸기 → 하지 않는다(맥북 주소가 바뀌면 `secrets.h` 를 다시 만들어 올려야 한다 — Phase 4 에서 풀린다). "지연 가운데 망이 차지하는 몫"은 아래 지연 예산에 적었다.
- 에코 제거의 방식·세기 바꾸기 → 하지 않는다(PL3c-D13: VOIP 방식은 세기를 못 바꾼다 — 라이브러리 역어셈블로 확인). Opus 복잡도·가변 비트율·FEC·DTX → 하지 않는다(PL3c-D8).
- esp-sr 의 자동 음량(`esp_agc`)·잡음 억제(`esp_ns`) → 쓰지 않는다: 둘 다 10ms 묶음 전용이고(`managed_components/espressif__esp-sr/include/esp32p4_less_v3/esp_agc.h:26`, `esp_ns.h` 의 `ns_pro_create` 주석) 우리 묶음은 16ms 다. 자동 음량은 "상대가 말하는 동안 올리지 않는다"를 넣을 자리가 없다 — `tx_dyn.h` 를 직접 썼다(PL3c-D7).
- 마이크 증폭을 **올리는** 것 → 하지 않는다(PL3c-D9).
- "양보 한 번"(3b 최종 검수 M-5 의 첫 습관 — 지터 버퍼가 비었을 때 10ms 쯤 더 기다렸다 메우기) → 넣지 않았다: 멈춤 길이가 0.15~0.25초에 퍼져 있으면 그 가운데 10ms 띠에 드는 것은 열에 하나 이하이고, 해제 태스크에 시간 대기를 넣으면 출력 태스크가 묶음을 연달아 요구하는 순간(깨어난 직후)과 부딪혀 `v_under` 를 만든다.
- 진단 명령을 빌드 옵션으로 묶기, 부팅 패닉 고리 방어 → **Phase 4 첫 과제로 넘긴다**(PL3c-D14). 손잡이는 Task 11 의 굳히기까지 쓰여야 하고, 묶는 일은 명령 스무 개의 시험을 건드린다.
- 목소리 녹음·내보내기(진단용 포함) → 넣지 않는다.

**영향 범위**

- 저장소 파일: 새 파일 4개(`main/voice_tune.h`·`tx_dyn.h`, `tools/test_voice_tune.cpp`·`test_tx_dyn.cpp`), 고치는 파일 17개(`main/voice.h`·`voice.cpp`·`voice_core.h`·`jitter_buffer.h`·`audio_out.h`·`audio_out.cpp`·`relay_link.h`·`relay_link.cpp`·`net_wifi.h`·`net_wifi.cpp`·`phone_main.cpp`·`call_fsm.h`·`app.cpp`, `tools/test_jitter_buffer.cpp`·`test_voice_core.cpp`·`test_call_fsm.cpp`·`test_phone_ctl.py`) + Task 12 에서 글만(`README.md`·통화 설계서·이 계획서).
- 기기: Task 7 에서 2번·1번 순서로 다시 플래시(각 5분 안팎, 저장된 Wi-Fi 목록·SD 카드·조용한 모드 설정은 그대로). Task 11 에서 한 번 더. NVS 에 새 열쇠는 없다(손잡이는 저장하지 않는다).
- 교환기: 건드리지 않는다. 과금 없음. 비트율 손잡이를 40kbps 로 올리면 한쪽 방향 약 59kbps(기본 24kbps 는 약 43kbps 그대로).
- 새 산출물은 git 제외 폴더 `firmware/tab5_phone/out/` 에만 생긴다(`p3q_*`, `fw_prev_5935ddd/`).

## 롤백

- **그 자리에서(다시 올리지 않고)**: 시리얼 `lvl 0` — 소리 경로가 `5935ddd` 와 표본 하나까지 같아진다(재부팅하면 부팅 값으로 돌아온다). 다른 손잡이도 `tilt 0`·`res 0`·`mic 24`·`opus 24`·`jb 0`·`nodelay 0` 이면 `TUNE legacy=1`. 급하면 `quiet on`.
- **기기 → 직전 펌웨어**(`5935ddd`, Task 6 Step 1 이 `out/fw_prev_5935ddd/build/` 에 보관): `cd ~/tab5-toy-phone/firmware/tab5_phone && source ~/esp/esp-idf-5.5.4/export.sh > /dev/null 2>&1 && PORT=$(python3 tools/phone_ctl.py port 2) && python -m esptool --chip esp32p4 -p "$PORT" read_mac | grep -i "aa:bb:cc:dd:ee:02" && cd out/fw_prev_5935ddd/build && python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash @flash_args` (1번은 `port 1`, `aa:bb:cc:dd:ee:01`). 스피커가 켜져 있으면 먼저 `quiet on`. 이 명령은 실행해 본 적이 없다(모양은 3b 에서 `build/` 로 올린 명령과 같다). 그 앞의 판: `out/fw_prev_e40d097/`(절전 끄기 직전)·`fw_prev_d77ae98/`(밤에 검증된 목소리 빌드)·`fw_prev_5d9935f/`(신호음만)·`fw_prev_066cf57/`(소리 없음).
- **코드**: 커밋을 뒤에서부터 `git revert` 한다. Task 6 의 커밋 `771b9fe`(1차 굳히기)만 되돌리면 부팅 값이 `5935ddd` 와 같아진다(손잡이는 남는다). `d285c33`(Task 5) → `9de0596`(Task 4 — 자동 동기화 커밋 `sync: 2026-10-05 09:35` 가 `tx_dyn.h`·`test_tx_dyn.cpp` 두 파일을 담았다) → `657224a`(Task 3) → `91adf23`(Task 2) → `199f00e`(Task 1) 순서로 되돌리면 전부 사라진다(Task 5 를 남기고 2~4 를 되돌리면 빌드되지 않는다 — `voice.cpp` 가 `tx_dyn.h`·`voice_tune.h` 를 쓴다). Task 6b 는 시험 파일뿐이라 아무 때나 되돌려도 된다. 2덩어리는 Task 10 → 9 순서.
- **교환기**: 바뀌지 않는다.

## 먹먹함·작음 — 후보별 판독

숫자는 전부 `firmware/tab5_phone/out/` 의 로그에서 읽었다. `v_mic`·`v_ref`·`v_aec`(캡처 쪽)는 **최근 약 1초(62묶음)의 RMS**, `v_spk`(받는 쪽)는 **최근 1초(50프레임)에 푼 소리의 RMS** 다 — 말 사이의 쉼이 들어 있어 "말하는 순간의 크기"보다 낮게 나온다(`voice.cpp` 의 `rms_blocks`, `voice_core.h` 의 `rms_frames_`). dBFS 는 32768 기준.

| 후보 | 지금의 숫자(조건) | 판독 | 이 계획이 하는 것 |
|---|---|---|---|
| (다) **보내는 크기가 작다** | 같은 책상(`p3d_ab_aec_mic.log`, 08:1x): 말하는 쪽 `v_mic` 204~554 → `v_aec` 70~279 → 4배 해서 280~1116(−41~−29dBFS). 두 방(`p3e_tworoom_unit2_b.log`, 09:02): 받는 쪽 `v_spk` 1410·447(−27·−37dBFS). 통화에 알맞은 말소리는 −26~−20dBFS | **주된 원인.** 고정 4배가 상한(`txgain` 1~4)이라 10~17dB 모자란다. 더 올리면 가까이서 말할 때 넘친다(두 방 통화의 한 순간 `v_aec`=695 → 4배에 꼭대기가 이미 1만 안팎) | `tx_dyn.h` 평탄화(`lvl`): 말소리의 꼭대기를 16000(−6dBFS)으로, 이득 1~12배(손잡이로 16배까지). **1덩어리의 부팅 값 `lvl 12`**(PL3c-D12). 맥 시험: 작은 말 +12.0dB(`lvl 16`), 큰 말은 꼭대기 16476 으로 눌림(고정 4배는 32767 에서 잘림) |
| (라) 재생이 작다 | `vol=100`(0dB) 이 코덱의 상한(`esp_codec_dev.c:79-115`). 받은 소리가 −27~−37dBFS 라 DAC 의 위 10dB 이상을 쓰지 못한다 | (다)의 결과다. 받는 쪽에서 따로 키울 것이 없다 | 없음(보내는 쪽에서 맞춘다 — 압축도 큰 신호를 다루게 된다) |
| (가) 에코 제거가 목소리를 깎는다 | Phase 0: 메아리가 없는 구간에서 목소리 감쇠 약 1dB(설계서 §8 H2). 아침 같은 책상: `v_aec`÷`v_mic` = −4~−9dB — 단 그때 되먹임 칸에 상대 소리가 있었다(`v_ref` 233~3956: 내 목소리가 옆 기기를 거쳐 0.3초 늦게 돌아온 것) → **겹쳐 말하는 조건**이라 깎인 것. 두 방: `v_mic`=1035 → `v_aec`=695(−3.5dB, `v_ref`=514) | 크기는 상대가 조용할 때 1dB 쯤, 상대 소리가 날 때 3~9dB. **고음을 깎는지는 모른다**(닫힌 라이브러리) — 재야 안다. 아침의 A/B 는 겹쳐 말하는 조건이라 다시 해야 한다 | 재는 수단: `vsrc probe` + `vtone`(Task 7 M2 — 주파수별 `v_aec`÷`v_mic`). 세기 손잡이는 없다(PL3c-D13). 보완: `lvl`(크기)·`tilt`(고음) |
| (나) Opus 24kbps·복잡도 0 | 16kHz 모노·VOIP·고정 24kbps(`voice.cpp` 의 `OPUS_BITRATE`·`OPUS_COMPLEXITY`). 시험음 1kHz 는 압축을 거쳐 그대로 나온다(`v_spk` 1350~1414) | 광대역 말소리에 24kbps 는 모자라지 않는다. 다만 아주 작은 입력(−50dBFS)을 압축해 왔다 — (다)를 고치면 함께 풀린다. 고음 쪽은 재지 않았다 | 재는 수단: `vsrc tone` + `vtone`(Task 7 M1 — 주파수별 받는 쪽 `v_spk`, `opus 24` 대 `opus 40`). 손잡이 `opus 12~40`. 복잡도는 못 바꾼다(PL3c-D8) |
| (마) 스피커·마이크·거리 | 1번은 제 스피커를 마이크로 2.3~2.7배로 줍고 2번은 0.86배(`p3c_loud_unit1/2.log`, `p3d_erle_unit1/2.log`). 작은 내장 스피커, 책상 위 먼 마이크 | "멀리 있는 것처럼"의 몫 — 방의 울림이 함께 잡힌다. **소프트웨어로 못 고친다.** 거리가 반이면 +6dB | 재는 수단: 스피커를 켠 채 주파수별 받는 쪽 `v_mic`(Task 8 뒤 M5). 듣기 문항 3(가까이/멀리). 사용 안내에 "30~50cm". 이어폰은 Phase 5 |

## 지연 예산 (입 → 귀)

지연 재기(`vping`)의 왕복 값에는 보내는 쪽의 ③④⑤⑥ + 받는 쪽의 ⑦⑧⑨ + 되돌아오는 길의 ①③④⑤⑥ + 재는 쪽의 ⑦ 이 들어 있다(3b 계획서의 정의) → **한쪽 길 ≈ 왕복 ÷ 2 + 25ms + ②**. "실측"은 `VPING` 줄의 왕복 값과 그때의 `jb=깊이/목표`, "판독"은 그것을 구간에 나눈 것이다.

| 구간 | 3b 예산 | 밤 실측·판독(`p3b_ping.log`, d77ae98, 02시) | 아침 실측·판독(`p3d_ping.log`, 5935ddd, 08:2x — 2.4GHz 혼잡) | 3c 로 기대하는 값(아침 같은 망, `jb 5`·`nodelay 1`) | 근거 |
|---|---|---|---|---|---|
| ① 캡처 묶음(16ms) | 0~16 (평균 8) | 8 | 8 | 8 | 드라이버(묶음 256) |
| ② 에코 제거 안의 지연 | 모름(0~16) | 모름 | 모름 | 모름 | 닫힌 라이브러리 — 재지 못한다 |
| ③ 16ms 묶음 → 20ms 프레임 | 0~32 (평균 16) | 16 | 16 | 16 | 맥 시험 "uplink: 4 frames per 5 blocks" |
| ④ Opus 알고리즘 지연 | 6.5 | 6.5 | 6.5 | 6.5 | libopus 의 일반 값(이 부품에서 확인하지 않았다) |
| ⑤⑥ 보내기 + 망 + 교환기(맥북이 같은 Wi-Fi) | 3~7 | 약 9(모아 보내기 포함) | **약 40** — 왕복 540~573 에서 고정 구간 95 와 두 지터 버퍼(깊이 9·8~10 프레임) 360 을 뺀 나머지의 절반 | 25~40(`nodelay` 로 10~15 줄 것으로 **기대** — 재 봐야 안다) | `VPING … jb=9/8`·`jb=10/8` |
| ⑦ 지터 버퍼 | 40~60 | 약 70(목표 3~5) | **160~200**(목표 8, 한때 10 = 상한) | **80~100**(상한 5) | `v_jb`, 맥 시뮬레이션 |
| ⑧ 링버퍼 | 0~20 | 10 | 10 | 10 | 맥 시험 "rx: it never decodes further ahead than one block + one frame" |
| ⑨ 출력 DMA(3칸) | 32 | 32 | 32 | 32 | 그대로 둔다 — 쓰기 간격 최대가 25.0ms 까지 나왔다(`aud_gap=24963`·`25086`): 2칸(여유 32ms)이면 남는 것이 7ms |
| ⑩ DAC·스피커·공기 | 1~3 | 2 | 2 | 2 | — |
| **합(② 빼고)** | 약 115 | **약 155**(왕복 253~301 → 한쪽 151~176) | **약 295**(왕복 540~573) | **약 185~215** | |

- 맥 시뮬레이션(`test_voice_core.cpp --info`, 구간 ①③⑥⑦⑧ — 마이크 표본이 받는 쪽 출력 태스크에 넘어가기까지. 180초 × 4회 평균):

  | 망(흉내) | 규칙 | 평균 지연 | 95% | 끊김/분 | 메운 프레임/분 | 버린 프레임/분 | 튼 비율 | 목표 평균(최대) |
  |---|---|---|---|---|---|---|---|---|
  | 조용함(3~7ms) | `jb 0`(3b) / `jb 4`~`8` | 81 / 81 | 81 | 0 | 0 | 0 | 99.99% | 2.1(3) / 2.0(3) |
  | 밤(0.15~0.25초 멈춤이 45초에 한 번) | `jb 0` | 113 | 130 | 1.4 | 4.2 | 9.0 | 99.67% | 2.3(3) |
  | 〃 | `jb 4`~`8` | 106 | 125 | 1.4 | 4.2 | 9.4 | 99.66% | 2.1(3) |
  | **아침**(흔들림 0~40ms, 0.15~0.25초 멈춤이 5.5초에 한 번, 0.8초 멈춤이 1분에 한 번) | `jb 0` | **263** | 317 | 6.8 | 15.2 | 31.7 | 98.83% | 8.8(10) |
  | 〃 | `jb 4` | **134** | 151 | 11.8 | 35.0 | 89.9 | 96.97% | 3.8(4) |
  | 〃 | `jb 5` | **151** | 177 | 11.8 | 34.9 | 79.9 | 97.29% | 4.6(5) |
  | 〃 | `jb 6` | 172 | 216 | 11.7 | 34.1 | 69.1 | 97.63% | 5.4(6) |
  | 〃 | `jb 8` | 208 | 262 | 10.4 | 27.8 | 53.2 | 98.14% | 6.7(8) |
  | 절전(0.3초마다 뭉쳐 옴 — fix 3 전) | `jb 0` / `jb 4`~`8` | 329 / 329 | 381 | 159 | 285 / 287 | 354 / 369 | 87.95% / 87.44% | 9.8(10) |

  읽는 법: 아침 같은 망에서 3b 의 규칙은 목표가 상한(200ms)까지 올라가 머문다 — 끊김이 5초마다 와서 "10초 무사"가 오지 않는다. 상한을 4~5 로 두면 지연이 0.11~0.13초 줄고, 값으로 끊김이 분당 6.8 → 11.8 번, 버리는 소리가 분당 0.6초 → 1.6~1.8초가 된다(멈춤이 버퍼보다 길면 어느 쪽이든 끊긴다 — 깊은 버퍼는 그 절반쯤만 막는다). 조용한 망·밤의 망에서는 차이가 없거나 조금 낫다. **기기의 아침 실측은 흉내보다 나빴다**(끊김 분당 6.6~10.9 — `p3d_call30.log`·`p3d_ab_aec_mic.log`) — 흉내의 숫자는 "방향과 크기"로 읽는다.
- **1덩어리 뒤 실측**(`out/p3q_r4_ping.log`, `771b9fe`, `lvl 0`·`jb 0`·`nodelay 0`, 09:42 — 기기 보고): 왕복 가운데 값 1번 445ms · 2번 430ms(한쪽 약 240ms), 시간 초과 0. 아침(540~573)과 밤(253~301)의 사이 = 그 시각의 망이 그 사이였다(직전 시험음 통화의 `v_jb` 목표 6~7). 30초 통화: 메움 0.26~0.32%·버림 0.13%·튼 비율 99.5~99.6%·`v_under=0`. 1덩어리는 지연을 건드리지 않았으므로(부팅 값 `jb 0`·`nodelay 0`) 이 값은 "기준선의 또 한 점"이다.
- 가장 큰 덩어리는 ⑦(지터 버퍼)이고 그다음이 ⑤⑥(망)이다. ⑤⑥ 가운데 **맥북이 같은 Wi-Fi 로 교환기 노릇을 하는 몫**(음성이 Wi-Fi 를 두 번 거친다)은 이 계획이 못 줄인다 — Phase 4 에서 교환기가 맥미니(유선)로 가면 한 번이 된다.
- **합격 기준**(Task 11 에서 굳힌 값으로): 아침과 같은 혼잡한 망(`jb 0` 이면 `v_jb` 목표가 6 이상으로 올라가는 망)에서 `VPING` 왕복 가운데 값 **≤ 380ms**(한쪽 약 0.215초 이하), 조용한 망에서 **≤ 301ms**(밤의 값보다 나빠지지 않음). 못 미치면 값을 적고 `jb` 를 한 칸 내려 다시 잰다 — 끊김(`v_urun`)이 분당 15 를 넘으면 그 값은 쓰지 않는다.

## CPU·메모리 예산

**CPU** (한 코어 기준. 부하 = `v_aec_us` 평균 ÷ 16000 + (`v_enc_us` 평균 + `v_dec_us` 평균) ÷ 20000 — 3b·Phase 0 과 같은 식)

| 일 | 실측(조건) | 몫 | 3c 가 더하는 것 |
|---|---|---|---|
| 에코 제거 한 묶음 | 평균 2906~3117µs / 최대 4552µs(`p3b_*`·`p3d_*`·`p3e_*`) | 18.2~19.5% | 없음 |
| Opus 압축 한 프레임(복잡도 0·24kbps) | 조용한 방 2733µs / **말소리 3471~4227µs**(최대 7740µs) | 13.7 / 17.4~21.1% | `opus` 를 올리면 늘 수 있다 — **재지 않았다**(Task 7 M1 에서 `opus 40` 의 `v_enc_us` 를 적는다). 복잡도 5 는 Phase 0 에서 평균 6863~7416µs(`tab5_phone_probe/out/task5.log:58-60`, 무음에 가까운 입력) → 말소리면 한 프레임에 10ms 안팎 = 이것만 50% — 손잡이로 두지 않았다 |
| Opus 해제 한 프레임 | 408~450µs | 2.0~2.3% | 없음 |
| 평탄화·셈(`tx_dyn`, 마이크 넘침, 되먹임 세기) | — | 어림 0.2~0.4%(묶음마다 곱셈·비교 1천여 번 = 수십 µs) | **`v_aec_us`·`v_enc_us` 에 잡히지 않는다** — `aud_late`·`v_under` 와 `aud_us` 로 본다 |
| **합** | 조용한 방 33.7~34.2% / **말소리 37.7~42.7%** | | 합격 기준 **≤ 50%**(그대로), `aud_late`=0, `v_under`=0 |
| 1덩어리 뒤 실측(`771b9fe`, `lvl 0`, 조용한 방 — 기기 보고) | 시험음 통화 35.9~36.0%, 30초 통화 38.3~38.4%, `aud_late`=0·`v_under`=0 | | 평탄화가 켜진 말소리 통화의 부하는 듣기 로그에서 본다 |

**내부 메모리** (기준 `heap_int_min` ≥ 100000, 멈춤선 60000)

| 항목 | 값 | 근거 |
|---|---|---|
| 부팅 뒤 대기(`5935ddd`) | `heap_int` 약 115,600 | `p3d_boot_unit2.log`·`p3d_call30.log` 의 통화 전 STAT |
| 통화·몰림 뒤 최저(`5935ddd`, 아침) | 1번 90,395 · 2번 **80,463** — 기준 아래 | `p3d_ab_aec_mic.log`. 대기 값에서 35KB 내려갔다 = Wi-Fi 전송 계층의 버퍼 풀(1,668바이트 단위)이 **한꺼번에 몰린 최대 개수(약 21개)**까지 자라고 돌려주지 않는다(`managed_components/espressif__esp_hosted/host/drivers/mempool/mempool.c:87-160`, 대기열 송신 20 + 수신 20 — `sdkconfig` 의 `CONFIG_ESP_HOSTED_SDIO_TX_Q_SIZE`·`RX_Q_SIZE`). 20분 동안 대기 중 `heap_int` 는 88,511~91,631 에 머물렀다(`p3e_tworoom_unit2.log`) |
| 되찾는 것 ① 캡처 태스크 스택 48 → 32KB | **+16,384** | `stk_vcap`(최저 여유) 실측: 만든 직후 46,380~46,412(초기화 2.7KB) → 에코 제거만 42,964(6.2KB) → 시험음 통화 27,716(21.4KB) → 마이크 통화 **25,204~25,236(23.9KB)** — 밤 3분 통화·아침 말소리 통화·통화 20여 번 동안 그대로(`p3b_boot_unit1.log`·`p3b_tone_try1.log`·`p3d_ab_aec_mic.log`). 32KB 면 여유 약 8.8KB |
| 되찾는 것 ② 송신 태스크 스택 8KB 를 PSRAM 으로 | **+8,192** | 해제 태스크(`v_rx`, 48KB)가 3b 내내 PSRAM 스택으로 돌았다. `stk_vtx` 최저 여유 7,176(쓴 것 1KB) |
| 3c 가 더 쓰는 정적 메모리 | −216 | 사본 빌드의 `idf.py size`: DIRAM 149,788 → 150,004 |
| **3c 뒤 부팅 대기(예상)** | **약 139,900** | 합 |
| 3c 뒤 부팅 대기(**실측**, `771b9fe`, 09:40) | 1번 **140,171** · 2번 **139,519**(`heap_int_min` 136,923 · 136,271) | `out/p3q_boot_unit1.log`·`p3q_boot_unit2.log` — 기기 보고 |
| **3c 뒤 최저(예상, 아침과 같은 몰림)** | **약 104,800** — 기준 위 | 139,900 − 35,100 |
| 3c 뒤 통화 뒤(**실측**, 시험음·지연 재기·30초 통화·평탄화 확인 뒤, 09:44) | `heap_int` 1번 122,723 · 2번 128,743, `heap_int_min` **116,527 · 121,751** — 기준 위 | `out/p3q_final.log`. 부팅 값에서 11~18KB 내려가 머문다 = 위의 버퍼 풀이 자란 만큼(돌려주지 않는다). 아침만큼 몰린 망은 아니었다(지연 가운데 값 430~445ms) |
| 3c 뒤 스택(**실측**) | `stk_vcap` 부팅 뒤 26,612 · 26,564(`selftest` 가 에코 제거를 돌린 뒤 — `5935ddd` 의 같은 시점 42,964 − 16,384 = 26,580), 통화 중 최저 **8,820**(예상 8.8KB). `stk_vtx` 7,888 → 7,168 · 7,152 | 같은 로그. 말소리 + 평탄화가 켜진 통화의 최저값은 듣기 로그에서 본다 |
| 3c 뒤 나쁜 망의 통화(**실측**, 듣기 1회차 09:50, 2번 — 지터 버퍼 목표가 상한 10, 틈 1.5초) | `heap_int_min` 121,751 → **106,859** — 기준 위(여유 6.9KB), `stk_vcap` 최저 8,820 그대로(큰 말소리 + `lvl 12`) | `out/p3q_listen_ab.log`. 위의 예상(약 104,800)과 맞는다 = 되찾은 24KB 가 없었다면 82,000 안팎이었을 통화다 |
| 이론상 가장 큰 몰림(풀 40개 = 66.7KB) | 약 73,200 — 기준 아래, 멈춤선 위 | 아직 본 적 없다. 풀의 실제 상한은 기기에서 재야 안다(Phase 4 전 과제로 넘김) |

- 30KB 를 다 되찾지는 못했다(24.4KB). 나머지 길은 값이 따른다 — 캡처 스택까지 PSRAM 으로(+32KB, 대신 에코 제거·압축이 느려질 수 있다: 해제는 PSRAM 스택에서 306 → 410µs 로 34% 느려졌고, 같은 비율이면 말소리 부하가 50% 를 넘는다), 버퍼 풀의 대기열 줄이기(`sdkconfig` 변경 — 몰릴 때 패킷을 버리게 된다). 둘 다 넣지 않았다(PL3c-D2). Phase 4 의 TLS(내부 25~35KB)와 함께 다시 정한다.
- 각 스택의 합격 기준: `stk_vcap` ≥ 6000(말소리 통화 뒤), 나머지 `stk_*` ≥ 1024.

## 이 계획에서 내린 결정 (PL3c-D)

통화 설계서의 결정(PH-D1~D9), 3b 의 결정(PL3b-D1~D27)은 여기서 바꾼 것 말고는 그대로 따른다. 바꾼 것: PL3b-D10(고정 4배 → 평탄화의 바닥 이득), PL3b-D15(캡처 스택 48KB·송신 스택 내부 → D1·D2), PL3b-D5(지터 버퍼 규칙 → 손잡이로 고를 수 있게, D5).

| ID | 결정 | 이유 | 비용 | 탈출구 |
|---|---|---|---|---|
| PL3c-D1 | **캡처 태스크(`v_cap`) 스택 48 → 32KB**(내부 메모리 그대로) | 실측으로 쓰는 것이 23.9KB 다(위 표). 가장 깊이 쓰는 것은 Opus 압축. 내부 메모리 16KB 를 되찾는다(3b 최종 검수 M-11) | 여유가 24KB → 8.8KB. Opus 가 다른 방식으로 넘어가면(64kbps 근처) 더 쓸 수 있다 → 비트율 손잡이는 40kbps 까지. 넘치면 통화 중 패닉(하드웨어 스택 가드 `CONFIG_ESP_SYSTEM_HW_STACK_GUARD=y`) | `voice.cpp` 의 `CAP_STACK` 을 48 로 |
| PL3c-D2 | **송신 태스크(`v_tx`) 스택 8KB 를 PSRAM 으로**(크기는 그대로). 캡처 스택은 PSRAM 으로 보내지 않고, 버퍼 풀의 대기열도 건드리지 않는다 | 20ms 에 한 번 60여 바이트를 소켓에 쓰는 태스크라 스택이 느려져도 박자에 영향이 없다. 크기를 줄이지 않는 것은 끊김 핸들러와 Phase 4 의 TLS 쓰기가 이 스택에서 돌기 때문 | PSRAM 스택 태스크가 하나 더. 끊김 핸들러가 이 스택에서 도는 길은 기기에서 돈 적이 없다(Task 11 Step 3) | `TX_STACK_CAPS` 를 `MALLOC_CAP_INTERNAL \| MALLOC_CAP_8BIT` 로 |
| PL3c-D3 | **손잡이 = 저장하지 않는 시리얼 명령, 부팅 값은 `voice_tune.h` 한 곳, 새 값은 새 줄 `TUNE`**. `STAT` 은 건드리지 않는다. 굳히기는 `DEFAULT_*` 아홉 줄의 숫자만 고친다(`D/verify/freeze_tune.py`) | 원인 후보마다 다시 올리지 않고 바꿔 듣는다(판정 P3B-R17). `STAT` 에 칸을 더하면 기존 도구 시험의 줄 끝 단언 셋을 고쳐야 한다 — 새 줄이면 기존 시험을 한 글자도 안 고친다. 굳힌 값이 범위 밖이면 헤더의 `static_assert` 가 빌드를 막는다 | 진단 명령이 9개 늘었다(Phase 4 에서 묶는다). 상태를 보려면 `stat` 과 `tune` 둘을 보내야 한다 | `DEFAULT_*` 를 `LEGACY_*` 로. 명령은 `phone_main.cpp` 의 한 블록 |
| PL3c-D4 | **사람 없이 재는 수단**: 시험음의 주파수(`vtone 300~7000`), 에코 제거를 거치는 시험음(`vsrc probe` — 마이크 자리에 크기 500 의 시험음), 받은 프레임 사이의 가장 긴 틈(`rxgap`), 마이크가 꼭대기에 닿은 표본 수(`mclip`), Wi-Fi 절전 상태(`ps`) | "먹먹함"이 압축·에코 제거·스피커 가운데 어디서 생기는지를 주파수별 숫자로 가른다. 망이 멈춘 길이와 마이크 넘침은 지금 로그로는 안 보인다 | 고른 시험음은 사람 목소리가 아니다 — 에코 제거가 잡음으로 보고 지울 수 있다(그러면 그 사실이 답이다) | 명령과 `voice_core.h` 의 `PROBE`·`ArrivalGap`·`clipped` 를 지운다 |
| PL3c-D5 | **지터 버퍼의 규칙을 `Policy` 로**: 기본은 3b 그대로(`jb 0`), `jb 3~10` 은 새 규칙 + 목표 상한. 새 규칙: 5초 무사하면 목표 −1(3b 는 10초), 깊어지면 0.5초 뒤부터 0.1초마다 한 프레임씩 버림(3b 는 1초 뒤 한꺼번에), 다시 튼 직후 0.5초 안에 몰려온 꼬리는 바로 버림, **끊김이 1초 안에 세 번 거듭되면 그동안은 3b 규칙** | 아침의 망에서 목표가 상한까지 올라가 머물렀다(지연의 절반 이상). 시뮬레이션: 평균 지연 263 → 134~151ms. "세 번 거듭" 예외가 없으면 0.3초마다 뭉쳐 오는 망(절전이 다시 켜진 경우)에서 소리의 2/3 를 버린다(예외로 3b 와 같은 88%) | 상한보다 긴 멈춤마다 끊긴다(분당 6.8 → 11.8, 시뮬레이션). 통화 시작 직후의 끊김 두 번까지는 상한을 지키지만 세 번이면 넘는다 | `jb 0`. 상수는 `jitter_buffer.h` 의 `TUNED_*` |
| PL3c-D6 | **`TCP_NODELAY` 는 손잡이(`nodelay 0\|1`)로, 접속 경로를 건드리지 않고 건다**: 붙은 뒤 lwIP 의 소켓 번호 범위를 훑어 "연결된 TCP 소켓"에 `setsockopt`. 다시 붙을 때마다 다시 건다 | 부품에는 설정 칸이 없다 — `esp_websocket_client.h:154-162` 의 설정 구조체에 없고, `tcp_transport`·`esp-tls` 어디에도 `TCP_NODELAY` 가 없다(검색 0건). 프레임마다 머리와 몸통을 두 번에 쓴다(`tcp_transport/transport_ws.c:420·428`). 공식 길(`ext_transport` + `esp_transport_get_socket`, `transport.c:311`)은 접속 경로(주소·토큰 헤더·제어 프레임 설정)를 우리가 다시 짜야 하고 틀리면 교환기에 못 붙는다(기기에서만 드러난다). 교환기 → 기기 방향은 이미 꺼져 있다(파이썬 asyncio 의 기본) | 소켓을 "찾아서" 건다(이 펌웨어의 TCP 연결은 교환기 하나뿐). 작은 조각이 그대로 나가 패킷 수가 는다. **효과의 크기는 재 봐야 안다**(Task 7 M3) | `nodelay 0`(부팅 값). 효과가 없으면 `relay_link.cpp` 의 RL-D12 블록째 지운다. 엄밀하게는 `ext_transport` 로 |
| PL3c-D7 | **보내는 크기는 `tx_dyn.h` 가 다듬는다**: 평탄화(`lvl` — 말소리 꼭대기를 16000 으로, 1배 ~ 상한, 천천히 오르고 빨리 내림, 20000 위는 부드럽게 눌러 32767 을 넘지 않음), 고음 올리기(`tilt`), 남은 메아리 누르기(`res`). 셋 다 0 이면 3b 의 고정 이득과 표본까지 같다. **상대 소리가 나는 동안(되먹임 칸 RMS ≥ 150, 그친 뒤 0.2초 포함)에는 이득이 고정 이득(4배)을 넘지 않는다 — 상대 소리가 시작되는 첫 묶음(16ms)만은 배운 이득에서 4배로 내려오는 중이다**(표본마다 내려가고 오르지 않는다 — 1덩어리 검수 Minor 1, Task 6b 의 시험이 고정) | 고정 이득을 올리면 큰 소리가 찢어지고 메아리·잡음이 같이 커진다. 고리(스피커 → 마이크)는 상대 소리가 날 때만 닫히므로, 그동안의 이득을 3b 와 같게 두면 **고리 이득·하울링 여유가 3b 와 같다**(한쪽: 스피커 → 마이크 → 4배 가 +10.7~+19dB, 에코 제거 28~36dB 뒤 −12~−17dB. 아침 로그로 잰 "상대에게 돌려보내는 메아리"는 1번 −10.9dB·2번 −20.5dB → 두 기기를 한 바퀴 돌면 약 −31dB. 첫 16ms 에 한해 최대 +9.5dB(`lvl 16` 이면 +12dB)가 더해질 수 있으나 그래도 한 바퀴가 0dB 아래이고, 한 묶음으로는 고리가 자라지 못한다). 동시에 말하기는 3b 와 같은 크기로 나간다 | 판단이 한 묶음(16ms) 늦다. 고르게 이어지는 소리(선풍기·시험음)는 약 7초 동안 말소리로 친다(최대 3배 커진다). `res` 는 상대가 말하는 동안의 작은 말소리(남은 메아리 어림의 2.5배 아래)를 함께 누른다. 문턱(말소리 40·상대 소리 150)은 아침 로그의 숫자에서 정했다 — 사람 목소리로 돌려 본 적이 없다 | `lvl 0`·`tilt 0`·`res 0` |
| PL3c-D8 | **Opus 는 비트율만 손잡이로(`opus 12~40`kbps)**, `esp_opus_enc_set_bitrate` 로 압축과 압축 사이에(캡처 태스크에서) 걸고, 건 뒤에 출력 버퍼 크기를 다시 묻는다. 복잡도·VBR·FEC·DTX 는 그대로 | 부품에 실행 중 비트율 함수가 있다(`esp_opus_enc.h:171`, 잠금 없음 — `:158-160`). 역어셈블(`lib/esp32p4/libesp_audio_codec.a` 의 `esp_opus_enc.c.obj`): 범위는 6~510kbps, 바꾸면 요구 출력 버퍼가 "프레임 바이트 + 100"으로 다시 셈되고(24kbps = 160), `esp_opus_enc_process` 는 넘겨준 길이가 그보다 작으면 그 프레임을 압축하지 않는다 → 올린 뒤 다시 묻지 않으면 전 프레임이 실패한다. 복잡도는 열 때만 정해지고(`esp_opus_enc.h:90`) 5 는 부하 초과 | 40 을 넘기지 않는다(출력 버퍼 256, 캡처 스택 32KB). 압축기가 거절하면 앞 값으로 돌아간다 | `opus 24` |
| PL3c-D9 | **마이크 증폭은 내리는 쪽만 손잡이로(`mic 12~24`, 3dB 단위). 부팅 값 24 그대로** | Phase 0(2번 기기): 30dB·음량 100 에서 메아리 꼭대기가 넘쳤다(0.05%, 잔향 감소 35.9dB ↔ 24dB 는 0%·41.8dB — `tab5_phone_probe/out/task5.log:520-526`). 1번은 제 스피커를 2번보다 7~9dB 더 크게 줍는다(1kHz 시험음 1414 → `mic` 3280~3857 ↔ 2번 1211) → 24dB 에서도 큰 소리는 넘친다(`v_spk=5904` 일 때 `v_mic=19777` — `p3d_ab_aec_mic.log`). 올리면 고리 이득도 그만큼 오른다. 크기는 `lvl` 이 맡는다 | 1번의 넘침은 남아 있다 — `mclip` 으로 재고 `mic 18` 로 내린다(내린 만큼은 `lvl` 이 다시 올린다 — 상한 16). 통화 중에 바꾸면 딸깍 소리가 날 수 있다 | `mic 24` |
| PL3c-D10 | **수신 무음 감시(2덩어리)**: 통화 중 상대의 소리 프레임이 **5초** 동안 하나도 안 오면 **끊는다** — 끊기를 누른 것과 같은 길(`hangup` 을 보내고 `ended` 를 기다린다)이고 안내는 "연결이 끊어졌습니다". 첫 프레임이 오기 전에는 세지 않는다 | 상대가 사라지면 교환기가 알 때까지(실측 17초, 계약상 45초) 마이크가 열려 있다(3b 최종 검수 M-9). DTX 가 꺼져 있어 프레임은 20ms 마다 온다. 교환기는 통화 중인 어느 쪽의 `hangup` 에도 양쪽에 `ended` 를 보낸다(`server/phone_core.py:160-162, 204-214`) — 계약 §9.4 그대로. "마이크만 닫기"·"화면에만 알리기"는 통화가 반쯤 열린 채 남는다 | 받는 길만 5초 넘게 막혔다 풀리는 망에서는 산 통화를 끊는다. 소리 장치가 죽어 프레임을 한 번도 안 보내는 상대와의 통화는 이 감시가 못 끝낸다(3b 의 "소리 없이도 통화 흐름은 된다"를 지키려는 것) | `app.cpp` 에서 `VOICE_LOST` 를 넣는 두 줄을 지운다 |
| PL3c-D11 | **Wi-Fi 절전은 "읽어서 보여 주기"까지만**(`TUNE ps=`, 10초마다). 스스로 다시 끄지는 않는다 | 호스트 쪽에는 설정이 사라질 길이 없고(fix 3 검수), C6 가 혼자 리셋되면 `CONFIG_ESP_HOSTED_TRANSPORT_RESTART_ON_FAILURE=y` 로 호스트도 다시 시작해 부팅 때 다시 건다. 읽는 길은 C6 까지 간다(`esp_wifi_with_remote.c:115-117` → `esp_hosted_api.c:361-364` → `rpc_wrap.c:1540-1552`) | `ps` 가 0 이 아닌 것을 사람이 봐야 안다. 지터 버퍼의 "세 번 거듭" 예외(D5)가 그때의 소리를 3b 수준으로 지킨다 | `net_wifi.cpp` 의 N-D5 두 줄 |
| PL3c-D12 | **1덩어리의 부팅 값은 `lvl 12` 하나만 바꾼다**(Task 6 — 따로 커밋). 나머지 여덟은 `5935ddd` 그대로. 회귀는 같은 바이너리에 시리얼 `lvl 0` 을 줘서 본다 | 사용자의 첫째 불만이 크기이고, `txgain`·`vol` 은 이미 상한이라 지금은 키울 길이 없다(컨트롤러 전달 2026-10-05). 올리려면 두 기기를 맥북에 꽂아야 하므로 한 번 올려서 바로 커져야 한다. `lvl 0` 이면 표본까지 같으므로 회귀와 개선을 한 바이너리에서 가를 수 있다. 12배(+9.5dB)인 이유: 아침 말소리(1초 RMS 110~350)를 −26dBFS 근처로 올리는 데 5~15배가 든다 — 16 은 듣고 정한다 | 의뢰의 "손잡이 빌드의 기본 동작은 5935ddd 와 같다"에서 한 칸 벗어났다(따로 커밋·같은 바이너리 회귀로 뜻은 지켰다). 평탄화가 기기에서 처음 도는 채로 켜져 있다 | Task 6 의 커밋을 되돌리거나 `freeze_tune.py --set lvl=0` |
| PL3c-D13 | **에코 제거의 방식·세기는 바꾸지 않는다. 출력 DMA 는 3칸 그대로** | `aec_set_nlp_level` 은 FD 방식(5·6)에서만 듣고 VOIP 방식에서는 경고만 낸다(`libesp_audio_processor.a` 의 `esp_aec.c.obj` 역어셈블 + 문자열 "Only full-duplex AEC support NLP level setting"; 헤더 `esp_aec.h:24-31, 134`·`esp_aec_nlp.h:9-13`). `FD_HIGH_PERF` 는 Phase 0 에서 결과가 깨졌고, SR 방식은 묶음이 512 다. `aec_create` 는 상태를 이미 PSRAM 에 잡는다(같은 역어셈블: caps = 1028 = `MALLOC_CAP_SPIRAM\|8BIT`) — 메모리를 더 되찾을 것이 없다. DMA 는 지연 예산 ⑨ | 에코 제거가 고음을 깎는 것으로 재지면 `tilt` 로 보완하는 것 말고 길이 없다 | `FD_LOW_COST` 를 다시 본다(조사 보고 `audio-reference-diff.md` §3) |
| PL3c-D14 | **교환기 코드를 바꾸지 않는다. 진단 명령 묶기·부팅 패닉 고리 방어·교환기 쪽 프레임 간격 로그·버퍼 풀 최대치 측정은 Phase 4 첫 과제로 넘긴다** | 교환기는 통화 중의 이진 프레임을 그대로 건네고 `hangup` 에 답한다 — 3c 에 필요한 것이 이미 있다. 넘기는 것들은 기기를 떨어뜨려 두기 전(Phase 4)에 필요하고 지금의 불만(크기·또렷함)과 무관하다 | 손잡이 9개가 제품 펌웨어에 진단 명령으로 남는다(시리얼로만 닿는다) | — |
| PL3c-D15 | **두 덩어리**: 1덩어리(Task 1~8, 6b) = 메모리 + 손잡이·측정 수단 + 크기 굳히기 + 올리기·듣기. 2덩어리(Task 9~12) = 무음 감시·미룬 지적·끊김 시험·듣고 고른 값 굳히기·문서 — **맥미니로 옮길 때**(사용자 2026-10-05 09:18). 2덩어리의 코드(Task 9·10)는 초안이 검증돼 있다 | 사용자가 느끼는 것(크기)을 먼저. 2덩어리는 망이 나빠지는 단계(Phase 4) 전에만 들어가면 된다 | 1덩어리와 2덩어리 사이에 상대가 사라졌을 때의 17~45초(M-9)가 그대로 남는다 | 2덩어리를 바로 이어서 한다(패치는 1덩어리 위에 그대로 적용된다) |
| PL3c-D16 | **(열림 — Task 8 Step 4 뒤에 정한다) 1번 기기의 메아리 넘침**: 먼저 실행 중 손잡이로 시험한다(`1:mic 18`). 넘침이 원인으로 확인되면 부팅 값은 (가) 두 기기 공통 `mic 18` + `lvl 16`, 안 되면 (나) 기기 번호별 부팅 값 | 1번은 제 스피커를 2번보다 7~9dB 크게 줍는다(스피커를 켠 `selftest`: `mic` 3421 대 1305). 받는 목소리의 꼭대기가 16000 이 되면 1번 마이크에서 37000~43000 = 넘치고, 넘친 메아리는 에코 제거가 못 지운다. 6dB 내리면 20000 안팎 = 넘치지 않는다. 듣기 1회차의 끝에 1번 쪽에서 큰 소리가 왔다(원인 미확정). **같은 바이너리를 올리므로 `DEFAULT_MIC_DB` 는 두 기기에 함께 걸린다** | (가): 2번의 마이크도 6dB 내려간다 — 평탄화가 상한까지 메우지만(`lvl 16` 이면 +2.5dB 더) 작게 말하는 사람은 최대 3.5dB 손해. (나): 기기 하나의 편차를 펌웨어 표에 굽는다 — 기기를 바꾸면 틀린 값이 된다. 어느 쪽이든 1번 쪽 사람의 목소리가 작아질 수 있다 | `mic 24`(손잡이)·`freeze_tune.py --set mic=24`. 다른 길: 평탄화의 목표 꼭대기를 내린다(`TARGET_PEAK` 16000 → 8000 — 모두가 6dB 작아진다), 1번의 음량만 내린다(`vol` — 1번 쪽 사람이 작게 듣는다) |

---

과제 묶음(검수가 들어가는 자리): **1덩어리 = Task 1~6**(코드) 뒤에 검수 → Task 7·8(기기). Task 6b(시험만)는 2덩어리의 검수에 함께 넣는다. **2덩어리 = Task 9~10**(코드) 뒤에 검수 → Task 11(기기) → Task 12(문서).

맥 시험 한 벌을 돌리는 명령(아래에서 "맥 시험"이라 부른다 — `T` 에 더 돌릴 시험 이름을 넣는다):
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone/tools && mkdir -p /tmp/p3q && for t in wifi_store call_fsm relay_proto device_id ui_layout tones pcm jitter_buffer voice_core $T; do clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_$t.cpp -o /tmp/p3q/t_$t 2> /tmp/p3q/cc_$t.txt; echo "$t: $(/tmp/p3q/t_$t | tail -1) (경고 $(wc -l < /tmp/p3q/cc_$t.txt | tr -d ' ')줄)"; done
```
도구 pytest: `cd ~/tab5-toy-phone/firmware/tab5_phone/tools && PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider test_phone_ctl.py 2>&1 | tail -1`
기준선(적용 전, 사본에서 잰 값): `wifi_store: 16 passed` · `call_fsm: 105 ok` · `relay_proto: 61 ok` · `device_id: 9 ok` · `ui_layout: 91 ok` · `tones: 89 ok` · `pcm: 37 ok` · `jitter_buffer: 65 ok` · `voice_core: 138 ok`(전부 `0 failed`, 경고 0줄), 도구 pytest `223 passed`, 교환기 pytest `137 passed`, 빌드 `0x288450`(사본 — 자리 값 `secrets.h`. 저장소의 `5935ddd` 빌드는 2,656,160 B = `0x2887a0` 이라 저장소가 사본보다 `0x350` 쯤 크다 → 아래의 크기는 "사본 값 + 약 0x350"으로 읽는다. `(58%) free` 는 같아야 한다).
아래 `P=~/tab5-toy-phone/.superpowers/sdd/2026-10-05-tab5-phone-phase3c-quality/draft/patches`.

### Task 1: 내부 메모리 24KB 되찾기 — 캡처 스택 48 → 32KB, 송신 스택은 PSRAM

> **실행됨(2026-10-05, 구현 에이전트 — 보고 `chunk1-impl-report.md`)**: 커밋 `199f00e`. 실패 → 통과의 문구가 아래 Expected 와 같았다.

**Files:**
- Modify: `firmware/tab5_phone/main/voice.cpp`(상수 `CAP_STACK`, 새 상수 `TX_STACK_CAPS`, `v_tx` 를 만드는 줄, 머리말 주석)
- Test: `firmware/tab5_phone/tools/test_phone_ctl.py`(끝에 시험 하나 추가)

**Interfaces:**
- Consumes: 없음(`5935ddd` 의 `voice.cpp`).
- Produces: 동작은 그대로. 기기에서 `stk_vcap` 의 부팅 값이 약 30,000(= 46,412 − 16,384), `heap_int` 가 약 24KB 늘어난다(Task 7 R3 이 본다).

- [x] **Step 1: 시험을 먼저 넣는다**(소스의 모양을 고정 — 동작은 기기 과제가 본다)

```bash
cd ~/tab5-toy-phone && git apply $P/T1-01-stack-shape-test.patch
```
```diff
--- a/firmware/tab5_phone/tools/test_phone_ctl.py
+++ b/firmware/tab5_phone/tools/test_phone_ctl.py
@@ -1088,3 +1088,15 @@
                           "1:vsrc ref", r"1:wait:^ERR vsrc speaker-on$:3"], pair=True)
     assert pc.run_steps(hub, plan), "\n".join(out)
     assert links["1"].cmds == ["txgain 2", "txgain 9", "vol 60", "vsrc ref"]
+
+
+# ── Phase 3c (계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md) ─────────────────────────────────────────────────────────────────
+# 기기에서만 드러나는 것 가운데 "소스가 이 모양이다"로 고정할 수 있는 것을 고정한다(동작은 기기 과제가 본다).
+def test_phase3c_the_capture_stack_is_32k_internal_and_the_sender_stack_is_in_psram():
+    """PL3c-D1·D2: 내부 메모리 24KB 를 되찾는다 — 캡처 태스크 48 → 32KB(내부 그대로), 송신 태스크 8KB 는 PSRAM 으로. 해제 태스크는 3b 그대로 PSRAM."""
+    src = VOICE_CPP.read_text(encoding="utf-8")
+    assert re.search(r"\bCAP_STACK = 32 \* 1024;", src) and not re.search(r"\bCAP_STACK = 48 \* 1024;", src)
+    assert 'xTaskCreatePinnedToCore(cap_task, "v_cap", CAP_STACK, nullptr, CAP_PRIO, &s_cap_task, CAP_CORE)' in src          # 무거운 쪽은 내부 메모리 스택 그대로
+    assert re.search(r"\bTX_STACK = 8 \* 1024;", src) and re.search(r"\bTX_STACK_CAPS = MALLOC_CAP_SPIRAM \| MALLOC_CAP_8BIT;", src)
+    assert 'xTaskCreatePinnedToCoreWithCaps(tx_task, "v_tx", TX_STACK, nullptr, TX_PRIO, &s_tx_task, TX_CORE, TX_STACK_CAPS)' in src
+    assert 'xTaskCreatePinnedToCoreWithCaps(rx_task, "v_rx", RX_STACK, nullptr, RX_PRIO, &s_rx_task, RX_CORE, RX_STACK_CAPS)' in src
```

- [x] **Step 2: 실패를 확인한다**

Run: 도구 pytest
Expected: `1 failed, 223 passed` — 실패한 것은 `test_phase3c_the_capture_stack_is_32k_internal_and_the_sender_stack_is_in_psram`.

- [x] **Step 3: 구현**

```bash
cd ~/tab5-toy-phone && git apply $P/T1-02-voice-stacks.patch
```
```diff
--- a/firmware/tab5_phone/main/voice.cpp
+++ b/firmware/tab5_phone/main/voice.cpp
@@ -10,7 +10,7 @@
 //   AEC_MODE_FD_HIGH_PERF 는 결과가 깨져 쓰지 않는다(Phase 0).
 //   시험 펌웨어와 다른 점(전부 계획서 "확인하지 못한 것"에 있다):
 //   PL3b-D15 태스크를 셋으로 나눴다(시험 펌웨어는 한 태스크가 읽기·에코 제거·압축·해제·쓰기를 다 했다):
-//          v_cap(코어 1, 우선순위 7, 스택 48KB 내부 메모리) 마이크 → 에코 제거 → 압축 / v_rx(코어 1, 7, 48KB PSRAM) 해제 / v_tx(코어 0, 5, 8KB 내부) 교환기로 보내기.
+//          v_cap(코어 1, 우선순위 7, 스택 32KB 내부 메모리 — 3b 는 48KB, PL3c-D1) 마이크 → 에코 제거 → 압축 / v_rx(코어 1, 7, 48KB PSRAM) 해제 / v_tx(코어 0, 5, 8KB PSRAM — PL3c-D2) 교환기로 보내기.
 //          이유: 재생은 3a 의 출력 태스크가 끌어가는 구조이고, 보내기는 Wi-Fi 가 막히면 2초까지 멈춘다(그동안 마이크·재생이 멎으면 안 된다).
 //          스택 48KB 는 시험 펌웨어가 세 가지(에코 제거·압축·해제)를 차례로 부르던 한 태스크의 값이다 — 둘로 나눈 각각에 그대로 준다.
 //          무거운 쪽(v_cap — 묶음마다 약 5ms)은 시험 펌웨어처럼 내부 메모리 스택이다(Phase 0 의 부하 30~37% 가 그 조건에서 잰 값).
@@ -59,10 +59,22 @@
 constexpr aec_mode_t AEC_MODE = AEC_MODE_VOIP_HIGH_PERF;        // Phase 0: 잔향 감소 29~42dB, 묶음 256표본. FD_HIGH_PERF 는 쓰지 않는다
 constexpr int OPUS_BITRATE = 24000;                             // PH-D5
 constexpr int OPUS_COMPLEXITY = 0;                              // Phase 0: 5 는 부하 53.6% 로 초과, 0 은 30~37%
-constexpr int CAP_CORE = 1, CAP_PRIO = 7, CAP_STACK = 48 * 1024;   // PL3b-D15
+// PL3c-D1 캡처 태스크의 스택 48 → 32KB(내부 메모리 16KB 를 되찾는다 — 3b 최종 검수 M-11).
+//   근거(기기 실측, STAT stk_vcap = 최저 여유): 만든 직후 46,380(초기화는 2.7KB 만 쓴다) → 에코 제거만 돈 뒤(selftest) 42,964(6.2KB) →
+//   시험음 통화 27,716(21.4KB) → 마이크 통화 25,204~25,236(23.9KB — 밤의 3분 통화, 아침의 말소리 통화, 통화 20여 번 동안 그대로).
+//   가장 깊이 쓰는 것은 Opus 압축(복잡도 0 · 24kbps)이다. 32KB 면 여유 약 8.8KB(27%).
+//   비용: 여유가 24KB → 8.8KB 로 준다. 복잡도를 올리거나 Opus 가 다른 방식으로 넘어가면(64kbps 근처) 스택을 더 쓸 수 있다 — 비트율 손잡이는 40kbps 까지만(PL3c-D8).
+//         넘치면 하드웨어 스택 가드가 패닉을 낸다(CONFIG_ESP_SYSTEM_HW_STACK_GUARD=y) — 통화 중에만 도는 태스크라 부팅 고리는 아니다.
+//   합격 기준(기기 과제): 말소리 통화 뒤 stk_vcap >= 6000. 되돌리기: 이 숫자를 48 로.
+constexpr int CAP_CORE = 1, CAP_PRIO = 7, CAP_STACK = 32 * 1024;   // PL3b-D15, PL3c-D1
 constexpr int RX_CORE = 1, RX_PRIO = 7, RX_STACK = 48 * 1024;
 constexpr UBaseType_t RX_STACK_CAPS = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;   // 해제 태스크의 스택은 PSRAM(내부 메모리 48KB 를 아낀다)
-constexpr int TX_CORE = 0, TX_PRIO = 5, TX_STACK = 8 * 1024;       // 보내기 실패 때 교환기 연결의 끊김 핸들러가 이 스택에서 돈다(relay 태스크와 같은 크기)
+// PL3c-D2 송신 태스크의 스택(8KB)도 PSRAM 에 둔다(내부 메모리 8KB 를 되찾는다). 크기는 그대로 — 보내기 실패 때 교환기 연결의 끊김 핸들러가
+//   이 스택에서 돌고(relay 태스크와 같은 크기), Phase 4 에서는 TLS 쓰기도 여기서 돈다. 해제 태스크(v_rx)가 같은 방식(PSRAM 스택)으로 3b 내내 돌았다.
+//   이 태스크는 20ms 에 한 번 소켓에 60여 바이트를 쓰는 것이 전부라 스택이 느려져도 박자에 영향이 없다(무거운 캡처 태스크는 내부 메모리 그대로).
+//   비용: PSRAM 스택의 태스크가 하나 더 는다. 되돌리기: TX_STACK_CAPS 를 MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT 로.
+constexpr int TX_CORE = 0, TX_PRIO = 5, TX_STACK = 8 * 1024;
+constexpr UBaseType_t TX_STACK_CAPS = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
 constexpr uint32_t PING_TIMEOUT_MS = 3000;
 constexpr uint32_t SELFTEST_HOLD_MS = 720;                         // 시험음(0.64초)이 스스로 줄어 끝난 뒤에 점검을 닫는다(중간에 끊으면 딸깍 소리)
 
@@ -298,7 +310,7 @@
     if (s_codec_fail) { s_cap_task = nullptr; return s_codec_fail; }
 
     if (xTaskCreatePinnedToCoreWithCaps(rx_task, "v_rx", RX_STACK, nullptr, RX_PRIO, &s_rx_task, RX_CORE, RX_STACK_CAPS) != pdPASS) return "task-rx";
-    if (xTaskCreatePinnedToCore(tx_task, "v_tx", TX_STACK, nullptr, TX_PRIO, &s_tx_task, TX_CORE) != pdPASS) return "task-tx";
+    if (xTaskCreatePinnedToCoreWithCaps(tx_task, "v_tx", TX_STACK, nullptr, TX_PRIO, &s_tx_task, TX_CORE, TX_STACK_CAPS) != pdPASS) return "task-tx";
     if (!audioout::add_source(fill)) return "source";
     relaylink::set_voice_sink(on_frame);
     return nullptr;
```

- [x] **Step 4: 통과를 확인한다**

Run: 도구 pytest → `224 passed`. 맥 시험(`T=""`) → 기준선과 같다.
(사본 빌드: `0x288450`, 우리 코드 경고 0.)

- [x] **Step 5: 커밋**

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/tools/test_phone_ctl.py firmware/tab5_phone/main/voice.cpp && git commit -m "perf(phone): 내부 메모리 24KB 되찾기 — 캡처 태스크 스택 48→32KB, 송신 태스크 스택은 PSRAM (PL3c-D1·D2, 3b 최종 검수 M-11)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 2: 손잡이의 값·범위 `voice_tune.h` 와 지터 버퍼 규칙 `Policy` (시험 먼저)

> **실행됨(2026-10-05, 구현 에이전트 — 보고 `chunk1-impl-report.md`)**: 커밋 `91adf23`. 실패 → 통과의 문구가 아래 Expected 와 같았다.

**Files:**
- Create: `firmware/tab5_phone/main/voice_tune.h`, `firmware/tab5_phone/tools/test_voice_tune.cpp`
- Modify: `firmware/tab5_phone/main/jitter_buffer.h`
- Test: `firmware/tab5_phone/tools/test_jitter_buffer.cpp`(끝에 추가)

**Interfaces:**
- Consumes: 없음.
- Produces(`voice_tune.h`, 이름공간 `voicetune`): `LEGACY_*`·`DEFAULT_*`(각 9개: `TXGAIN VOL MIC_DB KBPS LVL TILT RES_DB JB NODELAY`), 범위 상수(`MIC_DB_MIN/MAX/STEP`·`KBPS_MIN/MAX`·`LVL_MIN/MAX`·`TILT_MAX`·`RES_DB_STEP/MAX`·`JB_MIN/MAX`·`VTONE_MIN/MAX/DEFAULT`), `bool ok_txgain|ok_vol|ok_mic_db|ok_kbps|ok_lvl|ok_tilt|ok_res_db|ok_jb|ok_nodelay|ok_vtone(int)`, `struct Knobs { int txgain, vol, mic_db, kbps, lvl, tilt, res_db, jb, nodelay; }`, `bool same(const Knobs&, const Knobs&)`, `Knobs legacy()`, `bool is_default(const Knobs&)`, `bool is_legacy(const Knobs&)`, `bool meas_valid(const Knobs&)`, `constexpr int opus_out_need(int kbps, int frame_samples = 320, int rate = 16000)`, `OPUS_OUT_CAP = 256`.
- Produces(`jitter_buffer.h`, 이름공간 `jitterbuf`): `struct Policy { int max_frames, calm_pops, catchup_pops, catchup_step, resume_pops, burst_pops; }`, `Policy legacy_policy()`, `Policy tuned_policy(int cap)`, `Buffer::set_policy(const Policy&)`, `const Policy &Buffer::policy() const`. 기본 동작(규칙을 걸지 않은 `Buffer`)은 3b 와 같다.

- [x] **Step 1: 시험을 먼저 넣는다**

```bash
cd ~/tab5-toy-phone && git apply $P/T2-01-tune-jitter-tests.patch
```
```diff
--- /dev/null
+++ b/firmware/tab5_phone/tools/test_voice_tune.cpp
@@ -0,0 +1,57 @@
+// test_voice_tune.cpp — voice_tune.h 단위 시험:
+//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_voice_tune.cpp -o /tmp/t_voice_tune && /tmp/t_voice_tune
+// 계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D3·D8, Review Focus 4(손잡이가 기본 동작을 바꿔 놓음):
+//   손잡이의 범위(소리가 커지는 쪽은 3b 의 값을 넘지 못한다), "부팅 값과 같은가"·"5935ddd 와 같은가"·"selftest·vping 판정이 유효한가", Opus 출력 버퍼 크기.
+// 이 시험은 DEFAULT_* 의 값 자체를 고정하지 않는다(마지막 과제가 고친다) — 그 값이 LEGACY_* 와 같은지는 draft/verify/tune_check.py 와 기기의 TUNE 줄(legacy=1)이 본다.
+#include "voice_tune.h"
+#include <cstdio>
+using namespace voicetune;
+static int ok = 0, bad = 0;
+#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)
+
+int main()
+{
+    {
+        CHECK("legacy = what 5935ddd did: x4, volume 100, mic 24 dB, 24 kbps, leveller off, no treble lift, echo press off, old jitter rule, Nagle on",
+              LEGACY_TXGAIN == 4 && LEGACY_VOL == 100 && LEGACY_MIC_DB == 24 && LEGACY_KBPS == 24 && LEGACY_LVL == 0 && LEGACY_TILT == 0 && LEGACY_RES_DB == 0 && LEGACY_JB == 0 && LEGACY_NODELAY == 0);
+        const Knobs boot, old = legacy();
+        CHECK("a fresh Knobs holds the boot defaults; legacy() holds the 5935ddd values", is_default(boot) && is_legacy(old)
+              && boot.txgain == DEFAULT_TXGAIN && boot.vol == DEFAULT_VOL && boot.mic_db == DEFAULT_MIC_DB && boot.kbps == DEFAULT_KBPS
+              && boot.lvl == DEFAULT_LVL && boot.tilt == DEFAULT_TILT && boot.res_db == DEFAULT_RES_DB && boot.jb == DEFAULT_JB && boot.nodelay == DEFAULT_NODELAY);
+        bool each = true;
+        for (int f = 0; f < 9; ++f) {
+            Knobs k = legacy();
+            int *field[] = {&k.txgain, &k.vol, &k.mic_db, &k.kbps, &k.lvl, &k.tilt, &k.res_db, &k.jb, &k.nodelay};
+            *field[f] += 1;
+            if (is_legacy(k) || same(k, legacy())) each = false;
+        }
+        CHECK("changing any one of the nine knobs makes it 'not legacy' (none is forgotten in the comparison)", each && sizeof(Knobs) == 9 * sizeof(int));
+        Knobs k = legacy();
+        CHECK("selftest / vping verdicts are valid only at send gain x4 and volume 100 ...", meas_valid(k));
+        k.txgain = 2; const bool g = meas_valid(k); k.txgain = 4; k.vol = 90; const bool v = meas_valid(k);
+        CHECK("... not when either is turned down", !g && !v);
+        k = legacy(); k.mic_db = 18; k.lvl = 16; k.tilt = 2; k.res_db = 12; k.jb = 4; k.nodelay = 1; k.kbps = 32;
+        CHECK("... and the other knobs do not matter for them (diagnostic sources bypass the leveller)", meas_valid(k) && !is_legacy(k));
+    }
+    {
+        CHECK("txgain 1..4, volume 0..100 (as in 3b)", ok_txgain(1) && ok_txgain(4) && !ok_txgain(0) && !ok_txgain(5) && ok_vol(0) && ok_vol(100) && !ok_vol(101) && !ok_vol(-1));
+        CHECK("mic gain: 12..24 dB in 3 dB steps — never above the 3b value (louder would raise the loop gain)",
+              ok_mic_db(12) && ok_mic_db(15) && ok_mic_db(18) && ok_mic_db(21) && ok_mic_db(24) && !ok_mic_db(27) && !ok_mic_db(30) && !ok_mic_db(9) && !ok_mic_db(13) && !ok_mic_db(0) && MIC_DB_MAX == LEGACY_MIC_DB);
+        CHECK("bitrate: 12..40 kbps", ok_kbps(12) && ok_kbps(24) && ok_kbps(40) && !ok_kbps(11) && !ok_kbps(41) && !ok_kbps(0) && !ok_kbps(24000));
+        CHECK("leveller: 0 (off) or a ceiling of x6..x16", ok_lvl(0) && ok_lvl(6) && ok_lvl(16) && !ok_lvl(1) && !ok_lvl(5) && !ok_lvl(17) && !ok_lvl(-1));
+        CHECK("treble lift: 0 (off), 1 or 2", ok_tilt(0) && ok_tilt(1) && ok_tilt(2) && !ok_tilt(3) && !ok_tilt(-1));
+        CHECK("echo press: 0, 6, 12 or 18 dB", ok_res_db(0) && ok_res_db(6) && ok_res_db(12) && ok_res_db(18) && !ok_res_db(3) && !ok_res_db(24) && !ok_res_db(-6));
+        CHECK("jitter rule: 0 (the 3b rule) or a ceiling of 3..10 frames", ok_jb(0) && ok_jb(3) && ok_jb(10) && !ok_jb(1) && !ok_jb(2) && !ok_jb(11) && !ok_jb(-1));
+        CHECK("nodelay: 0 or 1; test tone: 300..7000 Hz (below half the sample rate)", ok_nodelay(0) && ok_nodelay(1) && !ok_nodelay(2) && ok_vtone(300) && ok_vtone(1000) && ok_vtone(7000) && !ok_vtone(299) && !ok_vtone(7001) && VTONE_DEFAULT == 1000 && VTONE_MAX < 8000);
+        bool defaults_ok = ok_txgain(DEFAULT_TXGAIN) && ok_vol(DEFAULT_VOL) && ok_mic_db(DEFAULT_MIC_DB) && ok_kbps(DEFAULT_KBPS) && ok_lvl(DEFAULT_LVL) && ok_tilt(DEFAULT_TILT) && ok_res_db(DEFAULT_RES_DB) && ok_jb(DEFAULT_JB) && ok_nodelay(DEFAULT_NODELAY);
+        CHECK("whatever the boot defaults are, each lies inside its knob's range", defaults_ok);
+    }
+    {
+        CHECK("encoder output buffer the part asks for: bytes of one 20 ms frame + 100 (160 at 24 kbps — the 'opus_out_max' the device printed)",
+              opus_out_need(24) == 160 && opus_out_need(12) == 130 && opus_out_need(32) == 180 && opus_out_need(40) == 200);
+        bool fits = true; for (int k = KBPS_MIN; k <= KBPS_MAX; ++k) if (opus_out_need(k) > OPUS_OUT_CAP || k * 1000 / 400 + 2 > 200) fits = false;
+        CHECK("every bitrate the knob allows fits the capture task's buffer (256) and the tx queue slot (200 with the 2-byte sequence)", fits && OPUS_OUT_CAP == 256);
+    }
+    printf("\n%d ok, %d failed\n", ok, bad);
+    return bad ? 1 : 0;
+}
--- a/firmware/tab5_phone/tools/test_jitter_buffer.cpp
+++ b/firmware/tab5_phone/tools/test_jitter_buffer.cpp
@@ -3,6 +3,7 @@
 // 설계서 §9.6: 60ms 로 시작해 40~200ms 사이에서 조절, 빈 프레임은 손실 은닉, 300ms 를 넘으면 오래된 것부터 버려 지연이 쌓이지 않게.
 // 뒤쪽의 "오래·거칠게" 시험은 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md Review Focus 3(손실·순서 뒤바뀜·몰림·오래 통화).
 #include "jitter_buffer.h"
+#include <algorithm>
 #include <cstdio>
 #include <cstring>
 #include <map>
@@ -348,7 +349,156 @@
             if (b.depth() < 0 || b.depth() > OVERFLOW_FRAMES || b.target_frames() < MIN_FRAMES || b.target_frames() > MAX_FRAMES) sane = false;
         }
         CHECK("5 million random pushes / pops / resets: never out of bounds, every played frame is one that was accepted", sane);
+    }
+
+
+    // ════ Phase 3c (JB-D3, 계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D5): 깊이를 정하는 규칙(Policy) ════
+    // 틀고 있는 상태로 만든다: 목표만큼 채우고 한 번 꺼낸다. 돌려주는 값 = 다음에 넣을 순번
+    auto prime = [](Buffer &b, uint16_t from) { uint16_t q = from; while (b.depth() < b.target_frames()) put(b, q++); b.pop(); return q; };
+    // 한 프레임 넣고 한 프레임 꺼내기를 n 번(끊김 없이 흐른다)
+    auto steady = [](Buffer &b, uint16_t &q, int n) { for (int i = 0; i < n; ++i) { put(b, q++); b.pop(); } };
+    // 다 떨어지게 한 뒤 다시 채워 튼다(끊김 한 번). 돌려주는 값 = 그때의 목표
+    auto underrun = [](Buffer &b, uint16_t &q) { while (b.pop().kind != Kind::SILENCE) {} while (b.buffering()) { put(b, q++); b.pop(); } return b.target_frames(); };
+    {
+        const Policy l = legacy_policy();
+        CHECK("policy: the default is the 3b rule (ceiling 200 ms, 10 s calm, 1 s then all at once, no resume trim, no burst rule)",
+              l.max_frames == MAX_FRAMES && l.calm_pops == CALM_POPS && l.catchup_pops == CATCHUP_POPS && l.catchup_step == 0 && l.resume_pops == 0 && l.burst_pops == 0);
+        Buffer b;
+        CHECK("policy: a new buffer starts with it", b.policy().max_frames == 10 && b.policy().calm_pops == 500 && b.policy().catchup_step == 0);
+        const Policy t = tuned_policy(4);
+        CHECK("policy: tuned(4) = ceiling 80 ms, 5 s calm, 0.5 s then one frame per 0.1 s, trim a burst within 0.5 s of resuming, burst rule 1 s",
+              t.max_frames == 4 && t.calm_pops == 250 && t.catchup_pops == 25 && t.catchup_step == 5 && t.resume_pops == 25 && t.burst_pops == 50);
+        CHECK("policy: tuned() clamps the ceiling to 3..10 frames (the call still starts at 60 ms)", tuned_policy(1).max_frames == 3 && tuned_policy(3).max_frames == 3 && tuned_policy(10).max_frames == 10 && tuned_policy(99).max_frames == 10 && tuned_policy(-5).max_frames == 3);
+    }
+    {
+        // 기본 규칙을 다시 걸어도 3b 와 글자 그대로 같다: 같은 무작위 입력 200만 번에 두 버퍼가 같은 것을 낸다
+        uint32_t seed = 4242; auto rnd = [&seed](uint32_t m) { seed = seed * 1664525u + 1013904223u; return (seed >> 8) % m; };
+        Buffer a, b; b.set_policy(legacy_policy()); bool same = true; uint16_t base = 0; uint8_t d[MAX_PAYLOAD];
+        for (int i = 0; i < 2000000 && same; ++i) {
+            if (rnd(100) < 52) {
+                const uint16_t q = (uint16_t)(rnd(200) == 0 ? rnd(65536) : base + rnd(30) - 3); const size_t len = 1 + rnd(100);
+                memset(d, (uint8_t)q, sizeof d); a.push(q, d, len); b.push(q, d, len);
+                if (rnd(6) == 0) ++base;
+            } else {
+                const Pop x = a.pop(), y = b.pop();
+                if (x.kind != y.kind || x.seq != y.seq || x.len != y.len) same = false;
+                if (x.kind == Kind::PLAY) base = (uint16_t)(x.seq + 1);
+            }
+            if (a.depth() != b.depth() || a.target_frames() != b.target_frames() || a.buffering() != b.buffering()) same = false;
+        }
+        const Stats &x = a.stats(), &y = b.stats();
+        CHECK("policy: setting the legacy rule explicitly changes nothing — 2 million random pushes / pops give identical output and counters",
+              same && x.played == y.played && x.lost == y.lost && x.overflow_drop == y.overflow_drop && x.catchup_drop == y.catchup_drop && x.underruns == y.underruns && x.late == y.late && x.played > 100000);
+    }
+    {
+        // ① 상한: 끊김이 드문드문(2초보다 뜸하게) 오면 목표는 상한에서 멈춘다. 3b 는 200ms 까지 올라간다
+        Buffer t, l; t.set_policy(tuned_policy(4));
+        uint16_t qt = prime(t, 0), ql = prime(l, 0); int top_t = 0, top_l = 0;
+        for (int i = 0; i < 12; ++i) { steady(t, qt, 150); steady(l, ql, 150); top_t = underrun(t, qt); top_l = underrun(l, ql); }     // 3초마다 한 번 끊긴다
+        CHECK("ceiling: underruns 3 s apart push the 3b rule to its 200 ms ceiling, the tuned rule stops at its own (80 ms)", top_l == 10 && top_t == 4 && t.stats().underruns == 12);
+        // ② 5초 무사하면 한 프레임 내려온다(3b 는 10초)
+        steady(t, qt, 245); const int before = t.target_frames(); steady(t, qt, 10);
+        CHECK("calm: 5 s (250 frames) without an underrun brings the target down one frame; the 3b rule needs 10 s", before == 4 && t.target_frames() == 3);
+        steady(l, ql, 260);
+        CHECK("... (3b after the same 5 s: still at 200 ms)", l.target_frames() == 10);
+        steady(t, qt, 600);
+        CHECK("calm: it never goes below 40 ms", t.target_frames() == MIN_FRAMES);
+    }
+    {
+        // 끊김이 1초 안에 세 번 거듭된다(0.3초마다 뭉쳐 오는 망 — 절전이 다시 켜진 경우): 그동안은 3b 처럼 200ms 까지 올라간다
+        Buffer t; t.set_policy(tuned_policy(4)); uint16_t q = prime(t, 0); int top = 0;
+        steady(t, q, 10); underrun(t, q); steady(t, q, 10); const int second = underrun(t, q);      // 통화 시작에 흔한 것: 끊김 두 번이 붙어 온다
+        CHECK("burst rule: two underruns close together (common right after a call opens) do not pass the knob", second == 4);
+        for (int i = 0; i < 12; ++i) { steady(t, q, 10); top = underrun(t, q); }                    // 0.2초마다 계속 끊긴다
+        CHECK("burst rule: a third and further underruns within 1 s of each other may climb past the knob to the 3b ceiling (a low ceiling would throw most of the audio away)", top == 10);
+        steady(t, q, 60); const int hold = underrun(t, q);
+        CHECK("burst rule: once underruns are more than 1 s apart again, the knob's ceiling is back (no further rise)", hold == 10 && t.target_frames() == 10);
+        steady(t, q, 6 * 250 + 10);
+        CHECK("burst rule: ... and 5 s of calm at a time brings it back down under the knob", t.target_frames() <= 4);
+    }
+    {
+        // ③ 깊어졌을 때: 3b 는 1초 뒤 한꺼번에(큰 건너뜀 한 번), 새 규칙은 0.5초 지켜본 뒤 0.1초마다 한 프레임
+        Buffer t, l; t.set_policy(tuned_policy(4));
+        uint16_t qt = prime(t, 0), ql = prime(l, 0);
+        steady(t, qt, 30); steady(l, ql, 30);                                                        // 다시 튼 지 0.6초 — "다시 틀기 직후"가 아니다
+        for (int i = 0; i < 9; ++i) { put(t, qt++); put(l, ql++); }                                  // 9프레임이 몰려왔다(180ms 멈췄다 풀림 — 끊기지는 않았다)
+        const int deep_t = t.depth(), deep_l = l.depth();
+        std::vector<int> jumps_t, jumps_l; uint16_t last_t = 0, last_l = 0; bool have = false; int back_t = -1, back_l = -1;
+        for (int i = 0; i < 80; ++i) {
+            put(t, qt++); put(l, ql++);
+            const Pop a = t.pop(), b = l.pop();
+            if (have) { jumps_t.push_back((uint16_t)(a.seq - last_t)); jumps_l.push_back((uint16_t)(b.seq - last_l)); }
+            last_t = a.seq; last_l = b.seq; have = true;
+            if (back_t < 0 && t.depth() - t.target_frames() < 2) back_t = i;
+            if (back_l < 0 && l.depth() - l.target_frames() < 2) back_l = i;
+        }
+        const int big_t = *std::max_element(jumps_t.begin(), jumps_t.end()), big_l = *std::max_element(jumps_l.begin(), jumps_l.end());
+        CHECK("catch-up: both rules start 8 frames (160 ms) too deep", deep_t == deep_l && deep_t - 3 >= 8);
+        CHECK("catch-up: the 3b rule plays 1 s late, then skips it all in one jump (8 frames = 160 ms of speech gone at once)", big_l >= 8 && back_l >= 49 && l.stats().catchup_drop >= 7);
+        CHECK("catch-up: the tuned rule waits 0.5 s, then drops one frame every 0.1 s — never more than one frame (20 ms) at a time", big_t == 2 && t.stats().catchup_drop >= 6 && back_t >= 24);
+        CHECK("catch-up: ... and is back near its target within 1.3 s", back_t >= 0 && back_t <= 65);
     }
+    {
+        // 다시 틀기 시작한 직후 몰려온 꼬리: 새 규칙은 기다리지 않고 버린다(이미 그만큼 늦은 소리), 3b 는 1초 동안 늦게 튼다
+        Buffer t, l; t.set_policy(tuned_policy(4));
+        uint16_t qt = prime(t, 0), ql = prime(l, 0);
+        steady(t, qt, 200); steady(l, ql, 200);
+        while (t.pop().kind != Kind::SILENCE) {} while (l.pop().kind != Kind::SILENCE) {}           // 망이 멈췄다 — 둘 다 다 떨어져 무음
+        for (int i = 0; i < 4; ++i) { put(t, qt++); put(l, ql++); }                                  // 풀렸다: 먼저 네 프레임
+        t.pop(); l.pop();                                                                            // 다시 틀기 시작
+        for (int i = 0; i < 9; ++i) { put(t, qt++); put(l, ql++); }                                  // 꼬리 아홉 프레임이 곧바로 따라온다
+        const uint32_t ovf_t = t.stats().overflow_drop, ovf_l = l.stats().overflow_drop;
+        put(t, qt++); put(l, ql++); t.pop(); l.pop();
+        CHECK("resume: a burst that lands right after playback restarts is trimmed at once by the tuned rule (counted with the restart drops)",
+              t.depth() <= t.target_frames() && t.stats().overflow_drop > ovf_t && t.stats().catchup_drop == 0);
+        CHECK("resume: ... the 3b rule keeps playing it 180 ms late for a second", l.depth() - l.target_frames() >= 8 && l.stats().overflow_drop == ovf_l);
+    }
+    {
+        // 통화 중에 규칙을 바꾼다
+        Buffer b; uint16_t q = prime(b, 0);
+        for (int i = 0; i < 9; ++i) { steady(b, q, 150); underrun(b, q); }
+        const int high = b.target_frames();
+        b.set_policy(tuned_policy(5));
+        CHECK("set_policy during a call: a target above the new ceiling comes down to it at once", high == 10 && b.target_frames() == 5);
+        bool in_order = true; uint16_t last = 0; bool have = false;
+        for (int i = 0; i < 200; ++i) { put(b, q++); const Pop p = b.pop(); if (p.kind == Kind::PLAY) { if (have && (int16_t)(p.seq - last) <= 0) in_order = false; last = p.seq; have = true; } }
+        CHECK("set_policy during a call: playback carries on in order, the surplus is shed by the new rule", in_order && b.depth() - b.target_frames() < 2);
+        b.reset();
+        CHECK("set_policy: the rule survives reset() (a new call keeps the knob)", b.policy().max_frames == 5 && b.policy().catchup_step == 5 && b.target_frames() == START_FRAMES);
+        Policy odd = {1, 0, 0, 99, -3, -1};
+        b.set_policy(odd);
+        CHECK("set_policy: nonsense values are clamped (ceiling >= 40 ms, counts >= 1, step <= wait)", b.policy().max_frames == MIN_FRAMES && b.policy().calm_pops == 1 && b.policy().catchup_pops == 1 && b.policy().catchup_step == 1 && b.policy().resume_pops == 0 && b.policy().burst_pops == 0);
+        b.set_policy(legacy_policy());
+        CHECK("set_policy: and back to the 3b rule", b.policy().max_frames == MAX_FRAMES && b.policy().catchup_step == 0);
+    }
+    {
+        // 아무렇게나(새 규칙, 상한 3~10): 죽지 않고, 셈이 맞고, 꺼낸 프레임은 전부 받아들인 것
+        uint32_t seed = 31337; auto rnd = [&seed](uint32_t m) { seed = seed * 1664525u + 1013904223u; return (seed >> 8) % m; };
+        Buffer b; b.set_policy(tuned_policy(4)); std::map<uint16_t, uint8_t> content; bool sane = true; uint16_t base = 0; uint8_t d[MAX_PAYLOAD + 4];
+        for (int i = 0; i < 3000000 && sane; ++i) {
+            const uint32_t op = rnd(1000);
+            if (op < 540) {
+                const uint16_t q = (uint16_t)(rnd(50) == 0 ? rnd(65536) : base + rnd(40) - 4);
+                const size_t len = rnd(40) == 0 ? rnd(MAX_PAYLOAD + 4) : 1 + rnd(120);
+                const uint8_t fill = (uint8_t)rnd(256);
+                memset(d, fill, sizeof d);
+                const uint32_t before = b.stats().dup + b.stats().late + b.stats().bad;
+                b.push(q, d, len);
+                if (b.stats().dup + b.stats().late + b.stats().bad == before) content[q] = fill;
+                if (rnd(8) == 0) ++base;
+            } else if (op < 990) {
+                const Pop p = b.pop();
+                if (p.kind == Kind::PLAY) {
+                    auto it = content.find(p.seq);
+                    if (it == content.end() || p.len == 0 || p.len > MAX_PAYLOAD || p.data[0] != it->second || p.data[p.len - 1] != it->second) sane = false;
+                    base = (uint16_t)(p.seq + 1);
+                } else if (p.len != 0) sane = false;
+            } else if (op < 995) b.set_policy(rnd(4) == 0 ? legacy_policy() : tuned_policy(3 + (int)rnd(8)));
+            else if (rnd(50) == 0) { b.reset(); content.clear(); }
+            if (b.depth() < 0 || b.depth() > OVERFLOW_FRAMES || b.target_frames() < MIN_FRAMES || b.target_frames() > MAX_FRAMES) sane = false;
+        }
+        CHECK("3 million random pushes / pops / rule changes / resets: never out of bounds, every played frame is one that was accepted", sane);
+    }
 
     printf("\n%d ok, %d failed\n", ok, bad);
     return bad ? 1 : 0;
```

- [x] **Step 2: 실패를 확인한다**

Run: 맥 시험 가운데 둘만 — `cd ~/tab5-toy-phone/firmware/tab5_phone/tools && for t in voice_tune jitter_buffer; do clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_$t.cpp -o /tmp/p3q/t_$t 2>&1 | grep -m1 "error:"; done`
Expected: `test_voice_tune.cpp:6:10: fatal error: 'voice_tune.h' file not found`, `test_jitter_buffer.cpp:363:15: error: unknown type name 'Policy'`.

- [x] **Step 3: `voice_tune.h`**

```bash
cd ~/tab5-toy-phone && git apply $P/T2-02-voice-tune.patch
```
```diff
--- /dev/null
+++ b/firmware/tab5_phone/main/voice_tune.h
@@ -0,0 +1,94 @@
+// voice_tune.h — 통화 품질 손잡이: 기본값(부팅 값), 범위, 값 읽기. 하드웨어와 무관한 순수 코드(맥에서 단위 시험: tools/test_voice_tune.cpp).
+//
+// 계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md 의 결정 PL3c-D3.
+//   손잡이 = 실행 중에 시리얼 명령으로 바꿔 들어 보는 값(저장하지 않는다 — 재부팅하면 아래 기본값). 3b 의 txgain·vol 과 같은 방식이다.
+//   이유: "먹먹하다·작다·메아리·지연"의 원인 후보마다 다시 빌드·플래시하지 않고 그 자리에서 바꿔 듣고 재려는 것(판정 P3B-R17).
+//   기본값은 손잡이를 넣기 전(5935ddd)의 동작과 같게 시작한다(LEGACY_*) — 회귀와 개선을 가를 수 있게.
+//   굳히기 과제는 아래 DEFAULT_* 아홉 줄의 숫자만 고친다(draft/verify/freeze_tune.py 가 범위를 확인하며 고친다) — 굳힌 뒤에도 시리얼로 LEGACY_* 값을 주면 5935ddd 의 동작이 그대로 나온다.
+//   비용: 진단 명령이 여덟 개 늘었다(Phase 4 의 "진단 명령 묶기"에 함께 넣는다). 되돌리기: DEFAULT_* 를 LEGACY_* 로.
+#pragma once
+#include <cstdint>
+
+namespace voicetune {
+
+// ── 5935ddd 의 동작(손잡이를 넣기 전의 고정값) ─────────────────────────────────
+constexpr int LEGACY_TXGAIN = 4, LEGACY_VOL = 100, LEGACY_MIC_DB = 24, LEGACY_KBPS = 24, LEGACY_LVL = 0, LEGACY_TILT = 0, LEGACY_RES_DB = 0, LEGACY_JB = 0, LEGACY_NODELAY = 0;
+
+// ── 기본값(부팅 값). 굳히기 과제는 이 아홉 줄의 숫자만 고친다 ──────────────────
+constexpr int DEFAULT_TXGAIN  = 4;      // 보내는 이득(배) — 평탄화(lvl)가 꺼져 있을 때의 고정 이득이자, 켜져 있을 때 "말소리가 아닌 것"에 거는 이득
+constexpr int DEFAULT_VOL     = 100;    // 코덱 재생 음량(%)
+constexpr int DEFAULT_MIC_DB  = 24;     // 마이크 증폭(dB)
+constexpr int DEFAULT_KBPS    = 24;     // Opus 비트율(kbps)
+constexpr int DEFAULT_LVL     = 0;      // 보내는 크기 평탄화: 0 = 끔(고정 이득), 6~16 = 말소리에 거는 이득의 상한(배)
+constexpr int DEFAULT_TILT    = 0;      // 보내는 소리의 고음 올리기: 0 = 끔, 1 = 조금(4kHz +4dB), 2 = 많이(4kHz +7dB)
+constexpr int DEFAULT_RES_DB  = 0;      // 남은 메아리 누르기(dB): 0 = 끔
+constexpr int DEFAULT_JB      = 0;      // 지터 버퍼: 0 = 3b 의 규칙 그대로, 3~10 = 새 규칙 + 목표 상한(프레임)
+constexpr int DEFAULT_NODELAY = 0;      // 교환기 소켓의 TCP_NODELAY
+
+// ── 범위 ──────────────────────────────────────────────────────────────────────
+constexpr int TXGAIN_MIN = 1, TXGAIN_MAX = 4;
+constexpr int VOL_MIN = 0, VOL_MAX = 100;
+// 마이크 증폭은 3b 의 값(24dB)보다 올리지 못한다: 올리면 스피커 → 마이크 고리 이득이 그만큼 오르고, 1번 기기는 24dB 에서도 큰 소리의 메아리가 마이크에서 넘친다(PL3c-D9).
+constexpr int MIC_DB_MIN = 12, MIC_DB_MAX = 24, MIC_DB_STEP = 3;
+// Opus 비트율: 부품의 범위는 6~510kbps(esp_opus_enc_set_bitrate). 위를 40 으로 묶는 이유는 PL3c-D8(압축 출력 버퍼·캡처 태스크 스택).
+constexpr int KBPS_MIN = 12, KBPS_MAX = 40;
+constexpr int LVL_MIN = 6, LVL_MAX = 16;            // 0(끔)은 따로
+constexpr int TILT_MAX = 2;                         // 0, 1, 2
+constexpr int RES_DB_STEP = 6, RES_DB_MAX = 18;     // 0, 6, 12, 18
+constexpr int JB_MIN = 3, JB_MAX = 10;              // 0(옛 규칙)은 따로
+constexpr int VTONE_MIN = 300, VTONE_MAX = 7000, VTONE_DEFAULT = 1000;   // 시험음 주파수(Hz). 표본율 16kHz 의 절반(8kHz) 아래
+
+inline bool ok_txgain(int v) { return v >= TXGAIN_MIN && v <= TXGAIN_MAX; }
+inline bool ok_vol(int v) { return v >= VOL_MIN && v <= VOL_MAX; }
+inline bool ok_mic_db(int v) { return v >= MIC_DB_MIN && v <= MIC_DB_MAX && v % MIC_DB_STEP == 0; }
+inline bool ok_kbps(int v) { return v >= KBPS_MIN && v <= KBPS_MAX; }
+inline bool ok_lvl(int v) { return v == 0 || (v >= LVL_MIN && v <= LVL_MAX); }
+inline bool ok_tilt(int v) { return v >= 0 && v <= TILT_MAX; }
+inline bool ok_res_db(int v) { return v >= 0 && v <= RES_DB_MAX && v % RES_DB_STEP == 0; }
+inline bool ok_jb(int v) { return v == 0 || (v >= JB_MIN && v <= JB_MAX); }
+inline bool ok_nodelay(int v) { return v == 0 || v == 1; }
+inline bool ok_vtone(int v) { return v >= VTONE_MIN && v <= VTONE_MAX; }
+
+// 굳힌 값이 범위를 벗어나면 빌드가 실패한다(잘못 굳힌 판이 기기에 올라가지 않게).
+static_assert(TXGAIN_MIN <= DEFAULT_TXGAIN && DEFAULT_TXGAIN <= TXGAIN_MAX, "DEFAULT_TXGAIN out of range");
+static_assert(VOL_MIN <= DEFAULT_VOL && DEFAULT_VOL <= VOL_MAX, "DEFAULT_VOL out of range");
+static_assert(MIC_DB_MIN <= DEFAULT_MIC_DB && DEFAULT_MIC_DB <= MIC_DB_MAX && DEFAULT_MIC_DB % MIC_DB_STEP == 0, "DEFAULT_MIC_DB out of range");
+static_assert(KBPS_MIN <= DEFAULT_KBPS && DEFAULT_KBPS <= KBPS_MAX, "DEFAULT_KBPS out of range");
+static_assert(DEFAULT_LVL == 0 || (LVL_MIN <= DEFAULT_LVL && DEFAULT_LVL <= LVL_MAX), "DEFAULT_LVL out of range");
+static_assert(DEFAULT_TILT >= 0 && DEFAULT_TILT <= TILT_MAX, "DEFAULT_TILT out of range");
+static_assert(DEFAULT_RES_DB >= 0 && DEFAULT_RES_DB <= RES_DB_MAX && DEFAULT_RES_DB % RES_DB_STEP == 0, "DEFAULT_RES_DB out of range");
+static_assert(DEFAULT_JB == 0 || (JB_MIN <= DEFAULT_JB && DEFAULT_JB <= JB_MAX), "DEFAULT_JB out of range");
+static_assert(DEFAULT_NODELAY == 0 || DEFAULT_NODELAY == 1, "DEFAULT_NODELAY out of range");
+
+// 손잡이 아홉 개의 지금 값. STAT 이 아니라 TUNE 줄에 싣는다(기존 STAT 줄은 한 글자도 바꾸지 않는다).
+struct Knobs {
+    int txgain = DEFAULT_TXGAIN, vol = DEFAULT_VOL, mic_db = DEFAULT_MIC_DB, kbps = DEFAULT_KBPS;
+    int lvl = DEFAULT_LVL, tilt = DEFAULT_TILT, res_db = DEFAULT_RES_DB, jb = DEFAULT_JB, nodelay = DEFAULT_NODELAY;
+};
+inline bool same(const Knobs &a, const Knobs &b)
+{
+    return a.txgain == b.txgain && a.vol == b.vol && a.mic_db == b.mic_db && a.kbps == b.kbps && a.lvl == b.lvl && a.tilt == b.tilt && a.res_db == b.res_db && a.jb == b.jb && a.nodelay == b.nodelay;
+}
+inline Knobs legacy()
+{
+    Knobs k;
+    k.txgain = LEGACY_TXGAIN; k.vol = LEGACY_VOL; k.mic_db = LEGACY_MIC_DB; k.kbps = LEGACY_KBPS; k.lvl = LEGACY_LVL; k.tilt = LEGACY_TILT; k.res_db = LEGACY_RES_DB; k.jb = LEGACY_JB; k.nodelay = LEGACY_NODELAY;
+    return k;
+}
+// dflt = 지금 값이 부팅 값과 같은가. legacy = 지금 값이 5935ddd 의 동작과 같은가(기본값을 굳히기 전의 빌드는 부팅 직후 둘 다 1, 굳힌 빌드는 dflt 만 1).
+inline bool is_default(const Knobs &k) { return same(k, Knobs()); }
+inline bool is_legacy(const Knobs &k) { return same(k, legacy()); }
+// selftest 의 합격 판정(되먹임 칸 300 이상)과 vping 의 삑 찾기(1500 이상)는 "보내는 이득 4배 · 음량 100"을 전제로 한다(fix 2 재검수 Minor 3):
+//   음량을 낮추면 되먹임 칸이 같이 작아지고(88 아래면 ok=0), 응답하는 기기의 이득이 낮으면 돌아온 삑이 문턱에 못 미친다(VPING timeout).
+//   진단 출처(ref·mic)에는 평탄화·메아리 누르기를 걸지 않으므로(voice.cpp) 다른 손잡이는 이 판정을 흔들지 않는다.
+inline bool meas_valid(const Knobs &k) { return k.txgain == LEGACY_TXGAIN && k.vol == LEGACY_VOL; }
+
+// Opus 압축기가 요구하는 출력 버퍼 크기(바이트). 부품(esp_audio_codec 2.5.0)의 esp_opus_enc_set_bitrate 가 비트율을 바꿀 때 다시 셈하는 식 그대로다
+//   (역어셈블: 프레임 표본 수 × 비트율 ÷ (표본율 × 8) + 100 — 24kbps·20ms·16kHz 에서 160 = 부팅 줄의 opus_out_max).
+//   esp_opus_enc_process 는 넘겨준 출력 버퍼 길이가 이 값보다 작으면 그 프레임을 압축하지 않고 오류를 낸다 → 비트율을 올린 뒤에는 이 값을 다시 물어야 한다.
+constexpr int opus_out_need(int kbps, int frame_samples = 320, int rate = 16000) { return frame_samples * kbps * 1000 / (rate * 8) + 100; }
+constexpr int OPUS_OUT_CAP = 256;                   // 캡처 태스크의 압축 출력 버퍼. 40kbps 의 요구(200)를 넉넉히 담는다
+static_assert(opus_out_need(KBPS_MAX) <= OPUS_OUT_CAP, "the encoder output buffer must hold the largest bitrate the knob allows");
+static_assert(opus_out_need(24) == 160, "24 kbps / 20 ms / 16 kHz -> 60 + 100 (the value the device printed as opus_out_max)");
+
+}  // namespace voicetune
```

- [x] **Step 4: `jitter_buffer.h` 의 규칙**

```bash
cd ~/tab5-toy-phone && git apply $P/T2-03-jitter-policy.patch
```
```diff
--- a/firmware/tab5_phone/main/jitter_buffer.h
+++ b/firmware/tab5_phone/main/jitter_buffer.h
@@ -13,6 +13,18 @@
 //   16·16·16·32ms 간격으로 불린다(평균 20ms). 10초·1초 규칙은 pop 횟수(500·50)로 센다.
 // JB-D1: 전송이 TCP(WebSocket)라 순서 뒤바뀜·중복은 실제로는 오지 않는다. 그래도 순번으로 정렬·중복 제거를 하는 것은
 //        교환기를 UDP 계열(LiveKit, PH-D1 탈출구)로 바꿔도 이 파일을 그대로 쓰기 위해서다. 비용: 칸 32개 × 398바이트 = 약 13KB.
+// JB-D3 (Phase 3c, 계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D5): 깊이를 정하는 규칙을 Policy 로 묶어 실행 중에 바꿀 수 있게 했다.
+//        기본(legacy_policy)은 위의 3b 규칙 그대로다(시험 65개가 고치지 않고 통과). 새 규칙 tuned_policy(상한)은 흔들리는 망에서 지연이 쌓여 머무는 것을 고친다:
+//          ① 목표의 상한을 낮춘다(3b 는 200ms): 2026-10-05 아침의 망은 0.15~0.25초 멈춤이 5초에 한 번꼴이라 "10초 무사"가 오지 않아 목표가 상한까지 올라가 머물렀다
+//             (양쪽 160~200ms = 왕복 지연 540~570ms 의 절반 이상). 멈춤이 목표보다 길면 깊은 버퍼로도 끊김을 못 막는다 — 지연만 늘어난다.
+//             단 끊김이 1초 안에 세 번 거듭되면(절전이 다시 켜진 것 같은 망 — 0.3초마다 뭉쳐 온다) 그동안은 3b 의 규칙으로 돈다(상한 200ms, ③ 도 3b 처럼):
+//             낮은 상한을 고집하면 소리의 대부분을 버리게 된다(맥 시뮬레이션: 상한 4 고정이면 33% 만 튼다 → 이 예외로 3b 와 같은 88%).
+//             "세 번"인 이유: 통화가 열린 직후에는 끊김이 한두 번 붙어 오는 일이 흔하다(3b 기기 기록) — 그것으로 상한을 넘기지 않으려는 것.
+//          ② 5초 무사하면 목표를 줄인다(3b 는 10초).
+//          ③ 깊어지면 1초 기다렸다 한꺼번에 버리는 대신(0.2초를 통째로 건너뛴다) 0.5초 지켜본 뒤 0.1초마다 한 프레임씩 버린다(20ms 씩 — 이어 붙는 자리는 Concealer 가 5ms 에 걸쳐 잇는다).
+//             다시 틀기 시작한 직후 0.5초 안에 몰려온 꼬리는 기다리지 않고 바로 버린다(이미 그만큼 늦은 소리다).
+//        비용: 상한이 낮으면 그보다 긴 멈춤마다 끊긴다(3b 는 200ms 까지 버텼다) — 맥 시뮬레이션(test_voice_core.cpp --info)이 아침 모양의 망에서 두 규칙의 지연·끊김을 나란히 찍는다.
+//        되돌리기: legacy_policy(시리얼 `jb 0`).
 #pragma once
 #include <cstddef>
 #include <cstdint>
@@ -31,6 +43,24 @@
 const int    SLOTS           = 32;     // 순번 창(2의 거듭제곱)
 const size_t MAX_PAYLOAD     = 398;    // 음성 프레임 400바이트 - 순번 2바이트(§9.4)
 
+// JB-D3 깊이를 정하는 규칙. 값이 LEGACY 와 같으면 동작이 3b 와 글자 그대로 같다.
+struct Policy {
+    int max_frames;         // 목표의 상한(프레임). MIN_FRAMES 이상
+    int calm_pops;          // 이만큼 끊김 없이 틀면 목표를 한 프레임 줄인다
+    int catchup_pops;       // 목표보다 2프레임 넘게 깊은 채 이만큼 지나면 줄인다
+    int catchup_step;       // 0 = 그때 목표까지 한꺼번에 줄인다(3b). N = 가장 오래된 한 프레임만 버리고, 계속 깊으면 N pop 뒤에 또 한 프레임
+    int resume_pops;        // 다시 틀기 시작한 뒤 이 pop 수 안에 2프레임 넘게 깊어지면 기다리지 않고 바로 목표로 줄인다. 0 = 하지 않는다(3b)
+    int burst_pops;         // 앞 끊김 뒤 이 pop 수 안에 또 끊기는 일이 두 번 이어지면(= 세 번째 끊김) "거듭 끊기는 망"으로 본다: 끊김이 이 pop 수보다 뜸해질 때까지 상한은 MAX_FRAMES, 줄이는 규칙은 3b 것. 0 = 보지 않는다
+};
+const int TUNED_CALM_POPS = 250, TUNED_CATCHUP_POPS = 25, TUNED_CATCHUP_STEP = 5, TUNED_RESUME_POPS = 25, TUNED_BURST_POPS = 50;   // 5초 / 0.5초 / 0.1초 / 0.5초 / 1초
+inline Policy legacy_policy() { Policy p = {10, 500, 50, 0, 0, 0}; return p; }
+// cap = 목표 상한(프레임). 범위 밖이면 가까운 끝으로 맞춘다(START_FRAMES 아래로는 내리지 않는다 — 통화 시작의 60ms 는 그대로다).
+inline Policy tuned_policy(int cap)
+{
+    Policy p = {cap < 3 ? 3 : cap > 10 ? 10 : cap, TUNED_CALM_POPS, TUNED_CATCHUP_POPS, TUNED_CATCHUP_STEP, TUNED_RESUME_POPS, TUNED_BURST_POPS};
+    return p;
+}
+
 enum class Kind : uint8_t { SILENCE, PLAY, LOST };
 
 struct Pop {
@@ -54,16 +84,34 @@
 
 class Buffer {
 public:
-    Buffer() { reset(); }
+    Buffer() : pol_(legacy_policy()) { reset(); }
 
-    void reset()
+    void reset()                                                    // 규칙(Policy)은 남는다 — 통화마다 다시 걸지 않아도 된다
     {
         for (int i = 0; i < SLOTS; ++i) slot_[i].used = false;
         count_ = 0; started_ = false; buffering_ = true;
-        target_ = START_FRAMES; conceal_run_ = 0; calm_ = 0; deep_ = 0;
+        target_ = START_FRAMES; conceal_run_ = 0; calm_ = 0; deep_ = 0; resume_left_ = 0; since_urun_ = 0x7FFFFFFF; bursty_ = false; short_run_ = 0;
         next_ = 0; stats_ = Stats();
+        if (target_ > pol_.max_frames) target_ = pol_.max_frames;
     }
 
+    // JB-D3 규칙을 바꾼다(통화 중에도 된다). 목표가 새 상한보다 높으면 그 자리에서 상한으로 내린다 — 넘치는 프레임은 규칙대로(한꺼번에 또는 하나씩) 줄어든다.
+    void set_policy(const Policy &p)
+    {
+        pol_ = p;
+        if (pol_.max_frames < MIN_FRAMES) pol_.max_frames = MIN_FRAMES;
+        if (pol_.max_frames > MAX_FRAMES) pol_.max_frames = MAX_FRAMES;
+        if (pol_.calm_pops < 1) pol_.calm_pops = 1;
+        if (pol_.catchup_pops < 1) pol_.catchup_pops = 1;
+        if (pol_.catchup_step < 0) pol_.catchup_step = 0;
+        if (pol_.catchup_step > pol_.catchup_pops) pol_.catchup_step = pol_.catchup_pops;
+        if (pol_.resume_pops < 0) pol_.resume_pops = 0;
+        if (pol_.burst_pops < 0) pol_.burst_pops = 0;
+        if (target_ > pol_.max_frames) target_ = pol_.max_frames;
+        calm_ = 0; deep_ = 0; bursty_ = false; short_run_ = 0;
+    }
+    const Policy &policy() const { return pol_; }
+
     int depth() const { return count_; }
     bool buffering() const { return buffering_; }               // 채우는 중(무음) — 통화 시작 직후와 끊긴 뒤
     int target_frames() const { return target_; }
@@ -93,20 +141,29 @@
             if (count_ < target_) return out;                    // 아직 덜 찼다 → 무음
             if (count_ - target_ >= 2) stats_.overflow_drop += trim_to(target_);   // 밀려 있던 만큼은 버리고 가장 새 프레임부터(한 프레임 여유는 둔다)
             buffering_ = false; conceal_run_ = 0; calm_ = 0; deep_ = 0;
+            resume_left_ = bursty_ ? 0 : pol_.resume_pops;
         }
 
-        if (count_ - target_ >= 2) {                             // 틀던 중 깊어짐: 1초 지켜본 뒤 목표로
-            if (++deep_ >= CATCHUP_POPS) { stats_.catchup_drop += trim_to(target_); deep_ = 0; }
+        if (bursty_ && since_urun_ > pol_.burst_pops) bursty_ = false;      // 끊김이 뜸해졌다 → 새 규칙으로 돌아간다
+        const int wait = bursty_ ? CATCHUP_POPS : pol_.catchup_pops, step = bursty_ ? 0 : pol_.catchup_step;
+        if (count_ - target_ >= 2) {                             // 틀던 중 깊어짐
+            if (resume_left_ > 0) { stats_.overflow_drop += trim_to(target_); deep_ = 0; }   // JB-D3 ③ 다시 틀기 시작한 직후 몰려온 꼬리: 바로 목표로("다시 틀기 시작할 때 버린 것"으로 센다)
+            else if (++deep_ >= wait) {                          // 3b: 1초 지켜본 뒤 목표로 / 새 규칙: 0.5초 지켜본 뒤 0.1초마다 한 프레임
+                if (step == 0) { stats_.catchup_drop += trim_to(target_); deep_ = 0; }
+                else { stats_.catchup_drop += trim_to(count_ - 1); deep_ = wait - step; }
+            }
         } else {
             deep_ = 0;
         }
+        if (resume_left_ > 0) --resume_left_;
+        if (since_urun_ < 0x7FFFFFFF) ++since_urun_;
 
         Slot &s = slot_[next_ & (SLOTS - 1)];
         if (s.used) {
             out.kind = Kind::PLAY; out.seq = next_; out.len = s.len; memcpy(out.data, s.data, s.len);
             s.used = false; --count_; ++next_;
             conceal_run_ = 0; ++stats_.played;
-            if (++calm_ >= CALM_POPS) { calm_ = 0; if (target_ > MIN_FRAMES) --target_; }
+            if (++calm_ >= pol_.calm_pops) { calm_ = 0; if (target_ > MIN_FRAMES) --target_; }
             return out;
         }
 
@@ -116,7 +173,13 @@
         }
 
         // 다 떨어졌다 — 기다리던 순번(next_)은 그대로 둔다(JB-D2)
-        if (conceal_run_ == 0) { ++stats_.underruns; calm_ = 0; if (target_ < MAX_FRAMES) ++target_; }
+        if (conceal_run_ == 0) {
+            ++stats_.underruns; calm_ = 0;
+            short_run_ = since_urun_ <= pol_.burst_pops ? short_run_ + 1 : 0;  // 앞 끊김 뒤 금방 또 끊긴 횟수(이어서)
+            bursty_ = pol_.burst_pops > 0 && short_run_ >= 2;                  // JB-D3 ①: 세 번째 — 거듭 끊기는 망
+            if (target_ < (bursty_ ? MAX_FRAMES : pol_.max_frames)) ++target_;
+            since_urun_ = 0;
+        }
         if (conceal_run_ < MAX_CONCEAL) {
             ++conceal_run_; ++stats_.lost; out.kind = Kind::LOST;
             return out;
@@ -145,8 +208,9 @@
 
     Slot     slot_[SLOTS];
     int      count_;
-    bool     started_, buffering_;
-    int      target_, conceal_run_, calm_, deep_;
+    bool     started_, buffering_, bursty_;
+    int      target_, conceal_run_, calm_, deep_, resume_left_, since_urun_, short_run_;
+    Policy   pol_;
     uint16_t next_;
     Stats    stats_;
 };
```

- [x] **Step 5: 통과를 확인한다**

Run: 맥 시험(`T="voice_tune"`)
Expected: `jitter_buffer: 90 ok, 0 failed`, `voice_tune: 17 ok, 0 failed`, 나머지는 기준선과 같다(`voice_core: 138 ok` — 기본 규칙이 3b 그대로라는 뜻이다), 경고 전부 0줄. 부팅 값: `python3 $P/../verify/freeze_tune.py ~/tab5-toy-phone/firmware/tab5_phone/main/voice_tune.h --expect legacy` → `기대와 같음: legacy`.
(사본 빌드: `0x288760`, 우리 코드 경고 0.)

- [x] **Step 6: 커밋**

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/tools/test_voice_tune.cpp firmware/tab5_phone/tools/test_jitter_buffer.cpp firmware/tab5_phone/main/voice_tune.h firmware/tab5_phone/main/jitter_buffer.h && git commit -m "feat(phone): 통화 품질 손잡이의 값·범위(voice_tune.h) + 지터 버퍼 규칙을 Policy 로 — 기본은 3b 그대로, 시험 먼저 (PL3c-D3·D5)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 3: `voice_core.h` — 재는 수단(시험음 주파수·`probe`·도착 틈·마이크 넘침)과 규칙 건네기 (시험 먼저)

> **실행됨(2026-10-05, 구현 에이전트 — 보고 `chunk1-impl-report.md`)**: 커밋 `657224a`. 실패 → 통과의 문구가 아래 Expected 와 같았다.

**Files:**
- Modify: `firmware/tab5_phone/main/voice_core.h`
- Test: `firmware/tab5_phone/tools/test_voice_core.cpp`(추가)

**Interfaces:**
- Consumes: Task 2 의 `jitterbuf::Policy`·`legacy_policy()`·`tuned_policy(int)`.
- Produces(이름공간 `voicecore`): `TestTone::set_hz(int)`, `enum class Src { AEC, MIC, REF, TONE, PROBE }`(이름 `"probe"`), `PROBE_AMP = 500`, `bool uses_dyn(Src)`(= `AEC`·`PROBE`), `uint32_t clipped(const int16_t*, int)`, `class ArrivalGap { reset(); hit(uint32_t now_ms); max_ms(); }`, `Receiver<Lock>::set_policy(const jitterbuf::Policy&)`. `src_allowed(Src::PROBE, true)` 는 참(스피커가 켜져 있어도 된다).

- [x] **Step 1: 시험을 먼저 넣는다**(아침의 망을 흉내 낸 시뮬레이션 포함 — `--info` 로 위 "지연 예산"의 표를 찍는다)

```bash
cd ~/tab5-toy-phone && git apply $P/T3-01-voice-core-tests.patch
```
```diff
--- a/firmware/tab5_phone/tools/test_voice_core.cpp
+++ b/firmware/tab5_phone/tools/test_voice_core.cpp
@@ -179,6 +179,75 @@
     }
     int pct(int p) const { if (delay_ms.empty()) return -1; std::vector<int> v = delay_ms; std::sort(v.begin(), v.end()); return v[(v.size() - 1) * (size_t)p / 100]; }
 };
+
+// ── Phase 3c (계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D5): 여러 번 멈추는 망에서 지터 버퍼의 규칙을 견준다 ──
+//    위 Sim 과 같은 길(보내는 쪽 16ms 묶음 → 프레임 → 망 → Receiver → 받는 쪽 16ms 묶음)에 2026-10-05 아침의 도착 모양을 넣는다:
+//    흔들림(jitter) + 짧은 멈춤이 몇 초마다(그동안 보낸 것은 풀릴 때 한꺼번에 — TCP) + 가끔 긴 멈춤, 또는 일정한 주기로만 배달(Wi-Fi 절전의 모양).
+struct Net3c {
+    int64_t base = 5000, jitter = 20000;                    // 망 지연 = base + [0, jitter) µs
+    int64_t stall_every = 0, stall_min = 0, stall_max = 0;  // 짧은 멈춤: 평균 간격(±50%)과 길이 범위(µs). 0 = 없음
+    int64_t big_every = 0, big_len = 0;                     // 긴 멈춤
+    int64_t clump = 0;                                      // > 0: 이 주기의 경계에서만 배달한다
+};
+struct Run3c { double mean = 0, urun_min = 0, lost_min = 0, drop_min = 0, played_pct = 0, tgt_mean = 0; int p95 = 0, tgt_max = 0; };
+static Run3c run3c(const Net3c &net, const jitterbuf::Policy &pol, int seconds, uint32_t seed)
+{
+    g_seed = seed; g_sig = Sig::RAMP;
+    static Rx rx(g_nolock);
+    rx.set_policy(pol);
+    rx.start();
+    Uplink up; up.start();
+    struct Pkt { int64_t at; uint8_t f[64]; size_t n; };
+    std::vector<Pkt> wire; size_t head = 0;
+    const int64_t end = (int64_t)seconds * 1000000;
+    std::vector<std::pair<int64_t, int64_t>> stalls;
+    if (net.stall_every) for (int64_t t = net.stall_every; t < end;) { stalls.push_back({t, net.stall_min + (int64_t)rnd((uint32_t)(net.stall_max - net.stall_min + 1))}); t += net.stall_every / 2 + (int64_t)rnd((uint32_t)net.stall_every); }
+    if (net.big_every) for (int64_t t = net.big_every; t < end; t += net.big_every) stalls.push_back({t, net.big_len});
+    double tx_next = 16000; int64_t rx_next = 5000 + 16000, last_arrive = 0, g = 0; uint32_t sent = 0;
+    std::vector<int> delay; double tsum = 0; long tn = 0; Run3c r;
+    while (rx_next < end) {
+        if (tx_next <= (double)rx_next) {
+            const int64_t now = (int64_t)tx_next;
+            int16_t blk[BLOCK]; for (int i = 0; i < BLOCK; ++i) blk[i] = sig(g + i);
+            up.push(blk); g += BLOCK;
+            int16_t fr[FRAME]; uint16_t seq;
+            while (up.pop(fr, &seq)) {
+                Pkt p; p.n = frame_bytes((uint32_t)(DISCARD_BLOCKS * BLOCK) + sent * FRAME, seq, p.f); ++sent;
+                int64_t at = now + net.base + (net.jitter ? (int64_t)rnd((uint32_t)net.jitter) : 0);
+                for (const auto &s : stalls) if (at >= s.first && at < s.first + s.second) at = s.first + s.second;
+                if (net.clump) at = (at / net.clump + 1) * net.clump;
+                if (at < last_arrive) at = last_arrive;                         // TCP: 순서대로
+                last_arrive = p.at = at; wire.push_back(p);
+            }
+            tx_next += 16000;
+            continue;
+        }
+        const int64_t now = rx_next;
+        while (head < wire.size() && wire[head].at <= now) { rx.push(wire[head].f, wire[head].n); ++head; if (rx.want_pump()) rx.pump(dec, fresh); }
+        int16_t out[BLOCK]; pull(rx, out);
+        bool run = true; for (int i = 1; i < BLOCK; ++i) if ((out[i] - out[i - 1] + RAMP_MOD) % RAMP_MOD != 1) { run = false; break; }
+        if (run) {
+            const double sample_us = 1e6 / RATE;
+            const int64_t newest = (int64_t)((double)now / sample_us);
+            const int64_t k = newest - ((newest - out[0]) % RAMP_MOD + RAMP_MOD) % RAMP_MOD;
+            delay.push_back((int)(((double)now - (double)k * sample_us) / 1000.0));
+        }
+        const RxStats st = rx.stats(); tsum += st.target; ++tn; if (st.target > r.tgt_max) r.tgt_max = st.target;
+        rx_next += 16000;
+    }
+    const RxStats st = rx.stats(); const double min = seconds / 60.0;
+    std::sort(delay.begin(), delay.end());
+    if (!delay.empty()) { double sum = 0; for (int d : delay) sum += d; r.mean = sum / (double)delay.size(); r.p95 = delay[delay.size() * 95 / 100]; }
+    r.urun_min = st.jb.underruns / min; r.lost_min = st.concealed / min; r.drop_min = (st.jb.overflow_drop + st.jb.catchup_drop + st.jb.late) / min;
+    r.played_pct = 100.0 * st.played / (st.frames ? st.frames : 1); r.tgt_mean = tn ? tsum / (double)tn : 0;
+    rx.stop(); rx.set_policy(jitterbuf::legacy_policy());
+    g_sig = Sig::SINE;
+    return r;
+}
+static Net3c net_quiet() { Net3c n; n.base = 3000; n.jitter = 4000; return n; }
+static Net3c net_night() { Net3c n; n.stall_every = 45000000; n.stall_min = 150000; n.stall_max = 250000; return n; }                    // 2026-10-05 02시: 0.15~0.25초 멈춤이 3분에 네 번쯤
+static Net3c net_morning() { Net3c n; n.jitter = 40000; n.stall_every = 5500000; n.stall_min = 150000; n.stall_max = 250000; n.big_every = 60000000; n.big_len = 800000; return n; }   // 08시: 30초에 5~6번 + 가끔 0.8초
+static Net3c net_powersave() { Net3c n; n.jitter = 10000; n.clump = 307200; return n; }                                                 // Wi-Fi 절전(fix 3 전): 0.3초마다 뭉쳐 온다
 
 int main(int argc, char **argv)
 {
@@ -755,7 +824,94 @@
         CHECK("rx: under() (a lock-free read for the output task) equals the count in stats()", rx.under() >= 1 && rx.under() == rx.stats().under);
         rx.start();
         CHECK("rx: a new call resets it", rx.under() == 0);
+    }
+
+    // ════ Phase 3c: 측정 수단과 지터 버퍼 규칙 (계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D4·D5) ════
+    {
+        // 시험음의 주파수를 바꾼다(vtone) — 여러 주파수로 "어느 대역이 어디서 깎이는가"를 잰다
+        bool exact = true; int cross[3] = {0, 0, 0}; const int hz[3] = {300, 3000, 6000};
+        for (int k = 0; k < 3; ++k) {
+            TestTone t; t.set_hz(hz[k]); Pcm v(RATE); t.render(v.data(), RATE, TEST_AMP);
+            for (int i = 1; i < RATE; ++i) if (v[i - 1] < 0 && v[i] >= 0) ++cross[k];
+            for (int i = 0; i < RATE; ++i) if (std::abs(v[i] - TEST_AMP * std::sin(2 * PI * hz[k] * i / RATE)) > 6) exact = false;
+        }
+        CHECK("test tone: set_hz gives an exact sine at 300, 3000 and 6000 Hz, same level", exact && std::abs(cross[0] - 300) <= 1 && std::abs(cross[1] - 3000) <= 1 && std::abs(cross[2] - 6000) <= 1);
+        TestTone a, b; Pcm x(RATE), y(RATE); a.render(x.data(), RATE, TEST_AMP); b.set_hz(TEST_HZ); b.render(y.data(), RATE, TEST_AMP);
+        CHECK("test tone: without set_hz it is still 1 kHz (the 3b tone, sample for sample)", x == y);
+        TestTone c; Pcm z(400); c.render(z.data(), 200, 4000); c.set_hz(2500); c.render(z.data() + 200, 200, 4000);
+        CHECK("test tone: changing the frequency keeps the phase (no click at the switch)", max_jump(z) <= (int)std::ceil(4000 * 2 * PI * 2500 / RATE) + 4);
     }
+    {
+        // 출처 probe: 마이크 자리에 시험음을 넣어 에코 제거를 거치게 한다. 보내는 것은 에코 제거 결과(평소와 같은 길 — 같은 이득)
+        int16_t clean[BLOCK], mic[BLOCK], ref[BLOCK], out[BLOCK], want[BLOCK]; TestTone tone;
+        for (int i = 0; i < BLOCK; ++i) { clean[i] = (int16_t)(50 + i % 9); mic[i] = (int16_t)-700; ref[i] = 9000; }
+        pick(Src::PROBE, clean, mic, ref, tone, out); pick(Src::AEC, clean, mic, ref, tone, want);
+        Src s = Src::AEC;
+        CHECK("diagnostic 'probe': what is sent is the echo-cancelled signal with the same gain as normal (the tone goes in before the canceller, on the device)",
+              std::equal(out, out + BLOCK, want) && parse_src("probe", &s) && s == Src::PROBE && std::string(src_name(Src::PROBE)) == "probe" && PROBE_AMP == 500 && PROBE_AMP * TX_GAIN_DEFAULT == TEST_AMP);
+        CHECK("diagnostic 'probe': allowed with the speaker on (the real microphone is not used, so no loop closes); mic / ref still are not",
+              src_allowed(Src::PROBE, true) && src_allowed(Src::PROBE, false) && effective_src(Src::PROBE, true) == Src::PROBE && !src_allowed(Src::MIC, true) && !src_allowed(Src::REF, true));
+        CHECK("the leveller / echo press apply only to the echo-cancelled path (aec, probe) — diagnostic mic / ref keep the fixed gain so 'vping' stays comparable",
+              uses_dyn(Src::AEC) && uses_dyn(Src::PROBE) && !uses_dyn(Src::MIC) && !uses_dyn(Src::REF) && !uses_dyn(Src::TONE));
+    }
+    {
+        int16_t m[8] = {0, 32767, -32768, 32766, -32767, 100, 32767, -1};
+        CHECK("mic clip count: samples sitting at either end of the 16-bit range (an echo that overloads the mic cannot be cancelled)", clipped(m, 8) == 3 && clipped(m, 1) == 0 && clipped(m, 0) == 0);
+    }
+    {
+        ArrivalGap g;
+        CHECK("arrival gap: nothing yet -> 0", g.max_ms() == 0);
+        g.hit(1000);
+        CHECK("arrival gap: one frame alone is not a gap", g.max_ms() == 0);
+        g.hit(1020); g.hit(1041); g.hit(1300); g.hit(1320);
+        CHECK("arrival gap: the longest pause between two frames (259 ms here)", g.max_ms() == 259);
+        g.reset(); g.hit(5000);
+        CHECK("arrival gap: reset per call — the pause between two calls is not counted", g.max_ms() == 0);
+        ArrivalGap w; w.hit(0xFFFFFFF0u); w.hit(0x00000010u);
+        CHECK("arrival gap: the millisecond clock wrapping around is still 32 ms", w.max_ms() == 32);
+    }
+    {
+        // Receiver 가 규칙을 지터 버퍼에 건넨다. 통화가 새로 열려도 남는다
+        g_sig = Sig::SINE;
+        static Rx rx(g_nolock);
+        rx.set_policy(jitterbuf::tuned_policy(4)); rx.start();
+        uint32_t idx = 0;
+        for (int round = 0; round < 8; ++round) {                               // 3초 틀고 0.4초 멈추기를 여덟 번
+            for (int i = 0; i < 150; ++i) { feed(rx, idx++); int16_t b[BLOCK]; pull(rx, b); if (i % 4 == 3) pull(rx, b); }
+            pull_n(rx, 25); idx += 20;
+        }
+        const RxStats capped = rx.stats();
+        rx.start();
+        for (int i = 0; i < 3; ++i) feed(rx, i);
+        rx.set_policy(jitterbuf::legacy_policy()); rx.start(); idx = 0;
+        for (int round = 0; round < 8; ++round) {
+            for (int i = 0; i < 150; ++i) { feed(rx, idx++); int16_t b[BLOCK]; pull(rx, b); if (i % 4 == 3) pull(rx, b); }
+            pull_n(rx, 25); idx += 20;
+        }
+        const RxStats old = rx.stats();
+        CHECK("rx: set_policy reaches the jitter buffer — eight stalls 3 s apart leave the tuned rule at its ceiling (4) and the 3b rule well above it",
+              capped.jb.underruns >= 7 && capped.target <= 4 && old.target >= 8);
+    }
+    Run3c m_old, m_jb4, m_jb6, n_old, n_jb4, q_old, q_jb4, p_old, p_jb4;
+    {
+        // 아침의 망: 지금 규칙은 목표가 상한(200ms)까지 올라가 머문다. 새 규칙은 상한에서 멈추고 지연이 0.1초 넘게 준다. 값은 끊김이 느는 것
+        const jitterbuf::Policy old = jitterbuf::legacy_policy(), jb4 = jitterbuf::tuned_policy(4), jb6 = jitterbuf::tuned_policy(6);
+        m_old = run3c(net_morning(), old, 180, 1000); m_jb4 = run3c(net_morning(), jb4, 180, 1000); m_jb6 = run3c(net_morning(), jb6, 180, 1000);
+        CHECK("sim, the morning network (stalls of 0.15-0.25 s every 5.5 s): the 3b rule climbs to its 200 ms ceiling and stays (mean delay over 220 ms)",
+              m_old.tgt_max == 10 && m_old.tgt_mean > 7.5 && m_old.mean > 220);
+        CHECK("sim, the morning network: 'jb 4' holds the target near 80 ms and cuts the mean delay by more than 100 ms", m_jb4.tgt_mean < 4.3 && m_jb4.mean < m_old.mean - 100 && m_jb4.p95 < m_old.p95 - 100);
+        CHECK("sim, the morning network: the price — more underruns (each stall longer than the buffer is heard), still over 96 % of the frames played",
+              m_jb4.urun_min > m_old.urun_min && m_jb4.urun_min < 16 && m_jb4.played_pct > 96.0 && m_old.played_pct > 98.0);
+        CHECK("sim, the morning network: 'jb 6' sits in between (delay and underruns)", m_jb6.mean > m_jb4.mean + 20 && m_jb6.mean < m_old.mean - 50 && m_jb6.played_pct >= m_jb4.played_pct);
+        n_old = run3c(net_night(), old, 180, 1000); n_jb4 = run3c(net_night(), jb4, 180, 1000);
+        CHECK("sim, the night network (a stall every 45 s): the tuned rule is no worse — same underruns, the delay does not stay high after a stall",
+              n_jb4.urun_min <= n_old.urun_min + 0.4 && n_jb4.mean <= n_old.mean + 1 && n_jb4.p95 <= n_old.p95 && n_jb4.played_pct > 99.3);
+        q_old = run3c(net_quiet(), old, 120, 1000); q_jb4 = run3c(net_quiet(), jb4, 120, 1000);
+        CHECK("sim, a quiet network: no underrun, nothing dropped, the same delay under either rule", q_old.urun_min == 0 && q_jb4.urun_min == 0 && q_jb4.drop_min == 0 && std::abs(q_jb4.mean - q_old.mean) < 2);
+        p_old = run3c(net_powersave(), old, 60, 1000); p_jb4 = run3c(net_powersave(), jb4, 60, 1000);
+        CHECK("sim, frames arriving in 0.3 s clumps (Wi-Fi power save back on): the burst rule falls back to the 3b behaviour instead of throwing most of the audio away",
+              p_jb4.tgt_max == 10 && p_jb4.played_pct > p_old.played_pct - 2.0 && p_old.played_pct > 80.0);
+    }
 
     if (info) {
         printf("\n-- delay simulation (ms from a sample entering the microphone to the output task taking it; add DMA write-ahead + codec/AEC internals) --\n");
@@ -770,6 +926,18 @@
         }
         printf("  %-30s median delay over 64 runs: min %d  mean %ld  max %d\n", "LAN, start phase swept", lo, sum / n, hi);
     }
+    if (info) {
+        printf("\n-- Phase 3c: jitter rule vs network (one way, mic sample -> output task; add DMA 32 ms + codec/AEC internals). 180 s each (power save 60 s) --\n");
+        printf("  %-34s %-7s %5s %5s %7s %7s %7s %7s %5s %4s\n", "network", "rule", "mean", "p95", "urun/m", "lost/m", "drop/m", "play%", "tgt", "max");
+        const struct { const char *name; Net3c net; int sec; } nets[] = {{"quiet (3-7 ms)", net_quiet(), 180}, {"night (stall / 45 s)", net_night(), 180}, {"morning (stall / 5.5 s + 0.8 s / min)", net_morning(), 180}, {"power save (0.3 s clumps)", net_powersave(), 60}};
+        const struct { const char *name; jitterbuf::Policy pol; } rules[] = {{"jb 0", jitterbuf::legacy_policy()}, {"jb 4", jitterbuf::tuned_policy(4)}, {"jb 5", jitterbuf::tuned_policy(5)}, {"jb 6", jitterbuf::tuned_policy(6)}, {"jb 8", jitterbuf::tuned_policy(8)}};
+        for (const auto &n : nets) for (const auto &r : rules) {
+            Run3c a; const int N = 4;
+            for (int i = 0; i < N; ++i) { const Run3c x = run3c(n.net, r.pol, n.sec, 1000u + (uint32_t)i * 77u); a.mean += x.mean / N; a.p95 += x.p95; a.urun_min += x.urun_min / N; a.lost_min += x.lost_min / N; a.drop_min += x.drop_min / N; a.played_pct += x.played_pct / N; a.tgt_mean += x.tgt_mean / N; a.tgt_max = std::max(a.tgt_max, x.tgt_max); }
+            printf("  %-34s %-7s %5.0f %5d %7.1f %7.1f %7.1f %7.2f %5.1f %4d\n", n.name, r.name, a.mean, a.p95 / N, a.urun_min, a.lost_min, a.drop_min, a.played_pct, a.tgt_mean, a.tgt_max);
+        }
+        (void)m_old; (void)m_jb4; (void)m_jb6; (void)n_old; (void)n_jb4; (void)q_old; (void)q_jb4; (void)p_old; (void)p_jb4;
+    }
     printf("\n%d ok, %d failed\n", ok, bad);
     return bad ? 1 : 0;
 }
```

- [x] **Step 2: 실패를 확인한다**

Run: `cd ~/tab5-toy-phone/firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_voice_core.cpp -o /tmp/p3q/t_voice_core 2>&1 | grep -m1 "error:"`
Expected: `test_voice_core.cpp:197:8: error: no member named 'set_policy' in 'voicecore::Receiver<NoLock>'`.

- [x] **Step 3: 구현**

```bash
cd ~/tab5-toy-phone && git apply $P/T3-02-voice-core.patch
```
```diff
--- a/firmware/tab5_phone/main/voice_core.h
+++ b/firmware/tab5_phone/main/voice_core.h
@@ -74,40 +74,46 @@
     for (int i = 0; i < frames; ++i) { mic[i] = slots[i * IN_SLOTS + MIC_SLOT]; ref[i] = slots[i * IN_SLOTS + REF_SLOT]; }
 }
 
-// 1kHz 시험음(위상을 이어 간다).
+// 시험음(위상을 이어 간다). 기본 1kHz. PL3c-D4: 주파수를 바꿀 수 있다(set_hz — 시리얼 `vtone <Hz>`) → 여러 주파수로 보내 "어느 대역이 어디서 깎이는가"를 사람 없이 잰다.
 class TestTone {
 public:
     void reset() { ph_ = 0; }
+    void set_hz(int hz) { step_ = tones::phase_step((uint16_t)hz); }       // 위상은 그대로 이어 간다(바꾸는 자리에 계단이 없다)
     void render(int16_t *out, int n, int amp)
     {
-        const uint32_t step = tones::phase_step(TEST_HZ);
-        for (int i = 0; i < n; ++i) { out[i] = (int16_t)(tones::sine_q15(ph_) * amp / 32767); ph_ += step; }
+        for (int i = 0; i < n; ++i) { out[i] = (int16_t)(tones::sine_q15(ph_) * amp / 32767); ph_ += step_; }
     }
 
 private:
-    uint32_t ph_ = 0;
+    uint32_t ph_ = 0, step_ = tones::phase_step(TEST_HZ);
 };
 
 // PL3b-D11 무엇을 보낼까. AEC = 평소(에코 제거 결과). 나머지는 사람이 시리얼로 켜는 진단용이고 통화가 끝나면 AEC 로 돌아간다.
 //   MIC = 에코 제거를 거치지 않은 마이크, REF = 스피커 되먹임 칸(이 기기가 내고 있는 소리 — 앰프를 꺼도 잡힌다), TONE = 1kHz 시험음.
-enum class Src : uint8_t { AEC, MIC, REF, TONE };
+//   PL3c-D4 PROBE = 마이크 자리에 시험음(PROBE_AMP)을 넣어 에코 제거를 "거치게" 한 것. 보내는 것은 에코 제거 결과(평소와 같은 길)다 →
+//     STAT 의 v_mic(넣은 크기)와 v_aec(나온 크기)의 비가 "에코 제거가 그 주파수의 가까운 소리를 얼마나 깎는가"다. 진짜 마이크 소리는 쓰이지 않는다(고리가 닫히지 않는다).
+enum class Src : uint8_t { AEC, MIC, REF, TONE, PROBE };
+constexpr int PROBE_AMP = 500;              // 말소리가 마이크에 잡히는 크기 근처(2026-10-05 실측 v_mic 204~554). 4배로 보내면 상대에게는 시험음(2000)과 같은 크기다
 inline const char *src_name(Src s)
 {
-    switch (s) { case Src::AEC: return "aec"; case Src::MIC: return "mic"; case Src::REF: return "ref"; case Src::TONE: return "tone"; }
+    switch (s) { case Src::AEC: return "aec"; case Src::MIC: return "mic"; case Src::REF: return "ref"; case Src::TONE: return "tone"; case Src::PROBE: return "probe"; }
     return "?";
 }
 inline bool parse_src(const char *text, Src *out)
 {
-    static const Src ALL[] = {Src::AEC, Src::MIC, Src::REF, Src::TONE};
+    static const Src ALL[] = {Src::AEC, Src::MIC, Src::REF, Src::TONE, Src::PROBE};
     for (Src s : ALL) if (text && !strcmp(text, src_name(s))) { *out = s; return true; }
     return false;
 }
 // PL3b-D22 (fix 2) 진단 출처 mic·ref 는 스피커 앰프가 꺼져 있을 때만 쓴다. 이유: 양쪽이 ref(또는 mic)면 메아리 제거를 거치지 않는 고리가 닫혀
 //   (ref 는 공기도 거치지 않는다) 한 길마다 이득이 +4.5dB 이상이라 2초 안에 최대 크기가 된다(최종 검수 M-2). aec·tone 은 늘 허용.
 //   비용: 스피커를 켠 채 마이크 원음·되먹임 칸을 보내 보는 진단은 못 한다(조용한 모드에서 하면 된다). 되돌리기: src_allowed 를 늘 true 로.
-inline bool src_allowed(Src s, bool amp_on) { return !amp_on || s == Src::AEC || s == Src::TONE; }
+inline bool src_allowed(Src s, bool amp_on) { return !amp_on || s == Src::AEC || s == Src::TONE || s == Src::PROBE; }   // PROBE 는 마이크를 쓰지 않아 고리가 닫히지 않는다 → 늘 허용
 // 통화 도중 앰프가 켜졌으면(quiet off) 이미 골라 둔 mic·ref 는 aec 로 되돌린다. 캡처 태스크가 묶음마다 이 값을 쓴다(경합이 있어도 한 묶음 안에 닫힌다).
 inline Src effective_src(Src s, bool amp_on) { return src_allowed(s, amp_on) ? s : Src::AEC; }
+// PL3c-D7 보내는 크기 다듬기(tx_dyn.h — 평탄화·남은 메아리 누르기)는 "에코 제거를 거친 소리"에만 건다(aec·probe).
+//   진단 출처 mic·ref 는 3b 의 고정 이득 그대로다 → 지연 재기(vping — 상대가 ref 를 돌려보낸다)의 삑 크기가 손잡이에 따라 달라지지 않는다. tone 은 원래 이득이 없다.
+inline bool uses_dyn(Src s) { return s == Src::AEC || s == Src::PROBE; }
 
 // 시리얼 명령의 숫자 인자 읽기: 숫자(0~9)만 1~4글자, lo..hi 안. 부호·공백·소수점·글자·빈 글자·범위 밖은 거절하고 *out 을 건드리지 않는다.
 inline bool parse_uint(const char *text, int lo, int hi, int *out)
@@ -246,6 +252,32 @@
     uint32_t dropped_ = 0;
 };
 
+// PL3c-D4 마이크 묶음에서 꼭대기에 닿은(넘친) 표본 수. 스피커 소리가 마이크에서 넘치면 그 메아리는 에코 제거가 지우지 못한다 — 1번 기기는 제 스피커를 2.3~2.7배로 줍는다
+//   (2026-10-05: 상대 소리 v_spk=5904 일 때 v_mic=19777). 통화마다 세어 TUNE 줄의 mclip= 으로 낸다 → 마이크 증폭(mic 손잡이)을 내릴지의 근거.
+inline uint32_t clipped(const int16_t *s, int n)
+{
+    uint32_t c = 0;
+    for (int i = 0; i < n; ++i) if (s[i] >= 32767 || s[i] <= -32768) ++c;
+    return c;
+}
+
+// PL3c-D4 받은 프레임 사이의 가장 긴 틈(ms) — "망이 얼마나 오래 멈췄다 풀렸나"를 기기에서 바로 본다(TUNE 줄의 rxgap=). 통화마다 reset.
+//   쓰는 쪽 = 웹소켓 태스크(프레임이 올 때마다 hit), 읽는 쪽 = 시리얼 태스크. 시계(ms)가 한 바퀴 돌아도 맞는다.
+class ArrivalGap {
+public:
+    void reset() { have_ = false; max_.store(0); }
+    void hit(uint32_t now_ms)
+    {
+        if (have_.load()) { const uint32_t gap = now_ms - last_.load(); if (gap > max_.load()) max_.store(gap); }
+        last_.store(now_ms); have_.store(true);
+    }
+    uint32_t max_ms() const { return max_.load(); }
+
+private:
+    std::atomic<uint32_t> last_{0}, max_{0};
+    std::atomic<bool> have_{false};
+};
+
 // ── 받는 길 ──────────────────────────────────────────────────────────────────
 // PL3b-D7 빠진 프레임 메우기. esp_audio_codec 2.5.0 의 Opus 해제기는 손실 은닉을 해 주지 않는다(계획서의 근거) → 직전 프레임을 줄여 가며 되풀이한다.
 //   lost    직전 소리를 거울처럼 되짚으며(끝 표본에서 거꾸로 — 이어 붙는 자리에 계단이 없다) MAX_CONCEAL 프레임(60ms)에 걸쳐 0 까지 줄인다
@@ -342,6 +374,13 @@
     }
     void stop() { open_ = false; }
     bool open() const { return open_.load(); }
+    // PL3c-D5 지터 버퍼의 규칙을 바꾼다(시리얼 `jb` — 통화 중에도 된다, 통화가 끝나도 남는다)
+    void set_policy(const jitterbuf::Policy &p)
+    {
+        lock_.lock();
+        jb_.set_policy(p);
+        lock_.unlock();
+    }
     uint32_t under() const { return under_.load(); }   // 굶음 횟수(잠금 없이 읽는다 — 출력 태스크용. stats() 는 지터 버퍼 잠금을 잡는다)
 
     void push(const uint8_t *frame, size_t len)     // [순번 2][Opus]
```

- [x] **Step 4: 통과를 확인한다**

Run: 맥 시험(`T="voice_tune"`) → `voice_core: 158 ok, 0 failed`, 나머지 그대로. 이어서 `/tmp/p3q/t_voice_core --info | sed -n '/Phase 3c: jitter rule/,$p'` → "지연 예산"의 시뮬레이션 표와 같은 숫자(아침 망 `jb 0` 평균 263, `jb 4` 134, `jb 5` 151).
(사본 빌드: `0x288770`, 우리 코드 경고 0.)

- [x] **Step 5: 커밋**

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/tools/test_voice_core.cpp firmware/tab5_phone/main/voice_core.h && git commit -m "feat(phone): voice_core — 시험음 주파수·probe 출처·도착 틈·마이크 넘침 셈·지터 규칙 건네기 + 아침 망 시뮬레이션 (PL3c-D4·D5)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 4: `tx_dyn.h` — 보내는 소리의 평탄화·고음 올리기·남은 메아리 누르기 (시험 먼저)

> **실행됨(2026-10-05, 구현 에이전트 — 보고 `chunk1-impl-report.md`)**: 커밋 `9de0596` — 구현자가 커밋하기 전에 자동 동기화(`sync: 2026-10-05 09:35`)가 두 파일을 먼저 담았다(내용은 같다). 실패 → 통과의 문구가 아래 Expected 와 같았다.

**Files:**
- Create: `firmware/tab5_phone/main/tx_dyn.h`, `firmware/tab5_phone/tools/test_tx_dyn.cpp`

**Interfaces:**
- Consumes: `pcm.h` 의 `pcm::gain_q8`·`pcm::sat16`·`pcm::isqrt64`(3b 그대로).
- Produces(이름공간 `txdyn`): `BLOCK = 256`, `TARGET_PEAK = 16000`, `KNEE = 20000`, `CEIL = 32767`, `FAR_ON = 150`, `struct Params { int base_q8; int lvl_max; int res_db; int tilt; }`, `int16_t soft_clip(int32_t)`, `class Dyn { void reset(); void process(int16_t *blk, uint32_t ref_rms, const Params &p); int gain_q8() const; uint32_t suppressed() const; uint32_t far_blocks() const; uint32_t speech_blocks() const; }`. `lvl_max`·`res_db`·`tilt` 가 모두 0 이면 `process` 는 `pcm::gain_q8(blk, BLOCK, base_q8)` 와 표본까지 같다.

- [x] **Step 1: 시험을 먼저 넣는다**

```bash
cd ~/tab5-toy-phone && git apply $P/T4-01-tx-dyn-tests.patch
```
```diff
--- /dev/null
+++ b/firmware/tab5_phone/tools/test_tx_dyn.cpp
@@ -0,0 +1,257 @@
+// test_tx_dyn.cpp — tx_dyn.h 단위 시험:
+//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_tx_dyn.cpp -o /tmp/t_tx_dyn && /tmp/t_tx_dyn
+//   /tmp/t_tx_dyn --info   → 시험 뒤에 크기 숫자(계획서 "먹먹함·작음" 표의 "맥 시험" 칸)를 찍는다
+// 계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D7, Review Focus 1(이득을 키웠더니 넘치거나 하울링)·4(손잡이가 기본 동작을 바꿔 놓음):
+//   손잡이가 꺼져 있으면 3b 와 표본 하나까지 같다 / 넘치지 않는다 / 작은 말은 키우고 큰 말은 누른다 / 소리가 없을 때 잡음을 끌어올리지 않는다 /
+//   상대가 말하는 동안에는 고정 이득을 넘지 않는다 / 상대 소리만 날 때 남은 메아리를 누른다 / 동시에 말하면 누르지 않는다 / 고음 올리기의 크기.
+#include "tx_dyn.h"
+#include <algorithm>
+#include <cmath>
+#include <cstdio>
+#include <cstring>
+#include <vector>
+using namespace txdyn;
+static int ok = 0, bad = 0;
+#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)
+
+typedef std::vector<int16_t> Pcm;
+static const double PI = 3.14159265358979323846;
+static const int RATE = 16000;
+static uint32_t g_seed = 7;
+static uint32_t rnd(uint32_t m) { g_seed = g_seed * 1664525u + 1013904223u; return (g_seed >> 8) % m; }
+static double rms(const Pcm &v, size_t from = 0, size_t to = SIZE_MAX) { double s = 0; size_t n = 0; for (size_t i = from; i < v.size() && i < to; ++i) { s += (double)v[i] * v[i]; ++n; } return n ? std::sqrt(s / n) : 0; }
+static int peak(const Pcm &v, size_t from = 0, size_t to = SIZE_MAX) { int m = 0; for (size_t i = from; i < v.size() && i < to; ++i) m = std::max(m, std::abs((int)v[i])); return m; }
+static double db(double a, double b) { return 20 * std::log10((a + 1e-9) / (b + 1e-9)); }
+
+// 말소리 비슷한 것: 0.2초 소리(300Hz + 1200Hz, 올림 코사인 모양) + 0.1초 쉼. peak = 소리의 꼭대기
+static int16_t speech_at(long i, int peak_amp)
+{
+    const long k = i % (RATE * 3 / 10);
+    if (k >= RATE / 5) return 0;
+    const double env = 0.5 - 0.5 * std::cos(2 * PI * k / (RATE / 5));
+    return (int16_t)std::lround(peak_amp * env * (0.6 * std::sin(2 * PI * 300 * i / RATE) + 0.4 * std::sin(2 * PI * 1200 * i / RATE)));
+}
+static int16_t noise_at(int amp) { return (int16_t)((int)rnd((uint32_t)(2 * amp + 1)) - amp); }
+static int16_t tone_at(long i, int amp, int hz) { return (int16_t)std::lround(amp * std::sin(2 * PI * hz * i / RATE)); }
+
+// 한 방향을 seconds 초 돌린다. near(i) = 에코 제거 뒤의 내 쪽 소리, ref(block) = 그 묶음의 되먹임 칸 세기. 돌려주는 것 = 나간 소리
+template <class Near, class Ref>
+static Pcm run(Dyn &d, const Params &p, double seconds, Near near, Ref ref, std::vector<int> *gains = nullptr)
+{
+    Pcm out; const long blocks = (long)(seconds * RATE / BLOCK);
+    for (long b = 0; b < blocks; ++b) {
+        int16_t blk[BLOCK];
+        for (int i = 0; i < BLOCK; ++i) blk[i] = near(b * BLOCK + i);
+        d.process(blk, ref(b), p);
+        if (gains) gains->push_back(d.gain_q8());
+        out.insert(out.end(), blk, blk + BLOCK);
+    }
+    return out;
+}
+static const Params OFF = {1024, 0, 0, 0};
+
+int main(int argc, char **argv)
+{
+    const bool info = argc > 1 && !strcmp(argv[1], "--info");
+
+    // ── 손잡이가 꺼져 있으면 3b 그대로 ─────────────────────────────────────
+    {
+        g_seed = 11; Dyn d; bool same = true;
+        for (int t = 0; t < 4000 && same; ++t) {
+            int16_t a[BLOCK], b[BLOCK]; const int amp = t % 7 == 0 ? 32767 : 1 + (int)rnd(12000);
+            for (int i = 0; i < BLOCK; ++i) a[i] = b[i] = (int16_t)(t % 11 == 0 ? (i % 2 ? 32767 : -32768) : noise_at(amp));
+            const Params p = {256 * (1 + (int)rnd(4)), 0, 0, 0};
+            d.process(a, rnd(6000), p); pcm::gain_q8(b, BLOCK, p.base_q8);
+            if (memcmp(a, b, sizeof a)) same = false;
+        }
+        CHECK("off (lvl 0, res 0): bit for bit the fixed gain of 3b, clipping included, whatever the loopback level", same && d.gain_q8() == 0);
+        Dyn e; int16_t a[BLOCK], b[BLOCK]; bool same2 = true;
+        for (int t = 0; t < 2000 && same2; ++t) {
+            for (int i = 0; i < BLOCK; ++i) a[i] = b[i] = speech_at((long)t * BLOCK + i, 3000);
+            const Params p = {1024, 0, 12, 0};
+            e.process(a, 0, p); pcm::gain_q8(b, BLOCK, 1024);
+            if (memcmp(a, b, sizeof a)) same2 = false;
+        }
+        CHECK("res on but the far end silent: still bit for bit the fixed gain (nothing is pressed, hard clip as before)", same2 && e.suppressed() == 0);
+    }
+
+    // ── 넘치지 않는다 ─────────────────────────────────────────────────────
+    {
+        bool mono = true, bounded = true, smooth = true; int16_t prev = soft_clip(-200000);
+        for (int32_t v = -200000; v <= 200000; ++v) {
+            const int16_t y = soft_clip(v);
+            if (y < prev) mono = false;
+            if ((v > 0 && y <= 0) || (v < 0 && y >= 0) || y > CEIL || y < -CEIL) bounded = false;
+            if (y - prev > 1) smooth = false;
+            prev = y;
+        }
+        CHECK("soft clip: untouched up to the knee (20000), never beyond +-32767, never changes sign", bounded && soft_clip(KNEE) == KNEE && soft_clip(-KNEE) == -KNEE && soft_clip(12345) == 12345 && soft_clip(0) == 0);
+        CHECK("soft clip: monotonic and with no step anywhere (slope <= 1) — it squeezes, it does not cut", mono && smooth && soft_clip(1 << 30) <= CEIL && soft_clip(60000) > 28000);
+        g_seed = 5; Dyn d; bool safe = true; long n = 0; int32_t before = 0;
+        for (int t = 0; t < 40000 && safe; ++t) {
+            int16_t x[BLOCK], y[BLOCK]; const int kind = (int)rnd(5); const int amp = kind == 0 ? 32767 : kind == 1 ? 20 : 1 + (int)rnd(32767);
+            for (int i = 0; i < BLOCK; ++i) x[i] = y[i] = kind == 4 ? (int16_t)(i % 2 ? 32767 : -32768) : noise_at(amp);
+            const Params p = {256 * (1 + (int)rnd(4)), rnd(3) ? 6 + (int)rnd(11) : 0, 6 * (int)rnd(4), (int)rnd(3)};
+            d.process(y, rnd(4) ? rnd(8000) : 0, p);
+            for (int i = 0; i < BLOCK; ++i, ++n) {
+                const int32_t f = x[i] + (x[i] - before) * tilt_a_q8(p.tilt) / 256;       // 고음 올리기를 거친 값(넓은 셈) — 그 부호가 나간 소리의 부호여야 한다
+                before = x[i];
+                if ((f > 0 && y[i] < 0) || (f < 0 && y[i] > 0)) safe = false;           // 돌면 부호가 뒤집힌다
+            }
+            if (rnd(500) == 0) { d.reset(); before = 0; }
+        }
+        CHECK("10 million random samples with every knob setting: the output never wraps around (the sign never flips)", safe && n > 10000000);
+    }
+
+    // ── 작은 말은 키운다 ───────────────────────────────────────────────────
+    double quiet_gain_db = 0, loud_out_peak = 0;
+    {
+        Dyn d, legacy; const Params lvl = {1024, 16, 0, 0};
+        std::vector<int> g;
+        const Pcm in = run(legacy, {256, 0, 0, 0}, 6, [](long i) { return speech_at(i, 600); }, [](long) { return 0u; });
+        const Pcm a = run(d, lvl, 6, [](long i) { return speech_at(i, 600); }, [](long) { return 0u; }, &g);
+        Dyn l2; const Pcm b = run(l2, OFF, 6, [](long i) { return speech_at(i, 600); }, [](long) { return 0u; });
+        const size_t from = 4 * RATE;                                                    // 4초 뒤(자리 잡은 뒤)
+        quiet_gain_db = db(rms(a, from), rms(b, from));
+        CHECK("lvl 16, quiet speech (peak 600 after the echo canceller): after 4 s it goes out at least 9 dB louder than the fixed x4", quiet_gain_db >= 9.0);
+        CHECK("... and its peaks sit near the target (-6 dBFS), nowhere near clipping", peak(a, from) >= 7000 && peak(a, from) <= KNEE);
+        CHECK("... the gain starts at the fixed gain and never exceeds the knob (x16)", g.front() <= 1024 + 1024 / 4 + 1 && *std::max_element(g.begin(), g.end()) <= 16 * 256);
+        bool slow = true; for (size_t i = 1; i < g.size(); ++i) if (g[i] > g[i - 1] + g[i - 1] / 4 + 1) slow = false;
+        CHECK("... it rises by at most a quarter per block (no jump in loudness)", slow);
+        Dyn d6; const Pcm c = run(d6, {1024, 6, 0, 0}, 6, [](long i) { return speech_at(i, 600); }, [](long) { return 0u; });
+        CHECK("lvl 6: the same speech is held to x6 (the knob is the ceiling)", d6.gain_q8() <= 6 * 256 && db(rms(c, from), rms(b, from)) < 4.0 && db(rms(c, from), rms(b, from)) > 2.5);
+        (void)in;
+    }
+    // ── 큰 말은 누른다 ─────────────────────────────────────────────────────
+    {
+        Dyn d, l; const Params lvl = {1024, 16, 0, 0};
+        const Pcm a = run(d, lvl, 3, [](long i) { return speech_at(i, 12000); }, [](long) { return 0u; });
+        const Pcm b = run(l, OFF, 3, [](long i) { return speech_at(i, 12000); }, [](long) { return 0u; });
+        long cut_fixed = 0, cut_lvl = 0; for (size_t i = RATE; i < a.size(); ++i) { if (std::abs((int)b[i]) >= 32767) ++cut_fixed; if (std::abs((int)a[i]) >= 32000) ++cut_lvl; }
+        loud_out_peak = peak(a, RATE);
+        CHECK("lvl on, loud speech (peak 12000): the fixed x4 cuts hundreds of samples at full scale, the leveller none", cut_fixed > 500 && cut_lvl == 0);
+        CHECK("... its peaks come down to about the target", loud_out_peak <= KNEE + 2500 && loud_out_peak >= 12000 && d.gain_q8() < 512);
+        Dyn s; Pcm first;
+        for (int b2 = 0; b2 < 3; ++b2) { int16_t blk[BLOCK]; for (int i = 0; i < BLOCK; ++i) blk[i] = tone_at(b2 * BLOCK + i, 30000, 500); s.process(blk, 0, lvl); first.insert(first.end(), blk, blk + BLOCK); }
+        CHECK("a sudden shout: squeezed under full scale from the first sample, and the gain is down to x1 within one block (16 ms)", peak(first) <= CEIL && s.gain_q8() == 256 && peak(first, 2 * BLOCK) <= 30000);
+    }
+    // ── 고음 올리기(tilt) ─────────────────────────────────────────────────
+    double tilt_db[3][4] = {};
+    {
+        const int hz[4] = {500, 1000, 2000, 4000};
+        for (int t = 0; t <= 2; ++t) for (int k = 0; k < 4; ++k) {
+            Dyn d; const Params p = {256, 0, 0, t};
+            const Pcm y = run(d, p, 1, [&](long i) { return tone_at(i, 1000, hz[k]); }, [](long) { return 0u; });
+            Dyn e; const Pcm x = run(e, {256, 0, 0, 0}, 1, [&](long i) { return tone_at(i, 1000, hz[k]); }, [](long) { return 0u; });
+            tilt_db[t][k] = db(rms(y, 1000), rms(x, 1000));
+        }
+        CHECK("tilt 0: nothing changes at any frequency", std::fabs(tilt_db[0][0]) < 0.01 && std::fabs(tilt_db[0][3]) < 0.01);
+        CHECK("tilt 1: a gentle treble lift — about +0.5 dB at 1 kHz, +1.6 dB at 2 kHz, +4 dB at 4 kHz, under +0.2 dB at 500 Hz",
+              tilt_db[1][0] < 0.2 && std::fabs(tilt_db[1][1] - 0.5) < 0.3 && std::fabs(tilt_db[1][2] - 1.6) < 0.4 && std::fabs(tilt_db[1][3] - 4.0) < 0.4);
+        CHECK("tilt 2: about +1.1 dB at 1 kHz, +3.4 dB at 2 kHz, +7 dB at 4 kHz, under +0.4 dB at 500 Hz",
+              tilt_db[2][0] < 0.4 && std::fabs(tilt_db[2][1] - 1.1) < 0.3 && std::fabs(tilt_db[2][2] - 3.4) < 0.4 && std::fabs(tilt_db[2][3] - 7.0) < 0.4);
+        Dyn d; Pcm sw;
+        for (int b = 0; b < 40; ++b) { int16_t blk[BLOCK]; for (int i = 0; i < BLOCK; ++i) blk[i] = tone_at((long)b * BLOCK + i, 3000, 700); const Params p = {256, 0, 0, b < 20 ? 0 : 2}; d.process(blk, 0, p); sw.insert(sw.end(), blk, blk + BLOCK); }
+        int jump = 0; for (size_t i = 1; i < sw.size(); ++i) jump = std::max(jump, std::abs(sw[i] - sw[i - 1]));
+        CHECK("tilt switched on in mid-stream: the step is at most twice the tone's own slope (the filter kept following the signal while it was off — not a jump of the full amplitude)",
+              jump <= 2 * (int)(3000 * 2 * 3.1416 * 700 / RATE + 1) + 16 && jump < 3000 / 1);
+        int16_t top[BLOCK]; for (int i = 0; i < BLOCK; ++i) top[i] = (int16_t)(i % 2 ? 32767 : -32768);
+        Dyn f; const Params p = {1024, 0, 0, 2}; f.process(top, 0, p);
+        bool ok_sign = true; for (int i = 0; i < BLOCK; ++i) if ((i % 2 ? top[i] <= 0 : top[i] >= 0)) ok_sign = false;
+        CHECK("tilt on a full-scale input: clipped, never wrapped", ok_sign);
+    }
+    // ── 소리가 없을 때 잡음을 끌어올리지 않는다 ───────────────────────────
+    {
+        g_seed = 3; Dyn d, l; const Params lvl = {1024, 16, 0, 0};
+        const Pcm a = run(d, lvl, 5, [](long) { return noise_at(30); }, [](long) { return 0u; });
+        g_seed = 3; const Pcm b = run(l, OFF, 5, [](long) { return noise_at(30); }, [](long) { return 0u; });
+        CHECK("lvl 16, room noise only: goes out exactly as loud as with the fixed gain — never louder", a == b && d.gain_q8() == 1024);
+        // 말하다 그친다: 0.2초 뒤에는 다시 고정 이득
+        Dyn e; std::vector<int> g;
+        g_seed = 9; run(e, lvl, 5, [](long i) { return (int16_t)(speech_at(i, 600) + noise_at(10)); }, [](long) { return 0u; }, &g);
+        const int talking = e.gain_q8();
+        g.clear(); run(e, lvl, 1, [](long) { return noise_at(10); }, [](long) { return 0u; }, &g);
+        CHECK("after speech stops the noise is back at the fixed gain within 0.25 s (the high gain is not left on the room noise)", talking > 3000 && g[15] == 1024 && g.back() == 1024);
+        g.clear(); run(e, lvl, 0.2, [](long i) { return speech_at(i + 800, 600); }, [](long) { return 0u; }, &g);
+        CHECK("... and when speech starts again the remembered gain is back within 0.15 s", g[9] >= talking * 9 / 10);
+    }
+    {
+        // 고르게 이어지는 소리(기기 시험의 probe 시험음, 선풍기): 처음 몇 초는 말소리로 치고(시험음으로 평탄화를 잴 수 있다), 그 뒤에는 방의 잡음으로 배워 고정 이득으로 돌아간다
+        Dyn d; const Params lvl = {1024, 12, 0, 0}; std::vector<int> g;
+        run(d, lvl, 12, [](long i) { return tone_at(i, 500, 1000); }, [](long) { return 0u; }, &g);
+        const int at2s = g[2 * RATE / BLOCK], at4s = g[4 * RATE / BLOCK], at12s = g.back();
+        CHECK("a steady tone (amplitude 500, like the 'probe' test tone): levelled to the knob's ceiling (x12) within 2 s and still there at 4 s", at2s == 12 * 256 && at4s == 12 * 256);
+        CHECK("... and learned as background noise within 12 s — a steady hum is not boosted for ever", at12s == 1024);
+    }
+    // ── 상대가 말하는 동안: 고정 이득을 넘지 않는다, 남은 메아리는 누른다 ──
+    double pressed_db = 0, dt_loss_db = 0;
+    {
+        const Params both = {1024, 16, 12, 0};
+        Dyn d, l;
+        // 상대 소리만: 되먹임 칸 1400, 남은 메아리 = 그 4%(1kHz) — 2026-10-05 실측과 같은 크기
+        auto echo = [](long i) { return tone_at(i, 80, 1000); };
+        const Pcm a = run(d, both, 3, echo, [](long) { return 1400u; });
+        const Pcm b = run(l, OFF, 3, echo, [](long) { return 1400u; });
+        pressed_db = db(rms(a, RATE), rms(b, RATE));
+        CHECK("far end talking, I am silent (res 12): what goes back is 12 dB below the fixed gain", pressed_db < -11.5 && pressed_db > -12.5 && d.suppressed() > 100);
+        CHECK("... and the leveller never treats the leftover echo as speech (gain stays at or below the fixed gain)", d.gain_q8() <= 1024);
+        // 동시에 말하기: 상대 소리가 나는 중에 내 말(꼭대기 1500)
+        Dyn e, l2; run(e, both, 2, echo, [](long) { return 1400u; });                    // 먼저 2초 상대만(누르는 중)
+        std::vector<int> g;
+        const Pcm c = run(e, both, 3, [&](long i) { return (int16_t)(speech_at(i, 1500) + echo(i)); }, [](long) { return 1400u; }, &g);
+        const Pcm f = run(l2, OFF, 3, [&](long i) { return (int16_t)(speech_at(i, 1500) + echo(i)); }, [](long) { return 1400u; });
+        // 말하는 구간(0.2초 소리)의 세기만 비교한다
+        double sa = 0, sb = 0; for (size_t i = 0; i < c.size(); ++i) if ((long)i % (RATE * 3 / 10) > 800 && (long)i % (RATE * 3 / 10) < 2400) { sa += (double)c[i] * c[i]; sb += (double)f[i] * f[i]; }
+        dt_loss_db = 10 * std::log10((sa + 1) / (sb + 1));
+        CHECK("double talk: my speech over the far end's goes out within 1 dB of the fixed gain (not pressed)", dt_loss_db > -1.0 && dt_loss_db < 0.5);
+        CHECK("double talk: the press is released within one block (16 ms) of my speech starting", g[1] == 1024 || g[2] == 1024);
+        CHECK("double talk: the gain never goes above the fixed gain while the far end is talking (the loop gain is never higher than in 3b)", *std::max_element(g.begin(), g.end()) <= 1024);
+        // 상대가 그친 뒤: 0.2초(울림) 지나면 누르기를 푼다
+        g.clear(); g_seed = 2; run(e, both, 1, [](long) { return noise_at(10); }, [](long) { return 0u; }, &g);
+        CHECK("far end stops: the press ends after the 0.2 s reverb hold", g[5] < 1024 && g[30] == 1024);
+    }
+    {
+        // 에코 제거가 아직 자리 잡지 못했다(남은 메아리가 되먹임 칸의 2배): 누르지 않고, 이득도 올리지 않는다 = 3b 와 같은 소리
+        const Params both = {1024, 16, 18, 0};
+        Dyn d, l; auto echo = [](long i) { return tone_at(i, 4000, 700); };
+        const Pcm a = run(d, both, 2, echo, [](long) { return 1400u; });
+        const Pcm b = run(l, OFF, 2, echo, [](long) { return 1400u; });
+        CHECK("echo canceller not converged (echo 2x the loopback): not pressed, not boosted — it goes out no louder than with the fixed gain", rms(a, RATE) <= rms(b, RATE) + 1 && d.gain_q8() <= 1024);
+    }
+    {
+        // 상대 쪽 잡음이 문턱 아래(되먹임 칸 60): 상대 소리로 치지 않는다 → 내 말은 평소처럼 키운다
+        Dyn d; const Params lvl = {1024, 16, 12, 0};
+        run(d, lvl, 5, [](long i) { return speech_at(i, 600); }, [](long) { return 60u; });
+        CHECK("a quiet hiss from the far end (loopback 60 < 150) does not count as the far end talking: my speech is still levelled", d.gain_q8() > 3000 && d.far_blocks() == 0 && d.suppressed() == 0);
+        Dyn e; run(e, lvl, 5, [](long i) { return speech_at(i, 600); }, [](long) { return 2000u; });
+        CHECK("far end loud the whole time: my speech stays at the fixed gain (safe side), nothing above it", e.gain_q8() <= 1024 && e.far_blocks() > 300);
+    }
+    {
+        Dyn d; const Params lvl = {1024, 16, 12, 0};
+        run(d, lvl, 3, [](long i) { return speech_at(i, 600); }, [](long b) { return b % 40 < 20 ? 1400u : 0u; });
+        const uint32_t s = d.suppressed(), f = d.far_blocks(), sp = d.speech_blocks();
+        d.reset();
+        CHECK("reset (a new call): gain back to the start, counters cleared", s + f + sp > 0 && d.suppressed() == 0 && d.far_blocks() == 0 && d.speech_blocks() == 0 && d.gain_q8() == 0);
+        g_seed = 77; bool sane = true;
+        for (int t = 0; t < 300000 && sane; ++t) {
+            int16_t x[BLOCK]; const int amp = 1 + (int)rnd(rnd(3) ? 400 : 32000);
+            for (int i = 0; i < BLOCK; ++i) x[i] = noise_at(amp);
+            const Params p = {256 * (1 + (int)rnd(4)), 6 + (int)rnd(11), 6 * (int)rnd(4), (int)rnd(3)};
+            d.process(x, rnd(3) ? rnd(6000) : 0, p);
+            if (d.gain_q8() < 16 || d.gain_q8() > 16 * 256) sane = false;
+        }
+        CHECK("300 thousand random blocks: the gain always stays between the deepest press and x16", sane);
+    }
+
+    if (info) {
+        printf("\n-- 크기(맥 시험, 계획서 표) --\n");
+        printf("작은 말(에코 제거 뒤 꼭대기 600) · lvl 16: 고정 4배보다 %+.1f dB\n", quiet_gain_db);
+        printf("큰 말(꼭대기 12000) · lvl 16: 나가는 꼭대기 %.0f (고정 4배는 32767 에서 잘림)\n", loud_out_peak);
+        printf("상대 소리만(되먹임 1400, 남은 메아리 4%%) · res 12: 고정 4배보다 %+.1f dB\n", pressed_db);
+        printf("동시에 말하기(내 말 꼭대기 1500) · lvl 16 · res 12: 고정 4배보다 %+.2f dB\n", dt_loss_db);
+        printf("고음 올리기(500 / 1k / 2k / 4kHz): tilt 1 = %+.1f / %+.1f / %+.1f / %+.1f dB, tilt 2 = %+.1f / %+.1f / %+.1f / %+.1f dB\n",
+               tilt_db[1][0], tilt_db[1][1], tilt_db[1][2], tilt_db[1][3], tilt_db[2][0], tilt_db[2][1], tilt_db[2][2], tilt_db[2][3]);
+    }
+    printf("\n%d ok, %d failed\n", ok, bad);
+    return bad ? 1 : 0;
+}
```

- [x] **Step 2: 실패를 확인한다**

Run: `cd ~/tab5-toy-phone/firmware/tab5_phone/tools && clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_tx_dyn.cpp -o /tmp/p3q/t_tx_dyn 2>&1 | grep -m1 "error:"`
Expected: `test_tx_dyn.cpp:7:10: fatal error: 'tx_dyn.h' file not found`.

- [x] **Step 3: 구현**

```bash
cd ~/tab5-toy-phone && git apply $P/T4-02-tx-dyn.patch
```
```diff
--- /dev/null
+++ b/firmware/tab5_phone/main/tx_dyn.h
@@ -0,0 +1,163 @@
+// tx_dyn.h — 보내는 소리의 크기 다듬기: 말소리 평탄화(작은 말은 키우고 큰 말은 누른다) + 남은 메아리 누르기. 하드웨어 무관(맥 시험: tools/test_tx_dyn.cpp).
+//
+// 계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md 의 결정 PL3c-D7. 에코 제거(esp-sr)를 거친 16ms 묶음을 받아 제자리에서 고친다.
+//   왜: 2026-10-05 아침 실측 — 말소리가 에코 제거 뒤 RMS 70~279(−53~−41dBFS)로 작고, 3b 의 고정 4배로는 −41~−29dBFS 다(통화에 알맞은 크기는 −26~−20dBFS).
+//       고정 이득을 더 올리면 큰 소리가 찢어지고 메아리도 그만큼 커진다. 1번 기기가 상대에게 돌려보내는 메아리는 −10.9dB 로 크다(2번은 −20.5dB).
+//   세 손잡이(셋 다 0 이면 3b 의 "고정 이득 × 넘치면 자르기"와 표본 하나까지 같다 — 시험이 고정한다):
+//     lvl  0 = 끔 / 6~16 = 말소리에 거는 이득의 상한(배). 말소리의 꼭대기를 TARGET_PEAK(−6dBFS)로 맞춘다. 이득은 1배 ~ 상한 사이에서 천천히 오르고 빨리 내린다.
+//     tilt 0 = 끔 / 1·2 = 고음을 올린다(1차 필터 y = x + a·(x − 앞 표본), a = 0.5·1.0 → 1kHz +0.5·+1.1dB, 2kHz +1.6·+3.4dB, 4kHz +4.0·+7.0dB).
+//          "먹먹하다"에 대한 손잡이다: 작은 스피커·먼 마이크·에코 제거가 깎는 고음을 보내는 쪽에서 미리 올린다. 값: 고음 쪽 잡음도 같이 오른다.
+//     res  0 = 끔 / 6·12·18 = 상대 소리만 나고 내 말이 없을 때 보내는 소리를 이만큼(dB) 누른다.
+//   지키는 것(시험이 하나씩 고정한다):
+//     · 넘치지 않는다 — lvl 이 켜져 있으면 KNEE 위는 부드럽게 눌러 ±32767 안에 둔다(돌지 않고, 딱 잘리지도 않는다).
+//     · 소리가 없을 때 잡음을 끌어올리지 않는다 — 말소리가 아닌 묶음의 이득은 고정 이득(base)을 넘지 않는다.
+//     · 상대가 말하는 동안에는 이득이 고정 이득을 넘지 않는다 → 스피커 → 마이크 고리의 이득이 3b 보다 커지는 일이 없다(하울링 여유는 그대로이거나 res 만큼 는다).
+//     · 동시에 말하기 — 상대 소리가 나는 중에도 내 말이 "남은 메아리의 어림 × 2.5"보다 크면 누르지 않는다. 누르던 중이면 한 묶음(16ms) 안에 푼다.
+//   비용: 판단이 한 묶음(16ms) 늦다(말의 첫 16ms 는 앞 묶음의 이득으로 나간다). 상대 소리가 나는 동안의 작은 말소리(남은 메아리 어림의 2.5배 아래)는 res 만큼 눌린다.
+//         에코 제거가 아직 자리 잡지 못한 동안(통화 첫 1~2초, 기기를 옮긴 직후)에는 남은 메아리가 어림보다 커서 "내 말"로 보인다 → 누르지 않는다(3b 와 같은 소리).
+//   되돌리기: lvl 0 · tilt 0 · res 0.
+#pragma once
+#include <cstdint>
+
+#include "pcm.h"
+
+namespace txdyn {
+
+constexpr int BLOCK = 256;                      // 한 묶음(16ms @16kHz) — voicecore::BLOCK 과 같다(voice.cpp 의 static_assert)
+constexpr int TARGET_PEAK = 16000;              // 말소리 꼭대기의 목표(−6dBFS). 말소리의 RMS 는 꼭대기의 1/4~1/5 → 약 −19dBFS
+constexpr int KNEE = 20000, CEIL = 32767;       // KNEE 위는 부드럽게 눌러 CEIL 을 넘지 않는다
+constexpr uint32_t FAR_ON = 150;                // 되먹임 칸의 세기가 이 이상이면 "상대 소리가 나고 있다"(되먹임 칸 = 코덱 출력의 0.42배 — 시험음 2000 이 590)
+constexpr int FAR_HOLD = 12;                    // 상대 소리가 그친 뒤 이만큼(약 0.2초)은 방의 울림이 남아 있다고 본다
+constexpr int HANG = 12;                        // 말이 끊긴 뒤 이만큼(약 0.2초)은 말소리 이득을 유지한다(낱말 끝이 잘리지 않게)
+constexpr uint32_t RHO_INIT = 4096, RHO_MAX = 16384;    // 남은 메아리 ÷ 되먹임 칸(Q16). 시작 0.0625(−24dB — 실측 0.037~0.063), 위 한계 0.25
+constexpr uint32_t DT_MARGIN_Q8 = 640;          // 내 말로 보려면 남은 메아리 어림의 2.5배
+constexpr uint32_t GATE_MIN = 40, NOISE_INIT = 20, NOISE_MAX = 400;   // 말소리로 치는 세기의 바닥 / 바닥 잡음 어림의 처음 값·위 한계(에코 제거 뒤의 RMS)
+
+struct Params {
+    int base_q8;        // 고정 이득(txgain × 256)
+    int lvl_max;        // 0 = 평탄화 끔, 6~16 = 말소리 이득의 상한(배)
+    int res_db;         // 0 = 끔, 6·12·18
+    int tilt;           // 0 = 끔, 1·2 = 고음 올리기
+};
+constexpr int tilt_a_q8(int tilt) { return tilt <= 0 ? 0 : tilt == 1 ? 128 : 256; }       // 필터의 a(Q8): 0, 0.5, 1.0
+
+// |v| 가 KNEE 를 넘으면 부드럽게 눌러 CEIL 아래에 둔다(기울기가 KNEE 에서 1 로 이어진다 — 꺾이는 자리에 계단이 없다). 부호는 그대로.
+inline int16_t soft_clip(int32_t v)
+{
+    const int32_t a = v < 0 ? -v : v;
+    if (a <= KNEE) return (int16_t)v;
+    const int64_t room = CEIL - KNEE, d = (int64_t)a - KNEE;
+    const int32_t y = KNEE + (int32_t)(room * d / (d + room));
+    return (int16_t)(v < 0 ? -y : y);
+}
+
+class Dyn {
+public:
+    Dyn() { reset(); }
+    void reset()                                 // 통화가 열릴 때
+    {
+        g_q8_ = 0; env_ = 0; rho_ = RHO_INIT; noise_q4_ = NOISE_INIT * 16; ref_env_ = 0; prev_x_ = 0;
+        far_hold_ = 0; hang_ = 0; suppressed_ = 0; far_blocks_ = 0; speech_blocks_ = 0;
+    }
+
+    // blk = 에코 제거를 거친 한 묶음(BLOCK 표본) → 제자리에서 보낼 소리로. ref_rms = 같은 묶음의 되먹임 칸 세기.
+    void process(int16_t *blk, uint32_t ref_rms, const Params &p)
+    {
+        if (const int a = tilt_a_q8(p.tilt)) {              // 고음 올리기: 이득을 걸기 전에(넘치면 여기서는 그냥 자른다 — 에코 제거 뒤의 소리는 작아서 닿지 않는다)
+            for (int i = 0; i < BLOCK; ++i) { const int32_t x = blk[i]; blk[i] = pcm::sat16(x + (x - prev_x_) * a / 256); prev_x_ = x; }
+        } else {
+            prev_x_ = blk[BLOCK - 1];                       // 꺼져 있는 동안에도 앞 표본은 따라간다(켜는 순간 계단이 없다)
+        }
+        if (p.lvl_max == 0 && p.res_db == 0) {              // 평탄화·누르기가 꺼져 있다 = 3b 의 고정 이득
+            pcm::gain_q8(blk, BLOCK, p.base_q8);
+            g_q8_ = 0;                                      // 다시 켜면 처음부터(고정 이득에서 시작한다)
+            return;
+        }
+        const int32_t base = p.base_q8;
+        if (g_q8_ == 0) { g_q8_ = base; env_ = (uint32_t)((int64_t)TARGET_PEAK * 256 / base); }     // 처음: 고정 이득에서 시작
+        // ── 재기 ──
+        uint64_t sum = 0; int32_t pk = 0;
+        for (int i = 0; i < BLOCK; ++i) { const int32_t a = blk[i] < 0 ? -(int32_t)blk[i] : blk[i]; if (a > pk) pk = a; sum += (uint64_t)((int64_t)a * a); }
+        const uint32_t r = pcm::isqrt64(sum / BLOCK);
+        // ── 상대 소리가 나고 있나 ──
+        const bool far_now = ref_rms >= FAR_ON;
+        if (far_now) far_hold_ = FAR_HOLD; else if (far_hold_ > 0) --far_hold_;
+        const bool far = far_hold_ > 0;
+        ref_env_ = ref_rms > ref_env_ ? ref_rms : ref_env_ - ref_env_ / 8;          // 울림이 남는 동안의 어림(한 묶음에 1/8 씩 준다)
+        if (far) ++far_blocks_;
+        // ── 내 말이 있나 ──
+        bool near;
+        if (far) {
+            if (far_now) {                                                          // 남은 메아리의 비율: 내려갈 때는 바로, 올라갈 때는 천천히(겹쳐 말하는 동안 부풀지 않게)
+                const uint64_t q = ((uint64_t)r << 16) / ref_rms;
+                if (q < rho_) rho_ = (uint32_t)((rho_ + q) / 2);
+                else { rho_ += rho_ / 128 + 1; if (rho_ > RHO_MAX) rho_ = RHO_MAX; }
+            }
+            const uint64_t echo = ((uint64_t)rho_ * ref_env_) >> 16;                // 지금 남아 있을 메아리의 세기
+            const uint32_t noise = noise_q4_ >> 4;
+            uint64_t need = echo * DT_MARGIN_Q8 / 256;
+            if (need < 3ull * noise) need = 3ull * noise;
+            if (need < GATE_MIN) need = GATE_MIN;
+            near = r > need;
+        } else {
+            // 바닥 잡음의 어림(1/16 눈금): 내려갈 때는 바로(말 사이의 쉼에서 방의 잡음으로 내려온다), 올라갈 때는 아주 천천히 — 한 묶음에 1/256(1초에 1.28배).
+            //   고르게 이어지는 소리(선풍기·시험음)는 약 7초 뒤에 잡음으로 배운다. 그 전까지는 말소리로 친다(말에는 1초 안에 쉼이 있다).
+            const uint32_t r_q4 = r << 4;
+            if (r_q4 < noise_q4_) noise_q4_ = (noise_q4_ + r_q4) / 2;
+            else { noise_q4_ += noise_q4_ / 256 + 1; if (noise_q4_ > NOISE_MAX * 16) noise_q4_ = NOISE_MAX * 16; }
+            const uint32_t noise = noise_q4_ >> 4;
+            const uint32_t gate = 3 * noise > GATE_MIN ? 3 * noise : GATE_MIN;
+            near = r > gate;
+        }
+        if (near) { hang_ = HANG; ++speech_blocks_; } else if (hang_ > 0) --hang_;
+        // ── 말소리의 꼭대기를 따라간다(상대 소리가 없을 때만 — 메아리를 말소리로 배우지 않는다) ──
+        if (near && !far) env_ = (uint32_t)pk > env_ ? (uint32_t)pk : env_ - env_ / 64;       // 말하는 동안 한 묶음에 1/64 씩 내려간다(약 0.7초에 반 — 이득은 천천히 오른다)
+        if (near && (uint32_t)pk > env_) env_ = (uint32_t)pk;                        // 큰 소리는 어느 때든 바로 따라간다(이득을 내리는 쪽)
+        if (env_ < 1) env_ = 1;
+        // ── 이 묶음의 이득 ──
+        int32_t speech_q8 = base;
+        if (p.lvl_max > 0) {
+            const int64_t want = (int64_t)TARGET_PEAK * 256 / env_;
+            const int32_t top = p.lvl_max * 256;
+            speech_q8 = want > top ? top : want < 256 ? 256 : (int32_t)want;
+        }
+        const int32_t quiet_q8 = speech_q8 < base ? speech_q8 : base;              // 말소리가 아닌 것·상대가 말하는 동안: 고정 이득을 넘지 않는다
+        int32_t target;
+        bool pressed = false;
+        if (far) {
+            target = quiet_q8;
+            if (!near && hang_ == 0 && p.res_db > 0) { target >>= p.res_db / 6; pressed = true; }     // 6dB = 반
+        } else {
+            target = hang_ > 0 ? speech_q8 : quiet_q8;
+        }
+        if (pressed) ++suppressed_;
+        // ── 옮겨 가기: 내릴 때는 이 묶음 안에, 올릴 때는 고정 이득까지는 이 묶음 안에(누르던 것을 푼다), 그 위로는 한 묶음에 1/4 씩 ──
+        int32_t next = target;
+        if (target > g_q8_) {
+            const int32_t fast_to = target < quiet_q8 ? target : quiet_q8;
+            int32_t up = g_q8_ < fast_to ? fast_to : g_q8_;
+            if (target > up) { up += up / 4 + 1; if (up > target) up = target; }
+            next = up;
+        }
+        const bool soft = p.lvl_max > 0;
+        for (int i = 0; i < BLOCK; ++i) {
+            const int32_t g = g_q8_ + (int32_t)((int64_t)(next - g_q8_) * (i + 1) / BLOCK);
+            const int64_t v = (int64_t)blk[i] * g / 256;
+            blk[i] = soft ? soft_clip(v > 0x3FFFFFFF ? 0x3FFFFFFF : v < -0x3FFFFFFF ? -0x3FFFFFFF : (int32_t)v) : pcm::sat16((int32_t)(v > 0x3FFFFFFF ? 0x3FFFFFFF : v < -0x3FFFFFFF ? -0x3FFFFFFF : v));
+        }
+        g_q8_ = next;
+    }
+
+    int gain_q8() const { return g_q8_; }                    // 지금 이득(Q8). 0 = 손잡이가 꺼져 있다
+    uint32_t suppressed() const { return suppressed_; }      // 남은 메아리를 누른 묶음 수(통화마다)
+    uint32_t far_blocks() const { return far_blocks_; }      // 상대 소리가 나던 묶음 수
+    uint32_t speech_blocks() const { return speech_blocks_; }
+
+private:
+    int32_t g_q8_, prev_x_;
+    uint32_t env_, rho_, noise_q4_, ref_env_;
+    int far_hold_, hang_;
+    uint32_t suppressed_, far_blocks_, speech_blocks_;
+};
+
+}  // namespace txdyn
```

- [x] **Step 4: 통과를 확인한다**

Run: 맥 시험(`T="voice_tune tx_dyn"`) → `tx_dyn: 34 ok, 0 failed`, 나머지 그대로. 이어서 `/tmp/p3q/t_tx_dyn --info | tail -8`
Expected(끝의 표):
```
작은 말(에코 제거 뒤 꼭대기 600) · lvl 16: 고정 4배보다 +12.0 dB
큰 말(꼭대기 12000) · lvl 16: 나가는 꼭대기 16476 (고정 4배는 32767 에서 잘림)
상대 소리만(되먹임 1400, 남은 메아리 4%) · res 12: 고정 4배보다 -12.0 dB
동시에 말하기(내 말 꼭대기 1500) · lvl 16 · res 12: 고정 4배보다 +0.00 dB
고음 올리기(500 / 1k / 2k / 4kHz): tilt 1 = +0.1 / +0.5 / +1.6 / +4.0 dB, tilt 2 = +0.3 / +1.2 / +3.4 / +7.0 dB
```
(사본 빌드: `0x288770` — 아직 아무도 쓰지 않는다.)

- [x] **Step 5: 커밋**

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/tools/test_tx_dyn.cpp firmware/tab5_phone/main/tx_dyn.h && git commit -m "feat(phone): tx_dyn.h — 보내는 소리 평탄화·고음 올리기·남은 메아리 누르기(셋 다 0 이면 3b 와 표본까지 같다), 시험 먼저 (PL3c-D7)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 5: 손잡이를 전화기에 잇는다 — 명령 9개, 새 줄 `TUNE` (부팅 값은 `5935ddd` 그대로)

> **실행됨(2026-10-05, 구현 에이전트 — 보고 `chunk1-impl-report.md`)**: 커밋 `d285c33`. 실패 → 통과의 문구가 아래 Expected 와 같았다.

**Files:**
- Modify: `firmware/tab5_phone/main/voice.h`·`voice.cpp`(손잡이·평탄화·`probe`·비트율·틈·넘침), `audio_out.h`·`audio_out.cpp`(실행 중 마이크 증폭), `relay_link.h`·`relay_link.cpp`(`TCP_NODELAY`), `net_wifi.h`·`net_wifi.cpp`(절전 상태 읽기), `phone_main.cpp`(명령·`TUNE`)
- Test: `firmware/tab5_phone/tools/test_phone_ctl.py`(추가)

**Interfaces:**
- Consumes: Task 2~4 의 `voicetune::*`, `jitterbuf::tuned_policy`·`legacy_policy`, `voicecore::uses_dyn`·`clipped`·`ArrivalGap`·`Src::PROBE`·`PROBE_AMP`·`TestTone::set_hz`·`Receiver::set_policy`, `txdyn::Dyn`·`Params`.
- Produces: `voice::set_kbps(int)`·`set_lvl(int)`·`set_tilt(int)`·`set_res(int)`·`set_jb(int)`·`set_vtone(int)`(범위 밖이면 false), `struct voice::Tune { int kbps, lvl, tilt, res_db, jb, vtone; int gain_q8; uint32_t supp, far; uint32_t rxgap_ms; uint32_t mic_clip; }`, `void voice::tune(Tune*)`; `audioout::set_mic_gain(int db)`(→ `VolResult`)·`mic_gain()`; `relaylink::set_nodelay(bool)`(→ 건 소켓 수, 잠금 실패 −1)·`nodelay()`·`nodelay_socks()`; `netwifi::ps()`. 시리얼: `tune`·`lvl`·`tilt`·`res`·`mic`·`opus`·`jb`·`nodelay`·`vtone`·`vsrc probe`. 줄 모양:
  `TUNE dflt=%d legacy=%d meas=%d txgain=%d vol=%d mic=%d kbps=%d lvl=%d tilt=%d res=%d jb=%d nodelay=%d vtone=%d ps=%d nd_socks=%d g_q8=%d supp=%u far=%u rxgap=%u mclip=%u`

- [x] **Step 1: 시험을 먼저 넣는다**

```bash
cd ~/tab5-toy-phone && git apply $P/T5-01-tune-tool-tests.patch
```
```diff
--- a/firmware/tab5_phone/tools/test_phone_ctl.py
+++ b/firmware/tab5_phone/tools/test_phone_ctl.py
@@ -1100,3 +1100,115 @@
     assert re.search(r"\bTX_STACK = 8 \* 1024;", src) and re.search(r"\bTX_STACK_CAPS = MALLOC_CAP_SPIRAM \| MALLOC_CAP_8BIT;", src)
     assert 'xTaskCreatePinnedToCoreWithCaps(tx_task, "v_tx", TX_STACK, nullptr, TX_PRIO, &s_tx_task, TX_CORE, TX_STACK_CAPS)' in src
     assert 'xTaskCreatePinnedToCoreWithCaps(rx_task, "v_rx", RX_STACK, nullptr, RX_PRIO, &s_rx_task, RX_CORE, RX_STACK_CAPS)' in src
+
+
+# ── Phase 3c: 통화 품질 손잡이(PL3c-D3) — 새 줄 TUNE, 손잡이 명령의 응답. STAT 줄은 바뀌지 않는다 ───────────────────────────────────────────
+VOICE_TUNE_H = Path(__file__).resolve().parent.parent / "main" / "voice_tune.h"
+VOICE_CORE_H = Path(__file__).resolve().parent.parent / "main" / "voice_core.h"
+TUNE3C_FORMAT = ("TUNE dflt=%d legacy=%d meas=%d txgain=%d vol=%d mic=%d kbps=%d lvl=%d tilt=%d res=%d jb=%d nodelay=%d vtone=%d ps=%d nd_socks=%d"
+                 " g_q8=%d supp=%u far=%u rxgap=%u mclip=%u")
+
+
+def firmware_tune_format():
+    """main/phone_main.cpp 의 printf("TUNE …") 형식 문자열(끝의 \\n 은 뗀다)."""
+    src = PHONE_MAIN_CPP.read_text(encoding="utf-8")
+    m = re.search(r'printf\("(TUNE (?:[^"\\]|\\.)*)\\n",', src)
+    assert m, "phone_main.cpp 에서 TUNE 형식을 찾지 못함"
+    return m.group(1)
+
+
+def tune_line(**over):
+    """펌웨어의 형식 문자열에 값을 채운 TUNE 줄. 값을 주지 않은 칸은 5935ddd 와 같은 설정으로 대기 중인 기기의 값."""
+    vals = {"dflt": 1, "legacy": 1, "meas": 1, "txgain": 4, "vol": 100, "mic": 24, "kbps": 24, "lvl": 0, "tilt": 0, "res": 0, "jb": 0, "nodelay": 0, "vtone": 1000,
+            "ps": 0, "nd_socks": -1, "g_q8": 0, "supp": 0, "far": 0, "rxgap": 0, "mclip": 0}
+    vals.update(over)
+    out = []
+    for word in firmware_tune_format().split(" "):
+        key, eq, spec = word.partition("=")
+        assert not eq or key in vals, f"펌웨어 TUNE 에 이 시험이 모르는 칸이 있다: {word}"
+        out.append(f"{key}={vals[key]}" if eq else word)
+    return " ".join(out)
+
+
+def parse_kv(line):
+    return {k: int(v) for k, v in re.findall(r"(\w+)=(-?\d+)", line)}
+
+
+def test_phase3c_stat_line_is_exactly_the_5935ddd_line():
+    """3c 는 STAT 에 칸을 더하지 않는다(새 값은 TUNE 줄에) — 기존 도구·기록 창·앞 계획의 식이 그대로 맞는다."""
+    assert firmware_stat_format() == STAT3A_FORMAT + STAT3B_TAIL + STAT3B2_TAIL
+    assert stat3b().endswith(" aud_gap=16100 v_under_ms=0 txgain=4 vol=100")
+
+
+def test_phase3c_tune_line_format_is_pinned():
+    assert firmware_tune_format() == TUNE3C_FORMAT
+    line = tune_line()
+    assert line == "TUNE dflt=1 legacy=1 meas=1 txgain=4 vol=100 mic=24 kbps=24 lvl=0 tilt=0 res=0 jb=0 nodelay=0 vtone=1000 ps=0 nd_socks=-1 g_q8=0 supp=0 far=0 rxgap=0 mclip=0"
+    assert "%" not in line and pc.parse_stat(line) is None                                           # STAT 줄이 아니다 — parse_stat 은 건드리지 않는다
+    d = parse_kv(tune_line(legacy=0, lvl=12, g_q8=3072, nd_socks=1, rxgap=262, ps=-1))
+    assert d["legacy"] == 0 and d["lvl"] == 12 and d["g_q8"] == 3072 and d["nd_socks"] == 1 and d["rxgap"] == 262 and d["ps"] == -1 and d["mic"] == 24
+    assert len(d) == 20
+
+
+@pytest.mark.parametrize("pattern, kwargs, want", [
+    (r"^TUNE dflt=1 legacy=1 meas=1 ", {}, True),                                                    # 손잡이를 건드리지 않은 기기(기본값을 굳히기 전의 빌드)
+    (r"^TUNE dflt=1 legacy=1 meas=1 ", {"legacy": 0, "lvl": 12}, False),
+    (r"^TUNE .* legacy=1 ", {"dflt": 0}, True),                                                      # 굳힌 빌드에서 시리얼로 옛 값을 준 기기
+    (r"^TUNE .* meas=1 ", {"meas": 0, "vol": 80}, False),
+    (r"^TUNE .* ps=0 ", {}, True),
+    (r"^TUNE .* ps=0 ", {"ps": 1}, False),
+    (r"^TUNE .* nodelay=1 .* nd_socks=1 ", {"nodelay": 1, "nd_socks": 1}, True),
+    (r"^TUNE .* lvl=12 tilt=0 res=0 ", {"lvl": 12}, True),
+])
+def test_phase3c_tune_patterns_of_the_device_tasks(pattern, kwargs, want):
+    assert bool(re.search(pattern, tune_line(**kwargs))) is want
+
+
+def tune_header_ints():
+    return {k: int(v) for k, v in re.findall(r"\b([A-Z][A-Z_]+[A-Z])\s*= (-?\d+)\b", VOICE_TUNE_H.read_text(encoding="utf-8"))}
+
+
+def test_phase3c_knob_answers_have_the_documented_shape_and_their_ranges_match_the_header():
+    src = PHONE_MAIN_CPP.read_text(encoding="utf-8")
+    for want in ('printf("OK %s %d\\n", name, get())', 'printf("ERR %s %s\\n", name, range)', '"OK mic %d\\n"', '"ERR mic 12-24 step 3\\n"', '"ERR mic no-audio\\n"', '"ERR mic i2c\\n"',
+                 'knob_cmd(cmd, "lvl", 0, voicetune::LVL_MAX, "0|6-16"', 'knob_cmd(cmd, "tilt", 0, voicetune::TILT_MAX, "0-2"', 'knob_cmd(cmd, "res", 0, voicetune::RES_DB_MAX, "0|6|12|18"',
+                 'knob_cmd(cmd, "opus", voicetune::KBPS_MIN, voicetune::KBPS_MAX, "12-40"', 'knob_cmd(cmd, "jb", 0, voicetune::JB_MAX, "0|3-10"',
+                 'knob_cmd(cmd, "vtone", voicetune::VTONE_MIN, voicetune::VTONE_MAX, "300-7000"', 'knob_cmd(cmd, "nodelay", 0, 1, "0|1"', 'cmd == "tune"'):
+        assert want in src, want
+    h = tune_header_ints()                                                                           # 응답에 적힌 범위 = voice_tune.h 의 범위
+    assert (h["LVL_MIN"], h["LVL_MAX"], h["TILT_MAX"], h["RES_DB_STEP"], h["RES_DB_MAX"]) == (6, 16, 2, 6, 18)
+    assert (h["KBPS_MIN"], h["KBPS_MAX"], h["JB_MIN"], h["JB_MAX"], h["VTONE_MIN"], h["VTONE_MAX"]) == (12, 40, 3, 10, 300, 7000)
+    assert (h["MIC_DB_MIN"], h["MIC_DB_MAX"], h["MIC_DB_STEP"]) == (12, 24, 3)
+    assert (h["TXGAIN_MIN"], h["TXGAIN_MAX"], h["VOL_MIN"], h["VOL_MAX"]) == (1, 4, 0, 100)                # 3b 의 txgain·vol 과 같은 범위
+    assert h["MIC_DB_MAX"] == h["LEGACY_MIC_DB"] == 24                                                # 마이크 증폭은 3b 의 값보다 올리지 못한다(고리 이득)
+    assert '"probe"' in VOICE_CORE_H.read_text(encoding="utf-8")                                     # vsrc probe
+
+
+def test_phase3c_boot_defaults_live_on_nine_lines_the_freeze_tool_can_rewrite():
+    """굳히기 과제(freeze_tune.py)는 voice_tune.h 의 DEFAULT_* 아홉 줄의 숫자만 고친다 — 그 줄의 모양을 고정한다(값은 고정하지 않는다)."""
+    lines = re.findall(r"^constexpr int DEFAULT_([A-Z_]+)\s+= (\d+);", VOICE_TUNE_H.read_text(encoding="utf-8"), re.M)
+    assert [k for k, _ in lines] == ["TXGAIN", "VOL", "MIC_DB", "KBPS", "LVL", "TILT", "RES_DB", "JB", "NODELAY"]
+    h = tune_header_ints()
+    assert (h["LEGACY_TXGAIN"], h["LEGACY_VOL"], h["LEGACY_MIC_DB"], h["LEGACY_KBPS"], h["LEGACY_LVL"], h["LEGACY_TILT"], h["LEGACY_RES_DB"], h["LEGACY_JB"], h["LEGACY_NODELAY"]) == (4, 100, 24, 24, 0, 0, 0, 0, 0)
+    assert h["DEFAULT_TXGAIN"] == 4                                                                  # 3b 의 시험이 TX_GAIN_DEFAULT = 4 를 고정한다(평탄화가 그 위를 맡는다) — 굳히기에서 바꾸지 않는다
+
+
+def test_phase3c_knob_commands_are_sent_as_plain_lines_and_the_answers_can_be_waited_for():
+    hub, links, clock, out = make_hub()
+
+    def respond(link, cmd):
+        word = cmd.split()
+        if cmd == "tune":
+            link.say(tune_line(legacy=0, lvl=12, g_q8=1024), at=clock.t)
+        elif word[0] in ("lvl", "tilt", "res", "opus", "jb", "vtone", "nodelay", "mic") and len(word) == 2:
+            link.say(f"ERR lvl 0|6-16" if cmd == "lvl 3" else f"OK {word[0]} {word[1]}", at=clock.t)
+        elif cmd == "vsrc probe":
+            link.say("OK vsrc probe", at=clock.t)
+    links["1"].respond = links["2"].respond = respond
+    steps = ["both:lvl 12", r"both:wait:^OK lvl 12$:3", "1:lvl 3", r"1:wait:^ERR lvl 0\|6-16$:3", "2:tilt 1", r"2:wait:^OK tilt 1$:3", "2:res 12", r"2:wait:^OK res 12$:3",
+             "1:mic 18", r"1:wait:^OK mic 18$:3", "1:opus 32", r"1:wait:^OK opus 32$:3", "both:jb 4", r"both:wait:^OK jb 4$:3", "both:nodelay 1", r"both:wait:^OK nodelay 1$:3",
+             "1:vtone 3000", r"1:wait:^OK vtone 3000$:3", "1:vsrc probe", r"1:wait:^OK vsrc probe$:3", "both:tune", r"both:wait:^TUNE dflt=\d legacy=0 .* lvl=12 .* g_q8=1024 :3"]
+    plan = pc.plan_steps(steps, pair=True)
+    assert pc.run_steps(hub, plan), "\n".join(out)
+    assert links["1"].cmds == ["lvl 12", "lvl 3", "mic 18", "opus 32", "jb 4", "nodelay 1", "vtone 3000", "vsrc probe", "tune"]
+    assert links["2"].cmds == ["lvl 12", "tilt 1", "res 12", "jb 4", "nodelay 1", "tune"]
```

- [x] **Step 2: 실패를 확인한다**

Run: 도구 pytest
Expected: `11 failed, 226 passed` — 실패한 것은 전부 `test_phase3c_tune_…`·`test_phase3c_knob_…`(`test_phase3c_stat_line_is_exactly_the_5935ddd_line` 과 `…boot_defaults_live_on_nine_lines…` 는 이미 통과한다).

- [x] **Step 3: 구현**

```bash
cd ~/tab5-toy-phone && git apply $P/T5-02-knobs-wiring.patch
```
```diff
--- a/firmware/tab5_phone/main/voice.h
+++ b/firmware/tab5_phone/main/voice.h
@@ -1,5 +1,5 @@
 // voice.h — 목소리 통화 경로(기기 전용). 판단은 voice_core.h(맥 시험)에 있고, 여기는 그것을 태스크·코덱·교환기에 잇기만 한다.
-// 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md PL3b-D5~D16.
+// 계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md PL3b-D5~D16. Phase 3c(docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md): 통화 품질 손잡이(PL3c-D3~D9).
 //   보내는 길(캡처 태스크): 마이크 4칸 읽기 → 에코 제거(esp-sr) → Opus 압축 → 프레임 포장 → 보내기 대기열 → (송신 태스크) 교환기
 //   받는 길: (웹소켓 태스크) 프레임 → 지터 버퍼 → (해제 태스크) Opus 해제·메우기 → 링버퍼 → (출력 태스크) audio_out 의 둘째 공급자
 //   통화 밖에서는 세 태스크가 모두 잠들어 있고 마이크를 읽지 않는다(PH-D6: 받기 전에는 마이크가 열리지 않는다).
@@ -7,6 +7,7 @@
 #include <cstdint>
 
 #include "voice_core.h"
+#include "voice_tune.h"
 
 namespace voice {
 
@@ -26,6 +27,22 @@
 bool set_tx_gain(int n);
 int tx_gain();
 
+// ── Phase 3c 통화 품질 손잡이(PL3c-D3). 저장하지 않는다(재부팅하면 voice_tune.h 의 DEFAULT_*). 통화 안팎 어느 때나. 범위 밖이면 false 와 값은 그대로 ──
+bool set_kbps(int kbps);                    // Opus 비트율 12~40kbps. 캡처 태스크가 다음 묶음에서 압축기에 건다(통화 밖이면 다음 통화의 첫 묶음에서). 압축기가 거절하면 앞 값으로 되돌아간다
+bool set_lvl(int max_gain);                 // 보내는 크기 평탄화: 0 = 끔(고정 이득), 6~16 = 말소리 이득의 상한(배) — tx_dyn.h
+bool set_tilt(int n);                       // 보내는 소리의 고음 올리기: 0 = 끔, 1·2 — tx_dyn.h
+bool set_res(int db);                       // 남은 메아리 누르기: 0·6·12·18dB — tx_dyn.h
+bool set_jb(int cap);                       // 지터 버퍼 규칙: 0 = 3b 그대로, 3~10 = 새 규칙 + 목표 상한(프레임). 통화 중에도 바로 바뀐다 — jitter_buffer.h JB-D3
+bool set_vtone(int hz);                     // 시험음(vsrc tone·probe)의 주파수 300~7000Hz. selftest 의 시험음은 늘 1kHz
+struct Tune {                               // TUNE 줄에 싣는 것(phone_main 이 음량·마이크 증폭·nodelay·절전 상태를 더해 찍는다)
+    int kbps, lvl, tilt, res_db, jb, vtone; // 손잡이의 지금 값
+    int gain_q8;                            // 평탄화가 지금 걸고 있는 이득(256 = 1배). 손잡이(lvl·res)가 둘 다 꺼져 있으면 0
+    uint32_t supp, far;                     // 이번(또는 직전) 통화에서: 남은 메아리를 누른 묶음 수 / 상대 소리가 나던 묶음 수(묶음 = 16ms)
+    uint32_t rxgap_ms;                      // 이번(또는 직전) 통화에서 받은 프레임 사이의 가장 긴 틈(ms) — 망이 멈췄다 풀린 길이
+    uint32_t mic_clip;                      // 이번(또는 직전) 통화에서 마이크 칸이 꼭대기에 닿은 표본 수(스피커 소리가 마이크에서 넘친다 → mic 손잡이를 내릴 근거)
+};
+void tune(Tune *out);
+
 bool ping();                                // 통화 중에만: 지연 재기 한 번. 결과는 print_pending 이 "VPING rt_ms=…" 또는 "VPING timeout" 으로 찍는다
 struct SelfTest { uint32_t mic, ref, aec; };
 bool selftest(SelfTest *out);               // 통화 밖에서만: 시험음을 0.64초 내며 마이크 칸(mic)·되먹임 칸(ref)·에코 제거 뒤(aec)의 세기를 잰다. 약 0.7초 기다린다
--- a/firmware/tab5_phone/main/voice.cpp
+++ b/firmware/tab5_phone/main/voice.cpp
@@ -44,9 +44,13 @@
 
 #include "audio_out.h"
 #include "relay_link.h"
+#include "tx_dyn.h"
 
 static_assert(voicecore::RATE == audioout::RATE && voicecore::BLOCK == audioout::BLOCK && voicecore::IN_SLOTS == audioout::IN_SLOTS,
               "voice_core.h and audio_out.h must agree on rate, block size and mic slots");
+static_assert(txdyn::BLOCK == voicecore::BLOCK, "tx_dyn.h works on the same 16 ms block");
+static_assert(voicetune::TXGAIN_MIN == voicecore::TX_GAIN_MIN && voicetune::TXGAIN_MAX == voicecore::TX_GAIN_MAX && voicetune::DEFAULT_TXGAIN == voicecore::TX_GAIN_DEFAULT,
+              "voice_tune.h and voice_core.h must agree on the send gain (the 3b tests pin TX_GAIN_DEFAULT = 4)");
 
 namespace voice {
 namespace {
@@ -57,7 +61,7 @@
 
 constexpr int AEC_FILTER_LEN = 4;                               // esp-sr 권장값(시험 펌웨어와 같다)
 constexpr aec_mode_t AEC_MODE = AEC_MODE_VOIP_HIGH_PERF;        // Phase 0: 잔향 감소 29~42dB, 묶음 256표본. FD_HIGH_PERF 는 쓰지 않는다
-constexpr int OPUS_BITRATE = 24000;                             // PH-D5
+constexpr int OPUS_BITRATE = voicetune::DEFAULT_KBPS * 1000;   // PH-D5 — 부팅 값(voice_tune.h, 지금 24kbps). 실행 중에는 set_kbps(PL3c-D8)
 constexpr int OPUS_COMPLEXITY = 0;                              // Phase 0: 5 는 부하 53.6% 로 초과, 0 은 30~37%
 // PL3c-D1 캡처 태스크의 스택 48 → 32KB(내부 메모리 16KB 를 되찾는다 — 3b 최종 검수 M-11).
 //   근거(기기 실측, STAT stk_vcap = 최저 여유): 만든 직후 46,380(초기화는 2.7KB 만 쓴다) → 에코 제거만 돈 뒤(selftest) 42,964(6.2KB) →
@@ -97,6 +101,12 @@
 std::atomic<Src> s_src{Src::AEC};
 TxGain s_tx_gain;                                               // PL3b-D23: 보내는 이득 1~4배(기본 4). 쓰기 = 시리얼 태스크, 읽기 = 캡처 태스크
 std::atomic<uint32_t> s_gen{0};                                 // PL3b-D27: 통화가 열릴 때마다 오르는 세대 번호(set(true)) — 캡처 태스크가 "새 통화"를 모드와 별개로 알아본다
+// PL3c-D3 통화 품질 손잡이(쓰기 = 시리얼 태스크, 읽기 = 캡처 태스크). 부팅 값은 voice_tune.h
+std::atomic<int> s_kbps_want{voicetune::DEFAULT_KBPS}, s_lvl{voicetune::DEFAULT_LVL}, s_tilt{voicetune::DEFAULT_TILT}, s_res_db{voicetune::DEFAULT_RES_DB}, s_jb{voicetune::DEFAULT_JB},
+                 s_vtone{voicetune::VTONE_DEFAULT};
+std::atomic<int> s_dyn_gain_q8{0};                              // 평탄화가 지금 걸고 있는 이득(TUNE g_q8=)
+std::atomic<uint32_t> s_dyn_supp{0}, s_dyn_far{0}, s_mic_clip{0};   // s_mic_clip: 이번 통화에서 마이크 칸이 꼭대기에 닿은 표본 수(TUNE mclip=)
+ArrivalGap s_rx_gap;                                            // PL3c-D4: 받은 프레임 사이의 가장 긴 틈(웹소켓 태스크가 적는다)
 std::atomic<uint32_t> s_call_t0_ms{0}, s_first_under_ms{0};     // PL3b-D25: 이번 통화의 시작 시각 / 첫 굶음(v_under)이 난 시각(통화 시작 뒤 ms, 없으면 0)
 Mutex s_jb_lock, s_txq_lock;
 Receiver<Mutex> *s_rx = nullptr;                                // 지터 버퍼(약 13KB)가 들어 있어 PSRAM 에 둔다
@@ -147,6 +157,7 @@
 // relay_link 가 받은 음성 프레임을 넘긴다(웹소켓 태스크 — 복사만 하고 바로 돌아온다). 통화 밖에서 온 것은 Receiver 가 버린다
 void on_frame(const uint8_t *frame, size_t len)
 {
+    if (s_rx->open()) s_rx_gap.hit(now_ms());                   // PL3c-D4: 통화 중에 받은 프레임 사이의 틈(통화 밖의 것은 재지 않는다)
     s_rx->push(frame, len);
     if (s_rx->want_pump()) xTaskNotifyGive(s_rx_task);          // want_pump 는 Ring::size() 를 읽는다. 여기는 쓰는 태스크도 읽는 태스크도 아닌 제3 태스크라 순간적으로 큰 값이 나올 수 있다 —
                                                                 // 결과는 깨우기 한 번을 빼먹는 것뿐이고 출력 태스크가 16ms 안에 다시 깨운다(최종 검수 M-8, 동작은 그대로)
@@ -209,7 +220,10 @@
     }
     int enc_in = 0;
     if (esp_opus_enc_open(&ecfg, sizeof ecfg, &s_enc) != ESP_AUDIO_ERR_OK || esp_opus_enc_get_frame_size(s_enc, &enc_in, &s_enc_out) != ESP_AUDIO_ERR_OK) return "opus-enc";
-    if (enc_in != FRAME * 2 || s_enc_out <= 0 || (size_t)s_enc_out + 2 > MAX_TX_FRAME) { ESP_LOGE(TAG, "opus frame %d/%d", enc_in, s_enc_out); return "opus-size"; }
+    // s_enc_out = 압축기가 요구하는 출력 버퍼 크기(프레임 바이트 + 100 — voice_tune.h 의 opus_out_need). 버퍼(OPUS_OUT_CAP)에 들어가야 한다.
+    //   3b 는 이것을 보내기 칸(MAX_TX_FRAME)과 견줬다 — 24kbps 에서는 같은 답이지만 비트율을 올리면 요구(40kbps = 200)가 칸보다 커진다. 실제 프레임(40kbps = 100바이트)이
+    //   칸에 들어가는지는 pack_voice 가 프레임마다 본다(안 들어가면 그 프레임은 v_txfail 로 센다).
+    if (enc_in != FRAME * 2 || s_enc_out <= 0 || s_enc_out > voicetune::OPUS_OUT_CAP) { ESP_LOGE(TAG, "opus frame %d/%d", enc_in, s_enc_out); return "opus-size"; }
     esp_opus_dec_cfg_t dcfg = ESP_OPUS_DEC_CONFIG_DEFAULT();
     dcfg.sample_rate = ESP_AUDIO_SAMPLE_RATE_16K; dcfg.channel = ESP_AUDIO_MONO; dcfg.frame_duration = ESP_OPUS_DEC_FRAME_DURATION_20_MS;
     if (esp_opus_dec_open(&dcfg, sizeof dcfg, &s_dec) != ESP_AUDIO_ERR_OK) return "opus-dec";
@@ -219,6 +233,18 @@
     return nullptr;
 }
 
+// PL3c-D8 Opus 비트율을 실행 중에 바꾼다. 캡처 태스크에서만, 압축과 압축 사이에 부른다 — 부품의 이 함수에는 잠금이 없다(esp_opus_enc.h:158-160 의 주의).
+//   바꾼 뒤에는 압축기가 요구하는 출력 버퍼 크기를 다시 묻는다: esp_opus_enc_process 는 넘겨준 길이가 그보다 작으면 그 프레임을 압축하지 않는다(역어셈블로 확인 — 계획서 PL3c-D8).
+//   인코더를 다시 열지 않는다(복잡도·VBR·FEC 는 열 때만 정해진다 — 손잡이로 두지 않았다).
+bool apply_kbps(int kbps)
+{
+    if (esp_opus_enc_set_bitrate(s_enc, kbps * 1000) != ESP_AUDIO_ERR_OK) return false;
+    int in = 0, out = 0;
+    if (esp_opus_enc_get_frame_size(s_enc, &in, &out) != ESP_AUDIO_ERR_OK || in != FRAME * 2 || out <= 0 || out > voicetune::OPUS_OUT_CAP) return false;
+    s_enc_out = out;
+    return true;
+}
+
 void cap_task(void *)
 {
     s_codec_fail = codec_setup();
@@ -227,9 +253,11 @@
 
     // fix 1 F3: 압축기 입출력은 Phase 0 시험 펌웨어(probe_main.cpp 111~113, 154~155)와 같게 16바이트 정렬 버퍼로 준다. 압축 출력은 따로 받아 pack_voice 가 pkt 에 싣는다.
     alignas(16) static int16_t blk[BLOCK], frame[FRAME];
-    alignas(16) static uint8_t enc_out[MAX_TX_FRAME];
+    alignas(16) static uint8_t enc_out[voicetune::OPUS_OUT_CAP];   // PL3c-D8: 40kbps 의 요구(200)까지 담는다(3b 는 MAX_TX_FRAME = 200 — 24kbps 의 요구 160)
     static uint8_t pkt[MAX_TX_FRAME];
-    Uplink up; TestTone tone; PingGen ping;
+    Uplink up; TestTone tone, probe; PingGen ping;
+    static txdyn::Dyn dyn;                                       // PL3c-D7: 보내는 크기 다듬기(손잡이 lvl·tilt·res 가 셋 다 0 이면 3b 의 고정 이득 그대로)
+    int kbps_now = voicetune::DEFAULT_KBPS;                      // 압축기에 지금 걸려 있는 비트율
     pcm::Rms r_mic, r_ref, r_aec, t_mic, t_ref, t_aec;
     int rms_blocks = 0, test_blocks = 0;
     Mode was = Mode::OFF;
@@ -239,13 +267,19 @@
         if (mode == Mode::OFF) { was = Mode::OFF; ulTaskNotifyTake(pdTRUE, portMAX_DELAY); continue; }   // 통화 밖: 마이크를 읽지 않는다(PH-D6)
         if (gen.fresh(s_gen.load(), mode != was)) {             // 방금 켜졌다(통화 또는 점검)
             was = mode;
-            up.start(); tone.reset(); ping.stop();
+            up.start(); tone.reset(); probe.reset(); ping.stop();
+            dyn.reset(); s_dyn_gain_q8 = 0; s_dyn_supp = 0; s_dyn_far = 0; s_mic_clip = 0;
             r_mic.take(); r_ref.take(); r_aec.take(); t_mic.take(); t_ref.take(); t_aec.take();
             rms_blocks = 0; test_blocks = 0;
         }
         if (!audioout::read_mic(s_slots, BLOCK)) { vTaskDelay(pdMS_TO_TICKS(16)); continue; }   // 실패해도 박자를 지킨다(쉬지 않고 돌면 화면이 굶는다)
         ++s_cap_blocks;
         split_slots(s_slots, BLOCK, s_mic, s_ref);
+        if (const uint32_t c = clipped(s_mic, BLOCK)) s_mic_clip += c;   // PL3c-D4: 마이크가 넘친 표본(진짜 마이크 소리를 센다 — probe 를 넣기 전에)
+        const Src src = mode == Mode::CALL ? effective_src(s_src.load(), audioout::amp_is_on()) : Src::AEC;   // PL3b-D22: 스피커가 켜졌으면 mic·ref 는 aec 로
+        const int vtone = s_vtone.load();
+        tone.set_hz(vtone); probe.set_hz(vtone);
+        if (src == Src::PROBE) probe.render(s_mic, BLOCK, PROBE_AMP);   // PL3c-D4: 마이크 자리에 시험음을 넣어 에코 제거를 거치게 한다(진짜 마이크 소리는 쓰이지 않는다)
         int64_t t0 = esp_timer_get_time();
         aec_process(s_aec, s_mic, s_ref, s_clean);
         s_t_aec.add(esp_timer_get_time() - t0);
@@ -271,9 +305,19 @@
                 s_ping_t0_ms = now_ms() - (uint32_t)((BLOCK - at) * 1000 / RATE);
                 s_ping_armed = true;
             }
+        } else if (uses_dyn(src)) {                             // PL3c-D7: 에코 제거를 거친 소리(aec·probe)는 크기 다듬기를 거친다
+            pcm::Rms rr; rr.add(s_ref, BLOCK);                  //   이 묶음의 되먹임 칸 세기 = "상대 소리가 지금 나고 있나"
+            memcpy(blk, s_clean, sizeof blk);
+            const txdyn::Params dp = {s_tx_gain.q8(), s_lvl.load(), s_res_db.load(), s_tilt.load()};
+            dyn.process(blk, rr.value(), dp);
+            s_dyn_gain_q8 = dyn.gain_q8(); s_dyn_supp = dyn.suppressed(); s_dyn_far = dyn.far_blocks();
         } else {
-            pick(effective_src(s_src.load(), audioout::amp_is_on()), s_clean, s_mic, s_ref, tone, blk, s_tx_gain.q8());   // PL3b-D22·D23: 스피커가 켜졌으면 mic·ref 는 aec 로, 이득은 실행 중 값
+            pick(src, s_clean, s_mic, s_ref, tone, blk, s_tx_gain.q8());   // 진단 출처(mic·ref·tone): 3b 그대로(PL3b-D23 의 고정 이득)
         }
+        if (const int want = s_kbps_want.load(); want != kbps_now) {      // PL3c-D8: 비트율 손잡이 — 압축 앞에서 건다
+            if (apply_kbps(want)) kbps_now = want;
+            else { ESP_LOGW(TAG, "opus bitrate %d kbps refused - back to %d", want, kbps_now); apply_kbps(kbps_now); s_kbps_want = kbps_now; }
+        }
         up.push(blk);
         uint16_t seq = 0;
         while (up.pop(frame, &seq)) {                           // 시험 펌웨어의 esp_opus_enc_process 호출 그대로
@@ -301,6 +345,7 @@
     if (!rx_mem || !txq_mem) return "memory";
     s_rx = new (rx_mem) Receiver<Mutex>(s_jb_lock);
     s_txq = new (txq_mem) TxFifo<Mutex>(s_txq_lock);
+    if (voicetune::DEFAULT_JB) s_rx->set_policy(jitterbuf::tuned_policy(voicetune::DEFAULT_JB));   // PL3c-D5: 부팅 값이 0 이면 건드리지 않는다(3b 의 규칙)
 
     // 에코 제거·Opus 는 캡처 태스크가 자기 스택(48KB)에서 만든다 — 끝날 때까지 여기서 기다린다. 통화 밖이라 그 뒤 태스크는 잠든다.
     static StaticSemaphore_t done_buf;
@@ -343,6 +388,7 @@
         s_tx_ok = 0; s_tx_fail = 0; s_enc_fail = 0;
         s_t_aec.reset(); s_t_enc.reset(); s_t_dec.reset();
         s_call_t0_ms = now_ms(); s_first_under_ms = 0;          // PL3b-D25: rx->start() 앞에서 — 그 뒤에야 open 이 참이 되어 공급자가 첫 굶음을 적는다
+        s_rx_gap.reset();                                       // PL3c-D4: 틈은 통화마다 다시 잰다
         s_rx->start();                                          // 지터 버퍼를 비우고 받기 시작한다
         s_gen.fetch_add(1);                                     // PL3b-D27: 모드를 CALL 로 바꾸기 직전에 올린다 — 캡처 태스크가 CALL 을 보면 새 세대도 반드시 보인다
         s_mode = Mode::CALL;
@@ -371,6 +417,25 @@
 bool set_tx_gain(int n) { return s_tx_gain.set(n); }
 int tx_gain() { return s_tx_gain.get(); }
 
+// ── Phase 3c 통화 품질 손잡이(PL3c-D3) ──
+bool set_kbps(int kbps) { if (!voicetune::ok_kbps(kbps)) return false; s_kbps_want = kbps; return true; }
+bool set_lvl(int max_gain) { if (!voicetune::ok_lvl(max_gain)) return false; s_lvl = max_gain; return true; }
+bool set_tilt(int n) { if (!voicetune::ok_tilt(n)) return false; s_tilt = n; return true; }
+bool set_res(int db) { if (!voicetune::ok_res_db(db)) return false; s_res_db = db; return true; }
+bool set_jb(int cap)
+{
+    if (!voicetune::ok_jb(cap) || !s_ready) return false;
+    s_rx->set_policy(cap ? jitterbuf::tuned_policy(cap) : jitterbuf::legacy_policy());     // 지터 버퍼 잠금 안에서 바뀐다(통화 중에도)
+    s_jb = cap;
+    return true;
+}
+bool set_vtone(int hz) { if (!voicetune::ok_vtone(hz)) return false; s_vtone = hz; return true; }
+void tune(Tune *o)
+{
+    o->kbps = s_kbps_want.load(); o->lvl = s_lvl.load(); o->tilt = s_tilt.load(); o->res_db = s_res_db.load(); o->jb = s_jb.load(); o->vtone = s_vtone.load();
+    o->gain_q8 = s_dyn_gain_q8.load(); o->supp = s_dyn_supp.load(); o->far = s_dyn_far.load(); o->rxgap_ms = s_rx_gap.max_ms(); o->mic_clip = s_mic_clip.load();
+}
+
 bool ping()
 {
     if (s_mode.load() != Mode::CALL || s_ping_armed.load()) return false;
--- a/firmware/tab5_phone/main/audio_out.h
+++ b/firmware/tab5_phone/main/audio_out.h
@@ -54,6 +54,9 @@
 enum class VolResult : uint8_t { OK, RANGE, NO_AUDIO, I2C };
 VolResult set_volume(int percent);          // RANGE = 0~100 밖, NO_AUDIO = 소리 장치가 안 열렸다, I2C = 코덱 쓰기 실패(값은 그대로)
 int volume();                               // 지금 값(STAT vol=)
+// PL3c-D9 마이크 증폭(12~24dB, 3dB 단위) — 실행 중에만 유효하다(저장하지 않는다. 재부팅하면 시작 값). 통화 중에도 된다(바꾸는 순간 딸깍 소리가 날 수 있다).
+VolResult set_mic_gain(int db);             // RANGE = 범위·단위 밖, NO_AUDIO = 소리 장치가 안 열렸다, I2C = 칩에서 되읽은 값이 다르다(앞 값으로 되돌린다)
+int mic_gain();                             // 지금 값(TUNE mic=)
 
 bool ready();                               // 소리 경로 전체가 준비됐나(STAT audio=). fail 이 한 번이라도 불렸으면 false
 void block_us(uint32_t *avg, uint32_t *max);   // 소리를 낸 묶음 하나를 만드는 데 걸린 시간의 평균·최대(µs, 부팅 뒤). 묶음 하나 = 16000µs(STAT aud_us=평균/최대).
--- a/firmware/tab5_phone/main/audio_out.cpp
+++ b/firmware/tab5_phone/main/audio_out.cpp
@@ -35,6 +35,7 @@
 #include "freertos/task.h"
 
 #include "pcm.h"
+#include "voice_tune.h"
 
 namespace audioout {
 namespace {
@@ -44,10 +45,10 @@
 constexpr uint8_t ES8388_ADDR = 0x10, ES7210_ADDR = 0x40, PI4IO1_ADDR = 0x43;
 constexpr uint32_t I2C_FREQ = 400000;
 constexpr gpio_num_t PIN_MCLK = GPIO_NUM_30, PIN_BCLK = GPIO_NUM_27, PIN_WS = GPIO_NUM_29, PIN_DOUT = GPIO_NUM_26, PIN_DIN = GPIO_NUM_28;
-constexpr int VOLUME = 100;                     // PL3a-D2
+constexpr int VOLUME = voicetune::DEFAULT_VOL;  // PL3a-D2. Phase 3c: 부팅 값은 voice_tune.h 한 곳에 둔다(PL3c-D3) — 지금 100
 constexpr int DMA_DESC = 3;                     // PL3b-D4: DMA 칸 수. 출력은 (3 − 1) × 16ms = 32ms 앞서 쓰여 있다
 constexpr int64_t LATE_US = (int64_t)DMA_DESC * BLOCK * 1000000 / RATE;   // 48ms: 앞 묶음 뒤 이만큼 지나서야 쓰면 그 사이 DMA 가 바닥났다
-constexpr int MIC_GAIN_DB = 24;                 // PL3b-D10: 마이크(MIC1 = 칸0)의 증폭. 3dB 단위, 30 이하
+constexpr int MIC_GAIN_DB = voicetune::DEFAULT_MIC_DB;   // PL3b-D10: 마이크(MIC1 = 칸0)의 증폭. 3dB 단위, 30 이하. 부팅 값(voice_tune.h — 지금 24), 실행 중에는 set_mic_gain(PL3c-D9)
 constexpr uint8_t ES7210_MIC1_GAIN_REG = 0x43, ES7210_MIC3_GAIN_REG = 0x45;   // 아래 4비트 = 증폭 ÷ 3dB (es7210.c: _es7210_set_channel_gain)
 static_assert(MIC_GAIN_DB % 3 == 0 && MIC_GAIN_DB >= 0 && MIC_GAIN_DB <= 30, "ES7210 gain steps are 3 dB up to 30 dB");
 constexpr int MAX_SOURCES = 2;                  // 신호음 + 목소리
@@ -64,6 +65,7 @@
 std::atomic<bool> s_failed{false}, s_amp{false};
 std::atomic<uint32_t> s_max_us{0}, s_avg_us{0}, s_wr_err{0}, s_rd_err{0}, s_i2c_err{0}, s_late{0}, s_gap_max{0};
 std::atomic<int> s_vol{VOLUME};                  // 코덱 재생 음량(%): 시작 때 VOLUME, 실행 중 vol 명령(PL3b-D24)이 바꾼다. 쓰기가 성공한 값만 담는다
+std::atomic<int> s_mic_db{MIC_GAIN_DB};          // 마이크 증폭(dB): 시작 때 MIC_GAIN_DB, 실행 중 mic 명령(PL3c-D9)이 바꾼다. 칩에서 되읽어 확인한 값만 담는다
 int16_t s_mix[BLOCK], s_tmp[BLOCK];             // 출력 태스크에서만 쓴다
 uint64_t s_sum_us = 0;                          // 출력 태스크에서만 쓴다. 3a 최종 검수 M-5: 32비트 합계는 소리 낸 누적 63시간에 넘친다(통화는 계속 소리를 낸다)
 uint32_t s_blocks = 0;
@@ -166,14 +168,14 @@
 // PL3b-D10 마이크 증폭은 입력 장치를 "연 뒤에" 건다: es7210 드라이버가 장치를 켜면서(es7210_start → mic_select) 증폭을 드라이버 기본값으로
 // 덮어쓴다(Phase 0 에서 이 줄이 없어 마이크가 29dB 작게 잡혔다). 입력 0(MIC1) = 칸0 = 마이크, 입력 2(MIC3) = 칸1 = 되먹임(0dB — 30dB 면 포화).
 // 입력 장치를 닫았다 열거나 껐다 켜면 드라이버가 다시 지우므로 그때마다 다시 불러야 한다(지금은 부팅 때 한 번 열고 닫지 않는다).
-bool mic_gain_apply()
+bool mic_gain_apply(int db = MIC_GAIN_DB)       // db: 3dB 단위, 30 이하(부르는 쪽이 확인한다)
 {
-    if (esp_codec_dev_set_in_channel_gain(s_in, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), (float)MIC_GAIN_DB) != ESP_CODEC_DEV_OK) return false;
+    if (esp_codec_dev_set_in_channel_gain(s_in, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), (float)db) != ESP_CODEC_DEV_OK) return false;
     if (esp_codec_dev_set_in_channel_gain(s_in, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(2), 0.0f) != ESP_CODEC_DEV_OK) return false;
     bool ok1 = false, ok3 = false;              // 위 호출은 쓰기가 실패해도 OK 를 돌려준다 → 칩에서 되읽는다
     const uint8_t g1 = es7210_reg(ES7210_MIC1_GAIN_REG, &ok1), g3 = es7210_reg(ES7210_MIC3_GAIN_REG, &ok3);
-    if (!ok1 || !ok3 || (g1 & 0x0F) != MIC_GAIN_DB / 3 || (g3 & 0x0F) != 0) {
-        ESP_LOGE(TAG, "mic gain not applied: read %d/%d reg43=0x%02x reg45=0x%02x (want low nibble %d and 0)", (int)ok1, (int)ok3, g1, g3, MIC_GAIN_DB / 3);
+    if (!ok1 || !ok3 || (g1 & 0x0F) != db / 3 || (g3 & 0x0F) != 0) {
+        ESP_LOGE(TAG, "mic gain not applied: read %d/%d reg43=0x%02x reg45=0x%02x (want low nibble %d and 0)", (int)ok1, (int)ok3, g1, g3, db / 3);
         return false;
     }
     return true;
@@ -320,6 +322,18 @@
 }
 int volume() { return s_vol.load(); }
 
+// PL3c-D9 실행 중 마이크 증폭. 부팅 때와 같은 길(mic_gain_apply — 걸고, 칩의 증폭 레지스터를 되읽어 확인한다). 되먹임 칸(MIC3)은 늘 0dB 그대로다.
+// 올리는 쪽은 부팅 값의 상한(voicetune::MIC_DB_MAX = 24)까지만: 올리면 스피커 → 마이크 고리 이득이 그만큼 오른다. 코덱 제어 인터페이스가 I2C 잠금을 스스로 잡는다(set_volume 과 같다).
+VolResult set_mic_gain(int db)
+{
+    if (!voicetune::ok_mic_db(db)) return VolResult::RANGE;
+    if (!s_in || !s_ready.load()) return VolResult::NO_AUDIO;
+    if (!mic_gain_apply(db)) { mic_gain_apply(s_mic_db.load()); return VolResult::I2C; }   // 실패하면 앞 값으로 되돌려 본다(그것도 실패하면 로그가 남는다)
+    s_mic_db = db;
+    return VolResult::OK;
+}
+int mic_gain() { return s_mic_db.load(); }
+
 bool add_source(source_fn fn)
 {
     for (auto &slot : s_src) {
--- a/firmware/tab5_phone/main/relay_link.h
+++ b/firmware/tab5_phone/main/relay_link.h
@@ -40,4 +40,9 @@
 typedef void (*voice_sink_t)(const uint8_t *frame, size_t len);
 void set_voice_sink(voice_sink_t fn);                   // Phase 3: 받은 음성 프레임을 넘겨받을 함수
 
+// Phase 3c(relay_link.cpp RL-D12): 교환기 소켓의 TCP_NODELAY. 값은 기억해 두고 다시 붙을 때마다 건다. 저장하지 않는다(부팅 값 = voicetune::DEFAULT_NODELAY).
+int  set_nodelay(bool on);          // 돌려주는 값 = 지금 옵션을 건 소켓 수(붙어 있지 않으면 0 — 값만 기억), 잠금을 2초 안에 못 얻으면 -1
+bool nodelay();                     // 기억한 값
+int  nodelay_socks();               // 마지막으로 옵션을 건 소켓 수(-1 = 아직 건 적 없음)
+
 }  // namespace relaylink
--- a/firmware/tab5_phone/main/relay_link.cpp
+++ b/firmware/tab5_phone/main/relay_link.cpp
@@ -31,6 +31,16 @@
 //   RL-D11 (3a 최종 검수 M-8) stop 이 도는 동안 접속이 완료되면 핸들러가 LINK_UP 을 대기열에 넣는다. stop 뒤에 그 흔적(UP)이 보이면
 //         LINK_DOWN 을 뒤따라 올려 상태 기계가 "붙었다 → 끊겼다"로 읽게 한다(전에는 LINK_UP 만 남아 잠깐 대기 화면이 됐다).
 //         fix 1 F2: 재접속 블록 첫머리의 "UP 이었나" 읽기와 IDLE 쓰기도 exchange 한 번으로 묶었다(따로면 그 사이 UP 이 된 것을 놓쳤다).
+//   RL-D12 (Phase 3c, 계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D6, 3b 최종 검수 M-4) 교환기 소켓의 TCP_NODELAY 를 실행 중에 켜고 끈다(set_nodelay).
+//         왜: 전송 계층은 프레임마다 머리(6바이트)와 몸통(62바이트)을 두 번에 나눠 쓴다(tcp_transport/transport_ws.c:420·428). 기본 동작(Nagle)에서는 몸통이 앞 조각의 확인 응답이 올 때까지
+//             잡혀 있어 프레임이 뭉쳐 나가고, 조각 하나를 잃으면 뒤 조각이 안 나가 빠른 재전송이 걸리지 않는다(재전송 시한까지 0.5초 넘게 멈춘다).
+//         길: 부품에는 설정 칸이 없다(esp_websocket_client 1.8.0 의 설정 구조체·tcp_transport·esp-tls 어디에도 TCP_NODELAY 가 없다 — 검색 0건). 소켓 번호를 얻는 공식 길은
+//             전송 객체를 직접 만들어 넘기는 것(cfg.ext_transport + esp_transport_get_socket)인데, 그러면 접속 경로(주소·토큰 헤더·제어 프레임 설정)를 우리가 다시 짜야 하고
+//             틀리면 교환기에 붙지 못한다(기기에서만 드러난다). 그래서 접속 경로는 건드리지 않고, 붙은 뒤에 lwIP 의 소켓 번호 범위(LWIP_SOCKET_OFFSET 부터 CONFIG_LWIP_MAX_SOCKETS 개,
+//             lwip/port/include/lwipopts.h:986)를 훑어 "연결된 TCP 소켓"에 옵션을 건다. 이 펌웨어의 TCP 연결은 교환기 하나뿐이다(HTTP·MQTT 를 쓰지 않는다).
+//         비용: 소켓을 "찾아서" 건다는 점(다른 TCP 연결이 생기면 거기에도 걸린다 — 해가 되지는 않는다). 작은 조각이 그대로 나가 패킷 수가 는다(프레임당 2개).
+//         언제: set_nodelay 가 그 자리에서 한 번, 그리고 다시 붙을 때마다(CONNECTED) 지금 값으로 다시. 보내기와 같은 잠금(s_tx_lock) 안에서 건다.
+//         탈출구: 기본값 0(끔)이면 3b 와 같다. 효과가 없으면 손잡이째 지운다. 더 엄밀하게 하려면 ext_transport 로.
 #include "relay_link.h"
 
 #include <atomic>
@@ -44,8 +54,10 @@
 #include "freertos/queue.h"
 #include "freertos/semphr.h"
 #include "freertos/task.h"
+#include "lwip/sockets.h"
 
 #include "net_wifi.h"
+#include "voice_tune.h"
 
 namespace relaylink {
 namespace {
@@ -67,6 +79,9 @@
 SemaphoreHandle_t s_tx_lock = nullptr;                           // RL-D10
 constexpr TickType_t TEXT_LOCK_WAIT = pdMS_TO_TICKS(2000), VOICE_LOCK_WAIT = pdMS_TO_TICKS(20);
 
+std::atomic<bool> s_nodelay{voicetune::DEFAULT_NODELAY != 0};    // RL-D12
+std::atomic<int> s_nodelay_socks{-1};                            // 마지막으로 옵션을 건 소켓 수(-1 = 아직 건 적 없음)
+
 char s_url[URL_BUF], s_headers[200];
 constexpr int64_t START_RETRY_US = 5LL * 1000000;                // RL-D8
 
@@ -75,6 +90,22 @@
 int s_rx_len = 0;
 uint8_t s_rx_op = 0;
 bool s_rx_drop = false;
+
+// RL-D12 연결된 TCP 소켓 전부에 TCP_NODELAY 를 건다(또는 푼다). 돌려주는 값 = 건 소켓 수. 부르는 쪽이 s_tx_lock 을 쥐고 있거나, 아직 아무도 보내지 않는 때(CONNECTED 핸들러)다.
+int apply_nodelay(bool on)
+{
+    int n = 0;
+    const int v = on ? 1 : 0;
+    for (int fd = LWIP_SOCKET_OFFSET; fd < LWIP_SOCKET_OFFSET + CONFIG_LWIP_MAX_SOCKETS; ++fd) {
+        int type = 0; socklen_t len = sizeof type;
+        if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &len) != 0 || type != SOCK_STREAM) continue;     // 소켓이 아니거나 TCP 가 아니다
+        struct sockaddr_storage peer; socklen_t plen = sizeof peer;
+        if (getpeername(fd, (struct sockaddr *)&peer, &plen) != 0) continue;                            // 아직 연결되지 않았다
+        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &v, sizeof v) == 0) ++n;
+    }
+    s_nodelay_socks = n;
+    return n;
+}
 
 void post(In::Kind k, const relayproto::Signal *sig = nullptr)
 {
@@ -124,6 +155,7 @@
     case WEBSOCKET_EVENT_CONNECTED:
         s_phase = Phase::UP; s_auth_rejected = false; s_up_since_us = esp_timer_get_time();
         s_rx_len = 0; s_rx_drop = false;
+        if (s_nodelay.load()) apply_nodelay(true);               // RL-D12: 새 소켓이다 — 켜 둔 손잡이를 다시 건다(끈 상태면 건드리지 않는다 = 3b 그대로)
         printf("RELAY up=1 code=0 attempt=%d\n", 0);          // 붙었으니 0 (s_fails 는 아직 묵은 실패 횟수일 수 있다 — 모양은 그대로)
         post(In::Kind::LINK_UP);
         break;
@@ -286,4 +318,16 @@
 
 void set_voice_sink(voice_sink_t fn) { s_voice_sink = fn; }
 
+int set_nodelay(bool on)                                         // RL-D12
+{
+    s_nodelay = on;
+    if (!s_tx_lock || s_phase != Phase::UP) return 0;            // 붙어 있지 않다 — 값만 기억한다(다음에 붙을 때 건다)
+    if (xSemaphoreTake(s_tx_lock, TEXT_LOCK_WAIT) != pdTRUE) return -1;
+    const int n = s_phase == Phase::UP ? apply_nodelay(on) : 0;
+    xSemaphoreGive(s_tx_lock);
+    return n;
+}
+bool nodelay() { return s_nodelay.load(); }
+int nodelay_socks() { return s_nodelay_socks.load(); }
+
 }  // namespace relaylink
--- a/firmware/tab5_phone/main/net_wifi.h
+++ b/firmware/tab5_phone/main/net_wifi.h
@@ -29,6 +29,7 @@
 bool  connected();
 void  ssid(char *out, size_t cap);          // 연결된 망 이름(없으면 빈 글자)
 int   rssi();                               // 연결된 망의 신호(dBm), 없으면 -127. 매니저가 10초마다 읽어 둔 값(연결 직후 1초쯤은 -127)
+int   ps();                                 // Wi-Fi 절전 상태(N-D5): 0 = 꺼짐(통화에 필요한 값), 1·2 = 켜짐, -1 = 못 읽음·아직 안 읽음. 매니저가 10초마다 읽어 둔 값
 uint32_t change_seq();                      // 위 상태·스캔 결과·수동 연결 상태가 바뀔 때마다 1 증가
                                             //   (신호는 막대 칸수 wifistore::bars 가 바뀔 때만 — 1dBm 흔들림으로 화면을 다시 그리지 않게)
 
--- a/firmware/tab5_phone/main/net_wifi.cpp
+++ b/firmware/tab5_phone/main/net_wifi.cpp
@@ -78,6 +78,7 @@
 std::atomic<int>  s_disc_reason{0};
 std::atomic<int>  s_auto_fails{0};
 std::atomic<int>  s_rssi{-127};
+std::atomic<int>  s_ps{-1};                  // N-D5
 std::atomic<uint32_t> s_seq{0};
 
 // ── s_lock 보호 (화면 태스크가 읽는다) ──
@@ -332,6 +333,8 @@
     const int prev = s_rssi.exchange(ap.rssi);
     if (!s_ok) { s_rssi = -127; return; }                             // 읽는 사이 끊김 → 끊김 처리가 넣은 -127 을 덮어쓰지 않게 되돌린다
     if (prev == -127 || wifistore::bars(prev) != wifistore::bars(ap.rssi)) bump();
+    wifi_ps_type_t ps = WIFI_PS_NONE;                                 // N-D5
+    s_ps = esp_wifi_get_ps(&ps) == ESP_OK ? (int)ps : -1;
 }
 
 void manager_loop()
@@ -427,7 +430,11 @@
     if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK || esp_wifi_start() != ESP_OK) { init_failed("wifi start"); vTaskDelete(nullptr); return; }
     // N-D4 Wi-Fi 절전 끄기. 이유: 기본값(모뎀 절전)이면 공유기가 DTIM 마다 모아 둔 자료를 한꺼번에 줘서 통화 중 받는 음성이 0.3초씩 뭉쳐 온다(2026-10-05 실측).
     //      비용: Wi-Fi 칩이 늘 깨어 있어 전력을 더 쓴다 — USB 전원 탁상 전화기라 수용. 탈출구: 이 블록을 지우거나 WIFI_PS_MIN_MODEM; 배터리로 쓰면 통화 중에만 끈다.
-    //      실패해도 부팅을 멈추지 않는다(C6 펌웨어가 모를 수 있음). 드라이버 설정이라 재접속 때 다시 걸지 않는다.
+    //      실패해도 부팅을 멈추지 않는다(C6 펌웨어가 모를 수 있음). 한 번만 건다: 호스트 쪽에는 이 설정이 사라질 길이 없다(esp_wifi_start·init 은 여기서 각 1회, 재접속·망 전환은
+//      connect/disconnect/scan 만 쓴다 — fix 3 검수가 소스로 확인). 단 C6 가 혼자 리셋된 뒤에도 남는지는 확인하지 못했다 → N-D5 가 지금 값을 읽어 TUNE 줄의 ps= 로 보여 준다.
+//   N-D5 (Phase 3c, fix 3 검수 Minor ①) 절전 상태를 신호 세기와 같은 박자(10초)로 읽어 둔다: esp_wifi_get_ps → ps(). 0 = 꺼짐(WIFI_PS_NONE), 1·2 = 켜짐, -1 = 못 읽음·아직 안 읽음.
+//      이유: 절전이 다시 켜지면 받는 음성이 0.3초씩 뭉쳐 오는데(2026-10-05 아침의 고장) 로그로는 "망이 나쁜 것"과 구별되지 않는다. 화면 태스크가 아니라 매니저 태스크가 읽는다(N-D3 과 같은 이유).
+//      비용: 10초마다 C6 까지 다녀오는 호출이 하나 는다. 0 이 아니어도 스스로 다시 걸지는 않는다(값을 보고 정한다 — 계획서 PL3c-D11). 탈출구: refresh_rssi 의 두 줄을 지운다.
     {
         const esp_err_t pe = esp_wifi_set_ps(WIFI_PS_NONE);
         if (pe == ESP_OK) ESP_LOGI(TAG, "wifi power save off");
@@ -477,6 +484,7 @@
 }
 
 int rssi() { return s_ok ? s_rssi.load() : -127; }
+int ps() { return s_ps.load(); }
 
 uint32_t change_seq() { return s_seq; }
 
--- a/firmware/tab5_phone/main/phone_main.cpp
+++ b/firmware/tab5_phone/main/phone_main.cpp
@@ -8,6 +8,9 @@
 //   stat | t <x> <y> | shot | reboot
 //   진단(사람이 켠다): fault audio | quiet on|off | selftest | vsrc aec|mic|ref|tone | vping     (계획 docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md PL3b-D2·D11·D12·D14)
 //   실행 중 조절(저장하지 않는다 — 재부팅하면 기본값. PL3b-D23·D24): txgain [1..4] (보내는 이득, 기본 4) | vol [0..100] (재생 음량, 기본 100). 인자 없이 보내면 지금 값
+//   통화 품질 손잡이(Phase 3c — 계획 docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md PL3c-D3. 저장하지 않는다, 부팅 값은 voice_tune.h. 인자 없이 보내면 지금 값):
+//     lvl [0|6..16] 보내는 크기 평탄화 | tilt [0..2] 고음 올리기 | res [0|6|12|18] 남은 메아리 누르기 | mic [12..24 3dB 단위] 마이크 증폭 | opus [12..40] 비트율(kbps)
+//     | jb [0|3..10] 지터 버퍼 규칙 | nodelay [0|1] TCP_NODELAY | vtone [300..7000] 시험음 주파수 | vsrc probe (에코 제거를 거치는 시험음) | tune (손잡이·측정값 한 줄)
 #include <M5Unified.h>
 #include <cstdio>
 #include <cstdlib>
@@ -125,6 +128,39 @@
     printf("SHOT END %u\n", (unsigned)sum);
 }
 
+// PL3c-D3 손잡이와 그 곁의 측정값을 한 줄로. STAT 줄은 건드리지 않는다(기존 줄 모양 그대로 — 도구·앞 계획의 정규식이 그대로 맞는다).
+//   dflt = 손잡이가 전부 부팅 값인가 / legacy = 전부 5935ddd 의 값인가 / meas = selftest·vping 의 판정이 유효한가(txgain 4 · vol 100 — voice_tune.h meas_valid)
+//   ps = Wi-Fi 절전(0 이어야 한다, -1 = 아직 못 읽음 — net_wifi N-D5) / nd_socks = TCP_NODELAY 를 마지막으로 건 소켓 수(-1 = 건 적 없음)
+//   g_q8 = 평탄화가 지금 걸고 있는 이득(256 = 1배, 0 = 평탄화·누르기 꺼짐) / supp·far = 이번 통화에서 남은 메아리를 누른 묶음·상대 소리가 나던 묶음(16ms 단위)
+//   rxgap = 이번 통화에서 받은 프레임 사이의 가장 긴 틈(ms) / mclip = 이번 통화에서 마이크 칸이 꼭대기에 닿은 표본 수(스피커 소리가 마이크에서 넘친다)
+static voicetune::Knobs knobs_now(const voice::Tune &t)
+{
+    voicetune::Knobs k;
+    k.txgain = voice::tx_gain(); k.vol = audioout::volume(); k.mic_db = audioout::mic_gain(); k.kbps = t.kbps;
+    k.lvl = t.lvl; k.tilt = t.tilt; k.res_db = t.res_db; k.jb = t.jb; k.nodelay = relaylink::nodelay() ? 1 : 0;
+    return k;
+}
+static void print_tune()
+{
+    voice::Tune t; voice::tune(&t);
+    const voicetune::Knobs k = knobs_now(t);
+    printf("TUNE dflt=%d legacy=%d meas=%d txgain=%d vol=%d mic=%d kbps=%d lvl=%d tilt=%d res=%d jb=%d nodelay=%d vtone=%d ps=%d nd_socks=%d g_q8=%d supp=%u far=%u rxgap=%u mclip=%u\n",
+           (int)voicetune::is_default(k), (int)voicetune::is_legacy(k), (int)voicetune::meas_valid(k), k.txgain, k.vol, k.mic_db, k.kbps, k.lvl, k.tilt, k.res_db, k.jb, k.nodelay, t.vtone,
+           netwifi::ps(), relaylink::nodelay_socks(), t.gain_q8, (unsigned)t.supp, (unsigned)t.far, (unsigned)t.rxgap_ms, (unsigned)t.mic_clip);
+}
+static bool meas_valid_now() { voice::Tune t; voice::tune(&t); return voicetune::meas_valid(knobs_now(t)); }
+// 손잡이 명령 하나: "<name>" = 지금 값, "<name> <숫자>" = 바꾸기. set 이 false 면 "ERR <name> <범위>". 돌려주는 값 = 이 명령이었나
+static bool knob_cmd(const std::string &cmd, const char *name, int lo, int hi, const char *range, int (*get)(), bool (*set)(int))
+{
+    const size_t n = strlen(name);
+    if (cmd.compare(0, n, name) != 0 || (cmd.size() != n && cmd[n] != ' ')) return false;
+    int v = 0;
+    if (cmd.size() == n) printf("OK %s %d\n", name, get());
+    else if (voicecore::parse_uint(cmd.c_str() + n + 1, lo, hi, &v) && set(v)) printf("OK %s %d\n", name, get());
+    else printf("ERR %s %s\n", name, range);
+    return true;
+}
+
 static void serial_cmd(const std::string &cmd)
 {
     int x = 0, y = 0;
@@ -147,6 +183,7 @@
         if (voice::selftest(&r)) printf("SELFTEST amp=%d mic=%u ref=%u aec=%u ok=%d\n", (int)audioout::amp_is_on(), (unsigned)r.mic, (unsigned)r.ref, (unsigned)r.aec,
                                         (int)(r.ref >= voicecore::SELFTEST_REF_MIN));
         else printf("SELFTEST FAIL busy-or-no-audio\n");
+        if (!meas_valid_now()) print_tune();                    // PL3c-D3: 이득·음량이 4배·100 이 아니면 위의 ok= 는 믿을 수 없다 — 그 사정(meas=0)을 바로 뒤에 한 줄로 남긴다
     }
     else if (cmd.rfind("vsrc ", 0) == 0) {                      // PL3b-D11: 통화 중에만, 통화가 끝나면 aec 로 돌아간다
         voicecore::Src s;
@@ -170,7 +207,29 @@
             else printf(r == audioout::VolResult::NO_AUDIO ? "ERR vol no-audio\n" : "ERR vol i2c\n");
         }
     }
-    else if (cmd == "vping") printf(voice::ping() ? "OK vping\n" : "ERR vping\n");   // PL3b-D12: 결과는 "VPING rt_ms=…" 줄로(1초쯤 뒤)
+    else if (cmd == "vping") {                                  // PL3b-D12: 결과는 "VPING rt_ms=…" 줄로(1초쯤 뒤)
+        printf(voice::ping() ? "OK vping\n" : "ERR vping\n");
+        if (!meas_valid_now()) print_tune();                    // PL3c-D3: 이 기기의 이득·음량이 4배·100 이 아니다 — timeout 이 나와도 고장이 아닐 수 있다(상대 기기도 같은 조건이어야 한다)
+    }
+    // ── Phase 3c 통화 품질 손잡이(PL3c-D3): 범위는 voice_tune.h, 저장하지 않는다 ──
+    else if (cmd == "tune") print_tune();
+    else if (knob_cmd(cmd, "lvl", 0, voicetune::LVL_MAX, "0|6-16", [] { voice::Tune t; voice::tune(&t); return t.lvl; }, [](int v) { return voice::set_lvl(v); })) {}
+    else if (knob_cmd(cmd, "tilt", 0, voicetune::TILT_MAX, "0-2", [] { voice::Tune t; voice::tune(&t); return t.tilt; }, [](int v) { return voice::set_tilt(v); })) {}
+    else if (knob_cmd(cmd, "res", 0, voicetune::RES_DB_MAX, "0|6|12|18", [] { voice::Tune t; voice::tune(&t); return t.res_db; }, [](int v) { return voice::set_res(v); })) {}
+    else if (knob_cmd(cmd, "opus", voicetune::KBPS_MIN, voicetune::KBPS_MAX, "12-40", [] { voice::Tune t; voice::tune(&t); return t.kbps; }, [](int v) { return voice::set_kbps(v); })) {}
+    else if (knob_cmd(cmd, "jb", 0, voicetune::JB_MAX, "0|3-10", [] { voice::Tune t; voice::tune(&t); return t.jb; }, [](int v) { return voice::set_jb(v); })) {}
+    else if (knob_cmd(cmd, "vtone", voicetune::VTONE_MIN, voicetune::VTONE_MAX, "300-7000", [] { voice::Tune t; voice::tune(&t); return t.vtone; }, [](int v) { return voice::set_vtone(v); })) {}
+    else if (knob_cmd(cmd, "nodelay", 0, 1, "0|1", [] { return relaylink::nodelay() ? 1 : 0; }, [](int v) { return relaylink::set_nodelay(v != 0) >= 0; })) {}
+    else if (cmd == "mic" || cmd.rfind("mic ", 0) == 0) {      // PL3c-D9: 마이크 증폭 12~24dB(3dB 단위). 코덱 쓰기가 실패할 수 있어 응답이 셋이다(vol 과 같은 모양)
+        int n = 0;
+        if (cmd == "mic") printf("OK mic %d\n", audioout::mic_gain());
+        else if (!voicecore::parse_uint(cmd.c_str() + 4, voicetune::MIC_DB_MIN, voicetune::MIC_DB_MAX, &n) || !voicetune::ok_mic_db(n)) printf("ERR mic 12-24 step 3\n");
+        else {
+            const audioout::VolResult r = audioout::set_mic_gain(n);
+            if (r == audioout::VolResult::OK) printf("OK mic %d\n", audioout::mic_gain());
+            else printf(r == audioout::VolResult::NO_AUDIO ? "ERR mic no-audio\n" : "ERR mic i2c\n");
+        }
+    }
     else printf("ERR unknown\n");
     fflush(stdout);
 }
```

- [x] **Step 4: 통과를 확인한다**

Run: 도구 pytest → `237 passed`. 맥 시험(`T="voice_tune tx_dyn"`) → 전부 그대로(16·105·61·9·91·89·37·90·158·17·34, 경고 0줄). `python3 $P/../verify/freeze_tune.py ~/tab5-toy-phone/firmware/tab5_phone/main/voice_tune.h --expect legacy` → `기대와 같음: legacy`.
(사본 빌드: `0x28a390`, 우리 코드 경고 0, `dependencies.lock` 그대로.)

- [x] **Step 5: 커밋**

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/tools/test_phone_ctl.py firmware/tab5_phone/main/voice.h firmware/tab5_phone/main/voice.cpp firmware/tab5_phone/main/audio_out.h firmware/tab5_phone/main/audio_out.cpp firmware/tab5_phone/main/relay_link.h firmware/tab5_phone/main/relay_link.cpp firmware/tab5_phone/main/net_wifi.h firmware/tab5_phone/main/net_wifi.cpp firmware/tab5_phone/main/phone_main.cpp && git commit -m "feat(phone): 통화 품질 손잡이 연결 — lvl·tilt·res·mic·opus·jb·nodelay·vtone·tune, 새 줄 TUNE(STAT 그대로), 부팅 값은 5935ddd 와 같다 (PL3c-D3~D9)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 6: 1차 굳히기(`lvl` 0 → 12), 지금 기기의 빌드 보관, 새로 빌드

> **실행됨(2026-10-05, 구현 에이전트 — 보고 `chunk1-impl-report.md`)**: 커밋 `771b9fe`, 빌드 `771b9fe`(`-dirty` 없음) 2,664,288 B = `0x28a760`·58% 여유·경고 0·`dependencies.lock` 불변, 검사합 앞 12자 `74a683983fb6`, 보관본 `out/fw_prev_5935ddd/build/` 확인. 실패 → 통과의 문구가 아래 Expected 와 같았다.

**Files:**
- Modify: `firmware/tab5_phone/main/voice_tune.h`(한 줄)
- Create(git 제외): `firmware/tab5_phone/out/fw_prev_5935ddd/build/`, `out/p3q_build.log`, `out/p3q_fw_build.sha256`, `out/p3q_fw_version.txt`

**Interfaces:**
- Consumes: Task 1~5 의 커밋 다섯, 저장소의 `build/`(지금 두 기기에 올라가 있는 `5935ddd` 빌드), `out/p3d_fw_build.sha256`.
- Produces: Task 7 이 올릴 `build/` 와 그 검사합, 롤백용 `out/fw_prev_5935ddd/build/`.

- [x] **Step 1: 굳힌다**(PL3c-D12)

```bash
cd ~/tab5-toy-phone && git apply $P/T6-01-first-freeze.patch && python3 $P/../verify/freeze_tune.py firmware/tab5_phone/main/voice_tune.h --expect lvl=12 | tail -1
```
```diff
--- a/firmware/tab5_phone/main/voice_tune.h
+++ b/firmware/tab5_phone/main/voice_tune.h
@@ -19,7 +19,7 @@
 constexpr int DEFAULT_VOL     = 100;    // 코덱 재생 음량(%)
 constexpr int DEFAULT_MIC_DB  = 24;     // 마이크 증폭(dB)
 constexpr int DEFAULT_KBPS    = 24;     // Opus 비트율(kbps)
-constexpr int DEFAULT_LVL     = 0;      // 보내는 크기 평탄화: 0 = 끔(고정 이득), 6~16 = 말소리에 거는 이득의 상한(배)
+constexpr int DEFAULT_LVL     = 12;      // 보내는 크기 평탄화: 0 = 끔(고정 이득), 6~16 = 말소리에 거는 이득의 상한(배)
 constexpr int DEFAULT_TILT    = 0;      // 보내는 소리의 고음 올리기: 0 = 끔, 1 = 조금(4kHz +4dB), 2 = 많이(4kHz +7dB)
 constexpr int DEFAULT_RES_DB  = 0;      // 남은 메아리 누르기(dB): 0 = 끔
 constexpr int DEFAULT_JB      = 0;      // 지터 버퍼: 0 = 3b 의 규칙 그대로, 3~10 = 새 규칙 + 목표 상한(프레임)
```
Expected: `기대와 같음: lvl=12`. 맥 시험(`T="voice_tune tx_dyn"`)·도구 pytest(`237 passed`)는 그대로다(시험은 부팅 값 자체를 고정하지 않는다 — 모양과 범위만).

- [x] **Step 2: 1덩어리의 파일을 초안과 대조하고 커밋한다**

```bash
cd ~/tab5-toy-phone && for f in main/voice_tune.h main/jitter_buffer.h main/audio_out.h main/relay_link.h main/relay_link.cpp main/net_wifi.h main/net_wifi.cpp main/phone_main.cpp tools/test_voice_tune.cpp; do cmp firmware/tab5_phone/$f $P/../files/firmware/tab5_phone/$f || echo "다름: $f"; done; echo "대조 끝"
```
Expected: `대조 끝` 한 줄뿐(`다름:` 이 없다). Task 6b 가 더하는 시험 파일(`test_jitter_buffer.cpp`·`test_tx_dyn.cpp`)과 2덩어리가 더 고치는 파일(`voice.h`·`voice.cpp`·`voice_core.h`·`tx_dyn.h`(주석)·`audio_out.cpp`·`call_fsm.h`·`app.cpp`·`test_voice_core.cpp`·`test_call_fsm.cpp`·`test_phone_ctl.py`)은 여기서 대조하지 않는다.
```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/main/voice_tune.h && git commit -m "feat(phone): 1차 굳히기 — 부팅 값 lvl 0→12(말소리 평탄화 켬). 사용자 판정 \"먹먹하게 작은 소리로, 아주 멀리있는 것처럼 들림\" (PL3c-D12)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

**여기서 1덩어리의 검수가 들어간다**(Task 1~6 의 커밋 여섯). — 실제로는 속도를 위해 검수와 Step 3~5·Task 7 의 조용한 점검을 **동시에** 돌렸고, 스피커를 켜는 것만 검수 판정 뒤로 뒀다(컨트롤러 판정 P3C-R3). 검수 결과: Spec 통과 · Critical 0 · Important 0 · Minor 8(`chunk1-review.md` — 처리는 아래 "2덩어리"의 표).

- [x] **Step 3: 지금 기기에 올라가 있는 빌드를 보관한다**(다음 Step 이 `build/` 를 덮어쓴다)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && grep '"project_version"' build/project_description.json && shasum -a 256 -c out/p3d_fw_build.sha256 && mkdir -p out/fw_prev_5935ddd/build/bootloader out/fw_prev_5935ddd/build/partition_table && cp build/flash_args build/tab5_phone.bin out/fw_prev_5935ddd/build/ && cp build/bootloader/bootloader.bin out/fw_prev_5935ddd/build/bootloader/ && cp build/partition_table/partition-table.bin out/fw_prev_5935ddd/build/partition_table/ && shasum -a 256 out/fw_prev_5935ddd/build/tab5_phone.bin | cut -c1-12 && git status --short | grep -c "fw_prev"
```
Expected: `    "project_version":    "5935ddd",`, `OK` 3줄, `3f7f271db840`, `0`. **버전이 `5935ddd` 가 아니거나 검사합이 `FAILED` 면 멈춘다(BLOCKED)** — 누가 그 뒤에 다시 빌드한 것이라 보관본이 "기기에 올라간 것"이라는 보장이 없다(아무것도 지우지 말고 보고한다).

- [x] **Step 4: 전체 빌드**(export 와 빌드를 같은 호출에, `nice` 를 붙여서)

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && shasum -a 256 dependencies.lock | cut -c1-64 > /tmp/p3q_lock_before.txt && source ~/esp/esp-idf-5.5.4/export.sh > /dev/null 2>&1 && nice -n 10 idf.py build > out/p3q_build.log 2>&1; echo "rc=$?"; grep -E "tab5_phone.bin binary size|Project build complete" out/p3q_build.log; echo "우리 코드 경고: $(grep 'warning:' out/p3q_build.log | grep -c 'tab5_phone/main/')"; echo "오류: $(grep -c 'error:' out/p3q_build.log)"; shasum -a 256 dependencies.lock | cut -c1-64 | cmp - /tmp/p3q_lock_before.txt && echo "dependencies.lock 그대로"
```
Expected: `rc=0`, `tab5_phone.bin binary size 0x28a… bytes. Smallest app partition is 0x600000 bytes. 0x375… bytes (58%) free.`(사본 `0x28a390` — 저장소는 `0x28a000~0x28b000` 사이면 정상), `Project build complete. To flash, run:`, `우리 코드 경고: 0`, `오류: 0`, `dependencies.lock 그대로`. `rc` 가 0 이 아니면 `grep -m5 "error:" out/p3q_build.log` 를 보고에 붙이고 멈춘다(코드를 고치지 않는다).

- [x] **Step 5: 검사합과 버전을 적어 둔다**

```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && shasum -a 256 build/tab5_phone.bin build/bootloader/bootloader.bin build/partition_table/partition-table.bin > out/p3q_fw_build.sha256 && grep '"project_version"' build/project_description.json | tee out/p3q_fw_version.txt && git log --oneline -1 && cmp -s build/tab5_phone.bin out/fw_prev_5935ddd/build/tab5_phone.bin; echo "보관본과 다른가(1 이어야 한다): $?"
```
Expected: 버전 한 줄(바로 아래 `git log` 의 SHA 와 같다. `-dirty` 가 붙으면 `git status --short` 를 보고에 적는다), `보관본과 다른가(1 이어야 한다): 1`. **이 뒤로 Task 8 이 끝날 때까지 다시 빌드하지 않는다.**

### Task 6b: 시험 보강 — 변이 시험과 검수가 드러낸 구멍 (시험만 추가, 구현은 그대로)

> **실행됨(2026-10-05 10:1x, 구현 에이전트 — 컨트롤러의 Task C3)**: 커밋 `d3da36f`(시험 파일 둘, +97줄). 작성자 확인: 저장소의 두 시험 파일이 `D/files` 와 같고, 저장소의 펌웨어 `main`·`tools` 전체가 사본의 Task 6b 단계와 같다. 기기의 빌드는 `771b9fe` 그대로다(다시 빌드하지 않았다).

**Files:**
- Test: `firmware/tab5_phone/tools/test_jitter_buffer.cpp`(시험 2개 추가 → 92), `firmware/tab5_phone/tools/test_tx_dyn.cpp`(시험 12개 추가 → 46)

**Interfaces:**
- Consumes: 저장소에 들어가 있는 `jitter_buffer.h`·`tx_dyn.h`(Task 2·4 — `771b9fe` 그대로).
- Produces: 없음. 펌웨어는 한 바이트도 바뀌지 않는다 → **다시 빌드·플래시하지 않는다**(기기 과제·듣기와 겹쳐도 된다. 다만 커밋이 하나 늘어 다음 빌드의 버전 문자열은 달라진다).

왜 따로인가: 1덩어리의 패치를 넘긴 뒤에 순수 헤더를 일부러 망가뜨리는 시험(83가지 — 작업 폴더의 `mutate3c.py`)을 돌렸더니 1덩어리의 시험이 **11가지를 잡지 못했다**: 상대가 말하는 동안에도 말소리 이득을 건다 / 메아리로 말소리 크기를 배운다 / 이득이 한 번에 뛴다 / 이득을 묶음 끝에서야 바꾼다 / 무릎 위를 그냥 자른다 / 큰 소리를 바로 따라가지 않는다 / 남은 메아리 어림이 내려가지 않는다 / 내 말 판정의 여유가 없다(이상 `tx_dyn.h`) / 거듭 끊김 상태가 풀리지 않는다 / 거듭 끊기는 동안에도 새 규칙으로 버린다(이상 `jitter_buffer.h`) + 변이 정의 하나의 자리 오류. 시험 12개를 더해 83가지를 전부 잡는다(`logs/mutate3c.txt` → `mutate3c_c.txt`). 여기에 1덩어리 검수의 Minor 1·6(상대 소리가 시작되는 첫 묶음 **안**의 이득을 표본마다 본 시험이 없다)을 시험 2개로 고정했다. **구현은 고치지 않았다** — 이 시험들은 지금의 코드에서 통과한다(그래서 "실패 먼저"가 없다. 대신 위의 변이들이 이 시험들을 실패시킨다).

- [x] **Step 1: 적용**

```bash
cd ~/tab5-toy-phone && git apply $P/T6b-01-more-tests.patch
```
```diff
--- a/firmware/tab5_phone/tools/test_jitter_buffer.cpp
+++ b/firmware/tab5_phone/tools/test_jitter_buffer.cpp
@@ -415,6 +415,20 @@
         CHECK("burst rule: once underruns are more than 1 s apart again, the knob's ceiling is back (no further rise)", hold == 10 && t.target_frames() == 10);
         steady(t, q, 6 * 250 + 10);
         CHECK("burst rule: ... and 5 s of calm at a time brings it back down under the knob", t.target_frames() <= 4);
+    }
+    {
+        // 거듭 끊기는 동안(마지막 끊김 뒤 1초)에는 깊어져도 3b 처럼 기다린다 — 0.5초 만에 버리기 시작하지 않는다. 끊김이 뜸해지면 새 규칙으로 돌아온다
+        Buffer t; t.set_policy(tuned_policy(4)); uint16_t q = prime(t, 0);
+        for (int i = 0; i < 6; ++i) { steady(t, q, 10); underrun(t, q); }                           // 0.2초마다 끊긴다 → 거듭 끊기는 망
+        for (int i = 0; i < 4; ++i) put(t, q++);                                                    // 뭉쳐 온 것(다음 0.3초치)
+        const uint32_t before = t.stats().catchup_drop + t.stats().overflow_drop;
+        steady(t, q, 40);
+        CHECK("burst rule: while underruns keep coming, a deep buffer is left alone for 0.8 s (that surplus is the next clump's supply)", t.stats().catchup_drop + t.stats().overflow_drop == before);
+        steady(t, q, 300);                                                                          // 6초 무사 → 거듭 끊김이 풀렸다
+        for (int i = 0; i < 4; ++i) put(t, q++);                                                    // 목표보다 3프레임 깊다(300ms 는 넘지 않는다)
+        uint16_t last = 0; bool have = false; int big = 0;
+        for (int i = 0; i < 80; ++i) { put(t, q++); const Pop p = t.pop(); if (have) big = std::max(big, (int)(uint16_t)(p.seq - last)); last = p.seq; have = true; }
+        CHECK("burst rule: once the underruns have stopped, the tuned catch-up is back (one frame at a time, not the whole surplus in one jump)", big == 2 && t.depth() - t.target_frames() < 2);
     }
     {
         // ③ 깊어졌을 때: 3b 는 1초 뒤 한꺼번에(큰 건너뜀 한 번), 새 규칙은 0.5초 지켜본 뒤 0.1초마다 한 프레임
--- a/firmware/tab5_phone/tools/test_tx_dyn.cpp
+++ b/firmware/tab5_phone/tools/test_tx_dyn.cpp
@@ -87,6 +87,8 @@
         }
         CHECK("soft clip: untouched up to the knee (20000), never beyond +-32767, never changes sign", bounded && soft_clip(KNEE) == KNEE && soft_clip(-KNEE) == -KNEE && soft_clip(12345) == 12345 && soft_clip(0) == 0);
         CHECK("soft clip: monotonic and with no step anywhere (slope <= 1) — it squeezes, it does not cut", mono && smooth && soft_clip(1 << 30) <= CEIL && soft_clip(60000) > 28000);
+        CHECK("soft clip: above the knee it keeps rising all the way (no flat top): 32767 -> 26383, 40000 < 60000 < the ceiling",
+              soft_clip(32767) == 26383 && soft_clip(-32767) == -26383 && soft_clip(40000) < soft_clip(60000) && soft_clip(60000) < CEIL && soft_clip(200000) < CEIL);
         g_seed = 5; Dyn d; bool safe = true; long n = 0; int32_t before = 0;
         for (int t = 0; t < 40000 && safe; ++t) {
             int16_t x[BLOCK], y[BLOCK]; const int kind = (int)rnd(5); const int amp = kind == 0 ? 32767 : kind == 1 ? 20 : 1 + (int)rnd(32767);
@@ -183,6 +185,70 @@
         CHECK("a steady tone (amplitude 500, like the 'probe' test tone): levelled to the knob's ceiling (x12) within 2 s and still there at 4 s", at2s == 12 * 256 && at4s == 12 * 256);
         CHECK("... and learned as background noise within 12 s — a steady hum is not boosted for ever", at12s == 1024);
     }
+    // ── 변이 시험이 찾은 구멍을 막는 시험들(이득이 옮겨 가는 모양, 상대가 말하는 동안의 상한, 남은 메아리 어림) ──
+    {
+        // 이득은 묶음 "안에서" 고르게 옮겨 간다: 갑자기 커진 소리(값이 한결같은 20000)에서 이웃 표본 사이에 계단이 없다
+        Dyn d; const Params lvl = {1024, 16, 0, 0}; Pcm out;
+        for (int b = 0; b < 4; ++b) { int16_t blk[BLOCK]; for (int i = 0; i < BLOCK; ++i) blk[i] = 20000; d.process(blk, 0, lvl); out.insert(out.end(), blk, blk + BLOCK); }
+        int jump = 0; for (size_t i = 1; i < out.size(); ++i) jump = std::max(jump, std::abs(out[i] - out[i - 1]));
+        CHECK("the gain glides inside the block: a level that jumps to 20000 comes down with no step between neighbouring samples (not one block late, all at once)", jump <= 300 && out[BLOCK] <= 20100 && out[0] > 28000);
+        // 다시 말하기 시작할 때: 기억한 이득(16배)으로 한 번에 뛰지 않고 한 묶음에 1/4 씩
+        Dyn e; std::vector<int> g;
+        run(e, lvl, 5, [](long i) { return speech_at(i, 600); }, [](long) { return 0u; });
+        run(e, lvl, 1, [](long) { return (int16_t)0; }, [](long) { return 0u; });
+        const int rest = e.gain_q8();
+        run(e, lvl, 0.2, [](long i) { return speech_at(i + 800, 600); }, [](long) { return 0u; }, &g);
+        bool quarter = g[0] <= rest + rest / 4 + 1; for (size_t i = 1; i < g.size(); ++i) if (g[i] > g[i - 1] + g[i - 1] / 4 + 1) quarter = false;
+        CHECK("speech starting again: the remembered gain comes back a quarter per block, never in one jump", rest == 1024 && quarter && g.back() > 3000);
+    }
+    {
+        // 내 작은 말로 16배를 배운 뒤 상대가 말하기 시작한다(나는 계속 말한다): 이득은 한 묶음 안에 고정 이득 아래로 — 고리 이득이 3b 보다 커지지 않는다
+        Dyn d; const Params lvl = {1024, 16, 0, 0}; std::vector<int> g;
+        run(d, lvl, 5, [](long i) { return speech_at(i, 600); }, [](long) { return 0u; });
+        const int learned = d.gain_q8();
+        run(d, lvl, 2, [](long i) { return speech_at(i, 600); }, [](long) { return 1400u; }, &g);
+        CHECK("I learned x16 on my quiet speech, then the far end starts while I keep talking: the gain is at or under the fixed gain from the first block on",
+              learned > 3500 && *std::max_element(g.begin(), g.end()) <= 1024);
+        g.clear(); run(d, lvl, 1, [](long i) { return speech_at(i, 600); }, [](long) { return 0u; }, &g);
+        CHECK("... and when the far end stops (after the 0.2 s reverb hold) my learned gain is still there", g[10] <= 1024 && g[20] > 3500 && g.back() > 3500);
+    }
+    {
+        // 상대 소리가 나는 동안 들어온 소리로는 "말소리의 크기"를 배우지 않는다: 자리 잡지 못한 메아리(꼭대기 800 — 내 말로 보일 만큼 크다)가 2초 들어와도, 그 뒤 내 작은 말의 이득은 고정 이득에서 천천히 오른다
+        Dyn d; const Params lvl = {1024, 16, 0, 0}; std::vector<int> g;
+        run(d, lvl, 2, [](long i) { return tone_at(i, 800, 700); }, [](long) { return 1400u; });
+        run(d, lvl, 0.4, [](long) { return (int16_t)0; }, [](long) { return 0u; });
+        run(d, lvl, 0.2, [](long i) { return speech_at(i + 800, 300); }, [](long) { return 0u; }, &g);
+        CHECK("sound heard while the far end talks does not teach the leveller: 2 s of unconverged echo, then my quiet speech still starts near the fixed gain (x4), not at x16", g[9] < 1536);
+        // 상대가 말하는 중의 내 큰 소리: 바로 따라가 이득을 내린다(고정 이득에서도 넘치므로)
+        Dyn e;
+        const Pcm loud = run(e, lvl, 1, [](long i) { return speech_at(i, 12000); }, [](long) { return 1400u; });
+        CHECK("a loud voice during the far end's speech still pulls the gain down at once (peaks near the target, not squeezed against the ceiling)", e.gain_q8() < 512 && peak(loud, RATE / 2) <= KNEE + 2500);
+    }
+    {
+        // 남은 메아리의 어림은 실제 비율로 내려간다: 메아리가 1% 뿐인 기기에서는 상대가 말하는 중의 내 작은 말(꼭대기 400)도 내 말로 알아본다
+        const Params both = {1024, 16, 12, 0};
+        Dyn d, l; auto echo = [](long i) { return tone_at(i, 20, 1000); };
+        run(d, both, 2, echo, [](long) { return 1400u; });
+        const Pcm a = run(d, both, 3, [&](long i) { return (int16_t)(speech_at(i, 400) + echo(i)); }, [](long) { return 1400u; });
+        const Pcm b = run(l, OFF, 3, [&](long i) { return (int16_t)(speech_at(i, 400) + echo(i)); }, [](long) { return 1400u; });
+        double sa = 0, sb = 0; for (size_t i = 0; i < a.size(); ++i) if ((long)i % (RATE * 3 / 10) > 1000 && (long)i % (RATE * 3 / 10) < 2200) { sa += (double)a[i] * a[i]; sb += (double)b[i] * b[i]; }
+        CHECK("double talk with a quiet voice (peak 400) on a device whose leftover echo is only 1 %: still within 1.5 dB of the fixed gain (the echo estimate follows the real ratio down)",
+              10 * std::log10((sa + 1) / (sb + 1)) > -1.5);
+        // 상대 소리가 아주 클 때(되먹임 6000)의 남은 메아리 4%(RMS 170)는 바닥 잡음 문턱보다 크다 — 그래도 메아리로 보고 누른다(어림의 2.5배가 문턱)
+        Dyn e, m; auto big = [](long i) { return tone_at(i, 240, 1000); };
+        const Pcm c = run(e, both, 3, big, [](long) { return 6000u; });
+        const Pcm f = run(m, OFF, 3, big, [](long) { return 6000u; });
+        CHECK("a loud far end (loopback 6000) leaves an echo well above the noise gate: it is still pressed by 12 dB (the margin is 2.5x the estimate, not the noise floor)",
+              db(rms(c, RATE), rms(f, RATE)) < -11.0 && e.suppressed() > 100);
+    }
+    {
+        // 상대 소리가 없으면 누르지 않는다(res 만 켠 채 조용한 방): 고정 이득 그대로
+        g_seed = 21; Dyn d, l; const Params res = {1024, 0, 18, 0};
+        const Pcm a = run(d, res, 2, [](long) { return noise_at(12); }, [](long) { return 0u; });
+        g_seed = 21; const Pcm b = run(l, OFF, 2, [](long) { return noise_at(12); }, [](long) { return 0u; });
+        CHECK("no far-end sound -> nothing is pressed, however quiet my side is (res 18)", a == b && d.suppressed() == 0);
+    }
+
     // ── 상대가 말하는 동안: 고정 이득을 넘지 않는다, 남은 메아리는 누른다 ──
     double pressed_db = 0, dt_loss_db = 0;
     {
@@ -243,6 +309,23 @@
         CHECK("300 thousand random blocks: the gain always stays between the deepest press and x16", sane);
     }
 
+    {
+        // 1덩어리 검수 Minor 1·6: 위의 시험들은 "묶음 끝의 이득"만 본다. 여기서는 묶음 안을 표본마다 본다(입력을 256 으로 고르게 주면 나간 표본 = 그 표본에 걸린 이득(Q8)이다).
+        //   상대 소리가 시작되는 첫 묶음(16ms)은 배운 이득에서 고정 이득으로 "내려오는 중"이라 그 안의 표본에는 고정 이득보다 큰 이득이 걸린다 — 오르는 표본은 없고, 묶음 끝에서 고정 이득에 닿는다.
+        //   둘째 묶음부터는 어느 표본도 고정 이득을 넘지 않는다(고리 이득이 3b 보다 클 수 있는 것은 길어야 그 16ms 다).
+        Dyn d; const Params lvl = {1024, 12, 0, 0};
+        run(d, lvl, 5, [](long i) { return speech_at(i, 600); }, [](long) { return 0u; });
+        const int learned = d.gain_q8();
+        int16_t x[BLOCK]; for (int i = 0; i < BLOCK; ++i) x[i] = 256;
+        d.process(x, 1400, lvl);
+        bool falling = x[0] <= learned; for (int i = 1; i < BLOCK; ++i) if (x[i] > x[i - 1]) falling = false;
+        CHECK("the far end starts while my gain is x12: inside that one block (16 ms) the gain only comes down, sample by sample, and ends at the fixed gain",
+              learned > 2500 && falling && x[BLOCK - 1] == 1024 && d.gain_q8() == 1024);
+        bool under = true;
+        for (int b = 0; b < 60; ++b) { for (int i = 0; i < BLOCK; ++i) x[i] = 256; d.process(x, 1400, lvl); for (int i = 0; i < BLOCK; ++i) if (x[i] > 1024) under = false; }
+        CHECK("... and from the second block on no sample is above the fixed gain while the far end is heard", under);
+    }
+
     if (info) {
         printf("\n-- 크기(맥 시험, 계획서 표) --\n");
         printf("작은 말(에코 제거 뒤 꼭대기 600) · lvl 16: 고정 4배보다 %+.1f dB\n", quiet_gain_db);
```

- [x] **Step 2: 통과를 확인한다**

Run: 맥 시험(`T="voice_tune tx_dyn"`)
Expected: `jitter_buffer: 92 ok, 0 failed`, `tx_dyn: 46 ok, 0 failed`, 나머지 그대로(16·105·61·9·91·89·37·158·17), 경고 전부 0줄. 이어서 `cd ~/tab5-toy-phone && for f in tools/test_jitter_buffer.cpp tools/test_tx_dyn.cpp; do cmp firmware/tab5_phone/$f $P/../files/firmware/tab5_phone/$f || echo "다름: $f"; done; git status --short` → `다름:` 없음, 바뀐 파일은 그 둘뿐.

- [x] **Step 3: 커밋**

```bash
cd ~/tab5-toy-phone && git add firmware/tab5_phone/tools/test_jitter_buffer.cpp firmware/tab5_phone/tools/test_tx_dyn.cpp && git commit -m "test(phone): 3c 시험 보강 — 변이 시험이 못 잡던 11가지(평탄화 9·지터 2)와 상대 소리가 시작되는 첫 묶음 안의 이득(1덩어리 검수 Minor 1·6). 구현 불변 (PL3c-D5·D7)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 7: 기기 검증 A — 두 대에 올리고, **스피커를 끈 채로** 확인한다

**Files:**
- Create(git 제외): `firmware/tab5_phone/out/p3q_*.log`, `out/p3q_flash_unit*.txt`

**Interfaces:**
- Consumes: Task 6 의 `build/`·`out/p3q_fw_build.sha256`·`out/fw_prev_5935ddd/`, `tools/phone_ctl.py`, `D/verify/voice_check.py`·`tune_report.py`·`touch_audit.py`, 켜져 있는 개발용 교환기(바꾸지 않는다), 3b 계획서 Task 8 의 Step 6·7·8 명령(회귀용).
- Produces: 부팅 줄·`TUNE`·메모리, 옛 동작 회귀(같은 바이너리에 `lvl 0`), 평탄화가 기기에서 도는 기록, (시간이 되면) 주파수별 측정·`nodelay`·`jb` 의 비교.

> **실행됨(2026-10-05 09:37~09:46, 기기 에이전트 — 보고 `chunk1-device-report.md`, 산출물 `out/p3q_*`)**: R0~R5 통과(멈춤 조건 0회), R6(스피커 켜기)은 컨트롤러가 검수 판정 뒤에. 숫자는 끝의 "진행 로그 — 1덩어리 실행 결과". **남은 것: 아래 측정 M1~M4.**

아래 R0~R7 은 인계문(`chunk1-handoff.md`) 4절 그대로다. 읽는 법: "0번 보관"·"펌웨어 빌드 명령" = 이 계획의 Task 6 Step 3·4, "5절의 롤백" = 위 "롤백", "6절" = Task 8, `$W` = 작업 폴더(`~/tab5-toy-phone/.superpowers/sdd/2026-10-05-tab5-phone-phase3c-quality`), `C=$W/draft/verify`. R3 의 "`stk_vcap` 약 30,000"은 **약 26,600 이 맞다**(아래 결과 — 부팅 줄 뒤의 `selftest` 가 에코 제거를 한 번 돌린 뒤의 최저값이다: `5935ddd` 의 같은 시점 42,964 − 16,384 = 26,580).

좌표: 통화 `640,360` · 받기 `404,428` · 끊기 `644,606`. 모든 명령은 export 하지 **않은** 호출에서(플래시만 export). 산출물 접두어 `out/p3q_`. `C=$W/draft/verify`.
**시작 조건**: 사용자에게 알리고(아이가 통화 중일 수 있다 — 두 기기 스피커가 켜져 있다), 두 기기가 `fsm=IDLE` 인 것을 본 뒤에. **올리려면 두 기기가 모두 맥북에 꽂혀 있어야 한다** — 어느 쪽이 보이는지는 가정하지 않는다:

- **R0 누가 보이나·교환기·대기 상태**
  ```bash
  cd ~/tab5-toy-phone/firmware/tab5_phone && for u in 1 2; do python3 tools/phone_ctl.py port $u > /dev/null 2>&1 && echo "${u}번: 보임" || echo "${u}번: 안 보임"; done
  ```
  (`port` 는 포트를 열지 않는다.) 둘 다 `보임` 이 아니면 **사용자에게 "안 보이는 기기를 맥북에 꽂아 주세요"라고 부탁하고 기다린다.** 교환기는 단독 호출 `bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh status` → `relay: listening on port 5094, healthz={"ok": true}`(꺼져 있으면 켜지 말고 멈춘다).
  ```bash
  cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py pair 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1:60' 'both:until:quiet on:^OK quiet on amp=0$:10' 'both:until:stat:^STAT .*fsm=IDLE.* amp=0 :10' > out/p3q_before.log 2>&1; echo "rc=$?"; for u in 1 2; do grep -E "^\[$u\] STAT " out/p3q_before.log | tail -1 | grep -oE '^\[.\]|heap_int=[0-9]+|heap_int_min=[0-9]+|stk_vcap=[0-9]+|stk_vtx=[0-9]+' | tr '\n' ' '; echo; done
  ```
  Expected `rc=0`. `quiet on` 을 **재부팅 전에** 건다(스피커를 켠 채 재부팅하면 "퍽" 소리가 날 수 있다 — 소스 판독, 기기 미확인). 조용한 모드는 NVS 에 남는다. `rc=1`(대기 상태가 아니다 = 누가 쓰는 중)이면 멈춘다.
- **R1 빌드와 검사합**(0번 보관 뒤에만): 위 "펌웨어 빌드" 명령 → `rc=0`, 크기 `0x28a000~0x28b000`(사본 `0x28a390`), `(58%) free`, 경고 0. 이어서
  `cd ~/tab5-toy-phone/firmware/tab5_phone && shasum -a 256 build/tab5_phone.bin build/bootloader/bootloader.bin build/partition_table/partition-table.bin > out/p3q_fw_build.sha256 && grep '"project_version"' build/project_description.json | tee out/p3q_fw_version.txt && git log --oneline -1`. **이 뒤로 듣기가 끝날 때까지 다시 빌드하지 않는다.**
- **R2 2번 → 1번에 올린다**(같은 바이너리, MAC 으로 확인, `idf.py flash` 금지). 2번:
  ```bash
  cd ~/tab5-toy-phone/firmware/tab5_phone && shasum -a 256 -c out/p3q_fw_build.sha256 && source ~/esp/esp-idf-5.5.4/export.sh > /dev/null 2>&1 && PORT=$(python3 tools/phone_ctl.py port 2) && python -m esptool --chip esp32p4 -p "$PORT" read_mac 2>&1 | tee out/p3q_flash_unit2.txt | grep -i "aa:bb:cc:dd:ee:02" && cd build && python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash @flash_args 2>&1 | tee -a ../out/p3q_flash_unit2.txt | grep -E "Hash of data verified|Hard resetting|rror"
  ```
  Expected: `OK` 3줄, `MAC: aa:bb:cc:dd:ee:02`, `Hash of data verified.` 3줄, `Hard resetting via RTS pin...`. (1번은 `port 1`, `aa:bb:cc:dd:ee:01`, `p3q_flash_unit1.txt`.) MAC 줄이 안 나오면 아무것도 쓰이지 않는다 — 멈춘다.
- **R3 부팅 줄·TUNE·메모리**(기기마다. 아래는 2번 — 1번은 `--unit 1`·`unit=1 id=tab5-1`·`p3q_boot_unit1.log`)
  ```bash
  cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py --unit 2 run 'until:stat:^STAT :60' 'reboot' 'wait:^PHONE BOOT unit=2 id=tab5-2:30' 'wait:^AUDIO (ready|FAIL):20' 'wait:^VOICE ready:10' 'until:stat:^STAT .*fsm=IDLE.*relay=1.*audio=1 .* amp=0 :120' 'sleep:12' 'until:tune:^TUNE :10' 'selftest' 'wait:^SELFTEST :5' 'until:stat:^STAT .*fsm=IDLE:10' > out/p3q_boot_unit2.log 2>&1; echo "rc=$?"; grep -E "App version|Compile time|ELF file SHA256|wifi power save|^PHONE (BOOT|FAIL)|^AUDIO|^VOICE ready|^SELFTEST|^TUNE" out/p3q_boot_unit2.log | sed 's/^.*app_init: //; s/^.*netwifi: /netwifi: /'; grep "^STAT" out/p3q_boot_unit2.log | tail -1 | grep -oE 'heap_int=[0-9]+|heap_int_min=[0-9]+|stk_[a-z]+=[0-9]+|amp=[01]' | tr '\n' ' '; echo
  ```
  Expected `rc=0` 와: `netwifi: wifi power save off`, `PHONE BOOT unit=2 id=tab5-2`, `AUDIO ready rate=16000 block=256 vol=100 dma=3 mic=24 amp=0`, `VOICE ready chunk=256 opus_in=640 opus_out_max=160 bitrate=24000 aec=VOIP_HIGH_PERF`, `TUNE dflt=1 legacy=0 meas=1 txgain=4 vol=100 mic=24 kbps=24 lvl=12 tilt=0 res=0 jb=0 nodelay=0 vtone=1000 ps=0 nd_socks=-1 g_q8=0 supp=0 far=0 rxgap=0 mclip=0`, `SELFTEST amp=0 mic=<작은 수> ref=<300~1200> aec=<작은 수> ok=1`.
  **통과 기준**: `ps=0`(−1 이면 10초 뒤 `tune` 을 다시 — 계속 −1 이거나 1·2 면 보고), `stk_vcap` 이 `5935ddd` 의 부팅 값(46,412)보다 16,384 작은 **약 30,000**, `stk_vtx` 약 7,900, `heap_int` 가 `5935ddd` 의 부팅 뒤 대기 값(115,600 안팎)보다 **약 24,000 큰 139,000 안팎**(135,000 미만이면 보고에 적는다 — 메모리를 못 되찾은 것), 두 기기의 `App version`·`Compile time`·`ELF file SHA256` 이 같다. `AUDIO FAIL`·`PHONE FAIL`·`PHONE BOOT` 되풀이·`Guru`·`Backtrace` 가 보이면 **멈추고 5절의 롤백**(1번에는 올리지 않는다).
- **R4 옛 동작 회귀 — 같은 바이너리에 시리얼로 옛 값**(조용한 채): `both:until:lvl 0:^OK lvl 0$:5` → `both:until:tune:^TUNE dflt=0 legacy=1 meas=1 :5` 를 건 뒤, 3b Task 8 의 Step 6(시험음 통화 — 양쪽 `vsrc tone`, 8초)·Step 7(지연 재기 — 상대 `vsrc ref`, 방향마다 `vping` 다섯 번)·Step 8(30초 통화) 명령을 **산출물 이름만 `p3q_r4_*.log` 로 바꿔** 그대로 돌린다(명령 전문: `docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md` Task 8, 좌표·식 그대로 통한다). 판정은 `python3 $C/voice_check.py out/p3q_r4_tone.log --during-call --voice 1 --amp 0 --min-frames 250 --spk 1100:1700 --ref 300:1200`(`check rc=0`, 내부 메모리만 걸리면 3), 지연은 `--pings 700` 으로 가운데 값을 적는다(`5935ddd` 아침 540~573ms, 밤 253~301ms — 망에 따라 다르다. 판정이 아니라 기록), 30초 통화는 `python3 $C/tune_report.py rate out/p3q_r4_call30.log --min-stk-vcap 6000`. **통과 기준**: `v_under=0`, `aud_late=0`, 부하 ≤ 50%, `stk_vcap ≥ 6000`(말소리가 없으니 27,748−16,384 ≈ 11,400 이 예상값), `VOICE on`/`off` 의 짝, 통화 밖 `v_cap` 불변(`--idle-cap`). 끝나면 `both:until:lvl 12:^OK lvl 12$:5`.
- **R5 평탄화가 기기에서 도는가**(조용한 채, 사람 없이 — 1번이 걸고 2번이 받는다. 1번이 `probe` 를 보내고 2번이 받는 크기를 본다)
  ```bash
  cd ~/tab5-toy-phone/firmware/tab5_phone && I='both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE.* amp=0 .* voice=0 :60'; python3 tools/phone_ctl.py pair "$I" 'both:until:lvl 0:^OK lvl 0$:5' \
    '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' 'sleep:2' '2:tap:404:428' 'both:wait:^FSM .* -> IN_CALL acts=0x180 notice=NONE$:10' 'sleep:2' \
    '1:until:vsrc probe:^OK vsrc probe$:5' 'sleep:4' 'both:until:stat:^STAT .*fsm=IN_CALL.* voice=1 :10' '1:until:tune:^TUNE :5' \
    '1:until:lvl 12:^OK lvl 12$:5' 'sleep:4' 'both:until:stat:^STAT .*fsm=IN_CALL.* voice=1 :10' '1:until:tune:^TUNE :5' \
    '2:until:vsrc tone:^OK vsrc tone$:5' 'sleep:3' 'both:until:stat:^STAT .*fsm=IN_CALL.* voice=1 :10' '1:until:tune:^TUNE :5' \
    '1:tap:644:606' '1:wait:^VOICE off$:10' '2:wait:^VOICE off$:10' 'sleep:2' 'both:until:lvl 12:^OK lvl 12$:5' 'both:until:stat:^STAT .*fsm=IDLE.* voice=0 vsrc=aec :15' > out/p3q_r5_level.log 2>&1; echo "rc=$?"; grep -cE '!!' out/p3q_r5_level.log; grep -E "^\[1\] TUNE" out/p3q_r5_level.log | grep -oE 'lvl=[0-9]+|g_q8=[0-9]+|far=[0-9]+|mclip=[0-9]+' | paste -sd' ' -; grep -E "^\[2\] STAT .* voice=1 " out/p3q_r5_level.log | grep -oE 'v_spk=[0-9]+' | paste -sd' ' -; grep -E "^\[1\] STAT .* voice=1 " out/p3q_r5_level.log | grep -oE 'v_mic=[0-9]+|v_aec=[0-9]+' | paste -sd' ' -
  ```
  읽는 법 — 세 구간의 1번 `TUNE` 과 2번 `v_spk`: ① `lvl 0`: `g_q8=0`, 2번 `v_spk` = 1번 `v_aec` × 4 안팎(1번 `v_mic` 는 354 안팎 = 넣은 시험음) ② `lvl 12`: `g_q8` 이 1024 보다 크고 3072 이하, **2번 `v_spk` 가 ①의 1.8배 이상**이고 6000 이하 ③ 2번이 시험음을 보내는 동안(1번의 되먹임 칸에 상대 소리가 잡힌다): 1번 `g_q8 ≤ 1024`, `far` 가 0 보다 크다 = **상대 소리가 나는 동안에는 4배를 넘지 않는다.**
  `rc=0`·`!!` 0줄이 기본. ②가 안 맞고 1번 `v_aec` 가 40 아래면(에코 제거가 고른 시험음을 잡음으로 보고 지운 것 — 닫힌 라이브러리라 미리 알 수 없었다) 결함이 아니라 "시험음으로는 못 잰다"이다 → 값을 적고 R7 의 사람 목소리로 확인한다. `g_q8` 이 3072 를 넘거나 ③에서 1024 를 넘으면 **결함 — 멈춘다.**
- **R6 스피커 켜기**(사용자와 함께, 통화 밖에서, **한 기기씩**): `--unit N run 'until:quiet off:^OK quiet off amp=1$:10' 'selftest' 'wait:^SELFTEST amp=1 :5'` → `mic` 가 조용할 때(R3 의 값)보다 수십 배 크면 스피커·마이크가 산 것(아침: 1번 3280~3857, 2번 1211). 두 기기는 **다른 방**에(같은 방이면 하울링).
- **R7 듣기**(아래 6절) — 통화가 끝날 때마다 `both:tune` 을 받아 `g_q8`·`mclip`·`rxgap`·`supp`·`far` 를 `out/p3q_listen.log` 에 남기고, 통화 중에는 10초마다 `both:stat`. 끝나면 `python3 $C/tune_report.py rate out/p3q_listen.log --min-stk-vcap 6000`.
  **`mclip` 판정**: 통화 1분에 표본은 96만 개다. 1번의 `mclip` 이 1분에 500 을 넘으면(0.05% — Phase 0 에서 메아리 제거가 6dB 나빠진 수준) `1:mic 18` 을 걸고 한 통화 더 해서 비교한다(말하는 사람에게 "내 목소리가 돌아오나요?"를 묻는다).
- 끝나면: 밤에는 `quiet on`(사용자가 원하면), 손잡이는 재부팅하면 부팅 값으로 돌아간다는 것을 사용자에게 알린다.

**측정 M1~M4**(조용한 채, 사람 없이. R5 까지 통과한 뒤 — 시간이 없으면 건너뛰고 Task 8 로 가도 된다. 건너뛴 것은 Task 11 Step 1 에서 한다 — **2026-10-05 에는 건너뛰었다**). 시작 전에 두 기기를 `quiet on` 으로(지금 스피커가 켜져 있다). **`opus 40` 은 캡처 스택을 더 쓸 수 있다**(검수 Minor 7 — 32KB 스택에서 재 본 적 없다, 컨트롤러 판정 P3C-R4 로 이번 회차에는 쓰지 않았다): M1 은 `opus 24` 를 먼저 끝내고, `opus 40` 은 `opus 32` 로 한 번 돌려 `stk_vcap ≥ 6000` 을 본 뒤에만 건다(M1 명령의 `for k in 24 40` 을 `24` → `32` → `40` 순서로 한 번씩, 사이마다 `both:stat` 의 `stk_vcap` 확인. 6000 아래면 거기서 멈춘다). `I='both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE.* amp=0 .* voice=0 :60'`, 통화를 여는 단계 `CALL=('1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' 'sleep:2' '2:tap:404:428' 'both:wait:^FSM .* -> IN_CALL acts=0x180 notice=NONE$:10' 'sleep:2')`, 끝내는 단계 `END=('1:tap:644:606' '1:wait:^VOICE off$:10' '2:wait:^VOICE off$:10' 'sleep:2' 'both:until:stat:^STAT .*fsm=IDLE.* voice=0 vsrc=aec :15')`.

- [ ] **M1 압축이 어느 높이를 깎나**(1번이 시험음을 보내고 2번의 `v_spk` 를 본다. `opus 24` 와 `opus 40`)
  ```bash
  cd ~/tab5-toy-phone/firmware/tab5_phone && C=~/tab5-toy-phone/.superpowers/sdd/2026-10-05-tab5-phone-phase3c-quality/draft/verify; I='both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE.* amp=0 .* voice=0 :60'; CALL=('1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' 'sleep:2' '2:tap:404:428' 'both:wait:^FSM .* -> IN_CALL acts=0x180 notice=NONE$:10' 'sleep:2'); END=('1:tap:644:606' '1:wait:^VOICE off$:10' '2:wait:^VOICE off$:10' 'sleep:2' 'both:until:stat:^STAT .*fsm=IDLE.* voice=0 vsrc=aec :15'); for k in 24 40; do S=(); for hz in 300 500 1000 2000 3000 4000 6000; do S+=("1:until:vtone $hz:^OK vtone $hz\$:5" 'sleep:3' 'both:until:stat:^STAT .*fsm=IN_CALL.* voice=1 :10'); done; python3 tools/phone_ctl.py pair "$I" "1:until:opus $k:^OK opus $k\$:5" "${CALL[@]}" '1:until:vsrc tone:^OK vsrc tone$:5' "${S[@]}" '1:until:vtone 1000:^OK vtone 1000$:5' "${END[@]}" '1:until:opus 24:^OK opus 24$:5' > out/p3q_m1_tone_opus$k.log 2>&1; echo "opus $k rc=$?"; python3 $C/tune_report.py sweep out/p3q_m1_tone_opus$k.log --tx 1 --rx 2; grep -E "^\[1\] STAT .* voice=1 " out/p3q_m1_tone_opus$k.log | tail -1 | grep -oE 'v_enc_us=[0-9/]+'; done
  ```
  읽는 법: 받는 쪽 `v_spk` 가 주파수마다 1414 안팎(1kHz 대비 0dB)이면 압축은 그 높이를 깎지 않는다. 6kHz 가 −3dB 넘게 내려가면 "압축이 고음을 깎는다" — `opus 40` 에서 나아지는지 본다. `v_enc_us`(평균/최대)를 두 비트율에서 적는다(CPU 예산).
- [ ] **M2 에코 제거가 가까운 소리의 어느 높이를 깎나**(`lvl 0` 으로 두고 1번이 `probe` 를 보낸다 — 1번의 `v_aec`÷`v_mic`)
  ```bash
  cd ~/tab5-toy-phone/firmware/tab5_phone && C=~/tab5-toy-phone/.superpowers/sdd/2026-10-05-tab5-phone-phase3c-quality/draft/verify; I='both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE.* amp=0 .* voice=0 :60'; CALL=('1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' 'sleep:2' '2:tap:404:428' 'both:wait:^FSM .* -> IN_CALL acts=0x180 notice=NONE$:10' 'sleep:2'); END=('1:tap:644:606' '1:wait:^VOICE off$:10' '2:wait:^VOICE off$:10' 'sleep:2' 'both:until:stat:^STAT .*fsm=IDLE.* voice=0 vsrc=aec :15'); S=(); for hz in 300 500 1000 2000 3000 4000 6000; do S+=("1:until:vtone $hz:^OK vtone $hz\$:5" 'sleep:3' 'both:until:stat:^STAT .*fsm=IN_CALL.* voice=1 :10'); done; python3 tools/phone_ctl.py pair "$I" 'both:until:lvl 0:^OK lvl 0$:5' "${CALL[@]}" '1:until:vsrc probe:^OK vsrc probe$:5' "${S[@]}" '1:until:vtone 1000:^OK vtone 1000$:5' "${END[@]}" 'both:until:lvl 12:^OK lvl 12$:5' > out/p3q_m2_probe.log 2>&1; echo "rc=$?"; python3 $C/tune_report.py sweep out/p3q_m2_probe.log --tx 1 --rx 2
  ```
  읽는 법: `aec/mic` 칸이 주파수마다 0dB 안팎이면 에코 제거는 가까운 소리를 그대로 통과시킨다(먹먹함의 원인이 아니다). 높은 쪽만 크게 내려가면(예: 4kHz −6dB) "에코 제거가 고음을 깎는다" → `tilt` 가 보완책. 전부 −20dB 아래면 에코 제거가 고른 시험음을 잡음으로 지우는 것 — 이 방법으로는 못 잰다(그렇게 적는다).
- [ ] **M3 `nodelay` 의 효과**: 3b 계획서 Task 8 Step 7(지연 재기 — 상대 `vsrc ref`, 방향마다 `vping` 다섯 번) 명령을 `out/p3q_m3_ping_nd0.log` 로 한 번, 맨 앞에 `'both:until:nodelay 1:^OK nodelay 1$:5'` 를 넣고 `out/p3q_m3_ping_nd1.log` 로 한 번(끝에 `'both:until:nodelay 0:^OK nodelay 0$:5'`). 각각 `python3 $C/voice_check.py <로그> --pings 700`. 두 번째 실행의 `both:tune` 에 `nodelay=1 … nd_socks=1` 이 보여야 한다(0 이면 소켓을 못 찾은 것 — 보고). **기록할 것**: 방향별 왕복 가운데 값, `v_txdrop`·`v_txfail`, `rxgap`. 가운데 값이 10ms 넘게 줄고 `v_txfail=0` 이면 Task 11 에서 `nodelay=1` 로 굳힐 후보.
- [ ] **M4 `jb` 의 효과**: 3b Task 8 Step 8(30초 통화)의 `sleep` 을 60 으로 늘린 통화를 `jb 0`(`out/p3q_m4_jb0.log`)과 `both:until:jb 5:^OK jb 5$:5` 뒤(`out/p3q_m4_jb5.log`)로 한 번씩, 통화 중 10초마다 `both:stat`. `python3 $C/tune_report.py rate <로그>` 로 끊김/분·튼 비율·`jb_target` 범위·`rxgap` 을 나란히 적는다. 끝나면 `both:until:jb 0:^OK jb 0$:5`.

**결함이 나오면**: 고치지 않는다. 로그 앞뒤 30줄과 `TUNE`·`STAT` 를 보고에 붙이고 멈춘다. 급하면 `lvl 0`(옛 소리) 또는 위 "롤백".

### Task 8: 기기 검증 B — **사용자와 함께** 듣는다 (컨트롤러가 진행)

**Files:**
- Create(git 제외): `firmware/tab5_phone/out/p3q_listen.log`, `out/p3q_loud_unit*.log`

**Interfaces:**
- Consumes: Task 7 을 통과한 두 기기(조용한 모드), 사용자.
- Produces: 다섯 문항의 답(가 / 나 / 같다)과 그때의 `TUNE`(`g_q8`·`mclip`·`rxgap`·`supp`·`far`)·`STAT` — Task 11 의 굳히기가 쓴다.

> **상태(2026-10-05 10시, 컨트롤러 기록 + 작성자의 로그 판독)**: R6 끝남(두 기기 `SELFTEST amp=1 … ok=1`). **듣기 1회차(09:50~09:52, `out/p3q_listen_ab.log`)는 비교로서 무효다** — 책상의 2번 기기 바로 옆에서 맥북이 읽는 말(`say`, 음량 75)을 말하는 사람 대신 썼는데 마이크에 너무 컸고, 사용자가 헷갈려 통화를 끊고 1번을 껐다(판정 P3C-R5 → R6). **`lvl`·`tilt`·`res` 의 "가/나" 답은 하나도 받지 못했다.** 그 로그가 준 자료(2번 기기, 통화 약 54초):
> - 2번 마이크가 넘쳤다: `mclip` 303 → 525 → 811 → 1989, 끝의 `v_mic` 12,896. 처음의 303 은 상대 소리가 전혀 없을 때(`far=0`) 생겼다 = 바로 옆 맥북 소리가 넘친 것. **말하는 쪽에서 가까이·크게 나는 소리도 `mclip` 을 올린다** — `mclip` 만으로는 "메아리가 넘쳤다"고 읽을 수 없다(아래 Step 3 의 읽는 법). 끝의 811 → 1989 는 상대 소리가 크게 오던 때와 겹친다(그 1초의 `v_mic` 는 `v_ref` 의 5.5배 — 스피커를 켠 `selftest` 에서는 2.2배 — 이고 `v_aec` 는 1074): 맥북 소리인지 2번 제 스피커의 소리인지 가를 수 없다.
> - 평탄화는 큰 입력에서 고정 4배보다 **아래로** 내려갔다(`g_q8` 1024 → 1859 → 1024 → 1753 → 491 — 설계대로: 꼭대기를 16000 에 맞춘다. `5935ddd` 였다면 4배로 잘렸을 소리다).
> - 2번이 1번에게서 받은 소리: 통화 중 세 번의 `STAT` 에서는 `v_spk` 5~28(조용함)이었는데 `far`(상대 소리가 난 묶음)는 0 → 94 → 287 → 404 로 쌓였고 **끝의 1초는 `v_spk` 3837·`v_ref` 2354** — 1번 쪽에서 큰 소리가 왔다. 1번은 USB 에 없어 그쪽 기록이 없다 → **1번의 스피커 소리가 1번 마이크에서 넘쳐 새어 나온 메아리인지, 1번 앞의 사람·손 소리인지는 이 로그로 가를 수 없다**(컨트롤러의 추정은 앞쪽).
> - 망이 나빴다(지터 버퍼 목표가 상한 10, 끊김 13회 = 분당 14, 메움 1.0%·버림 2.7%, 받은 프레임 사이 가장 긴 틈 1.5초): `heap_int_min` 121,751 → **106,859**(기준 100,000 위 — 예산의 "아침 같은 몰림" 예상 104,800 과 맞는다). `stk_vcap` 최저는 큰 말소리 + 평탄화에서도 **8,820 그대로**, `v_under=0`·`aud_late=0`, 압축 평균 4.3ms·최대 8.2ms.
> - 다음(판정 P3C-R6): 합성 음성의 가/나 를 그만두고 **평소처럼 한 통화 + 한 줄 답**("아침보다 잘 들리는가 / 내 목소리가 돌아오거나 삐 소리가 나는가"). 답이 없으면 부팅 값은 지금 그대로다.

- [x] **Step 1: 스피커를 켠다**(Task 7 의 R6 — 한 기기씩, 통화 밖에서, 두 기기를 다른 방에)
- [ ] **Step 2: 듣는다**(아래 질문지 — 컨트롤러가 읽어 준다)
- [ ] **Step 3: 숫자를 남긴다**(Task 7 의 R7 — 통화마다 `tune`, `tune_report.py rate`, `mclip` 판정)
  **`mclip` 을 읽는 법(R7 의 "1번의 `mclip` 이 1분에 500 을 넘으면"을 이렇게 고쳐 읽는다)**: `mclip` 은 그 기기의 마이크가 꼭대기에 닿은 표본 수일 뿐 원인을 가리지 않는다. ① **그 기기 쪽 사람이 말하는 동안** 올랐다 = 가까이서 크게 말한 것(메아리와 무관 — 평탄화가 이득을 내려 보낸다. 1회차 2번의 처음 303 이 이 경우다 — `far=0`). ② **그 기기 쪽은 조용하고 상대만 말하는 동안** 올랐다(`far` 가 함께 오르고, `v_mic` 가 `v_ref` 의 여러 배) = 제 스피커 소리에 마이크가 넘친 것 → 메아리가 지워지지 않고 상대에게 돌아간다. **②만이 `mic 18` 의 근거다.** 그래서 `mclip` 은 늘 `v_mic`·`v_ref`·`far`, 그리고 "그때 어느 쪽이 말했나"와 함께 적는다.
- [ ] **Step 4: 1번의 메아리 시험 — `mic 24` 대 `mic 18`**(가장 큰 열린 위험. 사용자와 함께, 스피커 켠 채, 두 기기는 다른 방. **1번이 맥북 USB 에 보여야 한다**: `python3 tools/phone_ctl.py port 1` — 안 보이면 사용자에게 꽂아 달라고 부탁한다. 2번은 USB 가 없어도 된다)
  평소처럼 통화를 걸어 받은 뒤, **2번 쪽 사람만** 평소 크기로 말하고(책 읽기 — 맥북 소리 금지) **1번 쪽은 조용히** 있는다. 그동안 컨트롤러가:
  ```bash
  cd ~/tab5-toy-phone/firmware/tab5_phone && python3 tools/phone_ctl.py --unit 1 run 'until:stat:^STAT .*fsm=IN_CALL.* voice=1 :30' 'until:mic:^OK mic 24$:5' 'until:tune:^TUNE :5' 'sleep:20' 'until:stat:^STAT .*fsm=IN_CALL.* voice=1 :10' 'until:tune:^TUNE :5' 'until:mic 18:^OK mic 18$:5' 'until:tune:^TUNE :5' 'sleep:20' 'until:stat:^STAT .*fsm=IN_CALL.* voice=1 :10' 'until:tune:^TUNE :5' > out/p3q_echo_unit1.log 2>&1; echo "rc=$?"; grep -E "^TUNE" out/p3q_echo_unit1.log | grep -oE ' mic=[0-9]+|far=[0-9]+|mclip=[0-9]+' | paste -sd' ' -; grep -E "^STAT .* voice=1 " out/p3q_echo_unit1.log | grep -oE 'v_mic=[0-9]+|v_ref=[0-9]+|v_aec=[0-9]+|v_spk=[0-9]+' | paste -sd' ' -
  ```
  (이 명령은 실행해 본 적이 없다 — 모양은 R3 의 `--unit N run` 과 같다. `TUNE` 이 네 번 찍힌다: `mic 24` 구간의 앞·뒤, `mic 18` 구간의 앞·뒤.)
  읽는 법: 구간마다 `mclip` 이 **늘어난 양**(뒤 − 앞)과 `far` 가 늘어난 양(= 상대 소리가 난 묶음. 20초에 수백이어야 시험이 성립한다 — 0 에 가까우면 2번 쪽이 말하지 않은 것). 20초는 표본 32만 개 → `mic 24` 구간의 `mclip` 증가가 160(0.05%)을 넘고 `mic 18` 구간에서 0 가까이로 줄면 "넘침이 있고 `mic 18` 이 없앤다". 그리고 **2번 쪽 사람에게** 묻는다: "말할 때 내 목소리가 조금 뒤에 돌아와 들리나요? 앞 20초와 뒤 20초 중 어느 쪽이 덜한가요?" 증폭을 바꾼 직후 1~2초는 에코 제거가 달라진 크기에 다시 자리 잡는 중이라 메아리가 잠깐 샐 수 있다 — 뒤 20초의 처음 몇 초는 빼고 듣는다(딸깍 소리도 한 번 날 수 있다). 끝나면 `mic 24` 로 되돌리지 않아도 된다(재부팅하면 부팅 값) — 다만 **`mic 18` 인 채로는 1번 쪽 사람의 목소리가 상대에게 최대 6dB 작게 갈 수 있다**(평탄화가 상한까지 메운다)는 것을 알고 둔다.
  결과 → 결정 PL3c-D16(Task 11 Step 1 의 표): `mic 24` 구간에서 `mclip` 이 늘지 않고 메아리도 없다 = 이 위험은 닫는다(1회차의 큰 소리는 다른 원인).

혼자면 한쪽 기기 앞에 책 읽는 소리(휴대폰 녹음·라디오)를 30~50cm 거리에서 틀어 두고 다른 방에서 듣는다. 컨트롤러가 손잡이를 바꿀 때마다 **"가"·"나"를 채팅으로 알린다**(각 8초쯤, 두 번 되풀이). 답은 "가 / 나 / 같다" 중 하나.

1. **크기** — 말하는 쪽 기기에: 가 = `lvl 0`(아침의 소리), 나 = `lvl 12`. "어느 쪽이 더 잘 들리나요?" → "나"면 이어서 가 = `lvl 12`, 나 = `lvl 16`: "더 큰 쪽이 좋은가요, 아니면 시끄럽거나 쉬익 소리가 거슬리나요?"
2. **또렷함(먹먹함)** — 말하는 쪽 기기에: 가 = `tilt 0`, 나 = `tilt 2`. "어느 쪽 말이 더 또렷한가요?" → "나"인데 날카로우면 `tilt 1` 로 한 번 더.
3. **가까이 / 멀리** — 손잡이 없이: 말하는 사람이 기기에서 팔 길이(약 60cm)에서 한 문장, 한 뼘 반(약 30cm)에서 한 문장. "차이가 큰가요?"(크면: 멀리서 들리는 것의 큰 몫은 거리다 — 사용 안내에 적는다.)
4. **내 목소리가 돌아오나(메아리)** — **말하는 사람에게** 묻는다. 듣는 쪽 기기에: 가 = `res 0`, 나 = `res 12`. "말할 때 내 목소리가 조금 뒤에 돌아와 들리나요? 어느 쪽이 덜한가요?" 그리고 듣는 사람에게 "나"에서 동시에 말해 보게 하고 "상대 말이 끊기거나 작아지나요?"(작아지면 `res 6`).
5. **벨·연결음·종료음** — 한 번 걸어서: "벨 소리 크기는 어떤가요? 크다 / 알맞다 / 작다", "지지직거리나요?"

(시간이 남으면) 아침에 답을 못 받은 것: 말하는 쪽 기기를 `quiet on`(그 기기의 스피커가 꺼져야 `vsrc mic` 이 듣는다) 한 뒤 가 = `vsrc aec`, 나 = `vsrc mic` — "어느 쪽이 더 또렷하고 큰가요?" 끝나면 `vsrc aec`·`quiet off`.
사용자가 듣기를 하지 않으면: 부팅 값은 지금 그대로(`lvl 12`, 나머지 `5935ddd`) 두고, R5 의 숫자와 `mclip` 만 기록한다.
고른 값을 부팅 값으로 굳히는 법(다음 빌드에서): `python3 $W/draft/verify/freeze_tune.py ~/tab5-toy-phone/firmware/tab5_phone/main/voice_tune.h --set lvl=16,tilt=1,res=12,mic=18` 처럼 고른 칸만 → 맥 시험·pytest·빌드(범위 밖 값은 도구가 거절하고, 헤더의 `static_assert` 가 빌드에서 한 번 더 막는다).

- [ ] **(사용자가 있고 시간이 되면) M5 스피커·마이크가 어느 높이를 못 내나**: 2번의 스피커를 켠 채(1번은 `quiet on`) Task 7 의 M1 명령을 `opus 24` 한 번만, 산출물 `out/p3q_m5_acoustic.log` 로 돌린다(시험음 크기 2000 — 벨보다 12dB 작다. 7개 음이 3초씩 난다고 사용자에게 알린다). `tune_report.py sweep … --tx 1 --rx 2` 의 **받는 쪽 `v_mic`** 칸이 2번의 스피커 → 2번의 마이크다: 1kHz 대비 −10dB 아래인 높이는 이 스피커·마이크가 잘 못 내는 대역이다("여기까지"의 숫자).

---

## 2덩어리 — 맥미니로 옮길 때 (초안 검증됨)

Task 9·10 의 패치는 Task 6b 까지 적용한 저장소 위에 그대로 적용된다 — 사본에서 순서대로 적용·시험·빌드를 확인했고(`logs/verify_t9.txt`·`verify_t10.txt`·`verify_build_t9.txt`·`verify_build_t10.txt`), 검증 때의 저장소 HEAD(`771b9fe`)에 `T6b-01` → `T9-01`·`T9-02` → `T10-01`·`T10-02` 를 누적으로 `git apply --check` 해서 통과했다(`logs/verify_check.txt`). 그 뒤 Task 6b 가 `d3da36f` 로 들어갔고, 그 위(HEAD `19ff832`)에도 T9·T10 네 개가 누적으로 통과한다(임시 색인으로 다시 확인). 패치 전문은 `D/patches/` 에 있고 여기에는 싣지 않는다(목록·검사합은 "초안 목록").

**1덩어리 검수(`chunk1-review.md`)의 Minor 8건 — 처리**

| # | 지적 | 처리 |
|---|---|---|
| 1 | 말하던 중(12배)에 상대 소리가 시작되면 그 첫 묶음(16ms) 안에서는 이득이 3072 → 1024 로 내려오는 중이라 4배를 넘는다. "넘지 않는다"는 묶음 끝 기준으로만 참 | 동작은 그대로 둔다(한 번에 내리면 딸깍 소리가 난다. 메아리가 돌아오기 전의 묶음이고 눌림이 넘침을 막는다). **문구를 바로잡았다** — 이 계획서 PL3c-D7, `tx_dyn.h` 머리말(T10-02, 주석만). **시험으로 고정** — Task 6b "inside that one block (16 ms) the gain only comes down …"·"from the second block on no sample is above the fixed gain" |
| 2 | 통화 중 `lvl 12 → 0` 은 보간 없이 한 표본에서 내려간다(딸깍 한 번). 시험 없음 | **초안 없음 — 2덩어리에서 시험 먼저로**(작다): 꺼짐으로 바뀐 첫 묶음만 앞 이득에서 고정 이득으로 보간하고, 그 뒤는 지금처럼 3b 와 표본까지 같게. 그때까지: 듣기에서 손잡이는 말 사이에 바꾼다 |
| 3 | `lvl 0` + `res > 0` 이면 부드러운 눌림 없이 3b 의 자르기 | 그대로(3b 와 같은 동작). 부드러운 눌림은 `lvl` 이 켜져 있을 때만이다(PL3c-D7·`tx_dyn.h` 머리말에 그렇게 적혀 있다) |
| 4 | `nodelay` 는 값을 먼저 쓰고 잠금 시간 초과면 `ERR` — 응답은 실패인데 값은 바뀐다. `nodelay 0` 도 연결 중이면 소켓을 훑는다. 콘솔이 최대 2초 막힐 수 있다 | **초안 없음 — 2덩어리에서 시험 먼저로**: `relay_link.cpp` 의 `set_nodelay` 가 잠금을 먼저 잡고, 실패하면 값을 바꾸지 않은 채 −1. **`nodelay=1` 을 부팅 값으로 굳히려면(Task 11) 그 전에 고친다** |
| 5 | 비트율이 거절된 직후에 들어온 `opus N` 을 덮어쓸 수 있다(경쟁 한 번) | 그대로(무해 — 다시 보내면 된다) |
| 6 | 시험 공백: 손잡이 중간 변경 / 고이득에서 상대 소리 시작의 묶음 안 이득 / 실제 음성 파형 / 수렴 뒤 "내 말" 판정의 오탐·미탐 / 무작위 시험이 크기 경계를 따로 보지 않음 | 묶음 안 이득 = Task 6b. 손잡이 중간 변경 = 2번과 함께. 실제 음성·오탐 = Task 8 의 귀와 `TUNE` 의 `g_q8`·`supp`·`far`(맥 시험으로는 못 닫는다) |
| 7 | Opus 40kbps 에서의 캡처 스택 사용량 미측정(32KB 로 줄었다) | Task 7 M1 에 순서를 넣었다(24 → 32 → 40, 사이마다 `stk_vcap ≥ 6000`). 그 전에는 `opus` 를 쓰지 않는다(컨트롤러 판정 P3C-R4). 굳히기 표(Task 11)는 `opus=32` 까지만 |
| 8 | Task 4 가 자동 동기화 커밋(`9de0596`)에 담겼다 | 내용은 같다. 되돌리는 법에 그 이름으로 적었다(위 "롤백") |

### Task 9: 수신 무음 감시 — 상대의 소리가 5초 없으면 끊는다 (시험 먼저)

**Files:**
- Modify: `firmware/tab5_phone/main/call_fsm.h`(사건 `Ev::VOICE_LOST` — 맨 끝에 추가, `IN_CALL` 에서의 전이), `voice_core.h`(`RX_SILENCE_MS = 5000`, `class RxWatch`, `Receiver::frames()`), `voice.h`·`voice.cpp`(`voice::rx_frames()`), `app.cpp`(감시 시작·사건 넣기)
- Test: `firmware/tab5_phone/tools/test_call_fsm.cpp`·`test_voice_core.cpp`(추가)

**Interfaces:**
- Consumes: 3b 의 `callfsm::Fsm`, `voicecore::Receiver`.
- Produces: `callfsm::Ev::VOICE_LOST`(값 14 — `TICK` 13 뒤), `IN_CALL` + `VOICE_LOST` → `acts = A_SEND_HANGUP | A_VOICE_STOP | A_TONE_END (0x248)`, `notice = LOST`, `ENDING`(3초 제한). `class voicecore::RxWatch { void start(uint32_t now_ms); bool check(uint32_t now_ms, uint32_t frames); uint32_t silent_ms(uint32_t now_ms) const; }`, `uint32_t voice::rx_frames()`. 기기 로그: `VOICE off` 다음 줄에 `FSM IN_CALL -> ENDING acts=0x248 notice=LOST`(사용자가 끊으면 `notice=NONE`).

- [ ] **Step 1: 시험** — `cd ~/tab5-toy-phone && git apply $P/T9-01-watch-tests.patch`
- [ ] **Step 2: 실패 확인** — `test_call_fsm.cpp:448:26: error: no member named 'VOICE_LOST' in 'callfsm::Ev'`, `test_voice_core.cpp:918:9: error: unknown type name 'RxWatch'`
- [ ] **Step 3: 구현** — `cd ~/tab5-toy-phone && git apply $P/T9-02-rx-watch.patch`
- [ ] **Step 4: 통과 확인** — 맥 시험(`T="voice_tune tx_dyn"`): `call_fsm: 114 ok, 0 failed`, `voice_core: 173 ok, 0 failed`, 나머지 그대로(`jitter_buffer 92`·`tx_dyn 46` — Task 6b 뒤, `tones 89` — 신호음 쪽은 새 사건을 모른다). 도구 pytest `237 passed`. (사본 빌드 `0x28a510`, 경고 0.)
- [ ] **Step 5: 커밋** — `git add firmware/tab5_phone/tools/test_call_fsm.cpp firmware/tab5_phone/tools/test_voice_core.cpp firmware/tab5_phone/main/call_fsm.h firmware/tab5_phone/main/voice_core.h firmware/tab5_phone/main/voice.h firmware/tab5_phone/main/voice.cpp firmware/tab5_phone/main/app.cpp`, 메시지 `feat(phone): 수신 무음 감시 — 통화 중 상대의 소리가 5초 없으면 끊는다(VOICE_LOST → hangup, 안내 LOST), 시험 먼저 (PL3c-D10, 3b 최종 검수 M-9)` + 서명 줄.

### Task 10: 미룬 지적 넷 (시험 먼저)

**Files:**
- Modify: `firmware/tab5_phone/main/audio_out.cpp`(`fail()` 이 앰프 끄기 실패를 알린다 + 주석의 사실 정정), `voice_core.h`(`GenWatch::stale`), `voice.cpp`(읽기 뒤 세대 재확인, `selftest` 에서도 세대 올림), `tx_dyn.h`(머리말 주석 두 줄 — 1덩어리 검수 Minor 1 의 문구 정정, 코드는 그대로)
- Test: `firmware/tab5_phone/tools/test_voice_core.cpp`·`test_phone_ctl.py`(추가 — 소스 모양 시험 넷: 앰프 끄기의 자리와 실패 경로(fix 2 재검수 Minor 2·4), 묵은 묶음 버리기·`selftest` 세대(Minor 6·범위 밖 1), 절전 끄기·읽기(fix 3 검수 Minor ①), 무음 감시의 연결)

**Interfaces:**
- Consumes: Task 5·9 의 코드.
- Produces: `bool voicecore::GenWatch::stale(uint32_t gen) const`. 동작: 마이크를 읽는 16ms 사이에 끄기 → 켜기가 지나가면 그 묶음을 버린다 / `selftest` 를 연달아 보내도 둘째가 제대로 잰다 / 실패 경로에서 앰프를 못 끄면 경고 로그 한 줄.

- [ ] **Step 1: 시험** — `cd ~/tab5-toy-phone && git apply $P/T10-01-minor-tests.patch`
- [ ] **Step 2: 실패 확인** — `test_voice_core.cpp:997:100: error: no member named 'stale' in 'voicecore::GenWatch'`, 도구 pytest `2 failed, 239 passed`(`…a_block_read_across_a_restart_is_dropped…`, `…the_amp_is_forced_off_right_after_m5_begin…`)
- [ ] **Step 3: 구현** — `cd ~/tab5-toy-phone && git apply $P/T10-02-minors.patch`
- [ ] **Step 4: 통과 확인** — `voice_core: 176 ok, 0 failed`, 도구 pytest `241 passed`, 교환기 pytest `137 passed`, 그리고 전 파일 대조: `cd ~/tab5-toy-phone && find $P/../files -type f | while read f; do cmp "$f" "${f#$P/../files/}" || echo "다름: $f"; done; echo "대조 끝"` — 단 `main/voice_tune.h` 는 Task 11 에서 굳힌 뒤라면 다를 수 있다. (사본 빌드 `0x28a410`, 경고 0.)
- [ ] **Step 5: 커밋** — `git add firmware/tab5_phone/tools/test_voice_core.cpp firmware/tab5_phone/tools/test_phone_ctl.py firmware/tab5_phone/main/audio_out.cpp firmware/tab5_phone/main/voice_core.h firmware/tab5_phone/main/voice.cpp firmware/tab5_phone/main/tx_dyn.h`, 메시지 `fix(phone): 미룬 지적 — 앰프 끄기 실패 경고, 읽는 사이 지나간 끄기→켜기의 묵은 묶음 버림, selftest 도 세대 올림, 소스 모양 시험 넷 (fix 2 재검수 Minor 2·4·6·범위 밖 1, fix 3 검수 Minor ①)` + 서명 줄.

**여기서 2덩어리의 검수가 들어간다**(Task 9·10 의 커밋 둘 + 위 표의 2·4번을 고쳤으면 그 커밋).

### Task 11: 듣고 고른 값을 굳히고, 올리고, 통화 중 끊김을 시험한다

**Files:**
- Modify: `firmware/tab5_phone/main/voice_tune.h`(`DEFAULT_*` 의 숫자 — 도구로)
- Create(git 제외): `out/fw_prev_<Task 6 의 버전>/build/`, `out/p3q2_*.log`

- [ ] **Step 1: 값을 정한다**(Task 8 의 답과 Task 7 의 숫자로. Task 7 의 M1~M4 를 건너뛰었으면 여기서 먼저 한다)

  | 근거 | 굳히는 값 |
  |---|---|
  | 문항 1: `lvl 12` 가 `lvl 0` 보다 낫고, `lvl 16` 이 더 낫다 / 12 가 낫다 / 0 이 낫다(쉬익 소리·울렁임) | `lvl=16` / `lvl=12`(그대로) / `lvl=0`(그리고 보고 — 문턱을 다시 봐야 한다) |
  | 문항 2: `tilt 2` 가 또렷하다 / 날카롭다 → `tilt 1` 이 낫다 / 같다 | `tilt=2` / `tilt=1` / 그대로 0 |
  | 문항 4: `res 12` 에서 메아리가 덜하고 상대 말이 안 끊긴다 / 끊긴다 → `res 6` / 메아리가 원래 없다 | `res=12` / `res=6` / 그대로 0 |
  | Task 8 Step 4: 1번 쪽이 조용할 때 1번의 `mclip` 이 늘고(`mic 24`), `mic 18` 에서 줄고, 말하는 사람이 "메아리가 덜하다"고 한다 | **결정 PL3c-D16** — 두 기기가 같은 바이너리라 `mic` 의 부팅 값은 하나다: (가) `mic=18,lvl=16` 을 두 기기 공통으로(도구 한 줄. 2번의 작은 목소리는 지금보다 최대 3.5dB 작아진다 — 그래도 `5935ddd` 보다는 크다) 또는 (나) 기기 번호로 부팅 값을 고르는 작은 코드(1번만 18 — 초안 없음, 시험 먼저). (가)로 올린 뒤 2번 쪽 말이 작다는 답이 나오면 (나) |
  | M3: `nodelay 1` 에서 왕복 가운데 값이 10ms 넘게 줄고 `v_txfail=0` | `nodelay=1` |
  | M4·아침 같은 망: `jb 0` 에서 `v_jb` 목표가 6 이상으로 올라가 머물고, `jb 5` 의 끊김이 분당 15 이하 | `jb=5`(조용한 망뿐이면 그대로 0 — 차이가 없다) |
  | M1: `opus 40` 에서 6kHz 가 3dB 넘게 낫고 `v_enc_us` 평균이 5000 아래 | `opus=32`(40 은 스택·부하 여유를 위해 쓰지 않는다) |
  | **사용자가 듣기를 하지 않았다**(2026-10-05 10시 현재 이 경우다 — 1회차 무효) | `lvl=12`·`tilt=0`·`res=0` 그대로. `nodelay`·`jb` 만 위 측정 기준으로. **귀로 고른 값이 없으면 `lvl`·`tilt`·`res` 는 굳히지 않는다** |

  ```bash
  cd ~/tab5-toy-phone && python3 .superpowers/sdd/2026-10-05-tab5-phone-phase3c-quality/draft/verify/freeze_tune.py firmware/tab5_phone/main/voice_tune.h --set <고른 칸만: 예 lvl=16,tilt=1,res=12,nodelay=1>
  ```
  Expected: 고친 줄마다 `DEFAULT_… : 앞 → 뒤`, 표, `고친 줄 N개`(범위 밖이면 `범위 밖(고치지 않았다): …` 와 rc=2 — 값을 다시 본다). 이어서 맥 시험(`T="voice_tune tx_dyn"`)·도구 pytest(`241 passed`) — 전부 그대로여야 한다(사본에서 `lvl=16,tilt=1,res=12,mic=18,opus=32,jb=5,nodelay=1` 을 한꺼번에 넣어 확인했다: 시험 그대로, 빌드 `0x28a410`·경고 0). 커밋: `git add firmware/tab5_phone/main/voice_tune.h`, 메시지 `feat(phone): 통화 품질 부팅 값 굳히기 — <고친 칸과 값>, 근거 <문항·측정> (PL3c-D3)` + 서명 줄.
- [ ] **Step 2: 보관·빌드·올리기·회귀** — Task 6 Step 3~5 와 Task 7 의 R0~R5 를 그대로(보관 폴더는 `out/fw_prev_<지금 기기의 버전>/build/` — `out/p3q_fw_version.txt` 의 버전, 검사합은 `out/p3q_fw_build.sha256`, 산출물 접두어 `p3q2_`). R3 의 `TUNE` 기대 줄은 굳힌 값으로 읽는다(`dflt=1 legacy=0`).
- [ ] **Step 3: 통화 중 연결이 끊겼다 돌아온다**(3b 최종 검수 M-10 — 조용한 채. 교환기를 끄지 않고, 2번과 같은 ID 로 한 번 접속해 2번을 밀어낸다)
  ```bash
  cd ~/tab5-toy-phone/firmware/tab5_phone && I='both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.*audio=1 tone=NONE.* amp=0 .* voice=0 :60'; (sleep 14; cd ~/tab5-toy-phone/server && python3 tools/phone_fake_peer.py tab5-2 sleep:1 > ~/tab5-toy-phone/firmware/tab5_phone/out/p3q2_kick_peer.txt 2>&1) & python3 tools/phone_ctl.py pair "$I" '1:tap:640:360' '2:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' 'sleep:2' '2:tap:404:428' 'both:wait:^FSM .* -> IN_CALL acts=0x180 notice=NONE$:10' 'both:until:stat:^STAT .*fsm=IN_CALL.* voice=1 :10' \
    '2:wait:^RELAY up=0 :20' '2:wait:^VOICE off$:5' '2:wait:^FSM IN_CALL -> OFFLINE acts=0x240 notice=LOST$:5' '1:wait:^VOICE off$:10' '1:wait:^FSM IN_CALL -> IDLE acts=0x240 notice=LOST$:5' \
    '2:wait:^RELAY up=1 :40' 'sleep:3' 'both:until:stat:^STAT .*fsm=IDLE.*relay=1.*peer=1.* voice=0 :30' 'sleep:3' 'both:until:stat:^STAT .*fsm=IDLE.* voice=0 :10' \
    '2:tap:640:360' '1:wait:^FSM IDLE -> INCOMING acts=0x20 notice=NONE$:10' 'sleep:2' '1:tap:404:428' 'both:wait:^FSM .* -> IN_CALL acts=0x180 notice=NONE$:10' 'sleep:5' 'both:until:stat:^STAT .*fsm=IN_CALL.* voice=1 :10' \
    '2:tap:644:606' 'both:wait:^VOICE off$:10' 'sleep:2' 'both:until:stat:^STAT .*fsm=IDLE.* voice=0 :15' > out/p3q2_kick.log 2>&1; echo "rc=$?"; wait; grep -cE '!!' out/p3q2_kick.log; for u in 1 2; do grep -E "^\[$u\] STAT " out/p3q2_kick.log | tail -1 | grep -oE '^\[.\]|v_tx=[0-9]+|v_rx=[0-9]+|v_under=[0-9]+|stk_vtx=[0-9]+|stk_vcap=[0-9]+|heap_int_min=[0-9]+' | tr '\n' ' '; echo; done; python3 ~/tab5-toy-phone/.superpowers/sdd/2026-10-05-tab5-phone-phase3c-quality/draft/verify/voice_check.py out/p3q2_kick.log --idle-cap
  ```
  Expected: `rc=0`, `0`, 기기마다 한 줄(`stk_vtx` ≥ 1024, `v_tx`·`v_rx` 가 100 이상 = 다시 붙은 뒤의 통화에서 목소리가 양쪽으로 흘렀다, `v_under=0`), `voice_check` 가 통화 밖 `v_cap` 불변을 확인. 읽는 법: 밀려난 2번은 연결이 끊기는 그 자리에서 `VOICE off`(마이크가 닫힌다), 1번은 교환기의 `ended(lost)` 로 닫힌다. 2번은 스스로 다시 붙고(1~2초, 길면 30초 — RL-D3), 그 뒤의 통화가 된다. **`stk_vtx`·`heap_int_min` 을 보고에 적는다**(끊김 핸들러가 PSRAM 스택에서 돈 첫 기록 — PL3c-D2). 실패하면 고치지 않고 로그를 붙여 보고한다.
- [ ] **Step 4: 상대가 사라진다**(무음 감시 — 3b Task 8 Step 10 의 명령 그대로, 산출물 `out/p3q2_reboot.log`): 통화 중 2번을 `reboot` → 1번에 **5~8초 안에** `VOICE off` → `FSM IN_CALL -> ENDING acts=0x248 notice=LOST` → `FSM ENDING -> IDLE …`(3b 에서는 약 17초 뒤 `IN_CALL -> IDLE … notice=LOST` 였다). 그 사이 1번의 `v_tx` 가 멈추는지(마이크가 닫혔다) STAT 두 번으로 본다.
- [ ] **Step 5: (컨트롤러가) 교환기를 내렸다 올린다** — 통화 중에 컨트롤러가 단독 호출 `bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh restart`. 두 기기 모두 `RELAY up=0` → `VOICE off` → `FSM IN_CALL -> OFFLINE acts=0x240 notice=LOST`, 교환기가 돌아오면 `RELAY up=1` → `fsm=IDLE`, 그 뒤 통화 한 번이 된다(`out/p3q2_relay_restart.log`). 재접속을 다섯 번 되풀이하며 `heap_int` 를 적는다(조사 보고 B6 — 재연결 반복 시 내부 메모리).
- [ ] **Step 6: 스피커를 켜고 사용자와 짧게 확인**(Task 8 의 문항 1·4 만 — 굳힌 값이 들은 것과 같은가).

### Task 12: 기록 — README, 통화 설계서, 이 계획서

**Files:**
- Modify: `firmware/tab5_phone/README.md`("시리얼 명령"에 손잡이 9개와 `TUNE` 줄 — 이 계획서 Task 5 의 Interfaces 와 인계문 3절의 표 그대로, "구조"에 `voice_tune.h`·`tx_dyn.h`, "기기 검증"에 3c 의 실측), `docs/exec-plans/2026-10-03-tab5-phone.md`(§5 Phase 3 아래 3c 한 줄, §8 진행 로그에 "Phase 3c 결과" — 사용자 판정 원문, 굳힌 값과 근거, 지연 예산의 실측, 메모리, 확인하지 못한 것), 이 계획서(상태·체크박스·진행 로그).
- README 는 3b 문서 과제가 2026-10-05 에 고쳤다(`667739e`·`782dc45`) — 그 위에 덧붙인다(패치를 미리 만들지 않은 이유).

- [ ] **Step 1**: 위 세 파일에 적는다. 측정값에는 조건(빌드·손잡이 값·같은 방/다른 방·시각)을 붙이고, "소스 판독"과 "기기에서 확인"을 구별한다. 사용자 판정은 원문 그대로 싣는다.
- [ ] **Step 2**: `python3 $P/../verify/secret_scan.py` 로 새로 적은 글에 주소·토큰·망 이름 모양이 없는지 본다(3b Task 10 과 같은 사용법). 커밋: `docs(phone): Phase 3c 통화 품질 결과 기록` + 서명 줄.

---

## 검증 기준 (이 계획의 완료 조건)

**1덩어리**
- [x] 맥 시험: 기존 9종이 수정 없이 통과(16·105·61·9·91·89·37), `jitter_buffer` 90, `voice_core` 158, `voice_tune` 17, `tx_dyn` 34, 경고 0. 도구 pytest 237, 교환기 pytest 137. — 구현 보고
- [x] Task 6b 뒤: `jitter_buffer` 92, `tx_dyn` 46(나머지 그대로). — `d3da36f`
- [x] 펌웨어 빌드 성공, 우리 코드 경고 0, `dependencies.lock` 불변, 앱 파티션 58% 여유. — `0x28a760`
- [x] 기기: 두 대에 같은 바이너리. 부팅 줄이 그대로(`AUDIO ready … vol=100 … mic=24`, `VOICE ready … bitrate=24000 …`), `TUNE dflt=1 legacy=0 … lvl=12 … ps=0`.
- [x] 메모리: 부팅 뒤 `heap_int` 가 `5935ddd` 보다 약 24KB 크다(139,000 안팎) — 실측 140,171·139,519. 통화 뒤 `heap_int_min` ≥ 100000 — 실측 116,527·121,751(시험음·지연·30초 통화 뒤. 아침만큼 혼잡한 망에서는 아직 못 봤다). `stk_vcap` ≥ 6000 — 시험음·조용한 통화에서 8,820(**말소리 통화 뒤의 값은 듣기 로그에서 확인할 것** — 여유가 얇다), 나머지 `stk_*` ≥ 1024.
- [x] 회귀: `lvl 0` 으로 3b 의 시험음 통화·지연 재기·30초 통화가 3b 기준 그대로 통과(`v_under=0`, `aud_late=0`, 부하 ≤ 50%, `VOICE on/off` 의 짝). — 통화 밖 `v_cap` 불변(`--idle-cap`)만 돌리지 않았다(기기 보고의 걱정 4) → Task 11 Step 2 에서.
- [x] 평탄화: 시험음(`probe`)으로 `lvl 12` 의 받는 쪽 `v_spk` 가 `lvl 0` 의 1.8배 이상·6000 이하 — 846 → 3353(3.96배), 상대 소리가 나는 동안 `g_q8 ≤ 1024` — 1024(`far=239`), `mclip=0`.
- [ ] 사용자: "아침보다 크게 들린다"(문항 1). 동시에 말하기가 나빠지지 않았다(문항 4 의 끝 질문).

**2덩어리**
- [ ] 맥 시험 `call_fsm` 114, `voice_core` 176. 도구 pytest 241. 빌드·경고·잠금 파일은 위와 같다.
- [ ] 굳힌 값으로: 지연 예산의 합격 기준(`VPING` 왕복 ≤ 380ms 혼잡 / ≤ 301ms 조용), 끊김 분당 15 이하.
- [ ] 통화 중 끊김: 밀려난 기기·남은 기기 모두 `VOICE off`, 다시 붙은 뒤 통화가 된다, `stk_vtx` ≥ 1024. 상대 재부팅: 5~8초 안에 `notice=LOST` 로 끝난다.
- [ ] 문서 세 곳 갱신, 비밀 모양 0건.

## 조사 보고의 "확인할 것"·"미확인" — 옮긴 것과 옮기지 않은 것

`docs/exec-plans/2026-10-04-tab5-phone-research/` 세 보고를 훑었다(Wi-Fi 절전처럼 계획에서 빠지는 일을 되풀이하지 않으려는 것).

| 보고의 항목 | 처리 |
|---|---|
| `websocket-client.md:87` Wi-Fi 절전 | fix 3 에서 껐다. 이번: 지금 값을 읽어 `TUNE ps=`(Task 5, PL3c-D11) |
| `websocket-client.md:116-119` B4 — 상향 음성이 묶여 나감(`TCP_NODELAY`), "pbuf 가 쓰기 가능 한도를 넘는다 → `CONFIG_LWIP_TCP_SND_BUF_DEFAULT` 를 함께 키워야" | **옮김**: `nodelay` 손잡이(Task 5, PL3c-D6)와 측정(Task 7 M3). 송신 버퍼 설정은 바꾸지 않는다 — `sdkconfig` 를 건드리지 않고, 켰을 때 `v_txdrop`·`v_txfail` 이 느는지로 판단한다 |
| `websocket-client.md:82` 웹소켓 태스크 스택 8192 의 실제 사용량 | 재졌다(`stk_ws` 최저 여유 6,128~6,196 = 쓴 것 2KB, TLS 없이). TLS 는 Phase 4 — **넘김** |
| `websocket-client.md` B5 접속 버퍼 2048 | 이미 반영(`sdkconfig.defaults`) |
| `websocket-client.md` B6 재연결 반복 시 내부 메모리 감소 보고 | **옮김**: Task 11 Step 5(재접속 다섯 번의 `heap_int`) |
| `websocket-client.md` §9 밀어낼 때 고유 닫기 코드(서버 계약 변경) | 옮기지 않음 — 교환기를 바꾸지 않는다(PL3c-D14) |
| `audio-reference-diff.md` §5-1 Opus 손실 은닉 | 3b 에서 닫힘(PL3b-D7 — 직접 메운다) |
| `audio-reference-diff.md` §5-9 DTX·VBR | 그대로 끔(PL3c-D8). 무음 감시(PL3c-D10)가 "DTX 없음 = 프레임이 안 오면 상대가 없다"에 기댄다 |
| `audio-reference-diff.md` §5-11 "VOIP 방식은 NLP 세기를 조절할 수 없다 … 필요해지면 `FD_LOW_COST` 가 후보" | 역어셈블로 다시 확인(PL3c-D13). `FD_LOW_COST` 시험은 **옮기지 않음** — 에코 제거를 바꾸는 일은 Phase 0 수준의 재검증이 들고 지금의 첫 불만(크기)과 무관하다. `res` 가 그 자리를 메운다 |
| `audio-reference-diff.md` §6 FD 가 깨진 원인, ES8388 레지스터 0x08, AFE 내부값, VideoLink, 공장 펌웨어 | 옮기지 않음 — 이 계획이 건드리지 않는 것들이다 |
| `audio-reference-diff.md` §5-4·§6 이어폰 감지 | Phase 5 — **넘김** |
| `snapbooth-reuse.md:170` 한 번 누름이 여러 번 잡힘 / `:268` MAC / `:274` 1번의 NVS | Phase 2 에서 닫힘(TapGuard, 부팅 로그 대조, NVS 지우고 다시) |

## 초안 목록

| 패치(`D/patches/`) | sha256 앞 12자 | +줄 / −줄 | 고치는 파일 | 상태 |
|---|---|---|---|---|
| `T1-01-stack-shape-test.patch` | `3f91feae54a1` | +12 / −0 | `test_phone_ctl.py` | 저장소에 있음 `199f00e` |
| `T1-02-voice-stacks.patch` | `7f803fdc4330` | +16 / −4 | `voice.cpp` | 저장소에 있음 `199f00e` |
| `T2-01-tune-jitter-tests.patch` | `a00c1a8c971a` | +207 / −0 | `test_voice_tune.cpp`, `test_jitter_buffer.cpp` | 저장소에 있음 `91adf23` |
| `T2-02-voice-tune.patch` | `74c372e92c7f` | +94 / −0 | `voice_tune.h` | 저장소에 있음 `91adf23` |
| `T2-03-jitter-policy.patch` | `6535e5d5d091` | +73 / −9 | `jitter_buffer.h` | 저장소에 있음 `91adf23` |
| `T3-01-voice-core-tests.patch` | `28552b2d41f9` | +168 / −0 | `test_voice_core.cpp` | 저장소에 있음 `657224a` |
| `T3-02-voice-core.patch` | `be34185fdd64` | +47 / −8 | `voice_core.h` | 저장소에 있음 `657224a` |
| `T4-01-tx-dyn-tests.patch` | `6e581b2087ef` | +257 / −0 | `test_tx_dyn.cpp` | 저장소에 있음 `9de0596` |
| `T4-02-tx-dyn.patch` | `c1b27f4b6b7a` | +163 / −0 | `tx_dyn.h` | 저장소에 있음 `9de0596` |
| `T5-01-tune-tool-tests.patch` | `5368b8cfab4a` | +112 / −0 | `test_phone_ctl.py` | 저장소에 있음 `d285c33` |
| `T5-02-knobs-wiring.patch` | `03484f601b94` | +231 / −15 | `voice.h`, `voice.cpp`, `audio_out.h`, `audio_out.cpp`, `relay_link.h`, `relay_link.cpp`, `net_wifi.h`, `net_wifi.cpp`, `phone_main.cpp` | 저장소에 있음 `d285c33` |
| `T6-01-first-freeze.patch` | `9a9a4fed030b` | +1 / −1 | `voice_tune.h` | 저장소에 있음 `771b9fe` |
| `T6b-01-more-tests.patch` | `c14633e43a05` | +97 / −0 | `test_jitter_buffer.cpp`, `test_tx_dyn.cpp` | 저장소에 있음 `d3da36f` |
| `T9-01-watch-tests.patch` | `31023d591141` | +112 / −0 | `test_call_fsm.cpp`, `test_voice_core.cpp` | 남음(2덩어리) |
| `T9-02-rx-watch.patch` | `a88f4342f911` | +52 / −1 | `call_fsm.h`, `voice_core.h`, `voice.h`, `voice.cpp`, `app.cpp` | 남음(2덩어리) |
| `T10-01-minor-tests.patch` | `4ef21d277ebf` | +60 / −0 | `test_voice_core.cpp`, `test_phone_ctl.py` | 남음(2덩어리) |
| `T10-02-minors.patch` | `29731bf131e1` | +14 / −3 | `audio_out.cpp`, `voice_core.h`, `voice.cpp`, `tx_dyn.h` | 남음(2덩어리) |

| 도구(`D/verify/` — 저장소에 넣지 않는다) | sha256 앞 12자 | 하는 일 |
|---|---|---|
| `freeze_tune.py` | `d015e92554c7` | `voice_tune.h` 의 부팅 값 보기·확인·고치기(`--show`·`--expect`·`--set`, 범위 확인) |
| `secret_scan.py` | `8350672ef27a` | 3b 것 그대로 — 로그·문서에 비밀 모양이 없는지 |
| `touch_audit.py` | `fc39218464c8` | 3b 것 그대로 — 주입한 탭 수와 `touch=` 대조 |
| `tune_report.py` | `f8c77313234c` | `TUNE`·`STAT` 로그를 표로(`tune`·`rate`·`sweep`) |
| `voice_check.py` | `9bfb0c48a7fd` | 3b 것 그대로 — 통화 로그의 판정(`--during-call`·`--pings`·`--idle-cap`) |

- `D/files/` — 2덩어리까지 적용한 뒤의 파일 전문 21개(저장소 경로 그대로).

- Task 1~6 과 6b 의 패치 13개는 이 문서에 전문이 실려 있고, 2덩어리의 패치 4개와 도구는 `D` 에만 있다(위 표의 검사합으로 확인한다).
- 사본 검증 기록: 작업 폴더의 `logs/verify_*.txt`(단계별 실제 출력 — 다섯 번째 실행, 2026-10-05 10:01~10:05), `logs/verify_progress.txt`(22단계 전부 rc=0), 변이 시험 `logs/mutate3c.txt`(첫 실행 — 11가지 생존)·`mutate3c_b.txt`·`mutate3c_c.txt`(보강 뒤)·`mutate3c_d.txt`(마지막 파일로 다시 — 83가지 전부 잡힘). 지터 버퍼 규칙을 고르던 탐색 `explore/jb_sim.cpp`·`logs/explore_jb_sim_v3.txt`.
- `git apply --check`(`logs/verify_check.txt`): T1~T6 의 12개는 1덩어리를 얹기 전의 `782dc45` 에 누적으로 통과하고 그 결과가 저장소의 `771b9fe` 와 같다(펌웨어 `main`·`tools` 전 파일 — `logs/verify_t6.txt` "다른 파일: 0개"). T6b·T9·T10 의 5개는 검증 때의 저장소 HEAD(`771b9fe`)에 누적으로 통과. Task 6b 가 커밋된 뒤(`d3da36f`)의 HEAD `19ff832` 에는 T9·T10 의 4개가 누적으로 통과.

## 진행 로그

### 2026-10-05 — 계획 작성

- 의뢰(`plan-brief.md`) → 작성 중 컨트롤러 전달 넷: ① 첫 기록 창(`p3e_tworoom_unit2.log`)에는 통화가 없다 ② 두 방 통화 한 번(`p3e_tworoom_unit2_b.log`) ③ 사용자 판정 "먹먹하게 작은 소리로, 아주 멀리있는 것처럼 들림." — 크기·또렷함이 첫째, 1덩어리가 기본으로 더 커야 한다 ④ "가자 가자. 속도를 내서 가자" — 1덩어리를 먼저 인계, 2덩어리는 맥미니로 옮길 때.
- **기록 창에서 읽은 것**: `p3e_tworoom_unit2.log`(08:27~08:47, 2번, STAT 121줄) — 전부 `fsm=IDLE`, `touch`·`v_cap` 불변 = 통화 없음. 교환기·상대 연결은 20분 내내 유지(`relay=1 peer=1`), 대기 중 `heap_int` 88,511~91,631(통화들 뒤 풀이 자란 채), `heap_int_min` 80,463 그대로. `p3e_tworoom_unit2_b.log`(2번, 09:02 쯤) — 두 방·양쪽 스피커의 통화 4.8초: 받은 240 프레임 가운데 메움 7(2.9%)·버림 0·끊김 4(통화 시작 2.3초 안에 3 — 시작 직후 붙어 오는 끊김), 지터 버퍼 목표 6 → 7, 압축 평균 3.5~3.9ms, 한 순간 `v_mic`=1035·`v_ref`=514·`v_aec`=695·`v_spk`=1410, 끝의 `v_spk`=447. `p3e_tworoom_unit1.log`(09:04~09:33, 1번, STAT 181줄) — 창 안에는 통화 없음. 첫 줄에 직전 통화(위 통화와 다른 것 — 33.5초)의 셈이 남아 있다: 받은 1,678 가운데 메움 7(0.42%)·버림 0·끊김 4, 목표 5.
- **빌드와 통화의 겹침**: 사본 빌드는 09:00:41~09:01:24, 09:10~09:17 에 세 번(각 8~15초), 09:19~09:34 에 검증 세 바퀴, 09:46~09:48 과 10:01~10:05 에 한 바퀴씩(전부 `nice -n 10`). 위의 두 방 통화(09:02:00~09:02:15 쯤 — 로그의 `up=` 과 파일 시각으로 역산)는 첫 빌드가 끝난 뒤였다. 09:04~09:33 의 기록 창에는 통화가 없었다 → **이 계획이 인용한 `p3e_*` 통화 실측 가운데 빌드와 겹친 것은 없다.** 다만 **09:46~09:48 과 10:01~10:05 의 사본 빌드는 컨트롤러의 스피커 켜기(09:46)·사용자 듣기(09:51~)와 겹쳤다** — 그 시간대의 듣기 로그(`out/p3q_listen_ab.log`)에 끊김이 보이면 이 빌드를 의심할 것(작성자는 그 로그를 판독하지 않았다).
- 사본 검증: 위 "한눈에 보기"의 마지막 항목. 변이 시험의 첫 실행에서 살아남은 11가지 → 시험 12개를 더해 83가지 전부 잡음. **구현 패치는 그 과정에서 바뀌지 않았다.**
- **작성 중에 기준이 움직였다**: 네 번째 검증(09:46)은 "지금 HEAD"에서 사본을 떴는데 그 사이 컨트롤러가 1덩어리를 커밋해(`771b9fe`) T1~T6 패치가 "이미 적용됨"으로 실패했다(`logs/run4_base_moved/`) — 패치의 결함이 아니다. 검증 스크립트의 기준을 `782dc45` 로 못박고, 변이 시험으로 더한 시험을 이미 커밋된 T2-01·T4-01 에 섞지 않고 **따로 떼어 Task 6b 로** 만든 뒤 다섯 번째 검증을 돌렸다(전부 통과).
- 작성 중 고친 초안의 결함 둘(시험이 잡았다): ① 지터 버퍼의 "하나씩 버리기"가 0.1초만 깊어도 버리기 시작해 흔들림만 있는 망에서 끊김이 분당 0.2 → 4.2 로 늘었다 → "0.5초 지켜본 뒤"로 ② 평탄화의 바닥 잡음 어림이 1초에 60 씩 올라가 1.3초 넘게 이어지는 말을 잡음으로 배웠다 → 1/16 눈금으로 바꿔 약 7초로.

### 2026-10-05 — 1덩어리 실행 결과 (컨트롤러의 기록 `progress.md`·`chunk1-impl-report.md`·`chunk1-review.md`·`chunk1-device-report.md` 에서 옮김 — 작성자가 직접 잰 것이 아니다)

- **코드**(09:2x~09:37): 커밋 `199f00e`·`91adf23`·`657224a`·`9de0596`(자동 동기화 커밋이 Task 4 를 담음)·`d285c33`·`771b9fe`. 시험이 인계문과 같은 문구로 실패 → 통과. 끝 값: 맥 16·105·61·9·91·89·37·90·158·17·34, 도구 pytest 237, 교환기 137. 빌드 `771b9fe` 2,664,288 B(`0x28a760`), 경고 0. 커밋 서명은 구현 에이전트의 모델(Sonnet 5.5)로 들어갔다(판정 P3C-R2 — 그대로 둔다). 작성자 확인: 저장소 `771b9fe` 의 펌웨어 `main`·`tools` 가 사본의 Task 6 단계와 파일 하나까지 같다.
- **검수**(code-reviewer): Spec 통과 · Critical 0 · Important 0 · Minor 8(처리는 "2덩어리"의 표). "스피커를 켜도 되는가: 예, 조건부" — R3·R5 통과 뒤, 처음 듣기는 어른끼리, 두 기기는 다른 방, `opus` 는 24 에서만, 이상하면 `lvl 0`.
- **기기**(09:37~09:44, 두 기기 `771b9fe`, 스피커 끈 채): 부팅 줄·`TUNE dflt=1 legacy=0 meas=1 … lvl=12 … ps=0`·`SELFTEST ok=1` 이 기대와 같다. 메모리·스택·부하·지연은 위 예산 표의 "실측" 줄. R4(`lvl 0` = 옛 동작): 시험음 통화·지연 재기·30초 통화 전부 기준 통과. R5(평탄화, 1번이 `probe` 송신): `lvl 0` → `g_q8=0`·받는 쪽 `v_spk` 846 / `lvl 12` → `g_q8=3072`·`v_spk` 3353(3.96배) / 상대 시험음이 나는 동안 `g_q8=1024`·`far=239` / `mclip=0`. 1번의 `v_mic` 353 → `v_aec` 293~339 = **에코 제거는 고른 1kHz 시험음을 지우지 않고 −0.4~−1.6dB 로 통과시킨다**(→ Task 7 M2 의 주파수별 측정이 뜻이 있다). 패닉·재부팅 0, 터치 대조 일치, 비밀 모양 0.
- **스피커 켜기**(09:46, 컨트롤러): 2번 `SELFTEST amp=1 mic=1305 ref=592 aec=202 ok=1`, 1번 `mic=3421 ref=569 aec=665 ok=1`(아침과 같은 수준 — 1번의 `mic` 가 2번의 2.6배인 것도 그대로).
- **듣기 1회차**(09:50~09:52): 무효 — Task 8 머리의 "상태"에 적었다. **귀로 고른 값 없음.** 사용자가 왜 끊었는지는 아직 모른다(컨트롤러 기록: "맥북에서 소리나는게 맞는거지" → "1기기를 껐음").
- 컨트롤러 판정(이 계획과 다르게 간 곳): P3C-R1(계획서 전에 인계문으로 실행), P3C-R3(검수와 조용한 점검을 동시에, 스피커는 검수 뒤), P3C-R4(`opus` 손잡이는 이번 회차에 쓰지 않는다 · 송신 PSRAM 스택의 끊김 길은 2덩어리 Task 11 Step 3 에 맡긴다), P3C-R5(듣기를 맥북의 읽는 말로 — 문항 1·2 만) → P3C-R6(그 방식을 그만두고 평소 통화 한 번 + 한 줄 답).
- **작성자의 맥 작업이 기기 측정과 겹친 시간**(이 맥북이 교환기다): 변이 시험(맥 clang 을 연달아 — **`nice` 없이**, 한 코어의 약 90%)이 09:36~09:38·09:42~09:46 에 돌았고 이것이 **R4 의 지연 재기(09:42)·30초 통화(09:43)·R5(09:43)와 겹쳤다.** 네 번째 검증(09:46~09:48, 빌드는 `nice -n 10`)은 스피커 켜기(09:46)와 겹쳤다. 듣기 1회차(09:50~09:52)에는 아무것도 돌리지 않았다(그 통화의 나쁜 망은 이쪽 탓이 아니다). 다섯 번째 검증(10:01~10:05)과 변이 재확인(10:06~10:08, `nice` 없음) 때 통화가 있었는지는 모른다. → **R4 의 왕복 445·430ms 와 메움 0.26~0.32% 를 기준선으로 인용할 때는 "교환기 맥북에 다른 부하가 있었다"를 붙인다**(영향의 크기는 재지 않았다).

### 확인하지 못한 것 (위험이 큰 순서 — 1덩어리 실행·듣기 1회차 뒤 기준)

1. **받는 목소리가 커진 뒤 1번 기기가 메아리를 흘리는가(가장 큰 열린 위험).** 계산으로는 넘친다(디지털 꼭대기 16000 → 1번 마이크 37000~43000). 듣기 1회차의 끝 1초에 1번 쪽에서 큰 소리가 왔지만(2번의 `v_spk` 3837) 1번의 기록이 없어 원인을 가르지 못했다. 조용한 점검의 `mclip=0` 은 스피커가 꺼진 값이다. **다음 걸음: Task 8 Step 4**(1번을 USB 에 두고, 1번 쪽이 조용한 20초의 `mclip`·`far` 를 `mic 24` 와 `mic 18` 에서 재고, 말하는 사람에게 메아리를 묻는다) → 결정 PL3c-D16(두 기기가 같은 바이너리라 `mic` 부팅 값은 공통이다). 그때까지의 탈출구: `lvl 0`(옛 크기 — 넘침도 옛 수준), 급하면 `quiet on`.
2. **평탄화가 사람 목소리에서 어떻게 들리는가 — 귀의 답이 아직 없다.** 기기에서 확인된 것: 고른 시험음에서 12배까지 오르고 상대 소리가 나면 4배로 내려온다, 너무 큰 입력에서는 4배 아래로 내려간다. 사람 목소리·방 잡음·TV 소리에서의 동작(말 사이의 "쉬익"·울렁임, 말이 말소리로 안 잡힘)과 `lvl 12`/`16`·`tilt`·`res` 의 선택은 사용자의 귀로만 닫힌다 → **그 답이 올 때까지 `lvl`·`tilt`·`res` 는 굳히지 않는다.**
3. **`mclip` 은 원인을 가리지 않는다**(말하는 쪽의 큰 소리로도 오른다) — `v_mic`·`v_ref`·`far`·"누가 말했나"와 함께 읽어야 하고, `tune_report.py` 는 아직 그렇게 묶어 보여 주지 않는다(사람이 읽는다).
4. **송신 태스크의 PSRAM 스택에서 끊김 핸들러가 도는 길**(3b 에서도 기기에서 돈 적 없음 — Task 11 Step 3 이 처음. 그 전에 통화 중 Wi-Fi·교환기가 끊기면 그 기기가 재부팅될 수 있다: 통화는 어차피 끊긴 상황이고 스스로 대기로 돌아오지만 스피커가 켜져 있으면 "퍽" 가능).
5. **캡처 태스크 스택의 여유가 얇다**: 최저 8,820(32KB 가운데) — 시험음·조용한 통화·큰 말소리 + 평탄화(54초)에서 같은 값. `opus` 를 올린 통화에서는 재지 않았다(Task 7 M1 의 순서를 지킨다). 6,000 아래로 가면 `CAP_STACK` 을 40KB 로(되찾는 메모리 24 → 16KB).
6. **손잡이를 줄 때 처음 도는 길 넷**: 실행 중 비트율 바꾸기(`esp_opus_enc_set_bitrate` — 역어셈블로만 확인), 소켓을 훑어 `TCP_NODELAY` 걸기(+ 검수 Minor 4), 통화 중 마이크 증폭 쓰기(Task 8 Step 4 가 처음), 통화 중 지터 규칙 바꾸기.
7. **지연·끊김 쪽은 3c 가 아직 아무것도 바꾸지 않았다**(부팅 값 `jb 0`·`nodelay 0`): 지터 버퍼 새 규칙의 실제 끊김·지연(시뮬레이션은 방향과 크기만), `nodelay` 의 효과(기대 10~15ms — 추정), Opus 비트율별 압축 시간·고음 — Task 7 M1~M4. 나쁜 망에서의 `heap_int_min` 은 한 번 봤다(106,859 — 여유 6.9KB. 더 나쁜 망·더 긴 통화는 아직).
8. R4 의 지연·끊김 값이 작성자의 맥 부하와 겹쳤다(위 "겹친 시간"). 통화 밖 `v_cap` 불변 판정(`--idle-cap`)을 1덩어리 기기 점검에서 돌리지 않았다(줄의 짝은 맞았다).
9. 무음 감시의 5초가 실제 망에서 산 통화를 끊는 일이 없는지(2덩어리 — 듣기 1회차에 받은 프레임 사이의 틈이 1.5초까지 났다: 5초의 여유는 3배쯤).
10. 스피커를 켠 채 재부팅할 때의 "퍽"(소스 판독 — 3b 부터 미확인).
11. 닫힌 것(1덩어리에서 확인됨): `esp_wifi_get_ps` 가 C6 까지 다녀와 실제 값을 준다(`ps=0`, `v_under=0`), 32KB 캡처 스택으로 통화가 돈다, 내부 메모리 +24KB(나쁜 망에서도 기준 위), 에코 제거가 `probe` 시험음을 통과시킨다, 평탄화의 이득이 기기에서 설계대로 움직인다(0 / 3072 / 상대 소리 중 1024 / 큰 입력에서 491).
