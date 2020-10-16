#ifndef COLLECTOR_H
#define COLLECTOR_H
#define COLLECTOR_BUFFER    (1024*1024)
#define METRIC_DATA_BUFFER  (512*1024)
#include <pthread.h>
struct data_collector {
    //FIXME
    //this may not be enougth
    char visual_data[COLLECTOR_BUFFER];
    char json_data[COLLECTOR_BUFFER];
    void * (*do_collect)(void *arg);
    pthread_rwlock_t data_lock;
    pthread_t resp_ti;
    pthread_t resp_json_ti;
    pthread_t collect_ti;
    int (*start_collector)(struct data_collector *collector);
    int (*stop_collector)(struct data_collector *collector);
};
void init_collector(struct data_collector *collector, void * (*handler)(void *arg));
void exit_collector(struct data_collector *collector);
#endif //end of COLLECTOR_H
