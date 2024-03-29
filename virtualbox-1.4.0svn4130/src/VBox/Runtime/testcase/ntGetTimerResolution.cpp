/* $Id: ntGetTimerResolution.cpp 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * innotek Portable Runtime - Win32 (NT) testcase for getting the timer resolution.
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
#define _WIN32_WINNT 0x0500
#include <Windows.h>
#include <stdio.h>

extern "C" {
/* from sysinternals. */
NTSYSAPI LONG NTAPI NtQueryTimerResolution(OUT PULONG MinimumResolution, OUT PULONG MaximumResolution, OUT PULONG CurrentResolution);
}


int main()
{
    ULONG Min = ~0;
    ULONG Max = ~0;
    ULONG Cur = ~0;
    NtQueryTimerResolution(&Min, &Max, &Cur);
    printf("NtQueryTimerResolution -> Min=%lu Max=%lu Cur=%lu (100ns)\n", Min, Max, Cur);

#if 0
    /* figure out the 100ns relative to the 1970 epoc. */
    SYSTEMTIME st;
    st.wYear = 1970;
    st.wMonth = 1;
    st.wDayOfWeek = 4; /* Thor's day. */
    st.wDay = 1;
    st.wHour = 0;
    st.wMinute = 0;
    st.wSecond = 0;
    st.wMilliseconds = 0;

    FILETIME ft;
    if (SystemTimeToFileTime(&st, &ft))
    {
        printf("epoc is %I64u (0x%08x%08x)\n", ft, ft.dwHighDateTime, ft.dwLowDateTime);
        if (FileTimeToSystemTime(&ft, &st))
            printf("unix epoc: %d-%02d-%02d %02d:%02d:%02d.%03d (week day %d)\n",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, st.wDayOfWeek);
        else
            printf("FileTimeToSystemTime failed, lasterr=%d\n", GetLastError());
    }
    else
        printf("SystemTimeToFileTime failed, lasterr=%d\n", GetLastError());

    ft.dwHighDateTime = 0;
    ft.dwLowDateTime = 0;
    if (FileTimeToSystemTime(&ft, &st))
        printf("nt time start: %d-%02d-%02d %02d:%02d:%02d.%03d (week day %d)\n",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, st.wDayOfWeek);
    else
        printf("FileTimeToSystemTime failed, lasterr=%d\n", GetLastError());
#endif
    return 0;
}

