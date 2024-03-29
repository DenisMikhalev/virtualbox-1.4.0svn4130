/* $Id: VMReq.cpp 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * VM - Virtual Machine
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
#define LOG_GROUP LOG_GROUP_VM
#include <VBox/mm.h>
#include <VBox/vmm.h>
#include "VMInternal.h"
#include <VBox/vm.h>

#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int  vmR3ReqProcessOne(PVM pVM, PVMREQ pReq);


/**
 * Allocate and queue a call request.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use VMR3ReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using VMR3ReqFree().
 *
 * @returns VBox status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pVM             The VM handle.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happends.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   cArgs           Number of arguments following in the ellipsis.
 *                          Not possible to pass 64-bit arguments!
 * @param   ...             Function arguments.
 */
VMR3DECL(int) VMR3ReqCall(PVM pVM, PVMREQ *ppReq, unsigned cMillies, PFNRT pfnFunction, unsigned cArgs, ...)
{
    va_list va;
    va_start(va, cArgs);
    int rc = VMR3ReqCallV(pVM, ppReq, cMillies, VMREQFLAGS_VBOX_STATUS, pfnFunction, cArgs, va);
    va_end(va);
    return rc;
}


/**
 * Allocate and queue a call request to a void function.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use VMR3ReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using VMR3ReqFree().
 *
 * @returns VBox status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pVM             The VM handle.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happends.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   cArgs           Number of arguments following in the ellipsis.
 *                          Not possible to pass 64-bit arguments!
 * @param   ...             Function arguments.
 */
VMR3DECL(int) VMR3ReqCallVoid(PVM pVM, PVMREQ *ppReq, unsigned cMillies, PFNRT pfnFunction, unsigned cArgs, ...)
{
    va_list va;
    va_start(va, cArgs);
    int rc = VMR3ReqCallV(pVM, ppReq, cMillies, VMREQFLAGS_VOID, pfnFunction, cArgs, va);
    va_end(va);
    return rc;
}


/**
 * Allocate and queue a call request to a void function.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use VMR3ReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using VMR3ReqFree().
 *
 * @returns VBox status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pVM             The VM handle.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happends, unless fFlags
 *                          contains VMREQFLAGS_NO_WAIT when it will be optional and always NULL.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   fFlags          A combination of the VMREQFLAGS values.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   cArgs           Number of arguments following in the ellipsis.
 *                          Not possible to pass 64-bit arguments!
 * @param   ...             Function arguments.
 */
VMR3DECL(int) VMR3ReqCallEx(PVM pVM, PVMREQ *ppReq, unsigned cMillies, unsigned fFlags, PFNRT pfnFunction, unsigned cArgs, ...)
{
    va_list va;
    va_start(va, cArgs);
    int rc = VMR3ReqCallV(pVM, ppReq, cMillies, fFlags, pfnFunction, cArgs, va);
    va_end(va);
    return rc;
}


/**
 * Allocate and queue a call request.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use VMR3ReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using VMR3ReqFree().
 *
 * @returns VBox status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pVM             The VM handle.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happends, unless fFlags
 *                          contains VMREQFLAGS_NO_WAIT when it will be optional and always NULL.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   fFlags          A combination of the VMREQFLAGS values.
 * @param   cArgs           Number of arguments following in the ellipsis.
 *                          Stuff which differs in size from uintptr_t is gonna make trouble, so don't try!
 * @param   Args            Argument vector.
 */
