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
 * AppDevice.c
 * AppDevice Commands Handler
 *
 * Author: Govind Kothandapani <govindk@ami.com>
 *       : Rama Bisa <ramab@ami.com>
 *       : Basavaraj Astekar <basavaraja@ami.com>
 *       : Bakka Ravinder Reddy <bakkar@ami.com>
 *
 *****************************************************************/
#define ENABLE_DEBUG_MACROS  0
#include <defs.h>
#include "Types.h"
#include "Debug.h"
#include "IPMI_Main.h"
#include "SharedMem.h"
#include "Support.h"
#include "Message.h"
#include "IPMIDefs.h"
#include "MsgHndlr.h"
#include "IPMI_IPM.h"
#include "IPMI_AppDevice.h"
#include "AppDevice.h"
#include "RMCP.h"
#include "MD.h"
#include "LANIfc.h"
//#include "WDT.h"
//#include "NVRAccess.h"
#include "Util.h"
#include "libipmi_struct.h"
#include "nwcfg.h"
#include "Ethaddr.h"
#include "IPMIConf.h"
//#include "IPMBIfc.h"
//#include "IPMI_KCS.h"
//#include "ipmi_userifc.h"
//#include "hmac_md5.h"
//#include "hmac_sha1.h"
//#include "MD5_128.h"
//#include "Badpasswd.h"
//#include "hal_hw.h"
#include "iniparser.h"
#include "Session.h"
#include <linux/if.h>
#include "LANConfig.h"
#include <netdb.h>        /* getaddrinfo(3) et al.                       */
#include <netinet/in.h>   /* sockaddr_in & sockaddr_in6 definition.      */
#include "userprivilege.h"
#include <ctype.h>
//#include "PDKBridgeMsg.h"
#include "featuredef.h"
//#include "blowfish.h"

#define USER_ID_ENABLED 	0x01
#define USER_ID_DISABLED 	0x02
#define OP_USERID_ONLY_LENGTH    2
#define OP_ENABLE_USER_ID    	 1
#define OP_DISABLE_USER_ID    	 0
#define BIT3_BIT0_MASK     0xf
#define GET_AUTH_TYPE_MASK  0xc0
#define AUTH_TYPE_V15	0x0
#define AUTH_TYPE_V20	0x40
#define AUTH_CODE_V15_MASK  0x0f
#define AUTH_CODE_V15_1  0x1
#define AUTH_CODE_V15_2  0x2
#define AUTH_CODE_V15_3  0x5
#define AUTH_CODE_V20_MASK  0x3f
#define MIN_AUTH_CODE_V20 0x04
#define MAX_AUTH_CODE_V20 0xc0
#define NULL_USER                 1
#define ZERO_SETSELECTOR 0x00
#define MAX_TYPE_OF_ENCODING 0x02
#define MAX_STRING_LENGTH_COPY 14

/* Reserved bit macro definitions */
#define RESERVED_BITS_SENDMS 0x03 //(BIT1 | BIT0)

/* Auth code length */
#define HMAC_SHA1_96_LEN            12

#if APP_DEVICE == 1

#define COUNT_INCREASE  1
#define COUNT_DECREASE -1
#define MAX_BT_PKT_LEN 255

#define RESERVED_USERS_FILE "/etc/reservedusers"

/*** Global variables ***/
_FAR_   INT8U   g_TmrRunning;
/*** Module variables ***/
static  _FAR_   MsgPkt_T    m_MsgPkt; /**< Message Packet for posting and retrieving messaged to/from queue */
static INT8U m_Set_ChReserveBit[] ={0xF0,0x0,0x30};

extern IfcName_T Ifcnametable[MAX_LAN_CHANNELS];

/**
 * @fn CheckReservedUsers
 * @brief This function will checks for reserved users.
 * @param  Username - Username.
 * @retval availability of reserved users.
 */
static int CheckForReservedUsers(char *Username)
{
    dictionary *dict = NULL;
    char *sectionname = NULL;
    int nsec=0 , i=0;

    TDBG("filename is %s\n",RESERVED_USERS_FILE);
    dict = iniparser_load(RESERVED_USERS_FILE);
    if(dict == NULL)
    {
        TINFO("Unable to parse dummy users file :%s", RESERVED_USERS_FILE);
        return -1;
    }
    nsec = iniparser_getnsec(dict);
    for (i=0;i<nsec;i++)
    {
        sectionname = iniparser_getsecname (dict, i);
        if(NULL == sectionname)
        {
            TINFO("Unable to get setion name of dummy users file :%s",RESERVED_USERS_FILE);
            iniparser_freedict(dict);
            return -1;
        }
        if(strcmp(sectionname, Username)==0)
        {
            iniparser_freedict(dict);
            return -1;
        }
    }
    iniparser_freedict(dict);
    return 0;
}

static void UpdateCurrentEnabledUserCount(int value, int bmcInstId)
{
    if (value == 0) return;

    _FAR_ ChannelInfo_T* pChannelInfo = NULL;
    _FAR_ ChannelInfo_T* pNvrChannelInfo = NULL;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[bmcInstId];
    INT8U maxUserCount = pBMCInfo->IpmiConfig.MaxChUsers;
    INT8U channelIndex = 0;


    for (channelIndex = 0; channelIndex < MAX_NUM_CHANNELS; channelIndex++)
    {
        if (pBMCInfo->ChConfig[channelIndex].ChType == 0xff) continue;

        pChannelInfo = (ChannelInfo_T*)&pBMCInfo->ChConfig[channelIndex].ChannelInfo;

        if (((value > 0) && ((pChannelInfo->NoCurrentUser + value) > maxUserCount)) ||
                ((value < 0) && ((pChannelInfo->NoCurrentUser + value) < 0)))
        {
            continue;
        }

        pNvrChannelInfo = GetNVRChConfigs(pChannelInfo, bmcInstId);

        pChannelInfo->NoCurrentUser+=value;
        pNvrChannelInfo->NoCurrentUser+=value;
        //              FlushChConfigs((INT8U*)pNvrChannelInfo,pNvrChannelInfo->ChannelNumber,bmcInstId);
    }
}

static int IsPrivilegeAvailable(INT8U requestedPrivilege, INT8U channelNumber, int bmcInstId)
{
    //_FAR_   PMConfig_T* pPMConfig = (_FAR_ PMConfig_T*) GetNVRAddr(NVRH_PMCONFIG, bmcInstId);
    BMCInfo_t *pBMCInfo = &g_BMCInfo[bmcInstId];
    INT8U EthIndex = GetEthIndex(channelNumber & 0x0F, bmcInstId);
    INT8U privilege = 0x00;

    if(channelNumber != g_BMCInfo[bmcInstId].SERIALch){
        /* LAN IFC */
        if(0xff == EthIndex) return -1;

        //Get requested privilege status (enabled or disabled) from PMConfig by channel
        switch (requestedPrivilege)
        {
            case PRIV_CALLBACK:
                privilege = pBMCInfo->LANCfs[EthIndex].AuthTypeEnables.AuthTypeCallBack;
                break;

            case PRIV_USER:
                privilege = pBMCInfo->LANCfs[EthIndex].AuthTypeEnables.AuthTypeUser;
                break;

            case PRIV_OPERATOR:
                privilege = pBMCInfo->LANCfs[EthIndex].AuthTypeEnables.AuthTypeOperator;
                break;

            case PRIV_ADMIN:
                privilege = pBMCInfo->LANCfs[EthIndex].AuthTypeEnables.AuthTypeAdmin;
                break;

            case PRIV_OEM:
                privilege = pBMCInfo->LANCfs[EthIndex].AuthTypeEnables.AuthTypeOem;
                break;

            default:
                return -1;
        }
    }else{
        /* Serial IFC */
        //Get requested privilege status (enabled or disabled) from PMConfig by channel
        switch (requestedPrivilege)
        {
            case PRIV_CALLBACK:
                privilege = pBMCInfo->SMConfig.AuthTypeEnable.Callback;
                break;

            case PRIV_USER:
                privilege = pBMCInfo->SMConfig.AuthTypeEnable.User;
                break;

            case PRIV_OPERATOR:
                privilege = pBMCInfo->SMConfig.AuthTypeEnable.Operator;
                break;

            case PRIV_ADMIN:
                privilege = pBMCInfo->SMConfig.AuthTypeEnable.Admin;
                break;

            case PRIV_OEM:
                privilege = pBMCInfo->SMConfig.AuthTypeEnable.oem;
                break;

            default:
                return -1;
        }

    }

    //All bits are 0 that means privilege level is disabled
    return ((privilege != 0x00) ? 0 : -1);
}

/*-------------------------------------
 * ValidateIPMBChksum1
 *-------------------------------------*/
/* Function to validate IPMB Checksum1 for SendMessage Cmd */
static int ValidateIPMBChksum1(_NEAR_ INT8U* Data)
{
    int i=0;
    INT8U chksum=0;

    for (i = 0; i < 3; i++)
    {
        chksum += *(Data + i);
    }

    if (chksum != 0)
    {
        return FALSE;
    }
    return TRUE;

}

#if 0
/*-------------------------------------
 * ResetWDT
 *-------------------------------------*/
    int
ResetWDT (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    INT8U	u8ExpirationFlag;
    BMCInfo_t *pBMCInfo = &g_BMCInfo[BMCInst];


    if (pBMCInfo->Msghndlr.TmrSet == FALSE)
    {
        *pRes = CC_ATTEMPT_TO_RESET_UNIN_WATCHDOG;
        return sizeof (*pRes);
    }

    // save the WDT expiration flag for later use
    u8ExpirationFlag = g_WDTTmrMgr.WDTTmr.ExpirationFlag;


    /* Reset of Watchdog should not happen once
       once pretimeout interrupt interval is reached*/
    if(pBMCInfo->WDTPreTmtStat == TRUE)
    {
        *pRes = CC_PARAM_NOT_SUP_IN_CUR_STATE;
        return sizeof (*pRes);
    }

    g_WDTTmrMgr.TmrPresent  = TRUE;
    g_WDTTmrMgr.TmrInterval = pBMCInfo->WDTConfig.InitCountDown;
    g_WDTTmrMgr.PreTimeOutInterval = SEC_TO_MS * pBMCInfo->WDTConfig.PreTimeOutInterval;

    /* if the pre-timeout interrupt is not configured, adjust the pre-timeout interrupt
       timeout value beyound the regular WDT timeout value so that it won't get triggered
       before the WDT timeout. */
    if ((pBMCInfo->WDTConfig.TmrActions & 0x70) == 0)
    {
        g_WDTTmrMgr.PreTimeOutInterval = g_WDTTmrMgr.TmrInterval+ 1;
    }

    _fmemcpy (&g_WDTTmrMgr.WDTTmr, &pBMCInfo->WDTConfig, sizeof (WDTConfig_T));

    // restore the WDT expiration flag, don't use the one from the flash
    g_WDTTmrMgr.WDTTmr.ExpirationFlag = u8ExpirationFlag;

    // clear WDT sensor event history
    if( g_corefeatures.internal_sensor == ENABLED )
        RestartWD2Sensor(BMCInst);

    FlushIPMI((INT8U*)&pBMCInfo->WDTConfig,(INT8U*)&pBMCInfo->WDTConfig,pBMCInfo->IPMIConfLoc.WDTDATAddr,
            sizeof(WDTConfig_T),BMCInst);

    if(BMC_GET_SHARED_MEM(BMCInst)->IsWDTPresent == FALSE)
    {
        LOCK_BMC_SHARED_MEM(BMCInst);
        BMC_GET_SHARED_MEM(BMCInst)->IsWDTRunning=TRUE;
        BMC_GET_SHARED_MEM(BMCInst)->IsWDTPresent=TRUE;
        UNLOCK_BMC_SHARED_MEM(BMCInst);
        sem_post(&g_BMCInfo[BMCInst].WDTSem);
    }
    else
    {
        LOCK_BMC_SHARED_MEM(BMCInst);
        BMC_GET_SHARED_MEM(BMCInst)->IsWDTRunning=TRUE;
        BMC_GET_SHARED_MEM(BMCInst)->IsWDTPresent=TRUE;
        UNLOCK_BMC_SHARED_MEM(BMCInst);
        //Set SetWDTUpdated flag to reload initial countdown value.
        g_BMCInfo[BMCInst].SetWDTUpdated = TRUE;
    }


    *pRes = CC_NORMAL;

    return sizeof (*pRes);
}



/*---------------------------------------
 * SetWDT
 *---------------------------------------*/
    int
SetWDT (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  SetWDTReq_T*    pSetWDTReq = (_NEAR_ SetWDTReq_T*)pReq;
#if GET_MSG_FLAGS != UNIMPLEMENTED
    GetMsgFlagsRes_T   GetMsgFlagsRes;
#endif
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];


    //Check for Reserved bits
    if((pSetWDTReq->TmrUse & (BIT5 | BIT4 | BIT3)) || !(pSetWDTReq->TmrUse & (BIT2 | BIT1 | BIT0)) || ((pSetWDTReq->TmrUse & (BIT1 | BIT2)) == (BIT1 | BIT2)) ||
            (pSetWDTReq->TmrActions & (BIT7 |BIT6 | BIT3 | BIT2)) || (pSetWDTReq->ExpirationFlag & (BIT7 | BIT6 | BIT0)))
    {
        *pRes = CC_INV_DATA_FIELD;
        return sizeof(*pRes);
    }

#if NO_WDT_PRETIMEOUT_INTERRUPT == 1
    // do not support pre-timeout interrupt
    if (pSetWDTReq->TmrActions & 0x70)
    {
        *pRes = CC_INV_DATA_FIELD;
        return sizeof(*pRes);
    }
#endif

    pSetWDTReq->InitCountDown = htoipmi_u16(pSetWDTReq->InitCountDown);

    // error out if the pre-timeout interrupt is greater than the initial countdown value
    if (pSetWDTReq->InitCountDown < 10 * pSetWDTReq->PreTimeOutInterval)
    {
        *pRes = CC_INV_DATA_FIELD;
        return sizeof(*pRes);
    }

    // only clear the memory version of the bit(s) when the input bit is set #31175
    g_WDTTmrMgr.WDTTmr.ExpirationFlag &= ~pSetWDTReq->ExpirationFlag;
    pSetWDTReq->ExpirationFlag = g_WDTTmrMgr.WDTTmr.ExpirationFlag;


    /* Copy the Timer configuration in NVRAM */
    LOCK_BMC_SHARED_MEM(BMCInst);
    _fmemcpy ((_FAR_ INT8U*)&pBMCInfo->WDTConfig, (_FAR_ INT8U*)pSetWDTReq, sizeof (WDTConfig_T));
    UNLOCK_BMC_SHARED_MEM(BMCInst);

    if (TRUE ==BMC_GET_SHARED_MEM(BMCInst)->IsWDTRunning)
    {
        /* To check wheather Dont stop bit is set or not */
        if (pSetWDTReq->TmrUse & 0x40)
        {
            /* Set the count down value to given value */
            g_WDTTmrMgr.TmrPresent = TRUE;
            LOCK_BMC_SHARED_MEM(BMCInst);
            BMC_GET_SHARED_MEM(BMCInst)->IsWDTPresent =TRUE;
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            g_WDTTmrMgr.TmrInterval= pSetWDTReq->InitCountDown;
            g_WDTTmrMgr.PreTimeOutInterval = (SEC_TO_MS * pSetWDTReq->PreTimeOutInterval);

            /* If PreTimeOutInt is set, clear it */
            if (0 != (pSetWDTReq->TmrActions & 0x70))
            {
                pSetWDTReq->TmrActions &= ~0x70;
            }
            else
            {
                // if the pre-timeout interrupt is not configured, adjust the pre-timeout interrupt
                // timeout value beyound the regular WDT timeout value so that it won't get triggered
                // before the WDT timeout.
                g_WDTTmrMgr.PreTimeOutInterval = pSetWDTReq->InitCountDown + 1;
            }
            _fmemcpy (&g_WDTTmrMgr.WDTTmr, pSetWDTReq, sizeof (WDTConfig_T ));

        }
        else
        {
            /* Stop the timer */
            g_WDTTmrMgr.TmrPresent = FALSE;
            LOCK_BMC_SHARED_MEM(BMCInst);
            BMC_GET_SHARED_MEM(BMCInst)->IsWDTRunning=FALSE;
            BMC_GET_SHARED_MEM(BMCInst)->IsWDTPresent =FALSE;
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            g_WDTTmrMgr.TmrInterval= pSetWDTReq->InitCountDown;
            g_WDTTmrMgr.PreTimeOutInterval = SEC_TO_MS * pSetWDTReq->PreTimeOutInterval;
            // clear WDT sensor event history
            if( g_corefeatures.internal_sensor == ENABLED)
                RestartWD2Sensor(BMCInst);
        }

        /* Clear the  pre-timeout interupt flag */
        LOCK_BMC_SHARED_MEM(BMCInst);
        pBMCInfo->WDTConfig.PreTimeoutActionTaken = 0x00;
        BMC_GET_SHARED_MEM (BMCInst)->MsgFlags &= ~0x08; /* Clear the flag */
#if GET_MSG_FLAGS != UNIMPLEMENTED
        // Clear SMS_ATN bit if and only if the Get Message Flag return 0 in byte 2.
        GetMsgFlags (NULL, 0, (INT8U *)&GetMsgFlagsRes,BMCInst);
        TDBG("GetMsgFlagsRes.CompletionCode : %X, GetMsgFlagsRes.MsgFlags : %X\n",
                GetMsgFlagsRes.CompletionCode, GetMsgFlagsRes.MsgFlags);
        if (GetMsgFlagsRes.CompletionCode == CC_NORMAL && GetMsgFlagsRes.MsgFlags == 0)
#else
            if((BMC_GET_SHARED_MEM(BMCInst)->MsgFlags & BIT3_BIT0_MASK) == 0)
#endif
            {
                /* Clear the SMS_ATN bit */
                if (pBMCInfo->IpmiConfig.KCS1IfcSupport == 1)
                {
                    CLEAR_SMS_ATN (0, BMCInst);
                }
                if (pBMCInfo->IpmiConfig.KCS2IfcSupport == 1)
                {
                    CLEAR_SMS_ATN (1, BMCInst);
                }
                if (pBMCInfo->IpmiConfig.KCS3IfcSuppport == 1)
                {
                    CLEAR_SMS_ATN (2, BMCInst);
                }
            }
        UNLOCK_BMC_SHARED_MEM(BMCInst);

    }
    else
    {
        g_WDTTmrMgr.TmrInterval = pSetWDTReq->InitCountDown;
        g_WDTTmrMgr.TmrPresent = FALSE;
        LOCK_BMC_SHARED_MEM(BMCInst);
        BMC_GET_SHARED_MEM(BMCInst)->IsWDTPresent =FALSE;
        UNLOCK_BMC_SHARED_MEM(BMCInst);
        // clear WDT sensor event history
        if( g_corefeatures.internal_sensor == ENABLED)
            RestartWD2Sensor(BMCInst);
    }

    // Modify ARP status to resume the thread
    // after receiving set Watchdog Timer command
    //BMC_GET_SHARED_MEM(BMCInst)->GratArpStatus = RESUME_ARPS;

    int i = 0;

    for (i = 0; i < MAX_LAN_CHANNELS; i++)
    {
        if((pBMCInfo->LANConfig.LanIfcConfig[i].Enabled == TRUE)
                && (pBMCInfo->LANConfig.LanIfcConfig[i].Up_Status == LAN_IFC_UP))
        {
            BMC_GET_SHARED_MEM(BMCInst)->ArpSuspendStatus[i] = RESUME_ARPS;
            UpdateArpStatus(i, BMC_GET_SHARED_MEM(BMCInst)->IsWDTRunning, BMCInst);
        }
    }

    FlushIPMI((INT8U*)&pBMCInfo->WDTConfig,(INT8U*)&pBMCInfo->WDTConfig,pBMCInfo->IPMIConfLoc.WDTDATAddr,
            sizeof(WDTConfig_T),BMCInst);
    // set the "Don't Log" bit
    g_WDTTmrMgr.WDTTmr.TmrUse &= 0x7F;
    g_WDTTmrMgr.WDTTmr.TmrUse |= (pSetWDTReq->TmrUse & 0x80);

    g_BMCInfo[BMCInst].SetWDTUpdated = TRUE;
    g_BMCInfo[BMCInst].Msghndlr.TmrSet = TRUE;

    *pRes = CC_NORMAL;

    return sizeof (*pRes);
}


/*---------------------------------------
 * GetWDT
 *---------------------------------------*/
    int
GetWDT (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  GetWDTRes_T*    pGetWDTRes = (_NEAR_ GetWDTRes_T*)pRes;
    BMCInfo_t *pBMCInfo = &g_BMCInfo[BMCInst];

    /* Copy the current settings from the NVRAM */
    LOCK_BMC_SHARED_MEM(BMCInst);
    _fmemcpy ((_FAR_ INT8U*)&pGetWDTRes->CurrentSettings,
            (_FAR_ INT8U*)&pBMCInfo->WDTConfig, sizeof (WDTConfig_T));
    UNLOCK_BMC_SHARED_MEM(BMCInst);

    // get the WDT expiration from the global veriable in memory, not from the flash
    pGetWDTRes->CurrentSettings.ExpirationFlag = g_WDTTmrMgr.WDTTmr.ExpirationFlag;

    // get the current "Don't Log" bit
    pGetWDTRes->CurrentSettings.TmrUse &= 0x7F;
    pGetWDTRes->CurrentSettings.TmrUse |= (g_WDTTmrMgr.WDTTmr.TmrUse & 0x80);
    if (TRUE == BMC_GET_SHARED_MEM(BMCInst)->IsWDTPresent)
    {
        // set the WDT running bit #30235/30467
        pGetWDTRes->CurrentSettings.TmrUse |= 0x40;
        /* Present count down in 1/100 of second */
    }
    else
    {
        // clear the WDT running bit #30235/30467  for Timer Use (ie) WatchDog Timer status
        pGetWDTRes->CurrentSettings.TmrUse &= ~0x40;
        pGetWDTRes->CurrentSettings.ExpirationFlag = (pGetWDTRes->CurrentSettings.ExpirationFlag) & 0x3E;
    }

    pGetWDTRes->PresentCountDown			   = g_WDTTmrMgr.TmrInterval;
    pGetWDTRes->CurrentSettings.InitCountDown = htoipmi_u16(pGetWDTRes->CurrentSettings.InitCountDown);
    pGetWDTRes->CompletionCode                = CC_NORMAL;

    return sizeof (GetWDTRes_T);
}
#endif

/*---------------------------------------
 * SetBMCGlobalEnables
 *---------------------------------------*/
    int
SetBMCGlobalEnables (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    INT8U GblEnblByte = *pReq;
    MsgPkt_T MsgPkt;

    _fmemset (&MsgPkt, 0, sizeof (MsgPkt_T));

    /* Check For the reserved bit 4 */
    if ( GblEnblByte & BIT4)
    {
        *pRes = CC_INV_DATA_FIELD;
        return sizeof (*pRes);
    }

    if (((BMC_GET_SHARED_MEM (BMCInst)->GlobalEnables ^ GblEnblByte)) & 0x20)
    {
        /* OEM 0 puts us in ICTS compatibility mode for IPMIv2,
         * Send a message to lan process so it can change behavior
         */
        MsgPkt.Channel    = GetLANChannel(0, BMCInst);
        MsgPkt.Param      = LAN_ICTS_MODE;
        MsgPkt.Privilege  = PRIV_LOCAL;
        if (GblEnblByte & 0x20)
            MsgPkt.Cmd = 1;
        else
            MsgPkt.Cmd = 0;
        PostMsg(&MsgPkt,LAN_IFC_Q,BMCInst);
    }

    BMC_GET_SHARED_MEM (BMCInst)->GlobalEnables = GblEnblByte;
    *pRes = CC_NORMAL;

    return sizeof (*pRes);
}


/*---------------------------------------
 * GetBMCGlobalEnables
 *---------------------------------------*/
    int
GetBMCGlobalEnables (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  GetBMCGblEnblRes_T* pGetBMCGblEnblRes = (_NEAR_ GetBMCGblEnblRes_T*)pRes;

    pGetBMCGblEnblRes->CompletionCode = CC_NORMAL;
    pGetBMCGblEnblRes->BMCGblEnblByte = BMC_GET_SHARED_MEM (BMCInst)->GlobalEnables;

    return sizeof (GetBMCGblEnblRes_T);
}


/*---------------------------------------
 * ClrMsgFlags
 *---------------------------------------*/
    int
ClrMsgFlags (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_ ClearMsgsFlagReq_T* pClearMsgsFlagReq = (_NEAR_ ClearMsgsFlagReq_T*)pReq;
#if GET_MSG_FLAGS != UNIMPLEMENTED
    GetMsgFlagsRes_T   GetMsgFlagsRes;
#endif
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    //Check for Reserved bits
    if(pClearMsgsFlagReq->Flag & (BIT4 | BIT2))
    {
        *pRes = CC_INV_DATA_FIELD;
        return sizeof(*pRes);
    }

    /* Flush Receive Message Queue */
    if (0 != (pClearMsgsFlagReq->Flag & 0x01))
    {
        while (0 == GetMsg (&m_MsgPkt, &g_RcvMsgQ[g_BMCInfo[BMCInst].Msghndlr.CurKCSIfcNum][0], WAIT_NONE,BMCInst))
        {
            BMC_GET_SHARED_MEM (BMCInst)->NumRcvMsg[g_BMCInfo[BMCInst].Msghndlr.CurKCSIfcNum]--;
        }

        BMC_GET_SHARED_MEM (BMCInst)->MsgFlags &= ~0x01; /* Clear the flag */
    }

    /* Flush Event Message Buffer */
    if (0 != (pClearMsgsFlagReq->Flag & 0x02))
    {
        while (0 == GetMsg (&m_MsgPkt, EVT_MSG_Q, WAIT_NONE,BMCInst))
        {
            BMC_GET_SHARED_MEM (BMCInst)->NumEvtMsg--;
        }

        BMC_GET_SHARED_MEM (BMCInst)->MsgFlags &= ~0x02; /* Clear the flag */
    }

    /* Clear WatchdogTimer Interrupt*/
    if (0 != (pClearMsgsFlagReq->Flag & 0x08))
    {
        /* Clear the  pre-timeout interupt flag */
        pBMCInfo->WDTConfig.PreTimeoutActionTaken = 0x00;
        BMC_GET_SHARED_MEM (BMCInst)->MsgFlags &= ~0x08; /* Clear the flag */
        //        FlushIPMI((INT8U*)&pBMCInfo->WDTConfig,(INT8U*)&pBMCInfo->WDTConfig,pBMCInfo->IPMIConfLoc.WDTDATAddr,
        //                           sizeof(WDTConfig_T),BMCInst);

    }

#if GET_MSG_FLAGS != UNIMPLEMENTED
    // Clear SMS_ATN bit if and only if the Get Message Flag return 0 in byte 2.
    GetMsgFlags (NULL, 0, (INT8U *)&GetMsgFlagsRes,BMCInst);
    TDBG("GetMsgFlagsRes.CompletionCode : %X, GetMsgFlagsRes.MsgFlags : %X\n",
            GetMsgFlagsRes.CompletionCode, GetMsgFlagsRes.MsgFlags);
    if (GetMsgFlagsRes.CompletionCode == CC_NORMAL && GetMsgFlagsRes.MsgFlags == 0)
#else
        if((BMC_GET_SHARED_MEM(BMCInst)->MsgFlags & BIT3_BIT0_MASK) == 0)
#endif
        {
#if 0
            /* Clear the SMS_ATN bit */
            if (pBMCInfo->IpmiConfig.KCS1IfcSupport == 1)
            {
                CLEAR_SMS_ATN (0, BMCInst);
            }
            if (pBMCInfo->IpmiConfig.KCS2IfcSupport == 1)
            {
                CLEAR_SMS_ATN (1, BMCInst);
            }
            if (pBMCInfo->IpmiConfig.KCS3IfcSuppport == 1)
            {
                CLEAR_SMS_ATN (2, BMCInst);
            }
#endif
        }

    *pRes = CC_NORMAL;

    return sizeof (*pRes);
}


