#include <graphics.h>
#include <serpentis.h>
#include <stdlib.h>

#define BALL_MAX 12

struct ball {
    int x;
    int y;
    int dx;
    int dy;
    int radius;
    int color;
};

static struct ball balls[BALL_MAX];
static int ball_count;

static void add_ball(void)
{
    if (ball_count == BALL_MAX)
        return;
    struct ball *b = &balls[ball_count++];
    b->radius = 6 + rand() % 12;
    b->x = (b->radius + rand() % (gfx_width() - 2 * b->radius)) * 16;
    b->y = (b->radius + rand() % (gfx_height() - 2 * b->radius)) * 16;
    b->dx = (rand() % 2 ? 1 : -1) * (20 + rand() % 40);
    b->dy = (rand() % 2 ? 1 : -1) * (20 + rand() % 40);
    b->color = 9 + rand() % 7;
}

static void step(struct ball *b)
{
    int left = b->radius * 16, right = (gfx_width() - b->radius) * 16;
    int top = b->radius * 16, bottom = (gfx_height() - b->radius) * 16;
    b->dy += 3;
    b->x += b->dx;
    b->y += b->dy;
    if (b->x < left) {
        b->x = left;
        b->dx = -b->dx;
    } else if (b->x > right) {
        b->x = right;
        b->dx = -b->dx;
    }
    if (b->y > bottom) {
        b->y = bottom;
        b->dy = -b->dy * 9 / 10;
    } else if (b->y < top) {
        b->y = top;
        b->dy = -b->dy;
    }
}

int main(void)
{
    if (gfx_open("Bounce", 360, 240) < 0)
        return 1;
    srand((unsigned)serp_ticks());
    for (int i = 0; i < 5; i++)
        add_ball();
    struct gfx_event event;
    unsigned long frames = 0;
    for (;;) {
        int type = gfx_wait_tick(&event);
        if (type == GFX_CLOSE)
            break;
        if (type == GFX_KEY) {
            if (event.key == 'q' || event.key == SERP_KEY_ESCAPE)
                break;
            if (event.key == ' ')
                add_ball();
            continue;
        }
        if (type == GFX_MOUSE && (event.pressed & GFX_BUTTON_LEFT) && ball_count < BALL_MAX) {
            add_ball();
            balls[ball_count - 1].x = event.x * 16;
            balls[ball_count - 1].y = event.y * 16;
            continue;
        }
        if (type != GFX_TICK && type != GFX_RESIZE)
            continue;
        frames++;
        gfx_clear(GFX_BLACK);
        for (int i = 0; i < ball_count; i++) {
            step(&balls[i]);
            gfx_fill_circle(balls[i].x / 16, balls[i].y / 16, balls[i].radius, balls[i].color);
        }
        gfx_printf(6, 4, GFX_DARK_GRAY, "%d balls  frame %lu  space adds one", ball_count, frames);
        gfx_present();
    }
    return 0;
}
