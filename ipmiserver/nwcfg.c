/*****************************************************************
 ******************************************************************
 ***                                                            ***
 ***        (C)Copyright 2008, American Megatrends Inc.         ***
 ***                                                            ***
 ***                    All Rights Reserved                     ***
 ***                                                            ***
 ***       5555 Oakbrook Parkway, Norcross, GA 30093, USA       ***
 ***                                                            ***
 ***                     Phone 770.246.8600                     ***
 ***                                                            ***
 ******************************************************************
 ******************************************************************
 ******************************************************************
 *
 * Filename: nwcfg.c
 *
 * Description: Contains code for the basic network library APIs.
 *
 * Author: Anurag Bhatia
 *
 * Revision History:
 * 04/29/2005
 *    defines are added. Code cleanup done.
 *
 * 04/17/2006
 *    Rewritten ReadNWCfg code. All the functions to
 *    retrieve the network configuration have been
 *    changed to use IOCTLS and no process is being
 *    forked from this library to retrieve network
 *    configuration information anymore.
 *
 *06/09/2010 
 *    Author : Arunkumar S
 *    DNS Architecture related modifications done.
 *    API's added for Multiple interface related DNS support.
 *    Comments added for functions.
 *
 *06/10/2010
 *    Author : Arunkumar S
 *    LAN Enable/Disable feature support added in libnetwork layer.
 *     	
 *08/03/2010
 *    Authors : Joey Chen, Arunkumar S
 *    Register BMC Enable support added in core.
 *
 *01/25/2011
 *    Author : Muthuchamy.K
 *    Removed hard coded Interface Name.
 *    Added GetIfcNameByIndex function to get the Interface Name.
 *    Hard coded interface name is replaced with GetIfcNameByIndex
 *    function call.
 *02/03/2011
 *    Author : Muthuchamy.k
 *    Bonding Support added in libnetwork layer.    
 ******************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <mcheck.h>
#include "unix.h"
#include "dbgout.h"
#include "nwcfg.h"
#include "exterrno.h"
#include <sys/ioctl.h>
#include <linux/route.h>
#include "coreTypes.h"
//#include "validate.h"
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/ether.h>
#include <netpacket/packet.h>
#include "ncml.h"
//#include "lanchcfg.h" //moved to nwcfg.h
/*ipv6*/
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <linux/mii.h>

/*dns*/
#include <ctype.h>
//#include "hostname.h"

#include "iniparser.h"
#include "featuredef.h"
#define IP_LINKLOCAL    0x01
#define IP_SITELOCAL    0x02
#define IP_GLOBAL       0x04
#define MAX_ETH   5
#define VLAN_IFC_MTU_SIZE 1496
#define READ_CFG    0
#define WRITE_CFG   1
#define REG_BMC_FQDN 0x10
#define UNREG_BMC_FQDN 0x00

/* dhcpv6 */
#define DHCP6CCONF "/conf/dhcp6c.conf"
#define NEW_DHCP6CCONF "/conf/new_dhcp6c.conf"
#define ACTIVESLAVE "/conf/activeslave.conf"

#define MAX_TEMP_ARRAY_SIZE 64

INT8S   Dhcp_PID_File[MAX_STR_LENGTH]= "/var/run/udhcpc.";
INT8S   Dhcp6_PID_File[MAX_STR_LENGTH]= "/var/run/dhcp6c.";
INT8S   Eth_NetWork_If[MAX_STR_LENGTH]= "eth";
INT8S   IfUp_Ifc[MAX_STR_LENGTH]=" /sbin/ifup ";
INT8S   IfDown_Ifc[MAX_STR_LENGTH]= "/sbin/ifdown ";
INT8S   IfEnslave[MAX_STR_LENGTH]="/sbin/ifenslave";
INT8S PID[MAX_STR_LENGTH]=".pid";

SERVICE_CONF_STRUCT g_serviceconf;

char *ModifyServiceNameList[MAX_SERVICE]={ 
    WEB_SERVICE_NAME,    
    KVM_SERVICE_NAME,    
    CDMEDIA_SERVICE_NAME,
    FDMEDIA_SERVICE_NAME,
    HDMEDIA_SERVICE_NAME,
};

char *RestartServices[MAX_RESTART_SERVICE] = {
    "/etc/init.d/adviserd.sh restart &",
    "/etc/init.d/cdserver restart &",
    "/etc/init.d/fdserver restart &",
    "/etc/init.d/hdserver restart &",
};

IfcName_T Ifcnametable [MAX_CHANNEL] = 
{
    //        {"eno16777736", 0, 0},   /*test*/
    {{0}, 0, 0},   /*test*/
    {{0}, 0, 0},
    {{0}, 0, 0},
    {{0}, 0, 0},
};



static  NWCFG_STRUCT m_NwCfgInfo[MAX_ETH];

static  NWCFG6_STRUCT m_NwCfgInfo_v6[MAX_ETH];

static  BondConf     m_NwBondInfo[MAX_BOND];

static  ActiveConf      m_NwActiveslave[MAX_BOND];
static  int m_NoofInterface;

INT8S m_VLANInterface [MAX_STR_LENGTH];
INT8S m_LANIndex=1;

int CheckInterfacePresence (char * Ifc);
int registerBMCstatus (INT8U *registerBMC);
int UpdateBondConf(INT8U Enable,INT8U BondIndex,INT8U BondMode,INT16U MiiInterval);
int UpdateBondInterface(INT8U Enable,INT8U BondIndex,INT8U Slaves,INT8U Ethindex);
int UpdateActiveSlave(INT8U Flag);
int nwGetBondCfg();

int InitIfcNameTable()
{
    int NIC_Count = 2;
    //    NIC_Count = GetMacrodefine_getint("CONFIG_SPX_FEATURE_GLOBAL_NIC_COUNT", 0);

    if(NIC_Count == 0x04)
    {
        IfcName_T Ifctable [MAX_CHANNEL] = 
        {
            {"eth0", 0, 0},    
            {"eth1", 1, 0},
            {"eth2", 2, 0},
            {"eth3", 3, 0},     
        };
        memcpy(Ifcnametable,Ifctable,sizeof(Ifcnametable));
    }
    else if(NIC_Count == 0x03)
    {
        IfcName_T Ifctable [MAX_CHANNEL] = 
        {
            {"eth0", 0, 0},    
            {"eth1", 1, 0},
            {"eth2", 2, 0},         
            {"bond0", 3, 0}, 
        };
        memcpy(Ifcnametable,Ifctable,sizeof(Ifcnametable));
    }
    else if(NIC_Count == 0x02)
    {
        IfcName_T Ifctable [MAX_CHANNEL] = 
        {
            {"eth0", 0, 0},    
            {"eth1", 1, 0},
            {"bond0", 2, 0},       
            {{0}, 0, 0},
        };
        memcpy(Ifcnametable,Ifctable,sizeof(Ifcnametable));
    }

    return 0;
}

/* The following function is used by UpdateInterface fucntion to convert
   the IP address to a string to save it in /etc/network/interfaces */
void ConvertIPnumToStr(unsigned char *var, unsigned int len,unsigned char *string)
{
    struct in_addr* ipadd;
    INT32U param = var [0] << 24 | var [1] << 16 | var [2] << 8 | var [3];
    param = htonl(param);

    ipadd = (struct in_addr*)(&param);
    sprintf((char*)string, "%d.%d.%d.%d", var[0],var[1],var[2],var[3]);
}

/************************************************************************
function : ConvertIP6numToStr
brief : converts the ipv6 address number to string format.
param : variable that contains numerical value of the address.
param : length of the string to be converted. usually INET6_ADDRSTRLEN
param : string that stores ipv6 address
 ************************************************************************/
void ConvertIP6numToStr(unsigned char *var, unsigned int len,unsigned char *string)
{
    if (NULL == inet_ntop (AF_INET6, (char*)var, (char*)string, len))
    {
        TCRIT("%d: %s\n",errno,strerror(errno));
    }
}

/*
 * @fn ConvertNETMaskArrayTonum
 * @brief This function is used to calculate the IPV6 prefix length 
 * @params var[in] Netmask IPv6 address
 * @params len [in] IPv6 Address length
 * @returns the decimal notation of IPv6 prefix length
 */
int ConvertNETMaskArrayTonum( INT8U*var, unsigned int len)
{
    int prefixLen = 0;
    while ((var[prefixLen] == 0xff) && (prefixLen < len))
        prefixLen++;

    if(prefixLen == IP6_ADDR_LEN)
        return prefixLen*8;

    unsigned char prefixTrailer = var[prefixLen];

    prefixLen*=8; // Compute bit count

    // Look at the next octet for partial mask fill
    // and recompute prefixLen
    int itr = 7;
    while (itr > 0)
    {
        if ((prefixTrailer >> itr) & 0x01)
            prefixLen++;
        itr--;
    }
    return prefixLen;
}

/************************************************************************
function : nwMakeIFUp
brief : brings up the interface.
param : EthIndex : Index value of LAN channel.
 ************************************************************************/
int nwMakeIFUp(INT8U EthIndex)
{
    int retval = 0;
    INT8S  FullIfUp_Eth[MAX_STR_LENGTH];

    /*Check the Interface Enabled status*/
    if(Ifcnametable[EthIndex].Enabled != 1)
        return 0;

    /* check for vlan interface. if present then do ifup for vlan. else do ifup for lan interface */

    if( m_NwCfgInfo[EthIndex].VLANID )    //VLAN UP
    {
        TDBG("In vlan mode");
        char cmd[40]=VLAN_NETWORK_CONFIG_FILE;
        sprintf(cmd,"%s%d",cmd,EthIndex);
        //       if(((retval = safe_system(cmd)) < 0))
        //       {
        //           TCRIT("ERROR %d: %s failed\n",retval,IFUP_BIN_PATH);
        //       }
    }

    else
    {
        TDBG("In LAN mode");
        sprintf(FullIfUp_Eth,"%s %s",IfUp_Ifc,Ifcnametable[EthIndex].Ifcname);
        //       if(((retval = safe_system(FullIfUp_Eth)) < 0))
        //       {
        //           TCRIT("ERROR %d: %s failed\n",retval,IFUP_BIN_PATH);
        //       }
    }
    return 0;
}

/************************************************************************
function : nwMakeIFDown
brief : brings down the interface.
param : EthIndex : Index value of LAN channel.
 ************************************************************************/
int nwMakeIFDown(INT8U EthIndex)
{
    int retval = 0;
    INT8S  FullIfDown_Eth[MAX_STR_LENGTH];

    /*Check the Interface Enabled status*/
    if(Ifcnametable[EthIndex].Enabled != 1)
        return 0;

    /* check for vlan interface. if present then do ifup for vlan. else do ifup for lan interface */
    if( m_NwCfgInfo[EthIndex].VLANID )
    {
        if(CheckInterfacePresence(m_VLANInterface)==0)      //VLAN DOWN
        {
            char cmd[40]=VLAN_ONLY_IFDOWN;
            sprintf(cmd,"%s%d",cmd,EthIndex);
            //            if(((retval = safe_system(cmd)) < 0))
            //            {
            //                TCRIT("ERROR %d: %s failed\n",retval,IFUP_BIN_PATH);
            //            }
        }
    }

    else
    {
        sprintf(FullIfDown_Eth,"%s%s ",IfDown_Ifc,Ifcnametable[EthIndex].Ifcname);
        //        if(((retval = safe_system(FullIfDown_Eth)) < 0))
        //        {
        //            TCRIT("ERROR %d: %s failed\n",retval,IFDOWN_BIN_PATH);
        //        }
    }
    return 0;
}


/************************************************************
Function	: nwIsDhcpRunning()
Parameters	: None
Return value	: Returns dhcpcd's process-id if running,
Otherwise zero.
 ************************************************************/
int nwIsDhcpRunning(INT8U EthIndex)
{
    FILE *dhcpCfgFile = NULL;
    int dhcp_pid = 0;
    INT8S  Dhcp_Pid_file[40];

    sprintf(Dhcp_Pid_file,"%s%s.pid",Dhcp_PID_File,Ifcnametable[EthIndex].Ifcname);
    dhcpCfgFile = fopen(Dhcp_Pid_file,"r");
    if(dhcpCfgFile != NULL)
    {
        fseek(dhcpCfgFile,0,SEEK_END);
        if(ftell(dhcpCfgFile) > 0)
        {
            fseek(dhcpCfgFile,0,SEEK_SET);
            fscanf(dhcpCfgFile,"%d",&dhcp_pid);
        }
        fclose(dhcpCfgFile);
    }
    return dhcp_pid;
}

/*ipv6*/
/************************************************************************ 
function : nwIsDhcp6Running
brief : checks for the dhcp6c daemon running and returns the status.
param : EthIndex : Index value of LAN channel.
 ************************************************************************/
int nwIsDhcp6Running(INT8U EthIndex)
{
    FILE *dhcp6CfgFile = NULL;
    int dhcp6_pid = 0;
    INT8S  Dhcp6_Pid_file[40];

    sprintf(Dhcp6_Pid_file,"%s%s.pid",Dhcp6_PID_File,Ifcnametable[EthIndex].Ifcname);
    dhcp6CfgFile = fopen(Dhcp6_Pid_file,"r");
    if(dhcp6CfgFile != NULL)
    {
        fseek(dhcp6CfgFile,0,SEEK_END);
        if(ftell(dhcp6CfgFile) > 0)
        {
            fseek(dhcp6CfgFile,0,SEEK_SET);
            fscanf(dhcp6CfgFile,"%d",&dhcp6_pid);
        }
        fclose(dhcp6CfgFile);
    }
    return dhcp6_pid;
}


/************************************************************************
function : isFamilyEnabled
brief : checks for the family type (i.e. AF_INET or AF_INET6) availability from the interfaces file.
param : IfaceFileAbsPath : interface file's absolute path
param : ifname : interface name to be tested.
param : family : family type to be checked.
 ************************************************************************/
static int isFamilyEnabled(char *IfaceFileAbsPath, char *ifname, int family)
{
    FILE*   fpinterfaces;
    int     ipfound = 0;
    char    oneline [100];

    if(IfaceFileAbsPath == NULL)
        fpinterfaces = fopen(NETWORK_IF_FILE,"r");
    else
        fpinterfaces = fopen(IfaceFileAbsPath,"r");

    if (fpinterfaces == NULL)
    {
        //no file just return NULL entires
        TDBG("no file just return NULL entires\n");
        return 0;
    }

    //now read resolv.conf file here
    while (!feof(fpinterfaces))
    {
        if (fgets(oneline,79,fpinterfaces) == NULL)
        {
            //printf("EOF reading resolv.conf ifle\n");
            break;
        }

        if( oneline[0] == '#') continue;

        if(strstr(oneline, "iface"))
        {
            switch(family)
            {
                case AF_INET6 :
                    /* ipv6 address family */
                    if(strstr(oneline, "inet6") != 0 && strstr(oneline, ifname) != 0 )
                    {
                        ipfound = 1;
                    }
                    break;
                case AF_INET :
                    /* ipv4 address family */                    
                    if((strstr(oneline, "inet6") == 0) && (strstr(oneline, "inet") != 0) && (strstr(oneline, ifname) != 0))
                    {
                        ipfound = 1;
                    }
                    //    ipfound = (!IsIPV4Disabled());
                    break;
            }
        }

    }
    fclose (fpinterfaces);

    return ipfound;
}

int nwGetNWInformations(NWCFG_STRUCT *cfg,char *IFName)
{
    int r;
    struct ifreq ifr;
    int skfd;
    unsigned char tmp[MAC_ADDR_LEN];
    struct sockaddr_in *info;

    skfd = socket(PF_INET, SOCK_DGRAM, 0 );
    if ( skfd < 0 )
    {
        TCRIT("can't open socket: %s\n",strerror(errno));
        return -1;
    }

    /* Get MAC address */
    memset(&ifr,0,sizeof(struct ifreq));
    memset(tmp, 0, MAC_ADDR_LEN);
    strcpy(ifr.ifr_name, IFName);
    ifr.ifr_hwaddr.sa_family = AF_INET;
    r = ioctl(skfd, SIOCGIFHWADDR, &ifr);
    if ( r < 0 )
    {
        TCRIT("IOCTL to get MAC failed: %d\n",r);
        close (skfd);
        return -1;
    }
    memcpy(cfg->MAC, ifr.ifr_hwaddr.sa_data, MAC_ADDR_LEN);

    /* Get IP address */
    memset(&ifr,0,sizeof(struct ifreq));
    memset(tmp, 0, IP_ADDR_LEN);
    strcpy(ifr.ifr_name, IFName);
    info = (struct sockaddr_in *) &(ifr.ifr_addr);
    info->sin_family = AF_INET;
    r = ioctl(skfd, SIOCGIFADDR, &ifr);
    if ( r < 0 )
    {
        close (skfd);
        TDBG("IOCTL to get IP failed: %d\n",r);		// This is a normal case. Don't print message
        return -2;
    }
    memcpy(cfg->IPAddr,(char *) &(info->sin_addr.s_addr),IP_ADDR_LEN);

    /* Get Broadcast address */
    memset(&ifr,0,sizeof(struct ifreq));
    memset(tmp, 0, IP_ADDR_LEN);
    strcpy(ifr.ifr_name, IFName);
    info = (struct sockaddr_in *) &(ifr.ifr_broadaddr);
    info->sin_family = AF_INET;
    r = ioctl(skfd, SIOCGIFBRDADDR, &ifr);
    if ( r < 0 )
    {
        close (skfd);
        TCRIT("IOCTL to get Broadcast failed: %d\n",r);
        return -1;
    }
    memcpy(cfg->Broadcast,(char *) &(info->sin_addr.s_addr),IP_ADDR_LEN);

    /* Get Netmask */
    memset(&ifr,0,sizeof(struct ifreq));
    memset(tmp, 0, IP_ADDR_LEN);
    strcpy(ifr.ifr_name, IFName);
    info = (struct sockaddr_in *) &(ifr.ifr_netmask);
    info->sin_family = AF_INET;
    r = ioctl(skfd, SIOCGIFNETMASK, &ifr);
    if ( r < 0 )
    {
        close (skfd);
        TCRIT("IOCTL to get Netmask failed: %d\n",r);
        return -1;
    }
    memcpy(cfg->Mask,(char *) &(info->sin_addr.s_addr),IP_ADDR_LEN);

    (void) close(skfd);
    return 0;
}


int nwGetNWInformation(NWCFG_STRUCT *cfg,INT8U EthIndex)
{
    int r;
    struct ifreq ifr;
    int skfd;
    unsigned char tmp[MAC_ADDR_LEN];
    struct sockaddr_in *info;
    INT8S  FullEth_Network_if[MAX_STR_LENGTH];

    skfd = socket(PF_INET, SOCK_DGRAM, 0 );
    if ( skfd < 0 )
    {
        printf("can't open socket: %s\n",strerror(errno));
        return -1;
    }

    /* Get MAC address */
    memset(&ifr,0,sizeof(struct ifreq));
    memset(tmp, 0, MAC_ADDR_LEN);


    memset(FullEth_Network_if,0,MAX_STR_LENGTH);

    if(m_LANIndex)
    {
        sprintf(FullEth_Network_if,"%s",Ifcnametable[EthIndex].Ifcname);
    }
    else
    {
        sprintf(FullEth_Network_if,"%s",m_VLANInterface);
    }

    strncpy(ifr.ifr_name, FullEth_Network_if,IFNAMSIZ - 1);

    ifr.ifr_hwaddr.sa_family = AF_INET;
    r = ioctl(skfd, SIOCGIFHWADDR, &ifr);
    if ( r < 0 )
    {
        printf("IOCTL to get MAC failed: %d\n",r);
        close (skfd);
        return -1;
    }
    memcpy(cfg->MAC, ifr.ifr_hwaddr.sa_data, MAC_ADDR_LEN);

    /* Get IP address */
    memset(&ifr,0,sizeof(struct ifreq));
    memset(tmp, 0, IP_ADDR_LEN);
    memset(cfg->IPAddr,0,sizeof(cfg->IPAddr));
    memset(cfg->Broadcast,0,sizeof(cfg->Broadcast));
    memset(cfg->Mask,0,sizeof(cfg->Mask));

    strncpy(ifr.ifr_name, FullEth_Network_if,IFNAMSIZ - 1);

    info = (struct sockaddr_in *) &(ifr.ifr_addr);
    info->sin_family = AF_INET;

    r = ioctl(skfd, SIOCGIFADDR, &ifr);

    if ( r == 0 )
    {
        memcpy(cfg->IPAddr,(char *) &(info->sin_addr.s_addr),IP_ADDR_LEN);

        /* Get Broadcast address */
        memset(&ifr,0,sizeof(struct ifreq));
        memset(tmp, 0, IP_ADDR_LEN);

        strncpy(ifr.ifr_name, FullEth_Network_if,IFNAMSIZ - 1);
        info = (struct sockaddr_in *) &(ifr.ifr_broadaddr);
        info->sin_family = AF_INET;
        r = ioctl(skfd, SIOCGIFBRDADDR, &ifr);
        if ( r < 0 )
        {
            printf("IOCTL to get Broadcast failed: %d\n",r);
            return -1;
        }
        memcpy(cfg->Broadcast,(char *) &(info->sin_addr.s_addr),IP_ADDR_LEN);

        /* Get Netmask */
        memset(&ifr,0,sizeof(struct ifreq));
        memset(tmp, 0, IP_ADDR_LEN);

        strncpy(ifr.ifr_name, FullEth_Network_if,IFNAMSIZ - 1);

        info = (struct sockaddr_in *) &(ifr.ifr_netmask);
        info->sin_family = AF_INET;
        r = ioctl(skfd, SIOCGIFNETMASK, &ifr);
        if ( r < 0 )
        {
            close (skfd);
            TCRIT("IOCTL to get Netmask failed: %d\n",r);
            return -1;
        }

        memcpy(cfg->Mask,(char *) &(info->sin_addr.s_addr),IP_ADDR_LEN);

    }
    (void) close(skfd);
    return 0;
}

/**
 *@fn nwReadNWCfg_IPv6
 *@brief This function is invoked to read only ipv6 network configurations.
 *@param cfg - Pointer to Structure used to get IPv6 network configurations
 *@param EthIndex - pointer to char used to store Interface Index value.
 *@return Returns 0 on success and -1 on fails
 */
