#include <IPMIConf.h>
#include <pthread.h>
#include <MsgHndlr.h>
#include <SysMonitor.h>
#include <IPMI.h>
#include <defs.h>
#include <libagent_server.h>
#include <signal.h>
#include <stddef.h>

BMCInfo_t g_BMCInfo[MAX_NUM_BMC];
CoreFeatures_T g_corefeatures;
CoreMacros_T g_coremacros;
INT32U IPMITimeout = 0;
sem_t sem_QUEUE;
IPMIQueue_T g_IPMIIfcQueue[MAX_IPMI_IFCQ];
int BMCInst = 0;
#define SEND_SYSINFO_INTERVAL   (5)

void *IpmiTool(void *arg)
{
    int idx;
    char cmd[1024];
    while(1)
    {
        strcpy (cmd, "ipmitool -U admin -P admin raw 0x2e 0x50");
        pthread_mutex_lock(&(nodeInfoMtx));
        for (idx = 0; idx < sizeof(gNodeAgentInfo); idx++) {
            sprintf(cmd, "%s 0x%02x", cmd, *(((uint8_t *)&gNodeAgentInfo) + idx));
        }
        pthread_mutex_unlock(&(nodeInfoMtx));
        system(cmd);
        sleep(SEND_SYSINFO_INTERVAL);
    }
}

void action(int signum) {
    if (signum == SIGQUIT) {
        exit(0);
    }
}
void main(int argc, char **argv)
{
    signal(SIGQUIT, action);
    pthread_t msghndlr_pid;
    pthread_t lanifctask_pid;
    pthread_t sysmonitor_pid;
    pthread_t ipmitool_pid;
    pthread_t socket_pid;
    int libagent_srv_fd;

    libagent_srv_fd = init_srv(PORT);
    BMCInit (0);
    if(sem_init(&sem_QUEUE, 0, 1) ==-1){
        IPMI_ERROR("A lock for IPMI QUEUE is not created properly!!!\n");
        return;
    }

    pthread_create(&sysmonitor_pid, NULL, SysMonitor, NULL);
    pthread_create(&msghndlr_pid, NULL, MsgHndlr, &BMCInst);
    pthread_create(&lanifctask_pid, NULL, LANIfcTask, &BMCInst);
    //pthread_create(&ipmitool_pid, NULL, IpmiTool, NULL);
    //pthread_create(&socket_pid, NULL, AgentServer, &libagent_srv_fd);

    while(1){sleep(1);}
}

