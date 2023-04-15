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


#ifndef _BASE_
#define _BASE_


/** @file
 * @defgroup common Common
 * @brief Common definitions and classes for all *Home2L* components.
 *
 * @{
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <regex.h>
#include <stdarg.h>

#include <config.H>





// *************************** Configuration ***********************************


#ifdef __ANDROID__
#define ANDROID 1   ///< @brief 1, if compiling for Android (NDK), 0 otherwise
#else
#define ANDROID 0
#endif





// ***************** Basic definitions and functions ***************************


/** @defgroup common_basics Basics
 * @brief Some very basic definitions and functions
 *
 * @{
 */


#define MIN(A, B) ((A) < (B) ? (A) : (B))   ///< Typeless min-operator
#define MAX(A, B) ((A) > (B) ? (A) : (B))   ///< Typeless max-operator

#define STR(X) #X
  ///< @brief Stringify macro

#define OFFSET(type, field) ((unsigned) &(((type *) 0)->field))
  ///< @brief Byte offset of a field in struct or class

#define ENTRIES(ARRAY) ((int) (sizeof (ARRAY) / sizeof (ARRAY[0])))
  ///< @brief Number of entries of a statically declared constant array

#define CLEAR(X) bzero (&(X), sizeof (X))
  ///< @brief Set variable/object 'X' to all-zero


/// @name Atomic read and write operations ...
///   (presently GCC-specific)
/// @{
#define ATOMIC_READ(OBJ)        __atomic_load_n (&(OBJ), __ATOMIC_RELAXED)
#define ATOMIC_WRITE(OBJ, VAL)  __atomic_store_n (&(OBJ), VAL, __ATOMIC_RELAXED)
#define ATOMIC_INC(OBJ, N)      __atomic_exchange_n (&(OBJ), (OBJ) + (N), __ATOMIC_RELAXED)
/// @}


/// @name Enhanced file and directory operations ...
/// @{
size_t Write (int fd, const void *buf, size_t count);
  ///< @brief Similar to POSIX write(), but writes all 'count' bytes as possible.
  ///
  /// This function repeatedly calls write(2) until 'count' bytes have been written.
  /// On error, the actual number of bytes written is returned (which is less than
  /// 'count'), and 'errno' is set accordingly.
  /// If the file is opened in non-blocking mode, the function does not wait, but
  /// writes as many bytes as possible immediately and sets/leaves 'errno == EAGAIN'.
  /// @return the number of bytes written successfully.

size_t Read (int fd, void *buf, size_t count);
  ///< @brief Similar to POSIX read(), but reads all 'count' bytes as possible.
  ///
  /// This function repeatedly calls read(2) until 'count' bytes have been read.
  /// On error or end-of-file, the actual number of bytes read is returned (which
  /// is less than 'count'), and 'errno' is set accordingly. In the case of an
  /// end-of-file condition, 'errno' is set to 0.
  /// If the file is opened in non-blocking mode, the function does not wait, but
  /// returns the bytes available immediately and sets/leaves 'errno == EAGAIN'.
  /// @return the number of bytes read successfully.

bool MakeDir (const char *relOrAbsPath, bool setHome2lGroup = true);
  ///< @brief Create a directory (and all its parents as necessary).
  ///
  /// If the argument is a symbolic links, the target directory (tree) is created
  /// if it does not yet exist.
  /// @param relOrAbsPath is either an absolute path or a path relative to
  ///     HOME2L_ROOT and must identify a directory.
  /// @param setHome2lGroup selects to set the directory group ownerships to HOME2L_USER, if possible.
  /// @return success status (on error, a warning is logged).

bool UnlinkTree (const char *relOrAbsPath, const char *skipPattern = NULL);
  ///< @brief Unlink (remove) a directory with all its decendants.
  ///
  /// @param relOrAbsPath is either an absolute path or a path relative to
  ///     HOME2L_ROOT and must identify a directory.
  /// @param skipPattern identifies portions not to be unlinked. Presently, only
  ///     a whitespace-separated list of items of the form "\<name\>" is supported,
  ///     which means that, if `relOrAbsPath` is a directory, its top-level
  ///     directory \<name\> is skipped.
  ///     Further variants may be implemented in the future.
  /// @return success status (on error, a warning is logged).

bool ReadDir (const char *relOrAbsPath, class CKeySet *ret);
  ///< @brief Read a directory into a @ref CKeySet object.
  ///
  /// @param relOrAbsPath is either an absolute path or a path relative to $PWD
  /// @param ret points to the @ref CKeySet object to be filled
  /// @return success status (on error, a warning is logged).
  ///
  /// In case of an error, the contents of 'ret' are unspecified.

/// @}


/// @}  // Basics





// ********************** Logging and Debugging ********************************


/** @defgroup common_logging Logging
 * @brief Macros for logging.
 *
 * @{
 */


#if WITH_DEBUG == 1
extern int envDebug;            ///< Debug level (read-only; may also be mapped to a constant macro).
#else
#define envDebug 0
#endif


typedef void FLogCbMessage (const char *title, const char *msg);
  ///< @brief Callback to display a message box using OS mechanisms.
  /// This is mainly used if the program is aborted using an ERROR... or ABORT... macro
  /// and presently only supported on Android.
typedef void FLogCbToast (const char *msg, bool showLonger);
  ///< @brief Callback to display a short info popup using OS mechanisms.
  /// This is used if for messages printed using an INFO... macro, where the message is
  /// preceeded by "-t- " (short toast) or "-T- " (long toast).
  /// Toasts are presently only supported on Android.


#if !ANDROID
void LogToSyslog (const char *instanceName = NULL);      ///< Redirect logging to syslog from now; instance name must be passed if this is called before EnvInit()
void LogClose ();
bool LoggingToSyslog ();
void LogStack ();         ///< Log a stack trace (requires linker flag '-rdynamic' to print function names)
static inline void LogSetCbMessage (FLogCbMessage *_cbMessage) {}
static inline void LogSetCallbacks (FLogCbMessage *_cbMessage, FLogCbToast *_cbToast) {}
#else
static inline void LogToSyslog () {}
static inline void LogClose () {}
static inline bool LoggingToSyslog () { return true; }
static inline void LogStack () {}
void LogSetCallbacks (FLogCbMessage *_cbMessage, FLogCbToast *_cbToast);
#endif

void LogPara (const char *_logHead, const char* _logFile, int _logLine);    // Helper only; use the following macros instead
void LogPrintf (const char *format, ...);     // Helper only; use the following macros instead


#if WITH_DEBUG == 1
#define DEBUG(LEVEL, MSG) do { if (envDebug >= LEVEL) { LogPara ("DEBUG-" #LEVEL, __FILE__, __LINE__); LogPrintf (MSG); } } while (0)
  ///< @brief Print a debug message.
  /// This is only effective if the tool was compiled with WITH_DEBUG == 1 and the
  /// configuration parameter 'debug' is set to a value > 0.
  ///
  /// Conventions for the debug level (examples):
  /// - 1: Everything which does not flood the log files
  /// - 2: GPIO value changes; Temporary outputs
  /// - 3: Details: Network conversation (Resources), debug output of underlying libs (ALSA, GStreamer, PJSIP, Linphone)
  ///
  /// Messages of level 3 or more must have a topic-related prefix "[<topic>]" to allow later filtering,
#define DEBUGF(LEVEL, FMT) do { if (envDebug >= LEVEL) { LogPara ("DEBUG-" #LEVEL, __FILE__, __LINE__); LogPrintf FMT; } } while (0)
  ///< @brief Print a formatted debug message.
  /// The argument must contain a parantheses with arguments, which will then be
  /// passed to a printf() function variant. For example:
  /// `DEBUGF(("Current value is %i", my_value))`
#else
#define DEBUG(LEVEL, MSG) do {} while (0)
#define DEBUGF(LEVEL, FMT) do {} while (0)
#endif

#define INFO(MSG) do { LogPara ("INFO", __FILE__, __LINE__); LogPrintf (MSG); } while (0)
  ///< @brief Print a debug message to the console.
#define INFOF(FMT) do { LogPara ("INFO", __FILE__, __LINE__); LogPrintf FMT; } while (0)
  ///< @brief Print a formatted info message.

#define WARNING(MSG) do { LogPara ("WARNING", __FILE__, __LINE__); LogPrintf (MSG); } while (0)
  ///< @brief Print a warning message to the console.
#define WARNINGF(FMT) do { LogPara ("WARNING", __FILE__, __LINE__); LogPrintf FMT; } while (0)
  ///< @brief Print a formatted warning message.

#define SECURITY(MSG) do { LogPara ("SECURITY", __FILE__, __LINE__); LogPrintf (MSG); } while (0)
  ///< @brief Print a security-related warning to the console.
  /// These are warnings an adminstrator should regularly check out for, since
  /// they may be caused by an attack.
#define SECURITYF(FMT) do { LogPara ("SECURITY", __FILE__, __LINE__); LogPrintf FMT; } while (0)
  ///< @brief Print a formatted security-related warning.

#define ERROR(MSG) do { LogPara ("ERROR", __FILE__, __LINE__); LogPrintf (MSG); _exit (3); } while (0)
  ///< @brief Print an error message to the console and quit the application.
#define ERRORF(FMT) do { LogPara ("ERROR", __FILE__, __LINE__); LogPrintf FMT; _exit (3); } while (0)
  ///< @brief Print a formatted error message to the console and quit the application.

#define ABORT(MSG) do { LogPara ("ERROR", __FILE__, __LINE__); LogPrintf (MSG); abort (); } while (0)
  ///< @brief Print an error message to the console and abort the application.
  /// Unlike `ERROR`, this uses the `abort()` system call, which may generate
  /// a core dump to be used for debugging.
