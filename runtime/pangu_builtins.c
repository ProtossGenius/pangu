#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>

// Forward declarations for cross-references
void *make_dyn_str_array(void);
void  dyn_str_array_push(void *ap, const char *val);

// --- Backtrace with PGL source location (Linux) ---

// JIT function address → source location registry
typedef struct {
    void       *addr;
    const char *name;
    const char *file;
    int         line;
} PgJitFuncInfo;

static PgJitFuncInfo *g_jit_funcs = NULL;
static int g_jit_func_count = 0;
static int g_jit_func_cap   = 0;

void pg_register_jit_function(void *addr, const char *name,
                               const char *file, int line) {
    if (g_jit_func_count >= g_jit_func_cap) {
        g_jit_func_cap = g_jit_func_cap ? g_jit_func_cap * 2 : 64;
        g_jit_funcs = (PgJitFuncInfo *)realloc(
            g_jit_funcs, g_jit_func_cap * sizeof(PgJitFuncInfo));
    }
    PgJitFuncInfo *info = &g_jit_funcs[g_jit_func_count++];
    info->addr = addr;
    info->name = name;
    info->file = file;
    info->line = line;
}

// Find the JIT function whose address is closest to (but not after) the given address
static const PgJitFuncInfo *pg_lookup_jit_function(void *addr) {
    const PgJitFuncInfo *best = NULL;
    for (int i = 0; i < g_jit_func_count; i++) {
        if (g_jit_funcs[i].addr <= addr) {
            if (!best || g_jit_funcs[i].addr > best->addr) {
                best = &g_jit_funcs[i];
            }
        }
    }
    return best;
}

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
        // First check JIT function registry
        const PgJitFuncInfo *jit_info = pg_lookup_jit_function(buffer[i]);
        if (jit_info) {
            fprintf(stderr, "  [%d] %s at %s:%d\n", i,
                    jit_info->name, jit_info->file, jit_info->line);
            continue;
        }
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

// --- Reflection runtime ---

typedef struct {
    const char *name;
    int field_count;
    const char **field_names;
    const char **field_types;
    int ann_count;
    const char **ann_keys;
    const char **ann_values;
    const int *ann_field_indices;
} PanguTypeMeta;

static int g_type_count = 0;
static PanguTypeMeta *g_type_registry = NULL;

void __pangu_register_types(int count, void *registry) {
    g_type_count = count;
    g_type_registry = (PanguTypeMeta *)registry;
}

static PanguTypeMeta *find_type_meta(const char *name) {
    for (int i = 0; i < g_type_count; i++) {
        if (strcmp(g_type_registry[i].name, name) == 0)
            return &g_type_registry[i];
    }
    return NULL;
}

int reflect_type_count(void) {
    return g_type_count;
}

const char *reflect_type_name(int index) {
    if (index < 0 || index >= g_type_count) return "";
    return g_type_registry[index].name;
}

int reflect_field_count(const char *type_name) {
    PanguTypeMeta *m = find_type_meta(type_name);
    return m ? m->field_count : 0;
}

const char *reflect_field_name(const char *type_name, int index) {
    PanguTypeMeta *m = find_type_meta(type_name);
    if (!m || index < 0 || index >= m->field_count) return "";
    return m->field_names[index];
}

const char *reflect_field_type(const char *type_name, int index) {
    PanguTypeMeta *m = find_type_meta(type_name);
    if (!m || index < 0 || index >= m->field_count) return "";
    return m->field_types[index];
}

int reflect_annotation_count(const char *type_name) {
    PanguTypeMeta *m = find_type_meta(type_name);
    return m ? m->ann_count : 0;
}

const char *reflect_annotation_key(const char *type_name, int index) {
    PanguTypeMeta *m = find_type_meta(type_name);
    if (!m || index < 0 || index >= m->ann_count) return "";
    return m->ann_keys[index];
}

const char *reflect_annotation_value(const char *type_name, int index) {
    PanguTypeMeta *m = find_type_meta(type_name);
    if (!m || index < 0 || index >= m->ann_count) return "";
    return m->ann_values[index];
}

int reflect_annotation_field_index(const char *type_name, int index) {
    PanguTypeMeta *m = find_type_meta(type_name);
    if (!m || index < 0 || index >= m->ann_count) return -1;
    return m->ann_field_indices[index];
}

