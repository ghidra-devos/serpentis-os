#include <serpentis.h>
#include <stdio.h>
#include <string.h>
#include <termgfx.h>

#define WAIT_TICKS 100

static unsigned char *frames;
static volatile uint32_t *shown;
static unsigned sequence;

static void send(const char *text)
{
    serp_write(1, text, strlen(text));
}

unsigned char *tg_open(int width, int height)
{
    char text[64];
    struct serp_event event;

    if (serp_isatty(1) <= 0)
        return 0;
    fflush(stdout);
    serp_shm_attach();
    snprintf(text, sizeof text, "\033_Go%d;%d;%d\033\\", serp_getpid(), width, height);
    send(text);

    unsigned long deadline = serp_ticks() + WAIT_TICKS;
    while (serp_ticks() < deadline) {
        if (serp_event(&event, SERP_EVENT_WANT_TICK) != SERP_EVENT_MESSAGE)
            continue;
        if (event.data.message.op == TERMGFX_REFUSED)
            return 0;
        if (event.data.message.op != TERMGFX_OPENED)
            continue;
        frames = (unsigned char *)(SERP_SHM_ADDRESS + (unsigned long)event.data.message.a);
        shown = (volatile uint32_t *)(SERP_SHM_ADDRESS + (unsigned long)event.data.message.b);
        sequence = 0;
        return frames;
    }
    return 0;
}

unsigned char *tg_frame(void)
{
    char text[32];

    if (!frames)
        return 0;
    sequence++;
    snprintf(text, sizeof text, "\033_Gf%u\033\\", sequence);
    send(text);
    unsigned long deadline = serp_ticks() + WAIT_TICKS;
    while (*shown != sequence) {
        if (serp_ticks() >= deadline)
            return 0;
        serp_yield();
    }
    return frames + (sequence & 1) * TERMGFX_FRAME_BYTES;
}

void tg_close(void)
{
    if (!frames)
        return;
    frames = 0;
    send("\033_Gc\033\\");
}
