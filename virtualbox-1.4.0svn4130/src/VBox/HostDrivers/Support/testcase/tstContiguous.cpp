/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Testcases:
 * Test contiguous memory
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
#include <VBox/sup.h>
#include <VBox/param.h>
#include <iprt/runtime.h>
#include <iprt/stream.h>
#include <stdlib.h>
#include <string.h>



int main(int argc, char **argv)
{
    int rc;
    int rcRet = 0;

    RTR3Init(false);
    rc = SUPInit();
    RTPrintf("tstContiguous: SUPInit -> rc=%Vrc\n", rc);
    rcRet += rc != 0;
    if (!rc)
    {
        /*
         * Allocate a bit of contiguous memory.
         */
        RTHCPHYS HCPhys;
        void *pv = SUPContAlloc(8, &HCPhys);
        rcRet += pv == NULL || HCPhys == 0;
        if (pv && HCPhys)
        {
            memset(pv, 0xff, PAGE_SIZE * 8);
            pv = SUPContAlloc(5, &HCPhys);
            rcRet += pv == NULL || HCPhys == 0;
            if (pv && HCPhys)
            {
                memset(pv, 0x7f, PAGE_SIZE * 5);
                rc = SUPContFree(pv, 5);
                rcRet += rc != 0;
                if (rc)
                    RTPrintf("tstContiguous: SUPContFree failed! rc=%Vrc\n", rc);
            }
            else
                RTPrintf("tstContiguous: SUPContAlloc (2nd) failed!\n");
        }
        else
            RTPrintf("tstContiguous: SUPContAlloc failed!\n");

        rc = SUPTerm();
        RTPrintf("tstContiguous: SUPTerm -> rc=%Vrc\n", rc);
        rcRet += rc != 0;
    }

    return rcRet;
}
