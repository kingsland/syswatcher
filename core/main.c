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
    printf("recv sig %d\n", signo);
    delete_all_metric();
    list_metric();
    plugin_server_finish();
    exit_logger(&log_unit);
    exit(0);
}

void signal_register(void) {
    signal(SIGINT, exit_action);
    signal(SIGQUIT, exit_action);
    signal(SIGKILL, exit_action);
    signal(SIGTERM, exit_action);
}

/*****************test delete this late*************/
int collect_cpu_freq(item_t *item) {
    printf("%s %d\n", __func__, __LINE__);
    return 0;
}

int collect_cpu_usage(item_t *item) {
    printf("%s %d\n", __func__, __LINE__);
    return 0;
}

plugin_channel_t plugin = {
    .plugin_id = 1000,
    .name = "cpu misc",
    .desc = "cpu misc desc",
    .sub_metric_num = 2,
};
/*************end of test**************************/

int
main() {
    init_logger(&log_unit, LEVEL_INFO);
    int count = 100;
    while(count--) {
        print_log(LEVEL_INFO, "%s %d\n", __func__, __LINE__);
    }
    printf("syswatcher core init\n");
    signal_register();
    init_syswatcher(&watcher);

    plugin_server_start(watcher.add_metric, watcher.del_metric, &watcher);

#if 0
    /*****************test delete this late*************/
    plugin_sub_channel_t *sub_channel = (plugin_sub_channel_t *)malloc(sizeof(plugin_sub_channel_t) * 2);
    strcpy(sub_channel->subname, "cpu freq");
    strcpy(sub_channel->subdesc, "cpu freq desc");
    sub_channel->run_once = true;
    sub_channel->interval = TRAVERSAL_INTERVAL;
    sub_channel->collect_data_func = collect_cpu_freq;

    strcpy((sub_channel + 1)->subname, "cpu usage");
    strcpy((sub_channel + 1)->subdesc, "cpu usage desc");
    (sub_channel + 1)->run_once = false;
    (sub_channel + 1)->interval = TRAVERSAL_INTERVAL;
    (sub_channel + 1)->collect_data_func = collect_cpu_usage;
    plugin.sub_channel = sub_channel;
    watcher.add_metric((void *)(&watcher), &plugin);
    /*************end of test**************************/
#endif
    watcher.traversal_metric_units();

    return 0;
}
