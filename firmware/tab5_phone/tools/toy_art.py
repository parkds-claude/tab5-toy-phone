"""toy_art.py — 토이폰 테마의 그림 원본(SVG 조각). 캐릭터 넷(곰·토끼·로봇·병아리)과 아이콘.

여기가 그림의 정본이다. tools/gen_toy_sprites.py 가 이 목록을 rsvg-convert 로 그려 main/ui_sprites.h 로 굽는다.
디자인 규칙: docs/design/README.md §5 (토이폰 테마). 좌표는 캐릭터마다 자기 좌표계(곰·로봇 240×260, 토끼 240×300, 병아리 150×160)이고
몸의 세로 대칭축(axis)과 발 밑 그림자 높이(base)를 함께 적는다 — 기기는 "대칭축 x, 바닥 y"로 놓는다(toy::character).

자세(pose)
  wave   한 손(자기 왼손)을 든 기본 자세. 화면 오른쪽에 설 때는 기기가 좌우를 뒤집어 그린다(바깥쪽 손을 들게).
  hi     wave 와 같은 그림을 작게(IDLE_SCALE) — 대기 화면에서 구름 위에 선 상대(전화기 3대: 상대 둘이 통화 키캡 위에 선다).
  sleep  눈 감고 Z — 상대 전화기가 꺼져 있을 때(hi 와 같은 크기). 뒤집지 않는다(Z 가 뒤집히면 안 된다).
  cup    종이컵 전화를 든 자세(자기 오른쪽). 오른쪽에 설 때는 뒤집는다(컵이 가운데를 향하게).
  cheer  두 손을 든 자세 — 전화가 왔을 때의 나.
  caller cheer + 좌우 울림 줄, 그림자 없음 — 전화를 건 상대(구름 위).
"""

INK = "#2A1F3D"
WHITE = "#FFFFFF"


def _stroked(inner: str, sw: float = 6) -> str:
    return f'<g stroke="{INK}" stroke-width="{sw}" stroke-linecap="round" stroke-linejoin="round">{inner}</g>'


def _eyes(y: int, closed: bool) -> str:
    if closed:
        return (f'<path d="M78 {y} Q86 {y + 8} 94 {y} M146 {y} Q154 {y + 8} 162 {y}" fill="none" stroke="{INK}" '
                f'stroke-width="4.5" stroke-linecap="round"/>')
    return (f'<ellipse cx="86" cy="{y}" rx="8" ry="10" fill="{INK}"/><circle cx="83" cy="{y - 4}" r="3" fill="{WHITE}"/>'
            f'<ellipse cx="154" cy="{y}" rx="8" ry="10" fill="{INK}"/><circle cx="151" cy="{y - 4}" r="3" fill="{WHITE}"/>')


def _zzz(x: int, y: int) -> str:
    return (f'<g fill="none" stroke="{INK}" stroke-width="5" stroke-linecap="round" stroke-linejoin="round">'
            f'<path d="M{x} {y} H{x + 16} L{x} {y + 18} H{x + 16}"/>'
            f'<path d="M{x + 24} {y - 32} H{x + 35} L{x + 24} {y - 19} H{x + 35}"/></g>')


def _arcs(cy: int) -> str:
    """전화 건 쪽의 좌우 울림 줄(노랑 + 잉크 테두리)."""
    shapes = [f"M-18 {cy - 34} Q-40 {cy} -18 {cy + 34}", f"M-44 {cy - 52} Q-80 {cy} -44 {cy + 52}",
              f"M258 {cy - 34} Q280 {cy} 258 {cy + 34}", f"M284 {cy - 52} Q320 {cy} 284 {cy + 52}"]
    under = "".join(f'<path d="{d}" stroke="{INK}" stroke-width="20"/>' for d in shapes)
    over = "".join(f'<path d="{d}" stroke="#FFD93D" stroke-width="10"/>' for d in shapes)
    return f'<g fill="none" stroke-linecap="round">{under}{over}</g>'


