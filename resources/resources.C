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
#include "rc_drivers.H"

#include <fnmatch.h>




// ********************* Environment Settings **********************************


ENV_PARA_STRING ("rc.persistent", envRcPersistent, NULL);
  /* Resources to be made persistent
   *
   * This is an alternative way to make a set of resources persistent.
   * It can be comma- or whitespace-separated list of resource URIs or patterns.
   * Wildcards are allowed. By default, only those resources specified in
   * \reftool{resources.conf} are persistent.
   *
   * With persistent resources, all pending requests are stored in a file and
   * retrieved again on the next startup. Only requests are stored, no values.
   * On read-only resources, this setting has no effect.
   *
   * Persistent requests are stored as persistent environment variables, at the
   * file is flushed (but without FS sync) before the request is actually applied
   * and reported back.
   */

ENV_PARA_STRING ("rc.userReqId", envRcUserReqId, "user");
  /* Request ID for user interactions, e.g. with the WallClock floorplan or with physical gadgets
   */

ENV_PARA_STRING("rc.userReqAttrs", envRcUserReqAttrs, "-31:00");
  /* Request attributes for for user interactions
   *
   * This parameter defines the attributes of requests generated on user
   * interactions, e.g. with the WallClock floorplan or with physical gadgets.
   *
   * The probably most useful attribute is the off-time. For example, if the
   * attribute string is "-31:00" and a user pushes a button to close the shades,
   * this overrides automatic rules until 7 a.m. on the next morning. Afterwards,
   * automatic rules may open them again.
   *
   * The request ID must be defined by setting \refenv{rc.userReqId} .
   * Adding an ID field to the attributes here has no effect.
   */


const char *const rcDefaultRequestId = "default";

const char *RcGetUserRequestId () { return envRcUserReqId; }
const char *RcGetUserRequestAttrs () { return envRcUserReqAttrs; }





// *************************** Helpers *****************************************


static bool rcInitCompleted = false;
  // Flag to indicate whether the library has completed initialization and is now running.
  // If set, no new drivers are allowed to be registered.


static bool IsValidIdentifier (const char *id, bool allowSlash) {
  const char *p;
  char c;

  // Check if 's' is a valid identifier for an instance, driver, resource or request...
  if (!id) return false;
  if (!*id) return false;
  for (p = id; *p; p++) {
    c = *p;
    if ((c < 'a' || c > 'z')  &&
        (c < 'A' || c > 'Z')  &&
        (c < '0' || c > '9' || p == id)  &&   // digits are not allowed as the first character
        (c != '-' || p == id) &&              // dash not allowed as the first character
        c != '_' && c != '.' &&
        (c != '/' || !allowSlash)) return false;
  }
  return true;
}





// *************************** Types *******************************************


// ***** Base types and declarations *****


static const char *rcTypeNames [] = {   // names of base and special types
  //~ "none", "bool", "int", "float", "string", "blob", "time", "color", "trigger", "mutex"
  "none", "bool", "int", "float", "string", "time", "trigger", "mutex"
};


static ERcType rcBaseTypeList [] = {    // base type for base and special types
  //~ rctNone, rctBool, rctInt, rctFloat, rctString, rctBlob, rctTime, rctColor, rctInt, rctString
  rctNone, rctBool, rctInt, rctFloat, rctString, rctTime, rctInt, rctString
};


typedef struct {
  const char *id;
  ERcType base;   // 'rctInt' or 'rctFloat'
  const char *unit;
} TRcUnitType;


typedef struct {
  const char *id;
  int values;
  const char **valueList;
} TRcEnumType;





// ***** Built-in unit types *****


static const TRcUnitType rcUnitTypeList[rctUnitTypesEND - rctUnitTypesBase] = {
  { "percent", rctFloat, "%" },
  { "temp", rctFloat, "°C" }
  // --> New unit types may be added here. <--
};





// ***** Built-in enum types *****


#define ENUM_VALUES(T) sizeof (T##Values) / sizeof (const char *), T##Values

static const char *rctUseStateValues [] = { "day", "night", "away", "vacation" };
static const char *rctWindowStateValues [] = { "closed", "tilted", "open", "openOrTilted" };
static const char *rctPhoneStateValues [] = { "idle", "ringing", "call" };
static const char *rctPlayerStateValues [] = { "stopped", "paused", "playing" };

static const TRcEnumType rcEnumTypeList[rctEnumTypesEND - rctEnumTypesBase] = {
  { "use",    ENUM_VALUES(rctUseState) },
  { "window", ENUM_VALUES(rctWindowState) },
  { "phone",  ENUM_VALUES(rctPhoneState) },
  { "player",  ENUM_VALUES(rctPlayerState) },
  // --> New enum types may be added here. <--
  //
  // Values may only contain letters, digits and underscores:
  //    `<enum> ::= [_a-zA-Z][_a-zA-Z0-9]+`
  //
  // A value starting with '?' represents an invalid value.
};





// ***** Functions *****


#define rcBaseTypes ((int) (sizeof (rcTypeNames) / sizeof (rcTypeNames[0])))
#define rcUnitTypes ((int) (sizeof (rcUnitTypeList) / sizeof (TRcUnitType)))
#define rcEnumTypes ((int) (sizeof (rcEnumTypeList) / sizeof (TRcEnumType)))


static inline void URcValueClear (URcValue *val) { val->vAny = 0; }


const char *RcTypeGetName (ERcType t) {
  if (t < rctUnitTypesBase) {         // Base type...
    return rcTypeNames[(int) t];
  }
  else if (t < rctEnumTypesBase) {    // Unit type...
    return rcUnitTypeList[((int) t - rctUnitTypesBase)].id;
  }
  else {                              // Enum type...
    return rcEnumTypeList[((int) t - rctEnumTypesBase)].id;
  }
}


ERcType RcTypeGetFromName (const char *name) {
  int n;

  for (n = 1; n < rcBaseTypes; n++)
    if (strcasecmp (name, rcTypeNames[n]) == 0) return (ERcType) n;
  for (n = 0; n < rcUnitTypes; n++)
    if (strcasecmp (name, rcUnitTypeList[n].id) == 0) return (ERcType) (n + rctUnitTypesBase);
  for (n = 0; n < rcEnumTypes; n++)
    if (strcasecmp (name, rcEnumTypeList[n].id) == 0) return (ERcType) (n + rctEnumTypesBase);

  // Failure...
  return rctNone;
}


// Get base type ...
ERcType RcTypeGetBaseType (ERcType t) {
  if (t < rctUnitTypesBase) {         // Base type...
    return rcBaseTypeList[(int) t];
  }
  else if (t < rctEnumTypesBase) {    // Unit type...
    return rcUnitTypeList[((int) t - rctUnitTypesBase)].base;
  }
  else {                              // Enum type...
    return rctInt;
  }
}


bool RcTypeIsStringBased (ERcType t) {
  if (t >= rctUnitTypesBase) return false;    // unit or enum types are never string-based.
  return rcBaseTypeList[t] == rctString;
}


// Unit types ...
const char *RcTypeGetUnit (ERcType t) {
  if (!RcTypeIsUnitType (t)) return CString::emptyStr;
  return rcUnitTypeList[t - rctUnitTypesBase].unit;
}


// Enumeration types ...
int RcTypeGetEnumValues (ERcType t) {
  if (!RcTypeIsEnumType (t)) return 0;
  return rcEnumTypeList[t- rctEnumTypesBase].values;
}


const char *RcTypeGetEnumValue (ERcType t, int idx, bool warn) {

  // Sanity...
  if (!RcTypeIsEnumType (t)) {
    if (warn) WARNINGF (("'RcTypeGetEnumValue()' called for a non-enum type %02x", (int) t));
    return "?";
  }
  if (idx < 0 || idx >= rcEnumTypeList[t - rctEnumTypesBase].values) {
    if (warn) WARNINGF (("'RcTypeGetEnumValue()' called for a type '%s' with invalid index %i", RcTypeGetName (t), idx));
    return "?";
  }

  // Return...
  return rcEnumTypeList[t- rctEnumTypesBase].valueList[idx];
}


int RcTypeGetEnumIdx (ERcType t, const char *value, bool warn) {
  const TRcEnumType *enumType;
  int n;

  // Weak sanity...
  if (!RcTypeIsEnumType (t)) {
    if (warn) WARNINGF (("'RcTypeGetEnumValue()' called for a non-enum type %02x", (int) t));
    return -1;
  }

  // Search for the value...
  enumType = &rcEnumTypeList[t- rctEnumTypesBase];
  for (n = 0; n < enumType->values; n++)
    if (strcmp (value, enumType->valueList[n]) == 0) return n;  // success

  // Not found ...
  if (warn) WARNINGF (("'RcTypeGetEnumIdx()' called for a type '%s' with invalid value '%s'", RcTypeGetName (t), value));
  return -1;
}


// Writing to string...
static void AppendValue (CString *ret, URcValue val, ERcType type, bool precise, int stringChars) {
  CString s;
  char *p, *q, buf[64];
  ERcType baseType = RcTypeGetBaseType (type);

  if (precise && baseType == rctFloat) {
    ret->AppendF ("$%08x", (uint32_t) val.vInt);
  }
  else switch (baseType) {
    case rctBool:
      ret->Append (val.vBool ? '1' : '0');
      break;
    case rctInt:
      if (RcTypeIsEnumType (type))
        ret->Append (RcTypeGetEnumValue (type, val.vInt));
      else {
        ret->AppendF ("%i", val.vInt);
        if (RcTypeIsUnitType (type)) ret->Append (RcTypeGetUnit (type));
      }
      break;
    case rctFloat:
      snprintf (buf, sizeof (buf), "%f", val.vFloat);
      q = strrchr (buf, '.');
      if (q) {
        // Remove trailing '0's...
        p = q + strlen (q);
        while (p > q + 2 && p[-1] == '0') p--;
        *p = '\0';
      }
      ret->Append (buf);
      if (RcTypeIsUnitType (type)) ret->Append (RcTypeGetUnit (type));
      break;
    case rctString:
      ret->AppendEscaped (val.vString, stringChars);
      break;
    case rctTime:
      ret->Append (TicksAbsToString (&s, val.vTime, INT_MAX, precise));
      break;
    case rctNone:
    default:
      ret->Append ('?');
      break;
  }
}


// Parsing ...
static bool ParseValue (const char *p, ERcType type, URcValue *retVal) {
  // Parse string 'p' for a value of type 'type'. The type must be given (!= 'rctNone').
  // On success, the value is returned via 'retVal'. On failure, 'false' is returned
  // and '*retVal' remains unchanged.
  // This function expects the value and nothing more as an input 'p'. In particular,
  // white spaces (leading, trailing) are not tolerated!
  URcValue val;
  ERcType baseType;
  char *q;
  int idx;
  bool ok = true;

  if (!p) return false;     // sanity

  URcValueClear (&val);
  baseType = RcTypeGetBaseType (type);
  if (p[0] == '$' && baseType == rctFloat) {    // precise value?
    val.vInt = (uint32_t) strtoll (p + 1, &q, 16);         // parse as hexadecimal
    //~ INFOF (("### ParseValue: '%s' -> %08x -> %f", p, val.vInt, val.vFloat));
    if (q[0] != '\0') ok = false;
  }
  else switch (baseType) {
    case rctBool:
      ok = BoolFromString (p, &val.vBool);
      //~ INFOF (("### ParseValue ('%s') = %i, ok = %i", p, (int) val.vBool, (int) ok));
      break;
    case rctInt:
      val.vInt = (int) strtol (p, &q, 0);    // '0': accept any base
      if (q == p) ok = false;   // parsing failed
      else {
        if (q[0] != '\0') {
          // Something is behind the last valid digit: This must be the unit, the correct one!
          if (!RcTypeIsUnitType (type)) ok = false;   // not a unit type
          else if (strcmp (q, RcTypeGetUnit (type)) != 0) ok = false;   // wrong unit
        }
      }
      if (!ok) {
        if (type == rctTrigger) {
          // In the context of a 'Report...' action or request, the value of the passed
          // 'RcValueState' object is irrelevant. For this reason, we tolerate eventual
          // syntax errors for trigger values.
          val.vInt = 0;
          ok = true;  // everything is ok;
        }
        else if (RcTypeIsEnumType (type)) {
          // We may have an enum type...
          idx = RcTypeGetEnumIdx (type, p);
          if (idx >= 0) {
            val.vInt = idx;
            ok = true;
          }
        }
      }
      break;
    case rctFloat:
      val.vFloat = strtof (p, &q);
      if (q == p) ok = false;   // parsing failed
      else {
        if (q[0] != '\0') {
          // Something is behind the last valid digit: This must be the unit, the correct one!
          if (!RcTypeIsUnitType (type)) ok = false;   // not a unit type
          else if (strcmp (q, RcTypeGetUnit (type)) != 0) ok = false;   // wrong unit
        }
      }
      break;
    case rctString:
      {
        CString str;
        ok = str.SetUnescaped (p);
        //~ INFOF (("### ParseValue (rctString, '%s') -> OK=%i, '%s'", p, (int) ok, str.Get ()));
        if (ok) val.vString = str.Disown ();
      }
      break;
    case rctTime:
      ok = TicksAbsFromString (p, &val.vTime);
      break;
    default:
      ok = false;
  }
  if (ok) *retVal = val;
  return ok;
}





// *************************** CRcValueState ***********************************


// ***** General setters *****


void CRcValueState::Clear (ERcType _type, ERcState _state) {
  if (RcTypeIsStringBased (type)) free ((void *) val.vString);
  URcValueClear (&val);
  type = _type;
  state = _state;
  timeStamp = 0;
}


void CRcValueState::Set (const CRcValueState *vs2) {
  //~ CString s1, s2;
  //~ INFOF (("### CRcValueState::Set (): %s <- %s", ToStr (&s1, true), vs2->ToStr (&s2, true)));

  // Sanity ...
  if (!vs2) {
    Clear ();
    return;
  }

  // Free string on old value ...
  if (RcTypeIsStringBased (type)) {
    if (val.vString) {
      free ((void *) val.vString);
      val.vString = NULL;
    }
  }

  // Copy value ...
  if (RcTypeIsStringBased (vs2->type)) {
    val.vString = NULL;
    if (vs2->val.vString) if (vs2->val.vString[0])
      val.vString = strdup (vs2->val.vString);    // non-empty string => create own copy
    // empty string are implicitly normalize to 'NULL'
  }
  else val = vs2->val;    // no string => just copy

  // Copy attributes ...
  type = vs2->type;
  state = vs2->state;
  timeStamp = 0;
}



// ***** Multi-type setters *****


