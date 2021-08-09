#include <metric.h>
#include <log.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <cJSON.h>
#include <fifo.h>
struct syswatcher watcher;
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

struct fifo_node *make_action(struct sub_metric_unit *subunit)
{
    INIT_FIFO_NODE(action_node);
    action_node->priv = (void *)subunit;
    return action_node;
}

int del_action(struct fifo_node *action)
{
    DESTROY_FIFO_NODE(action);
    return 0;
}

int do_run_sub_metric(struct metric_unit *unit)
{
    struct list_head *head = &(unit->sub_node_head);
    struct list_head *pos;
    struct sub_metric_unit *subunit;
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
            struct fifo_node *action;
            struct fifo_head *fifo = unit->update_thread_info->action_fifo;
            subunit->reset_time_ring(subunit);
            action = make_action(subunit);
            fifo->push(fifo, action);
        }
        pthread_mutex_unlock(&(subunit->sub_unit_lock));
    }
    return 0;
}

void *update_thread(void *arg)
{
    struct thread_info *ti = (struct thread_info *)arg;
    struct fifo_head *fifo = ti->action_fifo;
    struct fifo_node *action;
    struct sub_metric_unit *subunit;
    while(ti->thread_running)
    {
        action = fifo->pop(fifo);
        //FIXME change to block mode;
        if (action == NULL) {
            usleep(10000);
        } else {
            subunit = (struct sub_metric_unit *)action->priv;
            subunit->do_update(subunit->data_collection);
            del_action(action);
        }
    }
    pthread_exit(NULL);
}

struct thread_info *make_ti(struct metric_unit *unit)
{
    struct thread_info *(ti) = (struct thread_info *)malloc(sizeof(struct thread_info));
    pthread_mutex_init(&(ti->ti_mtx), NULL);
    ti->thread_running = 1;
    ti->action_fifo = init_fifo();
    pthread_create(&(ti->id), NULL, update_thread, ti);
    return ti;
}

int destroy_ti(struct thread_info *ti)
{
    ti->thread_running = 0;
    pthread_join(ti->id, NULL);
    del_fifo(ti->action_fifo);
    free(ti);
    ti = NULL;
    return 0;
}

void _run_sub_metric(struct metric_unit *unit)
{
    int trigger = 0;
    pthread_mutex_lock(&(unit->unit_lock));
    trigger = unit->time_ring_move_forward(unit);
    if (trigger) {
        do_run_sub_metric(unit);
    }
    pthread_mutex_unlock(&(unit->unit_lock));
}

void _add_sub_metric(struct metric_unit *unit, struct sub_metric_unit *subunit)
{
    list_add_tail(&(subunit->sub_node), &(unit->sub_node_head));
}

void print_val(data_type t, val_t val, char *val_str)
{
    switch (t) {
        case M_INT8:
            sprintf(val_str, "%d", val.int8);
            break;
        case M_INT16:
            sprintf(val_str, "%d", val.int16);
            break;
        case M_INT32:
            sprintf(val_str, "%d", val.int32);
            break;
        case M_INT64:
            sprintf(val_str, "%ld", val.int64);
            break;
        case M_UINT8:
            sprintf(val_str, "%u", val.uint8);
            break;
        case M_UINT16:
            sprintf(val_str, "%u", val.uint16);
            break;
        case M_UINT32:
            sprintf(val_str, "%u", val.uint32);
            break;
        case M_UINT64:
            sprintf(val_str, "%lu", val.uint64);
            break;
        case M_FLOAT:
            sprintf(val_str, "%f", val.f);
            break;
        case M_DOUBLE:
            sprintf(val_str, "%f", val.d);
            break;
        case M_STRING:
            sprintf(val_str, "%s", val.str);
        default:
            break;
    }
}

