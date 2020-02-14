/*****************************************************************
 ******************************************************************
 ***                                                            ***
 ***        (C)Copyright 2010, American Megatrends Inc.         ***
 ***                                                            ***
 ***                    All Rights Reserved                     ***
 ***                                                            ***
 ***       5555 Oakbrook Parkway, Norcross, GA 30093, USA       ***
 ***                                                            ***
 ***                     Phone 770.246.8600                     ***
 ***                                                            ***
 ******************************************************************
 ******************************************************************
 ******************************************************************
 *
 * Filename: ncml.c
 *
 * Description: Contains code for the network conection manager library APIs.
 *
 ******************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "unix.h"
#include "dbgout.h"
#include <sys/types.h>
#include <sys/stat.h>

#include "ncml.h"
//#include "defservicecfg.h"
#include <netinet/in.h>
#include "dictionary.h"
#include "iniparser.h"

#include <linux/if.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "netmon_io.h"
#include "parse-ex.h"
#include "semaph.h"
#include "featuredef.h"

#define TIMEOUTD_CONF   "/etc/timeouts"
#define TIMEOUTD_APP    "/etc/init.d/timeout restart"

static char semaphoreKey[]= "libncmlconf";

SERVICE_CONF_STRUCT g_ServiceDefConfTbl[MAX_SERVICE_NUM] = 
{
    {
        WEB_SERVICE_NAME,
        1,
        "eth0",
        80,
        443,
        0x708,
        0x94,
        0x80
    },

    {
        KVM_SERVICE_NAME,
        1,
        "eth0",
        7578,
        7582,
        0xFFFFFFFF,
        0x82,
        0x80
    },

    {
        CDMEDIA_SERVICE_NAME,
        1,
        "eth0",
        5120,
        5124,
        0xFFFFFFFF,
        0x81,
        0x80
    },

    {
        FDMEDIA_SERVICE_NAME,
        1,
        "eth0",
        5122,
        5126,
        0xFFFFFFFF,
        0x81,
        0x80
    },

    {
        HDMEDIA_SERVICE_NAME,
        1,
        "eth0",
        5123,
        5127,
        0xFFFFFFFF,
        0x81,
        0x80
    },

    {
        SSH_SERVICE_NAME,
        1,
        "FFFFFFFFFFFFFFFF",
        0xFFFFFFFF,
        22,
        600,
        0xFF,
        0xFF
    },

    {
        TELNET_SERVICE_NAME,
        0,
        "FFFFFFFFFFFFFFFF",
        23,
        0xFFFFFFFF,
        600,
        0xFF,
        0xFF
    },
}; 

/*
 *@fn get_network_interface_count
 *@brief This function return the number of all network interfaces                       
 *@param ifcount - The count of all interfaces
 *@return Returns -1 on failure
 *        Returns 0 on success
 */
int get_network_interface_count(int *ifcount)
{   
#if 0
    int fd;
    int ret;

    fd = open("/dev/netmon", O_RDONLY);
    if (fd == -1)
    {
        //        TCRIT("ERROR: open failed (%s)\n", strerror(errno));
        return -1;
    }

    ret = ioctl(fd, NETMON_GET_INTERFACE_COUNT, ifcount);
    if (ret < 0)
    {
        TCRIT("ERROR: Get Numnber of Interfaces failed (%s)\n",strerror(errno));
        close(fd);
        return -1;
    }

    TDBG ("Number of Network Interfaces = %d\n",*ifcount);

    close(fd);

#else
    *ifcount = 1;
#endif
    return 0;
}

/*
 *@fn get_up_network_interfaces
 *@brief This function return the network interfaces with up status in the system at the time                       
 *@param up_interfaces - An array of interface name strings that are currently enabled
 *@param ifupcount - The count of the interfaces that are currently enabled
 *@param ifcount - The count of all interfaces
 *@return Returns -1 on failure
 *        Returns 0 on success
 */
int get_up_network_interfaces(char *up_interfaces, int *ifupcount, int ifcount)
{
    int fd;
    int ret,i;
    INTERFACE_LIST *ilist;
    char *names;
    unsigned char *up_status;

    if (ifcount < 0)
    {
        TCRIT("ERROR: The number of interface less then 0\n");
        return -1;
    }

    fd = open("/dev/netmon", O_RDONLY);
    if (fd == -1)
    {
        TCRIT("ERROR: open failed (%s)\n", strerror(errno));
        return -1;
    }

    /*
     * Get all interface list
     */ 
    ilist= (INTERFACE_LIST *)malloc(sizeof(INTERFACE_LIST));
    if (ilist == NULL)
    {
        TCRIT("ERROR: Unable to allocate memory for interface list\n");
        close(fd);
        return -1;
    }

    names = malloc(ifcount* (IFNAMSIZ+1));
    if (names == NULL)
    {
        TCRIT("ERROR: Unable to allocate memory for interface names list\n");
        close(fd);
        free(ilist);
        return -1;
    }
    ilist->ifname = names;

    up_status = malloc(ifcount* (sizeof(unsigned char)));
    if (up_status == NULL)
    {
        TCRIT("ERROR: Unable to allocate memory for interface active status list\n");
        close(fd);
        free(names);
        free(ilist);
        return -1;
    }
    ilist->ifupstatus = up_status;

    ilist->count=0;
    ret = ioctl(fd, NETMON_GET_INTERFACE_LIST, ilist);
    if (ret < 0)
    {
        TCRIT("ERROR: Get Iterfaces failed (%s)\n",strerror(errno));
        close(fd);
        free(up_status); 
        free(names);
        free(ilist);
        return -1;
    }

    /*
     * Only copy the interface name with UP status
     */
    (*ifupcount)=0;
    for(i=0;i<ilist->count;i++)
    {
        if(ilist->ifupstatus[i] == 1)
        {
            (*ifupcount)++;
            memcpy(&up_interfaces[i*(IFNAMSIZ+1)], &names[i*(IFNAMSIZ+1)], (IFNAMSIZ+1));
        }
    }

    close(fd);
    free(up_status); 
    free(names);
    free(ilist);

    return 0;
}

/*
 *@fn get_network_interfaces_name
 *@brief This function return the network interfaces name for the ifcount
 *@param up_interfaces - An array of interface name strings of all enabled interfaces
 *@param ifcount - The count of all interfaces
 *@return Returns -1 on failure
 *        Returns 0 on success
 */
