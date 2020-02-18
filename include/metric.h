#ifndef METRIC_H
#define METRIC_H
#include <list.h>
#include <stdint.h>
#include <time.h>
#include <defs.h>
#include <pthread.h>

typedef struct mate_t {
    char        name[16];
    char        unit[16];
    enum        data_type t;
    union       val_t val;
} mate_t;

struct sub_metric_unit {
    struct      list_head sub_node;
    char        sub_metric_name[METRIC_NAME_LEN];
    char        sub_metric_description[METRIC_DESCRIPTION_LEN];
    int32_t     run_time;                   //infinite -1
    pthread_rwlock_t sub_unit_lock;
    int         data_num;
    mate_t      *data;
    void        (*del_sub_metric_safely)(struct sub_metric_unit *subunit);
    void        (*del_sub_metric)(struct sub_metric_unit *subunit);
    int32_t     (*update_data)(struct sub_metric_unit *);
    void        (*do_update)(void *);
};

struct metric_unit {
    struct      list_head node;
    struct      list_head sub_node_head;
    char        metric_name[METRIC_NAME_LEN];
    char        metric_description[METRIC_DESCRIPTION_LEN];
    pthread_mutex_t updating;
    pthread_rwlock_t unit_lock;
    pthread_t   update_id;
    void        (*add_sub_metric)(struct metric_unit *, struct sub_metric_unit *);
    void        (*run_sub_metric)(struct metric_unit *);
    void        (*del_metric_safely)(struct metric_unit *unit);
    void        (*del_metric)(struct metric_unit *unit);
    time_t      last_update_time;
    time_t      expire_time;
};

#define TRAVERSAL_INTERVAL  (2)

struct syswatcher {
    struct list_head *metrics_head;
    pthread_t traversal_thread_id;
    void (*traversal_metric_units)(void);
    struct list_head *(*create_metrics_chain)(void);
};

void init_syswatcher(struct syswatcher *watcher);

struct syswatcher watcher;
void list_metric(void);
#endif  //end of METRIC_H
