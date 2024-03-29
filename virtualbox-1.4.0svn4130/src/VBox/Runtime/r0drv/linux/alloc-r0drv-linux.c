/* $Id: alloc-r0drv-linux.c 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * innotek Portable Runtime - Memory Allocation, Ring-0 Driver, Linux.
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
#include "the-linux-kernel.h"
#include <iprt/mem.h>
#include <iprt/assert.h>
#include "r0drv/alloc-r0drv.h"

#if defined(RT_ARCH_AMD64) || defined(__DOXYGEN__)
/**
 * We need memory in the module range (~2GB to ~0) this can only be obtained
 * thru APIs that are not exported (see module_alloc()).
 *
 * So, we'll have to create a quick and dirty heap here using BSS memory.
 * Very annoying and it's going to restrict us!
 */
# define RTMEMALLOC_EXEC_HEAP
#endif
#ifdef RTMEMALLOC_EXEC_HEAP
# include <iprt/heap.h>
# include <iprt/spinlock.h>
# include <iprt/err.h>
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifdef RTMEMALLOC_EXEC_HEAP
/** The heap. */
static RTHEAPSIMPLE g_HeapExec = NIL_RTHEAPSIMPLE;
/** Spinlock protecting the heap. */
static RTSPINLOCK   g_HeapExecSpinlock = NIL_RTSPINLOCK;


/**
 * API for cleaning up the heap spinlock on IPRT termination.
 * This is as RTMemExecDonate specific to AMD64 Linux/GNU.
 */
void rtR0MemExecCleanup(void)
{
    RTSpinlockDestroy(g_HeapExecSpinlock);
    g_HeapExecSpinlock = NIL_RTSPINLOCK;
}


/**
 * Donate read+write+execute memory to the exec heap.
 *
 * This API is specific to AMD64 and Linux/GNU. A kernel module that desires to
 * use RTMemExecAlloc on AMD64 Linux/GNU will have to donate some statically
 * allocated memory in the module if it wishes for GCC generated code to work.
 * GCC can only generate modules that work in the address range ~2GB to ~0
 * currently.
 *
 * The API only accept one single donation.
 *
 * @returns IPRT status code.
 * @param   pvMemory    Pointer to the memory block.
 * @param   cb          The size of the memory block.
 */
RTR0DECL(int) RTR0MemExecDonate(void *pvMemory, size_t cb)
{
    int rc;
    AssertReturn(g_HeapExec == NIL_RTHEAPSIMPLE, VERR_WRONG_ORDER);

    rc = RTSpinlockCreate(&g_HeapExecSpinlock);
    if (RT_SUCCESS(rc))
    {
        rc = RTHeapSimpleInit(&g_HeapExec, pvMemory, cb);
        if (RT_FAILURE(rc))
            rtR0MemExecCleanup();
    }
    return rc;
}
#endif /* RTMEMALLOC_EXEC_HEAP */



/**
 * OS specific allocation function.
 */
PRTMEMHDR rtMemAlloc(size_t cb, uint32_t fFlags)
{
    /*
     * Allocate.
     */
    PRTMEMHDR pHdr;
    Assert(cb != sizeof(void *)); /* 99% of pointer sized allocations are wrong. */
    if (fFlags & RTMEMHDR_FLAG_EXEC)
    {
#if defined(RT_ARCH_AMD64)
# ifdef RTMEMALLOC_EXEC_HEAP
        if (g_HeapExec != NIL_RTHEAPSIMPLE)
        {
            RTSPINLOCKTMP SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
            RTSpinlockAcquireNoInts(g_HeapExecSpinlock, &SpinlockTmp);
            pHdr = (PRTMEMHDR)RTHeapSimpleAlloc(g_HeapExec, cb + sizeof(*pHdr), 0);
            RTSpinlockReleaseNoInts(g_HeapExecSpinlock, &SpinlockTmp);
            fFlags |= RTMEMHDR_FLAG_EXEC_HEAP;
        }
        else
# endif
            pHdr = (PRTMEMHDR)__vmalloc(cb + sizeof(*pHdr), GFP_KERNEL | __GFP_HIGHMEM, PAGE_KERNEL_EXEC);

#elif defined(PAGE_KERNEL_EXEC) && defined(CONFIG_X86_PAE)
        pHdr = (PRTMEMHDR)__vmalloc(cb + sizeof(*pHdr), GFP_KERNEL | __GFP_HIGHMEM,
                                    __pgprot(cpu_has_pge ? _PAGE_KERNEL_EXEC | _PAGE_GLOBAL : _PAGE_KERNEL_EXEC));
#else
        pHdr = (PRTMEMHDR)vmalloc(cb + sizeof(*pHdr));
#endif
    }
    else
    {
        if (cb <= PAGE_SIZE)
        {
            fFlags |= RTMEMHDR_FLAG_KMALLOC;
            pHdr = kmalloc(cb + sizeof(*pHdr), GFP_KERNEL);
        }
        else
            pHdr = vmalloc(cb + sizeof(*pHdr));
    }

    /*
     * Initialize.
     */
    if (pHdr)
    {
        pHdr->u32Magic  = RTMEMHDR_MAGIC;
        pHdr->fFlags    = fFlags;
        pHdr->cb        = cb;
        pHdr->u32Padding= 0;
    }
    return pHdr;
}


/**
 * OS specific free function.
 */
