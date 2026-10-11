// relay_diag.h — 교환기 접속의 순수 판단(ESP-IDF 없이 맥에서 시험한다 — tools/test_relay_diag.cpp). Phase 4: 주소가 wss:// 면 TLS 로 붙는다.
//   - 주소의 scheme 가르기(ws = 평문·LAN 개발용, wss = TLS·운영)
//   - 접속이 실패한 까닭 가르기(토큰 거절 / DNS / TCP / TLS / 인증서 / HTTP) — 화면의 "기기 등록 오류"는 토큰 거절일 때만 나와야 한다
//   - 접속·생존 확인의 시간 값과 그 사이의 관계
#pragma once
#include <cstdint>

namespace relaydiag {

// ── 주소 ──
enum class Scheme : uint8_t { NONE, WS, WSS };

constexpr char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }

// "ws://…" → WS, "wss://…" → WSS, 그 밖(빈 글·nullptr 포함) → NONE. 대소문자를 가리지 않는다(부품도 scheme 을 그렇게 읽는다 — 서로 다르게 읽으면 TLS 인데 인증서 묶음이 안 걸린다)
constexpr Scheme scheme_of(const char *url)
{
    if (!url) return Scheme::NONE;
    if (lower(url[0]) != 'w' || lower(url[1]) != 's') return Scheme::NONE;
    const int i = lower(url[2]) == 's' ? 3 : 2;
    if (url[i] != ':' || url[i + 1] != '/' || url[i + 2] != '/') return Scheme::NONE;
    return i == 3 ? Scheme::WSS : Scheme::WS;
}

constexpr const char *scheme_name(Scheme s) { return s == Scheme::WSS ? "wss" : s == Scheme::WS ? "ws" : "none"; }

// ── 접속 실패의 까닭 ──
// esp-tls 가 남기는 오류 코드(esp-tls/esp_tls_errors.h:24-50). 여기서는 값으로 들고, relay_link.cpp 의 static_assert 가 진짜 값과 같은지 빌드에서 확인한다.
constexpr int ERR_DNS           = 0x8001;   // ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME
constexpr int ERR_SOCKET        = 0x8002;   // ESP_ERR_ESP_TLS_CANNOT_CREATE_SOCKET
constexpr int ERR_FAMILY        = 0x8003;   // ESP_ERR_ESP_TLS_UNSUPPORTED_PROTOCOL_FAMILY
constexpr int ERR_CONNECT       = 0x8004;   // ESP_ERR_ESP_TLS_FAILED_CONNECT_TO_HOST
constexpr int ERR_SETOPT        = 0x8005;   // ESP_ERR_ESP_TLS_SOCKET_SETOPT_FAILED
constexpr int ERR_TIMEOUT       = 0x8006;   // ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT
constexpr int ERR_CLOSED_FIN    = 0x8008;   // ESP_ERR_ESP_TLS_TCP_CLOSED_FIN
constexpr int ERR_TLS_FIRST     = 0x8010;   // ESP_ERR_MBEDTLS_CERT_PARTLY_OK — mbedTLS 쪽 코드의 처음
constexpr int ERR_TLS_SETUP     = 0x8017;   // ESP_ERR_MBEDTLS_SSL_SETUP_FAILED (메모리 부족이면 여기 — tls=-0x7F00)
constexpr int ERR_TLS_HANDSHAKE = 0x801A;   // ESP_ERR_MBEDTLS_SSL_HANDSHAKE_FAILED
constexpr int ERR_TLS_LAST      = 0x801D;   // ESP_ERR_MBEDTLS_SSL_READ_FAILED — mbedTLS 쪽 코드의 끝

enum class Fail : uint8_t { AUTH, DNS, TCP, TLS, CERT, HTTP, OTHER };

// P4N-D5 접속 단계에서 끊겼을 때 까닭을 가른다.
//   http_status = 접속 응답의 상태 코드(부품이 지난 시도의 값을 지우지 않는다 — tcp_transport/transport_ws.c:314 에서만 쓰고 접속 첫머리에 0 으로 되돌리지 않는다)
//   tls_esp_err = 이번 실패에서 전송 계층이 남긴 esp-tls 오류(없으면 0), cert_flags = 인증서 검증 깃발(mbedtls x509.h 의 BADCERT_*, 없으면 0)
//   규칙: 전송 계층 오류가 있으면 응답을 받기 전에 끊긴 것이다 → 상태 코드는 묵은 값이므로 보지 않는다. 오류가 없을 때만 상태 코드로 가른다.
//   한계: "401 + 전송 오류 없음"은 AUTH 로 둔다 — 진짜 401 이 이어질 때 60초 간격을 지키는 쪽이 더 중요하다(틀린 토큰으로 서버를 두드리지 않는다).
//         그래서 401 을 받은 뒤의 시도가 접속 응답을 기다리다 시간 초과로 끝나면(전송 계층은 아무것도 남기지 않는다 — tcp_transport/transport_ssl.c:266-272) 묵은 401 이 AUTH 로 읽힌다.
//         부품은 "이번 시도에 응답을 받았는가"를 알려 주지 않는다(상태 코드를 읽는 길 하나뿐이고 지우는 길이 없다). 다음에 붙거나 전송 오류가 나면 풀린다.
constexpr Fail classify(int http_status, int tls_esp_err, int cert_flags)
{
    if (tls_esp_err == ERR_DNS) return Fail::DNS;
    if (tls_esp_err == ERR_SOCKET || tls_esp_err == ERR_FAMILY || tls_esp_err == ERR_CONNECT || tls_esp_err == ERR_SETOPT
        || tls_esp_err == ERR_TIMEOUT || tls_esp_err == ERR_CLOSED_FIN) return Fail::TCP;
    if (tls_esp_err >= ERR_TLS_FIRST && tls_esp_err <= ERR_TLS_LAST) return cert_flags ? Fail::CERT : Fail::TLS;
    if (tls_esp_err != 0) return Fail::OTHER;
    if (http_status == 401) return Fail::AUTH;
    if (http_status > 0 && http_status != 101) return Fail::HTTP;      // 101 은 지난번에 붙었을 때의 값이 남은 것
    return Fail::OTHER;
}

