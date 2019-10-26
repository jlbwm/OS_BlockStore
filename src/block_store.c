#include <stdio.h>
#include <stdint.h>
#include <string.h> 
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
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

// destroy bitmap itself, don't destory bitmap's data 
void bitmap_destroy_overlay(bitmap_t *bitmap) {
    free(bitmap);
}

// created the new bitmap and copy it to bs and finally free the bm
bool store_bitmap(block_store_t* bs, bitmap_t* bm) {
    if(!bm || !bs) {
        return false;
    }
    memcpy((void*)bs, (void*)bm, BITMAP_SIZE_BYTES);
    // do not destory bitmap->data
    bitmap_destroy_overlay(bm);
    return true;
}

// extract the bitmap (first block of block_store) 
bitmap_t* extract_bitmap(const block_store_t* bs) {
    bitmap_t* bm = bitmap_create_nodata(BLOCK_STORE_AVAIL_BLOCKS);
    if (!bm) {
        printf("extract error\n");
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
    bitmap_t* bm = bitmap_create(BLOCK_STORE_AVAIL_BLOCKS);
    if(!bm) {
        free(bs);
        return NULL;
    }
    store_bitmap(bs, bm);

    return bs;
}

void block_store_destroy(block_store_t *const bs) {
    if (!bs) {
        return;
    }
    // destory the bitmap and it's data section first
    bitmap_t* bm = extract_bitmap(bs);
    bitmap_destroy(bm);
    // then free the entire the block_store
    free(bs);
}

size_t block_store_allocate(block_store_t *const bs) {
    if (!bs) {
        return SIZE_MAX;
    }
    bitmap_t* bm = extract_bitmap(bs);
    if (!bm) {
        return SIZE_MAX;
    }
    size_t free_block = bitmap_ffz(bm);
    if(free_block != SIZE_MAX) { 
        bitmap_set(bm, free_block); 
    }
    store_bitmap(bs, bm);

    return free_block; 
}

bool block_store_request(block_store_t *const bs, const size_t block_id) {
    if (!bs || block_id > BLOCK_STORE_AVAIL_BLOCKS) {
        return false;
    }
    bitmap_t* bm = extract_bitmap(bs);
    if (bitmap_test(bm, block_id)) {
        bitmap_destroy_overlay(bm);
        return false;
    }
    bitmap_set(bm, block_id);
    store_bitmap(bs, bm);
    return true;

    
}

void block_store_release(block_store_t *const bs, const size_t block_id) {
    if (!bs) {
        return;
    }

    bitmap_t* bm = extract_bitmap(bs);
    
    if (bitmap_test(bm, block_id)) {
        bitmap_reset(bm, block_id);
    }
    store_bitmap(bs, bm);

}

size_t block_store_get_used_blocks(const block_store_t *const bs) {
    if (!bs) {
        return SIZE_MAX;
    }
    bitmap_t* bm = extract_bitmap(bs);
    bitmap_destroy_overlay(bm);
    return bitmap_total_set(bm);
}

size_t block_store_get_free_blocks(const block_store_t *const bs) {
    if (!bs) {
        return SIZE_MAX;
    }
    return BLOCK_STORE_AVAIL_BLOCKS - block_store_get_used_blocks(bs);
}

size_t block_store_get_total_blocks() {
    return BLOCK_STORE_AVAIL_BLOCKS;
}

size_t block_store_read(block_store_t *const bs, const size_t block_id, void *buffer) {
    if (!bs || !buffer || block_id > BLOCK_STORE_AVAIL_BLOCKS) {
        return 0;
    }
    bitmap_t* bm = extract_bitmap(bs);
    if (!bitmap_test(bm, block_id)) {
        // if block_id not set
        printf("not set yet");
        bitmap_destroy_overlay(bm);
        return 0;
    }
    memcpy(buffer, bs->blocks[block_id].cells, BLOCK_SIZE_BYTES); 
    // printf("read result:\n");
    // for (size_t i = 0; i < BLOCK_SIZE_BYTES; i++) {
    //     printf("[%zd]: %c\n", i, *(char*)buffer);
    //     buffer++;
    // }
    bitmap_destroy_overlay(bm);
    return BLOCK_SIZE_BYTES;
}

size_t block_store_write(block_store_t *const bs, const size_t block_id, const void *buffer) {
    if (!bs || !buffer || block_id > BLOCK_STORE_AVAIL_BLOCKS) {
        return 0;
    }
    bitmap_t* bm = extract_bitmap(bs);
    if (!bm) {
        bitmap_destroy_overlay(bm);
        return 0;
    }
    memcpy(bs->blocks[block_id].cells, buffer, BLOCK_SIZE_BYTES);
    // printf("write result:\n");
    // for (size_t i = 0; i < BLOCK_SIZE_BYTES; i++) {
    //     printf("[%zd]: %c\n", i, bs->blocks[block_id].cells[i]);
    // }
    bitmap_set(bm, block_id);
    store_bitmap(bs, bm);
    return BLOCK_SIZE_BYTES;
}

// recover bitmap data section from block_store first block
void recover_bitmap_data(block_store_t *const bs) {
    if (!bs) {
        return;
    }
    bitmap_t* bm = extract_bitmap(bs);
    uint8_t* data = get_bitmap_data(bm);
    void* start_ptr = (void*)bs + BITMAP_SIZE_BYTES;
    memcpy((void*)data, start_ptr, BITMAP_SIZE_BYTES);
    bitmap_destroy_overlay(bm);
}

block_store_t *block_store_deserialize(const char *const filename) {
    if(!filename) { 
        return NULL; 
    }
    int fd = open(filename, O_RDONLY);
    if (fd < 0){
        perror("Error");
        return NULL;
    }

    block_store_t* bs = block_store_create();
    if(!bs){
        close(fd);
        return NULL;
    }
    // recover block_store itself from the filename
    read(fd, bs, BLOCK_STORE_NUM_BYTES);
    // recover bitmap_data section from blockstore first block
    recover_bitmap_data(bs);

    close(fd);

    if(errno){
        perror("Error");
        block_store_destroy(bs);
        return NULL;
    }
    return bs;
}

bool store_bitmap_data(block_store_t *const bs) {
    if (!bs) {
        return false;
    }
    bitmap_t* bm = extract_bitmap(bs);
    if (!bm) {
        return false;
    }
    uint8_t* const data = get_bitmap_data(bm);
    if (!data) {
        bitmap_destroy_overlay(bm);
        return false;
    }
    void* start_ptr = (void*)bs + BITMAP_SIZE_BYTES;
    memcpy((void*)start_ptr, (void*)data, BITMAP_SIZE_BYTES);
    bitmap_destroy_overlay(bm);
    return true;
}

size_t block_store_serialize(block_store_t *const bs, const char *const filename) {
    if(!bs || !filename) {
        return 0;
    }
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
    int flags = O_CREAT | O_TRUNC | O_WRONLY;

    int fd = open(filename, flags, mode);
    if (fd < 0){
        perror("Error");
        return 0;
    }
    // store bitmap->data to the first block
    if (!store_bitmap_data(bs)) {
        printf("data equal to null");
        return 0;
    }
    write(fd, bs, BLOCK_STORE_NUM_BYTES);
    close(fd);
    if(errno){
        perror("Error");
        return 0;
    }
    return BLOCK_STORE_NUM_BYTES;
}


