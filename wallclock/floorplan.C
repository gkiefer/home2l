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


#include "floorplan.H"
#include "system.H"
#include "apps.H"
#include "app_phone.H"

#include <resources.H>

#include <dirent.h>


static class CFloorplan *fpFloorplan = NULL;
static class CScreenFloorplan *fpScreen = NULL;





// *************************** Environment options *****************************


ENV_PARA_STRING ("floorplan.rcTree", envFloorplanRcTree, "/alias");
  /* Root of the resource tree for floorplan gadgets
   *
   * Resources for floorplan gadgets are expected to have an ID like
   * \texttt{<floorplan.rcTree>/<floorplan ID>/<gadget ID>/<resource>}.
   */

ENV_PARA_STRING ("floorplan.useStateRc", envFloorplanUseState, "/local/timer/twilight/day");
  /* Resource representing the current home's use state (present, absent, ...)
   *
   * If defined, certain sensor data is highlighted (e.g. open windows or locks)
   * are highlighted depending on the use state.
   *
   * The resource may have type 'rctUseState' or 'bool'. A boolean value of
   * 'false' is interpreted as 'night', a boolean value of 'true' is equivalent
   * to 'day'.
   */

ENV_PARA_STRING ("floorplan.weatherRc", envFloorplanWeather, NULL);
  /* Resource representing the weather status
   *
   * If defined, certain sensor data is highlighted depending on the weather.
   *
   * At present, the resource must have type 'rctBool', and a value of 'true'
   * is interpreted as a warning of any kind (rain or worse). In the future,
   * an enumeration type may be introduced here to distinguish different
   * warning conditions (e.g. storm, rain, snow).
   */


ENV_PARA_STRING("floorplan.requestAttrs", envFloorplanReqAttrs, NULL);
  /* Request attributes for user interactions with the floorplan [\refenv{rc.userReqAttrs}]
   *
   * Define request attributes for any user interactions with the floorplan.
   *
   * By default, the value of \refenv{rc.userReqAttrs} is used.
   */


ENV_PARA_INT ("floorplan.motionRetention", envFloorplanMotionRetention, 300);
  /* Retention time (s) for a motion detector display
   */



// ***** Documentation of per-gadget options *****


ENV_PARA_BOOL ("floorplan.rwin.shades", envFloorplanRwinShades, false);
  /* Enable/disable the shades resource for roof window (rwin) gadgets.
   *
   * This sets the default for any \refenv{floorplan.gadgets.<gadgetID>.shades}
   * setting.
   */
ENV_PARA_SPECIAL ("floorplan.gadgets.<gadgetID>.shades", bool, NODEFAULT);
  /* For roof window (rwin) gadgets: Enable shades resource
   *
   * If \texttt{<gadgetID>} refers to a roof window with electric shades,
   * this option should be set 'true' and a resource referred by
   * \texttt{/alias/<floorplan>/<gadget>/shades} is used to control it.
   *
   * By default, the \refenv{floorplan.rwin.shades} setting is used.
   */


ENV_PARA_BOOL ("floorplan.rwin.actuator", envFloorplanRwinActuator, false);
  /* Enable/disable the actuator resource for roof window (rwin) gadgets.
   *
   * This sets the default for any \refenv{floorplan.gadgets.<gadgetID>.actuator}
   * setting.
   */
ENV_PARA_SPECIAL ("floorplan.gadgets.<gadgetID>.actuator", bool, NODEFAULT);
  /* For roof window (rwin) gadgets: Enable an actuator resource
   *
   * If \texttt{<gadgetID>} refers to a roof window with an actuator for opening/closing,
   * this option should be set 'true' and a resource referred by
   * \texttt{/alias/<floorplan>/<gadget>/actuator} is used to control it.
   *
   * By default, the \refenv{floorplan.rwin.actuator} setting is used.
   */


ENV_PARA_SPECIAL ("floorplan.gadgets.<gadgetID>.dial", const char *, NULL);
  /* For phone gadgets: Set the number to dial resource
   *
   * Number to dial if a phone icon is pushed in the floorplan.
   * By default, if the gadget ID ends with digits, the trailing digits
   * are dialed with asterix ("*") prepended.
   */





// *****************************************************************************
// *                                                                           *
// *                          Headers                                          *
// *                                                                           *
// *****************************************************************************



// ***************** Model-related classes ***************************


enum EFloorplanViewLevel {
  fvlNone = -1,
  fvlMini = 0,
  fvlFull,
  fvlZoom,    // (reserved for zoom)
  fvlEND
};

#define FP_MAX_VIEWS ((int) fvlZoom)  //  ((int) fvlEND)
#define FP_MAX_GADGET_RESOURCES 4     // Maximum average number of resources the gadgets can depend on


static inline bool GadgetTypeIsIcon (EGadgetType t) { return t >= gtLock && t <= gtService; }


enum EGadgetEmph {
  geNone = 0,
  geAttention,  // attention recommended; e.g. unlocked door at night or running service when out of home.
  geAlert,      // action required; e.g. open window during bad weather
  geError,      // technical problem (resource unavailable)
  geEND
};


class CGadget {
  public:
    CGadget ();
    virtual ~CGadget ();

    // Init/Done ...
    void InitBase (class CFloorplan *_floorplan, const char *_gdtId, EGadgetType _gdtType);
      ///< Is called before InitSub() to do common initializations.
    virtual void InitSub (int _x, int _y, int _orient, int _size) = 0;
      ///< Is called after 'InitBase' during construction.
      /// It must
      ///   a) set all specific static properties as stated below.
      ///   b) register all resources.
      ///   c) initialize all sub-class custom fields.
    void RegisterResource (CResource *rc);  ///< Helper for 'InitSub()'

    // Querying (static) properties ...
    class CFloorplan *Floorplan () { return floorplan; }
    const char *Id () { return gdtId.Get (); }
    EFloorplanViewLevel VisibilityLevel () { return visibilityLevel; }
    bool IsVisible (EFloorplanViewLevel level) { return level >= visibilityLevel; }
    bool IsPushable () { return pushable; }
    SDL_Rect *BaseArea () { return &baseArea; }      // Exact area of the surface surface (without surrounding)

    // Gadget-type specific callbacks ...
    virtual bool UpdateSurface () = 0;
      ///< Update the surface and all related fields ('surf*').
      /// Must maintain 'surf' to point to a valid surface. The subclass is the owner of 'surf'.
      /// This method is always automatically called if:
      ///   a) any of the resources declared with RegisterResource() had a "value/state changed" event.
      ///   b) On initialization or view changes.
      ///
      /// @return hint wether redrawing may be necessary (returning 'true' is usually ok;
      ///    returning 'false' on no change may improve performance)

    virtual void OnPushed (CButton *btn, bool longPush) {}
      ///< Is called whenever a pushable gadget is pushed.
      /// To open menus etc. inside this function, the surrounding screen (or its absence!)
      /// can be determined by 'Floorplan ()->Screen ()'.

    // View-related ...
    void SetView (EFloorplanViewLevel _viewLevel);
      ///< Set the current view level to 'level' and update the surface and view area/frame
    SDL_Rect *ViewArea () { return &viewArea; }      // Exact area of the surface surface

    // Getting the surface...
    SDL_Surface *Surface () { return surf; }
    EGadgetEmph SurfaceEmph () { return surfEmph; }

  protected:
    friend class CFloorplan;

    // Common static properties (set in InitBase() )...
    class CFloorplan *floorplan;
    CString gdtId;
    EGadgetType gdtType;

    // Specific static properties (must be set in sub-class InitSub(), if non-default values are appropriate)...
    SDL_Rect baseArea;                    // basic geometry (no default - must always be set)
    EFloorplanViewLevel visibilityLevel;  // Default = 'fvlFull'
    bool pushable;                        // Default = 'false'

    // Current view properties ...
    EFloorplanViewLevel viewLevel;
    SDL_Rect viewArea;              // = 'baseArea' transformed to the current view

    // Surface & properties ...
    //   These fields are managed by the subclass and must be updated in 'UpdateSurface ()' depending
    //   on any other fields above.
    SDL_Surface *surf;              // owner is the subclass
    EGadgetEmph surfEmph;
};


class CFloorplan {
  public:
    CFloorplan () { Init (); }
    ~CFloorplan () { Done (); }
    void Init ();
    void Done ();
    void Clear () { Done(); Init (); }

    // Setup ...
    bool Setup (const char *_fpoName);
      ///< Load a floorplan object (.fpo directory) and setup this object.

    // Accessing gadgets and other information ...
    int Gadgets () { return gadgets; }
      ///< Get number of gadgets.
    CGadget *Gadget (int idx) { return gadgetList[idx]; }

    const char *Lid () { return lid.Get (); }

    // Set/get view ...
    //   Note: Only one view is allowed at a time.
    void SetViewGeometry (EFloorplanViewLevel level, int _scale, int _x0, int _y0);
      ///< Set geometry parameters for a view.
      /// This must be called before the first SetView() call for view level 'level'.
      /// @param x0 is the X offset (in view coordinates).
      /// @param y0 is the Y offset (in view coordinates).
      /// @param scale is the LD of the scale; 0 corresponds to the resolution of the mini view (256x128),
      ///    any other value to a zoom factor of 2^n (e.g. 1024x512 => scale = 2)
    int GetViewScale (EFloorplanViewLevel level) { return scale[level]; }
    int GetX0 (EFloorplanViewLevel level) { return x0[level]; }
    int GetY0 (EFloorplanViewLevel level) { return y0[level]; }

    void SetView (EFloorplanViewLevel _viewLevel, CScreen *_screen = NULL);
      ///< Select a view level and assign the floorplan to a screen.
      /// This will set subscriptions according to the given level.
      /// Subscriptions are set independent of '_screen'. For the mini display, '_screen' must be NULL.

    // Iterate ...
    void Iterate ();

    int ChangedGadgets () { return changedGadgets; }
      ///< Get the number of changed gadgets after last 'Iterate()' call.
    int ChangedGadgetIdx (int n) { return changedGadgetsIdxList[n]; }
      ///< Get index of n'th changed gadget.
    CGadget *ChangedGadget (int n) { return gadgetList[changedGadgetsIdxList[n]]; }
      ///< Get n'th changed gadget.
    bool ChangedEmph () { return emphChanged; }
    SDL_Surface *GetEmphSurface ();

    bool HaveAlert () { return emphGadgetsBlinking != 0; }

    // Statistics ...
    int EmphGadgets () { return emphGadgets; }
    int EmphGadgetsBlinking () { return emphGadgetsBlinking; }

    // Screen display...
    CScreen *Screen () { return screen; }
    SDL_Surface *GetBuildingSurface (EFloorplanViewLevel level) { return buildingSurfList[level]; }
      ///< Get the background image showing the static building elements.
    void UnsetScreen () { SetView (fvlMini, NULL); }
      ///< Unassign a screen.
      /// This will tell me that the floorplan is no longer visible on a full or zoom screen.
      /// **Note:** Subscriptions will be switched to "mini" level.

    // Helpers for 'CGadget' objects...
    void RegisterResource (CGadget *gdt, CResource *rc);
      ///< Register a resource to be sensitive on; to be used only in 'CGadget::InitSub()'.
      /// It is legal to pass NULL (in which case nothing happens).
    CResource *UseStateRc () { return rcUseState; }
      ///< Get the resource representing the use state.
      /// Gadgets depending on this may query this and register the resource for changes.
    ERctUseState GetValidUseState ();
      ///< Get the current use state.
    CResource *WeatherRc () { return rcWeather; }
      ///< Get the resource representing weather warnings.
      /// Gadgets depending on this may query this and register the resource for changes.
      /// At present, this resource must be of type 'bool' and is set if the windows
      /// should be closed due to rain or similar conditions. In the future, a new weather
      /// condition type may be introduced so that, for example, a warning to open the shades
      /// during storm or snow may be issued, too.
    bool GetValidWeather ();
      ///< Get the current value of the weather warning state.
    CResource *TimerRc () { return RcGet ("/local/timer/now"); }
      ///< Get the resource representing the current time.
      /// This is used by the motion gadget to switch off the icon after the retention time.

  protected:

    // General ...
    CString fpoName, lid;
    CResource *rcUseState, *rcWeather;
    int preScale;     // 0 = floorplan has 256x128 pixels; 1 = floorplan has 128x64 pixels

    // Gadgets ...
    int gadgets, registeringGadget;
    CGadget **gadgetList;

    // Views ...
    int viewLevels;
    EFloorplanViewLevel viewLevel;
    int scale[FP_MAX_VIEWS], x0[FP_MAX_VIEWS], y0[FP_MAX_VIEWS];
    SDL_Surface *buildingSurfList[FP_MAX_VIEWS];
    class CScreen *screen;

    // Resources and subscriptions ...
    CRcSubscriber subscr;
    int rcGdtEntries;
    struct SResourceAndGadget *rcGdtList;
    int changedGadgets, *changedGadgetsIdxList;

    // Gadget emphasis...
    int emphGadgets, *emphGadgetsIdxList;
    int emphGadgetsBlinking;

    bool emphChanged;
    SDL_Surface *emphSurf;
    TTicksMonotonic emphBlinkT;
    bool emphBlinkOn;
};





// ***************** View-related classes ***********************


class CScreenFloorplan: public CScreen, public CTimer {
  public:
    CScreenFloorplan ();
    virtual ~CScreenFloorplan () { Clear (); }

    void Clear ();
    void Setup (CFloorplan *_floorplan, TTicksMonotonic _tInterval = FP_UPDATE_INTERVAL);

    void CheckAlert (CScreen *_returnScreen = NULL);