def _cup(y: int, paw: str) -> str:
    """종이컵 전화(캐릭터의 오른쪽 뺨 옆). y = 컵 가운데 높이. 줄은 기기가 컵 끝(224, y)에서 잇는다."""
    return _stroked(f'<path d="M176 {y - 22} L224 {y - 14} L224 {y + 14} L176 {y + 22} Z" fill="#FF5C5C"/>'
                    f'<ellipse cx="176" cy="{y}" rx="7" ry="22" fill="{WHITE}"/>'
                    f'<circle cx="204" cy="{y + 24}" r="13" fill="{paw}"/>', 5)


# ── 곰 (1번 전화기) — 좌표계 240×260, 대칭축 x=120, 바닥 y=246 ──────────────────────────────
def bear(pose: str) -> str:
    body, dark, light, muzzle = "#FFA94D", "#F08C2E", "#FFE0B8", "#FFF3E0"
    up_l = f'<ellipse cx="30" cy="104" rx="17" ry="30" transform="rotate(-40 30 104)" fill="{body}"/>'
    up_r = f'<ellipse cx="210" cy="104" rx="17" ry="30" transform="rotate(40 210 104)" fill="{body}"/>'
    rest_l = f'<ellipse cx="28" cy="172" rx="16" ry="28" transform="rotate(16 28 172)" fill="{body}"/>'
    rest_r = f'<ellipse cx="212" cy="172" rx="16" ry="28" transform="rotate(-16 212 172)" fill="{body}"/>'
    arms = {"wave": up_l + rest_r, "sleep": rest_l + rest_r, "cup": rest_l, "cheer": up_l + up_r, "caller": up_l + up_r}[pose]
    s = ""
    if pose == "caller":
        s += _arcs(120)
    else:
        s += f'<ellipse cx="120" cy="246" rx="78" ry="10" fill="{INK}" opacity="0.18"/>'
    s += _stroked(f'<circle cx="52" cy="46" r="28" fill="{body}"/><circle cx="188" cy="46" r="28" fill="{body}"/>')
    s += f'<circle cx="50" cy="44" r="12" fill="#FFD9A8"/><circle cx="190" cy="44" r="12" fill="#FFD9A8"/>'
    s += _stroked(f'<ellipse cx="88" cy="236" rx="26" ry="13" fill="{dark}"/><ellipse cx="152" cy="236" rx="26" ry="13" fill="{dark}"/>'
                  + arms +
                  f'<path d="M120 40 C176 40 214 84 214 146 C214 206 178 236 120 236 C62 236 26 206 26 146 C26 84 64 40 120 40 Z" fill="{body}"/>')
    s += f'<ellipse cx="120" cy="186" rx="50" ry="37" fill="{light}"/>'
    s += f'<path d="M50 112 Q58 76 92 60" fill="none" stroke="{WHITE}" stroke-width="9" stroke-linecap="round" opacity="0.55"/>'
    s += f'<ellipse cx="120" cy="149" rx="27" ry="21" fill="{muzzle}"/>'
    s += _eyes(124, pose == "sleep")
    s += f'<ellipse cx="120" cy="140" rx="9" ry="6.5" fill="{INK}"/>'
    if pose in ("cup", "cheer", "caller"):
        s += f'<path d="M110 151 Q120 170 130 151 Z" fill="#E5484D" stroke="{INK}" stroke-width="4.5" stroke-linejoin="round"/>'
    elif pose == "sleep":
        s += f'<path d="M120 146 V151 M114 155 Q120 159 126 155" fill="none" stroke="{INK}" stroke-width="4.5" stroke-linecap="round"/>'
    else:
        s += (f'<path d="M120 146 V152 M108 153 Q114 161 120 152 Q126 161 132 153" fill="none" stroke="{INK}" stroke-width="4.5" '
              f'stroke-linecap="round" stroke-linejoin="round"/>')
    s += '<circle cx="60" cy="150" r="12" fill="#FF6B81" opacity="0.55"/>'
    if pose == "cup":
        s += _cup(148, body)
    else:
        s += '<circle cx="180" cy="150" r="12" fill="#FF6B81" opacity="0.55"/>'
    if pose == "sleep":
        s += _zzz(226, 40)
    return s


