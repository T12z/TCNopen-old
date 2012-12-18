/******************************************************************************/
/**
 * @file            trdp_mdcom.c
 *
 * @brief           Functions for MD communication
 *
 * @details
 *
 * @note            Project: TCNOpen TRDP prototype stack
 *
 * @author          Simone Pachera, FARsystems
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

#include "trdp_if_light.h"
#include "trdp_utils.h"
#include "trdp_mdcom.h"


/*******************************************************************************
 * DEFINES
 */


/*******************************************************************************
 * TYPEDEFS
 */


/******************************************************************************
 *   Locals
 */

/******************************************************************************/
/** Send MD packet
 *
 *  @param[in]      mdSock          socket descriptor
 *  @param[in]      pPacket         pointer to packet to be sent
 *  @retval         TRDP_NO_ERR         no error
 *  @retval         TRDP_UNKNOWN_ERR    error
 */
TRDP_ERR_T  trdp_sendMD (
    INT32            mdSock,
    const MD_ELE_T  *pPacket)
{
    TRDP_ERR_T err = TRDP_UNKNOWN_ERR;


    if (err != TRDP_NO_ERR)
    {
        vos_printf(VOS_LOG_ERROR, "trdp_sendMD failed\n");
        return TRDP_IO_ERR;
    }
    return TRDP_NO_ERR;
}

/******************************************************************************/
/** Receive MD packet
 *
 *  @param[in]      mdSock          socket descriptor
 *  @param[out]     ppPacket        pointer to pointer to received packet
 *  @param[out]     pSize           pointer to size of received packet
 *  @param[out]     pIPAddr         pointer to source IP address of packet
 *  @retval         TRDP_NO_ERR         no error
 *  @retval         TRDP_UNKNOWN_ERR    error
 */
TRDP_ERR_T  trdp_rcvMD (
    INT32         mdSock,
    MD_HEADER_T   **ppPacket,
    INT32         *pSize,
    UINT32        *pIPAddr)
{
    TRDP_ERR_T err = TRDP_UNKNOWN_ERR;


    return err;
}

/******************************************************************************/
/** Check for incoming md packet
 *
 *  @param[in]      appHandle       session pointer
 *  @param[in]      pPacket         pointer to the packet to check
 *  @param[in]      packetSize      size of the packet
 */
TRDP_ERR_T trdp_mdCheck (
    TRDP_SESSION_PT appHandle,
    MD_HEADER_T     *pPacket,
    UINT32          packetSize)
{
    TRDP_ERR_T  err = TRDP_NO_ERR;
    UINT32      l_datasetLength = vos_ntohl(pPacket->datasetLength);

    /* size shall be in MIN..MAX */
    if (TRDP_NO_ERR == err)
    {
        /* Min size is sizeof(MD_HEADER_T) because in case of no data no further data and data crc32 are added */
        if (packetSize < (sizeof(MD_HEADER_T)) ||
            packetSize > TRDP_MAX_MD_PACKET_SIZE)
        {
            appHandle->stats.udpMd.numProtErr++;
            vos_printf(VOS_LOG_ERROR, "MDframe size error (%u)\n",
                       (UINT32) packetSize);
            err = TRDP_WIRE_ERR;
        }
    }

    /*	crc check */
    if (TRDP_NO_ERR == err)
    {
        /* Check Header CRC */
        {
            UINT32 crc32 = vos_crc32(0xffffffff, (UINT8 *)pPacket, sizeof(MD_HEADER_T));

            if (crc32 != 0)
            {
                appHandle->stats.udpMd.numCrcErr++;
                vos_printf(VOS_LOG_ERROR, "MDframe header crc error.\n");
                err = TRDP_CRC_ERR;
           }
        }

        /* Check Data CRC */
        if(l_datasetLength > 0)
        {
            /* Check only if we have some data */
            UINT32  crc32       = vos_crc32(0xffffffff, (UINT8 *) pPacket + sizeof(MD_HEADER_T), l_datasetLength);
            UINT32  le_crc32    = MAKE_LE(crc32);

            UINT8   *pDataCRC   = (UINT8 *) pPacket + packetSize - 4;
            UINT32  pktCRC      = ((UINT32 *) pDataCRC)[0];

            if (le_crc32 != pktCRC)
            {
                appHandle->stats.udpMd.numCrcErr++;
                vos_printf(VOS_LOG_ERROR, "MDframe data crc error.\n");
                err = TRDP_CRC_ERR;
            }
        }
    }

    /*	Check protocol version	*/
    if (TRDP_NO_ERR == err)
    {
        UINT16 l_protocolVersion = vos_ntohs(pPacket->protocolVersion);
        #define TRDP_PROTOCOL_VERSION_CHECK_MASK  0xFF00
        if ((l_protocolVersion & TRDP_PROTOCOL_VERSION_CHECK_MASK) !=
            (TRDP_PROTO_VER & TRDP_PROTOCOL_VERSION_CHECK_MASK))
        {
            appHandle->stats.udpMd.numProtErr++;
            vos_printf(VOS_LOG_ERROR, "MDframe protocol error (%04x != %04x))\n",
                       l_protocolVersion,
                       TRDP_PROTO_VER);
            err = TRDP_WIRE_ERR;
        }
    }

    /*	Check protocol type	*/
    if (TRDP_NO_ERR == err)
    {
        UINT16 l_msgType = vos_ntohs(pPacket->msgType);
        switch(l_msgType)
        {
            /* valid message type ident */
            case TRDP_MSG_MN:
            case TRDP_MSG_MR:
            case TRDP_MSG_MP:
            case TRDP_MSG_MQ:
            case TRDP_MSG_MC:
            case TRDP_MSG_ME:
            {}
             break;
            /* invalid codes */
            default:
            {
                appHandle->stats.udpMd.numProtErr++;
                vos_printf(VOS_LOG_ERROR, "MDframe type error, received %04x\n",
                           l_msgType);
                err = TRDP_WIRE_ERR;
            }
            break;
        }
    }

    /* check telegram length */
    if (TRDP_NO_ERR == err)
    {
        UINT32  expectedLength  = 0;

        if(l_datasetLength > 0)
        {
            expectedLength = sizeof(MD_HEADER_T) + l_datasetLength + 4;
        }
        else
        {
            expectedLength = sizeof(MD_HEADER_T);
        }

        if (packetSize < expectedLength)
        {
            appHandle->stats.udpMd.numProtErr++;
            vos_printf(VOS_LOG_ERROR, "MDframe invalid length, received %d, expected %d\n",
                       packetSize,
                       expectedLength);
            err = TRDP_WIRE_ERR;
        }
    }

    return err;
}


