#include <math.h>
#include <serpentis.h>
#include <stdio.h>
#include <termgfx.h>

#define WIDTH 256
#define HEIGHT 128
#define WAVE 64

static const unsigned char water[] = {41, 32, 33, 34, 35};
static unsigned char near_left[WIDTH * HEIGHT];
static unsigned char near_right[WIDTH * HEIGHT];
static int wave[WAVE];

static void setup(void)
{
    for (int i = 0; i < WAVE; i++)
        wave[i] = (int)(sin(i * 2 * M_PI / WAVE) * 100);
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            near_left[y * WIDTH + x] = (int)hypot(x - WIDTH / 3, y - HEIGHT / 2) & (WAVE - 1);
            near_right[y * WIDTH + x] = (int)hypot(x - 2 * WIDTH / 3, y - HEIGHT / 3) & (WAVE - 1);
        }
    }
}

static void draw(unsigned char *pixels, int t)
{
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        int sum = wave[(near_left[i] - t) & (WAVE - 1)] + wave[(near_right[i] - t) & (WAVE - 1)];
        pixels[i] = water[(sum + 200) * 4 / 400];
    }
}

int main(void)
{
    printf("two stones in a pond\n");
    unsigned char *pixels = tg_open(WIDTH, HEIGHT);
    if (!pixels) {
        fprintf(stderr, "ripple: no terminal to draw in\n");
        return 1;
    }
    setup();
    unsigned long start = serp_ticks();
    while (serp_ticks() - start < 400 && !serp_getkey(SERP_KEY_NOWAIT)) {
        draw(pixels, (int)(serp_ticks() - start) / 2);
        pixels = tg_frame();
        if (!pixels)
            break;
        serp_sleep(2);
    }
    tg_close();
    printf("done\n");
    return 0;
}
