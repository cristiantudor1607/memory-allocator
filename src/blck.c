// SPDX-License-Identifier: BSD-3-Clause

#include "blck.h"

/* ----- MEMORY LIST RELATED FUNCTIONS ----- */

void set_list_head(block_meta_t *block)
{
	head = block;
}

void add_block(block_meta_t *block)
{
	if (!head) {
		set_list_head(block);
        list_size++;
		return;
	}

	/* Go to the end of the list */
	block_meta_t *iterator = head;
	while (iterator->next != NULL)
		iterator = iterator->next;

    /* Make the connexions between block and iterator */
	iterator->next = block;
	block->prev = iterator;

    list_size++;
}

void extract_block(block_meta_t *block)
{
    block_meta_t *prev = block->prev;
    block_meta_t *next = block->next;

    /* If prev is NULL and next is NULL, then the block is the only block in 
    Memory List: In this case,  we just reset the head */
    if (!prev && !next) {
        head->size = 0;
        head->status = STATUS_FREE;
        head->prev = NULL;
        head->next = NULL;
        return;
    }

    /* If just prev is NULL, then the block is the head of the Memory List: In
    this case, we make the next block the head */
    if (!prev) {
        head->size = next->size;
        head->status = next->status;
        head->prev = NULL;
        head->next = next->next;
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

block_meta_t* split_block(block_meta_t *unused_block, size_t payload_size)
{
    /* Calculate the raw size of the block */
    size_t raw_block = BLOCK_ALIGN + ALIGN(unused_block->size);

    /* Calculate the raw size of the new chunk */
    size_t raw_chunk = BLOCK_ALIGN + ALIGN(payload_size);

    size_t free_memory = raw_block - raw_chunk;

    /* Get a pointer to the resulting free block */
    void *p = (void *)((char *)unused_block + raw_chunk);
    block_meta_t *free_block = (block_meta_t *)p;

    /* Set the fields of the allocated chunk */
    unused_block->size = payload_size;
    unused_block->status = STATUS_ALLOC;

    /* Set the fields of the remaining free zone */
    free_block->size = ALIGN(free_memory - BLOCK_ALIGN);
    free_block->status = STATUS_FREE;

    /* Make the connections for the resulting free block */
    free_block->prev = unused_block;
    free_block->next = unused_block->next;

    /* Modify the remaining connections */
    if (unused_block->next)
        unused_block->next->prev = free_block;

    unused_block->next = free_block;

    list_size++;

    return free_block;
}

block_meta_t *find_best_block(size_t size)
{
    block_meta_t *return_block = NULL;
    block_meta_t *iterator = head;

    while (iterator) {
        /* If the chunk isn't marked as free, skip */
        if (iterator->status != STATUS_FREE) {
            iterator = iterator->next;
            continue;
        }

        /* If the current block cannot fit the new memory, skip */
        if (ALIGN(iterator->size) < ALIGN(size)) {
            iterator = iterator->next;
            continue;
        }

        /* If it finds a perfect block */
        if (ALIGN(iterator->size) == ALIGN(size))
            return iterator;
        
        /* If it finds the first fitting zone */
        if (!return_block) {
            return_block = iterator;
            iterator = iterator->next;
            continue;
        }

        /* If it finds a zone more suitable */
        if (ALIGN(return_block->size) > ALIGN(iterator->size)) {
            return_block = iterator;
            iterator = iterator->next;
            continue;
        }

        iterator = iterator->next;   
    }

    return return_block;
}

block_meta_t *get_last_block()
{
    block_meta_t *iterator = head;
    if (iterator == NULL)
        return NULL;

    while (iterator->next != NULL)
        iterator = iterator->next;

    return iterator;
}

/* ----- ALLOCATION RELATED FUNCTIONS ----- */

void *alloc_raw_memory(size_t raw_size, alloc_type_t syscall_type)
{
    void *p;
    if (syscall_type == BRK) {
        p = sbrk(raw_size);
    } else {
        p = mmap(NULL, raw_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    }

    if (p == MAP_FAILED)
        return NULL;

    return p;
}

block_meta_t *alloc_new_block(size_t payload_size)
{	
    /* Calculated the raw memory size that will be used for the payload */
    size_t raw_size = BLOCK_ALIGN + ALIGN(payload_size);

    /* Get the memory using either sbrk or mmap, depending on required size */
    void *p;
    if (raw_size <= MMAP_THRESHOLD) {
        p = alloc_raw_memory(raw_size, BRK);
    } else {
        p = alloc_raw_memory(raw_size, MMAP);
    }

    /* If something went wrong return NULL */
    if (!p)
        return NULL;

    /* Make the header of the zone */
    block_meta_t *block = (block_meta_t *)p;
    block->size = payload_size;
    
    if (raw_size <= MMAP_THRESHOLD)
        block->status = STATUS_ALLOC;
    else
        block->status = STATUS_MAPPED;

    block->prev = NULL;
    block->next = NULL;

    return block;
}

block_meta_t *prealloc_heap()
{
    /* Get a relative big chunk of memory generated by sbrk */
    void *p = alloc_raw_memory(HEAP_PREALLOCATION_SIZE, BRK);

    if (!p)
        return NULL;

    block_meta_t *preallocated_zone = (block_meta_t *)p;

    /* The free space of the zone will not be exactly 128 bytes */
    preallocated_zone->size = HEAP_PREALLOCATION_SIZE - BLOCK_ALIGN;
    preallocated_zone->status = STATUS_FREE;

    return preallocated_zone;
}

void *extend_heap(size_t size)
{
    void *p = sbrk(size);
    if (p == MAP_FAILED)
        return NULL;

    return p;
}

block_meta_t *reuse_block(size_t size)
{
    if (!head)
        return NULL;
    
    size_t raw_size = BLOCK_ALIGN + ALIGN(size);
    if (raw_size > MMAP_THRESHOLD)
        return NULL;

    /* Get the tail of the list, to expand it if possible */
    block_meta_t *tail = get_last_block();

    /* Find a fitting block*/
    block_meta_t *block = find_best_block(size);

    /* If nothing was found, and the tail isn't free, there's nothing
    to do */
    if (!block && tail->status != STATUS_FREE) 
        return NULL;

    /* If the tail is free, and it didin't find any block */
    if (!block) {
        block_meta_t *ptr = tail;
        void *new_zone = extend_heap(ALIGN(size - tail->size));
        DIE(!new_zone, "failed to extend the heap\n");
        ptr->size = size;
        ptr->status = STATUS_ALLOC;
        return ptr;
    }

    if (block->size == size) {
        block->status = STATUS_ALLOC;
        return block;
    }

    if (get_raw_reusable_memory(block, size) < MIN_SPACE) {
        block->status = STATUS_ALLOC;
        return block;
    }

    split_block(block, size);
    return block;
}

/* ----- DEALLOCATION RELATED FUNCTIONS ----- */

void mark_freed(block_meta_t *block)
{
    if (block->status != STATUS_ALLOC)
        DIE(1, "Invalid call of function\n");

    block->status = STATUS_FREE;
}

int free_mmaped_block(block_meta_t *block)
{
    size_t length = BLOCK_ALIGN + ALIGN(block->size);
    return munmap((void *)block, length);
}

/* ----- OTHER FUNCTIONS ----- */

void *get_address_by_block(block_meta_t *block)
{
    return (void *)((char *)block + BLOCK_ALIGN);
}

block_meta_t *get_block_by_address(void *addr)
{
    return (block_meta_t *)((char *)addr - BLOCK_ALIGN);
}

size_t get_raw_reusable_memory(block_meta_t *block, size_t new_size)
{
    /* Calculate the raw size for the new memory*/
    size_t raw_size = BLOCK_ALIGN + ALIGN(new_size);

    /* Calculate the raw size of the block */
    size_t capacity = BLOCK_ALIGN + ALIGN(block->size);

    return capacity - raw_size;
}

/* ----- DEBUGGING FUNCTIONS ----- */
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
	printf_("BLOCK SIZE: %ld / %ld\n", block->size, BLOCK_ALIGN + ALIGN(block->size));
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
    block_meta_t *item = head;
    if (item == NULL) {
        printf_("Memory List is Empty\n");
        return;
    }

	while (item != NULL) {
		print_block(item);
		item = item->next;
	}
}