int get_network_interfaces_name(char *up_interfaces, int ifcount)
{
    int fd;
    int ret,i;
    INTERFACE_LIST *ilist;
    char *names;
    unsigned char *up_status;

    if (ifcount < 0)
    {
        TCRIT("ERROR: The number of interface less then 0\n");
        return -1;
    }

    fd = open("/dev/netmon", O_RDONLY);
    if (fd == -1)
    {
        TCRIT("ERROR: open failed (%s)\n", strerror(errno));
        return -1;
    }

    /*
     * Get all interface list
     */ 
    ilist= (INTERFACE_LIST *)malloc(sizeof(INTERFACE_LIST));
    if (ilist == NULL)
    {
        TCRIT("ERROR: Unable to allocate memory for interface list\n");
        close(fd);
        return -1;
    }
    memset(ilist,0,sizeof(INTERFACE_LIST));
    names = malloc(ifcount* (IFNAMSIZ+1));
    if (names == NULL)
    {
        TCRIT("ERROR: Unable to allocate memory for interface names list\n");
        close(fd);
        free(ilist);
        return -1;
    }
    memset(names,0,(ifcount* (IFNAMSIZ+1)));
    ilist->ifname = names;

    up_status = malloc(ifcount* (sizeof(unsigned char)));
    if (up_status == NULL)
    {
        TCRIT("ERROR: Unable to allocate memory for interface active status list\n");
        close(fd);
        free(names);
        free(ilist);
        return -1;
    }
    ilist->ifupstatus = up_status;

    ilist->count=0;
    ret = ioctl(fd, NETMON_GET_INTERFACE_LIST, ilist);
    if (ret < 0)
    {
        TCRIT("ERROR: Get Iterfaces failed (%s)\n",strerror(errno));
        close(fd);
        free(up_status); 
        free(names);
        free(ilist);
        return -1;
    }

    /*
     * copy the interface name
     */
    for(i=0;i<ilist->count;i++)
    {
        memcpy(&up_interfaces[i*(IFNAMSIZ+1)], &names[i*(IFNAMSIZ+1)], (IFNAMSIZ+1));
    }

    close(fd);
    free(up_status); 
    free(names);
    free(ilist);

    return 0;
}

/*
 *@fn wait_network_state_change
 *@brief Wakes up when there is any state change in network devices
 */
int wait_network_state_change()
{
    int fd,ret=0,count=0;

    fd = open("/dev/netmon", O_RDONLY);
    if (fd == -1)
    {
        TCRIT("ERROR: open failed (%s)\n", strerror(errno));
        return 0;
    }

    ret = ioctl(fd, NETMON_WAIT_FOR_INTERFACE_CHANGE, &count);
    if(ret < 0)
    {
        TCRIT("ERROR : Wait for Interface Changes is (%s) \n",strerror(errno));
        close(fd);
        return 0;
    }

    close(fd);

    return 0;
}

/*
 *@fn get_service_configurations
 *@brief This function should return the service configuration details for the service name
 *@param service_name - service name 
 *@param conf - service configuration details
 *@return Returns -1 on failure
 *        Returns 0 on success
 */
int get_service_configurations(char *service_name, SERVICE_CONF_STRUCT* conf)
{  
    dictionary *d = NULL;
    INTU    err_value = 0xFFFFFFFE;
    char temp[MAX_TEMP_ARRAY_SIZE];
    INTU tempval;
    char *str=NULL;
    char *sectionname=service_name;

    if(sectionname == NULL)
    {
        TCRIT("The input service name is NULL\n");
        return -1;
    }

    d = iniparser_load(SERVICE_CONF_FILE);
    if( d == NULL )
    {
        TCRIT("Unable to find/load/parse Configuration file : %s", SERVICE_CONF_FILE);
        return -1;
    }

    //get the values now
    //ServiceName
    memset(temp,0,sizeof(temp));
    sprintf(temp, "%s:%s", sectionname, STR_SERVICE_NAME);
    str = iniparser_getstr (d,temp);
    if(str == NULL)
    {  
        TDBG("Configuration %s is not found\n", STR_SERVICE_NAME);
        memset(conf->ServiceName,'\0',MAX_SERVICE_NAME_SIZE);
    }
    else
    {  
        strncpy(conf->ServiceName,str,MAX_SERVICE_NAME_SIZE);
        TDBG("%s = %s \n",STR_SERVICE_NAME, conf->ServiceName);
    }

    //CurrentState
    tempval = IniGetUInt(d,sectionname,STR_SERVICE_CURRENT_STATE,err_value);
    if(tempval == err_value)
    {
        TDBG("Cannot Find Current State for the given services\n");
    }
    else
    {
        conf->CurrentState = (INT8U)tempval;
        TDBG("Configured Current State is %x\n",conf->CurrentState);
    }

    //InterfaceName
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%s:%s",sectionname, STR_SERVICE_INTERFACE_NAME);
    str = iniparser_getstr (d,temp);
    if(str == NULL)
    {
        TDBG("Configuration %s is not found\n", STR_SERVICE_INTERFACE_NAME);
        memset(conf->InterfaceName,'\0',MAX_SERVICE_IFACE_NAME_SIZE);
    }
    else
    {  
        strncpy(conf->InterfaceName,str,MAX_SERVICE_IFACE_NAME_SIZE);
        TDBG("%s = %s \n",STR_SERVICE_INTERFACE_NAME, str);
    }

    //NonSecureAccessPort
    conf->NonSecureAccessPort = IniGetUInt(d,sectionname,STR_SERVICE_NONSECURE_PORT,err_value);
    TDBG("Configured %s value is %x \n", STR_SERVICE_NONSECURE_PORT, tempval);
    if(conf->NonSecureAccessPort == err_value)
    {
        TDBG("Cannot Find Non Secure Access Port for the given service\n");
    }

    //SecureAccessPort
    conf->SecureAccessPort = IniGetUInt(d,sectionname,STR_SERVICE_SECURE_PORT,err_value);      
    TDBG("Configured %s value is %lx \n", STR_SERVICE_SECURE_PORT, conf->SecureAccessPort);
    if(conf->SecureAccessPort == err_value)
    {
        TDBG("Cannot Find Non Secure Access Port for the given service\n");
    }

    //SessionInactivityTimeout
    conf->SessionInactivityTimeout = IniGetUInt(d,sectionname,STR_SERVICE_SESSION_TIMEOUT,err_value);  
    TDBG("Configured %s value is %x \n", STR_SERVICE_SESSION_TIMEOUT, conf->SessionInactivityTimeout);

    //Max Allow Session
    tempval = IniGetUInt(d,sectionname,STR_SERVICE_MAX_SESSIONS,err_value);
    if(tempval == err_value)
    {
        TDBG("Cannot Find Current State for the given services\n");
    }
    else
    {
        conf->MaxAllowSession = (INT8U)tempval;
        TDBG("Configured Current State is %x\n",conf->MaxAllowSession);
    }

    //Current Active Session
    tempval = IniGetUInt(d,sectionname,STR_SERVICE_ACTIVE_SESSIONS,err_value);
    if(tempval == err_value)
    {
        TDBG("Cannot Find Current State for the given services\n");
    }
    else
    {
        conf->CurrentActiveSession = (INT8U)tempval;
        TDBG("Configured Current State is %x\n",conf->CurrentActiveSession);
    }
    //MaxSessionInactivityTimeout
    conf->MaxSessionInactivityTimeout = IniGetUInt(d,sectionname,STR_MAX_SESSION_INACTIVITY_TIMEOUT,err_value);
    TDBG("Configured %s value is %x \n", STR_MAX_SESSION_INACTIVITY_TIMEOUT, conf->MaxSessionInactivityTimeout);
    //MinSessionInactivityTimeout
    conf->MinSessionInactivityTimeout = IniGetUInt(d,sectionname,STR_MIN_SESSION_INACTIVITY_TIMEOUT,err_value);
    TDBG("Configured %s value is %x \n", STR_MIN_SESSION_INACTIVITY_TIMEOUT, conf->MinSessionInactivityTimeout);

    iniparser_freedict(d);

    return 0;

}

