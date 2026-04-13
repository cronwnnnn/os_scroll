#ifndef UTILS_HASH_TABLE_H
#define UTILS_HASH_TABLE_H
#include "common/types.h"
#include "utils/linked_list.h"

// 构建了一个映射，一个kv对用于表示一个tcb以及其链表对应的链表节点？
struct hash_table_kv {
  uint32_t key;
  void* v_ptr;
};
typedef struct hash_table_kv hash_table_kv_t;

struct hash_table{
    linked_list_t* buckets;
    int32_t buckets_num;
    int32_t size;
};
typedef struct hash_table hash_table_t;


hash_table_t create_hash_table();

void hash_table_init(hash_table_t* this);

// If the key already exists in map, old value will be replaced and returned.
void* hash_table_put(hash_table_t* this, uint32_t key, void* v_ptr);

void* hash_table_get(hash_table_t* this, uint32_t key);

void* hash_table_remove(hash_table_t* this, uint32_t key);

void hash_table_clear(hash_table_t* this);

void hash_table_destroy(hash_table_t* this);





#endif