# Tab5 전화기 — 참고 구현 조사 (2026-10-04)

Phase 2~3 계획을 쓰기 전에 한 대조 조사 세 건의 보고 원문이다. 사용자 지시(2026-10-03 "관련 레포가 있으면 리버스 엔지니어링을 통해 진행할 것")에 따른 것으로,
기기에서 시행착오하기 전에 같은 기기에서 동작하는 구현과 줄 단위로 대조하기 위한 근거 자료다.

| 파일 | 대조 대상 | 한 줄 결론 |
|---|---|---|
| [audio-reference-diff.md](audio-reference-diff.md) | xiaozhi-esp32 Tab5 보드(커밋 0d576d3), M5Tab5-VideoLink(40f6b7e), esp-bsp m5stack_tab5 v1.3.1, esp-sr 2.5.5 ↔ 우리 시험 펌웨어 `firmware/tab5_phone_probe/` | 두 레포 모두 Tab5 에서 "스피커폰 동시 통화 + 에코 제거"를 돌리지 않는다. 하드웨어 초기화는 이미 같다. 가져올 것은 운영 규칙 4가지, 설계서 정정 3건 |
| [websocket-client.md](websocket-client.md) | esp_websocket_client 1.8.0 소스 + ESP-IDF 5.5.4 `transport_ws.c` ↔ 우리 교환기 계약(`server/phone_core.py`·`phone_relay.py`) | 쓸 수 있다. 기본 설정이면 세 군데가 계약과 어긋난다(자동 ping, 서버가 닫을 때 멈춤, 보내기 시간 제한) |
| [snapbooth-reuse.md](snapbooth-reuse.md) | 우리 스냅부스 펌웨어 `firmware/tab5_snapbooth/` ↔ 새 펌웨어 `firmware/tab5_phone/` 에 필요한 것 | 그대로 복사 가능한 파일은 6개, 나머지는 `main.cpp` 에서 줄 범위로 떼어 내야 한다 |

읽을 때 주의:
- 보고 안의 `<스크래치패드>/ref/…` 경로는 조사 때 임시로 받은 사본이다(남아 있지 않을 수 있다). 다시 볼 때는 적힌 커밋·판 번호로 받는다.
- 모두 **소스 읽기** 결과다. 기기에서 확인한 것이 아니며, 각 보고의 "확인 못 함" 절에 한계가 적혀 있다.
- 이 조사를 반영한 계획: `docs/exec-plans/2026-10-04-tab5-phone-phase2.md` (소리는 Phase 3 계획에서).
