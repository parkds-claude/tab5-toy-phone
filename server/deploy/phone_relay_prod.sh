#!/bin/bash
# phone_relay_prod.sh — Tab5 전화 교환기를 맥미니(24시간 서버)에 세우고 돌보는 스크립트. 맥미니에서 실행한다.
#
# 사용: phone_relay_prod.sh <낱말> [relay|tunnel] [--dry-run]
#   install          교환기 launchd 항목(com.example.tab5phone-relay) 만들기·등록·시작
#   tunnel-install   전용 Cloudflare 터널 tab5phone 만들기(없으면) → 설정 → 검사 → DNS → launchd 항목(com.example.tab5phone-tunnel) 등록·시작
#   start | stop | restart | status    교환기·터널 각각(뒤에 relay 또는 tunnel 을 쓰면 그쪽만. 터널 재시작은 통화를 끊으니 교환기만 고칠 땐 `restart relay`)
#   uninstall        둘 다 내리고 plist 삭제(터널·DNS 는 지우지 않고 안내만 출력)
#   --dry-run        실행할 명령만 출력(읽기 전용 확인 — launchctl print·lsof·cloudflared tunnel list·curl — 은 한다)
# 끝나는 값: 0 성공 · 1 실패/중단 · 2 인자가 틀림. 모든 낱말은 여러 번 실행해도 같은 결과(이미 된 것은 "이미"라고 말하고 그대로 둔다).
#
# 결정:
#  P4-D2 전용 터널 — 맥미니의 기존 터널(설정·launchd 항목은 아래 가드가 보호)은 읽기 외에 만지지 않는다.
#        이유: 거기에 호스트를 더하면 cloudflared 재시작으로 다른 서비스 20여 개가 끊기고, 앞으로 그 터널이 재시작될 때마다 통화가 끊긴다.
#        비용: 프로세스·plist 하나 더. 탈출구: uninstall 후 터널을 지우고 기존 설정에 호스트 한 덩이를 넣는다.
#  P4-D3 교환기는 사용자 영역(gui/<uid>) launchd — RunAtLoad+KeepAlive. plist 에 토큰이 없다(교환기가 server/.env.phone 을 읽는다).
#  P4N-D8 install 은 plist 가 같고 이미 떠 있으면 아무것도 안 한다(재실행이 통화를 끊지 않게). 비용: 이미 떠 있는 것의 "상태 고치기"는 restart 로 따로.
#  P4N-D9 BIND·PORT 는 plist 환경으로 고정(127.0.0.1:5094) — .env.phone 에 개발용 BIND=0.0.0.0 이 섞여 와도 밖으로 열리지 않는다.
set -euo pipefail
# ssh 비대화형 셸은 PATH 에 /opt/homebrew/bin 이 없다 — 뒤에 덧붙인다(앞에 두면 사용자가 지정한 도구·시험용 가짜 명령을 가린다).
PATH="$PATH:/opt/homebrew/bin:/usr/local/bin"

readonly LABEL_RELAY=com.example.tab5phone-relay
readonly LABEL_TUNNEL=com.example.tab5phone-tunnel
readonly TUNNEL_NAME=tab5phone
readonly HOST=phone.example.com
readonly PORT=5094
readonly PYTHON="${PHONE_PYTHON:-/opt/homebrew/bin/python3}"
readonly SERVER_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly DEPLOY_DIR="$SERVER_DIR/deploy"
readonly ENV_FILE="$SERVER_DIR/.env.phone"
readonly LA_DIR="$HOME/Library/LaunchAgents"
readonly LOG_DIR="$HOME/Library/Logs/tab5phone"
readonly CF_DIR="$HOME/.cloudflared"
readonly TUNNEL_CFG="$CF_DIR/tab5phone.yml"
readonly TUNNEL_CRED="$CF_DIR/tab5phone.credentials.json"   # P4N-D12: 전용 credentials 경로(create 에 명시) — <UUID>.json 을 기대하지 않는다
readonly SHARED_CFG="$CF_DIR/config.yml"  # GUARD
DOMAIN="gui/$(id -u)"
DRY=0

usage() {
  echo "사용: $(basename "$0") install | tunnel-install | start | stop | restart | status | uninstall   [relay|tunnel] [--dry-run]" >&2
  exit 2
}
say() { echo "$*"; }
die() { echo "중단: $*" >&2; exit 1; }
# 쓰는 명령은 전부 run 을 거친다 — --dry-run 이면 출력만.
run() { if [ "$DRY" = 1 ]; then echo "[dry-run] $*"; else "$@"; fi; }
guard() { case "$1" in */.cloudflared/config.yml|*maintunnel*) die "공용 터널 설정·항목은 건드리지 않는다: $1" ;; esac; }  # GUARD

