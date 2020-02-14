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
 * RMCP.c
 * RMCP Message Handler
 *
 * Author: Govind Kothandapani <govindk@ami.com>
 *       : Bakka Ravinder Reddy <bakkar@ami.com>
 *
 *****************************************************************/
#include "MsgHndlr.h"
#include "Support.h"
#include "Debug.h"
#include "OSPort.h"
#include "Message.h"
#include "IPMIDefs.h"
#include "PMConfig.h"
#include "SharedMem.h"
#include "Session.h"
#include "LANIfc.h"
//#include "MD.h"
#include "Util.h"
#include "RMCP.h"
//#include "RMCP+.h"
//#include "AES.h"
#include "App.h"
#include "IPMI_Main.h"
//#include "hmac_md5.h"
//#include "hmac_sha1.h"
//#include "MD5_128.h"
#include "Ethaddr.h"
#include "IPMIConf.h"
#include "IPMI_SensorEvent.h"
//#include "Badpasswd.h"
//#include "PendTask.h"
//#include "SEL.h"
#include "featuredef.h"
//#include "blowfish.h"

/*** Local definitions ***/
#define RMCP_VERSION                6
#define IPMI_MESSAGE_CLASS          7
#define PRESENCE_PING_MSGTYPE       0x80
#define RMCP_VERSION                6

#define AMI_CMD_NETFN_LUN           (((0x2E | 1) << 2) | 0x00)
#define AMI_CMD_12                  0x12
#define PING_IPMI_15_SUPPORT        1
#define PING_IPMI_20_SUPPORT        2
#define MAX_AUTH_CODE_SIZE          12
#define INIT_VECTOR_SIZE            16
#define INTEGRITY_MASK              BIT6
#define CONFIDENT_MASK              BIT7

#define PAYLOAD_RSSP_OS_REQ         0x10
#define PAYLOAD_RSSP_OS_RES         0x11
#define PAYLOAD_RAKP_MSG1           0x12
#define PAYLOAD_RAKP_MSG2           0x13
#define PAYLOAD_RAKP_MSG3           0x14
#define PAYLOAD_RAKP_MSG4           0x15

#define IPMI_EVENT_TYPE_BASE        0x09

IfcType_T IfcType = LAN_IFC;

/*** Prototype Declaration ***/
static int   ProcIPMIReq        (_FAR_ SessionInfo_T*  pSessionInfo, INT8U Payload, MiscParams_T *pParams,INT8U Channel,int BMCInst);
static BOOL  ValidateRMCPHdr    (_NEAR_ RMCPHdr_T* pRMCPHdr);
static BOOL  ValidateSessionHdr (INT32U SessionID, INT32U SeqNo, int BMCInst);
static INT8U ProcessPingMsg     (_NEAR_ RMCPHdr_T* pRMCPReq,
        _NEAR_ RMCPHdr_T* pRMCPRes,int BMCInst);
//static BOOL  ValidateAuthCode   (_NEAR_ INT8U* pAuthCode, _FAR_ INT8U* pPassword,
//                                 _NEAR_ SessionHdr_T* pSessionHdr,
//                                 _NEAR_ IPMIMsgHdr_T* pIPMIMsg);
static int   Proc20Payload      (_NEAR_ RMCPHdr_T* pRMCPReq,
        _NEAR_ RMCPHdr_T* pRMCPRes, MiscParams_T *pParams, INT8U Channel, int BMCInst);


/*** Local typedefs ***/
/**
 * @struct PreSessionCmd_T
 * @brief Pre-session command entry.
 **/
typedef struct
{
    INT8U   NetFn;
    INT8U   Cmd;
} PreSessionCmd_T;

/**
 * @brief Message Payload Handler function.
 * @param pReq   - Request message.
 * @param ReqLen - Request length.
 * @param pRes   - Response message.
 * @return 0 if success, -1 if error.
 **/
typedef int (*pPayloadHndlr_T) (_NEAR_ INT8U* pReq, INT8U ReqLen,
        _NEAR_ INT8U* pRes, MiscParams_T *pParams,INT8U Channel, int BMCInst);

/**
 * @struct PayloadTbl_T;
 * @brief Payload Table structure.
 **/
typedef struct
{
    INT8U           Payload;
    pPayloadHndlr_T PayloadHndlr;
} PayloadTbl_T;


static const PayloadTbl_T m_PayloadTbl [] =
{
    /*  Payload              Handler           */
    {PAYLOAD_RSSP_OS_REQ,   NULL/*RSSPOpenSessionReq*/  },
    {PAYLOAD_RAKP_MSG1,     NULL/*RAKPMsg1*/            },
    {PAYLOAD_RAKP_MSG3,     NULL/*RAKPMsg3*/            },
};

/* Pre-Session establishment commands */
static const PreSessionCmd_T m_PreSessionCmdsTbl[] =
{
    { NETFN_APP,    CMD_GET_CH_AUTH_CAP },
    { NETFN_APP,    CMD_GET_SESSION_CHALLENGE },
    { NETFN_APP,    CMD_GET_DEV_GUID },
    { NETFN_APP,    CMD_GET_CH_CIPHER_SUITES },
    { NETFN_APP,    CMD_GET_SYSTEM_GUID },
    { NETFN_SENSOR, CMD_PET_ACKNOWLEDGE},
};

