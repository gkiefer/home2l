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


#ifndef _ENV_
#define _ENV_

// This file is SWIG-readable and part of the Python library.


/** @file
 *
 * @defgroup env Environment
 * @brief Access configuration parameters and query environment properties.
 *
 * @{
 */

#ifdef SWIG
%pythoncode %{
## @defgroup env Environment
## @brief Access configuration parameters and query environment properties.
##
## More detailed information on these functions can be found in the
## [*C/C++ API documentation*](../home2l-api_c/group__env.html).
## @{
%}
#endif


#ifndef SWIG

#include "base.H"

#include <stdio.h>

#endif // SWIG



// ********************** Global settings **************************************


/// @defgroup env_settings Static Settings
/// @brief Global compile-time settings.
/// @{


#define HOME2L_USER "home2l"    ///< User and group name of the Home2L user (for background processes).


/// @}  // @defgroup




// ***************** Basic Environment Information and Helpers *****************


/// @defgroup env_helpers Helpers
/// @brief Direct access to basic environment properties and helpers.
/// @{


/// @name Environment ...
/// @{

const char *EnvBuildOS ();        ///< @brief Operating software ("Debian", "Android", ...).
const char *EnvBuildArch ();      ///< @brief Operating architecture (e.g. "i386", "armhf", ...).

const char *EnvMachineName ();    ///< @brief Machine name (network host name).

const char *EnvExecPathName ();
  ///< @brief Full path name of the executable (== argv[0]).
  ///
  /// **Note:** The return values of EnvExecPathName() and EnvExecName() may be undefined (== NULL),
  ///           e.g. if the main program is a Python script or an Android app (TBD: FIX?).
const char *EnvExecName ();       ///< @brief File name of the executable without path.

const char *EnvInstanceName ();
  ///< @brief  Instance name - the identifier for the running process.
  ///
  /// The instance name weakly identifies the running process as follows:
  ///
  /// * By default ('EnvInit' called with 'argv[0][0] != '/'), it is set to the executable
  ///   name (without leading "home2l-").
  ///   By calling 'EnvInit' with 'argv[0][0] != '/', it can be set to any given name,
  ///   in which case EnvExecPathName() will return NULL. In this case 'argv[0]' must
  ///   refer to the instance full name.
  ///
  /// * In a correct installation and during normal operation, no two processes with the same
  ///   instance name shall be executed simultaneously. This must be considered in the per-host
  ///   setup and when writing tools for which multiple running instances regularly may make sense.
  ///
  /// * The required uniqueness (previous note) is weak in the sense that no technical mechanisms
  ///   exist to enforce it and no software may rely on it. An example is the case where an
  ///   administrator starts two Home2l shells simultaneously, which may well act as a single
  ///   instance.
  ///
  /// * Code requiring strict uniqueness should use the PID as an additional criterion
  ///   (presently done so in the *Resources* library to identify non-server hosts).
  ///

const char *EnvDroidId ();        ///< @brief Droid ID

int EnvPid ();                    ///< @brief Operating system's process identifier (PID).
bool EnvHaveTerminal ();          ///< @brief 'true', if the application has been started from an interactive terminal.

void EnvReadIniFile (const char *fileName, CDictFast<CString> *map);
  ///< @brief Read a .ini file.
  /// 'fileName' must be an absolute path name.

/// @}



/// @name Directories ...
/// @{

const char *EnvHome2lRoot ();     ///< @brief Root installation path: all static data (blob + etc).
const char *EnvHome2lEtc ();      ///< @brief 'etc' root path: configuration data.
const char *EnvHome2lVar ();      ///< @brief 'var' root path: dynamic data (saved over reboots).
const char *EnvHome2lTmp ();      ///< @brief 'tmp' root path: dynamic data (not need to save over reboots).

#ifndef SWIG