/*
 * @fn update_timeout_conf
 * @brief Function which updates the timeout config file.
 * @param[in] timeout - Number of Minutes for inactivity timeout
 */
static int update_timeout_conf (INTU timeout)
{
    FILE *fp = fopen(TIMEOUTD_CONF, "w");
    if (fp == NULL)
    {
        return -1;
    }

    fprintf(fp, "Al:*:*:*:%d:0:0:0", timeout);
    fclose (fp);
    // restarting timeout daemon.
    if (system(TIMEOUTD_APP) == -1)
    {
        return -1;
    }
    return 0;
}

/*
 *@fn set_service_configurations
 *@brief This function should setup the service configuration details for the service name 
 *@param service_name - service name 
 *@param conf - service configuration details
 *@return Returns -1 on failure
 *        Returns 0 on success
 */
int set_service_configurations(char *service_name, SERVICE_CONF_STRUCT* conf,int timeoutd_sess_timeout)
{
    dictionary *d = NULL;
    char temp[MAX_TEMP_ARRAY_SIZE];
    char tempval[MAX_TEMP_ARRAY_SIZE];
    char *sectionname=service_name;
    FILE* fp;
    char InterfaceName[MAX_SERVICE_IFACE_NAME_SIZE+1];

    memset(InterfaceName,0,sizeof(InterfaceName));

    if(sectionname == NULL)
    {
        TCRIT("The input service name is NULL\n");
        return -1;
    }

    sem_t *semaphore = CreateSemaphore(&semaphoreKey[0]);
    if(semaphore == SEM_FAILED) return -1;

    WaitSemaphore(semaphore);

    d = iniparser_load(SERVICE_CONF_FILE);
    if( d == NULL )
    {
        TCRIT("Unable to find/load/parse Configuration file : %s", SERVICE_CONF_FILE);
        ReleaseSemaphore(semaphore,&semaphoreKey[0]);
        return -1;
    }
    /* check for NULL pointer */
    if( conf == NULL )
    {
        TCRIT(" Pointer that is passed to set configuration is NULL\n");
        ReleaseSemaphore(semaphore,&semaphoreKey[0]);
        return -1;
    }

    //set the values now
    //ServiceName
    conf->ServiceName[strlen(conf->ServiceName)] = 0;
    memset(temp,0,sizeof(temp));
    sprintf(temp, "%s:%s", sectionname, STR_SERVICE_NAME);
    iniparser_setstr(d, temp, conf->ServiceName);

    //CurrentState
    sprintf(temp,"%s:%s",sectionname, STR_SERVICE_CURRENT_STATE);
    sprintf(tempval, "%u", conf->CurrentState);
    iniparser_setstr(d, temp, tempval);

    //InterfaceName
    if(conf->InterfaceName[0] == '\0')
    {
        TDBG("%s is NULL\n", STR_SERVICE_INTERFACE_NAME);
    }   
    memcpy(InterfaceName,conf->InterfaceName,MAX_SERVICE_IFACE_NAME_SIZE);
    InterfaceName[strlen(conf->InterfaceName)] = '\0';
    sprintf(temp, "%s:%s", sectionname, STR_SERVICE_INTERFACE_NAME);
    iniparser_setstr(d, temp, InterfaceName);

    //NonSecureAccessPort
    sprintf(temp,"%s:%s",sectionname, STR_SERVICE_NONSECURE_PORT);
    sprintf(tempval, "%u", conf->NonSecureAccessPort);
    iniparser_setstr(d, temp, tempval);

    //SecureAccessPort
    sprintf(temp,"%s:%s",sectionname, STR_SERVICE_SECURE_PORT);
    sprintf(tempval, "%u", conf->SecureAccessPort);
    iniparser_setstr(d, temp, tempval);


    if (ENABLED == timeoutd_sess_timeout)
    {
        /* In timeoutd there is not difference between ssh and telnet timeout.
         * Also it sets the timeout in minutes, so timeout should be in multiple of 60.
         * It will set the timeout in configuration file
         */
        if (strcmp(service_name, TELNET_SERVICE_NAME) == 0)
        {
            //SessionInactivityTimeout
            conf->SessionInactivityTimeout = ((conf->SessionInactivityTimeout+59)/60) * 60;
            sprintf(temp,"%s:%s", SSH_SERVICE_NAME, STR_SERVICE_SESSION_TIMEOUT);
            sprintf(tempval, "%u", conf->SessionInactivityTimeout);
            iniparser_setstr(d, temp, tempval);

            if(isNotEditable((INT8U *)&conf->SessionInactivityTimeout,sizeof(conf->SessionInactivityTimeout)) 
                    && !isNotApplicable((INT8U *)&conf->SessionInactivityTimeout,sizeof(conf->SessionInactivityTimeout)))
            {
                getNotEditableData((INT8U *)&conf->SessionInactivityTimeout,sizeof(conf->SessionInactivityTimeout),NULL);
            }

            if (update_timeout_conf(conf->SessionInactivityTimeout/60) != 0)
            {
                TCRIT("Could not update timeout conf file\n");
                iniparser_freedict(d);
                ReleaseSemaphore(semaphore,&semaphoreKey[0]);
                return -1;
            }
        }
        else if (strcmp(service_name, SSH_SERVICE_NAME) == 0)
        {
            //SessionInactivityTimeout
            conf->SessionInactivityTimeout = ((conf->SessionInactivityTimeout+59)/60) * 60;
            sprintf(temp,"%s:%s", TELNET_SERVICE_NAME, STR_SERVICE_SESSION_TIMEOUT);
            sprintf(tempval, "%u", conf->SessionInactivityTimeout);
            iniparser_setstr(d, temp, tempval);

            if(isNotEditable((INT8U *)&conf->SessionInactivityTimeout,sizeof(conf->SessionInactivityTimeout)) 
                    && !isNotApplicable((INT8U *)&conf->SessionInactivityTimeout,sizeof(conf->SessionInactivityTimeout)))
            {
                getNotEditableData((INT8U *)&conf->SessionInactivityTimeout,sizeof(conf->SessionInactivityTimeout),NULL);
            }

            if (update_timeout_conf(conf->SessionInactivityTimeout/60) != 0)
            {
                TCRIT("Could not update timeout conf file\n");
                iniparser_freedict(d);
                ReleaseSemaphore(semaphore,&semaphoreKey[0]);
                return -1;
            }
        }
    }

    //SessionInactivityTimeout
    sprintf(temp,"%s:%s",sectionname, STR_SERVICE_SESSION_TIMEOUT);
    sprintf(tempval, "%u", conf->SessionInactivityTimeout);
    iniparser_setstr(d, temp, tempval);

    //MaxAllowSession
    sprintf(temp,"%s:%s",sectionname, STR_SERVICE_MAX_SESSIONS);
    sprintf(tempval, "%u", conf->MaxAllowSession);
    iniparser_setstr(d, temp, tempval);

    //CurrentActiveSession
    sprintf(temp,"%s:%s",sectionname, STR_SERVICE_ACTIVE_SESSIONS);
    sprintf(tempval, "%u", conf->CurrentActiveSession);
    iniparser_setstr(d, temp, tempval);

    fp = fopen(SERVICE_CONF_FILE,"w");
    if(fp == NULL)
    {
        printf("Could not open config file %s to set config\n", SERVICE_CONF_FILE);
        iniparser_freedict(d);
        ReleaseSemaphore(semaphore,&semaphoreKey[0]);
        return -1;
    }

    iniparser_dump_ini(d,fp);
    fclose(fp);
    iniparser_freedict(d);

    ReleaseSemaphore(semaphore,&semaphoreKey[0]);

    return 0;
}