    // Callbacks...
    virtual void Activate (bool on = true);   // from 'CScreen'
    virtual void OnTime ();                   // from 'CTimer'
    void OnButtonPushed (CButton *btn, bool longPush);
    virtual bool HandleEvent (SDL_Event *ev);

    // Updating ...
    void UpdateRequest (ERctUseState useStateReq = (ERctUseState) -1);

  protected:
    CFloorplan *floorplan;
    EFloorplanViewLevel view;
    TTicksMonotonic tInterval;

    CWidget wdgBuilding;  // static building plan for the background
    CWidget wdgEmph;      // Emphasis / highlighting
    CWidget **wdgList;    // elements may point to objects of class 'CWidget' or 'CFlatButton', depending on whether the gadget is pushable
    int pushableGadgets;
    CWidget *wdgPoolNorm;
    CFlatButton *wdgPoolPushable;

    CButton *buttonBar;
    ERctUseState lastUseState, lastUseStateReq;

    bool haveAlert;
    CScreen *returnScreen; // screen to return to after an alert
};





// *****************************************************************************
// *                                                                           *
// *                          Implementations                                  *
// *                                                                           *
// *****************************************************************************


// Settings related to icon geometries...
#define ICON_SCALE 3    // scale level of the stored bitmaps (3 corresponds to a zoomed view of 2048x1024 pixels)

#define FULL_SCALE 2    // scale level of the full-screen view


static CRcRequest *NewUserRequest () {
  CRcRequest *req = new CRcRequest ((CRcValueState *) NULL, NULL, rcPrioUser);
    // init with default attributes
  req->SetAttrsFromStr (envFloorplanReqAttrs ? envFloorplanReqAttrs : RcGetUserRequestAttrs ());
    // set request user attributes
  req->SetGid (RcGetUserRequestId ());
    // set GID
  return req;
}


static void HandleLongPush (CResource *rc) {
  CRcRequest *req, reqDefault;
  bool setNotReset = false, haveDefault;

  // Determine new user request ...
  req = NewUserRequest ();
  switch (rc->Type ()) {
    case rctBool:
      setNotReset = (rc->ValidBool (false) == false);
      req->SetValue (setNotReset ? true : false);
      break;
    case rctPercent:
      setNotReset = (rc->ValidFloat (0.0f) == 0.0f);
      req->SetValue (setNotReset ? 100.0f : 0.0f);
      break;
    default:
      // Unsupported type: Ignore long push
      FREEO (req);
  }

  // Check if we should do "auto" instead of "reset" ...
  if (req && !setNotReset) {

    // Check if a "_default" or "default" request has been set ...
    rc->GetRequest (&reqDefault, "_default");
    haveDefault = reqDefault.Value ()->IsValid ();
    if (!haveDefault) {
      rc->GetRequest (&reqDefault, "default");
      haveDefault = reqDefault.Value ()->IsValid ();
    }

    // If we have a default, just remove the current user request ...
    if (haveDefault) {
      FREEO (req);
      rc->DelRequest (RcGetUserRequestId ());
    }
  }

  // Set request if adequate ...
  if (req) rc->SetRequest (req);
}





// *************************** CResourceDialog & friends ***********************


#define RCDLG_PERCENT_STEPS 6         // Number of buttons for 'rctPercent' resources (steps including 0% and 100%
#define RCDLG_HORIZONTAL_THRESHOLD 2  // Up to this number of choices are put in a horizontal layout with buttons instead of a listbox.

#define RCDLG_VALUE_BUTTON_WIDTH 160  // width of the value button
#define RCDLG_CHOICE_MINWIDTH 160     // minimum width of a choice button or listbox

#define RCDLG_COL_CHOICE    BLACK     // color of (unselected) choice buttons or list items
#define RCDLG_COL_SELECTED  (color)   // color of selected choice ('color' = selected color variable)
//~ #define RCDLG_COL_CHOICE    ColorDarker (color, 0x40)
//~ #define RCDLG_COL_SELECTED  (color)


class CResourceDialog: public CMessageBox, public CTimer {
  public:
    CResourceDialog ();
    ~CResourceDialog () { Clear (); }
    void Clear ();

    void Setup (CResource *_rc, EGadgetType _subType, const char *_title);

    void SetLayout (bool _withInfo);
    void UpdateRequest (bool fetchReq);       // to be called if the request changed
    void UpdateView ();                       // to be called if the resource value/state changed
    int Run (CScreen *_screen);               // always returns 0

    // Callbacks...
    virtual void Start (CScreen *_screen);    // Must add own widgets
    virtual void Stop ();                     // Must del own widgets
    void OnButtonPushed (class CButton *, bool longPush);
    void OnListboxPushed (class CListbox *, int idx, bool longPush);
    virtual void OnTime ();                   // From 'CTimer': Refresh request list

  protected:
    void TimerSet () { CTimer::Set (-1024, 1024); }

    CString title;
    TColor color;
    CResource *rc;
    EGadgetType subType;
    CRcRequest request;   // current own request (e.g. with GID "#user")

    CButton btnValue;
    bool valueNotPlusButton;

    int choices;
    CListbox wdgChoices;
    CButton btnChoices[RCDLG_HORIZONTAL_THRESHOLD];
    int btnChoicesSelected;
    CString *choiceText;
    float *choiceVal;     // common type for values (string types are not supported)

    CButton btnBack, btnAuto, btnEdit;

    bool withInfo;
    CCanvas cvsInfo;
    CWidget wdgInfo;
    SDL_Surface *surfInfo;
};


class CScreenResourceEdit: public CInputScreen {
  public:
    CScreenResourceEdit () { inputReq = NULL; }
    ~CScreenResourceEdit () { if (inputReq) delete inputReq; }

    void Setup (CResource *_rc, CRcRequest *req) {
      rc = _rc;
      if (req) {
        req->Convert (_rc, false);
        CInputScreen::Setup (req->ToStr (false, false, 0, req->Priority () == rcPrioUser ? "#*@i" : "#@i"));
      }
      else {
        CString s;
        s.SetF (" %s", envFloorplanReqAttrs ? envFloorplanReqAttrs : RcGetUserRequestAttrs ());
        CInputScreen::Setup (s.Get ());
      }
    }

    CRcRequest *GetRequest () { CRcRequest *ret = inputReq; inputReq = NULL; return ret; }
      ///< Get request and ownership of the request object.

  protected:
    CResource *rc;
    CRcRequest *inputReq;

    // Callbacks ...
    virtual void Commit () {
      CString s;
      bool ok;

      // Create a request object and check it ...
      inputReq = new CRcRequest ();
      inputReq->SetGid (RcGetUserRequestId ());   // mainly to have 'inputReq->Convert()' print warnings with the correct ID
      inputReq->SetPriority (rcPrioUser);         // set default
      GetInput (&s);
      ok = inputReq->SetFromStr (s.Get ());
      if (ok) {
        inputReq->Convert (rc);
        ok = inputReq->IsCompatible ();
      }

      // Report error or complete ...
      if (ok) Return ();
      else {
        delete inputReq;
        inputReq = NULL;
        RunErrorBox (_("Syntax error in request specification"));
      }
    }
};


BUTTON_TRAMPOLINE(CbResourceDialogOnButtonPushed, CResourceDialog, OnButtonPushed);
LISTBOX_TRAMPOLINE(CbResourceDialogOnListboxPushed, CResourceDialog, OnListboxPushed);


CResourceDialog::CResourceDialog () {
  rc = NULL;
  subType = gtNone;
  valueNotPlusButton = false;
  surfInfo = NULL;

  choices = 0;
  btnChoicesSelected = -1;
  choiceText = NULL;
  choiceVal = NULL;

  withInfo = false;
  surfInfo = NULL;
}


void CResourceDialog::Clear () {
  wdgInfo.SetSurface (NULL);
  SurfaceFree (&surfInfo);
  if (choices) {
    delete [] choiceText;
    free (choiceVal);
    choices = 0;
  }
}


void CResourceDialog::Setup (CResource *_rc, EGadgetType _subType, const char *_title) {
  ERcType rcType;
  bool reverse;
  int n, idx, itemHeight;

  // Store parameters ...
  Clear ();
  rc = _rc;
  subType = _subType;
  withInfo = false;

  // Set title ...
  title.Clear ();
  if (_title) title.Set (_title);
  else switch (_subType) {
    case gtWindow:
    case gtRoofWindow:  title.SetC (_("Window"));     break;
    case gtShades:      title.SetC (_("Shades"));     break;
    case gtLight:       title.SetC (_("Light"));      break;
    case gtMail:        title.SetC (_("Mail"));       break;
    case gtPhone:       title.SetC (_("Phone"));      break;
    case gtMusic:       title.SetC (_("Music"));      break;
    case gtWlan:        title.SetC (_("Wifi Access Point"));  break;
    case gtBluetooth:   title.SetC (_("Bluetooth"));  break;
    case gtService:     title.SetC (_("Service"));    break;
    default:
      title.SetC (rc->Uri ());
  }

  // Analyse type and prepare choices and color ...
  color = DARK_BLUE;
  rcType = rc->Type ();
  switch (rcType) {

    case rctBool:
      choices = 2;
      choiceText = new CString [2];
      choiceVal = MALLOC (float, 2);
      choiceText[0].SetC (_("Off"));
      choiceVal[0] = 0.0;
      choiceText[1].SetC (_("On"));
      choiceVal[1] = 1.0;
      break;

    case rctPercent:
      reverse = (_subType == gtWindow || _subType == gtRoofWindow);
      choices = RCDLG_PERCENT_STEPS;
      choiceText = new CString [RCDLG_PERCENT_STEPS];
      choiceVal = MALLOC (float, RCDLG_PERCENT_STEPS);
      for (n = 0; n < RCDLG_PERCENT_STEPS; n++) {
        idx = reverse ? (RCDLG_PERCENT_STEPS - 1 - n) : n;
        choiceVal[idx] = (n * 100.0) / (RCDLG_PERCENT_STEPS - 1);
        choiceText[idx].SetF ("%.0f%%", choiceVal[idx]);
      }
      switch (_subType) {
        case gtShades:
          choiceText[0].SetC (_("0% = Up"));
          choiceText[RCDLG_PERCENT_STEPS-1].SetC (_("100% = Down"));
          break;
        case gtWindow:
        case gtRoofWindow:
          color = DARK_RED;
          choiceText[0].SetC (_("100% = Open"));
          choiceText[RCDLG_PERCENT_STEPS-1].SetC (_("0% = Closed"));
          break;
        default:
          break;
      }
      break;

    default:
      if (RcTypeIsEnumType (rcType)) {
        choices = RcTypeGetEnumValues (rcType);
        choiceText = new CString [choices];
        choiceVal = MALLOC (float, choices);
        for (n = 0; n < choices; n++) {
          choiceText[n].SetC (RcTypeGetEnumValue (rcType, n, true));
          choiceVal[n] = (float) n;
        }
      }
      // Default: Leave empty (choices = 0)
  }
  valueNotPlusButton = !RcTypeIsEnumType (rcType) && (rcType != rctBool);

  // Prepare buttons or listbox for choices...
  if (choices > RCDLG_HORIZONTAL_THRESHOLD) {
    // Listbox ...
    itemHeight = 360 / choices;
    if (itemHeight < 32) itemHeight = 32;
    if (itemHeight > 128) itemHeight = 128;
    wdgChoices.SetMode (lmSelectSingle, itemHeight);
    wdgChoices.SetFormat (
        FontGet (fntMono, 24), 0, DARK_GREY,    // font, alignment, grid
        WHITE, RCDLG_COL_CHOICE,                // normal (label, back)
        WHITE, RCDLG_COL_SELECTED,              // selected = requested state (label, back)
        YELLOW, RCDLG_COL_CHOICE                // special = real state (label, back)
      );
    wdgChoices.SetCbPushed (CbResourceDialogOnListboxPushed, this);
    wdgChoices.SetItems (choices);
    for (n = 0; n < choices; n++) wdgChoices.SetItem (n, choiceText[n]);
    wdgChoices.Render (NULL);     // Dummy render pass to set/update the virtual area
  }
  else if (choices > 0) {
    // Horizontal buttons (typically 2 for "on" and "off") ...
    for (n = 0; n < choices; n++) {
      CButton *btn = &btnChoices[n];
      btn->SetColor (RCDLG_COL_CHOICE);
      btn->SetCbPushed (CbResourceDialogOnButtonPushed, this);
    }
    btnChoicesSelected = -1;
  }
  else ASSERT (choices == 0);

  // Prepare general buttons ...
  btnValue.SetColor (GREY);
  btnValue.SetHotkey (SDLK_i);
  btnValue.SetCbPushed (CbResourceDialogOnButtonPushed, this);

  btnBack.SetColor (GREY);
  btnBack.SetLabel (WHITE, "ic-back-48");
  btnBack.SetHotkey (SDLK_ESCAPE);
  btnBack.SetCbPushed (CbResourceDialogOnButtonPushed, this);

  btnAuto.SetColor (RCDLG_COL_CHOICE);
  btnAuto.SetLabel (_("Auto"), WHITE);
  btnAuto.SetHotkey (SDLK_a);
  btnAuto.SetCbPushed (CbResourceDialogOnButtonPushed, this);

  btnEdit.SetColor (GREY);
  btnEdit.SetLabel (WHITE, "ic-edit-48");
  btnEdit.SetHotkey (SDLK_e);
  btnEdit.SetCbPushed (CbResourceDialogOnButtonPushed, this);

  // Prepare infobox ...
  wdgInfo.SetArea (Rect (0, 0, 1, 1));
  wdgInfo.SetTextureBlendMode (SDL_BLENDMODE_BLEND);

  cvsInfo.SetColors (TRANSPARENT);
  cvsInfo.SetTextureBlendMode (SDL_BLENDMODE_BLEND);
  cvsInfo.AddWidget (&wdgInfo);

  // Set layout and update contents ...
  SetLayout (false);

  // Read own request and initialize choices selection based on it ...
  UpdateRequest (true);
}


