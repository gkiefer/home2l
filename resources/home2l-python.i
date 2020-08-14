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


/* Home2L Python API.
 *
 * This file contains the top-level Home2l Python library.
 * It is designed to be read by SWIG only.
 */


#ifndef _HOME2L_PYTHON_
#define _HOME2L_PYTHON_


#ifndef SWIG
#error This file is to be read by SWIG only.
#endif


#include "resources.H"





// ***** SWIG Header *****


%module(threads="1", docstring="Home2L library.") SWIG_MODULE
%feature("autodoc", "1");

/* Note on threading with Python
 * -----------------------------
 *
 * [2018-09-03]
 *
 * The option 'threads="1"' is equivalent with passing '-threads' to the swig command line.
 * Its effect is that for each C function code is generated that releases Python's
 * "global interpreter lock" (GIL) on entry and re-aquires is on exit. This is required
 * for functions/methods that may block (and if Python threads are used by the main program).
 *
 * The option increases the code size by about 10% and future optimizations may be possible
 * by disabling this feature for small methods that cannot block (e.g. simple getters/setters),
 * However, as of today, the official swig documentation is lacking information on how to do this.
 *
 * Sources:
 * - `swig -python -help|grep threads`
 * - http://swig.10945.n7.nabble.com/How-to-release-Python-GIL-td5027.html
 * - http://matt.eifelle.com/2007/11/23/enabling-thread-support-in-swig-and-python/
 */


// To be included by 'home2l_wrapper.C'...
%{
#include "resources.H"
%}

// Types that need to be handled specially...
typedef long long TTicks;     // should be 'int64_t'; "#include <stdint.h>" apparently does not work
typedef int TTicksMonotonic;  // should be 'int32_t'


// Other files to be exported to Python...
%include "../common/env.H"
%include "resources.H"





// ***** Doxygen header *****


%pythoncode %{
## @file
##
## @package home2l
## The *Home2L* package.
##
##
## @defgroup home2l Home2L
## Top-level of the *Home2L* Python package
## @{
##
##
## @defgroup home2l_general General
## General functions of the *Home2L* package
##
## @defgroup home2l_time Time
## Date and time helpers
##
## @defgroup home2l_rules Rules
## Running functions on resource events/updates or at certain times.
##
## @defgroup home2l_drivers Drivers
## Defining custom drivers for resources.
##
%}






// *************************** Rules *******************************************


