/*****************************************************************
 ******************************************************************
 ***                                                            ***
 ***        (C)Copyright 2008, American Megatrends Inc.         ***
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
 ******************************************************************/

/*----------------------------------------------------------*/
/* Function which fetch the corresponding Eth number for given channel */
/* Function name  :GetEthIndex                                                          */
/* Input parameter : Channel No                                                        */
/* Output parameter : EthIndex   if channel not present it will return 0xff */
/*----------------------------------------------------------*/
#include "Types.h"
#include"Debug.h"
#include "IPMIConf.h"
//#include "PDKAccess.h"
#include "nwcfg.h"
#include "iniparser.h"


int nwlinkfd = -1;

/**
 *@fn SplitEthIfcnames
 *@brief This function splits the EthIndex got from BMC.conf
 *@return Returns 0
 */
int SplitEthIfcnames(char *Ifcnames, int BMCInst,int *count)
{
    char *pch=NULL;
    BMCInfo_t *pBMCInfo = &g_BMCInfo[BMCInst];

    pch = strtok (Ifcnames,",");

    while(pch != NULL)
    {
        memcpy(&pBMCInfo->EthIndexValues[*count],pch,strlen(pch));
        (*count)++;
        pch = strtok (NULL, ",");
    }

    pBMCInfo->LANIfccount = *count;

    return 0;
}

/*
 *@fn GetEthIndex
 *@brief This function fetches EthIndex by giving Channel Number
 *@param Channel - Channel Number
 *@return Returns the EthIndex on sucess
 *            Returns 0xFF on failure
 */
INT8U GetEthIndex(INT8U Channel,int BMCInst)
{
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    int i=0,j;
    char IFname[16];

    memset(IFname,0,sizeof(IFname));
#if 0
    if(g_PDKHandle[PDK_GETETHINDEX] != NULL)
    {
        INT8U Retval = 0;
        Retval = ((int(*)(INT8U,int)) g_PDKHandle[PDK_GETETHINDEX]) (Channel,BMCInst);
        return Retval; 
    }
#endif
    for(i=0;i<MAX_LAN_CHANNELS;i++)
    {
        sprintf(IFname,"bond%d",i);
        for(j=0;j<MAX_LAN_CHANNELS;j++)
        {
            if((strcmp(IFname,pBMCInfo->LANConfig.LanIfcConfig[j].ifname) == 0) 
                    && (pBMCInfo->LANConfig.LanIfcConfig[j].Up_Status == 1))
            {
                printf("%s,%dxxxxxxxxxxxxxxxxxxxxxxxxxxoooooooooooooooooooooooo\n",__func__,__LINE__);
                if(pBMCInfo->LANConfig.LanIfcConfig[j].Chnum== Channel)
                    return pBMCInfo->LANConfig.LanIfcConfig[j].Ethindex;
            }
        }
    }

    for(i=0;i<sizeof(pBMCInfo->LANConfig.LanIfcConfig)/sizeof(LANIFCConfig_T);i++)
    {
        if(pBMCInfo->LANConfig.LanIfcConfig[i].Chnum == Channel)
        {
            return pBMCInfo->LANConfig.LanIfcConfig[i].Ethindex;
        }
    }


    return 0xff;
}

/*
 *@fn GetLANChannel
 *@brief EthIndex - This function fetches the Lan Channel number by giving EthIndex
 *@param EthIndex - Ethernet Index
 *@return Returns respective channel number on success
 *            Returns 0xFF on failure
 */
INT8U GetLANChannel(INT8U EthIndex, int BMCInst)
{

    INT8U ChannelNum=0xFF;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    int i=0,j;
    char IFname[16];

    memset(IFname,0,sizeof(IFname));
#if 0
    if(g_PDKHandle[PDK_GETLANCHANNEL] != NULL)
    {
        INT8U Retval = 0;
        Retval = ((int(*)(INT8U,int)) g_PDKHandle[PDK_GETLANCHANNEL]) (EthIndex,BMCInst);
        return Retval; 
    }
#endif
    for(i=0;i<MAX_LAN_CHANNELS;i++)
    {
        sprintf(IFname,"bond%d",i);
        for(j=0;j<MAX_LAN_CHANNELS;j++)
        {
            if((strcmp(IFname,pBMCInfo->LANConfig.LanIfcConfig[j].ifname) == 0) 
                    && (pBMCInfo->LANConfig.LanIfcConfig[j].Up_Status == 1))
            {
                printf("%s,%dxxxxxxxxxxxxxxxxxxxxxxxxxxoooooooooooooooooooooooo\n",__func__,__LINE__);
                if(pBMCInfo->LANConfig.LanIfcConfig[j].Ethindex == EthIndex)
                    return pBMCInfo->LANConfig.LanIfcConfig[j].Chnum;
            }
        }
    }


    for(i=0;i<sizeof(pBMCInfo->LANConfig.LanIfcConfig)/sizeof(LANIFCConfig_T);i++)
    {
        if(pBMCInfo->LANConfig.LanIfcConfig[i].Ethindex == EthIndex)
        {
            return pBMCInfo->LANConfig.LanIfcConfig[i].Chnum;
        }
    }

    return ChannelNum;

}

