"""phone_relay_prod.sh(맥미니 배포 스크립트) 시험 — launchctl·cloudflared·lsof·curl 은 가짜(stub)로 바꾸고 HOME 을 임시 폴더로 삼는다.
진짜 launchd·터널·~/.cloudflared 는 건드리지 않는다. 실행: cd server && python3 -m pytest tests/test_phone_relay_prod.py -q
"""
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

DEPLOY = Path(__file__).resolve().parent.parent / "deploy"
SCRIPT_SRC = DEPLOY / "phone_relay_prod.sh"
TID = "12345678-1234-1234-1234-123456789abc"
DEFAULT_ID = "01e0b081-aaaa-bbbb-cccc-0123456789ab"   # 공용(maintunnel) 터널 ID 흉내

STUBS = {
    "launchctl": '#!/bin/bash\necho "launchctl $*" >> "$STUB_LOG"\ncase "$1" in bootout) [ -z "${STUB_STUCK:-}" ] && touch "$STUB_LOG.out";; bootstrap) [ -n "${STUB_BOOTSTRAP_FAIL:-}" ] && exit 5; rm -f "$STUB_LOG.out";; esac\nif [ "$1" = print ]; then [ -f "$STUB_LOG.out" ] && exit 113; [ -n "${STUB_LOADED:-}" ] && { echo "	pid = 4242"; exit 0; } || exit 113; fi\nexit 0\n',
    "lsof": '#!/bin/bash\necho "lsof $*" >> "$STUB_LOG"\n[ -n "${STUB_PORT_PID:-}" ] && { echo "$STUB_PORT_PID"; exit 0; }\nexit 1\n',
    "curl": '#!/bin/bash\necho "curl $*" >> "$STUB_LOG"\nprintf "%s" "${STUB_CURL:-200}"\n',
    # 맥미니의 함정 흉내: 설정(config.yml)이 있으면 이름으로 부른 info/route/delete 는 이름과 상관없이 "기본(공용) 터널"로 간다.
    "cloudflared": '''#!/bin/bash
echo "cloudflared $*" >> "$STUB_LOG"
DEFAULT="${STUB_DEFAULT_ID:-}"
case "$*" in
  *"tunnel list"*)
    ID="${STUB_TUNNEL_ID:-}"; [ -f "$STUB_LOG.created" ] && ID="${STUB_CREATE_ID:-}"
    if [ -n "$ID" ]; then echo "[{\\"id\\":\\"$ID\\",\\"name\\":\\"tab5phone\\",\\"deleted_at\\":\\"${STUB_DELETED_AT:-0001-01-01T00:00:00Z}\\"}]"; else echo "[]"; fi; exit 0 ;;
  *"tunnel info"*) [ -n "$DEFAULT" ] && { echo "NAME:     maintunnel"; echo "ID:       $DEFAULT"; exit 0; } || { echo "not found" >&2; exit 1; } ;;
  *"tunnel create"*) touch "$STUB_LOG.created"; prev=; for a in "$@"; do [ "$prev" = --credentials-file ] && echo '{}' > "$a"; prev=$a; done
                     [ -n "${STUB_CLOBBER:-}" ] && echo x >> "$STUB_CLOBBER"; echo "Created tunnel tab5phone"; exit 0 ;;
  *"route dns"*) [ -n "${STUB_DNS_EXISTS:-}" ] && { echo "An A, AAAA, or CNAME record with that host already exists." >&2; exit 1; } || exit 0 ;;
esac
exit 0
''',
}


