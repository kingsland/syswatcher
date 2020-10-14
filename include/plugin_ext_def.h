#ifndef __plugin_ext_def_h__
#define __plugin_ext_def_h__

#include <time.h>
#include <stdint.h>
#include <stdbool.h>

/* !!!!!  plugin_info_t->version = PLUGIN_RELEASE_VERSION  */
#define PLUGIN_RELEASE_VERSION  "v0.0.1"

#define ITEM_NAME_LENGTH    64
#define ITEM_DESC_LENGTH   1024
#define META_UNIT_LENGTH    16

/**********move this part to plugin header*********/
#define PLUGIN_NAME_LENGTH      (64)
#define PLUGIN_DESC_LENGTH      (1024)
#define MAX_STRING_SIZE         (128)
#define DATA_NAME_LENGTH        (16)

typedef unsigned long long plugin_key_t;

typedef enum data_type {
    M_UNDEF,
    M_INT8,
    M_UINT8,
    M_INT16,
    M_UINT16,
    M_INT32,
    M_UINT32,
    M_INT64,
    M_UINT64,
    M_FLOAT,
    M_DOUBLE,
    M_STRING
} data_type;

typedef union val_t {
    int8_t      int8;
    uint8_t     uint8;
    int16_t     int16;
    uint16_t    uint16;
    int32_t     int32;
    uint32_t    uint32;
    int64_t     int64;
    uint64_t    uint64;
    float       f;
    double      d;
    char        str[MAX_STRING_SIZE];
} val_t;

typedef struct mate_t {
    char        name[DATA_NAME_LENGTH];
    char        unit[META_UNIT_LENGTH];
    enum        data_type t;
    union       val_t val;
} mate_t;

typedef struct item_t {
    int element_num;
    mate_t *data;
} item_t;

/**************************************************/

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

void plugin_print_log(const char *fmt, ...);

#endif /* __plugin_ext_def_h__ */