// ── HashMap: string → string (open-addressing with linear probing) ──

typedef struct {
    char *key;
    char *value;
    int   occupied;
} HMEntry;

typedef struct {
    HMEntry *buckets;
    int      capacity;
    int      size;
} HashMap;

static unsigned int hm_hash(const char *key, int cap) {
    unsigned int h = 5381;
    while (*key) { h = h * 33 + (unsigned char)*key++; }
    return h % (unsigned int)cap;
}

static void hm_grow(HashMap *m) {
    int old_cap = m->capacity;
    HMEntry *old = m->buckets;
    m->capacity = old_cap * 2;
    m->buckets = (HMEntry *)calloc(m->capacity, sizeof(HMEntry));
    m->size = 0;
    for (int i = 0; i < old_cap; i++) {
        if (old[i].occupied) {
            unsigned int idx = hm_hash(old[i].key, m->capacity);
            while (m->buckets[idx].occupied)
                idx = (idx + 1) % m->capacity;
            m->buckets[idx].key = old[i].key;
            m->buckets[idx].value = old[i].value;
            m->buckets[idx].occupied = 1;
            m->size++;
        }
    }
    free(old);
}

void *make_map() {
    HashMap *m = (HashMap *)malloc(sizeof(HashMap));
    m->capacity = 16;
    m->size = 0;
    m->buckets = (HMEntry *)calloc(m->capacity, sizeof(HMEntry));
    return m;
}

void map_set(void *mp, const char *key, const char *value) {
    HashMap *m = (HashMap *)mp;
    if (m->size * 2 >= m->capacity) hm_grow(m);
    unsigned int idx = hm_hash(key, m->capacity);
    while (m->buckets[idx].occupied) {
        if (strcmp(m->buckets[idx].key, key) == 0) {
            free(m->buckets[idx].value);
            m->buckets[idx].value = strdup(value);
            return;
        }
        idx = (idx + 1) % m->capacity;
    }
    m->buckets[idx].key = strdup(key);
    m->buckets[idx].value = strdup(value);
    m->buckets[idx].occupied = 1;
    m->size++;
}

const char *map_get(void *mp, const char *key) {
    HashMap *m = (HashMap *)mp;
    unsigned int idx = hm_hash(key, m->capacity);
    while (m->buckets[idx].occupied) {
        if (strcmp(m->buckets[idx].key, key) == 0)
            return m->buckets[idx].value;
        idx = (idx + 1) % m->capacity;
    }
    return "";
}

int map_has(void *mp, const char *key) {
    HashMap *m = (HashMap *)mp;
    unsigned int idx = hm_hash(key, m->capacity);
    while (m->buckets[idx].occupied) {
        if (strcmp(m->buckets[idx].key, key) == 0) return 1;
        idx = (idx + 1) % m->capacity;
    }
    return 0;
}

int map_size(void *mp) {
    return ((HashMap *)mp)->size;
}

void map_delete(void *mp, const char *key) {
    HashMap *m = (HashMap *)mp;
    unsigned int idx = hm_hash(key, m->capacity);
    while (m->buckets[idx].occupied) {
        if (strcmp(m->buckets[idx].key, key) == 0) {
            free(m->buckets[idx].key);
            free(m->buckets[idx].value);
            m->buckets[idx].key = NULL;
            m->buckets[idx].value = NULL;
            m->buckets[idx].occupied = 0;
            m->size--;
            // Re-insert subsequent entries to maintain probe chain
            unsigned int next = (idx + 1) % m->capacity;
            while (m->buckets[next].occupied) {
                char *rk = m->buckets[next].key;
                char *rv = m->buckets[next].value;
                m->buckets[next].occupied = 0;
                m->size--;
                map_set(mp, rk, rv);
                free(rk);
                free(rv);
                next = (next + 1) % m->capacity;
            }
            return;
        }
        idx = (idx + 1) % m->capacity;
    }
}

void *map_keys(void *mp) {
    HashMap *m = (HashMap *)mp;
    void *arr = make_dyn_str_array();
    for (int i = 0; i < m->capacity; i++) {
        if (m->buckets[i].occupied) {
            dyn_str_array_push(arr, m->buckets[i].key);
        }
    }
    return arr;
}

