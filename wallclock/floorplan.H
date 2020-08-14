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


#ifndef _FLOORPLAN_
#define _FLOORPLAN_

#include "ui_widgets.H"


/** @file
 *
 * @addtogroup wallclock
 * @{
 *
 * @addtogroup wallclock_floorplan
 *
 * @{
 */


#define FP_WIDTH 256
  ///< @brief Logical width of the floorplan.
  /// The logical size of the floorplan is identical to the size in pixels of the mini view.
#define FP_HEIGHT 128
  ///< @brief Logical height of the floorplan.
#define FP_UPDATE_INTERVAL 64
  ///< @brief Number of milliseconds between updates
#define COL_FP_MAIN DARK_BLUE
  ///< @brief Main color of the floorplan screen
#define COL_FP_MAIN_DARKER ColorDarker (DARK_BLUE, 0x40)  ///< @internal


/** Gadget Type
 *
 * These constants are used to identify a gadget type and for the resource dialog.
 */
enum EGadgetType {
  gtNone = 0,

  /// @name Windows, shades and gates...
  /// @{
  gtWindow,       ///< window with a state of @ref ERctWindowState.
  gtShades,       ///< shades (type @ref rctPercent)
  gtRoofWindow,   ///< roof window, optionally including shades
  gtGarage,       ///< garage door (open/close)
  /// @}

  /// @name Icon-like ...
  /// @{
  gtLock,         ///< door lock
  gtMotion,       ///< motion status
  gtLight,        ///< light
  gtMail,         ///< mail box
  gtPhone,        ///< phone (with state @ref ERctPhoneState)
  gtMusic,        ///< music player
  gtWlan,         ///< Wifi access point
  gtBluetooth,    ///< Bluetooth
  gtService,      ///< some generic service
  /// @}

  /// @name Text...
  /// @{
  gtTemp,         ///< temperature value
  /// @}

  /// @name Meta ...
  /// @{
  gtZoom,
  gtEND
  /// @}
};





// *************************** Resource Dialog *********************************


void RunResourceDialog (class CResource *rc, EGadgetType subType = gtNone, const char *title = NULL);
  ///< @brief Run a dialog allowing to edit, add or delete a user request.
  ///
  /// @param rc is the resource.
  /// @param subType is a subtype indication. For example, if 'rc->Type()' is @ref rctPercent, setting the
  ///    subtype to @ref gtWindow will optimize the UI for a window opener ("%100" at the top), if set to
  ///    @ref gtShades, the UI presents a UI for shades ("0%" at the top).
  /// @param title is the title of the dialog. If set to NULL, the title will be derived from 'subType'.
  ///    If 'subType == gtNone', a generic default title will be shown.





// *************************** CWidgetFloorplan ********************************


/** @brief Floorplan widget.
 *
 * This is a mini view of the floorplan with 256x128 pixels.
 */
class CWidgetFloorplan: public CFlatButton, public CTimer {
  public:
    CWidgetFloorplan ();
    virtual ~CWidgetFloorplan ();

    void Setup (int x0, int y0, class CFloorplan *_floorplan = NULL, TTicksMonotonic _tInterval = FP_UPDATE_INTERVAL);
      ///< Setup the widget.
    bool IsOk () { return floorplan != NULL; }

    void Activate (bool on = true);
      ///< @brief (De-)Activate automatic updates of the mini-floorplan.
      ///
      /// This implies (un-)claiming the attached floorplan object.
      /// I.e., when active, an automatic timer runs 'CFloorplan::Iterate ()',
      /// and during activation, the floorplan's view level is set accordingly.
      /// This method should be called from the owning screen's Activate() method
      /// accordingly.

  protected:

    // Callbacks ...
    virtual void OnTime ();                   // from 'CTimer'
    virtual SDL_Surface *GetSurface ();       // from 'C[Flat]Button'
    virtual void OnPushed (bool longPush);    // from 'C[Flat]Button'

    // Variables ...
    class CFloorplan *floorplan;
    TTicksMonotonic tInterval;

    SDL_Surface *mapSurf;
};





// *************************** Top-Level ***************************************


void FloorplanInit ();
void FloorplanDone ();

void FloorplanActivate ();
  ///< @brief Activate the floorplan screen.

void FloorplanCheckAlert (CScreen *returnScreen = NULL);
  ///< @brief Check for an alert and if so activate the floorplan screen.
  /// @param returnScreen specifies the screen to return to as soon as the alert
  ///   is over. If NULL, the floorplan screen will remain activated when the
  ///   alert is over.

class CFloorplan *FloorplanGetMain ();
  ///< @brief Get a reference to the main floorplan (for CWidgetFloorplan::Setup() ).

void FloorplanUnsubscribeAll ();
  ///< @brief Unsubscribe all resources of the floorplan.
  ///
  /// On activation of the floorplan screen (FloorplanActivate () ) or some floorplan
  /// widget (CWidgetFloorplan), the resource subscriptions are updated to
  /// fit the screen or widget. On deactivation of those, nothing is changed.
  /// This function explicitly unsubscribes all resources to save power and CPU time.



/// @}  // wallclock_floorplan
/// @}  // wallclock


#endif
