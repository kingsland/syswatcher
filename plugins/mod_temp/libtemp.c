#include <plugin_def.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#define PATH_LEN    (64)
#define HWMON_COUNT (128)
char hwmon_entry[HWMON_COUNT][PATH_LEN];
char cputemp_entry[2*PATH_LEN];

void set_data_collect(mate_t *data_set, char *name,
        char *unit, enum data_type t, union val_t v)
{
    sprintf(data_set->name, "%s", name);
    sprintf(data_set->unit, "%s", unit);
    data_set->t = t;
    data_set->val = v;
}

int temperature_collect(item_t *data)
{
    char temp_str[32];
    int temp_int;
    val_t temp;
    int fd;
    int ret;
    bzero(temp_str, 32);
    mate_t *data_set = data->data;
    temp.int32 = 0;
    if (access(cputemp_entry, R_OK) == 0) {
        fd = open(cputemp_entry, O_RDONLY);
        ret = read(fd, temp_str, 32);
        if (ret > 0) {
            temp.int32 = atoi(temp_str)/1000;
        }
        close(fd);
    }

    set_data_collect(&(data_set[0]), "cpu_temp", "C", M_INT32, temp);
    return 0;
}

collect_item_t items[] = {
    {
        .item_name = "cpu temp",
        .item_desc = "temperature of cpu",
        .run_once = false,
        .collect_data_func = temperature_collect,
        .interval = 1,
        .data_count = 1,
    },
};

plugin_info_t pluginfo = {
    .name = "temps",
    .desc = "temperature of this board",
    .item_count = 1,
};

void find_entry(char path_array[][PATH_LEN])
{
    char path_prefix[32];
    char path[64];
    int count = 0;
    bzero(path_array, (PATH_LEN * HWMON_COUNT));
    sprintf(path_prefix, "/sys/class/hwmon/hwmon");
    while(1) {
        bzero(path, 64);
        sprintf(path, "%s%d", path_prefix, count);
        if (access(path, F_OK) == 0) {
            sprintf(path_array[count], "%s", path);
        } else {
            break;
        }
        count++;
    }
}

int find_cputemp_entry(char *hwmon_entry)
{
    char hwmon_name_path[PATH_LEN];
    char hwmon_name[32];
    size_t len;
    int fd;
    bzero(hwmon_name_path, PATH_LEN);
    sprintf(hwmon_name_path, "%s%s", hwmon_entry, "/name");
    if ((fd = open(hwmon_name_path, O_RDONLY)) <= 0) {
        return 1;
    }
    len = read(fd, hwmon_name, 16);
    close(fd);
    if (len > 0) {
        if ((strncmp(hwmon_name, "coretemp", strlen("coretemp")) == 0) ||
            (strncmp(hwmon_name, "cpu-hwmon", strlen("cpu-hwmon")) == 0)) {
            return 0;
        }
    }
    return 1;
}

PLUGIN_ENTRY(temp, plugin_info)
{
    PLUGIN_INIT(plugin_info, items, &pluginfo);
    int count;
    bzero(hwmon_entry, HWMON_COUNT * PATH_LEN);
    bzero(cputemp_entry, sizeof(cputemp_entry));
    find_entry(hwmon_entry);
    for (count = 0; count < HWMON_COUNT; count++) {
        if (strlen(hwmon_entry[count]) != 0) {
            if (find_cputemp_entry(hwmon_entry[count]) == 0) {
                sprintf(cputemp_entry, "%s/temp1_input", hwmon_entry[count]);
                break;
            }
        }
    }
    return 0;
}

PLUGIN_EXIT(temp, plugin_info)
{
    PLUGIN_FREE(plugin_info);
    return;
}
