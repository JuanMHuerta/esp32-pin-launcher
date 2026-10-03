// SPDX-License-Identifier: GPL-3.0-only
#include "scene.h"

#include "assets_generated.h"
#include "exterior_generated.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int8_t unit_x[64] = {
    64,  63,  63,  61,  59,  56,  53,  49,  45,  40,  35,  29,  24,  18,  12,  6,
    0,   -6,  -12, -18, -24, -29, -35, -40, -45, -49, -53, -56, -59, -61, -63, -63,
    -64, -63, -63, -61, -59, -56, -53, -49, -45, -40, -35, -29, -24, -18, -12, -6,
    0,   6,   12,  18,  24,  29,  35,  40,  45,  49,  53,  56,  59,  61,  63,  63,
};
static const int8_t unit_y[64] = {
    0,   6,   12,  18,  24,  29,  35,  40,  45,  49,  53,  56,  59,  61,  63,  63,
    64,  63,  63,  61,  59,  56,  53,  49,  45,  40,  35,  29,  24,  18,  12,  6,
    0,   -6,  -12, -18, -24, -29, -35, -40, -45, -49, -53, -56, -59, -61, -63, -63,
    -64, -63, -63, -61, -59, -56, -53, -49, -45, -40, -35, -29, -24, -18, -12, -6,
};

static const uint8_t font[36][7] = {
    {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14},
    {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17}, {14, 4, 4, 4, 4, 4, 14},
    {1, 1, 1, 1, 17, 17, 14},     {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17}, {14, 17, 17, 17, 17, 17, 14},
    {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},       {17, 17, 17, 17, 17, 17, 14},
    {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31},     {14, 17, 19, 21, 25, 17, 14},
    {4, 12, 4, 4, 4, 4, 14},      {14, 17, 1, 2, 4, 8, 31},     {30, 1, 1, 14, 1, 1, 30},
    {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30},   {14, 16, 16, 30, 17, 17, 14},
    {31, 1, 2, 4, 8, 8, 8},       {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14},
};

static uint32_t random_next(wayfarer_scene_t *scene)
{
    uint32_t value = scene->rng;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    scene->rng = value;
    return value;
}

static uint32_t hash32(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    return value ^ (value >> 16);
}

static uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
    return (uint16_t)(((uint16_t)(red >> 3) << 11) | ((uint16_t)(green >> 2) << 5) | (blue >> 3));
}

static void put_pixel(uint16_t *pixels, int y0, int rows, int x, int y, uint16_t color)
{
    if (x < 0 || x >= WAYFARER_WIDTH || y < y0 || y >= y0 + rows) {
        return;
    }
    pixels[(y - y0) * WAYFARER_WIDTH + x] = color;
}

static void blend_pixel(uint16_t *pixels, int y0, int rows, int x, int y, uint16_t color,
                        unsigned alpha)
{
    if (!alpha || x < 0 || x >= WAYFARER_WIDTH || y < y0 || y >= y0 + rows) {
        return;
    }
    if (alpha >= 255) {
        put_pixel(pixels, y0, rows, x, y, color);
        return;
    }
    uint16_t *dst = &pixels[(y - y0) * WAYFARER_WIDTH + x];
    unsigned inv = 255 - alpha;
    unsigned red = (((*dst >> 11) & 31) * inv + ((color >> 11) & 31) * alpha) / 255;
    unsigned green = (((*dst >> 5) & 63) * inv + ((color >> 5) & 63) * alpha) / 255;
    unsigned blue = ((*dst & 31) * inv + (color & 31) * alpha) / 255;
    *dst = (uint16_t)((red << 11) | (green << 5) | blue);
}

static void fill_rect(uint16_t *pixels, int y0, int rows, int x, int y, int width, int height,
                      uint16_t color)
{
    int x1 = x < 0 ? 0 : x;
    int y1 = y < y0 ? y0 : y;
    int x2 = x + width > WAYFARER_WIDTH ? WAYFARER_WIDTH : x + width;
    int y2 = y + height > y0 + rows ? y0 + rows : y + height;
    for (int py = y1; py < y2; ++py) {
        uint16_t *out = pixels + (py - y0) * WAYFARER_WIDTH;
        for (int px = x1; px < x2; ++px) {
            out[px] = color;
        }
    }
}

static void fill_rect_alpha(uint16_t *pixels, int y0, int rows, int x, int y, int width, int height,
                            uint16_t color, unsigned alpha)
{
    int x1 = x < 0 ? 0 : x;
    int y1 = y < y0 ? y0 : y;
    int x2 = x + width > WAYFARER_WIDTH ? WAYFARER_WIDTH : x + width;
    int y2 = y + height > y0 + rows ? y0 + rows : y + height;
    for (int py = y1; py < y2; ++py) {
        for (int px = x1; px < x2; ++px) {
            blend_pixel(pixels, y0, rows, px, py, color, alpha);
        }
    }
}

static void draw_line(uint16_t *pixels, int y0, int rows, int x0, int ystart, int x1, int y1,
                      uint16_t color, unsigned alpha)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - ystart), sy = ystart < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        blend_pixel(pixels, y0, rows, x0, ystart, color, alpha);
        if (x0 == x1 && ystart == y1) {
            break;
        }
        int twice = 2 * error;
        if (twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx) {
            error += dx;
            ystart += sy;
        }
    }
}

static void draw_circle(uint16_t *pixels, int y0, int rows, int cx, int cy, int radius,
                        uint16_t color, unsigned alpha)
{
    for (int i = 0; i < 64; ++i) {
        int x = cx + unit_x[i] * radius / 64;
        int y = cy + unit_y[i] * radius / 64;
        blend_pixel(pixels, y0, rows, x, y, color, alpha);
    }
}

static void draw_text(uint16_t *pixels, int y0, int rows, int x, int y, const char *text,
                      uint16_t color, int scale)
{
    for (; *text; ++text, x += 6 * scale) {
        if (*text == ' ') {
            continue;
        }
        int index = *text >= 'A' && *text <= 'Z'   ? *text - 'A'
                    : *text >= '0' && *text <= '9' ? 26 + *text - '0'
                                                   : -1;
        if (index < 0) {
            continue;
        }
        for (int gy = 0; gy < 7; ++gy) {
            for (int gx = 0; gx < 5; ++gx) {
                if (!(font[index][gy] & (1 << (4 - gx)))) {
                    continue;
                }
                fill_rect(pixels, y0, rows, x + gx * scale, y + gy * scale, scale, scale, color);
            }
        }
    }
}

/* The travel clock is integrated: entering/leaving a jump changes velocity
 * without teleporting stars or the distant scenery. All layers share it. */
static unsigned travel_speed(const wayfarer_scene_t *scene, uint32_t now_ms)
{
    if (scene->event != WAYFARER_EVENT_WARP) {
        return 256;
    }
    uint32_t age = now_ms - scene->event_started_ms;
    if (age >= scene->event_duration_ms) {
        return 256;
    }
    uint32_t ramp = age;
    uint32_t remaining = scene->event_duration_ms - age;
    if (ramp > remaining) {
        ramp = remaining;
    }
    if (ramp > 900) {
        ramp = 900;
    }
    return 256 + ramp * 1280 / 900;
}

/* Objects retain their world coordinates across encounter and jump boundaries. */
wayfarer_projection_t wayfarer_scene_project_body(const wayfarer_scene_t *scene,
                                                  const wayfarer_body_t *body, uint32_t now_ms)
{
    wayfarer_projection_t p = {0};
    if (!body->type) { return p; }
    uint32_t age = now_ms - body->born_ms;
    int advance = (int)((scene->travel_q8 - body->born_travel_q8) / (256U * 20U));
    int x = body->x + (int)((int64_t)body->vx * age / 1000);
    int y = body->y + (int)((int64_t)body->vy * age / 1000);
    p.z = body->z + (int)((int64_t)body->vz * age / 1000) - advance;
    if (p.z < 32 || age > 120000) { return p; }
    p.x = scene->focus_x + x * 256 / p.z;
    p.y = scene->focus_y + y * 256 / p.z;
    p.width = body->size * 256 / p.z;
    p.light = age < 1200 ? age * 255 / 1200 : 255;
    p.visible = p.width > 0 && p.x + p.width > 0 && p.x - p.width < WAYFARER_WIDTH &&
                p.y + p.width > 0 && p.y - p.width < WAYFARER_HEIGHT;
    return p;
}

