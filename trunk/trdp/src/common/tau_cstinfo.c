/**********************************************************************************************************************/
/**
 * @file            tau_cstinfo.c
 *
 * @brief           Functions for consist information access
 *
 * @details
 *
 * @note            Project: TCNOpen TRDP prototype stack
 *
 * @author          B. Loehr (initial version)
 *
 * @remarks This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 *          If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *          Copyright Bombardier Transportation Inc. or its subsidiaries and others, 2015. All rights reserved.
 *
 * $Id$
 *
 */

/***********************************************************************************************************************
 * INCLUDES
 */

#include <string.h>
#include <stdio.h>

#include "trdp_if_light.h"
#include "tau_tti.h"
#include "iec61375-2-3.h"
#include "vos_sock.h"

/***********************************************************************************************************************
 * DEFINES
 */


/***********************************************************************************************************************
 * TYPEDEFS
 */

/***********************************************************************************************************************
 * Locals
 */

/**********************************************************************************************************************/
/**    Getter function to retrieve a value from the consist info telegram value.
 *
 *  @param[in]      pCstInfo        pointer to packed consist info in network byte order
 *  @param[out]     pValue          pointer to value to return
 *
 *  @retval         none
 *
 */
UINT16 cstInfoGetPropSize (TRDP_CONSIST_INFO_T *pCstInfo)
{
    return vos_ntohs(pCstInfo->cstProp.len);
}

void cstInfoGetProperty (TRDP_CONSIST_INFO_T    *pCstInfo,
                         UINT8                  *pValue)
{
    memcpy(pValue, pCstInfo->cstProp.prop, vos_ntohs(pCstInfo->cstProp.len));
}

void cstInfoGetETBInfo (TRDP_CONSIST_INFO_T *pCstInfo,
                        UINT32              index,
                        TRDP_ETB_INFO_T     *pValue)
{
    UINT8   *pSrc   = (UINT8 *)&pCstInfo->cstProp + vos_ntohs(pCstInfo->cstProp.len) + 4 + 2;   /* pCstInfo->etbCnt   */
    UINT16  etbCnt  = vos_ntohs(*pSrc);
    if (index > etbCnt)
    {
        memset(pValue, 0, sizeof(TRDP_ETB_INFO_T));
    }
    TRDP_ETB_INFO_T *pCurInfo = (TRDP_ETB_INFO_T *) (pSrc + 2);
    *pValue = pCurInfo[index];
}

UINT32  cstInfoGetVehInfoSize (UINT8 *pVehList)
{
    /* Note: size is computed using
                fixed size of vehicle info struct
                plus size of properties
                minus 4 bytes of static array element [0] */
    return sizeof(TRDP_VEHICLE_INFO_T) +
           (vos_ntohs(((TRDP_VEHICLE_INFO_T *)pVehList)->vehProp.len) * sizeof(TRDP_PROP_T) - 4);
}

void cstInfoGetVehInfo (TRDP_CONSIST_INFO_T *pCstInfo,
                        UINT32              index,
                        TRDP_VEHICLE_INFO_T *pValue,
                        UINT32              *pSize)
{
    UINT8   *pSrc   = (UINT8 *)&pCstInfo->cstProp + vos_ntohs(pCstInfo->cstProp.len) + 4 + 2;   /* pCstInfo->etbCnt   */
    UINT16  etbCnt  = vos_ntohs(*pSrc);
    UINT16  vehCnt;
    UINT32  i;
    pSrc    += 2 + etbCnt * sizeof(TRDP_ETB_INFO_T) + 2;        /* pSrc points to first vehicle count    */
    vehCnt  = vos_ntohs(*pSrc);
    if (index > vehCnt)
    {
        memset(pValue, 0, sizeof(TRDP_VEHICLE_INFO_T));
    }
    UINT8 *pCurInfo = (UINT8 *) (pSrc + 2);
    *pSize = cstInfoGetVehInfoSize(pCurInfo);                   /* size of first vehicle info   */

    for (i = 0; i < index; i++)
    {
        pCurInfo    += *pSize;                                  /* advance to next info */
        *pSize      = cstInfoGetVehInfoSize(pCurInfo);          /* compute its size     */
    }

    memcpy(pValue, pCurInfo, *pSize);
}

