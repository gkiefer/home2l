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


#include "rc_drivers.H"

#include <ctype.h>
#include <errno.h>
#include <unistd.h>       // pipe(), ...
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>        // getaddrinfo
#include <arpa/inet.h>    // inet_pton()
#include <fnmatch.h>      // wildcard matching


/*
 * Rules for locking and deadlock prevention
 * =========================================
 *
 * 1. Usually, all necessary locking happens inside the classes, the user
 *    should not be bothered with locking.
 *
 * 2. Critical classes and objects are marked as such, and their fields are
 *    devided into "static" and "dynamic" ones. Only the "dynamic" data is
 *    protected by a mutex.
 *
 * 3. Deadlocks are avoided by breaking the hold-and-wait condition wherever possible.
 *    If this is not possible, objects have to be locked in the following order.
 *    Amoung objects of the same class (same "any" line), the locking order is
 *    according to positive memory adress ordering.
 *
 *    - any CRcDriver::mutex
 *    - any CRcHost::mutex
 *    - any CResource::mutex
 *    - any CRcSubscription::mutex
 *    - unregisteredResourceMapMutex
 *    - serverListMutex
 *    - any CRcServer::mutex
 *
 */


#define RESOLVE_LOCALHOST 1
  // If set, the name of the local machine is automatically mapped to 'localhost'
  // for network connections.





// *************************** Global variables ********************************



// ***** Environment settings *****

ENV_PARA_STRING ("rc.config", envRcConfigFile, "resources.conf");
  /* Name of the Resources configuration file (relative to the 'etc' domain)
   */

ENV_PARA_BOOL ("rc.enableServer", envServerEnabled, false);
  /* Enable the Resources server
   *
   * (Only) if true, the Resources server is started, and the local resources
   * are exported over the network.
   */

uint32_t envServeInterface = INADDR_LOOPBACK;    // binary version for 'envServerInterfaces' (safe restrictive default)
ENV_PARA_STRING ("rc.serveInterface", envServeInterfaceStr, "any");
  /* Select interface(s) for the server to listen on
   *
   * If set to ''any'', connections from any network interface are accepted.
   *
   * If set to ''local'', only connection attempts via the local interface (127.0.0.1)
   * are accepted. This may be useful for untrusted physical networks, where
   * actual connections are implemented e.g. by SSH tunnels.
   *
   * If a 4-byte IP4 address is given, only connections from the interface associated
   * with this IP address are accepted. This way, a certain interface can be selected.
   *
   * This value is passed to bind(2), see ip(7) for more details. The value of
   * ''any'' corresponds to INADDR\_ANY, the value of ''local'' corresponds to
   * INADDR\_LOOPBACK.
   */
uint32_t envNetwork, envNetworkMask;    // in network byte order
ENV_PARA_STRING ("rc.network", envNetworkStr, "127.0.0.1/32");
  /* Network prefix and mask for the Resources server (CIDR notation)
   *
   * Only connections from hosts of this subnet or from 127.0.0.1 (localhost)
   * are accepted by the server.
   */

ENV_PARA_INT ("rc.maxAge", envMaxAge, 60000);
  /* Maximum age (ms) tolerated for resource values and states
   *
   * If a client does not receive any sign of life from a server for this amount of time,
   * the resource is set to state "unknown" locally.
   * Servers send out regular "hello" messages every 2/3 of this time.
   * Reducing the value can guarantee to detect network failures earlier but will
   * increase the traffic overhead for the "hello" messages.
   *
   * This value must be consistent for the complete Home2L cluster.
   */

ENV_PARA_INT ("rc.netTimeout", envNetTimeout, 3000);
  /* Network operation timeout (ms)
   *
   * Waiting time until a primitive network operation (e.g. connection
   * establishment, response to a request) is assumed to have failed if
   * no reply has been received.
   */
ENV_PARA_INT ("rc.netRetryDelay", envNetRetryDelay, 60000);
  /* Time (ms) after which a failed network operation is repeated
   *
   * Only in the first period of \refenv{rc.netRetryDelay} milliseconds, the connection
   * retries are performed at faster intervals of \refenv{rc.netTimeout} ms.
   */
ENV_PARA_INT ("rc.netIdleTimeout", envNetIdleTimeout, 5000);
  /* Time (ms) after which an unused connection is disconnected
   */

ENV_PARA_INT ("rc.relTimeThreshold", envRelTimeThreshold, 60000);
  /* Threshold (in ms from now) below which remote requests are sent with relative times
   *
   * This option allows to compensate negative clock skewing effects between
   * different hosts.
   * If timed requests are sent to remote hosts, and the on/off times are in the future and in less then this
   * number of milliseconds from now, the times are transmitted relative to the current time. This way, the
   * duration of requests is retained, even if the clocks of the local and the remote host diverge.
   * (Example: A door opener request is timed for 1 second and should last exactly this time.)
   */





// ***** Environment *****


bool serverEnabled = false;

TTicksMonotonic RcNetTimeout () { return (TTicksMonotonic) envNetTimeout; }





// ***** Databases *****


CString localHostId;
int localPort = -1;

CDict<CRcHost> hostMap;

CMutex serverListMutex;         // 'serverList' is written only by [T:net], but also read by others
                                //  -> Lock must be acquired for writing OR by non-net-threads.
CRcServer *serverList = NULL;   // [T:w=net,r=any] servers are managed in a chained list and removed after disconnect and clearance

CDict<CRcDriver> driverMap;

CMutex subscriberMapMutex;
CDictRef<CRcSubscriber> subscriberMap;  // References to all registered subscribers

CDictFast<CString> aliasMap;

CMutex unregisteredResourceMapMutex;
CDictRef<CResource> unregisteredResourceMap;





// *************************** URI Path analysis *******************************


const char *uriRootArr [] = { "local", "host", "alias", "env" };
#define uriRoots (sizeof (uriRootArr) / sizeof (char *))

/* Note: When analysing paths, the domains can be abbreviated. Only the first
 *       letter is checked. Anything starting with the respective first letter
 *       is accepted as the respective domain.
 *
 * This may ba changed in the future.
 */


const char *RcGetRealPath (CString *ret, const char *uri, const char *workDir, int maxDepth) {
  CString aliasPart, *s, realUri, absUri;
  const char *p;
  int n;

  // Resolve relative path...
  if (uri[0] != '/' && workDir) {
    absUri.SetF ("%s/%s", workDir, uri);
    absUri.PathNormalize ();
    uri = absUri.Get ();
  }

  // Handle "local" domain...
  if (uri[0] == '/' && uri[1] == 'l') {

    // Handle path in "local" domain...
    p = uri + 2;
    while (p[0] != '\0' && p[-1] != '/') p++;
    ret->SetF ("/host/%s/%s", localHostId.Get (), p);
  }

  // Handle alias...
  else if (uri[0] == '/' && uri[1] == 'a') {
    p = uri + 2;
    while (p[0] && p[-1] != '/') p++;
    if (!p[0]) {

      // No second path component (e.g. URI = "/alias/")...
      //~ INFOF (("#   no second path component -> '%s'", uri));
      ret->Set (uri);
    }
    else {

      // Try to match sub-paths, start with the longest...
      aliasPart.Set (p);
      while (true) {
        //~ INFOF (("#   looking up '%s'", aliasPart.Get ()));
        s = aliasMap.Get (aliasPart.Get ());
        if (s) {

          // Found alias...
          ret->SetC (s->Get ());
          ret->Append (p + aliasPart.Len ());
          //~ INFOF (("#   found alias '%s' -> '%s'", s->Get (), ret->Get ()));
          if (maxDepth > 0 && (*ret)[0] == '/' && (*ret)[1] == 'a') {
            // The alias maps to another alias: Resolve it recursively...
            aliasPart.Set (*ret);      // save contents of 'realUri'
            ret->Set (RcGetRealPath (&realUri, aliasPart.Get (), NULL, maxDepth - 1));
            //~ INFOF (("#   ('%s') after recursion -> '%s'", uri, realUri.Get ()));
          }
          break;
        }
        else {

          // Cut off last path component and search again...
          n = aliasPart.RFind ('/');
          if (n <= 0) {
            //~ INFOF (("   no matching alias found -> '%s'", uri));
            ret->Set (uri);
            break;      // no more components
          }
          else aliasPart[n] = '\0';
        }
      }
    }
  }

  // Not an alias: Return unmodified (full) path ...
  else {

    //~ INFOF (("###   not an alias -> '%s'", uri));
    ret->Set (uri);
  }

  // Done...
  //~ INFOF(("### RcGetRealPath ('%s') -> '%s'", uri, ret->Get ()));
  return ret->Get ();
}


ERcPathDomain RcAnalysePath (const char *uri, const char **retLocalPath, CRcHost **retHost, CRcDriver **retDriver, CResource **retResource, bool allowWait) {
  const char *p, *q;
  CString comp;
  ERcPathDomain domain;
  const char *_retLocalPath;
  CRcHost *_retHost;
  CRcDriver *_retDriver;
  CResource *_retResource;

  //~ INFOF (("RcAnalysePath ('%s') ...", uri));

  // Set default return values...
  _retLocalPath = uri;
  _retHost = NULL;
  _retDriver = NULL;
  _retResource = NULL;

  // Sanity checks...
  if (!uri) { domain = rcpNone; goto done; }
  if (uri[0] != '/')  { domain = rcpNone; goto done; }

  // Check root (level-0) component...
  p = q = uri + 1;
  while (*q && *q != '/') q++;
  if (*q != '/') {
    // No trailing slash => leave it with root domain...
    _retLocalPath = p;
    domain = rcpRoot;
    goto done;
  }
  switch (*p) {
    case 'l': domain = rcpDriver; break;
    case 'h': domain = rcpHost; break;
    case 'a': domain = rcpAlias; break;
    case 'e': domain = rcpEnv; break;
    default: domain = rcpNone; goto done;
  }

  // Move next (level-1) component...
  p = (++q);
  while (*q && *q != '/') q++;

  // Try to further evaluate host path...
  if (domain == rcpHost && *q == '/') {
    //~ INFOF (("RcAnalysePath ('%s'): domain == rcpHost ...", uri));
    comp.Set (p, q - p);
    //~ INFOF(("  comparing '%s' with '%s'...", comp.Get (), localHostId.Get ()));
    if (comp.Compare (localHostId.Get ()) == 0) domain = rcpDriver;       // local host?
    else if ( (_retHost = hostMap.Get (comp.Get ())) ) domain = rcpResource;   // known remote host?

    // If host was known: Move on to next path component...
    if (domain != rcpHost) {
      p = (++q);
      while (*q && *q != '/') q++;
    }
  }

  // Try to further evaluate driver path...
  if (domain == rcpDriver && *q == '/') {
    comp.Set (p, q-p);
    if ( (_retDriver = driverMap.Get (comp.Get ()))) {
      domain = rcpResource;
      p = (++q);
      while (*q && *q != '/') q++;
    }
  }

  // Now 'p' is the local path...
  _retLocalPath = p;

  // Try to identify the resource...
  //~ INFOF (("p = '%s'", p));
  if (domain == rcpResource) {
    if (_retHost) _retResource = _retHost->GetResource (p, retResource ? allowWait : false);
    if (_retDriver) _retResource = _retDriver->GetResource (p);
  }

done:
  if (retLocalPath) *retLocalPath = _retLocalPath;
  if (retHost) *retHost = _retHost;
  if (retDriver) *retDriver = _retDriver;
  if (retResource) *retResource = _retResource;
  //~ INFOF(("### Analyse '%s': domain = %i, localPath = '%s', host = %08x, driver = %08x", uri, domain, _retLocalPath, _retHost, _retDriver));
  return domain;
}