# ── 토끼 (2번 전화기) — 좌표계 240×300, 대칭축 x=120, 바닥 y=286 ─────────────────────────────
def rabbit(pose: str) -> str:
    body, pink, foot = WHITE, "#FF9EC4", "#FFD6E5"
    up_l = f'<ellipse cx="30" cy="144" rx="17" ry="30" transform="rotate(-40 30 144)" fill="{body}"/>'
    up_r = f'<ellipse cx="210" cy="144" rx="17" ry="30" transform="rotate(40 210 144)" fill="{body}"/>'
    rest_l = f'<ellipse cx="28" cy="212" rx="16" ry="28" transform="rotate(16 28 212)" fill="{body}"/>'
    rest_r = f'<ellipse cx="212" cy="212" rx="16" ry="28" transform="rotate(-16 212 212)" fill="{body}"/>'
    arms = {"wave": up_l + rest_r, "sleep": rest_l + rest_r, "cup": rest_l, "cheer": up_l + up_r, "caller": up_l + up_r}[pose]
    if pose == "sleep":                                   # 잠들면 귀가 처진다
        ears = ((78, 70, -30), (162, 70, 30))
    else:
        ears = ((90, 60, -10), (150, 60, 8))
    s = ""
    if pose == "caller":
        s += _arcs(130)
    else:
        s += f'<ellipse cx="120" cy="286" rx="78" ry="10" fill="{INK}" opacity="0.18"/>'
    s += _stroked("".join(f'<ellipse cx="{x}" cy="{y}" rx="21" ry="50" transform="rotate({a} {x} {y})" fill="{body}"/>' for x, y, a in ears))
    s += "".join(f'<ellipse cx="{x}" cy="{y + 2}" rx="9" ry="32" transform="rotate({a} {x} {y + 2})" fill="{pink}"/>' for x, y, a in ears)
    s += _stroked(f'<ellipse cx="88" cy="276" rx="26" ry="13" fill="{foot}"/><ellipse cx="152" cy="276" rx="26" ry="13" fill="{foot}"/>'
                  + arms +
                  f'<path d="M120 80 C176 80 214 124 214 186 C214 246 178 276 120 276 C62 276 26 246 26 186 C26 124 64 80 120 80 Z" fill="{body}"/>')
    s += '<ellipse cx="120" cy="228" rx="48" ry="35" fill="#FFE6F0"/>'
    s += '<path d="M188 232 Q172 256 142 262" fill="none" stroke="#E6E1F2" stroke-width="9" stroke-linecap="round"/>'
    s += _eyes(164, pose == "sleep")
    s += '<ellipse cx="120" cy="178" rx="6.5" ry="4.5" fill="#FF6FA3"/>'
    if pose in ("cheer", "caller"):
        s += f'<path d="M109 187 Q120 208 131 187 Z" fill="#E5484D" stroke="{INK}" stroke-width="4.5" stroke-linejoin="round"/>'
    elif pose == "sleep":
        s += f'<path d="M114 188 Q120 193 126 188" fill="none" stroke="{INK}" stroke-width="4.5" stroke-linecap="round"/>'
    else:
        s += (f'<path d="M108 187 Q114 196 120 186 Q126 196 132 187" fill="none" stroke="{INK}" stroke-width="4.5" '
              f'stroke-linecap="round" stroke-linejoin="round"/>')
    s += f'<circle cx="60" cy="190" r="12" fill="{pink}" opacity="0.7"/>'
    if pose == "cup":
        s += _cup(188, body)
    else:
        s += f'<circle cx="180" cy="190" r="12" fill="{pink}" opacity="0.7"/>'
    if pose == "sleep":
        s += _zzz(204, 84)
    return s


