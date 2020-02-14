/****************************************************************
 **                                                            **
 **    (C)Copyright 2006-2009, American Megatrends Inc.        **
 **                                                            **
 **            All Rights Reserved.                            **
 **                                                            **
 **        5555 Oakbrook Pkwy Suite 200, Norcross,             **
 **                                                            **
 **        Georgia - 30093, USA. Phone-(770)-246-8600.         **
 **                                                            **
 ****************************************************************/

/******************************************************************
 * 
 * Filename: ipc.c
 *
 * Description: This file contains functions for creating abstractions
 * for mutexes on top of Sys V semaphores.
 *
 * Author: Andrew McCallum
 *
 ******************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "dbgout.h"

//#include "unix-private.h"
#include "unix.h"
#include "semaph.h"

typedef int bool;
#define FALSE   ( (bool)0 )
#define TRUE    ( (bool)1 )

#define SLEEP_ATOM  ( (useconds_t)100000 )

/** \name Control and Concurrency */
/** \{ */

static inline bool valid_sem_name( char *sem_name )
    /*@globals errno@*/
    /*@modifies errno@*/
{
    if( ( strlen( sem_name ) < SEM_NAME_MAX ) &&
            ( strchr( sem_name, ' ' ) == NULL ) )
        return( TRUE );
    else
    {
        errno = EINVAL;
        return( FALSE );
    }
}

#if 0 // OLD IMPLEMENATION
static int lookup_semid( FILE *sem_table, char *sem_name )
    /*@globals fileSystem, errno@*/
    /*@modifies sem_table, fileSystem, errno@*/
{
    /*@notnull@*/char line_buffer[ SEM_ENT_MAX ];
    bool found = FALSE;
    bool data_remaining = TRUE;
    char *semid_str;
    int semid;

    /* Make sure this name is valid */
    if( !valid_sem_name( sem_name ) )
        return( -1 );

    /* Move to the beginning of the file */
    if( fseek( sem_table, 0, SEEK_SET ) == -1 )
        return( -1 );

    /* Read until you find the one containing sem_name */
    while( !found && data_remaining )
    {
        if( fgets( line_buffer, (int)SEM_ENT_MAX, sem_table ) == NULL )
            data_remaining = FALSE;
        else
        {
            /* Search for the semaphore name */
            semid_str = strstr( line_buffer, sem_name );
            if( semid_str != NULL )
            {   
                /* Found the name.  But the name must also be at the beginning */
                /* of the line, and followed by a space to be a match.         */
                /* Otherwise, we could match "mypattern" when looking for      */
                /* "pattern" or match "pattern-new" when looking for "pattern" */
                if( ( semid_str == (char *)line_buffer ) &&
                        ( semid_str[ strlen( sem_name ) ] == ' ' ) )
                    found = TRUE;
            }
        }
    }

    if( found )
    {
        /* Extract the semaphore id */
        /* Contrary to splint's opinion, line_buffer and semid_str will */
        /* be completely defined when they are used here.               */
        /*@-compdef@*/
        semid_str = strchr( line_buffer, ' ' );
        if( semid_str != NULL )
        {
            /* Skip past the space to the actual string */
            semid_str++;

            /* Convert the string to an integer */
            semid = (int)strtol( semid_str, NULL, 10 );
        }
        else
            semid = -1;
        /*@=compdef@*/
    }
    else
        semid = -1;

    return( semid );
}


