#include <cpu_data.h>
#include <gm_file.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
/*
** A helper function to determine the number of cpustates in /proc/stat (MKN)
*/
#define NUM_CPUSTATES_24X 4
#define NUM_CPUSTATES_26X 7

/* sanity check and range limit */
double sanityCheck( int line, char *file, const char *func, double v, double diff, double dt, JT a, JT b, JT c, JT d )
{
    if ( v > 100.0 ) {
        plugin_print_log( "file %s, line %d, fn %s: val > 100: %g ~ %g / %g = (%llu - %llu) / (%llu - %llu)\n", file, line, func, v, diff, dt, a, b, c, d );
        return 100.0;
    }
    else if ( v < 0.0 ) {
        plugin_print_log( "file %s, line %d, fn %s: val < 0: %g ~ %g / %g = (%llu - %llu) / (%llu - %llu)\n", file, line, func, v, diff, dt, a, b, c, d );
        return 0.0;
    }
    return v;
}

unsigned int
num_cpustates_func(char *stat)
{
   char *p;
   unsigned int i=0;

   p = stat;

/*
** Skip initial "cpu" token
*/
   p = skip_token(p);
   p = skip_whitespace(p);
/*
** Loop over file until next "cpu" token is found.
** i=4 : Linux 2.4.x
** i=7 : Linux 2.6.x
** i=8 : Linux 2.6.11
*/
   while (strncmp(p, "cpu", 3)) {
     p = skip_token(p);
     p = skip_whitespace(p);
     i++;
     }

   return i;
}

int get_core_num(char *stat)
{
    char *p, *tmp;
    int count = 0;
    p = stat;
    while(1) {
        tmp = strstr(p, "cpu");
        if (tmp == NULL) {
            break;
        }
        tmp = tmp + strlen("cpu");
        p = tmp;
        count++;
    }
    return count-1;
}

JT core_total_jiffies_func(int core, char *stat)
{
    char *p, *tmp;
    int idx;
    JT user_jiffies, nice_jiffies, system_jiffies, idle_jiffies,
       wio_jiffies, irq_jiffies, sirq_jiffies, steal_jiffies;

    p = stat;
    for (idx = 0; idx < core + 1; idx++)
    {
        tmp = strstr(p, "cpu");
        tmp += 3;
        p = tmp;
    }
    p -= 3;

    p = skip_token(p);
    p = skip_whitespace(p);
    user_jiffies = strtod( p, &p );
    p = skip_whitespace(p);
    nice_jiffies = strtod( p, &p );
    p = skip_whitespace(p);
    system_jiffies = strtod( p, &p );
    p = skip_whitespace(p);
    idle_jiffies = strtod( p, &p );

    if(num_cpustates == NUM_CPUSTATES_24X)
        return user_jiffies + nice_jiffies + system_jiffies + idle_jiffies;

    p = skip_whitespace(p);
    wio_jiffies = strtod( p, &p );
    p = skip_whitespace(p);
    irq_jiffies = strtod( p, &p );
    p = skip_whitespace(p);
    sirq_jiffies = strtod( p, &p );

    if(num_cpustates == NUM_CPUSTATES_26X)
        return user_jiffies + nice_jiffies + system_jiffies + idle_jiffies +
            wio_jiffies + irq_jiffies + sirq_jiffies;

    p = skip_whitespace(p);
    steal_jiffies = strtod( p, &p );

    return user_jiffies + nice_jiffies + system_jiffies + idle_jiffies +
        wio_jiffies + irq_jiffies + sirq_jiffies + steal_jiffies;
}

#define MAX_CORE_NUM    (128)
struct core_stat {
    JT  last_idle_jiffies;
    JT  last_total_jiffies;
} core_stats[MAX_CORE_NUM];

float cpu_core_usage(int core, char *stat)
{
    char *p, *tmp;
    float val;
    int idx;
    JT idle_jiffies, total_jiffies, diff;
    p = stat;
    for (idx = 0; idx < core + 1; idx++)
    {
        tmp = strstr(p, "cpu");
        tmp += 3;
        p = tmp;
    }
    p -= 3;
    p = skip_token(p);
    p = skip_token(p);
    p = skip_token(p);
    p = skip_token(p);
    idle_jiffies  = strtod( p , (char **)NULL );
    total_jiffies = core_total_jiffies_func(core, stat);
    diff = idle_jiffies - core_stats[core].last_idle_jiffies;
    if (diff) {
        val = ((double)diff/(double)(total_jiffies - core_stats[core].last_total_jiffies)) * 100;
    } else {
        val = 0.0;
    }

    val = sanityCheck( __LINE__, __FILE__, __FUNCTION__, val, (double)diff, (double)(total_jiffies - core_stats[core].last_total_jiffies), idle_jiffies, core_stats[core].last_idle_jiffies, total_jiffies, core_stats[core].last_total_jiffies);
    core_stats[core].last_idle_jiffies  = idle_jiffies;
    core_stats[core].last_total_jiffies = total_jiffies;
    return val;
}

