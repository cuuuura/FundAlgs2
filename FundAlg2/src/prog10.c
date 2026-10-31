#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <float.h>
#include <limits.h>

typedef enum {
    SUCCESS = 0,
    ERR_INVALID_ARGUMENTS,
    ERR_MEMORY_ALLOCATION,
    ERR_OVERFLOW
} status_t;

static const char *status_msg(status_t s) {
    switch (s) {
    case SUCCESS: return "success";
    case ERR_INVALID_ARGUMENTS: return "invalid arguments";
    case ERR_MEMORY_ALLOCATION: return "memory allocation failed";
    case ERR_OVERFLOW: return "numeric overflow detected";
    default: return "unknown error";
    }
}

status_t new_polynom(double eps, double a, double **out_g, int n, ...) {
    if (out_g == NULL || n < 0 || n >= INT_MAX || eps < 0.0) return ERR_INVALID_ARGUMENTS;

    double *f = (double *)malloc((n + 1) * sizeof(double));
    if (f == NULL) return ERR_MEMORY_ALLOCATION;

    double *g = (double *)malloc((n + 1) * sizeof(double));
    if (g == NULL) {
        free(f);
        return ERR_MEMORY_ALLOCATION;
    }

    va_list ap;
    va_start(ap, n);
    for (int i = 0; i <= n; i++) {
        f[i] = va_arg(ap, double);
        if (!isfinite(f[i])) {
            free(f); free(g); va_end(ap);
            return ERR_OVERFLOW; 
        }
    }
    va_end(ap);


    for (int k = 0; k <= n; k++) {
        for (int i = n - 1; i >= k; i--) {
            f[i] = f[i] + a * f[i + 1];
            
            if (!isfinite(f[i])) {
                free(f);
                free(g);
                return ERR_OVERFLOW;
            }
        }
        g[k] = f[k];
        
        if (fabs(g[k]) < eps) {
            g[k] = 0.0;
        }
    }

    free(f);
    *out_g = g;
    return SUCCESS;
}

static void print_polynom(const char *label, const double *coeffs, int n, double a) {
    printf("%s(x) = ", label);
    for (int i = 0; i <= n; i++) {
        double val = coeffs[i];
        
        if (i > 0) {
            if (val >= 0.0) printf(" + ");
            else printf(" - ");
            val = fabs(val);
        } else if (val < 0.0) {
            printf("-");
            val = fabs(val);
        }

        if (i == 0) {
            printf("%.10g", val);
        } else if (i == 1) {
            printf("%.10g*(x - %.10g)", val, a);
        } else {
            printf("%.10g*(x - %.10g)^%d", val, a, i);
        }
    }
    printf("\n");
} 

int main(void) {
    double *g = NULL;
    status_t st;

    printf("\n");
    {
        st = new_polynom(1e-9, 1.0, &g, 2, 1.0, 2.0, 3.0);
        printf("f(x) = 1 + 2x + 3x^2, a = 1.0\n");
        if (st == SUCCESS) {
            print_polynom("g", g, 2, 1.0);
            free(g);
        } else {
            printf("Error: %s\n", status_msg(st));
        }
    }

    printf("\n\n");
    {
        st = new_polynom(1e-9, 0.0, &g, 1, 0.001, 0.002);
        printf("f(x) = 0.001 + 0.002x, a = 0.0\n");
        if (st == SUCCESS) {
            print_polynom("g", g, 1, 0.0);
            free(g);
        } else {
            printf("Error: %s\n", status_msg(st));
        }
    }

    printf("\n\n");
    {
        st = new_polynom(1e-9, 1e308, &g, 2, 1e308, 1e308, 1e308);
        printf("f(x) = 1e308 + 1e308*x + 1e308*x^2, a = 1e308\n");
        if (st == SUCCESS) {
            print_polynom("g", g, 2, 1e308);
            free(g);
        } else {
            printf("Expected error: %s\n", status_msg(st));
        }
    }

    printf("\n\n");
    {
        st = new_polynom(-1.0, 1.0, &g, 2, 1.0, 2.0, 3.0);
        if (st != SUCCESS) {
            printf("Expected error: %s\n", status_msg(st));
        }
    }

    return 0;
}