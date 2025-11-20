#include "../../include/file.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int safe_open(const char *path, int flags, mode_t mode) {
    int fd = open(path, flags, mode);
    if (fd < 0) {
        perror("[safe_open] Error al abrir archivo");
    }
    return fd;
}

ssize_t safe_read(int fd, void *buffer, size_t n) {
    size_t total = 0;

    while (total < n) {
        ssize_t bytes = read(fd, (char*)buffer + total, n - total);

        if (bytes < 0) {
            perror("[safe_read] Error al leer archivo");
            return -1;
        }
        if (bytes == 0) {
            break; // EOF
        }

        total += bytes;
    }

    return total;
}

int safe_write(int fd, const void *buffer, size_t n) {
    size_t written = 0;

    while (written < n) {
        ssize_t bytes = write(fd, (char*)buffer + written, n - written);

        if (bytes < 0) {
            perror("[safe_write] Error al escribir archivo");
            return -1;
        }

        written += bytes;
    }
    return 0;
}

int safe_close(int fd) {
    if (close(fd) < 0) {
        perror("[safe_close] Error al cerrar archivo");
        return -1;
    }
    return 0;
}

unsigned char *read_file_complete(const char *path, size_t *size_out) {
    int fd = safe_open(path, O_RDONLY, 0);
    if (fd < 0) return NULL;

    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("[read_file_complete] Error al obtener tamaño del archivo");
        safe_close(fd);
        return NULL;
    }

    size_t size = st.st_size;
    unsigned char *buffer = malloc(size);
    if (!buffer) {
        perror("[read_file_complete] malloc");
        safe_close(fd);
        return NULL;
    }

    ssize_t r = safe_read(fd, buffer, size);
    if (r < 0 || (size_t)r != size) {
        fprintf(stderr, "[read_file_complete] Error al leer archivo completo\n");
        free(buffer);
        safe_close(fd);
        return NULL;
    }

    safe_close(fd);
    *size_out = size;
    return buffer;
}

/* --------- NUEVO: escribir buffer completo ----------- */
int write_buffer_to_file(const char *path,
                         const unsigned char *buf,
                         size_t size)
{
    int fd = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return 1;

    if (safe_write(fd, buf, size) != 0) {
        safe_close(fd);
        return 1;
    }

    if (safe_close(fd) != 0) return 1;

    return 0;
}