int RmcpSeqNumValidation(SessionInfo_T* pSessionInfo, INT32U SessionSeqNum, IPMIMsgHdr_T* pIPMIMsgReq)
{
    INT32U  SeqTrac[SIXTEEN_COUNT_WINDOW_LEN];
    BOOL    TrackRollOver       = FALSE;
    INT32U  TrackRollOverSeq    = SEQNUM_ROLLOVER;
    int     i;

    if((pIPMIMsgReq->Cmd != CMD_ACTIVATE_SESSION) ||
            ((pIPMIMsgReq->Cmd == CMD_ACTIVATE_SESSION) &&
             (SessionSeqNum != 0)))
    {
        if( pSessionInfo->InboundSeq == SessionSeqNum)
            return -1;

        if((pSessionInfo->InboundSeq < (SEQNUM_ROLLOVER - EIGHT_COUNT_WINDOW_LEN)) && (pSessionInfo->InboundSeq > EIGHT_COUNT_WINDOW_LEN))
        {
            if(SessionSeqNum < pSessionInfo->InboundSeq)
            {
                if((pSessionInfo->InboundSeq -SessionSeqNum) > EIGHT_COUNT_WINDOW_LEN)
                {
                    return -1;
                }
                else
                {
                    for(i=0;i<EIGHT_COUNT_WINDOW_LEN;i++)
                    {
                        if(SessionSeqNum == pSessionInfo->InboundTrac[i])
                        {
                            if(((1 << i) & pSessionInfo->InboundRecv) != 0)
                            {
                                return -1;
                            }
                            else
                            {
                                pSessionInfo->InboundRecv |= (1<<i);
                            }
                        }
                    }
                }
            }
            else
            {
                if((SessionSeqNum - pSessionInfo->InboundSeq) > EIGHT_COUNT_WINDOW_LEN)
                {
                    return -1;
                }

                _fmemcpy((INT8U *)SeqTrac,(INT8U *)pSessionInfo->InboundTrac,(sizeof(INT32U) * EIGHT_COUNT_WINDOW_LEN));

                for(i=0; i < (SessionSeqNum - pSessionInfo->InboundSeq); i++)
                {
                    pSessionInfo->InboundTrac[i] = SessionSeqNum - (i+1);
                }

                pSessionInfo->InboundRecv = pSessionInfo->InboundRecv << (SessionSeqNum - pSessionInfo->InboundSeq);
                pSessionInfo->InboundRecv |= (1 << ((SessionSeqNum - pSessionInfo->InboundSeq)-1));

                _fmemcpy((INT8U *)&pSessionInfo->InboundTrac[SessionSeqNum - pSessionInfo->InboundSeq],
                        (INT8U *)&SeqTrac[0],
                        (sizeof(INT32U) *(EIGHT_COUNT_WINDOW_LEN - (SessionSeqNum - pSessionInfo->InboundSeq))));
                pSessionInfo->InboundSeq = SessionSeqNum;
            }
        }
        else if((pSessionInfo->InboundSeq  < EIGHT_COUNT_WINDOW_LEN)
                || (pSessionInfo->InboundSeq  > (SEQNUM_ROLLOVER -EIGHT_COUNT_WINDOW_LEN))) /* Checking for Roll over condition */
        {
            if(SessionSeqNum < pSessionInfo->InboundSeq)
            {
                if(!((((pSessionInfo->InboundSeq -SessionSeqNum) <= EIGHT_COUNT_WINDOW_LEN) &&
                                (((SEQNUM_ROLLOVER - pSessionInfo->InboundSeq) + SessionSeqNum+1) >= EIGHT_COUNT_WINDOW_LEN )) ||
                            ((((SEQNUM_ROLLOVER - pSessionInfo->InboundSeq) + SessionSeqNum+1) <= EIGHT_COUNT_WINDOW_LEN ) &&
                             (pSessionInfo->InboundSeq -SessionSeqNum) >= EIGHT_COUNT_WINDOW_LEN)))
                {
                    return -1;
                }
                else
                {
                    if((pSessionInfo->InboundSeq -SessionSeqNum) <= EIGHT_COUNT_WINDOW_LEN)
                    {
                        for(i=0;i<EIGHT_COUNT_WINDOW_LEN;i++)
                        {
                            if(SessionSeqNum == pSessionInfo->InboundTrac[i])
                            {
                                if(((1 << i) & pSessionInfo->InboundRecv) != 0)
                                {
                                    return -1;
                                }
                                else
                                {
                                    pSessionInfo->InboundRecv |= (1<<i);
                                }
                            }
                        }
                    }
                    else if(((SEQNUM_ROLLOVER - pSessionInfo->InboundSeq) + SessionSeqNum+1) <= EIGHT_COUNT_WINDOW_LEN )
                    {
                        _fmemcpy((INT8U *)SeqTrac,(INT8U *)pSessionInfo->InboundTrac,(sizeof(INT32U) * EIGHT_COUNT_WINDOW_LEN));

                        for(i=0; i < ((SEQNUM_ROLLOVER - pSessionInfo->InboundSeq) + SessionSeqNum+1); i++)
                        {
                            if(((SessionSeqNum - (i+1)) != 0) && (TrackRollOver == FALSE))
                                pSessionInfo->InboundTrac[i] = SessionSeqNum - (i+1);
                            else if(((SessionSeqNum - (i+1)) == 0) && (TrackRollOver == FALSE))
                            {
                                pSessionInfo->InboundTrac[i] = SessionSeqNum - (i+1);
                                TrackRollOver = TRUE;
                            }
                            else if(TrackRollOver ==  TRUE)
                            {
                                pSessionInfo->InboundTrac[i] = TrackRollOverSeq;
                                TrackRollOverSeq--;
                            }
                        }
                        TrackRollOverSeq = SEQNUM_ROLLOVER;
                        TrackRollOver = FALSE;

                        pSessionInfo->InboundRecv = pSessionInfo->InboundRecv << ((SEQNUM_ROLLOVER - pSessionInfo->InboundSeq) + SessionSeqNum+1);
                        pSessionInfo->InboundRecv |= (1 << (((SEQNUM_ROLLOVER - pSessionInfo->InboundSeq) + SessionSeqNum+1) -1));

                        _fmemcpy((INT8U *)&pSessionInfo->InboundTrac[(SEQNUM_ROLLOVER - pSessionInfo->InboundSeq) + SessionSeqNum+1],
                                (INT8U *)&SeqTrac[0],
                                (sizeof(INT32U) *(EIGHT_COUNT_WINDOW_LEN - ((SEQNUM_ROLLOVER - pSessionInfo->InboundSeq) + SessionSeqNum+1))));
                        pSessionInfo->InboundSeq = SessionSeqNum;
                    }
                }
            }
            else if(SessionSeqNum > pSessionInfo->InboundSeq)
            {
                if(!((((SessionSeqNum -pSessionInfo->InboundSeq) <= EIGHT_COUNT_WINDOW_LEN) &&
                                (((SEQNUM_ROLLOVER - SessionSeqNum) + pSessionInfo->InboundSeq+1) >= EIGHT_COUNT_WINDOW_LEN )) ||
                            ((((SEQNUM_ROLLOVER - SessionSeqNum) + pSessionInfo->InboundSeq+1) <= EIGHT_COUNT_WINDOW_LEN ) &&
                             (SessionSeqNum-pSessionInfo->InboundSeq) >= EIGHT_COUNT_WINDOW_LEN)))
                {
                    return -1;
                }
                else
                {
                    if((SessionSeqNum  - pSessionInfo->InboundSeq) <= EIGHT_COUNT_WINDOW_LEN)
                    {
                        _fmemcpy((INT8U *)SeqTrac,(INT8U *)pSessionInfo->InboundTrac,(sizeof(INT32U) * EIGHT_COUNT_WINDOW_LEN));
                        for(i=0;i<(SessionSeqNum  - pSessionInfo->InboundSeq) ;i++)
                        {
                            if(((SessionSeqNum - (i+1)) != 0) && (TrackRollOver == FALSE))
                            {
                                pSessionInfo->InboundTrac[i] = SessionSeqNum - (i+1);
                            }
                            else if(((SessionSeqNum - (i+1)) == 0) && (TrackRollOver == FALSE))
                            {
                                pSessionInfo->InboundTrac[i] = SessionSeqNum - (i+1);
                                TrackRollOver = TRUE;
                            }
                            else if(TrackRollOver ==  TRUE)
                            {
                                pSessionInfo->InboundTrac[i] = TrackRollOverSeq;
                                TrackRollOverSeq--;
                            }
                        }
                        TrackRollOverSeq = SEQNUM_ROLLOVER;
                        TrackRollOver = FALSE;

                        pSessionInfo->InboundRecv = pSessionInfo->InboundRecv << (SessionSeqNum  - pSessionInfo->InboundSeq);
                        pSessionInfo->InboundRecv |= (1 << ((SessionSeqNum  - pSessionInfo->InboundSeq) -1));

                        _fmemcpy((INT8U *)&pSessionInfo->InboundTrac[SessionSeqNum  - pSessionInfo->InboundSeq],
                                (INT8U *)&SeqTrac[0],
                                (sizeof(INT32U) *(EIGHT_COUNT_WINDOW_LEN - (SessionSeqNum - pSessionInfo->InboundSeq))));
                        pSessionInfo->InboundSeq = SessionSeqNum;
                    }
                    else if(((SEQNUM_ROLLOVER -SessionSeqNum) + pSessionInfo->InboundSeq+1) <= EIGHT_COUNT_WINDOW_LEN)
                    {
                        for(i=0;i<EIGHT_COUNT_WINDOW_LEN;i++)
                        {
                            if(SessionSeqNum == pSessionInfo->InboundTrac[i])
                            {
                                if(((1 << i) & pSessionInfo->InboundRecv) != 0)
                                {
                                    return -1;
                                }
                                else
                                {
                                    pSessionInfo->InboundRecv |= (1<<i);
                                }
                            }
                        }

                    }
                }
            }
        }
    }

    return 0;
}

/*-------------------------------------------
 * ProcessRMCPReq
 *-------------------------------------------*/
    INT32U