bool RcSelectResources (const char *pattern, int *retItems, CResource ***retItemArr, CKeySet *retWatchSet) {
  CString realPattern, realUri, aliasPart;
  const char *localPath;
  ERcPathDomain dom;
  CRcHost *rcHost;
  CRcDriver *rcDriver;
  CResource *rc;
  CKeySet newWatchSet;
  int n, k;

  *retItems = 0;
  *retItemArr = NULL;
  if (retWatchSet) retWatchSet->Clear ();

  // Resolve aliases and analyse path...
  realPattern.Set (RcGetRealPath (&realUri, pattern));
  dom = RcAnalysePath (realPattern.Get (), &localPath, &rcHost, &rcDriver, &rc);

  // Interpret for cases with at least host or driver fully specified...
  if (dom == rcpResource && (rcHost || rcDriver)) {
    if (rc) {

      // Case 1: 'RcAnalysePath' found an exact match => no wildcard, resource known => can just add it ...
      *retItems = 1;
      *retItemArr = MALLOC (CResource *, 1);
      (*retItemArr)[0] = rc;
      return true;
    }
    else {

      // Case 2: No exact match => may have a wildcard or not-yet existing resource...
      retWatchSet->Set (realPattern.Get ());   // need to watch it later

      if (rcHost) {
        // walk through all resources of the host...
        n = rcHost->LockResources ();
        *retItems = 0;
        *retItemArr = MALLOC (CResource *, n);
        for (k = 0; k < n; k++) {
          rc = rcHost->GetResource (k);
          rc->Lock ();
          if (fnmatch (localPath, rc->Lid (), URI_FNMATCH_OPTIONS) == 0)
            (*retItemArr) [(*retItems)++] = rc;
          rc->Unlock ();
        }
        rcHost->UnlockResources ();
      }

      if (rcDriver) {
        // walk through all resources of the host...
        n = rcDriver->LockResources ();
        *retItems = 0;
        *retItemArr = MALLOC (CResource *, n);
        for (k = 0; k < n; k++) {
          rc = rcDriver->GetResource (k);
          rc->Lock ();
          if (fnmatch (localPath, rc->Lid (), URI_FNMATCH_OPTIONS) == 0)
            (*retItemArr) [(*retItems)++] = rc;
          rc->Unlock ();
        }
        rcDriver->UnlockResources ();
      }
      return true;
    }
  }

  // All other cases are invalid or not implemented yet.
  return false;
}


// ***** URI Root *****


int RcGetUriRoots () {
  return uriRoots;
}


const char *RcGetUriRoot (int n) {
  return uriRootArr[n];
}





// *************************** Networking **************************************


/* Network protocol
 * ================
 *
 * 1. From client to server:
 *
 *  a) Operational messages
 *
 *    h <client host id> <prog name> <version>     # connect ("hello") message
 *
 *    s+ <subscriber> <driver>/<rcname>            # subscribe to resource (no wildcards allowed); <subscriber> is the origin of the subscriber
 *    s- <subscriber> <driver>/<rcname>            # unsubscribe to resource (no wildcards allowed)
 *
 *    r+ <driver>/<rcname> <reqGid> <request specification>     # add or change a request
 *    r- <driver>/<rcname> <reqGid>                             # remove a request
 *
 *  b) Informational messages
 *
 *    # i* messages must be issued synchronously, no new request may be issued before the "i."
 *    # response has been received.
 *
 *    ir <driver>/<rcname> <verbosity>  # request the output of 'CResource::GetInfo'
 *
 *    is <verbosity>                    # request the output of 'CRcSubscriber::GetInfoAll'
 *
 *  c) Shell execution
 *
 *    ec <command name> [<args>]      # Execute command defined by "sys.cmd.<command name>"
 *    e <text>                        # Supply data sent to STDIN of the previously started command
 *    e.                              # Send EOF to the previously started command
 *
 *
 * 2. From server to client:
 *
 *  a) Operational messages
 *
 *    h <prog name> <version>           # connect ("hello") message, sent in reply to client's "h ..." and sometimes as "alive" message
 *
 *    d <driver>/<rcname> <type> <rw>   # declaration of exported resource; is sent automatically for all resources after a connect
 *    d.                                # no more resources follow: client may disconnect if there are no other wishes
 *    d-                                # forget (unregister) all resources from this host
 *
 *    v <driver>/<rcname> [~]<value> [<timestamp>] # value/state changed; timestamp is set by client, the server timestamp is optional and presently ignored if sent
 *    v <driver>/<rcname> ?             # state changed to "unknown"
 *
 *  b) Informational messages
 *
 *    i <text>                  # response to any "i*" request (format: see 1.b), more lines may follow
 *    i.                        # end of info
 *
 *  c) Shell execution
 *
 *    e <text>                  # response to an 'e *' request (shell command); 'text' starts  exactly after two characters ("e ")
 *    e.                        # end of the response
 *
 *  d) Alive
 *
 *    At least every 'envMaxAge*2/3' milliseconds, a message is sent.
 *    If no other events occur, this is the "h ..." message.
 *
 */


/* On the updating of resource values:
 * (comment originally from CResource::ReadValueState)
 * TBD: Should we do this? Normally, a new subscriber should ASAP receive an
 *   event with the most up-to-date value. The stored value is already up-to-date,
 *   if another subscription to the resource already exists and no error
 *   (network disconnection) has happend.
 *   => In the network code, we must make sure, that on a connection loss,
 *      all dependent resources are set to 'rcsUnknown' after a given timeout.
 *   => Drivers are fully responsible for invalidations, i.e. if they loose
 *      contact to their hardware, they must activly invalidate the value.
 *   => Then, this code is correct. Also, this code does not require
 *      that the subscription receives an initial event.
 */


/* The NetThread
 * =============
 *
 * The NetThread is a background thread that handles all kind of networking tasks
 * not suitable for the main thread. These are:
 *
 * a) Monitor receiving sockets for all 'CRcHost' objects in 'hostMap'.
 * b) Monitor receiving sockets for all 'CRcServer' in 'serverList'.
 * c) Execute long-running methods in the background ("Net Tasks")
 */


static CNetThread netThread;


struct TNetTask {
  ENetOpcode opcode;
  CNetRunnable *runnable;
  void *data;
};


static const char *GetLocalUri (const char *localPath) {
  CString *ret = GetTTS ();
  ret->SetF ("/host/%s/%s", localHostId.Get (), localPath);
  return ret->Get ();
}


static CResource *GetLocalResource (const char *localPath) {
  return RcGetResource (GetLocalUri (localPath), false);
}


static const char *GetRemoteUri (CRcHost *host, const char *localPath) {
  CString *ret = GetTTS ();
  ret->SetF ("/host/%s/%s", host->Id (), localPath);
  return ret->Get ();
}


static CResource *GetRemoteResource (CRcHost *host, const char *localPath) {
  return RcGetResource (GetRemoteUri (host, localPath), false);
}


void CNetThread::Start () {
  struct sockaddr_in listenAdr;
  int sockOptPara;

  // Create Pipe for task messages...
  sleeper.EnableCmds (sizeof (TNetTask));

  // Initialize listening server socket...
  //~ INFO("### CNetThread::Start...");
  if (serverEnabled) {
    //~ INFO("### Enabling server...");

    // Create listening socket...
    listenFd = socket (AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0)
      ERRORF (("Failed to create socket: %s", strerror (errno)));
    if (fcntl (listenFd, F_SETFL, fcntl (listenFd, F_GETFL, 0) | O_NONBLOCK) < 0)
      ERRORF (("Failed to make socket non-blocking: %s", strerror (errno)));

    // Set 'SO_REUSEADDR' option to allow the reuse shortly after a restart...
    sockOptPara = 1;
    setsockopt (listenFd, SOL_SOCKET, SO_REUSEADDR, &sockOptPara, sizeof (sockOptPara));
      // Note: This code was developed and tested with the additional option SO_REUSEPORT. This
      //       option (introduced in Linux 3.9?) is not available in Android and probably not necessary here.

    // Bind the socket...
    bzero (&listenAdr, sizeof (listenAdr));
    listenAdr.sin_family = AF_INET;
    listenAdr.sin_addr.s_addr = envServeInterface;
    listenAdr.sin_port = htons (localPort);
    //~ INFOF(("### envPort = %i, listenAdr.sin_port = %i", envPort, listenAdr.sin_port));
    if (bind (listenFd, (struct sockaddr *) &listenAdr, sizeof (listenAdr)) < 0)
      ERRORF (("Failed to bind socket: %s", strerror (errno)));

    // Make socket passive...
    if (listen (listenFd, 8) < 0)
      ERRORF (("Failed to listen on socket: %s", strerror (errno)));

    INFOF (("Starting server '%s' listening on port %i (interface: %s)",
            localHostId.Get (), localPort, envServeInterfaceStr));
  }

  // Start the thread...
  CThread::Start ();
}


void CNetThread::Stop () {

  // Nicely stop the thread...
  if (IsRunning ()) {
    AddTask (noExit);
    Join ();
  }

  // Close server listening port...
  if (listenFd >= 0) {
    close (listenFd);
    listenFd = -1;
  }

  // We do NOT close the task pipe here, since some other threads may be sending some more tasks.
  // (These will remain in the pipe now, but not cause an error.)
}


void CNetThread::AddTask (ENetOpcode opcode, CNetRunnable *runnable, void *data) {
  TNetTask nt;

#if WITH_CLEANMEM
  bzero (&nt, sizeof (TNetTask));
#endif
  nt.opcode = opcode;
  nt.runnable = runnable;
  nt.data = data;

  //~ INFOF (("### CNetThread::AddTask (%i), sleeper = %08x", opcode, &sleeper));
  sleeper.PutCmd (&nt);
}


