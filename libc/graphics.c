#include <graphics.h>
#include <serpentis.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GUI_CREATE 1
#define GUI_DAMAGE 2
#define GUI_DESTROY 3
#define GUI_RESIZED 4
#define GUI_CREATED 101
#define GUI_KEY 102
#define GUI_MOUSE 103
#define GUI_CLOSE 104
#define GUI_REFUSED 105
#define GUI_RESIZE 106
#define SURFACE_BYTES 0x54000
#define WINDOW_MAX 24
#define FONT_BYTES (256 * GFX_CHAR_HEIGHT)

static int server;
static int window;
static int windowed;
static int opened;
static int resize_pending;
static int width;
static int height;
static int stride;
static int buttons;
static unsigned char *pixels;
static const unsigned char *font;

static void send_request(int op, int a, int b)
{
    struct serp_message message;
    memset(&message, 0, sizeof message);
    message.op = op;
    message.window = window;
    message.a = a;
    message.b = b;
    serp_send(server, &message);
}

static int open_window(const char *title, int requested_width, int requested_height)
{
    struct serp_message message;
    memset(&message, 0, sizeof message);
    message.op = GUI_CREATE;
    message.a = requested_width;
    message.b = requested_height;
    strncpy(message.title, title, sizeof message.title - 1);
    int sent = serp_send(server, &message);
    for (int attempt = 0; sent < 0 && sent != SERP_ERR_NO_PROCESS && attempt < 200; attempt++) {
        serp_yield();
        sent = serp_send(server, &message);
    }
    if (sent < 0)
        return -1;
    for (;;) {
        struct serp_event event;
        if (serp_event(&event, 0) != SERP_EVENT_MESSAGE)
            continue;
        if (event.data.message.op == GUI_REFUSED)
            return -1;
        if (event.data.message.op != GUI_CREATED)
            continue;
        window = event.data.message.window;
        pixels = (unsigned char *)(SERP_SHM_ADDRESS + (unsigned long)event.data.message.a);
        return 0;
    }
}

static int open_console(void)
{
    unsigned char *glyphs = malloc(FONT_BYTES);
    pixels = malloc(SERP_SCREEN_WIDTH * SERP_SCREEN_HEIGHT);
    if (!glyphs || !pixels)
        return -1;
    serp_service(SERP_SERVICE_CLAIM, 0);
    serp_service(SERP_SERVICE_FONT, glyphs);
    font = glyphs;
    width = stride = SERP_SCREEN_WIDTH;
    height = SERP_SCREEN_HEIGHT;
    return 0;
}

int gfx_open(const char *title, int requested_width, int requested_height)
{
    if (opened)
        return 0;
    server = serp_service(SERP_SERVICE_QUERY, 0);
    if (server > 0) {
        serp_shm_attach();
        font = (const unsigned char *)(SERP_SHM_ADDRESS + (unsigned long)WINDOW_MAX * SURFACE_BYTES);
        if (open_window(title, requested_width, requested_height) < 0)
            return -1;
        windowed = 1;
        width = stride = requested_width;
        height = requested_height;
    } else if (open_console() < 0) {
        return -1;
    }
    opened = 1;
    atexit(gfx_close);
    gfx_clear(GFX_BLACK);
    return 0;
}

void gfx_close(void)
{
    if (!opened)
        return;
    opened = 0;
    if (windowed)
        send_request(GUI_DESTROY, 0, 0);
    else
        serp_con_restore();
}

int gfx_width(void)
{
    return width;
}

int gfx_height(void)
{
    return height;
}

int gfx_windowed(void)
{
    return windowed;
}

unsigned char *gfx_buffer(void)
{
    return pixels;
}

int gfx_stride(void)
{
    return stride;
}

void gfx_fill_rect(int x, int y, int w, int h, int color)
{
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > width)
        w = width - x;
    if (y + h > height)
        h = height - y;
    if (w <= 0 || h <= 0)
        return;
    unsigned char *row = pixels + (long)y * stride + x;
    while (h--) {
        memset(row, color, w);
        row += stride;
    }
}

void gfx_clear(int color)
{
    gfx_fill_rect(0, 0, width, height, color);
}

void gfx_pixel(int x, int y, int color)
{
    if (x >= 0 && y >= 0 && x < width && y < height)
        pixels[(long)y * stride + x] = (unsigned char)color;
}

int gfx_read_pixel(int x, int y)
{
    if (x < 0 || y < 0 || x >= width || y >= height)
        return -1;
    return pixels[(long)y * stride + x];
}

void gfx_rect(int x, int y, int w, int h, int color)
{
    if (w <= 0 || h <= 0)
        return;
    gfx_fill_rect(x, y, w, 1, color);
    gfx_fill_rect(x, y + h - 1, w, 1, color);
    gfx_fill_rect(x, y, 1, h, color);
    gfx_fill_rect(x + w - 1, y, 1, h, color);
}

