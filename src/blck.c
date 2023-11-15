// SPDX-License-Identifier: BSD-3-Clause

#include "blck.h"

/* ----- MEMORY LIST RELATED FUNCTIONS ----- */

void set_list_head(block_meta_t *block)
{
	head = block;
}

block_meta_t *get_last_heap()
{
    if (!head)
        return NULL;

    block_meta_t *iter = head;
    while (iter->next)
        iter = iter->next;

    if (iter->status == STATUS_MAPPED)
        return NULL;

    return iter;
}

block_meta_t *get_heap_start()
{
    if (!head)
        return NULL;

    block_meta_t *iter = head;
    while (iter) {
        if (iter->status == STATUS_ALLOC)
            return iter;

        iter = iter->next;
    }

    return NULL;
}

block_meta_t *get_last_mmap()
{
    /* If Memory List is empty */
    if (!head)
        return NULL;

    /* If Memory List doesn't contain any maped block */
    if (head->status == STATUS_ALLOC)
        return NULL;

    /* Find the last maped block */
    block_meta_t *iter = head;
    while (iter->next != NULL) {
        if (iter->next->status == STATUS_ALLOC)
            break;

        iter = iter->next;
    }

    return iter;    
}

block_meta_t *get_last_block()
{
    if (!head)
        return NULL;

    block_meta_t *iter = head;
    while (iter->next)
        iter = iter->next;

    return iter;
}

void insert_mmaped_block(block_meta_t *block)
{
    /* When the list is empty, set it's head */
    if (!head) {
        set_list_head(block);
        return;
    }

    block_meta_t *last_mapped = get_last_mmap();
    
    /* If last_mapped is NULL, then there is no mapped block in the list */
    if (!last_mapped) {
        /* Make it the head */
        block->prev = NULL;
        block->next = head;
        head->prev = block;
        head = block;
        return;
    }

    block_meta_t *first_allocd = last_mapped->next;

    block->prev = last_mapped;
    block->next = first_allocd;
    last_mapped->next = block;
    if (first_allocd)
        first_allocd->prev = block;
    
}

void insert_heap_block(block_meta_t *block)
{
    /* When the list is empty, set it's head */
    if (!head) {
        set_list_head(block);
        return;
    }

    /* A new heap block will always go to the end of the list */
    block_meta_t *last_block = get_last_block();

    block->next = NULL;
    block->prev = last_block;
    last_block->next = block;
}

void add_block(block_meta_t *block)
{
	if (block->status == STATUS_ALLOC)
        insert_heap_block(block);
    else
        insert_mmaped_block(block);

    list_size++;
}

