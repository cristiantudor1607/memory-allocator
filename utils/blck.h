/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include "printf.h"
#include "block_meta.h"

extern block_meta_t *head;
extern int prealloc_done;
extern size_t list_size;

/* Debugging functions */
void print_block(block_meta_t *block);
void print_list();

/* Memory List related functions */
void set_list_head(block_meta_t *block);
void add_block(block_meta_t *block);

/**
 * @brief Find the smallest unused block that can be split into 2 separate
 * blocks, and the remaining memory of the second one can be reused for future
 * allocations.
 * 
 * @param size The size of the new memory chunk we want to add to the list.
 * @return block_meta_t* A pointer to the found block, or NULL, if none of
 * them fits and respects the resulting block reusability rule.
 */
block_meta_t *find_best_block(size_t size);

/**
 * @brief Check if the resulting block after split have memory that can be
 * reused (at least 1 byte).
 * 
 * @param block The unused block that should be split.
 * @param size The size of new block.
 * @return int: 1, if the block can be split, and the remainig memory can be
 * reused, 0, if the remaining memory cannot hold even 1 byte, and -1 in case
 * of errors.
 */
int check_resulting_reusability(block_meta_t *block, size_t size);

/**
 * @brief Split the given unused block in 2 adjacent blocks, the first one
 * used to simulate the new memory allocation, and the second one will be
 * marked as free, and it's space can be used for further allocations. The
 * caller should check if the block is valid and can be split. 
 * 
 * @param unused_block The block marked as free that we want to split.
 * @param payload_size The size of the new payload that we want to allocate
 * in the block.
 * @return block_meta_t* A pointer to the newly created free block.
 */
block_meta_t* split_block(block_meta_t *unused_block, size_t payload_size);

block_meta_t *get_last_heap();
block_meta_t *get_heap_start();
block_meta_t *get_last_mmap();
block_meta_t *get_last_block();

/* Allocation related functions */
void *alloc_raw_memory(size_t raw_size, alloc_type_t syscall_type);
void *extend_heap(size_t size);
block_meta_t *alloc_new_block(size_t payload_size, size_t limit);
block_meta_t *prealloc_heap();
block_meta_t *reuse_block(size_t size);

/* Deallocation related functions */
void mark_freed(block_meta_t *block);
void extract_block(block_meta_t *block);

void merge_with_prev(block_meta_t *block);
void merge_with_next(block_meta_t *block);
void merge_free_blocks(block_meta_t *block);


/**
 * @brief Eliberates a block that contains memory allocated by mmap syscall.
 * It's a wrraper of munmap syscall, specialized on metablocks.
 * 
 * @param block The block that should be freed.
 * @return int: Return value of the munmap syscall.
 */
int free_mmaped_block(block_meta_t *block);

/* Reallocation related functions */
block_meta_t *realloc_mapped_block(block_meta_t *block, size_t size);
block_meta_t *make_space(block_meta_t *block, size_t size);

/* Helper functions */
void *get_address_by_block(block_meta_t *block);
block_meta_t *get_block_by_address(void *addr);

/**
 * @brief Get the raw memory that will remain when you want to fit new_size
 * bytes on block memory space.
 * 
 * @param block The block marked as free.
 * @param new_size The new size of the chunk we want to allocate.
 * @return size_t: Raw memory in bytes.
 */
size_t get_raw_reusable_memory(block_meta_t *block, size_t new_size);

void *memset_block(block_meta_t *block, int c);

void copy_contents(block_meta_t *src_block, block_meta_t *dest_block);