# ── 인자 ──
POS=()
for a in "$@"; do
  case "$a" in
    --dry-run) DRY=1 ;;
    -*) usage ;;
    *) POS+=("$a") ;;
  esac
done
[ "${#POS[@]}" -ge 1 ] && [ "${#POS[@]}" -le 2 ] || usage
CMD="${POS[0]}"
TARGET="${POS[1]:-both}"
case "$CMD" in
  install|tunnel-install|uninstall) [ "${#POS[@]}" -eq 1 ] || usage ;;
  start|stop|restart|status) case "$TARGET" in both|relay|tunnel) ;; *) usage ;; esac ;;
  *) usage ;;
esac
[ "$DRY" = 1 ] && say "[dry-run] 실제로는 아무것도 바꾸지 않는다(읽기 전용 확인만 한다)"

# ── 공통 도구 ──
render() {  # 템플릿의 __자리__ 를 채운 글
  sed -e "s|__HOME__|$HOME|g" -e "s|__PYTHON__|$PYTHON|g" -e "s|__SERVER_DIR__|$SERVER_DIR|g" -e "s|__CLOUDFLARED__|${CLOUDFLARED:-cloudflared}|g" "$1"
}
loaded() { launchctl print "$DOMAIN/$1" >/dev/null 2>&1; }
service_pid() { launchctl print "$DOMAIN/$1" 2>/dev/null | awk '/^[[:space:]]*pid = /{print $3; exit}' || true; }
plist_of() { echo "$LA_DIR/$1.plist"; }
label_for() { [ "$1" = relay ] && echo "$LABEL_RELAY" || echo "$LABEL_TUNNEL"; }
targets() {  # start/restart 순서: 교환기 → 터널 / stop 순서는 반대
  case "$TARGET" in both) if [ "$1" = rev ]; then echo "tunnel relay"; else echo "relay tunnel"; fi ;; *) echo "$TARGET" ;; esac
}
# P4N-D10: 터널은 이름이 아니라 UUID 로만 다룬다. 이유: 맥미니에서는 기본 설정(공용 터널)이 자동으로 읽혀, 이름으로 부른 info/route dns/delete/run 이
#   이름과 상관없이 공용 터널(maintunnel)로 간다(cloudflared findID). 존재 확인은 `tunnel list --name --output json`(이름이 정확히 같고 삭제 안 된 것만).
#   비용: python3 로 JSON 을 읽는다. 탈출구: 없음(이 방식이 안전한 유일한 방법).
tunnel_id_by_name() {  # 없으면 빈 글자
  local json
  json="$("$CLOUDFLARED" tunnel list --name "$TUNNEL_NAME" --output json 2>/dev/null || true)"
  [ -n "$json" ] || return 0
  "$PYTHON" -c '
import json, sys
name = sys.argv[1]
try:
    items = json.loads(sys.stdin.read() or "[]") or []
except ValueError:
    items = []
for t in items:
    if t.get("name") == name and str(t.get("deleted_at", "0001-01-01T00:00:00Z")).startswith("0001-01-01"):
        print(t.get("id", ""))
        break
' "$TUNNEL_NAME" <<<"$json"
}
shared_tunnel_id() { awk '/^tunnel:/{print $2; exit}' "$CF_DIR/config.yml" 2>/dev/null || true; }  # GUARD
shared_fp() {  # 공용 설정 + 그 credentials 의 해시 한 줄(내용은 출력하지 않는다)
  local cred; cred="$(awk '/^credentials-file:/{print $2; exit}' "$SHARED_CFG" 2>/dev/null || true)"
  { cat "$SHARED_CFG" 2>/dev/null || true; echo ---; if [ -n "$cred" ]; then cat "$cred" 2>/dev/null || true; fi; } | shasum -a 256 | awk '{print $1}'
}
refuse_shared_id() {  # 얻은 UUID 가 공용 터널과 같으면 무조건 중단
  local shared; shared="$(shared_tunnel_id)"  # GUARD
  [ -z "$shared" ] || [ "$1" != "$shared" ] || die "이 ID 는 공용 터널이다 — 쓰지 않는다(tab5phone 이 아님)"
}

http_code() { curl -s -m 5 -o /dev/null -w '%{http_code}' "$1" 2>/dev/null || true; }