static wayfarer_body_t *new_body(wayfarer_scene_t *scene, unsigned type, uint32_t now_ms)
{
    for (unsigned i = 0; i < WAYFARER_BODY_CAPACITY; ++i) {
        wayfarer_body_t *b = &scene->bodies[i];
        if (!b->type || wayfarer_scene_project_body(scene,b,now_ms).z < 32 || now_ms-b->born_ms > 120000) {
            memset(b,0,sizeof(*b));
            b->type=(uint8_t)type; b->born_ms=now_ms; b->born_travel_q8=scene->travel_q8;
            return b;
        }
    }
    return NULL;
}

static uint8_t heading(int vx, int vy)
{
    int best = -2147483647;
    uint8_t result=0;
    for (unsigned i=0;i<64;++i) {
        int dot=unit_x[i]*vx+unit_y[i]*vy;
        if (dot>best) { best=dot; result=(uint8_t)i; }
    }
    return result;
}

static uint8_t next_local_event(wayfarer_scene_t *scene)
{
    const unsigned local_mask=((1U<<WAYFARER_EVENT_COUNT)-2U) &
                               ~(1U<<WAYFARER_EVENT_PLANET) & ~(1U<<WAYFARER_EVENT_WARP);
    if (!scene->events_remaining) { scene->events_remaining=(uint16_t)local_mask; }
    unsigned pick=random_next(scene) % (unsigned)__builtin_popcount(scene->events_remaining);
    for (uint8_t event=1;event<WAYFARER_EVENT_COUNT;++event) {
        if ((scene->events_remaining & (1U<<event)) && pick--==0) {
            scene->events_remaining &= (uint16_t)~(1U<<event);
            return event;
        }
    }
    return WAYFARER_EVENT_TRAFFIC;
}

static void spawn_ship(wayfarer_scene_t *scene, uint32_t now_ms, unsigned member, bool escort)
{
    wayfarer_body_t *b=new_body(scene,WAYFARER_BODY_SHIP,now_ms);
    if (!b) { return; }
    uint32_t h=hash32(scene->seed+scene->leg*3907U+scene->variant*7919U+member*1123U);
    int side=(h&1)?1:-1;
    b->model=(uint8_t)((scene->variant+member*2)%5);
    b->x=(int16_t)(side*(escort?170:270)+(int)member*18);
    b->y=(int16_t)((h&2?-1:1)*(escort?32:52)+(int)member*22);
    b->z=(int16_t)((escort?510:1100)+(int)member*180);
    b->size=(int16_t)(escort?105:145+(int)member*15);
    b->vx=(int16_t)(escort?0:-side*(28+(int)((h>>4)%29)));
    b->vy=(int16_t)(escort?0:(h&4?1:-1)*(8+(int)((h>>9)%13)));
    b->vz=(int16_t)(escort?50:10+(int)((h>>13)%21));
    /* A ship's own attitude persists when the observer accelerates past it. */
    b->angle=heading(b->vx*b->z+b->x*50,b->vy*b->z+b->y*50);
}

static void begin_leg(wayfarer_scene_t *scene, uint32_t now_ms)
{
    ++scene->leg;
    scene->local_event=next_local_event(scene);
    uint32_t h=hash32(scene->seed+scene->leg*7919U);
    wayfarer_body_t *planet=new_body(scene,WAYFARER_BODY_WORLD,now_ms);
    if (planet) {
        unsigned angle=(unsigned[]){2,6,26,30,34,38,58,62,14,50}[h%10];
        planet->model=(uint8_t)((h>>8)%6);
        int impact=225+(int)((h>>17)%36);
        planet->size=210;
        planet->x=(int16_t)(unit_x[angle]*impact/64);
        planet->y=(int16_t)(unit_y[angle]*impact/64);
        planet->z=1580;
    }
    /* Establish local landmarks at arrival, before the encounter approaches. */
    if (scene->local_event==WAYFARER_EVENT_STATION || scene->local_event==WAYFARER_EVENT_ECLIPSE) {
        wayfarer_body_t *b=new_body(scene,scene->local_event==WAYFARER_EVENT_STATION?
                                  WAYFARER_BODY_STATION:WAYFARER_BODY_ECLIPSE,now_ms);
        if (b) {
            b->x=(int16_t)((h&1?1:-1)*(b->type==WAYFARER_BODY_ECLIPSE?420:225));
            b->y=(int16_t)(-48+(int)((h>>13)%65));
            b->z=(int16_t)(b->type==WAYFARER_BODY_ECLIPSE?3150:1850);
            b->size=(int16_t)(b->type==WAYFARER_BODY_ECLIPSE?720:210);
            b->model=5;
        }
    }
    spawn_ship(scene,now_ms,0,false);
}

void wayfarer_scene_begin_event(wayfarer_scene_t *scene, uint8_t event, uint32_t now_ms,
                                uint32_t duration_ms, uint8_t variant, int direction)
{
    scene->event=event; scene->event_started_ms=now_ms;
    scene->event_duration_ms=duration_ms?duration_ms:1;
    scene->variant=variant; scene->direction=(int8_t)direction;
    scene->event_y=(int16_t)(64+variant%44);
    if (event==WAYFARER_EVENT_TRAFFIC || event==WAYFARER_EVENT_CONTACT || event==WAYFARER_EVENT_CONVOY) {
        unsigned count=event==WAYFARER_EVENT_CONVOY?3U:1U;
        for (unsigned i=0;i<count;++i) { spawn_ship(scene,now_ms,i,false); }
    } else if (event==WAYFARER_EVENT_STATION || event==WAYFARER_EVENT_ECLIPSE) {
        unsigned type=event==WAYFARER_EVENT_STATION?WAYFARER_BODY_STATION:WAYFARER_BODY_ECLIPSE;
        wayfarer_body_t *b=NULL;
        for (unsigned i=0;i<WAYFARER_BODY_CAPACITY;++i) {
            if (scene->bodies[i].type==type) { b=&scene->bodies[i]; break; }
        }
        if (!b) {
            b=new_body(scene,type,now_ms);
            if (b) {
                b->x=(int16_t)(direction*(type==WAYFARER_BODY_ECLIPSE?420:225));
                b->y=(int16_t)(-50+(variant%70));
                b->z=(int16_t)(type==WAYFARER_BODY_ECLIPSE?2000:1200);
                b->size=(int16_t)(type==WAYFARER_BODY_ECLIPSE?720:210);
            }
        }
        if (b) { b->phase_active=true; b->phase_started_ms=now_ms; b->phase_duration_ms=scene->event_duration_ms; }
    } else if (event==WAYFARER_EVENT_ALERT) {
        for (unsigned i=0;i<3;++i) {
            wayfarer_body_t *b=new_body(scene,WAYFARER_BODY_ASTEROID,now_ms);
            if (!b) { break; }
            unsigned angle=(variant+i*21)&63;
            b->x=(int16_t)(unit_x[angle]*100/64); b->y=(int16_t)(unit_y[angle]*100/64);
            b->z=(int16_t)(600+i*170); b->size=(int16_t)(30+i*7);
        }
    } else if (event==WAYFARER_EVENT_COMET) {
        wayfarer_body_t *b=new_body(scene,WAYFARER_BODY_COMET,now_ms);
        if (b) {
            b->x=(int16_t)(direction*-350); b->y=(int16_t)(-95+(variant%95)); b->z=1500;
            b->vx=(int16_t)(direction*100); b->vy=(int16_t)((variant&1)?18:-18);
            b->vz=20; b->size=24; b->angle=heading(b->vx,b->vy);
        }
    }
}

void wayfarer_scene_init(wayfarer_scene_t *scene, uint32_t seed)
{
    memset(scene,0,sizeof(*scene));
    scene->seed=seed?seed:0x6d2b79f5U; scene->rng=scene->seed;
    scene->speed_q8=256; scene->direction=1;
    scene->focus_x=(int16_t)(230+scene->seed%77);
    scene->focus_y=(int16_t)(62+(scene->seed>>8)%25);
    begin_leg(scene,0);
    wayfarer_scene_begin_event(scene,WAYFARER_EVENT_PLANET,0,18000,scene->bodies[0].model,1);
}

