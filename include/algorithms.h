#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include <stddef.h>
#include "pipeline.h"

int alg_compress_copy(const char *in_path, const char *out_path, CompressionAlgo algo);
int alg_decompress_copy(const char *in_path, const char *out_path, CompressionAlgo algo);
int alg_encrypt_copy(const char *in_path, const char *out_path, const char *key);
int alg_decrypt_copy(const char *in_path, const char *out_path, const char *key);

/* Low-level buffer APIs */
int rle_compress_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len);
int rle_decompress_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len);
int lzw_compress_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len);
int lzw_decompress_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len);

#endif