static int delete_semid( char *sem_name )
    /*@globals fileSystem, errno@*/
    /*@modifies fileSystem, errno@*/
{
    int semid;
    /*@notnull@*/char line_buffer[ SEM_ENT_MAX ];
    char target_buffer[ SEM_ENT_MAX ];
    char semid_list[ SEM_SET_MAX ][ SEM_ENT_MAX ];
    int i = 0;
    FILE *sem_table;
    FILE *sem_table_new;
    bool data_remaining = TRUE;

    /* Open semaphore lookup table */
    sem_table = fopen( SEM_LOOKUP_TABLE, "r+" );
    if( sem_table == NULL )
        return( -1 );

    /* Obtain exclusive access to the lookup table before we even     */
    /* check to see if the semaphore exists - this prevents a race    */
    /* condition where 2 processes try to delete a semaphore at once. */
    if( file_lock_write( fileno( sem_table ) ) == -1 )
    {
        (void)fclose( sem_table );
        return( -1 );
    }

    semid = lookup_semid( sem_table, sem_name );
    if( semid == -1 )
    {
        (void)file_unlock( fileno( sem_table ) );
        (void)fclose( sem_table );
        return( -1 );
    }

    (void)snprintf( target_buffer, SEM_ENT_MAX, "%s %d\n", sem_name, semid );

    /* Read until you find the one containing sem_name */
    while( data_remaining )
    {
        if( fgets( line_buffer, (int)SEM_ENT_MAX, sem_table ) == NULL )
            data_remaining = FALSE;

        /* Check for our target */
        if( strcmp( line_buffer, target_buffer ) != 0 )
        {
            /* Not our target, so save this one */
            (void)strncpy( semid_list[ i ], line_buffer, SEM_ENT_MAX );
            i++;
        }
    }

    /* Open semaphore lookup table for write.  We open a new stream because */
    /* we need to truncate the file to length 0 before starting, and we     */
    /* have to keep the old stream open to maintain our exclusive lock on   */
    /* the file.                                                            */
    sem_table_new = fopen( SEM_LOOKUP_TABLE, "w" );
    if( sem_table_new == NULL )
    {
        (void)file_unlock( fileno( sem_table ) );
        (void)fclose( sem_table );
        return( -1 );
    }

    while( --i >= 0 )
    {
        /*@-compdef@*//*@-usedef@*/
        fprintf( sem_table_new, "%s", semid_list[ i ] );
        /*@=compdef@*//*@=usedef@*/
    }

    (void)file_unlock( fileno( sem_table ) );
    (void)fclose( sem_table );
    (void)fclose( sem_table_new );

    return( 0 );
}
#endif

static int fcntl_lock_util( int fd, int cmd, short int type, off_t offset,
        short int whence, off_t len )
    /*@globals errno@*/
    /*@modifies errno@*/
{
    struct flock lock;

    lock.l_type = type;
    lock.l_start = offset;
    lock.l_whence = whence;
    lock.l_len = len;
    lock.l_pid = (pid_t)0;

    return( fcntl( fd, cmd, &lock ) );
}


static int fcntl_lock_util_nosignals( int fd, int cmd, short int type, off_t offset,
        short int whence, off_t len )
    /*@globals errno@*/
    /*@modifies errno@*/
{
    int retval;

    do
    {
        retval = fcntl_lock_util( fd, cmd, type, offset, whence, len );
    } while( ( retval < 0 ) && ( errno == EINTR ) );

    return( retval );
}


inline int file_lock_write( int fd )
{
    /* Lock the file from beginning to end, blocking if it is already locked */
    return( fcntl_lock_util( fd, F_SETLKW, (short)F_WRLCK, (off_t)0,
                (short)SEEK_SET, (off_t)0 ) );
}


inline int file_lock_write_nosignals( int fd )
{
    /* Lock the file from beginning to end, blocking if it is already locked */
    return( fcntl_lock_util_nosignals( fd, F_SETLKW, (short)F_WRLCK, (off_t)0,
                (short)SEEK_SET, (off_t)0 ) );
}


int file_lock_write_timeout( int fd, unsigned long timeout )
{
    int fcntl_ret = -1;
    int waiting_for_lock = 1;
    unsigned long retries = timeout / SLEEP_ATOM;

    while( ( waiting_for_lock != 0 ) && ( retries > 0 ) )
    {
        fcntl_ret = fcntl_lock_util( fd, F_SETLK, (short)F_WRLCK, (off_t)0,
                (short)SEEK_SET, (off_t)0 );

        if( fcntl_ret == -1 )
        {
            if( !( ( errno == EACCES ) || ( errno == EAGAIN ) ) )
                waiting_for_lock = 0;
            else
            {
                (void)usleep( SLEEP_ATOM );
                retries--;
            }
        }
        else
        {
            waiting_for_lock = 0;
        }
    }

    return( fcntl_ret );
}


