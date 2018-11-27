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


#ifndef _STREAMER_
#define _STREAMER_

#include "base.H"


enum EStreamerState {
  strOff = 0,      // No background thread, hands off the audio device
  strError,        // e.g., connection failed
  strBusy,         // connecting or buffering
  strOn            // audio should be hearable
};


static inline bool StreamerStateIsActive (EStreamerState s) { return s >= strBusy; }
  // Active states are those requiring eventual calls to 'StreamerIterate'.
  // 'StreamerStop' reliably switches to an inactive state.


// Note on concurrency: All interface methods here must be called from the main UI thread.

void StreamerInit ();
void StreamerDone ();

void StreamerStart (const char *host, int port, TTicksMonotonic bufferDuration);
void StreamerStop ();

EStreamerState StreamerIterate ();

EStreamerState StreamerState ();
static inline bool StreamerIsRunning () { return StreamerStateIsActive (StreamerState ()); }

const char *StreamerGetError (CString *s);   // return readable error message and reset the text internally
const char *StreamerGetError (); // (TTS)


#define STREAMER_LEVEL_UNKNOWN -INT_MAX

int StreamerGetDbLevel ();
  // Return the current level in DB (<= 0, 0 = maximum)
  // or STREAMER_LEVEL_UNKNOWN (= -INT_MAX / very, very silent), if the level is unknown.
  // Level measurement is activated by the first call to this function, and deactivated again
  // in 'StreamerStop ()'. The first call will most probably return STREAMER_LEVEL_UNKNOWN.
  // When activated, additional CPU load and power consumption may occur.


#endif
