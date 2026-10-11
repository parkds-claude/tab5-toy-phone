# tab5_phone — Tab5 2대 음성 전화기

두 대에 같은 펌웨어를 올린다. 기기 번호는 칩의 MAC 으로 정해진다(1번 `AA:BB:CC:DD:EE:01`, 2번 `AA:BB:CC:DD:EE:02`).
지금 단계(Phase 3b)는 걸기 → 벨 화면 → 받기/거절 → 통화 화면 → 끊기에 **신호음**(받는 쪽 벨, 거는 쪽 연결음, 통화가 끝났을 때 종료음)과 **목소리**가 붙었다: 받으면 두 기기가 스피커폰으로 동시에 말하고 듣는다(마이크 → 에코 제거 → Opus → 교환기 → 상대 스피커). 받기 전에는 마이크를 읽지 않고, 끊으면 그 자리에서 닫힌다.
**사람 귀로 확인된 것과 안 된 것**(2026-10-05 아침, 같은 책상에서 한 번에 한 대의 스피커만 켜고 한 시험 — 상세는 계획서 `…-phase3b-voice.md` 진행 로그): 스피커에서 실제로 소리가 나고 그 기기의 마이크가 들었다는 **기록**이 있고(`SELFTEST amp=1` 의 `mic`(빌드 `e40d097`) — 1번 3280 ↔ 조용할 때 12, 2번 1211 ↔ 조용할 때 26), 사용자가 통화해 보고 "2번은 잘 들림", "동시에 말해도 잘 됨. 다만 통화품질은 좀 아쉽긴 함"이라 했고 아쉬운 점으로 "먹먹함·작음", "메아리·지연"을 골랐다(끊김·뭉개짐, 찢어지는 소리는 고르지 않았다). **답을 받지 못한 것**: 벨·연결음·종료음의 음량·잡음, 벨 끄기로 실제 소리가 그치는지, 메아리 제거를 거친 소리와 마이크 그대로의 A/B 비교, 양쪽 스피커를 켜고 서로 다른 방에서 한 통화의 느낌, 같은 방 하울링. (→ 양쪽 스피커를 켜고 서로 다른 방에서 한 통화의 느낌은 2026-10-05 09시대에 답을 받았다: "먹먹하게 작은 소리로, 아주 멀리있는 것처럼 들림." — 아래 갱신 참조) 점검표 14문항 가운데 답이 있는 것은 일부다. 아래 "신호음" 절의 소리 모양·크기는 음량·잡음에 대한 사용자 답이 없어 설계 값 그대로다. **지금 알려진 부족함**: 통화 품질(먹먹함·작음)과 지연(아침에는 한쪽 길 약 0.3초), 내부 메모리가 기준 아래(아래 "맥 단위 시험"의 측정) — 다음은 3c(품질 다듬기) 계획 `docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md`(작성 중 — 2026-10-05) **→ 2026-10-05 오전 갱신: 그 계획서는 작성됐고, 첫 묶음(받는 목소리를 크게·또렷하게)은 이미 실행돼 두 기기에 `771b9fe` 로 올라가 있다 — 아래 "통화 품질 손잡이 (Phase 3c 첫 묶음)" 절.** 또 **서로 다른 방에서 양쪽 스피커를 켜고 한 통화(빌드 `5935ddd`, `txgain=4 vol=100`)에 대한 사용자 판정**을 받았다(2026-10-05 09시대): "먹먹하게 작은 소리로, 아주 멀리있는 것처럼 들림." (컨트롤러의 판독, 사용자의 말이 아님: 위 "먹먹함·작음"이 같은 책상 배치만의 탓은 아닌 것으로 보인다.) 그때 메아리·지연은 말하지 않았고 "없다"는 뜻인지는 묻지 않았다. 신호음 음량·잡음과 A/B 비교는 여전히 미답이다.
받는 쪽 벨 화면에는 **벨 끄기**(그 전화의 벨소리만 끈다 — 누른 뒤 그치도록 만들었다(3a 설계 값은 최대 0.1초, 3b 에서 출력 DMA 가 6칸 → 3칸이 되어 약 0.03초 — `main/audio_out.cpp` 의 `DMA_DESC` 주석을 소스로 판독한 값) — 이것은 드라이버 소스로 본 설계 값이고 기기에서 재지 않았다. 기록은 `tone=NONE` 으로 바뀌는 것까지만 보여 주며, 실제 소리가 그치는지는 2026-10-05 에도 사용자 답이 없다)와 **메시지로 거절**(문구 3개 중 하나로 거절 → 건 쪽 대기 화면에 "상대: …" 10초)도 있다.

**전화기 3대(2026-10-08 밤~)**: 전화기는 셋이다 — 1번·2번 = Tab5(이 펌웨어), 3번 = 스택짱(`firmware/stackchan_phone`, 같은 공통 헤더). 대기 화면에 상대 **둘**이 서고, 걸고 싶은 상대의 통화 키캡을 누른다. 아래 "전화기 3대 — 공통 헤더"·"전화기 3대 — 화면" 절. 이 판은 **기기에 올린 적이 없다**(빌드·맥 시험·화면 미리보기까지).

- 설계: `docs/exec-plans/2026-10-03-tab5-phone.md` (진행·결과는 §8)
- 구현 계획: `docs/exec-plans/2026-10-04-tab5-phone-phase2.md`
- 수신거부 메시지·벨 끄기: 설계 `docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md`, 계획 `…-decline-mute-plan.md`
- 신호음(Phase 3a): 계획 `docs/exec-plans/2026-10-04-tab5-phone-phase3a-tones.md` (결정 PL3a-D1~D16)
- 목소리 통화(Phase 3b): 계획 `docs/exec-plans/2026-10-04-tab5-phone-phase3b-voice.md` (결정 PL3b-D1~D27, 지연 예산, CPU·메모리 예산, 진행 로그에 Wi-Fi 절전 N-D4·실측·사용자 귀 판정)
- 참고 구현 조사: `docs/exec-plans/2026-10-04-tab5-phone-research/`

## 구조

| 파일 | 역할 | 시험 |
|---|---|---|
| `main/call_fsm.h` | 통화 상태 기계. 사건을 넣으면 다음 상태와 할 일을 돌려준다 | 맥 `tools/test_call_fsm.cpp` |
| `main/relay_proto.h` | 교환기 신호 읽기·쓰기, 음성 프레임 포장, 재연결 간격 | 맥 `tools/test_relay_proto.cpp` |
| `main/ui_layout.h`, `geom.h` | 어떤 화면인지, 터치가 어느 버튼인지, 터치를 받아들일지. 대기 화면의 상대 둘(통화 키캡 둘 · 이름 알약 · 캐릭터 칸)과 안내 띠의 자리, 그 slot 이 몇 번 기기인지(`peer_unit`·`call_unit`) | 맥 `tools/test_ui_layout.cpp` |
| `main/device_id.h/.cpp` | MAC → 기기 번호·교환기 ID·토큰 | 맥 `tools/test_device_id.cpp` |
| `main/roster.h` | 등록 기기 표(번호 1·2·3 ↔ 교환기 ID `tab5-1`·`tab5-2`·`stackchan-2`)와 상대 slot 규칙(나를 뺀 나머지를 번호 순으로 slot 0·1). 받은 신호의 기기 ID → slot, 걸 상대의 slot → 기기 ID. 스택짱 펌웨어와 함께 쓰는 순수 헤더(아래 "전화기 3대 — 공통 헤더") | 맥 `tools/test_roster.cpp` |
| `main/relay_link.h/.cpp` | 교환기(WebSocket) 연결·재연결·생존 확인 | 기기 |
| `main/net_wifi.h/.cpp`, `wifi_store.h` | Wi-Fi 매니저(저장 목록 + 고정 목록, 신호 센 순 자동 연결) | 맥 `tools/test_wifi_store.cpp` + 기기 |
| `main/raster.h`, `sprite.h` | 픽셀 합성의 순수 부분: 둥근 도형·글자 마스크·색 그림(RLE)·동산·굵은 곡선. 기기와 맥이 같은 코드로 그린다 | 맥 `tools/test_raster.cpp` |
| `main/gfx.h`, `gfx_draw.cpp` | 그리기 함수(도형·글자 자산·색 그림·숫자·시계) — `raster.h` 로만 그린다 | 맥 화면 미리보기 `tools/render_screens.py` |
| `main/gfx.cpp` | 기기 쪽: 합성 버퍼(1280×720) → 한 번에 출력, 스크린샷, 내장 글꼴 글자 | 기기 |
| `main/toy.h/.cpp` | 토이폰 테마의 색과 부품: 바탕(하늘·동산·구름)·키캡·알약·판·캐릭터(곰·토끼·로봇·병아리)·아이콘. 번호 ↔ 캐릭터(`who_of_unit`)의 정본 (아래 "화면 테마") | 맥 화면 미리보기 |
| `main/ui.h/.cpp` | 통화 화면 그리기 | 맥 화면 미리보기 + 기기(스크린샷) |
| `main/ui_wifi.h/.cpp` | Wi-Fi 설정 화면(목록 + 비밀번호 자판) | 맥 화면 미리보기 + 기기 |
| `main/app.h/.cpp` | 터치·교환기 신호·시간 → 상태 기계 → 할 일 실행 → 다시 그리기 | 기기 |
| `main/tones.h` | 신호음의 박자·주파수·크기, 딸깍 방지(5ms), 할 일 → 소리, 기록 줄 고르기 | 맥 `tools/test_tones.cpp` |
| `main/audio_out.h/.cpp` | 소리 장치: 스피커 출력(ES8388 + I2S 송신)과 마이크 입력(ES7210 + I2S 수신 4칸). 출력은 공급자들에게 PCM 을 받아 섞어 내고, 입력은 4칸을 읽어 준다 — 무엇에 쓰는지는 모른다 | 기기 |
| `main/tone_out.h/.cpp` | 신호음 공급자: 명령 우편함 + `TONE` 기록 줄(로직은 `tones.h`) | 기기 |
| `main/pcm.h` | 소리 계산: 섞기(더하고 넘치면 자름), 크기, 세기, 태스크 사이 링버퍼, 출력 박자 감시 | 맥 `tools/test_pcm.cpp` |
| `main/jitter_buffer.h` | 지터 버퍼: 들쭉날쭉 도착하는 음성 프레임을 20ms 박자로 내준다(60ms 로 시작, 40~200ms) | 맥 `tools/test_jitter_buffer.cpp` |
| `main/voice_core.h` | 목소리 통화의 판단: 켜고 끄기, 16ms 묶음 ↔ 20ms 프레임, 순번, 밀리면 버리기, 빠진 프레임 메우기, 시험음·지연 재기 | 맥 `tools/test_voice_core.cpp` |
| `main/voice.h/.cpp` | 목소리 태스크 셋(캡처·해제·송신)과 에코 제거(esp-sr)·Opus 호출. `audio_out` 의 둘째 공급자 | 기기 |
| `main/phone_main.cpp` | 부팅, 태스크 기동, 시리얼 명령 | 기기 |
| `tools/phone_ctl.py` | 시리얼 검증 도구(기기 번호로 선택, 두 기기 시나리오, 스크린샷, 터치 주입) | `tools/test_phone_ctl.py` |
| `tools/gen_ui_assets.py` | 고정 문구를 주아체(Jua)로 구워 `main/ui_assets.h` 를 만든다 | — |
| `tools/toy_art.py`, `tools/gen_toy_sprites.py` | 캐릭터·아이콘 그림의 원본(SVG 조각)과, 그것을 구워 `main/ui_sprites.h` 를 만드는 도구 | — |
| `tools/render_screens.py`, `tools/sim/` | 화면 미리보기: 기기와 같은 그리기 코드를 맥에서 돌려 모든 화면을 PNG 로 만든다 | — |
| `../../server/tools/phone_fake_peer.py` | 교환기에 "기기 한 대"로 붙어 대본대로 신호를 주고받는 가짜 상대 | `../../server/tests/test_phone_fake_peer.py` |
| `../../server/tools/phone_relay_dev.sh` | 개발용 교환기를 켜고 끄는 스크립트(아래 "교환기") | — |

`main/secrets.h`(Wi-Fi 목록 `WIFI_NETWORKS`·교환기 주소 `RELAY_URL`·기기 표 `PHONE_DEVICES` — 형식은 `main/secrets.h.example`), `tools/fonts/`, `out/`, `build/`, `managed_components/`, `sdkconfig`, `dependencies.lock` 은 git 제외. `secrets.h` 의 내용(Wi-Fi 비밀번호, 토큰, 교환기 주소)은 화면·로그·채팅·커밋에 내지 않는다.

## 맥 단위 시험

```
cd tools && SDK="$(xcrun --sdk macosx --show-sdk-path)"
for T in wifi_store call_fsm relay_proto device_id roster ui_layout tones pcm jitter_buffer voice_core voice_tune tx_dyn vol_steps relay_diag howl_loop raster; do clang++ -std=c++17 -Wall -Wextra -isysroot "$SDK" -I../main test_$T.cpp -o /tmp/t_$T && /tmp/t_$T | tail -1; done
python3 -m pytest -q -p no:cacheprovider test_phone_ctl.py      # ESP-IDF 를 export 하지 않은 셸에서
cd ~/tab5-toy-phone/server && python3 -m pytest -q -p no:cacheprovider   # 교환기 + 가짜 상대 (위의 cd tools 와 별개로 따로 실행)
```

- **지금 값(2026-10-08 밤 — 전화기 3대의 화면: 아래 "전화기 3대 — 화면")**: 맥 시험 16종 wifi_store 16 · call_fsm 173 · relay_proto 109 · device_id 16 · roster 51 · **ui_layout 176**(110 + 66) · tones 89 · pcm 42 · jitter_buffer 92 · voice_core 176 · voice_tune 20 · tx_dyn 57 · vol_steps 44 · relay_diag 46 · howl_loop 26 · raster 40, 컴파일 경고 0 / 도구 pytest **263**(`test_phone_ctl.py` 253 + `test_relay_tls_shape.py` 10) / 화면 미리보기 44장(바꾸지 않은 화면 19장은 바꾸기 전과 픽셀까지 같다). 전체 빌드 `tab5_phone.bin` 0x2f2db0(3,091,888 B, 앱 파티션 51% 여유 — 직전 보관 빌드 P5A 2,929,024 B 보다 +162,864 B, 거의 전부 그림 자산(+152,004 B)과 새 글자), 우리 코드 경고 0. 스택짱 프로젝트(`firmware/stackchan_phone` — 이 폴더의 공통 헤더를 끌어 쓴다)도 그대로 빌드된다(0x1b6000, 경고 0). 고친 옛 시험 줄은 그 절에 적었다. **기기에는 올리지 않았다 — 기기 실측은 없다.**
- 그 앞 시점(2026-10-08 저녁 — 전화기 3대의 공통 헤더: `roster.h`·신호 규약 v2. 화면은 그대로였다 — 아래 "전화기 3대 — 공통 헤더"): 맥 시험 16종 wifi_store 16 · **call_fsm 173**(114 + 59) · **relay_proto 109**(61 + 48) · **device_id 16**(9 + 7) · **roster 51**(새) · ui_layout 110 · tones 89 · pcm 42 · jitter_buffer 92 · voice_core 176 · voice_tune 20 · tx_dyn 57 · vol_steps 44 · relay_diag 46 · howl_loop 26 · raster 40, 컴파일 경고 0 / 도구 pytest 259(`test_phone_ctl.py` 249 + `test_relay_tls_shape.py` 10 — 바뀌지 않았다). 옛 시험은 한 줄도 고치지 않고 그대로 통과한다(call_fsm 의 114개·relay_proto 의 61개). 화면 미리보기 31장은 바꾸기 전과 파일이 같다(픽셀 단위). **기기에는 올리지 않았다 — 기기 실측은 없다.**
- 그 앞 시점(2026-10-05 오후 묶음 — 하울링 `guard`·음량 버튼·wss 연결, main 의 `489dd9e`): 맥 시험 14종 wifi_store 16 · call_fsm 114 · relay_proto 61 · device_id 9 · ui_layout 110 · tones 89 · pcm 37 · jitter_buffer 92 · voice_core 176 · voice_tune 18 · tx_dyn 46 · **vol_steps 44**(새) · **relay_diag 46**(새) · **howl_loop 19**(새, 약 15초), 도구 pytest 258 / 교환기 pytest 208(출처: 담당 보고서 셋과 `progress.md` 의 합친 뒤 점검·수정 1회차 줄). 각 담당의 빌드는 경고 0 이었다(하울링 2,663,840 B · 음량 2,665,904 B · 연결 2,676,192 B — 담당 사본에서 따로 만든 크기). **세 가지를 합친 판의 컨트롤러 빌드 크기·기기 실측은 아래 주석 자리에 컨트롤러가 적는다.**
  - **기기 점검(2026-10-05 15:2x, 스피커 끈 채, 빌드 `0ded2c7` 2,679,648 B — 연결은 LAN `ws://`)**: 30초 통화 손실 0.06%·0.13%, 버림 0, `v_under`·`aud_late` 0, 부하 약 36%, 지연 왕복 가운데 값 317·301ms, `stk_vcap` 8,804, `stk_ws` 6,128(평문), `heap_int_min` 130,599·133,299(부팅 직후 대기 138,119·138,075 — 같은 조건의 771b9fe 는 136,923·136,271). 음량 버튼 ①~④(버튼·저장·재부팅 뒤 유지·줄여 둔 기기의 `selftest`·통화 중 누르기) 통과. `wss://` 는 그 뒤 빌드 `1a75277` 로 쟀다(아래 줄). 로그 `out/p4a_*`.
  - **wss 점검(2026-10-05 17:1x~17:3x, 스피커 끈 채, 빌드 `1a75277` 2,679,792 B — 맥미니 교환기 `wss://phone.example.com/ws`)**: 부팅 `RELAYCFG scheme=wss tls=1` → `Certificate validated` → `RELAY up=1`(IP 를 받은 뒤 약 1초), `RELAYERR` 0줄, 부팅 고리 없음. 스택·메모리: 접속 직후 `stk_ws` 5,408(판정선 3,000)·`stk_relay` 약 6,100·`heap_int_min` 약 133,400, 30초 통화 뒤 `stk_vtx` 7,160·`stk_app` 약 10,150·`stk_vcap` 8,804·`heap_int_min` 약 131,300. 30초 통화 손실 0.07%·0.33%, `v_under`·`aud_late` 0, 부하 약 38%. **지연(왕복 `VPING` 가운데 값): `nodelay 0` 509·605ms / `nodelay 1` 379·397ms**(LAN 은 301~317ms). `nodelay 1` 로 3분 통화: 손실 0.05%·끊김 0, `heap_int_min` 120,835·121,239(통화 전보다 약 2,000 낮음 — 계속 내려가는 모양은 아니었으나 3분뿐이다), 통화 중 `heap_int` 121~130k ↔ 끊은 뒤 약 134k. 교환기 재시작 5회·통화 중 재시작 1회 통과(`server/deploy/README.md`). **확인하지 않은 것**: 서로 다른 망(집·핫스팟)의 통화, 한쪽 Wi-Fi 가 끊기는 경우, 10분 넘는 통화와 여러 번 통화 뒤의 내부 메모리(아침 판 `771b9fe` 는 약 5시간 쓰는 동안 부팅 직후 약 140k → 112~123k 로 내려가 있었다 — 원인 미조사), 사용자의 귀 판정. 로그 `out/p4b_*`.
