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


#ifndef _UI_WIDGETS_
#define _UI_WIDGETS_


/** @file
 *
 * @addtogroup ui
 * @{
 *
 * @defgroup ui_widgets Widgets
 * @brief Collection of UI widgets.
 *
 * This module contains the collection of common UI widgets or widget extensions.
 *
 * Widget extension classes are derived from CWidget and provide some extra functionality,
 * but are typically not used by themselves (e.g. CModalWidget or CCursorWidget).
 * They may be combined with other classes derived from CWidget by means of multiple inheritance.
 * **Important:** The other class must also inherit virtually from CWidget!
 *
 * Callbacks for widget actions can generally be defined in different ways:
 * 1. By overriding a virtual method named 'On<action> ()' (C++ style).
 * 2. By providing a callback function (C style) using a 'SetCb<action> ()' method.
 *    The user may supply a typeless pointer to some user-defined data.
 * 3. By defining a trampoline to an arbitrary method of another class. Trampolines
 *    are defined by macros named `<widget>_TRAMPOLINE()`.
 *
 * @{
 */

#include "ui_screen.H"





// *****************************************************************************
// *                                                                           *
// *                         Useful widget extensions                          *
// *                                                                           *
// *****************************************************************************


/// @defgroup ui_widgets_extensions Extensions
/// @brief Widget Extensions
/// @{





// *************************** CModalWidget ************************************


/** @brief Widget that can pop up on a screen, such as a menu or a message box.
 */
class CModalWidget: public virtual CWidget {
  public:

    /// @name Running the widget ...
    /// @{
    int Run (CScreen *_screen);  ///< @brief Run the widget until it is cancelled or a status >= 0 is set; return status.

    virtual void Start (CScreen *_screen);  ///< @brief Just start the modal widget.
    bool IsRunning ();                      ///< @brief Must be invoked regularly (in each UI iteration), if Run() is not used.
    virtual void Stop ();                   ///< @brief Remove widget from screen (e.g. useful for inactivity timeout).
    /// @}

    /// @name Status ...
    /// @{
    int GetStatus () { return status; }     ///< @brief Get status: -2: running, -1: cancelled (e.g. by touching outside), >= 0: success, user-defined code set by 'SetStatus'.
    void SetStatus (int _status) { status = _status; }    ///< @brief Set a status code.
    /// @}

    /// @name Behaviour for events outside the widget ...
    /// @{
    void SetNoCancelArea (SDL_Rect _rNoCancel) { rNoCancel = _rNoCancel; }
      ///< @brief Set the no-cancel area.
      ///
      /// A touch outside this area is interpreted as a "cancel" operation. Normally and by default, this is equal
      /// to the area of this menu ('rFrame'), but there may be cases where this area needs to be extended,
      /// for example, if a combination of menus are presented simultaneously such as the month/year selector in the calendar app.
    void RemoveNoCancelArea () { SetNoCancelArea (RectScreen ()); }
    /// @}

    /// @name Callbacks...
    /// @{
    virtual bool HandleEvent (SDL_Event *ev);     ///< @brief Handle event: must always be called last.
    /// @}

  protected:
    SDL_Rect rNoCancel;
    int status;
};





// *************************** CCursorWidget ***********************************


/** @brief Widget with an additional rectangular cursor.
 */
class CCursorWidget: public virtual CWidget {
  public:
    CCursorWidget () { ClearCursor (); cbHandleEvent = NULL; }

    void SetCursorFormat (TColor _cursorColor = ToColor (0xff, 0xff, 0xff, 0x20), SDL_BlendMode _blendMode = SDL_BLENDMODE_BLEND) { cursorColor = _cursorColor; blendMode = _blendMode; }

    void SetCursor (SDL_Rect _cursorArea) { cursorArea = _cursorArea; }
    void ClearCursor () { cursorArea.w = cursorArea.h = 0; }

    void SetCbHandleEvent (bool (*_cbHandleEvent) (SDL_Event *, void *), void *_data = NULL) { cbHandleEvent = _cbHandleEvent; cbHandleEventData = _data; }

    virtual void Render (SDL_Renderer *ren);
    virtual bool HandleEvent (SDL_Event *ev);

  protected:
    SDL_Rect cursorArea;
    TColor cursorColor;
    SDL_BlendMode blendMode;
    bool (*cbHandleEvent) (SDL_Event *, void *);
    void *cbHandleEventData;
};



/// @}    // ui_widgets_extensions





// *****************************************************************************
// *                                                                           *
// *                               The widgets                                 *
// *                                                                           *
// *****************************************************************************





// *************************** CButton *****************************************


/// @defgroup ui_widgets_button Button
/// @brief Push-button widgets
/// @{


#define BUTTON_DEFAULT_FONT FontGet (fntNormal, 24) ///< @brief Default font for CButton labels.
#define BUTTON_LABEL_BORDER 8                       ///< @brief Border spacing around labels in pixels.