#define ABORTF(FMT) do { LogPara ("ERROR", __FILE__, __LINE__); LogPrintf FMT; abort (); } while (0)
  ///< @brief Print a formatted error message to the console and abort the application.

#define ASSERT(COND) do { if (!(COND)) { LogPara ("ERROR", __FILE__, __LINE__); LogPrintf ("Assertion failed"); LogStack (); abort (); } } while (0)
  ///< @brief Check a condition and abort the application if not true.
#define ASSERTM(COND,MSG) do { if (!(COND)) { LogPara ("ERROR", __FILE__, __LINE__); LogPrintf ("Assertion failed: %s", MSG); LogStack (); abort (); } } while (0)
  ///< @brief Check a condition and, if not true, abort the application with a message.
#define ASSERTF(COND,FMT) do { if (!(COND)) { LogPara ("ERROR", __FILE__, __LINE__); LogPrintf ("Assertion failed - explanation follows."); LogPrintf FMT; abort (); LogStack (); abort (); } } while (0)
  ///< @brief Check a condition and, if not true, abort the application with a message.

#define ASSERT_WARN(COND) do { if (!(COND)) WARNING("Weak assertion failed"); } while (0)
  ///< @brief Check a condition and emit a warning if not true.




// ***** Compile-time assertion *****


#if __cplusplus >= 201103L
#define STATIC_ASSERT(COND) static_assert (COND, "please check!")
  ///< @brief Check a compile-time condition.
  /// This macro fails to compile with error "negative array size", if the asserted
  /// condition is not true.
#else
#define STATIC_ASSERT(COND) static inline void build_assert_##__LINE__() { (void) sizeof(char[(COND) - 1]); }
#endif


/// @}  // Logging





// *************************** Safe heap operations ****************************


/** @defgroup common_heap Heap
 * @brief Type-safe and null-pointer-aware heap operations.
 *
 * @{
 */


/// @name C-style Allocation ...
/// @{
#define MALLOC(T, N) (T*) malloc (sizeof (T) * (N))
  ///< @brief Allocate an array of `N` objects of type `T` (type-safe)
#define REALLOC(T, P, N) (T*) realloc (P, sizeof (T) * (N))
  ///< @brief Reallocate an array of `N` objects of type `T` (type-safe)
#define SETP(P, X) { if (P) free (P); P = (X); }
  ///< @brief Replace `P` with a new object/array, eventually free'ing the old one.
#define FREEP(P) SETP(P, NULL)
  ///< @brief Free object/array `P` and set `P = NULL`, but only if `P` was `!= NULL`.
/// @}


/// @name C++-style Allocation ...
/// @{
#define SETO(O, X) { if (O) delete O; O = (X); }
  ///< @brief Replace `O` with a new object, eventually deleting the old one.
#define FREEO(O) SETO(O, NULL)
  ///< @brief Free object `O`, but only if `O != NULL`

#define SETA(A, X) { if (A) delete [] A; A = (X); }
  ///< @brief Replace `A` with a new array, eventually deleting the old one.
#define FREEA(A) SETA(A, NULL)
  ///< @brief Free array `A`, but only if `A != NULL`
/// @}


/// @}  // Heap





// *************************** Version Helpers *********************************


/** @defgroup common_version Version
 * @brief Numeric version handling
 *
 * Helpers to convert version strings to numbers and vice versa.
 * Version numbers of type `uint32_t` can be compared arbitrarily in order to check
 * if some tool is newer or older than some reference.
 *
 * @{
 */


static inline uint32_t VersionCompose (uint8_t major, uint8_t minor, uint16_t revision, bool dirty = false) {
  ///< @brief Compose a numeric representation of the version.
  /// @param major is the major version component.
  /// @param minor is the minor version component.
  /// @param revision is the revision version component.
  /// @param dirty is a flag indicating whether the code has been modified locally compared to the version.
  return (((uint32_t) major) << 24) + (((uint32_t) minor) << 16) + (((uint32_t) revision) << 1) + ((uint32_t) dirty);
}
static inline int VersionMajor (uint32_t ver) { return (int) (ver >> 24); }
  ///< @brief Get the major component of the version
static inline int VersionMinor (uint32_t ver) { return (int) ((ver >> 16) & 0xff); }
  ///< @brief Get the minor component of the version
static inline int VersionRevision (uint32_t ver) { return (int) ((ver >> 1) & 0x7fff); }
  ///< @brief Get the revision component of the version
static inline bool VersionDirty (uint32_t ver) { return (ver & 1) == 1; }
  ///< @brief Get the "dirty" component of the version

uint32_t VersionFromStr (const char *str);
  ///< @brief Get a numeric representation of the version string.
  /// On syntax error, a warning is logged and 0 is returned.
const char *VersionToStr (class CString *ret, uint32_t ver);
  ///< @brief Get a string representation of the version.
  ///  This may be shorter than an eventual original string passed to @ref VersionFromStr(),
  ///  i.e. an eventual git ID is missing.

static inline const char *VersionGetOwnAsStr () { return buildVersion; }
  ///< Get my own build version as a string.
uint32_t VersionGetOwn ();
  ///< Get my own build version.


/// @}  // Version





// ********************** Localization and language ****************************


/** @defgroup common_language Language
 * @brief Localization and translation
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
#define _(STR) gettext(STR)       ///< GNU gettext-compliant macro for translating strings
#define gettext_noop(STR) STR
#define N_(STR) gettext_noop(STR) ///< GNU gettext-compliant macro for translating strings


// More helpers...
void LangTranslateNumber (char *str);
  ///< @brief Adapt numeric string by replacing all '.' by the locale's decimal point


/// @}  // Language





// *************************** Strings *****************************************


/** @defgroup common_strings Strings
 * @brief Helper functions and classes for string handling
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
 * @{
 */


#define WHITESPACE " \t\n\r\v"    ///< white space characters according to 'man 3 isspace'


// ***** Misc. helpers *****


class CString *GetThreadTempString ();
  ///< @brief Get the thread-local temporary string (TTS) of the calling thread.
  ///
  /// Sometimes, functions return a string and require the 'CString' object to
  /// be provided by the caller. Examples are the various 'ToStr ()' methods.
  ///
  /// This function 'GetThreadTempString ()' returns a single CString object that
  /// can be used globally. To avoid synchronization issues, there is one
  /// thread-temporary string (TTS) per thread.
  ///
  /// Use it and the @ref TTS macro with care! By convention, it is not allowed
  /// to call any function or method outside of this module (@ref common)
  /// between the first use of @ref TTS and the last use of the returned
  /// 'const char *' string.

#define TTS (GetThreadTempString ())
  ///< @brief Get the thread-local temporary string (TTS) of the calling thread (short version).

const char *StringF (class CString *ret, const char *fmt, ...);
  ///< @brief Return 'ret->Get ()'

bool BoolFromString (const char *str, bool *ret);
  ///< @brief Convert a string to a Boolean value.
  /// On success, '*ret' is set accordingly and 'true' is returned.
  /// On failure, 'false' is returned and '*ret' remains unchanged.
bool ValidBoolFromString (const char *str, bool defaultVal);
  ///< @brief Convert a string to a Boolean value. On failure, 'defaultVal' is returned.
bool IntFromString (const char *str, int *ret, int radix = 0);
  ///< @brief Convert a string to an integer using 'strtol'. On success, '*ret' is set accordingly and 'true' is returned.
  /// On failure, 'false' is returned and '*ret' remains unchanged.
  /// If 'radix == 0', it is auto-detected from the string (16 with a preceeding "0x", 10 otherwise).
int ValidIntFromString (const char *str, int defaultVal, int radix = 0);
  ///< @brief Convert a string to an integer. On failure, 'defaultVal' is returned.
bool UnsignedFromString (const char *str, unsigned *ret, int radix = 0);
  ///< @brief Convert a string to an unsigned integer using 'strtoul'. On success, '*ret' is set accordingly and 'true' is returned.
  /// On failure, 'false' is returned and '*ret' remains unchanged.
  /// If 'radix == 0', it is auto-detected from the string (16 with a preceeding "0x", 10 otherwise).
unsigned ValidUnsignedFromString (const char *str, unsigned defaultVal, int radix = 0);
  ///< @brief Convert a string to an unsigned integer. On failure, 'defaultVal' is returned.
bool FloatFromString (const char *str, float *ret);
  ///< @brief Convert a string to a floating-point value using 'strtof'.
  /// On success, '*ret' is set accordingly and 'true' is returned.
  /// On failure, 'false' is returned and '*ret' remains unchanged.
float ValidFloatFromString (const char *str, float defaultVal);
  ///< @brief Convert a string to a float value. On failure, 'defaultVal' is returned.

void StringStrip (char *str, const char *sepChars = WHITESPACE);
  ///< @brief Remove seperator (whitespace) characters at the beginning and end of the string.
void StringTruncate (char *str, const char *sepChars = WHITESPACE, bool after = false);
  ///< @brief Cut away anything from or after the first occurence of any of the given seperator characters.
void StringSplit (const char *str, int *retArgc, char ***retArgv, int maxArgc = INT_MAX, const char *sepChars = NULL, char **retRef = NULL);
  ///< @brief Split a string into (whitespace-)separated arguments.
  ///
  /// The number of extracted words is written to `*retArgc`, an array of that size is
  /// returned via `***retArgv`.
  /// If `maxArgc-1` strings have been split, the complete remaining string is
  /// returned as argument #`(maxArgc-1)`.
  ///
  /// If `sepChars == NULL`, whitespace characters are used as separators, and multiple of
  /// them count as a single separation. In all other cases, each individual separation
  /// character splits up a new argument, so that empty arguments are possible.
  ///
  /// The resulting (sub-)strings will be placed into a single chunk of heap-allocated
  /// memory. The caller must later (only) free `*retArgv[0]` and `*retArgv` or
  /// call StringSplitFree() on `retArgv`.
  /// `str` remains unchanged and owned by the caller.
  /// If 'retRef' is not NULL, a reference pointer is returned by which the caller
  /// can determine the original location of some argument or character.
  /// *To avoid confusion about memory handling, it is recommended to use the
  /// class `CSplitString` instead of this function.*
  ///
