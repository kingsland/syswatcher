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
 *****************************************************************
 *
 * MsgHndlr.c
 * Message Handler.
 *
 * Author: Govind Kothandapani <govindk@ami.com>
 *
 *****************************************************************/
//#define ICC_DEBUG /* garden */
#define _DEBUG_
#define UNIMPLEMENTED_AS_FUNC
#include <defs.h>
#include "Types.h"
#include "OSPort.h"
#include "Debug.h"
#include "Support.h"
#include "Message.h"
#include "MsgHndlr.h"
#include "IPMI_Main.h"
#include "IPMI_RMCP.h"
#include "IPMI_AMI.h"
#include "IPMIDefs.h"
#include "SharedMem.h"
//#include "AMI.h"
#include "App.h"
//#include "Bridge.h"
//#include "Chassis.h"
//#include "Storage.h"
//#include "SensorEvent.h"
#include "DeviceConfig.h"
//#include "ChassisDevice.h"
//#include "ChassisCtrl.h"
//#include "WDT.h"
//#include "SDR.h"
//#include "SEL.h"
//#include "FRU.h"
//#include "Sensor.h"
//#include "SensorMonitor.h"
//#include "FFConfig.h"
//#include "NVRAccess.h"
#include "Platform.h"
#include "PDKCmds.h"
//#include "ipmi_hal.h"
//#include "ipmi_int.h"
//#include "AMIDevice.h"
//#include "PendTask.h"
//#include "PEF.h"
//#include "SensorAPI.h"
//#include "IPMI_KCS.h"
#include "LANConfig.h"
#include "IPMIConf.h"
//#include "IPMBIfc.h"
/*added for the execution of the "Power Restore Policy"	*/
#include "PMConfig.h"
//#include "GroupExtn.h"
//#include "DCMDevice.h"
//#include "PDKAccess.h"
#include "GUID.h"
//#include "IfcSupport.h"
//#include "Badpasswd.h"
#include <dlfcn.h>
//#include "PDKCmdsAccess.h"
#include "featuredef.h"
#include <sys/prctl.h>
//#include "SSIAccess.h"
#include "featuredef.h"
//#include "PDKBridgeMsg.h"
#include "DeviceConfig.h"


#define PWR_ALWAYS_OFF             0x00
#define PWR_RESTORED               0x01
#define PWR_ALWAYS_ON              0x02
#define PWR_NO_CHANGE              0x03
#define PREV_POWER_STATE        0x01
#define MAX_STR_LENGTH             128

#ifdef CONFIGURABLE_SESSION_TIME_OUT
#define SESSION_FILE "/conf/time.txt"
#define CONF_SESSION readSessionTimeout( )

int readSessionTimeout(void)
{
    int fd;
    int ret;
    _FAR_  BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    char SessionTimeVal[255];
    int TimeOut;
    fd = open(SESSION_FILE, O_RDONLY);
    if(fd == -1)
    {
        return pBMCInfo->IpmiConfig.SessionTimeOut;
    }
    ret = read(fd, &SessionTimeVal, 255);
    if(ret == -1)
    {
        close(fd);
        return pBMCInfo->IpmiConfig.SessionTimeOut;
    }
    close(fd);
    TimeOut = atoi(SessionTimeVal);
    return TimeOut;
}
#endif

/*--------------------------------------------------------------------
 * Global Variables
 *--------------------------------------------------------------------*/


_FAR_ HQueue_T      hRcvMsg_Q;

_FAR_ char g_RcvMsgQ [3][RCVMSGQ_LENGTH] = {
    RCV_MSG_Q_01,
    RCV_MSG_Q_10,
    RCV_MSG_Q_11
};

/*-----------------------------------------------------------------------------
 * Function Prototypes
 *-----------------------------------------------------------------------------*/
//static void     ProcessTimerReq (int BMCInst);
static void     ProcessIPMIReq  (_NEAR_ MsgPkt_T* pReq, _NEAR_ MsgPkt_T* pRes, int BMCInst);
static int      ValidateMsgHdr  (_NEAR_ MsgPkt_T* pReq,int BMCInst);
static int      CheckSequenceNo (_NEAR_ MsgPkt_T* pReq);
static int      CheckPrivilege  (INT8U ReqPrivilege, INT8U CmdPrivilege);
static void     PendingBridgeResTimerTask   (int BMCInst);
static void     PendingSeqNoTimerTask       (int BMCInst);

/*--------------------------------------------------------------------
 * Module Variables
 *--------------------------------------------------------------------*/
DisableMsgFilterTbl_T m_DisableMsgFilterTbl [] =
{
    /* NET_FUN                                      Command */
    /*----------------- BMC Device and Messaging Commands ------------------*/
    { NETFN_APP,                    CMD_GET_CH_AUTH_CAP},
    { NETFN_APP,                    CMD_GET_CH_AUTH_CAP},
    { NETFN_APP,                    CMD_GET_SESSION_CHALLENGE},
    { NETFN_APP,                    CMD_ACTIVATE_SESSION},
    { NETFN_APP,                    CMD_SET_SESSION_PRIV_LEVEL},
    { NETFN_APP,                    CMD_CLOSE_SESSION},
    { NETFN_APP,                    CMD_GET_SESSION_INFO},
    /*------------------------ IPMI 2.0 specific Commands ------------------*/
    { NETFN_APP,                    CMD_ACTIVATE_PAYLOAD},
    { NETFN_APP,                    CMD_DEACTIVATE_PAYLOAD},
    { NETFN_APP,                    CMD_GET_PAYLD_ACT_STATUS},
    { NETFN_APP,                    CMD_GET_PAYLD_INST_INFO},
    { NETFN_APP,                    CMD_SET_USR_PAYLOAD_ACCESS},
    { NETFN_APP,                    CMD_GET_USR_PAYLOAD_ACCESS},
    { NETFN_APP,                    CMD_GET_CH_PAYLOAD_SUPPORT},
    { NETFN_APP,                    CMD_GET_CH_PAYLOAD_VER},
    { NETFN_APP,                    CMD_GET_CH_OEM_PAYLOAD_INFO},
    { NETFN_APP,                    CMD_SUS_RES_PAYLOAD_ENCRYPT},
    /*------------------------ IPMI 2.0 SOL Commands ------------------*/
    { NETFN_TRANSPORT,              CMD_GET_SOL_CONFIGURATION},
    { NETFN_TRANSPORT,              CMD_SET_SOL_CONFIGURATION},

    { NETFN_UNKNOWN,                0}, /* Indicate the end of array */
};

INT8U ExtNetFnMap[MAX_NUM_BMC][MAX_NETFN]={{0,0}};

MsgHndlrTbl_T m_MsgHndlrTbl [15] =
{
    { NETFN_APP,                    g_App_CmdHndlr                  },
    { NETFN_OEM,                    g_System_CmdHndlr               },
#if 0
    { NETFN_CHASSIS,                g_Chassis_CmdHndlr              },
    { NETFN_BRIDGE,                 g_Bridge_CmdHndlr               },
    { NETFN_SENSOR,                 g_SensorEvent_CmdHndlr          },
    { NETFN_STORAGE,                g_Storage_CmdHndlr              },
    { NETFN_TRANSPORT,              g_Config_CmdHndlr               },
    { NETFN_AMI,                    (CmdHndlrMap_T*)g_AMI_CmdHndlr  },
#endif
};


GroupExtnMsgHndlrTbl_T m_GroupExtnMsgHndlrTbl [10] =
{

};


TimerTaskTbl_T    m_TimerTaskTbl [20] =
{

    //    { 1,    PEFTimerTask	},
    //    { 1,    PETAckTimerTask },
    //    { 1,    PEFStartDlyTimerTask },
    { 1,    SessionTimerTask },
    { 1,    PendingBridgeResTimerTask },
    { 1,    PendingSeqNoTimerTask },
    //    { 1,    FlashTimerTask },

#if FRB_SUPPORT == 1
    { 1,    FRB3TimerTask   },
#endif /* #if FRB_SUPPORT == 1 */

#if SERIAL_MODEM_CONNECTION_ACTIVITY  != UNIMPLEMENTED
    //    { 2,    SerialModemPingTask },
#endif /* SERIAL_MODEM_CONNECTION_ACTIVITY */

    //   { 1,    MonitorPassword },
    { 1,    UDSSessionTimerTask },

};

INT8U GET_CH_AUTH_CAP_RES[] = {0x0, 0x1, 0x37, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x68};
INT8U GET_SESSION_CHALLENGE_RES[] = 
{0x0, 0xfb, 0x2d, 0xb5, 0xff, 0x54, 0xdf, 0xe1, 0xbf, 0x56, 0x47, 0x87, 0x88, 0xea, 0x7b, 0xa1, 0x54, 0x37, 0x5b, 0x79, 0xe3, 0xfc};
INT8U ACTIVATE_SESSION_RES[] = 
{0x0, 0x2, 0xfb, 0x2d, 0xb5, 0xff, 0x67, 0x45, 0x8b, 0x6b, 0x4, 0x16 };
INT8U SET_SESSION_PRIV_LEVEL_RES[] = 
{0x0, 0x4, 0x91 };

_FAR_  PendingBridgedResTbl_T	m_PendingBridgedResTbl[MAX_PENDING_BRIDGE_RES];
_FAR_  PendingSeqNoTbl_T	m_PendingSeqNoTbl[16][MAX_PENDING_SEQ_NO];
_FAR_  static INT8U		m_LastPendingSeqNo;
#if 0
/**
 * @fn CreateSystemGuid
 * @brief Creates System Guid
 * @param SystemGuid Pointer where the generated System GUID will be stored
 * @return Returns ZERO on success
 */
int CreateSystemGuid(INT8U *SystemGUID)
{
    IPMI_GUID_T GUID;
    CreateGUID(&GUID, (INT8U *)SYS_GUID_FILE);
    _fmemcpy (SystemGUID, (INT8U*)&GUID,16);
    return 0;
}

/**
 * @fn CreateDeviceGuid
 * @brief Creates Device Guid
 * @param DevGUID Pointer where the generated Device GUID will be stored
 * @return Returns ZERO on success
 */
int CreateDeviceGuid(INT8U *DevGUID)
{
    IPMI_GUID_T GUID;
    CreateGUID(&GUID,(INT8U *) DEV_GUID_FILE);
    /* Initialize guid with the Device GUID */
    _fmemcpy (DevGUID, (INT8U*) &GUID,16);
    return 0;
}
#endif

/**
 *@fn InitMsgHndlr
 *@brief Initializes the need things for MsgHndlr
 *@return none
 */
