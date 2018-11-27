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


#ifndef _BASE_
#define _BASE_


/** @file
 * @defgroup base Base
 * Basic definitions and classes for all *Home2L* components.
 *
 * @{
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <unistd.h>
#include <regex.h>

#include <config.H>





// ***************** Configuration *********************************************

#ifdef __ANDROID__
#define ANDROID 1   ///< 1, if compiling for Android (NDK), 0 otherwise
#else
#define ANDROID 0
#endif





// ***************** Basic definitions *****************************************


/// @defgroup base_basics Basics
/// Some very basic definitions
/// @{


#define MIN(A, B) ((A) < (B) ? (A) : (B))   ///< Typeless min-operator
#define MAX(A, B) ((A) > (B) ? (A) : (B))   ///< Typeless max-operator

#define STR(X) #X
  ///< Stringify macro

#define OFFSETOF(type, field) ((unsigned) &(((type *) 0)->field))
  ///< Byte offset of a field in struct or class

#define CLEAR(X) bzero (&(X), sizeof (X))
  ///< Set variable/object 'X' to all-zero


/// @name Atomic read and write operations ...
///   (presently GCC-specific)
/// @{
#define ATOMIC_READ(PTR) __sync_fetch_and_add (&(PTR), 0)
#define ATOMIC_WRITE(PTR, VAL) __sync_bool_compare_and_swap (&(PTR), PTR, VAL)
#define ATOMIC_INC(PTR, N) __sync_fetch_and_add ((&PTR), (N))
/// @}


/// @}  // Basics



// ***************** Logging and Debugging *************************************

/** @defgroup base_logging Logging
 * Macros for logging.
 *
 * @{
 */


#if WITH_DEBUG == 1
extern int envDebug;            ///< Debug level (read-only; may also be mapped to a constant macro).
#else
#define envDebug 0
#endif


#if !ANDROID
void LogToSyslog ();      // Redirect logging to syslog from now (under non-Android)
void LogClose ();
bool LoggingToSyslog ();
void LogStack ();         // Log a stack trace (requires linker flag '-rdynamic' to print function names)
#else
static inline void LogToSyslog () {}
static inline void LogClose () {}
static inline bool LoggingToSyslog () { return true; }
static inline void LogStack () {}
#endif

void LogPara (const char *_logHead, const char* _logFile, int _logLine);    // Helper only; use the following macros instead
void LogPrintf (const char *format, ...);     // Helper only; use the following macros instead


#if WITH_DEBUG == 1
#define DEBUG(LEVEL, MSG) do { if (envDebug >= LEVEL) { LogPara ("DEBUG", __FILE__, __LINE__); LogPrintf (MSG); } } while (0)
  ///< Print a debug message.
  /// This is only effective if the tool was compiled with WITH_DEBUG == 1 and the
  /// configuration parameter 'debug' is set to a value > 0.
  ///
  /// Conventions for the debug level (examples):
  /// - 1: Everything which does not flood the log files
  /// - 2: GPIO value changes; Temporary outputs
  /// - 3: Details: Network conversation (Resources), debug output of underlying libs (ALSA, GStreamer, PJSIP, Linphone)
  ///
  /// Messages of level 3 or more must have a topic-related prefix "[<topic>]" to allow later filtering,
#define DEBUGF(LEVEL, FMT) do { if (envDebug >= LEVEL) { LogPara ("DEBUG", __FILE__, __LINE__); LogPrintf FMT; } } while (0)
  ///< Print a formatted debug message.
  /// The argument must contain a parantheses with arguments, which will then be
  /// passed to a printf() function variant. For example:
  /// `DEBUGF(("Current value is %i", my_value))`
#else
#define DEBUG(LEVEL, MSG) do {} while (0)
#define DEBUGF(LEVEL, FMT) do {} while (0)
#endif

#define INFO(MSG) do { LogPara ("INFO", __FILE__, __LINE__); LogPrintf (MSG); } while (0)
  ///< Print a debug message to the console.
#define INFOF(FMT) do { LogPara ("INFO", __FILE__, __LINE__); LogPrintf FMT; } while (0)
  ///< Print a formatted info message.

#define WARNING(MSG) do { LogPara ("WARNING", __FILE__, __LINE__); LogPrintf (MSG); } while (0)
  ///< Print a warning message to the console.
#define WARNINGF(FMT) do { LogPara ("WARNING", __FILE__, __LINE__); LogPrintf FMT; } while (0)
  ///< Print a formatted warning message.

#define SECURITY(MSG) do { LogPara ("SECURITY", __FILE__, __LINE__); LogPrintf (MSG); } while (0)
  ///< Print a security-related warning to the console.
  /// These are warnings an adminstrator should regularly check out for, since
  /// they may be caused by an attack.
#define SECURITYF(FMT) do { LogPara ("SECURITY", __FILE__, __LINE__); LogPrintf FMT; } while (0)
  ///< Print a formatted security-related warning.

#define ERROR(MSG) do { LogPara ("ERROR", __FILE__, __LINE__); LogPrintf (MSG); _exit (3); } while (0)
  ///< Print an error message to the console and quit the application.
#define ERRORF(FMT) do { LogPara ("ERROR", __FILE__, __LINE__); LogPrintf FMT; _exit (3); } while (0)
  ///< Print a formatted error message to the console and quit the application.

#define ABORT(MSG) do { LogPara ("ERROR", __FILE__, __LINE__); LogPrintf (MSG); abort (); } while (0)
  ///< Print an error message to the console and abort the application.
  /// Unlike `ERROR`, this uses the `abort()` system call, which may generate
  /// a core dump to be used for debugging.
#define ABORTF(FMT) do { LogPara ("ERROR", __FILE__, __LINE__); LogPrintf FMT; abort (); } while (0)
  ///< Print a formatted error message to the console and abort the application.

#define ASSERT(COND) do { if (!(COND)) { LogPara ("ERROR", __FILE__, __LINE__); LogPrintf ("Assertion failed"); LogStack (); abort (); } } while (0)
  ///< Check a condition and abort the application if not true.
#define ASSERTM(COND,MSG) do { if (!(COND)) { LogPara ("ERROR", __FILE__, __LINE__); LogPrintf ("Assertion failed: %s", MSG); LogStack (); abort (); } } while (0)
  ///< Check a condition and, if not true, abort the application with a message.

#define ASSERT_WARN(COND) do { if (!(COND)) WARNING("Weak assertion failed"); } while (0)
  ///< Check a condition and emit a warning if not true.




// ***** Compile-time assertion *****


#define BUILD_ASSERT(NAME, COND) static inline void build_assert_##NAME() { (void) sizeof(char[(COND) - 1]); }
  ///< Check a compile-time condition.
  /// This macro fails to compile with error "negative array size", if the asserted
  /// condition is not true. `NAME` is an arbitrary identifier that must be unique
  /// in the application.


/// @}  // Logging





// *************************** Safe heap operations ****************************


/** @defgroup base_heap Heap
 * Type-safe and null-pointer-aware heap operations.
 *
 * @{
 */


/// @name C-style Allocation ...
/// @{
#define MALLOC(T, N) (T*) malloc (sizeof (T) * (N))
  ///< Allocate an array of `N` objects of type `T` (type-safe)
#define REALLOC(T, P, N) (T*) realloc (P, sizeof (T) * (N))
  ///< Reallocate an array of `N` objects of type `T` (type-safe)
#define SETP(P, X) { if (P) free (P); P = (X); }
  ///< Replace `P` with a new object/array, eventually free'ing the old one.
#define FREEP(P) SETP(P, NULL)
  ///< Free object/array `P`, but only if `P != NULL`