void CResourceDialog::SetLayout (bool _withInfo) {
  SDL_Rect r, *layout;
  int n, w, h, wContent, hContent, wChoiceMax = 0;

  withInfo = _withInfo;

  // Step 1: Determine desired content area ...

  //   ... title row ...
  wContent = FontGetWidth (MSGBOX_TITLE_FONT, title.Get ()) + MSGBOX_SPACE_X
             + (valueNotPlusButton ? RCDLG_VALUE_BUTTON_WIDTH : UI_BUTTONS_HEIGHT);
  hContent = 0;

  //   ... choices and info area ...
  if (choices > RCDLG_HORIZONTAL_THRESHOLD) {

    // vertical listbox ...
    wChoiceMax = RCDLG_CHOICE_MINWIDTH;
    for (n = 0; n < choices; n++) {
      w = wdgChoices.GetItemLabelWidth (n) + MSGBOX_SPACE_X;
      if (w > wChoiceMax) wChoiceMax = w;
    }
    w = wChoiceMax;
    h = wdgChoices.GetVirtArea ()->h;
    if (withInfo) w = UI_RES_X;               // with info: get all we can
  }
  else if (choices > 0) {

    // horizontal buttons ...
    w = choices * (RCDLG_CHOICE_MINWIDTH + UI_BUTTONS_SPACE) - UI_BUTTONS_SPACE;
    h = UI_BUTTONS_HEIGHT;
    if (withInfo) {
      if (w < UI_RES_X*3/4) w = UI_RES_X*3/4;   // with info: get 3/4 of the screen horizontally ...
      h = UI_RES_Y;                             // ... and all we can vertically
    }
  }
  else {

    // no choices, info only ...
    if (withInfo) {
      w = UI_RES_X*3/4;   // with info: get 3/4 of the screen horizontally ...
      h = UI_RES_Y;       // ... and all we can vertically
    }
    else w = h = 0;       // without info: need nothing
  }
  wContent = MAX (wContent, w);
  hContent += h;

  //   ... buttons ...
  w = 2 * RCDLG_CHOICE_MINWIDTH + 2 * UI_BUTTONS_SPACE;
  wContent = MAX (wContent, w);
  hContent += MSGBOX_SPACE_Y;     // add space between choices and buttons
  hContent += UI_BUTTONS_HEIGHT;


  // Step 2: Setup message box ...
  CMessageBox::Setup (title.Get (),
                      wContent, hContent,
                      0, NULL,        // no standard buttons
                      MSGBOX_COLOR,
                      -1);            // title left-justified
  // TBD: Move to a target position?


  // Step 3: Layout contents ...

  //   ... title row ...
  r = Rect (valueNotPlusButton ? RCDLG_VALUE_BUTTON_WIDTH : UI_BUTTONS_HEIGHT, UI_BUTTONS_HEIGHT);
  RectAlign (&r, *GetArea (), 1, -1);
  r.x -= MSGBOX_SPACE_X;
  r.y += (MSGBOX_SPACE_Y / 2);
  btnValue.SetArea (r);
  if (!valueNotPlusButton) btnValue.SetLabel (withInfo ? "-" : "+", WHITE, FontGet (fntNormal, 32));

  //   ... choices and info area ...
  h = rContent.h - MSGBOX_SPACE_Y - UI_BUTTONS_HEIGHT;    // height available for this area
  if (choices > RCDLG_HORIZONTAL_THRESHOLD) {

    // vertical listbox ...
    r = Rect (withInfo ? MIN (wChoiceMax, UI_RES_X / 2) : wChoiceMax * 3/2, wdgChoices.GetVirtArea ()->h);
    if (r.w > rContent.w) r.w = rContent.w;
    if (r.h > h) r.h = h;
    RectAlign (&r, rContent, withInfo ? -1 : 0, -1);
    wdgChoices.SetArea (r);

    if (withInfo) {
      r = Rect (rContent.w - wdgChoices.GetArea ()->w - MSGBOX_SPACE_X, rContent.h - (UI_BUTTONS_HEIGHT + MSGBOX_SPACE_Y));
      RectAlign (&r, rContent, 1, -1);
      cvsInfo.SetArea (r);
    }
  }
  else if (choices > 0) {

    // horizontal buttons ...
    r = Rect (rContent.w, UI_BUTTONS_HEIGHT);
    RectAlign (&r, rContent, 0, 1);
    r.y -= MSGBOX_SPACE_Y + UI_BUTTONS_HEIGHT;    // move up by space for standard buttons
    layout = LayoutRowEqually (r, choices);
    for (n = 0; n < choices; n++) btnChoices[n].SetArea (layout[n]);
    free (layout);

    if (withInfo) {
      r = Rect (rContent.w, rContent.h - 2 * (UI_BUTTONS_HEIGHT + MSGBOX_SPACE_Y));
      RectAlign (&r, rContent, 0, -1);
      cvsInfo.SetArea (r);
    }
  }
  else {

    // no choices, info only ...
    if (withInfo) {
      r = rContent;
      r.h -= (MSGBOX_SPACE_Y + UI_BUTTONS_HEIGHT);    // make space for standard buttons
      cvsInfo.SetArea (r);
    }
  }

  //   ... buttons ...
  r = Rect (rContent.w, UI_BUTTONS_HEIGHT);
  RectAlign (&r, rContent, 0, 1);
  layout = LayoutRow (r, UI_BUTTONS_SPACE, -1, -2, -1, 0);
  btnBack.SetArea (layout[0]);
  btnAuto.SetArea (layout[1]);
  btnEdit.SetArea (layout[2]);
  free (layout);

  // Add/delete info widget ...
  if (screen) {
    if (withInfo) screen->AddWidget (&cvsInfo, 1);
    else screen->DelWidget (&cvsInfo);
  }

  // Done ...
  UpdateView ();
}


void CResourceDialog::UpdateRequest (bool fetchReq) {
  int n, idx;

  //~ INFOF (("### UpdateRequest (%i) ...", (int) fetchReq));

  // Read own request ...
  if (fetchReq) rc->GetRequest (&request, RcGetUserRequestId ());
  request.Convert (rc, false);   // In case of an incompatibility, a warning would have been emitted before.
  //~ INFOF (("###   request = '%s'", request.ToStr ()));

  // Match it against the choices ...
  idx = -1;
  if (request.Value ()->IsKnown () && request.IsCompatible ()) {
    for (n = 0; n < choices; n++) if (choiceVal[n] == request.Value ()->ValidFloat (NAN)) {
      idx = n;
      break;
    }
  }
  //~ INFOF (("###   idx = %i", idx));

  // Update choice listbox or buttons ...
  if (choices > RCDLG_HORIZONTAL_THRESHOLD) {
    if (idx >= 0) wdgChoices.SelectItem (idx);
    else wdgChoices.SelectNone ();
  }
  else {
    btnChoicesSelected = idx;
    for (n = 0; n < choices; n++)
      btnChoices[n].SetColor (n == btnChoicesSelected ? RCDLG_COL_SELECTED : RCDLG_COL_CHOICE);
  }

  // Set color of "Auto" button ...
  btnAuto.SetColor (request.Value ()->IsValid () ? RCDLG_COL_CHOICE : RCDLG_COL_SELECTED);

  // Update info view ...
  if (withInfo) UpdateView ();
}


void CResourceDialog::UpdateView () {
  CRcValueState vs;
  CTextSet textSet;
  CSplitString lines;
  CString s;
  SDL_Rect *cr;
  char *p;
  float rcVal;
  int n, mark0, mark1;
  bool mark;

  rc->GetValueState (&vs);

  // Set value button ...
  if (valueNotPlusButton) btnValue.SetLabel (vs.ToStr (), YELLOW);

  // Highlight current value or its neighbors ...
  rcVal = vs.ValidFloat (NAN);
  mark0 = mark1 = -1;
  if (!isnan (rcVal)) for (n = 0; n < choices; n++) if (!isnan (choiceVal[n])) {
    if (choiceVal[n] <= rcVal) {
      if (mark0 < 0) mark0 = n;
      else if (choiceVal[n] > choiceVal[mark0]) mark0 = n;
    }
    if (choiceVal[n] >= rcVal) {
      if (mark1 < 0) mark1 = n;
      else if (choiceVal[n] < choiceVal[mark1]) mark1 = n;
    }
  }
  // Now 'mark0' points to next smaller (or equal) value,
  // 'mark1' points to next larger (or equal) value.
  for (n = 0; n < choices; n++) {
    mark = (n == mark0 || n == mark1) ? true : false;
    if (choices > RCDLG_HORIZONTAL_THRESHOLD) {
      // vertical choices (listbox) ...
      if (wdgChoices.GetItem (n)->isSpecial != mark) {
        wdgChoices.GetItem (n)->isSpecial = mark;
        wdgChoices.ChangedItems (n);
      }
    }
    else {
      // horizontal choices (buttons) ...
      btnChoices[n].SetLabel (choiceText[n], mark ? YELLOW : WHITE);
    }
  }

  // Set info text...
  if (withInfo) {
    rc->GetInfo (&s, 1);
    for (p = (char *) s.Get (); *p && *p != '\n'; p++) if (*p == '=' && p > s.Get ()) { p[-1] = '\n'; break; }
      // Insert a line break before the '=' in the first line to improve readability.
    lines.Set (s.Get (), INT_MAX, "\n");
    for (n = 0; n < lines.Entries (); n++) {
      p = (char *) lines.Get (n);
      while (*p && *p == ' ') p++;
      mark = (*p == '=' || *p == '!');      // highlight lines starting with "=" (value) or "!" (requests)
      textSet.AddLines (lines.Get (n), CTextFormat (FontGet (fntMono, 20), mark ? WHITE : LIGHT_GREY));
    }
    SurfaceSet (&surfInfo, textSet.Render ());
    wdgInfo.SetArea (Rect (surfInfo));
    wdgInfo.SetSurface (surfInfo);
    cr = cvsInfo.GetVirtArea ();
    cvsInfo.SetVirtArea (Rect (cr->x, cr->y, surfInfo->w, surfInfo->h));
  }
}


void CResourceDialog::Start (CScreen *_screen) {
  int n;

  CMessageBox::Start (_screen);
  if (_screen) {
    _screen->AddWidget (&btnValue, 1);

    if (choices > RCDLG_HORIZONTAL_THRESHOLD)
      _screen->AddWidget (&wdgChoices, 1);
    else
      for (n = 0; n < choices; n++) _screen->AddWidget (&btnChoices[n], 1);
    if (withInfo) _screen->AddWidget (&cvsInfo, 1);

    _screen->AddWidget (&btnBack, 1);
    _screen->AddWidget (&btnAuto, 1);
    _screen->AddWidget (&btnEdit, 1);
  }
}


void CResourceDialog::Stop () {
  int n;

  if (screen) {
    screen->DelWidget (&btnValue);

    if (choices > RCDLG_HORIZONTAL_THRESHOLD)
      screen->DelWidget (&wdgChoices);
    else
      for (n = 0; n < choices; n++) screen->DelWidget (&btnChoices[n]);
    screen->DelWidget (&cvsInfo);

    screen->DelWidget (&btnBack);
    screen->DelWidget (&btnAuto);
    screen->DelWidget (&btnEdit);
  }
  CMessageBox::Stop ();
}


int CResourceDialog::Run (CScreen *_screen) {
  CRcSubscriber subscr;
  CRcEvent ev;
  bool update;

  subscr.Register ("rcdialog");
  subscr.AddResource (rc);

  Start (_screen);
  UpdateView ();
  TimerSet ();
  while (IsRunning ()) {
    UiIterate ();
    update = false;
    while (subscr.PollEvent (&ev)) if (ev.Type () == rceValueStateChanged) {
      //~ INFOF (("### Event = '%s'", ev.ToStr ()));
      update = true;
    }
    if (update) {
      UpdateView ();
      TimerSet ();
    }
  }
  CTimer::Clear ();
  return 0;
}


void CResourceDialog::OnButtonPushed (class CButton *btn, bool longPush) {
  int idx;

  if (btn == &btnValue) {
    SetLayout (!withInfo);
  }
  else if (btn == &btnBack) {
    Stop ();
  }
  else if (btn == &btnAuto) {
    btnChoicesSelected = -1;
    OnListboxPushed (NULL, -1, longPush);
  }
  else if (btn == &btnEdit) {
    CScreenResourceEdit scrEdit;
    CRcRequest *req;
    CScreen *myScreen = screen;

    Stop ();          // stop message box (would not survive switching to the edit screen)
    scrEdit.Setup (rc, request.Value ()->IsValid () ? &request : NULL);
    scrEdit.Run ();
    req = scrEdit.GetRequest ();
    if (req) {
      req->SetGid (RcGetUserRequestId ());    // force user GID, because it can otherwise not be edited again here
      request.Set (req);
      if (req->Value ()->IsValid ()) rc->SetRequest (req);
      else rc->DelRequest (req->Gid ());
    }
    UpdateRequest (false);
    Start (myScreen); // start the message box again
  }
  else {
    idx = btn - &btnChoices[0];
    if (idx >= 0 && idx < RCDLG_HORIZONTAL_THRESHOLD) {
      btnChoicesSelected = idx;
      OnListboxPushed (NULL, idx, longPush);
    }
  }
}


