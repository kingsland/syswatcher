/****************************************************************
 ****************************************************************
 **                                                            **
 **    (C)Copyright 2005-2006, American Megatrends Inc.        **
 **                                                            **
 **            All Rights Reserved.                            **
 **                                                            **
 **        6145-F, Northbelt Parkway, Norcross,                **
 **                                                            **
 **        Georgia - 30071, USA. Phone-(770)-246-8600.         **
 **                                                            **
 ****************************************************************
 ****************************************************************/
/*****************************************************************
 *
 * LANIfc.c
 * LAN Interface Handler
 *
 * Author: Govind Kothandapani <govindk@ami.com>
 *       : Bakka Ravinder Reddy <bakkar@ami.com>
 *
 *****************************************************************/
#define _DEBUG_
#define ENABLE_DEBUG_MACROS   0
#include <calltrace.h>
#include <defs.h>
#include "Types.h"
#include "OSPort.h"
#include "IPMI_Main.h"
#include "MsgHndlr.h"
#include "Support.h"
#include "SharedMem.h"
#include "IPMI_LANIfc.h"
#include "IPMI_RMCP.h"
#include "Debug.h"
#include "PMConfig.h"
#include "RMCP.h"
#include "LANIfc.h"
#include "IPMIDefs.h"
#include "RMCP.h"
//#include "NVRAccess.h"
#include "Util.h"
//#include "MD.h"
#include "nwcfg.h"
#include <errno.h>
#include "Ethaddr.h"
#include "IPMIConf.h"
//#include "PDKAccess.h"
#include "ncml.h"
#include <netdb.h>        /* getaddrinfo(3) et al.                       */
#include <netinet/in.h>   /* sockaddr_in & sockaddr_in6 definition.      */
#include  <net/if.h>
#include <sys/prctl.h>
#include "featuredef.h"

#define NO_OF_RETRY                     3
#define MAX_POSSIBLE_IPMI_DATA_SIZE     1024*17
#define MAX_LAN_BUFFER_SIZE             1024 * 60
#define LAN_TIMER_INTERVAL              10

#define RMCP_CLASS_MSG_OFFSET           3
#define IPMI_MSG_AUTH_TYPE_OFFSET       4
#define RMCP_ASF_PING_MESSAGE_LENGTH    12
#define IPMI_MSG_LEN_OFFSET             13
#define IPMI20_MSG_LEN_OFFSET           14
#define RMCP_CLASS_MSG_ASF              0x06
#define RMCP_CLASS_MSG_IPMI             0x07


/**
 *@fn InitUDPSocket
 *@brief This function is invoked to initialize LAN udp sockets
 */
static int      InitUDPSocket (int EthIndex, int BMCInst);
static int      InitTCPSocket (int EthIndex, int BMCInst);
//static int      InitSocket (int BMCInst);
static void*    LANTimer (void*);
static void*    RecvLANPkt (void*);
static int      SendLANPkt (MsgPkt_T *pRes,int BMCInst);
static void     ProcessLANReq (_NEAR_ MsgPkt_T* pReq, MiscParams_T *pParams, int BMCInst);
static void     ProcessBridgeMsg (_NEAR_ MsgPkt_T* pReq, int BMCInst);
static int      SetIPv6Header (int socketID, int ethIndex,int BMCInst);
static int      SetIPv4Header (int socketID, int ethIndex,int BMCInst);

/**
 * @fn ConfigureVLANSocket
 * @brief configures vlan udp sockets
 * @return   1 if success, -1 if failed.
 **/
static int ConfigureVLANSocket (int Index,int BMCInst);

/*
 * @fn ConfigureBondSocket
 * @brief configures Bond udp sockets
 * @return 1 if success, -1 if failed.
 */
static int ConfigureSocket(int Ethindex,int BMCInst);
static int CheckInterfacePresence(char* ifcname);
/*
 * @fb DeConfigureBONDSocket
 * @brief Deconfigures Bond udp Sockets
 * @return 1 if success, -1 if failed.
 */
static int DeConfigureSocket(int Ethindex,int BMCInst);

static int ClearSocketTable (int BMCInst);

/****************************Our additions for timing out connections****************************/
/*We need a way to timeout connect calls faster in case of wrong email server ip entered
  so I created a connect_tmout function that gives this granularity
  we don't want to spend too much time in test alert etc waiting for connection timeouts*/

static void SetSocketNonBlocking(int s)
{
    int flags;

    flags = fcntl(s, F_GETFL, 0);
    if(flags == -1)
    {
        flags = 0;
    }

    flags |= O_NONBLOCK;

    flags = fcntl(s, F_SETFL, flags);
    if(flags == -1)
    {
        TCRIT("Could not set socket to non blocking!!\n");
    }

    return;
}

/*
 *@fn UpdateNetStateChange
 *@brief Updates the change in network state change
 */
int UpdateLANStateChange(int BMCInst)
{
    int up_count = 0,i,j,count=0,ifcupdated=0;
    BMCInfo_t *pBMCInfo = &g_BMCInfo[BMCInst];
    char *EthIfcname=NULL,ifname[MAX_ETHIFC_LEN];
    LANIFCConfig_T PrevLanIfcConfig[MAX_LAN_CHANNELS];

    if(get_network_interface_count(&count) < 0)
    {
        return -1;
    }

    EthIfcname = malloc(MAX_ETHIFC_LEN * count);
    if(EthIfcname == NULL)
    {
        TCRIT("LANIfc.c : Error in allocating memory \n");
        return 0;
    }

    memset(EthIfcname,0,MAX_ETHIFC_LEN * count);

    for(i=0;i<MAX_LAN_CHANNELS;i++)
    {

        for(j=0;j<count;j++)
        {
            if((strcmp(pBMCInfo->LANConfig.LanIfcConfig[i].ifname,&EthIfcname[j*MAX_ETHIFC_LEN]) == 0)
                    && (strlen(pBMCInfo->LANConfig.LanIfcConfig[i].ifname) != 0))
            {
                pBMCInfo->LANConfig.LanIfcConfig[i].Up_Status = LAN_IFC_UP;
                pBMCInfo->LANConfig.LanIfcConfig[i].Enabled = TRUE;
                ifcupdated = FLAG_SET;
            }
        }
    }
    free(EthIfcname);

    for(j=0;j <MAX_LAN_CHANNELS;j++)
    {
        if((pBMCInfo->LANConfig.LanIfcConfig[j].Enabled == TRUE) 
                && (pBMCInfo->LANConfig.LanIfcConfig[j].Up_Status == LAN_IFC_UP))
        {
            if( pBMCInfo->LANConfig.LANIFcheckFlag[j] == 0)
            {
                if(ConfigureSocket(j,BMCInst) != 1)
                {
                    TCRIT("Error in creating Bond Socket\n");
                }
                /*Set the LANInterface Enabled flag*/
                pBMCInfo->LANConfig.LANIFcheckFlag[j] = 1;
            }

            if(pBMCInfo->IpmiConfig.VLANIfcSupport == 1)
            {
                /*Configure the VLAN Sockets*/
                if(pBMCInfo->LANConfig.VLANID[j] != 0)
                {
                    memset(ifname,0,sizeof(ifname));
                    sprintf(ifname,"%s.%d",pBMCInfo->LANConfig.LanIfcConfig[j].ifname,pBMCInfo->LANConfig.VLANID[j]);
                    if(pBMCInfo->LANConfig.VLANIFcheckFlag[j] == 0)
                    {
                        if(CheckInterfacePresence(ifname) != -1)
                        {
                            if( 1 != ConfigureVLANSocket(j,BMCInst))
                            {
                                IPMI_WARNING ("VLAN socket configuration Failed");
                            }
                        }
                    }
                }
            }
        }

        if((pBMCInfo->LANConfig.LanIfcConfig[j].Enabled == TRUE) 
                && (pBMCInfo->LANConfig.LanIfcConfig[j].Up_Status == LAN_IFC_DOWN))
        {
            if( pBMCInfo->LANConfig.LANIFcheckFlag[j] != 0)
            {
                if(DeConfigureSocket(j,BMCInst) != 0)
                {
                    TCRIT("Error in deconfiguring bond socket\n");
                }
                pBMCInfo->LANConfig.LANIFcheckFlag[j] = 0;
            }

            if(pBMCInfo->IpmiConfig.VLANIfcSupport == 1)
            {
                /*Deconfigure the VLAN Sockets*/
                if(pBMCInfo->LANConfig.VLANIFcheckFlag[j] == 1)
                {
                    if(pBMCInfo->LANConfig.VLANUDPSocket[j] != -1)
                    {
                        close(pBMCInfo->LANConfig.VLANUDPSocket[j]);
                        pBMCInfo->LANConfig.VLANUDPSocket[j] = -1;
                        pBMCInfo->LANConfig.VLANIFcheckFlag[j] = 0;
                    }
                    if(pBMCInfo->LANConfig.VLANTCPSocket[j] != -1)
                    {
                        shutdown(pBMCInfo->LANConfig.VLANTCPSocket[j],SHUT_RDWR);
                        close(pBMCInfo->LANConfig.VLANTCPSocket[j]);
                        pBMCInfo->LANConfig.VLANTCPSocket[j] = -1;
                    }
                }
            }
        }
    }
    return 0;
}


/*
 *@fn LANMonitor
 *@brief Thread that monitors the state change in network devices and notifies   
 *          to initialize sockets based on NCML configurations and state change in network devices
 */
void *LANMonitor(void *pArg)
{
    int    *inst   = (int*)pArg;
    int    BMCInst = *inst;
    int Tonotify;
    prctl(PR_SET_NAME,__FUNCTION__,0,0,0);
    while(1)
    {
        /* Wakes up when there is state change in network devices */
        //        wait_network_state_change();

        POST_TO_Q(&Tonotify, sizeof(int), LAN_MON_Q, &Err, BMCInst);
    }

}

/**
 * @brief LAN Interface Task.
 **/