@pytest.fixture
def env(tmp_path):
    """임시 저장소 사본(server/deploy + server/.env.phone 자리) + 가짜 명령 + 임시 HOME."""
    repo = tmp_path / "repo"
    (repo / "server" / "deploy").mkdir(parents=True)
    for f in DEPLOY.iterdir():
        shutil.copy(f, repo / "server" / "deploy" / f.name)
    (repo / "server" / "phone_relay.py").write_text("# stub\n")
    home = tmp_path / "home"
    (home / ".cloudflared").mkdir(parents=True)
    bindir = tmp_path / "bin"
    bindir.mkdir()
    for name, body in STUBS.items():
        (bindir / name).write_text(body)
        (bindir / name).chmod(0o755)
    log = tmp_path / "stub.log"
    log.write_text("")
    e = dict(os.environ, HOME=str(home), PATH=f"{bindir}:/usr/bin:/bin", STUB_LOG=str(log), PHONE_PYTHON=sys.executable)
    for k in ("STUB_LOADED", "STUB_PORT_PID", "STUB_TUNNEL_ID", "STUB_DNS_EXISTS", "STUB_CURL", "STUB_DEFAULT_ID", "STUB_CREATE_ID", "STUB_DELETED_AT", "STUB_STUCK", "STUB_BOOTSTRAP_FAIL", "STUB_CLOBBER", "PHONE_WAIT_DOWN_SECS"):
        e.pop(k, None)
    return {"repo": repo, "home": home, "env": e, "log": log, "script": repo / "server" / "deploy" / "phone_relay_prod.sh"}


def sh(c, *args, **extra):
    e = dict(c["env"], **extra)
    p = subprocess.run(["/bin/bash", str(c["script"]), *args], env=e, capture_output=True, text=True, timeout=60)
    return p.returncode, p.stdout + p.stderr


def calls(c):
    return c["log"].read_text().splitlines()


def mutating(c):
    """가짜 명령 기록 중 상태를 바꾸는 호출(읽기 전용 print/info/lsof/curl 은 제외)."""
    bad = ("bootstrap", "bootout", "kickstart", "enable", "disable", "load", "unload", "tunnel create", "route dns", "tunnel delete", "ingress validate")
    return [l for l in calls(c) if any(b in l for b in bad)]


def make_env_file(c):
    f = c["repo"] / "server" / ".env.phone"
    f.write_text("PORT=5094\nBIND=127.0.0.1\nPHONE_TOKENS=tab5-1:aaaaaaaaaaaaaaaaaaaaaaaa,tab5-2:bbbbbbbbbbbbbbbbbbbbbbbb\n")
    f.chmod(0o600)
    return f


# ── 정적 ────────────────────────────────────────────────────────────────────
def test_script_syntax_and_header():
    assert subprocess.run(["bash", "-n", str(SCRIPT_SRC)]).returncode == 0
    text = SCRIPT_SRC.read_text(encoding="utf-8")
    assert text.startswith("#!/bin/bash") and "set -euo pipefail" in text
    assert os.access(SCRIPT_SRC, os.X_OK)


def test_templates_are_valid_plists_without_secrets():
    for f in DEPLOY.glob("*.plist.template"):
        text = f.read_text(encoding="utf-8")
        filled = text.replace("__HOME__", "/h").replace("__PYTHON__", "/p").replace("__SERVER_DIR__", "/s").replace("__CLOUDFLARED__", "/c")
        tmp = Path(os.environ.get("TMPDIR", "/tmp")) / ("lint_" + f.name + ".plist")
        tmp.write_text(filled)
        try:
            assert subprocess.run(["plutil", "-lint", str(tmp)], capture_output=True).returncode == 0, f.name
        finally:
            tmp.unlink()
        assert "PHONE_TOKENS" not in re.sub(r"<!--.*?-->", "", text, flags=re.S)
    relay = (DEPLOY / "com.example.tab5phone-relay.plist.template").read_text()
    assert "<key>KeepAlive</key>" in relay and "<key>RunAtLoad</key>" in relay and "127.0.0.1" in relay and "5094" in relay


def test_script_never_writes_to_shared_cloudflared_config():
    """P4-D2: 기존 터널 설정·launchd 항목은 읽기 외에 만지지 않는다. 그 이름이 나오는 실행 줄은 전부 `# GUARD` 로 표시된 거절 검사뿐이어야 한다."""
    for line in SCRIPT_SRC.read_text(encoding="utf-8").splitlines():
        code = line.strip()
        if code.startswith("#"):
            continue
        if re.search(r"config\.yml|maintunnel", code):
            assert code.endswith("# GUARD"), f"공용 터널 이름이 가드 밖에 나옴: {line}"


