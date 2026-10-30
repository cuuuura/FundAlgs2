#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

typedef enum {
    SUCCESS = 0,
    ERR_INVALID_ARGUMENTS,
    ERR_MEMORY_ALLOCATION
} status_t;

typedef struct {
    FILE *file;
    const char *str;
    size_t pos;
} Source;

static int get_char(Source *src) {
    if (src->file) return fgetc(src->file);
    if (src->str && src->str[src->pos]) return (unsigned char)src->str[src->pos++];
    return EOF;
}

static void unget_char(Source *src, int c) {
    if (c == EOF) return;
    if (src->file) {
        ungetc(c, src->file);
    } else {
        if (src->pos > 0) src->pos--;
    }
}

static void skip_ws(Source *src) {
    int c;
    while ((c = get_char(src)) != EOF && isspace((unsigned char)c));
    if (c != EOF) unget_char(src, c);
}

// %Ro
static int read_roman(Source *src) {
    skip_ws(src);
    int total = 0, prev = 0, c;
    while ((c = get_char(src)) != EOF) {
        int cur = 0;
        switch (toupper((unsigned char)c)) {
            case 'I': cur = 1; break;
            case 'V': cur = 5; break;
            case 'X': cur = 10; break;
            case 'L': cur = 50; break;
            case 'C': cur = 100; break;
            case 'D': cur = 500; break;
            case 'M': cur = 1000; break;
            default: unget_char(src, c); return total;
        }
        if (prev < cur) total -= prev * 2;
        total += cur;
        prev = cur;
    }
    return total;
}

// %Zr
static unsigned int read_zeckendorf(Source *src) {
    skip_ws(src);
    
    size_t cap = 64;
    size_t len = 0;
    char *bits = (char *)malloc(cap * sizeof(char));
    if (!bits) return 0;

    int c;
    while ((c = get_char(src)) != EOF) {
        if (c == '0' || c == '1') {
            if (len + 1 >= cap) {
                cap *= 2;
                char *tmp = (char *)realloc(bits, cap * sizeof(char));
                if (!tmp) {
                    free(bits);
                    return 0;
                }
                bits = tmp;
            }
            bits[len++] = (char)c;
        } else {
            unget_char(src, c);
            break;
        }
    }

    if (len == 0) {
        free(bits);
        return 0;
    }

    unsigned long long fib[64];
    fib[0] = 1; fib[1] = 2;
    for (int i = 2; i < 64; i++) fib[i] = fib[i-1] + fib[i-2];

    unsigned int result = 0;
    for (size_t i = 0; i < len - 1; i++) {
        if (bits[i] == '1' && i < 64) {
            result += (unsigned int)fib[i];
        }
    }
    
    free(bits);
    return result;
}

// %Cv и %CV 
static int read_base(Source *src, int base) {
    skip_ws(src);
    if (base < 2 || base > 36) base = 10;
    int val = 0, c;
    while ((c = get_char(src)) != EOF) {
        int digit = -1;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'z') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') digit = c - 'A' + 10;
        
        if (digit < 0 || digit >= base) {
            unget_char(src, c);
            break;
        }
        val = val * base + digit;
    }
    return val;
}

