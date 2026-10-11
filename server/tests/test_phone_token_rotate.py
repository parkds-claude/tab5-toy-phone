"""phone_token_rotate(토큰 교체 도구) 시험 — 전부 임시 파일로. 진짜 .env.phone·secrets.h 는 열지 않는다.
실행: cd server && python3 -m pytest tests/test_phone_token_rotate.py -q
"""
import os
import stat
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
import phone_token_rotate as rot

SECRETS = '''// 비밀
#pragma once
#define WIFI_NETWORKS { {"my-ssid", "my-password"} }
#define RELAY_URL "ws://192.168.0.10:5094/ws"
#define PHONE_DEVICES { \\
    { "AA:BB:CC:DD:EE:01", 1, "tab5-1", "old-token-one-0123456789" }, \\
    { "AA:BB:CC:DD:EE:02", 2, "tab5-2", "old-token-two-0123456789" }, \\
}
'''
ENV = "# 설정\nPORT=5099\nPHONE_TOKENS=tab5-1:old-token-one-0123456789,tab5-2:old-token-two-0123456789\nBIND=0.0.0.0\n"


@pytest.fixture
def files(tmp_path):
    env, sec = tmp_path / ".env.phone", tmp_path / "secrets.h"
    env.write_text(ENV, encoding="utf-8")
    env.chmod(0o600)
    sec.write_text(SECRETS, encoding="utf-8")
    return env, sec, tmp_path / "bk"


def run(files, *extra, capsys=None):
    env, sec, bk = files
    rc = rot.main(["--env", str(env), "--secrets", str(sec), "--backup-dir", str(bk), *extra])
    return rc, (capsys.readouterr().out if capsys else "")


def test_rotate_changes_tokens_in_both_files_and_keeps_other_lines(files, capsys):
    env, sec, bk = files
    rc, out = run(files, capsys=capsys)
    assert rc == 0
    e, s = env.read_text(encoding="utf-8"), sec.read_text(encoding="utf-8")
    assert "old-token" not in e and "old-token" not in s
    assert e.splitlines()[0] == "# 설정" and "PORT=5099" in e and "BIND=0.0.0.0" in e   # 다른 줄은 그대로
    toks = dict(p.split(":", 1) for p in [l for l in e.splitlines() if l.startswith("PHONE_TOKENS=")][0][13:].split(","))
    assert set(toks) == {"tab5-1", "tab5-2"} and toks["tab5-1"] != toks["tab5-2"] and all(len(v) >= 32 for v in toks.values())
    assert f'"tab5-1", "{toks["tab5-1"]}" }}' in s and f'"tab5-2", "{toks["tab5-2"]}" }}' in s
    assert 'RELAY_URL "ws://192.168.0.10:5094/ws"' in s and "my-password" in s            # 주소·Wi-Fi 는 그대로
    assert s.count("\\\n") == SECRETS.count("\\\n")                                       # 줄 이음 모양 유지
    for secret in (*toks.values(), "192.168", "my-password", "my-ssid", "old-token"):    # 출력에는 값이 없다
        assert secret not in out
    assert "바꿈" in out


def test_backup_is_made_before_change(files, capsys):
    env, sec, bk = files
    run(files, capsys=capsys)
    dirs = list(bk.iterdir())
    assert len(dirs) == 1
    assert (dirs[0] / ".env.phone").read_text(encoding="utf-8") == ENV
    assert (dirs[0] / "secrets.h").read_text(encoding="utf-8") == SECRETS
    assert stat.S_IMODE((dirs[0] / ".env.phone").stat().st_mode) == 0o600


def test_file_mode_is_kept_and_no_temp_files_left(files, capsys):
    env, sec, bk = files
    sec.chmod(0o640)
    run(files, capsys=capsys)
    assert stat.S_IMODE(env.stat().st_mode) == 0o600 and stat.S_IMODE(sec.stat().st_mode) == 0o640
    assert sorted(p.name for p in env.parent.iterdir()) == [".env.phone", "bk", "secrets.h"]


def test_relay_url_option_changes_url_line(files, capsys):
    env, sec, bk = files
    rc, out = run(files, "--relay-url", "wss://phone.example.com/ws", capsys=capsys)
    assert rc == 0
    assert 'RELAY_URL "wss://phone.example.com/ws"' in sec.read_text(encoding="utf-8")
    assert "192.168" not in out