const char *EnvGetHome2lRootPath (CString *ret, const char *relOrAbsPath);
  ///< @brief Return an absolute path name in the root domain.
  /// A path starting with '/' is considered already absolute and is returned unchanged, but normalized.
const char *EnvGetHome2lEtcPath (CString *ret, const char *relOrAbsPath);
  ///< @brief Return an absolute path name in the 'etc' domain.
const char *EnvGetHome2lVarPath (CString *ret, const char *relOrAbsPath);
  ///< @brief Return an absolute path name in the 'var' domain.
const char *EnvGetHome2lTmpPath (CString *ret, const char *relOrAbsPath);
  ///< @brief Return an absolute path name in the 'tmp' domain.

#else // SWIG

%feature("docstring") EnvGetHome2lRootPath "Return an absolute path name in the root domain.\n\n" \
  "Path starting with '/' are considered already absolute and are\n" \
  "returned unchanged, but normalized.\n"
%feature("docstring") EnvGetHome2lEtcPath "Return an absolute path name in the 'etc' domain.\n\n" \
  "Path starting with '/' are considered already absolute and are\n" \
  "returned unchanged, but normalized.\n"
%feature("docstring") EnvGetHome2lVarPath "Return an absolute path name in the 'var' domain.\n\n" \
  "Path starting with '/' are considered already absolute and are\n" \
  "returned unchanged, but normalized.\n"
%feature("docstring") EnvGetHome2lTmpPath "Return an absolute path name in the 'tmp' domain.\n\n" \
  "Path starting with '/' are considered already absolute and are\n" \
  "returned unchanged, but normalized.\n"

%newobject EnvGetHome2lRootPath (const char *);
%newobject EnvGetHome2lEtcPath (const char *);
%newobject EnvGetHome2lVarPath (const char *);
%newobject EnvGetHome2lTmpPath (const char *);
%inline %{
static inline const char *EnvGetHome2lRootPath (const char *relOrAbsPath) { CString s; EnvGetHome2lRootPath (&s, relOrAbsPath); return s.Disown (); }
static inline const char *EnvGetHome2lEtcPath (const char *relOrAbsPath) { CString s; EnvGetHome2lEtcPath (&s, relOrAbsPath); return s.Disown (); }
static inline const char *EnvGetHome2lVarPath (const char *relOrAbsPath) { CString s; EnvGetHome2lVarPath (&s, relOrAbsPath); return s.Disown (); }
static inline const char *EnvGetHome2lTmpPath (const char *relOrAbsPath) { CString s; EnvGetHome2lTmpPath (&s, relOrAbsPath); return s.Disown (); }
%} // inline

#endif // SWIG

bool EnvMkVarDir (const char *relOrAbsPath);
  ///< @brief Prepare (create) a directory in the 'var' domain.
  /// @param relOrAbsPath is either an absolute path or a path relative to the domain.
  ///
  /// On error, a warning is emitted and 'false' is returned.
bool EnvMkTmpDir (const char *relOrAbsPath);
  ///< @brief Prepare (create) a directory in the 'tmp' domain.

/// @}



/// @name Network ...
/// @{

bool EnvNetResolve (const char *hostAndPort, CString *retHost, int *retPort = NULL, int defaultPort = 0, bool warn = true);
  ///< @brief Get a resolved host + port combination.
  /// @param hostAndPort is a given network host name, optionally followed by ':' and a port number.
  ///        The host part is translated using the 'net.resolve.\<name\>' settings.
  /// @param retHost is set to the resolved networt host name or IP address.
  /// @param retPort may contain a pointer to the resolved port number. This is the first of the following
  ///        numbers: a) port appended to `hostAndPort`, b) port given in the 'net.resolve.\<name\>' setting,
  ///        c) port passed as `defaultPort`.
  /// @param defaultPort is the default port.
  /// @param warn decides whether a warning is logged on error.
  /// @return success status.

/// @}  // @name


/// @name Location ...
/// @{

float EnvLocationLatitudeN ();
  ///< @brief Get the WGS84 latitude north coordinate of the home (the 'location.latitudeN' setting).
