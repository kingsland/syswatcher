#include <Types.h>
#include <metrics.h>
#include <defs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <SysMonitor.h>

#define FILELEN     (256)
#define MAXLEN      (256)
struct CpuMap cpuMaps[] = {
    {0x1,   "phytium FT2000a"},
    {0xde,   "phytium FT1500a"},
    {0xde,  "FT1500A"},
};

int GetBladeIndex(NodeAgentInfo_T *nodeAgentInfo) {
    char buf[512];
    FILE *stream;
    char *p;
    char compstring[]="Hardware Address : 0x";
    int ret;

    stream = popen("ipmitool -U admin -P admin picmg addrinfo","r");
    fread(buf, sizeof(char), sizeof(buf), stream);
    p = strstr(buf, compstring);
    p += strlen(compstring);
    sscanf(p, "%x", &ret);
    //printf("%s%x\n", compstring, (UINT8)ret);
    fclose(stream);

    nodeAgentInfo->BladeIndex = (UINT8)ret;
    return 0;
}

int GetBIOSVersion(NodeAgentInfo_T *nodeAgentInfo) {
#if defined(ICC_PLATFORM_ARM)
    UINT8 versH, versL;
    versH = 0x3;
    versL = 0x0;
    nodeAgentInfo->BIOSVersion = versH<<4 | versL;
#else
    UINT8 versH, versL;
    versH = 0x3;
    versL = 0x0;
    nodeAgentInfo->BIOSVersion = versH<<4 | versL;
#endif
    return 0;
}

static int GetSPVersion(NodeAgentInfo_T *nodeAgentInfo) {
#if defined(ICC_PLATFORM_ARM)
#define LSB_FILE    "/etc/lsb-release"
#define SPVERS_STR  "DISTRIB_KYLIN_RELEASE="
    UINT8 spVersionH, spVersionL;
    int fd;
    char content[FILELEN];
    char *strptr, *changeline;
    struct stat buf;
    if (stat(LSB_FILE, &buf) < 0) {
        spVersionH = 0xf;
        spVersionL = 0xf;
    } else {
        fd = open(LSB_FILE, O_RDONLY);
        read(fd, content, FILELEN);
        strptr = strstr(content, "DISTRIB_KYLIN_RELEASE=");
        if (strstr == NULL) {
            spVersionH = 0xf;
            spVersionL = 0xf;
        } else {
            strptr = strstr(strptr, "=");
            strptr = strstr(strptr, "SP");
            changeline = strstr(strptr, "\n");
            *changeline = '\0';
            printf("%s\n", strptr);
            strptr += 2;
            spVersionH = 0;
            sscanf(strptr, "%hhu", &spVersionL);
        }
        nodeAgentInfo->ServicePackVersion = (spVersionH<<4) | (spVersionL);
        close(fd);
    }
#else
#define LSB_FILE    "/etc/lsb-release"
#define SPVERS_STR  "DISTRIB_KYLIN_RELEASE="
    UINT8 spVersionH, spVersionL;
    int fd;
    char content[FILELEN];
    char *strptr, *changeline;
    struct stat buf;
    if (stat(LSB_FILE, &buf) < 0) {
        spVersionH = 0xf;
        spVersionL = 0xf;
    } else {
        fd = open(LSB_FILE, O_RDONLY);
        read(fd, content, FILELEN);
        strptr = strstr(content, "DISTRIB_KYLIN_RELEASE=");
        if (strstr == NULL) {
            spVersionH = 0xf;
            spVersionL = 0xf;
        } else {
            strptr = strstr(strptr, "=");
            strptr = strstr(strptr, "SP");
            changeline = strstr(strptr, "\n");
            *changeline = '\0';
            printf("%s\n", strptr);
            strptr += 2;
            spVersionH = 0;
            sscanf(strptr, "%hhu", &spVersionL);
        }
        nodeAgentInfo->ServicePackVersion = (spVersionH<<4) | (spVersionL);
        close(fd);
    }

#endif
    return 0;
}

