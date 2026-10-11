# 맥북 인계 — 전화기 3대: Tab5 두 대에 새 펌웨어 올리기

- **Owner**: (owner)
- **작성**: 2026-10-08 밤, 맥미니의 Claude Code 세션 → **읽는 쪽: 맥북의 Claude Code 세션**(Tab5 두 대가 집에서 맥북에 꽂힌다)
- **상태**: active — **Tab5 두 대 모두 올리고 소리 없는 확인까지 끝(2026-10-08 1번 21:56 · 2번 22:07, 맥북, 같은 바이너리 `0a0d9f8`)**. **Tab5 끼리의 통화는 사용자가 확인(2026-10-09 "통화 잘 나눴어요" — 통화 중 기록 숫자는 재지 않음)**. 남은 것: §4(스택짱이 준비된 뒤 세 대 시험) · 통화음 P5A 의 구체 귀 판정
- **맥북 진행(2026-10-08 22시)**: 꽂혀 있던 것이 1번뿐이라 1번부터 했다(§2 — 한 대만 먼저 올려도 된다). 맥미니 SSH 가 Tailscale 재인증 대기라 꾸러미를 못 가져와 **맥북에서 빌드한 `0a0d9f8`** 을 올렸다: `firmware/tab5_phone/out/fw_2026-10-08_3way_0a0d9f8/build/`(맥북에만, 3,091,888 B, SHA256 앞 16자 `6fc1e7a533a43e20`). 맥미니 꾸러미와 소스는 같고 바이너리는 다르다 → **2번에도 이 맥북 꾸러미를 올렸다(22:07)**. 다시 올릴 일이 있으면 아래 "다"의 `out/fw_2026-10-08_3way` 자리에 `out/fw_2026-10-08_3way_0a0d9f8` 를 쓴다. 확인한 것·못 한 것은 `firmware/tab5_phone/README.md` "전화기 3대 — 화면"의 21:56 줄. 되돌릴 빌드: 1번 `out/fw_prev_1a75277/build/`, 2번 `out/fw_2026-10-08_p5a/build/`.
- **설계(정본)**: [2026-10-08-phone-3way-design.md](2026-10-08-phone-3way-design.md) · 지시서 [2026-10-08-phone-3way-stackchan-brief.md](2026-10-08-phone-3way-stackchan-brief.md) · 일지 `docs/daily/2026-10-08/claude.md`

사용자 지시(2026-10-08 20:02): "탭5는 집에 도착해서 맥북과 연결하여 작업을 진행할 때 스택짱까지 완료하고 맥북이 이어서 할 수 있도록 동기화를 해줘."

## 1. 지금 어디까지 됐나

| 덩어리 | 상태 | 어디 |
|---|---|---|
| 교환기 v2 | **운영 중**(2026-10-08 20:11 반영, 검수 93점, pytest 481). 옛 펌웨어(v1)의 Tab5 두 대는 "옛 짝"으로 예전과 똑같이 통화된다 | `server/phone_core.py`·`phone_relay.py`, 운영 문서 `server/deploy/README.md` |
| 공통 헤더 v2 | 끝(맥 시험 통과) | `firmware/tab5_phone/main/roster.h`·`call_fsm.h`·`relay_proto.h`·`device_id.h` |
| 공용 소리 코드 정리 | 끝(검수 95점 — Tab5 동작 불변). **기기에서는 아직 안 돌려 봄** | `audio_board.h`·`audio_board_tab5.cpp`·`board_profile.h`·`voice_cmd.cpp`, `voice.cpp` 의 `Encoder` |
| Tab5 화면(상대 둘·로봇·BUSY) | 끝(독립 검수 97점, 맥 시험 `ui_layout` 176·도구 pytest 263, 화면 미리보기 44장, 빌드 3.09MB). **기기에서는 아직 안 돌려 봄 — 맥북의 일** | `ui_layout.h`·`ui.cpp`·`toy.*`·`app.cpp`, 미리보기 `firmware/tab5_phone/out/screens/_sheet_3way.png`(다시 만들기: `tools/render_screens.py`) |
| 스택짱 전화기 펌웨어 | 빌드·맥 시험·화면 미리보기까지. **기기에 미설치** | `firmware/stackchan_phone/`(README 가 정본) |
| 스택짱 설치 | **사용자 손 대기**: RST 3초 → `tools/sc_install.sh` | 설계서 §13 |
| 스택짱 토큰 | **사용자 손 대기**: `phone_token_rotate.py --add-device …` | 설계서 TW-D22 |

