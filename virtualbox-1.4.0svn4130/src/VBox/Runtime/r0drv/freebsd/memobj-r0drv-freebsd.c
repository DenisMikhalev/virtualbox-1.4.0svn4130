/* $Id: memobj-r0drv-freebsd.c 4049 2007-08-07 01:45:14Z vboxsync $ */
/** @file
 * innotek Portable Runtime - Ring-0 Memory Objects, FreeBSD.
 */

/*
 * Copyright (c) 2007 knut st. osmundsen <bird-src-spam@anduin.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-freebsd-kernel.h"

#include <iprt/memobj.h>
#include <iprt/mem.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/param.h>
#include <iprt/process.h>
#include "internal/memobj.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The FreeBSD version of the memory object structure.
 */
typedef struct RTR0MEMOBJFREEBSD
{
    /** The core structure. */
    RTR0MEMOBJINTERNAL  Core;
    /** The VM object associated with the allocation. */   
    vm_object_t         pObject;
    /** the VM object associated with the mapping.
     * In mapping mem object, this is the shadow object?
     * In a allocation/enter mem object, this is the shared object we constructed (contig, perhaps alloc). */
    vm_object_t         pMappingObject;
} RTR0MEMOBJFREEBSD, *PRTR0MEMOBJFREEBSD;


MALLOC_DEFINE(M_IPRTMOBJ, "iprtmobj", "innotek Portable Runtime - R0MemObj");

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/


int rtR0MemObjNativeFree(RTR0MEMOBJ pMem)
{
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)pMem;
    int rc;

    switch (pMemFreeBSD->Core.enmType)
    {
        case RTR0MEMOBJTYPE_CONT:
            contigfree(pMemFreeBSD->Core.pv, pMemFreeBSD->Core.cb, M_IPRTMOBJ);
            if (pMemFreeBSD->pMappingObject)
            {
                rc = vm_map_remove(kernel_map,
                                   (vm_offset_t)pMemFreeBSD->Core.pv, 
                                   (vm_offset_t)pMemFreeBSD->Core.pv + pMemFreeBSD->Core.cb);
                AssertMsg(rc == KERN_SUCCESS, ("%#x", rc));
            }
            break;

        case RTR0MEMOBJTYPE_PAGE:
            if (pMemFreeBSD->pObject)
            {
                rc = vm_map_remove(kernel_map,
                                   (vm_offset_t)pMemFreeBSD->Core.pv, 
                                   (vm_offset_t)pMemFreeBSD->Core.pv + pMemFreeBSD->Core.cb);
                AssertMsg(rc == KERN_SUCCESS, ("%#x", rc));
            }
            else
            {
                free(pMemFreeBSD->Core.pv, M_IPRTMOBJ);
                if (pMemFreeBSD->pMappingObject)
                {
                    rc = vm_map_remove(kernel_map,
                                       (vm_offset_t)pMemFreeBSD->Core.pv, 
                                       (vm_offset_t)pMemFreeBSD->Core.pv + pMemFreeBSD->Core.cb);
                    AssertMsg(rc == KERN_SUCCESS, ("%#x", rc));
                }
            }
            break;

        case RTR0MEMOBJTYPE_LOCK:
        {
            vm_map_t pMap = kernel_map;
            if (pMemFreeBSD->Core.u.Lock.R0Process != NIL_RTR0PROCESS)
                pMap = &((struct proc *)pMemFreeBSD->Core.u.Lock.R0Process)->p_vmspace->vm_map;
            rc = vm_map_unwire(pMap, 
                               (vm_offset_t)pMemFreeBSD->Core.pv, 
                               (vm_offset_t)pMemFreeBSD->Core.pv + pMemFreeBSD->Core.cb, 
                               VM_MAP_WIRE_SYSTEM | VM_MAP_WIRE_NOHOLES);
            AssertMsg(rc == KERN_SUCCESS, ("%#x", rc));
            break;
        }

        case RTR0MEMOBJTYPE_RES_VIRT:
        {
            vm_map_t pMap = kernel_map;
            if (pMemFreeBSD->Core.u.Lock.R0Process != NIL_RTR0PROCESS)
                pMap = &((struct proc *)pMemFreeBSD->Core.u.Lock.R0Process)->p_vmspace->vm_map;
            rc = vm_map_remove(pMap,
                               (vm_offset_t)pMemFreeBSD->Core.pv, 
                               (vm_offset_t)pMemFreeBSD->Core.pv + pMemFreeBSD->Core.cb);
            AssertMsg(rc == KERN_SUCCESS, ("%#x", rc));
            break;
        }
        
        case RTR0MEMOBJTYPE_MAPPING:
        {
            /** @todo Figure out mapping... */
        }
            
        /* unused: */    
        case RTR0MEMOBJTYPE_LOW:
        case RTR0MEMOBJTYPE_PHYS:
        default:
            AssertMsgFailed(("enmType=%d\n", pMemFreeBSD->Core.enmType));
            return VERR_INTERNAL_ERROR;
    }
    
    Assert(!pMemFreeBSD->pMappingObject);

    return VINF_SUCCESS;
}


