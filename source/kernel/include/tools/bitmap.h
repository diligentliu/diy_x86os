#ifndef OS_BITMAP_H
#define OS_BITMAP_H

#include "comm/types.h"

typedef struct _bitmap_t {
	int bit_count;
	uint8_t *bits;
} bitmap_t;

int bitmap_byte_count(int bit_count);
void bitmap_init(bitmap_t *bitmap, uint8_t *bits, int bit_count, int init_bit);
int bitmap_get_bit(bitmap_t *bitmap, int index);
void bitmap_set_bit(bitmap_t *bitmap, int index, int count, int bit);
int bitmap_is_set(bitmap_t *bitmap, int index);
int bitmap_alloc_nbits(bitmap_t *bitmap, int bit, int count);

#endif //OS_BITMAP_H
