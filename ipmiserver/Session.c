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
 * Session.c
 * Session related functions
 *
 * Author: Govind Kothandapani <govindk@ami.com>
 * 		 : Rama Bisa <ramab@ami.com>
 *       : Basavaraj Astekar <basavaraja@ami.com>
 *       : Bakka Ravinder Reddy <bakkar@ami.com>
 *
 *****************************************************************/
#define ENABLE_DEBUG_MACROS    0
#include "Types.h"
#include "IPMIDefs.h"
#include "MsgHndlr.h"
#include "PMConfig.h"
#include "SharedMem.h"
//#include "NVRAccess.h"
#include "Session.h"
#include "Debug.h"
//#include "SerialRegs.h"
#include "SensorEvent.h"
//#include "IPMI_Sensor.h"
#include "Support.h"
#include "Ethaddr.h"
#include "AppDevice.h"
#include "Platform.h"
#include "IPMIConf.h"
//#include "SOL.h"
#include "nwcfg.h"
//#include "blowfish.h"
#include "featuredef.h"
#include "IPMIDefs.h"
#include "IPMI_AMI.h"
//#include "PDKCmdsAccess.h"
#include <sys/sysinfo.h>

#define TOTAL_INFINITE_CMDS  sizeof(m_InfiniteCmdsTbl)/sizeof(IPMICmdsFilterTbl_T)

static IPMICmdsFilterTbl_T m_InfiniteCmdsTbl [] =
{     /* NetFn */       /*  Command#   */
    {   NETFN_AMI,  CMD_AMI_YAFU_COMMON_NAK             },
    {   NETFN_AMI,  CMD_AMI_YAFU_GET_FLASH_INFO         },	
    {   NETFN_AMI,	CMD_AMI_YAFU_GET_FIRMWARE_INFO	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_GET_FMH_INFO	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_GET_STATUS,	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_ACTIVATE_FLASH	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_ALLOCATE_MEMORY	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_FREE_MEMORY	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_READ_FLASH	            },
    {   NETFN_AMI,	CMD_AMI_YAFU_WRITE_FLASH	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_ERASE_FLASH	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_PROTECT_FLASH	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_ERASE_COPY_FLASH	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_VERIFY_FLASH	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_READ_MEMORY	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_WRITE_MEMORY	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_COPY_MEMORY	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_COMPARE_MEMORY	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_CLEAR_MEMORY	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_GET_BOOT_CONFIG	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_SET_BOOT_CONFIG	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_GET_BOOT_VARS	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_DEACTIVATE_FLASH_MODE  },
    {   NETFN_AMI,	CMD_AMI_YAFU_RESET_DEVICE	    },
    {   NETFN_AMI,	CMD_AMI_YAFU_GET_ECF_STATUS         },
    {   NETFN_AMI,	CMD_AMI_YAFU_GET_VERIFY_STATUS	    },
    {   NETFN_AMI,	CMD_AMI_GET_CHANNEL_NUM	            },
    {   NETFN_AMI,	CMD_AMI_GET_ETH_INDEX	            },
    {   NETFN_AMI,	CMD_AMI_START_TFTP_FW_UPDATE    },
    {   NETFN_AMI,	CMD_AMI_GET_TFTP_FW_PROGRESS_STATUS },
    {   NETFN_AMI,	CMD_AMI_SET_FW_CONFIGURATION    },
    {   NETFN_AMI,       CMD_AMI_GET_FW_CONFIGURATION    },
    {   NETFN_AMI,       CMD_AMI_SET_FW_PROTOCOL},
    {   NETFN_AMI,       CMD_AMI_GET_FW_PROTOCOL},

};

/*********************************************************************************************
Name	:	SessionTimeOutTask
Input	:	void
Output	:	void
This program  checks for session timeout
 *********************************************************************************************/
void SessionTimerTask (int BMCInst)
{
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    _FAR_	SessionTblInfo_T*	pSessionTblInfo = &pBMCInfo->SessionTblInfo;
    INT8U				Index;

    for (Index=0; Index < pBMCInfo->IpmiConfig.MaxSession; Index++)
    {
        if (FALSE == pSessionTblInfo->SessionTbl[Index].Used)
        {
            continue;
        }

        if(pBMCInfo->IpmiConfig.SerialIfcSupport == 1)
        {
            if (pBMCInfo->SERIALch == pSessionTblInfo->SessionTbl[Index].Channel)
            {                
                if (!(pBMCInfo->SMConfig.SessionTermination & 0x02)) /* If Session Inactivity timeout disabled */
                {
                    continue;
                }
                else if (0 == pBMCInfo->SMConfig.SessionInactivity) /* Never Time Out */
                {
                    continue;
                }
            }
        }

        if(pBMCInfo->IpmiConfig.SOLIfcSupport == 1)
        {
            if(pSessionTblInfo->SessionTbl[Index].SessPyldInfo [PAYLOAD_SOL].Type == PAYLOAD_SOL)
            {     
                if (pBMCInfo->IpmiConfig.SOLSessionTimeOut == 0 ) /* Never Time Out */
                {
                    continue;
                }
            }
        }

        if(GetLinkStatus(pSessionTblInfo->SessionTbl[Index].Channel,BMCInst) !=0)
        {
            if (pSessionTblInfo->SessionTbl[Index].TimeOutValue > 0)
            {
                pSessionTblInfo->SessionTbl[Index].TimeOutValue--;
                continue;
            }
        }
        else
        {
            pSessionTblInfo->SessionTbl[Index].Linkstat = TRUE;
            continue;
        }

        IPMI_DBG_PRINT ("\nSessionTimerTask: Session Time Out Occured\n");
        IPMI_DBG_PRINT_2 ("SessionID = 0x%lX  Num of Sessions = %X\n", pSessionTblInfo->SessionTbl[Index].SessionID,
                pSessionTblInfo->Count);

        if(pBMCInfo->IpmiConfig.SerialIfcSupport == 1)
        {
            if (pBMCInfo->SERIALch == pSessionTblInfo->SessionTbl[Index].Channel)
            {
                BMC_GET_SHARED_MEM (BMCInst)->SerialSessionActive = FALSE;
            }
        }

        /* Delete the Session from session table */
        DeleteSession (&pSessionTblInfo->SessionTbl[Index],BMCInst);
    }
}


/*********************************************************************************************
Name	:	getChannelInfo
Input	:	ch	-	ChannelNumber
Output	:	channelInformations
This program  returns informations about the channel
 *********************************************************************************************/
