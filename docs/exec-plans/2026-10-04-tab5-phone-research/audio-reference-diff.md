# Tab5 음성 전화기 — 참고 구현과 우리 코드의 차이 목록 (Phase 2~3 계획용)

`~/tab5-toy-phone` 안은 읽기만 했고 `git status` 는 깨끗합니다. 빌드·플래시·시리얼 접근 없음, `secrets.h`·`.env*` 는 열지 않았습니다.

## 0. 경로 약칭

스크래치패드 = `<scratchpad>`

| 약칭 | 실제 경로 | 판 |
|---|---|---|
| X | `<스크래치패드>/ref/xiaozhi-esp32` | 커밋 0d576d3 (2026-10-02) |
| V | `<스크래치패드>/ref/M5Tab5-VideoLink` | 커밋 40f6b7e (2026-05-29) |
| B | `<스크래치패드>/ref/esp-bsp/bsp/m5stack_tab5` | v1.3.1 — VideoLink 가 소리 초기화를 전부 맡기는 공식 BSP라 추가로 받음 |
| S | `<스크래치패드>/ref/esp-sr` | master 7658101, 2.5.5 — `test_apps`·`docs` 만 |
| P | `~/tab5-toy-phone/firmware/tab5_phone_probe` | 우리 시험 펌웨어 |
| MC | `P/managed_components` | 우리 빌드에 실제로 잡힌 부품 소스 |
| IDF | `~/esp/esp-idf-5.5.4` | |
| D | `~/tab5-toy-phone/docs/exec-plans/2026-10-03-tab5-phone.md` | 설계서 |

자주 나오는 파일:
- X-codec = `X/main/boards/m5stack/tab5/tab5_audio_codec.cc`
- X-board = `X/main/boards/m5stack/tab5/m5stack_tab5.cc`
- X-svc = `X/main/audio/audio_service.cc` (`.h` 는 X-svc.h)
- X-afe = `X/main/audio/engines/afe_audio_engine.cc`
- X-app = `X/main/application.cc`
- X-ws = `X/main/protocols/websocket_protocol.cc`
- BA = `P/main/board_audio.cpp`, PM = `P/main/probe_main.cpp`
- CD-i2s = `MC/espressif__esp_codec_dev/platform/audio_codec_data_i2s.c`
- CD-dev = `MC/espressif__esp_codec_dev/esp_codec_dev.c`
- CD-8388 / CD-7210 = `MC/espressif__esp_codec_dev/device/es8388/es8388.c`, `.../es7210/es7210.c`

## 1. 먼저 알아야 할 결론

1. **두 참고 구현 모두 Tab5 에서 "스피커폰 동시 통화 + 에코 제거"를 돌리지 않습니다.** 본받을 검증된 통화 선례가 없고, 근거는 우리 Phase 0 실측뿐입니다.
   - xiaozhi: 기기 쪽 에코 제거 설정은 기본 꺼짐이고, 켤 수 있는 보드 목록에 Tab5 가 없습니다 (`X/main/Kconfig.projbuild:976-986`; Tab5 는 `:356` 한 곳에만 나옴).
   - 그래서 Tab5 는 `kAecOff` → 교대 대화로 동작하고, 말하는 동안 마이크 송신을 끕니다 (X-app:27-35, 1049-1052, 1188-1190).
   - 에코 제거는 "말하는 중 호출어 감지"용으로만 켜집니다 (X-afe:384-391).
   - VideoLink: 에코 제거·지터 버퍼·압축이 없고, 저자도 2대 연결을 검증하지 못했습니다 (`V/docs/architecture.md:86-88, 156-157, 165`).
   - D:45 의 "공개 선례"는 하드웨어 설정 선례로만 읽어야 합니다.
2. **하드웨어 초기화(I2S·코덱·증폭·앰프·기준 칸)는 이미 xiaozhi 와 같습니다.** 코드로 가져올 것은 없습니다.
3. **가져올 것은 운영 규칙 4가지입니다.**
   - 출력 장치만 따로 닫지 않기.
   - 통화 시작 때 해제기 reset + 대기열 비움 + 마이크 앞 120ms 버림.
   - 실시간 대기열은 오래된 것부터 버리고, 전송 실패 시 대기열 비우기.
   - PSRAM 설정 4종은 탈출구로 기록.
4. **FD 가 깨진 원인은 호출 방법 차이일 가능성이 낮습니다.** `FD_HIGH_PERF` 만 쓰는 실수(float) 엔진 쪽 문제로 좁혀집니다. xiaozhi 가 쓰는 것은 정수 엔진인 `FD_LOW_COST` 입니다 (§3).
5. **설계서 정정 3건**: 에코 제거 단위는 32ms 가 아니라 16ms, Opus 손실 은닉은 지원이 확인되지 않음, I2S DMA 가 재생 지연을 최대 90ms 까지 만들 수 있음 (§5).

## 2. 차이 목록