/*
 *@fn check_service_conf_file
 *@brief This function should check if the service configuration file exists,
 *       and revert it back to default if it doesn't exist. 
 * 
 *@return Returns -1 on failure
 *        Returns 0 on success, file exists.
 *        Returns 1 on success, revert file to default.
 */
int check_service_conf_file(void)
{
    struct stat buf;
    char *buff=NULL;
    FILE *createfd,*srcfd;
    unsigned long fsize;

    /* if service conf doesn't exist, revert it from the default backup */
    if (stat(SERVICE_CONF_FILE, &buf) == -1)
    {
        srcfd = fopen(DEFAULT_SERVICE_CONF_FILE,"rb");
        if(srcfd == NULL)
        {
            printf("Unable to open %s \n",DEFAULT_SERVICE_CONF_FILE);
            return -1;
        }

        fseek(srcfd,0,SEEK_END);
        fsize = ftell(srcfd);
        fseek(srcfd,0,SEEK_SET);

        buff = malloc(fsize);
        if(buff == NULL)
        {
            TCRIT("Error in allocating memory to hold service Configurations \n");
            fclose(srcfd);
            return -1;
        }

        if(fsize != fread(buff,sizeof(char),fsize,srcfd))
        {
            TCRIT("Error in reading service configurations from %s \n",DEFAULT_SERVICE_CONF_FILE);
            fclose(srcfd);
            free(buff);
            return -1;
        }

        createfd = fopen((char *)SERVICE_CONF_FILE,"wb+");
        if(createfd == NULL)
        {
            TCRIT("Unable to create %s\n",SERVICE_CONF_FILE);
            fclose(srcfd);
            free(buff);
            return -1;
        }

        if(fsize != fwrite(buff,sizeof(char),fsize,createfd))
        {
            TCRIT("Error in writing service configurations to %s",SERVICE_CONF_FILE);
            fclose(srcfd);
            fclose(createfd);
            free(buff);
            return -1;
        }

        fclose(srcfd);
        fclose(createfd);
        free(buff);
        TDBG("Successful revert default service configurations to %s",SERVICE_CONF_FILE);
        return 1;
    }

    /* service conf exist */
    return 0;
}


/*
 *@fn create_service_defconf_file
 *@brief This function should create the service configuration file 
 *       with default values within the default conf table
 * 
 *@return Returns -1 on failure
 *        Returns 0 on success
 */
int create_service_defconf_file(void)
{
    int i;
    FILE* fp;

    fp = fopen(SERVICE_CONF_FILE,"w");
    if(fp == NULL)
    {
        TCRIT("Could not open %s config file to write defaults with default values\n", SERVICE_CONF_FILE);
        return -1;
    }
    else
    {
        for(i=0;i<MAX_SERVICE_NUM;i++)
        {
            if( (g_ServiceDefConfTbl[i].ServiceName == NULL) || 
                    (strlen(g_ServiceDefConfTbl[i].ServiceName) == 0) )
            {   
                break;
            }
            fprintf(fp,"[%s]\n", g_ServiceDefConfTbl[i].ServiceName);
            fprintf(fp,"%s=%s\n",STR_SERVICE_NAME,g_ServiceDefConfTbl[i].ServiceName);
            fprintf(fp,"%s=%u\n",STR_SERVICE_CURRENT_STATE,g_ServiceDefConfTbl[i].CurrentState);
            fprintf(fp,"%s=%s\n",STR_SERVICE_INTERFACE_NAME,g_ServiceDefConfTbl[i].InterfaceName);
            fprintf(fp,"%s=%u\n",STR_SERVICE_NONSECURE_PORT,g_ServiceDefConfTbl[i].NonSecureAccessPort);
            fprintf(fp,"%s=%u\n",STR_SERVICE_SECURE_PORT,g_ServiceDefConfTbl[i].SecureAccessPort);
            fprintf(fp,"%s=%u\n",STR_SERVICE_SESSION_TIMEOUT,g_ServiceDefConfTbl[i].SessionInactivityTimeout);
            fprintf(fp,"%s=%u\n",STR_SERVICE_MAX_SESSIONS,g_ServiceDefConfTbl[i].MaxAllowSession);
            fprintf(fp,"%s=%u\n",STR_SERVICE_ACTIVE_SESSIONS,g_ServiceDefConfTbl[i].CurrentActiveSession);

            fprintf(fp,"\n");
        }
    }

    fclose(fp);
    return 0;
}



/*
 *@fn search_default_tbl_id
 *@brief This function should get the id number of the default value table of service configurations
 *@param service_name - service name 
 *@param id - table id
 *@return Returns -1 on failure
 *        Returns 0 on success
 */
    static 
int search_default_tbl_id(char *service_name, INT8U id)
{
    int i = 0;
    int len = 0;

    for(i=0;i<MAX_SERVICE_NUM;i++)
    {
        len = strlen(g_ServiceDefConfTbl[i].ServiceName);

        if( (g_ServiceDefConfTbl[i].ServiceName == NULL) || (len == 0) )
        {   
            break;
        }

        if(strncmp(service_name, g_ServiceDefConfTbl[i].ServiceName, len) == 0)
        {   
            id=i;
            return 0;
        }
    }

    return -1;  
}

/*
 *@fn init_service_configurations
 *@brief This function is for reading and checking values of service configurations.
 *       It should get service configuration for initialization of service application.
 *       If it cannot get meaningful value via get_service_configuration, 
 *       it will assign default value from table of the service default conf.
 *       And set it back to ncml configuration file via set_service_configuration. 
 *@param service_name - service name 
 *@param conf - service conf
 *@return Returns ERR_LOAD_DEFCONF - it cannot get service conf and cannot find default value table either.
 *        Returns ERR_GET_CONF - get service configurations fail.
 *        Returns ERR_GET_DEFCONF - get default service configurations fail
 *        Returns ERR_SET_CONF - set default service configuration back to conf file fail 
 *        Returns COMPLETION_SUCCESS - all success
 */
