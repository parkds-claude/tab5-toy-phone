#!/usr/bin/env python3
r"""sc_ctl.py — 스택짱 전화기(3번)의 시리얼 검증 도구.

Tab5 의 도구(../../tab5_phone/tools/phone_ctl.py)를 그대로 불러 쓴다(복사하지 않는다) — 단계 문법(until·wait·sleep·tap·shot·그 외 = 명령)과 줄 모양이 같다.
여기서 더하는 것은 셋뿐이다: ① 3번 = 스택짱 2호기(MAC AA:BB:CC:DD:EE:03)를 기기 표에 더한다 ② shot 을 PNG 로 바꿔 저장한다(스택짱은 JPEG 이 아니라
화면 그대로 RGB565 320×240 을 보낸다) ③ pair 에 함께 열 기기를 고른다(1번 Tab5 가 집에 있어도 2번·3번만으로 돌린다).

  python3 sc_ctl.py port                         스택짱의 포트 경로만 출력(포트를 열지 않는다). 전화기 펌웨어가 돌 때만 보인다
  python3 sc_ctl.py which                        지금 무엇이 도는가: phone(전화기 펌웨어·다운로드 모드 — USB 일련번호에 ':' 가 있다) | deck(스트림덱 = UIFlow2) | none
  python3 sc_ctl.py run <단계> …                 스택짱 한 대
  python3 sc_ctl.py pair [--with 2] <단계> …     스택짱(3:) + Tab5(기본 2번, 2:) 를 함께. 접두사는 2: 3: both:
예시 — 화면 찍기와 상태:
  python3 sc_ctl.py run 'until:stat:^STAT .*fsm=IDLE:60' 'shot:/tmp/sc-idle.png'
예시 — 스택짱이 2번 Tab5 에게 걸기(대기 화면 오른쪽 키캡 = slot 1 = 2번. 좌표는 main/sc_ui_layout.h 의 BTN_* 가운데):
  python3 sc_ctl.py pair '3:until:stat:^STAT .*fsm=IDLE.*peer1=1:60' '3:tap:238:110' '2:wait:^FSM IDLE -> INCOMING:10' '3:tap:160:197' '3:wait:^FSM ENDING -> IDLE:10'
주의: 스트림덱(UIFlow2)이 돌고 있을 때는 이 도구를 쓰지 않는다(그쪽 REPL 에 글자가 들어간다). 'which' 가 deck 이면 스트림덱의 "전화기" 버튼으로 먼저 넘어온다.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tab5_phone" / "tools"))
import phone_ctl  # noqa: E402

SC_UNIT, SC_MAC = "3", "AA:BB:CC:DD:EE:03"          # 스택짱 2호기. 1호기(AA:BB:CC:DD:EE:04)는 건드리지 않는다
W, H = 320, 240
TAB5 = dict(phone_ctl.UNITS)                        # {"1": MAC, "2": MAC}


def _hex(s):
    return "".join(ch for ch in (s or "").upper() if ch in "0123456789ABCDEF")


def which(ports=None):
    """'phone' | 'deck' | 'none'. 전화기 펌웨어·다운로드 모드는 칩 내장 USB(일련번호 'AA:BB:…'), UIFlow2 는 자체 USB('aabb…0000')."""
    if ports is None:
        ports = phone_ctl.list_usb_ports()
    for p in ports:
        if _hex(p.serial_number).startswith(_hex(SC_MAC)):
            return ("phone", p.device) if ":" in (p.serial_number or "") else ("deck", p.device)
    return ("none", None)


def find_port(unit, ports=None):
    if unit != SC_UNIT:
        return _tab5_find_port(unit, ports)
    kind, dev = which(ports)
    if kind == "phone":
        return dev
    sys.exit("스택짱이 스트림덱(UIFlow2)으로 돌고 있음 — \"전화기\" 버튼으로 넘어온 뒤 다시" if kind == "deck"
             else f"스택짱 2호기({SC_MAC})가 USB 에 보이지 않음 — 전원·케이블 확인")


def rgb565_to_png(raw, path):
    """화면 그대로(RGB565 리틀엔디언 320×240) → PNG."""
    if len(raw) != W * H * 2:
        raise ValueError(f"화면 크기가 다름: {len(raw)} 바이트(기대 {W * H * 2})")
    from PIL import Image
    img = Image.new("RGB", (W, H))
    px = img.load()
    for i in range(W * H):
        v = raw[2 * i] | (raw[2 * i + 1] << 8)
        px[i % W, i // W] = ((v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)
    img.save(path)


_tab5_find_port = phone_ctl.find_port
_tab5_shot = phone_ctl.Hub.shot


def _shot(self, unit, path):
    ok = _tab5_shot(self, unit, path)                # 받은 바이트를 path 에 쓴다(크기·검사합 확인은 그쪽이 한다)
    if not ok or unit != SC_UNIT:
        return ok
    try:
        raw = Path(path).read_bytes()
        rgb565_to_png(raw, path)
    except (OSError, ValueError) as e:
        self.note("!!", unit, f"shot 변환 실패: {e}")
        return False
    self.note("==", unit, f"PNG 로 바꿈: {path} ({W}×{H})")
    return True


phone_ctl.find_port = find_port
phone_ctl.Hub.shot = _shot


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    cmd, rest = argv[0], argv[1:]
    if cmd == "which":
        print(which()[0])
        return 0
    if cmd == "port":
        phone_ctl.UNITS = {SC_UNIT: SC_MAC}
        print(find_port(SC_UNIT))
        return 0
    if cmd == "run":
        phone_ctl.UNITS = {SC_UNIT: SC_MAC}
        return phone_ctl.main(["--unit", SC_UNIT, "run", *rest])
    if cmd == "pair":
        other = "2"
        if rest[:1] == ["--with"]:
            if len(rest) < 2 or rest[1] not in TAB5:
                print("pair --with 는 1 또는 2", file=sys.stderr)
                return 2
            other, rest = rest[1], rest[2:]
        phone_ctl.UNITS = {other: TAB5[other], SC_UNIT: SC_MAC}
        return phone_ctl.main(["pair", *rest])
    print(f"모르는 명령: {cmd} (port | which | run | pair)", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