@pytest.mark.parametrize("bad", ["http://x/ws", "wss://a b/ws", 'wss://a"b/ws', "wss://a\\b/ws", "ws://", ""])
def test_bad_relay_url_is_refused_and_nothing_changes(files, capsys, bad):
    env, sec, bk = files
    rc, _ = run(files, "--relay-url", bad, capsys=capsys)
    assert rc == 2
    assert env.read_text(encoding="utf-8") == ENV and sec.read_text(encoding="utf-8") == SECRETS and not bk.exists()


def test_missing_env_is_created_with_defaults(tmp_path, capsys):
    env, sec = tmp_path / ".env.phone", tmp_path / "secrets.h"
    sec.write_text(SECRETS, encoding="utf-8")
    rc = rot.main(["--env", str(env), "--secrets", str(sec), "--backup-dir", str(tmp_path / "bk")])
    assert rc == 0
    lines = env.read_text(encoding="utf-8").splitlines()
    assert "PORT=5094" in lines and "BIND=127.0.0.1" in lines and any(l.startswith("PHONE_TOKENS=tab5-1:") for l in lines)
    assert stat.S_IMODE(env.stat().st_mode) == 0o600
    assert [p.name for p in (tmp_path / "bk").iterdir() and list((tmp_path / "bk").iterdir())[0].iterdir()] == ["secrets.h"]   # 없던 파일은 보관할 것도 없다


def test_env_without_tokens_line_gets_one_appended(files, capsys):
    env, sec, bk = files
    env.write_text("PORT=5094", encoding="utf-8")                  # 끝 줄바꿈이 없어도
    run(files, capsys=capsys)
    lines = env.read_text(encoding="utf-8").splitlines()
    assert lines[0] == "PORT=5094" and lines[1].startswith("PHONE_TOKENS=")


def test_secrets_without_device_line_fails_and_nothing_changes(files, capsys):
    env, sec, bk = files
    broken = SECRETS.replace('"tab5-2"', '"tab5-9"')
    sec.write_text(broken, encoding="utf-8")
    rc, _ = run(files, capsys=capsys)
    assert rc == 1
    assert env.read_text(encoding="utf-8") == ENV and sec.read_text(encoding="utf-8") == broken and not bk.exists()


def test_tokens_are_random_each_run(files, capsys):
    env, sec, bk = files
    run(files, capsys=capsys)
    first = env.read_text(encoding="utf-8")
    run(files, capsys=capsys)
    assert env.read_text(encoding="utf-8") != first


# ── --check ─────────────────────────────────────────────────────────────────
def test_check_reports_match_without_values(files, capsys):
    env, sec, bk = files
    rc, out = run(files, "--check", capsys=capsys)
    assert rc == 0 and "일치" in out and "불일치" not in out
    for secret in ("old-token", "192.168", "my-password"):
        assert secret not in out
    assert "LAN/기타" in out and "ws" in out
    assert not bk.exists()                                          # --check 는 아무것도 쓰지 않는다


def test_check_reports_mismatch(files, capsys):
    env, sec, bk = files
    sec.write_text(SECRETS.replace("old-token-two-0123456789", "other-token-two-012345678"), encoding="utf-8")
    rc, out = run(files, "--check", capsys=capsys)
    assert rc == 1
    assert "tab5-1: 일치" in out and "tab5-2: 불일치" in out and "other-token" not in out


def test_check_shows_host_only_for_known_host(files, capsys):
    env, sec, bk = files
    run(files, "--relay-url", "wss://phone.example.com/ws", capsys=capsys)
    rc, out = run(files, "--check", capsys=capsys)
    assert rc == 0 and "wss" in out and "phone.example.com" in out
    run(files, "--relay-url", "wss://evil.other.net/ws", capsys=capsys)
    rc, out = run(files, "--check", capsys=capsys)
    assert "evil.other.net" not in out and "LAN/기타" in out


def test_check_with_missing_file_is_a_clear_failure(tmp_path, capsys):
    rc = rot.main(["--env", str(tmp_path / "no"), "--secrets", str(tmp_path / "no2"), "--check"])
    assert rc == 1


