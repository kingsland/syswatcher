#include <metric.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
struct metric_unit *make_unit(void);
void *do_run_sub_metric(void *arg) {
    struct metric_unit *unit = (struct metric_unit *)arg;
    pthread_mutex_lock(&(unit->updating));
    sleep(10);
    pthread_mutex_unlock(&(unit->updating));
    return NULL;
}

void _run_sub_metric(struct metric_unit *unit) {
    int ret;
    unit->last_update_time = time(NULL);
    if (unit->run_time != 0) {
        pthread_mutex_lock(&(unit->unit_mutex));
        ret = pthread_mutex_trylock(&(unit->updating));
        if (ret == 0) {
            //ok last update finished
            pthread_mutex_unlock(&(unit->updating));
            if (unit->run_time > 0) {
                unit->run_time--;
            }
            pthread_t id;
            pthread_create(&id, NULL, do_run_sub_metric, unit);
        } else {
            printf("not finish, skip %s\n", unit->metric_name);
        }
        pthread_mutex_unlock(&(unit->unit_mutex));
    }
}

void _add_sub_metric(struct metric_unit *unit, struct sub_metric_unit *subunit) {

}

struct list_head *create_metrics_chain(void) {
    int count = 4;
    NEW_LIST_NODE(head);
    for (; count > 0; count--) {
        struct metric_unit *unit;
        unit = make_unit();
        sprintf(unit->metric_name, "metric%d", count);
        sprintf(unit->metric_description, "this is metric%d, and this is for test;", count);
        list_add_tail(&(unit->node), head);
    }
    return head;
}

void metric_info(struct metric_unit *unit) {
    printf("run %s: ", unit->metric_name);
    printf("%s\n\n", unit->metric_description);
}

void list_metric(void) {
    struct list_head *pos;
    struct metric_unit *unit;
    list_for_each(pos, metrics_head) {
        unit = container_of(pos, struct metric_unit, node);
        metric_info(unit);
    }
}

struct metric_unit *make_unit(void) {
    struct metric_unit *unit = (struct metric_unit *)malloc(sizeof(struct metric_unit));
    memset(unit, 0, sizeof(struct metric_unit));
    INIT_LIST_HEAD(&(unit->node));
    INIT_LIST_HEAD(&(unit->sub_node_head));
    pthread_mutex_init(&(unit->updating), NULL);
    pthread_mutex_init(&(unit->unit_mutex), NULL);
    unit->add_sub_metric = _add_sub_metric;
    unit->run_sub_metric = _run_sub_metric;
    unit->run_time = -1;
    unit->last_update_time = 0;
    unit->expire_time = 0;
    return unit;
}

struct sub_metric_unit *make_subunit() {
    struct sub_metric_unit *subunit = (struct sub_metric_unit *)malloc(sizeof(struct sub_metric_unit));
    memset(subunit, 0, sizeof(struct sub_metric_unit));
    INIT_LIST_HEAD(&(subunit->sub_node));
    return subunit;
}

void *do_traversal_metric_units(void *arg) {
    struct list_head *head = (struct list_head *)arg;
    while(1) {
        struct list_head *pos;
        struct metric_unit *unit;
        list_for_each(pos, head) {
            unit = container_of(pos, struct metric_unit, node);
            unit->run_sub_metric(unit);
        }

        sleep(TRAVERSAL_INTERVAL);
    }
}

void destroy_unit(struct metric_unit *unit) {
    //delete all subunit
    pthread_mutex_destroy(&(unit->updating));
    pthread_mutex_destroy(&(unit->unit_mutex));
    free(unit);
    unit = NULL;
}

void destroy_subunit(struct sub_metric_unit *subunit) {
    free(subunit);
    subunit = NULL;
}

void traversal_metric_units(void) {
    pthread_create(&traversal_thread_id, NULL, do_traversal_metric_units, metrics_head);
    pthread_join(traversal_thread_id, NULL);
}
