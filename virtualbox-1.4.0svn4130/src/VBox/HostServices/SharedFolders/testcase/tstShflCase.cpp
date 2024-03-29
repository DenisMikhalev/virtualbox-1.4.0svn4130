/** @file
 *
 * Testcase for shared folder case conversion
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
#include <VBox/shflsvc.h>
#include <iprt/stream.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/fs.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/uni.h>
#include <stdio.h>

/* override for linux host */
#undef RTPATH_DELIMITER
#define RTPATH_DELIMITER       '\\'

#undef Log
#define Log(a)  printf a
#undef Log2
#define Log2    Log

#define RTPathQueryInfo     rtPathQueryInfo
#define RTDirOpenFiltered   rtDirOpenFiltered
#define RTDirClose          rtDirClose
#define RTDirReadEx         rtDirReadEx

static int iDirList = 0;
static int iDirFile = 0;

static char *pszDirList[] = 
{
"c:",
"c:\\test dir",
"c:\\test dir\\SUBDIR",
};

static char *pszDirListC[] = 
{
".",
"..",
"test dir"
};

static char *pszDirListTestdir[] = 
{
".",
"..",
"SUBDIR",
"a.bat",
"aTestJe.bat",
"aTestje.bat",
"b.bat",
"c.bat",
"d.bat",
"e.bat",
"f.bat",
"g.bat",
"h.bat",
"x.bat",
"z.bat",
};

static char *pszDirListSUBDIR[] = 
{
".",
"..",
"a.bat",
"aTestJe.bat",
"aTestje.bat",
"b.bat",
"c.bat",
"d.bat",
"e.bat",
"f.bat",
"g.bat",
"h.bat",
"x.bat",
"z.bat",
};

int rtDirOpenFiltered(PRTDIR *ppDir, const char *pszPath, RTDIRFILTER enmFilter)
{
    if (!strcmp(pszPath, "c:\\*"))
        iDirList = 1;
    else
    if (!strcmp(pszPath, "c:\\test dir\\*"))
        iDirList = 2;
    else
    if (!strcmp(pszPath, "c:\\test dir\\SUBDIR\\*"))
        iDirList = 3;
    else
        AssertFailed();

    *ppDir = (PRTDIR)1;
    return VINF_SUCCESS;
}

int rtDirClose(PRTDIR pDir)
{
    iDirFile = 0;
    return VINF_SUCCESS;
}

int rtDirReadEx(PRTDIR pDir, PRTDIRENTRYEX pDirEntry, unsigned *pcbDirEntry, RTFSOBJATTRADD enmAdditionalAttribs)
{
    switch(iDirList)
    {
    case 1:
        if (iDirFile == RT_ELEMENTS(pszDirListC))
            return VERR_NO_MORE_FILES;
        pDirEntry->cbName = strlen(pszDirListC[iDirFile]);
        strcpy(pDirEntry->szName, pszDirListC[iDirFile++]);
        break;
    case 2:
        if (iDirFile == RT_ELEMENTS(pszDirListTestdir))
            return VERR_NO_MORE_FILES;
        pDirEntry->cbName = strlen(pszDirListTestdir[iDirFile]);
        strcpy(pDirEntry->szName, pszDirListTestdir[iDirFile++]);
        break;
    case 3:
        if (iDirFile == RT_ELEMENTS(pszDirListSUBDIR))
            return VERR_NO_MORE_FILES;
        pDirEntry->cbName = strlen(pszDirListSUBDIR[iDirFile]);
        strcpy(pDirEntry->szName, pszDirListSUBDIR[iDirFile++]);
        break;
    }
    return VINF_SUCCESS;
}

int rtPathQueryInfo(const char *pszPath, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAdditionalAttribs)
{
    int cMax;
    char **ppszDirList;

    /* first try pszDirList */
    for (int i=0;i<RT_ELEMENTS(pszDirList);i++)
    {
        if(!strcmp(pszPath, pszDirList[i]))
            return VINF_SUCCESS;
    }
    
    switch(iDirList)
    {
    case 1:
        cMax = RT_ELEMENTS(pszDirListC);
        ppszDirList = pszDirListC;
        break;
    case 2:
        cMax = RT_ELEMENTS(pszDirListTestdir);
        ppszDirList = pszDirListTestdir;
        break;
    case 3:
        cMax = RT_ELEMENTS(pszDirListSUBDIR);
        ppszDirList = pszDirListSUBDIR;
        break;
    default:
        return VERR_FILE_NOT_FOUND;
    }
    for (int i=0;i<cMax;i++)
    {
        if(!strcmp(pszPath, ppszDirList[i]))
            return VINF_SUCCESS;
    }
    return VERR_FILE_NOT_FOUND;
}