/******************************************************************************/
/** Update the header values
 *
 *  @param[in]      pPacket         pointer to the packet to update
 */
void    trdp_mdUpdate (
    MD_ELE_T *pPacket)
{
    /* Initialize CRC calculation */
    UINT32  myCRC   = vos_crc32(0xffffffff, NULL, 0);
    UINT32  myCRC1  = myCRC;

    /* Get header and packet check sum values */
    UINT32  *hFCS   = &pPacket->frameHead.frameCheckSum;
    UINT32  *pFCS   = (UINT32 *)((UINT8 *)&pPacket->frameHead + pPacket->grossSize - 4);

    /* Calculate CRC for message head */
    myCRC = vos_crc32(myCRC,
                      (UINT8 *)&pPacket->frameHead,
                      sizeof(MD_HEADER_T) - 4);
    /* Convert to Little Endian */
    *hFCS = MAKE_LE(myCRC);

    /*
       Calculate CRC for message packet
     */
    if(pPacket->frameHead.datasetLength > 0)
    {
        myCRC1 = vos_crc32(myCRC1,
                           &pPacket->data[0],
                           vos_ntohl(pPacket->frameHead.datasetLength));
        *pFCS = MAKE_LE(myCRC1);
    }
}

/******************************************************************************/
/** Send MD packet
 *
 *  @param[in]      pdSock          socket descriptor
 *  @param[in]      pPacket         pointer to packet to be sent
 *  @retval         != NULL         error
 */
TRDP_ERR_T  trdp_mdSend (
    INT32           pdSock,
    const MD_ELE_T  *pPacket)
{
    VOS_ERR_T err = VOS_NO_ERR;

    if ((pPacket->pktFlags & TRDP_FLAGS_TCP) != 0)
    {
        err = vos_sockSendTCP(pdSock, (UINT8 *)&pPacket->frameHead, pPacket->grossSize);

    }
    else
    {
        err = vos_sockSendUDP(pdSock,
                              (UINT8 *)&pPacket->frameHead,
                              pPacket->grossSize,
                              pPacket->addr.destIpAddr,
                              TRDP_MD_UDP_PORT);
    }

    if (err != VOS_NO_ERR)
    {
        vos_printf(VOS_LOG_ERROR, "vos_sockSendUDP failed (Err: %d)\n", err);
        return TRDP_IO_ERR;
    }

    return TRDP_NO_ERR;
}

