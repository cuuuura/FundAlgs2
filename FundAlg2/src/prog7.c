#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <float.h>

typedef enum {
    OK = 0,
    ERR_ARGS,
    ERR_NULL,
    ERR_MEMORY,
    ERR_INVALID_VECTOR,
    ERR_INVALID_MATRIX
} Status;

static const char *status_msg(Status s) {
    switch (s) {
    case OK: return "ok";
    case ERR_ARGS: return "invalid arguments";
    case ERR_NULL: return "null pointer";
    case ERR_MEMORY: return "memory allocation failed";
    case ERR_INVALID_VECTOR: return "invalid vector dimensions";
    case ERR_INVALID_MATRIX: return "invalid matrix dimensions or data";
    default: return "unknown error";
    }
}

typedef struct {
    int n;
    double *coords;
} Vector;

typedef struct {
    int n;
    double **data;
} Matrix;

typedef double (*NormFunc)(const Vector *v, const void *ctx);

typedef struct {
    double max_norm;
    int count;
    const Vector **max_vectors;
} NormResult;

static int is_close(double a, double b, double eps) {
    if (isinf(a) && isinf(b)) return 1;
    if (isnan(a) || isnan(b)) return 0;
    return fabs(a - b) <= eps;
}

static double norm_inf(const Vector *v, const void *ctx) {
    (void)ctx;
    if (!v || !v->coords) return -1.0;
    double max_val = 0.0;
    for (int i = 0; i < v->n; i++) {
        double abs_val = fabs(v->coords[i]);
        if (abs_val > max_val) max_val = abs_val;
    }
    return max_val;
}

static double norm_p(const Vector *v, const void *ctx) {
    if (!v || !v->coords || !ctx) return -1.0;
    double p = *(const double *)ctx;
    if (p < 1.0) return -1.0;

    long double sum = 0.0L;
    for (int i = 0; i < v->n; i++) {
        sum += powl(fabsl(v->coords[i]), (long double)p);
    }
    return (double)powl(sum, 1.0L / p);
}

static double norm_A(const Vector *v, const void *ctx) {
    if (!v || !v->coords || !ctx) return -1.0;
    const Matrix *A = (const Matrix *)ctx;
    if (A->n != v->n || !A->data) return -1.0;

    long double sum = 0.0L;
    for (int i = 0; i < v->n; i++) {
        if (!A->data[i]) return -1.0;
        long double Ax_i = 0.0L;
        for (int j = 0; j < v->n; j++) {
            Ax_i += (long double)A->data[i][j] * v->coords[j];
        }
        sum += Ax_i * v->coords[i];
    }
    if (sum < 0.0L) return -1.0;
    return (double)sqrtl(sum);
}

static Status find_longest_vectors(NormResult *out_results, double epsilon, int n, int num_vectors, ...) {
    if (!out_results || n <= 0 || num_vectors <= 0 || epsilon < 0.0) return ERR_ARGS;

    va_list ap;
    va_start(ap, num_vectors);

    const Vector **vectors = (const Vector **)malloc((size_t)num_vectors * sizeof(const Vector *));
    if (!vectors) { va_end(ap); return ERR_MEMORY; }

    for (int i = 0; i < num_vectors; i++) {
        vectors[i] = va_arg(ap, const Vector *);
        if (!vectors[i] || !vectors[i]->coords) {
            free(vectors); va_end(ap); return ERR_NULL;
        }
        if (vectors[i]->n != n) {
            free(vectors); va_end(ap); return ERR_INVALID_VECTOR;
        }
    }

    int num_norms = va_arg(ap, int);
    if (num_norms <= 0) { free(vectors); va_end(ap); return ERR_ARGS; }

    for (int k = 0; k < num_norms; k++) {
        NormFunc func = va_arg(ap, NormFunc);
        void *ctx = va_arg(ap, void *);

        if (!func) {
            for (int j = 0; j < k; j++) {
                free(out_results[j].max_vectors);
                out_results[j].max_vectors = NULL;
                out_results[j].count = 0;
            }
            free(vectors); va_end(ap); return ERR_NULL;
        }

        // 1. Находим истинный максимум без эпсилона
        double true_max = -1.0;
        int is_first = 1;
        for (int i = 0; i < num_vectors; i++) {
            double val = func(vectors[i], ctx);
            if (val < 0.0) {
                for (int j = 0; j < k; j++) {
                    free(out_results[j].max_vectors);
                    out_results[j].max_vectors = NULL;
                    out_results[j].count = 0;
                }
                free(vectors); va_end(ap); return ERR_INVALID_MATRIX;
            }
            if (is_first || val > true_max) {
                true_max = val;
                is_first = 0;
            }
        }

        // 2. Считаем количество с учетом эпсилона
        int count = 0;
        for (int i = 0; i < num_vectors; i++) {
            double val = func(vectors[i], ctx);
            if (is_close(val, true_max, epsilon)) count++;
        }

        out_results[k].max_norm = true_max;
        out_results[k].count = count;
        out_results[k].max_vectors = (const Vector **)malloc((size_t)count * sizeof(const Vector *));
        
        if (!out_results[k].max_vectors) {
            for (int j = 0; j < k; j++) {
                free(out_results[j].max_vectors);
                out_results[j].max_vectors = NULL;
                out_results[j].count = 0;
            }
            free(vectors); va_end(ap); return ERR_MEMORY;
        }

        // 3. Заполняем (тут count уже точный)
        int idx = 0;
        for (int i = 0; i < num_vectors; i++) {
            double val = func(vectors[i], ctx);
            if (is_close(val, true_max, epsilon)) {
                out_results[k].max_vectors[idx++] = vectors[i];
            }
        }
    }

    va_end(ap);
    free(vectors);
    return OK;
}

