/**********************************************************************************************************************
 * \file Scu_Power_Down_Idle.c
 * \copyright Copyright (C) Infineon Technologies AG 2019
 *
 * Use of this file is subject to the terms of use agreed between (i) you or the company in which ordinary course of
 * business you are acting and (ii) Infineon Technologies AG or its licensees. If and as long as no such terms of use
 * are agreed, use of this file is subject to following:
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization obtaining a copy of the software and
 * accompanying documentation covered by this license (the "Software") to use, reproduce, display, distribute, execute,
 * and transmit the Software, and to prepare derivative works of the Software, and to permit third-parties to whom the
 * Software is furnished to do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including the above license grant, this restriction
 * and the following disclaimer, must be included in all copies of the Software, in whole or in part, and all
 * derivative works of the Software, unless such copies or derivative works are solely in the form of
 * machine-executable object code generated by a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *********************************************************************************************************************/

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "SCU_Power_Down_Idle.h"
#include "IfxStm_Timer.h"

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
IfxStm_Timer g_myTimer;

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
/* Defines Priority 2 out of 255. Note: highest possible priority is 255 */
#define ISR_PRIORITY_2      2
/* Defines CPU that triggers the interrupt */
#define ISR_PROVIDER_CPU0   0
/* Rate 0.5 Hz */
#define RATE_0_5_HZ         0.5
/* LED */
#define LED_D110            &MODULE_P13,3

/*********************************************************************************************************************/
/*----------------------------------------------Function Implementations---------------------------------------------*/
/*********************************************************************************************************************/
/* Maps the Interrupt Service Routine with the CPU and Priority */
IFX_INTERRUPT(systemTimerIsrCmp0, ISR_PROVIDER_CPU0, ISR_PRIORITY_2);

/* Interrupt Service Routine to switch the CPU0 between idle and run mode */
void systemTimerIsrCmp0(void)
{
    static boolean setIdle = TRUE;                                      /* Alternating between Idle and non-Idle    */

    IfxCpu_enableInterrupts();                                          /* Enabling interrupts as ISR disables it   */
    IfxStm_Timer_acknowledgeTimerIrq(&g_myTimer);                       /* IR acknowledge and set new compare value */

    if(setIdle == TRUE)
    {
        IfxCpu_setCoreMode(&MODULE_CPU0, IfxCpu_CoreMode_idle);         /* Set CPU0 in IDLE mode                    */
        setIdle = FALSE;
    }
    else
    {
        setIdle = TRUE;
    }
}

/* Defines the routine to initialize and start the system timer */
void configSystemTimer(void)
{
    IfxStm_Timer_Config timerConfig;

    IfxStm_Timer_initConfig(&timerConfig, &MODULE_STM0);                /* Setup timerConfig with default values    */

    timerConfig.base.frequency = RATE_0_5_HZ;                           /* Interrupt rate 0.5 Hz or every 2s        */
    timerConfig.base.isrPriority = ISR_PRIORITY_2;                      /* Interrupt priority                       */
    timerConfig.base.isrProvider = (IfxSrc_Tos)ISR_PROVIDER_CPU0;       /* CPU0 to trigger the interrupt            */
    timerConfig.comparator = IfxStm_Comparator_0;                       /* Comparator 0 register is used            */

    IfxStm_Timer_init(&g_myTimer, &timerConfig);                        /* Use timerConfig to initialize the STM    */

    IfxStm_Timer_run(&g_myTimer);                                       /* Run STM and set Compare Value            */
}

/* Initialize the LED port pin */
void configLED(void)
{
    /* Set Port Pin 13.3 to output mode and turn off the LED (LED is low-level active) */
    IfxPort_setPinModeOutput(LED_D110, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
    IfxPort_setPinHigh(LED_D110);
}

/* Toggle the LED port pin state */
void toggleLED(void)
{
    IfxPort_togglePin(LED_D110);                                        /* Toggle Port Pin 13.3                     */
}