# ── 검수 2·4 ────────────────────────────────────────────────────────────────
def test_second_write_failure_restores_first_file_and_fails(files, capsys, monkeypatch):
    """.env.phone 을 쓴 뒤 secrets.h 쓰기가 실패하면 보관본으로 되돌리고 값 없는 한 줄과 0 이 아닌 값으로 끝낸다."""
    env, sec, bk = files
    real = rot.atomic_write
    calls = []

    def flaky(path, text, tmp_dir=None):
        calls.append(Path(path).name)
        if Path(path).name == "secrets.h":
            raise OSError("디스크 가득")
        return real(path, text, tmp_dir) if tmp_dir else real(path, text)

    monkeypatch.setattr(rot, "atomic_write", flaky)
    rc, out = run(files, capsys=capsys)
    assert rc != 0
    assert calls[:2] == [".env.phone", "secrets.h"]
    assert env.read_text(encoding="utf-8") == ENV and sec.read_text(encoding="utf-8") == SECRETS
    assert "원래대로 되돌림" in out and "old-token" not in out and "디스크" not in out


def test_second_write_failure_removes_newly_created_env(tmp_path, capsys, monkeypatch):
    env, sec = tmp_path / ".env.phone", tmp_path / "secrets.h"
    sec.write_text(SECRETS, encoding="utf-8")
    real = rot.atomic_write

    def flaky(path, text, tmp_dir=None):
        if Path(path).name == "secrets.h":
            raise OSError("x")
        return real(path, text, tmp_dir)

    monkeypatch.setattr(rot, "atomic_write", flaky)
    rc = rot.main(["--env", str(env), "--secrets", str(sec), "--backup-dir", str(tmp_path / "bk")])
    assert rc != 0 and not env.exists() and sec.read_text(encoding="utf-8") == SECRETS


def test_temp_files_are_not_created_in_target_folders(files, capsys, monkeypatch):
    """검수 4: 임시 파일이 대상 폴더(gitignore 에 안 걸리는 곳)에 생기면 도중에 죽을 때 새 토큰이 GitHub 로 간다 — 보관 폴더(git 제외) 아래에서만 만든다."""
    env, sec, bk = files
    seen = []
    real_replace = os.replace

    def spy(src, dst):
        seen.append((sorted(p.name for p in env.parent.iterdir()), str(src)))
        return real_replace(src, dst)

    monkeypatch.setattr(os, "replace", spy)
    rc, _ = run(files, capsys=capsys)
    assert rc == 0 and len(seen) == 2
    for listing, src in seen:
        assert listing == [".env.phone", "bk", "secrets.h"], listing       # 대상 폴더에는 임시 파일이 없다
        assert str(bk) in src


# ════════════════════════════════════════════════════════════════════════════
# 기기 한 대 더하기 (--add-device) · 3대 확인 · 3대 교체 — docs/exec-plans/2026-10-08-phone-3way-design.md §3·§7
# 1번 Tab5 는 집에 있어 다시 올릴 수 없다 → 기존 토큰은 한 글자도 바뀌면 안 된다. 여기 시험이 그것을 고정한다.
# ════════════════════════════════════════════════════════════════════════════
import re  # noqa: E402
import shutil  # noqa: E402
import subprocess  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import phone_core  # noqa: E402

MAC3 = "AA:BB:CC:DD:EE:03"
ADD = ["--add-device", "stackchan-2", "--unit", "3", "--mac", MAC3]
OLD_ENV_LINE = "PHONE_TOKENS=tab5-1:old-token-one-0123456789,tab5-2:old-token-two-0123456789"
OLD = {"tab5-1": "old-token-one-0123456789", "tab5-2": "old-token-two-0123456789"}


def tokens_line(text):
    lines = [l for l in text.splitlines() if l.strip().startswith("PHONE_TOKENS=")]
    assert len(lines) == 1, lines
    return lines[0]


def new_token_in(text):
    """PHONE_TOKENS 줄 끝에 붙은 stackchan-2 의 토큰."""
    return re.search(r",stackchan-2:([A-Za-z0-9_-]+)", tokens_line(text)).group(1)


def compiles_as_device_table(secrets_text, want_rows, tmp_path):
    """고친 secrets.h 가 펌웨어가 쓰는 꼴(static const Entry TABLE[] = PHONE_DEVICES;)로 컴파일되고 줄 수가 맞는지. 컴파일러가 없으면 건너뛴다."""
    cxx = shutil.which("c++")
    if cxx is None:
        return None
    src = tmp_path / "table_check.cpp"
    src.write_text(secrets_text + "\nstruct Entry { const char *mac; int unit; const char *id; const char *token; };\n"
                   "static const Entry TABLE[] = PHONE_DEVICES;\n"
                   f"static_assert(sizeof(TABLE) / sizeof(TABLE[0]) == {want_rows}, \"rows\");\nint main() {{ return TABLE[0].unit; }}\n", encoding="utf-8")
    p = subprocess.run([cxx, "-std=c++17", "-fsyntax-only", "-Wall", "-Werror", "-Wno-pragma-once-outside-header", str(src)], capture_output=True, text=True)
    src.unlink()
    return p.returncode == 0