void metric_info(struct metric_unit *unit, char *metric_data, cJSON *root)
{
    struct list_head *pos;
    struct sub_metric_unit *subunit;
    char tmpstr[MAX_STRING_SIZE*2];
    cJSON * metric =  cJSON_CreateObject();
    sprintf(tmpstr, "metric: %s\n", unit->metric_name);
    cJSON_AddItemToObject(root, unit->metric_name, metric);
    cJSON_AddItemToObject(metric, "description", cJSON_CreateString(unit->metric_description));
    strncat(metric_data, tmpstr, strlen(tmpstr));

    list_for_each(pos, &(unit->sub_node_head)) {
        subunit = container_of(pos, struct sub_metric_unit, sub_node);
        cJSON *submetric = cJSON_CreateObject();
        cJSON_AddItemToObject(metric, subunit->sub_metric_name, submetric);
        cJSON_AddItemToObject(submetric, "description", cJSON_CreateString(subunit->sub_metric_description));
        sprintf(tmpstr, "    |--sub metric: %s\n", subunit->sub_metric_name);
        strncat(metric_data, tmpstr, strlen(tmpstr));
        item_t *data = subunit->data_collection;
        int count;
        char val_str[MAX_STRING_SIZE];
        for (count = 0; count < data->element_num; count++) {
            sprintf(tmpstr, "        |--%s ", data->data[count].name);
            strncat(metric_data, tmpstr, strlen(tmpstr));
            print_val(data->data[count].t, data->data[count].val, val_str);
            sprintf(tmpstr, "%s%s\n", val_str, strcmp(data->data[count].unit, UNIT_NA)?data->data[count].unit:"");
            strncat(metric_data, tmpstr, strlen(tmpstr));
            cJSON *data_item = cJSON_CreateObject();
            cJSON_AddItemToObject(submetric, data->data[count].name, data_item);
            cJSON_AddItemToObject(data_item, "value", cJSON_CreateString(val_str));
            cJSON_AddItemToObject(data_item, "unit", cJSON_CreateString(data->data[count].unit));
        }
    }
}

void *list_data(void *arg)
{
    struct list_head *pos;
    struct metric_unit *unit;
    struct data_collector *collector;
    char visual_data[COLLECTOR_BUFFER];
    char metric_data[METRIC_DATA_BUFFER];
    char *json_str;
    collector = &(watcher.collector);
    while(1) {
        sleep(1);
        bzero(visual_data, COLLECTOR_BUFFER);
        cJSON * root =  cJSON_CreateObject();
        pthread_rwlock_rdlock(&(watcher.plugin_lock));
        pthread_rwlock_wrlock(&(collector->data_lock));
        list_for_each(pos, &(watcher.metrics_head)) {
            unit = container_of(pos, struct metric_unit, node);
            bzero(metric_data, METRIC_DATA_BUFFER);
            metric_info(unit, metric_data, root);
            strncat(visual_data, metric_data, strlen(metric_data));
        }
        pthread_rwlock_unlock(&(collector->data_lock));
        pthread_rwlock_unlock(&(watcher.plugin_lock));
        sprintf(watcher.collector.visual_data, "%s", visual_data);
        json_str = cJSON_Print(root);
        sprintf(watcher.collector.json_data, "%s", json_str);
        free(json_str);
        cJSON_Delete(root);
    }
    return NULL;
}

struct metric_unit *make_unit(void)
{
    struct metric_unit *unit = (struct metric_unit *)malloc(sizeof(struct metric_unit));
    memset(unit, 0, sizeof(struct metric_unit));
    INIT_LIST_HEAD(&(unit->node));
    INIT_LIST_HEAD(&(unit->sub_node_head));
    pthread_mutex_init(&(unit->unit_lock), NULL);
    unit->update_thread_info = NULL;
    unit->plugin_id = 0;
    unit->add_sub_metric = _add_sub_metric;
    unit->do_del_metric = _do_del_metric;
    unit->run_sub_metric = _run_sub_metric;
    unit->time_ring_move_forward = _time_ring_move_forward;
    unit->last_update_time = 0;
    unit->expire_time = 0;
    return unit;
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
    return subunit;
}

