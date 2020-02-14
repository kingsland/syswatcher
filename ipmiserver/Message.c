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
 *****************************************************************
 ******************************************************************
 *
 * message.c
 * Messaging Functions.
 *
 *  Author: Govind Kothandapani <govindk@ami.com>
 *          Basavaraj Astekar <basavaraja@ami.com>
 ******************************************************************/
#define ENABLE_DEBUG_MACROS	0
#include "Types.h"
#include "OSPort.h"
#include "Message.h"
#include "Debug.h"
#include <stdlib.h>
#include <sys/errno.h>
#include "unix.h"
#include "IPMIConf.h"
#include <semaphore.h>

extern sem_t sem_QUEUE;

#define MAX_STR_LENGTH 	128	

#define LOCK_QUEUE(hQueue)                              \
    if (-1 == file_lock_write (hQueue))                     \
{                                                       \
    IPMI_WARNING ("Message.c : Error locking Queue\n");       \
}

#define UNLOCK_QUEUE(hQueue)                            \
    if (-1 == file_unlock (hQueue))                         \
{                                                       \
    IPMI_WARNING ("Message.c : Error unlocking Queue\n");     \
}                                                       \

#define WAIT_QUEUE \
    do{		    \
        sem_wait(&sem_QUEUE); \
    }while(0);
#define POST_QUEUE \
    do{		    \
        sem_post(&sem_QUEUE); \
    }while(0);

/**
 * @fn PostMsg
 * @brief Post a message to the destination task.
 * @param MsgPkt   - Message to be posted.
 * @param Queue	   - Queue to post this message to.
 * @return   0 if success, -1 if failed.
 **/
    int
PostMsg (_FAR_ MsgPkt_T* pMsgPkt, INT8S *Queuepath, int BMCInst )
{
    int     Err,i=0;
    INT16U  Size;
    INT8S	keyInstance[MAX_STR_LENGTH];
    memset(keyInstance,0,MAX_STR_LENGTH);
    sprintf(keyInstance, "%s%d", Queuepath, BMCInst); 

    Size    = sizeof (MsgPkt_T) - MSG_PAYLOAD_SIZE + pMsgPkt->Size;

    for(i=0;i< MAX_IPMI_IFCQ;i++)
    {
        if(strcmp(g_IPMIIfcQueue[i].IfcName,keyInstance) == 0)
        {
            LOCK_QUEUE   (g_IPMIIfcQueue[i].IfcQ);
            OS_ADD_TO_Q  (pMsgPkt, Size, g_IPMIIfcQueue[i].IfcQ, &Err);
            UNLOCK_QUEUE (g_IPMIIfcQueue[i].IfcQ);
            if ((Err == -1) || (Err != Size))
            {
                IPMI_WARNING ("Message.c : PostMsg %x %s\n",errno, strerror(errno));
                return -1;
            }             
            break;
        }
    }    

    if(i == MAX_IPMI_IFCQ)
    {
        return -1;
    }

    return 0;
}


/**
 * @fn AddToQueue
 * @brief Post a buffer to the destination task.
 * @param pBuf   - Buffer to be posted.
 * @param Queuepath    - Queuepath to post this buffer to.
 * @return   0 if success, -1 if failed.
 **/
    int
AddToQueue (void* pBuf, INT8S *Queuepath,INT32U Size,int BMCInst)
{
    int     Err,i=0;
    INT8S	keyInstance[MAX_STR_LENGTH];

    memset(keyInstance,0,MAX_STR_LENGTH);
    sprintf(keyInstance, "%s%d", Queuepath, BMCInst); 

    for(i=0;i< MAX_IPMI_IFCQ;i++)
    {
        if(strcmp(g_IPMIIfcQueue[i].IfcName,keyInstance) == 0)
        {
            LOCK_QUEUE   (g_IPMIIfcQueue[i].IfcQ);
            OS_ADD_TO_Q  (pBuf, Size, g_IPMIIfcQueue[i].IfcQ,&Err);
            UNLOCK_QUEUE (g_IPMIIfcQueue[i].IfcQ);

            if ((Err == -1) || (Err != Size))
            {
                IPMI_WARNING ("Message.c : Post To Queue %x %s\n",errno, strerror(errno));
                return -1;
            }             
            break;
        }
    }    

    if(i == MAX_IPMI_IFCQ)
    {
        return -1;
    }

    return 0;
}

