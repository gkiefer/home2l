/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2020 Gundolf Kiefer
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


#ifndef _APP_PHONE_
#define _APP_PHONE_

#include "base.H"


/** @file
 *
 * @addtogroup wallclock
 * @{
 *
 * @addtogroup wallclock_phone
 *
 * @{
 */


#if WITH_PHONE == 1 || DOXYGEN

void AppPhoneDial (const char *url, CScreen *returnScreen = NULL);
  ///< @brief Activate the phone applet and dial the given url/number.
  /// If 'returnScreen != NULL', the phone immediately returns to (activates)
  /// the passed screen instead of going into phone idle mode.

#else // WITH_PHONE == 1

static inline void AppPhoneDial (const char *, CScreen *) {
  WARNING ("AppPhoneDial(): No phone available.");
}

#endif // WITH_PHONE == 1



/// @}  // wallclock_phone
/// @}  // wallclock


#endif // _APP_PHONE_
