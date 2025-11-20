#ifndef LZW_H
#define LZW_H

#include <stddef.h>

int lzw_compress_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len);
int lzw_decompress_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len);

#endif