ProcessRMCPReq(_NEAR_ RMCPHdr_T* pRMCPReq, _NEAR_ RMCPHdr_T* pRMCPRes, MiscParams_T *pParams,INT8U Channel, int BMCInst)
{
    _FAR_   SessionInfo_T*  pSessionInfo;
    _NEAR_  IPMIMsgHdr_T*   pIPMIMsgReq;
    _NEAR_  IPMIMsgHdr_T*   pIPMIMsgRes;
    _NEAR_  INT8U*          pReqMsgAuthCode;
    _NEAR_  INT8U*          pResMsgAuthCode;
    _NEAR_  SessionHdr_T*   pReqSessionHdr = (_NEAR_ SessionHdr_T*)(pRMCPReq + 1);
    _NEAR_  SessionHdr_T*   pResSessionHdr = (_NEAR_ SessionHdr_T*)(pRMCPRes + 1);
    INT8U           IPMIMsgLen,AuthType;
    INT32U          SessionID;
    INT32U          SessionSeqNum;
    INT32U          ResLen, IPMIMsgResLen;
    _FAR_ BMCInfo_t*        pBMCInfo = &g_BMCInfo[BMCInst];

    /* Validate RMCP Header */
    if (TRUE != ValidateRMCPHdr(pRMCPReq))
    {
        IPMI_WARNING ("RMCP.c : RMCP header validation failed\n");
        return 0;
    }

    /* If RMCP Ping, process it seperately */
    if (pRMCPReq->MsgClass == 0x06)
    {
        return ProcessPingMsg (pRMCPReq, pRMCPRes,BMCInst);
    }

    /* Process IPMI 2.0 Separately */
#if IPMI20_SUPPORT == 1
    if (RMCP_PLUS_FORMAT == pReqSessionHdr->AuthType)
    {
        ResLen = Proc20Payload (pRMCPReq, pRMCPRes, pParams,Channel, BMCInst);
    }
    else
#endif
    {
        AuthType              = pReqSessionHdr->AuthType;
        SessionID             = pReqSessionHdr->SessionID;
        SessionSeqNum   = pReqSessionHdr->SessionSeqNum;

        /* Validate IPMI Session Header */
        if (TRUE != ValidateSessionHdr (SessionID, SessionSeqNum, BMCInst))
        {
            IPMI_WARNING ("RMCP.c : IPMI Session header validation failed\n");
            return 0;
        }

        /* Get Session Information */
        pSessionInfo = getSessionInfo (SESSION_ID_INFO, &SessionID, BMCInst);

        if (0 == pReqSessionHdr->AuthType)
        {
            IPMIMsgLen  = (INT8U) (*((_NEAR_ INT8U*)(pReqSessionHdr + 1)));

            pIPMIMsgReq = (_NEAR_ IPMIMsgHdr_T*) (((_NEAR_ INT8U*)(pReqSessionHdr + 1)) +
                    sizeof (IPMIMsgLen));
            pIPMIMsgRes = (_NEAR_ IPMIMsgHdr_T*) (((_NEAR_ INT8U*)(pResSessionHdr + 1)) +
                    sizeof (IPMIMsgLen));
        }
        else
        {
            pReqMsgAuthCode = ((_NEAR_ INT8U*)(pReqSessionHdr + 1));
            pResMsgAuthCode = ((_NEAR_ INT8U*)(pResSessionHdr + 1));
            IPMIMsgLen      = *(pReqMsgAuthCode + AUTH_CODE_LEN);
            pIPMIMsgReq     = (_NEAR_ IPMIMsgHdr_T*) (pReqMsgAuthCode + AUTH_CODE_LEN +
                    sizeof (IPMIMsgLen));
            pIPMIMsgRes     = (_NEAR_ IPMIMsgHdr_T*) (pResMsgAuthCode + AUTH_CODE_LEN +
                    sizeof (IPMIMsgLen));

            if (pSessionInfo == NULL)
            {
                IPMI_WARNING ("RMCP.c : pSessionInfo is NULL\n");
                return 0;
            }
#if 0
            if(FindUserLockStatus(pSessionInfo->UserId, Channel, BMCInst) == 0)
            {
                LOCK_BMC_SHARED_MEM(BMCInst);
                if (TRUE != ValidateAuthCode (pReqMsgAuthCode, pSessionInfo->Password,
                            pReqSessionHdr, pIPMIMsgReq))
                {
                    LockUser(pSessionInfo->UserId,Channel, BMCInst);
                    IPMI_WARNING ("RMCP.c : Invalid Authentication Code \n");
                    UNLOCK_BMC_SHARED_MEM(BMCInst);
                    return 0;
                }
                UnlockUser(pSessionInfo->UserId,Channel, BMCInst);
                UNLOCK_BMC_SHARED_MEM(BMCInst);
            }
            else
            {
                return 0;
            }
#endif
        }

        /* check for the pre-session commands */
        if (0 == SessionID)
        {
            int i;

            for (i = 0; i < sizeof (m_PreSessionCmdsTbl) / sizeof (PreSessionCmd_T); i++)
            {
                if ((m_PreSessionCmdsTbl[i].NetFn == (pIPMIMsgReq->NetFnLUN >> 2)) &&
                        (m_PreSessionCmdsTbl[i].Cmd == pIPMIMsgReq->Cmd))
                {
                    if( AuthType != AUTH_TYPE_NONE)
                    {
                        IPMI_WARNING("\n AuthType for Presession command not NONE\n");
                        return 0;
                    }
                    break;
                }
            }
            if (i >= (sizeof (m_PreSessionCmdsTbl) / sizeof (PreSessionCmd_T)))
            {
                return 0;
            }
        }
        else
        {
            if( AuthType  != pSessionInfo->AuthType)
            {
                IPMI_WARNING ("RMCP.c : Requested Authtype   mismatch with current Packet \n");
                return 0;
            }

            if(pBMCInfo->IpmiConfig.LinkDownResilentSupport == 1)
            {
                if(pSessionInfo->Linkstat == TRUE)
                {
                    pSessionInfo->Linkstat = FALSE;
                    memset(pSessionInfo->InboundTrac,0,SIXTEEN_COUNT_WINDOW_LEN);
                }
            }

            if(RmcpSeqNumValidation(pSessionInfo,ipmitoh_u32(SessionSeqNum),pIPMIMsgReq) != 0)
            {
                return 0;
            }
        }

        /* Frame the Message Packet for Message Handler */
        pBMCInfo->LANConfig.MsgReq.Cmd       = pIPMIMsgReq->Cmd;
        pBMCInfo->LANConfig.MsgReq.NetFnLUN  = pIPMIMsgReq->NetFnLUN;
        pBMCInfo->LANConfig.MsgReq.SessionID = SessionID;
        pBMCInfo->LANConfig.MsgReq.SessionType = LAN_SESSION_TYPE;

        UpdateGetMsgTime( &pBMCInfo->LANConfig.MsgReq,IfcType, BMCInst);

        /* If  Loopback session is created using External without libipmi  library LAN 1.5 */
        if(Channel == 0xFF && pParams->IsPktFromLoopBack ==TRUE )
        {
            IPMI_DBG_PRINT("\n LOOPBACK in 1.5\n");
            Channel = GetLANChannel(0, BMCInst);
        }

        pBMCInfo->LANConfig.MsgReq.Size = IPMIMsgLen;
        _fmemcpy(pBMCInfo->LANConfig.MsgReq.Data, (_FAR_ INT8U*) pIPMIMsgReq, pBMCInfo->LANConfig.MsgReq.Size);

        /* Post Msg to MsgHndlr and Get Res */
        if (0 != ProcIPMIReq (pSessionInfo, PAYLOAD_IPMI_MSG, pParams, Channel, BMCInst))
        {
            return 0;
        }

        /* Fill Response data */
        _fmemcpy (pRMCPRes, pRMCPReq, sizeof (RMCPHdr_T) + sizeof (SessionHdr_T));

        LOCK_BMC_SHARED_MEM(BMCInst);

        /* Increment session sequence number */
        if (NULL != pSessionInfo)
        {
            if(pSessionInfo->OutboundSeq == SEQNUM_ROLLOVER)
            {
                ((_NEAR_ SessionHdr_T*)pResSessionHdr)->SessionSeqNum = 0x00;
            }
            else
            {
                ((_NEAR_ SessionHdr_T*)pResSessionHdr)->SessionSeqNum =
                    pSessionInfo->OutboundSeq++;
            }
        }
        //#if 0
        /* Fill Authentication Code */
        if (0 != pReqSessionHdr->AuthType)
        {
            pResMsgAuthCode = (_NEAR_ INT8U*)(pResSessionHdr + 1);
            pIPMIMsgRes     = (_NEAR_ IPMIMsgHdr_T*)((_NEAR_ INT8U*)(pResSessionHdr + 1) +
                    AUTH_CODE_LEN + sizeof (IPMIMsgLen));
            IPMIMsgResLen      = AUTH_CODE_LEN + sizeof (IPMIMsgLen) + pBMCInfo->LANConfig.MsgRes.Size;
            /* Fill IPMI Message */
            _fmemcpy (pIPMIMsgRes, pBMCInfo->LANConfig.MsgRes.Data, pBMCInfo->LANConfig.MsgRes.Size);
            *(pResMsgAuthCode + AUTH_CODE_LEN) = pBMCInfo->LANConfig.MsgRes.Size;

            ComputeAuthCode (pSessionInfo->Password, pResSessionHdr, pIPMIMsgRes,
                    pResMsgAuthCode, MULTI_SESSION_CHANNEL);
        }
        else
            //#endif
        {
            pIPMIMsgRes = (_NEAR_ IPMIMsgHdr_T*)((_NEAR_ INT8U*)(pResSessionHdr + 1) +
                    sizeof (IPMIMsgLen));
            IPMIMsgResLen  = pBMCInfo->LANConfig.MsgRes.Size + sizeof (IPMIMsgLen);
            /* Fill IPMI Message */
            _fmemcpy (pIPMIMsgRes, pBMCInfo->LANConfig.MsgRes.Data, pBMCInfo->LANConfig.MsgRes.Size);
            *((_NEAR_ INT8U*) (pResSessionHdr + 1)) = pBMCInfo->LANConfig.MsgRes.Size;
        }

        UNLOCK_BMC_SHARED_MEM(BMCInst);
        ResLen = sizeof (RMCPHdr_T) + sizeof (SessionHdr_T) + IPMIMsgResLen;
    }
    /* Check if session to be deleted */
    if (0 != pBMCInfo->LANConfig.DeleteThisLANSessionID)
    {
        pSessionInfo = getSessionInfo (SESSION_ID_INFO, &pBMCInfo->LANConfig.DeleteThisLANSessionID, BMCInst);
        if (pSessionInfo == NULL)
        {
            IPMI_DBG_PRINT ("Error: No Session\n");
        }
        else
        {   
            //        	if ((AddLoginEvent ( pSessionInfo, EVENT_CONN_LOST, BMCInst )) != 0)
            //        	{
            //        		TCRIT("Problem while adding Log record \n");
            //        	}
            DeleteSession (pSessionInfo,BMCInst);
        }
        pBMCInfo->LANConfig.DeleteThisLANSessionID = 0;
    }

    return ResLen;
}

/**
 * @brief Process the IPMI request and prepare response.
 * @param pSessionInfo - Session information.
 * @param Payload     - Payload type.
 * @return 0 if success, -1 if error.
 **/
    static int
