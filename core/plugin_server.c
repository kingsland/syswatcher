#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include "plugin_server.h"
#include "plugin_protocol.h"
#include "plugin_mgr.h"
#include <dlfcn.h>
#include <errno.h>
#include "list.h"
//#include <setjmp.h>
//#include <signal.h>
//
//typedef struct plugin_obj
//{
//    int fd;
//    struct {
//        jmp_buf jmp_env;
//        int current_sig;
//        char occurred[64];
//    } sig;
//    plugin_mgr_t *mgr;
//    void* args;
//}plugin_obj_t;
//
//
static plugin_mgr_t *g_mgr = NULL;
//static plugin_obj_t plugin_object;
//
//void signal_hdl(int sig)
//{
//    plugin_object.sig.current_sig = sig;
//    plugin_object.sig.occurred[sig - 1]++;
//    longjmp(plugin_object.sig.jmp_env, sig);
//
//    return;
//}
//
//void signal_reg(void)
//{
//    signal(SIGHUP, signal_hdl);   // 1
//    signal(SIGINT, signal_hdl);   // 2
//    signal(SIGQUIT, signal_hdl);  // 3
//    signal(SIGILL, signal_hdl);   // 4
//    signal(SIGABRT, signal_hdl);  // 6
//    signal(SIGBUS, signal_hdl);   // 7
//    signal(SIGFPE, signal_hdl);   // 8
//    signal(SIGKILL, signal_hdl);  // 9
//    signal(SIGUSR1, signal_hdl);  // 10
//    signal(SIGSEGV, signal_hdl);  // 11
//    signal(SIGUSR2, signal_hdl);  // 12
//    signal(SIGTERM, signal_hdl);  // 15
//
//    return;
//}
//
static void* plugin_run(void*);

int plugin_server_start(int (*load)(void *, plugin_channel_t *),
                                 int (*unload)(void *,  plugin_key_t),
                                 void* context)
{
    int ret = -1;

    if (!plugin_mgr_check(g_mgr)) {
        g_mgr = plugin_mgr_init();
        if (g_mgr != NULL && plugin_mgr_check(g_mgr)) {
            g_mgr->load = load;
            g_mgr->unload = unload;
            g_mgr->context = context;
            g_mgr->plugin_id_inc++;
            pthread_create(&g_mgr->plugin_thread_id, NULL, plugin_run, g_mgr);
            ret = 0;
        }
    }

    return ret;
}

int plugin_server_finish(void)
{
    int ret = -1;

    if (plugin_mgr_check(g_mgr)) {
        if (g_mgr == NULL) {
            pthread_cancel(g_mgr->plugin_thread_id);
            ret = 0;
        }
        plugin_mgr_des(&g_mgr);
    }

    return ret;
}

