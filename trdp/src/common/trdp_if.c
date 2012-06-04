/******************************************************************************/
/**
 * @file            trdp_if.c
 *
 * @brief           Functions for ECN communication
 *
 * @details
 *
 * @note            Project: TCNOpen TRDP prototype stack
 *
 * @author          Bernd Loehr, NewTec GmbH
 *
 * @remarks All rights reserved. Reproduction, modification, use or disclosure
 *          to third parties without express authority is forbidden,
 *          Copyright Bombardier Transportation GmbH, Germany, 2012.
 *
 *
 * $Id$
 *
 */

/*******************************************************************************
 * INCLUDES
 */

#include <string.h>
#include <sys/select.h>
#include <arpa/inet.h>

#include "trdp_types.h"
#include "trdp_if_light.h"
#include "trdp_utils.h"
#include "trdp_pdcom.h"
#include "vos_thread.h"
#include "vos_sock.h"

/*******************************************************************************
 * TYPEDEFS
 */

/******************************************************************************
 * LOCALS
 */

static TRDP_APP_SESSION_T   sSession        = NULL;
static VOS_MUTEX_T          sSessionMutex   = NULL;
static UINT32 sTopoCount = 0;

/******************************************************************************
 * LOCAL FUNCTIONS
 */

/******************************************************************************
 * GLOBAL FUNCTIONS
 */

/******************************************************************************/
/** Check if the session handle is valid
 *
 *
 *  @param[in]      pSessionHandle		pointer to packet data (dataset)
 *	@retval			TRUE		is valid
 *	@retval			FALSE		is invalid
 */
BOOL    trdp_isValidSession (
    TRDP_APP_SESSION_T pSessionHandle)
{
    TRDP_SESSION_PT pSession = NULL;
    BOOL found = FALSE;

    if (pSessionHandle == NULL)
    {
        return FALSE;
    }

    if (vos_mutexLock(sSessionMutex) != VOS_NO_ERR)
    {
        return FALSE;
    }

    pSession = sSession;

    while (pSession)
    {
        if (pSession == (TRDP_SESSION_PT) pSessionHandle)
        {
            found = TRUE;
            break;
        }
        pSession = pSession->pNext;
    }
    vos_mutexUnlock(sSessionMutex);
    return found;
}

/******************************************************************************/
/** Get the session queue head pointer
 *
 *	@retval			&sSession
 */
TRDP_APP_SESSION_T *trdp_sessionQueue (void)
{
    return &sSession;
}

/**********************************************************************************************************************/
/** Initialize the TRDP stack.
 *
 *	tlc_init returns in pAppHandle a unique handle to be used in further calls to the stack.
 *
 *  @param[out]     pAppHandle          A handle for further calls to the trdp stack
 *  @param[in]      ownIpAddr           Own IP address, can be different for each process
 *                                      in multiprocessing systems
 *  @param[in]      leaderIpAddr		IP address of redundancy leader
 *  @param[in]      pPrintDebugString	Pointer to debug print function
 *  @param[in]      pMarshall           Pointer to marshalling configuration
 *  @param[in]      pPdDefault          Pointer to default PD configuration
 *  @param[in]      pMdDefault          Pointer to default MD configuration
 *  @param[in]      pMemConfig          Pointer to memory configuration
 *  @param[in]      option              options for library behavior
 *  @retval         TRDP_NO_ERR			no error
 *  @retval         TRDP_MEM_ERR		memory allocation failed
 *  @retval         TRDP_PARAM_ERR		initialization error
 *  @retval         TRDP_SOCK_ERR		socket error
 */
