#include <plugin_def.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define FILELEN     (256)
int os_release_collect(item_t *data)
{
#if 0
#define LSB_FILE    "/etc/os-release"
#define SPVERS_STR  "DISTRIB_KYLIN_RELEASE="
      uint8_t spVersionH, spVersionL;
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
         strptr = strstr(content, "DISTRIB_KYLIN_RELEASE=");
         if (strstr == NULL) {
             spVersionH = 0xf;
             spVersionL = 0xf;
         } else {
             strptr = strstr(strptr, "=");
             strptr = strstr(strptr, "SP");
             changeline = strstr(strptr, "\n");
             *changeline = '\0';
             strptr += 2;
             spVersionL = 0;
             sscanf(strptr, "%hhu", &spVersionH);
         }
         = (spVersionH<<4) | (spVersionL);
         close(fd);
     }
#endif
    
     sprintf(data->data[0].name, "OS-version");
     sprintf(data->data[0].unit,  "0x80" );
     return 0;
}

int bios_release_collect(item_t *data)
{
    uint8_t versH, versL;
    versH = 0x03;
    versL = 0x01;  

    sprintf(data->data[0].name, "bios-version");
    sprintf(data->data[0].unit, versH<<4 | versL);
}

collect_item_t items[] = {
    {
        .item_name = "os-release",
        .item_desc = "system version",
        .run_once = false,
        .collect_data_func = os_release_collect,
        .interval = 1,
        .data_count = 1,
    },

    {
        .item_name = "bios-release",
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