static int overvscanf(Source *src, const char *format, va_list args) {
    int count = 0;
    const char *p = format;

    while (*p) {
        if (*p == '%') {
            if (strncmp(p, "%Ro", 3) == 0) {
                int *ptr = va_arg(args, int*);
                *ptr = read_roman(src);
                count++;
                p += 3;
            }
            else if (strncmp(p, "%Zr", 3) == 0) {
                unsigned int *ptr = va_arg(args, unsigned int*);
                *ptr = read_zeckendorf(src);
                count++;
                p += 3;
            }
            else if (strncmp(p, "%Cv", 3) == 0 || strncmp(p, "%CV", 3) == 0) {
                int *ptr = va_arg(args, int*);
                int base = va_arg(args, int);
                *ptr = read_base(src, base);
                count++;
                p += 3;
            }
            else {
                // Обработка стандартных флагов
                const char *end = p + 1;
                while (*end && !strchr("diouxXeEfgGaAcspn", *end)) end++;
                
                if (*end) {
                    char type = *end;
                    size_t spec_len = end - p + 1;
   
                    char *spec = (char *)malloc(spec_len + 1);
                    char *spec_n = (char *)malloc(spec_len + 4); // + %n\0
                    
                    if (!spec || !spec_n) {
                        free(spec);
                        free(spec_n);
                        return count;
                    }
                    
                    memcpy(spec, p, spec_len);
                    spec[spec_len] = '\0';
                    snprintf(spec_n, spec_len + 4, "%s%%n", spec);
                    
                    int chars_read = 0;
                    int res = 0;

                    if (type == 'd' || type == 'i') {
                        int *ptr = va_arg(args, int*);
                        res = src->file ? fscanf(src->file, spec_n, ptr, &chars_read) : sscanf(src->str + src->pos, spec_n, ptr, &chars_read);
                    } else if (type == 'u' || type == 'o' || type == 'x' || type == 'X') {
                        unsigned int *ptr = va_arg(args, unsigned int*);
                        res = src->file ? fscanf(src->file, spec_n, ptr, &chars_read) : sscanf(src->str + src->pos, spec_n, ptr, &chars_read);
                    } else if (type == 'f' || type == 'e' || type == 'g') {
                        float *ptr = va_arg(args, float*);
                        res = src->file ? fscanf(src->file, spec_n, ptr, &chars_read) : sscanf(src->str + src->pos, spec_n, ptr, &chars_read);
                    } else if (type == 's' || type == 'c') {
                        char *ptr = va_arg(args, char*);
                        res = src->file ? fscanf(src->file, spec_n, ptr, &chars_read) : sscanf(src->str + src->pos, spec_n, ptr, &chars_read);
                    }

                    free(spec);
                    free(spec_n);

                    if (res > 0) {
                        count += res;
                        if (!src->file && chars_read > 0) src->pos += chars_read;
                    }
                    p = end + 1;
                } else {
                    get_char(src);
                    p++;
                }
            }
        } else if (isspace((unsigned char)*p)) {
            skip_ws(src);
            p++;
        } else {
            get_char(src);
            p++;
        }
    }
    return count;
}

int overfscanf(FILE *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    Source src = { stream, NULL, 0 };
    int res = overvscanf(&src, format, args);
    va_end(args);
    return res;
}

int oversscanf(const char *str, const char *format, ...) {
    va_list args;
    va_start(args, format);
    Source src = { NULL, str, 0 };
    int res = overvscanf(&src, format, args);
    va_end(args);
    return res;
}

int main(void) {
    int r1, r2, r3;
    unsigned int z;
    
    const char *str = "IV 1A 10011 FF";
    int res = oversscanf(str, "%Ro %Cv %Zr %CV", &r1, &r2, 16, &z, &r3, 16);
    
    if (res == 4) {
        printf("\n");
        printf("Ro (Roman IV): %d\n", r1);     
        printf("Cv (Hex 1A): %d\n", r2);     
        printf("Zr (Zeckendorf 10011): %u\n", z);  
        printf("CV (Hex FF): %d\n", r3);        
    }

    int a; float b; char c;
    if (oversscanf("42 3.14 X", "%d %f %c", &a, &b, &c) == 3) {
        printf("\n\n");
        printf("Int: %d, Float: %.2f, Char: %c\n", a, b, c);
    }

    FILE *f = tmpfile();
    if (f) {
        fputs("MMXXIV 1010", f);
        rewind(f);
        
        int year;
        if (overfscanf(f, "%Ro %Zr", &year, &z) == 2) {
            printf("\n\n");
            printf("Year: %d\n", year);   
            printf("Zr: %u\n", z);        
        }
        fclose(f);
    }

    return 0;
}