/* $Id: tstMicro.cpp 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * Micro Testcase, profiling special CPU operations.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/vm.h>
#include <VBox/vmm.h>
#include <VBox/mm.h>
#include <VBox/cpum.h>
#include <VBox/pdm.h>
#include <VBox/dbgf.h>
#include <VBox/pgm.h>
#include <VBox/err.h>
#include <VBox/param.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/runtime.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>

#include "tstMicro.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define TESTCASE    "tstVMM"

static const char *GetDescription(TSTMICROTEST enmTest)
{
    switch (enmTest)
    {
        case TSTMICROTEST_OVERHEAD:         return "Overhead";
        case TSTMICROTEST_INVLPG_0:         return "invlpg [0]";
        case TSTMICROTEST_INVLPG_EIP:       return "invlpg [EIP]";
        case TSTMICROTEST_INVLPG_ESP:       return "invlpg [ESP]";
        case TSTMICROTEST_CR3_RELOAD:       return "cr3 reload";
        case TSTMICROTEST_WP_DISABLE:       return "CR0.WP <- 0";
        case TSTMICROTEST_WP_ENABLE:        return "CR0.WP <- 1";

        case TSTMICROTEST_PF_R0:            return "R0 #PG (NULL)";
        case TSTMICROTEST_PF_R1:            return "R1 #PG (NULL)";
        case TSTMICROTEST_PF_R2:            return "R2 #PG (NULL)";
        case TSTMICROTEST_PF_R3:            return "R3 #PG (NULL)";

        default:
        {
            static char sz[64];
            RTStrPrintf(sz, sizeof(sz), "%d?", enmTest);
            return sz;
        }
    }
}


static void PrintHeaderInstr(void)
{
    RTPrintf(TESTCASE ": %-25s  %10s  %10s  %10s\n",
             "Test name",
             "Min",
             "Avg",
             "Max");
}

static void PrintResultInstr(PTSTMICRO pTst, TSTMICROTEST enmTest, int rc, uint64_t cMinTicks, uint64_t cAvgTicks, uint64_t cMaxTicks)
{
    if (VBOX_FAILURE(rc))
        RTPrintf(TESTCASE ": %-25s  %10llu  %10llu  %10llu - %Vrc cr2=%x err=%x eip=%x!\n",
                 GetDescription(enmTest),
                 cMinTicks,
                 cAvgTicks,
                 cMaxTicks,
                 rc,
                 pTst->u32CR2,
                 pTst->u32ErrCd,
                 pTst->u32EIP);
    else
        RTPrintf(TESTCASE ": %-25s  %10llu  %10llu  %10llu\n",
                 GetDescription(enmTest),
                 cMinTicks,
                 cAvgTicks,
                 cMaxTicks);
}

static void PrintHeaderTraps(void)
{
    RTPrintf(TESTCASE ": %-25s  %10s  %10s  %10s  %10s  %10s\n",
             "Test name",
             "Total",
             "ToRx",
             "Trap",
             "ToRxTrap",
             "int42-done");
}

static void PrintResultTrap(PTSTMICRO pTst, TSTMICROTEST enmTest, int rc)
{
    if (VBOX_FAILURE(rc))
        RTPrintf(TESTCASE ": %-25s  %10llu  %10llu  %10llu  %10llu  %10llu - %Vrc cr2=%x err=%x eip=%x!\n",
                 GetDescription(enmTest),
                 pTst->aResults[enmTest].cTotalTicks,
                 pTst->aResults[enmTest].cToRxFirstTicks,
                 pTst->aResults[enmTest].cTrapTicks,
                 pTst->aResults[enmTest].cToRxTrapTicks,
                 pTst->aResults[enmTest].cToR0Ticks,
                 rc,
                 pTst->u32CR2,
                 pTst->u32ErrCd,
                 pTst->u32EIP);
    else
        RTPrintf(TESTCASE ": %-25s  %10llu  %10llu  %10llu  %10llu  %10llu\n",
                 GetDescription(enmTest),
                 pTst->aResults[enmTest].cTotalTicks,
                 pTst->aResults[enmTest].cToRxFirstTicks,
                 pTst->aResults[enmTest].cTrapTicks,
                 pTst->aResults[enmTest].cToRxTrapTicks,
                 pTst->aResults[enmTest].cToR0Ticks);
}


/**
 * 'Allocate' selectors for 32-bit code/data in rings 0-3.
 *
 * 0060 - r0 code
 * 0068 - r0 data
 *
 * 1060 - r1 code
 * 1068 - r1 data
 *
 * 2060 - r2 code
 * 2068 - r2 data
 *
 * 3060 - r3 code
 * 3068 - r3 data
 *
 */
