#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>

static int _pg_argc;
static char** _pg_argv;

static void pg_print_backtrace() {
    void* buffer[64];
    int nframes = backtrace(buffer, 64);
    char** symbols = backtrace_symbols(buffer, nframes);
    fprintf(stderr, "Stack trace:\n");
    for (int i = 0; i < nframes; i++) {
        fprintf(stderr, "  %s\n", symbols[i]);
    }
    free(symbols);
}

static void pg_signal_handler(int sig) {
    fprintf(stderr, "\nFatal signal %d received\n", sig);
    pg_print_backtrace();
    _exit(1);
}

static void pg_install_signal_handlers() {
    signal(SIGSEGV, pg_signal_handler);
    signal(SIGABRT, pg_signal_handler);
    signal(SIGFPE, pg_signal_handler);
}

static void pg_panic(char* msg) {
    fprintf(stderr, "panic: %s\n", msg);
    pg_print_backtrace();
    _exit(1);
}

static int pg_system(char* cmd) {
    return system(cmd);
}

static void pg_println_int(int x) { printf("%d\n", x); }
static void pg_println_str(char* x) { printf("%s\n", x); }
static void pg_print_int(int x) { printf("%d", x); }
static void pg_print_str(char* x) { printf("%s", x); }

static int pg_str_len(char* s) { return (int)strlen(s); }
static int pg_str_char_at(char* s, int i) { return (int)(unsigned char)s[i]; }
static char* pg_char_to_str(int c) {
    char* s = (char*)malloc(2);
    s[0] = (char)c; s[1] = 0;
    return s;
}
static char* pg_str_concat(char* a, char* b) {
    int la = strlen(a), lb = strlen(b);
    char* r = (char*)malloc(la + lb + 1);
    memcpy(r, a, la); memcpy(r + la, b, lb); r[la+lb] = 0;
    return r;
}
static int pg_str_eq(char* a, char* b) { return strcmp(a, b) == 0 ? 1 : 0; }
static char* pg_str_substr(char* s, int start, int len) {
    char* r = (char*)malloc(len + 1);
    memcpy(r, s + start, len); r[len] = 0;
    return r;
}
static int pg_str_index_of(char* s, char* sub) {
    char* p = strstr(s, sub);
    return p ? (int)(p - s) : -1;
}
static int pg_str_starts_with(char* s, char* pfx) {
    return strncmp(s, pfx, strlen(pfx)) == 0 ? 1 : 0;
}
static char* pg_str_replace(char* s, char* old_s, char* rep) {
    char* p = strstr(s, old_s); if (!p) return s;
    int before = (int)(p - s), olen = strlen(old_s), rlen = strlen(rep), slen = strlen(s);
    char* r = (char*)malloc(slen - olen + rlen + 1);
    memcpy(r, s, before); memcpy(r+before, rep, rlen); strcpy(r+before+rlen, p+olen);
    return r;
}
static int pg_str_to_int(char* s) { return atoi(s); }
static char* pg_int_to_str(int n) {
    char* s = (char*)malloc(20);
    snprintf(s, 20, "%d", n);
    return s;
}

static char* pg_read_file(char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open: %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(sz + 1);
    fread(buf, 1, sz, f); buf[sz] = 0; fclose(f);
    return buf;
}
static void pg_write_file(char* path, char* content) {
    FILE* f = fopen(path, "w");
    if (!f) { fprintf(stderr, "Cannot write: %s\n", path); exit(1); }
    fputs(content, f); fclose(f);
}

static char* pg_make_array(int n) { return (char*)calloc(n, sizeof(int)); }
static int pg_array_get(char* arr, int i) { return ((int*)arr)[i]; }
static void pg_array_set(char* arr, int i, int v) { ((int*)arr)[i] = v; }
static char* pg_make_str_array(int n) { return (char*)calloc(n, sizeof(char*)); }
static char* pg_str_array_get(char* arr, int i) { return ((char**)arr)[i]; }
static void pg_str_array_set(char* arr, int i, char* v) { ((char**)arr)[i] = v; }

static char* pg_args(int i) { return _pg_argv[i]; }
static int pg_args_count() { return _pg_argc; }