write_text() {  # write_text <경로> <내용> <권한> — 같은 폴더 임시 파일 → mv. 바꿨으면 0, 이미 같으면 1
  local dest="$1" content="$2" mode="$3"
  guard "$dest"
  if [ -f "$dest" ] && [ "$(cat "$dest")" = "$content" ]; then return 1; fi
  if [ "$DRY" = 1 ]; then echo "[dry-run] 파일 쓰기: $dest"; return 0; fi
  mkdir -p "$(dirname "$dest")"
  local tmp; tmp="$(mktemp "$dest.XXXXXX")"
  printf '%s\n' "$content" > "$tmp"
  chmod "$mode" "$tmp"
  mv "$tmp" "$dest"
  return 0
}

install_agent() {  # install_agent <라벨> <템플릿 이름>
  local label="$1" tpl="$DEPLOY_DIR/$2" dest changed=0 was_loaded=0
  guard "$label"; dest="$(plist_of "$label")"
  [ -f "$tpl" ] || die "템플릿이 없음: $tpl"
  local content; content="$(render "$tpl")"
  loaded "$label" && was_loaded=1
  write_text "$dest" "$content" 644 && changed=1
  if [ "$changed" = 0 ] && [ "$was_loaded" = 1 ]; then say "이미 설치·실행 중: $label (그대로 둠)"; return 0; fi
  if [ "$was_loaded" = 1 ]; then run launchctl bootout "$DOMAIN/$label" || true; [ "$DRY" = 1 ] || sleep 1; fi
  run launchctl bootstrap "$DOMAIN" "$dest"
  say "등록·시작: $label"
}

