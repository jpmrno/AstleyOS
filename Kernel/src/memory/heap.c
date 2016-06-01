#include <heap.h>
#include <numbers.h>
#include <memory.h>

/**
 * Buddy implementation from: https://github.com/cloudwu/buddy
 *
 * MORE INFO:
 * http://bitsquid.blogspot.com.ar/2015/08/allocation-adventures-3-buddy-allocator.html
 */

#define _MEMORY_PAGE_SIZE	KILOBYTES(4) // TODO: En otro lado

#define KILOBYTE 			1024				// TODO: En otro lado todos
#define KILOBYTES(x) 		((x) * KILOBYTE)
#define MEGABYTE 			1048576
#define MEGABYTES(x) 		((x) * MEGABYTE)

// To change heap size, change:
#define HEAP_ALLOC_SIZE		MEGABYTES(512)
// Remember to change kernel's/userland's initial memory location
// ----------------------------

// To change heap location, change:
#define HEAP_MEMORY_BASE	0x100000
// Remember to change kernel's/userland's initial memory location
// ----------------------------

#define HEAP_MEMORY_SIZE	(HEAP_ALLOC_SIZE + HEAP_STRUCT_SIZE)
#define HEAP_MEMORY_END		(HEAP_MEMORY_BASE + HEAP_MEMORY_SIZE - 1)

#define HEAP_ALLOC_BASE		HEAP_MEMORY_BASE

#define HEAP_STRUCT_SIZE	sizeof(allocator_st)
#define HEAP_STRUCT_BASE	(HEAP_ALLOC_BASE + HEAP_ALLOC_SIZE)
#define HEAP_STRUCT_LEAF	_MEMORY_PAGE_SIZE
#define HEAP_STRUCT_TREE	(HEAP_ALLOC_SIZE / HEAP_STRUCT_LEAF) * 2 - 1

#define NODE_UNUSED 		0
#define NODE_USED 			1
#define NODE_SPLIT 			2
#define NODE_FULL 			3

typedef struct {
	unsigned int size;
	int levels;
	uint8_t tree[HEAP_STRUCT_TREE];
} allocator_st;

static int buddy_alloc(unsigned int size);
static void buddy_free(unsigned int offset);
static int buddy_offset(int index, int level);
static void buddy_markParent(int index);
static void buddy_combine(int index);

static allocator_st * allocator; // = {(1 << 17), 17, {NODE_UNUSED}}; // TODO: Calcular el 17 & Inicializar en 0 tree

void heap_init(void) {
	// memset(HEAP_ALLOC_BASE, 0, HEAP_ALLOC_SIZE);
	allocator = (allocator_st *) HEAP_STRUCT_BASE;

	allocator->size = (1 << 17); 	// TODO: Calcular el 17 y define
	allocator->levels = 17;			// TODO: Calcular el 17 y define
	memset(allocator->tree, NODE_UNUSED, HEAP_STRUCT_TREE);
}

void * heap_alloc(unsigned int size) {
	int offset;

	if(!size) {
		return NULL;
	}

	size = size / HEAP_STRUCT_LEAF + 1;
	offset = buddy_alloc(size);
	if(offset == -1) {
		return NULL;
	}

	return (void *) ((intptr_t) (HEAP_ALLOC_BASE + offset * HEAP_STRUCT_LEAF));
}

void * head_zalloc(unsigned int size) {
	void * addr;

	addr = heap_alloc(size);
	if(addr == NULL) {
		return NULL;
	}
	memset(addr, 0, size);

	return addr;
}

void heap_free(void * adrr) {
	buddy_free(((intptr_t) adrr - HEAP_ALLOC_BASE) / HEAP_STRUCT_LEAF);
}

static int buddy_alloc(unsigned int size) {
	int index = 0, level = 0;
	unsigned int length = allocator->size;

	if(size == 0) {
		return -1;
	}

	size = numnextPow2(size);

	if(size > length) {
		return -1;
	}

	while(index >= 0) {
		if(size == length) {
			if(allocator->tree[index] == NODE_UNUSED) {
				allocator->tree[index] = NODE_USED;
				buddy_markParent(index);
				return buddy_offset(index, level);
			}
		} else {
			// size < length
			switch(allocator->tree[index]) {
				case NODE_USED:
				case NODE_FULL:
					break;

				case NODE_UNUSED:
					// split first
					allocator->tree[index] = NODE_SPLIT;
					allocator->tree[index * 2 + 1] = NODE_UNUSED;
					allocator->tree[index * 2 + 2] = NODE_UNUSED;

				default:
					index = index * 2 + 1;
					length /= 2;
					level++;
					continue;
			}
		}

		if(index & 1) {
			++index;
			continue;
		}

		for(;;) {
			level--;
			length *= 2;
			index = (index + 1) / 2 - 1;

			if(index < 0) {
				return -1;
			}

			if(index & 1) {
				++index;
				break;
			}
		}
	}

	return -1;
}

static void buddy_free(unsigned int offset) {
	int index = 0;
	unsigned int length = allocator->size, left = 0;

	if(offset >= length) {
		// TODO: This should never happend
		return;
	}

	for(;;) {
		switch(allocator->tree[index]) {
			case NODE_USED:
				if(offset != left) {
					// TODO: This should never happend
					return;
				}
				buddy_combine(index);
				return;

			case NODE_UNUSED:
				// TODO: This should never happend
				return;

			default:
				length /= 2;
				if(offset < left + length) {
					index = index * 2 + 1;
				} else {
					left += length;
					index = index * 2 + 2;
				}
				break;
		}
	}
}

static int buddy_offset(int index, int level) {
	return ((index + 1) - (1 << level)) << (allocator->levels - level);
}

static void buddy_markParent(int index) {
	int buddy;

	for(;;) {
		buddy = index - 1 + (index & 1) * 2;

		if(buddy > 0 && (allocator->tree[buddy] == NODE_USED ||	allocator->tree[buddy] == NODE_FULL)) {
			index = (index + 1) / 2 - 1;
			allocator->tree[index] = NODE_FULL;
		} else {
			return;
		}
	}
}

static void buddy_combine(int index) {
	int buddy;

	for(;;) {
		buddy = index - 1 + (index & 1) * 2;

		if(buddy < 0 || allocator->tree[buddy] != NODE_UNUSED) {
			allocator->tree[index] = NODE_UNUSED;

			while(((index = (index + 1) / 2 - 1) >= 0) &&  allocator->tree[index] == NODE_FULL) {
				allocator->tree[index] = NODE_SPLIT;
			}

			return;
		}

		index = (index + 1) / 2 - 1;
	}
}
