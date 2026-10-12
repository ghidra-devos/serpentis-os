#include <graphics.h>
#include <serpentis.h>

#define LIMIT 48

static const unsigned char shades[] = {
    GFX_BLUE, GFX_LIGHT_BLUE, GFX_CYAN, GFX_LIGHT_CYAN, GFX_GREEN, GFX_LIGHT_GREEN,
    GFX_YELLOW, GFX_BROWN, GFX_LIGHT_RED, GFX_RED, GFX_MAGENTA, GFX_LIGHT_MAGENTA
};

static double center_x = -0.6;
static double center_y = 0.0;
static double scale = 3.2;

static int escape_time(double cx, double cy)
{
    double x = 0, y = 0;
    for (int i = 0; i < LIMIT; i++) {
        double xx = x * x, yy = y * y;
        if (xx + yy > 4)
            return i;
        y = 2 * x * y + cy;
        x = xx - yy + cx;
    }
    return LIMIT;
}

static void render(void)
{
    int width = gfx_width(), height = gfx_height();
    double step = scale / width;
    double left = center_x - step * width / 2;
    double top = center_y - step * height / 2;
    for (int row = 0; row < height; row++) {
        for (int column = 0; column < width; column++) {
            int n = escape_time(left + column * step, top + row * step);
            gfx_pixel(column, row, n == LIMIT ? GFX_BLACK : shades[n % sizeof shades]);
        }
    }
    gfx_printf(4, 2, GFX_WHITE, "x %.5f  y %.5f  zoom %.1f", center_x, center_y, 3.2 / scale);
    gfx_present();
}

int main(void)
{
    if (gfx_open("Mandelbrot", 320, 240) < 0)
        return 1;
    render();
    struct gfx_event event;
    for (;;) {
        int type = gfx_wait(&event);
        if (type == GFX_CLOSE || (type == GFX_KEY && (event.key == 'q' || event.key == SERP_KEY_ESCAPE)))
            break;
        if (type == GFX_RESIZE) {
            render();
        } else if (type == GFX_MOUSE && (event.pressed & GFX_BUTTON_LEFT)) {
            double step = scale / gfx_width();
            center_x += (event.x - gfx_width() / 2) * step;
            center_y += (event.y - gfx_height() / 2) * step;
            scale /= 2;
            render();
        } else if (type == GFX_MOUSE && (event.pressed & GFX_BUTTON_RIGHT)) {
            scale *= 2;
            render();
        }
    }
    return 0;
}
