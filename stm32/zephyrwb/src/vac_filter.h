/*
 * Median + running-average filter for the analog vacuum sensor.
 * Ported verbatim from getFilteredVacSensor() in the original firmware, with
 * the ring-buffer index off-by-one (OOB write) fixed.
 */
#ifndef VAC_FILTER_H
#define VAC_FILTER_H

#include <stdint.h>

/* Push one raw ADC sample; returns the filtered (median-then-averaged) value. */
uint16_t vac_filter_push(uint16_t raw);

#endif /* VAC_FILTER_H */
