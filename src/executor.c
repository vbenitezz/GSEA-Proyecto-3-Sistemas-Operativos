#define _POSIX_C_SOURCE 200809L
#include "../include/executor.h"
#include "../include/pipeline.h"
#include "../include/algorithms.h"
#include "../include/utils.h"
#include "../include/directory.h"
#include "../include/file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <semaphore.h>

/* process single file in thread */
void *process_file_pipeline(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    char *current_input = args->input_file_path;
    char *next_output = NULL;

    for (int i = 0; i < 4 && args->sequence[i] != OP_NONE; ++i) {
        OperationType op = args->sequence[i];

        /* determine next_output */
        if (args->sequence[i+1] != OP_NONE) {
            char template[] = "/tmp/gsea_XXXXXX";
            int fd = mkstemp(template);
            if (fd == -1) { perror("mkstemp"); goto cleanup; }
            close(fd);
            next_output = strdup(template);
            if (!next_output) { perror("strdup"); goto cleanup; }
        } else {
            next_output = args->output_file_path;
        }

        int rc = 1;
        if (op == OP_COMPRESS) {
            rc = alg_compress_copy(current_input, next_output, args->algo);
        } else if (op == OP_DECOMPRESS) {
            rc = alg_decompress_copy(current_input, next_output, args->algo);
        } else if (op == OP_ENCRYPT) {
            rc = alg_encrypt_copy(current_input, next_output, args->key);
        } else if (op == OP_DECRYPT) {
            rc = alg_decrypt_copy(current_input, next_output, args->key);
        }

        if (rc != 0) {
            fprintf(stderr, "[process_file_pipeline] Error op %d on %s -> %s\n", op, current_input, next_output ? next_output : "(null)");
            goto cleanup;
        }

        /* remove previous temp if it was temp (not original input) */
        if (i > 0 && current_input != args->input_file_path) {
            unlink(current_input);
            free(current_input);
        }

        if (args->sequence[i+1] != OP_NONE) {
            current_input = next_output;
            next_output = NULL;
        } else {
            printf("[process_file_pipeline] Archivo procesado: %s -> %s\n", args->input_file_path, args->output_file_path);
        }
    }

cleanup:
    if (current_input != NULL && current_input != args->input_file_path && current_input != args->output_file_path) {
        unlink(current_input);
        free(current_input);
    }

    if (args->input_file_path) free(args->input_file_path);
    if (args->output_file_path) free(args->output_file_path);

    if (args->limiter) sem_post(args->limiter);
    free(args);
    return NULL;
}

/* process directory concurrently */
int process_directory_concurrently(const char *input_dir, const char *output_dir, OperationType *op_sequence, size_t seq_len, int max_threads, char *key, CompressionAlgo algo) {
    DIR *dir = opendir(input_dir);
    if (!dir) { perror("opendir"); return 1; }
    if (mkdir(output_dir, 0777) != 0 && errno != EEXIST) { perror("mkdir"); closedir(dir); return 1; }

    if (max_threads <= 0) {
        long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
        max_threads = (ncpu > 0) ? (int)ncpu : 2;
    }

    sem_t limiter;
    if (sem_init(&limiter, 0, (unsigned)max_threads) != 0) { perror("sem_init"); closedir(dir); return 1; }

    struct dirent *entry;
    pthread_t *threads = NULL;
    size_t thread_count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".")==0 || strcmp(entry->d_name, "..")==0) continue;
        char *full_input = build_path(input_dir, entry->d_name);
        if (!full_input) continue;
        if (is_directory(full_input)) { free(full_input); continue; }

        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        if (!args) { free(full_input); continue; }

        args->input_file_path = full_input;
        args->output_file_path = build_path(output_dir, entry->d_name);
        args->key = key;
        args->limiter = &limiter;
        args->algo = algo;
        for (size_t i=0;i<4;i++) args->sequence[i] = (i < seq_len) ? op_sequence[i] : OP_NONE;

        if (sem_wait(&limiter) != 0) { perror("sem_wait"); if (args->output_file_path) free(args->output_file_path); if (args->input_file_path) free(args->input_file_path); free(args); continue; }

        threads = realloc(threads, (thread_count + 1) * sizeof(pthread_t));
        if (!threads) { perror("realloc"); if (args->output_file_path) free(args->output_file_path); if (args->input_file_path) free(args->input_file_path); free(args); sem_post(&limiter); continue; }

        if (pthread_create(&threads[thread_count], NULL, process_file_pipeline, args) != 0) {
            perror("pthread_create");
            if (args->output_file_path) free(args->output_file_path);
            if (args->input_file_path) free(args->input_file_path);
            free(args);
            sem_post(&limiter);
            continue;
        }
        thread_count++;
    }

    closedir(dir);

    for (size_t i=0;i<thread_count;i++) pthread_join(threads[i], NULL);
    if (threads) free(threads);
    sem_destroy(&limiter);
    return 0;
}