/// @}


/// @name C++-style Allocation ...
/// @{
#define SETO(O, X) { if (O) delete O; O = (X); }
  ///< Replace `O` with a new object, eventually deleting the old one.
#define FREEO(O) SETO(O, NULL)
  ///< Free object `O`, but only if `O != NULL`

#define SETA(A, X) { if (A) delete [] A; A = (X); }
  ///< Replace `A` with a new array, eventually deleting the old one.
#define FREEA(A) SETA(A, NULL)
  ///< Free array `A`, but only if `A != NULL`
/// @}


/// @}  // Heap



// ***************** Localization and language *********************************

/** @defgroup base_language Language
 * Localization and translation
 *
 * Localization and translation is typically supported in GUI programs
 * of the Home2L suite only. Command-line tools should always revert to the locale "C".
 *
 * @{
 */


// Select the 'gettext' implementation (GNU vs. internal)...
#if ANDROID
#define USE_GNU_GETTEXT 0   // Android does not have GNU gettext => use internal implementation
#else
#define USE_GNU_GETTEXT 0   // can be set to 1, presently set to 0 for debugging purposes
#endif


// Basic definitions...
#if USE_GNU_GETTEXT   // GNU gettext implementation ...

#include <libintl.h>

#else                 // Internal implementation ...

const char *LangGetText (const char *msgId);

#define gettext(STR) LangGetText (STR)

#endif


// Init/done...
void LangInit (const char *localeDir, const char *locale);
  // locale = "" or NULL: use environment setting LC_MESSAGES
  // 'localeDir' is expected to be '$HOME2L_ROOT/locale'. The domain will be automatically set to "home2l",
  // so that the translations for the complete Home2L suite are contained in a single file:
  // '$HOME2L_ROOT/locale/<locale>/LC_MESSAGES/home2l.mo'
void LangDone ();


// Marking macros...
#define _(STR) gettext(STR)       ///< Use this GNU gettext-compliant macro for translated strings
#define gettext_noop(STR) STR
#define N_(STR) gettext_noop(STR) ///< Use this GNU gettext-compliant macro for translated strings


// More helpers...
void LangTranslateNumber (char *str);
  ///< Adapt numeric string by replacing all '.' by the locale's decimal point


/// @}  // Language



// *************************** Strings *****************************************


/** @defgroup base_strings Strings
 * Helper functions and classes for string handling
 *
 * A note on string encoding
 * -------------------------
 *
 * By default, all strings use UTF-8 encoding. All length and position parameters are
 * measured in bytes. Hence, they do not reflect the real numbers of characters, if
 * characters other than ASCII 0..127 are used.
 *
 * In cases, where this matters (e.g. in 'CInputLine'), the 'CString' class may
 * alternatively carry ISO8859-1(-15?) encoded strings with a one-to-one
 * correspondence between characters and bytes. The caller is responsible for making
 * the necessary conversions in the right places.
 *
 * By convention, strings that are not UTF8-encoded are marked with a comment
 * containing 'ISO8859' at their declaration.
 *
 *
 * A note on dynamic memory ownership
 * ----------------------------------
 *
 * Unless specified otherwise, pointers returned by functions or methods
 * are references to some memory which remains owned by the callee. Its lifetime
 * should be documented with the method or class. If unspecified, a pointer
 * returned by a class method is valid until the respective object is destructed.
 *
 *
 * A note on thread-temporary strings (TTS)
 * ----------------------------------------
 *
 * Often, functions are supposed to return a string as a return value
 * ('const char *f (,..)') for which the caller should not need to take
 * responsibility. Examples are the various 'ToStr ()' methods.
 *
 * For these situations, the function 'GetThreadTempString ()' returns
 * a single CString object that can be used globally. To avoid
 * synchronization issues, there is one thread-temporary string (TTS)
 * per thread.
 *
 * Functions using the TTS are marked with "TTS". Callers must take care
 * that the returned pointer is valid only until the next function labelled with
 * "TTS" is called (or until it uses the TTS itself).
 *
 * @{
 */


#define WHITESPACE " \t\n\r\v"    ///< white space characters according to 'man 3 isspace'


// ***** Misc. helpers *****


class CString *GetThreadTempString ();    ///< Get the TTS of the calling thread
static inline class CString *GetTTS () { return GetThreadTempString (); }   ///< Get the TTS

const char *StringF (class CString *ret, const char *fmt, ...);
  ///< Return 'ret->Get ()'
const char *StringF (const char *fmt, ...);
  ///< (TTS) Create a formatted string (linke `sprinf()`, but with inlimited length).
  /// Returned pointer is valid until the next TTS usage.

bool IntFromString (const char *str, int *ret);
  ///< Convert a String to an integer using 'strtol'. On success, '*ret' is set accordingly and 'true' is returned.
  /// On failure, 'false' is returned and '*ret' remains unchanged.

void StringStrip (char *str, const char *sepChars = WHITESPACE);
  ///< Remove seperator (whitespace) characters at the beginning and end of the string
void StringSplit (const char *str, int *retArgc, char ***retArgv, int maxArgc = INT_MAX, const char *sepChars = WHITESPACE);
  /**< Split a string into (whitespace-)separated arguments.
   * The number of extracted words is written to `*retArgc`, an array of that size is
   * returned via `***retArgv`.
   * If `maxArgc-1` strings have been split, the complete remaining string is
   * returned as argument #`(maxArgc-1)`.
   *
   * The resulting (sub-)strings will be placed into a single chunk of heap-allocated
   * memory. The caller must later (only) free `**retArgv[0]` and `*retArgv`.
   * `str` remains unchanged and owned by the caller.
   * *To avoid confusion about memory handling, it is recommended to use the
   * class `CSplitString` instead of this function.*
   */


/// @name Path handling ...
/// @{
void PathNormalize (char *str);             ///< Treat string as a (full) pathname and normalize it
void PathRemoveTrailingSlashes (char *str); ///< Treat string as a (full) pathname and remove trailing slash(es)
const char *PathLeaf (const char *str);     ///< Get leaf component of a path

const char *GetAbsPath (const char *relOrAbsPath, const char *defaultPath);
  ///< (TTS) Get absolute path.
const char *GetAbsPath (class CString *ret, const char *relOrAbsPath, const char *defaultPath);
  ///< Get absolute path.
  /// @param ret String to write the result to
  /// @param relOrAbsPath Input path
  /// @param defaultPath Default path
  /// @return Normalized absolute path
  /// If the input path starts with '/', it is considered an absolute path and
  /// returned unchanged.Otherwise, `defaultPath` is prepended, and the normalized
  /// path is returned.
/// @}


/// @name Transcoding ...
/// @{
const char *ToUtf8 (const char *iso8859str);
  ///< (TTS) Transcode an ISO8859-coded string to UTF-8.
  /// Result is valid until next call of this function.
const char *ToIso8859 (const char *str);
  ///< (TTS) Transcode an ISO8859-coded string to UTF-8.
  /// Result is valid until next call of this function.
/// @}



// ***** CString *****


/** Dynamically allocated string.
 *
 * This is the main class used in the *Home2L* project for strings of arbitrary
 * length. The strings storage (typically heap) may either be managed by this
 * class, but it may also be managed by the caller in order to avoid memory
 * duplication with constant strings, for example.
 */
class CString {
  public:
    CString () { size = 0; ptr = (char *) emptyStr; }
    CString (CString& str) { size = 0; Set (str.Get ()); }    ///< Copy constructor
    //~ CString (CString& str, int n0 , int dn = INT_MAX) { size = 0; Set (str.Get () + n0, dn); }
    CString (const char *str, int maxLen = INT_MAX) { size = 0; Set (str, maxLen); }
    ~CString () { if (size) free (ptr); }

