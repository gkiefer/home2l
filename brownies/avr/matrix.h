/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2019-2020 Gundolf Kiefer
 *
 *  Home2L is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Home2L is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Home2L. If not, see <https://www.gnu.org/licenses/>.
 *
 */


#ifndef _MATRIX_
#define _MATRIX_

#include "core.h"

#if !WITH_MATRIX
EMPTY_MODULE(Matrix)
#else  // WITH_MATRIX


void MatrixInit ();
void MatrixIterate ();
void MatrixOnRegRead (uint8_t reg);
void MatrixOnRegWrite (uint8_t reg, uint8_t val);


#endif // WITH_MATRIX

#endif // _MATRIX_
