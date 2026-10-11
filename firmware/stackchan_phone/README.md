# stackchan_phone — 스택짱 2호기 전화기 펌웨어 (3번 전화기)

스택짱 2호기(M5Stack CoreS3 기반, ESP32-S3)가 Tab5 전화기 두 대와 같은 교환기로 통화한다. **스트림덱(UIFlow2)은 그대로 두고** 한 플래시에 함께 넣어, 버튼으로 오간다.

- 설계: [docs/exec-plans/2026-10-08-phone-3way-design.md](../../docs/exec-plans/2026-10-08-phone-3way-design.md) §10·§11
- 근거 실측(관문 실험): [docs/exec-plans/2026-10-08-stackchan-phone-probe.md](../../docs/exec-plans/2026-10-08-stackchan-phone-probe.md)
- 화면 규칙: [docs/design/README.md](../../docs/design/README.md) §5 (토이폰 테마)
- **상태(2026-10-09)**: 스택짱 2호기에 **설치됨**(14:12) — 교환기 등록(토큰)까지 끝났고, 스트림덱 ↔ 전화기 오가기·걸기(벨이 Tab5 두 대에 감)·손 없는 앱 갱신 두 길을 기기에서 확인했다. **통화(목소리)는 아직 한 번도 못 해 봤다**(Tab5 쪽에서 받아 줄 사람이 필요하다). 경과: [docs/daily/2026-10-09/claude.md](../../docs/daily/2026-10-09/claude.md)

## 한눈에

| | |
|---|---|
| 기기 | 스택짱 2호기, MAC `AA:BB:CC:DD:EE:03`. 교환기 ID `stackchan-2`, 번호 3, 캐릭터 = 로봇 |
| 오가기 | 스트림덱의 **"전화기" 타일** → 전화기 / 전화기 화면의 **"스트림덱" 키캡(두 번 누르기)** → 스트림덱. 약 8초, 마지막에 고른 쪽이 전원을 껐다 켜도 유지된다 |
| 스트림덱일 때 | 전화를 못 받는다(상대 화면에 "잠듦") |
| 없는 것 | 메시지로 거절·벨 끄기·Wi-Fi 설정 화면(고정 Wi-Fi 목록만), 서보·LED 연출 |
| 통화 상대 | **새 펌웨어(신호 규약 v2)를 올린 Tab5 만.** 옛 펌웨어의 Tab5 는 스택짱에게 "꺼짐"으로 보인다(설계 TW-D8) |

## 구조

스택짱 전용 코드는 `main/sc_*` 와 `main/audio_board_cores3.cpp` 뿐이다. 통화의 판단·소리 경로·교환기 연결은 **Tab5 전화기와 같은 파일**을 경로로 끌어 쓴다(`main/CMakeLists.txt` — 복사하지 않는다).

| 파일 | 역할 |
|---|---|
| `main/sc_main.cpp` | 부팅 순서, 태스크 기동, 시리얼 명령 |
| `main/sc_app.cpp` | 사건(터치·교환기 신호·시간) → 통화 상태 기계 → 할 일 실행 → 다시 그리기 |
| `main/sc_ui_layout.h` | 320×240 배치와 터치 판정(순수 — 맥 시험) |
| `main/sc_ui.cpp` | 화면 그리기(순수 — 맥의 화면 미리보기가 같은 파일을 쓴다) |
| `main/sc_gfx.cpp` | 합성 버퍼(PSRAM) → 화면, 화면 찍기 |
| `main/sc_net.cpp` | Wi-Fi(칩 내장, `secrets.h` 의 고정 목록) |
| `main/sc_boot.cpp` | 부팅 선택(스트림덱 ↔ 전화기), 죽는 고리 탈출, 멈춤 감시, 부팅 때 USB 설정 되돌리기(SB-D7 — 아래 "함정") |
| `main/sc_store.cpp` | 전화기 전용 설정 칸 `nvs_phone` + 플래시 일꾼 |
| `main/audio_board_cores3.cpp` | 소리 기판 부분(AW88298 앰프) |
| `main/sc_assets.h`·`sc_sprites.h` | 구운 글자·그림(`tools/gen_sc_assets.py`) |
| `../tab5_phone/main/` 의 `audio_out.cpp`·`voice.cpp`·`tone_out.cpp`·`relay_link.cpp`·`device_id.cpp`·`voice_cmd.cpp` 와 순수 헤더들 | Tab5 와 함께 쓴다. 칩마다 다른 값은 `board_profile.h` 한 곳 |

