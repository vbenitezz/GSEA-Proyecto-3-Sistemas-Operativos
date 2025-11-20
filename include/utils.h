#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

char *build_path(const char *dir, const char *file);
int read_random_bytes(unsigned char *buf, size_t n);

#endif
