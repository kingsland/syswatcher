#ifndef __plugin_protocol_h__
#define __plugin_protocol_h__

#include <stdint.h>

#define PROTOCOL_PIPE_PATH   "/srv/pipe/"
#define COMUNICATION_CMD    PROTOCOL_PIPE_PATH"cmd"
#define PLUGIN_CMD_LENGTH  10240

typedef struct plugin_cmd
{
    uint8_t  type;  /* type = 0x9X, X=cmd num */
    uint8_t  flag;  /* flag = 0x6b*/
    uint16_t size;
    char  buffer[PLUGIN_CMD_LENGTH];
}plugin_cmd_t;


#endif /* __plugin_protocol_h__ */