%pythoncode %{

## @addtogroup home2l_rules
## @{


_onEventDict = {}
_onUpdateDict = {}
_dailyDict = {}




def _BuildSubscriber (id, func, rcSet):
  # Internal function to create a subscriber and enable it for 'CRcEventProcessor::Select()'.

  def _SubscribeToRcSet (subscr, rcSet):
    if (isinstance (rcSet, (list, tuple, set))):
      for x in rcSet: _SubscribeToRcSet (subscr, x)
    else:
      RcSubscribe (subscr, rcSet)

  subscr = RcNewSubscriber (id)
  _SubscribeToRcSet (subscr, rcSet)
  subscr.SetInSelectSet (True)
  return subscr;





# ****************** RunOnEvent() **************************


## Define a function to be called on events.
def RunOnEvent (func, rcSet = None, data = None, id = None):
  """Define a function to be called whenever an event for a set of\n\
  resources occurs.\n\
  \n\
  'rcSet' can either be a 'CResource' object, an URI string or a tuple\n\
  or a list of any of those. 'data' is an optional reference to user\n\
  data that will be passed unchanged to any 'func' invocations.\n\
  \n\
  For each event encountered, the function 'func' will be called as follows:\n\
  \n\
      func (ev, rc, vs, data)\n\
  \n\
  where '(ERcEventType) ev' is the type of event, 'rc' is the affected\n\
  resource, and, if applicable, 'vs' is the new value and state that\n\
  has been reported for the resource. Arguments are passed as keyword\n\
  arguments. Hence, the names must match, and unsused arguments can be\n\
  abbreviated by '**kwargs'.\n\
  \n\
  The event type 'ev' can be one of the following:\n\
      rceValueStateChanged : The value or state has changed.\n\
      rceConnected         : The connection has been (re-)established.\n\
      rceDisconnected      : The connection has been lost.\n\
  \n\
  Unlike 'RunOnEvent', the function 'func' will be called for each single\n\
  event in the correct order in time, so that no temporary value changes\n\
  get lost.\n\
  \n\
  If 'rcSet' is not defined, the function 'func' is called with 'ev = rc = None'\n\
  and must then return the set. There is no way to change the set once\n\
  defined.\n\
  \n\
  The parameter 'id' can usually be left unspecified. In this case, the function\n\
  names are used as identifiers. If multiple functions are passed, the ID must\n\
  be left unspecified.\n\
  """
  if not rcSet: rcSet = func (ev = None, rc = None, vs = None, data = data)
  if not id: id = func.__name__
  subscr = _BuildSubscriber (id, func, rcSet)
  _onEventDict[id] = (func, data, subscr)


## Decorator to define a function to be called on events.
def onEvent (*rcSet):
  """Decorator variant of 'RunOnEvent'.\n\
  \n\
  This decorator allows to easily let a function be executed on given events\n\
  as follows:\n\
  \n\
      @onEvent (<resource set>)\n\
      def MyFunc (ev, rc, vs):\n\
        ...\n\
  \n\
  """
  def _Decorate (func):
    def _WrapperFunc (ev, rc, vs, data): data (ev, rc, vs)
    RunOnEvent (_WrapperFunc, rcSet, data=func, id=func.__name__)
    return _WrapperFunc
  return _Decorate





# ****************** RunOnUpdate() *************************


## Define a function to be called on value/state changes.
def RunOnUpdate (func, rcSet, data = None, id = None):
  """Define a function to be called on value/state changes of resources.\n\
  \n\
  'rcSet' can either be a 'CResource' object, an URI string or a tuple\n\
  or a list of any of those. 'data' is an optional reference to user\n\
  data that will be passed unchanged to any 'func' invocations.\n\
  \n\
  For each event encountered, the function 'func' will be called as follows:\n\
  \n\
      func (data)\n\
  \n\
  Arguments are passed as keyword arguments. Hence, the names must match,\n\
  and unsused arguments can be abbreviated by '**kwargs'.\n\
  \n\
  Unlike 'RunOnEvent', only 'rceValueStateChanged' events cause\n\
  invocations of 'func', and multiple events quickly following each\n\
  other may be merged to a single invocation, so that only the last value\n\
  is actually reported. For this reason, this mechanism is generally more\n\
  efficient and should be preferred if short intermediate value changes\n\
  are not of interest.\n\
  \n\
  Important: At the invocation of 'func', all resources contained in\n\
  'rcSet' may have changed since the last invocation.\n\
  (BTW: This is the reason why the function does not have any convenience
  arguments such as 'rc' and 'vs'.)
  \n\
  If 'rcSet' is not defined, the function 'func' is called with 'rc = None'\n\
  and must then return the set. There is no way to change the set once\n\
  defined.\n\
  \n\
  The parameter 'id' can usually be left unspecified. In this case, the function\n\
  names are used as identifiers. If multiple functions are passed, the ID must\n\
  be left unspecified.\n\
  """
  if not rcSet: rcSet = func (rc = None, vs = None, data = data)
  if not id: id = func.__name__
  subscr = _BuildSubscriber (id, func, rcSet)
  _onUpdateDict[id] = (func, data, subscr)


## Decorator to define a function to be called on value/state changes.
def onUpdate (*rcSet):
  """Decorator variant of 'RunOnUpdate'.\n\
  \n\
  This decorator allows to easily define a function executed on value changes\n\
  as follows:\n\
  \n\
      @onUpdate ( <resource set> )\n\
      def MyFunc ():\n\
        ...\n\
  \n\
  """
  def _Decorate (func):
    def _WrapperFunc (data): data ()
    RunOnUpdate (_WrapperFunc, rcSet, data=func, id=func.__name__)
    return _WrapperFunc
  return _Decorate





# ****************** RunAt() *******************************


_timerDict = {}


## Let a function be called at a given time or periodically.
def RunAt (func, t = 0, dt = 0, data = None, id = None):
  """Define a function to be called at a given time, optionally repeated\n\
  at a certain interval.\n\
  \n\
  The starting time 't' can be specified by anything accepted by TicksAbsOf().
  The interval 'interval' is specified in milliseconds.\n\
  If dt <= 0, the timer is executed once and then deactivated. Otherwise,
  the function is repeated regularly every 'dt' milliseconds.\n\
  \n\
  If 'id' is not set, the function name will be used as an ID.\n\
  Calling this function again with the same ID causes the old timer to be deleted.\n\
  Presently, there is no mechanism to delete a previously defined timer.\n\
  \n\
  The function 'func' will be called as follows:\n\
  \n\
      func (data)\n\
  \n\
  where 'data' is the user-defined data passed to 'RunAt'.\n\
  """
  if not id: id = func.__name__
  ep = CRcEventTimer (id)
  ep.SetInSelectSet (True)
  _timerDict[id] = (func, data, ep)
  ep.Set (TicksAbsOf (t), int (dt))


## Decorator to let a function be called at a given time or periodically.
def at (t = 0, dt = 0):
  """Decorator variant of 'RunAt'.\n\
  \n\
  This decorator allows to easily define a function executed at given times\n\
  as follows:\n\
  \n\
      @at (t = <t> [ , dt = <interval> ] )\n\
      def MyTimedFunc ():\n\
        ...\n\
  \n\
  """
  def _Decorate (func):
    def _WrapperFunc (data): data ()
    RunAt (_WrapperFunc, t, dt, data=func, id=func.__name__)
    return _WrapperFunc
  return _Decorate





# ****************** RunDaily() ****************************


## Let a function be called daily for setting permanent requests.
def RunDaily (func, hostSet = None, data = None, id = None):
  """Define a function to be called daily or whenever one of the hosts becomes\n\
  reachable (again).\n\
  \n\
  This can be used to set and keep persistent permanent requests in rules scripts.\n\
  'hostSet' can either be a single string or a tuple or a list of strings.\n\
  'data' is an optional reference to user data that will be passed unchanged to\n\
  any 'func' invocations.\n\
  \n\
  Whenever a host or a set of hosts becomes reachable, the function 'func' will\n\
  be called as follows:\n\
  \n\
      func (host, data)\n\
  \n\
  'host' is the host name for which rules must be updated.\n\
  Arguments are passed as keyword arguments. Hence, the names must match,\n\
  and unsused arguments can be abbreviated by '**kwargs'.\n\
  \n\
  Important: On each host in the set, the 'timer' driver must be enabled.\n\
  \n\
  The parameter 'id' can usually be left unspecified. In this case, the function\n\
  names are used as identifiers. If multiple functions are passed, the ID must\n\
  be left unspecified.\n\
  """
  def _AddToSet (s, host):
    if isinstance (host, (list, tuple, set)):
      for x in host: _AddToSet (s, x)
    else:
      s.add ("/host/" + host + "/timer/daily")

  if not id: id = func.__name__
  if hostSet:
    rcSet = set ()
    _AddToSet (rcSet, hostSet)
  else:
    rcSet = "/local/timer/daily"
  subscr = _BuildSubscriber (id, func, rcSet)
  _dailyDict[id] = (func, data, subscr)


## Decorator to let a function be called daily for setting permanent requests.
def daily (*hostSet):
  """Decorator variant of 'RunDaily'.\n\
  \n\
  This decorator allows to easily define a function executed daily or whenever
  a host becomes reachable (again) as follows:\n\
  \n\
      @daily ( [ <host set> ] )\n\
      def MyFunc (host):\n\
        ...\n\
  \n\
  """
  def _Decorate (func):
    def _WrapperFunc (host, data):
      if data.__code__.co_argcount == 0: data()     # for the case that no hosts are specified
      else: data (host)
    RunDaily (_WrapperFunc, hostSet, data=func, id=func.__name__)
    return _WrapperFunc
  return _Decorate



## @}

%} // %pythoncode





