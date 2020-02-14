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
 * Filename: semaph.c
 *
 ******************************************************************/

/*
 * Provide an simpler and easier to understand interface to the System V
 * semaphore system calls.  There are 7 routines available to the user:
 *
 *	id = sem_create(key, initval);	# create with initial value or open
 *	id = sem_open(key);		# open (must already exist)
 *	sem_wait(id);			# wait = P = down by 1
 *	sem_signal(id);			# signal = V = up by 1
 *	sem_op(id, amount);		# wait   if (amount < 0)
 *					# signal if (amount > 0)
 *	sem_close(id);			# close
 *	sem_rm(id);			# remove (delete)
 *
 * We create and use a 3-member set for the requested semaphore.
 * The first member, [0], is the actual semaphore value, and the second
 * member, [1], is a counter used to know when all processes have finished
 * with the semaphore.  The counter is initialized to a large number,
 * decremented on every create or open and incremented on every close.
 * This way we can use the "adjust" feature provided by System V so that
 * any process that exit's without calling sem_close() is accounted
 * for.  It doesn't help us if the last process does this (as we have
 * no way of getting control to remove the semaphore) but it will
 * work if any process other than the last does an exit (intentional
 * or unintentional).
 * The third member, [2], of the semaphore set is used as a lock variable
 * to avoid any race conditions in the sem_create() and sem_close()
 * functions.
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "dbgout.h"
#include "semaph.h"
#include <string.h>
#include <errno.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
extern int	errno;
#define	BIGCOUNT	10000		/* initial value of process counter */

/*
 * Define the semaphore operation arrays for the semop() calls.
 */

static int PerformSemOp(char * file, int nSemop);

/*! ************************************************************
 * @fn     ReleaseFileLock
 * @brief  Release the semaphore lock for a particular file.

 * @param[in]  file -  String value to get file name.

 * @return      SUCCESS or FAILURE.
 ***************************************************************/

int LINUX_ReleaseFileLock(char * file, char nLockType) 
{

    int retVal = -1;

    if(file != NULL) 
    {
        retVal = PerformSemOp(file, 1);
    }

    return(retVal);

}

/*! ************************************************************
 * @fn     GetFileLock
 * @brief  Get the semaphore lock for a particular file.

 * @param[in]  file -  String value to get file name.

 * @return      SUCCESS or FAILURE.
 ***************************************************************/

int LINUX_GetFileLock(char * file, char nLockType) 
{
    int retVal = -1;

    if(file != NULL) 
    {
        retVal = PerformSemOp(file, -1);
    }

    return (retVal);

}


/*! ************************************************************
 * @fn     PerformSemOp
 * @brief  Performs the given semaphore operation.

 * @param[in]  file -  String value to get file name.
 * @param[in]  nSemop - Operation to be performed.

 * @return      SUCCESS or FAILURE.
 ***************************************************************/

static int PerformSemOp(char * file, int nSemop) 
{

    int semid; /* semid of semaphore set */
    key_t key; /* key to pass to semget() */
    int nsems = 1; /* nsems to pass to semget() */
    FILE * fp = NULL;

    if((fp = fopen(file, "a+")) != NULL) 
    {
        // Just need to check that file is exist
        // if not exist then create that file
        fclose(fp);
    } 
    else 
    {
        return -1;
    }

    if((key = ftok(file, 'L')) == -1) 
    {
        TCRIT("ftok : Error obtaining ftok key for name semaphore.c\n");
        return -1;
    }

    if ((semid = sem_create_sysv(key, nsems)) == -1) 
    {
        TCRIT("semget: semget failed\n");
        return -1;
    }

    if ((sem_op_sysv(semid, nSemop, 0)) == -1) 
    {
        TCRIT("semop: failed\n");
        return -1;
    }
    if(nSemop > 0) sem_close_sysv(semid);

    return 0;
}






/****************************************************************************
 * Create a semaphore with a specified initial value.
 * If the semaphore already exists, we don't initialize it (of course).
 * We return the semaphore ID if all OK, else -1.
 */