태스크(코어): `audio` 스피커 출력(1) · `v_cap` 마이크 → 에코 제거(1) · `v_enc` Opus 압축(0) · `v_rx` Opus 해제(0) · `v_tx` 보내기(0) · `app` 화면·통화 진행(1) · `touch`(1) · `wifi`(0) · `relay`·`relay_ws`(0) · `flash` 플래시 일꾼(0) · `sercmd`(0).

**함정 — 스택이 PSRAM 인 태스크는 플래시를 만지면 안 된다.** ESP32-S3 는 플래시를 읽고 쓰는 동안 캐시를 꺼서 PSRAM 을 못 읽는다(IDF 가 assert 로 멈춘다). 내부 메모리가 빠듯해 `app`·`touch`·`sercmd`·`wifi`·`relay` 의 스택을 PSRAM 에 두었으므로, NVS·부팅 대상 바꾸기는 반드시 `scstore::run`(내부 스택의 일꾼)을 거친다.

**함정 — 스트림덱이 남긴 USB 설정.** UIFlow2 는 칩의 USB 단자를 USB-OTG 로 쓰면서 USB-Serial/JTAG 쪽을 "풀업 끔 고정"으로 둔다(`USB_SERIAL_JTAG.conf0 = 0x100`). 스트림덱의 `machine.reset()` 은 CPU 만 다시 켜서 이 값이 전화기 펌웨어까지 따라오고, IDF 의 USB 드라이버는 그것을 풀지 않는다 → 전화기는 멀쩡히 도는데 맥의 USB 에 기기가 없다(2026-10-09 첫 설치). 그래서 `app_main` 맨 앞의 `scboot::usb_restore()` 가 USB 단자 설정을 전원 켜질 때 값으로 되돌린다(`sc_boot.cpp` SB-D7). 이 줄을 빼면 스트림덱에서 넘어올 때마다 RST 를 눌러야 USB 가 잡힌다.

## 플래시 구성 (16MB, 이중 부팅)

| 칸 | 시작 | 크기 | |
|---|---|---|---|
| nvs · phy_init | 0x9000 · 0xF000 | | UIFlow2 것 그대로(전화기는 쓰지 않는다) |
| factory | 0x10000 | 0x990000 | UIFlow2 = 스트림덱 |
| sys | 0x9A0000 | 0x100000 | UIFlow2 것 그대로 |
| vfs | 0xAA0000 | **0x140000** | 스트림덱 파일. 4.75MB → 1.25MB 로 줄였다(파일은 그대로 옮긴다) |
| **ota_0** | **0xBE0000** | 0x370000 | **전화기 펌웨어** |
| otadata | 0xF50000 | 0x2000 | 부팅 선택(비어 있으면 스트림덱) |
| nvs_phone | 0xF52000 | 0x6000 | 전화기 설정(음량 단계 등) |
| storage | 0xF60000 | 0xA0000 | UIFlow2 것 그대로 |

부트로더는 UIFlow2 의 것을 그대로 쓴다. **`idf.py flash` 는 막아 두었다** — IDF 가 고르는 기본 앱 자리가 UIFlow2 자리라 스트림덱이 지워진다.
**`idf.py erase-flash` 도 쓰지 않는다** — 막을 방법이 없어 막지 못했다(스트림덱까지 전부 지운다).

## 빌드

```bash
cd firmware/stackchan_phone
source ~/esp/esp-idf-5.5.4/export.sh
idf.py build            # build/stackchan_phone.bin (약 1.8MB — 전화기 칸의 50%)
```

`main/secrets.h` 는 Tab5 프로젝트의 것(`../tab5_phone/main/secrets.h`, git 제외)을 함께 쓴다 — `PHONE_DEVICES` 에 스택짱 줄(MAC·번호 3·`stackchan-2`·토큰)이 있어야 교환기에 붙는다.

## 교환기에 등록 (한 번 — 사용자가 직접. **2026-10-09 13:59 끝남**, 아래는 기록·다시 할 때)

스택짱이 교환기에 붙으려면 토큰이 있어야 한다. 비밀 값을 만드는 일이라 Claude Code 가 대신 실행하지 못한다(자동 안전 검사). **맥미니 터미널에서 사용자가:**

