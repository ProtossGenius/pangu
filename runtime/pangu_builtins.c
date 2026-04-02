#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>

// --- Backtrace with PGL source location (Linux) ---
#ifdef __linux__
#include <execinfo.h>

void pg_print_backtrace(void) {
    void *buffer[64];
    int nframes = backtrace(buffer, 64);
    char **symbols = backtrace_symbols(buffer, nframes);

    // Read /proc/self/exe for addr2line
    char exe_path[1024];
    ssize_t exe_len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    int has_exe = (exe_len > 0);
    if (has_exe) exe_path[exe_len] = '\0';

    // Read base address from /proc/self/maps for PIE binaries
    void *base_addr = NULL;
    if (has_exe) {
        FILE *maps = fopen("/proc/self/maps", "r");
        if (maps) {
            char line[512];
            while (fgets(line, sizeof(line), maps)) {
                if (strstr(line, "r-xp") || strstr(line, "r--p")) {
                    if (strstr(line, exe_path)) {
                        unsigned long start;
                        if (sscanf(line, "%lx-", &start) == 1) {
                            base_addr = (void *)start;
                            break;
                        }
                    }
                }
            }
            fclose(maps);
        }
    }

    fprintf(stderr, "Stack trace:\n");
    for (int i = 0; i < nframes; i++) {
        if (has_exe) {
            // Subtract base address for PIE binaries
            void *offset = (void *)((char *)buffer[i] - (char *)base_addr);
            char resolved[512];
            char cmd[2048];
            snprintf(cmd, sizeof(cmd), "addr2line -e '%s' -f -C -p %p 2>/dev/null",
                     exe_path, offset);
            FILE *fp = popen(cmd, "r");
            if (fp) {
                if (fgets(resolved, sizeof(resolved), fp)) {
                    size_t len = strlen(resolved);
                    if (len > 0 && resolved[len - 1] == '\n') resolved[len - 1] = '\0';
                    if (strstr(resolved, "??") == NULL) {
                        fprintf(stderr, "  [%d] %s\n", i, resolved);
                        pclose(fp);
                        continue;
                    }
                }
                pclose(fp);
            }
        }
        // Fallback to backtrace_symbols
        fprintf(stderr, "  [%d] %s\n", i, symbols[i]);
    }
    free(symbols);
}
#else
void pg_print_backtrace(void) {
    fprintf(stderr, "(no backtrace available on this platform)\n");
}
#endif

// --- Signal handler ---
static void pg_signal_handler(int sig) {
    fprintf(stderr, "\nFatal signal %d received\n", sig);
    pg_print_backtrace();
    _exit(1);
}

void pg_install_signal_handlers(void) {
    signal(SIGSEGV, pg_signal_handler);
    signal(SIGABRT, pg_signal_handler);
    signal(SIGFPE, pg_signal_handler);
}

// --- Panic ---
void pg_panic(const char *msg) {
    fprintf(stderr, "panic: %s\n", msg);
    pg_print_backtrace();
    _exit(1);
}

// --- System ---
int pg_system(const char *cmd) {
    return system(cmd);
}

// --- str_ends_with ---
int pg_str_ends_with(const char *s, const char *suffix) {
    size_t slen = strlen(s), plen = strlen(suffix);
    if (plen > slen) return 0;
    return strcmp(s + slen - plen, suffix) == 0 ? 1 : 0;
}

// --- is_directory ---
int pg_is_directory(const char *path) {
    DIR *d = opendir(path);
    if (d) { closedir(d); return 1; }
    return 0;
}

