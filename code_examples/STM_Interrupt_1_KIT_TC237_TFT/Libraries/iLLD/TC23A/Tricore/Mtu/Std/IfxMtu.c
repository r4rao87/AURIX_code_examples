/**
 * \file IfxMtu.c
 * \brief MTU  basic functionality
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
 *
 */

/******************************************************************************/
/*----------------------------------Includes----------------------------------*/
/******************************************************************************/

#include "IfxMtu.h"

/** \addtogroup IfxLld_Mtu_Std_Utility
 * \{ */

/******************************************************************************/
/*-----------------------Private Function Prototypes--------------------------*/
/******************************************************************************/

/** \brief API to wait for requested tower depth.
 * \param towerDepth tower depth of MBIST Ram
 * \param numInstructions number of instructions
 * \param mbistSel Memory Selection
 * \return None
 */
IFX_STATIC void IfxMtu_waitForMbistDone(uint32 towerDepth, uint8 numInstructions, IfxMtu_MbistSel mbistSel);

/** \} */

/******************************************************************************/
/*-------------------------Function Implementations---------------------------*/
/******************************************************************************/

void IfxMtu_clearErrorTracking(IfxMtu_MbistSel mbistSel)
{
    Ifx_MC *mc = (Ifx_MC *)(IFXMTU_MC_ADDRESS_BASE + 0x100 * mbistSel);
    mc->ECCD.U |= (1 << IFX_MC_ECCD_TRC_OFF);
}


void IfxMtu_clearSram(IfxMtu_MbistSel mbistSel)
{
    uint8  isEndInitEnabled = 0;
    uint16 password         = 0;

    password = IfxScuWdt_getSafetyWatchdogPassword();

    /* Check if the Endinit is cleared by application. If not, then handle it internally inside teh function.*/
    if (IfxScuWdt_getSafetyWatchdogEndInit() == 1U)
    {
        /* Clear EndInit */
        IfxScuWdt_clearSafetyEndinit(password);
        isEndInitEnabled = 1;
    }

    IfxMtu_clearSramStart(mbistSel);

    /* Set EndInit Watchdog (to prevent Watchdog TO)*/
    IfxScuWdt_setSafetyEndinit(password);

    /* wait for the end of the fill operation */
    IfxMtu_waitForMbistDone(IfxMtu_sramTable[mbistSel].mbistDelay, 1, mbistSel);
    IFX_ASSERT(IFX_VERBOSE_LEVEL_ERROR, (((Ifx_MC *)(IFXMTU_MC_ADDRESS_BASE + 0x100 * mbistSel))->MSTATUS.B.DONE != 0));

    while (!IfxMtu_isMbistDone(mbistSel))
    {
        __nop();
    }

    /* Clear EndInit */
    IfxScuWdt_clearSafetyEndinit(password);

    IfxMtu_clearSramContinue(mbistSel);

    if (isEndInitEnabled == 1)
    {
        /* Set EndInit Watchdog (to prevent Watchdog TO)*/
        IfxScuWdt_setSafetyEndinit(password);
    }
}


void IfxMtu_clearSramContinue(IfxMtu_MbistSel mbistSel)
{
    /* Before clearing the ECC error flags we've to issue a dummy SRAM access to get a valid memory output */
    IfxMtu_readSramAddress(mbistSel, 0x0000);
    /* Note: a SMU alarm will be flagged HERE if the wrong ECC has been written! */
    IfxMtu_disableMbistShell(mbistSel);

    /* for auto-init memories: wait for the end of the clear operation */
    while (IfxMtu_isAutoInitRunning(mbistSel))
    {}
}


