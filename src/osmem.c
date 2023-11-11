// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"

/* Global head of the Memory List */
block_meta_t head = {0, STATUS_FREE, NULL, NULL};
size_t list_size = 0;

/* Global heap preallocation */
int preallocation_done = PREALLOCATION_NOT_DONE;

// ---------- MAIN FUNCTIONS ----------

void *os_malloc(size_t size)
{
	/* If size is 0, return NULL and do nothing*/
	if (size == 0)
		return NULL;

	/* Try to reserve a new block */
	block_meta_t *new_block;
	if (SIZE(size) <= MMAP_THRESHOLD) {
		if (preallocation_done == PREALLOCATION_NOT_DONE) {
			new_block = prealloc_heap();
			DIE(!new_block, "Failed Heap Preallocation\n");
			preallocation_done = PREALLOCATION_DONE;
			add_block(new_block);

			if (SIZE(size) == MMAP_THRESHOLD) {
				new_block->status = STATUS_ALLOC;
				return get_address_by_block(new_block);
			}

			split_block(new_block, size);
			return get_address_by_block(new_block);
		} else {
			new_block = alloc_new_block(size, BRK);
			DIE(!new_block, "Failed sbrk allocation\n");
		}

	} else {
		new_block = alloc_new_block(size, MMAP);
		DIE(!new_block, "Failed mmap allocation\n");
	}

	add_block(new_block);
	return get_address_by_block(new_block);
}

void os_free(void *ptr)
{	
	/* If pointer is NULL, do nothing */
	if (ptr == NULL)
		return;

	block_meta_t *block = get_block_by_address(ptr);
	if (block->status == STATUS_ALLOC) {
		mark_freed(block);
		return;
	}

	if (block->status == STATUS_MAPPED) {
		extract_block(block);
		free_mmaped_block(block);
		return;
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	/* TODO: Implement os_calloc */
	return NULL;
}

void *os_realloc(void *ptr, size_t size)
{
	/* TODO: Implement os_realloc */
	return NULL;
}
