#!/bin/bash
# sc_install.sh — 스택짱 2호기에 전화기 펌웨어를 "스트림덱(UIFlow2)과 함께" 처음 넣는다. 설계서 docs/exec-plans/2026-10-08-phone-3way-design.md §11.
#
#   먼저 사람이 한 번: 스택짱의 RST 를 3초(초록 불) → 다운로드 모드. 그 뒤는 이 스크립트가 끝까지 한다.
#   사용: tools/sc_install.sh [--dry-run] [--stay-deck]
#     --dry-run     기기에 쓰지 않고 할 일만 보여 준다(읽기 전용 확인은 한다)
#     --stay-deck   설치 뒤 스트림덱에 머문다(전화기로 넘어가 확인하는 마지막 단계를 건너뛴다)
#
#   하는 일(순서):
#     1. 기기 확인: 다운로드 모드인가, MAC 이 스택짱 2호기인가, 지금의 파티션 표가 UIFlow2 원래 것(백업과 같음)인가
#     2. 지금의 vfs(스트림덱 파일들)를 같은 파일을 담은 1.25MB 짜리로 다시 만든다(tools/sc_dualboot.py — 파일마다 SHA256 대조).
#        기기의 vfs 가 백업과 같으면(기기에서 계산한 MD5) 백업에서 만들고, 다르면 기기에서 읽는다
#     3. 쓴다: 새 파티션 표(0x8000) · 새 vfs(0xAA0000) · 전화기 앱(0xBE0000). 부팅 선택 칸·전화기 설정 칸(0xF50000~0xF58000)은 지운다 = "스트림덱부터"
#        부트로더(0x0)·UIFlow2 앱(0x10000)·sys·nvs·storage 는 한 바이트도 쓰지 않는다.
#     4. 재시작 → 스트림덱이 뜨는지(http://<IP>/health) → 스트림덱에 "전화기" 타일 올리기(~/Deskbot/firmware/stackdeck/deploy.py)
#     5. 전화기로 넘어가(부팅 대상 = ota_0) 전화기 펌웨어가 뜨는지 확인
#   되돌리기(오늘 아침 상태로): 다운로드 모드에서 esptool write-flash 0 backups/stackchan2_before-phone-probe_2026-10-08.bin  (README "복구")
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; ROOT="$(dirname "$HERE")"; REPO="$(cd "$ROOT/../.." && pwd)"
PY=/opt/homebrew/bin/python3; ESPTOOL="${ESPTOOL:-/opt/homebrew/bin/esptool}"
MAC="aa:bb:cc:dd:ee:03"
BACKUP="$REPO/backups/stackchan2_before-phone-probe_2026-10-08.bin"
APP="$ROOT/build/stackchan_phone.bin"; PT="$ROOT/build/partition_table/partition-table.bin"
WORK="$ROOT/out/install"; VENV="$ROOT/out/venv"
DECK_DIR="$HOME/Deskbot/firmware/stackdeck"; DECK_IP="${DECK_IP:-192.168.0.171}"
PT_ADDR=0x8000; VFS_ADDR=0xAA0000; VFS_OLD=0x4C0000; VFS_NEW=0x140000; APP_ADDR=0xBE0000; APP_MAX=$((0x370000)); SEL_ADDR=0xF50000; SEL_SIZE=0x8000
DRY=0; STAY=0
for a in "$@"; do case "$a" in --dry-run) DRY=1 ;; --stay-deck) STAY=1 ;; *) echo "사용: $(basename "$0") [--dry-run] [--stay-deck]" >&2; exit 2 ;; esac; done
die() { echo "중단: $*" >&2; exit 1; }
say() { echo "== $*"; }
run() { if [ "$DRY" = 1 ]; then echo "[dry-run] $*"; else "$@"; fi; }
# 속도(-b)를 주지 않는다: 칩 내장 USB 는 속도가 뜻이 없고, esptool 5.2.0 은 속도를 바꾼 뒤의 read-flash 가 첫 4KB 에서 멈춘다
# ("Serial data stream stopped" — 2026-10-09 이 기기에서 -b 460800 은 두 번 실패, 옵션 없이는 성공).
et() { "$ESPTOOL" --chip esp32s3 -p "$PORT" --before no-reset "$@"; }