void *CNetThread::Run () {
  CRcHost *host;
  CRcServer *server, **pSrv;
  TNetTask netTask;
  struct sockaddr_in sockAdr;
  socklen_t sockAdrLen;
  char buf[INET_ADDRSTRLEN+1];
  uint32_t peerAdr;   // IPv4 adress in network order
  uint16_t peerPort;  // in network order
  CString adrString;
  int n, fd;
  bool done;

  done = false;
  while (!done) {

    // Collect all receiving FDs from hosts and servers...
    //~ INFO("### CNetThread: Begin main loop...");
    sleeper.Clear ();
    if (listenFd >= 0) sleeper.AddReadable (listenFd);
    for (n = 0; n < hostMap.Entries (); n++) {
      host = hostMap.Get (n);
      sleeper.AddReadable (fd = host->Fd ());
      if (host->WritePending ()) sleeper.AddWritable (fd);
    }
    for (server = serverList; server; server = server->next) {
      sleeper.AddReadable (fd = server->Fd ());
      if (server->WritePending ()) sleeper.AddWritable (fd);
    }

    // Sleep...
    //~ INFOF(("### CNetThread: Sleep..."));
    sleeper.Sleep ();

    // Let hosts and servers receive their data...
    //~ INFOF(("### CNetThread: Handle readable and writable FDs..."));
    for (n = 0; n < hostMap.Entries (); n++) {
      host = hostMap.Get (n);
      if (sleeper.IsReadable (host->Fd ())) {
        host->OnFdReadable ();
        //~ INFOF (("### CRcHost::OnFdReadable (%s)", host->Id ()));
      }
      if (sleeper.IsWritable (host->Fd ())) {
        //~ INFOF (("### CRcHost::OnFdWritable (%s)", host->Id ()));
        host->OnFdWritable ();
      }
    }
    for (server = serverList; server; server = server->next) {
      if (sleeper.IsReadable (server->Fd ())) {
        //~ INFOF (("### CRcServer::OnFdReadable (%s)", server->HostId ()));
        server->OnFdReadable ();
      }
      if (sleeper.IsWritable (server->Fd ())) {
        //~ INFOF (("### CRcServer::OnFdWritable (%s)", server->HostId ()));
        server->OnFdWritable ();
      }
    }

    // Handle task...
    //~ INFOF(("### CNetThread: Handle tasks..."));
    if (sleeper.GetCmd (&netTask)) {
      switch (netTask.opcode) {
        case noExit:
          // Notify hosts to let them join their connection threads...
          for (n = 0; n < hostMap.Entries (); n++)
            hostMap.Get (n)->NetRun (netTask.opcode, netTask.data);
          // Stop the loop...
          done = true;
          break;
        default:
          netTask.runnable->NetRun (netTask.opcode, netTask.data);
      }
    }

    // Handle incoming connection requests...
    //~ INFOF(("### CNetThread: Handle incoming requests..."));
    if (sleeper.IsReadable (listenFd)) {
      sockAdrLen = sizeof (sockAdr);
      fd = accept (listenFd, (struct sockaddr *) &sockAdr, &sockAdrLen);
      if (fd < 0) ERRORF (("Failed to accept new connection: %s", strerror (errno)));
      peerAdr = sockAdr.sin_addr.s_addr;
      peerPort = sockAdr.sin_port;
      adrString.SetF ("%s:%i", inet_ntop (AF_INET, &(sockAdr.sin_addr), buf, INET_ADDRSTRLEN), (uint32_t) ntohs (peerPort));

      // Check client's IP adress...
      if (peerAdr != htonl (INADDR_LOOPBACK) && (peerAdr ^ envNetwork) & envNetworkMask) {
        ((char *) strchr (adrString.Get (), ':')) [0] = '\0';  // cut off port number
        WARNINGF (("Rejecting unauthorized connection attempt from %s", adrString.Get ()));
        close (fd);
      }
      else {

        // Make FD non-blocking...
        if (fcntl (fd, F_SETFL, fcntl (fd, F_GETFL, 0) | O_NONBLOCK) < 0)
          ERRORF (("Failed to make socket non-blocking (fd = %i): %s", fd, strerror (errno)));

        // Create and register new server object...
        server = new CRcServer (fd, peerAdr, peerPort, adrString.Get ());
        serverListMutex.Lock ();
        server->next = serverList;
        serverList = server;
        serverListMutex.Unlock ();
      }
    }

    // Cleanup disconnected servers...
    //   In order to delete a 'CRcServer' object, we must make sure that a) no thread may
    //   be accessing it and b) there is no pending operation for it in the task pipe.
    //   Here, we check a) and then post a 'snoDelete' command to the queue, which will be
    //   executed after all presently existing operations (b).
    //~ INFOF(("### CNetThread: Cleanup disconnected servers..."));
    serverListMutex.Lock ();
    pSrv = &serverList;
    while (*pSrv) {
      server = *pSrv;
      // Check complex condition a)...
      if (ATOMIC_READ (server->state) == scsDisconnected /* && !server->HasSubscribers () && !server->aliveTimer.Pending () && !server->execShell && !server->execTimer.Pending () */) {
        ATOMIC_WRITE (server->state, scsInDeletion);          // mark as "in deletion"
        *pSrv = server->next;                   // unlink from 'serverList'
        netThread.AddTask ((ENetOpcode) snoDelete, server);   // schedule deletion from heap
      }
      else pSrv = &((*pSrv)->next);    // advance pointer (only if this object was not unlinked)
    }
    serverListMutex.Unlock ();
  }

  // Disconnect and remove all servers ...
  serverListMutex.Lock ();
  while (serverList) {
    server = serverList;
    server->Disconnect ();
    ATOMIC_WRITE (server->state, scsInDeletion);
    serverList = server->next;
    delete server;
  }
  serverListMutex.Unlock ();

  // Done ...
  return NULL;
}


void RcNetStart () {
  int n;

  netThread.Start ();

  // Contact all known hosts to obtain info...
  for (n = 0; n < hostMap.Entries (); n++)
    netThread.AddTask ((ENetOpcode) hnoSend, hostMap.Get (n));
}


void RcNetStop () {
  TTicksMonotonic timeLeft;
  bool haveOpenRequests;
  int n;

  // Wait until no more requests are open to be transmitted to remote hosts (or timeout)...
  timeLeft = envNetTimeout;

  // a) Wait until unregistered resources with requests get registered...
  while (timeLeft > 0) {
    haveOpenRequests = false;
    unregisteredResourceMapMutex.Lock ();
    for (n = 0; n < unregisteredResourceMap.Entries (); n++)
      if (unregisteredResourceMap.Get (n)->HasRequests ()) haveOpenRequests = true;
    unregisteredResourceMapMutex.Unlock ();
    if (!haveOpenRequests) timeLeft = 0;   // no open requests => stop waiting
    else {
      Sleep (timeLeft > 64 ? 64 : timeLeft);
      timeLeft -= 64;
    }
  }

  // b) Wait until all pending writes to hosts are sent out...
  while (timeLeft > 0) {
    haveOpenRequests = false;
    for (n = 0; n < hostMap.Entries (); n++)
      if (hostMap.Get (n)->WritePending ()) { haveOpenRequests = true; break; }
    if (!haveOpenRequests) timeLeft = 0;   // no open requests => stop waiting
    else {
      Sleep (timeLeft > 64 ? 64 : timeLeft);
      timeLeft -= 64;
    }
  }

  // Note: We are lazy by not disconnecting all hosts and servers now.
  //   However, they will be closed by their destructors very soon anyway
  //   and no faulty behaviour should result from that. So we leave it this way.
  netThread.Stop ();
}





// *************************** CRcServer ***************************************


CRcServer::CRcServer (int _fd, uint32_t _peerIp4Adr, uint16_t _peerPort, const char *_peerAdrStr) {
  DEBUGF (1, ("Accepting client connection from '%s'", _peerAdrStr));
  //~ INFOF (("Server for '%s' starting, fd = %i", _peerAdrStr, _fd));

  fd = _fd;

  peerIp4Adr = _peerIp4Adr;
  peerPort = _peerPort;
  peerAdrStr.Set (_peerAdrStr);

  ATOMIC_WRITE (state, scsNew);

  execShell = NULL;
}


CRcServer::~CRcServer () {
  DEBUGF (1, ("Closing client connection from '%s'", peerAdrStr.Get ()));
  // see comment on thread-safety in 'CRcHost::~CRcHost'
  ASSERT (ATOMIC_READ (state) == scsInDeletion);
  //~ Disconnect ();
}


void CRcServer::Disconnect () {

  // Delete subscribers...
  Lock ();
  subscrDict.Clear();
  Unlock ();

  // Shell...
  execTimer.Clear ();
  FREEO(execShell);

  // Alive timer...
  aliveTimer.Clear ();

  // Close socket...
  //~ INFOF (("Server for '%s' disconnecting, old fd = %i", peerAdrStr.Get (), fd));
  if (fd >= 0) {
    //~ if (close (fd) < 0) DEBUGF (1, ("Error in 'close(%i)': %s", fd, strerror (errno)));
    close (fd);
    fd = -1;
  }

  // Update state...
  ATOMIC_WRITE (state, scsDisconnected);
}



// ***** Callbacks *****


static void CRcServerAliveTimerCallback (CTimer *, void *data) {
  netThread.AddTask ((ENetOpcode) snoAliveTimer, (CNetRunnable *) data);
}


static void CRcServerExecTimerCallback (CTimer *, void *data) {
  netThread.AddTask ((ENetOpcode) snoExecTimer, (CNetRunnable *) data);
}


static bool CRcServerCbOnSubscriberEvent (CRcEventProcessor *subscr, CRcEvent *ev, void *data) {
  //~ INFOF (("### CRcServerCbOnSubscriberEvent (%s): %s", subscr->InstId (), ev->ToStr ()));

  // Wake up the net thread, which will then poll the subscriber event.
  netThread.AddTask ((ENetOpcode) snoSubscriberEvent, (CNetRunnable *) data, subscr);
  return false;
}


