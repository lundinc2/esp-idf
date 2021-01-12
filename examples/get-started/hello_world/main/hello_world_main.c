/* Hello World Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */
/* FreeRTOS+POSIX */
#include "FreeRTOS_POSIX.h"
#include "FreeRTOS_POSIX/pthread.h"
#include "FreeRTOS_POSIX/mqueue.h"
#include "FreeRTOS_POSIX/time.h"
#include "FreeRTOS_POSIX/fcntl.h"
#include "FreeRTOS_POSIX/errno.h"

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

/* Constants */
#define LINE_BREAK    "\r\n"

/**
 * @brief Control messages.
 *
 * uint8_t is sufficient for this enum, that we are going to cast to char directly.
 * If ever needed, implement a function to properly typecast.
 */
/**@{ */
typedef enum ControlMessage
{
    eMSG_LOWER_INAVLID = 0x00,        /**< Guard, let's not use 0x00 for messages. */
    eWORKER_CTRL_MSG_CONTINUE = 0x01, /**< Dispatcher to worker, distributing another job. */
    eWORKER_CTRL_MSG_EXIT = 0x02,     /**< Dispatcher to worker, all jobs are finished and the worker receiving such can exit. */

    /* define additional messages here */

    eMSG_UPPER_INVALID = 0xFF /**< Guard, additional tasks shall be defined above. */
} eControlMessage;
/**@} */

/**
 * @defgroup Configuration constants for the dispatcher-worker demo.
 */
/**@{ */
#define MQUEUE_NUMBER_OF_WORKERS    ( 4 )                         /**< The number of worker threads, each thread has one queue which is used as income box. */

#if ( MQUEUE_NUMBER_OF_WORKERS > 10 )
    #error "Please keep MQUEUE_NUMBER_OF_WORKERS < 10."
#endif

#define MQUEUE_WORKER_QNAME_BASE                "/qNode0"         /**< Queue name base. */
#define MQUEUE_WORKER_QNAME_BASE_LEN            ( 6 )             /** Queue name base length. */

#define MQUEUE_TIMEOUT_SECONDS                  ( 1 )             /**< Relative timeout for mqueue functions. */
#define MQUEUE_MAX_NUMBER_OF_MESSAGES_WORKER    ( 1 )             /**< Maximum number of messages in a queue. */

#define MQUEUE_MSG_WORKER_CTRL_MSG_SIZE         sizeof( uint8_t ) /**< Control message size. */
#define DEMO_ERROR                              ( -1 )            /**< Any non-zero value would work. */
/**@} */

/**
 * @brief Structure used by Worker thread.
 */
/**@{ */
typedef struct WorkerThreadResources
{
    pthread_t pxID; /**< thread ID. */
    mqd_t xInboxID; /**< mqueue inbox ID. */
} WorkerThreadResources_t;
/**@} */

/**
 * @brief Structure used by Dispatcher thread.
 */
/**@{ */
typedef struct DispatcherThreadResources
{
    pthread_t pxID;    /**< thread ID. */
    mqd_t * pOutboxID; /**< a list of mqueue outbox ID. */
} DispatcherThreadResources_t;
/**@} */

/*-----------------------------------------------------------*/

static void * prvWorkerThread( void * pvArgs )
{
    WorkerThreadResources_t pArgList = *( WorkerThreadResources_t * ) pvArgs;

    printf( "Worker thread #[%d] - start %s", ( int ) pArgList.pxID, LINE_BREAK );

    struct timespec xReceiveTimeout = { 0 };

    ssize_t xMessageSize = 0;
    char pcReceiveBuffer[ MQUEUE_MSG_WORKER_CTRL_MSG_SIZE ] = { 0 };

    /* This is a worker thread that reacts based on what is sent to its inbox (mqueue). */
    while( true )
    {
        clock_gettime( CLOCK_REALTIME, &xReceiveTimeout );
        xReceiveTimeout.tv_sec += MQUEUE_TIMEOUT_SECONDS;

        xMessageSize = mq_receive( pArgList.xInboxID,
                                   pcReceiveBuffer,
                                   MQUEUE_MSG_WORKER_CTRL_MSG_SIZE,
                                   0 );

        /* Parse messages */
        if( xMessageSize == MQUEUE_MSG_WORKER_CTRL_MSG_SIZE )
        {
            switch( ( int ) pcReceiveBuffer[ 0 ] )
            {
                case eWORKER_CTRL_MSG_CONTINUE:
                    /* Task branch, currently only prints message to screen. */
                    /* Could perform tasks here. Could also notify dispatcher upon completion, if desired. */
                    printf( "Worker thread #[%d] -- Received eWORKER_CTRL_MSG_CONTINUE %s", ( int ) pArgList.pxID, LINE_BREAK );
                    break;

                case eWORKER_CTRL_MSG_EXIT:
                    printf( "Worker thread #[%d] -- Finished. Exit now. %s", ( int ) pArgList.pxID, LINE_BREAK );

                    return NULL;

                default:
                    /* Received a message that we don't care or not defined. */
                    break;
            }
        }
        else
        {
            /* Invalid message. Error handling can be done here, if desired. */
        }
    }

    /* You should never hit here. */
    /* return NULL; */
}

