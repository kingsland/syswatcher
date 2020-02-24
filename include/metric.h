#ifndef METRIC_H
#define METRIC_H
#include <list.h>
#include <stdint.h>
#include <time.h>
#include <defs.h>
#include <pthread.h>

struct sub_metric_unit {
    struct      list_head sub_node;
    char        sub_metric_name[METRIC_NAME_LENGTH];//app A module1
    char        sub_metric_description[METRIC_DESC_LENGTH];
    int32_t     run_time;
    pthread_rwlock_t sub_unit_lock;
    item_t      *data_collection;  //modules1 data0
                                    //modules1 data1 
    void        (*do_del_sub_metric)(struct sub_metric_unit *subunit);
    int32_t     (*update_data)(struct sub_metric_unit *);
    int         (*do_update)(item_t *);
};

typedef struct thread_info {
    pthread_t id;
    char name[10];
    struct  list_head node;
    pthread_mutex_t updating;
    struct metric_unit *unit;
} thread_info;

struct metric_unit {
    struct      list_head node;
    struct      list_head sub_node_head;
    char        metric_name[METRIC_NAME_LENGTH];//app A
    char        metric_description[METRIC_DESC_LENGTH];
    plugin_key_t plugin_id;
    pthread_rwlock_t unit_lock;
    thread_info *update_thread;
    void        (*add_sub_metric)(struct metric_unit *, struct sub_metric_unit *);
    void        (*run_sub_metric)(struct metric_unit *);
    void        (*do_del_metric)(struct metric_unit *unit);
    time_t      last_update_time;
    time_t      expire_time;
};

#define TRAVERSAL_INTERVAL  (2)

struct syswatcher {
    struct list_head metrics_head;
    struct list_head thread_pool;
    int (*add_metric)(void *watcher, plugin_channel_t *plugin_metrics);
    int (*del_metric)(void *watcher, plugin_key_t id);
    pthread_t traversal_thread_id;
    pthread_t recycle_thread_id;
    void (*traversal_metric_units)(struct syswatcher *watcher);
    void (*thread_recycle)(struct syswatcher *watcher);
};

void init_syswatcher(struct syswatcher *watcher);

struct syswatcher watcher;
void list_metric(void);
#endif  //end of METRIC_H