typedef void FCbButtonPushed (class CButton *button, bool longPush, void *data);
  ///< @brief Function prototype for CButton callbacks.


#define BUTTON_TRAMPOLINE(CB_NAME, CLASS_NAME, METHOD_NAME)             \
  void CB_NAME (class CButton *button, bool longPush, void *data) {     \
    ((CLASS_NAME *) data)->METHOD_NAME (button, longPush);              \
  }
  ///< @brief Macro to create a trampoline from CButton to another class.
  ///
  /// The class must define a method:
  ///
  ///     void CLASS_NAME::METHOD_NAME (CButton *button, bool longPush)
  ///
  /// If the method is protected, the following line may be added to the class:
  ///
  ///     friend CB_NAME (CButton *, bool, void *);
  ///
  /// To the button, the trampoline must be passed by a call like:
  ///
  ///     CButton::SetCbPushed (CB_NAME, <class object pointer>)
  ///


void CbActivateScreen (class CButton *, bool, void *screen);
  ///< @brief Convenience callback for all sort of "Jump-to-screen" buttons (including home).
  /// 'data' must point to a valid @ref CScreen object.


/** @brief Push-button widget.
 *
 * This is the normal push button.
 */
class CButton: public CWidget {
  public:
    CButton () { Init (); }
    virtual ~CButton () { Done (); }
    void Init ();
    void Done ();

    /// @name Quick setup...
    /// @{
    void Set (SDL_Rect _area, TColor _color = GREY);
    void Set (SDL_Rect _area, TColor _color, SDL_Surface *_icon);
    void Set (SDL_Rect _area, TColor _color, const char *text, TColor textColor = WHITE, TTF_Font *font = NULL);
    void Set (SDL_Rect _area, TColor _color, SDL_Surface *_icon, const char *text, TColor textColor = WHITE, TTF_Font *font = NULL);
    /// @}

    /// @name Set parameters...
    /// @{

    void SetArea (SDL_Rect _area);

    void SetColor (TColor _colNorm) { SetColor (_colNorm, _colNorm); }
    void SetColor (TColor _colNorm, TColor _colDown);

    void SetLabel (SDL_Surface *_icon, SDL_Rect *srcRect = NULL, bool takeOwnership = false);
      ///< @brief Set '_icon' as the button label.
      ///
      /// By default, the object keeps a reference to '_icon' internally, and the caller must
      /// keep the icon valid as long as the button object lives or until another label is set.
      /// If 'takeOwnership == true', the button object will free the surface as soon as appropriate
      /// (which may be later or very soon, e.g. during this call!). Hence, if the caller cannot
      /// conveniently keep '_icon' valid, the caller should make a copy (SurfaceDup() ) and
      /// pass 'takeOwnership = true' here.
      ///
      /// **Note:** It is legal to not call any of the SetLabel() methods and have no label at all.
      /// This is, for example, used in the floor plan view, where the "labels" are actually parts of
      /// other (background) widgets.
    void SetLabel (const char *text, TColor textColor = WHITE, TTF_Font *font = NULL);
      ///< @brief Set a text-only label.
    void SetLabel (SDL_Surface *_icon, const char *text, TColor textColor = WHITE, TTF_Font *font = NULL);
      ///< @brief Set icon (and optionally text) label. The icon has its own color(s).
    void SetLabel (TColor color, const char *iconName, const char *text = NULL, TTF_Font *font = NULL)
      { SetLabel (iconName ? IconGet (iconName, color) : NULL, text, color, font); }
      ///< @brief Set icon (and optionally text) label. Both have the same color.
    void ClearLabel () { SetLabel ((SDL_Surface *) NULL); }
      ///< @brief Remove label.
    void SetLabelAlignment (int _hAlign = 0, int _vAlign = 0) { hAlign = _hAlign; vAlign = _vAlign; ChangedSurface (); }
      ///< @brief Set/change label alignment.

    void SetHotkey (SDL_Keycode _hotkey) { hotkey = _hotkey; }
      ///< @brief Set hotkey which activates the button callback.

    /// @}

    /// @name Set actions...
    /// @{
    virtual void OnPushed (bool longPush);
    void SetCbPushed (FCbButtonPushed *cb, void *_data = NULL) { cbPushed = cb; cbPushedData = _data; }
    void *GetCbPushedData () { return cbPushedData; }
    /// @}

    /// @name Callbacks...
    /// @{
    virtual SDL_Surface *GetSurface ();
    virtual bool HandleEvent (SDL_Event *ev);
    /// @}

  protected:
    SDL_Surface *surfLabel;
    bool surfLabelIsOwned;
    TColor colNorm, colDown;
    int hAlign, vAlign;
    bool isDown, changed;

    FCbButtonPushed *cbPushed;
    void *cbPushedData;

    SDL_Keycode hotkey;

