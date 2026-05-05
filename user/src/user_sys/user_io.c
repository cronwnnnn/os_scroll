#include "common/types.h"
#include "syscall/syscall.h"

void printf(char* str, ...);
char buf[500];
int32_t pos = 0;

void readline(char* str){
    int i = 0;
    while (1)
    {
        str[i] = syscall_read_char();
        printf("%c", str[i]);
        if(str[i] == '\n' || str[i] == '\r'){
            break;
        }   
        i++;
    }
    // 覆盖换行符，不将其写入str中
    str[i] = '\0';

}



void monitor_write_str(const char* str){
    int i = 0;
    while(str[i] != 0){
        buf[pos++] = str[i];
        i++;
    }
}


void monitor_write_dec(int32_t num){
    if(num == 0){
        buf[pos++] = '0';
        return;
    }
    uint32_t unum;
    if(num < 0){
        buf[pos++] = '-';
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
            buf[pos++] = itoc[i];
        }
    }
}

void monitor_write_udec(uint32_t num){
    if(num == 0){
        buf[pos++] = '0';
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
            buf[pos++] = itoc[i];
        }
    }
}

void monitor_write_hex(uint32_t num){
    if(num == 0){
        buf[pos++] = '0';
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
            buf[pos++] = itoc[i];
        }
    }
}


void format_to_buf(const char *format, char *arg_ptr){
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
                buf[pos++] = c;
            }
            else if(format[i] == '%'){
                buf[pos++] = '%';
            }
            else {
                buf[pos++] = '%';
                buf[pos++] = format[i];
            }
        }else {
            buf[pos++] = format[i];
        }
        i++;
    }

}


void printf(char* str, ...){
    char* arg_ptr = (char*)&str;
    arg_ptr += 4;
    format_to_buf(str, arg_ptr);
    if(pos >= 500){
        buf[499] = '\0';
        syscall_print("print contents are too long");
    }else{
        buf[pos] = '\0';
    }
    syscall_print(buf);
    pos = 0;
}