// --- find_pgl_files: fills out_arr (char**) with sorted .pgl paths, returns count ---
int pg_find_pgl_files(const char *dir, char **out_arr) {
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *ent;
    int count = 0;
    char *names[4096];
    while ((ent = readdir(d)) != NULL) {
        int len = strlen(ent->d_name);
        if (len > 4 && strcmp(ent->d_name + len - 4, ".pgl") == 0) {
            size_t dlen = strlen(dir);
            int need_slash = (dlen > 0 && dir[dlen - 1] != '/') ? 1 : 0;
            char *path = (char *)malloc(dlen + need_slash + len + 1);
            memcpy(path, dir, dlen);
            if (need_slash) path[dlen] = '/';
            memcpy(path + dlen + need_slash, ent->d_name, len + 1);
            names[count++] = path;
        }
    }
    closedir(d);
    for (int i = 0; i < count - 1; i++)
        for (int j = i + 1; j < count; j++)
            if (strcmp(names[i], names[j]) > 0) {
                char *tmp = names[i];
                names[i] = names[j];
                names[j] = tmp;
            }
    for (int i = 0; i < count; i++)
        out_arr[i] = names[i];
    return count;
}

// === Pipeline Runtime ===
// Pipeline state: opaque struct managed by these functions.
// Layout: { int worker_id, char* cache_buf, int cache_len, int cache_cap,
//           void** out_buf, int out_count, int out_cap, int elem_size }
// All fields stored as a flat int64_t array for simplicity.

typedef struct {
    int worker_id;
    char *cache_buf;
    int cache_len;
    int cache_cap;
    char **out_buf;     // array of output pointers (each element is elem_size bytes)
    int out_count;
    int out_cap;
    int elem_size;
} PipelineState;

void *pg_pipeline_create(int elem_size) {
    PipelineState *s = (PipelineState *)calloc(1, sizeof(PipelineState));
    s->cache_cap = 256;
    s->cache_buf = (char *)calloc(s->cache_cap, 1);
    s->out_cap = 64;
    s->elem_size = elem_size > 0 ? elem_size : sizeof(void *);
    s->out_buf = (char **)calloc(s->out_cap, sizeof(char *));
    return s;
}

void pg_pipeline_destroy(void *state) {
    PipelineState *s = (PipelineState *)state;
    if (!s) return;
    free(s->cache_buf);
    for (int i = 0; i < s->out_count; i++) free(s->out_buf[i]);
    free(s->out_buf);
    free(s);
}

void pg_pipeline_cache_append(void *state, int ch) {
    PipelineState *s = (PipelineState *)state;
    if (s->cache_len >= s->cache_cap - 1) {
        s->cache_cap *= 2;
        s->cache_buf = (char *)realloc(s->cache_buf, s->cache_cap);
    }
    s->cache_buf[s->cache_len++] = (char)ch;
    s->cache_buf[s->cache_len] = '\0';
}

const char *pg_pipeline_cache_str(void *state) {
    PipelineState *s = (PipelineState *)state;
    char *result = (char *)malloc(s->cache_len + 1);
    memcpy(result, s->cache_buf, s->cache_len + 1);
    s->cache_len = 0;
    s->cache_buf[0] = '\0';
    return result;
}

void pg_pipeline_cache_reset(void *state) {
    PipelineState *s = (PipelineState *)state;
    s->cache_len = 0;
    s->cache_buf[0] = '\0';
}

void pg_pipeline_emit(void *state, void *elem) {
    PipelineState *s = (PipelineState *)state;
    if (s->out_count >= s->out_cap) {
        s->out_cap *= 2;
        s->out_buf = (char **)realloc(s->out_buf, s->out_cap * sizeof(char *));
    }
    char *copy = (char *)malloc(s->elem_size);
    memcpy(copy, elem, s->elem_size);
    s->out_buf[s->out_count++] = copy;
}

int pg_pipeline_output_count(void *state) {
    PipelineState *s = (PipelineState *)state;
    return s->out_count;
}

void *pg_pipeline_output_get(void *state, int index) {
    PipelineState *s = (PipelineState *)state;
    if (index < 0 || index >= s->out_count) return NULL;
    return s->out_buf[index];
}

void pg_pipeline_set_worker(void *state, int worker_id) {
    PipelineState *s = (PipelineState *)state;
    s->worker_id = worker_id;
}

int pg_pipeline_get_worker(void *state) {
    PipelineState *s = (PipelineState *)state;
    return s->worker_id;
}
