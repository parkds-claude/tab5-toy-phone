// voice_cmd.cpp — 설명은 voice_cmd.h. phone_main.cpp 에서 그대로 옮긴 코드다(동작·줄 모양 불변).
#include "voice_cmd.h"

#include <cstdio>
#include <cstring>

#include "audio_out.h"
#include "board_profile.h"
#include "net_wifi.h"
#include "relay_link.h"
#include "voice.h"

namespace voicecmd {

void print_tune();

namespace {

// PL3c-D3 손잡이와 그 곁의 측정값을 한 줄로. STAT 줄은 건드리지 않는다(기존 줄 모양 그대로 — 도구·앞 계획의 정규식이 그대로 맞는다).
//   dflt = 손잡이가 전부 부팅 값인가 / legacy = 전부 5935ddd 의 값인가 / meas = selftest·vping 의 판정이 유효한가(txgain 4 · vol 100 — voice_tune.h meas_valid)
//   ps = Wi-Fi 절전(0 이어야 한다, -1 = 아직 못 읽음 — net_wifi N-D5) / nd_socks = TCP_NODELAY 를 마지막으로 건 소켓 수(-1 = 건 적 없음)
//   g_q8 = 평탄화가 지금 걸고 있는 이득(256 = 1배, 0 = 평탄화·누르기 꺼짐) / supp·far = 이번 통화에서 남은 메아리를 누른 묶음·상대 소리가 나던 묶음(16ms 단위)
//   rxgap = 이번 통화에서 받은 프레임 사이의 가장 긴 틈(ms) / mclip = 이번 통화에서 마이크 칸이 꼭대기에 닿은 표본 수(스피커 소리가 마이크에서 넘친다)
voicetune::Knobs knobs_now(const voice::Tune &t)
{
    voicetune::Knobs k;
    k.txgain = voice::tx_gain(); k.vol = audioout::volume(); k.mic_db = audioout::mic_gain(); k.kbps = t.kbps;
    k.lvl = t.lvl; k.tilt = t.tilt; k.res_db = t.res_db; k.jb = t.jb; k.nodelay = relaylink::nodelay() ? 1 : 0;
    return k;
}
bool meas_valid_now() { voice::Tune t; voice::tune(&t); return voicetune::meas_valid(knobs_now(t)); }
// 손잡이 명령 하나: "<name>" = 지금 값, "<name> <숫자>" = 바꾸기. set 이 false 면 "ERR <name> <범위>". 돌려주는 값 = 이 명령이었나
bool knob_cmd(const std::string &cmd, const char *name, int lo, int hi, const char *range, int (*get)(), bool (*set)(int))
{
    const size_t n = strlen(name);
    if (cmd.compare(0, n, name) != 0 || (cmd.size() != n && cmd[n] != ' ')) return false;
    int v = 0;
    if (cmd.size() == n) printf("OK %s %d\n", name, get());
    else if (voicecore::parse_uint(cmd.c_str() + n + 1, lo, hi, &v) && set(v)) printf("OK %s %d\n", name, get());
    else printf("ERR %s %s\n", name, range);
    return true;
}

}  // namespace

void print_tune()
{
    voice::Tune t; voice::tune(&t);
    const voicetune::Knobs k = knobs_now(t);
    printf("TUNE dflt=%d legacy=%d meas=%d txgain=%d vol=%d mic=%d kbps=%d lvl=%d tilt=%d res=%d jb=%d nodelay=%d vtone=%d ps=%d nd_socks=%d g_q8=%d supp=%u far=%u rxgap=%u mclip=%u\n",
           (int)voicetune::is_default(k), (int)voicetune::is_legacy(k), (int)voicetune::meas_valid(k), k.txgain, k.vol, k.mic_db, k.kbps, k.lvl, k.tilt, k.res_db, k.jb, k.nodelay, t.vtone,
           netwifi::ps(), relaylink::nodelay_socks(), t.gain_q8, (unsigned)t.supp, (unsigned)t.far, (unsigned)t.rxgap_ms, (unsigned)t.mic_clip);
}

bool handle(const std::string &cmd)
{
    if (cmd.empty()) return false;
    else if (cmd == "selftest") {                               // PL3b-D14: 통화 밖에서만. 시험음 0.64초(조용한 모드면 소리 없이)
        voice::SelfTest r;
        if (voice::selftest(&r)) printf("SELFTEST amp=%d mic=%u ref=%u aec=%u ok=%d\n", (int)audioout::amp_is_on(), (unsigned)r.mic, (unsigned)r.ref, (unsigned)r.aec,
                                        (int)(r.ref >= boardprofile::SELFTEST_REF_MIN));
        else printf("SELFTEST FAIL busy-or-no-audio\n");
        if (!meas_valid_now()) print_tune();                    // PL3c-D3: 이득·음량이 4배·100 이 아니면 위의 ok= 는 믿을 수 없다 — 그 사정(meas=0)을 바로 뒤에 한 줄로 남긴다
    }
    else if (cmd.rfind("vsrc ", 0) == 0) {                      // PL3b-D11: 통화 중에만, 통화가 끝나면 aec 로 돌아간다
        voicecore::Src s;
        if (voicecore::parse_src(cmd.c_str() + 5, &s) && !voicecore::src_allowed(s, audioout::amp_is_on())) printf("ERR vsrc speaker-on\n");   // PL3b-D22: mic·ref 는 스피커가 꺼져 있을 때만
        else if (voicecore::parse_src(cmd.c_str() + 5, &s) && voice::set_source(s)) printf("OK vsrc %s\n", voicecore::src_name(s));
        else printf("ERR vsrc\n");
    }
    else if (cmd == "txgain" || cmd.rfind("txgain ", 0) == 0) {  // PL3b-D23: 보내는 쪽 디지털 이득 1~4배. 인자 없으면 지금 값
        int n = 0;
        if (cmd == "txgain") printf("OK txgain %d\n", voice::tx_gain());
        else if (voicecore::parse_uint(cmd.c_str() + 7, voicecore::TX_GAIN_MIN, voicecore::TX_GAIN_MAX, &n) && voice::set_tx_gain(n)) printf("OK txgain %d\n", voice::tx_gain());
        else printf("ERR txgain 1-%d\n", voicecore::TX_GAIN_MAX);
    }
    else if (cmd == "vol" || cmd.rfind("vol ", 0) == 0) {        // PL3b-D24: 재생 음량 0~100. 인자 없으면 지금 값. 소리 장치가 안 열렸으면 ERR
        int n = 0;
        if (cmd == "vol") printf("OK vol %d\n", audioout::volume());
        else if (!voicecore::parse_uint(cmd.c_str() + 4, 0, 100, &n)) printf("ERR vol 0-100\n");
        else {
            const audioout::VolResult r = audioout::set_volume(n);
            if (r == audioout::VolResult::OK) printf("OK vol %d\n", audioout::volume());
            else printf(r == audioout::VolResult::NO_AUDIO ? "ERR vol no-audio\n" : "ERR vol i2c\n");
        }
    }
    else if (cmd == "vloop on" || cmd == "vloop off") {       // VL-D1: 통화 밖에서만. 상대 없이 통화의 소리 길을 돌려 본다(끝나면 반드시 off)
        const bool want = cmd == "vloop on";
        printf("%s vloop %s\n", voice::loop(want) ? "OK" : "ERR", want ? "on" : "off");
    }
    else if (cmd == "vping") {                                  // PL3b-D12: 결과는 "VPING rt_ms=…" 줄로(1초쯤 뒤)
        printf(voice::ping() ? "OK vping\n" : "ERR vping\n");
        if (!meas_valid_now()) print_tune();                    // PL3c-D3: 이 기기의 이득·음량이 4배·100 이 아니다 — timeout 이 나와도 고장이 아닐 수 있다(상대 기기도 같은 조건이어야 한다)
    }
    // ── Phase 3c 통화 품질 손잡이(PL3c-D3): 범위는 voice_tune.h, 저장하지 않는다 ──
    else if (cmd == "tune") print_tune();
    else if (knob_cmd(cmd, "lvl", 0, voicetune::LVL_MAX, "0|6-16", [] { voice::Tune t; voice::tune(&t); return t.lvl; }, [](int v) { return voice::set_lvl(v); })) {}
    else if (knob_cmd(cmd, "tilt", 0, voicetune::TILT_MAX, "0-2", [] { voice::Tune t; voice::tune(&t); return t.tilt; }, [](int v) { return voice::set_tilt(v); })) {}
    else if (knob_cmd(cmd, "res", 0, voicetune::RES_DB_MAX, "0|6|12|18", [] { voice::Tune t; voice::tune(&t); return t.res_db; }, [](int v) { return voice::set_res(v); })) {}
    else if (knob_cmd(cmd, "guard", 0, voicetune::GUARD_MAX, "0-16", [] { return voice::guard(); }, [](int v) { return voice::set_guard(v); })) {}   // P4H-D1 메아리 지킴(저장하지 않는다)
    else if (knob_cmd(cmd, "rest", 0, voicetune::REST_MAX, "0|6|12|18", [] { return voice::rest(); }, [](int v) { return voice::set_rest(v); })) {}    // P5A-D1 쉼 누르기(저장하지 않는다)
    else if (knob_cmd(cmd, "dt", 0, voicetune::DT_MAX, "0|6|12", [] { return voice::dt(); }, [](int v) { return voice::set_dt(v); })) {}                // P5A-D2 겹침 누르기
    else if (knob_cmd(cmd, "rxgain", voicetune::RXGAIN_MIN, voicetune::RXGAIN_MAX, "100-300", [] { return voice::rxgain(); }, [](int v) { return voice::set_rxgain(v); })) {}   // P5A-D3 받는 목소리 키우기
    else if (knob_cmd(cmd, "hpf", 0, 1, "0|1", [] { return voice::hpf(); }, [](int v) { return voice::set_hpf(v); })) {}                            // P5A-D4 저음 걷어내기
    else if (knob_cmd(cmd, "opus", voicetune::KBPS_MIN, voicetune::KBPS_MAX, "12-40", [] { voice::Tune t; voice::tune(&t); return t.kbps; }, [](int v) { return voice::set_kbps(v); })) {}
    else if (knob_cmd(cmd, "jb", 0, voicetune::JB_MAX, "0|3-10", [] { voice::Tune t; voice::tune(&t); return t.jb; }, [](int v) { return voice::set_jb(v); })) {}
    else if (knob_cmd(cmd, "vtone", voicetune::VTONE_MIN, voicetune::VTONE_MAX, "300-7000", [] { voice::Tune t; voice::tune(&t); return t.vtone; }, [](int v) { return voice::set_vtone(v); })) {}
    else if (knob_cmd(cmd, "nodelay", 0, 1, "0|1", [] { return relaylink::nodelay() ? 1 : 0; }, [](int v) { return relaylink::set_nodelay(v != 0) >= 0; })) {}
    else if (cmd == "mic" || cmd.rfind("mic ", 0) == 0) {      // PL3c-D9: 마이크 증폭 12~24dB(3dB 단위). 코덱 쓰기가 실패할 수 있어 응답이 셋이다(vol 과 같은 모양)
        int n = 0;
        if (cmd == "mic") printf("OK mic %d\n", audioout::mic_gain());
        else if (!voicecore::parse_uint(cmd.c_str() + 4, voicetune::MIC_DB_MIN, voicetune::MIC_DB_MAX, &n) || !voicetune::ok_mic_db(n)) printf("ERR mic 12-24 step 3\n");
        else {
            const audioout::VolResult r = audioout::set_mic_gain(n);
            if (r == audioout::VolResult::OK) printf("OK mic %d\n", audioout::mic_gain());
            else printf(r == audioout::VolResult::NO_AUDIO ? "ERR mic no-audio\n" : "ERR mic i2c\n");
        }
    }
    else return false;
    return true;
}

}  // namespace voicecmd
