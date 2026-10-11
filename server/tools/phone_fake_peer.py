#!/usr/bin/env python3
"""phone_fake_peer.py — 교환기에 "기기 한 대"로 붙어 대본대로 신호를 주고받는다. 진짜 기기 한 대만으로 걸기·받기 화면을 시험할 때 상대 역할.

  python3 tools/phone_fake_peer.py tab5-1 <단계> ...            # http://127.0.0.1:5094 의 교환기에 tab5-1 로 접속(v1 = 옛 펌웨어 흉내)
  python3 tools/phone_fake_peer.py --base http://127.0.0.1:5095 tab5-1 <단계> ...
  python3 tools/phone_fake_peer.py --proto 2 stackchan-2 expect:peer:tab5-2:on send:call:tab5-2 expect:ringing ...   # v2 = 새 펌웨어 흉내
선택 인자(기기 ID 앞, 순서 무관):
  --base <주소>                         교환기 주소(기본 http://127.0.0.1:5094)
  --proto <1|2>                         신호 규약(설계서 2026-10-08-phone-3way-design.md §6, TW-D7). 2 면 접속 헤더 X-Phone-Proto: 2 를 보낸다.
                                        기본 1 = 헤더 없음 — 옛 대본이 그대로 돈다(to 없는 call 은 옛 짝에게, 상태 알림은 옛 짝 하나만)
단계:
  send:<call|accept|reject|hangup>     신호 보내기
  send:call:<기기ID>                    그 기기에게 건다({"t":"call","to":"<기기ID>"}). 등록 여부를 검사하지 않는다 — 미등록 ID 도 보낼 수 있어야 교환기를 시험한다
  send:reject:<정수>                    거절에 문구 번호(msg)를 실어 보낸다. 범위를 검사하지 않는다 — 틀린 번호(0, 100, -1)도 보낼 수 있어야 교환기를 시험한다
  expect:<t>[:<why>]                    그 신호가 올 때까지 기다림(기본 10초). 예: expect:ring, expect:ended:hangup, expect:ended:busy, expect:peer
  expect:ring:<기기ID>                  그 기기가 건 벨(from 이 그 ID)을 기다림. from 은 --proto 2 연결에만 실려 온다
  expect:peer:<기기ID>:<on|off>         그 기기가 켜짐/꺼짐이라는 상태 알림을 기다림. 기기 ID 가 실린 알림은 --proto 2 연결에만 온다
  expect:ended:reject:<정수>            msg 가 그 수와 같은 거절 종료를 기다림. 0 은 "msg 가 없어야 한다". 번호를 안 쓰면 msg 를 보지 않는다
  expect:<…>@<초>                       기다리는 시간 지정(맨 뒤). 예: expect:ended:timeout@40, expect:ended:reject:2@15, expect:ring:tab5-1@5
  sleep:<초>                            기다림(그동안 온 신호는 출력하고 버린다)
  voice:<프레임수>                      20ms 간격으로 음성 흉내 프레임([순번 2][0 × 60])을 보낸다(Phase 3 시험용)
기대는 **온 순서대로** 소비된다: expect 는 맞는 신호가 올 때까지 읽으며, 그 사이에 온 다른 신호는 출력하고 버린다(뒤의 expect 가 다시 보지 못한다).
  --proto 2 연결은 접속 직후 다른 기기 전부의 상태 알림을 **등록 순서**(PHONE_TOKENS 에 적힌 순서)로 받는다 — expect:peer 를 여럿 쓸 때는 그 순서로 적는다.
연결을 유지하는 동안 10초마다 ping 을 보낸다(교환기는 45초 무소식이면 끊는다). 토큰은 server/.env.phone 에서 읽고 출력하지 않는다.

주의: 같은 ID 의 진짜 기기가 붙어 있으면 그 기기가 밀려난다 — 흉내 낼 번호의 기기는 꺼 두거나 아직 전화기 펌웨어를 올리지 않은 상태여야 한다.
주의: send:call(to 없음)과 send:call:<기기ID> 는 그 교환기에 붙어 있는 **진짜 기기의 벨을 울린다**. 사람이 있는 곳의 기기(집의 1번)를 부르지 않게 대상을 확인한다.
"""
import asyncio
import json
import os
import re
import sys
import time
from pathlib import Path