void wayfarer_scene_update(wayfarer_scene_t *scene, uint32_t now_ms, bool tap)
{
    unsigned speed=travel_speed(scene,now_ms);
    scene->travel_q8+=(uint64_t)(now_ms-scene->updated_ms)*(speed+scene->speed_q8)/2;
    scene->updated_ms=now_ms; scene->speed_q8=(uint16_t)speed;
    if (tap) { scene->ping_active=true; scene->ping_started_ms=now_ms; }
    if (scene->ping_active && now_ms-scene->ping_started_ms>=1700) { scene->ping_active=false; }
    for (unsigned i=0;i<WAYFARER_BODY_CAPACITY;++i) {
        wayfarer_body_t *b=&scene->bodies[i];
        if (b->type && (wayfarer_scene_project_body(scene,b,now_ms).z<32 || now_ms-b->born_ms>120000)) {
            b->type=WAYFARER_BODY_NONE;
        }
    }
    if (scene->event!=WAYFARER_EVENT_NONE) {
        if (now_ms-scene->event_started_ms<scene->event_duration_ms) { return; }
        scene->previous_event=scene->event; scene->event=WAYFARER_EVENT_NONE;
        if (scene->previous_event==WAYFARER_EVENT_WARP) {
            /* The departure has physically passed the old scenery. */
            memset(scene->bodies,0,sizeof(scene->bodies));
            begin_leg(scene,now_ms); scene->journey_stage=0;
            scene->next_event_ms=now_ms+650;
        } else {
            ++scene->journey_stage;
            scene->next_event_ms=now_ms+2000;
            if (scene->journey_stage==2) { spawn_ship(scene,now_ms,1,true); }
        }
    }
    if ((int32_t)(now_ms-scene->next_event_ms)<0) { return; }
    uint8_t event=scene->journey_stage==0?WAYFARER_EVENT_PLANET:
                  scene->journey_stage==1?scene->local_event:WAYFARER_EVENT_WARP;
    unsigned duration=event==WAYFARER_EVENT_PLANET?18000:event==WAYFARER_EVENT_WARP?8000:
                      event==WAYFARER_EVENT_ECLIPSE?18000:event==WAYFARER_EVENT_STATION?17000:
                      event==WAYFARER_EVENT_ALERT?8000:12000;
    uint8_t variant=(uint8_t)random_next(scene);
    int direction=(random_next(scene)&1)?1:-1;
    wayfarer_scene_begin_event(scene,event,now_ms,duration,variant,direction);
}

static void draw_sprite(const wayfarer_sprite_t *sprite, int left, int top, int width, bool mirror,
                        unsigned alpha, int y0, int rows, uint16_t *pixels)
{
    if (width < 1) {
        return;
    }
    int height = sprite->height * width / sprite->width;
    if (height < 1 || top + height <= y0 || top >= y0 + rows || left + width <= 0 ||
        left >= WAYFARER_WIDTH) {
        return;
    }
    int x0 = left < 0 ? 0 : left;
    int x1 = left + width > WAYFARER_WIDTH ? WAYFARER_WIDTH : left + width;
    int first = top < y0 ? y0 : top;
    int last = top + height > y0 + rows ? y0 + rows : top + height;
    unsigned step = (unsigned)(sprite->width << 16) / (unsigned)width;
    for (int y = first; y < last; ++y) {
        const uint16_t *source =
            sprite->pixels + (y - top) * sprite->height / height * sprite->width;
        unsigned sample = (unsigned)(x0 - left) * step;
        uint16_t *out = pixels + (y - y0) * WAYFARER_WIDTH;
        for (int x = x0; x < x1; ++x, sample += step) {
            int sx = (int)(sample >> 16);
            uint16_t color = source[mirror ? sprite->width - 1 - sx : sx];
            if (color) {
                if (alpha >= 255) { out[x] = color; }
                else { blend_pixel(pixels, y0, rows, x, y, color, alpha); }
            }
        }
    }

}

static void draw_indexed_sprite(const wayfarer_indexed_t *sprite, int left, int top, int width,
                                bool mirror, unsigned light, unsigned opacity, int y0, int rows, uint16_t *pixels)
{
    if (width < 1) { return; }
    int height = sprite->height * width / sprite->width;
    if (height < 1 || top + height <= y0 || top >= y0 + rows || left + width <= 0 || left >= WAYFARER_WIDTH) {
        return;
    }
    uint16_t shaded[64];
    const uint16_t *palette = sprite->palette;
    if (light < 255) {
        for (unsigned i = 0; i < 64; ++i) {
            uint16_t c = palette[i];
            shaded[i] = (uint16_t)(((c >> 11) * light / 255U) << 11 |
                                   (((c >> 5) & 63) * light / 255U) << 5 |
                                   ((c & 31) * light / 255U));
        }
        palette = shaded;
    }
    int x0 = left < 0 ? 0 : left;
    int x1 = left + width > WAYFARER_WIDTH ? WAYFARER_WIDTH : left + width;
    int first = top < y0 ? y0 : top;
    int last = top + height > y0 + rows ? y0 + rows : top + height;
    unsigned step = (unsigned)(sprite->width << 16) / (unsigned)width;
    for (int y = first; y < last; ++y) {
        const uint8_t *source = sprite->pixels + (y - top) * sprite->height / height * sprite->width;
        unsigned sample = (unsigned)(x0 - left) * step;
        uint16_t *out = pixels + (y - y0) * WAYFARER_WIDTH;
        for (int x = x0; x < x1; ++x, sample += step) {
            int sx = (int)(sample >> 16);
            unsigned index = source[mirror ? sprite->width - 1 - sx : sx];
            if (index) {
                if (opacity>=255) { out[x] = palette[index]; }
                else { blend_pixel(pixels,y0,rows,x,y,palette[index],opacity); }
            }
        }
    }
}

static void draw_space_glow(int cx, int cy, int radius, uint16_t color, unsigned power,
                            int y0, int rows, uint16_t *pixels)
{
    if (radius < 1) { return; }
    /* Compute the same integer falloff once, rather than divide at every pixel. */
    unsigned falloff[radius + 1];
    for (int i = 0; i <= radius; ++i) { falloff[i] = power * (unsigned)i / (unsigned)radius; }
    int first = cy - radius > y0 ? cy - radius : y0;
    int last = cy + radius < y0 + rows ? cy + radius : y0 + rows;
    for (int y = first; y < last; ++y) {
        int vertical = abs(y - cy) * 2;
        if (vertical >= radius) { continue; }
        int reach = radius - vertical;
        int left = cx - reach < 0 ? 0 : cx - reach;
        int right = cx + reach > WAYFARER_WIDTH ? WAYFARER_WIDTH : cx + reach;
        for (int x = left; x < right; ++x) {
            unsigned alpha = falloff[reach - abs(x - cx)];
            blend_pixel(pixels, y0, rows, x, y, color, alpha);
        }
    }
}

static const wayfarer_indexed_t *ship_art(unsigned model)
{
    const wayfarer_indexed_t *art[]={&wayfarer_ship_freighter,&wayfarer_ship_courier,
        &wayfarer_ship_tanker,&wayfarer_shuttle,&wayfarer_tug};
    return art[model%5];
}

/* Inverse nearest-neighbor sampling rotates the actual pixel hull and its lights. */
static void draw_rotated_ship(const wayfarer_body_t *b, const wayfarer_projection_t *p,
                              uint32_t now_ms, int y0, int rows, uint16_t *pixels)
{
    const wayfarer_indexed_t *art=ship_art(b->model);
    int w=p->width, h=art->height*w/art->width;
    int c=unit_x[b->angle], s=unit_y[b->angle];
    int bx=(abs(c)*w+abs(s)*h)/128+2, by=(abs(s)*w+abs(c)*h)/128+2;
    if (w<1 || p->y+by+w/3+12<y0 || p->y-by-w/3-12>=y0+rows) { return; }
    int first=p->y-by>y0?p->y-by:y0, last=p->y+by+1<y0+rows?p->y+by+1:y0+rows;
    int left=p->x-bx>0?p->x-bx:0, right=p->x+bx+1<WAYFARER_WIDTH?p->x+bx+1:WAYFARER_WIDTH;
    int du=c*art->width*1024/w, dv=-s*art->width*1024/w;
    uint16_t palette[64];
    memcpy(palette,art->palette,sizeof(palette));
    int plume=8+w/5+(unit_y[(now_ms/35+b->model*9)&63]/18);
    for (int engine=-1;engine<=1;engine+=2) {
        int ex=p->x-c*w*43/(64*100)-s*engine*h/(64*4);
        int ey=p->y-s*w*43/(64*100)+c*engine*h/(64*4);
        for (int d=plume;d>=0;--d) {
            int x=ex-c*d/64, y=ey-s*d/64;
            blend_pixel(pixels,y0,rows,x,y,d<plume/4?rgb565(146,191,210):rgb565(51,109,157),
                        (unsigned)(plume-d)*p->light/(unsigned)(plume+1));
            if (w>80 && d<plume/2) { blend_pixel(pixels,y0,rows,x-s/64,y+c/64,rgb565(51,109,157),70); }
        }
    }
    for (int y=first;y<last;++y) {
        int u=(art->width<<15)+(left-p->x)*du-(y-p->y)*dv;
        int v=(art->height<<15)+(left-p->x)*dv+(y-p->y)*du;
        uint16_t *out=pixels+(y-y0)*WAYFARER_WIDTH;
        for (int x=left;x<right;++x,u+=du,v+=dv) {
            unsigned sx=(unsigned)(u>>16), sy=(unsigned)(v>>16);
            if (sx<(unsigned)art->width && sy<(unsigned)art->height) {
                unsigned index=art->pixels[sy*art->width+sx];
                if (index) {
                    if (p->light>=255) { out[x]=palette[index]; }
                    else { blend_pixel(pixels,y0,rows,x,y,palette[index],p->light); }
                }
            }
        }
    }
    if ((now_ms/420+b->model)%7<2) {
        int x=p->x-s*h/(64*3), y=p->y+c*h/(64*3);
        blend_pixel(pixels,y0,rows,x,y,rgb565(182,99,66),p->light);
        blend_pixel(pixels,y0,rows,p->x+s*h/(64*3),p->y-c*h/(64*3),rgb565(95,159,138),p->light);
    }
}