// **************************** Drivers *****************************************


%pythoncode %{

## @addtogroup home2l_drivers
## @{


_driverDict = {}
_bufferedResources = {}     # stored lists of resources


## Define a new driver.
def NewDriver (drvName, func, successState = rcsBusy, data = None):
  """Define a new event-based driver.\n\
  \n\
  'drvName' is the unique local id (LID) of the new driver.\n\
  '(ERcState) successState' determines the automatic reply sent out to the\n\
  system before 'func' is invoked. Possible values are:\n\
  \n\
    - 'rcsBusy':    (default) 'rcsBusy' with the *old* value is reported;\n\
                    the application must report a valid and new value later.\n\
  \n\
    - 'rcsValid':   the driven value is reported back; no further action by\n\
                    the driver necessary, but no errors are allowed to happen.\n\
  \n\
    - 'rcsUnknown' or 'rcsNoReport':\n\
                    nothing is reported back now; the application must report\n\
                    something soon and should report a valid and new value later.\n\
  \n\
  'data' is an optional reference to user data that will be passed\n\
  unchanged to any 'func' invocations.\n\
  \n\
  'func' is a function to drive new values and will be called as follows:\n\
  \n\
      func (rc, vs, data)\n\
  \n\
  where 'rc' is the resource to be driven and '(CRcValue) value' is the\n\
  new value to be driven.\n\
  \n\
  For reporting values from the resource, the 'CResource::Report...'\n\
  methods have to be used.\n\
  """
  RcRegisterDriver (drvName, successState).SetInSelectSet (True)
  _driverDict[drvName] = (func, data)
  # Apply buffered (previously defined) resources ...
  if drvName in _bufferedResources:
    for rcName, _type, _writable in _bufferedResources[drvName]:
      NewResource (drvName, rcName, _type, _writable)
    del _bufferedResources[drvName]


## Decorator to define a new driver.
def newDriver (drvName, successState = rcsBusy):
  """Decorator variant of 'NewDriver'.\n\
  \n\
  This decorator allows to easily define a driver as follows:\n\
  \n\
      @newDriver (<driver name>, [ <success state> ] )\n\
      def MyDriverFunc (rc, vs):\n\
        ...\n\
  \n\
  """
  def _Decorate (func):
    def _WrapperFunc (rc, vs, data): data (rc, vs)
    NewDriver (drvName, _WrapperFunc, successState, data=func)
    return _WrapperFunc
  return _Decorate


## Define a new resource.
def NewResource (drvName, rcName, _type, _writable = True):
  """Define a new resource managed by a driver defined by 'NewDriver'."""
  if drvName in _driverDict:
    return RcRegisterResource (drvName, rcName, _type, _writable)
  else:
    if not drvName in _bufferedResources: _bufferedResources[drvName] = []
    _bufferedResources[drvName].append ( (rcName, _type, _writable) )
    return RcGetResource ("/local/" + drvName + "/" + rcName)


## Define a new signal.
def NewSignal (sigName, _type, defaultVal = None):
  """Define a signal resource under '/local/signal/<sigName>."""
  if defaultVal == None:
    return RcRegisterSignal (sigName, _type)
  else:
    return RcRegisterSignal (sigName, CRcValueState (_type, defaultVal))



## @}   # home2l_drivers
%}





