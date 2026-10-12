#ifndef GRAPHICS_H
#define GRAPHICS_H

enum {
    GFX_BLACK,
    GFX_BLUE,
    GFX_GREEN,
    GFX_CYAN,
    GFX_RED,
    GFX_MAGENTA,
    GFX_BROWN,
    GFX_LIGHT_GRAY,
    GFX_DARK_GRAY,
    GFX_LIGHT_BLUE,
    GFX_LIGHT_GREEN,
    GFX_LIGHT_CYAN,
    GFX_LIGHT_RED,
    GFX_LIGHT_MAGENTA,
    GFX_YELLOW,
    GFX_WHITE
};

enum {
    GFX_NONE,
    GFX_KEY,
    GFX_MOUSE,
    GFX_RESIZE,
    GFX_CLOSE,
    GFX_TICK
};

enum {
    GFX_BUTTON_LEFT = 1,
    GFX_BUTTON_RIGHT = 2,
    GFX_BUTTON_MIDDLE = 4
};

enum {
    GFX_CHAR_WIDTH = 8,
    GFX_CHAR_HEIGHT = 16,
    GFX_TRANSPARENT = -1
};

struct gfx_event {
    int type;
    int key;
    int x;
    int y;
    int buttons;
    int pressed;
    int released;
};

int gfx_open(const char *title, int width, int height);
void gfx_close(void);
int gfx_width(void);
int gfx_height(void);
int gfx_windowed(void);
unsigned char *gfx_buffer(void);
int gfx_stride(void);

void gfx_clear(int color);
void gfx_pixel(int x, int y, int color);
int gfx_read_pixel(int x, int y);
void gfx_fill_rect(int x, int y, int width, int height, int color);
void gfx_rect(int x, int y, int width, int height, int color);
void gfx_line(int x0, int y0, int x1, int y1, int color);
void gfx_circle(int cx, int cy, int radius, int color);
void gfx_fill_circle(int cx, int cy, int radius, int color);
void gfx_char(int x, int y, int c, int color, int background);
void gfx_text(int x, int y, const char *text, int color, int background);
int gfx_printf(int x, int y, int color, const char *format, ...);
void gfx_blit(int x, int y, int width, int height, const unsigned char *pixels, int transparent);
void gfx_present(void);

int gfx_wait(struct gfx_event *event);
int gfx_poll(struct gfx_event *event);
int gfx_wait_tick(struct gfx_event *event);
int gfx_set_color(int index, int red, int green, int blue);

#endif