EXT_DECL TRDP_ERR_T tlc_init (
    TRDP_APP_SESSION_T              *pAppHandle,
    TRDP_IP_ADDR_T                  ownIpAddr,
    TRDP_IP_ADDR_T                  leaderIpAddr,
    const TRDP_PRINT_DBG_T          pPrintDebugString,
    const TRDP_MARSHALL_CONFIG_T    *pMarshall,
    const TRDP_PD_CONFIG_T          *pPdDefault,
    const TRDP_MD_CONFIG_T          *pMdDefault,
    const TRDP_MEM_CONFIG_T         *pMemConfig,
    TRDP_OPTION_T                   option)
{
    TRDP_ERR_T      ret         = TRDP_NO_ERR;
    TRDP_SESSION_PT pSession    = NULL;

    if (pAppHandle == NULL)
    {
        vos_printf(VOS_LOG_ERROR, "TRDP init failed\n");
        return TRDP_PARAM_ERR;
    }

    /*	Only the first session will allocate memory	and the mutex */
    if (sSession == NULL)
    {
        /*	Initialize VOS	*/
        vos_init(NULL, pPrintDebugString);

        if (pMemConfig == NULL)
        {
            vos_memInit(NULL, 0, NULL);
        }
        else
        {
            vos_memInit(pMemConfig->p, pMemConfig->size, pMemConfig->prealloc);
        }
        VOS_ERR_T err = vos_mutexCreate(&sSessionMutex);
        if (err != VOS_NO_ERR)
        {
            vos_printf(VOS_LOG_ERROR,
                       "TRDP init failed while creating session mutex\n");
            return TRDP_PARAM_ERR;
        }

    }

    pSession = (TRDP_SESSION_PT) vos_memAlloc(sizeof(TRDP_SESSION_T));
    if (pSession == NULL)
    {
        /* vos_memDelete(NULL); */
        vos_printf(VOS_LOG_ERROR, "TRDP init failed\n");
        return TRDP_MEM_ERR;
    }

    memset(pSession, 0, sizeof(TRDP_SESSION_T));

    pSession->realIP    = ownIpAddr;
    pSession->virtualIP = leaderIpAddr;

    if (pMarshall != NULL)
    {
        pSession->marshall = *pMarshall;
    }

    if (pPdDefault != NULL)
    {
        pSession->pdDefault = *pPdDefault;
    }
    else
    {
        pSession->pdDefault.sendParam.qos   = PD_DEFAULT_QOS;
        pSession->pdDefault.sendParam.ttl   = PD_DEFAULT_TTL;
        pSession->pdDefault.port = IP_PD_UDP_PORT;
    }

    if (pMdDefault != NULL)
    {
        pSession->mdDefault = *pMdDefault;
    }
    else
    {
        pSession->mdDefault.replyTimeout    = MD_DEFAULT_REPLY_TIMEOUT;
        pSession->mdDefault.confirmTimeout  = MD_DEFAULT_CONFIRM_TIMEOUT;
        pSession->mdDefault.udpPort         = IP_MD_UDP_PORT;
        pSession->mdDefault.tcpPort         = IP_MD_UDP_PORT;
        pSession->mdDefault.sendParam.qos   = MD_DEFAULT_QOS;
        pSession->mdDefault.sendParam.ttl   = MD_DEFAULT_TTL;
    }


    if (vos_mutexCreate(&pSession->mutex) != VOS_NO_ERR)
    {
        vos_printf(VOS_LOG_ERROR,
                   "TRDP init failed while creating session mutex\n");
        return TRDP_PARAM_ERR;
    }

    vos_clearTime(&pSession->interval);
    vos_clearTime(&pSession->nextJob);

    /*	Clear the socket pool	*/
    trdp_initSockets(pSession->iface);

#if MD_SUPPORT

    ret = trdp_initMD(pSession);
    if (ret != TRDP_NO_ERR)
    {
        vos_printf(VOS_LOG_ERROR, "TRDP initMD failed\n");
        vos_memFree(pSession);
        vos_memDelete(NULL);
        return ret;
    }
#endif

    /*	Queue the session in	*/
    vos_mutexLock(sSessionMutex);
    pSession->pNext = sSession;
    sSession        = pSession;
    *pAppHandle     = pSession;
    vos_mutexUnlock(sSessionMutex);

    vos_printf(VOS_LOG_INFO,
               "TRDP Stack Version %s: successfully initiated\n",
               LIB_VERSION);

    return ret;
}

/******************************************************************************/
/** Un-Initialize
 *  Clean up when app quits. Mainly used for debugging/test runs
 *
 */
TRDP_ERR_T tlc_terminate (
    TRDP_APP_SESSION_T appHandle)
{
    TRDP_SESSION_PT pSession = NULL;
    int found = FALSE;
    TRDP_ERR_T      ret = TRDP_NOINIT_ERR;

    /*	Find the session	*/

    if (appHandle == NULL)
    {
        return TRDP_PARAM_ERR;
    }

    vos_mutexLock(sSessionMutex);

    pSession = sSession;

    if (sSession == (TRDP_SESSION_PT) appHandle)
    {
        sSession    = sSession->pNext;
        pSession    = (TRDP_SESSION_PT) appHandle;
        found       = TRUE;
    }
    else
    {

        while (pSession)
        {
            if (pSession->pNext == (TRDP_SESSION_PT) appHandle)
            {
                pSession->pNext = pSession->pNext->pNext;
                found = TRUE;
                break;
            }
            pSession = pSession->pNext;
        }
    }

    /*	At this point we removed the session from the queue	*/
    if (found)
    {
        pSession = (TRDP_SESSION_PT) appHandle;

        /*	Take the session mutex to prevent someone sitting on the branch while we cut it	*/
        vos_mutexLock(pSession->mutex);

        /*	Release all allocated sockets and memory	*/
        while (pSession->pSndQueue != NULL)
        {
            PD_ELE_T *pNext = pSession->pSndQueue->pNext;

            /*	Only close socket if not used anymore	*/
            trdp_releaseSocket(pSession->iface, pSession->pSndQueue->socketIdx);
            vos_memFree(pSession->pSndQueue);
            pSession->pSndQueue = pNext;
        }

        vos_mutexUnlock(pSession->mutex);
        vos_mutexDelete(pSession->mutex);
        vos_memFree(pSession);

        ret = TRDP_NO_ERR;
    }

    vos_mutexUnlock(sSessionMutex);

    return ret;
}

/******************************************************************************/
/** Re-Initialize
 *  Should be called by the application when a link-down/link-up event
 *	has occured during normal operation.
 *	We re-join
 *
 */
TRDP_ERR_T tlc_reinit (
    TRDP_APP_SESSION_T appHandle)
{
    PD_ELE_T *iterPD;

    if (!trdp_isValidSession(appHandle))
    {
        return TRDP_NOINIT_ERR;
    }

    if (vos_mutexLock(appHandle->mutex) != VOS_NO_ERR)
    {
        return TRDP_NOINIT_ERR;
    }

    /*	Walk over the registered PDs */
    for (iterPD = appHandle->pSndQueue; iterPD != NULL; iterPD = iterPD->pNext)
    {
        if (iterPD->privFlags & TRDP_MC_JOINT &&
            iterPD->socketIdx != -1)
        {
            /*	Join the MC group again	*/
            vos_sockJoinMC(appHandle->iface[iterPD->socketIdx].sock,
                           iterPD->addr.mcGroup,
                           0);
        }
    }

    vos_mutexUnlock(appHandle->mutex);

    return TRDP_NO_ERR;
}

