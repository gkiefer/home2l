/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2015-2020 Gundolf Kiefer
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


#include "ui_widgets.H"
#include "system.H"
#include "apps.H"



#define MAX_CALS 10   // maximum number of distinct calendars





// *************************** Environment options *****************************


ENV_PARA_NOVAR ("calendar.enable", bool, envCalendarEnable, false);
  /* Enable calendar applet
   */
ENV_PARA_STRING ("calendar.host", envCalendarHost, NULL);
  /* Host with calendar files (local if unset)
   *
   * On the storage host, the tools \texttt{GNU patch}, \texttt{cat}, and
   * \texttt{remind} must be installed. The latter is not needed if
   * \refenv{calendar.nearbyHost} is defined.
   *
   * If a host is set, the application will use \texttt{ssh} to run any commands
   * on the host as user \texttt{'home2l'}. Hence, to access the calendars
   * as a unified user on the local machine, it is advisable to enter
   * \texttt{'localhost'} here.
   */
ENV_PARA_STRING ("calendar.nearbyHost", envCalendarNearbyHost, NULL);
  /* (Optional) Nearby host to accelerate calendar browsing
   *
   * If set, calendar files are cached locally (in RAM), and for processing,
   * \texttt{remind} is invoked on the machine passed here.
   * This improves the UI responsiveness if the network connection to the storage
   * host or the storage host itself is slow.
   *
   * On the nearby machine, \texttt{remind} must be installed.
   */

ENV_PARA_STRING ("calendar.dir", envCalendarDir, "var/calendars");
  /* Storage directory for calendar (reminder) files.
   *
   * The path may be either absolute or relative to HOME2L\_ROOT.
   */
ENV_PARA_SPECIAL ("calendar.<n>.name", const char *, NULL);
  /* Name for calendar $\#n$
   */
ENV_PARA_SPECIAL ("calendar.<n>.color", int, NODEFAULT);
  /* Color for calendar $\#n$
   *
   * This should by given as a 6-digit hex number in the form \texttt{0x<rr><gg><bb>}.
   */





// *********************************************************
// *                                                       *
// *                Model-related classes                  *
// *                                                       *
// *********************************************************





// ***************** Headers *********************


class CCalFile {
public:
  CCalFile () { isDefined = false; lineArr = NULL; lines = 0; }
  ~CCalFile () { Invalidate (); }

  // Definition (always valid) ...
  void SetNames (int _idx, TColor _color, const char *_name);

  bool IsDefined () { return isDefined; }
  const char *GetName () { return name; }
  int GetIdx () { return idx; }
  TColor GetColor () { return color; }

  // Data (loaded on demand) ...
  bool IsLoaded () { return lineArr != NULL; }

  void Invalidate ();      // Invalidate loaded data
  bool Load (CShellSession *shell);
    // Load if not yet loaded. Before GetLines() or GetLine() are used, this must
    // be called. To invalidate loaded data, Clear() must be called.

  int GetLines () { return lines; }
  char *GetLine (int n) { return lineArr[n]; }

protected:
  bool isDefined;
  CString name;
  char **lineArr;
  int idx, lines;   // 'idx' = number of file
  TColor color;     // file identification color (for the displays)
};


class CCalEntry {
public:
  CCalEntry () { next = NULL; calFile = NULL; }

  const char *GetMessage () { return msg.Get (); }
  TDate GetDate () { return date; }
  TTime GetTime () { return time; }
  TTime GetDur () { return dur; }
  bool IsAllDay () { return dur >= TIME_OF(24,0,0); }

  CCalFile *GetFile () { return calFile; }
  int GetLineNo () { return lineNo; }         // lineNo starting from 0 (not 1!)

  CCalEntry *GetNext () { return next; }

protected:
  friend class CCalViewData;

  TDate date;
  TTime time, dur;    // all-day event: time == 0, dur = 24h
  CString msg;

  CCalFile *calFile;
  TColor fileColor;
  int lineNo;

  CCalEntry *next;
};


class CCalViewData {
public:
  CCalViewData ();
  ~CCalViewData ();

  void Clear () { DelCalEntries (); }

  void SetFile (int fileNo, TColor color, const char *name);
  CCalFile *GetFile (int fileNo) { return &calFileArr[fileNo]; }

  void InvalidateFile (int fileNo) { calFileArr[fileNo].Invalidate (); }
    // Invalidate a single file and removes all cal entries related to it.
  bool LoadFile (int fileNo);

  void LoadCalEntries (int fileNo);
    // (re-)loads cal entries related to a file
  void LoadAllCalEntries ();
    // (re-)loads cal entries related to all files (e.g. after a reference date change)

  bool PatchFiles (CString *patch, CString *ret);
    // Applies the patch containes in 'patch'. On error, 'false' is returned and 'ret'
    // is filled with the output of the patch command.
    // This method does NOT reload any modified files.

  bool SetRefDate (TDate _refDate, int _weeks = 7);
     // Sets the current reference date and (re-)invokes remind if necessary.
     // Also decides about the actual time interval represented by the view,
     // e.g. the full month of the passed reference date including a preceeding and
     // one or two succeeding weeks.
     // Returns true if something has changed (e.g. caller must invoke 'LoadAllCalEntries' and update its drawings).

  TDate GetRefDate () { return refDate; }
  TDate GetFirstDate () { return firstDate; }
  TDate GetWeeks () { return weeks; }

  CCalEntry *GetFirstCalEntry () { return firstEntry; }
  CCalEntry *GetFirstCalEntryOfDate (TDate date);

  void ClearError () { errorFile = -1; }
  bool HaveError () { return errorFile >= 0; }
  const char *GetErrorMsg () { return errorMsg.Get (); }
  int GetErrorFile () { return errorFile; }
  int GetErrorLine () { return errorLine; }

protected:
  void AddCalEntries (CCalEntry *newList);  // merge in new list of entries in correct timed order
  void DelCalEntries (CCalFile *calFile = NULL);   // delete all entries / only those related to file 'calFile'
  CCalEntry *RunRemind (int fileNo);  // invoke remind and return a sorted list of cal entries

  CCalFile calFileArr[MAX_CALS];
  CCalEntry *firstEntry;

  CShellSession shellRemote, *shellNearby;

  int errorFile, errorLine;
  CString errorMsg;

  TDate refDate, firstDate;
  int weeks;
};





// ***************** CCalFile ******************************


void CCalFile::Invalidate () {
  int n;

  //~ INFOF (("### Invalidating '%s' ...", name.Get ()));
  if (lineArr) {
    for (n = 0; n < lines; n++) if (lineArr[n]) free (lineArr[n]);
    free (lineArr);
    lineArr = NULL;
  }
}


void CCalFile::SetNames (int _idx, TColor _color, const char *_name) {
  idx = _idx;
  color = _color;
  name.Set (_name);
  isDefined = true;
}


