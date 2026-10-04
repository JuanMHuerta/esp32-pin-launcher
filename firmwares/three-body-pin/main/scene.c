// SPDX-License-Identifier: GPL-3.0-only
#include "scene.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

enum {
    ORBIT_LIFETIME_MS = 65000,
    ORBIT_TOUCH_HOLD_MS = 700,
};
static const double ORBIT_ESCAPE_RADIUS = 6.4;

const char *orbit_name(unsigned n)
{
    static const char *names[ORBIT_PRESETS] = {
        "FIGURE EIGHT",  "CHAOTIC SUNS",    "BINARY VISITOR", "BROKEN TRIANGLE",
        "LONG APPROACH", "CLOSE ENCOUNTER", "ECHO ORBIT",     "SOLAR CROSSING",
    };
    return names[n % ORBIT_PRESETS];
}

double orbit_energy(const orbit_t *s)
{
    double e = 0;
    for (int i = 0; i < 3; i++) {
        const orbit_body_t *b = &s->body[i];
        e += .5 * b->m * (b->vx * b->vx + b->vy * b->vy);
        for (int j = i + 1; j < 3; j++) {
            double x = b->x - s->body[j].x, y = b->y - s->body[j].y;
            e -= b->m * s->body[j].m / sqrt(x * x + y * y + s->softening * s->softening);
        }
    }
    return e;
}

static const orbit_body_t initial_conditions[ORBIT_PRESETS][3] = {
    {
        /* Simó's figure eight, Chenciner/Montgomery (2000), G=m=1.
         * https://people.ucsc.edu/~rmont/Nbdy/NbdyB.html */
        {.97000436, -.24308753, .466203685, .432365730, 1},
        {-.97000436, .24308753, .466203685, .432365730, 1},
        {0, 0, -.932407370, -.864731460, 1},
    },
    {
        {-1, .2, .12, .18, 1},
        {1, .3, -.08, -.14, 1.3},
        {.1, -1.2, -.07, .12, .8},
    },
    {
        {-.5, 0, 0, -.70, 1},
        {.5, 0, 0, .70, 1},
        {-2, 1.6, .55, -.14, .45},
    },
    {
        {-1.20, -.45, .38, .12, 1.4},
        {1.15, -.38, -.42, .19, 1.0},
        {.16, 1.25, .05, -.45, .65},
    },
    {
        /* A perturbed rotating equilateral arrangement. */
        {-.8, -.46, .394, -.685, 1},
        {.8, -.46, .394, .685, 1},
        {0, .92, -.69, .025, 1},
    },
    {
        {-.86, 0, 0, -.50, 1},
        {.86, 0, 0, .50, 1},
        {0, .2, .50, .02, .55},
    },
    {
        {-1.25, -.16, .14, .48, 1.2},
        {1.08, -.18, -.18, .58, .9},
        {.1, 1.45, .04, -.69, .75},
    },
    {
        {-1.55, -.4, .3, .14, .8},
        {1.3, .5, -.32, -.05, 1.4},
        {.05, -1.1, .08, .38, .95},
    },
};

void orbit_init(orbit_t *s, unsigned preset)
{
    memset(s, 0, sizeof(*s));
    s->preset = preset % ORBIT_PRESETS;
    s->generation = 1;
    memcpy(s->body, initial_conditions[s->preset], sizeof(s->body));
    s->scale = 88;
    if (s->preset == 1) {
        s->softening = .04;
    } else if (s->preset == 2) {
        s->softening = .025;
    } else if (s->preset >= 3) {
        s->softening = .055;
    }

    double m = 0, x = 0, y = 0, vx = 0, vy = 0;
    for (int i = 0; i < 3; i++) {
        orbit_body_t *b = &s->body[i];
        m += b->m;
        x += b->x * b->m;
        y += b->y * b->m;
        vx += b->vx * b->m;
        vy += b->vy * b->m;
    }
    for (int i = 0; i < 3; i++) {
        s->body[i].x -= x / m;
        s->body[i].y -= y / m;
        s->body[i].vx -= vx / m;
        s->body[i].vy -= vy / m;
    }
    s->energy0 = orbit_energy(s);
}

void orbit_next(orbit_t *s)
{
    unsigned g = s->generation + 1, n = s->preset + 1;
    bool panel_visible = s->panel_visible;
    orbit_init(s, n);
    s->generation = g;
    s->panel_visible = panel_visible;
}