void InitMsgHndlr (void* pArg)
{
    int *inst = (int*) pArg;
    int BMCInst = *inst,Ret = 0;
    char keyInstance[MAX_STR_LENGTH];
    char SensorFileName[MAX_SEN_NAME_SIZE];
    struct stat buf;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    memset(keyInstance,0,MAX_STR_LENGTH);
    memset(SensorFileName,0,MAX_SEN_NAME_SIZE);

    SENSORTHRESH_FILE(BMCInst,SensorFileName);

    /* Update the Task Id & Message handler table information */
    BMC_GET_SHARED_MEM(BMCInst)->MsgHndlrID        = GET_TASK_ID ();

#ifdef CONFIGURABLE_SESSION_TIME_OUT
    BMC_GET_SHARED_MEM(BMCInst)->uSessionTimeout = CONF_SESSION;
#endif

    /* Chassis Ctrl Queue */
    OS_CREATE_Q (CHASSIS_CTRL_Q, Q_SIZE,BMCInst);
    OS_GET_Q(CHASSIS_CTRL_Q,O_RDWR,BMCInst);
    /* Receive message queue*/
    OS_CREATE_Q(EVT_MSG_Q, Q_SIZE,BMCInst);
    OS_GET_Q(EVT_MSG_Q,O_RDWR,BMCInst);
    /* PDK API queue        */
    OS_CREATE_Q (PDK_API_Q, Q_SIZE,BMCInst);
    OS_GET_Q(PDK_API_Q,O_RDWR,BMCInst);

    OS_CREATE_Q (SM_HNDLR_Q, Q_SIZE,BMCInst);
    OS_GET_Q(SM_HNDLR_Q,O_RDWR,BMCInst);
    /* Pending Task Queue */
    OS_CREATE_Q (PEND_TASK_Q, Q_SIZE,BMCInst);
    OS_GET_Q(PEND_TASK_Q,O_RDWR,BMCInst);
    /* PEF Task Queue */
    OS_CREATE_Q (PEF_TASK_Q, Q_SIZE,BMCInst);
    OS_GET_Q(PEF_TASK_Q,O_RDWR,BMCInst);
    OS_CREATE_Q (PEF_RES_Q, Q_SIZE,BMCInst);
    OS_GET_Q(PEF_RES_Q,O_RDWR,BMCInst);

    /* Node Lan Cfg Ctrl Queue */
    OS_CREATE_Q (NODE_LAN_CFG_Q, Q_SIZE,BMCInst);
    OS_GET_Q(NODE_LAN_CFG_Q,O_RDWR,BMCInst);

    /* Node Sensor Monitor Cfg Ctrl Queue */
    OS_CREATE_Q (NODE_SM_CFG_Q, Q_SIZE,BMCInst);
    OS_GET_Q(NODE_SM_CFG_Q,O_RDWR,BMCInst);

    memcpy(pBMCInfo->MsgHndlrTbl, m_MsgHndlrTbl,sizeof(m_MsgHndlrTbl));

#if 0	
    /*Generating  System GUID*/
    if(g_PDKHandle[PDK_INITSYSTEMGUID] != NULL)
    {
        Ret = ((int(*)(INT8U *))g_PDKHandle[PDK_INITSYSTEMGUID]) ((_FAR_ INT8U*) BMC_GET_SHARED_MEM(BMCInst)->SystemGUID);
    }

    if(g_PDKHandle[PDK_INITSYSTEMGUID] == NULL || Ret == 1)
    {
        CreateSystemGuid((_FAR_ INT8U*) BMC_GET_SHARED_MEM(BMCInst)->SystemGUID);
    }

    /*Generating Device GUID*/
    if(g_PDKHandle[PDK_INITDEVICEGUID] != NULL)
    {
        Ret = ((int(*)(INT8U *))g_PDKHandle[PDK_INITDEVICEGUID]) ((_FAR_ INT8U*) BMC_GET_SHARED_MEM(BMCInst)->DeviceGUID);
    }

    if(g_PDKHandle[PDK_INITDEVICEGUID] == NULL || Ret == 1)
    {
        CreateDeviceGuid((_FAR_ INT8U*) BMC_GET_SHARED_MEM(BMCInst)->DeviceGUID);
    }

    /* Initilize HAL layer */
    //IPMI_HAL_INIT (BMCInst);

    /* Initilize SDR, SEL, FRU and Sensors */
#if SDR_DEVICE == 1
    InitSDR (BMCInst);
#endif

#if SEL_DEVICE == 1
    InitSEL (BMCInst);
#endif

#if SENSOR_DEVICE == 1
    InitSensor(BMCInst);
#endif

    /* We are collecting the FRU Locator SDR in InitSensor for FRU  Validation */
#if FRU_DEVICE == 1
    InitFRU (BMCInst);
#endif
#endif

    /*Removing the SensorThreshold File if it exists*/
    if(0 == stat(SensorFileName,&buf))
    {
        unlink(SensorFileName);
    }

    return;
}

/**
 *@fn PowerCheck
 *@brief  Checks the AC power on status
 and the Power good status
 */
bool PowerCheck(int BMCInst)
{
    bool m_Ispoweron = FALSE;
    bool m_getpsgood = TRUE;

#if 0
    if(g_PDKHandle[PDK_ISACPOWERON] != NULL)
    {
        m_Ispoweron = ((bool(*)(int))g_PDKHandle[PDK_ISACPOWERON]) (BMCInst);
    }

    if(g_PDKHandle[PDK_GETPSGOOD] != NULL)
    {
        m_getpsgood = ((bool(*)(int))g_PDKHandle[PDK_GETPSGOOD]) (BMCInst);
    }
#endif

    return (m_Ispoweron == TRUE) &&( m_getpsgood != TRUE);
}


/**
 *@fn MsgHndlr
 *@brief Message Handler Task
 *       Starting Main function of MsgHndlr task
 */
