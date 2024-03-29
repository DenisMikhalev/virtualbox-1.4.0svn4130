/* $Id: tstRTProcWait.cpp 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * innotek Portable Runtime Testcase - RTProcWait.
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
#include <iprt/runtime.h>
#include <iprt/process.h>
#include <iprt/thread.h>
#include <iprt/stream.h>
#include <iprt/semaphore.h>
#include <iprt/err.h>
#include <iprt/string.h>




typedef struct SpawnerArgs
{
    RTPROCESS Process;
    const char *pszExe;
} SPAWNERARGS, *PSPAWNERARGS;


DECLCALLBACK(int) SpawnerThread(RTTHREAD Thread, void *pvUser)
{
    PSPAWNERARGS pArgs = (PSPAWNERARGS)pvUser;
    pArgs->Process = NIL_RTPROCESS;
    const char *apszArgs[3] = { pArgs->pszExe, "child", NULL };
    return RTProcCreate(apszArgs[0], apszArgs, NULL, 0, &pArgs->Process);
}


int main(int argc, char **argv)
{
    RTR3Init();
    if (argc == 2 && !strcmp(argv[1], "child"))
        return 42;

    RTPrintf("tstRTWait: spawning a child in a separate thread and waits for it in the main thread...\n");
    RTTHREAD  Thread;
    SPAWNERARGS Args = { NIL_RTPROCESS, argv[0] };
    int rc = RTThreadCreate(&Thread, SpawnerThread, &Args, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "SPAWNER");
    if (RT_SUCCESS(rc))
    {
        /* Wait for it to complete. */
        int rc2;
        int rc = RTThreadWait(Thread, RT_INDEFINITE_WAIT, &rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
        if (RT_SUCCESS(rc))
        {
            /* wait for the process to complete */
            RTPROCSTATUS Status;
            rc = RTProcWait(Args.Process, 0, &Status);
            if (RT_SUCCESS(rc))
            {
                if (    Status.enmReason == RTPROCEXITREASON_NORMAL
                    &&  Status.iStatus == 42)
                    RTPrintf("tstRTWait: Success!\n");
                else
                {
                    rc = VERR_GENERAL_FAILURE;
                    if (Status.enmReason != RTPROCEXITREASON_NORMAL)
                        RTPrintf("tstRTWait: Expected exit reason RTPROCEXITREASON_NORMAL, got %d.\n", Status.enmReason);
                    else
                        RTPrintf("tstRTWait: Expected exit status 42, got %d.\n", Status.iStatus);
                }
            }
            else
                RTPrintf("tstRTWait: RTProcWait failed with rc=%Vrc!\n", rc);
        }
        else
            RTPrintf("tstRTWait: RTThreadWait or SpawnerThread failed with rc=%Vrc!\n", rc);
    }
    else
        RTPrintf("tstRTWait: RTThreadCreate failed with rc=%Vrc!\n", rc);

    return RT_SUCCESS(rc) ? 0 : 1;
}
