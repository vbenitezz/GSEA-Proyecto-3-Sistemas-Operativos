#include "../../include/utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

char *build_path(const char *dir, const char *file) {
    size_t len = strlen(dir) + strlen(file) + 2;
    char *out = malloc(len);
    if (!out) return NULL;

    snprintf(out, len, "%s/%s", dir, file);
    return out;
}

int read_random_bytes(unsigned char *buf, size_t n) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        perror("[read_random_bytes] open");
        return 1;
    }

    ssize_t r = read(fd, buf, n);
    close(fd);

    if (r != (ssize_t)n) {
        fprintf(stderr, "[read_random_bytes] Error leyendo %zu bytes\n", n);
        return 1;
    }

    return 0;
}