static unsigned integer_root(unsigned value)
{
    unsigned result=0, bit=1U<<14;
    while (bit>value) { bit>>=2; }
    while (bit) {
        if (value>=result+bit) { value-=result+bit; result=(result>>1)+bit; }
        else { result>>=1; }
        bit>>=2;
    }
    return result;
}

static uint8_t solar_pixels[128*128], solar_alpha[64];
static uint16_t solar_palette[64];
static bool solar_ready;

static void prepare_solar_art(void)
{
    if (solar_ready) { return; }
    for (unsigned i=1;i<32;++i) {
        solar_palette[i]=rgb565((uint8_t)(134+i*3),(uint8_t)(86+i*4),(uint8_t)(55+i*3));
        solar_alpha[i]=255;
    }
    for (unsigned i=33;i<64;++i) {
        solar_palette[i]=rgb565(174,184,196);
        solar_alpha[i]=(uint8_t)((i-32)*7);
    }
    solar_palette[32]=rgb565(199,94,58); solar_alpha[32]=150;
    for (int y=0;y<128;++y) {
        for (int x=0;x<128;++x) {
            int dx=x-64, dy=y-64, r2=dx*dx+dy*dy;
            uint32_t grain=hash32((unsigned)(x*7919+y*3907));
            unsigned index=0;
            if (r2<29*29) {
                int tone=29-r2/54+(int)(grain%5)-2;
                if ((dx+9)*(dx+9)+(dy-5)*(dy-5)<13) { tone-=9; }
                if (tone<1) { tone=1; } if (tone>30) { tone=30; }
                index=(unsigned)tone+1;
            } else if (r2<63*63) {
                int radius=(int)integer_root((unsigned)r2);
                int angle=abs(dy)*16/(abs(dx)+abs(dy));
                if (dx<0) { angle=32-angle; } if (dy<0) { angle=(64-angle)&63; }
                /* Uneven magnetic streamers fade into space, rather than a solid ring. */
                int ray=45+(unit_x[(angle*2+12)&63]+64)*60/128+
                        unit_y[(angle*5+13)&63]/4+unit_y[(angle*9+radius/3)&63]/5;
                int power=(63-radius)*ray/100;
                if (radius<33) { power+=7; }
                if ((grain&3)==0) { power=power*3/4; }
                if (power>30) { power=30; }
                if (power>0) { index=(unsigned)power+33; }
                int loop=(dx+13)*(dx+13)+(dy+32)*(dy+32);
                if (loop>9*9 && loop<11*11 && r2>29*29 && r2<46*46) { index=32; }
            }
            solar_pixels[y*128+x]=(uint8_t)index;
        }
    }
    solar_ready=true;
}

static void draw_solar_art(const wayfarer_projection_t *p,int y0,int rows,uint16_t *pixels)
{
    prepare_solar_art();
    int left=p->x-p->width/2, top=p->y-p->width/2, width=p->width;
    if (width<1) { return; }
    int x0=left>0?left:0, x1=left+width<WAYFARER_WIDTH?left+width:WAYFARER_WIDTH;
    int first=top>y0?top:y0, last=top+width<y0+rows?top+width:y0+rows;
    unsigned step=128U*65536U/(unsigned)width;
    for (int y=first;y<last;++y) {
        const uint8_t *source=solar_pixels+(y-top)*128/width*128;
        unsigned sample=(unsigned)(x0-left)*step;
        for (int x=x0;x<x1;++x,sample+=step) {
            unsigned index=source[sample>>16];
            if (index) { blend_pixel(pixels,y0,rows,x,y,solar_palette[index],solar_alpha[index]*p->light/255); }
        }
    }
}

static void draw_eclipse(const wayfarer_scene_t *scene,const wayfarer_body_t *b,
                          const wayfarer_projection_t *p,uint32_t now_ms,int y0,int rows,uint16_t *pixels)
{
    (void)scene;
    draw_solar_art(p,y0,rows,pixels);
    int phase=b->phase_active?(int)((uint64_t)(now_ms-b->phase_started_ms)*1024/b->phase_duration_ms):0;
    int offset=(int)((int64_t)(phase-512)*p->width*110/102400);
    const wayfarer_indexed_t *moon=&wayfarer_worlds[5];
    int width=p->width*80*moon->width/(128*66);
    int height=width*moon->height/moon->width;
    int cx=p->x+offset, cy=p->y+offset/4;
    draw_indexed_sprite(moon,cx-width/2,cy-height/2,width,false,92,p->light,y0,rows,pixels);
    /* A localized bright bead at second/third contact; no uniform orange outline. */
    if (phase>365 && phase<410) {
        blend_pixel(pixels,y0,rows,p->x+p->width*28/128,p->y-2,rgb565(239,221,179),215);
    }
}

static unsigned sky_sector(const wayfarer_scene_t *scene)
{
    return (unsigned)((scene->travel_q8 >> 8) / 70000 + scene->seed % 5) % 5;
}

static void draw_space(const wayfarer_scene_t *scene, uint32_t now_ms, int y0, int rows,
                       uint16_t *pixels)
{
    static uint16_t colors[256];
    static uint32_t cached_time, cached_seed, cached_start, cached_duration;
    static uint64_t cached_travel;
    static uint8_t cached_event, cached_variant;
    static bool ready;
    uint32_t travel = (uint32_t)(scene->travel_q8 >> 8);
    unsigned level = 0;
    if (scene->event == WAYFARER_EVENT_NEBULA) {
        uint32_t age = now_ms - scene->event_started_ms;
        uint32_t fade = age;
        uint32_t remaining = age < scene->event_duration_ms ? scene->event_duration_ms - age : 0;
        if (fade > remaining) { fade = remaining; }
        if (fade > 1600) { fade = 1600; }
        level = fade / 160;
    }
    if (!ready || cached_time != now_ms || cached_seed != scene->seed || cached_travel != scene->travel_q8 ||
        cached_event != scene->event || cached_variant != scene->variant ||
        cached_start != scene->event_started_ms || cached_duration != scene->event_duration_ms) {
        /* Slowly morph between navy, teal, violet, amber and emerald dust regions. */
        static const unsigned tint[5][3] = {{77,91,119},{57,112,114},{118,76,128},{134,96,69},{72,109,89}};
        unsigned sector = sky_sector(scene);
        if (scene->event == WAYFARER_EVENT_NEBULA) { sector = (sector + scene->variant % 5) % 5; }
        unsigned next = (sector + 1) % 5;
        unsigned phase = travel % 70000;
        unsigned mix = phase > 54000 ? (phase - 54000) * 256 / 16000 : 0;
        unsigned r = (tint[sector][0] * (256 - mix) + tint[next][0] * mix) >> 8;
        unsigned g = (tint[sector][1] * (256 - mix) + tint[next][1] * mix) >> 8;
        unsigned b = (tint[sector][2] * (256 - mix) + tint[next][2] * mix) >> 8;
        const uint16_t *base = wayfarer_sky_palettes + level * 256;
        for (unsigned i = 0; i < 256; ++i) {
            unsigned rr = (base[i] >> 11) * r >> 7;
            unsigned gg = ((base[i] >> 5) & 63) * g >> 7;
            unsigned bb = (base[i] & 31) * b >> 7;
            colors[i] = (uint16_t)((rr > 31 ? 31 : rr) << 11 | (gg > 63 ? 63 : gg) << 5 | (bb > 31 ? 31 : bb));
        }
        ready = true; cached_time = now_ms; cached_seed = scene->seed; cached_travel = scene->travel_q8;
        cached_event = scene->event; cached_variant = scene->variant;
        cached_start = scene->event_started_ms; cached_duration = scene->event_duration_ms;
    }
    int drift = (int)(travel / 320);
    int bank = unit_y[(now_ms / 240) & 63];
    memset(pixels, 0, (size_t)rows * WAYFARER_WIDTH * sizeof(uint16_t));
    for (int y = y0; y < y0 + rows; ++y) {
        int sy = (y / 2 + (int)(travel / 1800)) & (WAYFARER_SKY_HEIGHT - 1);
        const uint8_t *sky = wayfarer_sky + sy * WAYFARER_SKY_WIDTH;
        int offset = drift + bank * (y - 90) / 1024;
        uint16_t *out = pixels + (y - y0) * WAYFARER_WIDTH;
        for (unsigned span = wayfarer_span_rows[y]; span < wayfarer_span_rows[y + 1]; ++span) {
            for (int x = wayfarer_window_spans[span].left; x < wayfarer_window_spans[span].right; ++x) {
                out[x] = colors[sky[(x + offset) & (WAYFARER_SKY_WIDTH - 1)]];
            }
        }
    }
    int beacon_x = 374 + unit_x[(travel / 1200) & 63] * 70 / 64;
    uint16_t light = sky_sector(scene) == 3 ? rgb565(252, 201, 144) : rgb565(168, 204, 247);
    draw_space_glow(beacon_x, 49, 24, light, 20, y0, rows, pixels);
    fill_rect(pixels, y0, rows, beacon_x, 49, 1, 1, rgb565(171, 190, 209));
}

