/* $Id: string.h 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * innotek Portable Runtime - Internal RTStr header.
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

#ifndef ___internal_string_h
#define ___internal_string_h

#include <iprt/string.h>

__BEGIN_DECLS

/** @def RTSTR_STRICT
 * Enables strict assertions on bad string encodings.
 */
#ifdef __DOXYGEN__
# define RTSTR_STRICT
#endif
/*#define RTSTR_STRICT*/

#ifdef RTSTR_STRICT
# define RTStrAssertMsgFailed(msg)              AssertMsgFailed(msg)
# define RTStrAssertMsgReturn(expr, msg, rc)    AssertMsgReturn(expr, msg, rc)
#else
# define RTStrAssertMsgFailed(msg)              do { } while (0)
# define RTStrAssertMsgReturn(expr, msg, rc)    do { if (!(expr)) return rc; } while (0)
#endif

#ifdef RT_WITH_VBOX
size_t rtstrFormatVBox(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput, const char **ppszFormat, va_list *pArgs, int cchWidth, int cchPrecision, unsigned fFlags, char chArgSize);
#endif
size_t rtstrFormatRt(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput, const char **ppszFormat, va_list *pArgs, int cchWidth, int cchPrecision, unsigned fFlags, char chArgSize);

__END_DECLS

#endif