- 직전 시점(3c 첫 묶음, 커밋 `771b9fe` + 시험만 더한 `d3da36f`): wifi_store 16 · call_fsm 105 · relay_proto 61 · device_id 9 · ui_layout 91 · tones 89 · pcm 37 · **jitter_buffer 92** · **voice_core 158** · **voice_tune 17**(새) · **tx_dyn 46**(새), 컴파일 경고 0 / 도구 pytest **237** / 교환기 pytest 137(출처: 3c 구현 보고 `chunk1-impl-report.md` 와 3c 진행 기록의 시험 보강 줄 — 문서 과제에서 위 명령(`voice_tune`·`tx_dyn` 포함)으로 직접 다시 돌려 같은 값을 확인했다). 전체 빌드 `tab5_phone.bin` 0x28a760(2,664,288 B, 앱 파티션 58% 여유), 우리 코드 경고 0, `dependencies.lock` 불변. 아래 줄은 그 앞 시점(`5935ddd`)의 값이다.
- 지금 값(Phase 3b + 수정, 커밋 `5935ddd`): wifi_store 16 · call_fsm 105 · relay_proto 61 · device_id 9 · ui_layout 91 · tones 89 · pcm 37 · jitter_buffer 65 · voice_core 138, 컴파일 경고 0 / 도구 pytest 223 / 교환기 pytest 137 (문서 과제에서 위 명령으로 다시 돌려 같은 값을 확인했다). 전체 빌드 `tab5_phone.bin` 0x2887a0(2,656,160 B, 앱 파티션 58% 여유), 우리 코드 경고 0, `dependencies.lock` 불변.
- 두 기기에서 잰 목소리 비용·품질(값은 `out/p3b_*`(밤, 빌드 `d77ae98`, 스피커 끔)·`out/p3c_*`(아침 `e40d097`)·`out/p3d_*`(아침 `5935ddd`) 로그와 계획서 진행 로그에 있다. **조건이 다르므로 밤 값과 아침 값을 섞어 읽지 않는다**):
  - 내부 메모리: 목소리 없는 `5d9935f` 의 `heap_int` 약 257.3K → 목소리 빌드 부팅 직후 약 115.2K(약 142KB 사용 — 예산 어림 안). `heap_int_min` 은 **밤 3분 통화 중 최저 102,011(1번)로 기준 100,000 위였으나, 아침에 1번 90,395·2번 80,463 으로 기준 아래**(멈춤선 60,000 위). 관측: 통화를 거친 뒤 내부 메모리가 줄고 그 하락이 정확히 1,668바이트의 배수 단위였다. 최종 검수는 이를 Wi-Fi(esp_hosted) 버퍼 풀이 몰림 최대치까지 자라고 돌려주지 않는 것으로 판독했고 누수가 아니라고 보았다 — 어디까지나 판독이며 확인된 것은 아니다. 처리: 3c 계획의 메모리 되찾기.
  - 스택 여유(밤 3분 통화 뒤): `stk_vcap` 25204 · `stk_vrx` 40644 · `stk_vtx` 7176 · `stk_audio` 5612(기준 1024 이상). 줄일 근거로 쓸 수 있다.
  - 부하(%) = 에코 제거 평균 ÷ 160 + (압축 평균 + 해제 평균) ÷ 200: 조용한 방 33.7~33.9%(시험음 통화 37%), 말소리 통화(아침, 약 35초) 2번 약 43%·1번 약 40%(압축 평균: 2번 2.7 → 4.3ms, 1번 3.6ms — `out/p3d_after_usercall.log` 에서 계산). 합격선 50%.
  - 지연(`VPING` 왕복, 한쪽 길 ≈ 왕복 ÷ 2 + 25ms): **밤** 1번 가운데 253ms → 약 151ms, 2번 285ms → 약 167ms(합격선 80~320 충족, 지터 버퍼 목표 2~3). **아침(Wi-Fi 절전을 끈 뒤)** 540~573ms → 한쪽 약 0.3초(합격선 **미달**, 지터 버퍼 목표 8). 에코 제거 안의 지연·Opus 알고리즘 지연·공기는 재지 못했다.
  - 손실: 밤 3분 통화 받은 약 9,070 프레임 중 메운 것 12 · 버린 것 22~23(약 0.37%, 합격선 2% 이하 충족, `v_under=0`). 아침 30초 통화(`5935ddd`): 1번 메운 9 · 버린 0(약 0.6%), 2번 메운 14 · 버린 38(약 3.6% = (메운 14 + 버린 38) ÷ 2번이 받은 1,460 프레임 — 합격선 **초과**. 1번은 9 ÷ 1,456 = 약 0.6%. 값은 `out/p3d_call30.log` 의 마지막 `STAT` 줄: 1번 `v_rx=1456 v_lost=9 v_drop=0`, 2번 `v_rx=1460 v_lost=14 v_drop=38`).
  - 메아리 제거 실측(아침, 한 대의 스피커만 켜고 상대가 1kHz 시험음, `txgain=4 vol=100`): 1번 31~36dB, 2번 약 28dB 감소. 1번이 제 스피커 소리를 마이크로 2.7배 더 크게 주워 상대에게 돌아가는 메아리가 약 10dB 더 크다(−10.9dB 대 −20.5dB). 한쪽 고리 이득(스피커 소리가 마이크를 거쳐 다시 보내질 때 곱해지는 배수) = 4 × `mic` ÷ 1414(1414 = 펌웨어가 보내는 시험음 크기 2000 의 16비트 실효값 2000 ÷ √2 — `main/voice_core.h` 의 `TEST_AMP`; 4 = 보내는 이득 `txgain` 기본값)를 계산한 값: 1번 약 9.3배(+19dB), 2번 약 3.4배(+10.7dB) — 최종 검수의 어림(3.3배)보다 1번이 약 3배 크다.
  - 소리 자체(신호음)의 비용은 Phase 3a 때 값: 내부 메모리 약 13.7KB, 소리 한 묶음(16ms) 평균 약 0.3ms.
  - **3c 첫 묶음(`771b9fe`)을 올린 뒤**(`out/p3q_*.log`, 스피커를 끈 채의 조용한 점검): 내부 메모리 `heap_int`: 올리기 직전(`5935ddd`, 통화를 여러 번 거친 뒤의 **대기값**) 1번 99,279·2번 91,631 → 3c 부팅 직후 140,171·139,519. **같은 조건(부팅 직후 대기) 비교**는 `5935ddd` 약 115.6K → 3c 약 140K(+약 24K). `heap_int_min`: 올리기 전 1번 90,395·2번 80,463 은 아침 내내 통화를 거친 뒤의 최소, 부팅 직후 136,923·136,271 은 통화 전의 최소라 **같은 조건이 아니다**(그래서 이 비교로 "얼마 늘었나"를 말하지 않는다). 통화를 거친 뒤 점검이 끝났을 때 `heap_int_min` 은 1번 116,527·2번 121,751 로 **기준 100,000 을 회복**했다(그 뒤 합성 음성 듣기 시도의 통화 뒤 2번 `heap_int_min` 은 106,859 — 기준 위지만 통화를 거치며 내려가는 경향은 3b 때와 같다. 출처 `out/p3q_listen_ab.log`). 캡처 태스크 스택 최저 여유 `stk_vcap` 8,820(스택은 32KB — 기준 6,000 위이나 여유가 얇다. 부팅 직후 값은 약 26,600). **옛 동작 회귀**(같은 바이너리에 `lvl 0`): 시험음 통화·30초 통화 모두 기준에 맞음, 30초 통화 메운 0.26~0.32%·버린 0.13%·부하 약 38%·`v_under=0`·`aud_late=0`, `VPING` 가운데 값 445·430ms(한쪽 길 어림 약 247·240ms) — **조건: 이 측정 동안 같은 맥북(교환기 겸용)에서 계획 담당의 변이 시험(맥 clang)이 돌았고(R4 와 R5 모두 겹쳤다 — 계획 담당의 보고) 지연·손실 값에 그 부하가 섞여 있으며 영향 크기는 재지 못했다.** 스피커를 켠 `SELFTEST`: 2번 `mic=1305 ref=592 aec=202`, 1번 `mic=3421 ref=569 aec=665`(`out/p3q_r6_unit1.log`·`p3q_r6_unit2.log`).
- 이 맥의 함정: ① `-isysroot` 가 없으면 링크가 안 된다(SDK 27 과 링커가 안 맞음). ② ESP-IDF 를 export 한 셸의 `python3` 에는 Pillow·pytest 가 없다 → 파이썬 도구·시험은 export 하지 않은 호출에서.

## 빌드·플래시

**두 기기에는 항상 같은 바이너리**를 올린다. 빌드는 한 번만 하고, 플래시는 빌드 폴더에서 `write_flash @flash_args` 로 한다. `idf.py flash` 는 그 사이 HEAD 가 움직였으면(devsync 가 5분마다 커밋한다) CMake 를 다시 돌려 버전 문자열·시각이 다른 새 바이너리를 만들기 때문에 쓰지 않는다.

```
cd ~/tab5-toy-phone/firmware/tab5_phone && source ~/esp/esp-idf-5.5.4/export.sh
idf.py set-target esp32p4 && idf.py build          # 처음에는 부품 내려받기로 5~10분, 이후는 idf.py build 만
```

기기가 화면이 꺼진 채 USB 에만 잡히고 `stat` 에 답이 없으면 다운로드 모드일 수 있다(시리얼에 `boot:0x204 (DOWNLOAD…)`·`waiting for download`). 그 상태에서도 올릴 수 있고, `--after hard_reset` 뒤에도 다시 다운로드 모드로 떨어지면 `python -m esptool --chip esp32p4 -p "$PORT" --after watchdog_reset read_mac` 으로 빠져나온다(2026-10-08 2번 기기에서 확인).

플래시 — **직전에 `read_mac` 으로 대상 기기를 확인**한다(MAC 줄이 안 나오면 `&&` 때문에 아무것도 쓰이지 않는다). 두 대가 함께 꽂혀 있으므로 포트는 `phone_ctl.py port <번호>` 로 고른다(`/dev/cu.usbmodem*` 를 직접 고르지 않는다).

```
cd ~/tab5-toy-phone/firmware/tab5_phone && source ~/esp/esp-idf-5.5.4/export.sh && PORT=$(python3 tools/phone_ctl.py port 2) && echo "2번 포트: $PORT" && python -m esptool --chip esp32p4 -p "$PORT" read_mac 2>&1 | tee out/flash_unit2_<커밋>.txt | grep -i "aa:bb:cc:dd:ee:02" && cd build && python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash @flash_args 2>&1 | tee -a ../out/flash_unit2_<커밋>.txt | grep -E "Hash of data verified|Hard resetting|rror"
```

1번은 `port 1` 과 `aa:bb:cc:dd:ee:01`, 로그 이름 `flash_unit1_<커밋>.txt`. 출력 파일 이름에 빌드 커밋(`<커밋>`)을 넣어, 이전 빌드의 플래시 원문(예: 증거로 남은 `dm_flash_unit*.txt`·`fx_flash_unit*.txt`)을 덮어쓰지 않게 한다. 기대: `MAC: …` 줄, `Hash of data verified.` 3줄, `Hard resetting via RTS pin...`.

올린 뒤 부팅과 버전을 확인한다(두 기기의 버전 세 줄이 같아야 같은 바이너리):

```
python3 tools/phone_ctl.py --unit 2 run 'until:stat:^STAT :60' 'reboot' 'wait:^PHONE BOOT unit=2 id=tab5-2:30' 'until:stat:^STAT .*fsm=IDLE.*relay=1:120' > out/boot_unit2_<커밋>.log 2>&1; echo "rc=$?"
grep -hE "App version|Compile time|ELF file SHA256" out/boot_unit1_<커밋>.log out/boot_unit2_<커밋>.log | sed 's/^.*app_init: //' | sort -u
```

- `snap_ctl.py`·`serial_ctl.py` 는 "첫 번째 포트"를 열어 엉뚱한 기기를 리셋시킬 수 있으니 쓰지 않는다.
- **새 장소에서는** `secrets.h` 의 고정 목록과 기기에 저장된 목록에 없는 Wi-Fi 에 붙지 못한다(1번이 그랬다). 화면의 Wi-Fi 배지(왼쪽 아래)를 눌러 망을 고르고 비밀번호를 직접 입력하면 저장되어 다음 부팅부터 자동으로 붙는다(부팅 뒤 약 17초에 대기 상태). 자동 도구로 비밀번호를 입력하지 않는다.
- 기기 로그의 `Version mismatch: Host [2.11.0] > Co-proc [0.0.0]` 경고와 `rpc_rsp … resp code [12294]` 1회는 두 기기 모두에서 똑같이 나오는 무해한 동작이다(스냅부스와 같은 부품 판). Wi-Fi 칩(C6) 펌웨어는 교체하지 않는다.

## 교환기 (개발 중에는 맥북에서)

교환기를 망에 여는 명령(`BIND=0.0.0.0 PORT=5094 python3 phone_relay.py`)은 Claude Code 의 자동 안전 검사가 막는다. 그래서 개발 중에는 스크립트 하나로 켜고 끈다 — 사용자가 이 스크립트 하나만 `~/.claude/settings.json` 의 허용 규칙(`Bash(bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh:*)`)에 직접 넣었다.

```
bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh start
bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh stop
bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh restart
bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh start-wrong-token     # 틀린 토큰으로 띄움(기기의 401 시험용)
bash ~/tab5-toy-phone/server/tools/phone_relay_dev.sh status
```

- 정확히 이 모양(`bash` + 절대 경로 + 낱말 하나)으로, **한 번의 호출에 이 명령 하나만** 쓴다(다른 명령과 `;`·`&&` 로 잇지 않는다). `0.0.0.0:5094` 로 열고 로그는 `/tmp/phone_relay_dev.log` — `start`·`restart` 때마다 **덮어쓰므로** 필요한 줄은 그 전에 `out/` 으로 복사한다.
- 사람이 터미널에서 직접 띄울 때의 원래 명령은 `cd ~/tab5-toy-phone/server && BIND=0.0.0.0 PORT=5094 python3 phone_relay.py` 다.
- 지금(2026-10-05 오후) 두 기기의 `RELAY_URL` 은 맥북의 LAN `ws://` 주소다(wss 로 바꾸기 전까지). 맥북이 다른 Wi-Fi 로 옮기면 주소가 바뀌므로 `secrets.h` 를 고치고 두 기기를 다시 올린다. wss 로 옮기는 순서·조건은 아래 "교환기 연결" 절.
- **교환기를 정상 종료(`stop`)하면 기기는 15~20초 뒤에야 안다**(기기 `ping_interval_sec=5` + `pingpong_timeout_sec=15`, 교환기 `phone_relay.py` 에 종료 훅이 없어 정상 종료 때 WebSocket 을 닫지 않는다 — **[2026-10-05 오후 갱신: 종료 훅이 들어가 정상 종료 때 닫기 코드 1001 을 보낸다(P4N-D5, 서버 보고서). 이 문장의 "15~20초"는 훅 이전 값이고 훅 이후 기기에서는 재지 않았다]**). 끄고 바로 `stat` 을 보면 낡은 `relay=1` 이 보인다. 프로세스가 죽어 연결이 끊기면(FIN) 부품이 바로 안다 — 코드로만 확인했고 기기에서 재지는 않았다. Phase 4(맥미니·터널) 전에 정할 항목이다.
- **시험이 끝나면 위 허용 규칙을 지운다**(LAN 에 열린 서비스를 켤 수 있는 규칙이다). Phase 3 을 이어서 할 거라면 그대로 두어도 된다. 규칙은 사용자가 직접 지운다.

## 시리얼 명령 (115200, 줄 단위)

기기가 받는 명령: `stat` · `t <x> <y>` · `shot` · `reboot`. 진단·조절용(사람이 켠다 — 아래 "목소리 통화"): `quiet on|off` · `selftest` · `vsrc aec|mic|ref|tone` · `vping` · `txgain [1..4]` · `vol [0..100]` · `fault audio`. 통화 품질 손잡이(Phase 3c, 아래 절): `lvl` · `tilt` · `res` · `mic` · `opus` · `jb` · `nodelay` · `vtone` · `vsrc probe` · `tune` · **`guard`**(2026-10-05 오후, 같은 절). 음량 버튼과 같은 길(아래 "화면 음량 버튼" 절): `volup` · `voldn` · `volstep [1..5]`.
기기가 스스로 내는 줄:

| 줄 | 뜻 |
|---|---|
| `PHONE BOOT unit=<번호> id=<tab5-N>` | 부팅. 기기 번호가 표에 없으면 `unit=0 id=unknown` |
| `FSM <이전> -> <이후> acts=0x… notice=<이름>` (+ 번호가 있을 때만 ` msg=<1~3>`) | 통화 상태가 바뀜. `acts` 는 상태 기계가 낸 할 일 비트 |
| `SCREEN <이름>` | 화면이 바뀜. `DECLINE` 은 메시지 고르기 화면 |
| `RELAY up=<0\|1> code=<0\|401> attempt=<n>` | 교환기 연결 변화. `up=1` 의 `attempt` 는 항상 0 |
| `STAT up=<초> unit= fsm= screen= wifi= relay= relay_code= peer= missed= touch= heap_int= heap_int_min= heap_psram= stk_app= stk_touch= stk_relay= stk_ws= audio= tone= aud_us= aud_err= stk_audio= amp= aud_late= voice= vsrc= v_cap= v_tx= v_txdrop= v_txfail= v_rx= v_play= v_lost= v_drop= v_under= v_jb= v_aec_us= v_enc_us= v_dec_us= v_mic= v_ref= v_aec= v_spk= v_rderr= stk_vcap= stk_vrx= stk_vtx= v_urun= v_ovf= v_catch= v_late= v_dup= v_bad= aud_gap= v_under_ms= txgain= vol= peer1= call_peer=` | `stat` 의 답. `touch` = 앱 태스크가 처리한 탭 수(실제 + 주입, 재부팅하면 0). 합격선: `heap_int_min` ≥ 100000, `stk_*` ≥ 1024. `stk_ws=0` 은 태스크가 아직 없는 부팅 직후 값. `audio=` 부터 다섯 칸은 소리(아래 "신호음"), `amp=` 부터는 목소리(아래 "목소리(Phase 3b)가 더한 것"). `v_urun=` 부터 열 칸은 원인을 가르는 칸(같은 절). 맨 끝의 두 칸은 전화기 3대(아래 "전화기 3대 — 화면"): `peer=` 는 slot 0, `peer1=` 은 slot 1 의 상대가 켜져 있나, `call_peer=` 는 지금·직전 통화의 상대 slot. **정규식에는 ` peer=1` 처럼 앞에 빈칸을 둔다**(빈칸이 없으면 `call_peer=1` 에도 맞는다) |
| `AUDIO ready rate=16000 block=256 vol=100 dma=3 mic=24 amp=<0\|1>` / `AUDIO FAIL <단계>` | 부팅 때 소리 장치(스피커 + 마이크)를 연 결과. `amp=0` 은 조용한 모드. `FAIL` 이면 3번까지 재시작해 다시 켜 보고(`PHONE audio-failed restart <1~3>`), 그래도 안 되면 `PHONE FAIL audio` 를 찍고 소리 없이 계속 돈다(`STAT … audio=0`). 단계: `lock`·`i2c`·`i2s`·`codec`·`amp`·`task`·`tones`·`voice`·`injected`(일부러 낸 실패) |
| `VOICE ready chunk=256 opus_in=640 opus_out_max=160 bitrate=24000 aec=VOIP_HIGH_PERF` | 부팅 때 목소리 경로(에코 제거·Opus·태스크 셋)가 준비됨. 실패하면 `AUDIO FAIL voice` |
| `amp forced off at boot` (로그 줄, `PHONE BOOT` 앞) / `netwifi: wifi power save off` (실패하면 `netwifi: wifi power save off FAILED: <이름> (0x..) — continuing`) | 부팅 로그: 앰프를 코드가 한 번 더 끈 것(PL3b-D21) / Wi-Fi 절전을 끈 것(N-D4). 실패해도 부팅은 계속 |
| `VOICE on` / `VOICE off` | 통화가 열려 마이크를 읽기 시작함 / 통화가 끝나 닫힘. 그 원인인 `FSM` 줄 **바로 앞**에 찍힌다(같은 태스크 — 순서가 늘 같다) |
| `TONE <BELL\|RINGBACK\|END> on` / `… off` | 신호음이 시작됨 / 그침. 소리를 만든 출력 태스크가 남기고 app 태스크가 다음 바퀴에 찍는다 — 원인이 된 `FSM` 줄과의 앞뒤는 정해져 있지 않다(대개 뒤). app 이 오래 멈춰 줄이 빠지면 `TONE ? lost=<n>` |
| `OK t x y` | 주입한 터치가 대기열(8칸)에 들어감. 너무 빨리 보내면 가득 차 버려진다 |
| `ERR t out-of-range` / `ERR unknown` | 좌표가 화면 밖인 `t` 명령 / 모르는 명령 |
| `VOLSTEP step=<1..5> max=5 vol=<0..100> saved=<0\|1>` | 음량 단계 상태(아래 "화면 음량 버튼" 절). 명령마다 한 줄, 저장을 마쳤을 때 한 번 더(`saved=1`), 부팅 때 한 번 |
| `RELAYCFG scheme=<ws\|wss\|none> tls=<0\|1>` | 부팅 때 한 번(`PHONE BOOT` 뒤): 교환기 주소의 종류. 주소 자체는 싣지 않는다(아래 "교환기 연결(wss)" 절) |
| `RELAYERR stage=<auth\|dns\|tcp\|tls\|cert\|http\|other> esp=0x<hex> tls=-0x<hex> flags=0x<hex> errno=<n> http=<n> attempt=<n>` | 교환기 접속 시도가 실패할 때마다, 바로 뒤에 기존 `RELAY up=0 code=… attempt=…` 줄 |
| `PEER slot=<0\|1> id=<기기ID> online=<0\|1>` | (v2) 교환기가 알린 상대 하나의 접속 상태를 받아들임. 접속 직후 다른 등록 기기마다 한 줄, 이후 바뀔 때마다. `slot` 은 이 기기가 보는 순서(아래 "전화기 3대 — 공통 헤더") |
| `RING slot=<0\|1> from=<기기ID>` | (v2) 전화 옴 신호를 받음 — 누가 걸었나. 바로 뒤에 `FSM IDLE -> INCOMING acts=0x20 …`(대기가 아니면 `FSM` 줄 없이 버려진다) |
| `CALL slot=<0\|1> to=<기기ID>` | (v2) 걸기 신호를 보냄 — 누구에게. 바로 뒤에 `FSM IDLE -> DIALING acts=0x1 …` |
| `SIGDROP ev=<peer\|ring> id=<기기ID\|->` | (v2) 상대를 알 수 없어 버린 신호: `id=-` 는 기기 ID 가 없는 옛(v1) 모양 — **이 줄이 접속할 때마다 나오면 교환기가 아직 v1 이다**(상대가 계속 꺼진 것으로 보이고 벨이 울리지 않는다). ID 가 찍혀 있으면 표(`main/roster.h`)에 없는 기기이거나 자기 자신 |
| `SHOT BEGIN … SHOT END` / `SHOT FAIL private` | 스크린샷. **입력이 든 비밀번호 자판 화면에서는 `SHOT FAIL private`** 로 거절한다(빈 자판·다른 화면은 찍힌다). 도구는 이 응답을 다시 받지 않고 실패로 끝낸다 |

수신거부 메시지·벨 끄기가 더한 것:
- 화면 이름 `DECLINE`(이때 `STAT … fsm=INCOMING screen=DECLINE`), 안내 이름 `REJECT_MSG`, `FSM` 줄 끝의 ` msg=<1~3>`(번호가 있을 때만).
- 벨 끄기: `FSM INCOMING -> INCOMING acts=0x80 notice=NONE`. 메시지 거절(받는 쪽): `FSM INCOMING -> ENDING acts=0x84 notice=NONE msg=2`. 거는 쪽: `FSM RINGBACK -> IDLE acts=0x40 notice=REJECT_MSG msg=2`.
- 그냥 거절만 엄격히 볼 때는 `notice=REJECTED$` 로 쓴다(`REJECT_MSG` 는 `REJECTED` 로 시작하지 않는다).

신호음(Phase 3a)이 더한 것:
- `STAT` 끝의 칸: `audio=<0|1>`(소리 장치가 열렸나) · `tone=<NONE|BELL|RINGBACK|END>`(지금 내는 신호음 — 박자의 쉼 동안에도 그 이름) · `aud_us=<평균>/<최대>`(소리 한 묶음 16000µs 를 만드는 데 든 시간 µs. 기록 줄을 찍은 묶음이 최댓값이 된다) · `aud_err=<코덱 쓰기 실패 횟수>` · `stk_audio=<출력 태스크 스택 여유>`. 앞의 칸은 그대로다.
- 한 통화에서 한 기기가 찍는 `TONE` 줄의 순서(맥 시험 `test_tones.cpp` "scenario …" 가 고정): 받는 쪽 `BELL on` → `BELL off`(받기·거절·벨 끄기·부재중 어느 것이든 한 번) → 통화가 끝나면 `END on` → `END off`. 거는 쪽 `RINGBACK on` → `RINGBACK off` → (거절당함·무응답·통화 뒤 끊김이면) `END on` → `END off`. 내가 걸다 취소하면 종료음이 없다.
- `TONE … off` 는 만든 소리가 0 이 된 때다. 스피커에서 실제로 그치는 것은 앞서 내보낸 만큼(DMA 3칸 가운데 2칸 = 약 0.03초. Phase 3a 때는 6칸이라 약 0.1초) 늦다 — 드라이버 소스로 본 값이며 기기에서 재지 않았다.
- 벨이 울리는지 볼 때는 `until:stat:^STAT .*fsm=INCOMING.*tone=BELL`, 벨 끄기 뒤에는 `…tone=NONE`. `TONE` 줄을 `wait` 로 기다릴 때는 그 앞뒤의 `FSM` 줄과 순서가 바뀔 수 있으니, 끝난 뒤 로그에서 기기별 `TONE` 줄만 뽑아 순서를 본다.

목소리(Phase 3b)가 더한 것:
- `STAT` 끝의 칸(앞의 칸은 그대로): `amp=<0|1>`(스피커 앰프 — 0 = 조용한 모드) · `aud_late=<출력이 48ms 넘게 늦어 스피커가 끊긴 횟수>` · `voice=<0|1>`(통화가 열려 마이크를 읽는 중) · `vsrc=<aec|mic|ref|tone>`(보내는 소리 — 평소 `aec`) · `v_cap=<마이크에서 읽은 16ms 묶음 수, 부팅 뒤 누적 — 통화 밖에서 늘면 안 된다>`.
- 이번(또는 직전) 통화의 셈 — 통화가 열릴 때 0 으로: `v_tx`(보낸 프레임, 20ms 하나) · `v_txdrop`(밀려서 버린 것) · `v_txfail`(보내기·압축 실패) · `v_rx`(받은 프레임) · `v_play`(해제해서 낸 것) · `v_lost`(빠져서 메운 것) · `v_drop`(늦거나 넘쳐 버린 것) · `v_under`(해제 태스크가 굶어 무음을 낸 횟수) · `v_jb=<지터 버퍼에 든 프레임>/<목표>`.
- 처리 시간 `v_aec_us`·`v_enc_us`·`v_dec_us` = `<평균>/<최대>` µs(에코 제거는 16ms 묶음마다, 압축·해제는 20ms 프레임마다). 부하(%) = 에코 제거 평균 ÷ 160 + (압축 평균 + 해제 평균) ÷ 200. 합격선 50%.
- 최근 1초의 세기(RMS): `v_mic`(마이크 칸) · `v_ref`(스피커 되먹임 칸 — 코덱이 실제로 내고 있는 소리. 앰프를 꺼도 잡힌다) · `v_aec`(에코 제거 뒤) · `v_spk`(낸 상대 목소리). `v_rderr` = 마이크 읽기 실패. `stk_vcap`·`stk_vrx`·`stk_vtx` = 목소리 태스크 셋의 스택 여유.
- 한 통화에서 한 기기가 찍는 줄의 순서: 거는 쪽 `VOICE on` → `FSM RINGBACK -> IN_CALL acts=0x180`, 받는 쪽 `FSM INCOMING -> ANSWERING acts=0x82` → `VOICE on` → `FSM ANSWERING -> IN_CALL acts=0x180`. 끊는 쪽 `VOICE off` → `FSM IN_CALL -> ENDING acts=0x248`, 상대 `VOICE off` → `FSM IN_CALL -> IDLE acts=0x240 notice=CALL_ENDED`(끊어졌으면 `notice=LOST`).
- 진단 명령의 답: `OK quiet on amp=0` / `OK quiet off amp=1`(실패 `ERR quiet …`) · `SELFTEST amp=<0|1> mic=<n> ref=<n> aec=<n> ok=<0|1>`(통화 중이거나 소리 장치가 없으면 `SELFTEST FAIL busy-or-no-audio`) · `OK vsrc <이름>` / `ERR vsrc`(통화 중이 아니거나 모르는 이름) · `OK vping` 뒤 약 1초 뒤(쉼 0.4초 + 왕복 — 소스 `phone_main.cpp` 의 주석이 "1초쯤 뒤")에 `VPING rt_ms=<n> jb=<깊이>/<목표>`(수정 묶음 2 에서 `jb=` 꼬리가 붙었다 — 줄 끝을 `$` 로 묶은 식은 맞지 않는다. 3초 안에 못 찾으면 `VPING timeout`) / `ERR vping` · `OK fault audio`.
- `STAT` 맨 끝의 열 칸(수정 묶음 2에서 더함 — 앞의 칸은 글자 하나 바뀌지 않았다): `v_urun`(지터 버퍼가 다 떨어진 **횟수** — `v_lost` 는 메운 프레임 수라 같은 때 늘지만 단위가 다르다) · `v_ovf`(지터 버퍼가 넘쳐 버린 프레임) · `v_catch`(지연이 쌓여 따라잡으려 버린 프레임) · `v_late`(순번이 늦어 버린 것) · `v_dup`(중복) · `v_bad`(규격 밖) · `aud_gap`(출력 쓰기 사이 간격의 최대 µs — 정상 16000, 48000 이상이면 스피커가 끊겼다) · `v_under_ms`(이번 통화의 첫 `v_under` 가 난 때, 통화 시작 뒤 ms, 없으면 0) · `txgain`(지금 보내는 이득 1~4배) · `vol`(지금 코덱 음량 0~100). **`v_drop` = `v_ovf + v_catch + v_late + v_dup + v_bad`**(한 줄 안에서 늘 같다 — 로그로 확인: 55 = 55, 48 = 48).
- 진단·조절 명령의 응답(소스 `main/phone_main.cpp` 와 글자까지 같다): `txgain` → `OK txgain <n>`(인자 없으면 지금 값, 범위 밖이면 `ERR txgain 1-4`, 기본 4, **재부팅하면 4**) · `vol` → `OK vol <n>`(인자 없으면 지금 값, 범위 밖이면 `ERR vol 0-100`, 소리 장치가 없으면 `ERR vol no-audio`, I2C 실패면 `ERR vol i2c`, 기본 100, **재부팅하면 100**) · `vsrc mic|ref` 는 스피커 앰프가 꺼져 있을 때만(켜져 있으면 `ERR vsrc speaker-on`, 하울링 방지 — PL3b-D22) · `quiet off` 를 받으면 통화 중이던 `mic`·`ref` 출처는 `aec` 로 돌아간다.
- 목소리 자료(PCM·압축 프레임)를 시리얼로 내보내는 명령은 없다 — 나오는 것은 세기 숫자와 셈뿐이다.

### 주입 좌표 (`tap:<x>:<y>` — 각 버튼 안의 한 점. 대부분 가운데이고 취소·끊기는 실제 시험에서 쓴 값(가운데가 아님). 정본은 `main/ui_layout.h`)

| 화면 | 버튼 | 좌표 |
|---|---|---|
| 대기 | 통화 키캡: 왼쪽(slot 0 — 번호가 작은 상대) · 오른쪽(slot 1) | 368,472 · 912,472 |
| 대기 | Wi-Fi 배지(왼쪽 아래) · 설정 화면 닫기 | 152,648 · 1081,648 |
| 거는 중 | 취소 | 640,604 |
| 벨 | 받기 · 거절 | 404,428 · 876,428 |
| 벨 | 벨 끄기 · 메시지로 거절 | 404,624 · 876,624 |
| 메시지 고르기 | 문구 1 · 2 · 3 | 240,168 · 640,168 · 1040,168 |
| 메시지 고르기 | 뒤로 | 404,624 |
| 통화 중 | 끊기 | 644,606 |
| 대기 · 통화 중 | 음량 크게 · 작게(오른쪽 세로 줄) | 1168,312 · 1168,608 |

화면이 바뀐 뒤의 탭 앞에는 `sleep:1` 을 둔다(화면이 바뀐 뒤 0.6초는 터치 가드가 받지 않는다). **벨 화면이 대기로 바뀐 직후의 통화 키캡 탭만 `sleep:2`**(그 경우는 1.5초 — 아래 "전화기 3대 — 화면").

### 검증 도구

```
python3 tools/phone_ctl.py port 2                # 2번 기기의 포트만 출력(포트를 열지 않는다)
python3 tools/phone_ctl.py --unit 2 run "until:stat:^STAT .*fsm=IDLE:60" "shot:out/idle.jpg"
python3 tools/phone_ctl.py pair "both:until:stat:^STAT .*fsm=IDLE.* peer=1:60" "1:tap:368:472" "2:wait:^FSM IDLE -> INCOMING:10" "2:tap:404:428"
```

단계 문법과 "지나간 줄을 놓치지 않는 방법"은 `tools/phone_ctl.py` 머리 주석에 있다. 판정이 헛돌지 않게 아래를 지킨다.

- **재연결 반복의 판정은 교환기 로그의 새 `online` 줄로 한다.** 기기는 교환기가 꺼진 것을 15~20초 뒤에야 알아서, 끄고 바로 `stat` 을 보면 낡은 `relay=1` 이 "다시 붙었다"로 읽힌다(30회 반복에서 35회 중 5회가 가짜 통과였다). `start` 때마다 로그 파일이 새로 만들어지므로 그 `online` 은 "새 교환기에 기기가 새로 접속했다"는 뜻이다.
- **통화 중 교환기를 끈 시험의 기대 패턴은 순서 무관**이다: `^FSM IN_CALL -> (OFFLINE|IDLE) acts=0x240 notice=LOST` + 마지막에 양쪽 `fsm=OFFLINE`. 교환기가 연결을 하나씩 닫아서, 먼저 끊긴 쪽만 `OFFLINE` 으로 바로 가고 다른 쪽은 `ended: lost` 를 받아 `IDLE`(LOST)을 거친다.
- 한 번만 찍히는 줄(`FSM IDLE -> OFFLINE` 등)을 놓치지 않으려면 **도구를 먼저 띄우고 교환기는 그 뒤에 끈다**(같은 셸에서 지연 종료). 상태는 지나간 줄이 아니라 `until:stat` 으로 본다.
- **자판 화면을 찍지 않는다.** `shot` 앞에는 `until:stat` 로 `screen=` 이 기대한 화면인지 확인한다(펌웨어가 입력이 든 비밀번호 화면은 거절하지만, 사람이 자판을 쓰는 중일 수 있는 시간에는 자동 촬영을 하지 않는다). 사람이 화면을 만지는 동안에는 `touch=` 대조가 어긋난다 — 시험 전에 만지지 말아 달라고 알린다.
- `heap_int` 는 읽을 때마다 ±1.6 KB 흔들린다(닫힌 소켓 정리 때문으로 보임) — 재연결 30회 전후 차이 −2 KB 대는 이 흔들림 안이라, 회차당 70 B 이하의 누수는 이 시험으로 구별하지 못한다.
- 시험 로그는 `out/`(git 제외)에만 둔다. 되돌릴 수 없는 작업(esptool 읽기·쓰기·verify)의 출력 원문은 `tee` 로 `out/` 에 남긴다.