    /// @name Initialization...
    ///@{
    void Clear () { SetC (emptyStr); }
    void Set (CString *str) { Set (str->Get ()); }
    void Set (CString *str, int n0, int dn = INT_MAX) { Set (str->Get () + n0, dn); }
    void Set (const char *str, int maxLen = INT_MAX);
      ///< Set (substring) from 'str'.
    void SetF (const char *fmt, ...);
      ///< Set using printf() formatting.
    void SetFV (const char *fmt, va_list ap);
      ///< Set using vprintf() formatting.

    void SetC (const char *_ptr);
      ///< Set without copying or taking ownership of '_ptr' (but "copy-on-write" semantics): more efficient, but requires '_ptr' to remain valid afterwards
      ///< '_ptr' must be a valid pointer (!= NULL).
    ///@}

    /// @name Ownership optimizations ...
    ///@{
    void SetO (const char *_ptr);
      ///< Set content and takes ownership of '_ptr'.
      ///< '_ptr' must have been dynamically allocated, since it will be free'd later using 'free ()'.
    char *Disown ();
      ///< Release ownership.
      /// Caller is responsible for freeing the pointer later using free().
      /// *Note:* If the memory of 'ptr' was previously not owned, a dynamic copy will be returned.
      ///       This guarantees, that 'free' can safely be called afterwards, but may be less efficient than
      ///       leaving the ownership with the string object.
    ///@}

    /// @name Static elements ...
    ///@{
    static const char emptyStr[];       ///< Use this whenever you need an empty string ("")
    ///@}

    /// @name Character encoding (see special not on string encoding in this file) ...
    ///@{
    void SetFromIso8859 (const char *iso8859str);   ///< 'this' will be a (normal) UTF-8 string, the source is expected to be ISO-8859
    void SetAsIso8859 (const char *str);            ///< 'this' will be ISO-8859-encoded string, the source is expected to be (normal) UTF-8
    ///@}

    /// @name Read access ...
    ///@{
    const char *Get () { return ptr; }    ///< Get the C string. Unless explicitely set by 'SetC', this will never return NULL or an invalid pointer
    int Len () { return strlen (ptr); }   ///< Get the lenght.
    bool IsEmpty () { return ptr[0] == '\0'; }  ///< Check, if empty.
    ///@}

    /// @name Modifications ...
    ///@{
    void Del (int n0, int dn = INT_MAX);

    void Ins (int n0, int dn, int *retInsPos = NULL);
    void Ins (int n0, char c);
    void Ins (int n0, const char *str, int maxLen = INT_MAX);
    void InsF (int n0, const char *fmt, ...);
    void InsFV (int n0, const char *fmt, va_list ap);

    void Append (char c) { Ins (INT_MAX, c); }
    void Append (const char *str, int maxLen = INT_MAX) { Ins (INT_MAX, str, maxLen); }
    void AppendF (const char *fmt, ...);
    void AppendFV (const char *fmt, va_list ap) { InsFV (INT_MAX, fmt, ap); }
    ///@}

    /// @name Extras ...
    ///@{
    int LFind (char c);
    int RFind (char c);
    int Compare (const char *str2);

    void Strip (const char *sepChars = WHITESPACE) { StringStrip (ptr, sepChars); }

    void Split (class CSplitString *args, int maxArgc = INT_MAX, const char *sepChars = WHITESPACE);
    void Split (int *retArgc, char ***retArgv, int maxArgc = INT_MAX, const char *sepChars = WHITESPACE) { StringSplit (ptr, retArgc, retArgv, maxArgc, sepChars); }

    void AppendFByLine (const char *fmt, const char *text);
      ///< Same as 'Set...', but appending the result to the current srtring.
    void SetFByLine (const char *fmt, const char *text) { Clear (); AppendFByLine (fmt, text); }
      ///< Split 'text' into lines and reassembles the text by processing each line through sprintf and 'fmt'.
      /// 'fmt' must contain no more than "%s" (or similar) conversion modifier and should contain a trailing '\n'.

    void AppendEscaped (const char *s, int maxChars = INT_MAX);
      ///< Same as 'Set...', but appending the result to the current srtring.
    void SetEscaped (const char *s, int maxChars = INT_MAX) { Clear (); AppendEscaped (s, maxChars); }
      ///< Set and escape all non-alphanumerical characters by "\...".
      /// If 's' is empty, "\0" is appended, so that the resulting string is never empty.
      /// Any escape sequence according to the C99 standard may be used.
      /// In addition to the C standard, the following sequences may occur here:
      /// `'\\s' == ' ' (space)`
      ///
      /// The resulting string does not contain any other characters besides '0'...'9', 'a'...'z', 'A'...'Z' and '\'.
      /// Even spaces are not contained.

    bool AppendUnescaped (const char *s);
      ///< Same as 'Set...', but appending the result to the current srtring.
    bool SetUnescaped (const char *s) { Clear (); return AppendUnescaped (s); }
      ///< Set and revers escapes. On error, 'false' is returned and the string remains unchanged.
      /// If the string has been generated by one of the '...Escaped' methods above, it is
      /// restored precisely. However, any character except '\' are tolerated in the input string
      /// and will be copied into the destination.
    /// @}

    /// @name Path handling ...
    /// These methods do the same as the functions with the same name.
    /// @{
    void PathNormalize () { ::PathNormalize (ptr); }
    void PathRemoveTrailingSlashes () { ::PathRemoveTrailingSlashes (ptr); }
    const char *PathLeaf () { return ::PathLeaf (ptr); }
    void PathGo (const char *where);
    void PathGoUp ();

    /// @name Using CString as a file buffer ...
    /// @{
    bool AppendFromFile (int fd);
      ///< Read as much as possible from 'fd' and append to buffer.
      /// If an end-of-file is encountered, 'false' is returned. 'fd' is NOT closed automatically.
    bool ReadLine (CString *retLine);
      ///< Consume and optionally return the first line from 'this'.
      /// On success, 'true' is returned. If no complete line is available, 'false' is returned
      /// and 'this' is not changed.
      /// In 'this', each line ends with a '\n'. The output ('retLine') does not contain the '\n'.
      /// 'retLine' can be 'NULL', in which case the line is ignored.
    ///@}

    /// @name Operators ...
    /// @{
    operator const char * () { return ptr; }

    CString& operator = (CString &str) { Set (str); return *this; }
    CString& operator = (const char *str) { Set (str); return *this; }
    char& operator [] (int n) { return ptr [n]; }
    CString operator + (const char *str);
    CString& operator += (char c) { Append (c); return *this; }
    CString& operator += (const char *str) { Append (str); return *this; }

    bool operator < (char *str) { return Compare (str) < 0; }
    bool operator > (char *str) { return Compare (str) > 0; }
    bool operator <= (char *str) { return Compare (str) <= 0; }
    bool operator >= (char *str) { return Compare (str) >= 0; }
    bool operator == (char *str) { return Compare (str) == 0; }
    bool operator != (char *str) { return Compare (str) != 0; }
    ///@}

    // Interfaces ...
    const char *ToStr () { return Get (); }   // (internal, for 'CDict')

  protected:
    char *ptr;
    int size;   // number of allocated bytes (including '\0`);
                //'size' == 0: nothing allocated in heap ('ptr' may still point to some constant string

    void SetSize (int _size);
};



// ***** CSplitString *****


/** Factory class to split a string into substrings
 *
 * Objects of this class are usually declared locally whereever
 * a string needs to be split into substrings, for example, as in
 * the argument analysis of a shell.
 */
