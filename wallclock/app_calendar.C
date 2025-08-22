/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2015-2025 Gundolf Kiefer
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

#include <sys/socket.h>
#include <netdb.h>        // getaddrinfo
#include <arpa/inet.h>    // inet_pton()
#include <errno.h>


#define MAX_CALS 10   // maximum number of distinct calendars
#define WEEKS 7





// *************************** Environment options *****************************


ENV_PARA_NOVAR ("calendar.enable", bool, envCalendarEnable, false);
  /* Enable the calendar applet
   */
ENV_PARA_STRING ("calendar.host", envCalendarHost, NULL);
  /* Storage host with calendar files (local if unset)
   *
   * For remind calendars, the tool \texttt{cat} must be installed on the storage
   * and \texttt{GNU patch} and \texttt{remind} are required on the local host
   * (included in the Android app).
   *
   * For iCal calendars, \reftool{home2l-pimd} must be running and \texttt{nc}
   * (e.g. netcat-openbsd >= 1.2.19) be installed on the storage host.
   *
   * If a host is set, the application will use \texttt{ssh} to run any commands
   * on the host as user \texttt{'home2l'}. Hence, to access the calendars
   * as a unified user on the local machine, it is advisable to enter
   * \texttt{'localhost'} here. To run all commands directly without using
   * \texttt{ssh}, leave this unset or empty.
   */

ENV_PARA_BOOL ("calendar.remindRemote", envCalendarRemindRemote, false);
  /* Run 'remind' on the remote host and not locally.
   *
   * If set, \texttt{remind} and \texttt{patch} are executed on the remote host
   * and not locally. On a very slow network connection, this may improve speed.
   */
ENV_PARA_PATH ("calendar.remindDir", envCalendarRemindDir, "calendars");
  /* Storage directory for calendar (remind) files.
   *
   * The path may be either absolute or relative to \refenv{sys.varDir}.
   */

ENV_PARA_STRING ("calendar.icalSocket", envCalendarIcalSocket, NULL);
  /* Socket to communicate with \reftool{home2l-pimd}.
   *
   * This may be an absolute path to a Unix domain socket on the host
   * specified by \refenv{calendar.host}. In this case, \texttt{ssh} is used
   * to communicate with the host.
   *
   * Alternatively, it may be a host and/or port specification for a
   * TCP/IP socket such as 'pimdhost:4711'. Both the host and the port have to
   * be supplied.
   */

ENV_PARA_SPECIAL ("calendar.<n>.id", const char *, NULL);
  /* ID for calendar $\#n$
   *
   * For remind files, this is the base file name (without ".rem").
   * For iCal directories, this is the directory containing the iCal files
   * on the socket server specified by \refenv{calendar.icalSocket}.
   *
   * If the ID ends with a '/', the iCal backend is used, otherwise, a remind
   * file is expected.
   */
ENV_PARA_SPECIAL ("calendar.<n>.name", const char *, NULL);
  /* Display name for calendar $\#n$
   *
   * This optioal argument allows to set a user-friendly display name.
   * If unset, \refenv{calendar.<n>.id} is used.
   */
ENV_PARA_SPECIAL ("calendar.<n>.color", int, NODEFAULT);
  /* Color for calendar $\#n$
   *
   * This should by given as a 6-digit hex number in the form \texttt{0x<rr><gg><bb>}.
   */





// *****************************************************************************
// *                                                                           *
// *                          Model-related classes                            *
// *                                                                           *
// *****************************************************************************





// *************************** Headers *****************************************


enum ECalBackend {
  cbRemind = 0,
  cbIcal,
  cbEND
};


class CCalFile {
public:
  CCalFile () { isDefined = false; }
  ~CCalFile () { Clear (); }

  // Definition (always valid after initialization) ...
  void Setup (int _idx, ECalBackend _backend, const char *_id, TColor _color, const char *_name = NULL);

  bool IsDefined () { return isDefined; }
  int GetIdx () { return idx; }
  ECalBackend GetBackend () { return backend; }
  const char *GetId () { return id.Get (); }
  const char *GetName () { return name.Get (); }
  TColor GetColor () { return color; }

  // Source lines as they are editable in the editor (loaded on demand by the 'remind' backend) ...
  void Clear ();                              // Clear loaded data
  void AppendLine (const char *line);         // Append a line during loading

  int GetLines () { return lineList.Entries (); }
  const char *GetLine (int n) { return lineList[n]->Get (); }

protected:
  bool isDefined;

  int idx;          // 'idx' = numerical identifier
  ECalBackend backend;
  CString id, name; // 'id' = identifier for the backend (e.g. file name); 'name' = display name
  TColor color;     // display color

  CListCompact<CString> lineList;
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
  int lineNo;

  CCalEntry *next;
};


class CCalViewData {
public:
  CCalViewData ();
  ~CCalViewData ();

  void Clear () { DelCalEntries (); }

  void SetupFile (int fileNo, const char *id, TColor color, const char *name = NULL);
  CCalFile *GetFile (int fileNo) { return &calFileArr[fileNo]; }

  void LoadCalEntries (int fileNo);
    // (Re-)load cal entries related to a file and make sure that the lines are loaded in the file object
  void LoadAllCalEntries ();
    // (Re-)load cal entries related to all files (e.g. after a reference date change)

