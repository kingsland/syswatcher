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
        .run_once = false,
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
    sprintf(data_set->name, "%s", name);
    sprintf(data_set->unit, "%s", unit);
    data_set->t = t;
    data_set->val = v;
}

int send_data(item_t *data);

int cpu_data_collect(item_t *data)
{
    int data_idx = 0;
    val_t cpu_idle, cpu_usage;
    char name[16];
    mate_t *data_set = data->data;
    data->elememt_num = items[1].data_count;
    //FIXME
    //rewrite this, data is unstable.
    //don't trust this api.
    for (;data_idx < data->elememt_num; data_idx++) {
        cpu_idle.f = cpu_core_usage(data_idx);
        cpu_usage.f = 100.0 - cpu_idle.f;
        if (data_idx != 0) {
            sprintf(name, "cpu%d usage", data_idx);
        } else {
            sprintf(name, "cpu usage");
        }
        set_data_collect(&(data_set[data_idx]), name, "%", M_FLOAT, cpu_usage);
    }
    for (data_idx = 0; data_idx < data->elememt_num; data_idx++) {
        printf("%s: %f%s\n", data_set[data_idx].name, data_set[data_idx].val.f, data_set[data_idx].unit);
    }
    
    send_data(data);

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