class CSplitString {
  public:
    CSplitString () { argc = 0; argv = NULL; }
    CSplitString (const char *str, int maxArgc = INT_MAX, const char *sepChars = WHITESPACE) { StringSplit (str, &argc, &argv, maxArgc, sepChars); }
      ///< Initialize, set and split `str` (see `Set ()`).
    ~CSplitString () { Clear (); }

    void Clear ();    ///< Clear the object (only necessary if another string is to be split)

    void Set (const char *str, int maxArgc = INT_MAX, const char *sepChars = WHITESPACE) { Clear (); StringSplit (str, &argc, &argv, maxArgc, sepChars); }
      /**< Set and split a string.
       * @param str The string to split
       * @param maxArgc Maximum number of strings to return. If more substrings are
       *      contained in the input the last ones are returned unsplit as a whole.
       * @param sepChars Set of characters treated as separators
       */

    int Entries () { return argc; }     ///< Get number of string after splitting
    const char *Get (int idx) { return argv[idx]; }       ///< Get one string
    const char *operator [] (int n) { return Get (n); }   ///< Get one string

  protected:
    int argc;
    char **argv;
};



// ***** Regexp *****


/** Factory class for regular expression matching.
 *
 * This is basically a wrapper for regexp(3).
 */
class CRegex {
  public:
    CRegex () { reValid = false; lastError = 0; }
    ~CRegex () { if (reValid) regfree (&re); }

    bool SetPattern (const char *pattern, int cflags = REG_EXTENDED);
      ///< Set and compile a search pattern.
      /// @param pattern The pattern
      /// @param cflags Bit-wise OR of the following flags (see `regexp(3)`):
      ///   `REG_EXTENDED`:  Use POSIX Extended Regular Expression syntax
      ///   `REG_ICASE`:     Ignore case
      ///   `REG_NOSUB`:     Do  not  report  position  of matches
      ///   `REG_NEWLINE`:   Match-any-character operators don't match a newline
      /// @return 'true' on success, 'false' on failure
    bool Match (const char *s, int eflags = 0, size_t maxMatches = 0, regmatch_t *retMatchList = NULL);
      ///< Perform a regular expression search.
      /// @param s The string to be analysed
      /// @param eflags Bit-wise OR of the following flags (see `regexp(3)`):
      ///   `REG_NOTBOL`: The match-beginning-of-line operator always fails to match
      ///   `REG_NOTEOL`: The match-end-of-line operator always fails
      /// @param maxMatches At most this many matches are returned
      /// @param retMatchList If not `NULL`, the list of matches is written into `*retMatchList` (see `regexp(3)`)

    /// @name Error handling ...
    bool IsValid () { return reValid; }
    const char *ErrorStr ();

  private:
    CString errorStr;
    bool reValid;
    int lastError;
    regex_t re;
};


/// @}  // Strings





// ***************** Dictionaries and key sets *********************************


/** @defgroup base_containers Containers
 * Lightweight container classes for dictionaries and key sets.
 *
 * @{
 */


#define DICT_ALIGNMENT 4
#define DICT_ALIGN(X) (((X) + DICT_ALIGNMENT - 1) & ~(DICT_ALIGNMENT - 1))

#define DICT_KEYSIZE DICT_ALIGN((int) sizeof (CString))


/** Raw dictionary (base class for other dictionary variants)
 *
 * A dictionary stores arbitrary (key, value) pairs. The key is always of type
 * `CString` / `const char *`. Internally, the dictionary is stored as one
 * array containing the keys and an arbitrary (constant) number of bytes as
 * a value payload.
 *
 * This class stored typeless value data and is the base class for the other
 * dirctionary classes. It should not be used directly.
 */
class CDictRaw {
  public:
    CDictRaw (int _valueSize) { data = NULL; recSize = DICT_KEYSIZE + DICT_ALIGN(_valueSize); entries = allocEntries = 0; }
    ~CDictRaw () { Clear (); }

    void Clear () { SetEntries (0); }

    void Del (int idx);   ///< Delete entry; complexity is O(n).
    void Del (const char *key) { Del (Find (key)); }  ///< Delete entry; complexity is O(n).

    int Entries () { return entries; }    ///< Get number of entries.

    const char *GetKey (int idx) { return ((CString *) GetKeyAdr (idx))->Get (); }

    int Find (const char *key, int *retInsIdx = NULL);
      ///< Make binary search and return index of entry found or -1 if the key does not exist.
      /// If `retInsIdx != NULL`, the index to insert a new item is return in `*retInsIdx`.
    void PrefixSearch (const char *key, int *retIdx0, int *retIdx1);
      ///< Determine all elements with keys starting with `key`.
      /// @param key Key prefix
      /// @param retIdx0 Pointer to first return value, index of first matching entry.
      /// @param retIdx1 Pointer to second return value, index behind last matching entry.

    void Dump ();   ///< Dump contents by means of `INFO` logs (for debugging)

  protected:
    void Copy (int idxDst, int idxSrc);            // byte-copy entry
    void Swap (int idx0, int idx1);                // byte-swap two entries

    void RecInit (int idx);     // initializes record in previously uninitialized memory
    void RecClear (int idx);    // clears (initialized) record such that no destructor call is necessary anymore

    void SetEntries (int _entries);

    virtual void ValueInit (void *p) {}             // initialize value data in previously uninitialized memory
    virtual void ValueClear (void *p) {}            // clear (initialized) record such that no destructor call is necessary anymore
    virtual void ValueSet (void *p, void *orig) {}  // copy 'orig' to 'p' properly; both are initialized objects
    virtual const char *ValueToStr (void *p);       // (TTS) return readable string for the 'Dump' method

    uint8_t *GetKeyAdr (int idx) { return idx < 0 ? NULL : data + recSize * idx; }
    uint8_t *GetValueAdr (int idx) { return idx < 0 ? NULL : GetKeyAdr (idx) + DICT_KEYSIZE; }
    void *GetValuePtr (int idx) { return idx < 0 ? NULL : * (void **) (GetKeyAdr (idx) + DICT_KEYSIZE); }

    void SetRaw (int idx, void *value);            // entry must exist and 'idx' be valid
    int SetRaw (const char *key, void *value);     // adds or replaces the keyed entry; returns new index

    void MergeRaw (CDictRaw *map2);                // merge 'map2' into this one

    uint8_t *data;
    int recSize, entries, allocEntries;
};


/** Fast dictionary
 *
 * This class stores the value object in the same array as the keys
 * and is therefore generally more efficient than the alternative class 'CDict'.
 *
 * The type 'T' must fullfill the following properties:
 * 1. The default constructor must not allocate any dynamic data
 *    (A sample object will be copied using 'memcpy' to construct other objects.)
 * 2. It must be location-invariant, since it may be moved in memory (e.g. no pointers
 *    must exist).
 * 3. It must implement a method 'const char *ToStr ()' for the 'Dump' method.
 *    (the returned string will not be used after a further call to this method for any
 *    object, so that it can e.g. be stored in a static variable)
 */
