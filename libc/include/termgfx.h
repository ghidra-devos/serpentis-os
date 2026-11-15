#ifndef TERMGFX_H
#define TERMGFX_H

/* pictures inside the terminal window. tg_open gives a width x height frame (one byte a
   pixel, width bytes a row) drawn under the text, tg_frame shows it and returns the next
   one to draw into. use palette 16-63, themes recolor 0-15. the last frame stays when the
   program exits. TERMGFX_CLEAR pixels let the terminal show through */

enum {
    TERMGFX_FRAME_BYTES = 0x10000,
    TERMGFX_CLEAR = 0xff,
    TERMGFX_OPENED = 300,
    TERMGFX_REFUSED = 301
};

unsigned char *tg_open(int width, int height);
unsigned char *tg_frame(void);
void tg_close(void);

#endif