/*---------------------------------------
  GetMsgFlags
  ---------------------------------------*/
    int
GetMsgFlags (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  GetMsgFlagsRes_T*   pGetMsgFlagsRes = (_NEAR_ GetMsgFlagsRes_T*)pRes;
    BMCInfo_t *pBMCInfo = &g_BMCInfo[BMCInst];

    /* get the message flags */
    pGetMsgFlagsRes->MsgFlags = BMC_GET_SHARED_MEM (BMCInst)->MsgFlags;

    if (BMC_GET_SHARED_MEM (BMCInst)->NumEvtMsg >= EVT_MSG_BUF_SIZE)
    {
        /* If Event MessageBuffer is Full set the BIT */
        pGetMsgFlagsRes->MsgFlags |= 0x02;
    }
    else
    {
        /* else reset the Flag */
        pGetMsgFlagsRes->MsgFlags &= ~0x02;
    }

    if(0 != BMC_GET_SHARED_MEM (BMCInst)->NumRcvMsg[g_BMCInfo[BMCInst].Msghndlr.CurKCSIfcNum])
    {
        /* if any Message in ReceiveMsgQ set the Flag */
        pGetMsgFlagsRes->MsgFlags |= 0x01;
    }
    else
    {
        /* else reset the Flag */
        pGetMsgFlagsRes->MsgFlags &= ~0x01;
    }

    /* get the  Pre-Timeout Bits Value & Set it to Response Data */
    //PRETIMEOUT BIT is 3rd bit so changed accordingly
    pGetMsgFlagsRes->MsgFlags |= (pBMCInfo->WDTConfig.PreTimeoutActionTaken & 0x08);

    /* Update the Message flags in shared Mem */
    BMC_GET_SHARED_MEM (BMCInst)->MsgFlags |=  pGetMsgFlagsRes->MsgFlags;
    pGetMsgFlagsRes->CompletionCode = CC_NORMAL;

    return sizeof (GetMsgFlagsRes_T);
}


/*---------------------------------------
 * EnblMsgChannelRcv
 *---------------------------------------*/
    int
EnblMsgChannelRcv (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  EnblMsgChRcvReq_T*  pEnblMsgChRcvReq = (_NEAR_ EnblMsgChRcvReq_T*)pReq;
    _NEAR_  EnblMsgChRcvRes_T*  pEnblMsgChRcvRes = (_NEAR_ EnblMsgChRcvRes_T*)pRes;
    _FAR_   ChannelInfo_T*      pChannelInfo;

    //Check for Reserved bits
    if(pEnblMsgChRcvReq->ChannelNum & (BIT7 | BIT6 | BIT5 | BIT4))
    {
        pEnblMsgChRcvRes->CompletionCode = CC_INV_DATA_FIELD;
        return sizeof(*pRes);
    }

    pChannelInfo = getChannelInfo (pEnblMsgChRcvReq->ChannelNum & 0x0F, BMCInst);

    TDBG ("ENBL_MSG_CH_RCV: processing..\n");

    if (NULL == pChannelInfo)
    {
        pEnblMsgChRcvRes->CompletionCode = CC_INV_DATA_FIELD;
        return sizeof (*pRes);
    }

    switch (pEnblMsgChRcvReq->ChannelState)
    {
        case 0:
            /* disable Receive Message Queue for this Channel */
            pChannelInfo->ReceiveMsgQ = 0x0;
            break;

        case 1:
            /*enable Recevive Message Queue for this Channel */
            pChannelInfo->ReceiveMsgQ = 0x1;
            break;

        case 2:
            /*get Channel State */
            pEnblMsgChRcvRes->ChannelState = pChannelInfo->ReceiveMsgQ;
            break;

        default:
            pEnblMsgChRcvRes->CompletionCode = CC_INV_DATA_FIELD;
            return sizeof (*pRes);
    }

    pEnblMsgChRcvRes->CompletionCode = CC_NORMAL;

    /*get Channel Number */
    pEnblMsgChRcvRes->ChannelNum = pEnblMsgChRcvReq->ChannelNum & 0x0F;

    pEnblMsgChRcvRes->ChannelState = pChannelInfo->ReceiveMsgQ;

    return sizeof (EnblMsgChRcvRes_T);
}


/*---------------------------------------
 * GetMessage
 *---------------------------------------*/
    int
GetMessage (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  GetMsgRes_T*    pGetMsgRes = (_NEAR_ GetMsgRes_T*)pRes;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];        INT8U           Index = 0;
#if GET_MSG_FLAGS != UNIMPLEMENTED
    GetMsgFlagsRes_T   GetMsgFlagsRes;
#endif

    if (0 != GetMsg (&m_MsgPkt, &g_RcvMsgQ[g_BMCInfo[BMCInst].Msghndlr.CurKCSIfcNum][0], WAIT_NONE,BMCInst))
    {
        /* if Queue is Empty */
        pGetMsgRes->CompletionCode = CC_GET_MSG_QUEUE_EMPTY;
        return  sizeof (*pRes);
    }

    BMC_GET_SHARED_MEM (BMCInst)->NumRcvMsg[g_BMCInfo[BMCInst].Msghndlr.CurKCSIfcNum]--;

#if GET_MSG_FLAGS != UNIMPLEMENTED
    // Clear SMS_ATN bit if and only if the Get Message Flag return 0 in byte 2.
    GetMsgFlags (NULL, 0, (INT8U *)&GetMsgFlagsRes,BMCInst);
    TDBG("GetMsgFlagsRes.CompletionCode : %X, GetMsgFlagsRes.MsgFlags : %X\n",
            GetMsgFlagsRes.CompletionCode, GetMsgFlagsRes.MsgFlags);
    if (GetMsgFlagsRes.CompletionCode == CC_NORMAL && GetMsgFlagsRes.MsgFlags == 0)
#else
        if (0 == BMC_GET_SHARED_MEM (BMCInst)->NumRcvMsg[g_BMCInfo[BMCInst].Msghndlr.CurKCSIfcNum])
#endif
        {
            /* Clear the SMS_ATN bit */
            //        CLEAR_SMS_ATN (g_BMCInfo[BMCInst].Msghndlr.CurKCSIfcNum,BMCInst);
            ;
        }

    /* Completion Code  */
    pGetMsgRes->CompletionCode  = CC_NORMAL;
    /* Channel number and privilege level */
    pGetMsgRes->ChannelNum      = m_MsgPkt.Channel;

    Index = sizeof (GetMsgRes_T);

    /* First byte should be session handle */
    if(pBMCInfo->IpmiConfig.SecondaryIPMBSupport == 0x01)
    {
        if ((PRIMARY_IPMB_CHANNEL != m_MsgPkt.Channel) && (pBMCInfo->SecondaryIPMBCh != m_MsgPkt.Channel))
        {
            pGetMsgRes->ChannelNum |= m_MsgPkt.Privilege << 0x04;
            pRes [Index++] = m_MsgPkt.Param;
        }
    }
    else
    {
        if(PRIMARY_IPMB_CHANNEL != m_MsgPkt.Channel)
        {
            pGetMsgRes->ChannelNum |= m_MsgPkt.Privilege << 0x04;
            pRes [Index++] = m_MsgPkt.Param;
        }
    }

    /* copy the Message data    */
    _fmemcpy ((_FAR_ INT8U*)&pRes[Index], &m_MsgPkt.Data[1], m_MsgPkt.Size-1);

    IPMI_DBG_PRINT ("GetMsg: Sending the following data through requested channel\n");
    IPMI_DBG_PRINT_BUF (pRes, m_MsgPkt.Size + Index);

    return  ((m_MsgPkt.Size-1)+ Index);      /*+ 2 for completion code & channel No. */
}


/*---------------------------------------
 * SendMessage
 *---------------------------------------*/
    int
SendMessage (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  SendMsgReq_T* pSendMsgReq = (_NEAR_ SendMsgReq_T*)pReq;
    _NEAR_  SendMsgRes_T* pSendMsgRes = (_NEAR_ SendMsgRes_T*)pRes;
    _NEAR_  IPMIMsgHdr_T* pIPMIMsgHdr;
    char QueueName[MAX_STR_LENGTH];
    _FAR_  BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    INT8U         Tracking;
    INT8U         Channel=0;
    INT8U         ResLen = 1;
    INT8U         RetVal = 0;
    INT8U         Offset = 1;
    INT8U         i;
    INT8U         SrcSessionHndl = 0;
    _FAR_   ChannelInfo_T*      pChannelInfo;

    if (ReqLen < 1)
    {
        *pRes = CC_REQ_INV_LEN;
        return  sizeof (*pRes);
    }

    if(pSendMsgReq->ChNoTrackReq == 0xC0)
    {
        *pRes = CC_INV_DATA_FIELD;
        return sizeof (*pRes);
    }

    /* Get the channel number */
    Channel = pSendMsgReq->ChNoTrackReq & 0x0F;

    /* Get Tracking field */
    Tracking = pSendMsgReq->ChNoTrackReq >> 6;

    if (Tracking == RESERVED_BITS_SENDMS)
    {
        *pRes = CC_INV_DATA_FIELD;
        return sizeof (*pRes);
    }

    m_MsgPkt.Param    = BRIDGING_REQUEST;
    m_MsgPkt.Channel  = Channel;
    m_MsgPkt.Size     = ReqLen - 1; /* -1 to skip channel num */

    /* Copy the message data */
    _fmemcpy (m_MsgPkt.Data, &pReq[1], m_MsgPkt.Size);
    /* Copy the IPMI Message header */
    pIPMIMsgHdr =  (_NEAR_ IPMIMsgHdr_T*)&m_MsgPkt.Data[Offset - 1];
    if(ValidateIPMBChksum1((_NEAR_ INT8U*)pIPMIMsgHdr) == FALSE)
    {
        *pRes = CC_INV_DATA_FIELD;
        return  sizeof (*pRes);
    }

    if(m_MsgPkt.Data[ReqLen - 2] != CalculateCheckSum2 ((_FAR_ INT8U*)pIPMIMsgHdr, ReqLen - Offset - 1))
    {
        *pRes = CC_INV_DATA_FIELD;
        return  sizeof (*pRes);
    }

#if 0
    printf("SendMsg: ReqLen = %d, size = %ld\n",ReqLen,m_MsgPkt.Size);

    for(i = 0; i < m_MsgPkt.Size;i++)
        printf("MsgPkt_Data %02X\n",m_MsgPkt.Data[i]);
    printf("\n");
#endif

    if(pBMCInfo->IpmiConfig.LANIfcSupport == 1)
    {
        /* To Check the Wheather  LAN Channel */
        if (IsLANChannel(pBMCInfo->Msghndlr.CurChannel  & 0xF, BMCInst))
        {
            _FAR_ SessionInfo_T* pSessionInfo = getSessionInfo (SESSION_ID_INFO, &g_BMCInfo[BMCInst].Msghndlr.CurSessionID,BMCInst);

            if (NULL != pSessionInfo)
            {
                SrcSessionHndl = pSessionInfo->SessionHandle;
            }

            TDBG ("SendMsg: To LAN Interface for reference\n");
            // Offset++; : causes bridging issues
            strcpy ((char *)m_MsgPkt.SrcQ, LAN_IFC_Q);
        }
    }

    if((Channel == PRIMARY_IPMB_CHANNEL) && pBMCInfo->IpmiConfig.PrimaryIPMBSupport == 1)
    {
        TDBG ("SendMsg: To Primary IPMB Interface\n");
        strcpy ((char *)m_MsgPkt.SrcQ, IPMB_PRIMARY_IFC_Q);
    }
    else if((pBMCInfo->SecondaryIPMBCh != CH_NOT_USED && Channel == pBMCInfo->SecondaryIPMBCh) && pBMCInfo->IpmiConfig.SecondaryIPMBSupport == 1)
    {
        TDBG ("SendMsg: To SMLink IPMB Interface\n");
        strcpy ((char *)m_MsgPkt.SrcQ, IPMB_SECONDARY_IFC_Q);
    }
    else if ((pBMCInfo->SERIALch != CH_NOT_USED && Channel == pBMCInfo->SERIALch) && pBMCInfo->IpmiConfig.SerialIfcSupport == 1)
    {
        TDBG ("SendMsg: To Serial Interface\n");
        Offset++;
        strcpy ((char *)m_MsgPkt.SrcQ, SERIAL_IFC_Q);
    }
    else if ((pBMCInfo->ICMBCh != CH_NOT_USED && Channel == pBMCInfo->ICMBCh) && pBMCInfo->IpmiConfig.ICMBIfcSupport == 1)
    {
        TDBG ("SendMsg: To ICMB Interface\n");
        strcpy ((char *)m_MsgPkt.SrcQ, ICMB_IFC_Q);
    }
    else if(pBMCInfo->SYSCh != CH_NOT_USED && Channel == pBMCInfo->SYSCh)
    {
        TDBG ("SendMsg: To System Interface\n");
        /*
         * According to IPMI Spec v2.0.
         * It is recommended to send CC_DEST_UNAVAILABLE
         * completion code, if the channel is disabled for
         * receiving messages.
         * */
        pChannelInfo = getChannelInfo(Channel,BMCInst);
        if(NULL == pChannelInfo)
        {
            *pRes = CC_INV_DATA_FIELD;
            return  sizeof (*pRes);
        }
        if (0x0 == pChannelInfo->ReceiveMsgQ)
        {
            TDBG ("The Channel(0x%x) has been Disabled "
                    "for Receive message\n", Channel);
            *pRes = CC_DEST_UNAVAILABLE;
            return sizeof (*pRes);
        }
        strcpy ((char *)m_MsgPkt.SrcQ, &g_RcvMsgQ[pBMCInfo->Msghndlr.CurKCSIfcNum][0]);
        m_MsgPkt.Param = SrcSessionHndl;
    }
    else
    {
        TDBG ("SendMsg: Invalid Channel\n");
        *pRes = CC_DEST_UNAVAILABLE;
        return sizeof (*pRes);
    }

    if( (TRUE == pBMCInfo->NMConfig.NMSupport) && (pBMCInfo->NMConfig.NMDevSlaveAddress == pIPMIMsgHdr->ResAddr) &&
            (Channel == (NM_PRIMARY_IPMB_BUS == pBMCInfo->NMConfig.NM_IPMBBus) ? pBMCInfo->PrimaryIPMBCh : pBMCInfo->SecondaryIPMBCh) )
    {
        if( (pBMCInfo->RMCPLAN1Ch == pBMCInfo->Msghndlr.CurChannel) ||
                (pBMCInfo->RMCPLAN2Ch == pBMCInfo->Msghndlr.CurChannel) ||
                (pBMCInfo->RMCPLAN3Ch == pBMCInfo->Msghndlr.CurChannel) ||
                (pBMCInfo->RMCPLAN4Ch == pBMCInfo->Msghndlr.CurChannel) ||
                (pBMCInfo->SERIALch   == pBMCInfo->Msghndlr.CurChannel) )
        {
            if(PRIV_ADMIN != pBMCInfo->Msghndlr.CurPrivLevel)
            {
                TDBG("Insufficient Privilege\n");
                *pRes = CC_INSUFFIENT_PRIVILEGE;
                return sizeof(*pRes);
            }
        }
    }
#if 0
    if(g_PDKHandle[PDK_BEFORESENDMESSAGE] != NULL)
    {
        RetVal = ( (int (*)(INT8U*, INT8U, INT8U*, int) ) g_PDKHandle[PDK_BEFORESENDMESSAGE]) ( pReq, ReqLen, pRes, BMCInst);

        if(0 < RetVal)
        {
            return RetVal;
        }
    }
#endif
    if (1 == Tracking)
    {
        /* Response length is set to zero to make MsgHndlr skip responding to this request
         * The Response will be handled by the ipmb interface after verifying NAK.
         */
        ResLen = 0;

        /* Tracking is not required if originator is System ifc */
        if (SYS_IFC_CHANNEL == (pBMCInfo->Msghndlr.CurChannel  & 0xF))
        {
            *pRes = CC_INV_DATA_FIELD;
            return sizeof (*pRes);
        }

        OS_THREAD_MUTEX_ACQUIRE(&pBMCInfo->PendBridgeMutex, WAIT_INFINITE);

        /* Store in the table for response tracking */
        for (i=0; i < sizeof (m_PendingBridgedResTbl)/sizeof (m_PendingBridgedResTbl[0]); i++)
        {
            if (FALSE == m_PendingBridgedResTbl[i].Used)
            {
                m_PendingBridgedResTbl[i].TimeOut = pBMCInfo->IpmiConfig.SendMsgTimeout;
                m_PendingBridgedResTbl[i].SeqNum  = GetIPMBSeqNumber(BMCInst);
                m_PendingBridgedResTbl[i].ChannelNum = Channel; /* Destination Channel number */
                m_PendingBridgedResTbl[i].OriginSrc  = ORIGIN_SENDMSG;

                if (1 != Offset)
                {
                    m_PendingBridgedResTbl[i].DstSessionHandle = pReq[Offset]; /* Session handle */
                }

                m_PendingBridgedResTbl[i].SrcSessionHandle = SrcSessionHndl;

                _fmemcpy (&m_PendingBridgedResTbl[i].ReqMsgHdr, pIPMIMsgHdr, sizeof (IPMIMsgHdr_T));

                /* Format the IPMI Msg Hdr */
                if(Channel == pBMCInfo->PrimaryIPMBCh)
                {
                    pIPMIMsgHdr->ReqAddr = pBMCInfo->IpmiConfig.PrimaryIPMBAddr;
                }
                else if(Channel == pBMCInfo->SecondaryIPMBCh)
                {
                    pIPMIMsgHdr->ReqAddr = pBMCInfo->IpmiConfig.SecondaryIPMBAddr;
                }
                else
                {
                    pIPMIMsgHdr->ReqAddr = pBMCInfo->IpmiConfig.BMCSlaveAddr;
                }

                pIPMIMsgHdr->RqSeqLUN = (pBMCInfo->SendMsgSeqNum << 2) & 0xFC; /* Seq Num and LUN =00 */

                /* Recalculate the checksum */
                m_MsgPkt.Data[ReqLen - 2] = CalculateCheckSum2 ((_FAR_ INT8U*)pIPMIMsgHdr, ReqLen - Offset - 1);
                if(IsLANChannel(pBMCInfo->Msghndlr.CurChannel  & 0xF, BMCInst))
                {
                    memset(QueueName,0,sizeof(QueueName));
                    sprintf(QueueName,"%s%d",LAN_IFC_Q,BMCInst);
                    strcpy ((char *)m_PendingBridgedResTbl[i].DestQ,QueueName);
                }
                else
                {
                    if(( (pBMCInfo->Msghndlr.CurChannel  & 0xF) == PRIMARY_IPMB_CHANNEL) && (pBMCInfo->IpmiConfig.PrimaryIPMBSupport == 1 ))
                    {
                        memset(QueueName,0,sizeof(QueueName));
                        sprintf(QueueName,"%s%d",IPMB_PRIMARY_IFC_Q,BMCInst);
                        strcpy ((char *)m_PendingBridgedResTbl[i].DestQ,QueueName);
                    }
                    else if((pBMCInfo->SecondaryIPMBCh != CH_NOT_USED && Channel == pBMCInfo->SecondaryIPMBCh) && pBMCInfo->IpmiConfig.SecondaryIPMBSupport == 1)
                    {
                        memset(QueueName,0,sizeof(QueueName));
                        sprintf(QueueName,"%s%d",IPMB_SECONDARY_IFC_Q,BMCInst);
                        strcpy ((char *)m_PendingBridgedResTbl[i].DestQ,QueueName);
                    }
                    else if ((pBMCInfo->SERIALch != CH_NOT_USED && Channel == pBMCInfo->SERIALch) && pBMCInfo->IpmiConfig.SerialIfcSupport == 1)
                    {
                        memset(QueueName,0,sizeof(QueueName));
                        sprintf(QueueName,"%s%d",SERIAL_IFC_Q,BMCInst);
                        strcpy ((char *)m_PendingBridgedResTbl[i].DestQ,QueueName);
                    }
                    else if((pBMCInfo->SMBUSCh != CH_NOT_USED && Channel == pBMCInfo->SMBUSCh) && pBMCInfo->IpmiConfig.SMBUSIfcSupport == 1)
                    {
                        //strcpy ((char *)m_PendingBridgedResTbl[i].DestQ, NULL);
                    }
                    else if ((pBMCInfo->ICMBCh != CH_NOT_USED && Channel == pBMCInfo->ICMBCh) && pBMCInfo->IpmiConfig.ICMBIfcSupport == 1)
                    {
                        memset(QueueName,0,sizeof(QueueName));
                        sprintf(QueueName,"%s%d",ICMB_IFC_Q,BMCInst);
                        strcpy ((char *)m_PendingBridgedResTbl[i].DestQ,QueueName);
                    }
                }
                m_PendingBridgedResTbl[i].Used = TRUE;
                IPMI_DBG_PRINT_1( "SendMessage:  Bridged message added index = %d.\n", i );

                break;
            }
        }
        OS_THREAD_MUTEX_RELEASE(&pBMCInfo->PendBridgeMutex);

        if (i == sizeof (m_PendingBridgedResTbl)/sizeof (m_PendingBridgedResTbl[0]))
        {
            /* If not been added to the Pending Bridge table, an error should be reported back.
               If not, for internal channel, the thread calling it may end up waiting! */
            *pRes = CC_NODE_BUSY;
            return  sizeof (*pRes);
        }
    }

    if ((pBMCInfo->SYSCh == (pBMCInfo->Msghndlr.CurChannel  & 0xF)) && pBMCInfo->IpmiConfig.SYSIfcSupport == 0x01)
    {
        /* Format the IPMI Msg Hdr */
        // Fill the address from Infrastrucure function instead of using PRIMARY_IPMB_ADDR/SECONDARY_IPMB_ADDR
        // TODO:AMI - Some function used to get our cell Id, probably infrastructure
        // cellId = GetCellId();
        if(Channel == pBMCInfo->PrimaryIPMBCh)
        {
            pIPMIMsgHdr->ReqAddr = pBMCInfo->IpmiConfig.PrimaryIPMBAddr;
        }
        else if(Channel == pBMCInfo->SecondaryIPMBCh)
        {
            pIPMIMsgHdr->ReqAddr = pBMCInfo->IpmiConfig.SecondaryIPMBAddr;
        }
        else
        {
            pIPMIMsgHdr->ReqAddr = pBMCInfo->IpmiConfig.BMCSlaveAddr;
        }


        /*Change the encapsulated request's LUN based on originating KCS interface */

        pIPMIMsgHdr->RqSeqLUN = (pIPMIMsgHdr->RqSeqLUN & 0xFC) | (pBMCInfo->Msghndlr.CurKCSIfcNum + 0x01);
        m_MsgPkt.Data[ReqLen - 2] = CalculateCheckSum2 ((_FAR_ INT8U*)pIPMIMsgHdr, ReqLen - Offset - 1);
    }

    IPMI_DBG_PRINT ("SendMsgCmd:Posting to interface");
    IPMI_DBG_PRINT_BUF (m_MsgPkt.Data, m_MsgPkt.Size);

    m_MsgPkt.Cmd = PAYLOAD_IPMI_MSG;

    /* Post the message to interface */
    if (0 != PostMsg (&m_MsgPkt,(INT8S *)m_MsgPkt.SrcQ, BMCInst))
    {
        TDBG ("SendMsg: Failed to post message to interface queue\n");
    }

    pSendMsgRes->CompletionCode = CC_NORMAL;

    if(pBMCInfo->IpmiConfig.SecondaryIPMBSupport == 0x01)
    {
        if ((PRIMARY_IPMB_CHANNEL== (pBMCInfo->Msghndlr.CurChannel  & 0xF) || pBMCInfo->SecondaryIPMBCh == (g_BMCInfo[BMCInst].Msghndlr.CurChannel  & 0xF)) && 1 == Tracking)
        {
            return 0;
        }
    }
    else
    {
        if ( PRIMARY_IPMB_CHANNEL == (pBMCInfo->Msghndlr.CurChannel  & 0xF) && (1 == Tracking))
        {
            return 0;
        }
    }
    return ResLen;
}


/*---------------------------------------
 * ReadEvtMsgBuffer
 *---------------------------------------*/
    int
ReadEvtMsgBuffer (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  ReadEvtMsgBufRes_T* pReadEvtMsgBufRes = (_NEAR_ ReadEvtMsgBufRes_T*)pRes;
#if GET_MSG_FLAGS != UNIMPLEMENTED
    GetMsgFlagsRes_T   GetMsgFlagsRes;
#endif
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    if (-1 == GetMsg(&m_MsgPkt, EVT_MSG_Q, WAIT_NONE, BMCInst))
    {
        /*If queue is empty */
        pReadEvtMsgBufRes->CompletionCode = CC_EVT_MSG_QUEUE_EMPTY;/* Queue is empty    */
        return  sizeof (*pRes);
    }

    BMC_GET_SHARED_MEM (BMCInst)->NumEvtMsg--;

#if GET_MSG_FLAGS != UNIMPLEMENTED
    // Clear SMS_ATN bit if and only if the Get Message Flag return 0 in byte 2.
    GetMsgFlags (NULL, 0, (INT8U *)&GetMsgFlagsRes,BMCInst);
    TDBG("GetMsgFlagsRes.CompletionCode : %X, GetMsgFlagsRes.MsgFlags : %X\n",
            GetMsgFlagsRes.CompletionCode, GetMsgFlagsRes.MsgFlags);
    if (GetMsgFlagsRes.CompletionCode == CC_NORMAL && GetMsgFlagsRes.MsgFlags == 0)
#else
        if (0 == BMC_GET_SHARED_MEM (BMCInst)->NumEvtMsg)
#endif
        {
            /* if there is no messssage in buffer reset SMS/EVT ATN bit */
            //       CLEAR_SMS_ATN ();
#if 0
            if (pBMCInfo->IpmiConfig.KCS1IfcSupport == 1)
            {
                CLEAR_SMS_ATN (0, BMCInst);
            }
            if (pBMCInfo->IpmiConfig.KCS2IfcSupport == 1)
            {
                CLEAR_SMS_ATN (1, BMCInst);
            }
            if (pBMCInfo->IpmiConfig.KCS3IfcSuppport == 1)
            {
                CLEAR_SMS_ATN (2, BMCInst);
            }
#endif
        }

    /* clear EventMessageBuffer full flag */
    BMC_GET_SHARED_MEM (BMCInst)->MsgFlags &= ~0x02;

    pReadEvtMsgBufRes->CompletionCode = CC_NORMAL; /* Completion Code   */

    /* copy the Message data */
    _fmemcpy (pReadEvtMsgBufRes->ResData, m_MsgPkt.Data, m_MsgPkt.Size);

    return sizeof (ReadEvtMsgBufRes_T);
}


