#include <cleanup.h>
#include <metric.h>
#include <list.h>
#include <stdio.h>

void delete_all_metric(void) {
    struct list_head *pos;
    struct list_head *n;
    struct metric_unit *unit;
    list_for_each_safe(pos, n, metrics_head) {
        unit = container_of(pos, struct metric_unit, node);
        delete_metric(unit);
    }
}

void delete_metric(struct metric_unit *unit) {
    list_del(&(unit->node));
    destroy_unit(unit);
}