def test_add_device_adds_one_token_and_one_row_and_keeps_existing_tokens_byte_for_byte(files, capsys, tmp_path):
    env, sec, bk = files
    rc, out = run(files, *ADD, capsys=capsys)
    assert rc == 0, out
    e, s = env.read_text(encoding="utf-8"), sec.read_text(encoding="utf-8")
    tok = new_token_in(e)
    assert len(tok) >= 32
    # .env.phone: PHONE_TOKENS 줄은 "옛 줄 그대로 + ,stackchan-2:<새 토큰>" 이고, 다른 줄은 글자 하나 다르지 않다
    assert tokens_line(e) == f"{OLD_ENV_LINE},stackchan-2:{tok}"
    assert e.replace(tokens_line(e), OLD_ENV_LINE) == ENV
    # secrets.h: 한 줄만 늘었고, 그 줄을 빼면 원래 글과 글자까지 같다. 새 줄은 표의 마지막 기기 줄 바로 뒤.
    row = f'    {{ "{MAC3}", 3, "stackchan-2", "{tok}" }}, \\\n'
    assert s.count(row) == 1 and s.replace(row, "") == SECRETS
    assert s.index('"tab5-2", "old-token-two-0123456789" }, \\\n') + len('"tab5-2", "old-token-two-0123456789" }, \\\n') == s.index(row)
    for dev, old in OLD.items():                               # 기존 두 토큰은 두 파일 모두에서 그대로
        assert f"{dev}:{old}" in e and f'"{dev}", "{old}" }}' in s
    # 출력에는 토큰·주소·MAC·Wi-Fi 값이 없다
    for secret in (tok, "old-token", "192.168", "my-password", "my-ssid", MAC3, MAC3.lower()):
        assert secret not in out
    assert "stackchan-2" in out and "그대로" in out
    assert compiles_as_device_table(s, 3, tmp_path) in (True, None)


def test_add_device_result_is_readable_by_the_relay_and_passes_check(files, capsys):
    """더한 뒤의 PHONE_TOKENS 를 교환기의 해석기가 3대로 읽고(적힌 순서 유지 = 옛 짝은 여전히 앞의 두 Tab5), --check 가 세 대 모두 일치라고 한다."""
    env, sec, bk = files
    assert run(files, *ADD, capsys=capsys)[0] == 0
    toks = phone_core.parse_tokens(tokens_line(env.read_text(encoding="utf-8")).split("=", 1)[1])
    assert list(toks) == ["tab5-1", "tab5-2", "stackchan-2"]
    assert {d: toks[d] for d in OLD} == OLD
    assert phone_core.parse_legacy_pair("", toks) == ("tab5-1", "tab5-2")
    rc, out = run(files, "--check", capsys=capsys)
    assert rc == 0 and out.count("일치") == 3 and "불일치" not in out
    assert "tab5-1: 일치" in out and "tab5-2: 일치" in out and "stackchan-2: 일치" in out


def test_add_device_makes_a_backup_keeps_modes_and_leaves_no_temp_files(files, capsys):
    env, sec, bk = files
    sec.chmod(0o640)
    assert run(files, *ADD, capsys=capsys)[0] == 0
    dirs = list(bk.iterdir())
    assert len(dirs) == 1
    assert (dirs[0] / ".env.phone").read_text(encoding="utf-8") == ENV and (dirs[0] / "secrets.h").read_text(encoding="utf-8") == SECRETS
    assert stat.S_IMODE(env.stat().st_mode) == 0o600 and stat.S_IMODE(sec.stat().st_mode) == 0o640
    assert sorted(p.name for p in env.parent.iterdir()) == [".env.phone", "bk", "secrets.h"]
    assert sorted(p.name for p in dirs[0].iterdir()) == [".env.phone", "secrets.h"]


