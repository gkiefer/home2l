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


/* Home2L Resources interface file.
 *
 * This file contains the shared interface for the Home2L Resources library.
 * It is the sole file to be included by any client application.
 * Also, it serves as a SWIG input file for the Python library 'home2l'.
 */


#ifndef _RC_LIB_
#define _RC_LIB_


// This file is SWIG-readable and part of the Python library.


#include "env.H"





// ***** Doxygen Header *****


/** @file
 *
 * @defgroup resources Resources
 * The *Home2L Resources* library.
 *
 * This module contains the shared interface for the *Home2L Resources* library.
 *
 * Notes
 * -----
 *
 * ### Terminology
 *
 * **URI (Uniform Resource Identifier):**
 *   Full path name for some object, including aliases and env settings.
 *
 * **LID (Local ID):**
 *   String to identify an object locally inside the process.
 *
 * **GID, ID (Global ID):**
 *   String to identify an object globally inside the Home2L net.
 *   If a class has no distinction between local and global IDs, the "G"
 *   may be skipped. Anyway, the ID is still globally unique.
 *
 * **Origin:**
 *   String with human-readable information on where an object comes from.
 *
 * **HID (host ID):**
 *   ID by which a host can be identified by the local instance for a re-connect.
 *
 * The exact syntaxes of LIDs and GIDs can be found in comments to the
 * respective '[Get]?id' and 'Set*id' methods of the various classes.
 *
 *
 * ### Resource URI (uniform resource identifier)
 *
 * General:  `/[<host>]/<driver>/<localPath>`
 *
 *
 *
 * @{
 *
 * @defgroup resources_general Main API
 * Top-level functionality of this library - most commonly used functions can be found here
 *
 * @defgroup resources_values Values, Types and States
 * Basic definitions for values and their types and state
 *
 * @defgroup resources_rc *Resource* Objects
 * Accessing resource objects
 *
 * @defgroup resources_subscribe Subscriptions
 * Subscribing to resources to read their values
 *
 * @defgroup resources_requests Requests
 * Placing requests to change resource values
 *
 * @defgroup resources_drivers Drivers
 * Defining custom drivers
 *
 * @defgroup resources_directory Directory Services
 * Querying local metadata directories (for shells or service tools)
 */

#ifdef SWIG
%pythoncode %{
## @defgroup resources Resources
## The *Home2L Resources* library.
##
## The API exports a large number of low-level C/C++ functions and classes, which in many cases
## are not relevant for rules scripts or Python-based drivers and are not documented in detail here.
## As a rule of thumb, the level of documentation reflects the relevance of some function.
##
## The most common function can be found in section @ref resources_general.
## Besides this, the classes CResource (@ref resources_rc) and CRcValueState (@ref resources_values)
## are important for accessing the data transported via resource objects.
##
## Most of the information presented here is also available as *Python docstrings*
## (avaliable via `help(<name>)` in a *Python* shell).
## Additional information can be found in the
## [*C/C++ API documentation*](../home2l-api_c/group__resources.html).
##
## @{
##
## @defgroup resources_general Main API
## Top-level functionality of the *Resources* library - most commonly used functions can be found here.
##
## @defgroup resources_values Values, Types and States
## Basic definitions for values and their types and state.
## Most relevant is the CRcValueState class to access the present value and state of a resource.
## More details can be found in the [*C/C++ API documentation*](../home2l-api_c/group__resources__values.html).
##
## @defgroup resources_rc *Resource* Objects
## Accessing resource objects.
## More details can be found in the [*C/C++ API documentation*](../home2l-api_c/group__resources__rc.html).
##
## @defgroup resources_directory Directory Services
## Querying local metadata (for interactive debugging).
##
## @defgroup resources_subscribe Subscriptions
## Low-level access to the C/C++ subscriber classes.
##
## @defgroup resources_requests Requests
## Low-level access to the C/C++ request class.
%}
#endif // SWIG





// *************************** (Forward) Declararions ****************************


#ifndef SWIG


// Forward declarations (full declaration below)...
class CRcSubscriber;
class CRcRequest;



// ***** Internal classes *****

class CRcHost;            // represents a (remote) host
class CRcDriver;          // represents a (local) driver
class CRcSubscriberLink;
class CResourceLink;


#endif // #ifndef SWIG



// ***** Request priorities *****

/// @addtogroup resources_requests
/// @{
#ifdef SWIG
%pythoncode %{
## @addtogroup resources_requests
## @{
%}
#endif // SWIG

/** @name Request Priorities
 *
 * Request priorities are in the range from 0..15, and the following constants define
 * some recommended values.
 *
 * For writable resources, their drivers should set a permanent request at prio 0 for the default value.
 * If no default is set, this lib will keep the last value if the last request is removed.
 *
 * @{
 */

#define rcPrioMin     0             ///< Minimum (lowest) priority
#define rcPrioMax     9             ///< Maximum (highest) priority

#define rcPrioDefault rcPrioMin     ///< Typical use: Preset defaults (typically by the driver)
#define rcPrioWeak    2             ///< Typical use: Automated rules (e.g. close window shades at certain times)
#define rcPrioNormal  5             ///< (Default value) Typical use: Direct user interaction (e.g. user pushes button to open window shades)
#define rcPrioStrong  8             ///< Typical use: Administrative interaction (default priority in the Home2l shell)
#define rcPrioSafety  rcPrioMax     ///< Typical use: Necessary actions to avoid damage (e.g. close roof window on bad weather)

/// @}  // @name


/// @}  // resources_requests
#ifdef SWIG
%pythoncode %{
## @}
%}
#endif // SWIG





// *************************** Values, Types and States ************************


/// @addtogroup resources_values
/// @{
#ifdef SWIG
%pythoncode %{
## @addtogroup resources_values
## @{
%}
#endif // SWIG





// ***** Types *****


#define rctBasicTypesBase rctBool
#define rctBasicTypesLast rctTime

#define rctSpecialTypesBase rctTrigger
#define rctSpecialTypesLast rctMutex

#define rctUnitTypesBase 0x40
#define rctUnitTypesLast (rctUnitTypesEND-1)

#define rctEnumTypesBase 0x80
#define rctEnumTypesLast (rctEnumTypesEND-1)


/// Resource major type
enum ERcType {
  rctNone = 0,    ///< No type

  /// @name Standard (base) data types ...
  /// @{
  rctBool,        ///< Boolean
  rctInt,         ///< Integer
  rctFloat,       ///< Float
  rctString,      ///< String
  rctTime,        ///< Time
  /// @}

  /// @name Special resource types...
  /// @{
  rctTrigger,   ///< [rctInt] Resource does not carry a value, but can transport events ("triggers"); 'vTriggerCount' counts the number of trigger events.
  rctMutex,     ///< [rctString] resource represents a mutex (not implemented yet)
                ///
                ///   **Note:** Mutex operations are not implemented yet. Once implemented, they will work as follows.
                ///
                ///   Operation of mutexes:
                ///
                ///   a) The value of the resource is the GID of the successful request. By using different
                ///      priorities, the mutex can be made preemptive. For non-preemptive mutexes, all
                ///      users should use the same priority ('prioNormal'). Any mutex resource should
                ///      have the empty string as a default (at priority 'prioDefault').
                ///
                ///   b) Locking procedure:
                ///        1. Issue request at a given GID `<gid>`
                ///        2. Wait until the value changes to `<gid>`
                ///
                ///      Unlocking:
                ///        1. Remove the request with GID `<gid>`
  /// @}

  /// @name Unit types ...
  /// This list may be extended in the future.
  /// To add new unit type, do the following:
  /// - Add a new entry at the end of this section.
  /// - In file resources.C, append a new entry to 'rcUnitTypeList`.
  ///
  /// Unit types can be based on 'rctInt' or 'rctFloat'.
  /// @{
  rctPercent = rctUnitTypesBase,
                ///< [float] Percentage (0.0 .. 100.0)
  rctTemp,      ///< [float] Temperature in degree centigrade
  rctUnitTypesEND,
  /// @}

  /// @name Enumeration types ...
  /// This list may be extended in the future.
  /// To add new enumeration type, do the following:
  /// - Add a new entry at the end of this section.
  /// - In file resources.C, append a new entry to 'rcEnumTypeList`.
  /// - (optional) Add an enum declaration named 'ERct<type>' below.
  ///
  /// Enumeration types are based on 'rctInt'.
  /// @{
  rctWindow = rctEnumTypesBase,
                ///< Window opening state ("closed", "tilted", "open", "openOrTilted")
  rctEnumTypesEND
  /// @}
};


// Type names ...
const char *RcTypeGetName (ERcType t);
ERcType RcTypeGetFromName (const char *name);

// Get base type ...
ERcType RcTypeGetBaseType (ERcType t);
  ///< Return the underlying base type as a unique representative for compatible classes (e.g. rctFloat/rctTemp -> rctFloat)
bool RcTypeIsStringBased (ERcType t);
  ///< Return if the base type is 'rctString'; This is the set of types that have dynamic data.

// Unit types ...
static inline bool RcTypeIsUnitType (ERcType t) { return t >= rctUnitTypesBase && t <= rctEnumTypesBase; }
  ///< Return whether the type is a unit type.
const char *RcTypeGetUnit (ERcType t);
  ///< Return the unit string; For non-unit types, an empty string is returned.

// Enumeration types ...
static inline bool RcTypeIsEnumType (ERcType t) { return t >= rctEnumTypesBase; }
  ///< Return whether the type is a enumeration type.
int RcTypeGetEnumValues (ERcType t);
  ///< Return the number of possible values for an enumeration type.
const char *RcTypeGetEnumValue (ERcType t, int idx, bool warn = true);
  ///< Return the value for 'idx' for an enumeration type.
  /// Allowed index vaules range from 0 to N-1, where N is the number of possible values.
  /// If the index is invalid, "?" is returned and, if 'warn == true', a warning is logged.
int RcTypeGetEnumIdx (ERcType t, const char *value, bool warn = true);
  ///< Return the index of some value for an enumeration type.
  /// If the 'value' is out of range, -1 is returned and, if 'warn == true', a warning is logged.





// ***** Enum declarations for enumeration types *****


enum ERctWindow {   // for 'rctWindow'
  rctWindowClosed = 0,
  rctWindowTilted,
  rctWindowOpen,
  rctWindowOpenOrTilted    // Type of opening is not known
};





// ***** Values *****


/// Union for an untyped value
#ifdef SWIG
%immutable vString;
#endif
union URcValue {
  uint64_t vAny;          // Pseudo-field to clear/compare independent of type

  bool vBool;
  int vInt;               // Width/value range can be machine dependent, but must be at least 32 bits (-2^31 .. +2^31-1).
  float vFloat;           // Encoding must be 32-bit floating point according to IEEE 754
  const char *vString;    // Pointer to a dynamically allocated string; NULL <=> string is empty ("")
  TTicks vTime;
};


#ifndef SWIG
BUILD_ASSERT(URcValue, sizeof (URcValue) == sizeof (URcValue::vAny));
#endif


/// @name Value ('URcValue') construction and evaluation ...
/// @{
static inline URcValue URcValueCleared () { URcValue ret; ret.vAny = 0; return ret; }   ///< Get cleared value (0 / 0.0 / false)

static inline URcValue URcValueOf (bool v) { URcValue ret; ret.vAny = 0; ret.vBool = v; return ret; }
static inline URcValue URcValueOf (int v) { URcValue ret; ret.vAny = 0; ret.vInt = v; return ret; }
static inline URcValue URcValueOf (float v) { URcValue ret; ret.vAny = 0; ret.vFloat = v; return ret; }
static inline URcValue URcValueOf (const char *v) { URcValue ret; ret.vAny = 0; ret.vString = v; return ret; }
static inline URcValue URcValueOf (TTicks v) { URcValue ret; ret.vAny = 0; ret.vTime = v; return ret; }
/// @}





// ***** State *****


/// Resource state
enum ERcState {
  rcsUnknown = 0,   ///< Value is presently unknown (e.g. outdated).
  rcsBusy,          ///< Devices is busy and/or switching to the value indicated by the associate value.
  rcsValid,         ///< Value is valid.
};





// ***** CRcValueState *****


/// Typed value tagged with a state and a time stamp.
#ifdef SWIG
%feature("docstring") CRcValueState "Typed value tagged with a state and a time stamp.\n\n"
  "The constants for type 'ERcType' have the prefix 'rct*'."
  "The state may be one of:\n"
  "  'rcsUnknown': The value is unknown.\n"
  "  'rcsBusy': The value is known, the underlying device is busy.\n"
  "  'rcsValid': The value is known and stable.\n"
  "The time stamp reflects the age of a value (last change/update). It is\n"
  "generally not valid to judge from the time stamp whether the value is\n"
  "outdated, since there are other mechanisms in the library setting the\n"
  "state to 'rcsUnknown' if, for example, the connection to a remote resource\n"
  "got lost."
#endif
class CRcValueState {
  public:
    CRcValueState () { type = rctNone; Clear (); }
    CRcValueState (const CRcValueState &vs) { type = rctNone; Set (vs.type, vs.val, vs.state); timeStamp = vs.timeStamp; }
    CRcValueState (ERcType _type) { type = rctNone; Clear (_type); }   // for initializing type, but no value & state
    CRcValueState (ERcType _type, URcValue _val, ERcState _state = rcsValid) { type = rctNone; Set (_type, _val, _state); }

    CRcValueState (ERcType _type, bool _val, ERcState _state = rcsValid) { type = rctNone; SetBool (_val, _state); }
    CRcValueState (ERcType _type, int _val, ERcState _state = rcsValid) { type = rctNone; SetGenericInt (_val, _type, _state); }
    CRcValueState (ERcType _type, float _val, ERcState _state = rcsValid) { type = rctNone; SetGenericFloat (_val, _type, _state); }
    CRcValueState (ERcType _type, const char *_val, ERcState _state = rcsValid) { type = rctNone; SetGenericString (_val, _type, _state); }
    CRcValueState (ERcType _type, TTicks _val, ERcState _state = rcsValid) { type = rctNone; SetTime (_val, _state); }

    ~CRcValueState () { Clear (rctNone); }

#ifndef SWIG
    CRcValueState& operator = (CRcValueState &_vs) { Set (_vs.type, _vs.val, _vs.state); timeStamp = _vs.timeStamp; return *this; }
#endif

