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


#ifndef _UI_WIDGETS_
#define _UI_WIDGETS_


/** @file
 * @addtogroup ui
 * @{
 *
 * @defgroup ui_widgets Widgets
 * Collection of UI widgets.
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
/// Widget Extensions
/// @{





// *************************** CModalWidget ************************************


/** Widget that can pop up on a screen, such as a menu or a message box.
 */
class CModalWidget: public virtual CWidget {
  public:

    /// @name Running the widget ...
    /// @{
    int Run (CScreen *_screen);  ///< Run the widget until it is cancelled or a status >= 0 is set; return status.

    virtual void Start (CScreen *_screen);  ///< Just start the modal widget.
    bool IsRunning ();                      ///< Must be invoked regularly (in each UI iteration), if Run() is not used.
    virtual void Stop ();                   ///< Remove widget from screen (e.g. useful for inactivity timeout).
    /// @}

    /// @name Status ...
    /// @{
    int GetStatus () { return status; }     ///< Get status: -2: running, -1: cancelled (e.g. by touching outside), >= 0: success, user-defined code set by 'SetStatus'.
    void SetStatus (int _status) { status = _status; }    ///< Set a status code.
    /// @}

    /// @name Behaviour for events outside the widget ...
    /// @{
    void SetNoCancelArea (SDL_Rect _rNoCancel) { rNoCancel = _rNoCancel; }
      /// Set the no-cancel area.
      /// A touch outside this area is interpreted as a "cancel" operation. Normally and by default, this is equal
      /// to the area of this menu ('rFrame'), but there may be cases where this area needs to be extended,
      /// for example, if a combination of menus are presented simultaneously such as the month/year selector in the calendar app.
    void RemoveNoCancelArea () { SetNoCancelArea (RectScreen ()); }
    /// @}

  protected:
    SDL_Rect rNoCancel;
    int status;

    virtual bool HandleEvent (SDL_Event *ev);     // must be called last
};





// *************************** CCursorWidget ***********************************


/** Widget with an additional rectangular cursor.
 */
class CCursorWidget: public virtual CWidget {
  public:
    CCursorWidget () { ClearCursor (); cbHandleEvent = NULL; }

    void SetCursorFormat (TColor _cursorColor = ToColor (0xff, 0xff, 0xff, 0x20), SDL_BlendMode _blendMode = SDL_BLENDMODE_BLEND) { cursorColor = _cursorColor; blendMode = _blendMode; }

    void SetCursor (SDL_Rect _cursorArea) { cursorArea = _cursorArea; }
    void ClearCursor () { cursorArea.w = cursorArea.h = 0; }

    void SetCbHandleEvent (bool (*_cbHandleEvent) (SDL_Event *, void *), void *_data = NULL) { cbHandleEvent = _cbHandleEvent; cbHandleEventData = _data; }

  protected:
    SDL_Rect cursorArea;
    TColor cursorColor;
    SDL_BlendMode blendMode;
    bool (*cbHandleEvent) (SDL_Event *, void *);
    void *cbHandleEventData;

    virtual void Render (SDL_Renderer *ren);
    virtual bool HandleEvent (SDL_Event *ev);
};



/// @}    // ui_widgets_extensions





// *****************************************************************************
// *                                                                           *
// *                               The widgets                                 *
// *                                                                           *
// *****************************************************************************





// *************************** CButton *****************************************


/// @defgroup ui_widgets_button Button
/// Push-button widget
/// @{


#define BUTTON_DEFAULT_FONT FontGet (fntNormal, 24)   ///< Default font for CButton labels.


typedef void FCbButtonPushed (class CButton *button, bool longPush, void *data);
  ///< Function prototype for CButton callbacks.


/** Macro to create a trampoline from CButton to another class.
 * The class must define a method:
 *
 *     void CLASS_NAME::METHOD_NAME (CButton *button, bool longPush)
 *
 * To the button, the trampoline must be passed by a call like:
 *
 *     CButton::SetCbPushed (CB_NAME, <class object pointer>)
 */
#define BUTTON_TRAMPOLINE(CB_NAME, CLASS_NAME, METHOD_NAME)                 \
  static void CB_NAME (class CButton *button, bool longPush, void *data) {  \
    ((CLASS_NAME *) data)->METHOD_NAME (button, longPush);                  \
  }


void CbActivateScreen (class CButton *, bool, void *screen);
  ///< Convenience callback for all sort of "Jump-to-screen" buttons (including home)
  /// 'data' must point to a valid 'CScreen' object.


/** Push-button widget.
 */