static int GetKernelVersion(NodeAgentInfo_T *nodeAgentInfo)
{
    FILE *fd;
    char buff[FILELEN], tmp[FILELEN], *p;
    unsigned char major, minor;

    fd = fopen("/proc/version", "r");
    fgets(buff, sizeof(buff), fd);
    fclose(fd);

    sscanf(buff, "%s %s %s %s", tmp,tmp,buff,tmp);
    p = strstr(buff, ".el");
    if(p) *p = '\0';
    sscanf(buff, "%hhu.%hhu.%s", &major, &minor, tmp);
    nodeAgentInfo->KernelVersion = (major << 4)|(minor & 0x0F);
    print_info("kernel version: %x\n", nodeAgentInfo->KernelVersion);

    return 0;
}

static int GetCPUModel(NodeAgentInfo_T *nodeAgentInfo)
{
#if defined (ICC_PLATFORM_X86)
    FILE *fd;
    char buff[FILELEN];
    fd = fopen("/sys/devices/cpu/type", "r");
    fgets(buff, sizeof(buff), fd);
    fclose(fd);

    sscanf(buff, "%d", &(nodeAgentInfo->CPUModel));
#elif defined (ICC_PLATFORM_ARM)
    int fd;
    char buff[FILELEN], cpuname[64], tmpchar[32];
    char *strptr, *changeline;
    INT8U cpumodel = 0xff;
    int idx;
    fd = open("/proc/cpuinfo", O_RDONLY);
    read(fd, buff, FILELEN);
    close(fd);
    strptr = strstr(buff, "model name");
    changeline = strstr(strptr, "\n");
    *changeline = '\0';
    strptr = strstr(strptr, ": ");
    strptr += 2;
    printf("%s\n", strptr);
    for (idx = 0; idx < sizeof(cpuMaps)/sizeof(struct CpuMap); idx++) {
        if (strcmp(cpuMaps[idx].CpuName, strptr) == 0) {
            cpumodel = cpuMaps[idx].CpuType;
        }
    }
    nodeAgentInfo->CPUModel = cpumodel;

#endif
    return 0;
}

static int GetRamSpeed(NodeAgentInfo_T *nodeAgentInfo)
{
#if defined (ICC_PLATFORM_X86)
    int fd, readnum;
    char *p, buff[MAXLEN], tmpbuff[MAXLEN], hz[20];

    if(system("dmidecode -t 17|grep \"Configured Clock Speed\"|grep MHz > /tmp/dmiinfo") < 0)
        return -1;
    fd = open("/tmp/dmiinfo", O_RDONLY);
    if(!fd) printf("please check dmidecode command\n");
    else{
        readnum = read(fd, buff, MAXLEN);
        close(fd);
        buff[readnum] = '\0';
        if(p = strstr(buff,"MHz")){
            sscanf(p, "%s %s %s %s %s",tmpbuff,tmpbuff,tmpbuff,hz,tmpbuff);
            nodeAgentInfo->RamSpeed = atoi(hz);
        }
    }
#elif defined (ICC_PLATFORM_ARM)
    nodeAgentInfo->RamSpeed = 1000;
    printf("RamSpeed: %d\n", nodeAgentInfo->RamSpeed);
#endif
    return 0;
}

static int GetRamSize(NodeAgentInfo_T *nodeAgentInfo)
{
    FILE *fd;
    char *p, tmp[20], buff[FILELEN];
    INT32U memsize;
    fd = fopen("/proc/meminfo", "r");
    while (fgets (buff, sizeof (buff), fd)){
        if(!strncmp(buff, "MemTotal:", 9)){
            p = strstr(buff, "MemTotal:");
            sscanf(p, "%[^ ]%u", tmp, &memsize);
        }
    }
    memsize /= 1024;
    nodeAgentInfo->RamSize = memsize;
    fclose(fd);
    print_info("RamSize: %uM\n", nodeAgentInfo->RamSize);
    return 0;
}

static int GetSSDInfo(NodeAgentInfo_T *nodeAgentInfo)
{
    double total_free = 0.0;
    double total_size = 0.0;
    find_disk_space(&total_size, &total_free);
    nodeAgentInfo->SSDSize = (INT8U)total_size;
    nodeAgentInfo->UsedSSDSize = (INT8U)(total_size - total_free);
    return 0;
}

int GetCPURate(NodeAgentInfo_T *nodeAgentInfo)
{
    INT8U cpu_usage = 100 - cpu_idle_func();
    nodeAgentInfo->CPURate = cpu_usage;
    print_info("cpu usage: %u\n", cpu_usage);
    return 0;
}

