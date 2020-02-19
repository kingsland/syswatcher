#include "plugin_ext_def.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int sysBB_mem_collect(item_t *data)
{
    printf("---*-- pluginBB sys_mem_collect ----*----\n");

    return 0;
}

int sysBB_cpu_collect(item_t *data)
{
    printf("--**-  pluginBB sys_cpu_collect --**------\n");

    return 0;
}

int pluginBB_init(plugin_info_t *plugin_info)
{
    printf("--**--  pluginBB pluginBB_init ----**----\n");
    
    plugin_info->version = PLUGIN_RELEASE_VERSION;
    plugin_info->name = "SystembbResource";
    plugin_info->desc = "collect systembb resource.";
    
    plugin_info->item_count = 2;
    plugin_info->collect_item = (collect_item_t*)malloc(sizeof(collect_item_t)*plugin_info->item_count);

    plugin_info->collect_item[0].item_name = "sysbb-mem";
    plugin_info->collect_item[0].item_desc = "collect memory information on systembb.";
    plugin_info->collect_item[0].run_once = true;
    plugin_info->collect_item[0].collect_data_func = sysBB_mem_collect;
    plugin_info->collect_item[0].interval = 2;
    plugin_info->collect_item[0].data_count = 3;
    
    plugin_info->collect_item[1].item_name = "sysbb-cpu";
    plugin_info->collect_item[1].item_desc = "collect cpu information on systembb.";
    plugin_info->collect_item[1].run_once = false;
    plugin_info->collect_item[1].collect_data_func = sysBB_cpu_collect;
    plugin_info->collect_item[1].interval = 5;
    plugin_info->collect_item[1].data_count = 1;
    
    return 0;
}

void pluginBB_exit(plugin_info_t *plugin_info)
{
    printf("--**--  pluginBB pluginBB_exit ----******----\n");
    
    if (plugin_info->collect_item != NULL) {
        free(plugin_info->collect_item);
        plugin_info->collect_item = NULL;
    }
    plugin_info->item_count = 0;
    
    return;
}

