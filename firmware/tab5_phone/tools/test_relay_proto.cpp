// test_relay_proto.cpp — relay_proto.h 단위 시험:
//   clang++ -std=c++17 -Wall -Wextra -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -I../main test_relay_proto.cpp -o /tmp/t_proto && /tmp/t_proto
// 교환기(server/phone_core.py)가 실제로 내는 글자와 같은 모양을 넣어 본다. 깨진 입력에 죽지 않는 것이 핵심(검토 초점).
#include "relay_proto.h"
#include <cstdio>
#include <cstring>
#include <string>
using namespace relayproto;
using callfsm::Ev;
using callfsm::Why;
static int ok = 0, bad = 0;
#define CHECK(name, cond) do { if (cond) { ++ok; printf("  ok   %s\n", name); } else { ++bad; printf("  FAIL %s\n", name); } } while (0)

static Signal P(const std::string &s) { return parse(s.data(), s.size()); }

int main()
{
    // ── 교환기가 보내는 신호 (json.dumps 기본 모양: 쉼표·쌍점 뒤에 공백) ─────
    { Signal s = P("{\"t\": \"peer\", \"online\": true}");  CHECK("peer online",  s.kind == Kind::EVENT && s.ev == Ev::PEER && s.flag); }
    { Signal s = P("{\"t\": \"peer\", \"online\": false}"); CHECK("peer offline", s.kind == Kind::EVENT && s.ev == Ev::PEER && !s.flag); }
    { Signal s = P("{\"t\":\"peer\",\"online\":true}");       CHECK("compact form too", s.kind == Kind::EVENT && s.ev == Ev::PEER && s.flag); }
    { Signal s = P("{\"t\": \"ringing\"}");   CHECK("ringing",   s.kind == Kind::EVENT && s.ev == Ev::RINGING); }
    { Signal s = P("{\"t\": \"ring\"}");      CHECK("ring (not confused with ringing)", s.kind == Kind::EVENT && s.ev == Ev::RING); }
    { Signal s = P("{\"t\": \"connected\"}"); CHECK("connected", s.kind == Kind::EVENT && s.ev == Ev::CONNECTED); }
    {
        const char *names[] = { "hangup", "reject", "timeout", "offline", "lost" };
        const Why want[] = { Why::HANGUP, Why::REJECT, Why::TIMEOUT, Why::OFFLINE, Why::LOST };
        bool all = true;
        for (int i = 0; i < 5; ++i) {
            Signal s = P(std::string("{\"t\": \"ended\", \"why\": \"") + names[i] + "\"}");
            all = all && s.kind == Kind::EVENT && s.ev == Ev::ENDED && s.why == want[i];
        }
        CHECK("ended with each why", all);
    }
    { Signal s = P("{\"t\": \"ended\", \"why\": \"weird\"}"); CHECK("ended with unknown why still ends the call", s.kind == Kind::EVENT && s.ev == Ev::ENDED && s.why == Why::HANGUP); }
    { Signal s = P("{\"t\": \"ended\"}");                     CHECK("ended without why still ends the call", s.kind == Kind::EVENT && s.ev == Ev::ENDED); }
    { Signal s = P("{\"t\": \"pong\", \"n\": 41}");          CHECK("pong n", s.kind == Kind::PONG && s.n == 41); }
    { Signal s = P("{\"why\": \"lost\", \"t\": \"ended\"}"); CHECK("key order does not matter", s.kind == Kind::EVENT && s.ev == Ev::ENDED && s.why == Why::LOST); }

    // ── 깨진·모르는 입력은 버린다(죽지 않는다) ──────────────────────────────
    CHECK("empty", P("").kind == Kind::NONE);
    CHECK("not json", P("hello").kind == Kind::NONE);
    CHECK("unknown t", P("{\"t\": \"selfdestruct\"}").kind == Kind::NONE);
    CHECK("t is a number", P("{\"t\": 5}").kind == Kind::NONE);
    CHECK("no t", P("{\"online\": true}").kind == Kind::NONE);
    CHECK("truncated", P("{\"t\": \"end").kind == Kind::NONE);
    CHECK("peer without online", P("{\"t\": \"peer\"}").kind == Kind::NONE);
    CHECK("peer online not bool", P("{\"t\": \"peer\", \"online\": 1}").kind == Kind::NONE);
    CHECK("pong without n", P("{\"t\": \"pong\"}").kind == Kind::NONE);
    CHECK("pong n not a number", P("{\"t\": \"pong\", \"n\": \"x\"}").kind == Kind::NONE);
    CHECK("device-only signal echoed back is ignored", P("{\"t\": \"call\"}").kind == Kind::NONE);
    { std::string big(5000, 'a'); CHECK("oversized text ignored", P("{\"t\": \"" + big + "\"}").kind == Kind::NONE); }
    { const char raw[] = { '{', '"', 't', '"', ':', '"', 'r', 0, 'n', 'g', '"', '}' }; CHECK("embedded NUL ignored", parse(raw, sizeof raw).kind == Kind::NONE); }
    CHECK("null pointer", parse(nullptr, 0).kind == Kind::NONE);
    {   // 값 안에 들어 있는 "t" 를 열쇠로 착각하지 않는다
        Signal s = P("{\"x\": \"\\\"t\\\": \\\"ring\\\"\", \"t\": \"connected\"}");
        CHECK("key inside a string value is not a key", s.kind == Kind::EVENT && s.ev == Ev::CONNECTED);
    }

    // ── 보내는 글자 ─────────────────────────────────────────────────────────
    CHECK("call text",   std::string(text_for(callfsm::A_SEND_CALL))   == "{\"t\":\"call\"}");
    CHECK("accept text", std::string(text_for(callfsm::A_SEND_ACCEPT)) == "{\"t\":\"accept\"}");
    CHECK("reject text", std::string(text_for(callfsm::A_SEND_REJECT)) == "{\"t\":\"reject\"}");
    CHECK("hangup text", std::string(text_for(callfsm::A_SEND_HANGUP)) == "{\"t\":\"hangup\"}");
    CHECK("no text for non-send action", text_for(callfsm::A_TONE_BELL) == nullptr);
    { char b[40]; int n = ping_text(b, sizeof b, 7); CHECK("ping text", n == (int)strlen(b) && std::string(b) == "{\"t\":\"ping\",\"n\":7}"); }
    { char b[8];  CHECK("ping text does not overflow a small buffer", ping_text(b, sizeof b, 123456) == 0); }

    // ── 음성 프레임: [순번 2바이트(큰 자리 먼저)][Opus] ─────────────────────
    {
        uint8_t opus[60]; for (int i = 0; i < 60; ++i) opus[i] = (uint8_t)(i + 1);
        uint8_t frame[80];
        size_t n = pack_voice(frame, sizeof frame, 0x1234, opus, 60);
        CHECK("pack voice: size and seq bytes", n == 62 && frame[0] == 0x12 && frame[1] == 0x34 && frame[2] == 1 && frame[61] == 60);
        uint16_t seq = 0; const uint8_t *body = nullptr; size_t blen = 0;
        CHECK("unpack voice roundtrip", unpack_voice(frame, n, &seq, &body, &blen) && seq == 0x1234 && blen == 60 && body == frame + 2);
        CHECK("pack voice refuses a frame over 400 bytes", pack_voice(frame, sizeof frame, 1, opus, 399) == 0);
        uint8_t bigbuf[500];
        CHECK("pack voice: 398 + 2 = 400 is allowed", pack_voice(bigbuf, sizeof bigbuf, 1, bigbuf + 2, 398) == 400);
        CHECK("pack voice: empty opus refused", pack_voice(frame, sizeof frame, 1, opus, 0) == 0);
        CHECK("pack voice: output too small refused", pack_voice(frame, 10, 1, opus, 60) == 0);
        CHECK("unpack voice: too short", !unpack_voice(frame, 2, &seq, &body, &blen));
        CHECK("unpack voice: over 400 refused", !unpack_voice(bigbuf, 401, &seq, &body, &blen));
        CHECK("unpack voice: null", !unpack_voice(nullptr, 10, &seq, &body, &blen));
    }

    // ── 재연결 간격: 1 → 2 → 4 … 최대 30초 (§9.5), 토큰 거절은 60초 (§9.7) ──
    CHECK("backoff 1s first", backoff_ms(0, false) == 1000);
    CHECK("backoff doubles", backoff_ms(1, false) == 2000 && backoff_ms(2, false) == 4000 && backoff_ms(4, false) == 16000);
    CHECK("backoff capped at 30s", backoff_ms(5, false) == 30000 && backoff_ms(50, false) == 30000 && backoff_ms(-3, false) == 1000);
    CHECK("auth failure waits 60s", backoff_ms(0, true) == 60000 && backoff_ms(9, true) == 60000);


    // ════ 수신거부 메시지: ended(why=reject) 의 msg, 보낼 때의 reject_text (설계서 decline-mute §5.2) ════
    {
        bool all = true;
        for (int n = 1; n <= 3; ++n) {
            const std::string d = std::to_string(n);
            Signal a = P("{\"t\":\"ended\",\"why\":\"reject\",\"msg\":" + d + "}");            // 교환기가 실제로 보내는 모양(공백 없음)
            Signal b = P("{\"t\": \"ended\", \"why\": \"reject\", \"msg\": " + d + "}");
            Signal c = P("{\"msg\": " + d + ", \"t\": \"ended\", \"why\": \"reject\"}");
            for (const Signal &s : { a, b, c }) all = all && s.kind == Kind::EVENT && s.ev == Ev::ENDED && s.why == Why::REJECT && s.msg == n;
        }
        CHECK("ended reject with msg 1..3 (compact, spaced, any key order)", all);
    }
    { Signal s = P("{\"t\":\"ended\",\"why\":\"reject\"}"); CHECK("ended reject without msg: 0 (old relay)", s.kind == Kind::EVENT && s.ev == Ev::ENDED && s.why == Why::REJECT && s.msg == 0); }
    {   // 번호가 틀려도 거절은 거절이다 — 통화는 끝나고 번호만 0 이 된다(4~99 는 교환기가 건네는 범위지만 기기가 모르는 번호)
        bool all = true;
        for (const char *v : { "0", "4", "99", "100", "-1", "2.0", "1e0", "\"2\"", "true", "null", "4294967297", "4294967298", "99999999999999999999" }) {
            Signal s = P(std::string("{\"t\":\"ended\",\"why\":\"reject\",\"msg\":") + v + "}");
            const bool good = s.kind == Kind::EVENT && s.ev == Ev::ENDED && s.why == Why::REJECT && s.msg == 0;
            if (!good) printf("       msg=%s -> kind=%d msg=%d\n", v, (int)s.kind, (int)s.msg);
            all = all && good;
        }
        CHECK("wrong msg = plain reject, call still ends", all);
    }
    {
        bool all = true;
        for (const char *why : { "hangup", "timeout", "offline", "lost", "weird" }) {
            Signal s = P(std::string("{\"t\":\"ended\",\"why\":\"") + why + "\",\"msg\":2}");
            all = all && s.kind == Kind::EVENT && s.ev == Ev::ENDED && s.why != Why::REJECT && s.msg == 0;
        }
        Signal nowhy = P("{\"t\":\"ended\",\"msg\":2}");
        CHECK("msg on a non-reject ended is dropped", all && nowhy.ev == Ev::ENDED && nowhy.msg == 0);
    }
    { Signal s = P("{\"t\":\"ring\",\"msg\":2}");          CHECK("msg on ring is dropped, ring still works", s.kind == Kind::EVENT && s.ev == Ev::RING && s.msg == 0); }
    { Signal s = P("{\"t\":\"pong\",\"n\":7,\"msg\":2}");  CHECK("pong unaffected", s.kind == Kind::PONG && s.n == 7 && s.msg == 0); }
    {
        Signal a = P("{\"t\":\"ended\",\"why\":\"reject\",\"msg\":1,\"msg\":3}");
        Signal b = P("{\"t\":\"ended\",\"why\":\"reject\",\"msg\":2,\"msg\":9}");
        CHECK("repeated msg: last one wins", a.msg == 3 && b.ev == Ev::ENDED && b.msg == 0);
    }
    CHECK("msg as array/object drops the whole signal (existing rule)",
          P("{\"t\":\"ended\",\"why\":\"reject\",\"msg\":[2]}").kind == Kind::NONE && P("{\"t\":\"ended\",\"why\":\"reject\",\"msg\":{\"a\":2}}").kind == Kind::NONE);
    { Signal s = P("{\"t\":\"ended\",\"why\":\"reject\",\"x\":\"\\\"msg\\\":2\"}"); CHECK("msg inside a string value is not a key", s.kind == Kind::EVENT && s.ev == Ev::ENDED && s.why == Why::REJECT && s.msg == 0); }
    {
        char b[40]; bool all = true;
        for (int n = 1; n <= 3; ++n) {
            const int len = reject_text(b, sizeof b, (uint8_t)n);
            all = all && len == (int)strlen(b) && std::string(b) == "{\"t\":\"reject\",\"msg\":" + std::to_string(n) + "}";
        }
        CHECK("reject text with number 1..3", all);
        int len = reject_text(b, sizeof b, 0);
        CHECK("reject text with 0 = plain", len == (int)strlen(b) && std::string(b) == text_for(callfsm::A_SEND_REJECT));
        reject_text(b, sizeof b, 4); const std::string four = b;
        len = reject_text(b, sizeof b, 255);
        CHECK("reject text out-of-range = plain", four == "{\"t\":\"reject\"}" && std::string(b) == "{\"t\":\"reject\"}" && len == 14);
        char small[10];
        CHECK("reject text does not overflow a small buffer", reject_text(small, sizeof small, 2) == 0 && reject_text(small, sizeof small, 0) == 0);
        CHECK("plain reject text unchanged", std::string(text_for(callfsm::A_SEND_REJECT)) == "{\"t\":\"reject\"}");
        Signal back = P("{\"t\":\"ended\",\"why\":\"reject\",\"msg\":99}");                   // 가장 긴 새 프레임(37바이트)도 상한 256 안
        CHECK("longest new frame fits the text limit", std::string("{\"t\":\"ended\",\"why\":\"reject\",\"msg\":99}").size() == 37 && back.kind == Kind::EVENT);
    }


    // ════ 신호 규약 v2 — 기기 ID 를 읽고 쓴다 (설계서 docs/exec-plans/2026-10-08-phone-3way-design.md §6, relay_proto.h RP-D2~D5) ════
    const auto id_is = [](const Signal &s, const char *want) { return std::string(s.peer_id) == want; };

    // ── 읽기: peer 의 "id", ring 의 "from", ended 의 why:"busy" ──
    { Signal s = P("{\"t\":\"peer\",\"id\":\"tab5-2\",\"online\":true}");       CHECK("v2 peer online with id", s.kind == Kind::EVENT && s.ev == Ev::PEER && s.flag && id_is(s, "tab5-2")); }
    { Signal s = P("{\"t\":\"peer\",\"id\":\"stackchan-2\",\"online\":false}"); CHECK("v2 peer offline with id", s.kind == Kind::EVENT && s.ev == Ev::PEER && !s.flag && id_is(s, "stackchan-2")); }
    { Signal s = P("{\"t\": \"peer\", \"id\": \"tab5-1\", \"online\": true}");  CHECK("v2 peer, spaced form", s.kind == Kind::EVENT && s.ev == Ev::PEER && s.flag && id_is(s, "tab5-1")); }
    { Signal s = P("{\"online\":true,\"id\":\"tab5-1\",\"t\":\"peer\"}");        CHECK("v2 peer, any key order", s.kind == Kind::EVENT && s.ev == Ev::PEER && s.flag && id_is(s, "tab5-1")); }
    { Signal s = P("{\"t\":\"ring\",\"from\":\"stackchan-2\"}");                 CHECK("v2 ring with from", s.kind == Kind::EVENT && s.ev == Ev::RING && id_is(s, "stackchan-2")); }
    { Signal s = P("{\"from\": \"tab5-1\", \"t\": \"ring\"}");                    CHECK("v2 ring, spaced and reordered", s.kind == Kind::EVENT && s.ev == Ev::RING && id_is(s, "tab5-1")); }
    { Signal s = P("{\"t\":\"ended\",\"why\":\"busy\"}");                         CHECK("ended busy", s.kind == Kind::EVENT && s.ev == Ev::ENDED && s.why == Why::BUSY && s.msg == 0 && id_is(s, "")); }
    { Signal s = P("{\"t\":\"ended\",\"why\":\"busy\",\"msg\":2}");               CHECK("ended busy drops a stray msg", s.ev == Ev::ENDED && s.why == Why::BUSY && s.msg == 0); }
    CHECK("ended 'busy' is matched exactly (not Busy, busy!, bus)",
          P("{\"t\":\"ended\",\"why\":\"Busy\"}").why == Why::HANGUP && P("{\"t\":\"ended\",\"why\":\"busy!\"}").why == Why::HANGUP && P("{\"t\":\"ended\",\"why\":\"bus\"}").why == Why::HANGUP);

    // ── v1 모양(id 없음)도 지금처럼 읽힌다 — peer_id 는 빈 글자 ──
    {
        Signal a = P("{\"t\":\"peer\",\"online\":true}"), b = P("{\"t\":\"ring\"}"), c = P("{\"t\": \"peer\", \"online\": false}");
        CHECK("v1 peer and ring still parse, without an id", a.kind == Kind::EVENT && a.ev == Ev::PEER && a.flag && id_is(a, "") && b.kind == Kind::EVENT && b.ev == Ev::RING && id_is(b, "")
              && c.ev == Ev::PEER && !c.flag && id_is(c, ""));
        bool none = true;
        for (const char *t : { "{\"t\":\"ringing\"}", "{\"t\":\"connected\"}", "{\"t\":\"ended\",\"why\":\"lost\"}", "{\"t\":\"pong\",\"n\":3}" }) none = none && id_is(P(t), "") && P(t).kind != Kind::NONE;
        CHECK("signals that carry no id have an empty peer_id", none && id_is(P("garbage"), "") && id_is(Signal{}, ""));
    }

    // ── 열쇠는 신호마다 하나다: peer 는 "id", ring 은 "from". 다른 신호에 붙은 것은 버린다 ──
    { Signal s = P("{\"t\":\"peer\",\"from\":\"tab5-1\",\"online\":true}"); CHECK("peer reads 'id', not 'from'", s.ev == Ev::PEER && s.flag && id_is(s, "")); }
    { Signal s = P("{\"t\":\"ring\",\"id\":\"tab5-1\"}");                    CHECK("ring reads 'from', not 'id'", s.ev == Ev::RING && id_is(s, "")); }
    { Signal s = P("{\"t\":\"ring\",\"id\":\"tab5-1\",\"from\":\"tab5-2\"}"); CHECK("ring with both keys uses 'from'", s.ev == Ev::RING && id_is(s, "tab5-2")); }
    { Signal s = P("{\"t\":\"peer\",\"from\":\"tab5-1\",\"id\":\"tab5-2\",\"online\":true}"); CHECK("peer with both keys uses 'id'", s.ev == Ev::PEER && id_is(s, "tab5-2")); }
    {
        bool none = true;
        for (const char *t : { "{\"t\":\"ringing\",\"id\":\"tab5-1\"}", "{\"t\":\"connected\",\"from\":\"tab5-1\"}", "{\"t\":\"ended\",\"why\":\"busy\",\"id\":\"tab5-1\",\"from\":\"tab5-2\"}",
                               "{\"t\":\"pong\",\"n\":1,\"id\":\"tab5-1\"}" }) none = none && P(t).kind != Kind::NONE && id_is(P(t), "");
        CHECK("an id on ringing/connected/ended/pong is dropped, the signal still works", none);
    }
    { Signal s = P("{\"t\":\"peer\",\"id\":\"tab5-2\",\"online\":true,\"to\":\"x\",\"extra\":12}"); CHECK("unknown extra keys do not disturb the id", s.ev == Ev::PEER && s.flag && id_is(s, "tab5-2")); }
    {   // 이스케이프가 든 열쇠는 그 열쇠가 아니다(기존 규칙: online·n·msg 와 같다) — ID 를 정하지도, 지우지도 않는다
        Signal a = P("{\"t\":\"peer\",\"\\id\":\"tab5-1\",\"online\":true}"), b = P("{\"t\":\"ring\",\"fr\\om\":\"tab5-1\"}");
        Signal c = P("{\"t\":\"peer\",\"id\":\"tab5-1\",\"\\id\":5,\"online\":true}"), d = P("{\"t\":\"ring\",\"from\":\"tab5-2\",\"\\from\":\"tab5-1\"}");
        CHECK("an escaped key is neither 'id' nor 'from'", a.ev == Ev::PEER && a.flag && id_is(a, "") && b.ev == Ev::RING && id_is(b, "") && c.ev == Ev::PEER && id_is(c, "tab5-1") && d.ev == Ev::RING && id_is(d, "tab5-2"));
    }

    // ── id 의 모양: 1~15자, 눈에 보이는 ASCII(따옴표·역슬래시 제외). 틀리면 "아이디 없음"으로 — 신호는 살아 있고 호출자가 버린다 ──
    { Signal s = P("{\"t\":\"peer\",\"id\":\"abcdefghijklmno\",\"online\":true}");  CHECK("id of 15 chars is kept", s.ev == Ev::PEER && id_is(s, "abcdefghijklmno")); }
    { Signal s = P("{\"t\":\"peer\",\"id\":\"abcdefghijklmnop\",\"online\":true}"); CHECK("id of 16 chars = no id (never a cut-off id)", s.kind == Kind::EVENT && s.ev == Ev::PEER && s.flag && id_is(s, "")); }
    { Signal s = P("{\"t\":\"ring\",\"from\":\"" + std::string(100, 'a') + "\"}");  CHECK("a very long from = no id", s.kind == Kind::EVENT && s.ev == Ev::RING && id_is(s, "")); }
    { Signal s = P("{\"t\":\"peer\",\"id\":\"\",\"online\":true}");                 CHECK("empty id = no id", s.ev == Ev::PEER && id_is(s, "")); }
    {
        bool all = true;
        for (const char *v : { "\"tab5\\u002d2\"", "\"tab5\\\\2\"", "\"a\\\"b\"", "\"tab5\\n\"", "\"tab5 2\"", "\" tab5-2\"", "\"\xEA\xB3\xB0\"", "\"tab5\x7F\"", "\"\xFF\"",
                               "5", "-1", "2.5", "true", "false", "null" }) {
            Signal a = P(std::string("{\"t\":\"peer\",\"id\":") + v + ",\"online\":true}");
            Signal b = P(std::string("{\"t\":\"ring\",\"from\":") + v + "}");
            const bool good = a.kind == Kind::EVENT && a.ev == Ev::PEER && a.flag && id_is(a, "") && b.kind == Kind::EVENT && b.ev == Ev::RING && id_is(b, "");
            if (!good) printf("       id=%s -> peer kind=%d id='%s' / ring kind=%d id='%s'\n", v, (int)a.kind, a.peer_id, (int)b.kind, b.peer_id);
            all = all && good;
        }
        CHECK("id that is escaped, spaced, non-ASCII, DEL, a number, true/false or null = no id (the signal survives)", all);
    }
    CHECK("id as array/object drops the whole signal (existing rule: no nesting)",
          P("{\"t\":\"peer\",\"id\":[\"tab5-1\"],\"online\":true}").kind == Kind::NONE && P("{\"t\":\"peer\",\"id\":{\"x\":\"tab5-1\"},\"online\":true}").kind == Kind::NONE
          && P("{\"t\":\"ring\",\"from\":{\"id\":\"tab5-1\"}}").kind == Kind::NONE && P("{\"t\":\"ring\",\"from\":[]}").kind == Kind::NONE && P("{\"t\":\"ring\",\"x\":{\"t\":\"ring\"}}").kind == Kind::NONE);
    {   // 같은 열쇠가 두 번: 마지막 것이 이긴다(msg 와 같은 규칙) — 마지막이 틀린 값이면 "없음"
        Signal a = P("{\"t\":\"peer\",\"id\":\"tab5-1\",\"id\":\"tab5-2\",\"online\":true}");
        Signal b = P("{\"t\":\"peer\",\"id\":\"tab5-1\",\"id\":5,\"online\":true}");
        Signal c = P("{\"t\":\"peer\",\"id\":5,\"id\":\"tab5-1\",\"online\":true}");
        Signal d = P("{\"t\":\"ring\",\"from\":\"tab5-1\",\"from\":\"abcdefghijklmnop\"}");
        Signal e = P("{\"t\":\"ring\",\"from\":\"tab5-1\",\"from\":null}");
        Signal f = P("{\"t\":\"peer\",\"id\":\"tab5-1\",\"online\":true,\"online\":false}");
        Signal g = P("{\"t\":\"ring\",\"from\":\"tab5-1\",\"t\":\"peer\",\"id\":\"tab5-2\",\"online\":true}");
        CHECK("repeated id/from: the last one wins, a bad last one means no id", id_is(a, "tab5-2") && a.ev == Ev::PEER && id_is(b, "") && b.ev == Ev::PEER && id_is(c, "tab5-1") && id_is(d, "") && d.ev == Ev::RING && id_is(e, "") && e.ev == Ev::RING);
        CHECK("repeated online / repeated t: the last one wins too", f.ev == Ev::PEER && !f.flag && id_is(f, "tab5-1") && g.kind == Kind::EVENT && g.ev == Ev::PEER && g.flag && id_is(g, "tab5-2"));
    }

    // ── 깨진 입력: 죽지 않고 통째로 버린다 ──
    {
        bool none = true;
        for (const char *t : { "{\"t\":\"peer\",\"id\":\"tab5-2,\"online\":true}",       // id 의 닫는 따옴표가 엉뚱한 자리
                               "{\"t\":\"ring\",\"from\":\"stackchan-2}",                 // 닫히지 않은 따옴표
                               "{\"t\":\"ring\",\"from\":\"stackchan-2\"",                // 닫히지 않은 객체
                               "{\"t\":\"ring\",\"from\":}", "{\"t\":\"ring\",\"from\"}", "{\"t\":\"ring\",\"from\":\"a\",}", "{\"t\":\"ring\" \"from\":\"a\"}",
                               "{\"t\":\"peer\",\"id\":tab5-1,\"online\":true}",           // 따옴표 없는 id
                               "{\"t\":\"peer\",\"id\":\"tab5-1\",\"online\":true}}", "[{\"t\":\"ring\",\"from\":\"tab5-1\"}]", "{\"t\":\"ring\",\"from\":\"tab5-1\"} x",
                               "{{\"t\":\"ring\"}}", "{\"t\":{\"t\":\"ring\"},\"from\":\"tab5-1\"}" }) {
            const bool good = P(t).kind == Kind::NONE && id_is(P(t), "");
            if (!good) printf("       not dropped: %s\n", t);
            none = none && good;
        }
        CHECK("broken v2 text is dropped whole (stray quote, unclosed string/object, missing value, bare word, nesting, trailing junk)", none);
        const char nul_id[] = { '{', '"', 't', '"', ':', '"', 'r', 'i', 'n', 'g', '"', ',', '"', 'f', 'r', 'o', 'm', '"', ':', '"', 't', 'a', 0, 'b', '"', '}' };
        CHECK("NUL inside an id drops the signal", parse(nul_id, sizeof nul_id).kind == Kind::NONE);
        bool cut = true;                                                       // 온전한 글자를 어디서 잘라도 죽지 않고 버린다
        const std::string whole = "{\"t\":\"peer\",\"id\":\"stackchan-2\",\"online\":false}";
        for (size_t n = 0; n < whole.size(); ++n) cut = cut && parse(whole.data(), n).kind == Kind::NONE;
        CHECK("every truncation of a v2 peer signal is dropped", cut && P(whole).kind == Kind::EVENT);
    }
    {   // 길이 상한 256바이트(교환기 MAX_TEXT_BYTES 와 같은 값): 딱 256 은 읽고 257 은 버린다
        const std::string head = "{\"t\":\"peer\",\"id\":\"tab5-2\",\"online\":true", tail = "}";
        const std::string at = head + std::string(MAX_TEXT - head.size() - tail.size(), ' ') + tail;
        const std::string over = head + std::string(MAX_TEXT - head.size() - tail.size() + 1, ' ') + tail;
        Signal a = P(at), b = P(over);
        CHECK("256 bytes is read, 257 is dropped", at.size() == 256 && a.kind == Kind::EVENT && a.ev == Ev::PEER && id_is(a, "tab5-2") && over.size() == 257 && b.kind == Kind::NONE && id_is(b, ""));
        Signal c = P("{\"t\":\"peer\",\"id\":\"" + std::string(300, 'x') + "\",\"online\":true}");
        Signal d = P("{\"t\":\"peer\",\"id\":\"" + std::string(200, 'x') + "\",\"online\":true}");
        CHECK("an id that makes the text too long drops it; a long id inside the limit only loses the id", c.kind == Kind::NONE && d.kind == Kind::EVENT && d.ev == Ev::PEER && id_is(d, ""));
        CHECK("longest real v2 frames fit easily", std::string("{\"t\":\"peer\",\"id\":\"abcdefghijklmno\",\"online\":false}").size() == 50 && std::string("{\"t\":\"call\",\"to\":\"abcdefghijklmno\"}").size() == 35);
    }

    // ── 상대에 달린 신호인가(peer·ring) — 호출자가 slot 을 찾아야 하는 신호 ──
    {
        const bool yes = about_peer(P("{\"t\":\"peer\",\"id\":\"tab5-1\",\"online\":true}")) && about_peer(P("{\"t\":\"ring\",\"from\":\"tab5-1\"}"))
                      && about_peer(P("{\"t\":\"peer\",\"online\":true}")) && about_peer(P("{\"t\":\"ring\"}"));   // id 가 없어도 "상대에 달린 신호"다 — 그래서 버려진다
        const bool no = !about_peer(P("{\"t\":\"ringing\"}")) && !about_peer(P("{\"t\":\"connected\"}")) && !about_peer(P("{\"t\":\"ended\",\"why\":\"busy\"}"))
                     && !about_peer(P("{\"t\":\"pong\",\"n\":1}")) && !about_peer(P("junk")) && !about_peer(Signal{});
        CHECK("about_peer: peer and ring only", yes && no);
    }

    // ── 쓰기: 걸기 글자 {"t":"call","to":"<id>"} ──
    {
        char b[SEND_BUF]; bool all = true;
        for (const char *id : { "tab5-1", "tab5-2", "stackchan-2", "a", "abcdefghijklmno", "A.b_c-9" }) {
            memset(b, '#', sizeof b);
            const int len = call_text(b, sizeof b, id);
            all = all && len == (int)strlen(b) && std::string(b) == std::string("{\"t\":\"call\",\"to\":\"") + id + "\"}";
        }
        CHECK("call text names the target id", all && std::string(b) == "{\"t\":\"call\",\"to\":\"A.b_c-9\"}");
        bool refused = true;
        for (const char *id : { "", "abcdefghijklmnop", "tab5\"1", "tab5\\1", "tab5\n1", "tab5\t1", "\x01", "tab5\x1F", "tab5\x7F", "tab5 1", " ", "\xEA\xB3\xB0", "tab5-1\r\n" }) {
            memset(b, '#', sizeof b);
            const bool r = call_text(b, sizeof b, id) == 0 && b[0] == 0;       // 글자를 만들지 않는다(반쯤 쓴 글자도 남기지 않는다)
            if (!r) printf("       not refused: '%s' -> '%s'\n", id, b);
            refused = refused && r;
        }
        memset(b, '#', sizeof b);
        CHECK("call text refuses a bad id (empty, 16+ chars, quote, backslash, control, DEL, space, non-ASCII) and null", refused && call_text(b, sizeof b, nullptr) == 0 && b[0] == 0);
        char small[30];
        const int exact = call_text(small, 28, "tab5-2");                      // {"t":"call","to":"tab5-2"} = 26자 + 끝의 0 = 27칸
        CHECK("call text: fits a buffer of its size + 1, not smaller", exact == 26 && call_text(small, 27, "tab5-2") == 26 && call_text(small, 26, "tab5-2") == 0 && small[0] == 0
              && call_text(small, 1, "tab5-2") == 0 && call_text(small, 0, "tab5-2") == 0 && call_text(nullptr, 40, "tab5-2") == 0);
        CHECK("the v1 call text is unchanged (old relay, old pair)", std::string(text_for(callfsm::A_SEND_CALL)) == "{\"t\":\"call\"}");
        CHECK("id_ok: the rule both directions share", id_ok("tab5-1") && id_ok("stackchan-2") && id_ok("abcdefghijklmno") && !id_ok("abcdefghijklmnop") && !id_ok("") && !id_ok(nullptr) && !id_ok("a b") && !id_ok("a\"")
              && !id_ok("a\\") && !id_ok("\x7F") && ID_CAP == 16 && sizeof(Signal{}.peer_id) == ID_CAP);
    }

    // ── 쓰기: "보내기" 할 일 하나 → 보낼 글자(v2). 걸기에 ID 가 없으면 보내지 않는다(옛 짝에게 거는 글자로 떨어지지 않는다 — RP-D4) ──
    {
        char b[SEND_BUF];
        const char *t = send_text_for(callfsm::A_SEND_CALL, b, sizeof b, "stackchan-2", 0);
        CHECK("send text: call names the id", t == b && std::string(t) == "{\"t\":\"call\",\"to\":\"stackchan-2\"}");
        CHECK("send text: call without a usable id is not sent at all", send_text_for(callfsm::A_SEND_CALL, b, sizeof b, "", 0) == nullptr && send_text_for(callfsm::A_SEND_CALL, b, sizeof b, nullptr, 0) == nullptr
              && send_text_for(callfsm::A_SEND_CALL, b, sizeof b, "bad\"id", 0) == nullptr && send_text_for(callfsm::A_SEND_CALL, b, 10, "tab5-1", 0) == nullptr);
        t = send_text_for(callfsm::A_SEND_REJECT, b, sizeof b, "tab5-1", 2);
        const bool rej_msg = std::string(t) == "{\"t\":\"reject\",\"msg\":2}";
        t = send_text_for(callfsm::A_SEND_REJECT, b, sizeof b, nullptr, 0);
        const bool rej_plain = std::string(t) == "{\"t\":\"reject\"}";
        t = send_text_for(callfsm::A_SEND_REJECT, b, 4, nullptr, 2);          // 버퍼가 작으면 번호 없는 거절(고정 글자)로 — 거절은 반드시 나간다
        CHECK("send text: reject carries the message number, or none", rej_msg && rej_plain && std::string(t) == "{\"t\":\"reject\"}");
        CHECK("send text: accept and hangup are the fixed texts and need no id", std::string(send_text_for(callfsm::A_SEND_ACCEPT, b, sizeof b, nullptr, 0)) == "{\"t\":\"accept\"}"
              && std::string(send_text_for(callfsm::A_SEND_HANGUP, b, sizeof b, "", 9)) == "{\"t\":\"hangup\"}" && std::string(send_text_for(callfsm::A_SEND_HANGUP, nullptr, 0, nullptr, 0)) == "{\"t\":\"hangup\"}");
        CHECK("send text: nothing for an action that is not a send", send_text_for(callfsm::A_TONE_BELL, b, sizeof b, "tab5-1", 0) == nullptr && send_text_for(0, b, sizeof b, "tab5-1", 0) == nullptr
              && send_text_for(callfsm::A_SEND_CALL | callfsm::A_SEND_HANGUP, b, sizeof b, "tab5-1", 0) == nullptr);
        CHECK("the send buffer holds the longest text", SEND_BUF >= 36 && call_text(b, sizeof b, "abcdefghijklmno") == 35 && reject_text(b, sizeof b, 3) == 22);
    }

    // ── 접속 헤더(§6.1): X-Device-Id · X-Device-Token · X-Phone-Proto: 2, 줄마다 \r\n ──
    {
        char h[200]; memset(h, '#', sizeof h);
        const int n = connect_headers(h, sizeof h, "tab5-2", "tok-two-0123456789");
        CHECK("connect headers: id, token and protocol 2, each line ends with CRLF",
              PROTO_VERSION == 2 && n == (int)strlen(h) && std::string(h) == "X-Device-Id: tab5-2\r\nX-Device-Token: tok-two-0123456789\r\nX-Phone-Proto: 2\r\n");
        char tight[64];
        const std::string want = "X-Device-Id: a\r\nX-Device-Token: b\r\nX-Phone-Proto: 2\r\n";
        CHECK("connect headers: fits a buffer of its size + 1, not smaller (then the buffer is left empty)", connect_headers(tight, want.size() + 1, "a", "b") == (int)want.size() && want == tight
              && connect_headers(tight, want.size(), "a", "b") == 0 && tight[0] == 0 && connect_headers(tight, 0, "a", "b") == 0);
        CHECK("connect headers: a null id, token or buffer gives nothing", connect_headers(h, sizeof h, nullptr, "x") == 0 && h[0] == 0 && connect_headers(h, sizeof h, "a", nullptr) == 0
              && connect_headers(nullptr, 200, "a", "b") == 0);
        const std::string tok43(43, 'k');                                     // 운영 토큰 길이(token_urlsafe(32))
        CHECK("connect headers: a real-size token and the longest id fit the firmware's 200-byte buffer", connect_headers(h, sizeof h, "abcdefghijklmno", tok43.c_str()) == 13 + 15 + 2 + 16 + 43 + 2 + 18);
    }

    printf("\n%d ok, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
