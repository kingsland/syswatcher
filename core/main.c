#include <stdio.h>
#include <metric.h>
int
main() {
    printf("syswatcher core init\n");
    metrics_head = create_metrics_chain(); //from config file
    struct list_head *pos;
    struct metric_unit *unit;
    list_for_each(pos, metrics_head) {
        unit = container_of(pos, struct metric_unit, node);
        unit->run_sub_metric(unit);
    }
}
