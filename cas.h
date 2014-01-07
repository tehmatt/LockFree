/*
 * CAS related stuff - GCC specific
 */

#include <stdint.h>

typedef unsigned __int128 uint128_t;
#define CAS __sync_bool_compare_and_swap

static inline int CAS2(void* v_mem, void* old_data, uint128_t old_count,
		void* new_data, uint128_t new_count) {
	uint128_t* mem = (uint128_t*) v_mem;
	uint128_t old = ((uint128_t) (uintptr_t) old_data) | (old_count << 64);
	uint128_t new = ((uint128_t) (uintptr_t) new_data) | (new_count << 64);
	return CAS(mem, old, new);
}
