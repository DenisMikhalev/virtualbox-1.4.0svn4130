/* $Id: DBGFLog.cpp 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * VMM DBGF - Debugger Facility, Log Manager.
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
#include <VBox/vmapi.h>
#include <VBox/vmm.h>
#include <VBox/dbgf.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/assert.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) dbgfR3LogModifyGroups(PVM pVM, const char *pszGroupSettings);
static DECLCALLBACK(int) dbgfR3LogModifyFlags(PVM pVM, const char *pszFlagSettings);
static DECLCALLBACK(int) dbgfR3LogModifyDestinations(PVM pVM, const char *pszDestSettings);


/**
 * Changes the logger group settings.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pszGroupSettings    The group settings string. (VBOX_LOG)
 */
DBGFR3DECL(int) DBGFR3LogModifyGroups(PVM pVM, const char *pszGroupSettings)
{
    AssertReturn(VALID_PTR(pVM), VERR_INVALID_POINTER);
    AssertReturn(VALID_PTR(pszGroupSettings), VERR_INVALID_POINTER);

    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)dbgfR3LogModifyGroups, 2, pVM, pszGroupSettings);
    if (VBOX_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);
    return rc;
}


/**
 * EMT worker for DBGFR3LogModifyGroups.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pszGroupSettings    The group settings string. (VBOX_LOG)
 */
static DECLCALLBACK(int) dbgfR3LogModifyGroups(PVM pVM, const char *pszGroupSettings)
{
    int rc = RTLogGroupSettings(NULL, pszGroupSettings);
    if (VBOX_SUCCESS(rc))
        rc = VMMR3UpdateLoggers(pVM);
    return rc;
}


/**
 * Changes the logger flag settings.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pszFlagSettings     The group settings string. (VBOX_LOG_FLAGS)
 */
DBGFR3DECL(int) DBGFR3LogModifyFlags(PVM pVM, const char *pszFlagSettings)
{
    AssertReturn(VALID_PTR(pVM), VERR_INVALID_POINTER);
    AssertReturn(VALID_PTR(pszFlagSettings), VERR_INVALID_POINTER);

    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)dbgfR3LogModifyFlags, 2, pVM, pszFlagSettings);
    if (VBOX_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);
    return rc;
}


/**
 * EMT worker for DBGFR3LogModifyFlags.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pszFlagSettings     The group settings string. (VBOX_LOG_FLAGS)
 */
static DECLCALLBACK(int) dbgfR3LogModifyFlags(PVM pVM, const char *pszFlagSettings)
{
    int rc = RTLogFlags(NULL, pszFlagSettings);
    if (VBOX_SUCCESS(rc))
        rc = VMMR3UpdateLoggers(pVM);
    return rc;
}


/**
 * Changes the logger destination settings.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pszDestSettings     The destination settings string. (VBOX_LOG_DEST)
 */
DBGFR3DECL(int) DBGFR3LogModifyDestinations(PVM pVM, const char *pszDestSettings)
{
    AssertReturn(VALID_PTR(pVM), VERR_INVALID_POINTER);
    AssertReturn(VALID_PTR(pszDestSettings), VERR_INVALID_POINTER);

    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)dbgfR3LogModifyDestinations, 2, pVM, pszDestSettings);
    if (VBOX_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);
    return rc;
}


/**
 * EMT worker for DBGFR3LogModifyFlags.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pszDestSettings     The destination settings string. (VBOX_LOG_DEST)
 */
static DECLCALLBACK(int) dbgfR3LogModifyDestinations(PVM pVM, const char *pszDestSettings)
{
    int rc = VERR_NOT_IMPLEMENTED; //RTLogDestination(NULL, pszDestSettings);
    if (VBOX_SUCCESS(rc))
        rc = VMMR3UpdateLoggers(pVM);
    return rc;
}

