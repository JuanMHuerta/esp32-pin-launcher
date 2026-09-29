#include "render.h"
#include "hot.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

struct fluid_renderer {
    float render_current[PIXEL_COUNT];
    float render_persistent[PIXEL_COUNT];
    uint16_t color[RENDER_LAYERS][PIXEL_COUNT];
    uint16_t color_lut[RENDER_SPEED_LEVELS][RENDER_LAYERS];
    float mass_ratio;
};

static int clampi(int x, int lo, int hi)
{ return x < lo ? lo : (x > hi ? hi : x); }

static uint16_t wire_rgb565(int red, int green, int blue)
{
    red = clampi(red, 0, 255);
    green = clampi(green, 0, 255);
    blue = clampi(blue, 0, 255);
    uint16_t rgb565 = (uint16_t)(((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3));
    return __builtin_bswap16(rgb565);
}

fluid_renderer *render_create(void)
{
#ifdef ESP_PLATFORM
    fluid_renderer *r = heap_caps_calloc(1, sizeof(*r), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
    fluid_renderer *r = calloc(1, sizeof(*r));
#endif
    if (!r) return NULL;

    static const float layer_brightness[RENDER_LAYERS] = {0.38f, 0.78f, 1.0f};
    /* Markers sample a continuous liquid.  Their local packing density
     * determines coverage, not the opacity of water inside the pool.
     * Keep the established full-water color and its velocity highlights. */
    for (int speed_level = 0; speed_level < RENDER_SPEED_LEVELS; ++speed_level)
        for (int layer = 0; layer < RENDER_LAYERS; ++layer) {
            float speed = (float)speed_level / (RENDER_SPEED_LEVELS - 1);
            float brightness = layer_brightness[layer] * GLOW;
            int red = (int)lroundf((8.0f + 4.0f * speed) * brightness);
            int green = (int)lroundf((98.0f + 48.0f * speed) * brightness);
            int blue = (int)lroundf((168.0f + 65.0f * speed) * brightness);
            r->color_lut[speed_level][layer] = wire_rgb565(red, green, blue);
        }
    return r;
}

void render_destroy(fluid_renderer *r)
{
    free(r);
}

void FLOW_HOT render_reconstruct(fluid_renderer *r, const fluid_snapshot *snapshot)
{
    /* Reconstruct continuous liquid from sparse particle splats before
     * thresholding into logical pixels.  A separable [1, 2, 1]/4 kernel
     * suppresses sub-pixel sampling holes without changing particle physics.
     * When force points into a wall, use a reflected inboard sample there so
     * a low-density contact cell remains visibly connected to the container.
     * On departure, retain the isolated edge rule: smoothing must not paint a
     * wet wall after particles have left it.  Each selected stencil preserves
     * total density. */
    bool touch_left = snapshot->acceleration_x < -RENDER_WALL_CONTACT_ACCEL;
    bool touch_right = snapshot->acceleration_x > RENDER_WALL_CONTACT_ACCEL;
    bool touch_top = snapshot->acceleration_y < -RENDER_WALL_CONTACT_ACCEL;
    bool touch_bottom = snapshot->acceleration_y > RENDER_WALL_CONTACT_ACCEL;
    for (int y = 0; y < PIXEL_GRID_Y; ++y) {
        int base = y * PIXEL_GRID_X;
        for (int x = 0; x < PIXEL_GRID_X; ++x) {
            int c = base + x;
            bool edge = x == 0 || x == PIXEL_GRID_X - 1;
            bool contact = (x == 0 && touch_left) ||
                           (x == PIXEL_GRID_X - 1 && touch_right);
            int left = x > 1 || (x == 1 && touch_left) ? c - 1 : c;
            int right = x + 2 < PIXEL_GRID_X ||
                        (x == PIXEL_GRID_X - 2 && touch_right) ? c + 1 : c;
            r->render_current[c] = edge && !contact ? snapshot->density[c] :
                0.5f * snapshot->density[c] +
                0.25f * (snapshot->density[left] + snapshot->density[right]);
        }
    }
    float mass = 0.0f;
    for (int i = 0; i < PIXEL_COUNT; ++i) {
        int y = i / PIXEL_GRID_X;
        bool edge = y == 0 || y == PIXEL_GRID_Y - 1;
        bool contact = (y == 0 && touch_top) ||
                       (y == PIXEL_GRID_Y - 1 && touch_bottom);
        int above = y > 1 || (y == 1 && touch_top) ? i - PIXEL_GRID_X : i;
        int below = y + 2 < PIXEL_GRID_Y ||
                    (y == PIXEL_GRID_Y - 2 && touch_bottom) ? i + PIXEL_GRID_X : i;
        float current = edge && !contact ? r->render_current[i] :
            0.5f * r->render_current[i] +
            0.25f * (r->render_current[above] + r->render_current[below]);
        r->render_persistent[i] = current > r->render_persistent[i] * RENDER_PERSISTENCE
            ? current : r->render_persistent[i] * RENDER_PERSISTENCE;
        mass += current;

        if (r->render_persistent[i] <= RENDER_DENSITY_THRESHOLD) {
            for (int layer = 0; layer < RENDER_LAYERS; ++layer) r->color[layer][i] = 0;
            continue;
        }
        int speed_level = clampi((int)lroundf(snapshot->speed[i] *
                                             ((float)(RENDER_SPEED_LEVELS - 1) / 255.0f)),
                                 0, RENDER_SPEED_LEVELS - 1);
        for (int layer = 0; layer < RENDER_LAYERS; ++layer)
            r->color[layer][i] = r->color_lut[speed_level][layer];
    }
    r->mass_ratio = mass / PARTICLE_COUNT;
}

void FLOW_HOT render_band(const fluid_renderer *r, int y, int rows, uint16_t *out)
{
    for (int py = y; py < y + rows; ++py) {
        int logical_y = py >> 3;
        int native_y = py & (PIXEL_NATIVE_SIZE - 1);
        if (native_y == 0 || native_y == 7) {
            memset(out, 0, PANEL_W * sizeof(*out));
            out += PANEL_W;
            continue;
        }
        int base = logical_y * PIXEL_GRID_X;
        for (int logical_x = 0; logical_x < PIXEL_GRID_X; ++logical_x) {
            int cell = base + logical_x;
            uint16_t outer = r->color[0][cell];
            uint16_t body = r->color[1][cell];
            uint16_t core = r->color[2][cell];
            *out++ = 0;
            if (native_y == 1 || native_y == 6) {
                *out++ = outer; *out++ = outer; *out++ = outer;
                *out++ = outer; *out++ = outer; *out++ = outer;
            } else if (native_y == 2 || native_y == 5) {
                *out++ = outer; *out++ = body; *out++ = body;
                *out++ = body; *out++ = body; *out++ = outer;
            } else {
                *out++ = outer; *out++ = body; *out++ = core;
                *out++ = core; *out++ = body; *out++ = outer;
            }
            *out++ = 0;
        }
    }
}

float render_mass_ratio(const fluid_renderer *r)
{
    return r->mass_ratio;
}