VMR3DECL(int) VMR3ReqCallV(PVM pVM, PVMREQ *ppReq, unsigned cMillies, unsigned fFlags, PFNRT pfnFunction, unsigned cArgs, va_list Args)
{
    LogFlow(("VMR3ReqCallV: cMillies=%d fFlags=%#x pfnFunction=%p cArgs=%d\n", cMillies, fFlags, pfnFunction, cArgs));

    /*
     * Check input.
     */
    if (!pfnFunction || !pVM || (fFlags & ~(VMREQFLAGS_RETURN_MASK | VMREQFLAGS_NO_WAIT)))
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }
    if (!(fFlags & VMREQFLAGS_NO_WAIT) || ppReq)
    {
        Assert(ppReq);
        *ppReq = NULL;
    }
    PVMREQ pReq = NULL;
    if (cArgs * sizeof(uintptr_t) > sizeof(pReq->u.Internal.aArgs))
    {
        AssertMsgFailed(("cArg=%d\n", cArgs));
        return VERR_TOO_MUCH_DATA;
    }

    /*
     * Allocate request
     */
    int rc = VMR3ReqAlloc(pVM, &pReq, VMREQTYPE_INTERNAL);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Initialize the request data.
     */
    pReq->fFlags         = fFlags;
    pReq->u.Internal.pfn = pfnFunction;
    pReq->u.Internal.cArgs = cArgs;
    for (unsigned iArg = 0; iArg < cArgs; iArg++)
        pReq->u.Internal.aArgs[iArg] = va_arg(Args, uintptr_t);

    /*
     * Queue the request and return.
     */
    rc = VMR3ReqQueue(pReq, cMillies);
    if (    VBOX_FAILURE(rc)
        && rc != VERR_TIMEOUT)
    {
        VMR3ReqFree(pReq);
        pReq = NULL;
    }
    if (!(fFlags & VMREQFLAGS_NO_WAIT))
    {
        *ppReq = pReq;
        LogFlow(("VMR3ReqCallV: returns %Vrc *ppReq=%p\n", rc, pReq));
    }
    else
        LogFlow(("VMR3ReqCallV: returns %Vrc\n", rc));
    Assert(rc != VERR_INTERRUPTED);
    return rc;
}


/**
 * Joins the list pList with whatever is linked up at *pHead.
 */
static void vmr3ReqJoinFreeSub(volatile PVMREQ *ppHead, PVMREQ pList)
{
    for (unsigned cIterations = 0;; cIterations++)
    {
        PVMREQ pHead = (PVMREQ)ASMAtomicXchgPtr((void * volatile *)ppHead, pList);
        if (!pHead)
            return;
        PVMREQ pTail = pHead;
        while (pTail->pNext)
            pTail = pTail->pNext;
        pTail->pNext = pList;
        if (ASMAtomicCmpXchgPtr((void * volatile *)ppHead, (void *)pHead, pList))
            return;
        pTail->pNext = NULL;
        if (ASMAtomicCmpXchgPtr((void * volatile *)ppHead, (void *)pHead, NULL))
            return;
        pList = pHead;
        Assert(cIterations != 32);
        Assert(cIterations != 64);
    }
}


/**
 * Joins the list pList with whatever is linked up at *pHead.
 */
static void vmr3ReqJoinFree(PVMINT pVMInt, PVMREQ pList)
{
    /*
     * Split the list if it's too long.
     */
    unsigned cReqs = 1;
    PVMREQ pTail = pList;
    while (pTail->pNext)
    {
        if (cReqs++ > 25)
        {
            const uint32_t i = pVMInt->iReqFree;
            vmr3ReqJoinFreeSub(&pVMInt->apReqFree[(i + 2) % ELEMENTS(pVMInt->apReqFree)], pTail->pNext);

            pTail->pNext = NULL;
            vmr3ReqJoinFreeSub(&pVMInt->apReqFree[(i + 2 + (i == pVMInt->iReqFree)) % ELEMENTS(pVMInt->apReqFree)], pTail->pNext);
            return;
        }
        pTail = pTail->pNext;
    }
    vmr3ReqJoinFreeSub(&pVMInt->apReqFree[(pVMInt->iReqFree + 2) % ELEMENTS(pVMInt->apReqFree)], pList);
}


/**
 * Allocates a request packet.
 *
 * The caller allocates a request packet, fills in the request data
 * union and queues the request.
 *
 * @returns VBox status code.
 *
 * @param   pVM             VM handle.
 * @param   ppReq           Where to store the pointer to the allocated packet.
 * @param   enmType         Package type.
 */
