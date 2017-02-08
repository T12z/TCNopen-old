/**********************************************************************************************************************/
/**
 * @file            posix/vos_thread.c
 *
 * @brief           Multitasking functions
 *
 * @details         OS abstraction of thread-handling functions
 *
 * @note            Project: TCNOpen TRDP prototype stack
 *
 * @author          Bernd Loehr, NewTec GmbH
 *
 * @remarks This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 *          If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *          Copyright Bombardier Transportation Inc. or its subsidiaries and others, 2013. All rights reserved.
 *
 * $Id$
 *
 *      BL 2017-02-08: Stacksize computation enhanced
 *      BL 2016-07-06: Ticket #122 64Bit compatibility (+ compiler warnings)
 */

#ifndef POSIX
#error \
    "You are trying to compile the POSIX implementation of vos_thread.c - either define POSIX or exclude this file!"
#endif

/***********************************************************************************************************************
 * INCLUDES
 */

#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>

#ifdef __APPLE__
#include <uuid/uuid.h>
#else
#include "vos_sock.h"
#endif

#include "vos_types.h"
#include "vos_thread.h"
#include "vos_mem.h"
#include "vos_utils.h"
#include "vos_private.h"

/***********************************************************************************************************************
 * DEFINITIONS
 */

#ifndef PTHREAD_MUTEX_RECURSIVE
#define PTHREAD_MUTEX_RECURSIVE  PTHREAD_MUTEX_RECURSIVE_NP
#endif

const size_t    cDefaultStackSize   = 4u * PTHREAD_STACK_MIN;
const UINT32    cMutextMagic        = 0x1234FEDCu;

int             vosThreadInitialised = FALSE;

/***********************************************************************************************************************
 *  LOCALS
 */
#ifdef __APPLE__
int sem_timedwait (sem_t *sem, const struct timespec *abs_timeout)
{
    /* sem_timedwait() is not supported by DARWIN / Mac OS X!  */
    /* This is a very simple replacement - only suitable for debugging/testing!!!
       It will fail if there are more than 2 threads waiting... */
    VOS_TIME_T now, timeOut;

    timeOut.tv_sec  = abs_timeout->tv_sec;
    timeOut.tv_usec = (__darwin_suseconds_t) abs_timeout->tv_nsec * 1000;

    for (;;)
    {
        if (sem_trywait(sem) == 0)
        {
            return 0;
        }
        usleep(10000);      /* cancellation point */
        if (errno == EINTR)
        {
            break;
        }
        vos_getTime(&now);
        if (vos_cmpTime(&timeOut, &now) < 0)
        {
            errno = ETIMEDOUT;
            break;
        }
    }

    return -1;
}

/* simulate
    int sem_init(sem_t *, int, unsigned int);
*/
int sem_init (sem_t *pSema, int flags, unsigned int mode)
{
    pSema = sem_open("/tmp/trdp.sema", O_CREAT, 0644, (UINT8)mode);
    if (pSema == SEM_FAILED)
    {
        return -1;
    }
    return 0;
    /* rc = sem_init((sem_t *)*pSema, 0, (UINT8)initialState); */
}
#endif


/***********************************************************************************************************************
 * GLOBAL FUNCTIONS
 */


/**********************************************************************************************************************/
/*  Threads
                                                                                                               */
/**********************************************************************************************************************/
/** Cyclic thread functions.
 *  Wrapper for cyclic threads. The thread function will be called cyclically with interval.
 *
 *  @param[in]      interval        Interval for cyclic threads in us (incl. runtime)
 *  @param[in]      pFunction       Pointer to the thread function
 *  @param[in]      pArguments      Pointer to the thread function parameters
 *  @retval         void
 */

#define NSECS_PER_USEC  1000
#define USECS_PER_MSEC  1000u
#define MSECS_PER_SEC   1000

/* This define holds the max amount os seconds to get stored in 32bit holding micro seconds        */
/* It is the result when using the common time struct with tv_sec and tv_usec as on a 32 bit value */
/* so far 0..999999 gets used for the tv_usec field as per definition, then 0xFFF0BDC0 usec        */
/* are remaining to represent the seconds, which in turn give 0x10C5 seconds or in decimal 4293    */
#define MAXSEC_FOR_USECPRESENTATION  4293U

