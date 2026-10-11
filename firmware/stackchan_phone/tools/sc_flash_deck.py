#!/usr/bin/env python3
r"""sc_flash_deck.py — 스트림덱(UIFlow2)이 돌고 있을 때 스택짱의 전화기 앱(ota_0 칸)을 갱신한다. 손이 필요 없다.

전화기 펌웨어가 돌 때는 tools/sc_flash.sh(esptool)를 쓴다. 스트림덱이 돌 때는 esptool 을 걸 수 없으므로(USB 에서 사라져 전원을 껐다 켜야 한다 —
~/CLAUDE.md Gotchas) 이 도구가 MicroPython 의 raw REPL 로 들어가 `esp32.Partition('ota_0').writeblocks()` 로 4KB 씩 쓴다.
쓰는 곳은 전화기 칸뿐이다(부트로더·파티션 표·스트림덱 앱·파일은 건드리지 않는다). 다 쓴 뒤 기기에서 SHA256 을 다시 계산해 견준다.
도는 동안 스트림덱 앱은 멈춰 있고(화면이 그대로 선다), 끝나면 — 성공이든 실패든 — 기기를 다시 켠다.

  python3 sc_flash_deck.py [--go] [--app build/stackchan_phone.bin]
    --go    다 쓴 뒤 전화기로 넘어간다(부팅 대상 = ota_0 → 재시작). 없으면 스트림덱이 다시 뜬다
  끝나는 값: 0 성공 · 1 실패(기기 상태·대조 불일치 — 부팅 대상은 스트림덱) · 2 인자가 틀림

결정:
  FD-D1 한 번에 2KB 씩 base64 로 보낸다(4KB 블록 = 두 번). 이유: 이 기기의 REPL 로 검증된 올리기(~/Deskbot/firmware/stackdeck/deploy.py)가 512B 씩이고,
        USB CDC 는 받는 쪽이 밀리면 보내는 쪽이 기다리므로 더 커도 되지만 여유를 둔다. 비용: 1.8MB 에 약 900번 주고받는다(2026-10-09 실측 42초).
  FD-D2 블록은 지우고 쓴다(writeblocks 의 인자 둘 모양). 마지막 블록의 남는 자리는 0xFF. 앱 뒤쪽의 옛 데이터는 지우지 않는다(부트로더는 이미지 길이만큼만 읽는다).
  FD-D3 넘어가기 전에 set_boot 가 앱 이미지를 다시 검사한다(MicroPython → IDF). 깨졌으면 예외가 나고 스트림덱에 머문다.
  FD-D4 raw REPL 에 들어간 뒤에는 어떻게 끝나든 기기를 다시 켠다(finally). 이유: 들어가면서 스트림덱 앱을 멈추므로, 중간에 실패한 채 두면 화면이 선 스트림덱이 남는다.
  FD-D5 반쯤 쓰인 앱으로 부팅되지 않게 한다: 쓰기 전에 부팅 대상을 지금 도는 스트림덱으로 못박고(= otadata 지움), 앱의 첫 블록(이미지 머리)을 먼저 지운 뒤 맨 마지막에 쓴다.
        이유: 부트로더의 이미지 검사에 기대지 않고도 "다 쓰기 전에는 전화기 칸에 부팅할 것이 없다"가 선다. 비용: 스트림덱 앱 검사 한 번(1~2초).
"""
import argparse
import base64
import hashlib
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import sc_ctl  # noqa: E402

BLOCK = 4096
CHUNK = 2048                      # FD-D1
APP_ADDR, APP_MAX = 0xBE0000, 0x370000
SETUP = ("import esp32,machine,uhashlib\nfrom ubinascii import a2b_base64 as a2b, hexlify\n"
         "p=esp32.Partition('ota_0');b=bytearray(4096);i=p.info();r=esp32.Partition(esp32.Partition.RUNNING)\n"
         "print(i[2],i[3],r.info()[4])")