VMR3DECL(int) VMR3ReqAlloc(PVM pVM, PVMREQ *ppReq, VMREQTYPE enmType)
{
    /*
     * Validate input.
     */
    if (    enmType < VMREQTYPE_INVALID
        ||  enmType > VMREQTYPE_MAX)
    {
        AssertMsgFailed(("Invalid package type %d valid range %d-%d inclusivly.\n",
                         enmType, VMREQTYPE_INVALID + 1, VMREQTYPE_MAX - 1));
        return VERR_VM_REQUEST_INVALID_TYPE;
    }

    /*
     * Try get a recycled packet.
     * While this could all be solved with a single list with a lock, it's a sport
     * of mine to avoid locks.
     */
    int cTries = ELEMENTS(pVM->vm.s.apReqFree) * 2;
    while (--cTries >= 0)
    {
        PVMREQ volatile *ppHead = &pVM->vm.s.apReqFree[ASMAtomicIncU32(&pVM->vm.s.iReqFree) % ELEMENTS(pVM->vm.s.apReqFree)];
#if 0 /* sad, but this won't work safely because the reading of pReq->pNext. */
        PVMREQ pNext = NULL;
        PVMREQ pReq = *ppHead;
        if (    pReq
            &&  !ASMAtomicCmpXchgPtr((void * volatile *)ppHead, (pNext = pReq->pNext), pReq)
            &&  (pReq = *ppHead)
            &&  !ASMAtomicCmpXchgPtr((void * volatile *)ppHead, (pNext = pReq->pNext), pReq))
            pReq = NULL;
        if (pReq)
        {
            Assert(pReq->pNext == pNext); NOREF(pReq);
#else
        PVMREQ pReq = (PVMREQ)ASMAtomicXchgPtr((void * volatile *)ppHead, NULL);
        if (pReq)
        {
            PVMREQ pNext = pReq->pNext;
            if (    pNext
                &&  !ASMAtomicCmpXchgPtr((void * volatile *)ppHead, pNext, NULL))
            {
                STAM_COUNTER_INC(&pVM->vm.s.StatReqAllocRaces);
                vmr3ReqJoinFree(&pVM->vm.s, pReq->pNext);
            }
#endif
            ASMAtomicDecU32(&pVM->vm.s.cReqFree);

            /*
             * Make sure the event sem is not signaled.
             */
            if (!pReq->fEventSemClear)
            {
                int rc = RTSemEventWait(pReq->EventSem, 0);
                if (rc != VINF_SUCCESS && rc != VERR_TIMEOUT)
                {
                    /*
                     * This shall not happen, but if it does we'll just destroy
                     * the semaphore and create a new one.
                     */
                    AssertMsgFailed(("rc=%Vrc from RTSemEventWait(%#x).\n", rc, pReq->EventSem));
                    RTSemEventDestroy(pReq->EventSem);
                    rc = RTSemEventCreate(&pReq->EventSem);
                    AssertRC(rc);
                    if (VBOX_FAILURE(rc))
                        return rc;
                }
                pReq->fEventSemClear = true;
            }
            else
                Assert(RTSemEventWait(pReq->EventSem, 0) == VERR_TIMEOUT);

            /*
             * Initialize the packet and return it.
             */
            Assert(pReq->enmType == VMREQTYPE_INVALID);
            Assert(pReq->enmState == VMREQSTATE_FREE);
            Assert(pReq->pVM == pVM);
            ASMAtomicXchgSize(&pReq->pNext, NULL);
            pReq->enmState = VMREQSTATE_ALLOCATED;
            pReq->iStatus  = VERR_VM_REQUEST_STATUS_STILL_PENDING;
            pReq->fFlags   = VMREQFLAGS_VBOX_STATUS;
            pReq->enmType  = enmType;

            *ppReq = pReq;
            STAM_COUNTER_INC(&pVM->vm.s.StatReqAllocRecycled);
            LogFlow(("VMR3ReqAlloc: returns VINF_SUCCESS *ppReq=%p recycled\n", pReq));
            return VINF_SUCCESS;
        }
    }

    /*
     * Ok allocate one.
     */
    PVMREQ pReq = (PVMREQ)MMR3HeapAlloc(pVM, MM_TAG_VM_REQ, sizeof(*pReq));
    if (!pReq)
        return VERR_NO_MEMORY;

    /*
     * Create the semaphore.
     */
    int rc = RTSemEventCreate(&pReq->EventSem);
    AssertRC(rc);
    if (VBOX_FAILURE(rc))
    {
        MMR3HeapFree(pReq);
        return rc;
    }

    /*
     * Initialize the packet and return it.
     */
    pReq->pNext    = NULL;
    pReq->pVM      = pVM;
    pReq->enmState = VMREQSTATE_ALLOCATED;
    pReq->iStatus  = VERR_VM_REQUEST_STATUS_STILL_PENDING;
    pReq->fEventSemClear = true;
    pReq->fFlags   = VMREQFLAGS_VBOX_STATUS;
    pReq->enmType  = enmType;

    *ppReq = pReq;
    STAM_COUNTER_INC(&pVM->vm.s.StatReqAllocNew);
    LogFlow(("VMR3ReqAlloc: returns VINF_SUCCESS *ppReq=%p new\n", pReq));
    return VINF_SUCCESS;
}


/**
 * Free a request packet.
 *
 * @returns VBox status code.
 *
 * @param   pReq            Package to free.
 * @remark  The request packet must be in allocated or completed state!
 */
VMR3DECL(int) VMR3ReqFree(PVMREQ pReq)
{
    /*
     * Ignore NULL (all free functions should do this imho).
     */
    if (!pReq)
        return VINF_SUCCESS;

    STAM_COUNTER_INC(&pReq->pVM->vm.s.StatReqFree);

    /*
     * Check packet state.
     */
    switch (pReq->enmState)
    {
        case VMREQSTATE_ALLOCATED:
        case VMREQSTATE_COMPLETED:
            break;
        default:
            AssertMsgFailed(("Invalid state %d!\n", pReq->enmState));
            return VERR_VM_REQUEST_STATE;
    }

    /*
     * Make it a free packet and put it into one of the free packet lists.
     */
    pReq->enmState = VMREQSTATE_FREE;
    pReq->iStatus  = VERR_VM_REQUEST_STATUS_FREED;
    pReq->enmType  = VMREQTYPE_INVALID;

    PVM pVM = pReq->pVM;
    if (pVM->vm.s.cReqFree < 128)
    {
        ASMAtomicIncU32(&pVM->vm.s.cReqFree);
        PVMREQ volatile *ppHead = &pVM->vm.s.apReqFree[ASMAtomicIncU32(&pVM->vm.s.iReqFree) % ELEMENTS(pVM->vm.s.apReqFree)];
        PVMREQ pNext;
        do
        {
            pNext = *ppHead;
            ASMAtomicXchgPtr((void * volatile *)&pReq->pNext, pNext);
        } while (!ASMAtomicCmpXchgPtr((void * volatile *)ppHead, (void *)pReq, (void *)pNext));
    }
    else
    {
        STAM_COUNTER_INC(&pReq->pVM->vm.s.StatReqFreeOverflow);
        RTSemEventDestroy(pReq->EventSem);
        MMR3HeapFree(pReq);
    }
    return VINF_SUCCESS;
}


/**
 * Queue a request.
 *
 * The quest must be allocated using VMR3ReqAlloc() and contain
 * all the required data.
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use VMR3ReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 *
 * @returns VBox status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pReq            The request to queue.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 */
VMR3DECL(int) VMR3ReqQueue(PVMREQ pReq, unsigned cMillies)
{
    LogFlow(("VMR3ReqQueue: pReq=%p cMillies=%d\n", pReq, cMillies));
    /*
     * Verify the supplied package.
     */
    if (pReq->enmState != VMREQSTATE_ALLOCATED)
    {
        AssertMsgFailed(("Invalid state %d\n", pReq->enmState));
        return VERR_VM_REQUEST_STATE;
    }
    if (   !pReq->pVM
        ||  pReq->pNext
        ||  !pReq->EventSem)
    {
        AssertMsgFailed(("Invalid request package! Anyone cooking their own packages???\n"));
        return VERR_VM_REQUEST_INVALID_PACKAGE;
    }
    if (    pReq->enmType < VMREQTYPE_INVALID
        || pReq->enmType > VMREQTYPE_MAX)
    {
        AssertMsgFailed(("Invalid package type %d valid range %d-%d inclusivly. This was verified on alloc too...\n",
                         pReq->enmType, VMREQTYPE_INVALID + 1, VMREQTYPE_MAX - 1));
        return VERR_VM_REQUEST_INVALID_TYPE;
    }

    int rc = VINF_SUCCESS;
    if (pReq->pVM->NativeThreadEMT != RTThreadNativeSelf())
    {
        /*
         * Insert it.
         */
        pReq->enmState = VMREQSTATE_QUEUED;
        PVMREQ pNext;
        do
        {
            pNext = pReq->pVM->vm.s.pReqs;
            pReq->pNext = pNext;
        } while (!ASMAtomicCmpXchgPtr((void * volatile *)&pReq->pVM->vm.s.pReqs, (void *)pReq, (void *)pNext));

        /*
         * Notify EMT.
         */
        VM_FF_SET(pReq->pVM, VM_FF_REQUEST);
        VMR3NotifyFF(pReq->pVM, false);

        /*
         * Wait and return.
         */
        if (!(pReq->fFlags & VMREQFLAGS_NO_WAIT))
            rc = VMR3ReqWait(pReq, cMillies);
        LogFlow(("VMR3ReqQueue: returns %Vrc\n", rc));
    }
    else
    {
        /*
         * The requester was EMT, just execute it.
         */
        pReq->enmState = VMREQSTATE_QUEUED;
        rc = vmR3ReqProcessOne(pReq->pVM, pReq);
        LogFlow(("VMR3ReqQueue: returns %Vrc (processed)\n", rc));
    }
    return rc;
}


/**
 * Wait for a request to be completed.
 *
 * @returns VBox status code.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pReq            The request to wait for.
 * @param   cMillies        Number of milliseconds to wait.
 *                          Use RT_INDEFINITE_WAIT to only wait till it's completed.
 */
VMR3DECL(int) VMR3ReqWait(PVMREQ pReq, unsigned cMillies)
{
    LogFlow(("VMR3ReqWait: pReq=%p cMillies=%d\n", pReq, cMillies));

    /*
     * Verify the supplied package.
     */
    if (    pReq->enmState != VMREQSTATE_QUEUED
        &&  pReq->enmState != VMREQSTATE_PROCESSING
        &&  pReq->enmState != VMREQSTATE_COMPLETED)
    {
        AssertMsgFailed(("Invalid state %d\n", pReq->enmState));
        return VERR_VM_REQUEST_STATE;
    }
    if (    !pReq->pVM
        ||  !pReq->EventSem)
    {
        AssertMsgFailed(("Invalid request package! Anyone cooking their own packages???\n"));
        return VERR_VM_REQUEST_INVALID_PACKAGE;
    }
    if (    pReq->enmType < VMREQTYPE_INVALID
        ||  pReq->enmType > VMREQTYPE_MAX)
    {
        AssertMsgFailed(("Invalid package type %d valid range %d-%d inclusivly. This was verified on alloc and queue too...\n",
                         pReq->enmType, VMREQTYPE_INVALID + 1, VMREQTYPE_MAX - 1));
        return VERR_VM_REQUEST_INVALID_TYPE;
    }

    /*
     * Check for deadlock condition
     */
    AssertMsg(!VMMR3LockIsOwner(pReq->pVM), ("Waiting for EMT to process a request, but we own the global VM lock!?!?!?!\n"));

    /*
     * Wait on the package.
     */
    int rc;
    if (cMillies != RT_INDEFINITE_WAIT)
        rc = RTSemEventWait(pReq->EventSem, cMillies);
    else
    {
        do
        {
            rc = RTSemEventWait(pReq->EventSem, RT_INDEFINITE_WAIT);
            Assert(rc != VERR_TIMEOUT);
        } while (pReq->enmState != VMREQSTATE_COMPLETED);
    }
    if (VBOX_SUCCESS(rc))
        ASMAtomicXchgSize(&pReq->fEventSemClear, true);
    if (pReq->enmState == VMREQSTATE_COMPLETED)
        rc = VINF_SUCCESS;
    LogFlow(("VMR3ReqWait: returns %Vrc\n", rc));
    Assert(rc != VERR_INTERRUPTED);
    return rc;
}


/**
 * Process pending request(s).
 *
 * This function is called from a forced action handler in
 * the EMT.
 *
 * @returns VBox status code.
 *
 * @param   pVM         VM handle.
 */
VMR3DECL(int) VMR3ReqProcess(PVM pVM)
{
    LogFlow(("VMR3ReqProcess: (enmVMState=%d)\n", pVM->enmVMState));

    /*
     * Process loop.
     *
     * We do not repeat the outer loop if we've got an informationtional status code
     * since that code needs processing by our caller.
     */
    int rc = VINF_SUCCESS;
    while (rc <= VINF_SUCCESS)
    {
        /*
         * Get pending requests.
         */
        VM_FF_CLEAR(pVM, VM_FF_REQUEST);
        PVMREQ pReqs = (PVMREQ)ASMAtomicXchgPtr((void * volatile *)&pVM->vm.s.pReqs, NULL);
        if (!pReqs)
            break;

        /*
         * Reverse the list to process it in FIFO order.
         */
        PVMREQ pReq = pReqs;
        if (pReq->pNext)
            Log2(("VMR3ReqProcess: 2+ requests: %p %p %p\n", pReq, pReq->pNext, pReq->pNext->pNext));
        pReqs = NULL;
        while (pReq)
        {
            Assert(pReq->enmState == VMREQSTATE_QUEUED);
            Assert(pReq->pVM == pVM);
            PVMREQ pCur = pReq;
            pReq = pReq->pNext;
            pCur->pNext = pReqs;
            pReqs = pCur;
        }


        /*
         * Process the requests.
         *
         * Since this is a FF worker certain rules applies to the
         * status codes. See the EM section in VBox/err.h and EM.cpp for details.
         */
        while (pReqs)
        {
            /* Unchain the first request and advance the list. */
            pReq = pReqs;
            pReqs = pReqs->pNext;
            pReq->pNext = NULL;

            /* Process the request */
            int rc2 = vmR3ReqProcessOne(pVM, pReq);

            /*
             * The status code handling extremely important yet very fragile. Should probably
             * look for a better way of communicating status changes to EM...
             */
            if (    rc2 >= VINF_EM_FIRST
                &&  rc2 <= VINF_EM_LAST
                &&  (   rc == VINF_SUCCESS
                     || rc2 < rc) )
                rc = rc2;
        }
    }

    LogFlow(("VMR3ReqProcess: returns %Vrc (enmVMState=%d)\n", rc, pVM->enmVMState));
    return rc;
}


/**
 * Process one request.
 *
 * @returns VBox status code.
 *
 * @param   pVM         VM handle.
 * @param   pReq        Request packet to process.
 */
static int  vmR3ReqProcessOne(PVM pVM, PVMREQ pReq)
{
    LogFlow(("vmR3ReqProcessOne: pReq=%p type=%d fFlags=%#x\n", pReq, pReq->enmType, pReq->fFlags));

    /*
     * Process the request.
     */
    Assert(pReq->enmState == VMREQSTATE_QUEUED);
    pReq->enmState = VMREQSTATE_PROCESSING;
    int     rcRet = VINF_SUCCESS;           /* the return code of this function. */
    int     rcReq = VERR_NOT_IMPLEMENTED;   /* the request status. */
    switch (pReq->enmType)
    {
        /*
         * A packed down call frame.
         */
        case VMREQTYPE_INTERNAL:
        {
            uintptr_t *pauArgs = &pReq->u.Internal.aArgs[0];
            union
            {
                PFNRT pfn;
                DECLCALLBACKMEMBER(int, pfn00)(void);
                DECLCALLBACKMEMBER(int, pfn01)(uintptr_t);
                DECLCALLBACKMEMBER(int, pfn02)(uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn03)(uintptr_t, uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn04)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn05)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn06)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn07)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn08)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn09)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn10)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn11)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn12)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
            } u;
            u.pfn = pReq->u.Internal.pfn;
