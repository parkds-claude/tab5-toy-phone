#!/bin/bash
# sc_flash.sh — 스택짱 전화기 펌웨어(앱만)를 다시 올린다. 손이 필요 없다 — 전화기 펌웨어가 돌고 있을 때(또는 다운로드 모드일 때)만 쓴다.
#   사용: tools/sc_flash.sh [--dry-run]
#   앱은 전화기 칸(ota_0 = 0xBE0000)에만 쓴다. 부트로더·파티션 표·스트림덱(UIFlow2)·설정 칸은 건드리지 않는다.
#   스트림덱이 돌고 있으면 거절한다: UIFlow2 가 도는 기기에 esptool 을 걸면 USB 에서 사라져 전원을 껐다 켜야 돌아온다(2026-10-08 세 번 확인 — ~/CLAUDE.md Gotchas).
#   그때는 스트림덱의 "전화기" 타일로 먼저 넘어온다. 처음 설치는 tools/sc_install.sh.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; ROOT="$(dirname "$HERE")"
PY=/opt/homebrew/bin/python3; ESPTOOL="${ESPTOOL:-/opt/homebrew/bin/esptool}"
APP="$ROOT/build/stackchan_phone.bin"; APP_ADDR=0xBE0000; APP_MAX=$((0x370000)); MAC="aa:bb:cc:dd:ee:03"
DRY=0; [ "${1:-}" = "--dry-run" ] && DRY=1
die() { echo "중단: $*" >&2; exit 1; }
run() { if [ "$DRY" = 1 ]; then echo "[dry-run] $*"; else "$@"; fi; }

[ -f "$APP" ] || die "앱이 없다: $APP (먼저 idf.py build)"
SIZE=$(stat -f %z "$APP"); [ "$SIZE" -le "$APP_MAX" ] || die "앱이 전화기 칸보다 크다: $SIZE > $APP_MAX"
KIND=$($PY "$HERE/sc_ctl.py" which)
[ "$KIND" = phone ] || die "스택짱이 '$KIND' 상태다 — 전화기 펌웨어가 돌 때만 올린다(스트림덱이면 \"전화기\" 타일로 먼저 넘어온다)"
PORT=$($PY "$HERE/sc_ctl.py" port)
echo "포트 $PORT · 앱 $SIZE B → $APP_ADDR"
if [ "$DRY" = 0 ]; then
  GOT=$("$ESPTOOL" --chip esp32s3 -p "$PORT" --after no-reset read-mac 2>&1 | grep -io "MAC: *[0-9a-f:]\{17\}" | head -1 | awk '{print tolower($2)}')
  [ "$GOT" = "$MAC" ] || die "MAC 이 스택짱 2호기가 아니다: '$GOT'"
  PORT=$($PY "$HERE/sc_ctl.py" port)          # 다운로드 모드로 들어가며 포트 이름이 바뀔 수 있다
fi
# 속도(-b)를 주지 않는다 — 칩 내장 USB 는 속도가 뜻이 없고 esptool 5.2.0 은 속도를 바꾸면 읽기가 멈춘다(tools/sc_install.sh 의 et 주석).
run "$ESPTOOL" --chip esp32s3 -p "$PORT" --before no-reset --after watchdog-reset write-flash --flash-mode keep --flash-freq keep --flash-size keep "$APP_ADDR" "$APP"
echo "올림 끝 — 전화기 펌웨어가 다시 뜬다(확인: tools/sc_ctl.py run 'until:stat:^STAT :40')"
