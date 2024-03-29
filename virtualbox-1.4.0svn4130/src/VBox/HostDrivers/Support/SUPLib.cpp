/* $Id: SUPLib.cpp 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * VirtualBox Support Library - Common code.
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

/** @page   pg_sup          SUP - The Support Library
 *
 * The support library is responsible for providing facilities to load
 * VMM Host Ring-0 code, to call Host VMM Ring-0 code from Ring-3 Host
 * code, to pin down physical memory, and more.
 *
 * The VMM Host Ring-0 code can be combined in the support driver if
 * permitted by kernel module license policies. If it is not combined
 * it will be externalized in a .r0 module that will be loaded using
 * the IPRT loader.
 *
 * The Ring-0 calling is done thru a generic SUP interface which will
 * tranfer an argument set and call a predefined entry point in the Host
 * VMM Ring-0 code.
 *
 * See @ref grp_sup "SUP - Support APIs" for API details.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_SUP
#include <VBox/sup.h>
#include <VBox/err.h>
#include <VBox/param.h>
#ifdef VBOX_WITHOUT_IDT_PATCHING
# include <VBox/vmm.h>
#endif
#include <VBox/log.h>

#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/alloca.h>
#include <iprt/ldr.h>
#include <iprt/asm.h>
#include <iprt/system.h>
#include <iprt/thread.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/env.h>

#include "SUPLibInternal.h"
#include "SUPDRVIOC.h"



/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** R0 VMM module name. */
#define VMMR0_NAME      "VMMR0"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef DECLCALLBACK(int) FNCALLVMMR0(PVMR0 pVMR0, unsigned uOperation, void *pvArg);
typedef FNCALLVMMR0 *PFNCALLVMMR0;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Pointer to the Global Information Page.
 *
 * This pointer is valid as long as SUPLib has a open session. Anyone using
 * the page must treat this pointer as higly volatile and not trust it beyond
 * one transaction.
 *
 * @todo This will probably deserve it's own session or some other good solution...
 */
DECLEXPORT(PSUPGLOBALINFOPAGE)  g_pSUPGlobalInfoPage;
/** Address of the ring-0 mapping of the GIP. */
static PSUPGLOBALINFOPAGE       g_pSUPGlobalInfoPageR0;
/** The physical address of the GIP. */
static RTHCPHYS                 g_HCPhysSUPGlobalInfoPage = NIL_RTHCPHYS;

/** The negotiated cookie. */
uint32_t            g_u32Cookie = 0;
/** The negotiated session cookie. */
uint32_t            g_u32SessionCookie;
/** Session handle. */
PSUPDRVSESSION      g_pSession;
/** R0 SUP Functions used for resolving referenced to the SUPR0 module. */
static PSUPQUERYFUNCS_OUT g_pFunctions;

#ifndef VBOX_WITHOUT_IDT_PATCHING
/** The negotiated interrupt number. */
static uint8_t      g_u8Interrupt = 3;
/** Pointer to the generated code fore calling VMMR0. */
static PFNCALLVMMR0 g_pfnCallVMMR0;
#endif
/** VMMR0 Load Address. */
static RTR0PTR      g_pvVMMR0 = NIL_RTR0PTR;
/** Init counter. */
static unsigned     g_cInits = 0;
/** Fake mode indicator. (~0 at first, 0 or 1 after first test) */
static uint32_t     g_u32FakeMode = ~0;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int supInitFake(PSUPDRVSESSION *ppSession);
static int supLoadModule(const char *pszFilename, const char *pszModule, void **ppvImageBase);
#ifndef VBOX_WITHOUT_IDT_PATCHING
static int supInstallIDTE(void);
#endif
static DECLCALLBACK(int) supLoadModuleResolveImport(RTLDRMOD hLdrMod, const char *pszModule, const char *pszSymbol, unsigned uSymbol, RTUINTPTR *pValue, void *pvUser);


SUPR3DECL(int) SUPInstall(void)
{
    return suplibOsInstall();
}


SUPR3DECL(int) SUPUninstall(void)
{
    return suplibOsUninstall();
}