    /// @name Set value and attributes (general methods) ...
    /// General methods to clear and set the object.
    /// Unless specified otherwise, all 'Set...' methods clear or set all attributes
    /// (i.e. the state and the time stamp). The time stamp is always cleared.
    /// @{
    void Clear () { Clear (type); }     ///< Clear value, leave type unchanged
    void Clear (ERcType _type, ERcState _state = rcsUnknown);
      ///< Clear to default value:
      ///   - rctBool: false
      ///   - rctInt / rctTrigger: 0
      ///   - rctFloat: 0.0
      ///   - rctString / rctMutex: "" (NULL)
    void Set (ERcType _type, URcValue _val, ERcState _state = rcsValid);
    void Set (URcValue _val, ERcState _state = rcsValid) { Set (type, _val, _state); }
    /// @}

    /// @name Set value and state (multi-type capable setters) ...
    /// Multi-type capable setters for setting values.
    /// The name indicates the input type.
    /// As the target type ('_type'), any type is allowed, and conversion is done as good as possible.
    /// For example, floating point values may be rounded to integers, or numbers are converted to
    /// 'true' if and only if they are != 0. With the exception of SetGenericString(), this never fails.
    /// @{
    void SetGenericInt (int _val, ERcType _type, ERcState _state = rcsValid);
      ///< Set from integer or boolean.
    void SetGenericFloat (float _val, ERcType _type, ERcState _state = rcsValid);
      ///< Set from float.
    bool SetGenericString (const char *_val, ERcType _type, ERcState _state = rcsValid);
      ///< Set from string. On syntax error, a warning is emitted, the default value (see 'Clear') is set,
      /// and 'false' is returned.
    /// @}

    /// @name Set value and and state (single-type setters) ...
    /// @{
    void SetBool (bool _val, ERcState _state = rcsValid)            { SetGenericInt    (_val, rctBool, _state);    }
    void SetInt (int _val, ERcState _state = rcsValid)              { SetGenericInt    (_val, rctInt, _state);     }
    void SetFloat (float _val, ERcState _state = rcsValid)          { SetGenericFloat  (_val, rctFloat, _state);   }
    bool SetString (const char *_val, ERcState _state = rcsValid)   { return SetGenericString (_val, rctMutex, _state);   }
    void SetTime (TTicks _val, ERcState _state = rcsValid);

    void SetTrigger (int _val = 0, ERcState _state = rcsValid)      { SetGenericInt    (_val, rctTrigger, _state); }
    bool SetMutex (const char *_val, ERcState _state = rcsValid)    { return SetGenericString (_val, rctMutex, _state);   }

    void SetUnitInt (ERcType type, int _val, ERcState _state = rcsValid)      { SetGenericInt   (_val, type, _state); }
    void SetUnitFloat (ERcType type, float _val, ERcState _state = rcsValid)  { SetGenericFloat (_val, type, _state); }

    void SetEnum (ERcType type, int _val, ERcState _state = rcsValid)         { SetGenericInt   (_val, type, _state); }
    /// @}

    /// @name Get value (strict typing) ...
    /// Get the value with strict typing.
    /// An incorrect type leads to a run-time error. The state is not considered.
    /// These are the most efficient methods.
    ///
    /// Use these methods for local/self-defined resources. The strict type checking will help
    /// you to discover bugs.
    ///
    /// Int() and Float() tolerate compatible types (trigger, unit, enum).
    /// @{
    URcValue      UValue () const   { return val; }
    int           GenericInt () const   { ASSERT (RcTypeGetBaseType (type) == rctInt);    return val.vInt;   }
    float         GenericFloat () const { ASSERT (RcTypeGetBaseType (type) == rctFloat);  return val.vFloat; }

    bool          Bool () const     { ASSERT (type == rctBool);     return val.vBool;   }
    int           Int () const      { ASSERT (type == rctInt);      return val.vInt; }
    float         Float () const    { ASSERT (type == rctFloat);    return val.vFloat; }
    const char *  String () const   { ASSERT (type == rctString);   return val.vString; }
    TTicks        Time () const     { ASSERT (type == rctTime);     return val.vTime;  }

    int           Trigger () const  { ASSERT (type == rctTrigger);  return val.vInt;    }
    const char *  Mutex () const    { ASSERT (type == rctMutex);    return val.vString; }

    int           UnitInt (ERcType _type) const    { ASSERT (type == _type);  return val.vInt;    }
    float         UnitFloat (ERcType _type) const  { ASSERT (type == _type);  return val.vFloat;  }

    int           EnumIdx () const  { ASSERT (RcTypeIsEnumType (type)); return val.vInt; }
    /// @}

#ifndef SWIG
    /// @name Get value (relaxed typing)...
    /// Get the value with relaxed typing.
    /// A type mismatch inside the same type class is silently tolerated. The state is considered,
    /// and 'rcsUnknown' is returned if either the state is 'rcsUnknown', the type does not match
    /// or 'this == NULL'. No runtime errors will be thrown.
    ///
    /// Use these methods for remote resources. The relaced typing will help to avoid run-time errors.
    ///
    /// @{
    ERcState GetValue (bool *retBool);
    ERcState GetValue (int *retInt);
    ERcState GetValue (float *retFloat);
    ERcState GetValue (CString *retString);
    ERcState GetValue (TTicks *retTime);
    /// @}
#endif

    /// @name Get value (relaxed typing with auto-correction) ...
    /// Get the value with relaxed typing and auto-correction.
    /// These methods always return a valid value of the respective type. If 'GetValue' would
    /// return 'rcsUnknown', the default value is returned. 'this == NULL' is tolerated.
    /// Returned strings are only accessible as long as 'this' remains live and unchanged!
    /// @{
    bool          ValidBool (bool defaultVal = false) const;
    int           ValidInt (int defaultVal = 0) const;
    float         ValidFloat (float defaultVal = 0.0) const;
    const char *  ValidString (const char *defaultVal = CString::emptyStr) const;
    TTicks        ValidTime (TTicks defaultVal = 0) const;

    int           ValidTrigger (int defaultVal = 0) const                       { return ValidInt (defaultVal); }
    const char *  ValidMutex (const char *defaultVal = CString::emptyStr) const { return ValidString (defaultVal); }

    int           ValidUnitInt (int defaultVal = 0) const         { return ValidInt (defaultVal); }
    float         ValidUnitFloat (float defaultVal = 0.0) const   { return ValidFloat (defaultVal); }
    int           ValidEnumIdx (int defaultVal = 0) const         { return ValidInt (defaultVal); }
    /// @}

    /// @name Attributes ...
    /// @{
    void SetType (ERcType _type) { Clear (_type); }
    void SetState (ERcState _state) { state = _state; }
    void SetTimeStamp (TTicks _timeStamp) { timeStamp = _timeStamp; }

    ERcType Type () const { return type; }
    ERcState State () const { return state; }
    TTicks TimeStamp () const { return timeStamp; }

    bool IsValid () const { return state == rcsValid; }
      ///< State is `rcsValid`, neither `rcsBusy` nor `rcsUnknown`.
    bool IsKnown () const { return state != rcsUnknown; }
      ///< State is either `rcsValid` or `rcsBusy`; In other words: the value can be retrieved.

    bool Equals (CRcValueState *val2) const;
      ///< Strict comparison: state, type and value must match exactly; time stamps are not compared.
    bool ValueEquals (URcValue val2) const;
    bool ValueEquals (CRcValueState *val2) const;
      ///< Relaxed comparison: type and value must match exactly; state and time stamps are not compared.
    /// @}

    /// @name Type conversion ...
    /// @{
    bool Convert (ERcType _type);
      ///< Try to convert the current value to a new type.
      /// If a conversion is not possible, 'false' is returned and the object remains unchanged.
    /// @}

    /// @name Stringification ...
    /// @{
    const char *ToStr (bool withType = false, bool withTimeStamp = false, bool precise = false, int stringChars = 20) const;
      ///< (TTS) Output the object contents as a string.
    const char *ToStr (CString *ret, bool withType = false, bool withTimeStamp = false, bool precise = false, int stringChars = 20) const;
      ///< Output the object contents as a string according to the synax described for 'SetFromStr'.
      ///
      /// If 'withType' is set, the type information is prepended.
      /// If 'precise' is set, the output is in a format that allows a transportation without information loss,
      /// but potentially not human-readable.
      /// 'stringChars' is the maximum number of characters printed for a value of type 'rctString' (including elipses).
      /// 'precise = true' implies 'stringChars = INT_MAX'.
      /// If 'withTimeStamp' is set AND the time stamp is > 0, the time stamp is appended to the output.

    bool SetFromStr (const char *str);
      ///< Set the object based on textual information.
      ///
      /// Syntax: `["("<type>")"] [!]<value>|? [@<timeStamp>]`
      ///
      /// On error, a warning is emitted, the default value at a state of 'rcsUnknown'
      /// is set (see 'Clear'), and 'false' is returned.
      /// If 'str' contains type information, the 'type' field is set accordingly.
      /// If not, and the current type is 'rctNone', the type is auto-detected.
      /// In all other cases, 'type' is not changed and must match the given `<value>`.
      ///
      /// Syntax of `<value>`:
      ///
      ///     <bool>    ::= [0fF] | [1tT+]                  : boolean value
      ///     <int>     ::= [-][0-9]+                       : integer value
      ///     <float>   ::= [-][0-9]*[.[0-9]+][E[+/-][0-9]+]: floating-point value
      ///     <precise> ::= $[0-9a-f]{8}                    : float value encoded as a 8-digit hex value
      ///     <string>  ::= [0-9a-zA-Z\]+ | \0              : string (\-escaped, UTF8 encoding); terminating 0 is optional except for the emtpy string (= "\0")
      ///     <time>    ::= <time>                          : time value; see TicksFromString() for syntax
      ///     <unit>    ::= [<float>|<int>]<unit>           : unit value (<unit> is the unit string)
      ///     <enum>    ::= [_a-zA-Z][_a-zA-Z0-9]+          : enumeration value
      ///
      /// A '!' indicates the state 'rcsBusy', a value of '?' the state 'rcsUnknown'.
      /// In all other cases, the state is 'rcsValid'.
      ///
      /// Whitespaces are tolerated and stripped away internally.
    bool SetFromStrFast (const char *str, bool warn = true);
      ///< Set the object based on textual information (fast version).
      /// This is a variant of SetFromStr() which does not copy 'str' internally and may be faster,
      /// especially for long strings. This method cannot not interpret type and timestamp information.
      /// Also, leading/trailing whitespaces are not tolerated.
    /// @}

  protected:
    friend class CResource;

    // Fields...
    ERcType type;
    URcValue val;
    ERcState state;
    TTicks timeStamp;   ///< time of last value/state change or trigger
};


// Python extension...
#ifdef SWIG
%extend CRcValueState {
  %newobject __str__ ();
  const char *__str__ () { CString s; $self->ToStr (&s); return s.Disown (); }
  %newobject __repr__ ();
  const char *__repr__ () { CString s; s.SetF ("(CRcValueState) %s", $self->ToStr (true)); return s.Disown (); }
  bool __eq__ (CRcValueState *other) { return $self->Equals (other); }
  bool __ne__ (CRcValueState *other) { return !$self->Equals (other); }
  %pythoncode %{
    pass    # (Workaround to keep SWIG from scrambling the indentation of the following code.)

    ## Return the value as a Python object.
    def Value(self):
      """Return the value as a Python object. In case of a state of 'rcsUnknown' or\n\
      if 'self' is not defined, 'None' is returned. 'self == None' is allowed."""
      if not self: return None
      if self.State () == rcsUnknown: return None
      t = RcTypeGetBaseType (self.Type ())
      if   t == rctBool:   return self.UValue().vBool
      elif t == rctInt:    return self.UValue().vInt
      elif t == rctFloat:  return self.UValue().vFloat
      elif t == rctString: return self.UValue().vString
      else: return None

    ## Return the most appropriate valid value as a Python object.
    def ValidValue(self, defaultVal):
      """Return the value as a Python object. Unlike 'Value', this method never\n\
      returns 'None', but reverts to the given default value, if 'self' does not\n\
      transort a valid value. 'self == None' is tolerated\n\
      """
      ret = self.Value ()
      if not ret: ret = defaultVal
      return ret
  %} // %pythoncode
}; // %extend CRcValueState
#endif


/// @}  // resources_values
#ifdef SWIG
%pythoncode %{
## @}
%}
#endif // SWIG





// *************************** Resources ***************************************


/** @addtogroup resources_rc
 *
 *
 * Notes
 * -----
 *
 * ### Resource lifecycle
 *
 * 1. Resources are always existent virtually (see comment on 'CResource::Get ()'). During their life, they may be registered
 *    or unregistered. Local resources are registered by their drivers during the elaboration phase and are
 *    typically unregistered when the process terminates. Remote resources are first registered when the connection
 *    succeeds and may be temporarily become unregistered if the server connection gets lost.
 *
 * 2. For an unregistered resource, its owner (driver or host) is not known or set. Its type may be unknown
 *    ('rctNone'), its value state is always 'rcsUnknown'. Calling 'Report*' or 'Drive*' methods is not allowed.
 *
 * 3. For a registered resource, its owner (driver or host) is known and set. Its type is known, and all methods
 *    can to be used.
 *
 * 4. The value type may change (only) during a unregistered -> registered transistion ('Register*' method).
 *
 * 5. Both requests and subscriptions are preserved when the registration state changes. During unregistration,
 *    subscriptions may be "parked" in the subscriber map for efficiency reasons. During registration, requests are
 *    converted to the (potentially) new value type. If this fails, a warning is emitted and they are kept with
 *    their old type. During evaluations, requests with a type different from its resource are silently ignored.
 *
 * 6. 'Register' and 'Unregister' for one object should always be called from the same thread. The caller is
 *    responsible for avoiding concurrent calls of these methods.
 *
 *
 * ### Interpretation of time stamps and validity
 *
 * 1. The 'timeStamp' attribute points to the time of the last localy known change in state or value.
 *
 * 2. Only real changes are accounted for. For example, multiple reports of the same value by a driver do not lead
 *    to updated time stamps after the first report. In other words, it is known that just before 'timeStamp'
 *    the value & state was different from now.
 *
 * 3. The time stamp is always measured against the local clock. It is ensured that the observed sequence is monotonic,
 *    i.e. the age of an event is never smaller than that of the previous event on the same resource.
 *
 * 4. Network delays are neglected in normal operation, i.e. the time stamp of a remote resource corresponds to the
 *    time the respective event was received on the client. (Reasoning: LAN delays are considered very small.)
 *    The only exception is when network errors (timeouts) occur (see below).
 *
 * On reliability and safety-critical operations:
 *
 * 5. Drivers are responsible to report 'rcsUnknown' themselves immediately within reasonable time if their hardware
 *    fails. The maximum delay between the real failure and reporting ("failure report delay") must be stated in the
 *    driver documentation.
 *
 * 6. Network connection failures lead to 'rcsUnknown' on the client immediately, i.e. after a time of at most
 *    'envMaxAge' (default: 60 secs.). On the client, the time stamp is set to max(now - 'envMaxAge', last reported
 *    time stamp). This way, the time stamp always points to a time at which the state was not unknown. The time stamp
 *    on a client may differ by at most 'envMaxAge' from that on the server. However, both the server and the client
 *    observe the time stamp to be monotonous locally.
 *
 * 7. Safety functions can then be implemented as follows. As an example consider the auto-closing a window on rainy
 *    weather, where the window is also to be closed if the rain state is unkown for a time of Tmax. A necessary
 *    condition for this to work is that Tmax >= 'envMaxAge'.
 *    The rule may be implemented as follows:
 *    - Subscribe to rain sensor, catch all events from now.
 *    - On 'rain == 1': close immediately (normal function)
 *    - On 'rain == rcsUnknown': Safety-close at time t >= rain.timeStamp + Tmax
 *
 * @{
 */
