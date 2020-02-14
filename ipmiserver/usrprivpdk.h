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
 * usrprivpdk.h
 * libuserprivilegepdk Library
 *
 * Author: Revanth A <revantha@amiindia.co.in>
 *
 *****************************************************************/
#ifndef LIBUSER_PRIVILEGE_PDK
#define LIBUSER_PRIVILEGE_PDK

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <grp.h>
//#include "stdgrps.h"

#define PRIV_MAP_TBL_SIZE		6

#define GRP_IPMI "ipmi"
#define GRP_LDAP "ldap"
#define GRP_AD "ad"

#define LAN1_ADMIN                  "lanadmin"
#define LAN1_OEM                    "lanoem"
#define LAN1_OPERA                  "lanoperator"
#define LAN1_USER                   "lanuser"
#define LAN1_CALLBACK               "lancallback"
#define LAN1_NOACCESS               "lannoaccess"

#define LAN2_ADMIN                  "lan2admin"
#define LAN2_OEM                    "lan2oem"
#define LAN2_OPERA                  "lan2operator"
#define LAN2_USER                   "lan2user"
#define LAN2_CALLBACK               "lan2callback"
#define LAN2_NOACCESS               "lan2noaccess"

#define LAN3_ADMIN                  "lan3admin"
#define LAN3_OEM                    "lan3oem"
#define LAN3_OPERA                  "lan3operator"
#define LAN3_USER                   "lan3user"
#define LAN3_CALLBACK               "lan3callback"
#define LAN3_NOACCESS               "lan3noaccess"

#define SERIAL_ADMIN               "serialadmin"
#define SERIAL_OEM                 "serialoem"
#define SERIAL_OPERA               "serialoperator"
#define SERIAL_USER                "serialuser"
#define SERIAL_CALLBACK    "serialcallback"
#define SERIAL_NOACCESS    "serialnoaccess"

#define CLP_SHELL               "SmashCLP"
#define CLI_SHELL                "Cli"
#define REMOTE_SHELL             "Remote"
#define IPMIBASICMODE_SHELL      "IPMIbasicMode"
#define IPMITERMINELMODE_SHELL   "IPMIterminalMode"

#define CLP_SHELL_TYPE                0
#define CLI_SHELL_TYPE                1
#define REMOTE_SHELL_TYPE             2
#define IPMIBASICMODE_SHELL_TYPE      3
#define IPMITERMINELMODE_SHELL_TYPE   4
#define MAX_SHELL_TYPES 5
#define MAX_ENHANCED_PRIV_TYPES 0
typedef struct
{
    char* grpname;
    int grppriv;
} GroupPriv_t;

typedef struct
{
    char* grpname;
    int shelltype;
} UserShell_t;

typedef struct
{
    struct
    {
        int lanpriv:4;
        int serialpriv:4;
        int lan1priv:4;
        int lan2priv:4;
    }ipmi;
    int PreferredShell;
} usrpriv_t;

extern UserShell_t g_PreferedShell[MAX_SHELL_TYPES];
extern GroupPriv_t g_EnhGrpPriv[MAX_ENHANCED_PRIV_TYPES];

int IsADUser(char *username);
int IsLDAPUser(char *username);
int GetUsrEnhPriv(char *usrname, gid_t *groups, int grpcount,usrpriv_t *usrpriv);
#endif /*LIBUSER_PRIVILLEGE_*/