wait_health() {  # 교환기가 뜰 때까지 최대 15초
  [ "$DRY" = 1 ] && { say "[dry-run] http://127.0.0.1:$PORT/healthz 가 200 이 될 때까지 기다림"; return 0; }
  local i
  for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
    [ "$(http_code "http://127.0.0.1:$PORT/healthz")" = 200 ] && { say "교환기 응답 확인: http://127.0.0.1:$PORT/healthz 200"; return 0; }
    sleep 1
  done
  die "교환기가 15초 안에 응답하지 않음 — 로그: $LOG_DIR/relay.log"
}

# ── install ──
cmd_install() {
  [ -f "$SERVER_DIR/phone_relay.py" ] || die "phone_relay.py 가 없음: $SERVER_DIR"
  [ -f "$ENV_FILE" ] || die "$ENV_FILE 가 없음 — 맥북의 server/.env.phone 을 파일 그대로 scp 로 옮긴 뒤 다시 실행"
  grep -q '^PHONE_TOKENS=' "$ENV_FILE" || die ".env.phone 에 PHONE_TOKENS= 줄이 없음"
  [ "$(stat -f %Lp "$ENV_FILE")" = 600 ] || { say ".env.phone 권한을 600 으로 조임"; run chmod 600 "$ENV_FILE"; }
  "$PYTHON" -c 'import aiohttp' 2>/dev/null || die "$PYTHON 에 aiohttp 가 없음 — $PYTHON -m pip install -r $SERVER_DIR/requirements-phone.txt (시험 도구 제외하려면 aiohttp 만)"
  local owner mine
  owner="$(lsof -nP -iTCP:$PORT -sTCP:LISTEN -t 2>/dev/null | head -1 || true)"
  mine="$(service_pid "$LABEL_RELAY")"
  if [ -n "$owner" ] && [ "$owner" != "$mine" ]; then die "포트 $PORT 를 다른 프로세스(pid $owner)가 쓰고 있음 — 먼저 정리: lsof -nP -iTCP:$PORT -sTCP:LISTEN"; fi
  run mkdir -p "$LOG_DIR"
  install_agent "$LABEL_RELAY" com.example.tab5phone-relay.plist.template
  wait_health
}

# ── tunnel-install ──
cmd_tunnel_install() {
  CLOUDFLARED="$(command -v cloudflared || true)"
  [ -n "$CLOUDFLARED" ] || die "cloudflared 가 없음 (brew install cloudflared)"
  [ -f "$CF_DIR/cert.pem" ] || die "$CF_DIR/cert.pem 이 없음 — 먼저 cloudflared tunnel login (브라우저에서 example.com 선택)"
  local tid=""
  tid="$(tunnel_id_by_name)"
  if [ -n "$tid" ]; then
    [[ "$tid" =~ ^[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}$ ]] || die "터널 ID 를 읽지 못함(cloudflared tunnel list --name $TUNNEL_NAME)"
    refuse_shared_id "$tid"
    say "터널 이미 있음: $TUNNEL_NAME ($tid)"
    [ -f "$TUNNEL_CRED" ] || die "터널 credentials 파일이 없음($TUNNEL_CRED) — 다른 기기에서 만든 터널이면 그 기기의 credentials 를 이 경로로 옮기거나, 터널을 지우고(UUID 로) 다시 만든다"
  else
    [ ! -e "$TUNNEL_CRED" ] || die "터널은 없는데 credentials 파일만 있음($TUNNEL_CRED) — 덮어쓰지 않는다. 옛 터널의 흔적이면 파일을 다른 곳에 옮기고 다시 실행"
    # P4N-D12: create 에 --credentials-file 을 명시하고, 앞뒤로 공용 설정·공용 credentials 해시를 비교한다(기본 설정이 읽히는 기기에서 공용 파일이 안 바뀌었는지)
    local before after
    before="$(shared_fp)"
    run "$CLOUDFLARED" tunnel create --credentials-file "$TUNNEL_CRED" "$TUNNEL_NAME"
    if [ "$DRY" = 1 ]; then tid="<터널ID>"; else
      after="$(shared_fp)"
      [ "$before" = "$after" ] || die "공용 터널 설정 또는 credentials 가 tunnel create 중에 바뀜 — 즉시 중단. 사람이 ~/.cloudflared 를 확인(백업 필요)"
      tid="$(tunnel_id_by_name)"
      [[ "$tid" =~ ^[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}$ ]] || die "터널을 만들었으나 ID 를 읽지 못함(cloudflared tunnel list --name $TUNNEL_NAME)"
      refuse_shared_id "$tid"
      [ -f "$TUNNEL_CRED" ] || die "터널은 만들었으나 credentials 파일이 없음($TUNNEL_CRED)"
    fi
  fi
  # 긴 연결(WebSocket)용 originRequest — snapbooth 설정(tunnel_add_snapbooth.sh)과 같은 값. 통화 중 연결은 기기가 15초마다 ping 해 계속 살아 있다.
  local cfg="tunnel: $tid
credentials-file: $TUNNEL_CRED
# Tab5 전화기 전용 터널 — phone_relay_prod.sh tunnel-install 이 만든다. 직접 고치지 말고 스크립트를 다시 실행할 것.
ingress:
  - hostname: $HOST
    service: http://127.0.0.1:$PORT
    originRequest:
      connectTimeout: 30s
      tlsTimeout: 10s
      keepAliveTimeout: 90s
      disableChunkedEncoding: false
  - service: http_status:404"
  [ "$DRY" = 1 ] || refuse_shared_id "$tid"             # 설정 쓰기 직전에 한 번 더
  write_text "$TUNNEL_CFG" "$cfg" 600 && say "터널 설정 씀: $TUNNEL_CFG" || say "터널 설정 이미 같음: $TUNNEL_CFG"
  run "$CLOUDFLARED" tunnel --config "$TUNNEL_CFG" ingress validate
  if [ "$DRY" = 1 ]; then echo "[dry-run] $CLOUDFLARED tunnel route dns $tid $HOST"; else
    local out
    if ! out="$("$CLOUDFLARED" tunnel route dns "$tid" "$HOST" 2>&1)"; then
      if grep -q 'already exists' <<<"$out"; then say "DNS 이미 있음: $HOST (그 레코드가 이 터널을 가리키는지는 status 의 https 응답으로 확인)"; else die "DNS 등록 실패: $out"; fi
    else say "DNS 등록: $HOST"; fi
  fi
  [ "$(http_code "http://127.0.0.1:$PORT/healthz")" = 200 ] || say "주의: 교환기(127.0.0.1:$PORT)가 아직 응답하지 않음 — 터널을 켜도 502 가 난다. install 을 먼저 실행"
  run mkdir -p "$LOG_DIR"
  install_agent "$LABEL_TUNNEL" com.example.tab5phone-tunnel.plist.template
  say "확인: $(basename "$0") status  (https://$HOST/healthz 가 200 이어야 함 — DNS 반영에 몇 초 걸릴 수 있음)"
}

# ── start/stop/restart ──
# P4N-D11: restart 는 bootout(SIGTERM) → 내려간 것 확인 → bootstrap. kickstart -k 는 SIGKILL 이라 교환기의 종료 훅(기기에 1001 닫기)이 안 돌아
#   기기가 15~20초 뒤에야 안다. 비용: 몇 초 더 걸림. 탈출구: kickstart -k 로 되돌리면 훅 없이 즉시 재시작.
restart_agent() {
  local label="$1" i
  run launchctl bootout "$DOMAIN/$label" || true
  if [ "$DRY" != 1 ]; then
    for ((i = 0; i < ${PHONE_WAIT_DOWN_SECS:-15}; i++)); do loaded "$label" || break; sleep 1; done
    loaded "$label" && die "$label 이 ${PHONE_WAIT_DOWN_SECS:-15}초 안에 내려가지 않음 — 서비스 상태를 확인하고, 다 내려간 것을 확인한 뒤 $(basename "$0") start 를 실행"
  fi
  [ "$DRY" = 1 ] || [ -f "$(plist_of "$label")" ] || die "plist 가 없어 다시 올릴 수 없음: $(plist_of "$label") — install 을 다시 실행"
  run launchctl bootstrap "$DOMAIN" "$(plist_of "$label")" || die "$label bootstrap 실패 — 서비스가 꺼진 채다. 원인(로그·plist)을 확인한 뒤 $(basename "$0") start 를 실행"
  say "restart(bootout→bootstrap): $label"
}
cmd_start() {  # start 와 restart 공용: 떠 있으면 start=그대로 kickstart, restart=kickstart -k
  local t label
  for t in $(targets fwd); do
    label="$(label_for "$t")"
    if loaded "$label"; then
      if [ "$CMD" = restart ]; then restart_agent "$label"; else run launchctl kickstart "$DOMAIN/$label"; say "start: $label"; fi
    elif [ -f "$(plist_of "$label")" ]; then
      run launchctl bootstrap "$DOMAIN" "$(plist_of "$label")"; say "등록·시작: $label"
    else
      say "설치 안 됨: $label — 먼저 $( [ "$t" = relay ] && echo install || echo tunnel-install )"
    fi
  done
}
cmd_stop() {
  local t label
  for t in $(targets rev); do
    label="$(label_for "$t")"
    if loaded "$label"; then run launchctl bootout "$DOMAIN/$label"; say "내림: $label"; else say "이미 내려가 있음: $label"; fi
  done
}

# ── status ──
cmd_status() {
  local t label pid code rc=0
  for t in $(targets fwd); do
    label="$(label_for "$t")"
    if loaded "$label"; then
      pid="$(service_pid "$label")"
      if [ -n "$pid" ]; then say "$label: 실행 중 (pid $pid)"; else say "$label: 등록됨, 프로세스 없음(멈춤 또는 재시작 대기)"; rc=1; fi
    else say "$label: 등록 안 됨"; rc=1; fi
    if [ "$t" = relay ]; then
      code="$(http_code "http://127.0.0.1:$PORT/healthz")"; say "  http://127.0.0.1:$PORT/healthz → ${code:-000}"; [ "$code" = 200 ] || rc=1
    else
      code="$(http_code "https://$HOST/healthz")"; say "  https://$HOST/healthz → ${code:-000}"; [ "$code" = 200 ] || rc=1
    fi
  done
  say "로그: $LOG_DIR/relay.log, $LOG_DIR/tunnel.log"
  return "$rc"
}

# ── uninstall ──
cmd_uninstall() {
  local label
  for label in "$LABEL_TUNNEL" "$LABEL_RELAY"; do
    guard "$label"
    if loaded "$label"; then run launchctl bootout "$DOMAIN/$label"; fi
    if [ -f "$(plist_of "$label")" ]; then guard "$(plist_of "$label")"; run rm -f "$(plist_of "$label")"; say "plist 삭제: $label"; else say "plist 없음: $label"; fi
  done
  say "터널·DNS·설정 파일·로그는 지우지 않았다. 완전히 없애려면 사람이 직접:"
  local tid=""
  CLOUDFLARED="$(command -v cloudflared || true)"
  [ -z "$CLOUDFLARED" ] || tid="$(tunnel_id_by_name)"
  say "  !! 이 기기에서는 이름으로 지우면 공용 터널이 지워질 수 있다 — 반드시 UUID 로:"
  if [ -n "$tid" ]; then say "  cloudflared tunnel delete $tid      # 터널 삭제(연결이 없어야 함)"
  else say "  cloudflared tunnel list --name $TUNNEL_NAME --output json 로 UUID 를 확인한 뒤 cloudflared tunnel delete <UUID>"; fi
  say "  Cloudflare 대시보드 → example.com → DNS 에서 $HOST 레코드 삭제"
  say "  rm $TUNNEL_CFG"
}

case "$CMD" in
  install) cmd_install ;;
  tunnel-install) cmd_tunnel_install ;;
  start|restart) cmd_start ;;
  stop) cmd_stop ;;
  status) cmd_status ;;
  uninstall) cmd_uninstall ;;
esac
