#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "plugin_protocol.h"
#include "srm_errno.h"

#define ERROR_INFO(fmt, args...) printf("\033[1;31;40m* ERROR :\033[0m "fmt"\n", ##args)


void help_information(const char *info)
{
    printf(" _____________________________________________________________\n");
    printf("|______________ \033[1;33;40mSystem Resource Monitor Toolkit\033[0m ______________|\n");
    printf("|                                                             |\n");
    printf("    \033[1mUsage:\033[0m \n");
    printf("           \033[1m%s  <load|unload>  <path>\033[0m\n", info);
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

    if (argc != 3) {
        help_information(strrchr(argv[0], '/')+1);
        ret = SRM_PARAMETER_ERR;
    } else {
        plugin_path = plugin_path_parser(argv[2]);
        if (plugin_path == NULL)
            ret = SRM_PARAMETER_ERR;

        if (!strncmp(argv[1], "load", 4))
            type = 0;
        else if (!strncmp(argv[1], "unload", 6))
            type = 1;
        else {
            help_information(strrchr(argv[0], '/')+1);
            ret = SRM_PARAMETER_ERR;
        }

        if (ret == SRM_OK) {
            fd = open(COMUNICATION_CMD, O_WRONLY);
            if (fd < 0) {
                ERROR_INFO("The SRM is not running,please start the SRM or contact the administrator first.");
                ret = SRM_PARAMETER_ERR;
            } else {
                cmd.type = (type & 0x0F) | 0x90;
                cmd.flag = 0x6B;
                cmd.size = strlen(plugin_path) + 1;
                memcpy(cmd.buffer, plugin_path, cmd.size);
                cmd.buffer[cmd.size -1] = '\0';
                len += cmd.size;

                for (int i=0; i<cmd.size+4; i++)
                    printf("%02X ",((char*)(&cmd))[i]);
                printf("\n");
                for (int i=4; i<cmd.size+4; i++)
                    printf("%c",((char*)(&cmd))[i]);
                printf("\n");

                len = 4+cmd.size;
                wl_len = write(fd, &cmd, len);

                /*while (len > 0) {
                    wl_len = write(fd, ((char*)(&cmd))+wt_len, len);
                    if (wl_len < 0)
                        perror("write");
                    wt_len += wl_len;
                    len -= wl_len;
                }*/

                close(fd);
            }

        }
    }

    return ret;
}

