/* $Id: time-linux.cpp 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * innotek Portable Runtime - Time, POSIX.
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
#define LOG_GROUP RTLOGGROUP_TIME
#define RTTIME_INCL_TIMEVAL
#include <sys/time.h>
#include <time.h>
#include <sys/syscall.h>
#include <unistd.h>
#ifndef __NR_clock_gettime
# define __NR_timer_create	259
# define __NR_clock_gettime	(__NR_timer_create+6)
#endif

#include <iprt/time.h>
#include "internal/time.h"


DECLINLINE(int) sys_clock_gettime(clockid_t id,  struct timespec *ts)
{
    int rc = syscall(__NR_clock_gettime, id, ts);
    if (rc >= 0)
        return rc;
    return -1;
}


/**
 * Wrapper around various monotone time sources.
 */
DECLINLINE(int) mono_clock(struct timespec *ts)
{
    static int iWorking = -1;
    switch (iWorking)
    {
#ifdef CLOCK_MONOTONIC
        /*
         * Standard clock_gettime()
         */
        case 0:
            return clock_gettime(CLOCK_MONOTONIC, ts);

        /*
         * Syscall clock_gettime().
         */
        case 1:
            return sys_clock_gettime(CLOCK_MONOTONIC, ts);

#endif /* CLOCK_MONOTONIC */


        /*
         * Figure out what's working.
         */
        case -1:
        {
            int rc;
#ifdef CLOCK_MONOTONIC
            /*
             * Real-Time API.
             */
            rc = clock_gettime(CLOCK_MONOTONIC, ts);
            if (!rc)
            {
                iWorking = 0;
                return 0;
            }

            rc = sys_clock_gettime(CLOCK_MONOTONIC, ts);
            if (!rc)
            {
                iWorking = 1;
                return 0;
            }
#endif /* CLOCK_MONOTONIC */

            /* give up */
            iWorking = -2;
            break;
        }
    }
    return -1;
}


DECLINLINE(uint64_t) rtTimeGetSystemNanoTS(void)
{
    /* check monotonic clock first. */
    static bool fMonoClock = true;
    if (fMonoClock)
    {
        struct timespec ts;
        if (!mono_clock(&ts))
            return (uint64_t)ts.tv_sec * (uint64_t)(1000 * 1000 * 1000)
                 + ts.tv_nsec;
        fMonoClock = false;
    }

    /* fallback to gettimeofday(). */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec  * (uint64_t)(1000 * 1000 * 1000)
         + (uint64_t)(tv.tv_usec * 1000);
}


/**
 * Gets the current nanosecond timestamp.
 *
 * This differs from RTTimeNanoTS in that it will use system APIs and not do any
 * resolution or performance optimizations.
 *
 * @returns nanosecond timestamp.
 */
RTDECL(uint64_t) RTTimeSystemNanoTS(void)
{
    return rtTimeGetSystemNanoTS();
}


/**
 * Gets the current millisecond timestamp.
 *
 * This differs from RTTimeNanoTS in that it will use system APIs and not do any
 * resolution or performance optimizations.
 *
 * @returns millisecond timestamp.
 */
RTDECL(uint64_t) RTTimeSystemMilliTS(void)
{
    return rtTimeGetSystemNanoTS() / 1000000;
}