typedef struct { int16_t x, y, tx, ty; uint16_t color; uint8_t alpha, sparkle; } star_point_t;
static star_point_t star_points[180];

static void draw_stars(const wayfarer_scene_t *scene, uint32_t now_ms, int y0, int rows,
                       uint16_t *pixels)
{
    static bool ready;
    static uint32_t cached_time, cached_seed;
    static uint64_t cached_travel;
    static unsigned cached_speed;
    if (!ready || cached_time != now_ms || cached_seed != scene->seed || cached_travel != scene->travel_q8 || cached_speed != scene->speed_q8) {
        uint32_t travel = (uint32_t)(scene->travel_q8 >> 8);
        int bank = unit_x[(now_ms / 240) & 63] * 3 / 64;
        const uint16_t tones[] = {rgb565(184,211,252), rgb565(238,234,210), rgb565(151,212,232), rgb565(246,192,161)};
        for (uint32_t i = 0; i < 180; ++i) {
            uint32_t seed = hash32(scene->seed + i * 0x9e3779b9U + 91U);
            star_point_t *p = &star_points[i];
            p->color = tones[(seed >> 3) & 3];
            if (i < 60) {
                p->x = (int16_t)((seed + travel / 900) % 620 - 42);
                p->y = (int16_t)((seed >> 12) % 177 + bank);
                p->tx = p->x; p->ty = p->y;
                p->alpha = (uint8_t)(44 + (unit_y[(now_ms / 110 + i * 13) & 63] + 64) / 3);
                p->sparkle = (seed & 31) == 0;
                continue;
            }
            int phase = (int)((travel / (i < 152 ? 18 + seed % 30 : 10 + seed % 12) + (seed >> 10)) & 1023);
            int radius = 2 + phase * phase / 3700;
            int vx = (int)((seed >> 6) & 1023) - 512;
            int vy = (int)((seed >> 17) & 511) - 256;
            int cx = scene->focus_x + bank, cy = scene->focus_y + bank / 2;
            p->x = (int16_t)(cx + vx * radius / 256);
            p->y = (int16_t)(cy + vy * radius / 256);
            int trail = (i < 152 ? 2 : 5) + (scene->speed_q8 - 256) / 17;
            int tail = phase > trail ? phase - trail : 0;
            int inner = 2 + tail * tail / 3700;
            p->tx = (int16_t)(cx + vx * inner / 256); p->ty = (int16_t)(cy + vy * inner / 256);
            unsigned alpha = 22U + (unsigned)phase * 180U / 1023U;
            if (phase < 70) { alpha = alpha * (unsigned)phase / 70U; }
            p->alpha = (uint8_t)alpha;
            p->sparkle = phase > 780 && (seed & 15) == 0;
        }
        ready = true; cached_time = now_ms; cached_seed = scene->seed; cached_travel = scene->travel_q8; cached_speed = scene->speed_q8;
    }
    for (unsigned i = 0; i < 180; ++i) {
        const star_point_t *p = &star_points[i];
        int low = p->y < p->ty ? p->y : p->ty;
        int high = p->y > p->ty ? p->y : p->ty;
        if (high + 3 < y0 || low - 3 >= y0 + rows) { continue; }
        if (i >= 60) {
            draw_line(pixels, y0, rows, p->tx, p->ty, p->x, p->y, p->color, p->alpha / 3U);
        }
        blend_pixel(pixels, y0, rows, p->x, p->y, p->color, p->alpha);
        if (p->sparkle) {
            draw_line(pixels, y0, rows, p->x - 2, p->y, p->x + 2, p->y, p->color, p->alpha / 2U);
            draw_line(pixels, y0, rows, p->x, p->y - 2, p->x, p->y + 2, p->color, p->alpha / 2U);
        }
    }
}

static void draw_debris(const wayfarer_scene_t *scene, uint32_t now_ms, int y0, int rows,
                        uint16_t *pixels)
{
    (void)now_ms;
    uint32_t travel = (uint32_t)(scene->travel_q8 >> 8);
    for (uint32_t i = 0; i < 5U; ++i) {
        uint32_t seed = hash32(scene->seed + i * 3907U);
        int phase = (int)((travel / (25 + seed % 13) + (seed >> 11)) & 1023);
        int radius = 28 + phase * phase / 3800;
        int angle = (int)(seed & 63);
        int width = 2 + phase * phase / 75000;
        int x = scene->focus_x + unit_x[angle] * radius * 2 / 64 - width / 2;
        int y = scene->focus_y + unit_y[angle] * radius / 64;
        draw_sprite(&wayfarer_asteroid, x, y, width, (seed & 64) != 0, 80 + (unsigned)phase / 6, y0,
                    rows, pixels);
    }

}

static void draw_comet_body(const wayfarer_body_t *b,const wayfarer_projection_t *p,
                            uint32_t now_ms,int y0,int rows,uint16_t *pixels)
{
    int c=unit_x[b->angle], s=unit_y[b->angle], size=p->width;
    if (size<2) { size=2; } if (size>18) { size=18; }
    int tail=60+size*8;
    uint16_t mist=rgb565(67,94,138), ice=rgb565(95,151,175);
    for (int stream=-3;stream<=3;++stream) {
        for (int segment=0;segment<10;++segment) {
            int d0=tail*segment/10,d1=tail*(segment+1)/10;
            int v0=stream*d0/55+d0*d0/(tail*30);
            int v1=stream*d1/55+d1*d1/(tail*30);
            v0+=unit_y[(now_ms/160+segment*3+stream*7)&63]/32;
            v1+=unit_y[(now_ms/160+(segment+1)*3+stream*7)&63]/32;
            draw_line(pixels,y0,rows,p->x-c*d0/64-s*v0/64,p->y-s*d0/64+c*v0/64,
                      p->x-c*d1/64-s*v1/64,p->y-s*d1/64+c*v1/64,
                      stream==0?ice:mist,(unsigned)(10-segment)*(40U-(unsigned)abs(stream)*7U)*p->light/2550);
        }
    }
    for (unsigned i=0;i<20;++i) {
        uint32_t grain=hash32(i*7919U+1123U);
        int d=8+(int)((grain+now_ms/45)%(unsigned)tail);
        int v=((int)(grain%13)-6)*d/tail+d*d/(tail*30);
        blend_pixel(pixels,y0,rows,p->x-c*d/64-s*v/64,p->y-s*d/64+c*v/64,
                    (grain&1)?mist:rgb565(131,113,86),p->light/5);
    }
    draw_space_glow(p->x,p->y,18+size,ice,p->light/5,y0,rows,pixels);
    draw_sprite(&wayfarer_asteroid,p->x-size/2,p->y-size/2,size,false,p->light,y0,rows,pixels);
    blend_pixel(pixels,y0,rows,p->x+c*size/(64*3),p->y+s*size/(64*3),rgb565(172,197,204),p->light);
}