void CRcServer::OnFdReadable () {
  CString line, def, info;
  bool error;
  char **argv;
  int argc;
  CResource *rc;
  CRcSubscriber *subscr;
  CRcValueState vs;
  CRcDriver *driver;
  CRcHost *host;
  const char *uri;
  TTicks t1;
  int n, k, num, verbosity;

  if (!receiveBuf.AppendFromFile (fd)) {
    DEBUGF (1, ("Server for '%s': Network receive error, disconnecting", HostId ()));
    Disconnect ();
  }
  error = false;
  while (receiveBuf.ReadLine (&line) && !error) {
    DEBUGF (3, ("[resources] From client '%s' (%s): '%s'", hostId.Get (), peerAdrStr.Get (), line.Get ()));

    // Interpret line...
    line.Strip ();
    error = false;
    argv = NULL;
    switch (line[0]) {

      case 'h':   // h <client host id> <version>     # connect ("hello") message
        line.Split (&argc, &argv);
        if (argc != 3) { error = true; break; }
        Lock ();
        hostId.Set (argv[1]);
        Unlock ();
        ATOMIC_WRITE (state, scsConnected);

        // Send "hello" back...
        sendBuf.AppendF ("h %s %s\n", EnvInstanceName (), buildVersion);

        // Send resources...
        for (n = 0; n < driverMap.Entries (); n++) {
          driver = driverMap.Get (n);
          //~ DEBUGF (("### Reporting ressources of driver '%s'...", driver->Id ()));
          num = driver->LockResources ();
          for (k = 0; k < num; k++) {
            rc = driver->GetResource (k);
            sendBuf.AppendF ("d %s/%s\n", driver->Lid (), rc->ToStr (true));
          }
          driver->UnlockResources ();
        }
        sendBuf.Append ("d.\n");
        //~ INFO ("### Reported resources.");
        //~ SendFlush ();
        ResetAliveTimer ();

        // Check if we know the client and eventually initiate a return visit ...
        host = hostMap.Get (hostId.Get ());
        if (host) {
          DEBUGF (1, ("[resources] I know this host '%s'... I am interested in it!", host->Id ()));
          host->RequestConnect ();
        }
        break;

      case 's':   // s+ <subscriber lid> <driver>/<rcname>             # subscribe to resource (no wildcards allowed)
                  // s- <subscriber lid> <driver>/<rcname>             # unsubscribe to resource (no wildcards allowed)
        line.Split (&argc, &argv);
        if (argc != 3) { error = true; break; }
        def.SetF ("%s/%s", hostId.Get (), argv[1]);   // subscriber GID
        subscr = subscrDict.Get (def.Get ());
        if (!subscr) {
          // Create new subscriber...
          subscr = new CRcSubscriber ();
          subscr->RegisterAsAgent (def.Get ());
          subscr->SetCbOnEvent (CRcServerCbOnSubscriberEvent, this);
          Lock ();
          subscrDict.Set (subscr->Lid (), subscr);
          Unlock ();
        }
        uri = GetLocalUri (argv[2]);
        switch (line[1]) {
          case '+':
            subscr->DelResources (uri);   // unsubscribe first - it may not have been properly cleared before
            subscr->AddResources (uri);
            break;
          case '-':
            subscr->DelResources (uri);
            break;
          default: error = true;
        }
        break;

      case 'r':   // r+ <driver>/<rcname> <reqGid> <request specification>     # add or change a request
                  // r- <driver>/<rcname> <reqGid>                             # remove a request
        line.Split (&argc, &argv, 3);
        rc = argc < 3 ? NULL : GetLocalResource (argv[1]);
        error = true;
        if (rc) switch (line[1]) {
          case '+':
            rc->SetRequestFromStr (argv[2]);
            error = false;
            break;
          case '-':
            line.Split (&argc, &argv, 4);
            if (argc == 3) {      // no time given ...
              rc->DelRequest (argv[2], 0);
              error = false;
            }
            else if (argc >= 4) if (argv[3][0] == '-')      // time attribute given ...
              if (TicksFromString (argv[3] + 1, &t1, true)) {
                rc->DelRequest (argv[2], t1);
                error = false;
              }
            break;
        }
        break;

      case 'i':
        switch (line[1]) {

          case 'r':   // ir <driver>/<rcname> <verbosity>  # request the output of 'CResource::GetInfo'
            line.Split (&argc, &argv);
            if (argc != 3) { error = true; break; }
            verbosity = argv[2][0] - '0';
            if (verbosity < 0 || verbosity > 3) { error = true; break; }
            rc = GetLocalResource (argv[1]);
            if (!rc) { WARNINGF (("Unknown resource '%s'", argv[1])); error = true; break; }

            rc->GetInfo (&info, verbosity);
            sendBuf.AppendFByLine ("i %s\n", info.Get ());
            break;

          case 's':   // is <verbosity>                    # request the output of 'CRcSubscriber::GetInfoAll'
            if (line.Len () != 4) { error = true; break; }
            verbosity = line[3] - '0';
            if (verbosity < 0 || verbosity > 3) { error = true; break; }
            CRcSubscriber::GetInfoAll (&info, verbosity);
            sendBuf.AppendFByLine ("i %s\n", info.Get ());
            break;

          default:
            error = true;
        }
        if (!error) {
          sendBuf.Append ("i.\n");
          ResetAliveTimer ();
        }
        break;

      case 'e':   // ec <command name> [<args>]      # Execute command defined by "sys.cmd.<command name>"
                  // e <text>                        # Supply data sent to STDIN of the previously started command
                  // e.                              # Send EOF to the previously started command
        switch (line[1]) {
          case 'c':
            line.Split (&argc, &argv, 3);
            if (argc < 2) { error = true; break; }
            if (!execShell) execShell = new CShellBare ();
            if (!execShell->StartRestricted (argv[1], argc == 3 ? argv[2] : NULL)) {
              WARNINGF (("Unable to start command '%s (%s)' - previous command not completed yet?", argv[1], argc == 3 ? argv[2] : ""));
              error = true;
            }
            execTimer.Set (0, 100, CRcServerExecTimerCallback, this);
            break;

          case ' ':
            execShell->WriteLine (line.Get () + 2);
            break;

          case '.':
            execShell->WriteClose ();
            break;

          default:
            error = true;
        }
        break;

      default:
        error = true;
    } // switch (line[0])

    // Cleanup and post-processing...
    if (argv) {
      FREEP (argv[0]);
      free (argv);
    }
    if (error) {
      SECURITYF (("Malformed message received from '%s' - disconnecting: '%s'", peerAdrStr.Get (), line.Get ()));
      Disconnect ();
    }
  } // while (...)
}


void CRcServer::NetRun (ENetOpcode opcode, void *data) {
  CString s, line;
  CRcEvent ev;
  bool canPostponeAliveTimer;

  DEBUGF (3, ("[resources] CRcServer::NetRun (%s, %i), state = %i", HostId (), opcode, ATOMIC_READ (state)));

  canPostponeAliveTimer = false;
  switch ((EServerNetOpcode) opcode) {

    case snoDelete:
      delete this;
      return;         // make sure this method is quit immediately ('this' is now gone and invalid...)

    case snoSubscriberEvent:
      if (ATOMIC_READ (state) != scsConnected) break;
      //~ INFOF (("### snoSubscriberEvent (%s)", HostId ()));

      while (((CRcSubscriber *) data)->PollEvent (&ev)) {
        //~ INFOF (("###   ev = %s", ev.ToStr ()));
        if (ev.Type () == rceValueStateChanged) {
          sendBuf.AppendF ("v %s/%s %s\n", ev.Resource ()->Driver ()->Lid (), ev.Resource ()->Lid (),
                            ev.ValueState ()->ToStr (&s, false, false, true));
          canPostponeAliveTimer = true;
        }
      }
      break;

    case snoAliveTimer:
      if (ATOMIC_READ (state) != scsConnected) break;
      sendBuf.AppendF ("h %s %s\n", EnvInstanceName (), buildVersion);
      break;

    case snoExecTimer:
      if (ATOMIC_READ (state) != scsConnected) break;
      while (execShell->ReadLine (&line))
        sendBuf.AppendF ("e %s\n", line.Get ());
      if (!execShell->IsRunning ()) {
        sendBuf.Append ("e.\n");
        execTimer.Clear ();
      }
      break;

    default:
      break;
  };

  if (canPostponeAliveTimer) ResetAliveTimer ();
}



// ***** Helpers *****


void CRcServer::SendFlush () {
  int bytesToWrite, bytesWritten;

  if (ATOMIC_READ (state) == scsConnected) {

    // Write 'sendBuf' to socket...
    bytesToWrite = sendBuf.Len ();
    if (bytesToWrite) {
      DEBUGF (3, ("[resources] Sending to client %s (%s):\n%s", hostId.Get (), peerAdrStr.Get (), sendBuf.Get ()));

      bytesWritten = write (fd, sendBuf.Get (), bytesToWrite);
      if (bytesWritten == bytesToWrite) sendBuf.Clear ();
      else {
        if (bytesWritten >= 0) DEBUGF (3, ("[resources]   ... written %i out of %i bytes.", bytesWritten, bytesToWrite));
        else {
          if (errno == EAGAIN || errno == EWOULDBLOCK) DEBUGF (3, ("[resources]   ... would block."));
          else DEBUGF (3, ("[resources]   ... error: %s", strerror (errno)));
        }
        // Could not write everything: schedule a retry...
        sendBuf.Del (0, bytesWritten);
        //~ netThread.AddTask ((ENetOpcode) snoSend, this);
      }
    }
  }
}


void CRcServer::ResetAliveTimer () {
  aliveTimer.Set (TicksMonotonicNow () + envMaxAge * 2 / 3, envMaxAge * 2 / 3, CRcServerAliveTimerCallback, this);
}



// ***** Info *****


void CRcServer::GetInfo (CString *ret, int verbosity) {
  static const char *stateNames[] = { "new", "connected", "disconnected", "in deletion" };
  CRcSubscriber *subscr;
  CString info;
  int n;

  Lock ();
  ret->SetF ("Client %-16s(%18s): %s\n", hostId.Get (), peerAdrStr.Get (), stateNames[ATOMIC_READ (state)]);
  if (verbosity >= 1) {
    if (!subscrDict.Entries ()) ret->Append ("  (no subscribers)\n");
    else for (n = 0; n < subscrDict.Entries (); n++) {
      subscr = subscrDict.Get (n);
      subscr->GetInfo (&info, verbosity - 1);
      ret->AppendFByLine ("  %s\n", info.Get ());
    }
  }
  Unlock ();
}


void CRcServer::PrintInfoAll (FILE *f, int verbosity) {
  CString info;
  CRcServer *srv;

  serverListMutex.Lock ();
  for (srv = serverList; srv; srv = srv->next) {
    srv->GetInfo (&info, verbosity);
    fprintf (f, "%s", info.Get ());
  }
  serverListMutex.Unlock ();
}





