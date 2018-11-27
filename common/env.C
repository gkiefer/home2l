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


#include "env.H"

#include <stdlib.h>   // for 'strtol'
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>   // for 'gethostname', 'isatty'
#include <fcntl.h>
#include <fnmatch.h>
#include <pwd.h>
#include <grp.h>      // for 'initgroups(3)'

#if !ANDROID
#include <locale.h>
#include <sys/resource.h>   // for 'setrlimit'
#endif




// *************************** Global settings *********************************


// ***** Debug *****

#if !ANDROID
ENV_PARA_BOOL("debug.enableCoreDump", envEnableCoreDump, false);
  /* Enable generation of a core dump using the setrlimit() system call (without size limit)
   */
#endif



// ***** Domain 'home2l' *****


ENV_PARA_VAR ("home2l.config", static const char *, envConfig, "etc/home2l.conf");
  /* Main configuration file (read-only)
   */
ENV_PARA_NOVAR ("home2l.version", const char *, buildVersion, NULL);
  /* Version of the Home2L suite (read-only)
   */
ENV_PARA_NOVAR ("home2l.buildDate", const char *, buildDate, NULL);
  /* Build date of the Home2L suite (read-only)
   */

// Operation software and architecture ...
ENV_PARA_VAR ("home2l.os", static const char *, buildOS, ANDROID ? "Android" : BUILD_OS);
  /* Operation software environment (Debian / Android) (read-only)
   *
   * This setting is determined by the build process.
   */
ENV_PARA_VAR ("home2l.arch", static const char *, buildArch, ANDROID ? NULL : BUILD_ARCH);
  /* Processor architecture (i386 / amd64 / armhf / <undefined>) (read-only)
   *
   * This setting is generated during the build process and taken from the processor
   * architecture reported by 'dpkg --print-architecture' in Debian.
   */



// ***** Domain 'sys' *****

ENV_PARA_NOVAR("sys.syslog", bool, envSyslog, false);
  /* Set to write all messages to syslog
   */

// Process environment...
ENV_PARA_VAR ("sys.machineName", static const char *, envMachineName, NULL);
  /* System host name (read-only)
   */
ENV_PARA_VAR ("sys.execPathName", static const char *, envExecPathName, NULL);
  /* Full path name of the executable (read-only)
   */
ENV_PARA_VAR ("sys.execName", static const char *, envExecName, NULL);
  /* File name of the executable without path (read-only)
   */
ENV_PARA_VAR ("sys.pid", static int, envPid, 0); // NODEFAULT
  /* System process ID (PID) (read-only)
   */

// Instance identification...
ENV_PARA_VAR ("sys.instanceName", static const char *, envInstanceName, NULL);
  /* Instance name (read-only)
   *
   * The instance name should uniquely identify the running process.
   * There is no technical mechanism to enforce uniqueness. Hence, it is up to the
   * administrator take care of that.
   *
   * The instance name can be set by the tool programmatically, or in some tools
   * with the '-x' command line option. By default, the instance name is set to
   * the name of the executable without an eventually leading "home2l-".
   */
ENV_PARA_STRING ("sys.droidId", envDroidId, "000");
  /* Droid ID
   *
   * This is the 3-digit number displayed on the wall clocks to indicate the serial number
   * of the device.
   * If the host name ends with three digits, the droid ID is automatically taken from that.
   */

// Directories...
ENV_PARA_STRING ("sys.rootDir", envRootDir, NULL);
  /* Home2L installation root directory (HOME2L\_ROOT)
   */
ENV_PARA_STRING ("sys.varDir", envVarDir, "/var/opt/home2l");
  /* Root directory for variable data
   */
ENV_PARA_STRING ("sys.tmpDir", envTmpDir, "/tmp/home2l");
  /* Root directory for temporary data
   */

// Locale
ENV_PARA_STRING ("sys.locale", envSysLocale, NULL);
  /* Define the locale for end-user applications in the 'll\_CC' format (e.g. ''de\_DE'')
   *
   * This setting defines the message language and formats of end-user applications.
   * Only end user applications (presently WallClock) are translated, command line tools
   * for administrators expect English language skills.
   */




// ***** Getters for frequently used variables *****


const char *EnvBuildOS ()   { return buildOS; }
const char *EnvBuildArch () { return buildArch; }

