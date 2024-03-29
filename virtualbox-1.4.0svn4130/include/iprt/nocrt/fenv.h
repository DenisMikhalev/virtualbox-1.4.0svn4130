/** @file
 * innotek Portable Runtime / No-CRT - fenv.h wrapper.
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

#ifndef ___iprt_nocrt_fenv_h
#define ___iprt_nocrt_fenv_h

#include <iprt/cdefs.h>
#ifdef RT_ARCH_AMD64
# include <iprt/nocrt/amd64/fenv.h>
#elif defined(RT_ARCH_X86)
# include <iprt/nocrt/x86/fenv.h>
#else
# error "IPRT: no fenv.h available for this platform, or the platform define is missing!"
#endif

#endif
