#ifndef __plugin_protocol_h__
#define __plugin_protocol_h__

#include <stdint.h>
#include <plugin_ext_def.h>

#define PROTOCOL_PIPE_PATH   "/tmp/"
#define COMUNICATION_CMD    PROTOCOL_PIPE_PATH"syswatcher.plugin"
#define PLUGIN_CMD_LENGTH  10240
#define FILE_PATH_LEN       (1024)

typedef struct plugin_cmd
{
    uint8_t  type;  /* type = 0x9X, X=cmd num */
    uint8_t  flag;  /* flag = 0x6b*/
    int argc;
    char path[FILE_PATH_LEN];
    char argv[MAX_ARGV][ARGV_LEN];
} plugin_cmd_t;



#endif /* __plugin_protocol_h__ */