/*---------------------------------------
 * GetBTIfcCap
 *---------------------------------------*/
    int
GetBTIfcCap (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  GetBTIfcCapRes_T* pGetBTIfcCapRes = (_NEAR_ GetBTIfcCapRes_T*)pRes;

    pGetBTIfcCapRes->CompletionCode     = CC_NORMAL;
    pGetBTIfcCapRes->NumReqSupported    = 2;
    pGetBTIfcCapRes->InputBufSize       = MAX_BT_PKT_LEN;
    pGetBTIfcCapRes->OutputBufSize      = MAX_BT_PKT_LEN;
    pGetBTIfcCapRes->RespTime           = 1;
    pGetBTIfcCapRes->Retries            = 0;

    return sizeof (GetBTIfcCapRes_T);
}


/*---------------------------------------
 * GetSystemGUID
 *---------------------------------------*/
    int
GetSystemGUID (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_ GetSysGUIDRes_T* pGetSysGUIDRes = (_NEAR_ GetSysGUIDRes_T*)pRes;

    pGetSysGUIDRes->CompletionCode  = CC_NORMAL;
    LOCK_BMC_SHARED_MEM (BMCInst);
    _fmemcpy (&pGetSysGUIDRes->Node, BMC_GET_SHARED_MEM (BMCInst)->SystemGUID, 16);
    UNLOCK_BMC_SHARED_MEM (BMCInst);

    return sizeof (GetSysGUIDRes_T);
}


#define SUPPORT_IPMI20  0x02
#define SUPPORT_IPMI15  0x01
/*---------------------------------------
 * GetChAuthCap
 *---------------------------------------*/
    int
GetChAuthCap (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_ GetChAuthCapReq_T*   pGetChAuthCapReq = (_NEAR_ GetChAuthCapReq_T*)pReq;
    _NEAR_ GetChAuthCapRes_T*   pGetChAuthCapRes = (_NEAR_ GetChAuthCapRes_T*)pRes;
    _FAR_  ChannelUserInfo_T*   pChUserInfo;
    _FAR_  UserInfo_T*          pUserInfo;
    _FAR_  ChannelInfo_T*       pChannelInfo;
    INT8U                       ChannelNum, Index;
    INT8U                       i;
    INT8U 			EthIndex=0;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    ChannelNum  = pGetChAuthCapReq->ChannelNum & 0x0F;
    memset (pGetChAuthCapRes, 0, sizeof (GetChAuthCapRes_T));

    //Check for Reserved bits
    if((pGetChAuthCapReq->ChannelNum & (BIT6 | BIT5 | BIT4)) || (pGetChAuthCapReq->PrivLevel == AUTHTYPE_NONE) || (pGetChAuthCapReq->PrivLevel > AUTHTYPE_OEM_PROPRIETARY))
    {
        pGetChAuthCapRes->CompletionCode = CC_INV_DATA_FIELD;   /*Reserved bits   */
        return sizeof (*pRes);
    }

    /* Information for this Channel */
    if (CURRENT_CHANNEL_NUM == ChannelNum)
    {
        ChannelNum = g_BMCInfo[BMCInst].Msghndlr.CurChannel  & 0xF;
    }

    if (0 != IsLANChannel(ChannelNum,BMCInst))
    {
        EthIndex= GetEthIndex(ChannelNum,BMCInst);
        if(0xff == EthIndex)
        {
            *pRes = CC_INV_DATA_FIELD;
            return sizeof (*pRes);
        }
    }

    pChannelInfo = getChannelInfo (ChannelNum, BMCInst);

    if (NULL == pChannelInfo)
    {
        /* Invalid channel  */
        pGetChAuthCapRes->CompletionCode = CC_INV_DATA_FIELD;   /*Invalid Channel   */
        return sizeof (*pRes);
    }

    pGetChAuthCapRes->CompletionCode    = CC_NORMAL; /* completion code */
    pGetChAuthCapRes->ChannelNum        = ChannelNum; /* channel No */


    if (IsLANChannel(ChannelNum, BMCInst)|| (pBMCInfo->IpmiConfig.SerialIfcSupport == 1 && ChannelNum == pBMCInfo->SERIALch))
    {
        LOCK_BMC_SHARED_MEM(BMCInst);

        /* Authentication Type Supported for given Privilege */
        pGetChAuthCapRes->AuthType = pChannelInfo->AuthType[pGetChAuthCapReq->PrivLevel - 1] & 0x37;

        /*  Get the NULL UserID informations */
        pChUserInfo  = getChUserIdInfo (NULL_USER_ID, &Index, pChannelInfo->ChannelUserInfo, BMCInst);
        pUserInfo    = getUserIdInfo (NULL_USER_ID, BMCInst);

        pGetChAuthCapRes->PerMsgUserAuthLoginStatus = 0;

        /* If NULL user not elabled or NULL user have no access to this channel */
        if ((NULL == pChUserInfo) || (NULL == pUserInfo) || (0 == pUserInfo->UserStatus) ||
                ((pChUserInfo->AccessLimit & 0x0F) == 0x0F))
        {
            /* If Null User Not Found, Reset NullUserNullPassword LoginStatus,
             * NullUser,  Password LoginStatus bits
             */
            pGetChAuthCapRes->PerMsgUserAuthLoginStatus &= ~0x03;
            /* Non Null User Name Enabled   since atleast one user should be enabled for a channel */
            pGetChAuthCapRes->PerMsgUserAuthLoginStatus |= 0x04;

            /* Null User Null Password Disabled */
            pGetChAuthCapRes->PerMsgUserAuthLoginStatus &= ~0x01;
            /* Null User with Password Disabled */
            pGetChAuthCapRes->PerMsgUserAuthLoginStatus &= ~0x02;
        }
        else /*else  if NULL User Enabled */
        {
            INT8U   UserPswd[MAX_PASSWD_LEN] = {0};
            INT8U EncryptionEnabled = 0;
            INT8U    PwdEncKey[MAX_SIZE_KEY] = {0};
#if 0
            if (g_corefeatures.userpswd_encryption == ENABLED)
            {
                if(getEncryptKey(PwdEncKey))
                {
                    TCRIT("Unable to get the encryption key. quitting...\n");
                    *pRes = CC_UNSPECIFIED_ERR;
                    return sizeof(*pRes);
                }
                if(DecryptPassword((INT8S *)(pBMCInfo->EncryptedUserInfo[NULL_USER_ID - 1].EncryptedPswd), MAX_PASSWORD_LEN, (INT8S*)UserPswd, MAX_PASSWORD_LEN, PwdEncKey))
                {
                    printf("\nError in GetChAuthCap() \n");
                    *pRes = CC_UNSPECIFIED_ERR;
                    return sizeof(*pRes);
                }
                EncryptionEnabled = 1;
            }
#endif
            if (((1 == EncryptionEnabled) && (0 ==_fmemcmp(UserPswd,"\0",1))) ||
                    ((0 == EncryptionEnabled) && (0 ==_fmemcmp(pUserInfo->UserPassword,"\0",1))))
            {
                /* Null User Null Password Enabled  */
                pGetChAuthCapRes->PerMsgUserAuthLoginStatus |= 0x01;
                /* Null User with Password Disabled */
                pGetChAuthCapRes->PerMsgUserAuthLoginStatus &= ~0x02;
            }
            else
            {
                /* Null User Null Password Disabled */
                pGetChAuthCapRes->PerMsgUserAuthLoginStatus &= ~0x01;
                /* Null User with Password Enabled  */
                pGetChAuthCapRes->PerMsgUserAuthLoginStatus |= 0x02;
            }

            if (pChannelInfo->NoCurrentUser < 2)
            {
                /* only one user ie. NULL User enabled - no Non NULL User Enabled */
                pGetChAuthCapRes->PerMsgUserAuthLoginStatus &= ~0x04;
            }
            else if (pChannelInfo->NoCurrentUser > 0)
            {
                /* one User other than Null User is Enabled - a Non NULL User is Enabled */
                pGetChAuthCapRes->PerMsgUserAuthLoginStatus |= 0x04;
            }
        }

        /* User level Authentication */
        pGetChAuthCapRes->PerMsgUserAuthLoginStatus |= (pChannelInfo->UserLevelAuth <<3);

        /* PerMessage Authentication */
        pGetChAuthCapRes->PerMsgUserAuthLoginStatus |= (pChannelInfo->PerMessageAuth << 4);

        UNLOCK_BMC_SHARED_MEM(BMCInst);
    }

#if 0 /*IPMI20_SUPPORT == 1*/
    if (pGetChAuthCapReq->ChannelNum & 0x80)
    {
        pGetChAuthCapRes->AuthType |= 0x80;

        // At present Lan channel alone supports RMCP+
        if ( IsLANChannel(ChannelNum, BMCInst))
        {
            pGetChAuthCapRes->ExtCap = (SUPPORT_IPMI20 | SUPPORT_IPMI15);

            /* Set KG status */
            for (i = 0; i < HASH_KEY_LEN; i++)
            {
                if (pBMCInfo->RMCPPlus[EthIndex].KGHashKey[i] != 0)	{ break;}
            }
            if (i < HASH_KEY_LEN) { pGetChAuthCapRes->PerMsgUserAuthLoginStatus |= 0x20; }
        }
        else if (pBMCInfo->IpmiConfig.SerialIfcSupport == 1 && ChannelNum == pBMCInfo->SERIALch)
        {
            pGetChAuthCapRes->ExtCap = SUPPORT_IPMI15;
        }
    }
#endif

    return sizeof (GetChAuthCapRes_T);
}


/*---------------------------------------
 * GetSessionChallenge
 *---------------------------------------*/
    int
GetSessionChallenge (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_ GetSesChallengeReq_T* pGetSesChalReq = (_NEAR_ GetSesChallengeReq_T*)pReq;
    _NEAR_ GetSesChallengeRes_T* pGetSesChalRes = (_NEAR_ GetSesChallengeRes_T*)pRes;
    INT8U                 Index;
    INT8U                 UserName[MAX_USERNAME_LEN];
    INT8U                 ChallengeString[CHALLENGE_STR_LEN];
    _FAR_  ChannelInfo_T*        pChannelInfo;
    _FAR_  ChannelUserInfo_T*    pChUserInfo;
    _FAR_  UserInfo_T*           pUserInfo;
    _FAR_  BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    SessionInfo_T         SessionInfo;
    INT32U                TempSessId;
    INT8U ChannelNum, EthIndex = 0, EnabledAuthType;
    INT8U AuthTypeTable[6] ={1,2,4,0,0x10,0x20};
    unsigned int seed = 1;
    char    UserPswd[MAX_PASSWD_LEN];
    INT8U    PwdEncKey[MAX_SIZE_KEY] = {0};

    TDBG (" Process GetSessChall\n ");
    /* get the Authentication Type Requested */

    // Check for Reserved bits
    if((pGetSesChalReq->AuthType == AUTHTYPE_RESERVED) || (pGetSesChalReq->AuthType > AUTHTYPE_OEM_PROPRIETARY))
    {
        pGetSesChalRes->CompletionCode = CC_INV_DATA_FIELD;
        return sizeof (*pRes);
    }

    ChannelNum = g_BMCInfo[BMCInst].Msghndlr.CurChannel  & 0xF;
    if(0 !=  IsLANChannel(ChannelNum,BMCInst))
    {
        EthIndex= GetEthIndex(g_BMCInfo[BMCInst].Msghndlr.CurChannel  & 0xF,BMCInst);
        if(0xff == EthIndex)
        {
            *pRes = CC_INV_DATA_FIELD;
            return sizeof (INT8U);
        }

        if((pBMCInfo->LANCfs[EthIndex].AuthTypeSupport & AuthTypeTable[pGetSesChalReq->AuthType]) != AuthTypeTable[pGetSesChalReq->AuthType])
        {
            *pRes = CC_INV_DATA_FIELD;
            return sizeof (INT8U);
        }
    }

    if(ChannelNum == g_BMCInfo[BMCInst].SERIALch)
    {
        if((pBMCInfo->SMConfig.AuthTypeSupport & AuthTypeTable[pGetSesChalReq->AuthType]) != AuthTypeTable[pGetSesChalReq->AuthType])
        {
            *pRes = CC_INV_DATA_FIELD;
            return sizeof (INT8U);
        }
    }

    /* get the user Name */
    _fmemcpy (UserName, pGetSesChalReq->UserName, sizeof (pGetSesChalReq->UserName));


    /*get the Information for this channel */
    pChannelInfo = getChannelInfo (g_BMCInfo[BMCInst].Msghndlr.CurChannel  & 0xF, BMCInst);

    if (NULL == pChannelInfo)
    {
        /* Invalid Channel */
        pGetSesChalRes->CompletionCode = CC_INV_DATA_FIELD;
        return sizeof (*pRes);
    }

    LOCK_BMC_SHARED_MEM(BMCInst);

    if((g_BMCInfo[BMCInst].Msghndlr.CurChannel & LOOP_BACK_REQ) && UserName[0]== 0)
    {
        pUserInfo = getUserIdInfo(NULL_USER, BMCInst);
    }
    else
    {
        /*get userInfo for the given userName*/
        pChUserInfo = getChUserInfo ((_NEAR_ char *)UserName, &Index, pChannelInfo->ChannelUserInfo,BMCInst);

        /*if user not found  */
        if (NULL == pChUserInfo)
        {
            /* Invalid user     */
            pGetSesChalRes->CompletionCode = CC_GET_SESSION_INVALID_USER;
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            return sizeof (*pRes);
        }
#if 0
        if (g_corefeatures.userpswd_encryption == ENABLED)
        {
            if(getEncryptKey(PwdEncKey))
            {
                TCRIT("Error in getting encryption key. So, quitting...\n");
                *pRes = CC_UNSPECIFIED_ERR;
                return sizeof(*pRes);
            }
            if(DecryptPassword((INT8S *)(pBMCInfo->EncryptedUserInfo[pChUserInfo->UserId - 1].EncryptedPswd), MAX_PASSWORD_LEN, UserPswd, MAX_PASSWORD_LEN, PwdEncKey))
            {
                *pRes = CC_UNSPECIFIED_ERR;
                return sizeof(*pRes);
            }
        }
#endif
        if(ChannelNum == g_BMCInfo[BMCInst].SERIALch)
        {
            switch (pChUserInfo->AccessLimit & 0x0F)
            {
                case 0x01:
                    /* call back */
                    EnabledAuthType = pBMCInfo->SMConfig.AuthTypeEnable.Callback;
                    break;

                case 0x02:
                    /* user */
                    EnabledAuthType = pBMCInfo->SMConfig.AuthTypeEnable.User;
                    break;

                case 0x03:
                    /* operator */
                    EnabledAuthType = pBMCInfo->SMConfig.AuthTypeEnable.Operator;
                    break;

                case 0x04:
                    /* admin */
                    EnabledAuthType = pBMCInfo->SMConfig.AuthTypeEnable.Admin;
                    break;

                case 0x05:
                    /* oem */
                    EnabledAuthType = pBMCInfo->SMConfig.AuthTypeEnable.oem;
                    break;

                case 0x0F:
                    pGetSesChalRes->CompletionCode = CC_GET_SESSION_INVALID_USER;
                    UNLOCK_BMC_SHARED_MEM(BMCInst);
                    return sizeof (*pRes);

                default:
                    pGetSesChalRes->CompletionCode = CC_INV_DATA_FIELD;
                    UNLOCK_BMC_SHARED_MEM(BMCInst);
                    return sizeof (*pRes);

            }

        }else{
            //LANCfs
            switch (pChUserInfo->AccessLimit & 0x0F)
            {
                case 0x01:
                    /* call back */
                    EnabledAuthType = pBMCInfo->LANCfs[EthIndex].AuthTypeEnables.AuthTypeCallBack;
                    break;

                case 0x02:
                    /* user */
                    EnabledAuthType = pBMCInfo->LANCfs[EthIndex].AuthTypeEnables.AuthTypeUser;
                    break;

                case 0x03:
                    /* operator */
                    EnabledAuthType = pBMCInfo->LANCfs[EthIndex].AuthTypeEnables.AuthTypeOperator;
                    break;

                case 0x04:
                    /* admin */
                    EnabledAuthType = pBMCInfo->LANCfs[EthIndex].AuthTypeEnables.AuthTypeAdmin;
                    break;

                case 0x05:
                    /* oem */
                    EnabledAuthType = pBMCInfo->LANCfs[EthIndex].AuthTypeEnables.AuthTypeOem;
                    break;

                case 0x0F:
                    pGetSesChalRes->CompletionCode = CC_GET_SESSION_INVALID_USER;
                    UNLOCK_BMC_SHARED_MEM(BMCInst);
                    return sizeof (*pRes);

                default:
                    pGetSesChalRes->CompletionCode = CC_INV_DATA_FIELD;
                    UNLOCK_BMC_SHARED_MEM(BMCInst);
                    return sizeof (*pRes);
            }

        }

        if( (EnabledAuthType & AuthTypeTable[pGetSesChalReq->AuthType])	!= AuthTypeTable[pGetSesChalReq->AuthType])
        {
            /* User has Auth type not supported */
            pGetSesChalRes->CompletionCode = CC_INV_DATA_FIELD;
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            return sizeof (*pRes);
        }

        pUserInfo = getUserIdInfo (pChUserInfo->UserId,BMCInst);
        if (NULL == pUserInfo || FALSE == pUserInfo->UserStatus)
        {
            if(NULL != pUserInfo && 1 == pChUserInfo->UserId && 0 == UserName[0])
                pGetSesChalRes->CompletionCode = CC_GET_SESSION_NULL_USER_DISABLED;
            else /* User does not exist */
                pGetSesChalRes->CompletionCode = CC_GET_SESSION_INVALID_USER;
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            return sizeof (*pRes);
        }

    }

    if (pUserInfo->MaxPasswordSize == (IPMI_20_PASSWORD_LEN - 2))
    {
        /* password len is not match */
        pGetSesChalRes->CompletionCode = CC_INV_DATA_FIELD;
        UNLOCK_BMC_SHARED_MEM(BMCInst);
        return sizeof (*pRes);
    }

    _fmemset (&SessionInfo, 0, sizeof (SessionInfo_T));

    do
    {
        /*generate 32 bit temp session Id*/
        TempSessId = ((INT32U)rand_r(&seed) << 16) | rand_r(&seed);
    } while ((NULL != getSessionInfo (SESSION_ID_INFO, &TempSessId, BMCInst)) || (0 == TempSessId));

    TDBG (" Challenger Str \n ");

    /* generate Randam Challenge String */
    for(Index=0;Index < CHALLENGE_STR_LEN;Index++)
    {
        ChallengeString[Index] = rand_r(&seed);
    }


    SessionInfo.UserId       = (INT8U)pUserInfo->UserId;
    /*Activated in ActivateSession Command*/
    SessionInfo.Activated    = FALSE;
    SessionInfo.AuthType     = pGetSesChalReq->AuthType;
    SessionInfo.Channel      = g_BMCInfo[BMCInst].Msghndlr.CurChannel  & 0xF;
    SessionInfo.SessionID    = TempSessId;

    if(pBMCInfo->IpmiConfig.SerialIfcSupport == 1)
    {
        if (pBMCInfo->SERIALch== (g_BMCInfo[BMCInst].Msghndlr.CurChannel  & 0xF))
        {
            SessionInfo.TimeOutValue = pBMCInfo->SMConfig.SessionInactivity * 30;
        }
    }
    else
    {
        /* set the session timeout value */
#ifdef CONFIGURABLE_SESSION_TIME_OUT
        SessionInfo.TimeOutValue = BMC_GET_SHARED_MEM(BMCInst)->uSessionTimeout;
#else
        SessionInfo.TimeOutValue = pBMCInfo->IpmiConfig.SessionTimeOut;
#endif

        if(pBMCInfo->IpmiConfig.SOLIfcSupport == 1)
        {
            if(SessionInfo.SessPyldInfo [PAYLOAD_SOL].Type == PAYLOAD_SOL)
                SessionInfo.TimeOutValue = pBMCInfo->IpmiConfig.SOLSessionTimeOut;
        }
    }

    if(g_BMCInfo[BMCInst].Msghndlr.CurChannel & LOOP_BACK_REQ)
        SessionInfo.IsLoopBack =TRUE;
    if (g_corefeatures.userpswd_encryption == ENABLED)
    {
        _fmemcpy (SessionInfo.Password,UserPswd, MAX_PASSWORD_LEN);
    }
    else
    {
        _fmemcpy (SessionInfo.Password,pUserInfo->UserPassword, MAX_PASSWORD_LEN);
    }

    /* save the challenge String */
    _fmemcpy (SessionInfo.ChallengeString,ChallengeString, CHALLENGE_STR_LEN);

    if(GetNumOfUsedSessions(BMCInst)>= pBMCInfo->IpmiConfig.MaxSession)
    {
        CleanSession(BMCInst);
    }
    /* Add Session information in Table */
    AddSession (&SessionInfo, BMCInst);

    UNLOCK_BMC_SHARED_MEM(BMCInst);

    pGetSesChalRes->CompletionCode = CC_NORMAL;
    pGetSesChalRes->TempSessionID  = TempSessId;

    _fmemcpy(pGetSesChalRes->ChallengeString, ChallengeString, CHALLENGE_STR_LEN);

    return sizeof (GetSesChallengeRes_T);
}

/*---------------------------------------
 * ActivateSession
 *---------------------------------------*/
    int
