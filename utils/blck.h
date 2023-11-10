/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include "printf.h"
#include "block_meta.h"

extern block_meta_t head;
extern int preallocation_done;

/* Debugging functions */
void print_block(block_meta_t *block);
void print_list();

/* Memory List related functions */
void set_list_head(block_meta_t *block);
void add_block(block_meta_t *block);
block_meta_t *find_best_block(size_t size);

/**
 * @brief Check if the block send as parameter can be split
 * 
 * @param block 
 * @param size 
 * @return int 
 */
int check_reusability_property(block_meta_t *block, size_t size);

/**
 * @brief Split a free block into two adjacent blocks, use the memory of the
 * first one and mark free the second one, if there is space remaining.
 * 
 * @param unused_block The block from the list marked as unused.
 * @param size The number of bytes we want to store in the block.
 */
void use_free_memory(block_meta_t *unused_block, size_t size);

/* Allocation related functions */
block_meta_t *alloc_new_block(size_t payload_size, alloc_type_t sys_used);
block_meta_t *prealloc_heap();

/* Deallocation related functions */
void mark_freed(block_meta_t *block);
void extract_block(block_meta_t *block);

/* Helper functions */
size_t get_padding(size_t chunk_size);
void *get_address_by_block(block_meta_t *block);
block_meta_t *get_block_by_address(void *addr);