SUPR3DECL(int) SUPInit(PSUPDRVSESSION *ppSession /* NULL */, size_t cbReserve /* 0 */)
{
    /*
     * Perform some sanity checks.
     * (Got some trouble with compile time member alignment assertions.)
     */
    Assert(!(RT_OFFSETOF(SUPGLOBALINFOPAGE, u64NanoTSLastUpdateHz) & 0x7));
    Assert(!(RT_OFFSETOF(SUPGLOBALINFOPAGE, aCPUs) & 0x1f));
    Assert(!(RT_OFFSETOF(SUPGLOBALINFOPAGE, aCPUs[1]) & 0x1f));
    Assert(!(RT_OFFSETOF(SUPGLOBALINFOPAGE, aCPUs[0].u64NanoTS) & 0x7));
    Assert(!(RT_OFFSETOF(SUPGLOBALINFOPAGE, aCPUs[0].u64TSC) & 0x7));
    Assert(!(RT_OFFSETOF(SUPGLOBALINFOPAGE, aCPUs[0].u64CpuHz) & 0x7));

    /*
     * Check if already initialized.
     */
    if (ppSession)
        *ppSession = g_pSession;
    if (g_cInits++ > 0)
        return VINF_SUCCESS;

    /*
     * Check for fake mode.
     *
     * Fake mode is used when we're doing smoke testing and debugging.
     * It's also useful on platforms where we haven't root access or which
     * we haven't ported the support driver to.
     */
    if (g_u32FakeMode == ~0U)
    {
        const char *psz = RTEnvGet("VBOX_SUPLIB_FAKE");
        if (psz && !strcmp(psz, "fake"))
            ASMAtomicCmpXchgU32(&g_u32FakeMode, 1, ~0U);
        else
            ASMAtomicCmpXchgU32(&g_u32FakeMode, 0, ~0U);
    }
    if (RT_UNLIKELY(g_u32FakeMode))
        return supInitFake(ppSession);

    /**
     * Open the support driver.
     */
    int rc = suplibOsInit(cbReserve);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Negotiate the cookie.
         */
        SUPCOOKIE_IN    In;
        SUPCOOKIE_OUT   Out = {0,0,0,0,0,NIL_RTR0PTR};
        strcpy(In.szMagic, SUPCOOKIE_MAGIC);
        In.u32ReqVersion = SUPDRVIOC_VERSION;
        In.u32MinVersion = SUPDRVIOC_VERSION & 0xffff0000;
        rc = suplibOsIOCtl(SUP_IOCTL_COOKIE, &In, sizeof(In), &Out, sizeof(Out));
        if (VBOX_SUCCESS(rc))
        {
            if ((Out.u32SessionVersion & 0xffff0000) == (SUPDRVIOC_VERSION & 0xffff0000))
            {
                /*
                 * Query the functions.
                 */
                SUPQUERYFUNCS_IN    FuncsIn;
                FuncsIn.u32Cookie           = Out.u32Cookie;
                FuncsIn.u32SessionCookie    = Out.u32SessionCookie;
                unsigned            cbFuncsOut = RT_OFFSETOF(SUPQUERYFUNCS_OUT, aFunctions[Out.cFunctions]);
                PSUPQUERYFUNCS_OUT  pFuncsOut = (PSUPQUERYFUNCS_OUT)RTMemAllocZ(cbFuncsOut);
                if (pFuncsOut)
                {
                    rc = suplibOsIOCtl(SUP_IOCTL_QUERY_FUNCS, &FuncsIn, sizeof(FuncsIn), pFuncsOut, cbFuncsOut);
                    if (VBOX_SUCCESS(rc))
                    {
                        g_u32Cookie         = Out.u32Cookie;
                        g_u32SessionCookie  = Out.u32SessionCookie;
                        g_pSession          = Out.pSession;
                        g_pFunctions        = pFuncsOut;
                        if (ppSession)
                            *ppSession = Out.pSession;

                        /*
                         * Map the GIP into userspace.
                         * This is an optional feature, so we will ignore any failures here.
                         */
                        if (!g_pSUPGlobalInfoPage)
                        {
                            SUPGIPMAP_IN GipIn = {0};
                            SUPGIPMAP_OUT GipOut = {NULL, 0};
                            GipIn.u32Cookie           = Out.u32Cookie;
                            GipIn.u32SessionCookie    = Out.u32SessionCookie;
                            rc = suplibOsIOCtl(SUP_IOCTL_GIP_MAP, &GipIn, sizeof(GipIn), &GipOut, sizeof(GipOut));
                            if (VBOX_SUCCESS(rc))
                            {
                                AssertRelease(GipOut.pGipR3->u32Magic == SUPGLOBALINFOPAGE_MAGIC);
                                AssertRelease(GipOut.pGipR3->u32Version >= SUPGLOBALINFOPAGE_VERSION);
                                ASMAtomicXchgSize(&g_HCPhysSUPGlobalInfoPage, GipOut.HCPhysGip);
                                ASMAtomicCmpXchgPtr((void * volatile *)&g_pSUPGlobalInfoPage, (void *)GipOut.pGipR3, NULL);
                                ASMAtomicCmpXchgPtr((void * volatile *)&g_pSUPGlobalInfoPageR0, (void *)GipOut.pGipR0, NULL);
                            }
                            else
                                rc = VINF_SUCCESS;
                        }
                        return rc;
                    }
                    RTMemFree(pFuncsOut);
                }
                else
                    rc = VERR_NO_MEMORY;
            }
            else
            {
                LogRel(("Support driver version mismatch: SessionVersion=%#x DriverVersion=%#x ClientVersion=%#x\n",
                        Out.u32SessionVersion, Out.u32DriverVersion, SUPDRVIOC_VERSION));
                rc = VERR_VM_DRIVER_VERSION_MISMATCH;
            }
        }
        else
        {
             if (rc == VERR_INVALID_PARAMETER) /* for pre 0x00040002 drivers */
                 rc = VERR_VM_DRIVER_VERSION_MISMATCH;
             if (rc == VERR_VM_DRIVER_VERSION_MISMATCH)
                 LogRel(("Support driver version mismatch: DriverVersion=%#x ClientVersion=%#x\n",
                         Out.u32DriverVersion, SUPDRVIOC_VERSION));
             else
                 LogRel(("Support driver version/Cookie negotiations error: rc=%Vrc\n", rc));
        }

        suplibOsTerm();
    }
    AssertMsgFailed(("SUPInit() failed rc=%Vrc\n", rc));
    g_cInits--;

    return rc;
}

/**
 * Fake mode init.
 */
static int supInitFake(PSUPDRVSESSION *ppSession)
{
    Log(("SUP: Fake mode!\n"));
    static const SUPFUNC s_aFakeFunctions[] =
    {
        /* name                                     function */
        { "SUPR0ObjRegister",                       0xefef0000 },
        { "SUPR0ObjAddRef",                         0xefef0001 },
        { "SUPR0ObjRelease",                        0xefef0002 },
        { "SUPR0ObjVerifyAccess",                   0xefef0003 },
        { "SUPR0LockMem",                           0xefef0004 },
        { "SUPR0UnlockMem",                         0xefef0005 },
        { "SUPR0ContAlloc",                         0xefef0006 },
        { "SUPR0ContFree",                          0xefef0007 },
        { "SUPR0MemAlloc",                          0xefef0008 },
        { "SUPR0MemGetPhys",                        0xefef0009 },
        { "SUPR0MemFree",                           0xefef000a },
        { "SUPR0Printf",                            0xefef000b },
        { "RTMemAlloc",                             0xefef000c },
        { "RTMemAllocZ",                            0xefef000d },
        { "RTMemFree",                              0xefef000e },
        { "RTSemFastMutexCreate",                   0xefef000f },
        { "RTSemFastMutexDestroy",                  0xefef0010 },
        { "RTSemFastMutexRequest",                  0xefef0011 },
        { "RTSemFastMutexRelease",                  0xefef0012 },
        { "RTSemEventCreate",                       0xefef0013 },
        { "RTSemEventSignal",                       0xefef0014 },
        { "RTSemEventWait",                         0xefef0015 },
        { "RTSemEventDestroy",                      0xefef0016 },
        { "RTSpinlockCreate",                       0xefef0017 },
        { "RTSpinlockDestroy",                      0xefef0018 },
        { "RTSpinlockAcquire",                      0xefef0019 },
        { "RTSpinlockRelease",                      0xefef001a },
        { "RTSpinlockAcquireNoInts",                0xefef001b },
        { "RTSpinlockReleaseNoInts",                0xefef001c },
        { "RTThreadNativeSelf",                     0xefef001d },
        { "RTThreadSleep",                          0xefef001e },
        { "RTThreadYield",                          0xefef001f },
        { "RTLogDefaultInstance",                   0xefef0020 },
        { "RTLogRelDefaultInstance",                0xefef0021 },
        { "RTLogSetDefaultInstanceThread",          0xefef0022 },
        { "RTLogLogger",                            0xefef0023 },
        { "RTLogLoggerEx",                          0xefef0024 },
        { "RTLogLoggerExV",                         0xefef0025 },
        { "AssertMsg1",                             0xefef0026 },
        { "AssertMsg2",                             0xefef0027 },
    };

    /* fake r0 functions. */
    g_pFunctions = (PSUPQUERYFUNCS_OUT)RTMemAllocZ(RT_OFFSETOF(SUPQUERYFUNCS_OUT, aFunctions[RT_ELEMENTS(s_aFakeFunctions)]));
    if (g_pFunctions)
    {
        g_pFunctions->cFunctions = RT_ELEMENTS(s_aFakeFunctions);
        memcpy(&g_pFunctions->aFunctions[0], &s_aFakeFunctions[0], sizeof(s_aFakeFunctions));
        g_pSession = (PSUPDRVSESSION)(void *)g_pFunctions;
        if (ppSession)
            *ppSession = g_pSession;
#ifndef VBOX_WITHOUT_IDT_PATCHING
        Assert(g_u8Interrupt == 3);
#endif

        /* fake the GIP. */
        g_pSUPGlobalInfoPage = (PSUPGLOBALINFOPAGE)RTMemPageAlloc(PAGE_SIZE);
        if (g_pSUPGlobalInfoPage)
        {
            g_pSUPGlobalInfoPageR0 = g_pSUPGlobalInfoPage;
            g_HCPhysSUPGlobalInfoPage = NIL_RTHCPHYS & ~(RTHCPHYS)PAGE_OFFSET_MASK;
            /* the page is supposed to be invalid, so don't set the magic. */
            return VINF_SUCCESS;
        }

        RTMemFree(g_pFunctions);
        g_pFunctions = NULL;
    }
    return VERR_NO_MEMORY;
}


