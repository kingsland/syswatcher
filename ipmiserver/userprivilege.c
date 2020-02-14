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
 * userprivilege.c
 * libUserPrivilege Library
 *
 * Author: Revanth A <revantha@amiindia.co.in>
 *
 *****************************************************************/
#include "dbgout.h"
#include "Debug.h"
//#include "usermapis.h"
#include "userprivilege.h"
#include "libipmi_struct.h"
#include "IPMI_AppDevice.h"
#include "lanchcfg.h"
//#include "libipmi_AMIOEM.h"

GroupPriv_t g_GrpPriv[MAX_PRIV_CHANNELS * PRIV_MAP_TBL_SIZE] = {{"",0}};
GroupPriv_t g_Lan1GrpPriv[PRIV_MAP_TBL_SIZE]=
{
    {LAN1_NOACCESS,         USER_PRIV_NO_ACCESS},
    {LAN1_CALLBACK,       USER_PRIV_CALLBACK},
    {LAN1_USER,                     USER_PRIV_USER}, 
    {LAN1_OPERA,                USER_PRIV_OPERATOR},
    {LAN1_ADMIN,                     USER_PRIV_ADMIN},
    {LAN1_OEM,                        USER_PRIV_PROPRIETARY},
};
GroupPriv_t g_SerialGrpPriv[PRIV_MAP_TBL_SIZE]=
{
    {SERIAL_NOACCESS,   USER_PRIV_NO_ACCESS<<4},
    {SERIAL_CALLBACK,   USER_PRIV_CALLBACK<<4},
    {SERIAL_USER,               USER_PRIV_USER<<4}, 
    {SERIAL_OPERA,      USER_PRIV_OPERATOR<<4},
    {SERIAL_ADMIN,           USER_PRIV_ADMIN<<4},
    {SERIAL_OEM,                   USER_PRIV_PROPRIETARY<<4},

};
GroupPriv_t g_Lan2GrpPriv[PRIV_MAP_TBL_SIZE]=
{
    {LAN2_NOACCESS,     USER_PRIV_NO_ACCESS<<8},
    {LAN2_CALLBACK,     USER_PRIV_CALLBACK<<8},
    {LAN2_USER,                     USER_PRIV_USER<<8},
    {LAN2_OPERA,        USER_PRIV_OPERATOR<<8},
    {LAN2_ADMIN,            USER_PRIV_ADMIN<<8},
    {LAN2_OEM,                    USER_PRIV_PROPRIETARY<<8}
};
const char *ServiceGrps[] = {GRP_IPMI,GRP_LDAP,GRP_AD,NULL};
int g_GrpPrivcnt = 0;

/**
 * @fn UpdateGrpPrivTable
 * @brief This function used Add group name and privilege in global group privilege table based on channel type
 * @param  chtype - Channel type.
 * @retval void
 **/
void UpdateGrpPrivTable(INT8U chtype)
{
    int i;
    for(i = 0; i < PRIV_MAP_TBL_SIZE ; i++)
    {
        if(chtype == LAN_RMCP_CHANNEL1_TYPE)
        {
            g_GrpPriv[g_GrpPrivcnt].grpname = g_Lan1GrpPriv[i].grpname;
            g_GrpPriv[g_GrpPrivcnt].grppriv = g_Lan1GrpPriv[i].grppriv;
        }
        else
        {
            if(chtype == LAN_RMCP_CHANNEL2_TYPE)
            {
                g_GrpPriv[g_GrpPrivcnt].grpname = g_Lan2GrpPriv[i].grpname;
                g_GrpPriv[g_GrpPrivcnt].grppriv = g_Lan2GrpPriv[i].grppriv;
            }
            else
            {
                g_GrpPriv[g_GrpPrivcnt].grpname = g_SerialGrpPriv[i].grpname;
                g_GrpPriv[g_GrpPrivcnt].grppriv = g_SerialGrpPriv[i].grppriv;
            }
        }
        g_GrpPrivcnt++;
    }
}
/**
 * @fn GetUsrGroups
 * @brief This function used to get user groups
 * @param  username - Username.
 * @param  ngrps - pointer of groups count 
 * @retval NULL on Failure
 gid_t pointer on Success
 **/
