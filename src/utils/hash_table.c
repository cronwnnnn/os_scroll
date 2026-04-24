#include "utils/hash_table.h"
#include "mem/kheap.h"

#define INITIAL_BUCKETS_NUM 16
#define LOAD_FACTOR 0.75

// 哈希表对于冲突使用的是链式方法解决
// 首先bucket中存了bucket_num个的链表，在查询时取相应index的链表中去查，如果这个链表的header不是，
// 就遍历这个链表去找，相当于产生冲突往后找

// 链表中存的是多个link_node，每个node的ptr指向了一个kv对

static void hash_table_expand(hash_table_t* this);

hash_table_t hash_table_create(){
    hash_table_t map;
    hash_table_init(&map);
    return map;

}

void hash_table_init(hash_table_t* this){
    this->size = 0;
    this->buckets_num = INITIAL_BUCKETS_NUM;
    this->buckets = (linked_list_t*)kmalloc(this->buckets_num * sizeof(linked_list_t));
    for(int32_t i = 0; i < this->buckets_num; i++){
        linked_list_init(&this->buckets[i]);
    }
}



static linked_list_node_t* hash_table_bucket_lookup(
    hash_table_t* this, linked_list_t* bucket, uint32_t key) {
  linked_list_node_t* kv_node = bucket->head;
  while (kv_node != nullptr) {
    hash_table_kv_t* kv = (hash_table_kv_t*)kv_node->ptr;
    if (kv->key == key) {
      return kv_node;
    }
    kv_node = kv_node->next;
  }
  return nullptr;
}

void* hash_table_get(hash_table_t* this, uint32_t key) {
  linked_list_t* bucket = &this->buckets[key % this->buckets_num];
  linked_list_node_t* kv_node = hash_table_bucket_lookup(this, bucket, key);
  if (kv_node != nullptr) {
    hash_table_kv_t* kv = (hash_table_kv_t*)kv_node->ptr;
    return kv->v_ptr;
  }
  return nullptr;
}

void* hash_table_put(hash_table_t* this, uint32_t key, void* v_ptr) {
  linked_list_t* bucket = &this->buckets[key % this->buckets_num];
  linked_list_node_t* kv_node = hash_table_bucket_lookup(this, bucket, key);
  if (kv_node != nullptr) {
    hash_table_kv_t* kv = (hash_table_kv_t*)kv_node->ptr;
    void* old_value_ptr = kv->v_ptr;
    kv->v_ptr = v_ptr;
    return old_value_ptr;
  }

  // Insert new kv node.
  hash_table_kv_t* new_kv = (hash_table_kv_t*)kmalloc(sizeof(hash_table_kv_t));
  new_kv->key = key;
  new_kv->v_ptr = v_ptr;
  linked_list_append_ele(bucket, new_kv);
  this->size++;

  // If map size exceeds load factor, expand the map.
  if (this->size > this->buckets_num * LOAD_FACTOR) {
    hash_table_expand(this);
  }

  return nullptr;
}


static void hash_table_expand(hash_table_t* this) {
  uint32_t new_buckets_num = this->buckets_num * 2;
  linked_list_t* new_buckets = (linked_list_t*)kmalloc(new_buckets_num * sizeof(linked_list_t));
  for (int32_t i = 0; i < new_buckets_num; i++) {
    linked_list_init(&new_buckets[i]);
  }

  // Place kv nodes into new buckets
  for (int32_t i = 0; i < this->buckets_num; i++) {
    linked_list_t* bucket = &this->buckets[i];
    linked_list_node_t* kv_node = bucket->head;
    while (kv_node != nullptr) {
      hash_table_kv_t* kv = (hash_table_kv_t*)kv_node->ptr;
      linked_list_node_t* crt_node = kv_node;
      kv_node = crt_node->next;
      linked_list_remove(bucket, crt_node);

      uint32_t key = kv->key;
      linked_list_append(&new_buckets[key % new_buckets_num], crt_node);
    }
  }

  kfree(this->buckets);

  this->buckets = new_buckets;
  this->buckets_num = new_buckets_num;
}

void* hash_table_remove(hash_table_t* this, uint32_t key) {
  linked_list_t* bucket = &this->buckets[key % this->buckets_num];
  linked_list_node_t* kv_node = hash_table_bucket_lookup(this, bucket, key);
  if (kv_node != nullptr) {
    linked_list_remove(bucket, kv_node);
    hash_table_kv_t* kv = (hash_table_kv_t*)kv_node->ptr;
    void* value = kv->v_ptr;
    kfree(kv);
    kfree(kv_node);
    this->size--;

    // TODO: shrink buckets if needed?
    return value;
  }
  return nullptr;
}
