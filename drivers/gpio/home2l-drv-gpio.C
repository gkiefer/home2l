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


#include "resources.H"

#include <dirent.h>
#include <fcntl.h>


static TTicks optInterval = 16;    // number of milliseconds between polls
static TTicks optInertia = 64;     // minimum number of milliseconds for which a value must remain constant before being reported


class CGpioPin {
  public:
    CGpioPin (CResource *_rc, int _fd, bool _isInput);
    ~CGpioPin ();

    void OnUnregister ();

    void DriveValue (CRcValueState *vs);

    void Iterate (TTicks now);  // [T:timer]

    CResource *rc;
    CGpioPin *next;

  protected:
    bool isInput;
    int fd;
    TTicks tLastChange, tLastError;    // 'tLastError' is for input (polled) pins to avoid repeation of errors flooding the log system
    int lastVal;    // -1 = no last value; 0 = false, 1 = true
};





// ***************** CGpioPin ******************************


CGpioPin::CGpioPin (CResource *_rc, int _fd, bool _isInput) {
  rc = _rc;
  isInput = _isInput;
  fd = _fd;
  tLastChange = tLastError = 0;
  lastVal = -1;
}


CGpioPin::~CGpioPin () {
  close (fd);
  fd = -1;
}


void CGpioPin::DriveValue (CRcValueState *vs) {
  char c;
  bool ok;

  //~ INFOF (("### CGpioPin (%s): DriveValue (%s)", rc->Uri (), v->ToStr ()));

  // Sanity ...
  ASSERT (!isInput);
  if (!vs->IsValid ()) return;    // without requests just leave the previous value

  // Write new value to sysfs...
  ok = (lseek (fd, 0, SEEK_SET) == 0);
  if (ok) {
    c = vs->Bool () + '0';
    ok = (write (fd, &c, 1) == 1);
  }

  // Print warning...
  if (!ok) WARNINGF (("Failed to drive value '%s' to GPIO '%s'", vs->ToStr (), rc->Uri ()));

  // Done...
  vs->SetState (ok ? rcsValid : rcsUnknown);
}


void CGpioPin::Iterate (TTicks now) {
  bool ok, val = false;
  char c;

  //~ INFOF (("### CGpioPin::Iterate ('%s')", rc->Lid ()));

  // Sanity...
  ASSERT (rc != NULL && isInput);
  if (fd < 0) return;           // On error, fd is set to -1 to avoid repeated error reports

  // Read pin value from sysfs...
  ok = (lseek (fd, 0, SEEK_SET) == 0);
  if (ok) {
    ok = (read (fd, &c, 1) == 1);
    val = c - '0';
  }

  // Report new value, but only if the last change is not too short ago (debouncing)...
  if (ok && val != lastVal) {
    DEBUGF (2, ("[GPIO] '%s': new value %i, old value/state = %i", rc->Uri (), (int) val, (int) lastVal));
    lastVal = val;
    tLastChange = now;
  }
  if (tLastChange && now >= tLastChange + optInertia) {
    DEBUGF (2, ("[GPIO] '%s': reporting %i", rc->Uri (), (int) lastVal));
    rc->ReportValue (lastVal);
    tLastChange = 0;
  }

  // Handle errors...
  if (!ok) {
    if (!tLastError) {
      WARNINGF (("Failed to read GPIO '%s'", rc->Uri ()));
      tLastError = TicksNow ();
    }
  }
  else {
    if (tLastError) {
      INFOF (("Could read GPIO '%s' again.", rc->Uri ()));
      tLastError = 0;
    }
  }
}





// ***************** Pin Management ************************


static CGpioPin *pinInList = NULL, *pinOutList = NULL;
static CTimer pinTimer;


static void PinsTimerCallback (CTimer *, void *) {
  TTicks now = TicksNow ();
  for (CGpioPin *pin = pinInList; pin; pin = pin->next) pin->Iterate (now);
}


