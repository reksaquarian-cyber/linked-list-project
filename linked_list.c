/**
 * linked_list.c
 * Thread-safe singly linked list implementation.
 *
 * Locking strategy
 * ----------------
 * A single POSIX mutex serialises every public method.  The mutex is
 * PTHREAD_MUTEX_ERRORCHECK so that double-lock attempts are detected
 * immediately (useful during testing) rather than silently deadlocking.
 *
 * Deadlock avoidance
 * ------------------
 * 1. Only one lock is ever held at a time – no lock ordering needed.
 * 2. The lock is always acquired at the top of each function and
 *    released at every exit path (including error paths).
 * 3. No function calls another public function while holding the lock,
 *    which would require a recursive mutex and could hide logic errors.
 */

#include "linked_list.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ------------------------------------------------------------------ */
/*  Internal helpers (called WITHOUT holding the lock)                */
/* ------------------------------------------------------------------ */

static Node *node_new(void *data) {
    Node *n = (Node *)malloc(sizeof(Node));
    if (!n) return NULL;
    n->data = data;
    n->next = NULL;
    return n;
}

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                         */
/* ------------------------------------------------------------------ */

int ll_init(LinkedList *list) {
    if (!list) return EINVAL;

    list->head = NULL;
    list->size = 0;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    int rc = pthread_mutex_init(&list->lock, &attr);
    pthread_mutexattr_destroy(&attr);
    return rc;
}

int ll_destroy(LinkedList *list) {
    if (!list) return EINVAL;

    pthread_mutex_lock(&list->lock);
    Node *cur = list->head;
    while (cur) {
        Node *next = cur->next;
        free(cur);
        cur = next;
    }
    list->head = NULL;
    list->size = 0;
    pthread_mutex_unlock(&list->lock);

    return pthread_mutex_destroy(&list->lock);
}

/* ------------------------------------------------------------------ */
/*  push                                                              */
/* ------------------------------------------------------------------ */

int ll_push(LinkedList *list, void *data) {
    Node *new_node = node_new(data);
    if (!new_node) return -1;

    pthread_mutex_lock(&list->lock);

    if (!list->head) {
        list->head = new_node;
    } else {
        Node *cur = list->head;
        while (cur->next) cur = cur->next;
        cur->next = new_node;
    }
    list->size++;

    pthread_mutex_unlock(&list->lock);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  pop                                                               */
/* ------------------------------------------------------------------ */

void *ll_pop(LinkedList *list) {
    pthread_mutex_lock(&list->lock);

    if (!list->head) {
        pthread_mutex_unlock(&list->lock);
        return NULL;
    }

    void *result;

    if (!list->head->next) {
        /* Single element */
        result = list->head->data;
        free(list->head);
        list->head = NULL;
    } else {
        /* Walk to second-to-last */
        Node *prev = NULL;
        Node *cur  = list->head;
        while (cur->next) {
            prev = cur;
            cur  = cur->next;
        }
        result     = cur->data;
        prev->next = NULL;
        free(cur);
    }

    list->size--;
    pthread_mutex_unlock(&list->lock);
    return result;
}

/* ------------------------------------------------------------------ */
/*  insertAfter                                                       */
/* ------------------------------------------------------------------ */

int ll_insert_after(LinkedList *list, void *data, void *after) {
    Node *new_node = node_new(data);
    if (!new_node) return -1;

    pthread_mutex_lock(&list->lock);

    Node *cur = list->head;
    while (cur) {
        if (cur->data == after) {
            new_node->next = cur->next;
            cur->next      = new_node;
            list->size++;
            pthread_mutex_unlock(&list->lock);
            return 0;
        }
        cur = cur->next;
    }

    /* after not found */
    pthread_mutex_unlock(&list->lock);
    free(new_node);
    return -2;
}

/* ------------------------------------------------------------------ */
/*  get                                                               */
/* ------------------------------------------------------------------ */

void *ll_get(LinkedList *list, size_t index) {
    pthread_mutex_lock(&list->lock);

    Node  *cur = list->head;
    size_t i   = 0;
    while (cur) {
        if (i == index) {
            void *result = cur->data;
            pthread_mutex_unlock(&list->lock);
            return result;
        }
        cur = cur->next;
        i++;
    }

    pthread_mutex_unlock(&list->lock);
    return NULL;   /* out of range */
}

/* ------------------------------------------------------------------ */
/*  delete                                                            */
/* ------------------------------------------------------------------ */

int ll_delete(LinkedList *list, void *data) {
    pthread_mutex_lock(&list->lock);

    if (!list->head) {
        pthread_mutex_unlock(&list->lock);
        return -1;
    }

    if (list->head->data == data) {
        Node *old  = list->head;
        list->head = old->next;
        free(old);
        list->size--;
        pthread_mutex_unlock(&list->lock);
        return 0;
    }

    Node *prev = list->head;
    Node *cur  = list->head->next;
    while (cur) {
        if (cur->data == data) {
            prev->next = cur->next;
            free(cur);
            list->size--;
            pthread_mutex_unlock(&list->lock);
            return 0;
        }
        prev = cur;
        cur  = cur->next;
    }

    pthread_mutex_unlock(&list->lock);
    return -1;   /* not found */
}

/* ------------------------------------------------------------------ */
/*  clear                                                             */
/* ------------------------------------------------------------------ */

void ll_clear(LinkedList *list) {
    pthread_mutex_lock(&list->lock);

    Node *cur = list->head;
    while (cur) {
        Node *next = cur->next;
        free(cur);
        cur = next;
    }
    list->head = NULL;
    list->size = 0;

    pthread_mutex_unlock(&list->lock);
}

/* ------------------------------------------------------------------ */
/*  size                                                              */
/* ------------------------------------------------------------------ */

size_t ll_size(LinkedList *list) {
    pthread_mutex_lock(&list->lock);
    size_t s = list->size;
    pthread_mutex_unlock(&list->lock);
    return s;
}
