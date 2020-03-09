#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <gm_file.h>
/* Needed for VLAN testing */
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_vlan.h>
#include <linux/sockios.h>
#include <stdint.h>
#include <data_collect.h>
#include <plugin_ext_def.h>

/* Linux Specific, but we are in the Linux machine file. */
#define MOUNTS "/proc/mounts"
#define JT unsigned long long
timely_file proc_stat    = { {0,0} , 1., "/proc/stat", NULL, BUFFSIZE };
timely_file proc_net_dev = { {0,0} , 1., "/proc/net/dev", NULL, BUFFSIZE };
/*
 ** A helper function to determine the number of cpustates in /proc/stat (MKN)
 */
#define NUM_CPUSTATES_24X 4
#define NUM_CPUSTATES_26X 7
static unsigned int num_cpustates;

/* Use unsigned long long for stats on systems with strtoull */
typedef unsigned long long stat_t;
/* /proc/net/dev hash table stuff */
typedef struct net_dev_stats net_dev_stats;
struct net_dev_stats {
    char *name;
    stat_t rpi;
    stat_t rpo;
    stat_t rbi;
    stat_t rbo;
    uint32_t bis;
    uint32_t bos;
    net_dev_stats *next;
};
#define NHASH 101
#define MULTIPLIER 31
static net_dev_stats *netstats[NHASH];
#define STAT_MAX ULLONG_MAX
#define PRI_STAT "llu"
#define strtostat(nptr, endptr, base) strtoull(nptr, endptr, base)

static double bytes_in=0, bytes_out=0, pkts_in=0, pkts_out=0;

/*
 ** Helper functions to hash /proc/net/dev stats (Kernighan & Pike)
 */
static unsigned int hashval(const char *s)
{
    unsigned int hval;
    unsigned char *p;

    hval = 0;
    for (p = (unsigned char *)s; *p != '\0'; p++)
        hval = MULTIPLIER * hval + *p;
    return hval % NHASH;
}

static net_dev_stats *hash_lookup(char *devname, size_t nlen)
{
    int hval;
    net_dev_stats *stats;
    char *name=strndup(devname, nlen);

    hval = hashval(name);
    for (stats = netstats[hval]; stats != NULL; stats = stats->next)
    {
        if (strcmp(name, stats->name) == 0) {
            free(name);
            return stats;
        }
    }

    stats = (net_dev_stats *)malloc(sizeof(net_dev_stats));
    if ( stats == NULL )
    {
        plugin_print_log("unable to allocate memory for /proc/net/dev/stats in hash_lookup(%s,%zd)", name, nlen);
        free(name);
        return NULL;
    }
    stats->name = strndup(devname,nlen);
    stats->rpi = 0;
    stats->rpo = 0;
    stats->rbi = 0;
    stats->rbo = 0;
    stats->bis = 0;
    stats->bos = 0;
    stats->next = netstats[hval];
    netstats[hval] = stats;

    free(name);
    return stats;
}

/*
 ** Helper functions for vlan interface testing
 */
static int is_vlan_iface(char *if_name)
{
    int fd,rc;
    struct vlan_ioctl_args vlan_args;

    fd = socket(PF_INET, SOCK_DGRAM, 0);

    // fail if can't open the socket
    if ( fd < 0 ) {
        return 0;
    };

    vlan_args.cmd = GET_VLAN_VID_CMD;
    strncpy(vlan_args.device1, if_name, sizeof(vlan_args.device1));
    rc = ioctl(fd,SIOCGIFVLAN,&vlan_args);

    close(fd);
    if (rc < 0) {
        return 0; // false
    } else {
        return 1; // vlan iface indeed
    }

};

