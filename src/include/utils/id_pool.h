#ifndef UTILS_ID_POOL_H
#define UTILS_ID_POOL_H

#include "common/types.h"
#include "utils/bit_map.h"
#include "sync/yieldlock.h"

// This is a thread-safe pool to allocate uint32_t id.
struct id_pool {
  int32_t size;
  int32_t max_size;
  int32_t expand_size;
  bitmap_t id_map;
  yieldlock_t lock;
};
typedef struct id_pool id_pool_t;

void id_pool_init(id_pool_t* this, uint32_t size, uint32_t max_size);
void id_pool_set_id(id_pool_t* this, uint32_t id);
bool id_pool_allocate_id(id_pool_t* this, uint32_t* id);
void id_pool_free_id(id_pool_t* this, uint32_t id);
void id_pool_destroy(id_pool_t* this);


// ****************************************************************************
void id_pool_test();

#endif
