#ifndef ASSERTIONS_H
#define ASSERTIONS_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/** Asserts that the given expression is a truthy value. */
#define ASSERT_TRUE(expr)                                                      \
    if (!(expr)) {                                                             \
        fprintf(                                                               \
            stderr,                                                            \
            "Assertion failed at line %d: `%s`\n",                             \
            __LINE__,                                                          \
            #expr                                                              \
        );                                                                     \
        return EXIT_FAILURE;                                                   \
    }

/** Asserts that the given expression is a falsy value. */
#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

/** Asserts that the two given expressions are equal using `==`. */
#define ASSERT_EQ(left, right) ASSERT_TRUE(left == right)

/** Asserts that the two given floating-point values are equal under the given
 * epsilon value. */
#define ASSERT_FLOAT_EQ(left, right, epsilon)                                  \
    ASSERT_TRUE(fabs(left - right) < epsilon)

/** Asserts that the two given expressions are not equal using `!=`. */
#define ASSERT_NEQ(left, right) ASSERT_TRUE(left != right)

#endif
