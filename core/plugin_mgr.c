#include "plugin_mgr.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <unistd.h>
#include "list.h"
#include "srm_errno.h"

//#define ERROR_INFO(FMT, ARGS...)  printf("\033[1;31;40m* ERROR :\033[0m %s:%d"FMT"\n", __FILE__, __LINE__, ##ARGS)
#define ERROR_INFO(FMT, ARGS...)  printf("\033[1;31;40m* ERROR :\033[0m "FMT"\n", ##ARGS)


plugin_mgr_t* plugin_mgr_init(void)
{
    plugin_mgr_t *mgr = (plugin_mgr_t *)malloc(sizeof(plugin_mgr_t));
    if (mgr != NULL) {
        NEW_LIST_NODE(head);
        if (head != NULL) {
            mgr->head = head;
            mgr->create_time = time(NULL);
            mgr->last_time = time(NULL);
            mgr->tag = PLUGIN_MGR_TAG_CHECKSUM;
            mgr->plugin_id_inc = PLUGIN_MGR_INVALID_ID;
            mgr->load = NULL;
            mgr->unload = NULL;
            mgr->context = NULL;
            mgr->plugins = 0;
        }
    }

    return mgr;
}

void plugin_mgr_des(plugin_mgr_t **mgr)
{
    struct list_head *pos, *n;

    if (*mgr != NULL) {
        list_for_each_safe(pos, n, (*mgr)->head) {
            plugin_unload(*mgr, (plugin_t*)pos);
            if (pos != NULL)
                free(pos);
        }
        free(*mgr);
        *mgr = NULL;
    }

    return;
}

bool plugin_mgr_check(plugin_mgr_t* mgr)
{
    bool flag = false;

    if (mgr != NULL)
        flag = mgr->tag == PLUGIN_MGR_TAG_CHECKSUM ? true : false;

    return flag;
}

plugin_t* plugin_search_by_name(plugin_mgr_t*mgr, const char *plugin_name)
{
    struct list_head *pos, *node= NULL;

    list_for_each(pos, mgr->head) {
        if (!strncmp(((plugin_t*)pos)->name, plugin_name, PLUGIN_MGR_NAME_LENGTH))
            node = pos;
    }

    return (plugin_t*)node;
}

plugin_t* plugin_search_by_id(plugin_mgr_t* mgr, const plugin_key_t id)
{
    struct list_head *pos, *node= NULL;

    list_for_each(pos, mgr->head) {
        if (((plugin_t*)pos)->id == id)
            node = pos;
    }

    return (plugin_t*)node;
}

