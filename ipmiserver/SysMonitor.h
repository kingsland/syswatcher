#ifndef SYSMONITOR_H
#define SYSMONITOR_H
#include <IPMI.h>
#include <net/if.h>
#include <pthread.h>
struct CpuMap {
    INT32U   CpuType;
    INT8U   CpuName[32];
    //can't be CpuName[64], or this will cause a segment fault I don't know why, it's weird.
};

typedef struct net_map {
    char name[IFNAMSIZ];
    uint32_t speedmode; //1000000k 40000000k
    uint32_t speed; //unit kb/sec
} net_map;

struct net_map netdevs[8];
void *SysMonitor(void *param);
pthread_mutex_t nodeInfoMtx;
NodeAgentInfo_T gNodeAgentInfo, gIpmiserverNodeInfo;
#endif
