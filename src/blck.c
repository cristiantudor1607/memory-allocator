// SPDX-License-Identifier: BSD-3-Clause

#include "blck.h"

// ---------- META_BLOCK IMPLEMENTATION ----------

// ---------- MEMORY LIST RELATED FUNCTIONS ----------

void set_list_head(block_meta_t *block)
{
	head.size = block->size;
	head.status = block->status;
	head.prev = NULL;
	head.next = NULL;
}

void add_block(block_meta_t *block)
{
	if (head.size == 0) {
		set_list_head(block);
        list_size++;
		return;
	}

	/* Go to the end of the list */
	block_meta_t *iterator = &head;
	while (iterator->next != NULL)
		iterator = iterator->next;

    /* Make the connexions between block and iterator */
	iterator->next = block;
	block->prev = iterator;

    list_size++;
}

int check_resulting_reusability(block_meta_t *block, size_t size)
{
    /* Invalid call of function */
    if (block == NULL || size == 0)
        return -1;

    /* Invalid block for reusability */
    if (block->status != STATUS_FREE)
        return -1;

    /* Invalid block size for reusability */
    if (block->size < size)
        return -1;

    /* Calculate the total free space of the unused block */
    size_t total_space = 0;
    size_t old_mem_size = block->size;
    total_space += METADATA_SIZE + METADATA_PAD + old_mem_size +
                   get_padding(old_mem_size);

    /* Calculate the new used space */
    size_t new_space = 0;
    new_space += METADATA_SIZE + METADATA_PAD + size +
                 get_padding(size);

    /* Calculate the difference between the old total memory and the new
    total memory */
    size_t difference = total_space - new_space;
    
    /* The difference should be a multiple of ALIGN_SIZE, so the remaining
    memory can be aligned, and greater than the MIN_SPACE */
    if ((MIN_SPACE <= difference) && (difference % ALIGN_SIZE == 0))
        return 1;

    return 0;
}

block_meta_t *find_best_block(size_t size)
{
    block_meta_t *return_block = NULL;
    block_meta_t *iterator = &head;

    while (iterator) {
        /* If the chunk isn't marked as free, skip this step */
        if (iterator->status != STATUS_FREE) {
            iterator = iterator->next;
            continue;
        }

        /* Check if the current block cannot be split */
        if (!check_resulting_reusability(iterator, size)) {
            iterator = iterator->next;
            continue;
        }

        /* If it finds the first fitting zone */
        if (iterator->size >= size && return_block == NULL) {
            return_block = iterator;
            iterator = iterator->next;
            continue;
        }

        /* If it reaches this point, we should check if return_block is NULL, 
        because the next if statement will try to dereferentiate it: if at
        least one fitting block wasn't found, we can't compare their sizes */
        if (return_block == NULL) {
            iterator = iterator->next;
            continue;
        }

        /* If the current block size is smaller than the size found by now, and
        it can fit size bytes in it, update it */
        if (return_block->size > iterator->size && iterator->size >= size) {
            return_block = iterator;
            iterator = iterator->next;
            continue;
        }

        iterator = iterator->next;
    }

    return return_block;
}

block_meta_t *split_block(block_meta_t *unused_block, size_t new_size)
{
    /* It will be useful in the future to know the size of the resulting free 
    block */
    size_t resultingblk_size = 0;

    /* Start from the size of the bigger unused block */
    resultingblk_size += METADATA_SIZE + METADATA_PAD + unused_block->size +
                   get_padding(unused_block->size);

    /* Subtract the memory used for the new allocation */
    resultingblk_size -= (METADATA_SIZE + METADATA_PAD + new_size +
                    get_padding(new_size));

    /* Subtract the memory that will be used for the block_meta header of the 
    chunk */
    resultingblk_size -= (METADATA_SIZE + METADATA_PAD);

    /* Calculate the total space ocuppied by the new allocation */
    size_t newblk_total_size = METADATA_SIZE + METADATA_PAD + new_size +
                               get_padding(new_size);

    /* Move a pointer to the start of the resulting free block */   
    block_meta_t *free_block = (block_meta_t *)((char *)unused_block +
                                newblk_total_size);

    /* Set the free block fields */
    free_block->size = resultingblk_size;
    free_block->status = STATUS_FREE;
    
    /* Set the connections of the free block */
    free_block->prev = unused_block;
    free_block->next = unused_block->next;

    /* Modify the remaining connections */
    if (unused_block->next)
        unused_block->next->prev = free_block;
    
    unused_block->next = free_block;

    /* Set the fields of the allocated block */
    unused_block->size = new_size;
    unused_block->status = STATUS_ALLOC;

    return free_block;
}

int is_in_list(block_meta_t *block)
{
    block_meta_t *iterator = &head;
    while (iterator) {
        if (iterator == block)
            return 1;
    }

    return 0;
}

// ---------- ALLOCATION RELATED FUNCTIONS ----------

