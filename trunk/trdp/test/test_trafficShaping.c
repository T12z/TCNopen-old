/**********************************************************************************************************************/
/**
 * @file            test_trafficShaping.c
 *
 * @brief           Test application for TRDP
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

/***********************************************************************************************************************
 * INCLUDES
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef POSIX
#include <unistd.h>
#include <sys/select.h>
#endif
#include "trdp_if_light.h"
#include "vos_thread.h"

/***********************************************************************************************************************
 * DEFINITIONS
 */
#define APP_VERSION     "1.0"

#define DATA_MAX        1000

#define PD_COMID1      	1001
#define PD_COMID_CYCLE1	1000000             /* in us (1000000 = 1 sec) */
#define PD_SIZE1		1000
#define PD_COMID2      	1002
#define PD_COMID_CYCLE2	100000              /* in us (100000 = 0.1 sec) */
#define PD_SIZE2		1000
#define PD_COMID3      	1003
#define PD_COMID_CYCLE3	20000               /* in us (20000 = 0.02 sec) */
#define PD_SIZE3		1000
#define PD_COMID4      	1004
#define PD_COMID_CYCLE4	50000                /* in us (50000 = 0.05 sec) */
#define PD_SIZE4		1000
#define PD_COMID5      	1005
#define PD_COMID_CYCLE5	20000               /* in us (20000 = 0.02 sec) */
#define PD_SIZE5		1000
#define PD_COMID6      	1006
#define PD_COMID_CYCLE6	10000000             /* in us (10000000 = 10 sec) */
#define PD_SIZE6		1000
#define PD_COMID7      	1007
#define PD_COMID_CYCLE7	5000000             /* in us (5000000 = 5 sec) */
#define PD_SIZE7		1000
#define PD_COMID8      	1008
#define PD_COMID_CYCLE8	1000000             /* in us (1000000 = 1 sec) */
#define PD_SIZE8		1000

/* We use dynamic memory	*/
#define RESERVED_MEMORY  100000


typedef struct testData {
    UINT32	comID;
    UINT32  cycle;
    UINT32	size;
} TESTDATA_T;

#define NoOfPackets		8
TESTDATA_T	gPD[NoOfPackets] = 
{
    1001, 1000000, 1000,
    1002, 100000, 1000,
    1003, 100000, 1000,
    1004, 2000000, 1000,
    1005, 30000, 1000,
    1006, 30000, 1000,
    1007, 50000, 1000,
    1008, 5000000, 1000
};

/* Print a sensible usage message */
void usage (const char *appName)
{
    printf("%s: Version %s\t(%s - %s)\n", appName, APP_VERSION, __DATE__, __TIME__);
    printf("Usage of %s\n", appName);
    printf("This tool sends PD messages to an ED.\n"
           "Arguments are:\n"
           "  own IP address in dotted decimal\n"
           "  target IP address in dotted decimal\n"
           );
}

/**********************************************************************************************************************/
/** callback routine for TRDP logging/error output
 *
 *  @param[in]      pRefCon			user supplied context pointer
 *  @param[in]		category		Log category (Error, Warning, Info etc.)
 *  @param[in]		pTime			pointer to NULL-terminated string of time stamp
 *  @param[in]		pFile			pointer to NULL-terminated string of source module
 *  @param[in]		LineNumber		line
 *  @param[in]		pMsgStr         pointer to NULL-terminated string
 *  @retval         none
 */
void dbgOut (
             void        *pRefCon,
             TRDP_LOG_T  category,
             const CHAR8 *pTime,
             const CHAR8 *pFile,
             UINT16      LineNumber,
             const CHAR8 *pMsgStr)
{
    const char *catStr[] = {"**Error:", "Warning:", "   Info:", "  Debug:"};
    if (category == VOS_LOG_DBG)
    {
        return;
    }

    printf("%s %s %s:%d %s",
           pTime,
           catStr[category],
           pFile,
           LineNumber,
           pMsgStr);
}


/**********************************************************************************************************************/
/** main entry
 *
 *  @retval         0		no error
 *  @retval         1		some error
 */
