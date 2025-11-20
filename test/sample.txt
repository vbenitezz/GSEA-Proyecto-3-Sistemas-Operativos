#define _POSIX_C_SOURCE 200809L
#include "../../include/algorithms/lzw.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

/* Optimized LZW (non-bitpacked) with hash table for (prefix,k)->code
   Implementation adapted from earlier provided code. */

typedef struct {
    uint32_t key;
    uint16_t code;
} HashEntry;

typedef struct {
    unsigned char *data;
    size_t len;
} SeqEntry;

static inline uint32_t make_key(uint32_t prefix, unsigned char k) {
    return (prefix << 8) ^ (uint32_t)k;
}

#define HT_BITS 18
#define HT_SIZE (1u << HT_BITS)
#define HT_MASK (HT_SIZE - 1u)
#define MAX_CODE 65535u
#define TABLE_SIZE (MAX_CODE + 1)
#define INITIAL_CODES 256u

static HashEntry *ht_alloc(void) {
    HashEntry *ht = calloc(HT_SIZE, sizeof(HashEntry));
    if (!ht) return NULL;
    for (size_t i = 0; i < HT_SIZE; ++i) ht[i].key = UINT32_MAX;
    return ht;
}
static void ht_free(HashEntry *ht) { free(ht); }

static int ht_find(HashEntry *ht, uint32_t key) {
    uint32_t h = (key * 2654435761u) & HT_MASK;
    uint32_t start = h;
    while (1) {
        uint32_t curk = ht[h].key;
        if (curk == UINT32_MAX) return -1;
        if (curk == key) return (int)ht[h].code;
        h = (h + 1) & HT_MASK;
        if (h == start) return -1;
    }
}

static int ht_lookup_or_insert(HashEntry *ht, uint32_t key, uint16_t code, int *found_out) {
    uint32_t h = (key * 2654435761u) & HT_MASK;
    uint32_t start = h;
    while (1) {
        uint32_t curk = ht[h].key;
        if (curk == UINT32_MAX) {
            ht[h].key = key;
            ht[h].code = code;
            *found_out = 0;
            return (int)h;
        }
        if (curk == key) {
            *found_out = 1;
            return (int)h;
        }
        h = (h + 1) & HT_MASK;
        if (h == start) break;
    }
    return -1;
}

/* Compression */
int lzw_compress_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len) {
    if (!in) return 1;
    if (in_len == 0) { *out = malloc(0); *out_len = 0; return 0; }

    HashEntry *ht = ht_alloc();
    if (!ht) return 1;

    uint32_t next_code = INITIAL_CODES;
    size_t cap = (in_len + 1) * 2;
    unsigned char *obuf = malloc(cap);
    if (!obuf) { ht_free(ht); return 1; }
    size_t ow = 0;

    uint32_t prefix = (uint32_t)in[0];
    for (size_t pos = 1; pos < in_len; ++pos) {
        unsigned char k = in[pos];
        uint32_t key = make_key(prefix, k);
        int idx = ht_find(ht, key);
        if (idx >= 0) {
            prefix = (uint32_t)idx;
            continue;
        } else {
            if (ow + 2 > cap) {
                cap *= 2;
                unsigned char *tmp = realloc(obuf, cap);
                if (!tmp) { free(obuf); ht_free(ht); return 1; }
                obuf = tmp;
            }
            obuf[ow++] = (unsigned char)((prefix >> 8) & 0xFF);
            obuf[ow++] = (unsigned char)(prefix & 0xFF);

            if (next_code <= MAX_CODE) {
                int f;
                ht_lookup_or_insert(ht, key, (uint16_t)next_code, &f);
                next_code++;
            } else {
                ht_free(ht);
                ht = ht_alloc();
                if (!ht) { free(obuf); return 1; }
                next_code = INITIAL_CODES;
            }
            prefix = (uint32_t)k;
        }
    }

    if (ow + 2 > cap) {
        cap *= 2;
        unsigned char *tmp = realloc(obuf, cap);
        if (!tmp) { free(obuf); ht_free(ht); return 1; }
        obuf = tmp;
    }
    obuf[ow++] = (unsigned char)((prefix >> 8) & 0xFF);
    obuf[ow++] = (unsigned char)(prefix & 0xFF);

    ht_free(ht);
    *out = obuf; *out_len = ow;
    return 0;
}

