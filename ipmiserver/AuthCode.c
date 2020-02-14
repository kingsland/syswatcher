/******************************************************************
 ******************************************************************
 ***                                                             **
 ***    (C)Copyright 2008-2009, American Megatrends Inc.         **
 ***                                                             **
 ***    All Rights Reserved.                                     **
 ***                                                             **
 ***    5555 , Oakbrook Pkwy, Norcross,                          **
 ***                                                             **
 ***    Georgia - 30093, USA. Phone-(770)-246-8600.              **
 ***                                                             **
 ******************************************************************
 ******************************************************************
 ******************************************************************
 *
 * AuthCode.c
 * Authentication Code Caluculation
 *
 *  Author: Winston <winstonv@amiindia.co.in>
 ******************************************************************/

#define ENABLE_DEBUG_MACROS 0

#include "Types.h"
#include "Debug.h"
#include "RMCP.h"
#include "MD.h"



/**
 * @fn ComputeAuthCode
 * @brief Compute authentication code.
 * @param pPassword     - User password.
 * @param pSessionHdr   - Session header RMCP message.
 * @param pIPMIMsg      - IPMI message payload.
 * @param pAuthCode     - Authentication Code being generated.
 * @param ChannelType   - Channel Type.
 **/
    void
ComputeAuthCode (_FAR_ INT8U* pPassword, _NEAR_ SessionHdr_T* pSessionHdr,
        _NEAR_ IPMIMsgHdr_T* pIPMIMsg, _NEAR_ INT8U* pAuthCode,
        INT8U ChannelType)
{
    if (AUTH_TYPE_PASSWORD == pSessionHdr->AuthType)
    {
        _fmemcpy (pAuthCode, pPassword, IPMI15_MAX_PASSWORD_LEN);
    }
    else
    {
        INT8U   AuthBuf [MAX_AUTH_PARAM_SIZE];
        INT16U  AuthBufLen = 0;
        INT8U   IPMIMsgLen = *((_NEAR_ INT8U*) pIPMIMsg - 1);

        /* Password */
        _fmemcpy (AuthBuf, pPassword, IPMI15_MAX_PASSWORD_LEN);
        AuthBufLen += IPMI15_MAX_PASSWORD_LEN;
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
        _fmemcpy (AuthBuf + AuthBufLen, pPassword, IPMI15_MAX_PASSWORD_LEN);
        AuthBufLen += IPMI15_MAX_PASSWORD_LEN;

        switch (pSessionHdr->AuthType)
        {
            case AUTH_TYPE_MD2 :
                //            AuthCodeCalMD2 (AuthBuf, pAuthCode, AuthBufLen);
                break;

            case AUTH_TYPE_MD5 :
                AuthCodeCalMD5 (AuthBuf, pAuthCode, AuthBufLen);
                break;

            default  :
                IPMI_WARNING ("RMCP.c : Invalid Authentication Type \n");
        }
    }
}



