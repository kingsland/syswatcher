#include <plugin_def.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <plugin_ext_def.h>
#include <dmidecode.h>
#include <util.h>

#define FILELEN     (256)

int os_release_collect(item_t *data)
{
    char *buf;
    const char *cmd = "lsb_release -i";
    const char *cmd_ver = "lsb_release -r";
    char exec_cmd[1024];
    char exec_cmd_1[1024];
    FILE* fp;
    FILE*fd;
    char *strpstr_name;
    char *strpstr_ver;
    char *changeline;
    char *changeline_1;
    //os name
    fp = popen(cmd, "r");
    if(fp == NULL){
        sprintf(data->data[0].name, "This system is not a standard linux.");
    }
    fgets(exec_cmd, sizeof(exec_cmd), fp);
    pclose(fp);
    strpstr_name = strstr(exec_cmd, ":");
    changeline_1 = strstr(strpstr_name, "\n");
    *changeline_1 = '\0';
    strpstr_name += 2;

    //os ver
    fd = popen(cmd_ver, "r");
    if(fd == NULL){
        sprintf(data->data[0].name, "This system is not a standard linux.");
    }
    fgets(exec_cmd_1, sizeof(exec_cmd_1), fd);
    pclose(fd);
    strpstr_ver = strstr(exec_cmd_1, ":");
    changeline = strstr(strpstr_ver, "\n");
    *changeline = '\0';
    strpstr_ver += 2;


    sprintf(data->data[0].name, "distributor");
    data->data[0].t = M_STRING;
    sprintf(data->data[0].val.str, "%s", strpstr_name);

    sprintf(data->data[1].name, "version");
    data->data[1].t = M_STRING;
    sprintf(data->data[1].val.str, "%s", strpstr_ver);



#if 0
        #define LSB_FILE    "/home/juan/work/cVPX6-EON-D-XK-2/kyinfo"
        #define SPVERS_STR  "dist_id="
        uint8_t spVersionH, spVersionL;
        // uint32_t revision;
        int fd;
        char content[FILELEN];
        char *strptr, *changeline;
        struct stat buf;
        if (stat(LSB_FILE, &buf) < 0) {
            spVersionH = 0xf;
            spVersionL = 0xf;
        } else {
            fd = open(LSB_FILE, O_RDONLY);
            read(fd, content, FILELEN);
            strptr = strstr(content, SPVERS_STR);
            if (strstr == NULL) {
                spVersionH = 0xf;
                spVersionL = 0xf;
            } else {
                strptr = strstr(strptr, "=");
                strptr = strstr(strptr, "sp");
                changeline = strstr(strptr, "\n");
                *changeline = '\0';
                strptr += 2;
                spVersionL = 0;
                sscanf(strptr, "%hhu", &spVersionH);
            }

            close(fd);
        }

        sprintf(data->data[0].name, "OS Revision:");
        data->data[0].t = M_UINT32;
        data->data[0].val.uint32= (spVersionH<<4) | (spVersionL);
 #endif
    return 0;
}

extern char dmidecode_val[24];
int bios_release_collect(item_t *data)
{
    dmi_bios_version();
    sprintf(data->data[0].name, "BIOS Revision");
    data->data[0].t = M_STRING;
    sprintf( data->data[0].val.str, "%s", dmidecode_val);  //固件版本

}

collect_item_t items[] = {
    {
        .item_name = "OS-Iinformation",
        .item_desc = "system version",
        .run_once = false,
        .collect_data_func = os_release_collect,
        .interval = 1,
        .data_count = 2,
    },

    {
        .item_name = "BIOS-Infortion",
        .item_desc = "it is bios version",
        .run_once = false,
        .collect_data_func = bios_release_collect,
        .interval = 1,
        .data_count = 1,
    },
};

plugin_info_t pluginfo = {
    .name = "sysinfo",
    .desc = "included bios-version and os-version",
    .item_count = 2,
};


PLUGIN_ENTRY(sysinfo, plugin_info)
{
    PLUGIN_INIT(plugin_info, items, &pluginfo);
    return 0;
}

PLUGIN_EXIT(sysinfo, plugin_info)
{
    PLUGIN_FREE(plugin_info);
    return;
}