import aiohttp

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import phone_core
import phone_relay

SIGNALS = ("call", "accept", "reject", "hangup")
DEFAULT_BASE = "http://127.0.0.1:5094"
# 대본에 쓰는 기기 ID 의 모양: 영문자로 시작하는 영문·숫자·-·_ (tab5-1, stackchan-2).
#   이유: 숫자만 쓴 값(send:call:2)은 문구 번호를 call 에 잘못 붙인 것일 가능성이 높다 — 지금까지도 단계 오류였다. 빈 값(셸 변수가 비어 `expect:ring:` 이 된 경우)을
#         "아무 벨이나"로 받으면 시험이 거짓 통과한다. 비용: 숫자로 시작하는 ID 는 대본에 못 쓴다. 탈출구: 이 식을 넓힌다(등록 여부는 원래 검사하지 않는다).
_DEV_ID = re.compile(r"[A-Za-z][A-Za-z0-9_-]*", re.ASCII)


def parse_step(step):
    """단계 글자 → (종류, 인자…). 잘못된 단계는 ValueError.
    send   → ("send", 신호, 덧붙는 값 또는 None)                 덧붙는 값: reject = 문구 번호(정수), call = 거는 상대의 기기 ID(글자)
    expect → ("expect", 신호, 조건1 또는 None, 조건2 또는 None, 초)   조건의 뜻은 matches() 참고"""
    kind, _, rest = step.partition(":")
    if kind == "send":
        sig, sep, arg = rest.partition(":")
        if sig not in SIGNALS:
            raise ValueError(f"send 는 {SIGNALS} 중 하나: {step}")
        if not sep:
            return ("send", sig, None)
        if sig == "reject":
            return ("send", sig, _msg_num(arg, step))
        if sig == "call":
            return ("send", sig, _dev_id(arg, step))
        raise ValueError(f"뒤에 붙는 값은 reject(문구 번호)와 call(기기 ID)에만: {step}")
    if kind == "expect":
        body, _, secs = rest.partition("@")
        secs = float(secs) if secs else 10.0
        t, has_tail, tail = body.partition(":")
        if not t:
            raise ValueError(f"expect 에 신호 이름이 없음: {step}")
        if t == "ring" and has_tail:                           # expect:ring:<건 기기ID>
            return ("expect", t, _dev_id(tail, step), None, secs)
        if t == "peer" and has_tail:                           # expect:peer:<기기ID>:<on|off>
            dev, _, state = tail.partition(":")
            if state not in ("on", "off"):
                raise ValueError(f"expect:peer:<기기ID>:<on|off> 모양이어야 함: {step}")
            return ("expect", t, _dev_id(dev, step), state == "on", secs)
        why, sep, num = tail.partition(":")
        if sep and (t, why) != ("ended", "reject"):
            raise ValueError(f"문구 번호는 expect:ended:reject 에만 붙인다: {step}")
        return ("expect", t, why or None, _msg_num(num, step) if sep else None, secs)
    if kind == "sleep":
        return ("sleep", float(rest))
    if kind == "voice":
        return ("voice", int(rest))
    raise ValueError(f"모르는 단계: {step}")


def _msg_num(text, step):
    """문구 번호 글자 → 정수. 정수가 아니면 단계 오류(범위는 보지 않는다)."""
    if not re.fullmatch(r"-?\d+", text, re.ASCII):
        raise ValueError(f"문구 번호는 정수: {step}")
    return int(text)


