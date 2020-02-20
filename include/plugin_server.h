#ifndef __plugin_server_h__
#define __plugin_server_h__

#include "plugin_channel_def.h"

int plugin_server_start(int (*load)(void *, plugin_channel_t *),
                                 int (*unload)(void *, plugin_key_t),
                                 void* context);

int plugin_server_finish(void);

#endif /* __plugin_server_h__ */

