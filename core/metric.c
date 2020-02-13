#include <metric.h>
#include <string.h>
#include <stdio.h>
struct metric_unit *make_unit() {
    struct metric_unit *unit = (struct metric_unit *)malloc(sizeof(struct metric_unit));
    memset(unit, 0, sizeof(struct metric_unit));
    INIT_LIST_HEAD(&(unit->node));
    INIT_LIST_HEAD(&(unit->sub_node_head));
    return unit;
}

void destory_unit(struct metric_unit *unit) {
    free(unit);
    unit = NULL;
}

struct sub_metric_unit *make_subunit() {
    struct sub_metric_unit *subunit = (struct sub_metric_unit *)malloc(sizeof(struct sub_metric_unit));
    memset(subunit, 0, sizeof(struct sub_metric_unit));
    INIT_LIST_HEAD(&(subunit->sub_node));
    return subunit;
}

void destory_subunit(struct sub_metric_unit *subunit) {
    free(subunit);
    subunit = NULL;
}

void _run_sub_metric(struct metric_unit *unit) {
    unit->last_update_time = time(NULL);
    if (unit->run_time != 0) {
        if (unit->run_time > 0) {
            unit->run_time--;
        }
        printf("==================\n");
        printf("%s\n", unit->metric_name);
        printf("%s\n\n", unit->metric_description);
    }
}

struct list_head *create_metrics_chain(void) {
    int count = 4;
    NEW_LIST_NODE(head);
    for (; count > 0; count--) {
        struct metric_unit *unit;
        unit = make_unit();
        sprintf(unit->metric_name, "metric%d", count);
        sprintf(unit->metric_description, "this is metric%d, and this is for test;", count);
        unit->run_time = 10;
        unit->last_update_time = 0;
        unit->expire_time = 0;
        unit->run_sub_metric = _run_sub_metric;
        list_add_tail(&(unit->node), head);
    }
    return head;
}