bool CCalFile::Load (CShellSession *shell) {
  CString s, cmd, line;
  char **_lineArr;
  int n, lineArrSize;

  // Sanity...
  if (IsLoaded ()) return true;
  //~ INFOF (("### Loading '%s' ...", name.Get ()));

  // Open file...
  cmd.SetF ("cat %s/%s.rem", envCalendarDir, name.Get ());
  DEBUGF (1, ("Running '%s' on '%s' ...", cmd.Get (), shell->Host ()));
  shell->Start (cmd.Get (), true);
  shell->WriteClose ();   // we are not going to write anything

  // Read loop...
  lines = lineArrSize = 0;
  while (!shell->ReadClosed ()) {
    shell->WaitUntilReadable ();
    while (shell->ReadLine (&line)) {
      //~ INFOF(("# Read '%s'.", line.Get ()));
      // realloc buffer if necessary...
      if (lines >= lineArrSize) {
        lineArrSize = lineArrSize ? (lineArrSize << 1) : 64;
        _lineArr = MALLOC(char *, lineArrSize);
        for (n = 0; n < lines; n++) _lineArr[n] = lineArr[n];
        SETP(lineArr, _lineArr);
      }
      // store line...
      lineArr[lines] = line.Disown ();
      lines++;
    }
  }

  // Complete...
  shell->Wait ();
  if (shell->ExitCode ()) {
    WARNINGF (("Command '%s' exited with error (%i)", cmd.Get (), shell->ExitCode ()));
    StringF (&s, _("Failed to load calendar file '%s/%s.rem':\n"),
                   envCalendarDir, name.Get ());
    if (!lines) s.AppendF ("\n(no output)");
    else {
      for (n = 0; n < lines && n < 10; n++) s.AppendF ("\n%s", lineArr[n]);
      if (n < lines) s.Append ("\n...");
    }
    Invalidate ();
    RunErrorBox (s, NULL, -1, FontGet (fntMono, 20));
    return false;
  }
  //~ INFOF (("Done with command '%s'", cmd.Get ()));
  return true;
}





// ***************** CCalViewData **************************


CCalViewData::CCalViewData () {
  weeks = 0;
  firstEntry = NULL;
  errorFile = -1;
  firstDate = refDate = 0;
  shellRemote.SetHost (envCalendarHost);
  if (!envCalendarNearbyHost) shellNearby = NULL;
  else if (!envCalendarNearbyHost[0]) shellNearby = NULL;
  else {
    DEBUGF (1, ("Enabling nearby session on '%s'...", envCalendarNearbyHost));
    shellNearby = new CShellSession ();
    if (ANDROID || strcmp (envCalendarNearbyHost, "localhost") != 0)    // On Linux, skip ssh if 'localhost' is entered.
      shellNearby->SetHost (envCalendarNearbyHost);
  }
}


CCalViewData::~CCalViewData () {
  Clear ();
  FREEO(shellNearby);
}


void CCalViewData::SetFile (int fileNo, TColor color, const char *name) {
  calFileArr[fileNo].SetNames (fileNo, color, name);
}


bool CCalViewData::LoadFile (int fileNo) {
  ASSERT (fileNo >= 0 && fileNo < MAX_CALS);
  return calFileArr[fileNo].Load (&shellRemote);
}


void CCalViewData::LoadCalEntries (int fileNo) {
  //~ INFOF (("### LoadCalEntries(%i)", fileNo));
  DelCalEntries (&calFileArr[fileNo]);
  if (weeks) AddCalEntries (RunRemind (fileNo));
}


bool CCalViewData::PatchFiles (CString *patch, CString *ret) {
  CString s;
  int exitCode;

  exitCode = shellRemote.Run (StringF (&s, "cd %s; patch -ubNp1", envCalendarDir), patch->Get (), ret);
  return (exitCode == 0);
}


bool CCalViewData::SetRefDate (TDate _refDate, int _weeks) {
  TDate _firstDate, firstOfMonth;
  bool update;

  firstOfMonth = DateFirstOfMonth (_refDate);
  _firstDate = DateIncByDays (firstOfMonth, -GetWeekDay (firstOfMonth) - 7);
  update = (_firstDate != firstDate || _weeks != weeks);

  refDate = _refDate;
  firstDate = _firstDate;
  weeks = _weeks;

  return update;
}


CCalEntry *CCalViewData::GetFirstCalEntryOfDate (TDate date) {
  CCalEntry *ret = firstEntry;
  while (ret && ret->date < date) ret = ret->next;
  return ret;
}


static inline int CalEntryCompare (CCalEntry *ce1, CCalEntry *ce2) {
  if (ce1->GetDate () != ce2->GetDate ()) return ce1->GetDate () - ce2->GetDate ();
  else return ce1->GetTime () - ce2->GetTime ();
}


void CCalViewData::AddCalEntries (CCalEntry *newList) {
  CCalEntry *ce, **pCe;

  pCe = &firstEntry;
  while (newList) {
    while (*pCe && CalEntryCompare (*pCe, newList) <= 0) pCe = &((*pCe)->next);
      // 'pCe' now points to a pointer to a larger element or to the last pointer in the list
    ce = newList;
    newList = newList->next;
    ce->next = *pCe;
    *pCe = ce;
  }
}


void CCalViewData::DelCalEntries (CCalFile *calFile) {
  CCalEntry *ce, **pCe;

  pCe = &firstEntry;
  while (*pCe) {
    ce = *pCe;
    if (!calFile || ce->calFile == calFile) {
      // Delete the entry...
      *pCe = ce->next;
      delete ce;
    }
    else {
      // Do not delete, move on to next entry...
      pCe = &((*pCe)->next);
     }
   }
}


