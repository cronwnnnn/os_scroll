#ifndef DRIVER_VIDEO_H
#define DRIVER_VIDEO_H

#include "common/types.h"

#define VIDEO_MODE_TEXT 3
#define VIDEO_MODE_13H  0x13

#define VIDEO_WIDTH_13H  320
#define VIDEO_HEIGHT_13H 200

void video_set_mode(uint32_t mode);
void video_set_palette(uint32_t index, uint32_t r, uint32_t g, uint32_t b);
int32_t video_blit(uint8_t* buffer, uint32_t width, uint32_t height);

#endif
