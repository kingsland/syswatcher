#include <metric.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
struct metric_unit *make_unit(void);
struct sub_metric_unit *make_subunit(char *name,
        char *description, int32_t run_time,
        item_t *data, int (*update)(item_t *));
void free_subunit(struct sub_metric_unit *subunit);
void _add_sub_metric(struct metric_unit *unit, struct sub_metric_unit *subunit);
void _do_del_metric(struct metric_unit *unit);
void _do_del_metric_safely(struct metric_unit *unit);
void _destroy_subunit(struct sub_metric_unit *subunit);
void _free_subunit(struct sub_metric_unit *subunit);

void *do_run_sub_metric(void *arg)
{
    struct metric_unit *unit = (struct metric_unit *)arg;
    struct list_head *head = &(unit->sub_node_head);
    pthread_mutex_lock(&(unit->updating));
    struct list_head *pos;
    struct sub_metric_unit *subunit;
    list_for_each(pos, head) {
        subunit = container_of(pos, struct sub_metric_unit, sub_node);
        if (subunit->run_time == 0) {
            continue;
        }
        if (subunit->run_time != -1) {
            subunit->run_time--;
        }
        if (subunit->update_data != NULL) {
            subunit->update_data(subunit);
        }
    }

    pthread_mutex_unlock(&(unit->updating));
    pthread_exit(NULL);
}

void _run_sub_metric(struct metric_unit *unit)
{
    int ret;
    pthread_rwlock_wrlock(&(unit->unit_lock));
    ret = pthread_mutex_trylock(&(unit->updating));
    if (ret == 0) {
        pthread_mutex_unlock(&(unit->updating));
        unit->last_update_time = time(NULL);
        pthread_create(&(unit->update_id), NULL, do_run_sub_metric, unit);
    } else {
        printf("not finish, skip %s\n", unit->metric_name);
    }
    pthread_rwlock_unlock(&(unit->unit_lock));
}

void _add_sub_metric(struct metric_unit *unit, struct sub_metric_unit *subunit)
{
    list_add_tail(&(subunit->sub_node), &(unit->sub_node_head));
}

//void cpu_name_update(void *data) {
//    strcpy(data, "intel x86");
//    printf("%s %d\n", __func__, __LINE__);
//}
//
//void cpu_load_update(void *data) {
//    float freq = 3.4;
//    memcpy(data, &freq, sizeof(freq));
//    printf("%s %d\n", __func__, __LINE__);
//}
//
//void create_sub_metric_chain(struct metric_unit *unit)
//{
//    struct sub_metric_unit *subunit;
//    char *name = "cpu generic information";
//    char *desc = "cpu name, frequency...";
//    mate_t *data = (mate_t *)malloc(sizeof(mate_t));
//    subunit = make_subunit(name, desc, 1, 1, data, cpu_name_update);
//    unit->add_sub_metric(unit, subunit);
//    name = "cpu load";
//    desc = "load";
//    data = (mate_t *)malloc(sizeof(mate_t));
//    subunit = make_subunit(name, desc, -1, 1, data, cpu_load_update);
//    unit->add_sub_metric(unit, subunit);
//}
//
//struct list_head *_create_metrics_chain(void)
//{
//    int count = 4;
//    struct metric_unit *unit;
//    NEW_LIST_NODE(head);
//    for (; count > 0; count--) {
//        unit = make_unit();
//        sprintf(unit->metric_name, "metric%d", count);
//        sprintf(unit->metric_description, "this is metric%d, and this is for test;", count);
//        list_add_tail(&(unit->node), head);
//    }
//    unit = make_unit();
//    sprintf(unit->metric_name, "cpu misc");
//    sprintf(unit->metric_description, "about cpu info.");
//    create_sub_metric_chain(unit);
//    list_add_tail(&(unit->node), head);
//
//    return head;
//}

void metric_info(struct metric_unit *unit)
{
    printf("metric: %s\n", unit->metric_name);
}

void list_metric(void)
{
    struct list_head *pos;
    struct metric_unit *unit;
    list_for_each(pos, &(watcher.metrics_head)) {
        unit = container_of(pos, struct metric_unit, node);
        metric_info(unit);
    }
}

struct metric_unit *make_unit(void)
{
    struct metric_unit *unit = (struct metric_unit *)malloc(sizeof(struct metric_unit));
    memset(unit, 0, sizeof(struct metric_unit));
    INIT_LIST_HEAD(&(unit->node));
    INIT_LIST_HEAD(&(unit->sub_node_head));
    pthread_mutex_init(&(unit->updating), NULL);
    pthread_rwlock_init(&(unit->unit_lock), NULL);
    unit->plugin_id = 0;
    unit->add_sub_metric = _add_sub_metric;
    unit->do_del_metric = _do_del_metric;
    unit->do_del_metric_safely = _do_del_metric_safely;
    unit->run_sub_metric = _run_sub_metric;
    unit->last_update_time = 0;
    unit->expire_time = 0;
    return unit;
}

