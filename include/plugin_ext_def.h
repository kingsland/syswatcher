#ifndef __plugin_ext_def_h__
#define __plugin_ext_def_h__

#include <time.h>
#include "defs.h"

/* !!!!!  plugin_info_t->version = PLUGIN_RELEASE_VERSION  */
#define PLUGIN_RELEASE_VERSION  "v0.0.1"

#define ITEM_NAME_LENGTH    64
#define ITEM_DESC_LENGTH   1024
#define META_NAME_LENGTH    64
#define META_UNIT_LENGTH    16

typedef struct collect_item
{
    char *item_name;
    char *item_desc;
    bool run_once;
    int (*collect_data_func)(item_t *);
    time_t interval;
    unsigned data_count;
}collect_item_t;

typedef struct plugin_info
{
    char *name;
    char *desc;
    char *version;
    collect_item_t *collect_item;
    unsigned item_count;
}plugin_info_t;

/*
#define add_plugin_info(plugin_info_p, name, desc, item_count)   do {\
                                plugin_info_p->version = PLUGIN_RELEASE_VERSION;\
                                plugin_info_p->name = name;\
                                plugin_info_p->desc = desc;\
                                plugin_info_p->item_count = item_count;\
                                plugin_info_p->collect_item = (collect_item_t*)malloc(sizeof(collect_item_t)*item_count);}while(0)

#define add_collect_info_by_index(plugin_info_p, item_index, item_name, item_desc, \
                                is_run_once, collect_func, collect_interval, data_count)   \
                                        plugin_info_p->collect_item[item_index].item_name = item_name;\
                                        plugin_info_p->collect_item[item_index].item_desc = item_desc;\
                                        plugin_info_p->collect_item[item_index].run_once = is_run_once;\
                                        plugin_info_p->collect_item[item_index].collect_data_func = collect_func;\
                                        plugin_info_p->collect_item[item_index].interval = collect_interval;\
                                        plugin_info_p->collect_item[item_index].data_count = data_count

#define add_data_desc() 

#define del_all_info(plugin_info_p)   \
                        if (plugin_info_p->collect_item != NULL) {\
                            free(plugin_info_p->collect_item);\
                            plugin_info_p->collect_item = NULL;\
                        }\
                        plugin_info_p->itme_count = 0
*/
/*
#define plugin_init(plugin_name) int plugin_name##_init(plugin_info_t *)
#define plugin_exit(plugin_name) int plugin_name##_exit(plugin_info_t *)
*/

#endif /* __plugin_ext_def_h__ */

