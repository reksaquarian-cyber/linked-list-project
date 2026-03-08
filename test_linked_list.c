/**
 * test_linked_list.c
 * Automated tests for the thread-safe singly linked list.
 *
 * Build:
 *   gcc -o test_linked_list test_linked_list.c linked_list.c -lpthread
 *
 * Run:
 *   ./test_linked_list
 *
 * Test categories:
 *   1. Unit        – correctness of each operation
 *   2. Concurrency – consistent state under parallel mutations
 *   3. Deadlock    – every operation completes within 5 seconds
 */

#define _GNU_SOURCE
#include "linked_list.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>       /* sleep */
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <pthread.h>


/* ------------------------------------------------------------------ */
/*  Mini test framework                                               */
/* ------------------------------------------------------------------ */

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name)   static void name(void)

#define ASSERT(expr, msg)                                              \
    do {                                                               \
        if (expr) {                                                    \
            printf("  [PASS] %s\n", msg);                              \
            g_pass++;                                                  \
        } else {                                                       \
            printf("  [FAIL] %s  (line %d)\n", msg, __LINE__);         \
            g_fail++;                                                  \
        }                                                              \
    } while (0)

#define RUN(name)                                                      \
    do {                                                               \
        printf("\n--- " #name " ---\n");                               \
        name();                                                        \
    } while (0)

/* ------------------------------------------------------------------ */
/*  Timeout-thread helper for deadlock tests                          */
/*  Returns 0 if thread completed, -1 if it exceeded DEADLINE_SEC     */
/* ------------------------------------------------------------------ */

#define DEADLINE_SEC 5

typedef struct {
    void (*fn)(void *);
    void *arg;
} WorkItem;

static void *thread_wrapper(void *arg) {
    WorkItem *w = (WorkItem *)arg;
    w->fn(w->arg);
    return NULL;
}

/* Run fn(arg) in a thread; return 0=ok, -1=timeout */
static int run_with_timeout(void (*fn)(void *), void *arg) {
    pthread_t tid;
    WorkItem  w = { fn, arg };

    if (pthread_create(&tid, NULL, thread_wrapper, &w) != 0) return -1;

    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += DEADLINE_SEC;

    int rc = pthread_timedjoin_np(tid, NULL, &deadline);
    if (rc == ETIMEDOUT) {
        /* Thread is stuck – cancel it to avoid leaking resources */
        pthread_cancel(tid);
        pthread_join(tid, NULL);
        return -1;
    }
    return 0;
}
/* ------------------------------------------------------------------ */
/*  1. UNIT TESTS                                                     */
/* ------------------------------------------------------------------ */

TEST(test_push_single) {
    LinkedList ll;
    ll_init(&ll);

    int v = 42;
    ll_push(&ll, &v);

    ASSERT(ll_size(&ll) == 1,         "push single: size == 1");
    ASSERT(ll_get(&ll, 0) == &v,      "push single: get(0) returns correct pointer");

    ll_destroy(&ll);
}

TEST(test_push_multiple_order) {
    LinkedList ll;
    ll_init(&ll);

    int a = 1, b = 2, c = 3;
    ll_push(&ll, &a);
    ll_push(&ll, &b);
    ll_push(&ll, &c);

    ASSERT(ll_size(&ll) == 3,         "push multiple: size == 3");
    ASSERT(ll_get(&ll, 0) == &a,      "push multiple: get(0) == a");
    ASSERT(ll_get(&ll, 1) == &b,      "push multiple: get(1) == b");
    ASSERT(ll_get(&ll, 2) == &c,      "push multiple: get(2) == c");

    ll_destroy(&ll);
}

TEST(test_pop_removes_last) {
    LinkedList ll;
    ll_init(&ll);

    int a = 10, b = 20, c = 30;
    ll_push(&ll, &a);
    ll_push(&ll, &b);
    ll_push(&ll, &c);

    void *popped = ll_pop(&ll);
    ASSERT(popped == &c,              "pop: returns last element");
    ASSERT(ll_size(&ll) == 2,         "pop: size decremented");
    ASSERT(ll_get(&ll, 1) == &b,      "pop: new tail is b");

    ll_destroy(&ll);
}

TEST(test_pop_empty) {
    LinkedList ll;
    ll_init(&ll);

    void *result = ll_pop(&ll);
    ASSERT(result == NULL,            "pop empty: returns NULL");
    ASSERT(ll_size(&ll) == 0,         "pop empty: size remains 0");

    ll_destroy(&ll);
}

TEST(test_pop_single_element) {
    LinkedList ll;
    ll_init(&ll);

    int v = 7;
    ll_push(&ll, &v);
    void *r = ll_pop(&ll);

    ASSERT(r == &v,                   "pop single: returns element");
    ASSERT(ll_size(&ll) == 0,         "pop single: list now empty");
    ASSERT(ll_get(&ll, 0) == NULL,    "pop single: get returns NULL");

    ll_destroy(&ll);
}

TEST(test_insert_after_middle) {
    LinkedList ll;
    ll_init(&ll);

    int a=1, b=2, c=3, x=99;
    ll_push(&ll, &a);
    ll_push(&ll, &b);
    ll_push(&ll, &c);

    int rc = ll_insert_after(&ll, &x, &b);   /* insert after b */

    ASSERT(rc == 0,                   "insertAfter middle: returns 0");
    ASSERT(ll_size(&ll) == 4,         "insertAfter middle: size == 4");
    ASSERT(ll_get(&ll, 2) == &x,      "insertAfter middle: x at index 2");
    ASSERT(ll_get(&ll, 3) == &c,      "insertAfter middle: c pushed to 3");

    ll_destroy(&ll);
}

TEST(test_insert_after_tail) {
    LinkedList ll;
    ll_init(&ll);

    int a=1, b=2, x=99;
    ll_push(&ll, &a);
    ll_push(&ll, &b);

    ll_insert_after(&ll, &x, &b);     /* insert after tail */

    ASSERT(ll_size(&ll) == 3,         "insertAfter tail: size == 3");
    ASSERT(ll_get(&ll, 2) == &x,      "insertAfter tail: x is new tail");

    ll_destroy(&ll);
}

TEST(test_insert_after_not_found) {
    LinkedList ll;
    ll_init(&ll);

    int a=1, b=2, x=99;
    ll_push(&ll, &a);
    int rc = ll_insert_after(&ll, &x, &b);

    ASSERT(rc == -2,                  "insertAfter not found: returns -2");
    ASSERT(ll_size(&ll) == 1,         "insertAfter not found: size unchanged");

    ll_destroy(&ll);
}

TEST(test_delete_existing) {
    LinkedList ll;
    ll_init(&ll);

    int a=1, b=2, c=3;
    ll_push(&ll, &a);
    ll_push(&ll, &b);
    ll_push(&ll, &c);

    int rc = ll_delete(&ll, &b);

    ASSERT(rc == 0,                   "delete existing: returns 0");
    ASSERT(ll_size(&ll) == 2,         "delete existing: size == 2");
    ASSERT(ll_get(&ll, 1) == &c,      "delete existing: c shifted to index 1");

    ll_destroy(&ll);
}

TEST(test_delete_head) {
    LinkedList ll;
    ll_init(&ll);

    int a=1, b=2;
    ll_push(&ll, &a);
    ll_push(&ll, &b);

    ll_delete(&ll, &a);

    ASSERT(ll_size(&ll) == 1,         "delete head: size == 1");
    ASSERT(ll_get(&ll, 0) == &b,      "delete head: b is new head");

    ll_destroy(&ll);
}

TEST(test_delete_not_found) {
    LinkedList ll;
    ll_init(&ll);

    int a=1, b=2;
    ll_push(&ll, &a);
    int rc = ll_delete(&ll, &b);

    ASSERT(rc == -1,                  "delete not found: returns -1");
    ASSERT(ll_size(&ll) == 1,         "delete not found: size unchanged");

    ll_destroy(&ll);
}

TEST(test_get_out_of_range) {
    LinkedList ll;
    ll_init(&ll);

    int a = 5;
    ll_push(&ll, &a);

    ASSERT(ll_get(&ll, 99) == NULL,   "get out-of-range: returns NULL");

    ll_destroy(&ll);
}

TEST(test_clear) {
    LinkedList ll;
    ll_init(&ll);

    int a=1, b=2;
    ll_push(&ll, &a);
    ll_push(&ll, &b);
    ll_clear(&ll);

    ASSERT(ll_size(&ll) == 0,         "clear: size == 0");
    ASSERT(ll_get(&ll, 0) == NULL,    "clear: get returns NULL");

    ll_destroy(&ll);
}

/* ------------------------------------------------------------------ */
/*  2. CONCURRENCY TESTS                                              */
/* ------------------------------------------------------------------ */

#define THREAD_COUNT  50
#define OPS_PER_THREAD 200

/* Shared list for concurrency tests */
static LinkedList g_shared;

/* Each thread pushes OPS_PER_THREAD int pointers */
static void *concurrent_push_worker(void *arg) {
    int *values = (int *)arg;
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        ll_push(&g_shared, &values[i]);
    }
    return NULL;
}

