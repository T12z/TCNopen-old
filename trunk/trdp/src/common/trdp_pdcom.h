/******************************************************************************/
/**
 * @file            trdp_pdcom.h
 *
 * @brief           Functions for PD communication
 *
 * @details
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
 *      BL 2014-07-14: Ticket #46: Protocol change: operational topocount needed
 *                     Ticket #47: Protocol change: no FCS for data part of telegrams
 */


#ifndef TRDP_PDCOM_H
#define TRDP_PDCOM_H

/*******************************************************************************
 * INCLUDES
 */

#include "trdp_private.h"

/*******************************************************************************
 * DEFINES
 */


/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * GLOBAL FUNCTIONS
 */

void        trdp_pdInit(PD_ELE_T *, TRDP_MSG_T, UINT32 topoCount, UINT32 optopoCount, UINT32 replyComId, UINT32 replyIpAddress);
void        trdp_pdUpdate (
    PD_ELE_T *);

TRDP_ERR_T  trdp_pdPut (
    PD_ELE_T *,
    TRDP_MARSHALL_T func,
    void            *refCon,
    const UINT8     *pData,
    UINT32          dataSize);

void        trdp_pdDataUpdate (
    PD_ELE_T *pPacket);

TRDP_ERR_T  trdp_pdCheck (
    PD_HEADER_T *pPacket,
    UINT32      packetSize);

TRDP_ERR_T  trdp_pdSend (
    INT32       pdSock,
    PD_ELE_T    *pPacket,
    UINT16      port);

TRDP_ERR_T trdp_pdGet (
    PD_ELE_T            *pPacket,
    TRDP_UNMARSHALL_T   unmarshall,
    void                *refCon,
    const UINT8         *pData,
    UINT32              *pDataSize);

TRDP_ERR_T  trdp_pdSendQueued (
    TRDP_SESSION_PT appHandle);

TRDP_ERR_T  trdp_pdReceive (
    TRDP_SESSION_PT pSessionHandle,
    INT32           sock);

void        trdp_pdCheckPending (
    TRDP_APP_SESSION_T  appHandle,
    TRDP_FDS_T          *pFileDesc,
    INT32               *pNoDesc);

void        trdp_pdHandleTimeOuts (
    TRDP_SESSION_PT appHandle);

TRDP_ERR_T  trdp_pdCheckListenSocks (
    TRDP_SESSION_PT appHandle,
    TRDP_FDS_T      *pRfds,
    INT32           *pCount);

TRDP_ERR_T trdp_pdDistribute (
    PD_ELE_T *pSndQueue);

#endif
