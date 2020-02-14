/*****************************************************************
 *****************************************************************
 ***                                                            **
 ***    (C)Copyright 2005-2006, American Megatrends Inc.        **
 ***                                                            **
 ***            All Rights Reserved.                            **
 ***                                                            **
 ***        6145-F, Northbelt Parkway, Norcross,                **
 ***                                                            **
 ***        Georgia - 30071, USA. Phone-(770)-246-8600.         **
 ***                                                            **
 *****************************************************************
 ******************************************************************
 *
 * lanconfig.c
 * Lan Configuration functions.
 *
 *  Author: Bakka Ravinder Reddy <bakkar@ami.com>
 *
 ******************************************************************/
#define ENABLE_DEBUG_MACROS 0
#include "IPMIConf.h"
#include "LANConfig.h"
#include "MsgHndlr.h"
#include "Debug.h"
#include "Support.h"
#include "IPMI_LANConfig.h"
#include "PMConfig.h"
#include "SharedMem.h"
#include "IPMIDefs.h"
//#include "NVRAccess.h"
#include "Util.h"
#include "Session.h"
//#include "WDT.h"
#include "LANIfc.h"
#include "AppDevice.h"
//#include "RMCP+.h"
#include "IPMI_Main.h"
#include "IPMI_LANConfig.h"
#include "nwcfg.h"
#include "PendTask.h"
//#include "Ciphertable.h"
#include "Ethaddr.h"
//#include "sendarp.h"
//#include "PDKAccess.h"
#include "Message.h"
#include <linux/if.h>
//#include "Badpasswd.h"
//#include "libncsiconf.h"
#include <linux/ip.h>
#include <dlfcn.h>
#include<sys/prctl.h>
#include "featuredef.h"
//#include <flashlib.h>


/* Reserved bit macro definitions */
#define RESERVED_BITS_SUSPENDBMCARPS                    0xF0 //(BIT7 | BIT6 | BIT5 | BIT4)
#define RESERVED_BITS_GETIPUDPRMCPSTATS_CH              0xF0 //(BIT7 | BIT6 | BIT5 | BIT4)
#define RESERVED_BITS_GETIPUDPRMCPSTATS_CLRSTATE        0xFE //(BIT7 | BIT6 | BIT5 | BIT4 | BIT3 | BIT2 | BIT1)

/*** Local definitions ***/
#define CHANNEL_ID_MASK                         0x0f
#define SET_IN_PROGRESS_MASK                    0x03
#define PARAMETER_REVISION_MASK                 0x0f
#define DEST_ADDR_DATA2_ADDR_FORMAT_MASK        0xf0
#define PARAMETER_REVISION_FORMAT               0x11
#define GET_PARAMETER_REVISION_MASK             0x80
#define LAN_CALLBACK_AUTH_SUPPORT               0x17    /* MD2 & MD5 supported  */
#define LAN_USER_AUTH_SUPPORT                   0x17    /* MD2 & MD5 supported  */
#define LAN_OPERATOR_AUTH_SUPPORT               0x17    /* MD2 & MD5 supported  */
#define LAN_ADMIN_AUTH_SUPPORT                  0x17    /* MD2 & MD5 supported  */
#define BRD_CAST_BIT_MASK                       0xFF
#define LOOP_BACK_BIT_MASK                      0x7F
#define SUBNET_MASK_BIT_CHECK                   0x80

#define LAN_CONFIGURATION_SET_IN_PROGRESS       0x01
#define LAN_CONFIGURATION_SET_COMPLETE          0x00

#define GRATIUTOUS_ENABLE_MASK                  1
#define ENABLE_ARP_RESPONSES                    2
#define SUSPEND_ARP_RSVD_BIT_MASK               0xFC
#define ENABLE_ARPS                             0x03
#define SUSPEND_GRAT_ARP                        0x01
#define SUSPEND_ARP                             0x02

#define ARP_IGNORE_ON	8
#define ARP_IGNORE_OFF	0

#define VLAN_MASK_BIT  0x8000       /* VLAN enable bit */


/* Reserved Bits */
#define RESERVED_VALUE_70						0x70
#define RESERVED_VALUE_F0						0xF0

/**
 *@fn NwInterfacePresenceCheck
 *@brief This function is invoked to check network interface presence
 *@param Interface - Char Pointer to buffer for which interface to check
 */
static int NwInterfacePresenceCheck (char * Interface);

/*** Module Variables ***/
//_FAR_        INT8U  m_ArpSuspendReq;

char **explode(char separator, char *string);
int IPAddrCheck(INT8U *Addr,int params);
extern int GetLanAMIParamValue (INT8U* ParamSelect, INT8U* ImpType);

extern IfcName_T Ifcnametable[MAX_LAN_CHANNELS];
#define MAX_LAN_PARAMS_DATA  20
typedef struct
{
    INT8U	Params;
    INT8U	ReservedBits [MAX_LAN_PARAMS_DATA];
    INT8U	DataLen;

} LANCfgRsvdBits_T;

static LANCfgRsvdBits_T m_RsvdBitsCheck [] = {

    /* Param                 Reserved Bits                    Data Size   */
    { 0,	     			{ 0xFC }, 				 0x1 },	/* Set In progress  */
    { 1,				{ 0xC8 },					 0x1 }, 	 /* Authenication type */
    { 2,				{ 0xC8,0xC8,0xC8,0xC8,0xC8 }, 0x5}, 	 /* Authenication Support Enable  */
    { 4,				{ 0xF0 },					 0x1 }, 	 /* l */
    { 7,				{ 0x0,0x1F,0x01 },			 0x3 }, 	 /* l */
    { 0xA,				{ 0xFC },					 0x1 }, 	 /* l */
    { 0x11,			{ 0xF0 },					 0x1 }, 	 /* l */
    { 0x12,			{ 0xF0,0x78,0x0,0xF8 },		 0x4 },
    { 0x13,			{ 0xF0,0x0F, 0xFE },		 0x3 },
    { 0x14,			{ 0x0,0x70},				 0x2 },
    { 0x15,			{ 0xF8 },					 0x1 },
    { 0x16,			{ 0xE0 },					 0x1 },
    { 0x17,			{ 0xFF },					 0x1 },
    { 0x18,			{ 0xFF },					 0x1 },
    { 0x19,			{ 0xF0,0x0F },				 0x2 },
    { 0x1A,			{ 0xFE },				 	 0x1 }
};



/**
 * @brief LAN configuration request parameter lengths
 **/
static const INT8U LanconfigParameterLength [] = {
    1,  /* Set in progress */
    1,  /* Authentication type support */
    5,  /* Authentication type enables */
    4,  /* IP address */
    1,  /* IP address source */
    6,  /* MAC address */
    4,  /* Subnet mask */
    3,  /* IPv4 header parameters */
    2,  /* Primary RMCP port number */
    2,  /* Secondary RMCP port number */
    1,  /* BMC generated ARP control */
    1,  /* Gratuitous ARP */
    4,  /* Default Gateway address */
    6,  /* Default Gateway MAC address */
    4,  /* Backup Gateway address */
    6,  /* Backup Gateway MAC address */
    18, /* Community string */
    1,  /* Number of destinations */
    4,  /* Destination type */
    13, /* Destination address */
    2,  /* VLAN ID */
    1,  /* VLAN Priority */
    1,  /* Cipher suite entry support */
    17, /* Cipher suite entries */
    9,  /* Cipher suite Privilege levels */
    4,  /* VLAN tags destination address  */
    6,  /* Bad Password Threshold */
    (9+16) /* IPv6 Destination address */
};

/* A copy of ip_tos2prio with numeric format in "linux/net/ipv4/route.c" */ 
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
static BOOL enableSetMACAddr = FALSE;

/*** Global Variables ***/
const unsigned char  g_CipherRec [MAX_CIPHER_SUITES_BYTES] =
{
    /* Standard records */
    /*- Start -- ID --- Auth --- Intgr - Conf-*/
    0xC0,   0x00,   0x00,   0x40,   0x80,
    0xC0,   0x01,   0x01,   0x40,   0x80,
    0xC0,   0x02,   0x01,   0x41,   0x80,
    0xC0,   0x03,   0x01,   0x41,   0x81,
    0xC0,   0x06,   0x02,   0x40,   0x80,
    0xC0,   0x07,   0x02,   0x42,   0x80,
    0xC0,   0x08,   0x02,   0x42,   0x81,
    0xC0,   0x0B,   0x02,   0x43,   0x80,
    0xC0,   0x0C,   0x02,   0x43,   0x81,
    0xC0,   0x0F,   0x03,   0x40,   0x80,
    0xC0,   0x10,   0x03,   0x44,   0x80,
    0xC0,   0x11,   0x03,   0x44,   0x81,
    /*- Start --OEM-ID -- OEM-IANA - Auth --- Intgr - Conf-*/
};

/*-------------------------------------------------------
 * SetLanConfigParam
 *-------------------------------------------------------*/
    int
