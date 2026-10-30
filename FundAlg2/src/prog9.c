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
    case ERR_INVALID_BASE: return "invalid base (must be 2..36)";
    case ERR_INVALID_VALUE: return "invalid value (must be in (0; 1))";
    default: return "unknown error";
    }
}

static status_t is_finite(double x, int base, bool *out_is_finite) {
    if (out_is_finite == NULL) return ERR_INVALID_ARGUMENTS;
    if (base < 2 || base > 36) return ERR_INVALID_BASE;
    
    if (x <= 0.0 || x >= 1.0) return ERR_INVALID_VALUE;

    const int MAX_ITER = 1100;
    
    double current = x;
    for (int i = 0; i < MAX_ITER; i++) {
        current *= (double)base;
        
        long long int_part = (long long)current;
        double frac_part = current - (double)int_part;
        
        if (fabs(frac_part) < EPS) {
            *out_is_finite = true;
            return SUCCESS;
        }
        
        current = frac_part;
    }

    *out_is_finite = false;
    return SUCCESS;
}

status_t check_fractions(int base, int count, bool *out_results, ...) {
    if (out_results == NULL) return ERR_INVALID_ARGUMENTS;
    if (base < 2 || base > 36) return ERR_INVALID_BASE;
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

    return 0;
}