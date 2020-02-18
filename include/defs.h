#ifndef DEFS_H
#define DEFS_H
#include <stdint.h>
#include <stdbool.h>
/**********move this part to plugin header*********/
#define PLUGIN_NAME_LENGTH      (64)
#define PLUGIN_DESC_LENGTH      (1024)
#define MAX_STRING_SIZE         (128)
#define DATA_NAME_LENGTH        (16)
#define UNIT_LENGTH             (16)
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
    float       f;
    double      d;
    char        str[MAX_STRING_SIZE];
} val_t;

typedef struct mate_t {
    char        name[DATA_NAME_LENGTH];
    char        unit[UNIT_LENGTH];
    enum        data_type t;
    union       val_t val;
} mate_t;

typedef struct item_t {
    int elememt_num;
    mate_t *data;
} item_t;

/**************************************************/

#define METRIC_NAME_LENGTH      (PLUGIN_NAME_LENGTH)
#define METRIC_DESC_LENGTH      (PLUGIN_DESC_LENGTH)

typedef struct plugin_channel {
    unsigned long long plugin_id;
    char name[PLUGIN_NAME_LENGTH];
    char desc[PLUGIN_DESC_LENGTH];
    bool run_once;
    time_t interval;
    item_t *item;
    int (*collect_data_func)(item_t *);
} plugin_channel_t;
#endif  //end of DEFS_H