def test_add_device_twice_changes_nothing_the_second_time_and_says_so(files, capsys):
    env, sec, bk = files
    assert run(files, *ADD, capsys=capsys)[0] == 0
    e1, s1 = env.read_text(encoding="utf-8"), sec.read_text(encoding="utf-8")
    rc, out = run(files, *ADD, capsys=capsys)
    assert rc == 0 and "이미" in out and "바꾸지 않" in out
    assert env.read_text(encoding="utf-8") == e1 and sec.read_text(encoding="utf-8") == s1
    assert len(list(bk.iterdir())) == 1                        # 보관도 다시 하지 않는다
    assert new_token_in(e1) not in out


def test_adding_an_already_registered_tab5_changes_nothing(files, capsys):
    env, sec, bk = files
    rc, out = run(files, "--add-device", "tab5-1", "--unit", "1", "--mac", "AA:BB:CC:DD:EE:01", capsys=capsys)
    assert rc == 0 and "이미" in out
    assert env.read_text(encoding="utf-8") == ENV and sec.read_text(encoding="utf-8") == SECRETS and not bk.exists()


@pytest.mark.parametrize("only_in", ["env", "secrets"])
def test_add_device_present_in_only_one_file_changes_nothing_and_fails(files, capsys, only_in):
    """한쪽에만 있는 어긋난 상태 — 스스로 맞추려 들지 않는다(어느 쪽 토큰이 기기에 구워졌는지 도구는 모른다). 아무것도 안 바꾸고 그렇다고 말한다."""
    env, sec, bk = files
    half_env = ENV.replace(OLD_ENV_LINE, OLD_ENV_LINE + ",stackchan-2:half-token-0123456789abcdef")
    half_sec = SECRETS.replace("}, \\\n}", '}, \\\n    { "%s", 3, "stackchan-2", "half-token-0123456789abcdef" }, \\\n}' % MAC3)
    assert half_sec != SECRETS
    if only_in == "env":
        env.write_text(half_env, encoding="utf-8")
    else:
        sec.write_text(half_sec, encoding="utf-8")
    before = env.read_text(encoding="utf-8"), sec.read_text(encoding="utf-8")
    rc, out = run(files, *ADD, capsys=capsys)
    assert rc == 1 and "한쪽" in out and "half-token" not in out
    assert (env.read_text(encoding="utf-8"), sec.read_text(encoding="utf-8")) == before and not bk.exists()


@pytest.mark.parametrize("unit,mac,word", [("2", MAC3, "번호"), ("1", MAC3, "번호"), ("3", "AA:BB:CC:DD:EE:02", "MAC"), ("3", "aa:bb:cc:dd:ee:01", "MAC")])
def test_add_device_refuses_a_unit_or_mac_already_in_the_table(files, capsys, unit, mac, word):
    env, sec, bk = files
    rc, out = run(files, "--add-device", "stackchan-2", "--unit", unit, "--mac", mac, capsys=capsys)
    assert rc == 1 and word in out and "바꾸지 않" in out
    assert env.read_text(encoding="utf-8") == ENV and sec.read_text(encoding="utf-8") == SECRETS and not bk.exists()


def test_add_device_sees_a_mac_already_in_the_table_whatever_its_letter_case(files, capsys):
    """표에 소문자로 적힌 MAC 도 같은 기기다(펌웨어는 대소문자를 가리지 않고 맞춘다 — device_id.h same_mac). 같은 MAC 을 두 줄에 두면 뒤 줄은 영영 안 읽힌다."""
    env, sec, bk = files
    lower = SECRETS.replace("AA:BB:CC:DD:EE:01", "aa:bb:cc:dd:ee:01")
    sec.write_text(lower, encoding="utf-8")
    for mac in ("AA:BB:CC:DD:EE:01", "aa:bb:cc:dd:ee:01", "AA:bb:CC:dd:EE:01"):
        rc, out = run(files, "--add-device", "stackchan-2", "--unit", "3", "--mac", mac, capsys=capsys)
        assert rc == 1 and "MAC" in out and "바꾸지 않" in out
        assert env.read_text(encoding="utf-8") == ENV and sec.read_text(encoding="utf-8") == lower and not bk.exists()