PIN_DECK = "r.set_boot();print('boot=deck')"                    # FD-D5: factory 를 고르면 IDF 가 otadata 를 지운다
WIPE_HEAD = "p.writeblocks(0,b'\\xff'*4096)"                    # FD-D5
GO_PHONE = "p.set_boot();print('boot=ota_0')"                   # FD-D3
RESET = "machine.reset()"
_now, _sleep = time.time, time.sleep      # 시험이 바꿔 끼운다


def blocks(image):
    """앱 이미지 → 4KB 블록들(마지막은 0xFF 로 채움 — FD-D2)."""
    out = []
    for i in range(0, len(image), BLOCK):
        b = image[i:i + BLOCK]
        out.append(b + b"\xff" * (BLOCK - len(b)))
    return out


def write_order(n):
    """블록을 쓰는 순서: 1, 2, …, n-1, 0 — 이미지 머리(0번)가 맨 마지막이다(FD-D5)."""
    return list(range(1, n)) + ([0] if n else [])


def block_steps(index, block):
    """블록 하나를 쓰는 REPL 문장들. 마지막 문장이 그 블록을 지우고 쓴다."""
    if len(block) != BLOCK:
        raise ValueError("블록 크기가 4096 이 아니다")
    steps = []
    for off in range(0, BLOCK, CHUNK):
        text = base64.b64encode(block[off:off + CHUNK]).decode()
        steps.append(f"b[{off}:{off + CHUNK}]=a2b('{text}')")
    steps[-1] += f";p.writeblocks({index},b)"
    return steps


def hash_step(length):
    return (f"h=uhashlib.sha256();n={length};k=0\n"
            "while n>0:\n p.readblocks(k,b);m=min(n,4096);h.update(memoryview(b)[:m]);n-=m;k+=1\n"
            "print(hexlify(h.digest()).decode())")


def parse_reply(buf):
    """raw REPL 의 답 b'OK<출력>\x04<오류>\x04>' → 출력 글자. 모양이 다르거나 오류가 있으면 RuntimeError."""
    if not buf.startswith(b"OK") or not buf.endswith(b"\x04>"):
        raise RuntimeError(f"기기가 문장을 받지 않았다: {buf[:120]!r}")
    out, sep, err = buf[2:-2].rpartition(b"\x04")              # 오류 칸은 맨 끝 — 출력에 \x04 가 섞여도 여기서 갈린다
    if not sep:
        raise RuntimeError(f"답의 모양이 다르다: {buf[:120]!r}")
    if err.strip():
        raise RuntimeError("기기 오류: " + err.decode("utf-8", "replace").strip()[-300:])
    return out.decode("utf-8", "replace").strip()


class Repl:
    """MicroPython raw REPL. 답이 끝나는 표시(\x04>)가 오면 바로 돌아온다."""

    def __init__(self, port, ser=None):
        if ser is None:
            import serial
            ser = serial.Serial(port, 115200, timeout=0.2)
        self.s = ser

    def _read_until(self, done, timeout, what):
        buf, end = b"", _now() + timeout
        while not done(buf):
            if _now() > end:
                raise RuntimeError(f"{what}: {buf[-120:]!r}")
            buf += self.s.read(self.s.in_waiting or 1)
        return buf

    def enter(self):
        for _ in range(2):                      # 돌고 있는 앱(asyncio)을 멈춘다
            self.s.write(b"\x03")
            _sleep(0.3)
        self.s.write(b"\x01")                   # raw REPL
        # 프롬프트 맞추기: 표시 문장의 답이 올 때까지 온 것은 버린다(멈춘 앱의 출력·REPL 안내문). 빈 Ctrl-D 는 보내지 않는다 —
        # raw REPL 에서 빈 실행은 소프트 재시작이라 UIFlow2 의 시작 글자("Syncing resources…")가 몇 초 뒤에 끼어든다(2026-10-09 첫 실행이 그래서 멈췄다).
        _sleep(0.3)
        self.s.write(b"print('SC-SYNC')\x04")
        self._read_until(lambda b: b.endswith(b"SC-SYNC\r\n\x04\x04>"), 15, "raw REPL 에 들어가지 못했다")

    def run(self, code, timeout=15.0):
        self.s.write(code.encode() + b"\x04")
        buf = self._read_until(lambda b: b.endswith(b"\x04>") and b.count(b"\x04") >= 2, timeout, f"기기가 답하지 않는다({code[:40]!r})")
        return parse_reply(buf)

    def send(self, code):                       # 답을 기다리지 않는다(재시작)
        self.s.write(b"\x03" + code.encode() + b"\x04")
        _sleep(0.5)

    def close(self):
        try:
            self.s.close()
        except Exception:
            pass


