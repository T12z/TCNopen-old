﻿/******************************************************************************/
/**
 * @file            trdp_env.h
 *
 * @brief           Definition of the TRDP constants and specific calculations
 *
 * @details
 *
 * @note            Project: TRDP SPY
 *
 * @author          Florian Weispfenning, Bombardier Transportation
 *
 * @remarks This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 *          If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *          Copyright Bombardier Transportation Inc. or its subsidiaries and others, 2013. All rights reserved.
 *
 * $Id: $
 *
 * @addtogroup Definitions
 * @{
 */

#ifndef TRDP_ENVIRONMENT_H
#define TRDP_ENVIRONMENT_H

/*******************************************************************************
 * INCLUDES
 */
#include <string.h>
#include <glib.h>

/*******************************************************************************
 * DEFINES
 */

#define TRDP_BOOL8      1   /**< =UINT8, 1 bit relevant (equal to zero -> false, not equal to zero -> true) */
#define TRDP_CHAR8		2	/**< char, can be used also as UTF8 */
#define TRDP_UTF16		3	/**< Unicode UTF-16 character */
#define TRDP_INT8		4	/**< Signed integer, 8 bit */
#define TRDP_INT16		5	/**< Signed integer, 16 bit */
#define TRDP_INT32		6	/**< Signed integer, 32 bit */
#define TRDP_INT64		7	/**< Signed integer, 64 bit */
#define TRDP_UINT8		8	/**< Unsigned integer, 8 bit */
#define TRDP_UINT16		9	/**< Unsigned integer, 16 bit */
#define TRDP_UINT32		10	/**< Unsigned integer, 32 bit */
#define TRDP_UINT64		11	/**< Unsigned integer, 64 bit */
#define TRDP_REAL32		12	/**< Floating point real, 32 bit */
#define TRDP_REAL64		13	/**< Floating point real, 64 bit */
#define TRDP_TIMEDATE32	14	/**< 32 bit UNIX time */
#define TRDP_TIMEDATE48	15	/**< 48 bit TCN time (32 bit seconds and 16 bit ticks) */
#define TRDP_TIMEDATE64	16	/**< 32 bit seconds and 32 bit microseconds */

#define TRDP_DEFAULT_UDPTCP_MD_PORT 20550   /*< Default port address for Message data (MD) communication */
#define TRDP_DEFAULT_UDP_PD_PORT    20548   /*< Default port address for Process data (PD) communication */

#define PROTO_TAG_TRDP          "TRDP"
#define PROTO_NAME_TRDP         "Train Real Time Data Protocol"
#define PROTO_FILTERNAME_TRDP   "trdp"

#define TRDP_HEADER_OFFSET_SEQCNT           0
#define TRDP_HEADER_OFFSET_PROTOVER         4
#define TRDP_HEADER_OFFSET_TYPE             6
#define TRDP_HEADER_OFFSET_COMID            8
#define TRDP_HEADER_OFFSET_ETB_TOPOCNT      12
#define TRDP_HEADER_OFFSET_OP_TRN_TOPOCNT   16
#define TRDP_HEADER_OFFSET_DATASETLENGTH    20

#define TRDP_HEADER_PD_OFFSET_RESERVED      24
#define TRDP_HEADER_PD_OFFSET_REPLY_COMID   28
#define TRDP_HEADER_PD_OFFSET_REPLY_IPADDR  32
#define TRDP_HEADER_PD_OFFSET_FCSHEAD       36
#define TRDP_HEADER_PD_OFFSET_DATA          40

#define TRDP_HEADER_MD_OFFSET_REPLY_STATUS  24
#define TRDP_HEADER_MD_SESSIONID0           28
#define TRDP_HEADER_MD_SESSIONID1           32
#define TRDP_HEADER_MD_SESSIONID2           36
#define TRDP_HEADER_MD_SESSIONID3           40
#define TRDP_HEADER_MD_REPLY_TIMEOUT        44
#define TRDP_HEADER_MD_SRC_URI              48
#define TRDP_HEADER_MD_DEST_URI             80
#define TRDP_HEADER_MD_OFFSET_FCSHEAD       112
#define TRDP_HEADER_MD_OFFSET_DATA          116


#define TRDP_MD_HEADERLENGTH    TRDP_HEADER_MD_OFFSET_DATA /**< Length of the TRDP header of an MD message */

#define TRDP_FCS_LENGTH 4   /**< The CRC calculation results in a 32bit result so 4 bytes are necessary */

/*******************************************************************************
 * TYPEDEFS
 */


/*******************************************************************************
 * GLOBAL FUNCTIONS
 */


/** @fn guint32 trdp_fcs32(const guint8 buf[], guint32 len, guint32 fcs)
 *
 * @brief Compute crc32 according to IEEE802.3.
 *
 * @note Returned CRC is inverted
 *
 * @param[in] buf   Input buffer
 * @param[in] len   Length of input buffer
 * @param[in] fcs   Initial (seed) value for the FCS calculation
 *
 * @return Calculated fcs value
 */
guint32 trdp_fcs32(const guint8 buf[], guint32 len, guint32 fcs);

/** @fn guint8 trdp_dissect_width(guint32 type)
 * @brief Lookup table for length of the standard types.
 * The width of an element in bytes.
 * Extracted from table3 at TCN-TRDP2-D-BOM-011-19.
 *
 * @param[in] type			the numeric representation of a type
 *
 * @return the width in byte of one element of the given type
 */
guint8 trdp_dissect_width(guint32 type);

#endif

/** @} */
