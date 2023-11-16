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

/**
 * @brief Head of the Memory List
 */
extern block_meta_t *head;

/**
 * @brief Preallocation status variable
 */
extern int prealloc_done;

/**
 * @brief Make the block sent as parameter be the new head of Memory List. The
 * function is called only when list is empty, so it doesn't relink the old
 * head's prev and next pointers
 * 
 * @param block The first allocated block that will become the head of the list
 */
void set_list_head(block_meta_t *block);

/**
 * @brief Get the last block allocated on heap
 * 
 * @return block_meta_t* The last block allocated on heap, or NULL in case
 * there is no block allocated using sbrk()
 */
block_meta_t *get_last_heap();

/**
 * @brief Get the first block allocated on heap
 * 
 * @return block_meta_t* The first block allocated on heap, or NULL in case
 * there is no block allocated using sbrk()
 */
block_meta_t *get_heap_start();

/**
 * @brief Get the last block allocated using mmap
 * 
 * @return block_meta_t* The last block allocated using mmap, or NULL in case
 * there are only blocks on heap
 */
block_meta_t *get_last_mmap();

/**
 * @brief Get the last block from global Memory List
 * 
 * @return block_meta_t* The last block allocated, or NULL in case list is
 * empty
 */
block_meta_t *get_last_block();

/**
 * @brief Insert in Memory List a block allocated using mmap syscall
 * 
 * @param block The block that was previously verified to be allocated using
 * mmap
 */
void insert_mmaped_block(block_meta_t *block);

/**
 * @brief Insert in Memory List a block allocated using brk syscall
 * 
 * @param block The block that was previously verified to be allocated using
 * brk
 */
void insert_heap_block(block_meta_t *block);

/**
 * @brief Insert a new block in Memory List, no matter it's type
 * 
 * @param block An allocated block
 */
void add_block(block_meta_t *block);

/**
 * @brief Split the given unused block in 2 adjacent blocks, the first one
 * used to simulate the new memory allocation, and the second one will be
 * marked as free, and it's space can be used for further allocations. The
 * caller should check if the block is valid and can be split
 * 
 * @param unused_block The block marked as free that we want to split
 * @param payload_size The size of the new payload that we want to allocate
 * in block sent as parameter
 * @return block_meta_t* A pointer to the newly created free block
 */
block_meta_t* split_block(block_meta_t *unused_block, size_t payload_size);

/**
 * @brief Unlink the block from other blocks, and relink the prev and next
 * pointers, if they exist
 * 
 * @param block The block to be removed. It works for any block, but is
 * designed only for mmaped blocks.
 */
void extract_block(block_meta_t *block);

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
 * @brief Allocates a new chunk of memory using the syscall name send as
 * parameter
 * 
 * @param raw_size The size (in bytes) that should be allocated
 * @param syscall_type The syscall that should be used when allocating the
 * new memory
 * @return void* Pointer to the new allocated memory, or NULL in case of
 * failure
 */
void *alloc_raw_memory(size_t raw_size, alloc_type_t syscall_type);

/**
 * @brief Allocates a new block
 * 
 * @param payload_size The size that can be used for storing some data
 * @param limit The limit parameter is used to indicate which size is
 * considered too big to be allocated using brk syscall
 * @return block_meta_t* Pointer to the new block (!not new memory chunk), 
 * or NULL in case allocation fails
 */
block_meta_t *alloc_new_block(size_t payload_size, size_t limit);

/**
 * @brief Preallocates a size of 128 kB on heap (including the size of block
 * structure )
 * 
 * @return block_meta_t* Pointer to the block that contains the 128 kB
 * preallocated on heap, or NULL in case of allocation fails
 */
block_meta_t *prealloc_heap();

/**
 * @brief Expand the heap with size bytes
 * 
 * @param size The additional memory in bytes
 * @return void* Previous program break (as the manual says about sbrk)
 */
void *expand_heap(size_t size);

/**
 * @brief Reuse blocks that are free. The functions does more than that, it
 * splits a block if it is too big, expand the heap to make room for new
 * memory chunk, or it does nothing to the block
 * 
 * @param size The size that should be allocated
 * @return block_meta_t* Pointer to the found block, or NULL in case it didn't
 * find anything
 */
block_meta_t *reuse_block(size_t size);

/**
 * @brief Marks a block as free. If the block wasn't allocated on heap, the
 * program dies, but this is a measure of safety for the developer, to know
 * that this function should be called only on blocks marked as STATUS_FREE
 * 
 * @param block The block that should be marked as free. It was previously
 * checked to be allocated by sbrk()
 */
void mark_freed(block_meta_t *block);

/**
 * @brief Merge a block with it's next neighbour, if is possible
 * 
 * @param block The "central" block that should be merged with it's next
 */
void merge_with_next(block_meta_t *block);
void merge_with_prev(block_meta_t *block);
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
block_meta_t *move_to_mmap_space(block_meta_t *block, size_t size);
block_meta_t *unite_blocks(block_meta_t *block, size_t size);
void *truncate_block(block_meta_t *block, size_t new_size);
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

size_t get_raw_size(block_meta_t *block);