int nwReadNWCfg_IPv6(NWCFG6_STRUCT *cfg, INT8U EthIndex)
{
    struct ifaddrs *ifAddrOrig = 0;
    struct ifaddrs *ifAddrsP = 0;
    int retValue = 0;
    INT8S  FullEth_Network_if[MAX_STR_LENGTH];

    if(m_NwCfgInfo[EthIndex].VLANID)
    {
        TDBG("nwcfg.c : nwReadNWCfg_IPv6 : In VLAN mode");
        sprintf(FullEth_Network_if,"%s.%d",Ifcnametable[EthIndex].Ifcname,m_NwCfgInfo[EthIndex].VLANID);
    }
    else
    {
        TDBG("nwcfg.c : nwReadNWCfg_IPv6 : In LAN mode");
        sprintf(FullEth_Network_if,"%s",Ifcnametable[EthIndex].Ifcname);
    }

    /* do memset for cfg struct to avoid junk values */
    memset (cfg,0,sizeof(NWCFG6_STRUCT));

    if(m_NwCfgInfo[EthIndex].VLANID)
        cfg->enable = (unsigned char) isFamilyEnabled(VLAN_INTERFACES_FILE, FullEth_Network_if, AF_INET6);
    else            
        cfg->enable = (unsigned char) isFamilyEnabled(NETWORK_IF_FILE, FullEth_Network_if, AF_INET6);

    if(cfg->enable != 1)
    {
        memset(cfg,0,sizeof(NWCFG6_STRUCT));
        TDBG("IPv6 Disabled.. !");
        return 0;
    }
    /* we have two options to check for IP address source. either checking for 
       dhcp6c process running or checks for the /etc/network/interfaces file */

    GetNoofInterface();

    cfg->CfgMethod = m_NwCfgInfo_v6[EthIndex].CfgMethod;

    /* getifaddrs - which will get all the available list of network addresses. so 
       after getting the addresses, we have to filter it with address family and EthIndex */
    if (getifaddrs (&ifAddrOrig) == 0)
    {
        ifAddrsP = ifAddrOrig;
        int index=0;
        while (ifAddrsP != NULL )
        {
            if(strcmp (ifAddrsP->ifa_name,FullEth_Network_if) == 0 && ifAddrsP->ifa_addr->sa_family == AF_INET6 )
            {
                unsigned char temp[3];
                memcpy (temp, ((struct sockaddr_in6 *)(ifAddrsP->ifa_addr))->sin6_addr.s6_addr16, 2);

                /* check for link-local ip */
                if(temp[0] == 0xFE && temp[1]>>6 == 2)
                {
                    // Link Local
                    memcpy (cfg->LinkIPAddr, ((struct sockaddr_in6 *)(ifAddrsP->ifa_addr))->sin6_addr.s6_addr16, 16);
                    cfg->LinkPrefix = (unsigned char)ConvertNETMaskArrayTonum(((struct sockaddr_in6 *)(ifAddrsP->ifa_netmask))->sin6_addr.s6_addr,IP6_ADDR_LEN);
                }
                else if (temp[0] == 0xFE && temp[1]>>6 == 3)
                {
                    //Site Local
                    memcpy (cfg->SiteIPAddr, ((struct sockaddr_in6 *)(ifAddrsP->ifa_addr))->sin6_addr.s6_addr16, 16);
                    cfg->SitePrefix = (unsigned char)ConvertNETMaskArrayTonum(((struct sockaddr_in6 *)(ifAddrsP->ifa_netmask))->sin6_addr.s6_addr,IP6_ADDR_LEN);
                }
                else
                {
                    /* retrived ip is global ipv6 address */				
                    if (index < MAX_IPV6ADDRS) 
                    {
                        memcpy ( cfg->GlobalIPAddr[index], ((struct sockaddr_in6 *)(ifAddrsP->ifa_addr))->sin6_addr.s6_addr16, 16);
                        cfg->GlobalPrefix[index] = (unsigned char)ConvertNETMaskArrayTonum(((struct sockaddr_in6 *)(ifAddrsP->ifa_netmask))->sin6_addr.s6_addr,IP6_ADDR_LEN);
                        index++;
                    }
                }
            }
            ifAddrsP = ifAddrsP->ifa_next;
        }
        freeifaddrs(ifAddrOrig);
    }
    else
    {
        TCRIT("getifaddrs failed");
        return -1;
    }

    //    if (cfg->GlobalPrefix[0] == 0)
    //    {
    /* set the default prefix length to 64, which is ipv6 default */
    /*        cfg->GlobalPrefix[0] = 64;
              }*/

    /* Get default gateway */
    if((retValue = GetDefaultGateway_ipv6(cfg->Gateway,&EthIndex) ))
    {
        if(retValue == ENODATA)
        {
            memset(cfg->Gateway, 0, IP6_ADDR_LEN);
        }
        else
        {
            TCRIT("GetDefaultGateway_IPv6 failed");
            /* if reading of gateway fails dont return value as -1, it means reading of network configuration operation is failed.. */
            //return -1;
            return 0;
        }
    }

    return 0;
}



/* IPv6_route format
   ====================
   00000000000000000000000000000000 00 00000000000000000000000000000000 00
   +------------------------------+ ++ +------------------------------+ ++
   |                                |  |                                |
   1                                2  3                                4

   00000000000000000000000000000000 ffffffff 00000001 00000001 00200200 lo
   +------------------------------+ +------+ +------+ +------+ +------+ ++
   |                                |        |        |        |        |
   5                                6        7        8        9        10

   1. IPv6 destination network displayed in 32 hexadecimal chars without colons
   as separator
   2. IPv6 destination prefix length in hexadecimal
   3. IPv6 source network displayed in 32 hexadecimal chars without colons as
   separator
   4. IPv6 source prefix length in hexadecimal
   5. IPv6 next hop displayed in 32 hexadecimal chars without colons as separator
   6. Metric in hexadecimal
   7. Reference counter
   8. Use counter
   9. Flags
   10. Device name
 */
/* This function scans thru the /proc/net/ipv6_route system file and looks for
   the default gateway entry */
int GetDefaultGateway_ipv6(unsigned char *gw,INT8U *Interface)
{
    FILE* ipv6_routeFp = NULL;
    char tempstr[256];
    char FirstTime = 1;
    char iface[16];
    char FullEth_Network_if[MAX_STR_LENGTH];
    unsigned char otherstr[50];
    unsigned char gway[INET6_ADDRSTRLEN];
    unsigned char addrstr[50];
    unsigned int flags;
    unsigned int t;
    int i, j;

    if((ipv6_routeFp = fopen(PROC_NET_IPV6ROUTE_FILE,"r")) == NULL)
    {
        TCRIT("fopen failed for %s\n",PROC_NET_IPV6ROUTE_FILE);
        return errno;
    }

    /* Get Gateway entry with the flag and interface name */

    /* We assume that the interface is eth0. It however can be
       changed to any interface type, we can take that interface type
       as an input parameter */
    sprintf(FullEth_Network_if,"%s",Ifcnametable[*Interface].Ifcname);

    while(!feof(ipv6_routeFp))
    {
        fgets(tempstr,256,ipv6_routeFp);
        if(FirstTime)
        {
            /* We can simply ignore first line as it just contains the field names */
            FirstTime = 0;
            continue;
        }

        if(strcmp(tempstr,"\0")) // Skip empty lines
        {
            sscanf(tempstr,"%32s %02x %32s %02x %32s %08x %08x %08x %08x %s\n",
                    otherstr,	// Destination network
                    &t, 		// Destination prefix length
                    otherstr, 	// Source network
                    &t, 		// Source prefix length
                    gway,	 	// Gateway
                    &t,			// Metric
                    &t,			// Reference counter
                    &t,			// Use counter
                    &flags,		// Flags
                    iface);		// Interface

            /* Default gateway entry has the flag field set to 0x0003 */
            if((flags == DEFAULT_GW_FLAGS) && (strcmp(iface,(char *) FullEth_Network_if) == 0))
            {
                i = 0; j = 0;
                while (i < 32)
                {
                    addrstr[j++] = gway[i++];

                    if ((i % 4) == 0)
                    {
                        addrstr[j++] = ':';
                    }
                }
                addrstr[--j] = '\0';

                TDBG("Gw addstr : %s", addrstr);
                if (inet_pton(AF_INET6, (char*) addrstr, gw) == 0)
                {
                    TCRIT("Error converting v6 gateway IP address\n");
                    fclose(ipv6_routeFp);
                    return ENODATA; /* No default entry */
                }

                break;
            }
        }
    }

    if(feof(ipv6_routeFp))
    {
        TDBG("No default entry in gateway info.\n");
        fclose(ipv6_routeFp);
        return ENODATA; /* No default entry */
    }

    fclose(ipv6_routeFp);
    return 0;
}

/* This function scans thru the /proc/net/route system file and looks for
   the default gateway entry */
int GetDefaultGateway(unsigned char *gw,INT8U *Interface)
{
    FILE* routeFp = NULL;
    char tempstr[256];
    char FirstTime = 1;
    char iface[16];
    unsigned long dest;
    unsigned long gway;
    unsigned long flags;
    unsigned long t;

    if((routeFp = fopen(PROC_NET_ROUTE_FILE,"r")) == NULL)
    {
        TCRIT("fopen failed for %s\n",PROC_NET_ROUTE_FILE);
        return errno;
    }

    /* Get Gateway entry with the flag and interface name */

    /* We assume that the interface is eth0. It however can be
       changed to any interface type, we can take that interface type
       as an input parameter */

    while(!feof(routeFp))
    {
        fgets(tempstr,256,routeFp);
        if(FirstTime)
        {
            /* We can simply ignore first line as it just contains the field names */
            FirstTime = 0;
            continue;
        }

        if(strcmp(tempstr,"\0")) // Skip empty lines
        {
            sscanf(tempstr,"%s	%lx	%lx	%lx	%lx	%lx	%lx	%lx	%lx	%lx	%lx\n",
                    iface,&dest,&gway,&flags,&t,&t,&t,&t,&t,&t,&t);
            /* Default gateway entry has the flag field set to 0x0003 */
            if((flags == DEFAULT_GW_FLAGS) && (strcmp(iface,(char *)Interface) == 0))
            {
                memcpy(gw,&gway,IP_ADDR_LEN);
                break;
            }
        }
    }

    if(feof(routeFp))
    {
        TDBG("No default entry in gateway info.\n");
        fclose(routeFp);
        return ENODATA; /* No default entry */
    }

    fclose(routeFp);
    return 0;
}


/**
 *@fn IsKernelIPv6Enabled
 *@brief This function is used to check for IPv6 support in the kernel.
 *@return Returns 0 on success and -1 on fails
 */
int IsKernelIPv6Enabled()
{
    FILE *fpproc;

    fpproc = fopen(KERNEL_IPV6_FILE,"r");

    if (fpproc == NULL)    
    {
        TCRIT (" File %s is not found. so IPv6 support is not present in kernel ", KERNEL_IPV6_FILE);
        return 0;
    }
    else
    {
        TCRIT (" File %s is present. so IPv6 support is present in kernel ", KERNEL_IPV6_FILE);
        fclose(fpproc);
        return 1;
    }

    /*control will not reach here */
    return 0;
}


/**
 *@fn nwReadNWCfg_v4_v6
 *@brief This function is invoked to Get the current network status of both IPv4 and IPv6 networks.
 *@		If there is no need of IPv6 data means, then just pass NULL to IPv6 pointer.
 *@param cfg - Pointer to Structure used to get IPv4 network configurations.
 *@param cfg6 - Pointer to Structure used to get IPv6 network configurations.
 *@param EthIndex - pointer to char used to store Interface Index value.
 *@return Returns 0 on success and -1 on fails
 */
int nwReadNWCfg_v4_v6(NWCFG_STRUCT *cfg, NWCFG6_STRUCT *cfg6, INT8U EthIndex,int global_ipv6)
{
    int retValue = 0;
    INT8S FullEth_Network_if[MAX_STR_LENGTH];

    /* Get Max no. of Interfaces */
    GetNoofInterface();

    /* check for NULL pointer */
    if( cfg == NULL )
    {
        TCRIT(" Pointer that is passed to get IPv4 n/w configuration is NULL. so returning ");
        return 0;
    }

    /* IPv6 pointer can be NULL also. so If it fails in NULL checking just continue*/
    if( cfg6 == NULL )
    {
        TDBG(" Pointer that is passed to get IPv6 n/w configuration is NULL. so just ignore getting Ipv6 network information ");
        //return 0; //just continue
    }


    /* Intialize the cfg structure */
    memset(cfg, 0, sizeof(NWCFG_STRUCT));

    /*  Update the  Eth Configuration */
    GetNwCfgInfo();

    /* while reading network configurations check for enable status. else return all zeros */
    if(m_NwCfgInfo[EthIndex].enable != 1)
    {
        if(m_NwCfgInfo_v6[EthIndex].enable == 1)
        {
            if(global_ipv6 == ENABLED)
            {
                TDBG("IPV6 only enabled");
            }
            else
            {
                TDBG ("\n LAN Enable is not true.. i.e. disabled\n");
                memset(cfg, 0, sizeof(NWCFG_STRUCT));
                return 0;
            }
        }
        else
        {
            TDBG ("\n LAN Enable is not true.. i.e. disabled\n");
            memset(cfg6, 0, sizeof(NWCFG6_STRUCT));
            return 0;
        }
    }
    memcpy(cfg,&m_NwCfgInfo[EthIndex],sizeof(m_NwCfgInfo[EthIndex]));
    memset(FullEth_Network_if,0,MAX_STR_LENGTH);

    if(m_NwCfgInfo[EthIndex].VLANID)
    {
        TDBG("nwcfg.c : nwReadNWCfg : In VLAN mode");
        sprintf(FullEth_Network_if,"%s.%d",Ifcnametable[EthIndex].Ifcname,m_NwCfgInfo[EthIndex].VLANID);
    }
    else
    {
        TDBG("nwcfg.c : nwReadNWCfg : In LAN mode");
        sprintf(FullEth_Network_if,"%s",Ifcnametable[EthIndex].Ifcname);
    }

    /* Get default gateway */
    retValue = GetDefaultGateway(cfg->Gateway,(INT8U *)FullEth_Network_if);
    if(retValue != 0)
    {
        memset(cfg->Gateway,0,IP_ADDR_LEN);
        /* If getting the gateway value fails just return 0, dont return the error codes
           because returning the error value makes n/w read operation to be getting failed. */
        retValue = 0;
    }

    /* function that is used to get IPv6 network configurations */
    if( cfg6 != NULL )
    {
        retValue = nwReadNWCfg_IPv6( cfg6, EthIndex );
        if(retValue != 0)
        {
            TCRIT(" Failed to Get IPv6 network configurations ");
            /* If getting the ipv6 value fails just return 0, dont return the error codes
               because returning the error value makes n/w read operation to be getting failed. */
            retValue = 0;
        }
    }
    else
    {
        TDBG(" Struct variable that is sent to get IPv6 network configuration is NULL");
    }

    return retValue;
}


/************************************************************************
Function	: nwReadNWCfgs()
Details:	: Gets the current network status based on multiple network
adapters and updates the .conf file in the system.
 ************************************************************************/
int nwReadNWCfgs(NWCFGS *cfg, ETHCFGS *ethcfg)
{
    FILE *fpdev;
    int ret = 0;
    int retValue = 0, i=0;
    char name[75];
    char InterfaceName[10];
    char count;
    char oneline[80];
    fpdev = fopen(DEV_FILE, "r");

    if(fpdev == NULL)
    {
        TDBG("Error in opening network device file\n");
        return -1;
    }
    memset(cfg,0,sizeof(NWCFGS));

    while(!feof(fpdev))
    {
        memset(InterfaceName,0,10);
        if (fgets(oneline,79,fpdev) == NULL)
            break;

        if(sscanf(oneline,"  eth%s", name) == 1)
        {
            count = name[0];
            sprintf(InterfaceName, "eth%c", count);
        }

        if(strcmp(InterfaceName, ""))
        {
            if(nwIsDhcpRunning(0) != 0)
                cfg->NwInfo[i].CfgMethod = CFGMETHOD_DHCP;
            else
                cfg->NwInfo[i].CfgMethod = CFGMETHOD_STATIC;

            ret = nwGetNWInformations(&(cfg->NwInfo[i]), InterfaceName);
            if(ret != 0)
            {
                /*
                 * Dont print this message in a typical case where interface has no ip
                 */
                if (ret != -2)
                    TCRIT("Unable to get network interface information for %s\n", InterfaceName);
                /*
                 * Continue with the next interface
                 */
                continue;
            }

            if(ethcfg != NULL)
            {
                //				strcpy((char *)cfg->NwInfo[i].IFName, InterfaceName);
                memcpy(cfg->NwInfo[i].IFName, InterfaceName, strlen(InterfaceName));
                if(nwGetEthInformation(&(ethcfg->EthInfo[i]), InterfaceName) != 0)
                {
                    TCRIT("Unable to get Ethernet interface information\n");
                    /*
                     * Continue with the next interface
                     */
                    continue;
                }
            }
            retValue = GetDefaultGateway(cfg->NwInfo[i].Gateway,(INT8U*)InterfaceName);
            if(retValue != 0)
                memset(cfg->NwInfo[i].Gateway,0,IP_ADDR_LEN);
            i++;
        }
    }
    cfg->IFCount = i;
    fclose(fpdev);
    return retValue;
}


/************************************************************************
function : nwUpdateInterfaces_v4_v6
brief : function that used to update network configuration file.
 ************************************************************************/
int nwUpdateInterfaces_v4_v6()
{
    FILE *cfgFile = NULL;
    INT8S  FullEth_Network_if[MAX_STR_LENGTH];
    int i;
    int j = 0;
    unsigned char str[32];
    unsigned char NewAddr[IP6_ADDR_LEN];
    unsigned char str1[INET6_ADDRSTRLEN];

    if((cfgFile = fopen(NETWORK_IF_FILE,"w"))==NULL)
    {
        TCRIT("Could not open %s\n",NETWORK_IF_FILE);
        return errno;
    }
    else
    {
        fprintf(cfgFile,AUTO_LOCAL_STR);
        fprintf(cfgFile,AUTO_LOCAL_LOOPBACK_STR);

        TDBG("m_NoofInterface : %d", m_NoofInterface );
        for(i=0;i<sizeof(Ifcnametable)/sizeof(IfcName_T);i++)
        {
            if(Ifcnametable[i].Ifcname[0] == 0)
                continue;
            if (m_NwCfgInfo[Ifcnametable[i].Index].enable != 1)
            {
                // If interface is not enabled dont write the network configurations
                if( m_NwCfgInfo_v6[Ifcnametable[i].Index].enable == 0 )
                {
                    TDBG("Interface eth%d is not enabled. so dont write anything",Ifcnametable[i].Index);
                    continue;
                }   
            }
            else
            {
                // If interface is not enabled dont write anything into config file.
                TDBG(" Interface is enabled...!!! ");
            }

            /* Store the NwCFg info */
            fprintf(cfgFile,"auto  ");
            memset(FullEth_Network_if,0,MAX_STR_LENGTH);
            sprintf(FullEth_Network_if,"%s  \n ",Ifcnametable[i].Ifcname);
            fprintf(cfgFile,"%s",FullEth_Network_if);

            if( m_NwCfgInfo_v6[Ifcnametable[i].Index].enable == 1 )
            {
                TDBG("nwWriteNwcfg for IPv6 ");
                if(m_NwCfgInfo_v6[Ifcnametable[i].Index].CfgMethod == CFGMETHOD_DHCP)
                {
                    TDBG("nwWriteNwcfg for IPv6 - DHCP");
                    memset(FullEth_Network_if,0,MAX_STR_LENGTH);
                    sprintf(FullEth_Network_if,"iface %s inet6 autoconf\n  ",Ifcnametable[i].Ifcname);
                    fprintf(cfgFile,"%s",FullEth_Network_if);
                }
                else
                {
                    TDBG("nwWriteNwcfg for IPv6 - STATIC");
                    /* To print the eth0/1/2 inet static  */
                    memset(FullEth_Network_if,0,MAX_STR_LENGTH);
                    sprintf(FullEth_Network_if,"iface %s inet6 static\n  ",Ifcnametable[i].Ifcname);
                    fprintf(cfgFile,"%s",(INT8U*)FullEth_Network_if);

                    /* IPv6 address */
                    ConvertIP6numToStr(m_NwCfgInfo_v6[Ifcnametable[i].Index].GlobalIPAddr[0],INET6_ADDRSTRLEN,str1);
                    if (strcmp ("::", (char *) str1) == 0)
                    {
                        strcpy( (char*) str1, " ");
                        m_NwCfgInfo_v6[Ifcnametable[i].Index].GlobalPrefix[0] = 0;
                    }
                    fprintf(cfgFile,"%s %s\n",IF_STATIC_IP_STR,str1);

                    /* IPv6 Prefix */
                    fprintf(cfgFile,"%s %d\n",IF_STATIC_MASK_STR,m_NwCfgInfo_v6[Ifcnametable[i].Index].GlobalPrefix[0]);

                    /* IPv6 Gateway */
                    ConvertIP6numToStr( m_NwCfgInfo_v6[Ifcnametable[i].Index].Gateway,46,str1);
                    if (strncmp("fe80::", (char*) str1, strlen("fe80::")) == 0)
                    {
                        // Gateway address is a link local address, add device name "eth0"
                        fprintf(cfgFile, "%s %s dev %s\n", IF_STATIC_GW_STR, str1, Ifcnametable[i].Ifcname);
                    }
                    else
                    {
                        fprintf(cfgFile, "%s %s\n", IF_STATIC_GW_STR, str1);
                    }
                    memset (NewAddr, 0, sizeof (NewAddr));
                    /*Add the static ipv6 address to file*/
                    for (j = 1; j< MAX_IPV6ADDRS; j++)
                    {
                        if (0 != memcmp(NewAddr, m_NwCfgInfo_v6[Ifcnametable[i].Index].GlobalIPAddr[j], IP6_ADDR_LEN))
                        {
                            ConvertIP6numToStr(m_NwCfgInfo_v6[Ifcnametable[i].Index].GlobalIPAddr[j],INET6_ADDRSTRLEN,str1);
                            fprintf(cfgFile, "%s %s/%d %s %s\n", IF_STATIC_ADDR_STR, str1, m_NwCfgInfo_v6[Ifcnametable[i].Index].GlobalPrefix[j], \
                                    "dev", Ifcnametable[i].Ifcname);
                        }
                    }
                }
            }

            // Dont write IPV4 configurations if it is disabled.
            if (m_NwCfgInfo[Ifcnametable[i].Index].enable == 1)
            {
                if(m_NwCfgInfo[Ifcnametable[i].Index].CfgMethod == CFGMETHOD_DHCP)
                {
                    TDBG("nwWriteNwcfg for IPv4 - DHCP");
                    memset(FullEth_Network_if,0,MAX_STR_LENGTH);
                    sprintf(FullEth_Network_if,"iface %s inet dhcp\n  ",Ifcnametable[i].Ifcname);
                    fprintf(cfgFile,"%s",FullEth_Network_if);
                }
                else
                {

                    TDBG("nwWriteNwcfg for IPv4- STATIC");
                    /* To print the eth0/1/2 inet static  */
                    memset(FullEth_Network_if,0,MAX_STR_LENGTH);
                    sprintf(FullEth_Network_if,"iface %s inet static\n  ",Ifcnametable[i].Ifcname);
                    fprintf(cfgFile,"%s",(INT8U*)FullEth_Network_if);

                    ConvertIPnumToStr(m_NwCfgInfo[Ifcnametable[i].Index].IPAddr,IP_ADDR_LEN,str);
                    fprintf(cfgFile,"%s %s\n",IF_STATIC_IP_STR,str);

                    ConvertIPnumToStr(m_NwCfgInfo[Ifcnametable[i].Index].Mask,IP_ADDR_LEN,str);
                    fprintf(cfgFile,"%s %s\n",IF_STATIC_MASK_STR,str);

                    ConvertIPnumToStr(m_NwCfgInfo[Ifcnametable[i].Index].Broadcast,IP_ADDR_LEN,str);
                    fprintf(cfgFile,"%s %s\n",IF_STATIC_BCAST_STR,str);

                    ConvertIPnumToStr(m_NwCfgInfo[Ifcnametable[i].Index].Gateway,IP_ADDR_LEN,str);
                    fprintf(cfgFile,"%s %s\n",IF_STATIC_GW_STR,str);
                }
            }
        }
        fclose(cfgFile);
    }
    return 0;
}

/**
 *@fn Write_dhcp6c_conf
 *@brief This function is used to write interface wise entries for dhcp6c.conf file.
 *@return Returns 0 on success and -1 on fails
 */
int Write_dhcp6c_conf()
{

    FILE *fpw;
    char oneline[80];
    char Ifname[16];
    int i;

    fpw=fopen(DHCP6CCONF,"w");
    if(fpw == NULL)
    {
        TCRIT(" Error Opening %s file \n", NEW_DHCP6CCONF);
        return -1;
    }

    GetNoofInterface();

    for(i=0;i<sizeof(Ifcnametable)/sizeof(IfcName_T);i++)
    {
        if(Ifcnametable[i].Ifcname[0] == 0)
            continue;

        if(m_NwCfgInfo_v6[i].enable != 1 )
            continue;

        if(m_NwCfgInfo[Ifcnametable[i].Index].VLANID)
        {
            sprintf(Ifname,"%s.%d",Ifcnametable[i].Ifcname,m_NwCfgInfo[Ifcnametable[i].Index].VLANID);
        }
        else
            sprintf(Ifname,"%s",Ifcnametable[i].Ifcname);

        INT8U iana = i+1;

        TDBG("\n iana - %d \n", iana); 
        sprintf(oneline,"interface %s {",Ifname);
        fputs(oneline,fpw);
        //fputs("interface eth1 {", fpw);

        sprintf(oneline,"\n\tsend ia-na %d%d%d%d%d;",iana,iana,iana,iana,iana);
        fputs(oneline,fpw);
        //fputs("\n\tsend ia-na 22222;", fpw);

        fputs("\n\trequest domain-name-servers;", fpw);
        fputs("\n\trequest domain-name;", fpw);
        fputs("\n\tscript \"/conf/dhcp6c-script\";\n\t};", fpw);

        sprintf(oneline,"\nid-assoc na %d%d%d%d%d {};\n",iana,iana,iana,iana,iana);
        fputs(oneline,fpw);
        //fputs("\nid-assoc na 22222 {};\n",fpw);
    }

    TDBG("\n Enable bit is not set.. so skipping eth1 entries...!!! \n");
    fputs("\n",fpw);
    fclose(fpw);

    return 0;
}
/**
 *@fn nwSyncNWCfg_ipv6
 *@brief This function is invoked to sync IPv6 network configurations.
 *@param cfg6 - Pointer to Structure used to set IPv6 network configurations
 *@param EthIndex - pointer to char used to store Interface Index value.
 *@return Returns 0 on success and -1 on fails
 */
