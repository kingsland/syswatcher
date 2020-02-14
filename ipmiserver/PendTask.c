/****************************************************************
 ****************************************************************
 **                                                            **
 **    (C)Copyright 2006-2007, American Megatrends Inc.        **
 **                                                            **
 **            All Rights Reserved.                            **
 **                                                            **
 **        6145-F, Northbelt Parkway, Norcross,                **
 **                                                            **
 **        Georgia - 30071, USA. Phone-(770)-246-8600.         **
 **                                                            **
 ****************************************************************
 *****************************************************************
 *
 * PendTask.c
 *      Any IPMI command operation which requires more
 *      response time can be posted to this task.
 *
 * Author: Vinothkumar S <vinothkumars@ami.com>
 *
 *****************************************************************/
#define ENABLE_DEBUG_MACROS    0
#include "Types.h"
#include "OSPort.h"
#include "Debug.h"
#include "Support.h"
#include "Message.h"
#include "MsgHndlr.h"
#include "IPMI_Main.h"
#include "IPMIDefs.h"
#include "SharedMem.h"
//#include "AMI.h"
#include "App.h"
//#include "Bridge.h"
//#include "Chassis.h"
//#include "Storage.h"
#include "SensorEvent.h"
#include "DeviceConfig.h"
//#include "ChassisDevice.h"
//#include "WDT.h"
//#include "SDR.h"
//#include "SEL.h"
//#include "FRU.h"
//#include "Sensor.h"
//#include "SensorMonitor.h"
//#include "FFConfig.h"
//#include "NVRAccess.h"
#include "Platform.h"
//#include "ipmi_hal.h"
//#include "ipmi_int.h"
//#include "AMIDevice.h"
#include "PendTask.h"
#include "nwcfg.h"
#include <unistd.h>
#include <sys/reboot.h>
#include <linux/if.h>
#include <linux/reboot.h>
#include"Ethaddr.h"
#include "LANIfc.h"
//#include "PDKAccess.h"
//#include "IPMI_AMIConf.h"
#include "LANConfig.h"
//#include "hostname.h"
#include <sys/reboot.h>
#include <linux/reboot.h>
#include<sys/prctl.h>
//#include "flshdefs.h"
//#include "flashlib.h"
//#include "flshfiles.h"
#include "ncml.h"
//#include "libncsiconf.h"
#include "featuredef.h"
#include <linux/ip.h>

/*--------------------------------------------------------------------
 * Global Variables
 *--------------------------------------------------------------------*/



/*-----------------------------------------------------------------------------
 * Function Prototypes
 *-----------------------------------------------------------------------------*/
static PendCmdHndlrTbl_T* GetPendTblEntry (PendTaskOperation_E Operation);
//static int PendSetIPAddress (INT8U *pData, INT32U DataLen,INT8U EthIndex,int BMCInst);
//static int PendSetSubnet (INT8U *pData, INT32U DataLen,INT8U EthIndex,int BMCInst);
//static int PendSetGateway (INT8U *pData, INT32U DataLen,INT8U EthIndex,int BMCInst);
//static int PendSetSource (INT8U *pData, INT32U DataLen,INT8U EthIndex,int BMCInst);
//static int PendDelayedColdReset (INT8U *pData, INT32U DataLen,INT8U EthIndex,int BMCInst);
//static int PendSendEmailAlert (INT8U *pData, INT32U DataLen,INT8U EthIndex,int BMCInst);
void ConvertIPnumToStr(unsigned char *var, unsigned int len,unsigned char *string);

/**
 *@fn PendSetVLANIfcID
 *@brief This function is invoked to set the valn id for the specified interface
 *@param pData -  Pointer to buffer where network configurations saved
 *@param DataLen -  unsigned integer parameter to buffer where length of the input data specified
 *@param EthIndex -  char value to bufferr where index for Ethernet channel is saved
 */
//static int PendSetVLANIfcID(INT8U* pData,INT32U DataLen,INT8U EthIndex,int BMCInst);

/**
 *@fn PendDeConfigVLANInterface
 *@brief This function is invoked to de-configure vlan sockets
 *@param pData -  Pointer to buffer where network configurations saved
 *@param DataLen -  unsigned integer parameter to buffer where length of the input data specified
 *@param EthIndex -  char value to bufferr where index for Ethernet channel is saved
 */
//static int PendDeConfigVLANInterface(INT8U* pData, INT32U DataLen,INT8U EthIndex,int BMCInst);


/**
 *@fn PendSetIPv4Headers
 *@brief This function is invoked to set IPv4 headers.
 *@param pData -  Pointer to buffer where network configurations saved
 *@param DataLen -  unsigned integer parameter to buffer where length of the input data specified
 *@param EthIndex -  char value to bufferr where index for Ethernet channel is saved
 *@return Returns 0 on success
 */
//int PendSetIPv4Headers(INT8U* pData, INT32U DataLen,INT8U EthIndex,int BMCInst);

//int PendSetRMCPPort(INT8U* pData,INT32U Datalen,INT8U EthIndex,int BMCInst);

//static int PendSetAllDNSCfg(INT8U* pData, INT32U DataLen,INT8U EthIndex,int BMCInst);

//static int PendSetIPv6Cfg(INT8U* pData, INT32U DataLen,INT8U EthIndex,int BMCInst);

//static int PendSetEthIfaceState(INT8U* pData, INT32U DataLen,INT8U EthIndex,int BMCInst);

//static int PendSetMACAddress(INT8U* pData, INT32U DataLen, INT8U EthIndex, int BMCInst);


//static int PendSetIPv6Enable(INT8U* pData, INT32U DataLen, INT8U EthIndex, int BMCInst);

//static int PendSetIPv6Source(INT8U* pData, INT32U DataLen, INT8U EthIndex, int BMCInst);

//static int PendSetIPv6Address(INT8U* pData, INT32U DataLen, INT8U EthIndex, int BMCInst);

//static int PendSetIPv6PrefixLength(INT8U* pData, INT32U DataLen, INT8U EthIndex, int BMCInst);

//static int PendSetIPv6Gateway(INT8U* pData, INT32U DataLen, INT8U EthIndex, int BMCInst);

//static int PendConfigBonding(INT8U* pData,INT32U DataLen,INT8U EthIndex,int BMCInst);

//static int PendActiveSlave(INT8U* pData,INT32U DataLen,INT8U Ethindex,int BMCInst);

//static int PendRestartServices(INT8U * pData, INT32U DataLen, INT8U Ethindex,int BMCInst);

//static int PendStartFwUpdate_Tftp(INT8U *pData, INT32U DataLen, INT8U Ethindex,int BMCInst);

//static int PendSetNCSIChannelID(INT8U *pData, INT32U DataLen, INT8U Ethindex,int BMCInst);

/*------------------ Pending Operation Table --------------------------*/
const PendCmdHndlrTbl_T m_PendOperationTbl [] =
{
    /* Operation                    Handler      */
    //    { PEND_OP_SET_IP,               PendSetIPAddress            },
    //    { PEND_OP_SET_SUBNET,           PendSetSubnet               },
    //    { PEND_OP_SET_GATEWAY,          PendSetGateway              },
    //    { PEND_OP_SET_SOURCE,           PendSetSource               },
    //    { PEND_OP_DELAYED_COLD_RESET,   PendDelayedColdReset        },
    //    { PEND_OP_SEND_EMAIL_ALERT,     PendSendEmailAlert          },
    //    { PEND_OP_SET_VLAN_ID,          PendSetVLANIfcID            },
    //    { PEND_OP_DECONFIG_VLAN_IFC,    PendDeConfigVLANInterface   },
    //    { PEND_OP_SET_IPV4_HEADERS,     PendSetIPv4Headers          },
    //    { PEND_RMCP_PORT_CHANGE,        PendSetRMCPPort             },
    //    { PEND_OP_SET_ALL_DNS_CFG,      PendSetAllDNSCfg            },
    //    { PEND_OP_SET_IPV6_CFG,         PendSetIPv6Cfg              },
    //    { PEND_OP_SET_ETH_IFACE_STATE,      PendSetEthIfaceState    },
    //    { PEND_OP_SET_MAC_ADDRESS,		PendSetMACAddress	},
    //    { PEND_OP_SET_IPV6_ENABLE,      PendSetIPv6Enable   	},
    //    { PEND_OP_SET_IPV6_IP_ADDR_SOURCE,      PendSetIPv6Source   },
    //    { PEND_OP_SET_IPV6_IP_ADDR,     PendSetIPv6Address    	},
    //    { PEND_OP_SET_IPV6_PREFIX_LENGTH,     PendSetIPv6PrefixLength  },
    //    { PEND_OP_SET_IPV6_GATEWAY,     PendSetIPv6Gateway    	},
    //    { PEND_OP_SET_BOND_IFACE_STATE,       PendConfigBonding	},
    //    { PEND_OP_SET_ACTIVE_SLAVE,     PendActiveSlave		},
    //    { PEND_OP_RESTART_SERVICES,     PendRestartServices		},
    //    { PEND_OP_START_FW_UPDATE_TFTP, PendStartFwUpdate_Tftp	},
    //    { PEND_OP_SET_NCSI_CHANNEL_ID,  PendSetNCSIChannelID	}   

};