const char *tlc_getVersion (void)
{
    return LIB_VERSION;
}

/**********************************************************************************************************************/
/** Do not send non-redundant PDs when we are follower.
 *
 *  @param[in]      appHandle           the handle returned by tlc_init
 *  @param[in]      redId               will be set for all ComID's with the given redId, 0 to change for all redId
 *  @param[in]      leader              TRUE if we send
 *
 *  @retval         TRDP_NO_ERR	        no error
 *  @retval         TRDP_PARAM_ERR      parameter error / redId not existing
 *  @retval         TRDP_NOINIT_ERR		handle invalid
 */
TRDP_ERR_T tlp_setRedundant (
    TRDP_APP_SESSION_T  appHandle,
    UINT32              redId,
    BOOL                leader)
{
    if (!trdp_isValidSession(appHandle))
    {
        return TRDP_NOINIT_ERR;
    }

    if (vos_mutexLock(appHandle->mutex) != VOS_NO_ERR)
    {
        return TRDP_NOINIT_ERR;
    }

    /*	TBD! Handle list of redundant comIds	*/

    appHandle->beQuiet  = !leader;
    appHandle->redID    = redId;

    vos_mutexUnlock(appHandle->mutex);
    return TRDP_NO_ERR;
}

/**********************************************************************************************************************/
/** Get status of redundant ComIds.
 *
 *  @param[in]      appHandle           the handle returned by tlc_init
 *  @param[in]      redId               will be returned for all ComID's with the given redId, 0 for all redId
 *  @param[in,out]  pLeader             TRUE if we send (leader)
 *
 *  @retval         TRDP_NO_ERR	        no error
 *  @retval         TRDP_PARAM_ERR      parameter error / redId not existing
 *  @retval         TRDP_NOINIT_ERR		handle invalid
 */
EXT_DECL TRDP_ERR_T tlp_getRedundant (
    TRDP_APP_SESSION_T  appHandle,
    UINT32              redId,
    BOOL                *pLeader)
{
    if (pLeader == NULL)
    {
        return TRDP_PARAM_ERR;
    }
    if (!trdp_isValidSession(appHandle))
    {
        return TRDP_NOINIT_ERR;
    }

    if (vos_mutexLock(appHandle->mutex) != VOS_NO_ERR)
    {
        return TRDP_NOINIT_ERR;
    }

    /*	TBD! Search list of redundant comIds	*/
    *pLeader = !appHandle->beQuiet;

    vos_mutexUnlock(appHandle->mutex);

    return TRDP_NO_ERR;
}

/******************************************************************************/
/** Set new topocount for trainwide communication
 *
 *	This value is used for validating outgoing and incoming packets only!
 *
 *  @param[in]      topoCount			New topoCount value
 */
void       tlc_setTopoCount (UINT32 topoCount)
{
    sTopoCount = topoCount;
}

/**********************************************************************************************************************/
/** Prepare for sending PD messages.
 *  Queue a PD message, it will be send when trdp_work has been called
 *
 *  @param[in]      appHandle           the handle returned by tlc_init
 *  @param[out]     pPubHandle			returned handle for related unprepare
 *  @param[in]      comId				comId of packet to send
 *  @param[in]      topoCount			valid topocount, 0 for local consist
 *  @param[in]      srcIpAddr			own IP address, 0 - srcIP will be set by the stack
 *  @param[in]      destIpAddr			where to send the packet to
 *  @param[in]      interval			frequency of PD packet (>= 10ms) in usec
 *  @param[in]      redId		        0 - Non-redundant, > 0 valid redundancy group
 *  @param[in]      pktFlags            OPTIONS: TRDP_FLAGS_MARSHALL, TRDP_FLAGS_CALLBACK
 *  @param[in]      pSendParam          optional pointer to send parameter, NULL - default parameters are used
 *  @param[in]      pData               pointer to packet data / dataset
 *  @param[in]      dataSize            size of packet data <= 1436 without FCS
 *  @param[in]      subs                substitution (Ladder)
 *  @param[in]      offsetAddress       offset (Ladder)
 *
 *  @retval         TRDP_NO_ERR	        no error
 *  @retval         TRDP_PARAM_ERR      parameter error
 *  @retval         TRDP_MEM_ERR		could not insert (out of memory)
 *  @retval         TRDP_NOINIT_ERR		handle invalid
 *  @retval         TRDP_NOPUB_ERR		Already published
 */