#ifdef RT_ARCH_AMD64
            switch (pReq->u.Internal.cArgs)
            {
                case 0:  rcRet = u.pfn00(); break;
                case 1:  rcRet = u.pfn01(pauArgs[0]); break;
                case 2:  rcRet = u.pfn02(pauArgs[0], pauArgs[1]); break;
                case 3:  rcRet = u.pfn03(pauArgs[0], pauArgs[1], pauArgs[2]); break;
                case 4:  rcRet = u.pfn04(pauArgs[0], pauArgs[1], pauArgs[2], pauArgs[3]); break;
                case 5:  rcRet = u.pfn05(pauArgs[0], pauArgs[1], pauArgs[2], pauArgs[3], pauArgs[4]); break;
                case 6:  rcRet = u.pfn06(pauArgs[0], pauArgs[1], pauArgs[2], pauArgs[3], pauArgs[4], pauArgs[5]); break;
                case 7:  rcRet = u.pfn07(pauArgs[0], pauArgs[1], pauArgs[2], pauArgs[3], pauArgs[4], pauArgs[5], pauArgs[6]); break;
                case 8:  rcRet = u.pfn08(pauArgs[0], pauArgs[1], pauArgs[2], pauArgs[3], pauArgs[4], pauArgs[5], pauArgs[6], pauArgs[7]); break;
                case 9:  rcRet = u.pfn09(pauArgs[0], pauArgs[1], pauArgs[2], pauArgs[3], pauArgs[4], pauArgs[5], pauArgs[6], pauArgs[7], pauArgs[8]); break;
                case 10: rcRet = u.pfn10(pauArgs[0], pauArgs[1], pauArgs[2], pauArgs[3], pauArgs[4], pauArgs[5], pauArgs[6], pauArgs[7], pauArgs[8], pauArgs[9]); break;
                case 11: rcRet = u.pfn11(pauArgs[0], pauArgs[1], pauArgs[2], pauArgs[3], pauArgs[4], pauArgs[5], pauArgs[6], pauArgs[7], pauArgs[8], pauArgs[9], pauArgs[10]); break;
                case 12: rcRet = u.pfn12(pauArgs[0], pauArgs[1], pauArgs[2], pauArgs[3], pauArgs[4], pauArgs[5], pauArgs[6], pauArgs[7], pauArgs[8], pauArgs[9], pauArgs[10], pauArgs[11]); break;
                default:
                    AssertReleaseMsgFailed(("cArgs=%d\n", pReq->u.Internal.cArgs));
                    rcRet = rcReq = VERR_INTERNAL_ERROR;
                    break;
            }