ActivateSession (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  ActivateSesReq_T*   pAcvtSesReq = (_NEAR_ ActivateSesReq_T*)pReq;
    _NEAR_  ActivateSesRes_T*   pAcvtSesRes = (_NEAR_ ActivateSesRes_T*)pRes;
    _FAR_   ChannelInfo_T*      pChannelInfo;
    _FAR_   UserInfo_T*         pUserInfo;
    _FAR_   ChannelUserInfo_T*  pChUserInfo;
    _FAR_   SessionInfo_T*      pSessInfo;
    INT8U   Index, i, EthIndex, netindex = 0xFF;
    BOOL    TrackRollOver = FALSE;
    INT32U  TrackRollOverSeq = SEQNUM_ROLLOVER;
    char    IfcName[IFNAMSIZ];

    /* Initial Outbound Sequence Number cannot be null */
    if (pAcvtSesReq->OutboundSeq == 0)
    {
        printf("%s:%d\n",__FUNCTION__,__LINE__);/*test*/
        pAcvtSesRes->CompletionCode = CC_ACTIVATE_SESS_SEQ_OUT_OF_RANGE;
        return sizeof(*pRes);
    }

    /*get information abt this channel*/
    pChannelInfo = getChannelInfo (g_BMCInfo[BMCInst].Msghndlr.CurChannel  & 0xF, BMCInst);
    if(NULL == pChannelInfo)
    {
        printf("%s:%d\n",__FUNCTION__,__LINE__);/*test*/
        *pRes = CC_INV_DATA_FIELD;
        return	sizeof (*pRes);
    }

    pSessInfo = getSessionInfo (SESSION_ID_INFO, &g_BMCInfo[BMCInst].Msghndlr.CurSessionID, BMCInst);

    if (pChannelInfo->SessionLimit <= pChannelInfo->ActiveSession)
    {
        if(NULL != pSessInfo)
        {
            /* Delete the Temporary session added in Get Session challenge */
            DeleteSession (pSessInfo,BMCInst);
        }
        printf("%s:%d\n",__FUNCTION__,__LINE__);/*test*/
        /*cant accept any more session*/
        pAcvtSesRes->CompletionCode = CC_ACTIVATE_SESS_NO_SESSION_SLOT_AVAILABLE;
        return  sizeof (*pRes);
    }

    /* if invalid session ID */
    if (NULL == pSessInfo)
    {
        printf("%s:%d\n",__FUNCTION__,__LINE__);/*test*/
        pAcvtSesRes->CompletionCode = CC_ACTIVATE_SESS_INVALID_SESSION_ID;
        return sizeof (*pRes);
    }

    /* if not serial channel, check EthIndex */
    if(pChannelInfo->ChannelNumber != g_BMCInfo[BMCInst].SERIALch){


        EthIndex= GetEthIndex(pSessInfo->Channel & 0x0F, BMCInst);
        if(0xff == EthIndex)
        {
            printf("%s:%d\n",__FUNCTION__,__LINE__);/*test*/
            *pRes = CC_INV_DATA_FIELD;
            DeleteSession (pSessInfo,BMCInst);
            return sizeof (INT8U);
        }
        memset(IfcName,0,sizeof(IfcName));
        /*Get the EthIndex*/
        //  if(GetIfcName(EthIndex,IfcName, BMCInst) == -1)
        {
            //      TCRIT("Error in Getting Ifc name\n");
            //     *pRes = CC_INV_DATA_FIELD;
            //     return sizeof (INT8U);
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
            printf("%s:%d\n",__FUNCTION__,__LINE__);/*test*/
            return sizeof (INT8U);
        }

        if (IsLANChannel( pSessInfo->Channel, BMCInst))
        {
            if(g_corefeatures.global_ipv6  == ENABLED)
            {
                if(GetIPAdrressType(&pSessInfo->LANRMCPPkt.IPHdr.Srcv6Addr[0]) == 4)
                {
                    /* MAC Address */
                    /* nwGetSrcCacheMacAddr */
                    nwGetSrcCacheMacAddr((INT8U*)&pSessInfo->LANRMCPPkt.IPHdr.Srcv6Addr[12],
                            netindex, pSessInfo->LANRMCPPkt.MACHdr.DestAddr);
                }
                else
                {
                    /* MAC Address */
                    nwGetSrcMacAddr_IPV6((INT8U*)&pSessInfo->LANRMCPPkt.IPHdr.Srcv6Addr,
                            pSessInfo->LANRMCPPkt.MACHdr.DestAddr);
                }
            }
            else
            {

                /* MAC Address */
                nwGetSrcCacheMacAddr((INT8U*)pSessInfo->LANRMCPPkt.IPHdr.Srcv4Addr, netindex,
                        pSessInfo->LANRMCPPkt.MACHdr.DestAddr);
            }
        }
    }


    // Check for Reserved bitss
    if((pAcvtSesReq->AuthType == AUTHTYPE_RESERVED) || (pAcvtSesReq->AuthType > AUTHTYPE_OEM_PROPRIETARY))
    {
        pAcvtSesRes->CompletionCode = CC_INV_DATA_FIELD;
        printf("%s:%d\n",__FUNCTION__,__LINE__);/*test*/
        return sizeof (*pRes);
    }

    if ((pAcvtSesReq->Privilege == PRIV_LEVEL_RESERVED) || (pAcvtSesReq->Privilege > PRIV_LEVEL_PROPRIETARY))
    {
        TDBG ("AppDevice.c : Activate Sesssion - Invalid Privilage level\n");
        printf("%s:%d\n",__FUNCTION__,__LINE__);/*test*/
        pAcvtSesRes->CompletionCode = CC_INV_DATA_FIELD;/* Privilege is Reserved */
        return sizeof (*pRes);
    }

    LOCK_BMC_SHARED_MEM(BMCInst);

    /* if requested authentication not supported */
    if(!(pChannelInfo->AuthType[pAcvtSesReq->Privilege - 1] & ( 1 << pAcvtSesReq->AuthType) ))
    {
        TDBG ("AppDevice.c : Activate Sesssion - Authentication not supported\n");
        printf("%s:%d\n",__FUNCTION__,__LINE__);/*test*/
        /* check for given AuthType Enabled/supported */
        pAcvtSesRes->CompletionCode = CC_INV_DATA_FIELD;
        UNLOCK_BMC_SHARED_MEM(BMCInst);
        return sizeof (*pRes);
    }

    pUserInfo = getUserIdInfo (pSessInfo->UserId, BMCInst);
    if (NULL == pUserInfo)
    {
        TDBG ("AppDevice.c : Activate Sesssion - Invalid user Id\n");
        pAcvtSesRes->CompletionCode = CC_INV_DATA_FIELD;
        printf("%s:%d\n",__FUNCTION__,__LINE__);/*test*/
        UNLOCK_BMC_SHARED_MEM(BMCInst);
        return sizeof (*pRes);
    }

    if (pUserInfo->CurrentSession >= pUserInfo->MaxSession)
    {
        UNLOCK_BMC_SHARED_MEM(BMCInst);

        printf ("AppDevice.c : Activate Sesssion - Max user session limit reached\n");
        /* Delete the temporary session aded in GetSessionChanllenge */
        DeleteSession (pSessInfo,BMCInst);
        /*max session for user reached  */
        printf("%s:%d\n",__FUNCTION__,__LINE__);/*test*/
        pAcvtSesRes->CompletionCode = CC_ACTIVATE_SESS_NO_SLOT_AVAILABLE_USER;
        return  sizeof (*pRes);
    }



    pChUserInfo = getChUserIdInfo (pSessInfo->UserId, &Index, pChannelInfo->ChannelUserInfo, BMCInst);

    if (NULL == pChUserInfo)
    {
        TDBG ("AppDevice.c : Activate Sesssion - Channeluser information not found\n");
        pAcvtSesRes->CompletionCode = CC_INV_DATA_FIELD;
        printf("%s:%d\n",__FUNCTION__,__LINE__);/*test*/
        UNLOCK_BMC_SHARED_MEM(BMCInst);
        return sizeof (*pRes);
    }

    /* if requested privilege is greater than privilege level for channel or user */
    if((pAcvtSesReq->Privilege > pChannelInfo->MaxPrivilege) || (pAcvtSesReq->Privilege > pChUserInfo->AccessLimit))
    {
        TDBG ("AppDevice.c : Activate Sesssion - requested privilage exceeded User/Channel\n");
        pAcvtSesRes->CompletionCode = CC_ACTIVATE_SESS_MAX_PRIVILEGE_EXCEEDS_LIMIT;
        printf("%s:%d\n",__FUNCTION__,__LINE__);/*test*/
        UNLOCK_BMC_SHARED_MEM(BMCInst);
        return sizeof (*pRes);
    }


    if (MULTI_SESSION_CHANNEL == pChannelInfo->SessionSupport)
    {
        if (0 != _fmemcmp (pAcvtSesReq->ChallengeString, pSessInfo->ChallengeString, CHALLENGE_STR_LEN))
        {
            TDBG ("AppDevice.c : Activate Sesssion - Invalid Challenge string\n");
            pAcvtSesRes->CompletionCode = CC_INV_DATA_FIELD;
            printf("%s:%d\n",__FUNCTION__,__LINE__);/*test*/
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            return sizeof (*pRes);
        }
    }
    else if (SINGLE_SESSION_CHANNEL == pChannelInfo->SessionSupport)
    {
        _NEAR_  SessionHdr_T SessionHdr;
        INT8U        AuthCode[AUTH_CODE_LEN];

        SessionHdr.AuthType  = pAcvtSesReq->AuthType;
        SessionHdr.SessionID = pSessInfo->SessionID;

        ComputeAuthCode (pSessInfo->Password,
                &SessionHdr,
                (_NEAR_ IPMIMsgHdr_T*)pAcvtSesReq->ChallengeString,
                AuthCode,
                SINGLE_SESSION_CHANNEL);

        //        if(FindUserLockStatus(pSessInfo->UserId,pSessInfo->Channel,BMCInst) == 0)
        {
            if((AUTH_TYPE_NONE != pAcvtSesReq->AuthType) &&
                    (0 != _fmemcmp ( AuthCode, pAcvtSesReq->ChallengeString,CHALLENGE_STR_LEN)))
            {
                PasswordViolation (BMCInst);
                //                LockUser(pSessInfo->UserId,pSessInfo->Channel,BMCInst);
                TDBG ("AppDevice.c : Activate Sesssion - Invalid Authentication\n");
                pAcvtSesRes->CompletionCode = CC_INV_DATA_FIELD;
                printf("%s:%d\n",__FUNCTION__,__LINE__);/*test*/
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                return sizeof (*pRes);
            }
        }
#if 0
        else
        {
            TDBG("AppDevice.c : Activate Session - User Locked \n");
            pAcvtSesRes->CompletionCode = CC_INV_DATA_FIELD;
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            return sizeof (*pRes);
        }

        UnlockUser(pSessInfo->UserId,pSessInfo->Channel,BMCInst);
#endif
        pSessInfo->SerialModemMode = BMC_GET_SHARED_MEM (BMCInst)->SerialModemMode;

    }

    if(!pSessInfo->IsLoopBack)
    {
        /* Update active sessions for current user */
        //        pUserInfo->CurrentSession++;

        /* No of active session     */
        //        pChannelInfo->ActiveSession++;
    }

    /*if per Messaging Auth is disabled set Auth  NONE  */
    if (pChannelInfo->PerMessageAuth == 1)
    {
        pSessInfo->AuthType = AUTH_TYPE_NONE;
    }
    else
    {
        pSessInfo->AuthType = pAcvtSesReq->AuthType;
    }

    if(pAcvtSesReq->Privilege == PRIV_CALLBACK)
    {
        pSessInfo->Privilege = PRIV_CALLBACK;
    }
    else
    {
        /* Set the Privilege to User irrespective of Request */
        pSessInfo->Privilege = PRIV_USER;
    }

    pSessInfo->Activated    = TRUE;
    pSessInfo->Channel      = (g_BMCInfo[BMCInst].Msghndlr.CurChannel  & 0xF);
    /* set the Max Privilege allowed for the Session*/
    pSessInfo->MaxPrivilege = pAcvtSesReq->Privilege;
    /* set the Out bound sequensce Number */
    pSessInfo->OutboundSeq  = ipmitoh_u32 (pAcvtSesReq->OutboundSeq);

    BMC_GET_SHARED_MEM (BMCInst)->SessionHandle += 1;

    pSessInfo->SessionHandle = BMC_GET_SHARED_MEM (BMCInst)->SessionHandle;

    pAcvtSesRes->CompletionCode = CC_NORMAL;
    pAcvtSesRes->AuthType = (pSessInfo->AuthType & 0x0F);
    pAcvtSesRes->SessionID = pSessInfo->SessionID;
    pAcvtSesRes->InboundSeq = random();

    if(pAcvtSesRes->InboundSeq  == 0)
        pSessInfo->InitialInboundSeq = SEQNUM_ROLLOVER;
    else
        pSessInfo->InitialInboundSeq = (ipmitoh_u32(pAcvtSesRes->InboundSeq) -1);

    for(i=0;i<EIGHT_COUNT_WINDOW_LEN;i++)
    {
        if(((pSessInfo->InitialInboundSeq - (i+1)) != 0) &&(TrackRollOver == FALSE))
            pSessInfo->InboundTrac[i] = pSessInfo->InitialInboundSeq - (i+1);
        else if(((pSessInfo->InitialInboundSeq - (i+1)) == 0) &&(TrackRollOver == FALSE))
        {
            pSessInfo->InboundTrac[i] = pSessInfo->InitialInboundSeq - (i+1);
            TrackRollOver = TRUE;
        }
        else if(TrackRollOver == TRUE)
        {
            pSessInfo->InboundTrac[i] = TrackRollOverSeq;
            TrackRollOverSeq--;
        }
    }
    pSessInfo->InboundRecv = 0xFF;
    pSessInfo->InboundSeq = (ipmitoh_u32(pAcvtSesRes->InboundSeq) -1);
    pAcvtSesRes->Privilege = pSessInfo->MaxPrivilege;

    UNLOCK_BMC_SHARED_MEM(BMCInst);

    return sizeof (ActivateSesRes_T);
}


/*---------------------------------------
 * SetSessionPrivLevel
 *---------------------------------------*/
    int
SetSessionPrivLevel (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  SetSesPrivLevelReq_T*   pSetSesPrivLevelReq = (_NEAR_ SetSesPrivLevelReq_T*)pReq;
    _NEAR_  SetSesPrivLevelRes_T*   pSetSesPrivLevelRes = (_NEAR_ SetSesPrivLevelRes_T*)pRes;
    INT8U                   ChannelNum, UserID, Index = 0;
    _FAR_   ChannelInfo_T*          pChannelInfo;
    _FAR_   ChannelUserInfo_T*      pChUser;
    _FAR_   SessionInfo_T*          pSessInfo;

    //Check for Reserved bits
    if((pSetSesPrivLevelReq->Privilege == PRIV_LEVEL_CALLBACK) || (pSetSesPrivLevelReq->Privilege > PRIV_LEVEL_PROPRIETARY))
    {
        pSetSesPrivLevelRes->CompletionCode = CC_INV_DATA_FIELD;
        return sizeof(SetSesPrivLevelRes_T);
    }

    /* GetCurCmdSession () */
    pSessInfo = getSessionInfo (SESSION_ID_INFO, &g_BMCInfo[BMCInst].Msghndlr.CurSessionID, BMCInst);
    if (NULL == pSessInfo)
    {
        TDBG ("AppDevice.c : SetSession Privilege - Invalid Session ID\n");
        pSetSesPrivLevelRes->CompletionCode = CC_SET_SESS_PREV_INVALID_SESSION_ID;
        return sizeof (*pRes);
    }

    LOCK_BMC_SHARED_MEM(BMCInst);

    UserID      = pSessInfo->UserId;
    ChannelNum  = pSessInfo->Channel;

    pChannelInfo = getChannelInfo (ChannelNum, BMCInst);
    if(NULL == pChannelInfo)
    {
        pSetSesPrivLevelRes->CompletionCode = CC_INV_DATA_FIELD;
        return	sizeof (*pRes);
    }

    pChUser = getChUserIdInfo (UserID, &Index, pChannelInfo->ChannelUserInfo, BMCInst);

    if(pSetSesPrivLevelReq->Privilege == 0)
    {
        pSetSesPrivLevelRes->CompletionCode = CC_NORMAL;
        pSetSesPrivLevelRes->Privilege      = pSessInfo->Privilege;
        UNLOCK_BMC_SHARED_MEM(BMCInst);
        return sizeof(SetSesPrivLevelRes_T);
    }

    if (IsPrivilegeAvailable(pSetSesPrivLevelReq->Privilege, ChannelNum, BMCInst) != 0)
    {
        pSetSesPrivLevelRes->CompletionCode = CC_SET_SESS_PREV_REQ_LEVEL_NOT_AVAILABLE;
        UNLOCK_BMC_SHARED_MEM(BMCInst);
        return sizeof(*pRes);
    }

    /* if Requested Privilege is greater than Privilege requested in ActivateSessionCommand
     * if requested privilege is greater than the channel privilege or user privilege
     */
    if((pSetSesPrivLevelReq->Privilege > pSessInfo->MaxPrivilege)    ||
            (pSetSesPrivLevelReq->Privilege > pChannelInfo->MaxPrivilege) ||
            (pSetSesPrivLevelReq->Privilege > pChUser->AccessLimit))
    {
        TDBG ("AppDevice.c : SetSession Privilege - Privilege exceeded Session/Channel/User limit\n");
        pSetSesPrivLevelRes->CompletionCode = CC_SET_SESS_PREV_REQ_PRIVILEGE_EXCEEDS_LIMIT;
        UNLOCK_BMC_SHARED_MEM(BMCInst);
        return sizeof (*pRes);/*Requested Privilege exceeds Session privilege Limit*/
    }

    /* According to the IPMI 2.0 Priv Level =1 is reserved */
    if((PRIV_USER      != pSetSesPrivLevelReq->Privilege) &&
            (PRIV_OPERATOR  != pSetSesPrivLevelReq->Privilege) &&
            (PRIV_ADMIN     != pSetSesPrivLevelReq->Privilege) &&
            (PRIV_OEM     != pSetSesPrivLevelReq->Privilege))
    {
        /*Requested Privilege exceeds ChannelPrivilege Limit */
        pSetSesPrivLevelRes->CompletionCode = CC_INV_DATA_FIELD;
        TDBG ("AppDevice.c : SetSession Privilege - Invalid Privilage\n");
        UNLOCK_BMC_SHARED_MEM(BMCInst);
        return sizeof (*pRes);
    }

    /* set the privilege for the session */
    pSessInfo->Privilege = pSetSesPrivLevelReq->Privilege;

    pSetSesPrivLevelRes->CompletionCode = CC_NORMAL;
    pSetSesPrivLevelRes->Privilege      = pSessInfo->Privilege;

    UNLOCK_BMC_SHARED_MEM(BMCInst);

    return sizeof (SetSesPrivLevelRes_T);
}


/*---------------------------------------
 * CloseSession
 *---------------------------------------*/
    int
CloseSession (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  CloseSesReq_T*  pCloseSesReq = (_NEAR_ CloseSesReq_T*)pReq;
    _FAR_   SessionInfo_T*  pSessInfo;
    INT32U          SessionID;

    *pRes = CC_REQ_INV_LEN;

    if ((4 != ReqLen) && (5 != ReqLen))
    {
        return sizeof (*pRes);
    }

    if (0 == pCloseSesReq->SessionID)
    {
        if (5 != ReqLen)
        {
            return sizeof (*pRes);
        }
        SessionID = (INT32U)pCloseSesReq->SessionHandle;
        *pRes = CC_CLOSE_INVALID_SESSION_ID_HANDLE;
        pSessInfo = getSessionInfo (SESSION_HANDLE_INFO, &SessionID, BMCInst);
    }
    else
    {
        if (4 != ReqLen)
        {
            return sizeof (*pRes);
        }
        SessionID = pCloseSesReq->SessionID;
        *pRes = CC_CLOSE_INVALID_SESSION_ID;
        pSessInfo = getSessionInfo (SESSION_ID_INFO, &SessionID, BMCInst);
    }

    if ((NULL == pSessInfo) || (0 == pSessInfo->Activated))
    {
        return sizeof (*pRes);
    }

    /* The SessionInfo is deleted form session table  from interface */
    //    DeleteSession(pSessInfo, BMCInst); /*Garden: or else, no next time*/
    *pRes = CC_NORMAL;

    return sizeof (*pRes);
}

#if GET_SESSION_INFO != UNIMPLEMENTED
/*---------------------------------------
 * GetSessionInfo
 *---------------------------------------*/
    int
GetSessionInfo (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  GetSesInfoReq_T* pGetSesInfoReq = (_NEAR_ GetSesInfoReq_T*)pReq;
    _NEAR_  GetSesInfoRes_T* pGetSesInfoRes = (_NEAR_ GetSesInfoRes_T*)pRes;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    LANSesInfoRes_T     LANSesInfo;
    LANIPv6SesInfoRes_T     LANIPv6SesInfo;
    _FAR_   SessionInfo_T*   pSessInfo;
    _FAR_   ChannelInfo_T*   pChannelInfo;
    _FAR_   void*            SessionArg;
    INT8U 	     SessionArgAlign[4];
    INT8U            SessionArgType,EthIndex, netindex = 0xFF;
    char    IfcName[IFNAMSIZ];
    int i;

    *pRes = CC_REQ_INV_LEN;

    switch (pGetSesInfoReq->SessionIndex)
    {
        case 0:
            /* Get session information for this session */
            if (1 != ReqLen)
            {
                return sizeof (*pRes);
            }
            SessionArgType  = SESSION_ID_INFO;
            SessionArg      = &g_BMCInfo[BMCInst].Msghndlr.CurSessionID;
            break;

        case 0xFF:
            if (5 != ReqLen)
            {
                return sizeof (*pRes);
            }
            SessionArgType  = SESSION_ID_INFO;

            SessionArgAlign[0]      = pGetSesInfoReq->SessionHandleOrID[0];
            SessionArgAlign[1]      = pGetSesInfoReq->SessionHandleOrID[1];
            SessionArgAlign[2]      = pGetSesInfoReq->SessionHandleOrID[2];
            SessionArgAlign[3]      = pGetSesInfoReq->SessionHandleOrID[3];
            SessionArg = SessionArgAlign;
            break;

        case 0xFE:
            if (2 != ReqLen)
            {
                return sizeof (*pRes);
            }
            if (pGetSesInfoReq->SessionHandleOrID[0] == 0)
            {
                *pRes=CC_INV_DATA_FIELD;
                return sizeof (*pRes);
            }
            SessionArgType  = SESSION_HANDLE_INFO;
            SessionArgAlign[0]      = pGetSesInfoReq->SessionHandleOrID[0];
            SessionArgAlign[1]      = pGetSesInfoReq->SessionHandleOrID[1];
            SessionArgAlign[2]      = pGetSesInfoReq->SessionHandleOrID[2];
            SessionArgAlign[3]      = pGetSesInfoReq->SessionHandleOrID[3];
            SessionArg = SessionArgAlign;
            break;

        default:
            if (1 != ReqLen)
            {
                return sizeof (*pRes);
            }
            SessionArgType  = SESSION_INDEX_INFO;
            SessionArg      = &pGetSesInfoReq->SessionIndex;
            break;
    }

    pSessInfo = getSessionInfo (SessionArgType, SessionArg, BMCInst);
    if (NULL == pSessInfo)
    {
        TDBG ("GetSessionInfo: pSessInfo is NULL\n");
        /* if there is no active channel for the current session Index
         * return the following bytes
         */

        pChannelInfo = getChannelInfo (g_BMCInfo[BMCInst].Msghndlr.CurChannel  & 0xF, BMCInst);
        if(NULL == pChannelInfo)
        {
            pGetSesInfoRes->CompletionCode = CC_INV_DATA_FIELD;
            return	sizeof (*pRes);
        }

        pGetSesInfoRes->CompletionCode          = CC_NORMAL;
        pGetSesInfoRes->SessionHandle           = BMC_GET_SHARED_MEM (BMCInst)->SessionHandle;
        pGetSesInfoRes->NumPossibleActiveSession=  pBMCInfo->IpmiConfig.MaxSession;
        pGetSesInfoRes->NumActiveSession        = GetNumOfActiveSessions (BMCInst);

        return (sizeof (GetSesInfoRes_T) - sizeof (ActiveSesInfo_T) - sizeof (SessionInfoRes_T));
    }

    memset (pGetSesInfoRes,0,sizeof(GetSesInfoRes_T));
    LOCK_BMC_SHARED_MEM(BMCInst);
    pChannelInfo = getChannelInfo (pSessInfo->Channel, BMCInst);
    if(NULL == pChannelInfo)
    {
        pGetSesInfoRes->CompletionCode = CC_INV_DATA_FIELD;
        return	sizeof (*pRes);
    }
    pGetSesInfoRes->CompletionCode              = CC_NORMAL;
    pGetSesInfoRes->SessionHandle               = pSessInfo->SessionHandle;
    pGetSesInfoRes->NumPossibleActiveSession    = pBMCInfo->IpmiConfig.MaxSession;
    pGetSesInfoRes->NumActiveSession            = GetNumOfActiveSessions (BMCInst);
    pGetSesInfoRes->ActiveSesinfo.UserID        = pSessInfo->UserId & 0x3F;
    pGetSesInfoRes->ActiveSesinfo.Privilege     = pSessInfo->Privilege & 0x0F;
    /* Set protocol bit as per Auth Type, bit4 must be 1 for IPMIv2.0 RMCP, 0 for IPMIv1.5 */
    if( AUTHTYPE_RMCP_PLUS_FORMAT == pSessInfo->AuthType )
    {
        pGetSesInfoRes->ActiveSesinfo.ChannelNum    = (pSessInfo->Channel & 0x0F) | 0x10;
    }else
    {
        pGetSesInfoRes->ActiveSesinfo.ChannelNum    = pSessInfo->Channel & 0x0F;
    }
    EthIndex= GetEthIndex(pSessInfo->Channel & 0x0F, BMCInst);
    if(0xff == EthIndex)
    {
        *pRes = CC_INV_DATA_FIELD;
        DeleteSession (pSessInfo,BMCInst);
        return sizeof (INT8U);
    }
    memset(IfcName,0,sizeof(IfcName));
    /*Get the EthIndex*/
    printf("%s,%d---------------------------------------hhhhhhhhhhhhhhhhhhhhhhhhhh\n",__func__,__LINE__);
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
    if (IsLANChannel( pSessInfo->Channel, BMCInst))
    {
        if(g_corefeatures.global_ipv6  == ENABLED)
        {
            if(GetIPAdrressType(&pSessInfo->LANRMCPPkt.IPHdr.Srcv6Addr[0])==4)
            {
                memset(&LANSesInfo,0,sizeof(LANSesInfo));
                /* IP Address */
                _fmemcpy (&(LANSesInfo.IPAddress),
                        &pSessInfo->LANRMCPPkt.IPHdr.Srcv6Addr[sizeof(struct in6_addr)-sizeof(struct in_addr)],
                        sizeof(struct in_addr));
                /* MAC Address */
                if(pSessInfo->LANRMCPPkt.MACHdr.DestAddr[0] == 0)
                    nwGetSrcCacheMacAddr((INT8U*)&pSessInfo->LANRMCPPkt.IPHdr.Srcv6Addr[12],
                            netindex, pSessInfo->LANRMCPPkt.MACHdr.DestAddr);

                _fmemcpy(LANSesInfo.MACAddress, pSessInfo->LANRMCPPkt.MACHdr.DestAddr, MAC_ADDR_LEN);
                /* Port number */
                LANSesInfo.PortNumber = pSessInfo->LANRMCPPkt.UDPHdr.SrcPort;
                UNLOCK_BMC_SHARED_MEM(BMCInst);

                _fmemcpy ((pRes+sizeof (GetSesInfoRes_T) - sizeof (SessionInfoRes_T)),&LANSesInfo, sizeof (LANSesInfoRes_T));
                return (sizeof (GetSesInfoRes_T) - sizeof (SessionInfoRes_T)+sizeof (LANSesInfoRes_T));
            }
            else
            {
                /* IP Address */
                memset(&LANIPv6SesInfo,0,sizeof(LANIPv6SesInfo));
                _fmemcpy (&(LANIPv6SesInfo.IPv6Address),
                        pSessInfo->LANRMCPPkt.IPHdr.Srcv6Addr,
                        sizeof(struct in6_addr));
                /* MAC Address */
                if(pSessInfo->LANRMCPPkt.MACHdr.DestAddr[0] == 0)
                    nwGetSrcMacAddr_IPV6((INT8U*)&pSessInfo->LANRMCPPkt.IPHdr.Srcv6Addr,
                            pSessInfo->LANRMCPPkt.MACHdr.DestAddr);

                _fmemcpy(LANIPv6SesInfo.MACAddress, pSessInfo->LANRMCPPkt.MACHdr.DestAddr, MAC_ADDR_LEN);
                /* Port number */
                LANIPv6SesInfo.PortNumber = pSessInfo->LANRMCPPkt.UDPHdr.SrcPort;
                UNLOCK_BMC_SHARED_MEM(BMCInst);

                _fmemcpy ((pRes+sizeof (GetSesInfoRes_T) - sizeof(SessionInfoRes_T)),&LANIPv6SesInfo, sizeof (LANIPv6SesInfoRes_T));
                return (sizeof (GetSesInfoRes_T) - sizeof (SessionInfoRes_T)+sizeof (LANIPv6SesInfoRes_T));
            }
        }
        else
        {

            /* IP Address */
            _fmemcpy (pGetSesInfoRes->SesInfo.LANSesInfo.IPAddress,
                    pSessInfo->LANRMCPPkt.IPHdr.Srcv4Addr,
                    sizeof (pSessInfo->LANRMCPPkt.IPHdr.Srcv4Addr));
            /* MAC Address */
            if(pSessInfo->LANRMCPPkt.MACHdr.DestAddr[0] == 0)
                nwGetSrcCacheMacAddr((INT8U*)pSessInfo->LANRMCPPkt.IPHdr.Srcv4Addr, netindex,
                        pSessInfo->LANRMCPPkt.MACHdr.DestAddr);
            _fmemcpy(pGetSesInfoRes->SesInfo.LANSesInfo.MACAddress,
                    pSessInfo->LANRMCPPkt.MACHdr.DestAddr, MAC_ADDR_LEN);
            /* Port number */
            pGetSesInfoRes->SesInfo.LANSesInfo.PortNumber = pSessInfo->LANRMCPPkt.UDPHdr.SrcPort;
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            return (sizeof (GetSesInfoRes_T) - sizeof (SessionInfoRes_T) + sizeof (LANSesInfoRes_T));
        }
    }
    else if (pBMCInfo->IpmiConfig.SerialIfcSupport == 1 && pBMCInfo->SERIALch== pSessInfo->Channel)
    {

        pChannelInfo = getChannelInfo (pBMCInfo->SERIALch, BMCInst);
        if(NULL == pChannelInfo)
        {
            pGetSesInfoRes->CompletionCode = CC_INV_DATA_FIELD;
            return	sizeof (*pRes);
        }

        pGetSesInfoRes->SesInfo.SerialSesInfo.SessionActivityType = 0;
        pGetSesInfoRes->SesInfo.SerialSesInfo.DestinationSelector = 0;

        UNLOCK_BMC_SHARED_MEM(BMCInst);
        return (sizeof (GetSesInfoRes_T) - sizeof (SessionInfoRes_T) + sizeof (SerialSesInfoRes_T));
    }
    else
    {
        pChannelInfo = getChannelInfo(pSessInfo->Channel, BMCInst);
        if(NULL == pChannelInfo)
        {
            pGetSesInfoRes->CompletionCode = CC_INV_DATA_FIELD;
            return sizeof (*pRes);
        }

        UNLOCK_BMC_SHARED_MEM(BMCInst);
        return (sizeof (GetSesInfoRes_T) - sizeof (SessionInfoRes_T));
    }
}
#endif


/**
 * @fn GetAuthCodeForTypeV15
 * @brief This function will use the encryption technique supported
 * 			in IPMI V1.5 in order to produce Auth Code.
 * @param[in] pUserInfo - Pointer to User info structure.
 * @param[in] pGetAuthCodeReq - Pointer to the structure of request data
 * @param[out] pGetAuthCodeRes - Pointer to the resultant data.
 * @retval size of the result data.
 */
    static int
