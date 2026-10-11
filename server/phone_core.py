"""phone_core.py — 전화 교환기의 순수 로직 (네트워크·시계 없음, pytest 로 바로 시험).

설계: docs/exec-plans/2026-10-08-phone-3way-design.md §6(신호 규약 v2)·§7 — 옛 규약(v1)과 계약은 2026-10-03-tab5-phone.md §9.3·§9.4
  - 등록된 기기 2~8대의 접속·이탈, 기기 쌍마다 독립인 통화(RINGING/IN_CALL), 통화별 30초 벨 제한, 45초 무소식 끊기, 신호 검증.
  - 시각은 호출자가 now(초, 단조 증가)로 넣는다. 결과는 "할 일" 목록으로 돌려준다:
      ("send", 기기ID, dict)   그 기기에 글자 프레임(JSON) 보내기
      ("close", conn)          그 연결 닫기
      ("log", dict)            기록 남기기 (시각·기기·통화 길이·종료 사유만 — 음성·토큰은 절대 넣지 않는다)
  - 수신거부 메시지(docs/exec-plans/2026-10-04-tab5-phone-decline-mute.md §5.2): reject 에 실린 문구 번호 msg 를
    ended 에 실어 양쪽에 건넨다. 교환기는 번호의 뜻을 모르고(DM-D1), 기록에도 넣지 않는다.

v2 에서 달라진 것(결정 TW-D7~D10):
  - TW-D7 규약 두 벌: 연결마다 proto(1 또는 2)를 기억하고, 그 연결에 보내는 프레임의 모양만 가른다(v2 = peer 에 id, ring 에 from).
          판정(누가 누구에게 걸 수 있나·busy·벨 제한)은 한 벌이다. 탈출구: 모든 기기가 v2 가 되면 PROTO_V1 갈래(_peer_frame·_reachable 의 v1 줄)를 지운다.
  - TW-D8 v1 기기는 옛 짝하고만 걸고 받는다 → _reachable() 한 곳이 "통화가 성립하는 쌍"을 정하고, 상태 알림(꺼짐으로 보임)과 걸기 판정 4번이 함께 쓴다.
  - TW-D9 벨·통화에 묶인 상대에게 걸면 ended:busy.
  - TW-D10 통화는 Call 객체(기기 쌍마다 하나). call_of[기기ID] 로 찾고 두 기기가 같은 객체를 가리킨다 — 한 기기는 많아야 한 통화에 묶인다.
"""
import hmac
import json

IDLE, RINGING, IN_CALL = "IDLE", "RINGING", "IN_CALL"
RING_TIMEOUT_S = 30.0          # 벨 제한 — 통화마다 따로 센다 (§6.4 6번)
SILENCE_TIMEOUT_S = 45.0       # 무소식 끊기 (§6.5)
MAX_VOICE_BYTES = 400          # 이보다 큰 음성 프레임은 버린다 (§9.4)
MIN_VOICE_BYTES = 3            # 순번 2바이트 + Opus 최소 1바이트
MAX_TEXT_BYTES = 256           # 신호(JSON)는 수십 바이트 — 이보다 길면 무시
MAX_PEER_BUFFER_BYTES = 4096   # PL-D3: 상대 쪽 송신 대기열이 이보다 쌓이면 음성을 버린다(약 1초 분량)
MIN_TOKEN_CHARS = 16
MIN_DEVICES, MAX_DEVICES = 2, 8   # §6.1: 등록 기기 2~8대
PROTO_V1, PROTO_V2 = 1, 2      # TW-D7: 연결의 신호 규약 버전
# DM-D6: 거절 문구 번호는 이 범위의 정수만 건넨다(뜻은 기기가 안다). 범위 밖·정수가 아닌 값은 버리지 않고 "번호 없는 거절"로 낮춘다.
#   이유: 문구를 늘려도 교환기를 고치지 않고, 번호가 틀렸다고 거절(맞는 신호)까지 무시되면 통화가 안 끝난다.
#   비용: 틀린 번호는 조용히 사라진다(bad_signal 에도 안 잡힌다). 탈출구: 이 두 값을 고치거나, _reject_msg 에서 bad_signal 을 올린다.
REJECT_MSG_MIN = 1
REJECT_MSG_MAX = 99