PendCmdStatus_T m_PendStatusTbl [] =
{
    /* Operation                    Status */
    { PEND_OP_SET_IP ,              PEND_STATUS_COMPLETED },
    { PEND_OP_SET_SUBNET ,          PEND_STATUS_COMPLETED },
    { PEND_OP_SET_GATEWAY ,         PEND_STATUS_COMPLETED },
    { PEND_OP_SET_SOURCE ,          PEND_STATUS_COMPLETED },
    { PEND_OP_DELAYED_COLD_RESET ,  PEND_STATUS_COMPLETED },
    { PEND_OP_SEND_EMAIL_ALERT ,    PEND_STATUS_COMPLETED },
    { PEND_OP_SET_VLAN_ID ,         PEND_STATUS_COMPLETED },
    { PEND_OP_DECONFIG_VLAN_IFC,    PEND_STATUS_COMPLETED },
    { PEND_OP_SET_IPV4_HEADERS,     PEND_STATUS_COMPLETED },
    { PEND_RMCP_PORT_CHANGE,        PEND_STATUS_COMPLETED },
    { PEND_OP_SET_ALL_DNS_CFG,      PEND_STATUS_COMPLETED },
    { PEND_OP_SET_ETH_IFACE_STATE,      PEND_STATUS_COMPLETED },
    { PEND_OP_SET_MAC_ADDRESS,		PEND_STATUS_COMPLETED },
    { PEND_OP_SET_IPV6_ENABLE,		PEND_STATUS_COMPLETED },
    { PEND_OP_SET_IPV6_IP_ADDR_SOURCE,		PEND_STATUS_COMPLETED },
    { PEND_OP_SET_IPV6_IP_ADDR,		PEND_STATUS_COMPLETED },  
    { PEND_OP_SET_IPV6_PREFIX_LENGTH,		PEND_STATUS_COMPLETED },
    { PEND_OP_SET_IPV6_GATEWAY,		PEND_STATUS_COMPLETED },
    { PEND_OP_SET_BOND_IFACE_STATE,               PEND_STATUS_COMPLETED},
    { PEND_OP_SET_ACTIVE_SLAVE,        PEND_STATUS_COMPLETED},
    { PEND_OP_RESTART_SERVICES,		PEND_STATUS_COMPLETED},
    { PEND_OP_START_FW_UPDATE_TFTP,	PEND_STATUS_COMPLETED},
    { PEND_OP_SET_NCSI_CHANNEL_ID,	PEND_STATUS_COMPLETED}
};

extern IfcName_T Ifcnametable[MAX_LAN_CHANNELS];
extern char *RestartServices[MAX_RESTART_SERVICE];
extern char *ModifyServiceNameList[MAX_SERVICES];

static const INT8U IP_TOS2PRIO[16] = {
    0,        /* TC_PRIO_BESTEFFORT,           */
    1,        /* ECN_OR_COST(FILLER),          */
    0,        /* TC_PRIO_BESTEFFORT,           */
    0,        /* ECN_OR_COST(BESTEFFORT),      */
    2,        /* TC_PRIO_BULK,                 */
    2,        /* ECN_OR_COST(BULK),            */
    2,        /* TC_PRIO_BULK,                 */
    2,        /* ECN_OR_COST(BULK),            */
    6,        /* TC_PRIO_INTERACTIVE,          */
    6,        /* ECN_OR_COST(INTERACTIVE),     */
    6,        /* TC_PRIO_INTERACTIVE,          */
    6,        /* ECN_OR_COST(INTERACTIVE),     */
    4,        /* TC_PRIO_INTERACTIVE_BULK,     */
    4,        /* ECN_OR_COST(INTERACTIVE_BULK),*/
    4,        /* TC_PRIO_INTERACTIVE_BULK,     */
    4         /* ECN_OR_COST(INTERACTIVE_BULK) */
};
#if 0
/**
 * PendCmdTask
 *
 * @brief IPMI Command Pending Task.
 **/
void* PendCmdTask (void *pArg)
{
    int *inst = (int*) pArg;
    int BMCInst = *inst;
    MsgPkt_T                MsgPkt;
    PendCmdHndlrTbl_T       *pPendTblEntry = NULL;
    INT8U EthIndex,netindex = 0xFF,i;
    char IfcName[16];
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    prctl(PR_SET_NAME,__FUNCTION__,0,0,0);

    IPMI_DBG_PRINT("Pending Task Started\n");

    /* Process Task from PendTask Q */
    while (1)
    {
        /* Wait for any Pending Operation */
        if (0 != GetMsg (&MsgPkt, PEND_TASK_Q, WAIT_INFINITE, BMCInst))
        {
            IPMI_WARNING ("PendTask.c : Error fetching messages from PenTaskQ\n");
            continue;
        }

        /* Get appropriate Pending Table Entry */
        pPendTblEntry = GetPendTblEntry (MsgPkt.Param);

        /* Check if match found  */
        if (pPendTblEntry == NULL)
        {
            /* If not in core it could be an handler in PDK */
            //pPendTblEntry = PDK_GetPendTblEntry (MsgPkt.Param);

            /* If not present in PDK then unknown operation */
            if (pPendTblEntry == NULL)
            {
                IPMI_WARNING ("PendTask.c : Unknown Pending Operation\n");
                continue;
            }
        }

        /* Hanlde Operation */
        if (pPendTblEntry->PendHndlr == NULL)
        {
            IPMI_WARNING ("PendTask.c : Unknown Operation Handler\n");
            continue;
        }

        EthIndex=GetEthIndex(MsgPkt.Channel, BMCInst);

        /*we should not pass this index value to libnetwork library.
          we can use this index value which is related to ipmi. 
          It will be useful in bonding mode*/
        pBMCInfo->LANConfig.g_ethindex = EthIndex;

        /*Get the EthIndex*/
        if(GetIfcName(EthIndex,IfcName, BMCInst) == -1)
        {
            TCRIT("Error in Getting Ethindex\n");
        }

        for(i=0;i<sizeof(Ifcnametable)/sizeof(IfcName_T);i++)
        {
            if(strcmp(Ifcnametable[i].Ifcname,IfcName) == 0)
            {
                netindex= Ifcnametable[i].Index;
                break;
            }
        }

        /*if(netindex == 0xFF)
          {
          TCRIT("Error in getting Ethindex\n");
          continue;
          }*/
        /* Operation Done !! */
        pPendTblEntry->PendHndlr (MsgPkt.Data, MsgPkt.Size,netindex,BMCInst);

    }

}
#endif
/**
 * PostPendTask
 *
 * @brief Post A Msg to Pending Task Table.
 **/
    int
PostPendTask (PendTaskOperation_E Operation, INT8U *pData, INT32U DataLen,INT8U Channel,int BMCInst)
{
    MsgPkt_T    MsgPkt;

    /* Fill Operation information */
    MsgPkt.Param = Operation;
    memcpy (MsgPkt.Data, pData, DataLen);
    MsgPkt.Size = DataLen;
    MsgPkt.Channel= Channel;


    /* Post to Pending Task Q */
    if (0 != PostMsg (&MsgPkt, PEND_TASK_Q,BMCInst))
    {
        IPMI_WARNING ("PendTask.c : Unable to post to PendTask Q\n");
        return -1;
    }

    return 0;
}


#if 0
/**
 * GetPendTblEntry
 *
 * @brief Fetches an handler from Pending Table.
 **/
    PendCmdHndlrTbl_T*