int nwSyncNWCfg_ipv4_v6(NWCFG_STRUCT *cfg,NWCFG6_STRUCT *cfg6, INT8U EthIndex)
{
    int retValue = 0;
    unsigned int ipA,ipB,ipC,ipD = 0;
    unsigned int nmA,nmB,nmC,nmD = 0;
    unsigned char bcA,bcB,bcC,bcD;

    /* check for NULL pointer */
    if( cfg == NULL )
    {
        TCRIT(" Pointer that is passed to set IPv4 n/w configuration is NULL. so returning ");
        return 0;
    }

    /* IPv6 pointer can be NULL also. so If it fails in NULL checking just continue*/
    if( cfg6 == NULL && cfg->enable != 0)
    {
        TCRIT(" Pointer that is passed to set IPv6 n/w configuration is NULL. so just ignore setting Ipv6 network information ");
        //return 0; //just continue
    }


    /* Get Max no.of Interfaces */
    GetNoofInterface();

    TDBG("iface down for %s is done", Ifcnametable[EthIndex].Ifcname);

    /* Calculate Broadcast if STATIC, and update it */
    if(cfg->CfgMethod == CFGMETHOD_STATIC)
    {
        ipA = cfg->IPAddr[0];ipB = cfg->IPAddr[1];ipC = cfg->IPAddr[2];ipD = cfg->IPAddr[3];
        nmA = cfg->Mask[0];nmB = cfg->Mask[1];nmC = cfg->Mask[2];nmD = cfg->Mask[3];

        /* Update Broadcast field */
        bcA = (ipA & nmA) | ~nmA;
        bcB = (ipB & nmB) | ~nmB;
        bcC = (ipC & nmC) | ~nmC;
        bcD = (ipD & nmD) | ~nmD;

        cfg->Broadcast[0] = bcA;
        cfg->Broadcast[1] = bcB;
        cfg->Broadcast[2] = bcC;
        cfg->Broadcast[3] = bcD;
    }

    /* If interface is currently disabled and now enable request has been 
       given, then the Interface has to be enabled. so Increment Max no. of 
       interfaces and write the configurations into /etc/network/interfaces */

    if((m_NwCfgInfo[EthIndex].enable == 0) && (cfg->enable == 1))
    {
        m_NoofInterface++;
    }

    memcpy(&m_NwCfgInfo[EthIndex],cfg,sizeof(m_NwCfgInfo[EthIndex]));

    // check for NULL pointer before copying
    if( cfg6 != NULL )
    {
        memcpy(&m_NwCfgInfo_v6[EthIndex],cfg6,sizeof(m_NwCfgInfo_v6[EthIndex]));
    }
    else
    {
        //Ponter is NULL. so just disable IPv6 flag
        m_NwCfgInfo_v6[EthIndex].enable = 0;
    }

    TDBG("memcpy done for both ipv4 and ipv6 addresses");

    /* Also update /etc/network/interfaces */
    //if((retValue = nwUpdateInterfaces()))
    if((retValue = nwUpdateInterfaces_v4_v6()))
    {
        TCRIT("nwUpdateInterfaces() failed.\n");
        return retValue;
    }

    /* Also update vlaninterfaces file */
    if((retValue = nwUpdateVLANInterfacesFile()))
    {
        TCRIT("nwUpdateVLANInterfacesFile() failed.\n");
        return retValue;
    }
    TDBG("Nwcfg.c: VLAN Interfaces file updated");

    if(cfg->enable == 1)
        Ifcnametable[EthIndex].Enabled = 1;
    else
        Ifcnametable[EthIndex].Enabled = 0;

    /* Also update dhcp6c.conf file */
    if( cfg6 != NULL)
    {
        if( (retValue = Write_dhcp6c_conf()) )
        {
            TCRIT("Write_dhcp6c_conf() failed.\n");
            return retValue;
        }
    }

    return retValue;
}
/**
 *@fn nwWriteNWCfg_ipv4_v6
 *@brief This function is invoked to set both IPv4 and IPv6 network configurations.
 *@		If there is no need to write IPv6 data means, then just pass NULL to IPv6 pointer.
 *@param cfg - Pointer to Structure used to set IPv4 network configurations
 *@param cfg6 - Pointer to Structure used to set IPv6 network configurations
 *@param EthIndex - pointer to char used to store Interface Index value.
 *@return Returns 0 on success and -1 on fails
 */
int nwWriteNWCfg_ipv4_v6(NWCFG_STRUCT *cfg, NWCFG6_STRUCT *cfg6, INT8U EthIndex)
{
    int retValue = 0;
    unsigned int ipA,ipB,ipC,ipD = 0;
    unsigned int nmA,nmB,nmC,nmD = 0;
    unsigned char bcA,bcB,bcC,bcD;

    if (m_NwCfgInfo[EthIndex].VLANID) {//enabled
#if 0
        if((retValue = safe_system(VLAN_PROC_SYS_RAC_NCSI_ENABLE_LAN)))
        {
            TCRIT("VLAN_PROC_SYS_RAC_NCSI_ENABLE_LAN () failed.\n");
        }
#endif
    }


    /* check for NULL pointer */
    if( cfg == NULL )
    {
        TCRIT(" Pointer that is passed to set IPv4 n/w configuration is NULL. so returning ");
        return 0;
    }

    /* IPv6 pointer can be NULL also. so If it fails in NULL checking just continue*/
    if( cfg6 == NULL && cfg->enable != 0)
    {
        TCRIT(" Pointer that is passed to set IPv6 n/w configuration is NULL. so just ignore setting Ipv6 network information ");
        //return 0; //just continue
    }


    /* Get Max no.of Interfaces */
    GetNoofInterface();

    TDBG("iface down for %s is done", Ifcnametable[EthIndex].Ifcname);

    /* make interface to be down before making any changes */
    if((retValue = nwMakeIFDown(EthIndex)))
    {
        TCRIT("nwMakeIFDown() failed.\n");
        return retValue;
    }

    /* Calculate Broadcast if STATIC, and update it */
    if(cfg->CfgMethod == CFGMETHOD_STATIC)
    {
        ipA = cfg->IPAddr[0];ipB = cfg->IPAddr[1];ipC = cfg->IPAddr[2];ipD = cfg->IPAddr[3];
        nmA = cfg->Mask[0];nmB = cfg->Mask[1];nmC = cfg->Mask[2];nmD = cfg->Mask[3];

        /* Update Broadcast field */
        bcA = (ipA & nmA) | ~nmA;
        bcB = (ipB & nmB) | ~nmB;
        bcC = (ipC & nmC) | ~nmC;
        bcD = (ipD & nmD) | ~nmD;

        cfg->Broadcast[0] = bcA;
        cfg->Broadcast[1] = bcB;
        cfg->Broadcast[2] = bcC;
        cfg->Broadcast[3] = bcD;
    }

    /* If interface is currently disabled and now enable request has been 
       given, then the Interface has to be enabled. so Increment Max no. of 
       interfaces and write the configurations into /etc/network/interfaces */

    if((m_NwCfgInfo[EthIndex].enable == 0) && (cfg->enable == 1))
    {
        m_NoofInterface++;
    }

    memcpy(&m_NwCfgInfo[EthIndex],cfg,sizeof(m_NwCfgInfo[EthIndex]));

    // check for NULL pointer before copying
    if( cfg6 != NULL )
    {
        memcpy(&m_NwCfgInfo_v6[EthIndex],cfg6,sizeof(m_NwCfgInfo_v6[EthIndex]));
    }
    else
    {
        //Ponter is NULL. so just disable IPv6 flag
        m_NwCfgInfo_v6[EthIndex].enable = 0;
    }

    TDBG("memcpy done for both ipv4 and ipv6 addresses");

    /* Also update /etc/network/interfaces */
    //if((retValue = nwUpdateInterfaces()))
    if((retValue = nwUpdateInterfaces_v4_v6()))
    {
        TCRIT("nwUpdateInterfaces() failed.\n");
        return retValue;
    }

    /* Also update vlaninterfaces file */
    if((retValue = nwUpdateVLANInterfacesFile()))
    {
        TCRIT("nwUpdateVLANInterfacesFile() failed.\n");
        return retValue;
    }
    TDBG("Nwcfg.c: VLAN Interfaces file updated");

    if(cfg->enable == 1)
        Ifcnametable[EthIndex].Enabled = 1;
    else
        Ifcnametable[EthIndex].Enabled = 0;

    /* Also update dhcp6c.conf file */
    if( cfg6 != NULL)
    {
        if( (retValue = Write_dhcp6c_conf()) )
        {
            TCRIT("Write_dhcp6c_conf() failed.\n");
            return retValue;
        }
    }

    if((retValue = nwMakeIFUp(EthIndex)))
    {
        TCRIT("nwMakeIFUp() failed.\n");
        return retValue;
    }

    return retValue;
}

/*
 * @fn nwConfigureBonding
 * @brief This function will update the bonding Configuration
 * @return 0 on success, -1 on failure
 */
int nwGetBondCfg()
{
    FILE *fp;
    INT8U slave =0;
    INT8S   Line[255],*pTok=NULL,Maxslave[8][16],Slave[64];
    INT16U BondMode=0,Interval =0,index=0,Bondindex=0;
    int count =0,i;

    memset(Maxslave,0,sizeof(Maxslave));

    fp = fopen (BONDING_CONF_FILE,"r");
    if(fp ==NULL)
    {
        printf("\n UpdateBondConf - cannot open %s File ..!! \n",BONDING_CONF_FILE);
        return -1;
    }

    while (!feof(fp))
    {
        if (fgets(Line,79,fp) == NULL)
        {
            TDBG("EOF reading %s file\n",BONDING_CONF_FILE);
            fclose(fp);
            return 0;
        }

        //a comment will be ignored by scanf
        if( Line[0] == '#') continue;
        sscanf(Line,"bond%hu %s %hu %hu",&Bondindex,Slave,&BondMode,&Interval);

        m_NwBondInfo[Bondindex].Enable = 1;
        m_NwBondInfo[Bondindex].MiiInterval = Interval;
        m_NwBondInfo[Bondindex].BondMode = BondMode;
        pTok=strtok(Slave,",");
        while(pTok != NULL)
        {
            memcpy(&Maxslave[count],pTok,sizeof(pTok));
            count++;
            pTok=strtok(NULL,",");

        }

        for(i=0;i<count;i++)
        {
            index=0;
            sscanf(Maxslave[i],"eth%hu",&index);
            slave=slave | (0x01 << index);
            m_NwCfgInfo[index].Slave =1;
        }
        m_NwBondInfo[Bondindex].Slave = slave;
    }
    fclose(fp);
    return 0;
}

/*
 * @fn CheckIfcLinkStatus
 * @brief This function will check the interface's Link health
 * @param Index [in] index value 
 * @return -1 on failure
 */
int CheckIfcLinkStatus(INT8U Index)
{
    int fd = -1;
    char ifcname[IFNAMSIZ];
    memset(ifcname,0,sizeof(ifcname));

    sprintf(ifcname,"eth%d",Index);

    fd=socket(AF_INET, SOCK_DGRAM, 0);

    if(GetNwLinkStatus(fd,ifcname) == 1)
        return 1;

    close(fd);
    return -1;
}

/*
 * @fn CheckIfcEntry
 * @brief This function will check the interface presence in ifcname table
 * @param Index [in] index value 
 * @param IfcType [in] interface type
 * @return 0 in success, -1 on failure
 */
int CheckIfcEntry(INT8U Index,INT8U IfcType)
{
    int i;
    char IfcName[16],Key[16];

    /*Get the interface Type*/
    if(IfcType == ETH_IFACE_TYPE)
    {
        sprintf(IfcName,"eth");
    }

    GetNoofInterface();

    /*Verify the interface entry in ifcname table*/
    sprintf(Key,"%s%d",IfcName,Index);
    for(i=0;i<sizeof(Ifcnametable)/sizeof(IfcName_T);i++)
    {
        if(strcmp(Ifcnametable[i].Ifcname,Key) == 0)
        {
            break;
        }
    }

    /*Verify Interface Entry*/
    if( i == sizeof(Ifcnametable)/sizeof(IfcName_T))
    {
        TCRIT("Interface is not configured in libnetwork layer\n");
        return -1;
    }

    return 0;
}

/*
 * @fn CheckBondSlave
 * @brief This function will check the given interfaces slave status
 * @param EthIndex[in] interface's Ethindex value
 * @returns 1 if the interface is a slave of any bond interface, otherwise 0
 */
int CheckBondSlave(INT8U EthIndex)
{
    int i;

    if(nwGetBondCfg() != 0)
    {
        TCRIT("Error in getting slave interface's configuration\n");
        return -1;
    }

    for(i = 0; i < sizeof(Ifcnametable)/sizeof(IfcName_T); i++)
    {
        if(Ifcnametable[i].Ifcname[0] == 0)
            continue;

        if(Ifcnametable[i].Index == EthIndex)
        {
            if(m_NwCfgInfo[Ifcnametable[i].Index].Slave == 1)
            {
                TDBG("Given interface is slave of bond interface index %d\n",m_NwCfgInfo[Ifcnametable[i].Index].BondIndex);
                return 1;
            }
            else
            {
                TDBG("Given interface is not a slave of any bond interface\n");
                return 0;
            }
        }
    }

    return 0;
}

/*
 * @fn UpdateBondInterface
 * @brief This function will update the interface file based on bonding Configuration
 * @param Enable [in] Enable/Disable
 * @param BondIndex [in] BondIndex value
 * @param Slaves [in] No of Slaves to be bonded
 * @return 0 in success, -1 on failure
 */
int UpdateBondInterface(INT8U Enable,INT8U BondIndex,INT8U Slaves,INT8U Ethindex)
{

    char key[16];
    int EthIndex = 0,i;

    /*Get the bond Index value*/
    sprintf(key,"bond%d",BondIndex);
    for(i=0;i< sizeof(Ifcnametable)/sizeof(IfcName_T);i++)
    {
        if(strcmp((Ifcnametable[i].Ifcname),key) == 0)
        {
            EthIndex = Ifcnametable[i].Index;
        }
    }

    /*Check the Enable Flag*/
    if(Enable == 1)
    {
        /*Disable all the Slave interfaces*/
        for(i=0;i<(sizeof(Ifcnametable)/sizeof(IfcName_T));i++)
        {

            /*Disable the already enabled slave*/
            if((m_NwCfgInfo[i].Slave == 1) && (m_NwCfgInfo[i].BondIndex == BondIndex))
            {
                m_NwCfgInfo[i].Slave = 0;
                m_NwCfgInfo[i].BondIndex = 0;
                m_NwCfgInfo[i].enable = 1;
                m_NwCfgInfo_v6[i].enable =1;
            }

            if((Slaves >> i) & IFACE_ENABLED && m_NwCfgInfo[i].Slave == 0)
            {
                m_NwCfgInfo[i].enable = 0;
                m_NwCfgInfo_v6[i].enable = 0;
                m_NwCfgInfo[i].Slave=1;
                m_NwCfgInfo[i].BondIndex = BondIndex;
                Ifcnametable[i].Enabled = 0;
            }
        }

        /*Do not update If it is already enabled*/
        if(m_NwCfgInfo[EthIndex].enable != 1)
        {
            /*Copy the Address source of Active Slave*/
            m_NwCfgInfo[EthIndex].CfgMethod = m_NwCfgInfo[Ethindex].CfgMethod;
            m_NwCfgInfo[EthIndex].enable = 1;
            m_NwCfgInfo_v6[EthIndex].enable =0; /*Disable the IPv6 Support now*/
            if(m_NwCfgInfo[EthIndex].CfgMethod == CFGMETHOD_STATIC)
            {

                memcpy(&m_NwCfgInfo[EthIndex].IPAddr,&m_NwCfgInfo[Ethindex].IPAddr,IP_ADDR_LEN);
                memcpy(&m_NwCfgInfo[EthIndex].Mask,&m_NwCfgInfo[Ethindex].Mask,IP_ADDR_LEN);
                memcpy(&m_NwCfgInfo[EthIndex].Gateway,&m_NwCfgInfo[Ethindex].Gateway,IP_ADDR_LEN);
                memcpy(&m_NwCfgInfo[EthIndex].Broadcast,&m_NwCfgInfo[Ethindex].Broadcast,IP_ADDR_LEN);

            }
        }

        /*Update the bonding related configurations*/
        m_NwCfgInfo[EthIndex].Master =1;
        m_NwCfgInfo[EthIndex].BondIndex = BondIndex;
    }
    else
    {

        for(i=0;i<MAX_ETH;i++)
        {
            if((Slaves >> i) & IFACE_ENABLED)
            {
                m_NwCfgInfo[i].Slave=0;
                m_NwCfgInfo[i].BondIndex = 0;
            }
        }

        /*Do not update If already disabled*/
        if (m_NwCfgInfo[EthIndex].enable != 0)
        {
            m_NwCfgInfo[EthIndex].enable = 0;
            m_NwCfgInfo_v6[EthIndex].enable = 0;
        }
        m_NwCfgInfo[EthIndex].Master = 0;
        m_NwCfgInfo[EthIndex].BondIndex = 0;
    }

    nwUpdateInterfaces_v4_v6();
    Write_dhcp6c_conf();
    return 0;
}

/*
 * @fn UpdateBondConf
 * @brief This function will update the Bond.conf file based on bonding configuration
 * @param Enable [in] Enable/Disable
 * @param BondIndex [in] Index value of bond interface
 * @param BondMode [in] Bonding Mode
 * @param MiiInterval [in] Mii Interval
 * @return 0 in success, -1 on failure
 */
int UpdateBondConf(INT8U Enable,INT8U BondIndex, INT8U BondMode,INT16U MiiInterval)
{

    char BondSlaves[64],IName[16],str[64],tmpstr[64];
    int i,j,len;
    FILE *fp;

    memset(str,0,sizeof(str));
    memset(tmpstr,0,sizeof(tmpstr));
    fp = fopen (BONDING_CONF_FILE,"w");
    if(fp ==NULL)
    {
        printf("\n UpdateBondConf - cannot open %s File ..!! \n",BONDING_CONF_FILE);
        return -1;
    }

    /*update the configuration file*/
    fprintf(fp,"#\n#Sample bonding configuration file\n#\n");
    fprintf(fp,"#Format of Entry :\n");
    fprintf(fp,"#Name Slaves Mode MiiMonitor\n#\n");

    if(Enable == 1)
    {
        m_NwBondInfo[BondIndex].Enable = 1;
        m_NwBondInfo[BondIndex].BondMode = BondMode;
        m_NwBondInfo[BondIndex].MiiInterval = MiiInterval;
    }
    else
    {
        m_NwBondInfo[BondIndex].Enable = 0;
        m_NwBondInfo[BondIndex].BondMode = 0;
        m_NwBondInfo[BondIndex].MiiInterval = 0;
    }

    for(i=0;i<MAX_BOND;i++)
    {
        memset(tmpstr,0,sizeof(tmpstr));
        if(m_NwBondInfo[i].Enable != 1)
            continue;

        /*Get the  Slave Interface count*/
        sprintf(IName,"eth");
        for(j=0;j<sizeof(Ifcnametable)/sizeof(IfcName_T);j++)
        {
            if((m_NwCfgInfo[j].Slave == 1) && (m_NwCfgInfo[j].BondIndex == i))
            {
                sprintf(str,"%s%d,",IName,j);
                strcat(tmpstr,str);
            }
        }
        len=strlen(tmpstr )-1;
        memset(BondSlaves,'\0',sizeof(BondSlaves));
        strncpy(BondSlaves,tmpstr,len);

        fprintf(fp,"bond%d %s %d %d\n",i,BondSlaves,m_NwBondInfo[i].BondMode,m_NwBondInfo[i].MiiInterval);
    }

    fclose(fp);
    return 0;
}

/*
 * @fn nwConfigureBonding
 * @brief This function will Enable/Disable the bonding support
 * @param BondCfg [in] Bonding configuration table
 * @param BondIndex [in] Index value of Bond interface to be configured
 * @return 0 on success, -1 on failure
 */
