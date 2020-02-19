#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include "plugin_protocol.h"
#include <errno.h>
#include <string.h>

int initialize(void)
{
    bool status = false;
    int fd;
    /*
    if ((fd = open(COMUNICATION_CMD, O_WRONLY| O_NONBLOCK | O_EXCL | O_CREAT, 0766)) <0) {
        if (errno == EEXIST) {
            if ((fd = open(COMUNICATION_CMD, O_WRONLY| O_NONBLOCK, 0766)) < 0){
                perror("open "COMUNICATION_CMD);
            } else {
                status = true;
            }
        }
    }else {
        status = true;
    }*/
    fd = open(COMUNICATION_CMD, O_WRONLY);
    return fd;
}

int main(int argc, char* argv[])
{
    int fd;
    /*int type = atoi(argv[1]);
    */unsigned char  type = 1;
    int len = 4, wt_len, wl_len;
    plugin_cmd_t cmd = {0x0};

    if (argc != 3) {
        printf("Usage: %s  load|unload  path\n", strrchr(argv[0], '/')+1);
        return -1;
    }
    else if (!strncmp(argv[1], "load", 4))
        type = 0;
    else if (!strncmp(argv[1], "unload", 6))
        type = 1;
    else {
        printf("Usage: %s  load|unload  path\n", strrchr(argv[0], '/')+1);
        return -1;
    }
    
    fd = initialize();

    cmd.type = (type & 0x0F) | 0x90;
    cmd.flag = 0x6B;
    cmd.size = strlen(argv[2]) + 1;
    memcpy(cmd.buffer, argv[2], cmd.size);
    cmd.buffer[cmd.size -1] = '\0';
    
    printf("Cmd:%x , size:%hd, flag:%#x, buf:%s\n", cmd.type, cmd.size, cmd.flag, cmd.buffer);

    len += cmd.size;
    
    for (int i=0; i<cmd.size+4; i++)
        printf(" %02X ",((char*)(&cmd))[i]);
    printf("\n");
    for (int i=4; i<cmd.size+4; i++)
        printf("%c",((char*)(&cmd))[i]);
    printf("\n");
    fd = open(COMUNICATION_CMD, O_WRONLY|O_NONBLOCK);
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
    
    return 0;
}