/**
 * @fn PostMsgNonBlock
 * @brief Post a message to the destination task without blocking.
 * @param MsgPkt   - Message to be posted.
 * @param Queue	   - Queue to post this message to.
 * @return   0 if success, -1 if failed.
 **/
    int
PostMsgNonBlock (_FAR_ MsgPkt_T* pMsgPkt, INT8S *Queuepath,int BMCInst )
{
    int     Err,i=0;
    INT16U  Size;
    int flags;
    INT8S	keyInstance[MAX_STR_LENGTH];


    memset(keyInstance,0,MAX_STR_LENGTH);
    sprintf(keyInstance, "%s%d", Queuepath, BMCInst); 

    Size    = sizeof (MsgPkt_T) - MSG_PAYLOAD_SIZE + pMsgPkt->Size;

    for(i=0;i< MAX_IPMI_IFCQ;i++)
    {
        if(strcmp(g_IPMIIfcQueue[i].IfcName,keyInstance) == 0)
        {

            LOCK_QUEUE   (g_IPMIIfcQueue[i].IfcQ);

            flags = fcntl(g_IPMIIfcQueue[i].IfcQ,F_GETFL);
            fcntl (g_IPMIIfcQueue[i].IfcQ, F_SETFL, flags|O_NONBLOCK);
            OS_ADD_TO_Q  (pMsgPkt, Size, g_IPMIIfcQueue[i].IfcQ, &Err);
            fcntl (g_IPMIIfcQueue[i].IfcQ, F_SETFL, flags);

            UNLOCK_QUEUE (g_IPMIIfcQueue[i].IfcQ);
            if ((Err == -1) || (Err != Size))
            {
                IPMI_WARNING ("Message.c : PostMsgNonBlock -  %s\n", strerror(errno));
                return -1;
            }
            break;
        }
    }

    if(i == MAX_IPMI_IFCQ)
    {
        return -1;
    }


    return 0;
}


/**
 * @fn GetMsg
 * @brief Gets the message posted to this task.
 * @param MsgPkt   - Pointer to the buffer to hold the message packet.
 * @param Queue    - Queue to fetch the message from.
 * @param NumMs    - Number of milli-seconds to wait.
 *  				 WAIT_NONE     - If the function has to return immediately.
 *					 WAIT_INFINITE - If the function has to wait infinitely.
 * NOTE :
 * @return   0 if success, -1 if failed.
 **/
    int