#ifdef SWIG
%pythoncode %{
## @addtogroup resources_rc
## @{
%}
#endif // SWIG


/** Home2L Resource
 *
 * This is the central class of the library. A 'Resource' may represent
 * a sensor, an actor, a stored value, a virtual device, a mutex, an event
 * trigger - almost anything. Resources are identified by a uniform resource
 * identifier (URI). Their values are generally read by subscribing to
 * them and written to by placing requests.
 *
 * The methods of this calls can be divided into three main categories:
 * - (App) = Methods for applications using resources.
 * - (Drv) = Methods for drivers, only needed for driver development.
 * - (Meta) = Methods to access meta data; for shells or similar services.
 *
 * @nosubgrouping
 */
#ifdef SWIG
%feature("docstring") CResource "Home2L Resource.\n\n"
  "This is the most central class of the library. A 'Resource' may represent\n"
  "a sensor, an actor, a stored value, a virtual device, a mutex, an event\n"
  "trigger - almost anything. Resources are identified by a uniform resource\n"
  "identifier (URI)."
#endif
class CResource /* final */ {
  public:


    // ***** For applications *****

    /// @name (App) Obtaining and managing resources ...
    /// @{
    static CResource *Get (const char *uri, bool allowWait = false);
      ///< Look up a resource by its URI and return a reference to it.
      ///
      /// The returned pointer is always valid (!= NULL) unless the URI is syntactically incorrect and will neither now
      /// nor in the future refer to a valid resource. Hence, the pointer can be used for identity comparisons.
      ///
      /// If 'allowWait' is 'false', the function returns immediately.
      /// If 'allowWait' is 'true', the function may wait/block for some time, increasing the chance that the resource
      /// is known afterwards.
      ///
      /// If the URI is syntactically incorrect, a warning is emitted and 'NULL' is returned.
      ///
    static void GarbageCollection ();
      ///< Remove all unregistered resources.
      ///
      /// The calling process must not hold (and later use) any references to 'CResource' objects if this function is
      /// used.
      ///
      /// Usually, this function is not needed. It is only required with very long uptimes which for some reason cannnot
      /// be restarted easily. Resources that get unregistered (e.g. because their server terminates) are not deleted
      /// from memory to keep eventual references to them valid. If such a resource is never re-registered again, e.g.
      /// because a server is removed from the net permanently or its naming scheme is changed, the unregistered
      /// resources still require (little) memory. If the number of unregistered resources exceeds "rc.maxOrphaned",
      /// the process is terminated.
    /// @}

    /// @name (App) Identification (static) ...
    /// @{
    const char *Uri () { return gid.Get (); } ///< Get the URI.

    bool Is (const char *uri);                        ///< Check identity by exact match.
    bool Is (CResource *rc) { return this == rc; }    ///< Check identity (for Python & eventual wrapper classes).
    bool IsLike (const char *pattern);                ///< Check identity by pattern.

    ERcType Type () { return valueState.type; }
    bool IsWritable () { return writable; }
    /// @}

    /// @name (App) Stringification ...
    /// @{
    const char *ToStr (CString *ret, bool pathLocal = false);   // semi-static, iff 'pathLocal' == true!!
    const char *ToStr (bool pathLocal = false);                 // (TTS) semi-static, iff 'pathLocal' == true!!
    /// @}

#ifndef SWIG

    /// @name (App) Getting the current value ...
    /// Access methods to get the last known value and state.
    ///
    /// The following '[Get]*'-methods immediately return the last known value/state of the resource,
    /// without querying the driver. To trigger a regular transport of data from a sensor/resource here,
    /// a subscription must be placed (see below).
    ///
    /// **Note:** The value and state may change any time. Each of the following methods
    /// returns a value/state consistent by itself.
    ///
    /// @{
    void GetValueState (CRcValueState *retValueState);
      ///< Get the value and state.

    // Get value (relaxed typing) ...
    ERcState GetValue (bool *retBool, TTicks *retTimeStamp = NULL);
      ///< Get a value of type "Bool" or a type based on it.
      /// Type mismatches inside a type class are silently tolerated.
      /// The returned state must be checked. If the state is 'rcsUnknown', the type does not match
      /// or if 'this == NULL', a value of 'rcsUnknown' is returned and '*ret...'
      /// is left unchanged. No runtime errors will be thrown.
    ERcState GetValue (int *retInt, TTicks *retTimeStamp = NULL);
      ///< Get a value of type "int" or a type based on it.
    ERcState GetValue (float *retFloat, TTicks *retTimeStamp = NULL);
      ///< Get a value of type "float" or a type based on it.
    ERcState GetValue (CString *retString, TTicks *retTimeStamp = NULL);
      ///< Get a value of type "string" or a type based on it.
    ERcState GetValue (TTicks *retTime);
      ///< Get a value of type "time".

    // Get value (relaxed typing with auto-correction) ...
    bool          ValidBool (bool defaultVal = false, TTicks *retTimeStamp = NULL);
      ///< Get a valid Boolean value.
      /// This and the following methods are shortcuts for first obtaining the 'ValueState' and then use the same methods
      /// of @ref CRcValueState on that object.
    int           ValidInt (int defaultVal = 0, TTicks *retTimeStamp = NULL);
      ///< Get a valid integer value.
    float         ValidFloat (float defaultVal = 0.0, TTicks *retTimeStamp = NULL);
      ///< Get a valid float value.
    const char *  ValidString (CString *ret, const char *defaultVal = CString::emptyStr, TTicks *retTimeStamp = NULL);
      ///< Get a valid string value.
    const char *  ValidString (const char *defaultVal = CString::emptyStr, TTicks *retTimeStamp = NULL);
      ///< Get a valid string value (TTS variant).
    TTicks        ValidTime (TTicks defaultVal = 0, TTicks *retTimeStamp = NULL);
      ///< Get a valid time value.

    int           ValidTrigger (int defaultVal = 0, TTicks *retTimeStamp = NULL)  { return ValidInt (defaultVal, retTimeStamp); }
      ///< Get a valid trigger value.
    const char *  ValidMutex (CString *ret, const char *defaultVal = CString::emptyStr, TTicks *retTimeStamp = NULL)  { return ValidString (ret, defaultVal, retTimeStamp); }
      ///< Get a valid mutex value.
    const char *  ValidMutex (const char *defaultVal = CString::emptyStr, TTicks *retTimeStamp = NULL)                { return ValidString (defaultVal, retTimeStamp); }
      ///< Get a valid mutex value (TTS variant).

    int           ValidUnitInt (int defaultVal = 0, TTicks *retTimeStamp = NULL)        { return ValidInt (defaultVal, retTimeStamp); }
      ///< Get a valid unit-type integer value.
    float         ValidUnitFloat (float defaultVal = 0.0, TTicks *retTimeStamp = NULL)  { return ValidFloat (defaultVal, retTimeStamp); }
      ///< Get a valid unit-type float value.
    int           ValidEnumIdx (int defaultVal = 0, TTicks *retTimeStamp = NULL)        { return ValidInt (defaultVal, retTimeStamp); }
      ///< Get a valid enumeration value index.

#endif

    /// @name (App) Subscriptions ...
    /// The usual way to query values is to place a subscription.
    ///
    /// As long as the subscription exists, up-to-date values will be forwarded to the application by
    /// means of a callback function or an event mechanism as defined by the @ref CRcSubscriber class.
    ///
    /// In the case of 'rceValueStateChanged' events, the returned event reflects the exact state of the resource
    /// at the time the event was caused. This important to not miss any event (e.g. 0-1-0 transitions).
    /// In contrast, if the actual state and value are queried using the 'Get...' methods above, the state
    /// at the time of the 'Get...' call, which may have changed again since the time of the event.
    /// This way, more up-to-date information may be obtained, but state/value transitions may be missed.
    ///
    /// @{
    CResource *Subscribe (CRcSubscriber *subscr) { SubscribePAL (subscr); return this; }
      ///< Subscribe to this resource. The caller remains owner of 'subscr'.
      /// If the present value of 'this' is known, an initial notification with the current value
      /// is sent to the subscriber.
      /// 'this' is returned to allow short constructs like "rc = RcGetResource (...)->Subscribe (subscr)".
    void Unsubscribe (CRcSubscriber *subscr) { UnsubscribePAL (subscr); }
      ///< Unsubscribe this resource.
    /// @}

    /// @name (App) Placing requests ...
    /// In order to accomodate multiple concurrent "write" operations to the same resource,
    /// a concept of "requests" is applied here.
    ///
    /// Each participant can add (and later remove)
    /// a request for a certain value to the resource, and the system will always select the
    /// value of the request with the highest priority.
    /// It is legal to call this method with 'this == NULL'. (Only) in this case, 'rcUri' is evaluated and must contain a valid URI.
    ///
    /// @{
    void SetRequestFromObj (CRcRequest *_request);
#ifndef SWIG
    void SetRequest (CRcRequest *_request) { SetRequestFromObj (_request); }
      ///< Add or replace a request (generic version).
      /// @param _request must by a dynamically allocated object, which is handed over and must no longer be used by the caller afterwards.
    void SetRequest (CRcValueState *value, const char *reqGid = NULL, int priority = rcPrioNormal, TTicks t0 = 0, TTicks t1 = 0, TTicks hysteresis = 0);
      ///< Add or replace a request (generic value, direct parameters).
      /// @param reqGid identifies the request; it is legal to pass NULL, in which case the instance name is used.
      /// @param value specifies the requested value (only type and value are used; the state is ignored).
      /// @param priority is the request priority attribute.
      /// @param t0 is the activation (start) time attribute (optional).
      /// @param t1 is the expiration (stop) time attribute (optional).
      /// @param hysteresis is the hysteresis attribute (optional).
      ///
      /// The request attributes are explaind in the [*Home2L Book*](../home2l-book.pdf).
    void SetRequest (bool valBool,          const char *reqGid = NULL, int priority = rcPrioNormal, TTicks t0 = 0, TTicks t1 = 0, TTicks hysteresis = 0);
      ///< Add or replace a request (direct value and parameters).
    void SetRequest (int valInt,            const char *reqGid = NULL, int priority = rcPrioNormal, TTicks t0 = 0, TTicks t1 = 0, TTicks hysteresis = 0);
      ///< Add or replace a request (direct value and parameters).
    void SetRequest (float valFloat,        const char *reqGid = NULL, int priority = rcPrioNormal, TTicks t0 = 0, TTicks t1 = 0, TTicks hysteresis = 0);
      ///< Add or replace a request (direct value and parameters).
    void SetRequest (const char *valString, const char *reqGid = NULL, int priority = rcPrioNormal, TTicks t0 = 0, TTicks t1 = 0, TTicks hysteresis = 0);
      ///< Add or replace a request (direct value and parameters).
    void SetRequest (TTicks valTime,        const char *reqGid = NULL, int priority = rcPrioNormal, TTicks t0 = 0, TTicks t1 = 0, TTicks hysteresis = 0);
      ///< Add or replace a request (direct value and parameters).
#endif // SWIG
    void SetRequestFromStr (const char *reqDef);
      ///< Set request by textual definition - see CRcRequest::SetFromStr() and the
      /// [*Home2L Book*](../home2l-book.pdf) for more information on the syntax of request definitions.

#ifndef SWIG
    void SetTrigger (const char *reqGid, int priority = rcPrioNormal, TTicks t0 = 0, TTicks t1 = 0, TTicks hysteresis = 0) { SetRequest (1, reqGid, priority, t0, t1, hysteresis); }
      ///< Set a trigger request (no value needs to be given).
#endif // SWIG
    void SetTriggerFromStr (const char *reqDef);
      ///< Set trigger request by textual definition.

    void DelRequest (const char *reqGid = NULL);
      ///< Delete a request.
      /// @param reqGid is the global request identifierr (GID) and identifies the request.
    /// @}

    /// @name (App) Emulate classical "read" and "write" operations ...
    /// The use of the following methods is not recommended.
    /// However, their implementations may be illustrative examples on how to work with subscriptions or requests.
    /// @{
#ifndef SWIG
    void ReadValueState (CRcValueState *retValueState, int maxTime = -1);
      ///< Perform a blocking read to obtain an up-to-date value from the driver.
      /// Internally, a temporary subscription is generated and the 'CRcSubscriber' methods are used
      /// to implement this reading.
#endif
    void WriteValue (CRcValueState value, int priority = rcPrioNormal, const char *reqGid = "rclib") { SetRequest (&value, reqGid, priority); }
      ///< Write a value by issueing or updating a request with the given global id.
      /// The implementation of this method can be used as a tutorial on working with requests.
    void UnWrite (const char *reqGid = "rclib") { DelRequest (reqGid); }
    /// @}