struct user_stat {
    JT  last_user_jiffies;
    JT  last_total_jiffies;
} user_stats[MAX_CORE_NUM] = {0};

float core_user_usage(int core, char *stat)
{
    char *p, *tmp;
    float val;
    int idx;
    JT user_jiffies, total_jiffies, diff;
    p = stat;
    for (idx = 0; idx < core + 1; idx++)
    {
        tmp = strstr(p, "cpu");
        tmp += 3;
        p = tmp;
    }
    p -= 3;
    p = skip_token(p);
    user_jiffies  = strtod( p , (char **)NULL );
    total_jiffies = core_total_jiffies_func(core, stat);
    diff = user_jiffies - user_stats[core].last_user_jiffies;
    if (diff) {
        val = ((double)diff/(double)(total_jiffies - user_stats[core].last_total_jiffies)) * 100;
    } else {
        val = 0.0;
    }

    val = sanityCheck( __LINE__, __FILE__, __FUNCTION__, val, (double)diff, (double)(total_jiffies - user_stats[core].last_total_jiffies), user_jiffies, user_stats[core].last_user_jiffies, total_jiffies, user_stats[core].last_total_jiffies);
    user_stats[core].last_user_jiffies  = user_jiffies;
    user_stats[core].last_total_jiffies = total_jiffies;
    return val;
}

struct sys_stat {
    JT  last_sys_jiffies;
    JT  last_total_jiffies;
} sys_stats[MAX_CORE_NUM] = {0};

float core_sys_usage(int core, char *stat)
{
    char *p, *tmp;
    float val;
    int idx;
    JT sys_jiffies, total_jiffies, diff;
    p = stat;
    for (idx = 0; idx < core + 1; idx++)
    {
        tmp = strstr(p, "cpu");
        tmp += 3;
        p = tmp;
    }
    p -= 3;
    p = skip_token(p);
    p = skip_token(p);
    p = skip_token(p);
    sys_jiffies  = strtod( p , (char **)NULL );
    if (num_cpustates > NUM_CPUSTATES_24X) {
        p = skip_token(p);
        p = skip_token(p);
        p = skip_token(p);
        sys_jiffies += strtod( p, (char **)NULL ); /* "intr" counted in system */
        p = skip_token(p);
        sys_jiffies += strtod( p, (char **)NULL ); /* "sintr" counted in system */
    }
    total_jiffies = core_total_jiffies_func(core, stat);
    diff = sys_jiffies - sys_stats[core].last_sys_jiffies;
    if (diff) {
        val = ((double)diff/(double)(total_jiffies - sys_stats[core].last_total_jiffies)) * 100;
    } else {
        val = 0.0;
    }

    val = sanityCheck( __LINE__, __FILE__, __FUNCTION__, val, (double)diff, (double)(total_jiffies - sys_stats[core].last_total_jiffies), sys_jiffies, sys_stats[core].last_sys_jiffies, total_jiffies, sys_stats[core].last_total_jiffies );

    sys_stats[core].last_sys_jiffies  = sys_jiffies;
    sys_stats[core].last_total_jiffies = total_jiffies;
    return val;
}

struct iowait_stat {
    JT  last_iowait_jiffies;
    JT  last_total_jiffies;
} iowait_stats[MAX_CORE_NUM] = {0};

float core_iowait_usage(int core, char *stat)
{
    char *p, *tmp;
    float val;
    int idx;
    JT iowait_jiffies, total_jiffies, diff;
    if (num_cpustates == NUM_CPUSTATES_24X) {
      val = 0.0;
      return val;
    }

    p = stat;
    for (idx = 0; idx < core + 1; idx++)
    {
        tmp = strstr(p, "cpu");
        tmp += 3;
        p = tmp;
    }
    p -= 3;
    p = skip_token(p);
    p = skip_token(p);
    p = skip_token(p);
    p = skip_token(p);
    p = skip_token(p);
    iowait_jiffies  = strtod( p , (char **)NULL );
    total_jiffies = core_total_jiffies_func(core, stat);
    diff = iowait_jiffies - iowait_stats[core].last_iowait_jiffies;
    if (diff) {
        val = ((double)diff/(double)(total_jiffies - iowait_stats[core].last_total_jiffies)) * 100;
    } else {
        val = 0.0;
    }

    val = sanityCheck( __LINE__, __FILE__, __FUNCTION__, val, (double)diff, (double)(total_jiffies - iowait_stats[core].last_total_jiffies), iowait_jiffies, iowait_stats[core].last_iowait_jiffies, total_jiffies, iowait_stats[core].last_total_jiffies );

    iowait_stats[core].last_iowait_jiffies  = iowait_jiffies;
    iowait_stats[core].last_total_jiffies = total_jiffies;
    return val;
}