const char *EnvMachineName ()   { return envMachineName; }
const char *EnvExecPathName ()  { return envExecPathName; }
const char *EnvExecName ()      { return envExecName; }

int EnvPid () { return envPid; }

const char *EnvInstanceName ()  { return envInstanceName; }

const char *EnvDroidId ()       { return envDroidId; }

const char *EnvHome2lRoot ()  { return envRootDir; }
const char *EnvHome2lVar ()   { return envVarDir; }
const char *EnvHome2lTmp ()   { return envTmpDir; }





// *************************** Misc Helpers ************************************


const char *EnvGetHome2lRootPath (CString *ret, const char *relOrAbsPath) {
  return GetAbsPath (ret, relOrAbsPath, envRootDir);
}


const char *EnvGetHome2lVarPath (CString *ret, const char *relOrAbsPath) {
  return GetAbsPath (ret, relOrAbsPath, envVarDir);
}


const char *EnvGetHome2lTmpPath (CString *ret, const char *relOrAbsPath) {
  return GetAbsPath (ret, relOrAbsPath, envTmpDir);
}


bool EnvHaveTerminal () {
  return isatty (fileno (stdin));
}


static bool DoMkdir (char *pn) {
  // 'pn' is temporarily modified (and restored finally).
  struct stat fileStat;
  struct passwd *pwEntry;
  char *p;
  bool ok;

  //~ INFOF (("# DoMkDir ('%s')", pn));

  // Check if 'pn' already exists...
  if (lstat (pn, &fileStat) == 0) {
    if (S_ISDIR (fileStat.st_mode)) return true;    // ok (probably), nothing to do
    WARNINGF (("Cannot create directory '%s': A file with the same name is in the way", pn));
    return false;
  }

  // Make parent directory...
  p = strrchr (pn, '/');
  if (!p || p == pn) {
    WARNINGF (("Cannot determine parent directory of '%s'", pn));
    return false;   // no parent
  }
  *p = '\0';
  ok = DoMkdir (pn);
  *p = '/';
  if (!ok) return false;    // parent creation failed (warning has already been emitted).

  // Make current directory with correct attributes...
  if (mkdir (pn, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
    WARNINGF (("Cannot create directory '%s': %s", pn, strerror (errno)));
    ok = false;
  }

  // Try to set group ownership (failure is not reported as error)...
  pwEntry = getpwnam (HOME2L_USER);
  if (!pwEntry) WARNINGF (("Cannot identify user '" HOME2L_USER "': %s", strerror (errno)));
  else {
    //~ INFOF (("# ... chown (%i)", pwEntry->pw_gid));
    if (chown (pn, (uid_t) -1, pwEntry->pw_gid) != 0) WARNINGF (("Cannot set group ownership on '%s': %s", pn, strerror (errno)));
  }

  // Done...
  return ok;
}


bool EnvMkVarDir (const char *relOrAbsPath) {
  CString absPath;

  EnvGetHome2lVarPath (&absPath, relOrAbsPath);
  ASSERT (absPath[0] == '/');
  return DoMkdir ((char *) absPath.Get ());
}


bool EnvMkTmpDir (const char *relOrAbsPath) {
  CString absPath;

  EnvGetHome2lTmpPath (&absPath, relOrAbsPath);
  ASSERT (absPath[0] == '/');
  return DoMkdir ((char *) absPath.Get ());
}





// *************************** Ini file parsing ********************************


static CKeySet sectionSet;


static inline bool IsSpace (char c) { return c == ' ' || c == '\t'; }
static inline bool IsQuote (char c) { return c == '\'' || c == '"'; }
static inline bool IsComment (char c) { return c == ';' || c == '#'; }
static inline bool IsKeyChar (char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strchr ("_-./:", c) != NULL; }


static void ReadIniFile (const char *fileName, CDictFast<CString> *map) {
  CString fileBuf, line, valStr, s;
  int fd, lineNo;
  bool relevant, ok, prodVal, varVal, neg;
  CSplitString sumStr, prodStr;
  char *key, *val, *p, *q;
  char quote;
  int n, k, i;

  DEBUGF (1, ("Reading '%s'...", fileName));
  relevant = true;
  lineNo = 0;
  fd = open (fileName, O_RDONLY);
  if (fd < 0) ERRORF (("Unable to open '%s'", fileName));
  while (fileBuf.AppendFromFile (fd)) while (fileBuf.ReadLine (&line)) {
    lineNo++;
    line.Strip ();
    ok = true;
    switch (line[0]) {

      // Comment or empty line...
      case '\0':
      case ';':
      case '#':
        break;

      // Section head ...
      case '[':
        p = (char *) line.Get () + 1;
        q = p;
        while (*q && *q != ']') q++;
        if (*q != ']') { ok = false; break; }
        *q = '\0';
        // Separate product terms...
        sumStr.Set (p, INT_MAX, ",+");
        relevant = false;
        for (n = 0; n < sumStr.Entries () && !relevant; n++) {
          // Separate literals...
          prodStr.Set (sumStr.Get (n), INT_MAX, "&@");
          prodVal = true;
          for (k = 0; k < prodStr.Entries () && prodVal; k++) {
            p = (char *) prodStr.Get (k);
            // Check for negator...
            while (IsSpace (*p)) p++;
            neg = (*p == '!');
            if (neg) p++;
            StringStrip (p);
            // Evaluate variable...
            varVal = false;
            for (i = 0; i < sectionSet.Entries () && !varVal; i++) {
              if ( fnmatch (p, sectionSet.GetKey (i), 0) == 0) varVal = true;
            }
            // Evaluate literal to product term...
            prodVal &= (neg ^ varVal);
          }
          // Evaluate product term to sum...
          relevant |= prodVal;
        }
        break;

      // Key/value pair ...
      default:
        if (!relevant) break;

        // Extract key...
        key = (char *) line.Get ();
        p = key;
        while (IsKeyChar (*p)) p++;
        q = p;
        while (IsSpace (*q)) q++;
        if (p == key || *q != '=') { ok = false; break; }   // no key or '=' is missing
        *p = '\0';

        // Extract value...
        val = q + 1;                      // first char after '='
        while (IsSpace (*val)) val++;     // first non-space char after '='
        p = q = val;                      // 'p' is write pointer, 'q' is read pointer
        if (IsQuote (*q)) quote = *(q++); // is the value quoted?
        else quote = '\0';
        while (*q && (quote ? *q != quote : !IsComment (*q))) {
            // Quoted values end at the closing quote (everything behind is ignored) or end of line.
            // Unquoted values may end at a comment.
          if (*q == '\\') {
            q++;
            if (!*q) { ok = false; break; }   // sudden end of line after '\'
          }
          *(p++) = *(q++);
        }
        if (!ok) break;
        if (!quote) while (IsSpace (p[-1])) p--;  // strip trailing space (if unquoted)
        *p = '\0';

        // Store result or include sub-file...
        if (strncmp (key, "include.", 8) == 0) {
          // keys named "include.<some name>" cause to include another configuration file;
          // the path must either be absolute or relative to HOME2L_ROOT
          ReadIniFile (EnvGetHome2lRootPath (&s, val), map);
        }
        else {
          valStr.Set (val);
          map->Set (key, &valStr);
        }
    }
    if (!ok) ERRORF (("Syntax error at '%s:%i'", fileName, lineNo));
  }
  close (fd);
}





// *************************** Settings dictionary *****************************


CDictFast<CString> envMap;





// ***** Default and preset values *****


static void EnvInitDefaults (const char *argv0, const char *instanceName) {
  CString s;
  char hostName[256];
  const char *str;
  int len;

  // Home2L version and build parameters ...
  EnvPut (buildVersionKey, buildVersion);
  EnvPut (buildDateKey, buildDate);
  EnvPut (buildOSKey, buildOS);
  EnvPut (buildArchKey, buildArch);

  // Main config file...
  str = getenv ("HOME2L_CONFIG");
  if (str) envConfig = str;
  EnvPut (envConfigKey, envConfig);

  // Host name...
#if !ANDROID
  if (gethostname (hostName, sizeof (hostName)) != 0) ERROR ("Cannot determine host (machine) name");
  EnvPut (envMachineNameKey, hostName);
#else
  //   In Android, 'gethostname (2)' just delivers "localhost". Hence we rely on the Java part of the
  //   app, which must have set the environment variable in advance...
#endif
  envMachineName = EnvGet (envMachineNameKey);

  // Process environment...
  envExecPathName = EnvPut (envExecPathNameKey, argv0);
  str = strrchr (argv0, '/');
  envExecName = EnvPut (envExecNameKey, str ? str + 1 : argv0);

  envPid = getpid ();
  EnvPut (envPidKey, StringF (&s, "%i", envPid));

  // Instance name ...
  if (!instanceName) {
    instanceName = envExecName;
    if (strncmp (instanceName, "home2l-", 7) == 0) instanceName += 7;
  }
  envInstanceName = EnvPut (envInstanceNameKey, instanceName);

  // Root directories ...
  str = getenv ("HOME2L_ROOT");
  if (!str) str = getenv ("PWD");
  if (!str) str = "/data/home2l";   // This is the default path under Android.
  envRootDir = EnvPut (envRootDirKey, str);

  EnvPut (envVarDirKey, envVarDir);
  EnvPut (envTmpDirKey, envTmpDir);

  // Droid ID...
  len = strlen (envMachineName);
  if (len >= 3) {
    if (envMachineName[len-3] >= '0' && envMachineName[len-3] <= '9'
        && envMachineName[len-2] >= '0' && envMachineName[len-2] <= '9'
        && envMachineName[len-1] >= '0' && envMachineName[len-1] <= '9')
      envDroidId = EnvPut (envDroidIdKey, envMachineName + 3);
  }
}





// *************************** Init/Done ***************************************


static char *paraAdditionalSections = NULL;


static void PrintBanner () {
  CString s;
  const char *title;

  title = EnvExecName ();
  if (strncmp (title, "home2l-", 7) != 0) title = "Home2L";
  s.SetF ("%s %s (%s) by Gundolf Kiefer", title, buildVersion, buildDate);
    // The first argument should normally be 'EnvExecName', but that may be 'NULL'.
  if (LoggingToSyslog ()) { INFO (s.Get ()); }
  else printf ("%s\n\n", s.Get ());
}


static void PrintUsage (const char *specOptions) {
  printf ("Usage:   %s [<confVar>=<value> ...] [<Options>]\n"
          "\n"
          "The options may be preceeded by arbitrary (re-)definitions of\n"
          "configuration variables.\n"
          "\n"
          "General options:\n"
          "  -h            : Print this help\n"
          "  -s <sections> : Define comma-separated list of additional\n"
          "                  configuration file sections\n"
          "  -c <conffile> : Set main configuration file [etc/home2l.conf]\n"
          "  -x <instname> : Set instance name [%s]\n",
          EnvExecName (), EnvInstanceName ());
  if (specOptions) printf ("\nTool-specific options:\n%s", specOptions);
}


static bool ParseGeneralOptions (int argc, char **argv, const char *specOptionsUsage) {
  CString s;
  int n;
  bool ok;

  ok = true;
  paraAdditionalSections = NULL;
  for (n = 1; n < argc && ok; n++) {
    if (argv[n][0] == '-') {
      switch (argv[n][1]) {
        case 'h':
          PrintUsage (specOptionsUsage);
          exit (0);
        case 's':
          paraAdditionalSections = argv[++n];
          break;
        case 'c':
          if (n < argc-1) envConfig = EnvPut (envConfigKey, GetAbsPath (&s, argv[++n], getenv ("PWD")));
          else ok = false;
          break;
        case 'x':
          if (n < argc-1) envInstanceName = EnvPut (envInstanceNameKey, argv[++n]);
          else ok = false;
          break;
      }
    }
  }
  return ok;
}


static void ParseConfAssignments (int argc, char **argv) {
  char *p;
  int n;

  for (n = 1; n < argc && argv[n][0] != '-'; n++) {  // traverse first non-option arguments
    p = strchr (argv[n], '=');
    if (!p) {
      WARNINGF (("Incorrect assignment '%s' - ignoring", argv[n]));
    }
    else {
      *p = '\0';
      EnvPut (argv[n], p+1);
      //~ INFOF(("ParseConfAssignments: Put '%s' = '%s'", argv[n], p+1));
      *p = '=';
    }
  }
}


static void ReadConfAssignmentsFromEnv () {
  char **argv, *env;
  int argc;

  env = getenv ("HOME2L_EXTRA");
  if (!env) return;
  StringSplit (env, &argc, &argv, INT_MAX, " ;");
  if (!argv) return;
  ParseConfAssignments (argc + 1, argv - 1);    // argv[0] will be skipped in 'ParseConfAssignments'
  free (argv[0]);
  free (argv);
}


void EnvInit (int argc, char **argv, const char *specOptionsUsage, const char *instanceName) {
  CString s;
  const char *key, *val;
  char *str, *token;
  int n;

  // Set very basic variables...
  EnvInitDefaults (argv[0], instanceName);

  // Print banner, init map with internal defaults and command line options...
  if (EnvHaveTerminal ()) PrintBanner ();
  if (!ParseGeneralOptions (argc, argv, specOptionsUsage)) {
    PrintUsage (specOptionsUsage);
    exit (3);
  }

  // Read main config file...
  //   Determine relevant sections...
  sectionSet.Clear ();
  sectionSet.Set (buildOS);               // OS environment
  sectionSet.Set (envMachineName);        // Host/machine name
  sectionSet.Set (envInstanceName);       // Instance name
  if (paraAdditionalSections) {           // Additional section passed as arguments...
    str = strdup (paraAdditionalSections);
    token = strtok (str, ",");
    while (token) {
      sectionSet.Set (token);
      token = strtok (NULL, ",");
    }
    free (str);
  }
  //~ sectionSet.DumpKeys ();
  //   Read the file...
  str = (char *) EnvGetPath (envConfigKey);     // convert config file path to absolute path
  ReadIniFile (str, &envMap);

  // Parse extra assignments from 'HOME2L_EXTRA' and the command line...
  ReadConfAssignmentsFromEnv ();
  ParseConfAssignments (argc, argv);

  // Switch to syslog if configured so...
  if (EnvGetBool (envSyslogKey, false)) LogToSyslog ();

  // Pre-initialize 'env...' variables...
  CEnvPara::GetAll (false);

  // Enable core dumps if requested...
#if !ANDROID
  if (envEnableCoreDump) {
    static const struct rlimit rUnlimited = { RLIM_INFINITY, RLIM_INFINITY };
    setrlimit (RLIMIT_CORE, &rUnlimited);
  }
#endif

  // Init localization...
  LangInit (EnvGetHome2lRootPath (&s, "locale"), envSysLocale);

  // Set some environment variables for child processes...
  s.SetF ("HOME2L_VAR=%s", envVarDir);
  putenv (s.Disown ());
  s.SetF ("HOME2L_TMP=%s", envTmpDir);
  putenv (s.Disown ());

  // Done...
  if (envDebug >= 1) {
    DEBUGF (1, ("Main configuration file is '%s'.", str));
    for (n = 0; n < envMap.Entries (); n++) {
      key = envMap.GetKey (n);
      val = envMap.Get (n)->Get ();
      if (strstr (key, "secret")) val = "<secret>";
      DEBUGF (1, ("  %s = %s", key, val));
    }
  }
}


void EnvDone () {
  EnvFlush ();
  LangDone ();
  LogClose ();
}





// ********************** Persistence ******************************************


static bool varPersistent = false;
static CString varFileName;
static bool varWriteThrough, varDirty;


void EnvInitPersistence (bool writeThrough, const char *_varFileName) {
  struct stat statBuf;

  // Init variables...
  varWriteThrough = writeThrough;
  if (_varFileName) EnvGetHome2lVarPath (&varFileName, _varFileName);
  else varFileName.SetF ("%s/home2l-%s.conf", envVarDir, EnvInstanceName ());
  varDirty = false;
  varPersistent = true;

  // Check for existince of a var file and eventually load it...
  if (stat (varFileName.Get (), &statBuf) == 0) {
    ReadIniFile (varFileName.Get (), &envMap);
    CEnvPara::GetAll (true);
  }
}


void EnvFlush () {
  FILE *f;
  int n, idx0, idx1;

  // Nothing to do? ...
  if (!varPersistent || !varDirty) return;

  // No "var.*" variables available? ...
  EnvGetPrefixInterval ("var.", &idx0, &idx1);
  if (idx0 >= idx1) return;

  // Try to write the file...
  f = fopen (varFileName.Get (), "wt");
  if (!f) {
    WARNINGF (("Unable to open '%s' for writing", varFileName.Get ()));
    return;
  }
  for (n = idx0; n < idx1; n++) {
    // TBD: Properly escape special characters and quotes in the the vaule string
    if (fprintf (f, "%s = \"%s\"\n", envMap.GetKey (n), envMap.Get (n)->Get ()) < 0) {
      WARNINGF (("Unable to write to '%s': %s", strerror (errno)));
      fclose (f);
      return;
    }
  }
  fclose (f);

  // Success ...
  varDirty = false;
}





// *************************** Get & Put ***************************************


const char *EnvGet (const char *key) {
  int n, idx;

  //~ INFOF (("# EnvGet ('%s')", key));
  if (strchr (key, ':')) {
    // Multiple keys are given: Try them sequentially...
    CSplitString keyList (key, INT_MAX, ":");
    for (n = 0; n < keyList.Entries (); n++) {
      idx = envMap.Find (keyList[n]);
      //~ INFOF (("#   trying '%s': %i", keyList[n], idx));
      if (idx >= 0) return envMap.Get (idx)->Get ();
    }
    DEBUGF (1, ("No matching configuration variable found for '%s' - assuming empty string.", key));
    return NULL;
  }
  else {
    // Simple case...
    idx = envMap.Find (key);
    if (idx >= 0) return envMap.Get (idx)->Get ();
    else {
      DEBUGF (1, ("Attempt to read non-existing configuration variable '%s' - assuming empty string.", key));
      return NULL;
    }
  }
}


const char *EnvPut (const char *key, const char *value) {
  CString valStr;
  int idx;
  //~ INFOF (("### Setting option: %s = %s", key, value));

  // Set the value...
  idx = -1;
  if (value) {
    valStr.SetC (value);
    idx = envMap.Set (key, &valStr);
    //~ INFOF (("###   value = %08x, valStr.ptr = %08x, envMap.Get ()->Get () = %08x", value, valStr.Get (), envMap.Get (key)->Get ()));
  }
  else
    envMap.Del (key);

  // Handle persistence...
  if (varPersistent) if (strncmp (key, "var.", 4) == 0) {
    varDirty = true;
    if (varWriteThrough) EnvFlush ();
  }

  // Done...
  return idx >= 0 ? envMap[idx]->Get () : NULL;
}


const char *EnvPut (const char *key, int value) {
  char buf[16];

  sprintf (buf, "%i", value);
  return EnvPut (key, buf);
}





// *************************** Get with type ***********************************


static void EnvWarn (bool warn, const char *key, const char *typeStr = NULL) {
  if (typeStr) { WARNINGF (("Configuration variable '%s' does not have a valid %s value.", key, typeStr)); }
  else if (warn) { WARNINGF (("Configuration variable '%s' is not defined.", key)); }
}


bool EnvGetString (const char *key, const char **ret, bool warn) {
  const char *_ret = EnvGet (key);
  if (_ret) {
    if (ret) *ret = _ret;
    return true;
  }
  else {
    EnvWarn (warn, key);
    return false;
  }
}


const char *EnvGetString (const char *key, const char *defaultVal, bool warn) {
  EnvGetString (key, &defaultVal, warn);
  return defaultVal;
}


bool EnvGetPath (const char *key, const char **ret, bool warn) {
  CString s, *val;
  int idx;
  const char *_ret;

  if ( (idx = envMap.Find (key)) < 0) _ret = NULL;
  else {
    val = envMap.Get (idx);
    val->Set (EnvGetHome2lRootPath (&s, val->Get ()));
    _ret = val->Get ();
  }
  if (_ret) {
    if (ret) *ret = _ret;
    return true;
  }
  else {
    EnvWarn (warn, key);
    return false;
  }
}


const char *EnvGetPath (const char *key, const char *defaultVal, bool warn) {
  EnvGetPath (key, &defaultVal, warn);
  return defaultVal;
}


bool EnvGetHostAndPort (const char *key, CString *retHost, int *retPort, int defaultPort, bool warn) {
  CString s;
  const char *sHostPort;
  char *p, *endPtr;
  int port;
  bool ok;

  ok = true;

  // Get key & check existence...
  sHostPort = EnvGet (key);
  if (!sHostPort) {
    EnvWarn (warn, key);
    retHost->Clear ();
    ok = false;
  }

  // Split and translate...
  port = 0;       // port found in the given host/port or alias
  if (ok) {
    retHost->Set (sHostPort);

    // Check, if given host/port has a port...
    p = (char *) strchr (retHost->Get (), ':');
    if (p) {
      *p = '\0';
      port = (int) strtol (p + 1, &endPtr, 0);
      if (*endPtr != '\0') ok = false;
    }
  }
  if (ok) {

    // Lookup alias...
    sHostPort = EnvGet (StringF (&s, "rc.resolve.%s", retHost->Get ()));
    if (sHostPort) {
      retHost->Set (sHostPort);

      // Check, if the alias has a port...
      p = (char *) strchr (retHost->Get (), ':');
      if (p) {
        *p = '\0';
        if (!port) {      // a locally defined port would dominate
          port = (int) strtol (p + 1, &endPtr, 0);
          if (*endPtr != '\0') ok = false;
        }
      }
    }
  }

  // Done...
  if (ok) {
    if (retPort) *retPort = port ? port : defaultPort;
  }
  else {
    EnvWarn (warn, key, "<host[:port]>");
    retHost->Clear ();
  }
  return ok;
}


bool EnvGetInt (const char *key, int *ret, bool warn) {
  const char *strVal;
  char *endPtr;
  int val;

  strVal = EnvGet (key);
  if (!strVal) {
    EnvWarn (warn, key);
    return false;
  }
  val = (int) strtol (strVal, &endPtr, 0);
  if (*strVal == '\0' || *endPtr != '\0') {
    EnvWarn (warn, key, "integer");
    return false;
  }
  if (ret) *ret = val;
  return true;
}


int EnvGetInt (const char *key, int defaultVal, bool warn) {
  EnvGetInt (key, &defaultVal, warn);
  return defaultVal;
}


bool EnvGetFloat (const char *key, float *ret, bool warn) {
  const char *strVal;
  char *endPtr;
  float val;

  strVal = EnvGet (key);
  if (!strVal) {
    EnvWarn (warn, key);
    return false;
  }
  val = strtof (strVal, &endPtr);
  if (*strVal == '\0' || *endPtr != '\0') {
    EnvWarn (warn, key, "float");
    return false;
  }
  if (ret) *ret = val;
  return true;
}


float EnvGetFloat (const char *key, float defaultVal, bool warn) {
  EnvGetFloat (key, &defaultVal, warn);
  return defaultVal;
}


bool EnvGetBool (const char *key, bool *ret, bool warn) {
  const char *strVal;
  bool val;

  strVal = EnvGet (key);
  if (!strVal) {
    EnvWarn (warn, key);
    return false;
  }
  if (strchr ("0fF-", strVal[0])) { val = false; }
  else if (strchr ("1tT+", strVal[0])) { val = true; }
  else {
    EnvWarn (warn, key, "boolean");
    return false;
  }
  if (ret) *ret = val;
  return true;
}


bool EnvGetBool (const char *key, bool defaultVal, bool warn) {
  EnvGetBool (key, &defaultVal, warn);
  return defaultVal;
}





// *************************** Advanced dictionary access **********************


void EnvGetPrefixInterval (const char *prefix, int *retIdx0, int *retIdx1) {
  envMap.PrefixSearch (prefix, retIdx0, retIdx1);
}


const char *EnvGetKey (int idx) {
  return envMap.GetKey (idx);
}


const char *EnvGetVal (int idx) {
  return envMap.Get (idx)->Get ();
}


CDictRaw *EnvGetKeySet () {
  return &envMap;
}





// ********************** Automated declaration & documentation ****************


CEnvPara *CEnvPara::first = NULL;


CEnvPara::CEnvPara (const char *_key, EEnvParaType _type, void *_pVar) {
  key = _key;
  type = _type;
  pVar = _pVar;
  next = first;
  first = this;
}


void CEnvPara::GetAll (bool withVarKeys) {
  CEnvPara *ep, **pEp;
  //~ bool isVarKey;

  pEp = &first;
  while ( (ep = *pEp) ) {
    //~ isVarKey = strncmp (ep->key, "var.", 4) == 0;
    //~ if (isVarKey == varKeys) switch (ep->type) {
    if (withVarKeys || strncmp (ep->key, "var.", 4) != 0) {
      switch (ep->type) {
        case eptString:
          EnvGetString (ep->key, (const char **) ep->pVar);
          break;
        case eptPath:
          EnvGetPath (ep->key, (const char **) ep->pVar);
          break;
        case eptInt:
          EnvGetInt (ep->key, (int *) ep->pVar);
          break;
        case eptFloat:
          EnvGetFloat (ep->key, (float *) ep->pVar);
          break;
        case eptBool:
          EnvGetBool (ep->key, (bool *) ep->pVar);
          break;
        default:
          ASSERT(false);
      }
      *pEp = ep->next;      // remove the element from the chained list
    }
    else
      pEp = &(ep->next);    // move pointer forward, leave element in list
  }
}