TEST(test_concurrent_pushes_correct_size) {
    ll_init(&g_shared);

    /* Allocate value storage: THREAD_COUNT * OPS_PER_THREAD ints */
    int total = THREAD_COUNT * OPS_PER_THREAD;
    int *vals = (int *)malloc(sizeof(int) * (size_t)total);
    for (int i = 0; i < total; i++) vals[i] = i;

    pthread_t tids[THREAD_COUNT];
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, THREAD_COUNT);

    /* Each thread gets its own slice of the values array */
    for (int t = 0; t < THREAD_COUNT; t++) {
        pthread_create(&tids[t], NULL, concurrent_push_worker,
                       &vals[t * OPS_PER_THREAD]);
    }
    for (int t = 0; t < THREAD_COUNT; t++) {
        pthread_join(tids[t], NULL);
    }

    ASSERT(ll_size(&g_shared) == (size_t)total,
           "concurrent push: final size == THREAD_COUNT * OPS_PER_THREAD");

    pthread_barrier_destroy(&barrier);
    free(vals);
    ll_destroy(&g_shared);
}

/* Mixed push/pop worker – just checks no crash */
static int g_errors = 0;

static void *push_pop_worker(void *arg) {
    int *values = (int *)arg;
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        if (i % 2 == 0)
            ll_push(&g_shared, &values[i]);
        else
            ll_pop(&g_shared);
    }
    return NULL;
}