static int vbsfCorrectCasing(char *pszFullPath, char *pszStartComponent)
{
    PRTDIRENTRYEX  pDirEntry = NULL;
    uint32_t       cbDirEntry, cbComponent;
    int            rc = VERR_FILE_NOT_FOUND;
    PRTDIR         hSearch = 0;
    char           szWildCard[4];

    Log2(("vbsfCorrectCasing: %s %s\n", pszFullPath, pszStartComponent));

    cbComponent = strlen(pszStartComponent);

    cbDirEntry = 4096;
    pDirEntry  = (PRTDIRENTRYEX)RTMemAlloc(cbDirEntry);
    if (pDirEntry == 0)
    {
        AssertFailed();
        return VERR_NO_MEMORY;
    }

    /** @todo this is quite inefficient, especially for directories with many files */
    Assert(pszFullPath < pszStartComponent-1);
    Assert(*(pszStartComponent-1) == RTPATH_DELIMITER);
    *(pszStartComponent-1) = 0;
    strcpy(pDirEntry->szName, pszFullPath);
    szWildCard[0] = RTPATH_DELIMITER;
    szWildCard[1] = '*';
    szWildCard[2] = 0;
    strcat(pDirEntry->szName, szWildCard);

    rc = RTDirOpenFiltered (&hSearch, pDirEntry->szName, RTDIRFILTER_WINNT);
    *(pszStartComponent-1) = RTPATH_DELIMITER;
    if (VBOX_FAILURE(rc))
        goto end;

    for(;;)
    {
        uint32_t cbDirEntrySize = cbDirEntry;

        rc = RTDirReadEx(hSearch, pDirEntry, &cbDirEntrySize, RTFSOBJATTRADD_NOTHING);
        if (rc == VERR_NO_MORE_FILES)
            break;

        if (VINF_SUCCESS != rc && rc != VWRN_NO_DIRENT_INFO)
        {
            AssertFailed();
            if (rc != VERR_NO_TRANSLATION)
                break;
            else
                continue;
        }

        Log2(("vbsfCorrectCasing: found %s\n", &pDirEntry->szName[0]));
        if (    pDirEntry->cbName == cbComponent
            &&  !RTStrICmp(pszStartComponent, &pDirEntry->szName[0]))
        {
            Log(("Found original name %s (%s)\n", &pDirEntry->szName[0], pszStartComponent));
            strcpy(pszStartComponent, &pDirEntry->szName[0]);
            rc = VINF_SUCCESS;
            break;
        }
    }
    if (VBOX_FAILURE(rc))
        Log(("vbsfCorrectCasing %s failed with %d\n", pszStartComponent, rc));

end:
    if (pDirEntry)
        RTMemFree(pDirEntry);

    if (hSearch)
        RTDirClose(hSearch);
    return rc;
}



