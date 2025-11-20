#include "../../include/utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char *build_path(const char *dir, const char *file) {
    size_t len = strlen(dir) + strlen(file) + 2;
    char *out = malloc(len);
    if (!out) return NULL;

    snprintf(out, len, "%s/%s", dir, file);
    return out;
}