void CRcValueState::SetGenericInt (int _val, ERcType _type, ERcState _state) {
  Clear (_type, _state);
  switch (RcTypeGetBaseType (_type)) {
    case rctBool:
    case rctInt:
      val.vInt = _val;
      break;
    case rctFloat:
      //~ INFOF(("### int -> float"));
      val.vFloat = _val;
      break;
    default:
      WARNINGF (("CRcValueState::SetInt () called for an incompatible type '%s'" , RcTypeGetName (_type)));
  }
}


void CRcValueState::SetGenericFloat (float _val, ERcType _type, ERcState _state) {
  Clear (_type, _state);
  switch (RcTypeGetBaseType (_type)) {
    case rctBool:
    case rctInt:
      //~ INFOF(("### float -> int"));
      val.vInt = (_val + 0.5);
      break;
    case rctFloat:
      val.vFloat = _val;
      break;
    default:
      WARNINGF (("CRcValueState::SetFloat () called for an incompatible type '%s'" , RcTypeGetName (_type)));
  }
}


bool CRcValueState::SetGenericString (const char *_val, ERcType _type, ERcState _state) {

  // Init ...
  Clear (_type, _state);

  // Handle target type string ...
  if (RcTypeIsStringBased (_type)) {
    val.vString = _val ? strdup (_val) : NULL;
    return true;
  }

  // Target type is no string: Try to convert ...
  if (ParseValue (_val, _type, &val)) return true;
  else {
    state = rcsUnknown;
    return false;
  }
}


void CRcValueState::SetTime (TTicks _val, ERcState _state) {
  Clear (rctTime, _state);
  val.vTime = _val;
}





// ***** Getting values *****


ERcState CRcValueState::GetValue (bool *retBool) {
  if (state == rcsUnknown) return rcsUnknown;
  if (RcTypeGetBaseType (type) != rctBool) if (!Convert (rctBool)) return rcsUnknown;
  *retBool = val.vBool;
  return state;
}


ERcState CRcValueState::GetValue (int *retInt) {
  if (state == rcsUnknown) return rcsUnknown;
  if (RcTypeGetBaseType (type) != rctInt) if (!Convert (rctInt)) return rcsUnknown;
  *retInt = val.vInt;
  return state;
}


ERcState CRcValueState::GetValue (float *retFloat) {
  if (state == rcsUnknown) return rcsUnknown;
  if (RcTypeGetBaseType (type) != rctFloat) if (!Convert (rctFloat)) return rcsUnknown;
  *retFloat = val.vFloat;
  return state;
}


ERcState CRcValueState::GetValue (CString *retString) {
  if (state == rcsUnknown) return rcsUnknown;
  if (RcTypeGetBaseType (type) != rctString) if (!Convert (rctString)) return rcsUnknown;
  retString->Set (val.vString);
  return state;
}


ERcState CRcValueState::GetValue (TTicks *retTime) {
  if (state == rcsUnknown) return rcsUnknown;
  if (RcTypeGetBaseType (type) != rctTime) if (!Convert (rctTime)) return rcsUnknown;
  *retTime = val.vTime;
  return state;
}


bool CRcValueState::ValidBool (bool defaultVal) {
  if (state == rcsUnknown) return defaultVal;
  if (RcTypeGetBaseType (type) != rctBool) if (!Convert (rctBool)) return defaultVal;
  return val.vBool;
}


int CRcValueState::ValidInt (int defaultVal) {
  if (state == rcsUnknown) return defaultVal;
  if (RcTypeGetBaseType (type) != rctInt) if (!Convert (rctInt)) return defaultVal;
  return val.vInt;
}


float CRcValueState::ValidFloat (float defaultVal) {
  if (state == rcsUnknown) return defaultVal;
  if (RcTypeGetBaseType (type) != rctFloat) if (!Convert (rctFloat)) return defaultVal;
  return val.vFloat;
}


const char *CRcValueState::ValidString (const char *defaultVal) {
  if (state == rcsUnknown) return defaultVal;
  if (RcTypeGetBaseType (type) != rctString) if (!Convert (rctString)) return defaultVal;
  return val.vString ? val.vString : CString::emptyStr;
}


TTicks CRcValueState::ValidTime (TTicks defaultVal) {
  if (state == rcsUnknown) return defaultVal;
  if (RcTypeGetBaseType (type) != rctTime) if (!Convert (rctTime)) return defaultVal;
  return val.vTime;
}


int CRcValueState::ValidUnitInt (ERcType _type, int defaultVal) const {
  if (state == rcsUnknown || type != _type) return defaultVal;
  if (RcTypeGetBaseType (_type) == rctFloat) return (int) val.vFloat;
  return val.vInt;
}


float CRcValueState::ValidUnitFloat (ERcType _type, float defaultVal) const {
  if (state == rcsUnknown || type != _type) return defaultVal;
  if (RcTypeGetBaseType (_type) == rctInt) return (float) val.vInt;
  return val.vFloat;
}


int CRcValueState::ValidEnumIdx (ERcType _type, int defaultVal) const {
  if (state == rcsUnknown || type != _type) return defaultVal;
  return val.vInt;
}



// ***** Attributes *****


bool CRcValueState::Equals (const CRcValueState *vs2) const {
  if (!vs2) return state == rcsUnknown;     // tolerate 'vs2 == NULL' (for Python API), consider nothing to be equal to "unknown"
  if (state != vs2->state) return false;
  if (state == rcsUnknown) return true;     // "unknown" is always equal to "unknown"
  return ValueEquals (vs2);
}


bool CRcValueState::ValueEquals (const CRcValueState *vs2) const {
  if (type != vs2->type) return false;
  if (RcTypeIsStringBased (type)) {
    if (!val.vString && !vs2->val.vString) return true;    // both strings are empty
    if (!val.vString || !vs2->val.vString) return false;   // one string is empty, the other is not
    // now no string is empty...
    return strcmp (val.vString, vs2->val.vString) == 0;
  }
  else
    return val.vAny == vs2->val.vAny;
}



// ***** Conversion *****


bool CRcValueState::Convert (ERcType _type) {
  ERcType baseType, _baseType;
  bool valBool;

  // Sanity / No-Op...
  if (_type == type || state == rcsUnknown) return true;   // nothing to do
  baseType = RcTypeGetBaseType (type);
  _baseType = RcTypeGetBaseType (_type);
  if (_baseType == baseType) {      // types are compatible?
    type = _type;
    return true;
  }

  // Anything from string...
  if (baseType == rctString) {
    CString s;
    CRcValueState vs;

    vs.Clear (_type, state);
    if (!vs.SetFromStr (val.vString)) return false;
    if (vs.type != _type) return false;   // The string may have contained type information incompatible with what we want.
    Set (&vs);
    return true;
  }

  // Anything to string...
  if (_baseType == rctString) {
    CString s;

    return SetGenericString (ToStr (&s), _type, state);
  }

  // All other possible cases...
  switch (baseType) {
    case rctBool:
    case rctInt:
      switch (_baseType) {
        case rctNone:
          return false;
        case rctBool:
          valBool = (val.vInt == 0 ? false : true);
          Clear (_type, state);
          val.vBool = valBool;
          break;
        default:
          SetGenericInt (val.vInt, _type, state);
      }
      break;
    case rctFloat:
      switch (_baseType) {
        case rctNone:
          return false;
        case rctBool:
          valBool = (val.vFloat == 0.0 ? false : true);
          Clear (_type, state);
          val.vBool = valBool;
          break;
        default:
          SetGenericFloat (val.vFloat, _type, state);
      }
      break;
    case rctTime:
      // Times cannot be converted to anything else.
      //return false;
    default:
      return false;
  }

  // Success...
  return true;
}



// ***** Stringification *****


const char *CRcValueState::ToStr (CString *ret, bool withType, bool withTimeStamp, bool precise, int stringChars) const {
  CString s;

  if (precise) stringChars = INT_MAX;

  // Type indicator...
  if (withType) ret->SetF ("(%s) ", RcTypeGetName (type));
  else ret->Clear ();

  // State indicator...
  switch (state) {
    case rcsBusy:
      ret->Append ('!');
      // fall-through
    case rcsValid:
      AppendValue (ret, val, type, precise, stringChars);
      break;

    default:  // probably 'rcsUnknown'
      ret->Append ('?');
  };

  // Timestamp...
  if (withTimeStamp && timeStamp > 0) {
    ret->Append (" @");
    ret->Append (TicksAbsToString (timeStamp, 3));
  }
  return ret->Get ();
}


const char *CRcValueState::ToStr (bool withType, bool withTimeStamp, bool precise, int stringChars) const {
  return ToStr (GetTTS (), withType, withTimeStamp, precise, stringChars);
}


bool CRcValueState::SetFromStr (const char *str) {
  CSplitString args;
  TTicks _timeStamp;
  const char *_valStr;
  int n;
  const char *p;
  char *q;
  bool ok;

  // Clear the value...
  Clear ();
  if (!str) return false;   // sanity
  ok = true;
  _valStr = NULL;

  // Split, strip and analyse the input, and set the type if given ...
  args.Set (str);
  for (n = 0; n < args.Entries () && ok; n++) {
    p = args[0];
    switch (p[0]) {
      case '(':
        // Read type information ...
        q = (char *) p + 1;
        while (*q && *q != ')') q++;
        if (*q != ')') ok = false;
        else {
          *q = '\0';   // Strings in 'args' are local copies, we are allowed to modify them.
          Clear (RcTypeGetFromName (p + 1));
        }
        break;
      case '@':
        // Read time stamp...
        ok = TicksAbsFromString (p + 1, &_timeStamp);
        break;
      default:
        // This must be the value (+ state)...
        if (_valStr) ok = false;
        else _valStr = p;
    }
  }

  // Read state & value ...
  if (ok) ok = SetFromStrFast (_valStr, false);

  // Set time stamp...
  if (ok) timeStamp = _timeStamp;

  // Warn & finish...
  if (!ok) {
    WARNINGF(("Invalid string '%s' passed to 'CRcValueState::SetFromStr' (type '%s')", str, RcTypeGetName (type)));
    Clear ();
  }
  return ok;
}


bool CRcValueState::SetFromStrFast (const char *str, bool warn) {
  bool ok;

  // Read state information...
  switch (str[0]) {
    case '?':
      state = rcsUnknown;
      str++;
      break;
    case '!':
      state = rcsBusy;
      str++;
      break;
    default:
      state = rcsValid;
  }

  // Read value...
  ok = true;
  if (state != rcsUnknown) {
    if (type != rctNone) ok = ParseValue (str, type, &val);
    else {
      Clear (rctString, state);
      ok = ParseValue (str, rctString, &val);
      if (!ok) Clear (rctNone);
    }
  }

  // Warn & finish...
  if (!ok) {
    if (warn) WARNINGF(("Invalid string '%s' passed to 'CRcValueState::SetFromStrFast' (type '%s')", str, RcTypeGetName (type)));
    Clear ();
  }
  return ok;
}





// *************************** CResource ***************************************


ENV_PARA_INT ("rc.maxOrphaned", envMaxOrphanedResources, 1024);
  /* Maximum number of allowed unregistered resources
   *
   * Resource objects (\texttt{class CResource}) are allocated on demand and
   * are usually never removed from memory, so that pointers to them can be used
   * as unique IDs during the lifetime of a programm. Unregistered resources
   * are those that presently cannot be linked to real local or remote resource.
   * They occur naturally, for example, if the network connection to a remote
   * host is not yet available. However, if the number of unregistered resources
   * exceeds a certain high number, there is probably a bug in the application
   * which may as a negative side-effect cause high CPU and network loads.
   *
   * This setting limits the number of unregistered resources. If the number is
   * exceeded, the application is terminated.
   */


// Initialization information for local resources derived from the configuration file ...
//   Keys are the respective URIs without the leading "/host/<hostId>/".
//   These dictionaries are cleared/invalid after drivers have been started ('RcStart ()').
CKeySet rcConfPersistence;                  // local resources configured persistent in 'resources.conf'
CDictFast<CString> rcConfDefaultRequests;   // default requests (as strings) configured in 'resources.conf'



// ***** Initialization and life cycle management *****


CResource::CResource () {
  regSeq = 0;

  rcHost = NULL;
  rcDriver = NULL;
  rcUserData = NULL;
  writable = true;     // 'true' to avoid warnings on requests to unregistered resources

  requestList = NULL;
  subscrList = NULL;
}


CResource::~CResource () {
  ASSERT (IsRegistered () == false);   // destruction may only happen after unregistration
  ClearRequestsAL ();
}


CResource *CResource::GetUnregistered (const char *uri) {
  // Get an unregistered resource for upcoming registration or use.
  // Never accessed resources are created on demand. In any case, the returned resource is contained in
  // 'unregisteredResourceMap' afterwards.
  CResource *rc;

  unregisteredResourceMapMutex.Lock ();
  rc = unregisteredResourceMap.Get (uri);
  unregisteredResourceMapMutex.Unlock ();
  if (!rc) {
    // Resource never queried => Create new object...
    //~ INFOF (("###   resource '%s' is NEW.", uri));
    rc = new CResource ();
    rc->gid.Set (uri);
    rc->lid = rc->gid.Get ();
    rc->PutUnregistered ();
  }
  return rc;
}


void CResource::PutUnregistered () {
  unregisteredResourceMapMutex.Lock ();
  ASSERT (unregisteredResourceMap.Get (Uri ()) == NULL);   // TBD: This is expensive => remove?
  if (unregisteredResourceMap.Entries () >= envMaxOrphanedResources)
    ERROR ("Maximum number of orphaned/invalid resources exceeded");
  unregisteredResourceMap.Set (Uri (), this);
  unregisteredResourceMapMutex.Unlock ();
}


CResource *CResource::Get (const char *uri, bool allowWait) {
  CString realUri;
  CResource *rc;
  ERcPathDomain domain;

  if (!uri) return NULL;
  RcGetRealPath (&realUri, uri, "/alias");
  domain = RcAnalysePath (realUri.Get (), NULL, NULL, NULL, &rc, allowWait);
  if (!rc && (domain == rcpResource || domain == rcpDriver || domain == rcpAlias)) {
    // Only domains of type 'rcpResource' get the chances to become registered some time later.
    // However, we must accept anything here (unless syntactically incorrect) so that
    // the caller does not receive NULLs for non-existing aliases or drivers.
    rc = GetUnregistered (realUri.Get ());
  }
  if (!rc) WARNINGF (("Invalid URI '%s'", uri));
  return rc;
}


