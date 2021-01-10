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


#include "phone.H"

#include "env.H"



// *************************** Environment settings ****************************


ENV_PARA_PATH ("phone.ringbackFile", envPhoneRingbackFile, "share/sounds/ringback.wav");
  /* Ringback file (Linphone backend only)
   *
   * This is the sound to be played to the caller while ringing.
   */

ENV_PARA_PATH ("phone.playFile", envPhonePlayFile, NULL);
  /* Phone play file (Linphone backend only)
   *
   * This is the background music played to a caller during transfer.
   * (may be removed in the future since PBX systems like ASTERISK
   * already provide this functionality)
   */
