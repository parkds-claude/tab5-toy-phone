#!/bin/bash
# phone_relay_dev.sh — 개발용: 맥북에서 전화기 교환기(phone_relay.py)를 같은 Wi-Fi 의 기기가 붙을 수 있게 띄우고 끈다.
# Phase 2~3 기기 시험 전용(계획 docs/exec-plans/2026-10-04-tab5-phone-phase2.md Task 7~9). 운영(맥미니·터널)은 Phase 4.
#
# PRD-D1: 교환기 시작·종료를 이 스크립트 하나로 모은다.
#   이유: Claude Code 의 자동 안전 검사가 "로컬 서비스를 망에 여는" 명령을 막는다. 사용자가 이 스크립트 한 개만
#         허용 규칙에 넣어(2026-10-04 승인) 시험이 교환기를 자동으로 켜고 끌 수 있게 한다. 주소·포트는 고정이고
#         인자는 정해진 낱말만 받는다 — 허용 규칙이 다른 서비스를 여는 데 쓰이지 않게.
#   비용: 계획서의 명령(BIND=0.0.0.0 PORT=5094 nohup python3 phone_relay.py …)과 모양이 달라진다.
#   탈출구: 이 파일을 지우고 계획서의 명령을 터미널에서 직접 쓴다.
#
# 사용: phone_relay_dev.sh start | start-wrong-token | stop | restart | status
#   start              0.0.0.0:5094 로 띄운다(토큰은 server/.env.phone). 이미 떠 있으면 그대로 둔다.
#   start-wrong-token  틀린 토큰으로 다시 띄운다(기기 쪽 "토큰 거절(401)" 시험용). 떠 있던 것은 먼저 끈다.
#   stop               끈다.
#   restart            끄고 올바른 토큰으로 다시 띄운다.
#   status             떠 있는지와 /healthz 응답.
# 끝나는 값: 0 성공 · 1 실패(안 뜸, 안 꺼짐) · 2 인자가 틀림.
set -u

readonly RELAY_PORT=5094      # 이름을 BIND·PORT 로 두면 아래의 "BIND=… PORT=… python3" 환경 지정이 readonly 오류로 무시된다
readonly RELAY_BIND=0.0.0.0
readonly LOG=/tmp/phone_relay_dev.log
readonly SERVER_DIR="$(cd "$(dirname "$0")/.." && pwd)"
readonly WRONG_TOKENS="tab5-1:wrong-token-aaaaaaaaaaaa,tab5-2:wrong-token-bbbbbbbbbbbb"

listening() { lsof -nP -iTCP:"$RELAY_PORT" -sTCP:LISTEN >/dev/null 2>&1; }
health()    { curl -s -m 2 "http://127.0.0.1:$RELAY_PORT/healthz"; }

stop_relay() {
    pkill -f "phone_relay\.py" 2>/dev/null
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        listening || { echo "relay: stopped"; return 0; }
        sleep 0.5
    done
    echo "relay: 포트 $RELAY_PORT 가 아직 열려 있음(끄지 못함)" >&2
    return 1
}

# $1 = "wrong" 이면 틀린 토큰으로
start_relay() {
    cd "$SERVER_DIR" || { echo "relay: $SERVER_DIR 로 못 감" >&2; return 1; }
    if [ "${1:-}" = "wrong" ]; then
        PHONE_TOKENS="$WRONG_TOKENS" BIND="$RELAY_BIND" PORT="$RELAY_PORT" nohup python3 phone_relay.py > "$LOG" 2>&1 &
    else
        BIND="$RELAY_BIND" PORT="$RELAY_PORT" nohup python3 phone_relay.py > "$LOG" 2>&1 &
    fi
    disown 2>/dev/null
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        if [ "$(health)" = '{"ok": true}' ]; then
            echo "relay: started on $RELAY_BIND:$RELAY_PORT${1:+ (wrong tokens)}"
            return 0
        fi
        sleep 0.5
    done
    echo "relay: 5초 안에 뜨지 않음 — 로그 $LOG" >&2
    return 1
}

case "${1:-}" in
    start)
        [ $# -eq 1 ] || { echo "usage: $0 start|start-wrong-token|stop|restart|status" >&2; exit 2; }
        if listening; then echo "relay: already running on port $RELAY_PORT"; exit 0; fi
        start_relay ;;
    start-wrong-token)
        [ $# -eq 1 ] || { echo "usage: $0 start|start-wrong-token|stop|restart|status" >&2; exit 2; }
        if listening; then stop_relay || exit 1; fi
        start_relay wrong ;;
    stop)
        [ $# -eq 1 ] || { echo "usage: $0 start|start-wrong-token|stop|restart|status" >&2; exit 2; }
        stop_relay ;;
    restart)
        [ $# -eq 1 ] || { echo "usage: $0 start|start-wrong-token|stop|restart|status" >&2; exit 2; }
        if listening; then stop_relay || exit 1; fi
        start_relay ;;
    status)
        [ $# -eq 1 ] || { echo "usage: $0 start|start-wrong-token|stop|restart|status" >&2; exit 2; }
        if listening; then echo "relay: listening on port $RELAY_PORT, healthz=$(health)"; else echo "relay: not running"; fi ;;
    *)
        echo "usage: $0 start|start-wrong-token|stop|restart|status" >&2
        exit 2 ;;
esac
