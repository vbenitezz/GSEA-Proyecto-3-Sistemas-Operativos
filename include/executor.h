#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "pipeline.h"   // Para ThreadArgs, CompressionAlgo, OperationType

/**
 * Procesa un único archivo (secuencial o dentro de un hilo).
 * Recibe un ThreadArgs* como void*.
 */
void *process_file_pipeline(void *arg);

/**
 * Procesa todos los archivos regulares dentro de un directorio.
 *
 * Parámetros:
 *   input_dir  -> directorio de entrada
 *   output_dir -> directorio de salida
 *   op_sequence -> arreglo de operaciones (c, d, e, u)
 *   seq_len     -> longitud del arreglo anterior
 *   max_threads -> máximo de hilos simultáneos (0 = núm. CPUs)
 *   key         -> clave para encriptación (si aplica)
 *   algo        -> algoritmo de compresión (LZW o RLE)
 */
int process_directory_concurrently(
    const char *input_dir,
    const char *output_dir,
    OperationType *op_sequence,
    size_t seq_len,
    int max_threads,
    char *key,
    CompressionAlgo algo
);

#endif