EXT_DECL TRDP_ERR_T tlp_publish (
    TRDP_APP_SESSION_T      appHandle,
    TRDP_PUB_T              *pPubHandle,
    UINT32                  comId,
    UINT32                  topoCount,
    TRDP_IP_ADDR_T          srcIpAddr,
    TRDP_IP_ADDR_T          destIpAddr,
    UINT32                  interval,
    UINT32                  redId,
    TRDP_FLAGS_T            pktFlags,
    const TRDP_SEND_PARAM_T *pSendParam,
    const UINT8             *pData,
    UINT32                  dataSize,
    BOOL                    subs,
    UINT16                  offsetAddress)
{
    PD_ELE_T        *pNewElement = NULL;
    TRDP_TIME_T     nextTime;
    TRDP_TIME_T     tv_interval;
    TRDP_ERR_T      ret         = TRDP_NO_ERR;
    TRDP_ADDRESSES  pubHandle   = {comId, srcIpAddr, destIpAddr, 0};

    /*	Check params	*/
    if (comId == 0)
    {
        return TRDP_PARAM_ERR;
    }

    if (pData != NULL &&
        (dataSize == 0 || interval < TIMER_GRANULARITY))
    {
        return TRDP_PARAM_ERR;
    }

    if (pPubHandle == NULL)
    {
        return TRDP_PARAM_ERR;
    }

    if (!trdp_isValidSession(appHandle))
    {
        return TRDP_NOINIT_ERR;
    }

    /*	Reserve mutual access	*/

    if (vos_mutexLock(appHandle->mutex) != VOS_NO_ERR)
    {
        return TRDP_NOINIT_ERR;
    }

    /*	Look for existing element	*/

    if (trdp_queue_find_addr(appHandle->pSndQueue, &pubHandle) != NULL)
    {
        ret = TRDP_NOPUB_ERR;
    }
    else
    {
        pNewElement = (PD_ELE_T *) vos_memAlloc(sizeof(PD_ELE_T));
        if (pNewElement == NULL)
        {
            ret = TRDP_MEM_ERR;
        }
        else
        {
            /*
               Compute the overal packet size
               Add padding bytes to align to 32 bits
             */
            pNewElement->dataSize   = dataSize;
            pNewElement->grossSize  = trdp_packetSizePD(dataSize);

            /*	Get a socket	*/
            ret = trdp_requestSocket(
                    appHandle->iface,
                    (pSendParam !=
                     NULL) ? pSendParam : &appHandle->
                    pdDefault.sendParam,
                    srcIpAddr,
                    TRDP_SOCK_PD,
                    appHandle->option,
                    &pNewElement->socketIdx);

            if (ret != TRDP_NO_ERR)
            {
                vos_memFree(pNewElement);
                pNewElement = NULL;
            }
        }
    }

    /*	Get the current time and compute the next time this packet should be sent.	*/

    if (ret == TRDP_NO_ERR && pNewElement != NULL)
    {
        vos_getTime(&nextTime);
        tv_interval.tv_sec  = interval / 1000000;
        tv_interval.tv_usec = interval % 1000000;
        vos_addTime(&nextTime, &tv_interval);

        /*	Update the internal data */
        pNewElement->addr       = pubHandle;
        pNewElement->timeToGo   = nextTime;
        pNewElement->interval   = tv_interval;
        pNewElement->pktFlags   = pktFlags;
        pNewElement->privFlags  = TRDP_PRIV_NONE;

        /*	Compute the header fields */
        trdp_pdInit(pNewElement, TRDP_MSG_PD, topoCount);

        /*	Insert at front	*/
        trdp_queue_ins_first(&appHandle->pSndQueue, pNewElement);

        *pPubHandle = &pNewElement->addr;

        ret = tlp_put(appHandle, *pPubHandle, pData, dataSize);
    }

    vos_mutexUnlock(appHandle->mutex);

    return ret;
}

/**********************************************************************************************************************/
/** Stop sending PD messages.
 *
 *  @param[in]      appHandle           the handle returned by tlc_init
 *  @param[in]      pubHandle			the handle returned by prepare
 *
 *  @retval         TRDP_NO_ERR	        no error
 *  @retval         TRDP_PARAM_ERR      parameter error
 *  @retval         TRDP_NOPUB_ERR		not published
 *  @retval         TRDP_NOINIT_ERR		handle invalid
 */

TRDP_ERR_T  tlp_unpublish (
    TRDP_APP_SESSION_T  appHandle,
    TRDP_PUB_T          pubHandle)
{
    PD_ELE_T    *pElement;
    TRDP_ERR_T  ret = TRDP_NOPUB_ERR;

    if (pubHandle == NULL)
    {
        return TRDP_PARAM_ERR;
    }

    if (!trdp_isValidSession(appHandle))
    {
        return TRDP_NOINIT_ERR;
    }

    /*	Reserve mutual access	*/
    if (vos_mutexLock(appHandle->mutex) != VOS_NO_ERR)
    {
        return TRDP_NOINIT_ERR;
    }

    /*	Remove from queue?	*/
    pElement = trdp_queue_find_addr(appHandle->pSndQueue, pubHandle);
    if (pElement != NULL)
    {
        trdp_queue_del_element(&appHandle->pSndQueue, pElement);
        vos_memFree(pElement);
        ret = TRDP_NO_ERR;
    }

    vos_mutexUnlock(appHandle->mutex);

    return ret;      /*	Not found	*/
}

/**********************************************************************************************************************/
/** Update the process data to send.
 *  Update previously published data. The new telegram will be sent earliest when tlc_process is called.
 *
 *  @param[in]      appHandle           the handle returned by tlc_init
 *  @param[in]      pubHandle			the handle returned by publish
 *  @param[in,out]	pData				pointer to application's data buffer
 *  @param[in,out]  dataSize			size of data
 *
 *  @retval         TRDP_NO_ERR	        no error
 *  @retval         TRDP_PARAM_ERR      parameter error
 *  @retval         TRDP_NOPUB_ERR		not published
 *  @retval         TRDP_NOINIT_ERR		handle invalid
 */
