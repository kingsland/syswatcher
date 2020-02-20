#ifndef LOG_H
#define LOG_H
#include <log_ext_def.h>
#include <list.h>
#include <pthread.h>
#include <stdio.h>
struct log_msg {
    struct list_head node;
    enum log_level level;
    char *buf;
};

struct logger {
    FILE *logfile;
    enum log_level level;
    //do log action, when log level which user give higher than this.
    struct list_head log_buffer;
    pthread_mutex_t res_mtx;
    pthread_mutex_t notify_mtx;
    pthread_cond_t notify_cond;
    pthread_t logging_id;
    int thread_running;
    int closed;
    void (*print_msg)(struct logger *log_unit, struct log_msg *msg);
    void (*clear_all_bufferd_msg)(struct logger *log_unit);
};

struct logger log_unit;
int init_logger(struct logger *log_unit, enum log_level level);
void exit_logger(struct logger *log_unit);
#endif  //end of LOG_H