gid_t* GetUsrGroups(char *username,int *ngrps)
{
    gid_t *groups =NULL;
    int ngroups=0;
    gid_t usrgrp_gid;

    if(0 !=getUsrGrpgid(username,&usrgrp_gid))
    {
        return NULL;
    }

    if (getgrouplist(username,usrgrp_gid, NULL, &ngroups) < 0) 
    {
        groups = (gid_t *) malloc(ngroups * sizeof (gid_t));
        getgrouplist(username, usrgrp_gid, groups, &ngroups);
    } 
    *ngrps=ngroups;
    return groups;
}
/**
 * @fn GetUsrPriv
 * @brief This function used to get User privilege.
 * @param  username - Username.
 * @param  priv - privilege 
 * @retval  0 - on Success
 -1 on Failure
 **/

int GetUsrPriv(char *username, usrpriv_t *usr_priv)
{

    struct group *gr;
    gid_t *groups =NULL;
    int ngroups=0;
    int j,index;
    INT32U *usrpriv=(INT32U *)usr_priv;

    memset(usr_priv, 0, sizeof(usrpriv_t));
    groups=GetUsrGroups(username,&ngroups);

    if(ngroups == 0||groups == NULL) 
    {
        TCRIT("User Groups Not Found !!\n");
        if(groups != NULL)
            free(groups);
        return -1;
    }
    for (index = 0; index < ngroups; index++)
    {
        gr = getgrgid (groups[index]);

        for(j = 0; j < PRIV_MAP_TBL_SIZE; j++)
        {
            if(strcasecmp(gr->gr_name,g_Lan1GrpPriv[j].grpname)==0)
            {
                (*usrpriv) |= g_Lan1GrpPriv[j].grppriv;
            }
        }
        for(j = 0; j < PRIV_MAP_TBL_SIZE; j++)
        {
            if(strcasecmp(gr->gr_name,g_Lan2GrpPriv[j].grpname)==0)
            {
                (*usrpriv) |= g_Lan2GrpPriv[j].grppriv;
            }
        }
        for(j = 0; j < PRIV_MAP_TBL_SIZE; j++)
        {
            if(strcasecmp(gr->gr_name,g_SerialGrpPriv[j].grpname)==0)
            {
                (*usrpriv) |= g_SerialGrpPriv[j].grppriv;
            }
        }
    }
    //    GetUsrEnhPriv(username, groups,ngroups, usr_priv);
    TDBG("\n User Privillege :%x",*usrpriv);
    free(groups);
    return 0;
}
/**
 * @fn AddIPMIUsrtoChGrp
 * @brief This function used to Add IPMI user in Channel groups.
 * @param  username - User Name.
 * @param  oldgrp - old group 
 * @param  newgrp - New group 
 * @retval  void
 **/
void AddIPMIUsrtoChGrp(char *username,char *oldgrp, char *newgrp)
{
    char user[MAX_USERNAME_LEN+1];

    memset(user,0,MAX_USERNAME_LEN+1);
    memcpy(user,username,MAX_USERNAME_LEN);
    //    if(strcmp(oldgrp,"")!=0)
    //        DeleteUserFromGroup(user, oldgrp);//Delete user from the old group

    //    if(strcmp(newgrp,"")!=0)
    //        AddUserToGroup(user, newgrp);//Add user in new group

}

/**
 * @fn CleanUpUsr
 * @brief This function used to delete IPMI user from groups based on channel type.
 * @param  username - User Name.
 * @param  chtype - channel type.
 * @retval  void
 **/
void CleanUpUsr(char *username , unsigned char chtype)
{
    struct group *gr;
    gid_t *groups =NULL;
    int ngroups=0;
    int index,j;

    groups=GetUsrGroups(username,&ngroups);

    if(ngroups == 0||groups == NULL) 
    {
        TCRIT("User Groups Not Found !!\n");
        if(groups != NULL)
            free(groups);
        return;
    }
    for (index = 0; index < ngroups; index++)
    {
        gr = getgrgid (groups[index]);
        if(chtype == LAN_RMCP_CHANNEL1_TYPE)
        {
            for(j = 0; j < PRIV_MAP_TBL_SIZE; j++)
            {
                if(strcasecmp(gr->gr_name,g_Lan1GrpPriv[j].grpname)==0)
                {
                    //                    DeleteUserFromGroup(username, gr->gr_name);
                    ;
                }
            }
        }
        else 
        {
            if(chtype == LAN_RMCP_CHANNEL2_TYPE)
            {
                for(j = 0; j < PRIV_MAP_TBL_SIZE; j++)
                {
                    if(strcasecmp(gr->gr_name,g_Lan2GrpPriv[j].grpname)==0)
                    {
                        //                        DeleteUserFromGroup(username, gr->gr_name);
                        ;
                    }
                }
            }
            else
            {
                for(j = 0; j < PRIV_MAP_TBL_SIZE; j++)
                {
                    if(strcasecmp(gr->gr_name,g_SerialGrpPriv[j].grpname)==0)
                    {
                        //                        DeleteUserFromGroup(username, gr->gr_name);
                        ;
                    }
                }
                for(j = 0; j < MAX_SHELL_TYPES; j++)
                {
                    //                    if(strcasecmp(gr->gr_name,g_PreferedShell[j].grpname)==0)
                    {
                        //                        DeleteUserFromGroup(username, gr->gr_name);
                        ;
                    }
                }
                for(j = 0; j < MAX_ENHANCED_PRIV_TYPES; j++)
                {
                    //                    if(strcasecmp(gr->gr_name,g_EnhGrpPriv[j].grpname)==0)
                    {
                        //                        DeleteUserFromGroup(username, gr->gr_name);
                        ;
                    }
                }
            }
        }
    }
    free(groups);
    return;
}
/**
 * @fn AddUsrtoIPMIGrp
 * @brief This function used to Add user in IPMI Grp and delete the user if it
 * already exist based on channel type
 * @param  username - Username.
 * @param  chtype - channel type.
 * @retval  void
 **/