int sem_create_sysv(key_t key, int initval)
{
    register int		id, semval;
    union semun {
        int		val;
        struct semid_ds	*buf;
        ushort		*array;
    } semctl_arg;


    struct sembuf	op_lock[2] = 
    {
        {2, 0, 0},	/* wait for [2] (lock) to equal 0 */
        {2, 1, SEM_UNDO}	/* then increment [2] to 1 - this locks it */
        /* UNDO to release the lock if processes exits
           before explicitly unlocking */
    };


    struct sembuf	op_endcreate[2] = {
        {1, -1, SEM_UNDO},/* decrement [1] (proc counter) with undo on exit */
        /* UNDO to adjust proc counter if process exits
           before explicitly calling sem_close() */
        {2, -1, SEM_UNDO}	/* then decrement [2] (lock) back to 0 */
    };


    if (key == IPC_PRIVATE)
        return(-1);	/* not intended for private semaphores */

    else if (key == (key_t) -1)
        return(-1);	/* probably an ftok() error by caller */

again:
    if ( (id = semget(key, 3, 0666 | IPC_CREAT)) < 0)
        return(-1);	/* permission problem or tables full */

    /*
     * When the semaphore is created, we know that the value of all
     * 3 members is 0.
     * Get a lock on the semaphore by waiting for [2] to equal 0,
     * then increment it.
     *
     * There is a race condition here.  There is a possibility that
     * between the semget() above and the semop() below, another
     * process can call our sem_close() function which can remove
     * the semaphore if that process is the last one using it.
     * Therefore, we handle the error condition of an invalid
     * semaphore ID specially below, and if it does happen, we just
     * go back and create it again.
     */

    if (semop(id, &op_lock[0], 2) < 0) {
        if (errno == EINVAL)
            goto again;
        TCRIT("can't lock");
    }

    /*
     * Get the value of the process counter.  If it equals 0,
     * then no one has initialized the semaphore yet.
     */

    if ( (semval = semctl(id, 1, GETVAL, 0)) < 0)
        TCRIT("can't GETVAL");

    if (semval == 0) {
        /*
         * We could initialize by doing a SETALL, but that
         * would clear the adjust value that we set when we
         * locked the semaphore above.  Instead, we'll do 2
         * system calls to initialize [0] and [1].
         */

        semctl_arg.val = initval;
        if (semctl(id, 0, SETVAL, semctl_arg) < 0)
            TCRIT("can SETVAL[0]");

        semctl_arg.val = BIGCOUNT;
        if (semctl(id, 1, SETVAL, semctl_arg) < 0)
            TCRIT("can SETVAL[1]");
    }

    /*
     * Decrement the process counter and then release the lock.
     */

    if (semop(id, &op_endcreate[0], 2) < 0)
        TCRIT("can't end create");

    return(id);
}

/****************************************************************************
 * Open a semaphore that must already exist.
 * This function should be used, instead of sem_create(), if the caller
 * knows that the semaphore must already exist.  For example a client
 * from a client-server pair would use this, if its the server's
 * responsibility to create the semaphore.
 * We return the semaphore ID if all OK, else -1.
 */

int sem_open_sysv(key_t key)
{
    register int	id;


    struct sembuf	op_open[1] = {
        {1, -1, SEM_UNDO}	/* decrement [1] (proc counter) with undo on exit */
    };

    if (key == IPC_PRIVATE)
        return(-1);	/* not intended for private semaphores */

    else if (key == (key_t) -1)
        return(-1);	/* probably an ftok() error by caller */

    if ( (id = semget(key, 3, 0)) < 0)
        return(-1);	/* doesn't exist, or tables full */

    /*
     * Decrement the process counter.  We don't need a lock
     * to do this.
     */

    if (semop(id, &op_open[0], 1) < 0)
        TCRIT("can't open");

    return(id);
}

/****************************************************************************
 * Remove a semaphore.
 * This call is intended to be called by a server, for example,
 * when it is being shut down, as we do an IPC_RMID on the semaphore,
 * regardless whether other processes may be using it or not.
 * Most other processes should use sem_close() below.
 */

void sem_rm_sysv(int id)
{
    if (semctl(id, 0, IPC_RMID, 0) < 0)
        TCRIT("can't IPC_RMID");
}

