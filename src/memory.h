
#pragma once

void *my_allocate(unsigned long long bytes);
void my_free(void *p);

void my_memcpy(void *dest, const void *src, size_t bytes);
void my_memset(void *dest, int value, size_t bytes);