//int initialize(void)
//{
//    /*
//    bool status = false;*/
//    int fd;
//
//    if (access(COMUNICATION_CMD, F_OK) < 0)
//        mkfifo(COMUNICATION_CMD, 0777);
///*
//    if (access(COMUNICATION_CMD, F_OK) < 0)
//        mkfifo(COMUNICATION_CMD, 0x777);
//    if ((fd = open(COMUNICATION_CMD, O_RDONLY | O_EXCL | O_CREAT, 0766)) <0) {
//        if (errno == EEXIST) {
//            if ((fd = open(COMUNICATION_CMD, O_RDONLY, 0766)) < 0){
//                perror("open "COMUNICATION_CMD);
//            } else {
//                status = true;
//            }
//        }
//    }else {
//        status = true;
//    }*/
//    fd = open(COMUNICATION_CMD, O_RDWR);
//    return fd;
//}
//
//void destory(int fd)
//{
//    if (fd > 0)
//        close(fd);
//
//    return;
//}
//
//void update_config(int *pfd)
//{
//    int nfd;
//    int fd = *pfd;
//    fd_set rfds;
//    int retval;
//    struct timeval timeout;
//    plugin_cmd_t cmd_data;
//    uint16_t rl_len=0, len=0;/*
//    uint16_t rd_len=0, rl_len=0, len=0;*/
//    char *p;
//    /*
//    if (*fd < 0) {
//        if ((*fd = initialize()) < 0)
//            return;
//    }
//    */
//    nfd = fd + 1;
//    FD_ZERO(&rfds);
//    FD_SET(fd, &rfds);
//
//    timeout.tv_sec = 2;
//    timeout.tv_usec = 0;
//
//    retval = select(nfd, &rfds, NULL, NULL, &timeout);
//    if (retval == -1) {
//        perror("select "COMUNICATION_CMD);
//    } else if (retval>0) {
//        /* printf("select TRUE\n"); */
//        if (FD_ISSET(fd, &rfds)) {
//            /* read head of message*/
///*            rd_len = 0;*/
//            len = 4;
//            p = (char*)&cmd_data;
//            #if 0
//            while (len > 0) {
//                /*rl_len = read(fd, ((char*)(&cmd_data))+rd_len, len);*/
//                rl_len = read(fd, p+rd_len, len);
//                if (rl_len <= 0)
//                    if (errno == EINTR || errno == EAGAIN)
//                        break;
//                    else
//                        continue;
//                rd_len += rl_len;
//                len -= rl_len;
//            }
//            #endif
//            rl_len = read(fd, p, len);
//            if (rl_len <= 0) {
//                if (errno == EINTR || errno == EAGAIN)
//                    printf("EINTR EAGAIN ");
//                else {
//                    perror("select ");
//                }
//            }
//            /* procotol check */
//            if (cmd_data.flag != 0x6b || (cmd_data.type & 0xF0) != 0x90)
//                return;
//
//            /* read body of message */
//            len = cmd_data.size;
//            #if 0
//            while (len > 0) {
//                /*rl_len = read(fd, (char*)(&cmd_data)+rd_len, len);*/
//                rl_len = read(fd, p+rd_len, len);
//                if (rl_len <= 0)
//                    if (!(errno == EINTR || errno == EAGAIN))
//                        break;
//                    else
//                        continue;
//                rd_len += rl_len;
//                len -= rl_len;
//            }
//            #endif
//            read(fd, p+4, len);
//
//            /* plugin parser and register or unregister op. */
//            plugin_parser(g_mgr, &cmd_data);
//        }
//
//    } else {
//            ;/*printf("select timeout.\n");*/
//    }
//
//    return ;
//}
//
/*int main(int argc, char* argv[]) */
static void* plugin_run(void* args)
{
    int fd;
    int nfd;
    fd_set rfds;
    int retval;
    plugin_cmd_t cmd_data;
    uint16_t len=0;
    char *p;

    if (access(COMUNICATION_CMD, F_OK) < 0)
        mkfifo(COMUNICATION_CMD, 0777);
    fd = open(COMUNICATION_CMD, O_RDWR);

    pthread_detach(pthread_self());

    while (true)
    {
        nfd = fd + 1;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        retval = select(nfd, &rfds, NULL, NULL, NULL);
        if (retval == -1) {
            perror("select "COMUNICATION_CMD);
        } else if (retval>0) {
            /* printf("select TRUE\n"); */
            if (FD_ISSET(fd, &rfds)) {
                /* read head of message*/
                len = 4;
                p = (char*)&cmd_data;
                read(fd, p, len);

                /* procotol check */
                if (cmd_data.flag != 0x6b || (cmd_data.type & 0xF0) != 0x90)
                    continue;

                /* read body of message */
                len = sizeof(plugin_cmd_t) - 4;
                read(fd, p+4, len);

                /* plugin parser and register or unregister op. */
                plugin_parser(g_mgr, &cmd_data);
            }

        } else {
                ;/*printf("select timeout.\n");*/
        }
    }

    return NULL;
}