void *MsgHndlr(void *pArg)
{
    MsgPkt_T	Req;
    MsgPkt_T	Res;
    MsgPkt_T    MsgPkt;
    INT8U    i=0,ethindex=0;
    int *inst = (int*)pArg;
    int BMCInst = *inst;
    int RetVal = -1;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    void *lib_handle = NULL;
    void (*pInterrupttask) (void);
    BMCArg *pMsgHndlrArg=NULL;
    prctl(PR_SET_NAME,__FUNCTION__,0,0,0);

    pthread_mutex_init(&pBMCInfo->SMmutex,NULL);
    char keyInstance[MAX_STR_LENGTH];
    memset(keyInstance,0,MAX_STR_LENGTH);
    OS_CREATE_Q (MSG_HNDLR_Q, Q_SIZE,BMCInst);
    OS_GET_Q(MSG_HNDLR_Q,O_RDWR,BMCInst);

    pBMCInfo->CurTimerTick = 0x00;
    pBMCInfo->Msghndlr.CurChannel = 0x0F;
    pBMCInfo->Msghndlr.ChassisControl = 0xFF;
    pBMCInfo->Msghndlr.FireWallRequired = TRUE;
    pBMCInfo->HostOFFStopWDT = FALSE;

#if 0	
    if(g_PDKHandle[PDK_ONTASKSTARTUP] != NULL)
    {
        ((void(*)(INT8U,int))g_PDKHandle[PDK_ONTASKSTARTUP]) (MSG_HNDLR_TASK_ID,BMCInst);
    }

    /* PDK Pre initialization */
    if(g_PDKHandle[PDK_PREINIT] != NULL)
    {
        ((void(*)(int))g_PDKHandle[PDK_PREINIT]) (BMCInst);
    }
#endif

    /* Initializes the message handler */
    InitMsgHndlr ( (void*)&BMCInst);
    /* Setting ACPI Power State to ACPI S0/G0 */
    LOCK_BMC_SHARED_MEM(BMCInst);
    BMC_GET_SHARED_MEM (BMCInst)->m_ACPISysPwrState = IPMI_ACPI_S0;
    UNLOCK_BMC_SHARED_MEM (BMCInst);

    /*Acquire SM mutex and this will be release inside
      Sensor monitor thread after sensor initialization */
    //    OS_THREAD_MUTEX_ACQUIRE(&pBMCInfo->SMmutex,WAIT_INFINITE);
    /* Start Sensor Monitor task */

#if 0
    if(g_PDKHandle[PDK_SHMCORCLIENT] != NULL)
    {
        TWARN_1("log test");/*test*/
        if(((int(*)(void))g_PDKHandle[PDK_SHMCORCLIENT])())    /* I'm a ShMC */
        {
            /* If I'm a ShMC, I needn't to monitor the basic sensor in blade node, so I halt the Task.
               this sensorMonitorTask just for blade to monitor the basic sensor*/
            OS_CREATE_THREAD (NodeSensorMonitorTask,(void*)&BMCInst, &Err);
            OS_CREATE_THREAD (SensorMonitorTask,(void*)&BMCInst, &Err); /* garden */
        }
        else
        {
            OS_CREATE_THREAD (SensorMonitorTask,(void*)&BMCInst, &Err);
        }
    }
#endif

    /*Attempt to acquire mutex again will make this
      task sleep until Sensor monitor releases it.*/
    OS_THREAD_MUTEX_ACQUIRE(&pBMCInfo->SMmutex,WAIT_INFINITE);
    OS_THREAD_MUTEX_RELEASE(&pBMCInfo->SMmutex);

#if 0	
    // Spawn PendCmd task as a thread
    OS_CREATE_THREAD (PendCmdTask, (void*)&BMCInst, &Err);

    // Spawn Chassis task as a thread
    OS_CREATE_THREAD (ChassisTask, (void*)&BMCInst, &Err);

    // Spawn chassis timer task as a thread
    OS_CREATE_THREAD (ChassisTimer, (void*)&BMCInst, &Err);

    lib_handle = dlopen("/usr/local/lib/libipmiinterruptsensor.so", RTLD_NOW);
    if (lib_handle != NULL)
    {
        pInterrupttask = dlsym(lib_handle,"InterruptTask");
        if(pInterrupttask != NULL)
        {
            OS_THREAD_MUTEX_ACQUIRE(&pBMCInfo->SMmutex,WAIT_INFINITE);
            // Spawn Interrupt task as a thread
            OS_CREATE_THREAD ((void *)pInterrupttask,(void *)&BMCInst, &Err);
            OS_THREAD_MUTEX_ACQUIRE(&pBMCInfo->SMmutex,WAIT_INFINITE);
            OS_THREAD_MUTEX_RELEASE(&pBMCInfo->SMmutex);
        }
    }
    /*Spawn OBSM Task as thread*/
    if(g_corefeatures.cmm_support == ENABLED)
    {
        void (*pOBSMtask) (void);
        void *libcmm_handle = NULL;
        libcmm_handle = dlopen(CMM_LIB_PATH, RTLD_NOW);
        if (libcmm_handle != NULL)
        {
            pOBSMtask = dlsym(libcmm_handle,"OBSMTask");
            if(pOBSMtask != NULL)
            {
                OS_CREATE_Q (OBSM_TASK_Q, Q_SIZE,BMCInst);
                OS_GET_Q(OBSM_TASK_Q,O_RDWR,BMCInst);
                OS_CREATE_THREAD ((void *)pOBSMtask,(void *)&BMCInst, &Err);
            }
        }
    }

    // Spawn PEF task as a thread
    OS_CREATE_THREAD (PEFTask, (void *)&BMCInst, &Err);

    /* Creating WatchDog Timer Task */
    OS_CREATE_THREAD(WDTTimerTask,(void*)&BMCInst,&Err);

    if(g_corefeatures.dual_image_support == ENABLED)
    {
        DefaultSettingsForDualImageSupport();
    }

    if(g_corefeatures.ssi_support == ENABLED)
    {
        /* Start Operational State Machine */
        if (g_SSIHandle[SSICB_OPSTATEMACH] != NULL)
        {
            OS_CREATE_THREAD((void *)g_SSIHandle[SSICB_OPSTATEMACH], (void*)&BMCInst, &Err);
        }
    }
#endif

    for (i = 0; i < MAX_LAN_CHANNELS; i++)
    {
        if((pBMCInfo->LANConfig.LanIfcConfig[i].Enabled == TRUE)
                && (pBMCInfo->LANConfig.LanIfcConfig[i].Up_Status == LAN_IFC_UP))
        {
            UpdateArpStatus(i, BMC_GET_SHARED_MEM(BMCInst)->IsWDTRunning, BMCInst);

            pMsgHndlrArg = malloc(sizeof(BMCArg));
            pMsgHndlrArg->BMCInst = BMCInst;
            ethindex = i;
            pMsgHndlrArg->Len = strlen((char *)&ethindex);
            pMsgHndlrArg->Argument = malloc(pMsgHndlrArg->Len);

            memcpy(pMsgHndlrArg->Argument,(char *)&ethindex,pMsgHndlrArg->Len);

            /* Create a thread to Send Gratuitous ARP Packet */
            OS_CREATE_THREAD ((void *)GratuitousARPTask,(void *)pMsgHndlrArg, NULL);
            usleep(20);
        }
    }

    /* Create Pending Bridge table mutex */
    OS_THREAD_MUTEX_INIT(pBMCInfo->PendBridgeMutex, PTHREAD_MUTEX_RECURSIVE);

#if 0
    /* PDK Post initialization */
    if(g_PDKHandle[PDK_POSTINIT] != NULL)
    {
        ((void (*)(int)) g_PDKHandle[PDK_POSTINIT]) (BMCInst);
    }
#endif

    /* Enable Global Sensor Scanning
     * after all initialization */
    //    API_GlobalSensorScanningEnable (TRUE, BMCInst);

    IPMI_DBG_PRINT ("Message handler running\n");

    OS_THREAD_MUTEX_RELEASE(&pBMCInfo->MsgHndlrMutex);

    // apply the power restore policy only if it is AC-on and the system is not on.
    //if (PDK_IsACPowerOn() == TRUE && PDK_GetPSGood() != TRUE)
    if(PowerCheck(BMCInst))
    {
#if 0
        switch(pBMCInfo->ChassisConfig.PowerRestorePolicy)
        {
            case PWR_ALWAYS_ON:
                IPMI_WARNING("Power Restore Policy Succeded with always on  \n");
                // save the restart reason before the actual power on action
                pBMCInfo->ChassisConfig.SysRestartCause = RESTART_CAUSE_AUTO_ALWAYS_ON;
                OnSetRestartCause(pBMCInfo->ChassisConfig.SysRestartCause, TRUE,BMCInst);
                FlushIPMI((INT8U*)&pBMCInfo->ChassisConfig,(INT8U*)&pBMCInfo->ChassisConfig,pBMCInfo->IPMIConfLoc.ChassisConfigAddr,sizeof(ChassisConfig_T),BMCInst);
                Platform_HostPowerOn(BMCInst);
                break;
            case PWR_RESTORED:
                // turn on the payload if the previous power-on was S0.
                if ((pBMCInfo->ChassisConfig.ChassisPowerState.PowerState & PREV_POWER_STATE) == TRUE)
                {
                    // save the restart reason before the actual power on action
                    pBMCInfo->ChassisConfig.SysRestartCause = RESTART_CAUSE_AUTO_PREV_STATE;
                    OnSetRestartCause(pBMCInfo->ChassisConfig.SysRestartCause, TRUE,BMCInst);
                    FlushIPMI((INT8U*)&pBMCInfo->ChassisConfig,(INT8U*)&pBMCInfo->ChassisConfig,pBMCInfo->IPMIConfLoc.ChassisConfigAddr,sizeof(ChassisConfig_T),BMCInst);
                    Platform_HostPowerOn(BMCInst);
                }
                break;
            case PWR_ALWAYS_OFF:
                // it is from AC-On, the payload is already in off mode,
                // simply set the ACPI state to S5, in case of the previous
                // ACPI state was restored during the AC-On.
#if 0
                if(g_PDKHandle[PDK_GETPSGOOD] != NULL)
                {
                    if(((int(*)(int))g_PDKHandle[PDK_GETPSGOOD]) (BMCInst) != TRUE)
                    {
                        SetACPIState(IPMI_ACPI_S5,BMCInst);
                    }
                }
#endif
                break;
            default :
                break;
        }
#endif
    }

    /* zc test, now I turn on the Node Sensor Monitor task*/
    MsgPkt.Param    = 0x01;     // SM_TASK_ON   0x01;
    MsgPkt.Size     = 0;
    PostMsg (&MsgPkt, NODE_SM_CFG_Q, BMCInst);

    while (1)
    {
        if (0 != GetMsg (&Req, MSG_HNDLR_Q, WAIT_INFINITE, BMCInst))
        {
            IPMI_WARNING ("Error fetching messages from hMsgHndlr_Q\n");
            continue;
        }
        switch (Req.Param)
        {
            case PARAM_IFC  :
                OS_THREAD_MUTEX_ACQUIRE(&pBMCInfo->CmdHndlrMutex,WAIT_INFINITE);
                IPMI_DBG_PRINT_1( "IFC packet received on %s.\n", Req.SrcQ );
#if 0
                if((g_corefeatures.ipmi_res_timeout == ENABLED) && IsMsgTimedOut(&Req) )
                {
                    IPMI_WARNING ("Timeout in fetching message from hMsgHndlr_Q\n");
                    OS_THREAD_MUTEX_RELEASE(&pBMCInfo->CmdHndlrMutex);
                    continue;
                }

                Res.Param = NO_RESPONSE;
                Res.Size  = 0;
                /* Update PDK Library with Channel number and Privelleg */
                if(g_PDKHandle[PDK_SETCURCMDCHANNELINFO] != NULL)
                {
                    ((void(*)(INT8U,INT8U,int))g_PDKHandle[PDK_SETCURCMDCHANNELINFO]) (Req.Channel,Req.Privilege,BMCInst);
                }
                if (Req.Channel & 0xF0) { pBMCInfo->Msghndlr.FireWallRequired = FALSE; }
                else					{ pBMCInfo->Msghndlr.FireWallRequired = TRUE;  }
                Req.Channel &= 0x0f; /* Only 4 bits are used */
                IPMI_DBG_PRINT_1("req channel is %d\n",Req.Channel);

                /* PDK Hook to select LUN TASK */
                if(g_PDKHandle[PDK_LUNREQUEST] != NULL)
                {
                    RetVal = ( (int (*)(MsgPkt_T *,MsgPkt_T *, int) ) g_PDKHandle[PDK_LUNREQUEST]) ( &Req, &Res,BMCInst);
                }
#endif

                if(-1 == RetVal)
                {
                    if ((SYS_IFC_CHANNEL == Req.Channel) || (pBMCInfo->IpmiConfig.ICMBIfcSupport == 1 && pBMCInfo->ICMBCh== Req.Channel) ||
                            (pBMCInfo->IpmiConfig.SMBUSIfcSupport == 1 && pBMCInfo->SMBUSCh == Req.Channel) || (USB_CHANNEL == Req.Channel)||(strcmp((char *)Req.SrcQ,UDS_RES_Q) == 0))
                    {
                        Res.Param = NORMAL_RESPONSE;
                        ProcessIPMIReq (&Req, &Res, BMCInst);
                        if(pBMCInfo->IpmiConfig.UDSIfcSupport == 0x01 && (strcmp((char *)Req.SrcQ,UDS_RES_Q) == 0))
                        {
                            Res.Size = Res.Size+1;
                            SwapUDSIPMIMsg(&Req,&Res);
                            Res.Data[Res.Size] = 0;
                        }
                        if ( pBMCInfo->IpmiConfig.SYSIfcSupport == 0x01 && SYS_IFC_CHANNEL == Req.Channel)
                        {
                            /* Skip current request if no response from Command Handler */
                            if (!Res.Size && (NORMAL_RESPONSE == Res.Param) )
                            {
                                OS_THREAD_MUTEX_RELEASE(&pBMCInfo->CmdHndlrMutex);
                                continue;
                            }

                            Res.SessionID = Req.SessionID;
                        }
                    }
                    else if(0 == ValidateMsgHdr (&Req, BMCInst))
                    {
                        IPMI_DBG_PRINT("ValidateMsgHdr returned success\n");
                        Res.Param = NORMAL_RESPONSE;
                        /* Swap the header and copy in response */
                        SwapIPMIMsgHdr ((_NEAR_ IPMIMsgHdr_T*)Req.Data, (_NEAR_ IPMIMsgHdr_T*)Res.Data);
                        /* Process the request */
                        ProcessIPMIReq (&Req, &Res, BMCInst);
                        /* Skip current request if no response from Command Handler */
                        if ( (sizeof(IPMIMsgHdr_T) == Res.Size) && (NORMAL_RESPONSE == Res.Param) )
                        {
                            OS_THREAD_MUTEX_RELEASE(&pBMCInfo->CmdHndlrMutex);
                            continue;
                        }

                        /* Calculate Checksum 2 */
                        Res.Data[Res.Size] = CalculateCheckSum2 (Res.Data, Res.Size);
                        Res.Size++;
                    }
                }

                if((g_corefeatures.ipmi_res_timeout == ENABLED) && IsMsgTimedOut(&Req) )
                {
                    IPMI_WARNING ("Timeout before posting message to Interface Queue\n");
                    OS_THREAD_MUTEX_RELEASE(&pBMCInfo->CmdHndlrMutex);
                    continue;
                }

                if ('\0' != Req.SrcQ[0])
                {
                    if (0 != PostMsg (&Res,(INT8S *)Req.SrcQ,BMCInst))
                    {
                        IPMI_WARNING ("MsgHndlr.c : Unable to post to Interface Q\n");
                    }
                    IPMI_DBG_PRINT_2( "MsgHndlr: Response posted back to %s, action = %d.\n", Req.SrcQ, (int) Res.Param );
                }
                OS_THREAD_MUTEX_RELEASE(&pBMCInfo->CmdHndlrMutex);
                break;

            case PARAM_TIMER :
                //                ProcessTimerReq (BMCInst);
                if(pBMCInfo->Msghndlr.ChassisIdentifyTimeout !=0)
                {
                    pBMCInfo->Msghndlr.ChassisIdentifyTimeout--;
                }
                break;

            default :
                IPMI_WARNING ("Invalid request\n");
                break;
        }

        /* Check if there is a request from the command handlers */
        if (0 != pBMCInfo->Msghndlr.WarmReset)
        {
            //            Platform_WarmReset (BMCInst);
            pBMCInfo->Msghndlr.WarmReset = 0;
        }
        if (0 != pBMCInfo->Msghndlr.ColdReset) { /*Platform_ColdReset (BMCInst);*/ pBMCInfo->Msghndlr.ColdReset = 0; }
        if (0 != pBMCInfo->Msghndlr.BBlk)	  {	/*Platform_ColdReset (BMCInst);*/ pBMCInfo->Msghndlr.BBlk = 0;      }

        switch (pBMCInfo->Msghndlr.ChassisControl)
        {
            case CHASSIS_POWER_DOWN : /*Platform_HostPowerOff (BMCInst);*/	    break;
            case CHASSIS_POWER_UP	: /*Platform_HostPowerOn (BMCInst);*/		break;
            case CHASSIS_POWER_CYCLE: /*Platform_HostPowerCycle (BMCInst);*/	break;
            case CHASSIS_HARD_RESET : /*Platform_HostColdReset (BMCInst);*/	break;
            default : break;
        }
        pBMCInfo->Msghndlr.ChassisControl = 0xFF;

        if (pBMCInfo->Msghndlr.ChassisIdentify == TRUE)
        {
            /*Platform_Identify (pBMCInfo->Msghndlr.ChassisIdentifyTimeout, pBMCInfo->Msghndlr.ChassisIdentifyForce,BMCInst);*/
            pBMCInfo->Msghndlr.ChassisIdentify = FALSE;
        }
    }

    OS_TASK_RETURN;
}


