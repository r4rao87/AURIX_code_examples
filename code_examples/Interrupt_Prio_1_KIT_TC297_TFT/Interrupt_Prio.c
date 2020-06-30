/**********************************************************************************************************************
 * \file Interrupt_Prio.c
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
#include "Interrupt_Prio.h"
#include "Ifx_Types.h"
#include "IfxGpt12.h"
#include "Bsp.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define ISR_PRIORITY_GPT12_T2_INT       52                          /* Define the GPT12 Timer 2 interrupt priority  */
#define ISR_PRIORITY_GPT12_T3_INT       51                          /* Define the GPT12 Timer 3 interrupt priority  */
#define ISR_PRIORITY_GPT12_T4_INT       50                          /* Define the GPT12 Timer 4 interrupt priority  */

#define LED_D110                        &MODULE_P13,3               /* LED D110 Port, Pin definition                */
#define LED_D109                        &MODULE_P13,2               /* LED D109 Port, Pin definition                */
#define LED_D108                        &MODULE_P13,1               /* LED D108 Port, Pin definition                */

#define LED_ON_TIME                     600                         /* LED are switched on for 600ms                */

/*********************************************************************************************************************/
/*---------------------------------------------Function Prototypes --------------------------------------------------*/
/*********************************************************************************************************************/
static void wait_ms(uint32 ms);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
/* Macro to define Interrupt Service Routine. */
IFX_INTERRUPT(GPT12_T4_Int0_Handler, 0, ISR_PRIORITY_GPT12_T4_INT);

/* Interrupt Service Routine of timer T4, gets triggered after timer overflow */
void GPT12_T4_Int0_Handler(void)
{
    IfxPort_setPinLow(LED_D110);    /* LED on                                                                       */
    IfxCpu_enableInterrupts();      /* Allows the interruptions of this function by higher prioritized interrupts   */

    /* Wait 600ms
     * This will not be the case since this ISR will be interrupted by two higher priority timers (T3 and T2)
     */
    wait_ms(LED_ON_TIME);

    IfxPort_setPinHigh(LED_D110);   /* LED off                                                                      */
}

/* Macro to define Interrupt Service Routine. */
IFX_INTERRUPT(GPT12_T3_Int0_Handler, 0, ISR_PRIORITY_GPT12_T3_INT);

/* Interrupt Service Routine of timer T3, gets triggered after timer overflow */
void GPT12_T3_Int0_Handler(void)
{
    IfxCpu_enableInterrupts();      /* Allows the interruption of this function by higher prioritized interrupts    */
    IfxPort_setPinLow(LED_D109);    /* LED on                                                                       */

    /* Wait 600ms
     * This will not be the case since this ISR will be interrupted by one higher priority timer (T2)
     */
    wait_ms(LED_ON_TIME);
    IfxPort_setPinHigh(LED_D109);   /* LED off                                                                      */
}

/* Macro to define Interrupt Service Routine. */
IFX_INTERRUPT(GPT12_T2_Int0_Handler, 0, ISR_PRIORITY_GPT12_T2_INT);

/* Interrupt Service Routine of timer T2, gets triggered after timer overflow */
void GPT12_T2_Int0_Handler(void)
{
    IfxCpu_enableInterrupts();      /* Allows the interruptions of this function by higher prioritized interrupts   */
    IfxPort_setPinLow(LED_D108);    /* LED on                                                                       */

    /* Wait 600ms */
    wait_ms(LED_ON_TIME);

    IfxPort_setPinHigh(LED_D108);   /* LED off                                                                      */
}

/* This function initializes the three LEDs D110 D109 D108 */
void init_LEDs(void)
{
    /* Set Ports 13.3 - 13.2 - 13.1 to output mode */
    IfxPort_setPinModeOutput(LED_D110, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
    IfxPort_setPinModeOutput(LED_D109, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
    IfxPort_setPinModeOutput(LED_D108, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);

    /* Switch off the LEDs*/
    IfxPort_setPinHigh(LED_D110);   /* Related to T4 ISR                                                            */
    IfxPort_setPinHigh(LED_D109);   /* Related to T3 ISR                                                            */
    IfxPort_setPinHigh(LED_D108);   /* Related to T2 ISR                                                            */
}

/* This function initializes timers T2, T3 and T4 of the block GPT1 of module GPT12 */
void init_GPT12_module(void)
{
    /* Enable GPT12 module */
    IfxGpt12_enableModule(&MODULE_GPT120);
    /* Select 32 as prescaler for the GPT1 module -> slowest clock frequency */
    IfxGpt12_setGpt1BlockPrescaler(&MODULE_GPT120, IfxGpt12_Gpt1BlockPrescaler_32);

    /* Set timers T2, T3 and T4 in timer mode */
    IfxGpt12_T2_setMode(&MODULE_GPT120, IfxGpt12_Mode_timer);
    IfxGpt12_T3_setMode(&MODULE_GPT120, IfxGpt12_Mode_timer);
    IfxGpt12_T4_setMode(&MODULE_GPT120, IfxGpt12_Mode_timer);
    /* Select 128 as prescaler for T2, T3, T4 to slow down the clock of GPT1 */
    IfxGpt12_T2_setTimerPrescaler(&MODULE_GPT120, IfxGpt12_TimerInputPrescaler_128);
    IfxGpt12_T3_setTimerPrescaler(&MODULE_GPT120, IfxGpt12_TimerInputPrescaler_128);
    IfxGpt12_T4_setTimerPrescaler(&MODULE_GPT120, IfxGpt12_TimerInputPrescaler_128);

    /* Service request configuration */
    /* Get source pointer of timer T2, initialize and enable */
    volatile Ifx_SRC_SRCR *src = IfxGpt12_T2_getSrc(&MODULE_GPT120);
    IfxSrc_init(src, IfxSrc_Tos_cpu0, ISR_PRIORITY_GPT12_T2_INT);                       /* Highest priority         */
    IfxSrc_enable(src);
    /* Get source pointer of timer T3, initialize and enable */
    src = IfxGpt12_T3_getSrc(&MODULE_GPT120);
    IfxSrc_init(src, IfxSrc_Tos_cpu0, ISR_PRIORITY_GPT12_T3_INT);                       /* Intermediate priority    */
    IfxSrc_enable(src);
    /* Get source pointer of timer T4, initialize and enable */
    src = IfxGpt12_T4_getSrc(&MODULE_GPT120);
    IfxSrc_init(src, IfxSrc_Tos_cpu0, ISR_PRIORITY_GPT12_T4_INT);                       /* Lowest priority          */
    IfxSrc_enable(src);

    /* Start timer T4 */
    IfxGpt12_T4_run(&MODULE_GPT120, IfxGpt12_TimerRun_start);
    /* Add a delay to ensure that lowest priority ISR is executed first */
    wait_ms(100);

    /* Start timers T2 & T3 */
    IfxGpt12_T2_run(&MODULE_GPT120, IfxGpt12_TimerRun_start);
    /* Add a delay to ensure it does not start before T2  */
    wait_ms(5);
    IfxGpt12_T3_run(&MODULE_GPT120, IfxGpt12_TimerRun_start);

}

/* Waits until a timeout in milliseconds */
void wait_ms(uint32 ms)
{
    sint32 Fsys = IfxStm_getFrequency(&MODULE_STM0);
    Ifx_TickTime waitms = (Fsys / (1000 / ms));

    wait(waitms);
}
