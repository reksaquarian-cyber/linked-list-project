/**
 * rest_api.c
 * REST API for the Thread-Safe Singly Linked List.
 *
 * Uses libmicrohttpd for HTTP serving and cJSON for JSON encode/decode.
 *
 * Build:
 *   gcc -o rest_api rest_api.c linked_list.c \
 *       -lmicrohttpd -lcjson -lpthread
 *
 * Install dependencies (Debian/Ubuntu):
 *   sudo apt-get install libmicrohttpd-dev libcjson-dev
 *
 * Endpoints:
 *   GET    /list                  - Return all elements
 *   GET    /list/index/<n>        - Return element at index n
 *   POST   /list/push             - Append element   body: {"value":"<v>"}
 *   DELETE /list/pop              - Remove last element
 *   POST   /list/insert_after     - Insert after     body: {"value":"<v>","after":"<a>"}
 *   DELETE /list/delete/<value>   - Remove first occurrence of value
 *   DELETE /list/clear            - Clear entire list
 */

#include <microhttpd.h>
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "linked_list.h"

#define PORT        5000
#define MAX_BODY    8192

/* Global shared list – all requests share one instance */
static LinkedList g_list;

/* ------------------------------------------------------------------ */
/*  Upload buffer (accumulate POST body)                              */
/* ------------------------------------------------------------------ */

typedef struct {
    char   buf[MAX_BODY];
    size_t len;
} UploadData;

/* ------------------------------------------------------------------ */
/*  JSON response helpers                                             */
/* ------------------------------------------------------------------ */

/* Build {"items":[...],"size":<n>} snapshot */
static cJSON *list_snapshot(void) {
    cJSON *root  = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();

    size_t n = ll_size(&g_list);
    for (size_t i = 0; i < n; i++) {
        char *v = (char *)ll_get(&g_list, i);
        cJSON_AddItemToArray(items, v ? cJSON_CreateString(v)
                                      : cJSON_CreateNull());
    }
    cJSON_AddItemToObject(root, "items", items);
    cJSON_AddNumberToObject(root, "size", (double)n);
    return root;
}

static struct MHD_Response *json_response(cJSON *obj, int *out_status, int status) {
    char *body = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    struct MHD_Response *resp = MHD_create_response_from_buffer(
        strlen(body), body, MHD_RESPMEM_MUST_FREE);
    MHD_add_response_header(resp, "Content-Type", "application/json");
    *out_status = status;
    return resp;
}

static struct MHD_Response *error_response(const char *msg,
                                            int *out_status, int status) {
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "error", msg);
    return json_response(obj, out_status, status);
}

/* ------------------------------------------------------------------ */
/*  Route handlers                                                    */
/* ------------------------------------------------------------------ */

/* GET /list */
static struct MHD_Response *handle_get_all(int *status) {
    cJSON *snap = list_snapshot();
    return json_response(snap, status, MHD_HTTP_OK);
}

/* GET /list/index/<n> */
static struct MHD_Response *handle_get_index(const char *path, int *status) {
    int idx = atoi(path + strlen("/list/index/"));
    if (idx < 0) return error_response("Invalid index", status, MHD_HTTP_BAD_REQUEST);

    char *v = (char *)ll_get(&g_list, (size_t)idx);
    if (!v)   return error_response("Index out of range", status, MHD_HTTP_NOT_FOUND);

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "index", idx);
    cJSON_AddStringToObject(obj, "value", v);
    return json_response(obj, status, MHD_HTTP_OK);
}

/* POST /list/push  body: {"value":"<v>"} */
static struct MHD_Response *handle_push(const char *body_str, int *status) {
    cJSON *body = cJSON_Parse(body_str);
    if (!body) return error_response("Invalid JSON", status, MHD_HTTP_BAD_REQUEST);

    cJSON *val = cJSON_GetObjectItem(body, "value");
    if (!val || !cJSON_IsString(val)) {
        cJSON_Delete(body);
        return error_response("Missing 'value'", status, MHD_HTTP_BAD_REQUEST);
    }

    char *data = strdup(val->valuestring);
    cJSON_Delete(body);

    if (ll_push(&g_list, data) != 0) {
        free(data);
        return error_response("Allocation failed", status, MHD_HTTP_INTERNAL_SERVER_ERROR);
    }

    cJSON *snap = list_snapshot();
    cJSON_AddStringToObject(snap, "message", "Pushed");
    return json_response(snap, status, MHD_HTTP_CREATED);
}

/* DELETE /list/pop */
static struct MHD_Response *handle_pop(int *status) {
    char *v = (char *)ll_pop(&g_list);
    if (!v) return error_response("List is empty", status, MHD_HTTP_NOT_FOUND);

    cJSON *snap = list_snapshot();
    cJSON_AddStringToObject(snap, "removed", v);
    free(v);
    return json_response(snap, status, MHD_HTTP_OK);
}

/* POST /list/insert_after  body: {"value":"<v>","after":"<a>"} */
static struct MHD_Response *handle_insert_after(const char *body_str, int *status) {
    cJSON *body = cJSON_Parse(body_str);
    if (!body) return error_response("Invalid JSON", status, MHD_HTTP_BAD_REQUEST);

    cJSON *jval   = cJSON_GetObjectItem(body, "value");
    cJSON *jafter = cJSON_GetObjectItem(body, "after");
    if (!jval || !cJSON_IsString(jval) ||
        !jafter || !cJSON_IsString(jafter)) {
        cJSON_Delete(body);
        return error_response("Need 'value' and 'after'", status, MHD_HTTP_BAD_REQUEST);
    }