template <typename T> class CDictFast: public CDictRaw {
  public:
    CDictFast (): CDictRaw (sizeof (T)) { ValueClear (&emptyObj); }

    /// @name Read access ...
    /// @{
    T *Get (int idx) { return (T *) GetValueAdr (idx); }
    T *Get (const char *key) { return Get (Find (key)); }
    T *operator [] (int idx) { return Get (idx); }
    T *operator [] (const char *key) { return Get (key); }
    /// @}

    /// @name Write access ...
    /// @{
    void Set (int idx, T *value) { SetRaw (idx, value); }
      ///< Set (replace) a value.
      /// The entry must exist and 'idx' be valid. Complexity is O(1).
    int Set (const char *key, T *value) { return SetRaw (key, value); }
      ///< Add or replace the keyed entry. Complexity is O(n).
      /// @return Index of new entry
    void Merge (CDictFast<T> *map2) { SetRaw (map2); }
      ///< Merge another map into this one.
      /// Complexity is O(n_this + n_map2).
      /// The recommended way to add a bunch of new keys to a map is to first add
      /// them to a new map and then use this method to merge them into 'this'
      /// using this method.
    /// @}

  protected:
    virtual void ValueInit (void *p) { memcpy (p, &emptyObj, sizeof (emptyObj)); }
    virtual void ValueClear (void *p) { ValueSet (p, &emptyObj); }
    virtual void ValueSet (void *p, void *orig) { * (T *) p = * (T *) orig; }
    virtual const char *ValueToStr (void *p) { return ((T *) p)->ToStr (); }

    T emptyObj;   // Template for an initialized object
};


/** Dictionary of references
 *
 * This class stores references to the named object without taking over ownership.
 * Use the class 'CDict' instead, if a classical container for dynamically
 * allocated objects is desired.
 *
 * The type 'T' must fullfill the following properties:
 *
 * 1. It must implement a method 'const char *ToStr ()' for the 'Dump' method.
 *    (the returned string will not be used after a further call to this method for any
 *    object, so that it can e.g. be stored in a static variable.)
 */
template <typename T> class CDictRef: public CDictRaw {
  public:
    CDictRef (): CDictRaw (sizeof (T *)) {}

    /// @name Read access ...
    /// @{
    T *Get (int idx) { return (T *) GetValuePtr (idx); }
    T *Get (const char *key) { return Get (Find (key)); }
    T *operator [] (int idx) { return Get (idx); }
    T *operator [] (const char *key) { return Get (key); }
    /// @}

    /// @name Write access ...
    /// @{
    void Set (int idx, T *value) { SetRaw (idx, &value); }
      ///< Set (replace) a value.
      /// The entry must exist and 'idx' be valid. Complexity is O(1).
    int Set (const char *key, T *value) { return SetRaw (key, &value); }
      ///< Add or replace the keyed entry. Complexity is O(n).
      /// @return Index of new entry

    void Merge (CDictRef<T> *map2) { SetRaw (map2); }
      ///< Merge another map into this one.
      /// Complexity is O(n_this + n_map2).
      /// The recommended way to add a bunch of new keys to a map is to first add
      /// them to a new map and then use this method to merge them into 'this'
      /// using this method.
    /// @}

  protected:
    virtual void ValueInit (void *p) { * (T **) p = NULL; }
    virtual void ValueClear (void *p) { * (T **) p = NULL; }
    virtual void ValueSet (void *p, void *orig) { * (T**) p = * (T**) orig; }
    virtual const char *ValueToStr (void *p) { return (* (T**) p)->ToStr (); }
};


/** Flexible dictionary
 *
 * The class is like 'CDictRef', except that it takes over ownership of
 * any object passed to the 'Set ()' calls.
 * Presently, there is no way to release ownership, so that the 'Merge'
 * method has to be disabled.
 */
template <typename T> class CDict: public CDictRef<T> {
  public:

    /// @name Write access ...
    /// @{
    void Merge (CDict<T> *map2) { ASSERT (false); }  ///< Merging with proper ownerships not implemented (yet)
    /// @}

  protected:
    virtual void ValueClear (void *p) { if (* (T**) p) { delete * (T **) p; * (T **) p = NULL; } }
    //~ virtual void ValueSet (void *p, void *orig) { if (* (T**) p) delete * (T **) p; * (T**) p = new T (* (T**) orig); }
};


/** Set of strings (keys)
 */
class CKeySet: public CDictRaw {
  public:
    CKeySet (): CDictRaw (0) {}

    /// @name Read access ...
    /// @{
    const char *operator [] (int idx) { return GetKey (idx); }
    /// @}

    /// @name Write access ...
    /// @{
    int Set (const char *key) { return SetRaw (key, NULL); }

    void Merge (CKeySet *map2) { MergeRaw (map2); }
    /// @}

  protected:
    const char *ValueToStr (void *p) { return NULL; }
};


/// @}  // Containers





// ********************** Date & Time ******************************************


/** @defgroup base_datetime Date and Time
 * Various defitions related to date and time handling.
 *
 * The *Home2L* suite uses two different measures of time (clocks), as also supported
 * by the Linux kernel:
 * 1. The real time (`TTicks`): This is the absolute time, eventually synchronized
 *    via network with time servers. This clock should be used for calendar or
 *    alarm clock applications, where the absolute date and time matters.
 * 2. A monotonic time (`TTicksMonotonic`): This is some relative time which never
 *    makes quick changes in speed or even may move backwards (as is possible with
 *    `TTicks` after network synchronization). This clock is used wherever possible,
 *    particularly for @ref base_timers.
 *
 * Besides the two basic types `TTicks` and `TTicksMonotonic`, this group contains
 * some helper types `TDate` and `TTime` for calendar time handling.
 * @{
 */


/// @name Ticks (real and monotonic) ...
/// @{

typedef int64_t TTicks;           ///< Real time; number of milliseconds since the Epoch (1970-01-01-000000 UTC)
typedef int32_t TTicksMonotonic;  ///< Monotonic clock; number of milliseconds since the first invocation of GetTicksMonotonic(); TBD: test int64_t

#define NEVER -1                  ///< Represents no time (for both types `TTicks` and `TTicksMonotonic`)

static inline bool TicksIsNever (TTicks t) { return t < 0; }
static inline bool TicksMonotonicIsNever (TTicksMonotonic t) { return t < 0; }

TTicks TicksNow ();                   ///< Get the current real time in milliseconds
TTicksMonotonic TicksMonotonicNow (); ///< Get the current monotonic in milliseconds

TTicks TicksFromMonotic (TTicksMonotonic tm); ///< Conversion function
TTicksMonotonic TicksToMonotonic (TTicks t);  ///< Conversion function

const char *TicksToString (TTicks ticks, int fracDigits = INT_MAX, bool precise = false);
  ///< (TTS) Get a string of the form "YYYY-MM-DD-hhmmss[.<millis>]" in local time.
const char *TicksToString (CString *ret, TTicks ticks, int fracDigits = INT_MAX, bool precise = false);
  ///< Get a string of the form "YYYY-MM-DD-hhmmss[.<millis>]" in local time.
  /// @param ret Return string
  /// @param ticks Ticks to be converted
  /// @param fracDigits Number of fractional digits. If set to -1, the seconds are skipped, too.
  ///             If set to 'INT_MAX', the number of (fractional) seconds digits is determined
  ///             automatically (trailing 0s may be skipped).
  /// @param precise If 'true', a string of the form "`t<n>`" is returned, where
  ///             `<n>` is the decimal number milliseconds since the Epoch.
bool TicksFromString (const char *str, TTicks *ret);
  ///< Convert a string to an absolute ticks value. The following formats are accepted:
  ///  - `YYYY-MM-DD[-hhmm[ss[.<millis>]]]` : readable time, interpreted as local time
  ///  - `t<unsigned integer>`              : absolute time in milliseconds since the Epoch
  ///  - `<integer>`                        : relative time, milliseconds from now
  ///  - `hh:mm[:ss[.<millis>]]`            : time relative to 0:00 today; `hh` may be > 23 to specify a time in the coming days (*Beware of potential race conditions when using this close to midnight!*)

void TicksMonotonicToStructTimeval (TTicksMonotonic t, struct timeval *retTv);