CCalEntry *CCalViewData::RunRemind (int fileNo) {
  CCalEntry *first, **pLast, *ce;
  CString cmd, line;
  CShellSession *shell;
  char strTime[8], strDur[8];
  int sendLine, lineNo, n, msgPos, year, mon, day;
  bool canSend, canReceive;

  // Build command line for remind & start ...
  shell = shellNearby ? shellNearby : &shellRemote;
  if (shellNearby) {
    // Nearby shell: Load file locally and pipe it through remind ...
    if (!LoadFile (fileNo)) return NULL;
    cmd.SetF ("remind -l -ms+%i -b2 -gaaad - %i-%02i-%02i",
              weeks,
              YEAR_OF(firstDate), MONTH_OF(firstDate), DAY_OF(firstDate));
  }
  else {
    // Remote shell: Let remind load the file on the remote machine ...
    cmd.SetF ("remind -l -ms+%i -b2 -gaaad %s/%s.rem %i-%02i-%02i",
              weeks,
              envCalendarDir, calFileArr[fileNo].GetName (),
              YEAR_OF(firstDate), MONTH_OF(firstDate), DAY_OF(firstDate));
  }
  DEBUGF (1, ("For calendar #%i, running '%s' on '%s'...", fileNo, cmd.Get (), shell->Host () ? shell->Host () : "<localhost>"));
  shell->Start (cmd.Get (), true);

  // Communication loop ... l
  first = NULL;
  pLast = &first;
  sendLine = 0;
  canSend = false;      // will remain 'false' in remote mode
  lineNo = -1;
  if (errorFile == fileNo) ClearError ();
  if (!shellNearby) shell->WriteClose ();     // we won't send input in remote mode
  while (!shell->ReadClosed ()) {
    shell->CheckIO (shellNearby ? &canSend : NULL, &canReceive);
    if (canSend) {      // only effective in "nearby" mode
      if (sendLine >= calFileArr[fileNo].GetLines ()) shell->WriteClose ();
      else shell->WriteLine (calFileArr[fileNo].GetLine (sendLine++));
    }
    if (canReceive) if (shell->ReadLine (&line)) {

      // Check for file/line number information...
      if (sscanf (line.Get (), "# fileinfo %d", &lineNo) == 1) { lineNo--; }

      // Check for calendar entry...
      else if (sscanf (line.Get (), "%d/%d/%d * * %s %s %n", &year, &mon, &day, strDur, strTime, &msgPos) >= 5) {

        //~ INFOF(("### Got: %s", line.Get ()));
        ce = new CCalEntry ();

        ce->date = DATE_OF(year, mon, day);
        if (sscanf (strTime, "%d", &n) == 1) ce->time = TIME_OF(0, n, 0); else ce->time = 0;
        if (sscanf (strDur, "%d", &n) == 1) ce->dur = TIME_OF(0, n, 0); else ce->dur = TIME_OF(24,0,0);
        ce->msg.Set (line.Get () + msgPos);
        ce->calFile = &calFileArr[fileNo];
        ce->lineNo = lineNo;

        //~ INFOF (("### Parsed '%s' to CCalEntry:", line.Get ()));
        //~ INFOF (("#      %i-%02i-%02i at %2i:%02i dur %2i:%02i: %s",
                //~ YEAR_OF(ce->GetDate ()), MONTH_OF(ce->GetDate ()), DAY_OF(ce->GetDate ()),
                //~ HOUR_OF(ce->GetTime ()), MINUTE_OF(ce->GetTime ()), HOUR_OF(ce->GetDur ()), MINUTE_OF(ce->GetDur ()),
                //~ ce->GetMessage ()));
        //~ INFOF (("#      from '%s' (%i)", ce->calFile->GetName (), ce->lineNo));

        ce->next = NULL;
        *pLast = ce;
        pLast = &(ce->next);
      }

      // Check for error message...
      else if (sscanf (line.Get (), "-stdin-(%i):", &n) == 1) {
        //~ WARNINGF(("Remind error: '%s'", line.Get ()));
        if (!HaveError ()) {
          errorFile = fileNo;
          errorLine = n - 1;
          errorMsg.Set (line, line.LFind (':') + 2);
        }
      }
      else WARNINGF(("Unparsable line in remind output while processing '%s': %s",
                     calFileArr[fileNo].GetName (), line.Get ()));
    }
  }
  shell->Wait ();
  if (shell->ExitCode ())
    WARNINGF (("Command '%s' exited with error (%i)", cmd.Get (), shell->ExitCode ()));
  return first;
}





// *********************************************************
// *                                                       *
// *                View-related classes                   *
// *                                                       *
// *********************************************************


#define WEEKS 7

#define CELL_W 64
#define CELL_H 60
#define CELL0_W 32
#define CELL0_H 28

#define CAL_W (CELL0_W + CELL_W * 7)            // 7 Days a week + 1st column for calweeks
#define CAL_H (CELL0_H + CELL_H * WEEKS)        // 1st row contains week day names
#define CAL_X 0
#define CAL_Y (UI_RES_Y - UI_BUTTONS_HEIGHT - UI_BUTTONS_SPACE - CAL_H)

#define COL_BUTTONS DARK_CYAN

#define COL_CALGRID GREY              // line grid and calendar weeks
#define COL_CALBACK DARK_GREY         // calendar background
#define COL_CALMON  BLACK             // background of current month
#define COL_CALTEXT WHITE             // month days and headers
#define COL_CALCURSOR ToColor (0, 0xFF, 0xFF, 0x60)

#define COL_EVGRID DARK_GREY
#define COL_EVHEAD LIGHT_GREY
#define COL_EVBACK WHITE
#define COL_EVTEXT BLACK
#define COL_EVSELECTED CYAN


static inline SDL_Rect CellRect (int x, int y) {
  SDL_Rect ret = Rect (CELL0_W + CELL_W * ((x)-1), CELL0_H + CELL_H * ((y)-1), CELL_W - 1, CELL_H - 1);
  if (ret.x < 0) {
    ret.w += ret.x;
    ret.x = 0;
  }
  if (ret.y < 0) {
    ret.h += ret.y;
    ret.y = 0;
  }
  return ret;
}


class CEventsBox: public CListbox {
  public:
    CEventsBox () { viewData = NULL; fontBold = fontHead = NULL; }

    void Setup (CCalViewData *_viewData);    // calls 'SetMode' and 'SetLayout' with the respective parameters

    CCalEntry *GetCalEntry (int idx) { return (CCalEntry *) GetItem (idx)->data; }

  protected:
    TTF_Font *fontBold, *fontHead;
    class CCalViewData *viewData;

    virtual SDL_Surface *RenderItem (CListboxItem *item, int idx, SDL_Surface *prevSurf);
      // 'CListboxItem::data' contains a pointer to the respective 'CCalEntry' element of
      // a) the event, if 'isSpecial' == false
      // b) some event of the day, if 'isSpecial' == true
      // The item's 'text' field is not used (can be set to 0)
};


class CScreenCalEdit: public CInputScreen {
  public:
    CScreenCalEdit () { fileNo = 0; }
    ~CScreenCalEdit () {}

    bool Setup (CCalViewData *_viewData, int _fileNo = -1, int _lineNo = -1);
      // 'fileNo = -1' => last file, 'lineNo = -1' => new entry (for given file)

  protected:
    friend class CScreenCalMain;

    CButton btnTrash, btnSplitRepeated, btnCalNo, btnDatePrev, btnDateNext;

    CCalViewData *viewData;
    int origFileNo, origLineNo, fileNo, lineNo;
    TDate date;
    CString extraLines[2];   // extra lines to be added to the reminders file (e.g. when splitting repeated reminders)

    // Helpers ...
    void CommitOrDelete (bool commitNoDelete);

