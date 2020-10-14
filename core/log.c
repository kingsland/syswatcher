#include <log.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#define HEADER_LEN  (30)
#define HEADER_NUM  (20)
char header_str[][HEADER_NUM] = HEADER_STR;

void make_msg(struct log_msg *msg, char *sysmsg)
{
    char header[HEADER_LEN];
    char time_str[64];
    *sysmsg = '\0';
    if (msg->level < HEADER_NUM) {
        time_t t = time(NULL);
        sprintf(time_str, "%s", ctime(&t));
        time_str[strlen(time_str) - 1] = '\0';  //handle '\n'
        sprintf(header, "[%10s]", header_str[msg->level]);
        sprintf(sysmsg, "%s %s %s", time_str, header, msg->buf);
    }
}

void *logging_thread(void *arg)
{
    struct logger *log_unit = (struct logger *)arg;
    struct list_head *pos, *n;
    struct log_msg *msg;
    struct timespec ts;
    struct timeval tv;
    int cond_ret;
    char str[4096];
    while (log_unit->thread_running) {
        log_unit->closed = 0;
        pthread_mutex_lock(&(log_unit->notify_mtx));
        gettimeofday(&tv, NULL);
        ts.tv_nsec = tv.tv_usec * 1000 + 100000000;
        ts.tv_sec = tv.tv_sec;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_nsec -= 1000000000;
            ts.tv_sec++;
        }
        cond_ret = pthread_cond_timedwait(&(log_unit->notify_cond), &(log_unit->notify_mtx), &ts);
        pthread_mutex_unlock(&(log_unit->notify_mtx));
        if (cond_ret == ETIMEDOUT) {
            continue;
        }
        list_for_each_safe(pos, n, &(log_unit->log_buffer)) {
            msg = container_of(pos, struct log_msg, node);
            pthread_mutex_lock(&log_unit->res_mtx);
            list_del(pos);
            pthread_mutex_unlock(&log_unit->res_mtx);
            if (msg->level <= log_unit->level) {
                make_msg(msg, str);
                fwrite(str, strlen(str), 1, log_unit->logfile);
                fflush(log_unit->logfile);
            }
            free(msg->buf);
            free(msg);
        }
    }
    printf("exit log thread\n");
    pthread_exit(NULL);
}

void _print_msg(struct logger *log_unit, struct log_msg *msg)
{
    pthread_mutex_lock(&(log_unit->res_mtx));
    list_add_tail(&(msg->node), &(log_unit->log_buffer));
    pthread_mutex_unlock(&(log_unit->res_mtx));

    pthread_mutex_lock(&(log_unit->notify_mtx));
    pthread_cond_signal(&(log_unit->notify_cond));
    pthread_mutex_unlock(&(log_unit->notify_mtx));
}

void plugin_print_log(const char *fmt, ...)
{
    struct log_msg *msg;
    int size = 0;
    char *p = NULL;
    va_list ap;
    if (log_unit.closed) {
        return;
    }
    /* Determine required size */

    va_start(ap, fmt);
    size = vsnprintf(p, size, fmt, ap);
    va_end(ap);

    if (size < 0)
        return;

    size++;             /* For '\0' */
    p = malloc(size);
    if (p == NULL)
        return;

    va_start(ap, fmt);
    size = vsnprintf(p, size, fmt, ap);
    if (size < 0) {
        free(p);
        return;
    }
    va_end(ap);
    msg = (struct log_msg *)malloc(sizeof(struct log_msg));
    if (msg == NULL) {
        free(p);
        return;
    }
    msg->buf = p;
    msg->level = LEVEL_PLUGIN;
    log_unit.print_msg(&log_unit, msg);

}

void logging(enum log_level level, const char *fmt, ...)
{
    struct log_msg *msg;
    int size = 0;
    char *p = NULL;
    va_list ap;
    if (log_unit.closed) {
        return;
    }
    /* Determine required size */

    va_start(ap, fmt);
    size = vsnprintf(p, size, fmt, ap);
    va_end(ap);

    if (size < 0)
        return;

    size++;             /* For '\0' */
    p = malloc(size);
    if (p == NULL)
        return;

    va_start(ap, fmt);
    size = vsnprintf(p, size, fmt, ap);
    if (size < 0) {
        free(p);
        return;
    }
    va_end(ap);
    msg = (struct log_msg *)malloc(sizeof(struct log_msg));
    if (msg == NULL) {
        free(p);
        return;
    }
    msg->buf = p;
    msg->level = level;
    log_unit.print_msg(&log_unit, msg);
}


void _clear_all_bufferd_msg(struct logger *log_unit)
{
    struct list_head *pos, *n;
    struct log_msg *msg;
    list_for_each_safe(pos, n, &(log_unit->log_buffer)) {
        msg = container_of(pos, struct log_msg, node);
        pthread_mutex_lock(&log_unit->res_mtx);
        list_del(pos);
        pthread_mutex_unlock(&log_unit->res_mtx);
        free(msg->buf);
        free(msg);
    }
}

int init_logger(struct logger *log_unit, enum log_level level)
{
    char filename[64];
    sprintf(filename, "/var/log/syswatcher.log");
    log_unit->logfile = fopen(filename, "a+");
    if (log_unit->logfile == NULL) {
        perror("open log file");
        return 1;
    }
    log_unit->level = level;
    log_unit->print_msg = _print_msg;
    log_unit->clear_all_bufferd_msg = _clear_all_bufferd_msg;
    INIT_LIST_HEAD(&(log_unit->log_buffer));
    pthread_mutex_init(&(log_unit->notify_mtx), NULL);
    pthread_mutex_init(&(log_unit->res_mtx), NULL);
    pthread_cond_init(&(log_unit->notify_cond), NULL);
    log_unit->thread_running = 1;
    log_unit->closed = 1;
    pthread_create(&(log_unit->logging_id), NULL, logging_thread, log_unit);
    while(log_unit->closed);
    return 0;
}

void exit_logger(struct logger *log_unit)
{
    log_unit->closed = 1;
    log_unit->thread_running = 0;

    pthread_join(log_unit->logging_id, NULL);
    fclose(log_unit->logfile);
    log_unit->logfile = NULL;
    //normally, at this point, we have flush all the msg in log file
    //this call is not necessery
    log_unit->clear_all_bufferd_msg(log_unit);

    pthread_mutex_destroy(&(log_unit->notify_mtx));
    pthread_mutex_destroy(&(log_unit->res_mtx));
    pthread_cond_destroy(&(log_unit->notify_cond));
}
