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


#ifndef _ALARMCLOCK_
#define _ALARMCLOCK_

#include "ui_base.H"


/** @file
 *
 * @addtogroup wallclock
 * @{
 *
 * @addtogroup wallclock_alarmclock
 *
 * @{
 */


/// @name Alarm clock state ...
/// @{
enum TAlarmClockState {
  acsDisabled = 0,      ///< completely off
  acsStandby,           ///< in standby state (wakeup time may be set, but is in the future)
  acsSnooze,            ///< in snooze state
  acsAlarmPreRinging,   ///< ringing internally initially, before starting the music player
  acsAlarmMusicTrying,  ///< alarm with music player on (hopefully soon [again])
  acsAlarmMusicOk,      ///< alarm with music player on (for sure)
  acsAlarmRinging,      ///< alarm with internal bell (music player failed)
};

static inline bool AlarmClockStateIsAlarm (TAlarmClockState state) { return state >= acsAlarmPreRinging; }
TAlarmClockState AlarmClockGetState ();     ///< @brief Get the current alarm clock state.
/// @}


/// @name Init/done ...
/// @{
void AlarmClockInit ();   ///< @brief @brief Initialize the alarm clock.
void AlarmClockDone ();   ///< @brief Shutdown the alarm clock.
/// @}


/// @name Direct actions ...
/// @{
void AlarmClockEnableDisable (bool enable);    ///< @brief Enable/disable alarm clock as a whole.
void AlarmClockOff ();              ///< @brief "Off" button of the alarm clock => go to standby state.
void AlarmClockSnooze ();           ///< @brief "Snooze" button of the alarm clock.
/// @}


/// @name High-level actions / UI integration ...
/// @{
void AlarmClockSetButton (class CButton *btn);
  ///< @brief Define a main button. Its surface is set to indicate the alarm state.
  ///
  /// When pushed in a non-alarm state, the setup dialog is run.
  /// When pushed during an alarm, the alarm is snoozed on a short pushed and
  /// switched off on a long push.
  /// The push behaviour is identical to calling 'AlarmClockHandlePushed' with
  /// 'enableOff = true'.
void AlarmClockHandlePushed (bool enableOff, bool longPush);
  ///< @brief Handle a push on (one of) the alarm clock button(s) in the interface.
  ///
  /// In off/standby state, a push opens the setup dialog.
  /// If allowed ('enableOff == true'), a long push switches of alarm during alarm.
  /// Any other push during alarm
/// @}


/// @name Setup dialog ...
/// @{
void AlarmClockRunSetDialog ();     ///< @brief Run the setup screen to edit alarm times.
/// @}


/// @}  // wallclock_alarmclock
/// @}  // wallclock


#endif // _ALARMCLOCK_
