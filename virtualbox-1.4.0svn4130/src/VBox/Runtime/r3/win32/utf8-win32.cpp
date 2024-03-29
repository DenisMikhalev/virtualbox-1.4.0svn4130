/* $Id: utf8-win32.cpp 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * innotek Portable Runtime - UTF8 helpers.
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
#define LOG_GROUP RTLOGGROUP_UTF8
#include <Windows.h>

#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/err.h>


/**
 * Allocates tmp buffer, translates pszString from UTF8 to current codepage.
 *
 * @returns iprt status code.
 * @param   ppszString      Receives pointer of allocated native CP string.
 *                          The returned pointer must be freed using RTStrFree().
 * @param   pszString       UTF-8 string to convert.
 */
RTR3DECL(int)  RTStrUtf8ToCurrentCP(char **ppszString, const char *pszString)
{
    Assert(ppszString);
    Assert(pszString);

    /*
     * Check for zero length input string.
     */
    if (!*pszString)
    {
        *ppszString = (char *)RTMemTmpAllocZ(sizeof(char));
        if (*ppszString)
            return VINF_SUCCESS;
        return VERR_NO_TMP_MEMORY;
    }

    *ppszString = NULL;

    /*
     * Convert to wide char first.
     */
    PRTUCS2 pucszString = NULL;
    int rc = RTStrUtf8ToUcs2(&pucszString, pszString);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * First calc result string length.
     */
    int cbResult = WideCharToMultiByte(CP_ACP, 0, pucszString, -1, NULL, 0, NULL, NULL);
    if (cbResult > 0)
    {
        /*
         * Alloc space for result buffer.
         */
        LPSTR lpString = (LPSTR)RTMemTmpAlloc(cbResult);
        if (lpString)
        {
            /*
             * Do the translation.
             */
            if (WideCharToMultiByte(CP_ACP, 0, pucszString, -1, lpString, cbResult, NULL, NULL) > 0)
            {
                /* ok */
                *ppszString = lpString;
                RTMemTmpFree(pucszString);
                return VINF_SUCCESS;
            }

            /* translation error */
            int iLastErr = GetLastError();
            AssertMsgFailed(("Unicode to ACP translation failed. lasterr=%d\n", iLastErr));
            rc = RTErrConvertFromWin32(iLastErr);
        }
        else
            rc = VERR_NO_TMP_MEMORY;
        RTMemTmpFree(lpString);
    }
    else
    {
        /* translation error */
        int iLastErr = GetLastError();
        AssertMsgFailed(("Unicode to ACP translation failed lasterr=%d\n", iLastErr));
        rc = RTErrConvertFromWin32(iLastErr);
    }
    RTMemTmpFree(pucszString);
    return rc;
}

/**
 * Allocates tmp buffer, translates pszString from current codepage to UTF-8.
 *
 * @returns iprt status code.
 * @param   ppszString      Receives pointer of allocated UTF-8 string.
 *                          The returned pointer must be freed using RTStrFree().
 * @param   pszString       Native string to convert.
 */
RTR3DECL(int)  RTStrCurrentCPToUtf8(char **ppszString, const char *pszString)
{
    Assert(ppszString);
    Assert(pszString);
    *ppszString = NULL;

    /** @todo is there a quicker way? Currently: ACP -> UCS-2 -> UTF-8 */

    size_t cch = strlen(pszString);
    if (cch <= 0)
    {
        /* zero length string passed. */
        *ppszString = (char *)RTMemTmpAllocZ(sizeof(char));
        if (*ppszString)
            return VINF_SUCCESS;
        return VERR_NO_TMP_MEMORY;
    }

    /*
     * First calc result string length.
     */
    int rc;
    int cuc = MultiByteToWideChar(CP_ACP, 0, pszString, -1, NULL, 0);
    if (cuc > 0)
    {
        /*
         * Alloc space for result buffer.
         */
        PRTUCS2 pucszString = (PRTUCS2)RTMemTmpAlloc(cuc * sizeof(RTUCS2));
        if (pucszString)
        {
            /*
             * Do the translation.
             */
            if (MultiByteToWideChar(CP_ACP, 0, pszString, -1, pucszString, cuc) > 0)
            {
                /*
                 * Now we got UCS-2. Convert to UTF-8
                 */
                rc = RTStrUcs2ToUtf8(ppszString, pucszString);
                RTMemTmpFree(pucszString);
                return rc;
            }
            RTMemTmpFree(pucszString);
            /* translation error */
            int iLastErr = GetLastError();
            AssertMsgFailed(("ACP to Unicode translation failed. lasterr=%d\n", iLastErr));
            rc = RTErrConvertFromWin32(iLastErr);
        }
        else
            rc = VERR_NO_TMP_MEMORY;
    }
    else
    {
        /* translation error */
        int iLastErr = GetLastError();
        AssertMsgFailed(("Unicode to ACP translation failed lasterr=%d\n", iLastErr));
        rc = RTErrConvertFromWin32(iLastErr);
    }
    return rc;
}