@pytest.mark.parametrize("extra", [
    ["--add-device", "stackchan-2"],                                             # 번호·MAC 없음
    ["--add-device", "stackchan-2", "--unit", "3"],                              # MAC 없음
    ["--add-device", "stackchan-2", "--mac", MAC3],                              # 번호 없음
    ["--add-device", "stackchan-2", "--unit", "0", "--mac", MAC3],               # 번호 범위 밖
    ["--add-device", "stackchan-2", "--unit", "9", "--mac", MAC3],
    ["--add-device", "stackchan-2", "--unit", "x", "--mac", MAC3],
    ["--add-device", "stackchan-2", "--unit", "3", "--mac", "AA:BB:CC:DD:EE"],   # MAC 모양
    ["--add-device", "stackchan-2", "--unit", "3", "--mac", "AA-BB-CC-DD-EE-03"],
    ["--add-device", "stackchan-2", "--unit", "3", "--mac", "GG:BB:CC:DD:EE:03"],
    ["--add-device", 'stack"chan', "--unit", "3", "--mac", MAC3],                # C 문자열·설정 줄을 깨는 기기 ID
    ["--add-device", "a,b", "--unit", "3", "--mac", MAC3],
    ["--add-device", "a:b", "--unit", "3", "--mac", MAC3],
    ["--add-device", "a b", "--unit", "3", "--mac", MAC3],
    ["--add-device", "", "--unit", "3", "--mac", MAC3],
    ["--add-device", "3", "--unit", "3", "--mac", MAC3],
    ["--add-device", "a" * 16, "--unit", "3", "--mac", MAC3],                    # 펌웨어의 기기 ID 칸(16바이트)에 안 들어간다
    [*ADD, "--check"],                                                           # 함께 쓸 수 없는 낱말
    [*ADD, "--relay-url", "wss://phone.example.com/ws"],
    ["--unit", "3"],                                                             # --add-device 없이
    ["--mac", MAC3],
])
def test_add_device_bad_arguments_exit_2_and_change_nothing(files, capsys, extra):
    env, sec, bk = files
    rc, out = run(files, *extra, capsys=capsys)
    assert rc == 2 and "인자 오류" in out
    assert env.read_text(encoding="utf-8") == ENV and sec.read_text(encoding="utf-8") == SECRETS and not bk.exists()


def test_add_device_writes_the_mac_in_upper_case(files, capsys):
    env, sec, bk = files
    assert run(files, "--add-device", "stackchan-2", "--unit", "3", "--mac", MAC3.lower(), capsys=capsys)[0] == 0
    assert f'{{ "{MAC3}", 3, "stackchan-2", "' in sec.read_text(encoding="utf-8")


@pytest.mark.parametrize("q", ['"', "'"])
def test_add_device_into_a_quoted_env_value_stays_inside_the_quotes(files, capsys, q):
    env, sec, bk = files
    quoted = ENV.replace(OLD_ENV_LINE, f"PHONE_TOKENS={q}{OLD_ENV_LINE[13:]}{q}")
    env.write_text(quoted, encoding="utf-8")
    assert run(files, *ADD, capsys=capsys)[0] == 0
    line = tokens_line(env.read_text(encoding="utf-8"))
    tok = new_token_in(line)
    assert line == f"PHONE_TOKENS={q}{OLD_ENV_LINE[13:]},stackchan-2:{tok}{q}"
    assert run(files, "--check", capsys=capsys)[0] == 0


@pytest.mark.parametrize("env_text", [None, "", "PORT=5094\n", "PHONE_TOKENS=tab5-1:old-token-one-0123456789\n", "PHONE_TOKENS=tab5-1:short,tab5-2:old-token-two-0123456789\n",
                                      OLD_ENV_LINE + "\n" + OLD_ENV_LINE + "\n"])
def test_add_device_needs_a_registration_the_relay_can_read(files, capsys, env_text):
    """더하기는 "지금 돌아가는 등록에 한 대를 얹는" 낱말이다 — 파일이 없거나, 줄이 없거나, 교환기가 읽지 못하는 줄이면 아무것도 바꾸지 않는다."""
    env, sec, bk = files
    if env_text is None:
        env.unlink()
    else:
        env.write_text(env_text, encoding="utf-8")
    rc, out = run(files, *ADD, capsys=capsys)
    assert rc == 1 and "바꾸지 않" in out and "old-token" not in out
    assert sec.read_text(encoding="utf-8") == SECRETS and not bk.exists()
    assert (env.read_text(encoding="utf-8") == env_text) if env_text is not None else not env.exists()