// *************************** Time helpers ************************************


%pythoncode %{
## @addtogroup home2l_time
## @{
%}



// ***** C-implemented helpers (copied from 'base.H') *****


%feature("docstring") TicksNow "Get the current time in units of 'TTicks' (milliseconds)."
TTicks TicksNow ();

%feature("docstring") TicksToday "Get the time 0:00 of the current day in units of 'TTicks' (milliseconds)."
TTicks TicksToday ();

%feature("docstring") TicksOfDate "Convert a date into 'TTicks' units."
TTicks TicksOfDate (int dy, int dm, int dd);

%feature("docstring") TicksOfTime "Convert a time into 'TTicks' units (relative).\n\n"
  "The values returned by 'TicksOfDate' and 'TicksOfTime' as well as a\n"
  "number of milliseconds can be simply added to obtain a 'TTicks' value\n"
  "for a certain time at a certain date."
TTicks TicksOfTime (int th, int tm, int ts);

%feature("docstring") TicksOfDate "Convert a date into 'TTicks' units."
TTicks TicksOfDate (int dy, int dm, int dd);

%inline %{
static inline TTicks TicksOfSeconds (int secs) { return TICKS_FROM_SECONDS (secs); }
static inline TTicks TicksOfSeconds (float secs) { return (float) TICKS_FROM_SECONDS (secs); }
static inline TTicks TicksOfMillis (int ms) { return ms; }
%}



// ***** Internal functions *****


%newobject _TicksStr ();
%inline %{
static inline const char *_TicksStr (TTicks ticks) {
  CString s;
  TicksToString (&s, ticks);
  return s.Disown ();
}
static inline TTicks _TicksFromString (const char *str, bool absolute) {
  TTicks t;
  if (!TicksFromString (str, &t, absolute)) return -1;
  return t;
}
%}



