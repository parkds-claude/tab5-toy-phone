#!/usr/bin/env python3
"""esp_hosted 2.x 의 C-생성자 자동 초기화를 무력화한다 (멱등, CMake 가 빌드마다 실행).

이유: 생성자(esp_hosted_host_init)가 app_main 전에 esp_hosted_init() 를 호출해
 (1) 런타임 핀 설정(esp_hosted_sdio_set_config, Tab5 CLK12/CMD13/D0~3/RST15)이 반영되기 전에 기본 핀으로 전송을 띄우고
 (2) 큰 정적 풋프린트에서는 SDIO mempool 할당이 실패해 부팅 루프(tab5-ssh-terminal 사례).
main/net_wifi.cpp 의 매니저 태스크가 M5.begin() 이후 명시적으로 초기화한다. 재fetch 되어도 이 스크립트가 다시 적용.
출처: firmware/tab5_snapbooth/tools/patch_esp_hosted.py (동작 같음. 적용 여부 판정만 "PATCHED (tab5_" 로 넓혀
      스냅부스·시험 펌웨어에서 이미 패치된 폴더를 가져와도 오류가 나지 않게 했다).
"""
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
TARGET = ROOT / "managed_components/espressif__esp_hosted/host/port/esp/freertos/src/port_esp_hosted_host_init.c"
ORIG = "static void __attribute__((constructor)) esp_hosted_host_init(void)"
PATCHED = ("/* PATCHED (tab5_phone): 생성자 자동초기화 비활성 — net_wifi.cpp 매니저 태스크에서 명시 초기화 */\n"
           "static void __attribute__((unused)) esp_hosted_host_init(void)")

def main() -> int:
    if not TARGET.exists():
        print(f"[patch_esp_hosted] not fetched yet: {TARGET}"); return 0
    src = TARGET.read_text()
    if "PATCHED (tab5_" in src:
        print("[patch_esp_hosted] already applied"); return 0
    if ORIG not in src:
        print("[patch_esp_hosted] ERROR: constructor pattern not found — esp_hosted layout changed", file=sys.stderr); return 1
    TARGET.write_text(src.replace(ORIG, PATCHED)); print("[patch_esp_hosted] applied"); return 0

if __name__ == "__main__":
    sys.exit(main())
