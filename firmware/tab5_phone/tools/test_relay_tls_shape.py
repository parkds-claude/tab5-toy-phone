"""Phase 4(wss:// — TLS 로 교환기에 붙기)의 소스 모양 시험. 기기 없이 고정할 수 있는 약속만 본다.

    python3 -m pytest -q test_relay_tls_shape.py        (ESP-IDF export 를 하지 않은 셸에서)

지키는 것: wss 면 인증서 묶음을 건다 / 검증을 끄는 설정이 어디에도 없다 / TLS 메모리는 PSRAM / 접속 실패 줄(RELAYERR)의 모양 /
토큰이 로그로 나가는 길이 없다 / 순수 판단(relay_diag.h)의 오류 코드가 진짜 esp-tls 값에 묶여 있다 / 예시 주소가 운영 주소다.
"""
import re
from pathlib import Path

FW = Path(__file__).resolve().parent.parent
RELAY_LINK_CPP = FW / "main" / "relay_link.cpp"
RELAY_DIAG_H = FW / "main" / "relay_diag.h"
SECRETS_EXAMPLE = FW / "main" / "secrets.h.example"
SDKCONFIG_DEFAULTS = FW / "sdkconfig.defaults"


def code_only(text):
    """// 주석을 뺀 코드. 문자열 안의 // (주소의 "ws://")는 주석이 아니다."""
    out = []
    for line in text.splitlines():
        buf, in_str, i = [], False, 0
        while i < len(line):
            c = line[i]
            if in_str:
                buf.append(c)
                if c == "\\" and i + 1 < len(line):
                    buf.append(line[i + 1])
                    i += 1
                elif c == '"':
                    in_str = False
            elif c == '"':
                in_str = True
                buf.append(c)
            elif line.startswith("//", i):
                break
            else:
                buf.append(c)
            i += 1
        out.append("".join(buf).rstrip())
    return "\n".join(out)


def defaults():
    """sdkconfig.defaults 의 설정 줄(주석 제외) → {키: 값}."""
    d = {}
    for line in SDKCONFIG_DEFAULTS.read_text(encoding="utf-8").splitlines():
        m = re.match(r"^(CONFIG_[A-Z0-9_]+)=(.*)$", line.strip())
        if m:
            d[m.group(1)] = m.group(2)
    return d


def test_phase4_wss_attaches_the_certificate_bundle():
    src = code_only(RELAY_LINK_CPP.read_text(encoding="utf-8"))
    assert "if (scheme == relaydiag::Scheme::WSS) cfg.crt_bundle_attach = esp_crt_bundle_attach;" in src
    assert "const relaydiag::Scheme scheme = relaydiag::scheme_of(s_url);" in src
    assert 'strncmp(s_url, "wss://"' not in src                      # 주소를 두 가지 방식으로 읽지 않는다(판단은 relay_diag.h 한 곳)
    assert "#if !CONFIG_MBEDTLS_CERTIFICATE_BUNDLE" in src and "#error" in src   # 묶음이 설정에서 빠지면 빌드가 멈춘다


def test_phase4_nothing_switches_server_verification_off():
    banned = ["skip_cert_common_name_check", "use_global_ca_store", "cert_pem", "MBEDTLS_SSL_VERIFY_NONE", "MBEDTLS_SSL_VERIFY_OPTIONAL",
              "esp_tls_set_global_ca_store", "mbedtls_ssl_conf_authmode"]
    for path in sorted((FW / "main").glob("*.cpp")) + sorted((FW / "main").glob("*.h")):
        if path.name in ("secrets.h", "ui_assets.h", "ui_sprites.h"):                # 비밀과 구운 자산(글자·그림 — 코드가 아니다)
            continue
        src = code_only(path.read_text(encoding="utf-8"))
        for word in banned:
            assert word not in src, f"{path.name}: {word}"
    d = defaults()
    for key in ("CONFIG_ESP_TLS_INSECURE", "CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY", "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_NONE",
                "CONFIG_ESP_TLS_PSK_VERIFICATION"):
        assert d.get(key) != "y", key
    assert d.get("CONFIG_MBEDTLS_CERTIFICATE_BUNDLE") == "y" and d.get("CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL") == "y"
    src = code_only(RELAY_LINK_CPP.read_text(encoding="utf-8"))
    assert re.search(r"#if CONFIG_ESP_TLS_INSECURE \|\| CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY\n#error", src)   # 누가 menuconfig 에서 켜도 빌드가 멈춘다