```bash
cd ~/tab5-toy-phone && python3 server/tools/phone_token_rotate.py --add-device stackchan-2 --unit 3 --mac AA:BB:CC:DD:EE:03
python3 server/tools/phone_token_rotate.py --check        # tab5-1 · tab5-2 · stackchan-2 모두 "일치"
```

기존 Tab5 토큰은 바뀌지 않고, 화면에 토큰이 찍히지 않는다. 그 뒤(손 불필요): 이 펌웨어를 다시 빌드 → `tools/sc_flash.sh` → `server/deploy/phone_relay_prod.sh restart relay`(통화 중이 아닐 때). 그 전까지 스택짱은 "등록되지 않은 기기예요" 화면에 머문다(설치·소리·스트림덱 오가기는 된다).

## 설치 (처음 한 번 — 사람 손이 필요하다. **2026-10-09 14:08~14:12 끝남**)

1. **스택짱의 RST 를 초록 불이 켜질 때까지(약 3초)** → 다운로드 모드. 짧게 누르면 그냥 다시 켜진다(스트림덱이 다시 뜬다 — 그때는 한 번 더).
2. `tools/sc_install.sh` — MAC·파티션 표 확인 → 스트림덱 파일(vfs)을 작은 vfs 로 다시 만들기 → 파티션 표·vfs·전화기 앱 쓰기 → 스트림덱이 뜨는지 확인 → "전화기" 타일 올리기 → 전화기로 넘어가 확인. `--dry-run` 으로 쓰지 않고 볼 수 있다(먼저 한 번 돌릴 것 — 9초).
   - vfs 는 통째로 읽지 않는다: 기기의 vfs 가 백업의 vfs 와 같은지 **기기에서 계산한 MD5**(`verify-flash`)로 견주고, 같으면 백업에서 만든다. 다르면 기기에서 읽는다(이 USB 로는 11KB/s — 약 7분).
   - **esptool 에 속도(`-b`)를 주지 않는다**: esptool 5.2.0 은 칩 내장 USB 에서 `-b 460800` 뒤의 `read-flash` 가 첫 4KB 에서 멈춘다(`Serial data stream stopped`). 쓰기는 옵션 없이도 1.5~2.9Mbit/s 로 나온다.

## 앱 갱신 (손이 필요 없다 — 지금 무엇이 도는지에 따라)

| 지금 도는 것 | 도구 | 걸리는 시간(2026-10-09 실측) |
|---|---|---|
| 전화기 펌웨어 | `tools/sc_flash.sh` — esptool 로 `0xBE0000` 에만 쓰고 다시 켠다 | 약 24초 |
| 스트림덱 | `/opt/homebrew/bin/python3 tools/sc_flash_deck.py [--go]` — MicroPython 의 raw REPL 로 `ota_0` 칸에 4KB 씩 쓰고 기기에서 SHA256 을 다시 계산해 견준다. `--go` 면 전화기로 넘어간다. 도는 동안 스트림덱 화면은 서 있고, 끝나면 성공이든 실패든 기기를 다시 켠다. 반쯤 쓰인 앱으로는 부팅되지 않는다(부팅 대상을 스트림덱으로 못박고, 앱의 첫 블록을 맨 마지막에 쓴다) | 1.8MB 에 약 46초 |

**스트림덱(UIFlow2)이 도는 기기에는 esptool 을 걸지 않는다** — USB 에서 사라져 전원을 껐다 켜야 돌아온다(2026-10-08 세 번 확인). `sc_install.sh`·`sc_flash.sh` 는 그 상태면 거절하고, `sc_flash_deck.py` 는 반대로 스트림덱일 때만 돈다.
스트림덱 → 전화기를 손 없이 넘기려면 타일과 같은 동작인 `curl 'http://192.168.0.171/press?id=phone'`, 전화기 → 스트림덱은 `tools/sc_ctl.py run deck`.

## 복구