**맥북이 할 일은 Tab5 두 대에 새 빌드를 올리고 확인하는 것**이다(설계서 §14 "다"의 뒷부분). 스택짱은 사무실 맥미니에 꽂혀 있고 맥미니 세션이 맡는다.

## 2. 먼저 알아둘 것

- **교환기가 이미 v2 다.** 새 펌웨어는 접속 때 `X-Phone-Proto: 2` 를 보내고 걸 때 상대를 고른다(`{"t":"call","to":"<기기ID>"}`). 한 대만 먼저 올려도 된다 — 새 펌웨어(v2)와 옛 펌웨어(v1)는 Tab5 끼리(옛 짝) 서로 보이고 통화된다.
- **스택짱(3번)은 화면에 "꺼짐(잠든 로봇)"으로 보이는 것이 정상**이다 — 아직 설치·등록 전이고, 설치 뒤에도 평소에는 스트림덱으로 돌기 때문이다(그때는 교환기에 붙지 않는다).
- **같은 바이너리 원칙**: 빌드는 한 번만 하고 두 기기에 같은 것을 올린다. `idf.py flash` 대신 빌드 폴더에서 `write_flash @flash_args`(이유는 `firmware/tab5_phone/README.md` "두 기기에는 항상 같은 바이너리").
- **되돌릴 빌드**: `firmware/tab5_phone/out/fw_2026-10-08_p5a/build/`(오늘 낮까지의 최신 — 양쪽 맥에 있다).
- 지켜야 할 것(지시서 §4 그대로): `secrets.h`·`.env.phone` 의 내용을 화면·로그·채팅·커밋에 내지 않는다 · **git 태그를 만들지 않는다** · 커밋은 devsync 가 한다 · 걸기 시험은 상대 기기의 벨을 실제로 울린다(사람이 옆에 있으면 먼저 알린다) · 방전된 배터리의 Tab5 가 맥 포트에서 8초마다 재부팅하면 배터리를 빼 달라고 한다(2번은 배터리를 빼 둔 채다 — USB 를 뽑으면 꺼진다).
- 사용자는 낮 업무 시간에는 소리 나는 시험을 싫어한다. 집·저녁이면 물어보고 한다.

## 3. 순서

### 가. 동기화 확인
```bash
cd ~/tab5-toy-phone && git pull            # 평소에는 devsync 가 한다
git log --oneline -3
ls firmware/tab5_phone/main/roster.h firmware/tab5_phone/main/audio_board_tab5.cpp firmware/tab5_phone/main/voice_cmd.cpp firmware/stackchan_phone/README.md
```
넷 다 있어야 한다. `firmware/tab5_phone/main/secrets.h` 는 git 에 없다 — 맥북에 이미 있는 것을 그대로 쓴다(Tab5 는 자기 줄만 있으면 된다. 스택짱 줄은 Tab5 빌드에 필요 없다).

