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


#include "rc_core.H"

#if WITH_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif





// *************************** Environment settings ****************************


ENV_PARA_STRING ("shell.historyFile", envHistFile, ".home2l_history");
  /* Name of the history file for the home2l shell, relative to the user's home directory
   */
ENV_PARA_INT ("shell.historyLines", envHistLines, 64);
  /* Maximum number of lines to be stored in the history file
   *
   * If set to 0, no history file is written or read.
   */
ENV_PARA_INT ("shell.stringChars", envStringChars, 64);
  /* Maximum number of characters to print for a string.
   *
   * If set to 0, strings are never abbreviated.
   */





// *************************** Helpers *****************************************


static CString uriPwd ("/");


static const char *RcGetAbsPath (const char *uri) {
  static CString ret;

  if (!uri) return uriPwd.Get ();
  if (!uri[0]) return uriPwd.Get ();
  if (uri[0] == '/') ret.Set (uri);
  else ret.SetF ("%s/%s", uriPwd.Get (), uri);
  ret.PathNormalize ();
  return ret.Get ();
}


static CDictRaw *RcGetDirectory (ERcPathDomain dom, CRcHost *rcHost, CRcDriver *rcDriver) {
  static CKeySet ret;
  CDictRaw *map;
  int n, items;

  switch (dom) {
    case rcpRoot:
      ret.Clear ();
      for (n = 0; n < RcGetUriRoots (); n++) ret.Set (RcGetUriRoot (n));
      map = &ret;
      break;
    case rcpAlias:
      map = &aliasMap;
      break;
    case rcpHost:
      ret.Clear ();
      for (n = 0; n < hostMap.Entries (); n++) ret.Set (hostMap.GetKey (n));
      ret.Set (localHostId.Get ());
      map = &ret;
      break;
    case rcpDriver:
      map = &driverMap;
      break;
    case rcpResource:
      ret.Clear ();
      map = &ret;
      if (rcHost) {
        items = rcHost->LockResources ();
        for (n = 0; n < items; n++) ret.Set (rcHost->GetResource (n)->Lid ());
        rcHost->UnlockResources ();
      }
      else if (rcDriver) {
        items = rcDriver->LockResources ();
        for (n = 0; n < items; n++) ret.Set (rcDriver->GetResource (n)->Lid ());
        rcDriver->UnlockResources ();
      }
      else map = NULL;
      break;
    case rcpEnv:
      //~ INFO ("rcpEnv");
      map = EnvGetKeySet ();
      break;
    default:
      map = NULL;
      break;
  }
  return map;
}





// *************************** Subscriber **************************************


static CRcSubscriber *subscriber = NULL;
static volatile bool interrupted = false;


static void SignalHandler (int) {
  //~ INFOF (("### SignalHandler: Entry"));
  interrupted = true;
  if (subscriber) subscriber->Interrupt ();
  //~ INFOF (("### SignalHandler: Exit"));
}


static void PollSubscriber () {
  CRcEvent ev;

  while (subscriber->PollEvent (&ev)) {
    printf (": %s\n", ev.ToStr ());
  }
}




// *************************** Command functions *******************************


static bool doQuit = false;


static void CmdHelp (int argc, const char **argv);   // implemented in section "Command interpreter"


static void CmdQuit (int argc, const char **argv) {
  doQuit = true;
}


//~ static void CmdWait (int argc, const char **argv) {
//~ }


static void CmdTypesInfo (int argc, const char **argv) {
  int n;
  int k;

  puts ("Basic types:");
  for (n = rctBasicTypesBase; n <= rctBasicTypesLast; n++)
    printf ("  %s\n", RcTypeGetName ((ERcType) n));
  puts ("\nSpecial types:");
  for (n = rctSpecialTypesBase; n <= rctSpecialTypesLast; n++)
    printf ("  %s\n", RcTypeGetName ((ERcType) n));
  puts ("\nPhysical/unit types:");
  for (n = rctUnitTypesBase; n <= rctUnitTypesLast; n++)
    printf ("  %-11s = <%s> %s\n", RcTypeGetName ((ERcType) n), RcTypeGetName (RcTypeGetBaseType ((ERcType) n)), RcTypeGetUnit ((ERcType) n) );
  puts ("\nEnumeration types:");
  for (n = rctEnumTypesBase; n <= rctEnumTypesLast; n++) {
    printf ("  %-11s = { %s", RcTypeGetName ((ERcType) n), RcTypeGetEnumValue ((ERcType) n, 0));
    for (k = 1; k < RcTypeGetEnumValues ((ERcType) n); k++)
      printf (", %s", RcTypeGetEnumValue ((ERcType) n, k));
    printf (" }\n");
  }
}


