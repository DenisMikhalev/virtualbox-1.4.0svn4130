/* $Id: tstMicro.h 4071 2007-08-07 17:07:59Z vboxsync $ */
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

#ifndef ___tstMicro_h
#define ___tstMicro_h

/**
 * The testcase identifier.
 */
typedef enum TSTMICROTEST
{
    TSTMICROTEST_OVERHEAD = 0,
    TSTMICROTEST_INVLPG_0,
    TSTMICROTEST_INVLPG_EIP,
    TSTMICROTEST_INVLPG_ESP,
    TSTMICROTEST_CR3_RELOAD,
    TSTMICROTEST_WP_DISABLE,
    TSTMICROTEST_WP_ENABLE,

    TSTMICROTEST_TRAP_FIRST,
    TSTMICROTEST_PF_R0 = TSTMICROTEST_TRAP_FIRST,
    TSTMICROTEST_PF_R1,
    TSTMICROTEST_PF_R2,
    TSTMICROTEST_PF_R3,

    /** The max testcase. */
    TSTMICROTEST_MAX
} TSTMICROTEST;


/**
 *
 */
typedef struct TSTMICRORESULT
{
    /** The total number of ticks spent executing the testcase.
     * This may include extra overhead stuff if we're weird stuff during trap handler. */
    uint64_t    cTotalTicks;
    /** Number of ticks spent getting into Rx from R0.
     * This will include time spent setting up the testcase in R3. */
    uint64_t    cToRxFirstTicks;
    /** Number of ticks spent executing the trap.
     * I.e. from right before trapping instruction to the start of  the trap handler.
     * This does not apply to testcases which doesn't trap. */
    uint64_t    cTrapTicks;
    /** Number of ticks spent resuming Rx executing after a trap.
    * This does not apply to testcases which doesn't trap. */
    uint64_t    cToRxTrapTicks;
    /** Number of ticks to get to back to r0 after resuming the trapped code.
     * This does not apply to testcases which doesn't trap. */
    uint64_t    cToR0Ticks;
} TSTMICRORESULT, *PTSTMICRORESULT;

/**
 * Micro profiling testcase
 */
typedef struct TSTMICRO
{
    /** The GC address of this structure. */
    RTGCPTR     GCPtr;
    /** Just for proper alignment. */
    RTGCPTR     GCPtrStack;

    /** TSC sampled right before leaving R0. */
    uint64_t    u64TSCR0Start;
    /** TSC sampled right before the exception. */
    uint64_t    u64TSCRxStart;
    /** TSC sampled right after entering the trap handler. */
    uint64_t    u64TSCR0Enter;
    /** TSC sampled right before exitting the trap handler. */
    uint64_t    u64TSCR0Exit;
    /** TSC sampled right after resuming guest trap. */
    uint64_t    u64TSCRxEnd;
    /** TSC sampled right after re-entering R0. */
    uint64_t    u64TSCR0End;
    /** Number of times entered (should be one). */
    uint32_t    cHits;
    /** Advance EIP. */
    int32_t     offEIPAdd;
    /** The last CR3 code. */
    uint32_t    u32CR2;
    /** The last error code. */
    uint32_t    u32ErrCd;
    /** The last trap eip. */
    uint32_t    u32EIP;
    /** The original IDT address and limit. */
    VBOXIDTR    OriginalIDTR;
    /** Our IDT. */
    VBOXIDTE    aIDT[256];

    /** The overhead for the rdtsc + 2 xchg instr. */
    uint64_t    u64Overhead;

    /** The testresults. */
    TSTMICRORESULT  aResults[TSTMICROTEST_MAX];
    /** Ring-3 stack. */
    uint8_t     au8Stack[4096];

} TSTMICRO, *PTSTMICRO;


__BEGIN_DECLS

DECLASM(void) idtOnly42(PTSTMICRO pTst);


DECLASM(void) tstOverhead(PTSTMICRO pTst);
DECLASM(void) tstInvlpg0(PTSTMICRO pTst);
DECLASM(void) tstInvlpgEIP(PTSTMICRO pTst);
DECLASM(void) tstInvlpgESP(PTSTMICRO pTst);
DECLASM(void) tstCR3Reload(PTSTMICRO pTst);
DECLASM(void) tstWPEnable(PTSTMICRO pTst);
DECLASM(void) tstWPDisable(PTSTMICRO pTst);


DECLASM(int)  tstPFR0(PTSTMICRO pTst);
DECLASM(int)  tstPFR1(PTSTMICRO pTst);
DECLASM(int)  tstPFR2(PTSTMICRO pTst);
DECLASM(int)  tstPFR3(PTSTMICRO pTst);



DECLASM(void) tstTrapHandlerNoErr(void);
DECLASM(void) tstTrapHandler(void);
DECLASM(void) tstInterrupt42(void);

__END_DECLS

#endif
