#ifndef DEFS_H
#define DEFS_H
#include <stdint.h>
#include <stdbool.h>
#include <plugin_ext_def.h>
#define METRIC_NAME_LENGTH      (PLUGIN_NAME_LENGTH)
#define METRIC_DESC_LENGTH      (PLUGIN_DESC_LENGTH)

typedef struct plugin_sub_channel {
        char subname[PLUGIN_NAME_LENGTH];
        char subdesc[PLUGIN_DESC_LENGTH];
        bool run_once;
        time_t interval;
        item_t item;
        int (*collect_data_func)(item_t *);
} plugin_sub_channel_t;

typedef struct plugin_channel {
    plugin_key_t plugin_id;
    char name[PLUGIN_NAME_LENGTH];
    char desc[PLUGIN_DESC_LENGTH];
    int sub_metric_num;
    plugin_sub_channel_t *sub_channel;
} plugin_channel_t;
#endif  //end of DEFS_H
