#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _pg_argc;
static char** _pg_argv;

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

