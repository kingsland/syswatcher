#ifndef __plugin_def_h__
#define __plugin_def_h__
#include <plugin_ext_def.h>
#define PLUGIN_ENTRY(plugin_name, info)    \
    int plugin_name##_init (plugin_info_t *info) \

#define PLUGIN_EXIT(plugin_name, info)    \
    void plugin_name##_exit (plugin_info_t *info) \

#define PLUGIN_INIT(mgr_info, items, plugin_info)  \
{   \
    int count = 0;  \
    (plugin_info)->argc = (mgr_info)->argc;\
    memcpy((plugin_info)->argv, (mgr_info)->argv, (ARGV_LEN*((mgr_info)->argc)));\
    memcpy((mgr_info), (plugin_info), sizeof(plugin_info_t));  \
    (mgr_info)->version = PLUGIN_RELEASE_VERSION; \
    (mgr_info)->item_count = sizeof(items)/sizeof(collect_item_t);    \
    (mgr_info)->collect_item = (collect_item_t*)malloc(sizeof(collect_item_t)*(mgr_info)->item_count);  \
    for (;count < (mgr_info)->item_count; count++) {  \
        memcpy(&((mgr_info)->collect_item[count]), &((items)[count]), sizeof(collect_item_t));  \
    }   \
}

#define PLUGIN_FREE(info)   \
{   \
    if ((info)->collect_item != NULL) { \
        free((info)->collect_item); \
        (info)->collect_item = NULL;    \
    }   \
}

#endif /* __plugin_ext_def_h__ */

