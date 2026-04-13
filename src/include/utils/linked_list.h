#ifndef UTILS_LINKED_LIST_H
#define UTILS_LINKED_LIST_H
#include "common/types.h"

struct linked_list_node{
    type_t ptr;
    struct linked_list_node* prev;
    struct linked_list_node* next;
}__attribute__((packed));
typedef struct linked_list_node linked_list_node_t;


struct linked_list {
  linked_list_node_t* head;
  linked_list_node_t* tail;
  uint32_t size;
} __attribute__((packed));
typedef struct linked_list linked_list_t;

linked_list_t linked_list_create();
void linked_list_init(linked_list_t* list);
void linked_list_append(linked_list_t* this, linked_list_node_t* new_node);
void linked_list_insert(linked_list_t* this, linked_list_node_t* node, linked_list_node_t* new_node);
void linked_list_remove(linked_list_t* this, linked_list_node_t* node);
void linked_list_insert_to_head(linked_list_t* this, linked_list_node_t* new_node);
void linked_list_remove_ele(linked_list_t* this, type_t ptr);
void linked_list_append_ele(linked_list_t* this, type_t ptr);
void linked_list_move(linked_list_t* dst, linked_list_t* src);

#endif