static void PinsInit (CRcDriver *drv) {
  CString dirName, fileName, lid;
  CResource *rc;
  CGpioPin *pin;
  DIR *dir;
  struct dirent *dirEntry;
  char *p, *q;
  int fd = -1;
  bool ok, isInput = false, value = false;

  // Open directory...
  dirName.SetF ("%s/etc/gpio.%s", EnvHome2lRoot (), EnvMachineName ());
  //~ INFOF (("### Processing '%s'...", dirName.Get ()));
  dir = opendir (dirName.Get ());
  if (dir) while ( (dirEntry = readdir (dir)) ) {
    //~ INFOF (("### Processing '%s'...", dirEntry->d_name));
    ok = true;

    // Parse file name...
    p = dirEntry->d_name;
    if (p[0] == '.') continue;      // ignore names starting with '.' (e.g. ".", "..")
    q = NULL;
    while (*p && p < dirEntry->d_name + 255) {
      if (*p == '.') q = p;
      p++;
    }
    if (!q || *p) ok = false;
    if (ok) {
      ok = false;
      for (p = q + 1; *p && !ok; p++)
        switch (*p) {
          case 'i':
            isInput = true;
            ok = true;
            break;
          case '0':
          case '1':
            isInput = false;
            value = *p - '0';
            ok = true;
            break;
        }
    }
    if (!ok) WARNINGF (("Illegal GPIO name: '%s'", dirEntry->d_name));

    // Open sysfs file...
    if (ok) {
      fileName.SetF ("%s/%s/value", dirName.Get (), dirEntry->d_name);
      fd = open (fileName.Get (), isInput ? O_RDONLY : O_RDWR);
      if (fd < 0) {
        WARNINGF (("Cannot open GPIO file '%s'", fileName.Get ()));
        ok = false;
      }
    }

    // Create GPIO record...
    if (ok) {
      lid.Set (dirEntry->d_name, q - dirEntry->d_name);
      rc = CResource::Register (drv, lid.Get (), rctBool, !isInput);    // [RC:-]
      pin = new CGpioPin (rc, fd, isInput);
      rc->SetDriverData (pin);
      if (isInput) {
        // Input: Link to local list of pins to be polled...
        pin->next = pinInList;
        pinInList = pin;
      }
      else {
        // Output: Preset value to default defined in link...
        pin->next = pinOutList;
        pinOutList = pin;
        rc->SetDefault (value);
      }
    }
  }
  closedir (dir);

  // Setup timer...
  if (pinInList) pinTimer.Set (0, optInterval, PinsTimerCallback);
}


static void PinsDone () {
  CGpioPin *pin;

  // Stop timer...
  pinTimer.Clear ();

  // Unregister resources...
  while (pinInList) {
    pin = pinInList;
    pinInList = pinInList->next;
    pin->rc->SetDriverData (NULL);
    delete pin;
  }
  while (pinOutList) {
    pin = pinOutList;
    pinInList = pinOutList->next;
    pin->rc->SetDriverData (NULL);
    delete pin;
  }
}





// ***************** Interface function ********************


HOME2L_DRIVER(gpio) (ERcDriverOperation op, CRcDriver *drv, CResource *rc, CRcValueState *vs) {
  CGpioPin *pin;

  switch (op) {

    case rcdOpInit:
      PinsInit (drv);
      break;

    case rcdOpStop:
      PinsDone ();
      break;

    case rcdOpDriveValue:
      pin = (CGpioPin *) rc->DriverData ();
      pin->DriveValue (vs);
      break;
  }
}





// ********** Testing ***********


/*
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>



int main () {
  int fd, n;
  char c, last;

  fd = open ("/opt/home2l/etc/gpio.mccartney/btn2up.in/value", O_RDONLY);
  assert (fd > 0);
  last = '-';
  n = 0;
  while (1) {
    c = '-';
    assert (lseek (fd, 0, SEEK_SET) == 0);
    read (fd, &c, 1);
    if (c != last) {
      printf ("%8i: %c\n", n, c);
      last = c;
    }
    n++;
  }

  return 0;
}
*/