    /// @name Change management...
    /// @{
    void ChangedSurface () { changed = true; CWidget::ChangedSurface (); }
    /// @}
};



#define FLATBUTTON_COL_UP   BLACK
#define FLATBUTTON_COL_DOWN ToColor (0x60, 0x00, 0x00)
//~ #define FLATBUTTON_COL_DOWN LIGHT_BROWN // DARK_YELLOW


/** @brief Flat Push-button widget.
 *
 * This is a piece of text without a visible button, which behaves just like
 * a push-button. Examples are the launch buttons on the home screen.
 */
class CFlatButton: public CButton {
  public:
    CFlatButton () { SetColor (FLATBUTTON_COL_UP, FLATBUTTON_COL_DOWN); }

    virtual SDL_Surface *GetSurface ();
};





// ***** Creating main button bars *****


/// Descriptor for a button on the main button bar.
typedef struct {
  int layoutWidth;              ///< Width specification (absolute or relative), any value accepted by 'LayoutRow()' (except 0)
  TColor color;                 ///< Button color
  const char *iconName;         ///< Icon; if 'NULL', no icon is set; otherwise, 'IconGet(iconName)' is set as icon
  const char *text;             ///< Text; if 'NULL', no text is set
  FCbButtonPushed *cbPushed;    ///< Pushed callback; may be 'NULL' to not set a callback.
  SDL_Keycode hotkey;           ///< Hotkey; set to 'SDLK_UNKNOWN' to not enable a hotkey
} TButtonDescriptor;


CButton *CreateMainButtonBar (int buttons, TButtonDescriptor *descTable, CScreen *screen);
  ///< @brief Create a dynamic array of CButton objects for a main button bar.
  /// 'screen' is passed as the user data field to each button.
  /// If 'screen != NULL', all buttons are added to the screen as widgets.


/// @}  // ui_widgets_button





// *************************** CListbox ****************************************


/// @defgroup ui_widgets_listbox Listbox
/// @brief Listbox widget
/// @{


#define LISTBOX_ITEM_BORDER 4     ///< @brief Border spacing around labels in pixels.


/// List box mode
enum EListboxMode {
  lmReadOnly,       ///< Listbox is read-only, no selection & interaction.
  lmActivate,       ///< Items act like push-buttons, activation callback can be defined.
  lmSelectSingle,   ///< Items are exclusivly selectable (activation callback can be defined, too).
  lmSelectAny       ///< Items are arbitrarily selectable (activation callback can be defined, too).
};


typedef void FCbListboxPushed (class CListbox *, int idx, bool longPush, void *data);
  ///< @brief Callback prototype for CListbox push events.


#define LISTBOX_TRAMPOLINE(CB_NAME, CLASS_NAME, METHOD_NAME)                      \
  void CB_NAME (class CListbox *listbox, int idx, bool longPush, void *data) {    \
    ((CLASS_NAME *) data)->METHOD_NAME (listbox, idx, longPush);                  \
  }
  ///< @brief Macro to create a trampoline to another class.
  ///
  /// The class must define a method:
  ///
  ///     void CLASS_NAME::METHOD_NAME (CListbox *listbox, int idx, bool longPush)
  ///
  /// If the method is protected, the following line may be added to the class:
  ///
  ///     friend CB_NAME (CButton *, int, bool, void *);
  ///
  /// To the list box, the trampoline must be passed by a call like:
  ///
  ///     CListbox::SetCbPushed (CB_NAME, <class object pointer>)
  ///


/** @brief Listbox item.
 */
class CListboxItem {
  public:
    CListboxItem () { text = NULL; iconName = NULL; iconSurf = NULL; isSelected = isSpecial = false; data = NULL; changed = true; }

    bool IsSelected () { return isSelected; }

    void SetLabel (const char *_text, const char *_iconName = NULL) { text = _text; iconName = _iconName; iconSurf = NULL; }
      ///< @brief Set label with a text and icon, both are optional.
      /// The icon is recolored as the text.
    void SetLabel (const char *_text, SDL_Surface *_iconSurf) { text = _text; iconName = NULL; iconSurf = _iconSurf; }
      ///< @brief Set label with a text and icon, both are optional.
      /// The icon has its own color(s).

    bool isSpecial;   ///< Item is special, e.g. a head line.
    void *data;       ///< Reference to optional user data.

  protected:
    friend class CListbox;

    const char *text;
    const char *iconName;
    SDL_Surface *iconSurf;

    bool isSelected;
    bool changed;       ///< If set, item will be re-rendered at next occasion
};


/** @brief Listbox widget.
 */
class CListbox: public CCanvas {
  public:
    CListbox ();
    ~CListbox ();

