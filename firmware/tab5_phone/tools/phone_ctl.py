#!/usr/bin/env python3
r"""phone_ctl.py — tab5_phone 시리얼 검증 도구. 맥에서 화면 캡처·터치 주입·상태 읽기를 자동으로 한다.
기기는 번호(=MAC)로 고른다 — 두 대가 함께 꽂혀 있어도 꽂은 순서와 무관하다. 줄 모양은 README.md "시리얼 명령"(펌웨어 쪽 정본은 main/phone_main.cpp·main/app.cpp).

  python3 phone_ctl.py port 2                    2번 기기의 포트 경로만 출력(포트를 열지 않는다. idf.py -p 에 넘길 때)
  python3 phone_ctl.py --unit 2 run <단계> …     한 기기
  python3 phone_ctl.py pair <단계> …             두 기기를 함께 열고, 단계 앞에 기기 번호를 붙인다
  끝나는 값: 0 = 모든 단계 통과, 1 = 어느 단계가 실패(또는 포트 오류), 2 = 단계 글이 틀림(포트를 열기 전에 알린다)

단계 (정규식에 ^ * | 가 들어가므로 단계마다 작은따옴표로 감싼다)
  until:<명령>:<정규식>:<초>   명령을 2초마다 다시 보내며 정규식에 맞는 응답을 기다린다(부팅 중이든 이미 켜져 있든 통한다)
  wait:<정규식>:<초>           로그에 정규식에 맞는 줄이 나올 때까지 기다린다(없으면 실패). 이미 지나간 줄도 찾는다
  sleep:<초>                   로그를 보며 쉰다
  tap:<x>:<y>                  터치 주입: 't x y' 를 보내고 'OK t x y' 를 기다린 뒤 0.5초 쉰다(기기의 주입 대기열은 8칸 — 너무 빨리 보내면 가득 차 버려진다)
  shot:<파일>                  화면을 JPEG 파일로 저장. 크기·검사합이 어긋나면 3회까지 다시 받는다(비밀번호가 든 자판 화면은 기기가 거절 — 재시도 없이 실패)
  그 외                        그대로 한 줄 명령으로 보낸다 (stat, reboot …)

pair 의 접두사
  1:<단계>  2:<단계>           그 기기에만
  both:<단계>                  두 기기 모두. until·wait·tap 은 둘 다 맞아야 통과. both:shot 은 안 된다(같은 파일에 쓰게 된다)
  sleep:<초>                   접두사 없이 쓴다. 다른 단계에 접두사가 없으면 실행 전에 오류로 알린다
  출력은 줄 앞에 [1]·[2] 가 붙는다. 한 기기를 기다리는 동안에도 다른 기기의 로그를 계속 읽는다.

지나간 줄을 놓치지 않는 방법 (PC-D1)
  기기마다 "아직 소비하지 않은 줄" 대기열이 있다. 읽은 줄은 화면에 찍고 대기열에 쌓아 둔다.
  - wait      대기열 맨 앞부터 찾는다. 찾으면 그 줄과 그 앞의 줄이 소비된다 → 같은 기기의 wait 는 줄이 나오는 순서대로 쓴다.
  - until     명령을 보낸 뒤에 온 줄만 본다(묵은 응답에 속지 않게). 찾으면 그 앞의 줄까지 모두 소비 = "여기서부터 새로" 맞춤점.
  - tap·shot  자기 응답 줄(OK t, SHOT·B64)만 꺼내고 나머지는 남긴다 → 그 사이에 나온 FSM 줄을 뒤의 wait 가 찾는다.

예시 — 1번이 걸기 → 2번 벨 → 2번이 받기 → 1번이 끊기 (좌표는 main/ui_layout.h 의 BTN_* 가운데)
  python3 phone_ctl.py pair \
    'both:until:stat:^STAT .*fsm=IDLE.* peer=1:60' \
    '1:tap:368:472' \
    '2:wait:^FSM IDLE -> INCOMING:10' \
    '1:wait:^FSM DIALING -> RINGBACK:10' \
    '2:tap:404:428' \
    'both:wait:^FSM .* -> IN_CALL:10' \
    '1:shot:/tmp/phone-1-in-call.jpg' \
    '2:shot:/tmp/phone-2-in-call.jpg' \
    'both:until:stat:^STAT .*fsm=IN_CALL.*screen=IN_CALL:10' \
    '1:tap:644:606' \
    '1:wait:^FSM ENDING -> IDLE:10' \
    '2:wait:^FSM IN_CALL -> IDLE:10' \
    'both:until:stat:^STAT .*fsm=IDLE.*screen=IDLE.*missed=0:10'
  순서대로: 둘 다 대기+상대 보임 / 1번의 왼쪽 통화 키캡(368,472) / 2번에 벨 / 1번에 연결음 / 2번 받기(404,428) / 둘 다 통화 중 /
  통화 화면 캡처 두 장 / 캡처 뒤에도 통화 중인지 / 1번 끊기(644,606) / 1번·2번 대기로 / 부재중 0건.
  통화 키캡은 둘이다(전화기 3대): 왼쪽 368,472 = slot 0(번호가 작은 상대 — Tab5 두 대는 서로에게 이 자리), 오른쪽 912,472 = slot 1(Tab5 에게는 3번 스택짱).
  벨 화면이 대기로 바뀐 직후(상대가 끊음·무응답)의 통화 키캡 탭 앞에는 sleep:2 — 그 경우만 화면 가드가 1.5초다(통화 키캡이 받기·거절 자리와 겹친다. main/ui_layout.h UL-D7).
  STAT 의 peer= 는 slot 0, 줄 끝의 peer1= 은 slot 1 의 상대가 켜져 있나, call_peer= 는 지금·직전 통화의 상대 slot 이다.
    정규식에는 ' peer=1' 처럼 앞에 빈칸을 둔다 — 빈칸 없이 'peer=1' 로 쓰면 줄 끝의 'call_peer=1' 에도 맞아 slot 0 이 꺼져 있어도 통과한다.
  거절은 2번에 tap:876:428, 걸다가 취소는 1번에 tap:640:604, 한 기기만 볼 때는 --unit 2 run 'until:stat:^STAT :30' 'shot:/tmp/idle.jpg'.
  벨 끄기는 받는 쪽에 tap:404:624, 메시지로 거절은 tap:876:624 → 고르기 화면(SCREEN DECLINE)에서 문구 1·2·3 = tap:240:168·tap:640:168·tap:1040:168, 뒤로 = tap:404:624. 화면이 바뀐 뒤의 탭 앞에는 sleep:1(화면 가드 0.6초).
  음량(대기·통화 화면의 오른쪽 줄): 크게 = tap:1168:312, 작게 = tap:1168:608 → 기기가 'VOLSTEP step=<1..5> max=5 vol=<0..100> saved=<0|1>' 한 줄로 답한다.
  같은 길을 타는 명령: volup · voldn · volstep <1..5> · volstep(지금 값). 저장은 마지막으로 바뀐 3초 뒤·통화 밖에서 → 'wait:^VOLSTEP step=3 .*saved=1:10'.
  selftest·vping 의 판정(ok=·rt_ms)은 맨 위 단계(volstep 5 = vol 100)에서만 유효하다. 재는 절차(늘 이 순서로):
    'volstep' 으로 지금 단계 <n> 을 읽어 둔다 → 'volstep 5' → selftest(또는 vping) → 'volstep <n>' 으로 되돌린다
    (스피커가 켜져 있으면 그동안 소리가 커진다 → 'quiet on' 인 채로. vping 은 두 기기 모두 맨 위 단계여야 한다 — 상대가 줄여 둔 것은 이쪽 기기의 줄에 보이지 않는다.)
  줄여 둔 기기의 'SELFTEST … ok=0' 과 그 뒤의 'TUNE … meas=0 … vol=<100 아님>' 은 고장이 아니다 — 'ok=1' 만 기다리는 단계는 위 절차 없이는 실패한다.

주의
  - 포트를 열면 기기가 한 번 리셋될 수 있다 → 시나리오 하나는 한 번의 실행 안에서 끝내고, 첫 단계는 until 로 맞춘다.
  - '-> IDLE' 만 기다리면 부팅 때의 'FSM OFFLINE -> IDLE' 에도 맞는다 → 출발 상태까지 적는다(기기가 도중에 죽었다 살아나도 통과하지 않게).
  - shot 은 십수 초 걸릴 수 있다(115200bps 기준 추정). 벨은 30초면 교환기가 끊으므로 벨 화면 캡처는 한 장만.
  - 이 도구는 토큰·Wi-Fi 비밀번호를 다루지 않는다(규약상 기기도 어떤 줄에도 싣지 않는다).
"""
import argparse
import base64
import re
import sys
import time