int file_lock_write_timeout_nosignals( int fd, unsigned long timeout )
{
    int fcntl_ret = -1;
    int waiting_for_lock = 1;
    unsigned long retries = timeout / SLEEP_ATOM;

    while( ( waiting_for_lock != 0 ) && ( retries > 0 ) )
    {
        fcntl_ret = fcntl_lock_util_nosignals( fd, F_SETLK, (short)F_WRLCK, (off_t)0,
                (short)SEEK_SET, (off_t)0 );

        if( fcntl_ret == -1 )
        {
            if( !( ( errno == EACCES ) || ( errno == EAGAIN ) ) )
                waiting_for_lock = 0;
            else
            {
                (void)usleep( SLEEP_ATOM );
                retries--;
            }
        }
        else
        {
            waiting_for_lock = 0;
        }
    }

    return( fcntl_ret );
}


inline int file_lock_read( int fd )
{
    /* Lock the file from beginning to end, blocking if it is already locked */
    return( fcntl_lock_util( fd, F_SETLKW, (short)F_RDLCK, (off_t)0,
                (short)SEEK_SET, (off_t)0 ) );
}


inline int file_lock_read_nosignals( int fd )
{
    /* Lock the file from beginning to end, blocking if it is already locked */
    return( fcntl_lock_util_nosignals( fd, F_SETLKW, (short)F_RDLCK, (off_t)0,
                (short)SEEK_SET, (off_t)0 ) );
}


inline int file_unlock( int fd )
{
    /* Unlock the file */
    return( fcntl_lock_util( fd, F_SETLK, (short)F_UNLCK, (off_t)0,
                (short)SEEK_SET, (off_t)0 ) );
}

#if 0	// OLD IMPLEMENTATION
int mutex_create( Mutex_T *pHandle, char *mutex_name )
{
    int semid;
    union semun arg;
    struct sembuf operations[ 1 ];
    FILE *sem_table;

    /* Make sure the handle is valid */	
    if( (pHandle != NULL ) && (pHandle->OwnerPID != -1) )
    {
        printf ("Error: Invalid Handle or Handle already initialized \n");
        return( -1 );
    }

    /* Make sure this name is valid */
    if( !valid_sem_name( mutex_name ) )
    {
        printf ("Error: mutex name %s is not valid \n", mutex_name); 
        return( -1 );
    }

    /* Store mutex name in handle */
    strcpy (pHandle->MutexName, mutex_name);


    /* Open the lookup table for reading and writing, create it if it */
    /* does not exist, don't truncate it if it does                   */
    sem_table = fopen( SEM_LOOKUP_TABLE, "a+" );
    if( sem_table == NULL )
    {
        printf ("Error: Unable to open file %s \n", SEM_LOOKUP_TABLE);  
        return( -1 );
    }

    /* Obtain exclusive access to the lookup table before we even  */
    /* check to see if the semaphore exists - this prevents a race */
    /* condition where 2 processes try to add a semaphore at once. */
    if( file_lock_write( fileno( sem_table ) ) == -1 )
    {
        printf ("Error: file_lock_write() while creating mutex \n");  
        (void)fclose( sem_table );
        return( -1 );
    }

    /* Get the semid if it exists */
    semid = lookup_semid( sem_table, mutex_name );
    if( semid != -1 )
    {
        (void)file_unlock( fileno( sem_table ) );
        (void)fclose( sem_table );

        /* Initialize handle */
        pHandle->NestingCount = 0;
        pHandle->SemID        = semid;
        pHandle->OwnerPID     = getpid();	

        return 0;
    } 


    /* Create a new semaphore set */
    semid = semget( IPC_PRIVATE, 1, SEM_CREATE_FLG );
    if( semid == -1 )
    {
        printf ("Error: semget() failed while creating mutex \n ");  
        (void)file_unlock( fileno( sem_table ) );
        (void)fclose( sem_table );
        return( -1 );
    }
    else
    {
        /* Initialize it to 2 - we'll change this shortly */
        arg.val = 2;
        if( semctl( semid, 0, SETVAL, arg ) == -1 )
        {
            printf ("Error: semctl() failed while creating mutex \n ");    
            (void)file_unlock( fileno( sem_table ) );
            (void)fclose( sem_table );
            return( -1 );
        }

        /* Decrement it to its real start value of 1.  We do this to set */
        /* sem_otime to something besides 0, which lets others know that */
        /* we've finished initializing it.                               */
        operations[ 0 ].sem_num = 0;
        operations[ 0 ].sem_op = (short int)-1;
        operations[ 0 ].sem_flg = (short int)0;

        /*@-compdef@*/
        if( semop( semid, operations, (size_t)1 ) == -1 )
        {
            printf ("Error: semop() failed while creating mutex \n ");    
            (void)file_unlock( fileno( sem_table ) );
            (void)fclose( sem_table );
            return( -1 );
        }
        /*@=compdef@*/
    }

    /* Save the name and semid in a file */
    /* Move to the end of the file */
    if( fseek( sem_table, 0, SEEK_END ) == -1 )
    {
        printf ("Error: Saving mutex name in sem_table failed. \n ");    
        (void)file_unlock( fileno( sem_table ) );
        (void)fclose( sem_table );
        return( -1 );
    }

    /* Write the new entry */
    fprintf( sem_table, "%s %d\n", mutex_name, semid );

    (void)file_unlock( fileno( sem_table ) );
    (void)fclose( sem_table );

    /* Initialize handle */
    pHandle->NestingCount = 0;
    pHandle->SemID        = semid;
    pHandle->OwnerPID     = getpid();	

    return( 0 );
}