void extract_block(block_meta_t *block)
{
    block_meta_t *prev = block->prev;
    block_meta_t *next = block->next;

    /* If prev is NULL and next is NULL, then the block is the only block in 
    Memory List: In this case,  we just reset the head */
    if (!prev && !next) {
        head = NULL;
        return;
    }

    /* If just prev is NULL, then the block is the head of the Memory List: In
    this case, we make the next block the head */
    if (!prev) {
        head = head->next;
        head->prev = NULL;
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

    /* Calculate the raw size that the chunk will have, as if there 
    was a new block */
    size_t raw_chunk = BLOCK_ALIGN + ALIGN(payload_size);

    /* The memory remaining to create a new block and data */
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

block_meta_t *alloc_new_block(size_t payload_size, size_t limit)
{	
    /* Calculated the raw memory size that will be used for the payload */
    size_t raw_size = BLOCK_ALIGN + ALIGN(payload_size);

    /* Get the memory using either sbrk or mmap, depending on required size */
    void *p;
    if (raw_size <= limit) {
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
    
    if (raw_size <= limit)
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

void *expand_heap(size_t size)
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
    block_meta_t *tail = get_last_heap();
    if (!tail)
        return NULL;

    /* Find a fitting block*/
    block_meta_t *block = find_best_block(size);

    /* If nothing was found, and the tail isn't free, there's nothing
    to do */
    if (!block && tail->status != STATUS_FREE) 
        return NULL;

    /* If the tail is free, and it didin't find any block */
    if (!block) {
        block_meta_t *ptr = tail;
        void *new_zone = expand_heap(ALIGN(size) - ALIGN(tail->size));
        DIE(!new_zone, "failed to expand the heap\n");
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

    /* Daca crapa sterge asta */
    /* If the block was truncated in the past, restore it's size */
    void *start_addr = get_address_by_block(block);
    void *end_addr;
    if (!block->next)
        end_addr = sbrk(0);
    else
        end_addr = (void *)block->next;

    block->size = (size_t)(end_addr - start_addr);

    block->status = STATUS_FREE;
}

void merge_with_next(block_meta_t *block)
{
    block_meta_t *next = block->next;

    /* If next does not exist, there is nothing to do */
    if (!next)
        return;

    /* If it isn't a free block */
    if (next->status != STATUS_FREE)
        return;

    /* Compute the size of the resulting block after merging */
    size_t new_size = ALIGN(block->size) + ALIGN(next->size) + BLOCK_ALIGN;
    
    block_meta_t *new_next = next->next;

    /* If new_next exists, then it's prev pointer should point to the header of
    the block, because it will become the header of the big block */
    if (new_next)
        new_next->prev = block;

    block->next = new_next;
    block->size = new_size;

}

void merge_with_prev(block_meta_t *block)
{
    block_meta_t *prev = block->prev;

    /* If it doesn't exist, there is nothing to do */
    if (!prev)
        return;

    /* If it isn't a free block */
    if (prev->status != STATUS_FREE)
        return;

    /* Compute the size of the resulting block after merging */
    size_t new_size = ALIGN(block->size) + ALIGN(prev->size) + BLOCK_ALIGN;

    block_meta_t *new_next = block->next;

    if (new_next)
        new_next->prev = prev;

    prev->next = new_next;
    prev->size = new_size;
}

void merge_free_blocks(block_meta_t *block)
{
    merge_with_next(block);
    merge_with_prev(block);
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

void *memset_block(block_meta_t *block, int c)
{
    void *p = get_address_by_block(block);
    size_t len = ALIGN(block->size);

    return memset(p, c, len);
}


/* ----- REALLOCATION RELATED FUNCTIONS ----- */

block_meta_t *realloc_mapped_block(block_meta_t *block, size_t size)
{
    /* As a measure of safety, return NULL if the function isn't called with
    a block that wasn't mapped */
    if (block->status != STATUS_MAPPED)
        return NULL;

    size_t raw_size = BLOCK_ALIGN + ALIGN(size);

    block_meta_t *new_block;
    if (raw_size <= MMAP_THRESHOLD && prealloc_done == NOT_DONE) {
        new_block = prealloc_heap();
        if (!new_block)
            return NULL;

        add_block(new_block);
        if (raw_size < MMAP_THRESHOLD && MMAP_THRESHOLD - raw_size >= MIN_SPACE) {
            split_block(new_block, size);
        } else {
            new_block->status = STATUS_ALLOC;
        }

        prealloc_done = DONE;
        return new_block;
    }
    
    /* Search for unused blocks */
    if (raw_size <= MMAP_THRESHOLD) {
        block_meta_t *unused = reuse_block(size);
        if (unused)
            return unused;
    }

    /* Get a new block with the size adjusted */
    new_block = alloc_new_block(size, MMAP_THRESHOLD);
    if (!new_block)
        return NULL;

    add_block(new_block);

    return new_block;
}

block_meta_t *move_to_mmap_space(block_meta_t *block, size_t size)
{
    /* First, some measures of safety */
    if (ALIGN(size) + BLOCK_ALIGN <= MMAP_THRESHOLD)
        return NULL;

    if (block->status != STATUS_ALLOC)
        return NULL;

    /* Get a new block and copy the contents */
    block_meta_t *new_block = alloc_new_block(size, MMAP_THRESHOLD);
    if (!new_block)
        return NULL;

    copy_contents(block, new_block);
    
    /* Add the new block to the Memory List and mark the old one as free */
    add_block(new_block);

    return new_block;
}

block_meta_t *unite_blocks(block_meta_t *block, size_t size)
{
    while (block->next != NULL) {
        if (block->next->status != STATUS_FREE)
            break;
        
        block->size = get_raw_size(block);
        merge_with_next(block);

        if (ALIGN(block->size) >= ALIGN(size))
            return block;
    }

    return NULL;
}

void *truncate_block(block_meta_t *block, size_t new_size)
{
    size_t true_size = get_raw_size(block);
    if (true_size >= ALIGN(new_size)) {
        block->size = new_size;
        return get_address_by_block(block);
    }

    return NULL;
}

block_meta_t *make_space(block_meta_t *block, size_t size)
{
    while (block->next != NULL) {
        if (block->next->status != STATUS_FREE)
            break;
        
        /* Merge with next block */
        merge_with_next(block);

        /* If it matches, stop here */
        if (ALIGN(block->size) >= ALIGN(size))
            return block;
    }

    /* If it reaches this point, then there is no space, not even after
    merging all blocks */
    return NULL;
}

void copy_contents(block_meta_t *src_block, block_meta_t *dest_block)
{
    void *src = get_address_by_block(src_block);
    void *dest = get_address_by_block(dest_block);
    size_t n = src_block->size;
    memcpy(dest, src, n);
}

size_t get_raw_size(block_meta_t *block)
{
    void *start = get_address_by_block(block);
    void *end;
    if (!block->next)
        end = sbrk(0);
    else
        end = (void *)block->next;

    return (size_t)(end - start);
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