SetLanConfigParam (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  SetLanConfigReq_T*  pSetLanReq = (_NEAR_ SetLanConfigReq_T*) pReq;
    _NEAR_  SetLanConfigRes_T*  pSetLanRes = (_NEAR_ SetLanConfigRes_T*) pRes;
    _FAR_   BMCSharedMem_T*     pSharedMem = BMC_GET_SHARED_MEM (BMCInst);
    _FAR_   ChannelInfo_T*      ptrChannelInfo;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    NWCFG_STRUCT        NWConfig;
    NWCFG6_STRUCT      NWConfig6;
    INT32U GWIp,IPAddr;
    INT32U Subnetmask;
    int i,j=0;
    INT8U IsOemDefined = FALSE;
    INT8U EthIndex,netindex= 0xFF, currBmcGenArpCtrl;
    BMCArg *pLANArg=NULL; 

    char    VLANInterfaceName [32];
    INT16U vlanID=0;
    INT16U InvalidVlanID[3]={0,1,4095};
    char cmdSetPriority[32];
    int retValue=0,NIC_Count = 0;
    INT16U PriorityLevel[MAX_LAN_CHANNELS]= {0};
    INT8U  m_Lan_SetInProgress; /**< Contains setting LAN configuration status */

    static INT8U macEthIndex = 0xFF;
    INT8U TOS = 0, SkbPriority = 0; /* For VLAN priority parameter */
    int pendStatus = PEND_STATUS_COMPLETED;
    char IfcName[16];     /* Eth interface name */

    if ( ReqLen >= 2 )
    {
        ReqLen -= 2;
    }
    else
    {
        *pRes = CC_REQ_INV_LEN;
        return sizeof (INT8U);
    }
#if 0
    if(g_corefeatures.dual_image_support == ENABLED)
    {
        if(IsCardInActiveFlashMode())
        {
            IPMI_WARNING("Card in Active flash mode, Not safe to set any of LAN configurations...\n");
            *pRes = CC_UNSPECIFIED_ERR;
            return sizeof (*pRes);
        }
    }
#endif
    EthIndex= GetEthIndex(pSetLanReq->ChannelNum & 0x0F, BMCInst);
    if(0xff == EthIndex)
    {
        *pRes = CC_INV_DATA_FIELD;
        return sizeof (INT8U);
    }

    memset(IfcName,0,sizeof(IfcName));
    /*Get the EthIndex*/
    if(GetIfcName(EthIndex,IfcName, BMCInst) == -1)
    {
        TCRIT("Error in Getting Ifc name\n");
        *pRes = CC_INV_DATA_FIELD;
        return sizeof (INT8U);
    }

    for(i=0;i<sizeof(Ifcnametable)/sizeof(IfcName_T);i++)
    {
        if(strcmp(Ifcnametable[i].Ifcname,IfcName) == 0)
        {
            netindex= Ifcnametable[i].Index;
            break;
        }
    }

    if(netindex == 0xFF)
    {
        *pRes = CC_INV_DATA_FIELD;
        return sizeof (INT8U);
    }

    if((pSetLanReq->ParameterSelect >= MIN_LAN_OEM_CONF_PARAM) && 
            (pSetLanReq->ParameterSelect <= MAX_LAN_OEM_CONF_PARAM) )
    {
        /* Converts OEM parameter value to equivalent AMI parameter value */
        if (0 != GetLanAMIParamValue (&pSetLanReq->ParameterSelect, &IsOemDefined) )
        {
            pSetLanRes->CompletionCode = CC_PARAM_NOT_SUPPORTED;
            return sizeof(INT8U);
        }
#if 0
        /* Hook for OEM to handle this parameter */
        if ( (IsOemDefined)  && (g_PDKHandle[PDK_SETLANOEMPARAM] != NULL) )
        {
            return ((int(*)(INT8U *, INT8U, INT8U *,int))(g_PDKHandle[PDK_SETLANOEMPARAM]))(pReq, ReqLen, pRes, BMCInst);
        }
#endif   	
    }    

    if(0x1b >pSetLanReq->ParameterSelect  )   //Max known Lan paramter
    {
        if (ReqLen != LanconfigParameterLength [pSetLanReq->ParameterSelect ])
        {
            *pRes = CC_REQ_INV_LEN;
            return sizeof (INT8U);
        }
    }

    if(pSetLanReq->ChannelNum & RESERVED_VALUE_F0)
    {
        /* Alarm !!! Somebody is trying to set Reseved Bits */
        *pRes = CC_INV_DATA_FIELD;
        return sizeof (*pRes);
    }    

    /* Check for Reserved Bits */
    for (i = 0; i < sizeof (m_RsvdBitsCheck)/ sizeof (m_RsvdBitsCheck[0]); i++)
    {
        /* Check if this Parameter Selector needs Reserved bit checking !! */
        if (m_RsvdBitsCheck[i].Params == pSetLanReq->ParameterSelect)
        {
            //IPMI_DBG_PRINT_2 ("Param - %x, DataLen - %x\n", pSetLanReq->ParameterSelect, m_RsvdBitsCheck[i].DataLen);
            for (j = 0; j < m_RsvdBitsCheck[i].DataLen; j++)
            {
                //	IPMI_DBG_PRINT_2 ("Cmp  %x,  %x\n", pReq[2+j], m_RsvdBitsCheck[i].ReservedBits[j]);
                if ( 0 != (pReq[2+j] & m_RsvdBitsCheck[i].ReservedBits[j]))
                {
                    /* Alarm !!! Somebody is trying to set Reseved Bits */
                    *pRes = CC_INV_DATA_FIELD;
                    return sizeof (*pRes);
                }
            }
        }
    }

    ptrChannelInfo = getChannelInfo (pSetLanReq->ChannelNum & 0x0F, BMCInst);
    if(NULL == ptrChannelInfo)
    {
        *pRes = CC_INV_DATA_FIELD;
        return	sizeof(*pRes);
    }

    IPMI_DBG_PRINT_1 ("Parameter = %X\n", pSetLanReq->ParameterSelect);
#if 0
    if (g_PDKHandle[PDK_BEFORESETLANPARM] != NULL )
    {
        retValue = ((int(*)(INT8U *, INT8U, INT8U *,int))(g_PDKHandle[PDK_BEFORESETLANPARM]))(pReq, ReqLen, pRes, BMCInst);
        if(retValue != 0)
        {
            return retValue;
        }
    }
#endif
    switch (pSetLanReq->ParameterSelect)
    {
        case LAN_PARAM_SET_IN_PROGRESS:
            LOCK_BMC_SHARED_MEM(BMCInst);
            m_Lan_SetInProgress = BMC_GET_SHARED_MEM(BMCInst)->m_Lan_SetInProgress;
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            /* Commit Write is optional and supported
             * only if rollback is supported */
            if ( (GetBits(pSetLanReq->ConfigData.SetInProgress, SET_IN_PROGRESS_MASK) !=
                        LAN_CONFIGURATION_SET_COMPLETE) &&
                    (GetBits(pSetLanReq->ConfigData.SetInProgress, SET_IN_PROGRESS_MASK) !=
                     LAN_CONFIGURATION_SET_IN_PROGRESS) )
            {
                pSetLanRes->CompletionCode = CC_PARAM_NOT_SUPPORTED;
                return sizeof(*pSetLanRes);
            }
            else if ((GetBits(m_Lan_SetInProgress, SET_IN_PROGRESS_MASK) ==
                        LAN_CONFIGURATION_SET_IN_PROGRESS) &&
                    (GetBits(pSetLanReq->ConfigData.SetInProgress, SET_IN_PROGRESS_MASK) ==
                     LAN_CONFIGURATION_SET_IN_PROGRESS))
            {
                pSetLanRes->CompletionCode = CC_SET_IN_PROGRESS;
                return sizeof(*pSetLanRes);
            }

            LOCK_BMC_SHARED_MEM(BMCInst);
            BMC_GET_SHARED_MEM(BMCInst)->m_Lan_SetInProgress = pSetLanReq->ConfigData.SetInProgress;
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            break;

        case LAN_PARAM_AUTH_TYPE_SUPPORT:
            pSetLanRes->CompletionCode = CC_ATTEMPT_TO_SET_RO_PARAM;
            return sizeof(*pRes);

        case LAN_PARAM_AUTH_TYPE_ENABLES:
            for(i=0;i<5;i++)
            {
                /* Check for Unsupported AuthType */
                if (pBMCInfo->LANCfs[EthIndex].AuthTypeSupport != (pBMCInfo->LANCfs[EthIndex].AuthTypeSupport  |pReq[2+i]))
                {
                    IPMI_DBG_PRINT_2("\n Alarm !!! Somebody is trying to Unsupported Bit :%d \t%d\n",pReq[2+i],i);
                    *pRes = CC_INV_DATA_FIELD;
                    return sizeof (*pRes);
                }
            }

            LOCK_BMC_SHARED_MEM(BMCInst);
            _fmemcpy (&pBMCInfo->LANCfs[EthIndex].AuthTypeEnables,
                    &(pSetLanReq->ConfigData.AuthTypeEnables), sizeof(AuthTypeEnables_T));
            _fmemcpy (ptrChannelInfo->AuthType,
                    &(pSetLanReq->ConfigData.AuthTypeEnables), sizeof(AuthTypeEnables_T));
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            break;

        case LAN_PARAM_IP_ADDRESS:
            pendStatus = GetPendStatus(PEND_OP_SET_IP);
            if(pendStatus == PEND_STATUS_PENDING)
            {
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }
            //we need to do a read in hte pend task and not here
            // because if pend task is still working on setting the source for example t- by then we would have got the
            // next command which is ip address and then we would read back DHCP since nwcfg hasnt done its work yet etc. and all hell will breakloose.
            //nwReadNWCfg  (&NWConfig);

            if(pBMCInfo->LANCfs[EthIndex].IPAddrSrc == DHCP_IP_SOURCE)
            {
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }

            if(IPAddrCheck(pSetLanReq->ConfigData.IPAddr,LAN_PARAM_IP_ADDRESS))
            {
                *pRes = CC_INV_DATA_FIELD;
                return sizeof (INT8U);
            }

            pendStatus = GetPendStatus(PEND_OP_SET_IP);
            if(pendStatus == PEND_STATUS_PENDING)
            {
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }

            _fmemcpy (NWConfig.IPAddr, pSetLanReq->ConfigData.IPAddr, IP_ADDR_LEN);
            SetPendStatus(PEND_OP_SET_IP,PEND_STATUS_PENDING);
            PostPendTask(PEND_OP_SET_IP,(INT8U*)&NWConfig,sizeof(NWConfig),(pSetLanReq->ChannelNum & 0x0F),BMCInst);
            _fmemcpy (pBMCInfo->LANCfs[EthIndex].IPAddr, pSetLanReq->ConfigData.IPAddr, IP_ADDR_LEN);
            //nwWriteNWCfg (&NWConfig);
            break;

        case LAN_PARAM_IP_ADDRESS_SOURCE:

            if ((pSetLanReq->ConfigData.IPAddrSrc > BMC_OTHER_SOURCE)
                    ||(pSetLanReq->ConfigData.IPAddrSrc == UNSPECIFIED_IP_SOURCE))
            {
                *pRes = CC_INV_DATA_FIELD;
                return sizeof (INT8U);
            }
            if ( pBMCInfo->LANCfs[EthIndex].IPAddrSrc == pSetLanReq->ConfigData.IPAddrSrc )
            {
                TCRIT("LAN or VLAN if current SRC is DHCP/Static and incoming SRC is DHCP/Static, do nothing\n");
                break;
            }
            pBMCInfo->LANCfs[EthIndex].IPAddrSrc = pSetLanReq->ConfigData.IPAddrSrc ;
            if ( (pSetLanReq->ConfigData.IPAddrSrc == STATIC_IP_SOURCE ) || (pSetLanReq->ConfigData.IPAddrSrc == DHCP_IP_SOURCE ) )
            {
                pendStatus = GetPendStatus(PEND_OP_SET_SOURCE);
                if(pendStatus == PEND_STATUS_PENDING)
                {
                    *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                    return sizeof (INT8U);
                }
                NWConfig.CfgMethod = pSetLanReq->ConfigData.IPAddrSrc;
                SetPendStatus(PEND_OP_SET_SOURCE,PEND_STATUS_PENDING);
                PostPendTask(PEND_OP_SET_SOURCE,(INT8U*) &NWConfig,sizeof(NWConfig),(pSetLanReq->ChannelNum & 0x0F), BMCInst );
            }
            else if(pSetLanReq->ConfigData.IPAddrSrc == BIOS_IP_SOURCE)
            {
#if 0
                /*Perform OEM action*/
                if(g_PDKHandle[PDK_BIOSIPSOURCE] != NULL)
                {
                    retValue = ((int(*)(INT8U))g_PDKHandle[PDK_BIOSIPSOURCE]) (pSetLanReq->ChannelNum & CHANNEL_ID_MASK);
                    if(retValue == 1)
                    {
                        *pRes = CC_INV_DATA_FIELD;
                        return sizeof (*pRes);
                    }
                }
                else
#endif
                {
                    *pRes = CC_INV_DATA_FIELD;
                    return sizeof (*pRes);
                }
            }
            else if(pSetLanReq->ConfigData.IPAddrSrc == BMC_OTHER_SOURCE)
            {
#if 0
                /*Perform OEM action*/
                if(g_PDKHandle[PDK_BMCOTHERSOURCEIP] != NULL)
                {
                    retValue = ((int(*)(INT8U))g_PDKHandle[PDK_BMCOTHERSOURCEIP]) (pSetLanReq->ChannelNum & CHANNEL_ID_MASK);
                    if(retValue == 1)
                    {
                        *pRes = CC_INV_DATA_FIELD;
                        return sizeof (*pRes);
                    }
                }
                else
#endif
                {	
                    *pRes = CC_INV_DATA_FIELD;
                    return sizeof (*pRes);
                }
            }  
            break;

        case LAN_PARAM_MAC_ADDRESS:
#if 0
            nwReadNWCfg  (&NWConfig);
            printf ( "The MAC is %x %x %x %x %x %x \n", NWConfig.MAC [0],NWConfig.MAC [1],NWConfig.MAC [2],NWConfig.MAC [3],NWConfig.MAC [4],NWConfig.MAC [5] );

            _fmemcpy (NWConfig.MAC, pSetLanReq->ConfigData.MACAddr, MAC_ADDR_LEN);
            printf ( "The MAC is %x %x %x %x %x %x \n", NWConfig.MAC [0],NWConfig.MAC [1],NWConfig.MAC [2],NWConfig.MAC [3],NWConfig.MAC [4],NWConfig.MAC [5] );
            nwWriteNWCfg (&NWConfig);
#else
            /* According to IPMI 2.0 Specification Revision 3, the MAC address can be read only parameter*/
            //*pRes = CC_ATTEMPT_TO_SET_RO_PARAM;
            //return sizeof (*pRes);

            if (!enableSetMACAddr)
                pSetLanRes->CompletionCode = CC_ATTEMPT_TO_SET_RO_PARAM;
            else
            {
                EnableSetMACAddress_T macAddrEnabled;
                INT8U InvalidMac[MAC_ADDR_LEN] = {0};
                if(memcmp(&InvalidMac,&pSetLanReq->ConfigData.MACAddr,MAC_ADDR_LEN) == 0)
                {
                    pSetLanRes->CompletionCode = CC_INV_DATA_FIELD;
                    return sizeof(INT8U);
                }

                memset(&macAddrEnabled, 0, sizeof(EnableSetMACAddress_T));
                macAddrEnabled.EthIndex = macEthIndex;
                memcpy(&macAddrEnabled.MACAddress, &pSetLanReq->ConfigData.MACAddr, MAC_ADDR_LEN);

                pendStatus = GetPendStatus(PEND_OP_SET_MAC_ADDRESS);
                if(pendStatus == PEND_STATUS_PENDING)
                {
                    *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                    return sizeof (INT8U);
                }
                SetPendStatus(PEND_OP_SET_MAC_ADDRESS, PEND_STATUS_PENDING);
                PostPendTask(PEND_OP_SET_MAC_ADDRESS, (INT8U*)&macAddrEnabled, sizeof(EnableSetMACAddress_T), (pSetLanReq->ChannelNum & 0x0F), BMCInst);

                enableSetMACAddr = FALSE;
                macEthIndex = 0xFF;

                pSetLanRes->CompletionCode = CC_NORMAL;
            }

            return sizeof(*pSetLanRes);

#endif

        case LAN_PARAM_SUBNET_MASK:
            pendStatus = GetPendStatus(PEND_OP_SET_SUBNET);
            if(pendStatus == PEND_STATUS_PENDING)
            {
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }
            /*Returning valid completion code in case of attempt to set netmask in DHCP mode */
            if(pBMCInfo->LANCfs[EthIndex].IPAddrSrc == DHCP_IP_SOURCE)
            {
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }

            if(IPAddrCheck(pSetLanReq->ConfigData.SubNetMask,LAN_PARAM_SUBNET_MASK))
            {
                *pRes = CC_INV_DATA_FIELD;
                return sizeof (INT8U);
            }

            _fmemcpy (NWConfig.Mask, pSetLanReq->ConfigData.SubNetMask, IP_ADDR_LEN);
            SetPendStatus(PEND_OP_SET_SUBNET,PEND_STATUS_PENDING);
            PostPendTask(PEND_OP_SET_SUBNET,(INT8U*)&NWConfig,sizeof(NWConfig),(pSetLanReq->ChannelNum & 0x0F) , BMCInst);
            _fmemcpy (pBMCInfo->LANCfs[EthIndex].SubNetMask, pSetLanReq->ConfigData.SubNetMask, IP_ADDR_LEN);

            break;

        case LAN_PARAM_IPv4_HEADER:
            pendStatus = GetPendStatus(PEND_OP_SET_IPV4_HEADERS);
            if(pendStatus == PEND_STATUS_PENDING)
            {
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }
            if(!pSetLanReq->ConfigData.Ipv4HdrParam.TimeToLive > 0)
            {
                IPMI_DBG_PRINT("The requested IPv4 header(TTL) to set is invalid.\n");
                *pRes = CC_PARAM_OUT_OF_RANGE;
                return sizeof(*pRes);
            }
            if(pSetLanReq->ConfigData.Ipv4HdrParam.IpHeaderFlags == 0x60) // Flags can be either of the values: DF(0x40) or MF(0x20)
            {
                IPMI_DBG_PRINT("The requested IPv4 header(Flags) to set is invalid.\n");
                *pRes = CC_PARAM_OUT_OF_RANGE;
                return sizeof(*pRes);
            }
            LOCK_BMC_SHARED_MEM(BMCInst);
            _fmemcpy (&pBMCInfo->LANCfs[EthIndex].Ipv4HdrParam,
                    &pSetLanReq->ConfigData.Ipv4HdrParam, sizeof(IPv4HdrParams_T));
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            SetPendStatus(PEND_OP_SET_IPV4_HEADERS,PEND_STATUS_PENDING);
            PostPendTask(PEND_OP_SET_IPV4_HEADERS, (INT8U*)&(pSetLanReq->ConfigData.Ipv4HdrParam),
                    sizeof(pSetLanReq->ConfigData.Ipv4HdrParam),(pSetLanReq->ChannelNum & 0x0F),BMCInst);
            break;

        case LAN_PARAM_PRI_RMCP_PORT:
            pendStatus = GetPendStatus(PEND_RMCP_PORT_CHANGE);
            if(pendStatus == PEND_STATUS_PENDING)
            {
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }
            LOCK_BMC_SHARED_MEM(BMCInst);
            pBMCInfo->LANCfs[EthIndex].PrimaryRMCPPort = ipmitoh_u16 (pSetLanReq->ConfigData.PrimaryRMCPPort);
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            SetPendStatus(PEND_RMCP_PORT_CHANGE,PEND_STATUS_PENDING);
            PostPendTask(PEND_RMCP_PORT_CHANGE,(INT8U*)&(pSetLanReq->ConfigData.PrimaryRMCPPort),
                    sizeof(pSetLanReq->ConfigData.PrimaryRMCPPort),(pSetLanReq->ChannelNum & 0x0F),BMCInst);
            break;

        case LAN_PARAM_SEC_RMCP_PORT:
            /* Returning Invalid error message */
            *pRes = CC_PARAM_NOT_SUPPORTED;
            return sizeof (INT8U);
            /*pPMConfig->LANConfig[EthIndex].SecondaryPort = ipmitoh_u16 (pSetLanReq->ConfigData.SecondaryPort);*/
            break;

        case LAN_PARAM_BMC_GENERATED_ARP_CONTROL:

            currBmcGenArpCtrl = pBMCInfo->LANCfs[EthIndex].BMCGeneratedARPControl;

            if(currBmcGenArpCtrl != pSetLanReq->ConfigData.BMCGeneratedARPControl)
                pBMCInfo->LANCfs[EthIndex].BMCGeneratedARPControl = pSetLanReq->ConfigData.BMCGeneratedARPControl;

            if((ENABLE_ARP_RESPONSES & currBmcGenArpCtrl) !=
                    (ENABLE_ARP_RESPONSES & pSetLanReq->ConfigData.BMCGeneratedARPControl))
            {
                UpdateArpStatus(EthIndex, BMC_GET_SHARED_MEM(BMCInst)->IsWDTRunning, BMCInst);
            }

            if(!(GRATIUTOUS_ENABLE_MASK & currBmcGenArpCtrl) &&
                    (GRATIUTOUS_ENABLE_MASK & pSetLanReq->ConfigData.BMCGeneratedARPControl))
            {
                /* Create a thread to Send Gratuitous ARP Packet */
                pLANArg = malloc(sizeof(BMCArg));
                pLANArg->BMCInst = BMCInst; 
                pLANArg->Len = strlen((char *)&EthIndex);
                pLANArg->Argument = malloc(pLANArg->Len);
                memcpy(pLANArg->Argument,(char *)&EthIndex,pLANArg->Len);

                OS_CREATE_THREAD ((void *)GratuitousARPTask,(void *)pLANArg, NULL);
            }

            break;

        case LAN_PARAM_GRATITIOUS_ARP_INTERVAL:

            pBMCInfo->LANCfs[EthIndex].GratitousARPInterval =
                pSetLanReq->ConfigData.GratitousARPInterval;
            break;

        case LAN_PARAM_DEFAULT_GATEWAY_IP:
            pendStatus = GetPendStatus(PEND_OP_SET_GATEWAY);
            if(pendStatus == PEND_STATUS_PENDING)
            {
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }
            nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, netindex,g_corefeatures.global_ipv6);
            if(IPAddrCheck(pSetLanReq->ConfigData.DefaultGatewayIPAddr,LAN_PARAM_DEFAULT_GATEWAY_IP))
            {
                *pRes = CC_INV_DATA_FIELD;
                return sizeof (INT8U);
            }

            _fmemcpy ((INT8U*)&GWIp,pSetLanReq->ConfigData.DefaultGatewayIPAddr, IP_ADDR_LEN);
            _fmemcpy ((INT8U*)&Subnetmask,&NWConfig.Mask[0],IP_ADDR_LEN);
            _fmemcpy ((INT8U*)&IPAddr,&NWConfig.IPAddr[0], IP_ADDR_LEN);
            /* Allowing  When the Default Gateway is Zero without validation to clear the Default Gateway */
            if(GWIp != 0)
            {
                _fmemcpy ((INT8U*)&Subnetmask,pBMCInfo->LANCfs[EthIndex].SubNetMask,IP_ADDR_LEN);
                _fmemcpy ((INT8U*)&IPAddr,pBMCInfo->LANCfs[EthIndex].IPAddr, IP_ADDR_LEN);
                if((IPAddr & Subnetmask ) != (GWIp & Subnetmask))
                {
                    IPMI_DBG_PRINT("\n Default GatewayIP to set is not valid \n");
                    *pRes = CC_INV_DATA_FIELD;
                    return sizeof (INT8U);
                }
            }
            _fmemcpy (NWConfig.Gateway,pSetLanReq->ConfigData.DefaultGatewayIPAddr, IP_ADDR_LEN);
            LOCK_BMC_SHARED_MEM(BMCInst);
            _fmemcpy(pBMCInfo->LANCfs[EthIndex].DefaultGatewayIPAddr,
                    pSetLanReq->ConfigData.DefaultGatewayIPAddr,IP_ADDR_LEN);
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            SetPendStatus(PEND_OP_SET_GATEWAY,PEND_STATUS_PENDING);
            PostPendTask(PEND_OP_SET_GATEWAY,(INT8U*)&NWConfig,sizeof(NWConfig),(pSetLanReq->ChannelNum & 0x0F), BMCInst );
            break;

        case LAN_PARAM_DEFAULT_GATEWAY_MAC:

            LOCK_BMC_SHARED_MEM(BMCInst);
            _fmemcpy (pBMCInfo->LANCfs[EthIndex].DefaultGatewayMACAddr,
                    pSetLanReq->ConfigData.DefaultGatewayMACAddr, MAC_ADDR_LEN);
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            break;

        case LAN_PARAM_BACKUP_GATEWAY_IP:

            nwReadNWCfg_v4_v6( &NWConfig,&NWConfig6, netindex,g_corefeatures.global_ipv6);
            _fmemcpy ((INT8U*)&GWIp,pSetLanReq->ConfigData.BackupGatewayIPAddr, IP_ADDR_LEN);
            _fmemcpy ((INT8U*)&Subnetmask,&NWConfig.Mask[0],IP_ADDR_LEN);
            _fmemcpy ((INT8U*)&IPAddr,&NWConfig.IPAddr[0], IP_ADDR_LEN);
            if(GWIp != 0)
            {
                if((IPAddr & Subnetmask ) != (GWIp & Subnetmask))
                {
                    IPMI_DBG_PRINT("\n Backup GatewayIP to set is not valid \n");
                    *pRes = CC_INV_DATA_FIELD;
                    return sizeof (INT8U);
                }
            }
            LOCK_BMC_SHARED_MEM(BMCInst);
            _fmemcpy (pBMCInfo->LANCfs[EthIndex].BackupGatewayIPAddr,
                    pSetLanReq->ConfigData.BackupGatewayIPAddr, IP_ADDR_LEN);
            nwSetBkupGWyAddr(pSetLanReq->ConfigData.BackupGatewayIPAddr,netindex);
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            break;

        case LAN_PARAM_BACKUP_GATEWAY_MAC:

            LOCK_BMC_SHARED_MEM(BMCInst);
            _fmemcpy (pBMCInfo->LANCfs[EthIndex].BackupGatewayMACAddr,
                    pSetLanReq->ConfigData.BackupGatewayMACAddr, MAC_ADDR_LEN);
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            break;

        case LAN_PARAM_COMMUNITY_STRING:
#if 0
            if (g_PDKHandle[PDK_SETSNMPCOMMUNITYNAME] != NULL )
            {
                if(((int(*)(INT8U *, INT8U,int))(g_PDKHandle[PDK_SETSNMPCOMMUNITYNAME]))(pSetLanReq->ConfigData.CommunityStr,MAX_COMM_STRING_SIZE, BMCInst)==0)
                    break;
            }
#endif
            LOCK_BMC_SHARED_MEM(BMCInst);
            _fmemcpy (pBMCInfo->LANCfs[EthIndex].CommunityStr,
                    pSetLanReq->ConfigData.CommunityStr, MAX_COMM_STRING_SIZE);
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            break;

        case LAN_PARAM_DEST_NUM:

            pSetLanRes->CompletionCode = CC_ATTEMPT_TO_SET_RO_PARAM;
            return sizeof(INT8U);

        case LAN_PARAM_SELECT_DEST_TYPE:

            // if (pSetLanReq->ConfigData.DestType.SetSelect > NUM_LAN_DESTINATION)
            if (pSetLanReq->ConfigData.DestType.SetSelect > pBMCInfo->LANCfs[EthIndex].NumDest )
            {
                *pRes = CC_PARAM_OUT_OF_RANGE;
                return sizeof (*pRes);
            }

            if (0 == pSetLanReq->ConfigData.DestType.SetSelect)
            {
                LOCK_BMC_SHARED_MEM(BMCInst);
                _fmemcpy (&pSharedMem->VolLANDestType[EthIndex],
                        &pSetLanReq->ConfigData.DestType, sizeof(LANDestType_T));
                UNLOCK_BMC_SHARED_MEM(BMCInst);
            }
            else
            {
                LOCK_BMC_SHARED_MEM(BMCInst);
                _fmemcpy (&pBMCInfo->LANCfs[EthIndex].DestType [pSetLanReq->ConfigData.DestType.SetSelect - 1],
                        &pSetLanReq->ConfigData.DestType, sizeof(LANDestType_T));
                UNLOCK_BMC_SHARED_MEM(BMCInst);
            }
            break;

        case LAN_PARAM_SELECT_DEST_ADDR:
            pendStatus = GetPendStatus(PEND_OP_SET_GATEWAY);
            if(pendStatus == PEND_STATUS_PENDING)
            {
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }
            // if (pSetLanReq->ConfigData.DestAddr.SetSelect > NUM_LAN_DESTINATION)
            if (pSetLanReq->ConfigData.DestType.SetSelect > pBMCInfo->LANCfs[EthIndex].NumDest )
            {
                *pRes = CC_PARAM_OUT_OF_RANGE ;
                return sizeof (*pRes);
            }

            if (0 == pSetLanReq->ConfigData.DestAddr.SetSelect)
            {
                LOCK_BMC_SHARED_MEM(BMCInst);
                _fmemcpy (&pSharedMem->VolLANDest[EthIndex],
                        &pSetLanReq->ConfigData.DestAddr, sizeof(LANDestAddr_T));
                memset(pSharedMem->VolLANv6Dest,0,sizeof(LANDestv6Addr_T));
                UNLOCK_BMC_SHARED_MEM(BMCInst);
            }
            else
            {
                LOCK_BMC_SHARED_MEM(BMCInst);
                _fmemcpy (&pBMCInfo->LANCfs[EthIndex].DestAddr [pSetLanReq->ConfigData.DestAddr.SetSelect - 1],
                        &pSetLanReq->ConfigData.DestAddr, sizeof(LANDestAddr_T));
                memset( &pBMCInfo->LANCfs[EthIndex].Destv6Addr [pSetLanReq->ConfigData.Destv6Addr.SetSelect -1], 0 ,
                        sizeof(LANDestv6Addr_T));
                UNLOCK_BMC_SHARED_MEM(BMCInst);
            }
            /* Setting BackupGw to DefaultGw as per request to send trap */
            if(pSetLanReq->ConfigData.DestAddr.GateWayUse == 1)
            {
                IPMI_DBG_PRINT("Setting Backupgw to Defaultgwip as per Request \n");
                nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, netindex,g_corefeatures.global_ipv6);
                LOCK_BMC_SHARED_MEM(BMCInst);
                _fmemcpy(NWConfig.Gateway,pBMCInfo->LANCfs[EthIndex].BackupGatewayIPAddr,IP_ADDR_LEN);
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                SetPendStatus(PEND_OP_SET_GATEWAY,PEND_STATUS_PENDING);
                PostPendTask(PEND_OP_SET_GATEWAY,(INT8U*)&NWConfig,sizeof(NWConfig),(pSetLanReq->ChannelNum & 0x0F),BMCInst);

            }
            break;

        case LAN_PARAM_VLAN_ID:

            if( pBMCInfo->IpmiConfig.VLANIfcSupport == 1)
            {
                nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, netindex,g_corefeatures.global_ipv6);

                if((pSetLanReq->ConfigData.VLANID & VLAN_MASK_BIT) == VLAN_MASK_BIT)    /* checks for VLAN enable bit*/
                {
                    pendStatus = GetPendStatus(PEND_OP_SET_VLAN_ID);
                    if(pendStatus == PEND_STATUS_PENDING)
                    {
                        *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                        return sizeof (INT8U);
                    }
                    vlanID = (pSetLanReq->ConfigData.VLANID & 0xfff);    /* get the vlan d from the data */

                    for(i=0;i<sizeof(InvalidVlanID)/sizeof(INT16U);i++)
                    {
                        if(InvalidVlanID[i] != vlanID)
                        {
                            continue;
                        }
                        else        /*invalid vlan id */
                        {
                            *pRes = CC_INV_DATA_FIELD ;
                            return sizeof (*pRes);
                        }
                    }

                    if ( NWConfig.VLANID != 0)     /* checks if vlan id already present */
                    {
                        TCRIT(" VLAN ID is already Enabled. So De-configure previous VLAN ID then Configure New VLAN ID.\n");
                        *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE ;
                        return sizeof (*pRes);
                    }

                    NWConfig.VLANID=vlanID;
                    SetPendStatus(PEND_OP_SET_VLAN_ID,PEND_STATUS_PENDING);
                    PostPendTask(PEND_OP_SET_VLAN_ID,(INT8U*)&NWConfig,sizeof(NWConfig),(pSetLanReq->ChannelNum & 0x0F), BMCInst );
                    pBMCInfo->LANCfs[EthIndex].VLANID = pSetLanReq->ConfigData.VLANID;
                }
                else        /* Vlan Bit is Disabled */
                {
                    if(NWConfig.VLANID==0)        /* Vlan id is disabled */
                    {
                        if((pSetLanReq->ConfigData.VLANID & 0xfff)!=0)
                        {
                            pBMCInfo->LANCfs[EthIndex].VLANID = pSetLanReq->ConfigData.VLANID;
                        }

                        if((pSetLanReq->ConfigData.VLANID & 0xfff)==0)
                        {
                            if((pBMCInfo->LANCfs[EthIndex].VLANID & 0xfff)!=0)
                            {
                                pBMCInfo->LANCfs[EthIndex].VLANID = pSetLanReq->ConfigData.VLANID;
                            }
                        }
                    }

                    else                /* Vlan ID is enable. so deconfigure it */
                    {
                        memset(IfcName,0,sizeof(IfcName));
                        if(GetIfcName(EthIndex, IfcName,BMCInst) != 0)
                        {
                            TCRIT("Error in getting Interface Name for the Lan Index :%d\n",EthIndex);
                            *pRes = CC_INV_DATA_FIELD ;
                            return sizeof (*pRes);
                        }
                        sprintf(VLANInterfaceName, "%s.%d", IfcName, (int)(NWConfig.VLANID));
                        if (0 == NwInterfacePresenceCheck (VLANInterfaceName))
                        {
                            pendStatus = GetPendStatus(PEND_OP_SET_VLAN_ID);
                            if(pendStatus == PEND_STATUS_PENDING)
                            {
                                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                                return sizeof (INT8U);
                            }
                            SetPendStatus(PEND_OP_DECONFIG_VLAN_IFC,PEND_STATUS_PENDING);
                            PostPendTask(PEND_OP_DECONFIG_VLAN_IFC,(INT8U*)&NWConfig,sizeof(NWConfig),(pSetLanReq->ChannelNum & 0x0F), BMCInst );
                            pBMCInfo->LANCfs[EthIndex].VLANPriority =0;
                        }
                        pendStatus = GetPendStatus(PEND_OP_SET_SOURCE);
                        if(pendStatus == PEND_STATUS_PENDING)
                        {
                            *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                            return sizeof (INT8U);
                        }
                        //NWConfig.VLANID=0;
                        NWConfig.CfgMethod = pBMCInfo->LANCfs[EthIndex].IPAddrSrc;
                        SetPendStatus(PEND_OP_SET_SOURCE,PEND_STATUS_PENDING);
                        PostPendTask(PEND_OP_SET_SOURCE,(INT8U*) &NWConfig,sizeof(NWConfig),(pSetLanReq->ChannelNum & 0x0F), BMCInst );
                        pBMCInfo->LANCfs[EthIndex].VLANID = pSetLanReq->ConfigData.VLANID;
                    }

                }
            }
            else
            {
                pSetLanRes->CompletionCode = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof(INT8U);
            }
            break;

        case LAN_PARAM_VLAN_PRIORITY:

            if( pBMCInfo->IpmiConfig.VLANIfcSupport == 1)
            {
                if((pBMCInfo->LANCfs[EthIndex].VLANID & VLAN_MASK_BIT) != VLAN_MASK_BIT)    /* checks for VLAN enable bit*/
                {
                    if(g_corefeatures.vlan_priorityset == ENABLED)
                    {
                        if(pSetLanReq->ConfigData.VLANPriority > 7 )
                        {
                            TCRIT(" VLAN Priority value should be 0-7 \n");
                            *pRes = CC_INV_DATA_FIELD ;
                            return sizeof (*pRes);
                        }
                        if(ReadVLANFile(VLAN_PRIORITY_SETTING_STR, PriorityLevel) == -1)
                        {
                            return -1;
                        }
                        if(WriteVLANFile(VLAN_PRIORITY_SETTING_STR, PriorityLevel, netindex,pSetLanReq->ConfigData.VLANPriority) == -1)
                        {
                            return -1;
                        }
                        pBMCInfo->LANCfs[EthIndex].VLANPriority = pSetLanReq->ConfigData.VLANPriority;
                    }
                    else
                    {
                        TCRIT(" VLAN is not Configured \n");
                        *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                        return sizeof (*pRes);
                    }
                }
                else
                {
                    if(pSetLanReq->ConfigData.VLANPriority > 7 )
                    {
                        TCRIT(" VLAN Priority value should be 0-7 \n");
                        *pRes = CC_INV_DATA_FIELD ;
                        return sizeof (*pRes);
                    }

                    if(ReadVLANFile(VLAN_PRIORITY_SETTING_STR, PriorityLevel) == -1)
                    {
                        return -1;
                    }

                    if(WriteVLANFile(VLAN_PRIORITY_SETTING_STR, PriorityLevel, netindex,pSetLanReq->ConfigData.VLANPriority) == -1)
                    {
                        return -1;
                    }

                    pBMCInfo->LANCfs[EthIndex].VLANPriority = pSetLanReq->ConfigData.VLANPriority;
                    vlanID = (pBMCInfo->LANCfs[EthIndex].VLANID & 0xfff);
                    memset(IfcName,0,sizeof(IfcName));
                    if(GetIfcName(EthIndex, IfcName,BMCInst) != 0)
                    {
                        TCRIT("Error in getting Interface Name for the Lan Index :%d\n",EthIndex);
                        *pRes = CC_INV_DATA_FIELD;
                        return sizeof(*pRes);
                    }
                    /*vconfig set_egress_map <valninterface> <skb_buffer> <vlan-priority>*/
                    sprintf(cmdSetPriority,"vconfig set_egress_map  %s.%d  0  %d",IfcName,vlanID,pSetLanReq->ConfigData.VLANPriority);
                    //                    if(((retValue = safe_system(cmdSetPriority)) < 0))
                    //                    {
                    //                        TCRIT("ERROR %d: Set VLAN Priority failed\n",retValue);
                    //                    }

                    /* 
                     * Set priority of IPMI commands. 
                     * The skb->priority value of IPMI command will be modified by TOS option.
                     * So, use the mapping table to get the current value.
                     */
                    memset(&cmdSetPriority,0,sizeof(cmdSetPriority));
                    TOS = pBMCInfo->LANCfs[EthIndex].Ipv4HdrParam.TypeOfService;
                    SkbPriority = IP_TOS2PRIO[IPTOS_TOS(TOS)>>1];
                    sprintf(cmdSetPriority,"vconfig set_egress_map %s.%d %d %d",IfcName,vlanID,SkbPriority,pSetLanReq->ConfigData.VLANPriority);
                    //                    if(((retValue = safe_system(cmdSetPriority)) < 0))
                    //                    {
                    //                        TCRIT("ERROR %d: Set VLAN IPMI Priority failed\n",retValue);
                    //                    } 
                }
            }
            else
            {
                pSetLanRes->CompletionCode = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof(INT8U);
            }
            break;


        case LAN_PARAM_CIPHER_SUITE_ENTRY_SUP:
            pSetLanRes->CompletionCode = CC_ATTEMPT_TO_SET_RO_PARAM;
            return sizeof(INT8U);

        case LAN_PARAM_CIPHER_SUITE_ENTRIES:
            pSetLanRes->CompletionCode = CC_ATTEMPT_TO_SET_RO_PARAM;
            return sizeof(INT8U);
            break;

        case LAN_PARAM_CIPHER_SUITE_PRIV_LEVELS:
            LOCK_BMC_SHARED_MEM(BMCInst);
            _fmemcpy (pBMCInfo->LANCfs[EthIndex].CipherSuitePrivLevels,
                    pSetLanReq->ConfigData.CipherSuitePrivLevels, MAX_NUM_CIPHER_SUITE_PRIV_LEVELS);
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            break;

        case LAN_PARAM_VLAN_TAGS:
            if (0 == pSetLanReq->ConfigData.DestAddr.SetSelect)
            {
                LOCK_BMC_SHARED_MEM(BMCInst);
                _fmemcpy(&pSharedMem->VLANDestTag,
                        ((_NEAR_ INT8U*)&pSetLanReq->ConfigData) + 1, sizeof(VLANDestTags_T));
                UNLOCK_BMC_SHARED_MEM(BMCInst);
            }
            else
            {
                if (pSetLanReq->ConfigData.DestAddr.SetSelect > pBMCInfo->LANCfs[EthIndex].NumDest)
                {
                    pSetLanRes->CompletionCode = CC_PARAM_OUT_OF_RANGE;
                    return sizeof (INT8U);
                }
                LOCK_BMC_SHARED_MEM(BMCInst);
                _fmemcpy(&pBMCInfo->LANCfs[EthIndex].VLANDestTags [pSetLanReq->ConfigData.DestAddr.SetSelect - 1],
                        ((_NEAR_ INT8U*)&pSetLanReq->ConfigData) + 1, sizeof(VLANDestTags_T));
                UNLOCK_BMC_SHARED_MEM(BMCInst);
            }
            break;

        case LAN_PARAMS_BAD_PASSWORD_THRESHOLD:
            //            ClearUserLockAttempts(LAN_CHANNEL_BADP,BMCInst);
            LOCK_BMC_SHARED_MEM(BMCInst);
            _fmemcpy(&pBMCInfo->LANCfs[EthIndex].BadPasswd,
                    &pSetLanReq->ConfigData.BadPasswd,sizeof(BadPassword_T));
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            break;

        case LAN_PARAMS_AMI_OEM_SNMPV6_DEST_ADDR:

            if (pSetLanReq->ConfigData.DestType.SetSelect > pBMCInfo->LANCfs[EthIndex].NumDest )
            {
                TCRIT("Invalid data for SetSelect");
                *pRes = CC_INV_DATA_FIELD ;
                return sizeof (*pRes);
            }

            if (0 == pSetLanReq->ConfigData.Destv6Addr.SetSelect)
            {
                LOCK_BMC_SHARED_MEM(BMCInst);
                _fmemcpy (&pSharedMem->VolLANv6Dest,
                        &pSetLanReq->ConfigData.Destv6Addr, sizeof(LANDestv6Addr_T));

                memset(pSharedMem->VolLANDest,0,sizeof(LANDestAddr_T));

                UNLOCK_BMC_SHARED_MEM(BMCInst);
            }
            else
            {
                LOCK_BMC_SHARED_MEM(BMCInst);
                _fmemcpy (&pBMCInfo->LANCfs[EthIndex].Destv6Addr[pSetLanReq->ConfigData.Destv6Addr.SetSelect - 1],
                        &pSetLanReq->ConfigData.Destv6Addr, sizeof(LANDestv6Addr_T));

                memset(&pBMCInfo->LANCfs[EthIndex].DestAddr[pSetLanReq->ConfigData.DestAddr.SetSelect - 1],0,sizeof(LANDestAddr_T));

                UNLOCK_BMC_SHARED_MEM(BMCInst);
            }

            TDBG("\n SetLanconfig: Setting SNMPv6 configuration done..\n");

            break;

        case LAN_PARAMS_AMI_OEM_ENABLE_SET_MAC:
            NIC_Count = g_coremacros.global_nic_count;
            if (ReqLen != 1)
                pSetLanRes->CompletionCode = CC_REQ_INV_LEN;
            else if (pSetLanReq->ConfigData.EthIndex > ( NIC_Count- 1))
                pSetLanRes->CompletionCode = CC_INV_DATA_FIELD;
            else
            {
                enableSetMACAddr = TRUE;
                macEthIndex = pSetLanReq->ConfigData.EthIndex;
                pSetLanRes->CompletionCode = CC_NORMAL;
            }

            return sizeof(*pSetLanRes);


        case LAN_PARAMS_AMI_OEM_IPV6_ENABLE:
            pendStatus = GetPendStatus(PEND_OP_SET_IPV6_ENABLE);
            if(pendStatus == PEND_STATUS_PENDING)
            {
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }
            TDBG("\n Entered in LAN_PARAMS_AMI_OEM_IPV6_ENABLE \n");         
            if ( pBMCInfo->LANCfs[EthIndex].IPv6_Enable == pSetLanReq->ConfigData.IPv6_Enable)
            {
                TCRIT("LAN or VLAN - the current state is the same \n");
                break;
            }
            pBMCInfo->LANCfs[EthIndex].IPv6_Enable= pSetLanReq->ConfigData.IPv6_Enable;

            NWConfig6.enable= pSetLanReq->ConfigData.IPv6_Enable;           
            SetPendStatus(PEND_OP_SET_IPV6_ENABLE,PEND_STATUS_PENDING);
            PostPendTask(PEND_OP_SET_IPV6_ENABLE,(INT8U*) &NWConfig6,sizeof(NWConfig6),(pSetLanReq->ChannelNum & 0x0F), BMCInst );
            break;


        case LAN_PARAMS_AMI_OEM_IPV6_IP_ADDR_SOURCE:
            pendStatus = GetPendStatus(PEND_OP_SET_IPV6_IP_ADDR_SOURCE);
            if(pendStatus == PEND_STATUS_PENDING)
            {
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }
            TDBG("\n Entered in LAN_PARAMS_AMI_OEM_IPV6_IP_ADDR_SOURCE \n");
            /* check for IPv6 enable state */
            if ( pBMCInfo->LANCfs[EthIndex].IPv6_Enable != 1)
            {
                TCRIT("IPv6 is not enabled yet... \n");
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }

            if ((pSetLanReq->ConfigData.IPv6_IPAddrSrc > BMC_OTHER_SOURCE)
                    ||(pSetLanReq->ConfigData.IPv6_IPAddrSrc == UNSPECIFIED_IP_SOURCE))
            {
                *pRes = CC_INV_DATA_FIELD;
                return sizeof (INT8U);
            }
            else if((pSetLanReq->ConfigData.IPv6_IPAddrSrc == BIOS_IP_SOURCE)
                    || (pSetLanReq->ConfigData.IPv6_IPAddrSrc == BMC_OTHER_SOURCE))
            {
                /* we only support DHCP and Static IP source */
                *pRes = CC_INV_DATA_FIELD;
                return sizeof (INT8U);
            }

            if (pBMCInfo->LANCfs[EthIndex].IPv6_IPAddrSrc == pSetLanReq->ConfigData.IPv6_IPAddrSrc )
            {
                TCRIT("LAN or VLAN if current SRC is DHCP/Static and incoming SRC is DHCP/Static, do nothing\n");
                break;
            }

            pBMCInfo->LANCfs[EthIndex].IPv6_IPAddrSrc = pSetLanReq->ConfigData.IPv6_IPAddrSrc ;
            if ( (pSetLanReq->ConfigData.IPv6_IPAddrSrc == STATIC_IP_SOURCE ) || (pSetLanReq->ConfigData.IPv6_IPAddrSrc == DHCP_IP_SOURCE ) )
            {
                NWConfig6.CfgMethod = pSetLanReq->ConfigData.IPv6_IPAddrSrc;
                SetPendStatus(PEND_OP_SET_IPV6_IP_ADDR_SOURCE,PEND_STATUS_PENDING);
                PostPendTask(PEND_OP_SET_IPV6_IP_ADDR_SOURCE,(INT8U*) &NWConfig6,sizeof(NWConfig6),(pSetLanReq->ChannelNum & 0x0F), BMCInst );
            }
            break;            


        case LAN_PARAMS_AMI_OEM_IPV6_IP_ADDR:
            pendStatus = GetPendStatus(PEND_OP_SET_IPV6_IP_ADDR);
            if(pendStatus == PEND_STATUS_PENDING)
            {
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }
            TDBG("\n Entered in LAN_PARAMS_AMI_OEM_IPV6_IP_ADDR \n");
            /* check for IPv6 enable state */
            if ( pBMCInfo->LANCfs[EthIndex].IPv6_Enable != 1)
            {
                TCRIT("IPv6 is not enabled yet... \n");
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }

            /* Do IP address source check */
            if( pBMCInfo->LANCfs[EthIndex].IPv6_IPAddrSrc == DHCP_IP_SOURCE)
            {
                TCRIT("IPv6 Address source is currently in DHCP \n");
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }

            _fmemcpy(pBMCInfo->LANCfs[EthIndex].IPv6_IPAddr, pSetLanReq->ConfigData.IPv6Addr.IPv6_IPAddr, IP6_ADDR_LEN );

            SetPendStatus(PEND_OP_SET_IPV6_IP_ADDR,PEND_STATUS_PENDING);
            PostPendTask(PEND_OP_SET_IPV6_IP_ADDR, (INT8U*) &pSetLanReq->ConfigData.IPv6Addr,\
                    sizeof (IPv6Addr_T),(pSetLanReq->ChannelNum & 0x0F), BMCInst );
            break;


        case LAN_PARAMS_AMI_OEM_IPV6_PREFIX_LENGTH:
            pendStatus = GetPendStatus(PEND_OP_SET_IPV6_PREFIX_LENGTH);
            if(pendStatus == PEND_STATUS_PENDING)
            {
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }
            TDBG("\n Entered in LAN_PARAMS_AMI_OEM_IPV6_PREFIX_LENGTH \n");
            /* check for IPv6 enable state */
            if ( pBMCInfo->LANCfs[EthIndex].IPv6_Enable != 1)
            {
                TCRIT("IPv6 is not enabled yet... \n");
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }

            /* Do IP address source check */
            if( pBMCInfo->LANCfs[EthIndex].IPv6_IPAddrSrc == DHCP_IP_SOURCE)
            {
                TCRIT("IPv6 Address source is currently in DHCP \n");
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }

            pBMCInfo->LANCfs[EthIndex].IPv6_PrefixLen = pSetLanReq->ConfigData.IPv6Prefix.IPv6_PrefixLen;

            SetPendStatus(PEND_OP_SET_IPV6_PREFIX_LENGTH,PEND_STATUS_PENDING);
            PostPendTask(PEND_OP_SET_IPV6_PREFIX_LENGTH,(INT8U*) &pSetLanReq->ConfigData.IPv6Prefix, sizeof(IPv6Prefix_T),(pSetLanReq->ChannelNum & 0x0F), BMCInst );
            break;


        case LAN_PARAMS_AMI_OEM_IPV6_GATEWAY_IP:
            pendStatus = GetPendStatus(PEND_OP_SET_IPV6_GATEWAY);
            if(pendStatus == PEND_STATUS_PENDING)
            {
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }
            TDBG("\n Entered in LAN_PARAMS_AMI_OEM_IPV6_GATEWAY_IP \n");
            /* check for IPv6 enable state */
            if ( pBMCInfo->LANCfs[EthIndex].IPv6_Enable != 1)
            {
                TCRIT("IPv6 is not enabled yet... \n");
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }

            /* Do IP address source check */
            if( pBMCInfo->LANCfs[EthIndex].IPv6_IPAddrSrc == DHCP_IP_SOURCE)
            {
                TCRIT("IPv6 Address source is currently in DHCP \n");
                *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (INT8U);
            }

            _fmemcpy( pBMCInfo->LANCfs[EthIndex].IPv6_GatewayIPAddr, pSetLanReq->ConfigData.IPv6_GatewayIPAddr, IP6_ADDR_LEN );

            _fmemcpy( NWConfig6.Gateway, pSetLanReq->ConfigData.IPv6_GatewayIPAddr, IP6_ADDR_LEN );
            SetPendStatus(PEND_OP_SET_IPV6_GATEWAY,PEND_STATUS_PENDING);
            PostPendTask(PEND_OP_SET_IPV6_GATEWAY,(INT8U*) &NWConfig6,sizeof(NWConfig6),(pSetLanReq->ChannelNum & 0x0F), BMCInst );
            break;

        case LAN_PARAMS_AMI_OEM_PHY_SETTINGS:
            if(g_corefeatures.phy_support == ENABLED)
            {
                if(ReqLen != 4)
                {
                    pSetLanRes->CompletionCode = CC_REQ_INV_LEN;
                    return sizeof(*pSetLanRes);
                }
                memset(IfcName,0,sizeof(IfcName));
                if(GetIfcNameByIndex(EthIndex, IfcName) != 0)
                {
                    TCRIT("Error in Getting Interface Name for the lan Index:%d\n",EthIndex);
                    pSetLanRes->CompletionCode = CC_INV_DATA_FIELD;
                    return sizeof(*pSetLanRes);
                }
                if(pSetLanReq->ConfigData.PHYConfig.AutoNegotiationEnable == TRUE)
                {
                    /* Send 0xFF for both Speed and Duplex in case of Auto-Negotiation to differentiate with Force link mode */
                    retValue = nwSetEthInformation(0xff, 0xff, IfcName); 
                }
                else
                {
                    if((pSetLanReq->ConfigData.PHYConfig.Speed == 10 || pSetLanReq->ConfigData.PHYConfig.Speed == 100 || pSetLanReq->ConfigData.PHYConfig.Speed == 1000) && (pSetLanReq->ConfigData.PHYConfig.Duplex == 0 || pSetLanReq->ConfigData.PHYConfig.Duplex == 1))
                    {
                        retValue = nwSetEthInformation(pSetLanReq->ConfigData.PHYConfig.Speed, pSetLanReq->ConfigData.PHYConfig.Duplex, IfcName);
                        if(retValue != 0)
                            *pRes = CC_TIMEOUT;
                    }
                    else
                        *pRes = CC_INV_DATA_FIELD;
                }

                /* Storing the requested network link modes to a configuration file - /conf/phycfg.conf */
                void *pHandle = NULL;
                int (*pSetPHYConfig) (char *, int, int, int);
#if 0
                pHandle = dlopen("libphyconf.so", RTLD_LAZY);
                if(pHandle != NULL)
                {
                    pSetPHYConfig = dlsym(pHandle, "setPHYConfig");
                    if(pSetPHYConfig == NULL)
                    {
                        //IPMI_ERROR("Error in getting symbol: %s\n", dlerror());
                        dlclose(pHandle);
                        return sizeof(*pSetLanRes);
                    }

                    if(pSetPHYConfig(IfcName, pSetLanReq->ConfigData.PHYConfig.AutoNegotiationEnable, pSetLanReq->ConfigData.PHYConfig.Speed, pSetLanReq->ConfigData.PHYConfig.Duplex))
                    {
                        IPMI_ERROR("Error storing the requested PHY configuration values in /conf/phycfg.conf.\n");
                    }
                    dlclose(pHandle);
                }
                else
#endif
                    IPMI_ERROR("Error storing the requested PHY configuration values in /conf/phycfg.conf.\n");

                return sizeof(*pSetLanRes);
            }
            else
            {
                pSetLanRes->CompletionCode = CC_PARAM_NOT_SUPPORTED;
                return sizeof(INT8U);
            }


        case LAN_PARAMS_SSI_OEM_2ND_PRI_ETH_MAC_ADDR:
            if(g_corefeatures.ssi_support == ENABLED)
            {
                pendStatus = GetPendStatus(PEND_OP_SET_MAC_ADDRESS);
                if(pendStatus == PEND_STATUS_PENDING)
                {
                    *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                    return sizeof (INT8U);
                }
                if (!enableSetMACAddr)
                {
                    pSetLanRes->CompletionCode = CC_ATTEMPT_TO_SET_RO_PARAM;
                    return sizeof(INT8U);
                }
                else
                {
                    EnableSetMACAddress_T macAddrEnabled;
                    memset(&macAddrEnabled, 0, sizeof(EnableSetMACAddress_T));
                    macAddrEnabled.EthIndex = 0x1; /* Specify the 2nd interface */
                    memcpy(&macAddrEnabled.MACAddress, pSetLanReq->ConfigData.SSI2ndPriEthMACAddr, MAC_ADDR_LEN);
                    SetPendStatus(PEND_OP_SET_MAC_ADDRESS, PEND_STATUS_PENDING);
                    PostPendTask(PEND_OP_SET_MAC_ADDRESS, (INT8U*)&macAddrEnabled, sizeof(EnableSetMACAddress_T), 0x1, BMCInst);
                }
                break;
            }
            else
            {
                pSetLanRes->CompletionCode = CC_PARAM_NOT_SUPPORTED;
                return sizeof(INT8U);
            }

        case LAN_PARAMS_SSI_OEM_LINK_CTRL:
            if(g_corefeatures.ssi_support == ENABLED)
            {
                pSetLanRes->CompletionCode = CC_ATTEMPT_TO_SET_RO_PARAM; /* Read Only */
            }
            else
            {
                pSetLanRes->CompletionCode = CC_PARAM_NOT_SUPPORTED;
            }
            return sizeof(INT8U);

        case LAN_PARAMS_SSI_OEM_CMM_IP_ADDR:
            if(g_corefeatures.ssi_support == ENABLED)
            {
                _fmemcpy(pBMCInfo->SSIConfig.CMMIPAddr, pSetLanReq->ConfigData.CMMIPAddr, IP_ADDR_LEN);
                //            FlushIPMI((INT8U*)&pBMCInfo->SSIConfig, (INT8U*)&pBMCInfo->SSIConfig, pBMCInfo->IPMIConfLoc.SSIConfigAddr,
                //                      sizeof(SSIConfig_T), BMCInst);
                break;
            }
            else
            {
                pSetLanRes->CompletionCode = CC_PARAM_NOT_SUPPORTED;
                return sizeof(INT8U);
            }

        default:
            if(g_corefeatures.ncsi_cmd_support == ENABLED)
            {
                switch (pSetLanReq->ParameterSelect)
                {
                    case LAN_PARAMS_AMI_OEM_NCSI_CONFIG_NUM:
                        pSetLanRes->CompletionCode = CC_ATTEMPT_TO_SET_RO_PARAM;
                        return sizeof(*pSetLanRes);

                    case LAN_PARAMS_AMI_OEM_NCSI_SETTINGS:
                        NIC_Count=g_coremacros.global_nic_count;
                        if (ReqLen != 3)
                            pSetLanRes->CompletionCode = CC_REQ_INV_LEN;
                        else if (pSetLanReq->ConfigData.NCSIPortConfig.Interface >= NIC_Count)
                            pSetLanRes->CompletionCode = CC_INV_DATA_FIELD;
                        else
                        {
                            pendStatus = GetPendStatus(PEND_OP_SET_NCSI_CHANNEL_ID);
                            if(pendStatus == PEND_STATUS_PENDING)
                            {
                                *pRes = CC_NODE_BUSY;
                                return sizeof (INT8U);
                            }

                            NCSIConfig_T configData;
                            char interfaceName[8];
                            memset(&configData, 0, sizeof(NCSIConfig_T));
                            memset(interfaceName, 0, sizeof(interfaceName));

                            snprintf(interfaceName, sizeof(interfaceName), "%s%d", "eth", pSetLanReq->ConfigData.NCSIPortConfig.Interface);

                            //                        if (NCSIGetPortConfigByName(interfaceName, &configData) != 0)
                            //                            pSetLanRes->CompletionCode = CC_INV_DATA_FIELD;
                            //                        else
                            {
                                configData.PackageId = pSetLanReq->ConfigData.NCSIPortConfig.PackageId;
                                configData.ChannelId = pSetLanReq->ConfigData.NCSIPortConfig.ChannelId;

                                //                            if (NCSISetPortConfigByName(interfaceName, configData) != 0)
                                //                                pSetLanRes->CompletionCode = CC_INV_DATA_FIELD;
                                //                            else
                                {
                                    SetPendStatus(PEND_OP_SET_NCSI_CHANNEL_ID, PEND_STATUS_PENDING);
                                    PostPendTask(PEND_OP_SET_NCSI_CHANNEL_ID, NULL, 0, 0, BMCInst);
                                    pSetLanRes->CompletionCode = CC_NORMAL;
                                }
                            }
                        }
                        return sizeof(*pSetLanRes);
                    default:
                        TDBG("In Valid Option\n");
                }
            }
            pSetLanRes->CompletionCode = CC_PARAM_NOT_SUPPORTED;
            return sizeof(INT8U);
    }

    pSetLanRes->CompletionCode = CC_NORMAL;
