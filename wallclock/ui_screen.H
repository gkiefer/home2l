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


#ifndef _SCREEN_
#define _SCREEN_


/** @file
 * @addtogroup ui
 * @{
 *
 * @defgroup ui_screen Screen
 * Screen and widget management.
 *
 * A *screen* in this toolkit is similar to what is a window on a desktop system or an "activity" in Android.
 * It represents the present screen contents and covers the whole display. At a time, only one screen can be
 * active.
 *
 * A screen contains a stack of *widgets*, each covering a rectangular part of the screen. New widgets are
 * added on top and may hide (parts of) other widgets added earlier.
 *
 * @{
 */


#include "ui_base.H"


/* Some pieces of the code are intentionally robust enough to tolerate 'this == NULL'.
 * GCC 6 by default emits warnings in this case. We disable them.
 */
#if defined(__GNUC__) && !ANDROID
#pragma GCC diagnostic ignored "-Wnonnull-compare"
#endif





// *************************** General *****************************************


/// @name General definitions ...
/// @{


#define UI_BUTTONS_HEIGHT 64    ///< Height of the button bar.
#define UI_BUTTONS_SPACE 8      ///< Standard spacing between buttons.
#define UI_BUTTONS_RECT Rect (0, UI_RES_Y - UI_BUTTONS_HEIGHT, UI_RES_X, UI_BUTTONS_HEIGHT)   ///< Button area on the bottom of the display.
#define UI_BUTTONS_BACKWIDTH (UI_BUTTONS_HEIGHT + UI_BUTTONS_HEIGHT/2)    ///< Standard width of the "back" button,

#define UI_USER_RECT Rect (0, 0, UI_RES_X, UI_RES_Y - UI_BUTTONS_HEIGHT - UI_BUTTONS_SPACE)
  ///< Rectangle describing the screen space usable for applets.


/// @}


class CScreen;





// *************************** CWidget *****************************************


/** Base class for all widgets.
 * This class is not abstract, but by itself allows to display arbitrary SDL surfaces.
 */
class CWidget {
  public:
    CWidget ();
    virtual ~CWidget () { ClearTexture (); }

    /// @name Setup ...
    /// @{
    void Set (SDL_Surface *_surf, int x0, int y0) {
      SetArea (Rect (_surf)); RectMove (&area, x0, y0); SetSurface (_surf);
    }

    void SetArea (SDL_Rect _area) { area = _area; }
    SDL_Rect *GetArea () { return &area; }

    class CScreen *GetScreen () { return screen; }
    class CCanvas *GetCanvas () { return canvas; }
    /// @}

    /// @name Coordinate transformations (screen <-> render <-> local) ...
    /// @{
    void LocalToScreenCoords (int *x, int *y);
    void ScreenToLocalCoords (int *x, int *y);

    void GetMouseEventPos (SDL_Event *ev, int *x, int *y);
    /// @}

    /// @name Surface-oriented drawing ...
    /// This class itself is able to display a non-interactive surface.
    /// @{
    void SetSurface (SDL_Surface *_surface) { surface = _surface; ChangedSurface (); }
      ///< Set the static surface to display.
      /// The caller stays owner of surface, must not be deleted afterwards.
      /// The surface dimensions must match 'area'.
    virtual SDL_Surface *GetSurface () { return surface; }
      ///< Get an up-to-date surface or 'NULL' if none is available.
    /// @}