void CResourceDialog::OnListboxPushed (class CListbox *lb, int idx, bool longPush) {
  // This method also handles the horizontal layout case, in which case 'lb == NULL'.
  CRcRequest *req;

  //~ INFOF (("### CResourceDialog::OnListboxPushed (%i, %i)", idx, (int) longPush));
  if (lb) idx = wdgChoices.GetSelectedItem ();    // in the listbox case: check if the selection has just been unselected (= auto)
  if (idx < 0) {
    rc->DelRequest (RcGetUserRequestId ());
    request.Reset ();
  }
  else {
    req = NewUserRequest ();
    req->SetValue (choiceVal[idx]);
    request.Set (req);                            // store as current request
    rc->SetRequest (req);
  }
  UpdateRequest (false);
  if (!longPush && !withInfo) Stop ();
}


void CResourceDialog::OnTime () {
  UpdateView ();
}





// ***** RunResourceDialog() *****


void RunResourceDialog (CResource *rc, EGadgetType subType, const char *title) {
  CResourceDialog dlg;

  dlg.Setup (rc, subType, title);
  dlg.Run (CScreen::ActiveScreen ());
}





// *************************** CGadget + Subclasses ****************************


BUTTON_TRAMPOLINE(CbGadgetOnButtonPushed, CGadget, OnPushed);





// ********** Helpers **********


static CResource *GetGadgetResource (CGadget *gdt, const char *name) { // TTS
  return RcGet (StringF ("%s/%s/%s/%s", envFloorplanRcTree, gdt->Floorplan ()->Lid (), gdt->Id (), name));
}


static const char *GetGadgetEnvKey (CGadget *gdt, const char *name = NULL) {    // TTS
  return StringF (name ? "floorplan.gadgets.%s.%s" : "floorplan.gadgets.%s", gdt->Id (), name);
}


static ERctWindowState ReadValidWindowState (CRcValueState *vs) {
  // Read 'vs' as a window state in a type-tolerant way. Boolean values are
  // allowed as well, where 'false' is interpreted as "closed" and anything else
  // as "open or tilted".
  // Percentage values are interpreted as a actuator state, where 0.0% represents
  // the "closed" state, everything else "open or tilted".
  if (vs->Type () == rctWindowState)
    return (ERctWindowState) vs->ValidEnumIdx (rctWindowState, rcvWindowOpenOrTilted);
  else if (vs->Type () == rctPercent)
    return vs->ValidFloat (100.0) == 0.0 ? rcvWindowClosed : rcvWindowOpenOrTilted;
  else
    return vs->ValidBool (true) ? rcvWindowOpenOrTilted : rcvWindowClosed;
}





// ********** CGadget **********


CGadget::CGadget () {
  floorplan = NULL;
  gdtType = gtNone;
  visibilityLevel = fvlNone;
  pushable = false;

  viewLevel = fvlNone;
  surf = NULL;
  surfEmph = geNone;
}


CGadget::~CGadget () {
  surf = NULL;
}


void CGadget::InitBase (class CFloorplan *_floorplan, const char *_gdtId, EGadgetType _gdtType) {
  floorplan = _floorplan;
  gdtId.Set (_gdtId);
  gdtType = _gdtType;

  // Defaults for static properties...
  visibilityLevel = fvlFull;
  pushable = false;
}


void CGadget::RegisterResource (CResource *rc) {
  floorplan->RegisterResource (this, rc);
}


void CGadget::SetView (EFloorplanViewLevel _viewLevel) {
  int s;

  if (_viewLevel != viewLevel) {

    // Determine 'viewArea'...
    s = floorplan->GetViewScale (_viewLevel);
    viewArea.x = (baseArea.x << s) + floorplan->GetX0 (_viewLevel);
    viewArea.y = (baseArea.y << s) + floorplan->GetY0 (_viewLevel);
    viewArea.w = (baseArea.w << s);
    viewArea.h = (baseArea.h << s);
    if (GadgetTypeIsIcon (gdtType)) {   // limit icons to 96x96...
      ASSERT (viewArea.w == viewArea.h);
      if (viewArea.w > 96) {
        viewArea.x = viewArea.x + viewArea.w / 2 - 48;
        viewArea.y = viewArea.y + viewArea.h / 2 - 48;
        viewArea.w = viewArea.h = 96;
      }
    }

    // Update surface...
    viewLevel = _viewLevel;
    UpdateSurface ();
  }
}





// ********** gdtTypeInfo **********


static const struct {
  const char *name;
  const char *icon;
} gdtTypeInfo [gtEND] = {
  { NULL, NULL },      // gtNone = 0

  { "win",        NULL },      // gtWindow
  { "shades",     NULL },      // gtShades
  { "rwin",       NULL },      // gtRoofWindow
  { "garage",     NULL },      // gtGarage

  { "lock",       NULL },              // gtLock  (varying icons: "padlock" / "padlock_open")
  { "motion",     "walk" },            // gtMotion
  { "light",      "light" },           // gtLight
  { "mail",       "email" },           // gtMail
  { "phone",      "phone" },           // gtPhone
  { "music",      "audio" },           // gtMusic
  { "wlan",       "wifi_tethering" },  // gtWlan
  { "bluetooth",  "bluetooth" },       // gtBluetooth
  { "service",    "service" },         // gtService

  { "temp", NULL },      // gtTemp

  { "zoom", "zoom_in" }      // gtZoom
};





// ********** CGadgetWindow **********


class CGadgetWindow: public CGadget {
  // Handles: gtWindow

  public:
    virtual void InitSub (int _x, int _y, int _orient, int _size);
    virtual bool UpdateSurface ();

  protected:
    CResource *rcState;
    int orient, size;
};


#define WIN_DEPTH 2     // Thickness of outer walls in size units


static inline int WinRoomDepth (int size) {
  // Space required for the icons inside the room.
  // These values must match the icon files ("fp-win<size>*").
  switch (size) {
    case  4: return 2;
    case  6: return 3;
    case  8:
    case 12: return 5;
  }
  ASSERT (false);
}


void CGadgetWindow::InitSub (int _x, int _y, int _orient, int _size) {
  int roomDepth;

  orient = _orient;
  size = _size;

  // Area...
  roomDepth = WinRoomDepth (size);
  switch (_orient & 3) {
    case 0: // North
    case 2: // South
      baseArea = Rect (_x - _size/2, _y - WIN_DEPTH/2, _size, WIN_DEPTH + roomDepth);
      if ((_orient & 3) == 2) baseArea.y -= roomDepth;
      break;
    case 1: // East
    case 3: // West
      baseArea = Rect (_x - WIN_DEPTH/2, _y - _size/2, WIN_DEPTH + roomDepth, _size);
      if ((_orient & 3) == 1) baseArea.x -= roomDepth;
      break;
  }

  // Other static properties ...
  visibilityLevel = fvlMini;

  // Resources...
  rcState = GetGadgetResource (this, "state");
  RegisterResource (rcState);
  RegisterResource (floorplan->UseStateRc ());
  RegisterResource (floorplan->WeatherRc ());
}


bool CGadgetWindow::UpdateSurface () {
  CRcValueState vs;
  ERctWindowState state;
  char buf[16];
  TColor color;
  int scale, surfOrient;

  // Get state...
  rcState->GetValueState (&vs);
  state = ReadValidWindowState (&vs);
  if (state == rcvWindowOpenOrTilted) state = rcvWindowOpen;

  // Determine color...
  if (viewLevel == fvlMini) {
    if (state == rcvWindowClosed) color = GREY;
    else color = WHITE;
  }
  else {
    if (state == rcvWindowClosed) color = WHITE;
    else color = YELLOW;
  }

  // Get and scale icon...
  surfOrient = (state == rcvWindowOpen) ? orient : (orient & 3);
  snprintf (buf, sizeof (buf), "fp-win%02i%c", size, "ctll" [state]);
  scale = floorplan->GetViewScale (viewLevel);
  surf = IconGet (buf, color, (viewLevel == fvlMini) ? TRANSPARENT : BLACK, 1 << (ICON_SCALE - scale), surfOrient, true);
    // Usually, all gadget surfaces must have a transparent background.
    // The only exception is made here for windows in the normal (full-screen) views,
    // since the window surface must overwrite the building wall.

  // Set highlight status...
  surfEmph = geNone;
  if (!vs.IsKnown ()) surfEmph = geError;
  else if (state != rcvWindowClosed) {
    if (floorplan->GetValidUseState () >= ((state == rcvWindowTilted) ? rcvUseAway : rcvUseNight)) surfEmph = geAttention;
    if (floorplan->GetValidWeather ()) surfEmph = (state == rcvWindowTilted) ? geAttention : geAlert;
  }

  // Done...
  return true;
}





// ********** CGadgetShades **********


#define SHADES_THICKNESS 2


class CGadgetShades: public CGadget {
  // Handles: gtShades

  public:
    virtual ~CGadgetShades () { SurfaceFree (&surf); }

    virtual void InitSub (int _x, int _y, int _orient, int _size);
    virtual bool UpdateSurface ();
    virtual void OnPushed (CButton *btn, bool longPush);

  protected:
    CResource *rcShades;
    int orient;
};


void CGadgetShades::InitSub (int _x, int _y, int _orient, int _size) {
  orient = _orient;

  // Area...
  switch (_orient) {
    case 0: // North
    case 2: // South
      baseArea = Rect (_x - _size/2, _y, _size, SHADES_THICKNESS);
      if (_orient == 0) baseArea.y -= SHADES_THICKNESS;
      break;
    case 1: // East
    case 3: // West
      baseArea = Rect (_x, _y - _size/2, SHADES_THICKNESS, _size);
      if (_orient == 3) baseArea.x -= SHADES_THICKNESS;
      break;
  }

  // Other static properties ...
  visibilityLevel = fvlMini;
  pushable = true;

  // Resources...
  rcShades = GetGadgetResource (this, "shades");
  RegisterResource (rcShades);
  RegisterResource (floorplan->UseStateRc ());
  RegisterResource (floorplan->WeatherRc ());
}


bool CGadgetShades::UpdateSurface () {
  CRcValueState vs;
  SDL_Surface *_surf;
  SDL_Rect r;
  TColor color, colTransition;
  float shades;
  int ratioInt, ratioFrac, thickness;

  // Read 'rcShades' and set 'surfEmph'...
  rcShades->GetValueState (&vs);
  if (!vs.IsKnown ()) {
    surfEmph = geError;
    shades = 99.0;    // make almost fully visible (almost to give it the highlight color)
  }
  else {
    surfEmph = geNone;
    shades = vs.ValidFloat (0.0);
    if (shades < 0.0) shades = 0.0;
    if (shades > 100.0) shades = 100.0;
  }

  // Render surface...
  if (shades == 0.0) _surf = NULL;     // Fully open: Remove surface for efficiency reasons.
  else {
    _surf = CreateSurface (viewArea.w, viewArea.h);
    if (viewLevel == fvlMini)
      color = (shades < 100.0) ? WHITE : GREY;
    else
      color = (shades < 100.0) ? YELLOW : WHITE;    // consistent with 'CGadgetWindow'
    thickness = ((orient & 1) ? viewArea.w : viewArea.h) << 8;
    ratioFrac = shades / 100.0 * thickness;
    if (ratioFrac < 256) ratioFrac = 256;   // make almost-open shades visible clearly
    if (ratioFrac > thickness) ratioFrac = thickness;
    ratioInt = (ratioFrac >> 8);
    ratioFrac &= 0xff;
    colTransition = ColorBlend (TRANSPARENT, color, ratioFrac);
    switch (orient & 3) {
      case 0:       // north ...
        SDL_FillRect (_surf, NULL, ToUint32 (TRANSPARENT));   // clear
        r = Rect (0, viewArea.h - ratioInt, viewArea.w, ratioInt);
        SDL_FillRect (_surf, &r, ToUint32 (color));           // fill colored area
        if (ratioFrac) {
          r.y--; r.h = 1;
          SDL_FillRect (_surf, &r, ToUint32 (colTransition)); // fill transition line
        }
        r.w = viewArea.w;
        break;
      case 1:       // east ...
        SDL_FillRect (_surf, NULL, ToUint32 (TRANSPARENT));   // clear
        r = Rect (0, 0, ratioInt, viewArea.h);
        SDL_FillRect (_surf, &r, ToUint32 (color));           // fill colored area
        if (ratioFrac) {
          r.x = ratioInt; r.w = 1;
          SDL_FillRect (_surf, &r, ToUint32 (colTransition)); // fill transition line
        }
        r.w = viewArea.w;
        break;
      case 2:       // south ...
        SDL_FillRect (_surf, NULL, ToUint32 (TRANSPARENT));   // clear
        r = Rect (0, 0, viewArea.w, ratioInt);
        SDL_FillRect (_surf, &r, ToUint32 (color));           // fill colored area
        if (ratioFrac) {
          r.y = ratioInt; r.h = 1;
          SDL_FillRect (_surf, &r, ToUint32 (colTransition)); // fill transition line
        }
        r.w = viewArea.w;
        break;
      case 3:       // west ...
        SDL_FillRect (_surf, NULL, ToUint32 (TRANSPARENT));   // clear
        r = Rect (viewArea.w - ratioInt, 0, ratioInt, viewArea.h);
        SDL_FillRect (_surf, &r, ToUint32 (color));           // fill colored area
        if (ratioFrac) {
          r.x--; r.w = 1;
          SDL_FillRect (_surf, &r, ToUint32 (colTransition)); // fill transition line
        }
        r.w = viewArea.w;
        break;
    } // switch
  } // if (shades == 0.0) ... else ...
  SurfaceSet (&surf, _surf);

  // Set highlight status (without geError) ...
  if (surfEmph != geError && shades > 0.0 && shades < 100.0) {
    if (floorplan->GetValidUseState () >= rcvUseVacation) surfEmph = geAttention;
    if (floorplan->GetValidWeather ()) surfEmph = geAttention;
  }

  // Done ...
  return true;
}


