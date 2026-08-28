/*
 * FreeRTOS V202212.00
 * Optimized for PolarFire SoC (MPFS) + lwIP TCP/IP Stack Integration
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* PolarFire HAL includes */
#include "mpfs_hal/mss_hal.h"
#include "drivers/mss/mss_mmuart/mss_uart.h"

/*-----------------------------------------------------------
 * Application Specific Definitions
 *-----------------------------------------------------------*/
#define CLINT_CTRL_ADDR                         ( 0x02000000UL )
#define configMTIME_BASE_ADDRESS                ( CLINT_CTRL_ADDR + 0xBFF8UL )
#define configMTIMECMP_BASE_ADDRESS             ( CLINT_CTRL_ADDR + 0x4000UL )

#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0

/* FIX: PolarFire SoC CLINT mtime timer runs at 1 MHz */
#define configCPU_CLOCK_HZ                      ( 1000000UL )
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )

#define configMAX_PRIORITIES                    ( 7 )
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 512 )
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 128 * 1024 ) ) /* Expanded for lwIP Pools */
#define configMAX_TASK_NAME_LEN                 ( 16 )
#define configUSE_TRACE_FACILITY                0
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 0
#define configUSE_MUTEXES                       1
#define configQUEUE_REGISTRY_SIZE               8
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_APPLICATION_TASK_TAG          0
#define configUSE_COUNTING_SEMAPHORES           1
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_QUEUE_SETS                    1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   3

/* Co-routine definitions */
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         ( 2 )

/* Software timer definitions (Required for lwIP tcpip_thread timeouts) */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH                16
#define configTIMER_TASK_STACK_DEPTH            ( 512 )

/* Task priorities */
#ifndef uartPRIMARY_PRIORITY
    #define uartPRIMARY_PRIORITY                ( configMAX_PRIORITIES - 3 )
#endif

/* Include API functions */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskCleanUpResources           1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_eTaskGetState                   1
#define INCLUDE_xTimerPendFunctionCall          1
#define INCLUDE_xTaskAbortDelay                 1
#define INCLUDE_xTaskGetHandle                  1
#define INCLUDE_xSemaphoreGetMutexHolder        1

/* Assert configuration */
void vAssertCalled( void );
#define configASSERT( x ) if( ( x ) == 0 ) vAssertCalled()

/* Map to platform UART write function */
#define configPRINT_STRING( pcString )          MSS_UART_polled_tx_string( &( g_mss_uart0_lo ), pcString )

/* Disable regression test flags for production/application build */
#define configSTART_TASK_NOTIFY_TESTS             0
#define configSTART_TASK_NOTIFY_ARRAY_TESTS       0
#define configSTART_BLOCKING_QUEUE_TESTS          0
#define configSTART_SEMAPHORE_TESTS               0
#define configSTART_POLLED_QUEUE_TESTS            0
#define configSTART_INTEGER_MATH_TESTS            0
#define configSTART_GENERIC_QUEUE_TESTS           0
#define configSTART_PEEK_QUEUE_TESTS              0
#define configSTART_MATH_TESTS                    0
#define configSTART_RECURSIVE_MUTEX_TESTS         0
#define configSTART_COUNTING_SEMAPHORE_TESTS      0
#define configSTART_QUEUE_SET_TESTS               0
#define configSTART_QUEUE_OVERWRITE_TESTS         0
#define configSTART_EVENT_GROUP_TESTS             0
#define configSTART_INTERRUPT_SEMAPHORE_TESTS     0
#define configSTART_QUEUE_SET_POLLING_TESTS       0
#define configSTART_BLOCK_TIME_TESTS              0
#define configSTART_ABORT_DELAY_TESTS             0
#define configSTART_MESSAGE_BUFFER_TESTS          0
#define configSTART_STREAM_BUFFER_TESTS           0
#define configSTART_STREAM_BUFFER_INTERRUPT_TESTS 0
#define configSTART_TIMER_TESTS                   0
#define configSTART_REGISTER_TESTS                0
#define configSTART_DELETE_SELF_TESTS             0

/* Route FreeRTOS RISC-V external interrupts to Microchip PLIC driver */
#define portasmHANDLE_INTERRUPT handle_m_ext_interrupt

#endif /* FREERTOS_CONFIG_H */