say "0. 준비물"
[ -f "$APP" ] && [ -f "$PT" ] || die "빌드가 없다 — 먼저: cd $ROOT && source ~/esp/esp-idf-5.5.4/export.sh && idf.py build"
[ -f "$BACKUP" ] || die "백업이 없다: $BACKUP"
[ "$(stat -f %z "$APP")" -le "$APP_MAX" ] || die "앱이 전화기 칸보다 크다"
mkdir -p "$WORK"
if [ ! -x "$VENV/bin/python" ] || ! "$VENV/bin/python" -c "import littlefs" 2>/dev/null; then
  say "   littlefs 도구 설치($VENV)"
  "$PY" -m venv "$VENV" && "$VENV/bin/pip" install -q littlefs-python
fi
"$VENV/bin/python" "$HERE/sc_dualboot.py" check "$BACKUP" "$PT" >/dev/null || die "새 파티션 표가 UIFlow2 의 표와 맞지 않는다(sc_dualboot.py check)"

say "1. 기기 확인"
KIND=$($PY "$HERE/sc_ctl.py" which)
[ "$KIND" = phone ] || die "스택짱이 '$KIND' 상태다 — RST 를 3초(초록 불) 눌러 다운로드 모드로 넣은 뒤 다시 실행한다. 스트림덱(UIFlow2)이 도는 기기에는 esptool 을 걸지 않는다"
PORT=$($PY "$HERE/sc_ctl.py" port)
GOT=$(et --after no-reset read-mac 2>&1 | grep -io "MAC: *[0-9a-f:]\{17\}" | head -1 | awk '{print tolower($2)}')
[ "$GOT" = "$MAC" ] || die "MAC 이 스택짱 2호기($MAC)가 아니다: '$GOT' (다운로드 모드가 아니면 MAC 을 못 읽는다)"
et --after no-reset read-flash "$PT_ADDR" 0xC00 "$WORK/pt_now.bin" >/dev/null
dd if="$BACKUP" of="$WORK/pt_backup.bin" bs=1 skip=$((PT_ADDR)) count=$((0xC00)) 2>/dev/null
if cmp -s "$WORK/pt_now.bin" "$PT"; then die "이미 설치돼 있다(파티션 표가 새 것이다) — 앱만 바꾸려면 tools/sc_flash.sh"; fi
cmp -s "$WORK/pt_now.bin" "$WORK/pt_backup.bin" || die "지금의 파티션 표가 백업과도, 새 표와도 다르다 — 모르는 상태라 쓰지 않는다($WORK/pt_now.bin 을 확인)"
say "   MAC $GOT · 파티션 표 = UIFlow2 원래 것"

say "2. 스트림덱 파일(vfs)을 1.25MB 로 다시 만들기"
# 먼저 기기의 vfs 가 백업의 vfs 와 같은지 기기에서 계산한 MD5 로 견준다(몇 초). 같으면 백업에서 만든다 — 통째로 읽으면 이 USB 로는 약 7분(11KB/s)이 걸린다.
dd if="$BACKUP" of="$WORK/vfs_backup.bin" bs=65536 skip=$((VFS_ADDR / 65536)) count=$((VFS_OLD / 65536)) 2>/dev/null
SAME=$(et --after no-reset verify-flash "$VFS_ADDR" "$WORK/vfs_backup.bin" 2>&1 || true)
if grep -q "Verification successful" <<<"$SAME"; then
  say "   기기의 vfs = 백업의 vfs(MD5 일치) — 백업에서 만든다"
  cp "$WORK/vfs_backup.bin" "$WORK/vfs_now.bin"
else
  say "   기기의 vfs 가 백업과 다르다 — 기기에서 읽는다(약 7분)"
  et --after no-reset read-flash "$VFS_ADDR" "$VFS_OLD" "$WORK/vfs_now.bin" | tail -2
