#ifndef METRIC_H
#define METRIC_H
#include <list.h>
#include <stdint.h>
#include <time.h>
#include <defs.h>
#include <pthread.h>

struct sub_metric_unit {
    struct      list_head sub_node;
    char        sub_metric_name[METRIC_NAME_LEN];
    char        sub_metric_description[METRIC_DESCRIPTION_LEN];
    int32_t     run_time;                   //infinite -1
    pthread_rwlock_t sub_unit_lock;
    enum        data_type t;
    uint32_t    size;
    char        unit[8];
    int32_t     (*update_data)(struct sub_metric_unit *);
    void        (*do_update)(void *);
    char        data[0];
};

struct metric_unit {
    struct      list_head node;
    struct      list_head sub_node_head;
    char        metric_name[METRIC_NAME_LEN];
    char        metric_description[METRIC_DESCRIPTION_LEN];
    pthread_mutex_t updating;
    pthread_rwlock_t unit_lock;
    void        (*add_sub_metric)(struct metric_unit *, struct sub_metric_unit *);
    void        (*run_sub_metric)(struct metric_unit *);
    time_t      last_update_time;
    time_t      expire_time;
};

#define TRAVERSAL_INTERVAL  (2)

struct list_head *metrics_head;
pthread_t traversal_thread_id;
struct list_head *create_metrics_chain(void);
void destroy_unit(struct metric_unit *unit);
void destroy_subunit(struct sub_metric_unit *subunit);
void list_metric(void);
void traversal_metric_units(void);
#endif  //end of METRIC_H
