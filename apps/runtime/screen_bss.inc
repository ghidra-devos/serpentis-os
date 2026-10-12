align 16
screen_pixels rb SCREEN_PIXEL_W * SCREEN_PIXEL_H
screen_font rb SCREEN_FONT_BYTES
screen_event:
    rb EVENT_BYTES
