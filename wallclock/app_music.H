/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2015-2018 Gundolf Kiefer
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


#ifndef _APP_MUSIC_
#define _APP_MUSIC_

#include "base.H"


/** @file
 * @addtogroup wallclock
 * @{
 *
 * @defgroup wallclock_music Music Player
 * Interacting with the music player.
 *
 * @{
 */


#if WITH_MUSIC == 1


void AppMusicPlayerOn ();
  ///< Switch on the music player. If necessary, a connection the the last MPD servcer is openend, and that
  /// player is set into the "play" state.
void AppMusicPlayerOff ();
  ///< Stop the music player.


bool AppMusicIsPlaying ();
  ///< Return 'true' if the player is actually playing
bool AppMusicIsPlayingOrShouldBe ();
  ///< Return 'true' if the player is either actually playing or should be playing,
  /// but is not for a reason not intended by the user.
  /// Use this variant to decide if background mode can be quit.
bool AppMusicIsPlayingForSure (int minDb = -INT_MAX);
  ///< Return 'true' if the player is actually playing and it is guaranteed that either the user can hear something
  /// or the player is muted. (At least this is the goal - there may be some more holes to fix, for example, if the
  /// music itself contains silence. The latter, however, is addressed by the 'minDb' argument if the output is
  /// streamed to the local device.)
  /// Use this variant for an alarm clock to check if the user gets woken up or if some other measure has to be taken.
  /// 'minDb' is the minimum required DB level (< 0; 0 = max.) that is considered to be sufficiently hearable.


#else // WITH_MUSIC == 1


static inline void AppMusicPlayerOn () {}
static inline void AppMusicPlayerOff () {}

static inline bool AppMusicIsPlaying () { return false; }
static inline bool AppMusicIsPlayingOrShouldBe () { return false; }
static inline bool AppMusicIsPlayingForSure () { return false; }


#endif // WITH_MUSIC == 1


/// @}  // wallclock_music
/// @}  // wallclock


#endif // _APP_MUSIC_