/**
 *@fn ValidateMsgHdr
 *@brief Validates the Message header and keeps track of the messages that has been bridged
 *@param pReq Request packet of the command to be executed
 *@return Returns -1 in case of the response to the bridged message
 *            Returns 0 otherwise
 */
    static int
ValidateMsgHdr (_NEAR_ MsgPkt_T* pReq, int BMCInst)
{
    int	i, j;
    _NEAR_	IPMIMsgHdr_T*	pIPMIMsgReq = (_NEAR_ IPMIMsgHdr_T*)&pReq->Data;
    INT8U	KCSIfcNum;
    _FAR_   ChannelInfo_T*  pChannelInfo;
    char LANQueueName[MAX_STR_LENGTH],PrimaryIPMBQueueName[MAX_STR_LENGTH],SecondaryIPMBQueueName[MAX_STR_LENGTH],QueueName[MAX_STR_LENGTH];
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    memset(LANQueueName,0,sizeof(LANQueueName));
    memset(PrimaryIPMBQueueName,0,sizeof(PrimaryIPMBQueueName));
    memset(SecondaryIPMBQueueName,0,sizeof(SecondaryIPMBQueueName));
    memset(QueueName,0,sizeof(QueueName));

    IPMI_DBG_PRINT_1("LUN received is %d\n",(pReq->NetFnLUN & 0x03));

    /* Check for the request message LUN */
    switch (pReq->NetFnLUN & 0x03)
    {
        case BMC_LUN_00:
            /* if request return 0 */
            if (0 == (pReq->NetFnLUN & 0x04))
            {
                //if (pReq->Channel != LAN_RMCP_CHANNEL)
                //{
                //if( 0 != CheckSequenceNo ( pReq ) )	{ return -1;}
                //}
                return 0;
            }

            OS_THREAD_MUTEX_ACQUIRE(&pBMCInfo->PendBridgeMutex, WAIT_INFINITE);
            /* Check the sequence number in table */
            for (i=0; i < sizeof (m_PendingBridgedResTbl)/sizeof (m_PendingBridgedResTbl[0]); i++)
            {
                IPMI_DBG_PRINT_1( "MsgHndlr: Checking outside message with sequence number %d.\n", i );

                if (TRUE == m_PendingBridgedResTbl[i].Used)
                {
                    if ((pReq->Channel                  == m_PendingBridgedResTbl[i].ChannelNum)  &&
                            (NET_FN(pIPMIMsgReq->RqSeqLUN)  == m_PendingBridgedResTbl[i].SeqNum)      &&
                            (NET_FN(pIPMIMsgReq->NetFnLUN)  == NET_FN((m_PendingBridgedResTbl[i].ReqMsgHdr.NetFnLUN + 0x04)))  &&
                            (pIPMIMsgReq->Cmd               == m_PendingBridgedResTbl[i].ReqMsgHdr.Cmd)  &&
                            (pIPMIMsgReq->ReqAddr           == m_PendingBridgedResTbl[i].ReqMsgHdr.ResAddr))
                    {
                        IPMI_DBG_PRINT ("MsgHndlr: Match Found\n");

                        /* Change the requester's address, sequence number and LUN */
                        pIPMIMsgReq->ResAddr  = m_PendingBridgedResTbl[i].ReqMsgHdr.ReqAddr;
                        pIPMIMsgReq->RqSeqLUN = m_PendingBridgedResTbl[i].ReqMsgHdr.RqSeqLUN;

                        /* Calculate the Second CheckSum */
                        pReq->Data[ pReq->Size - 1 ] = CalculateCheckSum2( pReq->Data, pReq->Size - 1 );
                        /*
#188589: When external app sends request to BMC it can use it's own
SW ID as per IPMI spec. Currently  Intel DPCCLI uses 0x81 as SW ID which (ReqAddr)
Once the BMC populates this into pReq, we also need to recompute chk1(checksum).
If not DPCCLI would fail to read HSC/NM FW version because of chk1 failure
                         */
                        pReq->Data[2] = (~(pReq->Data[0] + pReq->Data[1])) +1; //2's complement checksum

                        pReq->Param = BRIDGING_REQUEST;

                        IPMI_DBG_PRINT ("MsgHndlr: Response\n");
                        IPMI_DBG_PRINT_BUF (pReq->Data, pReq->Size);
                        sprintf(LANQueueName,"%s%d",LAN_IFC_Q,BMCInst);
                        sprintf(PrimaryIPMBQueueName,"%s%d",IPMB_PRIMARY_IFC_Q,BMCInst);
                        sprintf(SecondaryIPMBQueueName,"%s%d",IPMB_SECONDARY_IFC_Q,BMCInst);
                        if (0 == strcmp ((char *)m_PendingBridgedResTbl[i].DestQ, LANQueueName))
                        {
                            if(g_corefeatures.send_msg_cmd_prefix == ENABLED)
                            {
                                memmove( &pReq->Data [sizeof (IPMIMsgHdr_T) + 1], &pReq->Data[0], pReq->Size ); 
                                /* Increase Size for Msg Hdr + Comp. Code + Whole Checksum */
                                pReq->Size += sizeof (IPMIMsgHdr_T) + 2;

                                _fmemcpy (pReq->Data, &m_PendingBridgedResTbl[i].ResMsgHdr, sizeof (IPMIMsgHdr_T));
                                pReq->Data[sizeof (IPMIMsgHdr_T)] = CC_NORMAL;

                                SwapIPMIMsgHdr (&m_PendingBridgedResTbl[i].ReqMsgHdr, (_NEAR_ IPMIMsgHdr_T*) &pReq->Data[sizeof (IPMIMsgHdr_T) + 1]);                        
                                pReq->Data [pReq->Size - 1] =  CalculateCheckSum2 (pReq->Data, pReq->Size - 1);

                            }

                            for (j=pReq->Size - 1; j>=0; j--)
                            {
                                pReq->Data[j+1] = pReq->Data[j];
                            }
                            pReq->Data[0] = m_PendingBridgedResTbl[i].SrcSessionHandle;
                            pReq->Size++;
                            pReq->Cmd = PAYLOAD_IPMI_MSG;
                            strcpy(QueueName,LAN_IFC_Q);
                        }
                        else if (pBMCInfo->IpmiConfig.PrimaryIPMBSupport == 1 && 0 == strcmp ((char *)m_PendingBridgedResTbl[i].DestQ,PrimaryIPMBQueueName))
                        {
                            for (j=pReq->Size - 1; j>=0; j--)
                            {
                                pReq->Data[j + sizeof (IPMIMsgHdr_T) + 1] = pReq->Data[j];
                            }
                            _fmemcpy (pReq->Data, &m_PendingBridgedResTbl[i].ResMsgHdr, sizeof (IPMIMsgHdr_T));
                            pReq->Data[sizeof (IPMIMsgHdr_T)] = CC_NORMAL;
                            pReq->Size++;
                            strcpy(QueueName,IPMB_PRIMARY_IFC_Q);
                        }
                        else if (pBMCInfo->IpmiConfig.SecondaryIPMBSupport == 1 && 0 == strcmp ((char *)m_PendingBridgedResTbl[i].DestQ,SecondaryIPMBQueueName))
                        {
                            for (j=pReq->Size - 1; j>=0; j--)
                            {
                                pReq->Data[j + sizeof (IPMIMsgHdr_T) + 1] = pReq->Data[j];
                            }
                            _fmemcpy (pReq->Data, &m_PendingBridgedResTbl[i].ResMsgHdr, sizeof (IPMIMsgHdr_T));
                            pReq->Data[sizeof (IPMIMsgHdr_T)] = CC_NORMAL;
                            pReq->Size++;
                            strcpy(QueueName,IPMB_SECONDARY_IFC_Q);
                        }
                        else
                        {
#if 0
                            /* PDK Hook to format Pending Bridge Response Packet for other destinations */
                            if(g_PDKHandle[PDK_FORMATBRIDGERESPKT] != NULL)
                            {
                                ( (void (*)(MsgPkt_T *, int, int) ) g_PDKHandle[PDK_FORMATBRIDGERESPKT]) ( pReq, i, BMCInst);
                            }	
#endif                    
                            sprintf (QueueName,"%s",m_PendingBridgedResTbl[i].DestQ);
                        }

                        /* Post the data to Destination Interface queue */
                        PostMsg (pReq,QueueName,BMCInst);

                        m_PendingBridgedResTbl[i].Used = FALSE;

                        IPMI_DBG_PRINT_1( "MsgHndlr:  cleared pending message index = %d.\n", i );
                        OS_THREAD_MUTEX_RELEASE(&pBMCInfo->PendBridgeMutex);
                        return -1;
                    }
                }
            }
            OS_THREAD_MUTEX_RELEASE(&pBMCInfo->PendBridgeMutex);
            return -1;

        case BMC_LUN_01:
        case BMC_LUN_10:
        case BMC_LUN_11:

#if 0
            if ( (pReq->NetFnLUN & 0x03) == BMC_LUN_11)
            {
                if(g_PDKCmdsHandle[PDKCMDS_GETLUN11MSGHNDLRMAP] != NULL)
                {
                    if (0 == ((int(*)(INT8U, INT8U, CmdHndlrMap_T**, int))g_PDKCmdsHandle[PDKCMDS_GETLUN11MSGHNDLRMAP])(NET_FN(pReq->NetFnLUN), pReq->Cmd, NULL, BMCInst) )
                    {
                        return 0;
                    }
                }
            }
#endif            
            if(pBMCInfo->IpmiConfig.KCS1IfcSupport == 1 ||pBMCInfo->IpmiConfig.KCS2IfcSupport == 1 ||pBMCInfo->IpmiConfig.KCS3IfcSuppport == 1)
            {
                pChannelInfo = getChannelInfo(pReq->Channel,BMCInst);
                if(NULL == pChannelInfo)
                {
                    IPMI_WARNING ("Unable to get Ch information of Channel: %d\n", pReq->Channel);
                    return -1;
                }

                if (0x0 == pChannelInfo->ReceiveMsgQ)
                {
                    IPMI_DBG_PRINT_1 ("The Channel(0x%x) has been Disabled for Receive message\n",
                            pReq->Channel);
                    return -1;
                } else 
                {
                    IPMI_DBG_PRINT_1 ("The Channel(0x%x) is Enabled for Receive message\n",
                            pReq->Channel);
                }

                KCSIfcNum = g_BMCInfo[BMCInst].Msghndlr.CurKCSIfcNum;
                IPMI_DBG_PRINT_1 ("MsgHndlr: Message from outside, posting to KCS%d RcvMsgQ\n", KCSIfcNum);
                /* Post to the Receive message queue */
                if (0 != PostMsgNonBlock (pReq, &g_RcvMsgQ[KCSIfcNum][0],BMCInst))
                {
                    IPMI_WARNING ("MsgHndlr.c : Unable to post g_hRcvMsg_Q[KCSIfcNum]\n");
                    return -1;
                }

                BMC_GET_SHARED_MEM(BMCInst)->NumRcvMsg[KCSIfcNum]++;
                /* Setting to denote that there is message in Receive Message Queue */
                BMC_GET_SHARED_MEM (BMCInst)->MsgFlags |= 0x01;

                IPMI_DBG_PRINT_1 ("MsgHndlr: Posted the msg to KCS%d g_hRcvMsg_Q\n", KCSIfcNum);

#if 0
                /* Set the SMS attention bit */
                SET_SMS_ATN (KCSIfcNum, BMCInst);
                if(g_corefeatures.kcs_obf_bit == ENABLED)
                {
                    SET_OBF (KCSIfcNum, BMCInst);
                }
#endif
                return -1;
            }

        default:
            return -1;
    }
}