GetPendTblEntry (PendTaskOperation_E Operation)
{
    int i = 0;

    /*  Search through the pending table and find a match */
    for (i = 0; i < sizeof (m_PendOperationTbl)/ sizeof (m_PendOperationTbl[0]); i++)
    {
        if (m_PendOperationTbl [i].Operation == Operation)
        {
            return (PendCmdHndlrTbl_T*)&m_PendOperationTbl[i];
        }
    }

    /* Match not found !!! */
    return NULL;
}
#endif
/*
 * @fn SetPendStatus
 * @brief This function updates the status of Pend Task Operation
 * @param Action -  Pend Task Operation to update
 * @param Status - Status of the Pend Task Operation
 * @return Returns 0 on success
 *             Returns -1 on failure
 */
int SetPendStatus (PendTaskOperation_E Action, int Status)
{
    int i = 0;

    for(i=0;i < sizeof(m_PendStatusTbl)/ sizeof(m_PendStatusTbl[0]);i++)
    {
        if(m_PendStatusTbl[i].Action == Action)
        {
            m_PendStatusTbl [i].PendStatus = Status;
            return 0;
        }
    }

    /* Match not found !!!*/
    return -1;
}

/*
 * @fn GetPendStatus
 * @brief This function retrives the status of Pend Task Operation
 * @param Action -  Pend Task Operation to update
 * @return Returns Pend Task Operation Status
 *             Returns -1 on failure
 */
int GetPendStatus (PendTaskOperation_E Action)
{
    int i=0;

    for(i=0;i < sizeof(m_PendStatusTbl)/ sizeof(m_PendStatusTbl[0]);i++)
    {
        if(m_PendStatusTbl[i].Action == Action)
        {
            return m_PendStatusTbl[i].PendStatus;
        }
    }

    /* Match not found*/
    return -1;
}
#if 0
/**
 * PendSetIPAddress
 *
 * @brief Set IP Address.
 **/
int PendSetIPAddress (INT8U *pData, INT32U DataLen,INT8U EthIndex,int BMCInst)
{
    NWCFG_STRUCT        NWConfig;
    NWCFG6_STRUCT        NWConfig6;

    //nwReadNWCfg(&NWConfig,EthIndex);
    nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, EthIndex,g_corefeatures.global_ipv6);
    memcpy (NWConfig.IPAddr, ((NWCFG_STRUCT*)pData)->IPAddr, IP_ADDR_LEN);
    //nwWriteNWCfg (&NWConfig,EthIndex);
    nwWriteNWCfg_ipv4_v6 ( &NWConfig, &NWConfig6, EthIndex);
    SetPendStatus (PEND_OP_SET_IP,PEND_STATUS_COMPLETED);

    return 0;
}


int PendSetSubnet(INT8U* pData,INT32U DataLen,INT8U EthIndex,int BMCInst)
{
    NWCFG_STRUCT        NWConfig;
    NWCFG6_STRUCT        NWConfig6;

    //nwReadNWCfg(&NWConfig,EthIndex);
    nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, EthIndex,g_corefeatures.global_ipv6);
    memcpy (NWConfig.Mask,  ((NWCFG_STRUCT*)pData)->Mask, IP_ADDR_LEN);
    //nwWriteNWCfg(&NWConfig,EthIndex);
    nwWriteNWCfg_ipv4_v6 ( &NWConfig, &NWConfig6, EthIndex);
    SetPendStatus (PEND_OP_SET_SUBNET,PEND_STATUS_COMPLETED);

    return 0;
}

int PendSetGateway(INT8U* pData, INT32U DataLen,INT8U EthIndex,int BMCInst)
{
    NWCFG_STRUCT    NWConfig;
    NWCFG6_STRUCT        NWConfig6;    
    unsigned char  NullGateway[4];
    INT32U GwIP;

    memset(NullGateway,0,IP_ADDR_LEN);

    //nwReadNWCfg(&NWConfig,EthIndex);
    nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, EthIndex,g_corefeatures.global_ipv6);    
    if(memcmp(((NWCFG_STRUCT*)pData)->Gateway,NullGateway,IP_ADDR_LEN)== 0)
    {
        nwGetBkupGWyAddr(((NWCFG_STRUCT*)pData)->Gateway,EthIndex);
    }
    memcpy (NWConfig.Gateway,((NWCFG_STRUCT*)pData)->Gateway, IP_ADDR_LEN);
    if(NWConfig.CfgMethod == CFGMETHOD_DHCP)
    {
        nwDelExistingGateway(EthIndex);
        memcpy((INT8U*)&GwIP,NWConfig.Gateway,IP_ADDR_LEN);
        nwSetGateway((INT8U*)&NWConfig.Gateway,EthIndex);
    }
    else
    {
        //nwWriteNWCfg(&NWConfig,EthIndex);
        nwWriteNWCfg_ipv4_v6 ( &NWConfig, &NWConfig6, EthIndex);
    }

    SetPendStatus (PEND_OP_SET_GATEWAY,PEND_STATUS_COMPLETED);

    return 0;
}

int PendSetSource(INT8U* pData, INT32U DataLen,INT8U EthIndex,int BMCInst)
{
    NWCFG_STRUCT        NWConfig;
    NWCFG6_STRUCT        NWConfig6;        
    DOMAINCONF      DomainCfg;
    DNSCONF     DNS;
    INT8U               regBMC_FQDN[MAX_LAN_CHANNELS];
    int i;

    memset(&DomainCfg,0,sizeof(DOMAINCONF));
    memset(&DNS,0,sizeof(DNSCONF));
    memset(regBMC_FQDN,0,sizeof(regBMC_FQDN));

    nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, EthIndex,g_corefeatures.global_ipv6);    
    NWConfig.CfgMethod = ((NWCFG_STRUCT*)pData)->CfgMethod;
    if(NWConfig.CfgMethod == CFGMETHOD_STATIC)
    {
        ReadDNSConfFile(&DomainCfg, &DNS,regBMC_FQDN);

        if(DomainCfg.EthIndex == EthIndex)
        {
            if(DomainCfg.v4v6 == 1)
                DomainCfg.dhcpEnable= 0;

            if(DNS.DNSDHCP == 1)
                DNS.DNSDHCP = 0;

            for(i=0;i<MAX_LAN_CHANNELS;i++)
            {
                if((regBMC_FQDN[i] & 0x10) == 0x10)
                    regBMC_FQDN[i] |=0x00;
            }
        }
        else
        {
            regBMC_FQDN[EthIndex] |= 0x0;
        }

        WriteDNSConfFile(&DomainCfg, &DNS, regBMC_FQDN);

        /*If the state changed to static, get the OEM configured static address from the Hook*/
        if(g_PDKHandle[PDK_GETSTATICNWCFG] != NULL)
        {
            ((int(*)(INT8U*, INT8U*, INT8U, int))g_PDKHandle[PDK_GETSTATICNWCFG]) ((INT8U *)&NWConfig, (INT8U *)&NWConfig6, EthIndex, BMCInst);
        }
    }
    nwWriteNWCfg_ipv4_v6 ( &NWConfig, &NWConfig6, EthIndex);    
    SetPendStatus (PEND_OP_SET_SOURCE,PEND_STATUS_COMPLETED);

    return 0;
}


int PendDelayedColdReset (INT8U* pData, INT32U DataLen,INT8U EthIndex,int BMCInst)
{
    OS_TIME_DELAY_HMSM (0, 0, 5, 0);
    /* PDK Module Post Set Reboot Cause*/
    if(g_PDKHandle[PDK_SETREBOOTCAUSE] != NULL)
    {
        ((INT8U(*)(INT8U,int)) g_PDKHandle[PDK_SETREBOOTCAUSE])(SETREBOOTCAUSE_IPMI_CMD_PROCESSING,BMCInst);
    }

    reboot (LINUX_REBOOT_CMD_RESTART);
    SetPendStatus (PEND_OP_DELAYED_COLD_RESET,PEND_STATUS_COMPLETED);
    return 0;
}
int  PendSendEmailAlert (INT8U *pData, INT32U DataLen,INT8U EthIndex,int BMCInst)
{
    /* Send the Email Alert */
    SetExtCfgReq_T* pSetCfgReq  =  ( SetExtCfgReq_T* ) pData;

    IPMI_DBG_PRINT_1 ("Send Email Alert  - %d \n",pSetCfgReq->Index);

    /* Since the Token Starts by 1 but the indexing of the array will be zero based.*/
    if(g_PDKHandle[PDK_FRAMEANDSENDMAIL] != NULL)
    {
        ((int(*)(INT8U,INT8U,int))g_PDKHandle[PDK_FRAMEANDSENDMAIL]) (pSetCfgReq->Index -1,EthIndex,BMCInst);
    }
    SetPendStatus (PEND_OP_SEND_EMAIL_ALERT,PEND_STATUS_COMPLETED);

    return 0;
}

