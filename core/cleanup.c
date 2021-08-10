#include <cleanup.h>
#include <metric.h>
#include <list.h>
#include <stdio.h>
#include <plugin_server.h>
#include <log.h>
#include <pthread.h>

pthread_mutex_t clean_mutex = PTHREAD_MUTEX_INITIALIZER;
int clean_done = 0;
void delete_metric(struct metric_unit *unit) {
    unit->do_del_metric(unit);
}

void delete_all_metric(void) {
    struct list_head *pos;
    struct list_head *n;
    struct metric_unit *unit;
    list_for_each_safe(pos, n, &(watcher.metrics_head)) {
        unit = container_of(pos, struct metric_unit, node);
        delete_metric(unit);
    }
}

void stop_collector(void) {
    watcher.collector.stop_collector(&(watcher.collector));
}

void cleanup(void)
{
    pthread_mutex_lock(&clean_mutex);
    if (!clean_done) {
        logging(LEVEL_ZERO, "AT EXIT\n");
        logging(LEVEL_ZERO, "stop collector\n");
        stop_collector();
        logging(LEVEL_ZERO, "delete all metric\n");
        delete_all_metric();
        logging(LEVEL_ZERO, "unload plugin server\n");
        plugin_server_finish();
        logging(LEVEL_ZERO, "clean syswatcher\n");
        exit_syswatcher(&watcher);
        logging(LEVEL_ZERO, "DONE\n");
        exit_logger(&log_unit);
        clean_done = 1;
    }
    pthread_mutex_unlock(&clean_mutex);
}
