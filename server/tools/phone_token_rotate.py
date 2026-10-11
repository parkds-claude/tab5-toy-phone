#!/usr/bin/env python3
"""phone_token_rotate.py — 교환기 토큰을 서버 설정(.env.phone)과 기기 비밀 파일(secrets.h)에 함께 쓴다: 전부 교체 / 기기 한 대 더하기 / 확인.

  python3 server/tools/phone_token_rotate.py                       # 교체: 등록된 **모든** 기기의 토큰을 새로 만든다(.env.phone + secrets.h)
  python3 server/tools/phone_token_rotate.py --relay-url wss://phone.example.com/ws   # 교체하면서 주소 줄도 바꿈
  python3 server/tools/phone_token_rotate.py --add-device stackchan-2 --unit 3 --mac AA:BB:CC:DD:EE:03   # 더하기: 기존 토큰은 그대로 두고 한 대만
  python3 server/tools/phone_token_rotate.py --check               # 두 파일의 토큰이 기기마다 맞는지·주소 종류만 확인(아무것도 쓰지 않음)

P4N-D4 (교체): 개발 중 평문으로 LAN 을 지난 토큰을 버리고 새로 만든다.
  - 출력은 "바꿈/그대로"와 길이·줄 수뿐. 토큰·LAN 주소·Wi-Fi 값·MAC 은 어디에도 찍지 않는다(호스트는 *.example.com 일 때만).
  - 바꾸기 전에 두 파일을 firmware/tab5_phone/out/secrets_backup/<시각>/ 에 보관한다(그 폴더만 git 제외 —
    저장소 다른 자리에 .bak 을 만들면 자동 동기화가 GitHub 로 올린다).
  - 쓰기는 같은 폴더의 임시 파일 → os.replace(중간에 죽어도 반쪽 파일이 안 남는다), 권한은 원래 파일 그대로.
  - 두 파일의 새 내용을 먼저 다 만들어 검증한 뒤에야 보관·쓰기를 시작한다(한쪽만 바뀐 채 끝나지 않게).
  비용: 되돌리려면 보관 폴더의 파일을 손으로 복사. 탈출구: 보관 폴더의 두 파일을 원래 자리에 되돌린다.
  !! 교체는 모든 기기의 토큰을 바꾼다 → 모든 기기에 새 secrets.h 를 다시 구워야 한다. 다시 올릴 수 없는 기기(집에 있는 1번)가 있는 동안에는 쓰지 않는다.

TW-ADD (더하기 — 설계서 docs/exec-plans/2026-10-08-phone-3way-design.md §3·§7, 기기 3대째 = 스택짱):
  새 토큰 하나를 만들어 ① .env.phone 의 PHONE_TOKENS 값 **끝에** `,<기기ID>:<토큰>` 을 붙이고 ② secrets.h 의 PHONE_DEVICES 표 마지막 기기 줄 뒤에
  `{ "<MAC>", <번호>, "<기기ID>", "<토큰>" }, \\` 한 줄을 넣는다. 그 밖의 글자는 건드리지 않는다.
  이유: 1번 Tab5 는 집에 있어 다시 올릴 수 없다 — 기존 토큰이 한 글자라도 바뀌면 그 기기가 "기기 등록 오류"로 끊긴다. 끝에 붙이는 까닭은
        PHONE_TOKENS 의 적힌 순서가 옛 짝의 기본값(앞 두 대)이기 때문이다(phone_core.parse_legacy_pair).
  지키는 것: 쓰기 전에 새 글을 다시 읽어 "기존 기기의 ID·토큰·순서가 그대로이고 늘어난 것은 그 한 대뿐"임을 확인한다(교환기의 해석기로). 어긋나면 아무것도 쓰지 않는다.
        이미 있는 기기면 아무것도 바꾸지 않고 그렇다고 말한다(한쪽 파일에만 있어도 스스로 맞추지 않는다 — 어느 쪽 토큰이 기기에 구워졌는지 도구는 모른다).
  비용: 표가 "기기 한 줄에 하나, 줄 끝 역슬래시" 모양(secrets.h.example)이 아니면 거절한다. 탈출구: 보관 폴더의 두 파일을 되돌리거나, 두 파일에 손으로 한 줄씩 넣고 --check.

끝나는 값: 0 성공(더하기에서 "이미 있음"도 0) · 1 파일/형식 문제 · 2 인자가 틀림.
교체·더하기 뒤에는: 바뀐 기기에 새 secrets.h 로 펌웨어를 다시 굽고, 교환기가 도는 기기(맥미니)의 .env.phone 이 같은 값인지 확인한 뒤 교환기를 재시작한다
(토큰은 교환기가 뜰 때 한 번 읽는다 — 재시작 전에는 새 기기가 401 로 거절된다). 두 파일은 git 에 오르지 않으니 다른 기기(맥북)에는 파일째로 옮긴다.
"""
import argparse
import os
import re
import secrets
import stat
import sys
import tempfile
import time
from pathlib import Path
from urllib.parse import urlparse

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import phone_core   # 표준 라이브러리만 쓰는 순수 모듈 — 교환기와 같은 규칙(기기 수·토큰 길이·순서)으로 검사하려고 가져온다

