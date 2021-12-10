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

#include <syslog.h>
#include <unistd.h>   // for 'daemon(3)'
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>
#include <sys/select.h>
#include <grp.h>      // for 'initgroups(3)'
#include <stdarg.h>


ENV_PARA_INT ("daemon.minRunTime", envMinRunTime, 3000);
  /* Minimum run time below which a process is restarted only with a delay
   */
ENV_PARA_INT ("daemon.retryWait", envRetryWait, 60000);
  /* Restart wait time if a processes crashed quickly
   */
ENV_PARA_STRING ("daemon.pidFile", envPidFile, NULL);
  /* PID file for use with 'start-stop-daemon'
   */
ENV_PARA_SPECIAL ("daemon.run.<script>", const char *, NULL);
  /* Define a script to be started and controlled by the daemon
   */


/* Notes on job management
 * -----------------------
 *
 * For each task, a new process group is created. If this daemon process is killed or
 * crashes, all tasks must be shut down by this process (eventually in a signal handler).
 *
 * Command to show the process tree:
 *
 * > ps -u home2l -jfH
 *
 * Behavior if a sub-task crashes:
 * - If it ran longer than 'envMinRunTime', it will be restarted immediately.
 * - If it ran shorter than that time, it will be restarted after a waiting of 'envRetryWait'.
 *
 * If the daemon process is killed, all sub-tasks are killed, too. The respective
 * signals are caught and handled accordingly.
 *
 * A crash of the daemon (e.g. SEGV or ABRT) is not handled, the children keep on running.
 * The code should be hardened and bug-free, so that such crashes do not happen.
 *
 */


class CTask: public CShellBare {
  public:
    CTask () { startTime = 0; SetNewProcessGroup (); }
    virtual ~CTask ();

    void Setup (const char *_id, const char *_cmd);

    virtual void Kill (int sig = SIGTERM);

    void Process ();
    void RetryNow () { if (startTime < 0) startTime = 0; Process (); }

    const char *ToStr (CString *) { return id.Get (); }

  protected:
    CString id, cmd;
    TTicks startTime;
    CTimer retryTimer;
};


static bool foregroundMode = false;
static bool running = true;
static CSleeper sleeper;
static CDictCompact<CTask> taskMap;





// ***************** Helpers *******************************


static void LogF (int priority, const char *format, ...) {
  CString s;
  char c;

  va_list ap;
  va_start (ap, format);

  if (LoggingToSyslog ())
    vsyslog (priority, format, ap);
  else {
    switch (priority) {
      case LOG_INFO:    c = 'I'; break;
      case LOG_WARNING: c = 'W'; break;
      case LOG_ERR:     c = 'E'; break;
      default: c = 'D';
    }
    printf ("%s [%c] ", TicksAbsToString (&s, TicksNow ()), c);
    vprintf (format, ap);
    putchar ('\n');
  }
  va_end (ap);
}





// ***************** CTask *********************************


static void CbRetryTimer (CTimer *, void *data) {
  //~ INFO ("### CbRetryTimer ()");
  ((CTask *) data)->RetryNow ();
}


CTask::~CTask () {
  retryTimer.Clear ();
  CShellBare::Done ();
}


void CTask::Setup (const char *_id, const char *_cmd) {
  id.Set (_id);
  cmd.Set (_cmd);
  Process ();
}


void CTask::Kill (int sig) {
  retryTimer.Clear ();
  CShellBare::Kill (sig);
}


void CTask::Process () {
  CString line;
  TTicks now, lifeTime;

  // Check and log task's stdout (and stderr)...
  if (!ReadClosed ()) {
    while (ReadLine (&line))
      LogF (LOG_INFO, "From '%s': %s", id.Get (), line.Get ());
  }

  // Handle a non-running task...
  //~ INFOF (("# CTask::Process ('%s')", id.Get ()));
  if (!IsRunning ()) {
    now = TicksNowMonotonic ();
    if (startTime > 0) {  // Task was running and has stopped somehow...
      lifeTime = now - startTime;
      if (lifeTime >= envMinRunTime) {
        LogF (LOG_INFO,
              ExitCode () >= 0 ? "Task '%s' has exited (code %i) - restarting now."
                               : "Task '%s' has died - restarting now.",
              id.Get (), ExitCode ());
        startTime = 0;  // -> mark to restart now
      }
      else {
        LogF (LOG_INFO,
              ExitCode () >= 0 ? "Task '%s' has exited (code %i) after only %i second(s) - restarting in %i seconds."
                               : "Task '%s' has died after only %3$i second(s) - restarting in %4$i seconds.",
              id.Get (), ExitCode (),
              (int) SECONDS_FROM_TICKS (lifeTime), (int) SECONDS_FROM_TICKS (envRetryWait - lifeTime));
        retryTimer.Set (startTime + envRetryWait, 0, CbRetryTimer, this);   // -> restart later
        startTime = -1; // -> suspend
      }
    }
    if (startTime == 0) {
      // (Re-)Start the task...
      LogF (LOG_INFO, "Starting task '%s'...", id.Get ());
      Start (cmd.Get (), true);
      startTime = now;
      retryTimer.Clear ();
    }
  }
}





// ***************** Signal Handler ***********************


void SigToSelfPipe (int sig) {
  sleeper.PutCmd (&sig);
}





// ***************** Main **********************************


