
#include "common/types.h"
#include "common/secure.h"

void memset(void* dest, uint8_t val, size_t len){
    Assert(dest != NULL);
    for(size_t i = 0; i < len; i++)
        *((uint8_t*)dest + i) = val;
}

void memcpy(void* dest, const void* src, size_t len){
    Assert(dest != NULL && src != NULL);
    for(size_t i = 0; i < len; i++)
        *((uint8_t*)dest + i) = *((uint8_t*)src + i);
}

int32_t strcpy(char* dest, const char* src){
    Assert(dest != NULL && src != NULL);

    int32_t i = 0;
    for(i = 0; src[i]; i++){
        dest[i] = src[i];
    }
    dest[i] = '\0';
    return i;
}

int32_t strcmp(const char* src1, const char* src2){
    //如果两者相等，返回0，最后一个字母src1 > src2返回1，否则返回-1
    Assert(src1 != NULL && src2 != NULL);
    int32_t i = 0;
    while(src1[i] != '\0'){
        if(src2[i] == '\0')
            return 1;
        if(src1[i] != src2[i])
            return src1[i] > src2[i] ? 1 : -1;
        i++;
    }
    return src2[i] == '\0' ? 0 : -1;
}

size_t strlen(const char* src){
    Assert(src != NULL);
    size_t num = 0;
    while(src[num]){
        num++;
    }
    return num;
}

int32_t int2str(char* dst, int32_t num) {
    Assert(dst != NULL);  
    uint32_t start = 0;
    uint32_t unum;
    
    // 对于极端情况 int的最小负数没有对应的正数，要用uint来表示
    if (num < 0) {
        dst[start++] = '-';
        unum = (uint32_t)(-num);
    } else {
        unum = num;
    }
    
    if (unum == 0) {
        dst[start++] = '0';
        dst[start] = '\0';
        return start;
    }

    char buf[20];
    buf[19] = '\0';
    uint32_t i = 18;
    while (unum > 0) {
        uint32_t remain = unum % 10;
        buf[i--] = '0' + remain;
        unum = unum / 10;
    }

    strcpy(dst + start, buf + i + 1);
    // 返回字符串长度
    return start + 18 - i;
}

int32_t uint2str(char* dst, uint32_t num) {
    Assert(dst != NULL);  
    uint32_t start = 0;
    if (num == 0) {
        dst[start++] = '0';
        dst[start] = '\0';
        return start;
    }

    char buf[20];
    buf[19] = '\0';
    uint32_t i = 18;
    while (num) {
        uint32_t remain = num % 10;
        buf[i--] = '0' + remain;
        num = num / 10;
    }

    strcpy(dst + start, buf + i + 1);
    // 返回字符串长度
    return start + 18 - i;
}

int32_t int2hex(char* dst, uint32_t num) {
    Assert(dst != NULL);  
    uint32_t start = 0;
    dst[start++] = '0';
    dst[start++] = 'x';

    if (num == 0) {
        dst[start++] = '0';
        dst[start] = '\0';
        return start;
    }

    char buf[20];
    buf[19] = '\0';
    uint32_t i = 18;
    while (num > 0) {
        uint32_t remain = num % 16;
        if (remain <= 9) {
        buf[i--] = '0' + remain;
        } else {
        buf[i--] = 'a' + (remain - 10);
        }
        num = num / 16;
    }

    strcpy(dst + start, buf + i + 1);
    return start + 18 - i;
}



void sprintf(char* dst, const char* format, ...){
    char *arg = (char *)(&format);
    arg += 4;// skip format string
    char buf[20] = {};
    int32_t len = sizeof(buf);
    int32_t i = 0;
    int32_t write_index = 0;
    while(format[i] != 0){
        memset(buf, 0, len);
        if(format[i] == '%'){
            i++;
            if(format[i] == 0){
                break;
            }
            if(format[i] == 'd'){
                int32_t num = *((int32_t*)arg);
                arg += 4;
                int2str(buf, num);
                write_index += strcpy(dst + write_index, buf);
            }
            else if(format[i] == 'u'){
                uint32_t unum = *((uint32_t*)arg);
                arg += 4;
                uint2str(buf, unum);
                write_index += strcpy(dst + write_index, buf);
            }
            else if(format[i] == 'x' || format[i] == 'X'){
                uint32_t hexnum = *((uint32_t*)arg);
                arg += 4;
                int2hex(buf, hexnum);
                write_index += strcpy(dst + write_index, buf);
            }
            else if(format[i] == 's'){
                // arg是一个指向指针的指针，他作为指针指向了栈中字符串的指针，因此声明为char**，
                // 最后解引用一次获得字符串的指针，即str
                char* str = *((char**)arg);
                arg += 4;
                if(str == NULL){
                    str = "(null)";
                }
                write_index += strcpy(dst + write_index, str);
            }
            else if(format[i] == 'c'){
                char c = *((char*)arg);
                arg += 4;
                dst[write_index++] = c;
            }
            else if(format[i] == '%'){
                dst[write_index++] = '%';
            }
            else {
                dst[write_index++] = '%';
                dst[write_index++] = format[i];
            }
        }else {
            dst[write_index++] = format[i];
        }
        i++;
    }
    dst[write_index] = '\0';
}