def parse_tokens(spec):
    """'tab5-1:토큰,tab5-2:토큰,stackchan-2:토큰' → {기기ID: 토큰}(적힌 순서 그대로). 2~8대, 토큰 16자 이상. 오류 문구에 토큰을 넣지 않는다.
    순서가 뜻을 가진다: 옛 짝의 기본값이 "적힌 순서의 앞 두 대"다(parse_legacy_pair) — 그래서 정렬하지 않는다."""
    out = {}
    for part in (spec or "").split(","):
        part = part.strip()
        if not part:
            continue
        dev, sep, tok = part.partition(":")
        dev, tok = dev.strip(), tok.strip()
        if not sep or not dev or not tok:
            raise ValueError("PHONE_TOKENS 형식 오류: '기기ID:토큰,기기ID:토큰'")
        if len(tok) < MIN_TOKEN_CHARS:
            raise ValueError(f"PHONE_TOKENS: {dev} 의 토큰이 너무 짧음({MIN_TOKEN_CHARS}자 이상)")
        if dev in out:
            raise ValueError(f"PHONE_TOKENS: 기기 ID 중복({dev})")
        out[dev] = tok
    if not MIN_DEVICES <= len(out) <= MAX_DEVICES:
        raise ValueError(f"PHONE_TOKENS: 기기는 {MIN_DEVICES}~{MAX_DEVICES}대여야 함")
    return out


def _check_legacy_pair(pair, device_ids):
    """옛 짝 = 등록된 서로 다른 기기 ID 두 개. 맞으면 튜플로 돌려준다.
    오류 문구에 받은 값을 넣지 않는다 — PHONE_TOKENS 값을 잘못 붙여 넣었다면 그 글자가 토큰이고, 이 문구는 기록으로 간다."""
    pair = tuple(pair)
    if len(pair) != 2:
        raise ValueError("PHONE_LEGACY_PAIR: 기기 ID 두 개여야 함('기기ID,기기ID')")
    if any(not isinstance(p, str) or p not in device_ids for p in pair):
        raise ValueError("PHONE_LEGACY_PAIR: 등록되지 않은 기기 ID 가 있음(PHONE_TOKENS 의 ID 와 글자까지 같아야 함)")
    if pair[0] == pair[1]:
        raise ValueError("PHONE_LEGACY_PAIR: 두 기기 ID 가 같음")
    return pair


def parse_legacy_pair(spec, device_ids):
    """'tab5-1,tab5-2' → ('tab5-1', 'tab5-2'). 비어 있으면 등록 순서의 앞 두 대(§6.1). 틀린 값(미등록 ID·같은 ID 둘·개수 틀림)은 ValueError."""
    ids = tuple(device_ids)
    if not (spec or "").strip():
        return _check_legacy_pair(ids[:2], ids)
    return _check_legacy_pair([p.strip() for p in spec.split(",")], ids)


def proto_from_header(value):
    """접속 헤더 X-Phone-Proto 의 값 → 규약 버전. 정확히 "2" 일 때만 v2, 없거나 그 밖은 v1 (§6.1).
    이유: 옛 펌웨어는 이 헤더를 보내지 않는다 — 모르는 값을 v2 로 받으면 옛 기기에 v2 모양의 프레임이 간다. 탈출구: v3 가 생기면 여기 한 곳."""
    return PROTO_V2 if value == "2" else PROTO_V1


def _bytes(s):
    """글자 → 바이트. 헤더가 깨진 UTF-8 이면 aiohttp 가 짝 없는 대리 문자로 넘긴다 — surrogatepass 로 어떤 글자든 예외 없이 바이트로 만든다."""
    return s.encode("utf-8", "surrogatepass")


def check_token(tokens, dev, token):
    """등록된 기기이고 토큰이 맞으면 True. 비교는 hmac.compare_digest (시간차 공격 방지). 글자가 아닌 값·깨진 글자에도 예외 없이 False."""
    if not isinstance(dev, str) or not isinstance(token, str):
        return False
    got = _bytes(token)
    want = tokens.get(dev)
    if want is None:
        hmac.compare_digest(got, b"\0" * len(got))   # 미등록 ID 도 같은 시간 소모
        return False
    return hmac.compare_digest(got, _bytes(want))


def should_drop_voice(peer_buffered_bytes):
    """PL-D3: 상대 연결의 송신 대기열이 밀려 있으면 True (지연이 쌓이지 않게 새 음성을 버린다)."""
    return peer_buffered_bytes > MAX_PEER_BUFFER_BYTES


