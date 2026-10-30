#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#include <limits.h>

typedef enum {
    OK = 0,
    ERR_ARGS,
    ERR_NULL,
    ERR_MEMORY,
    ERR_FORMAT
} Status;

static const char *status_msg(Status s) {
    switch (s) {
    case OK: return "ok";
    case ERR_ARGS: return "invalid arguments";
    case ERR_NULL: return "null pointer";
    case ERR_MEMORY: return "memory allocation failed";
    case ERR_FORMAT: return "invalid format string";
    default: return "unknown error";
    }
}

static Status append_char(char **buf, size_t *len, size_t *cap, char c) {
    if (*len + 1 >= *cap) {
        size_t new_cap = (*cap == 0) ? 64 : *cap * 2;
        char *tmp = (char *)realloc(*buf, new_cap * sizeof(char));
        if (!tmp) return ERR_MEMORY;
        *buf = tmp;
        *cap = new_cap;
    }
    (*buf)[(*len)++] = c;
    (*buf)[*len] = '\0';
    return OK;
}

static Status append_str(char **buf, size_t *len, size_t *cap, const char *str) {
    size_t slen = strlen(str);
    if (*len + slen >= *cap) {
        size_t new_cap = (*cap == 0) ? 64 : *cap;
        while (new_cap <= *len + slen) new_cap *= 2;
        char *tmp = (char *)realloc(*buf, new_cap * sizeof(char));
        if (!tmp) return ERR_MEMORY;
        *buf = tmp;
        *cap = new_cap;
    }
    memcpy(*buf + *len, str, slen);
    *len += slen;
    (*buf)[*len] = '\0';
    return OK;
}

// %Ro
static Status append_roman(int val, char **buf, size_t *len, size_t *cap) {
    if (val <= 0) return OK; 
    const int values[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
    const char *romans[] = {"M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I"};
    
    for (int i = 0; i < 13; i++) {
        while (val >= values[i]) {
            Status st = append_str(buf, len, cap, romans[i]);
            if (st != OK) return st;
            val -= values[i];
        }
    }
    return OK;
}

// %Zr
static Status append_zeckendorf(unsigned int val, char **buf, size_t *len, size_t *cap) {
    if (val == 0) {
        Status st = append_char(buf, len, cap, '0');
        if (st != OK) return st;
        return append_char(buf, len, cap, '1');
    }
    
    unsigned long long fib[64];
    fib[0] = 1; fib[1] = 2;
    int count = 2;
    while (fib[count - 1] + fib[count - 2] <= val) {
        fib[count] = fib[count - 1] + fib[count - 2];
        count++;
    }
    
    char bits[64] = {0};
    for (int i = count - 1; i >= 0; i--) {
        if (val >= fib[i]) {
            bits[i] = '1';
            val -= fib[i];
        } else {
            bits[i] = '0';
        }
    }
    
    for (int i = 0; i < count; i++) {
        Status st = append_char(buf, len, cap, bits[i]);
        if (st != OK) return st;
    }
    return append_char(buf, len, cap, '1');
}

// %Cv и %CV
static Status append_base(int val, int base, int is_upper, char **buf, size_t *len, size_t *cap) {
    if (base < 2 || base > 36) base = 10;
    if (val == 0) return append_char(buf, len, cap, '0');
    
    const char *digits = is_upper ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" : "0123456789abcdefghijklmnopqrstuvwxyz";
    char temp[65];
    int pos = 0;
    long long v = val;
    
    if (v < 0) v = -v;
    
    while (v > 0) {
        temp[pos++] = digits[v % base];
        v /= base;
    }
    
    if (val < 0) {
        Status st = append_char(buf, len, cap, '-');
        if (st != OK) return st;
    }
    for (int i = pos - 1; i >= 0; i--) {
        Status st = append_char(buf, len, cap, temp[i]);
        if (st != OK) return st;
    }
    return OK;
}

// %to и %TO
static Status append_from_base(const char *str, int base, char **buf, size_t *len, size_t *cap) {
    if (base < 2 || base > 36) base = 10;
    
    long long val = 0;
    int sign = 1;
    const char *p = str;
    if (*p == '-') { sign = -1; p++; }
    
    while (*p) {
        int digit = -1;
        if (*p >= '0' && *p <= '9') digit = *p - '0';
        else if (*p >= 'a' && *p <= 'z') digit = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'Z') digit = *p - 'A' + 10;
        
        if (digit < 0 || digit >= base) break;
        val = val * base + digit;
        p++;
    }
    
    val *= sign;
    
    char temp[32];
    snprintf(temp, sizeof(temp), "%lld", val);
    return append_str(buf, len, cap, temp);
}