static void SetupSelectors(PVM pVM)
{
    /*
     * Find the GDT - This is a HACK :-)
     */
    RTGCPTR     GCPtr = CPUMGetHyperGDTR(pVM, NULL);
    PX86DESC    paGDTEs = (PX86DESC)MMHyperGC2HC(pVM, GCPtr);

    for (unsigned i = 0; i <= 3; i++)
    {
        RTSEL Sel = (i << 12) + 0x60;

        /* 32-bit code selector. */
        PX86DESC pGDTE = &paGDTEs[Sel >> X86_SEL_SHIFT];
        pGDTE->au32[0] = pGDTE->au32[1] = 0;
        pGDTE->Gen.u16LimitLow  = ~0;
        pGDTE->Gen.u4LimitHigh  = ~0;
        pGDTE->Gen.u1Granularity= 1;
        pGDTE->Gen.u1Present    = 1;
        pGDTE->Gen.u2Dpl        = i;
        pGDTE->Gen.u1DefBig     = 1;
        pGDTE->Gen.u1DescType   = 1; /* !system */
        pGDTE->Gen.u4Type       = X86_SEL_TYPE_ER_ACC;

        /* 32-bit data selector. */
        pGDTE++;
        pGDTE->au32[0] = pGDTE->au32[1] = 0;
        pGDTE->Gen.u16LimitLow  = ~0;
        pGDTE->Gen.u4LimitHigh  = ~0;
        pGDTE->Gen.u1Granularity= 1;
        pGDTE->Gen.u1Present    = 1;
        pGDTE->Gen.u2Dpl        = i;
        pGDTE->Gen.u1DefBig     = 1;
        pGDTE->Gen.u1DescType   = 1; /* !system */
        pGDTE->Gen.u4Type       = X86_SEL_TYPE_RW_ACC;
    }
}


static DECLCALLBACK(int) doit(PVM pVM)
{
    RTPrintf(TESTCASE ": testing...\n");
    SetupSelectors(pVM);

    /*
     * Loading the module and resolve the entry point.
     */
    int rc = PDMR3LoadGC(pVM, NULL, "tstMicroGC.gc");
    if (VBOX_FAILURE(rc))
    {
        RTPrintf(TESTCASE ": Failed to load tstMicroGC.gc, rc=%Vra\n", rc);
        return rc;
    }
    RTGCPTR GCPtrEntry;
    rc = PDMR3GetSymbolGC(pVM, "tstMicroGC.gc", "tstMicroGC", &GCPtrEntry);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf(TESTCASE ": Failed to resolve the 'tstMicroGC' entry point in tstMicroGC.gc, rc=%Vra\n", rc);
        return rc;
    }
    RTGCPTR GCPtrStart;
    rc = PDMR3GetSymbolGC(pVM, "tstMicroGC.gc", "tstMicroGCAsmStart", &GCPtrStart);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf(TESTCASE ": Failed to resolve the 'tstMicroGCAsmStart' entry point in tstMicroGC.gc, rc=%Vra\n", rc);
        return rc;
    }
    RTGCPTR GCPtrEnd;
    rc = PDMR3GetSymbolGC(pVM, "tstMicroGC.gc", "tstMicroGCAsmEnd", &GCPtrEnd);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf(TESTCASE ": Failed to resolve the 'tstMicroGCAsmEnd' entry point in tstMicroGC.gc, rc=%Vra\n", rc);
        return rc;
    }

    /*
     * Allocate and initialize the instance data.
     */
    PTSTMICRO pTst;
    rc = MMHyperAlloc(pVM, RT_ALIGN_Z(sizeof(*pTst), PAGE_SIZE), PAGE_SIZE, MM_TAG_VM, (void **)&pTst);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf(TESTCASE ": Failed to resolve allocate instance memory (%d bytes), rc=%Vra\n", sizeof(*pTst), rc);
        return rc;
    }
    pTst->GCPtr = MMHyperHC2GC(pVM, pTst);
    pTst->GCPtrStack = MMHyperHC2GC(pVM, &pTst->au8Stack[sizeof(pTst->au8Stack) - 32]);

    /* the page must be writable from user mode */
    rc = PGMMapModifyPage(pVM, pTst->GCPtr, sizeof(*pTst), X86_PTE_US | X86_PTE_RW, ~(uint64_t)(X86_PTE_US | X86_PTE_RW));
    if (VBOX_FAILURE(rc))
    {
        RTPrintf(TESTCASE ": PGMMapModifyPage -> rc=%Vra\n", rc);
        return rc;
    }

    /* all the code must be executable from R3. */
    rc = PGMMapModifyPage(pVM, GCPtrStart, GCPtrEnd - GCPtrStart + PAGE_SIZE, X86_PTE_US, ~(uint64_t)X86_PTE_US);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf(TESTCASE ": PGMMapModifyPage -> rc=%Vra\n", rc);
        return rc;
    }
    PGMR3DumpHierarchyHC(pVM, PGMGetHyper32BitCR3(pVM), X86_CR4_PSE, false, 4, NULL);