int mutex_lock( Mutex_T *pHandle, long timeoutval)
{
    FILE *sem_table;
    struct sembuf operations[ 1 ];
    struct semid_ds semdata;
    union semun arg;
    int semid;
    struct timespec timeout;

    /* Make sure handle is valid */	
    if( pHandle != NULL )
    {
        /* Make sure handle belongs to the same thread */
        if ( (pHandle->OwnerPID == -1) || (pHandle->OwnerPID != getpid()) )
        {
            printf ("Error: Invalid Handle or Handle created by another thread %d, %d\n", pHandle->OwnerPID, getpid());
            return( -1 );
        }
    }
    else return -1;


    /* Open the lookup table for reading */
    sem_table = fopen( SEM_LOOKUP_TABLE, "r" );
    if( sem_table == NULL )
        return( -1 );

    /* Lock the file for reading */
    if( file_lock_read( fileno( sem_table ) ) == -1 )
    {
        (void)fclose( sem_table );
        return( -1 );
    }

    /* Get the semid if it exists */
    semid = lookup_semid( sem_table, pHandle->MutexName );
    if( semid == -1 )
        return( semid );

    (void)file_unlock( fileno( sem_table ) );
    (void)fclose( sem_table );


    /* Provide access if the last operation on the semaphore was
       from the same thread */ 
    //printf ("Cpid - %d, Spid - %d\n", getpid(), semctl( semid, 0, GETPID));

    if ((pHandle->NestingCount != 0) && (semctl( semid, 0, GETPID) == pHandle->OwnerPID) )
    {
        pHandle->NestingCount++;
        return 0;
    }

    pHandle->NestingCount++;

    /* Wait until the semaphore has been initialized */
    /* (This should never take long)                 */
    do
    {
        /* arg.buf is implicitly only, but is not really responsible   */
        /* for freeing the reference (which is local to this function) */
        /*@-immediatetrans@*/
        arg.buf = &semdata;
        /*@=immediatetrans@*/
        if( semctl( semid, 0, IPC_STAT, arg ) == -1 )
            return( -1 );

        /* The call to semctl defines the semdata struct */
        /*@-usedef@*/
        if( semdata.sem_otime != (time_t)0 )
            (void)usleep( (useconds_t)1000 );
        /*@=usedef@*/
    }
    while( semdata.sem_otime == (time_t)0 );

    operations[ 0 ].sem_num = 0;
    operations[ 0 ].sem_op = (short int)-1;
    operations[ 0 ].sem_flg = (short int)SEM_UNDO;


    /* construct timeout parameter */ 
    if (0 != timeoutval)
    {
        timeout.tv_sec   = timeoutval;
        timeout.tv_nsec  = 0;

        /* Invoke the timeout version of semop function */
        return ( semtimedop( semid, operations, (size_t)1, &timeout ) );
    }

    /* operations really is completely defined, splint just has problems */
    /* figuring that sort of thing out for arrays.                       */
    /*@-compdef@*/
    return( semop( semid, operations, (size_t)1 ) );
    /*@=compdef@*/
}


