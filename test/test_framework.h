// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        printf("Assertion failed: %s, function %s, file %s, line %d.\n", #expr, __func__, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_NOT_EQUAL(a, b) do { \
    if ((a) == (b)) { \
        printf("Assertion failed: %s != %s, function %s, file %s, line %d.\n", #a, #b, __func__, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

#define ASSERT_EQUAL(a, b) do { \
    if ((a) != (b)) { \
        printf("Assertion failed: %s == %s, function %s, file %s, line %d.\n", #a, #b, __func__, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

#define ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        printf("Assertion failed: %s != NULL, function %s, file %s, line %d.\n", #ptr, __func__, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

#define ASSERT_NULL(ptr) do { \
    if ((ptr) != NULL) { \
        printf("Assertion failed: %s == NULL, function %s, file %s, line %d.\n", #ptr, __func__, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

#define RUN_TEST(test) do { \
    printf("Running %s...\n", #test); \
    if (test() != 0) { \
        printf("%s failed.\n", #test); \
        return 1; \
    } else { \
        printf("%s passed.\n", #test); \
    } \
} while (0)

#endif // TEST_FRAMEWORK_H