GetMsg (_FAR_ MsgPkt_T*	pMsgPkt, INT8S *Queuepath, INT16S Timeout,int BMCInst)
{
    int     Err,i=0;
    int     Size;
    INT8S	keyInstance[MAX_STR_LENGTH];


    memset(keyInstance,0,MAX_STR_LENGTH);
    sprintf(keyInstance, "%s%d", Queuepath, BMCInst); 

    WAIT_QUEUE
        for(i=0;i<MAX_IPMI_IFCQ;i++)
        {
            if(strcmp(g_IPMIIfcQueue[i].IfcName,keyInstance) == 0)
            {
                break;
            }
        }
    POST_QUEUE

        if(i == MAX_IPMI_IFCQ)
        {
            IPMI_WARNING("Message.c : Unable to Get Queue : %s",keyInstance);
            return -1;
        }

    if (WAIT_INFINITE != Timeout)
    {
        struct timeval      Timeval;
        fd_set              fdRead;
        int                 n, RetVal;

        FD_ZERO (&fdRead);
        FD_SET (g_IPMIIfcQueue[i].IfcQ, &fdRead);
        n = g_IPMIIfcQueue[i].IfcQ + 1;

        Timeval.tv_sec  = 0;
        Timeval.tv_usec = Timeout * 1000 * 1000;
        RetVal = select (n, &fdRead, NULL, NULL, &Timeval);
        if (-1 == RetVal)
        {
            IPMI_WARNING ("Message.c : Error waiting on Queue %s\n", strerror (errno));
            return -1;
        }
        else if (0 == RetVal)
        {
            return -2;
        }
    }

    /* Get the header first*/
    Size = sizeof (MsgPkt_T) - MSG_PAYLOAD_SIZE;
    //	printf("%s:%d g_IPMIIfcQueue[%d].IfcQ=%d keyInstance=%s\n",__FUNCTION__,__LINE__,
    //		i,g_IPMIIfcQueue[i].IfcQ,keyInstance);
    OS_GET_FROM_Q (pMsgPkt, Size, g_IPMIIfcQueue[i].IfcQ, WAIT_INFINITE, &Err);
    if (Err == -1)
    {
        IPMI_WARNING ("Message.c : GetMsg %s\n",strerror(errno));
        return -1;
    }

    /* Get the payload data */
    Size = pMsgPkt->Size;
    OS_GET_FROM_Q (pMsgPkt->Data, Size, g_IPMIIfcQueue[i].IfcQ, WAIT_INFINITE, &Err);
    if (Err == -1)
    {
        IPMI_WARNING ("Message.c : GetMsg %s\n",strerror(errno));
        return -1;
    }

    return 0;
}


/**
 * @fn GetMsgInMsec
 * @brief Gets the message posted to this task.
 * @param MsgPkt   - Pointer to the buffer to hold the message packet.
 * @param Queue    - Queue to fetch the message from.
 * @param NumMs    - Number of milli-seconds to wait.
 * 
 * WAIT_NONE       - If the function has to return immediately.
 * WAIT_INFINITE   - If the function has to wait infinitely.
 * NOTE :
 * @return   0 if success, -1 if failed.
 **/
    int
GetMsgInMsec (_FAR_ MsgPkt_T*	pMsgPkt, INT8S *Queuepath, INT16U Timeout, int BMCInst)
{
    int     Err,i=0;
    int     Size;
    INT8S	keyInstance[MAX_STR_LENGTH];

    memset(keyInstance,0,MAX_STR_LENGTH);
    sprintf(keyInstance, "%s%d", Queuepath, BMCInst); 

    for(i=0;i<MAX_IPMI_IFCQ;i++)
    {
        if(strcmp(g_IPMIIfcQueue[i].IfcName,keyInstance) == 0)
        {
            break;
        }
    }

    if(i == MAX_IPMI_IFCQ)
    {
        IPMI_WARNING("Message.c : Unable to Get Queue : %s",keyInstance);
        return -1;
    }

    if (WAIT_INFINITE != Timeout)
    {
        struct timeval      Timeval;
        fd_set              fdRead;
        int                 n, RetVal;

        FD_ZERO (&fdRead);
        FD_SET (g_IPMIIfcQueue[i].IfcQ, &fdRead);
        n = g_IPMIIfcQueue[i].IfcQ + 1;

        Timeval.tv_sec  = Timeout/1000;
        Timeval.tv_usec = (Timeout % 1000) * 1000;
        RetVal = select (n, &fdRead, NULL, NULL, &Timeval);
        if (-1 == RetVal)
        {
            IPMI_WARNING ("Message.c : Error waiting on Queue %s\n", strerror (errno));
            return -1;
        }
        else if (0 == RetVal)
        {
            return 1;
        }
    }

    /* Get the header first*/
    Size = sizeof (MsgPkt_T) - MSG_PAYLOAD_SIZE;
    OS_GET_FROM_Q (pMsgPkt, Size, g_IPMIIfcQueue[i].IfcQ, WAIT_INFINITE, &Err);
    if (Err == -1)
    {
        IPMI_WARNING ("Message.c : GetMsg %s\n",strerror(errno));
        return -1;
    }

    /* Get the payload data */
    Size = pMsgPkt->Size;
    OS_GET_FROM_Q (pMsgPkt->Data, Size, g_IPMIIfcQueue[i].IfcQ, WAIT_INFINITE, &Err);
    if (Err == -1)
    {
        IPMI_WARNING ("Message.c : GetMsg %s\n",strerror(errno));
        return -1;
    }

    return 0;
}


