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

typedef struct {
    size_t line_num;
    size_t col_num;
} Match;

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

static Status find_occurrences(const char *text, size_t text_len, const char *pat, size_t pat_len, Match **out_matches, size_t *out_count) {
    if (pat_len == 0 || text_len < pat_len) {
        *out_matches = NULL;
        *out_count = 0;
        return OK;
    }

    size_t cap = 16;
    size_t count = 0;
    Match *matches = (Match *)malloc(cap * sizeof(Match));
    if (!matches) return ERR_MEMORY;

    size_t current_line = 1;
    size_t line_start_pos = 0;

    int shift[256];
    for (int i = 0; i < 256; i++) shift[i] = (int)pat_len;
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
            if (count == cap) {
                cap *= 2;
                Match *tmp = (Match *)realloc(matches, cap * sizeof(Match));
                if (!tmp) { free(matches); return ERR_MEMORY; }
                matches = tmp;
            }

            size_t current_pos = line_start_pos;
            size_t l = current_line;
            while (current_pos < i) {
                if (text[current_pos] == '\n') {
                    l++;
                    line_start_pos = current_pos + 1;
                }
                current_pos++;
            }

            matches[count].line_num = l;
            matches[count].col_num = i - line_start_pos + 1;
            count++;

            i += shift[(unsigned char)text[i + pat_len - 1]];
        } else {
            i += shift[(unsigned char)text[i + pat_len - 1]];
        }
    }

    *out_matches = matches;
    *out_count = count;
    return OK;
}

static Status search_in_file(const char *path, const char *substr, size_t substr_len, Match **out_matches, size_t *out_count) {
    if (!path || !substr || !out_matches || !out_count) return ERR_NULL;
    if (substr_len == 0) return ERR_EMPTY_SUBSTR;

    char *buf = NULL;
    size_t len = 0;
    Status st = load_file(path, &buf, &len);
    if (st != OK) return st;

    st = find_occurrences(buf, len, substr, substr_len, out_matches, out_count);
    free(buf);
    return st;
}

Status search_in_files(const char *substr, int nfiles, size_t *out_total, size_t *out_counts, Match **out_matches, ...) {
    if (!substr || !out_total || !out_counts || !out_matches) return ERR_NULL;
    if (nfiles < 0) return ERR_ARGS;
    if (substr[0] == '\0') return ERR_EMPTY_SUBSTR;

    size_t substr_len = strlen(substr);
    size_t total = 0;

    va_list ap;
    va_start(ap, out_matches);
    for (int i = 0; i < nfiles; i++) {
        const char *path = va_arg(ap, const char *);
        if (!path) {
            // Откат изменений при ошибке
            for (int j = 0; j < i; j++) free(out_matches[j]);
            va_end(ap);
            return ERR_NULL;
        }
        
        Status file_st = search_in_file(path, substr, substr_len, &out_matches[i], &out_counts[i]);
        if (file_st != OK) {
            // Откат изменений при ошибке
            for (int j = 0; j < i; j++) free(out_matches[j]);
            va_end(ap);
            return file_st;
        }
        total += out_counts[i];
    }
    va_end(ap);

    *out_total = total;
    return OK;
}

int main(void) {
    {
        const char *paths[] = {"f1.txt", "f2.txt"};
        int nfiles = 2;
        
        // Выделяем массивы под результаты
        Match **matches = (Match **)malloc(nfiles * sizeof(Match *));
        size_t *counts = (size_t *)malloc(nfiles * sizeof(size_t));
        size_t total = 0;

        if (!matches || !counts) {
            free(matches); free(counts);
            return 1;
        }

        Status st = search_in_files("hello", nfiles, &total, counts, matches, paths[0], paths[1]);
        
        if (st != OK) {
            printf("Error: %s\n", status_msg(st));
        } else {
            printf("Total occurrences: %zu\n", total);
            for (int i = 0; i < nfiles; i++) {
                printf("  %s: %zu matches\n", paths[i], counts[i]);
                for (size_t j = 0; j < counts[i]; j++) {
                    printf("    Line %zu, Sym %zu\n", matches[i][j].line_num, matches[i][j].col_num);
                }
                free(matches[i]);
            }
        }
        
        free(matches);
        free(counts);
    }
    return 0;
}