int testCase(char *pszFullPath, bool fWildCard = false)
{
    int rc;
    RTFSOBJINFO info;
    char *pszWildCardComponent = NULL;

    if (fWildCard)
    {
        /* strip off the last path component, that contains the wildcard(s) */
        uint32_t len = strlen(pszFullPath);
        char    *src = pszFullPath + len - 1;

        while(src > pszFullPath)
        {
            if (*src == RTPATH_DELIMITER)
                break;
            src--;
        }
        if (*src == RTPATH_DELIMITER)
        {
            bool fHaveWildcards = false;
            char *temp = src;

            while(*temp)
            {
                char uc = *temp;
                /** @todo should depend on the guest OS */
                if (uc == '*' || uc == '?' || uc == '>' || uc == '<' || uc == '"')
                {
                    fHaveWildcards = true;
                    break;
                }
                temp++;
            }

            if (fHaveWildcards)
            {
                pszWildCardComponent = src;
                *pszWildCardComponent = 0;
            }
        }
    }

    rc = RTPathQueryInfo(pszFullPath, &info, RTFSOBJATTRADD_NOTHING);
    if (rc == VERR_FILE_NOT_FOUND || rc == VERR_PATH_NOT_FOUND)
    {
        uint32_t len = strlen(pszFullPath);
        char    *src = pszFullPath + len - 1;
        
        Log(("Handle case insenstive guest fs on top of host case sensitive fs for %s\n", pszFullPath));

        /* Find partial path that's valid */
        while(src > pszFullPath)
        {
            if (*src == RTPATH_DELIMITER)
            {
                *src = 0;
                rc = RTPathQueryInfo (pszFullPath, &info, RTFSOBJATTRADD_NOTHING);
                *src = RTPATH_DELIMITER;
                if (rc == VINF_SUCCESS)
                {
#ifdef DEBUG
                    *src = 0;
                    Log(("Found valid partial path %s\n", pszFullPath));
                    *src = RTPATH_DELIMITER;
#endif
                    break;
                }
            }

            src--;
        }
        Assert(*src == RTPATH_DELIMITER && VBOX_SUCCESS(rc));
        if (    *src == RTPATH_DELIMITER 
            &&  VBOX_SUCCESS(rc))
        {
            src++;
            for(;;)
            {
                char *end = src;
                bool fEndOfString = true;

                while(*end)
                {
                    if (*end == RTPATH_DELIMITER)
                        break;
                    end++;
                }

                if (*end == RTPATH_DELIMITER)
                {
                    fEndOfString = false;
                    *end = 0;
                    rc = RTPathQueryInfo(src, &info, RTFSOBJATTRADD_NOTHING);
                    Assert(rc == VINF_SUCCESS || rc == VERR_FILE_NOT_FOUND || rc == VERR_PATH_NOT_FOUND);
                }
                else
                if (end == src)
                    rc = VINF_SUCCESS;  /* trailing delimiter */
                else
                    rc = VERR_FILE_NOT_FOUND;
            
                if (rc == VERR_FILE_NOT_FOUND || rc == VERR_PATH_NOT_FOUND)
                {
                    /* path component is invalid; try to correct the casing */
                    rc = vbsfCorrectCasing(pszFullPath, src);
                    if (VBOX_FAILURE(rc))
                    {
                        if (!fEndOfString)
                              *end = RTPATH_DELIMITER;
                        break;
                    }
                }

                if (fEndOfString)
                    break;

                *end = RTPATH_DELIMITER;
                src = end + 1;
            }
            if (VBOX_FAILURE(rc))
                Log(("Unable to find suitable component rc=%d\n", rc));
        }
        else
            rc = VERR_FILE_NOT_FOUND;

    }
    if (pszWildCardComponent)
        *pszWildCardComponent = RTPATH_DELIMITER;

    if (VBOX_SUCCESS(rc))
        Log(("New valid path %s\n", pszFullPath));
    else
        Log(("Old invalid path %s\n", pszFullPath));
    return rc;
}


int main(int argc, char **argv)
{
    char szTest[128];

    strcpy(szTest, "c:\\test Dir\\z.bAt");
    testCase(szTest);
    strcpy(szTest, "c:\\test dir\\z.bAt");
    testCase(szTest);
    strcpy(szTest, "c:\\test dir\\SUBDIR\\z.bAt");
    testCase(szTest);
    strcpy(szTest, "c:\\test dir\\SUBDiR\\atestje.bat");
    testCase(szTest);
    strcpy(szTest, "c:\\TEST dir\\subDiR\\aTestje.baT");
    testCase(szTest);
    strcpy(szTest, "c:\\TEST dir\\subDiR\\*");
    testCase(szTest, true);
    strcpy(szTest, "c:\\TEST dir\\subDiR\\");
    testCase(szTest ,true);
    strcpy(szTest, "c:\\test dir\\SUBDIR\\");
    testCase(szTest);
    strcpy(szTest, "c:\\test dir\\invalid\\SUBDIR\\test.bat");
    testCase(szTest);
    return 0;
}