float EnvLocationLongitudeE ();
  ///< @brief Get the WGS84 longitude east coordinate of the home (the 'location.longitudeE' setting).



/// @}  // @name




/// @}  // @defgroup





// ********************** Environment dictionary *****************************


/// @defgroup env_general General
/// @brief Init/done and persistence.
/// @{


/// @name Init/Done ...
/// @{
void EnvInit (int argc, char **argv, const char *specOptionsUsage = NULL, const char *instanceName = NULL, bool noBanner = false);
  ///< @brief Initialize the environment module.
  /// @param argc is the main funcion's 'argc' parameter.
  /// @param argv is the main funcion's 'argv' parameter.
  /// @param specOptionsUsage allows to add extra usage information on specific tool options.
  /// @param instanceName defines the instance name, which otherwise is derived from the executable name (argv[0]).
  /// @param noBanner suppresses the output of the tool banner (for command line tools with minimal output).
void EnvPrintBanner ();
  ///< @brief Print the tool banner (only useful if banner was suppressed in the @ref EnvInit() call).
void EnvDone ();
  ///< @brief Shutdown the environment module.
/// @}


/// @name Persistence ...
/// @{
void EnvEnablePersistence (bool writeThrough = false, const char *_varFileName = NULL);
  ///< @brief Enable the persistence of all environment variables starting with "var.*".
  ///
  /// @param writeThrough decides whether the file is written back automatically with
  /// each @ref EnvPut() call changing any "var.*" variable. If set to 'false',
  /// the file is only written back on shutdown ( @ref EnvDone() ) or on a explicit
  /// flush ( @ref EnvFlush() ).
  ///
  /// @param _varFileName is the filename to store the variables, which should be
  /// pathname relative to the "var" directory. By default, "home2l-<instance name>.conf"
  /// is used. It is discouraged to pass an absolute path there.
  ///
  /// Persistent variables are (tried to be) read from the filesystem on this call,
  /// and they may override any settings in the main configuration file.
  ///
  /// This function may be called multiple times, for example, on tool initialization
  /// and then later again inside @ref RcInit(), if the user has declared persistent
  /// resources. On the second invocation, '_varFileName' *must* be NULL. If
  /// 'writeThrough' is set differently, a logical OR of all passed values will
  /// become effective.

void EnvFlush ();   ///< @brief Write back any persistent variables now.
/// @}


/// @}  // @defgroup



/// @defgroup env_dictionary Dynamic Settings
/// @brief Accessing and managing the environment dictionary.
/// @{


/// @name Primitive Get/Put ...
/// @{

#ifdef SWIG
%feature("docstring") EnvGet "Get a Home2L configuration variable from the environment."
#endif
const char *EnvGet (const char *key);
  ///< @brief Generically read a setting; Returns 'NULL' on non-existence and never prints warnings.
  ///
  /// 'key' can optionally be a sequence of keys separated by a colon (":"). In this case,
  /// the keys are tried in sequential order and the first available value is returned.

const char *EnvPut (const char *key, const char *value);
  ///< @brief Generically put a string into the dictionary; Passing 'value == NULL' removes the entry.
  /// @return Pointer to the copy of the value as stored in the database.
  /// **Note:** It is allowed to call this method before 'EnvInit' to pre-initialize environment settings.
  ///       This feature is e.g. used for the Android initialization to set the hostname.
const char *EnvPut (const char *key, int value);
  ///< @brief Put an integer or boolean variable into the dictionary.
  /// @return Pointer the copy of the value as stored in the database.
static inline void EnvDel (const char *key) { EnvPut (key, (const char *) NULL); }
  ///< @brief Remove an entry from the environment dictionary.

/// @}


#ifndef SWIG

