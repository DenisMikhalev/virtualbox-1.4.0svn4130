/* $Id: initterm.h 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * innotek Portable Runtime - Initialization & Termination.
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

#ifndef ___internal_initterm_h
#define ___internal_initterm_h

#include <iprt/cdefs.h>

__BEGIN_DECLS

#ifdef IN_RING0

/**
 * Platform specific initialization.
 *
 * @returns IPRT status code.
 */
int rtR0InitNative(void);

/**
 * Platform specific terminiation.
 */
void rtR0TermNative(void);

#endif /* IN_RING0 */

__END_DECLS

#endif

