#define _POSIX_C_SOURCE 200809L
#include "../../include/algorithms/feistel.h"
#include "../../include/utils.h"
#include "../../include/file.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Dependen de utilidades I/O (leer archivo completo, escribir buffer, generar IV) */
extern int read_random_bytes(unsigned char *buf, size_t n);
extern unsigned char *read_file_complete(const char *path, size_t *size);
extern int write_buffer_to_file(const char *path, const unsigned char *buf, size_t size);

/* Rotaciones */
static inline uint32_t rol32(uint32_t x, int s) { return (x << s) | (x >> (32 - s)); }
static void feistel_key_schedule(const unsigned char *key, size_t key_len, uint32_t round_keys[16]) {
    if (key_len == 0) key = (const unsigned char *)"default", key_len = 7;
    uint32_t acc = 0x9e3779b9u;
    for (int i = 0; i < 16; ++i) {
        uint32_t v = acc;
        for (size_t j = 0; j < key_len; ++j) {
            v = v * 31 + key[j];
            v = rol32(v, (j + i) & 31);
        }
        round_keys[i] = v ^ (uint32_t)(key_len * (i + 1));
        acc += 0x7f4a7c15u;
    }
}
static uint32_t feistel_F(uint32_t half, uint32_t rk) {
    uint32_t x = half;
    x += rk;
    x ^= rol32(half, 5);
    x += (rk ^ 0xA5A5A5A5u);
    x = rol32(x, 11);
    x ^= (rk >> 3);
    return x;
}
static void feistel_encrypt_block(uint8_t block[8], uint32_t round_keys[16]) {
    uint32_t L = (block[0]<<24)|(block[1]<<16)|(block[2]<<8)|block[3];
    uint32_t R = (block[4]<<24)|(block[5]<<16)|(block[6]<<8)|block[7];
    for (int r = 0; r < 16; ++r) {
        uint32_t newL = R;
        uint32_t newR = L ^ feistel_F(R, round_keys[r]);
        L = newL; R = newR;
    }
    block[0]=(L>>24)&0xFF; block[1]=(L>>16)&0xFF; block[2]=(L>>8)&0xFF; block[3]=L&0xFF;
    block[4]=(R>>24)&0xFF; block[5]=(R>>16)&0xFF; block[6]=(R>>8)&0xFF; block[7]=R&0xFF;
}
static void feistel_decrypt_block(uint8_t block[8], uint32_t round_keys[16]) {
    uint32_t L = (block[0]<<24)|(block[1]<<16)|(block[2]<<8)|block[3];
    uint32_t R = (block[4]<<24)|(block[5]<<16)|(block[6]<<8)|block[7];
    for (int r = 15; r >= 0; --r) {
        uint32_t newR = L;
        uint32_t newL = R ^ feistel_F(L, round_keys[r]);
        L = newL; R = newR;
    }
    block[0]=(L>>24)&0xFF; block[1]=(L>>16)&0xFF; block[2]=(L>>8)&0xFF; block[3]=L&0xFF;
    block[4]=(R>>24)&0xFF; block[5]=(R>>16)&0xFF; block[6]=(R>>8)&0xFF; block[7]=R&0xFF;
}
/* padding helpers */
static unsigned char *pad_pkcs7(const unsigned char *in, size_t in_len, size_t *out_len) {
    size_t block = 8;
    size_t pad = block - (in_len % block);
    *out_len = in_len + pad;
    unsigned char *out = malloc(*out_len);
    if (!out) return NULL;
    memcpy(out, in, in_len);
    for (size_t i = 0; i < pad; ++i) out[in_len + i] = (unsigned char)pad;
    return out;
}
static unsigned char *unpad_pkcs7(unsigned char *in, size_t in_len, size_t *out_len) {
    if (in_len == 0 || in_len % 8 != 0) return NULL;
    unsigned char pad = in[in_len - 1];
    if (pad == 0 || pad > 8) return NULL;
    for (size_t i = 0; i < pad; ++i) if (in[in_len - 1 - i] != pad) return NULL;
    *out_len = in_len - pad;
    unsigned char *out = malloc(*out_len);
    if (!out) return NULL;
    memcpy(out, in, *out_len);
    return out;
}
static void xor_block(uint8_t *dst, const uint8_t *a, const uint8_t *b) { for (int i=0;i<8;i++) dst[i]=a[i]^b[i]; }