/// @name Get with type ...
/// These functions get an environment parameter and convert them to a given type.
/// For each type, there are two variants:
/// 1. An advanced function returning the success status and - only on success - writing the result back via a pointer.
/// 2. A simple function returning that type; On error, the type's default value is returned.
/// In all variants, a warning is emitted in case of a syntax error.
/// If a parameter does not existence, 'false' is returned as a success code, and a warning is emitted if 'warnIfMissing'
/// is set. Most parameters have reasonable defaults, so that 'warnIfMissing' is set to 'false' by default.
/// To help with the discovery of typos in parameter keys, all accesses to non-existent parameters are logged
/// as debug messages of level 1 in @ref EnvGet().
///
/// **Note:** The functions with implicit translation capabilities ('EnvGetPath*' and 'EnvGetHost*') convert
///   their strings in place. This means:
///   1. Later retrievals by EnvGet() will return the absolute path, not the original string, e.g. from the configuration file.
///   2. These functions may only be called from the main thread.
/// @{

bool EnvGetString (const char *key, const char **ret, bool warnIfMissing = false);
  ///< @brief Read a string (advanced variant).
const char *EnvGetString (const char *key, const char *defaultVal, bool warnIfMissing = false);
  ///< @brief Read a string (simple variant).
static inline const char *EnvGetString (const char *key, bool warnIfMissing) { return EnvGetString (key, (const char *) NULL, warnIfMissing); }
  ///< @brief Read a string (simple variant) without default.
static inline const char *EnvGetString (const char *key) { return EnvGet (key); }
  ///< @brief Accelerator for 'EnvGetString (key, warnIfMissing = false)'.

bool EnvGetPath (const char *key, const char **ret, const char *path = NULL, bool warnIfMissing = false);
  ///< @brief Read a path, eventually convert value to an absolute path before (advanced).
  /// The returned path is always absolute. Relative paths are interpreted as relative to 'path' or HOME2L_ROOT and made absolute.
const char *EnvGetPath (const char *key, const char *defaultVal, const char *path = NULL, bool warnIfMissing = false);
  ///< @brief Read a path, eventually convert value to an absolute path before (simple).
static inline const char *EnvGetPath (const char *key, const char *path = NULL, bool warnIfMissing = false) { return EnvGetPath (key, (const char *) NULL, path, warnIfMissing); }
  ///< @brief Read a path (simple variant) without default.
bool EnvGetHostAndPort (const char *key, CString *retHost, int *retPort = NULL, int defaultPort = 0, bool warnIfMissing = false);
  ///< @brief Get a host + port combination; host names are translated using the 'net.resolve.\<name\>' settings.
static inline bool EnvGetHost (const char *key, CString *retHost, bool warnIfMissing = false) { return EnvGetHostAndPort (key, retHost, NULL, 0, warnIfMissing); }
  ///< @brief Get a host name; host names are translated using the 'net.resolve.\<name\>' settings.


bool EnvGetInt (const char *key, int *ret, bool warnIfMissing = false);                  ///< Read an integer (advanced).
int EnvGetInt (const char *key, int defaultVal = 0, bool warnIfMissing = false);         ///< Read an integer (simple).

bool EnvGetFloat (const char *key, float *ret, bool warnIfMissing = false);              ///< Read a float value (advanced).
float EnvGetFloat (const char *key, float defaultVal = 0.0, bool warnIfMissing = false); ///< Read a float value (simple).

bool EnvGetBool (const char *key, bool *ret, bool warnIfMissing = false);                ///< Read a boolean value (advanced).
bool EnvGetBool (const char *key, bool defaultVal = false, bool warnIfMissing = false);  ///< Read a boolean value (simple).

/// @}

#endif // SWIG




#ifndef SWIG

/// @name Advanced access to the dictionary ...
/// @{
void EnvGetPrefixInterval (const char *prefix, int *retIdx0, int *retIdx1);
const char *EnvGetKey (int idx);
const char *EnvGetVal (int idx);

const CDictRaw *EnvGetKeySet ();
  ///< @brief Get the complete key set for direct key listing.
  ///
  /// Manipulations of the returned object are strictly forbidden!