    // ***** For drivers *****

    /// @name (Drv) Registration and life cycle management of local resources ...
    /// @{

#ifndef SWIG
    static CResource *Register (CRcDriver *_rcDriver, const char *_lid, ERcType _type, bool _writable) { return Register (NULL, _rcDriver, _lid, _type, _writable); }
    static CResource *Register (CRcDriver *_rcDriver, const char *_lid, const char *rcTypeDef) { return Register (NULL, _rcDriver, _lid, rcTypeDef); }
      ///< Register a new local resource for a driver.
      /// Returns 'NULL' on error and prints a warning.
      /// 'rcTypeDef' is a textual type definition, syntax: `<type> (ro|wr)`
      ///
      /// **Note:** For the automatic documentation system, invocations of this method must be documented as follows:
      /// - If a default value is to be set, the SetDefault() call must follow in the next line.
      /// - The next line must be a comment of the form: `/ * [RC:<driver>] <brief description>`
      /// - If a long description is supplied, the next line must be: " *"
      ///   ... then the description follows in lines starting with "*".
      /// - The comment block must end with a line starting with "* /" (end of comment - ignore the space here).
      /// - To explicitly exclude the resource from documentation, place a line
      ///   with "/ * [RC:-] * /"  (end/start of comment - ignore the spaces here) below call.
#endif

    void SetDefault (CRcValueState *vs)     { SetRequest (vs, "_default", rcPrioDefault); }
      ///< Set a default value.
    void SetDefault (bool valBool)          { SetRequest (valBool,    "_default", rcPrioDefault); }
    void SetDefault (int valInt)            { SetRequest (valInt,     "_default", rcPrioDefault); }
    void SetDefault (float valFloat)        { SetRequest (valFloat,   "_default", rcPrioDefault); }
    //~ void SetDefault (double valFloat)       { SetRequest ((float) valFloat, "_default", rcPrioDefault); }
    void SetDefault (const char *valString) { SetRequest (valString,  "_default", rcPrioDefault); }
    void SetDefault (TTicks valTime)        { SetRequest (valTime,    "_default", rcPrioDefault); }

    void Unregister ();
      ///< Unregister a resource.
      /// Unused 'CResource' objects are never deleted from heap, but only marked as unregistered
      /// and its state set to 'rcsUnknown', if they are no longer used.

    /// @}

    /// @name (Drv) User data for the driver ...
    /// @{
    void *DriverData () { return rcDriverData; }
    void SetDriverData (void *_data) { rcDriverData = _data; }
    /// @}

    /// @name (Drv) Reporting values from the real device ...
    /// To be called by a driver to report a change in the real device.
    ///
    /// *Note:* The methods 'Report...' and 'DriveValue' serve as serialization points on the data paths
    /// Driver -> Subscribers and Subscribers -> Driver, respectively. The internal methods filter out duplicates
    /// reported one immediately after the other. Neither a driver nor a subscriber have to worry about duplicates.
    ///
    /// @{

    void ReportValueState (CRcValueState *_valueState);
      ///< Report value and state; if different from previous value/state, notify all subscribers.
    void ReportValue (URcValue _value, ERcState _state = rcsValid);
      ///< Report value and optionaly state (see 'ReportValueState')
    void ReportValue (bool _value, ERcState _state = rcsValid);
    void ReportValue (int _value, ERcState _state = rcsValid);
    void ReportValue (float _value, ERcState _state = rcsValid);
    void ReportValue (const char *_value, ERcState _state = rcsValid);
    void ReportValue (TTicks _value, ERcState _state = rcsValid);
    void ReportUnknown () { CRcValueState vs (Type ()); ReportValueState (&vs); }
    void ReportTrigger () { ReportValueState (NULL); }
      ///< For triggers, this method must be called.

    /// @}


    // ***** For info & directory services *****

    /// @name (Meta) For info & directory services ...
    /// @{

    void GetInfo (CString *ret, int verbosity = 1, bool allowNet = true);
      ///< Get textual information on this resource.
      /// 'verbosity == 0': single line, 1: list pending requests and subscriptions
      /// By default ('allowNet = true'), the method is network transparent, and for remote resources, its host is contacted.
      /// With 'allowNet = false', not network operation is performed and the locally available information is shown (for diagnostic purposes).
    void PrintInfo (FILE *f = stdout, int verbosity = 1, bool allowNet = true);

    int LockSubscribers ();
      ///< Return number of all active subscribers for a resource and locks this object.
      /// During the locked state, other operations are also locked out, so the caller should
      /// use this carefully and unlock as soon as possible.
      /// On error, a warning is emitted, but all successfully read subscribers are returned.
    CRcSubscriber *GetSubscriber (int n);
    void UnlockSubscribers () { Unlock (); }

    int LockRequests ();
      ///< Return number of active requests for a resource and locks this object.
      /// During the locked state, other operations are also locked out, so the caller should
      /// use this carefully and unlock as soon as possible.
      /// On error, a warning is emitted, but all successfully read requests are returned.
    CRcRequest *GetRequest (int n);
    void UnlockRequests () { Unlock (); }

    /// @}

#ifndef SWIG

    /// @name (Meta/App) Properties for internal or special use (semi-static and dynamic) ...
    /// Unless specified otherwise, the data returned by the following methods is almost static ("semi-static") in the sense
    /// that it may change if and only if the object is (re-)registered or unregistered.
    /// @{

    const char *Gid () { return gid.Get (); }         ///< Global ID (presently equivalent to the URI) of the resource.
    const char *Lid () { return ATOMIC_READ (lid); }
      ///< Path relative to a) 'this's host (remote resource) or b) driver (local resource).
    class CRcHost *Host () { return ATOMIC_READ (rcHost); }
    class CRcDriver *Driver () { return ATOMIC_READ (rcDriver); }

    unsigned RegSeq () { return ATOMIC_READ (regSeq); }
      ///< The 'regSeq' number is incremented each time the resource is registered or unregistered.
      /// Reading it before and after a critical section allows a caller to check whether semi-static
      /// data may have changed during the critical section and eventually withdraw and repeat the action.

    bool IsRegistered () { return (ATOMIC_READ (regSeq) & 1) == 1; }
    void WaitForRegistration ();    ///< Wait until registered or a network timeout occured.
    bool HasSubscribers () { return ATOMIC_READ (subscrList) != NULL; }
      ///< Return 'true' iff this resource is subscribed (dynamic, *not* semi-static).
    bool HasRequests () { return ATOMIC_READ (requestList) != NULL; }
      ///< Return 'true' iff this has requests (dynamic, *not* semi-static).

    /// @}

#endif


    // ***** Private *****

    // Constructor/destructor (private!)...
    //   Only this class itself is allowed to create or destroy new objects.
    //   SWIG is the only reason why the con- & destructor are not declared private.
#ifndef SWIG  // for not offering the constructor in the API
    CResource ();
    ~CResource ();
#endif

  private:

#ifndef SWIG
    friend void CResourceRequestsTimerCallback (CTimer *, void *);
    friend bool RcSelectResources (const char *, int *, CResource ***, CKeySet *);
    friend class CRcSubscriber;
    friend class CRcHost;
#endif

    // Registration and life cycle management ...
    static CResource *GetUnregistered (const char *uri);
    void PutUnregistered ();

    static CResource *Register (CRcHost *_rcHost, CRcDriver *_rcDriver, const char *_lid, ERcType _type, bool _writable);
      // Generic & main registration method; used by all other 'Registration' variants
    static CResource *Register (CRcHost *_rcHost, CRcDriver *_rcDriver, const char *_lid, const char *rcTypeDef);
      // Register with textual description; used by full textual registration (below) and for the "external" driver
      // Syntax of 'rcTypeDef':   <type> (ro|wr)
      // returns 'NULL' on error and prints a warning
    static CResource *Register (const char *rcDef);
      // Register with full textual description; used by 'rc_core' for remote resources
      // Syntax: <URI> <type> (ro|wr)
      // returns 'NULL' on error and prints a warning

    // Value & state...
    const CRcValueState *ValueState () { return &valueState; }      // 'this' must be locked as long as the returned value is accessed

    // Reading values...
    void SubscribePAL (CRcSubscriber *subscr, bool resLocked = false, bool subLocked = false);
      // 'resLocked' and 'subLocked' must be set if a lock on the resource or subscription is already held.
      // The method may temporarily release a lock to avoid deadlocks.
    void UnsubscribePAL (CRcSubscriber *subscr, bool resLocked = false, bool subLocked = false);
      // 'resLocked' and 'subLocked' must be set if a lock on the resource or subscription is already held.
      // The method may shortly release a lock to avoid deadlocks.

    // Requests...
    void ClearRequestsAL ();
    static bool DoDelRequest (CRcRequest **pList, const char *reqGid);

    CRcRequest *GetWinningRequest (TTicks t);
    void EvaluateRequests ();     // checks and processes all pending requests

    // Drivers ...
    bool NotifySubscribers (int evType);
    bool NotifySubscribersAL (int evType);    // 'evType' is effectively of type 'ERcEventType'
    void ReportNetLost ();  // Report that the network connection to the server was lost (like 'ReportUnknown' but with different time stamp behaviour; see above).
    // ... The '...AL' variants assume that the resource has already been locked by the caller.

    void ReportValueStateAL (CRcValueState *_valueState, TTicks _timeStamp = 0);
    void ReportValueAL (URcValue _value, ERcState _state = rcsValid);
    void ReportUnknownAL () { CRcValueState vs (Type ()); ReportValueStateAL (&vs); }
    void ReportTriggerAL () { ReportValueStateAL (NULL); }

    // Driving values to the real device...
    void DriveValue (URcValue val);
      // Propagate a new requested value to the driver (to make that happen in the real device).

    // Helpers...
    void Lock () { mutex.Lock (); }
    void Unlock () { mutex.Unlock (); }


    // BEGIN static data (never changed after the initialization of the object)...
    //   No locking required.
    CString gid;                // aka URI

    // BEGIN semi-static data ...
    //   Fields marked with "[atomic]" must be accessed using the 'ATOMIC_*' macros on each write access and on read accesses if'this' is not locked.
    //   Fields marked with "[mutex]" may only be accessed if 'this' is locked.
    //   The consistency of all semi-static data as a whole is protected by the sequence lock 'regSeq'.

    // Resource identification...
    class CRcHost *rcHost;      // [atomic] 'NULL' => local resource or unregistered
    class CRcDriver *rcDriver;  // [atomic] 'NULL' => remote resource or unregistered
    void *rcDriverData;         // user data for the driver (driver cares for concurrent access)
    const char *lid;            // [atomic] local resources: relative path without driver; remote: with driver name as first component; points into 'gid.Get ()'.

    // Resource properties...
    unsigned regSeq;            // [atomic]
    bool writable;              // (not "atomic" since only one byte is relevant)

    // BEGIN dynamic data ...
    //   All fields may only be accessed if 'this' is locked.

    // Current value, its type, and its state...
    CRcValueState valueState;   // only state and value are dynamic; 'valueState.type' is semi-static (not "atomic" since only one byte is relevant)

    // Internal...
    //   'CResource' objects are managed by a 'CDict' associated with a driver (local resources) or
    //   host (remote resources).
    CMutex mutex;               // protects 'this' including the request list
    CRcRequest *requestList;
    CTimer requestTimer;        // timer for the next evaluation of requests
    CRcSubscriberLink *subscrList;
};


// Python extensions...
#ifdef SWIG
%extend CResource {
  %newobject __str__ ();
  const char *__str__ () { CString s; $self->ToStr (&s); return s.Disown (); }
  %newobject __repr__ ();
  const char *__repr__ () { CString s; s.SetF ("(CResource) %s", $self->ToStr ()); return s.Disown (); }
  bool __eq__ (CResource *other) { return $self == other; }
  bool __ne__ (CResource *other) { return $self != other; }

  %feature("docstring") ValueState "Get the current value and state of the resource."
  %newobject ValueState ();
  CRcValueState *ValueState () { CRcValueState *ret = new CRcValueState (); $self->GetValueState (ret); return ret; }

  %pythoncode %{
    pass    # (Workaround to keep SWIG from scrambling the indentation of the following code.)

    def Value (self):
      """Get a value ('None' if invalid) of this resource (use ValueState() to get the complete value and state)."""
      return self.ValueState ().Value ()

    def ValidValue (self, defaultVal):
      """Get a valid value of this resource (use ValueState() to get the complete value and state)."""
      return self.ValueState ().ValidValue (defaultVal)

    def SetRequest (self, value, id = None, priority = None, t0 = None, t1 = None, hysteresis = None):
      """Set a request with selected attributes."""
      req = CRcRequest ()
      req.Set (value, id, priority, t0, t1, hysteresis)
      req.thisown = 0   # tell swig to disown the object
      self.SetRequestFromObj (req)

    def SetTrigger (self, id = None, priority = None, t0 = None, t1 = None, hysteresis = None):
      """Set a request for a trigger resource with selected attributes."""
      req = CRcRequest ()
      req.Set (1, id, priority, t0, t1, hysteresis)
      req.thisown = 0   # tell swig zo disown the object
      self.SetRequestFromObj (req)
  %}
};  // %extend CResource
#endif


/// @}  // resources_rc
#ifdef SWIG
%pythoncode %{
## @}
%}
#endif // SWIG





// *************************** Events ******************************************


/** @addtogroup resources_subscribe
 *
 * Event model
 * -----------
 *
 * The event model allows even remote hosts to securely catch all events
 * reported by the driver. This is important, if a sensor (e.g. a push button)
 * may change its value very quickly, but event quick changes need to be processed.
 * If this property cannot be guaranteed (e.g. because a host is temporarily down or
 * the network connection lost), evens of type 'retDisconnected' and hopefully later
 * 'retConnected' are dispatched, so that the subscriber is informed about this situation.
 *
 * @{
 */
#ifdef SWIG
%pythoncode %{
## @addtogroup resources_subscribe
## @{
%}
#endif // SWIG


/// Event type
enum ERcEventType {
  rceNone = 0,                ///< None (for dummy events)

  rceTimer,                   ///< [timer] Timer alarm

  rceValueStateChanged,       ///< [subscriber] Value or state has changed
  rceDisconnected,            ///< [subscriber] Connection to resource was lost
  rceConnected,               ///< [subscriber] Connection to resource is (re-)established

