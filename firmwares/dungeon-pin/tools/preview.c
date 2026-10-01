#include "../main/dungeon.h"
#include "../main/paint.h"

#include <stdio.h>

/* 7 room set pieces × 5 phases, 4 regular enemies × 5, 3 bosses × 5. */
enum { SCALE = 4, COLUMNS = 5, CARDS = 70, ROWS = CARDS / COLUMNS };

static void advance(dungeon_t *d, unsigned ms)
{
    while (ms) { unsigned step = ms > 17 ? 17 : ms; dungeon_step(d, step); ms -= step; }
}

int main(void)
{
    uint16_t frames[CARDS][DUNGEON_W * DUNGEON_H];
    for (unsigned card = 0; card < CARDS; ++card) {
        dungeon_t d;
        dungeon_init(&d, card + 1);
        advance(&d, 2330);
        d.state = card < 55 ? DUNGEON_ROOM : DUNGEON_BOSS;
        d.room = card < 35 ? (dungeon_room_t)(card / 5u) : DUNGEON_AMBUSH;
        d.enemy = (uint8_t)((card - 35) / 5u % 4u);
        d.layout = (uint8_t)(card % 3u);
        d.boss = (dungeon_boss_t)((card - 55) / 5u % DUNGEON_BOSS_COUNT);
        d.phase = card % 5u;
        dungeon_paint(&d, frames[card]);
    }
    printf("P6\n%d %d\n255\n", COLUMNS * DUNGEON_W * SCALE, ROWS * DUNGEON_H * SCALE);
    for (int y = 0; y < ROWS * DUNGEON_H * SCALE; ++y) for (int x = 0; x < COLUMNS * DUNGEON_W * SCALE; ++x) {
        unsigned panel = (unsigned)(x / (DUNGEON_W * SCALE)) + COLUMNS * (unsigned)(y / (DUNGEON_H * SCALE));
        uint16_t p = frames[panel][((y / SCALE) % DUNGEON_H) * DUNGEON_W + ((x / SCALE) % DUNGEON_W)];
        unsigned char rgb[3] = {(unsigned char)((p >> 8) & 0xf8u), (unsigned char)((p >> 3) & 0xfcu), (unsigned char)((p << 3) & 0xf8u)};
        fwrite(rgb, sizeof(rgb), 1, stdout);
    }
    return 0;
}
