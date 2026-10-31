#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

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
static int read_roman(Source *src, int *out_val) {
    skip_ws(src);
    int total = 0, prev = 0, c;
    int read_any = 0;
    
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
            default: unget_char(src, c); goto done;
        }
        if (prev < cur) total -= prev * 2;
        total += cur;
        prev = cur;
        read_any = 1;
    }
done:
    if (!read_any) return 0; // Ничего не прочитали
    *out_val = total;
    return 1;
}

// %Zr
static int read_zeckendorf(Source *src, unsigned int *out_val) {
    skip_ws(src);
    
    size_t cap = 64, len = 0;
    char *bits = (char *)malloc(cap * sizeof(char));
    if (!bits) return 0;

    int c;
    while ((c = get_char(src)) != EOF) {
        if (c == '0' || c == '1') {
            if (len + 1 >= cap) {
                cap *= 2;
                char *tmp = (char *)realloc(bits, cap * sizeof(char));
                if (!tmp) { free(bits); return 0; }
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
        return 0; // Ничего не прочитали
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
    *out_val = result;
    return 1;
}

// %Cv и %CV 
static int read_base(Source *src, int base, int *out_val) {
    skip_ws(src);
    if (base < 2 || base > 36) base = 10;
    
    int c = get_char(src);
    int sign = 1;
    
    // Обработка отрицательных чисел
    if (c == '-') {
        sign = -1;
        c = get_char(src);
    } else if (c == '+') {
        c = get_char(src);
    }

    int val = 0;
    int read_any = 0;

    while (c != EOF) {
        int digit = -1;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'z') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') digit = c - 'A' + 10;
        
        if (digit < 0 || digit >= base) {
            unget_char(src, c);
            break;
        }
        val = val * base + digit;
        read_any = 1;
        c = get_char(src);
    }

    if (!read_any) {
        // Если минус был, но цифр нет, возвращаем минус обратно в поток
        if (sign == -1) unget_char(src, '-'); 
        return 0;
    }
    
    *out_val = sign * val;
    return 1;
}

static int overvscanf(Source *src, const char *format, va_list args) {
    int count = 0;
    const char *p = format;

    while (*p) {
        if (*p == '%') {
            if (strncmp(p, "%Ro", 3) == 0) {
                int *ptr = va_arg(args, int*);
                if (read_roman(src, ptr)) count++;
                p += 3;
            }
            else if (strncmp(p, "%Zr", 3) == 0) {
                unsigned int *ptr = va_arg(args, unsigned int*);
                if (read_zeckendorf(src, ptr)) count++;
                p += 3;
            }
            else if (strncmp(p, "%Cv", 3) == 0 || strncmp(p, "%CV", 3) == 0) {
                int *ptr = va_arg(args, int*);
                int base = va_arg(args, int);
                if (read_base(src, base, ptr)) count++;
                p += 3;
            }
            else {
                // Обработка стандартных флагов
                const char *end = p + 1;
                
                if (*end == '*') end++;
                while (*end >= '0' && *end <= '9') end++;
                if (*end == 'h' || *end == 'l' || *end == 'L') end++;
                
                if (*end == '[') {
                    end++;
                    if (*end == '^') end++;
                    while (*end && *end != ']') end++;
                    if (*end == ']') end++;
                } else {
                    end++;
                }
                
                if (*end) {
                    size_t spec_len = end - p;
                    char *spec = (char *)malloc(spec_len + 1);
                    char *spec_n = (char *)malloc(spec_len + 4);
                    if (!spec || !spec_n) {
                        free(spec); free(spec_n);
                        return count;
                    }
                    
                    memcpy(spec, p, spec_len);
                    spec[spec_len] = '\0';
                    snprintf(spec_n, spec_len + 4, "%s%%n", spec);
                    
                    int chars_read = 0;
                    int res = 0;
                    int is_star = (strchr(spec, '*') != NULL);

                    if (!is_star) {
                        char type = *(end - 1);
                        if (type == 'd' || type == 'i') {
                            int *ptr = va_arg(args, int*);
                            res = src->file ? fscanf(src->file, spec_n, ptr, &chars_read) : sscanf(src->str + src->pos, spec_n, ptr, &chars_read);
                        } else if (type == 'u' || type == 'o' || type == 'x' || type == 'X') {
                            unsigned int *ptr = va_arg(args, unsigned int*);
                            res = src->file ? fscanf(src->file, spec_n, ptr, &chars_read) : sscanf(src->str + src->pos, spec_n, ptr, &chars_read);
                        } else if (type == 'f' || type == 'e' || type == 'g' || type == 'E' || type == 'G') {
                            float *ptr = va_arg(args, float*);
                            res = src->file ? fscanf(src->file, spec_n, ptr, &chars_read) : sscanf(src->str + src->pos, spec_n, ptr, &chars_read);
                        } else if (type == 's' || type == 'c' || type == '[') {
                            char *ptr = va_arg(args, char*);
                            res = src->file ? fscanf(src->file, spec_n, ptr, &chars_read) : sscanf(src->str + src->pos, spec_n, ptr, &chars_read);
                        }
                    } else {
                        res = src->file ? fscanf(src->file, spec_n, &chars_read) : sscanf(src->str + src->pos, spec_n, &chars_read);
                    }

                    free(spec);
                    free(spec_n);

                    if (res <= 0 && chars_read == 0) {
                        return count;
                    }

                    if (res > 0) count += res;
                    if (!src->file && chars_read > 0) src->pos += chars_read;
                    
                    p = end;
                } else {
                    return count;
                }
            }
        } else if (isspace((unsigned char)*p)) {
            skip_ws(src);
            p++;
        } else {
            int c = get_char(src);
            if (c == EOF || c != (unsigned char)*p) {
                if (c != EOF) unget_char(src, c);
                return count;
            }
            p++;
        }
    }
    return count;
}

int overfscanf(FILE *stream, const char *format, ...) {
    if (!stream || !format) return EOF;
    va_list args;
    va_start(args, format);
    Source src = { stream, NULL, 0 };
    int res = overvscanf(&src, format, args);
    va_end(args);
    return res;
}

int oversscanf(const char *str, const char *format, ...) {
    if (!str || !format) return EOF;
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

    printf("\n");
    {
        int val;
        if (oversscanf("-1A", "%Cv", &val, 16) == 1) {
            printf("Negative Cv (-1A hex): %d\n", val); // Ожидается -26
        }
    }

    printf("\n");
    {
        int b;
        char word[20];
        if (oversscanf("42 100 hello", "%*d %d %[a-z]", &b, word) == 2) {
            printf("Star & Set: b=%d, word=%s\n", b, word);
        }
    }

    printf("\n");
    {
        int a;
        int c = oversscanf("A 42", "B %d", &a);
        printf("Literal mismatch check: read %d items (expected 0)\n", c);
    }

    printf("\n");
    {
        int a;
        int c = oversscanf("", "%d", &a);
        printf("EOF check: read %d items (expected 0)\n", c);
    }

    FILE *f = tmpfile();
    if (f) {
        fputs("MMXXIV 1010", f);
        rewind(f);
        
        int year;
        if (overfscanf(f, "%Ro %Zr", &year, &z) == 2) {
            printf("\n");
            printf("Year: %d\n", year);   
            printf("Zr: %u\n", z);        
        }
        fclose(f);
    }

    return 0;
}