/**
 *@fn PendSetVLANIfcID
 *@brief This function is invoked to set the valn id for the specified interface
 *@param pData -  Pointer to buffer where network configurations saved
 *@param DataLen -  unsigned integer parameter to buffer where length of the input data specified
 *@param EthIndex -  char value to bufferr where index for Ethernet channel is saved
 *@return Returns 0 on success
 */
int PendSetVLANIfcID(INT8U* pData,INT32U DataLen,INT8U EthIndex,int BMCInst)
{
    NWCFG_STRUCT        NWConfig;
    NWCFG6_STRUCT        NWConfig6;    
    char CmdsetVconfig[32];
    INT16U vlanID[MAX_LAN_CHANNELS]={0};
    int retValue = 0,Index;
    char Ifcname[16];
    DOMAINCONF      DomainCfg;
    DNSCONF     DNS;
    INT8U               regBMC_FQDN[MAX_LAN_CHANNELS];
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    SERVICE_CONF_STRUCT g_serviceconf;
    char cmdSetPriority[32];
    INT8U TOS = 0, SkbPriority = 0; /* For VLAN priority parameter */

    memset(&DomainCfg,0,sizeof(DOMAINCONF));
    memset(&DNS,0,sizeof(DNSCONF));
    memset(regBMC_FQDN,0,sizeof(regBMC_FQDN));
    memset(Ifcname,0,sizeof(Ifcname));

    nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, EthIndex,g_corefeatures.global_ipv6);
    NWConfig.VLANID =  ((NWCFG_STRUCT*)pData)->VLANID;

    TCRIT("\n %s:VLAN ID request received... VLANID = %d \n",__FUNCTION__,NWConfig.VLANID);

    /* Read VLAN IDs from the /conf/vlansetting.conf */
    if(ReadVLANFile(VLAN_ID_SETTING_STR, vlanID) == -1)
    {
        return -1;
    }

    if(ReadDNSConfFile(&DomainCfg, &DNS, regBMC_FQDN) != 0)
    {
        TCRIT("Error While Reading DNS configuration File\n");
    }

    /* Writing VLAN ID to VLAN setting file */
    if(WriteVLANFile(VLAN_ID_SETTING_STR, vlanID, EthIndex, NWConfig.VLANID) == -1)
    {
        return -1;
    }

    if(GetIfcName(pBMCInfo->LANConfig.g_ethindex,Ifcname,BMCInst) == -1)
    {
        return -1;
    }

    sprintf(CmdsetVconfig,"vconfig add %s %d",Ifcname,NWConfig.VLANID );

    if(((retValue = safe_system(CmdsetVconfig)) < 0))
    {
        TCRIT("ERROR %d: %s failed\n",retValue,IFUP_BIN_PATH);
    }

    sleep(2);

    if(ReadVLANFile(VLAN_ID_SETTING_STR, pBMCInfo->LANConfig.VLANID) == -1)
    {
        //return -1;
    }

    for(Index = 0; Index < MAX_SERVICE; Index++)
    {
        get_service_configurations(ModifyServiceNameList[Index],&g_serviceconf);
        if(strcmp(g_serviceconf.InterfaceName,Ifcname) == 0)
        {
            sprintf(g_serviceconf.InterfaceName,"%s.%d",Ifcname,NWConfig.VLANID);
            if(set_service_configurations(ModifyServiceNameList[Index],&g_serviceconf,g_corefeatures.timeoutd_sess_timeout) !=0)
            {
                TCRIT("Error in Setting service configuration for the service %s\n",ModifyServiceNameList[Index]);
            }
        }
    }

    nwWriteNWCfg_ipv4_v6 ( &NWConfig, &NWConfig6, EthIndex);

    if(WriteDNSConfFile(&DomainCfg, &DNS, regBMC_FQDN) != 0)
    {
        TCRIT("Error While Writing DNS configuration File\n");
    }
    if(g_corefeatures.vlan_priorityset == ENABLED)
    {
        /*vconfig set_egress_map <valninterface> <skb_buffer> <vlan-priority>*/
        sprintf(cmdSetPriority,"vconfig set_egress_map  %s.%d  0  %d",Ifcname,NWConfig.VLANID,pBMCInfo->LANCfs[EthIndex].VLANPriority);
        if(((retValue = safe_system(cmdSetPriority)) < 0))
        {
            TCRIT("ERROR %d: Set VLAN Priority failed\n",retValue);
        }
        /* 
         * Set priority of IPMI commands. 
         * The skb->priority value of IPMI command will be modified by TOS option.
         * So, use the mapping table to get the current value.
         */
        memset(&cmdSetPriority,0,sizeof(cmdSetPriority));
        TOS = pBMCInfo->LANCfs[EthIndex].Ipv4HdrParam.TypeOfService;
        SkbPriority = IP_TOS2PRIO[IPTOS_TOS(TOS)>>1];
        sprintf(cmdSetPriority,"vconfig set_egress_map %s.%d %d %d",Ifcname,NWConfig.VLANID,SkbPriority,pBMCInfo->LANCfs[EthIndex].VLANPriority);
        if(((retValue = safe_system(cmdSetPriority)) < 0))
        {
            TCRIT("ERROR %d: Set VLAN IPMI Priority failed\n",retValue);
        }
    }

    /*Restart the service to effect the changes*/
    for(Index = 0; Index < MAX_RESTART_SERVICE; Index++)
    {
        safe_system(RestartServices[Index]);
    }

    SetPendStatus (PEND_OP_SET_VLAN_ID,PEND_STATUS_COMPLETED);
    return 0;
}


/**
 *@fn PendDeConfigVLANInterface
 *@brief This function is invoked to de-configure vlan sockets
 *@param pData -  Pointer to buffer where network configurations saved
 *@param DataLen -  unsigned integer parameter to buffer where length of the input data specified
 *@param EthIndex -  char value to buffer where index for Ethernet channel is saved
 *@return Returns 0 on success
 */
int PendDeConfigVLANInterface(INT8U* pData, INT32U DataLen,INT8U EthIndex,int BMCInst)
{
    int retValue = 0,Index;
    INT16U vlanID[MAX_LAN_CHANNELS]={0};
    INT16U VLANPriorityLevel[MAX_LAN_CHANNELS];
    NWCFG_STRUCT        NWConfig;
    NWCFG6_STRUCT        NWConfig6 ;
    char str[40]=VLAN_NETWORK_DECONFIG_FILE;
    DOMAINCONF      DomainCfg;
    DNSCONF     DNS;
    char Ifcname[16] = {0};
    char tmpIfcname[16] = {0};
    SERVICE_CONF_STRUCT g_serviceconf;
    INT8U               regBMC_FQDN[MAX_LAN_CHANNELS];
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    memset(&DomainCfg,0,sizeof(DOMAINCONF));
    memset(&DNS,0,sizeof(DNSCONF));
    memset(regBMC_FQDN,0,sizeof(regBMC_FQDN));

    //nwReadNWCfg(&NWConfig, EthIndex);
    nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, EthIndex,g_corefeatures.global_ipv6);

    if(ReadDNSConfFile(&DomainCfg, &DNS, regBMC_FQDN) != 0)
    {
        TCRIT("Error While Reading DNS configuration File\n");
    }

    if(GetIfcName(pBMCInfo->LANConfig.g_ethindex,Ifcname,BMCInst) == -1)
    {
        return -1;
    }

    sprintf(str,"%s%d",str,EthIndex);
    if(((retValue = safe_system(str)) < 0))
    {
        TCRIT("ERROR %d: %s failed\n",retValue,IFUP_BIN_PATH);
    }
    /* Reset vlan setting files */
    if(ReadVLANFile(VLAN_ID_SETTING_STR, vlanID) == -1)
    {
        return -1;
    }

    if(WriteVLANFile(VLAN_ID_SETTING_STR, vlanID, EthIndex, 0) == -1)
    {
        return -1;
    }

    if(ReadVLANFile(VLAN_PRIORITY_SETTING_STR, VLANPriorityLevel) == -1)
    {
        return -1;
    }

    if(WriteVLANFile(VLAN_PRIORITY_SETTING_STR, VLANPriorityLevel, EthIndex, 0) == -1)
    {
        return -1;
    }

    sync();

    sleep(2);

    if(ReadVLANFile(VLAN_ID_SETTING_STR, pBMCInfo->LANConfig.VLANID) == -1)
    {
        //return -1;
    }

    for(Index = 0; Index < MAX_SERVICE; Index++)
    {
        get_service_configurations(ModifyServiceNameList[Index],&g_serviceconf);
        sprintf(tmpIfcname,"%s.%d",Ifcname,NWConfig.VLANID);
        if(strcmp(g_serviceconf.InterfaceName,tmpIfcname) == 0)
        {
            sprintf(g_serviceconf.InterfaceName,"%s",Ifcname);
            if(set_service_configurations(ModifyServiceNameList[Index],&g_serviceconf,g_corefeatures.timeoutd_sess_timeout) !=0)
            {
                TCRIT("Error in Setting service configuration for the service %s\n",ModifyServiceNameList[Index]);
            }
        }
    }

    /*Reset the VLANID*/
    NWConfig.VLANID=0;

    nwWriteNWCfg_ipv4_v6 ( &NWConfig, &NWConfig6, EthIndex);
    if(WriteDNSConfFile(&DomainCfg, &DNS, regBMC_FQDN) != 0)
    {
        TCRIT("Error While Writing DNS configuration File\n");
    }

    /*Restart the service to effect the changes*/
    for(Index = 0; Index < MAX_RESTART_SERVICE; Index++)
    {
        safe_system(RestartServices[Index]);
    }

    SetPendStatus (PEND_OP_DECONFIG_VLAN_IFC,PEND_STATUS_COMPLETED);
    return 0;
}


