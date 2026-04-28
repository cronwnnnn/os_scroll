#include "common/stdlib.h"
#include "mem/kheap.h"
#include "utils/string.h"

char** copy_str_array(uint32_t num, char* str_array[]){
    if (str_array == nullptr) {
        return nullptr;
    }

    int32_t len = 0;
    char** ptr = (char**)kmalloc(num * 4);
    for(uint32_t i = 0; i < num; i++){
        if (str_array[i] == nullptr) {
            ptr[i] = nullptr;
            continue;
        }

        len = 0;
        while (str_array[i][len]){
            len++;
        }
        char* str = (char*)kmalloc(len+1);
        strcpy_with_len(str, str_array[i]);
        ptr[i] = str;
    }
    return ptr;
}

void destroy_str_array(uint32_t num, char* str_array[]){
    if (str_array == nullptr) {
        return;
    }
    for(uint32_t i = 0; i < num; i++){
        kfree(str_array[i]);
    }
    kfree(str_array);
}