int32_t _update_data(struct sub_metric_unit *subunit)
{
    subunit->do_update(subunit->data_collection);
    return 0;
}

struct sub_metric_unit *make_subunit(char *name,
        char *description, int32_t run_time,
        item_t *data, int (*update)(item_t *))
{
    struct sub_metric_unit *subunit = (struct sub_metric_unit *)malloc(sizeof(struct sub_metric_unit));
    memset(subunit, 0, sizeof(struct sub_metric_unit));
    INIT_LIST_HEAD(&(subunit->sub_node));
    pthread_rwlock_init(&(subunit->sub_unit_lock), NULL);
    strcpy(subunit->sub_metric_name, name);
    strcpy(subunit->sub_metric_description, description);
    subunit->run_time = run_time;
    subunit->data_collection = data;
    subunit->do_del_sub_metric = _destroy_subunit;
    subunit->do_del_sub_metric_safely = _free_subunit;
    subunit->do_update = update;
    subunit->update_data = _update_data;
    return subunit;
}

void _do_del_metric_safely(struct metric_unit *unit)
{
    //this func may be block, but we should use this;
    struct list_head *pos, *n;
    struct sub_metric_unit *subunit;
    list_for_each_safe(pos, n, &(unit->sub_node_head)) {
        //free all the sub metrics
        subunit = container_of(pos, struct sub_metric_unit, sub_node);
        //printf("  |-free sub metric: %s\n", subunit->sub_metric_name);
        subunit->do_del_sub_metric_safely(subunit);
    }

    pthread_mutex_destroy(&(unit->updating));
    pthread_rwlock_destroy(&(unit->unit_lock));
    free(unit);
    unit = NULL;
}

void _do_del_metric(struct metric_unit *unit)
{
    //we use this only at exit.
    struct list_head *pos, *n;
    struct sub_metric_unit *subunit;
    printf("metric: %s\n", unit->metric_name);
    list_for_each_safe(pos, n, &(unit->sub_node_head)) {
        //free all the sub metrics
        subunit = container_of(pos, struct sub_metric_unit, sub_node);
        printf("  |-sub metric: %s\n", subunit->sub_metric_name);
        subunit->do_del_sub_metric(subunit);
    }

    pthread_mutex_destroy(&(unit->updating));
    pthread_rwlock_destroy(&(unit->unit_lock));
    free(unit);
    unit = NULL;
}

void _free_subunit(struct sub_metric_unit *subunit)
{
    //handler this gently
    pthread_rwlock_destroy(&(subunit->sub_unit_lock));
    //free thread
    free(subunit);
    subunit = NULL;
}

void _destroy_subunit(struct sub_metric_unit *subunit)
{
    pthread_rwlock_destroy(&(subunit->sub_unit_lock));
    free(subunit);
    subunit = NULL;
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

void _traversal_metric_units(void) {
    pthread_create(&(watcher.traversal_thread_id), NULL, do_traversal_metric_units, &(watcher.metrics_head));
    pthread_join(watcher.traversal_thread_id, NULL);
}

int _add_metric(void *watcher, plugin_channel_t *plugin_metrics)
{
    struct syswatcher *_watcher = (struct syswatcher *)watcher;
    struct metric_unit *unit = make_unit();
    int sub_metric_num = plugin_metrics->sub_metric_num;
    int count;
    plugin_sub_channel_t *plugin_sub_metric = plugin_metrics->sub_channel;
    strcpy(unit->metric_name, plugin_metrics->name);
    strcpy(unit->metric_description, plugin_metrics->desc);
    unit->plugin_id = unit->plugin_id;

    for (count = 0; count < sub_metric_num; count++) {
        struct sub_metric_unit *subunit =
                make_subunit((plugin_sub_metric + count)->subname,
                            (plugin_sub_metric + count)->subdesc,
                            (plugin_sub_metric + count)->run_once?1:-1,
                            &(plugin_sub_metric + count)->item,
                            (plugin_sub_metric + count)->collect_data_func);
        unit->add_sub_metric(unit, subunit);
    }
    list_add_tail(&(unit->node), &(_watcher->metrics_head));
    return 0;
}

int _del_metric(void *watcher, plugin_key_t id)
{
    struct list_head *head, *pos, *n;
    struct syswatcher *_watcher = watcher;
    struct metric_unit *unit;
    head = &(_watcher->metrics_head);
    list_for_each_safe(pos, n, head) {
        unit = container_of(pos, struct metric_unit, node);
        if (unit->plugin_id == id) {
            unit->do_del_metric(unit);
        }
    }
    return 0;
}

void init_syswatcher(struct syswatcher *watcher)
{
    INIT_LIST_HEAD(&(watcher->metrics_head));
    watcher->add_metric = _add_metric;
    watcher->del_metric = _del_metric;
    watcher->traversal_metric_units = _traversal_metric_units;
}