# ── 인자 ────────────────────────────────────────────────────────────────────
@pytest.mark.parametrize("args", [[], ["dance"], ["install", "now"], ["start", "everything"], ["install", "--dry"]])
def test_bad_args_exit_2_and_touch_nothing(env, args):
    rc, out = sh(env, *args)
    assert rc == 2 and "사용" in out
    assert mutating(env) == []


# ── install ─────────────────────────────────────────────────────────────────
def test_install_stops_without_env_file(env):
    rc, out = sh(env, "install", "--dry-run")
    assert rc != 0 and ".env.phone" in out
    assert mutating(env) == []


def test_install_dry_run_prints_commands_and_changes_nothing(env):
    make_env_file(env)
    rc, out = sh(env, "install", "--dry-run")
    assert rc == 0, out
    assert "bootstrap" in out and "com.example.tab5phone-relay" in out
    assert mutating(env) == []
    assert not (env["home"] / "Library").exists()                       # 파일도 안 쓴다
    assert "config.yml" not in out and "maintunnel" not in out
    assert "aaaaaaaa" not in out                                         # 토큰 값은 안 나온다


def test_install_stops_when_port_is_used_by_someone_else(env):
    make_env_file(env)
    rc, out = sh(env, "install", "--dry-run", STUB_PORT_PID="31337")
    assert rc != 0 and "5094" in out


def test_install_real_run_writes_plist_then_is_idempotent(env):
    make_env_file(env)
    rc, out = sh(env, "install")
    assert rc == 0, out
    plist = env["home"] / "Library" / "LaunchAgents" / "com.example.tab5phone-relay.plist"
    text = plist.read_text()
    assert "__" not in text and str(env["repo"] / "server" / "phone_relay.py") in text and str(env["home"]) in text
    assert (env["home"] / "Library" / "Logs" / "tab5phone").is_dir()
    first = len(mutating(env))
    assert any("bootstrap" in l for l in calls(env))
    rc, out = sh(env, "install", STUB_LOADED="1", STUB_PORT_PID="4242")      # 이미 떠 있고 plist 도 같다 → 그대로
    assert rc == 0 and "이미" in out
    assert len(mutating(env)) == first
    env_perm = oct((env["repo"] / "server" / ".env.phone").stat().st_mode & 0o777)
    assert env_perm == "0o600"


def test_install_tightens_loose_env_permissions(env):
    f = make_env_file(env)
    f.chmod(0o644)
    sh(env, "install")
    assert oct(f.stat().st_mode & 0o777) == "0o600"


# ── tunnel-install ──────────────────────────────────────────────────────────
def test_tunnel_install_requires_login_cert(env):
    rc, out = sh(env, "tunnel-install", "--dry-run")
    assert rc != 0 and "cloudflared tunnel login" in out
    assert mutating(env) == []


def test_tunnel_install_dry_run_new_tunnel(env):
    (env["home"] / ".cloudflared" / "cert.pem").write_text("x")
    rc, out = sh(env, "tunnel-install", "--dry-run")
    assert rc == 0, out
    for want in ("tab5phone.credentials.json tab5phone", "ingress validate", "route dns", "phone.example.com", "com.example.tab5phone-tunnel"):
        assert want in out, want
    assert mutating(env) == []
    assert "config.yml" not in out and "maintunnel" not in out
    assert not (env["home"] / ".cloudflared" / "tab5phone.yml").exists()


def test_tunnel_install_existing_tunnel_without_credentials_stops(env):
    (env["home"] / ".cloudflared" / "cert.pem").write_text("x")
    rc, out = sh(env, "tunnel-install", "--dry-run", STUB_TUNNEL_ID=TID)
    assert rc != 0 and "credentials" in out.lower()