void *LANIfcTask (void *pArg)
{
    MsgPkt_T    Req;
    int BMCInst;
    int i;
    int BMC;
    int Buffer  = 0;
    //INT8U     ICTSMode;
    BMCInst = *(int *)pArg;
    prctl(PR_SET_NAME,__FUNCTION__,0,0,0);

    char    keyInstance[MAX_STR_LENGTH];
    _FAR_   BMCInfo_t   *pBMCInfo = &g_BMCInfo[BMCInst];

    memset(keyInstance,0,MAX_STR_LENGTH);
    pBMCInfo->LANConfig.DeleteThisLANSessionID = 0;

#if 0
    if(g_PDKHandle[PDK_ONTASKSTARTUP] != NULL)
    {
        ((void(*)(INT8U,int))g_PDKHandle[PDK_ONTASKSTARTUP]) (LAN_IFC_TASK_ID,BMCInst);
    }
#endif

    /* Init LAN SMB */
    IPMI_DBG_PRINT ("LANIfc Started \n");

    OS_CREATE_Q (LAN_IFC_Q, Q_SIZE, BMCInst);
    OS_GET_Q(LAN_IFC_Q, O_RDWR, BMCInst);
    OS_CREATE_Q (LAN_RES_Q, Q_SIZE, BMCInst);
    OS_GET_Q(LAN_RES_Q, O_RDWR, BMCInst);
    /* Queue needed to monitor network device state change */
    OS_CREATE_Q(LAN_MON_Q,Q_SIZE,BMCInst);
    OS_GET_Q(LAN_MON_Q,O_RDWR,BMCInst);

    /* Get the Queue handle */

    if ( pBMCInfo->IpmiConfig.VLANIfcSupport == 1)
    {
        OS_CREATE_Q (VLAN_IFC_Q, Q_SIZE, BMCInst);
        OS_GET_Q (VLAN_IFC_Q, O_RDWR, BMCInst);
    }


    pBMCInfo->pSocketTbl = (SocketTbl_T*) malloc(sizeof(SocketTbl_T)*(pBMCInfo->IpmiConfig.MaxSession+1));
    if(pBMCInfo->pSocketTbl == NULL)
    {
        IPMI_ERROR("Error in allocating memory for Session Tbl \n");
    }

    /* Initialize the allocated memory to zero */
    memset(pBMCInfo->pSocketTbl, 0, sizeof(SocketTbl_T) *(pBMCInfo->IpmiConfig.MaxSession+1));
    BMC = BMCInst;
    /* Initialize the elements of VLAN socket array to -1 */ 
    memset(pBMCInfo->LANConfig.VLANUDPSocket, -1, sizeof(SOCKET) * MAX_LAN_CHANNELS); 

    memset(pBMCInfo->LANConfig.VLANTCPSocket, -1, sizeof(SOCKET) * MAX_LAN_CHANNELS); 

    memset(pBMCInfo->LANConfig.UDPSocket, -1,sizeof(SOCKET) * MAX_LAN_CHANNELS);

    memset(pBMCInfo->LANConfig.TCPSocket, -1,sizeof(SOCKET) * MAX_LAN_CHANNELS);

    memset(pBMCInfo->LANConfig.LANIFcheckFlag, 0,sizeof(int) * MAX_LAN_CHANNELS);

#if 0	
    if(ReadVLANFile(VLAN_ID_SETTING_STR, pBMCInfo->LANConfig.VLANID) == -1)
    {
        //return -1;
    }
#endif

    BMCInst = BMC;

    /* Retries can be added */
    UpdateLANStateChange(BMCInst);

    /* Thread to monitor the network State Change*/
    OS_CREATE_THREAD(LANMonitor,(void*)&BMCInst,&Err);
    /* Create a thread to receive LAN Packets */
    OS_CREATE_THREAD (RecvLANPkt, (void*)&BMCInst, &Err);

    /* Create a thread to handle socket timeout */
    OS_CREATE_THREAD (LANTimer, (void*)&BMCInst, &Err);

    OS_THREAD_MUTEX_RELEASE(&pBMCInfo->ThreadSyncMutex);

    /* Start with ICTS mode disabled */
    //ICTSMode = 0;

    while (1)
    {
        MiscParams_T    Params={0,0};
        /* Wait for a message in LANIfc interface Queue */
        if (0 != GetMsg (&Req, LAN_IFC_Q, WAIT_INFINITE, BMCInst))
        {
            IPMI_ERROR ("LANIfc.c : Error fetching message from hLANIfc_Q\n");
            continue;
        }
        switch (Req.Param)
        {
            case LAN_SMB_REQUEST :
                ProcessLANReq (&Req, &Params, BMCInst);
                break;

            case BRIDGING_REQUEST :
                IPMI_DBG_PRINT("BRIDGING_REQUEST"); /*garden*/
                ProcessBridgeMsg (&Req, BMCInst);
                break;

            case LAN_ICTS_MODE :
                //ICTSMode = Req.Cmd;
                break;

            case VLAN_SMB_REQUEST:
                Params.IsPktFromVLAN = TRUE;
                IPMI_DBG_PRINT ("Process VLAN SMB request\n");
                ProcessLANReq (&Req, &Params, BMCInst);
                break;

            case LOOP_BACK_LAN_SMB_REQUEST:
                Params.IsPktFromLoopBack = TRUE;
                IPMI_DBG_PRINT ("Loop Back LAN SMB request\n");
                ProcessLANReq (&Req, &Params, BMCInst);
                break;

            case LAN_CONFIG_IPV4_HEADER:

                IPMI_DBG_PRINT("LAN_CONFIG_IPV4_HEADER"); /*garden*/
                if(g_corefeatures.global_ipv6  == ENABLED)
                    SetIPv6Header(Req.Socket, Req.Channel,BMCInst);
                else
                    SetIPv4Header(Req.Socket, Req.Channel,BMCInst);


                break;

            case LAN_RMCP_PORT_CHANGE:
                IPMI_DBG_PRINT("LAN_RMCP_PORT_CHANGE"); /*garden*/
                //                if(ReadVLANFile(VLAN_ID_SETTING_STR, pBMCInfo->LANConfig.VLANID)==-1)
                {
                    for(i=0; i<MAX_LAN_CHANNELS; i++)
                    {
                        if(pBMCInfo->LANConfig.UDPSocket[i] != -1)
                        {
                            close(pBMCInfo->LANConfig.UDPSocket[i]);
                            pBMCInfo->LANConfig.UDPSocket[i]=-1;
                        }
                        if((pBMCInfo->LANConfig.LanIfcConfig[i].Enabled == TRUE) 
                                && (pBMCInfo->LANConfig.LanIfcConfig[i].Up_Status == LAN_IFC_UP))
                        {
                            if(InitUDPSocket(i, BMCInst) == -1)
                            {
                                IPMI_WARNING(" InitUDPSocket() : Error initializing UDP Sockets for LAN \n");
                            }
                        }
                    }
                }
                POST_TO_Q(&Buffer, sizeof(int), VLAN_IFC_Q, &Err, BMCInst);
                break;

            default :
                IPMI_WARNING ("LANIfc.c : Invalid request\n");
                break;
        }
    }

    OS_TASK_RETURN;
}


/**
 * @brief CheckInterfacePresence
 **/
    static int
CheckInterfacePresence (char *Ifc)
{
    int     r;
    int     skfd;
    struct  ifreq   ifr;
    unsigned char   tmp[MAC_ADDR_LEN];

    IPMI_DBG_PRINT_1 ("Checking the presence of %s\n", Ifc);

    skfd = socket(PF_INET, SOCK_DGRAM, 0);
    if ( skfd < 0 )
    {
        IPMI_ERROR("can't open socket: %s\n",strerror(errno));
        return -1;
    }

    /* Get MAC address */
    memset(&ifr, 0, sizeof(struct ifreq));
    memset(tmp, 0, MAC_ADDR_LEN);
    strcpy(ifr.ifr_name, Ifc);
    ifr.ifr_hwaddr.sa_family = AF_INET;
    r = ioctl(skfd, SIOCGIFHWADDR, &ifr);
    close (skfd);
    if ( r < 0 )
    {
        IPMI_ERROR("IOCTL to get MAC failed: %d\n",r);
        return -1;
    }
    IPMI_DBG_PRINT_1 (" %s Interface is present\n", Ifc);

    return 0;
}

/**
 *@fn InitUDPSocket
 *@brief This function is invoked to initialize LAN udp sockets
 *@return Returns 0 on success
 */
