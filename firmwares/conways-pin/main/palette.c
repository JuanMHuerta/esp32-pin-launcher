#include "palette.h"

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rgb_t;

static uint16_t rgb565(rgb_t color)
{
    return ((uint16_t)(color.red >> 3) << 11) |
           ((uint16_t)(color.green >> 2) << 5) |
           (color.blue >> 3);
}

static rgb_t scale(rgb_t color, uint8_t intensity)
{
    return (rgb_t) {
        .red = color.red * intensity / 255,
        .green = color.green * intensity / 255,
        .blue = color.blue * intensity / 255,
    };
}

static void build_tile(uint16_t tile[LIFE_CELL_PIXELS * LIFE_CELL_PIXELS],
                       rgb_t shadow, rgb_t rim, rgb_t face, rgb_t glint)
{
    // A tiny octagonal LED: deep corners, a circuit-like rim, and a lit core.
    static const uint8_t shade[6][6] = {
        {0, 0, 1, 1, 0, 0},
        {0, 1, 2, 2, 1, 0},
        {1, 2, 3, 3, 2, 1},
        {1, 2, 3, 3, 2, 1},
        {0, 1, 2, 2, 1, 0},
        {0, 0, 1, 1, 0, 0},
    };
    const rgb_t colors[] = {shadow, rim, face, glint};
    for (int y = 0; y < LIFE_CELL_PIXELS; ++y) {
        for (int x = 0; x < LIFE_CELL_PIXELS; ++x) {
            tile[y * LIFE_CELL_PIXELS + x] = rgb565(colors[shade[y][x]]);
        }
    }
}

void palette_build(uint16_t colors[LIFE_MAX_AGE + 1]
                   [LIFE_CELL_PIXELS * LIFE_CELL_PIXELS])
{
    static const rgb_t cyan_shadow = {0, 48, 86};
    static const rgb_t cyan_rim = {0, 154, 224};
    static const rgb_t cyan_face = {20, 217, 255};
    static const rgb_t cyan_glint = {145, 255, 255};
    static const rgb_t birth_shadow = {0, 86, 132};
    static const rgb_t birth_rim = {82, 216, 255};
    static const rgb_t white = {255, 255, 255};
    static const rgb_t magenta_shadow = {58, 0, 8};
    static const rgb_t magenta_rim = {150, 0, 22};
    static const rgb_t magenta_face = {255, 18, 164};
    static const rgb_t magenta_glint = {255, 126, 209};
    static const uint8_t death_intensity[] = {0, 32, 85, 170, 255};

    for (int pixel = 0; pixel < LIFE_CELL_PIXELS * LIFE_CELL_PIXELS; ++pixel) {
        colors[0][pixel] = 0;
    }
    build_tile(colors[LIFE_ALIVE], birth_shadow, birth_rim, white, white);
    for (int state = 1; state < LIFE_ALIVE; ++state) {
        const uint8_t intensity = death_intensity[state];
        build_tile(colors[state], scale(magenta_shadow, intensity),
                   scale(magenta_rim, intensity), scale(magenta_face, intensity),
                   scale(magenta_glint, intensity));
    }
    for (int state = LIFE_ALIVE + 1; state <= LIFE_MAX_AGE; ++state) {
        build_tile(colors[state], cyan_shadow, cyan_rim, cyan_face, cyan_glint);
    }
}