| 증상 | 조치 |
|---|---|
| 전화기 펌웨어가 켜지자마자 죽는다 | 세 번 연달아 죽으면 **스스로 스트림덱으로 돌아간다**(`sc_boot.cpp` SB-D5). USB 로그의 `SCBOOT reset=… crashes=…` 를 본다 |
| 위의 자동 복귀가 안 되는 죽음(`app_main` 에 닿기 전 — PSRAM 초기화 실패 등. `SCBOOT` 줄이 아예 안 찍힌다) | USB 로: `esptool --chip esp32s3 -p <포트> --after watchdog-reset erase-region 0xF50000 0x2000`(= 스트림덱부터). 전화기 펌웨어는 칩 내장 USB 라 죽는 중에도 esptool 이 붙는다 |
| 전화기 화면이 얼었다 | 20초 뒤 스스로 다시 켜진다(SB-D6). 되풀이되면 위와 같이 스트림덱으로 |
| 전화기 화면은 뜨는데 맥의 USB 에 기기가 없다(`sc_ctl.py which` → `none`) | `usb_restore()`(SB-D7)가 없는 옛 앱이 올라가 있다. RST 를 **짧게** 한 번(하드웨어 재시작은 USB 설정을 지운다 → 전화기가 USB 에 잡힌 채 다시 뜬다) 또는 화면의 "스트림덱" 키캡으로 돌아간 뒤 `sc_flash_deck.py` |
| 전화기에서 스트림덱으로 못 간다 | `tools/sc_ctl.py run deck`, 안 되면 `esptool … erase-region 0xF50000 0x2000` 뒤 재시작(= 스트림덱부터) |
| 전부 오늘 아침 상태로 | RST 3초 → `esptool --chip esp32s3 -p <포트> --before no-reset --after watchdog-reset write-flash 0 backups/stackchan2_before-phone-probe_2026-10-08.bin` (전화기는 사라지고 스트림덱만 남는다) |
| 스트림덱의 "전화기" 타일을 없애려면 | `~/Deskbot/firmware/stackdeck/main.py` 의 `PHONE_TILE = False` → `deploy.py` |

M5Burner 로 UIFlow2 를 다시 구우면 이 구성(파티션 표·전화기)은 사라진다 — 다시 `sc_install.sh`.

## 시리얼 명령·조작 도구

`tools/sc_ctl.py`(Tab5 의 `phone_ctl.py` 를 불러 쓴다 — 단계 문법이 같다). 전화기 펌웨어가 돌 때만 쓴다.

```bash
python3 tools/sc_ctl.py which                                   # phone | deck | none
python3 tools/sc_ctl.py run 'until:stat:^STAT .*fsm=IDLE:60' 'shot:/tmp/sc.png'
python3 tools/sc_ctl.py pair '3:tap:238:110' '2:wait:^FSM IDLE -> INCOMING:10'   # 스택짱(3:) + Tab5 2번(2:)
```

명령: `stat` · `t <x> <y>` · `shot` · `reboot` · `deck` · `mem` · `cpu [초]` · `vloop on|off`(상대 없이 통화 소리 길 돌리기) · `quiet on|off` · `volup`·`voldn`·`volstep [1..5]` · 그리고 Tab5 와 같은 소리 명령(`selftest`·`tune`·`vping`·`mic`·`opus`… — `voice_cmd.cpp`).
`STAT` 줄은 Tab5 와 칸 이름·순서가 같고, 끝에 스택짱 칸이 붙는다: `peer1` `call_peer` `deck` `stk_venc` `v_encdrop` `stk_flash` `link`.
부팅 줄: `SCBOOT running=ota_0 deck=1` → `SCBOOT reset=<이유> crashes=<n>` → `PHONE BOOT unit=3 id=stackchan-2 board=stackchan` → `AUDIO ready …` → `VOICE ready … aec=…` → `MEM …` → `PHONE READY`.

화면의 버튼 자리(가운데 좌표 — `main/sc_ui_layout.h`): 대기 = 왼쪽 상대 `81,110` · 오른쪽 상대 `238,110` · 스트림덱 `81,208` / 거는 중 취소 `160,197` / 전화 옴 받기 `81,195` · 거절 `238,195` / 통화 중 작게 `42,197` · 끊기 `160,197` · 크게 `278,197`.

## 맥에서 도는 시험·미리보기 (ESP-IDF 를 export 하지 않은 셸)

```bash
cd firmware/stackchan_phone
clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I main -I ../tab5_phone/main tools/test_sc_ui_layout.cpp -o /tmp/t && /tmp/t   # 144
/opt/homebrew/bin/python3 -m pytest -q -p no:cacheprovider tools/test_sc_ctl.py tools/test_sc_flash_deck.py    # 6 + 17
out/venv/bin/python -m pytest -q -p no:cacheprovider tools/test_sc_dualboot.py                                 # 23 (littlefs-python 이 든 가상환경 — sc_install.sh 가 만든다)
/opt/homebrew/bin/python3 tools/render_sc_screens.py        # out/screens/*.png, sheet.png — 기기와 같은 그리기 코드. 조각 다시 그리기가 통째 그리기와 픽셀까지 같은지도 견준다
/opt/homebrew/bin/python3 tools/gen_sc_assets.py            # 글자·그림을 다시 굽는다(그림 원본은 ../tab5_phone/tools/toy_art.py)
```