def test_add_device_refuses_a_ninth_device(tmp_path, capsys):
    ids = [f"dev-{i}" for i in range(1, 9)]
    env, sec = tmp_path / ".env.phone", tmp_path / "secrets.h"
    env.write_text("PHONE_TOKENS=" + ",".join(f"{d}:token-{d}-0123456789abcdef" for d in ids) + "\n", encoding="utf-8")
    rows = "".join(f'    {{ "AA:BB:CC:DD:EE:{i:02X}", {i}, "{d}", "token-{d}-0123456789abcdef" }}, \\\n' for i, d in enumerate(ids, 1))
    sec.write_text("#pragma once\n#define PHONE_DEVICES { \\\n" + rows + "}\n", encoding="utf-8")
    before = env.read_text(encoding="utf-8"), sec.read_text(encoding="utf-8")
    rc = rot.main(["--env", str(env), "--secrets", str(sec), "--backup-dir", str(tmp_path / "bk"), "--add-device", "ninth", "--unit", "8", "--mac", MAC3])
    assert rc == 1 and "8대" in capsys.readouterr().out
    assert (env.read_text(encoding="utf-8"), sec.read_text(encoding="utf-8")) == before


@pytest.mark.parametrize("secrets_text", [
    "#pragma once\n#define RELAY_URL \"wss://x.example.com/ws\"\n",                                         # 표가 없다
    '#define PHONE_DEVICES { { "AA:BB:CC:DD:EE:01", 1, "tab5-1", "old-token-one-0123456789" }, { "AA:BB:CC:DD:EE:02", 2, "tab5-2", "old-token-two-0123456789" } }\n',   # 한 줄짜리 표
    SECRETS + SECRETS.split("#define PHONE_DEVICES")[1].join(["#define PHONE_DEVICES", ""]),                 # 표가 둘
    SECRETS.replace("}, \\\n}", "}, \\\n"),                                                                  # 표가 파일 끝에서 끊김
])
def test_add_device_refuses_a_table_it_cannot_read_safely(files, capsys, secrets_text):
    env, sec, bk = files
    sec.write_text(secrets_text, encoding="utf-8")
    rc, out = run(files, *ADD, capsys=capsys)
    assert rc == 1 and "바꾸지 않" in out and "old-token" not in out
    assert env.read_text(encoding="utf-8") == ENV and sec.read_text(encoding="utf-8") == secrets_text and not bk.exists()


def test_add_device_adds_the_missing_comma_after_the_last_row_without_touching_tokens(files, capsys, tmp_path):
    """마지막 기기 줄 끝에 쉼표가 없는 표: 쉼표 하나를 넣고 새 줄을 잇는다 — 토큰 글자는 그대로, 결과는 컴파일된다."""
    env, sec, bk = files
    no_comma = SECRETS.replace('"old-token-two-0123456789" }, \\', '"old-token-two-0123456789" } \\')
    assert no_comma != SECRETS
    sec.write_text(no_comma, encoding="utf-8")
    assert run(files, *ADD, capsys=capsys)[0] == 0
    s = sec.read_text(encoding="utf-8")
    tok = new_token_in(env.read_text(encoding="utf-8"))
    assert '"tab5-1", "old-token-one-0123456789" }, \\\n' in s and '"tab5-2", "old-token-two-0123456789" }, \\\n' in s
    assert f'    {{ "{MAC3}", 3, "stackchan-2", "{tok}" }}, \\\n}}' in s
    assert compiles_as_device_table(s, 3, tmp_path) in (True, None)
    assert run(files, "--check", capsys=capsys)[0] == 0


def test_add_device_follows_the_indentation_of_the_table(files, capsys):
    env, sec, bk = files
    sec.write_text(SECRETS.replace("    { ", "\t{ "), encoding="utf-8")
    assert run(files, *ADD, capsys=capsys)[0] == 0
    assert f'\n\t{{ "{MAC3}", 3, "stackchan-2", "' in sec.read_text(encoding="utf-8")


def test_add_device_secrets_write_failure_restores_env_and_fails(files, capsys, monkeypatch):
    """.env.phone 에 토큰을 얹은 뒤 secrets.h 쓰기가 실패하면 보관본으로 되돌린다 — 교환기만 아는 토큰이 남지 않게."""
    env, sec, bk = files
    real = rot.atomic_write

    def flaky(path, text, tmp_dir=None):
        if Path(path).name == "secrets.h":
            raise OSError("디스크 가득")
        return real(path, text, tmp_dir)

    monkeypatch.setattr(rot, "atomic_write", flaky)
    rc, out = run(files, *ADD, capsys=capsys)
    assert rc != 0 and "원래대로 되돌림" in out and "디스크" not in out
    assert env.read_text(encoding="utf-8") == ENV and sec.read_text(encoding="utf-8") == SECRETS