    // Callbacks ...
    virtual void Commit () { CommitOrDelete (true); }
      ///< Called on push of the "OK" button. May call @ref Return() on success or not to continue editing.
    virtual void OnUserButtonPushed (CButton *btn, bool longPush);
      ///< Called on pushing any user button.
};


class CScreenCalMain: public CScreen {
  public:
    CScreenCalMain () { surfCalendar = NULL; fingerOnCalendar = false; }
    ~CScreenCalMain () {}

    void Setup ();

    void UpdateFile (int fileNo);   // (re-)load a calendar file
    void UpdateAllFiles ();         // (re-)load all calendar files

    void SetRefDate (TDate d, bool scrollEventList = true);
    TDate GetRefDate () { return viewData.GetRefDate (); }

    // Callbacks...
    bool CalendarHandleEvent (SDL_Event *ev);   // Handles events for 'wdgCalendar'
    void OnButtonPushed (CButton *b);
    //~ void OnMonthButtonPushed (CButton *b);
    void OnEventPushed (int idx, CCalEntry *calEntry);

  protected:
    CButton btnBack, btnPrevMon, btnNextMon, btnToday, btnReload, btnNew;
    CButton btnMonth;             // button above calendar
    CCursorWidget wdgCalendar;    // month calendar view...
    SDL_Surface *surfCalendar;
    bool fingerOnCalendar;
    CEventsBox wdgEvents;         // event list

    CCalViewData viewData;

    void DoUpdateFile (int fileNo);

    void DrawCalendar ();
    void EventListUpdate ();
    void EventListSetRefDate (TDate d, TDate oldD = 0, bool scrollTo = true);
    void RunMonthMenu ();
    void RunEditScreen (int _fileNo = -1, int _lineNo = -1);
};


static CScreenCalMain *scrCalMain = NULL;
static CScreenCalEdit *scrCalEdit = NULL;





// ***************** CEventsBox ****************************


void CEventsBox::Setup (CCalViewData *_viewData) {
  viewData = _viewData;
  SetMode (lmActivate, 48, 1);
  SetFormat (FontGet (fntNormal, 16), -1, COL_EVGRID,
              COL_EVTEXT, COL_EVBACK,
              COL_EVTEXT, COL_EVSELECTED,
              COL_EVTEXT, COL_EVHEAD);
  CCanvas::SetColors (BLACK, ToColor (0, 0, 0, 128));
  fontBold = fontHead = FontGet (fntBold, 16);
}


SDL_Surface *CEventsBox::RenderItem (CListboxItem *item, int idx, SDL_Surface *surf) {
  CCalEntry *calEntry = (CCalEntry *) item->data;
  CTextSet textSet;
  CString str1, str2;
  SDL_Rect r;
  TColor backColor;
  TDate d;
  TTime t0, t1;
  int n;

  if (!surf) surf = CreateSurface (area.w, itemHeight);
  backColor = item->isSpecial ? colBackSpecial : colBack;
  if (item->IsSelected () || (item->isSpecial && calEntry->GetDate () == viewData->GetRefDate ())) backColor = colBackSelected;
  SDL_FillRect (surf, NULL, ToUint32 (backColor));

  d = calEntry->GetDate ();
  if (item->isSpecial) {

    // Draw heading entry...
    //   TRANSLATORS: Format string for the day header in the calendar event list (de_DE: "%s   %i.%i.%i")
    //                Arguments are: <week day name>, <day>, <month>, <year>
    str1.SetF (_("%1$s   %3$i/%2$i/%4$i"), DayName (GetWeekDay (d)), DAY_OF(d), MONTH_OF(d), YEAR_OF(d));
    textSet.AddLines (str1, CTextFormat (fontHead, colLabel, backColor, 0, 0));
    r = Rect (0, 0, area.w, itemHeight);
    textSet.Render (surf, &r);
  }
  else {

    // Draw normal entry...
    str1.Set (calEntry->GetMessage ());
    if (calEntry->IsAllDay ()) str2.Clear ();
    else {
      t0 = calEntry->GetTime ();
      t1 = t0 + calEntry->GetDur ();
      str2.SetF ("   %i:%02i - %2i:%02i", HOUR_OF(t0), MINUTE_OF(t0), HOUR_OF(t1), MINUTE_OF(t1));
    }
    n = str1.LFind (';');
    if (n >= 0) {
      str2.Append ("   ");
      str2.Append (str1.Get () + n + 1);
      str1.Del (n);
    }
    textSet.AddLines (str1, CTextFormat (fontBold, colLabel, backColor, -1, 0));
    if (str2.Len () > 0) textSet.AddLines (str2, CTextFormat (font, colLabel, backColor, -1, 0));
    r = Rect (16, 0, area.w-12, itemHeight);    // "16" = space in front of line
    textSet.Render (surf, &r);

    // Draw calendar file indicator...
    r = Rect (0, 0, 12, itemHeight);
    SDL_FillRect (surf, &r, ToUint32 (calEntry->GetFile ()->GetColor ()));
  }

  return surf;
}





// ***************** CScreenCalEdit ************************


bool CScreenCalEdit::Setup (CCalViewData *_viewData, int _fileNo, int _lineNo) {
  static const int userBtnWidth [] = { -1, -2, -1, -1, -1 };
  CButton *userBtnList [] = { &btnTrash, &btnCalNo, &btnSplitRepeated, &btnDatePrev, &btnDateNext };
  CCalFile *calFile;
  int n;

  // Take over data...
  viewData = _viewData;
  origFileNo = _fileNo;
  origLineNo = lineNo = _lineNo;
  if (origFileNo >= 0) fileNo = origFileNo;    // take last file number unless '_fileNo' is >= 0
  date = _viewData->GetRefDate ();

  // Load file...
  if (!viewData->LoadFile (fileNo)) return false;

  // Output buffer ...
  for (n = 0; n < (int) (sizeof (extraLines) / sizeof (CString)); n++) extraLines[n].Clear ();

  // Buttons & layout ...
  CInputScreen::Setup (
      (fileNo >= 0 && lineNo >= 0) ? viewData->GetFile (fileNo)->GetLine (lineNo) : NULL,
      COL_BUTTONS, sizeof (userBtnList) / sizeof (userBtnList[0]), userBtnList, userBtnWidth
    );

  btnTrash.SetLabel (WHITE, "ic-delete_forever-48");

  //~ btnSplitRepeated.SetColor (DARK_GREY);
  btnSplitRepeated.SetLabel (GREY, "ic-repeat_one-48");   // not implemented yet (TBD)

  calFile = viewData->GetFile (fileNo);
  btnCalNo.SetColor (ColorScale (calFile->GetColor (), 0xc0));
  btnCalNo.SetLabel (calFile->GetName ());

  //~ btnDatePrev.SetColor (DARK_GREY);
  btnDatePrev.SetLabel ("-", GREY);   // not implemented yet (TBD)

  //~ btnDateNext.SetColor (DARK_GREY);
  btnDateNext.SetLabel ("+", GREY);   // not implemented yet (TBD)

  // Done ...
  return true;
}