// ***** Python functions *****


%pythoncode %{


import datetime;


def TicksStr (ticks):
  """Return a TTicks value as a string in Home2L format."""
  if not isinstance (ticks, int): return None
  return _TicksStr (ticks)


def TicksOf (something):
  """Magic wand to convert anything to an absolute or relative TTicks value as intuitively\n\
  as possible. Whether the returned value is absolute or relative depends on the argument\n\
  passed.\n\
  \n\
  - Values of type string are interpreted as time specifications for TicksFromString()\n\
    or as accepted by the Home2L Shell. The returned value is always an absolute time.\n\
  \n\
  - Values of numeral types are returned as they are.\n\
  \n\
  - Values of type 'datetime.date' or 'datetime.datetime' are interpreted as absolute\n\
    times and converted.\n\
  \n\
  - For values of type 'datetime.time', the relative time from the beginning of a day\n\
    is returned.\n\
  \n\
  - For values of type 'datetime.timedelta', the relative delta time in milliseconds\n\
    is returned.\n\
  """
  if isinstance (something, str):
    t = _TicksFromString (something, False)
    if t >= 0: return t
    else: return None

  if isinstance (something, int): return something
  if isinstance (something, float): return round (something)

  if isinstance (something, datetime.datetime):   # Note: 'datetime' is a subclass or 'date' => this must be first!
    return TicksOfDate (something.year, something.month, something.day) \
         + TicksOfTime (something.hour, something.minute, something.second) + round (something.microsecond / 1000)
  if isinstance (something, datetime.date):       # Note: 'datetime' is a subclass or 'date' => this must be behind!
    return TicksOfDate (something.year, something.month, something.day)
  if isinstance (something, datetime.time):
    return TicksOfTime (something.hour, something.minute, something.second) + round (something.microsecond / 1000)
  if isinstance (something, datetime.timedelta):
    return round (something.total_seconds() * 1000)

  return None     # We had an unkown type.


def TicksAbsOf (something):
  """Magic wand to convert anything to an absolute TTicks value as intuitively as possible.\n\
  The function is similar to TicksOf(), but it guarantees to return an absolute time.\n\
  \n\
  - Values of type string are interpreted as time specifications for TicksFromString()\n\
    or as accepted by the Home2L Shell. The returned value is always an absolute time.\n\
  \n\
  - Numeral types (int, float) are interpreted relative to now if their value is below 10^11\n\
    (approx. 3 years). Otherwise, they are returned as they are.\n\
  \n\
  - Values of type 'datetime.date' or 'datetime.datetime' are interpreted as absolute\n\
    times and returned.\n\
  \n\
  - Values of type 'datetime.time', are interpreted relative to today.\n\
    Note: Be careful when calling tthis variant close to midnight. Functions triggered by
    the '/local/timer/daily' resource or the '@daily()' decorator are guaranteed to be called\n\
    (shortly) after midnight and can thus safely use this variant.\n\
  \n\
  - For values of type 'datetime.timedelta', the time relative to now is returned.\n\
  """
  if isinstance (something, str):
    t = _TicksFromString (something, True)
    if t >= 0: return t
    else: return None

  if isinstance (something, int):
    if something > 999999999999: return something
    else: return TicksNow () + something
  if isinstance (something, float): return TicksAbsOf (round (something))

  if isinstance (something, datetime.time):
    return TicksToday () + round (((something.hour * 60 + something.minute) * 60 + something.second) * 1000 + something.microsecond / 1000)
  if isinstance (something, datetime.timedelta):
    return TicksNow () + round (something.total_seconds() * 1000)

  return TicksOf (something)


def TicksToPyDateTime (t):
  """Return a Python 'datetime.datetime' object for the ticks value."""
  return datetime.datetime.fromtimestamp (t/1000)


def TicksToPyTimeDelta (t):
  """Return a Python 'datetime.timedelta' object for the ticks value."""
  return datetime.timedelta (milliseconds = t)


def TicksFromUTC (t):
  """Correct a ticks value created from a UTC time string."""
  return t + (datetime.datetime.fromtimestamp (t/1000) - datetime.datetime.utcfromtimestamp (t/1000)).seconds * 1000;


def TicksToUTC (t):
  """Convert a ticks value to a value that will return UTC time when converted to a string."""
  return t + (datetime.datetime.utcfromtimestamp (t/1000) - datetime.datetime.fromtimestamp (t/1000)).seconds * 1000;


%}


