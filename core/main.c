#include <stdio.h>
#include <execinfo.h>
#include <unistd.h>
#include <metric.h>
#include <signal.h>
#include <sys/types.h>
#include <signal.h>
#include <cleanup.h>
#include <string.h>
#include <plugin_server.h>
#include <log.h>
#include <fifo.h>
#include <vers.h>
void exit_action(int signo) {
    cleanup();
    exit(0);
}

void
print_trace(void)
{
    int j, nptrs;
#define SIZE 100
    void *buffer[100];
    char **strings;

    nptrs = backtrace(buffer, SIZE);
    logging(LEVEL_ZERO, "backtrace() returned %d addresses\n", nptrs);

    /* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
       would produce similar output to the following: */

    strings = backtrace_symbols(buffer, nptrs);
    if (strings == NULL) {
        perror("backtrace_symbols");
        exit(EXIT_FAILURE);
    }

    for (j = 0; j < nptrs; j++)
        logging(LEVEL_ZERO, "%s\n", strings[j]);

    free(strings);
}

void segv_handler(int signo) {
    print_trace();
    cleanup();
    exit(0);
}

void signal_register(void) {
    signal(SIGINT, exit_action);
    signal(SIGQUIT, exit_action);
    signal(SIGKILL, exit_action);
    signal(SIGTERM, exit_action);
    signal(SIGSEGV, segv_handler);
}

void print_banner(void)
{
    logging(LEVEL_ZERO, "[           syswatcher          ]\n");
    logging(LEVEL_ZERO, "=================================\n");
    logging(LEVEL_ZERO, "version: %s\n", vers_info);
    logging(LEVEL_ZERO, "compile time: %s\n", time_info);
    logging(LEVEL_ZERO, "=================================\n");
}

struct fifo_head *fifo;
void *push_thread(void *arg)
{
    int count = 0;
    while(1) {
        INIT_FIFO_NODE(fnode);
        sprintf(fnode->priv, "count:%d", count++);
        fifo->push(fifo, fnode);
    }
}

void *pop_thread(void *arg)
{
    struct fifo_node *node;
    while(1) {
        node = fifo->pop(fifo);
        if (node != NULL) {
            DESTROY_FIFO_NODE(node);
        }
    }
}



int
main() {
    if (init_logger(&log_unit, LEVEL_INFO) != 0) {
        return 1;
    }
    print_banner();
    signal_register();
    init_syswatcher(&watcher);
    plugin_server_start(watcher.add_metric, watcher.del_metric, &watcher);
    watcher.collector.start_collector(&(watcher.collector));
    watcher.traversal_metric_units((void *)&watcher);
    return 0;
}
