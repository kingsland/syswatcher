#ifndef __plugin_mgr_h__
#define __plugin_mgr_h__

#include <stdio.h>
#include <stdbool.h>
#include "list.h"
#include <time.h>
#include "plugin_ext_def.h"
#include "plugin_protocol.h"
#include <defs.h>
#include <pthread.h>

#define PLUGIN_MGR_TAG_CHECKSUM     0xe50a7d21
#define PLUGIN_MGR_INVALID_ID           0
#define PLUGIN_MGR_NAME_LENGTH  256
#define PLUGIN_MGR_PATH_LENGTH  2048
#define PLUGIN_MGR_TITLE_LENGTH  256
#define PLUGIN_MGR_DESC_LENGTH  1024
#define PLUGIN_MGR_VERSION_LENGTH  64

#define PLUGIN_LOAD_DLOPEN          (1UL<<0)
#define PLUGIN_LOAD_VERS_CHECK      (1UL<<1)
#define PLUGIN_LOAD_INIT            (1UL<<2)

typedef int (*plugin_init_func_t)(plugin_info_t *);
typedef void (*plugin_exit_func_t)(plugin_info_t *);

typedef struct collect_item_list
{
    struct list_head node;
    int index;
    int (*collect_data_func)(item_t* data);
    item_t item;
}collect_item_list_t;

typedef struct plugin
{
    struct list_head node;
    struct list_head *head;
    plugin_key_t id;
    bool flag;    /* true:valid   false:invalid */
    void* handler;
    char name[PLUGIN_MGR_NAME_LENGTH];
    char path[PLUGIN_MGR_PATH_LENGTH];
    plugin_init_func_t plugin_init;
    plugin_exit_func_t plugin_exit;
    plugin_info_t plugin_info;
    plugin_channel_t notify_info;
    time_t load_time;
    time_t unload_time;
    uint64_t load_flags;
}plugin_t;

typedef struct plugin_mgr
{
    int tag;
    struct list_head *head;
    pthread_t plugin_thread_id;
    plugin_key_t plugin_id_inc;
    int (*load)(void*, plugin_channel_t *);
    int (*unload)(void*, unsigned long long);
    void* context;
    unsigned plugins;
    time_t create_time;
    time_t last_time;
}plugin_mgr_t;


/*========================  * PLUGIN MGR API *  =========================*/
plugin_mgr_t* plugin_mgr_init(void);
void plugin_mgr_des(plugin_mgr_t**);

bool plugin_mgr_check(plugin_mgr_t*);
plugin_t* plugin_search_by_name(plugin_mgr_t*, const char *);
plugin_t* plugin_search_by_id(plugin_mgr_t*, const plugin_key_t);
int plugin_parser(plugin_mgr_t*, plugin_cmd_t *);
int plugin_load(plugin_mgr_t*, plugin_t *);
int plugin_reload(plugin_mgr_t*, plugin_t *);
int plugin_unload(plugin_mgr_t*, plugin_t *);


#endif /* __plugin_mgr_h__ */

