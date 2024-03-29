; $Id: remainderl.asm 4071 2007-08-07 17:07:59Z vboxsync $
;; @file
; innotek Portable Runtime - No-CRT remainderl - AMD64 & X86.
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
%else
 %define _SP esp
 %define _BP ebp
%endif

;;
; See SUS.
; @returns st(0)
; @param    lrd1    [rbp + 10h]
; @param    lrd2    [rbp + 20h]
BEGINPROC RT_NOCRT(remainderl)
    push    _BP
    mov     _BP, _SP

%ifdef RT_ARCH_AMD64
    fld     tword [rbp + 10h + RTLRD_CB]
    fld     tword [rbp + 10h]
%else
    fld     tword [ebp + 8h + RTLRD_CB]
    fld     tword [ebp + 8h]
%endif

    fprem1
    fstsw   ax
    test    ah, 04h
    jnz     .done
    fstp    st1

.done:
    leave
    ret
ENDPROC   RT_NOCRT(remainderl)