int nwConfigureBonding(BondIface *BondCfg,INT8U EthIndex,int timeoutd_sess_timeout,int global_ipv6)
{
    char IfupdownStr[64],EnslaveIface[64],IName[16],str[64],BondSlaves[64],ifcname[16] = {0};
    int i,j,Ethindex;
    NWCFG6_STRUCT cfg6;
    NWCFG_STRUCT cfg;
    DOMAINCONF      DomainCfg;
    DNSCONF     DNS;
    INT8U               regBMC_FQDN[MAX_CHANNEL];
    INT8U Index=0;
    struct stat       Buf;

    memset(IfupdownStr,'\0',64);
    memset(str,'\0',64);
    memset(BondSlaves,'\0',64);
    memset(&DomainCfg,0,sizeof(DOMAINCONF));
    memset(&DNS,0,sizeof(DNSCONF));
    memset(regBMC_FQDN,0,sizeof(regBMC_FQDN));

    /*Remove the file if it exits*/
    if(0 == stat(ACTIVESLAVE,&Buf))
    {
        unlink(ACTIVESLAVE);
    }

    /*Preserve the interface file configuration when enabling bonding*/
    if(0 != stat(NETWORK_IF_FILE_TMP,&Buf) && BondCfg->Enable == 1)
    {
        if(copyFile(NETWORK_IF_FILE,NETWORK_IF_FILE_TMP) != 0 )
        {
            TINFO("Error in preserving old configurations\n");
        }

        if(copyFile(DNS_CONFIG_FILE,NETWORK_DNS_TMP) != 0 )
        {
            TINFO("Error in preserving dns configurations\n");
        }

    }

    /*Read Active Slave network configuration*/
    nwReadNWCfg_v4_v6(&cfg,& cfg6,EthIndex,global_ipv6);
    nwGetBondCfg();
    /*Read the DNS File*/
    if(ReadDNSConfFile(&DomainCfg,&DNS,regBMC_FQDN) != 0)
    {
        TCRIT("Error  in reading DNS Configuration File\n");
    }

    /*Get the Slave Interfaces */
    sprintf(IName,"eth");
    for(i=0;i<MAX_ETH;i++)
    {
        if((BondCfg->Slaves >> i) & IFACE_ENABLED)
        {
            sprintf(str,"%s%d ",IName,i);
            if(BondCfg->Enable == 1)
            {
                /*down the slave interfaces to be bonded*/
                sprintf(IfupdownStr,"%s %s",IfDown_Ifc,str);
                //                safe_system(IfupdownStr);
            }
            strcat(BondSlaves,str);
        }
    }

    sprintf(str,"bond%d",BondCfg->BondIndex);
    for(i=0;i< sizeof(Ifcnametable)/sizeof(IfcName_T);i++)
    {
        if(Ifcnametable[i].Ifcname[0] == 0)
            continue;

        if(strcmp((Ifcnametable[i].Ifcname),str) == 0)
        {
            Ethindex=Ifcnametable[i].Index;
            if(Ifcnametable[i].Enabled == 1)
            {
                nwReadNWCfg_v4_v6(&cfg,& cfg6,Ifcnametable[i].Index,global_ipv6);
                memset(IfupdownStr,0,sizeof(IfupdownStr));
                if(m_NwCfgInfo[Ifcnametable[i].Index].VLANID)
                {
                    sprintf(IfupdownStr,"%s%d",VLAN_ONLY_IFDOWN,Ifcnametable[i].Index);
                }
                else
                {
                    sprintf(IfupdownStr,"%s bond%d",IfDown_Ifc,BondCfg->BondIndex);
                }
                //                safe_system(IfupdownStr);
            }
            break;
        }
    }

    if(i == (sizeof(Ifcnametable)/sizeof(IfcName_T)))
    {
        TINFO("Bond interface is not Configured. Unable to Enable/Disable the interface\n");
        return -1;
    }

    if(BondCfg->Enable != 1)
    {
        /*Down the Bond Interface*/
        sprintf(IfupdownStr,"%s bond%d ",IfDown_Ifc,BondCfg->BondIndex);
        //        safe_system(IfupdownStr);

        /*Update the interface name for service configuration*/
        if(m_NwCfgInfo[Ifcnametable[i].Index].VLANID)
        {
            sprintf(ifcname,"%s.%d",Ifcnametable[i].Ifcname,m_NwCfgInfo[Ifcnametable[i].Index].VLANID);
        }
        else
        {
            sprintf(ifcname,"%s",Ifcnametable[i].Ifcname);
        }

        /*Disable the VLAN ID*/
        m_NwCfgInfo[Ethindex].VLANID = 0;
    }

    /*Update Bond Configuration into Interface File*/
    UpdateBondInterface(BondCfg->Enable,BondCfg->BondIndex,BondCfg->Slaves,EthIndex);

    /*Update Bond.conf file*/
    UpdateBondConf(BondCfg->Enable,BondCfg->BondIndex,BondCfg->BondMode,BondCfg->MiiInterval);

    if(BondCfg->Enable == 1)
    {
        DomainCfg.EthIndex=Ethindex;
        DomainCfg.v4v6=1;
        DNS.DNSIndex=Ethindex;
        regBMC_FQDN[Ethindex] = regBMC_FQDN[EthIndex];

        if(BondCfg->AutoConf == 1)
        {
            for(Index=0;Index < MAX_SERVICE;Index++)
            {
                get_service_configurations(ModifyServiceNameList[Index],&g_serviceconf);
                if(strcmp(g_serviceconf.InterfaceName,"both") == 0)
                {
                    sprintf(g_serviceconf.InterfaceName,"bond%d",BondCfg->BondIndex);
                    if(set_service_configurations(ModifyServiceNameList[Index],&g_serviceconf,timeoutd_sess_timeout) !=0)
                    {
                        TCRIT("Error in Setting the Configuration for the Requested Service\n");
                    }
                }
                else
                {
                    for(j=0;j < MAX_ETH;j++)
                    {
                        if((BondCfg->Slaves >> j) & IFACE_ENABLED)
                        {
                            sprintf(str,"eth%d",j);
                            if(strcmp((g_serviceconf.InterfaceName),str)==0)
                            {
                                sprintf(g_serviceconf.InterfaceName,"bond%d",BondCfg->BondIndex);
                                if(set_service_configurations(ModifyServiceNameList[Index],&g_serviceconf,timeoutd_sess_timeout) !=0)
                                {
                                    TCRIT("Error in Setting the Configuration for the Requested Service\n");
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

    }
    else
    {
        if(BondCfg->AutoConf == 1)
        {
            for(Index=0;Index < MAX_SERVICE;Index++)
            {
                get_service_configurations(ModifyServiceNameList[Index],&g_serviceconf);
                if(strcmp(g_serviceconf.InterfaceName,ifcname)==0)
                {
                    sprintf(g_serviceconf.InterfaceName,"both");
                    if(set_service_configurations(ModifyServiceNameList[Index],&g_serviceconf,timeoutd_sess_timeout) != 0 )
                    {
                        TCRIT("Error in Setting the service configuration\n");
                    }
                }
            }
        }
        Ifcnametable[Ethindex].Enabled = 0;
    }

    /*Update DNS configuration for bond interface*/
    WriteDNSConfFile(&DomainCfg, &DNS,regBMC_FQDN);

    /*Update the DNS File*/
    if(BondCfg->Enable == 1)
    {

        /*up all the interfaces*/
        for(i = 0; i < sizeof(Ifcnametable)/sizeof(IfcName_T); i++)
        {
            if(Ifcnametable[i].Ifcname[0] == 0)
                continue;

            if(Ifcnametable[i].Enabled != 1)
                continue;

            if( m_NwCfgInfo[Ifcnametable[i].Index].VLANID != 0)
            {
                sprintf(IfupdownStr,"%s%d",VLAN_NETWORK_CONFIG_FILE,Ifcnametable[i].Index);
            }
            else
            {
                sprintf(IfupdownStr,"%s %s",IfUp_Ifc,Ifcnametable[i].Ifcname);
            }
            //            safe_system(IfupdownStr);
        }

        /*detach the slaves*/
        sprintf(EnslaveIface,"%s -d bond%d %s",IfEnslave,BondCfg->BondIndex,BondSlaves);
        //        safe_system(EnslaveIface);

        /*Enslave the current active interface*/
        sprintf(EnslaveIface,"%s -f bond%d %s",IfEnslave,BondCfg->BondIndex,BondSlaves);
        //        safe_system(EnslaveIface);

        /*        if(BondCfg->BondMode == BOND_ACTIVE_BACKUP)
                  {
                  sprintf(EnslaveIface,"%s -c bond%d %s%d",IfEnslave,BondIndex,IName,BondCfg->ActiveSlave);
                  safe_system(EnslaveIface);
                  }*/
    }
    else
    {

        /*Restore the old configuration when bonding is disabled*/
        if(moveFile(NETWORK_IF_FILE_TMP, NETWORK_IF_FILE) != 0)
        {
            TCRIT("Error in copying old configurations\n");
            return -1;
        }

        /*Update the enabled interface status*/
        GetNoofInterface();

        /*Update DNS configuration for enaled interfaces*/
        if(moveFile(NETWORK_DNS_TMP,DNS_CONFIG_FILE) != 0)
        {
            TCRIT("Error in copying dns configurations\n");
        }

        nwReadNWCfg_v4_v6(&cfg,& cfg6,EthIndex,global_ipv6);

        /*Copy the Address source of Active Slave*/
        m_NwCfgInfo[EthIndex].CfgMethod = m_NwCfgInfo[Ethindex].CfgMethod;
        if(m_NwCfgInfo[EthIndex].CfgMethod == CFGMETHOD_STATIC)
        {
            memcpy(&m_NwCfgInfo[EthIndex].IPAddr,&m_NwCfgInfo[Ethindex].IPAddr,IP_ADDR_LEN);
            memcpy(&m_NwCfgInfo[EthIndex].Mask,&m_NwCfgInfo[Ethindex].Mask,IP_ADDR_LEN);
            memcpy(&m_NwCfgInfo[EthIndex].Gateway,&m_NwCfgInfo[Ethindex].Gateway,IP_ADDR_LEN);
            memcpy(&m_NwCfgInfo[EthIndex].Broadcast,&m_NwCfgInfo[Ethindex].Broadcast,IP_ADDR_LEN);
        }

        nwUpdateInterfaces_v4_v6();

        registerBMCstatus(regBMC_FQDN);

        DomainCfg.EthIndex=EthIndex;
        DNS.DNSIndex=EthIndex;

        regBMC_FQDN[EthIndex] = regBMC_FQDN[Ethindex];

        if(DomainCfg.dhcpEnable == 0)
        {
            for(i=0;i<MAX_CHANNEL;i++)
            {
                if((regBMC_FQDN[i] & REG_BMC_FQDN) == REG_BMC_FQDN)
                    regBMC_FQDN[i] |= UNREG_BMC_FQDN;
            }
        }

        /*Update the dhcp6 conf file*/
        Write_dhcp6c_conf();

        /*Update DNS configuration for bond interface*/
        WriteDNSConfFile(&DomainCfg, &DNS,regBMC_FQDN);

        /*Activate all the interface in interface file*/
        for(i=0;i<sizeof(Ifcnametable)/sizeof(IfcName_T);i++)
        {
            if(Ifcnametable[i].Ifcname[0] == 0)
                continue;

            if(nwMakeIFUp(Ifcnametable[i].Index) !=0)
            {
                TCRIT("Error While Activating interface %s\n",Ifcnametable[i].Ifcname);
                return -1;
            }
        }
    }

    /*Restart the service to effect the changes*/
    //    for(i=0;i<MAX_RESTART_SERVICE;i++)
    //    {
    //        safe_system(RestartServices[i]);
    //    }

    return 0;
}

/*
 * @fn nwGetBondConf
 * @brief This function will Get the bonding Configuration of Specified index
 * @param BondCfg [out] Bonding configuration table
 * @param BondIndex [in] Index value of Bond interface 
 * @return 0 on success, -1 on failure
 */
int nwGetBondConf(BondIface *BondCfg,INT8U BondIndex)
{
    int i;
    char Key[64];

    if(BondCfg == NULL)
    {
        TCRIT("NULL value is assigned to bondcfg..\n ");
        return -1;
    }

    memset(BondCfg,0,sizeof(BondIface));
    memset(Key,'\0',64);

    GetNoofInterface();

    /*Get the Bond Interface Enable State*/
    sprintf(Key,"bond%d",BondIndex);
    for(i=0;i<sizeof(Ifcnametable)/sizeof(IfcName_T);i++)
    {
        if(strcmp(Ifcnametable[i].Ifcname,Key) == 0)
        {
            if(Ifcnametable[i].Enabled == 1)
            {
                break;
            }
        }
    }

    if(i == (sizeof(Ifcnametable)/sizeof(IfcName_T)))
    {
        TINFO("Bond interface is not enabled\n");
        return -1;
    }

    nwGetBondCfg();

    /*Update the Bonding Configuraiton*/
    BondCfg->Enable = m_NwBondInfo[BondIndex].Enable;
    BondCfg->BondIndex = BondIndex;
    BondCfg->BondMode = m_NwBondInfo[BondIndex].BondMode;
    BondCfg->MiiInterval = m_NwBondInfo[BondIndex].MiiInterval;
    BondCfg->Slaves        = m_NwBondInfo[BondIndex].Slave;

    return 0;
}

/*
 * @fn UpdateActiveSlave
 * @brief This function is used to update the activeslave file
 * @param Flag to read or write the activeslave configuration
 * @returns 0 on success, -1 on failure
 */
int UpdateActiveSlave(INT8U Flag)
{
    FILE *fp;
    int i,len=0,j,count=0;
    char Slaves[64],IName[16],str[64],tmpstr[64];
    INT8S   Line[255],*pTok=NULL,Maxslave[8][16],Slave[64];
    INT16U  Bondindex = 0,index=0;
    INT8U   slave=0;

    memset(Slaves,0,sizeof(Slaves));
    memset(str,0,sizeof(str));
    memset(tmpstr,0,sizeof(tmpstr));
    memset(Slave,0,sizeof(Slave));
    memset(Maxslave,0,sizeof(Maxslave));

    if (Flag == WRITE_CFG)
    {
        fp = fopen (ACTIVESLAVE,"w");
        if(fp ==NULL)
        {
            TDBG("\n nwActiveSlave - cannot open /conf/activeslave.conf File ..!! \n");
            return -1;
        }

        sprintf(IName,"eth");

        for(j=0;j<MAX_BOND;j++)
        {
            if (m_NwActiveslave[j].Upslave == 0)
                continue;

            for(i=0;i<MAX_ETH;i++)
            {
                if((m_NwActiveslave[j].Upslave >> i) & IFACE_ENABLED)
                {
                    sprintf(str,"%s%d,",IName,i);
                    strcat(tmpstr,str);
                }
            }

            len=strlen(tmpstr )-1;
            memset(Slaves,'\0',sizeof(Slaves));
            strncpy(Slaves,tmpstr,len);

            fprintf(fp,"bond%d %s\n",j,Slaves);
        }
        fclose(fp);
    }
    else
    {
        fp = fopen (ACTIVESLAVE,"r");
        if(fp ==NULL)
        {
            m_NwActiveslave[Bondindex].Upslave = 0x0;
            TDBG("\n nwActiveSlave - cannot open /conf/activeslave.conf File ..!! \n");
            return -1;
        }

        while (!feof(fp))
        {
            if (fgets(Line,79,fp) == NULL)
            {
                TDBG("EOF reading %s file\n",ACTIVESLAVE);
                fclose(fp);
                return 0;
            }

            //a comment will be ignored by scanf
            if( Line[0] == '#') continue;
            sscanf(Line,"bond%hu %s",&Bondindex,Slave);

            pTok=strtok(Slave,",");
            while(pTok != NULL)
            {
                memcpy(&Maxslave[count],pTok,sizeof(pTok));
                count++;
                pTok=strtok(NULL,",");

            }

            for(i=0;i<count;i++)
            {
                index=0;
                sscanf(Maxslave[i],"eth%hu",&index);
                slave=slave | (0x01 << index);
            }
            m_NwActiveslave[Bondindex].Upslave = slave;
        }
        fclose(fp);
    }

    return 0;
}

/*
 * @fn nwActiveSlave
 * @brief This function will active the single slave for the bonding interface
 * @param SlaveIndex to be activated
 * @return 0 on success, -1 on failure
 */
int nwActiveSlave(INT8U BondIndex,INT8U SlaveIndex)
{
    int i;
    char ifconfig[64],str[16],tmpstr[64];
    INT8U Upslave = 0,DownSlave = 0;
    struct stat Buf;


    GetNoofInterface();
    nwGetBondCfg();
    UpdateActiveSlave(READ_CFG);

    memset(ifconfig,0,sizeof(ifconfig));
    memset(str,0,sizeof(str));
    memset(tmpstr,0,sizeof(tmpstr));

    if(m_NwBondInfo[BondIndex].Enable != 1)
    {
        TDBG("Bond interface %d is not Enabled\n",BondIndex);
        return -1;
    }

    if(SlaveIndex == 0xff)
    {
        for(i=0;i<MAX_ETH;i++)
        {
            if((m_NwBondInfo[BondIndex].Slave >> i) & IFACE_ENABLED)
            {
                memset(ifconfig,0,sizeof(ifconfig));
                sprintf(ifconfig,"ifconfig eth%d up",i);
                //                safe_system(ifconfig);
            }
        }

        /*Remove the file if it exits*/
        if(0 == stat(ACTIVESLAVE,&Buf))
        {
            unlink(ACTIVESLAVE);
        }

        /*Remove the old configuration*/
        m_NwActiveslave[BondIndex].Upslave = 0;
        return 0;
    }

    Upslave = SlaveIndex & m_NwBondInfo[BondIndex].Slave;

    DownSlave = Upslave ^ m_NwBondInfo[BondIndex].Slave;

    m_NwActiveslave[BondIndex].Upslave = Upslave;

    if( DownSlave == m_NwBondInfo[BondIndex].Slave )
    {
        TWARN("Can not disable all the slave interfaces.Atleast one interface should be up..\n");
        return -1;
    } 

    if(m_NwBondInfo[BondIndex].BondMode != BOND_ACTIVE_BACKUP)
    {
        /*up the slave interface*/
        for(i=0;i<MAX_ETH;i++)
        {
            memset(ifconfig,0,sizeof(ifconfig));
            if((Upslave>> i) & IFACE_ENABLED)
            {
                sprintf(ifconfig,"ifconfig eth%d up",i);
                //                safe_system(ifconfig);
            }
        }
    }

    if( Upslave == m_NwBondInfo[BondIndex].Slave )
    {
        /*Remove the activeslave file*/
        if( 0 == stat(ACTIVESLAVE,&Buf))
        {
            unlink(ACTIVESLAVE);
        }
        m_NwActiveslave[BondIndex].Upslave = 0x0;
        return 0;
    }

    if(m_NwBondInfo[BondIndex].BondMode == BOND_ACTIVE_BACKUP)
    {
        memset(ifconfig,0,sizeof(ifconfig));
        for(i=0;i<MAX_ETH;i++)
        {
            if((Upslave >> i) & IFACE_ENABLED)
            {
                sprintf(ifconfig,"ifenslave -c bond%d eth%d", BondIndex,i);
                //                safe_system(ifconfig);
                break;
            }
        }
    }
    else
    {
        for(i=0;i<MAX_ETH;i++)
        {
            memset(str,0,sizeof(str));
            memset(ifconfig,0,sizeof(ifconfig));

            if((DownSlave >> i) & IFACE_ENABLED)
            {
                /*down the Slave interfaces*/
                sprintf(ifconfig,"ifconfig eth%d down",i);
                //                safe_system(ifconfig);
            }
        }
    }

    UpdateActiveSlave(WRITE_CFG);
    /*Reset the Slave value*/
    m_NwBondInfo[BondIndex].Slave = 0x0;

    return 0;
}

/*
 * @fn nwGetActiveSlave
 * @brief This function will gets the active interface of specified bondindex
 * @param Bondindex [in] bonding index, Activeindex[out] active slaves
 * @return 0 on success, -1 on failure
 */
int nwGetActiveSlave(INT8U BondIndex,INT8U *ActiveIndex)
{

    GetNoofInterface();
    nwGetBondCfg();
    UpdateActiveSlave(READ_CFG);

    if(m_NwBondInfo[BondIndex].Enable != 1)
    {
        TDBG("Bond interface %d is not Enabled\n",BondIndex);
        *ActiveIndex = 0;
        return -1;
    }

    if(m_NwActiveslave[BondIndex].Upslave == 0x0)
    {
        *ActiveIndex = 0xff;
    }
    else
    {
        *ActiveIndex = m_NwActiveslave[BondIndex].Upslave;
    }

    return 0;
}
/* writes stringified version of ethernet adress at *MAC */
int nwSetMACAddr(char *MAC,int Index)
{
    int r, i=0;
    struct ifreq ifr;
    int skfd;
    unsigned int tmp[MAC_ADDR_LEN];
    INT8S FullEth_Network_if[MAX_STR_LENGTH];

    skfd = socket( PF_INET, SOCK_DGRAM, 0 );
    if ( skfd < 0 )
    {
        TCRIT("can't open socket: %s\n",strerror(errno));
        return -1;
    }

    memset(tmp, 0, MAC_ADDR_LEN);
    memset(&ifr,0,sizeof(ifr));
    memset(FullEth_Network_if,0,MAX_STR_LENGTH);

    sprintf(FullEth_Network_if,"%s%d",Eth_NetWork_If,Index);
    strncpy(ifr.ifr_name, FullEth_Network_if,IFNAMSIZ -1);

    r = ioctl(skfd, SIOCGIFHWADDR, &ifr);
    if ( r < 0 )
    {
        close (skfd);
        TCRIT("Getting MAC Address failed with errorcode:%d\n",r);
        return -1;
    }

    sscanf(MAC,"%02x:%02x:%02x:%02x:%02x:%02x",&tmp[0],&tmp[1],&tmp[2],&tmp[3],&tmp[4],&tmp[5]);
    for (i = 0; i < MAC_ADDR_LEN; i++)
        ifr.ifr_hwaddr.sa_data[i] = tmp[i];

    r = ioctl(skfd, SIOCSIFHWADDR, &ifr);
    if ( r < 0 )
    {
        close (skfd);
        TCRIT("Setting MAC Addr failed with errorcode:%d\n",r);
        return -1;
    }

    (void) close(skfd);

    return 0;
}

/* writes stringified version of ethernet adress at *MAC */
int nwGetMACAddr(char *MAC)
{
    int r;
    struct ifreq ifr;
    int skfd;
    unsigned char tmp[MAC_ADDR_LEN];
    INT8S FullEth_Network_if[MAX_STR_LENGTH];

    skfd = socket( PF_INET, SOCK_DGRAM, 0 );
    if ( skfd < 0 )
    {
        TCRIT("can't open socket: %s\n",strerror(errno));
        return -1;
    }

    memset(tmp, 0, MAC_ADDR_LEN);
    memset(&ifr,0,sizeof(ifr));
    memset(FullEth_Network_if,0,MAX_STR_LENGTH);
    sprintf(FullEth_Network_if,"%s%d",Eth_NetWork_If,0);
    strncpy(ifr.ifr_name, FullEth_Network_if,IFNAMSIZ -1);
    r = ioctl(skfd, SIOCGIFHWADDR, &ifr);
    if ( r < 0 )
    {
        close (skfd);
        TCRIT("Bzzt. No good: %d\n",r);
        return -1;
    }

    memcpy(tmp, ifr.ifr_hwaddr.sa_data, MAC_ADDR_LEN);

    (void) close(skfd);

    sprintf(MAC,"%02X%02X%02X%02X%02X%02X",
            tmp[0],tmp[1],tmp[2],tmp[3],tmp[4],tmp[5] );

    return 0;
}

/* Helps in deleting the existing GW IP */
int nwDelGatewayIP(INT8U* gw,INT32U Destination,INT32U NetMask,INT8U *Interface)
{
    INT8S DelGw[ROUTE_GW_LENGTH];
    INT8S Dest[IP_ADDR_LEN],Mask[IP_ADDR_LEN];

    if(Destination == 0x00)
        sprintf(DelGw,"route del default gw %d.%d.%d.%d %s",gw[0],gw[1],gw[2],gw[3],Interface);
    else
    {
        memcpy(Dest,(INT8S*)&Destination,IP_ADDR_LEN);
        memcpy(Mask,(INT8S*)&NetMask,IP_ADDR_LEN);
        sprintf(DelGw,"route del -net %d.%d.%d.%d netmask %d.%d.%d.%d gw %d.%d.%d.%d %s",Dest[0],Dest[1]
                ,Dest[2],Dest[3],Mask[0],Mask[1],Mask[2],Mask[3],gw[0],gw[1],gw[2],gw[3],Interface);
    }
    //    safe_system(DelGw);
    return 0;
}

/*Deletes the Existing gateway before adding the new Gw in DHCP mode */
int nwDelExistingGateway(INT8U EthIndex)
{
    FILE* routeFp = NULL;
    INT8S FullEth_Network_if[MAX_STR_LENGTH];
    INT8S tempstr[256];
    INT8S FirstTime = 1;
    INT8S iface[16];
    INT8U gw[IP_ADDR_LEN];
    INT32U dest,Mask=0;
    INT32U gway,gwayrepeat=0;
    INT32U flags;
    INT32U t;

    if((routeFp = fopen(PROC_NET_ROUTE_FILE,"r")) == NULL)
    {
        TCRIT("fopen failed for %s\n",PROC_NET_ROUTE_FILE);
        return errno;
    }
    sprintf(FullEth_Network_if,"%s",Ifcnametable[EthIndex].Ifcname);

    /* Get Gateway entry with the flag and interface name */
    while(!feof(routeFp))
    {
        fgets(tempstr,256,routeFp);
        if(FirstTime)
        {
            /* We can simply ignore first line as it just contains the field names */
            FirstTime = 0;
            continue;
        }

        if(strcmp(tempstr,"\0")) // Skip empty lines
        {
            sscanf(tempstr,"%s	%x	%x	%x	%x	%x	%x	%x	%x	%x	%x\n",
                    iface,&dest,&gway,&flags,&t,&t,&t,&Mask,&t,&t,&t);
            /* Default gateway entry has the flag field set to 0x0003 */
            if((flags == DEFAULT_GW_FLAGS) && (strcmp(iface,FullEth_Network_if) == 0) && (gwayrepeat != gway))
            {
                memcpy(gw,(INT8S*)&gway,IP_ADDR_LEN);
                nwDelGatewayIP(gw,dest,Mask,(INT8U*)&FullEth_Network_if[0]);
            }
            gwayrepeat = gway;
        }
    }

    fclose(routeFp);
    return 0;

}


/* Sets the Default Gateway Ip in DHCP mode */
int nwSetGateway(INT8U* GwIP,INT8U EthIndex)
{
    char Gateway[ROUTE_GW_LENGTH];
    INT8S FullEth_Network_if[MAX_STR_LENGTH];
    sprintf(FullEth_Network_if,"%s",Ifcnametable[EthIndex].Ifcname);
    sprintf(Gateway,"route add default gw %d.%d.%d.%d %s",GwIP[0],GwIP[1],GwIP[2],GwIP[3],FullEth_Network_if);
    //    safe_system(Gateway);
    return 0;
}

/* Gives the MAC address */
int nwGetExtMACAddr(unsigned char *MAC)
{
    int r;
    struct ifreq ifr;
    int skfd;
    unsigned char tmp[MAC_ADDR_LEN];
    INT8S FullEth_Network_if[MAX_STR_LENGTH];

    skfd = socket( PF_INET, SOCK_DGRAM, 0 );
    if ( skfd < 0 )
    {
        TCRIT("can't open socket: %s\n",strerror(errno));
        return -1;
    }

    memset(tmp, 0, MAC_ADDR_LEN);
    memset(FullEth_Network_if,0,MAX_STR_LENGTH);
    memset(&ifr,0,sizeof(ifr));
    sprintf(FullEth_Network_if,"%s%d",Eth_NetWork_If,0);
    strncpy(ifr.ifr_name, FullEth_Network_if,IFNAMSIZ - 1);
    r = ioctl(skfd, SIOCGIFHWADDR, &ifr);
    if ( r < 0 )
    {
        close (skfd);
        TCRIT("Bzzt. No good: %d\n",r);
        return -1;
    }

    memcpy(tmp, ifr.ifr_hwaddr.sa_data, MAC_ADDR_LEN);

    (void) close(skfd);
    memcpy(MAC, tmp, MAC_ADDR_LEN);
    return 0;
}


/* ipv6 supported function for getting ipv4 and ipv6 related DNS configurations */
/************************************************************************
Function	: nwGetResolvConf_v4_v6()
brief:	: function used for getting ipv4 and ipv6 related DNS configurations
pamam   : DNS1,DNS2 - ipv4 DNS address		  
pamam   : DNS1v6,DNS2v6 - ipv6 DNS address
pamam   : domain - domain name of the network.
pamam   : domainnamelen - domain name length.
 ************************************************************************/
int nwGetResolvConf_v4_v6(char* DNS1,char*DNS2,char *DNS3, INT8U DNSIPPriority,char* domain,unsigned char* domainnamelen)
{
    //read resolv.conf entries anyways
    FILE* fpresolv;
    char oneline[300];
    char nameserverip[INET6_ADDRSTRLEN];
    char domainval[300];
    int domainfound = 0;
    //start with primary
    int serverno = 1;
    int v4count = 0;
    int v6count = 0;
    char ipAddr[46];

    if(domain != NULL)
    {
        //        GetMacrodefine_string("CONFIG_SPX_FEATURE_GLOBAL_DEFAULT_DOMAINNAME",domain);
        if(domain == NULL)
            strcpy(domain,"Unknown");
    }

    fpresolv = fopen(RESOLV_CONF_FILE,"r");
    if (fpresolv == NULL)
    {
        //no file just return NULL entires
        return 0;
    }
    file_lock_write(fileno(fpresolv));

    //now read resolv.conf file here
    while (!feof(fpresolv))
    {
        if (fgets(oneline,299,fpresolv) == NULL)
        {
            TDBG("EOF reading resolv.conf file\n");
            /*            file_unlock(fileno(fpresolv));
                          fclose(fpresolv);
                          return 0;*/
            break;
        }

        //see if the line has nameserver
        //a comment will be ignored by scanf
        if( oneline[0] == '#') continue;

        if (sscanf(oneline," nameserver  %s",nameserverip) == 1)
        {
            //this is the line!!
            //check if it is a vlaid IP!!
            if( inet_aton(nameserverip,(struct in_addr*)ipAddr) != 0 )
            {
                if (serverno == 1)
                {
                    if(DNS1 != NULL)
                    {
                        DNS1[10] = 0xFF;
                        DNS1[11] = 0xFF;
                        memcpy(&DNS1[IP6_ADDR_LEN - IP_ADDR_LEN],ipAddr,IP_ADDR_LEN);
                    }
                    TDBG("found server 1 and it is %d %d %d %d\n",ipAddr[0],ipAddr[1],ipAddr[2],ipAddr[3]);
                    serverno++;
                    v4count++;
                }
                else if (serverno == 2)
                {
                    if(DNS2 != NULL)
                    {
                        DNS2[10] = 0xFF;
                        DNS2[11] = 0xFF;
                        memcpy(&DNS2[IP6_ADDR_LEN - IP_ADDR_LEN],ipAddr,IP_ADDR_LEN);
                    }
                    TDBG("found server 2 and it is %d %d %d %d\n",ipAddr[0],ipAddr[1],ipAddr[2],ipAddr[3]);
                    serverno++;
                    v4count++;
                    //we are done with dns server names
                }
                else if(serverno == 3)
                {
                    if(DNS3 != NULL)
                    {
                        DNS3[10] = 0xFF;
                        DNS3[11] = 0xFF;
                        memcpy(&DNS3[IP6_ADDR_LEN - IP_ADDR_LEN],ipAddr,IP_ADDR_LEN);
                    }
                    TDBG("found server 3 and it is %d %d %d %d\n",ipAddr[0],ipAddr[1],ipAddr[2],ipAddr[3]);
                    serverno++;
                    v4count++;
                }
            }
            else if( inet_pton (AF_INET6, nameserverip, ipAddr) > 0 )
            {
                TDBG("IPv6 DNS server");
                if (serverno == 1)
                {
                    if(DNS1 != NULL)
                    {
                        memcpy(DNS1,ipAddr,IP6_ADDR_LEN);
                    }
                    TDBG("found ipv6 server 1 and it is %s\n",DNS1);
                    serverno++;
                    v6count++;
                }
                else if (serverno == 2)
                {
                    if(DNS2 != NULL)
                    {
                        memcpy(DNS2,ipAddr,IP6_ADDR_LEN);
                    }
                    TDBG("found ipv6 server 2 and it is %s\n",DNS2);
                    serverno++;
                    v6count++;
                }
                else if(serverno == 3)
                {
                    if(DNS3 != NULL)
                    {
                        memcpy(DNS3,ipAddr,IP6_ADDR_LEN);
                    }
                    serverno++;
                    v6count++;
                }
            }
        }
        else if (sscanf(oneline," search %s",domainval) == 1)
        {
            //we found domain here
            TDBG("found domain\n");
            domainfound = 1;

            /* It might be much reasonable to check length here by our max value instead of user's max value */
            //if (*domainnamelen <= strlen(domainval)+1)
            if (DNSCFG_MAX_DOMAIN_NAME_LEN <= strlen(domainval)+1)
            {
                printf("\n *domainnamelen - %d \n", *domainnamelen);
                printf("\n  strlen(domainval)+1 - %zu \n",  strlen(domainval)+1);
                TWARN("Insufficiient buffer for holding domain name\n");
                //Maybe it shouldn't leave here just because getting domain name error.
                //*domainnamelen = strlen(domainval)+1;
                //file_unlock(fileno(fpresolv));
                //fclose(fpresolv);
                //return -1;
            }
            else
            {
                if(domain != NULL)
                {
                    *domainnamelen = strlen(domainval);        
                    //strncpy(domain,domainval,79);
                    strncpy(domain,domainval,*domainnamelen);
                }
            }
        }

        if (domainfound == 1 && serverno > 3)
        {
            //we found both domain and nameservers
            //printf("\nwe found both domain and nameservers\n");
            break;
        }
    }

    if(DNSIPPriority != 0)
    {
        TDBG("DNSIPPriority %d v4count %d v6count %d\n",DNSIPPriority,v4count,v6count);
        if((DNSIPPriority == 1 && v4count == 1) || (DNSIPPriority == 2 && v6count == 1))
        {
            memcpy(DNS3,DNS2,IP6_ADDR_LEN);
            memset(DNS2,0,IP6_ADDR_LEN);
        }

        if((DNSIPPriority == 1 && v4count == 0) || (DNSIPPriority == 2 && v6count == 0))
        {
            memcpy(DNS3,DNS1,IP6_ADDR_LEN);
            memset(DNS1,0,IP6_ADDR_LEN);
            memset(DNS2,0,IP6_ADDR_LEN);
        }
    }

    file_unlock(fileno(fpresolv));
    fclose(fpresolv);

    return 0;
}


/***************************/
/* DNS Configuration Functions  */
/***************************/
/**
 *@fn nwGetAllDNSConf
 *@brief This function is invoked to Get all the DNS related Configurations
 *@param HostnameConfig - Pointer to Structure used to get hostname configurations
 *@param DomainConfig - Pointer to Structure used to get Domain configurations
 *@param Dnsv4IPConfig - pointer to structure used to get IPv4 DNS server IP Configurations
 *@param Dnsv6IPConfig - pointer to structure used to get IPv6 DNS server IP Configurations
 *@param registerBMC - the array of register BMC flags with MAX LAN channel length 
 *@return Returns 0 on success and -1 on fails
 */
int nwGetAllDNSConf( HOSTNAMECONF *HostnameConfig, DOMAINCONF *DomainConfig, DNSCONF *DnsIPConfig, INT8U *regBMC_FQDN)
{
    int retVal=0;
    int hs;
    char *Domain;
    char *DNS1;
    char *DNS2;
    char *DNS3;
    char hostname[MAX_HOSTNAME_LEN];


    /* Read the dns related configuration from dns.conf file */
    retVal =ReadDNSConfFile(DomainConfig, DnsIPConfig, regBMC_FQDN);
    if( retVal != 0)
    {
        TCRIT(" Failed to Read DNS Configuration File..!!");
    }

    if (HostnameConfig != NULL)
    {
        memset(hostname,0,sizeof(MAX_HOSTNAME_LEN));
        //        GetHostNameConf( hostname, &hs);
        HostnameConfig->HostSetting = hs;
        HostnameConfig->HostNameLen = strlen(hostname);
        strncpy( (char*)HostnameConfig->HostName, hostname, strlen(hostname) );
        TDBG(" Host name - %s \n", HostnameConfig->HostName);
    }
    else
    {
        printf("\n nwGetAllDNSConf - Hostname structure is NULL \n");
    }

    /* Check for NULL data in each parameter that is passed to this function */
    /* Fill NULL values to the respective pointers thats going to be passed to 
       the nwGetResolvConf_v4_v6 which reads resolv.conf file */

    if( DomainConfig != NULL )     //check for NULL
    {
        Domain= (char *) &DomainConfig->domainname;
    }
    else
    {
        printf("\n nwGetAllDNSConf - DomainConfig structure is NULL \n");    
        Domain=NULL;
    }

    if( DnsIPConfig != NULL )     //check for NULL
    {
        DNS1 = (char *) &DnsIPConfig->DNSIP1;
        DNS2 = (char *) &DnsIPConfig->DNSIP2;
        DNS3 = (char *) &DnsIPConfig->DNSIP3;
    }
    else
    {
        DNS1 = NULL;
        DNS2 = NULL;
        DNS3 = NULL;
    }

    /* read the resolv.conf file for domain name and DNS server IP's */
    retVal= nwGetResolvConf_v4_v6(DNS1, DNS2, DNS3, DnsIPConfig->IPPriority,Domain, &DomainConfig->domainnamelen);

    if(retVal != 0)
    {
        TCRIT(" Failed to Get Domain and DNS related information from resolv.conf...!");
    }

    return retVal;
}

/**
 *@fn nwSetAllDNSConf
 *@brief This function is invoked to Set all the DNS related Configurations
 *@param HostnameConfig - Pointer to Structure used to set Hostname configurations
 *@param DomainConfig - Pointer to Structure used to set Domain configurations
 *@param Dnsv4IPConfig - pointer to structure used to set IPv4 DNS server IP Configurations
 *@param Dnsv6IPConfig - pointer to structure used to set IPv6 DNS server IP Configurations
 *@param registerBMC - the array of register BMC flags with MAX LAN channel length 
 *@return Returns 0 on success and -1 on fails
 */
int nwSetAllDNSConf( HOSTNAMECONF *HostnameConfig, DOMAINCONF *DomainConfig, DNSCONF *DnsIPConfig, INT8U *regBMC_FQDN)
{
    int retVal=0;
    char *Domain = NULL;
    INT8U i=0;

    GetNoofInterface();

    for(i=0; i<sizeof(Ifcnametable)/sizeof(IfcName_T); i++)
    {
        if(Ifcnametable[i].Ifcname[0] == 0)
            continue;

        if(Ifcnametable[i].Enabled != 1)
            continue;
        nwMakeIFDown(Ifcnametable[i].Index);
        TDBG(" eth%d DOWN is done........ ",i);    
    }


    /* Check for NULL data in each parameter that is passed to this function */
    /* If Null then Fill NULL values to the respective pointers thats going to be passed to 
       the nwSetResolvConf_v4_v6 which writes resolv.conf file. if a particular 
       data is NULL then that parameter will not be written to the file  */
    /* If parameter is not NULL the copy the corresponding data to the variable */

    /* Hostname Config */
    if( HostnameConfig != NULL )    //check for NULL
    {
        TDBG("\n Host name length - %d \n", strlen((char*) &HostnameConfig->HostName) );
        if( strlen((char*) &HostnameConfig->HostName) > MAX_HOSTNAME_LEN )
        {
            TWARN(" Host name length required to set is exceeded Maximum size of %d.. So Exiting... ", MAX_HOSTNAME_LEN );
        }
        else
        {
            //            retVal = SetHostNameConf( (char*) &HostnameConfig->HostName, (int ) HostnameConfig->HostSetting);
            if(retVal != 0)
            {
                TCRIT("Error while setting hostname \n");
            }
        }
    }
    else
    {
        printf("\n Hostname is NULL\n");
    }

    /* DomainConfig */
    if( DomainConfig != NULL )    //check for NULL
    {
        if(DomainConfig->dhcpEnable ==1)   //check for dhcp option
        {
            memset(DomainConfig->domainname,'\0',DNSCFG_MAX_DOMAIN_NAME_LEN);
            Domain=NULL;
        }
        else
        {
            Domain = (char *) &DomainConfig->domainname;
        }
    }
    else
    {
        Domain=NULL;
    }

    /* DnsIPConfig */
    if( DnsIPConfig != NULL )     //check for NULL
    {
        if(DnsIPConfig->DNSDHCP==1)     //check for dhcp option
        {
            memset(DnsIPConfig->DNSIP1,'\0',IP6_ADDR_LEN);
            memset(DnsIPConfig->DNSIP2,'\0',IP6_ADDR_LEN);
            memset(DnsIPConfig->DNSIP3,'\0',IP6_ADDR_LEN);
        }
    }

    retVal = WriteDNSConfFile( DomainConfig, DnsIPConfig, regBMC_FQDN);
    if(retVal != 0)
    {
        TCRIT("Error in update dns.conf file\n");
    }

    retVal= nwSetResolvConf_v4_v6((char *)&DnsIPConfig->DNSIP1,(char *)&DnsIPConfig->DNSIP2,(char *)&DnsIPConfig->DNSIP3,Domain);
    if(retVal != 0)
    {
        TCRIT("Error writing resolv.conf File..");
    }

    GetNoofInterface();

    for(i=0; i<sizeof(Ifcnametable)/sizeof(IfcName_T); i++)
    {
        if(Ifcnametable[i].Ifcname[0] == 0)
            continue;

        if(Ifcnametable[i].Enabled != 1)
            continue;
        nwMakeIFUp(Ifcnametable[i].Index);
        TDBG(" eth%d up is done........ ",i);    
    }

    /* While restarting both the interfaces, the first interface (eth0) 
       will not be registered properly to DNS, because of any one of the 
       parameter (Domain name or DNS server IP) is missing at that time.
       so just restart the first interface again to register with the DNS.

       nwMakeIFDown(0);
       TDBG(" eth0 DOWN is done........ ");
       nwMakeIFUp(0);
       TDBG(" eth0 restart is done........ ");*/

    return retVal;
}


/* function to read leading white spaces */
/**
 *@fn strskip
 *@brief function to read leading white spaces 
 *@param s - Pointer to char which holds the flag string
 *@return Returns the address of first non-whitespace character in the string
 */
char * strskip(char * s)
{
    char * skip = s;
    if (s==NULL) return NULL ;
    //while (isspace((int)*skip) && *skip) skip++;
    while (isspace((int)*skip)) skip++;
    return skip ;
}

/**
 *@fn registerBMCstatus
 *@brief This function is invoked to Get the BMC register status with the DNS server.
 *@param registerBMC - the array of register BMC flags with MAX LAN channel length 
 *@return Returns 0 on success and -1 on fails
 */
int registerBMCstatus ( INT8U *regBMC_FQDN )
{
    FILE *fp;
    char oneline[80];
    char ch[10];
    int i=0;
    char iface[10];

    GetNoofInterface();

    fp = fopen (CONFDNSCONF,"r");
    if(fp ==NULL)
    {
        printf("\n cannot open %s File ..!! \n",CONFDNSCONF);
        return -1;
    }
    while (!feof(fp))
    {
        if (fgets(oneline,79,fp) == NULL)
        {
            TDBG("EOF reading %s file\n",CONFDNSCONF);
            fclose(fp);
            return 0;
        }

        //a comment will be ignored by scanf
        if( oneline[0] == '#') continue;

        /* Read the DNS configurations by Interface wise */
        for(i=0;i<sizeof(Ifcnametable)/sizeof(IfcName_T);i++)
        {

            if(Ifcnametable[i].Ifcname[0] == 0)
                continue;

            if(Ifcnametable[i].Enabled != 1)
                continue;

            sprintf(iface,"%s",Ifcnametable[i].Ifcname);

            // After finding the entry for one interface, try reading the configutaions for that iface
            if(strstr(oneline,iface)!= NULL)
            {      
                //printf("\n %s Entry Found..!! \n",iface);
                while (!feof(fp))
                {
                    char *ptr;
                    if (fgets(oneline,79,fp) != NULL)
                    {
                        if(strstr(oneline,";;")!= NULL)
                        {
                            //printf("\n Reading all entries done..!! \n");
                            //End of the iface configuration is reached.. just braek
                            break;
                        }

                        /* Search for DO_DDNS flag */
                        if((strstr(oneline,"DO_DDNS"))!= NULL)
                        {
                            ptr=strskip(oneline);
                            //scan for the flag value
                            sscanf(ptr,"DO_DDNS=%s",ch);

                            if((regBMC_FQDN!= NULL) && (i<MAX_CHANNEL))
                            {
                                if( (strcmp(ch,"y")==0) || (strcmp(ch,"yes")==0) )
                                {
                                    regBMC_FQDN[Ifcnametable[i].Index]=1;
                                }
                                else
                                {
                                    regBMC_FQDN[Ifcnametable[i].Index]=0;
                                }
                            }
                        }
                        /* Search for SET_FQDN flag */
                        if((strstr(oneline,"SET_FQDN"))!= NULL)
                        {
                            ptr=strskip(oneline);
                            //scan for the flag value
                            sscanf(ptr,"SET_FQDN=%s",ch);

                            if((regBMC_FQDN!= NULL) && (i<MAX_CHANNEL))
                            {
                                if( (strcmp(ch,"y")==0) || (strcmp(ch,"yes")==0) )
                                {
                                    regBMC_FQDN[Ifcnametable[i].Index] |= REG_BMC_FQDN;
                                }
                                else
                                {
                                    regBMC_FQDN[Ifcnametable[i].Index] |= UNREG_BMC_FQDN;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    fclose(fp);
    return 0;
}

/**
 *@fn ReadDNSConfFile
 *@brief This function is invoked to Get all the DNS related Configurations from dns.conf file
 *@param DomainConfig - Pointer to Structure used to read Domain configurations
 *@param DnsIPConfig - pointer to structure used to read DNS server IP Configurations
 *@param regBMC_FQDN - the array of register BMC and FQDN method flags with MAX LAN channel length 
 *@return Returns 0 on success and -1 on fails
 */
int ReadDNSConfFile ( DOMAINCONF *DomainConfig, DNSCONF *DnsIPConfig, INT8U *regBMC_FQDN )
{
    FILE *fp;
    char oneline[80];
    char ch[10];
    int i=0;
    char iface[10];
    INT8U DomainFound=0;

    GetNoofInterface();

    fp = fopen (CONFDNSCONF,"r");
    if(fp ==NULL)
    {
        printf("\n ReadDNSConfFile - cannot open %s File ..!! \n",CONFDNSCONF);
        return -1;
    }
    while (!feof(fp))
    {
        if (fgets(oneline,79,fp) == NULL)
        {
            TDBG("EOF reading %s file\n",CONFDNSCONF);
            fclose(fp);
            return 0;
        }

        //a comment will be ignored by scanf
        if( oneline[0] == '#') continue;

        /* Read the DNS configurations by Interface wise */
        for(i=0;i<sizeof(Ifcnametable)/sizeof(IfcName_T);i++)
        {
            if(Ifcnametable[i].Ifcname[0] == 0)
                continue;

            if(Ifcnametable[i].Enabled != 1)
                continue;

            if(m_NwCfgInfo[Ifcnametable[i].Index].VLANID)
                sprintf(iface,"%s.%d",Ifcnametable[i].Ifcname,m_NwCfgInfo[Ifcnametable[i].Index].VLANID);
            else
                sprintf(iface,"%s",Ifcnametable[i].Ifcname);

            // After finding the entry for one interface, try reading the configutaions for that iface
            if(strstr(oneline,iface)!= NULL)
            {      
                //printf("\n %s Entry Found..!! \n",iface);
                while (!feof(fp))
                {
                    char *ptr;
                    if (fgets(oneline,79,fp) != NULL)
                    {
                        if(strstr(oneline,";;")!= NULL)
                        {
                            //printf("\n Reading all entries done..!! \n");
                            //End of the iface configuration is reached.. just braek
                            break;
                        }

                        /* Search for DO_DDNS flag */
                        if((strstr(oneline,"DO_DDNS"))!= NULL)
                        {
                            ptr=strskip(oneline);
                            //scan for the flag value
                            sscanf(ptr,"DO_DDNS=%s",ch);

                            if((regBMC_FQDN != NULL) && (i<MAX_CHANNEL))
                            {
                                if( (strcmp(ch,"y")==0) || (strcmp(ch,"yes")==0) )
                                {
                                    regBMC_FQDN[Ifcnametable[i].Index]=1;
                                }
                                else
                                {
                                    regBMC_FQDN[Ifcnametable[i].Index]=0;
                                }
                            }
                        }
                        /* Search for SET_FQDN flag */
                        if((strstr(oneline,"SET_FQDN"))!= NULL)
                        {
                            ptr=strskip(oneline);
                            //scan for the flag value
                            sscanf(ptr,"SET_FQDN=%s",ch);

                            if((regBMC_FQDN != NULL) && (i<MAX_CHANNEL))
                            {
                                if( (strcmp(ch,"y")==0) || (strcmp(ch,"yes")==0) )
                                {
                                    regBMC_FQDN[Ifcnametable[i].Index] |= REG_BMC_FQDN;
                                }
                                else
                                {
                                    regBMC_FQDN[Ifcnametable[i].Index] |= UNREG_BMC_FQDN;
                                }
                            }
                        }

                        /* Search for SET_DNS flag */
                        else if((strstr(oneline,"SET_DNS"))!= NULL)
                        {
                            ptr=strskip(oneline);
                            //scan for the flag value
                            sscanf(ptr,"SET_DNS=%s",ch);

                            if( DnsIPConfig != NULL )
                            {
                                if( (strcmp(ch,"y")==0) || (strcmp(ch,"yes")==0) )
                                {
                                    // DNS is dhcp
                                    DnsIPConfig->DNSDHCP=1;
                                    DnsIPConfig->DNSIndex=Ifcnametable[i].Index;
                                }
                                else
                                {
                                    if(DnsIPConfig->DNSDHCP != 1)  // If Its already marked as dhcp dont change it.
                                    {
                                        DnsIPConfig->DNSDHCP=0;
                                        DnsIPConfig->DNSIndex=0;
                                    }
                                }
                            }
                        }

                        /* Search for SET_DOMAIN flag */
                        else if(strstr(oneline,"SET_DOMAIN")!=NULL)
                        {
                            ptr=strskip(oneline);
                            //scan for the flag value
                            sscanf(ptr,"SET_DOMAIN=%s",ch);

                            if( DomainConfig != NULL)
                            {
                                if( (strcmp(ch,"y")==0) || (strcmp(ch,"yes")==0) )
                                {
                                    // Domain name is from dhcp
                                    DomainConfig->dhcpEnable=1;
                                    DomainConfig->EthIndex=Ifcnametable[i].Index;
                                    DomainConfig->v4v6=1;
                                    DomainFound=1;
                                }
                                else
                                {
                                    if( DomainFound != 1)    // If Its already marked as dhcp dont change it.
                                    {
                                        DomainConfig->dhcpEnable=0;
                                        DomainConfig->EthIndex=0;
                                        DomainConfig->v4v6=0;
                                    }
                                }
                            }
                        }

                        /* Search for SET_v6DNS flag */
                        else if(strstr(oneline,"SET_IPV6_PRIORITY")!=NULL)
                        {
                            ptr=strskip(oneline);
                            //scan for the flag value
                            sscanf(ptr,"SET_IPV6_PRIORITY=%s",ch);

                            if( DnsIPConfig != NULL )
                            {
                                if(DnsIPConfig->DNSIndex == Ifcnametable[i].Index)
                                {
                                    if( (strcmp(ch,"y")==0) || (strcmp(ch,"yes")==0))
                                    {
                                        // DNS v6 is dhcp
                                        DnsIPConfig->IPPriority = 2;
                                    }
                                    else
                                    {
                                        if(DnsIPConfig->DNSDHCP == 1)
                                        {
                                            DnsIPConfig->IPPriority = 1;
                                        }
                                        else
                                        {
                                            DnsIPConfig->IPPriority = 0;
                                        }
                                    }
                                }
                            }
                        }

                        /* Search for SET_v6DOMAIN flag */
                        else if(strstr(oneline,"SET_v6DOMAIN")!=NULL)
                        {
                            ptr=strskip(oneline);
                            //scan for the flag value
                            sscanf(ptr,"SET_v6DOMAIN=%s",ch);

                            if( DomainConfig != NULL )
                            {
                                if( (strcmp(ch,"y")==0) || (strcmp(ch,"yes")==0) )
                                {
                                    // DOmain name is from dhcp
                                    DomainConfig->dhcpEnable=1;
                                    DomainConfig->EthIndex=Ifcnametable[i].Index;
                                    DomainConfig->v4v6=2;
                                    DomainFound=1;
                                }
                                else
                                {
                                    if( DomainFound != 1) // If Its already marked as dhcp dont change it.
                                    {
                                        DomainConfig->dhcpEnable=0;
                                        DomainConfig->EthIndex=0;
                                        DomainConfig->v4v6=0;
                                    }
                                }
                            }
                        }

                    }
                }
            }
        }
    }

    fclose(fp);
    return 0;
}


/**
 *@fn WriteDNSConfFile
 *@brief This function is invoked to Write all the DNS related Configurations to dns.conf file
 *@param DomainConfig - Pointer to Structure used to write Domain configurations
 *@param Dnsv4IPConfigr - pointer to structure used to write IPv4 DNS server IP Configurations
 *@param Dnsv6IPConfigr - pointer to structure used to write IPv6 DNS server IP Configurations
 *@param registerBMC - the array of register BMC flags with MAX LAN channel length 
 *@return Returns 0 on success and -1 on fails
 */
int WriteDNSConfFile ( DOMAINCONF *DomainConfig, DNSCONF *DnsIPConfig, INT8U *regBMC_FQDN)
{
    FILE *fp;
    int i=0,ret=0;
    char yes[5]="y";
    char no[5]="n";

    fp = fopen (CONFDNSCONF_TMP,"w");
    if(fp ==NULL)
    {
        TCRIT(" WriteDNSConfFile - cannot open %s File ..!! \n",CONFDNSCONF_TMP);
        return -1;   
    }

    // Read total no.of Interfaces available
    GetNoofInterface();

    /* start writing the dns.conf file contents */

    /* Initialize the script variables first */
    fprintf(fp,"\n#!/bin/sh\n\n");

    fprintf(fp," DO_DDNS=%s\n",no);
    fprintf(fp," SET_FQDN=%s\n",no);
    fprintf(fp," SET_DOMAIN=%s\n",no);
    fprintf(fp," SET_v6DOMAIN=%s\n",no);
    fprintf(fp," SET_DNS=%s\n",no);
    fprintf(fp," SET_IPV6_PRIORITY=%s\n",no);

    fprintf(fp,"\ncase ${IFACE} in\n\n");

    for(i=0;i<sizeof(Ifcnametable)/sizeof(IfcName_T);i++)
    {
        if (Ifcnametable[i].Enabled != 1)
            continue;

        if(Ifcnametable[i].Ifcname[0] == 0)
            continue;

        /* write the configuration detailes interface wise */
        if(m_NwCfgInfo[Ifcnametable[i].Index].VLANID)
            fprintf(fp,"\t%s.%d)\n",Ifcnametable[i].Ifcname,m_NwCfgInfo[Ifcnametable[i].Index].VLANID);
        else
            fprintf(fp,"\t%s)\n",Ifcnametable[i].Ifcname);

        /*
           Set All the flag values interface-wise in dns.conf file according 
           to the values given as input
         */

        /* HOST NAME register start*/
        if((regBMC_FQDN!= NULL) && (i<MAX_CHANNEL))
        {
            if((regBMC_FQDN[Ifcnametable[i].Index] & 0x01) == 0x00)
            {
                fprintf(fp,"\t\tDO_DDNS=%s\n",no);
            }
            else if ((regBMC_FQDN[Ifcnametable[i].Index] & 0x01) == 0x01)
            {
                fprintf(fp,"\t\tDO_DDNS=%s\n",yes);
            }
        }
        else
        {
            fprintf(fp,"\t\tDO_DDNS=%s\n",no);
        }
        /* HOST NAME register done */

        /* FQDN register start*/
        if((regBMC_FQDN != NULL) && (i<MAX_CHANNEL))
        {
            if((regBMC_FQDN[Ifcnametable[i].Index] & REG_BMC_FQDN) == UNREG_BMC_FQDN)
            {
                fprintf(fp,"\t\tSET_FQDN=%s\n",no);
            }
            else if ((regBMC_FQDN[Ifcnametable[i].Index] & REG_BMC_FQDN) == REG_BMC_FQDN)
            {
                fprintf(fp,"\t\tSET_FQDN=%s\n",yes);
            }
        }
        else
        {
            fprintf(fp,"\t\tSET_FQDN=%s\n",no);
        }

        /* DOMAIN NAME */
        if( DomainConfig != NULL)
        {
            if(DomainConfig->dhcpEnable == 0)
            {
                fprintf(fp,"\t\tSET_DOMAIN=%s\n",no);
                fprintf(fp,"\t\tSET_v6DOMAIN=%s\n",no);
            }
            else if (DomainConfig->dhcpEnable == 1)
            {
                if(DomainConfig->EthIndex == Ifcnametable[i].Index)
                {
                    if(DomainConfig->v4v6 == 1)
                    {
                        fprintf(fp,"\t\tSET_DOMAIN=%s\n",yes);
                        fprintf(fp,"\t\tSET_v6DOMAIN=%s\n",no);
                    }
                    else if(DomainConfig->v4v6 == 2)
                    {
                        fprintf(fp,"\t\tSET_DOMAIN=%s\n",no);
                        fprintf(fp,"\t\tSET_v6DOMAIN=%s\n",yes);
                    }
                    else
                    {
                        fprintf(fp,"\t\tSET_DOMAIN=%s\n",no);
                        fprintf(fp,"\t\tSET_v6DOMAIN=%s\n",no);
                    }
                }
                else
                {
                    fprintf(fp,"\t\tSET_DOMAIN=%s\n",no);
                    fprintf(fp,"\t\tSET_v6DOMAIN=%s\n",no);
                }
            }
        }
        else
        {
            fprintf(fp,"\t\tSET_DOMAIN=%s\n",no);
            fprintf(fp,"\t\tSET_v6DOMAIN=%s\n",no);
        }
        /* DOMAIN NAME done */

        /* DNS Server IP */
        if( DnsIPConfig != NULL )
        {
            if(DnsIPConfig->DNSDHCP== 0)
            {
                fprintf(fp,"\t\tSET_DNS=%s\n",no);
            }
            else if(DnsIPConfig->DNSDHCP== 1)
            {
                if(DnsIPConfig->DNSIndex == Ifcnametable[i].Index)
                {
                    fprintf(fp,"\t\tSET_DNS=%s\n",yes); 
                }
                else
                {
                    fprintf(fp,"\t\tSET_DNS=%s\n",no); 
                }
            }
        }
        else
        {
            fprintf(fp,"\t\tSET_DNS=%s\n",no); 
        }
        /* DNS v4 Server IP setting done here */

        /* DNS v6 Server IP */
        if( DnsIPConfig != NULL)
        {
            if(DnsIPConfig->DNSDHCP == 0)
            {
                fprintf(fp,"\t\tSET_IPV6_PRIORITY=%s\n",no);
            }
            else if(DnsIPConfig->DNSDHCP == 1)
            {
                if(DnsIPConfig->IPPriority == 2 && DnsIPConfig->DNSIndex == Ifcnametable[i].Index)
                {
                    fprintf(fp,"\t\tSET_IPV6_PRIORITY=%s\n",yes); 
                }
                else
                {
                    fprintf(fp,"\t\tSET_IPV6_PRIORITY=%s\n",no);
                }
            }
        }
        else
        {
            fprintf(fp,"\t\tSET_IPV6_PRIORITY=%s\n",no); 
        }
        /* DNS v4 Server IP setting done here */

        fprintf(fp,"\t\t;; \n");
    }

    fprintf(fp,"\t*) \n");
    fprintf(fp,"\t;; \n");
    fprintf(fp,"esac\n");

    fclose(fp);

    ret = moveFile(CONFDNSCONF_TMP,CONFDNSCONF);
    if(ret != 0)	//Replaced system calls
    {
        TCRIT("Error moving %s config to %sconfig\n", CONFDNSCONF_TMP, CONFDNSCONF );
        return -1;
    }

    return 0;
}


/*********************************************************************/
/* Network Resolv.conf Set for both IPv4 and IPv6 DNS server address */
/*********************************************************************/
/**
 *@fn nwSetResolvConf_v4_v6
 *@brief This function is used to Write Domain name and DNS server IP's for Both IPv4 and IPv6 to resolv.conf file
 *@param dns1 - Pointer to char that holds first IPv4 DNS server IP
 *@param dns2 - Pointer to char that holds second IPv4 DNS server IP
 *@param dns3 - Pointer to char that holds first IPv6 DNS server IP
 *@return Returns 0 on success and -1 on fails
 */
int nwSetResolvConf_v4_v6(char* dns1,char* dns2,char* dns3,char* domain)
{
    FILE* fpout;
    int namect = 0;
    char oneline[ONELINE_LEN];
    int ret = 0;
    INT8U   IPv4Addr[IP_ADDR_LEN];
    char   DNSAddr[INET6_ADDRSTRLEN];

    /* Remove all the old entries from resolv.conf and write new entries */
    fpout = fopen(RESOLV_CONF_FILE_TEMP,"w");
    if(fpout == NULL)
    {
        TCRIT("Errore opening temp resolv conf for writing\n");
        return -1;
    }

    /* Write the Domain name first */
    /* if domain name is NULL or not present just ignore */
    if ((domain != NULL) && (strcmp(domain,"") != 0))
    {
        TDBG("domain name is given as input");
        sprintf(oneline,"search %s\n",domain);
        fputs(oneline,fpout);
    }
    else
    {
        TDBG("No domain information present.. error in setting domain name");
        strncpy(oneline,"",1);
    }

    /* write All the DNS server IP's. If the server IP is NULL just ignore it */
    if(namect == 0)
    {
        if(dns1 != NULL)
        {
            if(IN6_IS_ADDR_V4MAPPED(dns1))
            {
                memset(IPv4Addr,0,IP_ADDR_LEN);
                memcpy(IPv4Addr,&dns1[IP6_ADDR_LEN - IP_ADDR_LEN],IP_ADDR_LEN);
                memset(DNSAddr,0,INET6_ADDRSTRLEN);
                if(inet_ntop(AF_INET,IPv4Addr,(char*)DNSAddr,INET_ADDRSTRLEN) == NULL)
                {
                    printf("Invalid DNS Address\n");
                }
                else if(strcmp(DNSAddr,"0.0.0.0") != 0)
                {
                    sprintf(oneline,"nameserver %s\n",DNSAddr);
                    fputs(oneline,fpout);
                }
            }
            else
            {
                if(inet_ntop(AF_INET6,(char*)dns1,(char*)DNSAddr,INET6_ADDRSTRLEN) == NULL)
                {
                    printf("Invalid DNS IPv6 Address");
                }
                else if(strcmp(DNSAddr,"::") != 0)
                {
                    sprintf(oneline,"nameserver %s\n",DNSAddr);
                    fputs(oneline,fpout);
                }
            }
        }
        else
        {
            strncpy(oneline,"",1); //clear if dns1 name is NULL
        }
        namect++;
    }

    if(namect == 1)
    {
        //only if dns 2 is not null change it
        if(dns2 != NULL)
        {
            if(IN6_IS_ADDR_V4MAPPED(dns2))
            {
                memset(IPv4Addr,0,IP_ADDR_LEN);
                memcpy(IPv4Addr,&dns2[IP6_ADDR_LEN - IP_ADDR_LEN],IP_ADDR_LEN);
                memset(DNSAddr,0,INET6_ADDRSTRLEN);
                if(inet_ntop(AF_INET,IPv4Addr,(char*)DNSAddr,INET_ADDRSTRLEN) == NULL)
                {
                    printf("Invalid DNS Address\n");
                }
                else if(strcmp(DNSAddr,"0.0.0.0") != 0)
                {
                    sprintf(oneline,"nameserver %s\n",DNSAddr);
                    fputs(oneline,fpout);
                }
            }
            else
            {
                if(inet_ntop(AF_INET6,(char*)dns2,(char*)DNSAddr,INET6_ADDRSTRLEN) == NULL)
                {
                    printf("Invalid DNS IPv6 Address");
                }
                else if(strcmp(DNSAddr,"::") != 0)
                {
                    sprintf(oneline,"nameserver %s\n",DNSAddr);
                    fputs(oneline,fpout);
                }
            }

        }
        else
        {
            strncpy(oneline,"",1); //clear if dns2 name is NULL
        }
        namect++;
    }

    if(namect == 2)
    {
        if(dns3 != NULL)
        {
            if(IN6_IS_ADDR_V4MAPPED(dns3))
            {
                memset(IPv4Addr,0,IP_ADDR_LEN);
                memcpy(IPv4Addr,&dns3[IP6_ADDR_LEN - IP_ADDR_LEN],IP_ADDR_LEN);
                memset(DNSAddr,0,INET6_ADDRSTRLEN);
                if(inet_ntop(AF_INET,IPv4Addr,(char*)DNSAddr,INET_ADDRSTRLEN) == NULL)
                {
                    printf("Invalid DNS Address\n");
                }
                else if(strcmp(DNSAddr,"0.0.0.0") != 0)
                {
                    sprintf(oneline,"nameserver %s\n",DNSAddr);
                    fputs(oneline,fpout);
                }
            }
            else
            {
                if(inet_ntop(AF_INET6,dns3,(char*)DNSAddr,INET6_ADDRSTRLEN) == NULL)
                {
                    printf("Invalid DNS IPv6 Address");
                }
                else if(strcmp(DNSAddr,"::") != 0)
                {
                    sprintf(oneline,"nameserver %s\n",DNSAddr);
                    fputs(oneline,fpout);
                }
            }
        }
        else
        {
            strncpy(oneline,"",1); //clear if dnsv6_1 name is NULL
        }
        namect++;
    }

    fclose(fpout);

    ret = moveFile(RESOLV_CONF_FILE_TEMP,RESOLV_CONF_FILE);
    if(ret != 0)	//Replaced system calls
    {
        TCRIT("Error moving tmp resolv config to resolv config\n");
        return -1;
    }

    return 0;
}


/************************************************************************
Function	: CheckInterfacePresence()
brief:	: function used to check interface status
pamam   : Ifc - interface name to be checked.	
return     : returns 0 on success -1 on fail.
 ************************************************************************/
    int
CheckInterfacePresence (char * Ifc)
{
    int r;
    int skfd;
    struct ifreq ifr;
    unsigned char tmp[MAC_ADDR_LEN];

    //printf ("Checking the presence of %s\n", Ifc);

    skfd = socket(PF_INET, SOCK_DGRAM, 0 );
    if ( skfd < 0 )
    {
        printf("can't open socket: %s\n",strerror(errno));
        return -1;
    }

    /* Get MAC address */
    memset(&ifr,0,sizeof(struct ifreq));
    memset(tmp, 0, MAC_ADDR_LEN);
    strcpy(ifr.ifr_name, Ifc);
    ifr.ifr_hwaddr.sa_family = AF_INET;
    r = ioctl(skfd, SIOCGIFHWADDR, &ifr);
    close (skfd);
    if ( r < 0 )
    {
        //printf("IOCTL to get MAC failed: %d\n",r);
        return -1;
    }
    //printf (" %s Interface is present\n", Ifc);

    return 0;
}

void GetNwCfgInfo()
{
    int i=0;
    char buf[MAX_STR_LENGTH];
    INT16U VLANID[MAX_CHANNEL]={0};

    if(ReadVLANFile(VLAN_ID_SETTING_STR, VLANID) == -1)
    {
        return;
    }

    for(i=0;i<MAX_CHANNEL;i++)
    {
        m_NwCfgInfo[Ifcnametable[i].Index].VLANID=VLANID[Ifcnametable[i].Index];
    }

    for(i=0;i<sizeof(Ifcnametable)/sizeof(IfcName_T);i++)
    {

        if(Ifcnametable[i].Ifcname[0] == 0)
            continue;

        if(Ifcnametable[i].Enabled != 1)
            continue;

        if(m_NwCfgInfo[Ifcnametable[i].Index].VLANID)
        {
            m_LANIndex=0;
            memset(m_VLANInterface,0,MAX_STR_LENGTH);
            sprintf(m_VLANInterface,"%s.%d",Ifcnametable[i].Ifcname,m_NwCfgInfo[Ifcnametable[i].Index].VLANID);
            sprintf(buf,"%s",m_VLANInterface);
            if(CheckInterfacePresence(buf)==0)
            {
                nwGetNWInformation(&m_NwCfgInfo[Ifcnametable[i].Index],Ifcnametable[i].Index);
            }
            else
            {
                sprintf(buf,"eth%d",i);
                nwGetNWInformation(&m_NwCfgInfo[Ifcnametable[i].Index],Ifcnametable[i].Index);
            }
        }
        else
        {
            m_LANIndex=1;
            sprintf(buf,"%s",Ifcnametable[i].Ifcname);
            if(CheckInterfacePresence(buf)==0)
            {
                nwGetNWInformation(&m_NwCfgInfo[Ifcnametable[i].Index],Ifcnametable[i].Index);
            }
        }

    }
}



#define SOFT_CHECK

int nwGetEthInformation(ETHCFG_STRUCT *ethcfg, char * IFName)
{
#ifndef SOFT_CHECK
    int r;
#endif
    struct ifreq ifr;
    int skfd;
    struct ethtool_cmd ecmd;
    struct ethtool_wolinfo wol;
    int nReturn = -1;

    skfd = socket(PF_INET, SOCK_DGRAM, 0 );
    if ( skfd < 0 )
    {
        TCRIT("can't open socket: %s\n",strerror(errno));
        return nReturn;
    }

    memset(&ifr,0,sizeof(struct ifreq));
    memset(&ecmd,0,sizeof(struct ethtool_cmd));

    strcpy(ifr.ifr_name, IFName);

    ecmd.cmd = ETHTOOL_GSET;
    ifr.ifr_data = (char *)&ecmd;
#ifndef SOFT_CHECK
    r = ioctl(skfd, SIOCETHTOOL, &ifr);
#else
    ioctl(skfd, SIOCETHTOOL, &ifr);
#endif

#ifndef SOFT_CHECK
    if (r < 0) {
        nReturn = -1;
        TCRIT("IOCTL to get ETHCFG failed: %d\n",r);
    }
    else
#endif
    {
        ethcfg->speed = ecmd.speed;
        ethcfg->duplex = ecmd.duplex;
        ethcfg->autoneg = ecmd.autoneg;
        ethcfg->supported = ecmd.supported;
        nReturn = 0;
    }

#ifndef SOFT_CHECK
    r = ioctl(skfd, SIOCGIFMTU, &ifr);
#else
    ioctl(skfd, SIOCGIFMTU, &ifr);
#endif

#ifndef SOFT_CHECK
    if (r < 0) {
        nReturn = -1;
        TCRIT("IOCTL to get ETHCFG MTU failed: %d\n",r);
    }
    else
#endif
    {
        ethcfg->maxtxpkt = ifr.ifr_mtu;

        nReturn = 0;
    }

    wol.cmd = ETHTOOL_GWOL;
    ifr.ifr_data = (caddr_t)&wol;

#ifndef SOFT_CHECK
    r = ioctl(skfd, SIOCETHTOOL, &ifr);
#else
    ioctl(skfd, SIOCETHTOOL, &ifr);
#endif
#ifndef SOFT_CHECK
    if (r < 0) {
        nReturn = -1;
        TCRIT("IOCTL to get ETHCFG Wake-On-Lan Settings failed: %d\n",r);
    }
    else
#endif
    {
        ethcfg->wolsupported = wol.supported;
        ethcfg->wolopts = wol.wolopts;
        nReturn = 0;
    }

    (void) close(skfd);
    return nReturn;
}

int nwSetEthInformation(unsigned long speed, unsigned int duplex, char * IFName)
{
    int r;
    struct ifreq ifr;
    int skfd;
    struct ethtool_cmd ecmd;
    int nReturn = -1;

    int speed_wanted = -1;
    int duplex_wanted = -1;

    skfd = socket(PF_INET, SOCK_DGRAM, 0 );
    if ( skfd < 0 )
    {
        TCRIT("can't open socket: %s\n",strerror(errno));
        return nReturn;
    }

    memset(&ifr,0,sizeof(struct ifreq));
    memset(&ecmd,0,sizeof(struct ethtool_cmd));

    strcpy(ifr.ifr_name, IFName);

    ecmd.cmd = ETHTOOL_GSET;
    ifr.ifr_data = (char *)&ecmd;
    r = ioctl(skfd, SIOCETHTOOL, &ifr);

#ifndef SOFT_CHECK
    if (r < 0) {
        nReturn = -1;
        TCRIT("IOCTL to get ETHCFG failed: %d\n",r);
    }
    else
#endif
    {
        if (speed == 10)
            speed_wanted = SPEED_10;
        else if (speed == 100)
            speed_wanted = SPEED_100;
        else if (speed == 1000)
            speed_wanted = SPEED_1000;
#ifdef SPEED_2500
        else if (speed == 2500)
            speed_wanted = SPEED_2500;
#endif
        else if (speed == 10000)
            speed_wanted = SPEED_10000;

        if (duplex == 0)
            duplex_wanted = DUPLEX_HALF;
        else if (duplex == 1)
            duplex_wanted = DUPLEX_FULL;

        if(speed_wanted != -1)
        {
            ecmd.speed = (__u16)speed_wanted;
            ecmd.speed_hi = (__u16)(speed_wanted >>16);
            ecmd.autoneg = AUTONEG_DISABLE;
            ecmd.advertising = ecmd.supported & (
                    ADVERTISED_10baseT_Half |
                    ADVERTISED_10baseT_Full |
                    ADVERTISED_100baseT_Half |
                    ADVERTISED_100baseT_Full |
                    ADVERTISED_1000baseT_Half |
                    ADVERTISED_1000baseT_Full |
                    ADVERTISED_2500baseX_Full |
                    ADVERTISED_10000baseT_Full);
        }
        if(duplex_wanted != -1)
        {
            ecmd.duplex = duplex_wanted;
            ecmd.autoneg = AUTONEG_DISABLE;
        }
        if(speed_wanted == -1 && duplex_wanted == -1)
            ecmd.autoneg = AUTONEG_ENABLE;

        /* Try to perform the update. */
        ecmd.cmd = ETHTOOL_SSET;
        ifr.ifr_data = (char *)&ecmd;
        r = ioctl(skfd, SIOCETHTOOL, &ifr);
        if (r < 0)
        {
            nReturn = -1;
            printf("\nCannot set new Ethtool settings \n");
            TCRIT("Cannot set new Ethtool settings\n");
        }
        else
            nReturn = 0;
    }
    (void) close(skfd);
    return nReturn;
}


/**
 *@fn GetNoofInterface
 *@brief This function is invoked to get maximum number of available interface
 *@return Returns maximum EthIndex
 */
int GetNoofInterface()
{
    FILE *fpdev;
    char name[75];
    char oneline[80];
    char *EthName;
    char *SrcMethod;
    int Index=0,i;

    fpdev = fopen(NETWORK_IF_FILE, "r");

    if(NULL==fpdev )
    {
        //TCRIT(" Unable to Open the NETWORK_IF_FILE  file ");
        return 0;
    }

    while(!feof(fpdev))
    {
        if (fgets(oneline,79,fpdev) == NULL)
        {
            break;
        }

        memset(name,0,10);
        sscanf(oneline,"%s", name) ;
        if(strcmp(name,"iface")==0)
        {

            for(i=0;i<sizeof(Ifcnametable)/sizeof(IfcName_T);i++)
            {
                /* Get the highest value of Ethernet interface index value */
                if(Ifcnametable[i].Ifcname[0] == 0)
                    continue;
                EthName = strstr (oneline, Ifcnametable[i].Ifcname);
                if(EthName != NULL)
                {
                    Index=Ifcnametable[i].Index;
                    Ifcnametable[i].Enabled = 1;
                    break;
                }
            }

            if (i == sizeof(Ifcnametable)/sizeof(IfcName_T))
            {
                // No Interface found in the file.
                continue;
            }

            if (EthName != NULL)
            {
                if(strstr(oneline,"inet6") == NULL)
                {
                    m_NwCfgInfo[Index].enable = 1;
                    TDBG("IPv4 present");
                    SrcMethod=strstr(oneline, "dhcp");
                    if(SrcMethod != NULL)
                        m_NwCfgInfo[Index].CfgMethod=CFGMETHOD_DHCP;
                    else
                    {
                        SrcMethod=strstr(oneline, "static");
                        if(SrcMethod != NULL)
                            m_NwCfgInfo[Index].CfgMethod=CFGMETHOD_STATIC;
                        else
                            m_NwCfgInfo[Index].CfgMethod=0;
                    }
                }
                else if(strstr(oneline,"inet6") != NULL)
                {
                    m_NwCfgInfo_v6[Index].enable = 1;
                    TDBG("IPv6 present");
                    SrcMethod=strstr(oneline, "autoconf");
                    if(SrcMethod != NULL)
                    {
                        m_NwCfgInfo_v6[Index].CfgMethod=CFGMETHOD_DHCP;
                        TDBG("DHCP source");
                    }
                    else
                    {
                        TDBG("may be static source");
                        SrcMethod=strstr(oneline, "static");
                        if(SrcMethod != NULL)
                            m_NwCfgInfo_v6[Index].CfgMethod=CFGMETHOD_STATIC;
                        else
                        {
                            m_NwCfgInfo_v6[Index].CfgMethod=0;
                            TDBG("IP address source for IPv6 is zero now..");
                        }
                    }
                }
            }
        }
    }

    fclose(fpdev);
    return 0;
}



#if 1
int nwSetBkupGWyAddr(unsigned char *ip,INT8U EthIndex)
{
    GetNoofInterface();

    /*  Update the  Eth Configuration */
    GetNwCfgInfo();

    printf("The Backup Gateway Address : %d.%d.%d.%d\n",ip[0],ip[1],ip[2],ip[3]);
    memcpy(m_NwCfgInfo[EthIndex].BackupGateway,ip,IP_ADDR_LEN);

    return 1;
}

int nwGetBkupGWyAddr(unsigned char *ip,INT8U EthIndex)

{
    GetNoofInterface();

    /*  Update the  Eth Configuration */
    GetNwCfgInfo();

    memcpy(ip,m_NwCfgInfo[EthIndex].BackupGateway,IP_ADDR_LEN);

    return 1;
}

#endif



/************************************************************************
Function	: GetHostEthbyIPAddr()
Details:	: Gets the Eth Index based on the IPAddress.
 ************************************************************************/

int GetHostEthbyIPAddr(char *IPAddr)
{
    int i=0;

    if(0==*IPAddr )
        return -1;

    GetNoofInterface();

    /*  Update the  Eth Configuration */
    GetNwCfgInfo();


    for(i=0;i<sizeof(Ifcnametable)/sizeof(IfcName_T);i++)
    {
        if(Ifcnametable[i].Ifcname[0] == 0)
            continue;
        if(0==memcmp(m_NwCfgInfo[Ifcnametable[i].Index].IPAddr,IPAddr,IP_ADDR_LEN ))
        {
            return Ifcnametable[i].Index;
        }

    }

    return -1;
}

/*
 * @ fn GetHostEthByIPv6Addr
 * @ brief This function returns the Eth index for given IPv6 Address
 * @ param IPAddr [in] IPv6 Address
 * @ return Ethindex on success, -1 on failure
 */
int GetHostEthByIPv6Addr(char *IPAddr)
{
    int i=0;
    struct ifaddrs *ifAddrOrig = 0;
    struct ifaddrs *ifAddrsP = 0;
    char ifcname[IFNAMSIZ] = {0};

    GetNoofInterface();
    GetNwCfgInfo();

    if (getifaddrs (&ifAddrOrig) == 0)
    {
        ifAddrsP = ifAddrOrig;
        while (ifAddrsP != NULL )
        {
            if(!IN6_IS_ADDR_LINKLOCAL(((struct sockaddr_in6*)(ifAddrsP->ifa_addr))->sin6_addr.s6_addr) && ifAddrsP->ifa_addr->sa_family == AF_INET6 )
            {
                for(i =0;i<sizeof(Ifcnametable)/sizeof(IfcName_T);i++)
                {
                    if(Ifcnametable[i].Ifcname[0] == 0)
                    {
                        continue;
                    }

                    if(m_NwCfgInfo[Ifcnametable[i].Index].VLANID)
                    {
                        sprintf(ifcname,"%s.%d",Ifcnametable[i].Ifcname,m_NwCfgInfo[Ifcnametable[i].Index].VLANID);
                    }
                    else
                    {
                        strcpy(ifcname,Ifcnametable[i].Ifcname);
                    }
                    if((strcmp(ifcname,ifAddrsP->ifa_name) == 0)  && (memcmp(((struct sockaddr_in6*)(ifAddrsP->ifa_addr))->sin6_addr.s6_addr,IPAddr,IP6_ADDR_LEN) == 0))
                    {
                        freeifaddrs(ifAddrOrig);
                        return Ifcnametable[i].Index;
                    }
                }
            }
            ifAddrsP = ifAddrsP->ifa_next;
        }
        freeifaddrs(ifAddrOrig);
    }
    return -1;
}

/*
 * @fn GetIfcNameByIndex
 * @brief This function will return the InterfaceName based on Index 
 * @param Index value[in] IfcName [out]
 * @return 0 on success, -1 on failure
 * */
int GetIfcNameByIndex(int Index,char* IfcName)
{
    int i;

    for(i=0;i<sizeof(Ifcnametable)/sizeof(IfcName_T);i++)
    {
        if(Ifcnametable[i].Ifcname[0] == 0)
            continue;
        if(Index == Ifcnametable[i].Index)
        {
            strcpy(IfcName,Ifcnametable[i].Ifcname);
            return 0;
        }
    }
    return -1;
}

/*********************************************************************************
Function	: nwGetSrcMacAddr_IPV6
Details:	: This function gets the MacAddress providing IP
 **********************************************************************************/

int nwGetSrcMacAddr_IPV6(INT8U* IpAddr,INT8U *MacAddr)
{
    int sd;
    unsigned char data[1024];

    struct sockaddr_in6 addr;
    int s;

    sd  = socket(AF_INET, SOCK_PACKET, htons(ETH_P_IPV6));
    if(sd < 0)
    {
        TCRIT("Unable to open socket \n");
        return 0;
    }

    if ( (s = socket(PF_INET6, SOCK_DGRAM, 0)) < 0 )
    {
        close(sd);
        return 0;
    }

    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(9999);
    memcpy (&addr.sin6_addr,IpAddr, sizeof (struct in6_addr));
    memset(MacAddr,0,MAC_ADDR_LEN);
    sendto(s,NULL,0, 0, (struct sockaddr*)&addr, sizeof(addr));

    recvfrom(sd, data, sizeof(data), 0,0,0);
    memcpy(MacAddr,data+MAC_ADDR_LEN,MAC_ADDR_LEN);
    close(sd);
    close(s);
    return 0;

}
/*********************************************************************************
Function	: GetSrcMacAddr
Details:	: This function gets the MacAddress providing IP(other than present source) and EthIndex
via ARP
 **********************************************************************************/

int nwGetSrcMacAddr(INT8U* IpAddr,INT8U EthIndex,INT8U *MacAddr)
{

    struct in_addr dst,src;
    struct sockaddr_ll srcsock,dstsock,recvsock;
    int s,retry = 2;
    int cc;
    INT8S device[MAX_STR_LENGTH];
    int ifindex = 0;
    unsigned char *PrevMac=NULL,*p;
    struct ifreq ifr;
    unsigned char buf[256];
    struct arphdr *arph;
    unsigned char packet[4096];
    socklen_t alen;

    memset(MacAddr,0,MAC_ADDR_LEN);
    sprintf(device,"%s",Ifcnametable[EthIndex].Ifcname);
    s = socket(PF_PACKET, SOCK_DGRAM, 0);
    if(s < 0)
    {
        TCRIT("Unable to open socket \n");
        return 0;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, device, IFNAMSIZ - 1);

    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0)
    {
        TDBG("Interface %s not found", device);
        close(s);
        return 0;
    }

    ifindex = ifr.ifr_ifindex;

    memcpy((INT8S *)&dst,IpAddr,IP_ADDR_LEN);

    srcsock.sll_family = AF_PACKET;
    srcsock.sll_ifindex = ifindex;
    srcsock.sll_protocol = htons(ETH_P_ARP);
    if (bind(s, (struct sockaddr *) &srcsock, sizeof(srcsock)) == -1)
    {
        TDBG("Failure in Binding");
        close(s);
        return 0;
    }

    alen = sizeof(srcsock);

    if (getsockname(s, (struct sockaddr *) &srcsock, &alen) == -1)
    {
        TDBG("Failure in getsockname");
        close(s);
        return 0;
    }

    if (srcsock.sll_halen == 0)
    {
        TDBG("Interface %s is not able to communicate", device);
        close(s);
        return 0;
    }

    dstsock = srcsock;
    memset(dstsock.sll_addr, -1, dstsock.sll_halen);

    while(retry)
    {
        arph = (struct arphdr *) buf;
        p = (unsigned char *) (arph + 1);

        arph->ar_hrd = htons(srcsock.sll_hatype);
        arph->ar_hrd = htons(ARPHRD_ETHER);
        arph->ar_pro = htons(ETH_P_IP);
        arph->ar_hln = srcsock.sll_halen;
        arph->ar_pln = 4;
        arph->ar_op =  htons(ARPOP_REQUEST);

        memcpy(p, &srcsock.sll_addr, arph->ar_hln);
        p += srcsock.sll_halen;

        memcpy(p, (unsigned char *)&src, 4);
        p += 4;

        memcpy(p, (unsigned char *)&dstsock.sll_addr[0], arph->ar_hln);
        p += arph->ar_hln;

        memcpy(p, (unsigned char *)&dst, 4);
        p += 4;

        sendto(s, buf, p - buf, 0, (struct sockaddr *)&dstsock, sizeof(dstsock));

        alen = sizeof(recvsock);
        arph = (struct arphdr *) packet;
        p = (unsigned char *) (arph + 1);

        if ((cc = recvfrom(s, packet, sizeof(packet), 0,(struct sockaddr *) &recvsock, &alen)) < 0)
        {
            TDBG("Failed in Recvfrom");
        }

        if(retry == 2)
        {
            PrevMac= malloc(MAC_ADDR_LEN);
            if(PrevMac == NULL)
            {
                TCRIT("Error in Allocating memory\n");
                close(s);
                return -1;
            }
            memcpy(PrevMac,p,MAC_ADDR_LEN);
        }
        retry--;
    }

    if(0 != memcmp(p,PrevMac,MAC_ADDR_LEN))
    {
        free(PrevMac);
        close(s);
        return 0;
    }
    else
    {
        memcpy(MacAddr,PrevMac,MAC_ADDR_LEN);
    }

    free(PrevMac);
    close(s);
    return 0;

}

static unsigned char *ethernet_mactoa(struct sockaddr *addr) 
{ 
    static unsigned char buff[256]; 
    unsigned char *ptr = (unsigned char *) addr->sa_data;

    buff[0] = (ptr[0] & 0377);
    buff[1] = (ptr[1] & 0377);
    buff[2] = (ptr[2] & 0377);
    buff[3] = (ptr[3] & 0377);
    buff[4] = (ptr[4] & 0377);
    buff[5] = (ptr[5] & 0377);

    return (buff); 

}

/*********************************************************************************
Function    : nwGetSrcCacheMacAddr
Details:    : This function gets the MacAddress providing IP(other than present 
source) and EthIndex via arp cache
 **********************************************************************************/

int nwGetSrcCacheMacAddr(INT8U* IpAddr, INT8U EthIndex, INT8U *MacAddr)
{
    int s;
    INT8S device[MAX_STR_LENGTH];
    struct arpreq       areq;
    struct sockaddr_in *sin;
    unsigned char StrIpAddr[32];
    unsigned char gwIpAddr[IP_ADDR_LEN];

    memset(MacAddr, 0, MAC_ADDR_LEN);
    memset(&areq, 0, sizeof(areq));
    sprintf(device, "%s", Ifcnametable[EthIndex].Ifcname);
    s = socket(PF_PACKET, SOCK_DGRAM, 0);
    if(s < 0)
    {
        TCRIT("Error: Unable to open socket \n");
        return 0;
    }

    ConvertIPnumToStr(IpAddr, IP_ADDR_LEN, StrIpAddr);
    sin = (struct sockaddr_in *) &areq.arp_pa;
    sin->sin_family = AF_INET;
    if (inet_aton((char *)StrIpAddr, &(sin->sin_addr)) == 0)
    {
        TCRIT("Error: invalid numbers-and-dots IP address %s.\n", IpAddr);
        close(s);
        return 0;
    }

    sin = (struct sockaddr_in *) &areq.arp_ha;
    sin->sin_family = ARPHRD_ETHER;

    strncpy(areq.arp_dev, device, IFNAMSIZ - 1);
    if (ioctl(s, SIOCGARP, (caddr_t) &areq) == -1)
    {
        GetDefaultGateway(gwIpAddr, (INT8U *)device);
        ConvertIPnumToStr(gwIpAddr, IP_ADDR_LEN, StrIpAddr);
        sin = (struct sockaddr_in *) &areq.arp_pa;
        if (inet_aton((char *)StrIpAddr, &(sin->sin_addr)) == 0)
        {
            TCRIT("Error: invalid numbers-and-dots IP address %s.\n", IpAddr);
            close(s);
            return 0;
        }
        if (ioctl(s, SIOCGARP, (caddr_t) &areq) == -1)
        {
            TCRIT("Error: unable to make ARP request.\n");
            close(s);
            return 0;
        }
    }

    memcpy(MacAddr, ethernet_mactoa(&areq.arp_ha), MAC_ADDR_LEN);

    close(s);
    return 0;

}

/**
 *@fn nwUpdateVLANInterfacesFile
 *@brief This function is invoked to update vlan interfaces file
 *@return Returns 0 on success
 */
int nwUpdateVLANInterfacesFile(void)
{
    FILE* fp;
    unsigned char strBuf[32];
    unsigned char str1[INET6_ADDRSTRLEN];
    int count=0;

    /* Open VLAN Interface and VLAN ID files */
    fp = fopen(VLAN_INTERFACES_FILE,"w");

    if(fp == NULL)
    {
        TCRIT("Error opening VLAN interfaces file..VLAN may not go up!!!\n");
        return -1;
    }

    /* Updating vlaninterface file based number of interfaces */
    for(count=0;count<sizeof(Ifcnametable)/sizeof(IfcName_T);count++)
    {

        if(Ifcnametable[count].Ifcname[0] == 0)
            continue;

        if(Ifcnametable[count].Enabled != 1)
            continue;

        if( m_NwCfgInfo_v6[Ifcnametable[count].Index].enable == 1 )
        {
            TDBG("nwUpdateVLANInterfacesFile for IPv6 \n");
            if(m_NwCfgInfo_v6[Ifcnametable[count].Index].CfgMethod == CFGMETHOD_DHCP)
            {

                fprintf(fp,"iface %s.%d inet6 autoconf\n  ", Ifcnametable[count].Ifcname, m_NwCfgInfo[Ifcnametable[count].Index].VLANID);

            }
            else
            {
                TDBG("nwUpdateVLANInterfacesFile for IPv6 - STATIC");
                /* To print the eth0/1/2 inet static  */
                fprintf(fp,"iface %s.%d inet6 static\n  ", Ifcnametable[count].Ifcname, m_NwCfgInfo[Ifcnametable[count].Index].VLANID);

                /* IPv6 address */
                ConvertIP6numToStr(m_NwCfgInfo_v6[Ifcnametable[count].Index].GlobalIPAddr[0],INET6_ADDRSTRLEN,str1);
                if (strcmp("::", (char *) str1) == 0)
                {
                    strcpy((char*) str1, " ");
                    m_NwCfgInfo_v6[Ifcnametable[count].Index].GlobalPrefix[0] = 0;
                }
                fprintf(fp,"%s %s\n",IF_STATIC_IP_STR,str1);

                /* IPv6 Prefix */
                fprintf(fp,"%s %d\n",IF_STATIC_MASK_STR,m_NwCfgInfo_v6[Ifcnametable[count].Index].GlobalPrefix[0]);

                /* IPv6 Gateway */
                ConvertIP6numToStr(m_NwCfgInfo_v6[Ifcnametable[count].Index].Gateway,46,str1);
                if (strncmp("fe80::", (char*) str1, strlen("fe80::")) == 0)
                {
                    // Gateway address is a link local address, add device name "eth0"
                    fprintf(fp, "%s %s dev %s\n", IF_STATIC_GW_STR, str1, Ifcnametable[count].Ifcname);
                }
                else
                {
                    fprintf(fp, "%s %s\n", IF_STATIC_GW_STR, str1);
                }
            }
        }

        if(m_NwCfgInfo[Ifcnametable[count].Index].CfgMethod == CFGMETHOD_DHCP)
        {
            /* Write VLAN starting string to Vlan interface file   */
            fprintf(fp,"iface %s.%d inet dhcp\n",Ifcnametable[count].Ifcname,m_NwCfgInfo[Ifcnametable[count].Index].VLANID);
            /* Write VLAN IFC mtu size to vlaninterface file */
            fprintf (fp,"pre-up /sbin/ifconfig %s.%d mtu %d\n", Ifcnametable[count].Ifcname, m_NwCfgInfo[Ifcnametable[count].Index].VLANID, VLAN_IFC_MTU_SIZE);
        }
        else
        {
            /* Write VLAN starting string to Vlan interface file   */
            fprintf(fp,"iface %s.%d inet static\n",Ifcnametable[count].Ifcname,m_NwCfgInfo[Ifcnametable[count].Index].VLANID);


            ConvertIPnumToStr(m_NwCfgInfo[Ifcnametable[count].Index].IPAddr,IP_ADDR_LEN,strBuf);
            fprintf(fp,"%s %s\n",IF_STATIC_IP_STR,strBuf);

            ConvertIPnumToStr(m_NwCfgInfo[Ifcnametable[count].Index].Mask,IP_ADDR_LEN,strBuf);
            fprintf(fp,"%s %s\n",IF_STATIC_MASK_STR,strBuf);

            ConvertIPnumToStr(m_NwCfgInfo[Ifcnametable[count].Index].Broadcast,IP_ADDR_LEN,strBuf);
            fprintf(fp,"%s %s\n",IF_STATIC_BCAST_STR,strBuf);

            ConvertIPnumToStr(m_NwCfgInfo[Ifcnametable[count].Index].Gateway,IP_ADDR_LEN,strBuf);
            fprintf(fp,"%s %s\n",IF_STATIC_GW_STR,strBuf);

            /* Write VLAN IFC mtu size to vlaninterface file */
            fprintf(fp,"%s %d\n",IF_STATIC_MTU_STRING,VLAN_IFC_MTU_SIZE);
        }
    }
    /* closing VLAN interfaces file   */
    fclose(fp);
    return 0;
}


/*----------------------------------------------*/
/*     Functions to read and write VLAN Interface Files       */
/*----------------------------------------------*/

/**
 *@fn ReadVLANFile
 *@param SettingStr - Pointer to setting name that we want to read from vlan configurations file
 *@param desArr - pointer to an array where the reading has to be stored
 *@brief This function is invoked to read all the vlan configuration files
 *@return Returns 0 on success and -1 on fails
 */
int ReadVLANFile(char *SettingStr, INT16U  *desArr)
{
    dictionary *d = NULL;
    INTU    err_value = 0xFFFFFFFF;
    char temp[MAX_TEMP_ARRAY_SIZE];
    INTU tempval;
    char *sectionname=NULL;
    int nsec = 0, i = 0;
    INT8U Index;

    d = iniparser_load(VLANSETTING_CONF_FILE);
    if( d == NULL )
    {
        TDBG("Unable to find/load/parse Configuration file : %s", VLANSETTING_CONF_FILE);
        return -1;
    }

    nsec = iniparser_getnsec(d);
    for (i=0; i<nsec; i++)
    {
        sectionname = iniparser_getsecname (d, i);
        if(NULL == sectionname)
        {
            TINFO("Unable to get setion name of configuration file : %s", VLANSETTING_CONF_FILE);
            return -1;
        }

        /* Get VLAN_INDEX_STR under current section */
        err_value = 0xFFFFFFFF;
        memset(temp, 0, sizeof(temp));
        sprintf(temp,"%s:%s",sectionname, VLAN_INDEX_STR);
        tempval = iniparser_getint (d, temp, err_value);
        if(tempval == err_value)
        {
            TINFO("Configuration %s is not found\n", VLAN_INDEX_STR);
            Index = i; /* In default, vlan index is the same as section index */
        }
        else
        {
            Index = (INT8U)tempval;
            TDBG("Configured %s value is %x \n", VLAN_INDEX_STR, tempval);
        }

        /* Get value of SettingStr under current section */        
        err_value = 0xFFFFFFFF;
        memset(temp, 0, sizeof(temp));
        sprintf(temp, "%s:%s", sectionname, SettingStr);
        tempval = iniparser_getint (d, temp, err_value);
        if(tempval == err_value)
        {
            TDBG("Configuration %s is not found\n", SettingStr);
            /* Both vlanid and vlanpriority use 0 as default */
            desArr[Index] = 0;
        }
        else
        {
            desArr[Index] = tempval;
            TDBG("Configured %s value is %x \n", SettingStr, tempval);
        }
    }
    iniparser_freedict(d);

    return 0;
}

/**
 *@fn WriteVLANFile
 *@brief This function is invoked to write all the vlan configuration files
 *@param SettingStr - Pointer to setting name that we want to write into vlan configurations file
 *@param desArr - pointer to an array where the reading has to be stored
 *@param EthIndex - char value to Ethernet index
 *@param val - short int to the value that has to be written
 *@return Returns 0 on success and -1 on fails
 */
int WriteVLANFile(char *SettingStr, INT16U  *desArr, INT8U EthIndex, INT16U val)
{
    dictionary *d = NULL;
    INTU    err_value = 0xFFFFFFFF;
    char temp[MAX_TEMP_ARRAY_SIZE];
    INTU tempval;
    char tempstr[MAX_TEMP_ARRAY_SIZE];
    INTU SetValue;
    char *sectionname=NULL;
    int nsec = 0, i = 0;
    FILE* fp;
    INT8U Index;

    d = iniparser_load(VLANSETTING_CONF_FILE);
    if( d == NULL )
    {
        TDBG("Unable to find/load/parse Configuration file : %s", VLANSETTING_CONF_FILE);
        return -1;
    }

    nsec = iniparser_getnsec(d);
    for (i=0; i<nsec; i++)
    {
        sectionname = iniparser_getsecname (d, i);
        if(NULL == sectionname)
        {
            TINFO("Unable to get setion name of configuration file : %s", VLANSETTING_CONF_FILE);
            return -1;
        }

        /* Get VLAN_INDEX_STR under current section */
        err_value = 0xFFFFFFFF;
        memset(temp, 0, sizeof(temp));
        sprintf(temp,"%s:%s",sectionname, VLAN_INDEX_STR);
        tempval = iniparser_getint (d, temp, err_value);
        if(tempval == err_value)
        {
            TINFO("Configuration %s is not found\n", VLAN_INDEX_STR);
            Index = i;/* In default, vlan index is the same as section index */
        }
        else
        {
            Index = (INT8U)tempval;
            TDBG("Configured %s value is %x \n", VLAN_INDEX_STR, tempval);
        }

        /* Actually set value by index number */
        if(EthIndex == Index)
            SetValue = val;
        else
            SetValue = desArr[i];

        memset(temp, 0, sizeof(temp));
        memset(tempstr, 0, sizeof(tempstr));
        sprintf(temp, "%s:%s", sectionname, SettingStr);
        sprintf(tempstr, "%u", SetValue);

        iniparser_setstr(d, temp, tempstr);
    }

    fp = fopen(VLANSETTING_CONF_FILE,"w");
    if(fp == NULL)
    {
        TCRIT("Could not open config file %s to set config\n", VLANSETTING_CONF_FILE);
        iniparser_freedict(d);
        return -1;
    }

    iniparser_dump_ini(d,fp);
    fclose(fp);

    iniparser_freedict(d);

    return 0;
}


/*
 * @brief Getting the MAC and etnernet configuration
 *
 * @param[in] cfg - Both MAC and Ethernet configuration
 *          BurnedMAC - Burned MAC address
 *  `       Local_MAC - Locally admin MAC
 *          speed -
 *          duplex -
 *          autoneg -
 *          maxtxpkt -
 *
 * @returns 0 on success -1 on failure
 */
int nwGetNWExtEthCfg(NWEXT_ETHCFG *cfg)
{
    /* Setting all the other values to NULL */
    memset(cfg, 0x0, sizeof(NWEXT_ETHCFG));

    if (-1 == nwGetEthInformation(&cfg->eth_cfg, "eth0")) {
        /* first trying with ethtool. if that fails
         * tries without that. */
        /* Copying the MAC address to both BurnedMAC as
           well as Local_MAC */
        nwGetExtMACAddr(cfg->mac_cfg.BurnedMAC);
        return nwGetExtMACAddr(cfg->mac_cfg.Local_MAC);
    }

    /* Returning only the MAC addr */
    /* Copying the MAC address to both BurnedMAC as
       well as Local_MAC */
    nwGetExtMACAddr(cfg->mac_cfg.BurnedMAC);
    return nwGetExtMACAddr(cfg->mac_cfg.Local_MAC);
}


/*!
 * @brief Get interface enable state
 * This fuction gets the network interface
 * state and returns the status
 *
 * @returns NW_INTERFACE_ENABLE / NW_INTERFACE_DISABLE  on success
 -1 on failure
 */
int nwGetNWInterfaceStatus(void)
{
    return NW_INTERFACE_ENABLE;
}


/*!
 * @brief Getting the current capabilities of the ethernet conf
 * This fuction gets the ethernet configuration
 * capabilities like MAC, SPEED, DUPLEX, MTU, AUTONEG supports.
 *
 * @returns Capabilities FLAG on success
 */
int nwGetExtEthCaps(void)
{
    int NwEthCaps = 0;

    // SPEED support
    SET_FLAG(NwEthCaps, NWEXT_ETHCFG_SPEED);

    // DUPLEX support
    SET_FLAG(NwEthCaps, NWEXT_ETHCFG_DUPLEX);
    // MTU Support
    SET_FLAG(NwEthCaps, NWEXT_ETHCFG_MTU);

    return NwEthCaps;
}


/*!
 * @brief Writing Ethernet information
 * This fuction gets the current and ethernet
 * information like (speed, duplex, mtu, autoneg)
 *
 * @param[in] IFName - Name of the interface (act/pact)
 *            speed - Possible values are 10 or 100
 *            duplex - Possible values are full or half
 *            Autoneg - Possible values are 1/2
 *            MTU - Maximum transmission unit
 *
 * @returns 0 on succee , -1 on failure
 */
int nwSetExtEthInformation(ETHCFG_STRUCT *ethcfg, char * IFName, int setflag)
{
    int ipcaps = nwGetExtEthCaps();
    /*
     * ipcaps   - the capabilities of the system
     * setflag  - the flag representing the field to be set
     *            (comes in the request)
     *
     * (ipcaps & setflag) should be equal to setflag if the
     * caller is trying to set within the scope of the system.
     *
     * Incase if the caller is requesting for something out of scope
     * (i.e) something which is not support, this function will have
     * to return a error code with those unsupported fields set.
     *
     */
    if ((ipcaps & setflag) != setflag) {
        return (setflag ^ (ipcaps & setflag));
    }
    int r;
    struct ifreq ifr;
    int skfd;
    struct ethtool_cmd ecmd;
    int nReturn = -1;

    static int speed_wanted = -1;
    static int duplex_wanted = -1;

    skfd = socket(PF_INET, SOCK_DGRAM, 0 );
    if ( skfd < 0 )
    {
        TCRIT("can't open socket: %s\n",strerror(errno));
        return nReturn;
    }

    memset(&ifr,0,sizeof(struct ifreq));
    memset(&ecmd,0,sizeof(struct ethtool_cmd));

    strcpy(ifr.ifr_name, IFName);

    ecmd.cmd = ETHTOOL_GSET;
    ifr.ifr_data = (char *)&ecmd;
    r = ioctl(skfd, SIOCETHTOOL, &ifr);

#ifndef SOFT_CHECK
    if (r < 0) {
        nReturn = -1;
        TCRIT("IOCTL to get ETHCFG failed: %d\n",r);
    }
    else
#endif
    {
        /*
         * Checking if the caller has set the NWEXT_ETHCFG_SPEED
         * flag, which means the caller wishes to change the speed
         * to the new value given in the request.
         */
        if (CHECK_FLAG(setflag, NWEXT_ETHCFG_SPEED)) {
            if (ethcfg->speed == 10)
                speed_wanted = SPEED_10;
            else if (ethcfg->speed == 100)
                speed_wanted = SPEED_100;
            else if (ethcfg->speed == 1000)
                speed_wanted = SPEED_1000;
#ifdef SPEED_2500
            else if (ethcfg->speed == 2500)
                speed_wanted = SPEED_2500;
#endif
            else if (ethcfg->speed == 10000)
                speed_wanted = SPEED_10000;

            if(speed_wanted != -1)
                ecmd.speed = speed_wanted;
        }
        /*
         * Checking if the caller has set the NWEXT_ETHCFG_DUPLEX
         * flag, which means the caller wishes to change the duplex
         * to the new value given in the request.
         */
        if (CHECK_FLAG(setflag, NWEXT_ETHCFG_DUPLEX)) {
            if (ethcfg->duplex == NW_DUPLEX_HALF)//correcting the value.
                duplex_wanted = DUPLEX_HALF;
            else if (ethcfg->duplex == NW_DUPLEX_FULL)
                duplex_wanted = DUPLEX_FULL;

            if(duplex_wanted != -1)
                ecmd.duplex = duplex_wanted;
        }
        /*
         * Checking if the caller has set the NWEXT_ETHCFG_MTU
         * flag, which means the caller wishes to change the MTU
         * to the new value given in the request.
         */
        if (CHECK_FLAG(setflag, NWEXT_ETHCFG_MTU)) {
            ifr.ifr_mtu = ethcfg->maxtxpkt;
            r = ioctl(skfd,SIOCSIFMTU,&ifr);
            if (r<0)
            {	
                nReturn = -1;
                TCRIT("cannot set new MTU Ethtool settings ret = %d, errno =%d\n",r,errno);
            }
        }
        /*
         * Peforming ioctl() only when user has set either the
         * speed, mtu or duplex flag.
         */
        if ( (CHECK_FLAG(setflag, NWEXT_ETHCFG_SPEED))
                || (CHECK_FLAG(setflag, NWEXT_ETHCFG_DUPLEX)) ) {
            /* Try to perform the update. */
            ecmd.cmd = ETHTOOL_SSET;
            ifr.ifr_data = (char *)&ecmd;
            r = ioctl(skfd, SIOCETHTOOL, &ifr);
            if (r < 0)
            {
                nReturn = -1;
                TCRIT("Cannot set new Ethtool settings\n");
            }
        }
        else
            nReturn = 0;
    }
    (void) close(skfd);
    return nReturn;
    return -1;
}


/*!
 * @brief Writing the MAC and etnernet configuration
 *
 * @param[in] cfg - Both MAC and Ethernet configuration
 *  `       Local_MAC - Locally admin MAC
 *          speed -
 *          duplex -
 *          autoneg -
 *          maxtxpkt -
 *
 * @returns 0 on success -1 or ERROR_FLAG on failure
 */
int nwSetNWExtEthCfg(NWEXT_ETHCFG *cfg, int setflag)
{
    return nwSetExtEthInformation(&cfg->eth_cfg, "eth0", setflag);
}


/*!
 * @brief
 *
 * @returns Capabilitis FLAG on success
 */
int nwGetExtIPCaps(void)
{
    int NwIPCaps = 0;

    // capable of changing ip configuration method
    SET_FLAG(NwIPCaps, NWEXT_IPCFG_CFGMETHOD);

    // capable of chaning ip address manually
    SET_FLAG(NwIPCaps, NWEXT_IPCFG_IP);

    // capable of changing subnet mask
    SET_FLAG(NwIPCaps, NWEXT_IPCFG_MASK);

    // capable of changing default gateway
    SET_FLAG(NwIPCaps, NWEXT_IPCFG_GW);

    return NwIPCaps;
}


/*!
 * @brief Writing network ip information
 * This fuction sets the network enabled state,
 * pending ip information like ip, netmask, gateway
 *
 * @param[in] IFName - Name of the interface
 *            CfgMethod - Confg method NWCFGTYPE_DHCP or NWCFGTYPE_STATIC
 *                        or NWCFGTYPE_DHCPFIRST
 *            Enable - If interface is enabled, value is NW_INTERFACE_ENABLE
 *                     otherwise NW_INTERFACE_DISABLE.
 *            FB_IPAddr - Fall back IPAddress
 *            FB_Mask - Fall back netmask
 *            FB_Gateway - Fall back gatewayip
 *
 * @returns 0 on succee , -1 on failure
 */
int nwSetNWExtIPCfg (NWEXT_IPCFG *cfg, int nwSetFlag,int global_ipv6)
{

    NWCFG_STRUCT nwcfg;
    NWCFG6_STRUCT nwcfg_v6;

    if(nwReadNWCfg_v4_v6( &nwcfg, &nwcfg_v6,0,global_ipv6) < 0)
    {
        TCRIT("Unable to get IP information\n");
        return -1;
    }
    if(cfg == NULL || nwSetFlag == 0x0)
    {
        TCRIT(" NULL arguments\n");
        return -1;
    }


    nwcfg.CfgMethod = CFGMETHOD_STATIC;

    if( CHECK_FLAG(nwSetFlag, NWEXT_IPCFG_ALL)
            || CHECK_FLAG(nwSetFlag, NWEXT_IPCFG_FBIP))
    {
        memcpy(nwcfg.IPAddr, cfg->FB_IPAddr, sizeof(nwcfg.IPAddr));
    }


    if( CHECK_FLAG(nwSetFlag, NWEXT_IPCFG_ALL)
            || CHECK_FLAG(nwSetFlag, NWEXT_IPCFG_FBMASK))
    {
        memcpy(nwcfg.Mask, cfg->FB_Mask, sizeof(nwcfg.Mask));
    }
    if( CHECK_FLAG(nwSetFlag, NWEXT_IPCFG_ALL)
            || CHECK_FLAG(nwSetFlag, NWEXT_IPCFG_FBGW))
    {
        memcpy(nwcfg.Gateway, cfg->FB_Gateway, sizeof(nwcfg.Gateway));
    }

    if( nwWriteNWCfg_ipv4_v6( &nwcfg , &nwcfg_v6, 0) < 0 )
    {
        TCRIT("Unable to save IP cfg\n");
        return -1;
    }

    return 0;

}


static int nwGetExtNWInformations(NWEXT_IPCFG *cfg)
{

    NWCFGS act_cfg;
    memset(&act_cfg, 0x0, sizeof(act_cfg));

    if(nwReadNWCfgs(&act_cfg, NULL) < 0)
    {
        TCRIT("Unable to get IP information\n");
        return -1;
    }

    /*
     * This field can either be NWCFGTYPE_DHCP or
     * NWCFGTYPE_STATIC or NWCFGTYPE_DHCPFIRST
     */
    cfg->CfgMethod = act_cfg.NwInfo[0].CfgMethod;
    /*
     *  If interface is enabled, value is NW_INTERFACE_ENABLE
     *  otherwise NW_INTERFACE_DISABLE
     */
    cfg->Enable = NW_INTERFACE_ENABLE;
    /*
     * Current IP Origin NWCFGTYPE_DHCP or NWCFGTYPE_STATIC
     */
    cfg->IPOrigin = act_cfg.NwInfo[0].CfgMethod;

    /*
     * IP assigned: If IPOrgin is DHCP, then this is DHCP IP,
     * if the IPOrigin is Static, then this is Static IP address
     */
    memcpy( cfg->IPAddr, act_cfg.NwInfo[0].IPAddr, sizeof(act_cfg.NwInfo[0].IPAddr));
    memcpy( cfg->Mask, act_cfg.NwInfo[0].Mask, sizeof(act_cfg.NwInfo[0].Mask));
    memcpy( cfg->Gateway, act_cfg.NwInfo[0].Gateway, sizeof(act_cfg.NwInfo[0].Gateway));

    /*
     *  Manually configured Fall back (FB) IP
     */
    memcpy( cfg->FB_IPAddr, act_cfg.NwInfo[0].IPAddr, sizeof(act_cfg.NwInfo[0].IPAddr));
    memcpy( cfg->FB_Mask, act_cfg.NwInfo[0].Mask, sizeof(act_cfg.NwInfo[0].Mask));
    memcpy( cfg->FB_Gateway, act_cfg.NwInfo[0].Gateway, sizeof(act_cfg.NwInfo[0].Gateway));

    return 0;
}


/*
 * @brief Reading current/active network configuration using netman script
 * #param[out] cfg - IP, Netmask, Gateway, Conf method(dhcp/statis)
 * @returns 0 on succee , -1 on failure
 */
int nwGetNwActIPCfg( NWEXT_IPCFG *cfg )
{
    return( nwGetExtNWInformations(cfg) );
}

/*!
 * @brief Get DHCP server ip
 * This function gets the dhcp server ip address.
 *
 * @param[out] dhcpServerIP - IP address of DHCP server
 *
 * @returns 0  on success
 -1 on failure
 */
int nwGetDHCPServerIP(char *dhcpServerIP)
{

    if(dhcpServerIP == NULL) {
        return -1;
    }
    // TODO : Actual implementation to get the dhcp server ip
    strcpy(dhcpServerIP, "0.0.0.0");
    return 0;
}

/*
 *@fn GetNwLinkStatus_mii
 *@brief This function gets the network link status using MII
 *@param ifname - Interface Name
 */
int GetNwLinkStatus_mii(int fd,char *ifname)
{
    int Status =0;
    struct ifreq ifr;
    struct mii_ioctl_data *mii = (struct mii_ioctl_data *)&ifr.ifr_data;

    strncpy(ifr.ifr_name,ifname,IFNAMSIZ);
    if(ioctl(fd,SIOCGMIIPHY,&ifr) < 0)
    {
        perror("");
        printf("Error in IOCTL to get SIOCGMIIPHY \n");
        return -1;
    }

    mii->reg_num = MII_BMSR;

    if(ioctl(fd,SIOCGMIIREG,&ifr) < 0 )
    {
        printf("Error in IOCTL for SIOCGMIIREG \n");
        return -1;
    }

    Status = mii->val_out;

    TDBG("Status is %x \n",(Status & BMSR_LSTATUS));
    TDBG("Link status is %s \n",(Status & BMSR_LSTATUS)?"up":"down");

    return (Status & BMSR_LSTATUS);

}

/*
 *@fn GetNwLinkStatus
 *@brief This function gets the network link status using SIOCETHTOOL
 *@param ifname - Interface Name
 */
int GetNwLinkStatus_ethtool(int fd,char *ifname)
{
    struct ifreq ifr;
    struct ethtool_value edata;
    int ret =0;

    memset(&ifr, 0, sizeof(ifr));
    edata.cmd = ETHTOOL_GLINK;

    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);
    ifr.ifr_data = (char *) &edata;

    ret = ioctl(fd, SIOCETHTOOL, &ifr);
    if (ret == -1) 
    {
        if(errno == EOPNOTSUPP)
        {
            return 0xFF;
        }
        perror("");
        TWARN("Error in Getting Link status %x\n",errno);
        return -1;
    }

    return (edata.data ? 1:0);
}

/*
 *@fn GetNwLinkStatus
 *@brief This function get the link status
 *@param ifname - Interface name
 */
int GetNwLinkStatus(int fd,char *ifname)
{
    int ret =0;

    ret = GetNwLinkStatus_ethtool(fd,ifname);
    if(ret == 0xFF)
    {
        ret = GetNwLinkStatus_mii(fd,ifname);
    }

    return ret;
}



/*
 *@fn GetNwLinkType_mii
 *@brief This function gets the network link type using MII
 *@param ifname - Interface Name
 *@returns 0 for MDIO, 1 for NCSI and 0xFF on error
 */
int GetNwLinkType_mii(char *ifname)
{
    int linkType = 0;
    int fd = 0;
    int retVal = 0xFF;
    struct ifreq ifr;
    struct mii_ioctl_data *mii = (struct mii_ioctl_data *) &ifr.ifr_data;

    fd = socket(PF_INET, SOCK_DGRAM, 0);
    if(fd < 0)
    {
        TCRIT("GetNwLinkType_mii(): Can't open socket.\n");
        return retVal;
    }

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    if(ioctl(fd, SIOCGMIIPHY, &ifr) < 0)
    {
        TCRIT("GetNwLinkType_mii(): Error in IOCTL to get SIOCGMIIPHY. \n");
        goto error_out;
    }

    mii->reg_num = MII_PHY_ID1;
    if(ioctl(fd,SIOCGMIIREG,&ifr) < 0)
    {
        TCRIT("GetNwLinkType_mii(): Error in IOCTL for SIOCGMIIREG. \n");
        goto error_out;
    }

    linkType = mii->val_out;
    if(linkType != 0 && linkType != 0xFFFF) //Found valid PHY, so it should be of type MDIO
    {
        retVal = 0;
        goto error_out;
    }

    mii->reg_num = MII_PHY_ID2; //Proceeding to check second PHY register
    if(ioctl(fd, SIOCGMIIREG, &ifr) < 0)
    {
        TCRIT("GetNwLinkType_mii(): Error in IOCTL for SIOCGMIIREG. \n");
        retVal = 0xFF;
        goto error_out;
    }

    linkType = mii->val_out;
    if(linkType != 0 && linkType != 0xFFFF) //Found valid PHY, so it should be of type MDIO
    {
        retVal = 0;
        goto error_out;
    }
    else //Invalid PHY found; assuming the link type as NCSI
    {
        retVal = 1;
        goto error_out;
    } 

error_out:
    close(fd);
    return retVal;
}
int GetIPAdrressType(INT8U* IPAddr)
{
    struct sockaddr_in6 sadr;
    char hostBfr[ NI_MAXHOST ];   /* For use w/getnameinfo(3).    */
    sadr.sin6_family = AF_INET6;
    memcpy(&sadr.sin6_addr,IPAddr,sizeof(struct in6_addr));
    getnameinfo((struct sockaddr *)&sadr,sizeof(sadr),hostBfr,sizeof( hostBfr ),NULL,0,NI_NUMERICHOST | NI_NUMERICSERV );
    if(strstr(hostBfr,"."))return 4;
    return 6;
}

/**
 *@fn GetIPAddrstr
 *@brief This function is used to get IP address string for given ip or hostname
 *@param addr - pointer to string of IP address of hostname
 *@param pResIPaddr - pointer to IP address string
 *@return Returns address family (AF_INET or AF_INET6) on success and -1 on failure
 */

int GetIPAddrstr(unsigned char *addr, char *pResIPaddr)
{
    struct addrinfo hints,* pTempres,* pResult;
    struct sockaddr_in *ipv4;
    struct sockaddr_in6 *ipv6;
    void * ipaddr;
    char ipaddrstr[INET6_ADDRSTRLEN];
    int ret;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    ret = getaddrinfo((char *)addr, NULL, &hints, &pResult);
    if(ret != 0)
    {
        TCRIT("error in getting addrinfo\n");
        return -1;
    }
    for(pTempres = pResult; pTempres != NULL; pTempres = pTempres->ai_next)
    {
        if(pTempres->ai_family == AF_INET)
        {
            ipv4 = (struct sockaddr_in *)pTempres->ai_addr;
            ipaddr = &(ipv4->sin_addr);
            inet_ntop(AF_INET, ipaddr, ipaddrstr, INET_ADDRSTRLEN);
            ipaddrstr[INET6_ADDRSTRLEN-1]='\0';
            memcpy(pResIPaddr, ipaddrstr, INET_ADDRSTRLEN);
            freeaddrinfo( pResult );
            return AF_INET;
        }
        if(pTempres->ai_family == AF_INET6)
        {
            ipv6 = (struct sockaddr_in6 *)pTempres->ai_addr;
            ipaddr = &(ipv6->sin6_addr);
            inet_ntop(AF_INET6, ipaddr, ipaddrstr, INET6_ADDRSTRLEN);
            ipaddrstr[INET6_ADDRSTRLEN-1]='\0';
            memcpy(pResIPaddr, ipaddrstr, INET6_ADDRSTRLEN);
            freeaddrinfo( pResult );
            return AF_INET6;
        }
    }
    freeaddrinfo( pResult );
    return -1;
}

/**
 *@fn getFullyQualifiedDName
 *@brief This function is invoked to Get Fully Qualified Domain Name
 *@param fqdname - pointer to string of fqdname
 *@param EthIndex - Index value of LAN channel.
 *@return Returns length of fqdn on success and 0 on fails
 */

int getFullyQualifiedDName(char *fqdname, INT8U EthIndex)
{
    HOSTNAMECONF HostnameConfig;
    DOMAINCONF DomainConfig;
    DNSCONF DnsIPConfig;
    INT8U regBMC_FQDN[MAX_CHANNEL];
    int fqdnlen = 0;

    memset(&HostnameConfig, 0, sizeof(HostnameConfig));
    memset(&DomainConfig, 0, sizeof(DomainConfig));
    memset(&DnsIPConfig, 0, sizeof(DnsIPConfig));
    memset(&regBMC_FQDN, 0, sizeof(regBMC_FQDN));

    nwGetAllDNSConf(&HostnameConfig, &DomainConfig, &DnsIPConfig, regBMC_FQDN);

    if((regBMC_FQDN[EthIndex] & REG_BMC_FQDN) == REG_BMC_FQDN)
    {
        fqdnlen = HostnameConfig.HostNameLen + DomainConfig.domainnamelen + 2;
        snprintf(fqdname, fqdnlen, "%s.%s", HostnameConfig.HostName, DomainConfig.domainname);
        return fqdnlen;
    }
    else
    {
        fqdnlen = HostnameConfig.HostNameLen + 1;
        snprintf(fqdname, fqdnlen, "%s", HostnameConfig.HostName);
        return fqdnlen;
    }
    return 0;
}