def flash(r, image, go, say=print):
    """raw REPL 에 들어가 있는 기기에 앱을 쓴다. 성공하면 True(부팅 대상: go 면 전화기, 아니면 스트림덱). 실패는 RuntimeError — 부팅 대상은 스트림덱."""
    addr, size, running = r.run(SETUP).split()
    if int(addr) != APP_ADDR or int(size) != APP_MAX:
        raise RuntimeError(f"전화기 칸이 기대와 다르다(0x{int(addr):x} +0x{int(size):x})")
    if running != "factory":
        raise RuntimeError(f"지금 도는 것이 스트림덱 칸이 아니다({running})")
    r.run(PIN_DECK, timeout=30)                 # FD-D5
    r.run(WIPE_HEAD)
    bl, t0 = blocks(image), time.time()
    for done, n in enumerate(write_order(len(bl)), 1):
        for step in block_steps(n, bl[n]):
            r.run(step)
        if done % 64 == 0 or done == len(bl):
            say(f"  {done}/{len(bl)} 블록 ({time.time() - t0:.0f}초)")
    want, got = hashlib.sha256(image).hexdigest(), r.run(hash_step(len(image)), timeout=60)
    if got != want:
        r.run(WIPE_HEAD)                        # 틀린 것을 부팅할 수 있는 모양으로 남기지 않는다
        raise RuntimeError(f"기기에 쓰인 것이 다르다(기기 {got[:16]}… ≠ 파일 {want[:16]}…)")
    say(f"기기에서 다시 읽은 sha256 일치 ({time.time() - t0:.0f}초)")
    if go:
        say(r.run(GO_PHONE, timeout=30))
    return True


def main(argv=None, repl_factory=Repl):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--go", action="store_true")
    ap.add_argument("--app", default=str(HERE.parent / "build" / "stackchan_phone.bin"))
    a = ap.parse_args(argv)
    image = Path(a.app).read_bytes()
    if not image or image[0] != 0xE9 or len(image) > APP_MAX:
        print(f"중단: 앱 이미지가 아니거나 전화기 칸보다 크다({len(image)} B)", file=sys.stderr)
        return 1
    kind, port = sc_ctl.which()
    if kind != "deck":
        print(f"중단: 스택짱이 '{kind}' 상태다 — 스트림덱이 돌 때만 쓴다(전화기 펌웨어가 돌면 tools/sc_flash.sh)", file=sys.stderr)
        return 1
    print(f"포트 {port} · 앱 {len(image)} B = {len(blocks(image))} 블록 · sha256 {hashlib.sha256(image).hexdigest()[:16]}…")
    r, entered, ok = repl_factory(port), False, False
    try:
        r.enter()
        entered = True
        ok = flash(r, image, a.go, say=lambda m: print(m, flush=True))
    except (RuntimeError, OSError, ValueError) as e:      # OSError: 포트가 끊김(pyserial 의 SerialException 포함)
        print(f"중단: {e}", file=sys.stderr)
    finally:
        if entered:                             # FD-D4
            try:
                r.send(RESET)
                print("재시작 — " + ("전화기 펌웨어로 넘어간다" if ok and a.go else "스트림덱이 다시 뜬다(전화기로는 \"전화기\" 타일)"))
            except OSError as e:
                print(f"재시작을 보내지 못했다({e}) — 스트림덱이 멈춰 있으면 RST 를 짧게 한 번", file=sys.stderr)
        r.close()
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