void update_ifdata ( char *caller )
{
    char *p;
    int i;
    static struct timeval stamp={0,0};
    stat_t rbi=0, rbo=0, rpi=0, rpo=0;
    stat_t l_bytes_in=0, l_bytes_out=0, l_pkts_in=0, l_pkts_out=0;
    stat_t bytes_in_t=0, bytes_out_t=0;
    double l_bin, l_bout, l_pin, l_pout;
    net_dev_stats *ns;
    float t;
    struct net_map *netdev;

    p = update_file(&proc_net_dev);
    if ((proc_net_dev.last_read.tv_sec != stamp.tv_sec) &&
            (proc_net_dev.last_read.tv_usec != stamp.tv_usec))
    {
        /*  skip past the two-line header ... */
        p = index (p, '\n') + 1;
        p = index (p, '\n') + 1;

        /*
         * Compute timediff. Check for bogus delta-t
         */
        t = timediff(&proc_net_dev.last_read, &stamp);
        if ( t <  proc_net_dev.thresh) {
            plugin_print_log("update_ifdata(%s) - Dubious delta-t: %f", caller, t);
            return;
        }
        stamp = proc_net_dev.last_read;

        while (*p != 0x00)
        {
            /*  skip past the interface tag portion of this line */
            /*  but save the name of the interface (hash key) */
            char *src;
            size_t n = 0;

            char if_name[IFNAMSIZ];
            int vlan = 0; // vlan flag
            while (p != 0x00 && isblank(*p))
                p++;

            src = p;
            while (p != 0x00 && *p != ':')
            {
                n++;
                p++;
            }

            p = index(p, ':');

            /* l.flis: check whether iface is vlan */
            if (p && n < IFNAMSIZ) {
                strncpy(if_name,src,IFNAMSIZ);
                if_name[n] = '\0';
                vlan = is_vlan_iface(if_name);
            };

            /* Ignore 'lo' and 'bond*' interfaces (but sanely) */
            /* l.flis: skip vlan interfaces to avoid double counting*/
            if (p && strncmp (src, "lo", 2) &&
                    strncmp (src, "bond", 4) && !vlan)
            {
                p++;
                /* Check for data from the last read for this */
                /* interface.  If nothing exists, add to the table. */
                ns = hash_lookup(src, n);
                if ( !ns ) return;

                /* receive */
                rbi = strtostat(p, &p ,10);
                if ( rbi >= ns->rbi ) {
                    bytes_in_t = rbi - ns->rbi;
                } else {
                    plugin_print_log("update_ifdata(%s) - Overflow in rbi: %"PRI_STAT" -> %"PRI_STAT,caller,ns->rbi,rbi);
                    bytes_in_t = STAT_MAX - ns->rbi + rbi;
                }
                l_bytes_in += bytes_in_t;
                ns->rbi = rbi;

                rpi = strtostat(p, &p ,10);
                if ( rpi >= ns->rpi ) {
                    l_pkts_in += rpi - ns->rpi;
                } else {
                    plugin_print_log("updata_ifdata(%s) - Overflow in rpi: %"PRI_STAT" -> %"PRI_STAT,caller,ns->rpi,rpi);
                    l_pkts_in += STAT_MAX - ns->rpi + rpi;
                }
                ns->rpi = rpi;

                /* skip unneeded metrics */
                for (i = 0; i < 6; i++) rbo = strtostat(p, &p, 10);

                /* transmit */
                rbo = strtostat(p, &p ,10);
                if ( rbo >= ns->rbo ) {
                    bytes_out_t = rbo - ns->rbo;
                } else {
                    plugin_print_log("update_ifdata(%s) - Overflow in rbo: %"PRI_STAT" -> %"PRI_STAT,caller,ns->rbo,rbo);
                    bytes_out_t = STAT_MAX - ns->rbo + rbo;
                }
                l_bytes_out += bytes_out_t;
                ns->rbo = rbo;
                rpo = strtostat(p, &p ,10);
                if ( rpo >= ns->rpo ) {
                    l_pkts_out += rpo - ns->rpo;
                } else {
                    plugin_print_log("update_ifdata(%s) - Overflow in rpo: %"PRI_STAT" -> %"PRI_STAT,caller,ns->rpo,rpo);
                    l_pkts_out += STAT_MAX - ns->rpo + rpo;
                }
                ns->rpo = rpo;
                if (t > 0) {
                    ns->bis = (bytes_in_t/1024) / t;
                    ns->bos = (bytes_out_t/1024) / t;
                }
            }
            p = index (p, '\n') + 1;
        }

        /*
         * Compute rates in local variables
         */
        l_bin = l_bytes_in / t;
        l_bout = l_bytes_out / t;
        l_pin = l_pkts_in / t;
        l_pout = l_pkts_out / t;

        /*
         * Check for "invalid" data, caused by HW error. Throw away dubious data points
         * FIXME: This should be done per-interface, with threshholds depending on actual link speed
         */
        if ((l_bin > 1.0e13) || (l_bout > 1.0e13) ||
                (l_pin > 1.0e8)  || (l_pout > 1.0e8)) {
            plugin_print_log("update_ifdata(%s): %g %g %g %g / %g", caller,
                    l_bin, l_bout, l_pin, l_pout, t);
            return;
        }
        /*
         * Finally return Values
         */
        bytes_in  = l_bin;
        bytes_out = l_bout;
        pkts_in   = l_pin;
        pkts_out  = l_pout;
    }

    return;
}

void update_speed(void) {
    update_ifdata("BI");
}

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

    JT