  rceDriveValue               ///< [driver] Drive a new value
};



// ***** CRcEvent *****


/** Events delivered by a subscriber or to a driver.
 *
 * @nosubgrouping
 */
#ifdef SWIG
%feature("docstring") CRcEvent "Resource events delivered by a subscriber or to a driver.\n\n"
  "Possible event types are:\n"
  "  rceTimer:             Timer alarm.\n"
  "  rceValueStateChanged: The resource has changed its value.\n"
  "  rceDisconnected:      The connection to the (remote) resource has\n"
  "                        been lost.\n"
  "  rceConnected:         The connection to the (remote) resource has\n"
  "                        been established (again).\n"
  "  rceDriveValue:        Drive a value (for drivers).\n"
  "  rceNone:              Nothing (dummy event: igore).\n"
  "The attribute 'MorePending ()' indicates whether more events are waiting for the\n"
  "same subscriber. This can be used for performance optimizations to avoid\n"
  "unecessary recomputations."
#endif
class CRcEvent {
  public:
    CRcEvent (ERcEventType _type = rceNone, CResource *_resource = NULL, CRcValueState *_valueState = NULL, void *_data = NULL) { Set (_type, _resource, _valueState, _data); }

    /// @name Setting attributes ...
    /// @{
    void Clear () { Set (); }
    void Set (ERcEventType _type = rceNone, CResource *_resource = NULL, CRcValueState *_valueState = NULL, void *_data = NULL);

    void SetType (ERcEventType _type) { type = _type; }
    void SetResource (CResource *_resource) { resource = _resource; }
    void SetValueState (CRcValueState *_valueState);
    void SetData (void *_data) { data = _data; }
    /// @}

    /// @name Getting attributes ...
    /// @{
    ERcEventType Type () { return type; }
    CResource *Resource () { return resource; }
    CRcValueState *ValueState () { return &valueState; }
    void *Data () { return data; }
    /// @}

    /// @name Stringification ...
    /// @{
    const char *ToStr (CString *ret); ///< Get a readable string.
    const char *ToStr ();             ///< (TTS) Get a readable string.
    /// @}

  protected:
    friend class CRcEventProcessor;

    ERcEventType type;          // event type
    CResource *resource;
    CRcValueState valueState;   // value state for 'rceValueStateChanged' / value for 'rceDriveValue'
    void *data;                 // user data

    bool morePending;   // After 'PollEvent'/'WaitEvent' indicates whether more events are pending for
                        // the current subscriber to allow for optimized processing afterwards.
                        // Note: This should only be used if either all or no events are processed by callbacks.

    CRcEvent *next;
};


// Python extension...
#ifdef SWIG
%extend CRcEvent {
  %newobject __str__ ();
  const char *__str__ () { CString s; $self->ToStr (&s); return s.Disown (); }
  %newobject __repr__ ();
  const char *__repr__ () { CString s; s.SetF ("(CRcEvent) %s", $self->ToStr ()); return s.Disown (); }
}
#endif



// ***** CRcEventProcessor *****


typedef bool FRcEventFunc (class CRcEventProcessor *, CRcEvent *, void *);
  ///< Event processor callback function.
  /// Example: `bool OnEventFunc (CRcEventProcessor *ep, CRcEvent *ev, void *data);`

/** Event processor
 *
 * This class allows to queue and process events in three different ways:
 *
 * 1. Synchronously by calling PollEvent() or WaitEvent().
 * 2. Asynchronously by overloading the OnEvent() method.
 * 3. Asynchronously by providing a callback function.
 *
 * @nosubgrouping
 */
class CRcEventProcessor {
  public:
    CRcEventProcessor (bool _inSelectSet = false);
    virtual ~CRcEventProcessor ();

    /// @name Putting events ...
    /// @{
    void PutEvent (CRcEvent *ev);     ///< Enqueue/process an event; caller remains owner of 'ev'.
    /// @}

#ifndef SWIG

    /// @name Fetching events synchronously...
    /// @{
    bool PollEvent (CRcEvent *ev);
      ///< Check for an event (non-blocking) and return it at '*ev'.
      ///
      /// A return value of 'true' indicates that an event was waiting and (if 'ev != NULL') is now removed from the internal queue.
      /// If 'ev != NULL', this event is returned. Otherwise, the queue is left unchanged.
      ///
      /// Can be called from any thread: the user does not need to take
      /// synchronisation measures by calling these from his main thread.
      ///
    bool WaitEvent (CRcEvent *ev, int *maxTime = NULL);
      ///< Wait for an event and return it at '*ev'.
      ///
      /// A return value of 'true' indicates that an event was waiting and (if 'ev != NULL') is now removed from the internal queue.
      /// If 'ev != NULL', this event is returned. Otherwise, the queue is left unchanged.
      ///
      /// If 'maxTime != NULL', wait for at most '*maxTime' milliseconds and decrement '*maxTime' by the time
      /// actually waited. This way, the method can be called multiple times while approximately limiting the
      /// total waiting time (Note: The total time can still be longer due to preemption and additional execution times).
      ///
      /// Can be called from any thread: the user does not need to take
      /// synchronisation measures by calling these from his main thread.
      ///
    void Interrupt ();     ///< Request a running WaitEvent() to stop now.
    /// @}

    /// @name Event callbacks (asynchronous) ...
    /// Event callbacks are called asynchronously immediately on each invocation 'PutEvent'.
    ///
    /// Unlike the PollEvent() / WaitEvent() mechanism, the callback may be called from any thread, and the user is responsible
    /// for any necessary synchronisation with other threads. If and only if the callback returns 'true', the event will be
    /// enqueued for later 'PollEvent'/'WaitEvent' calls. This way, it is possible to do either all or just part
    /// of the work in the callback (e.g. wake up the polling thread). The default implementation just returns 'false',
    /// so that all events are queued up.
    ///
    /// It is not allowed to call any CRcEventProcessor() methods from the callback!
    ///
    /// @{
    virtual bool OnEvent (CRcEvent *ev);
    void SetCbOnEvent (FRcEventFunc *_cbEvent, void *_cbEventData = NULL);
    void ClearCbOnEvent () { SetCbOnEvent (NULL, NULL); }
    /// @}

#endif // SWIG

    /// @name Support for global event loops ...
    /// @{
    void SetInSelectSet (bool _inSelectSet);
      ///< Set whether or not the event processor is contained in the set of upcoming Select() operations.
    bool InSelectSet () { return inSelectSet; }

    static CRcEventProcessor *Select (int maxTime = -1);
      ///< Wait until *any* of the existing 'CRcEventProcessor' object has an event available.
      /// Returns an event processor with waiting events, or 'NULL' if 'maxTime' was exceeded when waiting.
      /// **Note:** It is mandatory that at least one (better: all) event is actually consumed afterwards,
      ///     which implies that the caller is able to process/delegate events for all of the existing
      ///     event processors. If no event is consumed, a busy waiting loop can result.
      ///     Subscribers with disabled event queues are never returned here.
      ///     In general, any internal event processors (e.g. drivers) should have working callbacks.
      ///     For the main program it is then safe, but necessary to run "ASSERT (ep->PollEvent (...))" on
      ///     unknown event owners.
    // TBD: Implement 'InterruptSelect' to interrupt a waiting 'Select'

    virtual const char *TypeId () { return CString::emptyStr; }   ///< (optional) Hint for the main event loop: object type; TTS is not allowed
    virtual const char *InstId () { return CString::emptyStr; }   ///< (optional) Hint for the main event loop: object instance; TTS is not allowed
    /// @}

    /// @name Stringification ...
    /// @{
    const char *ToStr (CString *ret) { return StringF (ret, "%s:%s", TypeId (), InstId ()); }
    const char *ToStr () { return ToStr (GetTTS ()); }    ///< (TTS)
    /// @}

  private:

    // Internal helpers ...
    bool DoPollEventAL (CRcEvent *ev);

    void DeleteFirstEventAL ();

    void LinkAL ();
    void UnlinkAL ();
    bool IsLinkedAL () { return next || pLastProc == &next; }

    // Dynamic data (protected by 'globMutex')...
    static CMutex globMutex;        // protects dynamic data of all event processors
    static CCond globCond;          // global condition variable for 'Select'
    CCond cond;                     // per-object condition variable for 'WaitEvent', mutex is 'globMutex'
    volatile bool interrupted;      // used (only) in 'WaitEvent' and 'Interrupt'

    FRcEventFunc *cbEvent;
    void *cbEventData;

    CRcEvent *firstEv, **pLastEv;   // linked list with efficient appending

    bool inSelectSet;
    static CRcEventProcessor *firstProc, **pLastProc;     // linked list of event processors with pending events
    CRcEventProcessor *next;        // 'next' pointer for 'firstProc'/'pLastProc' list
};


// Python extension...
#ifdef SWIG
%extend CRcEventProcessor {
  %newobject __str__ ();
  const char *__str__ () { CString s; $self->ToStr (&s); return s.Disown (); }
  %newobject __repr__ ();
  const char *__repr__ () { CString s; s.SetF ("(CRcEventProcessor) %s", $self->ToStr ()); return s.Disown (); }

  %newobject PollEvent ();
  CRcEvent *PollEvent () {
    CRcEvent ev, *ret;
    if (!$self->PollEvent (&ev)) return NULL;
    ret = new CRcEvent ();
    *ret = ev;
    //~ INFOF (("### Python: Pollevent () -> %08x", ret));
    return ret;
  }
};  // %extend CRcEventProcessor
#endif





// ***** CRcEventTimer *****


#ifdef SWIG
%warnfilter(401) CRcEventTimer;     // class 'CTimer' is not known to SWIG
#endif

/// Timer that sends 'rceTimer' events over the event system.
class CRcEventTimer: public CTimer, public CRcEventProcessor {
  public:
    CRcEventTimer (const char *_id, void *_data = NULL) { id.Set (_id); Set (_data); }
#ifndef SWIG
    CRcEventTimer (TTicksMonotonic _time, TTicksMonotonic _interval = 0, void *_data = NULL) { Set (_time, _interval, _data); }
#endif
    CRcEventTimer (TTicks _time, TTicks _interval = 0, void *_data = NULL) { Set (_time, _interval, _data); }

    void Set (void *_data) { CTimer::Set (NULL, _data, NULL); }
      ///< Setup timer without (re-)scheduling it.
#ifndef SWIG
    void Set (TTicksMonotonic _time, TTicksMonotonic _interval = 0, void *_data = NULL) { CTimer::Set (_time, _interval, NULL, _data, NULL); }
#endif
    void Set (TTicks _time, TTicks _interval = 0, void *_data = NULL) { CTimer::Set (TicksToMonotonic (_time), (TTicksMonotonic) _interval, NULL, _data, NULL); }
      ///< Setup and (re-)schedule timer.

    virtual void OnTime () { CRcEvent ev (rceTimer, NULL, NULL, data); PutEvent (&ev); }

    virtual const char *TypeId () { return "T"; }         // hint for Python/SWIG API
    virtual const char *InstId () { return id.Get (); }   // hint for Python/SWIG API

  protected:
    CString id;
};


/// @}  // resources_subscribe
#ifdef SWIG
%pythoncode %{
## @}
%}
#endif // SWIG





// *************************** Subscribers *************************************


/** @addtogroup resources_subscribe
 *
 * @{
 */
#ifdef SWIG
%pythoncode %{
## @addtogroup resources_subscribe
## @{
%}
#endif // SWIG



/** Subscriber to follow an arbitrary set of resources.
 *
 * A subscriber monitors any number of resources. The resources can be
 * specified by their real URIs or alias names. Wildcards as well as
 * presently unkown resources are allowed. Events can be fetched using
 * the @ref CRcEventProcessor mechanisms, typically Select() and PollEvent().
 *
 * **Note:** All public methods must be called from the same thread.
 * Subscribers are generally managed by the user/owner object. Internally, each remote host object
 * owns one subscriber to serve subscriptions from the remote host to local resources.
 *
 * @nosubgrouping
 */
#ifdef SWIG
%feature("docstring") CRcSubscriber "Subscription to any number of resources.\n\n"
  "A subscriber monitors any number of resources. The resources can be\n"
  "specified by their real URIs or alias names. Wildcards as well as\n"
  "presently unkown resources are allowed. Events can be fetched using\n"
  "'Select ()' and 'PollEvent'.\n"
#endif
class CRcSubscriber: public CRcEventProcessor {    // subscription to any number of resources
  public:
    CRcSubscriber () { resourceList = NULL; }     ///<
    CRcSubscriber (const char *_lid) { resourceList = NULL; Register (_lid); }
    virtual ~CRcSubscriber () { Unregister (); }

    /// @name Registration ...
    /// @{
    void Register (const char *_lid);
      ///< Enable the subscriber, set its LID and GID. Caller remains owner of object.
    void Unregister ();
      ///< Clear and unregister the subscriber.
    /// @}

    /// @name Properties and stringification ...
    /// @{
    const char *Lid () { return lid.Get (); }
    const char *Gid () { return gid.Get (); }

    virtual const char *TypeId () { return "S"; }     // hint for Python/SWIG API
    virtual const char *InstId () { return Lid (); }  // hint for Python/SWIG API

    const char *ToStr (CString *ret);
    const char *ToStr ();     ///< (TTS)

    bool IsEmpty () { return resourceList == NULL && watchSet.Entries () == 0; }
    /// @}

    /// @name Adding/removing resources ...
    /// @{
    CResource *AddResource (CResource *rc);
    CResource *AddResources (const char *pattern);
      ///< Add new resources by pattern. The pattern is also stored internally to catch possible resources added in the future.
      /// Returns the resource, if 'pattern' does not contain wildcards and refers to a single valid URI, or 'NULL' otherwise.
      /// It is allowed to pass multiple, comma-separated URIs or patterns here.
    void DelResource (CResource *rc);
    void DelResources (const char *pattern);
      ///< Remove resources by pattern.
      /// **Note:** When using wildcards, all patterns previously passed to AddResources() must either be covered
      ///   completely (or be identical), or must not have any common matching name with 'pattern'. For example,
      ///   it is NOT allowed to first add the pattern "a*" and later delete "*b".
    void Clear ();
      ///< Remove all resources.
    /// @}

