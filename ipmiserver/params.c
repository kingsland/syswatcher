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
 ** params.c
 ** 
 ** Functions to convert AMI OEM Parameters to OEM specific 
 ** parameter values
 **
 ** Author: senathipathyt@ami.com
 *******************************************************************/

//#include <stdio.h>
#include "Types.h"
#include "params.h"

/*-----------------------------------------------------------------
 * @fn GetLanOEMParamValue
 *
 * @brief This function is used to convert to OEM specific
 *        LAN Configuration parameters.
 *
 * @param  ParamSelect  : AMI parameter value
 *         
 * @return  OEM parameter value if match found
 *          ParamSelect if match not found
 *-----------------------------------------------------------------*/
INT8U GetLanOEMParamValue (INT8U ParamSelect)
{
    int i;

    for( i = 0; i < TOTAL_OEM_LAN_CONFIG_PARAMS; i++)
    {
        if(ParamSelect == m_OEMLanParamTbl[i].AMILanParam)
        {
            return m_OEMLanParamTbl[i].OEMLanParam;
        }		
    }

    return ParamSelect;		
}

/*-----------------------------------------------------------------
 * @fn GetLanAMIParamValue
 *
 * @brief This function is used to convert to AMI Standard
 *        LAN Configuration parameters.
 *
 * @param  ParamSelect  : OEM parameter value
 *         ImpType      : OEM implementation or not
 *         
 * @return  0 if success
 *         -1 if failed
 *-----------------------------------------------------------------*/
int GetLanAMIParamValue (INT8U* ParamSelect, INT8U* ImpType)
{

    int i;
    for( i = 0; i < TOTAL_OEM_LAN_CONFIG_PARAMS; i++)
    {
        if(*ParamSelect == m_OEMLanParamTbl[i].OEMLanParam)
        {
            *ImpType = m_OEMLanParamTbl[i].IsOEMImp;
            *ParamSelect = m_OEMLanParamTbl[i].AMILanParam;
            return 0;
        }		
    }

    return -1;  // parameter not supported
}
