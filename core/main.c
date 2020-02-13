#include <stdio.h>
#include <unistd.h>
#include <metric.h>
#include <signal.h>
#include <sys/types.h>
#include <signal.h>
#include <cleanup.h>

void exit_action(int signo) {
    printf("recv sig %d\n", signo);
    delete_all_metric();
    list_metric();
    exit(0);
}

void signal_register(void) {
    signal(SIGINT, exit_action);
    signal(SIGQUIT, exit_action);
    signal(SIGKILL, exit_action);
    signal(SIGTERM, exit_action);
}

int
main() {
    printf("syswatcher core init\n");
    signal_register();
    metrics_head = create_metrics_chain(); //from config file
    traversal_metric_units();
}
