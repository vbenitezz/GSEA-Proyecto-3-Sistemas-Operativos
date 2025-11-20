#define _POSIX_C_SOURCE 200809L
#include "../../include/algorithms.h"
#include "../../include/file.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

/* -------------------------------------------------------
   Utilities: read whole file and write whole file (use existing helpers)
   ------------------------------------------------------- */

/* helper to write buffer to path using safe_open / safe_write */
static int write_buffer_to_file(const char *out_path, const unsigned char *buf, size_t len) {
    int fd = safe_open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return 1;
    if (safe_write(fd, buf, len) != 0) {
        safe_close(fd);
        return 1;
    }
    safe_close(fd);
    return 0;
}

/* helper to read random bytes from /dev/urandom */
static int read_random_bytes(unsigned char *buf, size_t n) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return -1;
    ssize_t r = safe_read(fd, buf, n);
    close(fd);
    return (r == (ssize_t)n) ? 0 : -1;
}

/* =======================================================
   RLE: compress/decompress on byte stream
   Simple format: [count:1byte][value:1byte]...
   count 1..255. If run >255, split.
   ======================================================= */

int alg_compress_rle_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len) {
    if (!in) return 1;
    // Worst case: each byte becomes 2 bytes. Allocate 2*in_len
    size_t cap = in_len == 0 ? 1 : in_len * 2;
    unsigned char *res = malloc(cap);
    if (!res) return 1;
    size_t w = 0;

    size_t i = 0;
    while (i < in_len) {
        unsigned char val = in[i];
        unsigned int count = 1;
        i++;
        while (i < in_len && in[i] == val && count < 255) {
            count++; i++;
        }
        // write pair
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

int alg_decompress_rle_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len) {
    if (!in) return 1;
    // We don't know exact size, but upper-bound naive: sum of counts might be large.
    // We'll grow dynamically.
    size_t cap = 1024;
    unsigned char *res = malloc(cap);
    if (!res) return 1;
    size_t w = 0;
    size_t i = 0;
    while (i + 1 <= in_len - 1) { // need at least two bytes
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
    // If odd number of bytes -> malformed RLE
    if (i != in_len) { free(res); return 1; }

    *out = res;
    *out_len = w;
    return 0;
}

/* =======================================================
   LZW (simple, not bit-packed): codes stored as 16-bit big-endian.
   - dictionary initially contains 0..255 single bytes
   - next_code starts at 256, grows up to max_code (65535)
   - encoder outputs uint16_t codes (2 bytes each)
   - decoder reads uint16_t codes
   This is NOT bit-packed and thus less space-efficient than classic LZW,
   but simple to implement and robust for academic use.
   ======================================================= */

typedef struct {
    unsigned char *data;
    size_t len;
} LZWEntry;

static int lzw_add_entry(LZWEntry **dict, size_t *dict_size, size_t *dict_cap, const unsigned char *data, size_t len) {
    if (*dict_size >= *dict_cap) {
        size_t nc = (*dict_cap == 0) ? 512 : (*dict_cap * 2);
        LZWEntry *tmp = realloc(*dict, nc * sizeof(LZWEntry));
        if (!tmp) return 1;
        *dict = tmp;
        *dict_cap = nc;
    }
    unsigned char *copy = malloc(len);
    if (!copy) return 1;
    memcpy(copy, data, len);
    (*dict)[*dict_size].data = copy;
    (*dict)[*dict_size].len = len;
    (*dict_size)++;
    return 0;
}

/* search dict for sequence data,len; returns index or -1 */
static int lzw_find(LZWEntry *dict, size_t dict_size, const unsigned char *data, size_t len) {
    for (size_t i = 0; i < dict_size; ++i) {
        if (dict[i].len == len && memcmp(dict[i].data, data, len) == 0) return (int)i;
    }
    return -1;
}

int alg_compress_lzw_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len) {
    if (!in) return 1;
    // initialize dict with single bytes
    LZWEntry *dict = NULL;
    size_t dict_size = 0, dict_cap = 0;
    for (int i = 0; i < 256; ++i) {
        unsigned char b = (unsigned char)i;
        if (lzw_add_entry(&dict, &dict_size, &dict_cap, &b, 1) != 0) goto lzw_encode_fail;
    }

    // Working buffer for building w
    unsigned char *w = NULL; size_t wlen = 0;
    // output buffer (codes as 2 bytes big endian)
    size_t out_cap = (in_len + 1) * 2;
    unsigned char *obuf = malloc(out_cap);
    if (!obuf) goto lzw_encode_fail;
    size_t ow = 0;

    for (size_t pos = 0; pos < in_len; ++pos) {
        unsigned char k = in[pos];
        // candidate = w + k
        size_t cand_len = wlen + 1;
        unsigned char *cand = malloc(cand_len);
        if (!cand) { free(obuf); goto lzw_encode_fail; }
        if (wlen > 0) memcpy(cand, w, wlen);
        cand[wlen] = k;

        int idx = lzw_find(dict, dict_size, cand, cand_len);
        if (idx != -1) {
            // w = cand
            free(w);
            w = cand; wlen = cand_len;
            continue;
        } else {
            // output code for w
            int code;
            if (wlen == 0) code = (int)k; // shouldn't happen normally
            else {
                code = lzw_find(dict, dict_size, w, wlen);
                if (code < 0) { free(cand); goto lzw_encode_fail; }
            }
            // write code as uint16 BE
            if (ow + 2 > out_cap) {
                out_cap *= 2; unsigned char *tmp = realloc(obuf, out_cap);
                if (!tmp) { free(cand); free(obuf); goto lzw_encode_fail; }
                obuf = tmp;
            }
            obuf[ow++] = (unsigned char)((code >> 8) & 0xFF);
            obuf[ow++] = (unsigned char)(code & 0xFF);

            // add candidate to dict
            if (lzw_add_entry(&dict, &dict_size, &dict_cap, cand, cand_len) != 0) { free(cand); goto lzw_encode_fail; }
            // set w = [k]
            free(w);
            w = malloc(1);
            if (!w) goto lzw_encode_fail;
            w[0] = k; wlen = 1;
            free(cand); // already copied into dict
        }
    }

    // output last w if any
    if (wlen > 0) {
        int code = lzw_find(dict, dict_size, w, wlen);
        if (code < 0) goto lzw_encode_fail;
        if (ow + 2 > out_cap) {
            out_cap *= 2; unsigned char *tmp = realloc(obuf, out_cap);
            if (!tmp) goto lzw_encode_fail;
            obuf = tmp;
        }
        obuf[ow++] = (unsigned char)((code >> 8) & 0xFF);
        obuf[ow++] = (unsigned char)(code & 0xFF);
    }

    // cleanup dict and w
    for (size_t i = 0; i < dict_size; ++i) free(dict[i].data);
    free(dict);
    free(w);

    *out = obuf; *out_len = ow;
    return 0;

lzw_encode_fail:
    if (dict) { for (size_t i = 0; i < dict_size; ++i) if (dict[i].data) free(dict[i].data); free(dict); }
    if (w) free(w);
    return 1;
}

int alg_decompress_lzw_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len) {
    if (!in) return 1;
    // build dict: index->string
    LZWEntry *dict = NULL; size_t dict_size = 0, dict_cap = 0;
    for (int i = 0; i < 256; ++i) {
        unsigned char b = (unsigned char)i;
        if (lzw_add_entry(&dict, &dict_size, &dict_cap, &b, 1) != 0) goto lzw_decode_fail;
    }

    // read codes as uint16 BE
    if (in_len % 2 != 0) goto lzw_decode_fail;
    size_t codes = in_len / 2;
    size_t out_cap = 1024;
    unsigned char *obuf = malloc(out_cap);
    if (!obuf) goto lzw_decode_fail;
    size_t ow = 0;

    int prev_code = -1;
    for (size_t i = 0; i < codes; ++i) {
        int code = (in[2*i] << 8) | in[2*i+1];
        unsigned char *entry = NULL; size_t entry_len = 0;

        if (code < (int)dict_size) {
            entry_len = dict[code].len;
            entry = malloc(entry_len);
            if (!entry) goto lzw_decode_fail;
            memcpy(entry, dict[code].data, entry_len);
        } else {
            // Special case (KwKwK): entry = prev_string + first_char(prev_string)
            if (prev_code < 0) goto lzw_decode_fail;
            size_t plen = dict[prev_code].len;
            entry = malloc(plen + 1);
            if (!entry) goto lzw_decode_fail;
            memcpy(entry, dict[prev_code].data, plen);
            entry[plen] = dict[prev_code].data[0];
            entry_len = plen + 1;
        }

        // append entry to output
        if (ow + entry_len > out_cap) {
            while (ow + entry_len > out_cap) out_cap *= 2;
            unsigned char *tmp = realloc(obuf, out_cap);
            if (!tmp) { free(entry); goto lzw_decode_fail; }
            obuf = tmp;
        }
        memcpy(obuf + ow, entry, entry_len);
        ow += entry_len;

        // add new dict entry: prev_string + first_char(entry)
        if (prev_code >= 0) {
            size_t plen = dict[prev_code].len;
            unsigned char *newstr = malloc(plen + 1);
            if (!newstr) { free(entry); goto lzw_decode_fail; }
            memcpy(newstr, dict[prev_code].data, plen);
            newstr[plen] = entry[0];
            if (lzw_add_entry(&dict, &dict_size, &dict_cap, newstr, plen + 1) != 0) { free(newstr); free(entry); goto lzw_decode_fail; }
            // lzw_add_entry copied newstr
            free(newstr);
        }

        free(entry);
        prev_code = code;
    }

    for (size_t i = 0; i < dict_size; ++i) free(dict[i].data);
    free(dict);

    *out = obuf; *out_len = ow;
    return 0;

lzw_decode_fail:
    if (dict) { for (size_t i = 0; i < dict_size; ++i) if (dict[i].data) free(dict[i].data); free(dict); }
    return 1;
}

