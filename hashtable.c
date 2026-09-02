#include "hashtable.h"
#include "resp.h"
#include "hash.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define HT_BUCKETS_SIZE 4

typedef struct HTNode {
    char* key;
    BulkString_t* value;
    struct HTNode* next;
    struct HTNode* prev;
} HTNode_t;

typedef struct NodeResult {
    HTNode_t* prev;
    HTNode_t* node;
} NodeResult_t;

static HTNode_t** ht;

static size_t current_size = HT_BUCKETS_SIZE;
static size_t num_elements = 0;

void ht_init(void) {
    derive_secret((uint64_t)generate_rand());
    ht = malloc(sizeof(HTNode_t) * current_size);
    memset(ht, 0, sizeof(HTNode_t) * current_size);
}
void ht_free(void) {
    for (size_t i = 0; i < current_size; ++i) {
        HTNode_t* node = ht[i];
        if (node == NULL) {
            continue;
        }
        while (node != NULL) {
            HTNode_t* next = node->next;
            HTNode_t* tmp_node = node;
            free(tmp_node->key);
            free(tmp_node->value->value);
            free(tmp_node->value);
            free(tmp_node);
            node = next;
        }
    }
    free(ht);
}
void ht_print(void) {
    for (size_t i = 0; i < current_size; ++i) {
        printf("[%zu] -> ", i);
        HTNode_t* node = ht[i];
        for (; node != NULL; node = node->next) {
            printf("HTN{ %s, ", node->key);
            print_bs(node->value, false);
            printf(" } ->");
        }
        printf("\n");
    }
    printf("\n");
}
static void ht_resize(size_t new_size) {
    if (new_size == current_size) {
        return;
    }
    if (new_size < current_size) {
        // ensure the remainder are not leaked
        // TODO
    }
    ht = realloc(ht, sizeof(HTNode_t) * new_size);
    current_size = new_size;
}

/* returns the previous node before the node */
static NodeResult_t find_node_with_idx(char* key, uint64_t idx) {
    HTNode_t* bucket = ht[idx];
    HTNode_t* node = bucket;
    HTNode_t* prev_node = NULL;
    for (; node != NULL; node = node->next) {
        if (strcmp(key, node->key) == 0) {
            return (NodeResult_t){
                .node = node,
                .prev = prev_node
            };
        }
        prev_node = node;
    }
    return (NodeResult_t){
        .node = NULL,
        .prev = prev_node,
    };
}
static NodeResult_t find_node(char* key) {
    uint64_t digest = hash(key);
    uint64_t idx = digest % current_size;
    return find_node_with_idx(key, idx);
}
static HTNode_t* new_node(char* key, BulkString_t* in_value) {
    HTNode_t* node = malloc(sizeof(HTNode_t));
    node->key = malloc(strlen(key)+1);
    snprintf(node->key, strlen(key)+1, "%s", key);
    node->value = malloc(sizeof(BulkString_t));
    memset(node->value, 0, sizeof(BulkString_t));
    copy_bs(node->value, in_value);
    node->next = NULL;
    return node;
}

bool ht_set(char* key, BulkString_t* in_value) {
    uint64_t digest = hash(key);
    uint64_t idx = digest % current_size;
    NodeResult_t result = find_node_with_idx(key, idx);
    // TODO resize
    HTNode_t* prev = result.prev;
    HTNode_t* node = result.node;
    if (node == NULL) {
        if (RESIZE && (num_elements + 1 >= current_size / 2)) {
            ht_resize(current_size * 2);
        }
        if (prev == NULL) {
            // insert at bucket head
            ht[idx] = new_node(key, in_value);
        } else {
            // insert new node
            prev->next = new_node(key, in_value);
        }
        ++num_elements;
    } else {
        // overwrite
        copy_bs(node->value, in_value);
    }
    // TODO other false cases involve extra arguments to 'set'
    return true;
}
void ht_get(char* key, RespType_t* out_value) {
    NodeResult_t result = find_node(key);
    if (result.node != NULL) {
        out_value->type = BULKSTRING;
        copy_bs(&out_value->bulkstring, result.node->value);
    } else {
        respond_null(out_value);
    }
}