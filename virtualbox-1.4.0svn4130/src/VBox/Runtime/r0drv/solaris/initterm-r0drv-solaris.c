/* $Id: initterm-r0drv-solaris.c 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * innotek Portable Runtime - Initialization & Termination, R0 Driver, Solaris.
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
#include "the-solaris-kernel.h"
#include <iprt/err.h>
#include <iprt/assert.h>
#include "internal/initterm.h"


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/


int rtR0InitNative(void)
{
    return VINF_SUCCESS;
}


void rtR0TermNative(void)
{
}
