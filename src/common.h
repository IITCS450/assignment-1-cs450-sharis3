#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>

/* Print message and exit */
static inline void die(const char *msg) {
    perror(msg);
    exit(1);
}

#endif