_FAR_ ChannelInfo_T* getChannelInfo (INT8U ch,int BMCInst)
{
    INT8U Index;
    ChcfgInfo_T *pChannelInfo=NULL;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    if(IsLANChannel(ch, BMCInst))
    {
        if(pBMCInfo->IpmiConfig.LANIfcSupport != 1)
        {
            return NULL;
        }
    }else
    {
        if( PRIMARY_IPMB_CHANNEL == ch && pBMCInfo->IpmiConfig.PrimaryIPMBSupport != 1 )
        {
            return NULL;
        }
        else if (pBMCInfo->IpmiConfig.SecondaryIPMBSupport != 1 && (pBMCInfo->SecondaryIPMBCh != CH_NOT_USED && ch == pBMCInfo->SecondaryIPMBCh))
        {
            return NULL;
        }
        else if (pBMCInfo->IpmiConfig.SerialIfcSupport != 1 && (pBMCInfo->SERIALch != CH_NOT_USED && ch == pBMCInfo->SERIALch))
        {
            return NULL;
        }
        else if (pBMCInfo->IpmiConfig.ICMBIfcSupport != 1 && (pBMCInfo->ICMBCh != CH_NOT_USED && ch == pBMCInfo->ICMBCh))
        {
            return NULL;
        }
        else if (pBMCInfo->IpmiConfig.SMBUSIfcSupport !=1 && (pBMCInfo->SMBUSCh != CH_NOT_USED && ch == pBMCInfo->SMBUSCh))
        {
            return NULL;
        }
        else if (pBMCInfo->IpmiConfig.USBIfcSupport != 1 && (ch == USB_CHANNEL))
        {
            return NULL;
        }
        else if (pBMCInfo->IpmiConfig.SMMIfcSupport != 1 && (pBMCInfo->SMMCh != CH_NOT_USED && ch == pBMCInfo->SMMCh))
        {
            return NULL;
        }
        else if(pBMCInfo->IpmiConfig.SYSIfcSupport !=1 && ch == SYS_IFC_CHANNEL)
        {
            return NULL;
        }
    }

    for(Index=0;Index<MAX_NUM_CHANNELS;Index++)
    {
        if(pBMCInfo->ChConfig[Index].ChType != 0xff)
        {
            pChannelInfo = &pBMCInfo->ChConfig[Index];
            //printf("Channel numb is %x %x \n",pChannelInfo->ChannelInfo.ChannelNumber,ch);
            if(pChannelInfo->ChannelInfo.ChannelNumber == ch)
            {
                return (ChannelInfo_T *)&pBMCInfo->ChConfig[Index].ChannelInfo;
            }
        }
    }

    return NULL;
}

/**
 * @macro  GetNVRUsrCfgAddr
 * @brief Gets NVR UsrCfg address from RAM
 * @param NVRHandle   -  Handle for NVRAM
 *@return Returns the handle of NVR address from RAM
 **/
    INT8U*
GetNVRUsrCfgAddr(INT32U NVRHandle, int BMCInst) 
{
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    return ((INT8U *)&pBMCInfo->UserInfo[NVRHandle]);
}

/*********************************************************************************************
Name	:	CheckForDuplicateUsers
Input	:	UserName - Name of the User
Output	:	returns 0 for success and -1 for failure
This program  returns Informations about the user
 *********************************************************************************************/
INT8U CheckForDuplicateUsers (_NEAR_ INT8U* UserName, int BMCInst)
{
    int i = 0;
    _FAR_ UserInfo_T* pUserTable = (UserInfo_T *) GetNVRUsrCfgAddr (NVRH_USERCONFIG, BMCInst);
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    for(i=0; i<=pBMCInfo->IpmiConfig.MaxUsers; i++)
    {
        if(pUserTable[i].UserId != 0)
        {
            if (0 == _fmemcmp(pUserTable[i].UserName, UserName, MAX_USERNAME_LEN))
                return FALSE;
        }
    }	
    return TRUE;    
}

/*********************************************************************************************
Name	:	getUserIdInfo
Input	:	UserID - User ID
Output	:	User information
This program  returns Informations about the user
 *********************************************************************************************/
_FAR_ UserInfo_T*	getUserIdInfo (INT8U UserId, int BMCInst)
{
    _FAR_ UserInfo_T* pUserTable = (UserInfo_T *) GetNVRUsrCfgAddr (NVRH_USERCONFIG, BMCInst);
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    if (UserId == 0) { return NULL; }

    if (UserId <= pBMCInfo->IpmiConfig.MaxUsers)
    {
        return &pUserTable[UserId-1];
    }
    else
    {
        return NULL;
    }
}

/*********************************************************************************************
Name	:	getChUserPrivInfo
Input	:	userName - user name
Role - requested role
chIndex - channel's user index
pChUserInfo - channel's user information
Output	:	channel's matching user Information

This program returns information about the user for the given channel,
& Index of the user in Channel User Array.
 *********************************************************************************************/
_FAR_ ChannelUserInfo_T* getChUserPrivInfo (_NEAR_ char *userName, _NEAR_ INT8U Role, _NEAR_ INT8U* chIndex, _FAR_ ChannelUserInfo_T *pChUserInfo, int BMCInst)
{
    _FAR_ UserInfo_T* pUserTable = (UserInfo_T *) GetNVRUsrCfgAddr (NVRH_USERCONFIG, BMCInst);
    int userIndex;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    /* search the table */
    for(userIndex = 0; userIndex < pBMCInfo->IpmiConfig.MaxUsers; userIndex++)
    {
        for(*chIndex = 0; *chIndex < pBMCInfo->IpmiConfig.MaxChUsers; (*chIndex)++)
        {
            if ((Role == pChUserInfo[*chIndex].AccessLimit) &&
                    (1 == pUserTable[userIndex].UserStatus) &&
                    (pUserTable[userIndex].UserId == pChUserInfo[*chIndex].UserId))
            {
                /* if userName is not NULL then it is username/privilege
                   lookup else it is name_only lookup */
                if (0 != *userName)
                {
                    if (0 != _fmemcmp(pUserTable[userIndex].UserName, userName, MAX_USERNAME_LEN))
                    {
                        continue;
                    }
                }
                return (pChUserInfo + *chIndex);
            }
        }
    }
    return NULL;
}

