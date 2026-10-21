/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TESTS_H_
#define TESTS_H_

#include <zephyr/ztest.h>

#include <math.h>
#include <time.h>

/**
 * Assert that two floating point values are equal within a relative precision of 1e-5
 *
 * Same behavior as TEST_ASSERT_EQUAL_FLOAT of the previously used Unity framework.
 */
#define assert_float_equal(actual, expected) \
    zassert_within((double)(actual), (double)(expected), fabs((double)(expected)) * 1e-5, \
                   #actual " not equal to " #expected)

#endif /* TESTS_H_ */