#if 0    
    if(g_PDKHandle[PDK_POSTSETLANPARAM] != NULL)
    {
        ((int(*)(INT8U*, INT8U, int))(g_PDKHandle[PDK_POSTSETLANPARAM]))(pReq, ReqLen, BMCInst);
    }
#endif    
    //    FlushIPMI((INT8U*)&pBMCInfo->LANCfs[0],(INT8U*)&pBMCInfo->LANCfs[EthIndex],pBMCInfo->IPMIConfLoc.LANCfsAddr,
    //                      sizeof(LANConfig_T),BMCInst);
    return sizeof(*pSetLanRes);
}

/*
 *@fn IPAddrCheck function
 *@brief It will validate the IP Address and net Mask
 *@param Addr - IP Address or net Mask
 *@param params - parameter data to validate
 */
int IPAddrCheck(INT8U *Addr,int params)
{
    int i,maskcount=0,bitmask =0,j,bitcheck=0;

    for(i=0;i< IP_ADDR_LEN;i++)
    {
        if(Addr[i] == BRD_CAST_BIT_MASK)
        {
            maskcount++;
        }
    }

    if(maskcount == IP_ADDR_LEN)
    {
        return 1;
    }

    if(params == LAN_PARAM_SUBNET_MASK)
    {
        if(Addr[0] == BRD_CAST_BIT_MASK)
        {
            for(i=1;i< IP_ADDR_LEN;i++)
            {
                if(Addr[i-1] == 0)
                {
                    bitmask = 1;
                }
                if(Addr[i] != 0)
                {
                    for(j=0;j<8;j++)
                    {
                        if((Addr[i]<<j) & SUBNET_MASK_BIT_CHECK)
                        {
                            if(bitcheck == 1)
                            {
                                return 1;
                            }
                            continue;
                        }
                        bitcheck=1;
                    }
                    if((bitcheck == 1 && Addr[i-1] != BRD_CAST_BIT_MASK) || (Addr[i] > 0 && bitmask == 1))
                    {
                        return 1;
                    }
                }                       
            }
            return 0;
        }
        return 1;
    }

    if(Addr[0] == LOOP_BACK_BIT_MASK || Addr[0] == BRD_CAST_BIT_MASK)
    {
        return 1;
    }

    return 0;
}