분류: 【가져올 것】 / 【이미 같음】 / 【다르지만 유지】

### A. 하드웨어 초기화

| 항목 | 참고 구현 | 우리 코드 | 분류 | 이유 |
|---|---|---|---|---|
| I2S 포트·DMA | X: I2S_NUM_0 master, dma_desc 6 × dma_frame 240, auto_clear_after_cb=true (X-codec:97-105) | 기본 매크로 = 6×240 (`IDF/components/esp_driver_i2s/include/driver/i2s_common.h:22-31`) + auto_clear_after_cb=true (BA:78-79) | 이미 같음 | 값 동일 |
| 표본율 | X: 24000 in/out (`X/main/boards/m5stack/tab5/config.h:8-9`), 입력을 16k 로 변환 (X-svc:5-10, 73-80, 202-219). V/B: 48k 표준 모노로 만든 뒤 16k 로 open (`B/src/bsp_audio.c:31-36, 55`; `V/src/media/audio.cpp:130-137`) | 16000 (`P/main/board_audio.h:11`) | 다르지만 유지 | 에코 제거·Opus 가 16k 전제(PH-D5)라 변환기가 필요 없음. X 의 24k 는 서버 음성 기본값 때문 (`X/main/protocols/protocol.h:79`) |
| 송신(TX) 형식 | X: 표준 I2S, MCLK×256, 16bit, 스테레오 (X-codec:108-139) | 표준 I2S 기본 매크로, 16bit, 스테레오 (BA:96-104). 실제 적용값: `STD: TX data 16, slot 32, ws 32, MONO, 16000, mclk 256` (`P/out/task4.log:186-187`) | 이미 같음 | 처음 넣은 세부값은 open 때 esp_codec_dev 가 다시 씀 (CD-i2s:236-283, 409-429). 양쪽 다 출력을 channel=1 로 엶 (X-codec:218-225; BA:150-152) |
| 수신(RX) 형식 | X: TDM, MCLK×256, bclk_div 8, 16bit, 칸 0~3 (X-codec:141-175). B/V: 수신도 표준 모노라 기준 칸을 못 받음 (`B/src/bsp_audio.c:64-66`) | TDM 기본 매크로(mclk 256, bclk_div 8: `IDF/.../i2s_tdm.h:122-128`), 칸 0~3 (BA:82-90). 실제: `TDM: RX data 16, slot 16, ws 32, total_slot 4, mask 0xf` (`P/out/task4.log:183-184`) | 이미 같음 | X 와 같은 구조. BSP 방식은 가져오지 않음 |
| `PROBE_TX_TDM` 분기 | 참고 구현 어디에도 4칸 TDM 송신 없음. 공식 BSP 도 송신은 표준 I2S (`B/src/bsp_audio.c:31-36, 60-63`) | 0 으로 사용. 분기 코드는 남아 있음 (BA:13-17, 91-94, 175-178) | 다르지만 유지(=0) | 본 펌웨어에서는 분기를 지워도 됨. 주석의 "공장 BSP 방식" 근거는 확인 못 함 |
| 코덱 생성 순서 | X: I2S → data_if → ES8388 → ES7210 (X-codec:19-75). open 은 필요할 때 (X-svc:196-200, 341-345) | 버스 확인 → I2S → ES8388 → ES7210 → 출력 open → 입력 open → 증폭 → 음량 → 앰프 (BA:115-163) | 이미 같음 | 우리는 부팅 때 둘 다 열어 두는 점만 다름 |
| ES8388 `master_mode` | X: true (X-codec:45). B: false (`B/src/bsp_audio.c:133`) | false (BA:131) | 다르지만 유지 | 클럭은 P4 가 냄. 드라이버는 이 값을 레지스터 0x08 에 그대로 씀 (CD-8388:243). M5Unified 도 같은 레지스터에 0x00 을 씀 (`MC/m5stack__m5unified/src/M5Unified.inl:807`) |
| ES7210 마이크 선택 | X: MIC1~4 전부 (X-codec:68) | 같음 (BA:139) | 이미 같음 | — |
| 마이크 증폭 | X: open 직후 채널 0 에만 30dB (X-codec:17, 204-205). V: 전체 채널, 기본 60% = 24dB, open 뒤 설정 (`V/src/media/audio.cpp:136-140, 158-164`; `V/src/app/settings.cpp:50`) | open 뒤 입력 0 에 30dB, 기준은 0dB (BA:156-159) | 이미 같음 | 증폭은 3dB 단계, 최대 37.5dB (CD-7210:328-345). 재설정 규칙은 §5-5 |
| 음량 기본·범위 | X: 기본 70, NVS 저장, 0 이하면 10 (`X/main/audio/audio_codec.h:64`; `audio_codec.cc:29-36`). V: 기본 70 (`V/src/app/settings.cpp:49`). 0~100 → −50~0dB (CD-dev:79-87) | 기본 50, 0~100 (BA:160, 186-189) | 다르지만 유지 | 우리 실측(통화 80~100)이 우선. X 의 Tab5 README 에 "음량 조절이 안 맞음" 미해결 메모가 있음 (`X/main/boards/m5stack/tab5/README.md:49-53`). 음량은 open 때 자동 재적용됨 (CD-dev:117-122, 212) |
| 하드웨어 이득 | X: pa 5.0V / dac 3.3V (X-codec:48-49). B 도 같음 (`B/src/bsp_audio.c:122-125`) | 같음 (BA:133) | 이미 같음 | — |
| 앰프 켜기 | X: 확장칩 0x43 초기화 때 P1(SPK_EN)을 켜고 이후 손대지 않음 (X-board:70), pa_pin=-1 (X-codec:46). B: P1 출력·High (`B/src/bsp_feature_en.c:39-45`) | M5Unified 가 부팅 때 P1=0 으로 둠 (`MC/m5stack__m5unified/src/utility/Power_Class.inl:153`) → init 끝에 bitOn(0x43, 0x05, 0b10) (BA:161, 196-202) | 이미 같음 | 같은 핀·레지스터. 우리 쪽은 켜고 끌 수 있음 |
| 이어폰 감지 | X: 0x43 P7 을 입력·인터럽트 허용으로만 설정 (X-board:64, 68-69). 입력 레지스터 0x0F 는 정의만 있고 읽는 코드 없음 (X-board:51). B/V: 검색 0건. M5Unified: "P7 = HP_DET" 주석과 입력 설정뿐 (`Power_Class.inl:143-154`) | 없음 (설계 PH-D7·§9.5 에만 있음) | 참고할 구현 없음 | 극성·디바운스·스피커 자동 차단 여부를 기기에서 직접 확인해야 함 (§5-4) |

