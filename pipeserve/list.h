#ifndef LIST_H
#define LIST_H

#define LIST_EMPTY ((list_t *) NULL)

typedef struct list {
    int head;
    struct list *tail;
} list_t;

list_t *list_add(list_t *, int);
list_t *list_del(list_t *, int);

#endif
