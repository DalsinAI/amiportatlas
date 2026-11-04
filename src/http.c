/* AmiPortAtlas: HTTPS GET. MIT, (c) 2026 The AmiCradle Team.
 *
 * HTTP/1.1, one request per connection, Content-Length, chunked or
 * read-to-close bodies, redirects. TLS is GFOpenTLS with the system trust
 * store (S:GFOpenTLS/ca-bundle.pem); a certificate that does not check out
 * stops the fetch.
 */
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <proto/socket.h>
#ifdef AP_HAVE_GFOPENTLS
#include <libraries/gfopentls.h>
#include <proto/gfopentls.h>
#endif

#include "http.h"
#include "sha256.h"

#define AP_AGENT "AmiPortAtlas/0.1 (" AP_SYSTEM ")"

extern struct Library *SocketBase;
#ifdef AP_HAVE_GFOPENTLS
extern struct Library *GFOpenTLSBase;
static struct OTContext *tls_context;
#endif

typedef struct {
    LONG fd;
#ifdef AP_HAVE_GFOPENTLS
    struct OTConnection *tls;
#endif
    char buf[4096];
    long have, pos;
} Conn;

static void say(char *why, int whylen, const char *text)
{
    if (whylen > 0) {
        strncpy(why, text, (size_t)whylen - 1);
        why[whylen - 1] = 0;
    }
}

static int conn_open(Conn *c, const char *host, int port, int secure, char *why, int whylen)
{
    struct hostent *he;
    struct sockaddr_in sa;
    const char *reason;
    memset(c, 0, sizeof *c);
    c->fd = -1;
    if (!platform_open_net(&reason)) { say(why, whylen, reason); return 0; }
    he = gethostbyname((STRPTR)host);
    if (!he) { snprintf(why, (size_t)whylen, "%s could not be found. Is the network up?", host); return 0; }
    c->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (c->fd < 0) { say(why, whylen, "No socket could be made."); return 0; }
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short)port);
    memcpy(&sa.sin_addr, he->h_addr, sizeof sa.sin_addr);
    if (connect(c->fd, (struct sockaddr *)&sa, sizeof sa) < 0) {
        snprintf(why, (size_t)whylen, "%s did not answer.", host);
        return 0;
    }
    if (!secure) return 1;
#ifdef AP_HAVE_GFOPENTLS
    {
        LONG err = 0;
        if (!tls_context) {
            tls_context = OT_NewContext(NULL, &err);
            if (!tls_context) { snprintf(why, (size_t)whylen, "GFOpenTLS: %s", OT_ErrorString(err)); return 0; }
        }
        c->tls = OT_NewConnection(tls_context, (CONST_STRPTR)host, &err);
        if (!c->tls) { snprintf(why, (size_t)whylen, "GFOpenTLS: %s", OT_ErrorString(err)); return 0; }
        OT_SetSocket(c->tls, c->fd, SocketBase);
        OT_SetALPN(c->tls, (CONST_STRPTR)"http/1.1");
        if (OT_Handshake(c->tls) != OTERR_OK) {
            CONST_STRPTR text = OT_GetErrorText(c->tls);
            snprintf(why, (size_t)whylen, "A secure connection to %s failed: %s", host, (const char *)text);
            return 0;
        }
        return 1;
    }
#else
    say(why, whylen, "This build of AmiPortAtlas has no TLS, so it cannot fetch https:// addresses.");
    return 0;
#endif
}

static void conn_close(Conn *c)
{
#ifdef AP_HAVE_GFOPENTLS
    if (c->tls) {
        OT_Close(c->tls);
        OT_FreeConnection(c->tls);
        c->tls = NULL;
    }
#endif
    if (c->fd >= 0) CloseSocket(c->fd);
    c->fd = -1;
}

static long raw_read(Conn *c, char *buf, long n)
{
#ifdef AP_HAVE_GFOPENTLS
    if (c->tls) {
        LONG got = OT_Read(c->tls, buf, n);
        return got == OTERR_CLOSED ? 0 : got;  /* a server that just hangs up has finished */
    }
#endif
    return recv(c->fd, buf, n, 0);
}

