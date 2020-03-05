#include <metric.h>
#include <log.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
struct metric_unit *make_unit(void);
struct sub_metric_unit *make_subunit(char *name,
        char *description, int32_t run_time, time_t interval,
        item_t *data, int (*update)(item_t *));
void free_subunit(struct sub_metric_unit *subunit);
void _add_sub_metric(struct metric_unit *unit, struct sub_metric_unit *subunit);
void _do_del_metric(struct metric_unit *unit);
void _destroy_subunit(struct sub_metric_unit *subunit);
int _go_one_step(struct sub_metric_unit *subunit);
int _time_ring_move_forward(struct metric_unit *unit);
void _reset_time_ring(struct sub_metric_unit *subunit);

void _reset_time_ring(struct sub_metric_unit *subunit)
{
    subunit->time_ring_left = subunit->interval;
}
void *do_run_sub_metric(void *arg)
{
    struct metric_unit *unit = (struct metric_unit *)arg;
    struct thread_info *ti = unit->update_thread;
    struct list_head *head = &(unit->sub_node_head);
    struct list_head *pos;
    struct sub_metric_unit *subunit;

    pthread_detach(pthread_self());
    list_for_each(pos, head) {
        subunit = container_of(pos, struct sub_metric_unit, sub_node);
        if (subunit->run_time == 0) {
            continue;
        }
        pthread_mutex_lock(&(subunit->sub_unit_lock));
        if (subunit->run_time != -1) {
            subunit->run_time--;
        }
        if (subunit->time_ring_left == 0) {
            subunit->reset_time_ring(subunit);
            if (subunit->update_data != NULL) {
                subunit->update_data(subunit);
            }
        }
        pthread_mutex_unlock(&(subunit->sub_unit_lock));
    }

    pthread_mutex_unlock(&(ti->updating));
    pthread_exit(NULL);
}

struct thread_info *make_ti(struct metric_unit *unit)
{
    struct thread_info *(ti) = (struct thread_info *)malloc(sizeof(struct thread_info));
    ti->id = 0;
    ti->unit = unit;
    pthread_mutex_init(&(ti->updating), NULL);
    pthread_mutex_lock(&(ti->updating));
    INIT_LIST_HEAD(&(ti->node));
    return ti;
}

void _run_sub_metric(struct metric_unit *unit)
{
    int ret;
    struct thread_info *ti;
    int trigger = 0;
    pthread_mutex_lock(&(unit->unit_lock));
    trigger = unit->time_ring_move_forward(unit);
    if (trigger) {
        if (unit->update_thread == NULL) {
            ti = make_ti(unit);
            unit->update_thread = ti;
            pthread_create(&(ti->id), NULL, do_run_sub_metric, unit);
            list_add_tail(&(ti->node), &(watcher.thread_pool));
            unit->last_update_time = time(NULL);
        } else {
            logging(LEVEL_WARN, "not finish, skip %s\n", unit->metric_name);
        }
    }
    pthread_mutex_unlock(&(unit->unit_lock));
}

void _add_sub_metric(struct metric_unit *unit, struct sub_metric_unit *subunit)
{
    list_add_tail(&(subunit->sub_node), &(unit->sub_node_head));
}

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
    pthread_mutex_init(&(unit->unit_lock), NULL);
    unit->update_thread = NULL;
    unit->plugin_id = 0;
    unit->add_sub_metric = _add_sub_metric;
    unit->do_del_metric = _do_del_metric;
    unit->run_sub_metric = _run_sub_metric;
    unit->time_ring_move_forward = _time_ring_move_forward;
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
        char *description, int32_t run_time, time_t interval,
        item_t *data, int (*update)(item_t *))
{
    struct sub_metric_unit *subunit = (struct sub_metric_unit *)malloc(sizeof(struct sub_metric_unit));
    memset(subunit, 0, sizeof(struct sub_metric_unit));
    INIT_LIST_HEAD(&(subunit->sub_node));
    pthread_mutex_init(&(subunit->sub_unit_lock), NULL);
    strcpy(subunit->sub_metric_name, name);
    strcpy(subunit->sub_metric_description, description);
    subunit->run_time = run_time;
    subunit->interval = interval;
    subunit->time_ring_left = interval;
    subunit->reset_time_ring = _reset_time_ring;
    subunit->data_collection = data;
    subunit->do_del_sub_metric = _destroy_subunit;
    subunit->do_update = update;
    subunit->go_one_step = _go_one_step;
    subunit->update_data = _update_data;
    return subunit;
}

