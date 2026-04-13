#include "utils/linked_list.h"
#include "common/types.h"
#include "common/secure.h"
#include "mem/kheap.h"

linked_list_t linked_list_create(){
    linked_list_t list;
    linked_list_init(&list);
    return list;
}


void linked_list_init(linked_list_t* list){
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

void linked_list_append(linked_list_t* this, linked_list_node_t* new_node){
    Assert(new_node != NULL);
    new_node->next = NULL;
    if(this->size == 0){
        new_node->prev = NULL;
        this->head = new_node;
        this->tail = new_node;
    }else{
        this->tail->next = new_node;
        new_node->prev = this->tail;
        this->tail = new_node;
    }
    this->size++;
    return;
}

    // 用于将new_node 插入到node后面
void linked_list_insert(linked_list_t* this, linked_list_node_t* node, linked_list_node_t* new_node){
    // new_node 的前面一定是node，node为NULL也是
    new_node->prev = node;

    // next_node 用于找new_node 后面的到底是谁
    linked_list_node_t* next_node;

    // node 为NULL，表示要插入到头部
    if(node != NULL){
        next_node = node->next;
        node->next = new_node;
    }else{
        // insert to head
        // 当前的head变成了new_node 的next
        next_node = this->head;
        this->head = new_node;
    }
    // 对于两种情况都找到了new_node的next_node，直接插入
    new_node->next = next_node;

    // 看next_node 需不需要变
    if(next_node != NULL){
        next_node->prev = new_node;
    }else{
        this->tail = new_node;
    }

    this->size++;
}

void linked_list_remove(linked_list_t* this, linked_list_node_t* node){
    Assert(node != NULL);
    if(this->size == 0)
        return;
    linked_list_node_t* prev_node = node->prev;
    linked_list_node_t* next_node = node->next;
    if(prev_node != NULL){
        prev_node->next = next_node;
    }else{
        // is head
        this->head = next_node;
    }
    
    if(next_node != NULL){
        next_node->prev = prev_node;
    }else{
        // is tail
        this->tail = prev_node;
    }
    
    node->next = NULL;
    node->prev = NULL;

    this->size--;
}

void linked_list_move(linked_list_t* dst, linked_list_t* src) {
  dst->head = src->head;
  dst->tail = src->tail;
  dst->size = src->size;

  linked_list_init(src);
}

void linked_list_append_ele(linked_list_t* this, type_t ptr) {
  linked_list_node_t* node = (linked_list_node_t*)kmalloc(sizeof(linked_list_node_t));
  node->ptr = (void*)ptr;
  linked_list_append(this, node);
}

void linked_list_remove_ele(linked_list_t* this, type_t ptr) {
  linked_list_node_t* node = this->head;
  while (node != nullptr) {
    if (node->ptr == ptr) {
      linked_list_remove(this, node);
      break;
    }
    node = node->next;
  }
}

void linked_list_insert_to_head(linked_list_t* this, linked_list_node_t* new_node) {
  linked_list_insert(this, NULL, new_node);  
}