%pythoncode %{
## @}
%}    // time





// *************************** Top-level Python API ****************************

// The following definitions represent the top-level of the Python library named 'home2l'.
// Presently, it is identical to this module ('resources'). In the future, the following code
// may be moved away from here.


%pythoncode %{
## @addtogroup home2l_general
## @{
%}


// Register Home2lDone() as an 'atexit' function in Home2lInit(), so that this does
// not have to be called explicitly in rules scripts.
%pythoncode %{
  import atexit
%}
%feature("pythonappend") Home2lInit %{
  #~ print ("### Registering atexit function.")
  atexit.register (Home2lDone)
%}
%feature("pythonprepend") Home2lDone %{
  #~ print ("### Home2lDone()...")
  atexit.unregister (Home2lDone)
%}


%feature("docstring") Home2lInit "Initialize the Home2L package.\n\n"
  "The following optional arguments may be set:\n"
  "  instance : The instance name [default: name of the script/executable]\n"
  "             A leading 'home2l-' is stripped off.\n"
  "  server   : Enable the server\n"
  "  args     : Parse the command line arguments for standard Home2L arguments\n"
%inline %{
static inline void Home2lInit (const char *instance = NULL, bool server = false, bool args = true) {
  PyGILState_STATE gilState;
  PyObject *pyArgList, *pyArg;
  const char **argv = NULL;
  int argc = 0, n;
  bool ok;

  // Use the C-Python API to get 'sys.argv' ...

  // Acquire the "global interpreter lock" (GIL)...
  //   This is only necessary since the SWIG '-threads' option is globally on, but would not
  //   cause problems if some day that is deactivated for this function.
  gilState = PyGILState_Ensure();

  // Get the 'sys.argv' list object...
  pyArgList = PySys_GetObject("argv");     // borrowed reference
  ok = (pyArgList != NULL);
  //~ PyObject_Print (pyArgList, stdout, 0); putchar ('\n');  // Test-print it

  // Interpret the list...
  if (ok) ok = PySequence_Check (pyArgList);
  if (ok) {
    argc = PySequence_Size (pyArgList);
    ASSERT (argc >= 1);
    argv = MALLOC (const char *, argc);
    for (n = 0; n < argc && ok; n++) {
      pyArg = PySequence_GetItem (pyArgList, n);
      ok = PyUnicode_Check (pyArg);
      if (ok) if ( (argv[n] = PyUnicode_AsUTF8 (pyArg)) == NULL) ok = false;
    }
  }

  // Terminate on error. ERROR() exists the tool, so that no cleanup is made...
  if (!ok) ERROR (("Unable to obtain the command line arguments via Python's 'sys.argv'"));

  // Set a default instance name if neither 'argv[0]' nor 'instance' is defined...
  //   This happens in an interactive python shell.
  if (!instance && !argv[0][0]) instance = "python";

  // Do the Home2L initialization...
  //~ INFOF(("### instance = '%s'", instance));
  EnvInit (args ? argc : 1, (char **) argv, NULL, instance);
  RcInit (server, true);

  // Release the Python objects and the GIL...
  free (argv);
  for (n = 0; n < argc && ok; n++) {
    pyArg = PySequence_GetItem (pyArgList, n);
    // This was the second new reference we got. Now unref them both...
    Py_DECREF (pyArg);
    Py_DECREF (pyArg);
  }
  PyGILState_Release (gilState);
}
%}


%feature("docstring") Home2lStart "Start the background activities of the Home2L package.\n\n"
  "This primarily ends the elaboration phase of the Resources library and starts\n"
  "the server (if enabled). New drivers and resources must be defined before this.\n"
%inline %{
static inline void Home2lStart () {
  RcStart ();
}
%}

%feature("docstring") Home2lDone "Shutdown the Home2L package.\n\n"
  "It is not necessary to call this as the last command in a script, since it\n"
  "is executed automatically on exit.\n"
%inline %{
static inline void Home2lDone () {
  RcDone ();
  EnvDone ();
}
%}