void StringSplitFree (char ***argv);
  ///< @brief Free memory allocated by StringSplit().
  /// @param argv is the argument `regArgv` passed to StringSplit().

bool CharIsWhiteSpace (char c);
  ///< @brief Indicate whether the given character is white space (see WHITSPACE).


/// @name Path handling ...
/// @{
void PathNormalize (char *str);             ///< Treat string as a (full) pathname and normalize it
void PathRemoveTrailingSlashes (char *str); ///< Treat string as a (full) pathname and remove trailing slash(es)
const char *PathLeaf (const char *str);     ///< Get leaf component of a path

const char *GetAbsPath (class CString *ret, const char *relOrAbsPath, const char *defaultPath);
  ///< @brief Get absolute path.
  /// @param ret String to write the result to
  /// @param relOrAbsPath Input path
  /// @param defaultPath Default path
  /// @return Normalized absolute path
  ///
  /// If the input path starts with '/', it is considered an absolute path and
  /// returned unchanged.Otherwise, `defaultPath` is prepended, and the normalized
  /// path is returned.
/// @}


/// @name Transcoding ...
/// @{
const char *ToUtf8 (class CString *ret, const char *iso8859str);
  ///< @brief Transcode an ISO8859-coded string to UTF-8.
  /// Result is valid until next call of this function.
const char *ToIso8859 (class CString *ret, const char *str);
  ///< @brief Transcode an ISO8859-coded string to UTF-8.
  /// Result is valid until next call of this function.
/// @}



// ***** CString *****


/** @brief Dynamically allocated string.
 *
 * This is the main class used in the *Home2L* project for strings of arbitrary
 * length. The strings storage (typically heap) may either be managed by this
 * class, but it may also be managed by the caller in order to avoid memory
 * duplication with constant strings, for example.
 */
class CString {
  public:
    CString () { size = 0; ptr = (char *) emptyStr; }
    CString (const CString& str) { size = 0; Set (str.Get ()); }    ///< Copy constructor
    CString (const char *str, int maxLen = INT_MAX) { size = 0; Set (str, maxLen); }
    ~CString () { if (size) free (ptr); }

    /// @name Initialization...
    ///@{
    void Clear () { SetC (emptyStr); }
    void Set (char c) { Clear (); Insert (0, c); }
    void Set (const char *str, int maxLen = INT_MAX);
      ///< @brief Set (substring) from 'str'.
    void SetF (const char *fmt, ...);
      ///< @brief Set using printf() formatting.
    void SetFV (const char *fmt, va_list ap);
      ///< @brief Set using vprintf() formatting.

    void SetC (const char *_ptr);
      ///< @brief Set without copying or taking ownership of '_ptr' (but "copy-on-write" semantics).
      /// This is more efficient than Set(), but requires '_ptr' to remain valid afterwards.
      /// '_ptr' must be a valid pointer (!= NULL).
    ///@}

    /// @name Ownership optimizations ...
    ///@{
    void SetO (const char *_ptr);
      ///< @brief Set content and take ownership of '_ptr'.
      /// '_ptr' must have been dynamically allocated, since it will be free'd later using 'free ()'.
    char *Disown ();
      ///< @brief Return current string as a dynamic object and clear 'this'.
      ///
      /// The caller is responsible for freeing the pointer later using free().
      ///
      /// *Note:* If the memory of 'ptr' was previously not owned, a dynamic copy will be returned.
      ///       This guarantees, that 'free' can safely be called afterwards, but may be less efficient than
      ///       leaving the ownership with the string object.
    ///@}

    /// @name Static elements ...
    ///@{
    static const char * const emptyStr;       ///< Use this whenever you need an empty string ("")
    ///@}

    /// @name Character encoding (see special not on string encoding in this file) ...
    ///@{
    void SetFromIso8859 (const char *iso8859str);   ///< 'this' will be a (normal) UTF-8 string, the source is expected to be ISO-8859
    bool SetAsIso8859 (const char *str);            ///< 'this' will be ISO-8859-encoded string, the source is expected to be (normal) UTF-8
    ///@}

    /// @name Read access ...
    ///@{
    char *Get () const { return ptr; }    ///< Get the C string. Unless explicitely set by 'SetC', this will never return NULL or an invalid pointer
    int Len () { return strlen (ptr); }   ///< Get the length.
    bool IsEmpty () { return ptr[0] == '\0'; }  ///< Check, if empty.
    ///@}

    /// @name Modifications ...
    ///@{
    void Del (int n0, int dn = INT_MAX);

    void Insert (int n0, int dn, int *retInsPos = NULL);
    void Insert (int n0, char c);
    void Insert (int n0, const char *str, int maxLen = INT_MAX);
    void InsertF (int n0, const char *fmt, ...);
    void InsertFV (int n0, const char *fmt, va_list ap);

    void Append (char c) { Insert (INT_MAX, c); }
    void Append (const char *str, int maxLen = INT_MAX) { Insert (INT_MAX, str, maxLen); }
    void AppendF (const char *fmt, ...);
    void AppendFV (const char *fmt, va_list ap) { InsertFV (INT_MAX, fmt, ap); }
    ///@}

    /// @name Extras ...
    ///@{
    int Compare (const char *str2);

    void Strip (const char *sepChars = WHITESPACE);
    void Truncate (const char *sepChars = WHITESPACE, bool after = false);

    void Split (class CSplitString *args, int maxArgc = INT_MAX, const char *sepChars = WHITESPACE);
    void Split (int *retArgc, char ***retArgv, int maxArgc = INT_MAX, const char *sepChars = WHITESPACE) { StringSplit (ptr, retArgc, retArgv, maxArgc, sepChars); }

    void AppendFByLine (const char *fmt, const char *text);
      ///< @brief Same as SetFByLine(), but appending the result to the current srtring.
    void SetFByLine (const char *fmt, const char *text) { Clear (); AppendFByLine (fmt, text); }
      ///< @brief Split 'text' into lines and reassembles the text by processing each line through sprintf and 'fmt'.
      /// 'fmt' must contain no more than "%s" (or similar) conversion modifier and should contain a trailing '\n'.

    void AppendEscaped (const char *s, const char *dontEscape = NULL);
      ///< @brief Same as 'Set...', but appending the result to the current srtring.
    void SetEscaped (const char *s, const char *dontEscape = NULL) { Clear (); AppendEscaped (s, dontEscape); }
      ///< @brief Set and escape all non-alphanumerical characters by "\...".
      ///
      /// If 's' is empty, "\0" is appended, so that the resulting string is never empty.
      /// Any escape sequence according to the C99 standard may be used.
      /// In addition to the C standard, the following sequences may occur here:
      /// `'\\s' == ' ' (space)`
      ///
      /// By default, the resulting string does not contain any other characters besides
      /// '0'...'9', 'a'...'z', 'A'...'Z' and '\'. Even spaces are not contained.
      ///
      /// The 'dontEscape' parameter is a pragmatical option to improve the human-readability and
      /// memory consumption of the result. If set, characters in 'dontEscape' are not escaped.

    bool AppendUnescaped (const char *s);
      ///< @brief Same as 'Set...', but appending the result to the current srtring.
    bool SetUnescaped (const char *s) { Clear (); return AppendUnescaped (s); }
      ///< @brief Set and revers escapes. On error, 'false' is returned and the string remains unchanged.
      /// If the string has been generated by one of the '...Escaped' methods above, it is
      /// restored precisely. However, any character except '\' are tolerated in the input string
      /// and will be copied into the destination.
    /// @}

    /// @name Path handling ...
    /// These methods do the same as the functions with the same name.
    /// @{
    void PathNormalize ();
    void PathRemoveTrailingSlashes ();
    const char *PathLeaf () { return ::PathLeaf (ptr); }
    void PathGo (const char *where);
    void PathGoUp ();

    /// @name Using CString as a file buffer ...
    /// @{
    bool ReadFile (const char *relOrAbsPath);
      ///< @brief Read a complete text file into a string.
      /// The path may be absolute or relative to HOME2L_ROOT.
    bool AppendFromFile (int fd, const char *name = NULL);
      ///< @brief Read as much as possible from 'fd' and append to buffer.
      /// If an end-of-file is encountered, 'false' is returned. 'fd' is NOT closed automatically.
      /// This method can be used to read a file (or pipe or socket) asynchronously.
      /// The optional argument 'name' is the stream name logged in case of an error/warning.
    bool ReadLine (CString *ret);
      ///< @brief Consume and optionally return the first line from 'this'.
      /// On success, 'true' is returned. If no complete line is available, 'false' is returned
      /// and 'this' is not changed.
      /// In 'this', each line ends with a '\n'. The output ('retLine') does not contain the '\n'.
      /// 'retLine' can be 'NULL', in which case the line is ignored.
    ///@}

    /// @name Operators ...
    /// @{
    operator char * () { return ptr; }

    CString& operator = (const CString &str) { Set (str.Get ()); return *this; }
    CString& operator = (const char *str) { Set (str); return *this; }
    char& operator [] (int n) { return n >= 0 ? ptr [n] : ptr [strlen (ptr) + n]; }
    CString operator + (const char *str);
    CString& operator += (char c) { Append (c); return *this; }
    CString& operator += (const char *str) { Append (str); return *this; }