### B. 소리 처리

| 항목 | 참고 구현 | 우리 코드 | 분류 | 이유 |
|---|---|---|---|---|
| 에코 제거 방식 | X: esp-sr AFE. `afe_config_init("MR", models, AFE_TYPE_FD, AFE_MODE_LOW_COST)`, `AEC_MODE_FD_LOW_COST`, NLP `VERYAGGR`, `MORE_PSRAM` (X-afe:143-152, 179). 만든 직후 끔 (X-afe:201-203). V: 없음 | `aec_create(16000, 4, 1, AEC_MODE_VOIP_HIGH_PERF)` 직접 호출, 항상 처리 (PM:50-51, 96-98, 135) | 다르지만 유지 | 결론 1. AFE 로 바꾸면 feed/fetch 태스크와 내부 링버퍼(지연)가 따라옴 (X-afe:206-229, 252-271, 406-442) |
| 필터 길이·채널·묶음 | X: 필터 길이는 AFE 기본값(값 확인 못 함). feed 묶음은 `get_feed_chunksize` 로 받음 (X-afe:266-270, 값 확인 못 함). 헤더 권장 4 (`MC/espressif__esp-sr/include/esp32p4_less_v3/esp_aec.h:61`) | 4 / 1채널 / 256(VOIP), 512(FD) (PM:50, 98; `P/out/task4.log:209`; `P/out/task5.log:354`) | 이미 같음 | 권장값 |
| 입력 버퍼 정렬 | 공식 시험: 16바이트 정렬 할당 (`S/test_apps/esp-sr-afe/main/test_afe.cpp:340-343`). 헤더 요구 (`esp_aec.h:87`) | 16바이트 정렬, 내부 메모리 (PM:89, 111) | 이미 같음 | — |
| 기준 신호 추출 | X: 입력을 channel=4, mask=칸0\|칸1 로 열어 드라이버가 2칸만 넘김 (X-codec:194-204; CD-i2s:374-378). "MR" 문자열 (X-afe:132-139) | 4칸 다 읽고 칸0·칸1 을 직접 고름 (BA:153-155; PM:48-49, 126) | 다르지만 유지 | 결과는 같음. 4칸을 받아야 둘째 마이크(칸2)와 `chan` 진단을 쓸 수 있음. 비용은 읽는 양 2배. 탈출구: mask 를 0b0011 로 |
| 기준 증폭·지연 보정 | X: 기준 칸 증폭을 건드리지 않음 (X-codec:205), 지연 보정 없음 | 기준 0dB, 보정 없음 (BA:158; PM:126-135) | 이미 같음 | — |
| 잡음 억제·자동 음량·음성 감지 | X: ns=false, agc=false, vad=true(표시용) (X-afe:153-158, 178; `X/main/Kconfig.projbuild:972-973`). V: 없음 | 없음. 설계서에도 없음 | 이미 같음 | 음성 감지는 통화에 불필요 |
| 표본율 변환 | X: `esp_ae_rate_cvt`, complexity 2 (X-svc:5-10) | 없음 | 다르지만 유지 | 16k 통일이라 불필요 |

### C. 압축·버퍼