REPO = Path(__file__).resolve().parents[2]
DEFAULT_ENV = REPO / "server" / ".env.phone"
DEFAULT_SECRETS = REPO / "firmware" / "tab5_phone" / "main" / "secrets.h"
DEFAULT_BACKUP = REPO / "firmware" / "tab5_phone" / "out" / "secrets_backup"
DEVICES = ("tab5-1", "tab5-2")       # 두 파일 어디에도 기기가 없을 때(첫 설치)의 기본값

_URL_LINE = re.compile(r'^(\s*#define\s+RELAY_URL\s+)"[^"\n]*"', re.M)
_TABLE_HEAD = re.compile(r'[ \t]*#[ \t]*define[ \t]+PHONE_DEVICES\b')
# 기기 표의 한 줄: { "MAC", 번호, "기기ID", "토큰" }
_ROW = re.compile(r'\{\s*"([^"\\\n]*)"\s*,\s*(\d+)\s*,\s*"([^"\\\n]*)"\s*,\s*"([^"\\\n]*)"\s*\}')
# 더할 기기 ID: 영문자로 시작, 영문·숫자·-·_, 15자까지.
#   이유: 이 글자는 설정 줄(`ID:토큰,` — 쉼표·쌍점·공백이 구분자)과 C 문자열(따옴표·역슬래시)에 그대로 들어가고, 펌웨어가 신호에서 읽는 칸이 16바이트다
#         (설계서 §8 relay_proto.h Signal.peer_id[16] = 15자 + 끝 표시). 비용: 더 긴 이름은 못 쓴다. 탈출구: 펌웨어의 칸을 늘린 뒤 이 식의 14 를 고친다.
_DEVICE_ID = re.compile(r"[A-Za-z][A-Za-z0-9_-]{0,14}", re.ASCII)
_MAC = re.compile(r"(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}", re.ASCII)
UNIT_MIN, UNIT_MAX = 1, phone_core.MAX_DEVICES     # 기기 번호(화면에 보이는 순서, TW-D4). 등록이 8대까지이므로 1~8


def _dev_re(dev):
    """secrets.h 의 기기 표 줄에서 `"tab5-1", "토큰"` 의 토큰 글자만 잡는다."""
    return re.compile(r'(,\s*"%s"\s*,\s*)"[^"\\\n]*"' % re.escape(dev))


class FormatError(Exception):
    pass


def new_tokens(devices=DEVICES):
    return {d: secrets.token_urlsafe(32) for d in devices}


# ── .env.phone ──────────────────────────────────────────────────────────────
def _tokens_line_index(lines):
    """PHONE_TOKENS 줄의 번호. 없으면 None, 둘 이상이면 형식 오류."""
    hits = [i for i, l in enumerate(lines) if l.strip().startswith("PHONE_TOKENS=")]
    if len(hits) > 1:
        raise FormatError("PHONE_TOKENS 줄이 둘 이상")
    return hits[0] if hits else None


def _env_value(line):
    """`PHONE_TOKENS=값` 줄의 값. 따옴표로 감싼 값은 벗긴다 — 교환기(phone_relay.load_env_file)가 읽는 것과 같은 규칙."""
    v = line.split("=", 1)[1].strip()
    if len(v) >= 2 and v[0] == v[-1] and v[0] in "\"'":
        v = v[1:-1]
    return v


