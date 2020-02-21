#include <stdio.h>
#include <unistd.h>
#include <metric.h>
#include <signal.h>
#include <sys/types.h>
#include <signal.h>
#include <cleanup.h>
#include <string.h>
#include <plugin_server.h>
#include <log.h>

void exit_action(int signo) {
    logging(LEVEL_ZERO, "AT EXIT\n");
    logging(LEVEL_ZERO, "delete all metric\n");
    delete_all_metric();
    logging(LEVEL_ZERO, "unload plugin server\n");
    plugin_server_finish();
    logging(LEVEL_ZERO, "DONE\n");
    exit_logger(&log_unit);
    exit(0);
}

void signal_register(void) {
    signal(SIGINT, exit_action);
    signal(SIGQUIT, exit_action);
    signal(SIGKILL, exit_action);
    signal(SIGTERM, exit_action);
}

void print_banner(void)
{
    logging(LEVEL_ZERO, "[           syswatcher          ]\n");
    logging(LEVEL_ZERO, "=================================\n");
    logging(LEVEL_ZERO, "name:\n");
    logging(LEVEL_ZERO, "version:\n");
    logging(LEVEL_ZERO, "compile time:\n");
    logging(LEVEL_ZERO, "=================================\n");
}

int
main() {
    init_logger(&log_unit, LEVEL_INFO);
    print_banner();
    signal_register();
    init_syswatcher(&watcher);

    plugin_server_start(watcher.add_metric, watcher.del_metric, &watcher);

    watcher.traversal_metric_units();

    return 0;
}
