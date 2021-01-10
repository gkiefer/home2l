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


#ifndef _APPS_
#define _APPS_


/** @file
 *
 * @defgroup wallclock WallClock
 * @brief API to implement new *WallClock* applets.
 * @{
 *
 * @defgroup wallclock_system System
 * @brief Accessing system-specific functionalities and hardware.
 *
 * @defgroup wallclock_floorplan Floorplan
 * @brief Floorplan visualization.
 *
 * @defgroup wallclock_alarmclock Alarm Clock
 * @brief Interacting with the integrated alarm clock.
 *
 * @defgroup wallclock_phone Phone
 * @brief Interacting with the phone applet.
 *
 * @defgroup wallclock_music Music Player
 * @brief Interacting with the music player.
 *
 * @defgroup wallclock_apps Applets
 * @brief Managing applets.
 *
 * ### Developing new applets
 *
 * A new applet - say "Foobar" is integrated as follows.
 *
 * - Add a new entry to `apps_config.H`:
 *    @code
 *    #if WITH_FOOBAR == 1
 *    APP(Foobar, "foobar")
 *    #endif
 *    @endcode
 *
 * - Implement your applet in a file named `app_foobar.C`.
 *   If the codes requires multiple files, name the lower-level files like `app_foobar_mysubmodule.C`.
 *
 * - The file `app_foobar.C` must contain the main applet function named `AppFoobarFunc()`
 *   (see @ref FAppFunc) which implements all mandantory (= non-optional) operations from @ref EAppOperation.
 *
 * - Adapt the `Makefile` accordingly to support the `WITH_FOOBAR` setting and include all necessary
 *   .C files.
 *
 * @nosubgrouping
 *
 * @{
 */


#include "config.H"
#include "ui_base.H"


enum EAppId {
  appIdNONE = -1,
#define APP(NAME, ENV_PREFIX) appId##NAME,
#include "apps_config.H"
#undef APP
  appIdEND
};


#define APP_INIT_OK ((void *) 1)  ///< Return value for 'appOpInit': Initialization was successful.
#define APP_INIT_FAIL NULL        ///< Return value for 'appOpInit': Initialization failed.


/// Applet operations.
enum EAppOperation {
  appOpInit = 0,      ///< Initialize applet.
                      ///  - **Argument:**  (none)
                      ///  - **Return:**    APP_INIT_OK | APP_INIT_FAIL
                      ///
                      /// If APP_INIT_FAIL is returned, no other operations will be called again.

  appOpDone,          ///< Shutdown applet; Called on shutdown (not for disabled apps).

  appOpLabel,         ///< Set the launcher label.
                      ///  - **Argument:**  CButton *launchButton = (CButton *) data;
                      ///  - **Return:**    return NULL;
                      ///
                      /// Set the 'launchButton' label using its 'CButton::SetLabel*' methods.
                      /// The app may store the latest passed button object and later change the label arbitrarily.
                      /// The label must be transparent be colored according to the COL_APP_LABEL_* color scheme.
                      /// 'launchButton' may be NULL, indicating that not button changes can be performed.

  appOpActivate,      ///< Activate the main screen (no arguments, return value ignored).

  appOpLongPush,      ///< A long push on the launcher button has been exercised (optional).
                      /// **Note:** The applet has not been activated yet and will not be activated automatically.

  appOpUser = 0x80    ///< First user-defined operation.
};


typedef void *FAppFunc (int appOp, void *data);
  ///< @brief Prototype for applet entry functions.
  /// Example: `void *AppPhoneFunc (int appOp, void *data)`


///@name General ...
///@{
void AppsInit ();
void AppsDone ();

static inline int Apps () { return (int) appIdEND; }
///@}


///@name Sending intents to applets ...
///@{
void *AppCall (EAppId appId, int appOp, void *data = NULL);
static inline void AppActivate (EAppId appId) { AppCall (appId, appOpActivate); }
static inline void AppEscape () { AppActivate (appIdHome); }
bool AppEnabled (EAppId appId);
///@}


///@name Helpers for launch buttons ...
///@{
#define COL_APP_LABEL       ToColor (0x60, 0x60, 0x60)      ///< Normal launcher labels.
#define COL_APP_LABEL_LIVE  WHITE                           ///< App is active, and the user must be aware of that.
#define COL_APP_LABEL_BUSY  LIGHT_RED                       ///< App is busy with something.
#define COL_APP_LABEL_ALARM ToColor (0xff, 0x40, 0x40)      ///< Something very urgant/important is with the app.

extern TTF_Font *fntAppLabel;                               ///< Default font for launch buttons (read-only!).

#define APP_SET_LAUNCHER(BTN, ICON_NAME, TEXT, HOTKEY) \
            ((CButton *) (BTN))->SetLabel (COL_APP_LABEL, ICON_NAME "-48", TEXT, fntAppLabel); \
            ((CButton *) (BTN))->SetHotkey (HOTKEY);
  ///< @brief Set launcher appearance (standard color) and hotkey; Typical use: initialization.
  ///
  /// Example:
  /// @code
  /// APP_SET_LAUNCHER(data, "ic_foobar", "FooBar", SDLK_f);
  /// @endcode
#define APP_SET_LABEL(BTN, ICON_NAME, TEXT, COLOR) \
            ((CButton *) (BTN))->SetLabel ((COLOR), ICON_NAME "-48", TEXT, fntAppLabel)
  ///< @brief Set launcher appearance; Typical use is to change color or label.
  ///
  /// Example:
  /// @code
  /// APP_SET_LAUNCHER(data, "ic_foobar", "FooBar", COL_APP_LIVE);
  /// @endcode
#define APP_SET_HOTKEY(BTN, HOTKEY) \
            ((CButton *) (BTN))->SetHotkey (HOTKEY);
  ///< @brief Set the hotkey.
///@}


///@name Convenience callbacks for push buttons ...
///@{
void CbAppActivate (class CButton *, bool, void *appId);     // activates app, 'appId' is of type 'EAppId'
void CbAppEscape (class CButton *, bool, void *);            // for "back" button
///@}



/** @}  // wallclock_apps
 * @}   // wallclock
 */


#endif