// ***************** CRcHost & friends *********************


void *CConThread::Run () {
  char buf[INET_ADDRSTRLEN+1];
  CString s, newErrStr, hostId, netHost;
  //~ const char *netHostResolved;
  int netPort;
  struct addrinfo aHints, *aInfo;
  struct sockaddr_in sockAdr, *pSockAdr;
  fd_set fdSet;
  struct timeval tv;
  int soError;
  socklen_t soErrorLen = sizeof (soError);
  int errNo, bytes;
  bool ok;

  // Make a local copy of the host data to be safe towards cancellation...
  Lock ();
  if (host) {
    hostId.Set (host->Id ());
    netHost.Set (host->netHost.Get ());
    netPort = host->netPort;
  }
  else netPort = 0;
  Unlock ();

  // Go ahead...
  DEBUGF (1, ("Contacting server '%s'", hostId.Get ()));
  fd = -1;
  ok = true;

  // Resolve hostname if required...
  if (!port) {

    // Call 'getaddrinfo' to lookup host name...
    CLEAR (aHints);
    aHints.ai_family = AF_INET;     // we only accept ip4 adresses
    aHints.ai_socktype = SOCK_STREAM;
    errNo = getaddrinfo (netHost.Get (), NULL, &aHints, &aInfo);
    if (errNo) {
      ok = false;
      newErrStr.Set (gai_strerror (errNo));
      //freeaddrinfo (aInfo);aInfo = NULL;    // on Android, 'freeaddrinfo ()' here leads to a segfault

    }
    else {

      // Success: Store the address info...
      Lock ();
      pSockAdr = (struct sockaddr_in *) aInfo->ai_addr;
      ip4Adr = pSockAdr->sin_addr.s_addr;
      port = htons ((uint16_t) netPort);
      adrString.SetF ("%s:%i", inet_ntop (AF_INET, &(pSockAdr->sin_addr), buf, INET_ADDRSTRLEN), netPort);
      Unlock ();
      freeaddrinfo (aInfo);
    }
  }

  // Check cancellation state...
  Lock ();
  if (!host) ok = false;
  Unlock ();

  // Try to connect...
  if (ok) {

    // Create socket...
    fd = socket (AF_INET, SOCK_STREAM, 0);
    if (fd < 0) ERRORF (("Cannot create socket: %s", strerror (errno)));
    fcntl (fd, F_SETFL, O_NONBLOCK);    // make non-blocking

    //~ DEBUGF (2, ("# Connecting to '%s'...", hostId.Get ()));

    CLEAR (sockAdr);
    sockAdr.sin_family = AF_INET;
    sockAdr.sin_addr.s_addr = ip4Adr;
    sockAdr.sin_port = port;

    // Initiate 'connect' (non-blocking)...
    connect (fd, (sockaddr *) &sockAdr, sizeof (sockAdr));
    soError = errno;

    // Wait for completion or timeout...
    FD_ZERO (&fdSet);
    FD_SET (fd, &fdSet);
    tv.tv_sec = envNetTimeout / 1000;
    tv.tv_usec = (envNetTimeout % 1000) * 1000;
    if (select (fd + 1, NULL, &fdSet, NULL, &tv) == 1)
      getsockopt (fd, SOL_SOCKET, SO_ERROR, &soError, &soErrorLen);

    // Check success...
    if (soError != 0) {
      ok = false;
      newErrStr.Set (strerror (soError));
      close (fd);
      fd = -1;
    }
    //~ else DEBUGF (("# ... conntected to '%s'.", hostId.Get ()));
  }

  // Send greeting...
  //   This is the only place something is sent without using the 'CRcHost::Send' method.
  //   However, the channel has not yet transferred to the caller, so that no race conditions
  //   can occur. Second, the success of 'connect' does not guarantee that the connection is
  //   usable. Hence, we write something here.
  if (ok) {
    s.SetF ("h %s %s\n", localHostId.Get (), buildVersion);
    bytes = s.Len ();
    if (write (fd, s.Get (), bytes) != bytes) ok = false;
  }

  // Wrap up...
  Lock ();
  tLastAttempt = TicksNow ();
  if (!host) {    // cancelled?
    errString.Set ("(cancelled)");
  }
  else {          // not cancelled...
    if (ok) {
      DEBUGF (1, ("Connection to '%s' established.", hostId.Get ()));
      errString.Clear ();
    }
    else {
      if (newErrStr.Compare (errString.Get ()) != 0) {
        errString.SetO (newErrStr.Disown ());
        DEBUGF (1, ("Cannot %s '%s': %s - continue trying", port ? "connect to" : "resolve", hostId.Get (), errString.Get ()));
      }
    }
    netThread.AddTask ((ENetOpcode) (ok ? hnoConSuccess : hnoConFailed), host);
  }
  Unlock ();

  // Done...
  return NULL;
}





// ***** Callbacks *****


static void CRcHostTimerCallback (CTimer *, void *data) {
  netThread.AddTask ((ENetOpcode) hnoTimer, (CNetRunnable *) data);
}





// ***** Init/Done *****


CRcHost::CRcHost () {
  state = hcsNewRetryWait;
  fd = -1;
  sendBufEmpty = true;
  infoBusy = infoComplete = false;
  execBusy = execComplete = execWriteClosed = false;
  tAge = tRetry = tIdle = NEVER;
  ResetFirstRetry ();
  timer.Set (CRcHostTimerCallback, this);
  tLastAlive = NEVER;
  conThread = new CConThread ();
}


CRcHost::~CRcHost () {
  // Is this thread-safe?
  // -> Yes, if the net thread is completed (but not deleted) before any 'CRcHost' or 'CRcServer'
  //    objects are destroyed. Direct accesses can be made a) from the main thread (the one calling
  //    the constructor or destructor) or b) the net thread. All other threads (e.g. timer callbacks)
  //    must use 'CNetThread::AddTask' to delegate the work to the net thread. 'CNetThread::AddTask'
  //    itself is robust enough to ignore tasks if the thread is not running.
  timer.Clear ();
  conThread->Cancel ();
  // We should delete 'conThread' here, but do not do so to avoid waiting times when joining the thread.
#if WITH_CLEANMEM
  if (conThread->IsRunning ()) conThread->Join ();
  delete conThread;

  int n;
  while ( (n = resourceMap.Entries ()) > 0) resourceMap.Get (n - 1)->Unregister ();
#endif
}


void CRcHost::ClearResources () {
  CResource *rc;
  int n;

  Lock ();
  while ((n = resourceMap.Entries ()) > 0) {
    rc = resourceMap.Get (n - 1);
    Unlock ();      // 'rc->Unregister' will lock 'this' again
    rc->NotifySubscribers (rceDisconnected);
    rc->Unregister ();
    Lock ();
  }
  Unlock ();
}


CResource *CRcHost::GetResource (const char *rcLid, bool allowWait) {
  CResource *ret;
  TTicksMonotonic tWait;

  Lock ();
  ret = resourceMap.Get (rcLid);
  if (!ret && allowWait) {
    RequestConnect ();
    //~ INFO ("### try waiting");
    tWait = envNetTimeout;
    while (tWait > 0 && HostResourcesUnknown (state)) {
      //~ INFOF (("### waiting: state = %i", state));
      tWait = cond.Wait (&mutex, tWait);
    }
    ret = resourceMap.Get (rcLid);
  }
  Unlock ();
  return ret;
}


void CRcHost::RequestConnect () {
  //~ INFOF (("### CRcHost::RequestConnect ('%s')", Id ()));
  netThread.AddTask ((ENetOpcode) hnoSend, this);
}



// ***** Subscriptions *****


static const char *SubscribeCommand (CString *ret, CRcSubscriber *subscr, CResource *rc, char plusOrMinus) {
  //~ INFOF (("### SubscribeCommand: '%s'",  StringF (ret, "s%c %s %s", plusOrMinus, subscr->Lid (), rc->Lid ())));
  return StringF (ret, "s%c %s %s", plusOrMinus, subscr->Lid (), rc->Lid ());
}


void CRcHost::RemoteSubscribe (CRcSubscriber *subscr, CResource *rc) {
  CString s;
  Send (SubscribeCommand (&s, subscr, rc, '+'));
  //~ INFOF (("### Sent: '%s'", s.Get ()));
}


void CRcHost::RemoteUnsubscribe (CRcSubscriber *subscr, CResource *rc) {
  CString s;
  Send (SubscribeCommand (&s, subscr, rc, '-'));
  //~ INFOF (("### Sent: '%s'", s.Get ()));
}


static const char *RequestCommand (CString *ret, CResource *rc, const char *reqDef, char plusOrMinus) {
  return StringF (ret, "r%c %s %s", plusOrMinus, rc->Lid (), reqDef);
}


void CRcHost::RemoteSetRequest (CResource *rc, CRcRequest *req) {
  CString s1, s2;
  Send (RequestCommand (&s1, rc, req->ToStr (&s2, true, false, envRelTimeThreshold), '+'));
  //~ INFOF (("### RemoteSetRequest: '%s'", s1.Get ()));
}


void CRcHost::RemoteDelRequest (CResource *rc, const char *reqGid, TTicks t1) {
  CString s1, s2;
  if (t1) s2.SetF ("%s -%s", reqGid, TicksToString (&s1, t1, INT_MAX, true));
  else s2.SetC (reqGid);
  Send (RequestCommand (&s1, rc, s2.Get (), '-'));
}


bool CRcHost::RemoteInfoResource (CResource *rc, int verbosity, CString *retText) {
  CString s;
  return RemoteInfo (StringF (&s, "ir %s %i", rc->Lid (), verbosity), retText);
}


bool CRcHost::RemoteInfoSubscribers (int verbosity, CString *retText) {
  CString s;
  return RemoteInfo (StringF (&s, "is %i", verbosity), retText);
}



// ***** Helpers *****


bool CRcHost::CheckIfIdle () {
  int n;
  bool idle;

  Lock ();
  //~ INFOF(("###   state = %i, infoBusy = %i, execBusy = %i, sendBuf.IsEmpty() = %i", state, infoBusy, execBusy, sendBuf.IsEmpty ()));
  if (HostResourcesUnknown (state) || infoBusy || execBusy || !sendBuf.IsEmpty ()) idle = false;
  else {
    idle = true;
    //~ INFO("###   Simple cases indicate 'idle'.");
    for (n = 0; n < resourceMap.Entries (); n++)
      if (resourceMap.Get (n)->HasSubscribers ()) {
        idle = false;
        break;
      }
  }
  Unlock ();
  //~ INFOF (("### CheckIfIdle ('%s') -> %i", Id (), idle));
  return idle;
}


