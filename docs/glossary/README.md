# 용어사전 (m5tab5)

프로젝트 공통 용어. 새 용어·기능명·화면명·아키텍처 개념이 생기면 먼저 또는 동시에 갱신한다.

| 용어 | 뜻 |
|------|-----|
| **Tab5 스냅부스 (SnapBooth)** | Tab5 + USB 웹캠으로 만드는 휴대형 생성형 AI 미니 포토부스. 촬영→AI 배경·화풍 합성→화면 표시→QR 공유. getbolsnap(갯벌스냅) 파이프라인 재활용. 계획: [docs/exec-plans/2026-09-22-tab5-snapbooth.md](../exec-plans/2026-09-22-tab5-snapbooth.md) |
| **부스 백엔드 (snapbooth_server)** | 맥미니에서 도는 Tab5 스냅부스 전용 서버(포트 5085 예정). 기기 토큰 인증 + `/snap`(이미지+배경+스타일→Gemini 합성) + `/result` + `/share`. getbolsnap 본체와 분리(getbolsnap은 127.0.0.1 바인딩·행사 전용). |
| **UVC 경로** | 웹캠(USB Video Class)을 Tab5 USB-A 호스트에 직결해 프리뷰·프레임 캡처하는 방식. 소니 PTP(esp-photobooth)와 별개. MJPEG 프레임은 그 자체가 JPEG. |
| **D1 방식** | 맥미니가 화면을 PNG/JPEG로 렌더링해 보내고 Tab5는 표시만 하는 구조(README 핵심 전제). 반대는 LVGL 네이티브 UI. |
| **내장 카메라 폴백 (cam_internal)** | 웹캠(UVC)이 없을 때 Tab5 내장 MIPI-CSI 카메라(SC202CS, 1280×720)로 프리뷰·촬영하는 경로. 소스 중재 = UVC 우선, 3초 없으면 내장, 시리얼 `c` 강제. 정본 `firmware/tab5_snapbooth/main/cam_internal.cpp`. |
| **무복사 프리뷰 (zero-copy preview)** | 내장 카메라 V4L2 프레임 버퍼에 UI 를 직접 합성하고 그 버퍼에서 PPA 회전 출력 1회로 화면에 내는 방식. PPA 엔진이 1개라 복사+회전을 나누면 직렬화되어 8fps 로 떨어지는 문제의 해법(15~20fps). PPA 가 읽는 중인 프레임은 `s_pending_cam_idx` 로 붙들었다가 다음 출력 뒤 반환. |
| **IPA (Image Processing Algorithm)** | esp_video ISP 파이프라인의 자동 조정 알고리즘 묶음(AWB/AE(AGC)/CCM/감마/디노이즈). 센서별 JSON 으로 설정(`main/ipa/sc202cs_default.json` = SC2336 복제). 센서 드라이버가 µs 노출(EXPOSURE_US)을 지원해야 AE 가 정상 동작 → `tools/patch_sc202cs.py`. |
| **토이폰 테마** | Tab5 전화기 화면의 디자인 테마(2026-10-08~): 하늘·동산 바탕, 키캡 버튼, 캐릭터 셋. 공통 디자인 규칙의 예외이며 정본은 [docs/design/README.md §5](../design/README.md). 코드는 `firmware/tab5_phone/main/toy.h/.cpp`. |
| **키캡 (keycap)** | 토이폰 테마에서 "누르는 것"의 모양: 짙은 테두리 5px + 아래로 두꺼운 그림자 8px + 색 윗면. 그림자까지 터치 영역 안에 그린다. 그림자가 있으면 눌리고, 없으면(알약·판) 보여 주기만 한다. |
| **알약 · 판** | 토이폰 테마에서 "보여 주는 것"의 모양: 테두리만 있고 그림자가 없는 둥근 띠(알약 — 이름·안내·부재중)와 사각 판(음량 칸·비밀번호 칸). |
| **캐릭터 (곰·토끼·로봇·병아리)** | 토이폰 테마의 등장인물. 곰 = 1번 전화기, 토끼 = 2번 전화기, 로봇 = 3번 전화기(스택짱), 병아리 = 알림 담당. 내 캐릭터는 왼쪽·상대는 오른쪽에 서고(대기 화면만 예외 — 아래 "상대 줄"), 자세로 상태를 보여 준다(꺼지면 잠듦, 통화 중이면 종이컵 전화). 번호 ↔ 캐릭터의 정본은 `firmware/tab5_phone/main/toy.cpp` 의 `who_of_unit`, 그림 정본은 `firmware/tab5_phone/tools/toy_art.py`. |
| **상대 줄 (대기 화면)** | Tab5 대기 화면에서 상대 한 명을 보여 주는 세로줄: 위에서부터 **이름 알약**("N번 전화기" + 켜짐 불빛) · 구름 위의 캐릭터 · **통화 키캡**(둥근 키캡, 지름 176). 줄이 둘이고 왼쪽이 slot 0(번호가 작은 상대)·오른쪽이 slot 1. 내 캐릭터는 대기 화면에 서지 않고 왼쪽 위 **이름표**(작은 얼굴 + "N번 전화기")로만 남는다. 자리의 정본 `firmware/tab5_phone/main/ui_layout.h`(UL-D4: `BTN_CALL`·`BTN_CALL_B`·`PEER_NAME`·`PEER_BODY`), 규칙 [docs/design/README.md §5.4·§5.5](../design/README.md). |
| **벨 뒤 가드 (TAP_AFTER_BELL_GUARD_MS)** | 벨 화면(전화 옴)이 대기 화면으로 바뀐 직후 1.5초 동안 통화 키캡만 받지 않는 시간 가드. 통화 키캡이 받기·거절 자리와 겹치기 때문에 둔다 — 받으려는 순간 전화가 끊겨도, 거절을 두 번 눌러도 그 터치가 다른 상대에게 거는 것이 되지 않게(버튼이 없는 "끊는 중·연결 중" 화면을 거쳐 와도 같다). 그 밖의 터치는 화면이 바뀐 뒤 0.6초(화면 가드). `firmware/tab5_phone/main/ui_layout.h` UL-D7. |
| **안내 띠 (NOTICE_BAND)** | 잠깐 뜨는 안내 알약("꺼져 있어 걸 수 없어요"·"통화 중이에요" 등)의 자리: 모든 화면에서 맨 윗줄 가운데(y 14~78). 전화기 3대부터 이 자리다(그 전에는 한 줄 아래). `firmware/tab5_phone/main/ui_layout.h` UL-D6. |
| **색 그림 자산 (스프라이트, ui_sprite_t)** | 캐릭터·아이콘처럼 색이 여럿인 그림을 줄 단위 묶음(RLE: 길이·알파·RGB565)으로 구운 것. `tools/gen_toy_sprites.py` → `main/ui_sprites.h`. 한 색 알파 마스크인 글자 자산(`ui_asset_t`)과 구분한다. |
| **화면 미리보기 (render_screens)** | 기기와 같은 그리기 코드(`raster.h`·`gfx_draw.cpp`·`toy.cpp`·`ui.cpp`·`ui_wifi.cpp`)를 맥에서 컴파일해 모든 화면을 PNG 로 만드는 도구(`firmware/tab5_phone/tools/render_screens.py`). 내장 글꼴 글자만 맥의 비슷한 글꼴로 얹고 나머지는 기기와 픽셀이 같다. |
| **스택짱 전화기 소리 실험 (stackchan_phone_probe)** | 스택짱 2호기(M5Stack CoreS3 기반)를 세 번째 전화기로 쓸 수 있는지 가르는 관문 실험과 그 시험 펌웨어(`firmware/stackchan_phone_probe/`, 버리는 코드). 가설 S1 기준 신호 · S2 실시간 · S3 메아리 감소 · S4 공존. 계획·결과: [docs/exec-plans/2026-10-08-stackchan-phone-probe.md](../exec-plans/2026-10-08-stackchan-phone-probe.md) |
| **기준 신호 (하드웨어 · 소프트웨어)** | 에코 제거가 "스피커로 나간 소리"로 삼는 입력. **하드웨어 기준** = 스피커로 가는 소리를 회로가 마이크 칩의 한 칸으로 되돌려 준 것(Tab5: 칸 1). **소프트웨어 기준** = 그런 배선이 없을 때, 재생하려고 쓴 PCM 을 지연선에 넣어 늦춘 것(늦추는 양 = `swdelay`). 기준은 항상 마이크에 잡힌 메아리보다 먼저 와야 한다. |
| **등록 기기 표 (roster)** | 전화기로 등록된 기기의 번호(unit) ↔ 교환기 기기 ID 의 정본: 1 = `tab5-1`, 2 = `tab5-2`, 3 = `stackchan-2`. 코드는 `firmware/tab5_phone/main/roster.h`(순수 헤더 — Tab5 와 스택짱 펌웨어가 함께 쓴다). 토큰·MAC 은 여기 없고 `secrets.h` 의 `PHONE_DEVICES` 에 있으며, 그 번호·ID 는 이 표와 같아야 한다. |
| **상대 slot** | 한 전화기가 보는 "내 상대 가운데 몇 번째"(0·1). 나를 뺀 나머지를 번호 오름차순으로 센다 — 2번 기기에게 slot 0 = 1번, slot 1 = 3번. Tab5 두 대는 서로의 slot 0. 통화 상태 기계(`call_fsm.h`)·화면·기록 줄(`PEER slot=…`)이 이 번호를 쓰고, 교환기 글자에는 기기 ID 가 실린다(바꾸는 곳: `roster.h` 의 `route_slot`·`id_of_slot`). 기록의 slot 은 그 줄을 찍은 기기의 눈으로 읽는다. |
| **신호 규약 v2** | 전화기 3대용 교환기 ↔ 기기 글자 규약(2026-10-08~): 접속 헤더 `X-Phone-Proto: 2`, `peer` 에 `id`, `ring` 에 `from`, `call` 에 `to`, 종료 사유 `busy`. 옛 규약(v1 — 기기 ID 없음, 상대는 늘 하나)과 교환기에서 함께 받는다. 정본: [docs/exec-plans/2026-10-08-phone-3way-design.md §6](../exec-plans/2026-10-08-phone-3way-design.md), 기기 쪽 코드 `firmware/tab5_phone/main/relay_proto.h`. |
| **옛 짝** | 신호 규약 v1(옛 펌웨어)의 전화기끼리의 상대 — 지금은 Tab5 1번·2번. 교환기 설정 `PHONE_LEGACY_PAIR`(없으면 등록 순서의 앞 두 대). v1 기기는 옛 짝하고만 걸고 받고, 그 밖의 기기에게는 "꺼짐"으로 보인다(설계 TW-D8). |
| **스택짱 전화기 (stackchan_phone)** | 스택짱 2호기를 3번 전화기로 만드는 펌웨어(`firmware/stackchan_phone/`). 통화의 판단·소리 경로·교환기 연결은 Tab5 전화기와 같은 소스를 경로로 끌어 쓰고, 화면·Wi-Fi·부팅 선택만 따로다(`sc_*`). 캐릭터 = 로봇. |
| **이중 부팅 (스트림덱 ↔ 전화기)** | 스택짱 2호기의 한 플래시에 UIFlow2(스트림덱, `factory` 칸)와 전화기 펌웨어(`ota_0` 칸)를 함께 두고 `otadata` 칸이 다음에 부팅할 쪽을 기억하는 구성. 스트림덱의 "전화기" 타일과 전화기 화면의 "스트림덱" 키캡이 서로를 고르고 재시작한다(약 8초). 설계 TW-D14·D15. |
| **전화기 타일** | 스트림덱(3×2 타일)의 여섯째 자리에 끼우는 초록 수화기 타일 — 누르면 전화기 펌웨어로 넘어간다. 전화기 펌웨어가 설치돼 있을 때만 보인다(`~/Deskbot/firmware/stackdeck/main.py` 의 `PHONE_TILE`). |
| **USB 설정 되돌리기 (SB-D7)** | 스택짱 전화기 펌웨어가 부팅 맨 앞에서 칩의 USB(USB-Serial/JTAG) 단자 설정을 전원 켜질 때 값으로 되돌리는 일(`sc_boot.cpp` `usb_restore()`). 스트림덱(UIFlow2)은 같은 단자를 USB-OTG 로 쓰면서 USB-Serial/JTAG 를 "풀업 끔 고정"으로 두고, `machine.reset()` 은 CPU 만 다시 켜 그 값이 남는다 — 되돌리지 않으면 전화기는 도는데 맥의 USB 에 기기가 없다(2026-10-09). |
| **스트림덱 경유 앱 갱신** | 스트림덱이 도는 스택짱에 esptool 없이 전화기 앱을 올리는 길: `firmware/stackchan_phone/tools/sc_flash_deck.py` 가 MicroPython raw REPL 로 `ota_0` 칸에 4KB 씩 쓰고 기기에서 SHA256 을 다시 계산해 견준다. 전화기 펌웨어가 돌 때의 길은 `sc_flash.sh`(esptool). |
| **되돌림 시험 (vloop)** | 전화기 펌웨어의 진단 명령 `vloop on\|off`: 통화 밖에서 통화의 소리 길 전부(마이크 → 에코 제거 → Opus 압축 → 지터 버퍼 → 해제 → 스피커)를 상대 없이 돌린다 — 보낼 음성 프레임이 교환기 대신 자기 받는 길로 간다(`firmware/tab5_phone/main/voice.cpp` VL-D1, Tab5·스택짱 공용). 받아 줄 사람 없이 통화 중의 부하·스택·끊김을 잴 때 쓴다. 망 쪽 부하는 들어 있지 않다. |
| **기판 인터페이스 (audio_board)** | 소리 장치 코드에서 기판마다 다른 부분(핀·스피커 쪽 칩·앰프 켜고 끄기·재생 음량)만 모은 함수 여섯 개(`audio_board.h`). 구현은 Tab5 `audio_board_tab5.cpp`(ES8388), 스택짱 `audio_board_cores3.cpp`(AW88298). 나머지(`audio_out.cpp`)는 두 기기가 같은 파일. |
| **기판 프로필 (board_profile)** | 칩마다 달라야 하는 통화 값(마이크 증폭, 에코 제거 방식, 태스크의 코어·스택, 압축을 다른 코어로 떼는지)을 한 곳에 둔 헤더 `board_profile.h`. ESP32-S3 = 스택짱, 그 밖 = Tab5. |
| **플래시 일꾼** | 스택짱 전화기에서 플래시를 만지는 일(NVS·부팅 대상 바꾸기)을 대신 하는 내부 메모리 스택의 태스크(`sc_store.cpp` `scstore::run`). ESP32-S3 는 플래시를 읽고 쓰는 동안 PSRAM 을 못 읽어, 스택이 PSRAM 인 태스크가 직접 부르면 죽는다(설계 TW-D19). |
| **죽는 고리 탈출** | 스택짱 전화기 펌웨어가 패닉·감시 타이머로 세 번 연달아 다시 켜지면 스스로 스트림덱으로 돌아가는 안전장치(`sc_boot.cpp` SB-D5). 화면이 20초 얼면 일부러 다시 켜는 멈춤 감시(SB-D6)와 짝이다. |
