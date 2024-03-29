/* $Id: dir-win32.cpp 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * innotek Portable Runtime - Directory, win32.
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
#define LOG_GROUP RTLOGGROUP_DIR
#include <Windows.h>

#include <iprt/dir.h>
#include <iprt/path.h>
#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/log.h>
#include "internal/fs.h"
#include "internal/path.h"
#include "internal/dir.h"



RTDECL(bool) RTDirExists(const char *pszPath)
{
    bool fRc = false;

    /*
     * Convert to UCS2.
     */
    PRTUCS2 pucszString;
    int rc = RTStrUtf8ToUcs2(&pucszString, pszPath);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        /*
         * Query and check attributes.
         */
        DWORD dwAttr = GetFileAttributesW((LPCWSTR)pucszString);
        fRc = dwAttr != INVALID_FILE_ATTRIBUTES
            && (dwAttr & FILE_ATTRIBUTE_DIRECTORY);

        RTStrUcs2Free(pucszString);
    }

    LogFlow(("RTDirExists(%p:{%s}): returns %RTbool\n", pszPath, pszPath, fRc));
    return fRc;
}


RTDECL(int) RTDirCreate(const char *pszPath, RTFMODE fMode)
{
    /*
     * Validate the file mode.
     */
    int rc;
    fMode = rtFsModeNormalize(fMode, pszPath, 0);
    if (rtFsModeIsValidPermissions(fMode))
    {
        /*
         * Convert to UCS2.
         */
        PRTUCS2 pucszString;
        rc = RTStrUtf8ToUcs2(&pucszString, pszPath);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            /*
             * Create the directory.
             */
            if (CreateDirectoryW((LPCWSTR)pucszString, NULL))
                rc = VINF_SUCCESS;
            else
                rc = RTErrConvertFromWin32(GetLastError());

            RTStrUcs2Free(pucszString);
        }
    }
    else
    {
        AssertMsgFailed(("Invalid file mode! %RTfmode\n", fMode));
        rc = VERR_INVALID_FMODE;
    }

    LogFlow(("RTDirCreate(%p:{%s}, %RTfmode): returns %Rrc\n", pszPath, pszPath, fMode, rc));
    return rc;
}


RTDECL(int) RTDirRemove(const char *pszPath)
{
    /*
     * Convert to UCS2.
     */
    PRTUCS2 pucszString;
    int rc = RTStrUtf8ToUcs2(&pucszString, pszPath);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        /*
         * Remove the directory.
         */
        if (RemoveDirectoryW((LPCWSTR)pucszString))
            rc = VINF_SUCCESS;
        else
            rc = RTErrConvertFromWin32(GetLastError());

        RTStrUcs2Free(pucszString);
    }

    LogFlow(("RTDirRemove(%p:{%s}): returns %Rrc\n", pszPath, pszPath, rc));
    return rc;
}


int rtOpenDirNative(PRTDIR pDir, char *pszPathBuf)
{
    /*
     * Setup the search expression.
     *
     * pszPathBuf is pointing to the return 4K return buffer for the RTPathReal()
     * call in rtDirOpenCommon(), so all we gota do is check that we don't overflow
     * it when adding the wildcard expression.
     */
    size_t cchExpr;
    const char *pszExpr;
    if (pDir->enmFilter == RTDIRFILTER_WINNT)
    {
        pszExpr = pDir->pszFilter;
        cchExpr = pDir->cchFilter + 1;
    }
    else
    {
        pszExpr = "*";
        cchExpr = sizeof("*");
    }
    if (pDir->cchPath + cchExpr > RTPATH_MAX)
        return VERR_FILENAME_TOO_LONG;
    memcpy(pszPathBuf + pDir->cchPath, pszExpr, cchExpr);


    /*
     * Attempt opening the search.
     */
    int rc = VINF_SUCCESS;
#ifndef RT_DONT_CONVERT_FILENAMES
    PRTUCS2 puszName;
    rc = RTStrUtf8ToUcs2(&puszName, pszPathBuf);
    if (RT_SUCCESS(rc))
    {
        pDir->hDir    = FindFirstFileW((LPCWSTR)puszName, &pDir->Data);
#else
        pDir->hDir    = FindFirstFileA(pszPathBuf, &pDir->Data);
#endif
        if (pDir->hDir != INVALID_HANDLE_VALUE)
            pDir->fDataUnread = true;
        /* theoretical case of an empty directory. */
        else if (GetLastError() == ERROR_NO_MORE_FILES)
            pDir->fDataUnread = false;
        else
            rc = RTErrConvertFromWin32(GetLastError());
#ifndef RT_DONT_CONVERT_FILENAMES
        RTStrUcs2Free(puszName);
    }
#endif

    return rc;
}