int InitUDPSocket(int EthIndex, int BMCInst)
{
    struct  sockaddr_in6   Local6;
    int  v6Only = 0;
    struct  sockaddr_in   Local;
    char    UDPIfcName [MAX_STR_LENGTH];
    int     reuseaddr=1;
    BOOL    bWarned;
    _FAR_   BMCInfo_t   *pBMCInfo = &g_BMCInfo[BMCInst];
    //int EthIndexName;
    int ethindex = pBMCInfo->LANConfig.LanIfcConfig[EthIndex].Ethindex;
    int ret=0;

    if(pBMCInfo->LANConfig.UDPSocket[EthIndex] != -1)
    {
        close(pBMCInfo->LANConfig.UDPSocket[EthIndex]);
        pBMCInfo->LANConfig.UDPSocket[EthIndex] = -1;
    }

    memset(UDPIfcName,0,sizeof(UDPIfcName));

    if(g_corefeatures.global_ipv6  == ENABLED) 
    {
        Local6.sin6_family = AF_INET6;
        inet_pton(AF_INET6, "0::0", &Local6.sin6_addr);
        Local6.sin6_port = htons(pBMCInfo->LANCfs[ethindex].PrimaryRMCPPort);
        Local6.sin6_flowinfo=0;
        Local6.sin6_scope_id=0;

        pBMCInfo->LANConfig.UDPSocket[EthIndex] = socket ( AF_INET6, SOCK_DGRAM, 0);
    }
    else
    {
        Local.sin_family        = AF_INET;
        Local.sin_addr.s_addr   = INADDR_ANY;

        Local.sin_port = htons(pBMCInfo->LANCfs[ethindex].PrimaryRMCPPort);

        /* Initialize The Socket */
        pBMCInfo->LANConfig.UDPSocket[EthIndex] = socket ( AF_INET, SOCK_DGRAM, 0 );
    }
    if ( pBMCInfo->LANConfig.UDPSocket[EthIndex] == -1)
    {
        IPMI_ERROR ("LANIfc.c : Unable to create socket\n");
        return  -1;
    }
    IPMI_DBG_PRINT_1 ("UDP Socket = 0x%x\n", pBMCInfo->LANConfig.UDPSocket[EthIndex]);

    strcpy(UDPIfcName,pBMCInfo->LANConfig.LanIfcConfig[EthIndex].ifname);

    if (0 != setsockopt (pBMCInfo->LANConfig.UDPSocket[EthIndex], SOL_SOCKET,SO_BINDTODEVICE, UDPIfcName, sizeof (UDPIfcName)+1))
    {
        IPMI_DBG_PRINT ("LANIfc.c: SetSockOpt Failed for UDP Socket");
        return -1;
    }
    if (0 != setsockopt(pBMCInfo->LANConfig.UDPSocket[EthIndex], SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)))
    {
        IPMI_ERROR("LANIfc.c: Setsockopt(SO_REUSEADDR) Failed for UDP socket\n");
    }

    if(g_corefeatures.global_ipv6  == ENABLED)
    {
#if defined( IPV6_V6ONLY )
        setsockopt( pBMCInfo->LANConfig.UDPSocket[EthIndex], IPPROTO_IPV6, IPV6_V6ONLY, &v6Only,sizeof( v6Only ) );
#endif
        SetIPv6Header(pBMCInfo->LANConfig.UDPSocket[EthIndex], ethindex, BMCInst);
    }
    else
    {
        SetIPv4Header(pBMCInfo->LANConfig.UDPSocket[EthIndex], ethindex, BMCInst);
    }


    bWarned = FALSE;
    while(1)
    {
        if(g_corefeatures.global_ipv6  == ENABLED)
        {
            ret = bind(pBMCInfo->LANConfig.UDPSocket[EthIndex], (struct sockaddr *)&Local6, sizeof(Local6) );
        }
        else
        {
            ret = bind(pBMCInfo->LANConfig.UDPSocket[EthIndex], (struct sockaddr *)&Local, sizeof(Local) );
        }
        if (ret == -1)
        {
            if( EADDRINUSE == errno )
            {
                IPMI_WARNING("LANIfc.c : [%u] Warning binding socket, %d, %s\n", getpid(), errno, strerror(errno));
                sleep(2);
                bWarned = TRUE;
            }
            else
            {
                IPMI_ERROR  ("LANIfc.c : [%u] Error binding socket, %d, %s\n", getpid(), errno, strerror(errno));
                break;
            }
        }
        else
        {
            if( bWarned )
            {
                IPMI_WARNING("LANIfc.c : [%u] socket bound.\n", getpid());
            }
            else
            {
                IPMI_DBG_PRINT_1("LANIfc.c : [%u] socket bound.\n", getpid());
            }
            break;
        }
    }
    IPMI_DBG_PRINT_2("\n Socket Created for %x and Socket ID :%x\n", EthIndex, pBMCInfo->LANConfig.UDPSocket[EthIndex]);
    pBMCInfo->LANConfig.UDPChannel[EthIndex] = pBMCInfo->LANConfig.LanIfcConfig[EthIndex].Chnum;
    return 0;
}

/**
 *@fn InitTCPSocket
 *@brief This function is invoked to initialize LAN udp sockets
 *@return Returns 0 on success
 */
int InitTCPSocket(int EthIndex, int BMCInst)
{
    int     reuseaddr1 = 1;
    struct  sockaddr_in6   Local6;
    int  v6Only = 0;
    struct  sockaddr_in   Local;
    BOOL    bWarned;
    char ethname[MAX_ETHIFC_LEN];
    _FAR_   BMCInfo_t   *pBMCInfo = &g_BMCInfo[BMCInst];
    int ret = 0;

    if(pBMCInfo->LANConfig.TCPSocket[EthIndex] != -1)
    {
        shutdown(pBMCInfo->LANConfig.TCPSocket[EthIndex],SHUT_RDWR);
        close(pBMCInfo->LANConfig.TCPSocket[EthIndex]);
        pBMCInfo->LANConfig.TCPSocket[EthIndex] = -1;
    }
    if(g_corefeatures.global_ipv6  == ENABLED) 
    {
        Local6.sin6_family = AF_INET6;
        inet_pton(AF_INET6, "0::0", &Local6.sin6_addr);
        Local6.sin6_port = htons(LAN_RMCP_PORT);
        Local6.sin6_flowinfo=0;
        Local6.sin6_scope_id=0;
        pBMCInfo->LANConfig.TCPSocket[EthIndex] = socket ( AF_INET6, SOCK_STREAM, 0 );
    }
    else
    {
        Local.sin_family = AF_INET;
        Local.sin_addr.s_addr = INADDR_ANY;
        Local.sin_port = htons(LAN_RMCP_PORT);
        pBMCInfo->LANConfig.TCPSocket[EthIndex] = socket ( AF_INET, SOCK_STREAM, 0);
        if ( pBMCInfo->LANConfig.TCPSocket[EthIndex] == -1)
        {
            IPMI_ERROR ("LANIfc.c : Unable to create TCP socket\n");
            return -1;
        }
    }
    memset(ethname, 0, sizeof(ethname));
    strcpy(ethname, pBMCInfo->LANConfig.LanIfcConfig[EthIndex].ifname);

    /* Initialize The Socket */
    if(g_corefeatures.global_ipv6  == ENABLED)
    {
#if defined( IPV6_V6ONLY )
        setsockopt( pBMCInfo->LANConfig.TCPSocket[EthIndex], IPPROTO_IPV6, IPV6_V6ONLY, &v6Only,sizeof( v6Only ) );
#endif
    }

    if (0 != setsockopt (pBMCInfo->LANConfig.TCPSocket[EthIndex], SOL_SOCKET, SO_BINDTODEVICE, ethname, sizeof (ethname)+1))
    {
        IPMI_DBG_PRINT ("LANIfc.c: SetSockOpt Failed for UDP Socket");
        return -1;
    }

    if (0 != setsockopt(pBMCInfo->LANConfig.TCPSocket[EthIndex], SOL_SOCKET, SO_REUSEADDR, &reuseaddr1, sizeof(int)))
    {
        IPMI_ERROR("LANIfc.c: Setsockopt(SO_REUSEADDR) Failed for TCP socket\n");
    } 

    bWarned = FALSE;
    while(1)
    {
        if(g_corefeatures.global_ipv6  == ENABLED)
        {
            ret = bind(pBMCInfo->LANConfig.TCPSocket[EthIndex], (struct sockaddr *)&Local6, sizeof(Local6) );
        }
        else
        {
            ret = bind(pBMCInfo->LANConfig.TCPSocket[EthIndex], (struct sockaddr *)&Local, sizeof(Local) );
        }
        if (ret == -1)
        {
            if( EADDRINUSE == errno )
            {
                IPMI_WARNING("LANIfc.c : [%u] Warning binding socket, %d, %s\n", getpid(), errno, strerror(errno));
                sleep(2);
                bWarned = TRUE;
            }
            else
            {
                IPMI_ERROR  ("LANIfc.c : [%u] Error binding socket, %d, %s\n", getpid(), errno, strerror(errno));
                break;
            }
        }
        else
        {
            if( bWarned )
            {
                IPMI_WARNING("LANIfc.c : [%u] socket bound.\n", getpid());
            }
            else
            {
                IPMI_DBG_PRINT_1("LANIfc.c : [%u] socket bound.\n", getpid());
            }
            break;
        }
    }
    if (listen(pBMCInfo->LANConfig.TCPSocket[EthIndex], pBMCInfo->IpmiConfig.MaxSession) == -1)
    {
        IPMI_ERROR ("LANIfc.c : Error listen\n");
        return -1;
    }
    IPMI_DBG_PRINT_2("TCP Sockets Successful %d name %s\n",EthIndex,pBMCInfo->LANConfig.LanIfcConfig[EthIndex].ifname);
    pBMCInfo->LANConfig.TCPChannel[EthIndex] = pBMCInfo->LANConfig.LanIfcConfig[EthIndex].Chnum;
    return 0;
}

static int GetSocketInfoIndex(int Socket, int BMCInst)
{
    int i;
    _FAR_   BMCInfo_t   *pBMCInfo = &g_BMCInfo[BMCInst];
    SocketTbl_T *SocketTable = pBMCInfo->pSocketTbl;

    for (i = 0; i < (pBMCInfo->IpmiConfig.MaxSession + 1); i++)
    {
        if ((1 == SocketTable [i].Valid) && (SocketTable [i].Socket == Socket))
        {
            return i;
        }
    }
    return -1;
}

/**
 * @fn  AddSocket
 * @brief This function adds the new socket handle to socket table
 *
 * @param Socket - socket handle to be added.
 IsLoopBackSocket -If Socket is loopback. This will be true
 IsFixedSocket socket means, Socket which are created during InitSocket Process

 **/
    static int
AddSocket (int Socket,INT8U IsLoopBackSocket, INT8U IsFixedSocket, int BMCInst)
{
    int i;
    _FAR_   BMCInfo_t   *pBMCInfo = &g_BMCInfo[BMCInst];
    SocketTbl_T *SocketTable = pBMCInfo->pSocketTbl;

    for (i = 0; i < (pBMCInfo->IpmiConfig.MaxSession + 1); i++)
    {
        if (0 == SocketTable [i].Valid)
        {
            SocketTable [i].Socket = Socket;
            SocketTable [i].Valid  = 1;
            SocketTable [i].Time   = 0;
            SocketTable[i].IsLoopBackSocket= IsLoopBackSocket;
            SocketTable[i].IsFixedSocket=IsFixedSocket;
            return 0;
        }
    }

    IPMI_WARNING ("LANIfc.c : Error adding new socket to the table\n");
    return -1;
}

/**
 * @fn  RemoveSocket
 * @brief This function removes the socket from the socket table
 *
 * @param Socket - socket handle to be removed.
 **/
    static int
RemoveSocket (int Socket, int BMCInst)
{
    int i;
    _FAR_   BMCInfo_t   *pBMCInfo = &g_BMCInfo[BMCInst];
    SocketTbl_T *SocketTable = pBMCInfo->pSocketTbl;

    for (i = 0; i < (pBMCInfo->IpmiConfig.MaxSession + 1); i++)
    {
        if (Socket == SocketTable [i].Socket)
        {
            SocketTable [i].Socket = 0;
            SocketTable [i].Valid  = 0;
            return 0;
        }
    }

    IPMI_WARNING ("LANIfc.c : Error removing socket from the table\n");
    return -1;
}