#define TICKS_FROM_SECONDS(S) (((TTicks) (S)) * 1000)         ///< Convert seconds (integer!) to ticks
#define SECONDS_FROM_TICKS(T) (((TTicks) (T) + 500) / 1000)   ///< Convert seconds (integer!) to ticks

/// @}


/// @name Date/time: Types ...
/// @{

typedef int TDate;      // Date encoded as bit fields

#define DATE_OF(Y,M,D) ((TDate) (((Y) << 9) | ((M) << 5) | (D)))
#define YEAR_OF(DT) ((int) (DT) >> 9)
#define MONTH_OF(DT) (((int) (DT) >> 5) & 0xf)    // range is 1..12 (different from 'struct tm' in <time.h>)
#define DAY_OF(DT) ((int) (DT) & 0x1f)            // range is 1..31


typedef int TTime;      // Time in seconds since midnight

#define TIME_OF(H,M,S) ((TTime) ((H) * 3600 + (M) * 60 + (S)))
#define HOURS_OF(T) ((int) (T) / 3600)
#define MINUTES_OF(T) ((int) (T) / 60)
#define HOUR_OF(T) HOURS_OF(T)
#define MINUTE_OF(T) (MINUTES_OF(T) % 60)
#define SECOND_OF(T) ((int) (T) % 60)

/// @}


/// @name Conversion between ticks and date/time ...
/// @{
TTicks TicksOfDate (int dy, int dm, int dd);      // range of 'dm' is 1..12
TTicks TicksOfDate (TDate d);
TTicks TicksOfTime (int th, int tm, int ts);
TTicks TicksOfTime (TTime t);

TDate DateOfTicks (TTicks t);
TTime TimeOfTicks (TTicks t);

TTicks DateTimeToTicks (TDate d, TTime t, struct tm *retTm = NULL);
#if !ANDROID
TTicks DateTimeToTicksUTC (TDate d, TTime t, struct tm *retTm = NULL);
#endif
void TicksToDateTime (TTicks t, TDate *retDate, TTime *retTime, struct tm *retTm = NULL);
void TicksToDateTimeUTC (TTicks t, TDate *retDate, TTime *retTime, struct tm *retTm = NULL);
/// @}


/// @name Date/time: Current time...
/// @{
static inline void GetDateTimeNow (TDate *d, TTime *t) { TicksToDateTime (TicksNow (), d, t, NULL); }
  ///< Retrieve current date and time (both pointers may be NULL)
TDate Today ();
TTicks TicksToday ();
/// @}


/// @name Date/time: Arithmetics...
/// @{
TDate DateIncByDays (TDate date, int dDays);
int DateDiffByDays (TDate d1, TDate d0);    // returns "d1" - "d0" in days

TDate DateIncByMonths (TDate date, int dMon);

static inline TDate DateFirstOfMonth (TDate date) { return (date & ~0x1f) + 1; }

int GetWeekDay (TDate date);    // range is 0..6 (Mon..Sun)
int GetCalWeek (TDate date);
/// @}


/// @name Date/time: Written month and weekday names ...
/// @{
const char *MonthName (int dm);       ///< Get textual name. 1 = "January", ..., 12 = "December".
const char *MonthNameShort (int dm);  ///< Get textual shortname. 1 = "Jan", ..., 12 = "Dec".
const char *DayName (int wd);         ///< Get textual name. 0 = "Monday", ..., 6 = "Sunday".
const char *DayNameShort (int wd);    ///< Get textual name. 0 = "Mon", ..., 6 = "Sun".
/// @}



/// @}  // DateTime





// *************************** Timer *******************************************


/** @defgroup base_timers Timers
 * Scheduling timed and/or repeated operations.
 *
 * A timer, represented by a @ref CTimer object, allows to execute some user-defined
 * function at a given time. The class can be used in one of two ways,
 * 1. by setting a callback function of type @ref FTimerCallback, or
 * 2. by overriding the virtual method CTimer::OnTime().
 *
 * The timed functions are effectively executed from inside TimerIterate()
 * or TimerRun(), which is to be executed in some thread referred to as the
 * *timer thread*. It is recommend, but not required, that the timer thread
 * is the same as the main (UI) thread of the main application.
 *
 * The timer can be executed by the main program in one of three ways:
 *
 * 1. If there is already an exisiting main event loop (e.g. via SDL), the non-blocking
 *    functions `TimerIterate()` and `TimerGetDelay()` should be used.
 * 2. If the application does not have an own main loop, the functions
 *    `TimerRun()` (blocking), and `TimerStop()` may be used to easily implement one.
 * 3. As a third option, a separate timer thread can be started with
 *    `TimerStart()` and later be stopped by `TimerStop()`.
 *
 * **Note:** For efficiency reasons (minimize wakeups and context switches),
 * repeated (`_interval > 0`) timers should always defined such that the
 * interval is a power of 2. They are aligned internally to times dividable
 * by `_interval` in a way that they may happen earlier, but never later than
 * specified.
 *
 * @{
 */


typedef void FTimerCallback (class CTimer *timer, void *data);
  ///< Function type for timer callbacks
  /// @param timer The calling timer.
  /// @param data User data as passed to the @ref CTimer object.


/** Timer class
 */
class CTimer {
  public:
    CTimer ();
    CTimer (FTimerCallback *_func, void *_data = NULL, void *_creator = NULL) { Set (_func, _data, _creator); }
      ///< Construct with a callback function.
    CTimer (TTicksMonotonic _time, TTicksMonotonic _interval = 0, FTimerCallback *_func = NULL, void *_data = NULL, void *_creator = NULL) { Set (_time, _interval, _func, _data, _creator); }
      ///< Construct with a time/interval specification and eventually a callback function.
    virtual ~CTimer () { Clear (); }

    void Set (FTimerCallback *_func, void *_data = NULL, void *_creator = NULL);
      ///< Setup timer without (re-)scheduling it.
    void Set (TTicksMonotonic _time, TTicksMonotonic _interval = 0, FTimerCallback *_func = NULL, void *_data = NULL, void *_creator = NULL);
      ///< Setup and (re-)schedule timer.
      /// @param _func Callback function to be executed at the given times.
      /// @param _data Data to be passed as the `data` argument to the callback function.
      /// @param _creator A reference to the creator. If set (`!= NULL`), the class takes over the ownership.
      ///             of this object. In consequence, the caller does not need to keep a reference to this object,
      ///             but can eventually remove all of them using `DelByCreator()`.
      /// @param _time Absolute time of the next invocation. A value of 0 means "now".
      ///             A value < 0 means "in `-_time` milliseconds from now".
      /// @param _interval Repeat interval. A value of 0 inserts a one-shot timer.
      ///             This number should be a power of 2 (see note above).

    void Reschedule (TTicksMonotonic _time, TTicksMonotonic _interval = 0);
      ///< Change a timer (like 'Set'), but leave function and creator unchanged.
    void Clear ();                              ///< Remove timer from the event list.
    static void DelByCreator (void *_creator);  ///< Remove all timers created by `_creator` from the event list.
    void *GetCreator () { return creator; }

    bool Pending () { return isLinked; }
      ///< Indicate whether the timer is pending and may be executed in the future.
      /// **Note:** Be careful with potential race conditions. A value of 'false' can be safely be
      ///   used to draw conclusions, given that only the calling thread may call 'Set' & friends.
      ///   A value of 'true' may change to 'false' at any time.

    // Timer callback...
    virtual void OnTime () { if (func) func (this, data); }
      ///< [T:timer] Virtual function called when the timer triggers.

  protected:
    friend bool TimerIterate ();
    friend TTicksMonotonic TimerGetDelay ();
    friend void TimerRun ();

