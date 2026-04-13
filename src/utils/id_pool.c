#include "utils/id_pool.h"
#include "utils/bit_map.h"
#include "sync/yieldlock.h"
#include "common/secure.h"
#include "utils/utils.h"

void id_pool_init(id_pool_t* this, uint32_t size, uint32_t max_size){
    this->size = size;
    this->expand_size = size;
    this->max_size = max_size;
    bool right = bitmap_init(&this->id_map, NULL, this->size);
    Assert(right);
    yieldlock_init(&this->lock);
}

void id_pool_set_id(id_pool_t* this, size_t id){
    bitmap_set_bit(&this->id_map, id);
}

void id_pool_free_id(id_pool_t* this, size_t id){
    bitmap_clear_bit(&this->id_map, id);
}

void id_pool_destroy(id_pool_t* this){
    bitmap_destroy(&this->id_map);
}

bool id_pool_allocate_id(id_pool_t* this, uint32_t* id){
    yieldlock_lock(&this->lock);
    if(!bitmap_alloc_first_free(&this->id_map, id)){
        yieldlock_unlock(&this->lock);
        return false;
    }
    yieldlock_unlock(&this->lock);
    return true;
}


/*
static bool expand(id_pool_t* this){
    uint32_t new_size = min(this->size + this->expand_size, this->max_size);
    if(new_size == this->max_size){
        return false;
    }
    if(bit)
}*/