/**
 * @fn UpdateTimeout
 * @brief This function handles the socket time out.
 *
 * @param None.
 **/
    static int
UpdateTimeout (int BMCInst)
{
    int i;
    int TimeOut=0;

    _FAR_    BMCInfo_t   *pBMCInfo = &g_BMCInfo[BMCInst];
    SocketTbl_T *SocketTable = pBMCInfo->pSocketTbl;
    for (i = 0; i < (pBMCInfo->IpmiConfig.MaxSession + 1); i++)
    {
        if(((SocketTable [i].Valid) && (!SocketTable [i].IsFixedSocket)) )
        {
            if(SocketTable[i].IsLoopBackSocket)
            {
                TimeOut=pBMCInfo->IpmiConfig.SessionTimeOut;
            }
            else
            {
                if(IPMITimeout > 0)
                {
                    TimeOut = (IPMITimeout +10);
                }
                else
                {
                    /*If it is not defined the timeout values for loop back session should be
                      SESSION_TIMEOUT defined in config.make.ipmi (60 seconds) */
                    TimeOut = pBMCInfo->IpmiConfig.SessionTimeOut;
                }
            }
            /* if timed out remove the socket from the table and close it.*/
#ifdef  CONFIGURABLE_SESSION_TIME_OUT
            if (SocketTable [i].Time >= BMC_GET_SHARED_MEM(BMCInst)->uSessionTimeout)
#else
                if (SocketTable [i].Time >= TimeOut)
#endif
                {
                    IPMI_WARNING ("Socket timed out\n");
                    close (SocketTable [i].Socket);
                    SocketTable [i].Socket = 0;
                    SocketTable [i].Valid  = 0;
                }
                else
                {   
                    /* update the time */
                    SocketTable [i].Time+=LAN_TIMER_INTERVAL;
                }
        }
    }
    return 0;
}

/**
 * @fn  SetFd
 * @brief This function sets the file descriptor.
 * @param fd
 **/
    static void
SetFd (fd_set *fd, int BMCInst)
{
    int i;
    int x=0;
    _FAR_   BMCInfo_t   *pBMCInfo = &g_BMCInfo[BMCInst];
    SocketTbl_T *SocketTable = pBMCInfo->pSocketTbl;

    FD_ZERO (fd);

    if( pBMCInfo->IpmiConfig.VLANIfcSupport == 1)
    {
        FD_SET (pBMCInfo->LANConfig.hVLANIfc_Info_Q, fd);
    }

    for(x=0;x<MAX_LAN_CHANNELS;x++)
    {
        if((pBMCInfo->LANConfig.LanIfcConfig[x].Enabled == TRUE)
                && (pBMCInfo->LANConfig.LanIfcConfig[x].Up_Status == LAN_IFC_UP))
        {
            if(pBMCInfo->LANConfig.TCPSocket[x] != -1)
                FD_SET (pBMCInfo->LANConfig.TCPSocket[x], fd);
            if(pBMCInfo->LANConfig.VLANTCPSocket[x] != -1)
                FD_SET(pBMCInfo->LANConfig.VLANTCPSocket[x],fd);
        }
    } 

    for (i = 0; i < (pBMCInfo->IpmiConfig.MaxSession + 1); i++)
    {
        /*printf("SocketTable [%d].Valid=%d,SocketTable [%d].Socket=%d,LANConfig.hLANMon_Q=%lx\n",
          i,SocketTable [i].Valid,
          i,SocketTable [i].Socket,
          pBMCInfo->LANConfig.hLANMon_Q);*/
        if (1 == SocketTable [i].Valid)
        {
            FD_SET (SocketTable [i].Socket, fd);
        }
    }

    ///*test*/    FD_SET(pBMCInfo->LANConfig.hLANMon_Q,fd);

}
/**
 * @fn  ReadData
 * @brief This function receives the IPMI LAN request packets
 *
 * @param pMsgPkt - pointer to message packet
 * @param Socket  - Socket handle
 **/
    static
int  ReadData (MsgPkt_T *pMsgPkt, int Socket, int BMCInst)
{
    unsigned int    SourceLen = 0,Index,SocktIndex=0,addrlen=0;
    int Channel = 0;
    INT8U   *pData      = pMsgPkt->Data;
    INT16S  Len         = 0;
    INT16U  RecvdLen    = 0;
    INT16U  IPMIMsgLen  = 0;
    INT16U  IPMIMsgOffset   = 0;
    struct  sockaddr_in6 Sourcev6;
    struct  sockaddr_in6 server_v6addr;
    struct  sockaddr_in Sourcev4;
    struct  sockaddr_in server_v4addr;
    void *Source = NULL ;
    void *server_addr = NULL ;

    _FAR_    BMCInfo_t   *pBMCInfo = &g_BMCInfo[BMCInst];
    SocketTbl_T *SocketTable = pBMCInfo->pSocketTbl;
    if(g_corefeatures.global_ipv6  == ENABLED)
    {
        Source = &Sourcev6;
        SourceLen = sizeof (Sourcev6);
    }
    else
    {
        Source = &Sourcev4;
        SourceLen = sizeof (Sourcev4);
    }
    //SourceLen = sizeof (Source);
    /* Read minimum bytes to find class of message */
    while (RecvdLen < RMCP_CLASS_MSG_OFFSET)
    {
        Len = recvfrom (Socket, &pData[RecvdLen], MAX_LAN_BUFFER_SIZE, 0, (struct sockaddr *)Source, &SourceLen);
        if ((Len >= -1) && (Len <= 0))
        {
            return -1;
        }
        RecvdLen += (INT16U)Len;
    }


    /*  if RMCP Presence Ping Requested */
    if (RMCP_CLASS_MSG_ASF == pData[RMCP_CLASS_MSG_OFFSET])
    {
        /* Read remaining RMCP ASF ping message */
        while (RecvdLen < RMCP_ASF_PING_MESSAGE_LENGTH)
        {
            Len = recvfrom (Socket, &pData[RecvdLen], MAX_LAN_BUFFER_SIZE, 0, (struct sockaddr *)Source, &SourceLen);
            if ((Len >= -1) && (Len <= 0))
            {
                return -1;
            }
            RecvdLen += (INT16U)Len;
        }
    }
    /*else if IPMI RMCP request */
    else if (RMCP_CLASS_MSG_IPMI == pData[RMCP_CLASS_MSG_OFFSET])
    {
        /* Read minimum no of bytes for IPMI Auth type offset*/
        while (RecvdLen < IPMI_MSG_AUTH_TYPE_OFFSET)
        {
            Len = recvfrom (Socket, &pData[RecvdLen], MAX_LAN_BUFFER_SIZE, 0,(struct sockaddr *)Source, &SourceLen);
            if ((Len >= -1) && (Len <= 0))
            {
                return -1;
            }
            RecvdLen += (INT16U)Len;
        }
        /* Get the IPMI message length offset based on format/authentication type */
        if (pData [IPMI_MSG_AUTH_TYPE_OFFSET] == RMCP_PLUS_FORMAT)
        {
            IPMIMsgOffset = IPMI20_MSG_LEN_OFFSET + 1;
        }
        else if (pData [IPMI_MSG_AUTH_TYPE_OFFSET] == AUTH_TYPE_NONE)
        {
            IPMIMsgOffset = IPMI_MSG_LEN_OFFSET;
        }
        else
        {
            IPMIMsgOffset = IPMI_MSG_LEN_OFFSET + AUTH_CODE_LEN;
        }

        /* Read minimum no of bytes for IPMI message length offset*/
        while (RecvdLen < IPMIMsgOffset)
        {
            Len = recvfrom (Socket, &pData[RecvdLen], MAX_LAN_BUFFER_SIZE, 0,(struct sockaddr *)Source, &SourceLen);
            if ((Len >= -1) && (Len <= 0))
            {
                return -1;
            }
            RecvdLen += (INT16U)Len;
        }

        /* Get the IPMI message length based on RMCP format type */
        if (pData [IPMI_MSG_AUTH_TYPE_OFFSET] == RMCP_PLUS_FORMAT)
        {
            IPMIMsgLen = ipmitoh_u16 (*((INT16U*)&pData [IPMI20_MSG_LEN_OFFSET]));
        }
        else
        {
            IPMIMsgLen = pData [IPMIMsgOffset];
        }
        /* We are assuming that we cannot get more than 17 K data in IPMI Msg */
        /* This work around for fix the malformed IPMI Msg length */

        if(IPMIMsgOffset > MAX_POSSIBLE_IPMI_DATA_SIZE )
        {
            return -1;
        }
        /* Read the remaining IPMI message packets */
        while (RecvdLen < IPMIMsgLen)
        {
            Len = recvfrom (Socket, &pData[RecvdLen], MAX_LAN_BUFFER_SIZE, 0,(struct sockaddr *)Source, &SourceLen);
            if ((Len >= -1) && (Len <= 0))
            {
                if(Len == -1)
                {
                    if(errno == EAGAIN)
                    {
                        pBMCInfo->LANConfig.WaitCount--;
                    }
                    else
                        return -1;
                }
                else
                    return -1;
            }
            if(pBMCInfo->LANConfig.WaitCount == 0)
            {
                return -1;
            }
            RecvdLen += (INT16U)Len;
        }
    }/* else other RMCP class are not supported. */
    else
    {
        IPMI_WARNING ("Unknown RMCP class\n");
    }

    pMsgPkt->Size     = RecvdLen;
    if(g_corefeatures.global_ipv6  == ENABLED)
    {
        pMsgPkt->UDPPort  = ((struct sockaddr_in6 *)Source)->sin6_port;
    }
    else
    {
        pMsgPkt->UDPPort  = ((struct sockaddr_in *)Source)->sin_port;
    }

    pMsgPkt->Socket   = Socket;

    for(Index=0;Index<MAX_LAN_CHANNELS;Index++)
    {
        if(Socket == pBMCInfo->LANConfig.UDPSocket[Index])
        {
            pMsgPkt->Channel = pBMCInfo->LANConfig.UDPChannel[Index];
            break;
        }
    }

    if(SourceLen!=0) 
    {
        if(g_corefeatures.global_ipv6  == ENABLED)
        {
            memcpy (pMsgPkt->IPAddr, &((struct sockaddr_in6 *)Source)->sin6_addr, sizeof (struct in6_addr));
        }
        else
        {
            memcpy (pMsgPkt->IPAddr, &((struct sockaddr_in *)Source)->sin_addr.s_addr, sizeof (struct in_addr));
        }
    }

    pMsgPkt->Param    = LAN_SMB_REQUEST;

    if (pBMCInfo->IpmiConfig.VLANIfcSupport == 1)
    {
        for(Index=0; Index < MAX_LAN_CHANNELS; Index++)
        {
            if (Socket == pBMCInfo->LANConfig.VLANUDPSocket[Index])
            {
                pMsgPkt->Param   = VLAN_SMB_REQUEST;
                pMsgPkt->Channel = pBMCInfo->LANConfig.UDPChannel[Index];
                return 0;
            }
        }
    }

    SocktIndex=GetSocketInfoIndex(Socket, BMCInst);
    if(SocktIndex < 0)
    {
        IPMI_DBG_PRINT("\nUnable Find the Socket index\n");
        return 0;
    }

    /* For External TCP socket We have to fill the LANChannel properly */
    if(!SocketTable[SocktIndex].IsFixedSocket)
    {
        if(g_corefeatures.global_ipv6  == ENABLED)
        {
            addrlen = sizeof(server_v6addr);
            server_addr = &server_v6addr;
        }
        else
        {
            addrlen = sizeof(server_v4addr);
            server_addr = &server_v4addr;
        }

        if (0 != getsockname(Socket, (struct sockaddr *)server_addr, &addrlen))
        {
            IPMI_ERROR ("Getting TCP Server address failed\n");
            /* This will be considered UDP or VLAN UDP Packet */
            return 0 ;
        }
        if(g_corefeatures.global_ipv6  == ENABLED)
        {
            if(IN6_IS_ADDR_V4MAPPED(&server_v6addr.sin6_addr))
            {
                Channel = GetChannelByAddr((char*)&((struct sockaddr_in6 *)server_addr)->sin6_addr.s6_addr[IP6_ADDR_LEN-IP_ADDR_LEN], BMCInst);
            }
            else
            {
                Channel = GetChannelByIPv6Addr((char *)&((struct sockaddr_in6 *)server_addr)->sin6_addr, BMCInst);
            }
        }
        else
        {
            Channel= GetChannelByAddr((char*)&((struct sockaddr_in *)server_addr)->sin_addr,BMCInst);
        }

        if(Channel >= 0)
        {
            pMsgPkt->Channel = Channel;
        }
        else
        {
            pMsgPkt->Param      = LOOP_BACK_LAN_SMB_REQUEST;
            pMsgPkt->Channel    = 0xFF;// GetLANChannel(0);
        }
    }

    return 0;
}