def test_tunnel_install_real_run_writes_config_and_is_idempotent(env):
    cf = env["home"] / ".cloudflared"
    (cf / "cert.pem").write_text("x")
    (cf / "tab5phone.credentials.json").write_text("{}")
    rc, out = sh(env, "tunnel-install", STUB_TUNNEL_ID=TID)
    assert rc == 0, out
    cfg = (cf / "tab5phone.yml").read_text()
    assert f"tunnel: {TID}" in cfg and f"credentials-file: {cf}/tab5phone.credentials.json" in cfg
    assert "hostname: phone.example.com" in cfg and "service: http://127.0.0.1:5094" in cfg and "service: http_status:404" in cfg
    assert "keepAliveTimeout" in cfg and "connectTimeout" in cfg
    assert not (cf / "config.yml").exists()                              # 공용 설정은 만들지도 않는다
    assert "tunnel create" not in "\n".join(calls(env))                  # 이미 있는 터널은 다시 만들지 않는다
    assert (env["home"] / "Library" / "LaunchAgents" / "com.example.tab5phone-tunnel.plist").exists()
    n = len(calls(env))
    rc, out = sh(env, "tunnel-install", STUB_TUNNEL_ID=TID, STUB_LOADED="1", STUB_DNS_EXISTS="1")   # DNS 이미 있음·plist 같음 → 그대로
    assert rc == 0, out
    assert (cf / "tab5phone.yml").read_text() == cfg
    assert not any("bootstrap" in l for l in calls(env)[n:])


# ── 공용 터널 함정 (수정 1회차: 이름으로 부른 명령이 공용 터널로 가는 맥미니 현상) ──
def make_shared_config(c):
    cf = c["home"] / ".cloudflared"
    (cf / "config.yml").write_text(f"tunnel: {DEFAULT_ID}\ncredentials-file: {cf}/{DEFAULT_ID}.json\n")
    (cf / f"{DEFAULT_ID}.json").write_text("{}")
    (cf / "cert.pem").write_text("x")
    return cf


def test_shared_tunnel_is_not_mistaken_for_tab5phone_dry_run(env):
    make_shared_config(env)
    rc, out = sh(env, "tunnel-install", "--dry-run", STUB_DEFAULT_ID=DEFAULT_ID)    # tab5phone 은 아직 없다
    assert rc == 0, out
    assert "tab5phone.credentials.json tab5phone" in out and "이미 있음" not in out
    assert "route dns <터널ID>" in out or "route dns" in out and "route dns tab5phone" not in out
    assert not any("tunnel info" in l for l in calls(env))                  # 이름으로 부르는 info 는 쓰지 않는다
    assert mutating(env) == []


def test_shared_tunnel_trap_real_run_uses_uuid_only(env):
    cf = make_shared_config(env)
    new_id = "99999999-8888-7777-6666-555555555555"
    rc, out = sh(env, "tunnel-install", STUB_DEFAULT_ID=DEFAULT_ID, STUB_CREATE_ID=new_id)
    assert rc == 0, out
    cfg = (cf / "tab5phone.yml").read_text()
    assert f"tunnel: {new_id}" in cfg and DEFAULT_ID not in cfg and f"{cf}/tab5phone.credentials.json" in cfg
    c = calls(env)
    assert any(f"route dns {new_id} phone.example.com" in l for l in c)
    assert not any(re.search(r"route dns tab5phone|tunnel run tab5phone|tunnel delete", l) for l in c)
    assert (cf / "config.yml").read_text().startswith(f"tunnel: {DEFAULT_ID}")  # 공용 설정은 그대로


def test_create_names_its_own_credentials_file(env):
    """재검수 1: create 는 전용 credentials 경로를 명시한다(공용 설정의 경로가 쓰이지 않게)."""
    cf = make_shared_config(env)
    cred = cf / "tab5phone.credentials.json"
    rc, out = sh(env, "tunnel-install", "--dry-run", STUB_DEFAULT_ID=DEFAULT_ID)
    assert f"tunnel create --credentials-file {cred} tab5phone" in out
    rc, out = sh(env, "tunnel-install", STUB_DEFAULT_ID=DEFAULT_ID, STUB_CREATE_ID="99999999-8888-7777-6666-555555555555")
    assert rc == 0, out
    assert any(f"tunnel create --credentials-file {cred} tab5phone" in l for l in calls(env))
    assert cred.exists()


def test_existing_credentials_file_without_tunnel_stops_without_overwriting(env):
    cf = make_shared_config(env)
    cred = cf / "tab5phone.credentials.json"
    cred.write_text("precious")
    rc, out = sh(env, "tunnel-install", STUB_DEFAULT_ID=DEFAULT_ID)
    assert rc != 0 and "tab5phone.credentials.json" in out
    assert cred.read_text() == "precious" and not any("tunnel create" in l for l in calls(env))


