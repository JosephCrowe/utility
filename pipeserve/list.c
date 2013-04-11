#include <stdlib.h>
#include "list.h"

list_t *list_add(list_t *list, int value) {
    list_t *item = malloc(sizeof(list_t));
    item->head = value;
    item->tail = list;
    return item;
}

list_t *list_del(list_t *list, int value) {
    list_t head = { 0, list };
    for (list_t *prev = &head; prev->tail != NULL; prev = prev->tail) {
        if (prev->tail->head == value) {
            list_t *item = prev->tail;
            prev->tail = item->tail;
            free(item);
            break;
        }
    }
    return head.tail;
}