class CButton: public CWidget {
  public:
    CButton () { Init (); }
    virtual ~CButton () { Done (); }
    void Init ();
    void Done ();

    /// @name Quick setup...
    /// @{
    void Set (SDL_Rect _area, TColor _color);
    void Set (SDL_Rect _area, TColor _color, SDL_Surface *_icon);
    void Set (SDL_Rect _area, TColor _color, const char *text, TColor textColor = WHITE, TTF_Font *font = NULL);
    void Set (SDL_Rect _area, TColor _color, SDL_Surface *_icon, const char *text, TColor textColor = WHITE, TTF_Font *font = NULL);
    //~ void Set (SDL_Rect _area, TColor _color, SDL_Surface *_icon, TColor textColor, const char *text) { Set (_area, _color, _icon, NULL, textColor, text); }
    /// @}

    /// @name Set parameters...
    /// @{
    void SetArea (SDL_Rect _area);

    void SetColor (TColor _colNorm) { SetColor (_colNorm, _colNorm); }
    void SetColor (TColor _colNorm, TColor _colDown);

    void SetLabel (SDL_Surface *_icon, SDL_Rect *srcRect = NULL);
    void SetLabel (const char *text, TColor textColor = WHITE, TTF_Font *font = NULL);
    void SetLabel (SDL_Surface *_icon, const char *text, TColor textColor = WHITE, TTF_Font *font = NULL);
    void SetLabel (TColor color, const char *iconName, const char *text = NULL, TTF_Font *font = NULL)
      { SetLabel (iconName ? IconGet (iconName, color) : NULL, text, color, font); }
    void ClearLabel () { SetLabel ((SDL_Surface *) NULL); }

    void SetHotkey (SDL_Keycode _hotkey) { hotkey = _hotkey; }
    /// @}

    /// @name Set actions...
    /// @{
    virtual void OnPushed (bool longPush);
    void SetCbPushed (FCbButtonPushed *cb, void *_data = NULL) { cbPushed = cb; cbPushedData = _data; }
    /// @}

  protected:
    SDL_Surface *surfLabel;
    TColor colNorm, colDown;
    bool isDown, changed;

    FCbButtonPushed *cbPushed;
    void *cbPushedData;

    SDL_Keycode hotkey;

    /// @name Callbacks...
    /// @{
    virtual SDL_Surface *GetSurface ();
    virtual bool HandleEvent (SDL_Event *ev);
    /// @}

    /// @name Change management...
    /// @{
    void ChangedSurface () { changed = true; CWidget::ChangedSurface (); }
    /// @}
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
  ///< Create a dynamic array of CButton objects for a main button bar.
  /// If 'screen != NULL', all buttons are added to the screen as widgets.


/// @}  // ui_widgets_button





// *************************** CListbox ****************************************


/// @defgroup ui_widgets_listbox Listbox
/// Listbox widget
/// @{


/// List box mode
enum EListboxMode {
  lmReadOnly,       ///< Listbox is read-only, no selection & interaction.
  lmActivate,       ///< Items act like push-buttons, activation callback can be defined.
  lmSelectSingle,   ///< Items are exclusivly selectable.
  lmSelectAny       ///< Items are arbitrarily selectable.
};


typedef void FCbListboxPushed (class CListbox *, int idx, bool longPush, void *data);
  ///< Callback prototype for CListbox push events.


/** Macro to create a trampoline to another class.
 * The class must define a method:
 *
 *     void CLASS_NAME::METHOD_NAME (CListbox *listbox, int idx, bool longPush)
 *
 * To the list box, the trampoline must be passed by a call like:
 *
 *     CListbox::SetCbPushed (CB_NAME, <class object pointer>)
 */
#define LISTBOX_TRAMPOLINE(CB_NAME, CLASS_NAME, METHOD_NAME) \
  static void CB_NAME (class CListbox *listbox, int idx, bool longPush, void *data) {   \
    ((CLASS_NAME *) data)->METHOD_NAME (listbox, idx, longPush);                        \
  }


/// Listbox item.
class CListboxItem {
  public:
    CListboxItem () { text = NULL; data = NULL; surfIcon = NULL; isSelected = isSpecial = false; changed = true; }

    bool IsSelected () { return isSelected; }

    const char *text;        ///< Reference to (optional) text (class is not owner).
    SDL_Surface *surfIcon;   ///< Reference to (optional) icon surface (class is not owner).

    bool isSpecial;   ///< Item is special, e.g. a head line.
    void *data;       ///< Reference to optional user data.

  protected:
    friend class CListbox;