int InitializeSocket(fd_set *fdRead,unsigned int *n,int BMCInst)
{
    int x=0;
    _FAR_   BMCInfo_t   *pBMCInfo = &g_BMCInfo[BMCInst];

    ClearSocketTable(BMCInst);
    *n=0;

    for(x=0; x < MAX_LAN_CHANNELS; x++)
    {
        if((pBMCInfo->LANConfig.LanIfcConfig[x].Enabled == TRUE)
                && (pBMCInfo->LANConfig.LanIfcConfig[x].Up_Status == LAN_IFC_UP))
        {
            if (pBMCInfo->LANConfig.VLANIFcheckFlag[x] == 0)
            {
                if(pBMCInfo->LANConfig.UDPSocket[x] != -1)
                {
                    *n = (UTIL_MAX (*n, pBMCInfo->LANConfig.UDPSocket[x])) ;
                    /* Adding UDP Socket to the table */
                    AddSocket (pBMCInfo->LANConfig.UDPSocket[x], FALSE, TRUE, BMCInst);
                    SetSocketNonBlocking(pBMCInfo->LANConfig.UDPSocket[x]);
                }
            }
            else if((pBMCInfo->IpmiConfig.VLANIfcSupport == 1) && 
                    (pBMCInfo->LANConfig.VLANIFcheckFlag[x] == 1))
            {
                if(pBMCInfo->LANConfig.VLANUDPSocket[x] != -1)
                {
                    *n = (UTIL_MAX (*n, pBMCInfo->LANConfig.VLANUDPSocket[x])) ;
                    /* Adding VLAN UDP Socket to the table*/
                    AddSocket (pBMCInfo->LANConfig.VLANUDPSocket[x], TRUE, TRUE, BMCInst);
                }
            }

            if(pBMCInfo->LANConfig.TCPSocket[x] != -1)
            {
                *n = (UTIL_MAX (*n, pBMCInfo->LANConfig.TCPSocket[x])) ;
            }
            if(pBMCInfo->LANConfig.VLANTCPSocket[x] != -1)
            {
                *n = (UTIL_MAX(*n,pBMCInfo->LANConfig.VLANTCPSocket[x]));
            }
        }
    }

    if(pBMCInfo->IpmiConfig.VLANIfcSupport == 1)
    {
        *n = (UTIL_MAX (*n, pBMCInfo->LANConfig.hVLANIfc_Info_Q)) ;
    }

    *n = (UTIL_MAX (*n, pBMCInfo->LANConfig.hLANMon_Q)) ;

    return 0;
}

/**
 * @fn  RecvLANPkt
 * @brief This function receives the IPMI LAN request packets
 *        and post the received packets to LANIfc queue.
 * @param None
 **/
    static
void*  RecvLANPkt   (void *pArg)
{
    MsgPkt_T            MsgPkt;
    unsigned int        SourceLen, n=0, i, RetVal, NewTCPSocket;//x=0,
    struct sockaddr_in6  Sourcev6;
    struct sockaddr_in  Sourcev4;
    fd_set              fdRead;
    struct timeval      Timeout;
    INT8U               IsloopBackStatus=FALSE;
    int                 *inst   = (int*)pArg;
    int                 BMCInst = *inst;
    int                 Err,Buff;
    _FAR_   BMCInfo_t   *pBMCInfo = &g_BMCInfo[BMCInst];
    SocketTbl_T *SocketTable = pBMCInfo->pSocketTbl;
    void *Source = NULL;
    prctl(PR_SET_NAME,__FUNCTION__,0,0,0);

    IPMI_DBG_PRINT ("Recv LAN Pkt\n");

    if(0 != GetQueueHandle(LAN_MON_Q,&pBMCInfo->LANConfig.hLANMon_Q,BMCInst))
    {
        IPMI_WARNING("Error in getting LAN_MON_Q Handle \n");
    }

    if ( pBMCInfo->IpmiConfig.VLANIfcSupport == 1)
    {
        if(0 != GetQueueHandle(VLAN_IFC_Q, &pBMCInfo->LANConfig.hVLANIfc_Info_Q, BMCInst))
        {
            IPMI_WARNING("Error in getting VLanIfcQ Handle \n");
        }
    }

    InitializeSocket(&fdRead,&n,BMCInst);

    /* Loop forever */
    while (1)
    {
        IsloopBackStatus = FALSE;
        SetFd (&fdRead, BMCInst);

        /* Set the timeout value for the select */
#ifdef CONFIGURABLE_SESSION_TIME_OUT
        Timeout.tv_sec  =  BMC_GET_SHARED_MEM(BMCInst)->uSessionTimeout;
#else
        Timeout.tv_sec  =  pBMCInfo->IpmiConfig.SessionTimeOut;
#endif

        /* Wait for an event on the socket*/
        RetVal = select ((n+1), &fdRead, NULL, NULL, &Timeout);
        if (-1 == RetVal)
        {
            IPMI_ERROR ("LANIfc.c : Error in select - %s\n", strerror(errno));
            continue;
        }

        if (0 == RetVal)
        {
            /* Its due to timeout - continue */
            continue;
        }

        /* Initialization of socket is done based on changes in VLAN Configurations*/
        if ( pBMCInfo->IpmiConfig.VLANIfcSupport == 1)
        {
            if (FD_ISSET (pBMCInfo->LANConfig.hVLANIfc_Info_Q, &fdRead))
            {
                IPMI_DBG_PRINT_1("%s Received VLAN Notify Info \n",__FUNCTION__);
                OS_GET_FROM_Q(&Buff, sizeof(int), pBMCInfo->LANConfig.hVLANIfc_Info_Q, 0, &Err);
                UpdateLANStateChange(BMCInst);
                InitializeSocket(&fdRead,&n,BMCInst);
                continue;
            }
        }
        /* Initialization of socket is done based on Network state change
           or changes in NCML Configurations */
        if(FD_ISSET(pBMCInfo->LANConfig.hLANMon_Q,&fdRead))
        {
            OS_GET_FROM_Q(&Buff,sizeof(int), pBMCInfo->LANConfig.hLANMon_Q,0,&Err);
            UpdateLANStateChange(BMCInst);
            InitializeSocket(&fdRead,&n,BMCInst);
            continue;
        }

        for(i=0;i<MAX_LAN_CHANNELS;i++)
        {
            if((pBMCInfo->LANConfig.LanIfcConfig[i].Enabled == TRUE) 
                    && (pBMCInfo->LANConfig.LanIfcConfig[i].Up_Status == LAN_IFC_UP))
            {
                if(g_corefeatures.global_ipv6  == ENABLED)
                {
                    SourceLen = sizeof(Sourcev4);
                    Source = &Sourcev6;
                }
                else
                {
                    SourceLen = sizeof(Sourcev4);
                    Source = &Sourcev4;
                }
                if(pBMCInfo->LANConfig.TCPSocket[i] != -1)
                {
                    if (FD_ISSET (pBMCInfo->LANConfig.TCPSocket[i], &fdRead))
                    {
                        /*Accept new TCP connections */
                        NewTCPSocket = accept(pBMCInfo->LANConfig.TCPSocket[i], (struct sockaddr *)Source, &SourceLen );    
                        if (-1 == NewTCPSocket)
                        {
                            IPMI_WARNING ("LANIfc.c : Error accepting connections for BMCInst %d\n", BMCInst);
                            continue; //addednow
                        }
                        MsgPkt.Channel = pBMCInfo->LANConfig.TCPChannel[i];
                        /* Add the socket to the table */
                        if(0 == AddSocket (NewTCPSocket, IsloopBackStatus, FALSE, BMCInst))
                        {
                            /* Add the new TCP client to set */
                            FD_SET (NewTCPSocket, &fdRead);
                            n = (NewTCPSocket >= n) ? NewTCPSocket + 1 : n;
                        }
                        else
                        {
                            IPMI_WARNING ("LANIfc.c : Closing socket\n");
                            close (NewTCPSocket);
                        }
                    }
                }
                if(pBMCInfo->LANConfig.VLANTCPSocket[i] != -1)
                {
                    if (FD_ISSET (pBMCInfo->LANConfig.VLANTCPSocket[i], &fdRead))
                    {
                        /*Accept new TCP connections */
                        NewTCPSocket = accept(pBMCInfo->LANConfig.VLANTCPSocket[i], (struct sockaddr *)Source, &SourceLen );
                        if (-1 == NewTCPSocket)
                        {
                            IPMI_WARNING ("LANIfc.c : Error accepting connections for BMCInst %d\n", BMCInst);
                            continue; //addednow
                        }
                        /* Add the socket to the table */
                        if(0 == AddSocket (NewTCPSocket, IsloopBackStatus, FALSE, BMCInst))
                        {
                            /* Add the new TCP client to set */
                            FD_SET (NewTCPSocket, &fdRead);
                            n = (NewTCPSocket >= n) ? NewTCPSocket + 1 : n;
                        }
                        else
                        {
                            IPMI_WARNING ("LANIfc.c : Closing socket\n");
                            close (NewTCPSocket);
                        }
                    }
                }
                if(g_corefeatures.global_ipv6  == ENABLED)
                {
                    memcpy (&MsgPkt.IPAddr, &((struct sockaddr_in6 *)Source)->sin6_addr.s6_addr, sizeof (struct in6_addr));
                }
                else
                {
                    memcpy (&MsgPkt.IPAddr, &((struct sockaddr_in *)Source)->sin_addr.s_addr, sizeof (struct in_addr));
                }
            }
        }

        for (i = 0; i < (pBMCInfo->IpmiConfig.MaxSession + 1); i++)
        {
            if ((FD_ISSET (SocketTable [i].Socket, &fdRead)) && (SocketTable [i].Valid))
            {
                pBMCInfo->LANConfig.WaitCount = NO_OF_RETRY;
                /* Receive IPMI LAN request packets */
                if(0 == ReadData (&MsgPkt, SocketTable [i].Socket, BMCInst))
                {
                    IPMI_DBG_PRINT("LANIfc.c : Recv LAN Pkt:"); /*garden*/
                    IPMI_DBG_PRINT_BUF (MsgPkt.Data, MsgPkt.Size);

                    /* Post the request packet to LAN Interface Queue */
                    if (0 != PostMsg (&MsgPkt, LAN_IFC_Q, BMCInst))
                    {
                        IPMI_WARNING ("LANIfc.c : Error posting message to LANIfc Q\n");
                    }

                    SocketTable [i].Time = 0;
                }
                else
                {
                    if(!SocketTable[i].IsFixedSocket )
                    {
                        IPMI_WARNING ("LANIfc.c : Closing socket\n");
                        /* Remove the socket from the table and the set */
                        FD_CLR (SocketTable [i].Socket, &fdRead);
                        close (SocketTable [i].Socket);
                        RemoveSocket (SocketTable [i].Socket, BMCInst);
                    }
                }
            }/*if (FD_ISSET (Socket [i], &fdRead))*/
        }/*for (i = 0; i <= (MaxSession + 1); i++)*/
    }
    /* never gets here */
    return  0;
}