/*-----------------------------------------------------------*/

static void * prvDispatcherThread( void * pvArgs )
{
    DispatcherThreadResources_t pArgList = *( DispatcherThreadResources_t * ) pvArgs;

    printf( "Dispatcher thread - start %s", LINE_BREAK );

    struct timespec xSendTimeout = { 0 };

    ssize_t xMessageSize = 0;
    char pcSendBuffer[ MQUEUE_MSG_WORKER_CTRL_MSG_SIZE ] = { 0 };

    /* Just for fun, let threads do a total of 100 independent tasks. */
    int i = 0;
    const int totalNumOfJobsPerThread = 100;

    /* Distribute 1000 independent tasks to workers, in round-robin fashion. */
    pcSendBuffer[ 0 ] = ( char ) eWORKER_CTRL_MSG_CONTINUE;

    for( i = 0; i < totalNumOfJobsPerThread; i++ )
    {
        clock_gettime( CLOCK_REALTIME, &xSendTimeout );
        xSendTimeout.tv_sec += MQUEUE_TIMEOUT_SECONDS;

        printf( "Dispatcher iteration #[%d] -- Sending msg to worker thread #[%d]. %s", i, ( int ) pArgList.pOutboxID[ i % MQUEUE_NUMBER_OF_WORKERS ], LINE_BREAK );

        xMessageSize = mq_timedsend( pArgList.pOutboxID[ i % MQUEUE_NUMBER_OF_WORKERS ],
                                     pcSendBuffer,
                                     MQUEUE_MSG_WORKER_CTRL_MSG_SIZE,
                                     0,
                                     &xSendTimeout );

        if( xMessageSize != 0 )
        {
            /* This error is acceptable in our setup.
             * Since inbox for each thread fits only one message.
             * In reality, balance inbox size, message arrival rate, and message drop rate. */
            printf( "An acceptable failure -- dispatcher failed to send eWORKER_CTRL_MSG_CONTINUE to outbox ID: %x. errno %d %s",
                    ( int ) pArgList.pOutboxID[ i % MQUEUE_NUMBER_OF_WORKERS ], errno, LINE_BREAK );
        }
    }

    /* Control thread is now done with distributing jobs. Tell workers they are done. */
    pcSendBuffer[ 0 ] = ( char ) eWORKER_CTRL_MSG_EXIT;

    for( i = 0; i < MQUEUE_NUMBER_OF_WORKERS; i++ )
    {
        printf( "Dispatcher [%d] -- Sending eWORKER_CTRL_MSG_EXIT to worker thread #[%d]. %s", i, ( int ) pArgList.pOutboxID[ i % MQUEUE_NUMBER_OF_WORKERS ], LINE_BREAK );

        /* This is a blocking call, to guarantee worker thread exits. */
        xMessageSize = mq_send( pArgList.pOutboxID[ i % MQUEUE_NUMBER_OF_WORKERS ],
                                pcSendBuffer,
                                MQUEUE_MSG_WORKER_CTRL_MSG_SIZE,
                                0 );
    }

    return NULL;
}

/*-----------------------------------------------------------*/

/**
 * @brief Job distribution with actor model.
 *
 * See the top of this file for detailed description.
 */