    bool operator < (const char *str) { return Compare (str) < 0; }
    bool operator > (const char *str) { return Compare (str) > 0; }
    bool operator <= (const char *str) { return Compare (str) <= 0; }
    bool operator >= (const char *str) { return Compare (str) >= 0; }
    bool operator == (const char *str) { return Compare (str) == 0; }
    bool operator != (const char *str) { return Compare (str) != 0; }
    ///@}

    // Interfaces ...
    const char *ToStr (CString *) { return Get (); }   // (internal, for 'CDict')

  protected:
    void SetSize (int _size);
    void MakeWriteable () { if (!size) SetSize (strlen (ptr) + 1); }    // '+1' is just for the case that string was empty

    char *ptr;
    int size;   // number of allocated bytes (including '\0`);
                //'size' == 0: nothing allocated in heap ('ptr' may still point to some constant string

};



// ***** CSplitString *****


/** @brief Factory class to split a string into substrings.
 *
 * Objects of this class are usually declared locally whereever
 * a string needs to be split into substrings, for example, as in
 * the argument analysis of a shell.
 */
class CSplitString {
  public:
    CSplitString () { argc = 0; argv = NULL; ref = NULL; }
    CSplitString (const char *str, int maxArgc = INT_MAX, const char *sepChars = NULL) { StringSplit (str, &argc, &argv, maxArgc, sepChars, &ref); }
      ///< @brief Initialize, set and split `str` (see `Set ()`).
    ~CSplitString () { Clear (); }

    void Clear ();
      ///< @brief Clear the object (only necessary if another string is to be split)

    void Set (const char *str, int maxArgc = INT_MAX, const char *sepChars = NULL) { Clear (); StringSplit (str, &argc, &argv, maxArgc, sepChars, &ref); }
      ///< @brief Set and split a string.
      /// @param str The string to split
      /// @param maxArgc Maximum number of strings to return. If more substrings are
      ///      contained in the input the last ones are returned unsplit as a whole.
      /// @param sepChars Set of characters treated as separators
      ///

    int Entries () const { return argc; }     ///< Get number of string after splitting
    const char *Get (int idx) const { return argv[idx]; }       ///< Get one string
    const char *operator [] (int n) const { return Get (n); }   ///< Get one string

    int GetOffset (int argNo) { return (argv && ref) ? argv[argNo] - ref : 0; }
      ///< @brief Get offset of argument number `arg` in the original string.
    int GetOffset (const char *p) { return ref ? (p - ref) : 0; }
      ///< @brief Get offset of some character in some string in the original string.
    int GetIdx (int pos);
      ///< @brief Get the argument number of character number `pos` of the original string.
      /// On error, -1 is returned, unless 'pos' points behind some existing
      /// argument, in which this (or the last) argument is returned.

  protected:
    int argc;
    char **argv, *ref;
};



// ***** Regexp *****


/** @brief Factory class for regular expression matching.
 *
 * This is basically a wrapper for regexp(3).
 */
class CRegex {
  public:
    CRegex () { reValid = false; lastError = 0; }
    ~CRegex () { if (reValid) regfree (&re); }

    bool SetPattern (const char *pattern, int cflags = REG_EXTENDED);
      ///< @brief Set and compile a search pattern.
      /// @param pattern The pattern (or NULL to not match anything)
      /// @param cflags Bit-wise OR of the following flags (see `regexp(3)`):
      ///   `REG_EXTENDED`:  Use POSIX Extended Regular Expression syntax
      ///   `REG_ICASE`:     Ignore case
      ///   `REG_NOSUB`:     Do  not  report  position  of matches
      ///   `REG_NEWLINE`:   Match-any-character operators don't match a newline
      /// @return 'true' on success, 'false' on failure
    bool Match (const char *s, int eflags = 0, size_t maxMatches = 0, regmatch_t *retMatchList = NULL);
      ///< @brief Perform a regular expression search.
      /// @param s The string to be analysed
      /// @param eflags Bit-wise OR of the following flags (see `regexp(3)`):
      ///   `REG_NOTBOL`: The match-beginning-of-line operator always fails to match
      ///   `REG_NOTEOL`: The match-end-of-line operator always fails
      /// @param maxMatches At most this many matches are returned
      /// @param retMatchList If not `NULL`, the list of matches is written into `*retMatchList` (see `regexp(3)`)
      /// @return `true` if and only if the regular expression matched. If no pattern has been set before,
      ///   `false` is returned.

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





// ********************** Container Classes ************************************


/** @defgroup common_containers Containers
 * @brief Lightweight container classes for lists, dictionaries and key sets.
 *
 * @{
 */


#define CONT_ALIGNMENT 4
#define CONT_ALIGN(X) (((X) + CONT_ALIGNMENT - 1) & ~(CONT_ALIGNMENT - 1))



// ***** Stringification of container values ****


// Template for all classes, which must have a 'ToStr()' method ...
template <typename T> const char *ToStr (CString *ret, T *obj) { return obj->ToStr (ret); }

// Specializations for non-class types (more specializations may be added here as required) ...
template <> const char *ToStr<int> (CString *ret, int *obj);



// ***** CList... *****


/** @brief Raw list (base class for other list variants).
 *
 * This class stores typeless value data and is the base class for the other
 * list classes. It should not be used directly.
 */
class CListRaw {
  public:
    CListRaw (int _recSize);
    virtual ~CListRaw () { Clear (); }

    /// @name Read access ...
    /// @{
    int Entries () const { return entries; }
      ///< @brief Get number of entries.
    /// @}

    /// @name Write access ...
    /// @{
    void Clear () { SetEntries (0); }
      ///< @brief Clear the list.
    void Del (int idx);
      ///< @brief Delete entry; complexity is O(n).
    /// @}

    /// @name Debugging ...
    /// @{
    virtual void Dump (const char *name);   ///< Dump contents by means of `INFO` logs (for debugging)
    /// @}

  protected:

    // Methods to be overloaded by derived classes ...
    virtual void RecInit (void *p);
      // Initialize record in previously uninitialized memory.
      // This method is also used to wipe entries that have been copied and are no longer owned.
    virtual void RecClear (void *p);
      // Clear (initialized) record such that no destructor call is necessary anymore.

    virtual void ValueInit (void *p) {}
      // Initialize value data in previously uninitialized memory
    virtual void ValueClear (void *p) {}
      // Clear (initialized) record such that no destructor call is necessary anymore
    virtual void ValueSet (void *p, void *orig) {}
      // Copy or move 'orig' to 'p' properly. Both are initialized objects.
      // The derived class must specify whether 'orig' is copied or moved (= disowned) by this
      // and document the behavior for
    virtual const char *ValueToStr (CString *ret, void *p);
                                          // return readable string for the 'Dump' method

    // Helpers for derived classes ...
    void SetEntries (int _entries);

    uint8_t *GetRecAdr (int idx) const { return idx < 0 ? NULL : data + recSize * idx; }
      // Get memory address of whole record (including eventual tag)
    uint8_t *GetValueAdr (int idx) const { return idx < 0 ? NULL : data + recSize * idx + tagSize; }
      // Get memory address of the value part of the record (for '...Compact' classes)
    void *GetValuePtr (int idx) const { return idx < 0 ? NULL : * (void **) (GetValueAdr (idx)); }
      // (for non-compact classes) Take the value part inside the record as a pointer to the real value and dereference it

    void Copy (int idxDst, int idxSrc);   // byte-copy entry
    void Swap (void *rec0, void *rec1);   // byte-swap two entries (potentially from different objects)

    void SetRaw (int idx, void *value);
      // Set a new entry. The entry must exist and 'idx' be valid.
      // 'value' may either be copied or moved, see comment to 'ValueSet()'.
    void InsertRaw (int idx, void *value);
      // Insert a new entry in position 'idx'.
      // 'value' may either be copied or moved, see comment to 'ValueSet()'.
    void *DisownRaw (int idx);
      // (for non-compact classes) Take the value part inside the record as a pointer to the real value,
      // return it and set it to NULL in the record.