/*----------------------------------------------
 * GratuitousARPTask
 *----------------------------------------------*/
void* GratuitousARPTask (INT8U *Addr)
{
    INT8U               IntervalInSec;
    INT8U               Status;
    int                 nRet;
    NWCFG_STRUCT        NWConfig;
    NWCFG6_STRUCT       NWConfig6;
    INT8U               EthIndex,netindex= 0xFF;
    int BMCInst,i;
    char IfcName[16];
    BMCArg *GratArpArgs = (BMCArg *)Addr;
    BMCInst = GratArpArgs->BMCInst;
    BMCInfo_t *pBMCInfo = &g_BMCInfo[BMCInst];
    prctl(PR_SET_NAME,__FUNCTION__,0,0,0);

    memcpy(&EthIndex,GratArpArgs->Argument,GratArpArgs->Len);

    memset(IfcName,0,sizeof(IfcName));
    /*Get the EthIndex*/
    //if(GetIfcName(EthIndex,IfcName, BMCInst) == -1)
    //{
    //    TCRIT("Error in Getting Ifcname\n");
    //    return 0;
    //}

    for(i=0;i<sizeof(Ifcnametable)/sizeof(IfcName_T);i++)
    {
        if(strcmp(Ifcnametable[i].Ifcname,IfcName) == 0)
        {
            netindex= Ifcnametable[i].Index;
            break;
        }
    }

    if(netindex == 0xFF)
    {
        TCRIT("Error in gettting netindex\n");
        return 0;
    }

    if(Addr != NULL)
    {
        free(GratArpArgs->Argument);
        free(Addr);
    }

    TDBG ("Gratuitous ARP thread starts for ethindex : %x\n",EthIndex);
    while (1)
    {
        /*Is Gratiutous Arp Enabled */
        if (0 == (pBMCInfo->LANCfs[EthIndex].BMCGeneratedARPControl & GRATIUTOUS_ENABLE_MASK))
        {
            TDBG("Gratuitous ARP thread exits : Disable BMC-generated Gratuitous ARPs invoked\n");
            break;
        }

        /*Is Gratiutous Arp Suspended */
        Status = BMC_GET_SHARED_MEM(BMCInst)->ArpSuspendStatus[EthIndex];

        if ((0 != (Status & GRATIUTOUS_ENABLE_MASK)) && 
                (BMC_GET_SHARED_MEM(BMCInst)->IsWDTRunning == TRUE))
        {
            // Gratuitous ARP Suspended.
            // sleep requested to access Shared memory for two different threads.
            usleep (20);
            continue;
        }

        nwReadNWCfg_v4_v6( &NWConfig,&NWConfig6, netindex,g_corefeatures.global_ipv6);
        if(GetIfcName(EthIndex, IfcName,BMCInst) != 0)
        {
            TCRIT("Error in getting Interface Name for the Lan Index :%d\n",EthIndex);

        }
        if (NWConfig.IFName[0] == 0)
            sprintf((char *)&NWConfig.IFName, "%s",IfcName);

        TDBG ( "MAC is %2x:%2x:%2x:%2x:%2x:%2x \n", NWConfig.MAC [0], NWConfig.MAC [1],
                NWConfig.MAC [2], NWConfig.MAC [3], NWConfig.MAC [4], NWConfig.MAC [5] );
        TDBG ( "IP is %d.%d.%d.%d\n", NWConfig.IPAddr[0], NWConfig.IPAddr[1],
                NWConfig.IPAddr[2], NWConfig.IPAddr[3]);
        TDBG ( "Device Name : %s\n", (char *)&NWConfig.IFName);
#if 0
        nRet = SendGratuitousARPPacket((char *)&NWConfig.IFName, NWConfig.IPAddr, NWConfig.MAC);
        if (0 != nRet)
        {
            TCRIT("Gratuitous ARP thread exits : Unable to Send Gratuitous ARP packet\n");
            break;
        }
        TDBG ("Send Gratuitous Packet\n");
#endif
        if (0 == pBMCInfo->LANCfs[EthIndex].GratitousARPInterval)
        {
            IntervalInSec = 2; //Default 2 secs
        } else {
            // Gratuitous ARP interval in 500 millisecond increments.
            IntervalInSec = (pBMCInfo->LANCfs[EthIndex].GratitousARPInterval * 500)/1000;
        }
        usleep( IntervalInSec * 1000 * 1000 );
    }
    return 0;
}