/*********************************************************************************************
Name	:	getChUserInfo
Input	:	userName - user name
chIndex - to return Index of user in channelUser Array
pChUserInfo - channel's user information
Output	:	channel's matching user Information

This program returns information about the user for the given channel,
& Index of the user in Channel User Array.
 *********************************************************************************************/
_FAR_ ChannelUserInfo_T* getChUserInfo (_NEAR_ char *userName,  _NEAR_ INT8U* chIndex, _FAR_ ChannelUserInfo_T *pChUserInfo,int BMCInst)
{
    _FAR_ UserInfo_T* pUserTable = (UserInfo_T *) GetNVRUsrCfgAddr (NVRH_USERCONFIG, BMCInst);
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    int userIndex;

    /* search the table */
    for(userIndex = 0; userIndex < pBMCInfo->IpmiConfig.MaxUsers; userIndex++)
    {
        for(*chIndex = 0; *chIndex < pBMCInfo->IpmiConfig.MaxChUsers; (*chIndex)++)
        {
            if ((0 == _fmemcmp(pUserTable[userIndex].UserName, userName, MAX_USERNAME_LEN)) &&
                    /* Commented to return the pointer for disabled user */ 
                    /* (1 == pUserTable[userIndex].UserStatus) && */
                    (pUserTable[userIndex].UserId == pChUserInfo[*chIndex].UserId))
                return (pChUserInfo + *chIndex);
        }
    }
    return NULL;
}

/*********************************************************************************************
Name	:	getChUserIdInfo
Input	:	userId - User ID
Index - to return Index of user in channelUser Array.
pChUserInfo - channel's user information
Output	:	channel's matching user Information

This program returns information about the user for the given channel,
& Index of the user inChannel User Array.

 *********************************************************************************************/
_FAR_ ChannelUserInfo_T* getChUserIdInfo (INT8U userId, _NEAR_ INT8U *Index, _FAR_ ChannelUserInfo_T*	pChUserInfo, int BMCInst)
{
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    if (userId == 0) { return NULL; }

    for(*Index=0; *Index < pBMCInfo->IpmiConfig.MaxChUsers; (*Index)++)
    {
        if ((pChUserInfo->UserId == userId) && ((pChUserInfo->ID == USER_ID )))
        {
            return pChUserInfo;
        }
        pChUserInfo++;
    }
    return NULL;
}

/*********************************************************************************************
Name	:	GetNVRChConfigs
Input	:	pChannelInfo -Channel Information
Filename - Channel Name Information
Output	:	Respective Channel's Information

This program returns information about the respective channel.

 *********************************************************************************************/
ChannelInfo_T* GetNVRChConfigs(ChannelInfo_T *pChannelInfo, int BMCInst)
{
    int i=0;
    ChannelInfo_T *pNVRChInfo=NULL;

    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    for(i=0;i<MAX_NUM_CHANNELS;i++)
    {
        if( pBMCInfo->NVRChcfgInfo[i].ChType != 0xff)
        {
            pNVRChInfo = &pBMCInfo->NVRChcfgInfo[i].ChannelInfo;
            if(pChannelInfo->ChannelNumber == pNVRChInfo->ChannelNumber)
            {
                return (ChannelInfo_T *)&pBMCInfo->NVRChcfgInfo[i].ChannelInfo;
            }
        }
    }
    return NULL;
}

/**
 *@fn GetNVRChUserConfigs 
 *@brief This function is invoked to get NVR User informations of the channel
 *@param pChannelInfo - Channel Information
 *@param Filename - Size of the SDR repository
 *@return Returns Address of user information for the channel on success
 *              Returns NULL on Failure
 */
ChannelUserInfo_T* GetNVRChUserConfigs(ChannelInfo_T *pChannelInfo, int BMCInst)
{
    int i=0;
    ChannelInfo_T *pNVRChInfo = NULL;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    for(i=0;i<MAX_NUM_CHANNELS;i++)
    {
        if(pBMCInfo->NVRChcfgInfo [i].ChType != 0xff)
        {
            pNVRChInfo = &pBMCInfo->NVRChcfgInfo[i].ChannelInfo;
            if(pChannelInfo->ChannelNumber == pNVRChInfo->ChannelNumber)
            {
                return (ChannelUserInfo_T *)&pBMCInfo->NVRChcfgInfo[i].ChannelInfo.ChannelUserInfo[0];
            }
        }
    }
    return NULL;
}



/*********************************************************************************************
Name	:	getSessionInfo
Input	:	Arg - Tells the type of data passed in Session
Session - Either one of these: session ID, session handle, session index or
channel number
Output	:	Session Information

This program  returns the session information.
 *********************************************************************************************/
_FAR_ SessionInfo_T* getSessionInfo (INT8U Arg, _FAR_ void *Session,int BMCInst)
{
    INT8U				Index;
    INT8U				ActiveSesIndex = 0;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    _FAR_	SessionTblInfo_T*	pSessionTblInfo = &pBMCInfo->SessionTblInfo;

    for (Index = 0; Index < pBMCInfo->IpmiConfig.MaxSession; Index++)
    {
        if (FALSE == pSessionTblInfo->SessionTbl[Index].Used)
        {
            continue;
        }
        if (TRUE == pSessionTblInfo->SessionTbl[Index].Activated)
        {
            ActiveSesIndex++;
        }

        switch (Arg)
        {
            case SESSION_ID_INFO:
                if(pSessionTblInfo->SessionTbl[Index].SessionID == *((INT32U *)Session) )
                {
                    return &pSessionTblInfo->SessionTbl[Index];
                }
                break;

            case SESSION_REMOTE_INFO:
                if(pSessionTblInfo->SessionTbl[Index].RemConSessionID == *((INT32U *)Session) )
                {
                    return &pSessionTblInfo->SessionTbl[Index];
                }
                break;

            case SESSION_HANDLE_INFO:
                if (pSessionTblInfo->SessionTbl[Index].SessionHandle == *((_FAR_ INT8U*)Session) && pSessionTblInfo->SessionTbl[Index].Activated)
                {
                    return &pSessionTblInfo->SessionTbl[Index];
                }
                break;

            case SESSION_INDEX_INFO:
                if (ActiveSesIndex == *((_FAR_ INT8U*)Session))
                {
                    return &pSessionTblInfo->SessionTbl[Index];
                }
                break;

            case SESSION_CHANNEL_INFO:
                if (pSessionTblInfo->SessionTbl[Index].Channel == *((_FAR_ INT8U*)Session))
                {
                    return &pSessionTblInfo->SessionTbl[Index];
                }
                break;

            default:
                return NULL;
        }
    }
    //	printf("%s:%d\n",__FUNCTION__,__LINE__);
    return NULL;
}

