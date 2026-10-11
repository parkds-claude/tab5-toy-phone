// sprite.h — 색 그림 자산의 모양(하드웨어 무관, 맥 시험에서도 쓴다). 자산은 tools/gen_toy_sprites.py 가 main/ui_sprites.h 로 굽는다.
//   줄 하나 = 묶음(run)의 나열, 묶음 하나 = 16비트 둘: [길이(8) << 8 | 알파(8)], [RGB565]. 알파 0 = 건너뜀, 255 = 덮어쓰기, 그 사이 = 섞기.
//   row[y] .. row[y+1] 이 y 줄의 묶음(단위: 묶음 개수 — runs 의 색인은 2배). 줄 끝의 투명 묶음은 들어 있지 않다.
//   ax·by 는 그림 안에서의 기준점(캐릭터의 세로 대칭축 x, 발 밑 바닥 y) — 놓는 쪽은 "대칭축 x, 바닥 y"로 자리를 준다(gfx::blit_sprite_at).
#pragma once
#include <stdint.h>

typedef struct {
    uint16_t w, h;
    int16_t ax, by;
    const uint32_t *row;
    const uint16_t *runs;
} ui_sprite_t;
