/**
 * linked_list.h
 * Thread-safe singly linked list interface.
 * Synchronised with a single POSIX pthread_mutex_t (non-recursive).
 */

#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <pthread.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  Node                                                              */
/* ------------------------------------------------------------------ */

typedef struct Node {
    void        *data;      /* Pointer to heap-allocated caller data   */
    struct Node *next;
} Node;

/* ------------------------------------------------------------------ */
/*  List                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    Node           *head;
    size_t          size;
    pthread_mutex_t lock;
} LinkedList;

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                         */
/* ------------------------------------------------------------------ */

/**
 * Initialise a new linked list.
 * Returns 0 on success, non-zero on pthread error.
 */
int  ll_init(LinkedList *list);

/**
 * Destroy the list, freeing all nodes.
 * Does NOT free node->data – the caller owns that memory.
 * Returns 0 on success.
 */
int  ll_destroy(LinkedList *list);

/* ------------------------------------------------------------------ */
/*  Core Operations                                                   */
/* ------------------------------------------------------------------ */

/**
 * push – append *data to the END of the list.
 * Returns 0 on success, -1 on allocation failure.
 */
int  ll_push(LinkedList *list, void *data);

/**
 * pop – remove the LAST node and return its data pointer.
 * Returns NULL if the list is empty.
 */
void *ll_pop(LinkedList *list);

/**
 * insertAfter – insert *data immediately after the first node
 * whose data pointer equals *after.
 * Returns  0 on success,
 *         -1 on allocation failure,
 *         -2 if *after was not found.
 */
int  ll_insert_after(LinkedList *list, void *data, void *after);

/* ------------------------------------------------------------------ */
/*  Additional CRUD helpers                                           */
/* ------------------------------------------------------------------ */

/**
 * get – return the data pointer at zero-based *index*.
 * Returns NULL if index is out of range.
 */
void *ll_get(LinkedList *list, size_t index);

/**
 * delete – remove the first node whose data pointer equals *data*.
 * Returns  0 on success, -1 if not found.
 */
int  ll_delete(LinkedList *list, void *data);

/**
 * clear – remove all nodes (does NOT free node->data).
 */
void ll_clear(LinkedList *list);

/**
 * size – return the current number of nodes.
 */
size_t ll_size(LinkedList *list);

#endif /* LINKED_LIST_H */