GetAuthCodeForTypeV15 (UserInfo_T* pUserInfo,
        GetAuthCodeReq_T* pGetAuthCodeReq,
        GetAuthCodeRes_T* pGetAuthCodeRes,int BMCInst)
{
    INT8U   AuthCode;
    INT8U   InBuffer[2*IPMI15_MAX_PASSWORD_LEN + HASH_DATA_LENGTH];
    char    UserPswd[MAX_PASSWD_LEN];
    INT8U   PwdEncKey[MAX_SIZE_KEY] = {0};
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    memset(&(pGetAuthCodeRes->AuthCode), 0, AUTH_CODE_HASH_LEN);
    AuthCode = pGetAuthCodeReq->AuthType & AUTH_CODE_V15_MASK;
    if((pGetAuthCodeReq->AuthType & (BIT5 | BIT4)) ||
            (AuthCode == AUTHTYPE_RESERVED) || (AuthCode == AUTHTYPE_NONE) ||
            (AuthCode == AUTHTYPE_STRAIGHT_PASSWORD) ||
            (AuthCode > AUTHTYPE_OEM_PROPRIETARY))
    {
        pGetAuthCodeRes->CompletionCode = CC_INV_DATA_FIELD;
        return (sizeof (GetAuthCodeRes_T) - 4);
    }
#if 0
    if (g_corefeatures.userpswd_encryption == ENABLED)
    {
        if(getEncryptKey(PwdEncKey))
        {
            TCRIT("Unable to get the encryption key. quitting...\n");
            pGetAuthCodeRes->CompletionCode = CC_UNSPECIFIED_ERR;
            return sizeof(*pGetAuthCodeRes);
        }
        if(DecryptPassword((INT8S *)(pBMCInfo->EncryptedUserInfo[pUserInfo->UserId - 1].EncryptedPswd), MAX_PASSWORD_LEN, UserPswd, MAX_PASSWORD_LEN, PwdEncKey))
        {
            TCRIT("Error in decrypting the IPMI User password for User ID:%d.\n", pUserInfo->UserId);
            pGetAuthCodeRes->CompletionCode = CC_UNSPECIFIED_ERR;
            return sizeof(*pGetAuthCodeRes);
        }
        memcpy(&InBuffer[0],UserPswd,IPMI15_MAX_PASSWORD_LEN);
    }
    else
#endif
    {
        memcpy(&InBuffer[0],pUserInfo->UserPassword,IPMI15_MAX_PASSWORD_LEN);
    }

    LOCK_BMC_SHARED_MEM(BMCInst);
    switch (AuthCode)
    {
#if 0 // As per IPMIv2 Markup E4, Straight Password key is Reserved
        case AUTH_TYPE_PASSWORD: /* Straight password */
            if (0 == _fmemcmp (pUserInfo->UserPassword,pGetAuthCodeReq->HashData,IPMI15_MAX_PASSWORD_LEN))
            {
                _fmemcpy (pGetAuthCodeRes->AuthCode, "OK", 2);
            }
            else
            {
                pGetAuthCodeRes->CompletionCode = CC_INV_DATA_FIELD;
            }
            break;
#endif

        case AUTH_TYPE_MD2: /* MD2 */
#if 0
            _fmemcpy (&InBuffer[IPMI15_MAX_PASSWORD_LEN],
                    pGetAuthCodeReq->HashData, HASH_DATA_LENGTH);
            if (g_corefeatures.userpswd_encryption == ENABLED)
            {
                _fmemcpy(&InBuffer[IPMI15_MAX_PASSWORD_LEN+HASH_DATA_LENGTH],
                        UserPswd, IPMI15_MAX_PASSWORD_LEN);
            }
            else
            {
                _fmemcpy(&InBuffer[IPMI15_MAX_PASSWORD_LEN+HASH_DATA_LENGTH],
                        pUserInfo->UserPassword, IPMI15_MAX_PASSWORD_LEN);
            }

            AuthCodeCalMD2 (InBuffer, pGetAuthCodeRes->AuthCode, sizeof (InBuffer));
#endif
            break;

        case AUTH_TYPE_MD5: /* MD5 */
            _fmemcpy (&InBuffer[IPMI15_MAX_PASSWORD_LEN],
                    pGetAuthCodeReq->HashData, HASH_DATA_LENGTH);
            if (g_corefeatures.userpswd_encryption == ENABLED)
            {
                _fmemcpy(&InBuffer[IPMI15_MAX_PASSWORD_LEN+HASH_DATA_LENGTH],
                        UserPswd,IPMI15_MAX_PASSWORD_LEN);
            }
            else
            {
                _fmemcpy(&InBuffer[IPMI15_MAX_PASSWORD_LEN+HASH_DATA_LENGTH],
                        pUserInfo->UserPassword,IPMI15_MAX_PASSWORD_LEN);
            }
            AuthCodeCalMD5 (InBuffer, pGetAuthCodeRes->AuthCode, sizeof (InBuffer));
            break;

        default:
            pGetAuthCodeRes->CompletionCode = CC_INV_DATA_FIELD;
    }

    UNLOCK_BMC_SHARED_MEM(BMCInst);
    // IPMI V1.5 AuthCode is only 16 byte.
    return (sizeof (GetAuthCodeRes_T) - 4);
}

/**
 * @fn GetAuthCodeForTypeV20
 * @brief This function will use the encryption technique supported
 * 			in IPMI V2.0 in order to produce Auth Code.
 * @param[in] pUserInfo - Pointer to User info structure.
 * @param[in] pGetAuthCodeReq - Pointer to the structure of request data
 * @param[out] pGetAuthCodeRes - Pointer to the resultant data.
 * @retval size of the result data.
 */
    static int
GetAuthCodeForTypeV20 (UserInfo_T* pUserInfo,
        GetAuthCodeReq_T* pGetAuthCodeReq,
        GetAuthCodeRes_T* pGetAuthCodeRes,int BMCInst)
{
    INT8U   AuthCode;
    INT8U   UserPasswdLen=0;
    INT8U   DecVal = 0;
    _FAR_ INT8U m_SIK [SESSION_INTEGRITY_KEY_SIZE];
    char   UserPswd[MAX_PASSWD_LEN];
    unsigned char PwdEncKey[MAX_SIZE_KEY] = {0};
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
#if 0
    if (g_corefeatures.userpswd_encryption == ENABLED)
    {
        if(getEncryptKey(PwdEncKey))
        {
            TCRIT("Unable to get the encryption key. quitting...\n");
            pGetAuthCodeRes->CompletionCode = CC_UNSPECIFIED_ERR;
            return sizeof (*pGetAuthCodeRes);
        }
        if(DecryptPassword((INT8S *)(pBMCInfo->EncryptedUserInfo[pUserInfo->UserId - 1].EncryptedPswd), MAX_PASSWORD_LEN, UserPswd, MAX_PASSWORD_LEN, PwdEncKey))
        {
            TCRIT("\nError in decrypting the IPMI User password for user with ID:%d.\n", pUserInfo->UserId);
            pGetAuthCodeRes->CompletionCode = CC_UNSPECIFIED_ERR;
            return sizeof (*pGetAuthCodeRes);
        }
    }
    else
#endif
    {
        memcpy(&UserPswd,pUserInfo->UserPassword,MAX_PASSWD_LEN);
    }

    /* Calculate password length */
    UserPasswdLen = _fstrlen ((_FAR_ char*)UserPswd);
    UserPasswdLen = (UserPasswdLen > MAX_PASSWORD_LEN) ?
        MAX_PASSWORD_LEN : UserPasswdLen;

    memset(&(pGetAuthCodeRes->AuthCode), 0, AUTH_CODE_HASH_LEN);
    AuthCode = pGetAuthCodeReq->AuthType & AUTH_CODE_V20_MASK;
    /* Validate the Auth Code */
    if ((AuthCode > MIN_AUTH_CODE_V20) && (AuthCode < MAX_AUTH_CODE_V20))
    {
        pGetAuthCodeRes->CompletionCode = CC_INV_DATA_FIELD;
        return sizeof(GetAuthCodeRes_T);
    }

    LOCK_BMC_SHARED_MEM(BMCInst);
    switch(AuthCode)
    {
        case AUTH_NONE: /* none */
            TDBG ("\nInside AUTH_NONE in GetAuthCode");
            pGetAuthCodeRes->CompletionCode = CC_INV_DATA_FIELD;
            break;

        case AUTH_HMAC_SHA1_96: /* HMAC-SHA1-96 */
            TDBG ("\nInside AUTH_HMAC_SHA1_96 in GetAuthCode");
            //            hmac_sha1 ((char *)UserPswd, UserPasswdLen,
            //                    (char *)pGetAuthCodeReq->HashData, HASH_DATA_LEN,
            //                    (char *)m_SIK, SESSION_INTEGRITY_KEY_SIZE);

            //            hmac_sha1 ((char *)m_SIK, SESSION_INTEGRITY_KEY_SIZE,
            //                    (char *)pGetAuthCodeReq->HashData, HASH_DATA_LEN,
            //                    (char *)&(pGetAuthCodeRes->AuthCode), HMAC_SHA1_96_LEN);

            DecVal = AUTH_CODE_HASH_LEN - HMAC_SHA1_96_LEN;
            break;

        case AUTH_HMAC_MD5_128: /* HMAC-MD5-128 */
            TDBG ("\nInside AUTH_HMAC_MD5_128 in GetAuthCode");
            //            hmac_md5 ((unsigned char*)UserPswd, UserPasswdLen,
            //                    pGetAuthCodeReq->HashData, HASH_DATA_LEN,
            //                    m_SIK, SESSION_HMAC_MD5_I_KEY_SIZE);

            //            hmac_md5 (m_SIK, SESSION_HMAC_MD5_I_KEY_SIZE,
            //                    pGetAuthCodeReq->HashData, HASH_DATA_LEN,
            //                    (unsigned char *)&(pGetAuthCodeRes->AuthCode),
            //                    SESSION_HMAC_MD5_I_KEY_SIZE);
            DecVal = AUTH_CODE_HASH_LEN - SESSION_HMAC_MD5_I_KEY_SIZE;
            break;

        case AUTH_MD5_128: /* MD5-128 */
            TDBG ("\nInside AUTH_MD5_128 in GetAuthCode");
            //            MD5_128 ((char *)UserPswd, UserPasswdLen,
            //                    (char *)pGetAuthCodeReq->HashData, HASH_DATA_LEN,
            //                    (char *)m_SIK, SESSION_MD5_KEY_SIZE);

            //            MD5_128 ((char *)m_SIK, SESSION_INTEGRITY_KEY_SIZE,
            //                    (char *)pGetAuthCodeReq->HashData, HASH_DATA_LEN,
            //                    (char *)&(pGetAuthCodeRes->AuthCode), SESSION_MD5_KEY_SIZE);
            DecVal = AUTH_CODE_HASH_LEN - SESSION_MD5_KEY_SIZE;
            break;

        case AUTH_HMAC_SHA256_128: /* HMAC-SHA256-128 */
            TDBG ("\nInside AUTH_HMAC_SHA256_128 in GetAuthCode");
            //            hmac_sha256 ((unsigned char *)UserPswd, UserPasswdLen,
            //    				(unsigned char *)pGetAuthCodeReq->HashData, HASH_DATA_LEN,
            //    				(unsigned char *)m_SIK, SHA2_HASH_KEY_SIZE);

            //			hmac_sha256 (m_SIK, SHA2_HASH_KEY_SIZE,
            //    				(unsigned char *)pGetAuthCodeReq->HashData, HASH_DATA_LEN,
            //    				(unsigned char *)&(pGetAuthCodeRes->AuthCode), HMAC_SHA256_128_LEN);
            DecVal = AUTH_CODE_HASH_LEN - HMAC_SHA256_128_LEN;
            break;

            //! TODO: Need support in openssl.
        default: /* OEM or Reserved */
            pGetAuthCodeRes->CompletionCode = CC_INV_DATA_FIELD;
    }

    UNLOCK_BMC_SHARED_MEM(BMCInst);
    if (DecVal > sizeof(GetAuthCodeRes_T))
        return sizeof(GetAuthCodeRes_T);
    else
        return (sizeof(GetAuthCodeRes_T) - DecVal);
}

/**
 * @fn GetAuthCode
 * @brief This function will encrypt the given 16 byte data with
 *          the algorithm given and return Auth Code.
 * @param[in] pReq - Request data.
 * @param[in] ReqLen - Length of the request data.
 * @param[out] pRes - Result data
 * @retval size of the result data.
 */
    int
GetAuthCode (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,int BMCInst)
{
    _NEAR_  GetAuthCodeReq_T*   pGetAuthCodeReq = (_NEAR_ GetAuthCodeReq_T*)pReq;
    _NEAR_  GetAuthCodeRes_T*   pGetAuthCodeRes = (_NEAR_ GetAuthCodeRes_T*)pRes;
    _FAR_   UserInfo_T*         pUserInfo;
    INT8U               AuthType;
    _FAR_   ChannelInfo_T*      pChannelInfo;
    int nRetSize = 0;

    /* Check for Reserved Bits */
    if((pGetAuthCodeReq->ChannelNum & (BIT7 | BIT6 | BIT5 | BIT4)) ||
            (pGetAuthCodeReq->UserID & (BIT7 | BIT6)))
    {
        pGetAuthCodeRes->CompletionCode = CC_INV_DATA_FIELD;
        return sizeof(*pRes);
    }

    /* Validate the channel number given */
    if (CURRENT_CHANNEL_NUM == pGetAuthCodeReq->ChannelNum)
        pChannelInfo = getChannelInfo(g_BMCInfo[BMCInst].Msghndlr.CurChannel  & 0xF,BMCInst);
    else
        pChannelInfo = getChannelInfo(pGetAuthCodeReq->ChannelNum,BMCInst);

    if (NULL == pChannelInfo)
    {
        *pRes = CC_INV_DATA_FIELD;
        return sizeof(*pRes);
    }

    /* Get the user information for the given userID */
    pUserInfo = getUserIdInfo (pGetAuthCodeReq->UserID,BMCInst);
    if (NULL == pUserInfo)
    {
        TDBG ("AppDevice.c : GetAuthCode - Invalid user Id\n");
        pGetAuthCodeRes->CompletionCode = CC_INV_DATA_FIELD;
        return sizeof (*pRes);
    }

    AuthType = pGetAuthCodeReq->AuthType & GET_AUTH_TYPE_MASK;
    pGetAuthCodeRes->CompletionCode = CC_NORMAL;

    switch(AuthType)
    {
        case AUTH_TYPE_V15: /* IPMI v1.5 AuthCode Algorithms */
            nRetSize = GetAuthCodeForTypeV15(pUserInfo, pGetAuthCodeReq, pGetAuthCodeRes,BMCInst);
            break;

        case AUTH_TYPE_V20: /* IPMI v2.0/RMCP+ Algorithm Number */
            nRetSize = GetAuthCodeForTypeV20(pUserInfo, pGetAuthCodeReq, pGetAuthCodeRes,BMCInst);
            break;

        default:
            pGetAuthCodeRes->CompletionCode = CC_INV_DATA_FIELD;
            return sizeof(*pRes);
    }
    return nRetSize;
}


/*---------------------------------------
 * SetChAccess
 *---------------------------------------*/
    int
SetChAccess (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  SetChAccessReq_T*   pSetChAccessReq = (_NEAR_ SetChAccessReq_T*)pReq;
    INT8U               ChannelNum, AccessMode;
    _FAR_   ChannelInfo_T*      pNvrChInfo;
    _FAR_   ChannelInfo_T*      pChannelInfo;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    INT8U AccessFlags = 0,i=0;
    //    char ChFilename[MAX_CHFILE_NAME];

    /* Reserve Bit Checking for Set Channel Acces */
    for(i=0;i<ReqLen;i++)
    {
        if( 0 != (pReq[i] & m_Set_ChReserveBit[i]))
        {
            *pRes = CC_INV_DATA_FIELD;
            return	sizeof (*pRes);
        }
    }
    /* Reserve Value checking */

    if((pReq[1] & 0xC0)== 0xC0 || (pReq[2] & 0xC0) == 0xC0)
    {
        *pRes = CC_INV_DATA_FIELD;
        return	sizeof (*pRes);
    }

    ChannelNum = pSetChAccessReq->ChannelNum & 0x0F;
    if (CURRENT_CHANNEL_NUM == ChannelNum)
    {
        ChannelNum = g_BMCInfo[BMCInst].Msghndlr.CurChannel  & 0xF;
    }

    pChannelInfo = getChannelInfo(ChannelNum, BMCInst);
    if (NULL == pChannelInfo)
    {
        *pRes = CC_INV_DATA_FIELD;
        return  sizeof (*pRes);
    }

    if (((GetBits (pSetChAccessReq->Privilege, 0x0F)) < PRIV_LEVEL_CALLBACK) ||
            ((GetBits (pSetChAccessReq->Privilege, 0x0F)) > PRIV_LEVEL_PROPRIETARY))
    {
        IPMI_DBG_PRINT_1 ("Invalid Channel Privilege = 0x%x\n", pSetChAccessReq->Privilege);
        *pRes = CC_INV_DATA_FIELD;
        return  sizeof (*pRes);
    }

    /*point to NVRAM ChannelInfo */
    pNvrChInfo = GetNVRChConfigs(pChannelInfo,BMCInst);

    if (SESSIONLESS_CHANNEL == pChannelInfo->SessionSupport)
    {
        /* Channel is sessionless channel this command is not supported */
        *pRes = CC_SET_CH_COMMAND_NOT_SUPPORTED;
        return  sizeof (*pRes);
    }


    AccessMode = pSetChAccessReq->ChannelAccess & 0x07;
    if(ChannelNum == pBMCInfo->SERIALch)
    {
        //        if( NULL != g_PDKHandle[PDK_SWITCHMUX])
        //        {
        //            ((void(*)(INT8U, int))(g_PDKHandle[PDK_SWITCHMUX]))(AccessMode ,BMCInst);
        //        }
    }

    /* if the requested access mode is supported for the given channel */
    if (0 == (pChannelInfo->AccessModeSupported & (1 << AccessMode)))
    {
        *pRes = CC_SET_CH_ACCES_MODE_NOT_SUPPORTED;
        return  sizeof (*pRes);
    }

    AccessFlags = GetBits (pSetChAccessReq->ChannelAccess, 0xC0);

    switch (AccessFlags)
    {
        case 0:
            /* dont set channel access */
            break;

        case 1:
            /*set in  Non volatile Memory and in voatile Memory */
            LOCK_BMC_SHARED_MEM(BMCInst);
            pNvrChInfo->Alerting        = GetBits (pSetChAccessReq->ChannelAccess , 0x20);
            pNvrChInfo->PerMessageAuth  = GetBits (pSetChAccessReq->ChannelAccess , 0x10);
            pNvrChInfo->UserLevelAuth   = GetBits (pSetChAccessReq->ChannelAccess , 0x08);
            pNvrChInfo->AccessMode = AccessMode;
            /* write to NVRAM   */
            //            FlushChConfigs((INT8U*)pNvrChInfo,pNvrChInfo->ChannelNumber,BMCInst);
            pChannelInfo->Alerting      = pNvrChInfo->Alerting;
            pChannelInfo->PerMessageAuth= pNvrChInfo->PerMessageAuth;
            pChannelInfo->UserLevelAuth = pNvrChInfo->UserLevelAuth;
            pChannelInfo->AccessMode = AccessMode;
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            break;

        case 2:
            /*set in volatile Memmory only      */
            LOCK_BMC_SHARED_MEM(BMCInst);
            pChannelInfo->Alerting      = GetBits (pSetChAccessReq->ChannelAccess, 0x20);
            pChannelInfo->PerMessageAuth= GetBits (pSetChAccessReq->ChannelAccess, 0x10);
            pChannelInfo->UserLevelAuth = GetBits (pSetChAccessReq->ChannelAccess, 0x08);
            pChannelInfo->AccessMode = AccessMode;
            UNLOCK_BMC_SHARED_MEM(BMCInst);
    }

    switch (GetBits (pSetChAccessReq->Privilege, 0xC0))
    {
        case 0:
            /* dont set prilivege level */
            break;

        case 1:
            /* set in non volatile mem  and volatile  memeory*/ /*set privilege*/
            pNvrChInfo->MaxPrivilege = GetBits (pSetChAccessReq->Privilege, 0x0F);
            pChannelInfo->MaxPrivilege = pNvrChInfo->MaxPrivilege;
            break;

        case 2:
            /*set privilege*/
            /*  set in volatile memeory only    */
            pChannelInfo->MaxPrivilege = GetBits (pSetChAccessReq->Privilege, 0x0F);
    }

    *pRes = CC_NORMAL;

    return sizeof (*pRes);
}


/*---------------------------------------
 * GetChAccess
 *---------------------------------------*/
    int
GetChAccess (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  GetChAccessReq_T*   pGetChAccessReq = (_NEAR_ GetChAccessReq_T*)pReq;
    _NEAR_  GetChAccessRes_T*   pGetChAccessRes = (_NEAR_ GetChAccessRes_T*)pRes;
    _FAR_   ChannelInfo_T*      pChannelInfo;
    _FAR_   ChannelInfo_T*      pNvrChInfo;
    INT8U               ChannelNum,AccessFlag;

    /* Check for reserved bits */
    if((0 != (pGetChAccessReq->ChannelNum & 0xf0)) ||
            (0 != (pGetChAccessReq->	AccessFlag & 0x3f)))
    {
        pGetChAccessRes->CompletionCode = CC_INV_DATA_FIELD;
        return  sizeof (*pRes);
    }

    ChannelNum = pGetChAccessReq->ChannelNum & 0x0F;
    if (CURRENT_CHANNEL_NUM == ChannelNum)
    {
        ChannelNum = g_BMCInfo[BMCInst].Msghndlr.CurChannel  & 0xF;
    }

    pChannelInfo = getChannelInfo (ChannelNum, BMCInst);

    if (NULL == pChannelInfo)
    {
        pGetChAccessRes->CompletionCode = CC_INV_DATA_FIELD;
        return  sizeof (*pRes);
    }

    if (SESSIONLESS_CHANNEL == pChannelInfo->SessionSupport)
    {
        /* Channel is sessionless channel this command is not supported */
        pGetChAccessRes->CompletionCode = CC_GET_CH_COMMAND_NOT_SUPPORTED;
        return  sizeof (*pRes);
    }


    /* check for the reserved bits */
    if ( 0 !=  ( pGetChAccessReq->AccessFlag & 0x3F ) )
    {
        pGetChAccessRes->CompletionCode = CC_INV_DATA_FIELD;
        return  sizeof (*pRes);
    }

    AccessFlag = GetBits (pGetChAccessReq->AccessFlag, 0xC0);

    pGetChAccessRes->CompletionCode = CC_NORMAL;

    switch (AccessFlag)
    {
        case 1:
            /*  Get Channel Information from NVRAM */
            pNvrChInfo = GetNVRChConfigs(pChannelInfo,BMCInst);

            LOCK_BMC_SHARED_MEM(BMCInst);
            pGetChAccessRes->ChannelAccess  = SetBits (0x20, pNvrChInfo->Alerting);
            pGetChAccessRes->ChannelAccess |= SetBits (0x10, pNvrChInfo->PerMessageAuth);
            pGetChAccessRes->ChannelAccess |= SetBits (0x08, pNvrChInfo->UserLevelAuth);
            pGetChAccessRes->ChannelAccess |= SetBits (0x07, pNvrChInfo->AccessMode);
            pGetChAccessRes->Privilege      = SetBits (0x0F, pNvrChInfo->MaxPrivilege);
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            break;

        case 2:
            /*  Get  Channel Information from  Volatile RAM */
            LOCK_BMC_SHARED_MEM(BMCInst);
            pGetChAccessRes->ChannelAccess  = SetBits (0x20, pChannelInfo->Alerting);
            pGetChAccessRes->ChannelAccess |= SetBits (0x10, pChannelInfo->PerMessageAuth);
            pGetChAccessRes->ChannelAccess |= SetBits (0x08, pChannelInfo->UserLevelAuth);
            pGetChAccessRes->ChannelAccess |= SetBits (0x07, pChannelInfo->AccessMode);
            pGetChAccessRes->Privilege      = SetBits (0x0F, pChannelInfo->MaxPrivilege);
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            break;

        default:
            pGetChAccessRes->CompletionCode = CC_INV_DATA_FIELD;
            return  sizeof (*pRes);
    }

    return sizeof (GetChAccessRes_T);
}


/*---------------------------------------
 * GetChInfo
 *---------------------------------------*/
    int
GetChInfo (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  GetChInfoReq_T*     pGetChInfoReq = (_NEAR_ GetChInfoReq_T*)pReq;
    _NEAR_  GetChInfoRes_T*     pGetChInfoRes = (_NEAR_ GetChInfoRes_T*)pRes;
    _FAR_   ChannelInfo_T*      pChannelInfo;
    INT8U               ChannelNum;

    if(pGetChInfoReq->ChannelNum & (BIT7 | BIT6 | BIT5 | BIT4)) //Check for Reserved bits
    {
        pGetChInfoRes->CompletionCode = CC_INV_DATA_FIELD;
        return sizeof(*pRes);
    }

    ChannelNum = pGetChInfoReq->ChannelNum;
    if (CURRENT_CHANNEL_NUM == ChannelNum)
    {
        ChannelNum = g_BMCInfo[BMCInst].Msghndlr.CurChannel  & 0xF;

        /* UDS, not being a physical channel, will hold LAN properties */
        if(UDS_CHANNEL == ChannelNum)
        {
            ChannelNum = LAN_RMCP_CHANNEL1_TYPE;
        }
    }

    pChannelInfo = getChannelInfo(ChannelNum, BMCInst);
    if (NULL == pChannelInfo)
    {
        pGetChInfoRes->CompletionCode = CC_INV_DATA_FIELD ;
        return  sizeof (*pRes);
    }

    LOCK_BMC_SHARED_MEM(BMCInst);

    pGetChInfoRes->CompletionCode        = CC_NORMAL;
    pGetChInfoRes->ChannelNum            = ChannelNum;
    pGetChInfoRes->ChannelMedium         = pChannelInfo->ChannelMedium;
    pGetChInfoRes->ChannelProtocol       = pChannelInfo->ChannelProtocol;
    pGetChInfoRes->SessionActiveSupport  = pChannelInfo->SessionSupport << 6;
    pGetChInfoRes->SessionActiveSupport |= pChannelInfo->ActiveSession;

    _fmemcpy (pGetChInfoRes->VendorID, pChannelInfo->ProtocolVendorId,
            sizeof (pGetChInfoRes->VendorID));
    _fmemcpy (pGetChInfoRes->AuxiliaryInfo, pChannelInfo->AuxiliaryInfo,
            sizeof (pGetChInfoRes->AuxiliaryInfo));

    UNLOCK_BMC_SHARED_MEM(BMCInst);

    return sizeof (GetChInfoRes_T);
}

/*---------------------------------------
 * IsChannelSuppGroups
 *---------------------------------------*/