void CGadgetShades::OnPushed (CButton *btn, bool longPush) {
  if (longPush) HandleLongPush (rcShades);
  else RunResourceDialog (rcShades, gtShades);
}





// ********** CGadgetRoofWindow **********


class CGadgetRoofWindow: public CGadget {
  // Handles: gtRoofWindow

  public:
    virtual ~CGadgetRoofWindow () { SurfaceFree (&surfMerged); }

    virtual void InitSub (int _x, int _y, int _orient, int _size);
    virtual bool UpdateSurface ();
    virtual void OnPushed (CButton *btn, bool longPush);

    // TBD: Query parameters for an implicit text gadget showing the actuator state

  protected:
    CResource *rcState, *rcShades, *rcActuator;
    int size, orient;
    SDL_Surface *surfMerged;
};


#define RWIN_DEPTH 8            // Extent in in-out direction
#define RWIN_BORDER_CLOSED 0.5  // Border not covered by shades when closed
#define RWIN_BORDER_OPEN   1    // Border not covered by shades when open


void CGadgetRoofWindow::InitSub (int _x, int _y, int _orient, int _size) {
  orient = _orient;
  size = _size;

  // Area...
  if ((_orient & 1) == 0) {  // north or south
    baseArea = Rect (_x - _size/2, _y - RWIN_DEPTH/2, _size, RWIN_DEPTH);
  }
  else {                     // west or east
    baseArea = Rect (_x - RWIN_DEPTH/2, _y - _size/2, RWIN_DEPTH, _size);
  }

  // Other static properties ...
  visibilityLevel = fvlMini;
  pushable = true;

  // Resources...
  rcState = GetGadgetResource (this, "state");
  rcShades = EnvGetBool (GetGadgetEnvKey (this, "shades"), envFloorplanRwinShades) ?
              GetGadgetResource (this, "shades") : NULL;
  rcActuator = EnvGetBool (GetGadgetEnvKey (this, "actuator"), envFloorplanRwinActuator) ?
                GetGadgetResource (this, "actuator") : NULL;
  RegisterResource (rcState);
  RegisterResource (rcShades);
  RegisterResource (rcActuator);
  RegisterResource (floorplan->UseStateRc ());
  RegisterResource (floorplan->WeatherRc ());
}


bool CGadgetRoofWindow::UpdateSurface () {
  CRcValueState vs;
  SDL_Surface *surfShadesUp, *surfShadesDown;
  float shades;
  bool stateOpen;
  char buf[16];
  SDL_Rect r;
  TColor color;
  ERctUseState useState;
  int scale, pos0, posD, borderX2;

  surfEmph = geNone;

  // Read resources...
  rcState->GetValueState (&vs);
  if (!vs.IsKnown ()) {
    surfEmph = geError;
    stateOpen = true;
  }
  else
    stateOpen = (ReadValidWindowState (&vs) == rcvWindowClosed) ? false : true;

  if (rcShades) {
    rcShades->GetValueState (&vs);
    if (!vs.IsKnown ()) {
      surfEmph = geError;
      shades = 50.0;    // unknown position
    }
    else {
      shades = vs.ValidFloat (0.0);
      if (shades < 0.0) shades = 0.0;
      if (shades > 100.0) shades = 100.0;
    }
  }
  else shades = 0.0;

  if (rcActuator) {
    // If an actuator is present, the only thing we do here is to set 'stateOpen' if
    // its state is known and according to the actuator, the window is not fully closed.
    rcActuator->GetValueState (&vs);
    if (!vs.IsKnown ()) surfEmph = geError;
    else {
      if (vs.ValidFloat (0.0) != 0.0) stateOpen = true;
    }
  }

  // Determine color...
  if (viewLevel == fvlMini)
    color = (stateOpen || (shades > 0.0 && shades < 100.0)) ? WHITE : GREY;
  else
    color = (stateOpen || (shades > 0.0 && shades < 100.0)) ? YELLOW : WHITE;    // consistent with 'CGadgetWindow'

  // Get icon(s)...
  surfShadesUp = surfShadesDown = surf = NULL;
  SurfaceFree (&surfMerged);

  scale = floorplan->GetViewScale (viewLevel);

  snprintf (buf, sizeof (buf) - 1, "fp-rwin%02i%c", size, stateOpen ? 'o' : 'c');
  if (shades < 100.0)
    surfShadesUp = IconGet (buf, color, TRANSPARENT, 1 << (ICON_SCALE - scale), orient, true);
  if (shades > 0.0) {
    strcat (buf, "s");
    surfShadesDown = IconGet (buf, color, TRANSPARENT, 1 << (ICON_SCALE - scale), orient, true);
  }

  // Draw (potentially merged) result icon...
  if (shades == 0.0) surf = surfShadesUp;
  else if (shades == 100.0) surf = surfShadesDown;
  else {
    ASSERT (surfShadesUp != NULL && surfShadesDown != NULL && surfMerged == NULL);
    ASSERT (surfShadesUp->w == surfShadesDown->w && surfShadesUp->h == surfShadesDown->h);

    borderX2 = stateOpen ? (int) (2 * RWIN_BORDER_OPEN) : (int) (2 * RWIN_BORDER_CLOSED);
    pos0 = (borderX2 << scale) >> 1;
    posD = (int) round (shades / 100.0 * ((RWIN_DEPTH - borderX2) << scale));

    surfMerged = SurfaceDup (surfShadesUp);
    if (posD > 0) {   // sanity
      r = Rect (surfMerged);
      switch (orient) {
        case 0: // North
          r.y = r.h - pos0 - posD;
          r.h = posD;
          break;
        case 2: // South
          r.y = pos0;
          r.h = posD;
          break;
        case 3: // West
          r.x = r.w - pos0 - posD;
          r.w = posD;
          break;
        case 1: // East
          r.x = pos0;
          r.w = posD;
          break;
        default:
          ASSERT (false);
      }
      SurfaceBlit (surfShadesDown, &r, surfMerged, &r);
      surf = surfMerged;
    }
  }

  // Set highlight status (without geError) ...
  if (surfEmph != geError) {
    useState = floorplan->GetValidUseState ();
    if (useState >= rcvUseNight && stateOpen) surfEmph = geAttention;
    if (useState >= rcvUseVacation && shades > 0.0 && shades < 100.0) surfEmph = geAttention;
    if (floorplan->GetValidWeather ()) {
      if (shades > 0.0) surfEmph = geAttention;
      if (stateOpen) surfEmph = geAlert;
    }
  }

  // Done...
  return true;
}


void CGadgetRoofWindow::OnPushed (CButton *btn, bool longPush) {
  if (longPush) {   // long push ...
    if (rcActuator) {
      if (rcShades) RunResourceDialog (rcActuator, gtRoofWindow);
        // have both shades and actuator: run actuator dialog
      else HandleLongPush (rcActuator);
        // have actuator only: auto-set actuator (dialog would appear on simple push)
    }
    else {
      if (rcShades) HandleLongPush (rcShades);
        // have shades only: auto-set shades
    }
  }
  else {            // simple push ...
    if (rcShades) RunResourceDialog (rcShades, gtShades);
    else if (rcActuator) RunResourceDialog (rcActuator, gtRoofWindow);
  }
}





// ********** CGadgetGarage **********


class CGadgetGarage: public CGadget {
  // Handles: gtGarage

  public:
    virtual void InitSub (int _x, int _y, int _orient, int _size);
    virtual bool UpdateSurface ();

  protected:
    CResource *rcState;
    int orient;
};


#define GARAGE_WIDTH 16
#define GARAGE_DEPTH 4          // (maximum) wall thickness
#define GARAGE_ROOM_DEPTH 8     // additional depth if garage is open


void CGadgetGarage::InitSub (int _x, int _y, int _orient, int _size) {
  orient = _orient;

  // Area...
  switch (_orient) {
    case 0:   // North
      baseArea = Rect (_x - GARAGE_WIDTH/2, _y - GARAGE_DEPTH/2, GARAGE_WIDTH, GARAGE_DEPTH + GARAGE_ROOM_DEPTH);
      break;
    case 2:   // South
      baseArea = Rect (_x - GARAGE_WIDTH/2, _y - GARAGE_DEPTH/2 - GARAGE_ROOM_DEPTH, GARAGE_WIDTH, GARAGE_DEPTH + GARAGE_ROOM_DEPTH);
      break;
    case 3:   // West
      baseArea = Rect (_x - GARAGE_DEPTH/2, _y - GARAGE_WIDTH/2, GARAGE_DEPTH + GARAGE_ROOM_DEPTH, GARAGE_WIDTH);
      break;
    case 1:   // East
      baseArea = Rect (_x - GARAGE_DEPTH/2 - GARAGE_ROOM_DEPTH, _y - GARAGE_WIDTH/2, GARAGE_DEPTH + GARAGE_ROOM_DEPTH, GARAGE_WIDTH);
      break;
    default:
      ASSERT (false);
  }

  // Static properties ...
  visibilityLevel = fvlMini;

  // Resources ...
  rcState = GetGadgetResource (this, "state");
  RegisterResource (rcState);
  RegisterResource (floorplan->UseStateRc ());
  RegisterResource (floorplan->WeatherRc ());
}


bool CGadgetGarage::UpdateSurface () {
  CRcValueState vs;
  bool stateOpen;
  TColor color;

  surfEmph = geNone;

  // Read resource ...
  rcState->GetValueState (&vs);
  if (!vs.IsKnown ()) {
    surfEmph = geError;
    stateOpen = true;
  }
  else
    stateOpen = (ReadValidWindowState (&vs) == rcvWindowClosed) ? false : true;

  // Determine color...
  if (viewLevel == fvlMini)
    color = stateOpen ? WHITE : GREY;
  else
    color = stateOpen ? YELLOW : WHITE;    // consistent with 'CGadgetWindow' (open garage is no worse than tilted window)

  // Set surface...
  surf = IconGet (stateOpen ? "fp-garageo" : "fp-garagec",
                  color, (viewLevel == fvlMini) ? TRANSPARENT : BLACK,
                  1 << (ICON_SCALE - floorplan->GetViewScale (viewLevel)), orient, true);
    // Usually, all gadget surfaces must have a transparent background.
    // The only exception is made here for windows in the normal (full-screen) views,
    // since the window surface must overwrite the building wall.

  // Set highlight status ...
  if (stateOpen) {
    if (floorplan->GetValidUseState () >= rcvUseNight) surfEmph = geAttention;
    if (floorplan->GetValidWeather ()) surfEmph = geAlert;
  }
  if (!vs.IsKnown ()) surfEmph = geError;

  // Done...
  return true;
}





// ********** CGadgetIcon **********


class CGadgetIcon: public CGadget {
  // Handles: gtLock, gtMotion, gtPhone, gtMusic, gtWlan, gtBluetooth, gtService

  public:
    virtual void InitSub (int _x, int _y, int _orient, int _size);
    virtual bool UpdateSurface ();
    virtual void OnPushed (CButton *btn, bool longPush);

  protected:
    CResource *rcState;
    TTicksMonotonic tLastMotion;
};


void CGadgetIcon::InitSub (int _x, int _y, int _orient, int _size) {
  int iconSize = (6 << _size);

  // Area...
  baseArea = Rect (_x - iconSize / 2, _y - iconSize / 2, iconSize, iconSize);

  // Other static properties ...
  visibilityLevel = fvlMini; // (_size >= 1) ? fvlMini : fvlFull;

  // Resources ...
  rcState = GetGadgetResource (this, "state");
  RegisterResource (rcState);

  // Type-specifics...
  switch (gdtType) {

    case gtLock:
      RegisterResource (floorplan->UseStateRc ());
      break;

    case gtMotion:
      tLastMotion = NEVER;
      RegisterResource (floorplan->TimerRc ());
        // Needed to automatically turn off motion icon after some time.
      break;

    case gtPhone:
      pushable = true;
      break;

    case gtLight:
    case gtMusic:
    case gtWlan:
    case gtBluetooth:
    case gtService:
      RegisterResource (floorplan->UseStateRc ());
      pushable = true;
      break;

    case gtMail:
      RegisterResource (floorplan->UseStateRc ());
      break;
    default:
      break;
  };
}