/**
 *@fn PendSetIPv4Headers
 *@brief This function is invoked to set IPv4 headers.
 *@param pData -  Pointer to buffer where network configurations saved
 *@param DataLen -  unsigned integer parameter to buffer where length of the input data specified
 *@param EthIndex -  char value to bufferr where index for Ethernet channel is saved
 *@return Returns 0 on success
 */
int PendSetIPv4Headers(INT8U* pData, INT32U DataLen,INT8U EthIndex,int BMCInst)
{
    MsgPkt_T MsgPkt;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    /* Send a Mesg Request to LANIFC task to reinitialize the LAN sockets after new VLAN IFC*/
    MsgPkt.Param =LAN_CONFIG_IPV4_HEADER;
    MsgPkt.Channel = pBMCInfo->LANConfig.g_ethindex;

    if(pBMCInfo->LANConfig.UDPSocket[EthIndex] != -1)
    {
        MsgPkt.Socket = pBMCInfo->LANConfig.UDPSocket[EthIndex];
    }

    if((pBMCInfo->IpmiConfig.VLANIfcSupport == 1) && (pBMCInfo->LANConfig.VLANID[EthIndex] != 0))
    {
        if(pBMCInfo->LANConfig.VLANUDPSocket[EthIndex] != -1)
        {
            MsgPkt.Socket = pBMCInfo->LANConfig.VLANUDPSocket[EthIndex];
        }
    }

    /* Post the request packet to LAN Interface Queue */
    if (0 != PostMsg (&MsgPkt, LAN_IFC_Q, BMCInst))
    {
        IPMI_WARNING ("LANIfc.c : Error posting message to LANIfc Q\n");
    }

    SetPendStatus (PEND_OP_SET_IPV4_HEADERS,PEND_STATUS_COMPLETED);
    return 0;
}

/*
 *@fn PendSetRMCPPort
 *@brief This function is invoked to change RMCP port number
 *@param pData -  Pointer to buffer which hold the data to be posted
 *@param DataLen -  Specifies the length of the message to be posted
 *@param EthIndex -  Ethernet Index
 */
int PendSetRMCPPort(INT8U* pData,INT32U Datalen,INT8U EthIndex,int BMCInst)
{
    MsgPkt_T MsgPkt;

    MsgPkt.Param = LAN_RMCP_PORT_CHANGE;

    /* Post the request packet to LAN Interface Queue */
    if(0 != PostMsg(&MsgPkt, LAN_IFC_Q,BMCInst))
    {
        IPMI_WARNING("LANIfc.c : Error posting message to LANIfc Q\n");
    }
    SetPendStatus (PEND_RMCP_PORT_CHANGE,PEND_STATUS_COMPLETED);
    return 0;
}

/*
 *@fn PendSetAllDNSCfg
 *@brief This function is invoked to set all dns configuration
 *@param pData -  Pointer to buffer which hold the data to be posted
 *@param DataLen -  Specifies the length of the message to be posted
 *@param EthIndex -  Ethernet Index
 */
int PendSetAllDNSCfg(INT8U* pData, INT32U DataLen, INT8U EthIndex,int BMCInst)
{

    HOSTNAMECONF HostnameConfig;
    DOMAINCONF DomainConfig;
    DNSCONF DnsIPConfig;
    INT8U regBMC_FQDN[MAX_LAN_CHANNELS];

    memset(&HostnameConfig, 0, sizeof(HostnameConfig));
    memset(&DomainConfig, 0, sizeof(DomainConfig));
    memset(&DnsIPConfig, 0, sizeof(DnsIPConfig));
    memset(regBMC_FQDN, 0, sizeof(regBMC_FQDN));

    nwGetAllDNSConf( &HostnameConfig, &DomainConfig, &DnsIPConfig,regBMC_FQDN );

    LOCK_BMC_SHARED_MEM(BMCInst);

    /*
     * Start of converting data
     */
    //DNS SERVER IP
    DnsIPConfig.DNSIndex = BMC_GET_SHARED_MEM (BMCInst)->DNSconf.DNSIndex;
    DnsIPConfig.DNSDHCP = BMC_GET_SHARED_MEM (BMCInst)->DNSconf.DNSDHCP;
    DnsIPConfig.IPPriority = BMC_GET_SHARED_MEM (BMCInst)->DNSconf.IPPriority;

    if(DnsIPConfig.DNSDHCP== 1)
    {
        memset(DnsIPConfig.DNSIP1,'\0',IP6_ADDR_LEN);
        memset(DnsIPConfig.DNSIP2,'\0',IP6_ADDR_LEN);
        memset(DnsIPConfig.DNSIP3,'\0',IP6_ADDR_LEN);
    }
    else
    {
        memcpy(DnsIPConfig.DNSIP1,BMC_GET_SHARED_MEM (BMCInst)->DNSconf.DNSIPAddr1,IP6_ADDR_LEN);
        memcpy(DnsIPConfig.DNSIP2,BMC_GET_SHARED_MEM (BMCInst)->DNSconf.DNSIPAddr2,IP6_ADDR_LEN);
        memcpy(DnsIPConfig.DNSIP3,BMC_GET_SHARED_MEM (BMCInst)->DNSconf.DNSIPAddr3,IP6_ADDR_LEN);
    }

    //DOMAIN NAME
    DomainConfig.EthIndex = BMC_GET_SHARED_MEM (BMCInst)->DNSconf.DomainIndex;
    DomainConfig.v4v6 = BMC_GET_SHARED_MEM (BMCInst)->DNSconf.Domainpriority; //To support both IPv4 and IPv6 in DNS_CONFIG.
    DomainConfig.dhcpEnable = BMC_GET_SHARED_MEM (BMCInst)->DNSconf.DomainDHCP;
    if(DomainConfig.dhcpEnable == 1)
    {
        DomainConfig.domainnamelen = 0;
        memset(DomainConfig.domainname,'\0',DNSCFG_MAX_DOMAIN_NAME_LEN);
    }
    else
    {
        DomainConfig.domainnamelen = BMC_GET_SHARED_MEM (BMCInst)->DNSconf.DomainLen;
        if(DomainConfig.domainnamelen <= MAX_DOMAIN_NAME_STRING_SIZE)
        {
            memset(&DomainConfig.domainname , '\0', sizeof(DomainConfig.domainname));
            strncpy((char *)&DomainConfig.domainname, (char *)&BMC_GET_SHARED_MEM (BMCInst)->DNSconf.DomainName, DomainConfig.domainnamelen);
        }
    }

    //Register BMC Flag
    memcpy(regBMC_FQDN, BMC_GET_SHARED_MEM (BMCInst)->DNSconf.RegisterBMC, MAX_LAN_CHANNELS);

    if(BMC_GET_SHARED_MEM (BMCInst)->DNSconf.HostNameLen <= MAX_HOST_NAME_STRING_SIZE)
    {
        HostnameConfig.HostNameLen = BMC_GET_SHARED_MEM (BMCInst)->DNSconf.HostNameLen;
    }
    else
    {
        HostnameConfig.HostNameLen = MAX_HOST_NAME_STRING_SIZE;
    }
    HostnameConfig.HostSetting = BMC_GET_SHARED_MEM (BMCInst)->DNSconf.HostSetting;

    memset(&HostnameConfig.HostName , '\0', sizeof(HostnameConfig.HostName));
    strncpy((char *)&HostnameConfig.HostName, (char *)&BMC_GET_SHARED_MEM (BMCInst)->DNSconf.HostName, HostnameConfig.HostNameLen);

    UNLOCK_BMC_SHARED_MEM(BMCInst);
    /* End of converting data */

    nwSetAllDNSConf( &HostnameConfig, &DomainConfig, &DnsIPConfig, regBMC_FQDN);

    SetPendStatus (PEND_OP_SET_ALL_DNS_CFG, PEND_STATUS_COMPLETED);
    return 0;
}