%pythoncode %{


_home21Stopped = False
_dummyForDoxygen = 0


import signal


def Home2lIterate (maxTime = 0):
  """Iterate Home2L for a time of 'maxTime' ms and run all pending callback functions.\n\
  \n\
  If 'maxTime == 0', only the currently pending work is done, and the\n\
  function does not wait. If 'maxTime < 0', the function may wait\n\
  indefinitely (this feature is used by 'Run').\n\
  """
  #~ print ("### Home2lIterate")
  Home2lStart ()      # implicitly end the elaboration phase if not done yet

  # Switch off the Python-internal Ctrl-C handler while waiting ...
  #   This is necessary to keep Ctrl-C effective. Otherwise, keyboard interrupts would be deferred when
  #   waiting in a C function (e.g. during a long-waiting 'Select')
  if maxTime != 0:
    s = signal.getsignal (signal.SIGINT)
    signal.signal (signal.SIGINT, signal.SIG_DFL)
  ep = CRcEventProcessor.Select (maxTime);
  if maxTime != 0:
    signal.signal (signal.SIGINT, s)

  #~ print ("### Select done")
  if ep:
    epType = ep.TypeId ()
    epLid = ep.InstId ()
    #print ("### Event processor: ", epType, epLid);
    ev = ep.PollEvent ()
    if epType == 'S':     # Subscriber...
      # Check 'RunOnEvent'...
      if epLid in _onEventDict:
        func, data, subscr = _onEventDict[epLid]
        while ev:
          func (ev.Type (), ev.Resource (), ev.ValueState (), data)
          ev = ep.PollEvent ()
      # Check 'RunOnUpdate'...
      elif epLid in _onUpdateDict:
        func, data, subscr = _onUpdateDict[epLid]
        lastEv = None
        while ev:
          if ev.Type () == rceValueStateChanged: lastEv = ev
          ev = ep.PollEvent ()
        if lastEv:
          if func.__code__.co_argcount == 1: func (data)
          else: func ()     # tolerate function without argument
      # Check 'RunDaily'...
      elif epLid in _dailyDict:
        func, data, subscr = _dailyDict[epLid]
        hostSet = set()
        while ev:
          if ev.Type () == rceValueStateChanged or ev.Type () == rceConnected:
            host = ev.Resource ().Uri ().split ('/') [2]
            hostSet.add (host)
          ev = ep.PollEvent ()
        for host in hostSet:
          if func.__code__.co_argcount == 2:   func (host, data)
          elif func.__code__.co_argcount == 1: func (host)
          else:                                func ()
      else:
        print ("WARNING: Received event on unknown subscriber '" + epLid + "'")
    elif epType == 'D':   # Driver...
      # Check 'NewDriver'...
      while ev:
        func, data = _driverDict[epLid]
        if func.__code__.co_argcount == 3:
          func (ev.Resource (), ev.ValueState (), data)
        else:     # tolerate functions without the 'data' argument ...
          func (ev.Resource (), ev.ValueState ())
        ev = ep.PollEvent ()
    elif epType == 'T':   # Timer...
      # Check 'RunAt'...
      while ev:
        func, data, ep = _timerDict[epLid]
        if func.__code__.co_argcount == 1: func (data)
        else: func ()     # tolerate functions without an argument
        ev = ep.PollEvent ()


def Home2lRun ():
  """Run the Home2L main loop indefinitely (or until stopped).\n\
  Implies 'Start'.\n\
  """
  global _home21Stopped
  Home2lStart ()
  _home21Stopped = False
  while not _home21Stopped:
    Home2lIterate (-1)


def Home2lStop ():
  """Let 'Run' stop at next occasion.\n\
  \n\
  Note: This (and all top-level) functions may presently only be called\n\
  from a single (main) thread. In particular, stopping from a different\n\
  thread is not supported.\n\
  """
  #~ print ("### Home2lStop")
  global _home21Stopped
  _home21Stopped = True;
  # Note: There is presently no interrupt mechanism to stop 'CRcEventProcessor::Select'. This is no problem as long as
  #       'Stop' is only called from the main thread, namely from any callback function defined by 'RunOn...' or
  #       'NewDriver'. These functions are always called between 'Select' and the next check of 'doStop'.


## @} # home2l_general

## @} # home2l (Main)


%}   // %pythoncode


#endif  // _HOME2L_PYTHON_

