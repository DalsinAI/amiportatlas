/* AmiPortAtlas: one TCP connection, plain or through GFOpenTLS, for HTTP
 * (http.c) and FTP (ftp.c). MIT, (c) 2026 The AmiCradle Team. */
#ifndef AMIPORTATLAS_NET_H
#define AMIPORTATLAS_NET_H

#include <exec/types.h>

struct OTConnection;

typedef struct {
    LONG fd;
    struct OTConnection *tls;   /* NULL: plain */
    char buf[4096];
    long have, pos;
} Conn;

/* Connects to host:port, with secure through TLS at once. Answers 1, or 0
 * with an English reason; net_close() is wanted either way. */
int  net_open(Conn *c, const char *host, int port, int secure, char *why, int whylen);
/* TLS on a connection already made (FTP's AUTH TLS, its data connections). */
int  net_tls(Conn *c, const char *host, const char *alpn, char *why, int whylen);
void net_close(Conn *c);
long net_read(Conn *c, char *buf, long n);          /* 0: closed */
long net_write(Conn *c, const char *buf, long n);
int  net_fill(Conn *c);                             /* 0: closed or broken */
int  net_line(Conn *c, char *line, int max);        /* one line, without its CRLF */

#endif