/*---------------------------------------------------
 * GetLanConfigParam
 *---------------------------------------------------*/
    int
GetLanConfigParam (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  GetLanConfigReq_T*  pGetLanReq = (_NEAR_ GetLanConfigReq_T*) pReq;
    _NEAR_  GetLanConfigRes_T*  pGetLanRes = (_NEAR_ GetLanConfigRes_T*) pRes;
    _FAR_   BMCSharedMem_T*     pSharedMem = BMC_GET_SHARED_MEM (BMCInst);
    INT8U IsOemDefined = FALSE;
    NWCFG_STRUCT        NWConfig;
    NWCFG6_STRUCT        NWConfig6;
    //    V6DNS_CONFIG v6dnsconfig;
    INT8U EthIndex,netindex= 0xFF,i;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    int ncsiPortConfigNum = 0;

    ETHCFG_STRUCT PHYCfg;
    ChannelInfo_T* pChannelInfo = NULL;

    char IfcName[16];  /* Eth interface name */
    INT8U               ComStrLen=MAX_COMM_STRING_SIZE;
    int retValue = 0,NIC_Count = 0;

    pGetLanRes->CCParamRev.CompletionCode = CC_NORMAL;
    pGetLanRes->CCParamRev.ParamRevision  = PARAMETER_REVISION_FORMAT;

    if(pGetLanReq->ChannelNum & RESERVED_VALUE_70)
    {
        /* Alarm !!! Somebody is trying to set Reseved Bits */
        *pRes = CC_INV_DATA_FIELD;
        return sizeof (*pRes);
    }

    if((pGetLanReq->ParameterSelect >= MIN_LAN_OEM_CONF_PARAM) && 
            (pGetLanReq->ParameterSelect <= MAX_LAN_OEM_CONF_PARAM) )
    {
        /* Converts OEM parameter value to equivalent AMI parameter value */
        if (0 != GetLanAMIParamValue (&pGetLanReq->ParameterSelect, &IsOemDefined) )
        {
            pGetLanRes->CCParamRev.CompletionCode = CC_PARAM_NOT_SUPPORTED;
            return sizeof(INT8U);
        }
#if 0
        /* Hook for OEM to handle this parameter */
        if ( (IsOemDefined)  && (g_PDKHandle[PDK_GETLANOEMPARAM] != NULL) )
        {
            return ((int(*)(INT8U *, INT8U, INT8U *,int))(g_PDKHandle[PDK_GETLANOEMPARAM]))(pReq, ReqLen, pRes, BMCInst);
        }
#endif   	
    }
#if 0
    if (g_PDKHandle[PDK_BEFOREGETLANPARM] != NULL)
    {
        retValue = ((int(*)(INT8U *, INT8U, INT8U *,int))(g_PDKHandle[PDK_BEFOREGETLANPARM]))(pReq, ReqLen, pRes, BMCInst);
        if(retValue != 0)
        {
            return retValue;
        }
    }
#endif
    //! Validate the SetSelector value. 
    if (  (0x00 != pGetLanReq->SetSelect) &&  
            (pGetLanReq->ParameterSelect != LAN_PARAM_SELECT_DEST_TYPE) &&  
            (pGetLanReq->ParameterSelect != LAN_PARAM_SELECT_DEST_ADDR) &&  
            (pGetLanReq->ParameterSelect != LAN_PARAM_VLAN_TAGS) &&
            (pGetLanReq->ParameterSelect != LAN_PARAMS_AMI_OEM_SNMPV6_DEST_ADDR) && 
            (pGetLanReq->ParameterSelect != LAN_PARAMS_AMI_OEM_IPV6_IP_ADDR) &&
            (pGetLanReq->ParameterSelect != LAN_PARAMS_AMI_OEM_IPV6_PREFIX_LENGTH))
    {
        if( g_corefeatures.ncsi_cmd_support == ENABLED )
        {
            if(pGetLanReq->ParameterSelect != LAN_PARAMS_AMI_OEM_NCSI_SETTINGS)
            {
                *pRes = CC_INV_DATA_FIELD;  
                return sizeof (*pRes);  
            }
        }
        else
        {
            *pRes = CC_INV_DATA_FIELD;  
            return sizeof (*pRes);  
        }
    }  

    //! Validate the BlockSelector value.  
    if (0x00 != pGetLanReq->BlockSelect)  
    {
        *pRes = CC_INV_DATA_FIELD;  
        return sizeof (*pRes);  
    }


    EthIndex= GetEthIndex(pGetLanReq->ChannelNum & 0x0F, BMCInst);
    if(0xff == EthIndex)
    {
        *pRes = CC_INV_DATA_FIELD;
        return sizeof (INT8U);
    }

    /*Get the EthIndex*/
    if(GetIfcName(EthIndex,IfcName, BMCInst) != 0)
    {
        TCRIT("Error in Getting IfcName\n");
        *pRes = CC_INV_DATA_FIELD;
        return sizeof (INT8U);
    }

    for(i=0;i<sizeof(Ifcnametable)/sizeof(IfcName_T);i++)
    {
        if(strcmp(Ifcnametable[i].Ifcname,IfcName) == 0)
        {
            netindex= Ifcnametable[i].Index;
            break;
        }
    }

    if(netindex == 0xFF)
    {
        TCRIT("Error in Getting netindex %d %s\n",netindex,IfcName);
        *pRes = CC_INV_DATA_FIELD;
        return sizeof (INT8U);
    }

    if ((pGetLanReq->ChannelNum & GET_PARAMETER_REVISION_MASK) != 0)
    {
        if((MAX_LAN_CONF_PARAM >= pGetLanReq->ParameterSelect) ||
                ((MIN_LAN_OEM_CONF_PARAM <= pGetLanReq->ParameterSelect) && (MAX_LAN_OEM_CONF_PARAM >= pGetLanReq->ParameterSelect)) ) 
        {
            return sizeof(GetLanCCRev_T);
        }
        else
        {
            *pRes = CC_PARAM_NOT_SUPPORTED;
            return sizeof (*pRes);  
        }

    }
    else
    {
        switch(pGetLanReq->ParameterSelect)
        {
            case LAN_PARAM_SET_IN_PROGRESS:

                LOCK_BMC_SHARED_MEM(BMCInst);
                pGetLanRes->ConfigData.SetInProgress = BMC_GET_SHARED_MEM(BMCInst)->m_Lan_SetInProgress;
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                break;

            case LAN_PARAM_AUTH_TYPE_SUPPORT:

                pGetLanRes->ConfigData.AuthTypeSupport = pBMCInfo->LANCfs[EthIndex].AuthTypeSupport;
                break;

            case LAN_PARAM_AUTH_TYPE_ENABLES:

                LOCK_BMC_SHARED_MEM(BMCInst);
                _fmemcpy (&pGetLanRes->ConfigData.AuthTypeEnables,
                        &(pBMCInfo->LANCfs[EthIndex].AuthTypeEnables), sizeof(AuthTypeEnables_T));
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                break;

            case LAN_PARAM_IP_ADDRESS:
                /* Check for VLAN enable bit */
                nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, netindex,g_corefeatures.global_ipv6);
                _fmemcpy (pGetLanRes->ConfigData.IPAddr, NWConfig.IPAddr, IP_ADDR_LEN);


                break;

            case LAN_PARAM_IP_ADDRESS_SOURCE:
                nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, netindex,g_corefeatures.global_ipv6);

                pGetLanRes->ConfigData.IPAddrSrc = NWConfig.CfgMethod;
                break;

            case LAN_PARAM_MAC_ADDRESS:

                nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, netindex,g_corefeatures.global_ipv6);
                _fmemcpy (pGetLanRes->ConfigData.MACAddr, NWConfig.MAC, MAC_ADDR_LEN);

                break;

            case LAN_PARAM_SUBNET_MASK:
                nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, netindex,g_corefeatures.global_ipv6);
                _fmemcpy (pGetLanRes->ConfigData.SubNetMask, NWConfig.Mask, IP_ADDR_LEN);
                break;

            case LAN_PARAM_IPv4_HEADER:

                LOCK_BMC_SHARED_MEM(BMCInst);
                _fmemcpy (&pGetLanRes->ConfigData.Ipv4HdrParam,
                        &(pBMCInfo->LANCfs[EthIndex].Ipv4HdrParam), sizeof(IPv4HdrParams_T));
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                break;

            case LAN_PARAM_PRI_RMCP_PORT:

                pGetLanRes->ConfigData.PrimaryRMCPPort = htoipmi_u16(pBMCInfo->LANCfs[EthIndex].PrimaryRMCPPort);
                break;

            case LAN_PARAM_SEC_RMCP_PORT:
                /* Returning Invalid error message */
                *pRes = CC_PARAM_NOT_SUPPORTED;
                return sizeof (INT8U);
                /*pGetLanRes->ConfigData.SecondaryPort = htoipmi_u16(pPMConfig->LANConfig[EthIndex].SecondaryPort);*/
                break;

            case LAN_PARAM_BMC_GENERATED_ARP_CONTROL:

                pGetLanRes->ConfigData.BMCGeneratedARPControl = pBMCInfo->LANCfs[EthIndex].BMCGeneratedARPControl;
                break;
            case LAN_PARAM_GRATITIOUS_ARP_INTERVAL:
                pGetLanRes->ConfigData.GratitousARPInterval =
                    pBMCInfo->LANCfs[EthIndex].GratitousARPInterval;
                break;

            case LAN_PARAM_DEFAULT_GATEWAY_IP:

                nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, netindex,g_corefeatures.global_ipv6);
                _fmemcpy (pGetLanRes->ConfigData.DefaultGatewayIPAddr, NWConfig.Gateway, IP_ADDR_LEN);
                break;

            case LAN_PARAM_DEFAULT_GATEWAY_MAC:

                LOCK_BMC_SHARED_MEM(BMCInst);
                _fmemcpy (pGetLanRes->ConfigData.DefaultGatewayMACAddr,
                        pBMCInfo->LANCfs[EthIndex].DefaultGatewayMACAddr, MAC_ADDR_LEN);
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                break;

            case LAN_PARAM_BACKUP_GATEWAY_IP:

                LOCK_BMC_SHARED_MEM(BMCInst);
                nwGetBkupGWyAddr(pGetLanRes->ConfigData.BackupGatewayIPAddr,netindex);
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                break;

            case LAN_PARAM_BACKUP_GATEWAY_MAC:

                LOCK_BMC_SHARED_MEM(BMCInst);
                _fmemcpy (pGetLanRes->ConfigData.BackupGatewayMACAddr,
                        pBMCInfo->LANCfs[EthIndex].BackupGatewayMACAddr, MAC_ADDR_LEN);
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                break;

            case LAN_PARAM_COMMUNITY_STRING:
#if 0
                if (g_PDKHandle[PDK_GETSNMPCOMMUNITYNAME] != NULL )
                {
                    if(((int(*)(INT8U *, INT8U *,int))(g_PDKHandle[PDK_GETSNMPCOMMUNITYNAME]))(pGetLanRes->ConfigData.CommunityStr,&ComStrLen, BMCInst)==0)
                        break;
                }
#endif
                LOCK_BMC_SHARED_MEM(BMCInst);
                _fmemcpy (pGetLanRes->ConfigData.CommunityStr,
                        pBMCInfo->LANCfs[EthIndex].CommunityStr, MAX_COMM_STRING_SIZE);
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                break;

            case LAN_PARAM_DEST_NUM:

                pGetLanRes->ConfigData.NumDest = pBMCInfo->LANCfs[EthIndex].NumDest;
                break;

            case LAN_PARAM_SELECT_DEST_TYPE:


                //if (pGetLanReq->SetSelect > NUM_LAN_DESTINATION)
                if ( pGetLanReq->SetSelect   > pBMCInfo->LANCfs[EthIndex].NumDest )
                {
                    *pRes = CC_PARAM_OUT_OF_RANGE ;
                    return sizeof (*pRes);
                }

                if (0 == pGetLanReq->SetSelect)
                {
                    LOCK_BMC_SHARED_MEM(BMCInst);
                    _fmemcpy (&pGetLanRes->ConfigData.DestType,
                            &pSharedMem->VolLANDestType[EthIndex], sizeof(LANDestType_T));
                    UNLOCK_BMC_SHARED_MEM(BMCInst);
                }
                else
                {
                    LOCK_BMC_SHARED_MEM(BMCInst);
                    _fmemcpy (&pGetLanRes->ConfigData.DestType,
                            &(pBMCInfo->LANCfs[EthIndex].DestType[pGetLanReq->SetSelect - 1]),
                            sizeof(LANDestType_T));
                    UNLOCK_BMC_SHARED_MEM(BMCInst);
                }
                break;

            case LAN_PARAM_SELECT_DEST_ADDR:

                //if (pGetLanReq->SetSelect > NUM_LAN_DESTINATION)
                if ( pGetLanReq->SetSelect   > pBMCInfo->LANCfs[EthIndex].NumDest )
                {
                    *pRes = CC_PARAM_OUT_OF_RANGE ;
                    return sizeof (*pRes);
                }

                if (0 == pGetLanReq->SetSelect)
                {
                    LOCK_BMC_SHARED_MEM(BMCInst);
                    _fmemcpy (&pGetLanRes->ConfigData.DestAddr,
                            &pSharedMem->VolLANDest[EthIndex], sizeof(LANDestAddr_T));
                    UNLOCK_BMC_SHARED_MEM(BMCInst);
                }
                else
                {
                    LOCK_BMC_SHARED_MEM(BMCInst);
                    _fmemcpy (&pGetLanRes->ConfigData.DestAddr,
                            &(pBMCInfo->LANCfs[EthIndex].DestAddr[pGetLanReq->SetSelect - 1]),
                            sizeof(LANDestAddr_T));
                    UNLOCK_BMC_SHARED_MEM(BMCInst);
                }
                break;

            case LAN_PARAM_VLAN_ID:

                if( pBMCInfo->IpmiConfig.VLANIfcSupport == 1)
                {
                    pGetLanRes->ConfigData.VLANID = pBMCInfo->LANCfs[EthIndex].VLANID;
                }
                break;

            case LAN_PARAM_VLAN_PRIORITY:

                pGetLanRes->ConfigData.VLANPriority = pBMCInfo->LANCfs[EthIndex].VLANPriority;
                break;

            case LAN_PARAM_CIPHER_SUITE_ENTRY_SUP:

                pGetLanRes->ConfigData.CipherSuiteSup = N0_OF_CIPHER_SUITE_SUPPORTED;
                break;

            case LAN_PARAM_CIPHER_SUITE_ENTRIES:
                {
                    int i;
                    _fmemset (pGetLanRes->ConfigData.CipherSuiteEntries, 0,
                            sizeof (pGetLanRes->ConfigData.CipherSuiteEntries));
                    for (i = 0; i < (N0_OF_CIPHER_SUITE_SUPPORTED); i++)
                    {
                        pGetLanRes->ConfigData.CipherSuiteEntries[i+1] = g_CipherRec[1 + i * 5];
                    }
                }
                break;

            case LAN_PARAM_CIPHER_SUITE_PRIV_LEVELS:

                LOCK_BMC_SHARED_MEM(BMCInst);
                _fmemcpy (pGetLanRes->ConfigData.CipherSuitePrivLevels,
                        pBMCInfo->LANCfs[EthIndex].CipherSuitePrivLevels,
                        MAX_NUM_CIPHER_SUITE_PRIV_LEVELS);
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                break;

            case LAN_PARAM_VLAN_TAGS:

                *((_NEAR_ INT8U*)&pGetLanRes->ConfigData) = pGetLanReq->SetSelect;
                if (pGetLanReq->SetSelect > pBMCInfo->LANCfs[EthIndex].NumDest)
                {
                    pGetLanRes->CCParamRev.CompletionCode = CC_PARAM_OUT_OF_RANGE;
                    return sizeof (GetLanCCRev_T);
                }
                if (0 == pGetLanReq->SetSelect)
                {
                    LOCK_BMC_SHARED_MEM(BMCInst);
                    _fmemcpy (((_NEAR_ INT8U*) &pGetLanRes->ConfigData) + 1,&pSharedMem->VLANDestTag, sizeof(VLANDestTags_T));
                    UNLOCK_BMC_SHARED_MEM(BMCInst);
                }
                else
                {
                    if (pGetLanReq->SetSelect > pBMCInfo->LANCfs[EthIndex].NumDest)
                    {
                        pGetLanRes->CCParamRev.CompletionCode = CC_PARAM_OUT_OF_RANGE;
                        return sizeof(GetLanCCRev_T);
                    }

                    LOCK_BMC_SHARED_MEM(BMCInst);
                    _fmemcpy (((_NEAR_ INT8U*)&pGetLanRes->ConfigData) + 1,
                            &pBMCInfo->LANCfs[EthIndex].VLANDestTags[pGetLanReq->SetSelect - 1],
                            sizeof(VLANDestTags_T));
                    UNLOCK_BMC_SHARED_MEM(BMCInst);
                }
                break;

            case LAN_PARAMS_BAD_PASSWORD_THRESHOLD:
                LOCK_BMC_SHARED_MEM(BMCInst);
                _fmemcpy(&pGetLanRes->ConfigData.BadPasswd,
                        &pBMCInfo->LANCfs[EthIndex].BadPasswd,sizeof(BadPassword_T));
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                break;
            case LAN_PARAMS_AMI_OEM_SNMPV6_DEST_ADDR:

                if ( pGetLanReq->SetSelect   > pBMCInfo->LANCfs[EthIndex].NumDest )
                {
                    *pRes = CC_PARAM_OUT_OF_RANGE ;
                    return sizeof (*pRes);
                }

                if (0 == pGetLanReq->SetSelect)
                {
                    LOCK_BMC_SHARED_MEM(BMCInst);
                    _fmemcpy (&pGetLanRes->ConfigData.Destv6Addr,
                            &pSharedMem->VolLANv6Dest[EthIndex], sizeof(LANDestv6Addr_T));
                    UNLOCK_BMC_SHARED_MEM(BMCInst);
                }
                else
                {
                    LOCK_BMC_SHARED_MEM(BMCInst);
                    _fmemcpy (&pGetLanRes->ConfigData.Destv6Addr,
                            &(pBMCInfo->LANCfs[EthIndex].Destv6Addr[pGetLanReq->SetSelect - 1]),
                            sizeof(LANDestv6Addr_T));
                    UNLOCK_BMC_SHARED_MEM(BMCInst);
                }

                TDBG("\n GetLanconfig: Getting SNMPv6 configuration done..\n");

                return sizeof(GetLanConfigRes_T);

                break;

            case LAN_PARAMS_AMI_OEM_ENABLE_SET_MAC:

                pGetLanRes->ConfigData.ChangeMACEnabled = enableSetMACAddr;
                return sizeof(GetLanCCRev_T) + sizeof(INT8U);

            case LAN_PARAMS_AMI_OEM_IPV6_ENABLE:
                nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, netindex,g_corefeatures.global_ipv6);
                pGetLanRes->ConfigData.IPv6_Enable = NWConfig6.enable;
                return sizeof(GetLanCCRev_T) + sizeof(INT8U);

                break;

            case LAN_PARAMS_AMI_OEM_IPV6_IP_ADDR_SOURCE:
                nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, netindex,g_corefeatures.global_ipv6);
                pGetLanRes->ConfigData.IPv6_IPAddrSrc = NWConfig6.CfgMethod;
                return sizeof(GetLanCCRev_T) + sizeof(INT8U);

                break;

            case LAN_PARAMS_AMI_OEM_IPV6_IP_ADDR:
                nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, netindex,g_corefeatures.global_ipv6);
                _fmemcpy (pGetLanRes->ConfigData.IPv6_LinkAddr, NWConfig6.GlobalIPAddr[(pGetLanReq->SetSelect & 0x0F)], IP6_ADDR_LEN);
                return sizeof(GetLanCCRev_T) + IP6_ADDR_LEN;

                break;
            case LAN_PARAMS_AMI_OEM_IPV6_LINK_ADDR:
                nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, netindex,g_corefeatures.global_ipv6);
                _fmemcpy (pGetLanRes->ConfigData.IPv6_LinkAddr, NWConfig6.LinkIPAddr, IP6_ADDR_LEN);
                return sizeof(GetLanCCRev_T) + IP6_ADDR_LEN;

            case LAN_PARAMS_AMI_OEM_IPV6_PREFIX_LENGTH:
                nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, netindex,g_corefeatures.global_ipv6);
                pGetLanRes->ConfigData.IPv6_LinkAddrPrefix = NWConfig6.GlobalPrefix[(pGetLanReq->SetSelect & 0x0F)];
                return sizeof(GetLanCCRev_T) + sizeof(INT8U);

                break;

            case LAN_PARAMS_AMI_OEM_IPV6_LINK_ADDR_PREFIX:
                nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, netindex,g_corefeatures.global_ipv6);
                pGetLanRes->ConfigData.IPv6_LinkAddrPrefix = NWConfig6.LinkPrefix;
                return sizeof(GetLanCCRev_T) + sizeof(INT8U);

                break;

            case LAN_PARAMS_AMI_OEM_IPV6_GATEWAY_IP:
                nwReadNWCfg_v4_v6( &NWConfig, &NWConfig6, netindex,g_corefeatures.global_ipv6);
                _fmemcpy (pGetLanRes->ConfigData.IPv6_GatewayIPAddr, NWConfig6.Gateway, IP6_ADDR_LEN);
                return sizeof(GetLanCCRev_T) + IP6_ADDR_LEN;

                break;

            case LAN_PARAMS_AMI_OEM_PHY_SETTINGS:
                if(g_corefeatures.phy_support == ENABLED)
                {
                    memset(IfcName,0,sizeof(IfcName));
                    if(GetIfcNameByIndex(EthIndex, IfcName) != 0)
                    {
                        TCRIT("Error in Getting Interface Name for the lan Index:%d\n",EthIndex);
                    }
                    if(nwGetEthInformation(&PHYCfg, IfcName) !=0)
                    {
                        pGetLanRes->CCParamRev.CompletionCode = CC_INV_DATA_FIELD;
                        return sizeof(GetLanCCRev_T);
                    }

                    pGetLanRes->ConfigData.PHYConfig.Interface = EthIndex;
                    pGetLanRes->ConfigData.PHYConfig.AutoNegotiationEnable = PHYCfg.autoneg;
                    pGetLanRes->ConfigData.PHYConfig.Speed = PHYCfg.speed;
                    pGetLanRes->ConfigData.PHYConfig.Duplex = PHYCfg.duplex;
                    pGetLanRes->ConfigData.PHYConfig.CapabilitiesSupported = PHYCfg.supported;

                    return sizeof(GetLanCCRev_T) + sizeof(PHYConfig_T);
                }
                else
                {
                    pGetLanRes->CCParamRev.CompletionCode = CC_PARAM_NOT_SUPPORTED;
                    return sizeof(INT8U);
                }
            case LAN_PARAMS_SSI_OEM_2ND_PRI_ETH_MAC_ADDR:
                if(g_corefeatures.ssi_support == ENABLED)
                {
                    netindex = 0x1; /* Specify the 2nd interface */
                    nwReadNWCfg_v4_v6(&NWConfig, &NWConfig6, netindex,g_corefeatures.global_ipv6);
                    _fmemcpy(pGetLanRes->ConfigData.SSI2ndPriEthMACAddr, NWConfig.MAC, MAC_ADDR_LEN);
                    return sizeof(GetLanCCRev_T) + sizeof(pGetLanRes->ConfigData.SSI2ndPriEthMACAddr);
                }
                else
                {
                    pGetLanRes->CCParamRev.CompletionCode = CC_PARAM_NOT_SUPPORTED;
                    return sizeof(INT8U);
                }
                break;

            case LAN_PARAMS_SSI_OEM_LINK_CTRL:
                if(g_corefeatures.ssi_support == ENABLED)
                {
                    pGetLanRes->ConfigData.SSILinkControl = 0;

                    pChannelInfo = getChannelInfo(pBMCInfo->RMCPLAN1Ch, BMCInst);
                    if(NULL == pChannelInfo)
                    {
                        *pRes = CC_UNSPECIFIED_ERR;
                        return	sizeof (*pRes);
                    }

                    if (pChannelInfo->AccessMode == 0x02)		/* If 1st channal is available */
                        pGetLanRes->ConfigData.SSILinkControl |= BIT0;
                    NIC_Count = g_coremacros.global_nic_count;
                    if (NIC_Count == 2)
                    {
                        pChannelInfo = getChannelInfo(pBMCInfo->RMCPLAN2Ch, BMCInst);
                        if(NULL == pChannelInfo)
                        {
                            *pRes = CC_UNSPECIFIED_ERR;
                            return	sizeof (*pRes);
                        }

                        if (pChannelInfo->AccessMode == 0x02)	/* If 2nd channal is available */
                            pGetLanRes->ConfigData.SSILinkControl |= BIT1;
                    }
                    return sizeof(GetLanCCRev_T) + sizeof(pGetLanRes->ConfigData.SSILinkControl);
                }
                else
                {
                    pGetLanRes->CCParamRev.CompletionCode = CC_PARAM_NOT_SUPPORTED;
                    return sizeof(INT8U);
                }
                break;

            case LAN_PARAMS_SSI_OEM_CMM_IP_ADDR:
                if(g_corefeatures.ssi_support == ENABLED)
                {
                    _fmemcpy (pGetLanRes->ConfigData.CMMIPAddr, pBMCInfo->SSIConfig.CMMIPAddr, IP_ADDR_LEN);
                    return sizeof(GetLanCCRev_T) + sizeof(pGetLanRes->ConfigData.CMMIPAddr);
                }
                else
                {
                    pGetLanRes->CCParamRev.CompletionCode = CC_PARAM_NOT_SUPPORTED;
                    return sizeof(INT8U);
                }
                break;

            default:
                if(g_corefeatures.ncsi_cmd_support == ENABLED)
                {
                    switch(pGetLanReq->ParameterSelect)
                    {
                        case LAN_PARAMS_AMI_OEM_NCSI_CONFIG_NUM:
                            //                        NCSIGetTotalPorts(&ncsiPortConfigNum);

                            if (ncsiPortConfigNum >= 0xFF)
                            {
                                pGetLanRes->CCParamRev.CompletionCode = CC_INV_DATA_FIELD;
                                return sizeof(GetLanCCRev_T);
                            }
                            else
                            {
                                pGetLanRes->ConfigData.NumNCSIPortConfigs = ncsiPortConfigNum;
                                return sizeof(GetLanCCRev_T) + sizeof(INT8U);
                            }

                        case LAN_PARAMS_AMI_OEM_NCSI_SETTINGS:
                            NIC_Count=g_coremacros.global_nic_count;
                            if (pGetLanReq->SetSelect >= NIC_Count)
                            {
                                pGetLanRes->CCParamRev.CompletionCode = CC_INV_DATA_FIELD;
                                return sizeof(GetLanCCRev_T);
                            }
                            NCSIConfig_T configData;
                            char interfaceName[8];

                            memset(&configData, 0, sizeof(NCSIConfig_T));
                            memset(interfaceName, 0, sizeof(interfaceName));

                            snprintf(interfaceName, sizeof(interfaceName), "%s%d", "eth", pGetLanReq->SetSelect);
#if 0                        
                            if (NCSIGetPortConfigByName(interfaceName, &configData) != 0)
                            {
                                pGetLanRes->CCParamRev.CompletionCode = CC_INV_DATA_FIELD;
                                return sizeof(GetLanCCRev_T);
                            }
#endif
                            if (configData.PackageId >= 0xFF || configData.ChannelId >= 0xFF)
                            {
                                pGetLanRes->CCParamRev.CompletionCode = CC_INV_DATA_FIELD;
                                return sizeof(GetLanCCRev_T);
                            }

                            pGetLanRes->ConfigData.NCSIPortConfig.Interface = pGetLanReq->SetSelect;
                            pGetLanRes->ConfigData.NCSIPortConfig.PackageId = configData.PackageId;
                            pGetLanRes->ConfigData.NCSIPortConfig.ChannelId = configData.ChannelId;

                            return sizeof(GetLanCCRev_T) + sizeof(NCSIPortConfig_T);

                        default :
                            TDBG("In Valid Option\n");

                    }
                }
                pGetLanRes->CCParamRev.CompletionCode = CC_PARAM_NOT_SUPPORTED;
                return sizeof(INT8U);
        }

    }


    return sizeof(GetLanCCRev_T) + LanconfigParameterLength[pGetLanReq->ParameterSelect];
}


