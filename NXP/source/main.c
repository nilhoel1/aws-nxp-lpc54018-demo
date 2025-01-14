/*
 * The Clear BSD License
 * Copyright (c) 2013 - 2014, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted (subject to the limitations in the disclaimer below) provided
 * that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Amazon FreeRTOS V1.0.0
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software. If you wish to use our Amazon
 * FreeRTOS name, please do so in a fair use way that does not cause confusion.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

///////////////////////////////////////////////////////////////////////////////
//  Includes
///////////////////////////////////////////////////////////////////////////////

/* SDK Included Files */
#include "board.h"
#include "fsl_debug_console.h"

#include "pin_mux.h"

/* Amazon FreeRTOS Demo Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "aws_clientcredential.h"
#include "aws_logging_task.h"
#include "aws_wifi.h"
#include "aws_system_init.h"
#include "aws_dev_mode_key_provisioning.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "fsl_power.h"
#include "usb_device_config.h"

#include "adc.h"
#include "shell.h"
#include "flash_setup.h"
#include "wwdt.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define ENABLE_WATCHDOG

#define main_TASK_STACK_SIZE ((uint16_t)configMINIMAL_STACK_SIZE * (uint16_t)5)

#define LOGGING_TASK_PRIORITY (tskIDLE_PRIORITY + 1)
#define LOGGING_TASK_STACK_SIZE (200)
#define LOGGING_QUEUE_LENGTH (16)

typedef enum
{
    eWifi_On,
    eWifi_Connect,
    eWifi_GetIp,
    eWifi_FailureToConnect,
    eWifi_Connected,
} t_WifiStates;

typedef enum
{
    eMain_SystemInit,
    eMain_StartShell,
    eMain_Wifi_Connect,
    eMain_StartApplication,
    eMain_Running,
} t_MainStates;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
extern void vStartLedDemoTask(void);

static void prvMainTask(void *pvParameters);
static void prvMainInit(void);
static void vStartMainTask(void);

static t_WifiStates prvWifiConnect(void);

/*******************************************************************************
 * Variables
 ******************************************************************************/
t_WifiStates yWifiState = eWifi_On;
t_MainStates yMainState = eMain_SystemInit;
uint8_t Cnt_ConnectAP = 0;

/*******************************************************************************
 * Code
 ******************************************************************************/
void BOARD_InitLED()
{
    GPIO_PortInit(BOARD_LED3_GPIO, BOARD_LED3_GPIO_PORT);
    LED3_INIT(LOGIC_LED_OFF);
}

void turnLedOn()
{
    LED3_ON();
}

void turnLedOff()
{
    LED3_OFF();
}

void vApplicationDaemonTaskStartupHook(void)
{
    /* A simple example to demonstrate key and certificate provisioning in
     * microcontroller flash using PKCS#11 interface. This should be replaced
     * by production ready key provisioning mechanism. */
    vDevModeKeyProvisioning();

    vStartMainTask();
}

static void prvMainTask(void *pvParameters)
{
    for ( ;; )
    {
        switch ( yMainState )
        {
            case eMain_SystemInit:
                if (SYSTEM_Init() == pdPASS)
                {
                    yMainState = eMain_StartShell;

                }
            break;

            case eMain_StartShell:
                vStartShellTask();

                yMainState = eMain_Wifi_Connect;
            break;

            case eMain_Wifi_Connect:
                if ( eWifi_Connected == prvWifiConnect() )
                {
                    yMainState = eMain_StartApplication;
                }
            break;

            case eMain_StartApplication:
                vStartLedDemoTask();

                yMainState = eMain_Running;
            break;

            case eMain_Running:
            break;
        }

#ifdef ENABLE_WATCHDOG
        vWatchDogRefresh();
#endif
    }
}

static void prvMainInit(void)
{
    //vFlashReadWifiSetup();

    vInitShell();

    vAdcInit();

#ifdef ENABLE_WATCHDOG
    vWatchDogInit();
#endif
}