int rtR0MemObjNativeAllocPage(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    int rc;

    /* create the object. */
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(sizeof(*pMemFreeBSD), RTR0MEMOBJTYPE_PAGE, NULL, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;
    
    /* 
     * We've two options here both expressed nicely by how kld allocates 
     * memory for the module bits: 
     *      http://fxr.watson.org/fxr/source/kern/link_elf.c?v=RELENG62#L701 
     */
#if 0
    pMemFreeBSD->Core.pv = malloc(cb, M_IPRTMOBJ, M_ZERO);
    if (pMemFreeBSD->Core.pv)
    {
        *ppMem = &pMemFreeBSD->Core;
        return VINF_SUCCESS;
    }
    rc = VERR_NO_MEMORY;
    NOREF(fExecutable);

#else
    pMemFreeBSD->pObject = vm_object_allocate(OBJT_DEFAULT, cb >> PAGE_SHIFT);
    if (pMemFreeBSD->pObject)
    {
        vm_offset_t MapAddress = vm_map_min(kernel_map);
        rc = vm_map_find(kernel_map,                    /* map */ 
                         pMemFreeBSD->pObject,          /* object */
                         0,                             /* offset */
                         &MapAddress,                   /* addr (IN/OUT) */
                         cb,                            /* length */
                         TRUE,                          /* find_space */
                         fExecutable                    /* protection */
                         ? VM_PROT_ALL
                         : VM_PROT_RW,
                         VM_PROT_ALL,                   /* max(_prot) */
                         FALSE);                        /* cow (copy-on-write) */
        if (rc == KERN_SUCCESS)
        {
            rc = vm_map_wire(kernel_map,                /* map */
                             MapAddress,                /* start */
                             MapAddress + cb,           /* end */
                             VM_MAP_WIRE_SYSTEM | VM_MAP_WIRE_NOHOLES);
            if (rc == KERN_SUCCESS)
            {
                pMemFreeBSD->Core.pv = (void *)MapAddress;
                *ppMem = &pMemFreeBSD->Core;
                return VINF_SUCCESS;
            }
            
           vm_map_remove(kernel_map,
                         MapAddress,
                         MapAddress + cb);
        }
        else
            vm_object_deallocate(pMemFreeBSD->pObject);
        rc = VERR_NO_MEMORY; /** @todo fix translation (borrow from darwin) */
    }
    else
        rc = VERR_NO_MEMORY;
#endif
    
    rtR0MemObjDelete(&pMemFreeBSD->Core);
    return rc;
}


int rtR0MemObjNativeAllocLow(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    /*
     * Try a Alloc first and see if we get luck, if not try contigmalloc.
     * Might wish to try find our own pages or something later if this 
     * turns into a problemspot on AMD64 boxes. 
     */
    int rc = rtR0MemObjNativeAllocPage(ppMem, cb, fExecutable);
    if (RT_SUCCESS(rc))
    {
        size_t iPage = cb >> PAGE_SHIFT;
        while (iPage-- > 0)
            if (rtR0MemObjNativeGetPagePhysAddr(*ppMem, iPage) > (_4G - PAGE_SIZE))
            {
                RTR0MemObjFree(*ppMem, false);
                *ppMem = NULL;
                rc = VERR_NO_MEMORY;
                break;
            }
    }
    if (RT_FAILURE(rc))
        rc = rtR0MemObjNativeAllocCont(ppMem, cb, fExecutable);
    return rc;
}


int rtR0MemObjNativeAllocCont(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    /* create the object. */
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(sizeof(*pMemFreeBSD), RTR0MEMOBJTYPE_CONT, NULL, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;

    /* do the allocation. */
    pMemFreeBSD->Core.pv = contigmalloc(cb,                   /* size */
                                        M_IPRTMOBJ,           /* type */
                                        M_NOWAIT | M_ZERO,    /* flags */
                                        0,                    /* lowest physical address*/
                                        _4G-1,                /* highest physical address */
                                        PAGE_SIZE,            /* alignment. */
                                        0);                   /* boundrary */
    if (pMemFreeBSD->Core.pv)
    {
        pMemFreeBSD->Core.u.Cont.Phys = vtophys(pMemFreeBSD->Core.pv);
        *ppMem = &pMemFreeBSD->Core;
        return VINF_SUCCESS;
    }
    
    NOREF(fExecutable);
    rtR0MemObjDelete(&pMemFreeBSD->Core);
    return VERR_NO_MEMORY;
}


int rtR0MemObjNativeAllocPhys(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest)
{
    /** @todo check if there is a more appropriate API somewhere.. */
    
    /* create the object. */
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(sizeof(*pMemFreeBSD), RTR0MEMOBJTYPE_CONT, NULL, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;

    /* do the allocation. */
    pMemFreeBSD->Core.pv = contigmalloc(cb,                   /* size */
                                        M_IPRTMOBJ,           /* type */
                                        M_NOWAIT | M_ZERO,    /* flags */
                                        0,                    /* lowest physical address*/
                                        PhysHighest,          /* highest physical address */
                                        PAGE_SIZE,            /* alignment. */
                                        0);                   /* boundrary */
    if (pMemFreeBSD->Core.pv)
    {
        pMemFreeBSD->Core.u.Cont.Phys = vtophys(pMemFreeBSD->Core.pv);
        *ppMem = &pMemFreeBSD->Core;
        return VINF_SUCCESS;
    }
    
    rtR0MemObjDelete(&pMemFreeBSD->Core);
    return VERR_NO_MEMORY;
}


int rtR0MemObjNativeEnterPhys(PPRTR0MEMOBJINTERNAL ppMem, RTHCPHYS Phys, size_t cb)
{
    /* create the object. */
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(sizeof(*pMemFreeBSD), RTR0MEMOBJTYPE_PHYS, NULL, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;

    /* there is no allocation here, it needs to be mapped somewhere first. */
    pMemFreeBSD->Core.u.Phys.fAllocated = false;
    pMemFreeBSD->Core.u.Phys.PhysBase = Phys;
    *ppMem = &pMemFreeBSD->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeLockUser(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb, RTR0PROCESS R0Process)
{
    int rc;

    /* create the object. */
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(sizeof(*pMemFreeBSD), RTR0MEMOBJTYPE_LOCK, pv, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;
    
    /*
     * We could've used vslock here, but we don't wish to be subject to 
     * resource usage restrictions, so we'll call vm_map_wire directly.
     */
    rc = vm_map_wire(&((struct proc *)R0Process)->p_vmspace->vm_map, /* the map */ 
                     (vm_offset_t)pv,                               /* start */
                     (vm_offset_t)pv + cb,                          /* end */
                     VM_MAP_WIRE_SYSTEM | VM_MAP_WIRE_NOHOLES);     /* flags - SYSTEM? */
    if (rc == KERN_SUCCESS)
    {
        pMemFreeBSD->Core.u.Lock.R0Process = R0Process;
        *ppMem = &pMemFreeBSD->Core;
        return VINF_SUCCESS;
    }
    rtR0MemObjDelete(&pMemFreeBSD->Core);
    return VERR_NO_MEMORY;/** @todo fix mach -> vbox error conversion for freebsd. */
}


int rtR0MemObjNativeLockKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb)
{
    int rc;

    /* create the object. */
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(sizeof(*pMemFreeBSD), RTR0MEMOBJTYPE_LOCK, pv, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;

    /* lock the memory */
    rc = vm_map_wire(kernel_map,                                    /* the map */ 
                     (vm_offset_t)pv,                               /* start */
                     (vm_offset_t)pv + cb,                          /* end */
                     VM_MAP_WIRE_SYSTEM | VM_MAP_WIRE_NOHOLES);     /* flags - SYSTEM? */
    if (rc == KERN_SUCCESS)
    {
        pMemFreeBSD->Core.u.Lock.R0Process = NIL_RTR0PROCESS;
        *ppMem = &pMemFreeBSD->Core;
        return VINF_SUCCESS;
    }
    rtR0MemObjDelete(&pMemFreeBSD->Core);
    return VERR_NO_MEMORY;/** @todo fix mach -> vbox error conversion for freebsd. */
}


/**
 * Worker for the two virtual address space reservers.
 * 
 * We're leaning on the examples provided by mmap and vm_mmap in vm_mmap.c here.
 */
static int rtR0MemObjNativeReserveInMap(PPRTR0MEMOBJINTERNAL ppMem, void *pvFixed, size_t cb, size_t uAlignment, RTR0PROCESS R0Process, vm_map_t pMap)
{
    int rc;
    
    /* 
     * The pvFixed address range must be within the VM space when specified.
     */
    if (pvFixed != (void *)-1
        && (    (vm_offset_t)pvFixed      < vm_map_min(pMap)
            ||  (vm_offset_t)pvFixed + cb > vm_map_max(pMap)))
        return VERR_INVALID_PARAMETER;
    
    /* 
     * Create the object. 
     */
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(sizeof(*pMemFreeBSD), RTR0MEMOBJTYPE_RES_VIRT, NULL, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;

    /*
     * Allocate an empty VM object and map it into the requested map.
     */
    pMemFreeBSD->pObject = vm_object_allocate(OBJT_DEFAULT, cb >> PAGE_SHIFT);
    if (pMemFreeBSD->pObject)
    {
        vm_offset_t MapAddress = pvFixed != (void *)-1
                               ? (vm_offset_t)pvFixed
                               : vm_map_min(kernel_map);
        if (pvFixed)
            vm_map_remove(pMap,
                          MapAddress,
                          MapAddress + cb);
                               
        rc = vm_map_find(pMap,                          /* map */ 
                         pMemFreeBSD->pObject,          /* object */
                         0,                             /* offset */
                         &MapAddress,                   /* addr (IN/OUT) */
                         cb,                            /* length */
                         pvFixed == (void *)-1,         /* find_space */
                         VM_PROT_NONE,                  /* protection */
                         VM_PROT_ALL,                   /* max(_prot) ?? */
                         FALSE);                        /* cow (copy-on-write) */
        if (rc == KERN_SUCCESS)
        {
            if (R0Process != NIL_RTR0PROCESS)
            {    
                rc = vm_map_inherit(pMap,
                                    MapAddress,
                                    MapAddress + cb,
                                    VM_INHERIT_SHARE);
                AssertMsg(rc == KERN_SUCCESS, ("%#x\n", rc));
            }
            pMemFreeBSD->Core.pv = (void *)MapAddress;
            pMemFreeBSD->Core.u.ResVirt.R0Process = R0Process;
            *ppMem = &pMemFreeBSD->Core;
            return VINF_SUCCESS;
        }
        vm_object_deallocate(pMemFreeBSD->pObject);
        rc = VERR_NO_MEMORY; /** @todo fix translation (borrow from darwin) */
    }
    else
        rc = VERR_NO_MEMORY;
    rtR0MemObjDelete(&pMemFreeBSD->Core);
    return rc;
    
}

int rtR0MemObjNativeReserveKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pvFixed, size_t cb, size_t uAlignment)
{
    return rtR0MemObjNativeReserveInMap(ppMem, pvFixed, cb, uAlignment, NIL_RTR0PROCESS, kernel_map);
}


int rtR0MemObjNativeReserveUser(PPRTR0MEMOBJINTERNAL ppMem, void *pvFixed, size_t cb, size_t uAlignment, RTR0PROCESS R0Process)
{
    return rtR0MemObjNativeReserveInMap(ppMem, pvFixed, cb, uAlignment, R0Process,
                                        &((struct proc *)R0Process)->p_vmspace->vm_map);
}


int rtR0MemObjNativeMapKernel(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJ pMemToMap, void *pvFixed, size_t uAlignment, unsigned fProt)
{
    AssertMsgReturn(pvFixed == (void *)-1, ("%p\n", pvFixed), VERR_NOT_SUPPORTED);

/* Phys: see pmap_mapdev in i386/i386/pmap.c (http://fxr.watson.org/fxr/source/i386/i386/pmap.c?v=RELENG62#L2860) */
    
#if 0
/** @todo finish the implementation. */

    int rc;
    void *pvR0 = NULL;
    PRTR0MEMOBJFREEBSD pMemToMapOs2 = (PRTR0MEMOBJFREEBSD)pMemToMap;
    switch (pMemToMapOs2->Core.enmType)
    {
        /*
         * These has kernel mappings.
         */
        case RTR0MEMOBJTYPE_PAGE:
        case RTR0MEMOBJTYPE_LOW:
        case RTR0MEMOBJTYPE_CONT:
            pvR0 = pMemToMapOs2->Core.pv;
            break;

        case RTR0MEMOBJTYPE_PHYS:
            pvR0 = pMemToMapOs2->Core.pv;
            if (!pvR0)
            {
                /* no ring-0 mapping, so allocate a mapping in the process. */
                AssertMsgReturn(uAlignment == PAGE_SIZE, ("%#zx\n", uAlignment), VERR_NOT_SUPPORTED);
                AssertMsgReturn(fProt & RTMEM_PROT_WRITE, ("%#x\n", fProt), VERR_NOT_SUPPORTED);
                Assert(!pMemToMapOs2->Core.u.Phys.fAllocated);
                ULONG ulPhys = pMemToMapOs2->Core.u.Phys.PhysBase;
                rc = KernVMAlloc(pMemToMapOs2->Core.cb, VMDHA_PHYS, &pvR0, (PPVOID)&ulPhys, NULL);
                if (rc)
                    return RTErrConvertFromOS2(rc);
                pMemToMapOs2->Core.pv = pvR0;
            }
            break;

        case RTR0MEMOBJTYPE_LOCK:
            if (pMemToMapOs2->Core.u.Lock.R0Process != NIL_RTR0PROCESS)
                return VERR_NOT_SUPPORTED; /** @todo implement this... */
            pvR0 = pMemToMapOs2->Core.pv;
            break;

        case RTR0MEMOBJTYPE_RES_VIRT:
        case RTR0MEMOBJTYPE_MAPPING:
        default:
            AssertMsgFailed(("enmType=%d\n", pMemToMapOs2->Core.enmType));
            return VERR_INTERNAL_ERROR;
    }

    /*
     * Create a dummy mapping object for it.
     *
     * All mappings are read/write/execute in OS/2 and there isn't 
     * any cache options, so sharing is ok. And the main memory object
     * isn't actually freed until all the mappings have been freed up
     * (reference counting).
     */
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(RT_OFFSETOF(RTR0MEMOBJOS2, Lock), RTR0MEMOBJTYPE_MAPPING, pvR0, pMemToMapOs2->Core.cb);
    if (pMemFreeBSD)
    {
        pMemFreeBSD->Core.u.Mapping.R0Process = NIL_RTR0PROCESS;
        *ppMem = &pMemFreeBSD->Core;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
#endif
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeMapUser(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJ pMemToMap, void *pvFixed, size_t uAlignment, unsigned fProt, RTR0PROCESS R0Process)
{
    AssertMsgReturn(R0Process == RTR0ProcHandleSelf(), ("%p != %p\n", R0Process, RTR0ProcHandleSelf()), VERR_NOT_SUPPORTED);
    AssertMsgReturn(pvFixed == (void *)-1, ("%p\n", pvFixed), VERR_NOT_SUPPORTED);

#if 0
    int rc;
    void *pvR0;
    void *pvR3 = NULL;
    PRTR0MEMOBJFREEBSD pMemToMapOs2 = (PRTR0MEMOBJFREEBSD)pMemToMap;
    switch (pMemToMapOs2->Core.enmType)
    {
        /*
         * These has kernel mappings.
         */
        case RTR0MEMOBJTYPE_PAGE:
        case RTR0MEMOBJTYPE_LOW:
        case RTR0MEMOBJTYPE_CONT:
            pvR0 = pMemToMapOs2->Core.pv;
            break;

        case RTR0MEMOBJTYPE_PHYS:
            pvR0 = pMemToMapOs2->Core.pv;
#if 0/* this is wrong. */
            if (!pvR0)
            {
                /* no ring-0 mapping, so allocate a mapping in the process. */
                AssertMsgReturn(uAlignment == PAGE_SIZE, ("%#zx\n", uAlignment), VERR_NOT_SUPPORTED);
                AssertMsgReturn(fProt & RTMEM_PROT_WRITE, ("%#x\n", fProt), VERR_NOT_SUPPORTED);
                Assert(!pMemToMapOs2->Core.u.Phys.fAllocated);
                ULONG ulPhys = pMemToMapOs2->Core.u.Phys.PhysBase;
                rc = KernVMAlloc(pMemToMapOs2->Core.cb, VMDHA_PHYS | VMDHA_PROCESS, &pvR3, (PPVOID)&ulPhys, NULL);
                if (rc)
                    return RTErrConvertFromOS2(rc);
            }
            break;
#endif 
            return VERR_NOT_SUPPORTED;

        case RTR0MEMOBJTYPE_LOCK:
            if (pMemToMapOs2->Core.u.Lock.R0Process != NIL_RTR0PROCESS)
                return VERR_NOT_SUPPORTED; /** @todo implement this... */
            pvR0 = pMemToMapOs2->Core.pv;
            break;

        case RTR0MEMOBJTYPE_RES_VIRT:
        case RTR0MEMOBJTYPE_MAPPING:
        default:
            AssertMsgFailed(("enmType=%d\n", pMemToMapOs2->Core.enmType));
            return VERR_INTERNAL_ERROR;
    }

    /*
     * Map the ring-0 memory into the current process.
     */
    if (!pvR3)
    {
        Assert(pvR0);
        ULONG flFlags = 0;
        if (uAlignment == PAGE_SIZE)
            flFlags |= VMDHGP_4MB;
        if (fProt & RTMEM_PROT_WRITE)
            flFlags |= VMDHGP_WRITE;
        rc = RTR0Os2DHVMGlobalToProcess(flFlags, pvR0, pMemToMapOs2->Core.cb, &pvR3);
        if (rc)
            return RTErrConvertFromOS2(rc);
    }
    Assert(pvR3);

    /*
     * Create a mapping object for it.
     */
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(RT_OFFSETOF(RTR0MEMOBJOS2, Lock), RTR0MEMOBJTYPE_MAPPING, pvR3, pMemToMapOs2->Core.cb);
    if (pMemFreeBSD)
    {
        Assert(pMemFreeBSD->Core.pv == pvR3);
        pMemFreeBSD->Core.u.Mapping.R0Process = R0Process;
        *ppMem = &pMemFreeBSD->Core;
        return VINF_SUCCESS;
    }
    KernVMFree(pvR3);
    return VERR_NO_MEMORY;
#endif
    return VERR_NOT_IMPLEMENTED;
}


RTHCPHYS rtR0MemObjNativeGetPagePhysAddr(PRTR0MEMOBJINTERNAL pMem, unsigned iPage)
{
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)pMem;

    switch (pMemFreeBSD->Core.enmType)
    {
        case RTR0MEMOBJTYPE_LOCK:
        {
            if (    pMemFreeBSD->Core.u.Lock.R0Process != NIL_RTR0PROCESS
                &&  pMemFreeBSD->Core.u.Lock.R0Process != (RTR0PROCESS)curproc)
            {
                /* later */
                return NIL_RTHCPHYS;
            }  
        }
        case RTR0MEMOBJTYPE_PAGE:
        {
            uint8_t *pb = (uint8_t *)pMemFreeBSD->Core.pv + ((size_t)iPage << PAGE_SHIFT);
            return vtophys(pb);
        }
            
        case RTR0MEMOBJTYPE_CONT:
            return pMemFreeBSD->Core.u.Cont.Phys + (iPage << PAGE_SHIFT);

        case RTR0MEMOBJTYPE_PHYS:
            return pMemFreeBSD->Core.u.Phys.PhysBase + (iPage << PAGE_SHIFT);

        case RTR0MEMOBJTYPE_RES_VIRT:
        case RTR0MEMOBJTYPE_MAPPING:
        case RTR0MEMOBJTYPE_LOW:
        default:
            return NIL_RTHCPHYS;
    }
}