INT8U  IsChannelSuppGroups(INT8U ChannelNum,int BMCInst)
{
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    _FAR_   ChannelInfo_T*          pChannelInfo;
    if(IsLANChannel(ChannelNum, BMCInst))
    {

        pChannelInfo = getChannelInfo (ChannelNum, BMCInst);
        if(pChannelInfo==NULL)
            return 0;
        if(pChannelInfo->ChannelType==LAN_RMCP_CHANNEL1_TYPE)
        {
            return pChannelInfo->ChannelType;
        }
        else
        {
            if(pChannelInfo->ChannelType==LAN_RMCP_CHANNEL2_TYPE)
            {
                return pChannelInfo->ChannelType;
            }
            else
            {
                if(pChannelInfo->ChannelType==LAN_RMCP_CHANNEL3_TYPE)
                {
                    return pChannelInfo->ChannelType;
                }
                else
                    return 0;
            }
        }
        return 0;
    }
    else
    {
        if (pBMCInfo->IpmiConfig.SerialIfcSupport == 1 && (pBMCInfo->SERIALch != CH_NOT_USED && ChannelNum == pBMCInfo->SERIALch))
        {
            pChannelInfo = getChannelInfo (ChannelNum, BMCInst);
            if(pChannelInfo==NULL)
                return 0;
            return pChannelInfo->ChannelType;
        }
        else
            return 0;
    }
}
/*---------------------------------------
 * ModifyUsrGRP
 *---------------------------------------*/
    int
ModifyUsrGrp(char * UserName,INT8U ChannelNum,INT8U OldAccessLimit, INT8U NewAccessLimit )
{

    char oldgrp[20]="",newgrp[20]="";

    if(0 == NewAccessLimit)
    {
        DeleteUsrFromIPMIGrp(UserName);
    }

    if(PRIV_LEVEL_NO_ACCESS == OldAccessLimit)
    {
        //        strcpy(oldgrp,g_GrpPriv[g_ChannelPrivtbl[ChannelNum].Privilege].grpname);
        ;
    }
    else if(IGNORE_ADD_OR_REMOVE != OldAccessLimit)
    {
        //        strcpy(oldgrp,g_GrpPriv[g_ChannelPrivtbl[ChannelNum].Privilege+OldAccessLimit].grpname);
        ;
    }

    if(PRIV_LEVEL_NO_ACCESS == NewAccessLimit)
    {
        //        strcpy(newgrp,g_GrpPriv[g_ChannelPrivtbl[ChannelNum].Privilege].grpname);
        ;
    }
    else if(IGNORE_ADD_OR_REMOVE != NewAccessLimit)
    {
        //        strcpy(newgrp,g_GrpPriv[g_ChannelPrivtbl[ChannelNum].Privilege+NewAccessLimit].grpname);
        ;
    }

    AddIPMIUsrtoChGrp(UserName,(char *)oldgrp,(char *)newgrp);
    return 0;
}

/*---------------------------------------
 * SetUserAccess
 *---------------------------------------*/
    int
SetUserAccess (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  SetUserAccessReq_T*     pSetUserAccessReq = (_NEAR_ SetUserAccessReq_T*)pReq;
    _FAR_   ChannelUserInfo_T*      pChUserInfo;
    _FAR_   UserInfo_T*             pUserInfo;
    _FAR_   ChannelInfo_T*          pChannelInfo;
    _FAR_   ChannelInfo_T*          pNvrChInfo;
    _FAR_   BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    _FAR_ ChannelUserInfo_T * pNVRChUserInfo=NULL;
    //    char ChFilename[MAX_CHFILE_NAME];
    INT8U                   Index;
    INT8U                   ChannelNum,IPMIMessaging;
    INT8U                   OldAccessLimit;
    int ret=0;
    // Check for Reserved bits
    if((pSetUserAccessReq->UserID & (BIT7 | BIT6)) || (pSetUserAccessReq->SessionLimit & (BIT7 | BIT6 | BIT5 | BIT4)) || (pSetUserAccessReq->AccessLimit == PRIV_LEVEL_RESERVED) || (pSetUserAccessReq->AccessLimit > PRIV_LEVEL_PROPRIETARY && pSetUserAccessReq->AccessLimit != PRIV_LEVEL_NO_ACCESS))
    {
        *pRes = CC_INV_DATA_FIELD;
        return sizeof(*pRes);
    }

    ChannelNum = pSetUserAccessReq->ChannelNoUserAccess & 0x0F;
    if (CURRENT_CHANNEL_NUM == ChannelNum)
    {
        ChannelNum = g_BMCInfo[BMCInst].Msghndlr.CurChannel  & 0xF;
    }

    /*Removing the Hard coding of admin user
      if(pSetUserAccessReq->UserID == IPMI_ROOT_USER)
      {
     *pRes = CC_INV_DATA_FIELD;
     return  sizeof (*pRes);
     }
     */
    pChannelInfo = getChannelInfo (ChannelNum, BMCInst);

    if((NULL == pChannelInfo) || (pSetUserAccessReq->UserID > pBMCInfo->IpmiConfig.MaxUsers) || (SESSIONLESS_CHANNEL == pChannelInfo->SessionSupport))
    {
        *pRes = CC_INV_DATA_FIELD;
        return  sizeof (*pRes);
    }

    pUserInfo = getUserIdInfo(pSetUserAccessReq->UserID, BMCInst);
    if (NULL == pUserInfo)
    {
        TDBG ("AppDevice.c : SetUserAccess - Invalid user Id\n");
        *pRes = CC_INV_DATA_FIELD;
        return sizeof (*pRes);
    }


    TDBG("pChannle info is %p\n",pChannelInfo);
    TDBG("UserId id is %d\n",pSetUserAccessReq->UserID);
    TDBG("pUserInfo->ID is %ld and USERID is %ld\n",pUserInfo->ID,USER_ID);

    if (pUserInfo->ID != USER_ID)
    {
        TCRIT("Invalid data field\n");
        *pRes = CC_INV_DATA_FIELD;
        return  sizeof (*pRes);
    }

    /* User 's session limit should be lesser than Channel 's session limit */
    if(pSetUserAccessReq->SessionLimit > pBMCInfo->IpmiConfig.MaxSession)
    {
        *pRes = CC_INV_DATA_FIELD;
        return sizeof (*pRes);
    }


    LOCK_BMC_SHARED_MEM(BMCInst);
    pNvrChInfo = GetNVRChConfigs(pChannelInfo,BMCInst);
    pNVRChUserInfo = GetNVRChUserConfigs(pChannelInfo,BMCInst);
    pChUserInfo  = getChUserIdInfo (pSetUserAccessReq->UserID , &Index, pChannelInfo->ChannelUserInfo, BMCInst);

    if (NULL == pChUserInfo)
    {
        /* Add the user in NVRAM    */
        pChUserInfo         = AddChUser (pChannelInfo->ChannelUserInfo, &Index, BMCInst);
        if(pChUserInfo == NULL)
        {
            /*Return proper completion if the user exceeds the max channel users*/
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            *pRes = CC_INV_DATA_FIELD;
            return sizeof (*pRes);
        }

        pChUserInfo->ID     = USER_ID;
        pChUserInfo->UserId = pSetUserAccessReq->UserID;
        pNVRChUserInfo[Index].UserId = pSetUserAccessReq->UserID;

        pChUserInfo->AccessLimit = PRIV_LEVEL_NO_ACCESS;
        /* Initial depends on the  Request Bit */
        pChUserInfo->IPMIMessaging=FALSE;
        pChUserInfo->ActivatingSOL=FALSE;
        pChUserInfo->UserAccessCallback = FALSE;
        pChUserInfo->LinkAuth  = FALSE;
        pNVRChUserInfo[Index].ID = USER_ID;
        pNVRChUserInfo[Index].IPMIMessaging=FALSE;
        pNVRChUserInfo[Index].ActivatingSOL=FALSE;
        pNVRChUserInfo[Index].UserId = pSetUserAccessReq->UserID;
        pNVRChUserInfo[Index].LinkAuth    = FALSE;
        pNVRChUserInfo[Index].UserAccessCallback   =FALSE;
    }

    if (0 != (pSetUserAccessReq->ChannelNoUserAccess & 0x80))
    {
        /* set the user access for the channel  */
        IPMIMessaging = GetBits (pSetUserAccessReq->ChannelNoUserAccess, 0x10);
        if (FALSE == IPMIMessaging)
        {
            /* Disable the IPMI Messaging if Its in Enables state */
            if((TRUE == pChUserInfo->IPMIMessaging)&& (pChannelInfo->NoCurrentUser > 1) )
            {
                /* Initialize based on the Request Bit */
                pChUserInfo->IPMIMessaging=IPMIMessaging;
                pNVRChUserInfo[Index].IPMIMessaging=IPMIMessaging;
            }
        }
        else
        {
            if(TRUE == pChUserInfo->IPMIMessaging)
            {
                pChUserInfo->Lock = USER_UNLOCKED;
                pChUserInfo->LockedTime = 0;
                pChUserInfo->FailureAttempts = 0;
            }
            else if(FALSE == pChUserInfo->IPMIMessaging) /* Enable the IPMI Messaging  ,If its in disabled state */
            {
                /* Initialize based on the  Request Bit */
                pChUserInfo->IPMIMessaging=IPMIMessaging;
                pNVRChUserInfo[Index].IPMIMessaging=IPMIMessaging;
                pChUserInfo->Lock = USER_UNLOCKED;
                pChUserInfo->LockedTime = 0;
                pChUserInfo->FailureAttempts = 0;
            }
        }

        /* set in RAM   */
        pChUserInfo->UserAccessCallback = GetBits (pSetUserAccessReq->ChannelNoUserAccess, 0x40);
        pChUserInfo->LinkAuth           = GetBits (pSetUserAccessReq->ChannelNoUserAccess, 0x20);

        pNVRChUserInfo[Index].UserId   = pSetUserAccessReq->UserID;
        pNVRChUserInfo[Index].LinkAuth =
            GetBits (pSetUserAccessReq->ChannelNoUserAccess, 0x20);
        pNVRChUserInfo[Index].UserAccessCallback   =
            GetBits (pSetUserAccessReq->ChannelNoUserAccess, 0x40);


    }
    OldAccessLimit=pChUserInfo->AccessLimit;
    pChUserInfo->AccessLimit = GetBits (pSetUserAccessReq->AccessLimit, 0x0F);

    ret=ModifyUsrGrp((char *)pUserInfo->UserName,ChannelNum,OldAccessLimit,pChUserInfo->AccessLimit);
    if(ret < 0)
    {
        TCRIT("User Has No LAN or Serial Preivilege!!\n");
    }

    pNVRChUserInfo[Index].AccessLimit = pChUserInfo->AccessLimit;
    /* set in NVRAM */
    if (0 == (pUserInfo->MaxSession))
    {
        /* This is MAX User session Limit */
        pUserInfo->MaxSession = pBMCInfo->IpmiConfig.MaxSession;
    }

    UNLOCK_BMC_SHARED_MEM(BMCInst);

    //    FlushIPMI((INT8U*)&pBMCInfo->UserInfo[0],(INT8U*)&pBMCInfo->UserInfo[0],pBMCInfo->IPMIConfLoc.UserInfoAddr,
    //                      sizeof(UserInfo_T)*MAX_USER_CFG_MDS,BMCInst);
    //    FlushChConfigs((INT8U*)pNvrChInfo,pNvrChInfo->ChannelNumber,BMCInst);

    *pRes = CC_NORMAL;

    return sizeof (*pRes);
}


/*---------------------------------------
 * GetUserAccess
 *---------------------------------------*/
    int
GetUserAccess (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  GetUserAccessReq_T*     pGetUserAccessReq = (_NEAR_ GetUserAccessReq_T*)pReq;
    _NEAR_  GetUserAccessRes_T*     pGetUserAccessRes = (_NEAR_ GetUserAccessRes_T*)pRes;
    _FAR_   ChannelUserInfo_T*      pChUserInfo;
    _FAR_   UserInfo_T*             pUserInfo;
    _FAR_   ChannelInfo_T*          pChannelInfo;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    INT8U                   Index;

    // Check for Reserved bits
    if((pGetUserAccessReq->ChannelNum & (BIT7 | BIT6 | BIT5 | BIT4)) || (pGetUserAccessReq->UserID & (BIT7 | BIT6)) || (pGetUserAccessReq->UserID == 0x00))
    {
        pGetUserAccessRes->CompletionCode = CC_INV_DATA_FIELD;
        return sizeof(*pRes);
    }

    if (pGetUserAccessReq->ChannelNum == CURRENT_CHANNEL_NUM)
        pChannelInfo = getChannelInfo(g_BMCInfo[BMCInst].Msghndlr.CurChannel,BMCInst);
    else
        pChannelInfo = getChannelInfo(pGetUserAccessReq->ChannelNum,BMCInst);

    //TDBG("UserID is  %d\n",pGetUserAccessReq->UserID);

    if ((NULL == pChannelInfo)      ||
            (pGetUserAccessReq->UserID > pBMCInfo->IpmiConfig.MaxUsers)    ||
            (SESSIONLESS_CHANNEL == pChannelInfo->SessionSupport))
    {
        pGetUserAccessRes->CompletionCode = CC_INV_DATA_FIELD;
        return  sizeof (*pRes);
    }

    LOCK_BMC_SHARED_MEM(BMCInst);

    pUserInfo = getUserIdInfo (pGetUserAccessReq->UserID, BMCInst);

    if (NULL == pUserInfo)
    {
        UNLOCK_BMC_SHARED_MEM(BMCInst);
        pGetUserAccessRes->CompletionCode = CC_INV_DATA_FIELD;
        return  sizeof (*pRes);
    }

    if (TRUE == pUserInfo->UserStatus)
    {
        pGetUserAccessRes->CurrentUserID  = SetBits (0xC0, USER_ID_ENABLED);
    }
    else
    {
        pGetUserAccessRes->CurrentUserID  = SetBits (0xC0, USER_ID_DISABLED);
    }

    pChUserInfo = getChUserIdInfo (pGetUserAccessReq->UserID, &Index, pChannelInfo->ChannelUserInfo ,BMCInst);

    if (NULL == pChUserInfo)
    {
        pGetUserAccessRes->ChannelAccess  = SetBits (0x0F, 0x0F);
        pGetUserAccessRes->ChannelAccess |= SetBits (0x10, FALSE);
        pGetUserAccessRes->ChannelAccess |= SetBits (0x20, FALSE);
        pGetUserAccessRes->ChannelAccess |= SetBits (0x40, FALSE);
    }
    else
    {
        pGetUserAccessRes->ChannelAccess  = SetBits (0x0F, pChUserInfo->AccessLimit);
        pGetUserAccessRes->ChannelAccess |= SetBits (0x10, pChUserInfo->IPMIMessaging);
        pGetUserAccessRes->ChannelAccess |= SetBits (0x20, pChUserInfo->LinkAuth);
        pGetUserAccessRes->ChannelAccess |= SetBits (0x40, pChUserInfo->UserAccessCallback);
    }

    pGetUserAccessRes->CompletionCode = CC_NORMAL;
    pGetUserAccessRes->MaxNoUserID    = SetBits (0x3F, pChannelInfo->MaxUser);
    pGetUserAccessRes->CurrentUserID  |= SetBits (0x3F, pChannelInfo->NoCurrentUser);
    pGetUserAccessRes->FixedUserID      = SetBits (0x3F, pChannelInfo->NoFixedUser);

    UNLOCK_BMC_SHARED_MEM(BMCInst);

    return sizeof (GetUserAccessRes_T);
}


/*---------------------------------------
 * SetUserName
 *---------------------------------------*/
    int
SetUserName (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  SetUserNameReq_T*       pSetUserNameReq = (_NEAR_ SetUserNameReq_T*)pReq;
    _FAR_   UserInfo_T*             pUserInfo;
    _FAR_   ChannelInfo_T*          pChannelInfo=NULL;
    _FAR_   ChannelInfo_T*          pNvrChInfo;
    _FAR_   ChannelUserInfo_T*      pChUserInfo;
    _FAR_   ChannelUserInfo_T * pNVRChUserInfo=NULL;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    INT8U                   Ch, Index,Chtype;
    //INT16S                  Err;
    INT8U                   i;
    //    char ChFilename[MAX_CHFILE_NAME];
    //INT8U Handle = sizeof(ChInfo_T)-sizeof(ChannelInfo_T);

    // Check for Reserved bits
    if(pSetUserNameReq->UserID & (BIT7 | BIT6) || (pSetUserNameReq->UserID == 0x00))
    {
        *pRes = CC_INV_DATA_FIELD;
        return sizeof(*pRes);
    }

    /*User Id exceeded the limit or  Cant set NULL User */
    if (pSetUserNameReq->UserID > pBMCInfo->IpmiConfig.MaxUsers )
    {
        *pRes = CC_INV_DATA_FIELD ;
        return  sizeof (*pRes);
    }

    pUserInfo = getUserIdInfo(pSetUserNameReq->UserID, BMCInst);

    /* If the user is fixed user */
    if (((pUserInfo != NULL) && (pUserInfo->FixedUser == TRUE)))
    {
        *pRes = CC_INV_DATA_FIELD ;
        return  sizeof (*pRes);
    }

    /* We should not set the NULL user */
    if(0== pSetUserNameReq->UserName [0] )
    {
        printf("\n Setting the NULL user :%x",pSetUserNameReq->UserID);
        *pRes = CC_INV_DATA_FIELD ;
        return	sizeof (*pRes);
    }

    /* check for numeric first char and special chars */
    if( ( 0 != isdigit(pSetUserNameReq->UserName[0] )) ||
            ( NULL != strchr((const char *)pSetUserNameReq->UserName, ' ' )) ||
            ( NULL != strchr((const char *)pSetUserNameReq->UserName, ',' )) ||
            ( NULL != strchr((const char *)pSetUserNameReq->UserName, '.' )) ||
            ( NULL != strchr((const char *)pSetUserNameReq->UserName, ':' )) ||
            ( NULL != strchr((const char *)pSetUserNameReq->UserName, ';' )) ||
            ( NULL != strchr((const char *)pSetUserNameReq->UserName, '/' )) ||
            ( NULL != strchr((const char *)pSetUserNameReq->UserName, '\\' ))||
            ( NULL != strchr((const char *)pSetUserNameReq->UserName, '(' )) ||
            ( NULL != strchr((const char *)pSetUserNameReq->UserName, ')' )) )
    {
        *pRes = CC_INV_DATA_FIELD;
        return sizeof(*pRes);
    }

    //Don't check duplicated user names for 0xFF, this request is for deleting user
    if(0xFF != pSetUserNameReq->UserName [0])
    {
        //checking for Duplicate user names
        if(CheckForDuplicateUsers(pSetUserNameReq->UserName,BMCInst)==FALSE)
        {
            //setting user's name with same name
            if(!strcmp((char *)pUserInfo->UserName,(char *)pSetUserNameReq->UserName))
            {
                *pRes = CC_NORMAL;
                return sizeof (*pRes);
            }
            TINFO("Duplicate ipmi user!!\n");
            *pRes = CC_INV_DATA_FIELD;
            return	sizeof (*pRes);
        }
    }
    //checking for reserved user names
    if(CheckForReservedUsers((char *)pSetUserNameReq->UserName) != 0)
    {
        *pRes = CC_INV_DATA_FIELD;
        return  sizeof (*pRes);
    }

    LOCK_BMC_SHARED_MEM(BMCInst);

    /* Add to Linux data base */
    //Err = 0;

    /* If User[0] is 0xFF means the user is requested to delete */
    if ((0xFF == pSetUserNameReq->UserName [0]) ||
            (0 == pSetUserNameReq->UserName [0] ))
    {
        if ((0 == pUserInfo->CurrentSession) && (0 != disableUser (pSetUserNameReq->UserID,BMCInst)))
        {
            /* Delete this user form all the channel */
            for (Ch = 0; Ch < MAX_NUM_CHANNELS; Ch++)
            {
                if(pBMCInfo->ChConfig[Ch].ChType != 0xff)
                {
                    pChannelInfo = (ChannelInfo_T*)&pBMCInfo->ChConfig[Ch].ChannelInfo;
                }
                else
                {
                    continue;
                }

                pNvrChInfo = GetNVRChConfigs(pChannelInfo,BMCInst);
                pNVRChUserInfo = GetNVRChUserConfigs(pChannelInfo,BMCInst);

                pChUserInfo = getChUserIdInfo(pSetUserNameReq->UserID, &Index, pChannelInfo->ChannelUserInfo,BMCInst);

                if (pChUserInfo != NULL)
                {
                    ModifyUsrGrp((char *)pUserInfo->UserName,pChannelInfo->ChannelNumber,pChUserInfo->AccessLimit,IGNORE_ADD_OR_REMOVE);
                    AddIPMIUsrtoShellGrp((char *)pUserInfo->UserName, pUserInfo->UserShell, IGNORE_ADD_OR_REMOVE_SHELL);
                    pNVRChUserInfo[Index].UserId = 0;
                    pNVRChUserInfo[Index].IPMIMessaging=FALSE;
                    pNVRChUserInfo[Index].ActivatingSOL=FALSE;
                    pNVRChUserInfo[Index].ID=0;
                    pChUserInfo->ID=0;
                    pChUserInfo->UserId = 0;
                    pChUserInfo->IPMIMessaging=FALSE;
                    pChUserInfo->ActivatingSOL=FALSE;
                    if( pUserInfo->UserStatus == TRUE)
                    {
                        pChannelInfo->NoCurrentUser--;
                        pNvrChInfo->NoCurrentUser--;
                    }
                    //                    FlushChConfigs((INT8U*)pNvrChInfo,pNvrChInfo->ChannelNumber,BMCInst);
                }
                else
                {
                    if(pUserInfo != NULL)
                        DeleteUsrFromIPMIGrp((char *)pUserInfo->UserName);
                }
            }

            /* Checking for valid user, empty users doesnt conatain user directories*/
            if (USER_ID == pUserInfo->ID)
            {
                //removing the user directory created in add user
                //	         if(RemoveUsrDir((char *)pUserInfo->UserName)!=0)
                //	         {
                //		      *pRes = CC_UNSPECIFIED_ERR;
                //		      UNLOCK_BMC_SHARED_MEM(BMCInst);
                //		      return  sizeof (*pRes);
                //	         }
            }
            /* Delete the user both in Volatile & Non Volatile memory*/
            pBMCInfo->GenConfig.CurrentNoUser--;
            _fmemset (pUserInfo,    0, sizeof (UserInfo_T));
            //            FlushIPMI((INT8U*)&pBMCInfo->UserInfo[0],(INT8U*)&pBMCInfo->UserInfo[0],pBMCInfo->IPMIConfLoc.UserInfoAddr,
            //                              sizeof(UserInfo_T)*MAX_USER_CFG_MDS,BMCInst);
        }
        else
        {
            *pRes = CC_INV_DATA_FIELD ;
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            return  sizeof (*pRes);
        }
    }
    else if (USER_ID != pUserInfo->ID)//adding users
    {
        for (i = 0; i < MAX_USERNAME_LEN; i++)
        {
            if(!(isascii(pSetUserNameReq->UserName[i])))
            {
                *pRes = CC_INV_DATA_FIELD ;
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                return  sizeof (*pRes);
            }
        }
        /* If First time-if user ID does not exist  */
        /* if the user name is set for the first time */
        pUserInfo->ID            = USER_ID;
        pUserInfo->UserId        = pSetUserNameReq->UserID;
        /* allow for IPMI Mesaging  */
        pUserInfo->UserStatus    = FALSE;
        /* set default max session  */
        pUserInfo->MaxSession    = pBMCInfo->IpmiConfig.MaxSession;

        pBMCInfo->GenConfig.CurrentNoUser++;
        for (Ch = 0; Ch < MAX_NUM_CHANNELS; Ch++)
        {
            if((Chtype = IsChannelSuppGroups(Ch,BMCInst)) == 0)
                continue;
            AddUsrtoIPMIGrp((char *)pSetUserNameReq->UserName,Chtype);
            ModifyUsrGrp((char *)pSetUserNameReq->UserName,Ch,IGNORE_ADD_OR_REMOVE,PRIV_LEVEL_NO_ACCESS);
        }
        AddIPMIUsrtoShellGrp((char *)pSetUserNameReq->UserName, IGNORE_ADD_OR_REMOVE_SHELL, pUserInfo->UserShell);
        //creating ssh directorys for new user
#if 0
        if( CreateSSHUserDir((char *)pSetUserNameReq->UserName)  != 0)
        {
            *pRes = CC_UNSPECIFIED_ERR;
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            return  sizeof (*pRes);
        }
        //creating authentication key for new user
        if (CreateSSHAuthKeyFile((char *)pSetUserNameReq->UserName)!= 0)
        {
            *pRes = CC_UNSPECIFIED_ERR;
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            return	sizeof (*pRes);
        }
#endif
        TINFO("user folders created successfuy----\n");

    }
    else if (USER_ID == pUserInfo->ID)//modifying users
    {
        for (i = 0; i < MAX_USERNAME_LEN; i++)
        {
            if(!(isascii(pSetUserNameReq->UserName[i])))
            {
                *pRes = CC_INV_DATA_FIELD ;
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                return  sizeof (*pRes);
            }
        }
        //    	RenameUserDir((char *)pUserInfo->UserName, (char *)pSetUserNameReq->UserName);

        for (Ch = 0; Ch < MAX_NUM_CHANNELS; Ch++)
        {
            if((Chtype = IsChannelSuppGroups(Ch,BMCInst)) == 0)
            {
                continue;
            }
            AddUsrtoIPMIGrp((char *)pSetUserNameReq->UserName,Chtype);
            pChannelInfo = getChannelInfo (Ch, BMCInst);
            if(NULL == pChannelInfo)
            {
                *pRes = CC_INV_DATA_FIELD;
                return	sizeof (*pRes);
            }

            pChUserInfo = getChUserIdInfo(pSetUserNameReq->UserID, &Index, pChannelInfo->ChannelUserInfo,BMCInst);
            if (pChUserInfo != NULL)
            {
                ModifyUsrGrp((char *)pUserInfo->UserName, pChannelInfo->ChannelNumber, pChUserInfo->AccessLimit, IGNORE_ADD_OR_REMOVE);//Remove User
                AddIPMIUsrtoShellGrp((char *)pUserInfo->UserName, pUserInfo->UserShell, IGNORE_ADD_OR_REMOVE_SHELL);//Remove User from Shell Group
                ModifyUsrGrp((char *)pSetUserNameReq->UserName, pChannelInfo->ChannelNumber, IGNORE_ADD_OR_REMOVE, pChUserInfo->AccessLimit);//add user
                AddIPMIUsrtoShellGrp((char *)pSetUserNameReq->UserName, IGNORE_ADD_OR_REMOVE_SHELL, pUserInfo->UserShell);//Add User in Shell Group
            }
            else
            {
                ModifyUsrGrp((char *)pUserInfo->UserName, Ch,PRIV_LEVEL_NO_ACCESS, IGNORE_ADD_OR_REMOVE);//Remove User
                AddIPMIUsrtoShellGrp((char *)pUserInfo->UserName, pUserInfo->UserShell, IGNORE_ADD_OR_REMOVE_SHELL);//Remove User from default Shell Group
                ModifyUsrGrp((char *)pSetUserNameReq->UserName, Ch, IGNORE_ADD_OR_REMOVE, PRIV_LEVEL_NO_ACCESS);//add user
                AddIPMIUsrtoShellGrp((char *)pSetUserNameReq->UserName, IGNORE_ADD_OR_REMOVE_SHELL, pUserInfo->UserShell);//Add User in default Shell Group
            }
        }
    }
    _fmemcpy (pUserInfo->UserName, pSetUserNameReq->UserName, MAX_USERNAME_LEN);
    UNLOCK_BMC_SHARED_MEM(BMCInst);

    //            FlushIPMI((INT8U*)&pBMCInfo->UserInfo[0],(INT8U*)&pBMCInfo->UserInfo[0],pBMCInfo->IPMIConfLoc.UserInfoAddr,
    //                              sizeof(UserInfo_T)*MAX_USER_CFG_MDS,BMCInst);
    //            FlushIPMI((INT8U*)&pBMCInfo->GenConfig,(INT8U*)&pBMCInfo->GenConfig,pBMCInfo->IPMIConfLoc.GenConfigAddr,
    //                              sizeof(GENConfig_T),BMCInst);
    *pRes = CC_NORMAL;
    return sizeof (*pRes);
}