UNITS = {"1": "AA:BB:CC:DD:EE:01",       # 1번 전화기 (USB 일련번호 = MAC)
         "2": "AA:BB:CC:DD:EE:02"}       # 2번 전화기
SCREEN_W, SCREEN_H = 1280, 720           # 가로, setRotation(3). 터치 좌표도 이 좌표계
RESEND_S = 2.0                           # until 이 명령을 다시 보내는 간격
TAP_ACK_S = 3.0                          # 'OK t' 를 기다리는 한도
TAP_GAP_S = 0.5                          # 터치 주입 뒤 쉬는 시간 — 주입 대기열이 8칸뿐이라 쉬지 않고 보내면 가득 찬다
SHOT_RETRIES = 3                         # PC-D4: 처음 1번 + 다시 3번 = 최대 4번 받는다("최대 3회 다시 받는다"를 글자대로).
#                                          비용: 계속 실패하면 한 장에 최대 4분. 탈출구: 이 숫자
SHOT_START_S = 10.0                      # shot 을 보낸 뒤 SHOT BEGIN 이 이 안에 안 오면 그 시도는 실패(응답 없음)
SHOT_TOTAL_S = 60.0                      # 한 번의 전송 한도


# ── 기기 번호 → 포트 ─────────────────────────────────────────────────────────
def list_usb_ports():
    """맥에 보이는 직렬 포트 목록(포트를 열지 않는다). pyserial 은 쓸 때만 불러온다 — 순수 로직 시험에는 필요 없다."""
    from serial.tools import list_ports
    return list_ports.comports()


