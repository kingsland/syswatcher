#ifndef METRIC_H
#define METRIC_H
#include <list.h>
#include <stdint.h>
#include <time.h>
#include <defs.h>
#include <pthread.h>

#define RING_STEP   (1)

struct sub_metric_unit {
    struct      list_head sub_node;
    char        sub_metric_name[METRIC_NAME_LENGTH];//app A module1
    char        sub_metric_description[METRIC_DESC_LENGTH];
    int32_t     run_time;
    pthread_mutex_t sub_unit_lock;
    time_t      interval;
    time_t      time_ring_left;
    int         (*go_one_step)(struct sub_metric_unit *);
    void        (*reset_time_ring)(struct sub_metric_unit *);
    item_t      *data_collection;  //modules1 data0
                                   //modules1 data1
    void        (*do_del_sub_metric)(struct sub_metric_unit *subunit);
    int32_t     (*update_data)(struct sub_metric_unit *);
    int         (*do_update)(item_t *);
};

typedef struct thread_info {
    pthread_t id;
    int thread_running;
    struct fifo_head *action_fifo;
    pthread_mutex_t ti_mtx;
} thread_info;

struct metric_unit {
    struct      list_head node;
    struct      list_head sub_node_head;
    char        metric_name[METRIC_NAME_LENGTH];//app A
    char        metric_description[METRIC_DESC_LENGTH];
    plugin_key_t plugin_id;
    pthread_mutex_t unit_lock;
    thread_info *update_thread_info;
    int         exiting;
    int         (*time_ring_move_forward)(struct metric_unit *);
    void        (*add_sub_metric)(struct metric_unit *, struct sub_metric_unit *);
    void        (*run_sub_metric)(struct metric_unit *);
    void        (*do_del_metric)(struct metric_unit *unit);
    time_t      last_update_time;
    time_t      expire_time;
};

struct syswatcher {
    struct list_head metrics_head;
    pthread_mutex_t plugin_lock;
    int (*add_metric)(void *watcher, plugin_channel_t *plugin_metrics);
    int (*del_metric)(void *watcher, plugin_key_t id);
    pthread_t traversal_thread_id;
    pthread_t recycle_thread_id;
    void (*traversal_metric_units)(struct syswatcher *watcher);
    int (*start_collector)(struct syswatcher *watcher);
    int (*stop_collector)(struct syswatcher *watcher);
};

void init_syswatcher(struct syswatcher *watcher);

struct syswatcher watcher;
void list_metric(void);
#endif  //end of METRIC_H
