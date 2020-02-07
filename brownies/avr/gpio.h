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


#ifndef _GPIO_
#define _GPIO_

#include "core.h"


#if !WITH_GPIO
EMPTY_MODULE(Gpio)
#else   // WITH_GPIO


#include <avr/io.h>


#if GPIO_HAVE_UPPER
typedef uint16_t TGpioWord;
#else
typedef uint8_t TGpioWord;
#endif


extern TGpioWord gpioLastIn;


static inline void GpioInit () {
  register TGpioWord regVal;

  gpioLastIn = GPIO_FROM_PMASK (P_IN_MULTI (GPIO_TO_PMASK (GPIO_IN_PRESENCE)));
  regVal = gpioLastIn | GPIO_OUT_PRESET;
  RegSet (BR_REG_GPIO_0, regVal & 0xff);
  RegSet (BR_REG_GPIO_1, regVal >> 8);
}


static inline void GpioIterate () {
  register TGpioWord gpioIn = GPIO_FROM_PMASK (P_IN_MULTI (GPIO_TO_PMASK (GPIO_IN_PRESENCE)));
  if (gpioIn != gpioLastIn) {
    ReportChange (BR_CHANGED_GPIO);
    gpioLastIn = gpioIn;
  }
}


static inline void GpioOnRegRead (uint8_t reg) {
  register TGpioWord regVal;

  if (reg == BR_REG_GPIO_0 || (GPIO_HAVE_UPPER && reg == BR_REG_GPIO_1)) {
    GpioIterate ();
    if (!GPIO_HAVE_UPPER) {
      regVal = (RegGet (BR_REG_GPIO_0) & GPIO_OUT_PRESENCE) | gpioLastIn;
      RegSet (BR_REG_GPIO_0, regVal);
    }
    else {
      regVal = ((RegGet (BR_REG_GPIO_0) | (((uint16_t) RegGet (BR_REG_GPIO_1)) << 8)) & GPIO_OUT_PRESENCE) | gpioLastIn;
      RegSet (BR_REG_GPIO_0, regVal & 0xff);
      RegSet (BR_REG_GPIO_1, regVal >> 8);
    }
  }
}


static inline void GpioOnRegWrite (uint8_t reg, uint8_t val) {
  if (reg == BR_REG_GPIO_0) {
    P_OUT_MULTI (GPIO_TO_PMASK (LO (GPIO_OUT_PRESENCE)), GPIO_TO_PMASK (HILO (0, val)));
    RegSet (BR_REG_GPIO_0, (RegGet (BR_REG_GPIO_0) & ~LO(GPIO_OUT_PRESENCE))
                         | (val                    &  LO(GPIO_OUT_PRESENCE)));
  }
#if GPIO_HAVE_UPPER
  if (reg == BR_REG_GPIO_1) {
    P_OUT_MULTI (GPIO_TO_PMASK (GPIO_OUT_PRESENCE & 0xff00), GPIO_TO_PMASK (HILO(val, 0)));
    RegSet (BR_REG_GPIO_1, (RegGet (BR_REG_GPIO_1) & ~HI(GPIO_OUT_PRESENCE))
                         | (val                    &  HI(GPIO_OUT_PRESENCE)));
  }
#endif  // !GPIO_HAVE_UPPER
}


#endif  // WITH_GPIO


#endif // _GPIO_