def find_port(unit, ports=None):
    """기기 번호 → 시리얼 포트 경로. USB 일련번호(=MAC)로 찾으므로 꽂은 순서와 무관하다. 포트를 열지 않는다."""
    if unit not in UNITS:
        sys.exit(f"모르는 기기 번호: {unit} (1 또는 2)")
    if ports is None:
        ports = list_usb_ports()
    for p in ports:
        if (p.serial_number or "").upper() == UNITS[unit]:
            return p.device
    sys.exit(f"{unit}번 기기({UNITS[unit]})가 USB 에 보이지 않음 — 전원이 켜져 있는지, 데이터 케이블인지 확인")


# ── 줄 읽기(순수 함수) ───────────────────────────────────────────────────────
_INT, _FLOAT, _RATIO = re.compile(r"-?\d+"), re.compile(r"-?\d+\.\d+"), re.compile(r"\d+/\d+")


def parse_stat(line):
    """'STAT k=v …' → dict. 숫자는 int/float, 'a/b' 는 (a, b), 그 밖(fsm=IDLE 같은 글자 값)은 문자열. STAT 줄이 아니면 None."""
    if not line.startswith("STAT "):
        return None
    out = {}
    for k, v in re.findall(r"(\w+)=(\S+)", line):
        if _INT.fullmatch(v):
            out[k] = int(v)
        elif _FLOAT.fullmatch(v):
            out[k] = float(v)
        elif _RATIO.fullmatch(v):
            out[k] = tuple(int(x) for x in v.split("/"))
        else:
            out[k] = v
    return out


def _marker_number(line, what):
    parts = line.split()
    if len(parts) != 3 or not parts[2].isascii() or not parts[2].isdigit():
        raise ValueError(f"{what} 줄이 깨짐: {line!r}")
    return int(parts[2])


