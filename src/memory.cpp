
#include "memory.h"

#include <stdlib.h>
#include <string.h>


void *my_allocate(unsigned long long bytes)
{
    return malloc(bytes);
}

void my_free(void *p)
{
    free(p);
}

void my_memcpy(void *dest, const void *src, size_t bytes)
{
    memcpy(dest, src, bytes);
}

void my_memset(void *dest, int value, size_t bytes)
{
    memset(dest, value, bytes);
}

