/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2015-2024 Gundolf Kiefer
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
typedef long long TTicks;  // should be 'int32_t'
# ~ typedef int TTicks;  // should be 'int32_t'


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
## @defgroup home2l_resources Resources
## Defining local resources and custom drivers
##
## @defgroup home2l_rules Rules
## Running functions on resource updates, on events or at certain times
##
%}





// **************************** Local Resources ********************************


%pythoncode %{
## @addtogroup home2l_resources
## @{





# ****************** Resources *****************************


_RESOURCES_DRV = "resource"   # name of the (standard) resources driver

_resourcesHaveDriver = False  # mark if we already have a driver for resources
_resourcesDict = {}           # dictionary to map decorated functions to resources


def _resourcesDriverFunc (rc, vs):
  func, data, _rc = _resourcesDict[rc.Lid()]
  if func.__code__.co_argcount == 2:  func (rc, vs.Value ())     # tolerate functions without the 'data' argument
  else:                               func (rc, vs.Value (), data)


## Define a new local resource.
def NewResource (rcName, rcType, func = None, data = None):
  """Define a new local resource at '/local/resource/<rcName>'.\n\
  \n\
  'rcName' is the name of the resource, 'rcType' is its type. The resource will\n\
  be assigned to the built-in 'resource' driver.\n\
  \n\
  'func' is a function to drive new values and will be called as follows:\n\
  \n\
      func (rc, val [, data] )\n\
  \n\
  where 'rc' is the resource to be driven and 'val' is the new value to be driven.\n\
  'val' can be 'None', which indicates that presently no valid request exists.\n\
  For details, see the C API documentation on 'CRcDriver::DriveValue()'.\n\
  \n\
  If 'func == None', the resource is generated as a read-only resource.
  \n\
  'data' is an optional reference to user data that will be passed unchanged\n\
  to any 'func' invocations.\n\
  \n\
  For reporting values from the resource, the 'CResource::Report...' methods\n\
  have to be used.\n\
  \n\
  The drive function 'func' is responsible for immediately reporting any\n\
  value/state changes resulting from a drive operation. If the driven value\n\
  immediately reflects the actual state of the resource, this can be done by\n\
  calling 'CResource::ReportValue()'. If the new real value is not known\n\
  immediately, 'CResource::ReportBusy()' should be called now and the final\n\
  up-to-date value be reported later.\n\
  """
  global _resourcesHaveDriver
  if not _resourcesHaveDriver:
    RcRegisterDriver (_RESOURCES_DRV, rcsNoReport).SetInSelectSet (True)
    _driverDict[_RESOURCES_DRV] = (_resourcesDriverFunc, None)
    _resourcesHaveDriver = True
  rc = RcRegisterResource (_RESOURCES_DRV, rcName, rcType, False if func == None else True)
  _resourcesDict[rcName] = (func, data, rc)
  return rc


## Decorator to define a new local resource.
def resource (rcName, rcType, data = None):
  """Decorator variant of 'NewResource()'.\n\
  \n\
  This decorator allows to easily define a local writable resource as follows:\n\
  \n\
  |   @resource ( <rcName>, <type> [, <data>] )\n\
  |   def MyDriveFunc (rc, val [, data]):\n\
  |     ...\n\
  \n\
  """
  def _Decorate (func):
    NewResource (rcName, rcType, func, data)
    return func
  return _Decorate





# ****************** Gates *********************************


_GATES_DRV = "gate"       # name of the gates driver

_gatesHaveDriver = False  # mark if we already have a driver for gates
_gatesDict = {}           # dictionary to map decorated functions to resources


## Define a new gate.
def NewGate (rcName, rcType, rcSet, func, subscrId = None):
  """Define a new gate at '/local/gate/<rcName>'.\n\
  \n\
  A gate is a read-only resource showing a value that is calculated by the\n\
  gate function based on values of other resources.\n\
  \n\
  'rcName' is the name of the gate, 'rcType' is its type. The gate will be\n\
  assigned to the built-in 'gate' driver.
  \n\
  'rcSet' specifies the resource(s) the gate function depends on.\n\
  It may be a single resource or a tuple or list of multiple resources.\n\
  \n\
  'func' is the gate function. It is called as:\n\
  \n\
      value = func (a, b, c, ...)\n\
  \n\
  'a, b, c, ...' are positional arguments with arbitrary names by which the\n\
  values of the 'rcSet' resources are passed. The function must return the\n\
  actual value to be reported. If 'None' is returned, a state of 'rcsUnkown'\n\
  is reported. A state of 'rcsBusy' is never reported.\n\
  \n\
  'NewGate()' returns a reference to the newly created resource.\n\
  \n\
  Notes on writing gate functions ('func'):
  \n\
  1. Any of the arguments 'a, b, c, ...' may also be 'None' if the respective\n\
     resource has a state of 'rcsUnkown'. This must be considered when writing\n\
     gate functions. For example, to check whether a Boolean resource x is known\n\
     and false, the expression should be 'if x == False: ...' instead of\n\
     'if not x: ...', since in the latter case 'not x' would evaluate as true,\n\
     if x is 'None'. In general, Boolean resource values should always be used\n\
     in conjunction with a comparision. For example, an expression like\n\
     'a and b' may be written as '(a == True) and (b != False)', which implies,\n\
     that a value of 'None' is treated like 'False' for 'a', but like 'True'\n\
     for 'b'.\n\
  \n\
  2. To facilitate gate function writing, especially if the source resources have\n\
     numerical types, 'TypeError' exceptions are caught by the caller and result\n\
     in a return value of 'None'. 'TypeError' exceptions are typically raised\n\
     if one of the arguments of an arithmetic exception is 'None'.
  \n\
  3. The gate function is evaluated whenever a value/state change occurs for\n\
     any of the resources of 'rcSet', but only then. If resources are read\n\
     inside the gate function, they usually must be contained in 'rcSet'.\n\
     It is good practice to not read out resources directly, but only rely\n\
     on the arguments passed to the gate function.\n\
  """

  # Create driver and resource ...
  global _gatesHaveDriver
  if not _gatesHaveDriver:
    RcRegisterDriver (_GATES_DRV, rcsValid)
    # We skip adding it to '_driverDict', since all the resources are read-only
    _gatesHaveDriver = True
  rc = RcRegisterResource (_GATES_DRV, rcName, rcType, False)

  # Callback suitable for RunOnUpdate(), use closure to include 'rc' ...
  def _OnUpdateFunc (*args):
    try:              value = func (*args)
    except TypeError: value = None
    rc.ReportValue (value)
    # ~ print ("### NewGate._OnUpdateFunc (" + str (args) + "): value = " + str (value))

  # Register callback and finish ...
  if not subscrId: subscrId = _SubscrIdFromFunc (func)
  RunOnUpdate (_OnUpdateFunc, rcSet, subscrId = subscrId)
  return rc


## Decorator to define a new gate.
def gate (rcName, rcType, *rcSet):
  """Decorator variant of 'NewGate()'.\n\
  \n\
  This decorator allows to easily define a gate as follows:\n\
  \n\
  |   @gate ( <gate name>, <type>, <source resources> )\n\
  |   def MyGateFunc (a, b, c, ...):\n\
  |     ...\n\
  |     return ...\n\
  \n\
  """
  def _Decorate (func):
    _gatesDict[func.__name__] = NewGate (rcName, rcType, rcSet, func)
    return func
  return _Decorate


## Get resource reference of a decorated local resource or gate.
def GetLocalRc (func):
  """Get a resource reference of a local resource or gate by its decorated function.\n\
  \n\
  This function allows to query a resource object defined by the '@gate' or '@resource'\n\
  decorator by a reference to the decorated function.\n\
  """
  # ~ print ("### GetRateRc (" + func.__name__ + "): _gatesDict = " + str (_gatesDict))
  if func.__name__ in _gatesDict: return _gatesDict[func.__name__]
  for key, val in _resourcesDict.items():
    if val[0] == func: return val[2]
  raise KeyError ("No resource found for function '" + func.__name__ + "'.")





# ****************** Signals *******************************


## Define a new signal.
def NewSignal (sigName, rcType, defaultVal = None):
  """Define a signal resource under '/local/signal/<sigName>."""
  if defaultVal == None:
    return RcRegisterSignal (sigName, rcType)
  else:
    return RcRegisterSignal (sigName, CRcValueState (rcType, defaultVal))







# ****************** Custom Drivers ************************


_driverDict = {}
_bufferedResources = {}   # stored lists of resources


## Define a new driver.
def NewDriver (drvName, func = None, successState = rcsNoReport, data = None):
  """Define a new event-based driver.\n\
  \n\
  'drvName' is the unique local id (LID) of the new driver.\n\
  \n\
  'func' is a function to drive new values and will be called as follows:\n\
  \n\
      func (rc, vs [, data] )\n\
  \n\
  where 'rc' is the resource to be driven and 'vs' (type 'CRcValueState')\n\
  is the new value to be driven. 'vs' can (only) have the states 'rcsValid'\n\
  or 'rcsUnknown', where the latter indicates that presently no valid request\n\
  exists. For details, see the C API documentation on 'CRcDriver::DriveValue()'.\n\
  \n\
  'data' is an optional reference to user data that can be passed\n\
  unchanged to any 'func' invocations.\n\
  \n\
  '(ERcState) successState' determines the automatic reply sent out to the\n\
  system before 'func' is invoked. Possible values are:\n\
  \n\
    - 'rcsNoReport' or 'rcsUnknown' (default):\n\
                    nothing is reported back now; the application must report\n\
                    something soon and should report a valid and up-to-date value\n\
                    later.\n\
  \n\
    - 'rcsBusy':    'rcsBusy' with the *old* value is reported;\n\
                    the application must report a valid and up-to-date value later.\n\
  \n\
    - 'rcsValid':   the driven value is reported back; no further action by\n\
                    the driver necessary, but no errors are allowed to happen.\n\
  \n\
  For reporting values from the resource, the 'CResource::Report...'\n\
  methods have to be used.\n\
  """
  if drvName in ("signal", _GATES_DRV, _RESOURCES_DRV):
    print ("WARNING: Failed to register driver with reserved name '" + drvName + "'.")
    return
  RcRegisterDriver (drvName, successState).SetInSelectSet (True)
  _driverDict[drvName] = (func, data)
  # Apply buffered (previously defined) resources ...
  if drvName in _bufferedResources:
    for rcName, rcType, writable in _bufferedResources[drvName]:
      NewResource (drvName, rcName, rcType, writable)
    del _bufferedResources[drvName]


## Decorator to define a new driver.
def driver (drvName, successState = rcsBusy, data = None):
  """Decorator variant of 'NewDriver()'.\n\
  \n\
  This decorator allows to easily define a driver as follows:\n\
  \n\
  |   @driver ( <driver name> [, <success state>] [, <data>] )\n\
  |   def MyDriverFunc (rc, vs [, data]):\n\
  |     ...\n\
  \n\
  """
  def _Decorate (func):
    NewDriver (drvName, func, successState, data)
    return func
  return _Decorate


## Define a new resource.
def NewDriverResource (drvName, rcName, rcType, writable = True):
  """Define a new resource managed by a custom driver defined by 'NewDriver()'."""
  if drvName in _driverDict:
    return RcRegisterResource (drvName, rcName, rcType, writable)
  else:
    if not drvName in _bufferedResources: _bufferedResources[drvName] = []
    _bufferedResources[drvName].append ( (rcName, rcType, writable) )
    return RcGetResource ("/local/" + drvName + "/" + rcName)


## @}
%} // %pythoncode (home2l_resources)





