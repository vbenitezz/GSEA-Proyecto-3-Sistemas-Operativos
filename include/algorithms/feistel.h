#ifndef FEISTEL_H
#define FEISTEL_H

#include <stddef.h>

int feistel_encrypt_file(const char *in_path, const char *out_path, const char *key);
int feistel_decrypt_file(const char *in_path, const char *out_path, const char *key);

#endif