    /// @name Setup ...
    /// @{
    void SetMode (EListboxMode _mode, int _itemHeight, int _itemGap = 1);
      ///< @brief 'itemHeight' == 0 indicates variable-height items (Note: presently much worse performance than fixed-height);
      /// implicitly deletes all items.
    void SetFormat (TTF_Font *_font, int _hAlign = -1, TColor colGrid = BLACK,
                    TColor _colLabel = WHITE, TColor _colBack = DARK_DARK_GREY,
                    TColor _colLabelSelected = WHITE, TColor _colBackSelected = GREY,
                    TColor _colLabelSpecial = WHITE, TColor _colBackSpecial = DARK_GREY);
      ///< @brief Except 'colGrid', all parameters set here are only read in 'RenderItem' & can be ommited (or re-interpreted) if a custom render function is defined.
      /// 'colGrid' is the general background color which can also be set via 'CCanvas::SetColors'.
    void SetArea (SDL_Rect _area) { CCanvas::SetArea (_area); ChangedSetup (); }
    /// @}

    /// @name Content management ...
    /// @{
    void Clear () { SetItems (0); }
    void SetItems (int _items);
    int GetItems () { return items; }

    void SetItem (int idx, const char *_text, const char *_iconName = NULL, bool _isSpecial = false, void *data = NULL);
    void SetItem (int idx, const char *_text, SDL_Surface *_iconSurf, bool _isSpecial = false, void *data = NULL);
    CListboxItem *GetItem (int idx) { return &itemArr[idx]; }

    int GetItemHeight () { return itemHeight; }
    int GetItemLabelWidth (int idx);
      ///< @brief Retrieve the label (net) width of an item (e.g. to optimize geometry).
    SDL_Rect GetItemRect (int idx);
      ///< @brief Get the area of an item.

    void ScrollTo (int idx, int vAlign = -1) { CCanvas::ScrollTo (GetItemRect (idx), 0, vAlign); Changed (); }
    void ScrollIn (int idx) { CCanvas::ScrollIn (GetItemRect (idx)); Changed (); }
    /// @}

    /// @name Content selection ...
    /// @{
    void SelectItem (int idx, bool _isSelected = true);   ///< @brief (De-)Select item (for 'lmSelectSingle', 'lmSelectAny')
    void SelectAll (bool _isSelected = true);             ///< @brief Select all items (for 'lmSelectAny')
    void SelectNone () { SelectAll (false); }             ///< @brief Deselect all items (for 'lmSelectSingle', 'lmSelectAny')

    bool MouseDown () { return downIdx >= 0; }
      ///< @brief Determine whether the listbox is currently dragged by the mouse.
      /// In this case, calling a Select...() method may interfere with the user interaction and should be avoided.
    /// @}

    /// @name Returning results ...
    /// @{
    bool ItemIsSelected (int idx) { return itemArr[idx].isSelected; }
    int GetSelectedItem () { return selectedItem; }       // 'lmSelectSingle' only

    virtual void OnPushed (int idx, bool longPush) { if (cbPushed) cbPushed (this, idx, longPush, cbPushedData); }
    void SetCbPushed (FCbListboxPushed *_cbPushed, void *_data = NULL) { cbPushed = _cbPushed; cbPushedData = _data; }
    /// @}

    /// @name Change management ...
    /// @{
    void ChangedItems (int idx, int num = 1);    ///< Must be called if items changed and SetItem() was not called.
    /// @}

    /// @name Callbacks...
    /// @{
    virtual void Render (SDL_Renderer *ren);
    virtual bool HandleEvent (SDL_Event *ev);
    /// @}

  protected:
    CListboxItem *itemArr;
    int items;
    CWidget **pool;
    int poolSize, *poolIdx;
    int selectedItem, downIdx, downSelectedItem;
    bool noLongPush;
    bool changed;

    // Configuration options...
    EListboxMode mode;
    int itemGap, itemHeight;
    FCbListboxPushed *cbPushed;
    void *cbPushedData;

    // Infos for 'RenderItem' only...
    TTF_Font *font;
    int hAlign;
    TColor colLabel, colBack, colLabelSelected, colBackSelected, colLabelSpecial, colBackSpecial;

    /// @name Rendering ...
    /// @{
    virtual SDL_Surface *RenderItem (CListboxItem *item, int idx, SDL_Surface *surf);
      ///< @brief Render a list box item to an SDL surface and return it.
      ///
      /// This method may be overriden to implement a custom look of the items.
      /// The default implementation renders an icon and text (or one of both) as defined by CListboxItem::SetLabe()
      /// according the settings given by SetFormat() and interprets CListboxItem::isSpecial().
      /// The dimensions of the returned surface must match 'area.w' and 'itemHeight' (if >= 0).
      /// If 'itemHeight' == 0, the height is arbitrary and the layout will be adapted accordingly.
      /// The caller becomes owner of the returned surface.
      /// 'surf' is the previous surface which can be reused. If it is not NULL, it must be free'd or reused.
    /// @}

    /// @name Change management ...
    /// @{
    void ChangedSetup () { InvalidatePool (); Changed (); }
    void Changed () { changed = true; CCanvas::Changed (); }
    /// @}