static void derivative(const orbit_t *s, const double *v, double *d)
{
    for (int i = 0; i < 3; i++) {
        d[4 * i] = v[4 * i + 2];
        d[4 * i + 1] = v[4 * i + 3];
        d[4 * i + 2] = d[4 * i + 3] = 0;
    }
    for (int i = 0; i < 3; i++) {
        for (int j = i + 1; j < 3; j++) {
            double x = v[4 * j] - v[4 * i], y = v[4 * j + 1] - v[4 * i + 1];
            double r2 = x * x + y * y + s->softening * s->softening;
            double f = 1 / (r2 * sqrt(r2));
            d[4 * i + 2] += x * f * s->body[j].m;
            d[4 * i + 3] += y * f * s->body[j].m;
            d[4 * j + 2] -= x * f * s->body[i].m;
            d[4 * j + 3] -= y * f * s->body[i].m;
        }
    }
}

static void rk4(orbit_t *s, double dt)
{
    double v[12], t[12], k1[12], k2[12], k3[12], k4[12];
    for (int i = 0; i < 3; i++) {
        v[4 * i] = s->body[i].x;
        v[4 * i + 1] = s->body[i].y;
        v[4 * i + 2] = s->body[i].vx;
        v[4 * i + 3] = s->body[i].vy;
    }
    derivative(s, v, k1);
    for (int i = 0; i < 12; i++) {
        t[i] = v[i] + dt * .5 * k1[i];
    }
    derivative(s, t, k2);
    for (int i = 0; i < 12; i++) {
        t[i] = v[i] + dt * .5 * k2[i];
    }
    derivative(s, t, k3);
    for (int i = 0; i < 12; i++) {
        t[i] = v[i] + dt * k3[i];
    }
    derivative(s, t, k4);
    for (int i = 0; i < 12; i++) {
        v[i] += dt * (k1[i] + 2 * k2[i] + 2 * k3[i] + k4[i]) / 6;
    }
    for (int i = 0; i < 3; i++) {
        s->body[i].x = v[4 * i];
        s->body[i].y = v[4 * i + 1];
        s->body[i].vx = v[4 * i + 2];
        s->body[i].vy = v[4 * i + 3];
    }
    s->time += dt;
}

void orbit_advance(orbit_t *s, double dt)
{
    if (!isfinite(dt) || dt <= 0) {
        return;
    }
    while (dt > 1e-12) {
        /* Bound steps by both encounter free-fall and crossing times. Softening
         * regularizes close passages in chaotic presets; there are no wall bounces. */
        double h = .003;
        for (int i = 0; i < 3; i++) {
            for (int j = i + 1; j < 3; j++) {
                double x = s->body[i].x - s->body[j].x, y = s->body[i].y - s->body[j].y;
                double r = sqrt(x * x + y * y + s->softening * s->softening);
                double vx = s->body[i].vx - s->body[j].vx, vy = s->body[i].vy - s->body[j].vy;
                h = fmin(h, .018 * sqrt(r * r * r / (s->body[i].m + s->body[j].m)));
                h = fmin(h, .018 * r / (sqrt(vx * vx + vy * vy) + 1e-9));
            }
        }
        h = fmin(dt, h);
        rk4(s, h);
        dt -= h;
    }
}

static bool escaped(const orbit_t *s)
{
    for (int i = 0; i < 3; i++) {
        if (hypot(s->body[i].x, s->body[i].y) > ORBIT_ESCAPE_RADIUS) {
            return true;
        }
    }
    return false;
}

void orbit_step(orbit_t *s, uint32_t ms)
{
    /* A stalled frame must not trigger an unbounded physics catch-up. */
    if (ms > 100) {
        ms = 100;
    }
    s->elapsed_ms += ms;
    if (s->elapsed_ms >= ORBIT_LIFETIME_MS || escaped(s)) {
        orbit_next(s);
        return;
    }
    orbit_advance(s, ms * .00065);
    if (ms) {
        for (int i = 0; i < 3; i++) {
            s->trail[s->head][i][0] = (float)s->body[i].x;
            s->trail[s->head][i][1] = (float)s->body[i].y;
        }
        s->head = (s->head + 1) % ORBIT_TRAIL;
        if (s->count < ORBIT_TRAIL) {
            s->count++;
        }
    }
    if (escaped(s)) {
        orbit_next(s);
        return;
    }

    double extent = 1.1;
    for (int i = 0; i < 3; i++) {
        extent = fmax(extent, fmax(fabs(s->body[i].x), fabs(s->body[i].y) * 2.25));
    }
    double target = fmin(112, 110 / extent);
    s->scale += (target - s->scale) * (1 - exp(-(double)ms * .002));
}

