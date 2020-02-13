#ifndef METRIC_H
#define METRIC_H
#include <list.h>
#include <stdint.h>
#include <time.h>
#include <defs.h>
struct metric_unit {
    struct  list_head node;
    struct  list_head sub_node_head;
    char    metric_name[METRIC_NAME_LEN];
    char    metric_description[METRIC_DESCRIPTION_LEN];
    int32_t run_time;                   //infinite -1
    time_t  last_update_time;
    time_t  expire_time;
    void    (*run_sub_metric)(struct metric_unit *);
};

struct sub_metric_unit {
    struct  list_head sub_node;
    char    sub_metric_name[METRIC_NAME_LEN];
    char    sub_metric_description[METRIC_DESCRIPTION_LEN];
    int32_t (*update_data)(struct sub_metric_unit *);
    char    data[0];
};

struct list_head *metrics_head;
struct list_head *create_metrics_chain(void);
#endif  //end of METRIC_H