    bool isSelected;
    bool changed;       ///< If set, item will be re-rendered at next occasion
};


/** Listbox widget.
 */
class CListbox: public CCanvas {
  public:
    CListbox ();
    ~CListbox ();

    /// @name Setup ...
    /// @{
    void SetMode (EListboxMode _mode, int _itemHeight, int _itemGap = 1);
      // 'itemHeight' == 0 indicates variable-height items (Note: presently much worse performance than fixed-height)
      // implicitly deletes all items
    void SetFormat (TTF_Font *_font, TColor colGrid = BLACK, TColor _colLabel = WHITE, TColor _colBack = DARK_DARK_GREY, TColor _colSelected = GREY, TColor _colSpecial = DARK_GREY);
      // parameters set here are only read in 'RenderItem' & can be ommited (or re-interpreted) if a custom render function is defined.
      // 'colGrid' is the general background color which can also be set via 'CCanvas::SetColors'.
    void SetArea (SDL_Rect _area) { CCanvas::SetArea (_area); ChangedSetup (); }
    /// @}

    /// @name Content management ...
    /// @{
    void Clear () { SetItems (0); }
    void SetItems (int _items);
    int GetItems () { return items; }
    void SetItem (int idx, const char *_text, SDL_Surface *_surfIcon = NULL, bool _isSpecial = false, void *data = NULL);
    CListboxItem *GetItem (int idx) { return &itemArr[idx]; }

    SDL_Rect GetItemRect (int idx);
    void ScrollTo (int idx, int vAlign = -1) { CCanvas::ScrollTo (GetItemRect (idx), 0, vAlign); Changed (); }
    void ScrollIn (int idx) { CCanvas::ScrollIn (GetItemRect (idx)); Changed (); }
    /// @}

    /// @name Content selection ...
    /// @{
    void SelectItem (int idx, bool _isSelected = true);   // for 'lmSelectSingle', 'lmSelectAny'
    void SelectAll (bool _isSelected = true);    // 'lmSelectAny'
    void SelectNone () { SelectAll (false); }    // 'lmSelectSingle', 'lmSelectAny'
    /// @}

    /// @name Returning results ...
    /// @{
    bool ItemIsSelected (int idx) { return itemArr[idx].isSelected; }
    int GetSelectedItem () { return selectedItem; }  // 'lmSelectSingle' only

    virtual void OnPushed (int idx, bool longPush) { if (cbPushed) cbPushed (this, idx, longPush, cbPushedData); }
    void SetCbPushed (FCbListboxPushed *_cbPushed, void *_data = NULL) { cbPushed = _cbPushed; cbPushedData = _data; }
    /// @}

    /// @name Change management ...
    /// @{
    void ChangedItems (int idx, int num = 1);    ///< Must be called if items changed and SetItem() was not called.
    /// @}

  protected:
    CListboxItem *itemArr;
    int items;
    CWidget **pool;
    int poolSize, *poolIdx;
    int selectedItem, downIdx;
    bool noLongPush;
    bool changed;

    // Configuration options...
    EListboxMode mode;
    int itemGap, itemHeight;
    FCbListboxPushed *cbPushed;
    void *cbPushedData;

    // Infos for 'RenderItem' only...
    TTF_Font *font;
    TColor colLabel, colBack, colSelected, colSpecial;

    /// @name Rendering ...
    /// @{
    virtual SDL_Surface *RenderItem (CListboxItem *item, int idx, SDL_Surface *surf);
      ///< Render a list box item to an SDL surface and return it.
      /// This method may be overriden to implement a custom look of the items.
      /// The default implementation renders left-justified label and text from 'CListboxItem::surfLabel' and 'CListbox::text'
      /// according the settings given by 'SetFormat' and interprets 'CListboxItem::isSpecial'.
      /// The dimensions of the returned surface must match 'area.w' and 'itemHeight' (if >= 0).
      /// If 'itemHeight' == 0, the height is arbitrary and the layout will be adapted accordingly.
      /// The caller becomes owner of the returned surface.
      /// 'surf' is the previous surface which can be reused. If it is not NULL, it must be free'd or reused.
    /// @}

    /// @name Callbacks...
    /// @{
    virtual void Render (SDL_Renderer *ren);
    virtual bool HandleEvent (SDL_Event *ev);
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
/// Menu widget and function
/// @{


/** Menu widget.
 */
class CMenu: public CListbox, public CModalWidget {
  public:
    CMenu () { texFrame = NULL; }
    ~CMenu () { TextureFree (&texFrame); }