/*---------------------------------------
 * GetUserName
 *---------------------------------------*/
    int
GetUserName (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  GetUserNameReq_T*       pGetUserNameReq = (_NEAR_ GetUserNameReq_T*)pReq;
    _NEAR_  GetUserNameRes_T*       pGetUserNameRes = (_NEAR_ GetUserNameRes_T*)pRes;
    _FAR_   UserInfo_T*             pUserInfo;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    //INT16S                  Err;

    // Check for Reserved bits
    if(pGetUserNameReq->UserID & (BIT7 | BIT6) || (pGetUserNameReq->UserID == 0x00))
    {
        pGetUserNameRes->CompletionCode = CC_INV_DATA_FIELD ;
        return sizeof(*pRes);
    }

    if (pGetUserNameReq->UserID > pBMCInfo->IpmiConfig.MaxUsers)
    {
        /*  if user ID exceeded the Max limit */
        pGetUserNameRes->CompletionCode = CC_INV_DATA_FIELD ;
        return  sizeof (*pRes);/* Invalied user id */
    }

    pUserInfo = getUserIdInfo(pGetUserNameReq->UserID, BMCInst);

    /* if User is disabled or if User is not created */

    //if user is disabled we dont have to return invalid data field
    //instead we return everything possible about the user
    // if pUserInfo is NULL then nothing in the structure at all
    // pUserInfo being NULL is probably not possible
    // If the signature doesnt match then the useris not yet configured
    // so reasonable to return an error
    if ((NULL == pUserInfo) || (pUserInfo->ID != USER_ID))
    {
        /* User with given ID is disabled */
        *pRes = CC_INV_DATA_FIELD;
        return sizeof (*pRes);
    }

    //if we are here then the user is just disabled
    if(FALSE == pUserInfo->UserStatus)
    {
        TDBG("user is just dissabled\n");
        //user is just disabled!!
    }

    LOCK_BMC_SHARED_MEM(BMCInst);

    //Err = 0;
    pGetUserNameRes->CompletionCode = CC_NORMAL;
    _fmemcpy (pGetUserNameRes->UserName, pUserInfo->UserName, MAX_USERNAME_LEN);

    UNLOCK_BMC_SHARED_MEM(BMCInst);

    return sizeof (GetUserNameRes_T);
}


/*---------------------------------------
 * SetUserPassword
 *---------------------------------------*/
    int
SetUserPassword (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_  SetUserPswdReq_T*       pSetUserPswdReq = (_NEAR_ SetUserPswdReq_T*)pReq;
    _FAR_   UserInfo_T*             pUserInfo;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    INT8U                   UserId=0, Operation;
    INT8U   UserPswd[MAX_PASSWD_LEN];
    INT8S EncryptedUserPswd[MAX_ENCRYPTED_PSWD_LEN] = {0};
    INT8U PwdEncKey[MAX_SIZE_KEY] = {0};


    Operation = pSetUserPswdReq->Operation & 0x03;
    UserId    = pSetUserPswdReq->UserID & 0x3F;

    if ((((pSetUserPswdReq->UserID & TWENTY_BYTE_PWD) == 0) && (ReqLen != IPMI_15_PASSWORD_LEN))
            ||(((pSetUserPswdReq->UserID & TWENTY_BYTE_PWD) == TWENTY_BYTE_PWD) && (ReqLen != IPMI_20_PASSWORD_LEN)))
    {
        /* For enable or disable using the password field is optional */
        if (ReqLen == OP_USERID_ONLY_LENGTH)
        {
            if ((Operation != OP_ENABLE_USER_ID) && (Operation != OP_DISABLE_USER_ID))
            {
                *pRes = CC_REQ_INV_LEN;
                return  sizeof (*pRes); //Invalid operation.
            }
        }
        else
        {
            *pRes = CC_REQ_INV_LEN;
            return  sizeof (*pRes); //password len invalid
        }
    }

    /* Reserved bits checking */
    if((pSetUserPswdReq->UserID & BIT6) || (UserId == 0) || (pSetUserPswdReq->Operation & (BIT7 | BIT6 | BIT5 | BIT4 | BIT3 | BIT2)))
    {
        *pRes = CC_INV_DATA_FIELD;
        return	sizeof (*pRes);
    }

    if (ReqLen == IPMI_15_PASSWORD_LEN)
    {
        _fmemset (pSetUserPswdReq->Password + 16, 0, 4);
    }
    ReqLen -=2;

    if (UserId > pBMCInfo->IpmiConfig.MaxUsers)
    {
        *pRes = CC_INV_DATA_FIELD;
        return  sizeof (*pRes); /*User Id exceeded the limit*/
    }

    pUserInfo = getUserIdInfo (UserId, BMCInst);

    if ( NULL ==  pUserInfo )
    {
        IPMI_WARNING ("Invalid User Id \n");
        *pRes = CC_INV_DATA_FIELD;
        return  sizeof (*pRes);
    }

    TDBG("pUserInfo is %p\n",pUserInfo);

    *pRes = CC_NORMAL;

    switch (Operation)
    {

        case 0:
            TDBG("In disable user for user id %d\n",UserId);
            /*disable user  */
            if(pUserInfo == NULL)
            {
                //the user is already disabled!!
                //no point in disabling him again
            }
            else if(disableUser(UserId, BMCInst) == 0)
            {
                //we cannot disable this user because he is the only user in the channel
                TDBG("not disabling user\n");
                /* cannot disable user since this user is the only user in the channel*/
                *pRes = CC_SETPASSWORD_CANNOT_DISABLE_USER;
                return  sizeof (*pRes);
            }
            else
            {
                //here we can disable the user

                TDBG("will disable user\n");

                if (pUserInfo->UserStatus == TRUE)
                {
                    pBMCInfo->GenConfig.CurrentNoUser--;
                    UpdateCurrentEnabledUserCount(COUNT_DECREASE, BMCInst);
                }

                pUserInfo->UserStatus    = FALSE;

                //when user is disabled ".ssh" folder of the user  is renamed to restrict user login
                TDBG("renaming the .ssh folder to _.ssh\n");
                //		if(RenameUserSSHDir((char *)pUserInfo->UserName)!=0)
                //			{
                //			*pRes = CC_UNSPECIFIED_ERR;
                //			return  sizeof (*pRes);
                //			}
            }

            break;

        case 1:
            /*enable user   */
            if (USER_ID == pUserInfo->ID)
            {
                /* if for the first time then Increment the Current user No */
                if ( pUserInfo->UserStatus == FALSE )
                {
                    pBMCInfo->GenConfig.CurrentNoUser++;
                    UpdateCurrentEnabledUserCount(COUNT_INCREASE, BMCInst);
                }
                pUserInfo->UserStatus    = TRUE;

                //when user is enabled the pub key is restored to original name to allow login
                TDBG("restoring the ._ssh folder to .ssh\n");
                //		if( RestoreUsrSSHDir((char *)pUserInfo->UserName)!=0)
                //		{
                //			*pRes = CC_UNSPECIFIED_ERR;
                //			return  sizeof (*pRes);
                //		}
                if (0 == pUserInfo->UserName [0])
                {
                    TDBG ("The user Name is Not yet set.. So No synching with the linux \n");
                    *pRes = CC_NORMAL;
                    //                    FlushIPMI((INT8U*)&pBMCInfo->UserInfo[0],(INT8U*)&pBMCInfo->UserInfo[0],pBMCInfo->IPMIConfLoc.UserInfoAddr,
                    //                                      sizeof(UserInfo_T)*MAX_USER_CFG_MDS,BMCInst);
                    //                    FlushIPMI((INT8U*)&pBMCInfo->GenConfig,(INT8U*)&pBMCInfo->GenConfig,pBMCInfo->IPMIConfLoc.GenConfigAddr,
                    //                                      sizeof(GENConfig_T),BMCInst);
                    return  sizeof (*pRes);
                }
                TDBG  ("Synching with the Linux User dataBase \n");
            }
            else
            {
                /* Looks like the User info is not there So just enable the user in the IPMI  */
                /* no need to Ssync with  the linux databse since there will b No corresponding user in the linux database  */
                TCRIT("Enable user called on a not yet enabled user!!\n");
                /* If First time-if user ID does not exist  */
                /* if the user is enabled for the first time  */
                pUserInfo->ID            = USER_ID;
                pUserInfo->UserId        = UserId;
                pUserInfo->UserStatus    = TRUE;
                pUserInfo->MaxSession    = pBMCInfo->IpmiConfig.MaxSession;

                pBMCInfo->GenConfig.CurrentNoUser++;
                UpdateCurrentEnabledUserCount(COUNT_INCREASE, BMCInst);

                //when user is enabled the pub key is restored to original name to allow login
                TDBG("restoring the ._ssh folder to .ssh\n");
                //		if(RestoreUsrSSHDir((char *)pUserInfo->UserName)!=0)
                //		{
                //			*pRes = CC_UNSPECIFIED_ERR;
                //			return	sizeof (*pRes);
                //		}
                memset(pUserInfo->UserName, 0, MAX_USERNAME_LEN);
                *pRes = CC_NORMAL;
                return  sizeof (*pRes);
            }
            break;

        case 2:
            /*set password  */
            if (USER_ID == pUserInfo->ID)
            {
                LOCK_BMC_SHARED_MEM(BMCInst);
                pUserInfo->MaxPasswordSize = ReqLen;
#if 0
                if (g_corefeatures.userpswd_encryption == ENABLED)
                {
                    if(getEncryptKey(PwdEncKey))
                    {
                        TCRIT("Error in getting the encryption key. quitting...\n");
                        *pRes = CC_UNSPECIFIED_ERR;
                        return sizeof (*pRes);
                    }
                    if(EncryptPassword((INT8S *)pSetUserPswdReq->Password, MAX_PASSWORD_LEN, EncryptedUserPswd, MAX_PASSWORD_LEN, PwdEncKey))
                    {
                        TCRIT("Error in encrypting the IPMI User Password for user ID:%d\n", pSetUserPswdReq->UserID);
                        *pRes = CC_UNSPECIFIED_ERR;
                        return sizeof(INT8U);
                    }
                    LOCK_BMC_SHARED_MEM(BMCInst);
                    memset(&(pBMCInfo->EncryptedUserInfo[pSetUserPswdReq->UserID - 1].EncryptedPswd), 0, MAX_ENCRYPTED_PSWD_LEN);
                    memcpy(&(pBMCInfo->EncryptedUserInfo[pSetUserPswdReq->UserID - 1].EncryptedPswd), EncryptedUserPswd, MAX_ENCRYPTED_PSWD_LEN);
                    memcpy(pBMCInfo->UserInfo[UserId - 1].UserPassword, "$ENCRYPTED$", 11);
                    UNLOCK_BMC_SHARED_MEM(BMCInst);
                }
                else
#endif
                {
                    _fmemcpy (pUserInfo->UserPassword, pSetUserPswdReq->Password, MAX_PASSWORD_LEN);
                }

                UNLOCK_BMC_SHARED_MEM(BMCInst);
            }
            else
            {
                *pRes = CC_INV_DATA_FIELD;
                return sizeof (*pRes);
            }
            break;

        case 3:
            LOCK_BMC_SHARED_MEM(BMCInst);
#if 0
            if (g_corefeatures.userpswd_encryption == ENABLED)
            {
                if(getEncryptKey(PwdEncKey))
                {
                    TCRIT("Error in getting the encryption key. So,quitting....\n");
                    *pRes = CC_UNSPECIFIED_ERR;
                    return sizeof (*pRes);
                }
                if(DecryptPassword((INT8S *)(pBMCInfo->EncryptedUserInfo[UserId - 1].EncryptedPswd), MAX_PASSWORD_LEN, (char *)UserPswd, MAX_PASSWORD_LEN, PwdEncKey))
                {
                    TCRIT("Error in Decrypting IPMI User Password for user with ID:%d\n", UserId);
                    *pRes = CC_UNSPECIFIED_ERR;
                    return sizeof(*pRes);
                }
            }
            else
#endif
            {
                _fmemcpy (UserPswd, pUserInfo->UserPassword, MAX_PASSWORD_LEN);
            }
            /*Test Password */
            if( ReqLen != pUserInfo->MaxPasswordSize  && (pUserInfo->MaxPasswordSize != 0))
            {
                *pRes = CC_PASSWORD_TEST_FAILED_WRONG_SIZE;
            }
            else if (((FALSE == pUserInfo->UserStatus) && (pUserInfo->ID != USER_ID)) ||
                    (0 != _fmemcmp (UserPswd, pSetUserPswdReq->Password, MAX_PASSWORD_LEN)))
            {
                *pRes = CC_PASSWORD_TEST_FAILED;
            }

            UNLOCK_BMC_SHARED_MEM(BMCInst);
            return sizeof (*pRes);
            break;
    }

    //    FlushIPMI((INT8U*)&pBMCInfo->UserInfo[0],(INT8U*)&pBMCInfo->UserInfo[0],pBMCInfo->IPMIConfLoc.UserInfoAddr,
    //                      sizeof(UserInfo_T)*MAX_USER_CFG_MDS,BMCInst);
    //    FlushIPMI((INT8U*)&pBMCInfo->GenConfig,(INT8U*)&pBMCInfo->GenConfig,pBMCInfo->IPMIConfLoc.GenConfigAddr,
    //                      sizeof(GENConfig_T),BMCInst);
    //    FlushIPMI((INT8U*)&pBMCInfo->EncryptedUserInfo[0], (INT8U*)&pBMCInfo->EncryptedUserInfo[0], pBMCInfo->IPMIConfLoc.EncUserPasswdAddr,
    //                      sizeof(EncryptedUserInfo_T) * MAX_USER_CFG_MDS, BMCInst);
    return sizeof (*pRes);
}


/*---------------------------------------
 * MasterWriteRead
 *---------------------------------------*/
    int
MasterWriteRead (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_ MasterWriteReadReq_T* pMasterWriteReadReq = (_NEAR_ MasterWriteReadReq_T*)pReq;
    _NEAR_ MasterWriteReadRes_T* pMasterWriteReadRes = (_NEAR_ MasterWriteReadRes_T*)pRes;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    _NEAR_ INT8U*                OutBuffer;
    _NEAR_ INT8U*                InBuffer;
    INT8U                 ChannelNum, BusID, BusType, SlaveAddr, ReadCount, WriteCount;
    INT8S	BusName[64];
    int retval = 0;
    INT8U OrgReadCount;
    INT8U	I2CBusId=0;
    INT8U DevID = 0;            // This will be '0' by Default... /dev/peci0
    INT8U Target = 0x30;        // This is the Client address. 30h = 48 is Default for CPU0
    INT8U Domain = 0;           // Multi-Domain support. Default is '0'
    INT8U Xmit_Feedback = 0;    // If full response is Desired, enable this. Default is '1'
    INT8U AWFCS = 0;            // Enable AWFCS in the Transmitted packet by Hw. Default is '0'
    INT8U Read_Len = 0;         // Number of bytes of read back Data from PECI Client
    INT8U Write_Len = 0;        // Number of bytes of data, host is sending to client
    INT8U *Write_Data = NULL;   // Pointer to the Data sent by User to the PECI Client
    INT8U *Read_Data = NULL;    // Pointer to the Data received from PECI Client to be sent to user
    int ret_val = 0;

    if(pMasterWriteReadReq->SlaveAddress & BIT0)
    {
        *pRes = CC_INV_DATA_FIELD;
        return sizeof (*pRes);
    }

    ReadCount     = pMasterWriteReadReq->ReadCount;
    ChannelNum    = GetBits (pMasterWriteReadReq->BusTypeChNo, 0xF0);
    BusID         = GetBits (pMasterWriteReadReq->BusTypeChNo, 0x0E);
    BusType       = GetBits (pMasterWriteReadReq->BusTypeChNo, 0x01);
    SlaveAddr     = (pMasterWriteReadReq->SlaveAddress >> 1);

    // Command order:
    // ipmitool -H <IP> -I lan -U <Username> -P <Password> bus=7 <Slave_Addr> <Read_Len> <Write_Len> <AWFCS> <Domain> <Data>
    // <bus=7> : Bus# must be 0x07 for comunicating with PECI over IPMI. Other buses are not for this feature
    // <Slave_Addr> : This is the PECI Client target address.
    // <Read_Len> : Number of bytes of data to read from the PECI Response
    // <Write_Len> : Number of bytes of data being written, as per the PECI Spec. Number of Bytes after Domain.
    // <AWFCS> : 0x01 or 0x00 indicates enabling or disabling of AWFCS feature respectively
    // <Domain> : 0x01 or 0x00 indicates domain=1 or domain=0 for multi-domain commands respectively
    // <Data> : Rest of data like Command, and other params as per the PECI Spec.

    // If BusType is External (0x01) and BusID is 7, then we consider to communicate with PECI
    if(g_corefeatures.peci_over_ipmi == ENABLED )
    {
        if ((BusType & 0x01) && (BusID == 0x07))
        {
            DevID               = 0;
            Xmit_Feedback       = 0;
            Target              = pMasterWriteReadReq->SlaveAddress;
            Read_Len            = ReadCount;
            Write_Len           = pMasterWriteReadReq->Data[0];
            AWFCS               = pMasterWriteReadReq->Data[1];
            Domain              = pMasterWriteReadReq->Data[2];
            Write_Data          = &pMasterWriteReadReq->Data[3];
            Read_Data           = &pMasterWriteReadRes->Data[0];

            memset(&pMasterWriteReadRes->Data[0], 0, sizeof(pMasterWriteReadRes->Data));

            // Call the PECI Generic Handler for passing the command to the PECI Controller
            //            if(g_HALPECIHandle[HAL_PECI_COMMAND] != NULL)
            //            {
            //                ret_val = ((int(*)(char, char, char, char, char, char *, char, char *, char))g_HALPECIHandle[HAL_PECI_COMMAND]) (DevID, Target,
            //                                                                                                Domain, Xmit_Feedback, AWFCS,
            //                                                                                                (char *)Write_Data, Write_Len,
            //                                                                                                (char *)Read_Data, Read_Len );
            //            }
            //            else
            {
                pMasterWriteReadRes->CompletionCode = CC_PARAM_NOT_SUP_IN_CUR_STATE;
                return sizeof (*pRes);
            }

            /* Check if peci issue command operation is success or not */
            if (ret_val == -1)
            {
                pMasterWriteReadRes->CompletionCode = CC_INV_DATA_FIELD;
                return sizeof (*pRes);
            }

            pMasterWriteReadRes->CompletionCode = CC_NORMAL;

            return (sizeof (*pRes) + ReadCount);
        }
    }

    if(0 ==BusType){
        if(PRIMARY_IPMB_CHANNEL == ChannelNum)
        {
            I2CBusId=pBMCInfo->IpmiConfig.PrimaryIPMBBusNumber;
        }
        else if((pBMCInfo->IpmiConfig.SecondaryIPMBSupport == 0x01)&&(pBMCInfo->SecondaryIPMBCh == ChannelNum))
        {
            I2CBusId=pBMCInfo->IpmiConfig.SecondaryIPMBBusNumber;
        }else{
            *pRes = CC_INV_DATA_FIELD;
            return  sizeof (*pRes);
        }
    }
    else
    {
        if(BusID==pBMCInfo->IpmiConfig.PrimaryIPMBBusNumber)
        {
            *pRes = CC_INV_DATA_FIELD;
            return  sizeof (*pRes);
        }else if((pBMCInfo->IpmiConfig.SecondaryIPMBSupport == 0x01)&&(BusID==pBMCInfo->IpmiConfig.SecondaryIPMBBusNumber))
        {
            *pRes = CC_INV_DATA_FIELD;
            return  sizeof (*pRes);
        }else{
            I2CBusId=BusID;
        }
    }

    if (ReqLen < 3)
    {
        *pRes = CC_REQ_INV_LEN;
        return  sizeof (*pRes);
    }

    /* number of bytes to write
     * = pMasterWriteReadReq length - 3 for  Request Data byte -
     * BusTypeChNo,SlaveAddr,Readcount + 1 byte for address
     */
    WriteCount = ReqLen - 3;

    OutBuffer = (pMasterWriteReadReq->Data);
    InBuffer = pMasterWriteReadRes->Data;
    sprintf(BusName,"/dev/i2c%d",I2CBusId);

    // Save original ReadCount in case we need to modify it
    OrgReadCount = ReadCount;

    // If both ReadCount and WriteCount are zero, then force a read of 1 byte.
    // If we do not do this, the write command will fail.
    // Having both counts 0 is a way of "pinging" the given device to see if it
    // is responding to its address.

    if (ReadCount == 0 && WriteCount == 0)
    {
        ReadCount = 1;
    }

    if (ReadCount > 0)
    {
        //        if(g_HALI2CHandle[HAL_I2C_RW] != NULL)
        //        {
        //            retval = ((int(*)(char *,u8,u8 *,u8 *,size_t,size_t))g_HALI2CHandle[HAL_I2C_RW]) (BusName, SlaveAddr, OutBuffer, InBuffer, WriteCount, ReadCount);
        //            if (retval < 0)
        //            {
        //                 pMasterWriteReadRes->CompletionCode = (retval & MASTER_RW_ERRCODE);
        //                return sizeof (*pRes);
        //            }
        //        }
        //        else
        {
            pMasterWriteReadRes->CompletionCode = CC_PARAM_NOT_SUP_IN_CUR_STATE;
            return sizeof(*pRes);
        }

        ReadCount = OrgReadCount;

        /* copy the bytes read  to Response Data */
        _fmemcpy (pMasterWriteReadRes->Data, InBuffer, ReadCount);
    }
    else
    {
        /* No data to read so use master write instead,
         * otherwise some devices (EEPROM) that have not finished writing
         * will fail on the read transaction and possibly corrupt data
         */
        //        if(g_HALI2CHandle[HAL_I2C_MW] != NULL)
        //        {
        //            retval= ((ssize_t(*)(char *,u8,u8 *,size_t))g_HALI2CHandle[HAL_I2C_MW]) (BusName, SlaveAddr, OutBuffer, WriteCount);
        //            if(retval < 0)
        //            {
        //                pMasterWriteReadRes->CompletionCode = (retval & MASTER_RW_ERRCODE);
        //                return sizeof (*pRes);
        //            }
        //        }
        //        else
        {
            pMasterWriteReadRes->CompletionCode = CC_PARAM_NOT_SUP_IN_CUR_STATE;
            return sizeof(*pRes);
        }
    }

    pMasterWriteReadRes->CompletionCode = CC_NORMAL;

    return (sizeof (*pRes) + ReadCount);
}


#if 0
/*-------------------------------------------
 * ComputeAuthCode
 *-------------------------------------------*/
    void
ComputeAuthCode (_FAR_ INT8U* pPassword, _NEAR_ SessionHdr_T* pSessionHdr,
        _NEAR_ IPMIMsgHdr_T* pIPMIMsg, _NEAR_ INT8U* pAuthCode,
        INT8U ChannelType)
{
    if (AUTH_TYPE_PASSWORD == pSessionHdr->AuthType)
    {
        _fmemcpy (pAuthCode, pPassword, MAX_PASSWORD_LEN);
    }
    else
    {
        INT8U   AuthBuf [MAX_AUTH_PARAM_SIZE];
        INT16U  AuthBufLen = 0;
        INT8U   IPMIMsgLen = *((_NEAR_ INT8U*) pIPMIMsg - 1);

        /* Password */
        _fmemcpy (AuthBuf, pPassword, MAX_PASSWORD_LEN);
        AuthBufLen += MAX_PASSWORD_LEN;
        /* Session ID */
        _fmemcpy (AuthBuf + AuthBufLen, &pSessionHdr->SessionID, sizeof (INT32U));
        AuthBufLen += sizeof (INT32U);
        /* IPMI Response Message */
        _fmemcpy (AuthBuf + AuthBufLen, pIPMIMsg, IPMIMsgLen);
        AuthBufLen += IPMIMsgLen;

        if (ChannelType == MULTI_SESSION_CHANNEL)
        {
            /* Session Sequence Number */
            _fmemcpy (AuthBuf + AuthBufLen,
                    &pSessionHdr->SessionSeqNum, sizeof (INT32U));
            AuthBufLen += sizeof (INT32U);
        }
        /* Password */
        _fmemcpy (AuthBuf + AuthBufLen, pPassword, MAX_PASSWORD_LEN);
        AuthBufLen += MAX_PASSWORD_LEN;

        switch (pSessionHdr->AuthType)
        {
            case AUTH_TYPE_MD2 :
                AuthCodeCalMD2 (AuthBuf, pAuthCode, AuthBufLen);
                break;

            case AUTH_TYPE_MD5 :
                AuthCodeCalMD5 (AuthBuf, pAuthCode, AuthBufLen);
                break;

            default  :
                TDBG ("RMCP: Invalid Authentication Type \n");
        }
    }
}
#endif

/*---------------------------------------
 * GetSystemInfoParam
 *---------------------------------------*/
    int