def test_phase4_the_clock_is_not_trusted_because_the_device_has_none():
    # 시계가 없는 기기에서 유효 기간 검사를 켜면 모든 인증서가 "아직 유효하지 않음"으로 떨어진다(P4N-D1). 켜려면 SNTP 부터.
    assert defaults().get("CONFIG_MBEDTLS_HAVE_TIME_DATE") != "y"
    assert re.search(r"#if CONFIG_MBEDTLS_HAVE_TIME_DATE\n#error", code_only(RELAY_LINK_CPP.read_text(encoding="utf-8")))


def test_phase4_tls_memory_goes_to_psram_and_the_dma_engines_are_off():
    d = defaults()
    assert d.get("CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC") == "y"                 # P4N-D2: mbedTLS 의 할당은 전부 PSRAM
    assert d.get("CONFIG_MBEDTLS_HARDWARE_AES") == "n" and d.get("CONFIG_MBEDTLS_HARDWARE_SHA") == "n"   # P4N-D6: PSRAM 버퍼를 DMA 로 읽는 엔진은 쓰지 않는다
    assert d.get("CONFIG_MBEDTLS_DYNAMIC_BUFFER") != "y" and d.get("CONFIG_ESP_WS_CLIENT_SEPARATE_TX_LOCK") != "y"   # 앞 단계의 결정 그대로
    assert "CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN" not in d                      # 받는 레코드 한도는 기본 16384 그대로(줄이면 Cloudflare 의 인증서 묶음이 안 들어갈 수 있다)
    src = code_only(RELAY_LINK_CPP.read_text(encoding="utf-8"))
    assert re.search(r"#if !CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC \|\| CONFIG_MBEDTLS_HARDWARE_AES \|\| CONFIG_MBEDTLS_HARDWARE_SHA\n#error", src)   # 묵은 sdkconfig 로는 빌드되지 않는다


def test_phase4_the_ws_task_settings_come_from_relay_diag():
    src = code_only(RELAY_LINK_CPP.read_text(encoding="utf-8"))
    for line in ("cfg.task_stack = relaydiag::WS_TASK_STACK;", "cfg.network_timeout_ms = relaydiag::NET_TIMEOUT_MS;",
                 "cfg.ping_interval_sec = relaydiag::CTRL_PING_S;", "cfg.pingpong_timeout_sec = relaydiag::PONG_TIMEOUT_S;"):
        assert line in src, line
    assert "relaydiag::JSON_PING_S * 1000000LL" in src
    assert "cfg.enable_close_reconnect = true;" in src and 'cfg.task_name = "relay_ws";' in src   # 앞 단계의 설정 그대로


def test_phase4_a_failed_connect_is_classified_and_only_a_token_rejection_is_401():
    src = code_only(RELAY_LINK_CPP.read_text(encoding="utf-8"))
    assert "relaydiag::classify(e.esp_ws_handshake_status_code, te.esp, te.flags)" in src
    assert "s_auth_rejected = relaydiag::is_auth(why);" in src                # 이번 시도의 까닭으로 정한다 — 묵은 401 이 남지 않는다
    assert "esp_ws_handshake_status_code == 401" not in src                   # 상태 코드만 보고 정하던 옛 줄은 없다
    assert 'printf("RELAYERR stage=%s esp=0x%x tls=-0x%x flags=0x%x errno=%d http=%d attempt=%d\\n"' in src
    assert 'printf("RELAY up=1 code=0 attempt=%d\\n", 0);' in src             # 기존 줄 모양은 그대로
    assert src.count('printf("RELAY up=0 code=%d attempt=%d\\n", s_auth_rejected.load() ? 401 : 0, s_fails.load());') == 1
    assert src.count('printf("RELAY up=0 code=0 attempt=%d\\n", s_fails.load());') == 2
    assert 'printf("RELAYCFG scheme=%s tls=%d\\n"' in src                     # 부팅 때 한 번: 어느 길로 붙는 빌드인지(주소는 싣지 않는다)
    assert 'printf("PHONE FAIL relay scheme\\n");' in src                    # ws:// 도 wss:// 도 아니면 붙지 않는다