| 항목 | 참고 구현 | 우리 코드 | 분류 | 이유 |
|---|---|---|---|---|
| Opus 압축 | X: 16k 모노, 60ms, bitrate AUTO, APPLICATION_AUDIO, complexity 0, FEC off, DTX on, VBR on (X-svc.h:40, 67-80). V: 압축 없음, PCM 그대로 (`V/src/app/app_config.h:24-27`) | 16k 모노, 20ms, 24000bps, APPLICATION_VOIP, complexity 0, FEC·DTX·VBR 전부 off (PM:52-54, 100-102; 기본값 `MC/espressif__esp_audio_codec/include/encoder/impl/esp_opus_enc.h:99-110`) | 다르지만 유지 (complexity 0 은 이미 같음) | 20ms·VOIP 가 통화에 맞음. DTX·VBR 은 §5-9 에서 결정 필요 |
| Opus 해제 | X: `frame_recover = RECOVERY_NONE` 고정 (X-svc:413-418). 시작 때 `esp_opus_dec_reset` + 대기열 비움 (X-svc:798-818) | 해제만 (PM:160-164) | 가져올 것 | 통화 시작 때 해제기 reset + 대기열 비움. 손실 은닉은 §5-1 |
| 수신 버퍼링 | X: 해제 대기열 상한 1200ms, 가득 차면 새 것을 버림 (X-svc.h:43; X-svc:609-629; X-app:554-557). 재생 대기열 120ms (X-svc.h:42). 재정렬·적응 없음. V: 버퍼 없음, 받는 즉시 재생 (`V/src/media/audio.cpp:96-106`) | 없음. 설계는 60ms 시작, 40~200 적응, 300ms 초과 시 오래된 것 폐기 (D:170-171) | 다르지만 유지 | X 는 서버가 음성을 밀어 넣는 구조라 통화용 지터 버퍼 선례가 아님 |
| 송신 대기열 | X: 압축 대기열 2개, 가득 차면 오래된 것 폐기 (X-svc.h:41; X-svc:585-592). 전송 대기열도 오래된 것 폐기 (X-svc:495-500). 전송 실패 시 남은 것 전부 버림 (X-app:240-251) | 설계: 200ms 초과분 폐기 (D:171) | 가져올 것(패턴) | "막지 말고 오래된 것부터 버린다 + 전송 실패 시 비운다". 수치는 우리 200ms 유지 |
| 시작 직후 처리 | X: 음성 처리를 켤 때 입력 120ms 를 버림 (X-svc:276-279, 717) | 없음 | 가져올 것 | 통화 연결 직후 마이크 앞 120ms 버리기 |

### D. 실행 구조

| 항목 | 참고 구현 | 우리 코드 | 분류 | 이유 |
|---|---|---|---|---|
| 태스크 | X: audio_input 6KB/우선 8/코어0, audio_output 4KB/4, opus_codec 24KB/2 (X-svc:127-170), audio_afe 4KB(PSRAM 스택)/3 (`X/main/audio/engines/afe_audio_engine.h:59`; X-afe:206-229). V: mic_tx 4KB/5/코어1, spk_rx 4KB/5/코어0 (`V/src/media/audio.cpp:145-146`) | audio 하나 48KB/10/코어1 이 캡처·에코 제거·Opus·재생 전부, ui 8KB/4/코어1, 시리얼 12KB/3/코어0 (PM:357-359) | 다르지만 유지 | 단일 태스크로 부하 30~37% 통과. 송신·수신을 나눌지는 §5-7 |
| 메모리 배치 | X: `SPIRAM_MALLOC_ALWAYSINTERNAL=1024`, `RESERVE_INTERNAL=102400`, `TRY_ALLOCATE_WIFI_LWIP=y`, `MBEDTLS_EXTERNAL_MEM_ALLOC=y` (`X/sdkconfig.defaults.esp32p4:11-15`), `MBEDTLS_DYNAMIC_BUFFER=y` (`X/sdkconfig.defaults:23`), `ESP_HOSTED_MEMPOOL_PREFER_SPIRAM=y` (`X/main/boards/m5stack/tab5/sdkconfig.tab5:15`) | ALWAYSINTERNAL=16384, RESERVE=32768, Wi-Fi/LWIP PSRAM 미사용, mbedTLS 내부 메모리 (`P/sdkconfig:1589-1593, 2161-2168`) | 다르지만 유지 | 내부 메모리 최저 161KB, TLS 중 116KB 로 여유 (D:257). X 값은 탈출구로 기록. `MEMPOOL_PREFER_SPIRAM` 은 우리 esp_hosted 2.11.7 Kconfig 에 없어 못 가져옴 |
| I2C 공유 | X: IDF i2c_master 버스 하나에 코덱·확장칩·터치·카메라, 앱 수준 잠금 없음 (X-board:105-119, 153-154, 173-181, 543-547; X-codec:31-36). 터치 100kHz (X-board:175). B/V 도 같은 구조 (`B/src/bsp_i2c.c:28-35`) | `M5.In_I2C` 위에 코덱 제어를 얹고 앱 잠금을 한 겹 더, 400kHz (BA:24, 44-46, 54-56, 198-200, 207-210; PM:193). M5GFX 가 트랜잭션마다 내부 뮤텍스를 잡음 (`MC/m5stack__m5gfx/src/lgfx/v1/platforms/esp32/common.inl:1419-1430, 2231, 1797`) | 다르지만 유지 (PH-D4) | 화면·터치를 M5Unified 로 쓰는 한 드라이버를 둘로 나눌 수 없음. 30분 오류 0 실측. 주의점 §5-6 |
| 장치 닫기 | X: 15초 안 쓰면 닫음. 단 입력이 열려 있으면 출력은 닫지 않음 — 주석 "Keep TX clock when duplex RX is active; otherwise RX may stall" (X-svc.h:48; X-svc:833-853) | 부팅 때 열고 유지 | 가져올 것(규칙) | 출력만 닫지 말 것. 닫으려면 둘 다. 다시 열면 채널 증폭 재설정 |
| 부품 판 | X: esp-sr `==2.4.7`, esp_audio_codec `~2.5.0`, esp_codec_dev `~1.6.2`, esp_hosted `^2.12.11`, IDF `>=6.0.1` (`X/main/idf_component.yml:28-30, 42-43, 113-114, 155-156`). V: esp_hosted 2.12.8 + C6 펌웨어 자동 교체 (`V/src/idf_component.yml:17-19`; `V/src/net/wifi_coproc.cpp:71-110`) | esp-sr 2.5.5, esp_audio_codec 2.5.0, esp_codec_dev 1.6.2, esp_hosted 2.11.7, IDF 5.5.4 (`P/main/idf_component.yml:4-11`) | 이미 같음(esp_audio_codec·esp_codec_dev) / 다르지만 유지(esp-sr·esp_hosted) | X 도 같은 이유로 esp_audio_codec 을 2.5 에 묶음 (주석 `X/main/idf_component.yml:28`) |