bool CGadgetIcon::UpdateSurface () {
  char buf[256];
  const char *iconBaseName;
  CRcValueState vs;
  ERctPhoneState phoneState;
  TColor iconColor;
  bool changePossible, locked, motion;

  // Set defaults...
  changePossible = true;
  iconBaseName = gdtTypeInfo [gdtType].icon;
  iconColor = (viewLevel == fvlMini) ? GREY : WHITE;
  surfEmph = geNone;

  // Type-specific appearances...
  switch (gdtType) {

    case gtLock:
      rcState->GetValueState (&vs);
      locked = vs.ValidBool (false);
      iconBaseName = locked ? "padlock" : "padlock_open";
      iconColor = locked ? GREY : WHITE;
      if (floorplan->GetValidUseState () >= rcvUseNight && !locked)
        surfEmph = geAttention;
      if (!vs.IsKnown ()) surfEmph = geError;
      break;

    case gtMotion:
      iconColor = WHITE;
      rcState->GetValueState (&vs);
      motion = vs.ValidBool (false);
      if (!motion) {
        if (TicksMonotonicIsNever (tLastMotion)) {
          iconBaseName = NULL;
          changePossible = false;   // give hint that no redrawing is necessary
        }
        else {
          if (TicksMonotonicNow () > tLastMotion + TICKS_FROM_SECONDS (envFloorplanMotionRetention)) {
            iconBaseName = NULL;
            tLastMotion = NEVER;
          }
        }
      }
      else {
        tLastMotion = TicksMonotonicNow ();
        surfEmph = geAttention;
      }
      if (!vs.IsKnown ()) surfEmph = geError;
      break;

    case gtPhone:
      rcState->GetValueState (&vs);
      phoneState = rcvPhoneIdle;
      if (vs.Type () == rctBool) phoneState = vs.ValidBool (false) ? rcvPhoneInCall : rcvPhoneIdle;
      else if (vs.Type () == rctPhoneState) phoneState = (ERctPhoneState) vs.ValidUnitInt (rctPhoneState);
      else vs.Clear (rctPhoneState);

      if (!vs.IsKnown ()) surfEmph = geError;
      else switch (phoneState) {
        case rcvPhoneRinging:
          surfEmph = geAttention;
          // fall through: icon color is the same as in call...
        case rcvPhoneInCall:
          iconColor = (viewLevel == fvlMini) ? WHITE : YELLOW;
          break;
        default:
          break;
      }
      break;

    case gtMusic:
      // TBD
      break;

    case gtLight:
    case gtWlan:
    case gtBluetooth:
    case gtService:
      rcState->GetValueState (&vs);
      if (vs.IsBusy ()) iconColor = LIGHT_RED;
      else {
        if (vs.ValidBool (false))
          iconColor = (viewLevel == fvlMini) ? WHITE : YELLOW;
      }
      if (!vs.IsKnown ()) surfEmph = geError;
      else if (vs.ValidBool (false) == true && gdtType != gtLight) {
        if (floorplan->GetValidUseState () >=
              (gdtType == gtWlan ? rcvUseVacation : rcvUseNight)
            )
          surfEmph = geAttention;
      }
      break;

    case gtMail:
      iconColor = WHITE;
      rcState->GetValueState (&vs);
      if (vs.IsKnown ()) {
        if (vs.ValidBool (false)) {     // there is new mail ...
          if (floorplan->GetValidUseState () >= rcvUseVacation) surfEmph = geAttention;
        }
        else iconBaseName = NULL;       // no new mail
      }
      else surfEmph = geError;          // value is unknown
      break;

    default:
      ASSERT (false);
  };

  // Set the surface...
  if (!iconBaseName || viewArea.w < 12) surf = NULL;      // icon not visible
  else {
    if (viewLevel != fvlMini && baseArea.w > 6)
      iconColor = ColorScale (iconColor, 0x100 * 6 / baseArea.w);
    if (viewArea.w >= 48) {
      snprintf (buf, sizeof (buf),  "ic-%s-%02i", iconBaseName, viewArea.w);
      surf = IconGet (buf, iconColor);
    }
    else {
      snprintf (buf, sizeof (buf),  "ic-%s-48", iconBaseName);
      surf = IconGet (buf, iconColor, TRANSPARENT, 48 / viewArea.w);
    }
  }

  // Done...
  return changePossible;
}


void CGadgetIcon::OnPushed (CButton *btn, bool longPush) {
  char buf[64];
  const char *p, *phoneUrl;

  switch (gdtType) {
    case gtPhone:
      // Try to get number from environment...
      phoneUrl = EnvGet (GetGadgetEnvKey (this, "dial"));
      // Try to derive number from ID...
      if (!phoneUrl) {
        p = gdtId.Get () + gdtId.Len ();
        while (p > gdtId.Get () && p[-1] >= '0' && p[-1] <= '9') p--;
        if (*p) {
          snprintf (buf, sizeof (buf), "*%s", p);
          phoneUrl = buf;
        }
      }
      // Dial the number (or warn on failure)...
      if (!phoneUrl)
        WARNINGF (("Unable to determine the number to dial for gadget '%s'", gdtId.Get ()));
      else
        AppPhoneDial (phoneUrl, floorplan->Screen ());
      break;

    case gtMusic:
      // TBD: Activate music player and connect to this MPD
      break;

    case gtBluetooth:
      // TBD: Switch off bluetooth (request 'false' for 1 second?)
      break;

    case gtLight:
    case gtWlan:
    case gtService:
      if (longPush) HandleLongPush (rcState);
      else RunResourceDialog (rcState, gdtType);
      break;

    default:
      ASSERT (false);
  };
}





// ********** CGadgetText **********


class CGadgetText: public CGadget {
  // Handles: gtTemp, supplemental information

  public:
    virtual ~CGadgetText () { SurfaceFree (&surf); }

    virtual void InitSub (int _x, int _y, int _orient, int _size);
    virtual bool UpdateSurface ();

    void SetHideIfZero (bool _hideIfZero) { hideIfZero = _hideIfZero; }

  protected:
    int size;
    CResource *rcData;
    bool hideIfZero;
};


#define TEXT_WIDTH 16
#define TEXT_HEIGHT 6
#define TEXT_FONT FontGet (fntNormal, 20)


static void TextFormatData (char *buf, int bufSize, CRcValueState *vs) {
  // TBD: Merge with app_home.C:ScreenHomeFormatData()?
  switch (RcTypeGetBaseType (vs->Type ())) {
    case rctFloat:
      sprintf (buf, "%.1f%s", vs->GenericFloat (), RcTypeGetUnit (vs->Type ()));
      LangTranslateNumber (buf);
      break;
    default:
      strncpy (buf, vs->ToStr (), bufSize - 1); buf[bufSize - 1] = '\0';
  }
}


void CGadgetText::InitSub (int _x, int _y, int _orient, int _size) {
  int w, h;

  size = _size;
  hideIfZero = false;

  // Area ...
  if (_size >= 0) {
    w = TEXT_WIDTH << _size;
    h = TEXT_HEIGHT << _size;
  }
  else {
    w = TEXT_WIDTH >> -_size;
    h = TEXT_HEIGHT >> -_size;
  }
  baseArea = Rect (_x - (w >> 1), _y - (h >> 1), w, h);

  // Resources ...
  rcData = GetGadgetResource (this, "data");
  RegisterResource (rcData);
}


bool CGadgetText::UpdateSurface () {
  CRcValueState vs;
  char buf[64];
  SDL_Surface *surfText;
  TColor color;
  int scale;

  // Clear surface...
  SurfaceFree (&surf);
  surfEmph = geNone;

  // Read resource...
  rcData->GetValueState (&vs);

  // Check if unknown ...
  if (!vs.IsKnown ()) {
    surfEmph = geError;
    return true;
  }

  // Check if zero and thus to hide ...
  if (hideIfZero) {
    if (vs.ValidInt (-1) == 0) return true;
    if (vs.ValidFloat (-1.0) == 0.0) return true;
  }

  // Get color / handle busy state ...
  if (vs.State () == rcsBusy) {
    color = LIGHT_RED;
    vs.SetState (rcsValid);   // to not print the "busy" character
  }
  else
    color = WHITE;

  // Draw surface...
  surf = CreateSurface (viewArea.w, viewArea.h);
  SurfaceFill (surf, TRANSPARENT);
  TextFormatData (buf, sizeof (buf), &vs);
  scale = floorplan->GetViewScale (viewLevel) + size;
  if (scale >= 0) {   // sanity, smaller text is probably unreadable anyway
    surfText = FontRenderText (FontGet (fntNormal, 5 << scale), buf, color);
    SurfaceBlit (surfText, NULL, surf, NULL, 0, 0);
    SurfaceFree (&surfText);
  }

  // Done...
  return true;
}






// *************************** CFloorplan **************************************


struct SResourceAndGadget {
  CResource *rc;
  int gdtIdx;
};


void CFloorplan::Init () {
  int n;

  rcUseState = rcWeather = NULL;

  gadgets = 0;
  gadgetList = NULL;

  viewLevels = 0;
  viewLevel = fvlNone;
  screen = NULL;
  for (n = 0; n < FP_MAX_VIEWS; n++) buildingSurfList[n] = NULL;

  rcGdtEntries = 0;
  rcGdtList = NULL;
  changedGadgets = 0;
  changedGadgetsIdxList = NULL;

  emphGadgets = emphGadgetsBlinking = 0;
  emphGadgetsIdxList = NULL;
  emphChanged = false;
  emphSurf = NULL;
}


void CFloorplan::Done () {
  int n;

  subscr.Clear ();

  if (gadgetList) {
    for (n = 0; n < gadgets; n++) delete gadgetList[n];
    delete gadgetList;
    gadgetList = NULL;
  }

  for (n = 0; n < FP_MAX_VIEWS; n++) SurfaceFree (&buildingSurfList[n]);

  FREEA (rcGdtList);
  if (changedGadgetsIdxList) delete changedGadgetsIdxList;
  FREEA (emphGadgetsIdxList);

  SurfaceFree (&emphSurf);
}


static int CompareSResourceAndGadget (const void *_a, const void *_b) {
  SResourceAndGadget *a = (SResourceAndGadget *) _a;
  SResourceAndGadget *b = (SResourceAndGadget *) _b;

  if (a->rc < b->rc) return -1;
  if (a->rc > b->rc) return +1;
  return 0;
}


bool CFloorplan::Setup (const char *_lid) {
  DIR *dir;
  CDictFast<CString> map;
  CGadget *gdt;
  CString s, *val;
  char gdtTypeName[64];
  const char *gdtId, *gdtDef;
  EGadgetType gdtType = gtNone;
  int n, idx, x, y, orient, size;
  bool ok;

  // General / sanity ...
  viewLevels = 2;   // "mini" and "full"
  viewLevel = fvlNone;
  lid.Set (_lid);
  fpoName.Set (EnvGetHome2lEtcPath (&s, StringF ("%s.fpo", _lid)));

  // Read FPO if available or return 'false' on error ...
  dir = opendir (fpoName.Get ());
  if (dir) closedir (dir);
  else {
    WARNINGF (("Cannot find floorplan object '%s'", fpoName.Get ()));
    return false;
  }

  buildingSurfList[fvlMini] = SurfaceReadBmp (StringF (&s, "%s/mini.bmp", fpoName.Get ()));
  SurfaceMakeTransparentMono (buildingSurfList[fvlMini], COL_APP_LABEL.r);  // 0x68 = 40.7% ~= 60%*70%
  buildingSurfList[fvlFull] = SurfaceReadBmp (StringF (&s, "%s/full.bmp", fpoName.Get ()));
  //~ SurfaceMakeTransparentMono (buildingSurfList[fvlFull]);

  // Read map file ...
  EnvReadIniFile (StringF (&s, "%s/map.conf", fpoName.Get ()), &map);
  gdtId = ".scale";
  val = map.Get (gdtId);
  preScale = 0;
  if (val) {
    if (!IntFromString (val->Get (), &preScale))
      ERRORF (("Syntax error in %s/map.conf: '%s = %s'", fpoName.Get (), gdtId, val->Get ()));
    map.Del (gdtId);
  }

  // Prepare common resources...
  if (envFloorplanUseState) rcUseState = RcGet (envFloorplanUseState);
  if (envFloorplanWeather) rcWeather = RcGet (envFloorplanWeather);

  // Init data structures...
  gadgets = map.Entries ();
  gadgetList = new CGadget * [gadgets];

  rcGdtEntries = changedGadgets = 0;
  rcGdtList = new SResourceAndGadget [gadgets * FP_MAX_GADGET_RESOURCES];
  changedGadgetsIdxList = new int [gadgets];

  SETP(emphGadgetsIdxList, MALLOC(int, gadgets));
  emphGadgets = emphGadgetsBlinking = 0;
  emphChanged = false;

  for (idx = 0; idx < gadgets; idx++) {
    gdtId = map.GetKey (idx);
    gdtDef = map.Get (idx)->Get ();
    ok = (sscanf (gdtDef, "%63[^:]:%d:%d:%d:%d", gdtTypeName, &x, &y, &orient, &size) == 5);
    //~ INFOF (("### map.conf:%i/%i: %s = %s", idx, gadgets, gdtId, gdtDef));
    if (ok) {
      // Lookup gadget type...
      ok = false;
      for (n = 1; n < gtEND; n++)
        if (strcmp (gdtTypeName, gdtTypeInfo[n].name) == 0) {
          gdtType = (EGadgetType) n;
          ok = true;
          break;
        }
    }
    if (ok) {
      // Create appropriate object...
      switch (gdtType) {
        case gtWindow:      gdt = new CGadgetWindow ();       break;
        case gtShades:      gdt = new CGadgetShades ();       break;
        case gtRoofWindow:  gdt = new CGadgetRoofWindow ();   break;
        case gtGarage:      gdt = new CGadgetGarage ();       break;

        case gtTemp:        gdt = new CGadgetText ();         break;

        default:            ASSERT (GadgetTypeIsIcon (gdtType));
                            gdt = new CGadgetIcon ();
      }
      // Store and initialize gadget...
      gadgetList[idx] = gdt;
      registeringGadget = idx;    // for 'RegisterResource()'
      gdt->InitBase (this, gdtId, gdtType);
      //~ INFOF(("### Init gadget #%i / %s", idx, gdtId));
      gdt->InitSub (x, y, orient, size);
    }
    else ERRORF (("Syntax error in %s/map.conf: '%s = %s'", fpoName.Get (), gdtId, gdtDef));
  }

  // Sort the resource-gadget map...
  qsort (rcGdtList, rcGdtEntries, sizeof (SResourceAndGadget), CompareSResourceAndGadget);
  //~ INFOF (("### rc/gadget mapping list (%i entries):", rcGdtEntries));
  //~ for (n = 0; n < rcGdtEntries; n++)
    //~ INFOF (("###   %3i: %s: #%i / %s", n, rcGdtList[n].rc->Uri (), rcGdtList[n].gdtIdx, gadgetList[rcGdtList[n].gdtIdx]->Id ()));

  // Prepare subscriber ...
  subscr.Register ("floorplan");

  // Done: Report success...
  return true;
}