/**
 * @fn AddQueue
 * @brief Adds the Queue to the common Queue Handle structure
 * @param Queuepath - Queue Path
 * @return Return 0
 **/
int AddQueue(INT8S *Queuepath)
{
    int i=0;

    WAIT_QUEUE
        for(i=0; i< MAX_IPMI_IFCQ;i++)
        {
            if(strncmp(g_IPMIIfcQueue[i].IfcName,"",1) == 0)
            {
                strcpy(g_IPMIIfcQueue[i].IfcName,Queuepath);
                break;
            }
        }
    POST_QUEUE
        return 0;
}

/**
 * @fn GetQueue
 * @brief Gets the Queue Handle from the Common
 *            Queue Handle Structure
 * @param Queuepath - Queue Path
 * @return Return 0
 **/
int GetQueue(INT8S *Queuepath,int flags)
{
    int i=0;
    WAIT_QUEUE
        for(i=0;i<MAX_IPMI_IFCQ;i++)
        {
            if(strcmp(g_IPMIIfcQueue[i].IfcName,Queuepath) == 0)
            {
                g_IPMIIfcQueue[i].IfcQ = open (Queuepath, flags);
                if(g_IPMIIfcQueue[i].IfcQ == -1)
                {
                    IPMI_ERROR ("Error opening named pipe %s\n",Queuepath);
                    perror("");
                }
                break;
            }
        }
    POST_QUEUE
        if(i == MAX_IPMI_IFCQ)
        {
            return -1;
        }

    return 0;
}

/**
 * @fn GetQueueHandle
 * @brief Gets the Queue Handle from the Common Queue Handle Structure
 * @param Queuepath - Queue Path
 * @param IfcQ  Handle for the needed Queue
 * @return Return 0
 **/
int GetQueueHandle(INT8S *Queuepath,HQueue_T *IfcQ, int BMCInst)
{
    int i=0;
    INT8S	keyInstance[MAX_STR_LENGTH];


    memset(keyInstance,0,MAX_STR_LENGTH);
    sprintf(keyInstance, "%s%d", Queuepath, BMCInst); 
    WAIT_QUEUE
        for(i=0;i<MAX_IPMI_IFCQ;i++)
        {
            if(strcmp(g_IPMIIfcQueue[i].IfcName,keyInstance) == 0)
            {
                *IfcQ = g_IPMIIfcQueue[i].IfcQ;
                break;
            }
        }
    POST_QUEUE
        if(i == MAX_IPMI_IFCQ)
        {
            return -1;
        }    

    return 0;
}

/**
 * @brief Returns the number of messages in the Queue.
 * @param Queue Queue.
 **/
    int
NumMsg (HQueue_T hQueue)
{
    int     Depth;
    struct  stat Stat;

    if(0 == fstat (hQueue, &Stat))
    {
        IPMI_DBG_PRINT_1 ("Stat.st_size = %lx\n", Stat.st_size);
        Depth = Stat.st_size/sizeof (MsgPkt_T);
    }
    else
    {
        IPMI_DBG_PRINT_1 ("Error Stat.st_size = %lx\n", Stat.st_size);
        Depth = 0;
    }

    return Depth;
}

