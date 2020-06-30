/**
 * \file Ifx_Ssw_Tc5.c
 * \brief Startup Software for Core5
 *
 * \version iLLD_1_0_1_11_0
 * \copyright Copyright (c) 2018 Infineon Technologies AG. All rights reserved.
 *
 *
 *                                 IMPORTANT NOTICE
 *
 *
 * Use of this file is subject to the terms of use agreed between (i) you or 
 * the company in which ordinary course of business you are acting and (ii) 
 * Infineon Technologies AG or its licensees. If and as long as no such 
 * terms of use are agreed, use of this file is subject to following:


 * Boost Software License - Version 1.0 - August 17th, 2003

 * Permission is hereby granted, free of charge, to any person or 
 * organization obtaining a copy of the software and accompanying 
 * documentation covered by this license (the "Software") to use, reproduce,
 * display, distribute, execute, and transmit the Software, and to prepare
 * derivative works of the Software, and to permit third-parties to whom the 
 * Software is furnished to do so, all subject to the following:

 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer, must
 * be included in all copies of the Software, in whole or in part, and all
 * derivative works of the Software, unless such copies or derivative works are
 * solely in the form of machine-executable object code generated by a source
 * language processor.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE 
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.

 *
 */

/*******************************************************************************
**                      Includes                                              **
*******************************************************************************/
#include "Ifx_Cfg.h"
#include "Ifx_Ssw.h"
#include "Ifx_Ssw_Infra.h"

#include "IfxScu_reg.h"
#include "IfxStm_reg.h"

/******************************************************************************/
/*                          Macros                                            */
/******************************************************************************/
/** \brief Configuration for cache enable.
 *
 */
#ifndef IFX_CFG_SSW_ENABLE_TRICORE5_PCACHE
#   define IFX_CFG_SSW_ENABLE_TRICORE5_PCACHE (1U)  /**< Program Cache enabled by default*/
#endif
#ifndef IFX_CFG_SSW_ENABLE_TRICORE5_DCACHE
#   define IFX_CFG_SSW_ENABLE_TRICORE5_DCACHE (1U)  /**< Data Cache enabled by default*/
#endif
/*******************************************************************************
**                      Imported Function Declarations                        **
*******************************************************************************/
IFX_SSW_COMMON_LINKER_SYMBOLS();
IFX_SSW_CORE_LINKER_SYMBOLS(5);

extern void core5_main(void);
#if defined(__TASKING__)
__asm("\t .extern core5_main");
#endif

/*******************************************************************************
**                      Private Constant Definitions                          **
*******************************************************************************/

/*********************************************************************************
* - startup code
*********************************************************************************/
/*Add options to eliminate usage of stack pointers unnecessarily*/
#if defined(__HIGHTEC__)
#pragma GCC optimize "O2"
#endif