void CScreenCalEdit::OnUserButtonPushed (CButton *btn, bool longPush) {

  // Button 'btnTrash'...
  if (btn == &btnTrash) {
    if (RunSureBox (_("Really remove the current entry?")) == 1)
      CommitOrDelete (false);
  }

  // Button "Select calendar"....
  else if (btn == &btnCalNo) {
    CMenu menu;
    CCalFile *calFile;
    SDL_Rect r;
    int n, cals, calArr[MAX_CALS];

    // Turn off keyboard to generate space for the menu...
    SetKeyboard (false);

    // Setup menu...
    cals = 0;
    for (n = 0; n < MAX_CALS; n++) if (viewData->GetFile (n)->IsDefined ()) calArr[cals++] = n;
    r.x = btnCalNo.GetArea ()->x;
    r.y = btnCalNo.GetArea ()->y + btnCalNo.GetArea ()->h;
    r.w = UI_RES_X - r.x;
    r.h = UI_RES_Y - r.y;
    menu.Setup (r, -1, -1, COL_BUTTONS);
    menu.SetItems (cals);
    for (n = 0; n < cals; n++) menu.SetItem (n, viewData->GetFile (calArr[n])->GetName ());

    // Run menu...
    n = menu.Run (this);

    // Turn on keyboard again...
    SetKeyboard (true);

    // Evaluate selection...
    if (n >= 0) {
      fileNo = calArr[n];
      calFile = viewData->GetFile (fileNo);
      btnCalNo.SetColor (ColorScale (calFile->GetColor (), 0xc0));
      btnCalNo.SetLabel (calFile->GetName ());
    }
  }
}


void CScreenCalEdit::CommitOrDelete (bool commitNoDelete) {
  CString s, patch, input;
  CCalFile *calFile;
  int n, oldLines, newLines;

  // Load new (current) file ...
  if (!viewData->LoadFile (fileNo)) return;

  // Patch for new file...
  if (commitNoDelete) wdgInput.GetInput (&input);
  //   ... count lines to replace...
  oldLines = (fileNo == origFileNo && origLineNo >= 0) ? 1 : 0;
  //   ... count new lines ...
  newLines = 0;
  if (input[0]) newLines++;
  for (n = 0; n < (int) (sizeof (extraLines) / sizeof (CString)); n++)
    if (extraLines[n][0]) newLines++;
  //   ... write header ...
  calFile = viewData->GetFile (fileNo);
  if (lineNo < 0) lineNo = calFile->GetLines ();    // append new lines at end of file
  patch.SetF ("--- old/%s.rem\n+++ new/%s.rem\n@@ -%i,%i +%i,%i @@",
              calFile->GetName (), calFile->GetName (),
              lineNo + 1, oldLines, lineNo + 1, newLines);
  //   ... write old line ...
  if (oldLines) {
    s.SetF ("\n-%s", calFile->GetLine (lineNo));
    patch.Append (s);
  }
  //   ... write new lines ...
  if (extraLines[0][0]) patch.Append (StringF (&s, "\n+%s", extraLines[0].Get ()));
  if (input[0]) patch.Append (StringF (&s, "\n+%s", input.Get ()));
  if (extraLines[1][0]) patch.Append (StringF (&s, "\n+%s", extraLines[1].Get ()));

  // Patch for old file (if applicable)...
  if (origFileNo != fileNo && origLineNo >= 0) {
    if (viewData->LoadFile (origFileNo)) {
      calFile = viewData->GetFile (origFileNo);
      s.SetF ("\n--- old/%s.rem\n+++ new/%s.rem\n@@ -%i,1 +%i,0 @@\n-%s",
                calFile->GetName (), calFile->GetName (),
                origLineNo + 1, origLineNo + 1,
                calFile->GetLine (origLineNo));
      patch.Append (s);
    }
  }

  // Output/apply patch...
  //~ INFOF (("##### Created calendar patch:\n%s", patch.Get ()));
  if (viewData->PatchFiles (&patch, &s)) {
    Return ();
    UiIterateNoWait ();
    scrCalMain->UpdateFile (fileNo);
    if (origFileNo != fileNo && origLineNo >= 0) {
      UiIterateNoWait ();
      scrCalMain->UpdateFile (origFileNo);
    }
  }
  else {
    RunErrorBox (s, NULL, -1, FontGet (fntMono, 20));
  }
}





// ***************** CScreenCalMain ************************



// ***** Helpers *****


static bool CbCalendarHandleEvent (SDL_Event *ev, void *data) {
  return scrCalMain->CalendarHandleEvent (ev);
}


static void CbButtonPushed (class CButton *, bool, void *data) {
  scrCalMain->OnButtonPushed ((CButton *) data);
}

static void CbEventPushed (CListbox *lb, int idx, bool, void *) {
  scrCalMain->OnEventPushed (idx, (CCalEntry *) lb->GetItem (idx)->data);
}



// ***** Setup *****