#else /* x86: */
            size_t cbArgs = pReq->u.Internal.cArgs * sizeof(uintptr_t);
# ifdef __GNUC__
            __asm__ __volatile__("movl  %%esp, %%edx\n\t"
                                 "subl  %2, %%esp\n\t"
                                 "andl  $0xfffffff0, %%esp\n\t"
                                 "shrl  $2, %2\n\t"
                                 "movl  %%esp, %%edi\n\t"
                                 "rep movsl\n\t"
                                 "movl  %%edx, %%edi\n\t"
                                 "call  *%%eax\n\t"
                                 "mov   %%edi, %%esp\n\t"
                                 : "=a" (rcRet),
                                   "=S" (pauArgs),
                                   "=c" (cbArgs)
                                 : "0" (u.pfn),
                                   "1" (pauArgs),
                                   "2" (cbArgs)
                                 : "edi", "edx");
# else
            __asm
            {
                xor     edx, edx        /* just mess it up. */
                mov     eax, u.pfn
                mov     ecx, cbArgs
                shr     ecx, 2
                mov     esi, pauArgs
                mov     ebx, esp
                sub     esp, cbArgs
                and     esp, 0xfffffff0
                mov     edi, esp
                rep movsd
                call    eax
                mov     esp, ebx
                mov     rcRet, eax
            }
