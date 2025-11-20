#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "pipeline.h"

// Procesa un único archivo (secuencial o en hilo)
void *process_file_pipeline(void *arg);

// Recorre un directorio y crea un hilo por cada archivo regular.
// max_threads: número máximo de hilos simultáneos (si 0 -> sysconf(_SC_NPROCESSORS_ONLN)).
int process_directory_concurrently(const char *input_dir, const char *output_dir, OperationType *op_sequence, size_t seq_len, int max_threads, char *key);

#endif

