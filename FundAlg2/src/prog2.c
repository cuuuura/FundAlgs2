#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>

#define EPS 1e-9

typedef enum {
    SUCCESS = 0,
    ERR_INVALID_ARGUMENTS,
    ERR_INVALID_FLAG,
    ERR_MEMORY_ALLOCATION,
    ERR_FILE_OPEN,
    ERR_FILE_READ,
    ERR_FILE_WRITE,
    ERR_INVALID_FORMAT
} status_t;

static const char *status_msg(status_t s) {
    switch (s) {
    case SUCCESS: return "success";
    case ERR_INVALID_ARGUMENTS: return "invalid arguments";
    case ERR_INVALID_FLAG: return "invalid flag (use -a, -d, /a or /d)";
    case ERR_MEMORY_ALLOCATION: return "memory allocation failed";
    case ERR_FILE_OPEN: return "cannot open file";
    case ERR_FILE_READ: return "file read error";
    case ERR_FILE_WRITE: return "file write error";
    case ERR_INVALID_FORMAT: return "invalid input format";
    default: return "unknown error";
    }
}

typedef struct {
    unsigned int id;
    char *name;
    char *surname;
    double salary;
} Employee;

static int compare_double(double a, double b, double eps) {
    if (a - b > eps) return 1;
    if (b - a > eps) return -1;
    return 0;
}

static int is_latin_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// парсинг данных
static status_t parse_flag(const char *flag, int *out_ascending) {
    if (flag == NULL || out_ascending == NULL) return ERR_INVALID_ARGUMENTS;
    if (flag[0] != '-' && flag[0] != '/') return ERR_INVALID_FLAG;
    if (flag[1] == '\0' || flag[2] != '\0') return ERR_INVALID_FLAG;
    if (flag[1] == 'a') { *out_ascending = 1; return SUCCESS; }
    if (flag[1] == 'd') { *out_ascending = 0; return SUCCESS; }
    return ERR_INVALID_FLAG;
}

static status_t read_line(FILE *f, char **out_line) {
    if (!f || !out_line) return ERR_INVALID_ARGUMENTS;
    
    size_t cap = 64, len = 0;
    char *buf = (char *)malloc(cap*sizeof(char));
    if (!buf) return ERR_MEMORY_ALLOCATION;

    int c;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            char *tmp = (char *)realloc(buf, cap);
            if (!tmp) { free(buf); return ERR_MEMORY_ALLOCATION; }
            buf = tmp;
        }
        buf[len++] = (char)c;
    }

    if (ferror(f)) {
        free(buf);
        return ERR_FILE_READ;
    }
    
    if (c == EOF && len == 0) {
        free(buf);
        *out_line = NULL; 
        return SUCCESS;
    }
    
    if (len > 0 && buf[len - 1] == '\r') len--;
    buf[len] = '\0';
    *out_line = buf;
    return SUCCESS;
}

static status_t parse_employee_line(const char *line, Employee *out) {
    if (!line || !out) return ERR_INVALID_ARGUMENTS;
    const char *p = line;

    while (*p == ' ' || *p == '\t') p++;

    // id
    if (*p < '0' || *p > '9') return ERR_INVALID_FORMAT;
    unsigned long long id_val = 0;
    while (*p >= '0' && *p <= '9') {
        id_val = id_val * 10ULL + (unsigned long long)(*p - '0');
        if (id_val > (unsigned long long)UINT_MAX) return ERR_INVALID_FORMAT;
        p++;
    }
    if (*p != ' ' && *p != '\t') return ERR_INVALID_FORMAT;
    out->id = (unsigned int)id_val;

    while (*p == ' ' || *p == '\t') p++;

    // name
    const char *name_start = p;
    while (is_latin_letter(*p)) p++;
    size_t name_len = (size_t)(p - name_start);
    if (name_len == 0) return ERR_INVALID_FORMAT;
    if (*p != ' ' && *p != '\t') return ERR_INVALID_FORMAT;

    out->name = (char *)malloc((name_len + 1)*sizeof(char));
    if (!out->name) return ERR_MEMORY_ALLOCATION;
    memcpy(out->name, name_start, name_len);
    out->name[name_len] = '\0';

    while (*p == ' ' || *p == '\t') p++;

    // surname
    const char *surname_start = p;
    while (is_latin_letter(*p)) p++;
    size_t surname_len = (size_t)(p - surname_start);
    if (surname_len == 0) {
        free(out->name);
        return ERR_INVALID_FORMAT;
    }
    if (*p != ' ' && *p != '\t') {
        free(out->name);
        return ERR_INVALID_FORMAT;
    }

    out->surname = (char *)malloc(surname_len + 1);
    if (!out->surname) {
        free(out->name);
        return ERR_MEMORY_ALLOCATION;
    }
    memcpy(out->surname, surname_start, surname_len);
    out->surname[surname_len] = '\0';

    while (*p == ' ' || *p == '\t') p++;

    char *endptr = NULL;
    double salary = strtod(p, &endptr);
    
    if (endptr == p || !isfinite(salary) || salary < 0.0) {
        free(out->name); free(out->surname);
        return ERR_INVALID_FORMAT;
    }
    
    p = endptr;
    while (*p == ' ' || *p == '\t' || *p == '\r') p++;
    
    if (*p != '\0') {
        free(out->name); free(out->surname);
        return ERR_INVALID_FORMAT;
    }

    out->salary = salary;
    return SUCCESS;
}