    /// @name Texture-oriented drawing...
    /// @{
    void GetRenderArea (SDL_Rect *ra) { *ra = area; LocalToScreenCoords (&ra->x, &ra->y); }
      ///< Transpose 'area' to the current screen coordinates to be passed to all SDL rendering functions.
    virtual SDL_Texture *GetTexture ();
      ///< Return an up-to-date texture.
      /// The default implementation maintains an internal texture that is updated based on
      /// the surface set by SetSurface(). Widgets that generate SDL texture objects directly
      /// may override this method.
    void SetTextureBlendMode (SDL_BlendMode _sdlBlendMode) { sdlBlendMode = _sdlBlendMode; }
      ///< Set the blend mode for the texture created by the default implementation of GetTexture().
    virtual void Render (SDL_Renderer *ren);
      ///< Render this widget.
      /// The default implementation uses the 'GetTexture()' method above. By overriding this method
      /// it is possible to define widgets with a custom SDL rendering procedure, which may be more
      /// efficient than preparing a single texture containing the complete widget.
    void ClearTexture ();   ///< Clear the internally cached texture object to save memory.
    /// @}


  protected:
    friend class CScreen;
    friend class CCanvas;

    CWidget *next;
    class CScreen *screen;
    class CCanvas *canvas;

    SDL_Rect area;
    SDL_Surface *surface;
    SDL_Texture *texture;
    SDL_BlendMode sdlBlendMode;

    /// @name Event handling ...
    /// @{
    virtual bool HandleEvent (SDL_Event *ev) { return false; }
      ///< Handle an event and return 'true' if the event was consumed and is to be ignored by later widgets.
    /// @}

    /// @name Change management...
    /// The following methods trigger a redrawing at next occasion. The following conventions
    /// must be satisfied by this and all derived classes:
    /// 1. On a change, the most specialized 'Change*()' function is called, which may in turn call other 'Change*()' methods.
    /// 2. All public methods call their change methods => no need to make these public.
    /// 3. The 'Change*()' methods must be very fast (i.e. O(1)) in execution, at least if invoked repeatedly.
    /// @{
    void ChangedSurface () { Changed (); }  ///< Mark (only) the surface as changed to trigger a redrawing at next occasion
    void Changed ();                        ///< Anything may have changed: Trigger a redrawing at next occasion
    /// @}

  private:

    // Helpers...
    static void RenderList (CWidget *list, SDL_Renderer *ren);
      // Render this and all widgets of the linked list behind 'next'; first widgets appear on top
};





// *************************** CCanvas *****************************************


/** Canvas widget.
 * A *canvas* is a widget with a virtual drawable area, which is not equivalent to the actually visible
 * area. This is the base class for all potentially scrollable widgets such as CListBox or CMenu.
 * Scroll bars are displayed automatically as needed, and wiping finger events are evaluated for scrolling.
 *
 * Finger events on the right (lower) half are handled for scrolling, events on the other half of the widget
 * are ignored and may be interpreted for other widget functionality (selecting etc.).
 * The right/bottom 1/8 of the canvas contains the scroll bar.
 */
class CCanvas: public virtual CWidget {
  public:
    CCanvas () { virtArea = Rect (0, 0, 0, 0); firstWidget = NULL; SetDefaults (); }
    //~ virtual ~CCanvas () {}

    /// @name Setup ...
    /// @{
    void SetDefaults () { SetColors (); SetScrollbarWidth (); }   ///< Set default colors and scrollbar appearance.
    void SetColors (TColor _backColor = BLACK, TColor _scrollbarColor = ToColor (255, 255, 255, 32)) {
      backColor = _backColor; scrollbarColor = _scrollbarColor; Changed ();
    }
    void SetScrollbarWidth (int width = 8) { scrollbarWidth = width; Changed (); }

    void SetArea (SDL_Rect _area) { CWidget::SetArea (_area); SetVirtArea (_area); }
    /// @}

    /// @name Virtual area and scrolling ...
    /// @{
    void SetVirtArea (SDL_Rect r);
      ///< Set the virtual area in screen coordinates; ('virtArea' == 'area' represents no displacement);
      /// The areas of all subwidgets are relative to the virtual area
    SDL_Rect *GetVirtArea () { return &virtArea; }
    void MoveVirtArea (int toX0, int toY0) { SetVirtArea (Rect (toX0, toY0, virtArea.w, virtArea.h)); }
    void LimitVirtArea (SDL_Rect *r = NULL);
      ///< Move virtual area (or 'r') so that the visible area is completely filled.
    void ScrollTo (SDL_Rect r, int hAlign = 0, int vAlign = -1);    ///< Scroll such that 'r' is aligned according to '[vh]Align' (-1 = left/up, 0 = center, 1 = right/down).
    void ScrollIn (SDL_Rect r);      ///< Scroll just enough to get 'r' fully visible.
    /// @}

