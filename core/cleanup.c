#include <cleanup.h>
#include <metric.h>
#include <list.h>
#include <stdio.h>
#include <plugin_server.h>
#include <log.h>

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
    watcher.stop_collector(&watcher);
}

void cleanup(void)
{
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
}