#if 0
    /*
     * Disassemble the assembly...
     */
    RTGCPTR GCPtr = GCPtrStart;
    while (GCPtr < GCPtrEnd)
    {
        size_t  cb = 0;
        char    sz[256];
        int rc = DBGFR3DisasInstrEx(pVM, CPUMGetHyperCS(pVM), GCPtr, 0, sz, sizeof(sz), &cb);
        if (VBOX_SUCCESS(rc))
            RTLogPrintf("%s\n", sz);
        else
        {
            RTLogPrintf("%VGv rc=%Vrc\n", GCPtr, rc);
            cb = 1;
        }
        GCPtr += cb;
    }
#endif

    /*
     * Do the profiling.
     */
    /* execute the instruction profiling tests */
    PrintHeaderInstr();
    int i;
    for (i = TSTMICROTEST_OVERHEAD; i < TSTMICROTEST_TRAP_FIRST; i++)
    {
        TSTMICROTEST enmTest = (TSTMICROTEST)i;
        uint64_t    cMin = ~0;
        uint64_t    cMax = 0;
        uint64_t    cTotal = 0;
        unsigned    cSamples = 0;
        rc = VINF_SUCCESS;
        for (int c = 0; c < 100; c++)
        {
            int rc2 = VMMR3CallGC(pVM, GCPtrEntry, 2, pTst->GCPtr, enmTest);
            if (VBOX_SUCCESS(rc2))
            {
                uint64_t u64 = pTst->aResults[enmTest].cTotalTicks;
                if (cMin > u64)
                    cMin = u64;
                if (cMax < u64)
                    cMax = u64;
                cTotal += u64;
                cSamples++;
            }
            else if (VBOX_SUCCESS(rc))
                rc = rc2;
        }
        uint64_t cAvg = cTotal / (cSamples ? cSamples : 1);
        pTst->aResults[enmTest].cTotalTicks = cAvg;
        PrintResultInstr(pTst, enmTest, rc, cMin, cAvg, cMax);
        /* store the overhead */
        if (enmTest == TSTMICROTEST_OVERHEAD)
            pTst->u64Overhead = cMin;
    }


    /* execute the trap/cycle profiling tests. */
    RTPrintf("\n");
    PrintHeaderTraps();
    for (i = TSTMICROTEST_TRAP_FIRST; i < TSTMICROTEST_MAX; i++)
    {
        TSTMICROTEST enmTest = (TSTMICROTEST)i;
        rc = VMMR3CallGC(pVM, GCPtrEntry, 2, pTst->GCPtr, enmTest);
        PrintResultTrap(pTst, enmTest, rc);
    }


    RTPrintf(TESTCASE ": done!\n");
    return VINF_SUCCESS;
}


int main(int argc, char **argv)
{
    int     rcRet = 0;                  /* error count. */

    RTR3Init();

    /*
     * Create empty VM.
     */
    PVM pVM;
    int rc = VMR3Create(NULL, NULL, NULL, NULL, &pVM);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Do testing.
         */
        PVMREQ pReq1 = NULL;
        rc = VMR3ReqCallVoid(pVM, &pReq1, RT_INDEFINITE_WAIT, (PFNRT)doit, 1, pVM);
        AssertRC(rc);
        VMR3ReqFree(pReq1);

        STAMR3Dump(pVM, "*");

        /*
         * Cleanup.
         */
        rc = VMR3Destroy(pVM);
        if (!VBOX_SUCCESS(rc))
        {
            RTPrintf(TESTCASE ": error: failed to destroy vm! rc=%d\n", rc);
            rcRet++;
        }
    }
    else
    {
        RTPrintf(TESTCASE ": fatal error: failed to create vm! rc=%d\n", rc);
        rcRet++;
    }

    return rcRet;
}