/*********************************************************************************************
Name	: AddChUser
Input	: ChannelUserInfo
Output	: Pointer to the free entry from the channel's user table.
This program returns the free entry from the channel's user table.
 *********************************************************************************************/
_FAR_ ChannelUserInfo_T* AddChUser (_FAR_ ChannelUserInfo_T* pChUserInfo, _NEAR_ INT8U*	Index, int BMCInst)
{
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    *Index =0;
    while (*Index <pBMCInfo->IpmiConfig.MaxChUsers)
    {
        if(FALSE == pChUserInfo->IPMIMessaging )
        {
            if(pChUserInfo->ID != USER_ID )
                return pChUserInfo;
        }
        (*Index)++;
        pChUserInfo++;
    }
    return NULL;
}

/*********************************************************************************************
Name	:	disableUser
Input	:	userId
OutPut	:	TRUE - if user can be disabled
FALSE - if user can not be disabled
This program disables the user, delete the user information from the user table.
 *********************************************************************************************/
INT8U disableUser (INT8U UserId, int BMCInst)
{
    INT8U				ChannelNo;
    INT8U				Index=0;
    _FAR_	ChannelInfo_T*		pChannelInfo=NULL;
    _FAR_	ChannelUserInfo_T*	pChUserInfo;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    LOCK_BMC_SHARED_MEM(BMCInst);
    for (ChannelNo = 0; ChannelNo < MAX_NUM_CHANNELS; ChannelNo++)
    {
        if(pBMCInfo->ChConfig[ChannelNo].ChType != 0xff)
        {
            pChannelInfo = (ChannelInfo_T *)&pBMCInfo->ChConfig[ChannelNo].ChannelInfo;
            continue;
        }

        pChUserInfo = getChUserIdInfo(UserId, &Index, pChannelInfo->ChannelUserInfo, BMCInst);

        if ( (pChUserInfo != NULL) && (pChannelInfo->NoCurrentUser==1))
        {
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            return FALSE;	/* cannot disable user since as defined by IPMI Specification
                               atleast  one user should be enabled for a channel. If this is the
                               only user  enabled in a given channel  we cannot disable the user.*/
        }
    }

    UNLOCK_BMC_SHARED_MEM(BMCInst);
    return TRUE;
}


/*---------------------------------------
 * GetSelTimeStamp
 *---------------------------------------*/
    INT32U
GetTimeStamp(void)
{
    return (htoipmi_u32 (GET_SYSTEM_TIME_STAMP()));
}

/*********************************************************************************************
Name	:	GetNumOfActiveSessions
Input	:	Nothing
Output	:	Number of active Sessions

This program returns the number of active session(s) from the session table
 *********************************************************************************************/
INT8U GetNumOfActiveSessions (int BMCInst)
{
    INT8U				Index, Count = 0;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    _FAR_	SessionTblInfo_T*	pSessionTblInfo = &pBMCInfo->SessionTblInfo;

    OS_THREAD_MUTEX_ACQUIRE(&g_BMCInfo[BMCInst].SessionTblMutex, WAIT_INFINITE);
    for (Index = 0; Index < pBMCInfo->IpmiConfig.MaxSession; Index++)
    {
        if (FALSE == pSessionTblInfo->SessionTbl[Index].Used)
        {
            continue;
        }
        if (pSessionTblInfo->SessionTbl[Index].Activated)
            Count++;
    }
    OS_THREAD_MUTEX_RELEASE(&g_BMCInfo[BMCInst].SessionTblMutex);

    return Count;
}

/*********************************************************************************************
Name	:	GetNumOfUsedSessions
Input	:	Nothing
Output	:	Number of used Sessions

This program returns the number of used session(s) from the session table
 *********************************************************************************************/
INT8U GetNumOfUsedSessions (int BMCInst)
{
    INT8U				Index, Count = 0;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    _FAR_	SessionTblInfo_T*	pSessionTblInfo = &pBMCInfo->SessionTblInfo;

    OS_THREAD_MUTEX_ACQUIRE(&g_BMCInfo[BMCInst].SessionTblMutex, WAIT_INFINITE);
    for (Index = 0; Index < pBMCInfo->IpmiConfig.MaxSession; Index++)
    {
        if (FALSE == pSessionTblInfo->SessionTbl[Index].Used)
        {
            continue;
        }
        Count++;
    }
    OS_THREAD_MUTEX_RELEASE(&g_BMCInfo[BMCInst].SessionTblMutex);

    return Count;
}


/*********************************************************************************************
Name	:	CleanSession
Input	:	Nothing
Output	:	None

This program delete the oldest session filled but not activate
 *********************************************************************************************/
INT8U  CleanSession(int BMCInst)
{
    INT8U			Index;
    OldSessionInfo_T     OldSession;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];	
    _FAR_	SessionTblInfo_T*	pSessionTblInfo = &pBMCInfo->SessionTblInfo;


    OldSession.Time=0xFFFFFFFF;
    OldSession.Index= 0xFF;


    OS_THREAD_MUTEX_ACQUIRE(&g_BMCInfo[BMCInst].SessionTblMutex, WAIT_INFINITE);

    for (Index = 0; Index < pBMCInfo->IpmiConfig.MaxSession; Index++)
    {
        if (TRUE == pSessionTblInfo->SessionTbl[Index].Activated)
        {
            continue;
        }

        if(pSessionTblInfo->SessionTbl[Index].Time <OldSession.Time )
        {
            OldSession.Time=pSessionTblInfo->SessionTbl[Index].Time;
            OldSession.Index=Index;
        }
    }
    OS_THREAD_MUTEX_RELEASE(&g_BMCInfo[BMCInst].SessionTblMutex);

    if(OldSession.Index !=0xFF)
    {
        pSessionTblInfo->SessionTbl[OldSession.Index].Used = FALSE;
        pSessionTblInfo->Count--;
        pSessionTblInfo->SessionTbl[OldSession.Index].Time=0xffffffff;
    }else
    {
        return FALSE;
    }

    return TRUE;
}