void orbit_touch(orbit_t *s, bool down, uint32_t now_ms)
{
    if (!down) {
        s->touch_down = false;
        s->touch_fired = false;
        return;
    }
    if (!s->touch_down) {
        s->touch_down = true;
        s->touch_started_ms = now_ms;
        s->touch_fired = false;
    }
    if (!s->touch_fired && (uint32_t)(now_ms - s->touch_started_ms) >= ORBIT_TOUCH_HOLD_MS) {
        s->panel_visible = !s->panel_visible;
        s->touch_fired = true;
    }
}

static uint16_t color(int i)
{
    static const int c[3][3] = {{255, 184, 85}, {82, 222, 255}, {255, 92, 139}};
    return pin_rgb(c[i][0], c[i][1], c[i][2]);
}

static uint16_t add_light(uint16_t base, uint16_t light, unsigned amount)
{
    unsigned r = (base >> 11) & 31, g = (base >> 5) & 63, b = base & 31;
    r += (((light >> 11) & 31) * amount + 127) / 255;
    g += (((light >> 5) & 63) * amount + 127) / 255;
    b += ((light & 31) * amount + 127) / 255;
    if (r > 31) {
        r = 31;
    }
    if (g > 63) {
        g = 63;
    }
    if (b > 31) {
        b = 31;
    }
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static uint32_t mix32(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    return value ^ (value >> 16);
}

static unsigned nebula_noise(int x, int y)
{
    enum {
        CELL_W = 32,
        CELL_H = 24,
        GRID_W = (PIN_W + CELL_W - 1) / CELL_W + 1,
        GRID_H = (PIN_H + CELL_H - 1) / CELL_H + 1,
    };
    static uint8_t grid[GRID_H][GRID_W];
    static bool initialized;
    if (!initialized) {
        for (int row = 0; row < GRID_H; row++) {
            for (int column = 0; column < GRID_W; column++) {
                uint32_t key = (uint32_t)column * 0x9e3779b9U ^ (uint32_t)row * 0x85ebca6bU;
                grid[row][column] = (uint8_t)(mix32(key) >> 24);
            }
        }
        initialized = true;
    }

    int column = x / CELL_W, row = y / CELL_H;
    unsigned fx = (unsigned)(x % CELL_W), fy = (unsigned)(y % CELL_H);
    unsigned top_left = grid[row][column], top_right = grid[row][column + 1];
    unsigned bottom_left = grid[row + 1][column], bottom_right = grid[row + 1][column + 1];
    unsigned top = (top_left * (CELL_W - fx) + top_right * fx) / CELL_W;
    unsigned bottom = (bottom_left * (CELL_W - fx) + bottom_right * fx) / CELL_W;
    return (top * (CELL_H - fy) + bottom * fy) / CELL_H;
}

static void background(uint16_t *p)
{
    for (int y = 0; y < PIN_H; y++) {
        for (int x = 0; x < PIN_W; x++) {
            int center_dx = x - 126, center_dy = y - 61;
            int turbulence = ((int)nebula_noise(x, y) - 128) / 3;
            int cloud = 112 - center_dx * center_dx / 22 - center_dy * center_dy / 2 + turbulence;
            int cloud2_dx = x - 194, cloud2_dy = y - 76;
            int cloud2 =
                88 - cloud2_dx * cloud2_dx / 19 - cloud2_dy * cloud2_dy / 2 - turbulence / 2;
            uint16_t base = pin_rgb(2 + y / 48, 4 + (PIN_H - y) / 60, 11 + x / 90);
            if (cloud > 0) {
                base = add_light(base, pin_rgb(83, 45, 154), (unsigned)(cloud / 2));
            }
            if (cloud2 > 0) {
                base = add_light(base, pin_rgb(24, 93, 135), (unsigned)(cloud2 / 2));
            }
            p[y * PIN_W + x] = base;
        }
    }
    for (unsigned i = 0; i < 220; i++) {
        uint32_t position = mix32(i + 0x6d2b79f5U);
        uint32_t detail = mix32(position ^ 0xa511e9b3U);
        int x = 4 + (int)(position % (PIN_W - 8));
        int y = 4 + (int)(detail % (PIN_H - 8));
        unsigned tier = detail >> 29;
        uint16_t star;
        unsigned brightness;
        if (tier == 0) {
            star = pin_rgb(205, 224, 255);
            brightness = 145 + (detail & 63);
        } else if (tier < 3) {
            star = pin_rgb(139, 175, 226);
            brightness = 78 + (detail & 63);
        } else {
            star = pin_rgb(83, 111, 165);
            brightness = 38 + (detail & 47);
        }
        p[y * PIN_W + x] = add_light(p[y * PIN_W + x], star, brightness);
        if (tier == 0 && (detail & 1) == 0) {
            int dx = (detail & 2) ? 1 : 0;
            int dy = dx ? 0 : 1;
            p[(y + dy) * PIN_W + x + dx] = add_light(p[(y + dy) * PIN_W + x + dx], star, 42);
        }
    }
}

static void project(const orbit_t *s, double x, double y, int *px, int *py)
{
    *px = (int)lround(PIN_W / 2.0 + x * s->scale);
    *py = (int)lround(PIN_H / 2.0 - y * s->scale);
}

static void trail_segment(uint16_t *p, int x, int y, int xx, int yy, uint16_t c)
{
    if (x < 2 || x >= PIN_W - 2 || xx < 2 || xx >= PIN_W - 2 || y < 2 || y >= PIN_H - 2 || yy < 2 ||
        yy >= PIN_H - 2) {
        return;
    }
    pin_line(p, x, y, xx, yy, c);
}

static void stellar_glow(uint16_t *p, int x, int y, int radius, uint16_t c)
{
    int outer = radius + 15;
    for (int dy = -outer; dy <= outer; dy++) {
        for (int dx = -outer; dx <= outer; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 > outer * outer || x + dx < 0 || x + dx >= PIN_W || y + dy < 0 ||
                y + dy >= PIN_H) {
                continue;
            }
            unsigned intensity = (unsigned)((outer * outer - d2) * 56 / (outer * outer));
            int r2 = radius * radius;
            if (d2 <= r2) {
                intensity = 90 + (unsigned)((r2 - d2) * 150 / r2);
            }
            uint16_t *pixel = &p[(y + dy) * PIN_W + x + dx];
            *pixel = add_light(*pixel, c, intensity);
            if (d2 <= 2) {
                *pixel = add_light(*pixel, pin_rgb(255, 249, 229), 245);
            }
        }
    }
}

