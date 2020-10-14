#ifndef __plugin_def_h__
#define __plugin_def_h__
#include <plugin_ext_def.h>
#define PLUGIN_ENTRY(plugin_name, info)    \
    int plugin_name##_init (plugin_info_t *info) \

#define PLUGIN_EXIT(plugin_name, info)    \
    void plugin_name##_exit (plugin_info_t *info) \

#define PLUGIN_INIT(info_dst, items, info_src)  \
{   \
    int count = 0;  \
    memcpy((info_dst), (info_src), sizeof(plugin_info_t));  \
    (info_dst)->version = PLUGIN_RELEASE_VERSION; \
    (info_dst)->item_count = sizeof(items)/sizeof(collect_item_t);    \
    (info_dst)->collect_item = (collect_item_t*)malloc(sizeof(collect_item_t)*(info_dst)->item_count);  \
    for (;count < (info_dst)->item_count; count++) {  \
        memcpy(&((info_dst)->collect_item[count]), &((items)[count]), sizeof(collect_item_t));  \
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