TRDP_ERR_T tlp_put (
    TRDP_APP_SESSION_T  appHandle,
    TRDP_PUB_T          pubHandle,
    const UINT8         *pData,
    UINT32              dataSize)
{
    PD_ELE_T    *pElement;
    TRDP_ERR_T  ret = TRDP_NOPUB_ERR;

    if (pubHandle == NULL || pData == NULL || dataSize == 0)
    {
        return TRDP_PARAM_ERR;
    }

    if (!trdp_isValidSession(appHandle))
    {
        return TRDP_NOINIT_ERR;
    }

    /*	Reserve mutual access	*/
    if (vos_mutexLock(appHandle->mutex) != VOS_NO_ERR)
    {
        return TRDP_NOINIT_ERR;
    }

    /*	Find the published queue entry	*/
    pElement = trdp_queue_find_addr(appHandle->pSndQueue, pubHandle);

    if (pElement != NULL)
    {
        ret = trdp_pdPut(pElement,
                         appHandle->marshall.pCbMarshall,
                         appHandle->marshall.pRefCon,
                         pData,
                         dataSize);
    }

    vos_mutexUnlock(appHandle->mutex);

    return ret;      /*	Not found	*/
}

/**********************************************************************************************************************/
/** Get the lowest time interval for PDs.
 *  Return the maximum time interval suitable for 'select()' so that we
 *	can send due PD packets in time.
 *	If the PD send queue is empty, return zero time
 *
 *  @param[in]      appHandle           The handle returned by tlc_init
 *  @param[out]     pInterval			pointer to needed interval
 *  @param[in,out]  pFileDesc			pointer to file descriptor set
 *  @param[out]     pNoDesc				pointer to put no of used descriptors (for select())
 *  @retval         TRDP_NO_ERR			no error
 *  @retval         TRDP_NOINIT_ERR		handle invalid
 */
EXT_DECL TRDP_ERR_T tlc_getInterval (
    TRDP_APP_SESSION_T  appHandle,
    TRDP_TIME_T         *pInterval,
    TRDP_FDS_T          *pFileDesc,
    INT32               *pNoDesc)
{
    TRDP_TIME_T now;
    PD_ELE_T    *iterPD;

    if (!trdp_isValidSession(appHandle))
    {
        return TRDP_NOINIT_ERR;
    }

    if (vos_mutexLock(appHandle->mutex) != VOS_NO_ERR)
    {
        return TRDP_NOINIT_ERR;
    }

    /*	Get the current time	*/
    vos_getTime(&now);

    vos_clearTime(&appHandle->interval);

    /*	Walk over the registered PDs, find pending packets */

    /*	Find the packet which has to be received next:	*/
    for (iterPD = appHandle->pRcvQueue; iterPD != NULL; iterPD = iterPD->pNext)
    {
        if (!timerisset(&appHandle->interval) ||
            timercmp(&iterPD->timeToGo, &appHandle->interval, <=))
        {
            appHandle->interval = iterPD->timeToGo;

            /*	There can be several sockets depending on TRDP_PD_CONFIG_T	*/

            if (pFileDesc != NULL &&
                iterPD->socketIdx != -1 &&
                appHandle->iface[iterPD->socketIdx].sock != -1 &&
                appHandle->option & TRDP_OPTION_BLOCK)
            {
                if (!FD_ISSET(appHandle->iface[iterPD->socketIdx].sock,
                              (fd_set *)pFileDesc))
                {
                    FD_SET(appHandle->iface[iterPD->socketIdx].sock,
                           (fd_set *)pFileDesc);
                }
            }
        }
    }

    /*	Find the packet which has to be sent even earlier:	*/
    for (iterPD = appHandle->pSndQueue; iterPD != NULL; iterPD = iterPD->pNext)
    {
        if (!timerisset(&appHandle->interval) ||
            timercmp(&iterPD->timeToGo, &appHandle->interval, <=))
        {
            appHandle->interval = iterPD->timeToGo;
        }
    }

#if MD_SUPPORT
    /*	TBD: Check message data timeouts here	*/
#endif

    /*	if lowest time is not zero   */
    if (timerisset(&appHandle->interval) &&
        timercmp(&now, &appHandle->interval, <=))
    {
        vos_subTime(&appHandle->interval, &now);
        *pInterval = appHandle->interval;
        /*
           vos_printf(VOS_LOG_INFO, "interval sec = %lu, usec = %u\n",
                           sSession.interval.tv_sec,
                                 sSession.interval.tv_usec);
         */
    }
    else    /*	Default minimum time is 10ms	*/
    {
        pInterval->tv_sec   = 0;
        pInterval->tv_usec  = 10000; /* Minimum poll time 10ms	*/
        /* vos_printf(VOS_LOG_INFO, "no interval\n"); */
    }
    vos_mutexUnlock(appHandle->mutex);

    return TRDP_NO_ERR;
}

/**********************************************************************************************************************/
/** Work loop of the TRDP handler.
 *	Search the queue for pending PDs to be sent
 *	Search the receive queue for pending PDs (time out)
 *
 *
 *  @param[in]      appHandle           The handle returned by tlc_init
 *  @param[in]		pRfds				pointer to set of ready descriptors
 *  @param[in,out]	pCount				pointer to number of ready descriptors
 *  @retval         TRDP_NO_ERR			no error
 *  @retval         TRDP_NOINIT_ERR		handle invalid
 */
