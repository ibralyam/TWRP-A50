#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <lzma.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <linux/fs.h>

#pragma GCC diagnostic ignored "-Wunused-function"
#define MAX_RAW 67108864ULL
#define VBMETA_SIZE 9296ULL
#define IO_CHUNK (1u << 20)
#define IN_CHUNK (1u << 16)

#ifndef BYNAME_DEFAULT
#define BYNAME_DEFAULT "/dev/block/platform/13520000.ufs/by-name"
#endif

#if defined(EXPECT_XZ_SHA256) && defined(EXPECT_RAW_SHA256) && defined(EXPECT_VBMETA_SHA256)
#define FLASH_ENABLED 1
#endif

static int g_outfd = -1;

static void say(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fprintf(stderr, "%s\n", buf);
    if (g_outfd >= 0) {
        dprintf(g_outfd, "ui_print %s\nui_print\n", buf);
    }
}

typedef struct {
    uint32_t h[8];
    uint64_t len;
    uint8_t buf[64];
    size_t n;
} sha_t;

static const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha_block(sha_t *s, const uint8_t *p)
{
    uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2;
    int i;
    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3];
    e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (i = 0; i < 64; i++) {
        t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
        t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d;
    s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void sha_init(sha_t *s)
{
    static const uint32_t iv[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    memcpy(s->h, iv, sizeof(iv));
    s->len = 0;
    s->n = 0;
}

static void sha_update(sha_t *s, const void *data, size_t len)
{
    const uint8_t *p = data;
    s->len += len;
    while (len > 0) {
        size_t take = 64 - s->n;
        if (take > len) take = len;
        memcpy(s->buf + s->n, p, take);
        s->n += take;
        p += take;
        len -= take;
        if (s->n == 64) {
            sha_block(s, s->buf);
            s->n = 0;
        }
    }
}

static void sha_hex(sha_t *s, char out[65])
{
    uint8_t pad[72];
    uint64_t bits = s->len * 8;
    size_t padlen = (s->n < 56) ? (56 - s->n) : (120 - s->n);
    int i;
    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;
    sha_update(s, pad, padlen);
    for (i = 0; i < 8; i++) pad[i] = (uint8_t)(bits >> (56 - 8 * i));
    sha_update(s, pad, 8);
    for (i = 0; i < 8; i++) snprintf(out + i * 8, 9, "%08x", s->h[i]);
}

static int xwrite(int fd, const void *buf, size_t len)
{
    const uint8_t *p = buf;
    while (len > 0) {
        ssize_t r = write(fd, p, len);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += r;
        len -= (size_t)r;
    }
    return 0;
}

static int xpread(int fd, void *buf, size_t len, uint64_t off)
{
    uint8_t *p = buf;
    while (len > 0) {
        ssize_t r = pread(fd, p, len, (off_t)off);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return -1;
        p += r;
        off += (uint64_t)r;
        len -= (size_t)r;
    }
    return 0;
}

static int xzcat_stream(int in, int out)
{
    lzma_stream s = LZMA_STREAM_INIT;
    uint8_t *ib = malloc(IN_CHUNK), *ob = malloc(IO_CHUNK);
    lzma_action act = LZMA_RUN;
    lzma_ret r;
    int rc = 1;
    if (!ib || !ob) goto done;
    if (lzma_stream_decoder(&s, UINT64_MAX, LZMA_CONCATENATED) != LZMA_OK) goto done;
    s.next_out = ob;
    s.avail_out = IO_CHUNK;
    for (;;) {
        if (s.avail_in == 0 && act == LZMA_RUN) {
            ssize_t n = read(in, ib, IN_CHUNK);
            if (n < 0) {
                if (errno == EINTR) continue;
                goto done;
            }
            if (n == 0) act = LZMA_FINISH;
            s.next_in = ib;
            s.avail_in = (size_t)n;
        }
        r = lzma_code(&s, act);
        if (s.avail_out == 0 || r == LZMA_STREAM_END) {
            if (xwrite(out, ob, IO_CHUNK - s.avail_out) != 0) {
                rc = (errno == EPIPE) ? 0 : 1;
                goto done;
            }
            s.next_out = ob;
            s.avail_out = IO_CHUNK;
        }
        if (r == LZMA_STREAM_END) {
            rc = 0;
            break;
        }
        if (r != LZMA_OK) {
            fprintf(stderr, "xz: data is corrupt (%d)\n", (int)r);
            goto done;
        }
    }
done:
    lzma_end(&s);
    free(ib);
    free(ob);
    return rc;
}

static int applet_xz(const char *name, int argc, char **argv)
{
    int to_stdout = 0, keep = 0, i;
    const char *file = NULL;
    signal(SIGPIPE, SIG_IGN);
    for (i = 0; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            const char *f;
            for (f = argv[i] + 1; *f; f++) {
                if (*f == 'c') to_stdout = 1;
                else if (*f == 'k') keep = 1;
            }
        } else {
            file = argv[i];
        }
    }
    if (!strcmp(name, "xzcat")) to_stdout = 1;
    if (!file) return xzcat_stream(0, 1);
    {
        int in = open(file, O_RDONLY);
        int rc;
        if (in < 0) {
            fprintf(stderr, "%s: cannot open %s\n", name, file);
            return 1;
        }
        if (to_stdout) {
            rc = xzcat_stream(in, 1);
        } else {
            char outp[4096];
            size_t l = strlen(file);
            int out;
            if (l < 4 || strcmp(file + l - 3, ".xz") != 0 || l >= sizeof(outp)) {
                fprintf(stderr, "%s: unknown suffix\n", name);
                close(in);
                return 1;
            }
            memcpy(outp, file, l - 3);
            outp[l - 3] = '\0';
            out = open(outp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out < 0) {
                close(in);
                return 1;
            }
            rc = xzcat_stream(in, out);
            close(out);
            if (rc == 0 && !keep) unlink(file);
            if (rc != 0) unlink(outp);
        }
        close(in);
        return rc;
    }
}

static int applet_main(const char *base, int argc, char **argv)
{
    if (!strcmp(base, "xzcat") || !strcmp(base, "unxz")) {
        return applet_xz(base, argc - 1, argv + 1);
    }
    if (argc >= 2 && (!strcmp(argv[1], "xzcat") || !strcmp(argv[1], "unxz"))) {
        return applet_xz(argv[1], argc - 2, argv + 2);
    }
    if (argc >= 2) {
        execvp("toybox", argv + 1);
        fprintf(stderr, "busybox: applet not found: %s\n", argv[1]);
        return 127;
    }
    fprintf(stderr, "twrp_applet: xzcat unxz, other names go to toybox\n");
    return 1;
}

#ifdef FLASH_ENABLED

typedef struct {
    uint32_t method;
    uint32_t crc;
    uint64_t csize;
    uint64_t usize;
    uint64_t data_off;
} zent_t;

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t rd16(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static int zip_find(int fd, const char *name, zent_t *out)
{
    struct stat st;
    uint8_t tail[65557 + 22];
    size_t tl, i;
    uint64_t cd_off, cd_size, pos;
    uint32_t count;
    uint8_t *cd;
    int found = 0;
    if (fstat(fd, &st) != 0 || st.st_size < 22) return -1;
    tl = (size_t)((st.st_size < (off_t)sizeof(tail)) ? st.st_size : (off_t)sizeof(tail));
    if (xpread(fd, tail, tl, (uint64_t)st.st_size - tl) != 0) return -1;
    for (i = tl - 22 + 1; i-- > 0;) {
        if (rd32(tail + i) == 0x06054b50) break;
    }
    if (i == (size_t)-1 || rd32(tail + i) != 0x06054b50) return -1;
    count = rd16(tail + i + 10);
    cd_size = rd32(tail + i + 12);
    cd_off = rd32(tail + i + 16);
    if (cd_size == 0xFFFFFFFFu || cd_off == 0xFFFFFFFFu || count == 0xFFFFu) return -1;
    if (cd_size > (16u << 20)) return -1;
    cd = malloc(cd_size ? cd_size : 1);
    if (!cd) return -1;
    if (xpread(fd, cd, cd_size, cd_off) != 0) {
        free(cd);
        return -1;
    }
    pos = 0;
    while (pos + 46 <= cd_size) {
        uint32_t nl, el, cl, lho;
        if (rd32(cd + pos) != 0x02014b50) break;
        nl = rd16(cd + pos + 28);
        el = rd16(cd + pos + 30);
        cl = rd16(cd + pos + 32);
        lho = rd32(cd + pos + 42);
        if (pos + 46 + nl > cd_size) break;
        if (nl == strlen(name) && memcmp(cd + pos + 46, name, nl) == 0) {
            uint8_t lh[30];
            out->method = rd16(cd + pos + 10);
            out->crc = rd32(cd + pos + 16);
            out->csize = rd32(cd + pos + 20);
            out->usize = rd32(cd + pos + 24);
            if (out->csize == 0xFFFFFFFFu || out->usize == 0xFFFFFFFFu || lho == 0xFFFFFFFFu) break;
            if (xpread(fd, lh, 30, lho) != 0 || rd32(lh) != 0x04034b50) break;
            out->data_off = (uint64_t)lho + 30 + rd16(lh + 26) + rd16(lh + 28);
            found = 1;
            break;
        }
        pos += 46 + nl + el + cl;
    }
    free(cd);
    return found ? 0 : -1;
}

static int entry_check(int fd, const zent_t *e, const char *expect_sha, uint8_t *head, size_t headlen)
{
    uint8_t *buf = malloc(IO_CHUNK);
    uint64_t done = 0;
    uint32_t crc = 0;
    sha_t sh;
    char hex[65];
    if (!buf) return -1;
    sha_init(&sh);
    if (e->method != 0 || e->csize != e->usize) {
        free(buf);
        return -2;
    }
    while (done < e->csize) {
        size_t n = (e->csize - done > IO_CHUNK) ? IO_CHUNK : (size_t)(e->csize - done);
        if (xpread(fd, buf, n, e->data_off + done) != 0) {
            free(buf);
            return -1;
        }
        if (done == 0 && head) memcpy(head, buf, n < headlen ? n : headlen);
        crc = lzma_crc32(buf, n, crc);
        sha_update(&sh, buf, n);
        done += n;
    }
    free(buf);
    if (crc != e->crc) return -3;
    sha_hex(&sh, hex);
    if (strcasecmp(hex, expect_sha) != 0) return -4;
    return 0;
}

typedef struct {
    sha_t sha;
    uint64_t total;
    uint64_t limit;
    int out;
    uint8_t head[8];
    int err;
    int last_pct;
    uint64_t expect;
    const char *label;
} sink_t;

static int sink_feed(sink_t *k, const uint8_t *p, size_t n)
{
    if (k->total < 8) {
        size_t c = 8 - (size_t)k->total;
        if (c > n) c = n;
        memcpy(k->head + k->total, p, c);
    }
    if (k->total + n > k->limit) {
        k->err = 1;
        return -1;
    }
    sha_update(&k->sha, p, n);
    if (k->out >= 0 && xwrite(k->out, p, n) != 0) {
        k->err = 2;
        return -1;
    }
    k->total += n;
    if (k->expect) {
        int pct = (int)((k->total * 100) / k->expect);
        if (pct >= k->last_pct + 10) {
            k->last_pct = pct - (pct % 10);
            say("  %s %d%%", k->label, k->last_pct);
        }
    }
    return 0;
}

static int decode_region(int fd, uint64_t off, uint64_t len, sink_t *k)
{
    lzma_stream s = LZMA_STREAM_INIT;
    uint8_t *ib = malloc(IN_CHUNK), *ob = malloc(IO_CHUNK);
    uint64_t remaining = len;
    lzma_ret r;
    lzma_action act = LZMA_RUN;
    int rc = -1;
    if (!ib || !ob) goto done;
    if (lzma_stream_decoder(&s, UINT64_MAX, 0) != LZMA_OK) goto done;
    s.next_out = ob;
    s.avail_out = IO_CHUNK;
    for (;;) {
        if (s.avail_in == 0 && remaining > 0) {
            size_t n = (remaining > IN_CHUNK) ? IN_CHUNK : (size_t)remaining;
            if (xpread(fd, ib, n, off) != 0) goto done;
            off += n;
            remaining -= n;
            s.next_in = ib;
            s.avail_in = n;
        }
        if (remaining == 0 && s.avail_in == 0) act = LZMA_FINISH;
        r = lzma_code(&s, act);
        if (s.avail_out == 0 || r == LZMA_STREAM_END) {
            if (sink_feed(k, ob, IO_CHUNK - s.avail_out) != 0) goto done;
            s.next_out = ob;
            s.avail_out = IO_CHUNK;
        }
        if (r == LZMA_STREAM_END) {
            if (s.avail_in != 0 || remaining != 0) goto done;
            if (lzma_get_check(&s) == LZMA_CHECK_NONE) goto done;
            rc = 0;
            break;
        }
        if (r != LZMA_OK) goto done;
    }
done:
    lzma_end(&s);
    free(ib);
    free(ob);
    return rc;
}

static uint64_t node_size(int fd)
{
    uint64_t sz = 0;
    struct stat st;
    if (fstat(fd, &st) != 0) return 0;
    if (S_ISBLK(st.st_mode)) {
        if (ioctl(fd, BLKGETSIZE64, &sz) != 0) return 0;
        return sz;
    }
    return (uint64_t)st.st_size;
}

static int readback_sha(const char *path, uint64_t len, char hex[65])
{
    int fd = open(path, O_RDONLY);
    uint8_t *buf = malloc(IO_CHUNK);
    uint64_t done = 0;
    sha_t sh;
    if (fd < 0 || !buf) {
        if (fd >= 0) close(fd);
        free(buf);
        return -1;
    }
    sha_init(&sh);
    while (done < len) {
        size_t n = (len - done > IO_CHUNK) ? IO_CHUNK : (size_t)(len - done);
        if (xpread(fd, buf, n, done) != 0) {
            close(fd);
            free(buf);
            return -1;
        }
        sha_update(&sh, buf, n);
        done += n;
    }
    close(fd);
    free(buf);
    sha_hex(&sh, hex);
    return 0;
}

static int get_prop(const char *name, char *out, size_t outlen)
{
    char cmd[128], line[256];
    FILE *f;
    const char *files[] = { getenv("A50_PROP_FILE"), "/prop.default", "/default.prop", NULL };
    int i;
    out[0] = '\0';
    snprintf(cmd, sizeof(cmd), "getprop %s 2>/dev/null", name);
    f = popen(cmd, "r");
    if (f) {
        if (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            snprintf(out, outlen, "%s", line);
        }
        pclose(f);
        if (out[0]) return 0;
    }
    for (i = 0; files[i] || i < 3; i++) {
        size_t nl = strlen(name);
        if (!files[i]) continue;
        f = fopen(files[i], "r");
        if (!f) continue;
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, name, nl) == 0 && line[nl] == '=') {
                line[strcspn(line, "\r\n")] = '\0';
                snprintf(out, outlen, "%s", line + nl + 1);
                break;
            }
        }
        fclose(f);
        if (out[0]) return 0;
    }
    return -1;
}