SUPR3DECL(int) SUPTerm(bool fForced)
{
    /*
     * Verify state.
     */
    AssertMsg(g_cInits > 0, ("SUPTerm() is called before SUPInit()!\n"));
    if (g_cInits == 0)
        return VERR_WRONG_ORDER;
    if (g_cInits == 1 || fForced)
    {
        /*
         * NULL the GIP pointer.
         */
        if (g_pSUPGlobalInfoPage)
        {
            ASMAtomicXchgPtr((void * volatile *)&g_pSUPGlobalInfoPage, NULL);
            ASMAtomicXchgPtr((void * volatile *)&g_pSUPGlobalInfoPageR0, NULL);
            ASMAtomicXchgSize(&g_HCPhysSUPGlobalInfoPage, NIL_RTHCPHYS);
            /* just a little safe guard against threads using the page. */
            RTThreadSleep(50);
        }

        /*
         * Close the support driver.
         */
        int rc = suplibOsTerm();
        if (rc)
            return rc;

        g_u32Cookie         = 0;
        g_u32SessionCookie  = 0;
#ifndef VBOX_WITHOUT_IDT_PATCHING
        g_u8Interrupt       = 3;
#endif
        g_cInits            = 0;
    }
    else
        g_cInits--;

    return 0;
}


SUPR3DECL(SUPPAGINGMODE) SUPGetPagingMode(void)
{
    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPGETPAGINGMODE_IN In;
    In.u32Cookie        = g_u32Cookie;
    In.u32SessionCookie = g_u32SessionCookie;
    SUPGETPAGINGMODE_OUT Out = {SUPPAGINGMODE_INVALID};
    int rc;
    if (!g_u32FakeMode)
    {
        rc = suplibOsIOCtl(SUP_IOCTL_GET_PAGING_MODE, &In, sizeof(In), &Out, sizeof(Out));
        if (VBOX_FAILURE(rc))
            Out.enmMode = SUPPAGINGMODE_INVALID;
    }
    else
        Out.enmMode = SUPPAGINGMODE_32_BIT_GLOBAL;

    return Out.enmMode;
}

SUPR3DECL(int) SUPCallVMMR0Ex(PVMR0 pVMR0, unsigned uOperation, void *pvArg, unsigned cbArg)
{
    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPCALLVMMR0_IN In;
    In.u32Cookie        = g_u32Cookie;
    In.u32SessionCookie = g_u32SessionCookie;
    In.pVMR0            = pVMR0;
    In.uOperation       = uOperation;
    In.cbArg            = cbArg;
    In.pvArg            = pvArg;
    Assert(!g_u32FakeMode);
    SUPCALLVMMR0_OUT Out = {VINF_SUCCESS};
    int rc = suplibOsIOCtl(SUP_IOCTL_CALL_VMMR0, &In, sizeof(In), &Out, sizeof(Out));
    if (VBOX_SUCCESS(rc))
        rc = Out.rc;
    return rc;
}


SUPR3DECL(int) SUPCallVMMR0(PVMR0 pVMR0, unsigned uOperation, void *pvArg)
{
#ifndef VBOX_WITHOUT_IDT_PATCHING
    return g_pfnCallVMMR0(pVMR0, uOperation, pvArg);

#else
    if (RT_LIKELY(uOperation == VMMR0_DO_RAW_RUN))
    {
        Assert(!pvArg);
        return suplibOSIOCtlFast(SUP_IOCTL_FAST_DO_RAW_RUN);
    }
    if (RT_LIKELY(uOperation == VMMR0_DO_HWACC_RUN))
    {
        Assert(!pvArg);
        return suplibOSIOCtlFast(SUP_IOCTL_FAST_DO_HWACC_RUN);
    }
    if (uOperation == VMMR0_DO_NOP)
    {
        Assert(!pvArg);
        return suplibOSIOCtlFast(SUP_IOCTL_FAST_DO_NOP);
    }
    return SUPCallVMMR0Ex(pVMR0, uOperation, pvArg, pvArg ? sizeof(pvArg) : 0);
#endif
}


SUPR3DECL(int) SUPSetVMForFastIOCtl(PVMR0 pVMR0)
{
    SUPSETVMFORFAST_IN In;
    In.u32Cookie        = g_u32Cookie;
    In.u32SessionCookie = g_u32SessionCookie;
    In.pVMR0            = pVMR0;
    int rc;
    if (RT_LIKELY(!g_u32FakeMode))
        rc = suplibOsIOCtl(SUP_IOCTL_SET_VM_FOR_FAST, &In, sizeof(In), NULL, 0);
    else
        rc = VINF_SUCCESS;
    return rc;
}