static void vStartMainTask(void)
{
    (void)xTaskCreate(prvMainTask, "AWS-MAIN", main_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
}

int main(void)
{
    /* attach 12 MHz clock to FLEXCOMM0 (debug console) */
    CLOCK_AttachClk(BOARD_DEBUG_UART_CLK_ATTACH);

    /* reset USB0 and USB1 device */
    RESET_PeripheralReset(kUSB0D_RST_SHIFT_RSTn);
    RESET_PeripheralReset(kUSB1D_RST_SHIFT_RSTn);
    RESET_PeripheralReset(kUSB0HMR_RST_SHIFT_RSTn);
    RESET_PeripheralReset(kUSB0HSL_RST_SHIFT_RSTn);
    RESET_PeripheralReset(kUSB1H_RST_SHIFT_RSTn);

    NVIC_ClearPendingIRQ(USB0_IRQn);
    NVIC_ClearPendingIRQ(USB0_NEEDCLK_IRQn);
    NVIC_ClearPendingIRQ(USB1_IRQn);
    NVIC_ClearPendingIRQ(USB1_NEEDCLK_IRQn);

    BOARD_InitPins();
    BOARD_BootClockFROHF96M();

#if configUSE_TRACE_FACILITY == 1
    vTraceEnable(TRC_START); // Tracelyzer
#endif

#if (defined USB_DEVICE_CONFIG_LPCIP3511HS) && (USB_DEVICE_CONFIG_LPCIP3511HS)
    POWER_DisablePD(kPDRUNCFG_PD_USB1_PHY);
    /* enable usb1 host clock */
    CLOCK_EnableClock(kCLOCK_Usbh1);
    *((uint32_t *)(USBHSH_BASE + 0x50)) |= USBHSH_PORTMODE_DEV_ENABLE_MASK;
    /* enable usb1 host clock */
    CLOCK_DisableClock(kCLOCK_Usbh1);
#endif
#if (defined USB_DEVICE_CONFIG_LPCIP3511FS) && (USB_DEVICE_CONFIG_LPCIP3511FS)
    POWER_DisablePD(kPDRUNCFG_PD_USB0_PHY); /*< Turn on USB Phy */
    CLOCK_SetClkDiv(kCLOCK_DivUsb0Clk, 1, false);
    CLOCK_AttachClk(kFRO_HF_to_USB0_CLK);
    /* enable usb0 host clock */
    CLOCK_EnableClock(kCLOCK_Usbhsl0);
    *((uint32_t *)(USBFSH_BASE + 0x5C)) |= USBFSH_PORTMODE_DEV_ENABLE_MASK;
    /* disable usb0 host clock */
    CLOCK_DisableClock(kCLOCK_Usbhsl0);
#endif

    BOARD_InitDebugConsole();
    BOARD_InitLED();


    prvMainInit();

    xLoggingTaskInitialize(LOGGING_TASK_STACK_SIZE, LOGGING_TASK_PRIORITY, LOGGING_QUEUE_LENGTH);

    vTaskStartScheduler();

    for ( ;; )
    {
    }
}

static t_WifiStates prvWifiConnect(void)
{
    uint8_t tmp_ip[4] = { 0 };

    switch ( yWifiState )
    {
        case eWifi_On:
            configPRINTF( ("Starting WiFi...\r\n") );

            if ( eWiFiSuccess == WIFI_On() )
            {
                configPRINTF( ("WiFi module initialized.\r\n") );

                yWifiState = eWifi_Connect;
                configPRINTF( ("Starting WiFi Connection to AP\r\n") );
            }

        break;

        case eWifi_Connect:
            configPRINTF( ("Attempt to Connect %i.\r\n", Cnt_ConnectAP) );
            if ( eWiFiSuccess == WIFI_ConnectAP( &pxNetworkParams ) )
            {
                configPRINTF( ("WiFi connected to AP %s.\r\n", pxNetworkParams.pcSSID) );

                yWifiState = eWifi_GetIp;
            }
            else
            {
                Cnt_ConnectAP ++;
            }
            if (Cnt_ConnectAP == 6)
            {
                configPRINTF( ("WIFI Problem verify AP is reachable or right WIFI parameter, use readwifi command to see wifi parameter\r\n"));
                configPRINTF( ("Configure WIFI parameters with shell command, press Help for information\r\n"));
                configPRINTF( ("After writewifi command, reset the board\r\n"));
                yWifiState = eWifi_FailureToConnect;
            }
        break;

        case eWifi_GetIp:
            configPRINTF( ("Attempt to Get IP.\r\n") );
            if ( eWiFiSuccess == WIFI_GetIP( tmp_ip ) )
            {
                configPRINTF( ("IP Address acquired %d.%d.%d.%d\r\n", tmp_ip[0], tmp_ip[1], tmp_ip[2], tmp_ip[3]) );

                yWifiState = eWifi_Connected;
            }
            else
            {
                configPRINTF( ("WIFI Problem verify AP is reachable or right WIFI parameter, use readwifi command to see wifi parameter\r\n"));
                configPRINTF( ("Configure WIFI parameters with shell command, press Help for information\r\n"));
                configPRINTF( ("After writewifi command, reset the board\r\n"));
                yWifiState = eWifi_FailureToConnect;
            }
        break;

        case eWifi_FailureToConnect:
        break;

        case eWifi_Connected:
        break;
    }

    return yWifiState;
}

/* configUSE_STATIC_ALLOCATION is set to 1, so the application must provide an
 * implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
 * used by the Idle task. */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    /* If the buffers to be provided to the Idle task are declared inside this
     * function then they must be declared static - otherwise they will be allocated on
     * the stack and so not exists after this function exits. */
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle
     * task's state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
     * Note that, as the array is necessarily of type StackType_t,
     * configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
 * application must provide an implementation of vApplicationGetTimerTaskMemory()
 * to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    /* If the buffers to be provided to the Timer task are declared inside this
     * function then they must be declared static - otherwise they will be allocated on
     * the stack and so not exists after this function exits. */
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
     * task's state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task's stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
     * Note that, as the array is necessarily of type StackType_t,
     * configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
/**
 * @brief Warn user if pvPortMalloc fails.
 *
 * Called if a call to pvPortMalloc() fails because there is insufficient
 * free memory available in the FreeRTOS heap.  pvPortMalloc() is called
 * internally by FreeRTOS API functions that create tasks, queues, software
 * timers, and semaphores.  The size of the FreeRTOS heap is set by the
 * configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h.
 *
 */
void vApplicationMallocFailedHook()
{
    configPRINTF(("ERROR: Malloc failed to allocate memory\r\n"));
}

/**
 * @brief Loop forever if stack overflow is detected.
 *
 * If configCHECK_FOR_STACK_OVERFLOW is set to 1,
 * this hook provides a location for applications to
 * define a response to a stack overflow.
 *
 * Use this hook to help identify that a stack overflow
 * has occurred.
 *
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    portDISABLE_INTERRUPTS();

    /* Loop forever */
    for (;;)
        ;
}

void *pvPortCalloc(size_t xSize)
{
    void *pvReturn;

    pvReturn = pvPortMalloc(xSize);
    if (pvReturn != NULL)
    {
        memset(pvReturn, 0x00, xSize);
    }

    return pvReturn;
}