static int GetRamRate(NodeAgentInfo_T *nodeAgentInfo)
{
    FILE *fd;
    char *p, tmp[20], buff[FILELEN];
    INT32U memsize, memavailable;
    INT32U memRate;
    fd = fopen("/proc/meminfo", "r");
    while (fgets (buff, sizeof (buff), fd)){
        if(!strncmp(buff, "MemTotal:", 9)){
            p = strstr(buff, "MemTotal:");
            sscanf(p, "%[^ ]%u", tmp, &memsize);
        }
        if(!strncmp(buff, "MemAvailable:", 13)){
            p = strstr(buff, "MemAvailable:");
            sscanf(p, "%[^ ]%u", tmp, &memavailable);
        }

    }
    fclose(fd);
    memRate = ((memsize - memavailable)*100)/memsize;
    print_info("%u/%u RamRate: %d\n", memavailable, memsize, memRate);
    nodeAgentInfo->MemRate = memRate;
    return 0;
}

#define set_rate(idx)   \
    {   \
        rate[idx] = (netdevs[idx].speed * 800)/netdevs[idx].speedmode;  \
        print_info("%s %u\n", netdevs[idx].name, netdevs[idx].speed);    \
    }while(0)

static int GetEthRate(NodeAgentInfo_T *nodeAgentInfo)
{
    INT8U rate[32];
    int idx;
    update_speed();
    for (idx = 0; idx < 7; idx++) {
        set_rate(idx);
    }
    nodeAgentInfo->Eth0Rate = rate[0];
    nodeAgentInfo->Eth1Rate = rate[1];
    nodeAgentInfo->Eth2Rate = rate[2];
    nodeAgentInfo->Eth3Rate = rate[3];
    nodeAgentInfo->Eth4Rate = rate[4];
    nodeAgentInfo->Eth5Rate = rate[5];
    nodeAgentInfo->Eth6Rate = rate[6];
    nodeAgentInfo->Eth7Rate = rate[7];
    return 0;
}

#define set_netdev(netdev, _name, _speedmode) \
    {strcpy((netdev)->name, _name); \
    (netdev)->speedmode = _speedmode;}


void getnetname()
{
    char str[100];
    int count=0;
    int idx = 0;
#if defined (ICC_PLATFORM_ARM)
    FILE *fp = popen("/usr/local/agent/getnetname.sh", "r");
#else
    FILE *fp = popen("/usr/local/agent/getnetname.sh", "r");
#endif
    for (idx = 0; idx < sizeof(netdevs)/sizeof(struct net_map); idx++) {
        netdevs[idx].speedmode = 1024*1024;
    }
    while(fscanf(fp, "%s", str)!=EOF)
    {
        set_netdev(&netdevs[count], str, 1024*1024);
        count++;
    }
    netdevs[0].speedmode = 10*1024*1024;
    netdevs[1].speedmode = 10*1024*1024;
    netdevs[2].speedmode = 10*1024*1024;
    netdevs[3].speedmode = 10*1024*1024;

    fclose(fp);
}

void *SysMonitor(void *param)
{
    memset(&gNodeAgentInfo, 0, sizeof(gNodeAgentInfo));
    //getnetname();
    pthread_mutex_init(&(nodeInfoMtx), NULL);
    do {
        sleep(2);
        pthread_mutex_lock(&(nodeInfoMtx));
     /*   GetBladeIndex(&gNodeAgentInfo);
        GetBIOSVersion(&gNodeAgentInfo);
        GetKernelVersion(&gNodeAgentInfo);
        GetSPVersion(&gNodeAgentInfo);
        GetCPUModel(&gNodeAgentInfo);
        GetRamSpeed(&gNodeAgentInfo);
        GetRamSize(&gNodeAgentInfo);
        GetSSDInfo(&gNodeAgentInfo);
        GetCPURate(&gNodeAgentInfo);
        GetRamRate(&gNodeAgentInfo);
        GetEthRate(&gNodeAgentInfo);
      */
        gNodeAgentInfo.BIOSVersion ++;
        pthread_mutex_unlock(&(nodeInfoMtx));
    } while(1);
}