/// @}

#endif // SWIG


/// @}  // @defgroup





// ************ Parameter declaration with automatic documentation *************


/** @defgroup env_declaration Declaration
 * @brief Declaring environment parameters used in your code with automatic documentation.
 *
 * For declaring variables reflecting environment settings, the following macros should be used, and the variables
 * should be declared globally. This way, automatic tools can generate a documentation of the environment parameters.
 * The macros take care of properly initializing the variables in 'EnvInit', the user does not need to call 'EnvGet...'
 * for these variables. If this is not possible (e.g. because the key is variable or the type does not match any of
 * the standard types), the macro 'ENV_PARA_SPECIAL' must be inserted to inform the documentation tools.
 * 'ENV_PARA_SPECIAL' may be followed by (custom) variable declaration in the same line
 * Each macro invocation must be followed by a comment with the parameter documentation in the following format:
 *
 *     ENV_PARA_... (...)
 *      / * Short description (max. ~40 chars, no trailing period)
 *        *
 *        * Optional long description, optionally covering multiple lines or empty lines.
 *        * Only the last line must not be empty, and there must be exactly one empty
 *        * line between the short and the long description.
 *        *
 *        * Formatting can be done with LaTeX syntax.
 *        * /
 *
 * Details on formatting options can be found in the Home2L Book.
 *
 * **Note:** Persistent variables ("var.*" domain) cannot be initialized this way.
 *   They must be declared via 'ENV_PARA_SPECIAL'.
 *
 * @{
 */


enum EEnvParaType {
  eptString = 0,
  eptPath,
  eptInt,
  eptFloat,
  eptBool
};


class CEnvPara {
  public:
    CEnvPara (const char *_key, EEnvParaType _type, void *_pVar);

    static void GetAll (bool withVarKeys = true);

  protected:
    const char *key;
    EEnvParaType type;
    void *pVar;

    CEnvPara *next;
    static CEnvPara *first;
};


/// @name Variables with standard types ...
/// @{
#define ENV_PARA_STRING(key,varName,varDefault) const char *varName = varDefault; CEnvPara varName##PreInit (key, eptString, &varName); const char *varName##Key = key;
#define ENV_PARA_PATH(key,varName,varDefault) const char *varName = varDefault; CEnvPara varName##PreInit (key, eptPath, &varName); const char *varName##Key = key;
#define ENV_PARA_INT(key,varName,varDefault) int varName = varDefault; CEnvPara varName##PreInit (key, eptInt, &varName); const char *varName##Key = key;
#define ENV_PARA_FLOAT(key,varName,varDefault) float varName = varDefault; CEnvPara varName##PreInit (key, eptFloat, &varName); const char *varName##Key = key;
#define ENV_PARA_BOOL(key,varName,varDefault) bool varName = varDefault; CEnvPara varName##PreInit (key, eptBool, &varName); const char *varName##Key = key;
/// @}

/// @name Variables for special cases ...
/// such as free types, manual initialization or read-only variables.
/// @{
#define ENV_PARA_VAR(key,varType,varName,varDefault) varType varName = varDefault; const char *varName##Key = key;
/// @}

/// @name Keys without a variable ...
/// The macro just defines a key name as a constant variable.
/// @{
#define ENV_PARA_NOVAR(key,varType,varName,varDefault) const char *varName##Key = key;
  ///< @brief Argument 'varType' is used for automatic documentation.
/// @}

/// @name Groups of variables ...
/// such as patterned variables like `doorman.<PHONE>.register`.
/// @{
#define ENV_PARA_SPECIAL(keyPattern, varType, varDefault)
  ///< @brief Arguments 'varType' and 'varDefault' are used for automatic documentation.
/// @}


/// @}  // @defgroup


/// @}  // @file
#ifdef SWIG
%pythoncode %{
## @}
%}
#endif


#endif
