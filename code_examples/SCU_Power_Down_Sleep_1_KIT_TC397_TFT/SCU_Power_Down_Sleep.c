/**********************************************************************************************************************
 * \file SCU_Power_Down_Sleep.c
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
#include "SCU_Power_Down_Sleep.h"
#include "IfxGtm_Tom_Timer.h"
#include "IfxStm_Timer.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "Ifx_Types.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define ISR_PRIORITY_TOM    10                  /* TOM Interrupt priority                                           */
#define TOM_FREQ            25.0f               /* TOM frequency                                                    */
#define LED                 &MODULE_P13, 0      /* LED which is toggled in Interrupt Service Routine (ISR)          */

#define ISR_PRIORITY_STM    20                  /* STM Interrupt priority for interrupt ISR                         */
#define STM                 &MODULE_STM0        /* STM0 module is used in this example                              */

#define ALLOW_SLEEP_MODE    0x0                 /* Allow sleep mode for GTM                                         */
#define BLOCK_SLEEP_MODE    0x1                 /* Block sleep mode for STM                                         */
#define PMSWCR1_CPUSEL      0x1                 /* Set the CPU0 as CPU master                                       */
#define PMCSR0_REQSLP       0x2                 /* Request sleep mode                                               */

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
IfxGtm_Tom_Timer g_gtmTimer;                    /* TOM driver handle                                                */
IfxStm_Timer g_stmTimer;                        /* STM driver handle                                                */
float32 g_stmPeriod = 2.0;                      /* Period in seconds at which power modes toggle                    */
uint8 g_sleep = 0;                              /* Variable used to monitor the current set mode (sleep/run mode)   */

/*********************************************************************************************************************/
/*--------------------------------------------Function Implementations-----------------------------------------------*/
/*********************************************************************************************************************/
/* Macro to define the Interrupt Service Routine */
IFX_INTERRUPT(tomIsr, 0, ISR_PRIORITY_TOM);
IFX_INTERRUPT(stmIsr, 0, ISR_PRIORITY_STM);

/* TOM ISR to make the LED blink */
void tomIsr(void)
{
    /* Clear the timer event */
    IfxGtm_Tom_Timer_acknowledgeTimerIrq(&g_gtmTimer);

    /* Toggle the LED */
    IfxPort_togglePin(LED);
}

/* STM ISR to switch between run mode and sleep mode every two seconds */
void stmIsr(void)
{
    /* Enabling interrupts as ISR disables it */
    IfxCpu_enableInterrupts();

    /* Clear the timer event */
    IfxStm_Timer_acknowledgeTimerIrq(&g_stmTimer);

    /* Switch between run mode and sleep mode   */
    switchMode();
}

/* This function set the device either in run mode or in sleep mode */
void switchMode(void)
{
    if(g_sleep == 0)
    {
        /* Next interrupt --> run mode */
        g_sleep = 1;

        /* Clear safety EndInit protection */
        IfxScuWdt_clearSafetyEndinitInline(IfxScuWdt_getSafetyWatchdogPasswordInline());
        /* Clear EndInit protection */
        IfxScuWdt_clearCpuEndinit(IfxScuWdt_getCpuWatchdogPassword());

        GTM_CLC.B.EDIS = ALLOW_SLEEP_MODE;          /* Allow GTM to go into sleep mode                              */

        STM0_CLC.B.EDIS = BLOCK_SLEEP_MODE;         /* Prohibit STM to go into sleep mode                           */

        SCU_PMSWCR1.B.CPUSEL = PMSWCR1_CPUSEL;      /* Set the CPU0 as CPU master to trigger a power down mode      */

        SCU_PMCSR0.B.REQSLP = PMCSR0_REQSLP;        /* Request System Sleep Mode CPU0                               */

        /* Set safety EndInit protection */
        IfxScuWdt_setSafetyEndinitInline(IfxScuWdt_getSafetyWatchdogPasswordInline());
        /* Set EndInit protection */
        IfxScuWdt_setCpuEndinit(IfxScuWdt_getCpuWatchdogPassword());
    }
    else
    {
        /* Next interrupt --> sleep mode */
        g_sleep = 0;
    }
}

/* This function initialize the TOM to make an LED blink */
void initTom(void)
{
    IfxGtm_enable(&MODULE_GTM);                                     /* Enable the GTM                               */

    IfxGtm_Tom_Timer_Config timerConfig;                            /* Timer configuration structure                */
    IfxGtm_Tom_Timer_initConfig(&timerConfig, &MODULE_GTM);         /* Initialize the timer configuration           */

    timerConfig.base.frequency = TOM_FREQ;                          /* Set the timer frequency                      */
    timerConfig.base.isrPriority = ISR_PRIORITY_TOM;                /* Set the interrupt priority                   */
    timerConfig.base.isrProvider = IfxSrc_Tos_cpu0;                 /* Set the interrupt provider                   */
    timerConfig.tom = IfxGtm_Tom_1;                                 /* Define the used timer                        */
    timerConfig.timerChannel = IfxGtm_Tom_Ch_0;                     /* Define the used channel                      */
    timerConfig.clock = IfxGtm_Tom_Ch_ClkSrc_cmuFxclk3;             /* Define the used CMU clock                    */

    IfxGtm_Cmu_enableClocks(&MODULE_GTM, IFXGTM_CMU_CLKEN_FXCLK);   /* Enable the CMU clock                         */
    IfxGtm_Tom_Timer_init(&g_gtmTimer, &timerConfig);               /* Initialize the TOM                           */

    IfxPort_setPinModeOutput(LED, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general); /* Set port pin mode     */

    IfxGtm_Tom_Timer_run(&g_gtmTimer); /* Start the TOM */
}

/* This function initialize the STM to trigger an interrupt every two seconds */
void initStm(void)
{
    IfxStm_Timer_Config timerConfig;                                /* Timer configuration structure                */

    IfxStm_Timer_initConfig(&timerConfig, &MODULE_STM0);            /* Initialize it with default values            */

    timerConfig.base.frequency = 1 / g_stmPeriod;                   /* Interrupt rate every STM_PERIOD seconds      */
    timerConfig.base.isrPriority = ISR_PRIORITY_STM;                /* Interrupt priority                           */
    timerConfig.base.isrProvider = IfxSrc_Tos_cpu0;                 /* CPU0 to trigger the interrupt                */
    timerConfig.comparator = IfxStm_Comparator_0;                   /* Comparator 0 register is used                */

    IfxStm_Timer_init(&g_stmTimer, &timerConfig);                   /* Use timerConfig to initialize the STM        */

    IfxStm_Timer_run(&g_stmTimer);                                  /* Run the STM and set the compare Value        */
}
