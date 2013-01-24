/**********************************************************************************************************************/
/**
 * @file            vos_mem.h
 *
 * @brief           Memory and queue functions for OS abstraction
 *
 * @details         This module provides memory control supervison
 *
 * @note            Project: TCNOpen TRDP prototype stack
 *
 * @author          Bernd Loehr, NewTec GmbH
 *                  Peter Brander (Memory scheme)
 *
 * @remarks All rights reserved. Reproduction, modification, use or disclosure
 *          to third parties without express authority is forbidden,
 *          Copyright Bombardier Transportation GmbH, Germany, 2012.
 *
 *
 * $Id$
 *
 */

#ifndef VOS_MEM_H
#define VOS_MEM_H

/***********************************************************************************************************************
 * INCLUDES
 */

#include "vos_types.h"

#ifdef __cplusplus
extern "C" {
#endif


/***********************************************************************************************************************
 * DEFINES
 */

#ifndef __cdecl
#define __cdecl
#endif

#define VOS_MEM_MAX_PREALLOCATE  10     /*<< Max blocks to pre-allocate */

/** We internally allocate memory always by these block sizes. The largest available block is 524288 Bytes, provided
    the overal size of the used memory allocation area is larger. */
#define VOS_MEM_BLOCKSIZES  {32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, \
                             16384, 32768, 65536, 131072, 262144, 524288}

/** Default pre-allocation of free memory blocks. To avoid problems with too many small blocks and no large one.
   Specify how many of each block size that should be pre-allocated (and freed!) to pre-segment the memory area. */
#define VOS_MEM_PREALLOCATE  {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 4, 0, 0}

/***********************************************************************************************************************
 * TYPEDEFS
 */

/** Opaque queue define  */
typedef struct VOS_QUEUE *VOS_QUEUE_T;

/** enumeration for memory block sizes */
typedef enum
{
    TRDP_MEM_BLK_32 = 0,
    TRDP_MEM_BLK_64,
    TRDP_MEM_BLK_128,
    TRDP_MEM_BLK_256,
    TRDP_MEM_BLK_512,
    TRDP_MEM_BLK_1024,
    TRDP_MEM_BLK_2048,
    TRDP_MEM_BLK_4096,
    TRDP_MEM_BLK_8192,
    TRDP_MEM_BLK_16384,
    TRDP_MEM_BLK_32768,
    TRDP_MEM_BLK_65536,
    TRDP_MEM_BLK_131072,
    TRDP_MEM_BLK_262144,
    TRDP_MEM_BLK_524288,
    VOS_MEM_NBLOCKSIZES         /*<< Total number of different sizes of memory allocation blocks */
} VOS_MEM_BLK_T;

/***********************************************************************************************************************
 * PROTOTYPES
 */

/**********************************************************************************************************************/
/*  Memory                                                                                                            */
/**********************************************************************************************************************/

/**********************************************************************************************************************/
/** Initialize the memory unit.
 *  Init a supplied block of memory and prepare it for use with vos_alloc and vos_dealloc. The used block sizes can
 *  be supplied and will be preallocated.
 *
 *  @param[in]      pMemoryArea     Pointer to memory area to use
 *  @param[in]      size            Size of provided memory area
 *  @param[in]      fragMem         Pointer to list of preallocate block sizes, used to fragment memory for large blocks
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   parameter out of range/invalid
 *  @retval         VOS_MEM_ERR     no memory available
 */

EXT_DECL VOS_ERR_T vos_memInit (
    UINT8           *pMemoryArea,
    UINT32          size,
    const UINT32    fragMem[VOS_MEM_NBLOCKSIZES]);

/**********************************************************************************************************************/
/** Delete the memory area.
 *  This will eventually invalidate any previously allocated memory blocks! It should be called last before the
 *  application quits. No further access to the memory blocks is allowed after this call.
 *
 *  @param[in]      pMemoryArea     Pointer to memory area to use
 */

EXT_DECL void vos_memDelete (
    UINT8 *pMemoryArea);

/**********************************************************************************************************************/
/** Allocate a block of memory (from memory area above).
 *
 *  @param[in]      size            Size of requested block
 *
 *  @retval         Pointer to memory area
 *  @retval         NULL if no memory available
 */