상대 기기 없이 한 대만 시험할 때는 맥이 상대를 흉내 낸다(가짜 상대가 `tab5-1` 로 붙으므로 진짜 1번이 켜져 있으면 서로 밀어낸다 — 같은 ID 로 붙으면 옛 연결이 끊긴다):

```
python3 ~/tab5-toy-phone/server/tools/phone_fake_peer.py tab5-1 expect:peer sleep:2 send:call expect:ringing expect:connected@20 sleep:4 send:hangup expect:ended:hangup
```

단계: `expect:peer` · `expect:ring` · `expect:ringing` · `expect:connected` · `expect:ended:<사유>` · `send:call|accept|reject|hangup` · `sleep:<초>` · `voice:<프레임수>`(Phase 3 용), 맨 뒤에 `@<초>` 로 기다리는 한도. 수신거부 메시지용으로 `send:reject:<n>`(그 수를 `msg` 로 실어 보냄)과 `expect:ended:reject:<n>`(`0` = 번호가 없어야 함)이 있고, 번호를 쓰지 않으면 `msg` 를 보지 않는다.

## 신호음 (Phase 3a)

| 소리 | 언제 | 모양 | 크기(최고) |
|---|---|---|---|
| 벨 `BELL` | 전화가 오는 동안(받는 쪽) | 800·1000Hz 를 50ms 씩 번갈아 1초 → 2초 쉼, 반복 | 8000 |
| 연결음 `RINGBACK` | 상대 벨이 울리는 동안(거는 쪽) | 440+480Hz 를 함께 1초 → 2초 쉼, 반복 | 5300 |
| 종료음 `END` | 통화가 끝났을 때 한 번(거절당함·무응답·통화 뒤 끊김. 내가 걸다 취소한 때와 부재중에는 없다) | 660Hz 0.16초 → 쉼 0.08초 → 440Hz 0.28초 | 3700 |

- **바꾸는 법**: 모양은 `main/tones.h` 의 `BELL_SEGS`·`RINGBACK_SEGS`·`END_SEGS`(한 칸 = 주파수 둘·길이 ms), 크기는 같은 파일의 `BELL_AMP`·`RINGBACK_AMP`·`END_AMP`(16비트 표본의 최고값, 코덱 음량 100 기준). 고친 뒤 맥 시험 `test_tones.cpp` 의 박자·크기 시험을 함께 고치고 → 빌드 → 두 기기 다시 플래시. 코덱 음량은 `main/audio_out.cpp` 의 `VOLUME`(지금 100 고정 — 음량 조절 화면은 Phase 5).
- 소리가 없을 때 앰프는 켜진 채이고 I2S 는 0 을 낸다(켜고 끌 때의 퍽 소리를 피하려는 것 — PL3a-D4). 대기 중 쉬 소리가 거슬리면 그 결정의 탈출구를 본다.
- 소리 장치가 열리지 않으면(`AUDIO FAIL …`) 3번까지 재시작해 다시 켜 보고, 그래도 안 되면 소리 없이 그대로 동작한다(화면 표시는 아직 없다 — `STAT … audio=0` 으로만 보인다). 재부팅하면 다시 연다.
- 소리 장치는 스피커(I2S 송신)와 마이크(수신 4칸)를 함께 연다 — Phase 0 시험 펌웨어에서 실제로 돌았던 설정이다. 출력 경로는 "공급자에게 PCM 을 받아 낸다"(`audio_out.h` 의 `add_source`)이고 신호음이 첫 공급자, 목소리가 둘째다. 둘이 함께 소리를 내면 더한다(넘치면 자른다 — `pcm.h` 의 `mix_sources`).

## 목소리 통화 (Phase 3b)

받으면(`IN_CALL` 로 들어갈 때) 켜지고, 통화가 어느 길로 끝나든 그 자리에서 꺼진다. 받기 전에는 마이크를 읽지 않는다.

```
보내는 길  마이크 칩(칸0 = 마이크, 칸1 = 스피커 되먹임) → 에코 제거(16ms 묶음) → 4배 → 20ms 프레임 → Opus 24kbps → [순번 2][Opus] → 교환기
받는 길    교환기 → 지터 버퍼 → Opus 해제(빠진 것은 직전 소리를 줄여 가며 메움) → 링버퍼 → 출력 태스크(신호음과 섞음) → 스피커
```

| 태스크 | 하는 일 | 코어·우선순위·스택 |
|---|---|---|
| `v_cap` | 마이크 읽기 → 에코 제거 → Opus 압축 → 보내기 대기열 | 1 · 7 · 48KB(내부 메모리) |
| `v_rx` | 지터 버퍼에서 꺼내 Opus 해제 → 링버퍼 | 1 · 7 · 48KB(PSRAM) |
| `v_tx` | 대기열 → 교환기로 보내기(밀리면 버린다) | 0 · 5 · 8KB |
| `audio` | 16ms 마다 신호음·목소리를 받아 섞어 코덱에 쓴다(Phase 3a 그대로) | 1 · 6 · 6KB |

- **지연**(말한 뒤 상대 스피커에서 나기까지)의 내역과 각 버퍼의 깊이는 계획서 "지연 예산". 버퍼를 바꾸는 곳: 출력 DMA `main/audio_out.cpp` 의 `DMA_DESC`(3칸 = 32ms 앞서 씀), 지터 버퍼 `main/jitter_buffer.h` 의 `START_FRAMES`·`MIN_FRAMES`·`OVERFLOW_FRAMES`, 보내기 대기열 `main/voice_core.h` 의 `TX_QUEUE`.
- **크기를 바꾸는 법**(음량 조절 화면은 Phase 5): **재빌드 없이 실행 중에** 바꾸려면 시리얼 `txgain <1..4>`(내 목소리가 상대에게 보내지는 이득, 기본 4) · `vol <0..100>`(스피커 음량, 기본 100) — 둘 다 저장되지 않아 재부팅하면 기본값이다. 귀 판정에서 정한 값은 적어 두었다가 상수로 굳힌다: 보내는 이득 기본값 `main/voice_core.h` 의 `TX_GAIN_DEFAULT`(4)·`TX_GAIN_Q8`(256 = 1배), 상대 목소리 크기 같은 파일의 `PLAY_GAIN_Q8`, 코덱 음량 `main/audio_out.cpp` 의 `VOLUME`, 마이크 증폭 같은 파일의 `MIC_GAIN_DB`(지금 24dB — 30dB 는 스피커 소리가 마이크에서 포화해 에코 제거가 지우지 못한다). 고친 뒤 맥 시험 → 빌드 → 두 기기 다시 플래시. **`selftest`·`vping` 의 판정과 `voice_check.py` 기준은 `txgain=4 vol=100`(기본값)에서만 유효하다** — 기본값이 아니면 고장이 아닌데 고장처럼 보인다(`vol` 을 88 아래로 내리면 `selftest` 가 `ok=0`, 응답 기기가 `txgain 1` 이거나 `vol` 74 아래면 `VPING timeout` — 수정 재검수의 계산값이고 기기에서 확인하지는 못했다).
- **되돌림 시험** `vloop on|off`(통화 밖에서, 2026-10-09 — `voice.cpp` VL-D1): 통화의 소리 길 전부(마이크 → 에코 제거 → Opus 압축 → [교환기 대신 내 받는 길로] → 지터 버퍼 → 해제 → 스피커)를 **상대 없이** 돌린다. 통화 중의 부하·스택·메모리·밀린 소리(`stat`·`cpu`·`tune`)를 받아 줄 사람 없이 잴 때 쓴다. 보통 `vsrc tone` 과 함께(시험음을 보낸다 — 마이크가 고리에 들지 않는다); `vsrc aec`(기본)면 스피커 소리가 마이크로 되돌아 다시 나가는 고리가 생긴다(하울링 시험). 교환기를 거치지 않으므로 망 쪽 부하는 빠져 있다. 진짜 통화가 시작되면 되돌림은 닫히고 통화가 열린다. **끝나면 `vloop off`**(답 `OK vloop on|off`, 통화 중이면 `ERR vloop on`). 스택짱에서 잰 값은 `firmware/stackchan_phone/README.md`. Tab5 두 대에 올라가 있는 빌드(2026-10-08)에는 이 명령이 없다.
- **조용한 모드** `quiet on`: 스피커 앰프를 끈다(재부팅 뒤에도 유지, `quiet off` 로 푼다). 벨·연결음·목소리가 모두 조용하지만 코덱은 그대로 돌아 `v_ref`·`selftest` 로 소리 경로를 기록으로 볼 수 있다 — 사용자 없이 시험할 때 쓴다. **켜 둔 채 잊으면 벨이 울리지 않는다**(`STAT … amp=0`, 부팅 줄 `AUDIO ready … amp=0`). `quiet on|off` 는 저장된다 — 전원을 뽑았다 꽂아도 유지. 응답 `OK quiet on amp=0` / `OK quiet off amp=1`(저장이나 앰프 쓰기가 실패하면 `ERR quiet …`).
- **혼자 하는 점검** `selftest`(통화 밖에서): 1kHz 시험음을 0.64초 내며 마이크 칸·되먹임 칸·에코 제거 뒤의 세기를 잰다. `ok=1` = 되먹임 칸이 판정선 이상 = 코덱이 소리를 내고 있다(판정선은 기판마다 — `board_profile.h` `SELFTEST_REF_MIN`: Tab5 300 · 스택짱 70. 스택짱의 되먹임 칸은 앰프 뒤에서 와 스피커를 끄면 0 에 가깝다 → 스택짱은 스피커를 켜고 잰다). 앰프가 켜져 있으면 `mic` 가 조용한 모드 때보다 훨씬 크다 = 스피커에서 실제로 소리가 났고 마이크가 들었다. **1초 이상 띄워서 보낸다**(16ms 안에 연달아 보내면 둘째가 엉터리).
- **보내는 소리 고르기** `vsrc aec|mic|ref|tone`(통화 중에만, 통화가 끝나면 `aec` 로 돌아간다): `mic` = 에코 제거를 거치지 않은 마이크, `ref` = 스피커 되먹임 칸, `tone` = 1kHz 시험음. **`mic`·`ref` 는 스피커 앰프가 꺼져 있을 때만 받아들인다**(`ERR vsrc speaker-on`) — 스피커를 켠 채 양쪽을 `mic` 로 두면 하울링이 나기 때문이다. 스피커가 켜진 동안 쓸 수 있는 것은 `aec`·`tone` 이다.
- **지연 재기** `vping`(통화 중에만): 마이크 대신 "쉼 0.4초 → 삑 0.1초 → 쉼 0.5초"를 보내고, 상대가 돌려보낸 소리에서 삑을 찾을 때까지의 시간을 `VPING rt_ms=` 로 찍는다. 상대를 먼저 `vsrc ref`(전기적으로 돌려보냄 — 조용한 모드에서만 된다) 또는 `vsrc mic`(스피커 → 공기 → 마이크 — 스피커를 켜야 의미가 있는데 `mic` 은 스피커가 켜져 있으면 `ERR vsrc speaker-on` 으로 거절되므로 지금은 쓸 수 없는 방법이다)로 둔다. 줄 끝에 지터 버퍼의 깊이/목표가 붙는다(`jb=`). 한쪽 길(입 → 귀) ≈ `rt_ms` ÷ 2 + 25ms.
- **소리 장치 실패를 일부러 내 보기** `fault audio` → `reboot`: 다음 부팅부터 소리 장치를 열지 않고 실패로 친다 → 세 번 재시작 → 네 번째 부팅은 소리 없이 돈다(`PHONE FAIL audio`, `STAT … audio=0`) → 표시는 스스로 지워져 그다음 재부팅은 정상이다.
- **운영 주의 넷**(수정 재검수의 범위 밖 관찰 — 소스 판독이며 기기에서 일부러 만들어 보지는 않았다): ① `selftest` 는 1초 이상 띄워서. ② 스피커를 켠 채 재부팅하기 전에 `quiet on`(재부팅 때 앰프가 잠깐 켜졌다 꺼진다 — 소스로는 0.3~1초이고 실제로 "퍽" 소리가 났는지는 누구도 확인하지 못했다). ③ `quiet off` 는 통화 밖에서, 한 기기씩(조용한 모드에서 양쪽 `vsrc ref` 로 둔 채 통화 중에 `quiet off` 를 보내면 쌓인 0.1~0.3초가 최대 크기로 날 수 있다). ④ 같은 방에서 양쪽 스피커를 켜면 하울링 위험(한쪽 고리 이득 1번 +19dB · 2번 +10.7dB 로 계산했다 — 다른 방에서).
- **Wi-Fi 절전 끄기**(`main/net_wifi.cpp`, 결정 N-D4 — Wi-Fi 모듈 `net_wifi.cpp` 의 결정 번호, 이유·비용·탈출구는 계획서 3b 결정 표, 커밋 `5935ddd`): 문제 — 기본값(모뎀 절전)이면 공유기가 절전 중인 기기에게 줄 자료를 모아 두었다가 약 0.3초 주기로 한꺼번에 줘서, 통화 중 받는 음성이 0.3초씩 뭉쳐 왔다(2026-10-05 아침 20~30초 통화에서 받은 프레임의 10~18%가 지터 버퍼에서 넘쳐 버려지고 지터 버퍼 목표가 최대 10 까지 올랐다). 찾은 법 — 맥북에서 통화 중인 기기로 0.2초 간격 `ping` 을 보내니 응답 시간이 약 +107ms 씩 도는 톱니였고, 공유기까지는 평균 13~19ms 였다(이 `ping` 원본은 파일로 남기지 못했다 — 컨트롤러가 화면에서 본 값). 고친 것 — `esp_wifi_start()` 성공 직후 `esp_wifi_set_ps(WIFI_PS_NONE)` 한 줄(부팅 로그 `netwifi: wifi power save off`). 비용 — Wi-Fi 칩이 늘 깨어 있어 전력을 더 쓴다(USB 전원 탁상기라 수용, 배터리로 쓰면 통화 중에만 끈다). 탈출구 — 그 블록을 지우거나 `WIFI_PS_MIN_MODEM`. **효과(로그가 지지하는 것)**: 통화 중 버림 비율 10~18% → 0%(1번)·2.6%(2번), `v_urun` 13~26 → 5~6. **한계**: 크게 줄었으나 완전 해결은 아니다 — 수정 뒤에도 통화 중 `VPING` 285~573ms, 2번 `v_ovf` 약 4%(40/969). 원본이 남지 않은 값: 대기 중 `ping` 평균 133~159ms → 17.8ms(이전 컨트롤러가 화면에서 본 값, 같은 조건의 비교도 아니다). 미확인: Wi-Fi 칩(C6)이 혼자 리셋된 뒤에도 이 설정이 유지되는지.
- 두 기기를 같은 방에 두고 스피커를 켠 채 통화하면 서로의 스피커 소리가 상대 마이크로 들어가 메아리·하울링이 날 수 있다(에코 제거는 자기 스피커 소리만 지운다) — 시험은 다른 방에서 한다.

## 통화 품질 손잡이 (Phase 3c 첫 묶음)

**무엇을 하나**: 사용자가 "먹먹하게 작은 소리로, 아주 멀리있는 것처럼 들림."이라 해서, 내 목소리가 상대에게 **더 크고 고르게** 가도록 보내는 쪽에 "말소리 평탄화"(`lvl`)를 넣었다. 말이 시작되면 이득을 천천히 올려 말소리의 꼭대기가 알맞은 크기(−6dBFS)에 오게 하고(이득 1~12배, 예전의 고정 4배보다 최대 약 +9.5dB), 소리가 너무 크면 빨리 내린다. 상대가 말하는 동안에는 이득이 예전의 4배를 넘지 못한다(하울링을 막으려는 것 — 단 12배로 말하던 중 상대 소리가 시작되는 **첫 16ms 묶음**은 이득이 내려오는 중이라 4배를 넘는다. 과제 검수가 직접 재서 확인했고 리미터가 막아 하울링 위험은 아니라고 봤으며, 둘째 묶음으로 미뤘다). **`lvl 0` 을 주면 소리 경로가 `5935ddd` 와 표본 하나까지 같아진다(맥 시험으로 확인)**. **아무것도 저장되지 않는다** — 재부팅하면 부팅 값(`lvl 12`)으로 돌아온다. 부팅 값이 바뀐 것은 이것 하나(`lvl` 0 → 12)이고, `STAT` 줄은 한 글자도 바뀌지 않았다. 새 줄은 `TUNE` 하나다. (지연은 이 묶음이 건드리지 않았다.)

**명령**(응답은 소스 `main/phone_main.cpp` 의 형식 문자열과 글자까지 대조했다. 값 없이 이름만 보내면 지금 값을 `OK <이름> <값>` 으로 돌려준다):