void CResource::GarbageCollection () {
  int n;

  unregisteredResourceMapMutex.Lock ();
  for (n = 0; n < unregisteredResourceMap.Entries (); n++)
    delete unregisteredResourceMap.Get (n);
  unregisteredResourceMap.Clear ();
  unregisteredResourceMapMutex.Unlock ();
}


CResource *CResource::Register (CRcHost *_rcHost, CRcDriver *_rcDriver, const char *_lid,
                                ERcType _type, bool _writable, void *_data) {
  CResource *rc;
  CRcRequest *reqSaved, *reqNext, *reqDefault;
  CString uri, *reqStr;
  const char *key;
  int n;

  // Sanity...
  ASSERT (_rcHost || _rcDriver);
  if (!IsValidIdentifier (_lid, true)) ERRORF(("CResource::Register (): Invalid resource ID '%s'", _lid));
  reqDefault = NULL;

  // Get or create object and make sure it is unregistered...
  //   For efficiency reasons, we do not use 'Get ()' here.
  rc = NULL;
  if (_rcHost)                rc = _rcHost->GetResource (_lid);
  else /* if (_rcDriver) */   rc = _rcDriver->GetResource (_lid);
  if (rc) rc->Unregister ();  // presently registered
  else {                      // new or presently unregistered...
    //~ INFOF(("### Preparing unregistered LID '%s'", _lid));

    // Determine URI...
    if (_rcHost)              uri.SetF ("/host/%s/%s", RcGetHostId (_rcHost), _lid);
    else /* if (_rcDriver) */ uri.SetF ("/host/%s/%s/%s", localHostId.Get (), _rcDriver->Lid (), _lid);
    // Get unregistered...
    rc = GetUnregistered (uri.Get ());
  }

  //~ INFOF(("### Registering '%s'...", uri.Get ()));

  // Remove from 'unregisteredResourceMap'...
  unregisteredResourceMapMutex.Lock ();
  unregisteredResourceMap.Del (rc->Uri ());
  unregisteredResourceMapMutex.Unlock ();

  // Lock...
  //~ INFOF (("### ...registering: subscrList = %08x", rc->subscrList));
  rc->Lock ();

  // Setup resource (semi-static data)...
  ATOMIC_WRITE (rc->rcHost, _rcHost);
  ATOMIC_WRITE (rc->rcDriver, _rcDriver);
  rc->writable = _writable;
  ATOMIC_WRITE (rc->rcUserData, _data);
  if (_writable && _rcDriver) {

    // Set attributes (persistence, default request) ...
    key = uri.Get () + 7 + localHostId.Len ();    // 6 = strlen ("/host") + 2 * strlen ("/")

    // Persistence ...
    rc->persistent = (rcConfPersistence.Find (key) >= 0);           // persistent by resource configuration?
    //~ INFOF (("### Persistent ('%s') = %i", key, (int) rc->persistent));
    //~ rcConfPersistence.Dump ();

    if (!rc->persistent)
      rc->persistent = RcUriMatches (rc->Uri (), envRcPersistent);  // persistent by environment?
    if (rc->persistent) EnvEnablePersistence ();

    // Default request ...
    reqStr = rcConfDefaultRequests.Get (key);
    if (reqStr) {
      reqDefault = new CRcRequest ((CRcValueState *) NULL, rcDefaultRequestId, rcPrioDefault);
      if (!reqDefault->SetFromStr (reqStr->Get ())) FREEO(reqDefault);
    }
  }
  else rc->persistent = false;

  ATOMIC_WRITE (rc->lid, rc->gid.Get () + strlen (rc->gid.Get ()) - strlen (_lid));
  ASSERT (strcmp (rc->lid, _lid) == 0);
  //~ INFOF ((" ###   CResource::Register: rc = %08x, drv = '%s'/%08x, gid = '%s'/%08x, lid = '%s'/%08x",
          //~ rc, rc->rcDriver ? rc->rcDriver->Lid () : "(none)", rc->rcDriver, rc->gid.Get (), rc->gid.Get (), rc->lid, rc->lid));

  if (_type == rctTrigger) rc->valueState.SetTrigger ();
  else rc->valueState.Clear (_type);

  // Increment 'regSeq' to mark as registered ...
  ATOMIC_INC (rc->regSeq, 1);

  // Move all requests to a temporary reversed list; 'reqSaved' is the "first" pointer of that list...
  reqSaved = NULL;
  while (rc->requestList) {
    reqNext = reqSaved;
    reqSaved = rc->requestList;
    rc->requestList = reqSaved->next;
    reqSaved->next = reqNext;
  }

  // Unlock 'this'...
  //   All changes to 'rc' requiring locking have been completed by now.
  rc->Unlock ();

  // Add 'this' to the owner's resource map...
  //   This must happen after 'rc' is unlocked to avoid deadlocks
  if (_rcHost) {
    _rcHost->Lock ();
    _rcHost->resourceMap.Set (_lid, rc);
    _rcHost->Unlock ();
  }
  if (_rcDriver) {
    //~ ASSERT (!rcInitCompleted);
    if (rcInitCompleted)
      ERRORF (("Registration attempt for a local resource '%s/%s' after the initialization phase.", _rcDriver->Lid (), _lid));
    _rcDriver->Lock ();
    //~ INFOF (("### Adding resource '%s' to resource map of driver '%s'", rc->Uri (), _rcDriver->Lid ()));
    _rcDriver->resourceMap.Set (_lid, rc);
    _rcDriver->Unlock ();
  }

  // Check if some subscriber is interested in this resource...
  SubscriberMapLock ();
  for (n = 0; n < subscriberMap.Entries (); n++)
    subscriberMap.Get (n)->CheckNewResource (rc);
  SubscriberMapUnlock ();

  // Set back all requests (in correct order) to send remote requests to their hosts ...
  // For local resources, the evaluation is done later after the elaboration phase.
  // Requests set later in the following sequence may override earlier ones.

  // ... 1. Set default request if given in configuration ...
  if (reqDefault) rc->SetRequestFromObjNoEvaluate (reqDefault);

  // ... 2. For persistent resources: Set all stored requests (may override a configured default requests) ...
  if (rc->persistent) {
    CString prefix;
    CRcRequest *req;
    const char *reqId;
    int idx0, idx1, prefixLen, i;

    prefix.SetF ("var.rc.(%s).", rc->Gid ());
    prefixLen = prefix.Len ();
    EnvGetPrefixInterval (prefix.Get (), &idx0, &idx1);
    for (i = idx0; i < idx1; i++) {
      reqId = EnvGetKey (i) + prefixLen;
      req = new CRcRequest ();
      req->SetGid (reqId);
      if (req->SetFromStr (EnvGetVal (i))) rc->SetRequestFromObjNoEvaluate (req);
    }
  }

  // ... 3. Set all collected requests that have been set before registration ...
  while (reqSaved) {
    reqNext = reqSaved->next;
    rc->SetRequestFromObjNoEvaluate (reqSaved);
    reqSaved = reqNext;
  }

  // Done ...
  //~ INFOF (("### ...registered: subscrList = %08x", rc->subscrList));
  return rc;
}


CResource *CResource::Register (CRcHost *_rcHost, CRcDriver *_rcDriver, const char *_lid, const char *rcTypeDef, void *_data) {
  CSplitString arg;
  ERcType _type = rctNone;
  bool _writable = false;
  const char *p;
  bool ok;

  arg.Set (rcTypeDef);
  ok = (arg.Entries () == 2);
  if (ok) {

    // Type...
    _type = RcTypeGetFromName (arg.Get (0));
    if (_type == rctNone) ok = false;

    // Writable flag ...
    _writable = false;
    for (p = arg.Get (1); *p; p++) if (*p == 'w' || *p == 'W') _writable = true;
  }
  if (ok)
    return Register (_rcHost, _rcDriver, _lid, _type, _writable, _data);
  else
    WARNINGF(("Invalid resource type definition '%s' for resource '%s'", rcTypeDef, _lid));
  return NULL;
}


CResource *CResource::Register (const char *rcDef, void *_data) {
  CSplitString arg;
  CRcHost *_rcHost;
  CRcDriver *_rcDriver;
  const char *_lid;
  bool ok;

  arg.Set (rcDef, 2);
  ok = (arg.Entries () == 2);
  if (ok) {
    // URI...
    RcAnalysePath (arg.Get (0), &_lid, &_rcHost, &_rcDriver, NULL);
    ok = (_lid && (_rcHost || _rcDriver));
  }
  if (ok)
    return Register (_rcHost, _rcDriver, _lid, arg.Get (1), _data);
  else
    WARNINGF(("Invalid resource definition string '%s'", rcDef));
  return NULL;
}


void CResource::Unregister () {

  // Lock and return if already unregistered...
  Lock ();
  if (!IsRegistered ()) {
    Unlock ();
    return;
  }

  // Invalidate value...
  ReportUnknownAL ();

  // Increment 'regSeq' to mark as unregistered...
  ATOMIC_INC (regSeq, 1);

  // Unlock...
  //   All changes to 'this' have completed by here; The following code
  //   needs an unlocked 'this' to keep the correct deadlock-safe ordering.
  Unlock ();

  // Remove from host's or driver's map...
  if (rcHost) {
    rcHost->Lock ();
    rcHost->resourceMap.Del (lid);
    rcHost->Unlock ();
    rcHost = NULL;
  }
  if (rcDriver) {
    rcDriver->Lock ();
    rcDriver->resourceMap.Del (lid);
    rcDriver->Unlock ();
    rcDriver = NULL;
  }

  // Add to waiting room...
  PutUnregistered ();
}


void CResource::WaitForRegistration () {
  TTicksMonotonic timeLeft;

  if (!IsRegistered ()) {   // fast pre-check
    timeLeft = RcNetTimeout ();
    while (!IsRegistered () && timeLeft > 0) {
      Sleep (timeLeft > 64 ? 64 : timeLeft);
      timeLeft -= 64;
    }
  }
}



// ***** Identification *****


bool CResource::Is (const char *uri) {
  CString realUri;
  const char *p, *q;

  p = gid.Get ();
  q = RcGetRealPath (&realUri, uri);
  //~ INFOF(("# '%s' == '%s'?", p, q));
  while (true) {
    if (*p != *q) return false;
    if (!*p) return true;
    p++;
    q++;
  }
}


bool CResource::IsLike (const char *pattern) {
  return RcUriMatches (gid.Get (), pattern);
}


const char *CResource::ToStr (CString *ret, bool pathLocal) {
  Lock ();
  ret->SetF ("%s %s %s%s", pathLocal ? Lid () : Uri (), RcTypeGetName (Type ()), writable ? "wr" : "ro", persistent ? ",p" : CString::emptyStr);
  Unlock ();
  return ret->Get ();
}


const char *CResource::ToStr (bool pathLocal) {
  return ToStr (GetTTS (), pathLocal);
}


// ***** Reading values *****


void CResource::SubscribePAL (CRcSubscriber *subscr, bool resLocked, bool subLocked) {
  CRcSubscriberLink *sl;
  CResourceLink *rl;
  CRcEvent ev;

  //~ INFOF ((" ###  CResource::SubscribePAL: gid = %08x, lid = %08x", gid.Get (), lid));
  //~ INFOF(("### CResource::SubscribePAL (subscr = '%s'/%08x, rc = '%s')", subscr->Lid (), subscr, Lid ()));

  // (Re-)lock resource and subscription in a deadlock-safe way...
  if (!resLocked && subLocked) {
    subscr->Unlock ();
    Lock ();
    subscr->Lock ();
  }
  else {
    if (!resLocked) Lock ();
    if (!subLocked) subscr->Lock ();
  }

  // Check if resource and subscription are already linked...
  for (sl = subscrList; sl; sl = sl->next) if (sl->subscr == subscr) break;
  if (!sl) {

    // Check if the subscriber has been registered...
    ASSERTM (subscr->Gid () [0] != '\0', "Unable to subscribe with unregistered subscriber");

    // No duplicate: create the link...
    sl = new CRcSubscriberLink (subscr, subscrList);
    ATOMIC_WRITE (subscrList, sl);
    rl = new CResourceLink (this, subscr->resourceList);
    subscr->resourceList = rl;

    // Send subscription to remote host ...
    if (rcHost) rcHost->RemoteSubscribe (subscr, this);

    // Submit current value as an event...
    ev.Set (rceValueStateChanged, this, &valueState);
    subscr->NotifyAL (&ev);

    // For a local resource commit that we are connected...
    if (rcDriver) {
      ev.Set (rceConnected, this, &valueState);
      subscr->NotifyAL (&ev);
      sl->isConnected = true;
    }
  }

  // Unlock resource and subscription ...
  if (!subLocked) subscr->Unlock ();   // restore lock
  if (!resLocked) Unlock ();
}


void CResource::UnsubscribePAL (CRcSubscriber *subscr, bool resLocked, bool subLocked) {
  CRcSubscriberLink **sl, *vicSl;
  CResourceLink **rl, *vicRl;

  //~ INFOF(("### CResource::UnsubscribePAL (subscr = '%s'/%08x, rc = '%s')", subscr->Lid (), subscr, Lid ()));

  // Lock resource and subscription in a deadlock-safe way...
  if (!resLocked && subLocked) {
    subscr->Unlock ();
    Lock ();
    subscr->Lock ();
  }
  else {
    if (!resLocked) Lock ();
    if (!subLocked) subscr->Lock ();
  }

  // Remove subscription from link list...
  for (sl = &subscrList; *sl; sl = &((*sl)->next))
    if ((*sl)->subscr == subscr) {
      vicSl = *sl;
      ATOMIC_WRITE (*sl, vicSl->next);
      delete vicSl;
      break;
    }

  // Remove resource from link list...
  for (rl = &subscr->resourceList; *rl; rl = &((*rl)->next))
    if ((*rl)->resource == this) {
      vicRl = *rl;
      *rl = vicRl->next;
      delete vicRl;
      break;
    }

  // Post unsubscription to remote host ...
  if (rcHost) rcHost->RemoteUnsubscribe (subscr, this);

  // Unlock resource and subscription...
  if (!resLocked) Unlock ();
  if (!subLocked) subscr->Unlock ();
}


void CResource::GetValueState (CRcValueState *retValueState) {
  Lock ();
  *retValueState = valueState;
  Unlock ();
}


ERcState CResource::GetValue (bool *retBool, TTicks *retTimeStamp) {
  Lock ();
  ERcState ret = valueState.GetValue (retBool);
  if (retTimeStamp) *retTimeStamp = valueState.timeStamp;
  Unlock ();
  return ret;
}