total_jiffies_func ( void )
{
    char *p;
    JT user_jiffies, nice_jiffies, system_jiffies, idle_jiffies,
       wio_jiffies, irq_jiffies, sirq_jiffies, steal_jiffies;

    p = update_file(&proc_stat);
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

    JT
core_total_jiffies_func ( int core )
{
    char *p, *tmp;
    int idx;
    JT user_jiffies, nice_jiffies, system_jiffies, idle_jiffies,
       wio_jiffies, irq_jiffies, sirq_jiffies, steal_jiffies;

    p = update_file(&proc_stat);
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

int get_core_num ( void )
{
    char *p, *tmp;
    int count = 0;
    p = update_file(&proc_stat);
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

float cpu_system_func ( void )
{
    char *p;
    float val;
    static struct timeval stamp={0, 0};
    static JT last_system_jiffies,  system_jiffies,
              last_total_jiffies, total_jiffies, diff;

    p = update_file(&proc_stat);
    if((proc_stat.last_read.tv_sec != stamp.tv_sec) &&
            (proc_stat.last_read.tv_usec != stamp.tv_usec)) {
        stamp = proc_stat.last_read;

        p = skip_token(p);
        p = skip_token(p);
        p = skip_token(p);
        system_jiffies = strtod( p, (char **)NULL );
        if (num_cpustates > NUM_CPUSTATES_24X) {
            p = skip_token(p);
            p = skip_token(p);
            p = skip_token(p);
            system_jiffies += strtod( p, (char **)NULL ); /* "intr" counted in system */
            p = skip_token(p);
            system_jiffies += strtod( p, (char **)NULL ); /* "sintr" counted in system */
        }
        total_jiffies = total_jiffies_func();

        diff = system_jiffies - last_system_jiffies;

        if ( diff )
            val = ((double)diff/(double)(total_jiffies - last_total_jiffies)) * 100.0;
        else
            val = 0.0;

        val = sanityCheck( __LINE__, __FILE__, __FUNCTION__, val, (double)diff, (double)(total_jiffies - last_total_jiffies), system_jiffies, last_system_jiffies, total_jiffies, last_total_jiffies );

        last_system_jiffies  = system_jiffies;
        last_total_jiffies = total_jiffies;

    }
    return val;
}

float cpu_aidle_func ( void )
{
    char *p;
    float val;
    JT idle_jiffies, total_jiffies;

    p = update_file(&proc_stat);

    p = skip_token(p);
    p = skip_token(p);
    p = skip_token(p);
    p = skip_token(p);
    idle_jiffies  = (JT) strtod( p , (char **)NULL );
    total_jiffies = total_jiffies_func();

    val = ((double)idle_jiffies/(double)total_jiffies) * 100.0;

    val = sanityCheck( __LINE__, __FILE__, __FUNCTION__, val, (double)idle_jiffies, (double)total_jiffies, idle_jiffies, total_jiffies, 0, 0 );
    return val;
}

#define MAX_CORE_NUM    (128)
struct core_stat {
    JT  last_idle_jiffies;
    JT  last_total_jiffies;
} stats[MAX_CORE_NUM] = {0};

float cpu_core_usage(int core)
{
    char *p, *tmp;
    float val;
    int idx;
    JT idle_jiffies, total_jiffies, diff;
    p = update_file(&proc_stat);
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
    total_jiffies = core_total_jiffies_func(core);
    diff = idle_jiffies - stats[core].last_idle_jiffies;
    if (diff) {
        val = ((double)(diff * 100)/(double)(total_jiffies - stats[core].last_total_jiffies));
    } else {
        val = 0.0;
        printf("%llu %llu %llu %llu\n", idle_jiffies, total_jiffies, stats[core].last_idle_jiffies, stats[core].last_total_jiffies);
    }
    stats[core].last_idle_jiffies  = idle_jiffies;
    stats[core].last_total_jiffies = total_jiffies;
    return val;
}

float cpu_idle_func ( void )
{
    char *p;
    float val;
    static struct timeval stamp={0, 0};
    static JT last_idle_jiffies,  idle_jiffies,
              last_total_jiffies, total_jiffies, diff;

    p = update_file(&proc_stat);
    if((proc_stat.last_read.tv_sec != stamp.tv_sec) &&
            (proc_stat.last_read.tv_usec != stamp.tv_usec)) {
        stamp = proc_stat.last_read;

        p = skip_token(p);
        p = skip_token(p);
        p = skip_token(p);
        p = skip_token(p);
        idle_jiffies  = strtod( p , (char **)NULL );
        total_jiffies = total_jiffies_func();

        diff = idle_jiffies - last_idle_jiffies;

        if ( diff )
            val = ((double)diff/(double)(total_jiffies - last_total_jiffies)) * 100.0;
        else
            val = 0.0;

        val = sanityCheck( __LINE__, __FILE__, __FUNCTION__, val, (double)diff, (double)(total_jiffies - last_total_jiffies), idle_jiffies, last_idle_jiffies, total_jiffies, last_total_jiffies );

        last_idle_jiffies  = idle_jiffies;
        last_total_jiffies = total_jiffies;

    }

    return val;
}


struct nlist {
    struct nlist *next;
    char *name;
};

#define DFHASHSIZE 101
static struct nlist *DFhashvector[DFHASHSIZE];

/* --------------------------------------------------------------------------- */
unsigned int DFhash(const char *s)
{
    unsigned int hashval;
    for (hashval=0; *s != '\0'; s++)
        hashval = *s + 31 * hashval;
    return hashval % DFHASHSIZE;
}

/* --------------------------------------------------------------------------- */
/* From K&R C book, pp. 144-145 */
struct nlist * seen_before(const char *name)
{
    struct nlist *found=0, *np;
    unsigned int hashval;

    /* lookup */
    hashval=DFhash(name);
    for (np=DFhashvector[hashval]; np; np=np->next) {
        if (!strcmp(name,np->name)) {
            found=np;
            break;
        }
    }
    if (!found) {    /* not found */
        np = (struct nlist *) malloc(sizeof(*np));
        if (!np || !(np->name = (char *) strdup(name)))
            return NULL;
        np->next = DFhashvector[hashval];
        DFhashvector[hashval] = np;
        return NULL;
    }
    else /* found name */
        return found;
}

/* --------------------------------------------------------------------------- */
void DFcleanup()
{
    struct nlist *np, *next;
    int i;
    for (i=0; i<DFHASHSIZE; i++) {
        /* Non-standard for loop. Note the last clause happens at the end of the loop. */
        for (np = DFhashvector[i]; np; np=next) {
            next=np->next;
            free(np->name);
            free(np);
        }
        DFhashvector[i] = 0;
    }
}

/* --------------------------------------------------------------------------- */
int remote_mount(const char *device, const char *type)
{
    /* From ME_REMOTE macro in mountlist.h:
       A file system is `remote' if its Fs_name contains a `:'
       or if (it is of type smbfs and its Fs_name starts with `//'). */
    return ((strchr(device,':') != 0)
            || (!strcmp(type, "smbfs") && device[0]=='/' && device[1]=='/')
            || (!strncmp(type, "nfs", 3)) || (!strcmp(type, "autofs"))
            || (!strcmp(type,"gfs")) || (!strcmp(type,"none")) );
}

/* --------------------------------------------------------------------------- */
float device_space(char *mount, char *device, double *total_size, double *total_free)
{
    struct statvfs svfs;
    double blocksize;
    double free;
    double size;
    /* The percent used: used/total * 100 */
    float pct=0.0;

    /* Avoid multiply-mounted disks - not done in df. */
    if (seen_before(device)) return pct;

    if (statvfs(mount, &svfs)) {
        /* Ignore funky devices... */
        return pct;
    }

    free = svfs.f_bavail;
    size  = svfs.f_blocks;
    blocksize = svfs.f_bsize;
    /* Keep running sum of total used, free local disk space. */
    *total_size += size * blocksize;
    *total_free += free * blocksize;
    /* The percentage of space used on this partition. */
    pct = size ? ((size - free) / (double) size) * 100 : 0.0;
    return pct;
}

/* --------------------------------------------------------------------------- */
float find_disk_space(double *total_size, double *total_free)
{
    FILE *mounts;
    char procline[1024];
    char *mount, *device, *type, *mode, *other;
    /* We report in GB = 1e9 bytes. */
    double reported_units = 1e6;
    /* Track the most full disk partition, report with a percentage. */
    float thispct, max=0.0;

    /* Read all currently mounted filesystems. */
    mounts=fopen(MOUNTS,"r");
    if (!mounts) {
        printf("Df Error: could not open mounts file %s. Are we on the right OS?\n", MOUNTS);
        return max;
    }
    while ( fgets(procline, sizeof(procline), mounts) ) {
        device = procline;
        mount = index(procline, ' ');
        if (mount == NULL) continue;
        *mount++ = '\0';
        type = index(mount, ' ');
        if (type == NULL) continue;
        *type++ = '\0';
        mode = index(type, ' ');
        if (mode == NULL) continue;
        *mode++ = '\0';
        other = index(mode, ' ');
        if (other != NULL) *other = '\0';
        if (!strncmp(mode, "ro", 2)) continue;
        if (remote_mount(device, type)) continue;
        if (strncmp(device, "/dev/", 5) != 0 &&
                strncmp(device, "/dev2/", 6) != 0) continue;
        thispct = device_space(mount, device, total_size, total_free);
        plugin_print_log("Counting device %s (%.2f %%)\n", device, thispct);
        if (!max || max<thispct)
            max = thispct;
    }
    fclose(mounts);

    *total_size = *total_size / reported_units;
    *total_free = *total_free / reported_units;
    plugin_print_log("For all disks: %.3f MB total, %.3f MB free for users.\n", *total_size, *total_free);

    DFcleanup();
    return max;
}