# ── 로봇 (3번 전화기 = 스택짱) — 좌표계 240×260, 대칭축 x=120, 바닥 y=246 (곰과 같은 몸통 윤곽) ─────
# 곰·토끼와 한 식구로 보이게 몸통 곡선·팔·발·그림자·굵은 테두리를 그대로 쓰고, "기계 친구"임은 세 가지로만 보인다:
# 머리 위 안테나(빨간 공), 네모난 얼굴 창(그 안에 눈·입·볼), 배의 단추 셋. 잠들면 안테나 공이 꺼지고(회색) 창이 어두워진다.
def robot(pose: str) -> str:
    body, dark, belly, screen = "#8FE3CF", "#58C7AF", "#D9F7EF", "#F6FCFF"
    asleep = pose == "sleep"
    up_l = f'<ellipse cx="30" cy="104" rx="17" ry="30" transform="rotate(-40 30 104)" fill="{body}"/>'
    up_r = f'<ellipse cx="210" cy="104" rx="17" ry="30" transform="rotate(40 210 104)" fill="{body}"/>'
    rest_l = f'<ellipse cx="28" cy="172" rx="16" ry="28" transform="rotate(16 28 172)" fill="{body}"/>'
    rest_r = f'<ellipse cx="212" cy="172" rx="16" ry="28" transform="rotate(-16 212 172)" fill="{body}"/>'
    arms = {"wave": up_l + rest_r, "sleep": rest_l + rest_r, "cup": rest_l, "cheer": up_l + up_r, "caller": up_l + up_r}[pose]
    s = ""
    if pose == "caller":
        s += _arcs(120)
    else:
        s += f'<ellipse cx="120" cy="246" rx="78" ry="10" fill="{INK}" opacity="0.18"/>'
    # 안테나(몸통 뒤) — 잠들면 살짝 기울고 공이 꺼진다
    if asleep:
        s += _stroked(f'<path d="M120 46 Q118 30 106 22" fill="none"/><circle cx="102" cy="19" r="11" fill="#C9C4D6"/>')
    else:
        s += _stroked(f'<path d="M120 46 V24" fill="none"/><circle cx="120" cy="16" r="12" fill="#FF5C5C"/>')
        s += f'<circle cx="116" cy="12" r="3.5" fill="{WHITE}" opacity="0.8"/>'
    # 귀 자리의 둥근 나사(몸통 뒤)
    s += _stroked(f'<circle cx="34" cy="112" r="15" fill="{dark}"/><circle cx="206" cy="112" r="15" fill="{dark}"/>')
    s += _stroked(f'<ellipse cx="88" cy="236" rx="26" ry="13" fill="{dark}"/><ellipse cx="152" cy="236" rx="26" ry="13" fill="{dark}"/>'
                  + arms +
                  f'<path d="M120 40 C176 40 214 84 214 146 C214 206 178 236 120 236 C62 236 26 206 26 146 C26 84 64 40 120 40 Z" fill="{body}"/>')
    s += f'<path d="M50 112 Q58 76 92 60" fill="none" stroke="{WHITE}" stroke-width="9" stroke-linecap="round" opacity="0.55"/>'
    # 배의 판과 단추 셋
    s += f'<rect x="80" y="184" width="80" height="38" rx="15" fill="{belly}"/>'
    s += "".join(f'<circle cx="{x}" cy="203" r="8" fill="{c}" stroke="{INK}" stroke-width="3.5"/>'
                 for x, c in ((98, "#FF6B81"), (120, "#FFD93D"), (142, "#6BB8FF")))
    # 얼굴 창
    s += _stroked(f'<rect x="58" y="88" width="124" height="84" rx="30" fill="{"#DCE6F0" if asleep else screen}"/>', 5)
    s += _eyes(122, asleep)
    if pose in ("cup", "cheer", "caller"):
        s += f'<path d="M109 140 Q120 160 131 140 Z" fill="#E5484D" stroke="{INK}" stroke-width="4.5" stroke-linejoin="round"/>'
    elif asleep:
        s += f'<path d="M113 146 H127" fill="none" stroke="{INK}" stroke-width="4.5" stroke-linecap="round"/>'
    else:
        s += f'<path d="M107 141 Q120 155 133 141" fill="none" stroke="{INK}" stroke-width="4.5" stroke-linecap="round"/>'
    s += '<circle cx="76" cy="146" r="9" fill="#FF6B81" opacity="0.55"/>'
    if pose == "cup":
        s += _cup(148, body)
    else:
        s += '<circle cx="164" cy="146" r="9" fill="#FF6B81" opacity="0.55"/>'
    if asleep:
        s += _zzz(226, 40)
    return s


