#ifndef DEFS_H
#define DEFS_H
#define METRIC_NAME_LEN         (64)
#define METRIC_DESCRIPTION_LEN  (256)
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
#endif  //end of DEFS_H
