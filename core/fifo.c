#include <list.h>
#include <fifo.h>
#include <errno.h>
#include <stdio.h>

void _push(struct fifo_head *fifo, struct fifo_node *node)
{
    if (fifo->exit_flag) {
        return;
    }
    pthread_mutex_lock(&(fifo->fifo_mtx));
    list_add(&(fifo->head), &(node->node));
    pthread_mutex_unlock(&(fifo->fifo_mtx));
}

struct fifo_node *_pop(struct fifo_head *fifo)
{
    struct list_head *list_node;
    struct fifo_node *node = NULL;
    if (fifo->exit_flag) {
        return NULL;
    }
    pthread_mutex_lock(&(fifo->fifo_mtx));
    if (!fifo->is_empty(fifo)) {
        list_node = fifo->head.prev;
        node = container_of(list_node, struct fifo_node, node);
        list_del(list_node);
    }
    pthread_mutex_unlock(&(fifo->fifo_mtx));
    return node;
}

void _clear_all_node(struct fifo_head *fifo)
{
    struct list_head *head = &(fifo->head);
    struct list_head *pos, *n;
    struct fifo_node *node;
    pthread_mutex_lock(&(fifo->fifo_mtx));
    list_for_each_safe(pos, n, head) {
        node = container_of(pos, struct fifo_node, node);
        list_del(pos);
        free(node);
    }
    pthread_mutex_unlock(&(fifo->fifo_mtx));
}

int _is_empty(struct fifo_head *fifo)
{
    return (fifo->head.prev == &(fifo->head))?1:0;
}

struct fifo_head *init_fifo(void)
{
    struct fifo_head *fifo;
    fifo = (struct fifo_head *)malloc(sizeof(struct fifo_head));
    if (fifo == NULL) {
        return NULL;
    }
    pthread_mutex_init(&(fifo->fifo_mtx), NULL);
    INIT_LIST_HEAD(&(fifo->head));
    fifo->push = _push;
    fifo->pop = _pop;
    fifo->clear_all_node = _clear_all_node;
    fifo->is_empty = _is_empty;
    fifo->exit_flag = 0;
    return fifo;
}

void del_fifo(struct fifo_head *fifo)
{
    pthread_mutex_lock(&(fifo->fifo_mtx));
    fifo->exit_flag = 1;
    pthread_mutex_unlock(&(fifo->fifo_mtx));
    pthread_mutex_destroy(&(fifo->fifo_mtx));
    //pop all the node
    fifo->clear_all_node(fifo);
    free(fifo);
    fifo = NULL;
}