    /// @name Coordinate transformations (screen <-> sub-widget system) ...
    /// @{
    void WidgetToScreenCoords (int *x, int *y);
    void ScreenToWidgetCoords (int *x, int *y);
    /// @}

    /// @name Sub-Widget management ...
    /// "Add"/"Del" methods do not pass ownership, the caller remains owner.
    /// @{
    void AddWidget (CWidget *widget) { DoAddWidget (&firstWidget, widget); }
    void DelWidget (CWidget *widget) { DoDelWidget (&firstWidget, widget); }
    void DelAllWidgets ();
    /// @}

    /// @name Helpers ...
    /// @{
    bool IsVisible (SDL_Rect *r);
    bool IsVisible (CWidget *w) { return IsVisible (w->GetArea ()); }
    /// @}

  protected:
    SDL_Rect virtArea;
    CWidget *firstWidget;
    TColor backColor, scrollbarColor;
    int scrollbarWidth;

    /// @name Callbacks...
    /// @{
    virtual void Render (SDL_Renderer *ren);
    virtual bool HandleEvent (SDL_Event *ev);
      ///< Handles wiping/scrolling events, if virtual area is wider (higher) than physical area.
    /// @}

  private:

    // Helpers...
    void DoAddWidget (CWidget **pFirst, CWidget *widget);
    void DoDelWidget (CWidget **pFirst, CWidget *widget);
};





// *************************** CScreen *****************************************


/** Screen object.
 */
class CScreen {
  public:
    CScreen () { firstWidget = NULL; changed = true; withKeyboard = false; }
    virtual ~CScreen ();

    /// @name Class methods ...
    /// 'Screen[Init|Done]()' are mapped to the following methods.
    /// @{
    static void ClassInit () { /* activeScreen = NULL; */ }
    static void ClassDone () {}

    static void Refresh () { Changed (); }
      ///< Refresh screen (e.g. after the app has been woken up in Android).

    static void EmulateOff (bool off) { emulateOff = off; Changed (); }
      ///< Emulate screen off
    static void EmulateStandby (bool standby) { emulateStandby = standby; Changed (); }
      ///< Emulate screen standby mode
    /// @}

    /// @name (De-)activation ...
    /// @{
    bool IsActive () { return this == activeScreen; }
    static class CScreen *ActiveScreen () { return activeScreen; }
    virtual void Activate (bool on = true);
    void Deactivate () { Activate (false); }
    /// @}

    /// @name Widget management ...
    /// "Add"/"Del" methods do not pass ownership, caller remains owner.
    /// @{
    void AddWidget (CWidget *widget) { DoAddWidget (&firstWidget, widget); }
    void DelWidget (CWidget *widget) { DoDelWidget (&firstWidget, widget); }
    void DelAllWidgets ();
    /// @}

    /// @name Screen stacking ...
    /// @{
    void Run ();
      ///< Activate the screen, iterate until Return() is called and then return to the previously active screen.
      /// It is allowed to call this method from 'HandleEvent', e.g. for modal dialogs.
      /// If another screen is activated by 'Activate', this method returns immediately and does not
      /// switch back to the previous screen.
    void Return () { running = false; }
      ///< Let Run() return at next occasion.
    /// @}

    /// @name On-screen keyboard ...
    /// @{
    void SetKeyboard (bool on);
    bool HasKeyboard () { return withKeyboard; }
    /// @}

  protected:
    friend class CWidget;
    friend void UiIterate (bool noWait);

    static class CScreen *activeScreen;
    static bool changed;
    static bool emulateOff, emulateStandby;
    static bool keyboardOn;