void AddUsrtoIPMIGrp(char *username, unsigned char chtype)
{
    char user[MAX_USERNAME_LEN+1];

    memset(user,0,MAX_USERNAME_LEN+1);
    memcpy(user,username,MAX_USERNAME_LEN);
    //    if(DoesUserExistInGroup(user,GRP_IPMI))
    //    {
    //        CleanUpUsr(user, chtype);
    //    }
    //    AddUserToGroup(user, GRP_IPMI);
    return;
}
/**
 * @fn AddUsrtoIPMIGrp
 * @brief This function used to Add user in Shell Grp
 * @param  username - User Name.
 * @param  old_shell - old shell type 
 * @param  new_shell - New shell type 
 * @retval  void
 **/
void AddIPMIUsrtoShellGrp(char *username, unsigned char old_shell, unsigned char new_shell)
{
    char user[MAX_USERNAME_LEN+1];
    char newgrp[20]="", oldgrp[20]="";
    int i;
    memset(user, 0, MAX_USERNAME_LEN+1);
    memcpy(user, username, MAX_USERNAME_LEN);
#if 0
    if(old_shell != -1)
    {
        for(i=0; i < MAX_SHELL_TYPES; i++)
        {
            if(old_shell == g_PreferedShell[i].shelltype)
            {
                strcpy(oldgrp, g_PreferedShell[i].grpname);
                break;
            }
        }
    }
    if(new_shell != -1)
    {
        for(i=0; i < MAX_SHELL_TYPES; i++)
        {
            if(new_shell == g_PreferedShell[i].shelltype)
            {
                strcpy(newgrp,g_PreferedShell[i].grpname);
                break;
            }
        }
    }
    if(strcmp(oldgrp, "") != 0)
        DeleteUserFromGroup(user, oldgrp);//Delete user from the old group

    if(strcmp(newgrp, "") != 0)
        AddUserToGroup(user, newgrp);//Add user in new group
#endif
    return;
}

/**
 * @fn AddIPMIUsrtoFlagsGrp
 * @brief This function used to Add user in Flags Grp
 * @param  username - User Name.
 * @param  old_flags - old Flags 
 * @param  new_flags - New Flags 
 * @retval  void
 **/
void AddIPMIUsrtoFlagsGrp(char *username, int old_flags, int new_flags)
{
    char user[MAX_USERNAME_LEN+1];
    int i;
    memset(user, 0, MAX_USERNAME_LEN+1);
    memcpy(user, username, MAX_USERNAME_LEN);
#if 0    
    for(i=0; i < MAX_ENHANCED_PRIV_TYPES; i++)
    {
        if((new_flags & g_EnhGrpPriv[i].grppriv) == g_EnhGrpPriv[i].grppriv)
        {
            if((old_flags & g_EnhGrpPriv[i].grppriv) != g_EnhGrpPriv[i].grppriv)
                AddUserToGroup(user, g_EnhGrpPriv[i].grpname);//Add user in group
        }
        else
        {
            if((old_flags & g_EnhGrpPriv[i].grppriv) == g_EnhGrpPriv[i].grppriv)
                DeleteUserFromGroup(user, g_EnhGrpPriv[i].grpname);//Delete user from group
        }
    }
#endif
    return;
}

/**
 * @fn DeleteUsrFromIPMIGrp
 * @brief This function used to Delete user in IPMI Grp
 * @param  username - Username.
 * @retval  void
 **/
void DeleteUsrFromIPMIGrp(char *username)
{
    //    DeleteUserFromGroup(username, GRP_IPMI);
}

