#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>

// --- Backtrace (Linux) ---
#ifdef __linux__
#include <execinfo.h>
void pg_print_backtrace(void) {
    void *buffer[64];
    int nframes = backtrace(buffer, 64);
    char **symbols = backtrace_symbols(buffer, nframes);
    fprintf(stderr, "Stack trace:\n");
    for (int i = 0; i < nframes; i++)
        fprintf(stderr, "  %s\n", symbols[i]);
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