static void CmdNetworkInfo (int argc, const char **argv) {
  int n, k, verbosity;
  bool optWithSubscribers, optWithAgents, optWithResources;

  // Parse options...
  optWithSubscribers = optWithAgents = optWithResources = false;
  for (n = 1; n < argc; n++) for (k = 0; argv[n][k]; k++) switch (argv[n][k]) {
    case 's': optWithSubscribers = true; break;
    case 'r': optWithResources = true; break;
  }

  // Print info...
  verbosity = optWithResources ? 2 : optWithSubscribers ? 1 : 0;
  CRcHost::PrintInfoAll (stdout, verbosity);
  CRcServer::PrintInfoAll (stdout, verbosity);
}


static void CmdList (int argc, const char **argv) {
  CString s;
  ERcPathDomain dom;
  CRcHost *rcHost;
  CRcDriver *rcDriver;
  CResource *resource;
  CRcValueState vs;
  CDictRaw *map;
  const char *uri, *localPath, *key;
  int n, k;
  bool printList, optAllowNet;

  // Parse arguments...
  localPath = NULL;
  optAllowNet = true;
  for (n = 1; n < argc; n++) {
    if (argv[n][0] == '-') {
      for (k = 1; argv[n][k]; k++) switch (argv[n][k]) {
        case 'l': optAllowNet = false;
      }
    }
    else if (!localPath) localPath = argv[n];
  }

  // Analyse path...
  uri = localPath ? RcGetAbsPath (localPath) : uriPwd.Get ();
  uri = RcGetRealPath (&s, uri);
  //~ INFOF (("### CmdList -> RcAnalysePath"));
  dom = RcAnalysePath (uri, &localPath, &rcHost, &rcDriver, &resource, true);
  //~ INFOF (("### ... RcAnalysePath done"));

  // Print single object...
  printList = false;
  switch (dom) {
    case rcpHost:
      if (rcHost) rcHost->PrintInfo ();       // we have a single hit => print detailed info
      else printList = true;
      break;
    case rcpDriver:
      if (rcDriver) rcDriver->PrintInfo ();   // we have a single hit => print detailed info
      else printList = true;
      break;
    case rcpResource:
      //~ INFOF (("### CmdList -> rcpResource"));
      if (resource) {
        resource->PrintInfo (stdout, 1, optAllowNet);   // we have a single hit => print detailed info
        if (resource->Type () == rctString) {
          resource->GetValueState (&vs);
          if (vs.IsValid () && vs.Type () == rctString) {
            n = strlen (vs.String ());
            if (envStringChars <= 0 || n <= envStringChars)
              printf ("  = \"%s\"\n", vs.String ());
            else
              printf ("  = \"%.*s...\" (truncated after %i characters)\n",
                      envStringChars, vs.String (), envStringChars);
          }
        }
      }
      else printList = true;
      break;
    default:
      printList = true;
  }

  // Print list (if applicable)...
  if (printList) {
    map = RcGetDirectory (dom, rcHost, rcDriver);
    if (map) for (n = 0; n < map->Entries (); n++) {
      key = map->GetKey (n);
      if (strncmp (localPath, key, strlen (localPath)) == 0) {
        switch (dom) {
          case rcpAlias:
            printf ("%s -> %s\n", aliasMap.GetKey (n), aliasMap.Get (n)->Get ());
            break;
          case rcpEnv:
            printf ("%s = %s\n", key, EnvGet (key));
            break;
          default:
            printf ("%s\n", key);
        }
      }
    }
  }

  //~ INFOF (("dom = %i, rcHost = %i, rcDriver = %i, resource = %i, map = %i", dom, rcHost ? 1 : 0, rcDriver ? 1 : 0, resource ? 1 : 0, map ? map->Entries () : 0));
}