/**
 *@fn ProcessIPMIReq
 *@brief Processes the requested IPMI command
 *@param pReq Request of the command
 *@param pRes Response for the requested command
 *@return none
 */
    static void
ProcessIPMIReq (_NEAR_ MsgPkt_T* pReq, _NEAR_ MsgPkt_T* pRes, int BMCInst)
{
    CmdHndlrMap_T *    pCmdHndlrMap;
    PDK_ChannelInfo_T  PDKChInfo;
    INT32U             HdrOffset = 0;
    INT8U	       ResLen;
    INT8U         CmdOverride = 1,IfcSupport =0;
    INT8 MsgHndlrMapGot=0;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    INT8U    Index;
    _FAR_    ChannelInfo_T*        pChannelInfo=NULL;
    _FAR_    ChannelUserInfo_T*    pChUserInfo=NULL;
    _FAR_    SessionInfo_T*        pSessInfo=NULL;

    /* Set the Cmd and Net function in response packet */
    pRes->Cmd		= pReq->Cmd;
    pRes->NetFnLUN	= pReq->NetFnLUN & 0xFC;
    /* Normal IPMI Command response */
    pRes->Param = NORMAL_RESPONSE;

    IPMI_DBG_PRINT ("Processing IPMI Packet.\n");

    /* If Req is not from sysifc leave space for IPMI Msg Header */
    if(pReq->Channel != CH_NOT_USED)
    {
        if ((SYS_IFC_CHANNEL != pReq->Channel) && (pBMCInfo->ICMBCh != pReq->Channel) && (pBMCInfo->SMBUSCh != pReq->Channel) && (USB_CHANNEL != pReq->Channel)&&((strcmp((char *)pReq->SrcQ,UDS_RES_Q) != 0)))
        {
            HdrOffset = sizeof (IPMIMsgHdr_T);
            pReq->Size  = pReq->Size - HdrOffset - 1;
        }
        if(strcmp((char *)pReq->SrcQ,UDS_RES_Q) == 0)
        {
            HdrOffset = sizeof(IPMIUDSMsg_T);
            pReq->Size = pReq->Size -HdrOffset - 1;
        }
    }
    pRes->Size  = HdrOffset + sizeof (INT8U);

    PDKChInfo.ChannelNum = pReq->Channel;
    PDKChInfo.Privilege  = pReq->Privilege;

    if (pBMCInfo->IpmiConfig.PrimaryIPMBSupport == 0x01 && pReq->Channel == PRIMARY_IPMB_CHANNEL)
    {
        if ( 0 != CheckSequenceNo ( pReq ) )
        {
            pRes->Data[ HdrOffset ] = CC_CANNOT_EXEC_DUPL_REQ;
            IPMI_DBG_PRINT( "MsgHndlr.c : Duplicate Sequence number.\n" );
            return;
        }
    }

    IPMI_DBG_PRINT ("Processing PreProcessIPMI Cmd.\n");
    g_BMCInfo[BMCInst].Msghndlr.CurSessionID  = pReq->SessionID;

    /*get information abt this channel*/
    pChannelInfo = getChannelInfo ((pReq->Channel  & 0xF), BMCInst);
    /*get information abt this session*/
    pSessInfo = getSessionInfo (SESSION_ID_INFO, &g_BMCInfo[BMCInst].Msghndlr.CurSessionID, BMCInst);
    /* According IPMI spec, disable messaging function no need to control the session management command */
    if ((pSessInfo != NULL) && (pChannelInfo != NULL))
    {
        /*get information abt this channel of user*/
        pChUserInfo = getChUserIdInfo (pSessInfo->UserId, &Index, pChannelInfo->ChannelUserInfo, BMCInst);
        if (pChUserInfo != NULL)
        {
            if(pChUserInfo->IPMIMessaging == FALSE)
            {
                int i = 0;
                bool blCmdAllowed = FALSE;

                while (m_DisableMsgFilterTbl[i].NetFn != NETFN_UNKNOWN)
                {
                    if ((m_DisableMsgFilterTbl[i].NetFn == NET_FN (pReq->NetFnLUN)) &&
                            (m_DisableMsgFilterTbl[i].Command == pReq->Cmd))
                    {
                        /*Checking the special condition*/
                        if ((pReq->Cmd == CMD_GET_SOL_CONFIGURATION) || (pReq->Cmd == CMD_SET_SOL_CONFIGURATION))
                        {
                            if (pChUserInfo->ActivatingSOL)
                                blCmdAllowed = TRUE;
                        }
                        else
                            blCmdAllowed = TRUE;
                        break;
                    }
                    i++;
                }
                if (blCmdAllowed == FALSE)
                {
                    pRes->Data[ HdrOffset ] = CC_INV_CMD;
                    IPMI_DBG_PRINT( "MsgHndlr.c : IPMI messagging disabled\n" );
                    return;
                }
            }
        }
    }

#if 0
    /* PDK Module IPMI Command control function */
    if(g_PDKCmdsHandle[PDKCMDS_PDKPREPROCESSCMD] != NULL)
    {
        if ( -1 == ((int(*)(INT8U,INT8U,PDK_ChannelInfo_T*,INT8U*,INT8U,INT8U*,INT8U*,int))g_PDKCmdsHandle[PDKCMDS_PDKPREPROCESSCMD])
                (pReq->NetFnLUN,   pReq->Cmd,  &PDKChInfo, &pReq->Data[HdrOffset],  pReq->Size,
                 &pRes->Data[HdrOffset],  &ResLen ,BMCInst))
        {
            pRes->Size=ResLen;
            pRes->Size +=HdrOffset;
            IPMI_DBG_PRINT ("MsgHndlr.c : Command handled by PDK PreProcessCmd Module\n");
            return;
        }
    }

    if(g_PDKCmdsHandle[PDKCMDS_PDKISCOMMANDENABLED]  != NULL)
    {
        if(((int(*)(INT8U,INT8U*,INT8U,int))g_PDKCmdsHandle[PDKCMDS_PDKISCOMMANDENABLED])(NET_FN(pReq->NetFnLUN), &pReq->Data[HdrOffset], pReq->Cmd, BMCInst) != 0)
        {
            pRes->Data [HdrOffset] = CC_INV_CMD;
            //            IPMI_WARNING ("Invalid Net Function 0x%x or Invalid Command 0x%x\n",NET_FN(pReq->NetFnLUN), pReq->Cmd);/*garden rmv print*/
            return;
        }
    }
    if( (pReq->NetFnLUN & 0x03) == BMC_LUN_11)
    {
        if(g_PDKCmdsHandle[PDKCMDS_GETLUN11MSGHNDLRMAP] != NULL)
        {
            MsgHndlrMapGot = ((int(*)(INT8U, INT8U, CmdHndlrMap_T**, int))g_PDKCmdsHandle[PDKCMDS_GETLUN11MSGHNDLRMAP])(NET_FN(pReq->NetFnLUN), pReq->Cmd, &pCmdHndlrMap,BMCInst);
            if(MsgHndlrMapGot == 0)
            {
                CmdOverride = GetCmdHndlr(pReq,pRes,pCmdHndlrMap,HdrOffset,CmdOverride,&pCmdHndlrMap);
            }
        }
    }
    else if(g_PDKCmdsHandle[PDKCMDS_GETOEMMSGHNDLRMAP] != NULL)
    {
        MsgHndlrMapGot = ((int(*)(INT8U,CmdHndlrMap_T**,int))g_PDKCmdsHandle[PDKCMDS_GETOEMMSGHNDLRMAP])(NET_FN(pReq->NetFnLUN),&pCmdHndlrMap,BMCInst);
        if(MsgHndlrMapGot == 0)
        {
            CmdOverride = GetCmdHndlr(pReq,pRes,pCmdHndlrMap,HdrOffset,CmdOverride,&pCmdHndlrMap);
        }
    }
    else
    {
        CmdOverride = GetCmdHndlr(pReq,pRes,pCmdHndlrMap,HdrOffset,CmdOverride,&pCmdHndlrMap);
    }
#else
    CmdOverride = 0;
#endif

    if(CmdOverride == 0 || MsgHndlrMapGot == -1)
    {
        if (0 != GetMsgHndlrMap (NET_FN (pReq->NetFnLUN), &pCmdHndlrMap,BMCInst))
        {
            if(pBMCInfo->IpmiConfig.GrpExtnSupport == 1)
            {
                if (0 != GroupExtnGetMsgHndlrMap (NET_FN (pReq->NetFnLUN), pReq->Data [HdrOffset], &pCmdHndlrMap,BMCInst) )
                {
                    pRes->Data [HdrOffset] = CC_INV_CMD;
                    IPMI_WARNING ("MsgHndlr.c : Invalid Net Function 0x%x or Invalid Command 0x%x\n",NET_FN(pReq->NetFnLUN), pReq->Cmd);
                    return;
                }
            }
            else
            {
                pRes->Data [HdrOffset] = CC_INV_CMD;
                IPMI_WARNING ("MsgHndlr.c : Invalid Net Function 0x%x\n",NET_FN(pReq->NetFnLUN));
                return;
            }
        }
    }

    if(GetCmdHndlr(pReq,pRes,pCmdHndlrMap,HdrOffset,CmdOverride,&pCmdHndlrMap) == FALSE)
    {
        pRes->Data [HdrOffset] = CC_INV_CMD;
        return;
    }

#if 0
    GetIfcSupport(pCmdHndlrMap->IfcSupport,&IfcSupport,BMCInst);
    if(IfcSupport == 0xFF)
    {
        pRes->Data[HdrOffset] = CC_INV_CMD;
        return;
    }
#endif
#if 0
    // If the command was issued from PDK, do not check for firewall settings
    if (0 != strcmp ((char *)pReq->SrcQ, PDK_API_Q))
    {
        if (0 == CheckCmdCfg (pCmdHndlrMap, pReq->Channel, NET_FN (pReq->NetFnLUN), pReq->Cmd, BMCInst))
        {
            if ((g_BMCInfo[BMCInst].Msghndlr.FireWallRequired == FALSE) || (pReq->Channel == USB_CHANNEL)||(strcmp((char *)pReq->SrcQ,UDS_RES_Q) == 0))
            {
                IPMI_DBG_PRINT ("Firewall not applicable for this channel\n");
            }
            else
            {
                printf ( "The command is not supported/blocked for firewall\n");
                //IPMI_DBG_PRINT ( "The command is not supported/blocked for firewall\n");
                pRes->Data[HdrOffset] = CC_INSUFFIENT_PRIVILEGE;
                return;
            }
        }
    }
#endif
    /* Check for the request size */
    if (0xff != pCmdHndlrMap->ReqLen)
    {
        /* Check for invalid request size */
        if(pReq->Cmd!=CMD_GET_CH_AUTH_CAP && \
                pReq->Cmd!=CMD_GET_SESSION_CHALLENGE 
                /*&& pReq->Cmd!=CMD_ACTIVATE_SESSION \
                  && pReq->Cmd!=CMD_SET_SESSION_PRIV_LEVEL*/)
        {
            if (pCmdHndlrMap->ReqLen != pReq->Size)
            {
                IPMI_DBG_PRINT_BUF (pReq->Data, pReq->Size);
                pRes->Data [HdrOffset] = CC_REQ_INV_LEN;
                return;
            }
        }
    }


    /* Check for the privilege */
    if (0 != CheckPrivilege (pReq->Privilege, pCmdHndlrMap->Privilege))
    {
        pRes->Data [HdrOffset] 	= CC_INSUFFIENT_PRIVILEGE;
        IPMI_DBG_PRINT ("MsgHndlr.c : Insufficent privilage\n");
        return;
    }

    /**
     * Set the current privilege level & Channel ID & session ID. Could be used by the
     * Command handler.
     **/
    /* High BIT4 having loopback or Normal connection status*/

    g_BMCInfo[BMCInst].Msghndlr.CurChannel 	= pReq->Channel;/* Only lower four bits are required */	
    g_BMCInfo[BMCInst].Msghndlr.CurPrivLevel	= pReq->Privilege;
    g_BMCInfo[BMCInst].Msghndlr.CurSessionID  = pReq->SessionID;
    g_BMCInfo[BMCInst].Msghndlr.CurSessionType  = pReq->SessionType;

    /* Set current KCS Interface number based on source queue */
    if (pBMCInfo->IpmiConfig.SYSIfcSupport == 0x01 && SYS_IFC_CHANNEL == pReq->Channel)
    {
        if (strcmp ((char *)pReq->SrcQ, KCS1_RES_Q) == 0 )         { g_BMCInfo[BMCInst].Msghndlr.CurKCSIfcNum = 0;}
        else if (strcmp ((char *)pReq->SrcQ, KCS2_RES_Q) == 0 )    { g_BMCInfo[BMCInst].Msghndlr.CurKCSIfcNum = 1; }
        else if (strcmp ((char *)pReq->SrcQ, KCS3_RES_Q) == 0 )    { g_BMCInfo[BMCInst].Msghndlr.CurKCSIfcNum = 2; }
    }
    /* Invoke the command handler */
    if(ExtNetFnMap[BMCInst][NET_FN (pReq->NetFnLUN)]==1) /* if its AMI OEM command call extended command handler */
    {
        pRes->Size =
            ((ExCmdHndlrMap_T*)pCmdHndlrMap)->CmdHndlr (&pReq->Data [HdrOffset], pReq->Size, &pRes->Data [HdrOffset],BMCInst) + HdrOffset;
    }
    else
    {
        if(pReq->Cmd==CMD_GET_CH_AUTH_CAP) /*test*/
        {
            memcpy(&pRes->Data [HdrOffset], GET_CH_AUTH_CAP_RES, sizeof(GET_CH_AUTH_CAP_RES));
            pRes->Size = sizeof(GET_CH_AUTH_CAP_RES) + HdrOffset;
        }
        else if(pReq->Cmd==CMD_GET_SESSION_CHALLENGE)
        {
            memcpy(&pRes->Data [HdrOffset], GET_SESSION_CHALLENGE_RES, sizeof(GET_SESSION_CHALLENGE_RES));
            pRes->Size = sizeof(GET_SESSION_CHALLENGE_RES) + HdrOffset;
        }
        //       else if(pReq->Cmd==CMD_ACTIVATE_SESSION)
        //       {
        //           memcpy(&pRes->Data [HdrOffset], ACTIVATE_SESSION_RES, sizeof(ACTIVATE_SESSION_RES));
        //           pRes->Size = sizeof(ACTIVATE_SESSION_RES) + HdrOffset;
        //		   printf("%d: HdrOffset=%d sizeof=%d\n",__LINE__,HdrOffset,sizeof(ACTIVATE_SESSION_RES));
        //       }
        //       else if(pReq->Cmd==CMD_SET_SESSION_PRIV_LEVEL)
        //       {
        //           memcpy(&pRes->Data [HdrOffset], SET_SESSION_PRIV_LEVEL_RES, sizeof(SET_SESSION_PRIV_LEVEL_RES));
        //           pRes->Size = sizeof(SET_SESSION_PRIV_LEVEL_RES) + HdrOffset;
        //		   printf("%d: HdrOffset=%d sizeof=%d\n",__LINE__,HdrOffset,sizeof(SET_SESSION_PRIV_LEVEL_RES));
        //       }

        else
        {
            pRes->Size =
                pCmdHndlrMap->CmdHndlr (&pReq->Data [HdrOffset], pReq->Size, &pRes->Data [HdrOffset], BMCInst) + HdrOffset;
        }
    }

    IPMI_DBG_PRINT ("MsgHndlr.c : Processed command\n");
    if( (CMD_SEND_MSG == pReq->Cmd) && (NETFN_APP == pReq->NetFnLUN >> 2))
    {
        int i, Offset = 0;

        if ((0 == pRes->Size) &&
                ((pBMCInfo->IpmiConfig.PrimaryIPMBSupport == 0x01  && PRIMARY_IPMB_CHANNEL == pRes->Channel) || (pBMCInfo->IpmiConfig.SecondaryIPMBSupport == 0x01 && pBMCInfo->SecondaryIPMBCh == pRes->Channel)) )
        {
            pRes->Param = NO_RESPONSE;
            Offset = HdrOffset + 2;
        }
        else if (HdrOffset == pRes->Size)
        {
            Offset = HdrOffset + 1;
        }

        for (i=0; i < sizeof (m_PendingBridgedResTbl)/sizeof (m_PendingBridgedResTbl[0]); i++)
        {
            if (TRUE == m_PendingBridgedResTbl[i].Used)
            {
                if (0 == _fmemcmp (&m_PendingBridgedResTbl[i].ReqMsgHdr, &pReq->Data[Offset], sizeof (IPMIMsgHdr_T)))
                {
                    _fmemcpy (&m_PendingBridgedResTbl[i].ResMsgHdr, pRes->Data, sizeof (IPMIMsgHdr_T));
                }
            }
        }
    }

    if (m_PendingSeqNoTbl[pReq->Channel][m_LastPendingSeqNo].Used)
    {
        m_PendingSeqNoTbl[pReq->Channel][m_LastPendingSeqNo].Used = FALSE;
    }

#if 0
    /* PDK Module Post IPMI Command control function */
    if(g_PDKCmdsHandle[PDKCMDS_PDKPOSTPROCESSCMD] != NULL)
    {
        if ( -1 == ((int(*)(INT8U,INT8U,PDK_ChannelInfo_T*,INT8U*,INT8U,INT8U*,INT8U*,int))g_PDKCmdsHandle[PDKCMDS_PDKPOSTPROCESSCMD])
                (pReq->NetFnLUN,   pReq->Cmd,  &PDKChInfo,&pReq->Data[HdrOffset],  pReq->Size,
                 &pRes->Data[HdrOffset],  (INT8U*)&pRes->Size ,BMCInst))
        {
            pRes->Size += HdrOffset;
            IPMI_DBG_PRINT ("MsgHndlr.c : Command handled by PDK PostProcessCmd Module\n");
            return;
        }
    }
#endif

    return;
}
#if 0
/**
 *@fn ProcessTimerReq
 *@brief Executes Timer task function for every one second
 *@return none
 */
    static void