void _do_del_metric(struct metric_unit *unit)
{
    //we use this only at exit.
    struct list_head *pos, *n;
    struct sub_metric_unit *subunit;
    logging(LEVEL_INFO, "delete metric: %s\n", unit->metric_name);
    //waiting for thread exit

    pthread_mutex_lock(&(unit->unit_lock));
    list_del(&(unit->node));
    list_for_each_safe(pos, n, &(unit->sub_node_head)) {
        //free all the sub metrics
        subunit = container_of(pos, struct sub_metric_unit, sub_node);
        logging(LEVEL_INFO, "  |-sub metric: %s\n", subunit->sub_metric_name);
        subunit->do_del_sub_metric(subunit);
    }
    unit->update_thread = NULL;
    pthread_mutex_unlock(&(unit->unit_lock));
    pthread_mutex_destroy(&(unit->unit_lock));
    free(unit);
    unit = NULL;
}

void _destroy_subunit(struct sub_metric_unit *subunit)
{
    pthread_mutex_destroy(&(subunit->sub_unit_lock));
    list_del(&(subunit->sub_node));
    free(subunit);
    subunit = NULL;
}

void *do_traversal_metric_units(void *arg) {
    struct list_head *head = (struct list_head *)arg;
    while(1) {
        //we traversal the time ring here.
        struct list_head *pos;
        struct metric_unit *unit;
        list_for_each(pos, head) {
            unit = container_of(pos, struct metric_unit, node);
            unit->run_sub_metric(unit);
        }

        sleep(RING_STEP);
    }
}

void _traversal_metric_units(struct syswatcher *watcher)
{
    pthread_create(&(watcher->traversal_thread_id), NULL, do_traversal_metric_units, &(watcher->metrics_head));
    pthread_join(watcher->traversal_thread_id, NULL);
}

//FIXME
//重做线程管理，这里只能用大锁，消耗太大。
void *do_thread_recycle(void *arg)
{
    struct syswatcher *watcher = (struct syswatcher *)arg;
    struct list_head *head = &(watcher->metrics_head);
    struct list_head *pos;
    int ret;
    while(1) {
        struct thread_info *ti;
        struct metric_unit *unit;

        pthread_mutex_lock(&(watcher->plugin_lock));
        list_for_each(pos, head) {
            unit = container_of(pos, struct metric_unit, node);
            ti = unit->update_thread;
            if (ti == NULL) {
                continue;
            }
            pthread_mutex_lock(&(unit->unit_lock));
            ret = pthread_mutex_trylock(&(ti->updating));
            if (ret == 0) {
                pthread_mutex_unlock(&(ti->updating));
                unit->update_thread = NULL;
                //pthread_join(ti->id, NULL);
                //replace with detach
                pthread_mutex_destroy(&(ti->updating));
                free(ti);
            }
            pthread_mutex_unlock(&(unit->unit_lock));
        }
        pthread_mutex_unlock(&(watcher->plugin_lock));
        usleep(100000);
    }
}

void _thread_recycle(struct syswatcher *watcher)
{
    pthread_create(&(watcher->recycle_thread_id), NULL, do_thread_recycle, watcher);
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
    unit->plugin_id = plugin_metrics->plugin_id;

    for (count = 0; count < sub_metric_num; count++) {
        struct sub_metric_unit *subunit =
                make_subunit((plugin_sub_metric + count)->subname,
                            (plugin_sub_metric + count)->subdesc,
                            (plugin_sub_metric + count)->run_once?1:-1,
                            (plugin_sub_metric + count)->interval,
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
    pthread_mutex_lock(&(_watcher->plugin_lock));
    list_for_each_safe(pos, n, head) {
        unit = container_of(pos, struct metric_unit, node);
        if (unit->plugin_id == id) {
            unit->do_del_metric(unit);
        }
    }
    pthread_mutex_unlock(&(_watcher->plugin_lock));
    return 0;
}

/*
 * return value: step left.
 */
int _go_one_step(struct sub_metric_unit *subunit)
{
    time_t time_left;
    if (subunit->time_ring_left != 0) {
        subunit->time_ring_left --;
    }
    return subunit->time_ring_left;
}

int _time_ring_move_forward(struct metric_unit *unit)
{
    struct list_head *head, *pos;
    struct sub_metric_unit *subunit;
    int trigger = 0;
    head = &(unit->sub_node_head);
    list_for_each(pos, head) {
        subunit = container_of(pos, struct sub_metric_unit, sub_node);
        if (subunit->run_time != 0) {
            if (subunit->go_one_step(subunit) == 0) {
                trigger = 1;
            }
        }
    }
    return trigger;
}

void init_syswatcher(struct syswatcher *watcher)
{
    INIT_LIST_HEAD(&(watcher->metrics_head));
    INIT_LIST_HEAD(&(watcher->thread_pool));
    watcher->add_metric = _add_metric;
    watcher->del_metric = _del_metric;
    watcher->traversal_metric_units = _traversal_metric_units;
    watcher->thread_recycle = _thread_recycle;
    pthread_mutex_init(&(watcher->plugin_lock), NULL);
}
