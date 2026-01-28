
// parallel.c
// Build with: gcc -O2 -pthread parallel.c sequential.o -o sorter2
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

// ---- externs from sequential.c (no header file needed) ----
double now_ms(void);
void fill_random(int *a, size_t n, uint32_t seed);
int  is_sorted(const int *a, size_t n);
void mergesort_seq(int *a, size_t n);
void quicksort_seq(int *a, size_t n);

// ------------------ helpers ------------------
static int floor_log2(int x) {
    int d = 0;
    while (x > 1) { x >>= 1; d++; }
    return d;
}

static void die(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(EXIT_FAILURE);
}

static int parse_int(const char *s, const char *name) {
    char *end = NULL; errno = 0;
    long v = strtol(s, &end, 10);
    if (errno || !end || *end != '\0') { fprintf(stderr, "Invalid %s: %s\n", name, s); exit(1); }
    return (int)v;
}

static size_t parse_size(const char *s, const char *name) {
    char *end = NULL; errno = 0;
    unsigned long long v = strtoull(s, &end, 10);
    if (errno || !end || *end != '\0') { fprintf(stderr, "Invalid %s: %s\n", name, s); exit(1); }
    return (size_t)v;
}

/* =========================================================
   PARALLEL MERGE SORT (PTHREADS)
   ========================================================= */
static void mergesort_seq_range(int *a, int *tmp, int l, int r);
static void merge(int *a, int *tmp, int l, int m, int r);

static void mergesort_seq_range(int *a, int *tmp, int l, int r) {
    if (l >= r) return;
    int m = l + (r - l) / 2;
    mergesort_seq_range(a, tmp, l, m);
    mergesort_seq_range(a, tmp, m + 1, r);
    merge(a, tmp, l, m, r);
}

static void merge(int *a, int *tmp, int l, int m, int r) {
    int i = l, j = m + 1, k = l;
    while (i <= m && j <= r) {
        if (a[i] <= a[j]) tmp[k++] = a[i++];
        else             tmp[k++] = a[j++];
    }
    while (i <= m) tmp[k++] = a[i++];
    while (j <= r) tmp[k++] = a[j++];
    for (i = l; i <= r; i++) a[i] = tmp[i];
}

typedef struct {
    int *a;
    int *tmp;
    int l, r;
    int depth;
    int cutoff;
} merge_task_t;

static void mergesort_par_rec(int *a, int *tmp, int l, int r, int depth, int cutoff);

static void* mergesort_worker(void *arg) {
    merge_task_t *t = (merge_task_t*)arg;
    mergesort_par_rec(t->a, t->tmp, t->l, t->r, t->depth, t->cutoff);
    return NULL;
}

static void mergesort_par_rec(int *a, int *tmp, int l, int r, int depth, int cutoff) {
    if (l >= r) return;
    int len = r - l + 1;

    if (len <= cutoff || depth <= 0) {
        mergesort_seq_range(a, tmp, l, r);
        return;
    }

    int m = l + (r - l) / 2;

    pthread_t th;
    merge_task_t left = { a, tmp, l, m, depth - 1, cutoff };
    int rc = pthread_create(&th, NULL, mergesort_worker, &left);
    if (rc != 0) {
        // fallback
        mergesort_seq_range(a, tmp, l, m);
        mergesort_par_rec(a, tmp, m + 1, r, depth - 1, cutoff);
        merge(a, tmp, l, m, r);
        return;
    }

    mergesort_par_rec(a, tmp, m + 1, r, depth - 1, cutoff);
    pthread_join(th, NULL);
    merge(a, tmp, l, m, r);
}

void mergesort_par(int *a, size_t n, int threads, int cutoff) {
    if (n == 0) return;
    int *tmp = (int*)malloc(n * sizeof(int));
    if (!tmp) die("malloc failed (tmp)");
    int depth = floor_log2(threads);
    mergesort_par_rec(a, tmp, 0, (int)n - 1, depth, cutoff);
    free(tmp);
}

/* =========================================================
   PARALLEL QUICK SORT (PTHREADS)
   ========================================================= */
static inline void iswap(int *x, int *y) { int t=*x; *x=*y; *y=t; }

static int partition(int *a, int l, int r) {
    int mid = l + (r - l)/2;
    iswap(&a[mid], &a[r]);
    int pivot = a[r];
    int i = l - 1;
    for (int j = l; j < r; j++) {
        if (a[j] <= pivot) {
            i++;
            iswap(&a[i], &a[j]);
        }
    }
    iswap(&a[i+1], &a[r]);
    return i + 1;
}

static void quicksort_seq_rec(int *a, int l, int r) {
    while (l < r) {
        int p = partition(a, l, r);
        if (p - l < r - p) { quicksort_seq_rec(a, l, p - 1); l = p + 1; }
        else               { quicksort_seq_rec(a, p + 1, r); r = p - 1; }
    }
}