void CScreenCalMain::Setup () {
  char key[64];
  const char *val;
  SDL_Rect *layout;
  int n;

  // Main buttons...
  static const int layoutFmt[] = { UI_BUTTONS_BACKWIDTH, -1, -1, -1, -1, -1, 0 };
  layout = LayoutRow (UI_BUTTONS_RECT, layoutFmt);
  n = 0;

  btnBack.Set (layout[n++], COL_BUTTONS, IconGet ("ic-back-48"));
  btnBack.SetCbPushed (CbAppEscape);
  btnBack.SetHotkey (SDLK_ESCAPE);
  AddWidget (&btnBack);

  btnPrevMon.Set (layout[n++], COL_BUTTONS, IconGet ("ic-arrow_back-48"));
  btnPrevMon.SetCbPushed (CbButtonPushed, &btnPrevMon);
  btnPrevMon.SetHotkey (SDLK_COMMA);
  AddWidget (&btnPrevMon);

  btnNextMon.Set (layout[n++], COL_BUTTONS, IconGet ("ic-arrow_forward-48"));
  btnNextMon.SetCbPushed (CbButtonPushed, &btnNextMon);
  btnNextMon.SetHotkey (SDLK_PERIOD);
  AddWidget (&btnNextMon);

  btnToday.Set (layout[n++], COL_BUTTONS, _("Today"));
  btnToday.SetCbPushed (CbButtonPushed, &btnToday);
  btnToday.SetHotkey (SDLK_HOME);
  AddWidget (&btnToday);

  btnReload.Set (layout[n++], COL_BUTTONS, IconGet ("ic-refresh-48"));
  btnReload.SetCbPushed (CbButtonPushed, &btnReload);
  btnReload.SetHotkey (SDLK_F5);
  AddWidget (&btnReload);

  btnNew.Set (layout[n++], COL_BUTTONS, IconGet ("ic-add-48"));
  btnNew.SetCbPushed (CbButtonPushed, &btnNew);
  btnNew.SetHotkey (SDLK_PLUS);
  AddWidget (&btnNew);

  free (layout);

  // Month selection button...
  btnMonth.Set (Rect (0, 0, CAL_W, CAL_Y - UI_BUTTONS_SPACE), COL_BUTTONS); // DARK_GREY);
  btnMonth.SetCbPushed (CbButtonPushed, &btnMonth);
  AddWidget (&btnMonth);

  // Month view...
  wdgCalendar.SetArea (Rect (CAL_X, CAL_Y, CAL_W, CAL_H));
  wdgCalendar.SetCursorFormat (COL_CALCURSOR);
  wdgCalendar.SetCbHandleEvent (CbCalendarHandleEvent, this);
  AddWidget (&wdgCalendar);
  //~ DrawCalendar ();

  // List view...
  wdgEvents.Setup (&viewData);
  wdgEvents.SetArea (Rect (
                      CAL_X + CAL_W + 16,
                      0,
                      UI_RES_X - (CAL_X + CAL_W + 16),
                      UI_RES_Y - UI_BUTTONS_HEIGHT - UI_BUTTONS_SPACE
                    ));
  wdgEvents.SetCbPushed (CbEventPushed);
  AddWidget (&wdgEvents);

  //~ // Dump (uninitialized) data...
  //~ INFO ("### calender:viewData (before initialization)...");
  //~ for (n = 0; n < MAX_CALS; n++) {
    //~ CCalFile *calFile = viewData.GetFile (n);
    //~ INFOF (("# %2i. %i, '%s'", n, calFile->IsDefined (), calFile->GetName ()));
  //~ }

  // Init data...
  for (n = 0; n < MAX_CALS; n++) {
    sprintf (key, "calendar.%i.name", n);
    val = EnvGet (key);
    if (val) {
      sprintf (key, "calendar.%i.color", n);
      viewData.SetFile (n, ToColor (EnvGetInt (key)), val);
      //~ viewData.LoadFile (n);
    }
  }

  //~ // Dump (initialized) data...
  //~ INFO ("### calender:viewData (after initialization)...");
  //~ for (n = 0; n < MAX_CALS; n++) {
    //~ CCalFile *calFile = viewData.GetFile (n);
    //~ INFOF (("# %2i. %i, '%s'", n, calFile->IsDefined (), calFile->GetName ()));
  //~ }
}



// ***** Data (re-)loading *****


void CScreenCalMain::DoUpdateFile (int fileNo) {

  // Sanity...
  if (!viewData.GetFile (fileNo)->IsDefined ()) return;

  // Invalidate data...
  viewData.InvalidateFile (fileNo);

  // Load entries...
  viewData.LoadCalEntries (fileNo);
  while (viewData.HaveError ()) {
    RunErrorBox (viewData.GetErrorMsg (), NULL, -1, FontGet (fntMono, 20));
    RunEditScreen (viewData.GetErrorFile (), viewData.GetErrorLine ());
    viewData.LoadCalEntries (fileNo);
  }
  DrawCalendar ();
  UiIterateNoWait ();
}


void CScreenCalMain::UpdateFile (int fileNo) {
  DoUpdateFile (fileNo);
  EventListUpdate ();
  EventListSetRefDate (viewData.GetRefDate ());
}


void CScreenCalMain::UpdateAllFiles () {
  // visual feedback in the beginning and after each file
  wdgEvents.SetItems (0);
  viewData.Clear ();
  DrawCalendar ();
  UiIterateNoWait ();
  for (int n = 0; n < MAX_CALS; n++) DoUpdateFile (n);
  EventListUpdate ();
  EventListSetRefDate (viewData.GetRefDate ());
}


void CScreenCalMain::SetRefDate (TDate d, bool scrollEventList) {
  char buf[32];
  TDate d0, _d;
  int n;
  bool otherMonth;

  //~ INFOF(("### SetRefDate (%i-%02i-%02i)", YEAR_OF(d), MONTH_OF(d), DAY_OF(d)));

  _d = viewData.GetRefDate ();

  // Set ref date in data and determine if a month switch is necessery...
  otherMonth = viewData.SetRefDate (d, WEEKS);

  // Update month and year button...
  // TRANSLATORS: Format of the calendar's month & year button.
  sprintf(buf, "%s %d", MonthName (MONTH_OF(d)), YEAR_OF(d));
  btnMonth.SetLabel (buf, WHITE, FontGet (fntNormal, 24));

  // Set cursor in month view...
  d0 = viewData.GetFirstDate ();
  n = DateDiffByDays (d, d0);
  //~ INFOF(("### firstDate = %i-%02i-%02i, refDate = %i-%02i-%02i, diff = %i",
        //~ YEAR_OF(d0), MONTH_OF(d0), DAY_OF(d0), YEAR_OF(d), MONTH_OF(d), DAY_OF(d), n));
  wdgCalendar.SetCursor (CellRect ((n % 7) + 1, (n / 7) + 1));

  // Eventually reload cal entries...
  if (otherMonth) {
    // Early visual feedback...
    viewData.Clear ();
    DrawCalendar ();
    EventListUpdate ();
    UiIterateNoWait ();
    // Load the data...
    for (int n = 0; n < MAX_CALS; n++) if (viewData.GetFile (n)->IsDefined ()) {
      viewData.LoadCalEntries (n);
      DrawCalendar ();
      UiIterateNoWait ();
    }
    EventListUpdate ();
  }

  // Scroll list view to right position & highlight the current day...
  EventListSetRefDate (d, _d, scrollEventList || otherMonth);
}



// ***** Drawing *****


