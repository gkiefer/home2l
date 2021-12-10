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

// This is a demo resource driver, which can be used as a template for new
// drivers.


#include "resources.H"





// Locally managed resources ...
static CResource *rcVersion, *rcBool, *rcInt, *rcFloat, *rcString, *rcPercent, *rcTemp, *rcWindow;





// *************************** Interface function ******************************


HOME2L_DRIVER(demo) (ERcDriverOperation op, CRcDriver *drv, CResource *rc, CRcValueState *vs) {
  switch (op) {

    case rcdOpInit:
      /* Driver "Init" function:
       * - We must register all resources here and may start the background activity of the driver.
       * - From now on, changes can be reported by rc->ReportValue() at any time from any thread.
       */
      DEBUG (1, "drv-demo: Started.");
      rcVersion = RcRegisterResource (drv, "version", rctString, false);
        /* [RC:demo] Home2L version
         */
      rcVersion->ReportValue (buildVersion);
      rcBool = RcRegisterResource (drv, "demoBool", rctBool, true);
      rcBool->SetDefault (false);
        /* [RC:demo] Example resource of type 'bool'
         */
      rcInt = RcRegisterResource (drv, "demoInt", rctInt, true);
      rcInt->SetDefault (42);
        /* [RC:demo] Example resource of type 'int'
         */
      rcFloat = RcRegisterResource (drv, "demoFloat", rctFloat, true);
      rcFloat->SetDefault (3.14159265f);
        /* [RC:demo] Example resource of type 'float'
         */
      rcString = RcRegisterResource (drv, "hello", rctString, true);
      rcString->SetDefault ("world");
        /* [RC:demo] Example resource of type 'string'.
         *
         * Set a request to greet somebody else.
         */
      rcPercent = RcRegisterResource (drv, "demoPercent", rctPercent, true);
      rcPercent->SetDefault (56.7f);
        /* [RC:demo] Example resource of type 'percent'
         */
      rcTemp = RcRegisterResource (drv, "demoTemp", rctTemp, true);
      rcTemp->SetDefault (37.2f);
        /* [RC:demo] Example resource of type 'temp'
         */
      rcWindow = RcRegisterResource (drv, "demoWindow", rctWindowState, true);
      rcWindow->SetDefault (rcvWindowClosed);
        /* [RC:demo] Example resource of type 'window'
         */
      break;

    case rcdOpStop:
      /* Driver "Stop" function:
       * - We must close all own threads here and may not report any changes any more.
       * - The resources will be unregistered later on automatically.
       */
      DEBUG (1, "drv-demo: Stopped.");
      break;

    case rcdOpDriveValue:
      /* Drive a new value:
       * - Add code here to drive a new value to the real device (e.g. some actor).
       * - It is not allowed and not necessary to call a CResource::Report...() method
       *   here. The driven value with state "valid" will be reported automatically.
       *   If that value or state is not appropriate to report, change it in 'vs'.
       */
      CString s;
      DEBUGF (1, ("drv-demo: Driving a new value to '%s': %s", rc->Uri(), vs->ToStr (&s)));
      break;
  }
}