SUPR3DECL(int) SUPPageLock(void *pvStart, size_t cPages, PSUPPAGE paPages)
{
    /*
     * Validate.
     */
    AssertPtr(pvStart);
    AssertMsg(RT_ALIGN_P(pvStart, PAGE_SIZE) == pvStart, ("pvStart (%p) must be page aligned\n", pvStart));
    AssertPtr(paPages);

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPPINPAGES_IN      In;
    In.u32Cookie        = g_u32Cookie;
    In.u32SessionCookie = g_u32SessionCookie;
    In.pvR3             = pvStart;
    In.cPages           = cPages; AssertRelease(In.cPages == cPages);
    int rc;
    if (!g_u32FakeMode)
    {
        PSUPPINPAGES_OUT pOut;
        AssertCompile(sizeof(paPages[0]) == sizeof(pOut->aPages[0]));

#if 0
        size_t cbOut = RT_OFFSETOF(SUPPINPAGES_OUT, aPages[cPages]);
        pOut = (PSUPPINPAGES_OUT)RTMemTmpAllocZ(cbOut);
        if (!pOut)
            return VERR_NO_TMP_MEMORY;

        rc = suplibOsIOCtl(SUP_IOCTL_PINPAGES, &In, sizeof(In), pOut, cbOut);
        if (RT_SUCCESS(rc))
            memcpy(paPages, &pOut->aPages[0], sizeof(paPages[0]) * cPages);
        RTMemTmpFree(pOut);

#else
        /* a hack to save some time. */
        pOut = (PSUPPINPAGES_OUT)(void*)paPages;
        Assert(RT_OFFSETOF(SUPPINPAGES_OUT, aPages) == 0 && sizeof(paPages[0]) == sizeof(pOut->aPages[0]));
        rc = suplibOsIOCtl(SUP_IOCTL_PINPAGES, &In, sizeof(In), pOut, RT_OFFSETOF(SUPPINPAGES_OUT, aPages[cPages]));
#endif
    }
    else
    {
        /* fake a successfull result. */
        RTHCPHYS    Phys = (uintptr_t)pvStart + PAGE_SIZE * 1024;
        unsigned    iPage = cPages;
        while (iPage-- > 0)
            paPages[iPage].Phys = Phys + (iPage << PAGE_SHIFT);
        rc = VINF_SUCCESS;
    }

    return rc;
}


SUPR3DECL(int) SUPPageUnlock(void *pvStart)
{
    /*
     * Validate.
     */
    AssertPtr(pvStart);
    AssertMsg(RT_ALIGN_P(pvStart, PAGE_SIZE) == pvStart, ("pvStart (%p) must be page aligned\n", pvStart));

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPUNPINPAGES_IN  In;
    In.u32Cookie        = g_u32Cookie;
    In.u32SessionCookie = g_u32SessionCookie;
    In.pvR3             = pvStart;
    int rc;
    if (!g_u32FakeMode)
        rc = suplibOsIOCtl(SUP_IOCTL_UNPINPAGES, &In, sizeof(In), NULL, 0);
    else
        rc = VINF_SUCCESS;

    return rc;
}


SUPR3DECL(void *) SUPContAlloc(size_t cPages, PRTHCPHYS pHCPhys)
{
    return SUPContAlloc2(cPages, NIL_RTR0PTR, pHCPhys);
}


SUPR3DECL(void *) SUPContAlloc2(size_t cPages, PRTR0PTR pR0Ptr, PRTHCPHYS pHCPhys)
{
    /*
     * Validate.
     */
    AssertMsg(cPages > 0 && cPages < 256, ("cPages=%d must be > 0 and < 256\n", cPages));
    AssertPtr(pHCPhys);
    *pHCPhys = NIL_RTHCPHYS;
    AssertPtrNull(pR0Ptr);
    if (pR0Ptr)
        *pR0Ptr = NIL_RTR0PTR;

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPCONTALLOC_IN     In;
    In.u32Cookie        = g_u32Cookie;
    In.u32SessionCookie = g_u32SessionCookie;
    In.cPages           = cPages;
    SUPCONTALLOC_OUT    Out;
    int rc;
    if (!g_u32FakeMode)
        rc = suplibOsIOCtl(SUP_IOCTL_CONT_ALLOC, &In, sizeof(In), &Out, sizeof(Out));
    else
    {
        rc = SUPPageAlloc(In.cPages, &Out.pvR3);
        Out.HCPhys = (uintptr_t)Out.pvR3 + (PAGE_SHIFT * 1024);
        Out.pvR0 = (uintptr_t)Out.pvR3;
    }
    if (VBOX_SUCCESS(rc))
    {
        *pHCPhys = (RTHCPHYS)Out.HCPhys;
        if (pR0Ptr)
            *pR0Ptr = Out.pvR0;
        return Out.pvR3;
    }

    return NULL;
}


SUPR3DECL(int) SUPContFree(void *pv, size_t cPages)
{
    /*
     * Validate.
     */
    AssertPtr(pv);
    if (!pv)
        return VINF_SUCCESS;

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPCONTFREE_IN     In;
    In.u32Cookie        = g_u32Cookie;
    In.u32SessionCookie = g_u32SessionCookie;
    In.pvR3             = pv;
    int rc;
    if (!g_u32FakeMode)
        rc = suplibOsIOCtl(SUP_IOCTL_CONT_FREE, &In, sizeof(In), NULL, 0);
    else
        rc = SUPPageFree(pv, cPages);

    return rc;
}


SUPR3DECL(int) SUPLowAlloc(size_t cPages, void **ppvPages, PRTR0PTR ppvPagesR0, PSUPPAGE paPages)
{
    /*
     * Validate.
     */
    AssertMsg(cPages > 0 && cPages < 256, ("cPages=%d must be > 0 and < 256\n", cPages));
    AssertPtr(ppvPages);
    *ppvPages = NULL;
    AssertPtr(paPages);

    int rc;
    if (!g_u32FakeMode)
    {
        /*
         * Issue IOCtl to the SUPDRV kernel module.
         */
        SUPLOWALLOC_IN      In;
        In.u32Cookie        = g_u32Cookie;
        In.u32SessionCookie = g_u32SessionCookie;
        In.cPages           = cPages;
        size_t              cbOut = RT_OFFSETOF(SUPLOWALLOC_OUT, aPages[cPages]);
        PSUPLOWALLOC_OUT    pOut = (PSUPLOWALLOC_OUT)RTMemTmpAllocZ(cbOut);
        if (pOut)
        {
            rc = suplibOsIOCtl(SUP_IOCTL_LOW_ALLOC, &In, sizeof(In), pOut, cbOut);
            if (VBOX_SUCCESS(rc))
            {
                *ppvPages = pOut->pvR3;
                if (ppvPagesR0)
                    *ppvPagesR0 = pOut->pvR0;
                AssertCompile(sizeof(paPages[0]) == sizeof(pOut->aPages[0]));
                memcpy(paPages, &pOut->aPages[0], sizeof(paPages[0]) * cPages);
#ifdef VBOX_STRICT
                for (unsigned i = 0; i < cPages; i++)
                    AssertReleaseMsg(   paPages[i].Phys <= 0xfffff000
                                     && !(paPages[i].Phys & PAGE_OFFSET_MASK)
                                     && paPages[i].Phys > 0,
                                     ("[%d]=%VHp\n", paPages[i].Phys));
#endif
            }
            RTMemTmpFree(pOut);
        }
        else
            rc = VERR_NO_TMP_MEMORY;
    }
    else
    {
        rc = SUPPageAlloc(cPages, ppvPages);
        if (VBOX_SUCCESS(rc))
        {
            /* fake physical addresses. */
            RTHCPHYS    Phys = (uintptr_t)*ppvPages + PAGE_SIZE * 1024;
            unsigned    iPage = cPages;
            while (iPage-- > 0)
                paPages[iPage].Phys = Phys + (iPage << PAGE_SHIFT);
        }
    }

    return rc;
}


