// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

#ifdef _WIN32
.section .rdata,"dr"
#else
.section .rodata
.hidden mkguiicons_data
#endif
.balign 16
.global mkguiicons_data
mkguiicons_data:
.incbin "out/icons/data.bin"
#ifndef _WIN32
.section .note.GNU-stack,"",@progbits
#endif
