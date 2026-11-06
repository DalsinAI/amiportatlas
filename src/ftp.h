/* AmiPortAtlas: FTP and FTPS (explicit, AUTH TLS) for Atlas PUBLISH.
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * Passive mode, binary, one connection at a time. A source's owner gives
 * where its files go as ftp://USER@HOST[:PORT]/DRAWER/ or ftps://...; the
 * password is asked each time and never kept.
 */
#ifndef AMIPORTATLAS_FTP_H
#define AMIPORTATLAS_FTP_H

#include "http.h"

typedef struct Ftp Ftp;

/* Splits an upload address. Answers 0 when it isn't ftp:// or ftps://. */
int  ftp_url(const char *url, int *secure, char *user, int userlen, char *host, int hostlen,
             int *port, char *path, int pathlen);

/* Logs in and goes to the address's drawer. NULL with a reason on failure. */
Ftp *ftp_open(const char *url, const char *password, char *why, int whylen);
void ftp_close(Ftp *f);

/* A local file stored as name. With atomic it is written as name.part and
 * then renamed, so a reader never sees half of it (Aminet's upload drawer
 * renames nothing, so it gets files straight). progress as for http_get. */
int  ftp_put_file(Ftp *f, const char *local, const char *name, int atomic, HttpSink progress,
                  void *user, char *why, int whylen);
/* The same from memory. */
int  ftp_put_mem(Ftp *f, const char *data, long len, const char *name, int atomic, char *why, int whylen);
/* A file fetched into memory (at most max bytes, *out malloc'd and
 * 0-terminated). *missing is set when the server has no such file. */
int  ftp_get_mem(Ftp *f, const char *name, long max, char **out, long *len, int *missing,
                 char *why, int whylen);

#endif
