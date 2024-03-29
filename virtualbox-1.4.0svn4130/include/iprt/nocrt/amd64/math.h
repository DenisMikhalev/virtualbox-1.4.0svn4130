/** @file
 * innotek Portable Runtime / No-CRT - math.h, AMD inlined functions.
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

#ifndef ___iprt_nocrt_amd64_math_h
#define ___iprt_nocrt_amd64_math_h

#include <iprt/asm.h>


#if RT_INLINE_ASM_GNU_STYLE

DECLINLINE(long double) inline_atan2l(long double lrd1, long double lrd2)
{
    long double lrdResult;
    __asm__ __volatile__("fpatan"
                         : "=t" (lrdResult)
                         : "u" (lrd1),
                           "0" (lrd2)
                         : "st(1)");
    return lrdResult;
}

DECLINLINE(long double) inline_rintl(long double lrd)
{
    long double lrdResult;
    __asm__ __volatile__("frndint"
                         : "=t" (lrdResult)
                         : "0" (lrd));
    return lrdResult;
}

DECLINLINE(float) inline_rintf(float rf)
{
    return (float)inline_rintl(rf);
}

DECLINLINE(double) inline_rint(double rd)
{
    return (double)inline_rintl(rd);
}

DECLINLINE(long double) inline_sqrtl(long double lrd)
{
    long double lrdResult;
    __asm__ __volatile__("fsqrt"
                         : "=t" (lrdResult)
                         : "0" (lrd));
    return lrdResult;
}

DECLINLINE(float) inline_sqrtf(float rf)
{
    return (float)inline_sqrtl(rf);
}

DECLINLINE(double) inline_sqrt(double rd)
{
    return (double)inline_sqrt(rd);
}


# undef atan2l
# define atan2l(lrd1, lrd2)      inline_atan2l(lrd1, lrd2)
# undef rint
# define rint(rd)                inline_rint(rd)
# undef rintf
# define rintf(rf)               inline_rintf(rf)
# undef rintl
# define rintl(lrd)              inline_rintl(lrd)
# undef sqrt
# define sqrt(rd)                inline_sqrt(rd)
# undef sqrtf
# define sqrtf(rf)               inline_sqrtf(rf)
# undef sqrtl
# define sqrtl(lrd)              inline_sqrtl(lrd)

#endif /* RT_INLINE_ASM_GNU_STYLE */

#endif