/*********************************************************************************************
Name	:	DeleteSession
Input	:	pSessionInfo - session information
Output	:	Nothing

This program deletes the session from the session table
 *********************************************************************************************/
void DeleteSession(_FAR_ SessionInfo_T*	pSessionInfo,int BMCInst)
{
    INT8U			Index;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];			 
    _FAR_	SessionTblInfo_T*	pSessionTblInfo = &pBMCInfo->SessionTblInfo;
    _FAR_	UserInfo_T*		 pUserInfo;
    _FAR_   ChannelInfo_T *      pChannelInfo;

    OS_THREAD_MUTEX_ACQUIRE(&g_BMCInfo[BMCInst].SessionTblMutex, WAIT_INFINITE);

    for (Index = 0; Index < pBMCInfo->IpmiConfig.MaxSession; Index++)
    {
        if (FALSE == pSessionTblInfo->SessionTbl[Index].Used)
        {
            continue;
        }

        if (0 == _fmemcmp (&pSessionTblInfo->SessionTbl[Index], pSessionInfo , sizeof(SessionInfo_T)))
        {
            /* We have decrement  the Active session only .If session is activated */
            if(TRUE ==pSessionTblInfo->SessionTbl[Index].Activated)
            {
                if(!pSessionTblInfo->SessionTbl[Index].IsLoopBack)
                {
                    pChannelInfo= getChannelInfo(pSessionInfo->Channel, BMCInst);
                    if(NULL == pChannelInfo)
                    {
                        TDBG("Failed to get channel info while Deleting Session for Channel: %d\n",pSessionInfo->Channel);
                        return;
                    }

                    if(pChannelInfo!=NULL)
                        pChannelInfo->ActiveSession--;
                    pUserInfo = getUserIdInfo (pSessionInfo->UserId, BMCInst);
                    if (pUserInfo != NULL) { pUserInfo->CurrentSession--; }
                }
                pSessionTblInfo->SessionTbl[Index].Activated=FALSE;
                pSessionTblInfo->SessionTbl[Index].EventFlag = 0;
            }

            pSessionTblInfo->SessionTbl[Index].Used = FALSE;
            pSessionTblInfo->Count--;
            pSessionTblInfo->SessionTbl[Index].Time=0xFFFFFFFF;

            IPMI_DBG_PRINT_3("DeleteSession: SessionID = %lX	Num Session %X\t%x\n",
                    pSessionInfo->SessionID, pSessionTblInfo->Count,Index);

            // Don't create a DEACTIVATE_SOL task again. This is already done in Deactivate Payload
            // while communicate via freeipmi, it will send the Deactivate packet to the newly created session, so removed.
#if 0	
            if(pBMCInfo->IpmiConfig.SOLIfcSupport == 1)
            {
                if(pSessionInfo->SessPyldInfo [PAYLOAD_SOL].Type == PAYLOAD_SOL)
                {
                    MsgPkt_T	MsgPkt;

                    MsgPkt.Param = DEACTIVATE_SOL;
                    MsgPkt.Size = 0;
                    if( 0 != PostMsg (&MsgPkt, SOL_IFC_Q,BMCInst))    
                    {
                        IPMI_WARNING ("AppDevice+.c : Error posting message to SOLIfc_Q\n");
                    }
                }
            }
#endif
            OS_THREAD_MUTEX_RELEASE(&pBMCInfo->SessionTblMutex);

            return;
        }
    }

    OS_THREAD_MUTEX_RELEASE(&g_BMCInfo[BMCInst].SessionTblMutex);

    return;
}

/*********************************************************************************************
Name	:	AddSession
Input	:	pSessionInfo - session information
Output	:	Nothing

This program adds the session to the session table
 *********************************************************************************************/
void AddSession (_NEAR_ SessionInfo_T* pSessionInfo, int BMCInst)
{
    INT8U Index;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];		
    _FAR_	SessionTblInfo_T*	pSessionTblInfo = &pBMCInfo->SessionTblInfo;


    OS_THREAD_MUTEX_ACQUIRE(&g_BMCInfo[BMCInst].SessionTblMutex, WAIT_INFINITE);
    for (Index = 0; Index < pBMCInfo->IpmiConfig.MaxSession; Index++)
    {
        if (FALSE == pSessionTblInfo->SessionTbl[Index].Used)
        {
            _fmemcpy (&pSessionTblInfo->SessionTbl[Index], (_FAR_ INT8U*)pSessionInfo, sizeof (SessionInfo_T));
            pSessionTblInfo->SessionTbl[Index].Used = TRUE;
            pSessionTblInfo->Count++;
            pSessionTblInfo->SessionTbl[Index].Time= GetTimeStamp ();
            pSessionTblInfo->SessionTbl[Index].TimeOutValue=pBMCInfo->IpmiConfig.SessionTimeOut;
            IPMI_DBG_PRINT_3 ("AddSession: SessionID   = %lX  Num Session %X\t%x\n",pSessionInfo->SessionID, pSessionTblInfo->Count,Index);
            break;
        }
    }

    OS_THREAD_MUTEX_RELEASE(&g_BMCInfo[BMCInst].SessionTblMutex);

}

/*********************************************************************************************
Name	:	getPayloadActiveInst
Input	:	PayloadType
Output	:	Activated Instance information of a given payload type.

This program returns the information about the activated instances of a given payload type.
 *********************************************************************************************/
_FAR_ INT16U getPayloadActiveInst (INT8U PayloadType,int BMCInst)
{
    INT8U   PayloadIx;
    INT8U   Index;
    INT16U	ActivatedInst = 0;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    _FAR_	SessionTblInfo_T*	pSessionTblInfo = &pBMCInfo->SessionTblInfo;


    OS_THREAD_MUTEX_ACQUIRE(&g_BMCInfo[BMCInst].SessionTblMutex, WAIT_INFINITE);
    for (Index = 0; Index < pBMCInfo->IpmiConfig.MaxSession; Index++)
    {
        if (FALSE == pSessionTblInfo->SessionTbl[Index].Used)
        {
            continue;
        }
        if (FALSE == pSessionTblInfo->SessionTbl[Index].Activated)
        {
            continue;
        }

        for (PayloadIx = 0; PayloadIx < MAX_PYLDS_SUPPORT; PayloadIx++)
        {
            if (pSessionTblInfo->SessionTbl[Index].SessPyldInfo [PayloadIx].Type == PayloadType)
            {
                ActivatedInst |= pSessionTblInfo->SessionTbl[Index].SessPyldInfo [PayloadIx].ActivatedInst;
            }
        }
    }
    OS_THREAD_MUTEX_RELEASE(&g_BMCInfo[BMCInst].SessionTblMutex);

    return ActivatedInst;
}