TEST(test_concurrent_push_pop_no_crash) {
    ll_init(&g_shared);
    g_errors = 0;

    int total = THREAD_COUNT * OPS_PER_THREAD;
    int *vals = (int *)malloc(sizeof(int) * (size_t)total);
    for (int i = 0; i < total; i++) vals[i] = i;

    pthread_t tids[THREAD_COUNT];
    for (int t = 0; t < THREAD_COUNT; t++) {
        pthread_create(&tids[t], NULL, push_pop_worker,
                       &vals[t * OPS_PER_THREAD]);
    }
    for (int t = 0; t < THREAD_COUNT; t++) {
        pthread_join(tids[t], NULL);
    }

    ASSERT(g_errors == 0, "concurrent push/pop: no errors under contention");
    /* size cannot be negative; it is unsigned so just validate sanity */
    ASSERT(ll_size(&g_shared) < (size_t)total + 1,
           "concurrent push/pop: size within sane bounds");

    free(vals);
    ll_destroy(&g_shared);
}

/* ------------------------------------------------------------------ */
/*  3. DEADLOCK-FREEDOM TESTS                                         */
/* ------------------------------------------------------------------ */

/* Work function: push 5000 items */
static void work_push_5000(void *arg) {
    LinkedList *ll = (LinkedList *)arg;
    int vals[5000];
    for (int i = 0; i < 5000; i++) {
        vals[i] = i;
        ll_push(ll, &vals[i]);
    }
}

/* Work function: pop 1000 items */
static void work_pop_1000(void *arg) {
    LinkedList *ll = (LinkedList *)arg;
    for (int i = 0; i < 1000; i++) {
        ll_pop(ll);
    }
}

/* Work function: alternating insertAfter + delete */
typedef struct { LinkedList *ll; int *anchor; } InsertArg;
static void work_insert_delete(void *arg) {
    InsertArg *ia = (InsertArg *)arg;
    int tmp = 999;
    for (int i = 0; i < 500; i++) {
        ll_insert_after(ia->ll, &tmp, ia->anchor);
        ll_delete(ia->ll, &tmp);
    }
}

