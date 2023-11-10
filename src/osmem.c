// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"

// printf_("----- MALLOC -----\n");
// printf_("----- FREE -----\n");


/* Global head of the Memory List */
block_meta_t head = {0, STATUS_FREE, NULL, NULL};

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
	if (size < MMAP_THRESHOLD) {
		if (preallocation_done == PREALLOCATION_NOT_DONE) {
			new_block = prealloc_heap();
			preallocation_done = PREALLOCATION_DONE;
		} else {
			new_block = alloc_new_block(size, BRK_ALLOC);
		}
	} else {
		new_block = alloc_new_block(size, MMAP_ALLOC);
	}

	/* Check for allocation failures */
	DIE(!new_block, "Failed to reserve a new chunk of memory.\n");
	
	/* Add the block to the Memory List */
	add_block(new_block);
	
	/* Get the address of the payload to return it */
	void *p = get_address_by_block(new_block);
	
	/* Debug */
	print_list();

	return p;
}

void os_free(void *ptr)
{	
	/* If pointer is NULL, do nothing */
	if (ptr == NULL)
		return;


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
