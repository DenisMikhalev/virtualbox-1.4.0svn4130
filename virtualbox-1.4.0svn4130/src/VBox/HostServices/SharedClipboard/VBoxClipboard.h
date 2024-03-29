/** @file
 *
 * Shared Clipboard
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

#ifndef __VBOXCLIPBOARD__H
#define __VBOXCLIPBOARD__H

#define LOG_GROUP LOG_GROUP_HGCM
#include <VBox/log.h>

/** Constants needed for string conversions done by the Linux clipboard code. */
enum {
    /** In Linux, lines end with a linefeed character. */
    LINEFEED = 0xa,
    /** In Windows, lines end with a carriage return and a linefeed character. */
    CARRIAGERETURN = 0xd,
    /** Little endian "real" Utf16 strings start with this marker. */
    UTF16LEMARKER = 0xfeff,
    /** Big endian "real" Utf16 strings start with this marker. */
    UTF16BEMARKER = 0xfffe
};

enum {
    /** The number of milliseconds before the clipboard times out. */
    CLIPBOARDTIMEOUT = 2000
};

struct _VBOXCLIPBOARDCONTEXT;
typedef struct _VBOXCLIPBOARDCONTEXT VBOXCLIPBOARDCONTEXT;


typedef struct _VBOXCLIPBOARDCLIENTDATA
{
    struct _VBOXCLIPBOARDCLIENTDATA *pNext;
    struct _VBOXCLIPBOARDCLIENTDATA *pPrev;
    
    VBOXCLIPBOARDCONTEXT *pCtx;
    
    uint32_t u32ClientID;
    
    bool fAsync: 1; /* Guest is waiting for a message. */
    
    bool fMsgQuit: 1;
    bool fMsgReadData: 1;
    bool fMsgFormats: 1;
    
    struct {
        VBOXHGCMCALLHANDLE callHandle;
        VBOXHGCMSVCPARM *paParms;
    } async;
    
    struct {
         void *pv;
         uint32_t cb;
         uint32_t u32Format;
    } data;
    
    uint32_t u32AvailableFormats;
    uint32_t u32RequestedFormat;
    
} VBOXCLIPBOARDCLIENTDATA;

/*
 * The service functions. Locking is between the service thread and the platform dependedn windows thread.
 */
bool vboxSvcClipboardLock (void);
void vboxSvcClipboardUnlock (void);

void vboxSvcClipboardReportMsg (VBOXCLIPBOARDCLIENTDATA *pClient, uint32_t u32Msg, uint32_t u32Formats);


/*
 * Platform dependent functions.
 */
int vboxClipboardInit (void);
void vboxClipboardDestroy (void);

int vboxClipboardConnect (VBOXCLIPBOARDCLIENTDATA *pClient);
void vboxClipboardDisconnect (VBOXCLIPBOARDCLIENTDATA *pClient);

void vboxClipboardFormatAnnounce (VBOXCLIPBOARDCLIENTDATA *pClient, uint32_t u32Formats);

int vboxClipboardReadData (VBOXCLIPBOARDCLIENTDATA *pClient, uint32_t u32Format, void *pv, uint32_t cb, uint32_t *pcbActual);

void vboxClipboardWriteData (VBOXCLIPBOARDCLIENTDATA *pClient, void *pv, uint32_t cb, uint32_t u32Format);

int vboxClipboardSync (VBOXCLIPBOARDCLIENTDATA *pClient);

#endif /* __VBOXCLIPBOARD__H */
