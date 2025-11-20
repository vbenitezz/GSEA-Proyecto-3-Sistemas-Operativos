#ifndef COMPRESSION_SELECTOR_H
#define COMPRESSION_SELECTOR_H

#include <stddef.h>
#include "../algorithms.h"   /* for CompressionAlgo */

int compression_selector_compress_file(const char *in_path, const char *out_path, CompressionAlgo algo);
int compression_selector_decompress_file(const char *in_path, const char *out_path, CompressionAlgo algo);

#endif