    /// @name Synonyms for adding/removing resources (mainly for the Python wrappers) ...
    /// @{
    CResource *Subscribe (CResource *rc) { return AddResource (rc); }
    CResource *Subscribe (const char *uri) { return AddResources (uri); }
    void Unsubscribe (CResource *rc) { DelResource (rc); }
    void Unsubscribe (const char *pattern) { DelResources (pattern); }
    /// @}

#ifndef SWIG

    /// @name For directory services ...
    /// @{
    void GetInfo (CString *ret, int verbosity = 1);
      ///< Get Info. 'verbosity == 0': single line, 1: list entries / resources.
    static void GetInfoAll (CString *ret, int verbosity = 1);
      ///< Get info for all subscribers of this instance (see GetInfo() for info on 'verbosity').
    void PrintInfo (FILE *f = stdout, int verbosity = 1);

    void GetPatternSet (CKeySet *retPatternSet);    ///< Patterns from the watch set are followed by a "?"
    /// @}

  protected:
    friend class CResource;
    friend class CRcServer;

    // Locking...
    void Lock () { mutex.Lock (); } // INFOF (("# Thread #%08x: CRcSubscriber::Lock ()", pthread_self ()));
    void Unlock () { mutex.Unlock (); } // INFOF (("# Thread #08x: CRcSubscriber::Unlock ()", pthread_self ()));

    // Notifications from resource...
    void CheckNewResource (CResource *resource);    // check if new resource fits a watch pattern and eventually adds it
    void UnlinkResourceAL (CResource *resource);    // remove a resource and adds its name to the watch set (for temporarilly unregistered resources); Assumes that 'resource' is already locked for us.
    void NotifyAL (CRcEvent *ev) { PutEvent (ev); } // process an event from a resource; caller remains owner of 'ev'

    // Interaction with network server...
    void RegisterAsAgent (const char *_gid);
      // Alternative to 'Register' - register as an agent for some other (remote client) host.
    bool IsAgent () { return lid.Get () == gid.Get (); }    // subscriber is an agent

    // Static data...
    CString lid, gid;

    // Dynamic data (protected by the mutex)...
    CMutex mutex;
    CResourceLink *resourceList;
    CKeySet watchSet;     // contains URI patterns to be checked if new resources are registered

#endif // #ifndef SWIG

};


/// @}  // resources_subscribe
#ifdef SWIG
%pythoncode %{
## @}
%}
#endif // SWIG





// *************************** Requests ****************************************


/// @addtogroup resources_requests
/// @{
#ifdef SWIG
%pythoncode %{
## @addtogroup resources_requests
## @{
%}
#endif // SWIG


#define RCREQ_NONE INT_MAX     ///< Special value meaning "none" for the request attributes: priority, t0, t1, hysteresis

/** Request description object
 *
 * Request objects define the request for a certain value together with some
 * additional attributes. The concept of requests is introduced to deal with
 * potentially concurrent write accesses to resources. Besides the requested
 * value, a request may have some optional attributes set, which are explaind
 * in detail in the [*Home2L Book*](../home2l-book.pdf).
 *
 * @nosubgrouping
 */
#ifdef SWIG
%feature("docstring") CRcRequest "Request specifications.\n\n"
  "Request objects define the request for a certain value together with some\n"
  "additional attributes. The concept of requests is introduced to deal with\n"
  "potentially concurrent write accesses to resources. Besides the requested\n"
  "value, a request has the following attributes:\n"
  "  - a unique GID by which it can be later be changed or deleted.\n"
  "  - a priority (see also: comments on the 'prio*' constants)\n"
  "  - a starting time (optional)\n"
  "  - an expiration time (optional)\n"
  "  - a hysteresis to avoid too frequent value changes (optional)"
#endif
class CRcRequest {
  public:
    CRcRequest () { Reset (); }   ///< Default constructor
#ifndef SWIG
    CRcRequest (CRcValueState *_value, const char *_gid = NULL, int _priority = RCREQ_NONE, TTicks _t0 = RCREQ_NONE, TTicks _t1 = RCREQ_NONE, TTicks _hysteresis = RCREQ_NONE) { Reset (); Set (_value, _gid, _priority, _t0, _t1, _hysteresis); }
      ///< Construct with value and optional attributes.
    CRcRequest (bool        _value, const char *_gid = NULL, int _priority = RCREQ_NONE, TTicks _t0 = RCREQ_NONE, TTicks _t1 = RCREQ_NONE, TTicks _hysteresis = RCREQ_NONE) { Reset (); Set (_value, _gid, _priority, _t0, _t1, _hysteresis); }
      ///< Construct with value and optional attributes.
    CRcRequest (int         _value, const char *_gid = NULL, int _priority = RCREQ_NONE, TTicks _t0 = RCREQ_NONE, TTicks _t1 = RCREQ_NONE, TTicks _hysteresis = RCREQ_NONE) { Reset (); Set (_value, _gid, _priority, _t0, _t1, _hysteresis); }
      ///< Construct with value and optional attributes.
    CRcRequest (float       _value, const char *_gid = NULL, int _priority = RCREQ_NONE, TTicks _t0 = RCREQ_NONE, TTicks _t1 = RCREQ_NONE, TTicks _hysteresis = RCREQ_NONE) { Reset (); Set (_value, _gid, _priority, _t0, _t1, _hysteresis); }
      ///< Construct with value and optional attributes.
    CRcRequest (const char *_value, const char *_gid = NULL, int _priority = RCREQ_NONE, TTicks _t0 = RCREQ_NONE, TTicks _t1 = RCREQ_NONE, TTicks _hysteresis = RCREQ_NONE) { Reset (); Set (_value, _gid, _priority, _t0, _t1, _hysteresis); }
      ///< Construct with value and optional attributes.
    CRcRequest (TTicks      _value, const char *_gid = NULL, int _priority = RCREQ_NONE, TTicks _t0 = RCREQ_NONE, TTicks _t1 = RCREQ_NONE, TTicks _hysteresis = RCREQ_NONE) { Reset (); Set (_value, _gid, _priority, _t0, _t1, _hysteresis); }
      ///< Construct with value and optional attributes.
#endif // SWIG

    ~CRcRequest () {}

    /// @name Setting properties ...
    /// @{

    void Reset ();
      ///< Clear the value and set default attributes.

    void Set (CRcValueState *_value, const char *_gid = NULL, int _priority = RCREQ_NONE, TTicks _t0 = RCREQ_NONE, TTicks _t1 = RCREQ_NONE, TTicks _hysteresis = RCREQ_NONE);
      ///< Define a request with all parameters and attributes.
      /// @param _value is the requested value; Only the type and value are used, the state does not have any effect.
      /// @param _gid is the user-defined global ID (GID);  If 'NULL', the instance name is used as the GID.
      /// @param _priority is the request priority attribute.
      /// @param _t0 is the activation (on/start) time attribute (optional).
      /// @param _t1 is the expiration (off/stop) time attribute (optional).
      /// @param _hysteresis is the hysteresis attribute (optional).
      ///
      /// The request attributes are explaind in the [*Home2L Book*](../home2l-book.pdf).
      /// Attributes left with the defaults of 'RCREQ_NONE' or 'NULL' are not changed.
      /// Negative values t are interpreted as a time relative from now in the future (= t_now - t).
#ifndef SWIG
    void Set (bool        _value, const char *_gid = NULL, int _priority = RCREQ_NONE, TTicks _t0 = RCREQ_NONE, TTicks _t1 = RCREQ_NONE, TTicks _hysteresis = RCREQ_NONE);
    void Set (int         _value, const char *_gid = NULL, int _priority = RCREQ_NONE, TTicks _t0 = RCREQ_NONE, TTicks _t1 = RCREQ_NONE, TTicks _hysteresis = RCREQ_NONE);
    void Set (float       _value, const char *_gid = NULL, int _priority = RCREQ_NONE, TTicks _t0 = RCREQ_NONE, TTicks _t1 = RCREQ_NONE, TTicks _hysteresis = RCREQ_NONE);
    void Set (const char *_value, const char *_gid = NULL, int _priority = RCREQ_NONE, TTicks _t0 = RCREQ_NONE, TTicks _t1 = RCREQ_NONE, TTicks _hysteresis = RCREQ_NONE);
    void Set (TTicks      _value, const char *_gid = NULL, int _priority = RCREQ_NONE, TTicks _t0 = RCREQ_NONE, TTicks _t1 = RCREQ_NONE, TTicks _hysteresis = RCREQ_NONE);
#endif // SWIG

    void SetValue (CRcValueState *_value);                                ///< Set value (argument must be != NULL).
    void SetValue (bool _value);
    void SetValue (int _value);
    void SetValue (float _value);
    void SetValue (const char *_value);
    void SetValue (TTicks _value);

    void SetGid (const char *_gid) { gid.Set (_gid); }                    ///< Set GID (argument must be != NULL).

    void SetPriority (int _priority) { priority = _priority; }            ///< Set GID (argument must be != RCREQ_NONE).
    void SetTimeOn (TTicks _t0) { t0 = _t0; }                                ///< Set on time (argument must be != RCREQ_NONE).
    void SetTimeOff (TTicks _t1) { t1 = _t1; }                               ///< Set off time (argument must be != RCREQ_NONE).
    void SetHysteresis (TTicks _hysteresis) { hysteresis = _hysteresis; } ///< Set hysteresis (argument must be != RCREQ_NONE).

    /// @}

    /// @name Type conversion/setting ...
    /// @{
    void Convert (CResource *rc);
      ///< Try to convert type to match the given resource and set 'isCompatible' according to the success.
      /// On failure (incompatible types), a warning is emitted.
    /// @}

    /// @name Getting properties ...
    /// @{

    bool IsCompatible () { return isCompatible; }
    const char *Gid () { return gid.Get (); }
      ///< Requests only have a global ID, no local ID, since they do not "belong" to the initiating host or process.
      /// Based on the GID, the request can be set, changed or removed freely from any host or process in the *Home2L* cluster.
    const char *Origin () { return origin.Get (); }
      ///< Origin of the request.
      /// It is automatically set by Reset() to the request creation information
      /// in the form "<hostname>:<process name>[<pid>] <yyyy-mm-dd-hhmmss>".
      ///

    ERcType Type () { return value.Type (); }
    CRcValueState *Value () { return &value; }
    int Priority () { return priority; }
    TTicks TimeOn () { return t0; }
    TTicks TimeOff () { return t1; }
    TTicks Hysteresis () { return hysteresis; }

    /// @}

    /// @name String-based access ...
    /// @{

    bool SetFromStr (const char *str);
      /**< Set the request based on a textual definition.
       *
       * Syntax: `<value without type> [<attributes>]`
       *
       * The request options `<attributes>` may be a combination of the following:
       * - `#<id>  ` : Request ID [default: instance name]
       * - `*<prio>` : Priority (0..9) [default: 5]
       * - `+<time>` : Start time
       * - `-<time>` : End time
       *              `<time>` may be an absolute date/time in the format `YYYY-MM-DD-HHMM[SS[.frac]]`
       *                      or a relative time `<n>`, where `<n>` is the number of milliseconds in the future.
       *              See TicksFromString() for details and further options.
       * - `~<hyst>` : Hysteresis in milliseconds
       * - `@<origin>` : Origin in the form `<host>/<issue time>`; Should not be set manually.
       *
       * If some parameter is specified multiple times, the last occurence will overwrite the others.
       * Unspecified attributes are left unchanged.
       *
       * On success, 'true' is returned. On error, a warning is emmited.
       * The type is auto-detected.
       */
    const char *ToStr (CString *ret, bool beautiful = false, TTicks relativeTimeThreshold = 0);
      ///< Return a string in the format accepted by SetFromStr().
      ///
      /// If 't0' or 't1' are defined, in the future, and in less then 'relativeTimeThreshold' milliseconds
      /// from now, the time settings are output relative to the current time. This is useful if the
      /// request string is to be transmitted to another computer with a slightly different local time.
      /// (Example: A door opener request is timed for 1 second. If the server's local clock deviates from
      /// the local one by more than one second, this request may be completely ignored.)
    const char *ToStr (bool beautiful = false, TTicks relativeTimeThreshold = 0);
      ///< (TTS) Return a string in the format accepted by SetFromStr().

    /// @}

  protected:
    friend class CResource;

    void SetOrigin ();

    // Fields...
    bool isCompatible;
    CString gid, origin;
    CRcValueState value;
    int priority;
    TTicks t0, t1;                // start and end time (0 = not set); t0 == t1 => value is set once (and may then stay)
    TTicks hysteresis;            // hysteresis in milliseconds

    // Internal fields...
    //   Requests are managed as a linked list associated with a 'CResource' object.
    CRcRequest *next;
};


// Python extension...

#ifdef SWIG
%extend CRcRequest {

  %newobject __str__ ();
  const char *__str__ () { CString s; $self->ToStr (&s); return s.Disown (); }
  %newobject __repr__ ();
  const char *__repr__ () { CString s; s.SetF ("(CRcRequest) %s", $self->ToStr ()); return s.Disown (); }
  %pythoncode %{
    pass    # (Workaround to keep SWIG from scrambling the indentation of the following code.)

    ## Versatile setter.
    def Set (self, value = None, id = None, priority = None, t0 = None, t1 = None, hysteresis = None):
      """Set selected request attributes."""
      if value != None: self.SetValue (value)
      if id: self.SetGid (id)
      if priority: self.SetPriority (priority)
      if t0:
        ticks = TicksAbsOf (t0)
        if ticks: self.SetTimeOn (ticks)
        else: print ("WARNING: Invalid time specification '" + str(t0) + "' - ignoring!")
      if t1:
        ticks = TicksAbsOf (t1)
        if ticks: self.SetTimeOff (ticks)
        else: print ("WARNING: Invalid time specification '" + str(t1) + "' - ignoring!")
      if hysteresis: self.SetHysteresis (hysteresis)

  %} // %pythoncode
};  // %extend CRcRequest
#endif


/// @}  // resources_requests
#ifdef SWIG
%pythoncode %{
## @}
%}
#endif // SWIG





// *************************** Drivers *****************************************