/****************************************************************************
 * Close a semaphore.
 * Unlike the remove function above, this function is for a process
 * to call before it exits, when it is done with the semaphore.
 * We "decrement" the counter of processes using the semaphore, and
 * if this was the last one, we can remove the semaphore.
 */

void sem_close_sysv(int id)
{
    register int	semval;


    struct sembuf	op_close[3] = {
        {2, 0, 0},	/* wait for [2] (lock) to equal 0 */
        {2, 1, SEM_UNDO},	/* then increment [2] to 1 - this locks it */
        {1, 1, SEM_UNDO}	/* then increment [1] (proc counter) */
    };


    struct sembuf	op_unlock[1] = {
        {2, -1, SEM_UNDO}	/* decrement [2] (lock) back to 0 */
    };

    /*
     * The following semop() first gets a lock on the semaphore,
     * then increments [1] - the process counter.
     */

    if (semop(id, &op_close[0], 3) < 0)
        TCRIT("can't semop");

    /*
     * Now that we have a lock, read the value of the process
     * counter to see if this is the last reference to the
     * semaphore.
     * There is a race condition here - see the comments in
     * sem_create().
     */

    if ( (semval = semctl(id, 1, GETVAL, 0)) < 0)
        TCRIT("can't GETVAL");

    if (semval > BIGCOUNT)
        TCRIT("sem[1] > BIGCOUNT");
    else if (semval == BIGCOUNT)
        sem_rm_sysv(id);
    else
        if (semop(id, &op_unlock[0], 1) < 0)
            TCRIT("can't unlock");	/* unlock */
}

/****************************************************************************
 * Wait until a semaphore's value is greater than 0, then decrement
 * it by 1 and return.
 * Dijkstra's P operation.  Tanenbaum's DOWN operation.
 */

int sem_wait_sysv(int id, long timeoutval)
{
    return sem_op_sysv(id, -1, timeoutval);
}

/****************************************************************************
 * Increment a semaphore by 1.
 * Dijkstra's V operation.  Tanenbaum's UP operation.
 */

int sem_signal_sysv(int id)
{
    return sem_op_sysv(id, 1, 0);
}

/****************************************************************************
 * General semaphore operation.  Increment or decrement by a user-specified
 * amount (positive or negative; amount can't be zero).
 */

int sem_op_sysv(int id, int value, long timeoutval)
{

    struct timespec to;	

    struct sembuf	op_op[1] = {
        {0, 99, SEM_UNDO}	/* decrement or increment [0] with undo on exit */
        /* the 99 is set to the actual amount to add
           or subtract (positive or negative) */
    };

    if ( (op_op[0].sem_op = value) == 0)
    {
        TCRIT("can't have value == 0");
        return -1;
    }

    if (timeoutval == 0)
    {
        if (semop(id, &op_op[0], 1) < 0)
        {
            TCRIT("sem_op error");
            return -1;
        }
    }
    else
    {
        to.tv_sec = timeoutval;
        to.tv_nsec = 0;
        if (semtimedop(id, &op_op[0], 1, &to) < 0)
        {
            TCRIT("semtimedop error");
            return -1;
        }
    }


    return 0;

}

/****************************************************************************
 * Creates a semaphore if doesn't exist.If the semaphore already exists
 * then opens it and returns the semaphore.
 */
sem_t* CreateSemaphore(char *semaphoreKey)
{
    sem_t *semaphore = sem_open(semaphoreKey, O_CREAT | O_EXCL, 0666, 1);

    if (semaphore == SEM_FAILED)
    {
        int errNumber = errno;

        if (errNumber == EEXIST)	
            semaphore = sem_open(semaphoreKey, 0, 0666, 0);
    }

    return semaphore;
}

/****************************************************************************
 * Waits on a semaphore until the process counter reaches greater than zero
 */
void WaitSemaphore(sem_t* semaphore)
{
    sem_wait(semaphore);
}

/****************************************************************************
 * Release the semaphore after posting and incrementing the process counter
 * and unlink the semaphore by using the semaphore unique key
 */
void ReleaseSemaphore(sem_t *semaphore,char *semaphoreKey)
{
    sem_post(semaphore);
    sem_close(semaphore);
    sem_unlink(semaphoreKey);
}

