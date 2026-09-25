#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

static int g_testsRun = 0;
static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << #cond << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(actual, expected) \
    do { \
        if ((actual) != (expected)) { \
            std::cerr << "  FAILED: Expected '" << (expected) << "', but got '" << (actual) \
                      << "' (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            return false; \
        } \
    } while(0)

#define RUN_TEST(testFunc) \
    do { \
        g_testsRun++; \
        std::cout << "[RUN ] " << #testFunc << " ... "; \
        if (testFunc()) { \
            std::cout << "PASSED\n"; \
            g_testsPassed++; \
        } else { \
            std::cout << "FAILED\n"; \
            g_testsFailed++; \
        } \
    } while(0)
