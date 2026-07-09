#include "vac_filter.h"
#include <stdlib.h>
#include <string.h>

/* Size of the median filter and the running average, same as the original. */
#define NUM_VAC_VALUES 15
#define NUM_VAC_AVG    15

static int cmpfunc(const void *a, const void *b)
{
	uint16_t va = *(const uint16_t *)a;
	uint16_t vb = *(const uint16_t *)b;

	if (va > vb) {
		return 1;
	} else if (va == vb) {
		return 0;
	}
	return -1;
}

/*
 * A new value overwrites the oldest in a ring buffer. A sorted copy yields the
 * median (rejects "flyer" values); the median is then fed through a running
 * average to smooth it. Slow-moving vacuum values are the norm, so the added
 * latency is acceptable.
 */
uint16_t vac_filter_push(uint16_t raw)
{
	static uint16_t values_raw[NUM_VAC_VALUES];
	static uint16_t values_sorted[NUM_VAC_VALUES];
	static uint8_t  idx;
	static uint32_t avg;

	values_raw[idx] = raw;
	/* FIX: original wrapped on '> NUM_VAC_VALUES', letting idx reach 15 on a
	 * 15-element array (out-of-bounds write). Correct comparison is '>='. */
	if (++idx >= NUM_VAC_VALUES) {
		idx = 0;
	}

	memcpy(values_sorted, values_raw, sizeof(values_sorted));
	qsort(values_sorted, NUM_VAC_VALUES, sizeof(uint16_t), cmpfunc);

	avg = (avg * (NUM_VAC_AVG - 1) + values_sorted[NUM_VAC_VALUES / 2]) / NUM_VAC_AVG;
	return (uint16_t)avg;
}