void rtMemFree(PRTMEMHDR pHdr)
{
    pHdr->u32Magic += 1;
    if (pHdr->fFlags & RTMEMHDR_FLAG_KMALLOC)
        kfree(pHdr);
#ifdef RTMEMALLOC_EXEC_HEAP
    else if (pHdr->fFlags & RTMEMHDR_FLAG_EXEC_HEAP)
    {
        RTSPINLOCKTMP SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
        RTSpinlockAcquireNoInts(g_HeapExecSpinlock, &SpinlockTmp);
        RTHeapSimpleFree(g_HeapExec, pHdr);
        RTSpinlockReleaseNoInts(g_HeapExecSpinlock, &SpinlockTmp);
    }
#endif
    else
        vfree(pHdr);
}


/**
 * Compute order. Some functions allocate 2^order pages.
 *
 * @returns order.
 * @param   cPages      Number of pages.
 */
static int CalcPowerOf2Order(unsigned long cPages)
{
    int             iOrder;
    unsigned long   cTmp;

    for (iOrder = 0, cTmp = cPages; cTmp >>= 1; ++iOrder)
        ;
    if (cPages & ~(1 << iOrder))
        ++iOrder;

    return iOrder;
}


/**
 * Allocates physical contiguous memory (below 4GB).
 * The allocation is page aligned and the content is undefined.
 *
 * @returns Pointer to the memory block. This is page aligned.
 * @param   pPhys   Where to store the physical address.
 * @param   cb      The allocation size in bytes. This is always
 *                  rounded up to PAGE_SIZE.
 */
RTR0DECL(void *) RTMemContAlloc(PRTCCPHYS pPhys, size_t cb)
{
    int             cOrder;
    unsigned        cPages;
    struct page    *paPages;

    /*
     * validate input.
     */
    Assert(VALID_PTR(pPhys));
    Assert(cb > 0);

    /*
     * Allocate page pointer array.
     */
    cb = RT_ALIGN_Z(cb, PAGE_SIZE);
    cPages = cb >> PAGE_SHIFT;
    cOrder = CalcPowerOf2Order(cPages);
#ifdef RT_ARCH_AMD64 /** @todo check out if there is a correct way of getting memory below 4GB (physically). */
    paPages = alloc_pages(GFP_DMA, cOrder);
#else
    paPages = alloc_pages(GFP_USER, cOrder);
#endif
    if (paPages)
    {
        /*
         * Reserve the pages and mark them executable.
         */
        unsigned iPage;
        for (iPage = 0; iPage < cPages; iPage++)
        {
            Assert(!PageHighMem(&paPages[iPage]));
            if (iPage + 1 < cPages)
            {
                AssertMsg(          (uintptr_t)phys_to_virt(page_to_phys(&paPages[iPage])) + PAGE_SIZE
                                ==  (uintptr_t)phys_to_virt(page_to_phys(&paPages[iPage + 1]))
                          &&        page_to_phys(&paPages[iPage]) + PAGE_SIZE
                                ==  page_to_phys(&paPages[iPage + 1]),
                          ("iPage=%i cPages=%u [0]=%#llx,%p [1]=%#llx,%p\n", iPage, cPages,
                           (long long)page_to_phys(&paPages[iPage]),     phys_to_virt(page_to_phys(&paPages[iPage])),
                           (long long)page_to_phys(&paPages[iPage + 1]), phys_to_virt(page_to_phys(&paPages[iPage + 1])) ));
            }

            SetPageReserved(&paPages[iPage]);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 4, 20) /** @todo find the exact kernel where change_page_attr was introduced. */
            if (pgprot_val(MY_PAGE_KERNEL_EXEC) != pgprot_val(PAGE_KERNEL))
                MY_CHANGE_PAGE_ATTR(&paPages[iPage], 1, MY_PAGE_KERNEL_EXEC);
#endif
        }
        *pPhys = page_to_phys(paPages);
        return phys_to_virt(page_to_phys(paPages));
    }

    return NULL;
}


/**
 * Frees memory allocated ysing RTMemContAlloc().
 *
 * @param   pv      Pointer to return from RTMemContAlloc().
 * @param   cb      The cb parameter passed to RTMemContAlloc().
 */
RTR0DECL(void) RTMemContFree(void *pv, size_t cb)
{
    if (pv)
    {
        int             cOrder;
        unsigned        cPages;
        unsigned        iPage;
        struct page    *paPages;

        /* validate */
        AssertMsg(!((uintptr_t)pv & PAGE_OFFSET_MASK), ("pv=%p\n", pv));
        Assert(cb > 0);

        /* calc order and get pages */
        cb = RT_ALIGN_Z(cb, PAGE_SIZE);
        cPages = cb >> PAGE_SHIFT;
        cOrder = CalcPowerOf2Order(cPages);
        paPages = virt_to_page(pv);

        /*
         * Restore page attributes freeing the pages.
         */
        for (iPage = 0; iPage < cPages; iPage++)
        {
            ClearPageReserved(&paPages[iPage]);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 4, 20) /** @todo find the exact kernel where change_page_attr was introduced. */
            if (pgprot_val(MY_PAGE_KERNEL_EXEC) != pgprot_val(PAGE_KERNEL))
                MY_CHANGE_PAGE_ATTR(&paPages[iPage], 1, PAGE_KERNEL);
#endif
        }
        __free_pages(paPages, cOrder);
    }
}