/**
 * @fn SendLANPkt
 * @brief This function sends the IPMI LAN Response to the requester
 * @param pRes - Response message.
 **/
int SendLANPkt (MsgPkt_T *pRes,int BMCInst)
{
    struct  sockaddr_in6 Dest6;
    struct  sockaddr_in Dest;
    struct stat Stat;
    int ret = 0;

    if(g_corefeatures.global_ipv6  == ENABLED)
    {
        /* Set the destination UDP port and IP Address */
        Dest6.sin6_family     =   AF_INET6;
        Dest6.sin6_port       =   pRes->UDPPort;
        memcpy (&Dest6.sin6_addr.s6_addr, pRes->IPAddr, sizeof (struct in6_addr));
    }
    else
    {
        /* Set the destination UDP port and IP Address */
        Dest.sin_family     =   AF_INET;
        Dest.sin_port       =   pRes->UDPPort;
        memcpy (&Dest.sin_addr.s_addr, pRes->IPAddr, sizeof (struct in_addr));
    }
    //    printf("sts=%ld\n",pRes->Size);
    /* Send the LAN response packet */
    IPMI_DBG_PRINT_1("\n Im sending in socket :%x\n", pRes->Socket);

    //Check the socket before send a message on a socket
    if (fstat(pRes->Socket, &Stat) != -1) {
        if(g_corefeatures.global_ipv6  == ENABLED)
        {
            ret = sendto (pRes->Socket, pRes->Data, pRes->Size, 0, (struct sockaddr*)&Dest6, sizeof (Dest6));
        }
        else
        {
            ret = sendto (pRes->Socket, pRes->Data, pRes->Size, 0, (struct sockaddr*)&Dest, sizeof (Dest));
        }
        if (ret == -1)
        {
            IPMI_WARNING ("LANIfc.c : Error sending response packets to LAN, %s\n",strerror(errno));
        }
        else
        {
            IPMI_DBG_PRINT_1 ("LANIfc.c : LAN packet sent successfully. ret=%d\n",ret);
        }
    }

    return 1;
}


/**
 * @brief Process SMB Request.
 * @param pReq - Request message.
 **/
    static void
ProcessLANReq (_NEAR_ MsgPkt_T* pReq, MiscParams_T *pParams, int BMCInst)
{
    MsgPkt_T        Res;
    SessionInfo_T*  pSessionInfo;
    SessionHdr_T*   pSessionHdr;
    SessionHdr2_T*  pSessionHdr2;
    INT32U          SessionID =0;
    _FAR_   BMCSharedMem_T* pSharedMem = BMC_GET_SHARED_MEM (BMCInst);
    unsigned int SocktIndex=0;

    /* Copy the request to response */
    Res = *pReq;

    /* Save the LAN header inofmation */
    pSessionHdr  = (SessionHdr_T*) (((RMCPHdr_T*)pReq->Data) + 1);
    pSessionHdr2 = (SessionHdr2_T*)(((RMCPHdr_T*)pReq->Data) + 1);
    if (RMCP_PLUS_FORMAT == pSessionHdr->AuthType)
    {
        SessionID = pSessionHdr2->SessionID;
        pSessionInfo = getSessionInfo (SESSION_ID_INFO, &SessionID, BMCInst);
    }
    else
    {
        SessionID = pSessionHdr->SessionID;
        pSessionInfo = getSessionInfo (SESSION_ID_INFO, &SessionID, BMCInst);
    }
    if (NULL != pSessionInfo)
    {
        LOCK_BMC_SHARED_MEM(BMCInst);
        pSessionInfo->LANRMCPPkt.UDPHdr.SrcPort = Res.UDPPort;
        pSessionInfo->hSocket                   = Res.Socket;
        SocktIndex=GetSocketInfoIndex(pSessionInfo->hSocket, BMCInst);
        if(SocktIndex < 0)
        {
            IPMI_DBG_PRINT("\nUnable Find the Socket index\n");
            return;
        }
        if(g_corefeatures.global_ipv6  == ENABLED)
        {
            _fmemcpy (pSessionInfo->LANRMCPPkt.IPHdr.Srcv6Addr, Res.IPAddr, sizeof (struct in6_addr));
        }
        else
        {
            _fmemcpy (pSessionInfo->LANRMCPPkt.IPHdr.Srcv4Addr, Res.IPAddr, sizeof (struct in_addr));
        }    

        _fmemcpy (&pSessionInfo->LANRMCPPkt.RMCPHdr, Res.Data, sizeof (RMCPHdr_T));
        UNLOCK_BMC_SHARED_MEM(BMCInst);
    }

    /* Process the RMCP Request */
    Res.Size = ProcessRMCPReq ((RMCPHdr_T*)pReq->Data, (RMCPHdr_T*)Res.Data, pParams, pReq->Channel, BMCInst);
    /* ResLen is 0, don't send the packet */
    if (0 == Res.Size )
    {
        //        IPMI_WARNING ("LANIfc.c : LAN request packet dropped, not processed\n");
        return;
    }
    //Once Process RMCP Req is get success then we can Count that RMCP Packet
    //for GET IP RMCP UDP

    LOCK_BMC_SHARED_MEM(BMCInst);
    pSharedMem->gIPUDPRMCPStats++;
    UNLOCK_BMC_SHARED_MEM(BMCInst);
    /* Sent the response packet */
    SendLANPkt (&Res,BMCInst);
    return;
}

/*--------------------------------------------
 * ProcessBridgeMsg
 *--------------------------------------------*/
    static void