int plugin_parser(plugin_mgr_t* mgr, plugin_cmd_t *cmd)
{
    int ret = SRM_OK;
    size_t dlname_size;
    char *p1;
    plugin_t *plugin_node = NULL;
    char  plugin_name[PLUGIN_MGR_NAME_LENGTH] = {0x0};
    int  plugin_name_len;

    /* Extra dyanmic info and name of plugin */
    char *p = strrchr(cmd->buffer, '/');
    if (p == NULL)
        p = (char*)(cmd->buffer);
    else
        p++;
    p1 = (char*)(cmd->buffer);

    /* get dynamic name by point p1 */
    dlname_size = (size_t)(p1 + cmd->size - p);
    memcpy(plugin_name, p+3, dlname_size-3);
    /* plugin name.  eg:libpluginAA.so -- pluginAA */
    plugin_name_len = dlname_size-6 % PLUGIN_MGR_NAME_LENGTH;
    plugin_name[plugin_name_len- 1] = '\0';

    plugin_node = plugin_search_by_name(mgr, plugin_name);
    if (plugin_node != NULL) {
        switch (cmd->type & 0x0F) {
            case 0x1:
                plugin_unload(mgr, plugin_node);
                free(plugin_node);
                plugin_node = NULL;
                break;
            case 0x0: // as reload
            case 0x2:
                plugin_reload(mgr, plugin_node);
                break;
            default:
                ERROR_INFO("Opreration unknown.");
        }
    } else {
        switch (cmd->type & 0x0F) {
            case 0x0:
                plugin_node = (plugin_t*)malloc(sizeof(plugin_t));
                if (plugin_node != NULL) {
                    memcpy(plugin_node->name, plugin_name, plugin_name_len);
                    plugin_node->name[plugin_name_len - 1] = '\0';
                    memcpy(plugin_node->path, cmd->buffer, cmd->size);
                    plugin_node->path[cmd->size % PLUGIN_MGR_PATH_LENGTH] = '\0';
                    /* Initialize assgin */
                    plugin_node->id = PLUGIN_MGR_INVALID_ID;
                    plugin_node->handler = NULL;
                    plugin_node->flag = false;
                    plugin_node->plugin_init = NULL;
                    plugin_node->plugin_exit = NULL;
                    plugin_node->plugin_info.desc = NULL;
                    plugin_node->plugin_info.name = NULL;
                    plugin_node->plugin_info.version = NULL;
                    plugin_node->plugin_info.collect_item = NULL;
                    plugin_node->plugin_info.item_count = 0;
                    plugin_node->notify_info.sub_channel = NULL;
                    plugin_node->notify_info.sub_metric_num = 0;
                    plugin_node->notify_info.plugin_id = PLUGIN_MGR_INVALID_ID;
                    plugin_node->notify_info.name[0] = '\0';
                    plugin_node->notify_info.desc[0] = '\0';
                    /* load operation */
                    plugin_load(mgr, plugin_node);
                } else {
                    ret = SRM_ALLOC_SPACE_ERR;
                    ERROR_INFO("Alloc space of memory wrong.");
                }
                break;
            case 0x1:
            case 0x2:
                ret = SRM_NOT_ALLOWED_ERR;
                ERROR_INFO("Opreration not allowed where current system does not loaded plugin.");
                break;
            default:
                ERROR_INFO("Opreration unknown.");
        }
    }

    return ret;
}

int plugin_load(plugin_mgr_t* mgr, plugin_t *plugin_node)
{
    int ret = SRM_OK;
    char plugin_init_func_name[2048];
    char plugin_exit_func_name[2048];

    plugin_node->handler = dlopen(plugin_node->path, RTLD_LAZY);
    if (plugin_node->handler != NULL) {
        plugin_node->id = mgr->plugin_id_inc ++;
        plugin_node->flag = true;
        plugin_node->load_time = time(NULL);
        sprintf(plugin_init_func_name,"%s_init",plugin_node->name);
        sprintf(plugin_exit_func_name,"%s_exit",plugin_node->name);
        plugin_node->plugin_init = (plugin_init_func_t)
                                dlsym(plugin_node->handler, plugin_init_func_name);
        plugin_node->plugin_exit = (plugin_exit_func_t)
                                dlsym(plugin_node->handler, plugin_exit_func_name);

        plugin_node->plugin_init(&(plugin_node->plugin_info));
        if (strncmp(plugin_node->plugin_info.version, PLUGIN_RELEASE_VERSION, strlen(PLUGIN_RELEASE_VERSION))) {
            ret = SRM_VERSION_MISMATCH_ERR;
            ERROR_INFO("Plugin version does not match the SRM version.");
        } else {
            plugin_node->notify_info.plugin_id = plugin_node->id;
            strncpy(plugin_node->notify_info.name, plugin_node->plugin_info.name, PLUGIN_NAME_LENGTH);
            plugin_node->notify_info.name[PLUGIN_NAME_LENGTH - 1] = '\0';
            strncpy(plugin_node->notify_info.desc, plugin_node->plugin_info.desc, PLUGIN_DESC_LENGTH);
            plugin_node->notify_info.desc[PLUGIN_DESC_LENGTH - 1] = '\0';
            plugin_node->notify_info.sub_metric_num = plugin_node->plugin_info.item_count;
            plugin_node->notify_info.sub_channel =
                    (plugin_sub_channel_t*)malloc(sizeof(plugin_sub_channel_t)*plugin_node->notify_info.sub_metric_num);
            if (plugin_node->notify_info.sub_channel != NULL) {
                for (int i=0; i<plugin_node->notify_info.sub_metric_num; i++) {
                    strncpy(plugin_node->notify_info.sub_channel[i].subname,
                            plugin_node->plugin_info.collect_item[i].item_name,
                            PLUGIN_NAME_LENGTH);
                    plugin_node->notify_info.sub_channel[i].subname[PLUGIN_NAME_LENGTH - 1] = '\0';
                    strncpy(plugin_node->notify_info.sub_channel[i].subdesc,
                            plugin_node->plugin_info.collect_item[i].item_desc,
                            PLUGIN_DESC_LENGTH);
                    plugin_node->notify_info.sub_channel[i].subdesc[PLUGIN_DESC_LENGTH - 1] = '\0';
                    plugin_node->notify_info.sub_channel[i].run_once =
                                            plugin_node->plugin_info.collect_item[i].run_once;
                    plugin_node->notify_info.sub_channel[i].interval =
                                            plugin_node->plugin_info.collect_item[i].interval;
                    plugin_node->notify_info.sub_channel[i].collect_data_func =
                                            plugin_node->plugin_info.collect_item[i].collect_data_func;
                    plugin_node->notify_info.sub_channel[i].run_once =
                                            plugin_node->plugin_info.collect_item[i].run_once;
                    plugin_node->notify_info.sub_channel[i].item.elememt_num =
                                            plugin_node->plugin_info.collect_item[i].data_count;
                    plugin_node->notify_info.sub_channel[i].item.data =
                                    (mate_t*)malloc(sizeof(mate_t)*plugin_node->notify_info.sub_channel[i].item.elememt_num);
                    if (plugin_node->notify_info.sub_channel[i].item.data == NULL) {
                        plugin_node->notify_info.sub_channel[i].item.elememt_num = 0;
                    }
                }
            }
            list_add((struct list_head*)plugin_node, mgr->head);
            mgr->plugins++;
            mgr->load(mgr->context, &plugin_node->notify_info);
        }
    }else {
        ret = SRM_ALLOC_SPACE_ERR;
        ERROR_INFO("Alloc space of memory wrong.");
    }

    return ret;
}

