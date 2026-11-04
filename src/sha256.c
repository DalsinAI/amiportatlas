/* AmiPortAtlas: SHA-256 (FIPS 180-4), to check a download against the feed.
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * Plain C, so it runs the same on every target. A game archive of a few MB
 * takes well under a second on a 68040.
 */
#include <string.h>
#include "sha256.h"

static const unsigned long K[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
    0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL, 0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
    0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL, 0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
    0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL, 0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
    0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL, 0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

#define M32(x) ((x) & 0xffffffffUL)
#define ROR(x, n) M32(((x) >> (n)) | ((x) << (32 - (n))))

static void block(Sha256 *s, const unsigned char *b)
{
    unsigned long w[64], a, bb, c, d, e, f, g, h, t1, t2;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((unsigned long)b[4 * i] << 24) | ((unsigned long)b[4 * i + 1] << 16)
             | ((unsigned long)b[4 * i + 2] << 8) | (unsigned long)b[4 * i + 3];
    for (i = 16; i < 64; i++) {
        unsigned long s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        unsigned long s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = M32(w[i - 16] + s0 + w[i - 7] + s1);
    }
    a = s->h[0]; bb = s->h[1]; c = s->h[2]; d = s->h[3];
    e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (i = 0; i < 64; i++) {
        t1 = M32(h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i]);
        t2 = M32((ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & bb) ^ (a & c) ^ (bb & c)));
        h = g; g = f; f = e; e = M32(d + t1);
        d = c; c = bb; bb = a; a = M32(t1 + t2);
    }
    s->h[0] = M32(s->h[0] + a); s->h[1] = M32(s->h[1] + bb);
    s->h[2] = M32(s->h[2] + c); s->h[3] = M32(s->h[3] + d);
    s->h[4] = M32(s->h[4] + e); s->h[5] = M32(s->h[5] + f);
    s->h[6] = M32(s->h[6] + g); s->h[7] = M32(s->h[7] + h);
}

void sha256_init(Sha256 *s)
{
    static const unsigned long iv[8] = {
        0x6a09e667UL, 0xbb67ae85UL, 0x3c6ef372UL, 0xa54ff53aUL,
        0x510e527fUL, 0x9b05688cUL, 0x1f83d9abUL, 0x5be0cd19UL
    };
    memcpy(s->h, iv, sizeof iv);
    s->bytes_hi = s->bytes_lo = 0;
    s->used = 0;
}

void sha256_update(Sha256 *s, const void *data, unsigned long len)
{
    const unsigned char *p = (const unsigned char *)data;
    unsigned long lo = M32(s->bytes_lo + len);
    if (lo < s->bytes_lo) s->bytes_hi++;
    s->bytes_lo = lo;
    while (len > 0) {
        unsigned long n = 64 - s->used;
        if (n > len) n = len;
        memcpy(s->buf + s->used, p, n);
        s->used += n; p += n; len -= n;
        if (s->used == 64) { block(s, s->buf); s->used = 0; }
    }
}

void sha256_final(Sha256 *s, unsigned char out[32])
{
    unsigned long hi = M32((s->bytes_hi << 3) | (s->bytes_lo >> 29)), lo = M32(s->bytes_lo << 3);
    int i;
    s->buf[s->used++] = 0x80;
    if (s->used > 56) {
        memset(s->buf + s->used, 0, 64 - s->used);
        block(s, s->buf);
        s->used = 0;
    }
    memset(s->buf + s->used, 0, 56 - s->used);
    for (i = 0; i < 4; i++) {
        s->buf[56 + i] = (unsigned char)(hi >> (24 - 8 * i));
        s->buf[60 + i] = (unsigned char)(lo >> (24 - 8 * i));
    }
    block(s, s->buf);
    for (i = 0; i < 8; i++) {
        out[4 * i]     = (unsigned char)(s->h[i] >> 24);
        out[4 * i + 1] = (unsigned char)(s->h[i] >> 16);
        out[4 * i + 2] = (unsigned char)(s->h[i] >> 8);
        out[4 * i + 3] = (unsigned char)s->h[i];
    }
}

int sha256_matches(const unsigned char digest[32], const char *hex)
{
    static const char x[] = "0123456789abcdef";
    int i;
    if (!hex || strlen(hex) != 64) return 0;
    for (i = 0; i < 32; i++) {
        int hi = hex[2 * i], lo = hex[2 * i + 1];
        if (hi >= 'A' && hi <= 'F') hi += 32;
        if (lo >= 'A' && lo <= 'F') lo += 32;
        if (hi != x[digest[i] >> 4] || lo != x[digest[i] & 15]) return 0;
    }
    return 1;
}