/*
 * @ fn GetIfcName
 * @ brief This function returns the interface name for EthIndex
 * @ param EthIndex [in] index value of interface
 * @ Param IfcName [out] interface name for specified index
 * @ Param BMCInst [in]. BMC Instance
 * @ return 0 on success, -1 on failure
 */
INT8U GetIfcName(INT8U EthIndex,char *IfcName,int BMCInst)
{
    int i,j;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    char IFname[16];

    memset(IFname,0,sizeof(IFname));
    for(i=0;i<MAX_LAN_CHANNELS;i++)
    {
        sprintf(IFname,"bond%d",i);
        for(j=0;j<MAX_LAN_CHANNELS;j++)
        {
            if((strcmp(IFname,pBMCInfo->LANConfig.LanIfcConfig[j].ifname) == 0) 
                    && (pBMCInfo->LANConfig.LanIfcConfig[j].Up_Status == 1)
                    && (pBMCInfo->LANConfig.LanIfcConfig[j].Ethindex == EthIndex))
            {
                printf("%s,%dxxxxxxxxxxxxxxxxxxxxxxxxxxoooooooooooooooooooooooo\n",__func__,__LINE__);
                memcpy(IfcName,pBMCInfo->LANConfig.LanIfcConfig[j].ifname,sizeof(IFname));
                return 0;
            }
        }
    }

    for(i=0;i<sizeof(pBMCInfo->LANConfig.LanIfcConfig)/sizeof(LANIFCConfig_T);i++)
    {
        if(pBMCInfo->LANConfig.LanIfcConfig[i].Ethindex == EthIndex)
        {
            printf("%s,%dxxxxxxxxxxxxxxxxxxxxxxxxxxoooooooooooooooooooooooo\n",__func__,__LINE__);
            memcpy(IfcName,pBMCInfo->LANConfig.LanIfcConfig[i].ifname,sizeof(IFname));
            printf("%s,%d,%s\n",__func__,__LINE__,IfcName);
            return 0;
        }
    }

    return -1;
}

/*
 * @ fn GetChannelByAddr
 * @ brief This function returns the channel number for given IPv4 Address
 * @ param IPAddr [in] IPv4 Address
 * @ Param BMCInst [in]. BMC Instance
 * @ return channel Number on success, -1 on failure
 */
int GetChannelByAddr(char * IPAddr,int BMCInst)
{
    int Ethindex=0,i;
    char IfcName[16];
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    Ethindex = GetHostEthbyIPAddr(IPAddr);
    if(Ethindex == -1)
    {
        return -1;
    }

    if(GetIfcNameByIndex(Ethindex, IfcName) == 1)
    {
        return -1;
    }

    for(i=0;i<MAX_LAN_CHANNELS;i++)
    {
        if(strcmp(IfcName,pBMCInfo->LANConfig.LanIfcConfig[i].ifname) == 0)
            return pBMCInfo->LANConfig.LanIfcConfig[i].Chnum;
    }

    return -1;
}

/*
 * @ fn GetChannelByIPv6Addr
 * @ brief This function returns the channel number for given IPv6 Address
 * @ param IPAddr [in] IPv6 Address
 * @ Param BMCInst [in]. BMC Instance
 * @ return channel Number on success, -1 on failure
 */
int GetChannelByIPv6Addr(char * IPAddr,int BMCInst)
{
    int Ethindex = 0,i;
    char IfcName[16];
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    Ethindex = GetHostEthByIPv6Addr(IPAddr);
    if(Ethindex == -1)
    {
        return -1;
    }

    if(GetIfcNameByIndex(Ethindex, IfcName) == 1)
    {
        return -1;
    }

    for(i=0;i<MAX_LAN_CHANNELS;i++)
    {
        if(strcmp(IfcName,pBMCInfo->LANConfig.LanIfcConfig[i].ifname) == 0)
        {printf("%s,%dxxxxxxxxxxxxxxxxxxxxxxxxxxoooooooooooooooooooooooo\n",__func__,__LINE__);
            return pBMCInfo->LANConfig.LanIfcConfig[i].Chnum;}
    }

    return -1;
}
/*
 *@fn IsLANChannel
 *@brief This function helps to find whether the given channel number is LAN Channel or not
 *@param Channel - Channel Number
 *@return Returns 1 on success
 *            Returns 0 on failure
 */
INT8U IsLANChannel(INT8U Channel, int BMCInst)
{
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];
    int i=0;

    for(i=0;i<sizeof(pBMCInfo->LANConfig.LanIfcConfig)/sizeof(LANIFCConfig_T);i++)
    {
        if(pBMCInfo->LANConfig.LanIfcConfig[i].Chnum == Channel)
        {
            return 1;
        }
    }

    return 0;
}

