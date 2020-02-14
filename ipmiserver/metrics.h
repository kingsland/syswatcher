#ifndef METRICS_H
#define METRICS_H
#include <net/if.h>
#include <stdint.h>
float find_disk_space(double *total_size, double *total_free);
float cpu_idle_func ( void );
float cpu_aidle_func ( void );
float cpu_system_func ( void );
void update_speed(void);
#endif  //end of METRICS_H
