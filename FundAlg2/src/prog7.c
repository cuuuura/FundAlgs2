#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

typedef enum {
    OK = 0,
    ERR_ARGS,
    ERR_NULL,
    ERR_MEMORY,
    ERR_INVALID_VECTOR
} status_t;

static const char *status_msg(status_t s) {
    switch (s) {
    case OK: return "ok";
    case ERR_ARGS: return "invalid arguments";
    case ERR_NULL: return "null pointer";
    case ERR_MEMORY: return "memory allocation failed";
    case ERR_INVALID_VECTOR: return "invalid vector dimensions";
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

static double norm_inf(const Vector *v, const void *ctx) {
    (void)ctx;
    double max_val = 0.0;
    for (int i = 0; i < v->n; i++) {
        double abs_val = fabs(v->coords[i]);
        if (abs_val > max_val) max_val = abs_val;
    }
    return max_val;
}

static double norm_p(const Vector *v, const void *ctx) {
    double p = *(const double *)ctx;
    if (p < 1.0) return 0.0; 
    double sum = 0.0;
    for (int i = 0; i < v->n; i++) {
        sum += pow(fabs(v->coords[i]), p);
    }
    return pow(sum, 1.0 / p);
}

static double norm_A(const Vector *v, const void *ctx) {
    const Matrix *A = (const Matrix *)ctx;
    
    if (A == NULL || A->n != v->n) {
        return -1.0; 
    }

    double sum = 0.0;
    for (int i = 0; i < v->n; i++) {
        double Ax_i = 0.0;
        for (int j = 0; j < v->n; j++) {
            Ax_i += A->data[i][j] * v->coords[j];
        }
        sum += Ax_i * v->coords[i];
    }
    return sqrt(sum > 0.0 ? sum : 0.0);
}

static status_t find_longest_vectors(NormResult *out_results, double epsilon, int n, int num_vectors, ...) {
    if (!out_results || n <= 0 || num_vectors <= 0) return ERR_ARGS;

    va_list ap;
    va_start(ap, num_vectors);

    const Vector **vectors = (const Vector **)malloc((size_t)num_vectors * sizeof(const Vector *));
    if (!vectors) {
        va_end(ap);
        return ERR_MEMORY;
    }

    for (int i = 0; i < num_vectors; i++) {
        vectors[i] = va_arg(ap, const Vector *);
        if (!vectors[i]) {
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

        for (int i = 0; i < num_vectors; i++) {
            double val = func(vectors[i], ctx);
            if (val > max_val + epsilon) {
                max_val = val;
                count = 1;
            } else if (fabs(val - max_val) <= epsilon) {
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
            if (fabs(val - max_val) <= epsilon) { 
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
    double c3[] = {3.0, -4.0};
    double c4[] = {1.0, 1.0};
    
    Vector v1 = {n, c1};
    Vector v2 = {n, c2};
    Vector v3 = {n, c3};
    Vector v4 = {n, c4};

    double p = 2.0;
    double m_data_arr[] = {2.0, 0.0, 0.0, 2.0};
    double *m_data[] = {m_data_arr, m_data_arr + 2};
    Matrix A = {n, m_data};

    double epsilon = 1e-9;

    {
        printf("\n");
        int num_norms = 3;
        NormResult results[3] = {0}; 
        
        status_t st = find_longest_vectors(results, epsilon, n, 3, &v1, &v2, &v4, num_norms, norm_inf, NULL, norm_p, &p, norm_A, &A);

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
        
        status_t st = find_longest_vectors(results, epsilon, n, 2, &v1, &v3, num_norms, norm_inf, NULL);

        if (st != OK) {
            printf("Error: %s\n", status_msg(st));
        } else {
            printf("Norm Inf: Max value = %f, Count = %d\n", results[0].max_norm, results[0].count);
            for (int j = 0; j < results[0].count; j++) {
                const Vector *v = results[0].max_vectors[j];
                printf("  Vector: (%.1f, %.1f)\n", v->coords[0], v->coords[1]);
            }
            free(results[0].max_vectors);
        }
    }
    printf("\n\n");

    {
        int num_norms = 1;
        NormResult results[1] = {0};
        
        status_t st = find_longest_vectors(results, epsilon, n, -1, num_norms, norm_inf, NULL);
        if (st != OK) {
            printf("error: %s\n", status_msg(st));
        }
    }

    return 0;
}