ProcessTimerReq (int BMCInst)
{
    //_FAR_ static INT32U	CurTimerTick = 0;
    int i;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    void *lib_pdkhandle = NULL;
    void (*pPDKTimertask) (int);

    pBMCInfo->CurTimerTick++;
    for (i = 0; i < pBMCInfo->TimerTaskTblSize; i++)
    {
        if (0 == (pBMCInfo->CurTimerTick % pBMCInfo->TimerTaskTbl [i].NumSecs))
        {
            pBMCInfo->TimerTaskTbl [i].TimerFn (BMCInst);
        }
    }

    lib_pdkhandle = dlopen("/usr/local/lib/libipmipdk.so", RTLD_NOW);
    if (lib_pdkhandle != NULL)
    {
        pPDKTimertask = dlsym(lib_pdkhandle,"PDK_TimerTask");
        if(pPDKTimertask == NULL)
        {
            dlclose(lib_pdkhandle);   
            return;
        }
        pPDKTimertask(BMCInst);
    }
    dlclose(lib_pdkhandle);
    return;
}
#endif
/**
 *@fn CheckPrivilege
 *@brief Checks the privilege of the requested command
 *@param ReqPrivilege Privilege of the requested command
 *@param CmdPrivilege Privilege of the requested command in CmdHndlr map
 *@return Returns 0 on success
 *            Returns -1 on success
 */
    static int
CheckPrivilege (INT8U ReqPrivilege, INT8U CmdPrivilege)
{
    if (ReqPrivilege >= CmdPrivilege) { return 0; }
    return -1;
}

/**
 *@fn CheckSequenceNo
 *@brief Checks Sequence no validation for IPMB
 *@param pReq -Request for the command
 *@return Returns 0 on success
 *            Returns -1 on success
 */
    static int
CheckSequenceNo (_NEAR_ MsgPkt_T* pReq)
{
    INT8U i,j=0 ;
    INT8U SeqNum = ((_NEAR_ IPMIMsgHdr_T*)pReq->Data)->RqSeqLUN ;

    /* Check for duplicate request	*/
    for (i=0; i < sizeof (m_PendingSeqNoTbl[0])/sizeof (m_PendingSeqNoTbl[0][0]); i++)
    {
        if ((m_PendingSeqNoTbl[pReq->Channel][0].Used	== TRUE		) &&
                (m_PendingSeqNoTbl[pReq->Channel][0].SeqNum == SeqNum	) &&
                (m_PendingSeqNoTbl[pReq->Channel][0].NetFn	== pReq->NetFnLUN ) &&
                (m_PendingSeqNoTbl[pReq->Channel][0].Cmd	== pReq->Cmd	) )
        {
            IPMI_WARNING ("MsgHndlr.c : Duplicate Sequence Number received \n");
            return -1;
        }
    }

    /* Add the request to Sequence Number Table	*/
    for (i=0; i < sizeof (m_PendingSeqNoTbl[0])/sizeof (m_PendingSeqNoTbl[0][0]); i++)
    {
        /* Search for free element; if no free element use the oldest one	*/
        if (FALSE == m_PendingSeqNoTbl[pReq->Channel][i].Used )		{ j=i;	break;	}

        if(m_PendingSeqNoTbl[pReq->Channel][i].TimeOut < m_PendingSeqNoTbl[pReq->Channel][j].TimeOut )
        {
            j=i;
        }
    }

    m_PendingSeqNoTbl[pReq->Channel][j].Used	=	TRUE;
    m_PendingSeqNoTbl[pReq->Channel][j].TimeOut	=	SEQ_NO_EXPIRATION_TIME;
    m_PendingSeqNoTbl[pReq->Channel][j].SeqNum	=	SeqNum;
    m_PendingSeqNoTbl[pReq->Channel][j].NetFn	=	pReq->NetFnLUN;
    m_PendingSeqNoTbl[pReq->Channel][j].Cmd		=	pReq->Cmd;
    m_LastPendingSeqNo = j;

    return 0;
}

/**
 *@fn CalculateChecksum2
 *@brief Calculates the checksum
 *@param Pkt Pointer to the data for the checksum to be calculated
 *@param Len Size of data for checksum calculation
 *@return Returns the checksum value
 */
    extern INT32U