ProcIPMIReq (_FAR_  SessionInfo_T*  pSessionInfo, INT8U Payload, MiscParams_T *pParams,INT8U Channel, int BMCInst)
{
    _FAR_   ChannelInfo_T*      pChannelInfo;
    _FAR_   BMCInfo_t*          pBMCInfo = &g_BMCInfo[BMCInst];
    int	RetVal = 0;

    if (NULL != pSessionInfo)
    {
        pBMCInfo->LANConfig.MsgReq.Privilege         = pSessionInfo->Privilege;
#ifdef CONFIGURABLE_SESSION_TIME_OUT
        pSessionInfo->TimeOutValue = BMC_GET_SHARED_MEM(BMCInst)->uSessionTimeout;
#else
        if( pParams->IsPktFromVLAN || pParams->IsPktFromLoopBack )
            if(IPMITimeout > 0)
            {
                pSessionInfo->TimeOutValue = (IPMITimeout +10);
            }
            else
            {
                /*If it is not defined the timeout values for loop back session should be
                  SESSION_TIMEOUT defined in config.make.ipmi (60 seconds) */
                pSessionInfo->TimeOutValue =  pBMCInfo->IpmiConfig.SessionTimeOut; 
            }
        else
            pSessionInfo->TimeOutValue = pBMCInfo->IpmiConfig.SessionTimeOut; 
#endif

        if(pBMCInfo->IpmiConfig.SOLIfcSupport == 1)
        {
            if(pSessionInfo->SessPyldInfo [PAYLOAD_SOL].Type == PAYLOAD_SOL)
            {
                pSessionInfo->TimeOutValue = pBMCInfo->IpmiConfig.SOLSessionTimeOut;
            }
        }
    }

    /* Frame the Message Packet for Message Handler */
    pBMCInfo->LANConfig.MsgReq.Param      = PARAM_IFC;
    pBMCInfo->LANConfig.MsgReq.Channel    = Channel;
    strcpy ((char *)pBMCInfo->LANConfig.MsgReq.SrcQ, LAN_RES_Q);
    /* Check if this packet is from VLAN
     * if so indicate this to Msghandler
     * by adding it in BIT4. This will
     * not affect the regular channel
     * number as it is only four bits.
     */
    if( pParams->IsPktFromVLAN || pParams->IsPktFromLoopBack )
    {
        /* VLAN packet */
        pBMCInfo->LANConfig.MsgReq.Channel    |= LOOP_BACK_REQ;        	
    }

    if (PAYLOAD_IPMI_MSG == Payload)
    {
        IPMI_DBG_PRINT  ("RMCP.c : Posting message to Message Handler\n");
        pChannelInfo = getChannelInfo (Channel, BMCInst);
        if(pChannelInfo == NULL)
        {
            IPMI_WARNING("Invalid Channel number\n");
            return -1;
        }

#ifdef  LAN_RESTRICTIONS_BYPASS_FOR_LOOPBACK_AND_VLAN

        if ((pChannelInfo->AccessMode == 2) || (pParams->IsPktFromVLAN || pParams->IsPktFromLoopBack))
        {
            /* Post the message to message handler */
            if (0 != PostMsg (&pBMCInfo->LANConfig.MsgReq, MSG_HNDLR_Q,BMCInst))
            {
                IPMI_ASSERT (FALSE);
            }
        }
        else
        {
            IPMI_WARNING ("This is neither VLAN/loopback packet nor access mode enabled for this channel.\n");
            return -1;
        }
#else
        if((pChannelInfo->AccessMode == 0))
        {
            IPMI_WARNING("\n RMCP LAN Channel Disabled\n");
            return -1;
        }
        /* Post the message to message handler */
        IPMI_DBG_PRINT("RMCP.c : Post MsgReq:"); /*garden*/
        IPMI_DBG_PRINT_BUF (pBMCInfo->LANConfig.MsgReq.Data, pBMCInfo->LANConfig.MsgReq.Size);
        if (0 != PostMsg (&pBMCInfo->LANConfig.MsgReq, MSG_HNDLR_Q,BMCInst))
        {
            IPMI_ASSERT (FALSE);
        }
#endif
    }
    else if ((PAYLOAD_SOL == Payload) && (pSessionInfo->SessPyldInfo [PAYLOAD_SOL].Type == PAYLOAD_SOL))
    {
        if(pBMCInfo->IpmiConfig.SOLIfcSupport == 1)
        {
            /* Post the message to SOL queue */
            if (0 != PostMsg (&pBMCInfo->LANConfig.MsgReq,SOL_IFC_Q,BMCInst))
            {
                IPMI_ASSERT (FALSE);
            }
        }
        else
        {
            IPMI_ASSERT(FALSE);
        }
        return -1;
    }
    else
    {
        IPMI_ASSERT (FALSE);
        return -1;
    }

    do
    {
        RetVal = GetMsg (&pBMCInfo->LANConfig.MsgRes, LAN_RES_Q, pBMCInfo->LANConfig.MsgReq.ResTimeOut, BMCInst);

    }while(( g_corefeatures.ipmi_res_timeout == ENABLED) &&
            !RetVal && !IsResponseMatch(&pBMCInfo->LANConfig.MsgReq, &pBMCInfo->LANConfig.MsgRes) );

    if (-2 == RetVal)
    {
        FillIPMIResFailure (&pBMCInfo->LANConfig.MsgReq, &pBMCInfo->LANConfig.MsgRes, BMCInst);
    }
    else  if (0 != RetVal)
    {
        IPMI_ASSERT (FALSE);
        return -1;
    }

    IPMI_DBG_PRINT  ("RMCP.c : Received message from Message Handler\n");
    IPMI_DBG_PRINT("RMCP.c : Recv MsgRes:"); /*garden*/
    IPMI_DBG_PRINT_BUF (pBMCInfo->LANConfig.MsgRes.Data, pBMCInfo->LANConfig.MsgRes.Size);

    if (NO_RESPONSE == pBMCInfo->LANConfig.MsgRes.Param)
    {
        IPMI_WARNING ("RMCP.c : No response from message handler\n");
        return -1;
    }
#if 0    
    if (((pBMCInfo->LANConfig.MsgReq.NetFnLUN >> 2) == 0x06) && (pBMCInfo->LANConfig.MsgReq.Cmd  == 0x3A) &&
            (PAYLOAD_IPMI_MSG == Payload))
    {
        if ((AddLoginEvent ( pSessionInfo, EVENT_LOGIN, BMCInst )) != 0)
        {
            TCRIT("Problem while adding Log record \n");
        } 
    }    
#endif
    /* If Request IPMI Message is Close Session */
    if (((pBMCInfo->LANConfig.MsgReq.NetFnLUN >> 2) == 0x06) && (pBMCInfo->LANConfig.MsgReq.Cmd  == 0x3C) &&
            (PAYLOAD_IPMI_MSG == Payload))
    {
        /* If IPMI response is CC is NORMAL         */
        /* Remove Session Info from session table   */
        if (CC_NORMAL == pBMCInfo->LANConfig.MsgRes.Data [sizeof (IPMIMsgHdr_T)])
        {
            INT32U SessionID = 0;
            memcpy(&SessionID,&pBMCInfo->LANConfig.MsgReq.Data[sizeof(IPMIMsgHdr_T)],4);

            if(SessionID == pSessionInfo->SessionID)
            {
                //                if ((AddLoginEvent ( pSessionInfo, EVENT_LOGOUT, BMCInst )) != 0)
                //                {
                //                    TCRIT("Problem while adding Log record \n");
                //                }
                //                DeleteSession (pSessionInfo,BMCInst); /*Garden: or else, no next time*/
            }
        }
    }

    return 0;
}

/**
 * @brief Validate RMCP Header
 * @param pRMCPHdr - RMCP header.
 * @return TRUE if valid, FALSE if invalid.
 **/
    static BOOL