# ── 병아리 (알림 담당) — 좌표계 150×160, 대칭축 x=75, 바닥 y=151 ─────────────────────────────
def chick(pose: str) -> str:
    body, wing, beak = "#FFE14D", "#FFC93D", "#FF9F1C"
    if pose == "jump":
        wings = (f'<ellipse cx="20" cy="86" rx="10" ry="18" transform="rotate(-50 20 86)" fill="{wing}"/>'
                 f'<ellipse cx="130" cy="86" rx="10" ry="18" transform="rotate(50 130 86)" fill="{wing}"/>')
    else:
        wings = (f'<ellipse cx="17" cy="96" rx="10" ry="18" transform="rotate(14 17 96)" fill="{wing}"/>'
                 f'<ellipse cx="133" cy="96" rx="10" ry="18" transform="rotate(-14 133 96)" fill="{wing}"/>')
    b = _stroked(f'<path d="M64 30 Q58 8 72 14 Q76 2 86 12 Q96 8 88 30" fill="{body}"/>'
                 f'<ellipse cx="58" cy="145" rx="12" ry="6" fill="{beak}"/><ellipse cx="92" cy="145" rx="12" ry="6" fill="{beak}"/>'
                 + wings +
                 f'<path d="M75 24 C113 24 136 53 136 88 C136 122 112 144 75 144 C38 144 14 122 14 88 C14 53 37 24 75 24 Z" fill="{body}"/>', 5)
    b += f'<path d="M32 66 Q40 44 62 36" fill="none" stroke="{WHITE}" stroke-width="6" stroke-linecap="round" opacity="0.6"/>'
    if pose == "hum":
        b += f'<path d="M45 82 Q52 89 59 82 M91 82 Q98 89 105 82" fill="none" stroke="{INK}" stroke-width="4.5" stroke-linecap="round"/>'
    else:
        b += (f'<ellipse cx="52" cy="82" rx="6" ry="7.5" fill="{INK}"/><circle cx="50" cy="79" r="2.2" fill="{WHITE}"/>'
              f'<ellipse cx="98" cy="82" rx="6" ry="7.5" fill="{INK}"/><circle cx="96" cy="79" r="2.2" fill="{WHITE}"/>')
    b += f'<path d="M64 94 L75 88 L86 94 L75 103 Z" fill="{beak}" stroke="{INK}" stroke-width="4" stroke-linejoin="round"/>'
    b += '<circle cx="36" cy="102" r="8" fill="#FF6B81" opacity="0.5"/><circle cx="114" cy="102" r="8" fill="#FF6B81" opacity="0.5"/>'
    if pose == "jump":                                    # 몸은 떠 있고 그림자는 작게
        return f'<ellipse cx="75" cy="151" rx="34" ry="6" fill="{INK}" opacity="0.14"/><g transform="translate(0 -14)">{b}</g>'
    s = f'<ellipse cx="75" cy="151" rx="44" ry="7" fill="{INK}" opacity="0.18"/>' + b
    if pose == "hum":
        s += note(156, 38)
    return s


def note(x: int, y: int) -> str:
    """음표 하나. (x, y) = 머리 가운데."""
    return (f'<ellipse cx="{x}" cy="{y + 14}" rx="8" ry="6" transform="rotate(-20 {x} {y + 14})" fill="{INK}"/>'
            f'<path d="M{x + 7} {y + 12} V{y - 12} L{x + 19} {y - 7}" fill="none" stroke="{INK}" stroke-width="4" '
            f'stroke-linecap="round" stroke-linejoin="round"/>')


# ── 작은 얼굴(상단 표시용) ────────────────────────────────────────────────────────────────────
def head_bear(asleep: bool = False) -> str:
    eyes = (f'<path d="M13 25 Q16.5 28.5 20 25 M28 25 Q31.5 28.5 35 25" fill="none" stroke="{INK}" stroke-width="2.4" stroke-linecap="round"/>'
            if asleep else f'<circle cx="16.5" cy="25" r="2.6" fill="{INK}"/><circle cx="31.5" cy="25" r="2.6" fill="{INK}"/>')
    return (_stroked('<circle cx="11" cy="12" r="8" fill="#FFA94D"/><circle cx="37" cy="12" r="8" fill="#FFA94D"/>'
                     '<circle cx="24" cy="27" r="19" fill="#FFA94D"/>', 3)
            + '<ellipse cx="24" cy="33" rx="8" ry="6" fill="#FFF3E0"/>' + eyes
            + f'<ellipse cx="24" cy="30.5" rx="3" ry="2.2" fill="{INK}"/>')