EXT_DECL void vos_cyclicThread (
    UINT32              interval,
    VOS_THREAD_FUNC_T   pFunction,
    void                *pArguments)
{
    VOS_TIME_T  priorCall;
    VOS_TIME_T  afterCall;
    UINT32      execTime;
    UINT32      waitingTime;
    for (;; )
    {
        vos_getTime(&priorCall);  /* get initial time */
        pFunction(pArguments);    /* perform thread function */
        vos_getTime(&afterCall);  /* get time after function ghas returned */
        /* subtract in the pattern after - prior to get the runtime of function() */
        vos_subTime(&afterCall, &priorCall);
        /* afterCall holds now the time difference within a structure not compatible with interval */
        /* check if UINT32 fits to hold the waiting time value */
        if (afterCall.tv_sec <= MAXSEC_FOR_USECPRESENTATION)
        {
            /*           sec to usec conversion                               value normalized from 0 .. 999999*/
            execTime = ((UINT32) (afterCall.tv_sec * MSECS_PER_SEC * USECS_PER_MSEC) + (UINT32)afterCall.tv_usec);
            if (execTime > interval)
            {
                /*severe error: cyclic task time violated*/
                waitingTime = 0U;
                /* Log the runtime violation */
                vos_printLog(VOS_LOG_ERROR,
                             "cyclic thread with interval %d usec was running  %d usec\n",
                             interval, execTime);
            }
            else
            {
                waitingTime = interval - execTime;
            }
        }
        else
        {
            /* seems a very critical overflow has happened - or simply a misconfiguration */
            /* as a rough first guess use zero waiting time here */
            waitingTime = 0U;
            /* Have this value range violation logged */
            vos_printLog(VOS_LOG_ERROR,
                         "cyclic thread with interval %d usec exceeded time out by running %ld sec\n",
                         interval, afterCall.tv_sec);
        }
        (void) vos_threadDelay(waitingTime);
        pthread_testcancel();
    }
}

/**********************************************************************************************************************/
/** Initialize the thread library.
 *  Must be called once before any other call
 *
 *  @retval         VOS_NO_ERR          no error
 *  @retval         VOS_INIT_ERR        threading not supported
 */

