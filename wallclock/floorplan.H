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


void FloorplanInit ();
void FloorplanDone ();

void FloorplanActivate ();
  ///< @brief Activate the floorplan screen.

class CFloorplan *FloorplanGetMain ();
  ///< @brief Get a reference to the main floorplan (for CWidgetFloorplan::Setup() ).

void FloorplanUnsubscribeAll ();
  ///< @brief Unsubscribe all resources of the floorplan.
  ///
  /// On activation of the floorplan screen (FloorplanActivate () ) or some floorplan
  /// widget (CWidgetFloorplan), the resource subscriptions are updated to
  /// fit the screen or widget. On deactivation, nothing is changed.
  /// This function explicitly unsubscribes all resources to save power and CPU time.





// *************************** CWidgetFloorplan ***********************************


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
    TTicksMonotonic tInterval, tAlert;

    SDL_Surface *mapSurf;
};



/// @}  // wallclock_floorplan
/// @}  // wallclock


#endif