int app_main( void )
{
    printf( "Hello world!\n" );

    /* Print chip information */
    esp_chip_info_t chip_info;

    esp_chip_info( &chip_info );
    printf( "This is %s chip with %d CPU cores, WiFi%s%s, ",
            CONFIG_IDF_TARGET,
            chip_info.cores,
            ( chip_info.features & CHIP_FEATURE_BT ) ? "/BT" : "",
            ( chip_info.features & CHIP_FEATURE_BLE ) ? "/BLE" : "" );

    printf( "silicon revision %d, ", chip_info.revision );

    printf( "%dMB %s flash\n", spi_flash_get_chip_size() / ( 1024 * 1024 ),
            ( chip_info.features & CHIP_FEATURE_EMB_FLASH ) ? "embedded" : "external" );

    printf( "Free heap: %d\n", esp_get_free_heap_size() );

    int i = 0;
    int iStatus = 0;

    /* Handles of the threads and related resources. */
    DispatcherThreadResources_t pxDispatcher = { 0 };
    WorkerThreadResources_t pxWorkers[ MQUEUE_NUMBER_OF_WORKERS ] = { { 0 } };
    mqd_t workerMqueues[ MQUEUE_NUMBER_OF_WORKERS ] = { 0 };

    struct mq_attr xQueueAttributesWorker =
    {
        .mq_flags   = 0,
        .mq_maxmsg  = MQUEUE_MAX_NUMBER_OF_MESSAGES_WORKER,
        .mq_msgsize = MQUEUE_MSG_WORKER_CTRL_MSG_SIZE,
        .mq_curmsgs = 0
    };

    pxDispatcher.pOutboxID = workerMqueues;

    /* Create message queues for each worker thread. */
    for( i = 0; i < MQUEUE_NUMBER_OF_WORKERS; i++ )
    {
        /* Prepare a unique queue name for each worker. */
        char qName[] = MQUEUE_WORKER_QNAME_BASE;
        qName[ MQUEUE_WORKER_QNAME_BASE_LEN - 1 ] = qName[ MQUEUE_WORKER_QNAME_BASE_LEN - 1 ] + i;

        /* Open a queue with --
         * O_CREAT -- create a message queue.
         * O_RDWR -- both receiving and sending messages.
         */
        pxWorkers[ i ].xInboxID = mq_open( qName,
                                           O_CREAT | O_RDWR,
                                           ( mode_t ) 0,
                                           &xQueueAttributesWorker );

        if( pxWorkers[ i ].xInboxID == ( mqd_t ) -1 )
        {
            printf( "Invalid inbox (mqueue) for worker. %s", LINE_BREAK );
            iStatus = DEMO_ERROR;
            break;
        }

        /* Outboxes of dispatcher thread is the inboxes of all worker threads. */
        pxDispatcher.pOutboxID[ i ] = pxWorkers[ i ].xInboxID;
    }

    /* Create and start Worker threads. */
    if( iStatus == 0 )
    {
        for( i = 0; i < MQUEUE_NUMBER_OF_WORKERS; i++ )
        {
            ( void ) pthread_create( &( pxWorkers[ i ].pxID ), NULL, prvWorkerThread, &pxWorkers[ i ] );
        }

        /* Create and start dispatcher thread. */
        ( void ) pthread_create( &( pxDispatcher.pxID ), NULL, prvDispatcherThread, &pxDispatcher );

        /* Actors will do predefined tasks in threads. Current implementation is that
         * dispatcher actor notifies worker actors to terminate upon finishing distributing tasks. */

        /* Wait for worker threads to join. */
        for( i = 0; i < MQUEUE_NUMBER_OF_WORKERS; i++ )
        {
            ( void ) pthread_join( pxWorkers[ i ].pxID, NULL );
        }

        /* Wait for dispatcher thread to join. */
        ( void ) pthread_join( pxDispatcher.pxID, NULL );
    }

    /* Close and unlink worker message queues. */
    for( i = 0; i < MQUEUE_NUMBER_OF_WORKERS; i++ )
    {
        char qName[] = MQUEUE_WORKER_QNAME_BASE;
        qName[ MQUEUE_WORKER_QNAME_BASE_LEN - 1 ] = qName[ MQUEUE_WORKER_QNAME_BASE_LEN - 1 ] + i;

        if( pxWorkers[ i ].xInboxID != 0 )
        {
            ( void ) mq_close( pxWorkers[ i ].xInboxID );
            ( void ) mq_unlink( qName );
        }
    }

    /* Have something on console. */
    if( iStatus == 0 )
    {
        printf( "All threads finished. %s", LINE_BREAK );
    }
    else
    {
        printf( "Queues did not get initialized properly. Did not run demo. %s", LINE_BREAK );
    }

    for( int i = 10; i >= 0; i-- )
    {
        printf( "Restarting in %d seconds...\n", i );
        vTaskDelay( 1000 / portTICK_PERIOD_MS );
    }

    printf( "Restarting now.\n" );
    fflush( stdout );
    esp_restart();
}