def parse_shot(lines):
    """'SHOT BEGIN <바이트수>' … 'B64:…' … 'SHOT END <합>' → JPEG 바이트. 어긋나면 이유를 담은 ValueError.

    B64 가 아닌 줄(사이에 끼어든 다른 로그)은 건너뛴다. 새 SHOT BEGIN 이 오면 처음부터 다시 모은다(앞 전송의 찌꺼기 무시).
    PC-D3: B64 줄은 한 줄씩 엄격하게 푼다. 이유: 느슨하게 풀면 줄 안에 끼어든 글자가 조용히 버려지거나 데이터로 섞인다.
    비용: 규약에 없는 꾸밈(줄 끝 공백 등)이 붙으면 거절한다. 탈출구: validate=True 를 빼면 크기·검사합 검사만 남는다."""
    size, parts = None, []
    for ln in lines:
        if ln.startswith("SHOT FAIL"):
            raise ValueError(f"기기가 거절함: {ln}")
        if ln.startswith("SHOT BEGIN"):
            size, parts = _marker_number(ln, "SHOT BEGIN"), []
            if size == 0:
                raise ValueError("빈 스크린샷(0바이트)")
        elif size is None:
            continue                                    # 시작 전의 줄: 다른 로그이거나 앞 전송의 찌꺼기
        elif ln.startswith("B64:"):
            try:
                parts.append(base64.b64decode(ln[4:], validate=True))
            except ValueError:
                raise ValueError("B64 줄이 깨짐 — 다른 로그가 줄 안에 끼어들었거나 일부가 유실됨") from None
        elif ln.startswith("SHOT END"):
            want = _marker_number(ln, "SHOT END")
            data = b"".join(parts)
            if len(data) != size:
                raise ValueError(f"크기 불일치: 받은 {len(data)} / 보낸다던 {size}")
            if (sum(data) & 0xFFFFFFFF) != want:
                raise ValueError("검사합 불일치 — 전송 중 깨짐")
            return data
    if size is None:
        raise ValueError("응답 없음 — SHOT BEGIN 을 받지 못함")
    raise ValueError("끝 표시(SHOT END)를 받지 못함 — 시간 초과 또는 유실")


def shot_complete(lines):
    """스크린샷 줄 묶음이 끝났는가: 'SHOT FAIL', 또는 'SHOT BEGIN' 뒤의 'SHOT END'(BEGIN 앞의 END 는 앞 전송의 것)."""
    began = False
    for ln in lines:
        if ln.startswith("SHOT FAIL"):
            return True
        if ln.startswith("SHOT BEGIN"):
            began = True
        elif began and ln.startswith("SHOT END"):
            return True
    return False


def _is_shot_line(ln):
    return ln.startswith(("SHOT BEGIN", "SHOT END", "SHOT FAIL", "B64:"))


def split_lines(rest, data):
    """앞서 남은 조각 + 새로 읽은 바이트 → (완성된 줄들, 남은 조각). 줄 끝의 CR 은 뗀다. 줄 단위로 풀어 한글이 경계에서 깨지지 않는다."""
    *done, rest = (rest + data).split(b"\n")
    return [ln.decode("utf-8", "replace").rstrip("\r") for ln in done], rest


# ── 단계 문법(순수 함수) ─────────────────────────────────────────────────────
def _secs(text, step):
    try:
        v = float(text)
    except ValueError:
        v = -1.0
    if not 0 <= v < float("inf"):
        raise ValueError(f"초는 0 이상의 숫자여야 함: {step!r}")
    return v


def _regex(pat, step):
    try:
        re.compile(pat)
    except re.error as e:
        raise ValueError(f"정규식이 틀림({e}): {step!r}") from None
    return pat


def parse_until(step):
    """'until:<명령>:<정규식>:<초>' → (명령, 정규식, 초). 정규식 안의 ':' 는 허용(명령에는 없음)."""
    cmd, sep1, rest = step[len("until:"):].partition(":")
    pat, sep2, secs = rest.rpartition(":")
    if not (cmd and sep1 and pat and sep2):
        raise ValueError(f"until 은 until:<명령>:<정규식>:<초> 모양: {step!r}")
    return cmd, pat, _secs(secs, step)


def split_unit(step):
    """pair 단계의 기기 접두사를 뗀다: '1:tap:640:380' → ('1', 'tap:640:380'), 'both:…' → ('both', …), 없으면 (None, 원문).
    접두사는 정확히 '1:'·'2:'·'both:' 뿐 — 'sleep:1' 의 1, '3:…'·'12:…' 는 기기 번호가 아니다."""
    head, sep, rest = step.partition(":")
    if sep and (head in UNITS or head == "both"):
        return head, rest
    return None, step