def test_add_device_never_writes_when_its_own_check_finds_an_old_token_changed(files, capsys, monkeypatch):
    """마지막 안전장치: 새 글을 만든 뒤 "기존 토큰이 그대로인가"를 스스로 다시 확인한다 — 만드는 쪽에 버그가 생겨도 틀린 글은 쓰지 않는다."""
    env, sec, bk = files
    real = rot.add_env_token
    monkeypatch.setattr(rot, "add_env_token", lambda text, dev, token: real(text, dev, token).replace("old-token-one", "new-token-one"))
    rc, out = run(files, *ADD, capsys=capsys)
    assert rc == 1 and "바꾸지 않" in out
    assert env.read_text(encoding="utf-8") == ENV and sec.read_text(encoding="utf-8") == SECRETS and not bk.exists()


# ── 3대일 때의 --check 와 교체 ──────────────────────────────────────────────
def three(files, capsys):
    """두 파일을 3대 등록 상태로 만든다(더하기 낱말로)."""
    assert run(files, *ADD, capsys=capsys)[0] == 0
    env, sec, _ = files
    return env.read_text(encoding="utf-8"), sec.read_text(encoding="utf-8")


def test_check_sees_a_mismatch_in_the_third_device(files, capsys):
    env, sec, bk = files
    e, s = three(files, capsys)
    sec.write_text(s.replace(new_token_in(e), "other-token-0123456789abcdef"), encoding="utf-8")
    rc, out = run(files, "--check", capsys=capsys)
    assert rc == 1 and "tab5-1: 일치" in out and "tab5-2: 일치" in out and "stackchan-2: 불일치" in out
    assert "other-token" not in out and new_token_in(e) not in out


@pytest.mark.parametrize("missing_in", ["env", "secrets"])
def test_check_sees_a_third_device_registered_in_only_one_file(files, capsys, missing_in):
    env, sec, bk = files
    three(files, capsys)
    (env if missing_in == "env" else sec).write_text(ENV if missing_in == "env" else SECRETS, encoding="utf-8")
    rc, out = run(files, "--check", capsys=capsys)
    assert rc == 1 and "stackchan-2: 불일치 (한쪽에 없음)" in out and "tab5-1: 일치" in out


def test_plain_rotate_with_three_devices_changes_all_three_and_drops_none(files, capsys):
    """교체(인자 없이)는 등록된 기기 전부의 토큰을 바꾼다. 2대만 알던 때처럼 셋째를 .env.phone 에서 떨어뜨리면 안 된다."""
    env, sec, bk = files
    e1, s1 = three(files, capsys)
    old3 = new_token_in(e1)
    rc, out = run(files, capsys=capsys)
    assert rc == 0 and "3개" in out
    e2, s2 = env.read_text(encoding="utf-8"), sec.read_text(encoding="utf-8")
    toks = phone_core.parse_tokens(tokens_line(e2).split("=", 1)[1])
    assert list(toks) == ["tab5-1", "tab5-2", "stackchan-2"]                    # 순서(= 옛 짝)도 그대로
    assert "old-token" not in e2 + s2 and old3 not in e2 + s2 and len(set(toks.values())) == 3
    for dev, tok in toks.items():
        assert f'"{dev}", "{tok}" }}' in s2
    assert MAC3 in s2 and s2.count("\\\n") == s1.count("\\\n")
    assert run(files, "--check", capsys=capsys)[0] == 0
    assert not any(t in out for t in toks.values())


def test_plain_rotate_fails_when_a_registered_device_is_missing_from_secrets(files, capsys):
    env, sec, bk = files
    e, s = three(files, capsys)
    sec.write_text(SECRETS, encoding="utf-8")                  # secrets.h 에는 셋째가 없다
    n_backups = len(list(bk.iterdir()))
    rc, out = run(files, capsys=capsys)
    assert rc == 1 and "stackchan-2" in out
    assert env.read_text(encoding="utf-8") == e and sec.read_text(encoding="utf-8") == SECRETS and len(list(bk.iterdir())) == n_backups
