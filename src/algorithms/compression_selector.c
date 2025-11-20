#define _POSIX_C_SOURCE 200809L
#include "../../include/algorithms/compression_selector.h"
#include "../../include/algorithms/rle.h"
#include "../../include/algorithms/lzw.h"
#include "../../include/file.h"
#include <stdlib.h>

/* extern helpers from your io layer */
extern unsigned char *read_file_complete(const char *path, size_t *size);
extern int write_buffer_to_file(const char *path, const unsigned char *buf, size_t size);

/* compress file using chosen algorithm */
int compression_selector_compress_file(const char *in_path, const char *out_path, CompressionAlgo algo) {
    size_t in_len;
    unsigned char *in_buf = read_file_complete(in_path, &in_len);
    if (!in_buf) return 1;

    unsigned char *out_buf = NULL;
    size_t out_len = 0;
    int rc = 1;
    if (algo == COMP_RLE) rc = rle_compress_buf(in_buf, in_len, &out_buf, &out_len);
    else rc = lzw_compress_buf(in_buf, in_len, &out_buf, &out_len);

    free(in_buf);
    if (rc != 0) { if (out_buf) free(out_buf); return 1; }
    int w = write_buffer_to_file(out_path, out_buf, out_len);
    free(out_buf);
    return w;
}

/* decompress file using chosen algorithm */
int compression_selector_decompress_file(const char *in_path, const char *out_path, CompressionAlgo algo) {
    size_t in_len;
    unsigned char *in_buf = read_file_complete(in_path, &in_len);
    if (!in_buf) return 1;

    unsigned char *out_buf = NULL;
    size_t out_len = 0;
    int rc = 1;
    if (algo == COMP_RLE) rc = rle_decompress_buf(in_buf, in_len, &out_buf, &out_len);
    else rc = lzw_decompress_buf(in_buf, in_len, &out_buf, &out_len);

    free(in_buf);
    if (rc != 0) { if (out_buf) free(out_buf); return 1; }
    int w = write_buffer_to_file(out_path, out_buf, out_len);
    free(out_buf);
    return w;
}