; $Id: memcpy.asm 4071 2007-08-07 17:07:59Z vboxsync $
;; @file
; innotek Portable Runtime - No-CRT memcpy - AMD64 & X86.
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
; @param    pvDst   gcc: rdi  msc: rcx  x86:[esp+4]
; @param    pvSrc   gcc: rsi  msc: rdx  x86:[esp+8]
; @param    cb      gcc: rdx  msc: r8   x86:[esp+0ch]
BEGINPROC RT_NOCRT(memcpy)
        cld

        ; Do the bulk of the work.
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_MSC
        mov     r10, rdi                ; save
        mov     r11, rsi                ; save
        mov     rdi, rcx
        mov     rsi, rdx
        mov     rcx, r8
        mov     rdx, r8
 %else
        mov     rcx, rdx
 %endif
        mov     rax, rdi                ; save the return value
        shr     rcx, 3
        rep movsq
%else
        push    edi
        push    esi

        mov     ecx, [esp + 0ch + 8]
        mov     edi, [esp + 04h + 8]
        mov     esi, [esp + 08h + 8]
        mov     edx, ecx
        mov     eax, edi                ; save the return value
        shr     ecx, 2
        rep movsd
%endif

        ; The remaining bytes.
%ifdef RT_ARCH_AMD64
        test    dl, 4
        jz      .dont_move_dword
        movsd
%endif
.dont_move_dword:
        test    dl, 2
        jz      .dont_move_word
        movsw
.dont_move_word:
        test    dl, 1
        jz      .dont_move_byte
        movsb
.dont_move_byte:

%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_MSC
        mov     rdi, r10
        mov     rsi, r11
 %endif
%else
        pop     esi
        pop     edi
%endif
        ret
ENDPROC RT_NOCRT(memcpy)