| 명령 | 뜻 | 응답 | 언제 걸리나 |
|---|---|---|---|
| `lvl 0` / `lvl 6`~`lvl 16` | 말소리 평탄화 끔 / 이득의 상한(배). 부팅 값 12 | `OK lvl <n>` · `ERR lvl 0\|6-16` | 다음 묶음(16ms) |
| `tilt 0`~`tilt 2` | 보내는 소리의 고음 올리기(1: 4kHz +4dB, 2: +7dB). 부팅 값 0 | `OK tilt <n>` · `ERR tilt 0-2` | 다음 묶음 |
| `res 0\|6\|12\|18` | 상대 소리만 날 때 보내는 소리를 누른다(dB) — 내 메아리가 상대에게 덜 간다. 부팅 값 18(2026-10-08 P5A — 그 전에는 0) | `OK res <n>` · `ERR res 0\|6\|12\|18` | 다음 묶음 |
| `mic 12\|15\|18\|21\|24` | 마이크 증폭(dB), 24 가 상한·부팅 값 | `OK mic <n>` · `ERR mic 12-24 step 3` · `ERR mic no-audio` · `ERR mic i2c` | 그 자리(딸깍 소리가 날 수 있다) |
| `opus 12`~`opus 40` | Opus 비트율(kbps), 부팅 값 24. **아직 쓰지 않는다**(아래) | `OK opus <n>` · `ERR opus 12-40` | 통화 중이면 다음 묶음, 아니면 다음 통화. 압축기가 거절하면 로그 `opus bitrate N kbps refused - back to M` 과 함께 앞 값으로 |
| `jb 0` / `jb 3`~`jb 10` | 지터 버퍼: 3b 규칙 / 새 규칙 + 목표 상한(프레임 = 20ms) | `OK jb <n>` · `ERR jb 0\|3-10` | 그 자리(통화 중에도) |
| `nodelay 0\|1` | 교환기 소켓의 TCP_NODELAY | `OK nodelay <n>` · `ERR nodelay 0\|1` | 그 자리 + 켜 둔 값(1)일 때만 다시 붙을 때마다 다시 건다. 드물게 잠금 시간 초과로 `ERR` 가 나도 값은 바뀌어 있을 수 있다(과제 검수가 이월한 지적) |
| `vtone 300`~`vtone 7000` | 시험음(`vsrc tone`·`vsrc probe`)의 주파수(Hz). `selftest` 는 늘 1kHz | `OK vtone <n>` · `ERR vtone 300-7000` | 다음 묶음 |
| `vsrc probe` | 마이크 자리에 시험음(크기 500)을 넣어 에코 제거·평탄화를 **거치게** 한다(통화 중에만, 스피커가 켜져 있어도 된다 — 진짜 마이크 소리는 쓰이지 않는다) | `OK vsrc probe` · `ERR vsrc` | 통화가 끝나면 `aec` 로 |
| `guard 0`~`guard 16` | **메아리 지킴**(2026-10-05 오후, 아래 "메아리 지킴 `guard`"). 부팅 값 8(2026-10-08 P5A — 그 전에는 6). 0 = 끔(`771b9fe` 와 같음) | `OK guard <n>`(값 없이 보내면 지금 값) · `ERR guard 0-16` | 다음 묶음(16ms) |
| `tune` | 아래 `TUNE` 줄 | | |

`selftest`·`vping` 뒤에는 `txgain` 이 4 가 아니거나 `vol` 이 100 이 아닐 때(`meas=0`)에만 `TUNE` 줄이 따라 나온다("고장이 아니라 설정 탓"을 로그에 남긴다). 단 `vping` 은 **`OK vping`(또는 `ERR vping`) 응답 바로 뒤에** `TUNE` 줄이 나오고 `VPING …` 결과는 약 1초 뒤에 따로 나온다(소스 `phone_main.cpp`: `print_tune()` 이 응답 직후 불린다 — 3c 인계 쪽지의 "`vping` 뒤"라는 말은 이 점에서 모호하다). 부팅 값에서는 나오지 않는다.

**`TUNE` 줄**(명령 `tune` 의 응답. 형식은 `tools/test_phone_ctl.py` 의 `TUNE3C_FORMAT` 이 고정하고, 펌웨어 `print_tune()` 의 형식 문자열과 같다):

```
TUNE dflt=%d legacy=%d meas=%d txgain=%d vol=%d mic=%d kbps=%d lvl=%d tilt=%d res=%d jb=%d nodelay=%d vtone=%d ps=%d nd_socks=%d g_q8=%d supp=%u far=%u rxgap=%u mclip=%u
```

| 칸 | 뜻 |
|---|---|
| `dflt` | 손잡이가 전부 부팅 값인가(1 = 예) |
| `legacy` | 손잡이가 전부 `5935ddd` 의 값인가(`lvl 0` 등 — 1 = 예) |
| `meas` | `selftest`·`vping` 판정이 유효한가(`txgain=4` 이고 `vol=100` 일 때 1) |
| `txgain` `vol` `mic` `kbps` `lvl` `tilt` `res` `jb` `nodelay` `vtone` | 각 손잡이의 지금 값(`kbps` = Opus 비트율) |
| `ps` | Wi-Fi 절전 상태 — 0 이어야 한다(꺼짐), 1·2 = 켜짐, -1 = 읽기 실패 또는 아직 안 읽음. 10초마다 읽는다 |
| `nd_socks` | TCP_NODELAY 를 마지막으로 건 소켓 수(-1 = 건 적 없음) |
| `g_q8` | 평탄화가 마지막으로 건 이득(256 = 1배, 1024 = 4배, 3072 = 12배). 통화(또는 점검)가 **시작될 때 0 으로 지우고, 통화가 끝난 뒤에도 마지막 값이 남는다**(소스 `voice.cpp`; 그래서 0 은 부팅 직후이거나 `lvl 0` 일 때) |
| `supp` · `far` | 마지막 통화에서(다음 통화 시작 때 지운다) 남은 메아리를 누른 묶음 수 · 상대 소리가 나던 묶음 수(16ms 단위) |
| `rxgap` | 마지막 통화에서(다음 통화 시작 때 지운다) 받은 프레임 사이의 가장 긴 틈(ms) |
| `mclip` | 마지막 통화에서(다음 통화 시작 때 지운다) 마이크 칸이 꼭대기에 닿은 표본 수(스피커 소리가 마이크에서 넘친다는 신호 — 크면 1번에서 `mic 18` 을 시험) |

켠 직후 실제로 찍힌 줄(두 기기 같음, 부팅 직후 — `out/p3q_boot_unit1.log` 210행·`out/p3q_boot_unit2.log` 216행; 통화를 거친 뒤의 줄은 `out/p3q_final.log` 에 있고 두 기기가 서로 다르다 — 예: 1번 `g_q8=1024 far=255`, 2번 `g_q8=0 far=0`): `TUNE dflt=1 legacy=0 meas=1 txgain=4 vol=100 mic=24 kbps=24 lvl=12 tilt=0 res=0 jb=0 nodelay=0 vtone=1000 ps=0 nd_socks=-1 g_q8=0 supp=0 far=0 rxgap=0 mclip=0`.

**기기에서 확인된 것**(스피커를 끈 점검, `out/p3q_r5_level.log`): `lvl 0` 일 때 `g_q8=0`, `lvl 12` 일 때 이득이 상한 `g_q8=3072`(12배)에서 멈추고 받는 쪽 `v_spk` 가 846 → 3353(약 3.96배), 상대가 시험음을 보내는 구간에서는 `g_q8=1024`(4배)로 내려가 `far` 가 올랐다 — 평탄화가 기대대로 걸리고, 상대 소리 중에는 예전의 4배를 넘지 않았다(상대 소리가 시작되는 첫 16ms 묶음은 예외 — 위).

**합성 음성으로 사람 없이 잰 것(2026-10-05 10:25~10:27, `out/p3q_lvl_speech.log`, 두 기기를 같은 책상에 두고 `quiet on` — 소리 없이)**: 맥북이 아주 작은 볼륨으로 읽은 합성 말소리를 2번이 듣고 보내고(2번 `v_mic` 61~74), 1번이 받는다. 1번이 보내는 소리는 일부러 낮춰(`txgain 1`·`mic 12`) 2번이 "상대가 말한다"로 보지 않게 했다. **`lvl 0`**: 1번의 `v_spk` 286 · 246 · 223 · 253(2번의 `g_q8=0`). **`lvl 12`**: 2번의 `g_q8=3072`(상한 12배), 1번의 `v_spk` 344 → 626 → 635 → 828(천천히 올라간다; 뒤 셋의 평균 약 696 = `lvl 0` 평균 약 252 의 약 2.8배). **말이 그친 뒤**: `v_spk` 42(`lvl 0`) 대 45(`lvl 12`), `g_q8` 은 1024 로 돌아옴. `far=0`·`mclip=0`. **이것이 보여 주는 것**: 평탄화가 작은 말소리 같은 입력을 상한까지 끌어올리고, 말 사이의 잡음은 끌어올리지 않는다. **보여 주지 않는 것**: 숫자만 쟀고 귀로 판정하지 않았으며, 사람 말소리가 아니라 합성 음성이다.

**무엇을 아직 모르는가**(솔직하게):
1. **새 소리에 대한 사용자의 귀 판정은 아직 없다.** 한 번 시도한 듣기(`out/p3q_listen_ab.log`)는 **무효**다: 시험 차림의 잘못(컨트롤러) — 맥북이 읽어 준 합성 음성이 책상 기기의 마이크에 너무 컸고(`v_mic=12896`, `mclip` 303 → 1989 — 마이크가 넘쳐 평탄화가 이득을 4배 아래로 내려 가·나 비교가 성립하지 않았다), 스크립트 도중 통화가 끝났으며, 사용자가 맥북에서 소리가 나는 게 맞는지 헷갈려 1번을 껐다고 했다(귀 판정 없음). `lvl`·`tilt`·`res` 를 귀로 고른 값은 없고 부팅 값은 `lvl 12` / `tilt 0` / `res 0` 그대로다.
2. **평탄화는 사람 말소리로 판정된 적 없다**(문턱 40·150 은 아침 로그의 숫자로 정했다; 합성 음성의 숫자 측정만 있다 — 위 "합성 음성으로 사람 없이 잰 것", 귀 판정 아님). 방 잡음이 말소리로 잡히면 잡음이 최대 3배 커질 수 있다 — 그러면 `lvl 0` 으로 되돌린다.
3. **1번 기기가 메아리를 흘릴 수 있다**: 받는 목소리가 커진 만큼 1번의 스피커 → 마이크 결합(2번의 약 2.7배)으로 메아리가 상대에게 더 돌아갈 수 있다. 위 무효 시도에서 2번이 받은 소리가 컸다(`v_spk=3837`)고 처음에는 1번의 메아리 누출로 읽었으나 **정정한다: 원인 미확정 — 1번이 맥북 소리를 직접 들었을 가능성이 크다(컨트롤러 판독), 1번 메아리 누출은 미확인.** 근거(`out/p3q_after_listen_unit1.log`): 1번은 꺼진 적이 없고(`up=2597`) 그 통화 끝의 마이크 세기 `v_mic=9630` 이 `v_ref=943` 에 제 스피커 결합 2.3~2.7배를 곱한 약 2,200~2,500 보다 훨씬 커서 맥북 소리가 마이크로 직접 들어온 것으로 보이고, `mclip` 은 약 54초에 203(약 0.02%)이다. 다음 단계: 1번에서 `mic 18` 을 시험(3c 계획서 Task 8 Step 4).
4. **`opus` 손잡이는 기기에서 써 본 적이 없다**(판정 P3C-R4: 지금은 쓰지 않는다 — 24kbps 그대로). 40kbps 에서 캡처 스택이 충분한지도 재지 못했다.
5. **캡처 태스크의 스택 여유는 얇다**: 32KB 중 통화 중 최저 여유 8,820 바이트(기준 6,000 위).
6. 같은 바이너리를 올린 뒤 점검이 스피커를 끈 채 이루어졌으므로, 스피커를 켠 통화의 지연·손실은 이 빌드로 새로 재지 않았다(`lvl 0` 회귀 통화의 값은 위 "두 기기에서 잰 것" 참고).
7. 3c 계획서의 둘째 묶음(수신 무음 감시·통화 중 끊김 기기 시험 등)과 지연 개선은 아직 하지 않았다. 계획서: `docs/exec-plans/2026-10-05-tab5-phone-phase3c-quality.md`.

### 메아리 지킴 `guard` (2026-10-05 오후 — 약한 하울링 수정, 결정 P4H-D1~D6)

**무엇을 하나**: 말소리 평탄화(`lvl`)는 작은 말소리를 최대 12배까지 키운다. 그런데 "지금 스피커에서 상대 소리가 나고 있다"를 꽤 큰 소리(되먹임 칸 150 이상)일 때만 알아채서, 스피커에서 작게 나는 소리(상대의 작은 말, 돌아온 내 목소리의 꼬리)가 마이크로 다시 들어오면 그것을 "내 말"로 보고 같이 키웠다. 고친 방식: **지난 0.2초의 되먹임 칸 최대 × `guard`÷4 보다 큰 묶음만 "내 말"로 쳐서 키운다**(스피커 소리가 마이크로 되돌아온 것은 그 소리에 비례해 작아 이 문턱을 못 넘는다). 아무도 말하지 않을 때는 예전의 고정 4배 그대로이고, 내가 실제로 말하면 지금처럼 커진다.

- **값**: `guard 6`(부팅 값, 1.5배) · 4 = 1배 · 8 = 2배 · 12 = 3배 · 16 = 4배 · **`guard 0` = 끔 = `771b9fe` 와 같은 동작**. 범위 0~16, **저장되지 않는다**(재부팅하면 6). 통화 안팎 어느 때나 걸리고 다음 묶음(16ms)부터 적용된다.
- **`TUNE`·`STAT` 줄에는 없다**(P4H-D5 — 기존 줄 모양을 고정한 pytest 를 그대로 두려고). 지금 값은 `guard` 명령이 답한다. `TUNE` 의 `dflt`·`legacy` 는 이 값을 보지 않는다.
- **비용**: 상대 기기가 내 목소리를 크게 되돌리거나 상대 쪽이 시끄러우면 내 말이 덜 커진다(고정 4배 아래로는 안 내려간다). 고리 모형에서 듣는 쪽이 받는 말소리 크기(고정 이득 대비, 칸 평균)는 `guard` 0 → 1.61배, 4 → 1.54, **6 → 1.47**, 8 → 1.36, 12 → 1.16, 16 → 1.07.
- **무엇이 확인됐고 무엇이 아닌가**(정직하게): ① 확인(소스 + 맥 시험): 작은 남은 메아리를 말소리로 보고 키우는 길이 있었고, 이 규칙이 그것을 막는다(`tools/test_howl_loop.cpp`). ② **모형은 "끝없이 이어지는 하울링"을 고치기 전 코드에서도 재현하지 못했다**(조용한 306칸 중 0칸). 재현된 것은 "말이 끝난 뒤 1.5~2초 동안 남은 소리를 최대 12배로 키움"(27/306칸)이고 그것을 고쳤다(`guard 4` 이상에서 0칸). 사용자가 들은 "약한 하울링"이 이것인지는 **다른 방 통화로만 확인된다 — 아직 확인 전**. ③ 부팅 값 6 의 근거(메아리 감소 10~25dB·150~550 구간 직선 연결)는 **가정**이다(실측은 `selftest` 14.2·16.2dB, 고른 시험음 28~36dB). ④ 기기에서 `guard` 가 실제로 어떻게 들리는지는 이 문서를 쓴 때 확인되지 않았다.
- **통화해 보고 말할 때 바로 시도할 값**: 
  - "아직 하울링이 남는다": `both:guard 12` → `both:res 12`(상대 소리 때 남은 메아리를 12dB 누름) → `both:lvl 8` → `both:lvl 0`(여기서도 남으면 평탄화 탓이 아니다 → `mic 18`).
  - "소리가 다시 작다": `both:guard 4` → `both:guard 2` → `both:guard 0`(`771b9fe` 와 같음). `guard 0` 에서도 작으면 이번 수정 탓이 아니라 "돌아온 내 목소리가 키우기를 끄는" 원래 성질이다.
- **다른 방에서 하울링을 숫자로 볼 때**: 통화를 열고 양쪽 다 말 없이 30초, 10초마다 `stat`·`tune`. 조용함의 판정선: `v_spk` 가 처음 값 근처(앞 기록 42~45, 넉넉히 100 아래)에서 오르지 않음 · 말이 없을 때 `g_q8=1024` · `far` 가 10초에 10 넘게 늘지 않음. 하울링: `v_spk` 150 이상이 이어지거나 오름 · 말이 없는데 `g_q8 > 1024` · `far` 가 10초에 수백씩 늚.
- **귀 판정과 손잡이 사다리는 음량 5단계에서 한다.** 되먹임 칸은 코덱 음량 **뒤**에서 재므로(실측 `p4a_vol3.log`: ref 569@100 → 180@72 = −10.0dB) `guard` 는 비율이라 그대로 서지만 절대 문턱(`FAR_ON 150`·`TUNE far=`·`res`·`SELFTEST ok`·`vping`)은 음량 단계마다 5dB 어긋난다 — 1~2단계에서는 "상대 소리 중" 판정이 사실상 안 켜진다. **줄인 상태의 "하울링 없음"은 이 수정의 증거가 아니다.** 문턱을 단계에 맞추는 것은 다음 묶음이다.
- 탈출구: `guard 0`(실행 중) 또는 `main/voice_tune.h` 의 `GUARD_DEFAULT` 한 곳.

## 화면 음량 버튼 (2026-10-05 오후, 결정 P4V-D1~D7)

대기 화면과 통화 화면의 **오른쪽 세로 줄**에 크게(＋) / 막대 다섯 개 / 작게(−)가 있다. 누르면 그 자리에서 소리 크기가 바뀌고(통화 소리·벨·연결음 모두) 전원을 껐다 켜도 기억한다. 벨 화면·거는 중 화면에는 없다(거절 옆에서 잘못 누르지 않게).