SUPR3DECL(int) SUPLowFree(void *pv, size_t cPages)
{
    /*
     * Validate.
     */
    AssertPtr(pv);
    if (!pv)
        return VINF_SUCCESS;

    /*
     * Issue IOCtl to the SUPDRV kernel module.
     */
    SUPLOWFREE_IN     In;
    In.u32Cookie        = g_u32Cookie;
    In.u32SessionCookie = g_u32SessionCookie;
    In.pvR3             = pv;
    int rc;
    if (!g_u32FakeMode)
        rc = suplibOsIOCtl(SUP_IOCTL_LOW_FREE, &In, sizeof(In), NULL, 0);
    else
        rc = SUPPageFree(pv, cPages);

    return rc;
}


SUPR3DECL(int) SUPPageAlloc(size_t cPages, void **ppvPages)
{
    /*
     * Validate.
     */
    if (cPages == 0)
    {
        AssertMsgFailed(("Invalid param cPages=0, must be > 0\n"));
        return VERR_INVALID_PARAMETER;
    }
    AssertPtr(ppvPages);
    if (!ppvPages)
        return VERR_INVALID_PARAMETER;
    *ppvPages = NULL;

    /*
     * Call OS specific worker.
     */
    return suplibOsPageAlloc(cPages, ppvPages);
}


SUPR3DECL(int) SUPPageFree(void *pvPages, size_t cPages)
{
    /*
     * Validate.
     */
    AssertPtr(pvPages);
    if (!pvPages)
        return VINF_SUCCESS;

    /*
     * Call OS specific worker.
     */
    return suplibOsPageFree(pvPages, cPages);
}


SUPR3DECL(int) SUPLoadModule(const char *pszFilename, const char *pszModule, void **ppvImageBase)
{
    /*
     * Load the module.
     * If it's VMMR0.r0 we need to install the IDTE.
     */
    int rc = supLoadModule(pszFilename, pszModule, ppvImageBase);
#ifndef VBOX_WITHOUT_IDT_PATCHING
    if (    VBOX_SUCCESS(rc)
        &&  !strcmp(pszModule, "VMMR0.r0"))
    {
        rc = supInstallIDTE();
        if (VBOX_FAILURE(rc))
            SUPFreeModule(*ppvImageBase);
    }
#endif /* VBOX_WITHOUT_IDT_PATCHING */

    return rc;
}


#ifndef VBOX_WITHOUT_IDT_PATCHING
/**
 * Generates the code for calling the interrupt gate.
 *
 * @returns VBox status code.
 *          g_pfnCallVMMR0 is changed on success.
 * @param   u8Interrupt     The interrupt number.
 */
static int suplibGenerateCallVMMR0(uint8_t u8Interrupt)
{
    /*
     * Allocate memory.
     */
    uint8_t *pb = (uint8_t *)RTMemExecAlloc(256);
    AssertReturn(pb, VERR_NO_MEMORY);
    memset(pb, 0xcc, 256);
    Assert(!g_pfnCallVMMR0);
    g_pfnCallVMMR0 = *(PFNCALLVMMR0*)&pb;

    /*
     * Generate the code.
     */
#ifdef RT_ARCH_AMD64
    /*
     * reg params:
     *      <GCC>   <MSC>   <argument>
     *      rdi     rcx     pVMR0
     *      esi     edx     uOperation
     *      rdx     r8      pvArg
     *
     *      eax     eax     [g_u32Gookie]
     */
    *pb++ = 0xb8;                       /* mov eax, <g_u32Cookie> */
    *(uint32_t *)pb = g_u32Cookie;
    pb += sizeof(uint32_t);

    *pb++ = 0xcd;                       /* int <u8Interrupt> */
    *pb++ = u8Interrupt;

    *pb++ = 0xc3;                       /* ret */

#else
    /*
     * x86 stack:
     *          0   saved esi
     *      0   4   ret
     *      4   8   pVM
     *      8   c   uOperation
     *      c  10   pvArg
     */
    *pb++ = 0x56;                       /* push esi */

    *pb++ = 0x8b;                       /* mov eax, [pVM] */
    *pb++ = 0x44;
    *pb++ = 0x24;
    *pb++ = 0x08;                       /* esp+08h */

    *pb++ = 0x8b;                       /* mov edx, [uOperation] */
    *pb++ = 0x54;
    *pb++ = 0x24;
    *pb++ = 0x0c;                       /* esp+0ch */

    *pb++ = 0x8b;                       /* mov ecx, [pvArg] */
    *pb++ = 0x4c;
    *pb++ = 0x24;
    *pb++ = 0x10;                       /* esp+10h */

    *pb++ = 0xbe;                       /* mov esi, <g_u32Cookie> */
    *(uint32_t *)pb = g_u32Cookie;
    pb += sizeof(uint32_t);

    *pb++ = 0xcd;                       /* int <u8Interrupt> */
    *pb++ = u8Interrupt;

    *pb++ = 0x5e;                       /* pop esi */

    *pb++ = 0xc3;                       /* ret */
#endif

    return VINF_SUCCESS;
}


/**
 * Installs the IDTE patch.
 *
 * @return VBox status code.
 */