int init_service_configurations(char *service_name, SERVICE_CONF_STRUCT* conf,int timeoutd_sess_timeout)
{
    int ret = 0, ret1 = 0, ret2 = 0;
    INTU err_value = 0xFFFFFFFE;
    INT8U id = 0;
    INT8U err_value_flag = 0;
    SERVICE_CONF_STRUCT* defconf;

    ret1 = check_service_conf_file();
    if(ret1 < 0)
    {   /* Create all service configurations with default configuration table */
        ret2 = create_service_defconf_file();
        if(ret2 < 0)
        {   /* Don't have any NCML conf at all, please assign default value by service application itself */
            return ERR_LOAD_DEFCONF; 
        }
    }

    memset(conf, 0, sizeof(SERVICE_CONF_STRUCT));
    ret = get_service_configurations(service_name, conf);
    if(ret < 0)
    {
        return ERR_GET_CONF; /* cannot get service conf */
    }

    /* Get default service values from defconf table */
    if(search_default_tbl_id(service_name, id) < 0)
    {  
        TDBG("Read service configuration success, but get default values fail.");
        TDBG("Please check error value by service application itself.");
        return ERR_GET_DEFCONF;
    }
    else
        defconf = (SERVICE_CONF_STRUCT*)&g_ServiceDefConfTbl[id];   

    /*
     * Check all configurations that were reading from configration file, 
     * and assign a default value if read error.
     */ 
    //ServiceName
    if(strlen(conf->ServiceName) == 0 || (conf->ServiceName == NULL))
    {       
        TDBG("Unable to get service_name, use default value %s", defconf->ServiceName);    
        strncpy(conf->ServiceName,defconf->ServiceName,strlen(defconf->ServiceName));
        err_value_flag = 1;
    }

    //CurrentState
    if(conf->CurrentState == (unsigned char)err_value)
    {
        TDBG("Unable to get current_state, use default value %d",defconf->CurrentState);    
        conf->CurrentState = defconf->CurrentState;
        err_value_flag = 1;  
    } 

    //InterfaceName
    if(strlen(conf->InterfaceName) == 0 || (conf->InterfaceName == NULL))
    {       
        TDBG("Unable to get interface_name, use default value %s",defconf->InterfaceName);    
        strncpy(conf->InterfaceName,defconf->InterfaceName,strlen(defconf->InterfaceName));
        err_value_flag = 1;  
    }

    //NonSecureAccessPort
    if(conf->NonSecureAccessPort == (unsigned long)err_value)
    {
        TDBG("Unable to get nonscecure_port, use default value %d",defconf->NonSecureAccessPort);
        conf->NonSecureAccessPort = defconf->NonSecureAccessPort;
        err_value_flag = 1;  
    }    

    //SecureAccessPort
    if(conf->SecureAccessPort == (unsigned long)err_value)
    {
        TDBG("Unable to get secure_port, use default value %d",defconf->SecureAccessPort);
        conf->SecureAccessPort = defconf->SecureAccessPort;
        err_value_flag = 1;  
    }

    //SessionInactivityTimeout
    if (conf->SessionInactivityTimeout == err_value)
    {
        TDBG("Unable to get session_timeout, use default value %d",defconf->SessionInactivityTimeout);
        conf->SessionInactivityTimeout = defconf->SessionInactivityTimeout;
        err_value_flag = 1;  
    }

    //MaxAllowSession
    if(conf->MaxAllowSession == (unsigned char)err_value)
    {
        TDBG("Unable to get max_sessions, use default value %d",defconf->MaxAllowSession);
        conf->MaxAllowSession = defconf->MaxAllowSession;
        err_value_flag = 1;  
    }

    //CurrentActiveSession
    /*CurrentActiveSession has to be 0 at service start*/
    if(conf->CurrentActiveSession != defconf->CurrentActiveSession)
    {
        TDBG("active_sessions isn't %d, use default value %d", defconf->CurrentActiveSession, defconf->CurrentActiveSession);    
        /* set active_sessions to default_active_sessions */
        conf->CurrentActiveSession = defconf->CurrentActiveSession;
        err_value_flag = 1;  
    }

    /*If any default setting has been used for service, set it back to ncml configuration file.*/
    if(err_value_flag != 0)
    {
        ret = set_service_configurations(service_name, conf,timeoutd_sess_timeout);
        if(ret != 0)
        {
            TDBG("Error setting service configuration from ncml library.\n");
            return ERR_SET_CONF; /* Almost success except the final set action */
        }
    }

    return COMPLETION_SUCCESS;
}
/*
 *@fn Validate_SetServiceConf_Req_Parameter
 *@brief   It will validate the data requested by user to set in SetServiceConf command.
 *       It will not allow user to set port no which is already in use by other service and 
 *       also it will not allow user to set SessionInactivityTimeout other than specified range in ncml.conf file
 *@param ServiceName - service name 
 *@param SessionInactivityTimeout - session inactivity timeout
 *@param SecureAccessPort- secure port no
 *@param NonSecureAccessPort- non secure port no
 *@return Returns 
 *        Returns NCML_ERR_INVALID_PORT- both secure and nonsecure port are same
 *        Returns NCML_ERR_READ_NCML_CONF_ - error while reading ncml conf file.
 *        Returns NCML_ERR__INVALID_SESSION_INACTIVE_TIMEOUT - session inactive timeout value is not valid
 *        Returns NCML_ERR_PORT_ALREADY_IN_USE -port no is already in use. 
 *        Returns ERR_LOAD_CONF - if unable to load ncml.conf file
 *        Returns COMPLETION_SUCCESS - all success
 */
