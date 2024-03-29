/** @file
 * VirtualBox Video interface
 */

/*
 * Copyright (C) 2006 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBox_VBoxVideo_h
#define ___VBox_VBoxVideo_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

/* 
 * The last 4096 bytes of the guest VRAM contains the generic info for all
 * DualView chunks: sizes and offsets of chunks. This is filled by miniport.
 *
 * Last 4096 bytes of each chunk contain chunk specific data: framebuffer info,
 * etc. This is used exclusively by the corresponding instance of a display driver.
 *
 * The VRAM layout: 
 *     Last 4096 bytes - Adapter information area.
 *     4096 bytes aligned miniport heap (value specified in the config rouded up).
 *     Slack - what left after dividing the VRAM.
 *     4096 bytes aligned framebuffers:
 *       last 4096 bytes of each framebuffer is the display information area.
 *
 * The Virtual Graphics Adapter information in the guest VRAM is stored by the
 * guest video driver using structures prepended by VBOXVIDEOINFOHDR.
 *
 * When the guest driver writes dword 0 to the VBE_DISPI_INDEX_VBOX_VIDEO
 * the host starts to process the info. The first element at the start of
 * the 4096 bytes region should be normally be a LINK that points to
 * actual information chain. That way the guest driver can have some
 * fixed layout of the information memory block and just rewrite
 * the link to point to relevant memory chain.
 *
 * The processing stops at the END element.
 *
 * The host can access the memory only when the port IO is processed.
 * All data that will be needed later must be copied from these 4096 bytes.
 * But other VRAM can be used by host until the mode is disabled.
 *
 * The guest driver writes dword 0xffffffff to the VBE_DISPI_INDEX_VBOX_VIDEO
 * to disable the mode.
 *
 * VBE_DISPI_INDEX_VBOX_VIDEO is used to read the configuration information
 * from the host and issue commands to the host.
 * 
 * The guest writes the VBE_DISPI_INDEX_VBOX_VIDEO index register, the the
 * following operations with the VBE data register can be performed:
 * 
 * Operation            Result
 * write 16 bit value   NOP
 * read 16 bit value    count of monitors
 * write 32 bit value   sets the vbox command value and the command processed by the host
 * read 32 bit value    result of the last vbox command is returned
 */

#define VBOX_VIDEO_PRIMARY_SCREEN 0
#define VBOX_VIDEO_NO_SCREEN ~0

#define VBOX_VIDEO_MAX_SCREENS 64

/* The size of the information. */
#define VBOX_VIDEO_ADAPTER_INFORMATION_SIZE  4096
#define VBOX_VIDEO_DISPLAY_INFORMATION_SIZE  4096


/* The value for port IO to let the adapter to interpret the adapter memory. */
#define VBOX_VIDEO_DISABLE_ADAPTER_MEMORY        0xFFFFFFFF

/* The value for port IO to let the adapter to interpret the adapter memory. */
#define VBOX_VIDEO_INTERPRET_ADAPTER_MEMORY      0x00000000

/* The value for port IO to let the adapter to interpret the display memory.
 * The display number is encoded in low 16 bits. 
 */
#define VBOX_VIDEO_INTERPRET_DISPLAY_MEMORY_BASE 0x00010000


/* The end of the information. */
#define VBOX_VIDEO_INFO_TYPE_END          0
/* Instructs the host to fetch the next VBOXVIDEOINFOHDR at the given offset of VRAM. */
#define VBOX_VIDEO_INFO_TYPE_LINK         1
/* Information about a display memory position. */
#define VBOX_VIDEO_INFO_TYPE_DISPLAY      2
/* Information about a screen. */
#define VBOX_VIDEO_INFO_TYPE_SCREEN       3
/* Information about host notifications for the driver. */
#define VBOX_VIDEO_INFO_TYPE_HOST_EVENTS  4
/* Information about non-volatile guest VRAM heap. */
#define VBOX_VIDEO_INFO_TYPE_NV_HEAP      5
/* VBVA enable/disable. */
#define VBOX_VIDEO_INFO_TYPE_VBVA_STATUS  6
/* VBVA flush. */
#define VBOX_VIDEO_INFO_TYPE_VBVA_FLUSH   7
/* Query configuration value. */
#define VBOX_VIDEO_INFO_TYPE_QUERY_CONF32 8