RTDECL(int) RTDirClose(PRTDIR pDir)
{
    /*
     * Validate input.
     */
    if (!pDir)
        return VERR_INVALID_PARAMETER;
    if (pDir->u32Magic != RTDIR_MAGIC)
    {
        AssertMsgFailed(("Invalid pDir=%p\n", pDir));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Close the handle.
     */
    pDir->u32Magic++;
    if (pDir->hDir != INVALID_HANDLE_VALUE)
    {
        BOOL fRc = FindClose(pDir->hDir);
        Assert(fRc);
        pDir->hDir = INVALID_HANDLE_VALUE;
    }
    RTStrFree(pDir->pszName);
    pDir->pszName = NULL;
    RTMemFree(pDir);

    return VINF_SUCCESS;
}


RTDECL(int) RTDirRead(PRTDIR pDir, PRTDIRENTRY pDirEntry, unsigned *pcbDirEntry)
{
    /*
     * Validate input.
     */
    if (!pDir || pDir->u32Magic != RTDIR_MAGIC)
    {
        AssertMsgFailed(("Invalid pDir=%p\n", pDir));
        return VERR_INVALID_PARAMETER;
    }
    if (!pDirEntry)
    {
        AssertMsgFailed(("Invalid pDirEntry=%p\n", pDirEntry));
        return VERR_INVALID_PARAMETER;
    }
    unsigned cbDirEntry = sizeof(*pDirEntry);
    if (pcbDirEntry)
    {
        cbDirEntry = *pcbDirEntry;
        if (cbDirEntry < (unsigned)RT_OFFSETOF(RTDIRENTRY, szName[2]))
        {
            AssertMsgFailed(("Invalid *pcbDirEntry=%d (min %d)\n", *pcbDirEntry, RT_OFFSETOF(RTDIRENTRY, szName[2])));
            return VERR_INVALID_PARAMETER;
        }
    }

    /*
     * Fetch data?
     */
    if (!pDir->fDataUnread)
    {
#ifdef RT_DONT_CONVERT_FILENAMES
        BOOL fRc = FindNextFileA(pDir->hDir, &pDir->Data);

#else
        RTStrFree(pDir->pszName);
        pDir->pszName = NULL;

        BOOL fRc = FindNextFileW(pDir->hDir, &pDir->Data);
#endif
        if (!fRc)
        {
            int iErr = GetLastError();
            if (pDir->hDir == INVALID_HANDLE_VALUE || iErr == ERROR_NO_MORE_FILES)
                return VERR_NO_MORE_FILES;
            return RTErrConvertFromWin32(iErr);
        }
    }

#ifndef RT_DONT_CONVERT_FILENAMES
    /*
     * Convert the filename to UTF-8.
     */
    if (!pDir->pszName)
    {
        int rc = RTUtf16ToUtf8((PCRTUTF16)pDir->Data.cFileName, &pDir->pszName);
        if (RT_FAILURE(rc))
        {
            pDir->pszName = NULL;
            return rc;
        }
        pDir->cchName = strlen(pDir->pszName);
    }
#endif

    /*
     * Check if we've got enough space to return the data.
     */
#ifdef RT_DONT_CONVERT_FILENAMES
    const char  *pszName    = pDir->Data.cName;
    const size_t cchName    = strlen(pszName);
#else
    const char  *pszName    = pDir->pszName;
    const size_t cchName    = pDir->cchName;
#endif
    const size_t cbRequired = RT_OFFSETOF(RTDIRENTRY, szName[1]) + cchName;
    if (pcbDirEntry)
        *pcbDirEntry = cbRequired;
    if (cbRequired > cbDirEntry)
        return VERR_BUFFER_OVERFLOW;

    /*
     * Setup the returned data.
     */
    pDir->fDataUnread  = false;
    pDirEntry->INodeId = 0;
    pDirEntry->enmType = pDir->Data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY
                       ? RTDIRENTRYTYPE_DIRECTORY : RTDIRENTRYTYPE_FILE;
    pDirEntry->cbName  = (uint16_t)cchName;
    Assert(pDirEntry->cbName == cchName);
    memcpy(pDirEntry->szName, pszName, cchName + 1);

    return VINF_SUCCESS;
}


RTDECL(int) RTDirReadEx(PRTDIR pDir, PRTDIRENTRYEX pDirEntry, unsigned *pcbDirEntry, RTFSOBJATTRADD enmAdditionalAttribs)
{
    /*
     * Validate input.
     */
    if (!pDir || pDir->u32Magic != RTDIR_MAGIC)
    {
        AssertMsgFailed(("Invalid pDir=%p\n", pDir));
        return VERR_INVALID_PARAMETER;
    }
    if (!pDirEntry)
    {
        AssertMsgFailed(("Invalid pDirEntry=%p\n", pDirEntry));
        return VERR_INVALID_PARAMETER;
    }
    if (    enmAdditionalAttribs < RTFSOBJATTRADD_NOTHING
        ||  enmAdditionalAttribs > RTFSOBJATTRADD_LAST)
    {
        AssertMsgFailed(("Invalid enmAdditionalAttribs=%p\n", enmAdditionalAttribs));
        return VERR_INVALID_PARAMETER;
    }
    unsigned cbDirEntry = sizeof(*pDirEntry);
    if (pcbDirEntry)
    {
        cbDirEntry = *pcbDirEntry;
        if (cbDirEntry < (unsigned)RT_OFFSETOF(RTDIRENTRYEX, szName[2]))
        {
            AssertMsgFailed(("Invalid *pcbDirEntry=%d (min %d)\n", *pcbDirEntry, RT_OFFSETOF(RTDIRENTRYEX, szName[2])));
            return VERR_INVALID_PARAMETER;
        }
    }

    /*
     * Fetch data?
     */
    if (!pDir->fDataUnread)
    {
#ifdef RT_DONT_CONVERT_FILENAMES
        BOOL fRc = FindNextFileA(pDir->hDir, &pDir->Data);

#else
        RTStrFree(pDir->pszName);
        pDir->pszName = NULL;

        BOOL fRc = FindNextFileW(pDir->hDir, &pDir->Data);
#endif
        if (!fRc)
        {
            int iErr = GetLastError();
            if (pDir->hDir == INVALID_HANDLE_VALUE || iErr == ERROR_NO_MORE_FILES)
                return VERR_NO_MORE_FILES;
            return RTErrConvertFromWin32(iErr);
        }
    }

#ifndef RT_DONT_CONVERT_FILENAMES
    /*
     * Convert the filename to UTF-8.
     */
    if (!pDir->pszName)
    {
        int rc = RTUtf16ToUtf8((PCRTUTF16)pDir->Data.cFileName, &pDir->pszName);
        if (RT_FAILURE(rc))
        {
            pDir->pszName = NULL;
            return rc;
        }
        pDir->cchName = strlen(pDir->pszName);
    }
#endif

    /*
     * Check if we've got enough space to return the data.
     */
#ifdef RT_DONT_CONVERT_FILENAMES
    const char  *pszName    = pDir->Data.cName;
    const size_t cchName    = strlen(pszName);
#else
    const char  *pszName    = pDir->pszName;
    const size_t cchName    = pDir->cchName;
#endif
    const size_t cbRequired = RT_OFFSETOF(RTDIRENTRYEX, szName[1]) + cchName;
    if (pcbDirEntry)
        *pcbDirEntry = cbRequired;
    if (cbRequired > cbDirEntry)
        return VERR_BUFFER_OVERFLOW;

    /*
     * Setup the returned data.
     */
    pDir->fDataUnread  = false;
    pDirEntry->cbName  = (uint16_t)cchName;
    Assert(pDirEntry->cbName == cchName);
    memcpy(pDirEntry->szName, pszName, cchName + 1);
#ifndef RT_DONT_CONVERT_FILENAMES /* this ain't nice since the whole point of this define is not to drag in conversion... */
    if (pDir->Data.cAlternateFileName[0])
    {
        /* copy and calc length */
        PCRTUCS2 pucSrc = (PCRTUCS2)pDir->Data.cAlternateFileName;
        PRTUCS2  pucDst = pDirEntry->uszShortName;
        while (*pucSrc)
            *pucDst++ = *pucSrc++;
        pDirEntry->cucShortName = pucDst - &pDirEntry->uszShortName[0];
        /* zero the rest */
        const PRTUCS2 pucEnd = &pDirEntry->uszShortName[ELEMENTS(pDirEntry->uszShortName)];
        while (pucDst < pucEnd)
            *pucDst++ = '\0';
    }
    else
#endif
    {
        memset(pDirEntry->uszShortName, 0, sizeof(pDirEntry->uszShortName));
        pDirEntry->cucShortName = 0;
    }

    pDirEntry->Info.cbObject    = ((uint64_t)pDir->Data.nFileSizeHigh << 32)
                                |  (uint64_t)pDir->Data.nFileSizeLow;
    pDirEntry->Info.cbAllocated = pDirEntry->Info.cbObject;

    Assert(sizeof(uint64_t) == sizeof(pDir->Data.ftCreationTime));
    RTTimeSpecSetNtTime(&pDirEntry->Info.BirthTime,         *(uint64_t *)&pDir->Data.ftCreationTime);
    RTTimeSpecSetNtTime(&pDirEntry->Info.AccessTime,        *(uint64_t *)&pDir->Data.ftLastAccessTime);
    RTTimeSpecSetNtTime(&pDirEntry->Info.ModificationTime,  *(uint64_t *)&pDir->Data.ftLastWriteTime);
    pDirEntry->Info.ChangeTime  = pDirEntry->Info.ModificationTime;

    pDirEntry->Info.Attr.fMode  = rtFsModeFromDos((pDir->Data.dwFileAttributes << RTFS_DOS_SHIFT) & RTFS_DOS_MASK_NT,
                                                   pszName, cchName);

    /*
     * Requested attributes (we cannot provide anything actually).
     */
    switch (enmAdditionalAttribs)
    {
        case RTFSOBJATTRADD_EASIZE:
            pDirEntry->Info.Attr.enmAdditional          = RTFSOBJATTRADD_EASIZE;
            pDirEntry->Info.Attr.u.EASize.cb            = 0;
            break;

        case RTFSOBJATTRADD_UNIX:
            pDirEntry->Info.Attr.enmAdditional          = RTFSOBJATTRADD_UNIX;
            pDirEntry->Info.Attr.u.Unix.uid             = ~0U;
            pDirEntry->Info.Attr.u.Unix.gid             = ~0U;
            pDirEntry->Info.Attr.u.Unix.cHardlinks      = 1;
            pDirEntry->Info.Attr.u.Unix.INodeIdDevice   = 0;
            pDirEntry->Info.Attr.u.Unix.INodeId         = 0;
            pDirEntry->Info.Attr.u.Unix.fFlags          = 0;
            pDirEntry->Info.Attr.u.Unix.GenerationId    = 0;
            pDirEntry->Info.Attr.u.Unix.Device          = 0;
            break;

        case RTFSOBJATTRADD_NOTHING:
            pDirEntry->Info.Attr.enmAdditional          = RTFSOBJATTRADD_NOTHING;
            break;

        default:
            AssertMsgFailed(("Impossible!\n"));
            return VERR_INTERNAL_ERROR;
    }

    return VINF_SUCCESS;
}


RTDECL(int) RTDirRename(const char *pszSrc, const char *pszDst, unsigned fRename)
{
    /*
     * Validate input.
     */
    AssertMsgReturn(VALID_PTR(pszSrc), ("%p\n", pszSrc), VERR_INVALID_POINTER);
    AssertMsgReturn(VALID_PTR(pszDst), ("%p\n", pszDst), VERR_INVALID_POINTER);
    AssertMsgReturn(*pszSrc, ("%p\n", pszSrc), VERR_INVALID_PARAMETER);
    AssertMsgReturn(*pszDst, ("%p\n", pszDst), VERR_INVALID_PARAMETER);
    AssertMsgReturn(!(fRename & ~RTPATHRENAME_FLAGS_REPLACE), ("%#x\n", fRename), VERR_INVALID_PARAMETER);

    /*
     * Call the worker.
     */
    int rc = rtPathWin32MoveRename(pszSrc, pszDst,
                                   fRename & RTPATHRENAME_FLAGS_REPLACE ? MOVEFILE_REPLACE_EXISTING : 0,
                                   RTFS_TYPE_DIRECTORY);

    LogFlow(("RTDirRename(%p:{%s}, %p:{%s}, %#x): returns %Rrc\n", pszSrc, pszSrc, pszDst, pszDst, fRename, rc));
    return rc;
}

