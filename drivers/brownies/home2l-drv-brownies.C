/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2019-2020 Gundolf Kiefer
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


#include "resources.H"
#include "brownies.H"


CBrownieSet brDatabase;
CBrownieLink brLink;


class CRcBrownieDriver: public CRcEventDriver, public CThread {
  public:
    CRcBrownieDriver (CBrownieSet *_db)
      : CRcEventDriver ("brownies", rcsBusy) {
      db = _db;
      stop = false;
    }

    virtual void Stop () {      // from CRcDriver
      stop = true;
      Join ();
    }

    virtual void *Run () {      // from CRcThread
      bool haveSocketClient;

      brLink.SocketServerStart ();
      //~ INFO ("### CRcBrownieDriver: Entering main loop.");
      while (!stop) {
        haveSocketClient = brLink.SocketServerIterate (256);
          // Do not let the socket server sleep forever to allow the resources to
          // get invalidated if expired. Expiration is the only thing 'ResourcesIterate'
          // does if a socket client is connected.
        db->ResourcesIterate (haveSocketClient);
      }
      //~ INFO ("### CRcBrownieDriver: Leaving main loop.");
      brLink.SocketServerStop ();
      db->ResourcesDone ();
      return NULL;
    }

  protected:
    CBrownieSet *db;
    volatile bool stop;
};





// *************************** Interface function ******************************


HOME2L_DRIVER(brownies) (ERcDriverOperation op, CRcDriver *drv, CResource *, CRcValueState *) {
  CRcBrownieDriver *brDrv;

  // Delete default driver object ...
  ASSERT(op == rcdOpInit);
  drv->Unregister ();

  // Init database ...
  if (brDatabase.ReadDatabase ())
    INFOF (("Read database file '%s'.", envBrDatabaseFile));
  else {
    WARNINGF (("Failed to read database file '%s' - disabling Brownie driver.", envBrDatabaseFile));
    return;
  }

  // Init link ...
  if (brLink.Open () != brOk) {
    WARNINGF (("Failed to open Brownie link '%s': %s - disabling Brownie driver.",
               envBrLinkDev, BrStatusStr (brLink.Status ())));
    return;
  }
  else
    INFOF (("Connected to '%s' (%s).", brLink.IfName (), TwiIfTypeStr (brLink.IfType ())));

  // Create Brownie driver & thread ...
  brDrv = new CRcBrownieDriver (&brDatabase);
  brDrv->Register ();                         // register driver
  brDatabase.ResourcesInit (brDrv, &brLink);  // register resources
  brDrv->Start ();                            // start background thread
}