def head_rabbit(asleep: bool = False) -> str:
    eyes = (f'<path d="M13.5 34 Q17 37.5 20.5 34 M27.5 34 Q31 37.5 34.5 34" fill="none" stroke="{INK}" stroke-width="2.4" stroke-linecap="round"/>'
            if asleep else f'<circle cx="17" cy="34" r="2.6" fill="{INK}"/><circle cx="31" cy="34" r="2.6" fill="{INK}"/>')
    return (_stroked(f'<ellipse cx="16" cy="14" rx="6" ry="13" transform="rotate(-8 16 14)" fill="{WHITE}"/>'
                     f'<ellipse cx="32" cy="14" rx="6" ry="13" transform="rotate(8 32 14)" fill="{WHITE}"/>'
                     f'<circle cx="24" cy="36" r="18" fill="{WHITE}"/>', 3)
            + eyes + '<ellipse cx="24" cy="39.5" rx="2.6" ry="2" fill="#FF6FA3"/>'
            + '<circle cx="11.5" cy="41" r="3.2" fill="#FF9EC4"/><circle cx="36.5" cy="41" r="3.2" fill="#FF9EC4"/>')


def head_robot(asleep: bool = False) -> str:
    """로봇의 작은 얼굴. 좌표계 48×52 (안테나만큼 곰보다 4 높다)."""
    eyes = (f'<path d="M14.5 31 Q18 34.5 21.5 31 M26.5 31 Q30 34.5 33.5 31" fill="none" stroke="{INK}" stroke-width="2.4" stroke-linecap="round"/>'
            if asleep else f'<circle cx="18" cy="31" r="2.6" fill="{INK}"/><circle cx="30" cy="31" r="2.6" fill="{INK}"/>')
    ball = "#C9C4D6" if asleep else "#FF5C5C"
    mouth = (f'<path d="M21.5 37.5 H26.5" fill="none" stroke="{INK}" stroke-width="2" stroke-linecap="round"/>' if asleep
             else f'<path d="M20.5 36.5 Q24 40 27.5 36.5" fill="none" stroke="{INK}" stroke-width="2" stroke-linecap="round"/>')
    return (_stroked(f'<path d="M24 14 V8" fill="none"/><circle cx="24" cy="6" r="4" fill="{ball}"/>'
                     f'<circle cx="5.5" cy="32" r="4.5" fill="#58C7AF"/><circle cx="42.5" cy="32" r="4.5" fill="#58C7AF"/>'
                     f'<rect x="5" y="14" width="38" height="36" rx="14" fill="#8FE3CF"/>', 3)
            + f'<rect x="10.5" y="22" width="27" height="20" rx="8" fill="{"#DCE6F0" if asleep else "#F6FCFF"}" stroke="{INK}" stroke-width="2.2"/>'
            + eyes + mouth)


# ── 아이콘(잉크 한 색) ────────────────────────────────────────────────────────────────────────
def handset(rotate: int) -> str:
    """수화기. 0 = 엎어 놓은 모양(끊기·거절), -135 = 들어 올린 모양(통화·받기). 좌표계 64×64."""
    # 그림의 세로 범위가 16~48 (가운데 32)이 되게 잡는다 — 가운데가 어긋나 있으면 돌렸을 때 한쪽으로 치우친다(2026-10-08 검수 L-1).
    return (f'<g transform="rotate({rotate} 32 32)"><path d="M12 37 Q32 7 52 37" fill="none" stroke="{INK}" stroke-width="12" stroke-linecap="round"/>'
            f'<circle cx="12" cy="38" r="10" fill="{INK}"/><circle cx="52" cy="38" r="10" fill="{INK}"/></g>')


def speaker() -> str:
    """음량 표시의 스피커. 좌표계 40×56."""
    return f'<path d="M6 20 H17 L33 7 V49 L17 36 H6 Z" fill="{INK}" stroke="{INK}" stroke-width="6" stroke-linejoin="round"/>'


