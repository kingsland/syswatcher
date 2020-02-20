#include "plugin_mgr.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <unistd.h>
#include "list.h"

bool plugin_mgr_check(plugin_mgr_t* mgr)
{
    bool flag = false;

    if (mgr != NULL)
        flag = mgr->tag == PLUGIN_MGR_TAG_CHECKSUM ? true : false;

    return flag;
}

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
    struct list_head *pos_item, *n_item;

    if (*mgr != NULL) {
        list_for_each_safe(pos, n, (*mgr)->head) {
            list_for_each_safe(pos_item, n_item, ((plugin_t*)pos)->head) {
                free(((collect_item_list_t*)pos_item)->item.data);
                list_del(pos_item);
                free (pos_item);
            }
            ((plugin_t*)pos)->plugin_exit(&(((plugin_t*)pos)->plugin_info));
            list_del(pos);
            free(pos);
        }
        free(*mgr);
        *mgr = NULL;
    }

    return;
}

void plugin_parser(plugin_mgr_t* mgr, plugin_cmd_t *cmd)
{
    /*void *handler = NULL;
    char dlname[256];
    char plugin_name[256];*/

    struct list_head *pos;
    size_t dlname_size;
    char *p1;
    plugin_t *plugin_node = NULL;

    char *p = strrchr(cmd->buffer, '/');
    if (p == NULL)
        p = (char*)(cmd->buffer);
    else
        p++;
    p1 = (char*)(cmd->buffer);

    dlname_size = (size_t)(p1 + cmd->size - p);

    plugin_node = (plugin_t*)malloc(sizeof(plugin_t));
    if (plugin_node != NULL) {
        memcpy(plugin_node->name, p+3, dlname_size-3);
        plugin_node->name[dlname_size-7 % PLUGIN_MGR_NAME_LENGTH] = '\0';
        memcpy(plugin_node->path, cmd->buffer, cmd->size);
        plugin_node->path[cmd->size % PLUGIN_MGR_PATH_LENGTH] = '\0';

        switch (cmd->type & 0x0F) {
            case 0x0:
                list_for_each(pos, mgr->head) {
                    if (!strcmp(((plugin_t*)pos)->name, plugin_node->name)) {
                        break;
                    }
                }
                if (pos != mgr->head)
                    free(plugin_node);
                else
                    plugin_load(mgr, plugin_node);
                break;
            case 0x1:
                plugin_unload(mgr, plugin_node);
                break;
            default:
                free(plugin_node);
        }
    }

    return ;
}

int plugin_load(plugin_mgr_t* mgr, plugin_t *plugin_node)
{
    char plugin_init_func_name[2048];
    char plugin_exit_func_name[2048];
    /*
    collect_item_list_t *collect_item;*/

    plugin_node->handler = dlopen(plugin_node->path, RTLD_LAZY);
    if (plugin_node->handler != NULL) {
        plugin_node->flag = true;
        plugin_node->load_time = time(NULL);
        sprintf(plugin_init_func_name,"%s_init",plugin_node->name);
        sprintf(plugin_exit_func_name,"%s_exit",plugin_node->name);
        plugin_node->plugin_init = (plugin_init_func_t)
                                dlsym(plugin_node->handler, plugin_init_func_name);
        plugin_node->plugin_exit = (plugin_exit_func_t)
                                dlsym(plugin_node->handler, plugin_exit_func_name);

        plugin_node->plugin_init(&(plugin_node->plugin_info));
 #if 0
        NEW_LIST_NODE(head);
        if (head != NULL) {
            plugin_node->head = head;
            for (int i=0; i<plugin_node->plugin_info.item_count; i++) {
                collect_item = (collect_item_list_t*)malloc(sizeof(collect_item_list_t));
                if (collect_item != NULL) {
                    list_add((struct list_head*)collect_item, plugin_node->head);
                    collect_item->collect_data_func = plugin_node->plugin_info.collect_item[i].collect_data_func;
                    collect_item->item.elememt_num = plugin_node->plugin_info.collect_item[i].data_count;
                    collect_item->index = i;
                    collect_item->item.data = (mate_t*)malloc(sizeof(mate_t)*collect_item->item.elememt_num);
                    /*if (collect_item->item.obj == NULL) {

                    }*/

                    mgr->load(mgr->context, &plugin_node->plugin_info);
                }
            }
        }
#endif
        plugin_node->notify_info.plugin_id = mgr->plugin_id_inc;
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
        plugin_node->load_time = time(NULL);
        plugin_node->unload_time = 0;
        plugin_node->id = mgr->plugin_id_inc ++;
        list_add((struct list_head*)plugin_node, mgr->head);
        mgr->plugins++;
        mgr->load(mgr->context, &plugin_node->notify_info);
    }

    return 0;
}

int plugin_reload(plugin_mgr_t* mgr, plugin_t *plugin_node)
{

    return 0;
}

int plugin_unload(plugin_mgr_t* mgr, plugin_t *plugin_node)
{
    struct list_head *pos, *m;/*
    struct list_head *item_pos, *n;*/

    if (plugin_node != NULL) {
        list_for_each_safe(pos, m, mgr->head) {
#if 0
            if (!strcmp(((plugin_t*)pos)->name, plugin_node->name)) {
                list_for_each_safe(item_pos, n, ((plugin_t*)pos)->head) {
                    free(((collect_item_list_t*)item_pos)->item.data);
                    list_del(item_pos);
                    free(item_pos);
                }
                ((plugin_t*)pos)->plugin_exit(&(((plugin_t*)pos)->plugin_info));
                list_del(pos);
                dlclose(((plugin_t*)pos)->handler);
                free(pos);
                mgr->plugins--;
            }
#endif
            if (!strcmp(((plugin_t*)pos)->name, plugin_node->name)) {
                for (int i =0; i<((plugin_t*)pos)->notify_info.sub_metric_num; i++) {
                    if (((plugin_t*)pos)->notify_info.sub_channel[i].item.data != NULL) {
                        free(((plugin_t*)pos)->notify_info.sub_channel[i].item.data);
                        ((plugin_t*)pos)->notify_info.sub_channel[i].item.data = NULL;
                    }
                }

                if (((plugin_t*)pos)->notify_info.sub_channel != NULL) {
                    free(((plugin_t*)pos)->notify_info.sub_channel);
                    ((plugin_t*)pos)->notify_info.sub_channel = NULL;
                }
                mgr->unload(mgr->context, ((plugin_t*)pos)->id);
                ((plugin_t*)pos)->plugin_exit(&(((plugin_t*)pos)->plugin_info));
                list_del(pos);
                dlclose(((plugin_t*)pos)->handler);
                free(pos);
                mgr->plugins--;
            }
        }

        free(plugin_node);
    }

    return 0;
}


