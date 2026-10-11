// sc_sim.cpp — 맥에서 스택짱 화면을 그려 보는 도구(화면 미리보기). 기기와 같은 그리기 코드(main/sc_ui.cpp + raster.h)를 그대로 컴파일한다
//   → 여기서 본 화면과 기기 화면은 픽셀까지 같다. 쓰는 법은 tools/render_sc_screens.py.
//   sc_sim <출력 폴더> : 장면마다 <이름>.rgb565 (320×240, 리틀엔디언 RGB565) 를 쓴다. 장면 목록은 아래 SCENES.
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "sc_ui.h"

using sclayout::Link;
using sclayout::Model;
using callfsm::Notice;
using callfsm::State;

static Model base(State st)
{
    Model m;
    m.fsm = st; m.link = Link::UP; m.unit = 3;
    m.peer_unit[0] = 1; m.peer_unit[1] = 2;
    m.peer_online[0] = true; m.peer_online[1] = true;
    m.deck_ok = true; m.vol_step = 4;
    return m;
}

struct Scene { const char *name; std::function<Model()> make; int patch; };   // patch: 0 = 통째, 1 = 통째로 그린 뒤 시계 조각을 다시, 2 = 음량 조각을 다시

static const std::vector<Scene> SCENES = {
    { "01_connecting_wifi",   [] { Model m = base(State::OFFLINE); m.link = Link::WIFI_SEARCHING; return m; }, 0 },
    { "02_connecting_relay",  [] { Model m = base(State::OFFLINE); m.link = Link::RELAY_CONNECTING; return m; }, 0 },
    { "03_connecting_nowifi", [] { Model m = base(State::OFFLINE); m.link = Link::WIFI_NONE; m.deck_armed = true; return m; }, 0 },
    { "04_unregistered",      [] { Model m = base(State::OFFLINE); m.unit = 0; m.peer_unit[0] = m.peer_unit[1] = 0; m.deck_ok = false; return m; }, 0 },
    { "10_idle_both_on",      [] { return base(State::IDLE); }, 0 },
    { "11_idle_one_off",      [] { Model m = base(State::IDLE); m.peer_online[1] = false; m.missed = 2; return m; }, 0 },
    { "12_idle_both_off",     [] { Model m = base(State::IDLE); m.peer_online[0] = m.peer_online[1] = false; m.deck_armed = true; return m; }, 0 },
    { "13_idle_notice_busy",  [] { Model m = base(State::IDLE); m.notice = Notice::BUSY; m.missed = 12; return m; }, 0 },
    { "14_idle_notice_long",  [] { Model m = base(State::IDLE); m.notice = Notice::RELAY_SILENT; m.deck_ok = false; return m; }, 0 },
    { "15_idle_notice_msg",   [] { Model m = base(State::IDLE); m.notice = Notice::REJECT_MSG; m.notice_msg = 2; return m; }, 0 },
    { "20_dialing_bear",      [] { Model m = base(State::DIALING); m.call_peer = 0; return m; }, 0 },
    { "21_ringback_rabbit",   [] { Model m = base(State::RINGBACK); m.call_peer = 1; return m; }, 0 },
    { "30_incoming_bear",     [] { Model m = base(State::INCOMING); m.call_peer = 0; return m; }, 0 },
    { "31_incoming_rabbit",   [] { Model m = base(State::INCOMING); m.call_peer = 1; return m; }, 0 },
    { "40_answering",         [] { Model m = base(State::ANSWERING); m.call_peer = 1; return m; }, 0 },
    { "50_in_call_bear",      [] { Model m = base(State::IN_CALL); m.call_peer = 0; m.talk_seconds = 83; return m; }, 0 },
    { "51_in_call_rabbit",    [] { Model m = base(State::IN_CALL); m.call_peer = 1; m.talk_seconds = 3599; m.vol_step = 5; return m; }, 0 },
    { "52_in_call_patch_clock", [] { Model m = base(State::IN_CALL); m.call_peer = 0; m.talk_seconds = 84; return m; }, 1 },
    { "53_in_call_patch_vol", [] { Model m = base(State::IN_CALL); m.call_peer = 0; m.talk_seconds = 83; m.vol_step = 1; return m; }, 2 },
    { "60_ending",            [] { Model m = base(State::ENDING); m.call_peer = 0; return m; }, 0 },
    { "70_to_deck",           [] { Model m = base(State::IDLE); m.to_deck = true; return m; }, 0 },
    // 다른 기기 구성(같은 펌웨어를 1번·2번 자리에서 본 모습 — 표가 바뀌어도 그려지는지)
    { "80_idle_as_unit1",     [] { Model m = base(State::IDLE); m.unit = 1; m.peer_unit[0] = 2; m.peer_unit[1] = 3; return m; }, 0 },
    { "81_incoming_robot",    [] { Model m = base(State::INCOMING); m.unit = 1; m.peer_unit[0] = 2; m.peer_unit[1] = 3; m.call_peer = 1; return m; }, 0 },
};

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: sc_sim <out dir>\n"); return 2; }
    static uint16_t buf[sclayout::W * sclayout::H];
    const raster::Canvas cv = { buf, sclayout::W, sclayout::H };
    for (const Scene &sc : SCENES) {
        Model m = sc.make();
        if (sc.patch == 1) { Model before = m; before.talk_seconds -= 1; scui::draw(cv, before); for (int i = 0; i < 5; ++i) scui::draw_clock(cv, m); }
        else if (sc.patch == 2) { Model before = m; before.vol_step = 4; scui::draw(cv, before); for (int i = 0; i < 5; ++i) scui::draw_volume(cv, m); }
        else scui::draw(cv, m);
        if (sc.patch) {                                      // 조각만 다섯 번 다시 그린 것이 통째로 그린 것과 한 픽셀도 다르지 않아야 한다
            static uint16_t whole[sclayout::W * sclayout::H];
            const raster::Canvas cw = { whole, sclayout::W, sclayout::H };
            scui::draw(cw, m);
            if (memcmp(whole, buf, sizeof buf) != 0) { fprintf(stderr, "PATCH MISMATCH %s\n", sc.name); return 1; }
        }
        const std::string path = std::string(argv[1]) + "/" + sc.name + ".rgb565";
        FILE *f = fopen(path.c_str(), "wb");
        if (!f || fwrite(buf, sizeof buf, 1, f) != 1) { fprintf(stderr, "write failed: %s\n", path.c_str()); return 1; }
        fclose(f);
        printf("%s\n", sc.name);
    }
    return 0;
}
