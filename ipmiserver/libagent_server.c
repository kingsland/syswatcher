#include <stdlib.h>
#include <libagent_server.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <stdint.h>
#include <unistd.h>
#include <defs.h>
#include <stddef.h>
#include <IPMI.h>
#include <SysMonitor.h>
#include <string.h>

struct req {
    uint32_t cmd;
    uint32_t data_len;
    uint8_t args[128];
};

struct resp {
    uint32_t cmd;
    int32_t ret;
    uint32_t data_len;
    uint8_t msg[128];
};

void reuse_socket(int fd) {
    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(int));
}

int init_srv(int port)
{
    int listenfd;
    socklen_t len;
    struct sockaddr_in srvaddr;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        printf("open socket failed\n");
        exit(1);
    }
    reuse_socket(listenfd);
    bzero(&srvaddr, sizeof(srvaddr));
    srvaddr.sin_family = AF_INET;
    srvaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    srvaddr.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr *)&srvaddr, sizeof(srvaddr)) < 0) {
        printf("bind failed tw? check it.\n");
        exit(1);
    }
    listen(listenfd, 5);
    return listenfd;
}

#define NET_CMD_INVALID (0xff)
#define NET_CMD_RGST    (0x01)
#define NET_CMD_USRDATA (0x02)
#define NET_CMD_BIT     (0x03)

void req_bit(int fd, struct req *r) {
    struct resp rsp;
    int ret;
    bzero(&rsp, sizeof(rsp));
    rsp.ret = 0;
    rsp.cmd = NET_CMD_BIT;
    rsp.data_len = sizeof(NodeAgentInfo_T);
    *((NodeAgentInfo_T *)rsp.msg) = gNodeAgentInfo;
    ret = write(fd, &rsp, sizeof(rsp));
    return;
}

void req_usrdata(int fd, struct req *r) {
    NodeAgentInfo_T node_info = gNodeAgentInfo;
    uint32_t data_space = sizeof(((NodeAgentInfo_T *)0)->Reserved);
    int ret;
    int idx;
    char cmd[1024];
    struct resp rsp;
    bzero(&rsp, sizeof(rsp));
    rsp.ret = -1;
    rsp.cmd = NET_CMD_USRDATA;
    if (r->data_len > data_space) {
        rsp.ret = -1;
    } else {
        rsp.ret = 0;
        memcpy(node_info.Reserved, r->args, r->data_len);
    }
    strcpy (cmd, "ipmitool -U admin -P admin raw 0x2e 0x50");
    for (idx = 0; idx < sizeof(node_info); idx++) {
        sprintf(cmd, "%s 0x%02x", cmd, *(((uint8_t *)&node_info) + idx));
    }
    system(cmd);
    ret = write(fd, &rsp, sizeof(rsp));
    return;
}

void req_rgst(int fd, struct req *r) {
    struct resp rsp;
    int ret;
    bzero(&rsp, sizeof(rsp));
    printf("%s\n", r->args);
    rsp.ret = 0;
    rsp.cmd = NET_CMD_RGST;
    ret = write(fd, &rsp, sizeof(rsp));
    return;
}

void *req_handler(void *param)
{
    int fd = *(int *)param;
    pthread_detach(pthread_self());
    struct req r;
    r.cmd = NET_CMD_INVALID;
    if (read(fd, &r, sizeof(r)) < 0) {
        print_err("get req failed\n");
        goto get_req_failed;
    }
    switch(r.cmd) {
        case NET_CMD_RGST: {
            req_rgst(fd, &r);
            break;
        }
        case NET_CMD_USRDATA: {
            req_usrdata(fd, &r);
            break;
        }
        case NET_CMD_BIT: {
            req_bit(fd, &r);
            break;
        }
        default: {}
    }
get_req_failed:
    close(fd);
    free(param);//yes you should free this here.
    //we don't handle this outside this thread.
    param = NULL;
}

void *AgentServer(void *param) {
    int listenfd = *((int32_t *)param);
    struct sockaddr_in cliaddr;
    socklen_t len;
    pthread_t id;

    while(1) {
        int *cli_fd = (int *)malloc(sizeof(int));
        *cli_fd = accept(listenfd, (struct sockaddr *)&cliaddr, &len);
        pthread_create(&id, NULL, req_handler, cli_fd);
    }
    return 0;
}