TTicksMonotonic CRcHost::ResetTimes (bool resetAge, bool resetRetry, bool resetIdle) {
  TTicksMonotonic tNow, tNext;

  tNow = TicksMonotonicNow ();

  // Reset the selected timers...
  if (resetAge) {
    tAge = tNow + envMaxAge;
    ATOMIC_WRITE (tLastAlive, TicksNow ());
  }
  if (resetRetry) {
    if (!tFirstRetry) tFirstRetry = tNow;
    tRetry = tNow + ((tNow >= tFirstRetry + envNetRetryDelay) ? envNetRetryDelay : envNetTimeout);
    //~ INFOF (("### resetRetry: tNow = %i, tRetry = %i", tNow, tRetry));
  }
  if (resetIdle) tIdle = tNow + envNetIdleTimeout;

  // Find the next time for a timer to trigger...
  tNext = tAge;
  if (!tNext || (tRetry && tRetry < tNext)) tNext = tRetry;
  if (!tNext || (tIdle && tIdle < tNext)) tNext = tIdle;

  // Update the timer object...
  if (tNext) timer.Reschedule (tNext);
  else timer.Clear ();
    // NOTE on race conditions: The timer might have been pending for another, previously scheduled time 'tPrev'.
    //   If 'tPrev' is earlier than 'tNext' or 'tNext == 0', this might cause that earlier event to be discarded.
    //   This is no problem here, since 'tNext' is for sure the next time by which an event must be triggered.
    //   Spurious events may still occur, the 'NetRun' method will take care of this.

  // Return...
  //~ INFOF (("### ResetTimes (%s): tNow = %i, tNext = %i, tAge = %i, tRetry = %i (%i), tIdle = %i", Id (), tNow, tNext, tAge, tRetry, tFirstRetry, tIdle));
  return tNow;
}


void CRcHost::OnFdReadable () {
  CString line, s;
  bool error;
  CResource *rc;
  CRcSubscriber *subscr;
  CRcValueState vs;
  char **argv;
  int k, num, argc;

  if (!receiveBuf.AppendFromFile (fd)) {
    //~ INFOF(("### CRcHost: EOF (fd = %i)", fd));
    netThread.AddTask ((ENetOpcode) hnoDisconnnect, this); // connection seems to be closed from peer -> disconnect ourself, too
  }

  while (receiveBuf.ReadLine (&line)) {
    DEBUGF (3, ("[resources] From server %s: '%s'", Id (), line.Get ()));

    // Interpret line...
    line.Strip ();
    error = false;
    argv = NULL;
    switch (line[0]) {

      case 'h':   // h <prog name> <version>          # connection ("hello") message
        ResetAgeTime ();
        break;

      case 'd':   // d <driver>/<rcname> <type> <rw>  # declaration of exported resource
        if (line[1] == '-') {
          // Unregister all resources...
          ClearResources ();
        }
        else if (line[1] == '.') {
          // No more declarations...
          if (state == hcsNewConnected) state = hcsConnected;
          cond.Broadcast ();            // wake up an eventual 'RemoteInfo' thread so that it can cancel
          ResetIdleTime ();
        }
        else {
          // Register new resource...
          line.Split (&argc, &argv, 3);
          if (argc != 3) {
            error = true;
            break;
          }
          s.SetF ("/host/%s/%s %s", Id (), argv[1], argv[2]);
          //~ INFOF(("### def = '%s'", s.Get ()));
          rc = CResource::Register (s.Get ());
          if (!rc) break;   // invalid resource description => ignore

          // (Re-)Submit all subscriptions ...
          //~ INFOF (("### (Re-)submitting subscribers of '%s'...", rc->Uri ()));
          num = rc->LockLocalSubscribers ();
          if (num > 0) {
            for (k = 0; k < num; k++) {
              subscr = rc->GetLocalSubscriber (k);
              //~ INFOF (("### Re-submit subscriber '%s'", subscr->Gid ()));
              sendBuf.Append (SubscribeCommand (&s, subscr, rc, '+'));
              sendBuf.Append ('\n');
            }
            netThread.AddTask ((ENetOpcode) hnoSend, this);   // Schedule a write-out
          }
          rc->UnlockLocalSubscribers ();
        }
        break;

      case 'v':   // v <driver>/<rcname> ?|([~]<value>) [<timestamp>]  # value/state changed
        //~ INFOF (("### Received: '%s'", line.Get ()));
        line.Split (&argc, &argv, 4);
        if (argc < 3) { error = true; break; }
        rc = GetRemoteResource (this, argv[1]);
        if (!rc) { error = true; break; }
        vs.SetType (rc->Type ());
        if (!vs.SetFromStrFast (argv[2])) { error = true; break; }
        //~ INFOF (("### line = '%s', argv[2] = '%s'", line.Get (), argv[2]));
        rc->ReportValueState (&vs);
        rc->NotifySubscribers (rceConnected);
        //~ INFOF (("### Received: '%s'", line.Get ()));
        //~ INFOF (("#   vs = '%s'", vs.ToStr (&s)));
        ResetAgeTime ();
        break;

      case 'i':   // i <text> | i.       # response to any 'i*' request | end of info
        Lock ();
        if (line[1] == '.') infoComplete = true;
        else {
          infoResponse.Append (line.Get () + 2);
          infoResponse.Append ('\n');
        }
        Unlock ();
        cond.Broadcast ();
        break;

      case 'e':   // e <text> | e.       # response to an 'e*' request (shell command) | end of the response
        Lock ();
        if (line[1] == '.') execComplete = true;
        else {
          execResponse.Append (line.Get () + 2);
          execResponse.Append ('\n');
        }
        Unlock ();
        cond.Broadcast ();
        break;

      default:
        error = true;
    }

    // Cleanup and post-processing...
    if (argv) {
      FREEP (argv[0]);
      free (argv);
    }
    if (error) SECURITYF (("Malformed message received from '%s' - ignoring: '%s'", Id (), line.Get ()));
  }
}


void CRcHost::OnFdWritable () {
  //~ INFOF (("### CRcHost::OnFdWritable ('%s')", Id ()));
  netThread.AddTask ((ENetOpcode) hnoSend, this);
}