int Validate_SetServiceConfiguration(SERVICE_CONF_STRUCT *ReqConf)
{
    INTU TempMaxSessionTimeout,TempMinSessionTimeout,ServiceSessionTimeout;
    INT32U ServiceSecureAccessPort,ServiceNonSecureAccessPort;
    INT32U TempSecureAccessPort,TempNonSecureAccessPort;
    INT8U NoOfSection,ServiceCount,ServiceMaxSession,ServiceActiveSession;
    INTU TempVal;
    char *TempServiceName=NULL; 
    INTU Err = 0xFFFFFFFE;
    dictionary *d = NULL;
    INT8S   InterfaceName[MAX_SERVICE_IFACE_NAME_SIZE+1]; 
    char *str=NULL;
    char temp[MAX_TEMP_ARRAY_SIZE];
    int ifcount,j;
    char  *up_interfaces;

    memset(InterfaceName,0,sizeof(InterfaceName));   

    if(ReqConf == NULL)
    {
        TDBG("Request Configurations are NULL\n");
        return -1;
    } 
    if ((ReqConf->SecureAccessPort == ReqConf->NonSecureAccessPort) && 
            (ReqConf->SecureAccessPort != 0) && 
            (ReqConf->NonSecureAccessPort != 0))
    {
        return(NCML_ERR_INVALID_PORT);
    }

    /*Validation for Interface name*/
    if( !isStringNotApplicable(ReqConf->InterfaceName,MAX_SERVICE_IFACE_NAME_SIZE )) 
    {
        if(strncmp(ReqConf->InterfaceName,"both",sizeof(ReqConf->InterfaceName)) != 0)
        {
            if(get_network_interface_count(& ifcount) != 0)
            {
                TCRIT("Error in getting the interface count\n");
                return -1;
            }

            up_interfaces=malloc(MAX_ETHIFC_LEN * ifcount);
            if(up_interfaces==NULL)
            {
                TCRIT("unable to allocate the memory for interface name\n");
                return -1;
            }

            memset(up_interfaces,0,MAX_ETHIFC_LEN * ifcount);
            if(get_network_interfaces_name(up_interfaces ,ifcount) != 0)
            {
                free(up_interfaces);
                return -1;
            }
            for(j=0;j<ifcount;j++)
            {
                if( memcmp(ReqConf->InterfaceName,&up_interfaces[j*MAX_ETHIFC_LEN ],MAX_ETHIFC_LEN)  == 0)
                {
                    TDBG("Interface name is valide\n");
                    break;
                }
            }

            if(j == ifcount)
            {
                TDBG("Invalide interface name\n");
                free(up_interfaces);
                return(NCML_ERR_INVALID_INTERFACE_NAME);
            }
            free(up_interfaces);
        }
    }

    d = iniparser_load(SERVICE_CONF_FILE);
    if(d== NULL)
    {
        TDBG("Unable to find/load/parse NCML Configuration file :%s",SERVICE_CONF_FILE);
        return (ERR_LOAD_CONF);
    }
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%s:%s",ReqConf->ServiceName, STR_SERVICE_INTERFACE_NAME);
    str = iniparser_getstr (d,temp);
    if(str == NULL)
    {
        TDBG("Configuration %s is not found\n", STR_SERVICE_INTERFACE_NAME);
        memset(InterfaceName,'\0',MAX_SERVICE_IFACE_NAME_SIZE);
    }
    else
    {  
        strncpy(InterfaceName,str,MAX_SERVICE_IFACE_NAME_SIZE);
        InterfaceName[strlen(InterfaceName)]='\0';
        TDBG("%s = %s \n",STR_SERVICE_INTERFACE_NAME, str);
    }

    ServiceSecureAccessPort = IniGetUInt(d,ReqConf->ServiceName,STR_SERVICE_SECURE_PORT,Err);
    if(ServiceSecureAccessPort == Err)
    {
        TDBG("Configuration %s is not found\n",STR_SERVICE_SECURE_PORT);
        iniparser_freedict(d);
        return(NCML_ERR_READ_NCML_CONF);
    }
    else
    {
        TDBG("Configured %s value is %lx\n",STR_SERVICE_SECURE_PORT,ServiceSecureAccessPort);
    } 

    ServiceNonSecureAccessPort = IniGetUInt(d,ReqConf->ServiceName,STR_SERVICE_NONSECURE_PORT,Err);
    if(ServiceNonSecureAccessPort == Err)
    {
        iniparser_freedict(d);
        return(NCML_ERR_READ_NCML_CONF);
    }
    else
    {
        TDBG("Configured %s value is %lx\n",STR_SERVICE_NONSECURE_PORT,ServiceNonSecureAccessPort);
    } 

    TempVal = IniGetUInt(d,ReqConf->ServiceName,STR_SERVICE_MAX_SESSIONS,Err);
    if(TempVal == Err)
    {
        TDBG("Configuration %s is not found\n",STR_SERVICE_MAX_SESSIONS);
        iniparser_freedict(d);
        return(NCML_ERR_READ_NCML_CONF);
    }
    else
    {
        ServiceMaxSession = (INT8U)TempVal;
        TDBG("Configured %s value is %x\n",STR_SERVICE_MAX_SESSIONS,ServiceMaxSession);
    } 

    TempVal = IniGetUInt(d,ReqConf->ServiceName,STR_SERVICE_ACTIVE_SESSIONS,Err);
    if(TempVal == Err)
    {
        TDBG("Configuration %s not found\n",STR_SERVICE_ACTIVE_SESSIONS);
        iniparser_freedict(d);
        return(NCML_ERR_READ_NCML_CONF);
    }
    else
    {
        ServiceActiveSession = (INT8U)TempVal;
        TDBG("Configured %s value is %x\n",STR_SERVICE_ACTIVE_SESSIONS,ServiceActiveSession);
    }

    // Validate Inactivity timeout
    TempMaxSessionTimeout = IniGetUInt(d,ReqConf->ServiceName,STR_MAX_SESSION_INACTIVITY_TIMEOUT,Err);
    if(TempMaxSessionTimeout == Err)
    {
        TDBG("Configuration %s is not found\n", STR_MAX_SESSION_INACTIVITY_TIMEOUT);
        iniparser_freedict(d);
        return( NCML_ERR_READ_NCML_CONF);
    }
    else
    {
        TDBG("Configured %s value is %x \n", STR_MAX_SESSION_INACTIVITY_TIMEOUT, TempMaxSessionTimeout);
    }

    TempMinSessionTimeout = IniGetUInt(d,ReqConf->ServiceName,STR_MIN_SESSION_INACTIVITY_TIMEOUT,Err);  
    if(TempMinSessionTimeout == Err)
    {
        TDBG("Configuration %s is not found\n", STR_MIN_SESSION_INACTIVITY_TIMEOUT);
        iniparser_freedict(d);
        return(NCML_ERR_READ_NCML_CONF);
    }
    else
    {
        TDBG("Configured %s value is %x \n", STR_MIN_SESSION_INACTIVITY_TIMEOUT, TempMinSessionTimeout);
    }

    ServiceSessionTimeout = IniGetUInt(d,ReqConf->ServiceName,STR_SERVICE_SESSION_TIMEOUT,Err);
    if(ServiceSessionTimeout == Err)
    {
        TDBG("Configuration %s is not found\n",STR_SERVICE_SESSION_TIMEOUT);
        iniparser_freedict(d);
        return(NCML_ERR_READ_NCML_CONF);
    }
    else
    {
        TDBG("Configured %s value is %x \n",STR_SERVICE_SESSION_TIMEOUT,ServiceSessionTimeout);
    }

    if(isStringNotApplicable(InterfaceName,MAX_SERVICE_IFACE_NAME_SIZE) 
            && !isStringNotApplicable(ReqConf->InterfaceName,MAX_SERVICE_IFACE_NAME_SIZE))
    {
        iniparser_freedict(d);
        return(NCML_ERR_NCMLCONFIG_NA);
    }

    if(isNotApplicable((INT8U *)&ServiceNonSecureAccessPort,sizeof(ServiceNonSecureAccessPort)) 
            && !isNotApplicable((INT8U *)&ReqConf->NonSecureAccessPort,sizeof(ReqConf->NonSecureAccessPort)))
    {
        iniparser_freedict(d);
        return(NCML_ERR_NCMLCONFIG_NA);
    }

    if((ReqConf->NonSecureAccessPort != 0) && !isNotApplicable((INT8U *)&ServiceNonSecureAccessPort,sizeof(ServiceNonSecureAccessPort)) 
            && isNotEditable((INT8U *)&ServiceNonSecureAccessPort,sizeof(ServiceNonSecureAccessPort)))
    {
        iniparser_freedict(d);
        return(NCML_ERR_NCMLCONFIG_NE);
    }
    if(((ReqConf->NonSecureAccessPort > MAX_PORT_NUMBER)||(ReqConf->NonSecureAccessPort == 0)) 
            && !isNotEditable((INT8U *)&ServiceNonSecureAccessPort,sizeof(ServiceNonSecureAccessPort)))
    {
        iniparser_freedict(d);
        return(NCML_ERR_INVALID_PORT);
    }
    if(isNotEditable((INT8U *)&ServiceNonSecureAccessPort,sizeof(ServiceNonSecureAccessPort)) && (ReqConf->NonSecureAccessPort == 0))
    {
        ReqConf->NonSecureAccessPort = ServiceNonSecureAccessPort;
    }

    if(isNotApplicable((INT8U *)&ServiceSecureAccessPort,sizeof(ServiceSecureAccessPort)) 
            && !isNotApplicable((INT8U *)&ReqConf->SecureAccessPort,sizeof(ReqConf->SecureAccessPort)))
    {
        iniparser_freedict(d);
        return(NCML_ERR_NCMLCONFIG_NA);
    }

    if((ReqConf->SecureAccessPort != 0) && !isNotApplicable((INT8U *)&ServiceSecureAccessPort,sizeof(ServiceSecureAccessPort)) 
            && isNotEditable((INT8U *)&ServiceSecureAccessPort,sizeof(ServiceSecureAccessPort)))
    {
        iniparser_freedict(d);
        return(NCML_ERR_NCMLCONFIG_NE);
    }
    if(((ReqConf->SecureAccessPort > MAX_PORT_NUMBER)||(ReqConf->SecureAccessPort == 0)) 
            && !isNotEditable((INT8U *)&ServiceSecureAccessPort,sizeof(ServiceSecureAccessPort)))
    {
        iniparser_freedict(d);
        return(NCML_ERR_INVALID_PORT);
    }
    if(isNotEditable((INT8U *)&ServiceSecureAccessPort,sizeof(ServiceSecureAccessPort)) && (ReqConf->SecureAccessPort == 0))
    {
        ReqConf->SecureAccessPort = ServiceSecureAccessPort;
    }


    if((ReqConf->SessionInactivityTimeout != 0) && !isNotApplicable((INT8U *)&ServiceSessionTimeout,sizeof(ServiceSessionTimeout))
            && isNotEditable((INT8U *)&ServiceSessionTimeout,sizeof(ServiceSessionTimeout)))
    {
        iniparser_freedict(d);
        return(NCML_ERR_NCMLCONFIG_NE);
    }
    if(isNotEditable((INT8U *)&ServiceSessionTimeout,sizeof(ServiceSessionTimeout)) && (ReqConf->SessionInactivityTimeout == 0))
    {
        ReqConf->SessionInactivityTimeout = ServiceSessionTimeout;
    }
    if(((ReqConf->SessionInactivityTimeout > TempMaxSessionTimeout)||(ReqConf->SessionInactivityTimeout <  TempMinSessionTimeout) 
                ||(ReqConf->SessionInactivityTimeout > PORT_VAL_BYTE)) && !isNotEditable((INT8U *)&ServiceSessionTimeout,sizeof(ServiceSessionTimeout)))
    {
        iniparser_freedict(d);
        return(NCML_ERR_INVALID_SESSION_INACTIVE_TIMEOUT);
    }

    if(isNotApplicable((INT8U *)&ServiceMaxSession,sizeof(ServiceMaxSession)) && !isNotApplicable((INT8U *)&ReqConf->MaxAllowSession,sizeof(ReqConf->MaxAllowSession)))
    {
        iniparser_freedict(d);
        return(NCML_ERR_NCMLCONFIG_NA);
    }
    if(isNotEditable((INT8U *)&ServiceMaxSession,sizeof(ServiceMaxSession)) 
            && (ReqConf->MaxAllowSession != 0) && !isNotApplicable((INT8U *)&ServiceMaxSession,sizeof(ServiceMaxSession)))
    {
        iniparser_freedict(d);
        return(NCML_ERR_NCMLCONFIG_NE);
    }
    if(!isNotEditable((INT8U *)&ServiceMaxSession,sizeof(ServiceMaxSession)) 
            &&((ReqConf->MaxAllowSession > MAX_SESSION_COUNT)||(ReqConf->MaxAllowSession == 0)))
    {
        iniparser_freedict(d);
        return(NCML_ERR_SESSION_INVALID_COUNT);
    }
    if(isNotEditable((INT8U *)&ServiceMaxSession,sizeof(ServiceMaxSession)) && (ReqConf->MaxAllowSession == 0))
    {
        ReqConf->MaxAllowSession = ServiceMaxSession;
    }

    if(isNotApplicable((INT8U *)&ServiceSessionTimeout,sizeof(ServiceSessionTimeout)) && !isNotApplicable((INT8U *)&ReqConf->SessionInactivityTimeout,sizeof(ReqConf->SessionInactivityTimeout)))
    {
        iniparser_freedict(d);
        return(NCML_ERR_NCMLCONFIG_NA);
    }


    if(isNotApplicable((INT8U *)&ServiceActiveSession,sizeof(ServiceActiveSession)) 
            && !isNotApplicable((INT8U *)&ReqConf->CurrentActiveSession,sizeof(ReqConf->CurrentActiveSession)))
    {
        iniparser_freedict(d);
        return(NCML_ERR_NCMLCONFIG_NA);
    }
    if(isNotEditable((INT8U *)&ServiceActiveSession,sizeof(ServiceActiveSession)) && 
            (ReqConf->CurrentActiveSession != 0) && !isNotApplicable((INT8U *)&ServiceActiveSession,sizeof(ServiceActiveSession)))
    {
        iniparser_freedict(d);
        return(NCML_ERR_NCMLCONFIG_NE);
    }
    if( !isNotEditable((INT8U *)&ServiceActiveSession,sizeof(ServiceActiveSession)) && 
            ((ReqConf->CurrentActiveSession > MAX_SESSION_COUNT)||(ReqConf->CurrentActiveSession == 0)))
    {
        iniparser_freedict(d);
        return(NCML_ERR_SESSION_INVALID_COUNT);
    }
    if(isNotEditable((INT8U *)&ServiceActiveSession,sizeof(ServiceActiveSession)) &&
            (ReqConf->CurrentActiveSession == 0))
    {
        ReqConf->CurrentActiveSession = ServiceActiveSession;
    }

    // validate Secure port and Non secure port
    NoOfSection=  iniparser_getnsec(d);

    for(ServiceCount=0;ServiceCount<NoOfSection;ServiceCount++)
    {
        TempServiceName= iniparser_getsecname (d,ServiceCount);
        if((TempServiceName == NULL) || (strcmp(TempServiceName,ReqConf->ServiceName) == 0))
        {
            continue;
        }

        TempSecureAccessPort = IniGetUInt(d,TempServiceName,STR_SERVICE_SECURE_PORT,Err);  
        if(TempSecureAccessPort == Err)
        {
            TDBG("Configuration %s is not found\n", STR_SERVICE_SECURE_PORT);
            iniparser_freedict(d);
            return( NCML_ERR_READ_NCML_CONF);
        }
        else
        {
            TDBG("Configured %s value is %x \n", STR_SERVICE_SECURE_PORT, TempSecureAccessPort);
        }

        TempNonSecureAccessPort = IniGetUInt(d,TempServiceName,STR_SERVICE_NONSECURE_PORT,Err);  
        if(TempVal == Err)
        {
            TDBG("Configuration %s is not found\n",STR_SERVICE_NONSECURE_PORT);
            iniparser_freedict(d);
            return( NCML_ERR_READ_NCML_CONF);
        }
        else
        {
            TDBG("Configured %s value is %x \n", STR_SERVICE_NONSECURE_PORT, TempNonSecureAccessPort);
        }

        if(isNotEditable((INT8U *)&TempSecureAccessPort,sizeof(TempSecureAccessPort)) && 
                !isNotApplicable((INT8U *)&TempSecureAccessPort,sizeof(TempSecureAccessPort)))
        {
            getNotEditableData((INT8U *)&TempSecureAccessPort,sizeof(TempNonSecureAccessPort),NULL);

        }
        if(isNotEditable((INT8U *)&TempNonSecureAccessPort,sizeof(TempNonSecureAccessPort)) && 
                !isNotApplicable((INT8U *)&TempNonSecureAccessPort,sizeof(TempNonSecureAccessPort)))
        {
            getNotEditableData((INT8U *)&TempNonSecureAccessPort,sizeof(TempNonSecureAccessPort),NULL);
        }

        if(isNotEditable((INT8U *)&ReqConf->SecureAccessPort,sizeof(ReqConf->SecureAccessPort)) && 
                !isNotApplicable((INT8U *)&ReqConf->SecureAccessPort,sizeof(ReqConf->SecureAccessPort)))
        {
            getNotEditableData((INT8U *)&ReqConf->SecureAccessPort,sizeof(ReqConf->SecureAccessPort),(INT8U *)&ServiceSecureAccessPort);
        }
        else
        {
            ServiceSecureAccessPort = ReqConf->SecureAccessPort;
        }
        if(isNotEditable((INT8U *)&ReqConf->NonSecureAccessPort,sizeof(ReqConf->NonSecureAccessPort)) && 
                !isNotApplicable((INT8U *)&ReqConf->NonSecureAccessPort,sizeof(ReqConf->NonSecureAccessPort)))
        {
            getNotEditableData((INT8U *)&ReqConf->NonSecureAccessPort,sizeof(ReqConf->SecureAccessPort),(INT8U *)&ServiceNonSecureAccessPort);
        }
        else
        {
            ServiceNonSecureAccessPort = ReqConf->NonSecureAccessPort;
        }


        if((((TempSecureAccessPort==ServiceSecureAccessPort)||(TempSecureAccessPort==ServiceNonSecureAccessPort)) 
                    && !isNotApplicable((INT8U *)&TempSecureAccessPort,sizeof(TempSecureAccessPort)))||(((TempNonSecureAccessPort==ServiceSecureAccessPort)
                        ||(TempNonSecureAccessPort==ServiceNonSecureAccessPort)) && !isNotApplicable((INT8U *)&TempNonSecureAccessPort,sizeof(TempNonSecureAccessPort))))
        {
            TDBG("Ports Already in use\n");       
            iniparser_freedict(d);
            return(NCML_ERR_PORT_ALREADY_IN_USE);
        }
    }
    iniparser_freedict(d);
    return (COMPLETION_SUCCESS) ;
}