  bool ChangeFile (int fileNo, int lineNo, const char *newEntry);
    // Change (delete / add / change entry) a  file entry.
    // Afterwards, the file is invalidated (and not reloaded).
    // The change is written back to the backend storage, but the file object is not automatically reloaded.
    // Delete an entry: If 'newEntry' == NULL, the old entry identified by 'fileNo'/'lineNo' is deleted.
    // Add a new entry: If 'lineNo' < 0, a new entry is added ('newEntry' must be defined then).
    // Change an entry: If 'lineNo' >= 0, the existing entry is changed.
    // On error, an error box is shown, and false is returned.

  bool SetRefDate (TDate _refDate);
     // Sets the current reference date and (re-)invokes remind if necessary.
     // Also decides about the actual time interval represented by the view,
     // e.g. the full month of the passed reference date including a preceeding and
     // one or two succeeding weeks.
     // Returns true if something has changed (e.g. caller must invoke 'LoadAllCalEntries' and update its UI).

  TDate GetRefDate () { return refDate; }
  TDate GetFirstDate () { return firstDate; }

  CCalEntry *GetFirstCalEntry () { return firstEntry; }
  CCalEntry *GetFirstCalEntryOfDate (TDate date);

  void ClearError () { errorFile = -1; }
  bool HaveError () { return errorFile >= 0; }
  const char *GetErrorMsg () { return errorMsg.Get (); }
  int GetErrorFile () { return errorFile; }
  int GetErrorLine () { return errorLine; }

protected:
  void AddCalEntries (CCalEntry *newList);        // add a new list of unsorted entries in correct timed order
  void DelCalEntries (CCalFile *calFile = NULL);  // delete all entries / only those related to file 'calFile'

  bool RemindLoadFile (int fileNo);
  CCalEntry *RemindLoadCalEntries (int fileNo);
  bool RemindChangeFile (int fileNo, int lineNo, const char *newEntry);

  bool IcalCommunicate (CCalFile *calFile, const char *cmd, CString *ret);
  void IcalShowPimdError (const char *line);
  CCalEntry *IcalLoadCalEntries (int fileNo);
  bool IcalChangeFile (int fileNo, int lineNo, const char *newEntry);

  CCalFile calFileArr[MAX_CALS];
  CCalEntry *firstEntry;

  CShellSession shellRemote, shellLocal;
  CString pimdSocket;   // resolved host or path of the socket file (pimdPort < 0)
  int pimdPort;         // port on the resolved host or < 0 if a Unix domain socket is used

  int errorFile, errorLine;
  CString errorMsg;

  TDate refDate, firstDate;
};





// *************************** CCalFile ****************************************



void CCalFile::Setup (int _idx, ECalBackend _backend, const char *_id, TColor _color, const char *_name) {
  idx = _idx;
  backend = _backend;
  color = _color;
  id.Set (_id);
  if (_name) name.Set (_name);
  else name.SetC (id.Get ());
  isDefined = true;
}


void CCalFile::Clear () {
  //~ INFOF (("### Invalidating '%s' ...", name.Get ()));
  lineList.Clear ();
}


void CCalFile::AppendLine (const char *line) {
  CString s;

  s.SetC (line);
  lineList.Append (&s);
}





// *************************** CCalViewData ************************************


CCalViewData::CCalViewData () {
  CString s;

  firstEntry = NULL;
  errorFile = -1;
  firstDate = refDate = 0;
  if (envCalendarHost) if (envCalendarHost[0]) {
    EnvNetResolve (envCalendarHost, &s);
    shellRemote.SetHost (s.Get ());
  }
  pimdPort = -1;
  if (envCalendarIcalSocket) {
    if (envCalendarIcalSocket[0] == '/')
      pimdSocket.Set (envCalendarIcalSocket);
    else
      EnvNetResolve (envCalendarIcalSocket, &pimdSocket, &pimdPort);
  }
}


CCalViewData::~CCalViewData () {
  Clear ();
}


void CCalViewData::SetupFile (int fileNo, const char *id, TColor color, const char *name) {
  ECalBackend backend;

  backend = id [strlen (id) - 1] == '/' ? cbIcal : cbRemind;
  if (backend == cbIcal) {
    if (pimdSocket.IsEmpty ()) {
      WARNINGF(("No iCal socket defined: Ignoring calendar '%s'.", id));
      return;
    }
  }
  calFileArr[fileNo].Setup (fileNo, backend, id, color, name);
}