    // Data ...
    uint8_t *data;
    int recSize, tagSize, entries, allocEntries;
      // 'tagSize' is an offset inside a record. It can be used by derived classes,
      //  e.g. by 'CDictRaw' to reserve space for keys.
};


/** @brief Dynamic list.
 *
 * A list stores an arbitrary number typed objects retaining the order of objects.
 * Internally, the entries are stored in an array, which is dynamically resized as adequate.
 *
 * The values are dynamically allocated objects, to which pointers are stored in the array.
 * All 'Set...' methods require dynamic objects to be passed and take over their ownerships.
 * On element deletion, these objects are deleted. It is legal to pass NULL to 'Set...' methods.
 */
template <typename T> class CList: public CListRaw {
  public:
    CList (): CListRaw (sizeof (T *)) {}

    /// @name Read access ...
    /// @{
    T *Get (int idx) { return (T *) GetValuePtr (idx); }
    T *operator [] (int idx) { return Get (idx); }
    /// @}

    /// @name Write access ...
    /// For any operations passing a new value object, the list takes over the ownership:
    /// The pointer passed as 'value' remains valid until the entry is deleted from the list.
    /// Then it will be free'd using 'delete'.
    /// @{
    void Set (int idx, T *value) { SetRaw (idx, value); }
      ///< @brief Set (replace) a value.
      /// The entry must exist and 'idx' be valid. Complexity is O(1).
    void Insert (int idx, T *value) { InsertRaw (idx, value); }
      ///< @brief Insert a new value.
      /// Complexity is O(n).
    void Append (T *value) { InsertRaw (INT_MAX, value); }
      ///< @brief Append a new value.
      /// Complexity is O(1) if no resizing is necessary, else O(1).
    T *Disown (int idx) { return (T *) DisownRaw (idx); }
      ///< @brief Disown a value and clear it in the array.
    /// @}

  protected:
    virtual void ValueInit (void *p) { * (T **) p = NULL; }
    virtual void ValueClear (void *p) { if (* (T**) p) { delete * (T **) p; * (T **) p = NULL; } }
    virtual void ValueSet (void *p, void *orig) { ValueClear (p); * (T**) p = (T*) orig; }
    virtual const char *ValueToStr (CString *ret, void *p) { return ::ToStr<T> (ret, * (T**) p); }
};


/** @brief Compact list.
 *
 * This class is similar to @ref CList, but stores the value objects in one big array
 * and may therefore be more efficient in some cases. In particular, the memory layout
 * is more compact, and read accesses as well as changes to individual entries are faster.
 * On the other hand, adding or removing entries may be more time consuming for
 * large objects.
 *
 * The type 'T' must fulfill the following properties:
 * 1. The default constructor must not allocate any dynamic data.
 *    (A sample object will be copied using 'memcpy' to construct other objects.)
 * 2. It must be location-invariant, since it may be moved in memory.
 *    In particular, if elements are inserted or deleted, no pointers
 *    to dictionary elements must exist anywhere.
 * 3. The '=' operator must be defined appropriately.
 * 4. The '=' operator must free any dynamic data if the sample object is assigned.
 */
template <typename T> class CListCompact: public CListRaw {
  public:
    CListCompact (): CListRaw (sizeof (T)) {}
    virtual ~CListCompact () { Clear (); }

    /// @name Read access ...
    /// @{
    T *Get (int idx) { return (T *) GetValueAdr (idx); }
    T *operator [] (int idx) { return Get (idx); }
    /// @}

    /// @name Write access ...
    /// For any operations passing a new value object, the object is copied.
    /// @{
    void Set (int idx, T *value) { SetRaw (idx, value); }
      ///< @brief Set (replace) a value.
      /// The entry must exist and 'idx' be valid. Complexity is O(1).
    void Insert (int idx, T *value) { InsertRaw (idx, value); }
      ///< @brief Insert a new value.
      /// Complexity is O(n).
    void Append (T *value) { InsertRaw (INT_MAX, value); }
      ///< @brief Append a new value.
      /// Complexity is O(1) if no resizing is necessary, else O(1).
    /// @}

  protected:
    virtual void ValueInit (void *p) { memcpy (p, &emptyObj, sizeof (emptyObj)); }
    virtual void ValueClear (void *p) { ValueSet (p, &emptyObj); }
    virtual void ValueSet (void *p, void *orig) { * (T *) p = * (T *) orig; }
    virtual const char *ValueToStr (CString *ret, void *p) { return ::ToStr<T> (ret, (T*) p); }

    T emptyObj;   // Template for an initialized object
};


/** @brief Reference list.
 *
 * This class is similar to @ref CList, but stores references to named objects
 * without taking over ownership.
 */
template <typename T> class CListRef: public CList<T> {
  protected:
    virtual void ValueClear (void *p) { * (T **) p = NULL; }
    virtual void ValueSet (void *p, void *orig) { * (T**) p = (T*) orig; }
};



// ***** CDict... *****


#define DICT_KEYSIZE CONT_ALIGN((int) sizeof (CString))


/** @brief Raw dictionary (base class for other dictionary variants).
 *
 * This class stores typeless value data and is the base class for the other
 * dirctionary classes. It should not be used directly.
 */
class CDictRaw: public CListRaw {
  public:
    CDictRaw (int _valueSize): CListRaw (_valueSize + DICT_KEYSIZE) { tagSize = DICT_KEYSIZE; }

    /// @name Read access ...
    /// @{
    void Del (int idx) { CListRaw::Del (idx); }
      ///< @brief Delete entry; complexity is O(n).
      ///
      /// Entries with indices < 'idx' remain unchanged, all other entries may be modified.
      /// This is important to consider if, for example, multiple entries are to be deleted in a loop.
      /// Such a loop is legal if it traverses the indices in reverse order.
    void Del (const char *key) { Del (Find (key)); }
      ///< @brief Delete entry by key; complexity is O(n).
    /// @}

    /// @name Write access ...
    /// @{
    const char *GetKey (int idx) const { return ((CString *) GetRecAdr (idx))->Get (); }
      ///< @brief Get key by index.
    int Find (const char *key, int *retInsIdx = NULL);
      ///< @brief Make binary search and return index of entry found or -1 if the key does not exist.
      /// If `retInsIdx != NULL`, the index to insert a new item is return in `*retInsIdx`.
    void PrefixSearch (const char *key, int *retIdx0, int *retIdx1);
      ///< @brief Determine all elements with keys starting with `key`.
      /// @param key Key prefix
      /// @param retIdx0 Pointer to first return value, index of first matching entry.
      /// @param retIdx1 Pointer to second return value, index behind last matching entry.
    /// @}

    /// @name Debugging ...
    /// @{
    virtual void Dump (const char *name);
    /// @}

  protected:
    virtual void RecInit (void *p);
    virtual void RecClear (void *p);

    void SetRaw (int idx, void *value) { CListRaw::SetRaw (idx, value); }
    int SetRaw (const char *key, void *value);     // add or replace the keyed entry; returns new index