  private:

    // Helpers...
    void InvalidatePool ();   // called if something general changed (e.g. colors, area)
    void UpdatePool ();       // update pool before rendering
};


/// @}  // ui_widgets_listbox





// *************************** CMenu *******************************************


/// @defgroup ui_widgets_menu Menu
/// @brief Menu widget and function
/// @{


#define MENU_DEFAULT_FONT FontGet (fntNormal, 20) ///< Default font for menus.
#define MENU_DEFAULT_COLOR DARK_GREY              ///< Default color for menus.


/** @brief Menu widget.
 */
class CMenu: public CListbox, public CModalWidget {
  public:
    CMenu () { texFrame = NULL; }
    ~CMenu () { TextureFree (&texFrame); }

    /// @name Setup ...
    /// @{
    void Setup (SDL_Rect _rContainer, int _hAlign = -1, int _vAlign = -1,  // The exact geometry is determined in 'Start'
                TColor color = MENU_DEFAULT_COLOR, TTF_Font *_font = NULL);
    /// @}

    /// @name Setting menu entries ...
    /// @{
    void SetItems (const char *_itemStr);    ///< Set a list of items separated by '|'.
    void SetItems (int _items) { CListbox::SetItems (_items); }
    void SetItem (int idx, const char *_text, SDL_Surface *_surfIcon = NULL, bool _isSpecial = false, void *data = NULL) { CListbox::SetItem (idx, _text, _surfIcon, _isSpecial, data); }
    /// @}

    /// @name Running (see CModalWidget) ...
    /// @{
    int Run (CScreen *_screen, const char *_itemStr) { SetItems (_itemStr); return Run (_screen); }
    int Run (CScreen *_screen) { return CModalWidget::Run (_screen); }
    virtual void Start (CScreen *_screen);
    /// @}

    /// @name Status...
    /// The selected Menu entry is obtained by CModalWidget::GetStatus().
    /// If the status is < 0, the menu has been canceled.
    /// The following method(s) provide additional information.
    /// @{
    bool GetStatusLongPush () { return hadLongPush; }
      ///< @brief Return whether the push was a long push.
    /// @}

    /// @name Misc. ...
    /// @{
    SDL_Rect *GetArea () { return &rFrame; }   // Note: Is different from 'CWidget::area', which here only contains the canvas part of the menu
    /// @}

    /// @name Callbacks...
    /// @{
    virtual void Render (SDL_Renderer *ren);
    virtual bool HandleEvent (SDL_Event *ev);
    virtual void OnPushed (int idx, bool longPush);
    /// @}

  protected:
    SDL_Rect rContainer, rFrame;
    int hAlign, vAlign;
    CString itemStr;
    SDL_Texture *texFrame;
    bool hadLongPush;
};


// Easy-to-use function(s)...
int RunMenu (const char *_itemStr, SDL_Rect _rContainer, int _hAlign = -1, int _vAlign = -1,
             TColor color = MENU_DEFAULT_COLOR, TTF_Font *_font = NULL,
             bool *retLongPush = NULL);
  ///< @brief Run a menu with a single function call.
  /// @param _itemStr is a string containing the menu items separated by '|'.
  /// @param _rContainer specifies the rectangle in which the menu is placed.
  /// @param _hAlign specifies the horizontal alignment inside the container rectangle.
  /// @param _vAlign specifies the vertical alignment inside the container rectangle.
  /// @param color is the main menu color.
  /// @param _font is the menu font (if NULL, the default font is used).
  /// @param retLongPush optionally points to a variable which is set if the menu was completed with a long push.


/// @}  // ui_widgets_menu





// *************************** Message Box *************************************


/// @defgroup ui_widgets_messagebox Message Box
/// @brief Message box widget and functions
/// @{


#define MSGBOX_DEFAULT_FONT FontGet (fntNormal, 24)   ///< Default font for message box contents.
#define MSGBOX_TITLE_FONT FontGet (fntBold, 32)       ///< Title font for message boxes.

#define MSGBOX_COLOR DARK_GREY                        ///< Default color for message boxes.

#define MSGBOX_SPACE_X 32                             ///< Horizontal space around message box contents.
#define MSGBOX_SPACE_Y 32                             ///< Vertical space around message box contents and between the title and contents.
#define MSGBOX_BUTTON_MINWIDTH 160                        ///< Minimum width of message box buttons.



/// IDs for standard buttons.
enum EMessageButtonId {
  mbiCancel = 0,
  mbiOk,
  mbiNo,
  mbiYes,
  mbiEND
};


#define mbmCancel ((int) (1 << mbiCancel))    ///< Bit mask to select a "Cancel" button
#define mbmOk ((int) (1 << mbiOk))            ///< Bit mask to select an "OK" button
#define mbmNo ((int) (1 << mbiNo))            ///< Bit mask to select an "No" button
#define mbmYes ((int) (1 << mbiYes))          ///< Bit mask to select an "Yes" button