    static bool ClassIterateAL ();
    static TTicksMonotonic GetDelayTimeAL ();

    void InsertAL ();
    void UnlinkAL ();

    static CTimer *first;   // for chained list
    CTimer *next;           // for chained list
    bool isLinked;          // indicates, whether element is in the linked list

    TTicksMonotonic nextTicks, interval;
    void *creator;        // this object is managed externally, the caller has a reference to it and must remove it

    FTimerCallback *func;
    void *data;
};



/// @name Control functions (for the main program) ...
/// @{
bool TimerIterate ();
  ///< Returns false if nothing was done.
TTicksMonotonic TimerGetDelay ();
  ///< Returns number of milliseconds until next call to `TimerIterate()` is necessary, or -1 if no timer is pending.

void TimerRun ();   ///< Perform an endless loop calling timers until `TimerStop()` is called.
void TimerStart (); ///< Start `TimerRun()` in a background thread, which is to be stopped using `TimerStop()`.
                    ///< All timer functions are then run from the same, namely this background thread.
void TimerStop ();  ///< Request `TimerRun()` to return at next occasion (can be called from any thread).
                    ///   If a dedicatd timer thread was started, this is joined, too.
/// @}


/// @}  // Timers





// ***************** Threading and Synchronization ****************************


/** @defgroup base_threads Threads and Synchronization
 * Wrapping classes related to threads and synchronization.
 *
 * @{
 */


typedef void *FThreadRoutine (void *data);
  ///< Function type for thread routines
  /// @param data User data as passed to the @ref CThread object.


/** Class to wrap (POSIX) threads
 *
 * The thread routine can be supplied in the following ways:
 * 1. Instantiate the base class @ref CThread and specifiy a @ref FThreadRoutine function.
 * 2. Derive a subclass and overload CThread::Run().
 *
 * Please not, that there is intenionally no 'cancel()' functionality, since this
 * is a) presently not supported under Android, and b) almost certainly introduces
 * inproper code with race conditions.
 */
class CThread {
  public:
    CThread () { running = false; }
    virtual ~CThread () { if (running) Join (); }

    void Start (FThreadRoutine *_routine, void *_data = NULL);
      ///< Run the supplied function `_routine` in the background passing `_data` as an argument.
    void Start ();  ///< Run the virtual method Run() in the background.
    void *Join ();  ///< Join with the background thread.

    bool IsRunning () { return running;  } ///< (only for main thread)

  protected:
    friend void *CThreadRoutine (void *);

    virtual void *Run () { return NULL; } ///< Main thread routine (to be overloaded)

    bool running;
    pthread_t thread;
};


/** Class to wrap (POSIX) mutex objects
 */
class CMutex {
  public:
    CMutex ();
    ~CMutex ();

    void Lock ();
    bool TryLock ();
    void Unlock ();

  protected:
    friend class CCond;
    pthread_mutex_t mutex;
};


/** Class to wrap (POSIX) condition variables
 */
class CCond {
  public:
    CCond ();
    ~CCond ();

    void Wait (CMutex *mutex);
      ///< Wait until woken up.
      /// The supplied `mutex` must be locked and will be atomically unlocked while waiting.
    TTicksMonotonic Wait (CMutex *mutex, TTicksMonotonic maxTime);
      ///< Wait until woken up or a time-out occured.
      /// @param mutex Mutex to unlock while waiting.
      /// @param maxTime Maximum wait time in milliseconds.
      /// If woken up by signalling or by a spurious wakeup (see POSIX specification), the time left to wait is returned.
      /// If the maximum time has passed, -1 is returned.
      ///
      /// **Please note,** that unlike the POSIX recommendations, the time-out is not
      /// specified as an absolute time here, but as a time relative from "now".
      /// This is more convenient, but care has to be taken when calling this method
      /// multiple times to wait for a certain total time. To be accurate in total,
      /// the remaining time for the repeateds invocation should *not* be taken from
      /// the return value, but recomputed according to the current time (to be
      /// determined by TicksNow().
    void Signal ();           ///< Wakeup ONE waiting thread.
    void Broadcast ();        ///< Wakeup ALL waiting threads.

  protected:
    pthread_cond_t cond;
};


/** Sleep until one out of several i/o operations becomes possible.
 *
 * This class serves as an interface to the 'select' system call.
 * Furthermore, it helps to implement worker threads that (may) monitor files
 * and receive commands (events) with the help of a self-pipe.
 */
class CSleeper {
  public:
    CSleeper ();
    ~CSleeper () { Done (); }
    void Done ();

    /// @name Enable the command mechanism ...
    /// @{
    void EnableCmds (int _cmdRecSize);
      ///< Enable sending commands by PutCmd().
      /// To be called immediately after initialization and only if the command feature is used.
    /// @}

    /// @name For the sleeping thread: Preparation...
    /// @{
    void Clear ();
    void AddReadable (int fd);  ///< add file descriptor `fd` to monitor for readability, descriptors < 0 are silently ignored.
    void AddWritable (int fd);  ///< add file descriptor `fd` to monitor for writability, descriptors < 0 are silently ignored.
    /// @}

    /// @name For the sleeping thread: Sleep...
    /// @{
    void Sleep (TTicksMonotonic maxTime = -1);
      ///< Sleep until one file descriptor is readable/writable or a new command was received.
      /// @param maxTime Maximum waiting time; -1: wait forever, 0 => do not wait
      /// A non-blocking Sleep() (`maxTime` == 0) updates the internal readability/writability flags.
    /// @}

    /// @name For the sleeping thread: Afterwards...
    /// @{
    bool GetCmd (void *retCmdRec);  ///< Get and consume next command (non-blocking). @returns 'true' if none is available
    bool IsReadable (int fd);   ///< Check if file descriptor is readable (flags are only updated in Sleep() ).
    bool IsWritable (int fd);   ///< Check if file descriptor is writable (flags are only updated in Sleep() ).
    /// @}

    /// @name For other threads...
    /// @{
    void PutCmd (const void *cmdRec, TTicksMonotonic t = 0, TTicksMonotonic _interval = 0);
      ///< Send message to sleeper.
      /// `cmd == NULL` represents "no command" and will be ignored by receiver.
      /// The command can optionally be scheduled for a certain time or even be repeated.
      /// `t` is an absolute time, `t == 0` represents "now".
    /// @}

  protected:
    fd_set fdSetRead, fdSetWrite;
    int maxFd, cmdRecSize;
    int selfPipe[2];
};



void Sleep (TTicksMonotonic mSecs);
  ///< Suspend for `mSecs` milliseconds using `nanosleep()`.



/// @}  // Threads







// ********************** CShell and variants **********************************


/** @defgroup base_shells Shells
 * Shell classes to run and iteract with external processes.
 *
 * @{
 */



// ***** Abstract base class and functions *****


/** Abstract base class for shells that can execute system commands.
 * **Note:** All methods (for one object) must be called from the same thread.
 */
class CShell {
  public:
    CShell () { exitCode = -1; }
    virtual ~CShell () {}

