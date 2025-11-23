#include "common/io.h"
#include "common/types.h"
#include "monitor/monitor.h"

// 由于有scroll 因此不用做溢出检测，超过屏幕会自动滚动(即删除第一行的内容)


extern uint32_t get_esp();
void scroll_screen();
void monitor_print_char(char c);

// use array to represent video memory, uint16_T 是为了方便当作数组来访问显存，因为一个字符占两个字节
uint16_t* video_memory = (uint16_t*)0xC00B8000;
// 0-3fore color, 4-7background color
uint16_t defult_color = (COLOR_BLACK << 4) | (COLOR_WHITE & 0x0F);
// Attention is CHAR not BYTE
static int16_t cursor_x ;
static int16_t cursor_y ;

static void mov_cursor(){
    uint16_t pos = cursor_y * 80 + cursor_x;

    outb(0x3D4, 14);
    outb(0x3D5, pos >> 8);
    outb(0x3D4, 15);
    outb(0x3D5, pos);
}

static int16_t get_cursor_offset(){
    int16_t offset = cursor_y * VGA_WIDTH + cursor_x;
    return offset;
}

void monitor_print(const char* str){
    int i = 0;
    while(str[i] != 0){
        monitor_print_char(str[i]);
        i++;
    }
}

void monitor_print_char(char c){
    if(c == 0x08 && cursor_x > 0){
        cursor_x--;
    }
    else if(c == 0x09){
        cursor_x = (cursor_x + 8) & ~(8 - 1);
    }
    else if(c == '\n'){
        cursor_x = 0;
        cursor_y++;
    } 
    else if(c == '\r'){
        cursor_x = 0;
    } 
    else {
        int16_t offset = get_cursor_offset();
        video_memory[offset] = (defult_color << 8) | c;
        cursor_x++;
    }

    if(cursor_x >= VGA_WIDTH){
            cursor_x = 0;
            cursor_y++;
    }
    //scrolling
    scroll_screen();

    mov_cursor();
}

void scroll_screen(){
    if(cursor_y < VGA_HEIGHT){
        return;
    }
    int i;
    for(i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++){
        video_memory[i] = video_memory[i + VGA_WIDTH];
    }
    for(i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++){
        video_memory[i] = (defult_color << 8) | ' ';
    }
    cursor_y = VGA_HEIGHT - 1;
    cursor_x = 0;
    mov_cursor();
}

void monitor_clear(){
    const uint16_t blank = (defult_color << 8) | ' ';
    int16_t i;
    for(i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++){
        video_memory[i] = blank;
    }
    cursor_x = 0;
    cursor_y = 0;
    mov_cursor();
}