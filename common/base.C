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


#include "base.H"

#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/types.h>  // for 'readdir' & friends
#include <sys/stat.h>
#include <dirent.h>     // for 'readdir' & friends
#include <pwd.h>



#include "env.H"





// ***************** Basic definitions and functions ***************************


size_t Write (int fd, const void *buf, size_t count) {
  ssize_t n, total;

  total = 0;
  while (count > 0) {
    n = write (fd, ((const uint8_t *) buf) + total, count);
    if (n < 0) return total;    // error
    total += n;
    count -= n;
  }
  return total;
}


size_t Read (int fd, void *buf, size_t count) {
  ssize_t n, total;

  total = 0;
  while (count > 0) {
    n = read (fd, ((uint8_t *) buf) + total, count);
    if (n <= 0) {
      if (n == 0) errno = 0;    // end-of-file => 'errno' has not been set
      return total;             // error or end-of-file
    }
    total += n;
    count -= n;
  }
  return total;
}


static bool DoMakeDir (char *absPath, bool setHome2lGroup) {
  CString s;
  struct stat fileStat;
  struct passwd *pwEntry;
  char *p;
  bool ok;

  //~ INFOF (("# DoMakeDir ('%s')", absPath));

  // Check if 'absPath' already exists...
  if (lstat (absPath, &fileStat) == 0) {
    if (S_ISDIR (fileStat.st_mode)) return true;    // ok (probably), nothing to do
    WARNINGF (("Cannot create directory '%s': A file with the same name is in the way", absPath));
    return false;
  }

  // Make parent directory...
  p = strrchr (absPath, '/');
  if (!p || p == absPath) {
    WARNINGF (("Cannot determine parent directory of '%s'", absPath));
    return false;   // no parent
  }
  *p = '\0';
  ok = MakeDir (absPath, setHome2lGroup);
  *p = '/';
  if (!ok) return false;    // parent creation failed (warning has already been emitted).

  // Make current directory with correct attributes...
  if (mkdir (absPath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
    WARNINGF (("Cannot create directory '%s': %s", absPath, strerror (errno)));
    ok = false;
  }

  // Try to set group ownership (failure is not reported as error)...
  if (setHome2lGroup) {
#if !ANDROID
    pwEntry = getpwnam (HOME2L_USER);
    if (!pwEntry) WARNINGF (("Cannot identify user '" HOME2L_USER "': %s", strerror (errno)));
    else {
      //~ INFOF (("# ... chown (%i)", pwEntry->pw_gid));
      if (chown (absPath, (uid_t) -1, pwEntry->pw_gid) != 0)
        WARNINGF (("Failed to set group ownership on '%s': %s", absPath, strerror (errno)));
    }
#else // !ANDROID
  if (chmod (absPath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
    WARNINGF (("Failed to set permissions for '%s': %s", absPath, strerror (errno)));
#endif // !ANDROID
  }

  // Done...
  return ok;
}


bool MakeDir (const char *relOrAbsPath, bool setHome2lGroup) {
  CString s;
  char *realPath;
  char link[1024];    // buffer for a symbolic link
  int len;

  EnvGetHome2lRootPath (&s, relOrAbsPath);

  // Try to resolve a symbolic links in the path ...
  // a) ... using 'readlink', which works if a symbolic link is pointing nowhere yet (e.g. to a tmpfs), 'realpath' would fail on that.
  len = readlink (s.Get (), link, sizeof (link));
  if (len >= 0 && len < (int) sizeof (link)) {
    link[len] = '\0';
    DEBUGF (1, ("Resolving link: %s -> %s", s.Get (), link));
    s.SetC (link);
  }
  // b) ... using 'realpath', which is probably more flexible, but fails on a symbolic link pointing to a not-yet created directory.
  else {
    realPath = realpath (s.Get (), NULL);
    if (realPath) s.SetO (realPath);
    else DEBUGF (1, ("No resolvable symbolic links on the path '%s': %s", s.Get (), strerror (errno)));
  }

  // Go ahead ...
  return DoMakeDir ((char *) s.Get (), setHome2lGroup);
}


bool UnlinkTree (const char *relOrAbsPath, const char *skipPattern) {
  CSplitString skipList;
  const char *skipItem;
  CString s;
  DIR *dir;
  struct dirent *dirEnt;
  bool skip;
  int n;

  //~ INFOF (("### UnlinkTree ('%s', '%s')", relOrAbsPath, skipPattern));
  dir = opendir (EnvGetHome2lRootPath (&s, relOrAbsPath));
  if (!dir) {
    WARNINGF (("Failed to open directory '%s': %s", relOrAbsPath, strerror (errno)));
    return false;
  }
  if (skipPattern) skipList.Set (skipPattern);
  while ( (dirEnt = readdir (dir)) ) {

    // Check skip conditions ...
    skip = false;
    if (dirEnt->d_name[0] == '.' && (dirEnt->d_name[1] == '\0' || (dirEnt->d_name[1] == '.' && (dirEnt->d_name[2] == '\0'))))
      skip = true;                                          // skip "." and ".."
    for (n = 0; n < skipList.Entries () && !skip; n++) {    // skip if given by pattern ...
      skipItem = skipList.Get (n);
      if (skipItem[0] == '/' && strcmp (skipItem + 1, dirEnt->d_name) == 0) skip = true;
    }

    // Go ahead...
    if (skip) {
      //~ INFOF (("###   skipping '%s'", dirEnt->d_name));
    } else {
      s.SetF ("%s/%s", relOrAbsPath, dirEnt->d_name);
      s.PathNormalize ();
      //~ INFOF (("###   deleting '%s'", s.Get ()));
      if (dirEnt->d_type == DT_DIR) {
        if (!UnlinkTree (s.Get (), NULL)) {
          closedir (dir);
          return false;
        }
        DEBUGF (2, ("Removing directory '%s'.", s.Get ()));
        if (rmdir (s.Get ()) != 0) {
          WARNINGF (("Failed to unlink directory '%s': %s", s.Get (), strerror (errno)));
          closedir (dir);
          return false;
        }
      }
      else {
        DEBUGF (2, ("Removing file '%s'.", s.Get ()));
        if (unlink (s.Get ()) != 0) {
          WARNINGF (("Failed to unlink file '%s': %s", s.Get (), strerror (errno)));
          closedir (dir);
          return false;
        }
      }
    } // if (skip)

  }
  closedir (dir);
  return true;
}


bool ReadDir (const char *relOrAbsPath, class CKeySet *ret) {
  CString s;
  DIR *dir;
  struct dirent *dirEnt;
  bool skip;

  // Open directory ...
  dir = opendir (EnvGetHome2lRootPath (&s, relOrAbsPath));
  if (!dir) {
    WARNINGF (("Failed to open directory '%s': %s", relOrAbsPath, strerror (errno)));
    return false;
  }

  // Read directory ...
  ret->Clear ();
  errno = 0;
  while ( (dirEnt = readdir (dir)) ) {

    // Check skip conditions ...
    skip = false;
    if (dirEnt->d_name[0] == '.' && (dirEnt->d_name[1] == '\0' || (dirEnt->d_name[1] == '.' && (dirEnt->d_name[2] == '\0'))))
      skip = true;                                          // skip "." and ".."

    // Add to dictionary ...
    if (!skip) {
      s.SetC (dirEnt->d_name);
      if (dirEnt->d_type == DT_DIR) s.Append ('/');
      ret->Set (s.Get ());
    }
  }

  // Done ...
  if (!errno) closedir (dir);
  if (errno) {
    WARNINGF (("Failed to read directory '%s': %s", relOrAbsPath, strerror (errno)));
    return false;
  }
  return true;
}





// ***************** Logging and Debugging *************************************


#if WITH_DEBUG == 1
ENV_PARA_INT("debug", envDebug, 0);
  /* Level of debug output
   *
   * A value of 0 disables debug output messages. Higher values increase the verbosity of
   * the debug output.
   */
#endif


#if ANDROID


#include <android/log.h>


FLogCbMessage *logCbMessage = NULL;
FLogCbToast *logCbToast = NULL;


void LogSetCallbacks (FLogCbMessage *_cbMessage, FLogCbToast *_cbToast) {
  logCbMessage = _cbMessage;
  logCbToast = _cbToast;
}


#else // ANDROID


#include <syslog.h>
#include <execinfo.h>

static bool syslogOpen = false;


void LogToSyslog () {
  static CString ident;
  openlog (StringF (&ident, "home2l-%s", EnvInstanceName ()), 0, LOG_USER);
  syslogOpen = true;
}


void LogClose () {
  if (syslogOpen) closelog ();
}


bool LoggingToSyslog () { return syslogOpen; }


void LogStack () {
  void *array [10];
  char **strings;
  int i, size;

  size = backtrace (array, sizeof (array) / sizeof (void *));
  strings = backtrace_symbols (array, size);
  //~ printf ("Obtained %zd stack frames.\n", size);
  for (i = 0; i < size; i++) LogPrintf ("  %s", strings[i]);
  free (strings);
}


#endif // ANDROID


static const char *logHead, *logFile;
static int logLine;


void LogPara (const char *_logHead, const char* _logFile, int _logLine) {
  const char *p;
  int n;

  //~ __android_log_print (ANDROID_LOG_DEBUG, "home2l", "LogPrintf\n");
  p = _logFile + strlen (_logFile);
  for (n = 2; p > _logFile && n > 0; p--) if (p[-1] == '/') n--;

  logHead = _logHead;
  logFile = p;
  logLine = _logLine;
}


void LogPrintf (const char *format, ...) {
  char buf [1024];
#if ANDROID
  char msg[1024];
#endif
  va_list ap;
  int prio;

  if (logHead[0] == 'D' && !envDebug) return;   // do not output debug messages

  va_start (ap, format);
  vsnprintf (buf, sizeof(buf), format, ap);

#if ANDROID
  switch (logHead[0]) {
    case 'I':
      if (logCbToast)
        if (buf[0] == '-' && buf[2] == '-' && buf[3] == ' ' && (buf[1] == 't' || buf[1] == 'T'))
          logCbToast (buf + 4, buf[1] == 'T');
      prio = ANDROID_LOG_INFO;
      break;
    case 'W':
    case 'S': // security warning
      prio = ANDROID_LOG_WARN;
      break;
    case 'E':
      //~ INFOF (("### Error: logCbMessage = %08x", (uint32_t) logCbMessage));
      if (logCbMessage) {
        snprintf (msg, sizeof(msg), "%s\n(%s:%i)", buf, logFile, logLine);
        logCbMessage ("Error", msg);
      }
      prio = ANDROID_LOG_ERROR;
      break;
    default:
      prio = ANDROID_LOG_DEBUG;
  }
  __android_log_print (prio, "home2l", logHead[0] == 'S' ? "%s:%i: SECURITY: %s\n" : "%s:%i: %s\n", logFile, logLine, buf);
#else
  //~ fprintf (stderr, "%s %s:%i: %s\n", logHead, logFile, logLine, buf);
  if (syslogOpen) {
    switch (logHead[0]) {
      case 'I':
        prio = LOG_INFO;
        break;
      case 'W':
      case 'S': // security warning
        prio = LOG_WARNING;
        break;
      case 'E':
        prio = LOG_ERR;
        break;
      default:
        prio = LOG_DEBUG;
    }
    syslog (prio, "%s%s [%s:%i]\n", logHead[0] == 'S' ? "SECURITY: " : "", buf, logFile, logLine);
  }
  else
    fprintf (stderr, "%s:%i: [%s] %s: %s\n", logFile, logLine, EnvExecName (), logHead, buf);
  fflush (stderr);
#endif
}





// ***************** Localization and language *********************************


// **** Translation using GNU gettext *****


#if USE_GNU_GETTEXT


#include <locale.h>


void LangInit (const char *localeDir, const char *locale) {
  setlocale (LC_MESSAGES, locale ? locale : "");
  bindtextdomain ("home2l", localeDir);
  bind_textdomain_codeset ("home2l", "UTF-8");
  textdomain ("home2l");
}


void LangDone () {}



// **** Translation using internal implementation *****


#else // USE_GNU_GETTEXT

#include <sys/mman.h>
#include <sys/stat.h>


typedef struct {
  uint32_t magic;
  uint32_t formatRevision;
  uint32_t strings;
  uint32_t tableOriginalOfs;
  uint32_t tableTranslationOfs;
  uint32_t hashTableSize;
  uint32_t hashTableOfs;
} TMoHeader;


typedef struct {
  uint32_t len;
  uint32_t ofs;
} TMoStringDesc;


static CString moFileName;
static int moFd = -1;   // -1 = never opened, 0 = already tried to open, but without success
static uint8_t *moContent = NULL;
static unsigned moFileSize = 0;
static int moStrings = 0;
static TMoStringDesc *moTableOriginal, *moTableTranslation;


void LangInit (const char *localeDir, const char *locale) {
  CString s;
  char *p;

  // Fallback with locale to environment settings...
  if (!locale) {
    locale = (const char *) getenv ("LC_ALL");
    if (!locale) locale = (const char *) getenv ("LC_MESSAGES");
    if (!locale) locale = (const char *) getenv ("LANG");
    if (locale) {
      // Eventually cut off a suffix behind '.'
      s.Set (locale);
      p = (char *) strchr (s.Get (), '.');
      if (p) p[0] = '\0';
      locale = s.Get ();
    }
  }

  // Sanity...
  if (!localeDir || !locale) {
    moFd = 0;   // We have no .mo file and know this.
    return;
  }

  // Store MO file name for a later lazy translation ...
  moFileName.SetF ("%s/%s/LC_MESSAGES/home2l.mo", localeDir, locale);
  moFd = -1;   // mark as "to be opened"
}


static void LangOpenMoFile () {
  struct stat statInfo;
  TMoHeader *header;
  TMoStringDesc *desc;
  int n;
  bool ok;

  // Open MO file ...
  moFd = open (moFileName.Get (), O_RDONLY);
  if (moFd < 0) {
    WARNINGF (("Failed to open translation file '%s'", moFileName.Get ()));
    moFd = 0;
    return;
  }

  // MMap MO file ...
  fstat (moFd, &statInfo);
  moFileSize = statInfo.st_size;
  moContent = (uint8_t *) mmap (NULL, moFileSize, PROT_READ, MAP_PRIVATE, moFd, 0);
  //~ moContent = MALLOC (uint8_t, moFileSize);
  //~ read (moFd, moContent, moFileSize);
  if (!moContent) {
    WARNINGF (("Failed to mmap translation file '%s'", moFileName.Get ()));
    close (moFd);
    moFd = 0;
    return;
  }

  // Examine header and setup variables ...
  header = (TMoHeader *) moContent;
  if (header->magic != 0x950412de || header->formatRevision != 0) {
    WARNINGF (("Failed to read translation file '%s': wrong magic number (0x%08x) or format revision (0x%x)", moFileName.Get (), header->magic, header->formatRevision));
    LangDone ();
    return;
  }
  moStrings = header->strings;
  moTableOriginal = (TMoStringDesc *) (moContent + header->tableOriginalOfs);
  moTableTranslation = (TMoStringDesc *) (moContent + header->tableTranslationOfs);

  // Sanity (for security) ...
  ok = moStrings < 0x1000000 && header->tableOriginalOfs < 0x1000000 && header->tableTranslationOfs < 0x1000000
        && (uint8_t *) (moTableOriginal + moStrings) < moContent + moFileSize
        && (uint8_t *) (moTableTranslation + moStrings) < moContent + moFileSize;
  ASSERT (ok);
  if (ok) {
    for (n = 0; n < moStrings && ok; n++) {
      desc = &moTableOriginal[n];
      if (desc->ofs > moFileSize || desc->ofs + desc->len > moFileSize) ok = false;
      ASSERT (ok);
      if (moContent [desc->ofs + desc->len] != '\0') ok = false;
      ASSERT (ok);
      desc = &moTableTranslation[n];
      if (desc->ofs > moFileSize || desc->ofs + desc->len > moFileSize) ok = false;
      ASSERT (ok);
      if (moContent [desc->ofs + desc->len] != '\0') ok = false;
      ASSERT (ok);
    }
  }
  if (!ok) {
    WARNINGF (("Failed to read translation file '%s': strange arguments in header"));
    LangDone ();
    return;
  }
}


const char *LangGetText (const char *msgId) {
  int n0, n1, idx, c;

  // Sanity ...
  if (!msgId) return NULL;
  if (moFd < 0) LangOpenMoFile ();

  // Binary search in table of original strings ...
  n0 = 0;
  n1 = moStrings - 1;
  while (n1 >= n0) {
    idx = (n0 + n1) / 2;
    c = strcmp (msgId, (const char *) moContent + moTableOriginal[idx].ofs);
    //~ INFOF(("### Compared '%s' with %i:'%s' -> %i", msgId, idx, (const char *) moContent + moTableOriginal[idx].ofs, c));
    if (c == 0)     return (const char *) moContent + moTableTranslation[idx].ofs;
    else if (c < 0) n1 = idx - 1;
    else            n0 = idx + 1;
  }
  return msgId;
}


void LangDone () {

  // Mark tables clear ...
  moStrings = 0;

  // MUnmap MO file...
  if (moContent) {
    ASSERT (moFd > 0);
    munmap (moContent, moFileSize);
    moContent = NULL;
  }

  // Close MO file ...
  if (moFd > 0) {
    close (moFd);
    moFd = 0;   // Later calls to LangGetText() may happen, and they will return untranslated
                // strings, which is the safest sokution in this case.
  }
}



#endif



// ***** Additional helpers *****


void LangTranslateNumber (char *str) {
  static char decPoint = '\0';
  char *p;

  // Sanity...
  if (!str) return;

  // TRANSLATORS: Set the first character to the locale's numerical decimal point. All other characters are ignored.
  if (!decPoint) decPoint = _(". (decimal point)") [0];

  // Translate if necessary...
  if (decPoint != '.') for (p = str; *p; p++) if (*p == '.') *p = decPoint;
}









// *************************** Strings *****************************************


#define MAX_TTS_THREADS   32      // Maximum number of threads using the 'GetTTS ()' function

const char CString::emptyStr[] = "";



// ***** Misc. helpers *****


class CString *GetThreadTempString () {
  static int ttsThreads = 0;
  static pthread_t ttsThreadList[MAX_TTS_THREADS];
  static CString ttsStringList[MAX_TTS_THREADS];
  static CMutex mutex;

  pthread_t self;
  int n;

  self = pthread_self ();
  n = 0;
  mutex.Lock ();
  while (ttsThreadList[n] != self) {
    if (n >= ttsThreads) {
      ASSERT (ttsThreads < MAX_TTS_THREADS);
      ttsThreads++;
      ttsThreadList[n] = self;
    }
    else n++;
  }
  mutex.Unlock ();
  return &ttsStringList[n];
}


const char *StringF (CString *ret, const char *fmt, ...) {
  va_list ap;

  va_start (ap, fmt);
  ret->SetFV (fmt, ap);
  va_end (ap);
  return ret->Get ();
}


const char *StringF (const char *fmt, ...) {
  CString *tts;
  va_list ap;

  tts = GetTTS ();
  va_start (ap, fmt);
  tts->SetFV (fmt, ap);
  va_end (ap);
  return tts->Get ();
}


bool IntFromString (const char *str, int *ret, int radix) {
  long lRet;
  char *endPtr;

  while (str[0] == '0' && str[1] >= '0' && str[1] <= '9') str++;
    // remove leading 0's to prevent strtol to interpret the number in octal
  lRet = strtol (str, &endPtr, radix);
  if (*str != '\0' && *endPtr == '\0') {
    *ret = (int) lRet;
    return true;
  }
  else
    return false;
}


int ValidIntFromString (const char *str, int defaultVal, int radix) {
  IntFromString (str, &defaultVal, radix);
  return defaultVal;
}


bool FloatFromString (const char *str, float *ret) {
  float fRet;
  char *endPtr;

  fRet = strtof (str, &endPtr);
  if (*str != '\0' && *endPtr == '\0') {
    //~ INFOF (("### FloatFromString ('%s') = %f", str, fRet));
    *ret = fRet;
    return true;
  }
  else
    return false;
}


float ValidFloatFromString (const char *str, float defaultVal) {
  FloatFromString (str, &defaultVal);
  return defaultVal;
}


void StringStrip (char *str, const char *sepChars) {
  char *src, *dst;

  if (!sepChars) return;
  src = dst = str;
  while (src[0] && strchr (sepChars, src[0])) src++;
  while (src[0]) *dst++ = *src++;
  while (dst > str && strchr (sepChars, dst[-1])) dst--;
  *dst = '\0';
}


void StringSplit (const char *str, int *retArgc, char ***retArgv, int maxArgc, const char *sepChars, char **retRef) {
  int argc;
  char *c, sep, *buf, *src, *dst;

  // Sanity...
  *retArgc = 0;
  *retArgv = NULL;
  if (retRef) *retRef = NULL;
  if (!str || !sepChars || !sepChars[0]) return;

  // Copy and strip...
  buf = strdup (str);
  src = dst = buf;
  while (src[0] && strchr (sepChars, src[0])) src++;
  while (src[0]) *dst++ = *src++;
  while (dst > buf && strchr (sepChars, dst[-1])) dst--;
  *dst = '\0';
  if (!buf[0]) {      // buffer empty => return empty array
    free (buf);
    return;
  }
  if (retRef) *retRef = buf - src + dst;      // return reference pointer
  //~ INFOF (("### StringSplit: stripped: '%s' -> '%s'", str, buf));
  //~ ASSERT (buf[strlen(buf)-1] != ' ');

  // Pass 0: Unify seperators...
  sep = sepChars[0];
  for (c = buf; c[0]; c++) if (strchr (sepChars, c[0])) c[0] = sep;

  // Pass 1: Count arguments (= word beginnings)...
  argc = 1;
  for (c = buf + 1; c[0]; c++)
    if (c[-1] == sep && c[0] != sep) argc++;
  *retArgc = MIN (argc, maxArgc);

  // Pass 2: Write out word beginnings...
  *retArgv = MALLOC (char *, argc);
  (*retArgv) [0] = buf;
  argc = 1;
  for (c = buf + 1; c[0] && argc < maxArgc; c++)
    if (c[-1] == sep && c[0] != sep) (*retArgv) [argc++] = c;

  // Pass 3: Generate terminating null-characters...
  for (c = buf + 1; c < (*retArgv) [argc-1]; c++)
    if (c[-1] != sep && c[0] == sep) c[0] = '\0';
}


bool CharIsWhiteSpace (char c) {
  return strchr (WHITESPACE, c) != NULL;
}


void PathNormalize (char *str) {
  char *src, *dst;

  // Sanity...
  if (!str) return;
  if (!str[0]) return;

  // Remove double slashes...
  src = dst = str;
  while (src[1]) {
    if (src[0] != '/' || src[1] != '/') *(dst++) = *src;
    src++;
  }
  *(dst++) = *src;
  *dst = '\0';

  // Remove "/." and "/*/.." ...
  src = dst = str;
  while (src[0] && src[1]) {
    if (src[0] == '/' && src[1] == '.' && (src[2] == '/' || src[2] == '\0')) {
      // same directory: skip...
      do { src++; } while (src[0] != '/' && src[0] != '\0');
    }
    else if (src[0] == '/' && src[1] == '.' && src[2] == '.') {
      // directory up: return to previous component...
      do { dst--; } while (dst >= str && dst[0] != '/');
      do { src++; } while (src[0] != '/' && src[0] != '\0');
      if (str[0] == '/' && dst == str && *src != '/') dst++;   // do not remove leading '/'
    }
    else {
      // normal character: copy...
      *dst++ = *src++;
    }
  }
  *(dst++) = *src;
  *dst = '\0';
}


void PathRemoveTrailingSlashes (char *str) {
  char *p;

  p = strrchr (str, '/');
  if (p) if (p[1] == '\0') {
    while (p > str && p[-1] == '/') p--;
    p[0] = '\0';
  }
}


const char *PathLeaf (const char *str) {
  const char *p = strrchr (str, '/');
  return p ? p + 1 : str;
}


const char *GetAbsPath (CString *ret, const char *relOrAbsPath, const char *defaultPath) {
  //~ INFOF (("EnvGetHome2lRootPath ('%s')", relOrAbsPath));
  if (!relOrAbsPath)
    ret->Set (defaultPath);
  else if (relOrAbsPath[0] == '/' || !defaultPath)
    ret->Set (relOrAbsPath);
  else {
    ret->Set (defaultPath);
    ret->Append ("/");
    ret->Append (relOrAbsPath);
  }
  ret->PathNormalize ();
  return ret->Get ();
}


const char *GetAbsPath (const char *relOrAbsPath, const char *defaultPath) {
  return GetAbsPath (GetTTS (), relOrAbsPath, defaultPath);
}





// ***** CSplitString *****


void CSplitString::Clear () {
  if (argv) {
    free (argv[0]);
    free (argv);
    argv = NULL;
  }
  argc = 0;
  ref = NULL;
}


int CSplitString::GetIdx (int pos) {
  int n;

  for (n = 0; n < argc; n++) if (argv[n] - ref > pos) return n - 1;
  return argc - 1;
}





// ***** Transcoding helpers *****


const char *ToUtf8 (const char *iso8859str) {
  CString *ret = GetTTS ();

  ret->SetFromIso8859 (iso8859str);
  return ret->Get ();
}


const char *ToIso8859 (const char *str) {
  CString *ret = GetTTS ();

  ret->SetAsIso8859 (str);
  return ret->Get ();
}



// ***** Initialization *****


void CString::Set (const char *str, int maxLen) {
  int len;

  if (!str) Clear ();
  else {
    len = strlen (str);
    if (maxLen < len) len = maxLen;
    if (size) ptr[0] = '\0'; else ptr = (char *) emptyStr;  // accelerate 'SetSize' by resetting to empty string first
    if (len > 0) {
      SetSize (len+1);
      strncpy (ptr, str, len);
      ptr[len] = '\0';
    }
  }
}


void CString::SetF (const char *fmt, ...) {
  va_list ap;
  va_start (ap, fmt);
  SetFV (fmt, ap);
  va_end (ap);
}


void CString::SetFV (const char *fmt, va_list ap) {
  Clear ();
  if (fmt) {
    // TBD: need 'va_copy`? -> man 3 stdarg
    vasprintf (&ptr, fmt, ap);
    size = strlen (ptr) + 1;
  }
}


void CString::SetC (const char *_ptr) {
  if (size) {
    free (ptr);
    size = 0;
  }
  ptr = (char *) (_ptr ? _ptr : emptyStr);
}


void CString::SetO (const char *_ptr) {
  SetC (_ptr);
  if (_ptr) size = strlen (_ptr) + 1;
}


char *CString::Disown () {
  if (!size) ptr = strdup (ptr);    // make dynamic copy if memory was not owned before
  size = 0;
  return ptr;
}



// **** Transcoding ****


void CString::SetFromIso8859 (const char *iso8859str) {
  const char *src;
  char *dst, c;

  if (!iso8859str) { Clear (); return; }
  SetSize (2 * strlen (iso8859str) + 1);
  src = iso8859str;
  dst = ptr;
  while ( (c = *src++) ) {
    if (!(c & 0x80)) *dst++ = c;
    else {
      *dst++ = 0xc0 | ((unsigned char) c >> 6);
      *dst++ = 0x80 | (c & 0x3f);
    }
  }
  *dst = '\0';
}


void CString::SetAsIso8859 (const char *str) {
  const char *src;
  char *dst, c;
  bool error;

  if (!str) { Clear (); return; }
  SetSize (strlen (str) + 1);
  error = false;
  src = str;
  dst = ptr;
  while ( (c = *src++) ) {
    if (!(c & 0x80)) *dst++ = c;
    else if ((c & 0xfe) == 0xc2) {
      *dst = (c & 1) << 6;
      c = *src++;
      if ((c & 0xc0) == 0x80) *dst |= (c & 0xbf);
      else {
        *dst = '?';
        error = true;
      }
      dst++;
    }
    else {
      *dst++ = '?';
      error = true;
    }
  }
  *dst = '\0';
  if (error) WARNINGF(("Cannot encode string to ISO 8859: '%s'", ptr));
}



// ***** Modifications *****


void CString::Del (int n0, int dn) {
  char *p;
  int len;

  len = strlen (ptr);
  if (n0  > len - dn) dn = len - n0;
  if (dn <= 0) return;
  if (!size) SetSize (len-dn+1);    // copy-on-write
  for (p = ptr + n0 + dn; p <= ptr + len; p++) p[-dn] = p[0];
}


void CString::Insert (int n0, int dn, int *retInsPos) {
  char *p;
  int len;

  len = strlen (ptr);
  if (n0 > len) n0 = len;
  SetSize (len+dn+1);
  for (p = ptr + len; p >= ptr + n0; p--) p[dn] = p[0];
  if (retInsPos) *retInsPos = n0;
}


void CString::Insert (int n0, char c) {
  Insert (n0, 1, &n0);
  ptr[n0] = c;
}


void CString::Insert (int n0, const char *str, int maxLen) {
  int len = 0;
  while (str[len] && len < maxLen) len++;
  Insert (n0, len, &n0);
  memcpy (ptr + n0, str, len);
}


void CString::InsertF (int n0, const char *fmt, ...) {
  va_list ap;
  va_start (ap, fmt);
  InsertFV (n0, fmt, ap);
  va_end (ap);
}


void CString::InsertFV (int n0, const char *fmt, va_list ap) {
  CString sub;

  sub.SetFV (fmt, ap);
  Insert (n0, sub.Get ());
}


void CString::AppendF (const char *fmt, ...) {
  va_list ap;
  va_start (ap, fmt);
  AppendFV (fmt, ap);
  va_end (ap);
}



// ***** Extras *****


int CString::LFind (char c) {
  for (char *p = ptr; p && *p; p++) if (*p == c) return p - ptr;
  return -1;
}


int CString::RFind (char c) {
  int ret = -1;

  for (char *p = ptr; p && *p; p++) if (*p == c) ret = p - ptr;
  return ret;
}


int CString::Compare (const char *str2) {
  return strcmp (ptr, str2);
}


void CString::Strip (const char *sepChars) {
  MakeWriteable ();
  StringStrip (ptr, sepChars);
}


void CString::Split (CSplitString *args, int maxArgc, const char *sepChars) {
  args->Set (ptr, maxArgc, sepChars);
}


void CString::AppendFByLine (const char *fmt, const char *text) {
  CString s;
  const char *p;

  if (!text) return;

  do {
    p = strchr (text, '\n');
    if (!p) p = text + strlen (text);
    s.Set (text, p - text);
    text = p;
    if (text[0] == '\n') text++;
    AppendF (fmt, s.Get ());
  } while (text[0]);
}


static bool IsToEscape (char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ? false : true;
}


static int HexValue (char c) {
  if (c >= '0' && c <= '9') return (int) c - '0';
  if (c >= 'a' && c <= 'f') return (int) c - 'a' + 10;
  return -1;  // error code
}


static const char escapeKeys[] = "nrt\\s0";
static const char escapeVals[] = { 0x0a, 0x0d, 0x09, 0x5c, 0x20, 0x00 };


void CString::AppendEscaped (const char *s, int maxChars) {
  const char *src, *p;
  char *dst, *dst0, c;
  int n, k;
  bool empty;

  // Sanity & special cases...
  empty = false;
  if (!s) empty = true; else if (!s[0]) empty = true;
  if (empty) {
    Append ("\0");
    return;
  }

  // Count number of new characters and make space for appending...
  n = 0;
  for (src = s; *src && n < maxChars; src++) n += IsToEscape (*src) ? 4 : 1;
  k = strlen (ptr);
  SetSize (k + n + 1);

  // Do the transcoding...
  dst = dst0 = ptr + k;
  src = s;
  while (*src) {
    c = *src;
    if (IsToEscape (c)) {
      *(dst++) = '\\';
      p = strchr (escapeVals, c);
      if (p) *(dst++) = escapeKeys[p-escapeVals];
      else {
        sprintf (dst, "x%02x", c);
        dst += 3;
      }
    }
    else *(dst++) = c;
    if (dst - dst0 > maxChars - 3) {    // Abbreviate?
      dst = dst0 + maxChars;
      dst[-3] = dst[-2] = dst[-1] = '.';
      break;
    }
    src++;
  }

  // Done...
  *dst = '\0';
}


bool CString::AppendUnescaped (const char *s) {
  const char *src, *p;
  char *dst, c;
  int n, k, hex;
  bool ok;

  // Sanity...
  if (!s) return true;
  if (!s[0]) return true;

  // Prepare space...
  n = strlen (s);
  k = strlen (ptr);
  SetSize (k + n + 1);

  // Main loop...
  dst = ptr + k;
  src = s;
  ok = true;
  while (*src && ok) {
    c = *(src++);
    if (c == '\\') {
      c = *(src++);
      switch (c) {
        case '\0':      // unexpected end of string?
          ok = false;
          src--;        // go back to the terminating zero
          break;
        case 'x':       // hex escape?
          if ( (hex = HexValue (*(src++))) < 0) { ok = false; break; }
          c = (char) (hex << 4);
          if ( (hex = HexValue (*(src++))) < 0) { ok = false; break; }
          c |= (char) (hex);
          break;
        default:        // any short escape...
          p = strchr (escapeKeys, c);
          if (p) c = escapeVals[p-escapeKeys];
          else ok = false;
      }
    }
    *(dst++) = c;
  }

  // Done...
  if (ok) *dst = '\0';  // delimit string
  else ptr[k] = '\0';   // cut off all newly appended characters on error
  return ok;
}



// ***** Path handling *****


void CString::PathNormalize () {
  MakeWriteable ();
  ::PathNormalize (ptr);
}


void CString::PathRemoveTrailingSlashes () {
  MakeWriteable ();
  ::PathRemoveTrailingSlashes (ptr);
}


void CString::PathGo (const char *where) {
  if (where[0] == '/') Set (where);     // go absolute?
  else {
    Append ("/");                       // go relative...
    Append (where);
    PathNormalize ();
  }
}


void CString::PathGoUp () {
  PathGo ("..");
}



// ***** Using CString as a file buffer *****


bool CString::ReadFile (const char *relOrAbsPath) {
  CString s;
  int fd;

  Clear ();
  fd = open (EnvGetHome2lRootPath (&s, relOrAbsPath), O_RDONLY);
  if (fd < 0) return false;
  while (AppendFromFile (fd));
  close (fd);
  return true;
}


bool CString::AppendFromFile (int fd) {
  char buf[256];
  int n;

  // Read as much as possible into the read buffer...
  n = sizeof (buf) - 1;
  while (n >= (int) sizeof (buf) - 1) {
    //~ putchar ('-');
    n = read (fd, buf, sizeof (buf) - 1);  // returns 0 on EOF and <0 on error.
    if (n > 0) {         // successfully read something to append?
      buf[n] = '\0';
      Append (buf, n);
    }
    else if (n < 0 && errno != EAGAIN) {    // EAGAIN = would block on read: ok; everything else must not happen)
      WARNINGF (("'Error in 'read (fd = %i)': %s.", fd, strerror (errno)));
      //  close (fd);    // Do NOT close the channel, the caller must do it!
      n = 0;             // handle errors (other than 'EAGAIN') like EOF
    }
  }

  // Return 'false' if and only if the end of the stream is reached...
  return n != 0;
}


bool CString::ReadLine (CString *ret) {
  int n;

  // Try to return something...
  n = LFind ('\n');
  if (n < 0) return false;
  if (ret) ret->Set (ptr, n);
  Del (0, n + 1);
  //~ INFOF(("# ReadLine () -> '%s'", str->Get ()));
  return true;
}



// ***** Operators *****


CString CString::operator + (const char *str) {
  CString ret (*this);
  ret.Append (str);
  return ret;
}



// ***** Helpers (protected) *****


void CString::SetSize (int _size) {
  char *_ptr;

  if (_size > 0) {
    // new string is dynamically allocated...
    if (!size || _size > size) {
      _ptr = MALLOC(char, _size);
      strncpy (_ptr, ptr, _size);
      ASSERT (_size-1 >= 0);
      _ptr[_size-1] = '\0';
      if (size) free (ptr);
      ptr = _ptr;
      size = _size;
    }
  }
  else {
    // new string is not in heap
    if (size) {
      free (ptr);
      ptr = (char *) emptyStr;
      size = 0;
    }
  }
}



// ***** Regexp *****


const char *CRegex::ErrorStr () {
  char buf[256];

  regerror (lastError, &re, buf, sizeof (buf) - 1);
  errorStr.Set (buf);
  return errorStr.Get ();
}


bool CRegex::SetPattern (const char *pattern, int cflags) {
  reValid = ((lastError = regcomp (&re, pattern ? pattern : "a^", cflags)) == 0);
  //~ INFOF (("### CRegex: Compiling pattern '%s' -> OK = %i", pattern, (int) reValid));
  return reValid;
}


bool CRegex::Match (const char *s, int eflags, size_t maxMatches, regmatch_t *retMatchList) {
  //~ INFOF (("### CRegex: Matching against '%s'", s));
  return reValid && (lastError = regexec (&re, s, maxMatches, retMatchList, eflags)) == 0;
}




// ***************** Maps and key sets *********************


void CDictRaw::Copy (int idxDst, int idxSrc) {
  memcpy (GetKeyAdr (idxDst), GetKeyAdr (idxSrc), recSize);
}


void CDictRaw::Swap (int idx0, int idx1) {
#if DICT_ALIGNMENT == 4
  uint32_t *p0 = (uint32_t *) GetKeyAdr (idx0),
           *p1 = (uint32_t *) GetKeyAdr (idx1), tmp;
#else
#error "Unsupported value for DICT_ALIGNMENT"
#endif
  int bytesLeft = recSize;
  while (bytesLeft > 0) {
    tmp = *p0;
    *(p0++) = *p1;
    *(p1++) = tmp;
    bytesLeft -= DICT_ALIGNMENT;
  }
}


void CDictRaw::RecInit (int idx) {
  CString emptyString;
  uint8_t *p = GetKeyAdr (idx);
  memcpy (p, &emptyString, sizeof (emptyString));
  ValueInit (p + DICT_KEYSIZE);
}


void CDictRaw::RecClear (int idx) {
  uint8_t *p = GetKeyAdr (idx);
  ((CString *) p)->Clear ();
  ValueClear (p + DICT_KEYSIZE);
}


void CDictRaw::SetEntries (int _entries) {
  int n;

  // Realloc array if necessary...
  if (_entries >= allocEntries) {
    allocEntries = (_entries + 16);
    data = (uint8_t *) realloc (data, allocEntries * recSize);
  }

  // Clear entries that will be missing and init new entries...
  for (n = _entries; n < entries; n++) RecClear (n);
  for (n = entries; n < _entries; n++) RecInit (n);

  // Complete...
  if (_entries <= 0) {
    FREEP (data);    // free array on '_entries == 0'; important since used in destructor!
    allocEntries = 0;
  }
  entries = _entries;
}


const char *CDictRaw::ValueToStr (void *p) {
  CString *hexLine = GetTTS ();
  char buf[4];
  uint8_t *bp;
  int n;

  hexLine->Clear ();
  bp = (uint8_t *) p;
  for (n = 0; n < recSize - DICT_KEYSIZE; n++) {
    sprintf (buf, "%02x ", *bp);
    hexLine->Append (buf);
    bp++;
  }
  return hexLine->Get ();
}


void CDictRaw::SetRaw (int idx, void *value) {
  ValueSet (GetValueAdr (idx), value);
}


int CDictRaw::SetRaw (const char *key, void *value) {
  int idx, insIdx;

  idx = Find (key, &insIdx);
  if (idx < 0) {
    SetEntries (entries + 1);
    for (idx = entries - 1; idx > insIdx; idx--) Copy (idx, idx - 1);
    idx = insIdx;
    RecInit (idx);    // this was a duplicate of #'idx + 1' => decouple it
    ((CString *) GetKeyAdr (idx))->Set (key);
  }
  SetRaw (idx, value);
  return idx;
}


void CDictRaw::MergeRaw (CDictRaw *map2) {
  int i, i2, d, c;

  // Enlarge array and merge from back to front...
  i = entries - 1;
  i2 = map2->entries - 1;
  SetEntries (entries + map2->entries);
  d = entries - 1;
  while (i >= 0 && i2 >= 0) {
    c = strcmp (GetKey (i), map2->GetKey (i2));
    if (c > 0) {
      // Copy & consume entry from 'this'...
      if (d != i) Swap (d--, i--);    // 'Swap' is robust against construction/destruction
    }
    else { // c <= 0
      // Copy & consume entry from 'map2'...
      ((CString *) GetKeyAdr (d))->Set (map2->GetKey (i2));
      SetRaw (d--, map2->GetValueAdr (i2--));
      if (c == 0) i--;    // key exists in both maps: use value of 'map2' (considered newer) and consume both
    }
  }
  // Now either 'i' or 'i2' is 0, copy the remaining entries...
  while (i2 >= 0) {
    ((CString *) GetKeyAdr (d))->Set (map2->GetKey (i2));
    SetRaw (d--, map2->GetValueAdr (i2--));
  }
  if (d > i) {
    while (i >= 0) Swap (d--, i--);
    // Clear and delete superfluous entries in the beginning of the array (caused by duplicate keys)...
    d++;  // now points to first valid element and is equal to the number of elements to delete
    for (i = 0; i < d; i++) RecClear (i);
    for (i = 0; i < entries - d; i++) Copy (i, i + d);
    for (i = entries - d; i < entries; i++) RecInit (i);
    SetEntries (entries - d);
  }
}


void CDictRaw::Del (int idx) {
  int n;

  if (idx < 0) return;    // ignore non-existing index
  RecClear (idx);         // make record 'idx' overwritable
  for (n = idx + 1; n < entries; n++) Copy (n - 1, n);
  RecInit (entries - 1);  // this was a duplicate of #'entries-2' => decouple it
  SetEntries (entries - 1);
}


int CDictRaw::Find (const char *key, int *retInsIdx) {
  int n0, n1, idx, c;

  n0 = 0;
  n1 = entries - 1;
  while (n1 >= n0) {
    idx = (n0 + n1) / 2;
    c = strcmp (key, GetKey (idx));
    //~ INFOF(("### Compared '%s' with %i:'%s' -> %i", key, idx, GetKey (idx), c));
    if (c == 0) {
      if (retInsIdx) *retInsIdx = idx;
      return idx;
    }
    else if (c < 0) n1 = idx - 1;
    else n0 = idx + 1;
  }
  if (retInsIdx) *retInsIdx = n0;
  return -1;
}


void CDictRaw::PrefixSearch (const char *key, int *retIdx0, int *retIdx1) {
  int n0, n1, idx, idx0, idx1, c, keyLen;
  bool found;

  keyLen = strlen (key);

  // Find any matching key...
  n0 = idx = 0;
  n1 = entries - 1;
  found = false;
  while (!found && n1 >= n0) {
    idx = (n0 + n1) / 2;
    c = strncmp (key, GetKey (idx), keyLen);
    //~ INFOF(("### Compared '%s' with %i:'%s' -> %i", key, idx, GetKey (idx), c))
    if (c == 0) found = true;
    else if (c < 0) n1 = idx - 1;
    else n0 = idx + 1;
  }

  // Find first matching entry...
  idx0 = idx;
  while (idx0 > 0 && strncmp (key, GetKey (idx0 - 1), keyLen) == 0) idx0--;

  // Find first non-matching entry...
  idx1 = idx;
  while (idx1 < entries && strncmp (key, GetKey (idx1), keyLen) == 0) idx1++;

  // Return results...
  if (retIdx0) *retIdx0 = idx0;
  if (retIdx1) *retIdx1 = idx1;
}


void CDictRaw::Dump () {
  INFO ("CDictRaw::Dump ()...");
  for (int n = 0; n < entries; n++) INFOF (("%6i. %s = %s\n", n, GetKey (n), ValueToStr (GetValueAdr (n))));
}





// ********************** Date & Time **********************


//~ static inline TDate GetFirstOfMonth (TDate date) {
  //~ return (date & ~0x1f) + 1;
//~ }


TTicks TicksNow () {
  struct timespec ts;
  clock_gettime (CLOCK_REALTIME, &ts);
  return ((TTicks) ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}


TTicksMonotonic TicksMonotonicNow () {
  static int initSeconds = -1;
  struct timespec ts;

  clock_gettime (CLOCK_MONOTONIC, &ts);
  if (initSeconds < 0) initSeconds = ts.tv_sec;
  return ( (ts.tv_sec - initSeconds) * 1000) + ts.tv_nsec / 1000000;
}


TTicks TicksFromMonotic (TTicksMonotonic tm) {
  if (tm <= 0) return (TTicks) tm;     // '0' represents "as soon as possible", < 0 represents "relative from now in the future"
  return ((TTicks) (tm - TicksMonotonicNow ())) + TicksNow ();
}


TTicksMonotonic TicksToMonotonic (TTicks t) {
  //~ TTicks now = TicksNow ();
  //~ TTicksMonotonic monNow = TicksMonotonicNow ();
  //~ INFOF (("### TicksToMonotonic: ticksNow = %l, t = %l, monNow = %i", now, t, monNow));
  if (t <= 0) return (TTicksMonotonic) t;     // '0' represents "as soon as possible", < 0 represents "relative from now in the future"
  return ((TTicksMonotonic) (t - TicksNow ())) + TicksMonotonicNow ();
}


const char *TicksToString (CString *ret, TTicks ticks, int fracDigits, bool precise) {
  TDate d;
  TTime t;

  if (precise || TicksIsNever (ticks)) {
    ret->SetF ("t%lli", (long long) ticks);
  }
  else {
    TicksToDateTime (ticks, &d, &t);
    if (fracDigits == INT_MAX) {
      fracDigits = 3;
      if (ticks % 1000 == 0) {
        fracDigits = 0;
        if (ticks % 60000 == 0) fracDigits = -1;
      }
    }
    ret->SetF ("%04i-%02i-%02i-%02i%02i",
              YEAR_OF(d), MONTH_OF(d), DAY_OF(d),
              HOUR_OF(t), MINUTE_OF(t));
    if (fracDigits > -1) {
      ret->AppendF ("%02i", SECOND_OF(t));
      if (fracDigits > 0) {
        ret->AppendF (".%03i", ticks % 1000);
        if (fracDigits < 3) ((char *) ret->Get ()) [ret->Len () - 3 + fracDigits] = '\0';
      }
    }
  }
  return ret->Get ();
}


const char *TicksToString (TTicks ticks, int fracDigits, bool precise) {
  return TicksToString (GetTTS (), ticks, fracDigits, precise);
}


bool TicksFromString (const char *str, TTicks *ret, bool absolute) {
  int dy, dm, dd, th, tm, ts, millis;
  long long unsigned lluRet;
  const char *p;
  int n;

  if (str[0] == 't') {
    // Handle absolut time is given: return it ...
    if (sscanf (str + 1, "%llu", &lluRet) == 1) {
      *ret = (TTicks) lluRet;
      return true;
    }
    else return false;
  }
  else if (strchr (str, ':')) {
    // Handle day time...
    th = tm = ts = millis = 0;   // preset time
    n = sscanf (str, "%u:%02u:%02u.%03u", &th, &tm, &ts, &millis);
    if (n >= 2) {
      *ret = ( absolute ? DateTimeToTicks (Today (), TIME_OF (th, tm, ts))
                        : TICKS_FROM_SECONDS (TIME_OF (th, tm, ts)) )
             + millis;
      return true;
    }
    else return false;
  }
  else {
    // Handle full date or relative milliseconds...
    th = tm = ts = millis = 0;   // preset time
    n = sscanf (str, "%i-%u-%u-%02u%02u%02u.%03u", &dy, &dm, &dd, &th, &tm, &ts, &millis);
    //~ INFOF (("### TicksFromString ('%s') -> n = %i, dy = %i, dm = %i, dd = %i", str, n, dy, dm, dd));
    if (n >= 3) {
      // At least full date is given: return it...
      if (dy < 0) return false;   // negative numbers are only for relative times.
      *ret = DateTimeToTicks (DATE_OF (dy, dm, dd), TIME_OF (th, tm, ts)) + millis;
      return true;
    }
    else if (n == 1) {
      // A single integer is given: interpret it as milliseconds (or time in some other unit) from now ...
      p = str;
      while (*p >= '0' && *p <= '9') p++;
      switch (tolower (*p)) {
        case 's': millis = TICKS_FROM_SECONDS (dy); break;
        case 'm': millis = TICKS_FROM_SECONDS (60 * dy); break;
        case 'h': millis = TICKS_FROM_SECONDS (3600 * dy); break;
        case 'd': millis = TICKS_FROM_SECONDS (24 * 3600 * dy); break;
        case 'w': millis = TICKS_FROM_SECONDS (7 * 24 * 3600 * dy); break;
        default:  millis = dy;
      }
      *ret = absolute ? TicksNow () + millis : millis;
      return true;
    }
    else return false;
  }
}


void TicksMonotonicToStructTimeval (TTicksMonotonic t, struct timeval *retTv) {
  retTv->tv_sec = t / 1000;
  retTv->tv_usec = (t % 1000) * 1000;
}


TTicks TicksOfDate (int dy, int dm, int dd) {
  return DateTimeToTicks (DATE_OF (dy, dm, dd), 0);
}


TTicks TicksOfDate (TDate d) {
  return DateTimeToTicks (d, 0);
}


TTicks TicksOfTime (int th, int tm, int ts) {
  return 1000 * TIME_OF(th, tm, ts);
}


TTicks TicksOfTime (TTime t) {
  return 1000 * t;
}


TDate DateOfTicks (TTicks t) {
  TDate ret;
  TicksToDateTime (t, &ret, NULL);
  return ret;
}


TTime TimeOfTicks (TTicks t) {
  TTime ret;
  TicksToDateTime (t, NULL, &ret);
  return ret;
}


TTicks DateTimeToTicks (TDate d, TTime t, struct tm *retTm) {
  struct tm tm, *pTm;

  pTm = retTm ? retTm : &tm;

  pTm->tm_year = YEAR_OF(d) - 1900;    // Year - 1900
  pTm->tm_mon = MONTH_OF(d) - 1;
  pTm->tm_mday = DAY_OF(d);

  pTm->tm_hour = HOUR_OF(t);
  pTm->tm_min = MINUTE_OF(t);
  pTm->tm_sec = SECOND_OF(t);

  pTm->tm_isdst = -1;  // Daylight saving time; will be set

  return ((TTicks) mktime (pTm)) * 1000;
}


#if !ANDROID
TTicks DateTimeToTicksUTC (TDate d, TTime t, struct tm *retTm) {
  struct tm tm, *pTm;

  pTm = retTm ? retTm : &tm;

  pTm->tm_year = YEAR_OF(d) - 1900;    // Year - 1900
  pTm->tm_mon = MONTH_OF(d) - 1;
  pTm->tm_mday = DAY_OF(d);

  pTm->tm_hour = HOUR_OF(t);
  pTm->tm_min = MINUTE_OF(t);
  pTm->tm_sec = SECOND_OF(t);

  pTm->tm_isdst = -1;  // Daylight saving time; will be set

  return ((TTicks) timegm (pTm)) * 1000;
    // Note: 'timegm' is not POSIX-compliant. However, there is no better known way to do the task.
}
#endif


void TicksToDateTime (TTicks t, TDate *retDate, TTime *retTime, struct tm *retTm) {
  struct tm tm, *pTm;
  time_t posixTime;

  pTm = retTm ? retTm : &tm;
  posixTime = (time_t) (t / 1000);
  localtime_r (&posixTime, pTm);
  if (retDate) *retDate = DATE_OF(pTm->tm_year + 1900, pTm->tm_mon + 1, pTm->tm_mday);
  if (retTime) *retTime = TIME_OF(pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
}


void TicksToDateTimeUTC (TTicks t, TDate *retDate, TTime *retTime, struct tm *retTm) {
  struct tm tm, *pTm;
  time_t posixTime;

  pTm = retTm ? retTm : &tm;
  posixTime = (time_t) (t / 1000);
  gmtime_r (&posixTime, pTm);
  if (retDate) *retDate = DATE_OF(pTm->tm_year + 1900, pTm->tm_mon + 1, pTm->tm_mday);
  if (retTime) *retTime = TIME_OF(pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
}


TDate Today () {
  TDate ret;
  GetDateTimeNow (&ret, NULL);
  return ret;
}


TTicks TicksToday () {
  TDate d;
  TicksToDateTime (TicksNow (), &d, NULL, NULL);
  return TicksOfDate (d);
}


TDate DateIncByDays (TDate date, int dDays) {
  TTicks ticks = DateTimeToTicks (date, 0) + 86400000 * (TTicks) dDays + 43200000;
      // "+ 43200000" -> add half a day to avoid round-off errors (e.g. arround DST switches)
  TicksToDateTime (ticks, &date, NULL);
  return date;
}


int DateDiffByDays (TDate d1, TDate d0) {    // returns "d1" - "d0" in days
  return (DateTimeToTicks (d1, 0) - DateTimeToTicks (d0, 0) + 43200000) / 86400000;
      // "+ 43200000" -> add half a day to avoid round-off errors (e.g. arround DST switches)
}


TDate DateIncByMonths (TDate date, int dMon) {
  int m = 12 * YEAR_OF(date) + (MONTH_OF(date) - 1) + dMon;
  return DATE_OF(m / 12, (m % 12) + 1, DAY_OF(date));
}


int GetWeekDay (TDate date) {
  // return value: 0 (Monday) .. 6 (Sunday)
  struct tm tm;

  DateTimeToTicks (date, 0, &tm);
  return (tm.tm_wday + 6) % 7;
}


int GetCalWeek (TDate date) {
  struct tm tm;
  int yDay, week;

  DateTimeToTicks (date, 0, &tm);
  yDay = tm.tm_yday - (tm.tm_wday + 6) % 7;   // starting day of the week (monday)
  week = (yDay + 10) / 7;
    // Calculation according to ISO 8601 (formerly DIN 1355-1), see https://de.wikipedia.org/wiki/Woche
    //  4.1. [3] - -3.1.[-3] => KW 1
    // 11.1.[10] -  5.1.[4]  => KW 2
    // 18.1.[17] - 12.1.[11] => KW 3
  if (week <= 0) {   // in last week of previous year?
    // KNOWN BUG: The following determination of the number of previous year's calendar weeks is not always correct (assumes a leap year every 4 years without exceptions)
    if (yDay == 3) week = 53;   // last week ended with a thursday?
    else if (yDay == 4 && ((tm.tm_year-1) % 4 == 0)) week = 53;  // last year ended with a thursday and was a leap year?
    else week = 52;   // the most common case
  }
  return week;
}



// ***** Written month and weekday names *****

const char *monthNames[12] = { N_("January"), N_("February"), N_("March"), N_("April"), N_("May"), N_("June"), N_("July"), N_("August"), N_("September"), N_("October"), N_("November"), N_("December") };
const char *monthNamesShort[12] = { N_("Jan"), N_("Feb"), N_("Mar"), N_("Apr"), N_("May"), N_("Jun"), N_("Jul"), N_("Aug"), N_("Sep"), N_("Oct"), N_("Nov"), N_("Dec") };
const char *dayNames[7] = { N_("Monday"), N_("Tuesday"), N_("Wednesday"), N_("Thursday"), N_("Friday"), N_("Saturday"), N_("Sunday") };
const char *dayNamesShort[7] = { N_("Mon"), N_("Tue"), N_("Wed"), N_("Thu"), N_("Fri"), N_("Sat"), N_("Sun") };


const char *MonthName (int dm)      { return _(monthNames[dm-1]);       }
const char *MonthNameShort (int dm) { return _(monthNamesShort[dm-1]);  }
const char *DayName (int wd)        { return _(dayNames[wd]);           }
const char *DayNameShort (int wd)   { return _(dayNamesShort[wd]);      }





// ***************** Timer *********************************



// ***** Timer management *****


static CMutex timerMutex;
static CCond timerCond;
static volatile bool timerRunMainloop = true;


bool CTimer::ClassIterateAL () {
  CTimer *t;
  TTicksMonotonic curTicks;
  bool ret;

  ret = false;
  if (CTimer::first) {
    curTicks = TicksMonotonicNow ();
    while (CTimer::first && curTicks >= CTimer::first->nextTicks) {

      // Get next timer and remove first element...
      t = CTimer::first;
      //~ INFOF(("TimerIterate at %i: %08x at %i", curTicks, t, t->nextTicks));
      CTimer::first = t->next;   // remove first element

      // If repeated event: Re-insert the object for the next occasion...
      if (t->interval > 0) {
        if (!t->nextTicks) t->nextTicks = (curTicks - curTicks % t->interval);
        t->nextTicks += t->interval;
        if (curTicks > t->nextTicks)
          // we somehow lost multiple occurences => do not repeat them all, restart sensibly now
          t->nextTicks = (curTicks - curTicks % t->interval) + t->interval;
        t->InsertAL ();    // re-insert at appropriate position
      }

      // Now run the timer function...
      //   Important: This must be done after all list operations, since the timer function
      //   itself may reschedule/change this timer!
      timerMutex.Unlock ();   // mutex must be unlocked when 'func' is called!
      t->OnTime ();
      timerMutex.Lock ();

      // Delete obsolete internally managed timers...
      if (t->creator && t->interval == 0) delete t;  // we can do this, but only for internally managed objects!
      ret = true;
    }
  }
  return ret;
}


TTicksMonotonic CTimer::GetDelayTimeAL () {
  TTicksMonotonic curTicks;

  if (CTimer::first) {
    curTicks = TicksMonotonicNow ();
    if (curTicks >= CTimer::first->nextTicks) return 0;
    else return CTimer::first->nextTicks - curTicks;
  }
  else
    return INT_MAX;
}


bool TimerIterate () {
  bool ret;

  timerMutex.Lock ();
  ret = CTimer::ClassIterateAL ();
  timerMutex.Unlock ();
  return ret;
}


TTicksMonotonic TimerGetDelay () {
  TTicksMonotonic ret;

  //~ INFO ("### TimerGetDelay: Lock...");
  timerMutex.Lock ();
  ret = CTimer::GetDelayTimeAL ();
  //~ INFOF (("### TimerGetDelay: Unlock... delay = %i", (int) ret));
  timerMutex.Unlock ();
  return ret;
}


void TimerRun () {
  timerMutex.Lock ();
  while (timerRunMainloop) {
    //~ INFO ("TimerRun");
    CTimer::ClassIterateAL ();
    //~ INFO ("### TimerRun: timerCond.Wait...");
    timerCond.Wait (&timerMutex, CTimer::GetDelayTimeAL ());
    //~ INFOF (("### TimerRun: timerCond.Wait: done. Continue running = %i.", timerRunMainloop));
  }
  timerMutex.Unlock ();
}


static CThread *timerThread = NULL;


static void *TimerThreadRoutine (void *) {
  TimerRun ();
  //~ INFO("TimerThreadRoutine done");
  return NULL;
}


void TimerStart () {
  ASSERT (timerThread == NULL);
  timerRunMainloop = true;
  timerThread = new CThread ();
  timerThread->Start (TimerThreadRoutine);
}


void TimerStop () {
  //~ INFOF (("### TimerStop: timerThread = %08x, running = %i", timerThread, timerThread->IsRunning ()));
  timerMutex.Lock ();
  timerRunMainloop = false;
  timerCond.Signal ();
  timerMutex.Unlock ();
  if (timerThread) {
    timerThread->Join ();
    delete timerThread;
    timerThread = NULL;
  }
}





// ***** class 'CTimer' *****


CTimer *CTimer::first = NULL;


CTimer::CTimer () {
  next = NULL;
  isLinked = false;
  nextTicks = interval = 0;
  creator = NULL;
  func = NULL;
  data = NULL;
}


void CTimer::Set (FTimerCallback *_func, void *_data, void *_creator) {
  func = _func;
  data = _data;
  creator = _creator;
}


void CTimer::Set (TTicksMonotonic _time, TTicksMonotonic _interval, FTimerCallback *_func, void *_data, void *_creator) {
  //~ INFOF (("### CTimer::Set (_time = %i, _interval = %i), now = %i", _time, interval, TicksMonotonicNow ()));
  func = _func;
  data = _data;
  creator = _creator;
  Reschedule (_time, _interval);
}


void CTimer::Reschedule (TTicksMonotonic _time, TTicksMonotonic _interval) {
  timerMutex.Lock ();

  nextTicks = _time >= 0 ? _time : TicksMonotonicNow () - _time;
  interval = _interval;

  // Align 'nextTicks', if '_interval' is given and is a power of 2...
  if (_interval > 0 && ((_interval & (_interval - 1)) == 0))
    nextTicks = nextTicks & (~(_interval - 1));

  InsertAL ();

  timerCond.Signal ();    // wake up main loop to check if the new timer is newer than anything else
  timerMutex.Unlock ();
}


void CTimer::Clear () {
  timerMutex.Lock ();
  UnlinkAL ();
  timerMutex.Unlock ();
}


void CTimer::DelByCreator (void *_creator) {
  CTimer **pCur = &first, *victim;

  timerMutex.Lock ();
  while (*pCur) {
    if ((*pCur)->creator == _creator) {
      victim = *pCur;
      *pCur = (*pCur)->next;
      delete victim;
    }
    else pCur = &((*pCur)->next);
  }
  timerMutex.Unlock ();
}


void CTimer::InsertAL () {
  CTimer **pCur = &first;

  if (isLinked) UnlinkAL ();
  while (*pCur && (*pCur)->nextTicks <= nextTicks) pCur = &((*pCur)->next);
  isLinked = true;
  next = *pCur;
  (*pCur) = this;
}


void CTimer::UnlinkAL () {
  CTimer **pCur = &first;

  if (isLinked) {
    while (*pCur && (*pCur != this)) pCur = &((*pCur)->next);
    if (*pCur) *pCur = next;
    isLinked = false;    // mark as not pending (= unlinked)
  }
}





// ***************** Threading *****************************


// ***** CThread *****


void *CThreadRoutine (void *_data) {
  CThread *thread = (CThread *) _data;
  return thread->Run ();
}


void CThread::Start (FThreadRoutine *_routine, void *_data) {
  if (pthread_create (&thread, NULL, _routine, _data) != 0) ERROR("'pthread_create' failed.");
  running = true;
}


void CThread::Start () {
  Start (CThreadRoutine, this);
}


void *CThread::Join () {
  void *ret = NULL;
  if (!running) ERROR("'CThread::Join' called for non-running thread.");
  if (pthread_join (thread, &ret) != 0) ERROR("'pthread_join' failed.");
  running = false;
  return ret;
}





// ***** CMutex *****


CMutex::CMutex () {
  pthread_mutex_init (&mutex, NULL);
}


CMutex::~CMutex () {
   pthread_mutex_destroy(&mutex);
}


void CMutex::Lock () {
  ASSERT (pthread_mutex_lock (&mutex) == 0);
}


bool CMutex::TryLock () {
  return pthread_mutex_trylock (&mutex) == 0;
}


void CMutex::Unlock () {
  ASSERT (pthread_mutex_unlock (&mutex) == 0);
}





// ***** CCond *****

CCond::CCond () {
  pthread_cond_init (&cond, NULL);
}


CCond::~CCond () {
  pthread_cond_destroy (&cond);
}


void CCond::Wait (CMutex *mutex) {
  //~ INFOF(("### CCond::Wait ()..."));
  pthread_cond_wait (&cond, &mutex->mutex);
  //~ INFOF(("### CCond::Wait (): wakeup"));
}


int CCond::Wait (CMutex *mutex, TTicksMonotonic maxTime) {
  struct timespec absTime, now;
  int64_t longTime;
  TTicksMonotonic timeLeft;
  int errNo;

  clock_gettime (CLOCK_REALTIME, &absTime);
  //~ INFOF(("### CCond::Wait (maxTime): absTime = %lli/%lli, maxTime = %lli", (int64_t) absTime.tv_sec, (int64_t) absTime.tv_nsec, (int64_t) maxTime));
  longTime = ((int64_t) maxTime) * 1000000 + (int64_t) absTime.tv_nsec;
  absTime.tv_sec += (longTime / 1000000000);
  absTime.tv_nsec = longTime % 1000000000;
  //~ INFOF(("### absTime = %lli/%lli, longTime = %lli", (int64_t) absTime.tv_sec, (int64_t) absTime.tv_nsec, (int64_t) longTime));

  errNo = pthread_cond_timedwait (&cond, &mutex->mutex, &absTime);
  if (errNo == 0) {
    // woken up by signal or spurious wakeup
    clock_gettime (CLOCK_REALTIME, &now);
    timeLeft = (absTime.tv_sec - now.tv_sec) * 1000 + (absTime.tv_nsec - now.tv_nsec) / 1000000;
    //~ INFOF(("### CCond::Wait (maxTime): wakeup"));
    return MAX(0, timeLeft);
  }
  //~ INFOF(("### CCond::Wait (maxTime): timeout"));
  if (errNo == ETIMEDOUT) return -1;
  ERRORF(("'CCond::Wait (maxTime = %i)' -> 'pthread_cond_timedwait': %s", maxTime, strerror (errNo)));
}


void CCond::Signal () {
  pthread_cond_signal (&cond);
}


void CCond::Broadcast () {
  pthread_cond_broadcast (&cond);
}





// ***** CSleeper *****


static void CbSleeperTimer (CTimer *timer, void *data) {
  CSleeper *sleeper = (CSleeper *) timer->GetCreator ();
  sleeper->PutCmd (data);
}


CSleeper::CSleeper () {
  selfPipe[0] = selfPipe[1] = -1;
  cmdRecSize = 0;
  Clear ();
}


void CSleeper::Done () {
  CTimer::DelByCreator (this);
  if (selfPipe[0] >= 0) {
    close (selfPipe[0]);
    close (selfPipe[1]);
    selfPipe[0] = selfPipe[1] = -1;
  }
}


void CSleeper::EnableCmds (int _cmdRecSize) {
  ASSERT (pipe (selfPipe) == 0);
  cmdRecSize = _cmdRecSize;
}


void CSleeper::Clear () {
  FD_ZERO (&fdSetRead);
  FD_ZERO (&fdSetWrite);
  maxFd = -1;
  if (selfPipe[0] >= 0) {
    FD_SET (selfPipe[0], &fdSetRead);
    maxFd = selfPipe[0];
  }
}


void CSleeper::AddReadable (int fd) {
  if (fd >= 0) {
    FD_SET (fd, &fdSetRead);
    if (fd > maxFd) maxFd = fd;
  }
}


void CSleeper::AddWritable (int fd) {
  if (fd >= 0) {
    FD_SET (fd, &fdSetWrite);
    if (fd > maxFd) maxFd = fd;
  }
}


void CSleeper::Sleep (TTicksMonotonic maxTime) {
  struct timeval tv;

  //~ INFOF (("### CSleeper<%08x>::Sleep (maxTime = %i)...", this, (int) maxTime));
  if (maxTime) TicksMonotonicToStructTimeval (maxTime, &tv);
  ASSERT (maxFd >= 0);
  if (select (maxFd + 1, &fdSetRead, &fdSetWrite, NULL, maxTime >= 0 ? &tv : NULL) < 0) {
    if (errno != EINTR) ERRORF (("select() returned with error: %s", strerror (errno)));
      // 'EINTR' ("signal caught") is an acceptable error (see select(2)), but the only one.
  }
  //~ INFO ("### ... done - awake again.");
}


bool CSleeper::IsReadable (int fd) {
  return fd >= 0 ? (FD_ISSET (fd, &fdSetRead) != 0) : false;
}


bool CSleeper::IsWritable (int fd) {
  return fd >= 0 ? (FD_ISSET (fd, &fdSetWrite) != 0) : false;
}


bool CSleeper::GetCmd (void *retCmdRec) {
  if (selfPipe[0] < 0) return false;
  if (!IsReadable (selfPipe[0])) return false;
  ASSERT (read (selfPipe[0], retCmdRec, cmdRecSize) == cmdRecSize);
  return true;
}


void CSleeper::PutCmd (const void *cmdRec, TTicksMonotonic t, TTicksMonotonic _interval) {
  //~ INFOF (("### CSleeper<%08x>::PutCmd... selfPipe = %i/%i", this, selfPipe[0], selfPipe[1]));
  ASSERT (selfPipe[1] >= 0 && cmdRecSize > 0);   // commands must have been enabled
  if (t || _interval)
    new CTimer (t, _interval, CbSleeperTimer, this, this);
  else {
    //~ INFO ("###   write...");
    ASSERT (write (selfPipe[1], cmdRec, cmdRecSize) == cmdRecSize);
    //~ INFO ("###   ... done.");
  }
}





// ***** Misc. *****


void Sleep (TTicksMonotonic mSecs) {
  struct timespec req;

  req.tv_sec = mSecs / 1000;
  req.tv_nsec = (mSecs % 1000) * 1000000;
  nanosleep (&req, NULL);
}





// ********************** CShell ***************************


ENV_PARA_SPECIAL ("sys.cmd.<name>", const char *, NULL)
  /* Predefine a shell command
   *
   * For security reasons, shell commands executed remotely are never transferred over the network
   * and then executed directly. Instead, a server can only execute commands predefined on the server
   * side. This group of settings serves for pre-defining commands executed by a restricted shell.
   */


bool CShell::StartRestricted (const char *name, const char *args) {
  CString s;

  const char *cmd = EnvGet (StringF (&s, "sys.cmd.%s", name));
  if (!cmd) {
    WARNINGF (("Undefined command alias for '%s'", name));
    return false;
  }
  return Start (StringF (&s, cmd, args));
}


int CShell::Run (const char *cmd, const char *input, CString *output) {
  CString outLine;
  bool canRead;

  Start (cmd);
  if (input) WriteLine (input);
  WriteClose ();
  if (output) output->Clear ();
  while (!ReadClosed ()) {
    CheckIO (NULL, &canRead);
    if (canRead) if (ReadLine (&outLine)) if (output) {
      output->Append (outLine);
      output->Append ('\n');
    }
  }
  Wait ();
  return ExitCode ();
}





// ********************** CShellBare ***********************


void CShellBare::Done () {
  CShellBare::WriteClose ();
  CShellBare::Kill ();
  CShellBare::Wait ();
}


bool CShellBare::Start (const char *cmd, bool readStdErr) {
  int pipeToScript[2], pipeFromScript[2];
  CString s1, s2, sCmd;

  DEBUGF (1, (host ? "Starting shell command on host '%2$s': '%1$s' ..." :  "Starting shell command locally: '%s' ...", cmd ? cmd : host ? "<ssh>" : "<bash>", host));

  // Preparation ...
  Wait ();
  exitCode = killSig = -1;
  readBuf.Clear ();
  readBufMayContainLine = false;

  // Create pipes for communication...
  ASSERT (pipe (pipeToScript) == 0);
  ASSERT (pipe2 (pipeFromScript, O_NONBLOCK) == 0);
  //~ signal (SIGPIPE, SIG_IGN);

  // Store local (parent's) ends of pipes...
  fdToScript = pipeToScript[1];        // stdin to script
  fdFromScript = pipeFromScript[0];

  // Fork & start child...
  childPid = fork ();
  ASSERT (childPid >= 0);
  if (childPid == 0) {

    // I am the child: remap std i/o to the pipes...
    dup2 (pipeToScript[0], STDIN_FILENO);
    close (pipeToScript[1]);
    dup2 (pipeFromScript[1], STDOUT_FILENO);
    if (readStdErr) dup2 (pipeFromScript[1], STDERR_FILENO);
    close (pipeFromScript[0]);

    // Create a new process group and become its leader ...
    // ... so that eventually started sub-processes get killed (hung up) in 'Kill ()'
    if (newProcessGroup) setpgid (0, 0);

    // Execute script...
    if (host) if (host[0] == '\0') host = NULL;     // Normalize 'host'
    if (ANDROID && !host && cmd && cmd[0] != '/') {        // Local command with relativ path: prepend HOME2L_ROOT ...
      sCmd.SetF ("%s/%s", EnvHome2lRoot (), cmd);
      cmd = sCmd.Get ();
    }
#if ANDROID
    if (!host) {
      //~ INFOF (("### CShellBare: Starting '%s' ...", cmd));
      execl ("/system/bin/sh", "/system/bin/sh",
             cmd ? "-c" : NULL, cmd, NULL);
    }
    else {
      execl ("/system/bin/ssh", "/system/bin/ssh",
             "-i", StringF (&s1, "%s/etc/secrets/ssh/%s", EnvHome2lRoot (), EnvMachineName ()),             // identity
             "-o", StringF (&s2, "UserKnownHostsFile=%s/etc/secrets/ssh/known_hosts", EnvHome2lRoot ()),    // known hosts
             "-o", "NoHostAuthenticationForLocalhost=yes",
             "-o", "LogLevel=QUIET",
             "-l", "home2l", host,              // remote user and host
             cmd ? cmd : "/bin/bash", NULL);    // command or shell
        /*
         * Example: Pre-test a connection from 'inf629' to '192.168.2.11'
         *
         * root@espressowifi:/ # ssh -i /data/data/org.home2l.app/files/home2l/etc/secrets/ssh/inf629 \
         *                           -o UserKnownHostsFile=/data/data/org.home2l.app/files/home2l/etc/secrets/ssh/known_hosts \
         *                           home2l@192.168.2.11
         */
    }
#else
    if (!host)
      execl ("/bin/bash", "/bin/bash",
             cmd ? "-c" : NULL, cmd, NULL);
    else
      execl ("/usr/bin/ssh", "/usr/bin/ssh",
             "-l", "home2l", host,              // remote user and host
             cmd ? cmd : "/bin/bash", NULL);    // command or shell
#endif
    ERRORF (("Failed to start '%s': %$s", cmd, strerror (errno)));
      // we should never get here
  }
  else {

    // I am the parent: close unused pipe ends...
    close (pipeToScript[0]);
    close (pipeFromScript[1]);

    // Set the new process group PID to the child...
    //   The child does the same thing, too. Duplication of this operation is required to
    //   avoid race conditions: All following operations in both processes may assert that
    //   the migration has already happened. For details, see Chapter "28.6.3 Launching Jobs"
    //   in "The GNU C Library Reference Manual, for Version 2.24 of the GNU C Library".
    if (newProcessGroup) setpgid (childPid, childPid);
  }
  //~ INFOF (("### CShellBare::Start ('%s'): SUCCESS!", cmd));
  return true;
}


bool CShellBare::DoWaitPid (int options) {
  int ret, status;

  ASSERT (childPid > 0);
  ret = waitpid (childPid, &status, options);
  if (ret < 0) {
    WARNINGF (("'waitpid' failed: %s - Killing child process %i", strerror (errno), childPid));
    ASSERT (!ANDROID || errno == ECHILD);
      /* Note (2018-06-04):
       *    Normally, we should never get here unless there is a bug in the 'CShellBare' implementation.
       *    However, under Android it has been observed that 'waitpid ()' may suddenly report ECHILD
       *    ("No child processes") even if previous calls to waitpid () on the same child never reported
       *    an exit of the child in any way (see following cases). This seems to happen if the app
       *    is going to the background/foreground.
       *
       *    For this reason, we consider the exit of 'waitpid (childPid, ...)' with an error as
       *    another sign that the child has exited.
       */
    Kill (SIGABRT);     // abort child (if it still happens to be there)
    childPid = -1;
  }
  else if (ret != 0) {
    if (WIFEXITED(status)) {
      exitCode = WEXITSTATUS (status);
      //~ INFOF (("### Child %i has exited with code %i.", childPid, exitCode));
      childPid = -1;
    }
    else if (WIFSIGNALED (status)) {
      if (WTERMSIG(status) != killSig)
        WARNINGF(("Child terminated with signal %i ('%s')", WTERMSIG(status), strsignal (WTERMSIG(status))));
      childPid = -1;
    }
    else ASSERT (false);
  }

  //~ INFOF (("### DoWaitPid () -> childPid = %i", childPid));
  return (ret == 0);
}


bool CShellBare::IsRunning () {
  //~ INFOF (("### CShellBare::IsRunning (childPID = %i) ... ", childPid));
  if (childPid < 0) return false;
  return DoWaitPid (WNOHANG);
}


void CShellBare::Wait () {
  //~ INFOF (("### CShellBare::Wait (childPID = %i) ... ", childPid));
  while (childPid > 0) DoWaitPid (0);
  WriteClose ();
  if (fdFromScript > -1) {
    close (fdFromScript);
    fdFromScript = -1;
  }
}


void CShellBare::Kill (int sig) {
  killSig = sig;
  if (childPid > 0) kill (newProcessGroup ? -childPid : childPid, sig);
}


void CShellBare::CheckIO (bool *canWrite, bool *canRead, TTicksMonotonic maxTime) {
  fd_set rfds, wfds;
  struct timeval tv;
  bool waitOnWrite, waitOnRead;

  waitOnWrite = (canWrite && fdToScript > 0);
  waitOnRead = (canRead && fdFromScript > 0 && !readBufMayContainLine);

  TicksMonotonicToStructTimeval (maxTime, &tv);
  FD_ZERO (&wfds);
  if (waitOnWrite) FD_SET (fdToScript, &wfds);
  FD_ZERO (&rfds);
  if (waitOnRead) FD_SET (fdFromScript, &rfds);

  if (waitOnWrite || waitOnRead) {
    //~ INFOF (("### CShellBare::CheckIO(): select start, waitOnWrite = %i, waitOnRead = %i, fdToScript = %i, fdFromScript = %i", (int) waitOnWrite, (int) waitOnRead, fdToScript, fdFromScript));
    select (MAX (fdToScript, fdFromScript) + 1, &rfds, &wfds, NULL, maxTime >= 0 ? &tv : NULL);
    //~ INFO ("### CShellBare::CheckIO(): select done");
  }

  if (canWrite) *canWrite = (fdToScript > 0 ? (FD_ISSET (fdToScript, &wfds) != 0) : false);
  if (canRead) *canRead = (readBufMayContainLine ? true : fdFromScript > 0 ? (FD_ISSET (fdFromScript, &rfds) != 0) : false);
}


void CShellBare::WriteLine (const char *line) {
  int bytesLeft, ret;

  //~ INFOF(("# WriteLine ('%s')", line));
  if (fdToScript < 0) return;
  bytesLeft = strlen (line) + 1;
  while (bytesLeft > 0) {
    //~ INFOF(("#   ... '%s'", line));
    if (bytesLeft > 1) {
      ret = write (fdToScript, line, bytesLeft-1);
      line += ret;
    }
    else ret = write (fdToScript, "\n", 1);
    if (ret <= 0) {
      if (!ret) WARNING ("'write()' returned 0: strange, closing channel");
      else WARNINGF (("'Error in 'write()': %s. Closing channel.", strerror (errno)));
      WriteClose ();
      return;
    }
    bytesLeft -= ret;
  }
}


void CShellBare::WriteClose () {
  if (fdToScript > 0) {
    //~ write (fdToScript, "\04", 1);     // write Ctrl-D
    close (fdToScript);
    fdToScript = -1;
  }
}


bool CShellBare::ReadLine (CString *str) {

  // Read as much as possible into the read buffer...
  if (fdFromScript > 0) {
    if (!readBuf.AppendFromFile (fdFromScript)) {
      close (fdFromScript);       // EOF received...
      //~ INFOF (("### CShellBare::ReadLine (fd = %i) = EOF", fdFromScript));
      fdFromScript = -1;
    }
    else readBufMayContainLine = true;
  }

  // Try to return something...
  if (readBuf.ReadLine (str)) {
    //~ INFOF(("# ReadLine () -> '%s'", str->Get ()));
    return true;
  }
  readBufMayContainLine = false;
  return false;
}





// ********************** CShellSession *******************


static const char shellMagicString[] = "---8-----=HOME2L=---=MAGIC=-----8---";


void CShellSession::Done () {
  WriteClose ();
  Wait ();
  session.WriteLine ("exit");
  session.Done ();
}


bool CShellSession::Start (const char *cmd, bool readStdErr) {
  CString line;

  Wait ();    // just in case a previous command is still open
  if (!session.IsRunning ()) if (!session.StartSession (readStdErr)) return false;

  DEBUGF (1, ("Starting shell command in session: '%s' ...", cmd));

  // Submit command...
  //~ line.SetF ("%s << %s; export PS=\"%s $?\"", cmd, shellMagicString, shellMagicString);
  line.SetF ("%s << %s", cmd, shellMagicString);
  WriteLine (line);
  writeOpen = readOpen = true;
  return true;
}


void CShellSession::Wait () {
  bool canRead;

  while (readOpen) {
    CheckIO (&canRead, NULL);
    ReadLine (NULL);
  }
  WriteClose ();
}


void CShellSession::CheckIO (bool *canWrite, bool *canRead, TTicksMonotonic timeOut) {

  // Update 'readOpen'...
  if (canRead) if (session.ReadClosed ()) readOpen = false;

  // Delegate to the session shell...
  session.CheckIO (canWrite, canRead, timeOut);
}


void CShellSession::WriteClose () {
  CString line;

  if (writeOpen) {
    WriteLine (shellMagicString);
    line.SetF ("echo %s $?", shellMagicString);
    WriteLine (line);     // mark end of input
    //~ WriteLine ("\04");
    writeOpen = false;
  }
}


bool CShellSession::ReadLine (CString *str) {
  char *p, *magic;
  CString altStr, *_str;

  if (session.ReadClosed ()) readOpen = false;
  if (!readOpen) return false;
  _str = str ? str : &altStr;
  if (!session.ReadLine (_str)) return false;
  magic = (char *) strstr (_str->Get (), shellMagicString);
  if (magic) {
    p = magic + sizeof (shellMagicString);
    sscanf (p, "%d", &exitCode);
    readOpen = false;
    if (magic > _str->Get ()) {
      // Line contains some user data in the beginning: return that...
      magic[0] = '\0';
      return true;
    }
    return false;
  }
  return true;
}