// ── Integer Map: string → int ──

typedef struct {
    char *key;
    int   value;
    int   occupied;
} IMEntry;

typedef struct {
    IMEntry *buckets;
    int      capacity;
    int      size;
} IntMap;

static void im_grow(IntMap *m) {
    int old_cap = m->capacity;
    IMEntry *old = m->buckets;
    m->capacity = old_cap * 2;
    m->buckets = (IMEntry *)calloc(m->capacity, sizeof(IMEntry));
    m->size = 0;
    for (int i = 0; i < old_cap; i++) {
        if (old[i].occupied) {
            unsigned int idx = hm_hash(old[i].key, m->capacity);
            while (m->buckets[idx].occupied)
                idx = (idx + 1) % m->capacity;
            m->buckets[idx].key = old[i].key;
            m->buckets[idx].value = old[i].value;
            m->buckets[idx].occupied = 1;
            m->size++;
        }
    }
    free(old);
}

void *make_int_map() {
    IntMap *m = (IntMap *)malloc(sizeof(IntMap));
    m->capacity = 16;
    m->size = 0;
    m->buckets = (IMEntry *)calloc(m->capacity, sizeof(IMEntry));
    return m;
}

void int_map_set(void *mp, const char *key, int value) {
    IntMap *m = (IntMap *)mp;
    if (m->size * 2 >= m->capacity) im_grow(m);
    unsigned int idx = hm_hash(key, m->capacity);
    while (m->buckets[idx].occupied) {
        if (strcmp(m->buckets[idx].key, key) == 0) {
            m->buckets[idx].value = value;
            return;
        }
        idx = (idx + 1) % m->capacity;
    }
    m->buckets[idx].key = strdup(key);
    m->buckets[idx].value = value;
    m->buckets[idx].occupied = 1;
    m->size++;
}

int int_map_get(void *mp, const char *key) {
    IntMap *m = (IntMap *)mp;
    unsigned int idx = hm_hash(key, m->capacity);
    while (m->buckets[idx].occupied) {
        if (strcmp(m->buckets[idx].key, key) == 0)
            return m->buckets[idx].value;
        idx = (idx + 1) % m->capacity;
    }
    return 0;
}

int int_map_has(void *mp, const char *key) {
    IntMap *m = (IntMap *)mp;
    unsigned int idx = hm_hash(key, m->capacity);
    while (m->buckets[idx].occupied) {
        if (strcmp(m->buckets[idx].key, key) == 0) return 1;
        idx = (idx + 1) % m->capacity;
    }
    return 0;
}

int int_map_size(void *mp) {
    return ((IntMap *)mp)->size;
}

void *int_map_keys(void *mp) {
    IntMap *m = (IntMap *)mp;
    void *arr = make_dyn_str_array();
    for (int i = 0; i < m->capacity; i++) {
        if (m->buckets[i].occupied) {
            dyn_str_array_push(arr, m->buckets[i].key);
        }
    }
    return arr;
}

// ── Dynamic Array (resizable int array) ──

typedef struct {
    int *data;
    int  size;
    int  capacity;
} DynArray;

void *make_dyn_array() {
    DynArray *a = (DynArray *)malloc(sizeof(DynArray));
    a->capacity = 16;
    a->size = 0;
    a->data = (int *)malloc(a->capacity * sizeof(int));
    return a;
}

void dyn_array_push(void *ap, int val) {
    DynArray *a = (DynArray *)ap;
    if (a->size >= a->capacity) {
        a->capacity *= 2;
        a->data = (int *)realloc(a->data, a->capacity * sizeof(int));
    }
    a->data[a->size++] = val;
}

int dyn_array_get(void *ap, int index) {
    DynArray *a = (DynArray *)ap;
    if (index < 0 || index >= a->size) return 0;
    return a->data[index];
}

void dyn_array_set(void *ap, int index, int val) {
    DynArray *a = (DynArray *)ap;
    if (index >= 0 && index < a->size) a->data[index] = val;
}

int dyn_array_size(void *ap) {
    return ((DynArray *)ap)->size;
}

int dyn_array_pop(void *ap) {
    DynArray *a = (DynArray *)ap;
    if (a->size == 0) return 0;
    return a->data[--a->size];
}

// ── Dynamic String Array ──