EXT_DECL UINT8 *vos_memAlloc (
    UINT32 size);

/**********************************************************************************************************************/
/** Deallocate a block of memory (from memory area above).
 *
 *  @param[in]      pMemBlock       Pointer to memory block to be freed
 */

EXT_DECL void vos_memFree (
    void *pMemBlock);

/**********************************************************************************************************************/
/** Return used and available memory (of memory area above).
 *
 *  @param[out]     pAllocatedMemory    Pointer to allocated memory size
 *  @param[out]     pFreeMemory         Pointer to free memory size
 *  @param[out]     pMinFree            Pointer to minimal free memory size in statistics interval
 *  @param[out]     pNumAllocBlocks     Pointer to number of allocated memory blocks
 *  @param[out]     pNumAllocErr        Pointer to number of allocation errors
 *  @param[out]     pNumFreeErr         Pointer to number of free errors
 *  @param[out]     allocBlockSize      Pointer to list of allocated memory blocks
 *  @param[out]     usedBlockSize       Pointer to list of used memoryblocks
 *  @retval         VOS_NO_ERR          no error
 *  @retval         VOS_INIT_ERR        module not initialised
 *  @retval         VOS_PARAM_ERR       parameter out of range/invalid
 */

EXT_DECL VOS_ERR_T vos_memCount(
    UINT32  * pAllocatedMemory,
    UINT32  * pFreeMemory,
    UINT32  * pMinFree,
    UINT32  * pNumAllocBlocks,
    UINT32  * pNumAllocErr,
    UINT32  * pNumFreeErr,
    UINT32 allocBlockSize[VOS_MEM_NBLOCKSIZES],
    UINT32 usedBlockSize[VOS_MEM_NBLOCKSIZES]);

/**********************************************************************************************************************/
/*  Sorting/Searching                                                                                                 */
/**********************************************************************************************************************/

/**********************************************************************************************************************/
/** Sort an array.
 *  This is just a wrapper for the standard qsort function.
 *
 *  @param[in,out]  pBuf            Pointer to the array to sort
 *  @param[in]      num             number of elements
 *  @param[in]      size            size of one element
 *  @param[in]      compare         Pointer to compare function
 *  @retval         none
 */

EXT_DECL void vos_qsort (
    void        *pBuf,
    UINT32      num,
    UINT32      size,
    int         (*compare)(
        const   void *,
        const   void *));


/**********************************************************************************************************************/
/** Binary search in a sorted array.
 *  This is just a wrapper for the standard qsort function.
 *
 *  @param[in]      pKey            Key to search for
 *  @param[in]      pBuf            Pointer to the array to sort
 *  @param[in]      num             number of elements
 *  @param[in]      size            size of one element
 *  @param[in]      compare         Pointer to compare function
 *  @retval
 */

EXT_DECL void *vos_bsearch (
    const void  *pKey,
    const void  *pBuf,
    UINT32      num,
    UINT32      size,
    int         (*compare)(
        const   void *,
        const   void *));


/**********************************************************************************************************************/
/** Case insensitive string compare.
 *
 *  @param[in]      pStr1         Null terminated string to compare
 *  @param[in]      pStr1         Null terminated string to compare
 *  @param[in]      count         Maximum number of characters to compare 
 *
 *  @retval         0  - equal 
 *  @retval         <0 - string1 less than string 2
 *  @retval         >0 - string 1 greater than string 2
 */

EXT_DECL INT32 vos_strnicmp ( 
    const CHAR8 *pStr1,
    const CHAR8 *pStr2,
    UINT32       count );


/**********************************************************************************************************************/
/** String copy with length limitation.
 *
 *  @param[in]      pStrDst       Destination string
 *  @param[in]      pStrSrc       Null terminated string to copy
 *  @param[in]      count         Maximum number of characters to copy 
 *
 *  @retval         none
 */

EXT_DECL void vos_strncpy ( 
    CHAR8       *pStr1,
    const CHAR8 *pStr2,
    UINT32       count );

#ifdef __cplusplus
}
#endif

#endif /* VOS_MEM_H */