- **5단계 = 코덱 값 52 · 62 · 72 · 82 · 100**(−20 · −15 · −10 · −5 · 0dB, 5dB 간격). **맨 위가 지금까지의 소리 크기**라 지금보다 커지지 않는다. 맨 아래도 무음이 아니다. 5dB 씩 나눈 까닭: 이 부품은 코덱 값 1% 가 약 0.5dB 라 숫자를 고르게(100·80·60…) 나누면 두 번만 눌러도 거의 안 들린다(부품 소스를 읽은 모형 — 실제 출력은 기기에서 잴 수 있다).
- **자리**: 크게 `1168,312` · 작게 `1168,608`(주입 좌표, 위 표). 정본은 `main/ui_layout.h` 의 `BTN_VOL_UP`·`VOL_BARS`·`BTN_VOL_DN`. 맨 위/맨 아래에서는 그쪽 기호가 흐려진다. 화면이 바뀐 직후 0.6초·직전 터치 뒤 0.4초는 기존 터치 가드가 막는다.
- **저장**: 마지막으로 바뀐 **3초 뒤, 대기 상태(IDLE·OFFLINE)이고 신호음이 없을 때 한 번**(NVS 이름공간 `phone`, 키 `volstep`). 통화 중에 바꾸고 통화가 끝나기 전에 전원이 꺼지면 그 값은 남지 않는다(플래시 쓰기가 소리를 끊을 수 있어서 통화 중에는 쓰지 않는다). 저장된 값은 소리 장치를 연 직후(신호음·목소리·Wi-Fi·교환기 시작 전)에 걸린다. 저장된 값이 없으면 맨 위(100).
- **시리얼 명령**(화면 버튼과 같은 길): `volup` · `voldn` · `volstep <1..5>` · `volstep`(지금 값만). 기존 `vol <0..100>`(저장 안 됨)은 그대로다. **`volup`·`voldn` 은 `phone_ctl.py` 의 `until:` 로 보내지 않는다**(2초마다 다시 보내 한 단계씩 더 간다) — 그냥 보내고 `wait:` 로 받는다.
- **줄**: `VOLSTEP step=<1..5> max=5 vol=<0..100> saved=<0|1>` — `vol` 은 코덱의 지금 값(진단용 `vol` 로 옮겨 놓았으면 단계의 값과 다르게 보인다), `saved` = 이 단계가 저장소에 들어 있다. 오류: `ERR volstep 1-5`(값이 틀림) · `ERR volstep busy-or-no-app`(대기열 가득·화면 없음) · `VOLSTEP FAIL <no-audio|i2c|range> step=<n>`(코덱에 못 씀 — 단계 그대로). 부팅 줄 `AUDIO ready … vol=100` 은 그대로 100 을 찍는다 — 실제 값은 `STAT vol=`·`VOLSTEP vol=`.
- **`selftest`·`vping` 을 재는 절차**(음량을 줄여 둔 기기에서는 판정이 속는다): ① `volstep` 으로 지금 단계를 읽어 적어 둔다 → ② `volstep 5` → ③ 재기(`quiet on` 인 채로) → ④ 적어 둔 단계로 `volstep <n>` 되돌림. **줄여 둔 기기의 `SELFTEST … ok=0` 와 바로 뒤의 `TUNE … meas=0` 은 고장이 아니다** — 설정 탓이라는 신호다(도구가 자동으로 100 으로 맞추지는 않는다). `vping` 은 *상대* 기기의 음량이 삑 크기를 정하는데 `TUNE` 은 *이쪽* 값만 봐서, 상대만 줄였으면 `TUNE` 없이 `VPING timeout` 이 날 수 있다. 위 "통화 품질 손잡이"의 "`vol` 을 88 아래로 내리면 `selftest` 가 `ok=0`" 과 같은 이야기다.
- **기기에서 확인되지 않은 것**(이 문서를 쓴 때): 맨 아래(−20dB)에서 벨이 들리는지(안 들리면 `vol_steps.h` 의 `VOL_OF_STEP` 을 `{60,68,76,84,100}` 로), 통화 중 바꿀 때의 딸깍(코덱 레지스터를 5dB 한 번에 옮긴다), 오른쪽 끝의 터치 감도, 저장 순간의 "틱". 화면은 곧 다시 디자인될 예정이라 그리기(`ui.cpp` 의 `draw_volume_cell`·`ui_layout.h` 좌표)와 판단(`vol_steps.h`·`app.cpp`)을 다른 파일로 나눠 두었다.
- **음량을 줄이면 하울링 쪽 문턱이 어긋난다**: 되먹임 칸이 코덱 음량 뒤에서 재져서 단계마다 5dB 낮게 보인다(위 `guard` 절). 하울링 귀 판정과 `guard`·`res` 시험은 5단계(맨 위)에서 한다.
- 탈출구: 단계 표는 `main/vol_steps.h` 한 줄. 저장 시점의 "대기 상태일 때만" 조건을 풀려면 `app.cpp` 의 `should_write(now, …)` 둘째 인자를 `true` 로 둔다(그러면 통화 중에도 쓴다 — 권하지 않는다).

## 교환기 연결 — `ws://`(LAN) 과 `wss://`(운영) (2026-10-05 오후, 결정 P4N-D1~D7)

`secrets.h` 의 `RELAY_URL` 의 **앞머리만 보고** 갈린다: `ws://` = 같은 Wi-Fi 의 맥북(개발), `wss://` = 인터넷을 건너 맥미니 교환기(운영 예정 주소는 `secrets.h.example` 에 있다). 주소가 둘 다 아니면 `PHONE FAIL relay scheme` 을 찍고 붙지 않는다. 서버 쪽은 `server/deploy/README.md`.

- **TLS 설정 요지**: 인증서는 묶음 **전체**(`esp_crt_bundle_attach`)로 검증하고 호스트 이름도 본다 — **유효 기간은 보지 않는다**(기기에 시계가 없어서; 만료된 인증서도 받는 것이 선언한 비용). mbedTLS 할당은 전부 PSRAM(`MBEDTLS_EXTERNAL_MEM_ALLOC`), AES·SHA 는 소프트웨어(`MBEDTLS_HARDWARE_AES=n`·`SHA=n`), **TLS 1.2 만**(1.3 은 꺼져 있다 — Cloudflare 최소 TLS 판을 1.3 으로 올리면 못 붙는다). 검증을 끄는 설정(`skip_cert_common_name_check` 등)이 코드로 들어오면 `tools/test_relay_tls_shape.py` 가 실패한다 — 일부러다.
- **`sdkconfig.defaults` 를 고친 뒤에는 `sdkconfig` 를 지우고 다시 빌드한다.** `sdkconfig` 는 git 밖이라 묵은 설정이 남으면 `relay_link.cpp` 의 `#error` 가 빌드를 멈춘다(일부러). 
  ```
  cd ~/tab5-toy-phone/firmware/tab5_phone && cp sdkconfig /tmp/sdkconfig.before-phase4 && rm sdkconfig && source ~/esp/esp-idf-5.5.4/export.sh > /dev/null 2>&1 && nice -n 10 idf.py build > out/build_phase4.log 2>&1; echo rc=$?; diff /tmp/sdkconfig.before-phase4 sdkconfig
  ```
  기대하는 차이는 `MBEDTLS_EXTERNAL_MEM_ALLOC`(꺼짐 → y)·`MBEDTLS_HARDWARE_AES`·`HARDWARE_SHA`(y → 꺼짐)와 그 종속 키뿐이다.
- **`ws://` 로 되돌리는 법 — `RELAY_URL` 만 바꾸는 것으로는 안 된다**(정정, 최종 검수). 토큰 교체 뒤에는 `secrets.h` 가 새 토큰이고 맥북 교환기는 메모리에 옛 토큰을 들고 있어, 한쪽만 맞추면 401 이다. `phone_token_rotate.py` 에는 "주소만 바꾸기"가 없다(늘 새 토큰). 그래서 "주소만 `ws://` 로 바꿔 다시 빌드" 는 **새 토큰 + 맥북 교환기를 새 토큰으로 재시작**이 함께일 때만 된다. 되돌리는 정본은 보관한 **빌드 A**(아래). 부팅 줄 `RELAYCFG scheme=ws tls=0` 으로 확인.
- **조합 표 (기기 판 × 교환기 × 토큰)**:

| # | 상황 | 기기 | 교환기 | 통화 |
|---|---|---|---|---|
| ① | 지금 | 두 대 빌드 A `0ded2c7`(옛 토큰·LAN `ws://`) | 맥북 교환기(메모리에 옛 토큰) | 된다 |
| ② | 사용자가 토큰 교체를 실행한 뒤(파일의 토큰만 새것) | ①과 같다 | 맥북 교환기를 **재시작하지 않는 한** ①과 같다 | 된다 |
| ③ | wss 빌드를 **한 대만** 올린 창 | 한 대 = 맥미니 교환기, 다른 한 대 = 맥북 교환기 | 서로 다른 교환기 | **두 기기 사이 통화 안 됨**(그래서 사용자의 귀 판정 뒤에 한다) |
| ④ | 두 대 다 wss | 새 토큰 | 맥미니 교환기 | 맥미니 교환기로 된다 |
| ⑤ | 되돌리기 | 보관한 빌드 A 를 다시 굽는다(옛 토큰·LAN) | 맥북 교환기가 **옛 토큰으로** 떠 있어야 한다 | 된다 |

  맥북 교환기가 이미 새 토큰으로 재시작된 뒤에 되돌리려면 `out/secrets_backup/<시각>/` 의 두 파일을 제자리로 돌려놓고 맥북 교환기를 재시작해야 한다 — 그 복원도 **사용자가 한다**(비밀 값 쓰기를 자동 안전 검사가 막는다). **두 대 다 wss 로 붙기 전에는 맥북 교환기 재시작 금지.**
- **wss 로 올리는 순서의 조건**: ① 사용자의 귀 판정(하울링·음량)이 끝난 뒤 ② USB 시리얼을 부팅부터 기록하고 `quiet on` 으로 굽는다 ③ `PHONE BOOT` 가 2회 되풀이되면 즉시 보관한 빌드 A 로 되돌린다(스택 넘침 패닉 의심) ④ 기기를 집에 보내는 것은 통화 중 끊김(M-10) 시험과 "연속 패닉 → 교환기 접속 건너뜀" 장치가 들어간 뒤(둘 다 아직 없다 — 다음 묶음).
- **새 줄**: `RELAYCFG scheme= tls=`(부팅 때 한 번)·`RELAYERR stage=…`(실패할 때마다) — 모양은 위 "기기가 스스로 내는 줄" 표. 토큰·주소·ID 는 어느 줄에도 없다. 화면의 "기기 등록 오류"는 **진짜 토큰 거절(401)일 때만** 나온다(`relay_code == 401`), "교환기에 연결할 수 없음"은 `relay_attempt >= 2` 일 때.
- **`RELAYERR` 로 원인 가르기**:

| `RELAYERR` | 뜻 | 볼 곳 |
|---|---|---|
| `stage=dns esp=0x8001` | 이름 풀이 실패 | Wi-Fi 의 DNS, 주소 오타, 터널의 DNS 레코드 |
| `stage=tcp esp=0x8004`·`0x8006` | 443 연결 실패·시간 초과 | 망이 443 을 막는가 |
| `stage=cert esp=0x801a tls=-0x2700 flags=0x8` | 인증서를 믿을 수 없음 | 묶음에 없는 발급 기관, 또는 중간에서 가로채는 망. `flags=0x4` = 이름 불일치 |
| `stage=tls esp=0x801a tls=-0x7780` | 서버가 악수를 거절 | Cloudflare 최소 TLS 판이 1.3 이면 이렇게 된다 |
| `stage=tls … tls=-0x7f00` 또는 `esp=0x8017` | 메모리 부족 | `heap_psram`, sdkconfig |
| `stage=tls … tls=-0x7200` | TLS 가 아닌 곳에 wss | 주소·포트 |
| `stage=http http=502`·`530` | 터널까지는 갔고 맥미니 교환기가 꺼짐(530 = 터널 자체가 없음) | 서버 |
| `stage=http http=403` | Cloudflare 보안 규칙이 막음 | 기기의 User-Agent 는 `ESP32 Websocket Client` — 이 문자열을 막는 규칙인지 본다(소스로 안 값, 기기에서 확인 안 됨) |
| `stage=http http=404` | 경로 오타(`/ws`) | 주소 |
| `stage=auth http=401` | 토큰 거절 | `secrets.h` ↔ `.env.phone` |
| `stage=other` | 접속 응답 대기 시간 초과 등 | 앞뒤의 `esp-tls`·`websocket_client` 줄 |

  (표의 값은 부품 소스를 읽어 정한 것이다 — 기기에서 일부러 만들어 확인한 적 없다.) **한계**(수정 1회차 뒤에도 남는 것): 401 뒤의 접속 시도가 응답을 기다리다 시간 초과로 끝나면 묵은 401 이 `auth` 로 읽힌다.
- **wss 점검 판정선**(한 대만 먼저 굽고, 통화까지 한 뒤 `stat`): 악수는 `relay_ws` 태스크지만 **암호화 쓰기는 보내는 태스크에서 돈다** — `v_tx`(8KB·PSRAM), `app`, `relay`. 그래서 `stk_ws ≥ 3,000`(평문 때 6,128 — Cloudflare 가 발급 기관을 바꾸면 악수 스택 깊이가 달라진다. 모자라면 `relay_diag.h` 의 `WS_TASK_STACK` 을 12288 로 올려 다시 빌드, 비용 내부 메모리 약 4KB), `stk_vtx`·`stk_app`·`stk_relay` 모두 `≥ 1,536`, `heap_int_min ≥ 100,000`. **`heap_int_min` 은 30초 통화로만 판정하지 않는다**: `771b9fe` 는 부팅 직후 136~137K 였는데 4.7시간 쓴 뒤 107~114K 였고, 빌드 A 의 30초 값(약 130K)과 조건이 다르다(아래 "여유 6.8KB"는 그 쓴 뒤의 값). **wss 에서 교환기 재시작 10회 + 통화 뒤의 값**으로 누수를 본다.
- **기기에서 확인되지 않은 것(위험 순)**: ① 웹소켓 태스크 스택(8192 유지)이 TLS 악수를 견디는가 — 넘치면 접속하는 순간 패닉이라 **한 대만 먼저 굽는다**(위 판정선). ② 내부 메모리에 남는 3~6KB(어림 — 여유가 6.8KB 라 빠듯하다). ③ 소프트웨어 AES·SHA 의 부하·악수 시간(계산이지 측정이 아니다). ④ Cloudflare 를 거친 접속의 세부(제어 PING/PONG 통과, 접속 응답 헤더 크기, 지연). 담당 보고서의 숫자는 전부 소스로 따진 어림이고 기기 실측은 0건이다.

## 통화음 P5A — 더 크게 · 하울링 줄이기 · 또렷하게 (2026-10-08, 계획 `docs/exec-plans/2026-10-08-tab5-phone-audio-p5a.md`)

사용자: "통화음이 작은 편임 · 공간이 아예 달라도(집–사무실) 하울링이 조금 있음 · 선명하게". 고전 스피커폰의 방식(말하지 않는 쪽과 대기 상태를 누른다)을 넣고, 그렇게 번 여유로 받는 목소리를 2배로 키웠다. **아직 통화로 들어 본 적이 없다 — 모형과 맥 시험까지다.**

| 명령 | 뜻 | 부팅 값 | 2026-10-05 의 소리로 |
|---|---|---|---|
| `rest 0\|6\|12\|18` | **쉼 누르기**: 상대 소리도 내 말도 없으면 보내는 소리를 이만큼(dB) 누른다. 말이 시작되면 16ms 안에 풀리고, 쉼으로는 천천히(12dB 에 약 0.2초) 내려간다 | 12 | `rest 0` |
| `dt 0\|6\|12` | **겹침 누르기**: 상대 소리가 나는 동안에는 내 말(로 보이는 것)도 이만큼 누른다 — 에코 제거가 자리 잡기 전의 큰 메아리가 돌지 못하게 | 12 | `dt 0` |
| `res 0\|6\|12\|18` | 남은 메아리 누르기(기존 손잡이) | 18(전에는 0) | `res 0` |
| `guard 0`~`16` | 메아리 지킴(기존 손잡이) | 8(전에는 6) | `guard 6` |
| `rxgain 100`~`300` | **받는 목소리의 재생 이득(%)**. 꼭대기는 16000 위를 부드럽게 눌러 26000(스피커 한계 약 −2dBFS)을 넘지 않는다. 벨·연결음에는 걸리지 않는다 | 200(+6dB) | `rxgain 100` |
| `hpf 0\|1` | **저음 걷어내기**: 보내는 소리의 200Hz 아래(100Hz −12dB) | 1 | `hpf 0` |

- 응답은 다른 손잡이와 같다: 값 없이 보내면 `OK <이름> <지금 값>`, 범위 밖이면 `ERR <이름> <범위>`. **저장되지 않는다**(재부팅하면 부팅 값). `TUNE`·`STAT` 줄에는 싣지 않았다(줄 모양 그대로) — `TUNE` 의 `res=` 만 18 로 보인다.
- **두 기기에 같이 건다**(`pair 'both:rest 0'` …). 누르기는 양쪽이 다 해야 고리가 닫히지 않는다.
- 고리 모형(`tools/test_howl_loop.cpp`, 648칸 전부): 2026-10-05 의 값은 329칸이 말이 끝난 5초 뒤에도 울리고, 그 값에 재생만 2배면 534칸. 지금 부팅 값 + 재생 2배는 6칸(크게 울리는 칸 0), 듣는 말소리 +6.0dB. **모형의 메아리 감소 범위는 가정이다 — 귀로 확인해야 한다.**
- 고음 올리기 `tilt` 는 0 그대로다: 켜면 모형이 다시 울린다(6 → 200칸). 울림이 없는 것을 확인한 뒤 `tilt 1` 을 켜 본다.
- **통화해 보고 바로 시도할 값**은 계획서 §8 의 표(아직 하울링 → `rxgain 150` → `rest 18` → `guard 12` …, 겹쳐 말할 때 작다 → `dt 6`, 말끝마다 배경이 끊긴다 → `rest 6`).
- 1번 기기는 스피커 → 마이크 결합이 커서, 재생 2배에서 `TUNE mclip`(마이크가 넘친 표본 수)이 크게 늘면 `mic 21`·`mic 18` 을 시험한다.
- 화면의 음량 5단계는 그대로다(코덱 값). 받는 목소리 키우기는 모든 단계에 똑같이 걸린다 — 아래 "화면 음량 버튼"의 "맨 위가 지금까지의 소리 크기라 지금보다 커지지 않는다"는 코덱 값 이야기이고, 통화 목소리는 이제 그보다 6dB 크다.
- 위 "메아리 지킴 `guard`" 절의 "부팅 값 6"·"통화해 보고 바로 시도할 값"은 2026-10-05 의 기록이다 — 지금의 부팅 값과 시도 순서는 이 절과 계획서 §8 이 정본이다.

## 전화기 3대 — 공통 헤더 (2026-10-08 저녁, 설계 `docs/exec-plans/2026-10-08-phone-3way-design.md` §6·§8)

전화기를 Tab5 두 대에서 세 대(+ 스택짱 2호기)로 늘리는 일의 **둘째 단계(공통 헤더)** 까지 들어 있다. 판단 로직과 교환기 글자가 "상대 여럿"을 알게 됐고, **화면은 아직 상대 하나만 그린다**(다음 단계 — 설계서 §9).