/*@fn isNotApplicable
 *@brief This function checks whether the given data contains Not Applicable bytes
 *@param data - Pointer to the data
 *@param dataSize - Size of data
 *@return Returns 0 on failure
 *        Returns 1 on success
 */
int isNotApplicable(unsigned char *data, int dataSize)
{
    int j = 0;
    for(j = 0; j < dataSize; j++) {
        if (*(data + j) != NOT_APPLICABLE_BYTE) {
            return 0;
        }
    }
    return 1;
}

/*@fn isStringNotApplicable
 *@brief This function checks whether the given data contains Not Applicable string
 *@param data - Pointer to the data
 *@param size - length of the data
 *@return Returns 0 on Not Applicable
 *        Returns 1 on Applicable
 */
int isStringNotApplicable(char *data,int size)
{
    char temp[MAX_NA_ARRAY_SIZE];

    memset(temp,ASCII_F,sizeof(char)*size);
    temp[size] = '\0';
    TDBG("\n the data  %s \n",data);
    TDBG("\n the temp  %s \n",temp);

    if(strncmp(data, temp,size))
        return 0;
    else
        return 1; 
}


/*@fn isNotEditable
 *@brief This function checks whether first bit in the MSB is set to 1
 *@param data - Pointer to the data
 *@param dataSize - Size of data
 *@return Returns 0 on failure
 *        Returns 1 on success
 */
