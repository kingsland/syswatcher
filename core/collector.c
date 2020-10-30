#include <collector.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <log.h>
#include <unistd.h>

void reuse_socket(int fd)
{
    int reuse = 1;
    struct linger lg = {1, 0};
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(int));
    setsockopt(fd, SOL_SOCKET, SO_LINGER, &lg, sizeof(struct linger));
}

int init_srv(int port)
{
    int listenfd;
    struct sockaddr_in srvaddr;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        logging(LEVEL_ERR, "open socket failed\n");
        exit(-1);
        //FIXME
    }
    reuse_socket(listenfd);
    bzero(&srvaddr, sizeof(srvaddr));
    srvaddr.sin_family = AF_INET;
    srvaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    srvaddr.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr *)&srvaddr, sizeof(srvaddr)) < 0) {
        logging(LEVEL_ERR, "bind failed tw? check it.\n");
        exit(-1);
        //FIXME
    }
    listen(listenfd, 5);
    return listenfd;
}

//FIXME
void *resp_thread(void *arg)
{
    int listenfd;
    int cli_fd;
    struct sockaddr_in cliaddr;
    struct data_collector *collector = (struct data_collector *)arg;
    socklen_t len = sizeof(struct sockaddr_in);
    listenfd = init_srv(3200);
    while(1) {
        //FIXME
        cli_fd = accept(listenfd, (struct sockaddr *)&cliaddr, &len);
        write(cli_fd, collector->visual_data, strlen(collector->visual_data));
	usleep(10);
        close(cli_fd);
    }
}

void *json_resp_thread(void *arg)
{
    int listenfd;
    int cli_fd;
    struct sockaddr_in cliaddr;
    struct data_collector *collector = (struct data_collector *)arg;
    socklen_t len = sizeof(struct sockaddr_in);
    listenfd = init_srv(3201);
    while(1) {
        //FIXME
        cli_fd = accept(listenfd, (struct sockaddr *)&cliaddr, &len);
        write(cli_fd, collector->json_data, strlen(collector->json_data));
	usleep(10);
        close(cli_fd);
    }
}



int _start_collector(struct data_collector *collector)
{
    pthread_create(&collector->collect_ti, NULL, collector->do_collect, NULL);
    pthread_create(&collector->resp_ti, NULL, resp_thread, collector);
    pthread_create(&collector->resp_json_ti, NULL, json_resp_thread, collector);
    return 0;
}

int _stop_collector(struct data_collector *collector)
{
    return 0;
}

void init_collector(struct data_collector *collector, void * (*handler)(void *arg))
{
    collector->do_collect = handler;
    collector->start_collector = _start_collector;
    collector->stop_collector = _stop_collector;
    pthread_rwlock_init(&(collector->data_lock), NULL);
    return;
}

void exit_collector(struct data_collector *collector)
{
    pthread_rwlock_destroy(&(collector->data_lock));
    return;
}