// %mi, %mu, %md, %mf
static Status append_memory_dump(void *ptr, size_t size, char **buf, size_t *len, size_t *cap) {
    unsigned char *bytes = (unsigned char *)ptr;
    for (size_t i = 0; i < size; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            Status st = append_char(buf, len, cap, (bytes[i] & (1 << bit)) ? '1' : '0');
            if (st != OK) return st;
        }
        if (i < size - 1) {
            Status st = append_char(buf, len, cap, ' ');
            if (st != OK) return st;
        }
    }
    return OK;
}

static Status overvsprintf(char **out, const char *format, va_list args) {
    if (!out || !format) return ERR_NULL;

    char *buf = NULL;
    size_t len = 0, cap = 0;
    Status st = OK;

    va_list copy;
    va_copy(copy, args);
    const char *p = format;

    while (*p) {
        if (*p == '%') {
            if (strncmp(p, "%Ro", 3) == 0) {
                st = append_roman(va_arg(copy, int), &buf, &len, &cap);
                p += 3;
            }
            else if (strncmp(p, "%Zr", 3) == 0) {
                st = append_zeckendorf(va_arg(copy, unsigned int), &buf, &len, &cap);
                p += 3;
            }
            else if (strncmp(p, "%Cv", 3) == 0) {
                int val = va_arg(copy, int);
                int base = va_arg(copy, int);
                st = append_base(val, base, 0, &buf, &len, &cap);
                p += 3;
            }
            else if (strncmp(p, "%CV", 3) == 0) {
                int val = va_arg(copy, int);
                int base = va_arg(copy, int);
                st = append_base(val, base, 1, &buf, &len, &cap);
                p += 3;
            }
            else if (strncmp(p, "%to", 3) == 0) {
                const char *str = va_arg(copy, const char *);
                int base = va_arg(copy, int);
                st = append_from_base(str, base, &buf, &len, &cap);
                p += 3;
            }
            else if (strncmp(p, "%TO", 3) == 0) {
                const char *str = va_arg(copy, const char *);
                int base = va_arg(copy, int);
                st = append_from_base(str, base, &buf, &len, &cap);
                p += 3;
            }
            else if (strncmp(p, "%mi", 3) == 0) {
                int val = va_arg(copy, int);
                st = append_memory_dump(&val, sizeof(int), &buf, &len, &cap);
                p += 3;
            }
            else if (strncmp(p, "%mu", 3) == 0) {
                unsigned int val = va_arg(copy, unsigned int);
                st = append_memory_dump(&val, sizeof(unsigned int), &buf, &len, &cap);
                p += 3;
            }
            else if (strncmp(p, "%md", 3) == 0) {
                double val = va_arg(copy, double);
                st = append_memory_dump(&val, sizeof(double), &buf, &len, &cap);
                p += 3;
            }
            else if (strncmp(p, "%mf", 3) == 0) {
                double val = va_arg(copy, double);
                float f_val = (float)val;
                st = append_memory_dump(&f_val, sizeof(float), &buf, &len, &cap);
                p += 3;
            }
            else if (*(p + 1) == '%') {
                st = append_char(&buf, &len, &cap, '%');
                p += 2;
            }
            else {
                // Парсинг стандартных спецификаторов
                const char *end = p + 1;
                while (*end && !strchr("diouxXeEfgGaAcspn", *end)) end++;
                
                if (*end) {
                    size_t spec_len = end - p + 1;
                    char spec[32];
                    if (spec_len >= sizeof(spec)) spec_len = sizeof(spec) - 1;
                    memcpy(spec, p, spec_len);
                    spec[spec_len] = '\0';

                    va_list args_count;
                    va_copy(args_count, copy);
                    int n = vsnprintf(NULL, 0, spec, args_count);
                    va_end(args_count);

                    if (n >= 0) {
                        char *tmp = (char *)malloc((size_t)n + 1);
                        if (tmp) {
                            vsnprintf(tmp, (size_t)n + 1, spec, copy); 
                            st = append_str(&buf, &len, &cap, tmp);
                            free(tmp);
                        } else {
                            st = ERR_MEMORY;
                        }
                    } else {
                        st = ERR_FORMAT;
                    }
                    p = end + 1;
                } else {
                    st = append_char(&buf, &len, &cap, *p);
                    p++;
                }
            }
            if (st != OK) break;
        } else {
            st = append_char(&buf, &len, &cap, *p);
            if (st != OK) break;
            p++;
        }
    }

    va_end(copy);

    if (st != OK) {
        free(buf);
        return st;
    }

    if (buf == NULL) {
        buf = (char *)malloc(1);
        if (!buf) return ERR_MEMORY;
        buf[0] = '\0';
    }

    *out = buf;
    return OK;
}