void IfxMtu_clearSramStart(IfxMtu_MbistSel mbistSel)
{
    Ifx_MC *mc = (Ifx_MC *)(IFXMTU_MC_ADDRESS_BASE + 0x100 * mbistSel);

    IfxMtu_enableMbistShell(mbistSel);

    /* for auto-init memories: wait for the end of the clear operation */
    while (IfxMtu_isAutoInitRunning(mbistSel))
    {}

    /* write valid ECC code for all-zero data into RDBFL registers */
    {
        IFX_ASSERT(IFX_VERBOSE_LEVEL_ERROR, mbistSel < IFXMTU_NUM_MBIST_TABLE_ITEMS);
        const IfxMtu_SramItem *item      = (IfxMtu_SramItem *)&IfxMtu_sramTable[mbistSel];

        uint8                  numBlocks = item->numBlocks;
        IFX_ASSERT(IFX_VERBOSE_LEVEL_ERROR, numBlocks > 0);

        uint16                 dataSize   = item->dataSize;
        uint8                  eccSize    = item->eccSize;
        uint32                 eccInvPos0 = dataSize + item->eccInvPos0;
        uint32                 eccInvPos1 = dataSize + item->eccInvPos1;

        uint32                 memSize    = dataSize + eccSize;

        uint32                 bitPos     = 0;
        uint32                 wordIx     = 0;
        uint16                 data       = 0;
        /* de-serialize data stream into 16bit packets */
        uint32                 mem;

        for (mem = 0; mem < numBlocks; ++mem)
        {
            uint32 i;

            for (i = 0; i < memSize; ++i)
            {
                if ((i == eccInvPos0) || (i == eccInvPos1))
                {
                    data |= (1 << bitPos);
                }

                ++bitPos;

                if (bitPos >= 16)
                {
                    mc->RDBFL[wordIx++].U = data;
                    bitPos                = 0;
                    data                  = 0;
                }
            }
        }

        /* final word? */
        if (bitPos != 0)
        {
            mc->RDBFL[wordIx].U = data;
        }
    }

    /* start fill operation */
    uint16 mcontrolMask = 0x4000;                                                                        /* set USERED flag */
    mc->MCONTROL.U = mcontrolMask | (1 << IFX_MC_MCONTROL_DINIT_OFF) | (1 << IFX_MC_MCONTROL_START_OFF); /* START = DINIT = 1 */
    mc->MCONTROL.U = mcontrolMask | (0 << IFX_MC_MCONTROL_DINIT_OFF) | (1 << IFX_MC_MCONTROL_DINIT_OFF); /* START = 0 */
}


void IfxMtu_enableErrorTracking(IfxMtu_MbistSel mbistSel, boolean enable)
{
    Ifx_MC *mc = (Ifx_MC *)(IFXMTU_MC_ADDRESS_BASE + 0x100 * mbistSel);

    if (enable == FALSE)
    {
        mc->ECCS.U &= ~(1 << IFX_MC_ECCS_TRE_OFF);
    }
    else
    {
        mc->ECCS.U |= (1 << IFX_MC_ECCS_TRE_OFF);
    }
}


uint32 IfxMtu_getSystemAddress(IfxMtu_MbistSel mbistSel, Ifx_MC_ETRR trackedSramAddress)
{
    uint32 sramAddress   = trackedSramAddress.B.ADDR;
    uint32 mbi           = trackedSramAddress.B.MBI;
    uint32 systemAddress = 0;

    switch (mbistSel)
    {
    case IfxMtu_MbistSel_cpu0Pspr:
        systemAddress = 0x70100000 | ((sramAddress << 3) | ((mbi & 1) << 2));
        break;

    case IfxMtu_MbistSel_cpu0Dspr:
        systemAddress = 0x70000000 | ((sramAddress << 4) | ((mbi & 3) << 2));
        break;

    case IfxMtu_MbistSel_lmu:
        systemAddress = 0xb0000000 | (sramAddress << 3);
        break;

    default:
        systemAddress = 0; /* unsupported address descrambling */
    }

    return systemAddress;
}


uint8 IfxMtu_getTrackedSramAddresses(IfxMtu_MbistSel mbistSel, Ifx_MC_ETRR *trackedSramAddresses)
{
    Ifx_MC *mc                  = (Ifx_MC *)(IFXMTU_MC_ADDRESS_BASE + 0x100 * mbistSel);
    uint8   validFlags          = (mc->ECCD.U >> IFX_MC_ECCD_VAL_OFF) & IFX_MC_ECCD_VAL_MSK;
    uint8   numTrackedAddresses = 0;
    int     i;

#if IFX_MC_ECCD_VAL_LEN > IFXMTU_MAX_TRACKED_ADDRESSES
# error "Unexpected size of VAL mask"
#endif

    for (i = 0; i < IFXMTU_MAX_TRACKED_ADDRESSES; ++i)
    {
        if (validFlags & (1 << i))
        {
            trackedSramAddresses[numTrackedAddresses].U = mc->ETRR[i].U;
            ++numTrackedAddresses;
        }
    }

    return numTrackedAddresses;
}