static void CmdChDir (int argc, const char **argv) {
  //~ printf ("CmdChDir\n");
  if (argc >= 2) uriPwd.Set (RcGetAbsPath (argv[1]));
  if (uriPwd.IsEmpty ()) uriPwd = "/";
  if (RcPathIsDir (RcAnalysePath (uriPwd.Get (), NULL, NULL, NULL, NULL))) uriPwd.Append ('/');
  uriPwd.PathNormalize ();
  printf ("%s\n", uriPwd.Get ());
}


static void CmdSubscribe (int argc, const char **argv) {
  for (int n = 1; n < argc; n++) subscriber->AddResources (RcGetAbsPath (argv[n]));
  subscriber->PrintInfo ();
}


static void CmdUnsubscribe (int argc, const char **argv) {
  for (int n = 1; n < argc; n++) {
    if (argv[n][0] == '*' && argv[n][1] == '\0') subscriber->Clear ();    // Wildcard "*" deletes all.
    subscriber->DelResources (RcGetAbsPath (argv[n]));
  }
  subscriber->PrintInfo ();
}


static void CmdFollow (int argc, const char **argv) {
  char *endPtr;
  int timeLeft = 1;

  if (argc >= 2) {
    timeLeft = strtol (argv[1], &endPtr, 0);
    if (*endPtr != '\0' || argc > 2) {
      printf ("Syntax error.\n");
      return;
    }
  }
  while (!interrupted && timeLeft > 0) {
    subscriber->WaitEvent (NULL, argc >= 2 ? &timeLeft : NULL);
    PollSubscriber ();
    fflush (stdout);
  }
  putchar ('\n');
  //~ write (STDOUT_FILENO, "\n", 1);
}


static void CmdSetRequest (int argc, const char **argv) {
  // argv[1]: resource name (rel. path)
  // argv[2] .. argv[argc-1]: concatenate, then call 'CRcResource::SetFromStr ()
  CResource *rc = NULL;
  CRcRequest *req;
  CString reqDef;
  const char *rcUri;
  int n;
  bool ok;

  ok = (argc >= 3);
  if (!ok) printf ("Too few arguments.\n");

  if (ok) {

    // Lookup resource...
    rcUri = RcGetAbsPath (argv[1]);
    rc = RcGetResource (rcUri, false);
    if (!rc) {
      printf ("Invalid URI: '%s'\n", rcUri);
      ok = false;
    }
  }

  if (ok) {

    // Construct resource string...
    reqDef.Set (argv[2]);
    for (n = 3; n < argc; n++) {
      reqDef.Append (' ');
      reqDef.Append (argv[n]);
    }

    // Add request & print info...
    rc->WaitForRegistration ();
    //~ INFOF (("### reqDef = '%s'", reqDef.Get ()));
    req = new CRcRequest ();
    req->SetPriority (rcPrioStrong);
    req->SetFromStr (reqDef);
    rc->SetRequest (req);
    rc->PrintInfo ();
  }
}


static void CmdDelRequest (int argc, const char **argv) {
  // argv[1]: resource name (rel. path)
  // argv[2]: request ID (optional)
  CResource *rc = NULL;
  const char *rcUri;
  bool ok;

  ok = (argc == 2 || argc == 3);
  if (!ok) printf ("Wrong number of arguments!\n");

  if (ok) {

    // Lookup resource...
    rcUri = RcGetAbsPath (argv[1]);
    rc = RcGetResource (rcUri, false);
    if (!rc) {
      printf ("Invalid URI '%s'", rcUri);
      ok = false;
    }
  }

  if (ok) {

    // Delete request & print info...
    rc->WaitForRegistration ();
    rc->DelRequest ((argc == 3) ? argv[2] : NULL);
    rc->PrintInfo ();
  }
}


static void ExecuteCmd (const char *cmd);