### 나. 맥에서 도는 확인(기기 없이)
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone/tools
SDK="$(xcrun --sdk macosx --show-sdk-path)"; for T in wifi_store call_fsm relay_proto device_id roster ui_layout tones pcm jitter_buffer voice_core voice_tune tx_dyn vol_steps relay_diag howl_loop raster; do clang++ -std=c++17 -Wall -Wextra -isysroot "$SDK" -I../main test_$T.cpp -o /tmp/t_$T && /tmp/t_$T | tail -1; done
/opt/homebrew/bin/python3 -m pytest -q -p no:cacheprovider test_phone_ctl.py test_relay_tls_shape.py
cd .. && /opt/homebrew/bin/python3 tools/render_screens.py      # 화면 미리보기 — 새 대기 화면을 눈으로
```
전부 통과·경고 0 이어야 한다(기준선 숫자는 `firmware/tab5_phone/README.md` 의 가장 아래 줄).

### 다. 빌드 한 번 → 보관
```bash
cd ~/tab5-toy-phone/firmware/tab5_phone && source ~/esp/esp-idf-5.5.4/export.sh && idf.py build
mkdir -p out/fw_2026-10-08_3way && rm -rf out/fw_2026-10-08_3way/build && cp -R build out/fw_2026-10-08_3way/build
```
`main/` 경고 0.

맥미니에서 이미 빌드해 둔 꾸러미를 가져다 써도 된다(커밋 `1384b6e` 에서 빌드, 버전 문자열 `1384b6e`, `tab5_phone.bin` 3,091,888 B, SHA256 앞 16자 `e7bf5388a78599a6` — 꾸러미 안 `SHA256SUMS`):
```bash
scp -r user@100.64.0.1:m5tab5/firmware/tab5_phone/out/fw_2026-10-08_3way ~/tab5-toy-phone/firmware/tab5_phone/out/
cd ~/tab5-toy-phone/firmware/tab5_phone/out/fw_2026-10-08_3way/build && shasum -a 256 -c SHA256SUMS
```
(2026-10-08 밤에 맥미니 → 맥북으로 밀어 넣으려 했으나 맥북이 꺼져 있어 닿지 않았다.) 어느 쪽이든 **두 기기에 같은 것**을 올린다.

### (참고) 새 화면에서 달라진 것 — 시험 단계를 쓸 때
- 대기 화면의 통화 키캡이 둘이다: **왼쪽 = slot 0 `tap:368:472`, 오른쪽 = slot 1 `tap:912:472`**(옛 가운데 버튼 640,360 은 없다). slot 은 나를 뺀 번호 오름차순 — 2번에게는 왼쪽 = 1번, 오른쪽 = 3번(스택짱).
- `STAT` 줄 끝에 ` peer1=<0|1> call_peer=<slot>` 이 붙었다. **`call_peer=1` 안에 `peer=1` 글자가 들어 있다** → 정규식은 빈칸을 붙여 ` peer=1` 로 쓴다(`phone_ctl.py` 설명글·README 는 고쳐져 있다. 앞 계획서들의 옛 명령은 그대로다).
- 벨 화면이 대기로 바뀐 직후 **1.5초**는 통화 키캡을 받지 않는다(받기·거절 자리와 겹쳐서) → 자동 시험에서 그 뒤의 통화 탭 앞에는 `sleep:2`.
- 안내 알약이 맨 윗줄로 올라갔다. 새 안내 `BUSY` = "통화 중이에요".
- 기록 줄이 늘었다: `PEER slot=<n> id=<ID> online=<0|1>` · `RING slot=<n> from=<ID>` · `CALL slot=<n> to=<ID>` · `SIGDROP ev=<peer|ring> id=<ID|->`.

### 라. 2번 Tab5 먼저 올리기 → 확인
올리는 명령은 `firmware/tab5_phone/README.md` 의 "올리기"(MAC 확인 → `write_flash @flash_args`)를 `out/fw_2026-10-08_3way/build` 에서 그대로.

확인(2번만 새 펌웨어, 1번은 아직 옛 펌웨어인 상태):
1. 부팅 줄 `PHONE BOOT unit=2 id=tab5-2`, 교환기 기록(맥미니 `~/Library/Logs/tab5phone/relay.log`)에 `{"ev":"online","dev":"tab5-2","proto":2}`.
2. `stat` → `fsm=IDLE`, 그리고 줄 끝의 새 칸. 기록에 `PEER slot=0 id=tab5-1 online=1`(스택짱이 교환기에 등록된 뒤에는 `PEER slot=1 id=stackchan-2 online=0` 도 — 등록 전에는 교환기가 알리지 않아 이 줄이 없고 `peer1=0` 으로만 보인다. 2026-10-08 1번에서 확인). **`SIGDROP ev=peer id=-` 가 이어지면** 교환기가 v1 로 보고 있다는 뜻이다(헤더가 빠졌다).
3. 화면 찍기(`shot`): 대기 화면에 상대 둘 — 왼쪽 1번(곰, 켜짐), 오른쪽 3번(로봇, 잠듦).
4. 2번 → 1번 걸기·받기·통화·끊기(1번은 옛 화면 그대로 벨이 울린다). 1번 → 2번도.
5. 꺼진 상대(로봇) 키캡을 누르면 "상대 전화기가 꺼져 있어 걸 수 없어요" 안내.
6. 소리 장치 정리가 Tab5 에서 탈 없는지: 부팅 줄 `AUDIO ready … mic=24 amp=1`, `VOICE ready … aec=…`, 통화 중 `stat` 의 `aud_late=0`·`v_txfail=0`, `selftest`(조용한 모드로) `ok=1`.

### 마. 1번 Tab5 올리기 → 둘 다 v2 로 확인
- 1번의 대기 화면: 왼쪽 2번(토끼), 오른쪽 3번(로봇, 잠듦).
- 1 ↔ 2 걸기·받기·거절·메시지 거절·통화·끊기, 음량 작게/크게. `phone_ctl.py pair` 의 좌표 예시는 그 파일의 설명글(새 배치에 맞춰 고쳐져 있다).
- 통화음(P5A — 쉼·겹침 누르기, 받는 목소리 2배)은 사용자의 귀 판정이 아직 없다. 값은 `docs/exec-plans/2026-10-08-tab5-phone-audio-p5a.md` §8.

### 바. 끝나면
- `firmware/tab5_phone/README.md` 의 "지금 기기에 올라간 빌드" 줄, 설계서 §14 "다"·§15 체크, 일지(`docs/daily/<날짜>/claude.md`)를 고친다.
- 맥미니 세션에 알린다(`/list-agents` → SendMessage — 홈 `CLAUDE.md` "다중 세션 협업 규칙"): 두 대가 v2 로 올라갔다는 것.

## 4. 세 대를 함께 보는 시험(스택짱이 준비된 뒤)

스택짱이 전화기로 돌고(사무실, 맥미니 세션이 `firmware/stackchan_phone/tools/sc_ctl.py` 로 조작) Tab5 두 대가 v2 면:
- Tab5 의 대기 화면에 로봇이 깨어난다 → 로봇 키캡으로 걸기 → 스택짱에 벨(건 쪽 캐릭터가 뜬다) → 받기 → 통화 → 끊기.
- 스택짱 → Tab5 걸기. Tab5 끼리 통화 중일 때 스택짱이 걸면 "통화 중이에요"(busy).
- **스택짱의 통화음은 처음이다** — 에코·크기·끊김을 사용자가 귀로 본다. 스택짱 쪽 숫자는 `stat` 의 `aud_late`·`v_encdrop`·`heap_int_min`, `cpu 10`(코어별 60% 이하가 기준).

스택짱 없이 Tab5 한 대로 "상대가 셋째 기기일 때"의 화면을 보려면 가짜 기기: `python3 server/tools/phone_fake_peer.py --proto 2 stackchan-2 …`(쓰는 법은 그 파일의 설명글과 `server/deploy/README.md`). 스택짱의 토큰이 `.env.phone` 에 있어야 한다 — **토큰을 더하는 것은 사용자가 맥미니에서 직접** 하고(TW-D22), 맥북에서 쓰려면 두 비밀 파일을 사용자가 옮긴다.

## 5. 맥미니 쪽에 남은 일(참고)

1. 사용자: 스택짱 RST 3초 → 맥미니 세션: `firmware/stackchan_phone/tools/sc_install.sh`(스트림덱 확인 → "전화기" 타일 올리기 → 전화기로 넘어가 확인).
2. 사용자: `cd ~/tab5-toy-phone && python3 server/tools/phone_token_rotate.py --add-device stackchan-2 --unit 3 --mac AA:BB:CC:DD:EE:03` → `--check`.
3. 맥미니 세션: 스택짱 다시 빌드 → `tools/sc_flash.sh` → `server/deploy/phone_relay_prod.sh restart relay`(통화 중이 아닐 때 — 재시작하면 붙어 있던 전화기가 1~2초 끊겼다 다시 붙는다) → 기록의 `devices=tab5-1,tab5-2,stackchan-2`.
4. 사용자에게 확인받을 것: 설계서 §13.