static int PendSetIPv6Cfg(INT8U* pData, INT32U DataLen, INT8U EthIndex,int BMCInst)
{
    INT8U ethIndex = GetEthIndex( g_BMCInfo[BMCInst].Msghndlr.CurChannel,BMCInst);
    if (0xff == ethIndex) ethIndex = 0;

    NWCFG_STRUCT cfgIPv4;
    NWCFG6_STRUCT* newIPv6Cfg = (NWCFG6_STRUCT*)pData;

    nwReadNWCfg_v4_v6(&cfgIPv4, NULL, ethIndex,g_corefeatures.global_ipv6);
    nwWriteNWCfg_ipv4_v6(&cfgIPv4, newIPv6Cfg, ethIndex);

    SetPendStatus (PEND_OP_SET_IPV6_CFG, PEND_STATUS_COMPLETED);

    return 0;
}

/*
 *@fn PendSetIfaceState
 *@brief This function is invoked to set network interface state to enable or disable
 *@param pData -  Pointer to buffer which hold the data to be posted
 *@param DataLen -  Specifies the length of the message to be posted
 *@param EthIndex -  Ethernet Index
 */
int PendSetEthIfaceState(INT8U* pData, INT32U DataLen, INT8U EthIndex,int BMCInst)
{
    BMCInfo_t *pBMCInfo = &g_BMCInfo[BMCInst];
    EthIfaceState* pReq = (EthIfaceState*)pData;
    NWCFG_STRUCT cfg;
    NWCFG6_STRUCT cfg6;
    int retValue = 0;
    int eth_index = 0;

    eth_index = pReq->EthIndex;

    retValue = nwReadNWCfg_v4_v6(&cfg, &cfg6, eth_index,g_corefeatures.global_ipv6);
    if(retValue != 0)
        TCRIT("Error in reading network configuration.\n");

    switch(pReq->EnableState)
    {
        case DISABLE_V4_V6:
            cfg.enable = 0;
            cfg6.enable = 0;
            break;

        case ENABLE_V4:
            cfg.enable = 1;
            cfg6.enable = 0;
            break;

        case ENABLE_V4_V6:
            cfg.enable = 1;
            cfg6.enable = 1;
            break;

        case ENABLE_V6:
            cfg.enable  = 0;
            cfg6.enable = 1;
            break;

        default:
            SetPendStatus (PEND_OP_SET_ETH_IFACE_STATE, PEND_STATUS_COMPLETED);
            return -1;
            break;
    }

    cfg.CfgMethod = pBMCInfo->LANCfs[eth_index].IPAddrSrc;
    cfg6.CfgMethod = pBMCInfo->LANCfs[eth_index].IPv6_IPAddrSrc;

    if(cfg.CfgMethod == STATIC_IP_SOURCE)
    {
        //Set IP address
        _fmemcpy (&cfg.IPAddr, pBMCInfo->LANCfs[eth_index].IPAddr, IP_ADDR_LEN);
        //Set subnet mask
        _fmemcpy (&cfg.Mask, pBMCInfo->LANCfs[eth_index].SubNetMask, IP_ADDR_LEN);
        //Set default gateway
        _fmemcpy (&cfg.Gateway, pBMCInfo->LANCfs[eth_index].DefaultGatewayIPAddr, IP_ADDR_LEN);
    }

    if(cfg6.CfgMethod == STATIC_IP_SOURCE)
    {
        _fmemcpy(&cfg6.GlobalIPAddr[0],pBMCInfo->LANCfs[eth_index].IPv6_IPAddr,IP6_ADDR_LEN);
        _fmemcpy(&cfg6.Gateway,pBMCInfo->LANCfs[eth_index].IPv6_GatewayIPAddr,IP6_ADDR_LEN);
        cfg6.GlobalPrefix[0] = pBMCInfo->LANCfs[eth_index].IPv6_PrefixLen;
    }

    if(cfg6.enable == 0)
        retValue = nwWriteNWCfg_ipv4_v6(&cfg, NULL, eth_index);
    else
        retValue = nwWriteNWCfg_ipv4_v6(&cfg, &cfg6, eth_index);

    if(retValue != 0)
        TCRIT("Error in writing network configuration.\n");

    SetPendStatus (PEND_OP_SET_ETH_IFACE_STATE, PEND_STATUS_COMPLETED);

    return 0;
}