/** @addtogroup resources_drivers
 *
 * Defining an own driver can be done in three ways:
 *
 * 1. Deriving an own class from @ref CRcDriver and override the interface methods (see "protected:" section).
 * 2. Instantiating an @ref CRcDriver object and passing a driver function to CRcDriver::Register() (function-based driver).
 * 3. Instantiating an event-based driver (see @ref CRcEventDriver; only recommended for high-level language bindings).
 *
 * There is a clear distinction between an initialization phase and an active phase.
 * All resources must be registered in the initialization phase (Operation 'rcdOpInit').
 * 'rcdOpDriveValue' operations can only occur in the active phase.
 *
 * @{
 */


#ifndef SWIG


/// Driver operations
enum ERcDriverOperation {

  // For linking in C drivers...
  rcdOpInit = 0,      ///< Initialize driver object(s) of a linked-in driver.
    /// Parameter: (CRcDriver *drv)
    ///
    /// 'drv' is a 'CRcDriver' (base class) object pre-initialized and registered with a LID and 'FRcDriverFunc'
    /// according to the driver declaration table. For function-based driver which implements
    /// 'rcdStop' and 'rcdOpDriveValue' accordingly, everything is fine now.
    /// If the driver wants to use its own objects with classes derived from 'CRcDriver', it must delete 'drv' and
    /// may then create and register own driver(s).

  // Using a base-class 'CRcDriver' object to drive something...
  rcdOpStop,          ///< Stop the driver; refers to CRcDriver::Stop()
    /// Parameters: (CRcDriver *drv)
  rcdOpDriveValue     ///< Drive a value; refers to CRcDriver::DriveValue()
    /// Parameters: (CRcDriver *driver, CResource *rc, CRcValueState *vs)
};


typedef void FRcDriverFunc (ERcDriverOperation, CRcDriver *, CResource *, CRcValueState *);
  ///<  Resources driver function.
  ///
  /// Example: void DriverFunc_sample (int appOp, CRcDriver *driver, CResource *rc, CRcValueState *vs);


