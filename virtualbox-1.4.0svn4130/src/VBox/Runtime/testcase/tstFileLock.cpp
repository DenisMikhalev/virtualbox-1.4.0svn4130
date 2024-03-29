/* $Id: tstFileLock.cpp 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * innotek Portable Runtime Testcase - File Locks.
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
#include <iprt/file.h>
#include <iprt/stream.h>
#include <iprt/err.h>
#include <iprt/thread.h>  /* for RTThreadSleep() */
#include <iprt/runtime.h>
#include <iprt/string.h>

#include <stdio.h>

static char achTest1[] = "Test line 1\n";
static char achTest2[] = "Test line 2\n";
static char achTest3[] = "Test line 3\n";

static bool fRun = false;

/** @todo Create a tstFileLock-2 which doesn't require interaction and counts errors. */

int main()
{
    RTR3Init();
    RTPrintf("tstFileLock: TESTING\n");

    RTFILE File;
    int rc = RTFileOpen(&File, "tstLock.tst", RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    RTPrintf("File open: rc=%Vrc\n", rc);
    if (RT_FAILURE(rc))
    {
        if (rc != VERR_FILE_NOT_FOUND && rc != VERR_OPEN_FAILED)
        {
            RTPrintf("FATAL\n");
            return 1;
        }

        rc = RTFileOpen(&File, "tstLock.tst", RTFILE_O_READWRITE | RTFILE_O_CREATE | RTFILE_O_DENY_NONE);
        RTPrintf("File create: rc=%Vrc\n", rc);
        if (RT_FAILURE(rc))
        {
            RTPrintf("FATAL\n");
            return 2;
        }
        fRun = true;
    }

    /* grow file a little */
    rc = RTFileSetSize(File, fRun ? 2048 : 20480);
    RTPrintf("File size: rc=%Vrc\n", rc);

    int buf;
    /* read test. */
    rc = RTFileRead(File, &buf, sizeof(buf), NULL);
    RTPrintf("Read: rc=%Vrc\n", rc);

    /* write test. */
    rc = RTFileWrite(File, achTest1, strlen(achTest1), NULL);
    RTPrintf("Write: rc=%Vrc\n", rc);

    /* lock: read, non-blocking. */
    rc = RTFileLock(File, RTFILE_LOCK_READ | RTFILE_LOCK_IMMEDIATELY, 0, _4G);
    RTPrintf("Lock: read, non-blocking, rc=%Vrc\n", rc);
    bool fl = RT_SUCCESS(rc);

    /* read test. */
    rc = RTFileRead(File, &buf, sizeof(buf), NULL);
    RTPrintf("Read: rc=%Vrc\n", rc);

    /* write test. */
    rc = RTFileWrite(File, achTest2, strlen(achTest2), NULL);
    RTPrintf("Write: rc=%Vrc\n", rc);
    RTPrintf("Lock test will change in three seconds\n");
    for (int i = 0; i < 3; i++)
    {
        RTThreadSleep(1000);
        RTPrintf(".");
    }
    RTPrintf("\n");

    /* change lock: write, non-blocking. */
    rc = RTFileLock(File, RTFILE_LOCK_WRITE | RTFILE_LOCK_IMMEDIATELY, 0, _4G);
    RTPrintf("Change lock: write, non-blocking, rc=%Vrc\n", rc);
    RTPrintf("Test will unlock in three seconds\n");
    for (int i = 0; i < 3; i++)
    {
        RTThreadSleep(1000);
        RTPrintf(".");
    }
    RTPrintf("\n");

    /* remove lock. */
    if (fl)
    {
        fl = false;
        rc = RTFileUnlock(File, 0, _4G);
        RTPrintf("Unlock: rc=%Vrc\n", rc);
        RTPrintf("Write test will lock in three seconds\n");
        for (int i = 0; i < 3; i++)
        {
            RTThreadSleep(1000);
            RTPrintf(".");
        }
        RTPrintf("\n");
    }

    /* lock: write, non-blocking. */
    rc = RTFileLock(File, RTFILE_LOCK_WRITE | RTFILE_LOCK_IMMEDIATELY, 0, _4G);
    RTPrintf("Lock: write, non-blocking, rc=%Vrc\n", rc);
    fl = RT_SUCCESS(rc);

    /* grow file test */
    rc = RTFileSetSize(File, fRun ? 2048 : 20480);
    RTPrintf("File size: rc=%Vrc\n", rc);

    /* read test. */
    rc = RTFileRead(File, &buf, sizeof(buf), NULL);
    RTPrintf("Read: rc=%Vrc\n", rc);

    /* write test. */
    rc = RTFileWrite(File, achTest3, strlen(achTest3), NULL);
    RTPrintf("Write: rc=%Vrc\n", rc);
    RTPrintf("Continuing to next test in three seconds\n");
    for (int i = 0; i < 3; i++)
    {
        RTThreadSleep(1000);
        RTPrintf(".");
    }
    RTPrintf("\n");

    RTFileClose(File);
    RTFileDelete("tstLock.tst");


    RTPrintf("tstFileLock: I've no recollection of this testcase succeeding or not, sorry.\n");
    return 0;
}