static void free_employees(Employee *arr, size_t n) {
    if (!arr) return;
    for (size_t i = 0; i < n; i++) {
        free(arr[i].name);
        free(arr[i].surname);
    }
    free(arr);
}

static status_t load_employees(const char *path, Employee **out_arr, size_t *out_count) {
    if (!path || !out_arr || !out_count) return ERR_INVALID_ARGUMENTS;

    FILE *f = fopen(path, "r");
    if (!f) return ERR_FILE_OPEN;

    size_t cap = 4, size = 0;
    Employee *arr = (Employee *)malloc(cap * sizeof(Employee));
    if (!arr) { 
        fclose(f); 
        return ERR_MEMORY_ALLOCATION; 
    }

    char *line = NULL;
    status_t st = SUCCESS;

    for (;;) {
        st = read_line(f, &line);
        if (st != SUCCESS) break;
        if (line == NULL) break;

        int empty = 1;
        for (const char *q = line; *q != '\0'; q++) {
            if (*q != ' ' && *q != '\t' && *q != '\r') { empty = 0; break; }
        }
        if (empty) { 
            free(line); 
            line = NULL;
            continue; 
        }

        if (size == cap) {
            size_t new_cap = cap * 2;
            Employee *tmp = (Employee *)realloc(arr, new_cap * sizeof(Employee));
            if (!tmp) { 
                st = ERR_MEMORY_ALLOCATION; 
                break; 
            }
            arr = tmp;
            cap = new_cap;
        }

        Employee emp = { .id = 0, .name = NULL, .surname = NULL, .salary = 0.0 };;
        st = parse_employee_line(line, &emp);
        free(line);
        line = NULL;
        
        if (st != SUCCESS) break;
        arr[size++] = emp;
    }

    if (line) free(line);
    if (ferror(f)) {
        st = ERR_FILE_READ;
    }

    fclose(f);

    if (st != SUCCESS) {
        free_employees(arr, size);
        return st;
    }
    
    *out_arr = arr;
    *out_count = size;
    return SUCCESS;
}

static status_t write_employees(const char *path, const Employee *arr, size_t count) {
    if (!path || !arr) return ERR_INVALID_ARGUMENTS;

    FILE *f = fopen(path, "w");
    if (!f) return ERR_FILE_OPEN;

    status_t st = SUCCESS;
    for (size_t i = 0; i < count; i++) {
        if (fprintf(f, "%u %s %s %.10g\n", arr[i].id, arr[i].name, arr[i].surname, arr[i].salary) < 0) {
            st = ERR_FILE_WRITE;
            break;
        }
    }
    if (fclose(f) != 0 && st == SUCCESS) st = ERR_FILE_WRITE;
    return st;
}

static int compare_employees_asc(const void *a, const void *b) {
    const Employee *e1 = (const Employee *)a;
    const Employee *e2 = (const Employee *)b;

    int res = compare_double(e1->salary, e2->salary, EPS);
    if (res != 0) return res;

    res = strcmp(e1->surname, e2->surname);
    if (res != 0) return res;

    res = strcmp(e1->name, e2->name);
    if (res != 0) return res;

    if (e1->id < e2->id) return -1;
    if (e1->id > e2->id) return 1;
    return 0;
}

static int compare_employees_desc(const void *a, const void *b) {
    return compare_employees_asc(b, a);
}

static int is_same_file(const char *path1, const char *path2) {
    struct stat st1, st2;

    if (stat(path1, &st1) != 0) {
        return 0;
    }
    
    if (stat(path2, &st2) != 0) {
        return 0;
    }
    
    return (st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <input_file> <-a|/a|-d|/d> <output_file>\n", argv[0]);
        return 1;
    }

    if (is_same_file(argv[1], argv[3])) {
        fprintf(stderr, "Error: input and output files must be different\n");
        return 1;
    }

    int ascending = 1;
    status_t st = parse_flag(argv[2], &ascending);
    if (st != SUCCESS) {
        fprintf(stderr, "Error: %s\n", status_msg(st));
        return 1;
    }

    Employee *arr = NULL;
    size_t count = 0;
    st = load_employees(argv[1], &arr, &count);
    if (st != SUCCESS) {
        fprintf(stderr, "Error: %s\n", status_msg(st));
        return 1;
    }

    if (count > 1) {
        qsort(arr, count, sizeof(Employee), ascending ? compare_employees_asc : compare_employees_desc);
    }

    st = write_employees(argv[3], arr, count);
    free_employees(arr, count);

    if (st != SUCCESS) {
        fprintf(stderr, "Error: %s\n", status_msg(st));
        return 1;
    }
    return 0;
}