ERcState CResource::GetValue (int *retInt, TTicks *retTimeStamp) {
  Lock ();
  ERcState ret = valueState.GetValue (retInt);
  if (retTimeStamp) *retTimeStamp = valueState.timeStamp;
  Unlock ();
  return ret;
}


ERcState CResource::GetValue (float *retFloat, TTicks *retTimeStamp) {
  Lock ();
  ERcState ret = valueState.GetValue (retFloat);
  if (retTimeStamp) *retTimeStamp = valueState.timeStamp;
  Unlock ();
  return ret;
}


ERcState CResource::GetValue (CString *retString, TTicks *retTimeStamp) {
  Lock ();
  ERcState ret = valueState.GetValue (retString);
  if (retTimeStamp) *retTimeStamp = valueState.timeStamp;
  Unlock ();
  return ret;
}


bool CResource::ValidBool (bool defaultVal, TTicks *retTimeStamp) {
  Lock ();
  bool ret = valueState.ValidBool (defaultVal);
  if (retTimeStamp) *retTimeStamp = valueState.timeStamp;
  Unlock ();
  return ret;
}


int CResource::ValidInt (int defaultVal, TTicks *retTimeStamp) {
  Lock ();
  int ret = valueState.ValidInt (defaultVal);
  if (retTimeStamp) *retTimeStamp = valueState.timeStamp;
  Unlock ();
  return ret;
}


float CResource::ValidFloat (float defaultVal, TTicks *retTimeStamp) {
  Lock ();
  float ret = valueState.ValidFloat (defaultVal);
  if (retTimeStamp) *retTimeStamp = valueState.timeStamp;
  Unlock ();
  return ret;
}


const char *CResource::ValidString (CString *ret, const char *defaultVal, TTicks *retTimeStamp) {
  Lock ();
  ret->Set (valueState.ValidString (defaultVal));
  if (retTimeStamp) *retTimeStamp = valueState.timeStamp;
  Unlock ();
  return ret->Get ();
}


const char *CResource::ValidString (const char *defaultVal, TTicks *retTimeStamp) {
  return ValidString (GetTTS (), defaultVal, retTimeStamp);
}


TTicks CResource::ValidTime (TTicks defaultVal, TTicks *retTimeStamp) {
  Lock ();
  TTicks ret = valueState.ValidTime (defaultVal);
  if (retTimeStamp) *retTimeStamp = valueState.timeStamp;
  Unlock ();
  return ret;
}


int CResource::ValidUnitInt (ERcType _type, int defaultVal, TTicks *retTimeStamp) {
  Lock ();
  int ret = valueState.ValidUnitInt (_type, defaultVal);
  if (retTimeStamp) *retTimeStamp = valueState.timeStamp;
  Unlock ();
  return ret;
}


float CResource::ValidUnitFloat (ERcType _type, float defaultVal, TTicks *retTimeStamp) {
  Lock ();
  float ret = valueState.ValidUnitFloat (_type, defaultVal);
  if (retTimeStamp) *retTimeStamp = valueState.timeStamp;
  Unlock ();
  return ret;
}


int CResource::ValidEnumIdx (ERcType _type, int defaultVal, TTicks *retTimeStamp) {
  Lock ();
  int ret = valueState.ValidEnumIdx (_type, defaultVal);
  if (retTimeStamp) *retTimeStamp = valueState.timeStamp;
  Unlock ();
  return ret;
}


void CResource::ReadValueState (CRcValueState *retValueState, int maxTime) {
  CRcSubscriber *subscr;
  CRcEvent ev;

  // Invalidate return value...
  retValueState->Clear ();

  // Subscribe and wait for up-to-date value...
  subscr = new CRcSubscriber ();
  subscr->Register ("rclib");
  SubscribePAL (subscr);
  while (maxTime > 0) {
    if (subscr->WaitEvent (&ev, &maxTime))
      if (ev.Type () == rceValueStateChanged) {
        *retValueState = *ev.ValueState ();
        break;
      }
  }
  subscr->Unregister ();
}



// ***** Requesting values *****


void CResource::UpdatePersistentRequestAL (const char *reqId, CRcRequest *req) {
  CString key, reqDef;

  key.SetF ("var.rc.(%s).%s", Gid (), reqId);
  if (req)  EnvPut (key.Get (), req->ToStr (&reqDef, true, false, 0, "i#"));
  else      EnvDel (key.Get ());
  EnvFlush ();
}


void CResource::ClearRequestsAL () {
  CRcRequest *req;

  while (requestList) {
    req = requestList;
    ATOMIC_WRITE (requestList, req->next);
    delete req;
  }
}


bool CResource::DoDelRequestAL (CRcRequest **pList, const char *reqGid, TTicks t1, bool updatePersistence) {
  CRcRequest *oldReq, **pp;

  for (pp = pList; *pp; pp = &((*pp)->next))
    if (strcmp ((*pp)->Gid (), reqGid) == 0) {
      oldReq = *pp;
      if (!t1) {
        ATOMIC_WRITE (*pp, oldReq->next);
        delete oldReq;
        if (updatePersistence) UpdatePersistentRequestAL (reqGid, NULL);
      }
      else {
        oldReq->t1 = t1;
        if (updatePersistence) UpdatePersistentRequestAL (reqGid, oldReq);
      }
      return true;    // there can be only one request with that ID
    }
  return false;
}


bool CResource::DelRequestNoEvaluate (const char *reqGid, TTicks t1) {
  CRcRequest *req;
  bool reEvaluate, isRegistered;

  // Sanity...
  if (!reqGid) reqGid = EnvInstanceName ();
  if (reqGid[0] == '#') reqGid++;
  if (t1 == RCREQ_NONE) t1 = 0;

  // Remove the request from list...
  Lock ();
  reEvaluate = DoDelRequestAL (&requestList, reqGid, t1, persistent);
  isRegistered = IsRegistered ();
  if (!isRegistered) {
    // Add a dummy request with an "invalid" value as a marker, so that later
    // with the registration a "delete request" message will be sent to a remote
    // host.
    req = new CRcRequest ();
    req->SetGid (reqGid);
    req->SetTimeOff (t1);
    req->next = requestList;
    requestList = req;
  }
  Unlock ();

  // Re-evaluate 'this' or send to remote host ...
  if (isRegistered) {
    if (!rcHost) {
      if (reEvaluate) return true;
    }
    else rcHost->RemoteDelRequest (this, reqGid, t1);
  }
  return false;
}


void CResource::SetRequestFromObjNoEvaluate (CRcRequest *_request) {
  //~ INFOF (("### Set request '%s' on '%s'...", _request->ToStr (), Uri ()));

  // Sanity...
  if (!IsValidIdentifier (_request->Gid (), true)) {
    WARNINGF (("Ignoring request to '%s' with invalid ID: '%s'", Uri (),  _request->ToStr ()));
    delete _request;
    return;
  }
  if (_request->Repeat () && !_request->TimeOn ()) {
    WARNINGF (("Ignoring repeat attribute for request without an on-time to '%s': '%s'", Uri (),  _request->ToStr ()));
    _request->repeat = 0;
  }

  // Handle delete case ...
  if (!_request->Value ()->IsKnown ()) {
    DelRequestNoEvaluate (_request->Gid (), _request->TimeOff ());
    delete _request;
    return;
  }

  // Lock...
  Lock ();

  // Check and convert the request (but only if the resource is already registered) ...
  if (Type () != rctNone) {
    if (!writable) {
      WARNINGF (("Request '%s' to write-protected resource '%s' will have no effect", _request->ToStr (), Uri ()));
    }
    else
      _request->Convert (this);
  }

  // Add the request - locally or to remote host - and unlock...
  if (rcHost) {
    Unlock ();
    rcHost->RemoteSetRequest (this, _request);
  }
  else {
    if (persistent) UpdatePersistentRequestAL (_request->Gid (), _request);
    DoDelRequestAL (&requestList, _request->Gid (), 0, false);   // avoid duplicates: remove the old request, if it exists
    _request->next = requestList;
    ATOMIC_WRITE (requestList, _request);
    Unlock ();
  }
}


void CResource::SetRequestFromObj (CRcRequest *_request) {
  SetRequestFromObjNoEvaluate (_request);
  if (rcDriver) EvaluateRequests ();
}


void CResource::SetRequest (CRcValueState *value, const char *reqGid, int priority, TTicks t0, TTicks t1, TTicks repeat, TTicks hysteresis) {
  SetRequestFromObj (new CRcRequest (value, reqGid, priority, t0, t1, repeat, hysteresis));
}


void CResource::SetRequest (bool valBool, const char *reqGid, int priority, TTicks t0, TTicks t1, TTicks repeat, TTicks hysteresis) {
  SetRequestFromObj (new CRcRequest (valBool, reqGid, priority, t0, t1, repeat, hysteresis));
}


void CResource::SetRequest (int valInt, const char *reqGid, int priority, TTicks t0, TTicks t1, TTicks repeat, TTicks hysteresis) {
  SetRequestFromObj (new CRcRequest (valInt, reqGid, priority, t0, t1, repeat, hysteresis));
}


void CResource::SetRequest (float valFloat, const char *reqGid, int priority, TTicks t0, TTicks t1, TTicks repeat, TTicks hysteresis) {
  SetRequestFromObj (new CRcRequest (valFloat, reqGid, priority, t0, t1, repeat, hysteresis));
}


void CResource::SetRequest (const char *valString, const char *reqGid, int priority, TTicks t0, TTicks t1, TTicks repeat, TTicks hysteresis) {
  SetRequestFromObj (new CRcRequest (valString, reqGid, priority, t0, t1, repeat, hysteresis));
}


void CResource::SetRequest (TTicks valTime, const char *reqGid, int priority, TTicks t0, TTicks t1, TTicks repeat, TTicks hysteresis) {
  SetRequestFromObj (new CRcRequest (valTime, reqGid, priority, t0, t1, repeat, hysteresis));
}


void CResource::SetRequestFromStr (const char *reqDef) {
  CRcRequest *req = new CRcRequest ();
  req->SetFromStr (reqDef);
  SetRequestFromObj (req);
}


void CResource::SetTrigger (const char *reqGid, int priority, TTicks t0, TTicks repeat) {
  CRcRequest *req = new CRcRequest ((CRcValueState *) NULL, reqGid, priority, t0, RCREQ_NONE, repeat);
  req->SetForTrigger ();
  SetRequest (req);
}


void CResource::SetTriggerFromStr (const char *reqDef) {
  CRcRequest *req = new CRcRequest ();
  if (reqDef) req->SetFromStr (reqDef);
  req->SetForTrigger ();
  SetRequest (req);
}


void CResource::DelRequest (const char *reqGid, TTicks t1) {
  if (DelRequestNoEvaluate (reqGid, t1)) EvaluateRequests ();
}


void CResource::GetRequest (CRcRequest *ret, const char *reqGid, bool allowNet) {
  CRcRequest *req;

  // Sanity...
  ASSERT (ret != NULL && reqGid != NULL);

  ret->Reset ();

  // Local resource ...
  if (rcDriver) {
    Lock ();
    for (req = requestList; req; req = req->next) if (req->gid.Compare (reqGid) == 0) {

      // Success: Return ...
      *ret = *req;
      Unlock ();
      ret->next = NULL;
      return;
    }

    // Failure ...
    Unlock ();
  }

  // Remote resource ...
  //   TBD: Shorten this when request events are implemented!
  else if (rcHost && allowNet) {
    CSplitString info;
    CString s;
    const char *p;
    int n;
    bool ok;

    GetInfo (&s, 1, true);
    info.Set (s.Get (), INT_MAX, "\n");
    for (n = 0; n < info.Entries (); n++) {
      p = info.Get (n);
      while (p[0] == ' ') p++;
      if (p[0] == '!') {      // have a request line?
        ok = true;
        while (ok && p[0] != ')') {
          if (!p[0]) ok = false;
          p++;
        }
        if (ok) {
          p++;
          while (p[0] == ' ') p++;
          ok = ret->SetFromStr (p);
        }
        if (!ok)
          WARNINGF (("Syntax error in resource info string line: '%s'", s.Get ()));
        else {
          if (ret->gid.Compare (reqGid) == 0) {

            // Success: Return ...
            ret->Convert (this);    // Convert type
            return;
          }
        }
        ret->Reset ();
      }
    }
  }
}





// **** For drivers *****


void CResource::NotifySubscribers (int evType) {
  Lock ();
  NotifySubscribersAL (evType);
  Unlock ();
}


void CResource::NotifySubscribersAL (int evType) {
  CRcEvent ev;
  CRcSubscriberLink *sl;
  CRcSubscriber *subscr;
  bool wasConnected;

  //~ INFOF (("### NotifySubscribers: vs = '%s'", valueState.ToStr ()));
  ev.Set ((ERcEventType) evType, this, &valueState);
  for (sl = subscrList; sl; sl = sl->next) {

    // Track "connected" status with the subscriber link...
    wasConnected = sl->isConnected;
    if (evType == rceConnected) sl->isConnected = true;
    if (evType == rceDisconnected) sl->isConnected = false;

    // Notify subscriber, but avoid repeating 'rceConnected' or 'rceDisconnected' events...
    if ((evType != rceConnected && evType != rceDisconnected) || sl->isConnected != wasConnected) {
      subscr = sl->subscr;
      subscr->Lock ();
      subscr->NotifyAL (&ev);
      subscr->Unlock ();
    }
  }
}


void CResource::ReportValueStateAL (const CRcValueState *_valueState, TTicks _timeStamp) {
  bool changed, typeError;

  //~ CString s(valueState.ToStr ());
  //~ INFOF (("##### ReportValueStateAL (%s): vs = '%s' -> '%s'", Uri (), s.Get (), _valueState ? _valueState->ToStr () : "(NULL)"));

  changed = typeError = false;

  // Change value and state for triggers ...
  if (valueState.Type () == rctTrigger) {
    // For triggers, '_valueState' can be NULL or '*_valueState' unknown, in which case
    // the call is silently ignored and not reported.
    if (_valueState) if (_valueState->IsKnown ()) {
      if (_valueState->Type () != rctTrigger) typeError = true;
      else {
        changed = true;
        valueState.SetTrigger (_valueState->Trigger ());
      }
    }
  }

  // Change value and state (general case) ...
  else {
    if (!_valueState) {
      // Empty argument: Report unkown ...
      changed = valueState.IsKnown ();
      valueState.Clear ();
    }
    else if (_valueState->Type () == rctNone) {
      // Value/type was empty: Only report new state ...
      if (valueState.state == _valueState->state || valueState.state == rcsUnknown) changed = false;    // a previously unknown state remains so
      else {
        valueState.state = _valueState->state;
        changed = true;
      }
    }
    else {
      // General case ...
      CRcValueState _valueStateConverted;
      if (_valueState->Type () != Type ()) {
        _valueStateConverted.Set (_valueState);
        if (!_valueStateConverted.Convert (Type ())) typeError = true;
        else _valueState = &_valueStateConverted;
      }
      if (!typeError) {
        changed = !valueState.Equals (_valueState);
        if (changed) valueState = *_valueState;   // Be careful to not overwrite the time stamp if there is no change!
      }
    }
  }

  // Log warnings ...
  if (typeError) {
    CString s;
    WARNINGF (("Failed to report value '%s' for resource '%s': Incompatible type!", _valueState->ToStr (&s), Uri ()));
    return;
  }

  // If changed: Set time stamp and notify subscribers...
  if (changed) {
    valueState.SetTimeStamp (_timeStamp ? _timeStamp : TicksNow ());
    NotifySubscribersAL (rceValueStateChanged);
  }
}