EXT_DECL TRDP_ERR_T tlc_process (
    TRDP_APP_SESSION_T  appHandle,
    TRDP_FDS_T          *pRfds,
    INT32               *pCount)
{
    PD_ELE_T    *iterPD = NULL;
    TRDP_TIME_T now;
    TRDP_ERR_T  err = TRDP_NO_ERR;

    if (!trdp_isValidSession(appHandle))
    {
        return TRDP_NOINIT_ERR;
    }

    if (vos_mutexLock(appHandle->mutex) != VOS_NO_ERR)
    {
        return TRDP_NOINIT_ERR;
    }

    /*	Get the current time	*/
    vos_getTime(&now);

    vos_clearTime(&appHandle->interval);

    /*	Find the packet which has to be sent next:	*/
    for (iterPD = appHandle->pSndQueue; iterPD != NULL; iterPD = iterPD->pNext)
    {
        if (timercmp(&iterPD->timeToGo, &now, <=))
        {
            trdp_pdUpdate(iterPD);

            /*	Send the packet if it is not redundant	*/
            if (iterPD->socketIdx != -1 &&
                (!appHandle->beQuiet ||
                 (iterPD->pktFlags & TRDP_FLAGS_REDUNDANT)))
            {
                trdp_pdSend(appHandle->iface[iterPD->socketIdx].sock, iterPD);
            }

            /*	set new time	*/
            iterPD->timeToGo = iterPD->interval;
            vos_addTime(&iterPD->timeToGo, &now);
        }

        /*	Update the current time	*/
        vos_getTime(&now);
    }

    /*	Examine receive queue for late packets	*/
    for (iterPD = appHandle->pRcvQueue; iterPD != NULL; iterPD = iterPD->pNext)
    {
        if (timercmp(&iterPD->timeToGo, &now, <=) &&
            !(iterPD->privFlags & TRDP_TIMED_OUT))
        {
            /* Packet is late! We inform the user about this:	*/
            if (appHandle->pdDefault.pCbFunction != NULL)
            {
                TRDP_PD_INFO_T theMessage;
                theMessage.comId        = iterPD->addr.comId;
                theMessage.srcIpAddr    = iterPD->addr.srcIpAddr;
                theMessage.destIpAddr   = iterPD->addr.destIpAddr;
                theMessage.topoCount    = vos_ntohl(iterPD->frameHead.topoCount);
                theMessage.msgType      = vos_ntohs(iterPD->frameHead.msgType);
                theMessage.seqCount     = vos_ntohl(
                        iterPD->frameHead.sequenceCounter);
                theMessage.protVersion = vos_ntohs(
                        iterPD->frameHead.protocolVersion);
                theMessage.subs = vos_ntohs(
                        iterPD->frameHead.subsAndReserved);
                theMessage.offsetAddr   = vos_ntohs(
                        iterPD->frameHead.offsetAddress);
                theMessage.replyComId   = vos_ntohl(
                        iterPD->frameHead.replyComId);
                theMessage.replyIpAddr  = vos_ntohl(
                        iterPD->frameHead.replyIpAddress);
                theMessage.pUserRef     = iterPD->userRef;
                theMessage.resultCode   = TRDP_TIMEOUT_ERR;

                appHandle->pdDefault.pCbFunction(appHandle->pdDefault.pRefCon,
                                                 &theMessage, NULL, 0);
            }

            /*	Prevent repeated time out events	*/
            iterPD->privFlags |= TRDP_TIMED_OUT;
        }

        /*	Update the current time	*/
        vos_getTime(&now);
    }

    /*	Check the input params, in case we are in polling mode, the application
        is responsible to get any process data by calling tlp_get()	*/

    if (pRfds == NULL || pCount == NULL)
    {
        err = TRDP_NO_ERR;
    }
    else if (pCount != NULL && *pCount > 0)
    {
        /*	Check the sockets for received PD packets	*/

        for (iterPD = appHandle->pRcvQueue;
             iterPD != NULL;
             iterPD = iterPD->pNext)
        {
            if (iterPD->socketIdx != -1 &&
                FD_ISSET(appHandle->iface[iterPD->socketIdx].sock,
                         (fd_set *) pRfds))                                                 /*	PD frame received?	*/
            {
                /*	Compare the received data to the data in our receive queue
                    Call user's callback if data changed	*/
                err =
                    trdp_pdReceive(appHandle,
                                   appHandle->iface[iterPD->socketIdx].sock);
                if (err != TRDP_NO_ERR &&
                    err != TRDP_TIMEOUT_ERR)
                {
                    vos_printf(VOS_LOG_ERROR,
                               "Error receiving PD packet (Err: %d)\n",
                               err);
                }
                (*pCount)--;
                FD_CLR(appHandle->iface[iterPD->socketIdx].sock,
                       (fd_set *)pRfds);
            }
        }
    }

#if MD_SUPPORT

    /*	Check the socket for received MD packets	*/

    if (*pCount > 0 &&
        FD_ISSET(appHandle->mdRcvSock, (fd_set *) pRfds))            /*	MD frame received? */
    {

        /*
            receive and handle message data

           err = trdp_rcv_md(appHandle->mdRcvSock, mdComID);
           if (err != TRDP_NO_ERR &&
            err != TRDP_TIMEOUT_ERR)
           {
            vos_printf(VOS_LOG_ERROR, "Error receiving ComID %d (Err: %d)\n",
           *mdComID, err);
           }
         */

        (*pCount)--;
        FD_CLR(appHandle->mdRcvSock, (fd_set *) pRfds);
    }
#endif

    vos_mutexUnlock(appHandle->mutex);

    return err;
}