static void draw_bodies(const wayfarer_scene_t *scene,uint32_t now_ms,int y0,int rows,uint16_t *pixels)
{
    wayfarer_projection_t projected[WAYFARER_BODY_CAPACITY];
    unsigned order[WAYFARER_BODY_CAPACITY],count=0;
    for (unsigned i=0;i<WAYFARER_BODY_CAPACITY;++i) {
        projected[i]=wayfarer_scene_project_body(scene,&scene->bodies[i],now_ms);
        if (!projected[i].visible) { continue; }
        unsigned at=count;
        while (at && projected[order[at-1]].z<projected[i].z) { order[at]=order[at-1]; --at; }
        order[at]=i; ++count;
    }
    /* Far objects are actually occluded by nearer worlds, hulls and stations. */
    for (unsigned n=0;n<count;++n) {
        unsigned i=order[n];
        const wayfarer_body_t *b=&scene->bodies[i];
        const wayfarer_projection_t *p=&projected[i];
        if (b->type==WAYFARER_BODY_WORLD) {
            const wayfarer_indexed_t *art=&wayfarer_worlds[b->model%6];
            int h=art->height*p->width/art->width;
            draw_indexed_sprite(art,p->x-p->width/2,p->y-h/2,p->width,false,255,p->light,y0,rows,pixels);
        } else if (b->type==WAYFARER_BODY_SHIP) {
            draw_rotated_ship(b,p,now_ms,y0,rows,pixels);
        } else if (b->type==WAYFARER_BODY_STATION) {
            const wayfarer_indexed_t *art=&wayfarer_station;
            int h=art->height*p->width/art->width;
            draw_indexed_sprite(art,p->x-p->width/2,p->y-h/2,p->width,false,255,p->light,y0,rows,pixels);
            int angle=(int)((now_ms/240)&63), radius=p->width/2;
            wayfarer_projection_t drone=*p;
            drone.x+=unit_x[angle]*radius/64; drone.y+=unit_y[angle]*radius/160;
            drone.width=p->width/7;
            wayfarer_body_t tug=*b; tug.model=4; tug.angle=(uint8_t)((angle+16)&63);
            draw_rotated_ship(&tug,&drone,now_ms,y0,rows,pixels);
            if ((now_ms/550)&1) { blend_pixel(pixels,y0,rows,p->x,p->y-h/2+2,rgb565(177,134,88),p->light); }
        } else if (b->type==WAYFARER_BODY_COMET) {
            draw_comet_body(b,p,now_ms,y0,rows,pixels);
        } else if (b->type==WAYFARER_BODY_ECLIPSE) {
            draw_eclipse(scene,b,p,now_ms,y0,rows,pixels);
        } else if (b->type==WAYFARER_BODY_ASTEROID) {
            draw_sprite(&wayfarer_asteroid,p->x-p->width/2,p->y-p->width/2,p->width,false,p->light,y0,rows,pixels);
        }
    }
}

/* Interpolate the small sine table so illumination moves between table entries. */
static int light_wave(uint32_t now_ms, unsigned period_ms, unsigned offset)
{
    unsigned position = (now_ms % period_ms) * 64U;
    unsigned phase = (position / period_ms + offset) & 63;
    unsigned fraction = position % period_ms;
    int a = unit_y[phase], b = unit_y[(phase + 1) & 63];
    return a + (b - a) * (int)fraction / (int)period_ms;
}

/* Small internal lookup tables preserve material colors without repeating
 * saturating channel arithmetic for every logical cockpit pixel. */
static uint8_t shade5[64][32], shade6[64][64];
static bool shade_tables_ready;
static uint8_t exterior_gain[230][3], fixture_gain[2][118][3];
static uint32_t gain_time_ms;
static uint8_t gain_event, gain_region;
static unsigned gain_power, gain_red, gain_green, gain_blue;
static bool gains_ready;

static void prepare_shade_tables(void)
{
    if (shade_tables_ready) {
        return;
    }
    for (unsigned gain = 0; gain < 64; ++gain) {
        for (unsigned color = 0; color < 64; ++color) {
            unsigned value = color * gain / 32U;
            shade6[gain][color] = (uint8_t)(value > 63 ? 63 : value);
            if (color < 32) {
                shade5[gain][color] = (uint8_t)(value > 31 ? 31 : value);
            }
        }
    }
    shade_tables_ready = true;
}

static void draw_cabin_lighting(const wayfarer_scene_t *scene, const uint16_t *background,
                                uint32_t now_ms, int y0, int rows, uint16_t *pixels)
{
    prepare_shade_tables();
    uint8_t red_field[WAYFARER_WIDTH / 4];
    uint8_t green_field[WAYFARER_WIDTH / 4];
    uint8_t blue_field[WAYFARER_WIDTH / 4];
    bool alert = scene->event == WAYFARER_EVENT_ALERT;
    static const unsigned region_light[5][3] = {
        {104,191,249},{91,218,225},{183,139,255},{245,185,120},{113,218,172}
    };
    unsigned region = sky_sector(scene);
    if (scene->event == WAYFARER_EVENT_NEBULA) { region = (region + scene->variant % 5) % 5; }
    unsigned cool_r = alert ? 255U : region_light[region][0];
    unsigned cool_g = alert ? 75U : region_light[region][1];
    unsigned cool_b = alert ? 45U : region_light[region][2];
    int cool_x = 268 + light_wave(now_ms, 11520, 16) * 192 / 64;
    int cool_y = 124 + light_wave(now_ms, 11520, 11) * 24 / 64;
    int shadow_skew = light_wave(now_ms, 11520, 16) * 72 / 64;
    unsigned exterior_power = 135U + (unsigned)(light_wave(now_ms, 7680, 0) + 64) / 5U;
    for (unsigned i=0;!alert && i<WAYFARER_BODY_CAPACITY;++i) {
        const wayfarer_body_t *b=&scene->bodies[i];
        if (b->type!=WAYFARER_BODY_ECLIPSE) { continue; }
        wayfarer_projection_t p=wayfarer_scene_project_body(scene,b,now_ms);
        if (!p.visible) { continue; }
        int phase=b->phase_active?(int)((uint64_t)(now_ms-b->phase_started_ms)*1024/b->phase_duration_ms):0;
        int separation=abs(phase-512);
        unsigned sunshine=separation<80?8U:separation>180?255U:8U+(unsigned)(separation-80)*247/100;
        cool_r=244; cool_g=190; cool_b=145;
        cool_x=p.x; cool_y=p.y;
        exterior_power=exterior_power*sunshine/255*p.light/255;
        break;
    }
    unsigned left_power = 105U + (unsigned)(light_wave(now_ms, 9600, 0) + 64) / 4U;
    unsigned right_power = 105U + (unsigned)(light_wave(now_ms, 9600, 25) + 64) / 4U;
    unsigned warm_g = alert ? 76U : 171U;
    unsigned warm_b = alert ? 34U : 83U;

    /* Cache distance falloff and emitter tint once per frame. The tile loop
     * only fetches these contributions, rather than dividing at every surface. */
    if (!gains_ready || gain_time_ms != now_ms || gain_event != scene->event || gain_region != region ||
        gain_power!=exterior_power || gain_red!=cool_r || gain_green!=cool_g || gain_blue!=cool_b) {
        for (unsigned distance = 0; distance < 230; ++distance) {
            unsigned energy = exterior_power * (230U - distance) / 230U;
            exterior_gain[distance][0] = (uint8_t)(cool_r * energy >> 8);
            exterior_gain[distance][1] = (uint8_t)(cool_g * energy >> 8);
            exterior_gain[distance][2] = (uint8_t)(cool_b * energy >> 8);
        }
        for (unsigned side = 0; side < 2; ++side) {
            unsigned power = side ? right_power : left_power;
            for (unsigned distance = 0; distance < 118; ++distance) {
                unsigned energy = power * (118U - distance) / 118U;
                fixture_gain[side][distance][0] = (uint8_t)energy;
                fixture_gain[side][distance][1] = (uint8_t)(warm_g * energy >> 8);
                fixture_gain[side][distance][2] = (uint8_t)(warm_b * energy >> 8);
            }
        }
        gains_ready = true;
        gain_time_ms = now_ms;
        gain_event = scene->event;
        gain_region = (uint8_t)region;
        gain_power=exterior_power; gain_red=cool_r; gain_green=cool_g; gain_blue=cool_b;
    }

    /* Smooth light fields share coefficients over 4x4 tiles; material remains
     * at its original 2x2 resolution. Global tile origins keep DMA strips identical. */
    for (int y = y0 & ~3; y < y0 + rows; y += 4) {
        int cool_vertical = abs(y - cool_y) * 2;
        int lamp_y = y < 100 ? 24 : 170;
        int lamp_vertical = abs(y - lamp_y) * 2;
        int shadow_left = 184 + (y - 160) * shadow_skew / 80;
        int shadow_right = 350 + (y - 160) * shadow_skew / 80;
        const uint16_t *mask = background + y * WAYFARER_WIDTH;
        for (int x = 0; x < WAYFARER_WIDTH; x += 4) {
            const uint16_t *lower_mask = mask + 2 * WAYFARER_WIDTH;
            if (!mask[x] && !mask[x + 2] && !lower_mask[x] && !lower_mask[x + 2]) {
                continue;
            }
            int cool_distance = abs(x - cool_x) + cool_vertical;
            static const uint8_t unlit[3] = {0, 0, 0};
            const uint8_t *cool = cool_distance < 230 ? exterior_gain[cool_distance] : unlit;
            int lamp_x = y < 100 ? (x < 268 ? 42 : 494) : (x < 268 ? 134 : 402);
            int warm_distance = abs(x - lamp_x) + lamp_vertical;
            const uint8_t *warm = warm_distance < 118
                                      ? fixture_gain[x < 268 ? 0 : 1][warm_distance]
                                      : unlit;
            unsigned transmission = 128;
            unsigned shadow = 0;
            if (y >= 160) {
                int distance = abs(x - shadow_left);
                int right_distance = abs(x - shadow_right);
                if (right_distance < distance) {
                    distance = right_distance;
                }
                /* Soft penumbra behind the two canopy/console uprights. */
                if (distance < 18) {
                    shadow = (unsigned)(18 - distance) * 5U;
                    transmission -= (unsigned)(18 - distance) * 6U;
                }
            }
            unsigned screen = 0;
            if (y > 174 && x > 200 && x < 336) {
                int screen_distance = abs(x - 268) + abs(y - 210) * 2;
                if (screen_distance < 70) {
                    screen = (unsigned)(70 - screen_distance) * 43U >> 6;
                }
            }
            unsigned red_gain = 151U - shadow / 2U + (cool[0] * transmission >> 7) + warm[0];
            unsigned green_gain = 164U - shadow / 2U + (cool[1] * transmission >> 7) +
                                  warm[1] + screen;
            unsigned blue_gain = 185U - shadow / 2U + (cool[2] * transmission >> 7) +
                                 warm[2] + screen / 3U;
            red_field[x / 4] = (uint8_t)(red_gain >> 3);
            green_field[x / 4] = (uint8_t)(green_gain >> 3);
            blue_field[x / 4] = (uint8_t)(blue_gain >> 3);
        }
        for (int dy = 0; dy < 4; dy += 2) {
            int py = y + dy;
            if (py + 1 < y0 || py >= y0 + rows) {
                continue;
            }
            const uint16_t *source = background + py * WAYFARER_WIDTH;
            bool top_visible = py >= y0;
            bool below_visible = py + 1 < y0 + rows;
            uint16_t *top = top_visible ? pixels + (py - y0) * WAYFARER_WIDTH : NULL;
            uint16_t *below = below_visible ? pixels + (py + 1 - y0) * WAYFARER_WIDTH : NULL;
            for (int x = 0; x < WAYFARER_WIDTH; x += 2) {
                uint16_t material = source[x];
                if (!material) {
                    continue;
                }
                unsigned i = (unsigned)x / 4;
                uint16_t lit = (uint16_t)(shade5[red_field[i]][material >> 11] << 11 |
                                         shade6[green_field[i]][(material >> 5) & 63] << 5 |
                                         shade5[blue_field[i]][material & 31]);
                if (top_visible) {
                    top[x] = lit;
                    top[x + 1] = lit;
                }
                if (below_visible) {
                    below[x] = lit;
                    below[x + 1] = lit;
                }
            }
        }
    }
}

