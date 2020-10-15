#include <plugin_def.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/statvfs.h>

#define MOUNTS "/proc/mounts"
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
        //print_info("Counting device %s (%.2f %%)\n", device, thispct);
        if (!max || max<thispct)
            max = thispct;
    }
    fclose(mounts);

    *total_size = *total_size / reported_units;
    *total_free = *total_free / reported_units;
    //print_info("For all disks: %.3f MB total, %.3f MB free for users.\n", *total_size, *total_free);

    DFcleanup();
    return max;
}

int disk_data_collect(item_t *data)
{
    double total_free = 0.0;
    double total_size = 0.0;
    find_disk_space(&total_size, &total_free);
    sprintf(data->data[0].name, "disk total");
    sprintf(data->data[0].unit, "MB");
    data->data[0].t = M_DOUBLE;
    data->data[0].val.d = total_size;

    sprintf(data->data[1].name, "disk free");
    sprintf(data->data[1].unit, "MB");
    data->data[1].t = M_DOUBLE;
    data->data[1].val.d = total_free;

    sprintf(data->data[2].name, "disk usage");
    sprintf(data->data[2].unit, "%%");
    data->data[2].t = M_DOUBLE;
    data->data[2].val.d = (100.0 * (total_size - total_free))/total_size;

    return 0;
}

collect_item_t items[] = {
    {
        .item_name = "disk usage",
        .item_desc = "disk total free usage",
        .run_once = false,
        .collect_data_func = disk_data_collect,
        .interval = 1,
        .data_count = 3,
    },
};

plugin_info_t pluginfo = {
    .name = "disk misc",
    .desc = "disk informations",
    .item_count = 1,
};

PLUGIN_ENTRY(disk, plugin_info)
{
    PLUGIN_INIT(plugin_info, items, &pluginfo);
    return 0;
}

PLUGIN_EXIT(disk, plugin_info)
{
    PLUGIN_FREE(plugin_info);
    return;
}
