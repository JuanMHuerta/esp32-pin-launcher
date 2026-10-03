// SPDX-License-Identifier: GPL-3.0-only
#include "scene.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

const char *orbit_name(unsigned n)
{
    static const char *names[] = {"FIGURE EIGHT", "CHAOTIC SUNS", "BINARY VISITOR"};
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
void orbit_init(orbit_t *s, unsigned preset)
{
    memset(s, 0, sizeof(*s));
    s->preset = preset % ORBIT_PRESETS;
    s->generation = 1;
    if (s->preset == 0) {
        /* Simo's initial conditions, Chenciner/Montgomery (2000), G=m=1.
         * https://people.ucsc.edu/~rmont/Nbdy/NbdyB.html */
        s->body[0] = (orbit_body_t){.97000436, -.24308753, .466203685, .432365730, 1};
        s->body[1] = (orbit_body_t){-.97000436, .24308753, .466203685, .432365730, 1};
        s->body[2] = (orbit_body_t){0, 0, -.932407370, -.864731460, 1};
        s->scale = 63;
    } else if (s->preset == 1) {
        s->body[0] = (orbit_body_t){-1, .2, .12, .18, 1};
        s->body[1] = (orbit_body_t){1, .3, -.08, -.14, 1.3};
        s->body[2] = (orbit_body_t){.1, -1.2, -.07, .12, .8};
        s->softening = .04;
        s->scale = 47;
    } else {
        s->body[0] = (orbit_body_t){-.5, 0, 0, -.70, 1};
        s->body[1] = (orbit_body_t){.5, 0, 0, .70, 1};
        s->body[2] = (orbit_body_t){-2, 1.6, .55, -.14, .45};
        s->softening = .025;
        s->scale = 35;
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
    orbit_init(s, n);
    s->generation = g;
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
void orbit_step(orbit_t *s, uint32_t ms)
{
    /* A stalled frame must not trigger an unbounded physics catch-up. */
    if (ms > 100) {
        ms = 100;
    }
    s->elapsed_ms += ms;
    if (s->elapsed_ms >= 45000) {
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
    double extent = 1.1;
    for (int i = 0; i < 3; i++) {
        extent = fmax(extent, fmax(fabs(s->body[i].x), fabs(s->body[i].y) * 2.5));
    }
    double target = fmin(64, 83 / extent);
    s->scale += (target - s->scale) * (1 - exp(-(double)ms * .002));
}
static uint16_t color(int i)
{
    static const int c[3][3] = {{255, 181, 77}, {80, 221, 255}, {255, 88, 108}};
    return pin_rgb(c[i][0], c[i][1], c[i][2]);
}
static void project(const orbit_t *s, double x, double y, int *px, int *py)
{
    *px = (int)lround(91 + x * s->scale);
    *py = (int)lround(66 - y * s->scale);
}
static void trail_segment(uint16_t *p, int x, int y, int xx, int yy, uint16_t c)
{
    /* Keep plot lines out of the telemetry; distant escaped stars stay clipped. */
    if (x < 4 || x > 177 || xx < 4 || xx > 177 || y < 24 || y > 108 || yy < 24 || yy > 108) {
        return;
    }
    pin_line(p, x, y, xx, yy, c);
}
void orbit_paint(const orbit_t *s, uint16_t *p)
{
    uint16_t muted = pin_rgb(85, 117, 135), white = pin_rgb(208, 229, 231);
    for (int y = 0; y < PIN_H; y++) {
        for (int x = 0; x < PIN_W; x++) {
            p[y * PIN_W + x] = pin_rgb(3, 6 + (PIN_H - y) / 40, 12 + (PIN_W - x) / 90);
        }
    }
    for (unsigned i = 0; i < 82; i++) {
        unsigned h = i * 2654435761u;
        int x = 4 + (h % 174), y = 25 + ((h >> 10) % 82);
        p[y * PIN_W + x] = pin_rgb(20 + (h & 31), 31 + (h & 31), 49 + (h & 31));
    }
    for (int x = 19; x < 180; x += 24) {
        pin_line(p, x, 25, x, 108, pin_rgb(9, 20, 29));
    }
    for (int y = 30; y < 110; y += 20) {
        pin_line(p, 4, y, 177, y, pin_rgb(9, 20, 29));
    }
    pin_text(p, 7, 6, "THREE BODY", white);
    pin_text(p, 7, 16, orbit_name(s->preset), muted);
    pin_line(p, 183, 4, 183, 111, pin_rgb(31, 58, 69));
    pin_text(p, 190, 6, "GRAVITY LAB", white);
    for (unsigned n = 1; n < s->count; n++) {
        unsigned a = (s->head + ORBIT_TRAIL - s->count + n - 1) % ORBIT_TRAIL,
                 b = (a + 1) % ORBIT_TRAIL;
        for (int i = 0; i < 3; i++) {
            int x, y, xx, yy;
            project(s, s->trail[a][i][0], s->trail[a][i][1], &x, &y);
            project(s, s->trail[b][i][0], s->trail[b][i][1], &xx, &yy);
            trail_segment(p, x, y, xx, yy, pin_dim(color(i), 45 + (int)(n * 145 / s->count)));
        }
    }
    for (int i = 0; i < 3; i++) {
        int x, y;
        project(s, s->body[i].x, s->body[i].y, &x, &y);
        if (x >= 4 && x <= 177 && y >= 25 && y <= 108) {
            for (int dy = -7; dy <= 7; dy++) {
                for (int dx = -7; dx <= 7; dx++) {
                    int r = dx * dx + dy * dy;
                    if (r <= 49 && x + dx >= 4 && x + dx <= 177 && y + dy >= 25 && y + dy <= 108) {
                        uint16_t c = pin_dim(color(i), r < 6    ? 256
                                                       : r < 13 ? 220
                                                       : r < 26 ? 90
                                                                : 32);
                        if (r < 3) {
                            c = pin_rgb(255, 247, 224);
                        }
                        p[(y + dy) * PIN_W + x + dx] = c;
                    }
                }
            }
            pin_text(p, x > 160 ? x - 12 : x + 8, y - 3,
                     i == 0   ? "A"
                     : i == 1 ? "B"
                              : "C",
                     color(i));
        }
        char t[16];
        snprintf(t, sizeof(t), "%c M=%.2f", 'A' + i, s->body[i].m);
        pin_text(p, 190, 22 + i * 12, t, color(i));
    }
    char t[20];
    snprintf(t, sizeof(t), "T %6.2f", s->time);
    pin_text(p, 190, 64, t, white);
    snprintf(t, sizeof(t), "E %+.3f", orbit_energy(s));
    pin_text(p, 190, 76, t, muted);
    snprintf(t, sizeof(t), "dE %.4f%%", fabs((orbit_energy(s) - s->energy0) / s->energy0) * 100);
    pin_text(p, 190, 88, t, muted);
    pin_text(p, 190, 101, s->softening ? "SOFTENED" : "NEWTONIAN", muted);
    pin_line(p, 5, 112, 262, 112, pin_rgb(31, 58, 69));
    pin_text(p, 7, 113, "BOOT: NEXT   HOLD: LIBRARY", muted);
}
