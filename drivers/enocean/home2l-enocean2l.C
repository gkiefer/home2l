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


#include "env.H"

#include "enocean.H"


static void SignalHandler (int _sigNum) {
  //~ INFO ("### Interrupt ###");
  EnoInterrupt ();
}


int main (int argc, char **argv) {
  struct sigaction sigAction, sigActionSavedTerm, sigActionSavedInt;
  CEnoTelegram telegram;
  EEnoStatus status;
  CString s;

  // Startup ...
  EnvInit (argc, argv);
  EnoInit ();

  // Set signal handlers for SIGTERM (kill) and SIGINT (Ctrl-C) ...
  sigAction.sa_handler = SignalHandler;
  sigemptyset (&sigAction.sa_mask);
  sigAction.sa_flags = 0;
  sigaction (SIGTERM, &sigAction, &sigActionSavedTerm);
  sigaction (SIGINT, &sigAction, &sigActionSavedInt);

  // Run main loop in the foreground ...
  printf ("Waiting for EnOcean telegrams on '%s' ...\n", EnoLinkDevice ());
  do {
    status = EnoReceive (&telegram);
    if (status == enoOk)
      printf (": %s\n", telegram.ToStr (&s));
  }
  while (status != enoInterrupted);
  printf ("\nExiting ...\n");

  // Restore signal handlers
  sigaction (SIGTERM, &sigActionSavedTerm, NULL);
  sigaction (SIGINT, &sigActionSavedInt, NULL);

  // Done ...
  EnoDone ();
  EnvDone ();
  return 0;
}