/*********************************************************************************************
Name	:	getPayloadInstInfo
Input	:	Payloadtype, PayloadInst
Output	:	sessionID

This program  returns the session ID of the session that was activated under the given payload 
type and payload instance.
 *********************************************************************************************/
_FAR_ INT32U getPayloadInstInfo (INT8U PayloadType, INT16U PayloadInst, int BMCInst)
{
    INT8U   PayloadIx;
    INT8U   Index;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    _FAR_	SessionTblInfo_T*	pSessionTblInfo = &pBMCInfo->SessionTblInfo;


    OS_THREAD_MUTEX_ACQUIRE(&g_BMCInfo[BMCInst].SessionTblMutex, WAIT_INFINITE);
    for (Index = 0; Index < pBMCInfo->IpmiConfig.MaxSession; Index++)
    {
        if (FALSE == pSessionTblInfo->SessionTbl[Index].Used)
        {
            continue;
        }
        if (FALSE == pSessionTblInfo->SessionTbl[Index].Activated)
        {
            continue;
        }

        for (PayloadIx = 0; PayloadIx < MAX_PYLDS_SUPPORT; PayloadIx++)
        {
            if ((pSessionTblInfo->SessionTbl[Index].SessPyldInfo [PayloadIx].Type == PayloadType)
                    && (pSessionTblInfo->SessionTbl[Index].SessPyldInfo [PayloadIx].ActivatedInst & (1 << (PayloadInst - 1))))
            {
                OS_THREAD_MUTEX_RELEASE(&g_BMCInfo[BMCInst].SessionTblMutex);
                return pSessionTblInfo->SessionTbl[Index].SessionID;
            }
        }
    }

    OS_THREAD_MUTEX_RELEASE(&g_BMCInfo[BMCInst].SessionTblMutex);

    return 0;
}


static
MsgPkt_T	m_MsgPkt = {
    PARAM_IFC,
    0,                  /* Channel number not needed    */
    {0},                /* Source queue not needed      */
    CMD_PLATFORM_EVENT, /* Cmd                          */
    (NETFN_SENSOR << 2),/* Net Function                 */
    PRIV_LOCAL,         /* Privilage                    */
    0,                  /* Session ID not needed        */
    0,
    WAIT_INFINITE,
    0,
    {0},                /* IP Addr not needed           */
    0,                  /* UDPPort not needed           */
    0,                  /* Socket  not needed           */
    sizeof(SELEventRecord_T) + sizeof (IPMIMsgHdr_T) + 1,
    {
        0x20,               /* Generator ID             */
        IPMI_EVM_REVISION,  /* IPMI Version             */
        SENSOR_TYPE_SECUIRTY_VIOLATION,/*SensorType     */
        SECUIRTY_VIOLATION_SENSOR_NUMBER,
        SENSOR_SPECIFIC_READ_TYPE,
        PW_VIOLATION_OFFSET,
        0xff,
        0xff
    },
};

/*--------------------------------------------------------------------------*
 * PasswordViolation														*
 *--------------------------------------------------------------------------*/
    void
PasswordViolation (int BMCInst)
{
    /* Log the AC fail event to SEL	& send an alert */
    /* Post to Message Hndlr Queue	*/
    PostMsg (&m_MsgPkt, MSG_HNDLR_Q,BMCInst);

    return;
}



/*********************************************************************************************
Name	:	UDSSessionTimeOutTask
Input	:	void
Output	:	void
This program  checks for UDS session timeout
 *********************************************************************************************/
void UDSSessionTimerTask (int BMCInst)
{
    INT8U Index=0;
    BMCInfo_t *pBMCInfo = &g_BMCInfo[BMCInst];
    UDSSessionTblInfo_T *pUDSSessionTblInfo = &pBMCInfo->UDSSessionTblInfo;

    for(Index=0;Index<pBMCInfo->IpmiConfig.MaxSession;Index++)
    {
        if(FALSE == pUDSSessionTblInfo->UDSSessionTbl[Index].Activated)
        {
            /* Continue until we find the Next Slot which is being used to reduce the timeout */
            continue;
        }

        if((pUDSSessionTblInfo->UDSSessionTbl[Index].SessionTimeoutValue <= pBMCInfo->IpmiConfig.SessionTimeOut) && (pUDSSessionTblInfo->UDSSessionTbl[Index].SessionTimeoutValue != 0))
        {
            /* Reduce the Session Timeout Value if the session is not used */
            pUDSSessionTblInfo->UDSSessionTbl[Index].SessionTimeoutValue--;
        }
        else if(pUDSSessionTblInfo->UDSSessionTbl[Index].SessionTimeoutValue <= 0)
        {
            DeleteUDSSession(&pUDSSessionTblInfo->UDSSessionTbl[Index],BMCInst);
        }
    }
}



/*********************************************************************************************
Name	:	GetUDSSessionInfo
Input	:	Session - session ID
Output	:	Session Information

This program  returns the session information.
 *********************************************************************************************/
_FAR_ UDSSessionTbl_T* GetUDSSessionInfo (INT8U Type,void* Data,int BMCInst)
{
    INT8U Index;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    _FAR_ UDSSessionTblInfo_T*	pUDSSessionTblInfo = &pBMCInfo->UDSSessionTblInfo;


    for(Index=0;Index<pBMCInfo->IpmiConfig.MaxSession;Index++)
    {

        if(FALSE == pUDSSessionTblInfo->UDSSessionTbl[Index].Activated)
        {
            continue;
        }

        switch(Type)
        {
            case UDS_SESSION_ID_INFO:
                if(*((INT32U *)Data) == pUDSSessionTblInfo->UDSSessionTbl[Index].SessionID)
                {
                    return &pUDSSessionTblInfo->UDSSessionTbl[Index];
                }
                break;
            case UDS_SESSION_HANDLE_INFO:
                if(*((INT8U *)Data) == pUDSSessionTblInfo->UDSSessionTbl[Index].LoggedInSessionHandle)
                {
                    return &pUDSSessionTblInfo->UDSSessionTbl[Index];
                }
                break;
            case UDS_SESSION_INDEX_INFO:
                if((*((INT8U *)Data) <= pBMCInfo->IpmiConfig.MaxSession) && (*((INT8U *)Data ) > 0) && ((Index+1) == *((INT8U *)Data )))
                {
                    return &pUDSSessionTblInfo->UDSSessionTbl[Index];
                }
                break;
            case UDS_SOCKET_ID_INFO:
                if(*((int *)Data) == pUDSSessionTblInfo->UDSSessionTbl[Index].UDSSocket)
                {
                    return &pUDSSessionTblInfo->UDSSessionTbl[Index];
                }
                break;

            default:
                break;
        }
    }

    return NULL;
}