int plugin_reload(plugin_mgr_t* mgr, plugin_t *plugin_node)
{
    plugin_unload(mgr, plugin_node);

    plugin_load(mgr, plugin_node);

    return 0;
}

int plugin_unload(plugin_mgr_t* mgr, plugin_t *plugin_node)
{
    int ret = SRM_OK;

    if (plugin_node != NULL) {
        if (plugin_node->notify_info.sub_channel != NULL) {
            for (int i =0; i<plugin_node->notify_info.sub_metric_num; i++) {
                if (plugin_node->notify_info.sub_channel[i].item.data != NULL) {
                    free(plugin_node->notify_info.sub_channel[i].item.data);
                    plugin_node->notify_info.sub_channel[i].item.data = NULL;
                    plugin_node->notify_info.sub_channel[i].item.elememt_num = 0;
                }
            }

            if (plugin_node->notify_info.sub_metric_num) {
                free(plugin_node->notify_info.sub_channel);
                plugin_node->notify_info.sub_channel = NULL;
                plugin_node->notify_info.sub_metric_num = 0;
            }
            plugin_node->notify_info.plugin_id = PLUGIN_MGR_INVALID_ID;
            plugin_node->notify_info.name[0] = '\0';
            plugin_node->notify_info.desc[0] = '\0';
        }
        mgr->unload(mgr->context, plugin_node->id);
        plugin_node->id = PLUGIN_MGR_INVALID_ID;
        /* Release user space of itself, by plugin_info.collect_item variable */
        plugin_node->plugin_exit(&(plugin_node->plugin_info));
        list_del((struct list_head*)plugin_node);
        dlclose(plugin_node->handler);
        /* value assign */
        plugin_node->handler = NULL;
        plugin_node->flag = false;
        plugin_node->plugin_init = NULL;
        plugin_node->plugin_exit = NULL;
        plugin_node->plugin_info.desc = NULL;
        plugin_node->plugin_info.name = NULL;
        plugin_node->plugin_info.version = NULL;
        plugin_node->plugin_info.collect_item = NULL;
        plugin_node->plugin_info.item_count = 0;
        plugin_node->unload_time = time(NULL);
        mgr->plugins--;
    }

    return ret;
}