    void MergeRaw (CDictRaw *dict2);               // merge 'dict2' into this one and clear 'dict2'
};


/** @brief Dictionary.
 *
 * A dictionary stores arbitrary (key, value) pairs. The key is always of type
 * `CString` / `const char *`. Internally, the dictionary is stored in an array
 * containing the (key, value) pairs, which is kept ordered by the keys.
 *
 * The values are dynamically allocated objects, to which pointers are stored in the array.
 * All 'Set...' methods require dynamic objects to be passed and take over their ownerships.
 * On element deletion, these objects are deleted. It is legal to pass NULL to 'Set...' methods.
 */
template <typename T> class CDict: public CDictRaw {
  public:
    CDict (): CDictRaw (sizeof (T *)) {}

    /// @name Read access ...
    /// @{
    T *Get (int idx) { return (T *) GetValuePtr (idx); }
    T *Get (const char *key) { return Get (Find (key)); }
    T *operator [] (int idx) { return Get (idx); }
    T *operator [] (const char *key) { return Get (key); }
    /// @}

    /// @name Write access ...
    /// For any operations passing a new value object, the dictionary takes over
    /// the ownership: The pointer passed as 'value' remains valid until the entry
    /// is deleted from the list. Then it will be free'd using 'delete'.
    /// @{
    int Set (const char *key, T *value) { return SetRaw (key, value); }
      ///< @brief Add or replace the keyed entry. Complexity is O(n).
      /// @return Index of new entry
    void SetValue (int idx, T *value) { SetRaw (idx, value); }
      ///< @brief Set (replace) a value.
      /// @param idx is the index.
      /// @param value must be a dynamically allocated object or 'NULL'.
      ///        The dictionary will take over ownership and call 'delete' on it later.
      /// The entry must exist and 'idx' be valid. Complexity is O(1).
    T *DisownValue (int idx) { return (T *) DisownRaw (idx); }
      ///< @brief Disown a value and clear it in the dictionary.
    T *DisownValue (const char *key) { return (T *) DisownRaw (Find (key)); }
      ///< @brief Disown a value and clear it in the dictionary.

    void Merge (CDict<T> *dict2) { MergeRaw (dict2); }
      ///< @brief Merge another dictionary into this one.
      /// Complexity is O(n_this + n_dict2). 'dict2' will be empty afterwards.
      ///
      /// The recommended way to add a bunch of new entries to a dictionary is to
      /// first add them to a new dictionary and then use this method to merge
      /// them into 'this'.
    /// @}

  protected:
    virtual void ValueInit (void *p) { * (T **) p = NULL; }
    virtual void ValueClear (void *p) { if (* (T**) p) { delete * (T **) p; * (T **) p = NULL; } }
    virtual void ValueSet (void *p, void *orig) { ValueClear (p); * (T**) p = (T*) orig; }
    virtual const char *ValueToStr (CString *ret, void *p) { return ::ToStr<T> (ret, * (T**) p); }
};


/** @brief Compact dictionary.
 *
 * This class is similar to @ref CDict, but stores the value objects in one big array and may
 * therefore be more efficient in some cases. Also, The type 'T' must fullfill certain properties.
 *
 * See @ref CListCompact for details.
 */
template <typename T> class CDictCompact: public CDictRaw {
  public:
    CDictCompact (): CDictRaw (sizeof (T)) {}

    /// @name Read access ...
    /// @{
    T *Get (int idx) { return (T *) GetValueAdr (idx); }
    T *Get (const char *key) { return Get (Find (key)); }
    T *operator [] (int idx) { return Get (idx); }
    T *operator [] (const char *key) { return Get (key); }
    /// @}

    /// @name Write access ...
    /// @{
    int Set (const char *key, T *value) { return SetRaw (key, value); }
      ///< @brief Add or replace the keyed entry. Complexity is O(n).
      /// @return Index of new entry
    void SetValue (int idx, T *value) { SetRaw (idx, value); }
      ///< @brief Set (replace) a value.
      /// The entry must exist and 'idx' be valid. Complexity is O(1).
    void Merge (CDictCompact<T> *dict2) { MergeRaw (dict2); }
      ///< @brief Merge another map into this one.
      /// Complexity is O(n_this + n_dict2). 'dict2' will be empty afterwards.
      ///
      /// The recommended way to add a bunch of new entries to a dictionary is to
      /// first add them to a new dictionary and then use this method to merge
      /// them into 'this'.
    /// @}

  protected:
    virtual void ValueInit (void *p) { memcpy (p, (void *) &emptyObj, sizeof (emptyObj)); }
    virtual void ValueClear (void *p) { ValueSet (p, (void *) &emptyObj); }
    virtual void ValueSet (void *p, void *orig) { * (T *) p = * (T *) orig; }
    virtual const char *ValueToStr (CString *ret, void *p) { return ::ToStr<T> (ret, (T*) p); }

    T emptyObj;   // Template for an initialized object
};


/** @brief Dictionary of references.
 *
 * This class is similar to @ref CDict, but stores references to named objects
 * without taking over ownership.
 */
template <typename T> class CDictRef: public CDict<T> {
  protected:
    virtual void ValueClear (void *p) { * (T **) p = NULL; }
    virtual void ValueSet (void *p, void *orig) { * (T**) p = (T*) orig; }
};



// ***** CKeySet *****

/** @brief Set of strings (keys).
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
    void Merge (CKeySet *set2) { MergeRaw (set2); }
      ///< @brief Merge another map into this one.
      /// Complexity is O(n_this + n_set2). 'set2' will be empty afterwards.
      ///
      /// The recommended way to add a bunch of new entries to a dictionary is to
      /// first add them to a new dictionary and then use this method to merge
      /// them into 'this'.
    /// @}

    /// @name Debugging ...
    /// @{
    virtual void Dump (const char *name);
    /// @}
};


/// @}  // Containers





// *************************** Date & Time *************************************


/** @defgroup common_datetime Date and Time
 * @brief Various definitions related to date and time handling.
 *
 * Times are always represented by the type @ref TTicks and stored in integer units
 * of milliseconds.
 *
 * Depending on the context, the *Home2L* suite uses two different clocks, as
 * also supported by the Linux kernel, namely the real time and a monotonic time.
 *
 * To distinguish between different clocks or relative times, the following terminology
 * is used:
 *
 * 1. An **absolute** time refers to the number of of milliseconds since the
 *    Epoch (1970-01-01-000000 UTC). This clock is used for calendar or
 *    alarm clock applications, where the absolute date and time matters.
 *    The absolute clock is eventually synchronized via network with time servers
 *    to be kept accurate.
 *
 * 2. A **monotonic** time is relative to some reference time, usually the start
 *    of the application. It can never move backwards (as is possible with the
 *    absolute clock after network synchronization) and does not make quick changes in speed.
 *    The monotonic clock is used for @ref common_timers.
 *
 * 3. A **relative** is either relative to the current time or to some contexct-specific
 *    base time.
 *
 * Besides the basic type `TTicks`, this group contains some helper types
 * `TDate` and `TTime` for calendar time handling.
 * @{
 */


/// @name Ticks ...
/// @{

typedef int64_t TTicks;
  ///< @brief Time value (relative, absolute, or monotonic).

#define ASAP   ((TTicks) 0)
  ///< @brief Special value that may represent "as soon as possible" for absolute or monotonic times.
#define NEVER  ((TTicks) INT64_MIN)
  ///< @brief Special value that may represent "never" for absolute or monotonic times.
  /// Other application-specific special values may be defined as (NEVER + 1), (NEVER + 2), ... .
  /// The actual value is and will always be < 0.


TTicks TicksNow ();
  ///< @brief Get the current absolute time in milliseconds.
  /// This function never returns 0 or a negative value.
TTicks TicksNowMonotonic ();
  ///< @brief Get the current monotonic time in milliseconds.
  /// This function never returns 0 or a negative value.

TTicks TicksAbsFromMonotic (TTicks tm);
  ///< @brief Get the approximated absolute (real) time for a monotonic time.
  /// Arguments <= 0 are returned unmodified.
TTicks TicksMonotonicFromAbs (TTicks ta);
  ///< @brief Get the approximated monotonic time for an absolute time .
  /// Arguments <= 0 are returned unmodified.

const char *TicksAbsToString (CString *ret, TTicks ticks, int fracDigits = INT_MAX, bool precise = false);
  ///< @brief Get a string of the form "YYYY-MM-DD-hhmmss[.<millis>]" in local time.
  /// @param ret Return string
  /// @param ticks Ticks to be converted, must be an absolute time
  /// @param fracDigits Number of fractional digits. If set to -1, the seconds are skipped, too.
  ///             If set to 'INT_MAX', the number of (fractional) seconds digits is determined
  ///             automatically (trailing 0s may be skipped).
  /// @param precise If 'true', a string of the form "`t<n>`" is returned, where
  ///             `<n>` is the decimal number milliseconds since the Epoch.
const char *TicksRelToString (CString *ret, TTicks ticks);
  ///< @brief Get a string of the form "<integer>[<unit>]" for a relative time.
  /// @param ret Return string
  /// @param ticks Ticks to be converted
  ///
  /// The function guesses an appropriate time unit, e.g. seconds ('s') or minutes ('m')
  /// and outputs a string that is precise and at best effort human-readable.
  /// This implies that the original value was a multiple of the resulting unit.
  /// See TicksFromString () for a list of possible units.

bool TicksFromString (const char *str, TTicks *ret, bool absolute);
  ///< @brief Convert a string to a relative or absolute ticks value.
  ///
  /// The following formats are accepted:
  ///
  ///  - `YYYY-MM-DD[-hhmm[ss[.<millis>]]]` : readable time, interpreted as local time (only absolute).
  ///
  ///  - `t<unsigned integer>`              : absolute time in milliseconds since the Epoch (only absolute).
  ///
  ///  - `<integer>[<unit>]`                : relative time in milliseconds (or some other unit with `<unit>`);
  ///                                         Possible units are: seconds ('s'), minutes ('m'), hours ('h'), days ('d'), and weeks ('w');
  ///                                         If 'absolute == true': time is interpreted as relative from now.
  ///
  ///  - `hh:mm[:ss[.<millis>]]`            : `hh` may be > 23 to specify a time in the coming days (*Beware of potential race conditions when using this close to midnight!*);
  ///                                         If 'absolute == true': time relative to 0:00 today.
  ///
  /// If 'absolute' is set, an absolute time is returned in any case.
  /// Otherwise, the relative number of ticks is returned.
  ///
  /// **Note:** For code readability, this function should not be called directly.
  ///   Instead, TicksAbsFromString() or TicksRelFromString() should be used.
static inline bool TicksAbsFromString (const char *str, TTicks *ret) { return TicksFromString (str, ret, true); }
  ///< @brief Convert a string to an absolute ticks value. See TicksFromString() for supported string formats.
static inline bool TicksRelFromString (const char *str, TTicks *ret) { return TicksFromString (str, ret, false); }
  ///< @brief Convert a string to a relative ticks value. See TicksFromString() for supported string formats.

void TicksToStructTimeval (TTicks t, struct timeval *retTv);

#define TICKS_FROM_SECONDS(S) (((TTicks) (S)) * 1000)         ///< Convert seconds (integer!) to ticks
#define SECONDS_FROM_TICKS(T) (((TTicks) (T) + 500) / 1000)   ///< Convert ticks to seconds
#define TICKS_FROM_MILLIS(MS) ((TTicks) (MS))                 ///< Convert milliseconds (integer!) to ticks
#define MILLIS_FROM_TICKS(T)  ((int) (T))                     ///< Convert ticks to milliseconds

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
static inline void DateTimeNow (TDate *d, TTime *t) { TicksToDateTime (TicksNow (), d, t, NULL); }
  ///< @brief Retrieve current date and time (both pointers may be NULL)
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


/** @defgroup common_timers Timers
 * @brief Scheduling timed and/or repeated operations.
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
  ///< @brief Function type for timer callbacks
  /// @param timer The calling timer.
  /// @param data User data as passed to the @ref CTimer object.


/** @brief Timer class.
 */
class CTimer {
  public:
    CTimer ();
    CTimer (FTimerCallback *_func, void *_data = NULL, void *_creator = NULL) { Set (_func, _data, _creator); }
      ///< @brief Construct with a callback function.
    CTimer (TTicks _time, TTicks _interval = 0, FTimerCallback *_func = NULL, void *_data = NULL, void *_creator = NULL) { Set (_time, _interval, _func, _data, _creator); }
      ///< @brief Construct with a time/interval specification and eventually a callback function.
    virtual ~CTimer () { Clear (); }

    void Set (FTimerCallback *_func, void *_data = NULL, void *_creator = NULL);
      ///< @brief Setup timer without (re-)scheduling it.
    void Set (TTicks _time, TTicks _interval = 0, FTimerCallback *_func = NULL, void *_data = NULL, void *_creator = NULL);
      ///< @brief Setup and (re-)schedule timer.
      /// @param _time Monotonic time of the next invocation. A value of 0 means "now".
      ///             A value < 0 means "in `-_time` milliseconds from now".
      /// @param _interval Repeat interval. A value of <= 0 inserts a one-shot timer.
      ///             This number should be a power of 2 (see note above).
      /// @param _func Callback function to be executed at the given times.
      /// @param _data Data to be passed as the `data` argument to the callback function.
      /// @param _creator A reference to the creator. If set (`!= NULL`), the class takes over the ownership.
      ///             of this object. In consequence, the caller does not need to keep a reference to this object,
      ///             but can eventually remove all of them using `DelByCreator()`.

    void Reschedule (TTicks _time, TTicks _interval = 0);
      ///< @brief Change a timer (like 'Set'), but leave function and creator unchanged.
    void Clear ();                              ///< Remove timer from the event list.
    static void DelByCreator (void *_creator);  ///< Remove all timers created by `_creator` from the event list.
    void *GetCreator () { return creator; }

