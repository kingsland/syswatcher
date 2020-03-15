#include <plugin_ext_def.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <gm_file.h>
#include <cpu_data.h>

timely_file cpu_stat    = { {0,0} , 1., "/proc/stat", NULL, BUFFSIZE };
char filebuf[BUFFSIZE];
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
    char *p;
    int data_idx = 0;
    static uint64_t run_count = 0;
    val_t cpu_idle, cpu_usage, cpu_sys, cpu_user, cpu_iowait;
    char tmp[16], name[16];

    p = update_file(&cpu_stat);
    mate_t *data_set = data->data;
    data->elememt_num = items[1].data_count;
    for (;data_idx < data->elememt_num; data_idx++) {
        cpu_idle.f = cpu_core_usage(data_idx, p);
        cpu_usage.f = 100.0 - cpu_idle.f;
        cpu_user.f = core_user_usage(data_idx, p);
        cpu_sys.f = core_sys_usage(data_idx, p);
        cpu_iowait.f = core_iowait_usage(data_idx, p);
        if (data_idx != 0) {
            sprintf(tmp, "cpu%d", data_idx);
        } else {
            sprintf(tmp, "cpu");
        }
        if (run_count != 0) {
            sprintf(name, "%s usage", tmp);
            set_data_collect(&(data_set[data_idx]), name, "%", M_FLOAT, cpu_usage);
            //plugin_print_log("%s:%f\n", name, cpu_usage.f);

            sprintf(name, "%s user", tmp);
            set_data_collect(&(data_set[data_idx]), name, "%", M_FLOAT, cpu_user);
            //plugin_print_log("%s:%f\n", name, cpu_user.f);

            sprintf(name, "%s system", tmp);
            set_data_collect(&(data_set[data_idx]), name, "%", M_FLOAT, cpu_sys);
            //plugin_print_log("%s:%f\n", name, cpu_sys.f);

            sprintf(name, "%s iowait", tmp);
            set_data_collect(&(data_set[data_idx]), name, "%", M_FLOAT, cpu_iowait);
            //plugin_print_log("%s:%f\n", name, cpu_iowait.f);
        }
    }
    run_count++;
}

PLUGIN_ENTRY(cpu, plugin_info)
{
    char *stat;
    stat = update_file(&cpu_stat);
    corenum = get_core_num(stat);
    num_cpustates = num_cpustates_func(stat);
    items[1].data_count = corenum + 1;

    PLUGIN_INIT(plugin_info, items, &pluginfo);
    return 0;
}

PLUGIN_EXIT(cpu, plugin_info)
{
    PLUGIN_FREE(plugin_info);
    return;
}