bool CCalViewData::SetRefDate (TDate _refDate) {
  TDate _firstDate, firstOfMonth;
  bool update;

  firstOfMonth = DateFirstOfMonth (_refDate);
  _firstDate = DateIncByDays (firstOfMonth, -GetWeekDay (firstOfMonth) - 7);
  update = (_firstDate != firstDate);

  refDate = _refDate;
  firstDate = _firstDate;

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

  while (newList) {
    pCe = &firstEntry;
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


void CCalViewData::LoadCalEntries (int fileNo) {
  CCalFile *calFile = &calFileArr[fileNo];
  CCalEntry *list;

  // Sanity ...
  ASSERT(fileNo >= 0 && calFileArr[fileNo].IsDefined ());
  //~ INFOF (("### LoadCalEntries(%i)", fileNo));

  // Delegate to backend handler ...
  DelCalEntries (calFile);
  switch (calFile->GetBackend ()) {
    case cbRemind:
      list = RemindLoadCalEntries (fileNo);
      break;
    case cbIcal:
      list = IcalLoadCalEntries (fileNo);
      break;
    default:
      ASSERT (false);
  }
  AddCalEntries (list);
}


bool CCalViewData::ChangeFile (int fileNo, int lineNo, const char *newEntry) {
  CCalFile *calFile = &calFileArr[fileNo];
  bool ok = false;

  // Sanity ...
  ASSERT(fileNo >= 0 && calFileArr[fileNo].IsDefined ());

  // Delegate to backend handler ...
  switch (calFile->GetBackend ()) {
    case cbRemind:
      ok = RemindChangeFile (fileNo, lineNo, newEntry);
      break;
    case cbIcal:
      ok = IcalChangeFile (fileNo, lineNo, newEntry);
      break;
    default:
      ASSERT (false);
  }

  // Done ...
  return ok;
}





// ***** Backend: Remind *****


bool CCalViewData::RemindLoadFile (int fileNo) {
  CString s, cmd, line;
  CCalFile *calFile = &calFileArr[fileNo];
  int n, lines;

  // Sanity...
  ASSERT (fileNo >= 0 && fileNo < MAX_CALS);
  if (calFile->GetLines () > 0) return true;      // file already loaded

  //~ INFOF (("### Loading '%s' ...", name.Get ()));

  // Open file...
  cmd.SetF ("cat %s/%s.rem", envCalendarRemindDir, calFile->GetId ());
  DEBUGF (1, ("Running '%s' on '%s' ...", cmd.Get (), shellRemote.Host ()));
  shellRemote.Start (cmd.Get (), true);
  shellRemote.WriteClose ();   // we are not going to write anything

  // Read loop...
  calFile->Clear ();
  while (!shellRemote.ReadClosed ()) {
    shellRemote.WaitUntilReadable ();
    while (shellRemote.ReadLine (&line)) {
      calFile->AppendLine (line.Get ());
      //~ INFOF(("# Read '%s'.", line.Get ()));
    }
  }

  // Complete...
  shellRemote.Wait ();
  if (shellRemote.ExitCode ()) {
    WARNINGF (("Command '%s' on '%s' exited with error (%i)", cmd.Get (), shellRemote.Host (), shellRemote.ExitCode ()));
    StringF (&s, _("Failed to load calendar file '%s/%s.rem':\n"),
                   envCalendarRemindDir, calFile->GetId ());
    lines = calFile->GetLines ();
    if (!lines) s.AppendF (_("\n(no output)"));
    else {
      for (n = 0; n < lines && n < 10; n++) s.AppendF ("\n%s", calFile->GetLine (n));
      if (n < lines) s.Append ("\n...");
    }
    calFile->Clear ();
    RunErrorBox (s, NULL, -1, FontGet (fntMono, 20));
    return false;
  }
  //~ INFOF (("Done with command '%s'", cmd.Get ()));
  return true;
}


CCalEntry *CCalViewData::RemindLoadCalEntries (int fileNo) {
  CCalEntry *first, **pLast, *ce;
  CString cmd, line;
  CShellSession *shell;
  char strTime[32], strDur[32], *p;
  int sendLine, lineNo, n, msgPos, year, mon, day;
  bool canSend, canReceive;

  // Build command line for remind & start ...
  if (!envCalendarRemindRemote) {

    // Normal case: Load file locally and pipe it through a local remind instance ...
    if (!RemindLoadFile (fileNo)) return NULL;
#if !ANDROID
    cmd.SetF ("remind -l -ms+%i -b2 -gaaad - %i-%02i-%02i",
              WEEKS, YEAR_OF(firstDate), MONTH_OF(firstDate), DAY_OF(firstDate));
#else
    cmd.SetF ("%s/bin/remind -l -ms+%i -b2 -gaaad - %i-%02i-%02i",
              EnvHome2lRoot (),
              WEEKS, YEAR_OF(firstDate), MONTH_OF(firstDate), DAY_OF(firstDate));
#endif
    shell = &shellLocal;
  }
  else {

    // Remote processing: Let remind load the file on the remote machine ...
    cmd.SetF ("remind -l -ms+%i -b2 -gaaad %s/%s.rem %i-%02i-%02i",
              WEEKS,
              envCalendarRemindDir, calFileArr[fileNo].GetId (),
              YEAR_OF(firstDate), MONTH_OF(firstDate), DAY_OF(firstDate));
    shell = &shellRemote;
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
  if (envCalendarRemindRemote) shell->WriteClose ();     // we won't send input in remote mode
  while (!shell->ReadClosed ()) {
    shell->CheckIO (envCalendarRemindRemote ? NULL : &canSend, &canReceive);
    if (canSend) {      // only effective in "nearby" mode
      if (sendLine >= calFileArr[fileNo].GetLines ()) shell->WriteClose ();
      else shell->WriteLine (calFileArr[fileNo].GetLine (sendLine++));
    }
    if (canReceive) if (shell->ReadLine (&line)) {
      strTime[31] = strDur[31] = '\0';

      // Check for file/line number information...
      if (sscanf (line.Get (), "# fileinfo %d", &lineNo) == 1) { lineNo--; }

      // Check for calendar entry...
      else if (sscanf (line.Get (), "%d/%d/%d * * %31s %31s %n", &year, &mon, &day, strDur, strTime, &msgPos) >= 5) {

        //~ INFOF(("### Got: %s", line.Get ()));
        ce = new CCalEntry ();

        ce->date = DATE_OF(year, mon, day);
        if (sscanf (strTime, "%d", &n) == 1) ce->time = TIME_OF(0, n, 0); else ce->time = 0;
        if (sscanf (strDur, "%d", &n) == 1) ce->dur = TIME_OF(0, n, 0); else ce->dur = TIME_OF(24,0,0);
        if (ce->time + ce->dur > TIME_OF (24, 0, 0)) ce->dur = TIME_OF (24, 0, 0) - ce->time;
          // Note [2025-03-01]: If an event covers multiple days (e.g. "AT 12:00 dur 24:00"),
          //   'remind' generates separate outputs for each day. However, the duration is set
          //   to cover the current plus all following days. Here, we clip the duration to the
          //   end of the current day.
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
        WARNINGF (("Remind error: '%s'", line.Get ()));
        if (!HaveError ()) {
          errorFile = fileNo;
          errorLine = n - 1;
          p = strchr (line, ':');
          if (p) do  { p++; } while (*p != ' ');
          errorMsg.Set (p ? p : line.Get ());
        }
      }
      else WARNINGF (("Unparsable line in remind output while processing '%s': %s",
                      calFileArr[fileNo].GetId (), line.Get ()));
    }
  }
  shell->Wait ();
  if (shell->ExitCode ())
    WARNINGF (("Command '%s' exited with error (%i)", cmd.Get (), shell->ExitCode ()));
  return first;
}


bool CCalViewData::RemindChangeFile (int fileNo, int lineNo, const char *newEntry) {
  CString s, patch, msg;
  CCalFile *calFile;
  int _lineNo, oldLines, newLines, exitCode;

  // Sanity ...
  ASSERT (fileNo >= 0);
  calFile = GetFile (fileNo);

  // Create patch for file  ...
  oldLines = (lineNo >= 0) ? 1 : 0;
  newLines = newEntry ? 1 : 0;
  //   ... write header ...
  _lineNo = lineNo >= 0 ? lineNo : calFile->GetLines ();   // append new lines at end of file
  patch.SetF ("--- old/%s.rem\n+++ new/%s.rem\n@@ -%i,%i +%i,%i @@",
              calFile->GetId (), calFile->GetId (),
              _lineNo + 1, oldLines, _lineNo + 1, newLines);
  //   ... write old and new line as applicable ...
  if (oldLines > 0) patch.Append (StringF (&s, "\n-%s", calFile->GetLine (lineNo)));
  if (newLines > 0) patch.Append (StringF (&s, "\n+%s", newEntry));

  // Output/apply patch...
  if (!ANDROID || shellRemote.HasHost ())
    exitCode = shellRemote.Run (StringF (&s, "cd %s; patch -ubNp1", envCalendarRemindDir), patch.Get (), &msg);
  else
    exitCode = shellRemote.Run (StringF (&s, "cd %s; %s/bin/patch -ubNp1", envCalendarRemindDir, EnvHome2lRoot ()), patch.Get (), &msg);
  if (exitCode != 0) {
    RunErrorBox (msg.Get (), NULL, -1, FontGet (fntMono, 20));
    return false;
  }
  return true;
}





// ***** Backend: iCal *****


bool CCalViewData::IcalCommunicate (CCalFile *calFile, const char *cmd, CString *ret) {
  // Submit a command to 'home2l-pimd' and return its output in 'ret'.
  // On error, an error box is shown, and 'false' is returned.
  CString s, netcatCmd, part, errMsg;
  char buf[4096];
  struct addrinfo aHints, *aInfo;
  struct sockaddr_in sockAdr, *pSockAdr;
  uint32_t ip4Adr = 0;      // IPv4 adress in network order
  ssize_t bytes;
  int fd, errNo;

  // Sanity ...
  ret->Clear ();

  // Communicate using SSH and Unix socket ...
  if (pimdPort < 0) {

    // Submit command ...
    netcatCmd.SetF ("nc -NU %s", pimdSocket.Get (), calFile->GetId ());
    shellRemote.Start (netcatCmd.Get (), true);
    DEBUGF (1, ("Running '%s' on '%s:%s' ...", cmd, shellRemote.Host (), envCalendarIcalSocket));
    shellRemote.WriteLine (cmd);
    shellRemote.WriteClose ();   // we are done with writing

    // Read loop ...
    while (!shellRemote.ReadClosed ()) {
      shellRemote.WaitUntilReadable ();
      while (shellRemote.ReadLine (&part)) {
        ret->Append (part);
        ret->Append ('\n');
      }
    }

    // Complete...
    shellRemote.Wait ();
    if (shellRemote.ExitCode ()) {
      WARNINGF (("Failed to contact 'home2l-pimd' on '%s': Netcat exited with error (%i). Command was: %s",
                 shellRemote.Host (), shellRemote.ExitCode (), cmd));
      RunErrorBox (StringF (&s, _("Failed to contact 'home2l-pimd', command failed:\n%s"), shellRemote.Host (), cmd));
      return false;
    }
  }

  // Communicate directly using TCP/IP socket ...
  else {

    // Resolve hostname if required...
    CLEAR (aHints);
    aHints.ai_family = AF_INET;     // we only accept ip4 adresses
    aHints.ai_socktype = SOCK_STREAM;
    errNo = getaddrinfo (pimdSocket.Get (), NULL, &aHints, &aInfo);
    if (errNo) {
      errMsg.Set (gai_strerror (errNo));
      //freeaddrinfo (aInfo);aInfo = NULL;    // on Android, 'freeaddrinfo ()' here leads to a segfault
    }
    else {
      // Success: Store the address info...
      pSockAdr = (struct sockaddr_in *) aInfo->ai_addr;
      ip4Adr = pSockAdr->sin_addr.s_addr;
      freeaddrinfo (aInfo);
    }

    // Create socket ...
    if (errMsg.IsEmpty ()) {
      fd = socket (AF_INET, SOCK_STREAM, 0);
      if (fd < 0) errMsg.SetF ("Failed to create socket: %s", strerror (errno));
      //~ fcntl (fd, F_SETFL, O_NONBLOCK);    // make non-blocking
    }

    // Connect ...
    if (errMsg.IsEmpty ()) {
      //~ DEBUGF (2, ("# Connecting to '%s'...", hostId.Get ()));
      CLEAR (sockAdr);
      sockAdr.sin_family = AF_INET;
      ASSERT (ip4Adr != 0);
      sockAdr.sin_addr.s_addr = ip4Adr;
      sockAdr.sin_port = htons (pimdPort);
      if (connect (fd, (sockaddr *) &sockAdr, sizeof (sockAdr)) < 0) {
        errMsg.SetF ("Failed to connect to socket: %s", strerror (errno));
        close (fd);
        fd = -1;
      }
    }

    // Submit command ...
    if (errMsg.IsEmpty ()) {
      s.SetF ("%s\nq\n", cmd);
      bytes = s.Len () + 1;   // include the trailing '\0' as an EOF marker
      if (send (fd, s.Get (), bytes, 0) != bytes)
        errMsg.SetF ("Failed to send '%s'.", cmd);
    }

    // Receive reply ...
    if (errMsg.IsEmpty ()) {
      bytes = 1;
      while (bytes > 0) {
        bytes = recv (fd, buf, sizeof (buf) - 1, 0);
        buf[bytes] = '\0';
        ret->Append (buf);
      }
    }

    // Close and print eventual error ...
    if (errMsg.IsEmpty ()) {
      close (fd);
    }
    else {
      WARNINGF (("Failed to contact 'home2l-pimd' on '%s:%i': %s. Command was: %s",
                 pimdSocket.Get (), pimdPort, errMsg.Get (), cmd));
      RunErrorBox (StringF (&s, _("Failed to contact 'home2l-pimd' on %s:%i:\n%s"),
                            pimdSocket.Get (), pimdPort, errMsg.Get ()));
      return false;
    }
  }

  // Done ...
  return true;
}


void CCalViewData::IcalShowPimdError (const char *line) {
  // Check if 'line' contains a warning or error message and eventually
  // display a dialog to show it. Returns 'true', if an error message has been displayed.
  if (line[1] == ':') {
    switch (tolower (line[0])) {
      case 'w': RunWarnBox (line + 2); return;
      case 'e': RunErrorBox (line + 2); return;
    }
  }
  RunErrorBox (line);
}


CCalEntry *CCalViewData::IcalLoadCalEntries (int fileNo) {
  CCalFile *calFile = &calFileArr[fileNo];
  CCalEntry *first, **pLast, *ce, *ceNew;
  CSplitString lineList;
  CString s, line, cmd, output;
  TDate endDate;
  int year, mon, day, lastYear, lastMon, lastDay, atHour, atMin, durHour, durMin;
  int n, lineNo, msgPos;

  // Sanity...
  ASSERT (fileNo >= 0 && fileNo < MAX_CALS);

  // Run command ...
  endDate = DateIncByDays (firstDate, WEEKS * 7);
  cmd.SetF ("? %s %04i-%02i-%02i %04i-%02i-%02i",
            calFile->GetId (),
            YEAR_OF(firstDate), MONTH_OF(firstDate), DAY_OF(firstDate),
            YEAR_OF(endDate), MONTH_OF(endDate), DAY_OF(endDate)
            );
  if (!IcalCommunicate (calFile, cmd.Get (), &output)) return NULL;

  // Process output ...
  calFile->Clear ();
  first = NULL;
  pLast = &first;
  lineList.Set (output.Get (), INT_MAX, "\n");
  for (n = 0; n < lineList.Entries (); n++) {
    line.Set (lineList [n]);
    lineNo = calFile->GetLines ();
    calFile->AppendLine (line.Get ());
    line.Strip ();
    //~ INFOF(("# Read '%s'.", line.Get ()));
    ce = NULL;

    // Handle empty lines and end marker ...
    if (!line[0] || line[0] == '.') {
      // Just ignore the line: an EOF will follow anyway.
    }

    // Handle error lines ...
    else if (line[1] == ':') {
      IcalShowPimdError (line.Get ());
    }

    // Check for normal event ...
    //   Note: The following 'sscanf' calls must be ordered by descreasing number of arguments,
    //   since otherwise a misinterpretation may happen of a small number of args match a longer line.
    else if (sscanf (line.Get (), "%d-%d-%d AT %d:%d DUR %d:%d MSG %n",
                      &year, &mon, &day, &atHour, &atMin, &durHour, &durMin, &msgPos
                    ) == 7) {
      ce = new CCalEntry;
      ce->date = DATE_OF(year, mon, day);
      ce->time = TIME_OF(atHour, atMin, 0);
      ce->dur = TIME_OF(durHour, durMin, 0);
    }

    // Check for multi-day event ...
    else if (sscanf (line.Get (), "%d-%d-%d *1 UNTIL %d-%d-%d MSG %n",
                      &year, &mon, &day, &lastYear, &lastMon, &lastDay, &msgPos
                    ) == 6) {
      ce = new CCalEntry;
      ce->date = DATE_OF(year, mon, day);
      ce->time = TIME_OF(0, 0, 0);
      ce->dur = TIME_OF((DateDiffByDays (DATE_OF(lastYear, lastMon, lastDay), ce->date) + 1) * 24, 0, 0);
    }

    // Check for all-day event ...
    else if (sscanf (line.Get (), "%d-%d-%d MSG %n",
                      &year, &mon, &day, &msgPos
                    ) == 3) {
      ce = new CCalEntry;
      ce->date = DATE_OF(year, mon, day);
      ce->time = TIME_OF(0, 0, 0);
      ce->dur = TIME_OF(24, 0, 0);
    }

    // Default: Error
    else
      RunErrorBox (line.Get ());

    // Complete entry 'ce' and insert it, eventually splitting multi-day events ...
    ceNew = NULL;
    while (ce) {
      ce->calFile = &calFileArr[fileNo];
      ce->lineNo = lineNo;
      ce->msg.Set (line.Get () + msgPos);
      if (ce->time + ce->dur <= TIME_OF (24,0,0)) {
        ceNew = ce;
        ce = NULL;
      }
      else {

        // Split of new event for the first day ...
        ceNew = new CCalEntry;
        ceNew->date = ce->date;
        ceNew->time = ce->time;
        ceNew->dur = TIME_OF (24, 0, 0) - ce->time;
        ceNew->calFile = &calFileArr[fileNo];
        ceNew->lineNo = ce->lineNo;
        ceNew->msg.Set (ce->msg.Get ());

        // Adapt 'ce' to cover the remaining day(s) ...
        ce->date = DateIncByDays (ce->date, 1);
        ce->time = TIME_OF (0, 0, 0);
        ce->dur -= ceNew->dur;
      }

      // Append 'ceNew' to list ...
      ceNew->next = NULL;
      *pLast = ceNew;
      pLast = &(ceNew->next);
    }   // for (n = 0; n < output.Entries (); n++)
  }

  //~ INFOF (("Done with command '%s'", cmd.Get ()));
  return first;
}


bool CCalViewData::IcalChangeFile (int fileNo, int lineNo, const char *newEntry) {
  CCalFile *calFile;
  CString s, cmd, output;
  const char *line, *p;

  calFile = GetFile (fileNo);

  // Create command for "delete" case ...
  if (!newEntry) {
    line = calFile->GetLine (lineNo);
    p = strrchr (line, '@');
    if (!p) {
      RunErrorBox (StringF (&s, _("Missing event ID: %s"), line));
      return false;
    }
    cmd.SetF ("- %s %s\n", calFile->GetId (), p + 1);
  }

  // Create command for "add" and "change" cases ...
  else {
    cmd.SetF ("+ %s %s\n", calFile->GetId (), newEntry);
  }

  // Run command and show eventual warnings or errors ...
  if (!IcalCommunicate (calFile, cmd.Get (), &output)) return false;
  output.Strip (WHITESPACE"\n");
  if (output.Len () > 0) {
    IcalShowPimdError (output.Get ());
    return false;
  }

  // Done ...
  return true;
}





// *****************************************************************************
// *                                                                           *
// *                          View-related classes                             *
// *                                                                           *
// *****************************************************************************


#define CELL_W 64
#define CELL_H 60
#define CELL0_W 32
#define CELL0_H 28

#define CAL_W (CELL0_W + CELL_W * 7)            // 7 Days a week + 1st column for calweeks
#define CAL_H (CELL0_H + CELL_H * WEEKS)        // 1st row contains week day names
#define CAL_X 0
#define CAL_Y (UI_RES_Y - UI_BUTTONS_HEIGHT - UI_BUTTONS_SPACE - CAL_H)

#define COL_BUTTONS BROWN

#define COL_CALGRID GREY              // line grid and calendar weeks
#define COL_CALBACK DARK_GREY         // calendar background
#define COL_CALMON  BLACK             // background of current month
#define COL_CALTEXT WHITE             // month days and headers
#define COL_CALCURSOR ToColor (0xff, 0xff, 0x00, 0x60)
//~ #define COL_CALCURSOR ToColor (0xe0, 0x80, 0x30, 0x60)

#define COL_EVGRID DARK_GREY
#define COL_EVHEAD LIGHT_GREY
#define COL_EVBACK WHITE
#define COL_EVTEXT BLACK
#define COL_EVSELECTED YELLOW
//~ #define COL_EVSELECTED ToColor (0xc0, 0x70, 0x20)


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

    CButton btnTrash, btnCalNo;

    CCalViewData *viewData;
    int origFileNo, origLineNo, fileNo, lineNo;
    TDate date;

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
    CScreenCalMain () { surfCalendar = NULL; setRefDateRunning = false; lastUpdateAllFiles = NEVER; }
    ~CScreenCalMain () {}

    void Setup ();

    void UpdateFile (int fileNo);     // (re-)load a calendar file
    void UpdateAllFiles ();           // (re-)load all calendar files
    void UpdateOutdatedFiles ();      // update all calendar files if last loaded >5 minutes ago
    void HandleFileErrors ();         // let user correct errors during past 'Update*File' calls
                                      // and update the respective files again

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
    bool setRefDateRunning;       // flag to break recursion in 'SetRefDate()'
    CEventsBox wdgEvents;         // event list
    TTicks lastUpdateAllFiles;

    CCalViewData viewData;

    void DoUpdateFile (int fileNo);

    void DrawCalendar ();
    void EventListUpdate ();
    void EventListSetRefDate (TDate d, TDate oldD = 0, bool scrollTo = true);
    void RunMonthMenu ();
    void RunEditScreen (int _fileNo = -1, int _lineNo = -1);
};


static CScreenCalMain *scrCalMain = NULL;





// *************************** CEventsBox **************************************


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
  char *p;

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
    p = strrchr (str1, '@');
    if (p) {
      *p = '\0';
      str1.Strip ();
    }
    if (calEntry->IsAllDay ()) str2.Clear ();
    else {
      t0 = calEntry->GetTime ();
      t1 = t0 + calEntry->GetDur ();
      str2.SetF ("   %2i:%02i - %2i:%02i", HOUR_OF(t0), MINUTE_OF(t0), HOUR_OF(t1), MINUTE_OF(t1));
    }
    p = strchr (str1, ';');
    if (p) {
      str1.Del (p - str1.Get ());
      str2.Append ("   ");
      while (CharIsWhiteSpace (p[1])) p++;
      str2.Append (p + 1);
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





// *************************** CScreenCalEdit **********************************


bool CScreenCalEdit::Setup (CCalViewData *_viewData, int _fileNo, int _lineNo) {
  static const int userBtnWidth [] = { -1, -2 };
  CButton *userBtnList [] = { &btnTrash, &btnCalNo };
  CCalFile *calFile;

  // Take over data...
  viewData = _viewData;
  origFileNo = _fileNo;
  origLineNo = lineNo = _lineNo;
  if (origFileNo >= 0) fileNo = origFileNo;    // take last file number unless '_fileNo' is >= 0
  date = _viewData->GetRefDate ();

  // Buttons & layout ...
  CInputScreen::Setup (NULL,
      (fileNo >= 0 && lineNo >= 0) ? viewData->GetFile (fileNo)->GetLine (lineNo) : NULL,
      GREY, sizeof (userBtnList) / sizeof (userBtnList[0]), userBtnList, userBtnWidth
    );

  btnTrash.SetLabel (WHITE, "ic-delete_forever-48");

  calFile = viewData->GetFile (fileNo);
  btnCalNo.SetColor (ColorScale (calFile->GetColor (), 0xc0));
  btnCalNo.SetLabel (calFile->GetName ());

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
    menu.Setup (r, -1, -1, GREY);
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
  CString input;
  bool ok;

  // Change files ...
  if (commitNoDelete) {
    wdgInput.GetInput (&input);
    ok = true;
    if (origFileNo >= 0 && origFileNo != fileNo)
      ok = viewData->ChangeFile (origFileNo, origLineNo, NULL);
        // delete old entry in old file
    if (ok)
      ok = viewData->ChangeFile (fileNo, origFileNo != fileNo ? -1 : origLineNo, input.Get ());
        // add or change entry in new file
  }
  else {
    ok = viewData->ChangeFile (origFileNo, origLineNo, NULL);
      // delete entry
  }

  // Update screen ...
  if (ok) {
    Return ();
    UiIterateNoWait ();
    scrCalMain->UpdateFile (fileNo);
    if (origFileNo >= 0 && origFileNo != fileNo) {
      UiIterateNoWait ();
      scrCalMain->UpdateFile (origFileNo);
    }
  }
}





// *************************** CScreenCalMain **********************************



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
  const char *val, *name;
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
    sprintf (key, "calendar.%i.id", n);
    val = EnvGet (key);
    if (val) {
      sprintf (key, "calendar.%i.name", n);
      name = EnvGet (key);
      sprintf (key, "calendar.%i.color", n);
      viewData.SetupFile (n, val, ToColor (EnvGetInt (key)), name);
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

  // Clear data...
  viewData.GetFile (fileNo)->Clear ();

  // Load entries...
  viewData.LoadCalEntries (fileNo);
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
  lastUpdateAllFiles = TicksNow ();
}


void CScreenCalMain::UpdateOutdatedFiles () {
  if (lastUpdateAllFiles == NEVER || (TicksNow () > lastUpdateAllFiles + TICKS_FROM_SECONDS (5*60)))
    UpdateAllFiles ();
}


void CScreenCalMain::HandleFileErrors () {
  int ret, fileNo, lineNo;

  while (viewData.HaveError ()) {
    fileNo = viewData.GetErrorFile ();
    lineNo = viewData.GetErrorLine ();
    //~ RunErrorBox (viewData.GetErrorMsg (), NULL, -1, FontGet (fntMono, 20));
    ret = RunSureBox (_("Please correct:"), viewData.GetErrorMsg (), NULL, -1, FontGet (fntMono, 20));
    if (ret <= 0) return;
    RunEditScreen (fileNo, lineNo);
    viewData.LoadCalEntries (fileNo);
    DrawCalendar ();
    EventListUpdate ();
  }
}


void CScreenCalMain::SetRefDate (TDate d, bool scrollEventList) {
  char buf[32];
  TDate d0, _d;
  int n;
  bool otherMonth;

  // Check for recursion due to UI interactions ...
  if (setRefDateRunning) return;
  setRefDateRunning = true;
    // UiIterateNoWait () calls below may contain a recursive call of this function.
    // If this happens, we stop here. UI events will be ignored.

  //~ INFOF(("### SetRefDate (%i-%02i-%02i)", YEAR_OF(d), MONTH_OF(d), DAY_OF(d)));

  _d = viewData.GetRefDate ();

  // Set ref date in data and determine if a month switch is necessary...
  otherMonth = viewData.SetRefDate (d);

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
      //~ usleep (500000);
      UiIterateNoWait ();
    }
    EventListUpdate ();
  }

  // Scroll list view to right position & highlight the current day...
  EventListSetRefDate (d, _d, scrollEventList || otherMonth);

  // Done ...
  setRefDateRunning = false;
}



// ***** Drawing *****


void CScreenCalMain::DrawCalendar () {
  char buf[16];
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
  while (calEntry && calEntry->GetDate () < d) calEntry = calEntry->GetNext ();
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
  if (surf) SurfaceFree (surf);

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
    //~ INFOF (("### items = %3i, %s %s", items, TicksAbsToString (DateTimeToTicks (ce->GetDate (), ce->GetTime ())), ce->GetMessage ()));
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
    //~ INFOF (("### idx++ = %3i, %s %s", idx, TicksAbsToString (DateTimeToTicks (ce->GetDate (), ce->GetTime ())), ce->GetMessage ()));
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
    HandleFileErrors ();
  }
  else if (b == &btnNew) {
    RunEditScreen ();
    HandleFileErrors ();
  }
  else if (b == &btnMonth) {
    RunMonthMenu ();
  }
}