#define mbmOkCancel (mbmOk | mbmCancel)       ///< Bit mask for the "OK / Cancel" combination
#define mbmYesNoCancel (mbmYes | mbmNo | mbmCancel)   ///< Bit mask for the "Yes / No / Cancel" combination
#define mbmNone ((int) 0)                     ///< Bit mask for no button


/** @brief Message box widget.
 *
 * See @ref CModalWidget for options to run the message box.
 */
class CMessageBox: public CModalWidget {
  public:
    CMessageBox () { buttons = 0; buttonArr = NULL; screenHasKeyboard = false; }
    ~CMessageBox () { Stop (); SurfaceFree (&surface); }

    /// @name Setup ...
    /// @{
    void Setup (const char *title, int contentW, int contentH, int _buttons, CButton **_buttonArr, TColor color = MSGBOX_COLOR, int titleHAlign = 0);
      ///< @brief The most flexible way.
      ///
      /// 'content' is arbitrary content and must be transparent in empty areas.
      /// The buttons must be predefined, only the areas and the push action of all buttons
      /// will be (re-)set by 'Setup'.
      /// All parameters must remain valid until the message box is stopped.
      // TBD: FIX this: replace int "int contentW, int contentH" by "SDL_Surface *content"!!!
    CButton *GetStdButton (EMessageButtonId buttonId);
      ///< @brief Initialize and return a standard button to be used by the previous method.

    void Setup (const char *title, int contentW, int contentH, int buttonMask, TColor color = MSGBOX_COLOR);
      ///< @brief The easier way, if only standard buttons are used.
      /// 'buttonMask' is a bit-wise "or" of the message mask constants ('mbm...').
      // TBD: FIX this: replace int "int contentW, int contentH" by "SDL_Surface *content"!!!

    void Setup (const char *title, const char *text, SDL_Surface *icon = NULL, int buttonMask = mbmOk, int hAlign = 0, TTF_Font *font = NULL);
      ///< @brief The easiest way for standard text boxes.
    /// @}

    /// @name Properties ...
    /// @{
    SDL_Rect *ContentArea () { return &rContent; }
    /// @}

    /// @name Obtaining the result ...
    /// @{
    int GetChoice () { return GetStatus (); }
      ///< @brief Return the number of the button pushed, numbered from right to left;
      /// the rightmost button (typically "Cancel") has number 0.
    /// @}

    /// @name Callbacks ...
    /// @{
    virtual void Start (CScreen *_screen);
    virtual void Stop ();
    virtual void OnButtonPushed (CButton *button, bool);
    /// @}

  protected:
    CButton stdButtons[mbiEND], **buttonArr, *stdButtonArr[mbiEND];
    SDL_Rect rContent;
    int buttons;
    bool screenHasKeyboard;
};