## 기기에서 확인한 것 (2026-10-09 — 지금 기기에 있는 빌드: 1,795,392 B · sha256 `fc7467d6…`)

- 설치(`sc_install.sh`), 줄인 vfs 로 스트림덱이 뜸, "전화기" 타일, UIFlow2 부트로더(IDF 5.4.2)가 IDF 5.5.4 앱을 `ota_0` 에서 띄움.
- 교환기 접속(`proto:2`), 대기 화면(상대 둘 켜짐 — `out/dev/2026-10-09_idle.png`), 스택짱에서 건 벨이 Tab5 두 대에 감(`relay.log` 14:23).
- 오가기: 화면의 "스트림덱" 키캡(사용자) 1회 + 손 없이 왕복 2회 — 전화기 → 스트림덱 약 16초(`/health` 까지), 스트림덱 → 전화기는 USB 에 3초 · 교환기 접속까지 약 12초.
- 앱 갱신 두 길(`sc_flash.sh` 1회 · `sc_flash_deck.py --go` 2회 — 마지막 것이 지금의 빌드). 독립 검수 84점 "고치고 올린다" → 지적 반영(실패해도 스트림덱을 다시 켜기 · 앱 머리를 마지막에 쓰기 · 가짜 기기 시험).
- **상대 없이 통화 소리 길 돌리기**(`vloop on` — Tab5 README "되돌림 시험"): 시험음 26초에 보냄 1,248 = 받음, 잃음·버림·굶음 0, `aud_late` 0, `v_encdrop` 0. 처리 시간 에코 제거 6.9ms/16ms · 압축 7.6~7.9ms/20ms · 해제 1.9ms. **부하 코어 0 50% · 코어 1 60%.** 스택 여유 `stk_venc` 7,068/28K · `stk_vcap` 20,240/24K · `stk_vrx` 40,312 · `stk_vtx` 7,364. 내부 메모리 최저 65,927 B(통화 전과 같음). 마이크를 그대로 되돌린 채(음량 5칸) 맥 스피커로 9초 말소리를 들려줘도 마이크 넘침 0, 말이 끝난 뒤 되풀이되는 소리 없음.
- 스피커를 켠 자가 점검: `SELFTEST amp=1 mic=434 ref=134 aec=16~21 ok=1`(에코 제거가 0.64초 안에 26~29dB 줄인다).
- 대기 중 수치: `heap_int_min` 약 66KB · `stk_vcap` 20,320/24K(스피커 끈 자가 점검 뒤) · `stk_venc` 27,852/28K · `stk_flash` 7,412/8K · `aud_late` 0 · `aud_err` 0.

## 아직 확인하지 못한 것

- **진짜 통화** — 한 번도 연결된 적이 없다: 통화음이 사람 귀에 어떻게 들리는지, 두 기기 사이의 메아리·하울링, 망을 거칠 때의 부하·지연·내부 메모리(`heap_int_min` — 음성 프레임의 TLS 송수신이 더해진다), 받기 화면(전화 옴)·통화 중 화면. 소리 길 자체의 부하·스택·끊김은 위 "상대 없이 돌리기"로 쟀다.
- 스피커를 끈 채(`quiet on`)의 자가 점검은 이 기판에서 판정할 수 없다(`ref=1` → 늘 `ok=0` — 기준 신호가 앰프 출력에서 온다).
- `stk_flash`: "스트림덱"으로 갈 때 9MB 이미지 검사를 도는 일꾼의 스택 — 갔다 온 뒤에는 값이 새로 시작하므로 가는 순간의 최저값은 못 쟀다(네 번 다녀오는 동안 죽지는 않았다).
- 터치 감도·받기/거절 0.6초 가드의 실제 느낌, 오래 켜 둔 상태(메모리 추이).
- **스트림덱으로 간 직후 약 40초는 상대 화면에 "켜짐"으로 남는다**(연결을 닫지 않고 재시작 → 교환기가 무응답으로 알아챌 때까지). 개선 후보 — 설계서 진행 로그 2026-10-09.