void CFloorplan::RegisterResource (CGadget *gdt, CResource *rc) {

  // Sanity...
  if (!rc) return;
  ASSERT (rcGdtEntries < gadgets * FP_MAX_GADGET_RESOURCES);

  // Add the resource...
  //~ INFOF(("###   %3i. Registering resource '%s' for gadget #%i / '%s'", rcGdtEntries, rc->Uri (), registeringGadget, gdt->Id ()));
  rcGdtList[rcGdtEntries].gdtIdx = registeringGadget; // gdtIdx;
  rcGdtList[rcGdtEntries].rc = rc;
  rcGdtEntries++;
}


ERctUseState CFloorplan::GetValidUseState () {
  CRcValueState vs;

  if (!rcUseState) return rcvUseDay;      // No use state defined => Let 'day' be the default
  rcUseState->GetValueState (&vs);
  if (vs.Type () == rctBool)              // Resource is boolean indicating day time ...
    return vs.ValidBool (false) ? rcvUseDay : rcvUseNight;
  return (ERctUseState) rcUseState->ValidEnumIdx (rctUseState, rcvUseNight);
}


bool CFloorplan::GetValidWeather () {
  if (!rcWeather) return false;         // No weather resource => Do not warn
  return rcWeather->ValidBool (true);   // Weather resource defined, but not available => Warn
}


void CFloorplan::SetViewGeometry (EFloorplanViewLevel level, int _scale, int _x0, int _y0) {
  scale[level] = _scale + preScale;
  x0[level] = _x0;
  y0[level] = _y0;
}


void CFloorplan::SetView (EFloorplanViewLevel _viewLevel, CScreen *_screen) {
  CResource *rc;
  CGadget *gdt;
  int n, idx0, idx1;
  bool wasVisible, isVisible;

  //~ INFOF(("### CFloorplan::SetView (%i -> %i)", viewLevel, _viewLevel));
  screen = _screen;
  if (_viewLevel != viewLevel) {

    // Update subscriptions...
    if (_viewLevel == fvlNone) {
      subscr.Clear ();
      emphGadgets = emphGadgetsBlinking = 0;
      SurfaceFree (&emphSurf);
    }
    else {

      // Walk through the resources to check which of them
      // need to be subscribed to and which may be unsubscribed...
      idx0 = 0;
      while (idx0 < rcGdtEntries) {

        // Get the minimum possible visibility level for the current resource...
        rc = rcGdtList[idx0].rc;
        wasVisible = isVisible = false;
        idx1 = idx0;
        while (idx1 < rcGdtEntries && rcGdtList[idx1].rc == rc) {
          gdt = gadgetList[rcGdtList[idx1].gdtIdx];
          wasVisible = wasVisible || gdt->IsVisible (viewLevel);
          isVisible = isVisible || gdt->IsVisible (_viewLevel);
          idx1++;
        }
        //~ INFOF (("###   rc = %s, idx = %i..%i, visible = %i->%i", rc->Uri (), idx0, idx1, (int) wasVisible, (int) isVisible));

        // (Un)subscribe if applicable...
        if (wasVisible && !isVisible) subscr.DelResource (rc);
        if (!wasVisible && isVisible) subscr.AddResource (rc);

        // Progress pointer...
        idx0 = idx1;
      }
    }
    subscr.FlushEvents ();  // We will update gadgets manually => can discard presently pending events.

    // Update gadgets ...
    emphGadgets = emphGadgetsBlinking = 0;
    emphBlinkOn = true;
    emphBlinkT = NEVER;
    for (n = 0; n < gadgets; n++) {
      gdt = gadgetList[n];
      if (gdt->IsVisible (_viewLevel)) {
        gdt->SetView (_viewLevel);
        if (gdt->SurfaceEmph () != geNone) {
          emphGadgetsIdxList[emphGadgets++] = n;
          if (gdt->SurfaceEmph () == geAlert) emphGadgetsBlinking++;
        }
      }
    }
    if (!emphSurf) emphSurf = CreateSurface (FP_WIDTH, FP_HEIGHT);
      // Note: For efficiency reasons (to keep the widget fast), the emphasis
      //       has the same resolution as the widget, independent of 'preScale'!
    emphChanged = true;

    // Done...
    viewLevel = _viewLevel;
  }
}


void CFloorplan::Iterate () {
  CGadget *gdt;
  CResource *rc;
  CRcEvent ev;
  TTicksMonotonic now;
  int n, k, idx, step, gdtIdx;

  // Poll subscriber events and mark all affected gadgets...
  changedGadgets = 0;
  while (subscr.PollEvent (&ev)) {
    //~ INFOF (("CFloorplan::Iterate (): Event = %s", ev.ToStr ()));
    if (ev.Type () == rceValueStateChanged) {
      rc = ev.Resource ();
      //~ INFOF (("### Changed resource: %s", rc->Uri ()));

      // Binary search for 'rcGdtList' entry block...
      idx = 0;
      step = rcGdtEntries / 2;
      while (step > 0) {
        if (rcGdtList[idx + step].rc < rc) idx += step;
        step /= 2;
      }
      while (idx < rcGdtEntries && rcGdtList[idx].rc != rc) idx++;
      while (idx < rcGdtEntries && rcGdtList[idx].rc == rc) {
        gdtIdx = rcGdtList[idx].gdtIdx;
        idx++;
        if (gadgetList[gdtIdx]->IsVisible (viewLevel)) {
          // Check, if gadget is already in the "change gadgets" list...
          n = 0;
          while (n < changedGadgets && changedGadgetsIdxList[n] != gdtIdx) n++;
          if (n >= changedGadgets)  // No...
            changedGadgetsIdxList[changedGadgets++] = gdtIdx;
        }
      }
    }
  }

  // Update the gadgets...
  for (n = 0; n < changedGadgets; n++) {
    //~ INFOF (("### Changed gadget: #%i = %s", changedGadgetsIdxList[n], gadgetList[changedGadgetsIdxList[n]]->Id ()));
    idx = changedGadgetsIdxList[n];
    gdt = gadgetList[idx];
    if (!gdt->UpdateSurface ()) {

      // Gadget reported no change: Remove the entry from the "changed" list ...
      changedGadgetsIdxList[n] = changedGadgetsIdxList[changedGadgets-1];   // move last from list to current place
      changedGadgets--;   // decrement list size
      n--;  // decrement n to let this loop continue with the swapped-in element
    }
    else {

      // Update the emph list ...
      if (gdt->SurfaceEmph () == geNone) {
        // No emphasis now: Check if there was emphasis before...
        for (k = 0; k < emphGadgets; k++)
          if (emphGadgetsIdxList[k] == idx) {
            emphGadgets--;
            emphGadgetsIdxList[k] = emphGadgetsIdxList[emphGadgets];
            emphChanged = true;
            break;
          }
      }
      else {
        // Emphasis now: Store it (but avoid duplicates)...
        for (k = 0; k < emphGadgets; k++) if (emphGadgetsIdxList[k] == idx) break;
        if (k >= emphGadgets) emphGadgetsIdxList[emphGadgets++] = idx;
        emphChanged = true;     // we redraw in any case since the type of emphasis may have changed
      }
    }
  }

  // Handle blinking ...
  if (emphChanged) {            // Update 'emphGadgetsBlinking'...
    emphGadgetsBlinking = 0;
    for (n = 0; n < emphGadgets; n++)
      if (gadgetList[emphGadgetsIdxList[n]]->SurfaceEmph () == geAlert) { emphGadgetsBlinking++; }
    if (!emphGadgetsBlinking) emphBlinkT = NEVER;
  }
  if (emphGadgetsBlinking) {    // Check if the blinking state changed...
    now = TicksMonotonicNow ();
    if (emphBlinkT == NEVER || now > emphBlinkT) {
      emphBlinkOn = (emphBlinkT == NEVER) ? true : !emphBlinkOn;
      emphBlinkT = now + 500;
      emphChanged = true;
    }
  }
}


SDL_Surface *CFloorplan::GetEmphSurface () {
  static const TColor emphColorsMini[geEND] = { BLACK, GREY, DARK_YELLOW, LIGHT_RED };
  static const TColor emphColors[geEND] = { BLACK, ColorScale (YELLOW, 0x80), DARK_YELLOW, LIGHT_RED };
  CGadget *gdt;
  SDL_Rect r;
  int n, e;

  //~ INFOF (("### CFloorplan::GetEmphSurface (emphGadgets = %i)", emphGadgets));

  // Update the emphasis surface if necessary ...
  if (emphChanged) {
    SDL_FillRect (emphSurf, NULL, ToUint32 (TRANSPARENT));    // Clear
    for (e = geAttention; e < geEND; e++) if (e != geAlert || emphBlinkOn)
      for (n = 0; n < emphGadgets; n++) {
        gdt = gadgetList[emphGadgetsIdxList[n]];
        if (gdt->SurfaceEmph () == e) {
          r = *gdt->BaseArea ();
          r.x <<= preScale;
          r.y <<= preScale;
          r.w <<= preScale;
          r.h <<= preScale;
          RectGrow (&r, 8, 8);
          SDL_FillRect (emphSurf, &r, ToUint32 ((viewLevel == fvlMini) ? emphColorsMini[e] : emphColors[e]));
        }
      }
  }

  // Done...
  emphChanged = false;
  return (emphGadgets > 0) ? emphSurf : NULL;
}





// *************************** CWidgetFloorplan ********************************


CWidgetFloorplan::CWidgetFloorplan () {
  floorplan = NULL;
  mapSurf = NULL;
  tInterval = NEVER;
}


CWidgetFloorplan::~CWidgetFloorplan () {
  SurfaceFree (mapSurf);
}


void CWidgetFloorplan::Setup (int x0, int y0, class CFloorplan *_floorplan, TTicksMonotonic _tInterval) {
  floorplan = _floorplan ? _floorplan : fpFloorplan;
  tInterval = _tInterval;
  SetArea (Rect (x0, y0, FP_WIDTH, FP_HEIGHT));

  if (!floorplan) return;

  floorplan->SetViewGeometry (fvlMini, 0, 0, 0);
}


void CWidgetFloorplan::Activate (bool on) {
  CGadget *gdt;
  int idx;

  if (on) {

    // Sanity...
    if (!floorplan) return;

    // Update floorplan, initialize all data structures...
    floorplan->SetView (fvlMini);
    SurfaceSet (&mapSurf, SurfaceDup (floorplan->GetBuildingSurface (fvlMini)));
    ASSERT (mapSurf->w == FP_WIDTH && mapSurf->h == FP_HEIGHT);

    // Walk through all gadgets and draw map surface ...
    for (idx = 0; idx < floorplan->Gadgets (); idx++) {
      gdt = floorplan->Gadget (idx);
      if (gdt->IsVisible (fvlMini)) {
        gdt->UpdateSurface ();
        SurfaceBlit (gdt->Surface (), NULL, mapSurf, gdt->ViewArea ());
      }
    }

    // Trigger drawing ...
    ChangedSurface ();

    // Setup timer ...
    CTimer::Set (0, tInterval);
  }
  else {

    // Stop timer...
    CTimer::Clear ();
  }
}


void CWidgetFloorplan::OnTime () {
  CGadget *gdt;
  int n, idx;

  // Sanity...
  if (!floorplan) return;

  // Update surface...
  floorplan->Iterate ();
  for (n = 0; n < floorplan->ChangedGadgets (); n++) {
    idx = floorplan->ChangedGadgetIdx (n);
    gdt = floorplan->Gadget (idx);

    // Update 'mapSurf'...
    if (gdt->Surface ())
      SurfaceBlit (gdt->Surface (), NULL, mapSurf, gdt->ViewArea ());
    else
      SurfaceBlit (floorplan->GetBuildingSurface (fvlMini), gdt->ViewArea (), mapSurf, gdt->ViewArea ());
  }

  // Redraw button ...
  if (floorplan->ChangedGadgets () > 0 || floorplan->ChangedEmph ()) ChangedSurface ();

  // Activate floorplan screen on alert ...
  FloorplanCheckAlert (screen);
}


SDL_Surface *CWidgetFloorplan::GetSurface () {
  /* Note: Blitting a semi-transparent surface onto another surface, which is also semi-transparent
   *       is a bad idea in SDL2. The result will be unpredictable if the alpha values of both are
   *       close to zero.
   *
   *       For this reason (and partially for performance), we overload this method. The rendering
   *       stacks the following surfaces in this order (bottom to up):
   *
   *       1. Button backlight (down/up) - non-transparent
   *       2. Emphasis surface ('emphSurf')
   *       3. Map ('mapSurf')
   */
  SDL_Surface *emphSurf;

  if (changed) {

    // Draw the stack ...
    //   1. Button backlight ...
    SurfaceSet (&surface, CreateSurface (area.w, area.h));
    SDL_FillRect (surface, NULL, ToUint32 (isDown ? colDown : colNorm));

    // Decide if we show the emphasis map nothing at all ...
    if ((2 * floorplan->EmphGadgets () < floorplan->Gadgets () || floorplan->EmphGadgets () < 4) || floorplan->HaveAlert ()) {
      //   2. Emphasis surface ('emphSurf') ...
      emphSurf = floorplan->GetEmphSurface ();
      if (emphSurf) SurfaceBlit (emphSurf, NULL, surface, NULL, 0, 0, SDL_BLENDMODE_BLEND);
      //   3. Map ('mapSurf') ...
      //~ SDL_SetSurfaceAlphaMod (mapSurf, stampSurf ? 0x80 : 0xff);
      SurfaceBlit (mapSurf, NULL, surface, NULL, 0, 0, SDL_BLENDMODE_BLEND);
    }

    // Done ...
    changed = false;
  }
  return surface;
}


void CWidgetFloorplan::OnPushed (bool longPush) {
  FloorplanActivate ();
}





// *************************** CScreenFloorplan ********************************