class Call:
    """통화 1건(벨 울리는 중 또는 통화 중). 기기 쌍마다 하나이고, 끝나면 버린다(IDLE 인 Call 은 없다) — TW-D10, §7."""
    __slots__ = ("caller", "callee", "state", "ring_started", "ring_deadline", "call_started")

    def __init__(self, caller, callee, now):
        self.caller, self.callee = caller, callee
        self.state = RINGING
        self.ring_started, self.ring_deadline = now, now + RING_TIMEOUT_S
        self.call_started = 0.0

    def other(self, dev):
        return self.callee if dev == self.caller else self.caller


class Exchange:
    """등록 기기 2~8대 사이의 통화를 관리한다. 통화는 기기 쌍마다 독립이고 한 기기는 많아야 한 통화에 묶인다(TW-D10)."""

    def __init__(self, device_ids, legacy_pair=None):
        ids = tuple(device_ids)
        if not MIN_DEVICES <= len(ids) <= MAX_DEVICES or len(set(ids)) != len(ids):
            raise ValueError(f"기기는 서로 다른 {MIN_DEVICES}~{MAX_DEVICES}대여야 함")
        self.devices = ids         # 등록 순서 — 상태 알림을 보내는 순서이고, 옛 짝 기본값의 근거다
        # 옛 짝(§6.1): v1 연결끼리의 상대. 안 주면 등록 순서의 앞 두 대 — parse_legacy_pair("") 와 같은 규칙.
        self.legacy_pair = _check_legacy_pair(ids[:2] if legacy_pair is None else legacy_pair, ids)
        self.conns = {}            # 기기ID → 현재 연결(불투명 객체)
        self.last_seen = {}        # 기기ID → 마지막으로 프레임을 받은 시각
        self.proto = {}            # 기기ID → 지금 연결의 규약 버전(PROTO_V1 / PROTO_V2). 붙어 있는 동안만 있다
        self.call_of = {}          # 기기ID → Call. 통화의 두 기기가 같은 객체를 가리킨다
        self.stats = {"voice_fwd": 0, "voice_drop_size": 0, "voice_drop_state": 0, "voice_drop_backlog": 0, "bad_signal": 0}

    # ── 조회 ────────────────────────────────────────────────────────────────
    def legacy_peer_of(self, dev):
        """dev 의 옛 짝. 옛 짝에 들지 않는 기기(스택짱)는 None."""
        a, b = self.legacy_pair
        return b if dev == a else a if dev == b else None

    def conn_of(self, dev):
        return self.conns.get(dev)

    def is_online(self, dev):
        return dev in self.conns

    def state_of(self, dev):
        """그 기기가 묶인 통화의 상태. 묶여 있지 않으면 IDLE."""
        call = self.call_of.get(dev)
        return IDLE if call is None else call.state

    @property
    def state(self):
        """교환기 전체의 요약: 통화가 하나라도 있으면 IN_CALL, 아니고 벨이 있으면 RINGING, 없으면 IDLE.
        이유: snapshot()·시험이 쓰던 값의 모양을 유지한다(2대일 때는 옛 뜻과 글자까지 같다). 비용: 통화가 둘 이상이면 뭉뚱그려진다.
        탈출구: 통화별로 보려면 state_of(기기) 또는 call_of 를 쓴다."""
        states = {c.state for c in self.call_of.values()}
        return IN_CALL if IN_CALL in states else RINGING if RINGING in states else IDLE

    def snapshot(self):
        return {"state": self.state, "online": sorted(self.conns), "stats": dict(self.stats)}

    # ── 누가 누구를 보고, 누구와 통화할 수 있나 (§6.3·TW-D8) ────────────────
    def _reachable(self, a, b):
        """a 와 b 사이에 통화가 성립할 수 있는가: 둘 다 붙어 있고, v1 인 쪽은 상대가 자기 옛 짝이어야 한다(TW-D8).
        이 한 곳을 상태 알림("꺼짐으로 보임")과 걸기 판정 4번(offline)이 함께 쓴다 — 켜져 보이는 상대에게만 걸리고, 벨은 켜져 보이는 상대에게서만 온다.
        설계서 §6.4 4번은 받는 쪽이 v1 인 경우만 적었다. 거는 쪽이 v1 인 경우(v1 연결이 to 로 옛 짝 아닌 기기를 부름)도 여기서 같이 막는다:
          이유: TW-D8 은 "걸고 받는다" 양쪽이다. 안 막으면 v2 받는 쪽 화면에 '잠든' 캐릭터가 건 전화가 뜬다.
          비용: 가짜 기기를 --proto 2 없이 옛 짝 아닌 ID 로 붙여 to 로 걸면 offline 이 된다(진짜 v1 펌웨어는 to 를 보내지 않는다).
          탈출구: 아래 all() 을 b 쪽(받는 쪽) 검사만 남기면 §6.4 의 글자 그대로가 된다."""
        if a not in self.conns or b not in self.conns:
            return False
        return all(self.proto[x] == PROTO_V2 or self.legacy_peer_of(x) == y for x, y in ((a, b), (b, a)))

    def _hears_about(self, viewer, target):
        """viewer 연결이 target 의 상태 알림을 받는가: v2 는 다른 기기 전부, v1 은 옛 짝 하나만(§6.3)."""
        return self.proto[viewer] == PROTO_V2 or self.legacy_peer_of(viewer) == target

    def _peer_frame(self, viewer, target, online):
        """상태 알림 프레임. v2 연결에는 어느 기기인지(id)를 싣고, v1 연결에는 지금 모양 그대로(옛 짝 하나뿐이라 id 가 없다)."""
        if self.proto[viewer] == PROTO_V2:
            return {"t": "peer", "id": target, "online": online}
        return {"t": "peer", "online": online}

    def _watchers(self, dev):
        """dev 를 지금 "켜짐"으로 보고 있는 기기들(등록 순서) — dev 가 붙거나 떨어질 때 알려야 하는 상대."""
        return [v for v in self.devices if v != dev and v in self.conns and self._hears_about(v, dev) and self._reachable(v, dev)]

    # ── 접속·이탈 ───────────────────────────────────────────────────────────
    def attach(self, dev, conn, now, proto=PROTO_V1):
        if dev not in self.devices:
            raise ValueError("미등록 기기")
        if proto not in (PROTO_V1, PROTO_V2):
            raise ValueError("모르는 규약 버전")
        acts = []
        old = self.conns.get(dev)
        if old is not None:                      # 같은 ID 가 다시 붙음 → 옛 연결 정리 후 교체 (§6.5)
            # 규약이 바뀌어 다시 붙어도(v1 펌웨어 → v2 펌웨어) 따로 다루지 않는다: 떨어짐(보던 기기에 꺼짐) → 붙음(새 규약 기준으로 보이는 기기에 켜짐).
            #   이유: "누구에게 어떻게 보이나"가 _reachable 한 곳에서 다시 계산된다. 비용: 옛 짝은 꺼짐→켜짐을 연달아 받는다(지금도 재접속 때 그렇다).
            acts += self.detach(dev, old, now)
            acts.append(("close", old))
        self.conns[dev] = conn
        self.last_seen[dev] = now
        self.proto[dev] = proto
        # 접속 기록: v2 연결에만 "proto":2 를 싣는다(v1 의 줄은 지금까지와 글자가 같다). 규약 판은 비밀이 아니고, 현장에서 "새 펌웨어인데 헤더가 빠져 v1 로 붙었다"를
        # 기록으로 가릴 수 있게 한다(독립 검수 2026-10-08 Minor 1).
        acts.append(("log", {"ev": "online", "dev": dev, "proto": proto} if proto == PROTO_V2 else {"ev": "online", "dev": dev}))
        for other in self.devices:               # 새 연결에게: 들을 자격이 있는 상대 전부의 지금 상태(꺼진 것도 알린다)
            if other != dev and self._hears_about(dev, other):
                acts.append(("send", dev, self._peer_frame(dev, other, self._reachable(dev, other))))
        acts += [("send", v, self._peer_frame(v, dev, True)) for v in self._watchers(dev)]   # 다른 기기에게: 이 기기가 켜져 보이게 된 쪽만
        return acts

    def detach(self, dev, conn, now):
        if self.conns.get(dev) is not conn:      # 이미 교체된 옛 연결의 뒤늦은 이탈 → 무시
            return []
        watchers = self._watchers(dev)           # 지우기 전에 센다: 켜짐으로 알고 있던 기기에게만 꺼짐을 알린다(원래 꺼짐으로 보던 기기에는 다시 보내지 않는다)
        del self.conns[dev]
        del self.proto[dev]
        self.last_seen.pop(dev, None)
        acts = [("log", {"ev": "offline", "dev": dev})]
        call = self.call_of.get(dev)
        if call is not None:
            # PL-D2: 통화 중 이탈 = lost, 벨 울리는 중 이탈 = offline. 이 기기가 묶인 통화만 끝난다 — 다른 통화는 건드리지 않는다
            acts += self._end(call, "lost" if call.state == IN_CALL else "offline", now)
        acts += [("send", v, self._peer_frame(v, dev, False)) for v in watchers]
        return acts

    # ── 신호 ────────────────────────────────────────────────────────────────
    def on_text(self, dev, raw, now):
        if not self.is_online(dev):
            return []
        self.last_seen[dev] = now                # 어떤 프레임이든 "소식"으로 친다
        msg = self._parse(raw)
        if msg is None:
            self.stats["bad_signal"] += 1
            return []
        t = msg["t"]
        if t == "ping":
            n = msg.get("n")
            if isinstance(n, bool) or not isinstance(n, int):
                self.stats["bad_signal"] += 1
                return []
            return [("send", dev, {"t": "pong", "n": n})]
        if t == "call":
            return self._on_call(dev, msg, now)
        call = self.call_of.get(dev)             # accept·reject·hangup 은 그 기기가 묶인 통화에만 닿는다(남의 통화는 못 건드린다)
        if t == "accept":
            if call is not None and call.state == RINGING and dev == call.callee:
                return self._connect(call, now)
        elif t == "reject":
            if call is not None and call.state == RINGING and dev == call.callee:
                return self._end(call, "reject", now, self._reject_msg(msg))
        elif t == "hangup":
            if call is not None:
                return self._end(call, "hangup", now)
        else:
            self.stats["bad_signal"] += 1
        return []                                # 상태에 안 맞는 신호는 조용히 무시

    @staticmethod
    def _parse(raw):
        if not isinstance(raw, str) or len(raw.encode("utf-8", "replace")) > MAX_TEXT_BYTES:
            return None
        try:
            msg = json.loads(raw)
        except ValueError:
            return None
        if not isinstance(msg, dict) or not isinstance(msg.get("t"), str):
            return None
        return msg

    @staticmethod
    def _reject_msg(msg):
        """reject 에 실린 문구 번호. 정수(참·거짓 제외)이고 범위 안일 때만 돌려준다 — 아니면 None(번호 없는 거절, DM-D6)."""
        n = msg.get("msg")
        if isinstance(n, bool) or not isinstance(n, int) or not REJECT_MSG_MIN <= n <= REJECT_MSG_MAX:
            return None
        return n

    def _on_call(self, dev, msg, now):
        """걸기 판정 — 설계서 §6.4 의 여섯 줄을 위에서부터 그대로."""
        has_to = "to" in msg
        to = msg.get("to")
        mine = self.call_of.get(dev)
        if mine is not None:
            # 1. 벨을 받는 중이고 to 가 없거나 to 가 나에게 건 기기 → 받기로 취급(서로 동시에 걸기 — 옛 규칙의 일반형)
            if mine.state == RINGING and dev == mine.callee and (not has_to or to == mine.caller):
                return self._connect(mine, now)
            # 2. 이미 벨·통화에 묶여 있으면 무시(응답 없음) — 벨 받는 중에 제3자를 부른 것, 거듭 걸기, 통화 중 걸기
            return []
        # 3. to 가 있으면 등록된 "다른" 기기 ID 와 글자까지 같아야 한다. 아니면 무시(bad_signal).
        #    깨진 to(숫자·null·빈 글자·목록)를 "to 없음"으로 낮추지 않는다 — 옛 짝에게 벨이 가면 부르지 않은 사람(집의 1번)이 울린다.
        #    비용: 펌웨어가 to 를 잘못 실으면 걸리지 않고 응답도 없다(기기는 응답 없는 신호에 멈추지 않는다 — §9.4 계약). 탈출구: 이 줄에서 target 을 옛 짝으로.
        if has_to:
            if not isinstance(to, str) or to == dev or to not in self.devices:
                self.stats["bad_signal"] += 1
                return []
            target = to
        else:
            target = self.legacy_peer_of(dev)    # to 없는 call = 옛 짝에게(v1 호환. v2 연결이 보내도 같은 규칙 — §6.2)
            if target is None:                   # 옛 짝이 없는 기기 → 갈 곳이 없다. 설계서 글자대로 무시만 하고 bad_signal 로는 세지 않는다
                return []
        # 4. 대상이 꺼져 있거나 서로 통화가 성립하지 않는 쌍(v1 과 그 옛 짝이 아닌 기기 — TW-D8) → 거는 쪽에 offline.
        #    5번보다 먼저다: 볼 수 없는 기기가 통화 중인지는 알리지 않는다.
        if not self._reachable(dev, target):
            return self._refuse(dev, target, "offline")
        # 5. 대상이 벨·통화에 묶여 있으면 거는 쪽에만 busy (TW-D9). 묶여 있는 쪽에는 아무것도 보내지 않는다.
        if target in self.call_of:
            return self._refuse(dev, target, "busy")
        # 6. 벨: 거는 쪽에 ringing, 받는 쪽에 ring(v2 연결이면 건 기기를 from 으로). 30초 제한은 Call 이 든다.
        self.call_of[dev] = self.call_of[target] = Call(dev, target, now)
        ring = {"t": "ring", "from": dev} if self.proto[target] == PROTO_V2 else {"t": "ring"}
        return [("send", dev, {"t": "ringing"}), ("send", target, ring)]

    @staticmethod
    def _refuse(caller, callee, why):
        """벨까지 가지 못한 걸기(offline·busy): 거는 쪽에만 ended, 통화는 만들지 않는다. 기록은 길이 0 의 call_end(옛 offline 기록과 같은 모양)."""
        return [("send", caller, {"t": "ended", "why": why}),
                ("log", {"ev": "call_end", "caller": caller, "callee": callee, "why": why, "ring_s": 0, "talk_s": 0})]

    def _connect(self, call, now):
        call.state, call.call_started = IN_CALL, now
        return [("send", call.caller, {"t": "connected"}), ("send", call.callee, {"t": "connected"})]

    def _end(self, call, why, now, msg=None):
        """통화(또는 벨) 한 건을 끝낸다. 아직 붙어 있는 양쪽 모두에 ended 를 보낸다(PL-D1) — 그 둘에게만. msg = 거절 문구 번호(없으면 None)."""
        in_call = call.state == IN_CALL
        talk = round(now - call.call_started, 1) if in_call else 0
        ring = round((call.call_started if in_call else now) - call.ring_started, 1)
        for d in (call.caller, call.callee):
            if self.call_of.get(d) is call:
                del self.call_of[d]
        extra = {} if msg is None else {"msg": msg}       # 번호는 기기에만 간다 — 아래 기록(log)에는 넣지 않는다(§9.3)
        acts = [("send", d, {"t": "ended", "why": why, **extra}) for d in (call.caller, call.callee) if self.is_online(d)]
        acts.append(("log", {"ev": "call_end", "caller": call.caller, "callee": call.callee, "why": why, "ring_s": ring, "talk_s": talk}))
        return acts

    # ── 음성 ────────────────────────────────────────────────────────────────
    def route_voice(self, dev, size, now):
        """이진 프레임을 건넬 상대 기기ID(같은 통화의 상대), 버려야 하면 None. 내용은 보지 않는다(크기만)."""
        if not self.is_online(dev):
            return None
        self.last_seen[dev] = now
        if size > MAX_VOICE_BYTES or size < MIN_VOICE_BYTES:
            self.stats["voice_drop_size"] += 1
            return None
        call = self.call_of.get(dev)
        if call is None or call.state != IN_CALL:
            self.stats["voice_drop_state"] += 1
            return None
        self.stats["voice_fwd"] += 1
        return call.other(dev)

    # ── 시간 경과 ───────────────────────────────────────────────────────────
    def tick(self, now):
        acts = []
        for dev in list(self.conns):
            if now - self.last_seen.get(dev, now) > SILENCE_TIMEOUT_S:
                conn = self.conns[dev]
                acts.append(("log", {"ev": "silence_close", "dev": dev}))
                acts += self.detach(dev, conn, now)
                acts.append(("close", conn))
        for dev in self.devices:                 # 통화마다 한 번(건 쪽 기준, 등록 순서) — 벨 제한은 통화별이다
            call = self.call_of.get(dev)
            if call is not None and call.caller == dev and call.state == RINGING and now >= call.ring_deadline:
                acts += self._end(call, "timeout", now)
        return acts