static void draw_light_source(uint16_t *pixels, uint32_t now_ms, int y0, int rows,
                              int cx, int cy, unsigned phase, bool alert, int slope)
{
    unsigned power = 38U + (unsigned)(light_wave(now_ms, alert ? 1800U : 9600U, phase) + 64) / 4U;
    unsigned tint_r = 31U, tint_g = alert ? 21U : 47U, tint_b = alert ? 3U : 16U;
    for (int y = (y0 & ~1); y < y0 + rows; y += 2) {
        int vertical = abs(y - cy) * 2;
        if (vertical >= 24) {
            continue;
        }
        for (int x = cx - 24; x <= cx + 24; x += 2) {
            int distance = abs(x - cx) + vertical;
            if (distance >= 24 || x < 0 || x + 1 >= WAYFARER_WIDTH) {
                continue;
            }
            unsigned energy = power * (unsigned)(24 - distance) / 24U;
            for (int dy = 0; dy < 2; ++dy) {
                int py = y + dy;
                if (py < y0 || py >= y0 + rows) {
                    continue;
                }
                for (int dx = 0; dx < 2; ++dx) {
                    uint16_t *dst = pixels + (py - y0) * WAYFARER_WIDTH + x + dx;
                    unsigned r = (*dst >> 11) + (tint_r * energy >> 8);
                    unsigned g = ((*dst >> 5) & 63) + (tint_g * energy >> 8);
                    unsigned b = (*dst & 31) + (tint_b * energy >> 8);
                    *dst = (uint16_t)((r > 31 ? 31 : r) << 11 |
                                      (g > 63 ? 63 : g) << 5 | (b > 31 ? 31 : b));
                }
            }
        }
    }
    uint16_t rim = alert ? rgb565(255, 102, 45) : rgb565(255, 189, 100);
    uint16_t core = alert ? rgb565(255, 181, 109) : rgb565(255, 239, 188);
    for (int dx = -10; dx < 10; dx += 2) {
        int py = cy - 2 + dx * slope / 2;
        fill_rect(pixels, y0, rows, cx + dx, py, 2, 4, rim);
        if (dx > -10 && dx < 8) {
            fill_rect(pixels, y0, rows, cx + dx, py, 2, 2, core);
        }
    }
}

static void draw_cockpit_lamps(const wayfarer_scene_t *scene, uint32_t now_ms,
                             int y0, int rows, uint16_t *pixels)
{
    bool alert = scene->event == WAYFARER_EVENT_ALERT;
    uint16_t lamp = alert ? rgb565(255, 109, 57) : rgb565(255, 213, 136);
    unsigned power = 100U + (unsigned)(light_wave(now_ms, alert ? 1800U : 9600U, 0) + 64) / 2U;
    for (int y = y0; y < y0 + rows; ++y) {
        for (unsigned i = wayfarer_lamp_rows[y]; i < wayfarer_lamp_rows[y + 1]; ++i) {
            blend_pixel(pixels, y0, rows, wayfarer_lamp_x[i], y, lamp, power);
        }
    }
    draw_light_source(pixels, now_ms, y0, rows, 134, 170, 0, alert, 0);
    draw_light_source(pixels, now_ms, y0, rows, 402, 170, 25, alert, 0);
    draw_light_source(pixels, now_ms, y0, rows, 42, 24, 0, alert, -1);
    draw_light_source(pixels, now_ms, y0, rows, 494, 24, 25, alert, 1);
}

static void draw_gauge(const wayfarer_scene_t *scene, uint32_t now_ms, int y0, int rows,
                       uint16_t *pixels)
{
    uint16_t amber = rgb565(226, 167, 75);
    int value = 37 + unit_y[(now_ms / 115) & 63] * 4 / 64;
    if (scene->event == WAYFARER_EVENT_WARP) {
        value += scene->speed_q8 / 110;
    }
    draw_circle(pixels, y0, rows, 148, 201, 10, amber, 65);
    for (int tail = 0; tail < 3; ++tail) {
        int phase = (value - tail) & 63;
        draw_line(pixels, y0, rows, 148, 201, 148 + unit_x[phase] * 9 / 64,
                  201 + unit_y[phase] * 9 / 64, amber, tail ? 28U : 240U);
    }
    fill_rect(pixels, y0, rows, 147, 200, 2, 2, rgb565(247, 207, 125));
    /* Two vertical engine-load banks breathe with the throttle. */
    for (int bank = 0; bank < 2; ++bank) {
        int x = bank ? 354 : 181;
        int power = 8 + scene->speed_q8 / 190 + unit_y[(now_ms / 95 + bank * 9) & 63] / 24;
        for (int i = 0; i < 12; ++i) {
            fill_rect_alpha(pixels, y0, rows, x, 183 + i * 2, 3, 1,
                            i >= 12 - power ? amber : rgb565(32, 26, 15),
                            i >= 12 - power ? 175 : 190);
        }
    }
}