#pragma pack(1)
typedef struct _VBOXVIDEOINFOHDR
{
    uint8_t u8Type;
    uint8_t u8Reserved;
    uint16_t u16Length;
} VBOXVIDEOINFOHDR;


typedef struct _VBOXVIDEOINFOLINK
{
    /* Relative offset in VRAM */
    int32_t i32Offset;
} VBOXVIDEOINFOLINK;


/* Resides in adapter info memory. Describes a display VRAM chunk. */
typedef struct _VBOXVIDEOINFODISPLAY
{
    /* Index of the framebuffer assigned by guest. */
    uint32_t u32Index;

    /* Absolute offset in VRAM of the framebuffer to be displayed on the monitor. */
    uint32_t u32Offset;

    /* The size of the memory that can be used for the screen. */
    uint32_t u32FramebufferSize;

    /* The size of the memory that is used for the Display information.
     * The information is at u32Offset + u32FramebufferSize
     */
    uint32_t u32InformationSize;

} VBOXVIDEOINFODISPLAY;


/* Resides in display info area, describes the current video mode. */
#define VBOX_VIDEO_INFO_SCREEN_F_NONE   0x00
#define VBOX_VIDEO_INFO_SCREEN_F_ACTIVE 0x01

typedef struct _VBOXVIDEOINFOSCREEN
{
    /* Physical X origin relative to the primary screen. */
    int32_t xOrigin;

    /* Physical Y origin relative to the primary screen. */
    int32_t yOrigin;

    /* The scan line size in bytes. */
    uint32_t u32LineSize;

    /* Width of the screen. */
    uint16_t u16Width;

    /* Height of the screen. */
    uint16_t u16Height;

    /* Color depth. */
    uint8_t bitsPerPixel;

    /* VBOX_VIDEO_INFO_SCREEN_F_* */
    uint8_t u8Flags;
} VBOXVIDEOINFOSCREEN;

/* The guest initializes the structure to 0. The positions of the structure in the
 * display info area must not be changed, host will update the structure. Guest checks
 * the events and modifies the structure as a response to host.
 */
#define VBOX_VIDEO_INFO_HOST_EVENTS_F_NONE        0x00000000
#define VBOX_VIDEO_INFO_HOST_EVENTS_F_VRDP_RESET  0x00000080

typedef struct _VBOXVIDEOINFOHOSTEVENTS
{
    /* Host events. */
    uint32_t fu32Events;
} VBOXVIDEOINFOHOSTEVENTS;

/* Resides in adapter info memory. Describes the non-volatile VRAM heap. */
typedef struct _VBOXVIDEOINFONVHEAP
{
    /* Absolute offset in VRAM of the start of the heap. */
    uint32_t u32HeapOffset;

    /* The size of the heap. */
    uint32_t u32HeapSize;

} VBOXVIDEOINFONVHEAP;

/* Display information area. */
typedef struct _VBOXVIDEOINFOVBVASTATUS
{
    /* Absolute offset in VRAM of the start of the VBVA QUEUE. 0 to disable VBVA. */
    uint32_t u32QueueOffset;

    /* The size of the VBVA QUEUE. 0 to disable VBVA. */
    uint32_t u32QueueSize;

} VBOXVIDEOINFOVBVASTATUS;

typedef struct _VBOXVIDEOINFOVBVAFLUSH
{
    uint32_t u32DataStart;

    uint32_t u32DataEnd;

} VBOXVIDEOINFOVBVAFLUSH;

#define VBOX_VIDEO_QCI32_MONITOR_COUNT       0
#define VBOX_VIDEO_QCI32_OFFSCREEN_HEAP_SIZE 1

typedef struct _VBOXVIDEOINFOQUERYCONF32
{
    uint32_t u32Index;

    uint32_t u32Value;

} VBOXVIDEOINFOQUERYCONF32;
#pragma pack()

#endif
