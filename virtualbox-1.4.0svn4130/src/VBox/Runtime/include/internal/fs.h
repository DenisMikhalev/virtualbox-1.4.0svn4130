/* $Id: fs.h 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * innotek Portable Runtime - Internal RTFs header.
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

#ifndef ___internal_fs_h
#define ___internal_fs_h

#include <iprt/types.h>
#ifndef RT_OS_WINDOWS
# include <sys/stat.h>
#endif

__BEGIN_DECLS

RTFMODE rtFsModeFromDos(RTFMODE fMode, const char *pszName, unsigned cbName);
RTFMODE rtFsModeFromUnix(RTFMODE fMode, const char *pszName, unsigned cbName);
RTFMODE rtFsModeNormalize(RTFMODE fMode, const char *pszName, unsigned cbName);
bool    rtFsModeIsValid(RTFMODE fMode);
bool    rtFsModeIsValidPermissions(RTFMODE fMode);

size_t  rtPathVolumeSpecLen(const char *pszPath);
#ifndef RT_OS_WINDOWS
void    rtFsConvertStatToObjInfo(PRTFSOBJINFO pObjInfo, const struct stat *pStat, const char *pszName, unsigned cbName);
#endif

#ifdef RT_OS_LINUX
# ifdef __USE_MISC
#  define HAVE_STAT_TIMESPEC_BRIEF
# else
#  define HAVE_STAT_NSEC
# endif
#endif

__END_DECLS

#endif