static int check_device(void)
{
    char dev[256], prod[256], model[256];
    int ok = 0;
    get_prop("ro.product.device", dev, sizeof(dev));
    get_prop("ro.build.product", prod, sizeof(prod));
    get_prop("ro.boot.product.model", model, sizeof(model));
    say("  Device: %s / %s / %s", dev[0] ? dev : "?", prod[0] ? prod : "?", model[0] ? model : "?");
    if (strcasestr(dev, "a50s") || strcasestr(prod, "a50s") || strcasestr(model, "A507")) {
        say("  This is an A50s, not an A50.");
        return -1;
    }
    if (!strcasecmp(dev, "a50") || !strcasecmp(dev, "a50dd") ||
        !strcasecmp(prod, "a50") || !strcasecmp(prod, "a50dd")) ok = 1;
    if (model[0] && strncasecmp(model, "SM-A505", 7) != 0) {
        say("  Model is not SM-A505.");
        return -1;
    }
    if (!ok) {
        say("  This is not a Galaxy A50.");
        return -1;
    }
    return 0;
}

static int write_image(const char *dst, int zfd, const zent_t *e,
                       const sha_t *pass1, uint64_t total, int do_backup)
{
    int fd;
    sink_t k;
    char hex[65], hex2[65];
    char bak[] = "/tmp/recovery.bak";
    int have_bak = 0;
    sha_t p1 = *pass1;
    sha_hex(&p1, hex);
    if (do_backup) {
        struct statvfs sv;
        int src = open(dst, O_RDONLY);
        uint64_t psz = src >= 0 ? node_size(src) : 0;
        if (src >= 0 && statvfs("/tmp", &sv) == 0 &&
            (uint64_t)sv.f_bavail * sv.f_frsize > psz + (16u << 20)) {
            int bf = open(bak, O_WRONLY | O_CREAT | O_TRUNC, 0600);
            uint8_t *buf = malloc(IO_CHUNK);
            uint64_t d = 0;
            int ok = (bf >= 0 && buf);
            while (ok && d < psz) {
                size_t n = (psz - d > IO_CHUNK) ? IO_CHUNK : (size_t)(psz - d);
                if (xpread(src, buf, n, d) != 0 || xwrite(bf, buf, n) != 0) ok = 0;
                d += n;
            }
            free(buf);
            if (bf >= 0) close(bf);
            if (ok) {
                have_bak = 1;
                say("  Old recovery saved in memory.");
            } else {
                unlink(bak);
            }
        } else {
            say("  Not enough memory for a backup. Going on.");
        }
        if (src >= 0) close(src);
    }
    fd = open(dst, O_WRONLY);
    if (fd < 0) {
        say("  Cannot open %s", dst);
        return -1;
    }
    memset(&k, 0, sizeof(k));
    sha_init(&k.sha);
    k.out = fd;
    k.limit = MAX_RAW;
    k.expect = total;
    k.label = "Writing recovery";
    if (decode_region(zfd, e->data_off, e->csize, &k) != 0 || k.total != total) {
        say("  Write failed.");
        close(fd);
        goto restore;
    }
    sha_hex(&k.sha, hex2);
    if (strcmp(hex, hex2) != 0) {
        say("  Second pass does not match the first.");
        close(fd);
        goto restore;
    }
    fsync(fd);
    close(fd);
    sync();
    say("  Reading back to check...");
    if (readback_sha(dst, total, hex2) != 0 || strcmp(hex, hex2) != 0) {
        say("  Read back check failed.");
        goto restore;
    }
    if (have_bak) unlink(bak);
    return 0;
restore:
    if (have_bak) {
        int src = open(bak, O_RDONLY), out = open(dst, O_WRONLY);
        uint8_t *buf = malloc(IO_CHUNK);
        uint64_t d = 0, bsz = src >= 0 ? node_size(src) : 0;
        say("  Putting the old recovery back...");
        while (src >= 0 && out >= 0 && buf && d < bsz) {
            size_t n = (bsz - d > IO_CHUNK) ? IO_CHUNK : (size_t)(bsz - d);
            if (xpread(src, buf, n, d) != 0 || xwrite(out, buf, n) != 0) break;
            d += n;
        }
        free(buf);
        if (out >= 0) { fsync(out); close(out); }
        if (src >= 0) close(src);
        sync();
        unlink(bak);
        say(d == bsz ? "  Old recovery restored." : "  Restore failed. Flash recovery with Odin.");
    } else {
        say("  No backup. If recovery will not start, flash it with Odin.");
    }
    return -1;
}