def _dev_id(text, step):
    """대본의 기기 ID 글자를 검사해 그대로 돌려준다(모양만 본다 — 등록 여부는 교환기가 판단한다)."""
    if not _DEV_ID.fullmatch(text):
        raise ValueError(f"기기 ID 는 영문자로 시작하는 영문·숫자·-·_ : {step}")
    return text


def frame_of(sig, arg):
    """보낼 신호 프레임. 덧붙는 값은 call 이면 거는 상대(to), reject 면 문구 번호(msg). 없으면 옛 모양 그대로."""
    if arg is None:
        return {"t": sig}
    return {"t": sig, "to": arg} if sig == "call" else {"t": sig, "msg": arg}


def needs_v2(step):
    """v2 연결에서만 만족할 수 있는 기대인가 — 건 기기(from)·기기 ID 가 실린 상태 알림(id)은 v1 연결에 오지 않는다(§6.3)."""
    return step[0] == "expect" and step[1] in ("ring", "peer") and step[2] is not None


def matches(msg, t, why, num=None):
    """받은 신호가 기다리던 것인가. 조건 둘의 뜻은 신호마다 다르다(None 이면 그 조건을 보지 않는다):
      ended  why = 종료 사유, num = 거절 문구 번호 — None = msg 를 보지 않음(지금까지의 동작), 0 = msg 가 없어야 함, 그 밖 = msg 가 그 수와 같아야 함
      ring   why = 건 기기 ID(from 과 같아야 함)
      peer   why = 기기 ID(id 와 같아야 함), num = True/False(online 과 같아야 함)
    DM-D14: 판정이 느슨하면(번호·건 기기를 안 보면) 번호가 빠진 거절·엉뚱한 기기의 벨에도 기기 시험이 통과해 버린다."""
    if not (isinstance(msg, dict) and msg.get("t") == t):
        return False
    if t == "ring":
        return why is None or msg.get("from") == why
    if t == "peer":
        return (why is None or msg.get("id") == why) and (num is None or msg.get("online") is num)
    if not (why is None or msg.get("why") == why):
        return False
    if num is None:
        return True
    if num == 0:
        return "msg" not in msg
    got = msg.get("msg")
    return isinstance(got, int) and not isinstance(got, bool) and got == num