static int PendSetMACAddress(INT8U* pData, INT32U DataLen, INT8U EthIndex, int BMCInst)
{
    EnableSetMACAddress_T* data = (EnableSetMACAddress_T*)pData;
    char oldMACAddrString[18], newMACAddrString[18];
    char ethName[15];
    int result = 0;
    int SockFD = 0;
    struct ifreq IfReq;
    char *ParseMac = NULL;
    int Index = 0;
    unsigned long ConvMac = 0;
    int InterfaceEnabled = 0;
    char Cmd[128] = "/etc/init.d/networking restart";

    memset(oldMACAddrString, 0, sizeof(oldMACAddrString));
    memset(newMACAddrString, 0, sizeof(newMACAddrString));
    memset(ethName, 0, sizeof(ethName));

    // Frame the MAC Address and the Interface name
    sprintf(newMACAddrString, "%02x:%02x:%02x:%02x:%02x:%02x", 
            data->MACAddress[0], data->MACAddress[1], 
            data->MACAddress[2], data->MACAddress[3], 
            data->MACAddress[4], data->MACAddress[5]);

    /*	zc modify for eth1 can't set MAC by ipmitool
        if (date->EthIndex > 0 && data->EthIndex != 0xFF)
        sprintf(ethName, "eth%daddr", data->EthIndex);
        else
        strcpy(ethName, "data->ethaddr");
     */
    if (EthIndex > 0 && EthIndex != 0xFF)
        sprintf(ethName, "eth%daddr", EthIndex);
    else
        strcpy(ethName, "ethaddr");

    /* ------- This section sets the MAC Address in U-Boot Environment --------- */

    // Get the old MAC Address from the U-Boot Environment
    GetUBootParam(ethName, oldMACAddrString);

    // Compare with our new MAC to confirm if it is indeed a change of MAC
    if (memcmp(oldMACAddrString, newMACAddrString, strlen(newMACAddrString)) == 0)
    {
        result = 0;
        goto clear_exit;
    }

    // Update the U-Boot environment with the updated MAC Address
    result = SetUBootParam(ethName, newMACAddrString);
    if (result != 0)
    {
        result = -1;
        goto clear_exit;
    }	

    /* --------------------- End of U-Boot section --------------------------*/

    /*If the bond interface is enabled, reboot is needed*/
    //zc-	if(CheckBondSlave(date->EthIndex) == 1)
    if(CheckBondSlave(EthIndex) == 1)
    {
        TDBG("Given index is slave of bond interface\n");
        SetPendStatus(PEND_OP_SET_MAC_ADDRESS, PEND_STATUS_COMPLETED);
        /* PDK Module Post Set Reboot Cause*/
        if(g_PDKHandle[PDK_SETREBOOTCAUSE] != NULL)
        {
            ((INT8U(*)(INT8U,int)) g_PDKHandle[PDK_SETREBOOTCAUSE])(SETREBOOTCAUSE_IPMI_CMD_PROCESSING,BMCInst);
        }
        reboot(LINUX_REBOOT_CMD_RESTART);
        return 0;
    }

    /* ------- This section sets the MAC Address Temporarily in Linux environment --------- */
    /* ------- Upon next reboot, the new mac will take effect permanently 	      --------- */

    // Frame the Interface name based on the EthIndex
    /* zc -	
       if (date->EthIndex != 0xFF)
       {
       sprintf(ethName, "eth%d", date->EthIndex);
       }
     */	
    if (EthIndex != 0xFF)
    {
        sprintf(ethName, "eth%d", EthIndex);
    }	

    // Set the Interface name and its family
    sprintf(IfReq.ifr_name, "%s", ethName);
    IfReq.ifr_hwaddr.sa_family = AF_INET;

    // open a socket to the interface
    SockFD = socket(AF_INET, SOCK_STREAM, 0);
    if(SockFD < 0)
    {
        goto clear_exit;
    }

    // Read the Interface status and identify it is already UP
    result = ioctl(SockFD, SIOCGIFFLAGS, (char *)&IfReq);
    if (result < 0)
    {
        goto clear_exit;
    }		

    if (IfReq.ifr_flags & IFF_UP)
    {
        InterfaceEnabled = 1;
    }

    // If Interface is already UP, then disable the interface temporarily
    if (InterfaceEnabled)	
    {
        // disable the interface by setting the flag
        IfReq.ifr_flags &= ~(IFF_UP);

        // apply the flag by using the ioctl
        result = ioctl(SockFD, SIOCSIFFLAGS, (char *)&IfReq);
        if (result < 0) 
        {
            goto clear_exit;
        }
    }

    // Getting the Current MAC Address from the interface
    // This also fills the structure with proper data for all fields
    result = ioctl(SockFD, SIOCGIFHWADDR, (char *)&IfReq);
    if(result < 0)
    {
        goto clear_exit;
    }

    // Just replace the Data field of the structure with our new MAC Address
    // Have to convert the string to Data happy format
    ParseMac = strtok(newMACAddrString, ":");
    do
    {
        if (ParseMac)
        {
            sscanf(ParseMac, "%lx", &ConvMac);
            IfReq.ifr_hwaddr.sa_data[Index] = (char)(ConvMac);
            ParseMac = NULL;
        }
        else
        {
            break;
        }

        Index++;
        ParseMac = strtok(NULL, ":");
    } while (ParseMac != NULL);

    if ((ParseMac == NULL) && (Index < IFHWADDRLEN))
    {
        result = -1;
        goto clear_exit;
    }

    // Set the MAC Address for the interface
    result = ioctl(SockFD, SIOCSIFHWADDR, (char *)&IfReq);
    if (result < 0) 
    {
        goto clear_exit;
    }

    // If interface was enabled before, then bring back up the interface again
    // Else, we need not bring back up the interface, as it was already down
    if (InterfaceEnabled)
    {
        // enable the interface by setting the flag
        IfReq.ifr_flags |= (IFF_UP);

        // set the new flags for interface
        result = ioctl(SockFD, SIOCSIFFLAGS, (char *)&IfReq);
        if (result < 0) 
        {
            goto clear_exit;
        }

        safe_system(Cmd);
        OS_TIME_DELAY_HMSM (0, 0, 2, 0);
    }

    /* --------------------- End of Linux section --------------------------*/

clear_exit:
    // Close the socket if existing
    if (SockFD > 0)
        close(SockFD);

    // Set the status to COMPLETED before returning
    SetPendStatus(PEND_OP_SET_MAC_ADDRESS, PEND_STATUS_COMPLETED);

    return result;
}

static int PendSetIPv6Enable(INT8U* pData, INT32U DataLen, INT8U EthIndex, int BMCInst)
{
    NWCFG_STRUCT        NWConfig;
    NWCFG6_STRUCT        NWConfig6;

    nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, EthIndex,g_corefeatures.global_ipv6);
    NWConfig6.enable = ((NWCFG6_STRUCT*)pData)->enable;
    nwWriteNWCfg_ipv4_v6( &NWConfig, &NWConfig6,EthIndex);
    SetPendStatus (PEND_OP_SET_IPV6_ENABLE,PEND_STATUS_COMPLETED);

    return 0;

}

static int PendSetIPv6Source(INT8U* pData, INT32U DataLen, INT8U EthIndex, int BMCInst)
{
    NWCFG_STRUCT        NWConfig;
    NWCFG6_STRUCT        NWConfig6;
    DOMAINCONF      DomainCfg;
    DNSCONF     DNS;
    INT8U               regBMC_FQDN[MAX_LAN_CHANNELS];

    memset(&DomainCfg,0,sizeof(DOMAINCONF));
    memset(&DNS,0,sizeof(DNSCONF));
    memset(regBMC_FQDN,0,sizeof(regBMC_FQDN));

    nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, EthIndex,g_corefeatures.global_ipv6);
    NWConfig6.CfgMethod= ((NWCFG6_STRUCT*)pData)->CfgMethod;

    if(NWConfig6.CfgMethod == CFGMETHOD_STATIC)
    {
        ReadDNSConfFile(&DomainCfg, &DNS, regBMC_FQDN);

        if(DomainCfg.v4v6 == 2)
            DomainCfg.v4v6 = 0;

        if(DNS.DNSDHCP== 1)
            DNS.DNSDHCP= 0;

        WriteDNSConfFile(&DomainCfg, &DNS, regBMC_FQDN);
    }

    nwWriteNWCfg_ipv4_v6( &NWConfig, &NWConfig6,EthIndex);
    SetPendStatus (PEND_OP_SET_IPV6_IP_ADDR_SOURCE,PEND_STATUS_COMPLETED);

    return 0;

}

static int PendSetIPv6Address(INT8U* pData, INT32U DataLen, INT8U EthIndex, int BMCInst)
{
    NWCFG_STRUCT        NWConfig;
    NWCFG6_STRUCT        NWConfig6;
    IPv6Addr_T                  *NewIPv6Addr = (IPv6Addr_T *)pData;

    nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, EthIndex,g_corefeatures.global_ipv6);

    memcpy( NWConfig6.GlobalIPAddr[(NewIPv6Addr->IPv6_Cntr & 0x0F)], NewIPv6Addr->IPv6_IPAddr, IP6_ADDR_LEN );
    nwWriteNWCfg_ipv4_v6( &NWConfig, &NWConfig6,EthIndex);
    SetPendStatus (PEND_OP_SET_IPV6_IP_ADDR,PEND_STATUS_COMPLETED);

    return 0;

}

static int PendSetIPv6PrefixLength(INT8U* pData, INT32U DataLen, INT8U EthIndex, int BMCInst)
{
    NWCFG_STRUCT        NWConfig;
    NWCFG6_STRUCT        NWConfig6;
    IPv6Prefix_T       *NewIPv6Prefix = (IPv6Prefix_T *)pData;

    nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, EthIndex,g_corefeatures.global_ipv6);
    NWConfig6.GlobalPrefix[(NewIPv6Prefix->IPv6_Prepos & 0x0F)] = NewIPv6Prefix->IPv6_PrefixLen;
    nwWriteNWCfg_ipv4_v6( &NWConfig, &NWConfig6,EthIndex);
    SetPendStatus (PEND_OP_SET_IPV6_PREFIX_LENGTH,PEND_STATUS_COMPLETED);

    return 0;

}

static int PendSetIPv6Gateway(INT8U* pData, INT32U DataLen, INT8U EthIndex, int BMCInst)
{
    NWCFG_STRUCT        NWConfig;
    NWCFG6_STRUCT        NWConfig6;

    nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, EthIndex,g_corefeatures.global_ipv6);

    memcpy( NWConfig6.Gateway, ((NWCFG6_STRUCT*)pData)->Gateway, IP6_ADDR_LEN );

    nwWriteNWCfg_ipv4_v6( &NWConfig, &NWConfig6,EthIndex);
    SetPendStatus (PEND_OP_SET_IPV6_GATEWAY,PEND_STATUS_COMPLETED);

    return 0;

}