static void CmdRequestShortcut (int argc, const char **argv) {
  CString cmd;
  int n, ropt0;

  // Setup command...
  switch (argv[0][0]) {
    case '0':
    case '1':
      cmd.SetF ("r+ . %c", argv[0][0]);
      ropt0 = 1;
      break;
    case '!':
      if (argc < 2) {
        printf ("Missing value argument!\n");
        return;
      }
      cmd.SetF ("r+ . %s", argv[1]);
      ropt0 = 2;
      break;
    case '-':
      cmd.SetC ("r- .");
      ropt0 = argc;    // do not append anything
      break;
    default:
      ASSERT (false);
  }

  // Append request options...
  for (n = ropt0; n < argc; n++) cmd.AppendF (" %s", argv[n]);
  //~ printf ("### CMD = '%s'\n", cmd.Get ());

  // Execute command...
  ExecuteCmd (cmd.Get ());
}





// ************************* Main command interpreter **************************


typedef void (*TCmdFunc) (int argc, const char **argv);


struct TCmd {
  const char *name;     // command to type
  TCmdFunc func;
  const char *helpArgs, *helpText, *extraText;     // help: arguments, text
};


static const char *extraRequestShortcuts =
  "Examples for frequently useful request shortcuts:\n"
  "  Turn on some resource (e.g. a light):\n"
  "    > c /alias/my_light_to_test\n"
  "    > 1\n"
  "  Turn it off:\n"
  "    > 0\n"
  "  Keep our hands off it again:\n"
  "    > -\n"
  "  Simulate a button push of 500 ms:\n"
  "    > 1 -500\n"
  "  Simulate a button push of 500 ms, starting in 2 seconds:\n"
  "    > 1 +2000 -2500\n";


TCmd commandArr[] = {
  { "h", CmdHelp, "[<command>]", "Print help [on <command>]", NULL },
  { "help", CmdHelp, NULL, NULL, NULL },

  { "q", CmdQuit, "", "Quit", NULL },
  { "quit", CmdQuit, NULL, NULL, NULL },

  //~ { "w", CmdWait, "<ms>", "Wait for <ms> milliseconds", NULL },
  //~ { "wait", CmdWait, NULL, NULL, NULL },

  { "t", CmdTypesInfo, "", "List supported value types" },
  { "types", CmdTypesInfo, NULL, NULL, NULL },

  { "n", CmdNetworkInfo, "[<options>]", "Print network info",
          "Options:\n"
          "-s : Print subscribers\n"
          "-r : Also print resources for each subscriber\n" },
  { "network", CmdNetworkInfo, NULL, NULL, NULL },

  { "l", CmdList, "[<options>] [<path>]", "List object(s) [in <path>]",
          "Options:\n"
          "-l: Print local info on a resource\n"
          "\n"
          "The string of a string-typed resource is additionally printed unescaped,\n"
          "but only if the resource is local or subscribed to.\n" },
  { "list", CmdList, NULL, NULL },

  { "c", CmdChDir, "[<path>]", "Change or show working path", NULL },
  { "change", CmdChDir, NULL, NULL, NULL },

  { "s+", CmdSubscribe, "<rc>", "Subscribe to resource(s)", NULL },
  { "subscribe", CmdSubscribe, NULL, NULL, NULL },
  { "s-", CmdUnsubscribe, "<rc>", "Unsubscribe from resource(s)",
            "The special pattern '*' (usually not allowed) removes all subscriptions." },
  { "unsubscribe", CmdUnsubscribe, NULL, NULL, NULL },
  { "s", CmdSubscribe, "", "List subscriptions", NULL },
  { "subscriptions", CmdSubscribe, NULL, NULL, NULL },

  { "f", CmdFollow, "[<ms>]", "Follow subscriptions until Ctrl-C is pressed.",
            "Optionally, the commands stops automatically after <ms> milliseconds.\n"
            "This command can also be used to just wait for a certain time.\n" },
  { "follow", CmdFollow, NULL, NULL, NULL },

  { "r+", CmdSetRequest, "<rc> <value> [<ropts>]", "Add or change request",
            "Request options <attributes> :\n"
            "  <rc>    : Current resource identifier\n"
            "  <value> : Value to request\n"
            "  <ropts> : Additional request arguments as supported by 'CRcRequest::SetFromStr ()':\n"
            "             #<id>   : Request ID [default: 'shell']\n"
            "             *<prio> : Priority (0..15) [Default: 12 (rcPrioStrong)]\n"
            "             +<time> : Start time\n"
            "             -<time> : End time\n"
            "             ~<hyst> : Hysteresis in milliseconds\n"
            "\n"
            "The start/end times <time> may be given as absolute date/times in the format\n"
            "YYYY-MM-DD-HHMM[SS[.frac]] or a relative time <n>, where <n> is the number of\n"
            "milliseconds in the future.\n" },
  { "request", CmdSetRequest, NULL, NULL, NULL },
  { "r-", CmdDelRequest, "<rc> [#<reqGid>]", "Delete a request ('shell' if no <reqGid> is given)", NULL },
  { "unrequest", CmdDelRequest, NULL, NULL },

  { "0", CmdRequestShortcut, "[<ropts>]", "Shortcut for: r+ . 0 [<ropts>]", extraRequestShortcuts },
  { "1", CmdRequestShortcut, "[<ropts>]", "Shortcut for: r+ . 1 [<ropts>]", extraRequestShortcuts },
  { "!", CmdRequestShortcut, "<val> [<ropts>]", "Shortcut for: r+ . <val> [<ropts>]", extraRequestShortcuts },
  { "-", CmdRequestShortcut, "", "Shortcut for: r- .", extraRequestShortcuts }
};


