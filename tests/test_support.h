#ifndef CROWNLESS_TEST_SUPPORT_H
#define CROWNLESS_TEST_SUPPORT_H

#include <stdio.h>
#include <stdlib.h>


#define CC_CHECK(expression)                                                   \
    do {                                                                       \
        if (!(expression)) {                                                   \
            (void)fprintf(stderr, "%s:%d: check failed: %s\n",               \
                          __FILE__, __LINE__, #expression);                    \
            exit(EXIT_FAILURE);                                                \
        }                                                                      \
    } while (0)

#endif
