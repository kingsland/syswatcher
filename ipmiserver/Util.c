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
 * util.c
 * Utility functions.
 *
 * Author: Govind Kothandapani <govindk@ami.com>
 *
 *****************************************************************/
#include "Types.h"
#include "Util.h"
#include <sys/sysinfo.h> 


/*----------------------------------------
 * GetBits
 *----------------------------------------*/
    INT8U
GetBits (INT8U Val, INT8U Mask)
{
    INT8U ExtVal = 0;
    while (0 != Mask)
    {
        if (0 != (Mask & 0x80))
        {
            ExtVal <<= 1;
            if (0 != (Val & 0x80)) { ExtVal += 0x01; }
        }

        Val <<= 1;
        Mask <<= 1;
    }

    return ExtVal;
}

/*------------------------------------------
 * SetBits
 *------------------------------------------*/
    INT8U
SetBits (INT8U Mask, INT8U Val)
{
    INT8U FinVal = 0;
    INT8U i;

    for (i = 0; i < 8; i++)
    {
        FinVal >>= 1;
        if (0 != (Mask & 0x01))
        {
            if (0 != (Val & 0x01)) { FinVal |= 0x80; }
            Val >>= 1;
        }
        Mask   >>= 1;
    }

    return FinVal;
}

/*------------------------------------------
 * CalculateCheckSum
 *------------------------------------------*/
    INT8U
CalculateCheckSum (INT8U* Data, INT16U Len)
{
    INT8U   Sum;
    INT16U  i;

    Sum = 0;
    for (i = 0; i < Len; i++)
    {
        Sum += Data [i];
    }
    return (0x100 - Sum);
}

/*
 * @fn TimeUpdate
 * @return Returns the system uptime
 */
INT32U TimeUpdate()
{
    struct sysinfo time;
    sysinfo(&time);
    return time.uptime;
}

/**
 *@fn GetSysCtlvalue
 *@brief This function retrieves the values from Sysctl
 *@param TagName -Tagname form which value has to be retrieved
 *@param SysVal - Where the value is stored
 *@return Returns 0 on success
 *            Returns 1 on failure
 */
int GetJiffySysCtlvalue (const char *TagName, long long *SysVal)
{
    unsigned long RetVal;
    FILE* SysFile = fopen (TagName, "r");
    unsigned long Hertz = sysconf(_SC_CLK_TCK);

    if ((!SysFile) || (!SysVal))
        return 1;

    fscanf (SysFile, "%lu", &RetVal);
    fclose (SysFile);

    *SysVal = (RetVal*1000)/Hertz;
    return 0;
}

// Equivalent function for the systemcall 'mv'
// the old file will be moved/renamed to new file name
int moveFile(char *oldFile, char *newFile)
{
    int ret;
    ret = copyFile(oldFile, newFile);
    if(ret != 0)
    {
        printf(" %s not copied to %s", oldFile, newFile);
        return -1;
    }
    if(unlink(oldFile) != 0)
    {
        printf("%s not removed",oldFile);
        return -1;
    }
    return 0;
}

// Equivalent function for the systemcall 'cp'
// the source file will be copied to destineation file
int copyFile(char *sourceFile, char *destFile)
{
    FILE *fpRead, *fpWrite;
    char oneline[80];
    fpRead=fopen(sourceFile,"rb");
    if(fpRead == NULL)
    {
        printf("\n %s file not found",sourceFile);
        return -1;
    }
    fpWrite=fopen(destFile,"w");
    if(fpWrite == NULL)
    {
        printf("\n %s file not created",destFile);
        return -1;
    }
    while(!feof(fpRead))
    {
        if(fgets(oneline,79,fpRead) == NULL)
            break;
        fputs(oneline,fpWrite);
    }
    fclose(fpRead);
    fclose(fpWrite);
    return 0;
}