/**********************************************************************************************************************/
/** Prepare for receiving PD messages.
 *  Subscribe to a specific PD ComID and source IP
 *	To unsubscribe, set maxDataSize to zero!
 *
 *  @param[in]      appHandle           the handle returned by tlc_init
 *  @param[out]     pSubHandle			return a handle for these messages
 *  @param[in]      pUserRef			user supplied value returned within the info structure
 *  @param[in]      comId				comId of packet to receive
 *  @param[in]      topoCount			valid topocount, 0 for local consist
 *  @param[in]      srcIpAddr1			IP for source filtering, set 0 if not used
 *  @param[in]      srcIpAddr2			Second source IP address for source filtering, set to zero if not used.
 *                                      Used e.g. for source filtering of redundant devices.
 *  @param[in]      destIpAddr			IP address to join
 *  @param[in]      timeout		        timeout (>= 10ms) in usec
 *  @param[in]      toBehavior          timeout behavior
 *  @param[in]      maxDataSize			expected max. size of packet data
 *
 *  @retval         TRDP_NO_ERR	        no error
 *  @retval         TRDP_PARAM_ERR      parameter error
 *  @retval         TRDP_MEM_ERR		could not reserve memory (out of memory)
 *  @retval         TRDP_NOINIT_ERR		handle invalid
 */
EXT_DECL TRDP_ERR_T tlp_subscribe (
    TRDP_APP_SESSION_T  appHandle,
    TRDP_SUB_T          *pSubHandle,
    const void          *pUserRef,
    UINT32              comId,
    UINT32              topoCount,
    TRDP_IP_ADDR_T      srcIpAddr1,
    TRDP_IP_ADDR_T      srcIpAddr2,
    TRDP_IP_ADDR_T      destIpAddr,
    UINT32              timeout,
    TRDP_TO_BEHAVIOR_T  toBehavior,
    UINT32              maxDataSize)
{
    PD_ELE_T        *newPD = NULL;
    UINT32          grossDataSize;
    TRDP_TIME_T     now;
    TRDP_ERR_T      ret         = TRDP_NO_ERR;
    TRDP_ADDRESSES  subHandle   = {comId, srcIpAddr1, destIpAddr, 0};
    INT32           index;

    /*	Check params	*/
    if (comId == 0 || pSubHandle == NULL)
    {
        return TRDP_PARAM_ERR;
    }

    if (!trdp_isValidSession(appHandle))
    {
        return TRDP_NOINIT_ERR;
    }

    /*	Reserve mutual access	*/
    if (vos_mutexLock(appHandle->mutex) != VOS_NO_ERR)
    {
        return TRDP_NOINIT_ERR;
    }

    /*	Get the current time	*/
    vos_getTime(&now);

    /*	Check params	*/
    if (comId == 0 ||
        maxDataSize > MAX_PD_PACKET_SIZE ||
        timeout < TIMER_GRANULARITY)
    {
        return TRDP_PARAM_ERR;
    }

    /*	Look for existing element	*/

    if (trdp_queue_find_addr(appHandle->pRcvQueue, &subHandle) != NULL)
    {
        ret = TRDP_NOSUB_ERR;
    }
    else
    {
        /*	Find a (new) socket	*/

        ret = trdp_requestSocket(appHandle->iface,
                                 &appHandle->pdDefault.sendParam,
                                 destIpAddr,
                                 TRDP_SOCK_PD,
                                 appHandle->option,
                                 &index);
        if (ret == TRDP_NO_ERR)
        {
            /*	buffer size is PD_ELEMENT plus max. payload size plus padding & framecheck	*/

            grossDataSize = maxDataSize + sizeof(PD_HEADER_T) + sizeof(UINT32);

            /*	Allocate a buffer for this kind of packets	*/

            newPD = (PD_ELE_T *) vos_memAlloc(sizeof(PD_ELE_T));

            if (newPD == NULL)
            {
                ret = TRDP_MEM_ERR;
            }
            else
            {
                /*	Initialize some fields	*/

                if (IN_MULTICAST(destIpAddr))
                {
                    newPD->addr.mcGroup = destIpAddr;
                }

                newPD->addr.comId       = comId;
                newPD->addr.srcIpAddr   = srcIpAddr1;
                newPD->addr.destIpAddr  = destIpAddr;
                newPD->interval.tv_sec  = timeout / 1000000;
                newPD->interval.tv_usec = timeout % 1000000;
                newPD->timeToGo         = newPD->interval;
                newPD->grossSize        = grossDataSize;
                newPD->userRef      = pUserRef;
                newPD->socketIdx    = index;
                vos_getTime(&newPD->timeToGo);
                vos_addTime(&newPD->timeToGo, &newPD->interval);

                /*  append this subscribtion to our receive queue */
                trdp_queue_app_last(&appHandle->pRcvQueue, newPD);

                /*	Join a multicast group */
                if (vos_IsMulticast(newPD->addr.mcGroup) &&
                    !(newPD->privFlags & TRDP_MC_JOINT))
                {
                    vos_sockJoinMC(appHandle->iface[index].sock,
                                   newPD->addr.mcGroup,
                                   0);
                    /*	Remember we did this	*/
                    newPD->privFlags |= TRDP_MC_JOINT;
                }
                *pSubHandle = &newPD->addr;
                ret         = TRDP_NO_ERR;
            }
        }

    }

    vos_mutexUnlock(appHandle->mutex);

    return ret;
}

/**********************************************************************************************************************/
/** Stop receiving PD messages.
 *  Unsubscribe to a specific PD ComID
 *
 *  @param[in]      appHandle           the handle returned by tlc_init
 *  @param[in]      subHandle			the handle returned by subscription
 *  @retval         TRDP_NO_ERR	        no error
 *  @retval         TRDP_PARAM_ERR		parameter error
 *  @retval         TRDP_SUB_ERR		not subscribed
 *  @retval         TRDP_NOINIT_ERR		handle invalid
 */