### E. 연결

| 항목 | 참고 구현 | 우리 설계 | 분류 | 이유 |
|---|---|---|---|---|
| 접속·헤더 | X: 필요할 때만 접속 (X-ws:19-22). `SetHeader` 로 Authorization, Protocol-Version, Device-Id, Client-Id (X-ws:97-106). hello JSON 교환, 10초 제한 (X-ws:176-190). WebSocket 구현은 `78/esp-ml307` 의 `<web_socket.h>` (`X/main/protocols/websocket_protocol.h:7`; `X/main/idf_component.yml:31`) | 상시 연결 + `X-Device-Id`·`X-Device-Token` + esp_websocket_client (D:123, 160) | 다르지만 유지 | 라이브러리가 달라 헤더·401 처리 코드를 그대로 못 가져옴 |
| 이진 프레임 | X: v1 = Opus 그대로, v2 = 16바이트 머리, v3 = 4바이트 머리 (`X/main/protocols/protocol.h:19-33`; X-ws:29-53). 순번 없음 | `[순번 2바이트][Opus]` (D:137) | 다르지만 유지 | 지터 버퍼에 순번이 필요 |
| 재연결 | X: 자동 재연결 없음, 끊기면 콜백만 (X-ws:161-166). 120초 무수신이면 시간 초과 (`X/main/protocols/protocol.cc:108-117`) | 1→2→4…30초 재연결, 15초 ping (D:138, 160) | 다르지만 유지 | 대기 중에도 벨을 받아야 함 |
| 통화 연결 | V: mDNS `_rtsp._tcp` 로 발견 → 자동 연결. 음성은 UDP 5008 직통, 벨·받기 없음, 같은 LAN 전제 (`V/src/app/app_config.h:10-12, 24-27`; `V/src/main.cpp:38-45`; `V/src/media/audio.cpp:176-184`) | 교환기 경유, 벨 + 받기 (PH-D1·D2·D6) | 다르지만 유지 | 서로 다른 망이 요구사항 |

## 3. FD 방식이 깨진 원인 단서

### 확인한 사실

1. **`aec_create` 는 얇은 포장입니다.** `{mic_num=ch, ref_num=ch, out_num=ch, filter_length, sample_rate, caps=0x404, mode, nlp_level=1(AGGR)}` 을 만들어 `aec_create_from_config` 를 부릅니다.
   - 근거: `MC/espressif__esp-sr/lib/esp32p4_less_v3/libesp_audio_processor.a` 의 `esp_aec.c.obj`, `.text.aec_create` 0x00~0x2a 역어셈블.
   - 0x404 = `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` (`IDF/components/heap/include/esp_heap_caps.h:31, 39`).