static int supInstallIDTE(void)
{
    /* already installed? */
    if (g_u8Interrupt != 3 || g_u32FakeMode)
        return VINF_SUCCESS;

    int rc = VINF_SUCCESS;
    const unsigned  cCpus = RTSystemProcessorGetCount();
    if (cCpus <= 1)
    {
        /* UNI */
        SUPIDTINSTALL_IN  In;
        In.u32Cookie        = g_u32Cookie;
        In.u32SessionCookie = g_u32SessionCookie;
        SUPIDTINSTALL_OUT Out = {3};

        rc = suplibOsIOCtl(SUP_IOCTL_IDT_INSTALL, &In, sizeof(In), &Out, sizeof(Out));
        if (VBOX_SUCCESS(rc))
        {
            g_u8Interrupt = Out.u8Idt;
            rc = suplibGenerateCallVMMR0(Out.u8Idt);
        }
    }
    else
    {
        /* SMP */
        uint64_t        u64AffMaskSaved = RTThreadGetAffinity();
        uint64_t        u64AffMaskPatched = RTSystemProcessorGetActiveMask() & u64AffMaskSaved;
        unsigned        cCpusPatched = 0;

        for (int i = 0; i < 64; i++)
        {
            /* Skip absent and inactive processors. */
            uint64_t u64Mask = 1ULL << i;
            if (!(u64Mask & u64AffMaskPatched))
                continue;

            /* Change CPU */
            int rc2 = RTThreadSetAffinity(u64Mask);
            if (VBOX_FAILURE(rc2))
            {
                u64AffMaskPatched &= ~u64Mask;
                Log(("SUPLoadVMM: Failed to set affinity to cpu no. %d, rc=%Vrc.\n", i, rc2));
                continue;
            }

            /* Patch the CPU. */
            SUPIDTINSTALL_IN  In;
            In.u32Cookie        = g_u32Cookie;
            In.u32SessionCookie = g_u32SessionCookie;
            SUPIDTINSTALL_OUT Out = {3};

            rc2 = suplibOsIOCtl(SUP_IOCTL_IDT_INSTALL, &In, sizeof(In), &Out, sizeof(Out));
            if (VBOX_SUCCESS(rc2))
            {
                if (!cCpusPatched)
                {
                    g_u8Interrupt = Out.u8Idt;
                    rc2 = suplibGenerateCallVMMR0(Out.u8Idt);
                    if (VBOX_FAILURE(rc))
                        rc2 = rc;
                }
                else
                    Assert(g_u8Interrupt == Out.u8Idt);
                cCpusPatched++;
            }
            else
            {

                Log(("SUPLoadVMM: Failed to patch cpu no. %d, rc=%Vrc.\n", i, rc2));
                if (VBOX_SUCCESS(rc))
                    rc = rc2;
            }
        }

        /* Fail if no CPUs was patched! */
        if (VBOX_SUCCESS(rc) && cCpusPatched <= 0)
            rc = VERR_GENERAL_FAILURE;
        /* Ignore failures if a CPU was patched. */
        else if (VBOX_FAILURE(rc) && cCpusPatched > 0)
        {
            /** @todo add an eventlog/syslog line out this. */
            rc = VINF_SUCCESS;
        }

        /* Set/restore the thread affinity. */
        if (VBOX_SUCCESS(rc))
        {
            rc = RTThreadSetAffinity(u64AffMaskPatched);
            AssertRC(rc);
        }
        else
        {
            int rc2 = RTThreadSetAffinity(u64AffMaskSaved);
            AssertRC(rc2);
        }
    }
    return rc;
}
#endif /* !VBOX_WITHOUT_IDT_PATCHING */


/**
 * Resolve an external symbol during RTLdrGetBits().
 *
 * @returns VBox status code.
 * @param   hLdrMod         The loader module handle.
 * @param   pszModule       Module name.
 * @param   pszSymbol       Symbol name, NULL if uSymbol should be used.
 * @param   uSymbol         Symbol ordinal, ~0 if pszSymbol should be used.
 * @param   pValue          Where to store the symbol value (address).
 * @param   pvUser          User argument.
 */
static DECLCALLBACK(int) supLoadModuleResolveImport(RTLDRMOD hLdrMod, const char *pszModule,
                                                    const char *pszSymbol, unsigned uSymbol, RTUINTPTR *pValue, void *pvUser)
{
    AssertPtr(pValue);
    AssertPtr(pvUser);

    /*
     * Only SUPR0 and VMMR0.r0
     */
    if (    pszModule
        &&  *pszModule
        &&  strcmp(pszModule, "SUPR0.dll")
        &&  strcmp(pszModule, "VMMR0.r0"))
    {
        AssertMsgFailed(("%s is importing from %s! (expected 'SUPR0.dll' or 'VMMR0.r0', case-sensitiv)\n", pvUser, pszModule));
        return VERR_SYMBOL_NOT_FOUND;
    }

    /*
     * No ordinals.
     */
    if (pszSymbol < (const char*)0x10000)
    {
        AssertMsgFailed(("%s is importing by ordinal (ord=%d)\n", pvUser, (int)(uintptr_t)pszSymbol));
        return VERR_SYMBOL_NOT_FOUND;
    }

    /*
     * Lookup symbol.
     */
    /* skip the 64-bit ELF import prefix first. */
    if (!strncmp(pszSymbol, "SUPR0$", sizeof("SUPR0$") - 1))
        pszSymbol += sizeof("SUPR0$") - 1;

    /*
     * Check the VMMR0.r0 module if loaded.
     */
    /** @todo call the SUPLoadModule caller.... */
    /** @todo proper reference counting and such. */
    if (g_pvVMMR0 != NIL_RTR0PTR)
    {
        void *pvValue;
        if (!SUPGetSymbolR0((void *)g_pvVMMR0, pszSymbol, &pvValue))
        {
            *pValue = (uintptr_t)pvValue;
            return VINF_SUCCESS;
        }
    }

    /* iterate the function table. */
    int c = g_pFunctions->cFunctions;
    PSUPFUNC pFunc = &g_pFunctions->aFunctions[0];
    while (c-- > 0)
    {
        if (!strcmp(pFunc->szName, pszSymbol))
        {
            *pValue = (uintptr_t)pFunc->pfn;
            return VINF_SUCCESS;
        }
        pFunc++;
    }

    /*
     * The GIP.
     */
    /** @todo R0 mapping? */
    if (    pszSymbol
        &&  g_pSUPGlobalInfoPage
        &&  g_pSUPGlobalInfoPageR0
        &&  !strcmp(pszSymbol, "g_SUPGlobalInfoPage"))
    {
        *pValue = (uintptr_t)g_pSUPGlobalInfoPageR0;
        return VINF_SUCCESS;
    }

    /*
     * Despair.
     */
    c = g_pFunctions->cFunctions;
    pFunc = &g_pFunctions->aFunctions[0];
    while (c-- > 0)
    {
        AssertMsg2("%d: %s\n", g_pFunctions->cFunctions - c, pFunc->szName);
        pFunc++;
    }

    AssertMsgFailed(("%s is importing %s which we couldn't find\n", pvUser, pszSymbol));
    return VERR_SYMBOL_NOT_FOUND;
}


/** Argument package for supLoadModuleCalcSizeCB. */
typedef struct SUPLDRCALCSIZEARGS
{
    size_t          cbStrings;
    uint32_t        cSymbols;
    size_t          cbImage;
} SUPLDRCALCSIZEARGS, *PSUPLDRCALCSIZEARGS;

/**
 * Callback used to calculate the image size.
 * @return VINF_SUCCESS
 */