/* Wrapper functions to compress/decompress buffers using RLE or LZW */
int alg_compress_lzw_buf_wrapper(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len) {
    return alg_compress_lzw_buf(in, in_len, out, out_len);
}
int alg_decompress_lzw_buf_wrapper(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len) {
    return alg_decompress_lzw_buf(in, in_len, out, out_len);
}

/* =======================================================
   Feistel block cipher (16 rounds) - block size 8 bytes (64 bits)
   Mode: CBC with 8-byte IV stored at beginning of ciphertext
   Padding: PKCS#7 for 8-byte block
   Key schedule: derive 16 uint32_t round keys from provided key string
   Note: This is a pedagogical cipher, NOT production-grade AES.
   ======================================================= */

static inline uint32_t rol32(uint32_t x, int s) { return (x << s) | (x >> (32 - s)); }
static inline uint32_t ror32(uint32_t x, int s) { return (x >> s) | (x << (32 - s)); }

/* simple key schedule: mix key bytes into 16 uint32_t round keys */
static void feistel_key_schedule(const unsigned char *key, size_t key_len, uint32_t round_keys[16]) {
    // initialize from repeated key bytes
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

/* round function F: mix using addition, xor, rotates */
static uint32_t feistel_F(uint32_t half, uint32_t rk) {
    uint32_t x = half;
    x = x + rk;
    x = x ^ rol32(half, 5);
    x = x + ((rk ^ 0xA5A5A5A5u) & 0xFFFFFFFFu);
    x = rol32(x, 11);
    x ^= (rk >> 3);
    return x;
}

/* encrypt single 8-byte block in place */
static void feistel_encrypt_block(uint8_t block[8], uint32_t round_keys[16]) {
    uint32_t L = (block[0]<<24)|(block[1]<<16)|(block[2]<<8)|block[3];
    uint32_t R = (block[4]<<24)|(block[5]<<16)|(block[6]<<8)|block[7];
    for (int r = 0; r < 16; ++r) {
        uint32_t newL = R;
        uint32_t newR = L ^ feistel_F(R, round_keys[r]);
        L = newL; R = newR;
    }
    // pack back (note: after 16 rounds, swap or not? Here the Feistel structure already swapped each round)
    block[0] = (L >> 24) & 0xFF; block[1] = (L >> 16) & 0xFF; block[2] = (L >> 8) & 0xFF; block[3] = L & 0xFF;
    block[4] = (R >> 24) & 0xFF; block[5] = (R >> 16) & 0xFF; block[6] = (R >> 8) & 0xFF; block[7] = R & 0xFF;
}

/* decrypt single block */
static void feistel_decrypt_block(uint8_t block[8], uint32_t round_keys[16]) {
    uint32_t L = (block[0]<<24)|(block[1]<<16)|(block[2]<<8)|block[3];
    uint32_t R = (block[4]<<24)|(block[5]<<16)|(block[6]<<8)|block[7];
    for (int r = 15; r >= 0; --r) {
        uint32_t newR = L;
        uint32_t newL = R ^ feistel_F(L, round_keys[r]);
        L = newL; R = newR;
    }
    block[0] = (L >> 24) & 0xFF; block[1] = (L >> 16) & 0xFF; block[2] = (L >> 8) & 0xFF; block[3] = L & 0xFF;
    block[4] = (R >> 24) & 0xFF; block[5] = (R >> 16) & 0xFF; block[6] = (R >> 8) & 0xFF; block[7] = R & 0xFF;
}

/* PKCS#7-like padding for block size 8 */
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
    for (size_t i = 0; i < pad; ++i) {
        if (in[in_len - 1 - i] != pad) return NULL;
    }
    *out_len = in_len - pad;
    unsigned char *out = malloc(*out_len);
    if (!out) return NULL;
    memcpy(out, in, *out_len);
    return out;
}