ValidateRMCPHdr (_NEAR_ RMCPHdr_T* pRMCPHdr)
{
    /* If RMCP Packet is NULL */
    if (pRMCPHdr == NULL)
    {
        IPMI_WARNING ("RMCP.c : RMCP Packet is NULL\n");
        return FALSE;
    }

    /* Verify RMCP Version */
    if (pRMCPHdr->Version != RMCP_VERSION)
    {
        IPMI_WARNING ("RMCP.c : Invalid RMCP Version\n");
        return FALSE;
    }

    /* LOOK for RMCP MessageClass */
    if ((pRMCPHdr->MsgClass != IPMI_MESSAGE_CLASS) &&
            (pRMCPHdr->MsgClass != 0x06))
    {
        IPMI_DBG_PRINT ("RMCP.c : Invalid Message Class\n");
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Validate session header.
 * @param SessionID - Session ID.
 * @param SeqNo    - Session Sequence Number.
 * @return TRUE if valid, FALSE if invalid.
 **/
    static BOOL
ValidateSessionHdr (INT32U SessionID, INT32U SeqNo, int BMCInst)
{
    _FAR_  SessionInfo_T*   pSessionInfo;

    /* if its Pre Session commands  */
    if (0 == SessionID)
    {
        return TRUE;
    }

    pSessionInfo = getSessionInfo (SESSION_ID_INFO, &SessionID, BMCInst);
    if ( pSessionInfo == NULL)
    {
        return FALSE;
    }

    /* If packet is already received - drop the packet  */
    /*  if (ntohs(SeqNo) <= ntohs (CAST_32 (&pSessionInfo->InboundSeq)))
        {
        IPMI_DBG_PRINT ("RMCP: Duplicate Seq No - Packet dropped\n");
        return FALSE;
        }
     */

    /* inc the Sequence No  */

    return TRUE;
}

/**
 * @brief Process RMCP Ping Message.
 * @param pRMCPReq - Request RMCP message.
 * @param pRMCPRes - Response RMCP message.
 * @return the response length.
 **/
    static INT8U
ProcessPingMsg (_NEAR_ RMCPHdr_T* pRMCPReq, _NEAR_ RMCPHdr_T* pRMCPRes,int BMCInst)
{
    _NEAR_ RMCPPingHdr_T* pReqPingHdr = (_NEAR_ RMCPPingHdr_T*)(pRMCPReq + 1);
    _NEAR_ RMCPPingHdr_T* pResPingHdr = (_NEAR_ RMCPPingHdr_T*)(pRMCPRes + 1);
    _FAR_ BMCInfo_t*        pBMCInfo = &g_BMCInfo[BMCInst];

    if (PRESENCE_PING_MSGTYPE != pReqPingHdr->MsgType) { return 0; }
    if((pReqPingHdr->IANANum[0]!=0x00)||(pReqPingHdr->IANANum[1]!=0x00)||
            (pReqPingHdr->IANANum[2]!=0x11)||(pReqPingHdr->IANANum[3]!=0xBE)) 
    { return 0; }

    /* Construct Response Header */
    _fmemcpy (pResPingHdr, pReqPingHdr, sizeof (RMCPPingHdr_T));
    pResPingHdr->MsgType = 0x40;
    pResPingHdr->DataLen = 0x10;

    /* Fill Response Data */
    _fmemset (pResPingHdr + 1, 0, pResPingHdr->DataLen);
    *((_NEAR_ INT8U*)(pResPingHdr + 1) + 8) = 0x81;
    *((_NEAR_ INT8U*)(pResPingHdr + 1) + 4) = PING_IPMI_15_SUPPORT;

#if IPMI20_SUPPORT == 1
    *((_NEAR_ INT8U*)(pResPingHdr + 1) + 4) |= PING_IPMI_20_SUPPORT;
#endif

    /*Update the OEM IANA Number for DCMI Discovery (36465 = Data Center Manageability Forum,Spec .1.5)*/
    if(g_corefeatures.dcmi_1_5_support == ENABLED)
    {
        if(pBMCInfo->IpmiConfig.DCMISupport == 1)
        {
            *((_NEAR_ INT8U*)(pResPingHdr + 1) + 2) = 0x8E;
            *((_NEAR_ INT8U*)(pResPingHdr + 1) + 3) = 0x71;
        }
    }

    return (sizeof (RMCPHdr_T) + sizeof (RMCPPingHdr_T) + pResPingHdr->DataLen);
}

#if 0
/**
 * @brief Validate Authentication Code
 * @param pAuthCode - Request authentication code.
 * @param pPassword - Password string.
 * @param pSessionHdr - Request Session header.
 * @param pIPMIMsg - Request IPMI message.
 * @return TRUE if valid, FALSE if invalid.
 **/
    static BOOL
ValidateAuthCode (_NEAR_ INT8U* pAuthCode, _FAR_ INT8U* pPassword,
        _NEAR_ SessionHdr_T* pSessionHdr, _NEAR_ IPMIMsgHdr_T* pIPMIMsg)
{
    INT8U   ComputedAuthCode [AUTH_CODE_LEN];

    memset(ComputedAuthCode, 0, sizeof (ComputedAuthCode));

    ComputeAuthCode (pPassword, pSessionHdr, pIPMIMsg,
            ComputedAuthCode, MULTI_SESSION_CHANNEL);

    return (0 == _fmemcmp (pAuthCode, ComputedAuthCode, AUTH_CODE_LEN));
}
#endif
#if IPMI20_SUPPORT

/*-------------------------------------------
 * Frame20Payload
 *-------------------------------------------*/
int
Frame20Payload (INT8U PayloadType, _NEAR_ RMCPHdr_T* pRMCPPkt,
        _FAR_ INT8U* pPayload,  INT32U PayloadLen, _FAR_
        SessionInfo_T* pSessionInfo, int BMCInst)
{
    _FAR_  BMCInfo_t*       pBMCInfo        = &g_BMCInfo[BMCInst];
    _NEAR_ SessionHdr2_T*   pResSessionHdr  = (_NEAR_ SessionHdr2_T*)(pRMCPPkt + 1);
    _NEAR_ INT8U*           pRes            = (_NEAR_ INT8U*)(pResSessionHdr + 1);
    _NEAR_ INT8U*           pConfHdr;
    _NEAR_ INT8U*           pConfPayld;
    _NEAR_ INT8U*           pIntPad;
    _NEAR_ INT8U*           pResMsgAuthCode;
    INT8U                   ConfPadLen, IntPadLen;
    INT16U                  ConfPayldLen, AuthCodeLen;
    int                     i, ResLen;
    unsigned int            seed = 1;

    /* Fill Session Hdr */
    pResSessionHdr->AuthType      = RMCP_PLUS_FORMAT;
    pResSessionHdr->PayloadType   = PayloadType;

    LOCK_BMC_SHARED_MEM(BMCInst);

    if (NULL == pSessionInfo)
    {
        pResSessionHdr->SessionID = 0;
        pResSessionHdr->SessionSeqNum = 0;
    }
    else
    {
        /* Response packets should send the Remote Console
         * Session ID so the remote console can correctly
         * match up the session with its own table of active
         * session IDs. */
        pResSessionHdr->SessionID = pSessionInfo->RemConSessionID;

        /* Increment session sequence number */
        /* During RMCP Opensession , OutboundSeq initialized to 0 and but 0 is  reserved  */
        /* and also  When It reach 0xffffffff then It  become zero */
        if(0==pSessionInfo->OutboundSeq)
        {
            pSessionInfo->OutboundSeq=1;
        }
        pResSessionHdr->SessionSeqNum = htoipmi_u32(pSessionInfo->OutboundSeq++);
    }

    /* Fill Payload and Do Encryption if needed */
    if ((NULL != pSessionInfo) &&
            (0 != (pResSessionHdr->PayloadType & CONFIDENT_MASK)))
    {
        pConfHdr = (_NEAR_ INT8U*)(pResSessionHdr + 1);
        switch (pSessionInfo->ConfidentialityAlgorithm)
        {
            case CONF_AES_CBC_128:

                /* Fill Init Vector */
                for (i =0; i < CONF_AES_CBC_128_HDR_LEN; i++)
                {
                    pConfHdr [i] = (INT8U)rand_r (&seed);
                }
                pConfPayld = pConfHdr + CONF_AES_CBC_128_HDR_LEN;

                /* Add Padding; include size of confpadlen */
                ConfPadLen = (PayloadLen + 1) % CONF_BLOCK_SIZE;
                if (0 != ConfPadLen)
                {
                    ConfPadLen =  CONF_BLOCK_SIZE - ConfPadLen;
                }
                for (i = 0; i < ConfPadLen; i++)
                {
                    *(pPayload + PayloadLen + i) = i + 1;
                }
                *(pPayload + PayloadLen + ConfPadLen) = ConfPadLen;
                ConfPayldLen = PayloadLen + ConfPadLen + 1;
                //            aesEncrypt ((_FAR_ INT8U*)pPayload,  ConfPayldLen,
                //                        (_FAR_ INT8U*)pConfHdr, pSessionInfo->Key2,
                //                        (_FAR_ INT8U*)pConfPayld);

                IPMI_DBG_PRINT_BUF ((_FAR_ INT8U*)pConfPayld, ConfPayldLen);
                PayloadLen	  = ConfPayldLen + CONF_AES_CBC_128_HDR_LEN;
                pBMCInfo->LANConfig.MsgReq.Size = PayloadLen;
                break;

            case CONF_xCR4_128:
            case CONF_xCR4_40:
            default:
                IPMI_WARNING ("RMCP.c : Invalid confidentiality Algorithm\n");
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                return 0;
        }
    }
    else
    {
        _fmemcpy (pRes, pPayload, PayloadLen);
    }

    /* Fill Payload Length */
    pResSessionHdr->IPMIMsgLen = htoipmi_u16 (PayloadLen);

    ResLen = sizeof (RMCPHdr_T) + sizeof (SessionHdr2_T) + PayloadLen;

    /* Add Integrity Check Value */
    if ((NULL != pSessionInfo) &&
            (0 != (pResSessionHdr->PayloadType & INTEGRITY_MASK)))
    {
        /* Add Integrity Pad */
        pIntPad   = (_NEAR_ INT8U*)(pResSessionHdr + 1) + PayloadLen;
        IntPadLen = (sizeof (SessionHdr2_T) + PayloadLen + 2) % sizeof (INT32U);
        if (0 != IntPadLen)
        {
            IntPadLen  = sizeof(INT32U) - IntPadLen;
        }
        _fmemset (pIntPad, 0xFF, IntPadLen);
        *(pIntPad + IntPadLen)      = IntPadLen;    /* Integrity Pad Len  */
        *(pIntPad + IntPadLen + 1)  = 0x07;         /* Next Header        */

        pResMsgAuthCode =  pIntPad + IntPadLen + 2;
        AuthCodeLen     = sizeof (SessionHdr2_T) + PayloadLen + IntPadLen + 2;
        ResLen          += IntPadLen + 2;

        switch (pSessionInfo->IntegrityAlgorithm)
        {
            case AUTH_HMAC_SHA1_96:

                //            hmac_sha1 ((char *)pSessionInfo->Key1, HASH_KEY1_SIZE,
                //                       (_FAR_ INT8S*)pResSessionHdr, AuthCodeLen,
                //                       (char *)pResMsgAuthCode, MAX_INTEGRITY_LEN);
                ResLen += HMAC_SHA1_96_LEN;
                break;

            case AUTH_HMAC_MD5_128:
                //            hmac_md5(pSessionInfo->Key1,HASH_KEY1_SIZE,(_FAR_ INT8U*)pResSessionHdr, AuthCodeLen,
                //            pResMsgAuthCode, MAX_HMAC_MD5_INTEGRITY_LEN);
                ResLen += HMAC_MD5_LEN;
                break;

            case AUTH_MD5_128:
                //            MD5_128((char *)pSessionInfo->Password,MAX_PASSWORD_LEN,(_FAR_ INT8S*)pResSessionHdr, AuthCodeLen,
                //            (char *)pResMsgAuthCode, MAX_MD5_INTEGRITY_LEN);
                ResLen += MD5_LEN;
                break;

            case AUTH_HMAC_SHA256_128:

                //			hmac_sha256 ((unsigned char *)pSessionInfo->Key1, SHA2_HASH_KEY_SIZE, (unsigned char *)pResSessionHdr,
                //					AuthCodeLen, (unsigned char *)pResMsgAuthCode, HMAC_SHA256_128_LEN);                
                ResLen += HMAC_SHA256_128_LEN;  
                break;

            default:
                IPMI_WARNING ("RMCP.c : Invalid Integrity Algorithm\n");
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                return 0;
        }
    }

    UNLOCK_BMC_SHARED_MEM(BMCInst);

    return ResLen;
}

int RMCPplusSeqNumValidation(SessionInfo_T * pSessionInfo,INT32U SessionSeqNum)
{
    INT32U SeqTrac[SIXTEEN_COUNT_WINDOW_LEN];
    BOOL TrackRollOver = FALSE;
    INT32U TrackRollOverSeq = SEQNUM_ROLLOVER;
    int i=0;


    if( pSessionInfo->InboundSeq == SessionSeqNum)
        return -1;

    if((pSessionInfo->InboundSeq < (SEQNUM_ROLLOVER -RMCPPLUS_SEQUPLIMIT)) && (pSessionInfo->InboundSeq > RMCPPLUS_SEQLOWLIMIT))
    {
        if(SessionSeqNum < pSessionInfo->InboundSeq)
        {
            if((pSessionInfo->InboundSeq -SessionSeqNum) > RMCPPLUS_SEQLOWLIMIT)
                return -1;
            else
            {
                for(i=0; i < RMCPPLUS_SEQLOWLIMIT; i++)
                {
                    if(SessionSeqNum == pSessionInfo->InboundTrac[i])
                    {
                        if(((1 << i) & pSessionInfo->InboundRecv) != 0)
                            return -1;
                        else
                            pSessionInfo->InboundRecv |= (1<<i);
                    }
                }
            }
        }
        else
        {
            if((SessionSeqNum - pSessionInfo->InboundSeq) > RMCPPLUS_SEQUPLIMIT)
                return -1;

            _fmemcpy((INT8U *)SeqTrac,(INT8U *)pSessionInfo->InboundTrac,(sizeof(INT32U) * SIXTEEN_COUNT_WINDOW_LEN));

            for(i=0; i < (SessionSeqNum - pSessionInfo->InboundSeq); i++)
                pSessionInfo->InboundTrac[i] = SessionSeqNum - (i+1);


            pSessionInfo->InboundRecv = pSessionInfo->InboundRecv << (SessionSeqNum - pSessionInfo->InboundSeq);
            pSessionInfo->InboundRecv |= (1 << ((SessionSeqNum - pSessionInfo->InboundSeq)-1));

            _fmemcpy((INT8U *)&pSessionInfo->InboundTrac[SessionSeqNum - pSessionInfo->InboundSeq],
                    (INT8U *)&SeqTrac[0],
                    (sizeof(INT32U) *(SIXTEEN_COUNT_WINDOW_LEN - (SessionSeqNum - pSessionInfo->InboundSeq))));
            pSessionInfo->InboundSeq = SessionSeqNum;
        }
    }
    else if((pSessionInfo->InboundSeq  < RMCPPLUS_SEQLOWLIMIT)
            || (pSessionInfo->InboundSeq  > (SEQNUM_ROLLOVER -RMCPPLUS_SEQUPLIMIT)))  /* Checking condition for rollover */
    {
        if(SessionSeqNum < pSessionInfo->InboundSeq)
        {
            if(!((((pSessionInfo->InboundSeq -SessionSeqNum) <= RMCPPLUS_SEQLOWLIMIT) &&
                            (((SEQNUM_ROLLOVER - pSessionInfo->InboundSeq) + SessionSeqNum+1) >= RMCPPLUS_SEQLOWLIMIT )) ||
                        ((((SEQNUM_ROLLOVER - pSessionInfo->InboundSeq) + SessionSeqNum+1) <= RMCPPLUS_SEQLOWLIMIT ) &&
                         (pSessionInfo->InboundSeq -SessionSeqNum) >= RMCPPLUS_SEQLOWLIMIT)))
            {
                return -1;
            }
            else
            {
                if((pSessionInfo->InboundSeq -SessionSeqNum) <= RMCPPLUS_SEQLOWLIMIT)
                {
                    for(i=0; i < RMCPPLUS_SEQLOWLIMIT; i++)
                    {
                        if(SessionSeqNum == pSessionInfo->InboundTrac[i])
                        {
                            if(((1 << i) & pSessionInfo->InboundRecv) != 0)
                                return -1;
                            else
                                pSessionInfo->InboundRecv |= (1<<i);
                        }
                    }
                }
                else if(((SEQNUM_ROLLOVER - pSessionInfo->InboundSeq) + SessionSeqNum+1) <= RMCPPLUS_SEQUPLIMIT )
                {
                    _fmemcpy((INT8U *)SeqTrac,(INT8U *)pSessionInfo->InboundTrac,(sizeof(INT32U) * SIXTEEN_COUNT_WINDOW_LEN));

                    for(i=0; i < ((SEQNUM_ROLLOVER - pSessionInfo->InboundSeq) + SessionSeqNum+1) ; i++)
                    {
                        if(((SessionSeqNum - (i+1)) != 0) && (TrackRollOver == FALSE))
                            pSessionInfo->InboundTrac[i] = SessionSeqNum - (i+1);
                        else if(((SessionSeqNum - (i+1)) == 0) && (TrackRollOver == FALSE))
                        {
                            pSessionInfo->InboundTrac[i] = SessionSeqNum - (i+1);
                            TrackRollOver = TRUE;
                        }
                        else if(TrackRollOver ==  TRUE)
                        {
                            pSessionInfo->InboundTrac[i] = TrackRollOverSeq;
                            TrackRollOverSeq--;
                        }
                    }
                    TrackRollOverSeq = SEQNUM_ROLLOVER;
                    TrackRollOver = FALSE;

                    pSessionInfo->InboundRecv = pSessionInfo->InboundRecv << ((SEQNUM_ROLLOVER - pSessionInfo->InboundSeq) + SessionSeqNum+1);
                    pSessionInfo->InboundRecv |= (1 << (((SEQNUM_ROLLOVER - pSessionInfo->InboundSeq) + SessionSeqNum+1) -1));

                    _fmemcpy((INT8U *)&pSessionInfo->InboundTrac[(SEQNUM_ROLLOVER - pSessionInfo->InboundSeq) + SessionSeqNum+1],
                            (INT8U *)&SeqTrac[0],
                            (sizeof(INT32U) *(SIXTEEN_COUNT_WINDOW_LEN - ((SEQNUM_ROLLOVER - pSessionInfo->InboundSeq) + SessionSeqNum+1))));
                    pSessionInfo->InboundSeq = SessionSeqNum;
                }
            }
        }
        else if(SessionSeqNum > pSessionInfo->InboundSeq)
        {
            if(!((((SessionSeqNum -pSessionInfo->InboundSeq) <= RMCPPLUS_SEQUPLIMIT) &&
                            (((SEQNUM_ROLLOVER - SessionSeqNum) + pSessionInfo->InboundSeq+1) >= RMCPPLUS_SEQUPLIMIT )) ||
                        ((((SEQNUM_ROLLOVER - SessionSeqNum) + pSessionInfo->InboundSeq+1) <= RMCPPLUS_SEQLOWLIMIT ) &&
                         (SessionSeqNum-pSessionInfo->InboundSeq) >= RMCPPLUS_SEQLOWLIMIT)))
            {
                return -1;
            }
            else
            {
                if((SessionSeqNum  - pSessionInfo->InboundSeq) <= RMCPPLUS_SEQUPLIMIT)
                {
                    _fmemcpy((INT8U *)SeqTrac,(INT8U *)pSessionInfo->InboundTrac,(sizeof(INT32U) * SIXTEEN_COUNT_WINDOW_LEN));
                    for(i=0; i < (SessionSeqNum  - pSessionInfo->InboundSeq) ; i++)
                    {
                        if(((SessionSeqNum - (i+1)) != 0) && (TrackRollOver == FALSE))
                            pSessionInfo->InboundTrac[i] = SessionSeqNum - (i+1);
                        else if(((SessionSeqNum - (i+1)) == 0) && (TrackRollOver == FALSE))
                        {
                            pSessionInfo->InboundTrac[i] = SessionSeqNum - (i+1);
                            TrackRollOver = TRUE;
                        }
                        else if(TrackRollOver ==  TRUE)
                        {
                            pSessionInfo->InboundTrac[i] = TrackRollOverSeq;
                            TrackRollOverSeq--;
                        }
                    }
                    TrackRollOverSeq = SEQNUM_ROLLOVER;
                    TrackRollOver = FALSE;

                    pSessionInfo->InboundRecv = pSessionInfo->InboundRecv << (SessionSeqNum  - pSessionInfo->InboundSeq);
                    pSessionInfo->InboundRecv |= (1 << ((SessionSeqNum  - pSessionInfo->InboundSeq) -1));

                    _fmemcpy((INT8U *)&pSessionInfo->InboundTrac[SessionSeqNum  - pSessionInfo->InboundSeq],
                            (INT8U *)&SeqTrac[0],
                            (sizeof(INT32U) *(SIXTEEN_COUNT_WINDOW_LEN - (SessionSeqNum - pSessionInfo->InboundSeq))));
                    pSessionInfo->InboundSeq = SessionSeqNum;
                }
                else if(((SEQNUM_ROLLOVER -SessionSeqNum) + pSessionInfo->InboundSeq+1) <= RMCPPLUS_SEQLOWLIMIT)
                {
                    for(i=0; i < RMCPPLUS_SEQLOWLIMIT; i++)
                    {
                        if(SessionSeqNum == pSessionInfo->InboundTrac[i])
                        {
                            if(((1 << i) & pSessionInfo->InboundRecv) != 0)
                                return -1;
                            else
                                pSessionInfo->InboundRecv |= (1<<i);
                        }
                    }
                }
            }
        }
    }

    return 0;
}


/**
 * @brief Process IPMI 2.0 Payload.
 * @param pRMCPReq - RMCP request message.
 * @param pRMCPRes _ RMCP response message.
 * @return 0 if success, -1 if error.
 **/
    static int
Proc20Payload (_NEAR_ RMCPHdr_T* pRMCPReq, _NEAR_ RMCPHdr_T* pRMCPRes, MiscParams_T *pParams,INT8U Channel, int BMCInst)
{
    _NEAR_ SessionHdr2_T*  pReqSession2Hdr = (_NEAR_ SessionHdr2_T*)(pRMCPReq + 1);
    _NEAR_ SessionHdr2_T*  pResSession2Hdr = (_NEAR_ SessionHdr2_T*)(pRMCPRes + 1);
    _NEAR_ INT8U*          pReq  = (_NEAR_ INT8U *)(pReqSession2Hdr + 1);
    _NEAR_ INT8U*          pRes  = (_NEAR_ INT8U *)(pResSession2Hdr + 1);
    _FAR_  SessionInfo_T*  pSessionInfo = NULL;
    _NEAR_ INT8U*          pIntPad;
    _NEAR_ INT8U*          pConfHdr;
    _NEAR_ INT8U*          pConfPayld;
    _NEAR_ INT8U*          pReqMsgAuthCode;
    _FAR_  UserInfo_T*     pUserInfo;
    _FAR_  BMCInfo_t*      pBMCInfo = &g_BMCInfo[BMCInst];
    INT8U           Payload, IntPadLen, ComputedAuthCode [25];
    INT16U          IPMIMsgLen, AuthCodeLen, ConfPayldLen;
    INT32U          SessionID;
    INT32U          SessionSeqNum;
    int             len=0, i;
    INT8U           UserPswd [MAX_PASSWORD_LEN];
    INT8U PwdEncKey[MAX_SIZE_KEY] = {0};

    /* Get SessionID & Session Seq */
    SessionID       = pReqSession2Hdr->SessionID;
    SessionSeqNum   = pReqSession2Hdr->SessionSeqNum;

    /* Validate IPMI Session Header */
    if (TRUE != ValidateSessionHdr (SessionID, SessionSeqNum, BMCInst))
    {
        //        IPMI_WARNING ("RMCP.c : IPMI Session header validation failed\n");
        return 0;
    }

    IPMIMsgLen = ipmitoh_u16 (pReqSession2Hdr->IPMIMsgLen);
    Payload    = pReqSession2Hdr->PayloadType & 0x3F;

    /* Process PreSession Payloads */
    for (i = 0; i < sizeof (m_PayloadTbl) / sizeof (m_PayloadTbl [0]); i++)
    {
        if (m_PayloadTbl [i].Payload == Payload)
        {
            /* Copy RMCP & Session Hdr */
            _fmemcpy ((_FAR_ INT8U*)pRMCPRes, (_FAR_ INT8U*)pRMCPReq,
                    sizeof (RMCPHdr_T) +  sizeof (SessionHdr2_T));

            /* For response the type is type + 1 */
            pResSession2Hdr->PayloadType++;
            /* Copy message tag from request */
            *pRes = *pReq;

            /* Call the function and pass the data after message tag */
            LOCK_BMC_SHARED_MEM(BMCInst);
            //            len = m_PayloadTbl [i].PayloadHndlr ((pReq),
            //                  (INT8U)(IPMIMsgLen - sizeof (INT8U)), (pRes), pParams,Channel,BMCInst);
            UNLOCK_BMC_SHARED_MEM(BMCInst);

            /* Copy the message length */
            pResSession2Hdr->IPMIMsgLen = htoipmi_u16 ((len));

            if(len!=0)
                len += sizeof (RMCPHdr_T) + sizeof (SessionHdr2_T);

            return len;
        }
    }

    /* Check for Invalid Payload Type */
    if ((PAYLOAD_IPMI_MSG !=  Payload) && (PAYLOAD_SOL !=  Payload))
    {
        IPMI_WARNING ("RMCP.c : Invalid payload\n");
        return 0;
    }

    /* check for the pre-session commands */
    if (0 == SessionID)
    {
        int i;
        _NEAR_ IPMIMsgHdr_T* pIPMIMsg = (_NEAR_ IPMIMsgHdr_T*) pReq;

        for (i=0; i < sizeof (m_PreSessionCmdsTbl) / sizeof (PreSessionCmd_T); i++)
        {
            if ((m_PreSessionCmdsTbl[i].NetFn == (pIPMIMsg->NetFnLUN >> 2)) &&
                    (m_PreSessionCmdsTbl[i].Cmd == pIPMIMsg->Cmd))
            {
                pBMCInfo->LANConfig.MsgReq.Size = IPMIMsgLen;
                _fmemcpy (pBMCInfo->LANConfig.MsgReq.Data, pReq, pBMCInfo->LANConfig.MsgReq.Size);
                break;
            }
        }
        if (i >= (sizeof (m_PreSessionCmdsTbl) / sizeof (PreSessionCmd_T)))
        {
            IPMI_WARNING ("RMCP.c : Presession command not found\n");
            return 0;
        }
    }
    else
    {
        /* Get Session Information */
        pSessionInfo = getSessionInfo (SESSION_ID_INFO, &SessionID, BMCInst);

        if (NULL == pSessionInfo)
        {
            IPMI_WARNING ("RMCP.c : Proc20Payload - Invalid Session Id\n");
            return 0;
        }

        LOCK_BMC_SHARED_MEM(BMCInst);
        /* Check if session is activated */
        if (TRUE != pSessionInfo->Activated)
        {
            IPMI_WARNING ("RMCP.c : Session not activated with session id %lx\n", SessionID);
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            return 0;
        }

        if(pBMCInfo->IpmiConfig.LinkDownResilentSupport == 1)
        {
            if(pSessionInfo->Linkstat == TRUE)
            {
                pSessionInfo->Linkstat = FALSE;
                memset(pSessionInfo->InboundTrac,0,SIXTEEN_COUNT_WINDOW_LEN);
            }
        }

        if(((((_FAR_ IPMIMsgHdr_T*)pBMCInfo->LANConfig.MsgReq.Data)->NetFnLUN)>>2==NETFN_APP)&&((((_FAR_ IPMIMsgHdr_T*)pBMCInfo->LANConfig.MsgReq.Data)->Cmd)==CMD_SET_SESSION_PRIV_LEVEL )&&((pSessionInfo->EventFlag)== 1))
        {
            pSessionInfo->EventFlag=0;
            //        			if ((AddLoginEvent ( pSessionInfo, EVENT_LOGIN, BMCInst )) != 0)
            //        				TCRIT("Problem while adding Log record \n");        			
        }

        if(RMCPplusSeqNumValidation(pSessionInfo,ipmitoh_u32(SessionSeqNum)) != 0)
        {
            UNLOCK_BMC_SHARED_MEM(BMCInst);
            return 0;
        }

        if (0 != (pReqSession2Hdr->PayloadType & INTEGRITY_MASK))
        {
            INT8U Len;

            /*  check Integrity pad which starts from auth type till auth code */
            pIntPad   = (_NEAR_ INT8U*)(pReqSession2Hdr + 1) + IPMIMsgLen;

            IntPadLen = (sizeof (SessionHdr2_T) + IPMIMsgLen + 2) % sizeof (INT32U);
            if (0 != IntPadLen)
            {
                IntPadLen  = sizeof(INT32U) - IntPadLen;
            }

            if (pIntPad [IntPadLen] != IntPadLen)
            {
                IPMI_WARNING ("RMCP.c : Invalid Padlength\n");
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                return 0;
            }

            /* Check auth code */
            pReqMsgAuthCode =  pIntPad + IntPadLen + 2;
            AuthCodeLen     = sizeof (SessionHdr2_T) + IPMIMsgLen + IntPadLen + 2;


            switch (pSessionInfo->IntegrityAlgorithm)
            {
                case AUTH_HMAC_SHA1_96:
                    //                hmac_sha1 ((char *)pSessionInfo->Key1, HASH_KEY1_SIZE,
                    //                           (_FAR_ INT8S*)pReqSession2Hdr, AuthCodeLen,
                    //                           (_FAR_ INT8S*)ComputedAuthCode, MAX_INTEGRITY_LEN);
                    Len = HMAC_SHA1_96_LEN;
                    break;

                case AUTH_HMAC_MD5_128:
                    //                hmac_md5 (pSessionInfo->Key1, HASH_KEY1_SIZE,
                    //                            (_FAR_ INT8U*)pReqSession2Hdr, AuthCodeLen,
                    //                            (_FAR_ INT8U*)ComputedAuthCode, MAX_HMAC_MD5_INTEGRITY_LEN);
                    Len = HMAC_MD5_LEN;
                    break;

                case AUTH_MD5_128:
                    /* Get User Info */
                    pUserInfo = getUserIdInfo((INT8U)pSessionInfo->UserId, BMCInst);
#if 0
                    if (g_corefeatures.userpswd_encryption == ENABLED)
                    {
                        if(getEncryptKey(PwdEncKey))
                        {
                            TCRIT("Error in getting the encryption key. quittting...\n");
                            return -1;
                        }
                        if(DecryptPassword((INT8S *)(pBMCInfo->EncryptedUserInfo[pSessionInfo->UserId - 1].EncryptedPswd), MAX_PASSWORD_LEN, (INT8S *)UserPswd, MAX_PASSWORD_LEN, PwdEncKey))
                        {
                            TCRIT("Error in decrypting the user password for user ID:%d. .\n", pSessionInfo->UserId);
                            return -1;
                        }
                    }
                    else
#endif
                    {
                        _fmemcpy (UserPswd, pUserInfo->UserPassword, MAX_PASSWORD_LEN);
                    }

                    //                MD5_128((char *)pUserInfo->UserPassword, MAX_PASSWORD_LEN,
                    //                            (_FAR_ INT8S*)pReqSession2Hdr, AuthCodeLen,
                    //                            (_FAR_ INT8S*)ComputedAuthCode, MAX_MD5_INTEGRITY_LEN);
                    Len= MD5_LEN;
                    break;

                case AUTH_HMAC_SHA256_128:

                    //				hmac_sha256 ((unsigned char *)pSessionInfo->Key1, SHA2_HASH_KEY_SIZE, (unsigned char *)pReqSession2Hdr,
                    //						AuthCodeLen, (unsigned char *)ComputedAuthCode, SHA2_HASH_KEY_SIZE);                
                    Len= HMAC_SHA256_128_LEN;
                    break;                

                default:
                    IPMI_WARNING ("RMCP.c : Invalid Integrity Algorithm\n");
                    UNLOCK_BMC_SHARED_MEM(BMCInst);
                    return 0;
            }

            IPMI_DBG_PRINT_BUF ((_FAR_ INT8U*)ComputedAuthCode, Len);
            if (0 != _fmemcmp ((_FAR_ INT8U*)ComputedAuthCode, pReqMsgAuthCode, Len))
            {
                IPMI_WARNING ("RMCP.c : Integrity failed\n");
                UNLOCK_BMC_SHARED_MEM(BMCInst);
                return 0;
            }
        }

        /*  Decrypt the message if Encrypted */
        /* Verify confidentiality header and trailer */
        if (0 != (pReqSession2Hdr->PayloadType & CONFIDENT_MASK))
        {
            pConfHdr = (_NEAR_ INT8U*)(pReqSession2Hdr + 1);
            switch (pSessionInfo->ConfidentialityAlgorithm)
            {
                case CONF_AES_CBC_128:
                    pConfPayld   = pConfHdr + CONF_AES_CBC_128_HDR_LEN;
                    ConfPayldLen = IPMIMsgLen - CONF_AES_CBC_128_HDR_LEN;
                    //                aesDecrypt ((_FAR_ INT8U*)pConfPayld,  ConfPayldLen,
                    //                            (_FAR_ INT8U*)pConfHdr, pSessionInfo->Key2,
                    //                            (_FAR_ INT8U*)pBMCInfo->LANConfig.MsgReq.Data);

                    /* Remove pad length */
                    if (pBMCInfo->LANConfig.MsgReq.Data [ConfPayldLen - 1] >  CONF_BLOCK_SIZE)
                    {
                        IPMI_WARNING ("Invalid Conf Blocke size  %d\n", pBMCInfo->LANConfig.MsgReq.Data [ConfPayldLen - 1]);
                        UNLOCK_BMC_SHARED_MEM(BMCInst);
                        return 0;
                    }

                    ConfPayldLen -= (pBMCInfo->LANConfig.MsgReq.Data [ConfPayldLen - 1] + 1);
                    break;

                case CONF_xCR4_128:

                case CONF_xCR4_40:

                default:
                    IPMI_WARNING ("RMCP.c : Invalid confidentiality Algorithm\n");
                    UNLOCK_BMC_SHARED_MEM(BMCInst);
                    return 0;
            }

            pBMCInfo->LANConfig.MsgReq.Size = ConfPayldLen;
        }
        else
        {
            pBMCInfo->LANConfig.MsgReq.Size = IPMIMsgLen;
            _fmemcpy (pBMCInfo->LANConfig.MsgReq.Data, pReq, pBMCInfo->LANConfig.MsgReq.Size);
        }
        UNLOCK_BMC_SHARED_MEM(BMCInst);
    }

    /* Fill IPMI MsgPkt Request */
    pBMCInfo->LANConfig.MsgReq.Cmd       = ((_FAR_ IPMIMsgHdr_T*)pBMCInfo->LANConfig.MsgReq.Data)->Cmd;
    pBMCInfo->LANConfig.MsgReq.NetFnLUN  = ((_FAR_ IPMIMsgHdr_T*)pBMCInfo->LANConfig.MsgReq.Data)->NetFnLUN;
    pBMCInfo->LANConfig.MsgReq.SessionID = SessionID;
    pBMCInfo->LANConfig.MsgReq.SessionType = LAN_SESSION_TYPE;
    pBMCInfo->LANConfig.MsgReq.Channel =Channel;
    UpdateGetMsgTime( &pBMCInfo->LANConfig.MsgReq,IfcType, BMCInst);
    // Moved to line 966
    // Bug : if SessionID is 0 this Unlock will be called without a Lock
    //UNLOCK_BMC_SHARED_MEM();

    /* Process IPMI Request */
    if (0 != ProcIPMIReq (pSessionInfo, Payload, pParams,Channel, BMCInst))
    {
        return 0;
    }

    /* Fill Response data */
    _fmemcpy (pRMCPRes, pRMCPReq, sizeof (RMCPHdr_T) + sizeof (SessionHdr_T));


    return Frame20Payload (pReqSession2Hdr->PayloadType, pRMCPRes,
            pBMCInfo->LANConfig.MsgRes.Data, pBMCInfo->LANConfig.MsgRes.Size, pSessionInfo, BMCInst);
}

/**************************************************************************** 
 * fn AddLoginEvent
 * params:
 * pRMCPSession  pointer to RMCP Session information
 * EvtType	0x9 - login, 0xa - logout, 0xb - autologout, 0xc - connection lost
 *
 * return 	0 - success, -1 - failure
 ***************************************************************************/
#if 0 
int AddLoginEvent ( SessionInfo_T *pRMCPSession, unsigned char EvtType, int BMCInst)
{
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    int reslen = 0, ret = -1;
    AddSELRes_T  AddSelRes;
    INT8U IPAddr[IP_ADDR_LEN];
    SELOEM1Record_T  OEMSELRec;

#ifdef  CONFIG_SPX_FEATURE_IANA_0
    INT8U  MfgID[] = {  CONFIG_SPX_FEATURE_IANA_2,
        CONFIG_SPX_FEATURE_IANA_1, 
        CONFIG_SPX_FEATURE_IANA_0 };
#else
    INT8U MfgID[] = { 0, 0, 0 };
#endif

    /* Hook for RMCP Login Audit */
    if  (g_PDKHandle[PDK_RMCPLOGINAUDIT] != NULL) 
    {
        /* Return if event type bit is not set in event mask */
        if ( !((pBMCInfo->LoginAuditCfg.IPMIEventMask >> (EvtType - IPMI_EVENT_TYPE_BASE)) & 0x01 ) )
        {
            return reslen;
        }
        if(g_corefeatures.global_ipv6  == ENABLED)
        {
            ret = ((int(*)(INT8U , INT8U, INT8U *,int))(g_PDKHandle[PDK_RMCPLOGINAUDIT]))(EvtType, pRMCPSession->UserId, pRMCPSession->LANRMCPPkt.IPHdr.Srcv6Addr, BMCInst);
        }
        else
        {
            ret = ((int(*)(INT8U , INT8U, INT8U *,int))(g_PDKHandle[PDK_RMCPLOGINAUDIT]))(EvtType, pRMCPSession->UserId, pRMCPSession->LANRMCPPkt.IPHdr.Srcv4Addr, BMCInst);
        }
        if (ret != -1)
        {
            return 0;
        }

        if(g_corefeatures.global_ipv6  == ENABLED)
        {
            if(IN6_IS_ADDR_V4MAPPED(pRMCPSession->LANRMCPPkt.IPHdr.Srcv6Addr))
            {
                /* The last bytes of IP6 contains IP4 address */
                _fmemcpy(IPAddr, &pRMCPSession->LANRMCPPkt.IPHdr.Srcv6Addr[IP6_ADDR_LEN - IP_ADDR_LEN], sizeof (struct in_addr));
            }
            else
            {
                /*
                 *  IPV6 address so it will be filled with 0xff. 
                 */
                memset(IPAddr, 0xFF, sizeof (struct in_addr));
            }
        }
        else
        {
            _fmemcpy(IPAddr, pRMCPSession->LANRMCPPkt.IPHdr.Srcv4Addr, sizeof (struct in_addr));
        }

        /* This structure values are AMI specific SEL Record data */
        OEMSELRec.ID = 0x00;
        OEMSELRec.Type = 0xc1;
        OEMSELRec.TimeStamp = 0x00;
        memcpy(OEMSELRec.MftrID, MfgID, sizeof(MfgID));
        OEMSELRec.OEMData[0] = EvtType;
        OEMSELRec.OEMData[1] = pRMCPSession->UserId;
        memcpy (&OEMSELRec.OEMData[2], IPAddr, IP_ADDR_LEN);

        OS_THREAD_MUTEX_ACQUIRE(&pBMCInfo->SELMutex, WAIT_INFINITE);
        /*we are not posting login/logout audit logs to PEF*/
        reslen = LockedAddSELEntry((INT8U *)&OEMSELRec, sizeof(SELOEM1Record_T), (INT8U *)&AddSelRes, TRUE,POST_ONLY_SEL, BMCInst);
        /* Enable Reservation ID which was cancelled by this event */
        pBMCInfo->SELConfig.RsrvIDCancelled = FALSE;
        OS_THREAD_MUTEX_RELEASE(&pBMCInfo->SELMutex);
        return ( (sizeof(INT8U) == reslen) ? -1 : 0 );  //reslen is only Completion code size if error
    }
    return 0;
}
#endif

#endif /*#if IPMI20_SUPPORT*/
