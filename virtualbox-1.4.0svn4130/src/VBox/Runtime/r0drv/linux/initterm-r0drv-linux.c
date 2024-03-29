/* $Id: initterm-r0drv-linux.c 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * innotek Portable Runtime - Initialization & Termination, R0 Driver, Linux.
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
#include <iprt/err.h>
#include <iprt/assert.h>
#include "internal/initterm.h"


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#ifdef RT_ARCH_AMD64
/* in alloc-r0drv0-linux.c */
extern void rtR0MemExecCleanup(void);
#endif 


int rtR0InitNative(void)
{
    return VINF_SUCCESS;
}


void rtR0TermNative(void)
{
#ifdef RT_ARCH_AMD64
    rtR0MemExecCleanup();
#endif 
}

