/* AmiPortAtlas: HTTPS (and plain HTTP) GET over bsdsocket.library and
 * GFOpenTLS. MIT, (c) 2026 The AmiCradle Team. */
#ifndef AMIPORTATLAS_HTTP_H
#define AMIPORTATLAS_HTTP_H

/* Called with each piece of the body. total is the Content-Length, or -1.
 * Answer 0 to stop (the fetch then fails with "Stopped."). */
typedef int (*HttpSink)(void *user, const char *data, long len, long done, long total);

/* Fetches url, following up to five redirects (GitHub's release downloads
 * redirect once). Answers 1, or 0 with an English reason in why[0..whylen). */
int http_get(const char *url, HttpSink sink, void *user, char *why, int whylen);

/* Fetches url into a file (written to path.part, renamed when complete).
 * With sha256 (64 hex digits), the file is kept only when it matches. */
int http_get_file(const char *url, const char *path, const char *sha256,
                  HttpSink progress, void *user, char *why, int whylen);

/* Fetches url into memory (at most max bytes); *out is malloc'd, 0-terminated. */
int http_get_mem(const char *url, long max, char **out, long *len, char *why, int whylen);

#endif