/// @name Running a message box with a single function call ...
/// @{
int RunMessageBox (const char *title, const char *text, int buttonMask = mbmOk, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
  ///< @brief Run an arbitray message box.

int RunInfoBox (const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
int RunInfoBox (const char *title, const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
  ///< @brief Run an info box with an "OK" button.
int RunWarnBox (const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
int RunWarnBox (const char *title, const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
  ///< @brief Run an warning box with an "OK" button.
int RunErrorBox (const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
int RunErrorBox (const char *title, const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
  ///< @brief Run an error box with an "OK" button.

int RunSureBox (const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
int RunSureBox (const char *title, const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
  ///< @brief Run an "Are you sure?" box. Returns 1 if sure and <= 0 otherwise.
int RunQueryBox (const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
int RunQueryBox (const char *title, const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
  ///< @brief Run a "Yes/No/Cancel" query box. Returns 2 on "yes", 1 on "no" and 0 otherwise.
/// @}


/// @name Manually operated message boxes ...
/// (e.g. for "please wait..." popups)
/// @{
CMessageBox *StartMessageBox (const char *title, const char *text, SDL_Surface *icon = NULL, int buttonMask = mbmOk, int hAlign = 0, TTF_Font *font = NULL);
static inline void IterateMessageBox () { UiIterate (); }
static inline int GetMessageBoxStatus (CMessageBox *msgBox) { return msgBox->GetStatus (); }
void StopMessageBox (CMessageBox *msgBox);
/// @}


/// @}  // ui_widgets_messagebox





// *************************** CInputLine **************************************


/// @defgroup ui_widgets_inputline Input Line
/// @brief Input line widget
/// @{


#define INPUTLINE_FONTSIZE 24
#define INPUTLINE_UNDOS 32


/** @brief Input line widget.
 */
class CInputLine: public CCanvas {
  public:
    CInputLine () { font = NULL; inputLen = 0; mark0 = markD = 0; suggPos = -1; cbSuggCheck = NULL; suggData = NULL; undoFirst = redoFirst = NULL; surfMain = NULL; changedContent = changedMark = false; }
    ~CInputLine () { SurfaceFree (&surfMain); ClearHistory (); }

    /// @name Setup ...
    /// @{
    void Setup (int fontSize = INPUTLINE_FONTSIZE);
    void SetInput (const char *_inputStr = NULL, int _mark0 = 0, int _markD = 0);
      ///< @brief Set the input contents; implicitly calls 'ClearHistory' afterwards.
    void ClearHistory ();   ///< @brief Clear the undo & redo history.
    /// @}

    /// @name Obtaining the result ...
    /// @{
    void GetInput (CString *ret) { ret->SetFromIso8859 (input.Get ()); }
                                ///< @brief Retrieve the current string.
    bool InputModified ();      ///< @brief Return if anything may have changed.
    /// @}

    /// @name Editing ...
    /// @{
    int GetMark0 () { return mark0; }    ///< @brief Return the cursor position.
    int GetMarkD () { return markD; }
    void SetMark (int _mark0, int _markD = 0);
    void MoveMark (int _mark0);         ///< @brief Set 'mark0' while leaving 'mark0 + markD' constant.

    void InsChar (char c);
    void InsText (const char *txt, int chars = -1);
    void DelMarked ();
    void DelChar (int pos);

    CString *GetInput () { return &input; }
      ///< @brief Return string (ISO 8859!) that may be modified arbitrarily, followed by a call to ChangedInput().
    void ChangedInput ();   ///< @brief To be called if the imput string was changed.
    /// @}

    /// @name Clipboard ...
    /// @{
    void ClipboardCopy ();
    void ClipboardCut () { ClipboardCopy (); DelMarked (); }
    void ClipboardPaste ();
    /// @}

    /// @name Undo/redo ...
    /// @{
    void Undo ();
    void Redo ();
    /// @}

    /// @name Suggestions (TBD: yet to be implemented) ...
    /// @{
    void SetSuggestion (int _suggPos, const char *_suggText, int _suggMark0 = -1, int _suggMarkD = 0);    // '_suggText' must be in ISO 8859
    void ClearSuggestion () { SetSuggestion (-1, NULL); }
    void ApplySuggestion ();
    virtual void CheckSuggestion () { if (cbSuggCheck) cbSuggCheck (this, suggData); }    // called when
    void SetCbCheckSuggestion (void (*_cbSuggCheck) (CInputLine *, void *), void *_suggData) { cbSuggCheck = _cbSuggCheck; suggData = _suggData; }
    /// @}

    /// @name Callbacks...
    /// @{
    virtual void Render (SDL_Renderer *ren);
    virtual bool HandleEvent (SDL_Event *ev);
    /// @}

  protected:
    TTF_Font *font;
    int charWidth;
    CString input;    // ISO 8859
    int inputLen;
    int mark0, markD;     // 'markD' may also be negative; 'mark0' is always the cursor position
    class CUndoState *undoFirst, *redoFirst;

    int suggPos, suggMark0, suggMarkD;        // 'suggPos < 0' => no suggestion
    CString suggText; // ISO 8859
    void (*cbSuggCheck) (CInputLine *, void *);
    void *suggData;

    CCursorWidget wdgMain;
    SDL_Surface *surfMain;

    bool changedContent, changedMark;

    // Undoing/Redoing...
    void ClearStateList (class CUndoState **pList);
    void PushInput ();
    void PushMark ();
    void SetState (class CUndoState *s);

    // Change management...
    // ChangedInput (): see above
    void ChangedContent () { changedContent = true; Changed (); }   // input or suggestion
    void ChangedMark ();

    // Helpers...
    int GetCharOfMouseEvent (SDL_Event *ev);
};


/** @brief Screen with an input line widget.
 *
 * Since the on-screen keyboard requires some space, it is useful to use @ref CInputLine
 * indirectly with this screen. It shows an input line on top and a button bar below.
 * The button bar shows a back/escape button on the left, an "OK" and standard editing buttons
 * on the right and may contain optional user buttons in the center.
 */
class CInputScreen: public CScreen {
  public:
    CInputScreen () {}
    ~CInputScreen ();

    /// @name Setup ...
    /// @{
    void Setup (const char *label, const char *inputPreset, TColor color = GREY, int userBtns = 0, CButton **userBtnList = NULL, const int *userBtnWidth = NULL);
      ///< Set up the screen.
      /// @param label is an optional label printed before the input line (may be NULL for no label).
      /// @param inputPreset is the preset for the input string.
      /// @param color is the button color, individual colors may be set afterwards.
      /// @param userBtns is the number of user buttons.
      /// @param userBtnList is a list of pointers to the user button (may be NULL if userBtns == 0).
      /// @param userBtnWidth is a list of width arguments for the user button (see @ref LayoutRow(); may be NULL if userBtns == 0).
      /// For the user buttons, the area and color are set according to the parameters.
      /// The label must be set by the caller.
    /// @}

    /// @name Accessing the input line ...
    /// @{
    CInputLine *InputLine () { return &wdgInput; }
    void SetInput (const char *_inputStr=NULL, int _mark0=0, int _markD=0) { wdgInput.SetInput (_inputStr, _mark0, _markD); }
      ///< see @ref CInputLine::SetInput ().
    void GetInput (CString *ret) { wdgInput.GetInput (ret); }
      ///< see @ref CInputLine::GetInput ().
    bool InputModified () { return wdgInput.InputModified (); }
      ///< see @ref CInputLine::InputModified ().
    /// @}

  protected:
    CWidget wdgLabel;
    CInputLine wdgInput;
    CButton btnBack, btnUndo, btnRedo, btnCut, btnCopy, btnPaste, btnOk;

    /// @name Callbacks for derived classes ...
    /// @{
    virtual void Commit () { Return (); }
      ///< Called on push of the "OK" button. May call @ref Return() on success or not to continue editing.
    virtual void Cancel () { Return (); }
      ///< Called on push of the "back" button and after an eventual "are you sure?" confirmation.
      /// Must call @ref Return().
    virtual void OnUserButtonPushed (CButton *btn, bool longPush) {}
      ///< Called on push of any user button.
    /// @}

  private:
    friend void CbInputScreenOnButtonPushed (CButton *, bool, void *);

    void OnButtonPushed (CButton *btn, bool longPush);
};


/// @}  // ui_widgets_inputline





// *************************** CSlider *****************************************


/// @defgroup ui_widgets_slider Slider
/// @brief Horizontal or vertical slider
/// @{


#define SLIDER_WIDTH 48           ///< Default width of a slider.
#define SLIDER_BAR_HEIGHT 16      ///< Default height of a slider bar.


#define SLIDER_TRAMPOLINE(CB_NAME, CLASS_NAME, METHOD_NAME)                   \
  void CB_NAME (class CSlider *slider, int val, int lastVal, void *data) {    \
    ((CLASS_NAME *) data)->METHOD_NAME (slider, val, lastVal);                \
  }
  ///< @brief Macro to define a trampoline to another class.
  ///
  /// The class must define a method:
  ///
  ///     void CLASS_NAME::METHOD_NAME (CSlider *slider, int val, int lastVal)
  ///
  /// If the method is protected, the following line may be added to the class:
  ///
  ///     friend CB_NAME (CButton *, int, int, void *);
  ///
  /// To the slider, the trampoline must be passed by a call like:
  ///
  ///     CSlider::SetCbValueChanged (CB_NAME, <class object pointer>)
  ///


/** @brief Slider widget.
 */
class CSlider: public CWidget {
  public:
    CSlider ();
    virtual ~CSlider ();

    /// @name Setup...
    /// @{
    void SetFormat (TColor _colSlider, TColor _colBarLower, TColor _colBarUpper, TColor _colBack = TRANSPARENT, int _sliderW = SLIDER_WIDTH, int _barH = SLIDER_BAR_HEIGHT);
    void SetArea (SDL_Rect _area);
    void SetInterval (int _val0, int _val1, bool _continuousUpdate = true);
      ///< @brief Set the logical range of values; Possible values for 'val' are then '_val0 <= val <= _val1'.
      ///
      /// If '_continuousUpdate' is false, OnValueChanged() is only called when the slider is released.
      /// No action callback is invoked, even if the value has been clipped.
    /// @}

    /// @name Read and set value...
    /// @{
    int GetValue () { return val; }
    void SetValue (int _val, bool callOnValueChanged = false);
      ///< @brief Set the current value; The passed value will be clipped to the allowed range.
    /// @}

    /// @name Action callbacks...
    /// @{
    virtual void OnValueChanged (int val, int lastVal);
    void SetCbValueChanged (void (*_cbValueChanged) (class CSlider *, int, int, void *), void *_data = NULL) { cbValueChanged = _cbValueChanged; cbValueChangedData = _data; }
    /// @}

    /// @name Callbacks...
    /// @{
    virtual SDL_Surface *GetSurface ();
    virtual bool HandleEvent (SDL_Event *ev);
    /// @}

  protected:
    int sliderW, barH;
    TColor colSlider, colBarLower, colBarUpper, colBack;
    bool continuousUpdate, isDown, redraw;

    int val0, val1, val;
    int slider0, downX;    // X position of slider and of last "touch down" event

    void (*cbValueChanged) (class CSlider *, int, int, void *);
    void *cbValueChangedData;

    /// @name Change management...
    /// @{
    void ChangedSurface () { redraw = true; CWidget::ChangedSurface (); }
    void Changed () { redraw = true; CWidget::Changed (); }
    /// @}

    // Helpers...
    void SetSlider0 (int _slider0, bool updateVal);
};


/// @}  // ui_widgets_slider




/** @}  // ui_widgets
 * @}   // ui
 */


#endif