typedef struct {
    int *a;
    int l, r;
    int depth;
    int cutoff;
} quick_task_t;

static void quicksort_par_rec(int *a, int l, int r, int depth, int cutoff);

static void* quicksort_worker(void *arg) {
    quick_task_t *t = (quick_task_t*)arg;
    quicksort_par_rec(t->a, t->l, t->r, t->depth, t->cutoff);
    return NULL;
}

static void quicksort_par_rec(int *a, int l, int r, int depth, int cutoff) {
    if (l >= r) return;
    int len = r - l + 1;

    if (len <= cutoff || depth <= 0) {
        quicksort_seq_rec(a, l, r);
        return;
    }

    int p = partition(a, l, r);

    pthread_t th;
    quick_task_t left = { a, l, p - 1, depth - 1, cutoff };
    int rc = pthread_create(&th, NULL, quicksort_worker, &left);
    if (rc != 0) {
        quicksort_par_rec(a, l, p - 1, depth - 1, cutoff);
        quicksort_par_rec(a, p + 1, r, depth - 1, cutoff);
        return;
    }

    quicksort_par_rec(a, p + 1, r, depth - 1, cutoff);
    pthread_join(th, NULL);
}

void quicksort_par(int *a, size_t n, int threads, int cutoff) {
    if (n == 0) return;
    int depth = floor_log2(threads);
    quicksort_par_rec(a, 0, (int)n - 1, depth, cutoff);
}

/* =========================================================
   CLI BENCHMARK MAIN (outputs CSV rows)
   ========================================================= */
static void usage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s --algo merge|quick --mode seq|par --n N [--threads T] [--cutoff C] [--repeats K] [--seed S]\n\n"
        "Examples:\n"
        "  %s --algo merge --mode seq --n 1000000 --repeats 5\n"
        "  %s --algo merge --mode par --n 1000000 --threads 8 --cutoff 2048 --repeats 5\n",
        prog, prog, prog
    );
    exit(1);
}

int main(int argc, char **argv) {
    const char *algo = "merge";
    const char *mode = "seq";
    size_t n = 0;
    int threads = 4;
    int cutoff = 2048;
    int repeats = 5;
    uint32_t seed = 123;

    if (argc < 2) usage(argv[0]);

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--algo") && i + 1 < argc) algo = argv[++i];
        else if (!strcmp(argv[i], "--mode") && i + 1 < argc) mode = argv[++i];
        else if (!strcmp(argv[i], "--n") && i + 1 < argc) n = parse_size(argv[++i], "n");
        else if (!strcmp(argv[i], "--threads") && i + 1 < argc) threads = parse_int(argv[++i], "threads");
        else if (!strcmp(argv[i], "--cutoff") && i + 1 < argc) cutoff = parse_int(argv[++i], "cutoff");
        else if (!strcmp(argv[i], "--repeats") && i + 1 < argc) repeats = parse_int(argv[++i], "repeats");
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = (uint32_t)parse_int(argv[++i], "seed");
        else usage(argv[0]);
    }

    if (n == 0) die("n must be > 0");
    if (!strcmp(mode, "par") && threads < 1) die("threads must be >= 1");
    if (cutoff < 16) cutoff = 16;

    int *base = (int*)malloc(n * sizeof(int));
    int *work = (int*)malloc(n * sizeof(int));
    if (!base || !work) die("malloc failed (arrays)");

    fill_random(base, n, seed);

    printf("algo,mode,n,threads,cutoff,repeats,avg_ms,sorted_ok\n");

    double total = 0.0;
    int ok = 1;

    for (int r = 0; r < repeats; r++) {
        memcpy(work, base, n * sizeof(int));
        double t0 = now_ms();

        int is_merge = !strcmp(algo, "merge");
        int is_quick = !strcmp(algo, "quick");
        int is_seq   = !strcmp(mode, "seq");
        int is_par   = !strcmp(mode, "par");

        if (!is_merge && !is_quick) die("algo must be merge or quick");
        if (!is_seq && !is_par) die("mode must be seq or par");

        if (is_merge) {
            if (is_seq) mergesort_seq(work, n);
            else mergesort_par(work, n, threads, cutoff);
        } else {
            if (is_seq) quicksort_seq(work, n);
            else quicksort_par(work, n, threads, cutoff);
        }

        double t1 = now_ms();
        total += (t1 - t0);

        if (!is_sorted(work, n)) ok = 0;
    }

    printf("%s,%s,%zu,%d,%d,%d,%.3f,%d\n",
           algo, mode, n, (!strcmp(mode, "par") ? threads : 1),
           cutoff, repeats, total / repeats, ok);

    free(base);
    free(work);
    return ok ? 0 : 2;
}
