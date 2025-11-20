#define _POSIX_C_SOURCE 200809L
#include "../../include/executor.h"
#include "../../include/file.h"
#include "../../include/utils.h"
#include "../../include/directory.h"
#include "../../include/algorithms.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>   // unlink, close, sysconf
#include <errno.h>
#include <dirent.h>

void *process_file_pipeline(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    char *current_input = args->input_file_path; // puntero que apunta al archivo de entrada actual
    char *next_output = NULL;

    for (int i = 0; i < 4 && args->sequence[i] != OP_NONE; i++) {
        OperationType op = args->sequence[i];

        /* 1) Determinar destino: si hay otra operación -> archivo temporal; si no -> salida final */
        if (args->sequence[i+1] != OP_NONE) {
            char template[] = "/tmp/gsea_XXXXXX";
            int fd = mkstemp(template);
            if (fd == -1) {
                perror("[process_file_pipeline] mkstemp");
                goto cleanup_and_exit;
            }
            close(fd);
            next_output = strdup(template);
            if (!next_output) {
                perror("[process_file_pipeline] strdup");
                goto cleanup_and_exit;
            }
        } else {
            next_output = args->output_file_path; // apuntamos a la ruta final (no strdup)
        }

        /* 2) Ejecutar la operación */
        int rc = 1;
        if (op == OP_COMPRESS) {
            rc = alg_compress_copy(current_input, next_output);
        } else if (op == OP_DECOMPRESS) {
            rc = alg_decompress_copy(current_input, next_output);
        } else if (op == OP_ENCRYPT) {
            rc = alg_encrypt_copy(current_input, next_output, args->key);
        } else if (op == OP_DECRYPT) {
            rc = alg_decrypt_copy(current_input, next_output, args->key);
        }

        if (rc != 0) {
            fprintf(stderr, "[process_file_pipeline] Error aplicando op %d sobre %s -> %s\n", op, current_input, next_output);
            goto cleanup_and_exit;
        }

        /* 3) Avanzar pipeline de forma segura:
           - guardamos el puntero anterior en 'prev_input'
           - actualizamos current_input = next_output (siempre)
           - liberamos/unlink prev_input SOLO si fue un temporal creado por nosotros
        */
        char *prev_input = current_input;
        current_input = next_output;
        next_output = NULL;

        if (prev_input != args->input_file_path && prev_input != args->output_file_path) {
            if (unlink(prev_input) != 0) {
                perror("[process_file_pipeline] unlink temp");
            }
            free(prev_input);
        }

        /* Si era la última operación, informamos */
        if (args->sequence[i+1] == OP_NONE) {
            printf("[process_file_pipeline] Archivo procesado: %s -> %s\n", args->input_file_path, args->output_file_path);
        }
    }

cleanup_and_exit:
    /* Limpieza final: si current_input es un temporal distinto a input/output, liberarlo */
    if (current_input != NULL &&
        current_input != args->input_file_path &&
        current_input != args->output_file_path)
    {
        unlink(current_input);
        free(current_input);
    }

    /* Liberar rutas que fueron duplicadas por el creador del ThreadArgs */
    if (args->input_file_path) free(args->input_file_path);
    if (args->output_file_path) free(args->output_file_path);

    /* Señalizar al semáforo (si aplica) y liberar args */
    if (args->limiter) sem_post(args->limiter);
    free(args);
    return NULL; // Terminar hilo
}

int process_directory_concurrently(const char *input_dir, const char *output_dir, OperationType *op_sequence, size_t seq_len, int max_threads, char *key) {
    DIR *dir = opendir(input_dir);
    if (!dir) {
        perror("[process_directory_concurrently] opendir");
        return 1;
    }

    // crear directorio de salida si no existe
    if (mkdir(output_dir, 0777) != 0 && errno != EEXIST) {
        perror("[process_directory_concurrently] mkdir output");
        closedir(dir);
        return 1;
    }

    // determinar max threads
    if (max_threads <= 0) {
        long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
        max_threads = (ncpu > 0) ? (int)ncpu : 2;
    }

    sem_t limiter;
    if (sem_init(&limiter, 0, (unsigned)max_threads) != 0) {
        perror("[process_directory_concurrently] sem_init");
        closedir(dir);
        return 1;
    }

    struct dirent *entry;
    pthread_t *threads = NULL;
    size_t thread_count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char *full_input = build_path(input_dir, entry->d_name);
        if (!full_input) continue;

        if (is_directory(full_input)) {
            free(full_input);
            continue;
        }

        // preparar args para el hilo
        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        if (!args) {
            perror("[process_directory_concurrently] malloc args");
            free(full_input);
            continue;
        }

        // duplicar rutas para que cada hilo tenga su propia memoria
        args->input_file_path = full_input;
        args->output_file_path = build_path(output_dir, entry->d_name);
        args->key = key;
        args->limiter = &limiter;
        for (size_t i = 0; i < 4; i++) args->sequence[i] = (i < seq_len) ? op_sequence[i] : OP_NONE;

        // esperar disponibilidad (sem_wait), para no crear más de max_threads simultáneos
        if (sem_wait(&limiter) != 0) {
            perror("[process_directory_concurrently] sem_wait");
            // cleanup and skip
            if (args->output_file_path) free(args->output_file_path);
            if (args->input_file_path) free(args->input_file_path);
            free(args);
            continue;
        }

        // crear hilo
        threads = realloc(threads, (thread_count + 1) * sizeof(pthread_t));
        if (!threads) {
            perror("[process_directory_concurrently] realloc threads");
            // liberar y seguir
            if (args->output_file_path) free(args->output_file_path);
            if (args->input_file_path) free(args->input_file_path);
            free(args);
            sem_post(&limiter);
            continue;
        }
        if (pthread_create(&threads[thread_count], NULL, process_file_pipeline, args) != 0) {
            perror("[process_directory_concurrently] pthread_create");
            // cleanup
            if (args->output_file_path) free(args->output_file_path);
            if (args->input_file_path) free(args->input_file_path);
            free(args);
            sem_post(&limiter);
            continue;
        }

        thread_count++;
    }

    closedir(dir);

    // Esperar a todos los threads restantes. Cada hilo llama sem_post al terminar; aquí hacemos join.
    for (size_t i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    if (threads) free(threads);
    sem_destroy(&limiter);
    return 0;
}