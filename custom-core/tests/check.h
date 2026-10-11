/* Tiny test harness: counts checks, prints each failure, and the program exits nonzero if any failed. */
#ifndef CC_CHECK_H
#define CC_CHECK_H

#include <stdio.h>

static int cc_checks, cc_failures;

#define CHECK(cond) \
    do { \
        cc_checks++; \
        if (!(cond)) { \
            cc_failures++; \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

/* compares a cc_status (or any int) with its expected value and prints both on a mismatch */
#define CHECK_EQ(got, want) \
    do { \
        long long g_ = (long long)(got), w_ = (long long)(want); \
        cc_checks++; \
        if (g_ != w_) { \
            cc_failures++; \
            printf("FAIL %s:%d: %s = %lld, expected %lld\n", __FILE__, __LINE__, #got, g_, w_); \
        } \
    } while (0)

#define CHECK_STATUS(call, want) CHECK_EQ((call), (want))

static int cc_summary(const char *name)
{
    printf("%s: %d checks, %d failed\n", name, cc_checks, cc_failures);
    return cc_failures ? 1 : 0;
}

#endif