@pytest.mark.parametrize("which", ["config.yml", f"{DEFAULT_ID}.json"])
def test_shared_files_changed_during_create_aborts_loudly(env, which):
    """재검수 1: create 앞뒤로 공용 설정·공용 credentials 의 해시를 비교 — 달라지면 중단(내용은 출력하지 않는다)."""
    cf = make_shared_config(env)
    rc, out = sh(env, "tunnel-install", STUB_DEFAULT_ID=DEFAULT_ID, STUB_CREATE_ID="99999999-8888-7777-6666-555555555555", STUB_CLOBBER=str(cf / which))
    assert rc != 0 and "공용" in out and "바뀜" in out
    assert not (cf / "tab5phone.yml").exists()


def test_tunnel_whose_id_equals_shared_id_is_refused(env):
    cf = make_shared_config(env)
    rc, out = sh(env, "tunnel-install", "--dry-run", STUB_TUNNEL_ID=DEFAULT_ID)
    assert rc != 0 and "공용 터널" in out
    assert not (cf / "tab5phone.yml").exists()


def test_deleted_tunnel_is_treated_as_missing(env):
    (env["home"] / ".cloudflared" / "cert.pem").write_text("x")
    rc, out = sh(env, "tunnel-install", "--dry-run", STUB_TUNNEL_ID=TID, STUB_DELETED_AT="2026-01-01T00:00:00Z")
    assert rc == 0 and "tab5phone.credentials.json tab5phone" in out


# ── status / start / stop / uninstall ───────────────────────────────────────
def test_status_reports_both_services_and_healthz(env):
    rc, out = sh(env, "status", STUB_LOADED="1")
    assert rc == 0, out
    assert "tab5phone-relay" in out and "tab5phone-tunnel" in out and "127.0.0.1:5094/healthz" in out and "https://phone.example.com/healthz" in out
    rc, out = sh(env, "status", STUB_CURL="000")
    assert rc != 0                                                       # 응답이 없으면 실패 코드


@pytest.mark.parametrize("cmd", ["start", "stop", "restart"])
def test_lifecycle_dry_run_changes_nothing(env, cmd):
    rc, out = sh(env, cmd, "--dry-run")
    assert rc == 0, out
    assert mutating(env) == []
    assert "config.yml" not in out and "maintunnel" not in out


def test_restart_relay_uses_bootout_then_bootstrap_never_kill(env):
    """검수 1: kickstart -k 는 SIGKILL 이라 종료 훅(1001 닫기)이 안 돈다 — restart 는 bootout(SIGTERM) → 내려감 확인 → bootstrap."""
    make_env_file(env)
    sh(env, "install")
    n = len(calls(env))
    rc, out = sh(env, "restart", "relay", STUB_LOADED="1")
    assert rc == 0, out
    c = [l for l in calls(env)[n:] if l.startswith("launchctl") and not l.startswith("launchctl print")]
    assert not any("kickstart" in l for l in c), c
    assert [l.split()[1] for l in c] == ["bootout", "bootstrap"], c
    rc, out = sh(env, "restart", "tunnel", STUB_LOADED="1")           # 터널도 같은 길(고정된 규칙)
    assert not any("kickstart -k" in l for l in calls(env))


def test_script_extends_path_for_noninteractive_ssh():
    """검수 3: ssh 비대화형 셸은 PATH 에 /opt/homebrew/bin 이 없다 — 뒤에 덧붙인다(앞에 두면 가짜 명령·사용자 지정 도구를 가린다)."""
    text = SCRIPT_SRC.read_text(encoding="utf-8")
    assert 'PATH="$PATH:/opt/homebrew/bin:/usr/local/bin"' in text


def test_restart_that_cannot_bring_service_down_tells_to_run_start(env):
    """재검수 2: 안 내려가면 안내와 함께 중단(서비스는 그대로 떠 있다)."""
    make_env_file(env)
    sh(env, "install")
    rc, out = sh(env, "restart", "relay", STUB_LOADED="1", STUB_STUCK="1", PHONE_WAIT_DOWN_SECS="2")
    assert rc != 0 and "start" in out and "내려가지 않음" in out


