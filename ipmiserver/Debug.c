/*****************************************************************
 *****************************************************************
 ***                                                            **
 ***    (C)Copyright 2005-2006, American Megatrends Inc.        **
 ***                                                            **
 ***            All Rights Reserved.                            **
 ***                                                            **
 ***        6145-F, Northbelt Parkway, Norcross,                **
 ***                                                            **
 ***        Georgia - 30071, USA. Phone-(770)-246-8600.         **
 ***                                                            **
 *****************************************************************
 ******************************************************************
 *
 * debug.c
 * Debug Mode Functions
 *
 *  Author: Govind Kothandapani <govindk@ami.com>
 *
 ******************************************************************/

#include "Types.h"
#include "Debug.h"

    void
print_buf (INT8U * Buf, INT16U Len)
{
    int             i;
    INT8U* pBuf =   (INT8U*)Buf;

    for (i = 0; i < Len; i++)
    {
        if (i%20 == 0) printf ("\n");
        printf ("%02x ", pBuf [i]);
    }

    printf ("\n");
    return;
}