int isNotEditable(unsigned char *data, int dataSize)
{
    if (((*(data + (dataSize - 1))) & NOT_ENABLED_BYTE) == NOT_ENABLED_BYTE) {
        return 1;
    } else {
        return 0;
    }
}

/*@fn getNotEditableData
 *@brief This function is used to get the valid data from the masked data
 *       If validData is specified as NULL then the valid data is stored
 *       in masked data after unmasking
 *@param maskData - Pointer to the  masked data
 *@param validData - Pointer to the valid data 
 *@param dataSize - Size of data
 *@return Returns -0 on failure
 *        Returns 1 on success
 */
void getNotEditableData(unsigned char *maskData, int dataSize, unsigned char *validData)
{
    if(dataSize <= 0)
    {
        TDBG("Data Size Not Valid\n");
        return;
    }

    if((validData != NULL) && (maskData != NULL))
    {
        memcpy(validData, maskData, dataSize);
        *(validData + (dataSize - 1)) = (*(validData + (dataSize - 1))) & 0x7F;
    }
    else if(maskData != NULL)
    {
        *(maskData + (dataSize - 1)) = (*(maskData + (dataSize - 1))) & 0x7F;
    }
}

/*@fn getNotEditableMasksData
 *@brief This function is used to get the mask data from the valid data
 *       If maskData is specified as NULL then the masked data is stored
 *       in valid data after masking
 *@param maskData - Pointer to the  masked data
 *@param validData - Pointer to the valid data 
 *@param dataSize - Size of data
 *@return Returns -0 on failure
 *        Returns 1 on success
 */
void getNotEditableMaskData(unsigned char *validData, int dataSize, unsigned char *maskData)
{
    if(dataSize <= 0)
    {
        TDBG("Data Size Not Valid\n");
        return;
    }
    if((maskData != NULL) && (validData != NULL))
    {
        memcpy(maskData, validData, dataSize);
        *(maskData + (dataSize - 1)) = (*(maskData + (dataSize - 1))) | NOT_ENABLED_BYTE;
    }
    else if(validData != NULL)
    {
        *(validData + (dataSize - 1)) = (*(validData + (dataSize - 1))) | NOT_ENABLED_BYTE;
    }
}


