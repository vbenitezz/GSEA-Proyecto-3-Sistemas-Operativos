#ifndef RLE_H
#define RLE_H

#include <stddef.h>

int rle_compress_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len);
int rle_decompress_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len);

#endif