static int feistel_encrypt_buffer(const unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len, unsigned char **out, size_t *out_len) {
    uint32_t round_keys[16];
    feistel_key_schedule(key, key_len, round_keys);
    size_t padded_len; unsigned char *padded = pad_pkcs7(in, in_len, &padded_len);
    if (!padded) return 1;
    *out_len = 8 + padded_len;
    unsigned char *obuf = malloc(*out_len);
    if (!obuf) { free(padded); return 1; }
    if (read_random_bytes(obuf, 8) != 0) { free(padded); free(obuf); return 1; }
    uint8_t prev[8]; memcpy(prev, obuf, 8);
    for (size_t pos=0; pos<padded_len; pos+=8) {
        uint8_t block[8];
        xor_block(block, padded + pos, prev);
        feistel_encrypt_block(block, round_keys);
        memcpy(obuf + 8 + pos, block, 8);
        memcpy(prev, block, 8);
    }
    free(padded);
    *out = obuf;
    return 0;
}
static int feistel_decrypt_buffer(const unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len, unsigned char **out, size_t *out_len) {
    if (in_len < 8) return 1;
    uint32_t round_keys[16];
    feistel_key_schedule(key, key_len, round_keys);
    const unsigned char *iv = in;
    const unsigned char *ct = in + 8;
    size_t ct_len = in_len - 8;
    if (ct_len % 8 != 0) return 1;
    unsigned char *plain_padded = malloc(ct_len);
    if (!plain_padded) return 1;
    uint8_t prev[8]; memcpy(prev, iv, 8);
    for (size_t pos=0; pos<ct_len; pos+=8) {
        uint8_t block[8], dec[8];
        memcpy(block, ct + pos, 8);
        memcpy(dec, block, 8);
        feistel_decrypt_block(dec, round_keys);
        for (int i=0;i<8;i++) plain_padded[pos + i] = dec[i] ^ prev[i];
        memcpy(prev, block, 8);
    }
    unsigned char *unpadded = unpad_pkcs7(plain_padded, ct_len, out_len);
    free(plain_padded);
    if (!unpadded) return 1;
    *out = unpadded;
    return 0;
}

/* high level file API */
int feistel_encrypt_file(const char *in_path, const char *out_path, const char *key) {
    if (!key) return 1;
    size_t in_len; unsigned char *in_buf = read_file_complete(in_path, &in_len);
    if (!in_buf) return 1;
    unsigned char *out_buf = NULL; size_t out_len = 0;
    if (feistel_encrypt_buffer(in_buf, in_len, (const unsigned char*)key, strlen(key), &out_buf, &out_len) != 0) { free(in_buf); return 1; }
    free(in_buf);
    int wrc = write_buffer_to_file(out_path, out_buf, out_len);
    free(out_buf);
    return wrc;
}
int feistel_decrypt_file(const char *in_path, const char *out_path, const char *key) {
    if (!key) return 1;
    size_t in_len; unsigned char *in_buf = read_file_complete(in_path, &in_len);
    if (!in_buf) return 1;
    unsigned char *out_buf = NULL; size_t out_len = 0;
    if (feistel_decrypt_buffer(in_buf, in_len, (const unsigned char*)key, strlen(key), &out_buf, &out_len) != 0) { free(in_buf); return 1; }
    free(in_buf);
    int wrc = write_buffer_to_file(out_path, out_buf, out_len);
    free(out_buf);
    return wrc;
}