2. **모드별 엔진** (같은 객체의 `.text.aec_create_from_config`, `.text.aec_process`; `nm` 기호표로 객체 이름 확인):

   | 모드 | 엔진 | 묶음 | NLP |
   |---|---|---|---|
   | VOIP 3·4 | `esp_voip_*` | 256 | 고정 |
   | SR 0 | `esp_aec3_hps16fft_*` (정수, `esp_aec3_dlfft_s16.c.obj`) | 512 | 없음 (out_num 을 0 으로) |
   | SR 1 | `esp_aec3_dlfft_*` (float, `esp_aec3_dlfft_fp32.c.obj`) | 512 | 없음 |
   | **FD_LOW_COST 5** | 정수 엔진 | 512 | 있음 |
   | **FD_HIGH_PERF 6** | float 엔진 | 512 | 있음 |

   - FD 에서 `aec_process` 는 선형 단계를 out=NULL 로 부른 뒤 NLP 단계가 outdata 를 씁니다.
   - 우리 로그와 일치합니다: `chunk=512 … FD_HIGH_PERF` (`P/out/task5.log:354`), `chunk=256 … VOIP_HIGH_PERF` (`P/out/task4.log:209`).
3. **AFE 의 1마이크 구현도 같은 함수를 씁니다.** `esp_afe_sr_1mic.c.obj` 는 `aec_create_from_config` + `aec_process` 를 가져다 씁니다 (`libesp_audio_front_end.a` 의 `nm -u`). xiaozhi "MR" 이 이 객체로 가는지는 이름에서 추론한 것입니다.
4. **xiaozhi 와 우리 호출의 차이는 네 가지입니다.**

   | | xiaozhi | 우리 |
   |---|---|---|
   | 모드 | `FD_LOW_COST` (정수 엔진) (X-afe:151) | `FD_HIGH_PERF` (float 엔진) (PM:51 주석) |
   | NLP 단계 | `VERYAGGR` (X-afe:152) | `AGGR` (aec_create 기본) |
   | esp-sr 판 | `==2.4.7` (`X/main/idf_component.yml:43`) | 2.5.5 |
   | 호출 경로 | AFE 경유 | 직접 |

5. **공식 시험은 FD 를 직접 부르지 않습니다.** `aec_create` 직접 호출은 `AEC_MODE_SR_LOW_COST` 뿐이고 (`S/test_apps/esp-sr-afe/main/test_afe.cpp:333`; `S/test_apps/esp32c5/main/test_aec.cpp:81, 86`), FD 는 `afe_config_init(…, AFE_TYPE_FD, …)` 로만 돌립니다 (`test_afe.cpp:162-181`).
6. **우리 FD 녹음은 처음부터 끝까지 32767 입니다.** `P/out/fd_rec_v80/aec.pcm`, `fd_rec_v100/aec.pcm` 을 읽어 보니 135,971 표본 전부 32767 이고, 첫 표본부터 그렇습니다. 입력이 거의 무음인 앞 구간(마이크 ±7, 기준 ±2)에서도 같습니다. 판정 로그는 `P/out/task5.log:449-459`.
7. **FD 시험은 마이크 증폭 0dB 상태에서만 했습니다.** 녹음 시각은 21:09~21:10 이고 증폭 수정 커밋 28cb3da 는 21:38 입니다. 수정 뒤 재시험 기록은 없습니다.
8. **묶음 크기·정렬·스택은 요구를 지켰습니다.** 묶음은 `aec_get_chunksize` 로 받았고 (PM:98), 버퍼는 16바이트 정렬 (PM:89, 111), 스택 48KB (PM:358).

### 추측 (확인 못 함)

- **float 엔진의 결과가 NaN 또는 +Inf 일 가능성.** RISC-V 는 NaN·+Inf 를 정수로 바꾸면 최대 양수가 되고, int16 으로 자르면 +32767 입니다(CPU 규격 지식, 저장소 인용 아님). 정수 엔진에서는 이 증상이 나올 수 없습니다. 호출 순서가 AFE 와 같으므로(사실 3) 우리 쪽 사용법 오류일 가능성은 낮아 보입니다.
- 후보 원인으로 2.5.5 float 엔진 자체의 문제, 상태 메모리가 PSRAM 인 점, 거의 0 인 입력(증폭 0dB)이 있습니다. 어느 것인지는 가릴 수 없었습니다.

### 가르는 실험 (필요할 때만)

VOIP_HIGH_PERF 가 이미 통과했으므로 FD 는 Phase 3 을 막을 일이 아닙니다. 다시 볼 일이 생기면 한 번에 하나씩:
1. `AEC_MODE_FD_LOW_COST` — xiaozhi 가 쓰는 엔진.
2. `AEC_MODE_SR_HIGH_PERF` — 같은 float 엔진에 NLP 만 없음. 선형 단계와 NLP 중 어느 쪽인지 갈림.
3. `aec_create_from_config` 에 caps 를 내부 메모리로.
4. 마이크 30dB 상태에서 재시험.
5. `aec_get_config_string` 출력 확인.