def parse_step(step):
    """단계 글 한 개 → (종류, 인자). 종류: until·wait·sleep·tap·shot·send. 모양이 틀리면 ValueError."""
    if not step.strip():
        raise ValueError("빈 단계")
    if step.startswith("until:"):
        cmd, pat, secs = parse_until(step)
        return "until", (cmd, _regex(pat, step), secs)
    if step.startswith("wait:"):
        pat, sep, secs = step[len("wait:"):].rpartition(":")
        if not (pat and sep):
            raise ValueError(f"wait 는 wait:<정규식>:<초> 모양: {step!r}")
        return "wait", (_regex(pat, step), _secs(secs, step))
    if step.startswith("sleep:"):
        return "sleep", (_secs(step[len("sleep:"):], step),)
    if step.startswith("tap:"):
        m = re.fullmatch(r"tap:(\d+):(\d+)", step, re.ASCII)
        if not m:
            raise ValueError(f"tap 은 tap:<x>:<y> (0 이상의 정수) 모양: {step!r}")
        x, y = int(m[1]), int(m[2])
        if x >= SCREEN_W or y >= SCREEN_H:
            raise ValueError(f"tap 좌표가 화면({SCREEN_W}×{SCREEN_H}) 밖: {step!r}")
        return "tap", (x, y)
    if step.startswith("shot:"):
        if not step[len("shot:"):]:
            raise ValueError(f"shot 은 shot:<파일> 모양: {step!r}")
        return "shot", (step[len("shot:"):],)
    return "send", (step,)


def plan_steps(steps, pair, unit=None):
    """단계 글 목록 → [(기기들, 종류, 인자)]. 포트를 열기 전에 전부 확인한다 — 틀린 단계 하나 때문에 시나리오가 중간에 멈추지 않게.

    PC-D2: pair 에서 접두사 없는 단계는 sleep 만 받는다. 이유: 'stat' 을 누구에게 보낼지, 없는 번호 '3:stat' 을 그대로 보낼지
    짐작하면 엉뚱한 기기를 건드린다. 비용: 양쪽에 보낼 때도 both: 를 적어야 한다. 탈출구: 아래 검사를 "없으면 both" 로 바꾼다."""
    if not steps:
        raise ValueError("단계가 없음")
    plan = []
    for step in steps:
        who, rest = split_unit(step)
        if not pair and who is not None:
            raise ValueError(f"run 에서는 기기를 --unit 으로 고른다(단계에 접두사를 붙이지 않는다): {step!r}")
        kind, args = parse_step(rest)
        if not pair:
            plan.append(((unit,), kind, args))
            continue
        if who is None and kind != "sleep":
            raise ValueError(f"pair 단계에는 기기 번호가 필요(1: 2: both:): {step!r}")
        if who == "both" and kind == "shot":
            raise ValueError(f"both:shot 은 두 기기가 같은 파일에 쓰게 된다 — 1:shot:<파일> 2:shot:<파일> 로 나눈다: {step!r}")
        plan.append(((who,) if who in UNITS else tuple(UNITS), kind, args))
    return plan


# ── 줄 대기열(직렬 포트를 모른다 — 줄은 feed 로 넣는다) ───────────────────────
class LineQueue:
    """한 기기의 "아직 소비하지 않은 줄"(도착 순서).

    PC-D1: 읽은 줄을 버리지 않고 쌓아 두었다가, 기다리는 단계가 앞에서부터 찾아 소비한다.
    이유: 두 기기를 번갈아 다루면 한 기기의 단계를 처리하는 사이에 다른 기기의 줄이 지나간다(예: 1:tap 직후 2:wait).
    비용: 아무도 기다리지 않는 줄이 실행 내내 남는다(메모리, 그리고 넓은 정규식이 묵은 줄에 맞을 위험) → until 이 맞춤점 구실을 한다.
    탈출구: take 의 through 기본값을 바꾸면 "찾은 줄만 꺼내기"가 된다."""

    def __init__(self):
        self.lines = []

    def feed(self, line):
        self.lines.append(line)

    def mark(self):
        """지금 위치. 명령을 보내기 직전에 찍어 두면 "보낸 뒤에 온 줄"만 볼 수 있다."""
        return len(self.lines)

    def take(self, pattern, start=0, through=True):
        """start 번째부터 정규식에 맞는 첫 줄을 찾는다. 찾으면 through=True: 그 줄까지 전부 소비 / False: 그 줄만 꺼냄. 없으면 None."""
        rx = re.compile(pattern)
        for i in range(start, len(self.lines)):
            if rx.search(self.lines[i]):
                line = self.lines[i]
                del self.lines[(0 if through else i):i + 1]
                return line
        return None

    def pull(self, pred, start=0):
        """start 번째부터 pred 에 맞는 줄을 모두 꺼낸다(나머지는 그대로 남긴다)."""
        got = [ln for ln in self.lines[start:] if pred(ln)]
        if got:
            self.lines[start:] = [ln for ln in self.lines[start:] if not pred(ln)]
        return got


