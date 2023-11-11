/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <errno.h>
#include <stdio.h>
#include "printf.h"

#define DIE(assertion, call_description)									\
	do {													\
		if (assertion) {										\
			fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);					\
			perror(call_description);								\
			exit(errno);										\
		}												\
	} while (0)

/* Structure to hold memory block metadata */
struct block_meta  {
	size_t size;
	int status;
	struct block_meta *prev;
	struct block_meta *next;
};
typedef struct block_meta block_meta_t;

/* Enum used to separate the 2 types of allocations */
enum alloc_type {
	BRK,
	MMAP
};
typedef enum alloc_type alloc_type_t;

/* Block metadata status values */
#define STATUS_FREE   0
#define STATUS_ALLOC  1
#define STATUS_MAPPED 2

/* Some defines imported from tests/snippets/test-utils.h */

#define METADATA_SIZE		(sizeof(struct block_meta))
#define MOCK_PREALLOC		(128 * 1024 - METADATA_SIZE - 8)
#define MMAP_THRESHOLD		(128 * 1024)
#define NUM_SZ_SM		11
#define NUM_SZ_MD		6
#define NUM_SZ_LG		4
#define MULT_KB			1024

/* New defines */
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))
#define BLOCK_ALIGN ALIGN(sizeof(block_meta_t))

#define MIN_SPACE (BLOCK_ALIGN + ALIGN(1))

#define HEAP_PREALLOCATION_SIZE (128 * 1024)
#define DONE 1
#define NOT_DONE 0