void CScreenCalMain::DrawCalendar () {
  char buf[5];
  CCalEntry *calEntry;
  TDate d;
  TTime t0, t1;
  TTF_Font *fontHead, *fontCell;
  SDL_Surface *surf;
  SDL_Rect r, s;
  TColor col, colBar;
  int n, k, refMonth, idx, y0, y1;

  if (!surfCalendar) surfCalendar = CreateSurface (CAL_W, CAL_H);
  fontHead = FontGet (fntBold, 16);
  fontCell = FontGet (fntNormal, 16);
  surf = NULL;

  // Fill with grid color...
  SDL_FillRect (surfCalendar, NULL, ToUint32 (COL_CALGRID));

  // Top row...
  for (k = 0; k < 8; k++) {
    r = CellRect (k, 0);
    if (k > 0) {
      SurfaceSet (&surf, FontRenderText (fontHead, DayNameShort (k-1), COL_CALTEXT, COL_CALGRID));
      SurfaceBlit (surf, NULL, surfCalendar, &r);
    }
  }

  // Main rows...
  refMonth = MONTH_OF (viewData.GetRefDate ());
  d = viewData.GetFirstDate ();
  calEntry = viewData.GetFirstCalEntry ();
  for (n = 0; n < WEEKS; n++) {

    // calendar week ...
    r = CellRect (0, n+1);
    sprintf (buf, "%i", GetCalWeek (d));
    SurfaceSet (&surf, FontRenderText (fontCell, buf, COL_CALTEXT, COL_CALGRID));
    SurfaceBlit (surf, NULL, surfCalendar, &r);

    // Day cells...
    for (k = 0; k < 7; k++) {
      r = CellRect (k+1, n+1);
      col = (MONTH_OF (d) == refMonth) ? COL_CALMON : COL_CALBACK;
      SDL_FillRect (surfCalendar, &r, ToUint32 (col));

      // Draw occupation...
      while (calEntry && calEntry->GetDate () == d) {
        //~ INFOF (("## Cell of %i-%02i-%02i...", YEAR_OF(d), MONTH_OF(d), DAY_OF(d)));
        idx = calEntry->GetFile ()->GetIdx ();
        if (calEntry->IsAllDay ()) {
          s = Rect (r.x + 4 + 6 * idx, r.y + 4, 6, CELL_H - 9);
          colBar = ColorBlend (col, calEntry->GetFile ()->GetColor (), 0x80);
        }
        else {
          t0 = calEntry->GetTime ();
          t1 = t0 + calEntry->GetDur ();

          y0 = (t0 - TIME_OF(6, 0, 0)) * (CELL_H - 9) / TIME_OF(15, 0, 0) + 2;
          y1 = (t1 - TIME_OF(6, 0, 0)) * (CELL_H - 9) / TIME_OF(15, 0, 0) + 2;
          if (y0 < 4) y0 = 4;
          if (y0 > CELL_H-8) y0 = CELL_H-8;
          if (y1 <= y0) y1 = y0 + 1;
          if (y1 > CELL_H-7) y1 = CELL_H-7;
          s = Rect (r.x + 4 + 6 * idx, r.y + y0, 6, y1 - y0);

          colBar = calEntry->GetFile ()->GetColor ();
        }
        SDL_FillRect (surfCalendar, &s, ToUint32 (colBar));

        //~ INFOF (("# calEntry %i-%02i-%02i at %2i:%02i until %2i:%02i -> %i..%i: %s",
                //~ YEAR_OF(d), MONTH_OF(d), DAY_OF(d), HOUR_OF(t0), MINUTE_OF(t0), HOUR_OF(t1), MINUTE_OF(t1),
                //~ y0, y1, calEntry->GetMessage ()));

        calEntry = calEntry->GetNext ();
      }

      // Draw number...
      sprintf (buf, "%i", DAY_OF (d));
      SurfaceSet (&surf, FontRenderText (fontCell, buf, COL_CALTEXT));
      r.y += 1;
      r.w -= 4;
      SurfaceBlit (surf, NULL, surfCalendar, &r, 1, -1, SDL_BLENDMODE_BLEND);

      // Next day...
      d = DateIncByDays (d, 1);
    } // for (k...)
  } // for (n...)

  wdgCalendar.SetSurface (surfCalendar);
}


void CScreenCalMain::EventListUpdate () {
  CCalEntry *ce;
  TDate lastDate;
  int items, idx;

  // Pass 1: Count entries...
  items = 0;
  lastDate = 0;
  for (ce = viewData.GetFirstCalEntry (); ce; ce = ce->GetNext ()) {
    items++;
    if (ce->GetDate () != lastDate) {
      items++;    // header line
      lastDate = ce->GetDate ();
    }
    //~ INFOF (("### items = %3i, %s %s", items, TicksToString (DateTimeToTicks (ce->GetDate (), ce->GetTime ())), ce->GetMessage ()));
  }

  // Pass 2: Build listbox items...
  wdgEvents.SetItems (items);
  idx = 0;
  lastDate = 0;
  for (ce = viewData.GetFirstCalEntry (); ce; ce = ce->GetNext ()) {
    if (ce->GetDate () != lastDate) {
      wdgEvents.SetItem (idx++, NULL, (const char *) NULL, true, ce);    // add heading item
      lastDate = ce->GetDate ();
    }
    wdgEvents.SetItem (idx++, NULL, (const char *) NULL, false, ce);     // add as normal item
    //~ INFOF (("### idx++ = %3i, %s %s", idx, TicksToString (DateTimeToTicks (ce->GetDate (), ce->GetTime ())), ce->GetMessage ()));
  }
}


void CScreenCalMain::EventListSetRefDate (TDate d, TDate oldD, bool scrollTo) {
  int _idx, idx;

  idx = _idx = 0;
  while (idx < wdgEvents.GetItems () && ((CCalEntry *) wdgEvents.GetItem (idx)->data)->GetDate () < d) idx++;
  if (scrollTo) wdgEvents.ScrollTo (idx);
  if (d != oldD) {
    if (oldD > 0) {
      while (_idx < wdgEvents.GetItems () && ((CCalEntry *) wdgEvents.GetItem (_idx)->data)->GetDate () < oldD) _idx++;
      //~ INFOF(("# Titel colors changed: %i, %i.", _idx, idx));
      wdgEvents.ChangedItems (_idx);
    }
    wdgEvents.ChangedItems (idx);
  }
  Changed ();
}



// ***** Events *****


bool CScreenCalMain::CalendarHandleEvent (SDL_Event *ev) {
  SDL_Keycode key;
  SDL_Rect *area;
  int x, y, n, k;
  bool ret;

  ret = false;
  switch (ev->type) {
    case SDL_MOUSEBUTTONDOWN:
      if (ev->button.clicks != 1) break;  // only succeed on the first/short click to avoid uncontrolled page shifting on a long push
      //~ INFO ("### SDL_MOUSEBUTTONDOWN");
      wdgCalendar.GetMouseEventPos (ev, &x, &y);
      area = wdgCalendar.GetArea ();
      if (RectContains (area, x, y)) {
        k = (x - area->x - CELL0_W) / CELL_W;
        n = (y - area->y - CELL0_H) / CELL_H;
        if (k >= 0 && k < 7 && n >= 0 && n < WEEKS)
          SetRefDate (DateIncByDays (viewData.GetFirstDate (), 7 * n + k));
        ret = true;
      }
      break;
    //~ case SDL_MOUSEBUTTONUP:
      //~ INFO ("### SDL_MOUSEBUTTONUP");
      //~ break;
    case SDL_KEYDOWN:
      key = ev->key.keysym.sym;
      //~ INFOF (("### SDL_KEYDOWN: '%s'", SDL_GetKeyName (key)));
      if (ev->key.keysym.mod == KMOD_NONE) switch (key) {
        case SDLK_UP:
          SetRefDate (DateIncByDays (viewData.GetRefDate (), -7));
          break;
        case SDLK_DOWN:
          SetRefDate (DateIncByDays (viewData.GetRefDate (), +7));
          break;
        case SDLK_LEFT:
          SetRefDate (DateIncByDays (viewData.GetRefDate (), -1));
          break;
        case SDLK_RIGHT:
          SetRefDate (DateIncByDays (viewData.GetRefDate (), +1));
          break;
        case SDLK_PAGEUP:
          SetRefDate (DateIncByMonths (viewData.GetRefDate (), -1));
          break;
        case SDLK_PAGEDOWN:
          SetRefDate (DateIncByMonths (viewData.GetRefDate (), +1));
          break;
      }
      break;
  }
  return ret;
}