/******************************************************************************/
/** Receive MD packet
 *
 *  @param[in]      appHandle       session pointer
 *  @param[in]      mdSock          socket descriptor
 *  @param[in]      pPacket         pointer to received packet
 *  @retval         != NULL         error
 */
TRDP_ERR_T  trdp_mdRecv (
    TRDP_SESSION_PT appHandle,
    INT32           mdSock,
    MD_ELE_T        *pPacket)
{
    TRDP_ERR_T   err = TRDP_NO_ERR;

    UINT32 size = pPacket->grossSize;

    if ((pPacket->pktFlags & TRDP_FLAGS_TCP) != 0)
    {
        /* Read Header */
        err = (TRDP_ERR_T) vos_sockReceiveTCP(mdSock, (UINT8 *)&pPacket->frameHead, &size);
        vos_printf(VOS_LOG_INFO, "Read Header Size = %d\n", size);

        if(err == VOS_NODATA_ERR)
        {
            vos_printf(VOS_LOG_INFO, "trdp_mdRecv - The socket = %u has been closed \n",mdSock);
            return TRDP_NODATA_ERR;
        }

        if(err != VOS_NO_ERR)
        {
            vos_printf(VOS_LOG_ERROR, "trdp_mdRecv failed (Reading the msg Header) = %d\n",err);
            return TRDP_IO_ERR;
        }

        /* Get the rest of the message length */
        {
            UINT32 data_size;
            data_size = vos_ntohl(pPacket->frameHead.datasetLength) + sizeof(pPacket->frameHead.frameCheckSum);

            /*Read Data + CRC */
            err = (TRDP_ERR_T) vos_sockReceiveTCP(mdSock, (UINT8 *)&pPacket->data[0], &data_size);
            vos_printf(VOS_LOG_INFO, "Read Data + CRC Size = %d\n", data_size);

            size = size + data_size;
            //pPacket->grossSize = Size;
        }
    }
    else
    {
        err = (TRDP_ERR_T) vos_sockReceiveUDP(
                mdSock,
                (UINT8 *)&pPacket->frameHead,
                &size,
                &pPacket->addr.srcIpAddr);
    }

    pPacket->dataSize = size;

    if (err == TRDP_NODATA_ERR)
    {
        /* no data -> rx timeout */
        return TRDP_TIMEOUT_ERR;
    }

    if (err != TRDP_NO_ERR)
    {
        vos_printf(VOS_LOG_ERROR, "trdp_mdRecv failed = %d\n", err);
        return TRDP_IO_ERR;
    }

    /* received data */
    err = trdp_mdCheck(appHandle, &pPacket->frameHead, size);
	
    /*  Update statistics   */
    switch (err)
    {
        case TRDP_NO_ERR:
            appHandle->stats.udpMd.numRcv++;;
            break;
        case TRDP_CRC_ERR:
            appHandle->stats.udpMd.numCrcErr++;
            return err;
        case TRDP_WIRE_ERR:
            appHandle->stats.udpMd.numProtErr++;
            return err;
        default:
            return err;
    }
	
    if (err != TRDP_NO_ERR)
    {
        vos_printf(VOS_LOG_ERROR, "trdp_mdRecv failed = %d\n", err);
        return TRDP_IO_ERR;
    }

    return err;
}

/******************************************************************************/
/** Receiving MD messages
 *  Read the receive socket for arriving MDs, copy the packet to a new MD_ELE_T
 *  Check for protocol errors and dispatch to proper receive queue.
 *  Call user's callback if needed
 *
 *  @param[in]      appHandle           session pointer
 *  @param[in]      sock                the socket to read from
 *  @retval         TRDP_NO_ERR         no error
 *  @retval         TRDP_PARAM_ERR      parameter error
 *  @retval         TRDP_WIRE_ERR       protocol error (late packet, version mismatch)
 *  @retval         TRDP_QUEUE_ERR      not in queue
 *  @retval         TRDP_CRC_ERR        header checksum
 *  @retval         TRDP_TOPOCOUNT_ERR  invalid topocount
 */