static DECLCALLBACK(int) supLoadModuleCalcSizeCB(RTLDRMOD hLdrMod, const char *pszSymbol, unsigned uSymbol, RTUINTPTR Value, void *pvUser)
{
    PSUPLDRCALCSIZEARGS pArgs = (PSUPLDRCALCSIZEARGS)pvUser;
    if (    pszSymbol != NULL
        &&  *pszSymbol
        &&  Value <= pArgs->cbImage)
    {
        pArgs->cSymbols++;
        pArgs->cbStrings += strlen(pszSymbol) + 1;
    }
    return VINF_SUCCESS;
}


/** Argument package for supLoadModuleCreateTabsCB. */
typedef struct SUPLDRCREATETABSARGS
{
    size_t          cbImage;
    PSUPLDRSYM      pSym;
    char           *pszBase;
    char           *psz;
} SUPLDRCREATETABSARGS, *PSUPLDRCREATETABSARGS;

/**
 * Callback used to calculate the image size.
 * @return VINF_SUCCESS
 */
static DECLCALLBACK(int) supLoadModuleCreateTabsCB(RTLDRMOD hLdrMod, const char *pszSymbol, unsigned uSymbol, RTUINTPTR Value, void *pvUser)
{
    PSUPLDRCREATETABSARGS pArgs = (PSUPLDRCREATETABSARGS)pvUser;
    if (    pszSymbol != NULL
        &&  *pszSymbol
        &&  Value <= pArgs->cbImage)
    {
        pArgs->pSym->offSymbol = (uint32_t)Value;
        pArgs->pSym->offName = pArgs->psz - pArgs->pszBase;
        pArgs->pSym++;

        size_t cbCopy = strlen(pszSymbol) + 1;
        memcpy(pArgs->psz, pszSymbol, cbCopy);
        pArgs->psz += cbCopy;
    }
    return VINF_SUCCESS;
}


/**
 * Worker for SUPLoadModule().
 *
 * @returns VBox status code.
 * @param   pszFilename     Name of the VMMR0 image file
 */
static int supLoadModule(const char *pszFilename, const char *pszModule, void **ppvImageBase)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pszFilename, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszModule, VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppvImageBase, VERR_INVALID_PARAMETER);
    AssertReturn(strlen(pszModule) < SIZEOFMEMB(SUPLDROPEN_IN, szName), VERR_FILENAME_TOO_LONG);

    const bool fIsVMMR0 = !strcmp(pszModule, "VMMR0.r0");
    *ppvImageBase = NULL;

    /*
     * Open image file and figure its size.
     */
    RTLDRMOD hLdrMod;
    int rc = RTLdrOpen(pszFilename, &hLdrMod);
    if (!VBOX_SUCCESS(rc))
        return rc;

    SUPLDRCALCSIZEARGS CalcArgs;
    CalcArgs.cbStrings = 0;
    CalcArgs.cSymbols = 0;
    CalcArgs.cbImage = RTLdrSize(hLdrMod);
    rc = RTLdrEnumSymbols(hLdrMod, 0, NULL, 0, supLoadModuleCalcSizeCB, &CalcArgs);
    if (VBOX_SUCCESS(rc))
    {
        const uint32_t  offSymTab = RT_ALIGN_32(CalcArgs.cbImage, 8);
        const uint32_t  offStrTab = offSymTab + CalcArgs.cSymbols * sizeof(SUPLDRSYM);
        const uint32_t  cbImage   = RT_ALIGN_32(offStrTab + CalcArgs.cbStrings, 8);

        /*
         * Open the R0 image.
         */
        SUPLDROPEN_IN OpenIn;
        OpenIn.u32Cookie        = g_u32Cookie;
        OpenIn.u32SessionCookie = g_u32SessionCookie;
        OpenIn.cbImage          = cbImage;
        strcpy(OpenIn.szName, pszModule);
        SUPLDROPEN_OUT OpenOut;
        if (!g_u32FakeMode)
            rc = suplibOsIOCtl(SUP_IOCTL_LDR_OPEN, &OpenIn, sizeof(OpenIn), &OpenOut, sizeof(OpenOut));
        else
        {
            OpenOut.fNeedsLoading = true;
            OpenOut.pvImageBase = 0xef423420;
        }
        *ppvImageBase = (void *)OpenOut.pvImageBase;
        if (    VBOX_SUCCESS(rc)
            &&  OpenOut.fNeedsLoading)
        {
            /*
             * We need to load it.
             * Allocate memory for the image bits.
             */
            unsigned        cbIn = RT_OFFSETOF(SUPLDRLOAD_IN, achImage[cbImage]);
            PSUPLDRLOAD_IN  pIn = (PSUPLDRLOAD_IN)RTMemTmpAlloc(cbIn);
            if (pIn)
            {
                /*
                 * Get the image bits.
                 */
                rc = RTLdrGetBits(hLdrMod, &pIn->achImage[0], (uintptr_t)OpenOut.pvImageBase,
                                  supLoadModuleResolveImport, (void *)pszModule);

                if (VBOX_SUCCESS(rc))
                {
                    /*
                     * Get the entry points.
                     */
                    RTUINTPTR VMMR0Entry = 0;
                    RTUINTPTR ModuleInit = 0;
                    RTUINTPTR ModuleTerm = 0;
                    if (fIsVMMR0)
                        rc = RTLdrGetSymbolEx(hLdrMod, &pIn->achImage[0], (uintptr_t)OpenOut.pvImageBase, "VMMR0Entry", &VMMR0Entry);
                    if (VBOX_SUCCESS(rc))
                    {
                        int rc2 = RTLdrGetSymbolEx(hLdrMod, &pIn->achImage[0], (uintptr_t)OpenOut.pvImageBase, "ModuleInit", &ModuleInit);
                        if (VBOX_FAILURE(rc2))
                            ModuleInit = 0;

                        rc2 = RTLdrGetSymbolEx(hLdrMod, &pIn->achImage[0], (uintptr_t)OpenOut.pvImageBase, "ModuleTerm", &ModuleTerm);
                        if (VBOX_FAILURE(rc2))
                            ModuleTerm = 0;
                    }
                    if (VBOX_SUCCESS(rc))
                    {
                        /*
                         * Create the symbol and string tables.
                         */
                        SUPLDRCREATETABSARGS CreateArgs;
                        CreateArgs.cbImage = CalcArgs.cbImage;
                        CreateArgs.pSym    = (PSUPLDRSYM)&pIn->achImage[offSymTab];
                        CreateArgs.pszBase =     (char *)&pIn->achImage[offStrTab];
                        CreateArgs.psz     = CreateArgs.pszBase;
                        rc = RTLdrEnumSymbols(hLdrMod, 0, NULL, 0, supLoadModuleCreateTabsCB, &CreateArgs);
                        if (VBOX_SUCCESS(rc))
                        {
                            AssertRelease((size_t)(CreateArgs.psz - CreateArgs.pszBase) <= CalcArgs.cbStrings);
                            AssertRelease((size_t)(CreateArgs.pSym - (PSUPLDRSYM)&pIn->achImage[offSymTab]) <= CalcArgs.cSymbols);

                            /*
                             * Upload the image.
                             */
                            pIn->u32Cookie                  = g_u32Cookie;
                            pIn->u32SessionCookie           = g_u32SessionCookie;
                            pIn->pfnModuleInit              = (RTR0PTR)ModuleInit;
                            pIn->pfnModuleTerm              = (RTR0PTR)ModuleTerm;
                            if (fIsVMMR0)
                            {
                                pIn->eEPType                = pIn->EP_VMMR0;
                                pIn->EP.VMMR0.pvVMMR0       = OpenOut.pvImageBase;
                                pIn->EP.VMMR0.pvVMMR0Entry  = (RTR0PTR)VMMR0Entry;
                            }
                            else
                                pIn->eEPType                = pIn->EP_NOTHING;
                            pIn->offStrTab                  = offStrTab;
                            pIn->cbStrTab                   = (uint32_t)CalcArgs.cbStrings;
                            AssertRelease(pIn->cbStrTab == CalcArgs.cbStrings);
                            pIn->offSymbols                 = offSymTab;
                            pIn->cSymbols                   = CalcArgs.cSymbols;
                            pIn->cbImage                    = cbImage;
                            pIn->pvImageBase                = OpenOut.pvImageBase;
                            if (!g_u32FakeMode)
                                rc = suplibOsIOCtl(SUP_IOCTL_LDR_LOAD, pIn, cbIn, NULL, 0);
                            else
                                rc = VINF_SUCCESS;
                            if (    VBOX_SUCCESS(rc)
                                ||  rc == VERR_ALREADY_LOADED /* this is because of a competing process. */
                               )
                            {
                                if (fIsVMMR0)
                                    g_pvVMMR0 = OpenOut.pvImageBase;
                                RTMemTmpFree(pIn);
                                RTLdrClose(hLdrMod);
                                return VINF_SUCCESS;
                            }
                        }
                    }
                }
                RTMemTmpFree(pIn);
            }
            else
            {
                AssertMsgFailed(("failed to allocated %d bytes for SUPLDRLOAD_IN structure!\n", cbIn));
                rc = VERR_NO_TMP_MEMORY;
            }
        }
        else if (VBOX_SUCCESS(rc) && fIsVMMR0)
            g_pvVMMR0 = OpenOut.pvImageBase;
    }
    RTLdrClose(hLdrMod);
    return rc;
}