#define commands ((int) (sizeof (commandArr) / sizeof (TCmd)))


static void CmdHelp (int args, const char **argv) {
  CString part;
  const char *cmd, *altCmd, *helpArgs, *helpText, *extraText;
  int n, k;
  bool selected;

  putchar ('\n');
  for (n = 0; n < commands; n++) {
    cmd = commandArr[n].name;
    helpArgs = commandArr[n].helpArgs;
    helpText = commandArr[n].helpText;
    extraText = commandArr[n].extraText;

    if (helpArgs && helpText) {
      altCmd = NULL;
      if (n < commands-1) if (!commandArr[n+1].helpText) altCmd = commandArr[n+1].name;

      if (args == 1) selected = true;
      else {
        selected = false;
        for (k = 1; k < args; k++) {
          if (strcmp (argv[k], cmd) == 0) { selected = true; break; }
          if (altCmd) if (strcmp (argv[k], altCmd) == 0) { selected = true; break; }
        }
      }

      //~ INFOF (("### cmd = '%s'/'%s', helpArgs = '%s', helpText = '%s'", cmd, altCmd, helpArgs, helpText));

      if (selected) {
        part.SetF (altCmd ? "%s|%s %s" : "%s %3$s", cmd, altCmd, helpArgs);
        printf ("%s\n%s    %s\n\n", part.Get (), args == 1 ? "" : "\n", helpText);
        if (extraText && args > 1) {
          part.SetFByLine ("    %s\n", extraText);
          printf ("%s\n", part.Get ());
        }
      }
    }
  }
}


static void ExecuteCmd (const char *cmd) {
  char **cmdArgv;
  int n, cmdNo, cmdArgc;

  //~ INFOF (("### ExecuteCmd ('%s')", cmd));
  StringSplit (cmd, &cmdArgc, &cmdArgv);
  cmdNo = -1;
  for (n = 0; n < commands; n++)
    if (strcmp (cmdArgv[0], commandArr[n].name) == 0) { cmdNo = n; break; }
  //~ INFOF(("cmdNo = %i '%s'", cmdNo, cmdArgv[0]));
  if (cmdNo < 0) printf ("Error: Unknown command '%s'\n", cmdArgv[0]);
  else commandArr[cmdNo].func (cmdArgc, (const char **) cmdArgv);
  if (cmdArgv) {
    free (cmdArgv[0]);
    free (cmdArgv);
  }
}





// *************** Readline hooks **************************


#if WITH_READLINE


static int rlCompleteOffset;    // number of characters to skip in the completion display