TRDP_ERR_T  trdp_mdReceive (
    TRDP_SESSION_PT appHandle,
    INT32           sock)
{
    TRDP_ERR_T  err = TRDP_NO_ERR;
    UINT8       findSock;
    UINT8       sockPosition;

    /* get buffer at 1st call */
    if (appHandle->pMDRcvEle == NULL)
    {
        appHandle->pMDRcvEle = (MD_ELE_T *) vos_memAlloc(sizeof(MD_ELE_T) + TRDP_MAX_MD_PACKET_SIZE - sizeof(MD_HEADER_T));
        if (NULL == appHandle->pMDRcvEle)
        {
            vos_printf(VOS_LOG_ERROR, "Receiving MD: Out of receive buffers!\n");
            return TRDP_MEM_ERR;
        }

        memset(appHandle->pMDRcvEle, 0, sizeof(MD_ELE_T));

        if ((appHandle->mdDefault.flags & TRDP_FLAGS_TCP) != 0)
        {
            /* Received telegram size = telegram header + data(fixed to 32) + CRC
            * TODO The data size should be got from the header->datasetLength (to be dinamic),
            * and then call to read() function until all the data is read. */
            appHandle->pMDRcvEle->grossSize = sizeof(appHandle->pMDRcvEle->frameHead);
        }
        else
        {
            appHandle->pMDRcvEle->grossSize = TRDP_MAX_MD_PACKET_SIZE;
        }
     
        appHandle->pMDRcvEle->pktFlags  = appHandle->mdDefault.flags;
    }

    /* get packet */
    err = trdp_mdRecv(appHandle, sock, appHandle->pMDRcvEle);
    if (TRDP_NO_ERR != err)
    {
        return err;
    }

    /* process message */
    {
        MD_HEADER_T *pH = &appHandle->pMDRcvEle->frameHead;
        int         lF  = appHandle->pMDRcvEle->dataSize;

        vos_printf(VOS_LOG_ERROR,
                   "Received MD packet (space: %d len: %d)\n",
                   appHandle->pMDRcvEle->grossSize,
                   appHandle->pMDRcvEle->dataSize);

        /* Display incoming header */
        /* TCP cornerIp */
        if ((appHandle->mdDefault.flags & TRDP_FLAGS_TCP) != 0)
        {
            int i;

            for(findSock=0; findSock < VOS_MAX_SOCKET_CNT; findSock++)
            {
                if((appHandle->iface[findSock].sock > -1) && (appHandle->iface[findSock].sock == sock))
                {
                    break;
                }
            }

            appHandle->pMDRcvEle->addr.srcIpAddr = appHandle->iface[findSock].tcpParams.cornerIp;

            printf("***** from **** : %08X\n", appHandle->pMDRcvEle->addr.srcIpAddr);
            printf("sequenceCounter = %d\n", vos_ntohl(pH->sequenceCounter));
            printf("protocolVersion = %d\n", vos_ntohs(pH->protocolVersion));
            printf("msgType         = x%04X\n", vos_ntohs(pH->msgType        ));
            printf("comId           = %d\n", vos_ntohl(pH->comId          ));
            printf("topoCount       = %d\n", vos_ntohl(pH->topoCount      ));
            printf("datasetLength   = %d\n", vos_ntohl(pH->datasetLength  ));
            printf("replyStatus     = %d\n", vos_ntohl(pH->replyStatus    ));
            printf("sessionID       = ");
            for(i = 0; i < 16; i++)
            {
                printf("%02X ", pH->sessionID[i]);
            }
            printf("\n");
            printf("replyTimeout    = %d\n", vos_ntohl(pH->replyTimeout   ));
            printf("sourceURI       = ");
            for(i = 0; i < 32; i++)
            {
                if (pH->sourceURI[i])
                {
                    printf("%c", pH->sourceURI[i]);
                }
            }
            printf("\n");
            printf("destinationURI  = ");
            for(i = 0; i < 32; i++)
            {
                if (pH->destinationURI[i])
                {
                    printf("%c", pH->destinationURI[i]);
                }
            }
            printf("\n");
        }

        {
            /* checks */
            UINT32  l_comId = vos_ntohl(pH->comId);

            /* topo counter check for context */
            UINT32  l_topoCount = vos_ntohl(pH->topoCount);

           /* message type */
            UINT16      l_msgType = vos_ntohs(pH->msgType);

            /* find for subscriber */
            MD_ELE_T    *iterMD;
                        
            /* check for topo counter */
            if (l_topoCount != 0 && appHandle->topoCount != 0 && l_topoCount != appHandle->topoCount)
            {
                vos_printf(VOS_LOG_INFO,
                           "MD data with wrong topocount ignored (comId %u, topo %u)\n",
                           l_comId,
                           l_topoCount);

                appHandle->stats.udpMd.numTopoErr++;

                return TRDP_TOPO_ERR;
            }
       
            /* seach for existing listener */
            for(iterMD = appHandle->pMDRcvQueue; iterMD != NULL; iterMD = iterMD->pNext)
            {
                /* check for correct communication ID ... */
                if (l_comId != iterMD->u.listener.comId)
                {
                    continue;
                }

                /* 1st receive */
                if (iterMD->stateEle == TRDP_MD_ELE_ST_RX_ARM)
                {
                    /* message type depending */
                    switch(l_msgType)
                    {
                        /* 1st level: receive notify or request telegram */

                        case TRDP_MSG_MN: /* notify */
                        case TRDP_MSG_MR: /* request */
                        {
                            /* next step if request */
                            if (TRDP_MSG_MR == l_msgType)
                            {
                                /* in case for request, the listener is busy waiting for application reply or error */
                                iterMD->stateEle = TRDP_MD_ELE_ST_RX_REQ_W4AP_REPLY;
                            }

                            /* receive time */
                            vos_getTime(&iterMD->timeToGo);

                            /* timeout value */
                            iterMD->interval.tv_sec     = vos_ntohl(pH->replyTimeout) / 1000000;
                            iterMD->interval.tv_usec    = vos_ntohl(pH->replyTimeout) % 1000000;

                            /* save session Id for next steps */
                            memcpy(iterMD->sessionID, pH->sessionID, 16);

                            /* copy message to proper listener */
                            memcpy(&iterMD->frameHead, pH, lF);

                            if (appHandle->mdDefault.pfCbFunction != NULL)
                            {
                                TRDP_MD_INFO_T theMessage;

                                theMessage.srcIpAddr    = appHandle->pMDRcvEle->addr.srcIpAddr;
                                theMessage.destIpAddr   = 0;
                                theMessage.seqCount     = vos_ntohl(pH->sequenceCounter);
                                theMessage.protVersion  = vos_ntohs(pH->protocolVersion);
                                theMessage.msgType      = vos_ntohs(pH->msgType);
                                theMessage.comId        = iterMD->u.listener.comId;
                                theMessage.topoCount    = vos_ntohl(pH->topoCount);
                                theMessage.userStatus   = 0;
                                theMessage.replyStatus  = vos_ntohs(pH->replyStatus);
                                memcpy(theMessage.sessionId, pH->sessionID, 16);
                                theMessage.replyTimeout = vos_ntohl(pH->replyTimeout);
                                memcpy(theMessage.destURI, pH->destinationURI, TRDP_MAX_URI_USER_LEN);
                                memcpy(theMessage.srcURI, pH->sourceURI, TRDP_MAX_URI_USER_LEN);
							    theMessage.noOfRepliers = 0;
							    theMessage.numReplies   = 0;
                                theMessage.numRetriesMax = 0;
							    theMessage.numRetries   = 0;
							    theMessage.disableReplyRx = 0;
							    theMessage.numRepliesQuery = 0;
							    theMessage.numConfirmSent = 0;
							    theMessage.numConfirmTimeout = 0;
                                theMessage.pUserRef     = appHandle->mdDefault.pRefCon;
                                theMessage.resultCode   = TRDP_NO_ERR;

                                if ((appHandle->mdDefault.flags & TRDP_FLAGS_TCP) != 0)
                                {
                                    appHandle->mdDefault.pfCbFunction(
                                        appHandle->mdDefault.pRefCon,
                                        &theMessage,
                                        (UINT8 *)&(appHandle->pMDRcvEle->data[0]),lF);
                                }
                                else
							    {
                                    appHandle->mdDefault.pfCbFunction(
                                        appHandle->mdDefault.pRefCon,
                                        &theMessage,
                                        (UINT8 *)pH, lF);
                                }
                            }

                            /* TCP and Notify message */
                            if ((appHandle->mdDefault.flags & TRDP_FLAGS_TCP) != 0)
                            {
                                if(TRDP_MSG_MR == l_msgType)
                                {
                                    for(findSock = 0; findSock < VOS_MAX_SOCKET_CNT; findSock++)
                                    {
                                        if((appHandle->iface[findSock].sock == sock)
                                            && (appHandle->iface[findSock].sock != -1)
                                            && (appHandle->iface[findSock].rcvOnly == 1))
                                        {
                                            sockPosition = findSock;
                                            break;
                                        }
                                    }

                                    appHandle->iface[sockPosition].usage++;
                                    vos_printf(VOS_LOG_INFO, "Socket (Num = %d) usage incremented to (Num = %d)\n",
                                        appHandle->iface[sockPosition].sock, appHandle->iface[sockPosition].usage);

                                    /* Save the socket position in the listener */
                                    iterMD->socketIdx = sockPosition;
                                    vos_printf(VOS_LOG_INFO, "SocketIndex (Num = %d) saved in the Listener\n",
                                        iterMD->socketIdx);
                                }
                            }
                        }
                        break;

                        /* 2nd step: receive of reply message with or without confirmation request, error too handled */
                        case TRDP_MSG_MP: /* reply no conf */
                        case TRDP_MSG_MQ: /* reply with conf */
                        case TRDP_MSG_MC: /* confirm */
                        case TRDP_MSG_ME: /* reply error */
                        {
                            /* check on opened caller session to pair received replay/error telegram */
                            MD_ELE_T *sender_ele;
                            for (sender_ele = appHandle->pMDSndQueue; sender_ele != NULL; sender_ele = sender_ele->pNext)
                            {
                                /* check for comID .... */
                                if (sender_ele->u.caller.comId != l_comId)
                                {
                                    continue;
                                }

                                /* check for session ... */
                                if (0 != memcmp(&sender_ele->sessionID, pH->sessionID, 16))
                                {
                                    continue;
                                }

							    /* Found */
                                break;
                            }

                            /* Found */
                            if(sender_ele != NULL)
                            {
                                /**--**/
                                /* the sender sent request and is waiting for reply */
                                if (sender_ele->stateEle == TRDP_MD_ELE_ST_TX_REQUEST_W4Y)
                                {
                                    /* reply ok or reply error */
                                    if (l_msgType == TRDP_MSG_MP || l_msgType == TRDP_MSG_MQ || l_msgType == TRDP_MSG_ME)
                                    {
                                        UINT32 removeMdSendElement = 0;

									    /* Discard all MD Reply/ReplyQuery received after ReplyTimeout or expected replies received */
                                        if(sender_ele->disableReplyRx != 0)
                                        {
                                            break;
                                        }

                                        /* Info */
                                        vos_printf(VOS_LOG_INFO,
                                                    "MD RX/TX match (comId %u, topo %u)\n",
                                                    l_comId,
                                                    l_topoCount);

                                        /* Increment number of received replies */
                                        sender_ele->numReplies++;

                                        /* If received reply with confirm request */
                                        if (l_msgType == TRDP_MSG_MQ)
                                        {
                                            /* Increment number of ReplyQuery received, used to count nuomber of expected Confirm sent */
                                            sender_ele->numRepliesQuery++;
										
                                            /* ... move the listener waiting for sending confirm telegram */
                                            iterMD->stateEle = TRDP_MD_ELE_ST_RX_REPLY_W4AP_CONF;

                                            /* copy session id to listener */
                                            memcpy(&iterMD->sessionID, &sender_ele->sessionID, 16);

                                            /* forward timeout */
                                            iterMD->timeToGo    = sender_ele->timeToGo;
                                            iterMD->interval    = sender_ele->interval;

                                            /* Copy other identification arams */
                                            iterMD->u.listener.comId        = l_comId;
                                            iterMD->u.listener.topoCount    = l_topoCount;
                                            iterMD->u.listener.destIpAddr   = appHandle->pMDRcvEle->addr.srcIpAddr;
                                            memcpy(iterMD->u.listener.destURI, pH->destinationURI, TRDP_MAX_URI_USER_LEN);
                                        }

                                        /* Handle multiple replies */
                                        if(sender_ele->noOfRepliers == 1)
                                        {
                                            /* Disable Reply/ReplyQuery reception, only one expected */
                                            sender_ele->disableReplyRx = 1;
                                        }
                                        else if(sender_ele->noOfRepliers > 1)
                                        {
                                            /* Handle multiple known expected replies */
                                            if(sender_ele->noOfRepliers == sender_ele->numReplies)
                                            {
                                                /* Disable Reply/ReplyQuery reception because all expected are received */
                                                sender_ele->disableReplyRx = 1;
                                            }
                                        }
                                        else
                                        {
                                            /* Handle multiple unknown replies */
                                            /* Send element removed by timeout because number of repliers is unknown */
                                        }

                                        /* copy message to proper listener */
                                        memcpy(&iterMD->frameHead, pH, lF);

                                        if (appHandle->mdDefault.pfCbFunction != NULL)
                                        {
                                            TRDP_MD_INFO_T theMessage;

                                            theMessage.srcIpAddr    = appHandle->pMDRcvEle->addr.srcIpAddr;
                                            theMessage.destIpAddr   = 0;
                                            theMessage.seqCount     = vos_ntohl(pH->sequenceCounter);
                                            theMessage.protVersion  = vos_ntohs(pH->protocolVersion);
                                            theMessage.msgType      = vos_ntohs(pH->msgType);
                                            theMessage.comId        = iterMD->u.listener.comId;
                                            theMessage.topoCount    = vos_ntohl(pH->topoCount);
                                            theMessage.userStatus   = 0;
                                            theMessage.replyStatus  = vos_ntohs(pH->replyStatus);
                                            memcpy(theMessage.sessionId, pH->sessionID, 16);
                                            theMessage.replyTimeout = vos_ntohl(pH->replyTimeout);
                                            memcpy(theMessage.destURI, pH->destinationURI, TRDP_MAX_URI_USER_LEN);
                                            memcpy(theMessage.srcURI, pH->sourceURI, TRDP_MAX_URI_USER_LEN);
										    theMessage.noOfRepliers = sender_ele->noOfRepliers;
                                            theMessage.numReplies   = sender_ele->numReplies;
										    theMessage.numRetriesMax = sender_ele->numRetriesMax;
										    theMessage.numRetries   = sender_ele->numRetries;
										    theMessage.disableReplyRx = sender_ele->disableReplyRx;
										    theMessage.numRepliesQuery = sender_ele->numRepliesQuery;
										    theMessage.numConfirmSent = sender_ele->numConfirmSent;
										    theMessage.numConfirmTimeout = sender_ele->numConfirmTimeout;
                                            theMessage.pUserRef     = appHandle->mdDefault.pRefCon;
                                            theMessage.resultCode   = TRDP_NO_ERR;

                                            if ((appHandle->mdDefault.flags & TRDP_FLAGS_TCP) != 0)
                                            {
                                                appHandle->mdDefault.pfCbFunction(
                                                    appHandle->mdDefault.pRefCon,
                                                    &theMessage,
                                                    (UINT8 *)&(appHandle->pMDRcvEle->data[0]),lF);
										    }
                                            else
                                            {
                                                appHandle->mdDefault.pfCbFunction(
                                                    appHandle->mdDefault.pRefCon,
                                                    &theMessage,
                                                    (UINT8 *)pH, lF);
                                            }
                                        }

                                        /* TCP and Reply/ReplyError message */
                                        if ((appHandle->mdDefault.flags & TRDP_FLAGS_TCP) != 0)
                                        {
                                            for(findSock = 0; findSock < VOS_MAX_SOCKET_CNT; findSock++)
                                            {
                                                if((appHandle->iface[findSock].sock == sock)
                                                    && (appHandle->iface[findSock].sock != -1)
                                                    && (appHandle->iface[findSock].rcvOnly == FALSE))
                                                {
                                                    sockPosition = findSock;
                                                    break;
                                                }
                                            }

                                            iterMD->socketIdx = sockPosition;

                                            if((TRDP_MSG_MP == l_msgType) || (TRDP_MSG_ME == l_msgType))
                                            {
                                                appHandle->iface[sockPosition].usage--;
                                                vos_printf(VOS_LOG_INFO, "Socket (Num = %d) usage decremented to (Num = %d)\n",
                                                    appHandle->iface[sockPosition].sock, appHandle->iface[sockPosition].usage);

                                                /* If there is no at least one session using the socket, start the socket connectionTimeout */
                                                if((appHandle->iface[sockPosition].usage == 0) && (appHandle->iface[sockPosition].rcvOnly == FALSE))
                                                {
                                                    TRDP_TIME_T tmpt_interval, tmpt_now;

                                                    vos_printf(VOS_LOG_INFO, "The Socket (Num = %d usage=0) ConnectionTimeout will be started\n",
                                                        appHandle->iface[sockPosition].sock);
                                                    
                                                    /* Start the socket connectionTimeout */
                                                    tmpt_interval.tv_sec = appHandle->mdDefault.connectTimeout / 1000000;
                                                    tmpt_interval.tv_usec = appHandle->mdDefault.connectTimeout % 1000000;

                                                    vos_getTime(&tmpt_now);
                                                    vos_addTime(&tmpt_now, &tmpt_interval);

                                                    memcpy(&appHandle->iface[sockPosition].tcpParams.connectionTimeout, &tmpt_now, sizeof(TRDP_TIME_T));
                                                }
                                            }

                                            if (TRDP_MSG_MQ == l_msgType)
                                            {
                                                /* Save the socket position in the listener */
                                                iterMD->socketIdx = sockPosition;
                                                vos_printf(VOS_LOG_INFO, "SocketIndex (Num = %d) saved in the Listener\n",
                                                    iterMD->socketIdx);
                                            }
                                        }

                                        /* stop loop */
                                        break;
                                    }
                                }
                                /**--**/

                                /* the sender sent reply and is waiting for confirm */
                                if (sender_ele->stateEle == TRDP_MD_ELE_ST_TX_REPLYQUERY_W4C)
                                {
                                    /* confirm or error */
                                    if (l_msgType == TRDP_MSG_MC)
                                    {
                                        /* copy message to proper listener */
                                        memcpy(&iterMD->frameHead, pH, lF);

                                        if (appHandle->mdDefault.pfCbFunction != NULL)
                                        {
                                            TRDP_MD_INFO_T theMessage;

                                            theMessage.srcIpAddr    = appHandle->pMDRcvEle->addr.srcIpAddr;
                                            theMessage.destIpAddr   = 0;
                                            theMessage.seqCount     = vos_ntohl(pH->sequenceCounter);
                                            theMessage.protVersion  = vos_ntohs(pH->protocolVersion);
                                            theMessage.msgType      = vos_ntohs(pH->msgType);
                                            theMessage.comId        = iterMD->u.listener.comId;
                                            theMessage.topoCount    = vos_ntohl(pH->topoCount);
                                            theMessage.userStatus   = 0;
                                            theMessage.replyStatus  = vos_ntohs(pH->replyStatus);
                                            memcpy(theMessage.sessionId, pH->sessionID, 16);
                                            theMessage.replyTimeout = vos_ntohl(pH->replyTimeout);
                                            memcpy(theMessage.destURI, pH->destinationURI, TRDP_MAX_URI_USER_LEN);
                                            memcpy(theMessage.srcURI, pH->sourceURI, TRDP_MAX_URI_USER_LEN);
                                            theMessage.noOfRepliers = 0;
                                            theMessage.numReplies   = 0;
                                            theMessage.numRetriesMax = 0;
                                            theMessage.numRetries   = 0;
                                            theMessage.disableReplyRx = 0;
                                            theMessage.numRepliesQuery = 0;
                                            theMessage.numConfirmSent = 0;
                                            theMessage.numConfirmTimeout = 0;
                                            theMessage.pUserRef     = appHandle->mdDefault.pRefCon;
                                            theMessage.resultCode   = TRDP_NO_ERR;

                                            if ((appHandle->mdDefault.flags & TRDP_FLAGS_TCP) != 0)
                                            {
                                                appHandle->mdDefault.pfCbFunction(
                                                    appHandle->mdDefault.pRefCon,
                                                    &theMessage,
                                                    (UINT8 *)&(appHandle->pMDRcvEle->data[0]),lF);
                                            }
                                            else
                                            {
                                                appHandle->mdDefault.pfCbFunction(
                                                    appHandle->mdDefault.pRefCon,
                                                    &theMessage,
                                                    (UINT8 *)pH, lF);
                                            }
                                        }

                                        /* TCP and Confirm message */
                                        if ((appHandle->mdDefault.flags & TRDP_FLAGS_TCP) != 0)
                                        {
                                            for(findSock = 0; findSock < VOS_MAX_SOCKET_CNT; findSock++)
                                            {
                                                if((appHandle->iface[findSock].sock == sock)
                                                    && (appHandle->iface[findSock].sock != -1)
                                                    && (appHandle->iface[findSock].rcvOnly == 1))
                                                {
                                                    sockPosition = findSock;
                                                    break;
                                                }
                                            }

                                            iterMD->socketIdx = sockPosition;

                                            appHandle->iface[sockPosition].usage--;
                                            vos_printf(VOS_LOG_INFO, "Socket (Num = %d) usage decremented to (Num = %d)\n",
                                                appHandle->iface[sockPosition].sock, appHandle->iface[sockPosition].usage);
                                        }

                                        /* Remove element from queue */
                                        trdp_MDqueueDelElement(&appHandle->pMDSndQueue, sender_ele);

                                        /* free element */
                                        vos_memFree(sender_ele);

                                        /* stop loop */
                                        break;
                                    }
                                }
                            }
                        }
                        break;
                    }
                    break;
                }
            }
		   
            /* Statistics */
            if(iterMD == NULL)
            {
                appHandle->stats.udpMd.numNoListener++;
            }
        }
    }

    return TRDP_NO_ERR;
}