constexpr bool is_auth(Fail f) { return f == Fail::AUTH; }

constexpr const char *fail_name(Fail f)
{
    return f == Fail::AUTH ? "auth" : f == Fail::DNS ? "dns" : f == Fail::TCP ? "tcp" : f == Fail::TLS ? "tls"
         : f == Fail::CERT ? "cert" : f == Fail::HTTP ? "http" : "other";
}

// RELAYERR 줄의 http= 칸: 이번 시도의 응답일 때만 싣는다(묵은 값은 0 으로)
constexpr int shown_http(Fail f, int http_status) { return (f == Fail::AUTH || f == Fail::HTTP) ? http_status : 0; }

// ── 전송 오류 보관함(P4N-D7) ──
// 전송 계층이 남긴 오류 한 벌(부품의 사건 자료에서 옮겨 담는다). 전부 0 = 없음
struct TransportErr {
    int esp = 0;        // esp-tls 오류 코드(위의 ERR_*)
    int tls = 0;        // mbedTLS 오류(양수로 온다 — 찍을 때 -0x 를 붙인다)
    int flags = 0;      // 인증서 검증 깃발
    int sock = 0;       // 소켓 errno
    constexpr bool any() const { return esp != 0 || tls != 0 || flags != 0 || sock != 0; }
};

// 부품은 접속이 실패하면 ERROR 사건을 먼저 내고 DISCONNECTED 를 낸다(esp_websocket_client.c:1213·1218 → 1220). 사건을 낼 때마다, 직전 실패가 전송 오류였으면
// esp-tls 오류를 읽고 지운다(:227-233) — 그 표시(error_type)는 실패 때 켜지고(:282) 붙었을 때만 꺼진다(:1251). 그래서 붙은 뒤 첫 실패만 DISCONNECTED 에 오류가 남고,
// 두 번째 연속 실패부터는 ERROR 가 가져가 DISCONNECTED 에는 0 이 온다. ERROR 가 가져간 것을 여기 두었다가 같은 시도의 DISCONNECTED 가 이어받는다.
struct ErrStash {
    TransportErr held;
    bool has = false;

    constexpr void clear() { held = TransportErr{}; has = false; }          // 붙었을 때, 그리고 새 접속 시도의 첫머리
    // ERROR 사건: 부품이 칸을 채운 사건(전송 오류 종류)이고 무엇이든 들어 있을 때만 보관한다. 그 밖의 ERROR 는 칸이 초기화되지 않은 값이다
    constexpr void on_error(bool transport_type, const TransportErr &e) { if (transport_type && e.any()) { held = e; has = true; } }
    // DISCONNECTED 사건: 자기 값(전송 오류 종류일 때만 믿는다)에 esp-tls 오류가 있으면 그것, 없으면 보관값. 어느 쪽이든 보관함은 비운다 — 다음 실패가 다시 쓰지 않게
    constexpr TransportErr take(bool transport_type, const TransportErr &own)
    {
        const TransportErr mine = transport_type ? own : TransportErr{};
        const TransportErr out = (mine.esp != 0 || !has) ? mine : held;
        clear();
        return out;
    }
};

// ── 시간·스택 값(P4N-D3·D4). 관계는 tools/test_relay_diag.cpp 가 지킨다 ──
constexpr int NET_TIMEOUT_MS = 10000;           // TCP 연결·TLS 악수·접속 응답 각각의 한도(esp-tls/esp_tls.c:279·409·569). DNS 는 이 한도 밖이다(lwIP 가 정한다)
constexpr int TLS_CONNECT_MEASURED_MS = 1900;   // Phase 0 실측: 이 기기 → Cloudflare 첫 TLS 연결 1.4~1.9초
constexpr int CTRL_PING_S = 5;                  // 제어 PING 간격(RL-D1)
constexpr int PONG_TIMEOUT_S = 15;              // PONG 이 이만큼 없으면 기기가 끊는다
constexpr int JSON_PING_S = 15;                 // JSON ping 간격 — 교환기의 무소식 규칙이 세는 것은 이것뿐이다
constexpr int RELAY_SILENCE_S = 45;             // 교환기: 글자·이진 프레임이 이만큼 없으면 끊는다
constexpr int CLOUDFLARE_IDLE_S = 100;          // Cloudflare: 오가는 것이 이만큼 없으면 끊는다
constexpr int WS_TASK_STACK = 8192;             // 웹소켓 태스크 스택(내부 메모리). TLS 악수가 여기서 돈다 — 근거와 판정선은 relay_link.cpp P4N-D3

}  // namespace relaydiag