# endif
#endif /* x86 */
            if ((pReq->fFlags & (VMREQFLAGS_RETURN_MASK)) == VMREQFLAGS_VOID)
                rcRet = VINF_SUCCESS;
            rcReq = rcRet;
            break;
        }

        default:
            AssertMsgFailed(("pReq->enmType=%d\n", pReq->enmType));
            rcReq = VERR_NOT_IMPLEMENTED;
            break;
    }

    /*
     * Complete the request.
     */
    pReq->iStatus  = rcReq;
    pReq->enmState = VMREQSTATE_COMPLETED;
    if (pReq->fFlags & VMREQFLAGS_NO_WAIT)
    {
        /* Free the packet, nobody is waiting. */
        LogFlow(("vmR3ReqProcessOne: Completed request %p: rcReq=%Vrc rcRet=%Vrc - freeing it\n",
                 pReq, rcReq, rcRet));
        VMR3ReqFree(pReq);
    }
    else
    {
        /* Notify the waiter and him free up the packet. */
        LogFlow(("vmR3ReqProcessOne: Completed request %p: rcReq=%Vrc rcRet=%Vrc - notifying waiting thread\n",
                 pReq, rcReq, rcRet));
        ASMAtomicXchgSize(&pReq->fEventSemClear, false);
        int rc2 = RTSemEventSignal(pReq->EventSem);
        if (VBOX_FAILURE(rc2))
        {
            AssertRC(rc2);
            rcRet = rc2;
        }
    }
    return rcRet;
}




