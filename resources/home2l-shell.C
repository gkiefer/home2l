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


#include "rc_core.H"

#if WITH_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif


#define SHELL_NAME "shell"
  // This constant should reflect the tool executable name (without "home2l-" prefix).
  // It is use (amoung others):
  // - as the instance name for special invocations
  // - the subscriber ID
  // - request IDs




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





// *************************** Forward declarations ****************************


// Implemented in section "Command interpreter" ...
static bool ExecuteCmd (const char *cmd, bool interactive);

static bool CmdHelp (int argc, const char **argv, bool interactive);





// *************************** Helpers *****************************************


static CString workDir;


static const char *NormalizedUri (const char *uri) {
  static CString ret;

  RcPathNormalize (&ret, uri, workDir);
  return ret.Get ();
}


static void HelpOnCmd (const char *cmdName) {
  const char *helpArgs[] = { "h", cmdName };

  putchar ('\n');
  EnvPrintBanner ();
  CmdHelp (2, helpArgs, true);
}


static bool HandleHelpOption (int argc, const char **argv) {
  if (argc >= 2) if (argv[1][0] == '-' && argv[1][1] == 'h') {
    HelpOnCmd (argv[0]);
    return true;
  }
  return false;
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


static void PollSubscriber (bool withRequestChanges) {
  CRcEvent ev;
  CString s;

  while (subscriber->PollEvent (&ev)) {
    if (ev.Type () != rceRequestChanged || withRequestChanges)
      printf (": %s\n", ev.ToStr (&s));
  }
}




// *************************** Command functions *******************************


static bool doQuit = false;


static bool CmdQuit (int argc, const char **argv, bool interactive) {
  doQuit = true;
  return true;
}


static bool CmdTypesInfo (int argc, const char **argv, bool interactive) {
  int n, k;

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
  return true;
}


static bool CmdNetworkInfo (int argc, const char **argv, bool interactive) {
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

  // Done...
  return true;
}


static bool CmdList (int argc, const char **argv, bool interactive) {
  CKeySet dir;
  TRcPathInfo info;
  CString s, prefix;
  CRcValueState vs;
  const char *uri, *argPath, *key;
  int n, k;
  bool optAllowNet;

  // Parse arguments...
  argPath = NULL;
  optAllowNet = true;
  for (n = 1; n < argc; n++) {
    if (argv[n][0] == '-') {
      for (k = 1; argv[n][k]; k++) switch (argv[n][k]) {
        case 'h':
          HelpOnCmd (argv[0]);
          return true;
        case 'l':
          optAllowNet = false;
          break;
        default:
          printf ("Invalid argument: '%s'\n", argv[n]);
          return false;
      }
    }
    else if (!argPath) argPath = argv[n];
  }

  // Analyse path...
  uri = argPath ? NormalizedUri (argPath) : workDir.Get ();
  RcPathAnalyse (uri, &info, true);
  //~ INFOF (("### ... RcPathAnalyse done: state = %i", info.state));
  if (info.state == rcaAliasResolved) {

    // Have a resolvable alias: Show the alias and continue with the target ...
    //~ INFOF (("### target = '%s', localPath = '%s'", info.target, info.localPath));
    s.SetC (info.target);
    s.Append (info.localPath);
    RcPathAnalyse (s.Get (), &info, true);
    if (!info.resource && !RcPathIsDir (s.Get ()))
      // Alias ends neither at a known resource nor is a directory: Print alias target
      printf ("%s -> %s\n", uri, s.Get ());
  }

  // Print resource details ...
  if (info.resource) {
    info.resource->PrintInfo (stdout, 1, optAllowNet);
    if (info.resource->Type () == rctString) {
      info.resource->GetValueState (&vs);
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

  // ... or directory listing ...
  else {
    RcPathGetDirectory (uri, &dir, NULL, &prefix, true);
    for (n = 0; n < dir.Entries (); n++) {
      key = dir.GetKey (n);
      s.SetC (prefix);
      s.Append (key);
      RcPathAnalyse (s.Get (), &info, false);   // currently just to identify and resolve alias
      //~ INFOF (("### ... RcPathAnalyse: state = %i", info.state));
      if (info.state == rcaAliasResolved)
        printf ("%s -> %s%s\n", key, info.target, info.localPath);
      else
        printf ("%s\n", key);
    }
  }

  // Done...
  //~ INFOF (("dom = %i, rcHost = %i, rcDriver = %i, resource = %i, map = %i", dom, rcHost ? 1 : 0, rcDriver ? 1 : 0, resource ? 1 : 0, map ? map->Entries () : 0));
  return true;
}


static bool CmdChDir (int argc, const char **argv, bool interactive) {

  //~ printf ("CmdChDir\n");
  if (argc >= 2) workDir.Set (NormalizedUri (argv[1]));
  if (workDir.IsEmpty ()) workDir = "/";
  if (RcPathIsDir (workDir.Get ())) workDir.Append ('/');
  workDir.PathNormalize ();
  printf ("%s\n", workDir.Get ());
  return true;
}


static bool CmdSubscribe (int argc, const char **argv, bool interactive) {
  for (int n = 1; n < argc; n++) subscriber->AddResources (NormalizedUri (argv[n]));
  subscriber->PrintInfo ();
  return true;
}


static bool CmdUnsubscribe (int argc, const char **argv, bool interactive) {
  for (int n = 1; n < argc; n++) {
    subscriber->DelResources (NormalizedUri (argv[n]));
  }
  subscriber->PrintInfo ();
  return true;
}


static bool CmdFollow (int argc, const char **argv, bool interactive) {
  TTicks timeLeft = 1;
  int i, n;
  bool haveTime, newSubs, withRequestChanges;

  // Parse args ...
  haveTime = newSubs = withRequestChanges = false;
  for (n = 1; n < argc; n++) {
    if (argv[n][0] == '-') switch (argv[n][1]) {
      case 'h':
        HelpOnCmd (argv[0]);
        return true;
      case 'r':
        withRequestChanges = true;
        break;
      case 't':
        n++;
        if (n < argc) {
          if (IntFromString (argv[n], &i)) timeLeft = i;
          else {
            printf ("Invalid time value (must be an integer number of milliseconds).\n");
            return false;
          }
          haveTime = true;
          break;
        }
        // fall through on error
      default:
        printf ("Invalid arguments.\n");
        return false;
      }
    else {
      subscriber->AddResources (NormalizedUri (argv[n]));
      newSubs = true;
    }
  }

  // Print new suscribers ...
  if (newSubs) subscriber->PrintInfo ();

  // Go ahead ...
  while (!interrupted && timeLeft > 0) {
    subscriber->WaitEvent (NULL, haveTime ? &timeLeft : NULL);
    PollSubscriber (withRequestChanges);
    fflush (stdout);
  }
  putchar ('\n');
  //~ write (STDOUT_FILENO, "\n", 1);
  return !interrupted;
}


static bool CmdGet (int argc, const char **argv, bool interactive) {
  CRcSubscriber subscr, *savedSubscriber;
  CRcEvent ev;
  CResource *rc;
  CRcValueState *vs, vsRef;
  CString s;
  TTicks timeLeft = 1;
  int i, n;
  bool haveTime, notEqual, mindBusy, haveVsRef, success;

  // Parse options ...
  rc = NULL;
  haveTime = notEqual = mindBusy = haveVsRef = false;
  for (n = 1; n < argc; n++) {
    if (argv[n][0] == '-') switch (argv[n][1]) {

      // Options ...
      case 'h':
        HelpOnCmd (argv[0]);
        return true;
      case 't':
        n++;
        if (n >= argc) {
          printf ("Missing time value.\n");
          return false;
        }
        else {
          if (IntFromString (argv[n], &i)) timeLeft = i;
          else {
            printf ("Invalid time value (must be an integer number of milliseconds).\n");
            return false;
          }
          haveTime = true;
          break;
        }
        break;
      case 'n':
        notEqual = true;
        break;
      case 'b':
        mindBusy = true;
        break;
      default:
        printf ("Invalid option: '%s'\n", argv[n]);
        return false;

    }   // if ... switch (argv[n][1])
    else {
      if (!rc) {

        // Expect & parse resource ...
        rc = RcGet (argv[n]);
        if (!rc) {
          printf ("Invalid resource: '%s'.\n", argv[n]);
          return false;
        }
      }
      else if (!haveVsRef) {

        // Expect & parse value/state ...
        if (!vsRef.SetFromStr (argv[n])) {
          printf ("Invalid resource value: '%s'.\n", argv[n]);
          return false;
        }
        haveVsRef = true;
      }
      else {
        printf ("Invalid argument: '%s'\n", argv[n]);
        return false;
      }
    }
  }   // for
  if (!rc) {
    printf ("Missing resource argument.\n");
    HelpOnCmd (argv[0]);
    return false;
  }

  // Init local subscriber ...
  subscr.Register (SHELL_NAME ".get");
  subscr.AddResource (rc);
  savedSubscriber = subscriber;
  subscriber = &subscr;     // replace reference to receive keyboard interrupts

  // Go ahead ...
  success = false;
  vs = NULL;
  while (!interrupted && !success && timeLeft > 0) {
    subscr.WaitEvent (&ev, haveTime ? &timeLeft : NULL);
    if (ev.Type () == rceValueStateChanged) {
      vs = ev.ValueState ();
      if (!haveVsRef) {
        if (vs->IsKnown ()) success = true;
      }
      else {
        if (vsRef.Convert (vs->Type ())) {
          if (mindBusy) success = vsRef.Equals (vs);
          else success = vsRef.ValueEquals (vs);
          if (notEqual) success = !success;
        }
      }
    }
  }

  // Print value on success ...
  if (success) {
    if (!mindBusy) if (vs->IsKnown ()) vs->SetState (rcsValid);
    printf ("%s\n", vs->ToStr (&s));
  }
  //~ write (STDOUT_FILENO, "\n", 1);

  // Done ...
  subscriber = savedSubscriber;
  return success;
}


static bool CmdSetRequest (int argc, const char **argv, bool interactive) {
  // argv[1]: resource name (rel. path)
  // argv[2] .. argv[argc-1]: concatenate, then call 'CRcResource::SetFromStr ()
  CResource *rc = NULL;
  CRcRequest *req;
  CString reqDef;
  const char *rcUri;
  int n;
  bool ok;

  if (HandleHelpOption (argc, argv)) return true;
  ok = (argc >= 3);
  if (!ok) {
    printf ("Too few arguments.\n");
    HelpOnCmd (argv[0]);
  }

  if (ok) {

    // Lookup resource...
    rcUri = NormalizedUri (argv[1]);
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
    req->SetPriority (interactive ? rcPrioShell : rcPrioRule);
    req->SetFromStr (reqDef);
    rc->SetRequest (req);
    if (interactive) rc->PrintInfo ();
  }

  return ok;
}


static bool CmdDelRequest (int argc, const char **argv, bool interactive) {
  // argv[1]: resource name (rel. path)
  // argv[2]: request ID (optional)
  CResource *rc = NULL;
  const char *rcUri;
  bool ok;

  if (HandleHelpOption (argc, argv)) return true;
  ok = (argc == 2 || argc == 3);
  if (!ok) {
    printf ("Wrong number of arguments!\n");
    HelpOnCmd (argv[0]);
  }

  if (ok) {

    // Lookup resource...
    rcUri = NormalizedUri (argv[1]);
    rc = RcGetResource (rcUri, false);
    if (!rc) {
      printf ("Invalid URI '%s'\n", rcUri);
      ok = false;
    }
  }

  if (ok) {

    // Delete request & print info...
    rc->WaitForRegistration ();
    rc->DelRequest ((argc == 3) ? argv[2] : NULL);
    if (interactive) rc->PrintInfo ();
  }

  return ok;
}


static bool CmdRequestShortcut (int argc, const char **argv, bool interactive) {
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
        return false;
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
  return ExecuteCmd (cmd.Get (), interactive);
}





// ************************* Main command interpreter **************************


typedef bool (*TCmdFunc) (int argc, const char **argv, bool interactive);


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

  { "t", CmdTypesInfo, "", "List supported value types" },
  { "types", CmdTypesInfo, NULL, NULL, NULL },

  { "n", CmdNetworkInfo, "[<options>]", "Print network info",
          "Options:\n"
          "\n"
          "  -s : Print subscribers\n"
          "\n"
          "  -r : Also print resources for each subscriber\n" },
  { "network", CmdNetworkInfo, NULL, NULL, NULL },

  { "l", CmdList, "[<options>] [<path>]", "List object(s) [in <path>]",
          "Options:\n"
          "\n"
          "  -l : Print local info on a resource\n"
          "\n"
          "The string of a string-typed resource is additionally printed unescaped,\n"
          "but only if the resource is local or subscribed to.\n" },
  { "list", CmdList, NULL, NULL },
  { "show", CmdList, NULL, NULL },

  { "c", CmdChDir, "[<path>]", "Change or show working path", NULL },
  { "change", CmdChDir, NULL, NULL, NULL },

  { "s+", CmdSubscribe, "<pattern>", "Subscribe to resource(s)",
            "<pattern> is a single or a whitespace-separated list of resources.\n"
            "Within the resource expressions, both MQTT-style and filename-style wildcards\n"
            "can be used to select multiple resources:\n"
            "\n"
            "  '?' matches any single character except '/'.\n"
            "  '*' matches 0 or more characters except '/'.\n"
            "  '+' matches 1 or more characters except '/'.\n"
            "  '#' matches the complete remaining string (including '/' characters) and can\n"
            "      thus be used to select a complete subtree. If used, '#' must be the last\n"
            "      character in the expression. Anything behind a '#' is ignored silently.\n" },
  { "subscribe", CmdSubscribe, NULL, NULL, NULL },
  { "s-", CmdUnsubscribe, "<pattern>", "Unsubscribe from resource(s)",
            "<pattern> is a single or a whitespace-separated list of resources.\n"
            "Within the resource expressions, both MQTT-style and filename-style wildcards\n"
            "can be used to select multiple resources. See help on 's+' for details.\n"
            "\n"
            "To remove all subscriptions, enter the pattern '/#'." },
  { "unsubscribe", CmdUnsubscribe, NULL, NULL, NULL },
  { "s", CmdSubscribe, "", "List subscriptions", NULL },
  { "subscriptions", CmdSubscribe, NULL, NULL, NULL },

  { "f", CmdFollow, "[-t <ms>] [-r] [<pattern>]", "Follow subscriptions until Ctrl-C is pressed.",
          "If the '-t' option is set, the commands stops automatically after <ms> milliseconds.\n"
          "If the '-r' option is set, 'request changed' events are shown, which are usually hidden.\n"
          "If resources are passed as <pattern>, they are subscribed to first.\n"
          "This command can also be used to just wait for a certain time.\n" },
  { "follow", CmdFollow, NULL, NULL, NULL },

  { "get", CmdGet, "[<options>] <rc> [<vs>]", "Get a resource value and state, eventually after waiting for it",
          "Options:\n"
          "\n"
          "  -t <ms> : wait for up to <ms> milliseconds [default: wait indefinitely]\n"
          "\n"
          "  -n      : wait until the current resource value is NOT equal to <vs>\n"
          "\n"
          "  -b      : mind the busy state - \n"
          "            By default, no distinction is made between the states 'valid' and 'busy',\n"
          "            and any known value is printed without a '!' prefix. This flag changes this\n"
          "            behavior, and a comparison yields equality only if the states are equal, too.\n"
          "\n"
          "This command is designed for shell scripts to obtain resource values in different ways.\n"
          "\n"
          "If <vs> is not given, the command waits until a known value (state 'valid' or 'busy')\n"
          "is available (or at most <ms> milliseconds, if the -t option is given) and returns the value.\n"
          "\n"
          "If <vs> is given, the command waits until the resource assumes the given value.\n"
          "This can be used to wait until, for example, a certain sensor becomes active ('1').\n"
          "\n"
          "To wait for a more complex condition (e.g. a value beeing in a certain range) in a\n"
          "shell script, this command may be executed in a loop in such a way that '-n' is set\n"
          "and the last received value is passed as <vs>. After each iteration, the returned value\n"
          "can be checked by the calling script in an arbitrary way, and the wait condition ensures\n"
          "that no busy waiting happens. Please note, however, that quick value/state changes between\n"
          "different home2l-shell invocations may get lost. To make sure that all value/state change\n"
          "events are caught, the 'follow' command may be used.\n"
           },
  { "wait", CmdGet, NULL, NULL, NULL },

  { "r+", CmdSetRequest, "<rc> <value> [<ropts>]", "Add or change a request",
          "Request options <attributes> :\n"
          "\n"
          "  <rc>    : Resource identifier\n"
          "\n"
          "  <value> : Requested value\n"
          "\n"
          "  <ropts> : Additional request arguments as supported by 'CRcRequest::SetFromStr ()':\n"
          "             #<id>   : Request ID [default: '" SHELL_NAME "']\n"
          "             *<prio> : Priority (0..9) [Default: 7 (rcPrioShell)]\n"
          "             +<time> : Start time\n"
          "             -<time> : End time\n"
          "             ~<hyst> : Hysteresis in milliseconds\n"
          "\n"
          "The start/end times <time> may be given as absolute date/times in the format\n"
          "YYYY-MM-DD-HHMM[SS[.frac]] or a relative time <n>, where <n> is the number of\n"
          "milliseconds in the future.\n" },
  { "request", CmdSetRequest, NULL, NULL, NULL },
  { "r-", CmdDelRequest, "<rc> [#<reqGid>]", "Delete a request ('" SHELL_NAME "' if no <reqGid> is given)", NULL },
  { "delrequest", CmdDelRequest, NULL, NULL },

  { "0", CmdRequestShortcut, "[<ropts>]", "Shortcut for: r+ . 0 [<ropts>]", extraRequestShortcuts },
  { "1", CmdRequestShortcut, "[<ropts>]", "Shortcut for: r+ . 1 [<ropts>]", extraRequestShortcuts },
  { "!", CmdRequestShortcut, "<val> [<ropts>]", "Shortcut for: r+ . <val> [<ropts>]", extraRequestShortcuts },
  { "-", CmdRequestShortcut, "", "Shortcut for: r- .", extraRequestShortcuts }
};


#define commands ((int) (sizeof (commandArr) / sizeof (TCmd)))


static bool CmdHelp (int args, const char **argv, bool interactive) {
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
  return true;
}


static int FindCmd (const char *key) {
  for (int n = 0; n < commands; n++)
    if (strcmp (key, commandArr[n].name) == 0) return n;
  return -1;
}


static bool ExecuteCmd (const char *cmd, bool interactive) {
  char **cmdArgv;
  int cmdNo, cmdArgc;
  bool ok;

  ok = false;
  //~ INFOF (("### ExecuteCmd ('%s')", cmd));
  StringSplit (cmd, &cmdArgc, &cmdArgv);
  cmdNo = FindCmd (cmdArgv[0]);
  //~ INFOF(("cmdNo = %i '%s'", cmdNo, cmdArgv[0]));
  if (cmdNo < 0) printf ("Error: Unknown command '%s'\n", cmdArgv[0]);
  else ok = commandArr[cmdNo].func (cmdArgc, (const char **) cmdArgv, interactive);
  if (cmdArgv) {
    free (cmdArgv[0]);
    free (cmdArgv);
  }
  return ok;
}





// *************** Readline hooks **************************


#if WITH_READLINE


static int rlCompleteOffset;        // number of characters to skip in the completion display


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
  static CKeySet dir;
  static CString prefix;
  static int idx, idx1;
  CString ret;
  const char *p, *absPath;

  // New word to complete: initialize the generator...
  if (!state) {
    //~ INFOF (("### RlGeneratorUri('%s')", text));
    absPath = NormalizedUri (text);
    p = strrchr (absPath, '/');
    if (!p) return NULL;    // illegal current path
    ret.Set (absPath, p+1 - absPath);     // reuse 'ret' for temporarily storing the directory component of 'text'
    RcPathGetDirectory (ret.Get (), &dir, NULL, &prefix);
    dir.PrefixSearch (p+1, &idx, &idx1);
    //~ INFOF (("###   idx = %i, idx1 = %i", idx, idx1));
    rlCompleteOffset = ret.Len ();
  }

  // Return next matching word...
  if (idx < idx1) {
    ret.SetC (prefix);
    ret.Append (dir.GetKey (idx++));
    if (ret[-1] == '/') rl_completion_suppress_append = 1;
    //~ INFOF (("###   returning '%s'.", ret.Get ()));
    return ret.Disown ();
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





// ******************** main() *****************************


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
  int n, cmdNo;
  bool interactive, withServer, singleSpecial, ok;

  // Check how we were called & handle special short invocations ...
  //~ INFOF (("argv[0] == '%s'", argv[0]));
  singleSpecial = false;
  p = strchr (argv[0], '-');
  if (p) if (strcmp (p + 1, SHELL_NAME) != 0) {      // quick pre-check with "shell"
    cmdNo = FindCmd (p + 1);
    //~ INFOF (("### cmdNo = %i", cmdNo));
    if (cmdNo >= 0) {
      singleSpecial = true;
      line.SetC (p + 1);
      for (n = 1; n < argc; n++) {
        line.Append (' ');
        line.Append (argv[n]);
      }
    }
  }

  // Parse arguments & startup ...
  if (singleSpecial) {                      // Special case: Invocation for single command ...
    interactive = withServer = false;
    EnvInit (1, argv, NULL, SHELL_NAME, true);
      // Special initialization:
      // - discard all arguments (argv[0] must be passed!)
      // - set instance name to SHELL_NAME ("shell")
      // - suppress banner
  }
  else {                                    // General case ...
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
          n++;
          while (n < argc) {      // add all remaining arguments to the command(s)
            line.Append (argv[n]);
            line.Append (' ');
            n++;
          }

          break;
      }
    EnvInit (argc, argv,
             "  -n              : disable local server (default: use 'rc.enableServer' setting)\n"
             "  -e <command(s)> : execute the command(s) and quit\n"
             "  -i <command(s)> : execute the command(s), then continue interactively\n"
             "\n"
             "Options -e or -i must be specified last. All remaining arguments are interpreted\n"
             "as <command(s)>.\n",
             NULL, !interactive);     // no banner in non-interactive mode
  }
  if (!line.IsEmpty ()) argCmds.Set (line.Get (), INT_MAX, ";");

  // Init resources...
  RcInit (withServer, true);        // starts main timer loop in the background...
  RcStart ();
  RcPathNormalize (&workDir, ".");  // init working directory

  // Init subscriber...
  subscriber = new CRcSubscriber ();
  subscriber->Register (SHELL_NAME);

  if (interactive) {

    // Init history...
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
  ok = true;
  for (n = 0; n < argCmds.Entries () && ok; n++) {
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
      ok = ExecuteCmd (line.Get (), false);
      if (interrupted) ok = false;
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
      PollSubscriber (false);
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
        ok = true;
        argCmds.Set (line.Get (), INT_MAX, ";");
        for (n = 0; n < argCmds.Entries () && ok; n++) {
          line.Set (argCmds.Get (n));
          line.Strip ();
          ok = ExecuteCmd (line.Get (), true);
          if (interrupted) ok = false;
        }
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

  }    // if (interactive)

  //~ INFO ("### Interactive commands completed.");

  // Done...
  FREEO (subscriber);
  RcDone ();
  EnvDone ();
  return ok ? 0 : 1;
}