typedef struct {
    char **data;
    int    size;
    int    capacity;
} DynStrArray;

void *make_dyn_str_array() {
    DynStrArray *a = (DynStrArray *)malloc(sizeof(DynStrArray));
    a->capacity = 16;
    a->size = 0;
    a->data = (char **)malloc(a->capacity * sizeof(char *));
    return a;
}

void dyn_str_array_push(void *ap, const char *val) {
    DynStrArray *a = (DynStrArray *)ap;
    if (a->size >= a->capacity) {
        a->capacity *= 2;
        a->data = (char **)realloc(a->data, a->capacity * sizeof(char *));
    }
    a->data[a->size++] = strdup(val);
}

const char *dyn_str_array_get(void *ap, int index) {
    DynStrArray *a = (DynStrArray *)ap;
    if (index < 0 || index >= a->size) return "";
    return a->data[index];
}

void dyn_str_array_set(void *ap, int index, const char *val) {
    DynStrArray *a = (DynStrArray *)ap;
    if (index >= 0 && index < a->size) {
        free(a->data[index]);
        a->data[index] = strdup(val);
    }
}

int dyn_str_array_size(void *ap) {
    return ((DynStrArray *)ap)->size;
}

// str_join(arr, sep) → join all strings in arr with separator sep
const char *str_join(void *ap, const char *sep) {
    DynStrArray *a = (DynStrArray *)ap;
    if (a->size == 0) {
        char *empty = (char *)malloc(1);
        empty[0] = '\0';
        return empty;
    }
    int sep_len = strlen(sep);
    int total = 0;
    for (int i = 0; i < a->size; i++) {
        total += strlen(a->data[i]);
        if (i > 0) total += sep_len;
    }
    char *result = (char *)malloc(total + 1);
    int pos = 0;
    for (int i = 0; i < a->size; i++) {
        if (i > 0) {
            memcpy(result + pos, sep, sep_len);
            pos += sep_len;
        }
        int slen = strlen(a->data[i]);
        memcpy(result + pos, a->data[i], slen);
        pos += slen;
    }
    result[pos] = '\0';
    return result;
}

// ── String Builder ──

typedef struct {
    char *buf;
    int   len;
    int   cap;
} StringBuilder;

void *make_str_builder() {
    StringBuilder *sb = (StringBuilder *)malloc(sizeof(StringBuilder));
    sb->cap = 256;
    sb->len = 0;
    sb->buf = (char *)malloc(sb->cap);
    sb->buf[0] = '\0';
    return sb;
}

void sb_append(void *sbp, const char *s) {
    StringBuilder *sb = (StringBuilder *)sbp;
    int slen = strlen(s);
    while (sb->len + slen + 1 > sb->cap) {
        sb->cap *= 2;
        sb->buf = (char *)realloc(sb->buf, sb->cap);
    }
    memcpy(sb->buf + sb->len, s, slen);
    sb->len += slen;
    sb->buf[sb->len] = '\0';
}

void sb_append_int(void *sbp, int n) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%d", n);
    sb_append(sbp, tmp);
}

void sb_append_char(void *sbp, int ch) {
    StringBuilder *sb = (StringBuilder *)sbp;
    if (sb->len + 2 > sb->cap) {
        sb->cap *= 2;
        sb->buf = (char *)realloc(sb->buf, sb->cap);
    }
    sb->buf[sb->len++] = (char)ch;
    sb->buf[sb->len] = '\0';
}

const char *sb_build(void *sbp) {
    StringBuilder *sb = (StringBuilder *)sbp;
    char *result = strdup(sb->buf);
    return result;
}

void sb_reset(void *sbp) {
    StringBuilder *sb = (StringBuilder *)sbp;
    sb->len = 0;
    sb->buf[0] = '\0';
}

int sb_len(void *sbp) {
    return ((StringBuilder *)sbp)->len;
}

/* ===== String utility functions ===== */

// str_contains(haystack, needle) → 1 if found, 0 otherwise
int str_contains(const char *haystack, const char *needle) {
    return strstr(haystack, needle) != NULL ? 1 : 0;
}

// str_ends_with(s, suffix) → 1 if s ends with suffix
int str_ends_with(const char *s, const char *suffix) {
    size_t slen = strlen(s);
    size_t sufflen = strlen(suffix);
    if (sufflen > slen) return 0;
    return strcmp(s + slen - sufflen, suffix) == 0 ? 1 : 0;
}

