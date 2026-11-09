/*
 * util.c — Arena allocator, string buffer and error reporting.
 */
#include "tamizhi.h"
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

int g_error_count = 0;

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) { fprintf(stderr, "tamizhi: out of memory\n"); exit(1); }
    return p;
}

/* ─── Diagnostics ──────────────────────────────────────────────────────── */
int levenshtein_dist(const char *s1, const char *s2) {
    size_t l1 = strlen(s1);
    size_t l2 = strlen(s2);
    if (l1 == 0) return (int)l2;
    if (l2 == 0) return (int)l1;
    int *prev = malloc((l2 + 1) * sizeof(int));
    int *curr = malloc((l2 + 1) * sizeof(int));
    if (!prev || !curr) { free(prev); free(curr); return 999; }
    for (size_t j = 0; j <= l2; j++) prev[j] = (int)j;
    for (size_t i = 1; i <= l1; i++) {
        curr[0] = (int)i;
        for (size_t j = 1; j <= l2; j++) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            int del = prev[j] + 1;
            int ins = curr[j - 1] + 1;
            int sub = prev[j - 1] + cost;
            int min = del < ins ? del : ins;
            curr[j] = min < sub ? min : sub;
        }
        memcpy(prev, curr, (l2 + 1) * sizeof(int));
    }
    int res = prev[l2];
    free(prev);
    free(curr);
    return res;
}

void report_error(const char *filename, int line, int col,
                  const char *fmt, ...) {
    bool is_tty = isatty(fileno(stderr));
    const char *fn = filename ? filename : "<input>";
    if (is_tty) {
        fprintf(stderr, "\033[1m%s:%d:%d: \033[1;31merror:\033[0m \033[1m", fn, line, col);
    } else {
        fprintf(stderr, "%s:%d:%d: error: ", fn, line, col);
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    if (is_tty) fprintf(stderr, "\033[0m\n");
    else fputc('\n', stderr);
    g_error_count++;

    /* Source code snippet with line gutter and caret */
    if (filename && line > 0) {
        FILE *f = fopen(filename, "r");
        if (f) {
            char buf[1024];
            int cur_line = 1;
            while (fgets(buf, sizeof(buf), f)) {
                if (cur_line == line) {
                    size_t slen = strlen(buf);
                    while (slen > 0 && (buf[slen - 1] == '\r' || buf[slen - 1] == '\n')) {
                        buf[--slen] = '\0';
                    }
                    if (is_tty) {
                        fprintf(stderr, "\033[1;36m%5d |\033[0m %s\n", line, buf);
                        fprintf(stderr, "\033[1;36m      |\033[0m ");
                    } else {
                        fprintf(stderr, "%5d | %s\n", line, buf);
                        fprintf(stderr, "      | ");
                    }
                    for (int c = 1; c < col && c <= (int)slen; c++) {
                        if (buf[c - 1] == '\t') fputc('\t', stderr);
                        else fputc(' ', stderr);
                    }
                    if (is_tty) {
                        fprintf(stderr, "\033[1;31m^\033[0m\n");
                    } else {
                        fprintf(stderr, "^\n");
                    }
                    break;
                }
                cur_line++;
            }
            fclose(f);
        }
    }
}

/* ─── Arena ────────────────────────────────────────────────────────────── */
static ArenaBlock *block_new(size_t cap) {
    ArenaBlock *b = xmalloc(sizeof(ArenaBlock));
    b->base     = xmalloc(cap);
    b->offset   = 0;
    b->capacity = cap;
    b->next     = NULL;
    return b;
}

Arena *arena_create(void) {
    Arena *a = xmalloc(sizeof(Arena));
    a->head = a->current = block_new(ARENA_BLOCK_SIZE);
    a->total_allocated = 0;
    return a;
}

void *arena_alloc(Arena *a, size_t size) {
    size = (size + 7) & ~(size_t)7;                 /* 8-byte alignment */
    ArenaBlock *b = a->current;
    if (b->offset + size > b->capacity) {
        size_t cap = size > ARENA_BLOCK_SIZE ? size : ARENA_BLOCK_SIZE;
        ArenaBlock *nb = block_new(cap);
        b->next    = nb;
        a->current = nb;
        b          = nb;
    }
    void *p = b->base + b->offset;
    b->offset += size;
    a->total_allocated += size;
    return p;
}

char *arena_strndup(Arena *a, const char *s, size_t len) {
    char *d = arena_alloc(a, len + 1);
    memcpy(d, s, len);
    d[len] = '\0';
    return d;
}

char *arena_strdup(Arena *a, const char *s) {
    return arena_strndup(a, s, strlen(s));
}

void arena_destroy(Arena *a) {
    ArenaBlock *b = a->head;
    while (b) {
        ArenaBlock *next = b->next;
        free(b->base);
        free(b);
        b = next;
    }
    free(a);
}

/* ─── StrBuf ───────────────────────────────────────────────────────────── */
void strbuf_init(StrBuf *sb) {
    sb->data = xmalloc(STRBUF_INIT_CAP);
    sb->data[0] = '\0';
    sb->len = 0;
    sb->cap = STRBUF_INIT_CAP;
}

static void strbuf_reserve(StrBuf *sb, size_t extra) {
    if (sb->len + extra + 1 <= sb->cap) return;
    while (sb->cap < sb->len + extra + 1) sb->cap *= 2;
    sb->data = realloc(sb->data, sb->cap);
    if (!sb->data) { fprintf(stderr, "tamizhi: out of memory\n"); exit(1); }
}

void strbuf_append_n(StrBuf *sb, const char *s, size_t n) {
    strbuf_reserve(sb, n);
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

void strbuf_append(StrBuf *sb, const char *s) {
    strbuf_append_n(sb, s, strlen(s));
}

void strbuf_appendf(StrBuf *sb, const char *fmt, ...) {
    char tmp[512];                       /* fast path: most pieces are short */
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n < sizeof tmp) {
        strbuf_append_n(sb, tmp, (size_t)n);
        return;
    }
    strbuf_reserve(sb, (size_t)n);       /* slow path: format straight in */
    va_start(ap, fmt);
    vsnprintf(sb->data + sb->len, (size_t)n + 1, fmt, ap);
    va_end(ap);
    sb->len += (size_t)n;
}

void strbuf_free(StrBuf *sb) {
    free(sb->data);
    sb->data = NULL;
    sb->len = sb->cap = 0;
}

/* ─── File reading ─────────────────────────────────────────────────────── */
char *read_source_file(const char *path, Arena *a) {
    struct stat st;
    if (stat(path, &st) != 0) return NULL;
    if (!S_ISREG(st.st_mode)) { errno = S_ISDIR(st.st_mode) ? EISDIR : EINVAL; return NULL; }
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    size_t size = (size_t)st.st_size;
    char *buf = a ? arena_alloc(a, size + 1) : malloc(size + 1);
    if (!buf) { fclose(f); errno = ENOMEM; return NULL; }
    if (fread(buf, 1, size, f) != size) {
        int e = errno ? errno : EIO;
        fclose(f);
        if (!a) free(buf);
        errno = e;
        return NULL;
    }
    fclose(f);
    buf[size] = '\0';
    return buf;
}