void CResource::ReportValueState (const CRcValueState *_valueState) {
  Lock ();          // Lock resource
  ReportValueStateAL (_valueState);
  Unlock ();        // Unlock resource
}


void CResource::ReportValue (bool _value, ERcState _state) {
  CRcValueState vs (rctBool, _value, _state);
  Lock ();
  ReportValueStateAL (&vs);
  Unlock ();
}


void CResource::ReportValue (int _value, ERcState _state) {
  CRcValueState vs (rctInt, _value, _state);
  Lock ();
  ReportValueStateAL (&vs);
  Unlock ();
}


void CResource::ReportValue (float _value, ERcState _state) {
  CRcValueState vs (rctFloat, _value, _state);
  Lock ();
  ReportValueStateAL (&vs);
  Unlock ();
}


void CResource::ReportValue (const char *_value, ERcState _state) {
  CRcValueState vs;

  vs.SetState (_state);
  if (RcTypeIsStringBased (Type ())) {
    // Shortcut to avoid copying a larger string twice...
    vs.SetGenericString (NULL, Type ());
    vs.val.vString = _value;
    Lock ();
    ReportValueStateAL (&vs);
    Unlock ();
    vs.val.vString = NULL;
  }
  else {
    // Normal (safe) way...
    vs.SetGenericString (_value, Type ());
    Lock ();
    ReportValueStateAL (&vs);
    Unlock ();
  }
}


void CResource::ReportValue (TTicks _value, ERcState _state) {
  CRcValueState vs (rctTime, _value, _state);
  Lock ();
  ReportValueStateAL (&vs);
  Unlock ();
}


void CResource::ReportTrigger () {
  CRcValueState vs (rctTrigger, 0);
  Lock ();
  if (valueState.IsKnown ()) vs.SetTrigger (valueState.Trigger () + 1);
  ReportValueStateAL (&vs);
  Unlock ();
}


void CResource::ReportNetLost () {
  CRcValueState vs;
  TTicks tLast, tNew;

  Lock ();
  if (valueState.State () != rcsUnknown) {
    vs.Clear (Type ());
    tLast = valueState.TimeStamp ();
    ASSERT (rcHost);
    tNew = rcHost->LastAlive ();      // last time the host is known to be alive
    //~ if (!tNew) tNew = TicksNow () - envMaxAge;  // (should not be necessary)
    if (tNew < tLast) tNew = tLast;   // ... but not before the last known time stamp (should not happen, but ensures monotonic behaviour).
    ReportValueStateAL (&vs, tNew);
  }
  Unlock ();
}


void CResource::DriveValue (CRcValueState *vs, bool force) {

  //~ INFOF (("### CResource::DriveValue ('%s', '%s')", Uri (), vs->ToStr ()));

  // Sanity...
  ASSERT (rcDriver != NULL && Type () != rctNone);
    // This method should only be called for local registered resources which must have a driver!

  // Drive via driver...
  Lock ();
  if (force || !valueState.ValueEquals (vs) || !valueState.IsValid () || !vs->IsValid () || Type () == rctTrigger) {
    //~ INFOF (("### CResource::DriveValue ('%s', '%s')", Uri (), vs->ToStr (true)));
    if (Type () == rctTrigger) {
      if (vs->IsKnown ()) vs->SetTrigger (valueState.Trigger () + 1);
    }
    rcDriver->DriveValue (this, vs);
    // Note: The driver may have changed 'vs' to report a busy state or changes due to hardware.
    if (vs->IsKnown ()) ReportValueStateAL (vs);
      // report the value (if known)
  }
  //~ else INFO ("###    ... skipping (not new)");
  Unlock ();
}



// ***** For directory services *****


const char *CResource::GetInfo (CString *ret, int verbosity, bool allowNet) {
  CRcSubscriberLink *sl;
  CRcRequest *req;
  CString s;

  //~ INFOF (("### CResource::GetInfo ('%s')", Uri ()));
  if (!rcHost || !allowNet) {

    // Local resource (or locally available information on remote resource)...
    Lock ();
    ret->SetF ("%s[%s,%s%s] = %s%s\n",
               Uri (), RcTypeGetName (Type ()), writable ? "wr" : "ro", persistent ? ",p" : CString::emptyStr,
               valueState.ToStr (&s, false, true, false, 20), rcHost ? " (local)" : "");
    if (verbosity >= 1) {
      if (!requestList) ret->Append ("  (no requests)\n");
      else for (req = requestList; req; req = req->next) ret->AppendF ("  ! %s\n", req->ToStr (&s, false, true));

      if (!subscrList) ret->Append ("  (no subscriptions)\n");
      else for (sl = subscrList; sl; sl = sl->next) ret->AppendF ("  ? %s\n", sl->subscr->Gid ());
    }
    Unlock ();
  }
  else {

    // Remote resource...
    if (!rcHost->RemoteInfoResource (this, verbosity, ret)) {
      Lock ();
      ret->SetF ("%s[%s,%s] = %s\n  (host unreachable)\n",
                 Uri (), RcTypeGetName (Type ()), writable ? "wr" : "ro",
                 valueState.ToStr (&s, false, true, false, 20));
      Unlock ();
    }
  }
  return ret->Get ();
}


void CResource::PrintInfo (FILE *f, int verbosity, bool allowNet) {
  CString info;

  GetInfo (&info, verbosity, allowNet);
  fprintf (f, "%s", info.Get ());
}


int CResource::LockLocalSubscribers () {
  CRcSubscriberLink *sl;
  int n;

  Lock ();
  for (n = 0, sl = subscrList; sl; sl = sl->next) n++;
  return n;
}


CRcSubscriber *CResource::GetLocalSubscriber (int n) {
  CRcSubscriberLink *sl = subscrList;
  while (n-- > 0) sl = sl->next;
  return sl->subscr;
}


int CResource::LockLocalRequests () {
  CRcRequest *req;
  int n;

  Lock ();
  for (n = 0, req = requestList; req; req = req->next) n++;
  return n;
}


CRcRequest *CResource::GetLocalRequest (int n) {
  CRcRequest *req = requestList;
  while (n-- > 0) req = req->next;
  return req;
}



// ***** EvaluateRequests *****


void CResourceRequestsTimerCallback (CTimer *, void *data) {
  CResource *rc = (CResource *) data;
  rc->EvaluateRequests ();
}


CRcRequest *CResource::GetWinningRequest (TTicks t) {
  CRcRequest *req, *maxReq;
  int maxPrio;

  maxReq = NULL;
  maxPrio = -1;
  for (req = requestList; req; req = req->next) if (req->IsCompatible ())
    if (req->priority >= maxPrio && t >= req->t0 && (!req->t1 || t < req->t1)) {
      maxReq = req;
      maxPrio = req->priority;
    }
  return maxReq;
}


void CResource::EvaluateRequests (bool force) {
  CRcRequest *req, *bestReq, *finalReq, **pReq, **pBestReq;
  CRcValueState finalValueState;
  TTicks curTime, nextTime, bestTime;
  TTicksMonotonic curTicks, t;

  // NOTE on race conditions:
  //   We must make sure that any new value we drive here only depends on the request set, but never on the
  //   current value of the resource (which may be reported randomly by the driver). Otherwise, a feedback loop
  //   may be closed between the request(or)s and the driver. This becomes dangerous if a value change
  //   - caused by a past 'Evaluate/Drive...' operation
  //   - causes a 'Report...' operation in the future,
  //   - where the reported value is not necessarily equal to the driven one (drivers are allowed to deviate).

  //~ INFOF(("### '%s'->EvaluateRequests (force = %i) ...", Gid (), (int) force));
  //~ LogStack ();

  // Sanity...
  if (!Driver () || Type () == rctNone || !IsWritable ()) return;
    // If the type is 'rctNone', this resource has not been registered yet.
    // The evaluation will be triggered again after registration.

  // Lock ...
  //   'this' will be kept locked during the complete evaluation process.
  Lock ();
  curTime = TicksNow ();              // Absolute time in milliseconds since epoch
  curTicks = TicksMonotonicNow ();    // Semi-absolute time in milliseconds

  // Handle repetitions: Update 't0' / 't1' based on 'repeat' attributes ...
  for (req = requestList; req; req = req->next)
    if (req->repeat && req->t0 && req->t1) {
      // Note: We do not update a persistent request afterwards here, we rely on the fact that
      //       t0 and t1 are always updated appropriately here, even if their original
      //       values are very much back in the past.
      // Repeat back in time ...
      while (req->t1 - req->repeat > curTime) req->t1 -= req->repeat;
      while (req->t0 > req->t1) req->t0 -= req->repeat;
      // Repeat forward in time ...
      while (req->t1 <= curTime) {      // '<=' (and not '<') is important to not have it removed below!!
        req->t0 += req->repeat;
        req->t1 += req->repeat;
      }
    }

  // Evaluate ...
  if (Type () == rctTrigger) {

    // Case 1: Triggers (are handled differently) ...

    // Find earliest trigger before 'curTime' ...
    bestTime = curTime;
    pBestReq = NULL;
    for (pReq = &requestList; *pReq; pReq = &((*pReq)->next)) {
      // We do not need to check for incompatible requests, since we drive a fresh value generated by 'CRcValueState::SetTrigger()'.
      req = *pReq;
      if (req->t0 <= bestTime) {    // the last element in the list dominates (= earliest inserted)
        pBestReq = pReq;
        bestTime = req->t0;
      }
    }
    if (pBestReq) {

      // Remove that trigger...
      req = *pBestReq;
      if (req->repeat) {
        while (req->t0 <= curTime) req->t0 += req->repeat;   // update time for next occurence
        if (persistent) UpdatePersistentRequestAL (req->Gid (), req);
      }
      else {
        if (persistent) UpdatePersistentRequestAL (req->Gid (), NULL);
        *pBestReq = req->next;
        delete req;
      }

      // Let trigger happen...
      finalValueState.SetTrigger ();
    }
  }
  else {

    // Case 2: Normal values (non-triggers)...

    // Remove all obsolete requests...
    pReq = &requestList;
    while (*pReq) {
      req = *pReq;
      if (req->t1 > 0 && req->t1 <= curTime) {    // 't1' is exclusive: if equal, we can already delete
        if (persistent) UpdatePersistentRequestAL (req->Gid (), NULL);
        *pReq = req->next;
        delete req;
      }
      else pReq = &((*pReq)->next);
    }

    // Find currently active request with highest priority...
    finalReq = GetWinningRequest (curTime);
    if (finalReq) {
      //~ INFOF (("###    Winning request: '%s'", finalReq->ToStr ()));

      // Check hysteresis...
      //   The current value change is not executed, if a future event within the hysteresis time
      //   dictates a different value.
      if (finalReq->hysteresis) {
        //~ INFOF(("###    ... hysteresis = %i", finalReq->hysteresis));
        for (req = requestList; req; req = req->next) if (req->IsCompatible ()) {
          if (req->t0 && req->t0 > curTime && req->t0 <= curTime + finalReq->hysteresis) {  // starting time in the future during the hysteresis period?
            bestReq = GetWinningRequest (req->t0);                                          // get the winner at that time
            if (!finalReq->value.Equals (&bestReq->value)) { finalReq = NULL; break; }      // future winner dictates a value different from now's winner
          }
          if (req->t1 && req->t1 > curTime && req->t1 <= curTime + finalReq->hysteresis) {  // stop time in the future during the hysteresis period?
            bestReq = GetWinningRequest (req->t1);                                          // get the winner at that time
            if (!finalReq->value.Equals (&bestReq->value)) { finalReq = NULL; break; }      // future winner dictates a value different from now's winner
          }
        }
      }

      // Set value as final value if no hysteresis drop applies...
      if (finalReq) {
        finalValueState.Set (&finalReq->value);
        ASSERT (finalValueState.State () == rcsValid);
      }
      //~ else INFOF (("###    ... dropped due to hysteresis (%i)", finalReq->hysteresis));
    }
  }

  // Determine time of next check and set timer...
  nextTime = 0;   // 0 == none
  for (req = requestList; req; req = req->next) {
    if (req->t0 > curTime && (!nextTime || req->t0 < nextTime)) nextTime = req->t0;
    if (req->t1 > curTime && (!nextTime || req->t1 < nextTime)) nextTime = req->t1;
    //~ INFOF (("### Request '%s' -> curTime = %lli, nextTime = %lli", req->ToStr (), curTime, nextTime));
  }
  if (nextTime > 0) {
    t = curTicks + (nextTime - curTime);
    //~ INFOF (("'CResourceRequestsTimerCallback' in %i millis", t - curTicks));
    requestTimer.Set (t, 0, CResourceRequestsTimerCallback, this);
  }
  else requestTimer.Clear ();

  // Unlock...
  Unlock ();

  // Drive the value (cannot be done when 'this' is locked) ...
  DriveValue (&finalValueState, force);
}





// *************************** CRcEvent ****************************************


void CRcEvent::Set (ERcEventType _type, CResource *_resource, CRcValueState *_valueState, void *_data) {
  morePending = false;
  next = NULL;
  type = _type;
  resource = _resource;
  SetValueState (_valueState);
  data = _data;
}


void CRcEvent::SetValueState (CRcValueState *_valueState) {
  if (_valueState) valueState = *_valueState;
  else valueState.Clear ();
}


const char *CRcEvent::ToStr (CString *ret) {
  CString s;

  switch (type) {
    case rceNone:
      ret->SetC ("None");
      break;
    case rceTimer:
      ret->SetF ("Timer alarm (0x%08x)", data);
      break;
    case rceValueStateChanged:
      ret->SetF ("%s = %s", resource->Uri (), valueState.ToStr (&s, false, true));
      break;
    case rceDisconnected:
      ret->SetF ("%s disconnected", resource->Uri ());
      break;
    case rceConnected:
      ret->SetF ("%s connected", resource->Uri ());
      break;
    case rceDriveValue:
      ret->SetF ("Drive %s = %s", resource->Uri (), valueState.ToStr (&s));
      break;
    default:
      ret->SetC ("???");
  }
  return ret->Get ();
}


