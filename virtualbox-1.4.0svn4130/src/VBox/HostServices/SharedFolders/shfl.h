/** @file
 *
 * Shared Folders:
 * Main header. Common data and function prototypes definitions.
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

#ifndef __SHFL__H
#define __SHFL__H

#include <VBox/err.h>
#include <VBox/hgcmsvc.h>

#define LOG_GROUP LOG_GROUP_SHARED_FOLDERS
#include <VBox/log.h>

/**
 * Shared Folders client flags.
 * @{
 */

/** Client has queried mappings at least once and, therefore,
 *  the service can process its other requests too.
 */
#define SHFL_CF_MAPPINGS_QUERIED (0x00000001)

/** Mappings have been changed since last query. */
#define SHFL_CF_MAPPINGS_CHANGED (0x00000002)

/** Client uses UTF8 encoding, if not set then unicode 16 bit (UCS2) is used. */
#define SHFL_CF_UTF8             (0x00000004)

/** @} */

typedef struct _SHFLCLIENTDATA
{
    /** Client flags */
    uint32_t fu32Flags;

    RTUCS2   PathDelimiter;
} SHFLCLIENTDATA;


#endif /* __SHFL__H */