void cstInfoGetFctInfo (TRDP_CONSIST_INFO_T     *pCstInfo,
                        UINT32                  index,
                        TRDP_FUNCTION_INFO_T    *pValue,
                        UINT32                  *pSize)
{
    UINT8   *pSrc   = (UINT8 *)&pCstInfo->cstProp + vos_ntohs(pCstInfo->cstProp.len) + 4 + 2;   /* pCstInfo->etbCnt   */
    UINT16  etbCnt  = vos_ntohs(*pSrc);
    UINT16  vehCnt;
    UINT16  fctCnt;
    UINT32  itemSize;
    UINT32  i;
    pSrc    += 2 + etbCnt * sizeof(TRDP_ETB_INFO_T) + 2;        /* pSrc points to first vehicle count    */
    vehCnt  = vos_ntohs(*(UINT16 *)pSrc);
    UINT8   *pCurInfo = (UINT8 *) (pSrc + 2);
    itemSize = cstInfoGetVehInfoSize(pCurInfo);                 /* size of first vehicle info   */

    for (i = 0; i < vehCnt; i++)
    {
        pCurInfo    += itemSize;                                /* advance to next info */
        itemSize    = cstInfoGetVehInfoSize(pCurInfo);          /* compute its size     */
    }

    pSrc    = pCurInfo + itemSize + 2;                          /* pSrc points to fctCnt    */
    fctCnt  = vos_ntohs(*(UINT16 *)pSrc);

    if (index > fctCnt)
    {
        memset(pValue, 0, sizeof(TRDP_FUNCTION_INFO_T));
        *pSize = 0;
        return;
    }
    pSrc += 2;

    for (i = 0; i < index; i++)
    {
        pSrc += sizeof(TRDP_FUNCTION_INFO_T);                   /* advance to next info */
    }
    memcpy(pValue, pSrc, sizeof(TRDP_FUNCTION_INFO_T));
    pValue->fctId   = vos_ntohs(((TRDP_FUNCTION_INFO_T *)pSrc)->fctId);
    *pSize          = sizeof(TRDP_FUNCTION_INFO_T);
}

void cstInfoGetCltrCstInfo (
    TRDP_CONSIST_INFO_T     *pCstInfo,
    UINT32                  index,
    TRDP_FUNCTION_INFO_T    *pValue,
    UINT32                  *pSize)
{
    UINT8   *pSrc   = (UINT8 *)&pCstInfo->cstProp + vos_ntohs(pCstInfo->cstProp.len) + 4 + 2;   /* pCstInfo->etbCnt   */
    UINT16  etbCnt  = vos_ntohs(*pSrc);
    UINT16  vehCnt;
    UINT16  fctCnt;
    UINT16  cltrCstCnt;
    UINT32  itemSize;
    UINT32  i;
    pSrc    += 2 + etbCnt * sizeof(TRDP_ETB_INFO_T) + 2;        /* pSrc points to first vehicle count    */
    vehCnt  = vos_ntohs(*(UINT16 *)pSrc);
    UINT8   *pCurInfo = (UINT8 *) (pSrc + 2);
    itemSize = cstInfoGetVehInfoSize(pCurInfo);                 /* size of first vehicle info   */

    for (i = 0; i < vehCnt; i++)
    {
        pCurInfo    += itemSize;                                /* advance to next info */
        itemSize    = cstInfoGetVehInfoSize(pCurInfo);          /* compute its size     */
    }

    pSrc    = pCurInfo + itemSize + 2;                          /* pSrc points to fctCnt    */
    fctCnt  = vos_ntohs(*(UINT16 *)pSrc);

    pSrc += 2;

    for (i = 0; i < index; i++)
    {
        pSrc += sizeof(TRDP_FUNCTION_INFO_T);                   /* advance to next info */
    }

    pSrc        = pCurInfo + itemSize + 2;                      /* pSrc points to fctCnt    */
    cltrCstCnt  = vos_ntohs(*(UINT16 *)pSrc);

    if (index > fctCnt)
    {
        memset(pValue, 0, sizeof(TRDP_CLTR_CST_INFO_T));
        *pSize = 0;
        return;
    }

    pSrc += 2;

    for (i = 0; i < index; i++)
    {
        pSrc += sizeof(TRDP_CLTR_CST_INFO_T);                   /* advance to next info */
    }

    memcpy(pValue, pSrc, sizeof(TRDP_CLTR_CST_INFO_T));
    *pSize = sizeof(TRDP_CLTR_CST_INFO_T);
}