void IfxMtu_readSramAddress(IfxMtu_MbistSel mbistSel, uint16 sramAddress)
{
    Ifx_MC *mc = (Ifx_MC *)(IFXMTU_MC_ADDRESS_BASE + 0x100 * mbistSel);

    /* configure MBIST for single read opeation */
    uint16  mcontrolMask = 0x4000;                                                           /* set USERED flag */
    mc->MCONTROL.U = mcontrolMask | (1 << IFX_MC_MCONTROL_DIR_OFF);
    mc->CONFIG0.U  = (1 << IFX_MC_CONFIG0_NUMACCS_OFF) | (1 << IFX_MC_CONFIG0_ACCSTYPE_OFF); /* 1 read access */
    mc->CONFIG1.U  = 0;                                                                      /* ensure that linear scrambling is used */

    /* Set the address to be read (RAEN = 0) */
    mc->RANGE.U = sramAddress;

    /* Start operation */
    mc->MCONTROL.U = mcontrolMask | (1 << IFX_MC_MCONTROL_DIR_OFF) | (1 << IFX_MC_MCONTROL_START_OFF);
    mc->MCONTROL.U = mcontrolMask | (1 << IFX_MC_MCONTROL_DIR_OFF);

    /* wait for the end of the fill operation */
    IfxMtu_waitForMbistDone(256, 1, mbistSel);

    while (!IfxMtu_isMbistDone(mbistSel))
    {
        __nop();
    }
}


uint8 IfxMtu_runNonDestructiveInversionTest(IfxMtu_MbistSel mbistSel, uint8 rangeSel, uint8 rangeAddrUp, uint8 rangeAddrLow, uint16 *errorAddr)
{
    /* Select MBIST Memory Controller:
     * Ifx_MC is a type describing structure of MBIST Memory Controller
     * registers defined in IfxMc_regdef.h file - MC object */
    Ifx_MC *mc               = (Ifx_MC *)(IFXMTU_MC_ADDRESS_BASE + 0x100 * mbistSel);
    uint16  password         = 0;
    uint8   retVal           = 0U;
    uint8   isEndInitEnabled = 0;
    password = IfxScuWdt_getSafetyWatchdogPassword();

    /* Check if the Endinit is cleared by application. If not, then handle it internally inside teh function.*/
    if (IfxScuWdt_getSafetyWatchdogEndInit() == 1U)
    {
        /* Clear EndInit */
        IfxScuWdt_clearSafetyEndinit(password);
        isEndInitEnabled = 1;
    }

    /* Enable MBIST Memory Controller */
    IfxMtu_enableMbistShell(mbistSel);

    /* for auto-init memories: wait for the end of the clear operation */
    while (IfxMtu_isAutoInitRunning(mbistSel))
    {}

    /* Configure Non-destructive Inversion test */
    mc->CONFIG0.U = 0x4005; //NUMACCS=4, ACCSTYPE=5
    mc->CONFIG1.U = 0x5000; //AG_MOD=5
    /* Set the range register */
    mc->RANGE.U   = (rangeSel << 15) | (rangeAddrUp << 7) | (rangeAddrLow << 0);
    /* Run the tests */
    /* As per AP32917 and Errata MTU_TC.007 DIR is set to 0 */
    mc->MCONTROL.U       = 0x4001;
    mc->MCONTROL.B.START = 0;
    /* Set EndInit Watchdog (to prevent Watchdog TO)*/
    IfxScuWdt_setSafetyEndinit(password);

    /* wait for the end of the fill operation */
    IfxMtu_waitForMbistDone(IfxMtu_sramTable[mbistSel].mbistDelay, 4, mbistSel);
    IFX_ASSERT(IFX_VERBOSE_LEVEL_ERROR, mc->MSTATUS.B.DONE != 0);

    while (!IfxMtu_isMbistDone(mbistSel))
    {
        __nop();
    }

    /* Clear EndInit Again */
    IfxScuWdt_clearSafetyEndinit(password);

    /* Check the Fail Status */
    if (IfxMtu_checkErrorFlags(mbistSel))
    {
        /* Read the Error tracking register and return saying test failed */
        *errorAddr = mc->ETRR[0].U;
        retVal     = 1U;
    }

    /* Disable Memory Controller */
    IfxMtu_disableMbistShell(mbistSel);

    /* for auto-init memories: wait for the end of the clear operation */
    while (IfxMtu_isAutoInitRunning(mbistSel))
    {}

    /* Restore the endinit state */
    if (isEndInitEnabled == 1)
    {
        /* Set EndInit Watchdog (to prevent Watchdog TO)*/
        IfxScuWdt_setSafetyEndinit(password);
    }

    return retVal;
}


