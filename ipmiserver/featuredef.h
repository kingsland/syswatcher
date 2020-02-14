/*****************************************************************
 ******************************************************************
 ***                                                            ***
 ***        (C)Copyright 2012, American Megatrends Inc.         ***
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
 * Filename: featuredef.h
 *
 * Descriptions: library checks the feature entries in the /etc/core_feature 
 *   
 *
 * Author: Othiyappan.K
 *
 ******************************************************************/

#ifndef FEATUREDEF_H
#define FEATUREDEF_H

#define CORE_FEATURES_FILE "/etc/core_features"
#define CORE_MACRO_FILE "/etc/core_macros"

#define MAX_FILELINE_LENGTH 256
#define SECTION_NAME "Macro"
#define MAX_SHELL_NAME_LENGTH 25
#define ENABLED 0x01
#define CORE_FEATURE_ENABLED  1
#define MAX_LEN_Web_SSL_Message  16
#define MAX_LEN_Web_SSL_Version  16

typedef struct
{
    int global_ipv6;
    int bond_support;
    int pam_reorder;
    int service_config;
    int capture_bsod;
    int execdaemon;
    int single_port_app;
    int snmp_support;
    int lmedia_support;
    int dynamic_dns;
    int phy_support;
    int preserve_config;
    int rmedia_support;
    int web_preview;
    int fail_safe_config;
    int node_manager;
    int peci_over_ipmi;
    int sel_clock_sync;
    int circular_sel;
    int kcs_obf_bit;
    int ipmi_ipv6;
    int internal_sensor;
    int ncsi_cmd_support;
    int java_sol_support;
    int web_ssl_sha1_support;
    int web_ssl_tlsv1_support;
    int ssi_support;
    int ssi_event_forward;
    int ipmi_ver_check;
    int ipmi_res_timeout;
    int dcmi_1_5_support;
    int dual_image_support;
    int userpswd_encryption;
    int vlan_priorityset;
    int send_msg_cmd_prefix;
    int disable_pef_for_sel_entry;
    int cmm_support;
    int global_ssh_user;
    int global_ssh_operator;
    int global_telnet_user;
    int global_telnet_operator;
    int global_telnet_authorization;
    int global_ssh_authorization;
    int global_telnet_authenticate;
    int web_user_support;
    int web_operator_support;
    int web_auth_support;
    int web_ssl_md5_support;
    int web_ssl_v3_support;
    int web_javasol_max_tab;
    int web_enc_hash_support;
    int online_flashing_support;
    int timeoutd_sess_timeout;
    int lmedia_medium_type_sd;
    int fwupdate_protocol_select;
} CoreFeatures_T;

typedef struct
{
    int web_session_timeout;
    int ssi_bot_dev_num;
    int ssi_bot_dev_info_size;
    int res_timeout_compcode;
    int global_nic_count;
    int global_used_flash_size;
    int global_used_flash_start;
    int global_erase_blk_size;
    int ssi_max_power_draw;
    int ssi_pwr_multiplier;
    int kcs_res_timeout;
    int lan_res_timeout;
    int serial_res_timeout;
    int ipmb_res_timeout;
    unsigned int uboot_env_start;
    int global_flash_start;
    int JSOL_MAX_TAB;
    char Web_SSL_Message_Digest[MAX_LEN_Web_SSL_Message];
    char Web_SSL_Version[MAX_LEN_Web_SSL_Version];
}CoreMacros_T;

int IsFeatureEnabled (char *feature);
char * GetMacrodefine_string(const char * MacroName,char * Value);
int GetMacrodefine_getint(const char * key, int notfound);
double GetMacrodefine_getdouble( char * key, double notfound);
int GetMacrodefine_getboolean( const char * key, int notfound);
void RetrieveCoreFeatures(CoreFeatures_T *g_features);
void RetrieveCoreMacros(CoreMacros_T *g_coremacros,CoreFeatures_T *g_corefeatures);

#endif