/// Define the entry point of a binary driver.
/// Example:
/// @code
///   HOME2L_DRIVER(foo) (ERcDriverOperation op, CRcDriver *drv, CResource *rc, CRcValueState *vs) {
///     switch (op) {
///       case rcdOpInit:
///         ...
///         break;
///       case rcdOpStop:
///         ...
///         break;
///       case rcdOpDriveValue:
///         ...
///         break;
///     }
///   }
/// @endcode
#define HOME2L_DRIVER(NAME) \
  extern "C" { void Home2lRcDriverFunc_##NAME (ERcDriverOperation, CRcDriver *, CResource *, CRcValueState *); } \
  void Home2lRcDriverFunc_##NAME


/** Driver for local resources.
 *
 * All 'CRcDriver' objects must be allocated dynamically and registered before the Resources library is started (see RcStart() ).
 * On registration, the library becomes owner of the object.
 *
 * Registering an object of this class without a driver function ('NULL') results in a signal driver.
 *
 * @nosubgrouping
 */
class CRcDriver {
  public:
    CRcDriver (const char *_lid, FRcDriverFunc *_func = NULL) { lid.Set (_lid); func = _func; }
    virtual ~CRcDriver () { ClearResources (); }

    /// @name Life cycle ...
    /// @{
    void Register ();
      ///< Register and enable the driver. This must be done between the calls to RcInit() and RcStart(), i.e. the
      /// initialization phase. The class takes over the ownership of 'this', which must be a dynamically allocated
      /// object, which is deleted during RcDone().
    static CRcDriver *RegisterAndInit (const char *_lid, FRcDriverFunc *_func);
      ///< Create, register and initialize a function-based driver; Returns a reference to the @ref CRcDriver object.
    /// @}

    /// @name Properties ...
    /// @{
    const char *Lid () { return lid.Get (); }
    const char *ToStr () { return lid.Get (); }
    /// @}

    /// @name Resource management ...
    /// @{
    CResource *RegisterResource (const char *rcLid, ERcType _type, bool _writable = true) { return CResource::Register (this, rcLid, _type, _writable); }
    CResource *RegisterResource (const char *rcLid, const char *rcTypeDef) { return CResource::Register (this, rcLid, rcTypeDef); }
      ///< see also: CResource::Register()
    /// @}

    /// @name For directory services ...
    /// @{
    void PrintInfo (FILE *f = stdout);

    CResource *GetResource (const char *rcLid) { return resourceMap.Get (rcLid); }

    int LockResources () { Lock (); return resourceMap.Entries (); }
    CResource *GetResource (int n) { return resourceMap.Get (n); }
    void UnlockResources () { Unlock (); }
    /// @}

  protected:
    friend class CResource;
    friend void RcDriversStop ();

    /// @name Interface methods ...
    /// @{
    virtual void Stop ();
      ///< Stop all (own) threads and close all I/Os properly; Will be called from RcDone().
      /// Resources are still registered and will be unregistered afterwards.
    virtual void DriveValue (CResource *rc, CRcValueState *vs);
      ///< Drive a new value; Will be called from CResource::DriveValue().
      ///
      /// Propagate a new requested value to the device (e.g. to an actor). Afterwards, the new value and state pointed at
      /// by 'vs' will be reported automatically by the caller.
      ///
      /// It is legal to modify '*vs' to reflect a new state or even a value different from the driven one.
      /// The state of '*vs' is preset with 'rcsValid'. As required, the method shall change it to an appropriate
      /// state ('rcsUnknown' or 'rcsBusy'). If the new value is not known yet, 'rcsBusy' should be set now and
      /// a 'CRcResource::Report...()' method be called later with a valid value. If for some reason, nothing
      /// should be reported now at all, 'val->type' can be set to 'rctNone' by calling 'rc->Clear()'.
      ///
      /// It is not allowed to call any method of the 'rc' object (e.g. 'Report' methods) from here, since the
      /// resource may be locked.
      ///
      /// In most cases, just writing to the device is sufficient.
    /// @}

    /// @name Helper methods ...
    /// @{
    void Lock () { mutex.Lock (); }
    void Unlock () { mutex.Unlock (); }

    void ClearResources ();     ///< Unregister all resources.
    /// @}

    // Static data...
    CString lid;
    FRcDriverFunc *func;

    // Dynamic data (protected by the mutex)...
    CMutex mutex;
    CDictRef<CResource> resourceMap;   // object does not own the resources; when deleted here, they must be unregistered manually.
};


/** Local driver using the event processor mechanism for the 'DriveValue()' functionality.
 * The class is intended mainly for the Python API to allow writing simple applications (e.g. 'rules' scripts)
 * without parallel threads and an event loop design.
 */
class CRcEventDriver: public CRcDriver, public CRcEventProcessor {
  public:
    CRcEventDriver (const char *_lid, ERcState _successState = rcsValid): CRcDriver (_lid) { successState = _successState; }
      ///< Initialize the driver (does not register).
      /// '_successState' is the value returned by 'DriveValue'; Typical values are:
      /// - 'rcsValid': everything ok (no further action by the driver necessary)
      /// - 'rcsBusy': potentially not ok; Driver must report a valid value later.

    virtual const char *TypeId () { return "D"; }     // hint for Python/SWIG API
    virtual const char *InstId () { return Lid (); }  // hint for Python/SWIG API

  protected:
    virtual void Stop () {}   ///< The caller/owner must do this before 'RcDone()'. (The event processor may not work anymore by the time this method is called.)
    virtual void DriveValue (CResource *rc, CRcValueState *vs);

    ERcState successState;
};


#endif // #ifndef SWIG


/// @}  // resources_drivers
#ifdef SWIG
%pythoncode %{
## @}
%}
#endif // SWIG





// ************ Directory operations and host/driver/resource lookup ***********


/** @addtogroup resources_directory
 *
 * Directory operations and host/driver/resource lookup.
 *
 * @{
 */
#ifdef SWIG
%pythoncode %{
## @addtogroup resources_directory
## @{
%}
#endif // SWIG



// ***** Hosts *****


/// @name Hosts ...
/// @{

int RcGetHosts ();            ///< Get number of known hosts.
CRcHost *RcGetHost (int n);   ///< Get reference to host *n*, *0 <= n < number of hosts*.

CRcHost *RcGetHost (const char *id);      ///< Lookup host by its ID.
const char *RcGetHostId (CRcHost *host);  ///< Get textual ID of 'host'.


#ifndef SWIG
extern CString localHostId;
#endif
static inline const char *GetLocalHostId () { return localHostId.Get (); }  ///< Get the ID of the local instance.
#ifdef SWIG
%pythoncode %{
  pass

  ## Return a list of all known hosts (including our own one as #0).
  def RcHosts ():
    """Return a list of all known hosts (including our own one as #0)."""
    num = RcGetHosts ()
    ret = [ GetLocalHostId () ]
    for i in range (num):
      ret += [ RcGetHostId (RcGetHost (i)) ]
    return ret
%}
#endif

/// @}



// ***** Drivers (local) *****


/// @name Drivers (local) ...
/// @{

int RcGetDrivers ();
CRcDriver *RcGetDriver (int n);

CRcDriver *RcGetDriver (const char *lid);
const char *RcGetDriverId (CRcDriver *cur);

#ifdef SWIG
%pythoncode %{
  pass

  ## Return a list of all locally defined drivers.
  def RcDrivers ():
    """Return a list of all locally defined drivers."""
    num = RcGetDrivers ()
    ret = []
    for i in range (num):
      ret += [ RcGetDriverId (RcGetDriver (i)) ]
    return ret
%}
#endif

/// @}



// ***** Resources *****


/// @name Resources ...
/// @{

int RcLockHostResources (CRcHost *host);
  ///< Query number of resources for a *remote* host.
  /// The set is locked internally, meaning that no registration of new resources is possible
  /// until 'RcUnlockHostResources' is called!
  /// Only presently known resources are returned, no network queries are performed.
CResource *RcGetHostResource (CRcHost *host, int n);
void RcUnlockHostResources (CRcHost *host);

int RcLockDriverResources (CRcDriver *driver);
  ///< Query the number of resources for a *local* driver.
  /// The set is locked internally, meaning that no registration of new resources is possible
  /// until 'RcUnlockDriverResources' is called!
CResource *RcGetDriverResource (CRcDriver *driver, int n);
void RcUnlockDriverResources (CRcDriver *driver);

#ifdef SWIG
%pythoncode %{
  pass

  ## Return all known resources of a remote host.
  def RcHostResources (hostName):
    """Return all known resources of a remote host."""
    h = RcGetHost (hostName)
    num = RcLockHostResources (h)
    ret = []
    for i in range (num):
      ret += [ RcGetHostResource (h, i).Uri () ]
    RcUnlockHostResources (h)
    return ret

  ## Return all local resources managed by a driver.
  def RcDriverResources (driverName):
    """Return all local resources managed by a driver."""
    d = RcGetDriver (driverName)
    num = RcLockDriverResources (d)
    ret = []
    for i in range (num):
      ret += [ RcGetDriverResource (d, i).Uri () ]
    RcUnlockDriverResources (d)
    return ret

  ## Return a complete list of all known resources.
  def RcResources ():
    """Return a complete list of all known resources."""
    ret = []
    for d in RcDrivers ():
      ret += RcDriverResources (d)
    for h in RcHosts ():
      ret += RcHostResources (h)
    return ret

%}
#endif // SWIG

/// @}



// ***** Subscribers *****


/// @name Subscribers ...
/// @{

#ifndef SWIG
int RcLockSubscribers ();
  ///< Return the number of existing subscribers and locks the subscriber database.
  /// During the locked state, other operations are also locked out, so the caller should
  /// use this carefully and unlock as soon as possible.
CRcSubscriber *RcGetSubscriber (int n);
void RcUnlockSubscribers ();
#endif

/// @}


// Requests and per-resource subscribers: see 'class CResource'


/// @}  // resources_directory
#ifdef SWIG
%pythoncode %{
## @}
%}
#endif // SWIG






// *************************** High-level API **********************************


// ***** General functions *****


/// @addtogroup resources_general
/// @{
#ifdef SWIG
%pythoncode %{
## @addtogroup resources_general
## @{
%}
#endif // SWIG


#ifndef SWIG


/// @name Init/Done ...
/// @{
void RcInit (bool enableServer = false, bool inBackground = false);
  ///< Initialize the Resources library (elaboration phase).
  /// This may be followed by driver declarations, and after that RcStart() must be called.
  /// @param enableServer enables the server mode:
  ///   - false => server is off, independent on the configuration environment.
  ///   - true  => enable server if and as set in the configuration environment.
  /// @param inBackground switches on the timer thread using 'TimerStart' and friends. If disabled, the caller is
  ///    responsible for iterating the timer. This can be the case if the caller itself has a an own background timer
  ///    thread (e.g. 'home2l-wallclock') or if RcRun() / RcStop() is to be used for the main program.
void RcStart ();
  ///< Complete the elaboration phase and start active operation.
  /// No more drivers and resources may be declared after this.
void RcDone ();
  ///< Shutdown the Resources library
/// @}


/// @name Running the library tasks in foreground ...
/// Functions to run the background work of the Resources library in a user-supplied thread.
/// The following functions are only necessary and allowed if 'inBackground = false' was passed to RcInit() and the
/// main program does not maintain the timers otherwise (see section @ref base_timers in file @ref common/base.H).
/// In other words: These functions may be used if there is no main event loop yet.
/// @{
void RcIterate ();  ///< Perform pending actions and return.
void RcRun ();      ///< Run 'RcStart', then RcIterate() in a main event loop until RcStop() is called.
void RcStop ();     ///< Stop a loop started by RcRun().
/// @}


#endif



// ***** Looking up resources *****


/// @name Accessing resources ...
/// @{
#ifdef SWIG
%pythoncode %{
## @name Accessing resources ...
## @{
%}
#endif // SWIG


#ifdef SWIG
%feature("docstring") RcGetResource "Lookup a resource by its URI and return a reference to it."
%feature("docstring") RcGet "Lookup a resource by its URI and return a reference to it."
#endif

static inline CResource *RcGetResource (const char *uri, bool allowWait = false) { return CResource::Get (uri, allowWait); }
static inline CResource *RcGet (const char *uri, bool allowWait = false) { return CResource::Get (uri, allowWait); }
  ///< Lookup a resource by its URI and return a reference to it.
  /// @param uri is the full URI of the resource.
  /// @param allowWait allows to wait for network operations if the resource is not yet known.
  /// This is useful for the *Home2L Shell* to display the properties (type, state, value) right
  /// afterwards. In most other applications, the default value of 'false' is more appropriate.
  ///
  /// The returned pointer is always valid (!= NULL) unless the URI is syntactically incorrect and will neither now
  /// nor in the future refer to a valid resource. Hence, the pointer can be used for identity comparisons.
  /// It will be uniquely associated with the URI until the program terminates or RcGarbageCollection() is called.


static inline void RcGarbageCollection () { CResource::GarbageCollection (); }
  ///< (For experts only) Remove all presently unregistered resources from memory.
  /// This may free some memory, but at the same time invalidates any existing reference to @ref CResource objects.
  /// May only be called if no 'CResource *' reference is presently used!


/// @}
#ifdef SWIG
%pythoncode %{
## @}
%}
#endif // SWIG



// ***** Subscriptions *****


/// @name Subscriptions...
/// @{
#ifdef SWIG
%pythoncode %{
## @name Subscriptions (from C/C++ API) ...
## Low-level operations to directly create and use @ref CRcSubscriber objects.
## Most probably, you will not need these functions.
## The preferred way in Python is to use the more compfortable decorators and
## operations in section "Subscriptions, events and timers (Python API)" below.
## @{
%}
#endif // SWIG


#ifdef SWIG
%feature("docstring") RcSubscribe "Create and register a new subscriber object."
%feature("docstring") RcSubscribeToResource "Subscribe to a resource. Either create a new subscriber object or add it to an existing one."
%feature("docstring") RcUnsubscribeFromAll "Unsubscribe to all resources managed by the subscriber."
%newobject RcSubscribe (const char *);
%newobject RcSubscribeToResource (const char *, CResource *);
%newobject RcSubscribeToResource (const char *, const char *);
#endif

CRcSubscriber *RcSubscribe (const char *subscrLid);
  ///< Create and register a new subscriber object.
  /// @param subscrLid is the local ID of the subscriber.
  /// The object is owned by the caller and must be deleted later.
CRcSubscriber *RcSubscribeToResource (const char *subscrLid, CResource *rc);
  ///< Create and register a new subscriber object and subscribe to the given resource.
  /// @param subscrLid is the local ID of the subscriber.
  /// @param rc is a reference to the resource to subscribe to.
CRcSubscriber *RcSubscribeToResource (const char *subscrLid, const char *pattern);
  ///< Create and register a new subscriber and subscribe to a set of resources.
  /// @param subscrLid is the local ID of the subscriber.
  /// @param pattern may be the URI of a resource, a pattern with allowed wildcards, or a comma-separated list of URIs/patterns.

static inline CRcSubscriber *RcSubscribeToResource (CRcSubscriber *subscr, CResource *rc) { subscr->AddResource (rc); return subscr; }
  ///< Subscribe to an additional resource(s).
static inline CRcSubscriber *RcSubscribeToResource (CRcSubscriber *subscr, const char *pattern) { subscr->AddResources (pattern); return subscr; }
  ///< Subscribe to an additional set of resources.

static inline void RcUnsubscribeFromAll (CRcSubscriber *subscr) { subscr->Clear (); }
  ///< Unsubscribe to all resources presently subscribed by 'subscr'.
  /// The caller remains owner of 'subscr', which does not need to reside in the heap.
#ifndef SWIG
static inline void RcUnsubscribeFromAll (CRcSubscriber **pSubscr) { if (*pSubscr) { delete *pSubscr; *pSubscr = NULL; } }
  ///< Unsubscribe to all resources defined by 'subscr' and remove the subscriber object from heap.
#endif


/// @}
#ifdef SWIG
%pythoncode %{
## @}
%}
#endif // SWIG



// ***** Placing requests *****


/// @name Placing requests ...
/// @{
#ifdef SWIG
%pythoncode %{
## @name Placing requests ...
## Details on the request arguments can be found in the
## [*C/C++ API documentation*](../home2l-api_c/classCRcRequest.html)
## and the [*Home2L Book*](../home2l-book.pdf).
## @{
%}
#endif // SWIG


#ifdef SWIG
%feature("docstring") RcSetRequest "Add or change a request to a resource."
%feature("docstring") RcDelRequest "Delete a request from a resource."
%feature("docstring") RcSetTrigger "Add a trigger request."
#endif

// Normal requests...

#ifndef SWIG
static inline void RcSetRequest (CResource *rc, CRcRequest *req) { rc->SetRequest (req); }
void RcSetRequest (const char *rcUri, CRcRequest *req);
  ///< Add or replace a request (generic version).
  /// @param rcUri identifies the resource.
  /// @param req must by a dynamically allocated object, which is handed over and must no longer be used by the caller afterwards.
static inline void RcSetRequest (CResource *rc, CRcValueState *value, const char *reqGid, int priority = rcPrioNormal, TTicks t0 = 0, TTicks t1 = 0, TTicks hysteresis = 0) { rc->SetRequest (value, reqGid, priority, t0, t1, hysteresis); }
void RcSetRequest (const char *rcUri, CRcValueState *value, const char *reqGid, int priority = rcPrioNormal, TTicks t0 = 0, TTicks t1 = 0, TTicks hysteresis = 0);
  ///< Add or replace a request (generic value, direct parameters).
  /// @param reqGid identifies the request.
  /// @param rcUri identifies the resource.
  /// @param value specifies the requested value (only type and value are used; the state is ignored).
  /// @param priority is the request priority attribute.
  /// @param t0 is the activation (start) time attribute (optional).
  /// @param t1 is the expiration (stop) time attribute (optional).
  /// @param hysteresis is the hysteresis attribute (optional).
  ///
  /// The request attributes are explaind in the [*Home2L Book*](../home2l-book.pdf).
static inline void RcSetRequest (CResource *rc, bool valBool, const char *reqGid, int priority = rcPrioNormal, TTicks t0 = 0, TTicks t1 = 0, TTicks hysteresis = 0) { rc->SetRequest (valBool, reqGid, priority, t0, t1, hysteresis); }
void RcSetRequest (const char *rcUri, bool valBool, const char *reqGid, int priority = rcPrioNormal, TTicks t0 = 0, TTicks t1 = 0, TTicks hysteresis = 0);
  ///< Add or replace a request (direct value and parameters).
static inline void RcSetRequest (CResource *rc, int valInt, const char *reqGid, int priority = rcPrioNormal, TTicks t0 = 0, TTicks t1 = 0, TTicks hysteresis = 0) { rc->SetRequest (valInt, reqGid, priority, t0, t1, hysteresis); }
void RcSetRequest (const char *rcUri, int valInt, const char *reqGid, int priority = rcPrioNormal, TTicks t0 = 0, TTicks t1 = 0, TTicks hysteresis = 0);
  ///< Add or replace a request (direct value and parameters).
static inline void RcSetRequest (CResource *rc, float valFloat, const char *reqGid, int priority = rcPrioNormal, TTicks t0 = 0, TTicks t1 = 0, TTicks hysteresis = 0) { rc->SetRequest (valFloat, reqGid, priority, t0, t1, hysteresis); }
void RcSetRequest (const char *rcUri, float valFloat, const char *reqGid, int priority = rcPrioNormal, TTicks t0 = 0, TTicks t1 = 0, TTicks hysteresis = 0);
  ///< Add or replace a request (direct value and parameters).
static inline void RcSetRequest (CResource *rc, const char *valString, const char *reqGid, int priority = rcPrioNormal, TTicks t0 = 0, TTicks t1 = 0, TTicks hysteresis = 0) { rc->SetRequest (valString, reqGid, priority, t0, t1, hysteresis); }
void RcSetRequest (const char *rcUri, const char *valString, const char *reqGid, int priority = rcPrioNormal, TTicks t0 = 0, TTicks t1 = 0, TTicks hysteresis = 0);
  ///< Add or replace a request (direct value and parameters).
#else
%pythoncode %{
  def RcSetRequest (rc, value, id = None, priority = None, t0 = None, t1 = None, hysteresis = None):
    """Set a request to a resource with selected attributes."""
    if isinstance (rc, str): rc = RcGetResource (rc)
    rc.SetRequest (value, id, priority, t0, t1, hysteresis)
%} // %pythoncode
#endif // SWIG
void RcSetRequestFromStr (const char *rcUri, const char *reqDef);
  ///< Set request by textual definition - see CRcRequest::SetFromStr() and the
  /// [*Home2L Book*](../home2l-book.pdf) for more information on the syntax of request definitions.
static inline void RcSetRequestFromStr (CResource *rc, const char *reqDef) { rc->SetRequestFromStr (reqDef); }


// Triggers...
#ifndef SWIG
static inline void RcSetTrigger (CResource *rc, const char *reqGid, int priority = rcPrioNormal, TTicks t0 = 0, TTicks t1 = 0, TTicks hysteresis = 0) { rc->SetTrigger (reqGid, priority, t0, t1, hysteresis); }
void RcSetTrigger (const char *rcUri, const char *reqGid, int priority = rcPrioNormal, TTicks t0 = 0, TTicks t1 = 0, TTicks hysteresis = 0);
  ///< Set a trigger request (no value needs to be given).
#else
%pythoncode %{
  def RcSetTrigger (rc, id = None, priority = None, t0 = None, t1 = None, hysteresis = None):
    """Set a request to a trigger resource with selected attributes."""
    if isinstance (rc, str): rc = RcGetResource (rc)
    rc.SetRequest (1, id, priority, t0, t1, hysteresis)
%} // %pythoncode
#endif // SWIG
static inline void RcSetTriggerFromStr (CResource *rc, const char *reqDef) { rc->SetTriggerFromStr (reqDef); }
void RcSetTriggerFromStr (const char *rcUri, const char *reqDef);
  ///< Set trigger request by textual definition.


// Delete requests ...
static inline void RcDelRequest (CResource *rc, const char *reqGid = NULL) { rc->DelRequest (reqGid); }
void RcDelRequest (const char *rcUri, const char *reqGid = NULL);
  ///< Delete a request.
  /// @param rcUri identifies the resource.
  /// @param reqGid is the global request identifierr (GID) and identifies the request.

// Mutex operations (TBD) ...
//~ static inline CRcSubscriber *RcMutexRequestLock (const char *rcUri, const char *regGid, CRcSubscriber *subscr == NULL) { RcSetRequest (rcUri, reqGid, reqGid); return subscr; }
//~ void RcMutexWaitLock (const char *rcUri, const char *regGid, CRcSubscriber *subscr);
//~ bool RcMutexPollLock (const char *rcUri, const char *regGid, CRcSubscriber *subscr);
//~ void RcMutexUnlock (const char *rcUri, const char *regGid, CRcSubscriber *subscr);


/// @}  // @name (requests)
#ifdef SWIG
%pythoncode %{
## @}
%}
#endif // SWIG



// ***** Drivers *****


/// @name Drivers ...
/// These functions must be called between RcInit() and RcStart().
/// To lookup and return a driver object (usually not needed), use RcGetDriver ()
/// @{
#ifdef SWIG
%pythoncode %{
## @name Drivers (from C/C++ API) ...
## Low-level operations to directly create and use 'CRcDriver' objects.
## Most probably, you will not need these functions.
## The preferred way in Python is to use the more compfortable decorators and
## operations in module "Home2L / Drivers".
## @{
%}
#endif // SWIG



#ifdef SWIG
%feature("docstring") RcRegisterDriver "Register a new driver."
%feature("docstring") RcRegisterResource "Register a new resource to a driver."
%feature("docstring") RcRegisterSignal "Register a new signal for the built-in 'signal' driver."
#endif

/// @name Drivers: Registering a new driver ...
/// @{
#ifndef SWIG
static inline void RcRegisterDriver (CRcDriver *drv) { drv->Register (); }
  ///< Register a pre-initialized driver object (typically an object of a derived class with overloaded methods).
static inline void RcRegisterDriver (const char *drvLid, FRcDriverFunc *func) { CRcDriver::RegisterAndInit (drvLid, func); }
  ///< Register and initialize a driver by its function.
  /// The driver object is created internally, and 'func' is called with the 'rcdOpInit' operation.
#endif
CRcEventProcessor *RcRegisterDriver (const char *drvLid, ERcState _successState = rcsValid);
  ///< Register an event-based driver and return a reference to the event processor.
  /// Driven values are collected by the returned event processor, and the caller must process the events accordingly.
  /// Reporting of values can be done via the 'Report...()' methods of the respective resources.
  /// The @ref CRcEventProcessor object remains owned by the library.
  /// @param drvLid is the ID of the new driver.
  /// @param _successState' is the value returned by DriveValue(); senseful values are:
  ///   - 'rcsValid': everything ok (no further action by the driver necessary)
  ///   - 'rcsBusy': potentially not ok; Driver must report a valid value later.
/// @}

/// @name Drivers: Registering a new resource to a driver ...
/// See also: CResource::Register() .
/// @{
#ifndef SWIG
static inline CResource *RcRegisterResource (CRcDriver *drv, const char *rcLid, ERcType _type, bool _writable) { return CResource::Register (drv, rcLid, _type, _writable); }
static inline CResource *RcRegisterResource (CRcDriver *drv, const char *rcLid, const char *rcTypeDef) { return CResource::Register (drv, rcLid, rcTypeDef); }
#endif
CResource *RcRegisterResource (const char *drvLid, const char *rcLid, ERcType _type, bool _writable = true);
CResource *RcRegisterResource (const char *drvLid, const char *rcLid, const char *rcTypeDef);

CResource *RcRegisterSignal (const char *name, ERcType type);
  ///< Register a new resource for the built-in 'signal' driver.
CResource *RcRegisterSignal (const char *name, CRcValueState *vs);
  ///< Register a new resource for the built-in 'signal' driver and optionally set a default request.
  ///  The type is taken from 'vs', and if 'vs' is valid, a default request is set.
/// @}


/// @}  // drivers
#ifdef SWIG
%pythoncode %{
## @}
%}
#endif // SWIG




// ***** Executing shell commands *****

// TBD




/// @}  // resources_general
#ifdef SWIG
%pythoncode %{
## @}
%}
#endif // SWIG






// ***** Doxygen Footer *****

/// @}  // resources (Main)
#ifdef SWIG
%pythoncode %{
## @}
%}
#endif // SWIG


/// @}  // @file
#ifdef SWIG
%pythoncode %{
## @}   # @file
%}
#endif // SWIG



#endif // _RC_LIB_
