#include "common/io.h"
#include "common/types.h"
#include "monitor/monitor.h"

// 由于有scroll 因此不用做溢出检测，超过屏幕会自动滚动(即删除第一行的内容)


extern uint32_t get_esp();
void scroll_screen();
void monitor_print_char(char c);
void monitor_write_dec(int32_t num);
void monitor_write_udec(uint32_t num);
void monitor_write_hex(uint32_t num);
void monitor_write_str(const char* str);
void monitor_printf_args(const char *format, char *arg_ptr);

// use array to represent video memory, uint16_T 是为了方便当作数组来访问显存，因为一个字符占两个字节
uint16_t* video_memory = (uint16_t*)0xC00B8000;
// 0-3fore color, 4-7background color
uint16_t defult_color = (COLOR_BLACK << 4) | (COLOR_WHITE & 0x0F);
// Attention is CHAR not BYTE
static int16_t cursor_x ;
static int16_t cursor_y ;

static void mov_cursor(){
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;

    //cursor的位置占16bit
    //14表示接下来要输入cursor的高8位
    outb(0x3D4, 14);
    //输入高8位
    outb(0x3D5, pos >> 8);
    //15为cursor位置的低8位
    outb(0x3D4, 15);
    outb(0x3D5, pos);
}

static int16_t get_cursor_offset(){
    int16_t offset = cursor_y * VGA_WIDTH + cursor_x;
    return offset;
}

void monitor_print(const char* str){
    monitor_write_str(str);
}

void monitor_print_char(char c){
    if(c == 0x08 && cursor_x > 0){//back
        cursor_x--;
    }
    else if(c == 0x09){// Tab
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
    // 如果当前屏幕没满，不需要scroll
    if(cursor_y < VGA_HEIGHT){
        return;
    }
    int i;
    // 将每一行的内容向上移动一行，最后一行清空
    for(i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++){
        video_memory[i] = video_memory[i + VGA_WIDTH];
    }
    for(i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++){
        video_memory[i] = (defult_color << 8) | ' ';
    }
    //如果scroll则光标一定在最后一行的下一行的开头位置
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

void monitor_printf(const char *format, ...){
    char *arg = (char *)(&format);
    arg += 4;// skip format string
    monitor_printf_args(format, arg);
                
}

void monitor_write_str(const char* str){
    int i = 0;
    while(str[i] != 0){
        monitor_print_char(str[i]);
        i++;
    }
}


void monitor_write_dec(int32_t num){
    if(num == 0){
        monitor_print_char('0');
        return;
    }
    uint32_t unum;
    if(num < 0){
        monitor_print_char('-');
        unum = (uint32_t)(-num);
    } else {
        unum = num;
    }
    //
    char itoc[30] = {};
    int32_t i = 0;
    for(i = 0; unum && i < 30; i++){
        // 此处赋值的是ascii码，'0'的ascii码是48，因此需要加上'0',才是真正单个数字的字符
        itoc[i] = unum % 10 + '0'; 
        unum /= 10;
    }
    i--;
    for(; i >= 0; i--){
        if(itoc[i]){
            monitor_print_char(itoc[i]);
        }
    }
}

void monitor_write_udec(uint32_t num){
    if(num == 0){
        monitor_print_char('0');
        return;
    }
    
    char itoc[30] = {};
    int32_t i = 0;
    for(i = 0; num && i < 30; i++){
        itoc[i] = num % 10 + '0'; 
        num /= 10;
    }
    i--;
    for(; i >= 0; i--){
        if(itoc[i]){
            monitor_print_char(itoc[i]);
        }
    }
}

void monitor_write_hex(uint32_t num){
    if(num == 0){
        monitor_print_char('0');
        return;
    }
    
    char itoc[30] = {};
    int32_t i = 0;
    for(i = 0; num && i < 30; i++){
        uint32_t rem = num % 16;
        itoc[i] = (rem < 10) ? (rem + '0') : (rem - 10 + 'a');
        num /= 16;
    }
    i--;
    for(; i >= 0; i--){
        if(itoc[i]){
            monitor_print_char(itoc[i]);
        }
    }
}

void monitor_printf_args(const char *format, char *arg_ptr){
    int32_t i = 0;
    while(format[i] != 0){
        if(format[i] == '%'){
            i++;
            if(format[i] == 0){
                break;
            }
            if(format[i] == 'd'){
                int32_t num = *((int32_t*)arg_ptr);
                arg_ptr += 4;
                monitor_write_dec(num);
            }
            else if(format[i] == 'u'){
                uint32_t unum = *((uint32_t*)arg_ptr);
                arg_ptr += 4;
                monitor_write_udec(unum);
            }
            else if(format[i] == 'x' || format[i] == 'X'){
                uint32_t hexnum = *((uint32_t*)arg_ptr);
                arg_ptr += 4;
                monitor_write_hex(hexnum);
            }
            else if(format[i] == 's'){
                // arg_ptr是一个指向指针的指针，他作为指针指向了栈中字符串的指针，因此声明为char**，
                // 最后解引用一次获得字符串的指针，即str
                char* str = *((char**)arg_ptr);
                arg_ptr += 4;
                if (!str) {
                    str = "(null)";
                }
                monitor_write_str(str);
            }
            else if(format[i] == 'c'){
                char c = *((char*)arg_ptr);
                arg_ptr += 4;
                monitor_print_char(c);
            }
            else if(format[i] == '%'){
                monitor_print_char('%');
            }
            else {
                monitor_print_char('%');
                monitor_print_char(format[i]);
            }
        }else {
            monitor_print_char(format[i]);
        }
        i++;
    }

}