// *************************** Triggered Functions *****************************


%pythoncode %{
## @addtogroup home2l_rules
## @{


import re


_onEventDict = {}
_onUpdateDict = {}
_dailyDict = {}


def _BuildRcList (rcSet):
  # Internal function to get a clean list of resources from any arguments ...
  if isinstance (rcSet, (list, tuple, set)):
    ret = []
    for x in rcSet: ret += _BuildRcList (x)
    return ret
  elif isinstance (rcSet, CResource):
    return [ rcSet ]
  else:
    return [ RcGet (rcSet) ]


def _SubscrIdFromFunc (func):
  # Get a readable (and hopefully unique) ID string for an arbitrary function.
  if func.__name__ == "<lambda>":
    return "_" + re.sub("[^a-zA-Z0-9]+", "_", func.__code__.co_filename).strip ('_') + "_" + str (func.__code__.co_firstlineno)
  return func.__name__


def _BuildSubscriber (id, rcList):
  # Internal function to create a subscriber and enable it for 'CRcEventProcessor::Select()'.
  # ~ print ("### _BuildSubscriber (" + id + ", " + str(rcList))
  subscr = RcNewSubscriber (id)
  for rc in rcList: subscr.AddResource (rc)
  subscr.SetInSelectSet (True)
  return subscr;





# ****************** RunOnEvent() **************************


## Define a function to be called on events.
def RunOnEvent (func, rcSet, data = None, subscrId = None):
  """Define a function to be called whenever an event for a set of\n\
  resources occurs.\n\
  \n\
  'rcSet' can either be a 'CResource' object, an URI string or a tuple\n\
  or a list of any of those. 'data' is an optional reference to user\n\
  data that will be passed unchanged to any 'func' invocations.\n\
  \n\
  For each event encountered, the function 'func' will be called as follows:\n\
  \n\
      func (ev, rc, vs [ , data ] )\n\
  \n\
  where '(ERcEventType) ev' is the type of event, 'rc' is the affected\n\
  resource, and, if applicable, 'vs' is the new value and state that\n\
  has been reported for the resource.\n\
  \n\
  The event type 'ev' can be one of the following:\n\
      rceValueStateChanged : The value or state has changed.\n\
      rceConnected         : The connection has been (re-)established.\n\
      rceDisconnected      : The connection has been lost.\n\
  \n\
  Unlike 'RunOnUpdate', the function 'func' will be called for each single\n\
  event in the correct order in time, so that no temporary value changes\n\
  get lost.\n\
  \n\
  The parameter 'subscrId' sets the subscriber ID. It can usually be left\n\
  unspecified, in which case, the function name is used as an identifier.\n\
  """
  if not subscrId: subscrId = _SubscrIdFromFunc (func)
  subscr = _BuildSubscriber (subscrId, _BuildRcList (rcSet))
  _onEventDict[subscrId] = (func, subscr, data)


## Decorator to define a function to be called on events.
def onEvent (*rcSet):
  """Decorator variant of 'RunOnEvent()'.\n\
  \n\
  This decorator allows to easily let a function be executed on given events\n\
  as follows:\n\
  \n\
  |   @onEvent (<resource set>)\n\
  |   def MyFunc (ev, rc, vs):\n\
  |     ...\n\
  \n\
  """
  def _Decorate (func):
    RunOnEvent (func, rcSet)
    return func
  return _Decorate





# ****************** RunOnUpdate() *************************


## Define a function to be called on value/state changes of resources.
def RunOnUpdate (func, rcSet, data = None, subscrId = None):
  """Define a function to be called on value/state changes of resources.\n\
  \n\
  'rcSet' can either be a 'CResource' object, an URI string or a tuple\n\
  or a list of any of those. 'data' is an optional reference to user\n\
  data that will be passed unchanged to any 'func' invocations.\n\
  \n\
  If any of the specified resources changes its value or state,\n\
  the function 'func' will be called as follows\n\
  \n\
      func (a, b, c, ... [ , data ] )\n\
  \n\
  where "a, b, c, ..." are arbitrary positional arguments, which (if present)\n\
  are filled with the current values of the resources specified by 'rcSet'.\n\
  The arguments are optional, and their names can be chosen arbitratily.\n\
  Values are filled in in the same order as the ordering of the resources\n\
  specified by 'rcSet'. The values are obtained by calling 'CResource.Value()'.\n\
  \n\
  Unlike 'RunOnEvent', only 'rceValueStateChanged' events cause\n\
  invocations of 'func', and multiple events quickly following each\n\
  other may be merged to a single invocation, so that only the last value\n\
  is actually reported. For this reason, this mechanism is generally more\n\
  efficient and should be preferred if short intermediate value changes\n\
  are not of interest.\n\
  \n\
  Important: Resource states and values may change any time. If resource\n\
  values (or states) are queried again within the function, they may differ\n\
  from the result of a previous query.\n\
  \n\
  The parameter 'subscrId' sets the subscriber ID. It can usually be left\n\
  unspecified, in which case, the function name is used as an identifier.\n\
  """

  # Build subscriber ...
  if not subscrId: subscrId = _SubscrIdFromFunc (func)
  rcList = _BuildRcList (rcSet)
  subscr = _BuildSubscriber (subscrId, rcList)

  # Determine the number of function arguments or 'None' if it is variable ...
  funcArgs = func.__code__.co_argcount
  try:
    # Dirty trick to check if 'func' has variable positional arguments:
    # If behind the explicit arguments a variable named "args" follows, we assume
    # that the header continues with "*args".
    # TBD: Is there a better solution?
    if 'args' == func.__code__.co_varnames[funcArgs]:
      funcArgs = None
  except IndexError: pass
  # ~ print ("### RunOnUpdate(): funcArgs = " + str(funcArgs) + ", rcList = " + str (rcList))
  # ~ print ("### RunOnUpdate(): ... func.__code__.co_varnames = " + str (func.__code__.co_varnames))

  # Store record ...
  _onUpdateDict[subscrId] = (func, subscr, rcList, funcArgs, data)


## Decorator to define a function to be called on value/state changes.
def onUpdate (*rcSet):
  """Decorator variant of 'RunOnUpdate()'.\n\
  \n\
  This decorator allows to easily define a function executed on value changes\n\
  as follows:\n\
  \n\
  |   @onUpdate ( <resource set> )\n\
  |   def MyFunc ():\n\
  |     ...\n\
  \n\
  """
  def _Decorate (func):
    RunOnUpdate (func, rcSet)
    return func
  return _Decorate





# ****************** Connect...() *************************


## Define a connector between resources.
def Connect (target, rcSet, func = lambda x: x, attrs = None, id = None, priority = None, t0 = None, hysteresis = None, delDelay = None, subscrId = None):
  """Connect source (sensor) resource(s) to a target (actor) resource.\n\
  \n\
  This creates a connector, which continuously updates requests for the target\n\
  resource 'target' based on a transfer function depending on the values of\n\
  the resources given by 'rcSet'.\n\
  \n\
  'target' specifies the target resource - either by its URI or by a\n\
  'CResource' reference.\n\
  \n\
  'rcSet' specifies the source (sensor) resource(s). It may be a single\n\
  resource or a tuple or list of multiple resources.\n\
  \n\
  'func' is the transfer function transforming source (sensor) values into\n\
  request values for the target resource. It is called as:\n\
  \n\
      value = func (a, b, c, ...)\n\
  \n\
  'a, b, c, ...' are positional arguments with arbitrary names by which the\n\
  values of the 'rcSet' resources are passed. The function must return the actual\n\
  value requested for 'target'. If 'None' is returned, the request is deleted\n\
  (eventually after a delay specified by 'delDelay'). The default for 'func' is\n\
  the identiy function, transporting source values directly without modifications.\n\
  \n\
  The remaining arguments ('attrs', 'id', ..., 'delDelay') specify the request\n\
  attributes used for placing the respective requests. See RcSetRequest() for\n\
  details. Multiple connectors can be defined with different request IDs\n\
  and priorities, and the request resolution mechanism will resolve them\n\
  properly. This way, complex automation rules can be specified (one example is\n\
  given below).
  \n\
  Notes on writing transfer functions ('func'):
  \n\
  1. Any of the arguments 'a, b, c, ...' may also be 'None' if the respective\n\
     resource has a state of 'rcsUnkown'. This must be considered when writing\n\
     transfer functions. For example, to check whether a Boolean resource x is\n\
     known and false, the expression should be 'if x == False: ...' instead of\n\
     'if not x: ...', since in the latter case 'not x' would evaluate as true,\n\
     if x is 'None'. In general, Boolean resource values should always be used\n\
     in conjunction with a comparision. For example, an expression like\n\
     'a and b' may be written as '(a == True) and (b != False)', which implies,\n\
     that a value of 'None' is treated like 'False' for 'a', but like 'True'\n\
     for 'b'.\n\
  \n\
  2. To facilitate function writing, especially if the source resources have\n\
     numerical types, 'TypeError' exceptions are caught by the caller and result\n\
     in a return value of 'None'. 'TypeError' exceptions are typically raised\n\
     if one of the arguments of an arithmetic exception is 'None'.
  \n\
  3. The transfer function is evaluated whenever a value/state change occurs for\n\
     any of the resources of 'rcSet', but only then. If resources are read\n\
     inside the gate function, they usually must be contained in 'rcSet'.\n\
     It is good practice to not read out resources directly, but only rely\n\
     on the arguments passed to the gate function.
  \n\
  Examples:
  \n\
  - Let a light be switched on for 5 seconds if a motion sensor is activated, but only\n\
    at night time:\n\
  \n\
    |   rcLight = RcGet ('/alias/light')  # get reference to the light resource\n\
    |   rcLight.SetDefault (0)            # set default request: light off (0)\n\
    |   Connect (rcLight, '/alias/day', lamda x: 0 if x == True else None, attrs = '#daytime *3')\n\
    |     # keep light off (0) at day time (True) at high priority (*3)\n\
    |   Connect (rcLight, '/alias/motion', lamda x: 1 if x == True else None, attrs = '#motion *2', delDelay = '5s')\n\
    |     # switch light on (1) if the motion detector is active at a lower\n\
    |     # priority (*2); after the motion sensor becomes inactive, the request\n\
    |     # is deleted with a delay of 5 seconds ('5s')
  \n\
  - Force the light off at day time, but during rainy weather:
  \n\
    |   Connect (rcLight, ('/alias/motion', '/alias/rain'), lamda m, r: 0 if m == True and not r == True else None)\n\
  \n\
    or alternatively (decorator variant):
  \n\
    |   @connect (rcLight, ('/alias/motion', '/alias/rain')):\n\
    |   def LightOffDuringDayAndGoodWeather (motion, rain):\n\
    |     # (some additional code may be added here)
    |     if motion == True and not rain == True: return 0\n\
    |     else: return None\n\
  """

  # Convert 'target' into a resource object for faster access later ...
  if isinstance (target, str): target = RcGet (target)

  # Callback suitable for RunOnUpdate(), use closure to include request attributes ...
  def _OnUpdateFunc (*args):
    try:              value = func (*args)
    except TypeError: value = None
    target.SetRequest (value = value, attrs = attrs, id = id, priority = priority, t0 = t0, hysteresis = hysteresis, delDelay = delDelay)
    # ~ print ("### _OnUpdateFunc (" + str (args) + "): value = " + str (value))

  # Register callback ...
  # ~ print ("### Connect(): rcList = " + str (rcSet))
  if not subscrId: subscrId = _SubscrIdFromFunc (func)
  RunOnUpdate (_OnUpdateFunc, rcSet, subscrId = subscrId)


## Decorator to define a connector.
def connect (target, rcSet, attrs = None, id = None, priority = None, t0 = None, hysteresis = None, delDelay = None):
  """Decorator variant of 'Connect()'.\n\
  \n\
  This allows to decorate a transfer function, thus creating a connector,\n\
  which continuously updates requests for the target resource 'target' depending\n\
  on the values of the resources given by 'rcSet':\n\
  \n\
  |   @connect ( <target>, <rcSet> [, <request attrs>] )\n\
  |   def TransferFunc (a, b, c, ...):\n\
  |     ...\n\
  |     return <value>
  \n\
  """
  def _Decorate (func):
    Connect (target, rcSet, func = func, attrs = attrs, id = id, priority = priority, t0 = t0, hysteresis = hysteresis, delDelay = delDelay)
    return func
  return _Decorate


# TBD: The following shortcuts require a solution to set the subscriber ID properly (ideally to the file/line number of caller).


# ~ ## Decorator shortcut.
# ~ def Connect1x (target, source, attrs = None, id = None, priority = None, t0 = None, hysteresis = None, delDelay = None, subscrId = None):
  # ~ """Connect with funktion '1 if source > 0 else None'"""
  # ~ Connect (target, source, lambda x: 1 if x > 0 else None, attrs, id, priority, t0, hysteresis, delDelay, subscrId)


# ~ ## Decorator shortcut.
# ~ def Connect10 (target, source, attrs = None, id = None, priority = None, t0 = None, hysteresis = None, delDelay = None, subscrId = None):
  # ~ """Connect with funktion '1 if source > 0 else 0'"""
  # ~ Connect (target, source, lambda x: 1 if x > 0 else 0, attrs, id, priority, t0, hysteresis, delDelay, subscrId)


# ~ ## Decorator shortcut.
# ~ def Connect0x (target, source, attrs = None, id = None, priority = None, t0 = None, hysteresis = None, delDelay = None, subscrId = None):
  # ~ """Connect with funktion '0 if source > 0 else None'"""
  # ~ Connect (target, source, lambda x: 0 if x > 0 else None, attrs, id, priority, t0, hysteresis, delDelay, subscrId)


# ~ ## Decorator shortcut.
# ~ def Connect01 (target, source, attrs = None, id = None, priority = None, t0 = None, hysteresis = None, delDelay = None, subscrId = None):
  # ~ """Connect with funktion '1 if source > 0 else 1'"""
  # ~ Connect (target, source, lambda x: 0 if x > 0 else 1, attrs, id, priority, t0, hysteresis, delDelay, subscrId)





# ****************** RunAt() *******************************


_timerDict = {}


## Let a function be called at a given time or periodically.
def RunAt (func, t = 0, dt = 0, args = None, subscrId = None):
  """Define a function to be called at a given time, optionally repeated\n\
  at regular intervals.\n\
  \n\
  The starting time 't' can be specified by anything accepted by 'TicksAbsOf()'.\n\
  The interval 'dt' (if given) can be anything accepted by 'TicksRelOf()'.\n\
  If 'dt' <= 0, the timer is executed once and then deactivated. Otherwise,\n\
  the function is repeated regularly.\n\
  \n\
  The function 'func' will be called as follows:\n\
  \n\
      func (<args>)\n\
  \n\
  where <args> is the 'args' object passed to 'RunAt()', if 'func' has just one\n\
  argument and 'args' it is neither a tuple nor a dictionary.\n\
  If 'func' has at least two arguments and 'args' is a tuple or a dictionary,\n\
  its components are passed as individual arguments to 'func', either as\n\
  positional (tuple) or keyword (dictionary) arguments, respectively.\n\
  \n\
  If 'subscrId' is not set, the function name will be used as an ID.\n\
  Calling this function again with the same ID causes the old timer to be deleted.\n\
  Presently, there is no mechanism to delete a previously defined timer.\n\
  """
  if not subscrId: subscrId = _SubscrIdFromFunc (func)
  ep = CRcEventTimer (subscrId)
  ep.SetInSelectSet (True)
  ep.Set (TicksMonotonicFromAbs (TicksAbsOf (t)), TicksRelOf (dt))
  # ~ print ("### " + str (TicksNow ()) + ": RunAt(" + str (TicksAbsOf(t)) + ", " + str (TicksRelOf (dt)) + "): " + str (ep))
  _timerDict[subscrId] = (func, args, ep)
    # Note: We store 'ep' in '_timerDict' just to keep a reference to it.
    #   Otherwise, the garbage collector would remove the object together with its timer!


## Decorator to let a function be called at a given time or periodically.
def at (t = 0, dt = 0, args = None):
  """Decorator variant of 'RunAt()'.\n\
  \n\
  This decorator allows to execute a function at given times\n\
  as follows:\n\
  \n\
  |   @at ( t = <t> [, dt = <interval>] [, args = <args>] )\n\
  |   def MyTimedFunc ( [<args>] ):\n\
  |     ...\n\
  \n\
  """
  def _Decorate (func):
    RunAt (func, t, dt, args = args)
    return func
  return _Decorate





# ****************** RunDaily() ****************************


## Let a function be called daily for setting permanent requests.
def RunDaily (func, hostSet = None, data = None, subscrId = None):
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
      func ( [host [ , data ] ] )\n\
  \n\
  'host' is the host name for which rules must be updated.\n\
  \n\
  Important: On each host in the set, the 'timer' driver must be enabled.\n\
  \n\
  The parameter 'subscrId' can usually be left unspecified. In this case, the function\n\
  names are used as identifiers.\n\
  """
  def _AddToSet (s, host):
    if isinstance (host, (list, tuple, set)):
      for x in host: _AddToSet (s, x)
    else:
      s.add ("/host/" + host + "/timer/daily")

  if not subscrId: subscrId = _SubscrIdFromFunc (func)
  if hostSet:
    rcSet = set ()
    _AddToSet (rcSet, hostSet)
  else:
    rcSet = "/local/timer/daily"
  subscr = _BuildSubscriber (subscrId, _BuildRcList (rcSet))
  _dailyDict[subscrId] = (func, subscr, data)


## Decorator to let a function be called daily for setting permanent requests.
def daily (*hostSet):
  """Decorator variant of 'RunDaily'.\n\
  \n\
  This decorator allows to easily define a function executed daily or whenever
  a host becomes reachable (again) as follows:\n\
  \n\
  |   @daily ( [ <host set> ] )\n\
  |   def MyFunc (host):\n\
  |     ...\n\
  \n\
  """
  def _Decorate (func):
    RunDaily (func, hostSet)
    return func
  return _Decorate



## @}
%} // %pythoncode (home2l_rules)





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

%feature("docstring") TicksOfDate "Convert a date into 'TTicks' units (absolute)."
TTicks TicksOfDate (int dy, int dm, int dd);

%feature("docstring") TicksOfTime "Convert a time into 'TTicks' units (relative).\n\n"
  "The values returned by 'TicksOfDate' and 'TicksOfTime' as well as a\n"
  "number of milliseconds can be simply added to obtain a 'TTicks' value\n"
  "for a certain time at a certain date."
TTicks TicksOfTime (int th, int tm, int ts);

%inline %{
static inline TTicks TicksOfSeconds (int secs) { return TICKS_FROM_SECONDS (secs); }
static inline TTicks TicksOfSeconds (float secs) { return (float) TICKS_FROM_SECONDS (secs); }
static inline TTicks TicksOfMillis (int ms) { return ms; }
%}

TTicks TicksAbsFromMonotic (TTicks tm);
TTicks TicksMonotonicFromAbs (TTicks ta);



// ***** Internal functions *****


%newobject _TicksAbsStr ();
%newobject _TicksRelStr ();
%inline %{

static inline const char *_TicksAbsStr (TTicks ticks) {
  CString s;
  TicksAbsToString (&s, ticks);
  return s.Disown ();
}

static inline const char *_TicksRelStr (TTicks ticks) {
  CString s;
  TicksRelToString (&s, ticks);
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


def TicksAbsStr (ticks):
  """Return an absolute TTicks value as a string in Home2L format."""
  if not isinstance (ticks, int): return None
  return _TicksAbsStr (ticks)


def TicksRelStr (ticks):
  """Return a relative TTicks value as a string in Home2L format."""
  if not isinstance (ticks, int): return None
  return _TicksRelStr (ticks)


def TicksAbsOf (something):
  """Magic wand to convert anything to an absolute TTicks value as intuitively as possible.\n\
  The function is similar to TicksRelOf(), but it guarantees to return an absolute time.\n\
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
    Note: Be careful when calling this variant close to midnight. Functions triggered by\n\
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
    if something > 99999999999: return something
    else: return TicksNow () + something
  if isinstance (something, float): return TicksAbsOf (round (something))

  if isinstance (something, datetime.datetime):   # Note: 'datetime' is a subclass of 'date' => this must be first!
    return TicksOfDate (something.year, something.month, something.day) \
         + TicksOfTime (something.hour, something.minute, something.second) + round (something.microsecond / 1000)
  if isinstance (something, datetime.date):       # Note: 'datetime' is a subclass of 'date' => this must be behind!
    return TicksOfDate (something.year, something.month, something.day)
  if isinstance (something, datetime.time):
    return TicksToday () + round (((something.hour * 60 + something.minute) * 60 + something.second) * 1000 + something.microsecond / 1000)
  if isinstance (something, datetime.timedelta):
    return TicksNow () + round (something.total_seconds() * 1000)

  return None     # We had an unkown type.


def TicksRelOf (something):
  """Magic wand to convert anything to a relative 'TTicks' value as intuitively as possible.\n\
  The function is similar to TicksAbsOf(), but it guarantees to return a relative time.\n\
  \n\
  - Values of type string are interpreted as time specifications for TicksFromString()\n\
    or as accepted by the Home2L Shell.\n\
  \n\
  - Values of numeral types are returned as they are.\n\
  \n\
  - For values of type 'datetime.time', the relative time from the beginning of a day\n\
    is returned.\n\
  \n\
  - For values of type 'datetime.timedelta', the relative delta time in milliseconds\n\
    is returned.\n\
  \n\
  Note: Values of type 'datetime.date' or 'datetime.datetime' are not supported.\n\
  """
  if isinstance (something, str):
    t = _TicksFromString (something, False)
    if t >= 0: return t
    else: return None

  if isinstance (something, int): return something
  if isinstance (something, float): return round (something)

  if isinstance (something, datetime.time):
    return TicksOfTime (something.hour, something.minute, something.second) + round (something.microsecond / 1000)
  if isinstance (something, datetime.timedelta):
    return round (something.total_seconds() * 1000)

  return None     # We had an unkown type.


def TicksToPyDateTime (t):
  """Return a Python 'datetime.datetime' object for an absolute ticks value."""
  return datetime.datetime.fromtimestamp (t/1000)


def TicksToPyTimeDelta (t):
  """Return a Python 'datetime.timedelta' object for a relative ticks value."""
  return datetime.timedelta (milliseconds = t)


def TicksFromUTC (t):
  """Correct an absolute ticks value created from a UTC time string."""
  return t + (datetime.datetime.fromtimestamp (t/1000) - datetime.datetime.utcfromtimestamp (t/1000)).seconds * 1000;


def TicksToUTC (t):
  """Convert an absolute ticks value to a value that will return UTC time when converted to a string."""
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

_home2lStarted = False
_home2lStopped = False

%}
%feature("pythonappend") Home2lInit %{
  #~ print ("### Registering atexit function.")
  atexit.register (Home2lDone)

  global _home2lStarted
  _home2lStarted = False
  global _home2lStopped
  _home2lStopped = False
%}
%feature("pythonprepend") Home2lDone %{
  #~ print ("### Home2lDone()...")
  atexit.unregister (Home2lDone)
%}


