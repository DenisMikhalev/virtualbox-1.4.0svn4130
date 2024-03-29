/* $Id $ */
/** @file
 * innotek Portable Runtime - Convert Darwin Mach returns codes to iprt status codes.
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
#include <mach/kern_return.h>

#include <iprt/err.h>
#include <iprt/assert.h>


RTDECL(int) RTErrConvertFromDarwinKern(int iNativeCode)
{
    /*
     * 'optimzied' success case.
     */
    if (iNativeCode == KERN_SUCCESS)
        return VINF_SUCCESS;

    switch (iNativeCode)
    {
        case KERN_INVALID_ADDRESS:      return VERR_INVALID_POINTER;
        //case KERN_PROTECTION_FAILURE:
        //case KERN_NO_SPACE:
        case KERN_INVALID_ARGUMENT:     return VERR_INVALID_PARAMETER;
        //case KERN_FAILURE:
        //case KERN_RESOURCE_SHORTAGE:
        //case KERN_NOT_RECEIVER:
        case KERN_NO_ACCESS:            return VERR_ACCESS_DENIED;
        //case KERN_MEMORY_FAILURE:
        //case KERN_MEMORY_ERROR:
        //case KERN_ALREADY_IN_SET:
        //case KERN_NOT_IN_SET:
        //case KERN_NAME_EXISTS:
        //case KERN_ABORTED:
        //case KERN_INVALID_NAME:
        //case KERN_INVALID_TASK:
        //case KERN_INVALID_RIGHT:
        //case KERN_INVALID_VALUE:
        //case KERN_UREFS_OVERFLOW:
        //case KERN_INVALID_CAPABILITY:
        //case KERN_RIGHT_EXISTS:
        //case KERN_INVALID_HOST:
        //case KERN_MEMORY_PRESENT:
        //case KERN_MEMORY_DATA_MOVED:
        //case KERN_MEMORY_RESTART_COPY:
        //case KERN_INVALID_PROCESSOR_SET:
        //case KERN_POLICY_LIMIT:
        //case KERN_INVALID_POLICY:
        //case KERN_INVALID_OBJECT:
        //case KERN_ALREADY_WAITING:
        //case KERN_DEFAULT_SET:
        //case KERN_EXCEPTION_PROTECTED:
        //case KERN_INVALID_LEDGER:
        //case KERN_INVALID_MEMORY_CONTROL:
        //case KERN_INVALID_SECURITY:
        //case KERN_NOT_DEPRESSED:
        //case KERN_TERMINATED:
        //case KERN_LOCK_SET_DESTROYED:
        //case KERN_LOCK_UNSTABLE:
        case KERN_LOCK_OWNED:           return VERR_SEM_BUSY;
        //case KERN_LOCK_OWNED_SELF:
        case KERN_SEMAPHORE_DESTROYED:  return VERR_SEM_DESTROYED;
        //case KERN_RPC_SERVER_TERMINATED:
        //case KERN_RPC_TERMINATE_ORPHAN:
        //case KERN_RPC_CONTINUE_ORPHAN:
        case KERN_NOT_SUPPORTED:        return VERR_NOT_SUPPORTED;
        //case KERN_NODE_DOWN:
        //case KERN_NOT_WAITING:
        case KERN_OPERATION_TIMED_OUT:  return VERR_TIMEOUT;
    }

    /* unknown error. */
    AssertMsgFailed(("Unhandled error %#x\n", iNativeCode));
    return VERR_UNRESOLVED_ERROR;
}