void __Core5_start(void)
{
    unsigned int   stmCount;
    unsigned int   stmCountBegin = STM0_TIM0.U;     /* it is necessary to get this value to have minimum 100uS delay in subsequent CPU start */
    unsigned short wdtPassword   = Ifx_Ssw_getCpuWatchdogPasswordInline(&MODULE_SCU.WDTCPU[5]);

    /* Load user stack pointer */
    Ifx_Ssw_setAddressReg(sp, __USTACK(5));
    Ifx_Ssw_DSYNC();

    /* Set the PSW to its reset value in case of a warm start,clear PSW.IS */
    Ifx_Ssw_MTCR(CPU_PSW, IFX_CFG_SSW_PSW_DEFAULT);

    /* Clear the ENDINIT bit in the WDT_CON0 register, inline function */
    Ifx_Ssw_clearCpuEndinitInline(&MODULE_SCU.WDTCPU[5], wdtPassword);

    {
        Ifx_CPU_PCON0 pcon0;
        pcon0.U       = 0;
        pcon0.B.PCBYP = IFX_CFG_SSW_ENABLE_TRICORE5_PCACHE ? 0 : 1; /*depending on the enable bypass bit is reset/set */
        Ifx_Ssw_MTCR(CPU_PCON0, pcon0.U);
        Ifx_Ssw_ISYNC();
    }

    {
        Ifx_CPU_DCON0 dcon0;
        dcon0.U       = 0;
        dcon0.B.DCBYP = IFX_CFG_SSW_ENABLE_TRICORE5_DCACHE ? 0 : 1; /*depending on the enable bypass bit is reset/set */
        Ifx_Ssw_MTCR(CPU_DCON0, dcon0.U);
        Ifx_Ssw_ISYNC();
    }

    /* Load Base Address of Trap Vector Table. */
    Ifx_Ssw_MTCR(CPU_BTV, (unsigned int)__TRAPTAB(5));

    /* Load Base Address of Interrupt Vector Table. we will do this later in the program */
    Ifx_Ssw_MTCR(CPU_BIV, (unsigned int)__INTTAB(5));

    /* Load interrupt stack pointer. */
    Ifx_Ssw_MTCR(CPU_ISP, (unsigned int)__ISTACK(5));

    Ifx_Ssw_setCpuEndinitInline(&MODULE_SCU.WDTCPU[5], wdtPassword);

    /* initialize SDA base pointers */
    Ifx_Ssw_setAddressReg(a0, __SDATA1(5));
    Ifx_Ssw_setAddressReg(a1, __SDATA2(5));
    Ifx_Ssw_setAddressReg(a8, __SDATA3(5));
    Ifx_Ssw_setAddressReg(a9, __SDATA4(5));

    Ifx_Ssw_initCSA((unsigned int *)__CSA(5), (unsigned int *)__CSA_END(5));

    /* Clears any instruction buffer */
    Ifx_Ssw_ISYNC();

    stmCount = (unsigned int)(Ifx_Ssw_getStmFrequency() * IFX_CFG_SSW_STARTCPU_WAIT_TIME_IN_SECONDS);

    while ((unsigned int)(STM0_TIM0.U - stmCountBegin) < stmCount)
    {
        /* There is no need to check overflow of the STM timer.
         * When counter after overflow subtracted with counter before overflow,
         * the subtraction result will be as expected, as long as both are unsigned 32 bits
         * eg: stmCountBegin= 0xFFFFFFFE (before overflow)
         *     stmCountNow = 0x00000002 (before overflow)
         *     diff= stmCountNow - stmCountBegin = 4 as expected.*/
    }

    /*Initialize CPU Private Global Variables*/
    //TODO : This implementation is done once all compilers support this
#if (IFX_CFG_SSW_ENABLE_INDIVIDUAL_C_INIT != 0)
    (void)Ifx_Ssw_C_Init();
#endif

    /*Call main function of Cpu0 */
    Ifx_Ssw_jumpToFunction(core5_main);
}

/******************************************************************************
 *                        reset vector address                                *
 *****************************************************************************/
#if defined(__HIGHTEC__)
#pragma section
#pragma section ".start_cpu5" x
#endif
#if defined(__TASKING__)
#pragma protect on
#pragma section code "start_cpu5"
#endif
#if defined(__DCC__)
#pragma section CODE ".start_cpu5" X
#endif
#if defined(__ghs__)
#pragma ghs section text=".start_cpu5"
#endif

void _START5(void)
{
    Ifx_Ssw_jumpToFunction(__Core5_start);
}

/* reset the sections defined above, to normal region */
#if defined(__HIGHTEC__)
#pragma section
#endif
#if defined(__TASKING__)
#pragma protect restore
#pragma section code restore
#endif
#if defined(__DCC__)
#pragma section CODE
#endif
#if defined(__ghs__)
#pragma ghs section text=default
#endif

/*Restore the options to command line provided ones*/
#if defined(__HIGHTEC__)
#pragma GCC reset_options
#endif
