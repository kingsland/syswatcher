
#ifndef IPMI_H
#define IPMI_H

#define CMD_GET_SYS_STATE   0x10
#define CMD_SET_SYS_STATE   0x11
#include <Types.h>

typedef struct
{
    INT8U   Cmd;
    INT8U   ReturnCode;
    INT8U   BladeIndex;
    INT8U   BIOSVersion;
    INT8U   KernelVersion;
    INT8U   ServicePackVersion;
    INT32U   CPUModel;
    INT32U   RamSpeed;
    INT32U   RamSize;
    INT32U   SSDSize;
    INT8U   CPURate;
    INT8U   MemRate;
    INT32U   UsedSSDSize;
    INT8U   Eth0Rate;
    INT8U   Eth1Rate;
    INT8U   Eth2Rate;
    INT8U   Eth3Rate;
    INT8U   Eth4Rate;
    INT8U   Eth5Rate;
    INT8U   Eth6Rate;
    INT8U   Eth7Rate;
    INT8U   Reserved[28];
} PACKED  NodeAgentInfo_T;

typedef struct
{
    INT8U   Cmd; /*0x1*/
    INT8U   BladeIndex;

} PACKED  GetNodeAgentInfoReq_T;

typedef struct
{
    INT8U           CompletionCode;
    NodeAgentInfo_T NodeAgentInfo;

} PACKED  GetNodeAgentInfoRes_T;

extern int GetSysState (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst);
extern int GetDevId (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst);
extern int ActivateSession (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst);
extern int SetSessionPrivLevel (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst);
extern int CloseSession        (_NEAR_ INT8U* pReq, INT8U ReqLen, _NEAR_ INT8U* pRes,_NEAR_ int BMCInst);

#endif /*IPMI_H*/