def parse_env_tokens(text):
    """.env.phone 글 → {기기ID: 토큰}(적힌 순서). 줄이 없거나 못 읽으면 빈 표(여기서는 검사하지 않는다 — 검사는 phone_core.parse_tokens)."""
    for l in text.splitlines():
        if l.strip().startswith("PHONE_TOKENS="):
            return dict((d.strip(), t.strip()) for d, t in (p.split(":", 1) for p in _env_value(l).split(",") if ":" in p))
    return {}


def update_env_text(text, toks):
    """.env.phone 글: PHONE_TOKENS 줄만 바꾼다(없으면 끝에 덧붙임, 파일이 비었으면 PORT·BIND 와 함께 새로). toks 의 순서대로 쓴다."""
    line = "PHONE_TOKENS=" + ",".join(f"{d}:{t}" for d, t in toks.items())
    if not text.strip():
        return f"PORT=5094\nBIND=127.0.0.1\n{line}\n"
    lines = text.split("\n")
    hit = _tokens_line_index(lines)
    if hit is not None:
        lines[hit] = line
        return "\n".join(lines)
    return text + ("" if text.endswith("\n") else "\n") + line + "\n"


def add_env_token(text, dev, token):
    """.env.phone 글: PHONE_TOKENS 값의 끝에 `,기기ID:토큰` 을 붙인다. 그 줄의 나머지 글자(따옴표·앞뒤 공백)와 다른 줄은 그대로."""
    lines = text.split("\n")
    hit = _tokens_line_index(lines)
    if hit is None:
        raise FormatError(".env.phone 에 PHONE_TOKENS 줄이 없음(더하기는 이미 등록된 기기가 있어야 한다)")
    m = re.fullmatch(r'(\s*PHONE_TOKENS=\s*)(["\']?)(.*?)(\2)(\s*)', lines[hit])
    if m is None:                                               # 따옴표가 한쪽만 있는 줄 등 — 값을 문구에 싣지 않는다(토큰이 들어 있다)
        raise FormatError(".env.phone 의 PHONE_TOKENS 줄 모양을 읽지 못함")
    head, quote, value, _, tail = m.groups()
    sep = "" if value.rstrip().endswith(",") else ","          # 값이 쉼표로 끝나 있으면 겹쳐 쓰지 않는다
    lines[hit] = f"{head}{quote}{value}{sep}{dev}:{token}{quote}{tail}"
    return "\n".join(lines)


# ── secrets.h ───────────────────────────────────────────────────────────────
def read_table(text):
    """secrets.h 글에서 PHONE_DEVICES 표를 찾는다 → (줄 목록, 표의 첫 줄 번호, 끝 줄 번호, [(줄 번호, MAC, 번호, 기기ID, 토큰)]).
    표는 #define 한 개이고 줄 끝 역슬래시로 이어진다. 정의가 없거나 둘이거나 괄호 짝이 안 맞으면(파일 끝에서 끊김) 형식 오류."""
    lines = text.split("\n")
    heads = [i for i, l in enumerate(lines) if _TABLE_HEAD.match(l)]
    if len(heads) != 1:
        raise FormatError(f"secrets.h 에서 PHONE_DEVICES 정의를 {len(heads)}개 찾음(1개여야 함)")
    start = end = heads[0]
    while lines[end].rstrip().endswith("\\") and end + 1 < len(lines):
        end += 1
    body = "\n".join(lines[start:end + 1])
    if body.count("{") != body.count("}") or body.count("{") == 0:
        raise FormatError("secrets.h 의 PHONE_DEVICES 표가 닫히지 않았거나 모양을 읽지 못함")
    rows = [(i, m.group(1), int(m.group(2)), m.group(3), m.group(4)) for i in range(start, end + 1) for m in _ROW.finditer(lines[i])]
    return lines, start, end, rows


def parse_secrets_tokens(text):
    """secrets.h 글 → {기기ID: 토큰}(표에 적힌 순서). 표를 못 읽으면 빈 표."""
    try:
        return {dev: tok for _, _, _, dev, tok in read_table(text)[3]}
    except FormatError:
        return {}