def _say(text):
    print(text, flush=True)


class Hub:
    """기기들의 링크와 대기열 묶음. 링크는 read() -> bytes(잠깐 기다렸다 온 만큼), write(bytes) 만 있으면 된다 — 시험에서는 가짜를 넣는다.
    어느 기기를 기다리든 poll() 이 모든 링크를 읽으므로 다른 기기의 로그가 포트에 쌓이지 않는다."""

    def __init__(self, links, clock=None, out=None):
        self.links = dict(links)
        self.queues = {u: LineQueue() for u in self.links}
        self.rest = {u: b"" for u in self.links}
        self.tagged = len(self.links) > 1               # 두 대일 때만 줄 앞에 [1]·[2]
        self.clock = clock or time.monotonic
        self.out = out or _say

    def note(self, sign, unit, text):
        self.out(f"{sign} [{unit}] {text}" if self.tagged else f"{sign} {text}")

    def poll(self):
        """모든 기기에서 한 번씩 읽어 줄 단위로 대기열에 넣고 화면에 찍는다(B64 줄은 찍지 않는다)."""
        for u, link in self.links.items():
            lines, self.rest[u] = split_lines(self.rest[u], link.read())
            for ln in lines:
                if not ln:
                    continue
                self.queues[u].feed(ln)
                if not ln.startswith("B64:"):
                    self.out(f"[{u}] {ln}" if self.tagged else ln)

    def send(self, unit, cmd, quiet=False):
        if not quiet:
            self.note(">>", unit, cmd)
        self.links[unit].write((cmd + "\n").encode())

    def pump(self, secs):
        end = self.clock() + secs
        while self.clock() < end:
            self.poll()

    def expect(self, units, pattern, secs, marks=None, through=True, resend=None):
        """units 각각의 대기열에서 정규식에 맞는 줄을 찾는다(대기열부터 훑고, 없으면 읽으며 기다린다). → {기기: 줄} — 못 찾은 기기는 빠진다.
        marks 가 있으면 그 위치 뒤의 줄만 본다. resend 가 있으면 아직 못 찾은 기기에 2초마다 다시 보낸다."""
        pos = dict(marks) if marks else {u: 0 for u in units}
        found, t0 = {}, self.clock()
        next_send = t0 + RESEND_S
        while True:
            for u in units:
                if u not in found:
                    line = self.queues[u].take(pattern, pos[u], through)
                    if line is None:
                        pos[u] = self.queues[u].mark()          # 훑은 데까지는 다시 보지 않는다
                    else:
                        found[u] = line
            now = self.clock()
            if len(found) == len(units) or now - t0 >= secs:
                return found
            if resend and now >= next_send:
                for u in units:
                    if u not in found:
                        self.send(u, resend, quiet=True)
                next_send = now + RESEND_S
            self.poll()

    def _passed(self, units, found, text):
        for u in units:
            if u not in found:
                self.note("!!", u, text)
        return len(found) == len(units)

    def _send_marked(self, units, cmd):
        """이미 와 있던 줄을 먼저 읽어 "보내기 전"으로 치고, 위치를 찍은 뒤 보낸다."""
        self.poll()
        marks = {u: self.queues[u].mark() for u in units}
        for u in units:
            self.send(u, cmd)
        return marks

    def wait(self, units, pattern, secs):
        return self._passed(units, self.expect(units, pattern, secs), f"wait 실패: {pattern} ({secs:g}초 안에 나오지 않음)")

    def until(self, units, cmd, pattern, secs):
        marks = self._send_marked(units, cmd)
        found = self.expect(units, pattern, secs, marks, resend=cmd)
        return self._passed(units, found, f"until 실패: {cmd} → {pattern} ({secs:g}초 동안 다시 보냈지만 응답 없음)")

    def tap(self, units, x, y):
        marks = self._send_marked(units, f"t {x} {y}")           # 한 번만 보낸다 — 다시 보내면 두 번 눌릴 수 있다
        found = self.expect(units, rf"^OK t {x} {y}\s*$", TAP_ACK_S, marks, through=False)
        ok = self._passed(units, found, f"tap 실패: 't {x} {y}' 에 'OK t {x} {y}' 응답이 없음")
        if ok:
            self.pump(TAP_GAP_S)
        return ok

    def shot(self, unit, path):
        q, tries = self.queues[unit], SHOT_RETRIES + 1
        for n in range(1, tries + 1):
            mark, t0, got = self._send_marked((unit,), "shot")[unit], self.clock(), []
            while not shot_complete(got):
                waited = self.clock() - t0
                if waited >= SHOT_TOTAL_S or (waited >= SHOT_START_S and not any(ln.startswith("SHOT BEGIN") for ln in got)):
                    break
                self.poll()
                got += q.pull(_is_shot_line, mark)               # 스크린샷 줄만 꺼낸다 — 사이에 낀 다른 로그는 대기열에 남는다
            if any(ln.strip() == "SHOT FAIL private" for ln in got):
                # I-1: 비밀번호가 든 자판 화면이라 기기가 거절한 것 — 다시 받아도 같은 답이므로 재시도하지 않고 단계를 실패로 끝낸다.
                self.note("!!", unit, "shot 거절(SHOT FAIL private): 비밀번호가 든 자판 화면은 찍지 않는다 — 다시 받지 않음")
                return False
            try:
                data = parse_shot(got)
            except ValueError as e:
                self.note("!!", unit, f"shot 실패 {n}/{tries}: {e}")
                continue
            try:
                with open(path, "wb") as f:
                    f.write(data)
            except OSError as e:
                self.note("!!", unit, f"shot 저장 실패: {e}")
                return False
            self.note("==", unit, f"저장: {path} ({len(data)} B, {n}번째 전송, {self.clock() - t0:.1f}초)")
            return True
        return False