EXT_DECL VOS_ERR_T vos_threadInit (
    void)
{
    vosThreadInitialised = TRUE;

    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** De-Initialize the thread library.
 *  Must be called after last thread/timer call
 *
 */

EXT_DECL void vos_threadTerm (void)
{
    vosThreadInitialised = FALSE;
}


/**********************************************************************************************************************/
/** Create a thread.
 *  Create a thread and return a thread handle for further requests. Not each parameter may be supported by all
 *  target systems!
 *
 *  @param[out]     pThread         Pointer to returned thread handle
 *  @param[in]      pName           Pointer to name of the thread (optional)
 *  @param[in]      policy          Scheduling policy (FIFO, Round Robin or other)
 *  @param[in]      priority        Scheduling priority (1...255 (highest), default 0)
 *  @param[in]      interval        Interval for cyclic threads in us (optional)
 *  @param[in]      stackSize       Minimum stacksize, default 0: 16kB
 *  @param[in]      pFunction       Pointer to the thread function
 *  @param[in]      pArguments      Pointer to the thread function parameters
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_INIT_ERR    module not initialised
 *  @retval         VOS_NOINIT_ERR  invalid handle
 *  @retval         VOS_PARAM_ERR   parameter out of range/invalid
 *  @retval         VOS_THREAD_ERR  thread creation error
 */

EXT_DECL VOS_ERR_T vos_threadCreate (
    VOS_THREAD_T            *pThread,
    const CHAR8             *pName,
    VOS_THREAD_POLICY_T     policy,
    VOS_THREAD_PRIORITY_T   priority,
    UINT32                  interval,
    UINT32                  stackSize,
    VOS_THREAD_FUNC_T       pFunction,
    void                    *pArguments)
{
    pthread_t           hThread;
    pthread_attr_t      threadAttrib;
    struct sched_param  schedParam;  /* scheduling priority */
    int retCode;

    if (!vosThreadInitialised)
    {
        return VOS_INIT_ERR;
    }

    if (interval > 0)
    {
        vos_printLog(VOS_LOG_ERROR,
                     "%s cyclic threads not implemented yet\n",
                     pName);
        return VOS_INIT_ERR;
    }

    /* Initialize thread attributes to default values */
    retCode = pthread_attr_init(&threadAttrib);
    if (retCode != 0)
    {
        vos_printLog(VOS_LOG_ERROR,
                     "%s pthread_attr_init() failed (Err:%d)\n",
                     pName,
                     retCode );
        return VOS_THREAD_ERR;
    }

   /* Set the stack size */
   if (stackSize > PTHREAD_STACK_MIN)
   {
      if (stackSize % (UINT32)getpagesize() > 0u)
      {
         stackSize = ((stackSize / (UINT32)getpagesize()) + 1u) * (UINT32)getpagesize();
      }
      retCode = pthread_attr_setstacksize(&threadAttrib, (size_t) stackSize);
    }
    else
    {
        retCode = pthread_attr_setstacksize(&threadAttrib, cDefaultStackSize);
    }

    if (retCode != 0)
    {
        vos_printLog(
            VOS_LOG_ERROR,
            "%s pthread_attr_setstacksize() failed (Err:%d)\n",
            pName,
            retCode );
        return VOS_THREAD_ERR;
    }

    /* Detached thread */
    retCode = pthread_attr_setdetachstate(&threadAttrib,
                                          PTHREAD_CREATE_DETACHED);
    if (retCode != 0)
    {
        vos_printLog(
            VOS_LOG_ERROR,
            "%s pthread_attr_setdetachstate() failed (Err:%d)\n",
            pName,
            retCode );
        return VOS_THREAD_ERR;
    }

    /* Set the policy of the thread */
    if (policy != VOS_THREAD_POLICY_OTHER)
    {
        retCode = pthread_attr_setschedpolicy(&threadAttrib, (INT32)policy);
        if (retCode != 0)
        {
            vos_printLog(
                VOS_LOG_ERROR,
                "%s pthread_attr_setschedpolicy(%d) failed (Err:%d)\n",
                pName,
                policy,
                retCode );
            return VOS_THREAD_ERR;
        }
    }

    /* Set the scheduling priority of the thread */
    schedParam.sched_priority = priority;
    retCode = pthread_attr_setschedparam(&threadAttrib, &schedParam);
    if (retCode != 0)
    {
        vos_printLog(
            VOS_LOG_ERROR,
            "%s pthread_attr_setschedparam/priority(%d) failed (Err:%d)\n",
            pName,
            priority,
            retCode );
        /*return VOS_THREAD_ERR; */
    }

    /* Set inheritsched attribute of the thread */
    retCode = pthread_attr_setinheritsched(&threadAttrib,
                                           PTHREAD_EXPLICIT_SCHED);
    if (retCode != 0)
    {
        vos_printLog(
            VOS_LOG_ERROR,
            "%s pthread_attr_setinheritsched() failed (Err:%d)\n",
            pName,
            retCode );
        return VOS_THREAD_ERR;
    }

    /* Create the thread */
    retCode = pthread_create(&hThread, &threadAttrib, (void *(*)(
                                                           void *))pFunction,
                             pArguments);
    if (retCode != 0)
    {
        vos_printLog(VOS_LOG_ERROR,
                     "%s pthread_create() failed (Err:%d)\n",
                     pName,
                     retCode );
        return VOS_THREAD_ERR;
    }

    *pThread = (VOS_THREAD_T) hThread;

    /* Destroy thread attributes */
    retCode = pthread_attr_destroy(&threadAttrib);
    if (retCode != 0)
    {
        vos_printLog(
            VOS_LOG_ERROR,
            "%s pthread_attr_destroy() failed (Err:%d)\n",
            pName,
            retCode );
        return VOS_THREAD_ERR;
    }
    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** Terminate a thread.
 *  This call will terminate the thread with the given threadId and release all resources. Depending on the
 *  underlying architectures, it may just block until the thread ran out.
 *
 *  @param[in]      thread          Thread handle (or NULL if current thread)
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_THREAD_ERR  cancel failed
 */

EXT_DECL VOS_ERR_T vos_threadTerminate (
    VOS_THREAD_T thread)
{
    int retCode;

    retCode = pthread_cancel((pthread_t)thread);
    if (retCode != 0)
    {
        vos_printLog(VOS_LOG_ERROR,
                     "pthread_cancel() failed (Err:%d)\n",
                     retCode );
        return VOS_THREAD_ERR;
    }
    return VOS_NO_ERR;
}


/**********************************************************************************************************************/
/** Is the thread still active?
 *  This call will return VOS_NO_ERR if the thread is still active, VOS_PARAM_ERR in case it ran out.
 *
 *  @param[in]      thread          Thread handle
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   parameter out of range/invalid
 */

EXT_DECL VOS_ERR_T vos_threadIsActive (
    VOS_THREAD_T thread)
{
    int retValue;
    int policy;
    struct sched_param param;

    retValue = pthread_getschedparam((pthread_t)thread, &policy, &param);

    return (retValue == 0 ? VOS_NO_ERR : VOS_PARAM_ERR);
}



/**********************************************************************************************************************/
/*  Timers                                                                                                            */
/**********************************************************************************************************************/

/**********************************************************************************************************************/
/** Delay the execution of the current thread by the given delay in us.
 *
 *
 *  @param[in]      delay           Delay in us
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   parameter out of range/invalid
 */

EXT_DECL VOS_ERR_T vos_threadDelay (
    UINT32 delay)
{
    struct timespec wanted_delay;
    struct timespec remaining_delay;
    int ret;


    if (delay == 0u)
    {
        pthread_testcancel();

        /*    yield cpu to other processes   */
        if (sched_yield() != 0)
        {
            return VOS_PARAM_ERR;
        }
        return VOS_NO_ERR;
    }

    wanted_delay.tv_sec     = delay / 1000000u;
    wanted_delay.tv_nsec    = (delay % 1000000) * 1000;
    do
    {
        pthread_testcancel();
        ret = nanosleep(&wanted_delay, &remaining_delay);
        if (ret == -1 && errno == EINTR)
        {
            wanted_delay = remaining_delay;
        }
    }
    while (errno == EINTR);

    return VOS_NO_ERR;
}


/**********************************************************************************************************************/
/** Return the current time in sec and us
 *
 *
 *  @param[out]     pTime           Pointer to time value
 */

EXT_DECL void vos_getTime (
    VOS_TIME_T *pTime)
{
    struct timeval myTime;

    if (pTime == NULL)
    {
        vos_printLogStr(VOS_LOG_ERROR, "ERROR NULL pointer\n");
    }
    else
    {
#ifndef CLOCK_MONOTONIC

        /*    On systems without monotonic clock support,
            changing the system clock during operation
            might interrupt process data packet transmissions!    */

        /*lint -e(534) ignore return value */
        gettimeofday(&myTime, NULL);

#else

        struct timespec currentTime;

        clock_gettime(CLOCK_MONOTONIC, &currentTime);

        myTime.tv_sec   = currentTime.tv_sec;         \
        myTime.tv_usec  = currentTime.tv_nsec / 1000; \

#endif

        pTime->tv_sec   = myTime.tv_sec;
        pTime->tv_usec  = myTime.tv_usec;
    }
}

/**********************************************************************************************************************/
/** Get a time-stamp string.
 *  Get a time-stamp string for debugging in the form "yyyymmdd-hh:mm:ss.ms"
 *  Depending on the used OS / hardware the time might not be a real-time stamp but relative from start of system.
 *
 *  @retval         timestamp      "yyyymmdd-hh:mm:ss.ms"
 */

EXT_DECL const CHAR8 *vos_getTimeStamp (void)
{
    static char     pTimeString[32] = {0};
    struct timeval  curTime;
    struct tm       *curTimeTM;

    /*lint -e(534) ignore return value */
    gettimeofday(&curTime, NULL);
    curTimeTM = localtime(&curTime.tv_sec);

    if (curTimeTM != NULL)
    {
        /*lint -e(534) ignore return value */
        sprintf(pTimeString, "%04d%02d%02d-%02d:%02d:%02d.%03ld ",
                curTimeTM->tm_year + 1900,
                curTimeTM->tm_mon + 1,
                curTimeTM->tm_mday,
                curTimeTM->tm_hour,
                curTimeTM->tm_min,
                curTimeTM->tm_sec,
                (long) curTime.tv_usec / 1000L);
    }
    return pTimeString;
}


/**********************************************************************************************************************/
/** Clear the time stamp
 *
 *
 *  @param[out]     pTime           Pointer to time value
 */

EXT_DECL void vos_clearTime (
    VOS_TIME_T *pTime)
{
    if (pTime == NULL)
    {
        vos_printLogStr(VOS_LOG_ERROR, "ERROR NULL pointer\n");
    }
    else
    {
        timerclear(pTime);
    }
}

/**********************************************************************************************************************/
/** Add the second to the first time stamp, return sum in first
 *
 *
 *  @param[in, out]     pTime           Pointer to time value
 *  @param[in]          pAdd            Pointer to time value
 */

EXT_DECL void vos_addTime (
    VOS_TIME_T          *pTime,
    const VOS_TIME_T    *pAdd)
{
    if ((pTime == NULL) || (pAdd == NULL))
    {
        vos_printLogStr(VOS_LOG_ERROR, "ERROR NULL pointer\n");
    }
    else
    {
        VOS_TIME_T ltime;

        timeradd(pTime, pAdd, &ltime);
        *pTime = ltime;
    }
}

/**********************************************************************************************************************/
/** Subtract the second from the first time stamp, return diff in first
 *
 *
 *  @param[in, out]     pTime           Pointer to time value
 *  @param[in]          pSub            Pointer to time value
 */

EXT_DECL void vos_subTime (
    VOS_TIME_T          *pTime,
    const VOS_TIME_T    *pSub)
{
    if ((pTime == NULL) || (pSub == NULL))
    {
        vos_printLogStr(VOS_LOG_ERROR, "ERROR NULL pointer\n");
    }
    else
    {
        VOS_TIME_T ltime;

        timersub(pTime, pSub, &ltime);
        *pTime = ltime;
    }
}

/**********************************************************************************************************************/
/** Divide the first time value by the second, return quotient in first
 *
 *
 *  @param[in, out]     pTime           Pointer to time value
 *  @param[in]          divisor         Divisor
 */

EXT_DECL void vos_divTime (
    VOS_TIME_T  *pTime,
    UINT32      divisor)
{
    if ((pTime == NULL) || (divisor == 0))
    {
        vos_printLogStr(VOS_LOG_ERROR, "ERROR NULL pointer/parameter\n");
    }
    else
    {
        UINT32 temp;

        temp = pTime->tv_sec % divisor;
        pTime->tv_sec /= divisor;
        if (temp > 0)
        {
            pTime->tv_usec += temp * 1000000;
        }
        pTime->tv_usec /= (INT32)divisor;
    }
}

/**********************************************************************************************************************/
/** Multiply the first time by the second, return product in first
 *
 *
 *  @param[in, out]     pTime           Pointer to time value
 *  @param[in]          mul             Factor
 */

EXT_DECL void vos_mulTime (
    VOS_TIME_T  *pTime,
    UINT32      mul)
{
    if (pTime == NULL)
    {
        vos_printLogStr(VOS_LOG_ERROR, "ERROR NULL pointer/parameter\n");
    }
    else
    {
        pTime->tv_sec   *= mul;
        pTime->tv_usec  *= mul;
        while (pTime->tv_usec >= 1000000)
        {
            pTime->tv_sec++;
            pTime->tv_usec -= 1000000;
        }
    }
}

/**********************************************************************************************************************/
/** Compare the second to the first time stamp
 *
 *
 *  @param[in, out]     pTime           Pointer to time value
 *  @param[in]          pCmp            Pointer to time value to compare
 *  @retval             0               pTime == pCmp
 *  @retval             -1              pTime < pCmp
 *  @retval             1               pTime > pCmp
 */

EXT_DECL INT32 vos_cmpTime (
    const VOS_TIME_T    *pTime,
    const VOS_TIME_T    *pCmp)
{
    if ((pTime == NULL) || (pCmp == NULL))
    {
        return 0;
    }
    if (timercmp(pTime, pCmp, >))
    {
        return 1;
    }
    if (timercmp(pTime, pCmp, <))
    {
        return -1;
    }
    return 0;
}

/**********************************************************************************************************************/
/** Get a universal unique identifier according to RFC 4122 time based version.
 *
 *
 *  @param[out]     pUuID           Pointer to a universal unique identifier
 */

EXT_DECL void vos_getUuid (
    VOS_UUID_T pUuID)
{
#ifdef __APPLE__
    uuid_generate_time(pUuID);
#else
    /*  Manually creating a UUID from time stamp and MAC address  */
    static UINT16   count = 1u;
    VOS_TIME_T      current;
    VOS_ERR_T       ret;

    vos_getTime(&current);

    pUuID[0]    = current.tv_usec & 0xFFu;
    pUuID[1]    = (current.tv_usec & 0xFF00u) >> 8u;
    pUuID[2]    = (current.tv_usec & 0xFF0000u) >> 16u;
    pUuID[3]    = (current.tv_usec & 0xFF000000u) >> 24u;
    pUuID[4]    = current.tv_sec & 0xFFu;
    pUuID[5]    = (current.tv_sec & 0xFF00u) >> 8u;
    pUuID[6]    = (current.tv_sec & 0xFF0000u) >> 16u;
    pUuID[7]    = ((current.tv_sec & 0x0F000000u) >> 24u) | 0x4u; /*  pseudo-random version   */

    /* we always increment these values, this definitely makes the UUID unique */
    pUuID[8]    = (UINT8) (count & 0xFFu);
    pUuID[9]    = (UINT8) (count >> 8u);
    count++;

    /*  Copy the mac address into the rest of the array */
    ret = vos_sockGetMAC(&pUuID[10]);
    if (ret != VOS_NO_ERR)
    {
        vos_printLog(VOS_LOG_ERROR, "vos_sockGetMAC() failed (Err:%d)\n", ret);
    }
#endif
}


/**********************************************************************************************************************/
/*  Mutex & Semaphores                                                                                                */
/**********************************************************************************************************************/

/**********************************************************************************************************************/
/** Create a recursive mutex.
 *  Return a mutex handle. The mutex will be available at creation.
 *
 *  @param[out]     pMutex          Pointer to mutex handle
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_INIT_ERR    module not initialised
 *  @retval         VOS_PARAM_ERR   pMutex == NULL
 *  @retval         VOS_MUTEX_ERR   no mutex available
 */

EXT_DECL VOS_ERR_T vos_mutexCreate (
    VOS_MUTEX_T *pMutex)
{
    int err = 0;
    pthread_mutexattr_t attr;

    if (pMutex == NULL)
    {
        return VOS_PARAM_ERR;
    }

    *pMutex = (VOS_MUTEX_T) vos_memAlloc(sizeof (struct VOS_MUTEX));

    if (*pMutex == NULL)
    {
        return VOS_MEM_ERR;
    }

    err = pthread_mutexattr_init(&attr);
    if (err == 0)
    {
        err = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        if (err == 0)
        {
            err = pthread_mutex_init((pthread_mutex_t *)&(*pMutex)->mutexId, &attr);
        }
        pthread_mutexattr_destroy(&attr); /*lint !e534 ignore return value */
    }

    if (err == 0)
    {
        (*pMutex)->magicNo = cMutextMagic;
    }
    else
    {
        vos_printLog(VOS_LOG_ERROR, "Can not create Mutex(pthread err=%d)\n", err);
        vos_memFree(*pMutex);
        *pMutex = NULL;
        return VOS_MUTEX_ERR;
    }

    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** Create a recursive mutex.
 *  Fill in a mutex handle. The mutex storage must be already allocated.
 *
 *  @param[out]     pMutex          Pointer to mutex handle
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_INIT_ERR    module not initialised
 *  @retval         VOS_PARAM_ERR   pMutex == NULL
 *  @retval         VOS_MUTEX_ERR   no mutex available
 */

EXT_DECL VOS_ERR_T vos_mutexLocalCreate (
    struct VOS_MUTEX *pMutex)
{
    int err = 0;
    pthread_mutexattr_t attr;

    if (pMutex == NULL)
    {
        return VOS_PARAM_ERR;
    }

    err = pthread_mutexattr_init(&attr);
    if (err == 0)
    {
        err = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        if (err == 0)
        {
            err = pthread_mutex_init((pthread_mutex_t *)&pMutex->mutexId, &attr);
        }
        pthread_mutexattr_destroy(&attr); /*lint !e534 ignore return value */
    }

    if (err == 0)
    {
        pMutex->magicNo = cMutextMagic;
    }
    else
    {
        vos_printLog(VOS_LOG_ERROR, "Can not create Mutex(pthread err=%d)\n", err);
        return VOS_MUTEX_ERR;
    }

    return VOS_NO_ERR;
}


/**********************************************************************************************************************/
/** Delete a mutex.
 *  Release the resources taken by the mutex.
 *
 *  @param[in]      pMutex          mutex handle
 */

EXT_DECL void vos_mutexDelete (
    VOS_MUTEX_T pMutex)
{
    if ((pMutex == NULL) || (pMutex->magicNo != cMutextMagic))
    {
        vos_printLogStr(VOS_LOG_ERROR, "vos_mutexDelete() ERROR invalid parameter");
    }
    else
    {
        int err;

        err = pthread_mutex_destroy((pthread_mutex_t *)&pMutex->mutexId);
        if (err == 0)
        {
            pMutex->magicNo = 0;
            vos_memFree(pMutex);
        }
        else
        {
            vos_printLog(VOS_LOG_ERROR, "Can not destroy Mutex (pthread err=%d)\n", err);
        }
    }
}


/**********************************************************************************************************************/
/** Delete a mutex.
 *  Release the resources taken by the mutex.
 *
 *  @param[in]      pMutex          Pointer to mutex struct
 */

EXT_DECL void vos_mutexLocalDelete (
    struct VOS_MUTEX *pMutex)
{
    if ((pMutex == NULL) || (pMutex->magicNo != cMutextMagic))
    {
        vos_printLogStr(VOS_LOG_ERROR, "vos_mutexLocalDelete() ERROR invalid parameter");
    }
    else
    {
        int err;

        err = pthread_mutex_destroy((pthread_mutex_t *)&pMutex->mutexId);
        if (err == 0)
        {
            pMutex->magicNo = 0;
        }
        else
        {
            vos_printLog(VOS_LOG_ERROR, "Can not destroy Mutex (pthread err=%d)\n", err);
        }
    }
}


/**********************************************************************************************************************/
/** Take a mutex.
 *  Wait for the mutex to become available (lock).
 *
 *  @param[in]      pMutex          mutex handle
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   pMutex == NULL or wrong type
 *  @retval         VOS_MUTEX_ERR   no such mutex
 */

EXT_DECL VOS_ERR_T vos_mutexLock (
    VOS_MUTEX_T pMutex)
{
    int err;

    if ((pMutex == NULL) || (pMutex->magicNo != cMutextMagic))
    {
        return VOS_PARAM_ERR;
    }

    err = pthread_mutex_lock((pthread_mutex_t *)&pMutex->mutexId);
    if (err != 0)
    {
        vos_printLog(VOS_LOG_ERROR, "Unable to lock Mutex (pthread err=%d)\n", err);
        return VOS_MUTEX_ERR;
    }

    return VOS_NO_ERR;
}


/**********************************************************************************************************************/
/** Try to take a mutex.
 *  If mutex is can't be taken VOS_MUTEX_ERR is returned.
 *
 *  @param[in]      pMutex          mutex handle
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   pMutex == NULL or wrong type
 *  @retval         VOS_MUTEX_ERR   mutex not locked
 */

EXT_DECL VOS_ERR_T vos_mutexTryLock (
    VOS_MUTEX_T pMutex)
{
    int err;

    if ((pMutex == NULL) || (pMutex->magicNo != cMutextMagic))
    {
        return VOS_PARAM_ERR;
    }

    err = pthread_mutex_trylock((pthread_mutex_t *)&pMutex->mutexId);
    if (err == EBUSY)
    {
        return VOS_MUTEX_ERR;
    }
    if (err == EINVAL)
    {
        vos_printLog(VOS_LOG_ERROR, "Unable to trylock Mutex (pthread err=%d)\n", err);
        return VOS_MUTEX_ERR;
    }

    return VOS_NO_ERR;
}


/**********************************************************************************************************************/
/** Release a mutex.
 *  Unlock the mutex.
 *
 *  @param[in]      pMutex          mutex handle
 */

EXT_DECL VOS_ERR_T vos_mutexUnlock (
    VOS_MUTEX_T pMutex)
{

    if (pMutex == NULL || pMutex->magicNo != cMutextMagic)
    {
        vos_printLogStr(VOS_LOG_ERROR, "vos_mutexUnlock() ERROR invalid parameter");
        return VOS_PARAM_ERR;
    }
    else
    {
        int err;

        err = pthread_mutex_unlock((pthread_mutex_t *)&pMutex->mutexId);
        if (err != 0)
        {
            vos_printLog(VOS_LOG_ERROR, "Unable to unlock Mutex (pthread err=%d)\n", err);
            return VOS_MUTEX_ERR;
        }
    }

    return VOS_NO_ERR;
}



/**********************************************************************************************************************/
/** Create a semaphore.
 *  Return a semaphore handle. Depending on the initial state the semaphore will be available on creation or not.
 *
 *  @param[out]     ppSema          Pointer to semaphore pointer
 *  @param[in]      initialState    The initial state of the sempahore
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_INIT_ERR    module not initialised
 *  @retval         VOS_PARAM_ERR   parameter out of range/invalid
 *  @retval         VOS_SEMA_ERR    no semaphore available
 */

EXT_DECL VOS_ERR_T vos_semaCreate (
    VOS_SEMA_T          *ppSema,
    VOS_SEMA_STATE_T    initialState)
{
    VOS_ERR_T   retVal  = VOS_SEMA_ERR;
    int         rc      = 0;

    /*Check parameters*/
    if (ppSema == NULL)
    {
        vos_printLogStr(VOS_LOG_ERROR, "vos_SemaCreate() ERROR invalid parameter pSema == NULL\n");
        retVal = VOS_PARAM_ERR;
    }
    else if ((initialState != VOS_SEMA_EMPTY) && (initialState != VOS_SEMA_FULL))
    {
        vos_printLogStr(VOS_LOG_ERROR, "vos_SemaCreate() ERROR invalid parameter initialState\n");
        retVal = VOS_PARAM_ERR;
    }
    else
    {
        /*pThread Semaphore init*/
#ifdef __APPLE__
        static int  count = 1;
        char        tempPath[64];
        sprintf(tempPath, "/tmp/trdp%d.sema", count++);
        *ppSema = (VOS_SEMA_T) sem_open(tempPath, O_CREAT, 0644u, (UINT8)initialState);
        if ((sem_t *)*ppSema == SEM_FAILED)
        {
            rc = -1;
        }
#else
        /*Parameters are OK*/
        *ppSema = (VOS_SEMA_T) vos_memAlloc(sizeof (VOS_SEMA_T));

        if (*ppSema == NULL)
        {
            return VOS_MEM_ERR;
        }

        rc = sem_init((sem_t *)*ppSema, 0, (UINT8)initialState);
#endif
        if (0 != rc)
        {
            /*Semaphore init failed*/
            vos_printLog(VOS_LOG_ERROR, "vos_semaCreate() ERROR (%d) Semaphore could not be initialized\n", errno);
            retVal = VOS_SEMA_ERR;
        }
        else
        {
            /*Semaphore init successful*/
            retVal = VOS_NO_ERR;
        }
    }
    return retVal;
}


/**********************************************************************************************************************/
/** Delete a semaphore.
 *  This will eventually release any processes waiting for the semaphore.
 *
 *  @param[in]      sema            semaphore handle
 */

EXT_DECL void vos_semaDelete (VOS_SEMA_T sema)
{
    int rc = 0;

    /* Check parameter */
    if (sema == NULL)
    {
        vos_printLogStr(VOS_LOG_ERROR, "vos_semaDelete() ERROR invalid parameter\n");
    }
    else
    {
#ifdef __APPLE__
        rc = sem_close((sem_t *)sema);
        if (0 != rc)
        {
            /* Error closing Semaphore */
            vos_printLogStr(VOS_LOG_ERROR, "vos_semaDelete() ERROR sem_close failed\n");
        }
        else
        {
            /* Semaphore deleted successfully, free allocated memory */
            sem_unlink("/tmp/trdp.sema");
        }
#else
        int sval = 0;
        /* Check if this is a valid semaphore handle*/
        rc = sem_getvalue((sem_t *)sema, &sval);
        if (0 == rc)
        {
            rc = sem_destroy((sem_t *)sema);
            if (0 != rc)
            {
                /* Error destroying Semaphore */
                vos_printLogStr(VOS_LOG_ERROR, "vos_semaDelete() ERROR CloseHandle failed\n");
            }
            else
            {
                /* Semaphore deleted successfully, free allocated memory */
                vos_memFree(sema);
            }
        }
#endif
    }
    return;
}


/**********************************************************************************************************************/
/** Take a semaphore.
 *  Try to get (decrease) a semaphore.
 *
 *  @param[in]      sema            semaphore handle
 *  @param[in]      timeout         Max. time in us to wait, 0 means no wait
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_INIT_ERR    module not initialised
 *  @retval         VOS_NOINIT_ERR  invalid handle
 *  @retval         VOS_PARAM_ERR   parameter out of range/invalid
 *  @retval         VOS_SEMA_ERR    could not get semaphore in time
 */

EXT_DECL VOS_ERR_T vos_semaTake (
    VOS_SEMA_T  sema,
    UINT32      timeout)
{
    int             rc              = 0;
    VOS_ERR_T       retVal          = VOS_SEMA_ERR;
    struct timespec waitTimeSpec    = {0u, 0};

    /* Check parameter */
    if (sema == NULL)
    {
        vos_printLogStr(VOS_LOG_ERROR, "vos_semaTake() ERROR invalid parameter 'sema' == NULL\n");
        return VOS_PARAM_ERR;
    }
    else if (timeout == 0u)
    {
        /* Take Semaphore, return ERROR if Semaphore cannot be taken immediately instead of blocking */
        rc = sem_trywait((sem_t *)sema);
    }
    else if (timeout == VOS_SEMA_WAIT_FOREVER)
    {
        /* Take Semaphore, block until Semaphore becomes available */
        rc = sem_wait((sem_t *)sema);
    }
    else
    {
        /* Get time and convert it to timespec format */
#ifdef __APPLE__
        VOS_TIME_T waitTimeVos = {0u, 0};
        vos_getTime(&waitTimeVos);
        waitTimeSpec.tv_sec     = waitTimeVos.tv_sec;
        waitTimeSpec.tv_nsec    = waitTimeVos.tv_usec * NSECS_PER_USEC;
#else
        clock_gettime(CLOCK_REALTIME, &waitTimeSpec);
#endif

        /* add offset */
        if (timeout >= (USECS_PER_MSEC * MSECS_PER_SEC))
        {
            /* Timeout longer than 1 sec, add sec and nsec seperately */
            waitTimeSpec.tv_sec     += timeout / (USECS_PER_MSEC * MSECS_PER_SEC);
            waitTimeSpec.tv_nsec    += (timeout % (USECS_PER_MSEC * MSECS_PER_SEC)) * NSECS_PER_USEC;
        }
        else
        {
            /* Timeout shorter than 1 sec, only add nsecs */
            waitTimeSpec.tv_nsec += timeout * NSECS_PER_USEC;
        }
        /* Carry if tv_nsec > 1.000.000.000 */
        if (waitTimeSpec.tv_nsec >= NSECS_PER_USEC * USECS_PER_MSEC * MSECS_PER_SEC)
        {
            waitTimeSpec.tv_sec++;
            waitTimeSpec.tv_nsec -= NSECS_PER_USEC * USECS_PER_MSEC * MSECS_PER_SEC;
        }
        else
        {
            /* carry not necessary */
        }
        /* take semaphore with specified timeout */
        /* BL 2013-05-06:
           This call will fail under LINUX, because it depends on CLOCK_REALTIME (opposed to CLOCK_MONOTONIC)!
        */
#ifdef __QNXNTO__
        rc = sem_timedwait_monotonic((sem_t *)sema, &waitTimeSpec);
#else
        /* BL 2013-11-28:
           Currently, under Linux, there is no semaphore call which will work with CLOCK_MONOTONIC; the semaphore
           will fail if the clock was changed by the system (NTP, adjtime etc.).
           See also http://sourceware.org/bugzilla/show_bug.cgi?id=14717
        */
        rc = sem_timedwait((sem_t *)sema, &waitTimeSpec);
#endif
    }
    if (0 != rc)
    {
        /* Could not take Semaphore in time */
        retVal = VOS_SEMA_ERR;
    }
    else
    {
        /* Semaphore take success */
        retVal = VOS_NO_ERR;
    }
    return retVal;
}



/**********************************************************************************************************************/
/** Give a semaphore.
 *  Release (increase) a semaphore.
 *
 *  @param[in]      sema            semaphore handle
 */

EXT_DECL void vos_semaGive (
    VOS_SEMA_T sema)
{
    int rc = 0;

    /* Check parameter */
    if (sema == NULL)
    {
        vos_printLogStr(VOS_LOG_ERROR, "vos_semaGive() ERROR invalid parameter 'sema' == NULL\n");
    }
    else
    {
        /* release semaphore */
        rc = sem_post((sem_t *)sema);
        if (0 == rc)
        {
            /* Semaphore released */
        }
        else
        {
            /* Could not release Semaphore */
            vos_printLog(VOS_LOG_ERROR, "vos_semaGive() ERROR (%d) could not release semaphore\n", errno);
        }
    }
    return;
}