## 4. 라이선스

| 대상 | 라이선스 | 가져올 때 지킬 것 |
|---|---|---|
| xiaozhi-esp32 | MIT. "Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd." + "Copyright (c) 2025 Project Contributors" (`X/LICENSE:1-4`). 소스 파일에 개별 머리말은 없음 | 코드를 상당 부분 옮기면 저작권 2줄과 허가 문구 전문을 사본에 넣을 것 |
| M5Tab5-VideoLink | MIT. "Copyright (c) 2026 Austin McChord" (`V/LICENSE`) | 같은 조건. 다만 가져올 소리 코드가 사실상 없음. `c6_firmware/network_adapter.bin` 은 가져오지 말 것 |
| esp-bsp m5stack_tab5 (참고로 읽음) | Apache-2.0 (`B/src/bsp_audio.c:2-4`) | 복사 시 라이선스 사본 + 변경 고지 |
| esp-sr | "ESPRESSIF MIT License" — Espressif 제품에서의 사용 허가 (`MC/espressif__esp-sr/LICENSE` 머리) | Tab5 에서는 충족. 맥 단위 시험에 끌어오지 말 것 |
| esp_audio_codec | "Espressif Modified MIT" — Espressif 제품 전용, 타사 제품용 재배포 금지 (`esp_opus_enc.h:2-23`) | 같음 |
| esp_codec_dev / M5Unified·M5GFX | Apache-2.0 / MIT | 그대로 링크해 쓰는 것은 문제 없음 |

- **현재 우리 저장소의 빈틈**: BA:1 에 "근거: xiaozhi … (MIT)" 한 줄만 있고 저작권·허가 문구는 없습니다. 저장소 루트에 LICENSE·NOTICE 파일도 없습니다.
- 본 펌웨어에서 설정 블록을 옮겨 쓴다면 `firmware/tab5_phone/` 에 제3자 고지 파일(MIT 전문 + 저작권 2줄)을 두는 것이 안전합니다. 값·순서만 참고해 새로 쓰면 의무는 약합니다. 법률 자문은 아닙니다.

## 5. 설계서 §9.5·§9.6 과 충돌하거나 설계서가 놓친 것

1. **Opus 손실 은닉 (D:170)** — 지원이 확인되지 않습니다.
   - esp_audio_codec 2.5.0 README 는 손실 은닉을 SBC·LC3 에만 적고 (`MC/espressif__esp_audio_codec/README.md:195, 204`), Opus 해제기 항목에는 없습니다 (`README.md:167-171`).
   - 공통 구조체에 `frame_recover` 필드는 있습니다 (`MC/espressif__esp_audio_codec/include/decoder/esp_audio_dec.h:40-43, 70`). xiaozhi 는 쓰지 않습니다 (X-svc:417).
   - Phase 3 초반에 한 번 시험하고, 안 되면 대안(직전 프레임 감쇠 반복 또는 무음)을 계획에 넣어야 합니다.
2. **에코 제거 단위 (D:169, D:173)** — "약 32ms"가 아니라 16ms 입니다. 선택한 VOIP 모드의 실측이 256표본입니다 (`P/out/task4.log:209`). 32ms(512표본)는 FD·SR 계열입니다. 링버퍼가 필요한 것은 그대로입니다.
3. **재생 쪽 I2S DMA 지연이 예산에 없음 (D:173 "재생 20~40")** — 기본 DMA 는 6×240표본이라 16kHz 에서 한 칸 15ms, 송신 대기는 최대 90ms 입니다 (`IDF/.../i2s_common.h:25-26`; 우리도 기본값 BA:78).
   - 시험 펌웨어는 "읽은 만큼만 쓴다"로 묶여 있습니다 (PM:124, 172).
   - 수신 태스크를 따로 두면 이 묶임이 풀립니다. 쓰는 박자를 캡처에 묶을지, DMA 크기를 줄일지 결정이 필요합니다.
4. **이어폰 감지 (§9.5 board_audio, PH-D7)** — 참고 구현이 없습니다. 아는 것은 "0x43 의 P7 이 HP_DET 입력"이라는 주석뿐입니다.
   - 극성, 이어폰을 꽂으면 스피커가 하드웨어로 끊기는지, 헤드셋 마이크 배선, 이어폰일 때 에코 제거를 끌지가 설계에 없습니다.
   - Phase 5 전에 기기 검증 과제가 필요합니다.
5. **마이크 증폭 재설정 규칙이 진행 로그(D:264)에만 있음** — 모듈 책임으로 올려야 합니다.
   - `esp_codec_dev_set_in_channel_gain` 은 코덱 쓰기 결과를 보지 않고 OK 를 돌려주고 값을 저장하지도 않습니다 (CD-dev:477-493).
   - open 끝에는 전체 증폭을 저장값으로 다시 겁니다 (CD-dev:117-128, 212).
   - 그래서 D:264 의 "반환값 확인"만으로는 실패를 못 잡습니다. 레지스터 되읽기나 I2C 오류 계수 확인이 필요합니다.