    /* Find the after-node by string content */
    size_t n = ll_size(&g_list);
    void  *after_ptr = NULL;
    for (size_t i = 0; i < n; i++) {
        char *v = (char *)ll_get(&g_list, i);
        if (v && strcmp(v, jafter->valuestring) == 0) {
            after_ptr = v;
            break;
        }
    }

    if (!after_ptr) {
        cJSON_Delete(body);
        return error_response("'after' value not found", status, MHD_HTTP_NOT_FOUND);
    }

    char *new_data = strdup(jval->valuestring);
    cJSON_Delete(body);

    int rc = ll_insert_after(&g_list, new_data, after_ptr);
    if (rc != 0) {
        free(new_data);
        return error_response("Insert failed", status, MHD_HTTP_INTERNAL_SERVER_ERROR);
    }

    cJSON *snap = list_snapshot();
    cJSON_AddStringToObject(snap, "message", "Inserted");
    return json_response(snap, status, MHD_HTTP_CREATED);
}

/* DELETE /list/delete/<value> */
static struct MHD_Response *handle_delete_value(const char *path, int *status) {
    const char *target = path + strlen("/list/delete/");

    size_t n        = ll_size(&g_list);
    void  *data_ptr = NULL;
    for (size_t i = 0; i < n; i++) {
        char *v = (char *)ll_get(&g_list, i);
        if (v && strcmp(v, target) == 0) {
            data_ptr = v;
            break;
        }
    }

    if (!data_ptr) return error_response("Value not found", status, MHD_HTTP_NOT_FOUND);

    ll_delete(&g_list, data_ptr);
    free((char *)data_ptr);

    cJSON *snap = list_snapshot();
    cJSON_AddStringToObject(snap, "removed", target);
    return json_response(snap, status, MHD_HTTP_OK);
}

/* DELETE /list/clear */
static struct MHD_Response *handle_clear(int *status) {
    /* Free all string data before clearing */
    size_t n = ll_size(&g_list);
    for (size_t i = 0; i < n; i++) {
        char *v = (char *)ll_get(&g_list, i);
        free(v);
    }
    ll_clear(&g_list);
    cJSON *snap = list_snapshot();
    cJSON_AddStringToObject(snap, "message", "Cleared");
    return json_response(snap, status, MHD_HTTP_OK);
}

/* ------------------------------------------------------------------ */
/*  MHD request handler                                               */
/* ------------------------------------------------------------------ */

static enum MHD_Result request_handler(
        void *cls,
        struct MHD_Connection *conn,
        const char *url,
        const char *method,
        const char *version,
        const char *upload_data,
        size_t     *upload_data_size,
        void      **con_cls)
{
    (void)cls; (void)version;

    /* First call: allocate upload buffer */
    if (*con_cls == NULL) {
        UploadData *ud = (UploadData *)calloc(1, sizeof(UploadData));
        if (!ud) return MHD_NO;
        *con_cls = ud;
        return MHD_YES;
    }

    UploadData *ud = (UploadData *)*con_cls;

    /* Accumulate POST body */
    if (*upload_data_size > 0) {
        size_t space = MAX_BODY - ud->len - 1;
        size_t copy  = *upload_data_size < space ? *upload_data_size : space;
        memcpy(ud->buf + ud->len, upload_data, copy);
        ud->len += copy;
        ud->buf[ud->len] = '\0';
        *upload_data_size = 0;
        return MHD_YES;
    }

    /* Route */
    struct MHD_Response *resp;
    int status = MHD_HTTP_NOT_FOUND;

    if (strcmp(method, "GET") == 0) {
        if (strcmp(url, "/list") == 0)
            resp = handle_get_all(&status);
        else if (strncmp(url, "/list/index/", 12) == 0)
            resp = handle_get_index(url, &status);
        else
            resp = error_response("Not found", &status, MHD_HTTP_NOT_FOUND);

    } else if (strcmp(method, "POST") == 0) {
        if (strcmp(url, "/list/push") == 0)
            resp = handle_push(ud->buf, &status);
        else if (strcmp(url, "/list/insert_after") == 0)
            resp = handle_insert_after(ud->buf, &status);
        else
            resp = error_response("Not found", &status, MHD_HTTP_NOT_FOUND);

    } else if (strcmp(method, "DELETE") == 0) {
        if (strcmp(url, "/list/pop") == 0)
            resp = handle_pop(&status);
        else if (strcmp(url, "/list/clear") == 0)
            resp = handle_clear(&status);
        else if (strncmp(url, "/list/delete/", 13) == 0)
            resp = handle_delete_value(url, &status);
        else
            resp = error_response("Not found", &status, MHD_HTTP_NOT_FOUND);

    } else {
        resp = error_response("Method not allowed", &status, MHD_HTTP_METHOD_NOT_ALLOWED);
    }

    enum MHD_Result ret = MHD_queue_response(conn, (unsigned int)status, resp);
    MHD_destroy_response(resp);
    free(ud);
    *con_cls = NULL;
    return ret;
}

/* ------------------------------------------------------------------ */
/*  main                                                              */
/* ------------------------------------------------------------------ */

int main(void) {
    if (ll_init(&g_list) != 0) {
        fprintf(stderr, "Failed to initialise linked list\n");
        return 1;
    }

    struct MHD_Daemon *daemon = MHD_start_daemon(
        MHD_USE_INTERNAL_POLLING_THREAD,
        PORT,
        NULL, NULL,
        &request_handler, NULL,
        MHD_OPTION_END);

    if (!daemon) {
        fprintf(stderr, "Failed to start HTTP daemon on port %d\n", PORT);
        ll_destroy(&g_list);
        return 1;
    }

    printf("REST API listening on http://localhost:%d\n", PORT);
    printf("Press ENTER to stop...\n");
    getchar();

    MHD_stop_daemon(daemon);
    ll_destroy(&g_list);
    return 0;
}