/*********************************************************************************************
Name	:	AddUDSSession
Input	:	pUDSSessionInfo - session information
Output	:	return 0 on success ,-1 on failure

This program adds the session to the UDS session table
 *********************************************************************************************/
int AddUDSSession (_NEAR_ UDSSessionTbl_T* pUDSSessionInfo, int BMCInst)
{

    INT8U Index;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    _FAR_ UDSSessionTblInfo_T*	pUDSSessionTblInfo = &pBMCInfo->UDSSessionTblInfo;

    /* Acquire the UDS Session Mutex Lock */
    OS_THREAD_MUTEX_ACQUIRE(&g_BMCInfo[BMCInst].UDSSessionTblMutex, WAIT_INFINITE);

    for (Index = 0; Index < pBMCInfo->IpmiConfig.MaxSession; Index++)
    {
        if(TRUE == pUDSSessionTblInfo->UDSSessionTbl[Index].Activated)
        {
            /* Continue Inorder to get the next Free Slot in UDS Session Table */
            continue;
        }

        /* Copy the Session Information to Global BMC Info UDS Session Table */
        _fmemcpy (&pUDSSessionTblInfo->UDSSessionTbl[Index], (_FAR_ INT8U*)pUDSSessionInfo, sizeof (UDSSessionTbl_T));
        pUDSSessionTblInfo->SessionCount++;
        pUDSSessionTblInfo->UDSSessionTbl[Index].LoggedInTime = GetTimeStamp ();
        pUDSSessionTblInfo->UDSSessionTbl[Index].SessionTimeoutValue = pBMCInfo->IpmiConfig.SessionTimeOut;
        break;
    }

    /* Release the UDS Session Mutex Lock */
    OS_THREAD_MUTEX_RELEASE(&g_BMCInfo[BMCInst].UDSSessionTblMutex);

    if(Index == pBMCInfo->IpmiConfig.MaxSession)
    {
        IPMI_WARNING("Add Session Failed for UDS\n");
        return -1;
    }

    return 0;
}

/*********************************************************************************************
Name	:	DeleteUDSSession
Input	:	pUDSSessionInfo - session information
Output	:	return 0 on success,-1 on failure

This program deletes the session from the UDS session table
 *********************************************************************************************/
int DeleteUDSSession(_FAR_ UDSSessionTbl_T *pUDSSessionInfo,int BMCInst)
{
    INT8U Index;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    _FAR_ UDSSessionTblInfo_T*	pUDSSessionTblInfo = &pBMCInfo->UDSSessionTblInfo;

    /* Acquire the UDS Session Mutex Lock */
    OS_THREAD_MUTEX_ACQUIRE(&g_BMCInfo[BMCInst].UDSSessionTblMutex, WAIT_INFINITE);

    for(Index=0;Index<pBMCInfo->IpmiConfig.MaxSession;Index++)
    {
        if(FALSE == pUDSSessionTblInfo->UDSSessionTbl[Index].Activated)
        {
            /* Continue to get the Next Occupied Session Slot in UDS Session Slot*/
            continue;
        }

        if (0 == _fmemcmp (&pUDSSessionTblInfo->UDSSessionTbl[Index], pUDSSessionInfo , sizeof(UDSSessionTbl_T)))
        {
            /* Resetting the UDS Session Table Slot as the session is no longer required */
            pUDSSessionTblInfo->UDSSessionTbl[Index].Activated = FALSE;
            pUDSSessionTblInfo->UDSSessionTbl[Index].LoggedInTime = 0xFFFFFFFF;
            pUDSSessionTblInfo->UDSSessionTbl[Index].SessionTimeoutValue = 0;
            pUDSSessionTblInfo->UDSSessionTbl[Index].LoggedInUserID =  0;
            pUDSSessionTblInfo->UDSSessionTbl[Index].LoggedInChannel = 0xFF;
            pUDSSessionTblInfo->UDSSessionTbl[Index].LoggedInPrivilege = 0xFF;
            pUDSSessionTblInfo->SessionCount--;
            break;
        }
    }

    /* Release the UDS Session Mutex Lock */
    OS_THREAD_MUTEX_RELEASE(&g_BMCInfo[BMCInst].UDSSessionTblMutex);

    if(Index == pBMCInfo->IpmiConfig.MaxSession)
    {
        IPMI_WARNING("Delete Session Failed for UDS\n");
        return -1;
    }

    return 0;
}

/*---------------------------------------------------
 * @fn UpdateGetMsgTime
 * @brief Updates the Current Uptime and timeout value
 *        for an IPMI Message
 * 
 * @param pReq       : IPMI Message Packet
 * @param ResTimeOut : Timeout Macro string
 * @param BMCInst    : BMC Instance
 * 
 * @return none
 *----------------------------------------------------*/
void UpdateGetMsgTime (MsgPkt_T* pReq,IfcType_T IfcType, int BMCInst)
{
    pReq->ReqTime    = 0;
    pReq->ResTimeOut = WAIT_INFINITE;

    if( g_corefeatures.ipmi_res_timeout == ENABLED )
    {
        int i;
        struct sysinfo sys_info;

        for( i = 0; i < TOTAL_INFINITE_CMDS; i++)
        {
            if( NET_FN(pReq->NetFnLUN) == m_InfiniteCmdsTbl[i].NetFn && pReq->Cmd == m_InfiniteCmdsTbl[i].Cmd )
            {
                return;
            }
        }
#if 0        
        if(g_PDKCmdsHandle[PDKCMDS_PDKISINFINITECOMMAND]  != NULL)
        {
            if( ((BOOL(*)(INT8U,INT8U))g_PDKCmdsHandle[PDKCMDS_PDKISINFINITECOMMAND])(NET_FN(pReq->NetFnLUN), pReq->Cmd))
            {
                return;
            }
        }
#endif        
        /* Request UpTime */
        if(!sysinfo(&sys_info))
        {
            pReq->ReqTime    = sys_info.uptime;
            if(IfcType == IPMB_IFC)
            {
                pReq->ResTimeOut = g_coremacros.ipmb_res_timeout;
            }
            else if(IfcType == LAN_IFC)
            {
                pReq->ResTimeOut = g_coremacros.lan_res_timeout;
            }
            else if(IfcType == KCS_IFC)
            {
                pReq->ResTimeOut = g_coremacros.kcs_res_timeout;
            }
            else if(IfcType == SERIAL_IFC)
            {
                pReq->ResTimeOut = g_coremacros.serial_res_timeout;
            }
        }
    }
}