CalculateCheckSum2 (_FAR_ INT8U* Pkt, INT32U Len)
{
    INT8U	Sum;
    INT32U	i;

    /* Get Checksum 2 */
    Sum = 0;
    for (i = 3; i < Len; i++)
    {
        Sum += Pkt [i];
    }
    return (INT8U)(0xFF & (0x100 - Sum));
}

/**
 *@fn SwapIPMIMsgHdr
 *@brief Swaps the header and copies into response
 *@param pIPMIMsgReq Header of the Request
 *@param pIPMIMsgRes Header of the response
 *@return none
 */
    void
SwapIPMIMsgHdr (_NEAR_ IPMIMsgHdr_T* pIPMIMsgReq, _NEAR_ IPMIMsgHdr_T* pIPMIMsgRes)
{
    pIPMIMsgRes->ResAddr   = pIPMIMsgReq->ReqAddr;
    pIPMIMsgRes->NetFnLUN  = (pIPMIMsgReq->NetFnLUN & 0xFC) + 0x04;
    pIPMIMsgRes->NetFnLUN |= pIPMIMsgReq->RqSeqLUN & 0x03;

    /* Calculate the Checksum for above two bytes */
    pIPMIMsgRes->ChkSum   = (~(pIPMIMsgRes->ResAddr + pIPMIMsgRes->NetFnLUN) + 1);

    pIPMIMsgRes->ReqAddr  = pIPMIMsgReq->ResAddr;

    pIPMIMsgRes->RqSeqLUN = (pIPMIMsgReq->RqSeqLUN & 0xFC);
    pIPMIMsgRes->RqSeqLUN |= (pIPMIMsgReq->NetFnLUN & 0x03);

    pIPMIMsgRes->Cmd = pIPMIMsgReq->Cmd;

    return;
}

/**
 *@fn UnImplementedFunc
 *@brief Executes if the requested command in unimplemented
 *@param pReq Request for the command
 *@param ReqLen Request Length of the command
 *@param pRes Response for the command
 *@return Returns the size of the response
 */
    int
UnImplementedFunc (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,int BMCInst)
{
    *pRes = CC_INV_CMD;
    return sizeof (*pRes);
}

/**
 *@fn PendingBridgeResTimerTask
 *@brief Sends the timeout message to response queue
 *          if the message does not turn out within send message timeout
 *@return none
 */
    static void
PendingBridgeResTimerTask (int BMCInst)
{
    INT8U	i;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    char LANQueueName[MAX_STR_LENGTH],PrimaryIPMBQueueName[MAX_STR_LENGTH],SecondaryIPMBQueueName[MAX_STR_LENGTH],QueueName[MAX_STR_LENGTH];

    memset(LANQueueName,0,sizeof(LANQueueName));
    memset(PrimaryIPMBQueueName,0,sizeof(PrimaryIPMBQueueName));
    memset(SecondaryIPMBQueueName,0,sizeof(SecondaryIPMBQueueName));
    memset(QueueName,0,sizeof(QueueName));

    /* Check for any pending responses */
    for (i = 0; i < sizeof (m_PendingBridgedResTbl)/sizeof (m_PendingBridgedResTbl[0]); i++)
    {
        if (TRUE == m_PendingBridgedResTbl[i].Used)
        {
            m_PendingBridgedResTbl[i].TimeOut--;
            if (0 == m_PendingBridgedResTbl[i].TimeOut)
            {
                MsgPkt_T		Timeout;
                IPMIMsgHdr_T*   pIPMIMsgHdr  = (_NEAR_ IPMIMsgHdr_T*) Timeout.Data;

                /* Fill the response packet */
                SwapIPMIMsgHdr (&m_PendingBridgedResTbl[i].ReqMsgHdr, pIPMIMsgHdr);

                sprintf(LANQueueName,"%s%d",LAN_IFC_Q,BMCInst);
                sprintf(PrimaryIPMBQueueName,"%s%d",IPMB_PRIMARY_IFC_Q,BMCInst);
                sprintf(SecondaryIPMBQueueName,"%s%d",IPMB_SECONDARY_IFC_Q,BMCInst);

                if(m_PendingBridgedResTbl[i].ChannelNum == pBMCInfo->PrimaryIPMBCh)
                {
                    pIPMIMsgHdr->ReqAddr = pBMCInfo->IpmiConfig.PrimaryIPMBAddr;
                }
                else if(m_PendingBridgedResTbl[i].ChannelNum == pBMCInfo->SecondaryIPMBCh)
                {
                    pIPMIMsgHdr->ReqAddr = pBMCInfo->IpmiConfig.SecondaryIPMBAddr;
                }
                else
                {
                    pIPMIMsgHdr->ReqAddr = pBMCInfo->IpmiConfig.BMCSlaveAddr;
                }

                Timeout.Data [sizeof(IPMIMsgHdr_T)] = CC_TIMEOUT;

                Timeout.Size = sizeof (IPMIMsgHdr_T) + 1 + 1; // IPMI Header + Completion Code + Second Checksum

                /* Calculate the Second CheckSum */
                Timeout.Data[Timeout.Size - 1] = CalculateCheckSum2 (Timeout.Data, Timeout.Size-1);

                Timeout.Param = BRIDGING_REQUEST;

                if (0 == strcmp ((char *)m_PendingBridgedResTbl[i].DestQ, LANQueueName))
                {
                    int j;
                    for (j = Timeout.Size - 1; j >= 0; --j)
                    {
                        Timeout.Data [j+1] = Timeout.Data [j];
                    }

                    Timeout.Data[0] = m_PendingBridgedResTbl[i].SrcSessionHandle;
                    Timeout.Size++;
                    Timeout.Cmd = PAYLOAD_IPMI_MSG;
                    strcpy(QueueName, LAN_IFC_Q);
                }
                else if (pBMCInfo->IpmiConfig.PrimaryIPMBSupport == 1 && 0 == strcmp ((char *)m_PendingBridgedResTbl[i].DestQ, PrimaryIPMBQueueName))
                {
                    int j;
                    for (j = Timeout.Size - 1; j >= 0; --j)
                    {
                        Timeout.Data [j + sizeof (IPMIMsgHdr_T) + 1] = Timeout.Data [j];
                    }
                    _fmemcpy (Timeout.Data, &m_PendingBridgedResTbl[i].ResMsgHdr, sizeof (IPMIMsgHdr_T));
                    Timeout.Data[sizeof (IPMIMsgHdr_T)] = CC_NORMAL;
                    Timeout.Size++;
                    strcpy(QueueName, IPMB_PRIMARY_IFC_Q);
                }
                else if (pBMCInfo->IpmiConfig.SecondaryIPMBSupport == 1 && 0 == strcmp ((char *)m_PendingBridgedResTbl[i].DestQ, SecondaryIPMBQueueName))
                {
                    int j;
                    for (j = Timeout.Size - 1; j >= 0; --j)
                    {
                        Timeout.Data [j + sizeof (IPMIMsgHdr_T) + 1] = Timeout.Data [j];
                    }
                    _fmemcpy (Timeout.Data, &m_PendingBridgedResTbl[i].ResMsgHdr, sizeof (IPMIMsgHdr_T));
                    Timeout.Data[sizeof (IPMIMsgHdr_T)] = CC_NORMAL;
                    Timeout.Size++;
                    strcpy(QueueName, IPMB_SECONDARY_IFC_Q);
                }
                else 
                {
                    int j;
                    for (j = Timeout.Size - 1; j >= 0; --j)
                    {
                        Timeout.Data [j + sizeof (IPMIMsgHdr_T) + 1] = Timeout.Data [j];
                    }
                    _fmemcpy (Timeout.Data, &m_PendingBridgedResTbl[i].ResMsgHdr, sizeof (IPMIMsgHdr_T));
                    Timeout.Data[sizeof (IPMIMsgHdr_T)] = CC_TIMEOUT;
                    sprintf (QueueName,"%s",m_PendingBridgedResTbl[i].DestQ);
                }

                /* Post the data to Destination Interface queue */
                PostMsg (&Timeout,QueueName,BMCInst);

                m_PendingBridgedResTbl[i].Used = FALSE;
                IPMI_DBG_PRINT_1( "MsgHndlr: clean pending index = %d.\n", i );
            }
        }
    }
}

/**
 *@fn PendingSeqNoTimerTask
 *@brief Timertask which helpful in IPMB sequence number validation
 *@return none
 */
    static void
PendingSeqNoTimerTask (int BMCInst)
{
    INT8U	i,j;

    /* Check for any Sequence Number expiraied */
    for (i = 0; i < sizeof (m_PendingSeqNoTbl)/sizeof (m_PendingSeqNoTbl[0]); i++)
    {
        for (j = 0; j < sizeof (m_PendingSeqNoTbl[0])/sizeof (m_PendingSeqNoTbl[0][0]); j++)
        {
            if (TRUE == m_PendingSeqNoTbl[i][j].Used)
            {
                m_PendingSeqNoTbl[i][j].TimeOut--;
                if (0 == m_PendingSeqNoTbl[i][j].TimeOut)
                {
                    m_PendingSeqNoTbl[i][j].Used = FALSE;
                }
            }
        }
    }
}

/**
 *@fn GetMsgHndlrMap
 *@brief Gets the exact command Handler by comparing NetFn
 *@param Netfn -NetFunction of the Cmd to execute
 *@param pCmdHndlrMap Pointer to the Command Handler
 *@return Returns 0 on success
 *            Returns -1 on failure
 */
    int
GetMsgHndlrMap (INT8U NetFn, _FAR_ CmdHndlrMap_T ** pCmdHndlrMap,int BMCInst)
{
    int i;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    /* Get the command handler corresponding to the net function */
    for (i = 0; i <  BMC_GET_SHARED_MEM(BMCInst)->MsgHndlrTblSize ; i++)
    {
        if (pBMCInfo->MsgHndlrTbl [i].NetFn == NetFn) { break; }
    }

    /* Check if we have not found our net function */
    if (i ==  BMC_GET_SHARED_MEM(BMCInst)->MsgHndlrTblSize )
    {
        return -1;
    }

    /* Get the handler corresponding to the command */
    *pCmdHndlrMap = (CmdHndlrMap_T*)pBMCInfo->MsgHndlrTbl [i].CmdHndlrMap;
    return 0;
}

/**
 *@fn GetCmdHndlr
 *@brief Picks up the exact command to execute by comparing Cmd no.
 *@param pReq Request buffer for the command
 *@param pRes Response buffer for the command
 *@param pCmdHndlrMap
 *@param HdrOffset
 *@param CmdOverride
 *@param CmdHndlr
 *@return Returns TRUE on success
 *            Returns FALSE on failure
 */
