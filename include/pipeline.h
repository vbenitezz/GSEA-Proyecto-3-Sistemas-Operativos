#ifndef PIPELINE_H
#define PIPELINE_H

#include <semaphore.h>
#include <stddef.h>

// Para OperationType
typedef enum {
    OP_NONE = 0,
    OP_COMPRESS,
    OP_DECOMPRESS,
    OP_ENCRYPT,
    OP_DECRYPT
} OperationType;

// ⭐ Nuevo: Algoritmo de compresión
typedef enum {
    COMP_LZW = 0,
    COMP_RLE
} CompressionAlgo;

/*
 * ThreadArgs:
 * Estructura que se pasa a cada hilo que procesa un archivo.
 */
typedef struct {
    char *input_file_path;     // Se libera dentro del thread
    char *output_file_path;    // Puede ser temporal; se libera dentro del thread
    char *key;                 // No se duplica; NO liberar en thread
    OperationType sequence[4]; // Secuencia de operaciones c,d,e,u
    sem_t *limiter;            // Puede ser NULL si no hay límite de concurrencia

    // ⭐ Nuevo: algoritmo de compresión seleccionado desde main con -a rle|lzw
    CompressionAlgo algo;

} ThreadArgs;

#endif