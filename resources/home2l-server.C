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


int main (int argc, char **argv) {
  int sig;

  // Startup...
  EnvInit (argc, argv);
  RcInit (true);

  // Run main timer loop in the foreground...
  sig = RcRun ();
  if (sig) INFOF(("Received signal %i (%s) - exiting.", sig, strsignal (sig)));
  else INFO ("Exiting.");

  // Done ...
  RcDone ();
  EnvDone ();
  return 0;
}