void IfxMtu_writeSramAddress(IfxMtu_MbistSel mbistSel, uint16 sramAddress)
{
    Ifx_MC *mc               = (Ifx_MC *)(IFXMTU_MC_ADDRESS_BASE + 0x100 * mbistSel);
    uint8   isEndInitEnabled = 0;
    uint16  password         = 0;
    password = IfxScuWdt_getSafetyWatchdogPassword();

    /* Check if the Endinit is cleared by application. If not, then handle it internally inside teh function.*/
    if (IfxScuWdt_getSafetyWatchdogEndInit() == 1U)
    {
        /* Clear EndInit */
        IfxScuWdt_clearSafetyEndinit(password);
        isEndInitEnabled = 1;
    }

    /* configure MBIST for single write opeation */
    uint16 mcontrolMask = 0x4000;                                                            /* set USERED flag */
    mc->MCONTROL.U = mcontrolMask | (1 << IFX_MC_MCONTROL_DIR_OFF);
    mc->CONFIG0.U  = (1 << IFX_MC_CONFIG0_NUMACCS_OFF) | (0 << IFX_MC_CONFIG0_ACCSTYPE_OFF); /* 1 write access */
    mc->CONFIG1.U  = 0;                                                                      /* ensure that linear scrambling is used */

    /* Set the address to be written (RAEN = 0) */
    mc->RANGE.U = sramAddress;

    /* Start operation */
    mc->MCONTROL.U = mcontrolMask | (1 << IFX_MC_MCONTROL_DIR_OFF) | (1 << IFX_MC_MCONTROL_START_OFF);
    mc->MCONTROL.U = mcontrolMask | (1 << IFX_MC_MCONTROL_DIR_OFF);

    if (isEndInitEnabled == 1)
    {
        /* Set EndInit Watchdog (to prevent Watchdog TO)*/
        IfxScuWdt_setSafetyEndinit(password);
    }

    /* Wait for the end of the operation */
    IfxMtu_waitForMbistDone(IfxMtu_sramTable[mbistSel].mbistDelay, 1, mbistSel);
    IFX_ASSERT(IFX_VERBOSE_LEVEL_ERROR, mc->MSTATUS.B.DONE != 0);

    while (!IfxMtu_isMbistDone(mbistSel))
    {
        __nop();
    }
}


IFX_STATIC void IfxMtu_waitForMbistDone(uint32 towerDepth, uint8 numInstructions, IfxMtu_MbistSel mbistSel)
{
    uint32          waitFact = (SCU_CCUCON0.B.SPBDIV / SCU_CCUCON0.B.SRIDIV) * numInstructions;
    volatile uint32 waitTime;

    switch (mbistSel)
    {
    case IfxMtu_MbistSel_ethermac:
        waitFact = waitFact * SCU_CCUCON1.B.GTMDIV;
        break;

    case IfxMtu_MbistSel_mcan:
    case IfxMtu_MbistSel_mcan1:

        break;

    case IfxMtu_MbistSel_erayObf:
    case IfxMtu_MbistSel_erayIbfTbf:
        waitFact = (IfxScuCcu_getSriFrequency() / IfxScuCcu_getPllErayFrequency()) * numInstructions;
        break;

    case IfxMtu_MbistSel_erayMbf:
        waitFact = (IfxScuCcu_getSriFrequency() / IfxScuCcu_getPllErayFrequency()) * numInstructions;
        break;

    case IfxMtu_MbistSel_emem0:
    case IfxMtu_MbistSel_emem1:
    case IfxMtu_MbistSel_emem2:
    case IfxMtu_MbistSel_emem3:
    case IfxMtu_MbistSel_emem4:
    case IfxMtu_MbistSel_emem5:
    case IfxMtu_MbistSel_emem6:
    case IfxMtu_MbistSel_emem7:
    case IfxMtu_MbistSel_ememXtm0:
    case IfxMtu_MbistSel_ememXtm1:
    case IfxMtu_MbistSel_fft0:
    case IfxMtu_MbistSel_fft1:
        waitFact = waitFact * SCU_CCUCON2.B.BBBDIV;
        break;
    default:
        break;
    }

    if (numInstructions == 4)
    {
        waitTime = (towerDepth * waitFact) + 30;
    }
    else
    {
        waitTime = ((towerDepth / 4) * waitFact) + 30;
    }

    waitTime = waitTime / 3;

    while (waitTime--)
    {
        __nop();
    }
}