int main (int argc, char *argv[])
{
    int                 ip[4];
    TRDP_APP_SESSION_T  appHandle;  /*	Our identifier to the library instance	*/
    TRDP_PUB_T          pubHandle;  /*	Our identifier to the publication	*/
    TRDP_ERR_T          err;
    TRDP_PD_CONFIG_T    pdConfiguration = {NULL, NULL, {0, 64}, TRDP_FLAGS_NONE, 1000, TRDP_TO_SET_TO_ZERO};
    TRDP_MEM_CONFIG_T   dynamicConfig = {NULL, RESERVED_MEMORY, {0}};
    TRDP_PROCESS_CONFIG_T   processConfig   = {"Me", "", 0, 0, TRDP_OPTION_BLOCK | TRDP_OPTION_TRAFFIC_SHAPING};
    
    int                 rv = 0;
    UINT32              destIP = 0;
    UINT32              ownIP = 0;

    /*	Generate some data, that we want to send, when nothing was specified. */
    UINT8               *outputBuffer;
    UINT8               exampleData[DATA_MAX]   = "Hello World";
    int                 i;
    
    outputBuffer = exampleData;
    
    if (argc <= 2)
    {
        usage(argv[0]);
        return 1;
    }


    if (sscanf(argv[1], "%u.%u.%u.%u",  &ip[3], &ip[2], &ip[1], &ip[0]) < 4)
    {
        usage(argv[0]);
        exit(1);
    }
    else
    {
        ownIP = (ip[3] << 24) | (ip[2] << 16) | (ip[1] << 8) | ip[0];
    }

    if (sscanf(argv[2], "%u.%u.%u.%u",  &ip[3], &ip[2], &ip[1], &ip[0]) < 4)
    {
        usage(argv[0]);
        exit(1);
    }
    else
    {
        destIP = (ip[3] << 24) | (ip[2] << 16) | (ip[1] << 8) | ip[0];
    }

    if (destIP == 0)
    {
        fprintf(stderr, "No destination address given!\n");
        usage(argv[0]);
        return 1;
    }
    
    printf("%s: Version %s\t(%s - %s)\n", argv[0], APP_VERSION, __DATE__, __TIME__);

    /*	Init the library  */
    if (tlc_init(dbgOut,                              /* no logging	*/
                 &dynamicConfig) != TRDP_NO_ERR)    /* Use application supplied memory	*/
        
    {
        printf("Initialization error\n");
        return 1;
    }
    
    /*	Open a session  */
    if (tlc_openSession(&appHandle,
                        ownIP,
                        0,                         /* use default IP address */
                        NULL,                      /* no Marshalling	*/
                        &pdConfiguration, NULL,    /* system defaults for PD and MD	*/
                        &processConfig) != TRDP_NO_ERR)
    {
        printf("Initialization error\n");
        return 1;
    }

	for (i = 0; i < NoOfPackets; i++)
    {    
        
        /*	Copy the packet into the internal send queue, prepare for sending.	*/
        /*	If we change the data, just re-publish it	*/
        err = tlp_publish(  appHandle,                  /*	our application identifier	*/
                          &pubHandle,                 /*	our pulication identifier	*/
                          gPD[i].comID,
                          0,                          /*	local consist only			*/
                          0,                          /*	default source IP			*/
                          destIP,                     /*	where to send to			*/
                          gPD[i].cycle,             /*	Cycle time in us			*/
                          0,                          /*	not redundant				*/
                          TRDP_FLAGS_NONE,            /*	Use callback for errors		*/
                          NULL,                       /*	default qos and ttl			*/
                          (UINT8 *)outputBuffer,      /*	initial data                */
                          gPD[i].size,           			/*	data size					*/
                          FALSE,                      /*	no ladder					*/
                          0);                         /*	no ladder					*/
        
        
        if (err != TRDP_NO_ERR)
        {
            printf("prep pd error\n");
            tlc_terminate();
            return 1;
        }
    }
    
    
    /*
     Enter the main processing loop.
     */
    while (1)
    {
        fd_set  rfds;
        INT32   noDesc;
        struct timeval  tv;
        struct timeval  max_tv = {0, 10000};
        
        /*
         Prepare the file descriptor set for the select call.
         Additional descriptors can be added here.
         */
        FD_ZERO(&rfds);
        
        /*
         Compute the min. timeout value for select.
         This way we can guarantee that PDs are sent in time
         with minimum CPU load and minimum jitter.
         */
        tlc_getInterval(appHandle, (TRDP_TIME_T *) &tv, (TRDP_FDS_T *) &rfds, &noDesc);
        
        /*
         The wait time for select must consider cycle times and timeouts of
         the PD packets received or sent.
         If we need to poll something faster than the lowest PD cycle,
         we need to set the maximum time out our self.
         */
        if (vos_cmpTime((TRDP_TIME_T *) &tv, (TRDP_TIME_T *) &max_tv) > 0)
        {
            tv = max_tv;
        }
        
        /*
         Select() will wait for ready descriptors or time out,
         what ever comes first.
         */
        rv = select((int)noDesc, &rfds, NULL, NULL, &tv);
        
        /*
         Check for overdue PDs (sending and receiving)
         Send any pending PDs if it's time...
         Detect missing PDs...
         'rv' will be updated to show the handled events, if there are
         more than one...
         The callback function will be called from within the tlc_process
         function (in it's context and thread)!
         */
        tlc_process(appHandle, (TRDP_FDS_T *) &rfds, &rv);
        
        /* Handle other ready descriptors... */
        if (rv > 0)
        {
            printf("other descriptors were ready\n");
        }
        else
        {
            //printf(".");
			//fflush(stdout);
        }
        
        /* sprintf((char *)outputBuffer, "Just a Counter: %08d", hugeCounter++);
        
        err = tlp_put(appHandle, pubHandle, outputBuffer, outputBufferSize);
        if (err != TRDP_NO_ERR)
        {
            printf("put pd error\n");
            rv = 1;
            break;
        }	*/
    }
    
    /*
     *	We always clean up behind us!
     */
    tlp_unpublish(appHandle, pubHandle);
    
    tlc_terminate();
    return rv;
}