void CRcHost::NetRun (ENetOpcode opcode, void *) {
  TTicksMonotonic tNow;
  int bytesToWrite, bytesWritten;
  bool doConnect, doDisconnect, resetIdleTime, resetRetryTime;
  CResource *rc;
  CString s1, s2, pending;
  int n;

  /* Rules to avoid race conditions
   *
   * 1. State transitions only occur in this method, and they must by completed within one invocation of this method.
   *
   * 2. The state 'hcsConnecting' must not by left as long as 'conThread' is running. The only exception is the
   *    operation 'noExit' to quickly exit the program (see comment there).
   *
   * 3. NetOps are sent asynchronously and may be received here in any order, even a very weird one (e.g.: 'hnoSend'
   *    -> 'hnoDisconnet' -> 'hnoSend'). For this reason, each operation must be executed correctly independent on the
   *    current state - 'ASSERT' statements are not allowed.
   *    An exception are the 'hnoCon*' operations, which are only allowed to be received in the 'hcsConnecting' state,
   *    which can never be left without any of these two operations.
   */

  DEBUGF (3, ("[resources] CRcHost::NetRun (%s, %i), state = %i", Id (), opcode, state));

  // Reset action flags (selected during opcode interpretation and executed afterwards)
  doConnect = doDisconnect = false;
  resetIdleTime = resetRetryTime = false;

  // PART A: Interpret opcode and select actions to perform...
  //   Smaller actions and state transitions may already be performed here.
  //   Joining and cancelling the connection thread ONLY happens here (starting is done in 'doConnect' action).
  switch ((int) opcode) {

    case noExit:
      //~ INFOF(("### 'conThread' cancelled for host '%s', old fd = %i", Id (), fd));
      conThread->Cancel ();
      // NOTE: 'conThread->Join ()' is intentionally not performed to avoid unnecessary waiting on program shutdown.
      //       This is ok, since if the connection thread is still running, the state will remain 'hcsConnecting', anyway.
      //       However, implementing an option to restart the net thread (presently not needed) would require some
      //       redesign here.
      break;

    case hnoSend:
      switch (state) {
        case hcsStandby:
        case hcsNewRetryWait:
        case hcsRetryWait:
          doConnect = true;     // Initiate a connection
          break;
        case hcsNewConnecting:
        case hcsConnecting:
          // Do nothing: When these states are exited, 'noSend' will be sent automatically again.
          break;
        case hcsNewConnected:
        case hcsConnected:
          // Do nothing: Pending data will be sent automatically in this state (see below).
          break;
      };
      break;

    case hnoDisconnnect:
      switch (state) {
        case hcsStandby:
        case hcsNewRetryWait:
        case hcsRetryWait:
          // Do nothing (already disconnected).
          // The operation has no effect on eventual retries ('hcsRetryWait' state is not left).
          break;
        case hcsNewConnected:
        case hcsConnected:
          doDisconnect = true;
          break;
        case hcsNewConnecting:
        case hcsConnecting:
          // Solution 1: Stay in 'connecting' state (ignoring disconnect request - probably safe, but not propper).
          resetIdleTime = true;   // Enable idle timeout (to partially compensate for the formal incorrectness).
          break;
          //// Solution 2: Cancel connection process properly - formally correct, but may block the thread for a long time.
          //conThread.Cancel ();  //
          //conThread.Join ();    // NOTE: This call may block and delay the whole net thread.
          //doDisconnect = true;
          //break;
      };
      break;

    case hnoTimer:
      // Do nothing: Timers will be checked in any case below.
      break;

    case hnoConSuccess:
      ASSERT (state == hcsConnecting || state == hcsNewConnecting);  // see rules above
      //~ INFOF(("### 'conThread' completed for host '%s', fd = %i", Id (), fd));

      // Complete connection...
      conThread->Join ();
      fd = conThread->Fd ();
      ResetFirstRetry ();
      resetIdleTime = true;

      // Reset info & exec flags...
      infoBusy = infoComplete = false;
      infoResponse.Clear ();
      execBusy = execComplete = false;
      execResponse.Clear ();

      // Done...
      state = HostResourcesUnknown (state) ? hcsNewConnected : hcsConnected;
      break;

    case hnoConFailed:
      ASSERT (state == hcsConnecting || state == hcsNewConnecting);  // see rules above
      //~ INFOF(("### 'conThread' failed for host '%s': %s", Id (), conThread->ErrorString ()));

      conThread->Join ();
      state = HostResourcesUnknown (state) ? hcsNewRetryWait : hcsRetryWait;
      resetRetryTime = true;
      break;
  };

  // PART B: Actions and state transitions...

  // Action: Connect...
  if (doConnect) {
    ASSERT (!conThread->IsRunning ());
    //~ INFOF(("### 'conThread' started for host '%s', old fd = %i, state == %i", Id (), fd, state));
    conThread->Start (this);
    state = HostResourcesUnknown (state) ? hcsNewConnecting : hcsConnecting;
    resetIdleTime = true;
  }

  // Action: Disconnect...
  if (doDisconnect) {
    ASSERT (!conThread->IsRunning ());
    //~ INFOF(("### Disconnect for host '%s', fd = %i -> -1", Id (), fd));
    close (fd);
    fd = -1;
    Lock ();
    sendBuf.Clear ();     // clear send buffer (we are unable to send this anymore)

    // Submit disconnect event to subscribers and invalidate all resources ...
    for (n = 0; n < resourceMap.Entries (); n++) {
      rc = resourceMap.Get (n);
      rc->NotifySubscribers (rceDisconnected);
      rc->ReportNetLost ();
    }
    Unlock ();

    // Set next state...
    state = HostResourcesUnknown (state) ? hcsNewRetryWait
            : CheckIfIdle () ? hcsStandby
            : hcsRetryWait;
    resetRetryTime = true;
    cond.Broadcast ();            // wake up an eventual 'RemoteInfo' thread so that it can cancel
  }

  // PART C: Send pending data, if possible ...
  if (state == hcsConnected || state == hcsNewConnected) {
    Lock ();
    if (!sendBuf.IsEmpty ()) {          // Anything to send?
      // Write 'sendBuf' to socket...
      DEBUGF (3, ("[resources] Sending to server '%s':\n%s", Id (), sendBuf.Get ()));
      bytesToWrite = sendBuf.Len ();
      bytesWritten = write (fd, sendBuf.Get (), bytesToWrite);
      if (bytesWritten == bytesToWrite) sendBuf.Clear ();
      else {
        // Could not write everything...
        if (bytesWritten >= 0) DEBUGF (3, ("[resources]   ... written %i out of %i bytes.", bytesWritten, bytesToWrite));
        else {
          if (errno == EAGAIN || errno == EWOULDBLOCK) DEBUGF (3, ("[resources]   ... would block."));
          else DEBUGF (3, ("[resources]   ... error: %s", strerror (errno)));
        }
        sendBuf.Del (0, bytesWritten);
      }
      resetIdleTime = true;
    }
    sendBufEmpty = sendBuf.IsEmpty ();
    Unlock ();
  }

  // PART D: Handle timers...

  // Update all times and timer...
  if (state != hcsRetryWait && state != hcsNewRetryWait) {    // retries can only be initiated in these states...
    // Clear and disable the retry timer...
    tRetry = NEVER;
    resetRetryTime = false;
  }
  if (state != hcsConnected) {    // idle disconnects can only be initiated in this state...
    // Clear and disable the idle timer...
    tIdle = NEVER;
    resetIdleTime = false;
  }
  tNow = ResetTimes (false, resetRetryTime, resetIdleTime);
  // Note: Whether an action is applicable can be decided by the 't...' time variables now.

  // Check & handle age timeout ...
  if (tAge != NEVER && tNow >= tAge) {
    //~ INFOF (("### Age timout on host '%s' (tNow = %i, tAge = %i)", Id (), (int) tNow, (int) tAge));
    // Acknowledge / disable age time,,,
    tAge = NEVER;
    UpdateTimer ();
    // Schedule 'hnoDisconnect'...
    netThread.AddTask ((ENetOpcode) hnoDisconnnect, this);
  }

  // Check & handle retry timeout ...
  if (tRetry != NEVER && tNow >= tRetry) {
    //~ INFOF (("### Retry timout on host '%s' (tNow = %i, tRetry = %i)", Id (), (int) tNow, (int) tRetry));
    // Acknowledge / disable retry time,,,
    tRetry = NEVER;
    UpdateTimer ();
    //~ INFOF (("### Retry timeout ('%s')", Id ()));
    // Schedule a connection attempt now...
    netThread.AddTask ((ENetOpcode) hnoSend, this);
  }

  // Check & handle idle timeout ...
  if (tIdle != NEVER && tNow >= tIdle) {
    //~ INFOF (("### Idle check for host '%s' (tNow = %i, tIdle = %i)", Id (), (int) tNow, (int) tIdle));
    if (CheckIfIdle ()) {     // Really idle? Yes...
      // Acknowledge / disable idle time,,,
      tIdle = NEVER;
      UpdateTimer ();
      // Schedule 'hnoDisconnect'...
      netThread.AddTask ((ENetOpcode) hnoDisconnnect, this);
    }
    else ResetIdleTime ();    // No: Try again later
  }
}


void CRcHost::GetInfo (CString *ret, int verbosity) {
  static const char *stateFormats [] = {
    "New, connecting...\n",         // hcsNewConnecting
    "New, retrying, at %s: %s\n",   // hcsNewRetryWait
    "New, connected (since %s)\n",  // hcsNewConnected
    "Connecting...\n",              // hcsConnecting
    "Retrying, at %s: %s\n",        // hcsRetryWait
    "OK, connected (since %s)\n",   // hcsConnected
    "OK, standby (since %s)\n"      // hcsStandby
  };
  CString info;
  EHostConnectionState _state;
  const char *stateFormat;
  bool haveInfo;

  Lock ();
  conThread->Lock ();
  ret->SetF ("%-16s(%18s): ", Id (), conThread->AdrString ());
  _state = state;
    // Note: Access to 'state' is not synchronized by a mutex and may be inaccurate!
    //       We copy it to a local variable here.
  stateFormat = stateFormats[_state];
  if (_state == hcsNewRetryWait && TicksIsNever (conThread->LastAttempt ()))
    // There is a special case to consider: In the construction, the state
    // is initialized with 'hcsNewRetryWait' to initiate a new connection soon.
    // Since no valid timestamp and no error string is available, we replace
    // the format string and do not show misleading time/error strings.
    stateFormat = "New, trying...\n";
  ret->AppendF (stateFormat, TicksToString (conThread->LastAttempt (), 0), conThread->ErrorString ());
  conThread->Unlock ();
  Unlock ();

  if (verbosity >= 1) {
    haveInfo = false;
    if (!HostResourcesUnknown (state)) haveInfo = RemoteInfoSubscribers (verbosity - 1, &info);
    if (haveInfo) ret->AppendFByLine ("  %s\n", info.Get ());
    else ret->Append ("  (host unreachable)\n");
  }
}


void CRcHost::PrintInfo (FILE *f, int verbosity) {
  CString s;

  GetInfo (&s, verbosity);
  fprintf (f, "%s", s.Get ());
}


void CRcHost::PrintInfoAll (FILE *f, int verbosity) {
  for (int n = 0; n < hostMap.Entries (); n++) hostMap.Get (n)->PrintInfo (f, verbosity);
}



// ***** CShell methods *****


bool CRcHost::Start (const char *cmd, bool readStdErr) {
  ASSERTM (false, "'CShell::Start ()' cannot be called for a remote command.");
}


bool CRcHost::StartRestricted (const char *name, const char *args) {
  CString s;
  TTicksMonotonic tWait;

  // Preamble...
  tWait = envNetTimeout;
  Lock ();

  // Wait until channel is available, acquire channel by setting 'infoBusy'...
  while (execBusy) {
    tWait = cond.Wait (&mutex, tWait);
    if (tWait < 0) {
      Unlock ();
      WARNINGF (("Timeout when waiting for exec channel to host '%s'", Id ()));
      return false;
    }
  }
  execBusy = true;      // now _we_ make the channel busy

  // Submit command...
  execComplete = false;
  SendAL (StringF (&s, args ? "ec %s %s" : "ec %s", name, args));

  return true;
}


void CRcHost::Wait () {
  WriteClose ();
  Lock ();
  while (!execComplete && state == hcsConnected) cond.Wait (&mutex, envNetTimeout);
    // Note: This is very critical, so that we may wait longer than 'envNetTimeout' in general
  execBusy = false;
  Unlock ();
}


void CRcHost::CheckIO (bool *canWrite, bool *canRead, TTicksMonotonic timeOut) {
  bool _canWrite, _canRead;

  _canWrite = _canRead = false;
  Lock ();
  if (execBusy && ((canWrite && !execWriteClosed) || (canRead && !execComplete))) {   // waiting makes sense?
    while (!_canWrite && !_canRead && timeOut >= 0) {
      if (canWrite && !execWriteClosed) _canWrite = true;
      if (canRead && !execResponse.IsEmpty ()) _canRead = true;
      if (!_canWrite && !_canWrite) timeOut = cond.Wait (&mutex, timeOut);
    }
  }
  Unlock ();
  if (canWrite) *canWrite = _canWrite;
  if (canRead) *canRead = _canRead;
}


void CRcHost::WriteLine (const char *line) {
  CString s;
  Send (StringF (&s, "e %s", line));
}


void CRcHost::WriteClose () {
  Lock ();
  SendAL ("e.");
  execWriteClosed = true;
  Unlock ();
}


bool CRcHost::ReadLine (CString *str) {
  bool success;

  Lock ();
  success = execResponse.ReadLine (str);
  Unlock ();
  return success;
}



// ***** Helpers *****


void CRcHost::Send (const char *line) {
  Lock ();
  SendAL (line);
  Unlock ();
}


void CRcHost::SendAL (const char *line) {
  sendBuf.Append (line);
  sendBuf.Append ('\n');
  sendBufEmpty = false;
  //~ INFOF (("### CRcHost::SendAL ('%s', '%s')", Id (), line));
  netThread.AddTask ((ENetOpcode) hnoSend, this);   // eventually trigger to (re-)connect
}


bool CRcHost::RemoteInfo (const char *msg, CString *ret) {
  CString line;
  TTicksMonotonic tWait;

  //~ INFOF(("### CRcHost::RemoteInfo (%s, '%s')", Id (), msg));

  // Preamble...
  tWait = envNetTimeout;
  Lock ();

  // Wait until channel is available, acquire channel by setting 'infoBusy'...
  while (infoBusy) {
    //~ INFOF(("# pre-wait tWait = %i", tWait));
    tWait = cond.Wait (&mutex, tWait);
    if (tWait < 0) {
      Unlock ();
      WARNINGF (("Timeout when waiting for info channel to host '%s'", Id ()));
      return false;
    }
  }
  //~ INFO("## no more busy");
  infoBusy = true;      // now WE make the channel busy

  // Submit command...
  infoComplete = false;
  SendAL (msg);

  // Receive complete response...
  while (!infoComplete) {
    //~ INFOF(("# exec-wait tWait = %i", tWait));
    if (tWait < 0) {
      Unlock ();
      WARNINGF (("Timeout when waiting for info response from host '%s'", Id ()));
      if (state == hcsConnected || state == hcsNewConnected) netThread.AddTask ((ENetOpcode) hnoDisconnnect, this);
      infoBusy = false;
      return false;
    }
    tWait = cond.Wait (&mutex, tWait);
  }
  //~ INFO("## done");

  // Done...
  ret->SetO (infoResponse.Disown ());
  infoResponse.Clear ();
  infoBusy = false;
  Unlock ();
  return true;
}