    bool Pending () { return isLinked; }
      ///< @brief Indicate whether the timer is pending and may be executed in the future.
      ///
      /// **Note:** Be careful with potential race conditions. A value of 'false' can be safely be
      ///   used to draw conclusions, given that only the calling thread may call 'Set' & friends.
      ///   A value of 'true' may change to 'false' at any time.

    // Timer callback...
    virtual void OnTime () { if (func) func (this, data); }
      ///< @brief [T:timer] Virtual function called when the timer triggers.

  protected:
    friend bool TimerIterate ();
    friend TTicks TimerGetDelay ();
    friend void TimerRun ();
    friend void *TimerThreadRoutine (void *);

    static bool ClassIterateAL ();
    static TTicks GetDelayTimeAL ();

    void InsertAL ();
    void UnlinkAL ();

    static CTimer *first;   // for chained list
    CTimer *next;           // for chained list
    bool isLinked;          // indicates, whether element is in the linked list

    TTicks nextTicks, interval;
    void *creator;        // this object is managed externally, the caller has a reference to it and must remove it

    FTimerCallback *func;
    void *data;
};



/// @name Control functions (for the main program) ...
/// @{
bool TimerIterate ();
  ///< @brief Returns false if nothing was done.
TTicks TimerGetDelay ();
  ///< @brief Returns number of milliseconds until next call to @ref TimerIterate() is necessary, or -1 if no timer is pending.

void TimerStart ();
  ///< @brief Start @ref TimerRun() in a background thread, which is to be stopped using @ref TimerStop().
  ///  All timer functions are then run from the same, namely that background thread.
int TimerRun (bool catchSignals = true);
  ///< @brief Perform an endless loop calling timers until @ref TimerStop() is called.
  ///  @param catchSignals controls whether the signals SIGTERM ("exit nicely", default of the kill(1) command)
  ///    and SIGINT (Ctrl-C) are caught temporarly and lead to an immediate return of this function.
  ///  @return the signal number if the function has been stopped by a caught signal, else 0.
void TimerStop ();
  ///< @brief Request @ref TimerRun() to return at next occasion (can be called from any thread).
  ///  If a dedicated timer thread was started, this is joined, too.
/// @}


/// @}  // Timers





// ********************** Threading and Synchronization ************************


/** @defgroup common_threads Threads and Synchronization
 * @brief Wrapping classes related to threads and synchronization.
 *
 * @{
 */


typedef void *FThreadRoutine (void *data);
  ///< @brief Function type for thread routines
  /// @param data User data as passed to the @ref CThread object.


/** @brief Class to wrap (POSIX) threads.
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
      ///< @brief Run the supplied function `_routine` in the background passing `_data` as an argument.
    void Start ();  ///< @brief Run the virtual method Run() in the background.
    void *Join ();  ///< @brief Join with the background thread.

    bool IsRunning () { return running;  } ///< @brief (only for main thread)

  protected:
    friend void *CThreadRoutine (void *);

    virtual void *Run () { return NULL; } ///< @brief Main thread routine (to be overloaded)

    bool running;
    pthread_t thread;
};


/** @brief Class to wrap (POSIX) mutex objects.
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


/** @brief Class to wrap (POSIX) condition variables.
 */
class CCond {
  public:
    CCond ();
    ~CCond ();

    void Wait (CMutex *mutex);
      ///< @brief Wait until woken up.
      /// The supplied `mutex` must be locked and will be atomically unlocked while waiting.
    TTicks Wait (CMutex *mutex, TTicks maxTime);
      ///< @brief Wait until woken up or a time-out occured.
      /// @param mutex Mutex to unlock while waiting.
      /// @param maxTime Maximum wait time in milliseconds.
      ///
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
    void Signal ();           ///< @brief Wakeup ONE waiting thread.
    void Broadcast ();        ///< @brief Wakeup ALL waiting threads.

  protected:
    pthread_cond_t cond;
};


/** @brief Class allowing to sleep until one out of multiple i/o operations becomes possible.
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
      ///< @brief Enable sending commands by PutCmd().
      /// To be called immediately after initialization and only if the command feature is used.
    /// @}

    /// @name For the sleeping thread: Preparation...
    ///
    /// These methods must be called before each invocation of Sleep().
    /// @{
    void Prepare ();
      ///< @brief Prepare for sleeping, clear all file descriptors.
    void AddReadable (int fd);
      ///< @brief Add file descriptor `fd` to monitor for readability, descriptors < 0 are silently ignored.
    void AddWritable (int fd);
      ///< @brief Add file descriptor `fd` to monitor for writability, descriptors < 0 are silently ignored.
    /// @}

    /// @name For the sleeping thread: Sleep...
    /// @{
    void Sleep (TTicks maxTime = -1);
      ///< @brief Sleep until one file descriptor is readable/writable or a new command was received.
      /// @param maxTime Maximum waiting time; -1: wait forever, 0 => do not wait
      ///
      /// A non-blocking Sleep() (`maxTime` == 0) updates the internal readability/writability flags.
    /// @}

    /// @name For the sleeping thread: Afterwards...
    /// @{
    bool IsReadable (int fd);   ///< @brief Check if file descriptor is readable (flags are only updated in Sleep() ).
    bool IsWritable (int fd);   ///< @brief Check if file descriptor is writable (flags are only updated in Sleep() ).
    bool GetCmd (void *retCmdRec);
      ///< @brief Get and consume next command (non-blocking).
      /// @return 'false' if no command is available.
    /// @}

    /// @name For other threads...
    /// @{
    void PutCmd (const void *cmdRec, TTicks t = 0, TTicks _interval = 0);
      ///< @brief Send message to sleeper.
      ///
      /// `cmd == NULL` represents "no command" and will be ignored by receiver.
      /// The command can optionally be scheduled for a certain time or even be repeated.
      /// `t` is an absolute time, `t == 0` represents "now".
    void ClearTimedCmds () { CTimer::DelByCreator (this); }
      ///< @brief Clear any scheduled commands which have not yet been put into the queue.
      /// **Note:** Respective commands may still be in the queue and returned by GetCmd() after calling this!
    /// @}

  protected:
    fd_set fdSetRead, fdSetWrite;
    int maxFd, cmdRecSize;
    int selfPipe[2];
};



void Sleep (TTicks mSecs);
  ///< @brief Suspend for `mSecs` milliseconds using `nanosleep()`.



/// @}  // Threads







// *************************** CShell and variants *****************************


/** @defgroup common_shells Shells
 * @brief Shell classes to run and iteract with external processes.
 *
 * @{
 */



// ***** Abstract base class and functions *****


/** @brief Abstract base class for shells that can execute system commands.
 *
 * **Note:** All methods (for one object) must be called from the same thread.
 */
class CShell {
  public:
    CShell () { exitCode = -1; }
    virtual ~CShell () {}

    /// @name Execution control...
    /// @{
    virtual bool Start (const char *cmd, bool readStdErr = false) = 0;
      ///< @brief Start command.
      /// @param cmd The exact (and arbitrary) shell command. On Android, relative
      ///    paths are prepended with HOME2L\_ROOT.
      /// @param readStdErr decides whether STDERR is redirected to STDIN and
      ///    also returned via ReadLine() and friends. By default, it is left undirected.
      ///
      /// If a previous command is still running, Wait() is executed before.
      /// On error, a warning is printed and 'false' is returned.
      /// Only one command can be executed at a time. It can be understood that the Start() method includes
      /// an implicit Mutex-Lock operation, and the lock has been obtained (only) if 'true' is returned.
      /// The corresponding Mutex-Unlock operation is implied by calling Wait().
      ///
    virtual bool StartRestricted (const char *name, const char *args = NULL);
      ///< @brief Same as 'Start', but fetch the command from the environment.
      ///
      /// This way, only commands explicitly defined in the environment settings are allowed, which
      /// is a security improvment.
      /// The actual command is taken from the environment variable `rc.cmd.<name>`.
      /// The command name may contain the placeholder "%s", which is replaced by `args`.
      /// This mode of command definition is meant for remote network execution to avoid security holes.
      ///
    virtual bool IsRunning () { return !ReadClosed (); }
      ///< @brief Still running?
    virtual void Wait () = 0;
      ///< @brief Wait until current command completes.
    virtual void Kill (int sig = SIGTERM) { WriteClose (); }
      ///< @brief Kill the current command nicely.
      /// This may not work for all types of shells.
      /// A Wait() invocation is required anyway afterwards.
    int ExitCode () { return exitCode; }
      ///< @brief Get the exit code of last command.
    /// @}

    /// @name Simple running...
    /// @{
    int Run (const char *cmd, const char *input = NULL, CString *output = NULL);
      ///< @brief Run command 'cmd' synchronously and return its exit code.
      ///
      /// The standard input is taken from 'input' and the output is writen to the string referred to
      /// by 'output'.
      /// This function is only appropriate for short running
      /// commands since the calling thread blocks until the command completes.
    /// @}

