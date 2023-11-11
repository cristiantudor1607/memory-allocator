// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"

/* Global head of the Memory List */
block_meta_t *head;
size_t list_size = 0;

/* Global heap preallocation */
int prealloc_done = NOT_DONE;

// ---------- MAIN FUNCTIONS ----------

void *os_malloc(size_t size)
{
	/* If size is 0, return NULL and do nothing*/
	if (size == 0)
		return NULL;
	
	size_t raw_size = BLOCK_ALIGN + ALIGN(size);

	block_meta_t *new_block;
	if (raw_size <= MMAP_THRESHOLD && prealloc_done == NOT_DONE) {
		new_block = prealloc_heap();
		DIE(!new_block, "os_malloc: failed heap preallocation\n");

		add_block(new_block);
		if ((raw_size < MMAP_THRESHOLD) && (MMAP_THRESHOLD - raw_size >= MIN_SPACE)) {
			block_meta_t *freeblk = split_block(new_block, size);
			print_block(new_block);
			print_block(freeblk);
		}
			
		print_list();
		return get_address_by_block(new_block);
	}

	new_block = alloc_new_block(size);
	DIE(!new_block, "os_malloc: failed allocation\n");

	add_block(new_block);

	return get_address_by_block(new_block);
}

void os_free(void *ptr)
{	
	/* If pointer is NULL, do nothing */
	if (ptr == NULL)
		return;

	block_meta_t *block = get_block_by_address(ptr);
	if (block->status == STATUS_MAPPED) {
		int ret = free_mmaped_block(block);
		DIE(ret, "os_free: munmap failure\n");
		return;
	} else if (block->status == STATUS_FREE) {
		return;
	} else {
		mark_freed(block);
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
