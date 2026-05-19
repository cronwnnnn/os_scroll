#include "common/types.h"
#include "user_sys/syscall.h"
#include "user_sys/user_io.h"

#define VIDEO_MODE_TEXT 3
#define VIDEO_MODE_13H  0x13
#define WIDTH  320
#define HEIGHT 200

static uint8_t frame[WIDTH * HEIGHT];

static void build_palette(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t r = i & 0x3F;
        uint32_t g = (i >> 2) & 0x3F;
        uint32_t b = (63 - i) & 0x3F;
        syscall_video_set_palette(i, r, g, b);
    }
}

static void build_frame(uint32_t tick) {
    for (uint32_t y = 0; y < HEIGHT; y++) {
        for (uint32_t x = 0; x < WIDTH; x++) {
            frame[y * WIDTH + x] = (uint8_t)((x + y + tick) & 0xFF);
        }
    }
}

int main(int32_t argc, char** argv) {
    (void)argc;
    (void)argv;

    syscall_video_set_mode(VIDEO_MODE_13H);
    build_palette();

    uint32_t start = (uint32_t)syscall_get_ticks();
    while ((uint32_t)syscall_get_ticks() - start < 150) {
        build_frame((uint32_t)syscall_get_ticks() * 3);
        syscall_video_blit(frame, WIDTH, HEIGHT);
    }

    syscall_video_set_mode(VIDEO_MODE_TEXT);
    printf("video_test finished: mode 13h blit works\n");
    return 0;
}