def test_restart_bootstrap_failure_tells_service_is_down_and_to_run_start(env):
    make_env_file(env)
    sh(env, "install")
    rc, out = sh(env, "restart", "relay", STUB_LOADED="1", STUB_BOOTSTRAP_FAIL="1")
    assert rc != 0 and "꺼진" in out and "start" in out


def test_restart_relay_only_never_touches_tunnel(env):
    rc, out = sh(env, "restart", "relay", "--dry-run", STUB_LOADED="1")
    assert rc == 0 and "tab5phone-relay" in out and "tab5phone-tunnel" not in out


def test_uninstall_dry_run_only_prints_tunnel_deletion_guidance(env):
    rc, out = sh(env, "uninstall", "--dry-run", STUB_LOADED="1")
    assert rc == 0, out
    assert "bootout" in out and "tunnel delete" in out and "공용 터널이 지워질 수 있다" in out    # 안내로만 + 이름 삭제 경고
    assert "list" in out                                                  # UUID 를 못 얻으면 list 로 확인하라는 안내
    assert not any("delete" in l for l in calls(env)) and mutating(env) == []


def test_uninstall_guidance_uses_uuid_not_name(env):
    rc, out = sh(env, "uninstall", "--dry-run", STUB_TUNNEL_ID=TID)
    assert rc == 0 and f"tunnel delete {TID}" in out and "tunnel delete tab5phone" not in out


def test_uninstall_real_removes_plists_but_not_cloudflared_files(env):
    make_env_file(env)
    sh(env, "install")
    cf = env["home"] / ".cloudflared"
    (cf / "tab5phone.yml").write_text("keep")
    rc, out = sh(env, "uninstall", STUB_LOADED="1")
    assert rc == 0, out
    assert not list((env["home"] / "Library" / "LaunchAgents").glob("com.example.tab5phone-*.plist"))
    assert (cf / "tab5phone.yml").read_text() == "keep"
    assert not any("tunnel delete" in l for l in calls(env))
    rc, out = sh(env, "uninstall")                                       # 두 번째도 같은 결과
    assert rc == 0


# ── 신호 규약 v2 (docs/exec-plans/2026-10-08-phone-3way-design.md §6.1·§7) ──
def test_install_accepts_three_devices_and_a_legacy_pair_line(env):
    """설치 전 검사는 기기 수를 세지 않는다(2~8대·옛 짝의 판단은 교환기가 뜰 때 한다) — 3대 + PHONE_LEGACY_PAIR 가 든 .env.phone 으로도 그대로 설치된다.
    plist 에는 여전히 토큰도 옛 짝도 없다(교환기가 server/.env.phone 을 스스로 읽는다 — P4-D3)."""
    f = env["repo"] / "server" / ".env.phone"
    f.write_text("PORT=5094\nBIND=127.0.0.1\n"
                 "PHONE_TOKENS=tab5-1:aaaaaaaaaaaaaaaaaaaaaaaa,tab5-2:bbbbbbbbbbbbbbbbbbbbbbbb,stackchan-2:cccccccccccccccccccccccc\n"
                 "PHONE_LEGACY_PAIR=tab5-1,tab5-2\n")
    f.chmod(0o600)
    rc, out = sh(env, "install")
    assert rc == 0, out
    plist = (env["home"] / "Library" / "LaunchAgents" / "com.example.tab5phone-relay.plist").read_text()
    for word in ("PHONE_TOKENS", "PHONE_LEGACY_PAIR", "stackchan", "cccccccc"):
        assert word not in plist and word not in out
    assert any("bootstrap" in l for l in calls(env))


def test_script_has_no_fixed_device_names_or_counts():
    """스크립트는 어떤 기기가 몇 대 등록됐는지 모른다 — 기기 ID·토큰 개수를 전제한 줄이 생기면(예: 설치 전 검사) 이 시험이 알려 준다."""
    code = "\n".join(l for l in SCRIPT_SRC.read_text(encoding="utf-8").splitlines() if not l.strip().startswith("#"))
    for word in ("tab5-1", "tab5-2", "stackchan", "PHONE_LEGACY_PAIR"):
        assert word not in code, word
