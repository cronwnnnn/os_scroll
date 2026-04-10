#include "utils/ordered_array.h"


ordered_array_t ordered_array_create(type_t* array, uint32_t max_size, compare_func_t compare) {
    ordered_array_t arr;
    arr.array = array;
    arr.size = 0;
    arr.max_size = max_size;
    arr.compare = compare;
    return arr;
}

bool ordered_array_insert(ordered_array_t *this, type_t item) {
    if (this->size >= this->max_size) {
        return false; // 数组已满，无法插入
    }
    
    uint32_t i = 0;
    while (i < this->size && this->compare(this->array[i], item) <= 0) {
        i++;
    }
    
    for (uint32_t j = this->size; j > i; j--) {
        this->array[j] = this->array[j - 1];
    }
    
    this->array[i] = item;
    this->size++;
    
    return true; // 插入成功
}

type_t ordered_array_get(ordered_array_t *this, uint32_t index) {
    if (index >= this->size) {
        return NULL; // 索引越界，返回NULL
    }
    return this->array[index];
}

void ordered_array_remove(ordered_array_t *this, uint32_t index) {
    if (index >= this->size) {
        return; // 索引越界，返回NULL
    }
    for(size_t i = index; i < this->size - 1; i++) {
        this->array[i] = this->array[i + 1];
    }
    this->size--;
}

uint32_t ordered_array_find_element(ordered_array_t *this, type_t item) {
    for (uint32_t i = 0; i < this->size; i++) {
        if (this->array[i] == item) {
            return i;
        }
    }
    return this->size; // 返回size表示未找到
}

uint32_t ordered_array_remove_element(ordered_array_t *this, type_t item) {
    uint32_t index = this->size; // 初始化为size，表示未找到
    for (uint32_t i = 0; i < this->size; i++) {
        if (this->array[i] == item) {
           index = i; 
            break;
        }
    }
    if(index == this->size) {
        return 0; // 未找到元素，返回0
    }
    
    for(size_t i = index; i < this->size - 1; i++) {
        this->array[i] = this->array[i + 1];
    }
    this->size--;
    return 1; 
}