/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CAPTURE_H_
#define CAPTURE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* maximum number of edges recorded per signal */
#define CAPTURE_MAX_EDGES 32

struct capture_edge
{
    uint32_t time; /* capture timer counter value */
    bool level;    /* signal level after the edge */
};

struct capture_signal
{
    struct capture_edge edges[CAPTURE_MAX_EDGES];
    size_t num_edges;
    bool level_at_start;
    bool level_at_end;
};

struct capture_result
{
    struct capture_signal hs;
    struct capture_signal ls;
    /* an edge was missed because the capture register was not read in time */
    bool overrun;
};

/**
 * PWM timing derived from the captured edges (all values in nanoseconds)
 */
struct capture_timing
{
    /* number of complete high-side periods */
    int num_periods;
    uint32_t period_avg;
    uint32_t hs_on_avg;
    uint32_t ls_on_avg;
    /* number of intervals with both outputs off (should be 2 per period) */
    int num_deadtimes;
    uint32_t deadtime_min;
    uint32_t deadtime_max;
    /* total time with both outputs on */
    uint32_t overlap;
    /* captured edges are inconsistent (e.g. two rising edges in a row) */
    bool invalid;
};

/**
 * Initialize the capture timer and apply the pin configuration of the loop-back inputs
 *
 * @retval 0 on success
 * @retval -errno otherwise
 */
int capture_init(void);

/**
 * Record edges of the high-side and low-side signal
 *
 * Recording stops if CAPTURE_MAX_EDGES were captured for both signals or the timeout expired.
 * Interrupts are locked while recording.
 *
 * @param result Captured edges
 * @param timeout_us Maximum recording time in microseconds
 */
void capture_record(struct capture_result *result, uint32_t timeout_us);

/**
 * Calculate the PWM timing from captured edges
 *
 * @param result Captured edges
 * @param timing Calculated timing
 */
void capture_analyze(const struct capture_result *result, struct capture_timing *timing);

#endif /* CAPTURE_H_ */