static void RlDisplayMatchList (char **matches, int len, int max) {
  char **shortMatches;
  int n;

  shortMatches = MALLOC (char *, len + 1);
  for (n = 0; n <= len; n++) shortMatches[n] = matches[n] + rlCompleteOffset;
  rl_display_match_list (shortMatches, len, max - rlCompleteOffset);
  FREEP (shortMatches);
  rl_forced_update_display ();      // force to redisplay the prompt
}


static char *RlGeneratorCommands (const char *text, int state) {
  static int idx, textLen;
  const char *name;

  // New word to complete: initialize the generator...
  if (!state) {
    idx = 0;
    textLen = strlen (text);
    rlCompleteOffset = 0;
  }

  // Return the next name which partially matches from the command list...
  while (idx < commands) {
    name = commandArr[idx].name;
    idx++;
    if (strncmp (name, text, textLen) == 0) return strdup (name);
  }

  // No name matched: return NULL...
  return NULL;
}


static char *RlGeneratorUri (const char *text, int state) {
  static int idx, idx1;
  static CDictRaw *map;
  static ERcPathDomain dom;
  static int pathOfs;
  static CString realUri;
  static const char *localPath;
  const char *name;
  CRcHost *rcHost;
  CRcDriver *rcDriver;
  CResource *resource;
  CString ret;

  // New word to complete: initialize the generator...
  if (!state) {
    realUri.Set (RcGetRealPath (&ret, RcGetAbsPath (text)));
    //~ INFOF (("\n### text = '%s' -> absPath =  '%s' -> realUri -> '%s'", text, ret.Get (), realUri.Get ()));
    dom = RcAnalysePath (realUri.Get (), &localPath, &rcHost, &rcDriver, &resource);
    //~ INFOF (("### realUri = %s, dom = %i, localPath = %s, rcHost = %s, rcDriver = %s, resource = %s",
          //~ realUri.Get (), dom, localPath, rcHost ? rcHost->ToStr () : "(null)",
          //~ rcDriver ? rcDriver->ToStr () : "(null)", resource ? resource->ToStr () : "(null)"));
    map = RcGetDirectory (dom, rcHost, rcDriver);

    idx = 0;
    idx1 = map ? map->Entries () : 0;
    pathOfs = strlen (realUri.Get ()) - strlen (localPath);

    rlCompleteOffset = strlen (text);
    while (rlCompleteOffset > 0 && text[rlCompleteOffset-1] != '/') rlCompleteOffset--;
  }

  // Return next matching word...
  while (idx < idx1) {
    name = map->GetKey (idx++);
    if (strncmp (name, localPath, strlen (localPath)) == 0) {
      if (pathOfs > 0) ret.Set (realUri.Get (), pathOfs);
      else ret.Clear ();
      ret.Append (name + MAX (0, -pathOfs));

      if (dom == rcpAlias) rl_completion_suppress_append = 1;     // Aliases may point to incomplete paths => Do not append a space, the user will hit Tab again
      else if (RcPathIsDir (RcAnalysePath (ret.Get (), NULL, NULL, NULL, NULL))) {
        ret.Append ('/');
        rl_completion_suppress_append = 1;
      }
      return ret.Disown ();
    }
  }

  // No more matching word...
  return NULL;
}


static char **RlCompletionFunction (const char *text, int start, int end) {
  int idx0;

  //~ INFOF (("### RlCompletionFunction: text = '%s', rl_line_buffer = '%s'", text, rl_line_buffer));

  rl_attempted_completion_over = 1;     // disable filename expansion

  for (idx0 = 0; idx0 < start && rl_line_buffer[idx0] == ' '; idx0++);
    // let 'idx0' point to the first effective character of the line
  if (start == idx0 || rl_line_buffer[idx0] == 'h')
    // word is at the start of the line or the argument of "help": complete as command...
    return rl_completion_matches (text, RlGeneratorCommands);
  else
    // else: complete as URI...
    return rl_completion_matches (text, RlGeneratorUri);

  return NULL;
}


#endif // WITH_READLINE





// ******************** main *******************************