/* CBC XOR helper */
static void xor_block(uint8_t *dst, const uint8_t *a, const uint8_t *b) {
    for (int i = 0; i < 8; ++i) dst[i] = a[i] ^ b[i];
}

/* Feistel encrypt buffer (writes IV + ciphertext into out) */
static int feistel_encrypt_buffer(const unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len, unsigned char **out, size_t *out_len) {
    uint32_t round_keys[16];
    feistel_key_schedule(key, key_len, round_keys);

    size_t padded_len;
    unsigned char *padded = pad_pkcs7(in, in_len, &padded_len);
    if (!padded) return 1;

    // allocate out: IV(8) + padded_len
    *out_len = 8 + padded_len;
    unsigned char *obuf = malloc(*out_len);
    if (!obuf) { free(padded); return 1; }

    // IV
    if (read_random_bytes(obuf, 8) != 0) { free(padded); free(obuf); return 1; }

    uint8_t prev[8];
    memcpy(prev, obuf, 8);

    // CBC encryption
    for (size_t pos = 0; pos < padded_len; pos += 8) {
        uint8_t block[8];
        xor_block(block, padded + pos, prev);        // block = plaintext ^ prev
        feistel_encrypt_block(block, round_keys);   // encrypt
        memcpy(obuf + 8 + pos, block, 8);
        memcpy(prev, block, 8);
    }

    free(padded);
    *out = obuf;
    return 0;
}

