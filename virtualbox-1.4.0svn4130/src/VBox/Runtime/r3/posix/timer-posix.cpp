/* $Id: timer-posix.cpp 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * innotek Portable Runtime - Timer, POSIX.
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
#include <iprt/timer.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/log.h>
#include <iprt/asm.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/err.h>
#include "internal/magics.h"

#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#ifdef RT_OS_LINUX
# include <linux/rtc.h>
#endif
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#ifndef RT_OS_OS2
# include <pthread.h>
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The internal representation of a timer handle.
 */
typedef struct RTTIMER
{
    /** Magic.
     * This is RTTIMER_MAGIC, but changes to something else before the timer
     * is destroyed to indicate clearly that thread should exit. */
    uint32_t volatile       u32Magic;
    /** Flag indicating the the timer is suspended. */
    uint8_t volatile        fSuspended;
    /** Flag indicating that the timer has been destroyed. */
    uint8_t volatile        fDestroyed;
    /** The timer thread. */
    RTTHREAD                Thread;
    /** Event semaphore on which the thread is blocked. */
    RTSEMEVENT              Event;
    /** User argument. */
    void                   *pvUser;
    /** Callback. */
    PFNRTTIMER              pfnTimer;
    /** The timer interval. 0 if one-shot. */
    uint64_t                u64NanoInterval;
    /** The first shot interval. 0 if ASAP. */
    uint64_t volatile       u64NanoFirst;
    /** The error/status of the timer.
     * Initially -1, set to 0 when the timer have been successfully started, and
     * to errno on failure in starting the timer. */
    int volatile            iError;

} RTTIMER;


/**
 * Signal handler which ignore everything it gets.
 *
 * @param   iSignal     The signal number.
 */
static void rttimerSignalIgnore(int iSignal)
{
    //AssertBreakpoint();
}


/**
 * SIGALRM wait thread.
 */
