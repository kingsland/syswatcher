#ifndef FIFO_H
#define FIFO_H
#include <pthread.h>
#include <list.h>
#include <stdlib.h>
struct fifo_node {
    struct list_head node;
    void *priv;
};

struct fifo_head {
    struct list_head head;
    pthread_mutex_t fifo_mtx;
    int exit_flag;
    void (*push)(struct fifo_head *, struct fifo_node *);
    struct fifo_node * (*pop)(struct fifo_head *);
    void (*clear_all_node)(struct fifo_head *);
    int (*is_empty)(struct fifo_head *);
};

#define INIT_FIFO_NODE(fnode)   \
    struct fifo_node *(fnode) = (struct fifo_node *)malloc(sizeof(struct fifo_node));    \
    INIT_LIST_HEAD(&(fnode->node));
#define DESTROY_FIFO_NODE(node) \
    {free(node);node = NULL;}

struct fifo_head *init_fifo(void);
void del_fifo(struct fifo_head *fifo);
#endif  //end of FIFO_h
