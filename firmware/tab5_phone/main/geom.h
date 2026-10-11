// geom.h — 화면 크기와 사각형. 하드웨어 무관(맥 시험에서도 쓴다).
#pragma once

#define SCREEN_W 1280
#define SCREEN_H 720

struct Rect {
    int x, y, w, h;
    bool hit(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};
