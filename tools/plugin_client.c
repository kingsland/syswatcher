#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include "plugin_protocol.h"
#include "srm_errno.h"

#define ERROR_INFO(fmt, args...) printf("\033[1;31;40m* ERROR :\033[0m "fmt"\n", ##args)


void help_information(const char *info)
{
    printf(" _____________________________________________________________\n");
    printf("|_______________ \033[1;33;40mSystem Resource Monitor Tool\033[0m ________________|\n");
    printf("|                                                             |\n");
    printf("    \033[1mUsage:\033[0m \n");
    printf("           \033[1m%s  <load|unload|reload>  <path>\033[0m\n", info);
    printf("|_____________________________________________________________|\n");

    return;
}

char* plugin_path_parser(char* path)
{
    char *p = path;
    char buf[2048] = {0x0};
    char *plugin_path = NULL;
    unsigned length, buf_length, path_length;

    if (!access(p, R_OK)) {
        if (*p != '/') {
            if (getcwd(buf, 2048) == NULL) {
                ERROR_INFO("Operation failed,please try again.");
            } else {
                length = strlen(buf) + strlen(path) + 1;
                plugin_path = (char*)malloc(length + 1);
                if (plugin_path != NULL) {
                    strncpy(plugin_path, buf, strlen(buf));
                    plugin_path[strlen(buf)]= '/';
                    plugin_path[strlen(buf) + 1]= '\0';
                    strncat(plugin_path,path, strlen(path));
                    plugin_path[length]= '\0';
                } else {
                    ERROR_INFO("Operation failed,please try again.");
                }
            }
        } else {
            plugin_path = path;
        }
    } else {
        ERROR_INFO("Path does not exist,please check file path to try again.");
    }

    return plugin_path;
}

int main(int argc, char* argv[])
{
    int ret = SRM_OK;
    int fd;
    unsigned char  type = 1;
    int len = 4, wt_len, wl_len;
    plugin_cmd_t cmd = {0x0};
    char *plugin_path = NULL;
    if (argc < 3) {
        help_information(strrchr(argv[0], '/')+1);
        ret = SRM_PARAMETER_ERR;
    } else {
        cmd.argc = argc - 3;
        plugin_path = plugin_path_parser(argv[2]);
        if (plugin_path == NULL)
            ret = SRM_PARAMETER_ERR;

        if (!strncmp(argv[1], "load", 4)) {
            printf("load ");
            type = 0;
        } else if (!strncmp(argv[1], "unload", 6)) {
            printf("unload ");
            type = 1;
        } else if (!strncmp(argv[1], "reload", 6)) {
            printf("reload ");
            type = 2;
        } else {
            help_information(strrchr(argv[0], '/')+1);
            ret = SRM_PARAMETER_ERR;
        }

        if (ret == SRM_OK) {
            fd = open(COMUNICATION_CMD, O_WRONLY);
            if (fd < 0) {
                ERROR_INFO("The SRM is not running,please start the SRM or contact the administrator first.");
                ret = SRM_PARAMETER_ERR;
            } else {
                int idx = 0;
                int arg_len;
                cmd.type = (type & 0x0F) | 0x90;
                cmd.flag = 0x6B;
                sprintf(cmd.path, "%s", plugin_path);
                printf("%s ", cmd.path);
                bzero(cmd.argv, MAX_ARGV * ARGV_LEN);
                for (idx = 0; (idx < cmd.argc) && (idx < MAX_ARGV); idx++) {
                    bzero(cmd.argv[idx], ARGV_LEN);
                    arg_len = strlen(argv[idx + 3]) + 1;
                    snprintf(cmd.argv[idx], arg_len > ARGV_LEN?ARGV_LEN:arg_len,
                        "%s", argv[idx + 3]);
                    printf("%s ", cmd.argv[idx]);
                }
                printf("\n");
                wl_len = write(fd, &cmd, sizeof(plugin_cmd_t));
                close(fd);
            }

        }
    }

    return ret;
}