def ws_event_cases():
    """on_ws_event 의 case 덩이 → {사건 이름: 그 덩이의 코드}."""
    src = code_only(RELAY_LINK_CPP.read_text(encoding="utf-8"))
    body = src[src.index("void on_ws_event("):src.index("void relay_task(")]
    parts = re.split(r"case WEBSOCKET_EVENT_([A-Z_]+):", body)
    return {parts[i]: parts[i + 1] for i in range(1, len(parts) - 1, 2)}


def test_phase4_the_transport_error_taken_by_the_error_event_is_handed_to_disconnected():
    # P4N-D7: 부품은 접속 실패 때 ERROR 를 DISCONNECTED 보다 먼저 내며 esp-tls 오류를 읽어 지운다 → ERROR 에서 보관하고 DISCONNECTED 가 이어받는다
    cases = ws_event_cases()
    assert "ERROR" in cases                                                    # 전에는 default 로 흘려보냈다
    err = cases["ERROR"]
    assert "if (d->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) s_err_stash.on_error(true, transport_err(d->error_handle));" in err
    for word in ("s_phase =", "post(", "schedule_reconnect", "s_auth_rejected", "printf("):
        assert word not in err, word                                           # ERROR 는 여전히 상태 전이에 쓰지 않는다 — 오류 정보만 보관
    dis = cases["DISCONNECTED"]
    assert "const bool tr = e.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT;" in dis   # 이 종류일 때만 부품이 칸을 채운다(아니면 쓰레기 값)
    assert "s_err_stash.take(tr, tr ? transport_err(e) : relaydiag::TransportErr{})" in dis
    assert dis.index("s_err_stash.take(") < dis.index("relaydiag::classify(") < dis.index("s_auth_rejected = relaydiag::is_auth(why);")
    assert "s_err_stash.clear();" in cases["BEFORE_CONNECT"]                   # 접속 시도마다 빈 보관함으로 시작한다
    assert "s_err_stash.clear();" in cases["CONNECTED"]
    assert "s_err_stash" not in cases["DATA"] and "s_err_stash" not in cases["CLOSED"]
    src = code_only(RELAY_LINK_CPP.read_text(encoding="utf-8"))
    assert src.count("s_err_stash.") == 4 and "relaydiag::ErrStash s_err_stash;" in src   # 핸들러 밖에서는 만지지 않는다(핸들러는 부품의 잠금 아래 차례로 불린다)


def test_phase4_no_log_line_can_carry_the_token_or_the_address():
    src = code_only(RELAY_LINK_CPP.read_text(encoding="utf-8"))
    for line in src.splitlines():
        if not (re.search(r"(?<![a-z_])printf\(", line) or "ESP_LOG" in line):
            continue
        bare = re.sub(r'"(?:[^"\\]|\\.)*"', '""', line)                   # 글자 상수를 비우고 남는 것 = 인자
        for word in ("s_headers", "token", "s_url", "dev_id", "url"):
            assert not re.search(rf"\b{word}\b", bare), line               # 토큰·헤더·주소(LAN 주소일 수 있다)·ID 를 인자로 넘기지 않는다
    assert 'esp_log_level_set("transport_ws", ESP_LOG_NONE);' in src                        # RL-D9 그대로(접속 요청 전문을 찍는 부품 로그를 막는다)
    assert "memset(s_headers, 0, sizeof s_headers);" in src


def test_phase4_the_pure_error_codes_are_tied_to_the_real_esp_tls_values():
    hdr = RELAY_DIAG_H.read_text(encoding="utf-8")
    src = code_only(RELAY_LINK_CPP.read_text(encoding="utf-8"))
    pairs = re.findall(r"^constexpr int (ERR_[A-Z_]+)\s+= 0x[0-9A-Fa-f]{4};\s+// (ESP_ERR_[A-Z0-9_]+)", hdr, re.M)
    assert len(pairs) == 11
    for name, real in pairs:
        assert f"static_assert(relaydiag::{name} == {real}" in src, name


def test_phase4_the_example_address_is_the_production_one():
    ex = SECRETS_EXAMPLE.read_text(encoding="utf-8")
    assert '#define RELAY_URL "wss://phone.example.com/ws"' in ex
    assert "ws://192.168.0.10:5094/ws" in ex                                   # LAN 으로 되돌리는 꼴도 주석에 남긴다
    assert len(re.findall(r"^#define RELAY_URL ", ex, re.M)) == 1