EXT_DECL TRDP_ERR_T tlp_unsubscribe (
    TRDP_APP_SESSION_T  appHandle,
    TRDP_SUB_T          subHandle)
{
    PD_ELE_T    *pElement;
    TRDP_ERR_T  ret = TRDP_NOPUB_ERR;

    if (subHandle == NULL)
    {
        return TRDP_PARAM_ERR;
    }

    if (!trdp_isValidSession(appHandle))
    {
        return TRDP_NOINIT_ERR;
    }

    /*	Reserve mutual access	*/
    if (vos_mutexLock(appHandle->mutex) != VOS_NO_ERR)
    {
        return TRDP_NOINIT_ERR;
    }

    /*	Remove from queue?	*/
    pElement = trdp_queue_find_addr(appHandle->pSndQueue, subHandle);
    if (pElement != NULL)
    {
        trdp_queue_del_element(&appHandle->pSndQueue, pElement);
        vos_memFree(pElement);
        ret = TRDP_NO_ERR;
    }

    vos_mutexUnlock(appHandle->mutex);

    return ret;      /*	Not found	*/
}

/**********************************************************************************************************************/
/** Get the last valid PD message.
 *  This allows polling of PDs instead of event driven handling by callbacks
 *
 *  @param[in]      appHandle           the handle returned by tlc_init
 *  @param[in]      subHandle			the handle returned by subscription
 *  @param[in]      pktFlags		    OPTION: TRDP_FLAGS_MARSHALL
 *	@param[in,out]	pPdInfo             pointer to application's info buffer
 *  @param[in,out]	pData				pointer to application's data buffer
 *  @param[in,out]  pDataSize			in: size of buffer, out: size of data
 *
 *  @retval         TRDP_NO_ERR	        no error
 *  @retval         TRDP_PARAM_ERR      parameter error
 *  @retval         TRDP_SUB_ERR		not subscribed
 *  @retval         TRDP_TIMEOUT_ERR	packet timed out
 *  @retval         TRDP_NOINIT_ERR		handle invalid
 */
EXT_DECL TRDP_ERR_T tlp_get (
    TRDP_APP_SESSION_T  appHandle,
    TRDP_SUB_T          subHandle,
    TRDP_FLAGS_T        pktFlags,
    TRDP_PD_INFO_T      *pPdInfo,
    UINT8               *pData,
    UINT32              *pDataSize)
{
    PD_ELE_T    *pElement;
    TRDP_ERR_T  ret = TRDP_NOSUB_ERR;
    TRDP_TIME_T now;

    if (subHandle == NULL || pData == NULL || pDataSize == NULL)
    {
        return TRDP_PARAM_ERR;
    }

    if (!trdp_isValidSession(appHandle))
    {
        return TRDP_NOINIT_ERR;
    }

    /*	Reserve mutual access	*/
    if (vos_mutexLock(appHandle->mutex) != VOS_NO_ERR)
    {
        return TRDP_NOINIT_ERR;
    }

    /*	Find the published queue entry	*/
    pElement = trdp_queue_find_addr(appHandle->pRcvQueue, subHandle);

    if (pElement == NULL)
    {
        ret = TRDP_NOSUB_ERR;
    }
    else
    {
        /*	Check the supplied buffer size	*/
        if (pElement->dataSize > *pDataSize)
        {
            ret = TRDP_PARAM_ERR;
        }

        /*	Call the receive function if we are in non blocking mode	*/
        if (!(appHandle->option & TRDP_OPTION_BLOCK))
        {
            trdp_pdReceive(appHandle,
                           appHandle->iface[pElement->socketIdx].sock);
        }

        /*	Get the current time	*/
        vos_getTime(&now);
        /*	Check time out	*/
        if (timercmp(&pElement->timeToGo, &now, >))
        {
            /*	Packet is late	*/
            if (appHandle->pdDefault.toBehavior == TRDP_TO_SET_TO_ZERO)
            {
                memset(pData, 0, *pDataSize);
            }
            else /* TRDP_TO_KEEP_LAST_VALUE */
            {
                ;
            }
            ret = TRDP_TIMEOUT_ERR;
        }
        else if (pktFlags == TRDP_FLAGS_MARSHALL)
        {
            ret = trdp_pdGet(pElement,
                             appHandle->marshall.pCbUnmarshall,
                             appHandle->marshall.pRefCon,
                             pData,
                             *pDataSize);
        }
        else
        {
            ret = trdp_pdGet(pElement, NULL, NULL, pData, *pDataSize);
        }
    }

    if (pPdInfo != NULL)
    {
        pPdInfo->comId = pElement->addr.comId;
        pPdInfo->srcIpAddr      = pElement->addr.srcIpAddr;
        pPdInfo->destIpAddr     = pElement->addr.destIpAddr;
        pPdInfo->topoCount      = vos_ntohl(pElement->frameHead.topoCount);
        pPdInfo->msgType        = vos_ntohs(pElement->frameHead.msgType);
        pPdInfo->seqCount       = vos_ntohl(pElement->frameHead.sequenceCounter);
        pPdInfo->protVersion    = vos_ntohs(pElement->frameHead.protocolVersion);
        pPdInfo->subs           = vos_ntohs(pElement->frameHead.subsAndReserved);
        pPdInfo->offsetAddr     = vos_ntohs(pElement->frameHead.offsetAddress);
        pPdInfo->replyComId     = vos_ntohl(pElement->frameHead.replyComId);
        pPdInfo->replyIpAddr    = vos_ntohl(pElement->frameHead.replyIpAddress);
        pPdInfo->pUserRef       = pElement->userRef;        /* TBD: User reference given with the local subscribe?   */
        pPdInfo->resultCode     = TRDP_NO_ERR;
    }

    vos_mutexUnlock(appHandle->mutex);

    return ret;
}
