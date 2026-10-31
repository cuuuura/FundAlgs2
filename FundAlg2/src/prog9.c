#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>

#define EPS 1e-12

typedef enum {
    SUCCESS = 0,
    ERR_INVALID_ARGUMENTS,
    ERR_INVALID_BASE,
    ERR_INVALID_VALUE
} status_t;

static const char *status_msg(status_t s) {
    switch (s) {
    case SUCCESS: return "success";
    case ERR_INVALID_ARGUMENTS: return "invalid arguments";
    case ERR_INVALID_BASE: return "invalid base (must be >= 2)";
    case ERR_INVALID_VALUE: return "invalid value (must be finite and in (0; 1))";
    default: return "unknown error";
    }
}

// Восстановление дроби p/q из числа double с помощью цепных дробей
static void get_fraction(double x, long long *out_p, long long *out_q) {
    long long h1 = 1, h0 = 0;
    long long k1 = 0, k0 = 1;
    double b = x;
    
    for (int i = 0; i < 30; i++) {
        long long a = (long long)b;
        long long h2 = a * h1 + h0;
        long long k2 = a * k1 + k0;
        
        if (k2 > 1000000000LL || k2 <= 0) break;
        
        h0 = h1; h1 = h2;
        k0 = k1; k1 = k2;
        
        double frac = b - (double)a;
        if (fabs(frac) < EPS) break;
        
        b = 1.0 / frac;
    }
    
    *out_p = h1;
    *out_q = k1;
}

static status_t is_finite(double x, int base, bool *out_is_finite) {
    if (out_is_finite == NULL) return ERR_INVALID_ARGUMENTS;
    if (base < 2) return ERR_INVALID_BASE;
    
    // Проверка на NaN и inf
    if (isnan(x) || isinf(x)) return ERR_INVALID_VALUE;
    if (x <= 0.0 || x >= 1.0) return ERR_INVALID_VALUE;

    long long p, q;
    get_fraction(x, &p, &q);

    // Сокращаем дробь p/q на НОД
    long long a = p, b = q;
    while (b != 0) {
        long long temp = b;
        b = a % b;
        a = temp;
    }
    q /= a; 

    for (int i = 2; i <= base && q > 1; i++) {
        if (base % i == 0) {
            while (q % i == 0) {
                q /= i;
            }
        }
    }

    *out_is_finite = (q == 1);
    return SUCCESS;
}

status_t check_fractions(int base, int count, bool *out_results, ...) {
    if (out_results == NULL) return ERR_INVALID_ARGUMENTS;
    if (base < 2) return ERR_INVALID_BASE; 
    if (count <= 0) return ERR_INVALID_ARGUMENTS;

    va_list ap;
    va_start(ap, out_results);

    for (int i = 0; i < count; i++) {
        double x = va_arg(ap, double);
        
        status_t st = is_finite(x, base, &out_results[i]);
        if (st != SUCCESS) {
            va_end(ap);
            return st;
        }
    }

    va_end(ap);
    return SUCCESS;
}

int main(void) {
    {
        int count = 3;
        bool *results = (bool *)malloc(count * sizeof(bool));
        if (!results) return 1;

        double a = 0.5;
        double b = 0.25;
        double c = 1.0 / 3.0;

        status_t st = check_fractions(10, count, results, a, b, c);
        if (st == SUCCESS) {
            printf("0.5  -> %s\n", results[0] ? "finite" : "infinite");
            printf("0.25 -> %s\n", results[1] ? "finite" : "infinite");
            printf("1/3  -> %s\n", results[2] ? "finite" : "infinite");
        } else {
            printf("Error: %s\n", status_msg(st));
        }
        free(results);
    }
    printf("\n");
    {
        int count = 2;
        bool *results = (bool *)malloc(count * sizeof(bool));
        if (!results) return 1;

        double a = 0.5;
        double b = 0.1;

        status_t st = check_fractions(2, count, results, a, b);
        if (st == SUCCESS) {
            printf("0.5 -> %s\n", results[0] ? "finite" : "infinite");
            printf("0.1 -> %s\n", results[1] ? "finite" : "infinite");
        } else {
            printf("Error: %s\n", status_msg(st));
        }
        free(results);
    }
    printf("\n");
    {
        int count = 2;
        bool *results = (bool *)malloc(count * sizeof(bool));
        if (!results) return 1;

        double a = 0.5;
        double b = 1.0 / 3.0;

        status_t st = check_fractions(3, count, results, a, b);
        if (st == SUCCESS) {
            printf("0.5  -> %s\n", results[0] ? "finite" : "infinite");
            printf("1/3  -> %s\n", results[1] ? "finite" : "infinite");
        } else {
            printf("Error: %s\n", status_msg(st));
        }
        free(results);
    }
    printf("\n");
    {
        int count = 1;
        bool *results = (bool *)malloc(count * sizeof(bool));
        if (!results) return 1;

        status_t st = check_fractions(10, count, results, 1.5);
        if (st == SUCCESS) {
            printf("1.5 -> %s\n", results[0] ? "finite" : "infinite");
        } else {
            printf("Expected error: %s\n", status_msg(st));
        }
        free(results);
    }
    printf("\n");
    {
        int count = 2;
        bool *results = (bool *)malloc(count * sizeof(bool));
        if (!results) return 1;

        double nan_val = nan("");
        double tiny = 1e-10;

        status_t st = check_fractions(2, count, results, tiny, nan_val);
        if (st == SUCCESS) {
            printf("1e-10 in base 2 -> %s\n", results[0] ? "finite" : "infinite");
            printf("NaN -> %s\n", results[1] ? "finite" : "infinite");
        } else {
            printf("Expected error: %s\n", status_msg(st));
        }
        free(results);
    }

    return 0;
}