BUTTON_TRAMPOLINE(CbScreenFloorplanOnButtonPushed, CScreenFloorplan, OnButtonPushed)


enum EBtnIdFloorplan {
  btnIdFpBack = 0,
  btnIdFpUseAuto,
  btnIdFpUseDay,
  btnIdFpUseNight,
  btnIdFpUseLeaving,    // aka "away"
  btnIdFpUseVacation,
  btnIdFpEND
};


static TButtonDescriptor fpButtons[btnIdFpEND] = {
  { -1, COL_FP_MAIN, "ic-back-48",      NULL, CbAppEscape, SDLK_ESCAPE },                 // btnIdFpBack
  { -1, COL_FP_MAIN, NULL,        N_("Auto"), CbScreenFloorplanOnButtonPushed, SDLK_a },  // btnIdFpUseAuto
  { -1, COL_FP_MAIN, "ic-home-48",      NULL, CbScreenFloorplanOnButtonPushed, SDLK_d },  // btnIdFpUseDay
  { -1, COL_FP_MAIN, "ic-hotel-48",     NULL, CbScreenFloorplanOnButtonPushed, SDLK_n },  // btnIdFpUseNight
  { -1, COL_FP_MAIN, "ic-walk-48",      NULL, CbScreenFloorplanOnButtonPushed, SDLK_l },  // btnIdFpUseLeaving
  { -1, COL_FP_MAIN, "ic-terrain-48",   NULL, CbScreenFloorplanOnButtonPushed, SDLK_v },  // btnIdFpUseVacation
};


CScreenFloorplan::CScreenFloorplan () {
  floorplan = NULL;
  tInterval = 0;
  wdgList = NULL;
  wdgPoolNorm = wdgPoolPushable = NULL;
  buttonBar = NULL;
}


void CScreenFloorplan::Clear () {
  floorplan = NULL;
  FREEA (wdgList);
  FREEA (wdgPoolNorm);
  FREEA (wdgPoolPushable);
  FREEA (buttonBar);
}


void CScreenFloorplan::Setup (CFloorplan *_floorplan, TTicksMonotonic _tInterval) {
  CGadget *gdt;
  CFlatButton *btn;
  SDL_Rect r;
  int idx, numNorm, idxNorm, idxPushable;

  // Store args ...
  floorplan = _floorplan ? _floorplan : fpFloorplan;
  ASSERT (floorplan != NULL);
  view = fvlFull;
  tInterval = _tInterval;

  // Clear alert ...
  haveAlert = false;
  returnScreen = NULL;

  // Create widget pools and objects ...
  //   The widgets themselves are initialized in 'Activate()'.
  pushableGadgets = 0;
  for (idx = 0; idx < floorplan->Gadgets (); idx++)
    if (floorplan->Gadget (idx)->IsPushable ()) pushableGadgets++;
  numNorm = floorplan->Gadgets () - pushableGadgets;

  SETA (wdgPoolNorm, new CWidget [numNorm]);
  SETA (wdgPoolPushable, new CFlatButton [pushableGadgets]);
  SETA (wdgList, new CWidget * [floorplan->Gadgets ()]);
  idxNorm = idxPushable = 0;
  for (idx = 0; idx < floorplan->Gadgets (); idx++) {
    gdt = floorplan->Gadget (idx);
    if (!gdt->IsPushable ()) {
      wdgList[idx] = &wdgPoolNorm[idxNorm++];
      wdgList[idx]->SetTextureBlendMode (SDL_BLENDMODE_BLEND);
    }
    else {
      btn = &wdgPoolPushable[idxPushable++];
      wdgList[idx] = btn;
      //~ btn->SetColor (COL_FP_MAIN_DARKER, FLATBUTTON_COL_DOWN);
      btn->SetCbPushed (CbGadgetOnButtonPushed, gdt);
      btn->SetTextureBlendMode (SDL_BLENDMODE_ADD);     // make pushable widgets transparent; they have black (non-transparent) background
    }
  }

  // Send geometry values to 'floorplan'...
  r = Rect (0, 0, FP_WIDTH << FULL_SCALE, FP_HEIGHT << FULL_SCALE);
  RectCenter (&r, UI_USER_RECT);
  floorplan->SetViewGeometry (fvlFull, FULL_SCALE, r.x, r.y);

  // Prepare main widgets ...
  //   This only initializes *static* properties (geometry etc. is done in Activate() ).
  wdgBuilding.SetArea (r);

  wdgEmph.SetArea (r);
  wdgEmph.SetTextureBlendMode (SDL_BLENDMODE_ADD);

  // Button bar ...
  SETA (buttonBar, CreateMainButtonBar (btnIdFpEND, fpButtons, this));
  lastUseState = lastUseStateReq = (ERctUseState) -1;
  ASSERT (btnIdFpUseDay - 1 == btnIdFpUseAuto);
  buttonBar[btnIdFpUseDay + lastUseStateReq].SetColor (COL_FP_MAIN_DARKER);
  UpdateRequest ();
}


void CScreenFloorplan::CheckAlert (CScreen *_returnScreen) {
  bool _haveAlert = floorplan->HaveAlert ();
  if (_haveAlert != haveAlert) {
    if (_haveAlert) {
      SystemActiveLock ("_floorplan", false);
      Activate ();
      returnScreen = _returnScreen;
    }
    else {
      SystemActiveUnlock ("_floorplan", false);
      if (returnScreen) returnScreen->Activate ();
    }
    haveAlert = _haveAlert;
  }
}


void CScreenFloorplan::Activate (bool on) {
  CGadget *gdt;
  SDL_Surface *surf;
  SDL_Rect r;
  int n, idx;

  CScreen::Activate (on);
  if (on) {

    // Claim floorplan...
    floorplan->SetView (view, this);

    // Prepare screen...
    DelAllWidgets ();
    for (n = 0; n < btnIdFpEND; n++) AddWidget (&buttonBar[n]);
    ASSERT (floorplan != NULL);

    // Add building image...
    surf = floorplan->GetBuildingSurface (view);
    ASSERT (surf->w == (FP_WIDTH << FULL_SCALE) && surf->h ==  (FP_HEIGHT << FULL_SCALE));
    wdgBuilding.SetSurface (surf);
    AddWidget (&wdgBuilding);

    // Add static (normal) gadgets ...
    for (idx = 0; idx < floorplan->Gadgets (); idx++) {
      gdt = floorplan->Gadget (idx);
      if (!gdt->IsPushable ()) {
        gdt->UpdateSurface ();
        wdgList[idx]->SetArea (*gdt->ViewArea ());
        wdgList[idx]->SetSurface (gdt->Surface ());
        AddWidget (wdgList [idx]);
      }
    }

    // Add pushable gadgets ...
    for (idx = 0; idx < floorplan->Gadgets (); idx++) {
      gdt = floorplan->Gadget (idx);
      if (gdt->IsPushable ()) {
        r = *gdt->ViewArea ();
        RectGrow (&r, 16, 16);
        wdgList[idx]->SetArea (r);
        gdt->UpdateSurface ();
        static_cast<CFlatButton *> (wdgList[idx])->SetLabel (gdt->Surface ());
        AddWidget (wdgList [idx]);
      }
    }

    // Add highlighter...
    AddWidget (&wdgEmph);
      // Note: 'wdgEmph' must be the last widget added her - see comment in HandleEvent()

    // Setup timer ...
    CTimer::Set (0, tInterval);
  }
  else {

    // Stop timer & clean alert lock ...
    //   The "active" lock may have been issued due to an alert.
    //   In this case, we can only get here if the user quits the screen explicitely.
    CTimer::Clear ();
    SystemActiveUnlock ("_floorplan", false);
    returnScreen = NULL;
  }
}


void CScreenFloorplan::OnTime () {
  CGadget *gdt;
  ERctUseState useState;
  int n, idx;

  // Sanity...
  if (!floorplan) return;

  // Update floorplan ...
  floorplan->Iterate ();

  // Update dependent widgets ...
  for (n = 0; n < floorplan->ChangedGadgets (); n++) {
    idx = floorplan->ChangedGadgetIdx (n);
    gdt = floorplan->Gadget (idx);
    if (!gdt->IsPushable ())
      wdgList[idx]->SetSurface (gdt->Surface ());
    else
      static_cast<CFlatButton *> (wdgList[idx])->SetLabel (gdt->Surface ());
  }

  // Update emphasis ...
  if (floorplan->ChangedEmph ()) wdgEmph.SetSurface (floorplan->GetEmphSurface ());

  // Update button bar...
  useState = floorplan->GetValidUseState ();
  if (useState != lastUseState) {
    if (lastUseState >= 0) buttonBar[btnIdFpUseDay + lastUseState].SetLabel (WHITE, fpButtons[btnIdFpUseDay + lastUseState].iconName);
    if (useState >= 0) buttonBar[btnIdFpUseDay + useState].SetLabel (YELLOW, fpButtons[btnIdFpUseDay + useState].iconName);
    lastUseState = useState;
    UpdateRequest ();
  }

  // Handle alert...
  CheckAlert (NULL);
}


void CScreenFloorplan::OnButtonPushed (CButton *btn, bool longPush) {
  CResource *rc = floorplan->UseStateRc ();
  CRcRequest *req;
  ERctUseState useStateReq;
  EBtnIdFloorplan btnId = ((EBtnIdFloorplan) (btn - buttonBar));

  if (!rc) return;

  useStateReq = lastUseStateReq;

  switch (btnId) {
    case btnIdFpUseAuto:
      rc->DelRequest (RcGetUserRequestId ());
      useStateReq = (ERctUseState) -1;
      break;
    case btnIdFpUseDay:
    case btnIdFpUseNight:
    case btnIdFpUseLeaving:
    case btnIdFpUseVacation:
      req = NewUserRequest ();
      useStateReq = (ERctUseState) (btnId - btnIdFpUseDay);
      req->SetValue (useStateReq);
      rc->SetRequest (req);
      break;
    default:
      ASSERT (false);
  }

  UpdateRequest (useStateReq);
}


bool CScreenFloorplan::HandleEvent (SDL_Event *ev) {
  SDL_Rect *r;
  int n, x, y, dist, dx, dy, minIdx, minDist;

  // Select the nearest pushable first to improve the accuracy for close pushable gadgets...
  if (firstWidget == &wdgEmph && pushableGadgets >= 1 && ev->type == SDL_MOUSEBUTTONDOWN) {
    // Note: 'wdgEmph' must be the last widget added in 'Activate ()'. If this is not the topmost
    //       one here, we conclude that some modal widget is presently on top of it and revert to normal
    //       event handling.
    wdgPoolPushable[0].GetMouseEventPos (ev, &x, &y);
      // We have no canvas => can use the first (= any) gadget to transform the mouse coordinates once.
    minIdx = -1;
    minDist = INT_MAX;
    for (n = 0; n < pushableGadgets; n++) if (wdgPoolPushable[n].IsOnScreen (this)) {
      r = wdgPoolPushable[n].GetArea ();
      if (RectContains (r, x, y)) {
        dx = r->x + r->w/2 - x;
        dy = r->y + r->h/2 - y;
        dist = dx * dx + dy * dy;
        if (dist < minDist) {
          minIdx = n;
          minDist = dist;
        }
      }
    }
    if (minIdx >= 0) {
      // Pass the event to the closest pushable gadget...
      if (wdgPoolPushable[minIdx].HandleEvent (ev)) return true;
    }
  }

  // Not a push event on a pushable gadget or closes gadget could not handle the event
  // => continue with the normal strategy...
  return CScreen::HandleEvent (ev);
}


void CScreenFloorplan::UpdateRequest (ERctUseState useStateReq) {
  CRcRequest req;

  ASSERT (btnIdFpUseDay - 1 == btnIdFpUseAuto);
  //~ INFOF (("### CScreenFloorplan::UpdateRequest (%i) ...", (int) useStateReq));

  if (useStateReq < (ERctUseState) 0) {
    floorplan->UseStateRc ()->GetRequest (&req, RcGetUserRequestId ());
    //~ INFOF (("###   request = '%s'", req.ToStr ()));
    useStateReq = (ERctUseState) req.Value ()->ValidEnumIdx (rctUseState, -1);
  }

  if (useStateReq != lastUseStateReq) {
    ASSERT (btnIdFpUseDay - 1 == btnIdFpUseAuto);
    buttonBar[btnIdFpUseDay + lastUseStateReq].SetColor (COL_FP_MAIN);
    buttonBar[btnIdFpUseDay + useStateReq].SetColor (COL_FP_MAIN_DARKER);
    lastUseStateReq = useStateReq;
  }
}





// *************************** Top-Level ***************************************


static inline void FloorplanEnsureScreen () {   // Create screen if not yet there ...
  if (!fpScreen) {
    SETO (fpScreen, new CScreenFloorplan ());
    fpScreen->Setup (fpFloorplan);
  }
}


void FloorplanInit () {
  fpFloorplan = new CFloorplan ();
  if (!fpFloorplan->Setup ("floorplan")) FREEO(fpFloorplan);
}


void FloorplanDone () {
#if WITH_CLEANMEM
  FREEO (fpScreen);
  FREEO (fpFloorplan);
#endif // WITH_CLEANMEM
}


void FloorplanActivate () {

  // Sanity...
  if (!fpFloorplan) return;
  FloorplanEnsureScreen ();

  // Activate screen...
  fpScreen->Activate ();
}


void FloorplanCheckAlert (CScreen *returnScreen) {
  FloorplanEnsureScreen ();
  fpScreen->CheckAlert (returnScreen);
}


class CFloorplan *FloorplanGetMain () {
  return fpFloorplan;
}


void FloorplanUnsubscribeAll () {
  if (fpFloorplan) fpFloorplan->SetView (fvlNone);
}
