; $Id: lrintf.asm 4071 2007-08-07 17:07:59Z vboxsync $
;; @file
; innotek Portable Runtime - No-CRT lrintf - AMD64 & X86.
;

;
;  Copyright (C) 2006-2007 innotek GmbH
; 
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License as published by the Free Software Foundation,
;  in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
;  distribution. VirtualBox OSE is distributed in the hope that it will
;  be useful, but WITHOUT ANY WARRANTY of any kind.

%include "iprt/asmdefs.mac"

BEGINCODE

;;
; Round rd to the nearest integer value, rounding according to the current rounding direction.
; @returns 32-bit: eax  64-bit: rax
; @param    rf     32-bit: [esp + 4h]  64-bit: xmm0
BEGINPROC RT_NOCRT(lrintf)
%ifdef RT_ARCH_AMD64
    cvtss2si rax, xmm0
%else
    push    ebp
    mov     ebp, esp
    sub     esp, 8h

    fld     dword [ebp + 8h]
    fistp   dword [esp]
    fwait
    mov     eax, [esp]

    leave
%endif
    ret
ENDPROC   RT_NOCRT(lrintf)