- **기기 표와 slot**(`main/roster.h`): 번호 1 = `tab5-1`, 2 = `tab5-2`, 3 = `stackchan-2`. 상태 기계와 화면은 상대를 "내 상대 가운데 몇 번째"(**slot**)로 센다 — 나를 뺀 나머지를 번호 순으로: 1번 기기는 slot 0 = 2번·slot 1 = 3번, 2번 기기는 slot 0 = 1번·slot 1 = 3번, 3번 기기는 slot 0 = 1번·slot 1 = 2번. Tab5 두 대는 서로의 slot 0 이다.
- **신호 규약 v2**(`main/relay_proto.h`, 설계서 §6): 접속 때 헤더 `X-Phone-Proto: 2` 를 싣는다. 받는 글자 `{"t":"peer","id":"<기기ID>","online":…}` · `{"t":"ring","from":"<기기ID>"}` · `{"t":"ended","why":"busy"}`, 보내는 글자 `{"t":"call","to":"<기기ID>"}`. 기기 ID 는 15자까지의 눈에 보이는 ASCII(따옴표·역슬래시 제외)만 읽고 쓴다.
- **상태 기계**(`main/call_fsm.h`): 상대별 접속 상태(`online(slot)`·`online_mask`), 지금·직전 통화의 상대(`call_peer`), 결과에 실리는 상대(`Out.peer`). 꺼진 slot 에게는 걸지 않는다. 통화 중·벨 중인 상대에게 걸면 안내 `BUSY`(`FSM … notice=BUSY`). 거는 중에 **다른** 상대의 벨이 먼저 오면 벨 화면으로 넘어간다(같은 상대면 지금처럼 동시 발신 → 바로 연결).
- **화면**: 이 단계에서는 상대 하나(slot 0)만 그렸다. 지금은 상대 둘을 그린다 — 아래 "전화기 3대 — 화면".
- **올리는 순서**: 이 펌웨어는 **v2 교환기**가 있어야 통화된다. 옛(v1) 교환기에 붙으면 `SIGDROP ev=peer id=-` 가 찍히고 상대가 계속 꺼진 것으로 보이며 벨도 울리지 않는다(기기 ID 없는 신호를 "아마 slot 0"으로 받지 않는다 — `roster.h` RS-D4). 교환기를 먼저 올린다.
- **`secrets.h`**: `PHONE_DEVICES` 의 번호와 ID 는 `roster.h` 의 표와 같아야 한다(다르게 적은 줄은 무시되어 그 기기가 `PHONE BOOT unit=0 id=unknown` 으로 뜬다 — `device_id.h` DI-D1). 스택짱 줄의 꼴은 `main/secrets.h.example`.
- **알려진 한계(모의 실험에서 본 것 — 기기에서 본 것이 아니다)**: `ringing`·`connected`·`ended` 에는 "어느 통화의 신호인가"가 실려 있지 않다(통화 설계서 §9.4 가 적어 둔 구멍 그대로). 신호가 초 단위로 늦게 오는 망에서 끊기·걸기가 엇갈리면 앞 통화의 `ended` 를 새 통화의 것으로 읽을 수 있고, 상대가 둘이 되면서 그 결과가 "화면의 상대와 실제로 연결된 상대가 다르다"로도 나타날 수 있다. 지연이 0.15초 안쪽인 조건의 모의 실험에서는 한 번도 나오지 않았다. 고치려면 교환기가 그 세 신호에 상대의 기기 ID(또는 통화 번호)를 실어야 한다 — 기기의 읽기는 모르는 열쇠를 버리므로 교환기가 먼저 실어도 된다.
- 맥 시험: `tools/test_roster.cpp`(표·slot 규칙 + "받은 글자 → slot → 상태 기계 → 보낼 글자"를 `app.cpp` 와 같은 순서로 이어 보기), `test_call_fsm.cpp`·`test_relay_proto.cpp`·`test_device_id.cpp` 의 뒤쪽 절. **기기에서 확인한 것은 없다**(이 단계는 기기에 올리지 않았다).

## 전화기 3대 — 화면 (2026-10-08 밤, 설계 `docs/exec-plans/2026-10-08-phone-3way-design.md` §9, 디자인 규칙 `docs/design/README.md` §5.4·§5.5)

대기 화면이 "상대 하나 + 가운데 통화 버튼"에서 **상대 둘이 나란히 선 화면**으로 바뀌었다. 같은 바이너리가 1·2·3번 어느 자리에서도 돈다(3번은 스택짱이라 실제로는 이 화면을 쓰지 않는다).

- **대기 화면**: 내 캐릭터는 서지 않고 왼쪽 위 이름표(작은 얼굴 + "N번 전화기")로만 남는다. 상대 둘은 각자 위에서부터 **이름 알약("N번 전화기" + 켜짐 불빛) · 구름 위의 캐릭터 · 둥근 통화 키캡(지름 176)**. 왼쪽 = slot 0(번호가 작은 상대), 오른쪽 = slot 1 — 1번 기기는 토끼·로봇, 2번은 곰·로봇, 3번은 곰·토끼.
  - 켜짐·꺼짐은 셋으로 함께 보인다: 캐릭터(손 흔들기 ↔ 눈 감고 Z) · 불빛(초록 ↔ 흐림) · 키캡(초록·광택 ↔ 흐린 윗면·흐린 그림과 글자). 꺼진 상대의 키캡도 눌린다 → 안내 "상대 전화기가 꺼져 있어 걸 수 없어요".
  - 음량 칸·Wi-Fi 키캡·부재중 알약(과 병아리)은 예전 자리 그대로다.
- **버튼 좌표**(`main/ui_layout.h`): `BTN_CALL = { 368, 472, 88 }`(왼쪽, `Tap::CALL`, slot 0) · `BTN_CALL_B = { 912, 472, 88 }`(오른쪽, `Tap::CALL_B`, slot 1 — enum 맨 끝). 나머지 버튼은 그대로다.
  - 왜 그 자리인가(`ui_layout.h` UL-D4): 화면이 손가락 밑에서 바뀌어도 다른 화면의 버튼이 눌리지 않게, 두 키캡 모두 취소·끊기(x 464~824, y 536~)·벨 끄기/뒤로·메시지로 거절(y 568~)·문구 셋(y ~232)·Wi-Fi·음량 칸과 겹치지 않는다(빌드의 `static_assert` + 시험이 원의 점 하나까지 본다). 그 조건을 채우는 곳이 y 232~567 의 띠, x 는 464 의 왼쪽과 824 의 오른쪽뿐이다. 그래서 캐릭터는 그 위 하늘(구름 위)에 0.7배로 선다.
  - **받기·거절과의 겹침, 그리고 벨 뒤의 긴 가드**(`ui_layout.h` UL-D7, `app.cpp` AP-D4): 통화 키캡은 받기·거절(y 328~528)과 겹친다 — 둘까지 피할 자리는 화면에 없다. 예전 통화 버튼은 그 둘의 가장자리와만 겹쳤는데, 지금은 왼쪽 키캡이 받기의 한가운데 쪽, 오른쪽 키캡이 거절의 한가운데 쪽이다(키캡 넓이의 87%). 배치로 못 막는 이 두 쌍은 시간 가드로 막는다.
    - **벨 → 대기**(받으려는 순간 건 쪽이 끊거나 벨 30초가 다 됨 → 그 터치가 통화 키캡을 눌러 건 쪽이 아닌 상대의 벨을 울릴 수 있다): 벨 화면이 대기로 바뀐 직후 **1.5초** 동안은 통화 키캡을 받지 않는다(`TAP_AFTER_BELL_GUARD_MS` — 그 밖의 버튼은 0.6초 그대로). **내가 거절을 누른 뒤**(전화 옴 → 끊는 중 → 대기)와 받기를 누른 순간 건 쪽이 끊은 뒤(전화 옴 → 연결 중 → 대기)에도 같다 — 버튼이 없는 그 사이 화면은 "앞 화면"으로 세지 않는다(`guard_prev`): "안 눌렸나" 하고 거절 자리를 또 눌러도 오른쪽 상대에게 걸리지 않는다. 부재중·거절 직후 바로 걸려면 한 번 더 누른다. **자동 시험에서 벨 화면이 끝난 직후의 통화 키캡 탭 앞에는 `sleep:2`.**
    - **대기 → 벨**(통화 키캡을 누르려는 순간 전화가 옴 → 0.6초가 지난 터치는 왼쪽이면 받기, 오른쪽이면 거절): 늘리지 **않았다**. 받기·거절은 벨이 울리자마자 누르는 버튼이고, 기기 시험의 받기·거절 주입(좌표가 그대로다)이 "벨 뒤 0.6초"에 맞춰져 있어 기기 없이 늘리면 그 흐름을 깨뜨릴 수 있다. 기기에서 받기 흐름의 여유를 잰 뒤 정한다(늘리는 자리는 `ui_layout.h` 의 `screen_guard_for` 한 곳).
    - 다른 화면의 버튼 가운데 통화 키캡과 겹치는 것은 이 둘뿐이라는 것, 그리고 화면이 바뀔 때 같은 자리가 다른 버튼이 되는 쌍 전체(14쌍 — 12쌍은 전화기 3대 전부터 있던 것)를 시험이 표로 고정한다. 새 버튼이 다른 화면의 버튼 위에 놓이면 그 시험이 깨진다. `app.cpp` 가 이 판단들을 실제로 부르는지는 `test_phone_ctl.py` 가 글자로 묶는다. 기기에서 본 적은 없다.
- **거는 중·연결 중·통화 중**: 구도는 그대로(왼쪽 나, 오른쪽 상대), 상대의 캐릭터만 **그 통화의 상대**(`call_peer`)다. **전화 옴·메시지 고르기**: 구름 위에 **건 쪽** 캐릭터.
- **안내**: `BUSY` = "통화 중이에요"(건 상대가 다른 통화·벨 중). 안내 알약의 자리가 **맨 윗줄 가운데**(y 14~78)로 올라갔다 — 예전 자리(y 92~164)에 이름 알약 줄이 온다. 모든 화면에서 같은 자리다(`ui_layout.h` UL-D6).
- **캐릭터**: 로봇(3번) 한 벌이 늘었고, 자세 `hi`(손 흔들기를 0.7배로 — 대기 화면의 상대)가 생겼다. 잠든 그림(`sleep`)도 그 크기로 다시 구웠다 — 곰·토끼의 **그림은 그대로**이고 굽는 배율만 1.0 → 0.7 이다(잠든 그림은 대기 화면에서만 쓴다. 통화 키캡의 광택 `spr_gloss` 도 같은 까닭으로 0.7배, 큰 수화기 `spr_ico_call_big` 은 쓰는 곳이 없어 뺐다). 그 밖의 곰·토끼·병아리 그림은 구운 바이트까지 같다. 번호 ↔ 캐릭터는 `toy::who_of_unit` 한 곳, 화면은 번호만 넘긴다(`uilayout::peer_unit`·`call_unit` — 정본은 `roster.h`).
- **`STAT`**: 옛 칸은 글자 하나 그대로, 줄 맨 끝에 ` peer1=<0|1> call_peer=<slot>`(스택짱 펌웨어의 `STAT` 과 같은 이름·같은 순서). `peer=` 는 여전히 slot 0 이다. **함정**: `call_peer=1` 안에 `peer=1` 이 들어 있어, 빈칸 없이 쓴 식 `.*peer=1` 은 slot 0 이 꺼져 있어도 맞는다 → ` peer=1` 로 쓴다(위 예시·`phone_ctl.py` 머리 주석을 고쳤고 `test_phone_ctl.py` 가 고정한다). 저장소 밖의 옛 스크립트는 고치지 못했다.
- **다시 그리기**: 상대별 켜짐·꺼짐은 대기 화면만 그리므로, 통화 중에 다른 상대가 켜지고 꺼져도 화면을 통째로 다시 그리지 않는다(`uilayout::peers_same` — 맥 시험이 고정한다).
- **글자**: 이름 알약의 "N번 전화기" 는 32px(`lbl_name1~3` — 예전 "상대 전화기 켜짐" 알약과 같은 크기), 내 이름표는 28px 그대로(`lbl_unit1~3`), 통화 키캡의 "통화" 는 48px(`lbl_call_s`), 안내 "통화 중이에요"(`lbl_n_busy`). 기존 글자 자산의 이름·순서·바이트는 그대로이고 뒤에 8개를 더했다. 쓰이지 않게 된 `lbl_peer_on`·`lbl_peer_off`·`lbl_call` 은 이름·순서를 지키려고 남겼다(쓰이지 않는 자산은 바이너리에 들어가지 않는다).
- **화면 미리보기**: 44장 + `_sheet.png`(전부) + **`_sheet_3way.png`**(대기 9조합 = 내 번호 1·2·3 × 둘 다 켜짐/하나만/둘 다 꺼짐, 전화 옴 3 = 건 쪽이 곰·토끼·로봇, "통화 중이에요" 안내). 도구가 장면마다 캐릭터가 제 칸(`PEER_BODY`)을 넘지 않는지, 이름·안내 알약이 띠보다 넓지 않은지도 검사한다. 옛 이름의 장면 그림은 실행할 때 지운다.
- **고친 옛 시험 줄**(뜻은 그대로): `test_ui_layout.cpp` — ① 도우미 `M()` 이 상대 둘의 켜짐을 함께 넣고 내 번호를 1 로 둔다(상대가 있어야 통화 키캡이 있다) ② "통화 버튼 지름 200 이상" → "통화 키캡 둘 지름 176 이상"(설계서 §9 의 아래 한계, 주요 버튼 160 이상 규칙 안) ③ 음량 칸이 겹치지 않는 버튼 목록에 `BTN_CALL_B` ④ "음량 칸은 안내 상자 아래" 줄의 이름과 조건에 새 안내 띠. 옛 검사 110개 가운데 이름·조건을 고친 것은 ②·④ 둘이고 나머지 108개는 글자 그대로 통과한다. `test_phone_ctl.py` — 통화 좌표가 화면 한가운데(예전 통화 버튼)에서 왼쪽 키캡 368,472 로, 예시의 `peer=1` → ` peer=1`, `STAT` 고정값에 꼬리 상수 `STAT3WAY_TAIL`, `vol=` 을 줄 끝으로 묶은 식 하나(이제 마지막 칸이 아니다).
- **확인하지 못한 것**: 기기에서의 실제 모습·터치 전부(Tab5 가 연결돼 있지 않았다). 스택짱·1번 Tab5 와의 실제 통화, 새 화면을 그리는 데 드는 시간, 대기 화면의 내부 메모리.

## 화면 테마 — 토이폰 (2026-10-08, 계획 `docs/exec-plans/2026-10-08-tab5-phone-toy-ui.md`)

화면은 "장난감 전화기 + 캐릭터" 테마다. 규칙의 정본은 `docs/design/README.md` §5(색·키캡·캐릭터·화면별 구성), 코드는 `main/toy.h/.cpp`.