    CWidget *firstWidget;
    bool running, withKeyboard;

    // Callbacks...
    virtual bool HandleEvent (SDL_Event *ev);

    // Change management & rendering...
    static void Changed () { changed = true; }
    static void RenderUpdate ();

  private:

    // Helpers...
    void DoAddWidget (CWidget **pFirst, CWidget *widget);
    void DoDelWidget (CWidget **pFirst, CWidget *widget);
};





// *************************** Layout helpers **********************************


/// @name Layout helpers ...
/// @{


SDL_Rect *LayoutRow (SDL_Rect container, const int *format, int items = -1, int space = UI_BUTTONS_SPACE);
  ///< Layout a row of rectangles.
  /// @param container is the surrounding rectangle into which the row is fit.
  /// @param format is an array describing the layout:
  ///         - values > 0 describe fixed-width elements of width |value|.
  ///         - values < 0 describe variable-width elements of weight |value|.
  ///         - 0 terminates the list (then, 'items' does not need to be set).
  /// @param items is the number of items if set >= 0 (useful if the format array is not 0-terminated).
  /// @param space is the space between the items.
  /// If 'format' is ommited (== NULL), 'items' is obligatory, and all items are
  /// layed out with variable width and equal size.
  ///
  /// The returned array has a size equal to the 'format' list (without the 0)
  /// and must be free'd by the caller afterwards.
SDL_Rect *LayoutRow (SDL_Rect container, int space = UI_BUTTONS_SPACE, ...);
  ///< Layout a row of rectangles based on a 0-termintated list of format valus passed as variable arguments.
static inline SDL_Rect *LayoutRowEqually (SDL_Rect container, int items, int space = UI_BUTTONS_SPACE) { return LayoutRow (container, (const int *) NULL, items, space); }
  ///< Layout a row of 'items' rectangles with equal width.

SDL_Rect *LayoutCol (SDL_Rect container, const int *format, int items = -1, int space = UI_BUTTONS_SPACE);
  ///< Layout a column of rectangles.
  /// @param container is the surrounding rectangle into which the column is fit.
  /// @param format is an array describing the layout:
  ///         - values > 0 describe fixed-height elements of height |value|.
  ///         - values < 0 describe variable-height elements of weight |value|.
  ///         - 0 terminates the list (then, 'items' does not need to be set).
  /// @param items is the number of items if set >= 0 (useful if the format array is not 0-terminated).
  /// @param space is the space between the items.
  /// If 'format' is ommited (== NULL), 'items' is obligatory, and all items are
  /// layed out with variable width and equal size.
  ///
  /// The returned array has a size equal to the 'format' list (without the 0)
  /// and must be free'd by the caller afterwards.
SDL_Rect *LayoutCol (SDL_Rect container, int space = UI_BUTTONS_SPACE, ...);
  ///< Layout a column of rectangles based on a 0-termintated list of format valus passed as variable arguments.
static inline SDL_Rect *LayoutColEqually (SDL_Rect container, int items, int space = UI_BUTTONS_SPACE) { return LayoutRow (container, (const int *) NULL, items, space); }
  ///< Layout a column of 'items' rectangles with equal width.

SDL_Rect *LayoutMatrix (SDL_Rect container, const int *hFormat, const int *vFormat, int hItems = -1, int vItems = -1, int hSpace = UI_BUTTONS_SPACE, int vSpace = UI_BUTTONS_SPACE);
  ///< Layout a matrix of variable-size elements.
  /// 'hFormat' and 'vFormat' are the formats in horizontal and vertical direction, respectively
  /// The returned array has a size equal to |hFormat| * |vFormat| and is organized in lines.


/// @}





// *************************** Init/Done ***************************************


/// @name Init/Done ...
/// @{


static inline void ScreenInit () { CScreen::ClassInit (); }
static inline void ScreenDone () { CScreen::ClassDone (); }


/// @}





/** @}  // ui_screen
 * @}   // ui
 */


#endif
