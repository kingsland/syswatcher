#ifndef CPU_DATA_H
#define CPU_DATA_H
#include <plugin_ext_def.h>

#define JT unsigned long long
static unsigned int num_cpustates;

unsigned int num_cpustates_func(char *stat);
int get_core_num(char *stat);
float cpu_core_usage(int core, char *stat);
float core_user_usage(int core, char *stat);
float core_sys_usage(int core, char *stat);
float core_iowait_usage(int core, char *stat);
#endif  //end of CPU_DATA_H
