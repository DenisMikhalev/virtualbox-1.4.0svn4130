; $Id: floorf.asm 4071 2007-08-07 17:07:59Z vboxsync $
;; @file
; innotek Portable Runtime - No-CRT floorf - AMD64 & X86.
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

%ifdef RT_ARCH_AMD64
 %define _SP rsp
 %define _BP rbp
 %define _S  8
%else
 %define _SP esp
 %define _BP ebp
 %define _S  4
%endif

;;
; Compute the largest integral value not greater than rf.
; @returns st(0)
; @param    rf      32-bit: [ebp + 8]   64-bit: xmm0
BEGINPROC RT_NOCRT(floorf)
    push    _BP
    mov     _BP, _SP
    sub     _SP, 10h

%ifdef RT_ARCH_AMD64
    movss   [_SP], xmm0
    fld     dword [_SP]
%else
    fld     dword [_BP + _S*2]
%endif

    ; Make it round down by modifying the fpu control word.
    fstcw   [_BP - 10h]
    mov     eax, [_BP - 10h]
    or      eax, 00400h
    and     eax, 0f7ffh
    mov     [_BP - 08h], eax
    fldcw   [_BP - 08h]

    ; Round ST(0) to integer.
    frndint

    ; Restore the fpu control word.
    fldcw   [_BP - 10h]

%ifdef RT_ARCH_AMD64
    fstp    dword [_SP]
    movss   xmm0, [_SP]
%endif
    leave
    ret
ENDPROC   RT_NOCRT(floorf)

