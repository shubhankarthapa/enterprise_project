
// sequential.c
// Build with: gcc -O2 -c sequential.c -o sequential.o
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

void fill_random(int *a, size_t n, uint32_t seed) {
    uint32_t x = seed ? seed : 123456789u;
    for (size_t i = 0; i < n; i++) {
        x = 1664525u * x + 1013904223u;
        a[i] = (int)(x & 0x7fffffff);
    }
}

int is_sorted(const int *a, size_t n) {
    for (size_t i = 1; i < n; i++) {
        if (a[i-1] > a[i]) return 0;
    }
    return 1;
}

/* =========================
   SEQUENTIAL MERGE SORT
   ========================= */
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

static void mergesort_seq_rec(int *a, int *tmp, int l, int r) {
    if (l >= r) return;
    int m = l + (r - l) / 2;
    mergesort_seq_rec(a, tmp, l, m);
    mergesort_seq_rec(a, tmp, m + 1, r);
    merge(a, tmp, l, m, r);
}

void mergesort_seq(int *a, size_t n) {
    if (n == 0) return;
    int *tmp = (int*)malloc(n * sizeof(int));
    if (!tmp) { perror("malloc tmp"); exit(1); }
    mergesort_seq_rec(a, tmp, 0, (int)n - 1);
    free(tmp);
}

/* =========================
   SEQUENTIAL QUICK SORT
   ========================= */
static inline void iswap(int *x, int *y) {
    int t = *x; *x = *y; *y = t;
}

static int partition(int *a, int l, int r) {
    int mid = l + (r - l) / 2;
    iswap(&a[mid], &a[r]);
    int pivot = a[r];

    int i = l - 1;
    for (int j = l; j < r; j++) {
        if (a[j] <= pivot) {
            i++;
            iswap(&a[i], &a[j]);
        }
    }
    iswap(&a[i + 1], &a[r]);
    return i + 1;
}

static void quicksort_seq_rec(int *a, int l, int r) {
    while (l < r) {
        int p = partition(a, l, r);
        if (p - l < r - p) {
            quicksort_seq_rec(a, l, p - 1);
            l = p + 1;
        } else {
            quicksort_seq_rec(a, p + 1, r);
            r = p - 1;
        }
    }
}

void quicksort_seq(int *a, size_t n) {
    if (n == 0) return;
    quicksort_seq_rec(a, 0, (int)n - 1);
}
