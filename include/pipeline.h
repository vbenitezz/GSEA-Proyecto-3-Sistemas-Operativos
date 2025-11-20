#ifndef PIPELINE_H
#define PIPELINE_H

#include <semaphore.h>
#include <stddef.h>

typedef enum {
    OP_NONE = 0,
    OP_COMPRESS,
    OP_DECOMPRESS,
    OP_ENCRYPT,
    OP_DECRYPT
} OperationType;

typedef struct {
    char *input_file_path;   // se liberan dentro del thread
    char *output_file_path;  // ruta final o ruta temporal (no liberar si apunta a original de caller)
    char *key;               // puntero a clave (no duplicado)
    OperationType sequence[4];
    sem_t *limiter;          // semáforo para limitar concurrencia (puede ser NULL)
} ThreadArgs;

#endif