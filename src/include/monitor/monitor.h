#ifndef MONITOR_H
#define MONITOR_H

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

#define COLOR_BLACK 0
#define COLOR_BLUE 1
#define COLOR_GREEN 2
#define COLOR_CYAN 3
#define COLOR_RED 4
#define COLOR_FUCHSINE 5
#define COLOR_BROWN 6
#define COLOR_WHITE 7

// only fore colors have light version
#define COLOR_LIGHT_BLACK 8
#define COLOR_LIGHT_BLUE 9
#define COLOR_LIGHT_GREEN 10
#define COLOR_LIGHT_CYAN 11
#define COLOR_LIGHT_RED 12
#define COLOR_LIGHT_FUCHSINE 13
#define COLOR_LIGHT_BROWN 14
#define COLOR_LIGHT_WHITE 15





void monitor_print(const char* str);
void monitor_clear();

#endif