    /// @name Setup ...
    /// @{
    void Setup (SDL_Rect _rContainer, int _hAlign = -1, int _vAlign = -1,  // The exact geometry is determined in 'Start'
                TColor color = DARK_GREY, TTF_Font *_font = FontGet (fntNormal, 20));
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
      ///< Return whether the push was a long push.
    /// @}

    /// @name Misc. ...
    /// @{
    SDL_Rect *GetArea () { return &rFrame; }   // Note: Is different from 'CWidget::area', which here only contains the canvas part of the menu
    /// @}

  protected:
    SDL_Rect rContainer, rFrame;
    int hAlign, vAlign;
    CString itemStr;
    SDL_Texture *texFrame;
    bool hadLongPush;

    /// @name Callbacks...
    /// @{
    virtual void Render (SDL_Renderer *ren);
    virtual bool HandleEvent (SDL_Event *ev);
    virtual void OnPushed (int idx, bool longPush);
    /// @}
};


// Easy-to-use function(s)...
int RunMenu (const char *_itemStr, SDL_Rect _rContainer, int _hAlign = -1, int _vAlign = -1, TColor color = DARK_GREY, bool *retLongPush = NULL);
  ///< Run a menu with a single function call.
  /// @param _itemStr is a string containing the menu items separated by '|'.
  /// @param _rContainer specifies the rectangle in which the menu is placed.
  /// @param _hAlign specifies the horizontal alignment inside the container rectangle.
  /// @param _vAlign specifies the vertical alignment inside the container rectangle.
  /// @param color is the main menu color.
  /// @param retLongPush optionally points to a variable which is set if the menu was completed with a long push.


/// @}  // ui_widgets_menu





// *************************** Message Box *************************************


/// @defgroup ui_widgets_messagebox Message Box
/// Message box widget and functions
/// @{


#define MSGBOX_DEFAULT_FONT FontGet (fntNormal, 24)   ///< Default font for message box contents.


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


/** Message box widget
 *
 * See CModalWidget for options to run the message box.
 */
class CMessageBox: public CModalWidget {
  public:
    CMessageBox () { buttons = 0; buttonArr = NULL; screenHasKeyboard = false; }
    ~CMessageBox () { Stop (); SurfaceFree (&surface); }

    /// @name Setup ...
    /// @{
    void Setup (int contentW, int contentH, int _buttons, CButton **_buttonArr);
      ///< The most flexible way.
      /// 'content' is arbitrary content and must be transparent in empty areas.
      /// The buttons must be predefined, only the areas of all buttons will be set by 'Setup'.
      /// All parameters must remain valid until the message box is stopped.
      // TBD: FIX this: replace int "int contentW, int contentH" by "SDL_Surface *content"!!!
    CButton *GetStdButton (EMessageButtonId buttonId);
      ///< Initialize and return a standard button to be used by the previous method.

    void Setup (int contentW, int contentH, int buttonMask);
      ///< The easier way, if only standard buttons are used.
      /// 'buttonMask' is a bit-wise "or" of the message mask constants ('mbm...').
      // TBD: FIX this: replace int "int contentW, int contentH" by "SDL_Surface *content"!!!

    void Setup (const char *title, const char *text, SDL_Surface *icon = NULL, int buttonMask = mbmOk, int hAlign = 0, TTF_Font *font = NULL);
      ///< The easiest way for standard text boxes.
    /// @}

    /// @name Obtaining the result ...
    /// @{
    int GetChoice () { return GetStatus (); }  ///< Return the number of the button pushed, numbered from right to left; the rightmost button (typically "Cancel") has number 0.
    /// @}

