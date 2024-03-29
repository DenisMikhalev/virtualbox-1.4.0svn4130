/** @file
 * Shared Clipboard:
 * Common header for host service and guest clients.
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

#ifndef ___VBox_HostService_VBoxClipboardSvc_h
#define ___VBox_HostService_VBoxClipboardSvc_h

#include <VBox/types.h>
#include <VBox/VBoxGuest.h>
#include <VBox/hgcmsvc.h>

/*
 * The mode of operations.
 */
#define VBOX_SHARED_CLIPBOARD_MODE_OFF           0
#define VBOX_SHARED_CLIPBOARD_MODE_HOST_TO_GUEST 1
#define VBOX_SHARED_CLIPBOARD_MODE_GUEST_TO_HOST 2
#define VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL 3

/*
 * Supported data formats. Bit mask.
 */
#define VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT 0x01
#define VBOX_SHARED_CLIPBOARD_FMT_BITMAP      0x02
#define VBOX_SHARED_CLIPBOARD_FMT_HTML        0x04

/*
 * The service functions which are callable by host.
 */
#define VBOX_SHARED_CLIPBOARD_HOST_FN_SET_MODE   1

/*
 * The service functions which are called by guest.
 */
/* Call host and wait blocking for an host event VBOX_SHARED_CLIPBOARD_HOST_MSG_* */
#define VBOX_SHARED_CLIPBOARD_FN_GET_HOST_MSG      1
/* Send list of available formats to host. */
#define VBOX_SHARED_CLIPBOARD_FN_FORMATS           2
/* Obtain data in specified format from host. */
#define VBOX_SHARED_CLIPBOARD_FN_READ_DATA         3
/* Send data in requested format to host. */
#define VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA        4

/*
 * The host messages for the guest.
 */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT        1
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA   2
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS     3

/*
 * HGCM parameter structures.
 */
#pragma pack (1)
typedef struct _VBoxClipboardGetHostMsg
{
    VBoxGuestHGCMCallInfo hdr;

    /* VBOX_SHARED_CLIPBOARD_HOST_MSG_* */
    HGCMFunctionParameter msg;     /* OUT uint32_t */

    /* VBOX_SHARED_CLIPBOARD_FMT_*, depends on the 'msg'. */
    HGCMFunctionParameter formats; /* OUT uint32_t */
} VBoxClipboardGetHostMsg;

typedef struct _VBoxClipboardFormats
{
    VBoxGuestHGCMCallInfo hdr;

    /* VBOX_SHARED_CLIPBOARD_FMT_* */
    HGCMFunctionParameter formats; /* OUT uint32_t */
} VBoxClipboardFormats;

typedef struct _VBoxClipboardReadData
{
    VBoxGuestHGCMCallInfo hdr;

    /* Requested format. */
    HGCMFunctionParameter format; /* IN uint32_t */

    /* The data buffer. */
    HGCMFunctionParameter ptr;    /* IN linear pointer. */

    /* Size of returned data, if > ptr->cb, then no data was
     * actually transferred and the guest must repeat the call.
     */
    HGCMFunctionParameter size;   /* OUT uint32_t */

} VBoxClipboardReadData;

typedef struct _VBoxClipboardWriteData
{
    VBoxGuestHGCMCallInfo hdr;

    /* Returned format as requested in the VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA message. */
    HGCMFunctionParameter format; /* IN uint32_t */

    /* Data.  */
    HGCMFunctionParameter ptr;    /* IN linear pointer. */
} VBoxClipboardWriteData;
#pragma pack ()

#endif
