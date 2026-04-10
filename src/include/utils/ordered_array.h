#ifndef UTILS_ORDERED_ARRAY_H
#define UTILS_ORDERED_ARRAY_H

#include "common/types.h"

typedef void* type_t;
typedef int32_t (*compare_func_t)(type_t, type_t);

typedef struct {
    //指向存放指针的数组
    type_t *array;
    uint32_t size;
    uint32_t max_size;
    compare_func_t compare;
} ordered_array_t;

ordered_array_t ordered_array_create(type_t* array, uint32_t max_size, compare_func_t compare);
bool ordered_array_insert(ordered_array_t *this, type_t item);
type_t ordered_array_get(ordered_array_t *this, uint32_t index);
void ordered_array_remove(ordered_array_t *this, uint32_t index);
uint32_t ordered_array_find_element(ordered_array_t *this, type_t item);
uint32_t ordered_array_remove_element(ordered_array_t *this, type_t item);


#endif