int main (int argc, char **argv) {
  static const char *envKeyPrefix = "daemon.run.";
  const char *key, *id, *cmd;
  struct passwd *pwEntry;
  struct sigaction sigAction;
  TTicks delay;
  FILE *f;
  CTask *task;
  int n, sig, n0, n1, prefixLen;

  // Startup...
  for (n = 1; n < argc; n++) if (argv[n][0] == '-')
    if (argv[n][1] == 'd') foregroundMode = true;
  if (!foregroundMode) LogToSyslog ("daemon");
  EnvInit (argc, argv,
           "  -d : stay in the foreground (prepend 'debug=1' to enable debugging messages)\n");

  // Daemonize...
  if (!foregroundMode) {
    if (daemon (0, 0) != 0) ERRORF (("Failed to daemonize: %s", strerror (errno)));
  }

  // Write PID file if set...
  //   Note: The PID file will not be removed afterwards, since we drop privileges. To stop the
  //         daemon, 'start-stop-daemon' must be called with the '--remove-pidfile' option.
  if (envPidFile) {
    f = fopen (envPidFile, "wt");
    if (!f) {
      WARNINGF(("Cannot open PID file '%s'", envPidFile));
    }
    else {
      fprintf (f, "%i\n", getpid ());
      fclose (f);
    }
  }

  // Drop privileges...
  if (getuid () == 0) {
    // Program was started by root => change identity to 'home2l:home2l'...
    pwEntry = getpwnam (HOME2L_USER);
    if (!pwEntry) ERRORF (("Cannot identify user '" HOME2L_USER "': %s", strerror (errno)));
    if (initgroups (HOME2L_USER, pwEntry->pw_gid) != 0) ERRORF (("initgroups () failed: %s", strerror (errno)));
    if (setgid (pwEntry->pw_gid) != 0) ERRORF (("setgid(%i) failed: %s", pwEntry->pw_gid, strerror (errno)));
    if (setuid (pwEntry->pw_uid) != 0) ERRORF (("setuid(%i) failed: %s", pwEntry->pw_uid, strerror (errno)));
  }
  LogF (LOG_INFO, "Daemon started (uid = %i, gid = %i).", getuid (), getgid ());

  // Setup sleeper and signal handler...
  sleeper.EnableCmds (sizeof (int));
  sigAction.sa_handler = SigToSelfPipe;
  sigemptyset (&sigAction.sa_mask);
  sigAction.sa_flags = 0;
  sigaction (SIGTERM, &sigAction, NULL);    // 'kill'
  sigaction (SIGINT, &sigAction, NULL);     // keyboard interrupt (Ctrl-C)
  sigaction (SIGCHLD, &sigAction, NULL);    // Child stopped or terminated

  // Read config and setup 'taskMap'...
  EnvGetPrefixInterval (envKeyPrefix, &n0, &n1);
  prefixLen = strlen (envKeyPrefix);
  for (n = n0; n < n1; n++) {
    key = EnvGetKey (n);
    cmd = EnvGetVal (n);
    id = key + prefixLen;
    ASSERT (id && cmd);
    //~ INFOF(("Registering task '%s': '%s'", key, cmd));
    task = new CTask ();
    task->Setup (id, cmd);
    taskMap.Set (id, task);
  }

  // Main loop...
  running = true;
  if (!taskMap.Entries ()) {
    LogF (LOG_INFO, "No tasks defined: Exiting...");
    running = false;
  }
  while (running) {

    // Iterate timer...
    TimerIterate ();
    delay = TimerGetDelay ();
    //~ INFOF(("### TimerGetDelay () -> %i", delay))

    // Prepare & run 'select'...
    sleeper.Prepare ();
    for (n = 0; n < taskMap.Entries (); n++) sleeper.AddReadable (taskMap [n]->ReadFd ());
    sleeper.Sleep (delay);

    // Handle signals...
    if (sleeper.GetCmd (&sig)) {
      LogF (LOG_INFO, "Received signal %i ('%s')", sig, strsignal (sig));
      switch (sig) {
        case SIGTERM:
        case SIGINT:
          running = false;
          break;
        case SIGCHLD:
          // Some child exited: Process all tasks...
          for (n = 0; n < taskMap.Entries (); n++) taskMap[n]->Process ();
          sleeper.Prepare ();   // We do not have to process them later again (hack to make 'sleeper.IsReadable()' return 'false' from now)
          break;
        default: break;
      }
    }

    // Process tasks...
    if (running) for (n = 0; n < taskMap.Entries (); n++) {
      task = taskMap.Get (n);
      if (sleeper.IsReadable (task->ReadFd ())) taskMap[n]->Process ();
    }
  }

  // Kill all tasks...
  LogF (LOG_INFO, "Shutting down ...");
  if (taskMap.Entries ()) {
    for (n = 0; n < taskMap.Entries (); n++) {
      LogF (LOG_INFO, "Terminating task '%s'...", taskMap.GetKey (n));
      taskMap.Get (n)->Kill (SIGTERM);
    }
    for (n = 0; n < taskMap.Entries (); n++) {
      LogF (LOG_INFO, "Waiting for '%s' to finish ...", taskMap.GetKey (n));
      taskMap.Get (n)->Wait ();
    }
    taskMap.Clear ();
  }
  sleeper.Done ();

  // Shutdown...
  EnvDone ();
  LogF (LOG_INFO, "Daemon shut down.");
  return 0;
}
