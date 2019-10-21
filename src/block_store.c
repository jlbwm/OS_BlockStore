#include <stdio.h>
#include <stdint.h>
#include <string.h> 
#include "bitmap.h"
#include "block_store.h"

/**
#define BITMAP_SIZE_BYTES 32 // 2^8 bit  2^5 byte
#define BLOCK_STORE_NUM_BLOCKS 256   // 2^8 blocks. 
#define BLOCK_STORE_AVAIL_BLOCKS (BLOCK_STORE_NUM_BLOCKS - 1) // First block consumed by the FBM
#define BLOCK_SIZE_BYTES 256 // 2^8 BYTES per block
#define BLOCK_SIZE_BITS (BLOCK_SIZE_BYTES*8)
#define BLOCK_STORE_NUM_BYTES (BLOCK_STORE_NUM_BLOCKS * BLOCK_SIZE_BYTES) 256 * 256
*/
#define NUM_OF_BLOCK_STORE 1

typedef struct block {
    uint8_t cells[BLOCK_SIZE_BYTES];
} block_t;

struct block_store {
    block_t blocks[BLOCK_STORE_NUM_BLOCKS];
};

/* 
* private function
*/

// created the new bitmap and copy it to bs and finally free the bm
bool store_bitmap(block_store_t* bs, bitmap_t* bm) {
    if(!bm || !bs) {
        return false;
    }
    memcpy((void*)bs, (void*)bm, BITMAP_SIZE_BYTES);
    bitmap_destroy(bm);
    return true;
}

// extract the bitmap (first block of block_store) 
bitmap_t* extract_bitmap(block_store_t* bs) {
    bitmap_t* bm = bitmap_create(BITMAP_SIZE_BYTES << 0x3);
    if (!bm) {
        return NULL;
    }
    memcpy((void*)bm, (void*)bs, BITMAP_SIZE_BYTES);
    return bm;
}

block_store_t* block_store_create() {

    block_store_t* bs = calloc(NUM_OF_BLOCK_STORE, sizeof(block_store_t));
    if (!bs) {
        return NULL;
    }

    // bitmap_t* bm = extract_bitmap(bs);
    bitmap_t* bm = bitmap_create(BITMAP_SIZE_BYTES << 0x3);
    if(!bm) {
        free(bs);
        return NULL;
    }
    // // Set the first block(FBM) in use
    // bitmap_set(bm, 0);
    // // printf("set success\n");

    store_bitmap(bs, bm);

    return bs;
}

void block_store_destroy(block_store_t *const bs) {
    free(bs);
}

size_t block_store_allocate(block_store_t *const bs) {
    if (!bs) {
        return SIZE_MAX;
    }
    bitmap_t* bm =  extract_bitmap(bs);
    size_t free_block = bitmap_ffz(bm);

    if(free_block != SIZE_MAX) 
    { 
        bitmap_set(bm, free_block); 
    }
    store_bitmap(bs, bm);

    return free_block; 
}

// bool block_store_request(block_store_t *const bs, const size_t block_id) {
    
// }

// void block_store_release(block_store_t *const bs, const size_t block_id) {
// }

// size_t block_store_get_used_blocks(const block_store_t *const bs) {
//     return 0;
// }

// size_t block_store_get_free_blocks(const block_store_t *const bs) {
//     return 0;
// }

size_t block_store_get_total_blocks() {
    return BLOCK_STORE_AVAIL_BLOCKS;
}

// size_t block_store_read(const block_store_t *const bs, const size_t block_id, void *buffer) {
//     return 0;
// }

// size_t block_store_write(block_store_t *const bs, const size_t block_id, const void *buffer) {
//     return 0;
// }

// block_store_t *block_store_deserialize(const char *const filename) {
//     return NULL;
// }

// size_t block_store_serialize(const block_store_t *const bs, const char *const filename) {
//     return 0;
// }