const char *CRcEvent::ToStr () {
  return ToStr (GetTTS ());
}





// *************************** CRcEventProcessor *******************************


CMutex CRcEventProcessor::globMutex;
CCond CRcEventProcessor::globCond;
CRcEventProcessor *CRcEventProcessor::firstProc = NULL;
CRcEventProcessor **CRcEventProcessor::pLastProc = &CRcEventProcessor::firstProc;




// ***** Con-/Destructor *****


CRcEventProcessor::CRcEventProcessor (bool _inSelectSet) {
  firstEv = NULL;
  pLastEv = &firstEv;
  cbEvent = NULL;
  cbEventData = NULL;
  inSelectSet = _inSelectSet;
  next = NULL;
}


CRcEventProcessor::~CRcEventProcessor () {
  //~ INFOF(("### ~CRcEventProcessor ('%s'/%08x)", InstId (), this));
  globMutex.Lock ();      // This will wait (amoung others) if an OnEvent() instance is still running
  while (firstEv) DeleteFirstEventAL ();
  UnlinkAL ();
  globMutex.Unlock ();
}



// ***** Putting *****


void CRcEventProcessor::PutEvent (CRcEvent *ev) {
  CRcEvent *qev, lev;
  bool handled;

  //~ INFOF (("### PutEvent (%s, '%s')...", InstId (), ev->ToStr ()));

  // Lock...
  //   Note: It is very important to keep the lock for the complete procedure and embrace the callback AND the enqueuing.
  //   Otherwise, very annoying races can occur, in which the callback triggers an event to another thread, which then polls and
  //   may not receive this new event!
  globMutex.Lock ();

  // Invoke callback...
  //~ INFOF (("###   invoking callback...", InstId (), ev->ToStr ()));
  handled = OnEvent (ev);

  // Enqueue new event unless the callback has handled it...
  if (!handled) {
    //~ INFOF (("###   enqueuing event...", InstId (), ev->ToStr ()));

    // Create event object and append to list...
    qev = new CRcEvent ();
    *qev = *ev;
    qev->next = NULL;

    // Append to list...
    (*pLastEv) = qev;
    pLastEv = &qev->next;

    // Wake up eventually waiting threads...
    if (qev == firstEv) {
      // we added the first new element to an empty queue...
      //~ INFO ("###   -> first event to empty queue");
      cond.Signal ();               // wake up an eventually waiting thread
      if (inSelectSet) {
        //~ INFO ("###   -> ... and in select set");
        LinkAL ();   // consider in 'Select'
        globCond.Signal ();           // eventually wake up 'Select'
      }
    }
  } // if (!handled)

  // Unlock..
  globMutex.Unlock ();
}





// ***** Polling, Waiting and Callbacks *****


void CRcEventProcessor::DeleteFirstEventAL () {
  //~ INFOF (("### DeleteFirstEventAL ('%s')", InstId ()));
  ASSERT (firstEv != NULL);
  CRcEvent *vic = firstEv;
  firstEv = firstEv->next;
  delete vic;
  if (!firstEv) pLastEv = &firstEv;
}


bool CRcEventProcessor::DoPollEventAL (CRcEvent *ev) {
  bool ok;

  //~ INFOF(("### DoPollEventAL ('%s'): %s", InstId (), firstEv ? firstEv->ToStr () : " nothing pending"));

  // If requested and available: return and consume first event...
  ok = false;
  if (firstEv) {      // event available?
    ok = true;
    if (ev) {         // return and consume the event?
      //~ INFOF (("###   returning and consuming it."));
      *ev = *firstEv;
      ev->next = NULL;
      DeleteFirstEventAL ();
      ev->morePending = (firstEv ? true : false);
    }
    //~ else INFOF (("###   not touching it."));
  }

  // Check if more events are pending...
  if (firstEv) {
    //~ INFOF (("###   (more events are pending)"));
    cond.Signal ();       // more events available: wake up some other thread that may want to use it
    globCond.Signal ();
  }
  else UnlinkAL ();       // no more events availabe: unlink from processor list

  return ok;
}


bool CRcEventProcessor::PollEvent (CRcEvent *ev) {
  bool ret;

  globMutex.Lock ();
  ret = DoPollEventAL (ev);
  globMutex.Unlock ();
  return ret;
}


bool CRcEventProcessor::WaitEvent (CRcEvent *ev, int *maxTime) {
  int timeLeft;
  bool haveEvent;

  haveEvent = false;
  timeLeft = maxTime ? *maxTime : INT_MAX;
  globMutex.Lock ();
  interrupted = false;
  while (!haveEvent && !interrupted && (!maxTime || timeLeft > 0)) {
    haveEvent = DoPollEventAL (ev);
    if (!haveEvent) {
      if (maxTime) timeLeft = cond.Wait (&globMutex, timeLeft);
      else cond.Wait (&globMutex);
    }
  }
  globMutex.Unlock ();
  if (maxTime) *maxTime = timeLeft;
  return haveEvent;
}


void CRcEventProcessor::Interrupt () {
  interrupted = true;
  cond.Broadcast ();
}


void CRcEventProcessor::FlushEvents () {
  CRcEvent ev;

  globMutex.Lock ();      // This will wait (amoung others) if an OnEvent() instance is still running
  while (DoPollEventAL (&ev)) {}
  globMutex.Unlock ();
}


bool CRcEventProcessor::OnEvent (CRcEvent *ev) {
  if (cbEvent) return cbEvent (this, ev, cbEventData);
  else return false;
}


void CRcEventProcessor::SetCbOnEvent (FRcEventFunc *_cbEvent, void *_cbEventData) {
  globMutex.Lock ();
  cbEvent = _cbEvent;
  cbEventData = _cbEventData;
  globMutex.Unlock ();
}



// ***** Global event loop support *****


void CRcEventProcessor::LinkAL () {
  if (IsLinkedAL ()) return;
  //~ INFOF (("### Linking '%s'/%08x...", InstId (), this));
  //~ INFOF (("###   this = %08x, &next = %08x, next = %08x, &firstProc = %08x, firstProc = %08x, pLastProc = %08x", this, &next, next, &firstProc, firstProc, pLastProc));
  next = *pLastProc;
  *pLastProc = this;
  pLastProc = &next;
  //~ INFOF (("###   this = %08x, &next = %08x, next = %08x, &firstProc = %08x, firstProc = %08x, pLastProc = %08x", this, &next, next, &firstProc, firstProc, pLastProc));
  ASSERT (IsLinkedAL ());
}


void CRcEventProcessor::UnlinkAL () {
  CRcEventProcessor **pThis;

  if (!IsLinkedAL ()) return;
  //~ INFOF (("### Unlinking '%s'/%08x...", InstId (), this));
  //~ INFOF (("###   this = %08x, &next = %08x, next = %08x, &firstProc = %08x, firstProc = %08x, pLastProc = %08x", this, &next, next, &firstProc, firstProc, pLastProc));
  for (pThis = &firstProc; *pThis != this; pThis = &((*pThis)->next)) ASSERT (*pThis != NULL);
  *pThis = next;
  if (pLastProc == &next) pLastProc = pThis;
  next = NULL;
  //~ INFOF (("###   this = %08x, &next = %08x, next = %08x, &firstProc = %08x, firstProc = %08x, pLastProc = %08x", this, &next, next, &firstProc, firstProc, pLastProc));
  ASSERT (!IsLinkedAL ());
}


void CRcEventProcessor::SetInSelectSet (bool _inSelectSet) {
  globMutex.Lock ();
  if (!_inSelectSet) UnlinkAL ();
  else if (firstEv) LinkAL ();
  inSelectSet = _inSelectSet;
  globMutex.Unlock ();
}


CRcEventProcessor *CRcEventProcessor::Select (TTicksMonotonic maxTime) {
  TTicksMonotonic timeLeft;

  timeLeft = maxTime;
  //~ INFOF(("# CRcEventProcessor::Select (%i)", timeLeft));
  globMutex.Lock ();
  do {

    // Check list with processors owning pending events...
    while (firstProc) {
      //~ INFOF(("# Trying '%s'/'%s'...", firstProc->TypeId (), firstProc->InstId ()));
      if (firstProc->DoPollEventAL (NULL)) {
        // The above check should always (TBD: proof) (and definitely does mostly) return 'true'.
        // Hence, we could skip it here and document that the caller should not worry about spurious returns.
        // For efficiency reasons (outer loop may be in a high-level language, e.g. Python), we leave the small loop
        // here. TBD: Check, if we can replace the check with an 'ASSERT()'.

        // 'firstProc' is a valid candidate: return with success...
        //~ INFO("# ... Done: Success");
        globMutex.Unlock ();
        return firstProc;
      }
      else {
        //~ INFOF(("# -> nothing"));
        firstProc->UnlinkAL ();   // Entry has no pending events and is irrelevant: unlink it
      }
    }

    // Wait for signalling or until the time left is over...
    if (timeLeft > 0) timeLeft = globCond.Wait (&globMutex, timeLeft);
    else if (maxTime < 0) globCond.Wait (&globMutex);
  } while (maxTime < 0 || timeLeft > 0);

  //~ INFO("# ... Done: Timeout");
  globMutex.Unlock ();
  return NULL;
}





// *************************** CRcSubscriber ***********************************


bool CRcSubscriber::Register (const char *_lid) {
  ASSERTM (gid.IsEmpty (), "Unable to register subscriber twice");

  //~ INFOF (("### CRcSubscriber::Register ('%s')", _lid));
  // Sanity + set LID ...
  if (!_lid) _lid = CString::emptyStr;
  if (!_lid[0]) lid.SetF ("s%08x", (unsigned long) this);
  else {
    if (!IsValidIdentifier (_lid, true)) {
      WARNINGF(("Invalid subscriber ID '%s' - registration failed!", _lid));
      return false;
    }
    lid.Set (_lid);
  }

  // Set GID and register ...
  gid.SetF ("%s/%s", localHostId.Get (), lid.Get ());
  SubscriberMapLock ();
  subscriberMap.Set (lid.Get (), this);
  SubscriberMapUnlock ();

  // Success ...
  return true;
}


void CRcSubscriber::RegisterAsAgent (const char *_gid) {
  gid.Set (_gid);
  lid.SetC (gid.Get ());
  SubscriberMapLock ();
  subscriberMap.Set (lid.Get (), this);
  SubscriberMapUnlock ();
}


void CRcSubscriber::Unregister () {
  Clear ();
  SubscriberMapLock ();
  subscriberMap.Del (lid.Get ());
  SubscriberMapUnlock ();
}


const char *CRcSubscriber::ToStr (CString *ret) {
  CString s;

  ret->SetF ("%s:", Gid ());
  Lock ();
  for (CResourceLink *rc = resourceList; rc; rc = rc->next)
    ret->Append (StringF (&s, " %s", rc->resource->Uri ()));
  Unlock ();
  return ret->Get ();
}


const char *CRcSubscriber::ToStr () {
  return ToStr (GetTTS ());
}



// ***** Adding/removing resources *****


CResource *CRcSubscriber::AddResource (CResource *rc) {
  if (rc) rc->SubscribePAL (this, false, false);
  return rc;
}


CResource *CRcSubscriber::AddResources (const char *pattern) {
  CResource **selArr, *ret;
  CKeySet newWatchSet;
  char **patV;
  int n, items, patC;
  bool ok;

  // Handle multiple patterns in the 'pattern' string...
  if (!pattern) return NULL;
  if (strchr (pattern, ',')) {
    StringSplit (pattern, &patC, &patV, INT_MAX, "," WHITESPACE);
    if (patV) {
      for (n = 0; n < patC; n++) AddResources (patV[n]);
      free (patV[0]);
      free (patV);
      return NULL;
    }
  }

  // Handle single pattern...
  ret = NULL;
  ok = RcSelectResources (pattern, &items, &selArr, &newWatchSet);
  if (ok) {
    for (n = 0; n < items; n++) AddResource (selArr[n]);
    if (items == 1) ret = selArr[0];
    Lock ();
    watchSet.Merge (&newWatchSet);
    Unlock ();
  }
  else WARNINGF(("Malformed resource pattern or unresolvable alias '%s' - not subscribing anything", pattern));
  FREEP (selArr);
  return ret;
}


void CRcSubscriber::DelResource (CResource *rc) {
  if (rc) rc->UnsubscribePAL (this, false, false);
}


void CRcSubscriber::DelResources (const char *pattern) {
  CResourceLink *rl, *rlNext;
  char **patV;
  int n, patC;
  bool changed;

  // Handle multiple patterns in the 'pattern' string recursively...
  if (strchr (pattern, ' ')) {
    StringSplit (pattern, &patC, &patV);
    if (patV) {
      for (n = 0; n < patC; n++) DelResources (patV[n]);
      free (patV[0]);
      free (patV);
      return;
    }
  }

  // Now we have a single pattern.

  // Lock...
  Lock ();    // Another thread may call 'CheckNewResource' concurrently

  // Go through the watch set and remove items covered by 'pattern'...
  changed = true;
  while (changed) {
    // In order to not assume any (re-)ordering when removing items, we loop through the
    // key set until a complete pass with no item found has been performed.
    changed = false;
    for (n = 0; n < watchSet.Entries (); n++)
      if (fnmatch (pattern, watchSet.GetKey (n), URI_FNMATCH_OPTIONS) == 0) {
        watchSet.Del (n);
        changed = true;
        break;
      }
  }

  // Unsubscribe from all matching resources...
  rl = resourceList;
  while (rl) {
    rlNext = rl->next;    // '*rl' may not survive the following operations
    if (fnmatch (pattern, rl->resource->Uri (), URI_FNMATCH_OPTIONS) == 0)
      rl->resource->UnsubscribePAL (this, false, true);
    rl = rlNext;
  }

  // Unlock...
  Unlock ();
  return;
}


void CRcSubscriber::CheckNewResource (CResource *resource) {
  const char *uri;
  int n;

  uri = resource->Uri ();
  Lock ();
  for (n = 0; n < watchSet.Entries (); n++)
    if (fnmatch (watchSet.GetKey (n), uri, URI_FNMATCH_OPTIONS) == 0) {
      resource->SubscribePAL (this, false, true);    // tell 'SubscribePAL()' that this subscription is already locked
      if (strcmp (watchSet.GetKey (n), uri) == 0) watchSet.Del (uri);
      break;    // important, since we may have modified 'watchSet'
    }
  Unlock ();
}