    /// @name Execution control...
    /// @{
    virtual bool Start (const char *cmd) = 0;
      /**< Start command.
       * @param cmd The exact (and arbitrary) shell command.
       * If a previous command is still running, Wait() is executed before.
       * On error, a warning is printed and 'false' is returned.
       * Only one command can be executed at a time. It can be understood that the Start() method includes
       * an implicit Mutex-Lock operation, and the lock has been obtained (only) if 'true' is returned.
       * The corresponding Mutex-Unlock operation is implied by calling Wait().
       */
    virtual bool StartRestricted (const char *name, const char *args = NULL);
      /**< Same as 'Start', but fetch the command from the environment.
       * This way, only commands explicitly defined in the environment settings are allowed, which
       * is a security improvment.
       * The actual command is taken from the environment variable `rc.cmd.<name>`.
       * The command name may contain the placeholder "%s", which is replaced by `args`.
       * This mode of command definition is meant for remote network execution to avoid security holes.
       */
    virtual bool IsRunning () { return !ReadClosed (); }
      ///< Still running?
    virtual void Wait () = 0;
      ///< Wait until current command completes.
    virtual void Kill (int sig = SIGTERM) { WriteClose (); }
      ///< Kill the current command nicely.
      /// This may not work for all types of shells.
      /// A Wait() invocation is required anyway afterwards.
    int ExitCode () { return exitCode; }
      ///< Get the exit code of last command.
    /// @}

    /// @name Simple running...
    /// @{
    int Run (const char *cmd, const char *input = NULL, CString *output = NULL);
      ///< Run command 'cmd' synchronously and return its exit code.
      /// The standard input is taken from 'input' and the output is writen to the string referred to
      /// by 'output'.
      /// This function is only appropriate for short running
      /// commands since the calling thread blocks until the command completes.
    /// @}

    /// @name Accessing stdin & stdout...
    /// @{
    virtual void CheckIO (bool *canWrite, bool *canRead, TTicksMonotonic maxTime = -1) = 0;
      /**< Check whether the i/o channels can take/deliver data.
       * @param canWrite may point to a variable that will be set, depending on whether at least one
       *    byte can be written to the external command.
       * @param canRead may point to a variable that will be set, depending on whether at least one
       *    byte can be read from the external command.
       * If any of the two pointers is 'NULL', the caller indicates that he is not interested in the
       * respective result. Hence, the method waits until the respective other channel is ready.
       * @param maxTime is the maximum waiting time in milliseconds. A value <0 lets it wait forever,
       *    a value 0 does not cause any waiting at all.
       * In situations, where CheckIO() would wait forever, it returns immediately with both flags
       * set to 'false' (for example: read channel already closed, and `canWrite == NULL`).
       * **Note:** As long as both the read and the write channel are open, the caller (usually) must
       * always write *and* read as much as possible. Otherwise, a deadlock may occur.
       */
    bool WaitUntilReadable (TTicksMonotonic timeOut = -1) { bool canRead; CheckIO (NULL, &canRead, timeOut); return canRead; }
      /**< Wait until output of the external command is readable.
       * Be aware of potential deadlocks. This command can only be safely used if the external
       * command does not wait for input.
       * @return 'true', if the output is readable.
       */
    bool WaitUntilWritable (TTicksMonotonic timeOut = -1) { bool canWrite; CheckIO (&canWrite, NULL, timeOut); return canWrite; }
      /**< Wait until the external command can accept written input.
       * Be aware of potential deadlocks. This command can only be safely used if the external
       * command does not produce any output.
       * @return 'true', if the output is writeable.
       */

    bool Writable () { return WaitUntilWritable (0); }  ///< Poll for writability (non-blocking).
    virtual void WriteLine (const char *line) = 0;
      ///< Blocking write, execution is guaranteed.
      /// To avoid blocking, run Writable() or CheckI() first.
    void WriteLine (CString *str) { WriteLine (str->Get ()); }
    virtual void WriteClose () = 0;                     ///< Close the write channel.

    bool Readable () { return WaitUntilReadable (0); }  ///< Poll for readability.
    virtual bool ReadLine (CString *str) = 0;
      ///< Non-Blocking read, returns 'true' on success.
      /// `str` can be NULL, in which case the line is ignored, given the reading was successul.
    virtual bool ReadClosed () = 0;                     ///< Close the read channel.
    /// @}

  protected:
    int exitCode;
};




// ***** CShellBare *****


/** Normal shell: Execute commands individually
 *
 * Each command is executed separately using 'fork'/'exec'.
 */
class CShellBare: public CShell {
  public:
    CShellBare () { host = NULL; newProcessGroup = false; fdToScript = fdFromScript = childPid = -1; }
    virtual ~CShellBare () { Done (); }
    void Done ();

    /// @name Execution control...
    /// @{
    virtual bool Start (const char *cmd);  ///< Start new command, optionally on a remote host (see below).
    virtual bool IsRunning ();
    virtual void Wait ();
    virtual void Kill (int sig = SIGTERM); ///< (not tested in shell mode!)
    /// @}

    /// @name Accessing stdin & stdout...
    /// @{
    virtual void CheckIO (bool *canWrite, bool *canRead, TTicksMonotonic maxTime = 0);
      ///< Use select() to check whether the i/o channels can take/deliver data.

    virtual void WriteLine (const char *line);
    virtual void WriteClose ();

    virtual bool ReadLine (CString *str);
    virtual bool ReadClosed () { return fdFromScript < 0 && !readBufMayContainLine; }
    /// @}

    /// @name Query FDs to allow to use 'select' to wait for a new I/O status...
    ///   The FDs returned by the following methods may only be used by 'select()' or @ref CSleeper.
    ///   Reading from or writing to them is not allowed.
    ///   Waiting on the FDs is only allowed after Readable() / Writable()
    ///   (or their CheckIO() equivalents) returned 'false' before.

    /// @{
    int ReadFd () { return fdFromScript; }   ///< Get FD to allow select() by owner with multiple scripts.
    int WriteFd () { return fdToScript; }    ///< Get FD to allow select() by owner with multiple scripts.
    /// @}

    /// @name Extra features...
    /// @{
    void SetHost (const char *_host) { host = _host; }  ///< Set host to run subsequent commands on; 'NULL' represents the local host.
    void SetNewProcessGroup (bool on = true) { newProcessGroup = on; }
      ///< Create a new process group and let this process become the leader (for daemon to support job control).
    bool StartSession () { return Start (NULL); }       ///< Start a commmand shell session.
    /// @}

  protected:
    const char *host;
    bool newProcessGroup;
    CString readBuf;
    bool readBufMayContainLine;
    int fdToScript, fdFromScript;
    int childPid, killSig;

    bool DoWaitPid (int options);
};



// ***** CShellSession *****


/** Session shell: Execute multiple commands through one shell
 *
 * All commands are executed through a common command shell, which is (re-)opened by the 'Start' method
 * and left open until 'Done' is called. Compared to a bare shell, this greatly improves efficiency
 * if multiple commands have to be executed on a remote host.
 */
class CShellSession: public CShell {
  public:
    CShellSession () { writeOpen = readOpen = false; }
    virtual ~CShellSession () { Done (); }
    void Done ();

    /// @name Extra features ...
    /// @{
    void SetHost (const char *_host) { session.SetHost (_host); }
      ///< Define a remote host; must be called before first Start() call.
    /// @}

    /// @name Execution control...
    /// @{
    virtual bool Start (const char *cmd);
    virtual void Wait ();
    /// @}

    /// @name Accessing stdin & stdout...
    /// @{
    virtual void CheckIO (bool *canWrite, bool *canRead, TTicksMonotonic timeOut = -1);

    virtual void WriteLine (const char *line) { session.WriteLine (line); }
    virtual void WriteClose ();

    virtual bool ReadLine (CString *str);
    virtual bool ReadClosed () { return !readOpen; }

    int ReadFd () { return session.ReadFd (); }      ///< Get FD to allow select() by owner with multiple scripts.
    int WriteFd () { return session.WriteFd (); }    ///< Get FD to allow select() by owner with multiple scripts.
    /// @}

  protected:
    CShellBare session;
    bool writeOpen, readOpen;
};


/// @}  // Shells




/// @}  // @file


#endif
