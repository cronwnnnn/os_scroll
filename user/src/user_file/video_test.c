#include "common/types.h"
#include "driver/keyboard_read.h"
#include "user_sys/syscall.h"
#include "user_sys/user_io.h"

#define VIDEO_MODE_TEXT 3
#define VIDEO_MODE_13H  0x13
#define WIDTH  320
#define HEIGHT 200
#define BOX_SIZE 12

static uint8_t frame[WIDTH * HEIGHT];

static void build_palette(void) {
    syscall_video_set_palette(0, 0, 0, 0);
    syscall_video_set_palette(1, 8, 12, 20);
    syscall_video_set_palette(2, 4, 28, 32);
    syscall_video_set_palette(3, 63, 54, 20);
    syscall_video_set_palette(4, 63, 18, 18);
    syscall_video_set_palette(5, 18, 50, 63);
}

static void clear_frame(uint32_t tick) {
    for (uint32_t y = 0; y < HEIGHT; y++) {
        for (uint32_t x = 0; x < WIDTH; x++) {
            uint8_t shade = ((x / 16 + y / 12 + tick / 8) & 1) ? 1 : 2;
            frame[y * WIDTH + x] = shade;
        }
    }
}

static void draw_box(int32_t box_x, int32_t box_y, uint8_t color) {
    for (int32_t y = 0; y < BOX_SIZE; y++) {
        int32_t py = box_y + y;
        if (py < 0 || py >= HEIGHT) {
            continue;
        }
        for (int32_t x = 0; x < BOX_SIZE; x++) {
            int32_t px = box_x + x;
            if (px < 0 || px >= WIDTH) {
                continue;
            }
            frame[py * WIDTH + px] = color;
        }
    }
}

static void clamp_position(int32_t* x, int32_t* y) {
    if (*x < 0) {
        *x = 0;
    }
    if (*y < 0) {
        *y = 0;
    }
    if (*x > WIDTH - BOX_SIZE) {
        *x = WIDTH - BOX_SIZE;
    }
    if (*y > HEIGHT - BOX_SIZE) {
        *y = HEIGHT - BOX_SIZE;
    }
}

int main(int32_t argc, char** argv) {
    (void)argc;
    (void)argv;

    int32_t x = (WIDTH - BOX_SIZE) / 2;
    int32_t y = (HEIGHT - BOX_SIZE) / 2;
    bool running = true;

    syscall_video_set_mode(VIDEO_MODE_13H);
    build_palette();

    uint32_t last_tick = (uint32_t)syscall_get_ticks();
    while (running) {
        keyboard_event_t event;
        while (syscall_poll_keyboard_event(&event)) {
            if (event.is_make && event.key == KHE_ESCAPE) {
                running = false;
            }
        }

        uint32_t tick = (uint32_t)syscall_get_ticks();
        if (tick == last_tick) {
            continue;
        }
        last_tick = tick;

        int32_t speed = 3;
        if (syscall_get_key_state(KHE_ARROW_LEFT) || syscall_get_key_state('a')) {
            x -= speed;
        }
        if (syscall_get_key_state(KHE_ARROW_RIGHT) || syscall_get_key_state('d')) {
            x += speed;
        }
        if (syscall_get_key_state(KHE_ARROW_UP) || syscall_get_key_state('w')) {
            y -= speed;
        }
        if (syscall_get_key_state(KHE_ARROW_DOWN) || syscall_get_key_state('s')) {
            y += speed;
        }
        clamp_position(&x, &y);

        clear_frame(tick);
        draw_box(x, y, 3);
        draw_box(8, 8, 5);
        syscall_video_blit(frame, WIDTH, HEIGHT);
    }

    syscall_video_set_mode(VIDEO_MODE_TEXT);
    printf("video_test finished: keyboard event demo works\n");
    return 0;
}
