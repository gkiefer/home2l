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


#ifndef _SYSTEM_
#define _SYSTEM_


/** @file
 * @addtogroup wallclock
 * @{
 *
 * @defgroup wallclock_system System
 * Accessing system-specific functionalities and hardware.
 *
 * @nosubgrouping
 *
 * @{
 */


#include "ui_base.H"





// *************************** General *****************************************


/// Written name of the *WallClock* application ...
#define WALLCLOCK_NAME "WallClock"  // "Terminal" // "Klaus-Dieter"


/// @name General ...
/// @{


void SystemInit ();
void SystemDone ();


/// @}




// *************************** System mode *************************************


///
/** @name System mode ...
 *
 * To save power, a set of system modes is defined (see @ref ESystemMode ).
 *
 * In Android, the system modes are implemented as follows:
 *   - **smBackground:** Activity is in the background (i.e. not visible)
 *   - **smOff:**     very low brightness (50% of brightnessStandbyMin), 'FLAG_KEEP_SCREEN_ON' is unset (Android can put device in sleep mode)
 *   - **smStandby:** low brightness, 'FLAG_KEEP_SCREEN_ON' set
 *   - **smActive:**  normal brightness, 'FLAG_KEEP_SCREEN_ON' set
 *
 * In all modes including 'smOff', touching the screen/keyboard or triggering a light change causes a
 * switch to mode 'smActive'.
 *
 * If the device is put in sleep mode from outside (e.g. power button), this does not have
 * an effect on the internal system mode. In consequence, the device must manually be activated
 * again. This, however, may be helpful, e.g. to clean the display.
 *
 * In passive behaviour mode, the following simplified scheme is used:
 *   smBackground: Activity is not visible (paused)
 *   smStandby: 'FLAG_KEEP_SCREEN_ON' not set
 *   smActive:  'FLAG_KEEP_SCREEN_ON' set
 *
 * The app being paused does not imply 'smBackground', but any of the other states may also apply
 * (depending on the reason for pausing). The brightness is never controlled actively, and the
 * mechanism to fetch the app to the foreground automatically after some time is also disabled.
 *
 * @{
 */


/// System mode
enum ESystemMode {
  smNone = -1,          ///< Uninitialized; Value must not be used after initialization.
  smBackground = 0,     ///< Application is in background (e.g. other application in focus).
  smOff,                ///< Screen is off
  smStandby,            ///< Screen is in standby mode (not bright, but visible).
  smActive              ///< Screen is in active mode (bright, e.g. during user interaction)
};


ESystemMode SystemGetMode (); ///< Get the current system mode.


void SystemWakeupActive ();   ///< Wakeup into active mode (e.g. due to user interaction); Reset the standby & off timers.
void SystemWakeupStandby ();  ///< Wakeup into standby mode; Reset the off timer.
static inline void SystemWakeup () { SystemWakeupActive (); }   ///< Unspecific wakeup (redirects to SystemWakeupActive() ).


void SystemActiveLock (const char *reqName);
  ///< Lock the device in active mode, e.g. during a phone call.
  /// @param reqName is the ID of the request as visible in the resource(s).
void SystemActiveUnlock (const char *reqName);
  ///< Remove the specified active lock.
  /// @param reqName is the ID of the request as previously passed to SystemActiveLock().


void SystemStandbyLock (const char *reqName);
  ///< Lock the device in standby mode to prevent the screen to be switched off completely, e.g. during alarm clock wakeup.
  /// @param reqName is the ID of the request as visible in the resource(s).
void SystemStandbyUnlock (const char *reqName);
  ///< Remove the specified standby lock.
  /// @param reqName is the ID of the request as previously passed to SystemActiveLock().


/// @}





// *************************** Fore/background *********************************


/// @name Enter or return from background...
/// @{


void SystemGoBackground (const char *appStr = NULL);
  ///< Move the *WallClock* (Android) app into the background.
  /// @param appStr is the other Android app to be launched. By default, the home screen is activated.
void SystemGoForeground ();
  ///< Move the *WallClock* (Android) app into the foreground again.
static inline void SystemLaunchApp (const char *appStr) { SystemGoBackground (appStr); }
  ///< Launch an (Android) app ...

void SystemReportUiVisibility (bool foreNotBack);
  ///< Report background/foreground state from UI system (for 'ui_base.C').


/// @}


//~ // ***************** Immersive Mode ********************
//~
//~ void SystemSetImmersiveMode ();





// *************************** Muting ******************************************


/// @name Muting ...
/// The "mute" flag demands all "medium-priority" audio to be muted (e.g. during phone calls)
/// and presently affects the music player. The bit has no effect on the phone,
/// phone or door bell ringing or the alarm clock.
/// @{


class CResource *SystemGetMuteRc ();      ///< Get the "mute" resource.

void SystemMute (const char *reqName);    ///< Mute.
void SystemUnmute (const char *reqName);  ///< Unmute.
  ///< @param reqName is the name of the request as visible in the resource(s).
bool SystemIsMuted ();                    ///< Query the mute state in a polling fashion.


/// @}





// *************************** Audio *******************************************


/// @name Audio ...
/// **Deprecated:** The following functions are deprecated and can be removed
/// if all phone audio is directed via SDL.
/// @{


// TBD: Eliminate the following functions?

void SystemSetAudioNormal ();
void SystemSetAudioPhone ();
//~ void SystemSetAudioRinging ();


/// @}





// *************************** Bluetooth ***************************************


/// @name Bluetooth
/// @{


class CResource *SystemGetBluetoothRc ();
  ///< Get readable/writable resource to indicate if bluetooth is generally on.
class CResource *SystemGetBluetoothAudioRc ();
  ///< Get read-only resource to indicate if BT audio (output) is available.

bool SystemBluetoothGetState (bool *retBusy = NULL, bool *retAudio = NULL);
  ///< Return 'true' iff BT is currently on or switching on.
  /// @param *retBusy indicates whether BT is currently switching state.
  /// @param *retAudio is true, iff an audio device is coupled ("a2dp" or "headset" profile).
void SystemBluetoothSet (bool enable);
  ///< Generally enable or disable Bluetooth.
static inline void SystemBluetoothToggle () { SystemBluetoothSet (!SystemBluetoothGetState ()); }
  ///< Toggle the Bluetooth enable status.


/// @}




/** @}  // wallclock_system
 * @}   // wallclock
 */


#endif