void gfx_line(int x0, int y0, int x1, int y1, int color)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        gfx_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1)
            break;
        int twice = 2 * error;
        if (twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

void gfx_circle(int cx, int cy, int radius, int color)
{
    int x = radius, y = 0, error = 1 - radius;
    while (x >= y) {
        gfx_pixel(cx + x, cy + y, color);
        gfx_pixel(cx + y, cy + x, color);
        gfx_pixel(cx - y, cy + x, color);
        gfx_pixel(cx - x, cy + y, color);
        gfx_pixel(cx - x, cy - y, color);
        gfx_pixel(cx - y, cy - x, color);
        gfx_pixel(cx + y, cy - x, color);
        gfx_pixel(cx + x, cy - y, color);
        y++;
        if (error < 0) {
            error += 2 * y + 1;
        } else {
            x--;
            error += 2 * (y - x) + 1;
        }
    }
}

void gfx_fill_circle(int cx, int cy, int radius, int color)
{
    int x = radius, y = 0, error = 1 - radius;
    while (x >= y) {
        gfx_fill_rect(cx - x, cy + y, 2 * x + 1, 1, color);
        gfx_fill_rect(cx - x, cy - y, 2 * x + 1, 1, color);
        gfx_fill_rect(cx - y, cy + x, 2 * y + 1, 1, color);
        gfx_fill_rect(cx - y, cy - x, 2 * y + 1, 1, color);
        y++;
        if (error < 0) {
            error += 2 * y + 1;
        } else {
            x--;
            error += 2 * (y - x) + 1;
        }
    }
}

void gfx_char(int x, int y, int c, int color, int background)
{
    const unsigned char *glyph = font + (unsigned char)c * GFX_CHAR_HEIGHT;
    for (int row = 0; row < GFX_CHAR_HEIGHT; row++) {
        int bits = glyph[row];
        for (int column = 0; column < GFX_CHAR_WIDTH; column++) {
            if (bits & (0x80 >> column))
                gfx_pixel(x + column, y + row, color);
            else if (background >= 0)
                gfx_pixel(x + column, y + row, background);
        }
    }
}

void gfx_text(int x, int y, const char *text, int color, int background)
{
    int left = x;
    for (; *text; text++) {
        if (*text == '\n') {
            x = left;
            y += GFX_CHAR_HEIGHT;
            continue;
        }
        gfx_char(x, y, *text, color, background);
        x += GFX_CHAR_WIDTH;
    }
}

int gfx_printf(int x, int y, int color, const char *format, ...)
{
    char line[256];
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(line, sizeof line, format, arguments);
    va_end(arguments);
    gfx_text(x, y, line, color, GFX_TRANSPARENT);
    return length;
}

void gfx_blit(int x, int y, int w, int h, const unsigned char *source, int transparent)
{
    for (int row = 0; row < h; row++)
        for (int column = 0; column < w; column++) {
            int color = source[row * w + column];
            if (color != transparent)
                gfx_pixel(x + column, y + row, color);
        }
}

void gfx_present(void)
{
    if (!opened)
        return;
    if (!windowed) {
        serp_fb_blit(pixels, stride, 0, 0, width, height);
        return;
    }
    send_request(GUI_DAMAGE, 0, 0);
    if (resize_pending) {
        resize_pending = 0;
        send_request(GUI_RESIZED, width, height);
    }
}

static int mouse_event(struct gfx_event *event, int x, int y, int now)
{
    event->type = GFX_MOUSE;
    event->x = x;
    event->y = y;
    event->buttons = now;
    event->pressed = now & ~buttons;
    event->released = buttons & ~now;
    buttons = now;
    return GFX_MOUSE;
}

static int translate(struct serp_event *raw, struct gfx_event *event)
{
    memset(event, 0, sizeof *event);
    switch (raw->type) {
    case SERP_EVENT_KEY:
        event->type = GFX_KEY;
        event->key = raw->code;
        return GFX_KEY;
    case SERP_EVENT_MOUSE:
        return mouse_event(event, raw->x, raw->y, raw->data.buttons);
    case SERP_EVENT_TICK:
        event->type = GFX_TICK;
        return GFX_TICK;
    case SERP_EVENT_MESSAGE:
        break;
    default:
        return GFX_NONE;
    }
    struct serp_message *message = &raw->data.message;
    switch (message->op) {
    case GUI_KEY:
        event->type = GFX_KEY;
        event->key = message->a;
        return GFX_KEY;
    case GUI_MOUSE:
        return mouse_event(event, message->a, message->b, message->c);
    case GUI_RESIZE:
        width = stride = message->a;
        height = message->b;
        resize_pending = 1;
        event->type = GFX_RESIZE;
        event->x = width;
        event->y = height;
        return GFX_RESIZE;
    case GUI_CLOSE:
        event->type = GFX_CLOSE;
        return GFX_CLOSE;
    }
    return GFX_NONE;
}

static int next_event(struct gfx_event *event, int flags)
{
    for (;;) {
        struct serp_event raw;
        int kind = serp_event(&raw, flags);
        if (kind <= 0) {
            memset(event, 0, sizeof *event);
            if (flags & SERP_EVENT_NOWAIT)
                return GFX_NONE;
            continue;
        }
        int type = translate(&raw, event);
        if (type != GFX_NONE || (flags & SERP_EVENT_NOWAIT))
            return type;
    }
}

int gfx_wait(struct gfx_event *event)
{
    fflush(stdout);
    return next_event(event, 0);
}

int gfx_poll(struct gfx_event *event)
{
    return next_event(event, SERP_EVENT_NOWAIT);
}

int gfx_wait_tick(struct gfx_event *event)
{
    fflush(stdout);
    return next_event(event, SERP_EVENT_WANT_TICK);
}

int gfx_set_color(int index, int red, int green, int blue)
{
    unsigned char rgb[3];
    rgb[0] = (unsigned char)(red >> 2);
    rgb[1] = (unsigned char)(green >> 2);
    rgb[2] = (unsigned char)(blue >> 2);
    return serp_palette(index, 1, rgb);
}