void CScreenCalMain::OnEventPushed (int idx, CCalEntry *calEntry) {
  wdgEvents.ScrollIn (idx);
  if (!wdgEvents.GetItem (idx)->isSpecial) {
    RunEditScreen (calEntry->GetFile ()->GetIdx (), calEntry->GetLineNo ());
    HandleFileErrors ();
  }
  else
    SetRefDate (calEntry->GetDate (), false);
}


void CScreenCalMain::RunMonthMenu () {
  CMenu menuMon, menuYear;
  SDL_Rect *monFrame, *yearFrame, noCancelRect;
  char yearItems[12][10];
  int n, choiceMon, choiceYear, newYear, baseYear;
  bool buildYears, showYears, done;

  menuMon.Setup (Rect (0, UI_BUTTONS_HEIGHT, UI_RES_X, UI_RES_Y-UI_BUTTONS_HEIGHT), -1, -1, COL_BUTTONS);
  menuMon.SetItems (12);
  for (n = 0; n < 12; n++) menuMon.SetItem (n, MonthName (n + 1));
  menuMon.Start (this);

  monFrame = menuMon.GetArea ();
  menuYear.Setup (Rect (monFrame->w, UI_BUTTONS_HEIGHT, UI_RES_X-monFrame->w, UI_RES_Y-UI_BUTTONS_HEIGHT), -1, -1, COL_BUTTONS);
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
  CScreenCalEdit *scrCalEdit = NULL;
  CString *s;
  TDate d;

  SystemActiveLock ("_calendar");
  scrCalEdit = new CScreenCalEdit ();
  if (scrCalEdit->Setup (&viewData, _fileNo, _lineNo)) {
    if (_lineNo < 0) {
      d = GetRefDate ();
      s = scrCalEdit->wdgInput.GetInput ();
      s->SetF ("%i-%02i-%02i AT 8:00 DUR 1:00 MSG ",
               YEAR_OF(d), MONTH_OF(d), DAY_OF(d), YEAR_OF(d), MONTH_OF(d), DAY_OF(d));
      scrCalEdit->wdgInput.ChangedInput ();
      scrCalEdit->wdgInput.SetMark (14, 4);
    }
    scrCalEdit->Run ();
  }
  FREEO (scrCalEdit);
  SystemActiveUnlock ("_calendar");
}





// *************************** App function ************************************


void *AppFuncCalendar (int appOp, void *data) {
  switch (appOp) {

    case appOpInit:
      EnvGetPath (envCalendarRemindDirKey, &envCalendarRemindDir, EnvHome2lVar ());   // make path absolute
      return APP_INIT_OK;

    case appOpDone:
      FREEO(scrCalMain);
      break;

    case appOpLabel:
      APP_SET_LAUNCHER (data, "ic-today", _("Calendar"), SDLK_k);
      break;

    case appOpActivate:
      if (!scrCalMain) {
        scrCalMain = new CScreenCalMain ();
        scrCalMain->Setup ();
        scrCalMain->Activate ();
      }
      else {
        scrCalMain->Activate ();
        scrCalMain->UpdateOutdatedFiles ();
          // Note: This will eventually also try to draw the calendar and the event list.
          //   For this reason, it must not be called before the first call of 'scrCalMain->SetRefDate()'.
      }
      scrCalMain->SetRefDate (Today ());
      scrCalMain->HandleFileErrors ();
      break;
  }
  return NULL;
}