void CRcSubscriber::UnlinkResourceAL (CResource *resource) {
  Lock ();
  watchSet.Set (resource->Uri ());
  resource->UnsubscribePAL (this, true, true);
  Unlock ();
}


void CRcSubscriber::Clear () {
  Lock ();
  while (resourceList) resourceList->resource->UnsubscribePAL (this, false, true);
  watchSet.Clear ();
  Unlock ();
}



// ***** Directory service *****


void CRcSubscriber::GetInfo (CString *ret, int verbosity) {
  CKeySet keySet;
  int n;

  ret->SetF ("Subscriber '%s'\n", Gid ());
  if (verbosity >= 1) {
    GetPatternSet (&keySet);
    if (keySet.Entries ()) {
      for (n = 0; n < keySet.Entries (); n++)
        ret->AppendF ("  %s\n", keySet.GetKey (n));
    }
    else ret->Append ("  (none)\n");
  }
}


void CRcSubscriber::GetInfoAll (CString *ret, int verbosity) {
  CRcSubscriber *subscr;
  CString single;
  int n;

  ret->Clear ();
  SubscriberMapLock ();
  if (subscriberMap.Entries ()) for (n = 0; n < subscriberMap.Entries (); n++) {
    subscr = subscriberMap.Get (n);
    subscr->GetInfo (&single, verbosity);
    ret->Append (single.Get ());
  }
  else ret->Append ("(no subscribers)");
  SubscriberMapUnlock ();
}


void CRcSubscriber::PrintInfo (FILE *f, int verbosity) {
  CString info;

  GetInfo (&info, verbosity);
  fprintf (f, "%s", info.Get ());
}


void CRcSubscriber::GetPatternSet (CKeySet *retPatternSet) {
  CResourceLink *rl;
  CString s;
  int n;

  retPatternSet->Clear ();
  Lock ();
  for (rl = resourceList; rl; rl = rl->next)
    retPatternSet->Set (rl->resource->Uri ());
  for (n = 0; n < watchSet.Entries (); n++)
    retPatternSet->Set (StringF (&s, "%s?", watchSet.GetKey (n)));
  Unlock ();
}





// *************************** CRcRequest **************************************



// ***** Setting & getting *****


void CRcRequest::Reset () {

  // Clear value and meta fields ...
  isCompatible = false;   // be defensive by default
  next = NULL;
  value.Clear ();

  // Set default attributes ...
  gid.SetC (EnvInstanceName ());
  priority = rcPrioNormal;
  t0 = t1 = NEVER;
  repeat = 0;
  hysteresis = 0;

  // Set origin stamp ...
  SetOrigin ();
}


void CRcRequest::Set (CRcRequest *req) {
  value = *(req->Value ());
  isCompatible = false;
  gid.Set (req->gid);
  priority = req->priority;
  t0 = req->t0;
  t1 = req->t1;
  repeat = req->repeat;
  hysteresis = req->hysteresis;
}


void CRcRequest::Set (CRcValueState *_value, const char *_gid, int _priority, TTicks _t0, TTicks _t1, TTicks _repeat, TTicks _hysteresis) {
  TTicks now;

  if (_value) {
    value = *_value;
    isCompatible = false;
  }
  if (_gid) gid.Set (_gid);
  if (_priority != RCREQ_NONE) priority = _priority;
  if (_t0 < 0 || _t1 < 0) {
    now = TicksNow ();
    if (_t0 != RCREQ_NONE && _t0 < 0) _t0 = now - _t0;
    if (_t1 != RCREQ_NONE && _t1 < 0) _t1 = now - _t1;
  }
  if (_t0 != RCREQ_NONE) t0 = _t0;
  if (_t1 != RCREQ_NONE) t1 = _t1;
  if (_repeat != RCREQ_NONE) repeat = _repeat;
  if (_hysteresis != RCREQ_NONE) hysteresis = _hysteresis;
}


void CRcRequest::Set (bool _value, const char *_gid, int _priority, TTicks _t0, TTicks _t1, TTicks _repeat, TTicks _hysteresis) {
  value.SetBool (_value);
  isCompatible = false;
  Set ((CRcValueState *) NULL, _gid, _priority, _t0, _t1, _repeat, _hysteresis);
}


void CRcRequest::Set (int _value, const char *_gid, int _priority, TTicks _t0, TTicks _t1, TTicks _repeat, TTicks _hysteresis) {
  value.SetInt (_value);
  isCompatible = false;
  Set ((CRcValueState *) NULL, _gid, _priority, _t0, _t1, _repeat, _hysteresis);
}


void CRcRequest::Set (float _value, const char *_gid, int _priority, TTicks _t0, TTicks _t1, TTicks _repeat, TTicks _hysteresis) {
  value.SetFloat (_value);
  isCompatible = false;
  Set ((CRcValueState *) NULL, _gid, _priority, _t0, _t1, _repeat, _hysteresis);
}


void CRcRequest::Set (const char *_value, const char *_gid, int _priority, TTicks _t0, TTicks _t1, TTicks _repeat, TTicks _hysteresis) {
  value.SetString (_value);
  isCompatible = false;
  Set ((CRcValueState *) NULL, _gid, _priority, _t0, _t1, _repeat, _hysteresis);
}


void CRcRequest::Set (TTicks _value, const char *_gid, int _priority, TTicks _t0, TTicks _t1, TTicks _repeat, TTicks _hysteresis) {
  value.SetTime (_value);
  isCompatible = false;
  Set ((CRcValueState *) NULL, _gid, _priority, _t0, _t1, _repeat, _hysteresis);
}


void CRcRequest::SetValue (CRcValueState *_value) {
  value = *_value;
  isCompatible = false;
}


void CRcRequest::SetValue (bool _value) {
  value.SetBool (_value);
  isCompatible = false;
}


void CRcRequest::SetValue (int _value) {
  value.SetInt (_value);
  isCompatible = false;
}


void CRcRequest::SetValue (float _value) {
  value.SetFloat (_value);
  isCompatible = false;
}


void CRcRequest::SetValue (const char *_value) {
  value.SetString (_value);
  isCompatible = false;
}


void CRcRequest::SetValue (TTicks _value) {
  value.SetTime (_value);
  isCompatible = false;
}


void CRcRequest::SetForTrigger () {
  value.SetTrigger ();
  t1 = NEVER;
  hysteresis = 0;
  isCompatible = false;
}



// ***** Stringification *****


bool CRcRequest::SetSingleAttrFromStr (const char *str) {
  const char *p;
  bool ok = true;

  switch (str[0]) {
    case '#':
      gid.Set (str + 1);
      break;
    case '*':
      ok = IntFromString (str + 1, &priority);
      break;
    case '+':     // t0 and (optionally) repeat ...
      p = strchr (str + 1, '+');
      if (!p) p = str + 1;      // no repeat value
      else {
        if (p == str + 1)
          repeat = TICKS_FROM_SECONDS(TIME_OF (24,0,0));    // empty repeat expression -> 1 day
        else
          ok = TicksRelFromString (str + 1, &repeat);       // ticks value
      }
      if (ok) ok = TicksAbsFromString (p, &t0);
      break;
    case '-':
      ok = TicksAbsFromString (str + 1, &t1);
      break;
    case '~':
      ok = TicksRelFromString (str + 1, &hysteresis);
      break;
    case '@':
      origin.Set (str + 1);
      //~ INFOF(("### CRcRequest::SetFromStr: Set origin = '%s'", origin.Get ()));
      break;
    default:
      ok = false;
  }
  return ok;
}


bool CRcRequest::SetFromStr (const char *str) {
  char **argv;
  int n, argc;
  bool ok;

  //~ INFOF ( ("### CRcRequest::SetFromStr ('%s')", str));
  if (!str) return false;
  StringSplit (str, &argc, &argv);
  ok = (argc >= 1);

  // Value ...
  if (ok) {
    value.Clear (rctNone);
    //~ INFOF (("### CRcRequest::SetFromStr () -> value '%s'", argv[1]));
    ok = value.SetFromStr (argv[0]);
  }

  // Optional parameters ...
  for (n = 1; n < argc && ok; n++)
    ok = SetSingleAttrFromStr (argv[n]);

  // Warn & finish ...
  if (!ok) WARNINGF(("Malformed request specification '%s'", str));
  if (argv) {
    free (argv[0]);
    free (argv);
  }
  return ok;
}


bool CRcRequest::SetAttrsFromStr (const char *str) {
  char **argv;
  int n, argc;
  bool ok;

  //~ INFOF ( ("### CRcRequest::SetFromStr ('%s')", str));
  if (!str) return false;
  StringSplit (str, &argc, &argv);
  ok = true;

  // Optional parameters ...
  for (n = 0; n < argc && ok; n++)
    ok = SetSingleAttrFromStr (argv[n]);

  // Warn & finish ...
  if (!ok) WARNINGF(("Malformed attributes specification '%s'", str));
  if (argv) {
    free (argv[0]);
    free (argv);
  }
  return ok;
}


const char *CRcRequest::ToStr (CString *ret, bool precise, bool tabular, TTicks relativeTimeThreshold, const char *skipAttrs) {
  CString s;
  TTicks now = NEVER;

  ret->SetF (tabular ? "%-16s" : "%s", value.ToStr (&s, tabular, false, precise, 16));
  if (!gid.IsEmpty ()) if (!strchr (skipAttrs, '#')) ret->AppendF (tabular ? " #%-12s" : " #%s", gid.Get ());
  if (priority != RCREQ_NONE) if (!strchr (skipAttrs, '*')) ret->AppendF (" *%i", priority);
  if (relativeTimeThreshold && (t0 != NEVER || t1 != NEVER)) now = TicksNow ();
  if (t0) if (!strchr (skipAttrs, '+')) {
    ret->Append (" +");
    if (repeat) {
      if (repeat != TICKS_FROM_SECONDS(TIME_OF(24,0,0)))      // skip 1 day (implicit)
        ret->Append (TicksRelToString (&s, repeat));
      ret->Append ('+');
    }
    if (relativeTimeThreshold && t0 > now && t0 - now <= relativeTimeThreshold)
      ret->Append (TicksRelToString (&s, t0 - now));
    else
      ret->Append (TicksAbsToString (&s, t0));
  }
  if (t1) if (!strchr (skipAttrs, '-')) {
    ret->Append (" -");
    if (relativeTimeThreshold && t1 > now && t1 - now <= relativeTimeThreshold)
      ret->Append (TicksRelToString (&s, t1 - now));
    else
      ret->Append (TicksAbsToString (&s, t1));
  }
  if (hysteresis) if (!strchr (skipAttrs, '~')) ret->AppendF (" ~%s", TicksRelToString (&s, hysteresis));
  if (!strchr (skipAttrs, '@')) ret->AppendF (tabular ? "   @%s" : " @%s", origin.Get ());
  if (!isCompatible) if (!strchr (skipAttrs, 'i')) ret->Append (" (incompatible)");
  return ret->Get ();
}


const char *CRcRequest::ToStr (bool precise, bool tabular, TTicks relativeTimeThreshold, const char *skipAttrs) {
  return ToStr (GetTTS (), precise, tabular, relativeTimeThreshold, skipAttrs);
}


void CRcRequest::Convert (CResource *rc, bool warn) {
  isCompatible = value.Convert (rc->Type ());
  if (!isCompatible && warn)
    WARNINGF (("Request '%s' to resource '%s' has incompatible type and will have no effect.", ToStr (), rc->Uri ()));
}



// ***** Origin *****


void CRcRequest::SetOrigin () {
  CString s;
  origin.SetF ("%s/%s", localHostId.Get (), TicksAbsToString (&s, TicksNow (), 0));
}





// ************ Directory operations and host/driver/resource lookup ***********


//  ***** Hosts *****


int RcGetHosts () {
  return hostMap.Entries ();
}


CRcHost *RcGetHost (int n) {
  return hostMap.Get (n);
}


CRcHost *RcGetHost (const char *id) {
  return hostMap.Get (id);
}


const char *RcGetHostId (CRcHost *host) {
  return host ? host->Id () : NULL;
}



// ***** Drivers *****


int RcGetDrivers () {
  return driverMap.Entries ();
}


CRcDriver *RcGetDriver (int n) {
  return driverMap.Get (n);
}


CRcDriver *RcGetDriver (const char *lid) {
  return driverMap.Get (lid);
}


const char *RcGetDriverId (CRcDriver *driver) {
  return driver ? driver->Lid () : NULL;
}



// ***** Resources *****


int RcLockHostResources (CRcHost *host) {
  return host ? host->LockResources () : 0;
}


CResource *RcGetHostResource (CRcHost *host, int n) {
  return host ? host->GetResource (n) : NULL;
}


void RcUnlockHostResources (CRcHost *host) {
  if (host) host->UnlockResources ();
}


int RcLockDriverResources (CRcDriver *driver) {
  return driver ? driver->LockResources () : 0;
}


CResource *RcGetDriverResource (CRcDriver *driver, int n) {
  return driver ? driver->GetResource (n) : NULL;
}


void RcUnlockDriverResources (CRcDriver *driver) {
  if (driver) driver->UnlockResources ();
}



// ***** Subscribers *****


int RcLockSubscribers () {
  SubscriberMapLock ();
  return subscriberMap.Entries ();
}


CRcSubscriber *RcGetSubscriber (int n) {
  return subscriberMap.Get (n);
}


void RcUnlockSubscribers () {
  SubscriberMapUnlock ();
}





// *************************** CRcDriver ***************************************



// ***** Life cycle *****


void CRcDriver::Register () {
  DEBUGF (1, ("Registering driver '%s'.", lid.Get ()));
  if (!IsValidIdentifier (lid.Get (), false))
    ERRORF(("CRcDriver::Register (): Invalid driver ID '%s'", lid.Get ()));
  if (rcInitCompleted)
    ERRORF (("Registration attempt for driver '%s' after the initialization phase.", lid.Get ()));
  if (driverMap.Get (lid.Get ()))
    ERRORF (("Redefinition of driver '%s'.", lid.Get ()));
  driverMap.Set (lid.Get (), this);
}


void CRcDriver::RegisterAndInit (const char *_lid, FRcDriverFunc *_func) {
  CRcDriver *drv = new CRcDriver (_lid, _func);
  drv->Register ();
  if (drv->func) drv->func (rcdOpInit, drv, NULL, NULL);
}


void CRcDriver::Unregister () {
  DEBUGF (1, ("Unregistering driver '%s'.", lid.Get ()));
  ClearResources ();
  driverMap.Del (lid.Get ());
}