GetSystemInfoParam (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{

    _NEAR_ GetSystemInfoParamReq_T* pGetSysInfoReq = (_NEAR_ GetSystemInfoParamReq_T*)pReq;
    _NEAR_ GetSystemInfoParamRes_T* pGetSysInfoRes = ( _NEAR_ GetSystemInfoParamRes_T* ) pRes ;
    _NEAR_ GetSystemInfoParamOEMRes_T* pGetSysInfoOEMRes = ( _NEAR_ GetSystemInfoParamOEMRes_T* ) pRes ;
    BMCInfo_t *pBMCInfo = &g_BMCInfo[BMCInst];
    _FAR_   SystemInfoConfig_T*	pSysInfoCfg;
    INT8U resSize = sizeof(pGetSysInfoRes->CompletionCode) + sizeof(pGetSysInfoRes->ParamRevision);
    INT8U oem_len;
    unsigned long oem_addr;

    // Check for Reserved bits
    if(pGetSysInfoReq->ParamRev & (BIT6 | BIT5 | BIT4 | BIT3 | BIT2 | BIT1 | BIT0))
    {
        pGetSysInfoRes->CompletionCode = CC_INV_DATA_FIELD;
        return sizeof(pGetSysInfoRes->CompletionCode);
    }

    pGetSysInfoRes->CompletionCode   = CC_NORMAL;

    /* Fill the param's older Version and the present version */
    pGetSysInfoRes->ParamRevision    = ((PARAM_OLDER_REVISION << 4) & 0xF0 ) | PARAM_PRESENT_REVISION;

    /* Check for Revision only parameter */
    if (pGetSysInfoReq->ParamRev & GET_PARAM_REV_ONLY )
    {
        if((MAX_APP_CONF_PARAM >= pGetSysInfoReq->ParamSelector))
        {
            return resSize;
        }
        //        else if(( NULL != g_PDKHandle[PDK_GETSYSINFOPARAM]) &&
        //                    ((MIN_SYSINFO_OEM_CONF_PARAM <= pGetSysInfoReq->ParamSelector) && (MAX_SYSINFO_OEM_CONF_PARAM >= pGetSysInfoReq->ParamSelector)))
        //        {
        //            oem_len = ((int(*)(INT8U, unsigned long*,int))(g_PDKHandle[PDK_GETSYSINFOPARAM]))(pGetSysInfoReq->ParamSelector, &oem_addr ,BMCInst);
        //            if( oem_len == 0)
        //            {
        //                pGetSysInfoRes->CompletionCode = CC_SYS_INFO_PARAM_NOT_SUPPORTED;
        //                return sizeof(INT8U);
        //            }
        //            else
        //                return resSize;
        //        }
        else
        {
            *pRes = CC_PARAM_NOT_SUPPORTED;
            return sizeof (*pRes);
        }
    }

    /* Get Systen Info parameters from NVRAM */
    pSysInfoCfg = &pBMCInfo->SystemInfoConfig;

    switch(pGetSysInfoReq->ParamSelector)
    {
        case SET_IN_PROGRESS_PARAM:		/*Parameter 0 volatile*/
            if( (0x00 != pGetSysInfoReq->SetSelector) || (0x00 != pGetSysInfoReq->BlockSelector) )
            {
                pGetSysInfoRes->CompletionCode = CC_INV_DATA_FIELD;
                return sizeof(pGetSysInfoRes->CompletionCode);
            }

            LOCK_BMC_SHARED_MEM(BMCInst);
            pGetSysInfoRes->SysInfo.SetInProgress = BMC_GET_SHARED_MEM(BMCInst)->m_Sys_SetInProgress;
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            resSize++;
            break;

        case SYS_FW_VERSION_PARAM:
            _fmemset(&pGetSysInfoRes->SysInfo.SysVerInfo,0,sizeof(SysVerInfo_T));
            if((pGetSysInfoReq->SetSelector >= MAX_BLOCK_SIZE)|| (0x00 != pGetSysInfoReq->BlockSelector))
            {
                pGetSysInfoRes->CompletionCode = CC_INV_DATA_FIELD;
                return sizeof(pGetSysInfoRes->CompletionCode);
            }

            pGetSysInfoRes->SysInfo.SysVerInfo.SetSelector = pGetSysInfoReq->SetSelector;
            resSize++;
            if(pGetSysInfoReq->SetSelector==ZERO_SETSELECTOR)
            {
                pGetSysInfoRes->SysInfo.SysVerInfo.SysFWVersion[0]=pSysInfoCfg->SysFWVersion.TypeOfEncoding;
                pGetSysInfoRes->SysInfo.SysVerInfo.SysFWVersion[1]=pSysInfoCfg->SysFWVersion.StringLength;
                _fmemcpy(&pGetSysInfoRes->SysInfo.SysVerInfo.SysFWVersion[2], pSysInfoCfg->SysFWVersion.SysFWVersionName,MAX_STRING_LENGTH_COPY);
                resSize += MAX_BLOCK_SIZE;
            }
            else
            {
                _fmemcpy(&pGetSysInfoRes->SysInfo.SysVerInfo.SysFWVersion[0],
                        &pSysInfoCfg->SysFWVersion.SysFWVersionName[(pGetSysInfoReq->SetSelector * MAX_BLOCK_SIZE) - 2],MAX_BLOCK_SIZE);
                resSize += MAX_BLOCK_SIZE;
            }
            break;

        case SYS_NAME_PARAM:
            if((pGetSysInfoReq->SetSelector >= MAX_BLOCK_SIZE)|| (0x00 != pGetSysInfoReq->BlockSelector))
            {
                pGetSysInfoRes->CompletionCode = CC_INV_DATA_FIELD;
                return sizeof(pGetSysInfoRes->CompletionCode);
            }

            pGetSysInfoRes->SysInfo.SysNameInfo.SetSelector = pGetSysInfoReq->SetSelector;
            resSize++;
            if(pGetSysInfoReq->SetSelector==ZERO_SETSELECTOR)
            {
                pGetSysInfoRes->SysInfo.SysNameInfo.SysName[0]=pSysInfoCfg->SysName.TypeOfEncoding_Sys_Name;
                pGetSysInfoRes->SysInfo.SysNameInfo.SysName[1]=pSysInfoCfg->SysName.StringLength_Sys_Name;
                _fmemcpy(&pGetSysInfoRes->SysInfo.SysNameInfo.SysName[2],&pSysInfoCfg->SysName.SystemName,MAX_STRING_LENGTH_COPY);
                resSize += MAX_BLOCK_SIZE;
            }
            else
            {
                _fmemcpy(&pGetSysInfoRes->SysInfo.SysNameInfo.SysName[0],
                        &pSysInfoCfg->SysName.SystemName[(pGetSysInfoReq->SetSelector * MAX_BLOCK_SIZE) - 2], MAX_BLOCK_SIZE);
                resSize += MAX_BLOCK_SIZE;
            }
            break;

        case PRIM_OS_NAME_PARAM:
            if((pGetSysInfoReq->SetSelector >= MAX_BLOCK_SIZE)|| (0x00 != pGetSysInfoReq->BlockSelector))
            {
                pGetSysInfoRes->CompletionCode = CC_INV_DATA_FIELD;
                return sizeof(pGetSysInfoRes->CompletionCode);
            }

            pGetSysInfoRes->SysInfo.PrimOSInfo.SetSelector  = pGetSysInfoReq->SetSelector;
            resSize++;
            if(pGetSysInfoReq->SetSelector==ZERO_SETSELECTOR)
            {
                pGetSysInfoRes->SysInfo.PrimOSInfo.PrimaryOSName[0]=pSysInfoCfg->PrimaryOSName.TypeOfEncoding_PrimaryOSName;
                pGetSysInfoRes->SysInfo.PrimOSInfo.PrimaryOSName[1]=pSysInfoCfg->PrimaryOSName.StringLength_PrimaryOSName;
                _fmemcpy(&pGetSysInfoRes->SysInfo.PrimOSInfo.PrimaryOSName[2], &pSysInfoCfg->PrimaryOSName.PrimaryOperatingSystemName, MAX_STRING_LENGTH_COPY);
                resSize += MAX_BLOCK_SIZE;
            }
            else
            {
                _fmemcpy(&pGetSysInfoRes->SysInfo.PrimOSInfo.PrimaryOSName[0],
                        &pSysInfoCfg->PrimaryOSName.PrimaryOperatingSystemName[(pGetSysInfoReq->SetSelector * MAX_BLOCK_SIZE) - 2],MAX_BLOCK_SIZE);
                resSize += MAX_BLOCK_SIZE;
            }
            break;

        case OS_NAME_PARAM:
            /*Parameter 4 volatile*/
            _fmemset(&pGetSysInfoRes->SysInfo.OSInfo,0,sizeof(OSInfo_T));
            if((pGetSysInfoReq->SetSelector >= MAX_BLOCK_SIZE)||(0x00 != pGetSysInfoReq->BlockSelector))
            {
                pGetSysInfoRes->CompletionCode = CC_INV_DATA_FIELD;
                return sizeof(pGetSysInfoRes->CompletionCode);
            }
            pGetSysInfoRes->SysInfo.OSInfo.SetSelector  = pGetSysInfoReq->SetSelector;
            resSize++;
            if(pGetSysInfoReq->SetSelector==ZERO_SETSELECTOR)
            {
                LOCK_BMC_SHARED_MEM(BMCInst);
                pGetSysInfoRes->SysInfo.OSInfo.OperatingSystemName[0]=BMC_GET_SHARED_MEM(BMCInst)->OperatingSystemName.TypeOfEncoding_OSName;
                pGetSysInfoRes->SysInfo.OSInfo.OperatingSystemName[1]=BMC_GET_SHARED_MEM(BMCInst)->OperatingSystemName.StringLength_OSName;
                _fmemcpy(&pGetSysInfoRes->SysInfo.OSInfo.OperatingSystemName[2], BMC_GET_SHARED_MEM(BMCInst)->OperatingSystemName.OSName, MAX_STRING_LENGTH_COPY);
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                resSize += MAX_BLOCK_SIZE;
            }
            else
            {
                LOCK_BMC_SHARED_MEM(BMCInst);
                _fmemcpy(&pGetSysInfoRes->SysInfo.OSInfo.OperatingSystemName[0],
                        &BMC_GET_SHARED_MEM(BMCInst)->OperatingSystemName.OSName[(pGetSysInfoReq->SetSelector * MAX_BLOCK_SIZE) - 2],MAX_BLOCK_SIZE);
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                resSize += MAX_BLOCK_SIZE;
            }

            break;

        default:
#if 0
            if(g_PDKHandle[PDK_GETSYSINFOPARAM] != NULL &&
                    (pGetSysInfoReq->ParamSelector >= 192 && pGetSysInfoReq->ParamSelector <= 255))
            {
                oem_len = ((int(*)(INT8U, unsigned long*,int))(g_PDKHandle[PDK_GETSYSINFOPARAM]))(pGetSysInfoReq->ParamSelector, &oem_addr ,BMCInst);
                if( oem_len == 0)
                {
                    pGetSysInfoRes->CompletionCode = CC_SYS_INFO_PARAM_NOT_SUPPORTED;
                    return sizeof(INT8U);
                }
                else
                {
                    //Acquire the OEM parameters
                    if( oem_len < MSG_PAYLOAD_SIZE - sizeof(GetLanCCRev_T))
                    {
                        memcpy((char*)pGetSysInfoOEMRes + sizeof(GetSystemInfoParamOEMRes_T) ,\
                                (unsigned int*)oem_addr , oem_len);
                    }
                    else
                    {
                        pGetSysInfoRes->CompletionCode = CC_SYS_INFO_PARAM_NOT_SUPPORTED;
                        return sizeof(INT8U);
                    }
                    return sizeof(GetSystemInfoParamOEMRes_T) + oem_len;
                }
            }
            else
#endif
            {
                pGetSysInfoRes->CompletionCode = CC_SYS_INFO_PARAM_NOT_SUPPORTED;
                return sizeof(pGetSysInfoRes->CompletionCode);
            }
    }
    /* return the size of the response */
    return resSize;
}



/*---------------------------------------
 * SetSystemInfoParam
 *---------------------------------------*/
    int
SetSystemInfoParam (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    _NEAR_ SetSystemInfoParamReq_T*	pSetSysInfoReq = (_NEAR_ SetSystemInfoParamReq_T*)pReq;
    _NEAR_ SetSystemInfoParamOEMReq_T*	pSetSysInfoOEMReq = (_NEAR_ SetSystemInfoParamOEMReq_T*)pReq;
    _NEAR_ SetSystemInfoParamRes_T*	pSetSysInfoRes = (_NEAR_ SetSystemInfoParamRes_T*)pRes;
    BMCInfo_t *pBMCInfo = &g_BMCInfo[BMCInst];
    _FAR_ SystemInfoConfig_T*	pSysInfoCfg;
    INT8U *pSetInProgress;
    unsigned long oem_addr[2]={0};      // use oem_addr[1] as read-only/write-only flag
    int size;
#if 0
    //If the OEM parameter is existing, then skip the length check.
    if(g_PDKHandle[PDK_SETSYSINFOPARAM] != NULL && (pSetSysInfoReq->ParamSelector < 192 )){
        if( !( (SET_IN_PROGRESS_PARAM == pSetSysInfoReq->ParamSelector && ( ReqLen == ( sizeof(pSetSysInfoReq->ParamSelector) + sizeof(INT8U) /* for Data */ ) )) ||
                    ( pSetSysInfoReq->ParamSelector < MAX_PARAM_SELECTOR && ReqLen == ( sizeof(pSetSysInfoReq->ParamSelector) + sizeof(INT8U) /* for set Selector */ + MAX_BLOCK_SIZE))) )
        {
            pSetSysInfoRes->CompletionCode = CC_REQ_INV_LEN;
            return sizeof(pSetSysInfoRes->CompletionCode);
        }
    }
#endif
    pSetInProgress = &BMC_GET_SHARED_MEM(BMCInst)->m_Sys_SetInProgress;

    pSetSysInfoRes->CompletionCode = CC_NORMAL;

    /*Get NVRAM System Info Configuration parameters */
    pSysInfoCfg = &pBMCInfo->SystemInfoConfig;

    switch (pSetSysInfoReq->ParamSelector)
    {
        /*Parameter 0 volatile */
        case SET_IN_PROGRESS_PARAM:
            LOCK_BMC_SHARED_MEM(BMCInst);
            /* If Request for Set In progress */
            if(( SYS_INFO_SET_IN_PROGRESS == *pSetInProgress ) &&
                    ( SYS_INFO_SET_IN_PROGRESS == pSetSysInfoReq->SysInfo.SetInProgress ))
            {
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                /* Trying to SetinProgress when already in set*/
                pSetSysInfoRes->CompletionCode = CC_SYS_INFO_SET_IN_PROGRESS ;
                return sizeof(pSetSysInfoRes->CompletionCode);
            } else if( SYS_INFO_COMMIT_WRITE == pSetSysInfoReq->SysInfo.SetInProgress )
            {
                /* Write SysInfoConfig to NVR */
                //                FlushIPMI((INT8U*)&pBMCInfo->SystemInfoConfig,(INT8U*)&pBMCInfo->SystemInfoConfig,
                //                                  pBMCInfo->IPMIConfLoc.SystemInfoConfigAddr,sizeof(SystemInfoConfig_T),BMCInst);
                /* Write volatile data to the BMC Shared memory */

            } else if ( SYS_INFO_SET_COMPLETE == pSetSysInfoReq->SysInfo.SetInProgress )
            {
                //PMCONFIG_FILE(BMCInst,PMConfigFile);
                /* Set it to set Complete */
                *pSetInProgress = pSetSysInfoReq->SysInfo.SetInProgress;
                //                FlushIPMI((INT8U*)&pBMCInfo->SystemInfoConfig,(INT8U*)&pBMCInfo->SystemInfoConfig,
                //                                  pBMCInfo->IPMIConfLoc.SystemInfoConfigAddr,sizeof(SystemInfoConfig_T),BMCInst);
            } else if ( SYS_INFO_SET_IN_PROGRESS != pSetSysInfoReq->SysInfo.SetInProgress )
            {
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                pSetSysInfoRes->CompletionCode = CC_INV_DATA_FIELD;
                return sizeof(pSetSysInfoRes->CompletionCode);
            }

            *pSetInProgress = pSetSysInfoReq->SysInfo.SetInProgress;
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            break;

        case SYS_FW_VERSION_PARAM:

            if(pSetSysInfoReq->SysInfo.SysVerInfo.SetSelector >= MAX_BLOCK_SIZE)
            {
                pSetSysInfoRes->CompletionCode = CC_INV_DATA_FIELD;
                return sizeof(pSetSysInfoRes->CompletionCode);
            }
            if(pSetSysInfoReq->SysInfo.SysNameInfo.SetSelector==ZERO_SETSELECTOR)
            {
                if(pSetSysInfoReq->SysInfo.SysVerInfo.SysFWVersion[1]> MAX_FW_VER_LENGTH)
                {
                    pSetSysInfoRes->CompletionCode = CC_INV_DATA_FIELD;
                    return sizeof(pSetSysInfoRes->CompletionCode);
                }
                if(pSetSysInfoReq->SysInfo.SysVerInfo.SysFWVersion[0] > MAX_TYPE_OF_ENCODING)
                {
                    pSetSysInfoRes->CompletionCode = CC_INV_DATA_FIELD;
                    return sizeof(pSetSysInfoRes->CompletionCode);
                }
                pSysInfoCfg->SysFWVersion.TypeOfEncoding=pSetSysInfoReq->SysInfo.SysVerInfo.SysFWVersion[0];
                pSysInfoCfg->SysFWVersion.StringLength=pSetSysInfoReq->SysInfo.SysVerInfo.SysFWVersion[1];

                _fmemcpy(&pSysInfoCfg->SysFWVersion.SysFWVersionName[0],&pSetSysInfoReq->SysInfo.SysVerInfo.SysFWVersion[2], MAX_STRING_LENGTH_COPY);
                //                     FlushIPMI((INT8U*)&pBMCInfo->SystemInfoConfig,(INT8U*)&pBMCInfo->SystemInfoConfig.SysFWVersion,
                //                                                 pBMCInfo->IPMIConfLoc.SystemInfoConfigAddr,sizeof(SysFWVersion_T),BMCInst);
            }
            else
            {
                _fmemcpy(&pSysInfoCfg->SysFWVersion.SysFWVersionName[(pSetSysInfoReq->SysInfo.SysVerInfo.SetSelector* MAX_BLOCK_SIZE) - 2],
                        &pSetSysInfoReq->SysInfo.SysVerInfo.SysFWVersion[0],MAX_BLOCK_SIZE);
                //                    FlushIPMI((INT8U*)&pBMCInfo->SystemInfoConfig,
                //                                (INT8U*)&pBMCInfo->SystemInfoConfig.SysFWVersion,pBMCInfo->IPMIConfLoc.SystemInfoConfigAddr,sizeof(SysFWVersion_T),BMCInst);
            }
            break;

        case SYS_NAME_PARAM:
            if(pSetSysInfoReq->SysInfo.SysNameInfo.SetSelector >= MAX_BLOCK_SIZE )
            {
                pSetSysInfoRes->CompletionCode	= CC_INV_DATA_FIELD;
                return sizeof(pSetSysInfoRes->CompletionCode);
            }
            if(pSetSysInfoReq->SysInfo.SysNameInfo.SetSelector==ZERO_SETSELECTOR)
            {
                if((pSetSysInfoReq->SysInfo.SysNameInfo.SysName[1] > MAX_SYS_NAME_LENGTH))
                {
                    pSetSysInfoRes->CompletionCode	= CC_INV_DATA_FIELD;
                    return sizeof(pSetSysInfoRes->CompletionCode);
                }
                if(pSetSysInfoReq->SysInfo.SysNameInfo.SysName[0]>MAX_TYPE_OF_ENCODING)
                {
                    pSetSysInfoRes->CompletionCode	= CC_INV_DATA_FIELD;
                    return sizeof(pSetSysInfoRes->CompletionCode);
                }
                pSysInfoCfg->SysName.TypeOfEncoding_Sys_Name=pSetSysInfoReq->SysInfo.SysNameInfo.SysName[0];
                pSysInfoCfg->SysName.StringLength_Sys_Name=pSetSysInfoReq->SysInfo.SysNameInfo.SysName[1];

                _fmemcpy(&pSysInfoCfg->SysName.SystemName[0], &pSetSysInfoReq->SysInfo.SysNameInfo.SysName[2],MAX_STRING_LENGTH_COPY);
                //                    FlushIPMI((INT8U*)&pBMCInfo->SystemInfoConfig,(INT8U*)&pBMCInfo->SystemInfoConfig.SysName,
                //                                            pBMCInfo->IPMIConfLoc.SystemInfoConfigAddr,sizeof(SysName_T),BMCInst);
            }
            else
            {
                _fmemcpy(&pSysInfoCfg->SysName.SystemName[(pSetSysInfoReq->SysInfo.SysVerInfo.SetSelector* MAX_BLOCK_SIZE) - 2],
                        &pSetSysInfoReq->SysInfo.SysNameInfo.SysName,MAX_BLOCK_SIZE);
                //                    FlushIPMI((INT8U*)&pBMCInfo->SystemInfoConfig,(INT8U*)&pBMCInfo->SystemInfoConfig.SysName,
                //                                pBMCInfo->IPMIConfLoc.SystemInfoConfigAddr,sizeof(SysName_T),BMCInst);
            }
            break;

        case PRIM_OS_NAME_PARAM:
            if(pSetSysInfoReq->SysInfo.PrimOSInfo.SetSelector >= MAX_BLOCK_SIZE )
            {
                pSetSysInfoRes->CompletionCode = CC_INV_DATA_FIELD;
                return sizeof(pSetSysInfoRes->CompletionCode);
            }
            if(pSetSysInfoReq->SysInfo.PrimOSInfo.SetSelector==ZERO_SETSELECTOR)
            {
                if((pSetSysInfoReq->SysInfo.PrimOSInfo.PrimaryOSName[1] > MAX_PRIM_OS_NAME_LENGTH))
                {
                    pSetSysInfoRes->CompletionCode	= CC_INV_DATA_FIELD;
                    return sizeof(pSetSysInfoRes->CompletionCode);

                }
                if(pSetSysInfoReq->SysInfo.PrimOSInfo.PrimaryOSName[0]>MAX_TYPE_OF_ENCODING)
                {
                    pSetSysInfoRes->CompletionCode	= CC_INV_DATA_FIELD;
                    return sizeof(pSetSysInfoRes->CompletionCode);
                }

                pSysInfoCfg->PrimaryOSName.TypeOfEncoding_PrimaryOSName=pSetSysInfoReq->SysInfo.PrimOSInfo.PrimaryOSName[0];
                pSysInfoCfg->PrimaryOSName.StringLength_PrimaryOSName=pSetSysInfoReq->SysInfo.PrimOSInfo.PrimaryOSName[1];

                _fmemcpy(&pSysInfoCfg->PrimaryOSName.PrimaryOperatingSystemName[0], &pSetSysInfoReq->SysInfo.PrimOSInfo.PrimaryOSName[2], MAX_STRING_LENGTH_COPY);
                //                    FlushIPMI((INT8U*)&pBMCInfo->SystemInfoConfig,(INT8U*)&pBMCInfo->SystemInfoConfig.PrimaryOSName,
                //                                                    pBMCInfo->IPMIConfLoc.SystemInfoConfigAddr,sizeof(PrimaryOSName_T),BMCInst);
            }
            else
            {
                _fmemcpy(&pSysInfoCfg->PrimaryOSName.PrimaryOperatingSystemName[(pSetSysInfoReq->SysInfo.PrimOSInfo.SetSelector * MAX_BLOCK_SIZE) - 2],
                        &pSetSysInfoReq->SysInfo.PrimOSInfo.PrimaryOSName, MAX_BLOCK_SIZE);
                //                    FlushIPMI((INT8U*)&pBMCInfo->SystemInfoConfig,
                //                                (INT8U*)&pBMCInfo->SystemInfoConfig.PrimaryOSName,pBMCInfo->IPMIConfLoc.SystemInfoConfigAddr,sizeof(PrimaryOSName_T),BMCInst);
            }
            break;

        case OS_NAME_PARAM:
            /*Parameter 4 volatile*/
            if(pSetSysInfoReq->SysInfo.OSInfo.SetSelector >= MAX_BLOCK_SIZE )
            {
                pSetSysInfoRes->CompletionCode	= CC_INV_DATA_FIELD;
                return sizeof(pSetSysInfoRes->CompletionCode);
            }
            if(pSetSysInfoReq->SysInfo.OSInfo.SetSelector==ZERO_SETSELECTOR)
            {
                if(pSetSysInfoReq->SysInfo.OSInfo.OperatingSystemName[1] > MAX_OS_NAME_LENGTH)
                {
                    pSetSysInfoRes->CompletionCode	= CC_INV_DATA_FIELD;
                    return sizeof(pSetSysInfoRes->CompletionCode);
                }
                if(pSetSysInfoReq->SysInfo.OSInfo.OperatingSystemName[0] > MAX_TYPE_OF_ENCODING)
                {
                    pSetSysInfoRes->CompletionCode	= CC_INV_DATA_FIELD;
                    return sizeof(pSetSysInfoRes->CompletionCode);

                }
                LOCK_BMC_SHARED_MEM(BMCInst);
                BMC_GET_SHARED_MEM(BMCInst)->OperatingSystemName.TypeOfEncoding_OSName=pSetSysInfoReq->SysInfo.OSInfo.OperatingSystemName[0];
                BMC_GET_SHARED_MEM(BMCInst)->OperatingSystemName.StringLength_OSName=pSetSysInfoReq->SysInfo.OSInfo.OperatingSystemName[1];
                _fmemcpy(&BMC_GET_SHARED_MEM(BMCInst)->OperatingSystemName.OSName, &pSetSysInfoReq->SysInfo.OSInfo.OperatingSystemName[2],
                        MAX_STRING_LENGTH_COPY);
                UNLOCK_BMC_SHARED_MEM (BMCInst);
            }
            else
            {
                LOCK_BMC_SHARED_MEM(BMCInst);
                _fmemcpy(&(BMC_GET_SHARED_MEM(BMCInst)->OperatingSystemName.OSName[(pSetSysInfoReq->SysInfo.OSInfo.SetSelector * MAX_BLOCK_SIZE) - 2]),
                        &pSetSysInfoReq->SysInfo.OSInfo.OperatingSystemName,MAX_BLOCK_SIZE);
                UNLOCK_BMC_SHARED_MEM (BMCInst);
            }
            break;

        default:
#if 0
            if(g_PDKHandle[PDK_SETSYSINFOPARAM] != NULL &&
                    (pSetSysInfoReq->ParamSelector >= 192 && pSetSysInfoReq->ParamSelector <= 255))
            {
                oem_addr[0] = (unsigned long)((char*)pSetSysInfoOEMReq + sizeof(SetSystemInfoParamOEMReq_T));
                size = ((int(*)(INT8U, unsigned long*,int))(g_PDKHandle[PDK_SETSYSINFOPARAM]))(pSetSysInfoReq->ParamSelector, oem_addr ,BMCInst);
                if(size <= 0)
                {
                    switch (oem_addr[1]) {
                        case CC_SYS_INFO_READ_ONLY_PARAM:
                            pSetSysInfoRes->CompletionCode = CC_SYS_INFO_READ_ONLY_PARAM;
                            break;
                        default:
                            pSetSysInfoRes->CompletionCode = CC_PARAM_NOT_SUPPORTED;
                    }
                }
                else
                {
                    pSetSysInfoRes->CompletionCode = CC_SYS_INFO_PARAM_NOT_SUPPORTED;
                    return sizeof(pSetSysInfoRes->CompletionCode);
                }
            }
#else
            break;
#endif
    }
    return sizeof(pSetSysInfoRes->CompletionCode);
}

    int
GetDevId (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    GetDevIDRes_T *pDevInfo = (GetDevIDRes_T *)pRes;
    pDevInfo->CompletionCode = CC_NORMAL;
    pDevInfo->DeviceID = 0x11;
    pDevInfo->DevRevision = 0x22;
    pDevInfo->FirmwareRevision1 = 0x33;
    pDevInfo->FirmwareRevision1 = 0x44;
    pDevInfo->IPMIVersion = 0x55;
    pDevInfo->DevSupport = 0x66;
    pDevInfo->MfgID[0] = 0x77;
    pDevInfo->MfgID[1] = 0x88;
    pDevInfo->MfgID[2] = 0x99;
    pDevInfo->ProdID = 0xaa;
    pDevInfo->AuxFirmwareRevision = 0xbb;
    print_green("size: %lu", sizeof(GetDevIDRes_T));
    return sizeof (GetDevIDRes_T);
}
#endif  /* APP_DEVICE */
