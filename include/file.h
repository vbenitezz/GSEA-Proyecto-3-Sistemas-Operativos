#ifndef FILE_H
#define FILE_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>

// Abre un archivo. 'flags' es como O_RDONLY, O_WRONLY, etc.
int safe_open(const char *path, int flags, mode_t mode);

// Lee EXACTAMENTE n bytes, a menos que termine el archivo.
// Devuelve cantidad leída, o -1 si error.
ssize_t safe_read(int fd, void *buffer, size_t n);

// Escribe EXACTAMENTE n bytes, aunque write() escriba menos.
// Devuelve 0 si ok, -1 si error.
int safe_write(int fd, const void *buffer, size_t n);

// Cierra un archivo.
int safe_close(int fd);

// Carga un archivo COMPLETO en memoria (malloc).
unsigned char *read_file_complete(const char *path, size_t *size_out);

#endif