static long raw_write(Conn *c, const char *buf, long n)
{
#ifdef AP_HAVE_GFOPENTLS
    if (c->tls) return OT_Write(c->tls, buf, n);
#endif
    return send(c->fd, (APTR)buf, n, 0);
}

static int fill(Conn *c)
{
    long got;
    if (c->pos < c->have) return 1;
    got = raw_read(c, c->buf, (long)sizeof c->buf);
    if (got <= 0) return 0;
    c->have = got;
    c->pos = 0;
    return 1;
}

/* One header line, without its CRLF. */
static int read_line(Conn *c, char *line, int max)
{
    int n = 0;
    for (;;) {
        char ch;
        if (!fill(c)) return 0;
        ch = c->buf[c->pos++];
        if (ch == '\n') break;
        if (ch != '\r' && n < max - 1) line[n++] = ch;
    }
    line[n] = 0;
    return 1;
}

static int parse_url(const char *url, int *secure, char *host, int hostlen, int *port, const char **path)
{
    const char *p, *h;
    int n;
    if (strncmp(url, "https://", 8) == 0) { *secure = 1; *port = 443; p = url + 8; }
    else if (strncmp(url, "http://", 7) == 0) { *secure = 0; *port = 80; p = url + 7; }
    else return 0;
    h = p;
    while (*p && *p != '/' && *p != ':') p++;
    n = (int)(p - h);
    if (n <= 0 || n >= hostlen) return 0;
    memcpy(host, h, (size_t)n);
    host[n] = 0;
    if (*p == ':') { *port = atoi(p + 1); while (*p && *p != '/') p++; }
    *path = *p ? p : "/";
    return 1;
}

static int starts(const char *line, const char *name)
{
    while (*name) {
        char a = *line++, b = *name++;
        if (a >= 'A' && a <= 'Z') a += 32;
        if (a != b) return 0;
    }
    return 1;
}

int http_get(const char *url, HttpSink sink, void *user, char *why, int whylen)
{
    static char next[1024], line[1024];
    int hop;
    strncpy(next, url, sizeof next - 1);
    next[sizeof next - 1] = 0;
    for (hop = 0; hop < 6; hop++) {
        char host[256], req[1400];
        const char *path;
        int secure, port, status = 0, chunked = 0, ok = 0, redirect = 0;
        long total = -1, done = 0;
        Conn *c;
        if (!parse_url(next, &secure, host, (int)sizeof host, &port, &path)) {
            snprintf(why, (size_t)whylen, "Not a web address: %s", next);
            return 0;
        }
        c = (Conn *)malloc(sizeof(Conn));
        if (!c) { say(why, whylen, "Out of memory."); return 0; }
        if (!conn_open(c, host, port, secure, why, whylen)) { conn_close(c); free(c); return 0; }
        snprintf(req, sizeof req, "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: " AP_AGENT
                 "\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n", path, host);
        if (raw_write(c, req, (long)strlen(req)) != (long)strlen(req)) {
            say(why, whylen, "The request could not be sent.");
            conn_close(c); free(c); return 0;
        }
        if (!read_line(c, line, (int)sizeof line) || sscanf(line, "HTTP/%*d.%*d %d", &status) != 1) {
            say(why, whylen, "The server's answer could not be read.");
            conn_close(c); free(c); return 0;
        }
        for (;;) {
            if (!read_line(c, line, (int)sizeof line)) { status = 0; break; }
            if (!line[0]) break;
            if (starts(line, "content-length:")) total = atol(line + 15);
            else if (starts(line, "transfer-encoding:") && strstr(line, "chunked")) chunked = 1;
            else if (starts(line, "location:") && status >= 300 && status < 400) {
                const char *v = line + 9;
                while (*v == ' ') v++;
                if (*v == '/') snprintf(next, sizeof next, "%s://%s%s", secure ? "https" : "http", host, v);
                else { strncpy(next, v, sizeof next - 1); next[sizeof next - 1] = 0; }
                redirect = 1;
            }
        }
        if (redirect) { conn_close(c); free(c); continue; }
        if (status != 200) {
            if (status == 404) say(why, whylen, "The file is not on the server (404).");
            else snprintf(why, (size_t)whylen, "The server answered %d.", status);
            conn_close(c); free(c); return 0;
        }
        ok = 1;
        for (;;) {
            long want;
            if (chunked) {
                if (!read_line(c, line, (int)sizeof line)) { ok = 0; break; }
                want = strtol(line, NULL, 16);
                if (want == 0) break;
            } else {
                want = total >= 0 ? total - done : 0x7fffffffL;
                if (want == 0) break;
            }
            while (want > 0) {
                long n;
                if (!fill(c)) { if (!chunked && total < 0) want = 0; else ok = 0; break; }
                n = c->have - c->pos;
                if (n > want) n = want;
                done += n;
                if (!sink(user, c->buf + c->pos, n, done, total)) {
                    say(why, whylen, "Stopped.");
                    conn_close(c); free(c); return 0;
                }
                c->pos += n;
                want -= n;
            }
            if (!ok || (!chunked && (total < 0 || done >= total))) break;
            if (chunked && !read_line(c, line, (int)sizeof line)) { ok = 0; break; }
        }
        conn_close(c);
        free(c);
        if (!ok) say(why, whylen, "The connection broke before the whole file came.");
        return ok;
    }
    say(why, whylen, "Too many redirects.");
    return 0;
}

