/* AmiPortAtlas: FTP and FTPS for Atlas PUBLISH.
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * RFC 959 with RFC 4217 for TLS: AUTH TLS, PBSZ 0 and PROT P, so the data
 * connections go through GFOpenTLS too. Passive mode only (EPSV is not
 * needed: the data connection goes to the control connection's host).
 */
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net.h"
#include "ftp.h"

struct Ftp {
    Conn ctl;
    char host[256];
    int  secure;
    char line[512];
};

static void say(char *why, int whylen, const char *text)
{
    if (whylen > 0) { strncpy(why, text, (size_t)whylen - 1); why[whylen - 1] = 0; }
}

int ftp_url(const char *url, int *secure, char *user, int userlen, char *host, int hostlen,
            int *port, char *path, int pathlen)
{
    const char *p, *at, *h, *slash;
    int n;
    if (strncmp(url, "ftps://", 7) == 0) { *secure = 1; p = url + 7; }
    else if (strncmp(url, "ftp://", 6) == 0) { *secure = 0; p = url + 6; }
    else return 0;
    *port = 21;
    slash = strchr(p, '/');
    if (!slash) slash = p + strlen(p);
    at = strchr(p, '@');
    if (at && at < slash) {
        n = (int)(at - p);
        if (n <= 0 || n >= userlen) return 0;
        memcpy(user, p, (size_t)n);
        user[n] = 0;
        h = at + 1;
    } else {
        snprintf(user, (size_t)userlen, "anonymous");
        h = p;
    }
    for (p = h; p < slash && *p != ':'; p++) ;
    n = (int)(p - h);
    if (n <= 0 || n >= hostlen) return 0;
    memcpy(host, h, (size_t)n);
    host[n] = 0;
    if (*p == ':') *port = atoi(p + 1);
    snprintf(path, (size_t)pathlen, "%s", *slash ? slash : "/");
    return *port > 0;
}

/* The server's answer: its three-digit code, the last line kept in f->line. */
static int answer(Ftp *f)
{
    int code = 0;
    for (;;) {
        if (!net_line(&f->ctl, f->line, (int)sizeof f->line)) return 0;
        if (strlen(f->line) >= 4 && f->line[0] >= '1' && f->line[0] <= '5') {
            int c = atoi(f->line);
            if (f->line[3] == ' ' && (code == 0 || c == code)) return c;
            if (f->line[3] == '-' && code == 0) code = c;
        }
    }
}

static int command(Ftp *f, const char *fmt, const char *arg)
{
    char cmd[600];
    long n;
    snprintf(cmd, sizeof cmd, fmt, arg ? arg : "");
    n = (long)strlen(cmd);
    if (n + 2 >= (long)sizeof cmd) return 0;
    cmd[n++] = '\r';
    cmd[n++] = '\n';
    if (net_write(&f->ctl, cmd, n) != n) return 0;
    return answer(f);
}

static int failed(Ftp *f, char *why, int whylen, const char *what)
{
    snprintf(why, (size_t)whylen, "%s: %s", what, f->line[0] ? f->line : "the server stopped answering");
    return 0;
}

Ftp *ftp_open(const char *url, const char *password, char *why, int whylen)
{
    char user[128], path[300];
    int port, code;
    Ftp *f = (Ftp *)calloc(1, sizeof(Ftp));
    if (!f) { say(why, whylen, "Out of memory."); return NULL; }
    if (!ftp_url(url, &f->secure, user, (int)sizeof user, f->host, (int)sizeof f->host, &port, path, (int)sizeof path)) {
        snprintf(why, (size_t)whylen, "Not an ftp:// or ftps:// address: %s", url);
        free(f);
        return NULL;
    }
    if (!net_open(&f->ctl, f->host, port, 0, why, whylen)) goto fail;
    if (answer(f) != 220) { failed(f, why, whylen, "The server didn't greet"); goto fail; }
    if (f->secure) {
        if (command(f, "AUTH TLS", NULL) != 234) { failed(f, why, whylen, "The server has no TLS"); goto fail; }
        if (!net_tls(&f->ctl, f->host, NULL, why, whylen)) goto fail;
        if (command(f, "PBSZ 0", NULL) != 200 || command(f, "PROT P", NULL) != 200) {
            failed(f, why, whylen, "The server won't protect its data connections");
            goto fail;
        }
    }
    code = command(f, "USER %s", user);
    if (code == 331) code = command(f, "PASS %s", password ? password : "");
    if (code != 230) { failed(f, why, whylen, "The login failed"); goto fail; }
    if (command(f, "TYPE I", NULL) != 200) { failed(f, why, whylen, "Binary mode was refused"); goto fail; }
    if (strcmp(path, "/") != 0 && command(f, "CWD %s", path) != 250) {
        failed(f, why, whylen, "The drawer isn't there");
        goto fail;
    }
    return f;
fail:
    net_close(&f->ctl);
    free(f);
    return NULL;
}

void ftp_close(Ftp *f)
{
    if (!f) return;
    command(f, "QUIT", NULL);
    net_close(&f->ctl);
    free(f);
}

