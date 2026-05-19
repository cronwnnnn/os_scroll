#include "driver/video.h"
#include "common/io.h"
#include "common/stdlib.h"
#include "common/secure.h"
#include "monitor/monitor.h"

#define VGA_MISC_WRITE 0x3C2
#define VGA_SEQ_INDEX  0x3C4
#define VGA_SEQ_DATA   0x3C5
#define VGA_CRTC_INDEX 0x3D4
#define VGA_CRTC_DATA  0x3D5
#define VGA_GC_INDEX   0x3CE
#define VGA_GC_DATA    0x3CF
#define VGA_AC_INDEX   0x3C0
#define VGA_INSTAT     0x3DA
#define VGA_DAC_WRITE  0x3C8
#define VGA_DAC_DATA   0x3C9

#define VGA_MODE_REGS_SIZE (1 + 5 + 25 + 9 + 21)
#define VGA_MEMORY_13H ((uint8_t*)0xC00A0000)
#define VGA_FONT_BYTES (256 * 32)

static const uint8_t mode_13h_regs[VGA_MODE_REGS_SIZE] = {
    0x63,
    0x03, 0x01, 0x0F, 0x00, 0x0E,
    0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
    0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F, 0xFF,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x41, 0x00, 0x0F, 0x00, 0x00
};

static const uint8_t mode_text_regs[VGA_MODE_REGS_SIZE] = {
    0x67,
    0x03, 0x00, 0x03, 0x00, 0x02,
    0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F,
    0x00, 0x4F, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x00,
    0x9C, 0x0E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0xFF,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x0C, 0x00, 0x0F, 0x08, 0x00
};

static uint32_t current_mode = VIDEO_MODE_TEXT;
static uint8_t saved_text_font[VGA_FONT_BYTES];
static bool text_font_saved = false;

static const uint8_t text_palette[16][3] = {
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0x2A},
    {0x00, 0x2A, 0x00}, {0x00, 0x2A, 0x2A},
    {0x2A, 0x00, 0x00}, {0x2A, 0x00, 0x2A},
    {0x2A, 0x15, 0x00}, {0x2A, 0x2A, 0x2A},
    {0x15, 0x15, 0x15}, {0x15, 0x15, 0x3F},
    {0x15, 0x3F, 0x15}, {0x15, 0x3F, 0x3F},
    {0x3F, 0x15, 0x15}, {0x3F, 0x15, 0x3F},
    {0x3F, 0x3F, 0x15}, {0x3F, 0x3F, 0x3F}
};


static void select_font_plane(void) {
    outb(VGA_SEQ_INDEX, 0x02);
    outb(VGA_SEQ_DATA, 0x04);
    outb(VGA_SEQ_INDEX, 0x04);
    outb(VGA_SEQ_DATA, 0x06);

    outb(VGA_GC_INDEX, 0x04);
    outb(VGA_GC_DATA, 0x02);
    outb(VGA_GC_INDEX, 0x05);
    outb(VGA_GC_DATA, 0x00);
    outb(VGA_GC_INDEX, 0x06);
    outb(VGA_GC_DATA, 0x04);
}

static void save_text_font(void) {
    if (text_font_saved) {
        return;
    }

    select_font_plane();
    memcpy(saved_text_font, VGA_MEMORY_13H, VGA_FONT_BYTES);
    text_font_saved = true;
}

static void restore_text_font(void) {
    if (!text_font_saved) {
        return;
    }

    select_font_plane();
    memcpy(VGA_MEMORY_13H, saved_text_font, VGA_FONT_BYTES);
}

static void restore_text_palette(void) {
    for (uint32_t i = 0; i < 16; i++) {
        outb(VGA_DAC_WRITE, (uint8_t)i);
        outb(VGA_DAC_DATA, text_palette[i][0]);
        outb(VGA_DAC_DATA, text_palette[i][1]);
        outb(VGA_DAC_DATA, text_palette[i][2]);
    }
}

static void write_registers(const uint8_t* regs) {
    outb(VGA_MISC_WRITE, *regs);
    regs++;

    for (uint32_t i = 0; i < 5; i++) {
        outb(VGA_SEQ_INDEX, i);
        outb(VGA_SEQ_DATA, regs[i]);
    }
    regs += 5;

    outb(VGA_CRTC_INDEX, 0x03);
    outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) | 0x80);
    outb(VGA_CRTC_INDEX, 0x11);
    outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) & ~0x80);

    for (uint32_t i = 0; i < 25; i++) {
        uint8_t value = regs[i];
        if (i == 0x03) {
            value |= 0x80;
        } else if (i == 0x11) {
            value &= ~0x80;
        }
        outb(VGA_CRTC_INDEX, i);
        outb(VGA_CRTC_DATA, value);
    }
    regs += 25;

    for (uint32_t i = 0; i < 9; i++) {
        outb(VGA_GC_INDEX, i);
        outb(VGA_GC_DATA, regs[i]);
    }
    regs += 9;

    for (uint32_t i = 0; i < 21; i++) {
        (void)inb(VGA_INSTAT);
        outb(VGA_AC_INDEX, i);
        outb(VGA_AC_INDEX, regs[i]);
    }

    (void)inb(VGA_INSTAT);
    outb(VGA_AC_INDEX, 0x20);
}

void video_set_mode(uint32_t mode) {
    if (mode == VIDEO_MODE_13H) {
        if (current_mode == VIDEO_MODE_TEXT) {
            save_text_font();
        }
        write_registers(mode_13h_regs);
        memset(VGA_MEMORY_13H, 0, VIDEO_WIDTH_13H * VIDEO_HEIGHT_13H);
        current_mode = mode;
        return;
    }

    if (mode == VIDEO_MODE_TEXT) {
        write_registers(mode_text_regs);
        restore_text_font();
        write_registers(mode_text_regs);
        restore_text_palette();
        current_mode = mode;
        monitor_clear();
        return;
    }

    Panic("unsupported video mode %u", mode);
}

void video_set_palette(uint32_t index, uint32_t r, uint32_t g, uint32_t b) {
    if (index >= 256) {
        return;
    }
    outb(VGA_DAC_WRITE, (uint8_t)index);
    outb(VGA_DAC_DATA, (uint8_t)(r & 0x3F));
    outb(VGA_DAC_DATA, (uint8_t)(g & 0x3F));
    outb(VGA_DAC_DATA, (uint8_t)(b & 0x3F));
}

int32_t video_blit(uint8_t* buffer, uint32_t width, uint32_t height) {
    if (current_mode != VIDEO_MODE_13H) {
        return -1;
    }
    if (buffer == NULL || width != VIDEO_WIDTH_13H || height != VIDEO_HEIGHT_13H) {
        return -1;
    }

    memcpy(VGA_MEMORY_13H, buffer, VIDEO_WIDTH_13H * VIDEO_HEIGHT_13H);
    return 0;
}
