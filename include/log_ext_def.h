#ifndef LOG_EXT_DEF_H
#define LOG_EXT_DEF_H
enum log_level {
    LEVEL_NONE,
    LEVEL_ERR,
    LEVEL_WARN,
    LEVEL_INFO
};

void print_log(enum log_level level, const char *fmt, ...);
#endif  //end of LOG_EXT_DEF_H
