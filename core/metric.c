#include <metric.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
struct metric_unit *make_unit(void);
struct sub_metric_unit *make_subunit(char *name,
        char *description, int32_t run_time,
        enum data_type t, uint32_t size, char *unit,
        void (*update)(void *));
void *do_run_sub_metric(void *arg)
{
    struct metric_unit *unit = (struct metric_unit *)arg;
    struct list_head *head = &(unit->sub_node_head);
    pthread_mutex_lock(&(unit->updating));
    struct list_head *pos;
    struct sub_metric_unit *subunit;
    list_for_each(pos, head) {
        subunit = container_of(pos, struct sub_metric_unit, sub_node);
        if (subunit->update_data != NULL) {
            subunit->update_data(subunit);
        }
    }

    pthread_mutex_unlock(&(unit->updating));
    return NULL;
}

void _run_sub_metric(struct metric_unit *unit)
{
    int ret;
    pthread_rwlock_wrlock(&(unit->unit_lock));
    ret = pthread_mutex_trylock(&(unit->updating));
    if (ret == 0) {
        pthread_t id;
        pthread_mutex_unlock(&(unit->updating));
        unit->last_update_time = time(NULL);
        pthread_create(&id, NULL, do_run_sub_metric, unit);
    } else {
        printf("not finish, skip %s\n", unit->metric_name);
    }
    pthread_rwlock_unlock(&(unit->unit_lock));
}

void _add_sub_metric(struct metric_unit *unit, struct sub_metric_unit *subunit)
{
    list_add_tail(&(subunit->sub_node), &(unit->sub_node_head));
}

void cpu_name_update(void *data) {
    strcpy(data, "intel x86");
    printf("%s %d\n", __func__, __LINE__);
}

void cpu_freq_update(void *data) {
    float freq = 3.4;
    memcpy(data, &freq, sizeof(freq));
    printf("%s %d\n", __func__, __LINE__);
}

void create_sub_metric_chain(struct metric_unit *unit)
{
    struct sub_metric_unit *subunit;
    char *name = "cpu_name";
    char *desc = "the cpu name of /proc/cpuinfo";
    subunit = make_subunit(name, desc, -1, M_STRING, 64, NULL, cpu_name_update);
    unit->add_sub_metric(unit, subunit);
    name = "cpu_freq";
    desc = "cpu  frequency";
    subunit = make_subunit(name, desc, -1, M_FLOAT, sizeof(float), "GHz", cpu_freq_update);
    unit->add_sub_metric(unit, subunit);
}

struct list_head *create_metrics_chain(void)
{
    int count = 4;
    struct metric_unit *unit;
    NEW_LIST_NODE(head);
    for (; count > 0; count--) {
        unit = make_unit();
        sprintf(unit->metric_name, "metric%d", count);
        sprintf(unit->metric_description, "this is metric%d, and this is for test;", count);
        list_add_tail(&(unit->node), head);
    }
    unit = make_unit();
    sprintf(unit->metric_name, "cpu misc");
    sprintf(unit->metric_description, "about cpu info.");
    create_sub_metric_chain(unit);
    list_add_tail(&(unit->node), head);
    return head;
}

void metric_info(struct metric_unit *unit)
{
    printf("run %s: ", unit->metric_name);
    printf("%s\n\n", unit->metric_description);
}

void list_metric(void)
{
    struct list_head *pos;
    struct metric_unit *unit;
    list_for_each(pos, metrics_head) {
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
    unit->add_sub_metric = _add_sub_metric;
    unit->run_sub_metric = _run_sub_metric;
    unit->last_update_time = 0;
    unit->expire_time = 0;
    return unit;
}

int32_t _update_data(struct sub_metric_unit *subunit)
{
    subunit->do_update(subunit->data);
    return 0;
}

struct sub_metric_unit *make_subunit(char *name,
        char *description, int32_t run_time,
        enum data_type t, uint32_t size, char *unit,
        void (*update)(void *))
{
    struct sub_metric_unit *subunit = (struct sub_metric_unit *)malloc(sizeof(struct sub_metric_unit) + size);
    memset(subunit, 0, sizeof(struct sub_metric_unit));
    INIT_LIST_HEAD(&(subunit->sub_node));
    pthread_rwlock_init(&(subunit->sub_unit_lock), NULL);
    strcpy(subunit->sub_metric_name, name);
    strcpy(subunit->sub_metric_description, description);
    subunit->run_time = run_time;
    subunit->t = t;
    subunit->size = size;
    if (unit != NULL) {
        strcpy(subunit->unit, unit);
    }
    subunit->do_update = update;
    subunit->update_data = _update_data;
    return subunit;
}

void destroy_unit(struct metric_unit *unit)
{
    //delete all subunit
    pthread_mutex_destroy(&(unit->updating));
    pthread_rwlock_destroy(&(unit->unit_lock));
    free(unit);
    unit = NULL;
}

void destroy_subunit(struct sub_metric_unit *subunit) {
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

void traversal_metric_units(void) {
    pthread_create(&traversal_thread_id, NULL, do_traversal_metric_units, metrics_head);
    pthread_join(traversal_thread_id, NULL);
}