static DECLCALLBACK(int) rttimerThread(RTTHREAD Thread, void *pvArg)
{
    PRTTIMER pTimer = (PRTTIMER)(void *)pvArg;
    RTTIMER Timer = *pTimer;
    Assert(pTimer->u32Magic == RTTIMER_MAGIC);

    /*
     * Install signal handler.
     */
    struct sigaction SigAct;
    memset(&SigAct, 0, sizeof(SigAct));
    SigAct.sa_flags = SA_RESTART;
    sigemptyset(&SigAct.sa_mask);
    SigAct.sa_handler = rttimerSignalIgnore;
    if (sigaction(SIGALRM, &SigAct, NULL))
    {
        SigAct.sa_flags &= ~SA_RESTART;
        if (sigaction(SIGALRM, &SigAct, NULL))
            AssertMsgFailed(("sigaction failed, errno=%d\n", errno));
    }

    /*
     * Mask most signals except those which might be used by the pthread implementation (linux).
     */
    sigset_t SigSet;
    sigfillset(&SigSet);
    sigdelset(&SigSet, SIGTERM);
    sigdelset(&SigSet, SIGHUP);
    sigdelset(&SigSet, SIGINT);
    sigdelset(&SigSet, SIGABRT);
    sigdelset(&SigSet, SIGKILL);
#ifdef SIGRTMIN
    for (int iSig = SIGRTMIN; iSig < SIGRTMAX; iSig++)
        sigdelset(&SigSet, iSig);
#endif
    if (sigprocmask(SIG_SETMASK, &SigSet, NULL))
    {
        int rc = pTimer->iError = RTErrConvertFromErrno(errno);
        AssertMsgFailed(("sigprocmask -> errno=%d\n", errno));
        return rc;
    }

    /*
     * The work loop.
     */
    RTThreadUserSignal(Thread);
    while (     !pTimer->fDestroyed
           &&   pTimer->u32Magic == RTTIMER_MAGIC)
    {
        /*
         * Wait for a start or destroy event.
         */
        if (pTimer->fSuspended)
        {
            int rc = RTSemEventWait(pTimer->Event, RT_INDEFINITE_WAIT);
            if (RT_FAILURE(rc) && rc != VERR_INTERRUPTED)
            {
                AssertRC(rc);
                RTThreadSleep(1000); /* Don't cause trouble! */
            }
            if (    pTimer->fSuspended
                ||  pTimer->fDestroyed)
                continue;
        }

        /*
         * Start the timer.
         *
         * For some SunOS (/SysV?) threading compatibility Linux will only
         * deliver the SIGALRM to the thread calling setitimer(). Therefore
         * we have to call it here.
         *
         * It turns out this might not always be the case, see SIGALRM killing
         * processes on RH 2.4.21.
         */
        struct itimerval TimerVal;
        if (pTimer->u64NanoFirst)
        {
            uint64_t u64 = RT_MAX(1000, pTimer->u64NanoFirst);
            TimerVal.it_value.tv_sec     = u64 / 1000000000;
            TimerVal.it_value.tv_usec    = (u64 % 1000000000) / 1000;
        }
        else
        {
            TimerVal.it_value.tv_sec     = 0;
            TimerVal.it_value.tv_usec    = 10;
        }
        if (pTimer->u64NanoInterval)
        {
            uint64_t u64 = RT_MAX(1000, pTimer->u64NanoInterval);
            TimerVal.it_interval.tv_sec  = u64 / 1000000000;
            TimerVal.it_interval.tv_usec = (u64 % 1000000000) / 1000;
        }
        else
        {
            TimerVal.it_interval.tv_sec  = 0;
            TimerVal.it_interval.tv_usec = 0;
        }

        if (setitimer(ITIMER_REAL, &TimerVal, NULL))
        {
            ASMAtomicXchgU8(&pTimer->fSuspended, true);
            pTimer->iError = RTErrConvertFromErrno(errno);
            RTThreadUserSignal(Thread);
            continue; /* back to suspended mode. */
        }
        pTimer->iError = 0;
        RTThreadUserSignal(Thread);

        /*
         * Timer Service Loop.
         */
        sigemptyset(&SigSet);
        sigaddset(&SigSet, SIGALRM);
        do
        {
            siginfo_t SigInfo = {0};
#ifdef RT_OS_DARWIN
            if (RT_LIKELY(sigwait(&SigSet, &SigInfo.si_signo) >= 0))
            {
#else
            if (RT_LIKELY(sigwaitinfo(&SigSet, &SigInfo) >= 0))
            {
                if (RT_LIKELY(SigInfo.si_signo == SIGALRM))
#endif
                {
                    if (RT_UNLIKELY(    pTimer->fSuspended
                                    ||  pTimer->fDestroyed
                                    ||  pTimer->u32Magic != RTTIMER_MAGIC))
                        break;

                    pTimer->pfnTimer(pTimer, pTimer->pvUser);

                    /* auto suspend one-shot timers. */
                    if (RT_UNLIKELY(!pTimer->u64NanoInterval))
                    {
                        ASMAtomicXchgU8(&pTimer->fSuspended, true);
                        break;
                    }
                }
            }
            else if (errno != EINTR)
                AssertMsgFailed(("sigwaitinfo -> errno=%d\n", errno));
        } while (RT_LIKELY(   !pTimer->fSuspended
                           && !pTimer->fDestroyed
                           &&  pTimer->u32Magic == RTTIMER_MAGIC));

        /*
         * Disable the timer.
         */
        struct itimerval TimerVal2 = {{0,0}, {0,0}};
        if (setitimer(ITIMER_REAL, &TimerVal2, NULL))
            AssertMsgFailed(("setitimer(ITIMER_REAL,&{0}, NULL) failed, errno=%d\n", errno));

        /*
         * ACK any pending suspend request.
         */
        if (!pTimer->fDestroyed)
        {
            pTimer->iError = 0;
            RTThreadUserSignal(Thread);
        }
    }

    /*
     * Exit.
     */
    pTimer->iError = 0;
    RTThreadUserSignal(Thread);

    return VINF_SUCCESS;
}


RTDECL(int) RTTimerCreateEx(PRTTIMER *ppTimer, uint64_t u64NanoInterval, unsigned fFlags, PFNRTTIMER pfnTimer, void *pvUser)
{
    /*
     * Check if timer is busy.
     */
    struct itimerval TimerVal;
    if (getitimer(ITIMER_REAL, &TimerVal))
    {
        AssertMsgFailed(("getitimer() -> errno=%d\n", errno));
        return VERR_NOT_IMPLEMENTED;
    }
    if (    TimerVal.it_value.tv_usec || TimerVal.it_value.tv_sec
        ||  TimerVal.it_interval.tv_usec || TimerVal.it_interval.tv_sec
        )
    {
        AssertMsgFailed(("A timer is running. System limit is one timer per process!\n"));
        return VERR_TIMER_BUSY;
    }

    /*
     * Block SIGALRM from calling thread.
     */
    sigset_t SigSet;
    sigemptyset(&SigSet);
    sigaddset(&SigSet, SIGALRM);
    sigprocmask(SIG_BLOCK, &SigSet, NULL);

    /** @todo Move this RTC hack else where... */
    static bool fDoneRTC;
    if (!fDoneRTC)
    {
        fDoneRTC = true;
        /* check resolution. */
        TimerVal.it_interval.tv_sec = 0;
        TimerVal.it_interval.tv_usec = 1000;
        TimerVal.it_value = TimerVal.it_interval;
        if (    setitimer(ITIMER_REAL, &TimerVal, NULL)
            ||  getitimer(ITIMER_REAL, &TimerVal)
            ||  TimerVal.it_interval.tv_usec > 1000)
        {
            /*
             * Try open /dev/rtc to set the irq rate to 1024 and
             * turn periodic
             */
            Log(("RTTimerCreate: interval={%ld,%ld} trying to adjust /dev/rtc!\n", TimerVal.it_interval.tv_sec, TimerVal.it_interval.tv_usec));
#ifdef RT_OS_LINUX
            int fh = open("/dev/rtc", O_RDONLY);
            if (fh >= 0)
            {
                if (    ioctl(fh, RTC_IRQP_SET, 1024) < 0
                    ||  ioctl(fh, RTC_PIE_ON, 0) < 0)
                    Log(("RTTimerCreate: couldn't configure rtc! errno=%d\n", errno));
                ioctl(fh, F_SETFL, O_ASYNC);
                ioctl(fh, F_SETOWN, getpid());
                /* not so sure if closing it is a good idea... */
                //close(fh);
            }
            else
                Log(("RTTimerCreate: couldn't configure rtc! open failed with errno=%d\n", errno));
#endif
        }
        /* disable it */
        TimerVal.it_interval.tv_sec = 0;
        TimerVal.it_interval.tv_usec = 0;
        TimerVal.it_value = TimerVal.it_interval;
        setitimer(ITIMER_REAL, &TimerVal, NULL);
    }

    /*
     * Create a new timer.
     */
    int rc;
    PRTTIMER pTimer = (PRTTIMER)RTMemAlloc(sizeof(*pTimer));
    if (pTimer)
    {
        pTimer->u32Magic    = RTTIMER_MAGIC;
        pTimer->fSuspended  = true;
        pTimer->fDestroyed  = false;
        pTimer->Thread      = NIL_RTTHREAD;
        pTimer->Event       = NIL_RTSEMEVENT;
        pTimer->pfnTimer    = pfnTimer;
        pTimer->pvUser      = pvUser;
        pTimer->u64NanoInterval = u64NanoInterval;
        pTimer->iError      = 0;
        rc = RTSemEventCreate(&pTimer->Event);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            rc = RTThreadCreate(&pTimer->Thread, rttimerThread, pTimer, 0, RTTHREADTYPE_TIMER, RTTHREADFLAGS_WAITABLE, "Timer");
            AssertRC(rc);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Wait for the timer thread to initialize it self.
                 * This might take a little while...
                 */
                rc = RTThreadUserWait(pTimer->Thread, 45*1000);
                AssertRC(rc);
                if (RT_SUCCESS(rc))
                {
                    rc = RTThreadUserReset(pTimer->Thread); AssertRC(rc);
                    rc = pTimer->iError;
                    AssertRC(rc);
                    if (RT_SUCCESS(rc))
                    {
                        RTThreadYield(); /* <-- Horrible hack to make tstTimer work. (linux 2.6.12) */
                        *ppTimer = pTimer;
                        return VINF_SUCCESS;
                    }
                }

                /* bail out */
                ASMAtomicXchgU8(&pTimer->fDestroyed, true);
                ASMAtomicXchgU32(&pTimer->u32Magic, RTTIMER_MAGIC + 1);
                RTThreadWait(pTimer->Thread, 45*1000, NULL);
            }
            RTSemEventDestroy(pTimer->Event);
            pTimer->Event = NIL_RTSEMEVENT;
        }
        RTMemFree(pTimer);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


RTR3DECL(int)     RTTimerDestroy(PRTTIMER pTimer)
{
    LogFlow(("RTTimerDestroy: pTimer=%p\n", pTimer));

    /*
     * Validate input.
     */
    /* NULL is ok. */
    if (!pTimer)
        return VINF_SUCCESS;
    int rc = VINF_SUCCESS;
    AssertPtrReturn(pTimer, VERR_INVALID_POINTER);
    AssertReturn(pTimer->u32Magic == RTTIMER_MAGIC, VERR_INVALID_MAGIC);
    AssertReturn(pTimer->Thread != RTThreadSelf(), VERR_INTERNAL_ERROR);

    /*
     * Tell the thread to terminate and wait for it do complete.
     */
    ASMAtomicXchgU8(&pTimer->fDestroyed, true);
    ASMAtomicXchgU32(&pTimer->u32Magic, RTTIMER_MAGIC + 1);
    rc = RTSemEventSignal(pTimer->Event);
    AssertRC(rc);
    if (!pTimer->fSuspended)
    {
#ifndef RT_OS_OS2
        pthread_kill((pthread_t)RTThreadGetNative(pTimer->Thread), SIGALRM);
#endif
    }
    rc = RTThreadWait(pTimer->Thread, 30 * 1000, NULL);
    AssertRC(rc);

    RTSemEventDestroy(pTimer->Event);
    pTimer->Event = NIL_RTSEMEVENT;
    if (RT_SUCCESS(rc))
        RTMemFree(pTimer);
    return rc;
}


RTDECL(int) RTTimerStart(PRTTIMER pTimer, uint64_t u64First)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pTimer, VERR_INVALID_POINTER);
    AssertReturn(pTimer->u32Magic == RTTIMER_MAGIC, VERR_INVALID_MAGIC);
    AssertReturn(pTimer->Thread != RTThreadSelf(), VERR_INTERNAL_ERROR);

    /*
     * Already running?
     */
    if (!pTimer->fSuspended)
        return VERR_TIMER_ACTIVE;

    /*
     * Tell the thread to start servicing the timer.
     */
    RTThreadUserReset(pTimer->Thread);
    ASMAtomicXchgU64(&pTimer->u64NanoFirst, u64First);
    ASMAtomicXchgU8(&pTimer->fSuspended, false);
    int rc = RTSemEventSignal(pTimer->Event);
    if (RT_SUCCESS(rc))
    {
        rc = RTThreadUserWait(pTimer->Thread, 45*1000);
        AssertRC(rc);
        RTThreadUserReset(pTimer->Thread);
    }
    else
        AssertRC(rc);
    if (RT_FAILURE(rc))
        ASMAtomicXchgU8(&pTimer->fSuspended, false);

    return rc;
}


RTDECL(int) RTTimerStop(PRTTIMER pTimer)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pTimer, VERR_INVALID_POINTER);
    AssertReturn(pTimer->u32Magic == RTTIMER_MAGIC, VERR_INVALID_MAGIC);

    /*
     * Already running?
     */
    if (pTimer->fSuspended)
        return VERR_TIMER_SUSPENDED;

    /*
     * Tell the thread to stop servicing the timer.
     */
    RTThreadUserReset(pTimer->Thread);
    ASMAtomicXchgU8(&pTimer->fSuspended, true);
    int rc = VINF_SUCCESS;
    if (RTThreadSelf() != pTimer->Thread)
    {
#ifndef RT_OS_OS2
        pthread_kill((pthread_t)RTThreadGetNative(pTimer->Thread), SIGALRM);
#endif
        rc = RTThreadUserWait(pTimer->Thread, 45*1000);
        AssertRC(rc);
        RTThreadUserReset(pTimer->Thread);
    }

    return rc;
}