int main (int argc, char **argv) {
#if WITH_READLINE
  static const char *prompt = "\001\033[1m\002home2l>\001\033[0m\002 ";   // The '\001' and '\002' mark invisible characters, so that libreadline knows about the visible length of the prompt.
  const char *homeDir;
#else
  static const char *prompt = "\033[1mhome2l>\033[0m ";
  char buf[256];
#endif
  struct sigaction sigAction, sigActionSaved;
  CSplitString argCmds;
  CString line, histFile;
  char *p;
  int n;
  bool interactive, withServer;

  // Startup...
  EnvInit (argc, argv,
           "  -n                : disable local server (default: use 'rc.enableServer' setting)\n"
           "  -e '<command(s)>' : execute the command(s) and quit\n"
           "  -i '<command(s)>' : execute the command(s), then continue interactively\n");

  // Read arguments...
  interactive = true;
  withServer = true;
  p = NULL;
  for (n = 1; n < argc; n++) if (argv[n][0] == '-')
    switch (argv[n][1]) {
      case 'n':
        withServer = false;
        break;
      case 'e':
        interactive = false;
      case 'i':
        p = argv[n+1];
        break;
    }
  if (p) argCmds.Set (p, INT_MAX, ";");

  // Init resources...
  RcInit (withServer, true);    // starts main timer loop in the background...
  RcStart ();

  // Initialize subscriber...
  subscriber = new CRcSubscriber ();
  subscriber->Register ("shell");

  if (interactive) {

    // Setup history...
#if WITH_READLINE
    using_history ();
    homeDir = getenv ("HOME");
    if (!homeDir) homeDir = "/";
    GetAbsPath (&histFile, envHistFile, homeDir);
    if (envHistLines > 0) read_history (histFile.Get ());
#endif

    // Set signal handler for keyboard interrupt (Ctrl-C)...
    sigAction.sa_handler = SignalHandler;
    sigemptyset (&sigAction.sa_mask);
    sigAction.sa_flags = 0;
    sigaction (SIGINT, &sigAction, &sigActionSaved);
  }

  // Run non-interactive commands...
  interrupted = false;
  for (n = 0; n < argCmds.Entries () && !interrupted; n++) {
    line.Set (argCmds.Get (n));
    line.Strip ();
    if (line[0]) {
      if (interactive) {
        for (p = (char *) prompt; *p; p++) if (*p > '\002') putchar (*p);
        puts (line.Get ());
      }
#if WITH_READLINE
      if (interactive) add_history (line.Get ());
#endif
      ExecuteCmd (line.Get ());
    }
  }
  //~ INFO ("### Non-interactive commands completed.");

  // Run interactive main loop...
  if (interactive) {

    // Main input loop...
#if WITH_READLINE
    rl_readline_name = "home2l";     // allow conditional parsing of the ~/.inputrc file.
    rl_attempted_completion_function = RlCompletionFunction;    // Tab-completion
    rl_completion_display_matches_hook = RlDisplayMatchList;    // Abbreviation of match lists
    rl_completer_word_break_characters = (char *) " ";
#endif
    while (!doQuit) {
      PollSubscriber ();
#if WITH_READLINE
      p = readline (EnvHaveTerminal () ? prompt : NULL);
      if (!p) {
        if (EnvHaveTerminal ()) putchar ('\n');
        break;
      }
      line.SetO (p);
#else
      fputs (prompt, stdout);
      fflush (stdout);
      fgets (buf, sizeof (buf), stdin);
      if (feof (stdin)) {
        putchar ('\n');
        doQuit = true;
        break;
      }
      line.SetC (buf);
#endif
      line.Strip ();
      if (line[0]) {

        // Line not empty: Store in history and execute...
#if WITH_READLINE
        add_history (line.Get ());
#endif
        interrupted = false;
        ExecuteCmd (line.Get ());
      }
    }

    // Write back history...
#if WITH_READLINE
    if (envHistLines > 0) {
      stifle_history (envHistLines);
      write_history (histFile.Get ());
    }
#endif

    // Restore signal handler for keyboard interrupt (Ctrl-C)...
    sigaction (SIGINT, &sigActionSaved, NULL);
  }
  //~ INFO ("### Interactive commands completed.");

  // Done...
  FREEO (subscriber);
  RcDone ();
  EnvDone ();
  return 0;
}