void CScreenCalMain::OnButtonPushed (CButton *b) {
  if (b == &btnPrevMon) {
    SetRefDate (DateIncByMonths (GetRefDate (), -1));
  }
  else if (b == &btnNextMon) {
    SetRefDate (DateIncByMonths (GetRefDate (), +1));
  }
  else if (b == &btnToday) {
    SetRefDate (Today ());
  }
  else if (b == &btnReload) {
    UpdateAllFiles ();
  }
  else if (b == &btnNew) {
    RunEditScreen ();
  }
  else if (b == &btnMonth) {
    RunMonthMenu ();
  }
}


void CScreenCalMain::OnEventPushed (int idx, CCalEntry *calEntry) {
  wdgEvents.ScrollIn (idx);
  if (!wdgEvents.GetItem (idx)->isSpecial)
    RunEditScreen (calEntry->GetFile ()->GetIdx (), calEntry->GetLineNo ());
  else
    SetRefDate (calEntry->GetDate (), false);
}


void CScreenCalMain::RunMonthMenu () {
  CMenu menuMon, menuYear;
  SDL_Rect *monFrame, *yearFrame, noCancelRect;
  char yearItems[12][10];
  int n, choiceMon, choiceYear, newYear, baseYear;
  bool buildYears, showYears, done;

  menuMon.Setup (Rect (0, UI_BUTTONS_HEIGHT, UI_RES_X, UI_RES_Y-UI_BUTTONS_HEIGHT), -1, -1, DARK_CYAN);
  menuMon.SetItems (12);
  for (n = 0; n < 12; n++) menuMon.SetItem (n, MonthName (n + 1));
  menuMon.Start (this);

  monFrame = menuMon.GetArea ();
  menuYear.Setup (Rect (monFrame->w, UI_BUTTONS_HEIGHT, UI_RES_X-monFrame->w, UI_RES_Y-UI_BUTTONS_HEIGHT), -1, -1, DARK_CYAN);
  menuYear.SetItems (12);

  newYear = YEAR_OF (viewData.GetRefDate ());   // default for the selected year
  baseYear = newYear - (newYear % 10);
  showYears = buildYears = true;
  done = false;
  while (!done) {
    if (buildYears) {
      strcpy (yearItems[0], "<<<");
      strcpy (yearItems[11], ">>>");
      for (n = 1; n < 11; n++) sprintf (yearItems[n], "%i", baseYear + n - 1);
      for (n = 0; n < 12; n++) menuYear.SetItem (n, yearItems[n]);
      buildYears = false;
    }

    if (showYears) {
      menuYear.Start (this);   // 'menuMon' already started at this point
      yearFrame = menuYear.GetArea ();
      noCancelRect = Rect (monFrame->x, monFrame->y, monFrame->w + yearFrame->w, monFrame->h);
      menuMon.SetNoCancelArea (noCancelRect);
      menuYear.SetNoCancelArea (noCancelRect);
    }

    while (menuMon.IsRunning () && (!showYears || menuYear.IsRunning ())) UiIterate ();

    choiceMon = menuMon.GetStatus ();
    choiceYear = showYears ? menuYear.GetStatus () : -1;
    if (choiceYear >= 0) {
      // A button in the year menu was pushed...
      if (choiceYear == 0) {
        baseYear -= 10;
        buildYears = true;
      }
      else if (choiceYear == 11) {
        baseYear += 10;
        buildYears = true;
      }
      else {
        newYear = baseYear + choiceYear - 1;
        SetRefDate (DATE_OF (newYear, MONTH_OF (viewData.GetRefDate ()), 1));
        showYears = false;
      }
      //~ INFOF(("### baseYear = %i", baseYear));
    }
    else if (choiceMon >= 0) {
      // A button in the month menu was pushed...
      menuYear.Stop ();
      SetRefDate (DATE_OF (newYear, choiceMon + 1, 1));
      done = true;
    }
    else done = true;     // no choice => Menu was cancelled
  }
  menuMon.Stop ();
  menuYear.Stop ();
}


void CScreenCalMain::RunEditScreen (int _fileNo, int _lineNo) {
  CString *s;
  TDate d;

  SystemActiveLock ("_calendar");
  if (!scrCalEdit) scrCalEdit = new CScreenCalEdit ();

  // Load file...
  if (scrCalEdit->Setup (&viewData, _fileNo, _lineNo)) {
    if (_lineNo < 0) {
      d = GetRefDate ();
      s = scrCalEdit->wdgInput.GetInput ();
      s->SetF ("%i-%02i-%02i AT 8:00 DUR 1:00 *1 *7 UNTIL %i-%02i-%02i MSG ",
               YEAR_OF(d), MONTH_OF(d), DAY_OF(d), YEAR_OF(d), MONTH_OF(d), DAY_OF(d));
      scrCalEdit->wdgInput.ChangedInput ();
      scrCalEdit->wdgInput.SetMark (14, 4);
    }
    scrCalEdit->Run ();
  }
  SystemActiveUnlock ("_calendar");
}





// ***************** App function *******************


void *AppFuncCalendar (int appOp, void *data) {
  switch (appOp) {

    case appOpInit:
      envCalendarDir = EnvGetPath (envCalendarDirKey);   // make path absolute
      return APP_INIT_OK;

    case appOpDone:
      FREEO(scrCalMain);
      FREEO(scrCalEdit);
      break;

    case appOpLabel:
      APP_SET_LAUNCHER (data, "ic-today", _("Calendar"), SDLK_k);
      break;

    case appOpActivate:
      if (!scrCalMain) {
        scrCalMain = new CScreenCalMain ();
        scrCalMain->Setup ();
        scrCalMain->Activate ();
        scrCalMain->SetRefDate (Today ());
      }
      else
        scrCalMain->Activate ();
      scrCalMain->SetRefDate (Today ());
      break;
  }
  return NULL;
}
