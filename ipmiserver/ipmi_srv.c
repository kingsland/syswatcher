#include <IPMI.h>
#include <Support.h>
#include <Types.h>
#include <MsgHndlr.h>
#include <IPMI_App.h>
#include <IPMI_AppDevice.h>
#include <defs.h>
#include <metrics.h>
#include <SysMonitor.h>
#include <string.h>

int
GetSysState (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst);
int SetSysState (_NEAR_ INT8U* pReqData, INT8U ReqDataLen, _NEAR_ INT8U* pResData,_NEAR_ int BMCInst);
const CmdHndlrMap_T g_App_CmdHndlr [] =
{
    { CMD_ACTIVATE_SESSION, PRIV_NONE, ACTIVATE_SESSION, sizeof (ActivateSesReq_T), 0xAAAA ,0xFFFF},
    { CMD_SET_SESSION_PRIV_LEVEL, PRIV_USER, SET_SESSION_PRIV_LEVEL, sizeof (INT8U), 0xAAAA ,0xFFFF},
    { CMD_CLOSE_SESSION, PRIV_CALLBACK, CLOSE_SESSION, 0xFF, 0xAAAA ,0xFFFF},
    { CMD_GET_DEV_ID, PRIV_USER, GET_DEV_ID, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x0000, 0x0000}
};

const CmdHndlrMap_T g_System_CmdHndlr [] =
{
    { CMD_GET_SYS_STATE, PRIV_USER, GET_SYS_STATE, 0xFF, 0xAAAA, 0xFFFF},
    { CMD_SET_SYS_STATE, PRIV_USER, SET_SYS_STATE, 0xFF, 0xAAAA, 0xFFFF},
    { 0x00, 0x00, 0x00, 0x00, 0x0000, 0x0000}
};

int GetSysState (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst)
{
    GetNodeAgentInfoReq_T* pGetNodeAgentInfoReq = (GetNodeAgentInfoReq_T*)pReq;
    GetNodeAgentInfoRes_T* pGetNodeAgentInfoRes = (GetNodeAgentInfoRes_T*)pRes;
    memset(pGetNodeAgentInfoRes, 0, sizeof(GetNodeAgentInfoRes_T));
    pGetNodeAgentInfoRes->NodeAgentInfo = gNodeAgentInfo;
    return sizeof(GetNodeAgentInfoRes_T);
}

int SetSysState (_NEAR_ INT8U* pReqData, INT8U ReqDataLen, _NEAR_ INT8U* pResData,_NEAR_ int BMCInst)
{
    GetNodeAgentInfoReq_T* pGetNodeAgentInfoReq = (GetNodeAgentInfoReq_T*)pReqData;
    GetNodeAgentInfoRes_T* pGetNodeAgentInfoRes = (GetNodeAgentInfoRes_T*)pResData;
    
    memset(pGetNodeAgentInfoRes, 0, sizeof(GetNodeAgentInfoRes_T));
    printf("[ System Monitor Resource ]");
    print_buf(&gIpmiserverNodeInfo, sizeof(gIpmiserverNodeInfo));
    memcpy(&gIpmiserverNodeInfo, pReqData, 
                ReqDataLen>sizeof(gIpmiserverNodeInfo) ? sizeof(gIpmiserverNodeInfo) : ReqDataLen);
    pGetNodeAgentInfoRes->NodeAgentInfo = gIpmiserverNodeInfo;
    
    return sizeof(GetNodeAgentInfoRes_T);
}

