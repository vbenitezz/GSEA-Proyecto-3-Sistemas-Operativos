#define _POSIX_C_SOURCE 200809L
#include "../../include/algorithms/rle.h"
#include <stdlib.h>
#include <stddef.h>

/* RLE buffer functions (count,value) */
int rle_compress_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len)
{
    if (!in) return 1;
    size_t cap = in_len == 0 ? 1 : in_len * 2;
    unsigned char *res = malloc(cap);
    if (!res) return 1;
    size_t w = 0;
    size_t i = 0;
    while (i < in_len) {
        unsigned char val = in[i++];
        unsigned int count = 1;
        while (i < in_len && in[i] == val && count < 255) { count++; i++; }
        if (w + 2 > cap) {
            cap *= 2;
            unsigned char *tmp = realloc(res, cap);
            if (!tmp) { free(res); return 1; }
            res = tmp;
        }
        res[w++] = (unsigned char)count;
        res[w++] = val;
    }
    *out = res;
    *out_len = w;
    return 0;
}

int rle_decompress_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len)
{
    if (!in) return 1;
    size_t cap = 1024;
    unsigned char *res = malloc(cap);
    if (!res) return 1;
    size_t w = 0;
    size_t i = 0;
    while (i + 1 <= in_len - 1) {
        unsigned char count = in[i++];
        unsigned char val = in[i++];
        if (w + (size_t)count > cap) {
            while (w + (size_t)count > cap) cap *= 2;
            unsigned char *tmp = realloc(res, cap);
            if (!tmp) { free(res); return 1; }
            res = tmp;
        }
        for (unsigned int k = 0; k < count; ++k) res[w++] = val;
    }
    if (i != in_len) { free(res); return 1; }
    *out = res;
    *out_len = w;
    return 0;
}