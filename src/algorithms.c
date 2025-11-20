#define _POSIX_C_SOURCE 200809L
#include "../include/algorithms.h"
#include "../include/algorithms/compression_selector.h"
#include "../include/algorithms/feistel.h"

/* delegar en selector/feistel */
int alg_compress_copy(const char *in_path, const char *out_path, CompressionAlgo algo) {
    return compression_selector_compress_file(in_path, out_path, algo);
}
int alg_decompress_copy(const char *in_path, const char *out_path, CompressionAlgo algo) {
    return compression_selector_decompress_file(in_path, out_path, algo);
}
int alg_encrypt_copy(const char *in_path, const char *out_path, const char *key) {
    return feistel_encrypt_file(in_path, out_path, key);
}
int alg_decrypt_copy(const char *in_path, const char *out_path, const char *key) {
    return feistel_decrypt_file(in_path, out_path, key);
}