/*---------------------------------------------------
 * SuspendBMCArps
 *---------------------------------------------------*/
    int
SuspendBMCArps (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  SuspendBMCArpsReq_T*    pArpReq = (_NEAR_ SuspendBMCArpsReq_T*) pReq;
    _NEAR_  SuspendBMCArpsRes_T*    pArpRes = (_NEAR_ SuspendBMCArpsRes_T*) pRes;
    INT8U EthIndex;

    /* Verify Channel ID */
    pArpRes->CompletionCode = CC_INV_DATA_FIELD;

    if(pArpReq->ChannelNo & RESERVED_BITS_SUSPENDBMCARPS) return sizeof(*pRes);

    if (!IsLANChannel(pArpReq->ChannelNo & CHANNEL_ID_MASK, BMCInst) )
    {
        return sizeof (*pRes);
    }

    EthIndex= GetEthIndex(pArpReq->ChannelNo & 0x0F,BMCInst);
    if(0xff == EthIndex)
    {
        return sizeof (*pRes);
    }

    /* Reserved Bits Validation */
    if (0 != (SUSPEND_ARP_RSVD_BIT_MASK & pArpReq->ArpSuspend))
    {
        return sizeof (*pRes);
    }

    if (BMC_GET_SHARED_MEM(BMCInst)->IsWDTRunning == TRUE)
        BMC_GET_SHARED_MEM(BMCInst)->ArpSuspendStatus[EthIndex] = pArpReq->ArpSuspend;

    /* Update Response */
    pArpRes->CompletionCode   = CC_NORMAL;
    pArpRes->ArpSuspendStatus = UpdateArpStatus(EthIndex, BMC_GET_SHARED_MEM(BMCInst)->IsWDTRunning, BMCInst);

    return sizeof (SuspendBMCArpsRes_T);
}



/*---------------------------------------------------
 * GetIPUDPRMCPStats
 *---------------------------------------------------*/
    int
GetIPUDPRMCPStats (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  GetIPUDPRMCPStatsReq_T*    pGetIPUDPRMCPStatsReq = (_NEAR_ GetIPUDPRMCPStatsReq_T*) pReq;
    _NEAR_  GetIPUDPRMCPStatsRes_T*    pGetIPUDPRMCPStatsRes = (_NEAR_ GetIPUDPRMCPStatsRes_T*) pRes;
    _FAR_	BMCSharedMem_T* 	pSharedMem = BMC_GET_SHARED_MEM (BMCInst);

    FILE *fptr;
    char FDRead[512], FSRead[512];
    char *result = NULL;
    char **StrArray;
    int count = 0;
    int cStrings =0;

    if(pGetIPUDPRMCPStatsReq->ChannelNo & RESERVED_BITS_GETIPUDPRMCPSTATS_CH)
    {
        *pRes = CC_INV_DATA_FIELD;
        return sizeof(*pRes);
    }

    if(pGetIPUDPRMCPStatsReq->ClearStatus & RESERVED_BITS_GETIPUDPRMCPSTATS_CLRSTATE)
    {
        *pRes = CC_INV_DATA_FIELD;
        return sizeof(*pRes);
    }

    //Channel number [3:0] is valid.
    if ( !IsLANChannel(pGetIPUDPRMCPStatsReq->ChannelNo & CHANNEL_ID_MASK, BMCInst) )
    {
        pGetIPUDPRMCPStatsRes->CompletionCode = CC_INV_DATA_FIELD;
        return sizeof (*pRes);
    }

    //Clear the statistics if 1 is given in request.
    if ( pGetIPUDPRMCPStatsReq->ClearStatus & 0x01 )
    {
        /* <TBD BalaT>
         * Have to clear the already stored statistics when clear req is 1.
         */
        pGetIPUDPRMCPStatsRes->CompletionCode = CC_NORMAL;
        return sizeof (INT8U);
    }

    //All Statistics values are initialised to 0.
    memset (pGetIPUDPRMCPStatsRes, 0, sizeof(GetIPUDPRMCPStatsRes_T) );

    //All the datas are taken from /proc/net/snmp file
    fptr = fopen ("/proc/net/snmp","r+");
    if (fptr == NULL)
    {
        pGetIPUDPRMCPStatsRes->CompletionCode = CC_COULD_NOT_PROVIDE_RESP;
        return sizeof (INT8U);
    }

    while( NULL != fgets(FDRead,512,fptr) )
    {
        if ( NULL != strstr (FDRead, "Ip: ") )
        {
            count++;
            if (count == 2)
            {
                //To find the no. of valid strings in a line.
                strcpy (FSRead, FDRead);
                result = strtok( FSRead, " " );
                while( result != NULL )
                {
                    cStrings++;
                    result = strtok( NULL, " " );
                }

                //Condition to check so that explode doesnt try to read the strings from unknown location.
                if ( cStrings == 20)
                {
                    StrArray = explode (' ', FDRead);
                }
                else
                {
                    pGetIPUDPRMCPStatsRes->CompletionCode = CC_NORMAL;
                    return sizeof (INT8U);
                }

                //All Statistics stops acumulating at FFFFh unless otherwise noted.
                //IP packets received.
                if ( atol((char *)StrArray[3]) > 0xffff)
                    pGetIPUDPRMCPStatsRes->IPPacketsRecv = ( ( atol((char *)StrArray[3]) ) % 65535);
                else
                    pGetIPUDPRMCPStatsRes->IPPacketsRecv = atoi((char *)StrArray[3]);

                //IP Header Error.
                pGetIPUDPRMCPStatsRes->IPHeaderErr = atoi((char *)StrArray[4]);

                //IP Address Error.
                if ( atol((char *)StrArray[5]) > 0xffff)
                    pGetIPUDPRMCPStatsRes->IPAddrErr = ( ( atol((char *)StrArray[5]) ) % 65535);
                else
                    pGetIPUDPRMCPStatsRes->IPAddrErr = atoi((char *)StrArray[5]);

                //Fragmented IP Packets received.
                if ( atol((char *)StrArray[17]) > 0xffff)
                    pGetIPUDPRMCPStatsRes->FragIPPacketsRecv = ( ( atol((char *)StrArray[17]) ) % 65535);
                else
                    pGetIPUDPRMCPStatsRes->FragIPPacketsRecv = atoi((char *)StrArray[17]);

                //IP packets Transmitted.
                if ( atol((char *)StrArray[10]) > 0xffff)
                    pGetIPUDPRMCPStatsRes->IPPacketsTrans = ( ( atol((char *)StrArray[10]) ) % 65535);
                else
                    pGetIPUDPRMCPStatsRes->IPPacketsTrans = atoi((char *)StrArray[10]);

                count = 0;
            }
        }

        if ( NULL != strstr (FDRead, "Udp: ") )
        {
            count++;
            if (count == 2)
            {
                //To find the no. of valid strings in a line.
                cStrings = 0;
                strcpy (FSRead, FDRead);
                result = strtok( FSRead, " " );
                while( result != NULL )
                {
                    cStrings++;
                    result = strtok( NULL, " " );
                }

                //Condition to check so that explode doesnt try to read the strings beyond the valid location.
                if ( cStrings == 5)
                {
                    StrArray = explode (' ', FDRead);
                }
                else
                {
                    pGetIPUDPRMCPStatsRes->CompletionCode = CC_NORMAL;
                    return sizeof (INT8U);
                }

                //UDP packets received.
                if ( atol((char *)StrArray[1]) > 0xffff)
                    pGetIPUDPRMCPStatsRes->UDPPacketsRecv = ( ( atol((char *)StrArray[1]) ) % 65535);
                else
                    pGetIPUDPRMCPStatsRes->UDPPacketsRecv = atoi((char *)StrArray[1]);

                count = 0;
            }
        }
    }
    fclose(fptr);

    //Valid RMCP packets received.
    pGetIPUDPRMCPStatsRes->ValidRMCPPackets = pSharedMem->gIPUDPRMCPStats;

    //<TBD BalaT>To store the statistics across the system reset and power cycles

    pGetIPUDPRMCPStatsRes->CompletionCode = CC_NORMAL;
    return sizeof (GetIPUDPRMCPStatsRes_T);

}

/*-----------------------------------------------------
 * explode
 * Funntion to split the strings and store in an array
 *-----------------------------------------------------*/

char **explode(char separator, char *string)
{
    int start = 0, i, k = 1, count = 2;
    char **strarr;

    for (i = 0; string[i] != '\0'; i++)
        /* how many rows do we need for our array? */
        if (string[i] == separator)
            count++;

    /* count is at least 2 to make room for the entire string
     *      * and the ending NULL */
    strarr = malloc(count * sizeof(char*));
    i = 0;

    while (*string++ != '\0')
    {
        if (*string == separator)
        {
            strarr[i] = malloc(k - start + 2);
            strncpy(strarr[i], string - k + start, k - start + 1);
            strarr[i][k - start + 1] = '\0'; /* guarantee null termination */
            start = k;
            i++;
        }
        k++;
    }
    /* copy the last part of the string after the last separator */
    strarr[i] = malloc(k - start);
    strncpy(strarr[i], string - k + start, k - start - 1);
    strarr[i][k - start - 1] = '\0'; /* guarantee null termination */
    strarr[++i] = NULL;

    return strarr;
}

/*----------------------------------------------
 * UpdateArpStatus
 *----------------------------------------------*/
    INT8U
UpdateArpStatus (INT8U EthIndex,BOOL IsTimerRunning, int BMCInst)
{
    INT8U GratArpSuspend;
    INT8U ArpSuspend;
    INT8U Status;
    BMCInfo_t *pBMCInfo = &g_BMCInfo[BMCInst];
    char Cmds[50]={0}; // command string to perform BMC-generated ARP
    char IfcName[16];

    IPMI_DBG_PRINT_1 ("Timer - %x", IsTimerRunning);
    GratArpSuspend = ArpSuspend = 1;
    int ethindex = pBMCInfo->LANConfig.LanIfcConfig[EthIndex].Ethindex;

    // Check Gratuitous ARP is Enabled
    if (0 == (pBMCInfo->LANCfs[ethindex].BMCGeneratedARPControl & GRATIUTOUS_ENABLE_MASK))
    {
        GratArpSuspend = 0;
    }

    // Check ARP Response is Enabled
    if (0 == (pBMCInfo->LANCfs[ethindex].BMCGeneratedARPControl & ENABLE_ARP_RESPONSES))
    {
        ArpSuspend = 0;
    }

    /*Disable ARP */
    if (TRUE == IsTimerRunning)
    {
        /* WDT is running, check and suspend ARP if necessary */	
        if( (0 != (BMC_GET_SHARED_MEM(BMCInst)->ArpSuspendStatus[ethindex] & SUSPEND_GRAT_ARP)) &&
                (0 < GratArpSuspend) )
        {
            GratArpSuspend--;
        }

        if( (0 != (BMC_GET_SHARED_MEM(BMCInst)->ArpSuspendStatus[ethindex] & SUSPEND_ARP)) &&
                (0 < ArpSuspend) )
        {
            ArpSuspend--;
        }
    }

    memset(IfcName,0,sizeof(IfcName));
    //if(GetIfcNameByIndex(EthIndex, IfcName) != 0)
    //{
    //    //TCRIT("Error in getting Interface Name for the Lan Index :%d\n", EthIndex);
    //}
    //else
    //{
    //    /* Perform commands for BMC-generated Arp */
    //    memset(Cmds, 0, sizeof(Cmds));
    //    sprintf(Cmds, "/usr/local/bin/ArpSwitch.sh %s %d", IfcName, (!ArpSuspend) ? ARP_IGNORE_ON : ARP_IGNORE_OFF);
    //    //    	safe_system (Cmds);	
    //    /* Perform commands for BMC-generated Arp ends */
    //}

    /* Update Status */
    Status = ArpSuspend << 1;
    Status = Status | GratArpSuspend;

    return Status;
}


/**
 *@fn NwInterfacePresenceCheck
 *@brief This function is invoked to check network interface presence
 *@param Interface - Char Pointer to Interface for which interface to check
 *@return Returns 0 on success
 */
    static int
NwInterfacePresenceCheck (char * Interface)
{
    int r;
    int sockdes;
    struct ifreq Ifreq;
    unsigned char MAC[MAC_ADDR_LEN];

    IPMI_DBG_PRINT_1 ("Checking the presence of %s\n", Interface);

    sockdes = socket(PF_INET, SOCK_DGRAM, 0 );
    if ( sockdes < 0 )
    {
        IPMI_ERROR("can't open socket: %s\n",strerror(errno));
        return -1;
    }

    /* Get MAC address */
    memset(&Ifreq,0,sizeof(struct ifreq));
    memset(MAC, 0, MAC_ADDR_LEN);
    strcpy(Ifreq.ifr_name, Interface);
    Ifreq.ifr_hwaddr.sa_family = AF_INET;
    r = ioctl(sockdes, SIOCGIFHWADDR, &Ifreq);
    close (sockdes);
    if ( r < 0 )
    {
        //IPMI_ERROR("IOCTL to get MAC failed: %d\n",r);
        return -1;
    }
    IPMI_DBG_PRINT_1 (" %s Interface is present\n", Interface);

    return 0;
}


// zc add

    int
PerpareNodeLanConfigParam (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  GetNodeLanConfigReq_T*  pGetLanReq = (_NEAR_ GetNodeLanConfigReq_T*) pReq;
    MsgPkt_T	MsgPkt;
    INT8U  nChannelNum = pGetLanReq->ChannelNum;
    INT8U   nNodeID = pGetLanReq->NodeID;

    MsgPkt.Param    = GET_LAN_CFG_ACTION;
    MsgPkt.Data[0]  = nChannelNum;
    MsgPkt.Data[1]  = nNodeID;
    MsgPkt.Size     = 2;
    PostMsg (&MsgPkt, NODE_LAN_CFG_Q, BMCInst);

    return 0;
}

#if 0
    int
CheckPerpare(_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  CheckPerpareReadyRes_T*  pGetLanRes = (_NEAR_ CheckPerpareReadyRes_T*) pRes;
    _FAR_ NodeLanCfgSharedMem_T*    pNodeLanCfgSharedMem;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    MsgPkt_T	MsgPkt;

    //	printf("zc test@%s entry! \n ", __FUNCTION__);

    pNodeLanCfgSharedMem = (_FAR_ NodeLanCfgSharedMem_T*)&pBMCInfo->NodeLanCfgSharedMem;

    pGetLanRes->CCParamRev.CompletionCode = CC_NORMAL;
    pGetLanRes->CCParamRev.ParamRevision  = PARAMETER_REVISION_FORMAT;

    /* Acquire Shared memory   */
    OS_THREAD_MUTEX_ACQUIRE(&g_BMCInfo[BMCInst].NodeLanCfgSharedMemMutex, WAIT_INFINITE);

    pGetLanRes->Ready = pNodeLanCfgSharedMem->ReadyToGet;

    /* Release mutex for  shared memory */
    OS_THREAD_MUTEX_RELEASE(&g_BMCInfo[BMCInst].NodeLanCfgSharedMemMutex);

    /* here, I set Sensor Monitor Task to ON */
    if (pGetLanRes->Ready == 1)
    {
        MsgPkt.Param	= 0x01; 	// SM_TASK_ON 	0x01;
        MsgPkt.Size 	= 0;
        PostMsg (&MsgPkt, NODE_SM_CFG_Q, BMCInst);
    }	

    TDBG("zc test@%s, ready = %x \n", __FUNCTION__, pGetLanRes->Ready);

    return sizeof(CheckPerpareReadyRes_T);;
}
#endif

    int
