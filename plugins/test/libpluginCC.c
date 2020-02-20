#include "plugin_ext_def.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define desc()  for (int

int sysCC_mem_collect(item_t *data)
{
    printf("----#---- pluginCC sys_mem_collect ----#-------\n");

    return 0;
}

int sysCC_cpu_collect(item_t *data)
{
    printf("----#---  pluginCC sys_cpu_collect -----#------\n");

    char* o[] = {"sys", "user", "idle"};
    /*
    data->obj[0].name = 
    data->obj[0].unit = 
    data->obj[0].type = 
    data->obj[0].value
    
    data->obj[0].name = 
    data->obj[0].unit = 
    data->obj[0].type = 

    
    data->obj[0].value = */
    
    return 0;
}

int pluginCC_init(plugin_info_t *plugin_info)
{
    printf("----###  pluginCC pluginCC_init -###-------\n");
    
    
    plugin_info->version = PLUGIN_RELEASE_VERSION;
    plugin_info->name = "SystemccResource";
    plugin_info->desc = "collect systemcc resource.";
    
    plugin_info->item_count = 2;
    plugin_info->collect_item = (collect_item_t*)malloc(sizeof(collect_item_t)*plugin_info->item_count);

    plugin_info->collect_item[0].item_name = "syscc-mem";
    plugin_info->collect_item[0].item_desc = "collect memory information on systemcc.";
    plugin_info->collect_item[0].run_once = true;
    plugin_info->collect_item[0].collect_data_func = sysCC_mem_collect;
    plugin_info->collect_item[0].interval = 2;
    plugin_info->collect_item[0].data_count = 3;
    
    plugin_info->collect_item[1].item_name = "syscc-cpu";
    plugin_info->collect_item[1].item_desc = "collect cpu information on systemcc.";
    plugin_info->collect_item[1].run_once = false;
    plugin_info->collect_item[1].collect_data_func = sysCC_cpu_collect;
    plugin_info->collect_item[1].interval = 5;
    plugin_info->collect_item[1].data_count = 1;
    
/*
    add_plugin_info(plugin_info, "SystemccResource", "collect systemcc resource.", 2);
    
    add_collect_info_by_index(plugin_info, 0, "syscc-mem", "collect memory information on systemcc.", 
                           true, sysCC_mem_collect, 2, 3);
                           
    add_collect_info_by_index(plugin_info, 1, "syscc-cpu", "collect cpu information on systemcc.", 
                           false, sysCC_cpu_collect, 5, 1);
  */                         
    return 0;
}

void pluginCC_exit(plugin_info_t *plugin_info)
{
    printf("----###  pluginCC pluginCC_exit ------###--\n");
    
    if (plugin_info->collect_item != NULL) {
        free(plugin_info->collect_item);
        plugin_info->collect_item = NULL;
    }
    plugin_info->item_count = 0;
    
/*
    del_all_info(plugin_info);
  */  
    return;
}