    /// @name Accessing stdin & stdout...
    /// @{
    virtual void CheckIO (bool *canWrite, bool *canRead, TTicks maxTime = -1) = 0;
      ///< @brief Check whether the i/o channels can take/deliver data.
      /// @param canWrite may point to a variable that will be set, depending on whether at least one
      ///    byte can be written to the external command.
      /// @param canRead may point to a variable that will be set, depending on whether at least one
      ///    byte can be read from the external command.
      /// @param maxTime is the maximum waiting time in milliseconds. A value <0 lets it wait forever,
      ///    a value 0 does not cause any waiting at all.
      ///
      /// If any of the two pointers 'canWrite' and 'canRead' is 'NULL', the caller indicates that he is not interested in the
      /// respective result. Hence, the method waits until the respective other channel is ready.
      ///
      /// In situations, where CheckIO() would wait forever, it returns immediately with both flags
      /// set to 'false' (for example: read channel already closed, and `canWrite == NULL`).
      /// **Note:** As long as both the read and the write channel are open, the caller (usually) must
      /// always write *and* read as much as possible. Otherwise, a deadlock may occur.
      ///
    bool WaitUntilReadable (TTicks maxTime = -1) { bool canRead; CheckIO (NULL, &canRead, maxTime); return canRead; }
      ///< @brief Wait until output of the external command is readable.
      /// Be aware of potential deadlocks. This command can only be safely used if the external
      /// command does not wait for input.
      /// @return 'true', if the output is readable.
      ///
    bool WaitUntilWritable (TTicks maxTime = -1) { bool canWrite; CheckIO (&canWrite, NULL, maxTime); return canWrite; }
      ///< @brief Wait until the external command can accept written input.
      /// Be aware of potential deadlocks. This command can only be safely used if the external
      /// command does not produce any output.
      /// @return 'true', if the output is writeable.
      ///

    bool Writable () { return WaitUntilWritable (0); }
      ///< @brief Poll for writability (non-blocking).
    virtual void WriteLine (const char *line) = 0;
      ///< @brief Blocking write, execution is guaranteed.
      /// To avoid blocking, run Writable() or CheckI() first.
    void WriteLine (CString *str) { WriteLine (str->Get ()); }
      ///< @brief Write a line.
    virtual void WriteClose () = 0;
      ///< @brief Close the write channel.

    bool Readable () { return WaitUntilReadable (0); }
      ///< @brief Poll for readability.
    virtual bool ReadLine (CString *str) = 0;
      ///< @brief Non-Blocking read, returns 'true' on success.
      /// `str` can be NULL, in which case the line is ignored, given the reading was successul.
    virtual bool ReadClosed () = 0;
      ///< @brief Close the read channel.
    /// @}

  protected:
    int exitCode;
};




// ***** CShellBare *****


/** @brief Normal shell: Commands are executed individually.
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
    virtual bool Start (const char *cmd, bool readStdErr = false);
      ///< @brief Start new command, optionally on a remote host (see below).
    virtual bool IsRunning ();
    virtual void Wait ();
    virtual void Kill (int sig = SIGTERM); ///< (not tested in shell mode!)
    /// @}

    /// @name Accessing stdin & stdout...
    /// @{
    virtual void CheckIO (bool *canWrite, bool *canRead, TTicks maxTime = -1);
      ///< @brief Use select() to check whether the i/o channels can take/deliver data.

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
    int ReadFd () { return fdFromScript; }   ///< @brief Get FD to allow select() by owner with multiple scripts.
    int WriteFd () { return fdToScript; }    ///< @brief Get FD to allow select() by owner with multiple scripts.
    /// @}

    /// @name Extra features...
    /// @{
    void SetHost (const char *_host);
      ///< @brief Set host to run subsequent commands on; 'NULL' represents the local host.
    const char *Host () { return host; }
    void SetNewProcessGroup (bool on = true) { newProcessGroup = on; }
      ///< @brief Create a new process group and let this process become the leader (for daemon to support job control).
    bool StartSession (bool readStdErr = false) { return Start (NULL, readStdErr); }
      ///< @brief Start a commmand shell session.
    /// @}

  protected:
    CString id, host;
    bool newProcessGroup;
    CString readBuf;
    bool readBufMayContainLine;
    int fdToScript, fdFromScript;
    int childPid, killSig;

    bool DoWaitPid (int options);
};



// ***** CShellSession *****


/** @brief Session shell: Multiple commands are executed through one shell.
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
      ///< @brief Define a remote host; must be called before first Start() call.
    const char *Host () { return session.Host (); }
    /// @}

    /// @name Execution control...
    /// @{
    virtual bool Start (const char *cmd, bool readStdErr = false);
    virtual void Wait ();
    /// @}

    /// @name Accessing stdin & stdout...
    /// @{
    virtual void CheckIO (bool *canWrite, bool *canRead, TTicks maxTime = -1);

    virtual void WriteLine (const char *line) { session.WriteLine (line); }
    virtual void WriteClose ();

    virtual bool ReadLine (CString *str);
    virtual bool ReadClosed () { return !readOpen; }

    int ReadFd () { return session.ReadFd (); }      ///< @brief Get FD to allow select() by owner with multiple scripts.
    int WriteFd () { return session.WriteFd (); }    ///< @brief Get FD to allow select() by owner with multiple scripts.
    /// @}

  protected:
    CShellBare session;
    bool writeOpen, readOpen;
};


/// @}  // Shells





// *************************** Service managing ********************************


/** @defgroup common_service Service Management
 * @brief Helper class(es) for managing services.
 *
 * @{
 */



// ***** CServiceKeeper *****


/** @brief Service keeper: Helper class to keep an unreliable service running.
 *
 * This class takes care of keeping a service running by scheduling re-open attempts according
 * to some given strategy.
 *
 * The class can be used in two ways:
 *
 * 1. Callback use: Deriving a sub-class and overriding the virtual methods @ref DoOpen() and @ref DoClose().
 *    @ref Iterate() must be called regularly to get necessary actions performed.
 *
 * 2. Polling use: Instantiating this class and calling @ref OpenAttemptNow() and @ref CloseNow()
 *    and (re-)open or close the service accordingly.
 *
 */
class CServiceKeeper {
  public:

    /// @name Initialization and setup ...
    /// @{
    CServiceKeeper () { isOpen = shouldBeOpen = false; tDShort = tDLong = tShortToLong = 0; }
    CServiceKeeper (TTicks _tDShort, TTicks _tDLong = NEVER, TTicks _tShortToLong = NEVER)
      { isOpen = shouldBeOpen = false; Setup (_tDShort, _tDLong, _tShortToLong); }
    ~CServiceKeeper () { Close (); }

    void Setup (TTicks _tDShort, TTicks _tDLong = NEVER, TTicks _tShortToLong = NEVER) { tDShort = _tDShort; tDLong = _tDLong; tShortToLong = _tShortToLong; }
      ///< @brief Setup the retry policy.
      /// @param _tDShort is the retry interval after the service got lost (e.g. disconnected / closed).
      /// @param _tDLong is the retry interval after the service was lost for a longer time.
      /// @param _tShortToLong is the time after which the retry interval is changed from _tDShort to _tDLong.
    /// @}

    /// @name Using the service ...
    /// @{
    void Open () { shouldBeOpen = true; Refresh (); }
      ///< @brief Request the service to open.
      ///
      /// Actual opening will be deferred until the next call to Iterate().
      ///
    void Close () { shouldBeOpen = false; }
      ///< @brief Request the service to close.
      ///
      /// Actual opening will be deferred until the next call to Iterate().
      /// To properly close the service, it must be closed explicitely by the caller.
      /// In callback use, this can be accomplished by calling Iterate() until IsOpen()
      /// returns 'false'.
      ///

    void Refresh ();
      ///< @brief Force retry on next Iterate() and return to short retry intervals.

    void Iterate ();
      ///< @brief Iterate (callback use); perform all open/close actions as adequate. Not needed for polling use.

    bool IsOpen () { return isOpen; }
      ///< @brief check if the service is actually open.
    bool ShouldBeOpen () { return shouldBeOpen; }
      ///< @brief check if the service has been requested to be open.
    /// @}

    /// @name Callbacks (for callback use) ...
    ///
    /// These callbacks are always called from @ref Iterate().
    ///
    /// @{
    virtual void DoOpen () {}
      ///< @brief Attempt to open the service in callback use.
      /// If this method is overloaded it must call @ref ReportOpenAttempt() to report that the attempt has occurred.
      ///
    virtual void DoClose () {}
      ///< @brief Actually close the service.
      /// If this method is overloaded it must call @ref ReportClosed() to report that the closing has succeeded.
      /// Otherwise, this method will be called again on next call of @ref Iterate().
    /// @}

    /// @name Polling methods (for polling use) ...
    /// @{
    bool OpenAttemptNow ();
      ///< @brief Query if the connection should be (re-)opened now.
      /// After the attempt, @ref ReportOpenAttempt() must be called to report whether the attempt was successful or not.
    bool CloseNow ();
      ///< @brief Query if the connection should be closed now.
      /// Closing must not fail.
    /// @}

    /// @name Reporting about the service (for callback and polling use) ...
    /// @{
    void ReportLost ();
      ///< @brief Report that the service is no longer running.
    void ReportOpenAttempt (bool success);
      ///< @brief Report an open attempt (either successful or not).
      /// This must be called from DoOpen() or after OpenAttemptNow() has returned 'true' to confirm that
      /// an open attempt has taken placed.
    void ReportClosed () { isOpen = false; }
      ///< @brief Report that the service was actually closed.
      /// This must be calld from DoClose() or after CloseNow() has returned 'true' to confirm that
      /// the service is actually closed.
    /// @}

  protected:
    bool isOpen, shouldBeOpen;
    TTicks tLastOpen;    // last time the service was known to be open (only valid if 'isOpen == false' and 'shouldBeOpen == true')
    TTicks tNextAttempt; // time of next open attempt (only valid if 'isOpen == false' and 'shouldBeOpen == true')
    TTicks tDShort, tDLong, tShortToLong;
};


/// @}  // Service managing





// *************************** Doxygen Hooks  **********************************
//
// Declare groups for the other modules in "common".

/// @defgroup common_phone Phone
/// @brief Wrapper for IP phone functionality.

/// @defgroup common_streamer Streamer
/// @brief Wrapper for media streaming functionality (audio via *GStreamer*).



/// @}  // @file


#endif