/* Decompression */
int lzw_decompress_buf(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len) {
    if (!in) return 1;
    if (in_len == 0) { *out = malloc(0); *out_len = 0; return 0; }
    if (in_len % 2 != 0) return 1;
    size_t codes = in_len / 2;

    SeqEntry *table = calloc((MAX_CODE + 1), sizeof(SeqEntry));
    if (!table) return 1;

    for (int i = 0; i < 256; ++i) {
        table[i].data = malloc(1);
        if (!table[i].data) { for (int j=0;j<i;j++) free(table[j].data); free(table); return 1; }
        table[i].data[0] = (unsigned char)i;
        table[i].len = 1;
    }
    uint32_t next_code = INITIAL_CODES;

    size_t cap = 1024;
    unsigned char *obuf = malloc(cap);
    if (!obuf) { for (int i=0;i<256;i++) free(table[i].data); free(table); return 1; }
    size_t ow = 0;

    uint16_t code0 = (uint16_t)((in[0] << 8) | in[1]);
    uint32_t prev_code = code0;
    if (prev_code > MAX_CODE || table[prev_code].data == NULL) goto decode_fail;

    if (ow + table[prev_code].len > cap) { while (ow + table[prev_code].len > cap) cap *= 2; unsigned char *tmp = realloc(obuf, cap); if (!tmp) goto decode_fail; obuf = tmp; }
    memcpy(obuf + ow, table[prev_code].data, table[prev_code].len); ow += table[prev_code].len;

    for (size_t ci = 1; ci < codes; ++ci) {
        uint16_t code = (uint16_t)((in[2*ci] << 8) | in[2*ci + 1]);
        unsigned char *entry = NULL;
        size_t entry_len = 0;

        if (table[code].data != NULL) {
            entry_len = table[code].len;
            entry = malloc(entry_len);
            if (!entry) goto decode_fail;
            memcpy(entry, table[code].data, entry_len);
        } else {
            if (code != next_code) goto decode_fail;
            size_t plen = table[prev_code].len;
            entry = malloc(plen + 1);
            if (!entry) goto decode_fail;
            memcpy(entry, table[prev_code].data, plen);
            entry[plen] = table[prev_code].data[0];
            entry_len = plen + 1;
        }

        if (ow + entry_len > cap) { while (ow + entry_len > cap) cap *= 2; unsigned char *tmp = realloc(obuf, cap); if (!tmp) { free(entry); goto decode_fail; } obuf = tmp; }
        memcpy(obuf + ow, entry, entry_len); ow += entry_len;

        if (next_code <= MAX_CODE) {
            size_t plen = table[prev_code].len;
            unsigned char *newseq = malloc(plen + 1);
            if (!newseq) { free(entry); goto decode_fail; }
            memcpy(newseq, table[prev_code].data, plen);
            newseq[plen] = entry[0];
            table[next_code].data = newseq;
            table[next_code].len = plen + 1;
            next_code++;
        } else {
            for (uint32_t t = 256; t < next_code; ++t) {
                if (table[t].data) { free(table[t].data); table[t].data = NULL; table[t].len = 0; }
            }
            next_code = INITIAL_CODES;
        }

        free(entry);
        prev_code = code;
    }

    for (uint32_t i = 0; i < next_code; ++i) if (table[i].data) free(table[i].data);
    free(table);

    *out = obuf; *out_len = ow;
    return 0;

decode_fail:
    if (obuf) free(obuf);
    if (table) { for (uint32_t i=0;i<=MAX_CODE;i++) if (table[i].data) free(table[i].data); free(table); }
    return 1;
}