/*
 *@fn PendConfigBonding
 *@brief This function is invoked to Enable/Disable the Bonding Support
 *@param pData -  Pointer to buffer which hold the data to be posted
 *@param DataLen -  Specifies the length of the message to be posted
 *@param EthIndex -  Ethernet Index
 */
static int PendConfigBonding(INT8U * pData, INT32U DataLen, INT8U EthIndex, int BMCInst)
{
    BondIface*pConfigBond = (BondIface*)pData;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    INT8S   Ifcname[IFNAMSIZ];
    INT8U i,Channel=0,Ethindex=0xff;
    INT16U vlanID[MAX_LAN_CHANNELS]={0};
    INT16U VLANPriorityLevel[MAX_LAN_CHANNELS];


    memset(Ifcname,0,sizeof(Ifcname));
    sprintf(Ifcname,"bond%d",pConfigBond->BondIndex);

    for(i=0;i<MAX_LAN_CHANNELS;i++)
    {
        if(strcmp(pBMCInfo->LANConfig.LanIfcConfig[i].ifname,Ifcname) == 0)
        {
            Channel=pBMCInfo->LANConfig.LanIfcConfig[i].Chnum;
        }
    }

    Ethindex=GetEthIndex(Channel, BMCInst);
    if(Ethindex == 0xff)
    {
        TCRIT("Error in getting Ethindex");
        SetPendStatus (PEND_OP_SET_BOND_IFACE_STATE,PEND_STATUS_COMPLETED);
        return 0;
    }

    /* Read VLAN ID for bond interface */
    if(ReadVLANFile(VLAN_ID_SETTING_STR, vlanID) == -1)
    {
        //return -1;
    }

    nwConfigureBonding(pConfigBond,Ethindex,g_corefeatures.timeoutd_sess_timeout,g_corefeatures.global_ipv6);

    /*Disable the VLAN interface properly before disabling bond interface*/
    if(pConfigBond->Enable == 0 && vlanID[EthIndex] != 0)
    {
        if(WriteVLANFile(VLAN_ID_SETTING_STR, vlanID, EthIndex, 0) == -1)
        {
            return -1;
        }

        if(ReadVLANFile(VLAN_PRIORITY_SETTING_STR, VLANPriorityLevel) == -1)
        {
            return -1;
        }

        if(WriteVLANFile(VLAN_PRIORITY_SETTING_STR, VLANPriorityLevel, EthIndex, 0) == -1)
        {
            return -1;
        }

        if(ReadVLANFile(VLAN_ID_SETTING_STR, pBMCInfo->LANConfig.VLANID) == -1)
        {
            //return -1;
        }

        /*Update the NVRAM Configuration*/
        pBMCInfo->LANCfs[Ethindex].VLANID = 0;
        FlushIPMI((INT8U*)&pBMCInfo->LANCfs[0],(INT8U*)&pBMCInfo->LANCfs[Ethindex],pBMCInfo->IPMIConfLoc.LANCfsAddr,
                sizeof(LANConfig_T),BMCInst);
    }

    SetPendStatus (PEND_OP_SET_BOND_IFACE_STATE,PEND_STATUS_COMPLETED);

    return 0;
}

static int PendActiveSlave(INT8U * pData, INT32U DataLen, INT8U Ethindex, int BMCInst)
{
    ActiveSlave_T * pReq= (ActiveSlave_T *)pData;

    nwActiveSlave(pReq->BondIndex,pReq->ActiveIndex);
    SetPendStatus(PEND_OP_SET_ACTIVE_SLAVE,PEND_STATUS_COMPLETED);
    return 0;
}

static int PendRestartServices(INT8U *pData, INT32U DataLen, INT8U Ethindex,int BMCInst)
{
    BMCInfo_t *pBMCInfo = &g_BMCInfo[BMCInst];
    RestartService_T *pReq = (RestartService_T *) pData;
    int i;
    struct stat buf;

    if(pReq->ServiceName == IPMI)
    {
        close(pBMCInfo->UDSConfig.UDSSocket);	

        for(i=0;i<MAX_LAN_CHANNELS;i++)
        {		
            if (-1 != pBMCInfo->LANConfig.TCPSocket[i]) {
                shutdown(pBMCInfo->LANConfig.TCPSocket[i],SHUT_RDWR);
                close(pBMCInfo->LANConfig.TCPSocket[i]);
                pBMCInfo->LANConfig.TCPSocket[i] = 0;
            }
        }

        safe_system("/etc/init.d/ipmistack restart");
    }
    else if(pReq->ServiceName == WEBSERVER)
    {
        //remove tmp/sdr_data file, to reflect the appropriate SDR change in webUI
        unlink("/var/tmp/sdr_data");

        //restart the webserver to avoid the websession hang
        if(stat("/etc/init.d/webgo.sh",&buf) == 0)
        {
            safe_system("/etc/init.d/webgo.sh restart &");
        }
        else if(stat("/etc/init.d/lighttpd.sh",&buf) == 0)
        {
            safe_system("/etc/init.d/lighttpd.sh restart &");
        }

    }
    else if(pReq->ServiceName == REBOOT)
    {	
        sleep(pReq->SleepSeconds);

        //reboot the BMC stack
        reboot (LINUX_REBOOT_CMD_RESTART);
    }

    SetPendStatus(PEND_OP_RESTART_SERVICES,PEND_STATUS_COMPLETED);
    return 0;
}

static int PendStartFwUpdate_Tftp(INT8U *pData, INT32U DataLen, INT8U Ethindex,int BMCInst)
{
    FirmwareConfig_T *FwConfig = (FirmwareConfig_T*)pData;
    unsigned char PreserveCfg = 0;
    unsigned char ResetBMC = 0;
    int ProgressState = 0;
    unsigned char ProtocolType;
    ImageVerificationInfo VeriInfo;
    STRUCTURED_FLASH_PROGRESS flprog;
    STRUCTURED_DOWNLOAD_PROGRESS dlprog;

    PreserveCfg = FwConfig->Protocol.tftp.PreserveCfg;
    ProtocolType = FwConfig->ProtocolType;
    memset(&VeriInfo,0,sizeof(ImageVerificationInfo));
    memset(&flprog, 0, sizeof(STRUCTURED_FLASH_PROGRESS));
    memset(&dlprog, 0, sizeof(STRUCTURED_DOWNLOAD_PROGRESS));

    TINFO("PrepareFlashArea...\n");
    PrepareFlashArea(FLSH_CMD_PREP_TFTP_FLASH_AREA, g_corefeatures.dual_image_support);

    sleep(2);

    TWARN("DownloadFwImage...\n");
    DownloadFwImage(ProtocolType);
    do {
        sleep(2);
        ProgressState = GetDownloadProgress(&dlprog);
        TDBG("DL progress - %s , %s, %d, %d\n", dlprog.SubAction, dlprog.Progress, dlprog.State, ProgressState);
    } while(!ProgressState);

    if(dlprog.State == DLSTATE_ERROR)
        return 0;

    sleep(2);

    TDBG("VerifyFirmwareImage\n");
    VerifyFirmwareImage(&VeriInfo);

    TDBG("StartImageFlash\n");
    StartImageFlash(PreserveCfg, ResetBMC);
    do {
        sleep(2);
        ProgressState = GetFlashProgress(&flprog);

        if((flprog.State == FLSTATE_DOING) || (flprog.State == FLSTATE_TOBEDONE)) {
            continue;
        }

        printf("FL progress - %s , %s, %d, %d\n", flprog.SubAction, flprog.Progress, flprog.State, ProgressState);
    } while(!ProgressState);

    TCRIT("FLASH Complete...\n");
    if( (g_corefeatures.dual_image_support == ENABLED) && ( g_corefeatures.online_flashing_support == ENABLED) )
    {
        /* Need to do some basic setups to make ready Active flasher for next flash if it supports online flashing
           AbortFlash will do settings accordingly.*/
        AbortFlash(g_corefeatures.dual_image_support);
    }
    SetPendStatus(PEND_OP_START_FW_UPDATE_TFTP, PEND_STATUS_COMPLETED);
    return 0;
}

static int PendSetNCSIChannelID(INT8U *pData, INT32U DataLen, INT8U Ethindex,int BMCInst)
{	
    int ret = 0;

    if(safe_system("/usr/local/bin/ncsicfg")<0)
    {
        ret = -1;
    }	

    SetPendStatus(PEND_OP_SET_NCSI_CHANNEL_ID, PEND_STATUS_COMPLETED);

    return ret;
}
#endif