ProcessBridgeMsg (_NEAR_ MsgPkt_T* pReq, int BMCInst)
{
    MsgPkt_T        ResPkt;
    INT16U          PayLoadLen   = 0;
    INT8U           PayLoadType  = 0;
    SessionInfo_T   *pSessionInfo = getSessionInfo (SESSION_HANDLE_INFO, pReq->Data,BMCInst);

    IPMI_DBG_PRINT ("LANIfc: Bridge Request\n");
    //    PRINT_BUF (pReq->Data, pReq->Size);

    if (NULL == pSessionInfo)
    {
        IPMI_WARNING ("LANIfc: ProcessBridgeMsg - No Session with the LAN\n");
        return;
    }

    LOCK_BMC_SHARED_MEM(BMCInst);
    /* Copy Lan RMCP headers from Session Record */
    ResPkt.UDPPort  = pSessionInfo->LANRMCPPkt.UDPHdr.SrcPort;
    ResPkt.Socket   = pSessionInfo->hSocket;
    if(g_corefeatures.global_ipv6  == ENABLED)
    {
        _fmemcpy (ResPkt.IPAddr, pSessionInfo->LANRMCPPkt.IPHdr.Srcv6Addr, sizeof (struct in6_addr));
    }
    else
    {
        _fmemcpy (ResPkt.IPAddr, pSessionInfo->LANRMCPPkt.IPHdr.Srcv4Addr, sizeof (struct in_addr));
    }
    _fmemcpy (ResPkt.Data, &pSessionInfo->LANRMCPPkt.RMCPHdr, sizeof (RMCPHdr_T));

#if IPMI20_SUPPORT == 1
    if (RMCP_PLUS_FORMAT == pSessionInfo->AuthType)
    {
        /* Fill Session Header */
        pSessionInfo->OutboundSeq++;
        PayLoadLen      = pReq->Size - 1;
        PayLoadType     = pReq->Cmd;
        PayLoadType    |= (pSessionInfo->SessPyldInfo[PayLoadType].AuxConfig[0] & 0xC0);
        PayLoadLen      = Frame20Payload (PayLoadType, (_NEAR_ RMCPHdr_T*)&ResPkt.Data [0],
                &pReq->Data[1], PayLoadLen, pSessionInfo, BMCInst);
    }
    else
#endif /*IPMI20_SUPPORT == 1*/
    {
        /* Fill Session Header */
        _NEAR_ SessionHdr_T* pSessionHdr = (_NEAR_ SessionHdr_T*)(&ResPkt.Data [sizeof(RMCPHdr_T)]);
        _NEAR_ INT8U*        pPayLoad    = (_NEAR_ INT8U*)(pSessionHdr + 1);

        pSessionHdr->AuthType       = pSessionInfo->AuthType;
        pSessionHdr->SessionSeqNum  = pSessionInfo->OutboundSeq++;
        pSessionHdr->SessionID      = pSessionInfo->SessionID;
#if 0
        /* If AuthType is not 0 - Compute AuthCode */
        if (0 != pSessionInfo->AuthType)
        {
            IPMI_DBG_PRINT ("Compute Authcode\n");
            PayLoadLen = AUTH_CODE_LEN;
            pPayLoad [PayLoadLen++] = pReq->Size - 1;
            _fmemcpy (&pPayLoad [PayLoadLen], (pReq->Data + 1), (pReq->Size - 1));
            PayLoadLen += pReq->Size;
            PayLoadLen--;
            PayLoadLen += sizeof (SessionHdr_T) + sizeof (RMCPHdr_T);
            ComputeAuthCode (pSessionInfo->Password, pSessionHdr,
                    (_NEAR_ IPMIMsgHdr_T*) &pPayLoad [AUTH_CODE_LEN+1],
                    pPayLoad, MULTI_SESSION_CHANNEL);
        }
        else
#endif
        {
            pPayLoad [PayLoadLen++] = pReq->Size - 1;
            /*  Fill the ipmi message */
            _fmemcpy (&pPayLoad [PayLoadLen], (pReq->Data + 1), (pReq->Size - 1));
            PayLoadLen += pReq->Size;
            PayLoadLen--;
            PayLoadLen += sizeof (SessionHdr_T) + sizeof (RMCPHdr_T);
        }

    }

    UNLOCK_BMC_SHARED_MEM(BMCInst);

    ResPkt.Size = PayLoadLen;

    if(pSessionInfo->Activated)
    {
        /* Sent the response packet */
        SendLANPkt (&ResPkt,BMCInst);
    }

    return;
}


/**
 * @fn  LANTimer
 * @brief This function handles the time out for lan connections.
 * @param None
 **/
    static
void*  LANTimer (void *pArg)
{
    int *inst = (int*)pArg;
    int BMCInst= *inst;
    prctl(PR_SET_NAME,__FUNCTION__,0,0,0);

    while (1)
    {
        UpdateTimeout (BMCInst);
        sleep (LAN_TIMER_INTERVAL);
    }

    return 0;
}

/**
 * @fn ConfigureVLANSocket
 * @brief configures vlan udp sockets
 * @return   1 if success, -1 if failed.
 **/