/* PASV, then the data connection opened (and secured). */
static int data_open(Ftp *f, Conn *d, char *why, int whylen)
{
    int h1, h2, h3, h4, p1, p2;
    const char *p;
    memset(d, 0, sizeof *d);
    d->fd = -1;
    if (command(f, "PASV", NULL) != 227) return failed(f, why, whylen, "Passive mode was refused");
    p = strchr(f->line, '(');
    if (!p || sscanf(p + 1, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) != 6)
        return failed(f, why, whylen, "The passive answer could not be read");
    /* to the host the control connection reached, as a server behind NAT names its inside address */
    return net_open(d, f->host, p1 * 256 + p2, 0, why, whylen);
}

static int data_secure(Ftp *f, Conn *d, char *why, int whylen)
{
    return !f->secure || net_tls(d, f->host, NULL, why, whylen);
}

typedef struct { BPTR fh; const char *mem; long len, pos; } Src;

static long src_read(Src *s, char *buf, long n)
{
    if (s->fh) return Read(s->fh, buf, n);
    if (n > s->len - s->pos) n = s->len - s->pos;
    memcpy(buf, s->mem + s->pos, (size_t)n);
    s->pos += n;
    return n;
}

static int put(Ftp *f, Src *s, long total, const char *name, int atomic, HttpSink progress, void *user,
               char *why, int whylen)
{
    char part[300], *buf;
    Conn d;
    long done = 0, n;
    int code, ok = 1;
    snprintf(part, sizeof part, atomic ? "%s.part" : "%s", name);
    if (!data_open(f, &d, why, whylen)) { net_close(&d); return 0; }
    code = command(f, "STOR %s", part);
    if (code != 150 && code != 125) { net_close(&d); return failed(f, why, whylen, "The upload was refused"); }
    if (!data_secure(f, &d, why, whylen)) { net_close(&d); answer(f); return 0; }
    buf = (char *)malloc(8192);
    if (!buf) { net_close(&d); answer(f); say(why, whylen, "Out of memory."); return 0; }
    while ((n = src_read(s, buf, 8192)) > 0) {
        if (net_write(&d, buf, n) != n) { ok = 0; say(why, whylen, "The upload broke off."); break; }
        done += n;
        if (progress && !progress(user, buf, n, done, total)) { ok = 0; say(why, whylen, "Stopped."); break; }
    }
    free(buf);
    net_close(&d);
    if (answer(f) != 226 && ok) { ok = 0; failed(f, why, whylen, "The upload didn't finish"); }
    if (ok && atomic) {
        if (command(f, "RNFR %s", part) != 350 || command(f, "RNTO %s", name) != 250)
            ok = failed(f, why, whylen, "The upload couldn't be renamed into place");
    }
    return ok;
}

int ftp_put_file(Ftp *f, const char *local, const char *name, int atomic, HttpSink progress,
                 void *user, char *why, int whylen)
{
    Src s;
    long total;
    int ok;
    memset(&s, 0, sizeof s);
    s.fh = Open((STRPTR)local, MODE_OLDFILE);
    if (!s.fh) { snprintf(why, (size_t)whylen, "%s could not be opened.", local); return 0; }
    Seek(s.fh, 0, OFFSET_END);
    total = Seek(s.fh, 0, OFFSET_BEGINNING);
    ok = put(f, &s, total, name, atomic, progress, user, why, whylen);
    Close(s.fh);
    return ok;
}

int ftp_put_mem(Ftp *f, const char *data, long len, const char *name, int atomic, char *why, int whylen)
{
    Src s;
    memset(&s, 0, sizeof s);
    s.mem = data;
    s.len = len;
    return put(f, &s, len, name, atomic, NULL, NULL, why, whylen);
}

int ftp_get_mem(Ftp *f, const char *name, long max, char **out, long *len, int *missing,
                char *why, int whylen)
{
    Conn d;
    char *buf;
    long have = 0, n;
    int code, ok = 1;
    *missing = 0;
    *out = NULL;
    if (!data_open(f, &d, why, whylen)) { net_close(&d); return 0; }
    code = command(f, "RETR %s", name);
    if (code == 550) { net_close(&d); *missing = 1; return failed(f, why, whylen, "No such file"); }
    if (code != 150 && code != 125) { net_close(&d); return failed(f, why, whylen, "The download was refused"); }
    if (!data_secure(f, &d, why, whylen)) { net_close(&d); answer(f); return 0; }
    buf = (char *)malloc((size_t)max + 1);
    if (!buf) { net_close(&d); answer(f); say(why, whylen, "Out of memory."); return 0; }
    while (have < max && (n = net_read(&d, buf + have, max - have)) > 0) have += n;
    if (have >= max) { ok = 0; say(why, whylen, "The file is larger than expected."); }
    net_close(&d);
    if (answer(f) != 226 && ok) { ok = 0; failed(f, why, whylen, "The download didn't finish"); }
    if (!ok) { free(buf); return 0; }
    buf[have] = 0;
    *out = buf;
    *len = have;
    return 1;
}