/* Feistel decrypt buffer (expects IV + ciphertext) */
static int feistel_decrypt_buffer(const unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len, unsigned char **out, size_t *out_len) {
    if (in_len < 8) return 1; // must have IV
    uint32_t round_keys[16];
    feistel_key_schedule(key, key_len, round_keys);

    const unsigned char *iv = in;
    const unsigned char *ct = in + 8;
    size_t ct_len = in_len - 8;
    if (ct_len % 8 != 0) return 1;

    unsigned char *plain_padded = malloc(ct_len);
    if (!plain_padded) return 1;

    uint8_t prev[8];
    memcpy(prev, iv, 8);

    for (size_t pos = 0; pos < ct_len; pos += 8) {
        uint8_t block[8];
        memcpy(block, ct + pos, 8);
        uint8_t dec[8];
        memcpy(dec, block, 8);
        feistel_decrypt_block(dec, round_keys);
        // plaintext = dec ^ prev
        for (int i = 0; i < 8; ++i) plain_padded[pos + i] = dec[i] ^ prev[i];
        memcpy(prev, block, 8);
    }

    unsigned char *unpadded = unpad_pkcs7(plain_padded, ct_len, out_len);
    free(plain_padded);
    if (!unpadded) return 1;
    *out = unpadded;
    return 0;
}

