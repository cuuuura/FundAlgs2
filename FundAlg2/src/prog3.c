#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef enum {
    OK = 0,
    ERR_ARGS,
    ERR_NULL,
    ERR_MEMORY,
    ERR_OPEN,
    ERR_READ,
    ERR_EMPTY_SUBSTR
} Status;

static const char *status_msg(Status s) {
    switch (s) {
    case OK: return "ok";
    case ERR_ARGS: return "invalid arguments";
    case ERR_NULL: return "null pointer";
    case ERR_MEMORY: return "memory allocation failed";
    case ERR_OPEN: return "cannot open file";
    case ERR_READ: return "file read error";
    case ERR_EMPTY_SUBSTR: return "empty substring";
    default: return "unknown error";
    }
}

static Status load_file(const char *path, char **out_buf, size_t *out_len) {
    if (!path || !out_buf || !out_len) return ERR_NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return ERR_OPEN;

    size_t cap = 4096, len = 0;
    char *buf = (char *)malloc(cap * sizeof(char));
    if (!buf) { fclose(f); return ERR_MEMORY; }

    int c;
    while ((c = fgetc(f)) != EOF) {
        if (len + 1 >= cap) {
            cap *= 2;
            char *tmp = (char *)realloc(buf, cap * sizeof(char));
            if (!tmp) { 
                free(buf); 
                fclose(f); 
                return ERR_MEMORY; 
            }
            buf = tmp;
        }
        buf[len++] = (char)c;
    }

    if (ferror(f)) {
        free(buf);
        fclose(f);
        return ERR_READ;
    }
    
    fclose(f);
    buf[len] = '\0'; 

    *out_buf = buf;
    *out_len = len;
    return OK;
}

static size_t count_occurrences(const char *text, size_t text_len, const char *pat, size_t pat_len) {
    if (pat_len == 0 || text_len < pat_len) return 0;

    size_t count = 0;
    int shift[256];

    for (int i = 0; i < 256; i++) {
        shift[i] = (int)pat_len;
    }
    
    for (size_t i = 0; i < pat_len - 1; i++) {
        shift[(unsigned char)pat[i]] = (int)(pat_len - 1 - i);
    }

    size_t i = 0;
    while (i <= text_len - pat_len) {
        long long j = (long long)pat_len - 1;

        while (j >= 0 && text[i + j] == pat[j]) {
            j--;
        }

        if (j < 0) {
            count++;
            i += shift[(unsigned char)text[i + pat_len - 1]];
        } else {
            i += shift[(unsigned char)text[i + pat_len - 1]];
        }
    }
    return count;
}

static Status search_in_file(const char *path, const char *substr, size_t substr_len, size_t *out_count) {
    if (!path || !substr || !out_count) return ERR_NULL;
    if (substr_len == 0) return ERR_EMPTY_SUBSTR;

    char *buf = NULL;
    size_t len = 0;
    Status st = load_file(path, &buf, &len);
    if (st != OK) return st;

    *out_count = count_occurrences(buf, len, substr, substr_len);
    free(buf);
    return OK;
}

Status search_in_files(const char *substr, int nfiles, size_t *out_total, size_t *out_counts, ...) {
    if (!substr || !out_total) return ERR_NULL;
    if (nfiles < 0) return ERR_ARGS;
    if (substr[0] == '\0') return ERR_EMPTY_SUBSTR;

    size_t substr_len = strlen(substr);
    size_t total = 0;

    va_list ap;
    va_start(ap, out_counts);
    for (int i = 0; i < nfiles; i++) {
        const char *path = va_arg(ap, const char *);
        if (!path) {
            va_end(ap);
            return ERR_NULL;
        }
        size_t count = 0;
        Status file_st = search_in_file(path, substr, substr_len, &count);
        if (file_st != OK) {
            va_end(ap);
            return file_st;
        }
        total += count;
        if (out_counts) out_counts[i] = count;
    }
    va_end(ap);

    *out_total = total;
    return OK;
}

int main(void) {
    {
        const char *paths[] = {"f1.txt"};
        size_t counts[1] = {0};
        size_t total = 0;
        Status st = search_in_files("hello", 1, &total, counts, paths[0]);
        if (st != OK) {
            printf("Error: %s\n", status_msg(st));
        } else {
            printf("Total occurrences: %zu\n", total);
            printf("  %s: %zu\n", paths[0], counts[0]);
        }
    }
    printf("\n");
    {
        const char *paths[] = {"f1.txt", "f2.txt"};
        size_t counts[2] = {0, 0};
        size_t total = 0;
        Status st = search_in_files("hello", 2, &total, counts, paths[0], paths[1]);
        if (st != OK) {
            printf("Error: %s\n", status_msg(st));
        } else {
            printf("Total occurrences: %zu\n", total);
            for (int i = 0; i < 2; i++) {
                printf("  %s: %zu\n", paths[i], counts[i]);
            }
        }
    }
    printf("\n");
    {
        const char *paths[] = {"f1.txt", "f2.txt", "f3.txt"};
        size_t counts[3] = {0, 0, 0};
        size_t total = 0;
        Status st = search_in_files("hello", 3, &total, counts,
                                     paths[0], paths[1], paths[2]);
        if (st != OK) {
            printf("Error: %s\n", status_msg(st));
        } else {
            printf("Total occurrences: %zu\n", total);
            for (int i = 0; i < 3; i++) {
                printf("  %s: %zu\n", paths[i], counts[i]);
            }
        }
    }
    printf("\n");
    {
        const char *paths[] = {"f4.txt", "f4.txt"};
        size_t counts[2] = {0, 0};
        size_t total = 0;
        Status st = search_in_files("aa", 2, &total, counts,
                                     paths[0], paths[1]);
        if (st != OK) {
            printf("Error: %s\n", status_msg(st));
        } else {
            printf("Total occurrences: %zu\n", total);
            for (int i = 0; i < 2; i++) {
                printf("  %s: %zu\n", paths[i], counts[i]);
            }
        }
    }
    printf("\n");
    {   
        const char *paths[] = {"missing.txt"};
        size_t counts[1] = {0};
        size_t total = 0;
        Status st = search_in_files("hello", 1, &total, counts, paths[0]);
        if (st != OK) {
            printf("Expected error: %s\n", status_msg(st));
        } else {
            printf("Unexpected success, total=%zu\n", total);
        }
    }

    return 0;
}