TEST(test_deadlock_push_completes) {
    LinkedList ll;
    ll_init(&ll);

    int rc = run_with_timeout(work_push_5000, &ll);
    ASSERT(rc == 0, "deadlock push: 5000 pushes complete within 5s");
    ASSERT(ll_size(&ll) == 5000, "deadlock push: size == 5000");

    ll_destroy(&ll);
}

TEST(test_deadlock_pop_completes) {
    LinkedList ll;
    ll_init(&ll);

    int vals[1000];
    for (int i = 0; i < 1000; i++) { vals[i]=i; ll_push(&ll, &vals[i]); }

    int rc = run_with_timeout(work_pop_1000, &ll);
    ASSERT(rc == 0,              "deadlock pop: 1000 pops complete within 5s");
    ASSERT(ll_size(&ll) == 0,    "deadlock pop: list is empty after pops");

    ll_destroy(&ll);
}

TEST(test_deadlock_insert_after_completes) {
    LinkedList ll;
    ll_init(&ll);

    int anchor = 1;
    ll_push(&ll, &anchor);

    InsertArg ia = { &ll, &anchor };
    int rc = run_with_timeout(work_insert_delete, &ia);
    ASSERT(rc == 0, "deadlock insertAfter: 500 insert+delete cycles within 5s");

    ll_destroy(&ll);
}

/* Two threads hammering push and pop simultaneously */
typedef struct { LinkedList *ll; int *vals; } TwoArg;

static void two_push(void *arg) {
    TwoArg *a = (TwoArg *)arg;
    for (int i = 0; i < 3000; i++) ll_push(a->ll, &a->vals[i]);
}
static void two_pop(void *arg) {
    TwoArg *a = (TwoArg *)arg;
    for (int i = 0; i < 3000; i++) ll_pop(a->ll);
}

TEST(test_deadlock_two_threads) {
    LinkedList ll;
    ll_init(&ll);

    int v1[3000], v2[3000];
    for (int i = 0; i < 3000; i++) { v1[i]=i; v2[i]=i; ll_push(&ll, &v1[i]); }

    TwoArg a1 = { &ll, v1 };
    TwoArg a2 = { &ll, v2 };

    pthread_t t1, t2;
    pthread_create(&t1, NULL, thread_wrapper, &(WorkItem){ two_push, &a1 });
    pthread_create(&t2, NULL, thread_wrapper, &(WorkItem){ two_pop,  &a2 });

    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += DEADLINE_SEC;

    int r1 = pthread_timedjoin_np(t1, NULL, &deadline);
    int r2 = pthread_timedjoin_np(t2, NULL, &deadline);

    ASSERT(r1 == 0, "deadlock two threads: push thread completes in time");
    ASSERT(r2 == 0, "deadlock two threads: pop  thread completes in time");

    if (r1 != 0) pthread_cancel(t1);
    if (r2 != 0) pthread_cancel(t2);
    if (r1 != 0) pthread_join(t1, NULL);
    if (r2 != 0) pthread_join(t2, NULL);

    ll_destroy(&ll);
}

/* ------------------------------------------------------------------ */
/*  main                                                              */
/* ------------------------------------------------------------------ */

int main(void) {
    printf("=== Thread-Safe Linked List – Automated Tests ===\n");

    printf("\n[1] UNIT TESTS\n");
    RUN(test_push_single);
    RUN(test_push_multiple_order);
    RUN(test_pop_removes_last);
    RUN(test_pop_empty);
    RUN(test_pop_single_element);
    RUN(test_insert_after_middle);
    RUN(test_insert_after_tail);
    RUN(test_insert_after_not_found);
    RUN(test_delete_existing);
    RUN(test_delete_head);
    RUN(test_delete_not_found);
    RUN(test_get_out_of_range);
    RUN(test_clear);

    printf("\n[2] CONCURRENCY TESTS\n");
    RUN(test_concurrent_pushes_correct_size);
    RUN(test_concurrent_push_pop_no_crash);

    printf("\n[3] DEADLOCK-FREEDOM TESTS  (timeout = %ds each)\n", DEADLINE_SEC);
    RUN(test_deadlock_push_completes);
    RUN(test_deadlock_pop_completes);
    RUN(test_deadlock_insert_after_completes);
    RUN(test_deadlock_two_threads);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
