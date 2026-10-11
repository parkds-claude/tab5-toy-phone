# REQUIRES 를 적지 않는다: main 은 그러면 모든 컴포넌트를 자동으로 본다(관리형 컴포넌트 이름 오타 방지 — 시험 펌웨어와 같은 방식).
idf_component_register(SRCS
                           "phone_main.cpp"   # 부팅, 태스크 기동, 시리얼 명령
                           "voice_cmd.cpp"    # 소리·통화 품질의 시리얼 명령(스택짱 전화기와 함께 쓴다)
                           "app.cpp"          # 사건 → 통화 상태 기계 → 할 일 실행 → 다시 그리기
                           "ui.cpp"           # 통화 화면 그리기
                           "relay_link.cpp"   # 교환기 연결·재연결·ping
                           "device_id.cpp"    # 칩 MAC → 기기 번호·ID·토큰
                           "gfx.cpp"          # 화면 합성 버퍼·출력·스크린샷·내장 글꼴 글자(기기 쪽)
                           "gfx_draw.cpp"     # 그리기 함수(도형·글자 자산·색 그림) — raster.h 로만 그린다. 맥의 화면 미리보기와 함께 쓴다
                           "toy.cpp"          # 토이폰 테마의 부품(바탕·키캡·알약·캐릭터·아이콘)
                           "net_wifi.cpp"     # Wi-Fi 매니저
                           "ui_wifi.cpp"      # Wi-Fi 설정 화면
                           "audio_out.cpp"    # 스피커 출력(ES8388 + I2S 송신) — 공급자에게 PCM 을 받아 낸다(Phase 3a)
                           "audio_board_tab5.cpp"   # 소리 기판 부분(ES8388·확장칩 앰프) — audio_board.h. 스택짱은 ../stackchan_phone/main/audio_board_cores3.cpp
                           "tone_out.cpp"     # 신호음 공급자(벨·연결음·종료음, 박자는 tones.h)
                           "voice.cpp"        # 목소리 통화 경로(마이크 → 에코 제거 → Opus → 교환기 → 지터 버퍼 → Opus → 스피커, 판단은 voice_core.h) — Phase 3b
                       INCLUDE_DIRS ".")

# B-D1: -Wliteral-suffix 한 종류만 끈다(main 의 C++ 파일에만).
#   이유: <M5Unified.h> 를 포함하는 파일마다 ESP-IDF 5.5.4 헤더(esp_private/periph_ctrl.h:88, "…"CONFIG_IDF_TARGET 사이에 공백 없음)에서
#         같은 경고가 1건씩 찍힌다 — 스냅부스·시험 펌웨어 빌드 기록에도 있는 소음이다. 우리 코드의 새 경고가 묻히지 않게 끈다.
#   비용: 우리 코드에서 같은 종류("문자열"매크로 를 붙여 쓴 것)의 경고도 안 보인다. 탈출구: 아래 한 줄을 지운다.
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-literal-suffix>)
