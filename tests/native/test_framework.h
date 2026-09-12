// tests/native/test_framework.h
#pragma once
#include <cstdio>

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define CHECK(cond) do { \
    g_tests_run++; \
    if (!(cond)) { \
        g_tests_failed++; \
        printf("FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
    } \
} while (0)

#define CHECK_EQ(a, b) do { \
    g_tests_run++; \
    if (!((a) == (b))) { \
        g_tests_failed++; \
        printf("FAIL: %s == %s (%s:%d)\n", #a, #b, __FILE__, __LINE__); \
    } \
} while (0)

#define TEST_SUMMARY() do { \
    printf("%d/%d checks passed\n", g_tests_run - g_tests_failed, g_tests_run); \
    return g_tests_failed == 0 ? 0 : 1; \
} while (0)