def gloss() -> str:
    """통화 키캡의 광택(흰 타원, 반투명). 좌표계 84×64 — 지름 176 짜리 키캡(윗면 반지름 76)에는 0.7배로 굽는다."""
    return f'<ellipse cx="42" cy="32" rx="36" ry="16" transform="rotate(-28 42 32)" fill="{WHITE}" opacity="0.45"/>'


# ── 굽는 목록 ────────────────────────────────────────────────────────────────────────────────
# (이름, SVG 조각, viewBox(x, y, w, h), 배율, 대칭축 x, 바닥 y) — 대칭축·바닥은 조각의 좌표계 기준.
# 배율은 화면 배치(main/ui.cpp)가 정한 크기다: 거는 중의 상대 1.0, 대기 화면의 상대(hi·sleep) 0.7, 통화 0.88, 전화 옴의 나 0.7·건 쪽 0.625.
# IDLE_SCALE: 대기 화면의 상대 칸(main/ui_layout.h PEER_BODY, 224×232)에 귀가 긴 토끼와 구름까지 들어가는 크기다 — 바꾸면 그 칸과 화면 미리보기를 함께 본다.
# 목록의 순서는 main/toy.cpp 의 표(CHAR)와 같다: 새 자세는 맨 뒤에 더한다.
IDLE_SCALE = 0.7


def _char(prefix, fn, height, base):
    h = height
    return [
        (f"spr_{prefix}_wave",   fn("wave"),   (-12, 0, 264, h),                 1.0,        120, base),
        (f"spr_{prefix}_sleep",  fn("sleep"),  (-12, -6, 288, h + 6),            IDLE_SCALE, 120, base),
        (f"spr_{prefix}_cup",    fn("cup"),    (-12, 0, 264, h),                 0.88,       120, base),
        (f"spr_{prefix}_cheer",  fn("cheer"),  (-12, 0, 264, h),                 0.7,        120, base),
        (f"spr_{prefix}_caller", fn("caller"), (-90, 0, 420, h),                 0.625,      120, base + 3),
        (f"spr_{prefix}_hi",     fn("wave"),   (-12, 0, 264, h),                 IDLE_SCALE, 120, base),
    ]


SPRITES = (
    _char("bear", bear, 260, 246) + _char("rabbit", rabbit, 300, 286) + _char("robot", robot, 260, 246) + [
        ("spr_chick_big",   chick("idle"), (-6, 0, 162, 160),      1.0,  75, 151),
        ("spr_chick_idle",  chick("idle"), (-6, 0, 162, 160),      0.75, 75, 151),
        ("spr_chick_jump",  chick("jump"), (-14, -16, 178, 176),   0.85, 75, 151),
        ("spr_chick_hum",   chick("hum"),  (-6, 0, 190, 160),      0.75, 75, 151),
        ("spr_head_bear",         head_bear(False),   (0, 0, 48, 48), 44 / 48, 24, 48),
        ("spr_head_bear_sleep",   head_bear(True),    (0, 0, 48, 48), 44 / 48, 24, 48),
        ("spr_head_rabbit",       head_rabbit(False), (0, 0, 48, 56), 44 / 56, 24, 56),
        ("spr_head_rabbit_sleep", head_rabbit(True),  (0, 0, 48, 56), 44 / 56, 24, 56),
        ("spr_head_robot",        head_robot(False),  (0, 0, 48, 52), 44 / 52, 24, 52),
        ("spr_head_robot_sleep",  head_robot(True),   (0, 0, 48, 52), 44 / 52, 24, 52),
        ("spr_ico_call",     handset(-135), (0, 0, 64, 64), 68 / 64, 32, 64),
        ("spr_ico_hang",     handset(0),    (0, 0, 64, 64), 68 / 64, 32, 64),
        ("spr_ico_speaker",  speaker(),     (0, 0, 40, 56), 1.0,     20, 56),
        ("spr_ico_note",     note(12, 16),  (0, 0, 36, 40), 1.0,     18, 40),
        ("spr_gloss",        gloss(),       (0, 0, 84, 64), 0.7,     42, 64),
    ]
)

# 동산의 윗선(화면 좌표). 기기는 열마다 이 높이부터 바닥까지 칠한다(toy::background).
HILL_PATH = ((0, 566), (220, 520), (420, 600), (660, 560)), ((660, 560), (900, 522), (1080, 590), (1280, 548))