GetNodeLanConfigParam (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{

    _NEAR_  GetNodeLanConfigReq_T*  pGetLanReq = (_NEAR_ GetNodeLanConfigReq_T*) pReq;
    _NEAR_  GetNodeLanConfigRes_T*  pGetLanRes = (_NEAR_ GetNodeLanConfigRes_T*) pRes;
    _FAR_ NodeLanCfgSharedMem_T*    pNodeLanCfgSharedMem;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    INT8U  nNodeID = pGetLanReq->NodeID;
    //    INT8U  nChannelNum = pGetLanReq->ChannelNum;
    INT8U  pDataSize=0;
    NodeLANConfigUn_T*   NodeLanConfigData = NULL;

    TDBG("zc test@%s entry, b m_hNLSMSharedMemMutex \n", __FUNCTION__);
    /* Acquire Shared memory   */ 
    OS_THREAD_MUTEX_ACQUIRE(&pBMCInfo->m_hNLSMSharedMemMutex, WAIT_INFINITE);
    TDBG("garden@%s a m_hNLSMSharedMemMutex\n", __FUNCTION__); /* garden */

    pNodeLanCfgSharedMem = (_FAR_ NodeLanCfgSharedMem_T*)&pBMCInfo->NodeLanCfgSharedMem;

    pGetLanRes->CCParamRev.CompletionCode = CC_NORMAL;
    pGetLanRes->CCParamRev.ParamRevision  = PARAMETER_REVISION_FORMAT;
    pDataSize = sizeof(NodeLanCfgInfo_T);
    //    _fmemcpy(&pGetLanRes->NodeLanConfigData[nNodeID], &pNodeLanCfgSharedMem->NodeLancfgInfo[nNodeID], pDataSize);
    pGetLanRes->NodeLanConfigData[nNodeID].IPv4Addr[0]=pNodeLanCfgSharedMem->NodeLancfgInfo[nNodeID].IPv4Addr[0];
    pGetLanRes->NodeLanConfigData[nNodeID].IPv4Addr[1]=pNodeLanCfgSharedMem->NodeLancfgInfo[nNodeID].IPv4Addr[1];
    pGetLanRes->NodeLanConfigData[nNodeID].IPv4Addr[2]=pNodeLanCfgSharedMem->NodeLancfgInfo[nNodeID].IPv4Addr[2];
    pGetLanRes->NodeLanConfigData[nNodeID].IPv4Addr[3]=pNodeLanCfgSharedMem->NodeLancfgInfo[nNodeID].IPv4Addr[3];
    NodeLanConfigData = &pGetLanRes->NodeLanConfigData[nNodeID]; /* garden */
    /*printf("garden %s: nNodeID(%d) IPv4Addr(%d.%d.%d.%d)\n",
      __func__,nNodeID,
      NodeLanConfigData->IPv4Addr[0],
      NodeLanConfigData->IPv4Addr[1],
      NodeLanConfigData->IPv4Addr[2],
      NodeLanConfigData->IPv4Addr[3]);*/ /* garden */

    OS_THREAD_MUTEX_RELEASE(&pBMCInfo->m_hNLSMSharedMemMutex);

    //	printf("zc test@GetNodeLanConfigParam exit! \n");
    return sizeof(GetNodeLanConfigRes_T);
}

/*-------------------------------------------------------
 * SetNodeLanConfigParam
 *-------------------------------------------------------*/
    int
SetNodeLanConfigParam (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  SetNodeLanConfigReq_T*  pSetNodeLanReq = (_NEAR_ SetNodeLanConfigReq_T*) pReq;
    //    _NEAR_  SetNodeLanConfigRes_T*  pSetNodeLanRes = (_NEAR_ SetNodeLanConfigRes_T*) pRes;
    MsgPkt_T    MsgPkt;
    INT8U	nNodeID = pSetNodeLanReq->NodeID;
    INT8U   nChannelNum = pSetNodeLanReq->ChannelNum;
    INT8U   nParameterSelect = pSetNodeLanReq->ParameterSelect;
    INT8U   bRet = 0;

    TDBG("%s, nChannelNum = %x, NodeID = %x, nParameterSelect = %x \n", 
            __FUNCTION__, nChannelNum, nNodeID, nParameterSelect);

    MsgPkt.Param    = SET_LAN_CFG_ACTION;
    MsgPkt.Data[0]  = nChannelNum;
    MsgPkt.Data[1]  = nNodeID;
    MsgPkt.Data[2]  = nParameterSelect;

    switch (nParameterSelect)
    {
        case LAN_PARAM_IP_ADDRESS_SOURCE:
            MsgPkt.Data[3]  = pSetNodeLanReq->NodeLanConfigData.IPAddrSrc;
            MsgPkt.Size     = 4;
            TDBG("%s, IP Source = %x \n", __FUNCTION__, MsgPkt.Data[3]);
            break;
        case LAN_PARAM_IP_ADDRESS:
            _fmemcpy (&MsgPkt.Data[3], pSetNodeLanReq->NodeLanConfigData.IPv4Addr, IP_ADDR_LEN);
            MsgPkt.Size     = 7;
            TDBG("%s, IP Address = %d.%d.%d.%d \n", __FUNCTION__, 
                    MsgPkt.Data[3],
                    MsgPkt.Data[4],
                    MsgPkt.Data[5],
                    MsgPkt.Data[6]);
            break;
        case LAN_PARAM_SUBNET_MASK:
            _fmemcpy (&MsgPkt.Data[3], pSetNodeLanReq->NodeLanConfigData.SubNetMask, IP_ADDR_LEN);
            MsgPkt.Size     = 7;
            TDBG("%s, IP Subnet mask = %d.%d.%d.%d \n", __FUNCTION__, 
                    MsgPkt.Data[3],
                    MsgPkt.Data[4],
                    MsgPkt.Data[5],
                    MsgPkt.Data[6]);
            break;
        case LAN_PARAM_DEFAULT_GATEWAY_IP:
            _fmemcpy (&MsgPkt.Data[3], pSetNodeLanReq->NodeLanConfigData.DefaultGatewayIPAddr, IP_ADDR_LEN);
            MsgPkt.Size     = 7;
            TDBG("%s, IP Default Gateway IP = %d.%d.%d.%d \n", __FUNCTION__, 
                    MsgPkt.Data[3],
                    MsgPkt.Data[4],
                    MsgPkt.Data[5],
                    MsgPkt.Data[6]);
            break;
        default:
            printf("%s, do not support this Parameter by out handler \n", __FUNCTION__);
    }

    PostMsg (&MsgPkt, NODE_LAN_CFG_Q, BMCInst);

    return bRet;
}

#if 0
//INT8U LanTaskProcessing = 0;
    void*
NodeLanCfgTask (void *pArg)
{
    int *inst = (int*) pArg;
    int BMCInst = *inst;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    NodeLanCfgSharedMem_T*    pNodeLanCfgSharedMem;
    INT8U    		nNodeID, nChannelNum, nParameterSelect;
    int		NodePresent = -1;
    INT8U	pData[13];
    MsgPkt_T    MsgPkt;
    INT8U	bRet;

    //struct timeval t_ustart,t_uend;

    TDBG("zc test@NodeLanCfgTask entry! \n");
    pNodeLanCfgSharedMem = (_FAR_ NodeLanCfgSharedMem_T*)&pBMCInfo->NodeLanCfgSharedMem;

    while(1)
    {
        /* Wait for any messages */
        if (0 == GetMsg (&MsgPkt, NODE_LAN_CFG_Q, WAIT_INFINITE, BMCInst))
        {				
            switch (MsgPkt.Param)
            {
                case GET_LAN_CFG_ACTION:
                    TDBG("zc test@NodeLanCfgTask: Read \n");
                    nChannelNum = MsgPkt.Data [0];

                    //					LanTaskProcessing = 1;

                    //gettimeofday(&t_ustart, NULL);
                    //printf("%s, Get perpare time start: %ld s \n\n", __FUNCTION__, t_ustart.tv_sec);
                    //printf("%s, Get perpare time start: %ld us \n\n", __FUNCTION__, t_ustart.tv_usec);

                    OS_THREAD_MUTEX_ACQUIRE(&g_BMCInfo[BMCInst].NodeLanCfgSharedMemMutex, WAIT_INFINITE);
                    pNodeLanCfgSharedMem->ReadyToGet = 0;
                    OS_THREAD_MUTEX_RELEASE(&g_BMCInfo[BMCInst].NodeLanCfgSharedMemMutex);

                    for(nNodeID=0;nNodeID<=8;nNodeID++)
                    {	
#if 0				
                        if(g_PDKHandle[PDK_GETNODEPRESENT] != NULL)
                        {
                            NodePresent = ((int(*)(INT8U, int))g_PDKHandle[PDK_GETNODEPRESENT]) (nNodeID, BMCInst);
                        }
#endif
                        if (NodePresent)
                        {
                            pNodeLanCfgSharedMem->NodeStatus[nNodeID] = 1;
                            memset(pData, 0, 13);


                            //							gettimeofday(&t_ustart, NULL);
                            //							printf("%s, Get perpare time start: %ld s \n\n", __FUNCTION__, t_ustart.tv_sec);
                            //							printf("%s, Get perpare time start: %ld us \n\n", __FUNCTION__, t_ustart.tv_usec);
#if 0
                            if(g_PDKHandle[PDK_GETNODELANCONFIG] != NULL)
                            {
                                ((int(*)(int, INT8U*, INT8U, INT8U, INT8U))g_PDKHandle[PDK_GETNODELANCONFIG]) (BMCInst, pData, nNodeID, NODE_LAN_PARAMS_PET_OEM_GET_ALL_PARAMS, nChannelNum);
                                /* the structure returned by the Hook is
                                   INT8U               IPAddrSrc;
                                   INT8U               IPAddr [4];
                                   INT8U               SubNetMask [4];
                                   INT8U               DefaultGatewayIPAddr [IP_ADDR_LEN];
                                 */
                            }
#endif
                            //							LanTaskProcessing = 0;

                            OS_THREAD_MUTEX_ACQUIRE(&g_BMCInfo[BMCInst].NodeLanCfgSharedMemMutex, WAIT_INFINITE);

                            TDBG("zc test@NodeLanCfgTask:the node %x IP Address Source = %x \n", nNodeID, pData[0]);
                            pNodeLanCfgSharedMem->LanInfo[nNodeID].IPAddrSrc = pData[0];

                            TDBG("zc test@NodeLanCfgTask:the node %x IP address = %d.%d.%d.%d \n", nNodeID, pData[1], pData[2], pData[3], pData[4]);
                            _fmemcpy (pNodeLanCfgSharedMem->LanInfo[nNodeID].IPv4Addr, &pData[1], IP_ADDR_LEN);

                            TDBG("zc test@NodeLanCfgTask:the node %x Sub Net Mask address = %d.%d.%d.%d \n", nNodeID, pData[5], pData[6], pData[7], pData[8]);
                            _fmemcpy (pNodeLanCfgSharedMem->LanInfo[nNodeID].SubNetMask, &pData[5], IP_ADDR_LEN);

                            TDBG("zc test@NodeLanCfgTask:the node %x Default gateway IP address = %d.%d.%d.%d \n", nNodeID, pData[9], pData[10], pData[11], pData[12]);
                            _fmemcpy (pNodeLanCfgSharedMem->LanInfo[nNodeID].DefaultGatewayIPAddr, &pData[9], IP_ADDR_LEN);

                            OS_THREAD_MUTEX_RELEASE(&g_BMCInfo[BMCInst].NodeLanCfgSharedMemMutex);

                            //	Hrs, Mins, Secs, Millisecs
                            OS_TIME_DELAY_HMSM (0, 0, 0, 250);		// delay 250 Millisecs

                            //							gettimeofday(&t_uend, NULL);
                            //							printf("%s, Get perpare time End: %ld s \n\n", __FUNCTION__, t_uend.tv_sec);
                            //							printf("%s, Get perpare time End: %ld us \n\n", __FUNCTION__, t_uend.tv_usec);
                            //							printf("%s, the time for perpare Lan Cfg Param is %ld s \n", __FUNCTION__, (t_uend.tv_sec - t_ustart.tv_sec));
                            //							printf("%s, the time for perpare Lan Cfg Param is %ld us \n", __FUNCTION__, (t_uend.tv_usec - t_ustart.tv_usec));

                        }
                        else
                        {
                            OS_THREAD_MUTEX_ACQUIRE(&g_BMCInfo[BMCInst].NodeLanCfgSharedMemMutex, WAIT_INFINITE);
                            pNodeLanCfgSharedMem->NodeStatus[nNodeID] = 0;
                            OS_THREAD_MUTEX_RELEASE(&g_BMCInfo[BMCInst].NodeLanCfgSharedMemMutex);

                        }
                    }

                    OS_THREAD_MUTEX_ACQUIRE(&g_BMCInfo[BMCInst].NodeLanCfgSharedMemMutex, WAIT_INFINITE);
                    pNodeLanCfgSharedMem->ReadyToGet = 1;
                    OS_THREAD_MUTEX_RELEASE(&g_BMCInfo[BMCInst].NodeLanCfgSharedMemMutex);

                    //gettimeofday(&t_uend, NULL);
                    //printf("%s, Get perpare time End: %ld s \n\n", __FUNCTION__, t_uend.tv_sec);
                    //printf("%s, Get perpare time End: %ld us \n\n", __FUNCTION__, t_uend.tv_usec);
                    //printf("%s, the time for perpare Lan Cfg Param is %ld s \n", __FUNCTION__, (t_uend.tv_sec - t_ustart.tv_sec));

                    break;
                case SET_LAN_CFG_ACTION:
                    TDBG("zc test@NodeLanCfgTask: Write \n");
                    nChannelNum = MsgPkt.Data[0];
                    nNodeID = MsgPkt.Data[1];
                    nParameterSelect = MsgPkt.Data[2];

                    memset(pData, 0, 4);

                    //					OS_TIME_DELAY_HMSM (0, 0, 0, 500);		// delay 200 Millisecs
                    OS_THREAD_MUTEX_ACQUIRE(&g_BMCInfo[BMCInst].NodeLanCfgSharedMemMutex, WAIT_INFINITE);

                    switch (nParameterSelect)
                    {
                        case LAN_PARAM_SET_IN_PROGRESS:
                        case LAN_PARAM_IP_ADDRESS_SOURCE:
                            pData[0] = MsgPkt.Data[3];
                            //							pBMCInfo->NodeLanCfgSharedMem.LanInfo[nNodeID].IPAddrSrc = pData[0];
                            pNodeLanCfgSharedMem->LanInfo[nNodeID].IPAddrSrc = pData[0];
                            break;
                        case LAN_PARAM_IP_ADDRESS:
                            _fmemcpy (pData, &MsgPkt.Data[3], IP_ADDR_LEN);
                            //							_fmemcpy (pBMCInfo->NodeLanCfgSharedMem.LanInfo[nNodeID].IPv4Addr, pData, IP_ADDR_LEN);
                            _fmemcpy (pNodeLanCfgSharedMem->LanInfo[nNodeID].IPv4Addr, pData, IP_ADDR_LEN);
                            break;
                        case LAN_PARAM_SUBNET_MASK:
                            _fmemcpy (pData, &MsgPkt.Data[3], IP_ADDR_LEN);
                            //							_fmemcpy (pBMCInfo->NodeLanCfgSharedMem.LanInfo[nNodeID].SubNetMask, pData, IP_ADDR_LEN);
                            _fmemcpy (pNodeLanCfgSharedMem->LanInfo[nNodeID].SubNetMask, pData, IP_ADDR_LEN);
                            break;
                        case LAN_PARAM_DEFAULT_GATEWAY_IP:
                            _fmemcpy (pData, &MsgPkt.Data[3], IP_ADDR_LEN);
                            //							_fmemcpy (pBMCInfo->NodeLanCfgSharedMem.LanInfo[nNodeID].DefaultGatewayIPAddr, pData, IP_ADDR_LEN);
                            _fmemcpy (pNodeLanCfgSharedMem->LanInfo[nNodeID].DefaultGatewayIPAddr, pData, IP_ADDR_LEN);
                            break;
                        default:
                            printf("%s, no nothing here! \n", __FUNCTION__);
                    }
                    OS_THREAD_MUTEX_RELEASE(&g_BMCInfo[BMCInst].NodeLanCfgSharedMemMutex);

                    //					LanTaskProcessing = 1;
#if 0					
                    if(g_PDKHandle[PDK_SETNODELANCONFIG] != NULL)
                    {
                        bRet = ((INT8U(*)(int, INT8U*, INT8U, INT8U, INT8U))g_PDKHandle[PDK_SETNODELANCONFIG]) (BMCInst, pData, nNodeID, nParameterSelect, nChannelNum);
                    }
#endif
                    //					LanTaskProcessing = 0;
                    OS_TIME_DELAY_HMSM (0, 0, 0, 500);		// delay 500 Millisecs

                    //					FlushIPMI((INT8U*)&pBMCInfo->NodeLanCfgSharedMem,(INT8U*)&pBMCInfo->NodeLanCfgSharedMem,pBMCInfo->IPMIConfLoc.NodeLanCfgSharedMemAddr,
                    //                      sizeof(NodeLanCfgSharedMem_T),BMCInst);
                    break;
                default:
                    printf("%s, do noting! \n", __FUNCTION__);
            }
        }
    }

    return NULL;
}
#endif