// Helper for 'Home2lStart()' to avoid exporting C function 'RcStart()'.
%inline %{
static inline void _RcStart () {
  RcStart ();
}
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


import signal


## End the elaboration and start the background activities of the Home2L package.
def Home2lStart ():
  """Start the background activities of the Home2L package.\n\
  \n\
  This primarily ends the elaboration phase of the Resources library and starts\n\
  the server (if enabled). New drivers and resources must be defined before this.\n\
  """
  global _home2lStarted
  if not _home2lStarted:

    # Make some sanity checks ...
    for drvName in _bufferedResources.keys ():
      # Resources without driver...
      rcList = []
      for rcName, rcType, writable in _bufferedResources[drvName]: rcList.append (rcName)
      print ("WARNING: Resources declared for missing driver '" + drvName + "': " + str (rcList))

    # Start ...
    _RcStart ()
    _home2lStarted = True


## Iterate Home2L and run all pending callback functions.
def Home2lIterate (maxTime = 0):
  """Iterate Home2L for a time of 'maxTime' ms and run all pending callback functions.\n\
  \n\
  If 'maxTime == 0', only the currently pending work is done, and the\n\
  function does not wait. If 'maxTime < 0', the function may wait\n\
  indefinitely (this feature is used by 'Run').\n\
  """
  # ~ print ("### Home2lIterate()")
  global _home2lStarted
  if not _home2lStarted: Home2lStart ()      # implicitly end the elaboration phase if not done yet

  # Switch off the Python-internal Ctrl-C handler while waiting ...
  #   This is necessary to keep Ctrl-C effective. Otherwise, keyboard interrupts would be deferred when
  #   waiting in a C function (e.g. during a long-waiting 'Select')
  if maxTime != 0:
    s = signal.getsignal (signal.SIGINT)
    signal.signal (signal.SIGINT, signal.SIG_DFL)
  ep = CRcEventProcessor.Select (maxTime);
  if maxTime != 0:
    signal.signal (signal.SIGINT, s)
  # ~ print ("### Home2lIterate(): Select done")

  # Return if no event available ...
  if not ep: return
  ev = ep.PollEvent ()
  if not ev: return
  epType = ep.TypeId ()
  epLid = ep.InstId ()
  # ~ print ("### Home2lIterate(): Event: ", epType, epLid);

  # Process event ...
  if epType == 'S':     # Subscriber...

    # Check 'RunOnEvent'...
    if epLid in _onEventDict:
      func, subscr, data = _onEventDict[epLid]
      while ev:     # loop to quickly process many events
        if func.__code__.co_argcount == 3:     # tolerate functions without the 'data' argument ...
          func (ev.Type (), ev.Resource (), ev.ValueState ())
        else:
          func (ev.Type (), ev.Resource (), ev.ValueState (), data)
        ev = ep.PollEvent ()

    # Check 'RunOnUpdate'...
    elif epLid in _onUpdateDict:
      func, subscr, rcList, funcArgs, data = _onUpdateDict[epLid]
      lastEv = None
      while ev:
        if ev.Type () == rceValueStateChanged: lastEv = ev
        ev = ep.PollEvent ()
      if lastEv:
        argList = []
        # ~ print ("### rcList = " + str(rcList))
        for rc in rcList: argList.append (rc.Value ())
        if funcArgs != None or data != None: argList.append (data)
          # If the function arguments are variable (as in '_OnUpdateFunc()' in 'Connect()'),
          # we add the 'data' argument iff it is != 'None'. Otherwise, we pass as many
          # arguments as 'func' takes.
        if funcArgs != None: del argList[funcArgs:]     # tolerate functions with fewer arguments ...
        # ~ print ("### Home2lIterate() on update: funcArgs = " + str(funcArgs) + ", argList = " + str(argList))
        func (*argList)

    # Check 'RunDaily'...
    elif epLid in _dailyDict:
      func, subscr, data = _dailyDict[epLid]
      hostSet = set()
      while ev:
        if ev.Type () == rceValueStateChanged or ev.Type () == rceConnected:
          host = ev.Resource ().Uri ().split ('/') [2]
          hostSet.add (host)
        ev = ep.PollEvent ()
      for host in hostSet:
        if func.__code__.co_argcount == 0:   func ()
        elif func.__code__.co_argcount == 1: func (host)
        else:                                func (host, data)
    else:
      print ("WARNING: Received event on unknown subscriber '" + epLid + "'")

  # Driver (driving) ...
  elif epType == 'D':
    func, data = _driverDict[epLid]
    if func == None:
      print ("WARNING: Received drive event for '" + str (ev.Resource ()) + "', but driver has no drive function.")
    else:
      if func.__code__.co_argcount == 2:     # tolerate functions without the 'data' argument ...
        func (ev.Resource (), ev.ValueState ())
      else:
        func (ev.Resource (), ev.ValueState (), data)

  # Timer ('RunAt') ...
  elif epType == 'T':
    # ~ print ("### Home2lIterate(): Timer '" + epLid + "'")
    func, args, ep = _timerDict[epLid]
    if func.__code__.co_argcount == 0:    func ()       # tolerate functions without an argument
    elif func.__code__.co_argcount == 1:  func (args)   # single argument: pass unchecked
    elif isinstance (args, tuple):        func (*args)  # call with positional arguments
    elif isinstance (args, dict):         func (**args) # call with keyword arguments
    else:                                 func (args)   # call unchanged (this may fail, but will hopefully give a meaningful exception report)
    while ev: ev = ep.PollEvent ()  # Ignore overflown timer events


## Run the Home2L main loop indefinitely (or until stopped).
def Home2lRun ():
  """Run the Home2L main loop indefinitely (or until stopped).\n\
  Executes 'Home2lStart()' if this has not happened before.\n\
  """
  global _home2lStopped
  _home2lStopped = False
  while not _home2lStopped:
    Home2lIterate (-1)


## Let 'Home2lRun()' stop at next occasion.
def Home2lStop ():
  """Let 'Home2lRun()' stop at next occasion.\n\
  \n\
  Note: This (and all top-level) functions may presently only be called\n\
  from a single (main) thread. In particular, stopping from a different\n\
  thread is not supported.\n\
  """
  #~ print ("### Home2lStop")
  global _home2lStopped
  _home2lStopped = True;
  # Note: There is presently no interrupt mechanism to stop 'CRcEventProcessor::Select'. This is no problem as long as
  #       'Stop' is only called from the main thread, namely from any callback function defined by 'RunOn...' or
  #       'NewDriver'. These functions are always called between 'Select' and the next check of 'doStop'.


## @}

## @}


%}   // %pythoncode (home2l_general, home2l)


#endif  // _HOME2L_PYTHON_
