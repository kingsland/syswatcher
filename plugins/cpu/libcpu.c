#include <plugin_ext_def.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int cpu_spec_collect(item_t *data)
{
    plugin_print_log("%s %d\n", __func__, __LINE__);
    return 0;
}

collect_item_t items[] = {
    {
        .item_name = "cpu spec",
        .item_desc = "cpu name, Hz, core num, and others.",
        .run_once = false,
        .collect_data_func = cpu_spec_collect,
        .interval = 1,
        .data_count = 3,
    },
};

plugin_info_t pluginfo = {
    .name = "cpu misc",
    .desc = "all information about cpu",
    .item_count = 1,
};

PLUGIN_ENTRY(cpu, plugin_info)
{
    PLUGIN_INIT(plugin_info, items, &pluginfo);
    return 0;
}

PLUGIN_EXIT(cpu, plugin_info)
{
    //PLUGIN_FREE(plugin_info);
    return;
}