def update_secrets_text(text, toks, relay_url=None):
    """secrets.h 글: 기기 표에서 toks 에 든 기기들의 토큰 문자열(과 선택해서 RELAY_URL)만 바꾼다."""
    for d, tok in toks.items():
        text, n = _dev_re(d).subn(lambda m: m.group(1) + '"' + tok + '"', text)
        if n != 1:
            raise FormatError(f"secrets.h 기기 표에서 {d} 줄을 {n}개 찾음(1개여야 함)")
    if relay_url is not None:
        text, n = _URL_LINE.subn(lambda m: m.group(1) + '"' + relay_url + '"', text)
        if n != 1:
            raise FormatError(f"secrets.h 에서 RELAY_URL 줄을 {n}개 찾음(1개여야 함)")
    return text


def add_secrets_row(text, mac, unit, dev, token):
    """secrets.h 글: PHONE_DEVICES 표의 마지막 기기 줄 뒤에 한 줄을 넣는다(들여쓰기는 그 줄을 따른다). 기존 줄은 그대로 —
    다만 마지막 기기 줄 끝에 쉼표가 없으면 쉼표 하나를 넣는다(안 넣으면 컴파일이 깨진다)."""
    lines, start, end, rows = read_table(text)
    if not rows:
        raise FormatError("secrets.h 의 PHONE_DEVICES 표에서 기기 줄을 찾지 못함")
    per_line = {}
    for i, *_ in rows:
        per_line[i] = per_line.get(i, 0) + 1
    last = rows[-1][0]
    # 지원하는 모양: 기기 한 줄에 하나, 그 줄은 역슬래시로 끝나고 표의 닫는 괄호는 그 아래 줄에 있다(secrets.h.example).
    if any(n != 1 for n in per_line.values()) or last >= end or not lines[last].rstrip().endswith("\\"):
        raise FormatError("secrets.h 의 PHONE_DEVICES 표가 '기기 한 줄에 하나, 줄 끝 역슬래시' 모양이 아님 — 손으로 한 줄 넣고 --check 로 확인")
    m = list(_ROW.finditer(lines[last]))[-1]
    if not lines[last][m.end():].lstrip().startswith(","):     # 마지막 줄에 쉼표가 없다 → 닫는 괄호 바로 뒤에 넣는다
        lines[last] = lines[last][:m.end()] + "," + lines[last][m.end():]
    indent = re.match(r"[ \t]*", lines[last]).group(0)
    lines.insert(last + 1, f'{indent}{{ "{mac}", {unit}, "{dev}", "{token}" }}, \\')
    return "\n".join(lines)


def valid_relay_url(url):
    """ws:// 또는 wss:// + 호스트, 따옴표·역슬래시·공백 없음(C 문자열을 깨뜨리지 않게)."""
    if not url or any(c in url for c in ' \t\n"\\'):
        return False
    u = urlparse(url)
    return u.scheme in ("ws", "wss") and bool(u.hostname)


# ── 쓰기·보관 ───────────────────────────────────────────────────────────────
def atomic_write(path, text, tmp_dir=None):
    """임시 파일 → os.replace. 권한은 원래 파일 그대로(없던 파일은 0600).
    tmp_dir: 임시 파일을 만들 폴더. 대상 폴더(server/·main/)에 만들면 `.이름.XXXX` 가 gitignore 에 안 걸려, 도중에 죽을 때 새 토큰이 자동 동기화로 GitHub 에 오른다 —
    그래서 rotate 는 보관 폴더(out/ 아래, git 제외·같은 파일시스템)를 넘긴다. 없으면 대상 폴더(단독 사용·시험용)."""
    path = Path(path)
    mode = stat.S_IMODE(path.stat().st_mode) if path.exists() else 0o600
    fd, tmp = tempfile.mkstemp(prefix="." + path.name + ".", dir=str(tmp_dir or path.parent))
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            f.write(text)
            f.flush()
            os.fsync(f.fileno())
        os.chmod(tmp, mode)
        os.replace(tmp, path)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise


def backup(files, root):
    """바꾸기 전 보관. 폴더·파일 모두 소유자만 읽게(0700/0600). 없던 파일은 건너뛴다."""
    dest = Path(root) / time.strftime("%Y%m%d_%H%M%S")
    n = 0
    while dest.exists():                       # 같은 초에 두 번 실행해도 덮어쓰지 않는다
        n += 1
        dest = Path(root) / (time.strftime("%Y%m%d_%H%M%S") + f"_{n}")
    dest.mkdir(parents=True, mode=0o700)
    for p in files:
        p = Path(p)
        if p.exists():
            out = dest / p.name
            out.write_text(p.read_text(encoding="utf-8"), encoding="utf-8")
            os.chmod(out, 0o600)
    return dest


def commit(env_path, sec_path, new_env, new_sec, backup_root):
    """검증이 끝난 두 글을 보관 → .env.phone → secrets.h 순서로 쓴다(교체·더하기 공용). 성공하면 보관 폴더, 실패하면 None(안내는 여기서 찍는다).
    둘째 쓰기가 실패하면 첫 파일을 보관본으로 되돌린다 — 두 파일의 토큰이 어긋난 채 끝나지 않게(값은 출력하지 않는다)."""
    dest = backup([env_path, sec_path], backup_root)
    had_env = env_path.exists()
    atomic_write(env_path, new_env, dest)
    try:
        atomic_write(sec_path, new_sec, dest)
    except BaseException:
        try:
            if had_env:
                atomic_write(env_path, (dest / env_path.name).read_text(encoding="utf-8"), dest)
            else:
                env_path.unlink()
            print(f"실패: secrets.h 쓰기 오류 — 두 파일을 원래대로 되돌림(보관: {dest})")
        except Exception:                      # noqa: BLE001 — 되돌리기까지 실패하면 사람이 보관본으로 복구해야 한다
            print(f"실패: secrets.h 쓰기 오류, 되돌리기도 실패 — 보관 폴더의 두 파일을 손으로 복사: {dest}")
        return None
    return dest


# ── 낱말: 확인 ──────────────────────────────────────────────────────────────
def relay_url_kind(text):
    """(scheme, 호스트 또는 "LAN/기타"). 호스트는 *.example.com 일 때만 내보낸다."""
    m = re.search(r'^\s*#define\s+RELAY_URL\s+"([^"\n]*)"', text, re.M)
    if not m:
        return None
    u = urlparse(m.group(1))
    host = u.hostname or ""
    return u.scheme or "?", host if host.endswith(".example.com") else "LAN/기타"


def check(env_path, sec_path):
    try:
        e = parse_env_tokens(Path(env_path).read_text(encoding="utf-8"))
        stext = Path(sec_path).read_text(encoding="utf-8")
    except OSError as ex:
        print(f"확인 불가: 파일을 읽지 못함({type(ex).__name__})")
        return 1
    s = parse_secrets_tokens(stext)
    devices = list(e) + [d for d in s if d not in e] or list(DEVICES)     # 두 파일에 나오는 기기 전부(설정에 적힌 순서 먼저). 하나도 없으면 기본 두 대
    ok = True
    for d in devices:
        same = bool(e.get(d)) and e.get(d) == s.get(d)
        ok &= same
        print(f"{d}: {'일치' if same else '불일치'}" + ("" if e.get(d) and s.get(d) else " (한쪽에 없음)"))
    kind = relay_url_kind(stext)
    print("RELAY_URL: " + (f"{kind[0]}, {kind[1]}" if kind else "줄 없음"))
    return 0 if ok and kind else 1


# ── 낱말: 교체 ──────────────────────────────────────────────────────────────
def rotate(env_path, sec_path, backup_root, relay_url):
    env_path, sec_path = Path(env_path), Path(sec_path)
    try:
        stext = sec_path.read_text(encoding="utf-8")
        etext = env_path.read_text(encoding="utf-8") if env_path.exists() else ""
    except OSError as ex:
        print(f"실패: secrets.h 를 읽지 못함({type(ex).__name__})")
        return 1
    # 바꿀 기기 = 교환기에 등록된 기기 전부(.env.phone 에 적힌 순서 — 옛 짝의 근거라 순서를 지킨다). 설정이 아직 없으면 secrets.h 의 표, 그것도 없으면 기본 두 대.
    #   2대만 알던 때처럼 정해진 두 대만 다시 쓰면 셋째 기기가 .env.phone 에서 떨어져 나가 교환기에서 거절된다.
    devices = list(parse_env_tokens(etext)) or list(parse_secrets_tokens(stext)) or list(DEVICES)
    toks = new_tokens(devices)
    try:                                       # 먼저 둘 다 만들어 검증 — 하나라도 틀리면 아무것도 쓰지 않는다
        new_env = update_env_text(etext, toks)
        new_sec = update_secrets_text(stext, toks, relay_url)
    except FormatError as ex:
        print(f"실패: {ex} — 아무것도 바꾸지 않음")
        return 1
    dest = commit(env_path, sec_path, new_env, new_sec, backup_root)
    if dest is None:
        return 1
    print(f"토큰 바꿈: {len(devices)}개(각 {len(toks[devices[0]])}자), .env.phone {len(new_env.splitlines())}줄, secrets.h {len(new_sec.splitlines())}줄")
    print("RELAY_URL: " + ("바꿈" if relay_url else "그대로"))
    print(f"보관: {dest}")
    return 0


