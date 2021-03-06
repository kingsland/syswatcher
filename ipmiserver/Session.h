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
 * Session.h
 *
 *
 * Author: Govind Kothandapani <govindk@ami.com>
 * 		 : Rama Bisa <ramab@ami.com>
 *       : Basavaraj Astekar <basavaraja@ami.com>
 *       : Bakka Ravinder Reddy <bakkar@ami.com>
 *
 *****************************************************************/
#ifndef SESSION_H
#define SESSION_H
#include "Types.h"
#include "IPMI_LANIfc.h"
#include "IPMI_RMCP.h"
#include "PMConfig.h"
#include "IPMI_AppDevice.h"
#include "IPMIDefs.h"
#include "Message.h"

//#pragma pack( 1 )

/* Macros */
#define		SESSION_ID_INFO							1
#define		SESSION_HANDLE_INFO						2
#define		SESSION_INDEX_INFO						3
#define		SESSION_CHANNEL_INFO					4
#define		MGD_SYS_SESSION_ID_INFO					5
#define		SESSION_PAYLOAD_INFO					6
#define		SESSION_REMOTE_INFO					7
#define		MAX_INST_SUPPORTED						2
#define		SESSIONLESS_CHANNEL						0x00
#define		SINGLE_SESSION_CHANNEL					0x01
#define		MULTI_SESSION_CHANNEL					0x02
#define		SESSION_BASED_CHANNEL					0x03
#define		HASH_DATA_LENGTH						16
#define		NULL_USER_ID							1

#define UDS_SESSION_COUNT_INFO                      0x00
#define UDS_SESSION_ID_INFO                         0x01
#define UDS_SESSION_HANDLE_INFO                     0x02
#define UDS_SESSION_INDEX_INFO                      0x03
#define UDS_SOCKET_ID_INFO                          0x04

/**
 * NVRAM Handles
 **/
#define NVRH_USERCONFIG 0
#define NVRH_CHCONFIG     0
#define NVRH_SDR		0

/*** External Definitions ***/
#define SENSOR_TYPE_TEMP                0x01
#define SENSOR_TYPE_SECUIRTY_VIOLATION  0x06
#define SENSOR_TYPE_EVT_LOGGING         0x10
#define SENSOR_TYPE_SYSTEM_EVENT        0x12
#define SENSOR_TYPE_CRITICAL_INTERRUPT  0x13
#define SENSOR_TYPE_MODULE_BOARD        0x15
#define SENSOR_TYPE_WATCHDOG2           0x23
#define SENSOR_TYPE_OS_CRITICAL_STOP    0x20
#define SENSOR_TYPE_FRU_STATE           0x2C
#define SENSOR_TYPE_SERV_STATE          0xF0

#define FP_NMI_OFFSET                                  0x00
#define PW_VIOLATION_OFFSET                       0x05

#define WDT_SENSOR_NUMBER                             0xFE
#define SECUIRTY_VIOLATION_SENSOR_NUMBER    0xFD
#define NMI_SENSOR_NUMBER                              0xFC
#define SENSOR_SPECIFIC_READ_TYPE                  0x6F
#define PEF_ACTION_SEN_SPECIFIC_OFFSET 0xC4
#define OS_RUNTIME_CRITICAL_STOP 0x01

typedef struct
{
    INT16U  ID;
    INT8U   Type;
    INT32U  TimeStamp;

} PACKED  SELRecHdr_T;

/**
 * @struct SELEventRecord_T
 * @brief SEL Event Record
 **/
typedef struct
{
    /* SEL ENTRY RECORD HEADER */
    SELRecHdr_T hdr;

    /* RECORD BODY BYTES */
    INT8U   GenID [2];
    INT8U   EvMRev;
    INT8U   SensorType;
    INT8U   SensorNum;
    INT8U   EvtDirType;
    INT8U   EvtData1;
    INT8U   EvtData2;
    INT8U   EvtData3;

} PACKED  SELEventRecord_T;

/* UDS Session Table Info */
typedef struct
{
    INT32U SessionID;
    INT32U LoggedInTime;
    INT32U SessionTimeoutValue;
    SOCKET UDSSocket;
    INT8U LoggedInUsername[MAX_USER_NAME_LEN];
    INT8U LoggedInPassword[MAX_PASSWORD_LEN];
    INT8U LoggedInUserID;
    INT8U Activated;
    INT8U LoggedInSessionHandle;
    INT8U UDSChannelNum;
    INT8U LoggedInChannel;
    INT8U LoggedInPrivilege;
    INT8U AuthenticationMechanism;
}PACKED UDSSessionTbl_T;

typedef struct
{
    INT16U SessionCount;
    UDSSessionTbl_T *UDSSessionTbl;
}PACKED UDSSessionTblInfo_T;

/* SessPayloadInfo_T */
typedef struct
{
    INT8U    Type;
    INT8U    OemPldIANA [3];
    INT8U    OemPldID [2];
    INT8U    Version;
    INT16U    PortNum;
    INT16U    ActivatedInst;
    INT8U    AuxConfig [4];

} PACKED  SessPayloadInfo_T;