static const char *event_label(const wayfarer_scene_t *scene)
{
    switch (scene->event) {
    case WAYFARER_EVENT_TRAFFIC:
        return "TRAFFIC";
    case WAYFARER_EVENT_ALERT:
        return "CAUTION";
    case WAYFARER_EVENT_PLANET:
        return "FLYBY";
    case WAYFARER_EVENT_WARP:
        return "JUMP";
    case WAYFARER_EVENT_NEBULA:
        return "NEBULA";
    case WAYFARER_EVENT_CONTACT:
        return "CONTACT";
    case WAYFARER_EVENT_STATION:
        return "ORBITAL";
    case WAYFARER_EVENT_CONVOY:
        return "CONVOY";
    case WAYFARER_EVENT_COMET:
        return "COMET";
    case WAYFARER_EVENT_ECLIPSE:
        return "ECLIPSE";
    default:
        return "CRUISE";
    }
}

static void draw_instruments(const wayfarer_scene_t *scene, uint32_t now_ms, int y0, int rows,
                             uint16_t *pixels)
{
    uint16_t phosphor = rgb565(156, 190, 114);
    uint16_t dim = rgb565(63, 86, 52);
    uint16_t amber = rgb565(228, 166, 77);
    bool alert = scene->event == WAYFARER_EVENT_ALERT;
    uint16_t ink = alert ? amber : phosphor;
    /* Faces sit inside the generated beveled housings. Every graph, warning,
     * scan and ping stays on physical green glass; the canopy is just a view. */
    fill_rect_alpha(pixels, y0, rows, 230, 194, 76, 34, rgb565(9, 23, 19), 200);
    draw_text(pixels, y0, rows, 232, 196, event_label(scene), ink, 1);
    char speed[4];
    unsigned throttle = 74 + scene->speed_q8 / 15;
    snprintf(speed, sizeof(speed), "%03u", throttle % 1000);
    draw_text(pixels, y0, rows, 285, 196, speed, ink, 1);
    for (int x = 232; x <= 301; x += 7) {
        draw_line(pixels, y0, rows, x, 206, x, 218, dim, 100);
    }
    draw_line(pixels, y0, rows, 232, 212, 301, 212, dim, 110);
    int prev_x = 232, prev_y = 214;
    for (int i = 1; i < 12; ++i) {
        int x = 232 + i * 6;
        int y = 212 + unit_y[(now_ms / 180 + i * 4) & 63] * 5 / 64;
        draw_line(pixels, y0, rows, prev_x, prev_y, x, y, ink, 150);
        prev_x = x;
        prev_y = y;
    }
    int cursor = 232 + (int)((now_ms / 55) % 68);
    fill_rect_alpha(pixels, y0, rows, cursor, 205, 1, 15, ink, 105);
    char distance[12];
    snprintf(distance, sizeof(distance), "LY %05u",
             (unsigned)((scene->travel_q8 >> 8) / 2000) % 100000);
    draw_text(pixels, y0, rows, 232, 221, distance, dim, 1);
    for (int i = 0; i < 4; ++i) {
        int height = 1 + (int)((now_ms / 500 + (unsigned)i * 3) % 5);
        fill_rect_alpha(pixels, y0, rows, 290 + i * 3, 226 - height, 2, height, ink, 190);
    }

    fill_rect_alpha(pixels, y0, rows, 377, 191, 50, 23, rgb565(9, 23, 19), 210);
    if (alert) {
        draw_text(pixels, y0, rows, 381, 193, "CAUTION", amber, 1);
        draw_text(pixels, y0, rows, 381, 203,
                  scene->variant % 3 == 0   ? "DEBRIS"
                  : scene->variant % 3 == 1 ? "POWER"
                                            : "SIGNAL",
                  phosphor, 1);
        if ((now_ms / 600) & 1) {
            draw_line(pixels, y0, rows, 379, 211, 425, 211, amber, 200);
        }
    } else {
        draw_text(pixels, y0, rows, 380, 193, scene->ping_active ? "PING" : "SCAN", dim, 1);
        int cx = 414, cy = 202;
        draw_circle(pixels, y0, rows, cx, cy, 9, phosphor, 80);
        draw_circle(pixels, y0, rows, cx, cy, 5, phosphor, 35);
        int angle = (int)((now_ms / 40) & 63);
        for (int trail = 0; trail < 6; ++trail) {
            int phase = (angle - trail) & 63;
            draw_line(pixels, y0, rows, cx, cy, cx + unit_x[phase] * 8 / 64,
                      cy + unit_y[phase] * 8 / 64, phosphor, (unsigned)(150 - trail * 23));
        }
        if (scene->event == WAYFARER_EVENT_TRAFFIC || scene->event == WAYFARER_EVENT_CONTACT ||
        scene->event == WAYFARER_EVENT_CONVOY) {
            uint32_t age = now_ms - scene->event_started_ms;
            int bx = cx - 6 + (int)(age * 12 / scene->event_duration_ms);
            fill_rect(pixels, y0, rows, bx, cy - 3, 2, 2, amber);
        }
        if (scene->ping_active) {
            int radius = 1 + (int)((now_ms - scene->ping_started_ms) / 180) % 9;
            draw_circle(pixels, y0, rows, cx, cy, radius, amber, 195);
        }
        int tx = 380, ty = 207;
        for (int i = 0; i < 5; ++i) {
            int nx = tx + 4;
            int ny = 207 + unit_y[(now_ms / 80 + i * 11) & 63] * 3 / 64;
            draw_line(pixels, y0, rows, tx, ty, nx, ny, phosphor, 150);
            tx = nx;
            ty = ny;
        }
    }
    /* Subtle phosphor scan and shadowed horizontal lines give the faces depth. */
    int sweep = 195 + (int)((now_ms / 70) % 32);
    fill_rect_alpha(pixels, y0, rows, 230, sweep, 76, 1, phosphor, 14);
    for (int y = 195; y < 228; y += 2) {
        fill_rect_alpha(pixels, y0, rows, 230, y, 76, 1, rgb565(0, 4, 1), 18);
    }
    int right_sweep = 192 + (int)((now_ms / 90) % 21);
    fill_rect_alpha(pixels, y0, rows, 377, right_sweep, 50, 1, phosphor, 16);
}

static void draw_switches(const wayfarer_scene_t *scene, uint32_t now_ms, int y0, int rows,
                          uint16_t *pixels)
{
    static const int16_t lamps[][2] = {
        {117, 172}, {123, 172}, {130, 172}, {137, 172}, {144, 172}, {150, 172}, {385, 172},
        {392, 172}, {399, 172}, {406, 172}, {413, 172}, {420, 172}, {111, 199}, {117, 199},
        {123, 199}, {121, 192}, {454, 195}, {464, 195}, {64, 195},  {83, 194},
    };
    uint16_t amber = rgb565(246, 172, 64);
    uint16_t green = rgb565(165, 188, 105);
    for (unsigned i = 0; i < sizeof(lamps) / sizeof(lamps[0]); ++i) {
        uint32_t phase = now_ms / (350 + i * 23) + hash32(i + scene->seed);
        bool on = phase % 9 < 6;
        int x = lamps[i][0], y = lamps[i][1];
        if (!on) {
            fill_rect_alpha(pixels, y0, rows, x, y, 2, 1, rgb565(22, 16, 7), 185);
        } else {
            blend_pixel(pixels, y0, rows, x, y, i % 7 == 0 ? green : amber, 165);
        }
    }
}

void wayfarer_scene_render_strip(const wayfarer_scene_t *scene, const uint16_t *background,
                                 uint32_t now_ms, int y0, int rows, uint16_t *pixels)
{
    if (!scene || !background || !pixels || y0 < 0 || rows <= 0 || y0 + rows > WAYFARER_HEIGHT) {
        return;
    }
    draw_space(scene, now_ms, y0, rows, pixels);
    draw_stars(scene, now_ms, y0, rows, pixels);
    draw_bodies(scene, now_ms, y0, rows, pixels);
    draw_debris(scene, now_ms, y0, rows, pixels);
    draw_cabin_lighting(scene, background, now_ms, y0, rows, pixels);
    draw_gauge(scene, now_ms, y0, rows, pixels);
    draw_instruments(scene, now_ms, y0, rows, pixels);
    draw_switches(scene, now_ms, y0, rows, pixels);
    draw_cockpit_lamps(scene, now_ms, y0, rows, pixels);
}
