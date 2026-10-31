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

    double sum = 0.0;
    for (int i = 0; i < v->n; i++) {
        sum += pow(fabs(v->coords[i]), p);
    }
    
    if (isinf(sum)) return INFINITY;
    
    return pow(sum, 1.0 / p);
}

static double norm_A(const Vector *v, const void *ctx) {
    if (!v || !v->coords || !ctx) return -1.0;
    const Matrix *A = (const Matrix *)ctx;
    
    if (A->n != v->n || !A->data) return -1.0;

    double sum = 0.0;
    for (int i = 0; i < v->n; i++) {
        if (!A->data[i]) return -1.0;

        double Ax_i = 0.0;
        for (int j = 0; j < v->n; j++) {
            Ax_i += A->data[i][j] * v->coords[j];
        }
        sum += Ax_i * v->coords[i];
    }
    
    if (sum < 0.0) {
        if (sum > -1e-9) sum = 0.0;
        else return -1.0;
    }
    
    if (isinf(sum)) return INFINITY;
    return sqrt(sum);
}

static Status find_longest_vectors(NormResult *out_results, double epsilon, int n, int num_vectors, ...) {
    if (!out_results || n <= 0 || num_vectors <= 0 || epsilon < 0.0) return ERR_ARGS;

    va_list ap;
    va_start(ap, num_vectors);

    const Vector **vectors = (const Vector **)malloc((size_t)num_vectors * sizeof(const Vector *));
    if (!vectors) {
        va_end(ap);
        return ERR_MEMORY;
    }

    for (int i = 0; i < num_vectors; i++) {
        vectors[i] = va_arg(ap, const Vector *);
        if (!vectors[i] || !vectors[i]->coords) {
            free(vectors);
            va_end(ap);
            return ERR_NULL;
        }
        if (vectors[i]->n != n) {
            free(vectors);
            va_end(ap);
            return ERR_INVALID_VECTOR;
        }
    }

    int num_norms = va_arg(ap, int);
    if (num_norms <= 0) {
        free(vectors);
        va_end(ap);
        return ERR_ARGS;
    }

    for (int k = 0; k < num_norms; k++) {
        NormFunc func = va_arg(ap, NormFunc);
        void *ctx = va_arg(ap, void *);

        if (!func) {
            for (int j = 0; j < k; j++) {
                free(out_results[j].max_vectors);
                out_results[j].max_vectors = NULL;
                out_results[j].count = 0;
            }
            free(vectors);
            va_end(ap);
            return ERR_NULL;
        }

        double max_val = -1.0;
        int count = 0;
        int is_first = 1;

        for (int i = 0; i < num_vectors; i++) {
            double val = func(vectors[i], ctx);
            if (val < 0.0) { // Ошибка вычисления нормы
                for (int j = 0; j < k; j++) {
                    free(out_results[j].max_vectors);
                    out_results[j].max_vectors = NULL;
                    out_results[j].count = 0;
                }
                free(vectors);
                va_end(ap);
                return ERR_INVALID_MATRIX;
            }

            if (is_first) {
                max_val = val;
                count = 1;
                is_first = 0;
            } else if (val > max_val + epsilon) {
                max_val = val;
                count = 1;
            } else if (is_close(val, max_val, epsilon)) {
                count++;
            }
        }

        out_results[k].max_norm = max_val;
        out_results[k].count = count;
        out_results[k].max_vectors = (const Vector **)malloc((size_t)count * sizeof(const Vector *));
        
        if (!out_results[k].max_vectors) {
            for (int j = 0; j < k; j++) {
                free(out_results[j].max_vectors);
                out_results[j].max_vectors = NULL;
                out_results[j].count = 0;
            }
            free(vectors);
            va_end(ap);
            return ERR_MEMORY;
        }

        int idx = 0;
        for (int i = 0; i < num_vectors; i++) {
            double val = func(vectors[i], ctx);
            if (is_close(val, max_val, epsilon)) { 
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
    double c1[] = {3.0, 4.0};
    double c2[] = {-4.0, 3.0};
    double c4[] = {1.0, 1.0};
    double c5[] = {1e308, 1e308};
    
    Vector v1 = {n, c1};
    Vector v2 = {n, c2};
    Vector v4 = {n, c4};
    Vector v5 = {n, c5};

    double p = 2.0;
    double m_data_arr[] = {2.0, 0.0, 0.0, 2.0};
    double *m_data[] = {m_data_arr, m_data_arr + 2};
    Matrix A = {n, m_data};

    double m_bad_arr[] = {1.0, 0.0, 0.0, 1.0};
    double *m_bad_data[] = {m_bad_arr, m_bad_arr + 2};
    Matrix A_bad = {3, m_bad_data};

    double epsilon = 1e-9;

    printf("\n");
    {
        int num_norms = 3;
        NormResult results[3] = {0};
        
        Status st = find_longest_vectors(results, epsilon, n, 3, &v1, &v2, &v4, num_norms, norm_inf, NULL, norm_p, &p, norm_A, &A);

        if (st != OK) {
            printf("Error: %s\n", status_msg(st));
        } else {
            for (int i = 0; i < num_norms; i++) {
                printf("Norm %d: Max value = %f, Count = %d\n", i + 1, results[i].max_norm, results[i].count);
                for (int j = 0; j < results[i].count; j++) {
                    const Vector *v = results[i].max_vectors[j];
                    printf("  Vector: (%.1f, %.1f)\n", v->coords[0], v->coords[1]);
                }
                free(results[i].max_vectors);
            }
        }
    }

    printf("\n\n");
    {
        int num_norms = 2;
        NormResult results[2] = {0};
        
        Status st = find_longest_vectors(results, epsilon, n, 2, &v5, &v5, num_norms, norm_inf, NULL, norm_p, &p);

        if (st != OK) {
            printf("Error: %s\n", status_msg(st));
        } else {
            for (int i = 0; i < num_norms; i++) {
                printf("Norm %d: Max value = %f, Count = %d\n", i + 1, results[i].max_norm, results[i].count);
                for (int j = 0; j < results[i].count; j++) {
                    const Vector *v = results[i].max_vectors[j];
                    printf("  Vector: (%.1f, %.1f)\n", v->coords[0], v->coords[1]);
                }
                free(results[i].max_vectors);
            }
        }
    }

    printf("\n\n");
    {
        int num_norms = 1;
        NormResult results[1] = {0};
        
        Status st = find_longest_vectors(results, epsilon, n, 1, &v1, num_norms, norm_A, &A_bad);
        if (st != OK) {
            printf("Expected error: %s\n", status_msg(st));
        } else {
            printf("Unexpected success\n");
            free(results[0].max_vectors);
        }
    }

    printf("\n\n");
    {
        int num_norms = 1;
        NormResult results[1] = {0};
        
        Status st = find_longest_vectors(results, epsilon, n, -1, num_norms, norm_inf, NULL);
        if (st != OK) {
            printf("Expected error: %s\n", status_msg(st));
        }
    }

    return 0;
}