static int ConfigureVLANSocket( int Index,int BMCInst)
{
    struct  sockaddr_in6   VLANSockaddr6;
    int  v6Only = 0;
    struct  sockaddr_in   VLANSockaddr;
    char    VLANUDPInterface [MAX_STR_LENGTH];
    int     reuseaddr       = 1;
    int     VLANpriority    = 0;
    int     Ethindex=0;
    _FAR_   BMCInfo_t   *pBMCInfo   = &g_BMCInfo[BMCInst];
    int ret = 0;

    /* Close the VLAN IFC socket */
    if(pBMCInfo->LANConfig.VLANUDPSocket[Index] != -1)
    {
        close(pBMCInfo->LANConfig.VLANUDPSocket[Index]);
        pBMCInfo->LANConfig.VLANUDPSocket[Index] = -1;
    }
    if(pBMCInfo->LANConfig.VLANTCPSocket[Index] != -1)
    {
        shutdown(pBMCInfo->LANConfig.VLANTCPSocket[Index],SHUT_RDWR);
        close(pBMCInfo->LANConfig.VLANTCPSocket[Index]);
        pBMCInfo->LANConfig.VLANTCPSocket[Index] = -1;
    }


    if(pBMCInfo->LANConfig.VLANID[Index] != 0)
    {
        memset(VLANUDPInterface,0,sizeof(VLANUDPInterface));
        sprintf (VLANUDPInterface, "%s.%d", pBMCInfo->LANConfig.LanIfcConfig[Index].ifname, pBMCInfo->LANConfig.VLANID[Index]);

        while(1)
        {
            if (0 == CheckInterfacePresence (VLANUDPInterface))
            {
                Ethindex=pBMCInfo->LANConfig.LanIfcConfig[Index].Ethindex;
                if(g_corefeatures.global_ipv6  == ENABLED)
                {
                    VLANSockaddr6.sin6_family        = AF_INET6;
                    inet_pton(AF_INET6, "0::0", &VLANSockaddr6.sin6_addr);
                    VLANSockaddr6.sin6_port          = htons(pBMCInfo->LANCfs[Ethindex].PrimaryRMCPPort); //htons( LAN_RMCP_PORT);
                    VLANSockaddr6.sin6_flowinfo=0;
                    VLANSockaddr6.sin6_scope_id=0;

                    /* Initialize The VLAN Socket */
                    pBMCInfo->LANConfig.VLANUDPSocket[Index] = socket (AF_INET6, SOCK_DGRAM, 0);
                }
                else
                {
                    VLANSockaddr.sin_family        = AF_INET;
                    VLANSockaddr.sin_addr.s_addr   = INADDR_ANY;
                    VLANSockaddr.sin_port          = htons(pBMCInfo->LANCfs[Ethindex].PrimaryRMCPPort); //htons( LAN_RMCP_PORT);

                    /* Initialize The VLAN Socket */
                    pBMCInfo->LANConfig.VLANUDPSocket[Index] = socket (AF_INET, SOCK_DGRAM, 0);
                }
                if ( pBMCInfo->LANConfig.VLANUDPSocket[Index] == -1)
                {
                    IPMI_ERROR ("LANIfc.c : Unable to create VLAN UDP socket\n");
                    return 1;
                }

                IPMI_DBG_PRINT_1 ("VLAN UDP Socket = 0x%x\n", pBMCInfo->LANConfig.VLANUDPSocket[Index]);

                if ( 0 != setsockopt (pBMCInfo->LANConfig.VLANUDPSocket[Index], SOL_SOCKET, SO_BINDTODEVICE, VLANUDPInterface, strlen(VLANUDPInterface)+1) )
                {
                    IPMI_ERROR ("LANIfc.c: SetSockOpt Failed for VLAN UDP Socket");
                    return -1;
                }

                if (setsockopt(pBMCInfo->LANConfig.VLANUDPSocket[Index], SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1)
                {
                    IPMI_ERROR("LANIfc.c: Setsockopt(SO_REUSEADDR) Failed for VLAN socket\n");
                }

                if (setsockopt(pBMCInfo->LANConfig.VLANUDPSocket[Index], SOL_SOCKET, SO_PRIORITY, &VLANpriority, sizeof(VLANpriority)) == -1)
                {
                    IPMI_ERROR("LANIfc.c: Setsockopt(SO_PRIORITY) Failed for VLAN socket\n");
                }
                if(g_corefeatures.global_ipv6  == ENABLED)
                {
#if defined( IPV6_V6ONLY )
                    setsockopt( pBMCInfo->LANConfig.VLANUDPSocket[Index], IPPROTO_IPV6, IPV6_V6ONLY, &v6Only,sizeof( v6Only ) );
#endif
                    SetIPv6Header(pBMCInfo->LANConfig.VLANUDPSocket[Index], Ethindex, BMCInst);
                }
                else
                {
                    SetIPv4Header(pBMCInfo->LANConfig.VLANUDPSocket[Index], Ethindex, BMCInst);
                }

                /* Bind VLAN UDPSocket to Port, INET address */
                if(g_corefeatures.global_ipv6  == ENABLED)
                {
                    ret = bind( pBMCInfo->LANConfig.VLANUDPSocket[Index], (struct sockaddr *)&VLANSockaddr6, sizeof(VLANSockaddr6));
                }
                else
                {
                    ret = bind( pBMCInfo->LANConfig.VLANUDPSocket[Index], (struct sockaddr *)&VLANSockaddr, sizeof(VLANSockaddr));
                }
                if (ret == -1)
                {
                    IPMI_ERROR  ("LANIfc.c : Error binding VLAN UDP socket, %d, %s\n", errno, strerror(errno));
                    return -1;
                }

                pBMCInfo->LANConfig.VLANIFcheckFlag[Index]      = 1;
                IPMI_DBG_PRINT_1 ("VLAN interface initialized in LAN interface of IPMI stack %d\n" ,pBMCInfo->LANConfig.VLANID[Index]);

                if(pBMCInfo->LANConfig.UDPSocket[Index] != -1)
                {
                    close(pBMCInfo->LANConfig.UDPSocket[Index]);
                    pBMCInfo->LANConfig.UDPSocket[Index] = -1;
                    TCRIT("LANIfc.c: LAN UDP sockets closed successfully %d\n",Index);
                }
                if(g_corefeatures.global_ipv6  == ENABLED)
                {
                    VLANSockaddr6.sin6_family        = AF_INET6;
                    inet_pton(AF_INET6, "0::0", &VLANSockaddr6.sin6_addr);
                    VLANSockaddr6.sin6_port          = htons( LAN_RMCP_PORT);
                    VLANSockaddr6.sin6_flowinfo=0;
                    VLANSockaddr6.sin6_scope_id=0;

                    /* Initialize The VLAN Socket */
                    pBMCInfo->LANConfig.VLANTCPSocket[Index] = socket (AF_INET6, SOCK_STREAM, 0);
                }
                else
                {
                    VLANSockaddr.sin_family        = AF_INET;
                    VLANSockaddr.sin_addr.s_addr   = INADDR_ANY;
                    VLANSockaddr.sin_port          = htons( LAN_RMCP_PORT);

                    /* Initialize The VLAN Socket */
                    pBMCInfo->LANConfig.VLANTCPSocket[Index] = socket (AF_INET, SOCK_STREAM, 0);
                }
                if ( pBMCInfo->LANConfig.VLANTCPSocket[Index] == -1)
                {
                    IPMI_ERROR ("LANIfc.c : Unable to create VLAN UDP socket\n");
                    return 1;
                }

                IPMI_DBG_PRINT_1 ("VLAN UDP Socket = 0x%x\n", pBMCInfo->LANConfig.VLANTCPSocket[Index]);

                if ( 0 != setsockopt (pBMCInfo->LANConfig.VLANTCPSocket[Index], SOL_SOCKET, SO_BINDTODEVICE, VLANUDPInterface, strlen(VLANUDPInterface)+1) )
                {
                    IPMI_ERROR ("LANIfc.c: SetSockOpt Failed for VLAN UDP Socket");
                    return -1;
                }

                if (setsockopt(pBMCInfo->LANConfig.VLANTCPSocket[Index], SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1)
                {
                    IPMI_ERROR("LANIfc.c: Setsockopt(SO_REUSEADDR) Failed for VLAN socket\n");
                }

                if (setsockopt(pBMCInfo->LANConfig.VLANTCPSocket[Index], SOL_SOCKET, SO_PRIORITY, &VLANpriority, sizeof(VLANpriority)) == -1)
                {
                    IPMI_ERROR("LANIfc.c: Setsockopt(SO_PRIORITY) Failed for VLAN socket\n");
                }

                /* Bind VLAN TCPSocket to Port, INET address */
                if(g_corefeatures.global_ipv6  == ENABLED)
                {
                    ret = bind( pBMCInfo->LANConfig.VLANTCPSocket[Index], (struct sockaddr *)&VLANSockaddr6, sizeof(VLANSockaddr6)); 
                }
                else
                {
                    ret = bind( pBMCInfo->LANConfig.VLANTCPSocket[Index], (struct sockaddr *)&VLANSockaddr, sizeof(VLANSockaddr));
                }
                if (ret == -1)
                {
                    IPMI_ERROR  ("LANIfc.c : Error binding VLAN UDP socket, %d, %s\n", errno, strerror(errno));
                    return -1;
                }

                IPMI_DBG_PRINT_1("VLAN TCP socket is created %d\n",pBMCInfo->LANConfig.VLANTCPSocket[Index]);
                if (listen(pBMCInfo->LANConfig.VLANTCPSocket[Index], pBMCInfo->IpmiConfig.MaxSession) == -1)
                {
                    IPMI_ERROR ("LANIfc.c : Error listen\n");
                    return -1;
                }

                if(pBMCInfo->LANConfig.TCPSocket[Index] != -1)
                {
                    shutdown(pBMCInfo->LANConfig.TCPSocket[Index],SHUT_RDWR);
                    close(pBMCInfo->LANConfig.TCPSocket[Index]);
                    pBMCInfo->LANConfig.TCPSocket[Index] = -1;
                    TCRIT("LANIfc.c: LAN TCP sockets closed successfully %d\n",Index);
                }

                break;
            }
            else    /*Wait for VLAN interface to go up */
            {
                IPMI_DBG_PRINT_1("Waiting for VLAN interface presence before going up on VLAN%d\n", pBMCInfo->LANConfig.VLANID[Index]);
                sleep(5);
                continue;
            }
        }
    }

    return 1;
}

/**
 * @fn ConfigureBONDSocket
 * @brief configures bond udp sockets
 * @return   1 if success, -1 if failed.
 **/
static int ConfigureSocket(int Ethindex,int BMCInst)
{

    if (InitUDPSocket( Ethindex, BMCInst) != 0)
    {
        IPMI_ERROR("Error in creating UDP Sockets\n");
    }
    if (InitTCPSocket(Ethindex, BMCInst) != 0)
    {
        IPMI_ERROR("Error in creating TCP Sockets\n");
    }

    return 1;
}

static int DeConfigureSocket(int Ethindex,int BMCInst)
{
    _FAR_   BMCInfo_t   *pBMCInfo   = &g_BMCInfo[BMCInst];

    if(pBMCInfo->LANConfig.UDPSocket[Ethindex] != -1)
    {
        close(pBMCInfo->LANConfig.UDPSocket[Ethindex]);
        pBMCInfo->LANConfig.UDPSocket[Ethindex] = -1;
    }

    if(pBMCInfo->LANConfig.TCPSocket[Ethindex] != -1)
    {
        shutdown(pBMCInfo->LANConfig.TCPSocket[Ethindex],SHUT_RDWR);
        close(pBMCInfo->LANConfig.TCPSocket[Ethindex]);
        pBMCInfo->LANConfig.TCPSocket[Ethindex] = -1;
    }

    return 0;
}
    static int
ClearSocketTable (int BMCInst)
{
    int i;
    _FAR_   BMCInfo_t   *pBMCInfo       = &g_BMCInfo[BMCInst];
    SocketTbl_T *SocketTable    = pBMCInfo->pSocketTbl;

    for (i = 0; i < (pBMCInfo->IpmiConfig.MaxSession + 1); i++)
    {
        SocketTable [i].Valid = 0;
    }
    return 0;
}


static int SetIPv6Header(int socketID, int ethIndex, int BMCInst)
{
    BMCInfo_t *pBMCInfo = &g_BMCInfo[BMCInst];
    int   flag = 0;
    unsigned int TClass;
    unsigned int HOPLimt;
    TClass = pBMCInfo->LANCfs[ethIndex].Ipv4HdrParam.TypeOfService;
    HOPLimt = pBMCInfo->LANCfs[ethIndex].Ipv4HdrParam.TimeToLive;
    if (setsockopt(socketID, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &(HOPLimt), sizeof(unsigned int)) == -1)
    {
        perror("LANIfc.c: Setsockopt(IPV6_UNICAST_HOPS) Failed for UDP socket:");
        return -1;
    }

    flag = pBMCInfo->LANCfs[ethIndex].Ipv4HdrParam.IpHeaderFlags >> 5;
    if (setsockopt(socketID, IPPROTO_IPV6, IPV6_MTU_DISCOVER, &(flag), sizeof(int)) == -1)
    {
        IPMI_ERROR("LANIfc.c: Setsockopt(IPV6_MTU_DISCOVER) Failed for UDP socket\n");
        return -1;
    }

    if (setsockopt(socketID, IPPROTO_IPV6, IPV6_TCLASS, &(TClass), sizeof(unsigned int)) == -1)
    {
        IPMI_ERROR("LANIfc.c: Setsockopt(IPV6_TCLASS) Failed for UDP socket\n");
        return -1;
    }

    if (setsockopt(socketID, IPPROTO_IP, IP_TTL,
                &(pBMCInfo->LANCfs[ethIndex].Ipv4HdrParam.TimeToLive), sizeof(INT8U)) == -1)
    {
        perror("LANIfc.c: Setsockopt(IP_TTL) Failed for UDP socket:");
        return -1;
    }

    flag = pBMCInfo->LANCfs[ethIndex].Ipv4HdrParam.IpHeaderFlags >> 5;
    if (setsockopt(socketID, IPPROTO_IP, IP_MTU_DISCOVER,
                &(flag), sizeof(INT8U)) == -1)
    {
        IPMI_ERROR("LANIfc.c: Setsockopt(IP_MTU_DISCOVER) Failed for UDP socket\n");
        return -1;
    }

    if (setsockopt(socketID, IPPROTO_IP, IP_TOS,
                &(pBMCInfo->LANCfs[ethIndex].Ipv4HdrParam.TypeOfService), sizeof(INT8U)) == -1)
    {
        IPMI_ERROR("LANIfc.c: Setsockopt(IP_TOS) Failed for UDP socket\n");
        return -1;
    }

    return 0;
}

static int SetIPv4Header(int socketID, int ethIndex, int BMCInst)
{
    BMCInfo_t *pBMCInfo = &g_BMCInfo[BMCInst];
    INT8U   flag = 0;

    /* IPv4 Header changes */
    /*printf("\nIPv4 Header value TTL : %d, Flag : %d, TOS : %d\n",
      pPMConfig->LANConfig[ethIndex].Ipv4HdrParam.TimeToLive,
      pPMConfig->LANConfig[ethIndex].Ipv4HdrParam.IpHeaderFlags,
      pPMConfig->LANConfig[ethIndex].Ipv4HdrParam.TypeOfService);*/

    if (setsockopt(socketID, IPPROTO_IP, IP_TTL,
                &(pBMCInfo->LANCfs[ethIndex].Ipv4HdrParam.TimeToLive), sizeof(INT8U)) == -1)
    {
        perror("LANIfc.c: Setsockopt(IP_TTL) Failed for UDP socket:");
        return -1;
    }

    flag = pBMCInfo->LANCfs[ethIndex].Ipv4HdrParam.IpHeaderFlags >> 5;
    if (setsockopt(socketID, IPPROTO_IP, IP_MTU_DISCOVER,
                &(flag), sizeof(INT8U)) == -1)
    {
        IPMI_ERROR("LANIfc.c: Setsockopt(IP_MTU_DISCOVER) Failed for UDP socket\n");
        return -1;
    }

    if (setsockopt(socketID, IPPROTO_IP, IP_TOS,
                &(pBMCInfo->LANCfs[ethIndex].Ipv4HdrParam.TypeOfService), sizeof(INT8U)) == -1)
    {
        IPMI_ERROR("LANIfc.c: Setsockopt(IP_TOS) Failed for UDP socket\n");
        return -1;
    }

    return 0;
}