int InitDNSConfiguration(int BMCInst)
{
    HOSTNAMECONF    HostConf;
    DOMAINCONF      DomainConf;
    DNSCONF             DNSConf;
    INT8U                   RegBMC_FQDN[MAX_CHANNEL];
    int retval = 0;

    memset(&HostConf,0,sizeof(HOSTNAMECONF));
    memset(&DomainConf,0,sizeof(DOMAINCONF));
    memset(&DNSConf,0,sizeof(DNSCONF));
    memset(RegBMC_FQDN,0,MAX_CHANNEL);

    retval = nwGetAllDNSConf(&HostConf,&DomainConf,&DNSConf,RegBMC_FQDN);
    if(retval != 0)
    {
        TCRIT("Error in Getting All DNS configurtaion\n");
        return -1;
    }

    LOCK_BMC_SHARED_MEM(BMCInst);

    /*Update the HostName Settings*/
    BMC_GET_SHARED_MEM (BMCInst)->DNSconf.HostSetting = HostConf.HostSetting;
    BMC_GET_SHARED_MEM (BMCInst)->DNSconf.HostNameLen = HostConf.HostNameLen;
    memcpy(BMC_GET_SHARED_MEM (BMCInst)->DNSconf.HostName,HostConf.HostName,MAX_HOST_NAME_STRING_SIZE);

    /*Update the DNS Configurations*/
    BMC_GET_SHARED_MEM (BMCInst)->DNSconf.DNSDHCP = DNSConf.DNSDHCP;
    BMC_GET_SHARED_MEM (BMCInst)->DNSconf.DNSIndex = DNSConf.DNSIndex;
    BMC_GET_SHARED_MEM (BMCInst)->DNSconf.IPPriority = DNSConf.IPPriority;

    memcpy(BMC_GET_SHARED_MEM (BMCInst)->DNSconf.DNSIPAddr1,DNSConf.DNSIP1,IP6_ADDR_LEN);
    memcpy(BMC_GET_SHARED_MEM (BMCInst)->DNSconf.DNSIPAddr2,DNSConf.DNSIP2,IP6_ADDR_LEN);
    memcpy(BMC_GET_SHARED_MEM (BMCInst)->DNSconf.DNSIPAddr3,DNSConf.DNSIP3,IP6_ADDR_LEN);

    /*Update the Domain Configurations*/
    BMC_GET_SHARED_MEM (BMCInst)->DNSconf.DomainDHCP = DomainConf.dhcpEnable;
    BMC_GET_SHARED_MEM (BMCInst)->DNSconf.DomainIndex = DomainConf.EthIndex;
    BMC_GET_SHARED_MEM (BMCInst)->DNSconf.Domainpriority = DomainConf.v4v6;
    BMC_GET_SHARED_MEM (BMCInst)->DNSconf.DomainLen = DomainConf.domainnamelen;
    memcpy(BMC_GET_SHARED_MEM (BMCInst)->DNSconf.DomainName,DomainConf.domainname,MAX_DOMAIN_NAME_STRING_SIZE);

    /*Update the Register BMC and Method*/
    memcpy(BMC_GET_SHARED_MEM (BMCInst)->DNSconf.RegisterBMC,RegBMC_FQDN,MAX_CHANNEL);

    UNLOCK_BMC_SHARED_MEM(BMCInst);
    return 0;
}


/*
 *@fn GetLinkStatus
 *@brief This function helps to find the link status i.e link is up or down
 *@param Channel - Channel Number
 *@returns Retuns the link Status
 *              Returns -1 on failure
 */
int GetLinkStatus(INT8U Channel,int BMCInst)
{
    char ifname[16];
    int LinkStat=0,index;
    struct  stat Stat;
    _FAR_ BMCInfo_t* pBMCInfo = &g_BMCInfo[BMCInst];

    if(pBMCInfo->IpmiConfig.LinkDownResilentSupport == 1)
    {
        if(fstat(nwlinkfd,&Stat) == -1)
        {
            nwlinkfd = socket(AF_INET, SOCK_DGRAM, 0);
            if(nwlinkfd < 0) 
            {
                TWARN("Error in creating socket for GetNWLinkStatus \n");
                return -1;
            }
        }

        if(IsLANChannel(Channel,BMCInst))
        {
            memset(ifname,0,sizeof(ifname));
            index =GetEthIndex(Channel,BMCInst);
            if(GetIfcName(index,ifname,BMCInst) != 0)
            {
                TWARN("Error in Getting Interface name\n");
                return -1;
            }
            LinkStat = GetNwLinkStatus(nwlinkfd,ifname);
        }
    }
    else
    {
        return 1;
    }

    return LinkStat;
}

