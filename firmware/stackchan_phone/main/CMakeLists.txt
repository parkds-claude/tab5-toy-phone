# 스택짱 전화기 = 스택짱 전용 소스(sc_*, audio_board_cores3) + Tab5 전화기와 함께 쓰는 소스(경로로 끌어 쓴다 — 설계서 TW-D11: 공통 로직은 복사하지 않는다).
# REQUIRES 를 적지 않는다: main 은 그러면 모든 컴포넌트를 자동으로 본다(Tab5 프로젝트와 같은 방식).
set(TAB5 "${CMAKE_CURRENT_LIST_DIR}/../../tab5_phone/main")
idf_component_register(SRCS
                           "sc_main.cpp"              # 부팅, 태스크 기동, 시리얼 명령
                           "sc_app.cpp"               # 사건 → 통화 상태 기계 → 할 일 실행 → 다시 그리기
                           "sc_ui.cpp"                # 화면 그리기(순수 — 맥의 화면 미리보기와 함께 쓴다)
                           "sc_gfx.cpp"               # 합성 버퍼 → 화면, 화면 찍기
                           "sc_net.cpp"               # Wi-Fi(칩 내장, 고정 목록) — Tab5 의 net_wifi.h 인터페이스
                           "sc_boot.cpp"              # 스트림덱 ↔ 전화기 부팅 선택
                           "sc_store.cpp"             # 전화기 전용 설정 칸(nvs_phone) + 플래시 일꾼
                           "audio_board_cores3.cpp"   # 소리 기판 부분(AW88298 앰프) — Tab5 의 audio_board.h 인터페이스
                           # ── 아래는 Tab5 전화기와 같은 파일 ──
                           "${TAB5}/audio_out.cpp"    # 소리 장치(스피커 출력 + 마이크 입력)
                           "${TAB5}/tone_out.cpp"     # 신호음 공급자(벨·연결음·종료음)
                           "${TAB5}/voice.cpp"        # 목소리 통화 경로
                           "${TAB5}/voice_cmd.cpp"    # 소리·통화 품질의 시리얼 명령
                           "${TAB5}/relay_link.cpp"   # 교환기 연결·재연결·ping
                           "${TAB5}/device_id.cpp"    # 칩 MAC → 기기 번호·ID·토큰
                       INCLUDE_DIRS "." "${TAB5}")

# Tab5 프로젝트와 같은 이유(B-D1): <M5Unified.h> 를 포함하는 파일마다 ESP-IDF 헤더에서 같은 경고가 1건씩 찍힌다 — 우리 코드의 새 경고가 묻히지 않게 그 한 종류만 끈다.
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-literal-suffix>)