SUPR3DECL(int) SUPFreeModule(void *pvImageBase)
{
    /*
     * There is one special module. When this is freed we'll
     * free the IDT entry that goes with it.
     *
     * Note that we don't keep count of VMMR0.r0 loads here, so the
     *      first unload will free it.
     */
    if ((RTR0PTR)pvImageBase == g_pvVMMR0)
    {
        /*
         * This is the point where we remove the IDT hook. We do
         * that before unloading the R0 VMM part.
         */
        if (g_u32FakeMode)
        {
#ifndef VBOX_WITHOUT_IDT_PATCHING
            g_u8Interrupt = 3;
            RTMemExecFree(*(void **)&g_pfnCallVMMR0);
            g_pfnCallVMMR0 = NULL;
#endif
            g_pvVMMR0 = NIL_RTR0PTR;
            return VINF_SUCCESS;
        }

#ifndef VBOX_WITHOUT_IDT_PATCHING
        /*
         * Uninstall IDT entry.
         */
        int rc = 0;
        if (g_u8Interrupt != 3)
        {
            SUPIDTREMOVE_IN  In;
            In.u32Cookie        = g_u32Cookie;
            In.u32SessionCookie = g_u32SessionCookie;
            rc = suplibOsIOCtl(SUP_IOCTL_IDT_REMOVE, &In, sizeof(In), NULL, 0);
            g_u8Interrupt = 3;
            RTMemExecFree(*(void **)&g_pfnCallVMMR0);
            g_pfnCallVMMR0 = NULL;
        }
#endif
    }

    /*
     * Free the requested module.
     */
    SUPLDRFREE_IN In;
    In.u32Cookie        = g_u32Cookie;
    In.u32SessionCookie = g_u32SessionCookie;
    In.pvImageBase      = (RTR0PTR)pvImageBase;
    int rc = VINF_SUCCESS;
    if (!g_u32FakeMode)
        rc = suplibOsIOCtl(SUP_IOCTL_LDR_FREE, &In, sizeof(In), NULL, 0);
    if (    VBOX_SUCCESS(rc)
        &&  (RTR0PTR)pvImageBase == g_pvVMMR0)
        g_pvVMMR0 = NIL_RTR0PTR;
    return rc;
}


SUPR3DECL(int) SUPGetSymbolR0(void *pvImageBase, const char *pszSymbol, void **ppvValue)
{
    *ppvValue = NULL;

    /*
     * Do ioctl.
     */
    size_t              cchSymbol = strlen(pszSymbol);
    const size_t        cbIn = RT_OFFSETOF(SUPLDRGETSYMBOL_IN, szSymbol[cchSymbol + 1]);
    SUPLDRGETSYMBOL_OUT Out = { NIL_RTR0PTR };
    PSUPLDRGETSYMBOL_IN pIn = (PSUPLDRGETSYMBOL_IN)alloca(cbIn);
    pIn->u32Cookie        = g_u32Cookie;
    pIn->u32SessionCookie = g_u32SessionCookie;
    pIn->pvImageBase      = (RTR0PTR)pvImageBase;
    memcpy(pIn->szSymbol, pszSymbol, cchSymbol + 1);
    int rc;
    if (RT_LIKELY(!g_u32FakeMode))
        rc = suplibOsIOCtl(SUP_IOCTL_LDR_GET_SYMBOL, pIn, cbIn, &Out, sizeof(Out));
    else
    {
        rc = VINF_SUCCESS;
        Out.pvSymbol = 0xdeadf00d;
    }
    if (VBOX_SUCCESS(rc))
        *ppvValue = (void *)Out.pvSymbol;
    return rc;
}


SUPR3DECL(int) SUPLoadVMM(const char *pszFilename)
{
    void *pvImageBase;
    return SUPLoadModule(pszFilename, "VMMR0.r0", &pvImageBase);
}


SUPR3DECL(int) SUPUnloadVMM(void)
{
    return SUPFreeModule((void*)g_pvVMMR0);
}


SUPR3DECL(int) SUPGipGetPhys(PRTHCPHYS pHCPhys)
{
    if (g_pSUPGlobalInfoPage)
    {
        *pHCPhys = g_HCPhysSUPGlobalInfoPage;
        return VINF_SUCCESS;
    }
    *pHCPhys = NIL_RTHCPHYS;
    return VERR_WRONG_ORDER;
}