void CRcDriver::ClearResources () {
  CResource *rc;
  int n;

  Lock ();
  while ((n = resourceMap.Entries ()) > 0) {
    rc = resourceMap.Get (n - 1);
    Unlock ();      // 'rc->Unregister' may lock 'this' again
    rc->Unregister ();
    Lock ();
  }
  Unlock ();
}



// ***** Interface methods *****


void CRcDriver::Stop () {
  if (func) func (rcdOpStop, this, NULL, NULL);
}


void CRcDriver::DriveValue (CResource *rc, CRcValueState *vs) {
  if (func) func (rcdOpDriveValue, this, rc, vs);
  // leave 'vs' as it is => results in signal behaviour.
}



// ***** Directory services *****


void CRcDriver::PrintInfo (FILE *f) {
  fprintf (f, "Driver '%s'\n", Lid ());
}





// *************************** CRcEventDriver **********************************


void CRcEventDriver::DriveValue (CResource *rc, CRcValueState *vs) {
  CRcEvent ev(rceDriveValue, rc, vs);

  //~ INFOF (("### CRcEventDriver::DriveValue ('%s', '%s') -> quick success = %i", rc->Uri (), vs->ToStr (), successState));
  PutEvent (&ev);
  switch (successState) {
    case rcsValid:    break;   // no change; direct reporting
    case rcsBusy:     vs->SetToReportBusyOldVal (); break;
      // If the desired success state is 'rcsBusy', always report the old value with it
      // (important for shades, for example).
    case rcsUnknown:  vs->SetToReportNothing ();    break;
  }
}





// *************************** High-level API Helpers **************************


static inline void RcSetupRegistrationInfo (CString *attrs) {
  CSplitString lineSet, args;
  CString reqStr, uri, prefix;
  const char *key;
  int i, n;
  bool ok;

  //~ INFOF (("### RcSetupRegistrationInfo():\n%s", attrs->Get ()));
  lineSet.Set (attrs->Get (), INT_MAX, "\n");
  for (n = 0; n < lineSet.Entries (); n++) {
    // Syntax: <URI without "/host/<host>/"> [<attrs>]
    args.Set (lineSet [n]);
    if (args.Entries () < 1) continue;      // ignore empty lines
    RcGetRealPath (&uri, args[0], "/alias");
    //~ INFOF (("### RcSetupRegistrationInfo(): '%s' (alias '%s') ... ", uri.Get (), args[0]));
    ok = true;
    if (strncmp (uri.Get (), "/host/", 6) != 0) ok = false;
    else if (strncmp (uri.Get () + 6, localHostId.Get (), localHostId.Len ()) == 0) {
      key = uri.Get () + 7 + localHostId.Len ();    // 6 = strlen ("/host") + strlen ("/")
      reqStr.Clear ();
      for (i = 1; i < args.Entries (); i++) {
        switch (args[i][0]) {
          case '!':     // persistence marker ...
            rcConfPersistence.Set (key);
            //~ INFOF (("###   ... persistent: '%s'", key));
            break;
          case '+':     // request attribute ...
          case '-':
          case '~':
            reqStr.AppendF (" %s", args[i]);
            break;
          default:      // request value ...
            reqStr.SetC (args[i]);
        }
      }
      if (!reqStr.IsEmpty ()) {
        //~ INFOF (("### ... default request for '%s' = '%s'", key, reqStr.Get ()));
        rcConfDefaultRequests.Set (key, &reqStr);
      }
    }
    if (!ok) WARNINGF (("Ignoring illegal attributes set for '%s' (alias '%s')!", uri.Get (), args[0]));
  }
  //~ INFO ("### rcConfPersistence =");
  //~ rcConfPersistence.Dump ();
  //~ INFO ("### rcConfDefaultRequests =");
  //~ rcConfDefaultRequests.Dump ();
}


static inline void RcClearRegistrationInfo () {
  rcConfPersistence.Clear ();
  rcConfDefaultRequests.Clear ();
}


void RcRegisterConfigSignals (CString *signals) {
  CSplitString lineSet, args;
  ERcType rcType;
  int n;

  lineSet.Set (signals->Get (), INT_MAX, "\n");
  for (n = 0; n < lineSet.Entries (); n++) {
    // Syntax: <host> <name> <type>
    args.Set (lineSet [n]);
    if (args.Entries () < 1) continue;      // ignore empty lines
    ASSERT (args.Entries () == 3);
    if (localHostId.Compare (args[0]) == 0) {
      rcType = RcTypeGetFromName (args[2]);
      if (rcType != rctNone) RcDriversAddSignal (args[1], rcType);
      else WARNINGF(("Ignoring invalid signal definition (type error): 'S %s'", lineSet[n]));
    }
  }
}





// *************************** High-level API **********************************


// ***** General functions *****


static bool weOwnTheTimerThread = false;


void RcInit (bool enableServer, bool inBackground) {
  CString signals, attrs;

  // Sanity...
  if (!IsValidIdentifier (EnvInstanceName (), false))
    ERRORF (("Invalid instance name '%s'", EnvInstanceName ()));

  // Ignore 'SIGPIPE' signals...
  //   Such signals may occur on writes if the network connection is lost and by default, the program
  //   would exit then. To avoid this (failed writes are always checked for and handled accordingly),
  //   we ignore the signal.
  // TBD: Move this to a more global place, e.g. EnvInit()?
  signal (SIGPIPE, SIG_IGN);

  weOwnTheTimerThread = inBackground;

  // Initialization (pre-elaboration steps)...
  RcSetupNetworking (enableServer);
  RcReadConfig (&signals, &attrs);
  //~ INFOF (("### RcReadConfig() -> signals = '%s'", signals.Get ()));
  //~ INFOF (("### RcReadConfig() -> attrs = '%s'", attrs.Get ()));
  RcSetupRegistrationInfo (&attrs);

  // Elaboration phase...
  RcDriversInit ();
  RcRegisterConfigSignals (&signals);
}


void RcStart () {
  // End the initialization phase and start with the active phase, in which no more drivers are allowed to be declared.
  // This method is implicitely called by any of the 'RcRun', 'RcStart' or 'RcIterate' functions.
  CRcDriver *drv;
  int i, j, drivers, resources;

  // Startup active phase...
  if (!rcInitCompleted) {
    RcDriversStart ();            // This waits until all resources have been registered.
    RcClearRegistrationInfo ();
    RcNetStart ();
    if (weOwnTheTimerThread) TimerStart ();

    // Evaluate all local requests and drive values for the first time ...
    drivers = RcGetDrivers ();
    for (i = 0; i < drivers; i++) {
      drv = RcGetDriver (i);
      resources = drv->LockResources ();
      for (j = 0; j < resources; j++)
        drv->GetResource (j)->RedriveValue ();
      drv->UnlockResources ();
    }

    // Done ...
    rcInitCompleted = true;
  }
}


void RcDone () {
  /* A proper, safe shutdown without crashes can be conducted in two phases:
   * 1. All concurrent threads need to be stopped (or decoupled such as connection threads).
   * 2. All objects are cleaned up. Their destructors / 'Done' procedures should avoid accessing other
   *    modules/objects. Otherwise, the accesses must be considered in the ordering.
   */
  int n;

  // Phase 1: Stop all threads (except this main one)...
  if (rcInitCompleted) {

    // Stop timer thread if we are owner...
    if (weOwnTheTimerThread) {
      //~ INFO ("### TimerStop...");
      TimerStop ();
      //~ INFO ("### TimerStop: done.");
      weOwnTheTimerThread = false;
    }

    // Stop networking...
    RcNetStop ();

    // Stop all drivers...
    RcDriversStop ();

    // Phase 1 completed...
    rcInitCompleted = false;
  }

  // Stop and unregister all subscribers (just to improve efficiency)...
  SubscriberMapLock ();
  for (n = 0; n < subscriberMap.Entries (); n++) subscriberMap.Get (n)->Clear ();
  subscriberMap.Clear ();
  SubscriberMapUnlock ();

  // Phase 2: Clean up objects...
  RcDriversDone ();
  hostMap.Clear ();
#if WITH_CLEANMEM
  aliasMap.Clear ();
  for (n = 0; n < unregisteredResourceMap.Entries (); n++)
    delete unregisteredResourceMap.Get (n);
#endif
}


void RcIterate () {
  if (!rcInitCompleted) RcStart ();
  if (weOwnTheTimerThread) return;   // do nothing: the timer thread does everything.
  while (TimerIterate ());
}


int RcRun (bool catchSignals) {
  ASSERT (!weOwnTheTimerThread);     // other case not supported
  if (!rcInitCompleted) RcStart ();
  return TimerRun (catchSignals);
}


void RcStop () {
  ASSERT (!weOwnTheTimerThread);     // other case not supported
  TimerStop ();
}



// ***** Subscriptions *****


CRcSubscriber *RcNewSubscriber (const char *subscrLid, CResource *rc) {
  CRcSubscriber *subscr;

  //~ INFOF (("### RcSubscribe ('%s')", subscrLid));

  subscriberMapMutex.Lock ();
  subscr = subscriberMap.Get (subscrLid);
  subscriberMapMutex.Unlock ();
  if (subscr) ERRORF (("Redefinition of a subscriber with name '%s'", subscrLid));
  //~ subscr = new CRcSubscriber (subscrLid);
  subscr = new CRcSubscriber ();
  subscr->Register (subscrLid);
  //~ INFOF (("###   -> '%s'/%08x", subscr->Lid (), subscr));

  if (rc) subscr->AddResource (rc);

  return subscr;
}


CRcSubscriber *RcNewSubscriber (const char *subscrLid, const char *pattern) {
  CRcSubscriber *subscr = RcNewSubscriber (subscrLid);
  subscr->AddResources (pattern);
  return subscr;
}



// ***** Placing requests *****


void RcSetRequest (const char *rcUri, CRcRequest *req) {
  CResource *rc = RcGetResource (rcUri, false);
  ASSERT (rc != NULL);
  rc->SetRequestFromObj (req);
}


void RcSetRequest (const char *rcUri, CRcValueState *value, const char *reqGid, int priority, TTicks t0, TTicks t1, TTicks repeat, TTicks hysteresis) {
  CResource *rc = RcGetResource (rcUri, false);
  ASSERT (rc != NULL);
  rc->SetRequest (value, reqGid, priority, t0, t1, repeat, hysteresis);
}


void RcSetRequest (const char *rcUri, bool valBool, const char *reqGid, int priority, TTicks t0, TTicks t1, TTicks repeat, TTicks hysteresis) {
  CResource *rc = RcGetResource (rcUri, false);
  ASSERT (rc != NULL);
  rc->SetRequest (valBool, reqGid, priority, t0, t1, repeat, hysteresis);
}


void RcSetRequest (const char *rcUri, int valInt, const char *reqGid, int priority, TTicks t0, TTicks t1, TTicks repeat, TTicks hysteresis) {
  CResource *rc = RcGetResource (rcUri, false);
  ASSERT (rc != NULL);
  rc->SetRequest (valInt, reqGid, priority, t0, t1, repeat, hysteresis);
}


void RcSetRequest (const char *rcUri, float valFloat, const char *reqGid, int priority, TTicks t0, TTicks t1, TTicks repeat, TTicks hysteresis) {
  CResource *rc = RcGetResource (rcUri, false);
  ASSERT (rc != NULL);
  rc->SetRequest (valFloat, reqGid, priority, t0, t1, repeat, hysteresis);
}


void RcSetRequest (const char *rcUri, const char *valString, const char *reqGid, int priority, TTicks t0, TTicks t1, TTicks repeat, TTicks hysteresis) {
  CResource *rc = RcGetResource (rcUri, false);
  ASSERT (rc != NULL);
  rc->SetRequest (valString, reqGid, priority, t0, t1, repeat, hysteresis);
}


void RcSetRequestFromStr (const char *rcUri, const char *reqDef) {
  CResource *rc = RcGetResource (rcUri, false);
  ASSERT (rc != NULL);
  rc->SetRequestFromStr (reqDef);
}


void RcSetTrigger (const char *rcUri, const char *reqGid, int priority, TTicks t0, TTicks repeat) {
  CResource *rc = RcGetResource (rcUri, false);
  ASSERT (rc != NULL);
  rc->SetTrigger (reqGid, priority, t0, repeat);
}


void RcSetTriggerFromStr (const char *rcUri, const char *reqDef) {
  CResource *rc = RcGetResource (rcUri, false);
  ASSERT (rc != NULL);
  rc->SetTriggerFromStr (reqDef);
}


void RcDelRequest (const char *rcUri, const char *reqGid, TTicks t1) {
  CResource *rc = RcGetResource (rcUri, false);
  ASSERT (rc != NULL);
  rc->DelRequest (reqGid, t1);
}



// ***** Declaring drivers & their resources *****


CRcEventDriver *RcRegisterDriver (const char *drvLid, ERcState _successState) {
  CRcEventDriver *ret = new CRcEventDriver (drvLid, _successState);
  ret->Register ();
  return ret;
}


CResource *RcRegisterResource (const char *drvLid, const char *rcLid, ERcType _type, bool _writable, void *_data) {
  CRcDriver *drv = driverMap.Get (drvLid);
  //~ INFOF (("### RcRegisterResource ('%s' = %08x, '%s', ...)", drvLid, drv, rcLid));
  if (drv) return CResource::Register (drv, rcLid, _type, _writable, _data);
  else {
    WARNINGF (("Failed to register resource '%s' to non-existing driver '%s'", rcLid, drvLid));
    return NULL;
  }
}


CResource *RcRegisterResource (const char *drvLid, const char *rcLid, const char *rcTypeDef, void *_data) {
  CRcDriver *drv = driverMap.Get (drvLid);
  if (drv) return CResource::Register (drv, rcLid, rcTypeDef, _data);
  else {
    WARNINGF (("Failed to register resource '%s' to non-existing driver '%s'", rcLid, drvLid));
    return NULL;
  }
}


CResource *RcRegisterSignal (const char *name, ERcType type) {
  return RcDriversAddSignal (name, type);
}


CResource *RcRegisterSignal (const char *name, CRcValueState *vs) {
  return RcDriversAddSignal (name, vs);
}



// ***** Special functions *****


void RcBump (CResource *rc, bool soft) {
  CRcHost *host;
  int i;

  if (rc) {
    host = rc->Host ();
    if (host) host->RequestConnect (soft);
  }
  else {
    for (i = hostMap.Entries () - 1; i >= 0; i--)
      hostMap.Get (i)->RequestConnect (soft);
  }
}
