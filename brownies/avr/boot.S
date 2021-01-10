; This file is part of the Home2L project.
;
; (C) 2015-2021 Gundolf Kiefer
;
; Home2L is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; Home2L is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with Home2L. If not, see <https://www.gnu.org/licenses/>.



; **************************** Reset/Interrupt Vectors *************************

#if IS_MAINTENANCE

#if BROWNIE_BASE >= 0x0040

        .section        .boot,"ax",@progbits
        rjmp BROWNIE_BASE +0x00
        rjmp BROWNIE_BASE +0x02
        rjmp BROWNIE_BASE +0x04
        rjmp BROWNIE_BASE +0x06
        rjmp BROWNIE_BASE +0x08
        rjmp BROWNIE_BASE +0x0a
        rjmp BROWNIE_BASE +0x0c
        rjmp BROWNIE_BASE +0x0e
        rjmp BROWNIE_BASE +0x10
        rjmp BROWNIE_BASE +0x12
        rjmp BROWNIE_BASE +0x14
        rjmp BROWNIE_BASE +0x16
        rjmp BROWNIE_BASE +0x18
        rjmp BROWNIE_BASE +0x1a
        rjmp BROWNIE_BASE +0x1c
        rjmp BROWNIE_BASE +0x1e
        rjmp BROWNIE_BASE +0x20

#endif

#endif // IS_MAINTENANCE
