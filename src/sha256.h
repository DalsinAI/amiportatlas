/* AmiPortAtlas: SHA-256. MIT, (c) 2026 The AmiCradle Team. */
#ifndef AMIPORTATLAS_SHA256_H
#define AMIPORTATLAS_SHA256_H

typedef struct {
    unsigned long h[8];
    unsigned long bytes_hi, bytes_lo;
    unsigned long used;
    unsigned char buf[64];
} Sha256;

void sha256_init(Sha256 *s);
void sha256_update(Sha256 *s, const void *data, unsigned long len);
void sha256_final(Sha256 *s, unsigned char out[32]);

/* 1 when digest is the 64 hex digits in hex (either case). */
int  sha256_matches(const unsigned char digest[32], const char *hex);

#endif
