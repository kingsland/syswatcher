
#ifndef SUPPORT_H
#define SUPPORT_H

/*---------------------------------------------------------------------------*
 * DEVICES SUPPORTED
 *---------------------------------------------------------------------------*/
#define IPM_DEVICE					1
#define APP_DEVICE					1

/*---------------------------------------------------------------------------*
 * IPMI 2.0 SPECIFIC DEFINITIONS
 *---------------------------------------------------------------------------*/
#define IPMI20_SUPPORT				1
#define MAX_SOL_IN_PAYLD_SIZE       252
#define MAX_SOL_OUT_PAYLD_SIZE      252
#define MAX_IN_PAYLD_SIZE			1024
#define MAX_OUT_PAYLD_SIZE			1024
#define MAX_PYLDS_SUPPORT			2
#define MAX_PAYLD_INST				15  /* 1 to 15 only */
#define SYS_SERIAL_PORT_NUM			0

#define GET_SYS_STATE					GetSysState
#define SET_SYS_STATE					SetSysState
#define GET_DEV_ID      GetDevId

#define ACTIVATE_SESSION		ActivateSession			/*UNIMPLEMENTED*/
#define SET_SESSION_PRIV_LEVEL	SetSessionPrivLevel		/*UNIMPLEMENTED*/
#define CLOSE_SESSION			CloseSession			/*UNIMPLEMENTED*/

#endif /*SUPPORT_H*/