6. **I2C 잠금 규칙이 §9.5 에 없음** — M5GFX 는 트랜잭션 단위로만 잠급니다. `bitOn`/`bitOff` 같은 읽고-고쳐-쓰기는 트랜잭션 2개입니다 (`MC/m5stack__m5gfx/src/lgfx/v1/platforms/esp32/common.inl:2504-2514`).
   - 확장칩 0x43·0x44 를 건드리는 모든 코드(앰프, 이어폰 감지, Phase 5 의 배터리·충전 표시)를 같은 앱 잠금 안에서 부르는 규칙을 적어야 합니다.
7. **태스크 구성이 §9.5 에 없음** — 송신(캡처 박자)과 수신(망 박자)을 한 태스크로 할지 나눌지, 우선순위·코어·스택이 정해져 있지 않습니다.
   - 시험 펌웨어는 소리 태스크와 화면 태스크가 같은 코어 1 입니다 (PM:357-358).
   - esp_hosted 태스크는 우선순위 23 으로 소리(10)보다 높습니다 (`P/out/task5.log:154`).
   - WebSocket·TLS 태스크가 더해지므로 Phase 3 부하 재측정과 함께 정해야 합니다.
8. **장치 열고 닫기 규칙 없음** — 대기 중 절전으로 출력만 닫으면 입력이 멈출 수 있다는 xiaozhi 의 경고가 있습니다 (X-svc:845-848). 가장 단순한 안은 "항상 둘 다 열어 두고 앰프만 끈다"입니다.
9. **DTX·VBR 결정 없음 (PH-D5)** — 지금은 고정 24kbps, DTX 꺼짐입니다. DTX 를 켜면 "안 오는 프레임"이 무음인지 유실인지 지터 버퍼가 순번으로 구분해야 합니다. v1 은 끄는 편이 단순합니다.
10. **통화 시작·종료 때 초기화 항목 없음** — 해제기 reset, 지터 버퍼 비움, 마이크 앞 120ms 버림 (X-svc:276-279, 716-717, 798-818).
11. **§12 첫 줄(에코 제거 위험)은 그대로 최대 위험입니다.** 양쪽이 동시에 말할 때의 품질을 검증한 참고 구현이 없습니다. VOIP 모드는 NLP 세기를 조절할 수 없고(라이브러리 경고 문구 "Only full-duplex AEC support NLP level setting"), 조절이 필요해지면 `FD_LOW_COST` 가 후보입니다.

## 6. 확인 못 함

- xiaozhi AFE 의 실제 feed/fetch 묶음 크기, 필터 길이 기본값, AFE 내부 태스크의 코어·우선순위 (기본값이 닫힌 라이브러리 안).
- AFE 의 FD + HIGH_PERF 조합이 `AEC_MODE_FD_HIGH_PERF` 로 대응되는지.
- FD 가 깨진 진짜 원인 (§3 은 좁힌 것까지).
- ES8388 레지스터 0x08 에 0x01 을 쓰면 실제로 무엇이 바뀌는지 (칩 자료 미확인).
- 이어폰 감지 극성·배선, 스피커 자동 차단 여부, 헤드셋 마이크.
- Opus 해제기의 손실 은닉이 실제로 되는지, FEC 해제 지원 여부.
- VideoLink 가 2대 사이에서 실제로 소리가 오가는지 (빌드·실행하지 않음).
- M5Stack 공장 펌웨어의 소리 초기화 (읽지 않음) — `PROBE_TX_TDM` 주석의 근거.
- esp_websocket_client 의 헤더 추가·401 구분·압축 제안 (D:244). xiaozhi 는 다른 라이브러리라 참고가 안 됩니다.
- IDF i2c_master 드라이버의 내부 잠금 (소스 인용 안 함).

## 7. 작업 범위 메모

- 스크래치패드에 내가 받은 것: `ref/xiaozhi-esp32`, `ref/M5Tab5-VideoLink`, `ref/esp-bsp`, `ref/esp-sr`(둘은 필요한 폴더만), 그리고 역어셈블용 `arx/esp_aec.c.obj`.
- `ref/` 아래 `esp-protocols`, `esp-ml307`, `xiaozhi`, `registry-1.8.0`, `*.txt`, `*.diff` 는 내가 만든 것이 아닙니다. 건드리지 않았고 근거로도 쓰지 않았습니다.
- esp-sr 닫힌 라이브러리는 포장 함수 5개(`aec_create`, `aec_create_from_config`, `aec_process`, `aec_linear_process`, `aec_nlp_process`)만 역어셈블했고, 나머지는 기호표와 문자열만 봤습니다.
- 이 세션에서 figma·ms365·notion 연결은 인증이 안 돼 쓸 수 없습니다 (이번 작업과 무관).