static void paint_panel(const orbit_t *s, uint16_t *p)
{
    if (!s->panel_visible) {
        return;
    }
    const int left = 192;
    for (int y = 0; y < PIN_H; y++) {
        for (int x = left; x < PIN_W; x++) {
            p[y * PIN_W + x] = pin_dim(p[y * PIN_W + x], 82);
        }
    }
    uint16_t muted = pin_rgb(144, 169, 185), white = pin_rgb(223, 235, 236);
    pin_line(p, left - 1, 0, left - 1, PIN_H - 1, pin_rgb(56, 91, 111));
    for (int i = 0; i < 3; i++) {
        char t[12];
        snprintf(t, sizeof(t), "%c %4.2f", 'A' + i, s->body[i].m);
        pin_rect(p, left + 5, 11 + i * 12, 3, 3, color(i));
        pin_text(p, left + 13, 8 + i * 12, t, color(i));
    }
    char t[16];
    snprintf(t, sizeof(t), "%5.1f", s->time);
    pin_text(p, left + 5, 57, t, white);
    snprintf(t, sizeof(t), "%+.2f", orbit_energy(s));
    pin_text(p, left + 5, 70, t, muted);
    snprintf(t, sizeof(t), "%4.2f%%", fabs((orbit_energy(s) - s->energy0) / s->energy0) * 100);
    pin_text(p, left + 5, 83, t, muted);
}

void orbit_paint(const orbit_t *s, uint16_t *p)
{
    background(p);
    for (unsigned n = 1; n < s->count; n++) {
        unsigned a = (s->head + ORBIT_TRAIL - s->count + n - 1) % ORBIT_TRAIL,
                 b = (a + 1) % ORBIT_TRAIL;
        for (int i = 0; i < 3; i++) {
            int x, y, xx, yy;
            project(s, s->trail[a][i][0], s->trail[a][i][1], &x, &y);
            project(s, s->trail[b][i][0], s->trail[b][i][1], &xx, &yy);
            trail_segment(p, x, y, xx, yy, pin_dim(color(i), 38 + (int)(n * 168 / s->count)));
        }
    }
    for (int i = 0; i < 3; i++) {
        int x, y;
        project(s, s->body[i].x, s->body[i].y, &x, &y);
        int radius = 5 + (int)fmin(3, s->body[i].m * 1.4);
        stellar_glow(p, x, y, radius, color(i));
    }
    paint_panel(s, p);
}