block_meta_t *alloc_new_block(size_t payload_size, alloc_type_t sys_used)
{	
	size_t payload_pad = get_padding(payload_size);
	size_t used_size = 0;

	/* The used size for the new chunk of memory, first consists from the size
	of metadata block and it's padding */
	used_size += METADATA_SIZE + METADATA_PAD;

	/* Then we add the payload size and the padding of the payload */
	used_size += payload_size + payload_pad;

	/* Now, we allocate the new chunk using either sbrk or mmap */
	void *p = NULL;
	if (sys_used == BRK)
		p = sbrk(used_size);
	else
		p = mmap(NULL, used_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

	if (p == MAP_FAILED)
		return NULL;

	/* Convert the pointer to a block_meta_t, because it is easier to work in
	this form */
	block_meta_t *new_block = (block_meta_t *)p;

	/* Set the size to the payload size, not block size, because the paddings
	can be computed at any time knowing the size of the block */
	new_block->size = payload_size;

	/* Set the block status */
	if (sys_used == BRK)
		new_block->status = STATUS_ALLOC;
	else
		new_block->status = STATUS_MAPPED;

	/* The new block should have no connections */
	new_block->prev = NULL;
	new_block->next = NULL;

	return new_block;
}

block_meta_t *prealloc_heap()
{
	/* Get a bigger chunk of memory generated by brk syscall */
    block_meta_t *premem = alloc_new_block(HEAP_PREALLOCATION_SIZE, BRK);
    
    if (premem == NULL)
        return NULL;

    /* Mark the chunk as free */
    premem->status = STATUS_FREE;
    return premem;
}

// ---------- DEALLOCATION RELATED FUNCTIONS ----------

void mark_freed(block_meta_t *block)
{
    block->status = STATUS_FREE;
}

void extract_block(block_meta_t *block)
{
    block_meta_t *prev = block->prev;
    block_meta_t *next = block->next;

    /* If prev is NULL and next is NULL, then the block is the only block in 
    Memory List: In this case,  we just reset the head */
    if (!prev && !next) {
        head.size = 0;
        head.status = STATUS_FREE;
        head.prev = NULL;
        head.next = NULL;
        return;
    }

    /* If just prev is NULL, then the block is the head of the Memory List: In
    this case, we make the next block the head */
    if (!prev) {
        head.size = block->size;
        head.status = block->status;
        head.prev = NULL;
        head.next = block->next;
        return;
    }

    /* If next is NULL, then the block is the tail of the Memory List: In this
    case, we have to break the connexion between prev and block */
    if (!next) {
        prev->next = NULL;
        return;
    }

    next->prev = prev;
    prev->next = next;

    /* For more safety break the connections of the block */
    block->prev = NULL;
    block->next = NULL;
}

int free_mmaped_block(block_meta_t *block)
{
    size_t length = METADATA_SIZE + METADATA_PAD + block->size +
                        get_padding(block->size);

    return munmap(block, length);
}

// ---------- HELPERS ----------

size_t get_padding(size_t chunk_size)
{
	size_t floor = chunk_size / ALIGN_SIZE;

	/* If chunk_size is a multiple of ALIGN_SIZE */
	if (floor * ALIGN_SIZE == chunk_size)
		return 0;

	size_t top_align = (floor + 1) * ALIGN_SIZE;

	return top_align - chunk_size;
}

void *get_address_by_block(block_meta_t *block)
{
    void *p = (void *)block;
    return (char *)p + METADATA_SIZE + METADATA_PAD;
}

block_meta_t *get_block_by_address(void *addr)
{
    return (block_meta_t *)((char *)addr - (METADATA_SIZE + METADATA_PAD));
}

// ---------- DEBUGGING FUNCTIONS ----------

void print_block(block_meta_t *block)
{
	if (block == NULL) {
        printf_("NULL BLOCK\n");
        printf_("\n");
        return;
    }
    
    void *p = get_address_by_block(block);
    printf_("BLOCK ADDRESS: %p\n", block);
    printf_("MEMORY ADDRESS: %p\n", p);
	printf_("BLOCK SIZE: %ld + %ld + %ld + %ld\n", METADATA_SIZE, METADATA_PAD, 
            block->size, get_padding(block->size));
	printf_("BLOCK STATUS: ");
	if (block->status == STATUS_ALLOC) {
		printf_("STATUS_ALLOC\n");
	} else if (block->status == STATUS_MAPPED) {
		printf_("STATUS_MAPPED\n");
	} else {
		printf_("STATUS_FREE\n");
	}

    printf_("PREV: %p\n", block->prev);
    printf_("NEXT: %p\n", block->next);
   
    printf_("\n");
}

void print_list()
{
	printf_("<<<< Memory List <%ld> >>>>\n", list_size);
    block_meta_t *item = &head;
	while (item != NULL) {
		print_block(item);
		item = item->next;
	}
}