Status oversprintf(char **out, const char *format, ...) {
    va_list args;
    va_start(args, format);
    Status st = overvsprintf(out, format, args);
    va_end(args);
    return st;
}

Status overfprintf(FILE *stream, const char *format, ...) {
    if (!stream) return ERR_NULL;
    
    char *res = NULL;
    va_list args;
    va_start(args, format);
    Status st = overvsprintf(&res, format, args);
    va_end(args);

    if (st == OK && res) {
        fputs(res, stream);
        free(res);
    }
    return st;
}

int main(void) {
    {
        printf("\n");
        char *test = NULL;
        Status st = oversprintf(&test, "Roman: %Ro, Zeckendorf: %Zr", 1984, 100);
        if (st == OK) {
            printf("%s\n", test);
            free(test);
        } else {
            printf("Error: %s\n", status_msg(st));
        }
    }
    printf("\n");
    {
        char *test = NULL;
        Status st = oversprintf(&test, "Base 16: %Cv, Base 2: %CV", 255, 16, 255, 2);
        if (st == OK) {
            printf("%s\n", test);
            free(test);
        } else {
            printf("Error: %s\n", status_msg(st));
        }
    }
    printf("\n");

    {
        char *test = NULL;
        Status st = oversprintf(&test, "From base: %to, Standard int: %d, Float: %0.2f", "ff", 16, 42, 3.14159);
        if (st == OK) {
            printf("%s\n", test);
            free(test);
        } else {
            printf("Error: %s\n", status_msg(st));
        }
    }
    printf("\n");

    {
        Status st = overfprintf(stdout, "Int 1: %mi\n", 1);
        if (st != OK) printf("Error: %s\n", status_msg(st));
        
        st = overfprintf(stdout, "Double 1.0: %md\n", 1.0);
        if (st != OK) printf("Error: %s\n", status_msg(st));
        
        st = overfprintf(stdout, "Float 1.0: %mf\n", 1.0f);
        if (st != OK) printf("Error: %s\n", status_msg(st));
    }
    printf("\n");

    {
        char *test = NULL;
        Status st = oversprintf(&test, "A%cB%sC", 'x', "");
        if (st == OK) {
            printf("Result: '%s'\n", test);
            free(test);
        } else {
            printf("Error: %s\n", status_msg(st));
        }
    }

    return 0;
}