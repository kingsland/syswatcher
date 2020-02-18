#ifndef DEFS_H
#define DEFS_H
#define METRIC_NAME_LEN         (64)
#define METRIC_DESCRIPTION_LEN  (256)
#define MAX_STRING_SIZE         (128)
enum data_type {
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
};

union val_t {
    int8_t      int8;
    uint8_t     uint8;
    int16_t     int16;
    uint16_t    uint16;
    int32_t     int32;
    uint32_t    uint32;
    float       f;
    double      d;
    char        str[MAX_STRING_SIZE];
};

#endif  //end of DEFS_H