/*	structure to  keep track the information about a session.	*/
/* SessionInfo_T */
typedef struct
{
    INT8U			Used; /* Flag to indicate the slot used or not */
    INT32U			SessionID;
    INT8U			Activated;
    INT8U			Channel;
    INT8U			AuthType;
    INT8U			Privilege;
    INT8U			MaxPrivilege;
    INT32U			InboundSeq;
    INT32U			OutboundSeq;
    INT32U			TimeOutValue;
    INT8U			Password[MAX_PASSWORD_LEN];
    INT8U			UserId;
    INT8U			SessionHandle;
    INT8U			ChallengeString[CHALLENGE_STR_LEN];
    LANRMCPPkt_T	LANRMCPPkt;
    SOCKET			hSocket;
    BOOL			SerialModemMode;
    INT32U 			Time;
#if (IPMI20_SUPPORT == 1)
    INT8U				Lookup;
    INT32U				RemConSessionID;
    INT8U				RemConRandomNo [16];
    INT8U				MgdSysRandomNo [16];
    INT8U				AuthAlgorithm;
    INT8U				IntegrityAlgorithm;
    INT8U				ConfidentialityAlgorithm;
    INT8U				Key1 [MAX_HASH_KEY_SIZE];
    INT8U				Key2 [MAX_HASH_KEY_SIZE];
    SessPayloadInfo_T	SessPyldInfo [MAX_PYLDS_SUPPORT];
#endif
    INT32U                   InitialInboundSeq;
    INT32U                   InboundTrac[SIXTEEN_COUNT_WINDOW_LEN];
    INT16U                   InboundRecv;
    INT8U 			IsLoopBack;
    INT8U                    Linkstat;
    INT8U 			EventFlag;
    INT8U                       UserName[MAX_USER_NAME_LEN];
} SessionInfo_T;

/* SessionTblInfo_T */
typedef	struct
{
    INT16U			Count;
    SessionInfo_T	*SessionTbl;
} PACKED  SessionTblInfo_T;


typedef struct
{
    INT32U Time;
    INT8U  Index;
}PACKED  OldSessionInfo_T;

typedef enum
{
    KCS_IFC = 0x00,
    IPMB_IFC,
    LAN_IFC,
    SERIAL_IFC,
}IfcType_T;

//#pragma pack( )

/*--------------------------
 * Extern Declarations
 *--------------------------*/
extern		 void				SessionTimerTask (int BMCInst);
extern  ChannelInfo_T*  getChannelInfo (INT8U ch,int BMCInst);
extern _FAR_ ChannelUserInfo_T* getChUserIdInfo (INT8U userId, _NEAR_ INT8U *index, _FAR_ ChannelUserInfo_T* pChUserInfo, int BMCInst);
extern _FAR_ SessionInfo_T*		getSessionInfo (INT8U Arg, _FAR_ void *Session, int BMCInst);
extern _FAR_ ChannelUserInfo_T* getChUserPrivInfo (_NEAR_ char *userName, _NEAR_ INT8U Role, _NEAR_ INT8U* chIndex, _FAR_ ChannelUserInfo_T *pChUserInfo, int BMCInst);
extern _FAR_ ChannelUserInfo_T*	getChUserInfo (_NEAR_ char *userName, _NEAR_ INT8U* chIndex, _FAR_ ChannelUserInfo_T *pChUserInfo, int BMCInst);
extern _FAR_ UserInfo_T*		getUserIdInfo (INT8U userId, int BMCInst);
extern INT8U	CheckForDuplicateUsers (_NEAR_ INT8U* UserName, int BMCInst);
extern _FAR_ ChannelUserInfo_T*	AddChUser (_FAR_ ChannelUserInfo_T*	pChUserInfo, _NEAR_ INT8U*	Index,int BMCInst);
extern _FAR_ ChannelInfo_T* GetNVRChConfigs(ChannelInfo_T *pChannelInfo, int BMCInst);
extern _FAR_ ChannelUserInfo_T* GetNVRChUserConfigs(ChannelInfo_T *pChannelInfo,int BMCInst);
extern INT8U disableUser (INT8U UserId, int BMCInst);
extern INT8U GetNumOfActiveSessions (int BMCInst);
extern INT8U GetNumOfUsedSessions (int BMCInst);
extern void  DeleteSession (_FAR_ SessionInfo_T*	pSessionInfo,int BMCInst);
extern void  AddSession (_NEAR_ SessionInfo_T* pSessionInfo,int BMCInst);
extern INT8U CleanSession(int BMCInst);
extern _FAR_ INT16U	getPayloadActiveInst (INT8U PayloadType, int BMCInst);
extern _FAR_ INT32U	getPayloadInstInfo (INT8U PayloadType, INT16U PayloadInst,int BMCInst);
extern void	PasswordViolation (int BMCInst);
extern void UDSSessionTimerTask (int BMCInst);
extern int AddUDSSession (_NEAR_ UDSSessionTbl_T* pUDSSessionInfo, int BMCInst);
extern int DeleteUDSSession(_FAR_ UDSSessionTbl_T*  pUDSSessionInfo,int BMCInst);
extern _FAR_ UDSSessionTbl_T* GetUDSSessionInfo (INT8U Type,void *Data,int BMCInst);
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

extern void UpdateGetMsgTime (MsgPkt_T* pReq,IfcType_T IfcType, int BMCInst);

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

extern BOOL IsMsgTimedOut (MsgPkt_T* pReq);

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

extern BOOL IsResponseMatch (MsgPkt_T* pReq, MsgPkt_T* pRes);

/*-------------------------------------------------------
 * @fn FillIPMIResFailure
 * @brief Frames the Response packet when the time taken 
 *        to process an IPMI Command expires
 * 
 * @param pReq    : Request Message Packet
 *        pRes    : Response Message Packet
 *        BMCInst : BMC Instance Number
 * 
 * @return  none
 *------------------------------------------------------*/
extern void FillIPMIResFailure (_NEAR_ MsgPkt_T* pReq, _NEAR_ MsgPkt_T* pRes, int BMCInst);

#endif	/* SESSION_H */
