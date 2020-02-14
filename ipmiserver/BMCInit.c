
#include "IPMIConf.h"
#include "libipmi_struct.h"
#include "AppDevice.h"

void BMCInit (int BMCInst)
{
    int x=0;
    _FAR_   BMCInfo_t   *pBMCInfo = &g_BMCInfo[BMCInst];
    _FAR_	SessionTblInfo_T*	pSessionTblInfo = &pBMCInfo->SessionTblInfo;
    INT8U ChallengeString[] = {0x54, 0xdf, 0xe1, 0xbf, 0x56, 0x47, 0x87, 0x88, 0xea, 0x7b, 0xa1, 0x54, 0x37, 0x5b, 0x79, 0xe3};

    pBMCInfo->LANConfig.LanIfcConfig[0].Enabled = TRUE;
    pBMCInfo->LANConfig.LanIfcConfig[0].Up_Status = LAN_IFC_UP;
    pBMCInfo->LANConfig.LanIfcConfig[0].Chnum = 1;
    pBMCInfo->IpmiConfig.LANIfcSupport = 1;
    //strcpy(pBMCInfo->LANConfig.LanIfcConfig[0].ifname, "eno16777736");
    pBMCInfo->IpmiConfig.SessionTimeOut = 60;
    pBMCInfo->LANCfs[0].PrimaryRMCPPort = 623;

    pBMCInfo->ChConfig[0].ChType = 0x0;
    pBMCInfo->ChConfig[0].ChannelInfo.ChannelNumber = 0;
    pBMCInfo->ChConfig[1].ChType = 0xf;
    pBMCInfo->ChConfig[1].ChannelInfo.ChannelNumber = 15;
    pBMCInfo->ChConfig[2].ChType = 0x1;
    pBMCInfo->ChConfig[2].ChannelInfo.ChannelNumber = 1;
    pBMCInfo->ChConfig[2].ChannelInfo.AccessMode = 2;

    BMC_GET_SHARED_MEM(BMCInst)->MsgHndlrTblSize = 2;

    pBMCInfo->IpmiConfig.MaxSession = 36;
    pSessionTblInfo->SessionTbl = malloc(sizeof(SessionInfo_T)*2);
    pSessionTblInfo->SessionTbl[0].Used = 1;
    pSessionTblInfo->SessionTbl[0].Activated = 0;
    pSessionTblInfo->SessionTbl[0].SessionID = 0xffb52dfb;
    pSessionTblInfo->SessionTbl[0].AuthType = AUTH_TYPE_MD5;
    pSessionTblInfo->SessionTbl[0].UserId = 2;

    pBMCInfo->Msghndlr.CurChannel = 1;
    pBMCInfo->ChConfig[2].ChannelInfo.SessionLimit = 15;
    pBMCInfo->ChConfig[2].ChannelInfo.ActiveSession = 0;
    pBMCInfo->SERIALch = 255;
    pBMCInfo->ChConfig[2].ChannelInfo.AuthType[PRIV_LEVEL_ADMIN - 1] = 55;
    pBMCInfo->IpmiConfig.MaxUsers = 10;
    pBMCInfo->UserInfo[NVRH_USERCONFIG].MaxSession = 36;
    pBMCInfo->UserInfo[NVRH_USERCONFIG + 1].MaxSession = 36;

    pBMCInfo->IpmiConfig.MaxChUsers = 10;
    pBMCInfo->ChConfig[2].ChannelInfo.ChannelUserInfo[0].UserId = 1;
    pBMCInfo->ChConfig[2].ChannelInfo.ChannelUserInfo[0].ID = USER_ID;
    pBMCInfo->ChConfig[2].ChannelInfo.ChannelUserInfo[1].UserId = 2;
    pBMCInfo->ChConfig[2].ChannelInfo.ChannelUserInfo[1].ID = USER_ID;

    pBMCInfo->ChConfig[2].ChannelInfo.MaxPrivilege = PRIV_LEVEL_ADMIN;
    pBMCInfo->ChConfig[2].ChannelInfo.ChannelUserInfo[0].AccessLimit = PRIV_LEVEL_ADMIN;
    pBMCInfo->ChConfig[2].ChannelInfo.ChannelUserInfo[1].AccessLimit = PRIV_LEVEL_ADMIN;

    pBMCInfo->ChConfig[2].ChannelInfo.SessionSupport = MULTI_SESSION_CHANNEL;
    _fmemcpy (pSessionTblInfo->SessionTbl[0].ChallengeString, ChallengeString, CHALLENGE_STR_LEN);

    //SetSessionPrivLevel
    pBMCInfo->LANCfs[0].AuthTypeEnables.AuthTypeAdmin = 55;
    pBMCInfo->ChConfig[2].ChannelInfo.ChannelUserInfo[0].IPMIMessaging = 1;
    pBMCInfo->ChConfig[2].ChannelInfo.ChannelUserInfo[1].IPMIMessaging = 1;

}