static int flash_main(int argc, char **argv)
{
    const char *byname = getenv("A50_BYNAME");
    char rec_path[512], vb_path[512];
    int zfd, outfd, have_vb;
    zent_t rec, vb;
    sink_t k;
    char hex[65];
    uint8_t vbhead[8], rechead[8];
    uint64_t psz;
    int fd, rc;
    time_t t0 = time(NULL);

    if (argc < 4) {
        fprintf(stderr, "usage: update-binary API OUTFD ZIP\n");
        return 2;
    }
    signal(SIGXFSZ, SIG_IGN);
    outfd = atoi(argv[2]);
    if (outfd > 0) g_outfd = outfd;
    if (!byname || !*byname) byname = BYNAME_DEFAULT;
    snprintf(rec_path, sizeof(rec_path), "%s/recovery", byname);
    snprintf(vb_path, sizeof(vb_path), "%s/vbmeta", byname);

    say("==============================");
    say("  TWRP hybrid for Galaxy A50");
    say("  By ibralyam - t.me/ibralyam");
    say("==============================");
    say("[1/5] Checking device");
    if (check_device() != 0) {
        say("Nothing was written. Your phone is unchanged.");
        return 1;
    }
    if (access(rec_path, F_OK) != 0) {
        say("  Recovery partition not found.");
        say("Nothing was written. Your phone is unchanged.");
        return 1;
    }

    say("[2/5] Checking the package");
    zfd = open(argv[3], O_RDONLY);
    if (zfd < 0 || zip_find(zfd, "recovery.img.xz", &rec) != 0) {
        say("  recovery.img.xz not found in the zip.");
        say("Nothing was written. Your phone is unchanged.");
        return 1;
    }
    rc = entry_check(zfd, &rec, EXPECT_XZ_SHA256, rechead, sizeof(rechead));
    if (rc != 0) {
        say("  recovery.img.xz check failed (%d). The zip was changed or is broken.", rc);
        say("Nothing was written. Your phone is unchanged.");
        return 1;
    }
    have_vb = (zip_find(zfd, "vbmeta.img", &vb) == 0);
    if (!have_vb || vb.usize != VBMETA_SIZE ||
        entry_check(zfd, &vb, EXPECT_VBMETA_SHA256, vbhead, sizeof(vbhead)) != 0 ||
        memcmp(vbhead, "AVB0", 4) != 0) {
        say("  vbmeta.img check failed. The zip was changed or is broken.");
        say("Nothing was written. Your phone is unchanged.");
        return 1;
    }
    say("  Package is OK.");

    say("[3/5] Testing the xz stream (nothing is written)");
    memset(&k, 0, sizeof(k));
    sha_init(&k.sha);
    k.out = -1;
    k.limit = MAX_RAW;
    k.label = "Testing";
    if (decode_region(zfd, rec.data_off, rec.csize, &k) != 0) {
        say(k.err == 1 ? "  Image is bigger than 64 MB." : "  The xz stream is corrupt.");
        say("Nothing was written. Your phone is unchanged.");
        return 1;
    }
    if (k.total < 4096 || memcmp(k.head, "ANDROID!", 8) != 0) {
        say("  This is not a boot image.");
        say("Nothing was written. Your phone is unchanged.");
        return 1;
    }
    {
        sha_t tmp = k.sha;
        sha_hex(&tmp, hex);
    }
    if (strcasecmp(hex, EXPECT_RAW_SHA256) != 0) {
        say("  Unpacked image does not match. The zip was changed.");
        say("Nothing was written. Your phone is unchanged.");
        return 1;
    }
    say("  Stream is OK: %llu bytes.", (unsigned long long)k.total);

    fd = open(rec_path, O_RDONLY);
    psz = fd >= 0 ? node_size(fd) : 0;
    if (fd >= 0) close(fd);
    if (psz == 0 || k.total > psz) {
        say("  Image does not fit the recovery partition.");
        say("Nothing was written. Your phone is unchanged.");
        return 1;
    }

    say("[4/5] Writing vbmeta");
    if (access(vb_path, F_OK) != 0) {
        say("  No vbmeta partition on this phone. Skipped.");
    } else {
        int vfd = open(vb_path, O_WRONLY);
        uint8_t *vbuf = malloc(VBMETA_SIZE);
        char h2[65];
        if (vfd < 0 || !vbuf || xpread(zfd, vbuf, VBMETA_SIZE, vb.data_off) != 0 ||
            xwrite(vfd, vbuf, VBMETA_SIZE) != 0) {
            say("  vbmeta write failed.");
            if (vfd >= 0) close(vfd);
            free(vbuf);
            return 1;
        }
        fsync(vfd);
        close(vfd);
        free(vbuf);
        sync();
        if (readback_sha(vb_path, VBMETA_SIZE, h2) != 0 || strcasecmp(h2, EXPECT_VBMETA_SHA256) != 0) {
            say("  vbmeta read back check failed.");
            return 1;
        }
        say("  vbmeta written and checked.");
    }

    say("[5/5] Writing recovery");
    if (write_image(rec_path, zfd, &rec, &k.sha, k.total, 1) != 0) {
        say("ERROR: recovery was not installed.");
        return 1;
    }
    sync();
    say("  Recovery written and checked.");
    say("");
    say("==============================");
    say("All done in %lds. No automatic reboot.", (long)(time(NULL) - t0));
    say("Reboot to recovery to use it.");
    say("By ibralyam - t.me/ibralyam");
    say("==============================");
    close(zfd);
    return 0;
}

#endif

int main(int argc, char **argv)
{
    const char *base = strrchr(argv[0], '/');
    base = base ? base + 1 : argv[0];
    if (!strcmp(base, "xzcat") || !strcmp(base, "unxz") || !strcmp(base, "busybox") ||
        !strcmp(base, "twrp_applet")) {
        return applet_main(base, argc, argv);
    }
#ifdef FLASH_ENABLED
    return flash_main(argc, argv);
#else
    return applet_main(base, argc, argv);
#endif
}
