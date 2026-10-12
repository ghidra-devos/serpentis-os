#include <graphics.h>
#include <serpentis.h>

#define BAR_HEIGHT 22
#define SWATCH 18

static int color = GFX_LIGHT_CYAN;
static int brush = 2;
static int last_x = -1;
static int last_y = -1;

static void draw_bar(void)
{
    gfx_fill_rect(0, 0, gfx_width(), BAR_HEIGHT, GFX_DARK_GRAY);
    for (int i = 0; i < 16; i++) {
        int x = 2 + i * (SWATCH + 2);
        gfx_fill_rect(x, 2, SWATCH, SWATCH, i);
        if (i == color)
            gfx_rect(x - 1, 1, SWATCH + 2, SWATCH + 2, GFX_WHITE);
    }
    gfx_printf(2 + 16 * (SWATCH + 2) + 6, 3, GFX_WHITE, "brush %d", brush);
}

static void stroke(int x, int y)
{
    if (y < BAR_HEIGHT + brush)
        y = BAR_HEIGHT + brush;
    if (last_x >= 0)
        for (int dy = -brush / 2; dy <= brush / 2; dy++)
            for (int dx = -brush / 2; dx <= brush / 2; dx++)
                gfx_line(last_x + dx, last_y + dy, x + dx, y + dy, color);
    gfx_fill_circle(x, y, brush / 2, color);
    last_x = x;
    last_y = y;
}

static void clear_canvas(void)
{
    gfx_clear(GFX_BLACK);
    gfx_text(10, BAR_HEIGHT + 8, "draw with the mouse - 1..9 brush size, c clears, q quits", GFX_DARK_GRAY,
             GFX_TRANSPARENT);
    draw_bar();
}

int main(void)
{
    if (gfx_open("Sketch", 420, 300) < 0)
        return 1;
    clear_canvas();
    gfx_present();
    struct gfx_event event;
    for (;;) {
        int type = gfx_wait(&event);
        if (type == GFX_CLOSE)
            break;
        if (type == GFX_KEY) {
            if (event.key == 'q' || event.key == SERP_KEY_ESCAPE)
                break;
            if (event.key == 'c')
                clear_canvas();
            if (event.key >= '1' && event.key <= '9') {
                brush = event.key - '0';
                draw_bar();
            }
        } else if (type == GFX_RESIZE) {
            clear_canvas();
        } else if (type == GFX_MOUSE) {
            if ((event.pressed & GFX_BUTTON_LEFT) && event.y < BAR_HEIGHT) {
                int index = (event.x - 2) / (SWATCH + 2);
                if (index >= 0 && index < 16) {
                    color = index;
                    draw_bar();
                }
            } else if ((event.buttons & GFX_BUTTON_LEFT) && event.y >= BAR_HEIGHT) {
                stroke(event.x, event.y);
            } else {
                last_x = -1;
            }
        }
        gfx_present();
    }
    return 0;
}