int main(void) {
    int n = 2;
    double c1[] = {0.25e-6, 0.0};
    double c2[] = {1e-6, 0.0};
    double c3[] = {1.75e-6, 0.0};
    double c4[] = {1e308, 1e308};
    double c5[] = {3.0, 4.0}; 

    Vector v1 = {n, c1};
    Vector v2 = {n, c2};
    Vector v3 = {n, c3};
    Vector v4 = {n, c4};
    Vector v5 = {n, c5}; 

    double p = 2.0;
    double m_data_arr[] = {2.0, 0.0, 0.0, 2.0};
    double *m_data[] = {m_data_arr, m_data_arr + 2};
    Matrix A = {n, m_data};
    
    double m_bad_arr[] = {1.0, 0.0, 0.0, 1.0};
    double *m_bad_data[] = {m_bad_arr, m_bad_arr + 2};
    Matrix A_bad = {3, m_bad_data};

    double epsilon = 1e-6;

    printf("\n");
    {
        NormResult results[1] = {0};
        Status st = find_longest_vectors(results, epsilon, n, 3, &v1, &v2, &v3, 1, norm_inf, NULL);
        if (st == OK) {
            printf("Max norm: %e, Count: %d\n", results[0].max_norm, results[0].count);
            for (int i = 0; i < results[0].count; i++) {
                printf("  Vector: (%e, %e)\n", results[0].max_vectors[i]->coords[0], results[0].max_vectors[i]->coords[1]);
            }
            free(results[0].max_vectors);
        } else {
            printf("Error: %s\n", status_msg(st));
        }
    }

    printf("\n\n");
    {
        NormResult results[1] = {0};
        Status st = find_longest_vectors(results, epsilon, n, 1, &v4, 1, norm_p, &p);
        if (st == OK) {
            printf("Max norm: %e, Count: %d\n", results[0].max_norm, results[0].count);
            free(results[0].max_vectors);
        } else {
            printf("Error: %s\n", status_msg(st));
        }
    }

    printf("\n\n");
    {
        NormResult results[1] = {0};
        // Используем v5 (3, 4) и матрицу A (2*I). Норма должна быть sqrt(2*(3^2+4^2)) = sqrt(2*25) = sqrt(50) ≈ 7.071
        Status st = find_longest_vectors(results, epsilon, n, 1, &v5, 1, norm_A, &A);
        if (st == OK) {
            printf("Max norm: %f, Count: %d\n", results[0].max_norm, results[0].count);
            for (int i = 0; i < results[0].count; i++) {
                printf("  Vector: (%.1f, %.1f)\n", results[0].max_vectors[i]->coords[0], results[0].max_vectors[i]->coords[1]);
            }
            free(results[0].max_vectors);
        } else {
            printf("Error: %s\n", status_msg(st));
        }
    }

    printf("\n\n");
    {
        NormResult results[1] = {0};
        Status st = find_longest_vectors(results, epsilon, n, 1, &v5, 1, norm_A, &A_bad);
        if (st != OK) {
            printf("Expected error: %s\n", status_msg(st));
        } else {
            printf("Unexpected success\n");
            free(results[0].max_vectors);
        }
    }

    return 0;
}