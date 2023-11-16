// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"

/* Global head of the Memory List */
block_meta_t *head;

/* Global heap preallocation */
int prealloc_done = NOT_DONE;

void *os_malloc(size_t size)
{
	/* If size is 0, return NULL and do nothing*/
	if (size == 0)
		return NULL;

	size_t raw_size = BLOCK_ALIGN + ALIGN(size);

	block_meta_t *new_block;

	if (raw_size <= MMAP_THRESHOLD && prealloc_done == NOT_DONE) {
		new_block = prealloc_heap();
		DIE(!new_block, "malloc: failed heap preallocation\n");

		add_block(new_block);
		if ((raw_size < MMAP_THRESHOLD) && (MMAP_THRESHOLD - raw_size >= MIN_SPACE))
			split_block(new_block, size);
		else
			new_block->status = STATUS_ALLOC;

		/* Mark prealloc as done */
		prealloc_done = DONE;

		return get_address_by_block(new_block);
	}

	block_meta_t *free_block = reuse_block(size);

	if (free_block)
		return get_address_by_block(free_block);

	new_block = alloc_new_block(size, MMAP_THRESHOLD);
	DIE(!new_block, "malloc: failed allocation\n");

	add_block(new_block);

	return get_address_by_block(new_block);
}

void os_free(void *ptr)
{
	/* If pointer is NULL, do nothing */
	if (ptr == NULL)
		return;

	block_meta_t *block = get_block_by_address(ptr);

	/* If the address was generated by mmap */
	if (block->status == STATUS_MAPPED) {
		extract_block(block);
		int ret = free_mmaped_block(block);

		DIE(ret, "free: munmap failure\n");
		return;
	}

	/* If the address was generated by sbrk */
	if (block->status == STATUS_ALLOC) {
		mark_freed(block);
		merge_free_blocks(block);
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	if (!size || !nmemb)
		return NULL;

	size_t raw_size = BLOCK_ALIGN + ALIGN(nmemb * size);
	block_meta_t *new_block;

	if (raw_size <= PAGE_SIZE && prealloc_done == NOT_DONE) {
		new_block = prealloc_heap();
		memset_block(new_block, 0);
		DIE(!new_block, "calloc: failed heap preallocation\n");

		add_block(new_block);
		if ((raw_size < PAGE_SIZE) && (PAGE_SIZE - raw_size >= MIN_SPACE))
			split_block(new_block, nmemb * size);
		else
			new_block->status = STATUS_ALLOC;

		/* Mark prealloc as done */
		prealloc_done = DONE;

		return get_address_by_block(new_block);
	}

	if (raw_size > PAGE_SIZE) {
		new_block = alloc_new_block(nmemb * size, PAGE_SIZE);
		DIE(!new_block, "calloc: failed allocation\n");

		memset_block(new_block, 0);
		add_block(new_block);

		return get_address_by_block(new_block);
	}

	block_meta_t *free_block = reuse_block(nmemb * size);

	if (free_block)
		return memset_block(free_block, 0);

	new_block = alloc_new_block(nmemb * size, PAGE_SIZE);
	DIE(!new_block, "calloc: failed allocation\n");

	memset_block(new_block, 0);
	add_block(new_block);

	return get_address_by_block(new_block);
}

void *os_realloc(void *ptr, size_t size)
{
	if (!ptr && !size)
		return NULL;

	if (!ptr) {
		block_meta_t *unused = reuse_block(size);

		if (unused)
			return get_address_by_block(unused);
		else
			return os_malloc(size);
	}

	if (!size) {
		os_free(ptr);
		return NULL;
	}

	block_meta_t *block = get_block_by_address(ptr);

	if (block->status == STATUS_FREE)
		return NULL;

	/* First, for mapped blocks there is a single case */
	if (block->status == STATUS_MAPPED) {
		block_meta_t *new_block = realloc_mapped_block(block, size);

		DIE(!new_block, "realloc: failed allocation\n");
		os_free(ptr);
		return get_address_by_block(new_block);
	}

	/* If the heap block should be rellocated with mmap */
	if (ALIGN(size) + BLOCK_ALIGN > MMAP_THRESHOLD) {
		block_meta_t *new_block = move_to_mmap_space(block, size);

		DIE(!new_block, "realloc: failed allocation\n");
		os_free(ptr);
		return get_address_by_block(new_block);
	}


	/* If the size is smaller than all the memory allocated in block's memory,
	 * there are two possibilities: is considerable smaller, it should be
	 * splitted, else, it should be truncated
	 */
	size_t true_size = get_raw_size(block);

	if (ALIGN(size) <= true_size) {
		/* Split case */
		if (true_size - ALIGN(size) >= MIN_SPACE) {
			block->size = true_size;
			split_block(block, size);
			return ptr;
		}

		/* Truncate case */
		block->size = size;
		return ptr;
	}


	/* Check if memory can be expanded by expanding the heap */
	if (!block->next) {
		expand_heap(ALIGN(size) - true_size);
		block->size = size;
		return ptr;
	}

	/* Try merging free blocks to reach the wanted size */
	block_meta_t *merged_block = unite_blocks(block, size);

	if (merged_block)
		return get_address_by_block(merged_block);

	/* Try to reuse free block */
	block_meta_t *reused_block = reuse_block(size);

	if (reused_block) {
		block->size = true_size;
		copy_contents(block, reused_block);
		os_free(ptr);
		return get_address_by_block(reused_block);
	}

	/* Move to another zone */
	block_meta_t *new_zone = alloc_new_block(size, MMAP_THRESHOLD);

	DIE(!new_zone, "realloc: allocation failed\n");
	add_block(new_zone);
	copy_contents(block, new_zone);
	os_free(ptr);

	return get_address_by_block(new_zone);
}
