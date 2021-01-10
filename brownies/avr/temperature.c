/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2015-2021 Gundolf Kiefer
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


#include "configure.h"

#if WITH_TEMP_ZACWIRE

#include "temperature.h"


/* NOTE: This module makes and assumes exclusive use of the 8-bit timer.
 */


#if BR_CPU_FREQ != 1000000
#error "This module asserts a CPU clock frequency of 1 MHz"
#endif


volatile uint16_t temperatureValue;
uint16_t temperatureTimeUpdated;

#endif // WITH_TEMP_ZACWIRE