// str_trim(s) → new string with leading/trailing whitespace removed
char *str_trim(const char *s) {
    while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                       s[len-1] == '\n' || s[len-1] == '\r')) len--;
    char *result = (char *)malloc(len + 1);
    memcpy(result, s, len);
    result[len] = '\0';
    return result;
}

// str_to_upper(s) → new uppercased string
char *str_to_upper(const char *s) {
    size_t len = strlen(s);
    char *result = (char *)malloc(len + 1);
    for (size_t i = 0; i < len; i++) {
        result[i] = (s[i] >= 'a' && s[i] <= 'z') ? s[i] - 32 : s[i];
    }
    result[len] = '\0';
    return result;
}

// str_to_lower(s) → new lowercased string
char *str_to_lower(const char *s) {
    size_t len = strlen(s);
    char *result = (char *)malloc(len + 1);
    for (size_t i = 0; i < len; i++) {
        result[i] = (s[i] >= 'A' && s[i] <= 'Z') ? s[i] + 32 : s[i];
    }
    result[len] = '\0';
    return result;
}

// str_split(s, delim) → DynStrArray of substrings
void *str_split(const char *s, const char *delim) {
    void *arr = make_dyn_str_array();
    size_t dlen = strlen(delim);
    if (dlen == 0) {
        dyn_str_array_push(arr, s);
        return arr;
    }
    const char *start = s;
    const char *found;
    while ((found = strstr(start, delim)) != NULL) {
        size_t part_len = found - start;
        char *part = (char *)malloc(part_len + 1);
        memcpy(part, start, part_len);
        part[part_len] = '\0';
        dyn_str_array_push(arr, part);
        start = found + dlen;
    }
    dyn_str_array_push(arr, start);
    return arr;
}

// str_repeat(s, n) → new string repeating s n times
char *str_repeat(const char *s, int n) {
    if (n <= 0) {
        char *r = (char *)malloc(1);
        r[0] = '\0';
        return r;
    }
    size_t slen = strlen(s);
    size_t total = slen * n;
    char *result = (char *)malloc(total + 1);
    for (int i = 0; i < n; i++) {
        memcpy(result + i * slen, s, slen);
    }
    result[total] = '\0';
    return result;
}

// str_count(haystack, needle) → number of non-overlapping occurrences
int str_count(const char *haystack, const char *needle) {
    int count = 0;
    size_t nlen = strlen(needle);
    if (nlen == 0) return 0;
    const char *p = haystack;
    while ((p = strstr(p, needle)) != NULL) {
        count++;
        p += nlen;
    }
    return count;
}

// str_replace_all(s, old, new) → new string with ALL occurrences replaced
char *str_replace_all(const char *s, const char *old_s, const char *new_s) {
    size_t olen = strlen(old_s);
    if (olen == 0) {
        char *r = (char *)malloc(strlen(s) + 1);
        strcpy(r, s);
        return r;
    }
    size_t nlen = strlen(new_s);
    int cnt = str_count(s, old_s);
    size_t result_len = strlen(s) + cnt * (nlen - olen);
    char *result = (char *)malloc(result_len + 1);
    char *wp = result;
    const char *p = s;
    const char *found;
    while ((found = strstr(p, old_s)) != NULL) {
        size_t before = found - p;
        memcpy(wp, p, before);
        wp += before;
        memcpy(wp, new_s, nlen);
        wp += nlen;
        p = found + olen;
    }
    strcpy(wp, p);
    return result;
}

// sprintf wrapper: formats string like C sprintf, returns allocated string
#include <stdarg.h>
char *pg_sprintf(const char *fmt, ...) {
    va_list args, args2;
    va_start(args, fmt);
    va_copy(args2, args);
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    char *buf = (char *)malloc(len + 1);
    vsnprintf(buf, len + 1, fmt, args2);
    va_end(args2);
    return buf;
}

// read_line: reads a line from stdin, returns allocated string (without newline)
char *read_line(void) {
    size_t cap = 256;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    int c;
    while ((c = getchar()) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            buf = (char *)realloc(buf, cap);
        }
        buf[len++] = (char)c;
    }
    buf[len] = '\0';
    return buf;
}