/*---------------------------------------------------
 * @fn IsMsgTimedOut
 * @brief Checks if the time taken to process 
 *        the IPMI Command expires
 * 
 * @param pReq     : IPMI Message Packet
 * 
 * @return 1 if timed out
 *         0 if error or timeout not set 
 *----------------------------------------------------*/
BOOL IsMsgTimedOut (MsgPkt_T* pReq)
{
    struct sysinfo sys_info;

    if( pReq->ReqTime && (!sysinfo(&sys_info)) )
    {
        return ( (sys_info.uptime - pReq->ReqTime) > pReq->ResTimeOut) ? TRUE : FALSE;
    }

    return FALSE;
}

/*-----------------------------------------------------
 * @fn IsResponseMatch
 * @brief Checks if the Response Message corresponds to 
 *        the Request Message
 * 
 * @param pReq    : IPMI Request Message Packet
 * @param pRes    : IPMI Response Message Packet
 * 
 * @return  1 if match
 *          0 if mismatch
 *----------------------------------------------------*/
BOOL IsResponseMatch (MsgPkt_T* pReq, MsgPkt_T* pRes)
{

    if( ( ((((IPMIMsgHdr_T*)pRes->Data)->RqSeqLUN) >> 2) != ((((IPMIMsgHdr_T*)pReq->Data)->RqSeqLUN) >> 2) ) && 
            ( (pReq->NetFnLUN & 0x4) && (NO_RESPONSE != pRes->Param) ) )
    {
        return FALSE;
    }

    return TRUE;
}

/*-------------------------------------------------------
 * @fn FillIPMIResFailure
 * @brief Frames the Response packet when the time taken 
 *        to process an IPMI Command expires
 * 
 * @param pReq    : IPMI Request Message Packet
 * @param pRes    : IPMI Response Message Packet
 *        BMCInst : BMC Instance Number
 * 
 * @return  none
 *------------------------------------------------------*/
void FillIPMIResFailure (_NEAR_ MsgPkt_T* pReq, _NEAR_ MsgPkt_T* pRes, int BMCInst)
{
    int  i;
    BOOL IsBridgePending = FALSE;

    _NEAR_ IPMIMsgHdr_T*  pIPMIMsgReq = (_NEAR_ IPMIMsgHdr_T*)pReq->Data;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    if ( pReq->Channel == CH_NOT_USED || pReq->Channel == USB_CHANNEL || (strcmp((char *)pReq->SrcQ,UDS_RES_Q) == 0) ||
            pReq->Channel == pBMCInfo->ICMBCh || pReq->Channel == pBMCInfo->SMBUSCh )
    {
        return;
    }

    /* Set the Cmd and Net function in response packet */
    pRes->Cmd      = pReq->Cmd;
    pRes->NetFnLUN = pReq->NetFnLUN & 0xFC;

    /* Normal IPMI Command response */
    pRes->Param = NORMAL_RESPONSE;

    IPMI_DBG_PRINT ("Filling IPMI Packet for Response Timeout\n");

    if (SYS_IFC_CHANNEL == pReq->Channel)
    {
        pRes->Size    = sizeof (INT8U);
        pRes->Data[0] = g_coremacros.res_timeout_compcode;
    }
    else
    {
        pRes->Size  = sizeof (IPMIMsgHdr_T) + sizeof (INT8U);
        pRes->Data[sizeof (IPMIMsgHdr_T)] = g_coremacros.res_timeout_compcode;

        /* Check for Request Message */
        if (0 == (pReq->NetFnLUN & 0x04))
        {
            /* Swap the header and copy in response */
            SwapIPMIMsgHdr ((_NEAR_ IPMIMsgHdr_T*)pReq->Data, (_NEAR_ IPMIMsgHdr_T*)pRes->Data);

            /* Calculate Checksum 2 */
            pRes->Data[pRes->Size] = CalculateCheckSum2 (pRes->Data, pRes->Size);
            pRes->Size++;
        }
    }

    /* Check the sequence number in table */
    for (i=0; i < sizeof (m_PendingBridgedResTbl)/sizeof (m_PendingBridgedResTbl[0]); i++)
    {
        IPMI_DBG_PRINT_1( "MsgHndlr: Checking outside message with sequence number %d.\n", i );

        if (m_PendingBridgedResTbl[i].Used)
        {
            IsBridgePending = TRUE;

            if  ( (pReq->Channel == m_PendingBridgedResTbl[i].ChannelNum) &&
                    (NET_FN(pIPMIMsgReq->RqSeqLUN)  == m_PendingBridgedResTbl[i].SeqNum) &&
                    (NET_FN(pIPMIMsgReq->NetFnLUN)  == NET_FN((m_PendingBridgedResTbl[i].ReqMsgHdr.NetFnLUN + 0x04))) &&
                    (pIPMIMsgReq->Cmd               == m_PendingBridgedResTbl[i].ReqMsgHdr.Cmd) &&
                    (pIPMIMsgReq->ReqAddr           == m_PendingBridgedResTbl[i].ReqMsgHdr.ResAddr) )
            {
                m_PendingBridgedResTbl[i].Used = FALSE;
                IPMI_DBG_PRINT_1( "FillIPMIResFailure: Cleared pending index = %d.\n", i );

                IsBridgePending = FALSE;
                break;
            }
        }
    }
#if 0    
    /* PDK Hook to Clear Pending Bridge Table Entry for this message */
    if(IsBridgePending && g_PDKHandle[PDK_CLEARPENDBRIDGEENTRY] != NULL)
    {
        ((void (*)(MsgPkt_T *) ) g_PDKHandle[PDK_CLEARPENDBRIDGEENTRY]) (pReq);
    }
#endif    
}


