#include "tools/bitmap.h"
#include "tools/klib.h"

int bitmap_byte_count(int bit_count) {
	return (bit_count + 7) / 8;
}

void bitmap_init(bitmap_t *bitmap, uint8_t *bits, int bit_count, int init_bit) {
	bitmap->bit_count = bit_count;
	bitmap->bits = bits;

	int bytes = bitmap_byte_count(bitmap->bit_count);
	kernel_memset(bits, init_bit ? 0xFF : 0, bytes);
}

int bitmap_get_bit(bitmap_t *bitmap, int index) {
	return (bitmap->bits[index / 8] >> (index % 8)) & 0x1;
}

void bitmap_set_bit(bitmap_t *bitmap, int index, int count, int bit) {
	for (int i = 0; i < count && index < bitmap->bit_count; i++, index++) {
		if (bit) {
			bitmap->bits[index / 8] |= (1 << (index % 8));
		} else {
			bitmap->bits[index / 8] &= ~(1 << (index % 8));
		}
	}
}

int bitmap_is_set(bitmap_t *bitmap, int index) {
	return bitmap_get_bit(bitmap, index) ? 1 : 0;
}

int bitmap_alloc_nbits(bitmap_t *bitmap, int bit, int count) {
	int search_idx = 0;
	int ok_index = -1;
	while (search_idx < bitmap->bit_count) {
		if (bitmap_get_bit(bitmap, search_idx) != bit) {
			search_idx++;
			continue;
		}
		ok_index = search_idx;
		int i = 1;
		for (; i < count && search_idx < bitmap->bit_count; ++i) {
			if (bitmap_get_bit(bitmap, search_idx++) != bit) {
				ok_index = -1;
				break;
			}
		}

		if (i >= count) {
			bitmap_set_bit(bitmap, ok_index, count, ~bit);
			return ok_index;
		}
	}
	return -1;
}