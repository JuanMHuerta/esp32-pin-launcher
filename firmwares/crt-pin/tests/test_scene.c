// SPDX-License-Identifier: GPL-3.0-only
#include "scene.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool contains(const crt_t *s, const char *text)
{
    for (unsigned i = 0; i < s->visible; i++) {
        if (strstr(s->lines[i].text, text)) {
            return true;
        }
    }
    return false;
}
static uint32_t duration(const crt_event_t *events, unsigned count)
{
    uint32_t total = 0;
    for (unsigned i = 0; i < count; i++) {
        total += events[i].duration;
    }
    return total;
}
static uint32_t ready(crt_t *s, unsigned profile, uint32_t seed)
{
    unsigned count;
    const crt_event_t *boot = crt_boot_script(&count);
    uint32_t time = CRT_WARMUP_MS + duration(boot, count);
    crt_init(s, profile, seed);
    crt_step(s, time);
    assert(!s->booting && s->event == 0 && s->event_ms == 0);
    return time;
}
static void check_events(const crt_event_t *events, unsigned count)
{
    uint16_t glyph_pixels[PIN_W * PIN_H];
    for (unsigned i = 0; i < count; i++) {
        assert(strlen(events[i].text) <= CRT_COLS);
        assert(events[i].duration > 0);
        /* A fast command must still be fully typed before its output starts. */
        if (events[i].kind == CRT_COMMAND) {
            const char *prompt = strchr(events[i].text, '$');
            unsigned base = prompt ? (unsigned)(prompt - events[i].text) + 2 : 0;
            assert(events[i].duration > 100 + (strlen(events[i].text) - base) * 18);
        }
        assert(!strstr(events[i].text, "reboot"));
        assert(!strstr(events[i].text, "shutdown"));
        for (const char *ch = events[i].text; *ch; ch++) {
            if (*ch == ' ') {
                continue;
            }
            memset(glyph_pixels, 0, sizeof(glyph_pixels));
            char text[2] = {*ch, 0};
            pin_text(glyph_pixels, 0, 0, text, 0xffff);
            bool lit = false;
            for (int y = 0; y < 7; y++) {
                for (int x = 0; x < 5; x++) {
                    if (glyph_pixels[y * PIN_W + x]) {
                        lit = true;
                    }
                }
            }
            if (!lit) {
                fprintf(stderr, "Missing glyph for %c\n", *ch);
            }
            assert(lit);
        }
    }
}
int main(void)
{
    crt_t s;
    unsigned boot_count, total_events = 0;
    const crt_event_t *boot = crt_boot_script(&boot_count);
    check_events(boot, boot_count);
    uint32_t boot_ms = CRT_WARMUP_MS + duration(boot, boot_count), loop_ms = 0;
    for (unsigned profile = 0; profile < CRT_PROFILES; profile++) {
        unsigned count, commands = 0;
        const crt_event_t *events = crt_script(profile, &count);
        check_events(events, count);
        total_events += count;
        loop_ms += duration(events, count);
        assert(count >= 85);
        uint32_t now = 0;
        for (unsigned i = 0; i < count; i++) {
            if (events[i].kind == CRT_COMMAND) {
                commands++;
            }
            ready(&s, profile, profile & 1);
            crt_step(&s, now + events[i].duration - 1);
            assert(s.event == i);
            assert(s.visible == CRT_ROWS);
            for (unsigned l = 0; l < s.visible; l++) {
                assert(strlen(s.lines[l].text) <= CRT_COLS);
            }
            if (events[i].kind == CRT_PROGRESS) {
                assert(contains(&s, "99%"));
            }
            if (events[i].kind == CRT_TRACE) {
                const char *trace = s.lines[s.visible - 1].text;
                assert(contains(&s, events[i].text));
                assert(strchr(trace, '|') && strrchr(trace, '|') != strchr(trace, '|'));
                crt_t animated;
                ready(&animated, profile, profile & 1);
                crt_step(&animated, now);
                char first[CRT_COLS + 1];
                memcpy(first, animated.lines[animated.visible - 1].text, sizeof(first));
                crt_step(&animated, 450);
                assert(strcmp(first, animated.lines[animated.visible - 1].text) != 0);
            }
            if (events[i].kind == CRT_COMMAND) {
                assert(contains(&s, events[i].text));
            }
            now += events[i].duration;
        }
        assert(commands >= 20);
        /* All automatic transitions scroll one line, without clearing or booting. */
        ready(&s, profile, profile & 1);
        crt_step(&s, s.cycle_ms - 1);
        crt_line_t retained[CRT_ROWS - 1];
        memcpy(retained, s.lines + 1, sizeof(retained));
        uint64_t clock = s.elapsed_ms;
        crt_step(&s, 1);
        assert(s.profile == (profile + 1) % CRT_PROFILES && s.event == 0 && s.generation == 2);
        assert(!s.booting && s.visible == CRT_ROWS && s.elapsed_ms == clock + 1);
        assert(s.phosphor == (profile & 1));
        assert(memcmp(retained, s.lines, sizeof(retained)) == 0);
        /* Time chunking does not affect typing, scrolling, progress, or transitions. */
        crt_t a, b;
        crt_init(&a, profile, 1234);
        crt_init(&b, profile, 1234);
        for (unsigned i = 0; i < 40000; i++) {
            crt_step(&a, 25);
        }
        crt_step(&b, 1000000);
        assert(memcmp(&a, &b, sizeof(a)) == 0);
    }
    /* Warm-up occurs once, and command prompts precede rapid typing. */
    crt_init(&s, 0, 0);
    assert(s.visible == 0 && s.phosphor == CRT_GREEN);
    crt_step(&s, CRT_WARMUP_MS - 1);
    assert(s.visible == 0);
    crt_step(&s, 1);
    assert(s.visible == 1 && s.booting);
    ready(&s, 0, 0);
    assert(contains(&s, "ops@atlas:$ "));
    assert(!contains(&s, "uname"));
    crt_step(&s, 100 + 4 * 18);
    assert(contains(&s, "unam"));
    assert(!contains(&s, "uname"));
    crt_step(&s, 18);
    assert(contains(&s, "uname"));
    /* Hundreds of playlist wraps retain hue and continue without power collapse. */
    ready(&s, 0, 1);
    uint64_t start = s.elapsed_ms;
    crt_step(&s, loop_ms * 100);
    assert(s.profile == 0 && s.generation == 1 + CRT_PROFILES * 100);
    assert(!s.booting && s.phosphor == CRT_AMBER && s.visible == CRT_ROWS);
    assert(s.elapsed_ms == start + (uint64_t)loop_ms * 100);
    /* Crossing the old 32-bit millisecond limit must not replay startup. */
    s.elapsed_ms = UINT32_MAX - 10ULL;
    crt_step(&s, 100);
    assert(s.elapsed_ms == UINT32_MAX + 90ULL && !s.booting && s.phosphor == CRT_AMBER);
    crt_next(&s, 0);
    assert(s.profile == 1 && s.phosphor == CRT_GREEN);
    assert(s.booting && s.elapsed_ms == 0 && s.visible == 0 &&
           s.generation == 2 + CRT_PROFILES * 100);
    /* Renderer guards and monochrome phosphor throughout both themes. */
    uint16_t *pixels = malloc((PIN_W * PIN_H + 2) * sizeof(*pixels));
    assert(pixels);
    pixels[0] = pixels[PIN_W * PIN_H + 1] = 0x1234;
    for (unsigned theme = 0; theme < 2; theme++) {
        for (unsigned profile = 0; profile < CRT_PROFILES; profile++) {
            ready(&s, profile, theme);
            for (uint32_t t = 0; t < s.cycle_ms; t += 337) {
                crt_t frame = s;
                crt_step(&frame, t);
                crt_paint(&frame, pixels + 1);
                assert(pixels[0] == 0x1234 && pixels[PIN_W * PIN_H + 1] == 0x1234);
                unsigned lit = 0;
                for (int y = 8; y < 114; y++) {
                    for (int x = CRT_TEXT_X; x < 250; x++) {
                        uint16_t c = pixels[1 + y * PIN_W + x];
                        int r = ((c >> 11) & 31) * 8, g = ((c >> 5) & 63) * 4, b = (c & 31) * 8;
                        if (theme == CRT_GREEN) {
                            /* Soft green remains one hue, with a small neutral component
                             * throughout. */
                            assert(g >= 4 * r && g >= 4 * b);
                            if (g > 100) {
                                assert(r > 0 && b > 0);
                            }
                        } else {
                            assert(b == 0);
                            assert(r + 8 >= g); /* RGB565 rounding. */
                        }
                        if (((c >> 5) & 63) > 12) {
                            lit++;
                        }
                    }
                }
                assert(lit > 50); /* No power-off frame, including playlist wraps. */
            }
        }
    }
    /* The recovered footer space must actually show the twelfth text row. */
    ready(&s, 0, 0);
    for (unsigned row = 0; row < CRT_ROWS; row++) {
        snprintf(s.lines[row].text, sizeof(s.lines[row].text), "WATCH RECORD %02u / CRC VERIFIED",
                 row);
    }
    s.cursor = false;
    crt_paint(&s, pixels + 1);
    unsigned bottom_ink = 0;
    for (int y = 105; y < 114; y++) {
        for (int x = CRT_TEXT_X; x < 240; x++) {
            if (((pixels[1 + y * PIN_W + x] >> 5) & 63) > 12) {
                bottom_ink++;
            }
        }
    }
    assert(bottom_ink > 50);
    for (uint32_t t = 0; t < boot_ms; t += 137) {
        crt_init(&s, 0, 0);
        crt_step(&s, t);
        crt_paint(&s, pixels + 1);
        assert(pixels[0] == 0x1234 && pixels[PIN_W * PIN_H + 1] == 0x1234);
    }
    free(pixels);
    printf("CRT tests passed: %u loop events, %.1f second playlist\n", total_events,
           loop_ms / 1000.0);
}
