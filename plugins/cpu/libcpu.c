#include <plugin_def.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <gm_file.h>
#include <cpu_data.h>
char stat_buffer[BUFFSIZE];
char cpuinfo_buffer[BUFFSIZE];
timely_file cpu_stat    = { {0,0} , 0, "/proc/stat", stat_buffer, BUFFSIZE };
timely_file cpuinfo     = { {0,0} , 1, "/proc/cpuinfo", cpuinfo_buffer, BUFFSIZE };
char filebuf[BUFFSIZE];
int cpu_data_collect(item_t *data);
int cpu_spec_collect(item_t *data);
int corenum;
#define DATA_PER_CORE   (4)

collect_item_t items[] = {
    {
        .item_name = "cpu spec",
        .item_desc = "cpu name, core num, and others.",
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

char *find_cpuname() {
    char *info, *cpuname, *cpuname_end;
    info = update_file(&cpuinfo);
    info = strstr(info, "model name");
    if (info == NULL) {
        return NULL;
    }
    cpuname = skip_token(info);
    cpuname = skip_token(cpuname);
    cpuname = skip_token(cpuname);
    cpuname = skip_whitespace(cpuname);
    cpuname_end = strchr(cpuname, '\n');
    *cpuname_end = '\0';
    return cpuname;
}

int cpu_spec_collect(item_t *data)
{
#define CPUNAME_SIZE    (128)
    item_t *data_set = data;
    char *stat;
    char *cpuname;
    stat = update_file(&cpu_stat);

    cpuname = find_cpuname();
    data_set->element_num = 2;
    sprintf(data_set->data[0].name, "cpu name");
    sprintf(data_set->data[0].unit, UNIT_NA);
    data_set->data[0].t = M_STRING;
    sprintf(data_set->data[0].val.str, "%s", (cpuname == NULL)?"unknow":cpuname);

    sprintf(data_set->data[1].name, "cpu num");
    sprintf(data_set->data[1].unit, UNIT_NA);
    data_set->data[1].t = M_INT32;
    data_set->data[1].val.int32 = get_core_num(stat);

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
    int core_idx = 0;
    static uint64_t run_count = 0;
    val_t cpu_idle, cpu_usage, cpu_sys, cpu_user, cpu_iowait;
    char tmp[16], name[DATA_NAME_LENGTH];

    p = update_file(&cpu_stat);
    mate_t *data_set = data->data;
    data->element_num = items[1].data_count;
    for (;core_idx < (corenum + 1); core_idx++) {
        cpu_idle.f = cpu_core_usage(core_idx, p);
        cpu_usage.f = 100.0 - cpu_idle.f;
        cpu_user.f = core_user_usage(core_idx, p);
        cpu_sys.f = core_sys_usage(core_idx, p);
        cpu_iowait.f = core_iowait_usage(core_idx, p);
        if (core_idx != 0) {
            sprintf(tmp, "cpu%d", core_idx);
        } else {
            sprintf(tmp, "cpu total");
        }
        if (run_count != 0) {
            int count = 0;
            sprintf(name, "%s usage", tmp);
            set_data_collect(&(data_set[core_idx*(DATA_PER_CORE) + count++]), name, "%", M_FLOAT, cpu_usage);

            sprintf(name, "%s user", tmp);
            set_data_collect(&(data_set[core_idx*(DATA_PER_CORE) + count++]), name, "%", M_FLOAT, cpu_user);

            sprintf(name, "%s system", tmp);
            set_data_collect(&(data_set[core_idx*(DATA_PER_CORE) + count++]), name, "%", M_FLOAT, cpu_sys);

            sprintf(name, "%s iowait", tmp);
            set_data_collect(&(data_set[core_idx*(DATA_PER_CORE) + count++]), name, "%", M_FLOAT, cpu_iowait);
        }
    }
    run_count++;
    return 0;
}

PLUGIN_ENTRY(cpu, plugin_info)
{
    char *stat;
    stat = update_file(&cpu_stat);
    corenum = get_core_num(stat);
    num_cpustates = num_cpustates_func(stat);
    items[1].data_count = (corenum + 1) * (DATA_PER_CORE);

    PLUGIN_INIT(plugin_info, items, &pluginfo);
    return 0;
}

PLUGIN_EXIT(cpu, plugin_info)
{
    PLUGIN_FREE(plugin_info);
    return;
}