async def run(base, dev, steps, out=print, proto=1):
    phone_relay.load_env_file(Path(__file__).resolve().parent.parent / ".env.phone")
    tokens = phone_core.parse_tokens(os.environ.get("PHONE_TOKENS", ""))
    if dev not in tokens:
        raise ValueError(f"등록되지 않은 기기 ID: {dev}")
    plan = [parse_step(s) for s in steps]
    if proto != 2:
        late = [s for s, st in zip(steps, plan) if needs_v2(st)]
        if late:                                               # 붙기 전에 멈춘다 — 붙은 뒤라면 10초를 기다린 끝에 "오지 않음"으로만 보인다
            raise ValueError(f"--proto 2 가 필요한 단계(건 기기·기기 ID 는 v2 연결에만 실려 온다): {' '.join(late)}")
    headers = {"X-Device-Id": dev, "X-Device-Token": tokens[dev]}
    if proto == 2:
        headers["X-Phone-Proto"] = "2"                         # §6.1: 이 헤더가 정확히 "2" 일 때만 v2 연결
    t0 = time.monotonic()
    stamp = lambda: f"{time.monotonic() - t0:6.2f}s"
    last_state = {}                                            # 기기 ID → 마지막으로 받은 켜짐/꺼짐. 판정에는 쓰지 않는다 — 시간 초과 때 까닭을 짚어 주는 데만

    async with aiohttp.ClientSession() as s:
        async with s.ws_connect(base.rstrip("/") + "/ws", headers=headers) as ws:
            out(f"{stamp()} 접속됨 ({dev}, 규약 v{proto})")

            async def pinger():
                n = 0
                while True:
                    await asyncio.sleep(10)
                    n += 1
                    await ws.send_json({"t": "ping", "n": n})

            async def recv(deadline):
                """다음 신호 하나(pong 은 건너뜀). 시간이 다 되면 None, 연결이 닫히면 ConnectionError."""
                while True:
                    left = deadline - time.monotonic()
                    if left <= 0:
                        return None
                    try:
                        m = await ws.receive(timeout=left)
                    except asyncio.TimeoutError:
                        return None
                    if m.type == aiohttp.WSMsgType.TEXT:
                        msg = json.loads(m.data)
                        if msg.get("t") == "pong":
                            continue
                        out(f"{stamp()} ← {m.data}")
                        if msg.get("t") == "peer" and isinstance(msg.get("id"), str):
                            last_state[msg["id"]] = msg.get("online")
                        return msg
                    if m.type == aiohttp.WSMsgType.BINARY:
                        continue                                   # 상대가 보낸 음성 — 내용은 보지 않는다
                    raise ConnectionError(f"교환기가 연결을 닫음({m.type.name})")

            ping_task = asyncio.create_task(pinger())
            try:
                for st in plan:
                    if st[0] == "send":
                        await ws.send_json(frame_of(st[1], st[2]))
                        out(f"{stamp()} → {st[1]}" + ("" if st[2] is None else f" {'to' if st[1] == 'call' else 'msg'}={st[2]}"))
                    elif st[0] == "expect":
                        _, t, why, num, secs = st
                        deadline = time.monotonic() + secs
                        while True:
                            msg = await recv(deadline)
                            if msg is None:
                                want = t + (f":{why}" if why else "") + ("" if num is None else f":{'on' if num else 'off'}" if t == "peer" else f":{num}")
                                hint = ""
                                if t == "peer" and num is not None and last_state.get(why) is num:
                                    hint = " — 그 알림은 앞 단계를 기다리는 동안 이미 지나갔다(기대는 온 순서대로 소비된다: 접속 직후 상태 알림은 등록 순서로 오니 대본의 expect:peer 순서를 맞출 것)"
                                raise AssertionError(f"{secs:g}초 안에 {want} 가 오지 않음{hint}")
                            if matches(msg, t, why, num):
                                break
                    elif st[0] == "sleep":
                        deadline = time.monotonic() + st[1]
                        while await recv(deadline) is not None:
                            pass
                    elif st[0] == "voice":
                        for seq in range(st[1]):
                            await ws.send_bytes((seq & 0xFFFF).to_bytes(2, "big") + bytes(60))
                            await asyncio.sleep(0.02)
                        out(f"{stamp()} → 음성 흉내 {st[1]}프레임")
            finally:
                ping_task.cancel()
    out(f"{stamp()} FAKE OK")


def parse_cli(args):
    """명령줄 인자 → (교환기 주소, 규약 버전, 기기 ID, 단계들). 선택 인자(--base·--proto)는 기기 ID 앞에 어느 순서로든. 틀리면 ValueError."""
    args, base, proto = list(args), DEFAULT_BASE, 1
    while args and args[0].startswith("--"):
        opt = args.pop(0)
        if not args:
            raise ValueError(f"{opt} 뒤에 값이 없음")
        val = args.pop(0)
        if opt == "--base":
            base = val
        elif opt == "--proto":
            if val not in ("1", "2"):
                raise ValueError("--proto 는 1 또는 2")
            proto = int(val)
        else:
            raise ValueError(f"모르는 선택 인자: {opt}")
    if not args:
        raise ValueError("기기 ID 가 없음")
    return base, proto, args[0], args[1:]


def main(argv):
    try:
        base, proto, dev, steps = parse_cli(argv[1:])
    except ValueError as e:
        sys.exit(f"{__doc__}\n인자 오류: {e}")
    try:
        asyncio.run(run(base, dev, steps, proto=proto))
    except (AssertionError, ValueError, ConnectionError, aiohttp.ClientError, asyncio.TimeoutError) as e:
        sys.exit(f"FAKE FAIL: {type(e).__name__}: {e}")


if __name__ == "__main__":
    main(sys.argv)