/**
 * @fn getUsrGrpgid
 * @brief This function used to Get User Group gid
 * @param  username - Username.
 * @param  usrgrp_gid - User group GID.
 * @retval  -1 on fail
 0 on success
 **/

int getUsrGrpgid(char * username, gid_t *usrgrp_gid)
{
    gid_t *groups =NULL;
    struct group *gr=NULL;
    int ngroups=0,i,j;

    if(getgrouplist(username,0, NULL, &ngroups) < 0)
    {
        groups = (gid_t *) malloc(ngroups * sizeof (gid_t));
        getgrouplist(username,0, groups, &ngroups);
    }

    for(i=0;i<ngroups;i++)
    {
        gr=getgrgid (groups[i]);
        for(j=0;ServiceGrps[j] != NULL;j++)
        {
            if(strcasecmp(gr->gr_name,ServiceGrps[j])==0)
            {
                *usrgrp_gid=gr->gr_gid;
                return 0;
            }
        }
    }
    TCRIT("Invalid User !!\n");
    return -1;
}
/**
 * @fn GetUsrPrivFromServer
 * @brief This function used to get User privilege from server.
 * @param  username - Username.
 * @param  priv - privilege 
 * @retval  0 - on Success
 -1 on Failure
 **/
int GetUsrPrivFromServer(char *username, usrpriv_t *usrpriv)
{
    memset(usrpriv,0x0,sizeof(usrpriv_t));
    if(GetUsrPriv(username,usrpriv) < 0)//IPMI User
    {
#if 0
        if(IsLDAPUser(username) < 0)//LDAP User
        {
            if(IsADUser(username) < 0)//AD User
            {
                return -1;
            }
        }
#endif
    }
    if(GetUsrPriv(username,usrpriv) < 0)
    {
        return -1;
    }
    return 0;
}

/**
 * @fn GetUsrLANChPriv
 * @brief This function used to get User privilege based on Channel.
 * @param  usrpriv - pointer of user privilege structure.
 * @param  IPAddr - pointer of IPAddress
 * @return Returns privilege on success
 *                    -1 on fail
 **/
int GetUsrLANChPriv(usrpriv_t *usrpriv,unsigned char *IPAddr)
{
    int priv = 0;
    INT8U ChannelType = 0,ChannelNo=0;
    int wRet = 0;
    INT8U BMCInst=0;

    //	wRet = LIBIPMI_HL_AMIGetUDSInfo(&IPAddr[0],&ChannelType,&ChannelNo,&BMCInst,5);
    if((wRet != LIBIPMI_E_SUCCESS) || ((ChannelType == -1) && (BMCInst == 0)))
    {
        TCRIT("Unable to Get Channel Type and BMC Instance to Communicate\n");
        return -1;
    }

    switch(ChannelType)
    {
        case LAN_RMCP_CHANNEL1_TYPE:
            priv = usrpriv->ipmi.lanpriv;
            break;
        case LAN_RMCP_CHANNEL2_TYPE:
            priv = usrpriv->ipmi.lan1priv;
            break;
        case LAN_RMCP_CHANNEL3_TYPE:
            priv = usrpriv->ipmi.lan2priv;
            break;
        default:
            return -1;
            break;
    }

    switch(priv & 0x0F ) // we shall check with last four bits to identify the privilege
    {
        case PRIV_LEVEL_PROPRIETARY:
            return PRIV_LEVEL_ADMIN;//OEM privilege considered as Admin priv.  
            break;
        case PRIV_LEVEL_ADMIN:
            return PRIV_LEVEL_ADMIN;
            break;
        case PRIV_LEVEL_OPERATOR:
            return PRIV_LEVEL_OPERATOR;
            break;
        case PRIV_LEVEL_USER:
            return PRIV_LEVEL_USER;
            break;
        case PRIV_LEVEL_CALLBACK:
            return PRIV_LEVEL_CALLBACK;
            break;
        case PRIV_LEVEL_NO_ACCESS:
            return PRIV_LEVEL_NO_ACCESS;
            break;
        case PRIV_LEVEL_RESERVED:
            return PRIV_LEVEL_NO_ACCESS;
            break;
    }
    return -1;
}
/**
 * @fn GetUsrSerialChPriv
 * @brief This function used to get User privilege based on Channel.
 * @param  usrpriv - pointer of user privilege structure.
 * @return Returns privilege on success
 *                    -1 on fail
 **/
int GetUsrSerialChPriv(usrpriv_t *usrpriv)
{
    return usrpriv->ipmi.serialpriv;
}
