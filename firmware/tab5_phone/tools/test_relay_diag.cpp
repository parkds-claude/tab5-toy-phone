// test_relay_diag.cpp — relay_diag.h 의 순수 판단 시험(주소의 scheme, 접속 실패의 원인 가르기, 시간 제한끼리의 관계):
//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_relay_diag.cpp -o /tmp/t_rd && /tmp/t_rd
#include "relay_diag.h"
#include <cstdio>
#include <string>
using namespace relaydiag;
static int ok = 0, bad = 0;
#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)

int main()
{
    // ── scheme: wss 면 TLS(인증서 묶음), ws 면 평문(LAN 개발·되돌리기용), 그 밖은 붙지 않는다 ──
    CHECK("ws:// lan address is plain", scheme_of("ws://192.168.0.10:5094/ws") == Scheme::WS);
    CHECK("wss:// production address is tls", scheme_of("wss://phone.example.com/ws") == Scheme::WSS);
    CHECK("scheme is case-insensitive (the client lib parses it that way)", scheme_of("WSS://phone.example.com/ws") == Scheme::WSS && scheme_of("Ws://h/ws") == Scheme::WS);
    CHECK("http/https are not relay addresses", scheme_of("http://h/ws") == Scheme::NONE && scheme_of("https://h/ws") == Scheme::NONE);
    CHECK("a broken separator is rejected", scheme_of("wss:/h/ws") == Scheme::NONE && scheme_of("ws:h") == Scheme::NONE && scheme_of("wss//h") == Scheme::NONE);
    CHECK("empty, short and null are rejected", scheme_of("") == Scheme::NONE && scheme_of("ws") == Scheme::NONE && scheme_of("wss:") == Scheme::NONE && scheme_of(nullptr) == Scheme::NONE);
    CHECK("a scheme must start the string", scheme_of(" wss://h") == Scheme::NONE && scheme_of("xwss://h") == Scheme::NONE);
    CHECK("names for the boot line", std::string(scheme_name(Scheme::WS)) == "ws" && std::string(scheme_name(Scheme::WSS)) == "wss" && std::string(scheme_name(Scheme::NONE)) == "none");

    // ── 접속 실패의 원인: 전송 계층 오류가 잡혔으면 그것이 원인이다(HTTP 상태 코드는 지난 시도의 것이 남아 있을 수 있다 — transport_ws.c 는 지우지 않는다) ──
    CHECK("401 with no transport error is a token rejection", classify(401, 0, 0) == Fail::AUTH);
    CHECK("dns failure", classify(0, ERR_DNS, 0) == Fail::DNS);
    CHECK("dns failure with a stale 401 is still dns — not 'device registration error'", classify(401, ERR_DNS, 0) == Fail::DNS);
    CHECK("tcp connect failure and timeout", classify(0, ERR_CONNECT, 0) == Fail::TCP && classify(0, ERR_TIMEOUT, 0) == Fail::TCP && classify(0, ERR_SOCKET, 0) == Fail::TCP);
    CHECK("tcp closed by the peer while upgrading", classify(101, ERR_CLOSED_FIN, 0) == Fail::TCP);
    CHECK("tls handshake failure without certificate flags", classify(0, ERR_TLS_HANDSHAKE, 0) == Fail::TLS);
    CHECK("tls handshake failure with certificate flags is a certificate problem", classify(0, ERR_TLS_HANDSHAKE, 0x08) == Fail::CERT);
    CHECK("tls failure with a stale 401 is never a token rejection", classify(401, ERR_TLS_HANDSHAKE, 0) == Fail::TLS && classify(401, ERR_TLS_HANDSHAKE, 0x04) == Fail::CERT);
    CHECK("every mbedtls-side esp-tls code is tls", classify(0, ERR_TLS_FIRST, 0) == Fail::TLS && classify(0, ERR_TLS_SETUP, 0) == Fail::TLS && classify(0, ERR_TLS_LAST, 0) == Fail::TLS);
    CHECK("codes around the mbedtls range are not tls", classify(0, ERR_TLS_FIRST - 1, 0) == Fail::OTHER && classify(0, ERR_TLS_LAST + 1, 0) == Fail::OTHER);
    CHECK("tcp failure with a stale 401 is tcp", classify(401, ERR_CONNECT, 0) == Fail::TCP);
    CHECK("another http status (tunnel down: 502/530, wrong path: 404) is http", classify(502, 0, 0) == Fail::HTTP && classify(530, 0, 0) == Fail::HTTP && classify(404, 0, 0) == Fail::HTTP && classify(403, 0, 0) == Fail::HTTP);
    CHECK("no status and no error is other", classify(0, 0, 0) == Fail::OTHER && classify(-1, 0, 0) == Fail::OTHER);
    CHECK("a stale 101 (the last attempt succeeded) is other, not http", classify(101, 0, 0) == Fail::OTHER);
    CHECK("certificate flags alone (no esp-tls error) do not decide", classify(401, 0, 0x08) == Fail::AUTH);
    CHECK("only AUTH is a token rejection", is_auth(Fail::AUTH) && !is_auth(Fail::DNS) && !is_auth(Fail::TCP) && !is_auth(Fail::TLS) && !is_auth(Fail::CERT) && !is_auth(Fail::HTTP) && !is_auth(Fail::OTHER));
    CHECK("names for the RELAYERR line", std::string(fail_name(Fail::AUTH)) == "auth" && std::string(fail_name(Fail::DNS)) == "dns" && std::string(fail_name(Fail::TCP)) == "tcp"
          && std::string(fail_name(Fail::TLS)) == "tls" && std::string(fail_name(Fail::CERT)) == "cert" && std::string(fail_name(Fail::HTTP)) == "http" && std::string(fail_name(Fail::OTHER)) == "other");
    CHECK("the http status is reported only when it belongs to this attempt", shown_http(Fail::AUTH, 401) == 401 && shown_http(Fail::HTTP, 502) == 502 && shown_http(Fail::DNS, 401) == 0 && shown_http(Fail::TLS, 401) == 0
          && shown_http(Fail::CERT, 101) == 0 && shown_http(Fail::TCP, 401) == 0 && shown_http(Fail::OTHER, 101) == 0);

    // ── 전송 오류 보관함(P4N-D7): 부품은 접속 실패 때 ERROR 사건을 DISCONNECTED 보다 먼저 내고, 그때 esp-tls 오류를 읽어 지운다
    //    (esp_websocket_client.c:227-233·334 → 282-283). 붙은 뒤 첫 실패만 DISCONNECTED 에 오류가 남고, 두 번째 연속 실패부터는 ERROR 가 가져간다 ──
    const TransportErr none{};
    const TransportErr dns{ERR_DNS, 0, 0, 0};
    const TransportErr cert{ERR_TLS_HANDSHAKE, 0x2700, 0x08, 0};
    const TransportErr garbage{0x12345678, -77, 0x7fffffff, 9999};      // 전송 오류 종류가 아닌 사건의 칸 = 초기화되지 않은 값
    {
        ErrStash st;                                                    // 붙은 뒤 첫 실패: ERROR 는 종류가 NONE 이라 칸이 쓰레기 값, DISCONNECTED 가 진짜 값을 들고 온다
        st.on_error(false, garbage);
        CHECK("an ERROR event of another type is not kept (its fields are uninitialised)", !st.has);
        const TransportErr got = st.take(true, dns);
        CHECK("first failure after a connection: DISCONNECTED carries the error itself", got.esp == ERR_DNS && classify(0, got.esp, got.flags) == Fail::DNS);
    }
    {
        ErrStash st;                                                    // 두 번째 연속 실패부터: ERROR 가 오류를 가져가 지우고 DISCONNECTED 에는 0 이 온다
        st.on_error(true, cert);
        CHECK("an ERROR event of the transport type is kept", st.has && st.held.esp == ERR_TLS_HANDSHAKE);
        const TransportErr got = st.take(true, none);
        CHECK("DISCONNECTED with zeros inherits what ERROR took", got.esp == ERR_TLS_HANDSHAKE && got.tls == 0x2700 && got.flags == 0x08);
        CHECK("so a certificate failure after a stale 401 is cert — not 'device registration error'", classify(401, got.esp, got.flags) == Fail::CERT);
        CHECK("the kept error is used once", !st.has && st.held.esp == 0);
        const TransportErr next = st.take(true, none);                  // 다음 실패: ERROR 가 아무것도 못 가져갔다(진짜 401 — 전송 오류 없음)
        CHECK("the next failure does not reuse it", next.esp == 0 && next.tls == 0 && next.flags == 0 && next.sock == 0);
        CHECK("401 with no transport error from either event stays a token rejection (keeps the 60 s interval)", classify(401, next.esp, next.flags) == Fail::AUTH);
    }
    {
        ErrStash st;
        st.on_error(true, none);
        CHECK("an ERROR event with nothing in it keeps nothing", !st.has);
        st.on_error(true, TransportErr{0, 0, 0, 104});
        CHECK("a socket errno alone is kept for the log line", st.has && st.take(true, none).sock == 104);
        st.on_error(true, cert);
        st.clear();                                                     // CONNECTED, 그리고 새 접속 시도의 첫머리(BEFORE_CONNECT)
        CHECK("clear (connected / a new attempt begins) empties it", !st.has && st.take(true, none).esp == 0);
        st.on_error(true, cert);
        const TransportErr own = st.take(true, dns);
        CHECK("an error in DISCONNECTED itself wins over the kept one, and the kept one is dropped", own.esp == ERR_DNS && !st.has);
        st.on_error(true, cert);
        const TransportErr other = st.take(false, garbage);             // DISCONNECTED 가 전송 오류 종류가 아니면 자기 칸은 믿지 않는다
        CHECK("DISCONNECTED of another type: its own fields are ignored, the kept error is used", other.esp == ERR_TLS_HANDSHAKE && other.flags == 0x08);
        CHECK("DISCONNECTED of another type with nothing kept is all zeros", st.take(false, garbage).esp == 0 && !none.any() && cert.any());
    }
    {
        ErrStash st;                                                    // 줄거리 하나: 토큰 거절 → (Wi-Fi 가 흔들려) DNS 실패 두 번 → 다시 토큰 거절
        st.clear(); st.on_error(false, garbage);                                        // 시도 1: 진짜 401(붙은 뒤 첫 실패 — ERROR 종류 NONE)
        TransportErr e1 = st.take(true, none);
        st.clear(); st.on_error(true, dns);                                             // 시도 2: DNS 실패(ERROR 가 가져감), 상태 코드는 묵은 401
        TransportErr e2 = st.take(true, none);
        st.clear(); st.on_error(true, dns);                                             // 시도 3: 또 DNS 실패
        TransportErr e3 = st.take(true, none);
        st.clear(); st.on_error(true, none);                                            // 시도 4: 진짜 401
        TransportErr e4 = st.take(true, none);
        CHECK("401, dns, dns, 401 is read as auth, dns, dns, auth", classify(401, e1.esp, e1.flags) == Fail::AUTH && classify(401, e2.esp, e2.flags) == Fail::DNS
              && classify(401, e3.esp, e3.flags) == Fail::DNS && classify(401, e4.esp, e4.flags) == Fail::AUTH);
    }

    // ── 시간 제한끼리의 관계(P4N-D4): 값이 바뀌어 관계가 깨지면 여기서 걸린다 ──
    CHECK("network timeout leaves 5x room over the measured first tls connect (1.9 s)", NET_TIMEOUT_MS >= 5 * TLS_CONNECT_MEASURED_MS);
    CHECK("two json pings fit inside the relay's 45 s silence rule with room for one loss", 2 * JSON_PING_S + NET_TIMEOUT_MS / 1000 <= RELAY_SILENCE_S);
    CHECK("the control ping keeps cloudflare's 100 s idle cut far away", CTRL_PING_S * 10 <= CLOUDFLARE_IDLE_S && JSON_PING_S * 5 <= CLOUDFLARE_IDLE_S);
    CHECK("a dead link is noticed (pong timeout) before the relay's own rule fires", PONG_TIMEOUT_S < RELAY_SILENCE_S && PONG_TIMEOUT_S >= 2 * CTRL_PING_S);
    CHECK("the values are the ones the device has been running", NET_TIMEOUT_MS == 10000 && CTRL_PING_S == 5 && PONG_TIMEOUT_S == 15 && JSON_PING_S == 15 && WS_TASK_STACK == 8192);

    printf("\n%d ok, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