def run_steps(hub, plan):
    """plan_steps 가 만든 단계를 차례로 실행한다. 한 단계라도 실패하면 거기서 멈추고 False."""
    for units, kind, args in plan:
        if kind == "until":
            ok = hub.until(units, *args)
        elif kind == "wait":
            ok = hub.wait(units, *args)
        elif kind == "tap":
            ok = hub.tap(units, *args)
        elif kind == "shot":
            ok = hub.shot(units[0], *args)
        elif kind == "sleep":
            hub.pump(*args)
            ok = True
        else:
            for u in units:
                hub.send(u, *args)
            ok = True
        if not ok:
            return False
    hub.pump(0.5)
    return True


# ── 실제 포트(시험에서는 쓰지 않는다) ────────────────────────────────────────
class SerialLink:
    """실제 직렬 포트 한 개. 이 파일에서 포트를 여는 곳은 여기뿐이다. DTR 은 기본값(켜짐)으로 둔다."""

    def __init__(self, device):
        import serial
        self.s = serial.Serial(device, 115200, timeout=0.05)

    def read(self):
        return self.s.read(self.s.in_waiting or 1)       # 온 만큼 바로 받고, 없으면 0.05초까지만 기다린다

    def write(self, data):
        self.s.write(data)
        self.s.flush()


def open_links(units):
    ports = list_usb_ports()
    devices = {u: find_port(u, ports) for u in units}    # 모두 찾은 뒤에 연다 — 한 대라도 안 보이면 아무 포트도 열지 않는다
    return {u: SerialLink(d) for u, d in devices.items()}


def main(argv=None):
    ap = argparse.ArgumentParser(prog="phone_ctl.py", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--unit", help="기기 번호 1|2 (run 에 필요)")
    ap.add_argument("cmd", choices=["port", "run", "pair"])
    ap.add_argument("args", nargs="*", help="port: 기기 번호 / run·pair: 단계들")
    a = ap.parse_args(argv)
    if a.cmd == "port":
        print(find_port((a.args[0] if a.args else a.unit) or ""))
        return 0
    try:
        if a.cmd == "run" and a.unit not in UNITS:
            raise ValueError("run 에는 --unit 1 또는 --unit 2 가 필요")
        plan = plan_steps(a.args, pair=a.cmd == "pair", unit=a.unit)
    except ValueError as e:
        print(f"단계 오류: {e}", file=sys.stderr)
        return 2
    try:
        hub = Hub(open_links(tuple(UNITS) if a.cmd == "pair" else (a.unit,)))
        return 0 if run_steps(hub, plan) else 1
    except OSError as e:                                 # 포트를 다른 프로그램이 잡고 있거나, 도중에 USB 가 빠짐
        print(f"!! 포트 오류: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