# ── 낱말: 기기 한 대 더하기 (TW-ADD) ────────────────────────────────────────
def _registered(etext):
    """지금의 PHONE_TOKENS 를 교환기와 같은 규칙으로 읽는다 → {기기ID: 토큰}. 줄이 없거나 교환기가 읽지 못하는 값이면 형식 오류."""
    lines = etext.split("\n")
    hit = _tokens_line_index(lines)
    if hit is None:
        raise FormatError(".env.phone 에 PHONE_TOKENS 줄이 없음(더하기는 이미 등록된 기기가 있어야 한다 — 처음이면 인자 없이 실행)")
    try:
        return phone_core.parse_tokens(_env_value(lines[hit]))
    except ValueError as ex:                   # parse_tokens 의 문구에는 토큰이 없다
        raise FormatError(f"지금의 PHONE_TOKENS 줄을 교환기 규칙으로 읽지 못함({ex})")


def _verify_only_added(etext, stext, new_env, new_sec, dev, token, mac, unit):
    """쓰기 전의 마지막 확인: 새 두 글에서 기존 기기의 ID·토큰·순서가 그대로이고, 늘어난 것은 그 한 대(맨 끝)뿐인가. 아니면 형식 오류.
    이유: 기존 토큰이 한 글자라도 바뀌면 다시 올릴 수 없는 기기가 끊긴다 — 글을 만드는 쪽에 버그가 있어도 틀린 글은 디스크에 닿지 않게 한다."""
    old_env, old_rows = _registered(etext), read_table(stext)[3]
    new_env_toks, new_rows = _registered(new_env), read_table(new_sec)[3]
    if list(new_env_toks.items()) != [*old_env.items(), (dev, token)]:
        raise FormatError("안전 확인 실패: 새 .env.phone 에서 기존 기기의 ID·토큰·순서가 그대로가 아님")
    if [r[1:] for r in new_rows] != [*[r[1:] for r in old_rows], (mac, unit, dev, token)]:
        raise FormatError("안전 확인 실패: 새 secrets.h 에서 기존 기기 줄이 그대로가 아님")


