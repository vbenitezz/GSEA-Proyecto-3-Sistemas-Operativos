#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "../include/file.h"
#include "../include/utils.h"
#include "../include/directory.h"
#include "../include/pipeline.h"
#include "../include/executor.h"

void print_usage(char *prog) {
    printf("Uso: %s -i <input> -o <output> -m <ops> [-t max_threads] [-k key]\n", prog);
    printf("  -i <input>    : archivo o directorio de entrada\n");
    printf("  -o <output>   : archivo o directorio de salida\n");
    printf("  -m <ops>      : secuencia de operaciones, ej: c (compress), e (encrypt), d (decompress), u (decrypt)\n");
    printf("                  ejemplo: -m ce  (comprimir, luego encriptar)\n");
    printf("  -t <N>        : max threads (solo si input es directorio). Default: nro CPUs\n");
    printf("  -k <key>      : clave para encriptacion (si aplica)\n");
}

int parse_sequence(const char *s, OperationType *out, size_t *out_len) {
    size_t idx = 0;
    for (size_t i = 0; s[i] != '\0' && idx < 4; i++) {
        char ch = s[i];
        switch (ch) {
            case 'c': out[idx++] = OP_COMPRESS; break;
            case 'd': out[idx++] = OP_DECOMPRESS; break;
            case 'e': out[idx++] = OP_ENCRYPT; break;
            case 'u': out[idx++] = OP_DECRYPT; break;
            default:
                fprintf(stderr, "Operacion desconocida: %c\n", ch);
                return 1;
        }
    }
    *out_len = idx;
    return 0;
}

int main(int argc, char **argv) {
    char *input = NULL, *output = NULL, *ops = NULL, *key = NULL;
    int max_threads = 0;

    int opt;
    while ((opt = getopt(argc, argv, "i:o:m:t:k:")) != -1) {
        switch (opt) {
            case 'i': input = optarg; break;
            case 'o': output = optarg; break;
            case 'm': ops = optarg; break;
            case 't': max_threads = atoi(optarg); break;
            case 'k': key = optarg; break;
            default: print_usage(argv[0]); return 1;
        }
    }

    if (!input || !output || !ops) {
        print_usage(argv[0]);
        return 1;
    }

    OperationType seq[4] = {OP_NONE, OP_NONE, OP_NONE, OP_NONE};
    size_t seq_len = 0;
    if (parse_sequence(ops, seq, &seq_len) != 0) return 1;

    if (is_directory(input)) {
        // output debe ser directorio
        if (!is_directory(output)) {
            // intentar crear salida
            if (mkdir(output, 0777) != 0 && errno != EEXIST) {
                perror("No se pudo crear directorio de salida");
                return 1;
            }
        }
        return process_directory_concurrently(input, output, seq, seq_len, max_threads, key);
    } else {
        // archivo individual: ejecutar secuencial
        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        if (!args) { perror("malloc"); return 1; }
        args->input_file_path = strdup(input);
        args->output_file_path = strdup(output);
        args->key = key;
        args->limiter = NULL;
        for (size_t i = 0; i < 4; i++) args->sequence[i] = (i < seq_len) ? seq[i] : OP_NONE;

        process_file_pipeline(args);
        // process_file_pipeline libera args y rutas internamente
        return 0;
    }

    return 0;
}