int GetCmdHndlr(MsgPkt_T* pReq,MsgPkt_T* pRes,CmdHndlrMap_T* pCmdHndlrMap,
        INT32U HdrOffset,INT8U CmdOverride,CmdHndlrMap_T** CmdHndrl )
{
    while (1)
    {
        /**
         * If we reached the end of the Command Handler map - invalid command
         **/
        if(pReq->Cmd==CMD_GET_CH_AUTH_CAP || \
                pReq->Cmd==CMD_GET_SESSION_CHALLENGE 
                /*|| pReq->Cmd==CMD_ACTIVATE_SESSION \
                  || pReq->Cmd==CMD_SET_SESSION_PRIV_LEVEL*/) return TRUE;

            if (0 == pCmdHndlrMap->CmdHndlr)
            {
                if(CmdOverride == FALSE)
                {
                    pRes->Data [HdrOffset] = CC_INV_CMD;
                    IPMI_WARNING( "MsgHndlr.c : Invalid Command %d\n", pReq->Cmd );
                }
                return FALSE;
            }

        if (pCmdHndlrMap->Cmd == pReq->Cmd)
        {
            /* Check if command has been implemented */
            if ((pCmdHndlr_T)UNIMPLEMENTED == pCmdHndlrMap->CmdHndlr)
            {
                if(CmdOverride == FALSE)
                {
                    pRes->Data [HdrOffset] = CC_INV_CMD;
                    IPMI_WARNING ("MsgHndlr.c : Command is not implemented\n");
                }
                return FALSE;
            }
            else
            {
                break;
            }
        }

        pCmdHndlrMap++;

    }
    *CmdHndrl = pCmdHndlrMap;
    return TRUE;
}

/**
 *@fn GroupExtnGetMsgHndlrMap
 *@brief Gets the exact command Handler by comparing NetFn
 *@param Netfn -NetFunction of the Cmd to execute
 *@GroupExtnCode - Group Extension code
 *@param pCmdHndlrMap Pointer to the Command Handler
 *@return Returns 0 on success
 *            Returns -1 on failure
 */
    int
GroupExtnGetMsgHndlrMap (INT8U NetFn, INT8U GroupExtnCode, CmdHndlrMap_T ** pCmdHndlrMap,int BMCInst)
{
    int i;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];


    /* Get the command handler corresponding to the net function */
    for (i = 0; i < sizeof (pBMCInfo->GroupExtnMsgHndlrTbl) / sizeof (pBMCInfo->GroupExtnMsgHndlrTbl [0]); i++)
    {
        if ((pBMCInfo->GroupExtnMsgHndlrTbl [i].NetFn == NetFn) && (pBMCInfo->GroupExtnMsgHndlrTbl [i].GroupExtnCode == GroupExtnCode)) { break; }
    }

    /* Check if we have not found our net function */
    if (i == sizeof (pBMCInfo->GroupExtnMsgHndlrTbl) / sizeof (pBMCInfo->GroupExtnMsgHndlrTbl [0]))
    {
        return -1;
    }

    /* Get the handler corresponding to the command */
    *pCmdHndlrMap = (CmdHndlrMap_T*)pBMCInfo->GroupExtnMsgHndlrTbl [i].CmdHndlrMap;
    return 0;
}

/**
 * @fn GetIfcSupport
 * @brief This function checks the support of Interface before
 *            Interface specific commands are executed
 * @param IfcSupt - Interface support variable to be verified
 * @param IfcSupport - Gives the Interface presence support
 * @return Returns ZERO
 */
int GetIfcSupport(INT16U IfcSupt,INT8U *IfcSupport,int BMCInst)
{
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    switch(IfcSupt)
    {
        case LAN_IFC_SUP:
            if(pBMCInfo->IpmiConfig.LANIfcSupport == IFCENABLED)
            {
                *IfcSupport = 1;
            }
            else
            {
                *IfcSupport = 0xFF;
            }
            break;
        case SOL_IFC_SUP:
            if(pBMCInfo->IpmiConfig.SOLIfcSupport == IFCENABLED && pBMCInfo->IpmiConfig.LANIfcSupport == IFCENABLED)
            {
                *IfcSupport = 1;
            }
            else
            {
                *IfcSupport = 0xFF;
            }
            break;
        case SERIAL_IFC_SUP:
            if(pBMCInfo->IpmiConfig.SerialIfcSupport == IFCENABLED)
            {
                *IfcSupport = 1;
            }
            else
            {
                *IfcSupport = 0xFF;
            }
            break;
        default:
            *IfcSupport = 0x1;
    }
    return 0;
}

/**
 * *@fn Swap UDSIPMIMsg
 * *@brief Swaps the header and copies into response
 * *@param pIPMIMsgReq Header of the Request
 * *@param pIPMIMsgRes Header of the response
 * *@return none
 * */
    void
SwapUDSIPMIMsg (_NEAR_ MsgPkt_T* pIPMIMsgReq, _NEAR_ MsgPkt_T* pIPMIMsgRes)
{
    IPMIUDSMsg_T *pIPMIUDSMsgRes = (IPMIUDSMsg_T *)&pIPMIMsgRes->Data[0];
    IPMIUDSMsg_T *pIPMIUDSMsgReq = (IPMIUDSMsg_T *)&pIPMIMsgReq->Data[0];

    pIPMIMsgRes->NetFnLUN = pIPMIMsgReq->NetFnLUN;
    pIPMIMsgRes->Privilege = pIPMIMsgReq->Privilege;
    pIPMIMsgRes->Cmd = pIPMIMsgReq->Cmd;
    pIPMIMsgRes->Channel = pIPMIMsgReq->Channel;
    pIPMIMsgRes->SessionID   = pIPMIMsgReq->SessionID;
    pIPMIMsgRes->Socket = pIPMIMsgReq->Socket;

    pIPMIUDSMsgRes->NetFnLUN = pIPMIMsgReq->NetFnLUN;
    pIPMIUDSMsgRes->Privilege = pIPMIMsgReq->Privilege;
    pIPMIUDSMsgRes->Cmd = pIPMIMsgReq->Cmd;
    pIPMIUDSMsgRes->ChannelNum = pIPMIMsgReq->Channel;
    pIPMIUDSMsgRes->SessionID = pIPMIMsgReq->SessionID;
    pIPMIUDSMsgRes->AuthFlag = pIPMIUDSMsgReq->AuthFlag;
    pIPMIUDSMsgRes->IPMIMsgLen = pIPMIMsgRes->Size;

    strcpy( (char *)pIPMIUDSMsgRes->UserName, (char *)pIPMIUDSMsgReq->UserName);
    _fmemcpy(pIPMIUDSMsgRes->IPAddr, pIPMIUDSMsgReq->IPAddr, sizeof (struct in6_addr));  

    return;
}



INT8U GetIPMBSeqNumber( int BMCInst ) 
{
    int i=0;
    INT8U ReqSeqNum=0;
    _FAR_  BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    while(TRUE)
    {
        ReqSeqNum = pBMCInfo->SendMsgSeqNum = (pBMCInfo->SendMsgSeqNum + 1) & 0x3F;

        for (i = 0; i < sizeof (m_PendingBridgedResTbl)/sizeof (m_PendingBridgedResTbl[0]); i++)
        {
            if( (TRUE == m_PendingBridgedResTbl[i].Used) && (ReqSeqNum == m_PendingBridgedResTbl[i].SeqNum) )
                continue;
        }
        break;
    }

    return ReqSeqNum;

}


void RespondSendMessage ( MsgPkt_T* pReq, INT8U Status, int BMCInst)
{
    int  i;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    MsgPkt_T   ResPkt;
    _NEAR_ IPMIMsgHdr_T*  pIPMIResHdr = (_NEAR_ IPMIMsgHdr_T*)ResPkt.Data;
    _NEAR_ IPMIMsgHdr_T*  pIPMIReqHdr = (_NEAR_ IPMIMsgHdr_T*)pReq->Data;
    char LANQueueName[MAX_STR_LENGTH],PrimaryIPMBQueueName[MAX_STR_LENGTH],SecondaryIPMBQueueName[MAX_STR_LENGTH],QueueName[MAX_STR_LENGTH];

    memset(LANQueueName,0,sizeof(LANQueueName));
    memset(PrimaryIPMBQueueName,0,sizeof(PrimaryIPMBQueueName));
    memset(SecondaryIPMBQueueName,0,sizeof(SecondaryIPMBQueueName));
    memset(QueueName,0,sizeof(QueueName));

    sprintf(LANQueueName, "%s%d", LAN_IFC_Q, BMCInst);
    sprintf(PrimaryIPMBQueueName, "%s%d", IPMB_PRIMARY_IFC_Q, BMCInst);
    sprintf(SecondaryIPMBQueueName, "%s%d", IPMB_SECONDARY_IFC_Q, BMCInst);

    /* Check for any pending responses */
    for (i = 0; i < sizeof (m_PendingBridgedResTbl)/sizeof (m_PendingBridgedResTbl[0]); i++)
    {
        if ( (TRUE == m_PendingBridgedResTbl[i].Used) &&
                (pReq->Channel == m_PendingBridgedResTbl[i].ChannelNum) &&
                (NET_FN(pIPMIReqHdr->RqSeqLUN)  == m_PendingBridgedResTbl[i].SeqNum) &&
                (NET_FN(pIPMIReqHdr->NetFnLUN)  == NET_FN(m_PendingBridgedResTbl[i].ReqMsgHdr.NetFnLUN )) &&
                (pIPMIReqHdr->Cmd               == m_PendingBridgedResTbl[i].ReqMsgHdr.Cmd) &&
                (pIPMIReqHdr->ResAddr           == m_PendingBridgedResTbl[i].ReqMsgHdr.ResAddr)  )
        {

            /* Fill the response packet */
            if (ORIGIN_SENDMSG != m_PendingBridgedResTbl[i].OriginSrc)
            {
                SwapIPMIMsgHdr ( &m_PendingBridgedResTbl[i].ResMsgHdr, pIPMIResHdr );
            }
            else
            {
                _fmemcpy (pIPMIResHdr, &m_PendingBridgedResTbl[i].ResMsgHdr, sizeof (IPMIMsgHdr_T));
            }

            if (STATUS_OK == Status)
            {
                if ( (pBMCInfo->IpmiConfig.PrimaryIPMBSupport == 1 && 0 == strcmp ((char *)m_PendingBridgedResTbl[i].DestQ, PrimaryIPMBQueueName)) ||
                        (pBMCInfo->IpmiConfig.SecondaryIPMBSupport == 1 && 0 == strcmp ((char *)m_PendingBridgedResTbl[i].DestQ, SecondaryIPMBQueueName)))

                {
                    return;
                }

                ResPkt.Data [sizeof(IPMIMsgHdr_T)] = CC_NORMAL;
            }
            else if (STATUS_FAIL == Status)
            {
                ResPkt.Data [sizeof(IPMIMsgHdr_T)] = CC_NO_ACK_FROM_SLAVE;
                m_PendingBridgedResTbl[i].Used = FALSE;
            }
            else
            {
                ResPkt.Data [sizeof(IPMIMsgHdr_T)] = CC_UNSPECIFIED_ERR;
                m_PendingBridgedResTbl[i].Used = FALSE;
            }


            ResPkt.Size = sizeof (IPMIMsgHdr_T) + 1 + 1; // IPMI Header + Completion Code + Second Checksum

            /* Calculate the Second CheckSum */
            ResPkt.Data[ResPkt.Size - 1] = CalculateCheckSum2 (ResPkt.Data, ResPkt.Size-1);

            ResPkt.Param = BRIDGING_REQUEST;

            if (0 == strcmp ((char *)m_PendingBridgedResTbl[i].DestQ, LANQueueName))
            {
                //ResPkt.SessionID = m_PendingBridgedResTbl[i].ResMsgHdr.RqSeqLUN;
                strcpy (QueueName, LAN_RES_Q);
            }
            else 
            {
                sprintf (QueueName,"%s",m_PendingBridgedResTbl[i].DestQ);
            }

            /* Post the data to Destination Interface queue */
            PostMsg (&ResPkt, QueueName, BMCInst);

            return;
        }
    }
}