def add_device(env_path, sec_path, backup_root, dev, unit, mac):
    env_path, sec_path = Path(env_path), Path(sec_path)
    try:
        etext = env_path.read_text(encoding="utf-8")
        stext = sec_path.read_text(encoding="utf-8")
    except OSError as ex:
        print(f"실패: 두 파일을 읽지 못함({type(ex).__name__}) — 아무것도 바꾸지 않음")
        return 1
    try:
        rows = read_table(stext)[3]
        in_env, in_sec = dev in parse_env_tokens(etext), dev in {r[3] for r in rows}
        if in_env and in_sec:                  # 이미 있는 기기 → 아무것도 바꾸지 않고 그렇다고 말한다(토큰이 서로 다른 것만 알린다)
            same = parse_env_tokens(etext)[dev] == parse_secrets_tokens(stext).get(dev)
            print(f"{dev}: 이미 등록된 기기 — 아무것도 바꾸지 않음" + ("" if same else " (주의: 두 파일의 토큰이 서로 다름 — --check 로 확인)"))
            return 0 if same else 1
        if in_env or in_sec:
            print(f"{dev}: 한쪽 파일에만 있음(.env.phone {'있음' if in_env else '없음'} · secrets.h {'있음' if in_sec else '없음'}) — 아무것도 바꾸지 않음. "
                  "다른 기기(맥북·맥미니)의 파일을 가져와 맞추거나 그 줄을 손으로 지운 뒤 다시 실행")
            return 1
        registered = _registered(etext)
        if len(registered) >= phone_core.MAX_DEVICES:
            raise FormatError(f"등록 기기는 {phone_core.MAX_DEVICES}대까지(지금 {len(registered)}대)")
        if unit in {r[2] for r in rows}:
            raise FormatError(f"번호 {unit} 은(는) 이미 표에 있음")
        if mac in {r[1].upper() for r in rows}:
            raise FormatError("그 MAC 은 이미 표에 있음")
        token = secrets.token_urlsafe(32)
        new_env = add_env_token(etext, dev, token)
        new_sec = add_secrets_row(stext, mac, unit, dev, token)
        _verify_only_added(etext, stext, new_env, new_sec, dev, token, mac, unit)
    except FormatError as ex:
        print(f"실패: {ex} — 아무것도 바꾸지 않음")
        return 1
    dest = commit(env_path, sec_path, new_env, new_sec, backup_root)
    if dest is None:
        return 1
    print(f"기기 더함: {dev}(번호 {unit}) — 새 토큰 1개({len(token)}자), .env.phone {len(new_env.splitlines())}줄, secrets.h {len(new_sec.splitlines())}줄")
    print(f"기존 기기 {len(registered)}대의 토큰: 그대로")
    print(f"보관: {dest}")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true", help="토큰 일치·주소 종류만 확인(쓰지 않음)")
    ap.add_argument("--relay-url", help="RELAY_URL 줄도 이 주소(ws:// 또는 wss://)로 바꿈")
    ap.add_argument("--add-device", metavar="기기ID", help="기존 토큰은 그대로 두고 이 기기 한 대를 더함(--unit·--mac 과 함께)")
    ap.add_argument("--unit", help=f"더할 기기의 번호({UNIT_MIN}~{UNIT_MAX}, 화면에 보이는 순서)")
    ap.add_argument("--mac", help="더할 기기의 MAC(AA:BB:CC:DD:EE:FF)")
    ap.add_argument("--env", default=str(DEFAULT_ENV), help="서버 설정 파일(시험용)")
    ap.add_argument("--secrets", default=str(DEFAULT_SECRETS), help="기기 비밀 파일(시험용)")
    ap.add_argument("--backup-dir", default=str(DEFAULT_BACKUP), help="보관 폴더(시험용)")
    args = ap.parse_args(argv)
    if args.add_device is None and (args.unit is not None or args.mac is not None):
        print("인자 오류: --unit·--mac 은 --add-device 와 함께만 쓴다")
        return 2
    if args.add_device is not None:
        if args.check or args.relay_url is not None:
            print("인자 오류: --add-device 는 --check·--relay-url 과 함께 쓰지 않는다")
            return 2
        if not _DEVICE_ID.fullmatch(args.add_device):
            print("인자 오류: 기기 ID 는 영문자로 시작하는 영문·숫자·-·_ 15자 이내")
            return 2
        if args.unit is None or args.mac is None:
            print("인자 오류: --add-device 에는 --unit <번호> 와 --mac <MAC> 이 함께 있어야 함")
            return 2
        if not re.fullmatch(r"\d{1,2}", args.unit, re.ASCII) or not UNIT_MIN <= int(args.unit) <= UNIT_MAX:
            print(f"인자 오류: --unit 은 {UNIT_MIN}~{UNIT_MAX} 의 수")
            return 2
        if not _MAC.fullmatch(args.mac):
            print("인자 오류: --mac 은 AA:BB:CC:DD:EE:FF 모양(16진수 두 자리 여섯 묶음)")
            return 2
        return add_device(args.env, args.secrets, args.backup_dir, args.add_device, int(args.unit), args.mac.upper())
    if args.check:
        return check(args.env, args.secrets)
    if args.relay_url is not None and not valid_relay_url(args.relay_url):
        print("인자 오류: --relay-url 은 ws:// 또는 wss:// 주소(공백·따옴표·역슬래시 없음)여야 함")
        return 2
    return rotate(args.env, args.secrets, args.backup_dir, args.relay_url)


if __name__ == "__main__":
    sys.exit(main())