// Directory listing: find .pgl files in a directory
#include <dirent.h>
static int pg_find_pgl_files(char* dir, char* out_arr) {
    DIR* d = opendir(dir);
    if (!d) return 0;
    struct dirent* ent;
    int count = 0;
    // First pass: collect and sort
    char* names[4096];
    while ((ent = readdir(d)) != NULL) {
        int len = strlen(ent->d_name);
        if (len > 4 && strcmp(ent->d_name + len - 4, ".pgl") == 0) {
            // Build full path: dir + "/" + name
            int dlen = strlen(dir);
            int need_slash = (dlen > 0 && dir[dlen-1] != '/') ? 1 : 0;
            char* path = (char*)malloc(dlen + need_slash + len + 1);
            memcpy(path, dir, dlen);
            if (need_slash) path[dlen] = '/';
            memcpy(path + dlen + need_slash, ent->d_name, len + 1);
            names[count++] = path;
        }
    }
    closedir(d);
    // Simple sort
    for (int i = 0; i < count - 1; i++)
        for (int j = i + 1; j < count; j++)
            if (strcmp(names[i], names[j]) > 0) {
                char* tmp = names[i]; names[i] = names[j]; names[j] = tmp;
            }
    for (int i = 0; i < count; i++)
        ((char**)out_arr)[i] = names[i];
    return count;
}

// Check if path is a directory
static int pg_is_directory(char* path) {
    DIR* d = opendir(path);
    if (d) { closedir(d); return 1; }
    return 0;
}

static int pg_str_ends_with(char* s, char* suffix) {
    int slen = strlen(s), plen = strlen(suffix);
    if (plen > slen) return 0;
    return strcmp(s + slen - plen, suffix) == 0 ? 1 : 0;
}

// ===== Pipeline runtime =====
typedef struct {
    int worker_id;
    char* cache_buf;
    int cache_len;
    int cache_cap;
    char** out_buf;
    int out_count;
    int out_cap;
    int elem_size;
} PipelineState;

static char* pg_pipeline_create(int elem_size) {
    PipelineState* s = (PipelineState*)calloc(1, sizeof(PipelineState));
    s->worker_id = 0;
    s->cache_cap = 256;
    s->cache_buf = (char*)calloc(s->cache_cap, 1);
    s->cache_len = 0;
    s->out_cap = 64;
    s->out_buf = (char**)calloc(s->out_cap, sizeof(char*));
    s->out_count = 0;
    s->elem_size = elem_size;
    return (char*)s;
}

static void pg_pipeline_destroy(char* state) {
    PipelineState* s = (PipelineState*)state;
    if (s->cache_buf) free(s->cache_buf);
    if (s->out_buf) free(s->out_buf);
    free(s);
}

static void pg_pipeline_cache_append(char* state, int ch) {
    PipelineState* s = (PipelineState*)state;
    if (s->cache_len + 1 >= s->cache_cap) {
        s->cache_cap *= 2;
        s->cache_buf = (char*)realloc(s->cache_buf, s->cache_cap);
    }
    s->cache_buf[s->cache_len++] = (char)ch;
}

static char* pg_pipeline_cache_str(char* state) {
    PipelineState* s = (PipelineState*)state;
    char* result = (char*)malloc(s->cache_len + 1);
    memcpy(result, s->cache_buf, s->cache_len);
    result[s->cache_len] = '\0';
    s->cache_len = 0;
    return result;
}

static void pg_pipeline_cache_reset(char* state) {
    PipelineState* s = (PipelineState*)state;
    s->cache_len = 0;
}

static void pg_pipeline_emit(char* state, char* elem) {
    PipelineState* s = (PipelineState*)state;
    if (s->out_count >= s->out_cap) {
        s->out_cap *= 2;
        s->out_buf = (char**)realloc(s->out_buf, s->out_cap * sizeof(char*));
    }
    s->out_buf[s->out_count++] = elem;
}

static int pg_pipeline_output_count(char* state) {
    return ((PipelineState*)state)->out_count;
}

static char* pg_pipeline_output_get(char* state, int index) {
    return ((PipelineState*)state)->out_buf[index];
}

static void pg_pipeline_set_worker(char* state, int wid) {
    ((PipelineState*)state)->worker_id = wid;
}

static int pg_pipeline_get_worker(char* state) {
    return ((PipelineState*)state)->worker_id;
}