/* =======================================================
   High-level wrappers that operate on files (paths)
   - Choose RLE or LZW for compression using env var GSEA_COMP
   - Encryption uses Feistel-CBC
   ======================================================= */

int alg_compress_copy(const char *in_path, const char *out_path) {
    // choose algorithm
    const char *env = getenv("GSEA_COMP");
    size_t in_len;
    unsigned char *in_buf = read_file_complete(in_path, &in_len);
    if (!in_buf) return 1;

    unsigned char *out_buf = NULL;
    size_t out_len = 0;
    int rc = 1;
    if (env && strcmp(env, "RLE") == 0) {
        rc = alg_compress_rle_buf(in_buf, in_len, &out_buf, &out_len);
    } else {
        rc = alg_compress_lzw_buf(in_buf, in_len, &out_buf, &out_len);
    }
    free(in_buf);
    if (rc != 0) { if (out_buf) free(out_buf); return 1; }

    int wrc = write_buffer_to_file(out_path, out_buf, out_len);
    free(out_buf);
    return wrc;
}

int alg_decompress_copy(const char *in_path, const char *out_path) {
    // try both: if env forces RLE try RLE decode; if not, attempt LZW then RLE fallback.
    const char *env = getenv("GSEA_COMP");
    size_t in_len;
    unsigned char *in_buf = read_file_complete(in_path, &in_len);
    if (!in_buf) return 1;

    unsigned char *out_buf = NULL; size_t out_len = 0;
    int rc = 1;
    if (env && strcmp(env, "RLE") == 0) {
        rc = alg_decompress_rle_buf(in_buf, in_len, &out_buf, &out_len);
    } else {
        // try LZW first
        rc = alg_decompress_lzw_buf(in_buf, in_len, &out_buf, &out_len);
        if (rc != 0) {
            // fallback to RLE
            rc = alg_decompress_rle_buf(in_buf, in_len, &out_buf, &out_len);
        }
    }
    free(in_buf);
    if (rc != 0) { if (out_buf) free(out_buf); return 1; }

    int wrc = write_buffer_to_file(out_path, out_buf, out_len);
    free(out_buf);
    return wrc;
}

int alg_encrypt_copy(const char *in_path, const char *out_path, const char *key) {
    if (!key) return 1;
    size_t in_len; unsigned char *in_buf = read_file_complete(in_path, &in_len);
    if (!in_buf) return 1;
    unsigned char *out_buf = NULL; size_t out_len = 0;
    if (feistel_encrypt_buffer(in_buf, in_len, (const unsigned char*)key, strlen(key), &out_buf, &out_len) != 0) {
        free(in_buf); return 1;
    }
    free(in_buf);
    int wrc = write_buffer_to_file(out_path, out_buf, out_len);
    free(out_buf);
    return wrc;
}

int alg_decrypt_copy(const char *in_path, const char *out_path, const char *key) {
    if (!key) return 1;
    size_t in_len; unsigned char *in_buf = read_file_complete(in_path, &in_len);
    if (!in_buf) return 1;
    unsigned char *out_buf = NULL; size_t out_len = 0;
    if (feistel_decrypt_buffer(in_buf, in_len, (const unsigned char*)key, strlen(key), &out_buf, &out_len) != 0) {
        free(in_buf); return 1;
    }
    free(in_buf);
    int wrc = write_buffer_to_file(out_path, out_buf, out_len);
    free(out_buf);
    return wrc;
}