// *************************** Initialization **********************************


void RcSetupNetworking (bool enableServer) {
  CString s;
  char buf[64];
  struct sockaddr_in sa;
  int maskBits, ret;
  bool ok;

  // Enable/disable server ...
  serverEnabled = (envServerEnabled && enableServer);
  DEBUGF(1, ("Server %sabled by configuration%s %sabled by tool.",
             envServerEnabled ? "en" : "dis",
             envServerEnabled == enableServer ? " and" : ", but",
             enableServer ? "en" : "dis"));
  if (enableServer && !envServerEnabled)
    DEBUGF (1, ("Set '%s = 1' to enable the server (currently disabled).", envServerEnabledKey));
      // Debug level, not info to avoid unwanted output by the shell, where the server is
      // typically, but not always disabled.

  // Served interface(s)...
  //~ INFOF (("### envServeInterfaceStr = %s", envServeInterfaceStr));
  if (strcmp (envServeInterfaceStr, "any") == 0) envServeInterface = htonl (INADDR_ANY);
  else if (strcmp (envServeInterfaceStr, "local") == 0) envServeInterface = htonl (INADDR_LOOPBACK);
  else if ( (ret = inet_pton (AF_INET, envServeInterfaceStr, &(sa.sin_addr))) == 1) {
    envServeInterface = sa.sin_addr.s_addr;
  }
  else ERRORF (("Illegal syntax in '%s'", envServeInterfaceStrKey));

  // Allowed clients...
  //~ if (!EnvGet (envNetworkStrKey)) // Warn on potentially unaware network setting...
    //~ WARNINGF(("'%s' is not set, using the default of %s", envNetworkStrKey, envNetworkStr));
  ok = true;
  if ( (ret = sscanf (envNetworkStr, "%63[^/]/%i", buf, &maskBits)) != 2) {
    //~ INFOF(("### sscanf -> %i, buf = '%s'", ret, buf));
    ok = false;
  }
  else if ( (ret = inet_pton (AF_INET, buf, &(sa.sin_addr))) != 1) {
    //~ INFOF(("### inet_pton -> %i", ret));
    ok = false;
  }
  if (!ok) ERRORF(("Illegal syntax in '%s'", envNetworkStrKey));
  envNetwork = sa.sin_addr.s_addr;
  envNetworkMask = htonl (-(1 << (32-maskBits)));
}


// Helper for 'RcReadConfig()'...
static const char *AddHost (const char *id, const char *desc, int defaultPort) {
  CRcHost *host;
  CString s, sDesc, netHost;
  const char *netInstance, *netHostAndPort;
  int netPort;
  bool netHostIsLocal;
  char *p;

  //~ INFOF (("### AddHost ('%s'), localHostId = '%s'", id, localHostId.Get ()));

  // Determine 'netInstance' and resolved 'netHost', 'netPort' ...
  sDesc.Set (desc ? desc : id);
  p = (char *) strchr (sDesc.Get (), '@');    // Extract instance, if present...
  if (p) {
    *(p++) = '\0';
    netInstance = sDesc.Get ();
    netHostAndPort = p;
  }
  else {
    netInstance = NULL;
    netHostAndPort = sDesc.Get ();
  }
  if (!EnvNetResolve (netHostAndPort, &netHost, &netPort, defaultPort))
    return "Unresolvable host/port";
  if (netPort < 0) return "Unspecified network port";

  // Check, if the host maps to the local host ...
  netHostIsLocal = false;
  if (strcmp (netHost.Get (), "localhost") == 0)    // machine name is explicitly "localhost"?
    netHostIsLocal = true;
  else if (EnvNetResolve (EnvMachineName (), &s))   // resolve machine name
    if (strcmp (netHost.Get (), s.Get ()) == 0) {   // 'netHost' matches (resolved) machine name?
      netHostIsLocal = true;
      if (RESOLVE_LOCALHOST) netHost.SetC ("localhost");
        // Explicitly map to 'localhost', so that we can reach this host via the
        // local network interface, even if external network interfaces are disabled.
    }

  //~ INFOF (("### hostID = '%s', instance = %s', netHost = '%s', netPort = %i", id, desc, netHost, netPort));

  // Check for duplicates as foreign hosts ...
  if (hostMap.Find (id) >= 0) {
    if (desc) return "Redefined host (qualified host declarations must appear before implicit ones)";
    return NULL;    // simple occurence is known => ignore
  }
  if (localHostId.Compare (id) == 0) return NULL;

  // Check if we hit the local instance...
  if (serverEnabled && netHostIsLocal)
    if (!netInstance || strcmp (netInstance, EnvInstanceName ()) == 0) {  // matching instance name?
      //~ INFO ("###   HIT!");

      // Hit: It is our host...
      if (localPort < 0) {    // only accept the first match, ignore others
        //~ INFO ("###   Accepted.");
        localHostId.Set (id);
        localPort = netPort;
        DEBUGF (1, ("Identified myself as local server host '%s' = %s:%i", id, netHost.Get (), netPort));
        return NULL;
      }
      else {
        if (desc) return "Redefined local host (qualified declarations must appear before implicit ones)";
        return NULL;
      }
    }

  // Add to map ...
  DEBUGF (1, ("Adding remote host '%s' = %s:%i", id, netHost.Get (), netPort));
  host = new CRcHost ();
  host->Init (id, netHost, netPort);
  hostMap.Set (id, host);

  // Done...
  return NULL;
}


void RcReadConfig (CString *retSignals) {
  CString str;
  const char *fileName, *errStr;
  FILE *f;
  char buf[256], *p, *q, **argv;
  int argc, defaultPort;
  CRcValueState val;
  bool error;

  // Default config file...
  if (!envRcConfigFile) f = NULL;
  else if (!envRcConfigFile[0]) f = NULL;
  else {
    fileName = EnvGetHome2lEtcPath (&str, envRcConfigFile);
    ASSERT (fileName != NULL);
    f = fopen (fileName, "rt");
    if (!f) WARNINGF (("Failed to read file '%s': %s", fileName, strerror (errno)));
  }
  if (f) {

    // Main parsing loop...
    defaultPort = -1;
    error = false;
    errStr = NULL;
    buf[0] = '\0';
    while (!feof (f) && !error) {
      fgets (buf, sizeof (buf), f);
      //~ INFOF (("### Parsing '%s'", buf));
      p = strchr (buf, '#');    // remove comments...
      if (p) p[0] = '\0';
      StringSplit (buf, &argc, &argv);
      if (argc > 0) {
        switch (toupper (argv[0][0])) {

          case 'P':   // Default port ...
            if (argc != 2) error  = true;
            else {
              defaultPort = strtol (argv[1], &p, 0);
              if (*p != '\0') error  = true;
            }
            break;

          case 'H':   // Host ...
            // Syntax: H <host id>
            if (argc < 2 || argc > 3) error = true;
            else errStr = AddHost (argv[1], argc == 3 ? argv[2] : NULL, defaultPort);
            if (errStr) error = true;
            break;

          case 'A':   // Alias ...
            // Syntax: A <name> <target>
            if (argc != 3) error = true;
            else {
              GetAbsPath (&str, argv[2], "/host");
              aliasMap.Set (argv[1], &str);
              if (strncmp (str.Get (), "/host/", 6) == 0) {
                // Auto-add host...
                p = q = (char *) str.Get () + 6;
                while (q[0] && q[0] != '/') q++;
                q[0] = '\0';
                errStr = AddHost (p, NULL, defaultPort);
                if (errStr) error = true;
              }
            }
            break;

          case 'S':   // Signal...
            // Syntax: S <host id> <name> <type> [<default value>]
            if (argc < 4 || argc > 5) { error = true; break; }
            // Store signal...
            retSignals->AppendF ("%s %s %s %s\n", argv[1], argv[2], argv[3], argc > 4 ? argv[4] : "?");
            // Auto-add host...
            errStr = AddHost (argv[1], NULL, defaultPort);  // auto-add other host
            if (errStr) error = true;
            break;

          default:
            error = true;
        }
      }

      // Cleanup...
      if (error) ERRORF (("%s in file '%s': %s", errStr ? errStr : "Invalid line", fileName, buf));
      if (argv) {
        FREEP (argv[0]);
        free (argv);
      }
    }
    fclose (f);
  }

  // Check if we found ourselves in the host map and report whether and how we start a server ...
  //~ INFOF (("### serverEnabled = %i,  localPort = %i", (int) serverEnabled, localPort));
  if (serverEnabled && localPort < 0) {
    WARNINGF (("Could not identify myself in '%s' - disabling server.", envRcConfigFile));
    serverEnabled = false;
  }
  //~ if (serverEnabled) INFOF (("Starting server '%s' listening on port %i.", localHostId.Get (), localPort));

  // Set local host ID for clients...
  if (localPort < 0) localHostId.SetF ("%s<%s:%i>", EnvMachineName (), EnvInstanceName (), EnvPid ());

  //~ INFOF(("### localHostId = '%s'", localHostId.Get ()));
}


void RcRegisterConfigSignals (CString *signals) {
  CSplitString lineSet, args;
  CRcValueState vs;
  int n;

  lineSet.Set (signals->Get (), INT_MAX, "\n");
  for (n = 0; n < lineSet.Entries (); n++) {
    // Syntax: <host id> <name> <type> <default value or '?' if none>
    args.Set (lineSet [n], 5);
    ASSERT (args.Entries () == 4);
    if (localHostId.Compare (args[0]) == 0) {    // for me?
      vs.SetType (RcTypeGetFromName (args[2]));
      if (vs.Type () == rctNone) {
        WARNINGF(("Ignoring invalid signal definition (type error): 'S %s'", lineSet[n]));
      }
      else if (!vs.SetFromStr (args[3])) {
        WARNINGF(("Ignoring invalid signal definition (value error): 'S %s'", lineSet[n]));
      }
      else {
        RcDriversAddSignal (args[1], &vs);
      }
    }
  }
}