- **(테마를 입힌 때에는) 버튼 자리가 바뀌지 않았다.** 키캡은 그림자까지 `main/ui_layout.h` 의 터치 영역 안에 그린다 → 좌표와 테마가 무관하다. 달라진 판정은 하나였다: 안내 알약을 벨 화면에서도 그리지 않는다(`notice_visible`, UL-D3). (그 뒤 전화기 3대에서 통화 버튼이 둘이 되며 좌표가 바뀌었다 — 위 "전화기 3대 — 화면".)
- **캐릭터**: 곰 = 1번 전화기, 토끼 = 2번 전화기, 로봇 = 3번 전화기(스택짱), 병아리 = 알림. 내 캐릭터는 왼쪽·상대는 오른쪽(대기 화면만 예외 — 상대 둘이 서고 나는 이름표뿐). 상대가 꺼지면 잠들고, 통화 중이면 종이컵 전화를 줄로 잇는다. 지금은 정지 그림이다(상태가 바뀔 때만 자세가 바뀐다).
- **화면 미리보기(기기 없이)**: `python3 tools/render_screens.py` (export 하지 않은 셸) → `out/screens/*.png` 44장과 한 장에 모은 `_sheet.png`·`_sheet_3way.png`. 기기와 같은 `gfx_draw.cpp`·`toy.cpp`·`ui.cpp`·`ui_wifi.cpp` 를 맥에서 컴파일해 그린다 — 구운 글자·도형·캐릭터는 기기와 픽셀이 같고, 내장 글꼴 글자(Wi-Fi 이름·자판 글쇠)만 맥의 비슷한 글꼴로 얹는다. 장면은 `tools/sim/sim_main.cpp` 에 더한다. **화면을 고치면 올리기 전에 이것부터 본다**(캐릭터가 버튼과 겹치지 않는지, 글자가 칸을 넘지 않는지).
- **그림을 바꾸는 법**: `tools/toy_art.py`(SVG 조각, 자세·배율·기준점) → `python3 tools/gen_toy_sprites.py --preview /tmp/sprites.png --sizes` → 화면 미리보기 → 빌드. `rsvg-convert`(`brew install librsvg`)가 필요하다 — 그림을 바꿀 때만. 구운 `main/ui_sprites.h`(33장, 자료 약 446KB — 전화기 3대 전에는 24장 약 294KB)는 git 에 있다.
- **글자를 바꾸는 법**: `tools/gen_ui_assets.py` 의 문구 표 → `python3 tools/gen_ui_assets.py --preview /tmp/labels.png`. 글꼴은 `tools/fonts/Jua-Regular.ttf`(SIL OFL, <https://github.com/google/fonts/tree/main/ofl/jua> 에서 받아 둔다 — git 제외). 주아체에는 `…`·`—`·`·` 와 드문 한글이 없다: 없는 글자를 쓰면 도구가 오류로 멈춘다 → `...`·`-` 로 쓴다.
- **빌드 크기**: `tab5_phone.bin` 0x2cab80(2,927,488 B, 앱 파티션 53% 여유), 우리 코드 경고 0 (2026-10-08 낮, 맥미니에서 빌드). 전화기 3대의 화면까지 넣은 판은 0x2f2db0(3,091,888 B, 51% 여유 — 같은 날 밤).
- **(2026-10-08 밤) 다음에 올릴 빌드 = 전화기 3대**: `out/fw_2026-10-08_3way/build/`(신호 규약 v2 + 상대 둘 화면 + 공용 소리 코드 정리, git 제외 — 맥미니에 있다. 맥북은 같은 소스로 한 번 빌드해 같은 이름으로 보관하면 된다). **아직 어느 기기에도 올리지 않았다.** 올리는 순서·확인할 것: `docs/exec-plans/2026-10-08-phone-3way-handoff-macbook.md`. 교환기는 이미 v2 다(2026-10-08 20:11) — 한 대만 먼저 올려도 Tab5 끼리 통화된다. 되돌릴 빌드는 아래의 P5A.
- **(2026-10-08 21:56 1번 · 22:07 2번, 맥북) 두 기기에 전화기 3대 빌드를 올렸다 — 같은 바이너리**(두 기기의 `App version`·`Compile time`·ELF 세 줄이 같다: `out/boot_unit1_0a0d9f8.log`·`boot_unit2_0a0d9f8.log`). 한 번에 한 대씩만 USB 에 있었다(1번 → 2번 순서). 올린 것은 **맥북에서 빌드한 `0a0d9f8`**: `out/fw_2026-10-08_3way_0a0d9f8/build/`(git 제외 — 맥북에만 있다. `tab5_phone.bin` 3,091,888 B, SHA256 앞 16자 `6fc1e7a533a43e20`, 꾸러미 안 `SHA256SUMS`, `App version 0a0d9f8`·`Compile time Oct  8 2026 21:54:55`·ELF `6550f0c5c…`, `main/` 경고 0).
  - 왜 맥미니 꾸러미(`1384b6e`, `e7bf5388…`)가 아닌가: 맥미니 SSH 가 Tailscale 재인증을 기다려 가져오지 못했다. `1384b6e` 뒤로 `firmware/tab5_phone/` 은 바뀐 것이 없어 소스는 같지만, 버전 문자열·빌드 시각이 달라 **바이너리는 다르다** → **2번에도 이 맥북 꾸러미(`…_3way_0a0d9f8`)를 올렸다**(같은 바이너리 원칙). 맥미니의 `out/fw_2026-10-08_3way/` 는 어느 기기에도 올라가지 않았다.
  - 1번에서 확인한 것(기록 `out/flash_unit1_0a0d9f8.txt`·`boot_unit1_0a0d9f8.log`·`tap_unit1_0a0d9f8_peeroff.log`·`selftest_unit1_0a0d9f8.log`, 화면 `out/shot_unit1_0a0d9f8_idle.jpg`·`…_peeroff.jpg`): 검사합 3줄·`PHONE BOOT unit=1 id=tab5-1` 1회 · `AUDIO ready … mic=24 amp=1` · `VOICE ready … aec=VOIP_HIGH_PERF` · `RELAYCFG scheme=wss tls=1` · `PEER slot=0 id=tab5-2 online=1`(ID 가 실려 옴 = 교환기가 v2 로 봄, `SIGDROP` 없음) · `STAT … fsm=IDLE relay=1 peer=1 … peer1=0 call_peer=0`(`heap_int_min` 133195 · `stk_ws` 5392 · `stk_app` 10276, 부팅 뒤 대기) · 대기 화면이 미리보기 `11_idle_u1_on_off` 와 같은 구성(토끼 켜짐 · 로봇 잠듦) · 로봇 키캡 `tap:912:472` → `FSM IDLE -> IDLE acts=0x0 notice=PEER_OFF` 와 맨 윗줄 안내 · 조용한 모드 `SELFTEST amp=0 mic=7 ref=569 aec=5 ok=1`(재는 동안만 `quiet on`·`volstep 5`, 끝에 `OK quiet off amp=1`·`VOLSTEP step=4 … vol=82 saved=1` 로 되돌림 — 음량 4칸은 올리기 전 값).
  - 2번에서 확인한 것(22:07, 기록 `out/flash_unit2_0a0d9f8.txt`·`boot_unit2_0a0d9f8.log`·`tap_unit2_0a0d9f8_peeroff.log`·`selftest_unit2_0a0d9f8.log`, 화면 `out/shot_unit2_0a0d9f8_idle.jpg`·`…_peeroff.jpg`): 검사합 3줄·`PHONE BOOT unit=2 id=tab5-2` 1회 · `AUDIO ready … mic=24 amp=1` · `VOICE ready … aec=VOIP_HIGH_PERF` · `RELAYCFG scheme=wss tls=1` · `PEER slot=0 id=tab5-1 online=1` · `SIGDROP` 없음 · `STAT … fsm=IDLE relay=1 peer=1 … peer1=0 call_peer=0`(`heap_int_min` 131015 · `stk_ws` 5408 · `stk_app` 10276, 부팅 뒤 대기) · 대기 화면(곰 켜짐 · 로봇 잠듦, 음량 5칸) · 로봇 키캡 `tap:912:472` → `FSM IDLE -> IDLE acts=0x0 notice=PEER_OFF` · 조용한 모드 `SELFTEST amp=0 mic=8 ref=592 aec=5 ok=1`(끝에 `OK quiet off amp=1`, 음량은 올리기 전과 같은 5칸 `vol=100 saved=1`). 올리기 전 2번의 판은 기록상 P5A(`de2af38`) — 되돌릴 빌드 `out/fw_2026-10-08_p5a/build/`.
  - `PEER slot=1 id=stackchan-2 online=0` 줄은 **나오지 않는다**: 교환기는 등록된 기기(`self.devices`)에 대해서만 상태를 알리는데 스택짱은 아직 토큰 등록 전이다. 화면과 `peer1=0` 은 "꺼짐"으로 맞다.
  - **통화**: 사용자 확인(2026-10-09 아침에 전함): "잘 되네. 고마워" · "통화 잘 나눴어요." — 새 판 두 대로 실제 통화를 했다(2026-10-08 22:10 뒤 ~ 10-09 아침 사이). 어느 방향으로 몇 번 걸었는지, 거절·메시지 거절·벨 끄기·음량 버튼을 썼는지, 통화음(P5A)의 크기·하울링·지연에 대한 구체 판정은 듣지 못했다. 기기 기록(`STAT` 누적값 `aud_late`·`v_txfail` 등)도 읽지 못했다 — 그때 두 대 모두 USB 에 없었다.
  - **확인하지 못한 것**(통화는 위 줄 — 사용자가 귀로 확인, 숫자는 없음): ① 통화 중 기록 숫자(`aud_late`·`v_txfail`)와 자동 시나리오(걸기·받기·거절) — 2번이 다른 곳에서 켜져 있어 걸면 그쪽 벨이 울린다(밤) ② 교환기 기록의 `{"ev":"online","dev":"tab5-1","proto":2}`(맥미니 SSH 불가) ③ 음량 버튼 터치 ④ 두 기기를 함께 꽂은 `pair` 시험(한 대씩만 꽂혀 있었다). 올리기 전 1번의 판은 기록상 `1a75277`(기기에서 버전 줄을 읽지는 않았다 — `STAT` 에 `peer1=` 칸이 없었고 맥북 `build/` 가 그 판이었다).
  - 되돌리기(1번): 올리기 전 맥북 `build/`(`1a75277`, `tab5_phone.bin` 2,679,792 B·`1b9740896e584b88`)를 `out/fw_prev_1a75277/build/` 에 보관했다 — "빌드·플래시"와 같은 명령에서 `cd build` 를 `cd out/fw_prev_1a75277/build` 로(실행해 본 적 없다).
  - 맥북에서 화면 미리보기가 링크 오류로 죽던 것을 고쳤다: `tools/render_screens.py` 가 `xcrun --sdk macosx --show-sdk-path` 를 쓴다(고치기 전·후 그림 46장 동일).
- **지금 두 기기에 올릴 빌드**: `out/fw_2026-10-08_p5a/build/`(화면 + 통화음 P5A, git 제외 — 양쪽 맥에 있다). 올리는 순서는 화면 계획서 §8 "맥북에서 1번 기기에 올리는 순서". 화면만 든 앞 빌드는 `out/fw_toy_4c47474-dirty/build/`(2번 기기에 올라가 있는 것). 다시 빌드하면 다른 바이너리가 되므로 두 기기 모두 새로 올려야 한다.
- **기기 확인(2026-10-08, 2번 기기 한 대)**: 올렸다. 연결 중 화면과 대기 화면이 미리보기와 같은 구성으로 뜨는 것을 `shot` 으로 봤고(Wi-Fi·교환기 연결, `peer=1`), 음량을 5→3→5→4→5 로 바꾼 뒤의 화면이 바꾸기 전과 픽셀까지 같았다(음량 칸만 다시 그리는 길). `heap_int_min` 135235 · `stk_app` 10340(대기, 기준 100000·1024).
- **아직 확인하지 못한 것**: ① 통화(걸기·받기·거절·메시지 거절·벨 끄기) — 1번 기기가 다른 곳에서 켜져 있어 걸면 그쪽 벨이 울리므로 하지 않았다. 그래서 거는 중·전화 옴·통화 중·메시지 고르기 화면은 기기에서 본 적이 없다 ② 통화 중 매초 다시 그리기가 소리에 주는 영향(`STAT` 의 `aud_gap`·`v_urun`) ③ 화면 한 장 그리기에 드는 시간 ④ Wi-Fi 설정 화면 ⑤ 밝은 바탕에서 대기 60초 뒤 밝기 낮춤이 충분한지 ⑥ **1번 기기에는 올리지 않았다**(전 화면 그대로 — 신호 규약이 같아 섞여도 통화는 된다).

## 문구를 바꾸는 법 (메시지로 거절)

`tools/gen_ui_assets.py` 의 `REJECT_MSGS`(순서 = 번호) → 다시 굽기(`python3 tools/gen_ui_assets.py --preview /tmp/tab5_phone_assets.png`, export 하지 않은 셸에서 — 미리보기 그림으로 글자가 깨지지 않았는지 눈으로 본다. 주아체에 없는 글자가 있으면 오류로 멈춘다) → 화면 미리보기(`tools/render_screens.py` — 문구가 키캡 안에 들어가는지) → 빌드 → 두 기기 다시 플래시. 문구 **수**를 바꾸려면 `call_fsm.h` 의 `MSG_COUNT`, `ui_layout.h` 의 `Tap::MSG*`·`BTN_MSG`, `ui.cpp` 의 배열 둘을 함께 고친다(어긋나면 `static_assert` 가 빌드를 막는다). 현재 문구: 1 "회의 중이에요", 2 "지금은 받을 수 없어요", 3 "잠시 후 다시 걸게요".

## 복구

- **복구 명령은 이번(전화기) 펌웨어에서 아직 한 번도 실행해 본 적이 없다.** 아래 명령의 형태는 `build/` 에서 올린 `out/dm_flash_unit*.txt`·`out/fx_flash_unit*.txt` 와 같다(`python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash …`). 복구 전에는 항상 export 하고 `read_mac` 으로 대상 기기를 확인한다(MAC 줄이 안 나오면 `&&` 때문에 아무것도 쓰이지 않는다).
- **2번 → 스냅부스**: `cd ~/tab5-toy-phone/firmware/tab5_snapbooth && source ~/esp/esp-idf-5.5.4/export.sh && PORT=$(python3 ../tab5_phone/tools/phone_ctl.py port 2) && python -m esptool --chip esp32p4 -p "$PORT" read_mac | grep -i "aa:bb:cc:dd:ee:02" && idf.py -p "$PORT" flash`. (Phase 0 끝에 2번을 스냅부스로 복구한 적은 있다 — 통화 설계서 §8 — 그러나 그때 쓴 명령은 기록이 없고, 전화기 펌웨어에서 되돌린 적은 없다.) 안 되면 공장 백업 `backups/tab5-2_factory_aabbccddee02_2026-09-26.bin` 을 아래 1번과 같은 형태로 `write_flash 0x0` (저장된 Wi-Fi 목록은 사라진다).
- **1번 → 사진 액자**: `cd ~/tab5-toy-phone/firmware/tab5_phone && source ~/esp/esp-idf-5.5.4/export.sh && PORT=$(python3 tools/phone_ctl.py port 1) && python -m esptool --chip esp32p4 -p "$PORT" read_mac | grep -i "aa:bb:cc:dd:ee:01" && python -m esptool --chip esp32p4 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash 0x0 ../../backups/tab5-1_photoframe_aabbccddee01_2026-10-04.bin` (`-b 460800` 은 이 프로젝트에서 플래시에 쓴 값이다. 백업을 뜰 때 쓴 `-b 921600` 은 읽기에만 썼고 쓰기에는 써 본 적이 없다. 16MB 전체를 쓰므로 시간이 오래 걸린다). 맥북 `backups/` 에 3개가 있다 — `…_2026-10-04.bin`(원본), `…_b2.bin`, `…_b3.bin`. 서로 NVS 구역(0x9000~0xe000)만 다르다(사진 액자 펌웨어가 부팅마다 NVS 에 부팅 횟수를 쓰기 때문). b3 는 한 세션에서 읽고 바로 verify 한 것이다. 원본과 b3 는 맥미니에도 있다.
- **1번 → 공장 상태**: 공장 백업 `tab5-1_factory_aabbccddee01_2026-09-22.bin`(16,777,216 B, 검사합 OK)은 **맥미니** `~/tab5-toy-phone/backups/` 에만 있다(맥북에는 없다). 맥북으로 가져와 위 "1번 → 사진 액자"와 같은 명령의 파일 이름만 바꿔 쓴다. `backups/` 는 git 제외라 devsync 로 오지 않는다.
- **지금 기기에 올라간 빌드와 보관본**: 지금 두 기기에는 `5935ddd`(목소리 통화 + 최종 검수 수정 + Wi-Fi 절전 끄기)가 올라가 있다(두 기기 모두 `App version 5935ddd`·`Compile time Oct  5 2026 08:01:38`·ELF `8ebd8266e…` 로 같은 바이너리 — 2026-10-05 08:06~08:1x 에 올림. 버전은 `out/p3d_fw_version.txt`, 검사합 `out/p3d_fw_build.sha256`). 맥북에만 있는(git 제외) 보관본, 새것부터: `out/fw_prev_e40d097/build/` = Wi-Fi 절전 끄기 직전(`e40d097`), `out/fw_prev_d77ae98/build/` = 밤에 스피커를 끈 채 검증한 목소리 빌드(`d77ae98`), `out/fw_prev_5d9935f/build/` = 신호음만 있는 판(`5d9935f`, 되돌리는 명령은 계획서 `…-phase3b-voice.md` "롤백"), `out/fw_prev_066cf57/build/` = 소리 없는 Phase 2 최종 빌드(되돌리는 명령은 신호음 계획서 "롤백"), `out/fw_prev_113097c/build/` = Phase 2 최종 검수 수정 직전(`113097c`), `out/fw_prev/` = 새 기능 전(`a909f7d`). 보관본으로 되돌릴 때는 위 "빌드·플래시"와 같은 명령에서 `cd build` 를 `cd out/fw_prev_<커밋>/build`(`fw_prev` 는 `cd out/fw_prev`)로 바꾼다 — 이것도 실행해 본 적 없다. 소스에서 되돌릴 때: **판단 헤더(`618eebf`)·글자 자산(`5971d4c`)·화면·진행(`113097c`) 세 커밋은 함께** 되돌린다 — 중간 상태는 `-Werror=switch` 로 빌드되지 않는다. 교환기(`6ff6108`)와 가짜 상대 도구(`ff90e5b`)는 따로 되돌려도 된다. 목소리·소리 장치 커밋의 되돌리는 순서는 계획서 "롤백". **주의**: 한 번 `quiet off` 로 둔 기기는 저장되어 소리가 켜진 채 부팅한다 — 밤에 조용히 두려면 `quiet on`. **[2026-10-05 오전 갱신 — 위 `5935ddd` 설명은 그 앞 시점의 것]** 지금 두 기기에는 **`771b9fe`**(3c 첫 묶음)가 올라가 있고 스피커가 켜져 있다(두 기기 모두 `App version 771b9fe`·`Compile time Oct  5 2026 09:36:42`·ELF `1854d4ccf…`, 검사합 앞 12자 `74a683983fb6`, 버전은 `out/p3q_fw_version.txt`, 검사합 `out/p3q_fw_build.sha256`, 플래시 기록 `out/p3q_flash_unit1.txt`·`p3q_flash_unit2.txt`). 보관본은 새것부터 `out/fw_prev_5935ddd/build/`(3c 첫 묶음 직전 — 되돌리는 명령은 3c 계획서 "롤백", 형태는 위와 같고 `cd build` 를 `cd out/fw_prev_5935ddd/build` 로 바꾼다 — 실행해 본 적 없다) → 위에 적은 `fw_prev_e40d097`·`fw_prev_d77ae98`·`fw_prev_5d9935f`·`fw_prev_066cf57`… 순이다. 그 자리에서 소리만 옛것으로 돌리려면 시리얼 `lvl 0`(재부팅하면 12 로 돌아온다), 급하면 `quiet on`.
- **2026-10-05 오후 묶음의 보관본과 지금 올라간 것**: **첫째 되돌릴 판 = `out/fw_prev_0ded2c7/build/`**(지금 기기의 빌드 A `0ded2c7` — 옛 토큰·LAN `ws://`, 검사합 `out/p4a_fw_build.sha256` OK 3줄, wss 로 올린 뒤 문제가 생기면 이것으로). 그 앞 판: `out/fw_prev_771b9fe/build/` = 오늘 오후 묶음(하울링 `guard`·음량 버튼·wss) 직전 판(`771b9fe`) — 되돌리는 명령은 위와 같은 형태에서 `cd build` 를 `cd out/fw_prev_771b9fe/build` 로 바꾼다(두 보관본 모두 되돌리는 명령은 실행해 본 적 없다). 이 묶음 사이에 들어간 것: **수신 무음 감시**(상대 소리가 5초 없으면 끊음 — 커밋 `d03959c`)와 그 앞 묶음의 **미룬 지적**(`ad7be1d`) — 이 둘은 기준 `ad7be1d` 에 이미 들어 있다. **지금 기기에 어느 빌드가 올라가 있는지는 컨트롤러가 적는다:**
  - 지금 두 기기 = **`1a75277`**(`Compile time Oct  5 2026 17:13:23`, 2026-10-05 17:1x~17:2x 에 2번 → 1번 순서로 올림, 맥미니 교환기에 `wss://` 로 붙음, 스피커 켜짐, 부팅 값 `lvl 12`·`guard 6`·`volstep 5`). **실행 중 값으로 `nodelay 1` 을 걸어 뒀다**(지연 −130~−210ms — 재부팅하면 부팅 값 0 으로 돌아간다. 부팅 값으로 굳히는 것은 다음 빌드). 그 앞 판 `0ded2c7`(LAN `ws://`·옛 토큰)은 `out/fw_prev_0ded2c7/build/`.
- **백업을 새로 뜰 때(사진 액자 상태 등)**: 읽기와 `verify_flash` 를 **한 esptool 세션에서** 한다 — 읽기 전 `read_mac` 은 `--after no_reset`, `read_flash` 는 `--before no_reset --after no_reset_stub`, 이어서 `verify_flash` 는 `--before no_reset` 으로, 중간에 칩이 재시작되지 않게. 호출을 나누면 사진 액자가 부팅하며 NVS 를 고쳐 쓰기 때문에 항상 어긋난다.