int mutex_unlock( Mutex_T *pHandle)
{
    FILE *sem_table;
    struct semid_ds semdata;
    union semun arg;
    int semid;

    /* Make sure handle is valid */	
    if( pHandle != NULL )
    {
        /* Make sure handle belongs to the same thread */
        if ( (pHandle->OwnerPID == -1) || (pHandle->OwnerPID != getpid()) )
        {
            printf ("Error: Invalid Handle or Handle created by another thread\n");
            return( -1 );
        }
    }
    else return -1;


    /* Open the lookup table for reading */
    sem_table = fopen( SEM_LOOKUP_TABLE, "r" );
    if( sem_table == NULL )
        return( -1 );

    /* Lock the file for reading */
    if( file_lock_read( fileno( sem_table ) ) == -1 )
    {
        (void)fclose( sem_table );
        return( -1 );
    }

    /* Get the semid if it exists */
    semid = lookup_semid( sem_table, pHandle->MutexName );
    if( semid == -1 )
        return( semid );

    (void)file_unlock( fileno( sem_table ) );
    (void)fclose( sem_table );


    /* Provide access if the last operation on the semaphore was
       from the same thread */ 
    //printf ("Cpid - %d, Spid - %d\n", getpid(), semctl( semid, 0, GETPID));

    if ( (pHandle->NestingCount != 0) && (semctl( semid, 0, GETPID) == pHandle->OwnerPID) )  
    { 
        //printf ("NC - %d\n", (*pNestingCount));
        pHandle->NestingCount--;

        if (pHandle->NestingCount != 0)  { return 0;  } 
    }

    pHandle->NestingCount = 0;  

    /* Wait until the semaphore has been initialized */
    /* (This should never take long)                 */
    do
    {
        /* arg.buf is implicitly only, but is not really responsible   */
        /* for freeing the reference (which is local to this function) */
        /*@-immediatetrans@*/
        arg.buf = &semdata;
        /*@=immediatetrans@*/
        if( semctl( semid, 0, IPC_STAT, arg ) == -1 )
            return( -1 );

        /* The call to semctl defines the semdata struct */
        /*@-usedef@*/
        if( semdata.sem_otime != (time_t)0 )
            (void)usleep( (useconds_t)1000 );
        /*@=usedef@*/
    }
    while( semdata.sem_otime == (time_t)0 );

    /* Don't increment the semaphore value - set it to 1.  We're making   */
    /* a mutex,  which can only have two values - 1, or 0.  This prevents  */
    /* problems which can occur if mutex_unlock is called while the mutex */
    /* is already unlocked.                                               */
    arg.val = 1;


    return( semctl( semid, 0, SETVAL, arg ) );
}


