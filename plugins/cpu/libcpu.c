#include <plugin_ext_def.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <data_collect.h>

int cpu_data_collect(item_t *data);
int cpu_spec_collect(item_t *data);
int corenum;

collect_item_t items[] = {
    {
        .item_name = "cpu spec",
        .item_desc = "cpu name, Hz, core num, and others.",
        .run_once = true,
        .collect_data_func = cpu_spec_collect,
        .interval = 1,
        .data_count = 2,
    },
    {
        .item_name = "cpu usage",
        .item_desc = "cpu usage, sys user iowait...",
        .run_once = true,
        .collect_data_func = cpu_data_collect,
        .interval = 1,
    },
};

plugin_info_t pluginfo = {
    .name = "cpu misc",
    .desc = "all information about cpu",
    .item_count = 2,
};


int cpu_spec_collect(item_t *data)
{
    item_t *data_set = data;
    data_set->elememt_num = 2;
    sprintf(data_set->data[0].name, "cpu name");
    data_set->data[0].unit[0] = '\0';
    data_set->data[0].t = M_STRING;
    sprintf(data_set->data[0].val.str, "intel cpu");

    sprintf(data_set->data[1].name, "cpu frequency");
    sprintf(data_set->data[1].unit, "GHz");
    data_set->data[1].t = M_FLOAT;
    data_set->data[1].val.f = 3.4;

    return 0;
}

void set_data_collect(mate_t *data_set, char *name,
        char *unit, enum data_type t, union val_t v)
{
}

int cpu_data_collect(item_t *data)
{
    int data_idx = 0;
    val_t cpu_idle;
    mate_t *data_set = data->data;
    data->elememt_num = items[1].data_count;
    
    cpu_idle.f = cpu_idle_func();
    set_data_collect(&(data_set[data_idx]), "cpu usage", "%%", M_FLOAT, cpu_idle);
    printf("cpu_idle: %f\n", cpu_idle.f);

//    data_idx++;
//    for (; data_idx < data->elememt_num; data_idx++)
//    {
//        char name[16];
//        int core_idx = data_idx - 1;
//        sprintf(name, "cpu%d usage", data_idx);
//        cpu_idle.f = cpu_idle_func(core_idx);
//        set_data_collect(&(data_set[data_idx]), name, "%%", M_FLOAT, cpu_idle);
//        //printf("cpu%d_idle: %f\n", core_idx, cpu_idle.f);
//    }

    return 0;
}

PLUGIN_ENTRY(cpu, plugin_info)
{
    corenum = get_core_num();
    items[1].data_count = corenum + 1;
    PLUGIN_INIT(plugin_info, items, &pluginfo);
    return 0;
}

PLUGIN_EXIT(cpu, plugin_info)
{
    PLUGIN_FREE(plugin_info);
    return;
}
