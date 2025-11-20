#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include <stddef.h>

// API usada por el executor
// Devuelven 0 en éxito, >0 en error

// Compresión / descompresión (elige LZW por defecto; puedes forzar RLE con env GSEA_COMP=RLE)
int alg_compress_copy(const char *in_path, const char *out_path);
int alg_decompress_copy(const char *in_path, const char *out_path);

// Encriptación / desencriptación con Feistel-16 (CBC, IV antepuesto)
int alg_encrypt_copy(const char *in_path, const char *out_path, const char *key);
int alg_decrypt_copy(const char *in_path, const char *out_path, const char *key);

// Funciones directas si quieres invocarlas (no necesarias para executor)
int alg_compress_rle_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len);
int alg_decompress_rle_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len);

int alg_compress_lzw_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len);
int alg_decompress_lzw_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len);

#endif