int mutex_destroy( Mutex_T *pHandle )
{
    FILE *sem_table;
    int semid;
    union semun arg;

    /* Make sure handle is valid */	
    if( pHandle != NULL )
    {
        /* Make sure handle belongs to the same thread */
        if( (pHandle->OwnerPID == -1) || (pHandle->OwnerPID != getpid()) )
        {
            printf ("Error: Invalid Handle or Handle created by another thread\n");
            return( -1 );
        }
    }
    else return -1;


    /* Open the lookup table for reading */
    sem_table = fopen( SEM_LOOKUP_TABLE, "r" );
    if( sem_table == NULL )
        return( -1 );

    /* Lock the file for reading */
    if( file_lock_read( fileno( sem_table ) ) == -1 )
    {
        (void)fclose( sem_table );
        return( -1 );
    }

    /* Get the semid if it exists */
    semid = lookup_semid( sem_table, pHandle->MutexName );
    if( semid == -1 )
        return( semid );

    (void)file_unlock( fileno( sem_table ) );
    (void)fclose( sem_table );

    /* Uninitialize handle */
    pHandle->NestingCount = 0;
    pHandle->SemID        = -1;
    pHandle->OwnerPID     = -1;	

    /* Delete the mutex from the table */
    if( delete_semid( pHandle->MutexName ) == -1 )
    {
        return( -1 );
    }
    else
    {
        /* Delete it from the system */
        arg.val = 0;
        return( semctl( semid, 0, IPC_RMID, arg ) );
    }

}
#endif
/** \} */
//returns a Mutex_T* allocated here
Mutex_T* mutex_create_recursive( char *mutex_name )
{

    FILE* mutex_name_fp;
    char mutex_file_name[128]; //filename we use for mutex name and then doing a ftok
    key_t mutkey;
    int semid;

    Mutex_T* pmutex = NULL;

    pmutex = malloc(sizeof(Mutex_T));
    if(pmutex == NULL)
    {
        TCRIT("mutex_create_ftok:Allocating for mutex in mutex create failed\n");
        return NULL;
    }


    //create or reuse a existing file for the mutex name so that ftok can generate the key for it
    //there should nto be a race here (TM). fopen with w+ which will create it if it does nto exist or reuse a existin one
    sprintf(mutex_file_name,"%s",mutex_name);
    mutex_name_fp = fopen(mutex_file_name,"w+");
    if(mutex_name_fp == NULL)
    {
        TCRIT("mutex_create_ftok:Creating mutex name file failed\n");
        free(pmutex);
        return NULL;
    }
    else
    {
        fclose(mutex_name_fp); //dont need the file anymore..it just has to exist for ftok
    }


    //now generate a key using ftok based on mutex name and creating a file for the mutex in /var/lock
    //this should again be race free
    mutkey = ftok(mutex_file_name,'I');
    if(mutkey == -1)
    {
        TCRIT("mutex_create_ftok:Error obtaining ftok key for mutex_name %s\n",mutex_name);
        free(pmutex);
        return NULL;
    }


    //call library sem_create with the key and initial value
    //sem_create takes care of locking the create so that two threads don't collide during creation
    // also semcreate returns the id whether it created a semaphore or whether it got an existing one
    semid = sem_create_sysv(mutkey,1);
    if(semid < 0)
    {
        //error during sempahore creation or opening
        TCRIT("mutex_create_ftok:Error creating semaphore in sem_create\n");
        free(pmutex);
        return NULL;
    }

    //if we are here the semaphore already exists or is created in a locked manner. we trust richard stevens.
    //set owner id and other good things here
    strcpy (pmutex->MutexName, mutex_name);
    pmutex->NestingCount = 0;
    pmutex->SemID        = semid;
    pmutex->OwnerPID     = getpid();	
    pmutex->Key = mutkey;	


    return pmutex;
}


//locks a mutex ..what else
int mutex_lock_recursive(Mutex_T* pmutex,long timeoutval)
{

    //checks

    //is a vlaid mutex handle?
    if(pmutex == NULL)
    {
        TCRIT("Null pointer passed to mutex_lock\n");
        return -1;
    }

    //is the claling thread the owner of this mutex handle?
    //remember each thread should have its own handle.
    if(pmutex->OwnerPID != getpid())
    {
        TCRIT("Invalid handle for this process. This mutex handle is owned by %d while mutex_lock was called by id:%d\n",pmutex->OwnerPID,getpid());
        return -1;
    }

    //the question here is whether the thread already has the lock
    //to determine that we see if the NestingCount in this handle is non-zero. if so it has the lock already
    // should we check if the last operation on this was performed by the same thread? That is good for sanity but is 
    // //not a neccessary check. Because if NestingCount was incremented for this process then it definitely was calling this function
    // unless some data corruption occured in the handle
    if(pmutex->NestingCount != 0) //ok we already seem to have the lock
    {
        if(semctl( pmutex->SemID , 0, GETPID) != pmutex->OwnerPID)
        {
            TCRIT("Sanity check failed..though nesting count is greater than 0 the last semop was not performed by this process!!\n");
            return -1;
        }

        //increments nesting count here
        //this gives count of how many times this process has called lock 
        // ok so we dont actually go an try to lock which would block it
        // so we just increment NestingCount and go away
        pmutex->NestingCount++;
        return 0;
    }

    //if we are here the process should try to obtain the lock
    //we call the std stevens function
    if( sem_wait_sysv(pmutex->SemID,timeoutval) < 0)
    {
        TCRIT("Waiting for semaphore failed\n");
        return -1;
    }

    //once we have the lock 
    pmutex->NestingCount++; //indicates we have a lock
    //Sanity check
    if(pmutex->NestingCount != 1) 
    {
        TCRIT("NestingCount is not 1 eventhough we just obtained the lock!!\n");
    }

    return 0;

}