void _do_del_metric(struct metric_unit *unit)
{
    struct list_head *pos, *n;
    struct sub_metric_unit *subunit;
    logging(LEVEL_INFO, "delete metric: %s\n", unit->metric_name);

    //take off the node
    //and do the reset of things.
    pthread_mutex_lock(&(unit->unit_lock));
    list_del(&(unit->node));
    destroy_ti(unit->update_thread_info);
    list_for_each_safe(pos, n, &(unit->sub_node_head)) {
        subunit = container_of(pos, struct sub_metric_unit, sub_node);
        printf("sub metric%s\n", subunit->sub_metric_name);
        logging(LEVEL_INFO, "  |-sub metric: %s\n", subunit->sub_metric_name);
        subunit->do_del_sub_metric(subunit);
    }
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
    struct syswatcher *_watcher = (struct syswatcher *)arg;
    struct list_head *head = &(_watcher->metrics_head);
    while(1) {
        //we traversal the time ring here.
        struct list_head *pos;
        struct metric_unit *unit;
        pthread_rwlock_rdlock(&(_watcher->plugin_lock));
        list_for_each(pos, head) {
            unit = container_of(pos, struct metric_unit, node);
            if (unit != NULL) {
                if (!unit->exiting) {
                    unit->run_sub_metric(unit);
                }
            }
        }
        pthread_rwlock_unlock(&(_watcher->plugin_lock));
        sleep(RING_STEP);
    }
}

void _traversal_metric_units(struct syswatcher *watcher)
{
    pthread_create(&(watcher->traversal_thread_id), NULL, do_traversal_metric_units, watcher);
    pthread_join(watcher->traversal_thread_id, NULL);
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
    unit->exiting = 0;
    logging(LEVEL_INFO, "add metric: %s\n", unit->metric_name);
    for (count = 0; count < sub_metric_num; count++) {
        struct sub_metric_unit *subunit =
                make_subunit((plugin_sub_metric + count)->subname,
                            (plugin_sub_metric + count)->subdesc,
                            (plugin_sub_metric + count)->run_once?1:-1,
                            (plugin_sub_metric + count)->interval,
                            &(plugin_sub_metric + count)->item,
                            (plugin_sub_metric + count)->collect_data_func);
        logging(LEVEL_INFO, "  |--sub metric: %s\n", subunit->sub_metric_name);
        unit->add_sub_metric(unit, subunit);
    }
    unit->update_thread_info = make_ti(unit);
    pthread_rwlock_wrlock(&(_watcher->plugin_lock));
    list_add_tail(&(unit->node), &(_watcher->metrics_head));
    pthread_rwlock_unlock(&(_watcher->plugin_lock));
    return 0;
}

int _del_metric(void *watcher, plugin_key_t id)
{
    struct list_head *head, *pos, *n;
    struct syswatcher *_watcher = watcher;
    struct metric_unit *unit;
    head = &(_watcher->metrics_head);
    pthread_rwlock_wrlock(&(_watcher->plugin_lock));
    list_for_each_safe(pos, n, head) {
        unit = container_of(pos, struct metric_unit, node);
        if (unit->plugin_id == id) {
            unit->exiting = 1;
            unit->do_del_metric(unit);
        }
    }
    pthread_rwlock_unlock(&(_watcher->plugin_lock));
    return 0;
}

/*
 * return value: step left.
 */
int _go_one_step(struct sub_metric_unit *subunit)
{
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
    watcher->add_metric = _add_metric;
    watcher->del_metric = _del_metric;
    watcher->traversal_metric_units = _traversal_metric_units;
    pthread_rwlock_init(&(watcher->plugin_lock), NULL);
    init_collector(&(watcher->collector), list_data);
}

void exit_syswatcher(struct syswatcher *watcher)
{
    pthread_rwlock_destroy(&(watcher->plugin_lock));
}