    /// @name Callbacks ...
    /// @{
    virtual void Start (CScreen *_screen);
    virtual void Stop ();
    void OnButtonPushed (CButton *button, bool);
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

int RunInfoBox (const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
int RunInfoBox (const char *title, const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
int RunWarnBox (const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
int RunWarnBox (const char *title, const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
int RunErrorBox (const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
int RunErrorBox (const char *title, const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);

int RunSureBox (const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
int RunSureBox (const char *title, const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
  ///< Returns 1 if sure and <= 0 otherwise
int RunQueryBox (const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
int RunQueryBox (const char *title, const char *text, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
  ///< Returns 2 on "yes", 1 on "no" and 0 otherwise
/// @}


/// @name Manually operated message boxes ...
/// (e.g. for "please wait..." popups)
/// @{
CMessageBox *StartMessageBox (const char *title, const char *text, int buttonMask = mbmOk, SDL_Surface *icon = NULL, int hAlign = 0, TTF_Font *font = NULL);
static inline void IterateMessageBox () { UiIterate (); }
static inline int GetMessageBoxStatus (CMessageBox *msgBox) { return msgBox->GetStatus (); }
void StopMessageBox (CMessageBox *msgBox);
/// @}


/// @}  // ui_widgets_messagebox





// *************************** CInputLine **************************************


/// @defgroup ui_widgets_inputline Input Line
/// Input line widget
/// @{


#define INPUTLINE_UNDOS 32


/** Input line widget
 */
class CInputLine: public CCanvas {
  public:
    CInputLine () { font = NULL; inputLen = 0; mark0 = markD = 0; suggPos = -1; cbSuggCheck = NULL; suggData = NULL; undoFirst = redoFirst = NULL; surfMain = NULL; changedContent = changedMark = false; }
    ~CInputLine () { SurfaceFree (&surfMain); ClearHistory (); }

    /// @name Setup ...
    /// @{
    void Setup (int fontSize = 24);
    void SetInput (const char *_inputStr = NULL, int _mark0 = 0, int _markD = 0);
      ///< Set the input contents; implicitly calls 'ClearHistory' afterwards.
    void ClearHistory ();   ///< Clear the undo & redo history.
    /// @}

    /// @name Obtaining the result ...
    /// @{
    void GetResult (CString *ret) { ret->SetFromIso8859 (input.Get ()); }
    bool Modified ();      ///< Return if anything may have changed.
    /// @}

    /// @name Editing ...
    /// @{
    int GetMark0 () { return mark0; }    ///< Return the cursor position.
    int GetMarkD () { return markD; }
    void SetMark (int _mark0, int _markD = 0);
    void MoveMark (int _mark0);         ///< Set 'mark0' while leaving 'mark0 + markD' constant.

    void InsChar (char c);
    void InsText (const char *txt, int chars = -1);
    void DelMarked ();
    void DelChar (int pos);

    CString *GetInput () { return &input; }    /// Returns String (ISO 8859!) that may be modified arbitrarily, followed by a call to ChangedInput().
    void ChangedInput ();   ///< To be called if the imput string was changed.
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

    // Callbacks & helpers...
    virtual void Render (SDL_Renderer *ren);
    int GetCharOfMouseEvent (SDL_Event *ev);
    virtual bool HandleEvent (SDL_Event *ev);
};


/// @}  // ui_widgets_inputline





// *************************** CSlider ****************************************


/// @defgroup ui_widgets_slider Slider
/// Horizontal or vertical slider
/// @{


#define SLIDER_WIDTH 48           ///< Default width of a slider.
#define SLIDER_BAR_HEIGHT 16      ///< Default height of a slider bar.


/** Macro to define a trampoline to another class.
 * The class must define a method:
 *
 *     void CLASS_NAME::METHOD_NAME (CSlider *slider, int val, int lastVal)
 *
 * To the slider, the trampoline must be passed by a call like:
 *
 *     CSlider::SetCbValueChanged (CB_NAME, <class object pointer>)
 */
#define SLIDER_TRAMPOLINE(CB_NAME, CLASS_NAME, METHOD_NAME)                         \
  static void CB_NAME (class CSlider *slider, int val, int lastVal, void *data) {   \
    ((CLASS_NAME *) data)->METHOD_NAME (slider, val, lastVal);                      \
  }


/** Slider widget
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
      ///< Set the logical range of values; Possible values for 'val' are then '_val0 <= val <= _val1'.
      /// If '_continuousUpdate' is false, OnValueChanged() is only called when the slider is released.
      /// No action callback is invoked, even if the value has been clipped.
    /// @}

    /// @name Read and set value...
    /// @{
    int GetValue () { return val; }
    void SetValue (int _val, bool callOnValueChanged = false);
      ///< Set the current value; The passed value will be clipped to the allowed range.
    /// @}

    /// @name Action callbacks...
    /// @{
    virtual void OnValueChanged (int val, int lastVal);
    void SetCbValueChanged (void (*_cbValueChanged) (class CSlider *, int, int, void *), void *_data = NULL) { cbValueChanged = _cbValueChanged; cbValueChangedData = _data; }
    /// @}

  protected:
    int sliderW, barH;
    TColor colSlider, colBarLower, colBarUpper, colBack;
    bool continuousUpdate, isDown, redraw;

    int val0, val1, val;
    int slider0, downX;    // X position of slider and of last "touch down" event

    void (*cbValueChanged) (class CSlider *, int, int, void *);
    void *cbValueChangedData;

    /// @name Callbacks...
    /// @{
    virtual SDL_Surface *GetSurface ();
    virtual bool HandleEvent (SDL_Event *ev);
    /// @}

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