int mutex_destroy_recursive(Mutex_T* pmutex)
{
    //sanity checks
    if(pmutex == NULL)
    {
        TCRIT("Null pointer passed to mutex_lock\n");
        return -1;
    }

    //is the calling thread the owner of this mutex handle?
    //remember each thread should have its own handle.
    if(pmutex->OwnerPID != getpid())
    {
        TCRIT("mutex_destory_recursive: Invalid handle for this thread. This mutex handle is owned by %d while mutex_destroy was called by id:%d\n",pmutex->OwnerPID,getpid());
        return -1;
    }


    //in order to really release we have to see if NestingCount is exactly 0 now
    if(pmutex->NestingCount != 0)
    {
        TWARN("Trying to close with mutex owned by %d Nesting Count ! = 0 by id %d\n",pmutex->OwnerPID,getpid());
        return -1;
    }


    /* Delete semaphore from the system */
    sem_close_sysv(pmutex->SemID);

    pmutex->NestingCount = 0;
    pmutex->SemID        = -1;
    pmutex->OwnerPID     = -1;
    pmutex->Key = -1;				

    free(pmutex);
    return 0;
}

//unlocks a mutex
int mutex_unlock_recursive(Mutex_T* pmutex)
{
    //sanity checks
    if(pmutex == NULL)
    {
        TCRIT("Null pointer passed to mutex_lock\n");
        return -1;
    }

    //is the calling thread the owner of this mutex handle?
    //remember each thread should have its own handle.
    if(pmutex->OwnerPID != getpid())
    {
        TCRIT("Invalid handle for this thread. This mutex handle is owned by %d while mutex_unlock was called by id:%d\n",pmutex->OwnerPID,getpid());
        return -1;
    }


    //in order to really release we have to see if NestingCount is exactly 1 now
    if(pmutex->NestingCount != 0)
    {
        pid_t tmppid;
        //sanity check to see if the last op on this was from this pid. if not we should be freaking out
        //we are getting a unlock without a lock from this thread..not bad in itself but just good to know
        tmppid = semctl( pmutex->SemID , 0, GETPID) ;
        if(tmppid != pmutex->OwnerPID)
        {
            TCRIT("in mutex_unlock..Sanity check failed..though nesting count is greater than 0 the last semop was not performed by this process!!may indicate unlock was called without a lock\n");
            TCRIT("Mutex name :%s Owner PID is %d while last op was performed by %d\n",pmutex->MutexName,pmutex->OwnerPID,tmppid);
            return -1;
        }

        //if we are here then we can decrement nesting count
        pmutex->NestingCount--;

        //we decremented.if nesting count is not 0 right now we can go back without unlocking some upper lock is still in hold
        if(pmutex->NestingCount != 0) 
            return 0;

    }
    else
    {
        TCRIT("in mutex unlock: unlock may have been called wihtout lock!!\n");
        //in this case we cannot go and unlock the semaphore ..this will increase the sema count!! yeeks!!
        return 0;
    }

    //if we are here we came here because NestingCount is now 0 and therefore we can actually unlock
    if(sem_signal_sysv(pmutex->SemID) < 0)
    {
        TCRIT("Error unlocking!!\n");
        return -1;
    }

    return 0;
}


