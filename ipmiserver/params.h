/*******************************************************************
 ********************************************************************
 ****                                                              **
 ****    (C)Copyright 2010-2011, American Megatrends Inc.          **
 ****                                                              **
 ****    All Rights Reserved.                                      **
 ****                                                              **
 ****    5555 , Oakbrook Pkwy, Norcross,                           **
 ****                                                              **
 ****    Georgia - 30093, USA. Phone-(770)-246-8600.               **
 ****                                                              **
 ********************************************************************
 ********************************************************************
 ********************************************************************
 **
 ** params.h
 ** 
 ** OEM specific parameter macros and OEM specific to AMI parameter 
 ** mapping table 
 **
 ** Author: senathipathyt@ami.com
 *******************************************************************/

#ifndef PARAMS_H
#define PARAMS_H

#include "LANConfig.h"

#define LAN_PARAMS_OEM_VLANIFC_ENABLE           192
#define LAN_PARAMS_OEM_SNMPV6_DEST_ADDR         193
#define LAN_PARAMS_OEM_ENABLE_SET_MAC           194

#define LAN_PARAMS_OEM_IPV6_ENABLE              195
#define LAN_PARAMS_OEM_IPV6_IP_ADDR_SOURCE      196
#define LAN_PARAMS_OEM_IPV6_IP_ADDR             197
#define LAN_PARAMS_OEM_IPV6_PREFIX_LENGTH       198
#define LAN_PARAMS_OEM_IPV6_LINK_ADDR           207
#define LAN_PARAMS_OEM_IPV6_LINK_ADDR_PREFIX    208

#define LAN_PARAMS_OEM_IPV6_GATEWAY_IP          199

/* Parameter No. 200 to 202 are specified as SSI OEM LAN Parameters */
#define LAN_PARAMS_OEM_2ND_PRI_ETH_MAC_ADDR     200
#define LAN_PARAMS_OEM_LINK_CTRL                201
#define LAN_PARAMS_OEM_CMM_IP_ADDR              202

#define LAN_PARAMS_OEM_IPV6_DNS_SETTINGS        203
#define LAN_PARAMS_OEM_NCSI_CONFIG_NUM          204
#define LAN_PARAMS_OEM_NCSI_SETTINGS            205
#define LAN_PARAMS_OEM_PHY_SETTINGS             206

/*-------------------------
 * OEM Lan Config Parameter Table
 *-------------------------*/
typedef struct
{
    INT8U       OEMLanParam;
    INT8U       AMILanParam;
    INT8U       IsOEMImp;    //Is this parameter function defined by OEM
}  OEMLanParamTbl_T;

OEMLanParamTbl_T  m_OEMLanParamTbl [] =
{
    { LAN_PARAMS_OEM_VLANIFC_ENABLE,       LAN_PARAMS_AMI_OEM_VLANIFC_ENABLE,        FALSE },
    { LAN_PARAMS_OEM_SNMPV6_DEST_ADDR,     LAN_PARAMS_AMI_OEM_SNMPV6_DEST_ADDR,      FALSE },
    { LAN_PARAMS_OEM_ENABLE_SET_MAC,       LAN_PARAMS_AMI_OEM_ENABLE_SET_MAC,        FALSE },
    { LAN_PARAMS_OEM_IPV6_ENABLE,          LAN_PARAMS_AMI_OEM_IPV6_ENABLE,           FALSE },
    { LAN_PARAMS_OEM_IPV6_IP_ADDR_SOURCE,  LAN_PARAMS_AMI_OEM_IPV6_IP_ADDR_SOURCE,   FALSE },
    { LAN_PARAMS_OEM_IPV6_IP_ADDR,         LAN_PARAMS_AMI_OEM_IPV6_IP_ADDR,          FALSE },
    { LAN_PARAMS_OEM_IPV6_LINK_ADDR,       LAN_PARAMS_AMI_OEM_IPV6_LINK_ADDR,        FALSE },
    { LAN_PARAMS_OEM_IPV6_PREFIX_LENGTH,   LAN_PARAMS_AMI_OEM_IPV6_PREFIX_LENGTH,    FALSE },
    { LAN_PARAMS_OEM_IPV6_LINK_ADDR_PREFIX,LAN_PARAMS_AMI_OEM_IPV6_LINK_ADDR_PREFIX, FALSE },
    { LAN_PARAMS_OEM_IPV6_GATEWAY_IP,      LAN_PARAMS_AMI_OEM_IPV6_GATEWAY_IP,       FALSE },
    { LAN_PARAMS_OEM_IPV6_DNS_SETTINGS,    LAN_PARAMS_AMI_OEM_IPV6_DNS_SETTINGS,     FALSE },
    { LAN_PARAMS_OEM_NCSI_CONFIG_NUM,      LAN_PARAMS_AMI_OEM_NCSI_CONFIG_NUM,       FALSE },
    { LAN_PARAMS_OEM_NCSI_SETTINGS,        LAN_PARAMS_AMI_OEM_NCSI_SETTINGS,         FALSE },
    { LAN_PARAMS_OEM_2ND_PRI_ETH_MAC_ADDR, LAN_PARAMS_SSI_OEM_2ND_PRI_ETH_MAC_ADDR,  FALSE },
    { LAN_PARAMS_OEM_LINK_CTRL,            LAN_PARAMS_SSI_OEM_LINK_CTRL,             FALSE },
    { LAN_PARAMS_OEM_CMM_IP_ADDR,          LAN_PARAMS_SSI_OEM_CMM_IP_ADDR,           FALSE },
    { LAN_PARAMS_OEM_PHY_SETTINGS,         LAN_PARAMS_AMI_OEM_PHY_SETTINGS,          FALSE }	
};

#define TOTAL_OEM_LAN_CONFIG_PARAMS  sizeof(m_OEMLanParamTbl)/sizeof(OEMLanParamTbl_T)

#endif