/* ---- into a file ---- */

typedef struct {
    BPTR fh;
    Sha256 sha;
    HttpSink progress;
    void *user;
} FileJob;

static int file_sink(void *user, const char *data, long len, long done, long total)
{
    FileJob *j = (FileJob *)user;
    if (Write(j->fh, (APTR)data, len) != len) return 0;
    sha256_update(&j->sha, data, (unsigned long)len);
    return j->progress ? j->progress(j->user, data, len, done, total) : 1;
}

int http_get_file(const char *url, const char *path, const char *sha256,
                  HttpSink progress, void *user, char *why, int whylen)
{
    char part[300];
    unsigned char digest[32];
    FileJob j;
    int ok;
    snprintf(part, sizeof part, "%s.part", path);
    j.fh = Open((STRPTR)part, MODE_NEWFILE);
    if (!j.fh) { snprintf(why, (size_t)whylen, "%s could not be written.", part); return 0; }
    sha256_init(&j.sha);
    j.progress = progress;
    j.user = user;
    ok = http_get(url, file_sink, &j, why, whylen);
    Close(j.fh);
    if (ok && sha256) {
        sha256_final(&j.sha, digest);
        if (!sha256_matches(digest, sha256)) {
            say(why, whylen, "The download does not match the Port Atlas checksum, so it was not used.");
            ok = 0;
        }
    }
    if (ok) {
        DeleteFile((STRPTR)path);
        ok = Rename((STRPTR)part, (STRPTR)path) != 0;
        if (!ok) snprintf(why, (size_t)whylen, "%s could not be renamed.", part);
    }
    if (!ok) DeleteFile((STRPTR)part);
    return ok;
}

/* ---- into memory ---- */

typedef struct {
    char *buf;
    long len, cap, max;
} MemJob;

static int mem_sink(void *user, const char *data, long len, long done, long total)
{
    MemJob *m = (MemJob *)user;
    (void)done;
    if (m->len + len > m->max) return 0;
    if (m->len + len + 1 > m->cap) {
        long cap = m->cap ? m->cap : (total > 0 && total < m->max ? total + 1 : 65536);
        char *n;
        while (cap < m->len + len + 1) cap *= 2;
        n = (char *)realloc(m->buf, (size_t)cap);
        if (!n) return 0;
        m->buf = n;
        m->cap = cap;
    }
    memcpy(m->buf + m->len, data, (size_t)len);
    m->len += len;
    return 1;
}

int http_get_mem(const char *url, long max, char **out, long *len, char *why, int whylen)
{
    MemJob m;
    memset(&m, 0, sizeof m);
    m.max = max;
    if (!http_get(url, mem_sink, &m, why, whylen)) {
        if (m.len >= max) say(why, whylen, "The file is larger than expected.");
        free(m.buf);
        return 0;
    }
    if (!m.buf) { m.buf = (char *)malloc(1); if (!m.buf) { say(why, whylen, "Out of memory."); return 0; } }
    m.buf[m.len] = 0;
    *out = m.buf;
    *len = m.len;
    return 1;
}