fi
"$VENV/bin/python" "$HERE/sc_dualboot.py" vfs "$WORK/vfs_now.bin" "$WORK/vfs_new.bin" --size "$VFS_NEW" | tail -3
[ "$(stat -f %z "$WORK/vfs_new.bin")" -eq $((VFS_NEW)) ] || die "새 vfs 의 크기가 다르다"

say "3. 쓰기: 파티션 표 · vfs · 전화기 앱, 그리고 부팅 선택 칸 지우기"
run et --after no-reset write-flash --flash-mode keep --flash-freq keep --flash-size keep "$PT_ADDR" "$PT" "$VFS_ADDR" "$WORK/vfs_new.bin" "$APP_ADDR" "$APP"
run et --after no-reset erase-region "$SEL_ADDR" "$SEL_SIZE"
if [ "$DRY" = 1 ]; then say "dry-run 끝 — 기기에는 쓰지 않았다(기기는 다운로드 모드에 그대로 있다)"; exit 0; fi

say "4. 재시작 → 스트림덱 확인"
et --after watchdog-reset read-mac >/dev/null
OK=0
for i in $(seq 1 45); do
  if [ "$(curl -s -m 2 "http://$DECK_IP/health" || true)" = ok ]; then OK=1; break; fi
  sleep 2
done
[ "$OK" = 1 ] || die "스트림덱이 90초 안에 뜨지 않았다(http://$DECK_IP/health). USB 로 REPL 을 확인하고, 안 되면 README \"복구\""
say "   스트림덱 켜짐"
DECK_PORT=""
for i in $(seq 1 15); do
  KIND=$($PY "$HERE/sc_ctl.py" which)
  [ "$KIND" = deck ] && DECK_PORT=$($PY - <<PYEOF
import sys; sys.path.insert(0, "$HERE"); import sc_ctl; print(sc_ctl.which()[1])
PYEOF
) && break
  sleep 2
done
[ -n "$DECK_PORT" ] || die "스트림덱의 USB 포트가 보이지 않는다"
say "   \"전화기\" 타일 올리기($DECK_PORT)"
(cd "$DECK_DIR" && "$PY" deploy.py --port "$DECK_PORT" --files main.py icons/phone.png)
sleep 12
[ "$(curl -s -m 3 "http://$DECK_IP/health" || true)" = ok ] || die "타일을 올린 뒤 스트림덱이 뜨지 않았다 — ~/Deskbot/firmware/stackdeck 의 main.py 를 git 으로 되돌려 다시 올린다"
if [ "$STAY" = 1 ]; then say "설치 끝 — 스트림덱에 머문다. 전화기로는 스트림덱의 \"전화기\" 타일"; exit 0; fi

say "5. 전화기로 넘어가 확인"
DECK_PORT=$($PY - <<PYEOF
import sys; sys.path.insert(0, "$HERE"); import sc_ctl; print(sc_ctl.which()[1] or "")
PYEOF
)
(cd "$DECK_DIR" && "$PY" deploy.py --port "$DECK_PORT" --no-reset --exec "import esp32; esp32.Partition('ota_0').set_boot(); print('boot=ota_0')") | tail -2
(cd "$DECK_DIR" && "$PY" deploy.py --port "$DECK_PORT" --exec "print('reset')") >/dev/null || true
for i in $(seq 1 20); do [ "$($PY "$HERE/sc_ctl.py" which)" = phone ] && break; sleep 1; done
"$PY" "$HERE/sc_ctl.py" run 'until:stat:^STAT :40' 'shot:'"$WORK"'/phone_first.png' || die "전화기 펌웨어가 응답하지 않는다 — 세 번 연달아 죽으면 스스로 스트림덱으로 돌아간다(SCBOOT 줄을 본다)"
say "설치 끝 — 전화기 펌웨어가 돈다. 화면: $WORK/phone_first.png · 스트림덱으로는 화면의 \"스트림덱\" 키캡(두 번) 또는 tools/sc_ctl.py run deck"
