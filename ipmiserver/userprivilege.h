/****************************************************************
 ****************************************************************
 **                                                            **
 **    (C)Copyright 2010-2011, American Megatrends Inc.        **
 **                                                            **
 **            All Rights Reserved.                            **
 **                                                            **
 **        5555, Oakbrook Pkwy, Norcross,                      **
 **                                                            **
 **        Georgia - 30093, USA. Phone-(770)-246-8600.         **
 **                                                            **
 ****************************************************************
 *****************************************************************
 *
 * userprivilege.h
 * libUserPrivilege Library
 *
 * Author: Revanth A <revantha@amiindia.co.in>
 *
 *****************************************************************/
#ifndef LIBUSER_PRIVILEGE
#define LIBUSER_PRIVILEGE

#include "usrprivpdk.h"

#define USER_PRIV_NO_ACCESS							0x0F
#define USER_PRIV_PROPRIETARY							0x05
#define USER_PRIV_ADMIN								0x04
#define USER_PRIV_OPERATOR								0x03
#define USER_PRIV_USER									0x02
#define USER_PRIV_CALLBACK								0x01
#define USER_PRIV_UNKNOWN								0x00

#define MAX_PRIV_CHANNELS 0x3

/* Error Codes */
#define LIBIPMI_E_SUCCESS										0x0000

#define LIBIPMI_STATUS_SUCCESS									0x00


extern GroupPriv_t g_GrpPriv[MAX_PRIV_CHANNELS * PRIV_MAP_TBL_SIZE];
extern GroupPriv_t g_Lan1GrpPriv[PRIV_MAP_TBL_SIZE];
extern GroupPriv_t g_SerialGrpPriv[PRIV_MAP_TBL_SIZE];
extern GroupPriv_t g_Lan2GrpPriv[PRIV_MAP_TBL_SIZE];
extern int g_GrpPrivcnt;

gid_t* GetUsrGroups(char *username, int *ngrps);
int GetUsrPriv(char *username, usrpriv_t *usrpriv);
void AddIPMIUsrtoChGrp(char *username, char *oldgrp, char *newgrp);
void AddIPMIUsrtoShellGrp(char *username, unsigned char old_shell, unsigned char new_shell);
void AddIPMIUsrtoFlagsGrp(char *username, int old_flags, int new_flags);

void AddUsrtoIPMIGrp(char *username,unsigned char chtype);

void DeleteUsrFromIPMIGrp(char *username);
int getUsrGrpgid(char * username, gid_t *usr_grpgid);

int GetUsrPrivFromServer(char *username, usrpriv_t *usrpriv);
int GetUsrLANChPriv(usrpriv_t *usrpriv, unsigned char *IPAddr);
int GetUsrSerialChPriv(usrpriv_t *usrpriv);
void UpdateGrpPrivTable(unsigned char chtype);
#endif
