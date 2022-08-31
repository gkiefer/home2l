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

TTicks RcNetTimeout () { return (TTicks) envNetTimeout; }





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

CDictCompact<CString> aliasMap;

CMutex unregisteredResourceMapMutex;
CDictRef<CResource> unregisteredResourceMap;





// *************************** URI Path Handling *******************************


#define RC_MAX_ALIAS_DEPTH 8            // maximum number of redirections when resolving aliases


const char *pathRootNames [] = { "alias", "host", "local" };
ERcPathDomain pathRootDomains [] = { rcpAlias, rcpHost, rcpLocal };



// ***** Path Normalization and Resolution *****


const char *RcPathNormalize (CString *ret, const char *uri, const char *workDir) {
  if (!workDir) workDir = "/alias";
  if (uri[0] == '/') ret->SetC (uri);
  else ret->SetF ("%s/%s", workDir, uri);
  ret->PathNormalize ();
  return ret->Get ();
}


static void PrepareAliasMap () {          // Helper for 'RcReadConfig()'...
  CKeySet unresolved;
  CString s, *value;
  const char *key, *key0, *key1;
  int key0len, key1len, n;
  bool resolvedOne;

  // Check for aliases where one is a prefix of another...
  //   In this case, the other will be overloaded by the first and must be deleted with a warning.
  //
  //   Note: It is not possible so support it with reasonable effort, as this example shows:
  //     A first         /host/a/1    ; resources /host/a/1/x/wow, /host/a/1/y exist
  //     A first/second  /host/b      ; resources /host/b/2/x, /host/b/2/y exist
  //     A third         /alias/first -> /host/a/1
  //     => Directory of "/alias/first":        x y second
  //     => Directory of "/alias/first/second": 2
  //     => Directory of "/alias/third/second": 2  (A)
  //
  //   Case (A) cannot be processed correctly if aliases are pre-resolved ("third" -> "/host/a/1").
  //   Instead, in each indidual redirection step, a check must be performed. This would make directory
  //   discovery much more complex than it is implemented now.
  //
  key0 = aliasMap.GetKey (0);
  key0len = strlen (key0);
  n = 1;
  while (n < aliasMap.Entries ()) {
    key1 = aliasMap.GetKey (n);
    key1len = strlen (key1);
    if (strncmp (key0, key1, MIN (key0len, key1len)) == 0 && (key1[key0len] == '/' || key0[key0len] == '/')) {
      ASSERT (key0len < key1len);   // we assert that prefixes are sorted before the longer string (see strcmp(3) ).
      WARNINGF (("Alias '%s' is invisible behind '%s' and has no effect.", key1, key0));
      aliasMap.Del (n);
    }
    else {
      n++;
      key0 = key1;
      key0len = key1len;
    }
  }

  // Normalize targets ...
  for (n = 0; n < aliasMap.Entries (); n++) {
    value = aliasMap.Get (n);
    s.Set (*value);
    RcPathNormalize (value, s);
  }

  // Pre-resolve targets ...
  // a) Collect all indirect references ...
  for (n = 0; n < aliasMap.Entries (); n++) {
    value = aliasMap.Get (n);
    if (RcPathGetRootDomain (value->Get ()) != rcpHost) {
      //~ INFOF (("  unresolved (domain = %i): '%s -> %s'", RcPathGetRootDomain (value->Get ()), aliasMap.GetKey (n), value->Get ()));
      unresolved.Set (aliasMap.GetKey (n));
    }
  }
  // b) Iteratively try to resolve them ...
  resolvedOne = true;
  while (resolvedOne) {
    resolvedOne = false;
    for (n = unresolved.Entries () - 1; n >= 0; n--) {
      //~ const char *key = unresolved[n];
      //~ value = aliasMap.Get (key);
      //~ INFOF (("### n = %i/%i, key = '%s', value = '%s'", n, unresolved.Entries (), key, value ? value->Get () : NULL));
      value = aliasMap.Get (unresolved[n]);
      s.Set (value->Get ());
      RcPathResolve (value, s);
      if (RcPathGetRootDomain (value->Get ()) == rcpHost) {
        // Success!
        unresolved.Del (n);
        resolvedOne = true;
        //~ unresolved.Dump ();
      }
    }
  }
  // c) Warn and dismiss unresolvables ...
  for (n = unresolved.Entries () - 1; n >= 0; n--) {
    key = unresolved[n];
    value = aliasMap.Get (key);
    WARNINGF (("Unable to resolve alias: '%s' -> '%s'", key, value->Get ()));
    aliasMap.Del (key);
  }

  //~ for (n = 0; n < aliasMap.Entries (); n++)
    //~ INFOF (("  %s -> %s", aliasMap.GetKey (n), aliasMap.Get (n)->Get ()));
}


const char *RcPathResolve (CString *ret, const char *uri, const char *workDir, const char **retTarget, const char **retLocalPath) {
  CString absUri, aliasPart, *s;
  const char *p, *target;
  char *q;
  int len;

  // Preset return values and normalize input ...
  if (!retTarget && !retLocalPath) uri = RcPathNormalize (&absUri, uri);
  if (retTarget) *retTarget = NULL;
  ret->Set (uri);

  // Check root domain...
  switch (RcPathGetRootDomain (uri)) {

    case rcpNone:
    case rcpHost:

      // Syntax error or host domain: Return unmodified path ...
      ret->Set (uri);
      break;

    case rcpLocal:

      // Handle path in "local" domain...
      for (p = uri + 2; p[0] != '\0' && p[-1] != '/'; p++);   // move 'p' to start of next component
      ret->SetF ("/host/%s/%s", localHostId.Get (), p);
      break;

    case rcpAlias:

      // Handle alias...
      for (p = uri + 2; p[0] != '\0' && p[-1] != '/'; p++);   // move 'p' to start of next component
      if (!p[0]) {
        // No second path component: Skip (e.g. URI = "/alias/") ...
        //~ INFOF (("#   no second path component -> '%s'", uri));
        ret->Set (uri);
        break;
      }

      // Try to match sub-paths, start with the longest...
      aliasPart.Set (p);
      while (true) {
        //~ INFOF (("#   looking up '%s'", aliasPart.Get ()));
        s = aliasMap.Get (aliasPart.Get ());
        if (s) {          // Found alias...
          target = s->Get ();
          len = aliasPart.Len ();
          ret->SetC (target);        // alias target
          ret->Append (p + len);     // ... + local part
          if (retTarget) *retTarget = target;
          if (retLocalPath) *retLocalPath = p + len;
          break;
        }
        else {            // Sub-path not found: Cut off last path component and search again...
          q = strrchr (aliasPart, '/');
          if (!q) {   // no more components...
            //~ INFOF (("   no matching alias found -> '%s'", uri));
            ret->Set (uri);
            break;
          }
          else q[0] = '\0';
        }
      } // while (true);
      break;

    default:
      ASSERT (false);

  }

  // Done...
  //~ INFOF(("### RcPathResolve ('%s') -> '%s'", uri, ret->Get ()));
  return ret->Get ();
}





// ***** Path Analysis *****


static inline ERcPathDomain DoGetRootDomain (const char *str, int len) {
  const char *key;
  int n;

  // Search for keyword ...
  for (n = 0; n < ENTRIES (pathRootNames); n++)
    if (str[0] == (key = pathRootNames[n]) [0]) {       // quick pre-check for initial character
      if (len == (int) strlen (key))                    // pre-check length
        if (strncmp (str, key, len) == 0)  return pathRootDomains[n];
    }

  // Not found ...
  return rcpNone;
}


ERcPathDomain RcPathGetRootDomain (const char *uri) {
  const char *p;
  int len;

  // Sanity ...
  if (!uri) return rcpNone;
  if (uri[0] != '/') return rcpNone;

  // Get length ...
  p = strchr (uri + 1, '/');
  if (p) len = p - (uri + 1);
  else len = strlen (uri + 1);

  // Go ahead ...
  return DoGetRootDomain (uri + 1, len);
}


ERcPathAnalysisState RcPathAnalyse (const char *uri, TRcPathInfo *ret, bool allowWait) {
  ERcPathAnalysisState state;
  CString s;
  const char *p, *q;

  //~ INFOF (("### RcPathAnalyse ('%s') ...", uri));

  // Set default return values...
  RcPathInfoClear (ret);
  ret->localPath = uri;

  // Sanity checks...
  ASSERT (ret != NULL);
  if (!uri) { state = rcaNone; goto done; }
  if (uri[0] != '/') { state = rcaNone; goto done; }

  // Check root (level-0) component...
  //   From now on, 'p' points to the start of the current path component, and 'q' points to its end.
  p = q = uri + 1;
  while (*q && *q != '/') q++;
  //~ INFOF (("###   level-0: p = '%s', q = '%s'", p, q));
  if (*q != '/') {
    // No trailing slash => leave it with root state...
    ret->localPath = p;
    state = rcaRoot;
    goto done;
  }

  // Determine domain ...
  switch ( (ret->domain = DoGetRootDomain (p, q - p)) ) {
    case rcpHost:   state = rcaHost;   break;
    case rcpLocal:  state = rcaDriver; break;
    case rcpAlias:  state = rcaAlias;  break;
    default:        state = rcaNone;   goto done;
  }
  //~ INFOF (("###   state (level-0): %i", state));

  // Move next (level-1) component...
  p = (++q);
  while (*q && *q != '/') q++;

  // Try to further evaluate host path...
  if (state == rcaHost && *q == '/') {
    //~ INFOF (("RcPathAnalyse ('%s'): state == rcaHost ...", uri));
    s.Set (p, q - p);
    //~ INFOF(("  comparing '%s' with '%s'...", comp.Get (), localHostId.Get ()));
    if (s.Compare (localHostId.Get ()) == 0)
      state = rcaDriver;     // local host
    else {
      state = rcaResource;   // remote host
      ret->host = hostMap.Get (s.Get ());
    }

    // Move on to next path component...
    p = (++q);
    while (*q && *q != '/') q++;
  }

  // Try to further evaluate driver path...
  if (state == rcaDriver && *q == '/') {
    s.Set (p, q-p);
    state = rcaResource;
    ret->driver = driverMap.Get (s.Get ());

    // Move on to next path component...
    p = (++q);
    while (*q && *q != '/') q++;
  }

  // Now 'p' is the local path...
  ret->localPath = p;

  // Try to identify the resource ...
  //~ INFOF (("p = '%s'", p));
  if (state == rcaResource) {
    // Resource: Try to determine resource object ...
    if (ret->driver) ret->resource = ret->driver->GetResource (ret->localPath);
    else if (ret->host) ret->resource = ret->host->GetResource (ret->localPath, ret ? allowWait : false);
  }

  // And finally: Resolve aliases! ...
  else if (state == rcaAlias) {
    // Try to resolve alias ...
    RcPathResolve (&s, uri, NULL, &ret->target, &ret->localPath);
    //~ INFOF (("### Resolved alias: '%s' -> '%s' = '%s' + '%s'", uri, s.Get (), ret->target, ret->localPath));
    if (ret->target) state = rcaAliasResolved;
  }

done:
  //~ INFOF(("### Analyse '%s': state = %i, localPath = '%s', host = %08x, driver = %08x", uri, state, _retLocalPath, _retHost, _retDriver));
  ret->state = state;
  return state;
}


bool RcPathGetDirectory (const char *uri, CKeySet *ret, bool *retExists, CString *retPrefix, bool allowWait) {
  TRcPathInfo info;
  CString s, _prefix, *prefix, uriResolved;
  CResource *rc;
  int n, localOffset, idx0, idx1, items;
  bool ok, dirExists;

  // Sanity ...
  ASSERT (uri != NULL);
  if (uri[0] != '/') return false;
  dirExists = false;

  // Prepare URI and prefix ...
  prefix = retPrefix ? retPrefix : &_prefix;
  prefix->SetC (uri);
  prefix->Append ('/');
  prefix->PathNormalize ();

  //~ INFOF (("### RcPathGetDirectory ('%s' -> '%s') ...", uri, prefix->Get ()));

  // Analyse URI ...
  RcPathAnalyse (prefix->Get (), &info, allowWait);
  //~ INFOF (("###   Analysis: state = %i, localPath = '%s'", info.state, info.localPath));

  // Handle various states ...
  if (ret) ret->Clear ();
  ok = true;
  switch (info.state) {

    case rcaRoot:
      if (ret) for (n = 0; n < ENTRIES (pathRootNames); n++)
        ret->Set (StringF (&s, "%s/", pathRootNames[n]));
      dirExists = true;
      break;

    case rcaHost:
      if (ret) for (n = 0; n < hostMap.Entries (); n++)
        ret->Set (StringF (&s, "%s/", hostMap.GetKey (n)));
      ret->Set (StringF (&s, "%s/", localHostId.Get ()));
      dirExists = true;
      break;

    case rcaDriver:
      if (ret) for (n = 0; n < driverMap.Entries (); n++)
        ret->Set (StringF (&s, "%s/", driverMap.GetKey (n)));
      dirExists = true;
      break;

    case rcaResource:
      // Lock host or driver resources, respectively ...
      items = info.host ? info.host->LockResources () : info.driver ? info.driver->LockResources () : 0;
      //~ INFOF (("###   state 'resource': host = %s, driver = %s, items = %i", info.host ? info.host->ToStr () : "-", info.driver ? info.driver->ToStr () : "-", items));
      for (n = 0; n < items; n++) {
        // Get host or driver resource, respectively (one of the is always != NULL if we get here) ...
        rc = info.host ? info.host->GetResource (n) : info.driver->GetResource (n);
        localOffset = strlen (info.localPath);
        if (strncmp (rc->Lid (), info.localPath, localOffset) == 0) {
          if (ret) {
            s.Set (rc->Lid () + localOffset);
            s.Truncate ("/", true);
            ret->Set (s.Get ());
            //~ INFOF (("### resource dir: '%s' -> '%s'", rc->Uri (), s.Get ()));
          }
          if (retExists) {
            dirExists = true;
            if (!ret) break;
          }
        }
      }
      // Unlock host or driver resources, respectively ...
      if (info.host) info.host->UnlockResources ();
      else if (info.driver) info.driver->UnlockResources ();
      break;

    case rcaAlias:
    case rcaAliasResolved:
      if (info.target) {

        // a) We did a full or partial resolution: Recurse and get the directory of the target ...
        uriResolved.SetC (info.target);
        uriResolved.Append (info.localPath);
        ok = RcPathGetDirectory (uriResolved.Get (), ret, &dirExists, NULL, allowWait);
      }
      else {

        // b) No resolution at all: List sub-aliases ...
        aliasMap.PrefixSearch (info.localPath, &idx0, &idx1);
        if (ret) {
          CString subUri;

          localOffset = strlen (info.localPath);
          for (n = idx0; n < idx1; n++) {
            s.Set (aliasMap.GetKey (n));
            StringTruncate (s.Get () + localOffset, "/", true);
            if (s[-1] != '/') {
              // Last component of alias: Check if it points to a directory ...
              subUri.SetC (prefix->Get ());
              subUri.Append (s.Get () + localOffset);
              if (RcPathIsDir (subUri.Get ())) s.Append ('/');
            }
            ret->Set (s.Get () + localOffset);
            //~ INFOF (("### alias dir: '%s' -> '%s'", aliasMap.GetKey (n), s.Get () + localOffset));
          }
        }
        if (idx1 > idx0) dirExists = true;
      }
      break;

    default:
      ok = false;
  }

  // Done ...
  if (retExists) *retExists = dirExists;
  //~ if (ret) ret->Dump ();
  if (!ok) WARNINGF (("Invalid URI: '%s'", uri));
  return ok;
}


bool RcPathIsDir (const char *uri, const TRcPathInfo *info, bool allowWait) {
  TRcPathInfo _info;
  bool dirExists;

  if (!info) {
    RcPathAnalyse (uri, &_info);
    info = &_info;
  }
  switch (info->state) {
    case rcaRoot:
    case rcaHost:
    case rcaLocal:
    case rcaDriver:
      // It's clearly a directory ...
      return true;
    case rcaResource:
    case rcaAlias:
    case rcaAliasResolved:
      // Need further investigations ...
      RcPathGetDirectory (uri, NULL, &dirExists, NULL, allowWait);
      return dirExists;
    default:  // rcaNone
      // any other cases ...
      return false;
  }
}





// ***** Pattern Matching and Expansion *****


bool RcPathMatchesSingle (const char *uri, const char *exp) {
  for (; exp[0]; exp++) switch (exp[0]) {
    case '?':
      if (!uri[0] || uri[0] == '/') return false;
      uri++;
      break;
    case '+':
      if (!uri[0] || uri[0] == '/') return false;
      uri++;
      // fall through to match 0 or more characters
    case '*':
      // Any character sequence except '/' ...
      //   Skip repeated '+' or '*' ...
      while (exp[1] == '*' || exp[1] == '+') exp++;
      //   Loop over the current dir level of 'uri' and try to match its remainder recursively ...
      while (uri[0] && uri[0] != '/') {
        if (uri[0] == exp[1] || !exp[1] || exp[1] == '?' || exp[1] == '#')
          // select only cases that have a chance to match 'uri'
          if (RcPathMatchesSingle (uri, exp + 1)) return true;
        uri++;
      }
      //   Now 'uri' points to the end of the string or to the next '/'.
      //   We are now left with the case that the wildcard matches the complete component.
      break;
    case '#':
      // Any character sequence up to the end ...
      return true;
    default:
      // Normal case: the characters must be equal ...
      if (exp[0] != uri[0]) return false;
      uri++;
  }
  return exp[0] == uri[0];    // both point to a '\0' => strings match
}


bool RcPathMatches (const char *uri, const char *pattern) {
  CSplitString patternSet;
  const char *pat;
  int n;

  if (!pattern) return false;
  patternSet.Set (pattern, INT_MAX, "," WHITESPACE);
  for (n = 0; n < patternSet.Entries (); n++) {
    pat = patternSet.Get (n);
    if (pat[0]) if (RcPathMatchesSingle (uri, pat)) return true;
  }
  return false;
}


static bool DoResolvePattern (const char *_exp, CKeySet *retResolvedPattern, CListRef<CResource> *retResources) {
  // '_exp' must be a single, stripped, absolute pattern. The return structures are not cleared, and new resources are added.
  // 'retResolvedPattern' can be NULL so that partially expanded pattern are not added in recursive calls.
  // If 'retResolvedPattern == NULL', the caller MUST have made sure that '_exp' is resolved.
  CString exp, uri;
  TRcPathInfo info;
  char *wild;
  int n;
  bool ok = true;

  //~ INFOF (("# DoResolvePattern('%s') ...", _exp));

  // Sanity ...
  if (_exp[0] != '/') return false;

  // Search for the first wildcard ...
  exp.Set (_exp);
  wild = exp.Get () + strcspn (exp.Get (), "?*+#");

  // Handle cases with no wildcard ...
  if (!*wild) {
    RcPathResolve (&uri, exp.Get ());
    RcPathAnalyse (uri.Get (), &info);
    if (info.resource && retResources) {
      // Resource known => can just add it ...
      //~ INFOF (("#   adding resource: %s", info.resource->Uri ()));
      retResources->Append (info.resource);
    }
    else if (info.state == rcaResource) {
      // Resource not known => add it to the watch set ...
      //~ INFOF (("#   adding resolution: %s", uri.Get ()));
      if (retResolvedPattern) retResolvedPattern->Set (uri.Get ());
    }
    else {
      if (retResources && info.state == rcaNone) {   // avoid warnings if lazy wildcards are used (e.g.: "s- /#")
        WARNINGF (("Invalid URI or unresolvable alias: '%s' - skipping.", _exp));
        ok = false;
      }
    }
  }

  // Handle cases with wildcards ...
  else {
    CKeySet dir;
    CString cur, post, patResolved, key;
    char *p, *q;
    int preLen;
    bool isResolved, isDir;

    // Add the expression to the watch set, but only if it is resolvable ...
    if (retResolvedPattern) {
      RcPathResolve (&patResolved, exp.Get ());
      isResolved = (RcPathGetRootDomain (patResolved) == rcpHost);
      if (isResolved) retResolvedPattern->Set (patResolved.Get ());
      //~ else WARNINGF (("Failed to resolve '%s' - skipping.", patResolved.Get ()));
    }
    else
      isResolved = true;      // The caller made sure that '_exp' was already resolved.

    // Expand one level ...
    //   This only needs to be done if the pattern was not yet passed to 'retResolvedPattern'
    //   and no resources have to be returned.
    if (!isResolved || retResources) {

      // Determine the "pre" (in 'exp'), "cur" and "post" components, where "cur" is the first path component containing a wildcard ...
      p = q = wild;
      while (*p != '/') p--;
      while (*q != '/' && *q) q++;
      if (*q) post.Set (q);
      *q = '\0';
      cur.Set (p + 1);
      p[1] = '\0';        // cut off 'exp' after the final '/' of the prefix
      preLen = p + 1 - exp.Get ();
      //~ INFOF (("#   pre = '%s', cur = '%s', post = '%s'", exp.Get (), cur.Get (), post.Get ()));

      // Get directory and recurse for all matching patterns ...
      //~ INFOF (("#   reading directory '%s' ...", exp.Get ()));
      ok = RcPathGetDirectory (exp.Get (), &dir);
      for (n = 0; n < dir.Entries () && ok; n++) {
        key.Set (dir [n]);
        p = strchr (key.Get (), '/');
        isDir = (p != NULL);
        if (isDir) *p = '\0';   // truncate trailing "/" to ensure correct matching
        if (RcPathMatchesSingle (key.Get (), cur.Get ())) {
          exp[preLen] = '\0';        // reset "pre" string ...
          exp.Append (key);
          //~ INFOF (("#     match: '%s' -> '%s'", key.Get (), exp.Get ()));
          exp.Append (post);
          if (!post.IsEmpty () || !isDir)
            ok = DoResolvePattern (exp.Get (), isResolved ? NULL : retResolvedPattern, retResources);
          if (isDir && strchr (cur, '#')) {
            // Handle '#' wildcard: decend to next deeper directory
            exp.Append ("/#");
            ok = DoResolvePattern (exp.Get (), isResolved ? NULL : retResolvedPattern, retResources);
          }
        }
      }
      //~ INFOF (("#   ... done reading directory."));
    }
  }

  // Done ...
  return ok;
}


bool RcPathResolvePattern (const char *pattern, CKeySet *retResolvedPattern, CListRef<CResource> *retResources) {
  CSplitString patternSet;
  CString exp;
  int n;
  bool ok, allOk;

  // Sanity ...
  ASSERT (pattern != NULL && retResolvedPattern != NULL);

  // Clear returned containers ...
  retResolvedPattern->Clear ();
  if (retResources) retResources->Clear ();

  // Iterate over sub-patterns ...
  patternSet.Set (pattern, INT_MAX, "," WHITESPACE);
  allOk = true;
  for (n = 0; n < patternSet.Entries (); n++) {
    ok = true;

    // Make pattern absolute ...
    RcPathNormalize (&exp, patternSet [n]);

    // Accelerator: Skip non-host domains if the general "/#" pattern is given ...
    if (strrchr (exp.Get (), '/') == exp.Get () && strchr (exp.Get (), '#') != NULL)
      // 'exp' only contains a top-level, which contains the '#' wildcard ...
      if (RcPathMatchesSingle ("/host", exp.Get ()))    // ... and matches "/host"
        exp.SetC ("/host/#");                           // => limit the search to the "host" domain, which contains everything

    // Process pattern ...
    if (ok) ok = DoResolvePattern (exp.Get (), retResolvedPattern, retResources);

    // Wrap up ...
    if (!ok) allOk = false;
  }

  // Done ...
  return allOk;
}





// *************************** Networking **************************************


/* Network protocol
 * ================
 *
 * 1. From client to server:
 *
 *  a) Operational messages
 *
 *    h <client host id> <prog name> <version>   # connect ("hello") message
 *
 *    s+ <subscriber> <driver>/<rcLid>           # subscribe to resource (no wildcards allowed); <subscriber> is the origin of the subscriber
 *    s- <subscriber> <driver>/<rcLid>           # unsubscribe to resource (no wildcards allowed)
 *
 *    r+ <driver>/<rcLid> <reqGid> <request specification>    # add or change a request
 *    r- <driver>/<rcLid> <reqGid> [<t1>]                     # remove a request
 *
 *  b) Informational messages
 *
 *    # i* messages must be issued synchronously, no new request may be issued before the "i."
 *    # response has been received.
 *
 *    iq <driver>/<rcLid>               # request all pending resource requests (one per line)
 *                                      # (not a human-readable info, but using the same, blocking protocol)
 *
 *    ir <driver>/<rcLid> <verbosity>   # request the output of 'CResource::GetInfo'
 *
 *    is <verbosity>                    # request the output of 'CRcSubscriber::GetInfoAll'
 *
 *  c) Shell execution
 *
 *    ec <command name> [<args>]        # Execute command defined by "sys.cmd.<command name>"
 *    e <text>                          # Supply data sent to STDIN of the previously started command
 *    e.                                # Send EOF to the previously started command
 *
 *
 * 2. From server to client:
 *
 *  a) Operational messages
 *
 *    h <prog name> <version>           # connect ("hello") message, sent in reply to client's "h ..." and sometimes as "alive" message
 *
 *    d <driver>/<rcLid> <type> <rw>    # declaration of exported resource; is sent automatically for all resources after a connect
 *    d.                                # no more resources follow: client may disconnect if there are no other wishes
 *    d-                                # forget (unregister) all resources from this host
 *
 *    v <driver>/<rcLid> [~]<value> [<timestamp>]   # value/state changed; timestamp is set by client, the server timestamp is optional and presently ignored if sent
 *    v <driver>/<rcLid> ?                          # state changed to "unknown"
 *
 *    r <driver>/<rcLid> [<reqGid>]     # request changed (details can later be queried using 'iq').
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


static const char *GetLocalUri (CString *ret, const char *localPath) {
  ret->SetF ("/host/%s/%s", localHostId.Get (), localPath);
  return ret->Get ();
}


static CResource *GetLocalResource (CString *ret, const char *localPath) {
  return RcGetResource (GetLocalUri (ret, localPath), false);
}


static const char *GetRemoteUri (CString *ret, CRcHost *host, const char *localPath) {
  ret->SetF ("/host/%s/%s", host->Id (), localPath);
  return ret->Get ();
}


static CResource *GetRemoteResource (CRcHost *host, const char *localPath) {
  CString s;
  return RcGetResource (GetRemoteUri (&s, host, localPath), false);
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
    sleeper.Prepare ();
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
    while (!done && sleeper.GetCmd (&netTask)) {
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
  TTicks timeLeft;
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
  CString s, line, def, info;
  bool error;
  CSplitString args;
  CResource *rc;
  CRcSubscriber *subscr;
  CRcValueState vs;
  CRcDriver *driver;
  const char *uri;
  TTicks t1;
  int n, k, num, verbosity;

  if (!receiveBuf.AppendFromFile (fd, HostId ())) {
    DEBUGF (1, ("Server for '%s': Network receive error, disconnecting", HostId ()));
    Disconnect ();
  }
  error = false;
  while (receiveBuf.ReadLine (&line) && !error) {
    DEBUGF (3, ("From client '%s' (%s): '%s'", hostId.Get (), peerAdrStr.Get (), line.Get ()));

    // Interpret line...
    line.Strip ();
    error = false;
    switch (line[0]) {

      case 'h':   // h <client host id> <version>     # connect ("hello") message
        args.Set (line.Get ());
        if (args.Entries () != 3) { error = true; break; }
        Lock ();
        hostId.Set (args[1]);
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
            sendBuf.AppendF ("d %s/%s\n", driver->Lid (), rc->ToStr (&s, true));
          }
          driver->UnlockResources ();
        }
        sendBuf.Append ("d.\n");
        //~ INFO ("### Reported resources.");
        //~ SendFlush ();
        ResetAliveTimer ();

        // Soft-"Bump" all host connections, since we ourselves may have just regained network connectivity ...
        RcBump (NULL, true);
        break;

      case 's':   // s+ <subscriber lid> <driver>/<rcLid>             # subscribe to resource (no wildcards allowed)
                  // s- <subscriber lid> <driver>/<rcLid>             # unsubscribe to resource (no wildcards allowed)
        args.Set (line.Get ());
        if (args.Entries () != 3) { error = true; break; }
        def.SetF ("%s/%s", hostId.Get (), args[1]);   // subscriber GID
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
        uri = GetLocalUri (&s, args[2]);
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

      case 'r':   // r+ <driver>/<rcLid> <reqGid> <request specification>     # add or change a request
                  // r- <driver>/<rcLid> <reqGid> [<t1>]                      # remove a request
        args.Set (line.Get (), 3);
        rc = args.Entries () < 3 ? NULL : GetLocalResource (&s, args[1]);
        error = true;
        if (rc) switch (line[1]) {
          case '+':
            rc->SetRequestFromStr (args[2]);
            error = false;
            break;
          case '-':
            args.Set (line.Get (), 4);
            if (args.Entries () == 3) {      // no time given ...
              rc->DelRequest (args[2], 0);
              error = false;
            }
            else if (args.Entries () >= 4) if (args[3][0] == '-')      // time attribute given ...
              if (TicksAbsFromString (args[3] + 1, &t1)) {
                rc->DelRequest (args[2], t1);
                error = false;
              }
            break;
        }
        break;

      case 'i':
        switch (line[1]) {

          case 'q':   // iq <driver>/<rcLid>               # request all pending resource requests
            // The output of this will be used and parsed by 'CResource::GetRequestSet()'.
            args.Set (line.Get ());
            if (args.Entries () != 2) { error = true; break; }
            rc = GetLocalResource (&s, args[1]);
            if (!rc) { WARNINGF (("Unknown resource '%s'", args[1])); error = true; break; }
            else {
              CRcRequestSet reqSet;
              CRcRequest *req;

              ASSERT (rc->GetRequestSet (&reqSet, false));   // 'allowNet == false' to avoid accidental recursion
              for (n = 0; n < reqSet.Entries (); n++) {
                req = reqSet.Get (n);
                sendBuf.AppendF ("i %s\n", req->ToStr (&s, /* precise = */ true, false, 0, "i"));
                  // i <text>                  # response to any "i*" request
              }
            }
            break;

          case 'r':   // ir <driver>/<rcLid> <verbosity>  # request the output of 'CResource::GetInfo'
            // The output of this will usually be read by the human user.
            args.Set (line.Get ());
            if (args.Entries () != 3) { error = true; break; }
            verbosity = args[2][0] - '0';
            if (verbosity < 0 || verbosity > 3) { error = true; break; }
            rc = GetLocalResource (&s, args[1]);
            if (!rc) { WARNINGF (("Unknown resource '%s'", args[1])); error = true; break; }

            rc->GetInfo (&info, verbosity, false);   // 'allowNet == false' to avoid recursion
            sendBuf.AppendFByLine ("i %s\n", info.Get ());
              // i <text>                  # response to any "i*" request
            break;

          case 's':   // is <verbosity>                    # request the output of 'CRcSubscriber::GetInfoAll'
            // The output of this will usually be read by the human user.
            if (line.Len () != 4) { error = true; break; }
            verbosity = line[3] - '0';
            if (verbosity < 0 || verbosity > 3) { error = true; break; }
            CRcSubscriber::GetInfoAll (&info, verbosity);
            sendBuf.AppendFByLine ("i %s\n", info.Get ());
              // i <text>                  # response to any "i*" request
            break;

          default:
            error = true;
        }
        if (!error) {
          sendBuf.Append ("i.\n");
            // i.                        # end of info
          ResetAliveTimer ();
        }
        break;

      case 'e':   // ec <command name> [<args>]      # Execute command defined by "sys.cmd.<command name>"
                  // e <text>                        # Supply data sent to STDIN of the previously started command
                  // e.                              # Send EOF to the previously started command
        switch (line[1]) {
          case 'c':
            args.Set (line.Get (), 3);
            if (args.Entries () < 2) { error = true; break; }
            if (!execShell) execShell = new CShellBare ();
            if (!execShell->StartRestricted (args[1], args.Entries () == 3 ? args[2] : NULL)) {
              WARNINGF (("Unable to start command '%s (%s)' - previous command not completed yet?", args[1], args.Entries () == 3 ? args[2] : ""));
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

  DEBUGF (3, ("CRcServer::NetRun (%s, %i), state = %i", HostId (), opcode, ATOMIC_READ (state)));

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
        switch (ev.Type ()) {
          case rceValueStateChanged:
            sendBuf.AppendF ("v %s/%s %s\n", ev.Resource ()->Driver ()->Lid (), ev.Resource ()->Lid (),
                              ev.ValueState ()->ToStr (&s, false, false, true));
              // v <driver>/<rcLid> [~]<value> [<timestamp>]   # value/state changed
              // v <driver>/<rcLid> ?                          # state changed to "unknown"
            canPostponeAliveTimer = true;
            break;
          case rceRequestChanged:
            sendBuf.AppendF ("r %s/%s %s\n", ev.Resource ()->Driver ()->Lid (), ev.Resource ()->Lid (),
                             ev.ValueState ()->ValidString (CString::emptyStr));
              // r <driver>/<rcLid> [<reqGid>]       # request changed
            break;
          default:
            break;    // other events are not relevant
        }
      }
      break;

    case snoAliveTimer:
      if (ATOMIC_READ (state) != scsConnected) break;
      sendBuf.AppendF ("h %s %s\n", EnvInstanceName (), buildVersion);
        // h <prog name> <version>           # connect ("hello") message
      break;

    case snoExecTimer:
      if (ATOMIC_READ (state) != scsConnected) break;
      while (execShell->ReadLine (&line))
        sendBuf.AppendF ("e %s\n", line.Get ());
          // e <text>                  # response to an 'e *' request (shell command)
      if (!execShell->IsRunning ()) {
        sendBuf.Append ("e.\n");
          // e.                        # end of the response
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
      DEBUGF (3, ("Sending to client %s (%s):\n%s", hostId.Get (), peerAdrStr.Get (), sendBuf.Get ()));

      bytesWritten = write (fd, sendBuf.Get (), bytesToWrite);
      if (bytesWritten == bytesToWrite) sendBuf.Clear ();
      else {
        if (bytesWritten >= 0) DEBUGF (3, ("  ... written %i out of %i bytes.", bytesWritten, bytesToWrite));
        else {
          if (errno == EAGAIN || errno == EWOULDBLOCK) DEBUGF (3, ("  ... would block."));
          else DEBUGF (3, ("  ... error: %s", strerror (errno)));
        }
        // Could not write everything: schedule a retry...
        sendBuf.Del (0, bytesWritten);
        //~ netThread.AddTask ((ENetOpcode) snoSend, this);
      }
    }
  }
}


void CRcServer::ResetAliveTimer () {
  aliveTimer.Set (TicksNowMonotonic () + envMaxAge * 2 / 3, envMaxAge * 2 / 3, CRcServerAliveTimerCallback, this);
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
  //   However, the channel has not yet been transferred to the caller, so that no race conditions
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
      netThread.AddTask ((ENetOpcode) hnoConSuccess, host);
      RcBump (NULL, true);    // Soft-bump other connections, since we may just have regained network connectivity
    }
    else {
      if (newErrStr.Compare (errString) != 0) {
        errString.SetO (newErrStr.Disown ());
        DEBUGF (1, ("Cannot %s '%s': %s - continue trying", port ? "connect to" : "resolve", hostId.Get (), errString.Get ()));
      }
      netThread.AddTask ((ENetOpcode) hnoConFailed, host);
    }
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
  tAge = tRetry = tIdle = 0;
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
  TTicks tWait;

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


void CRcHost::RequestConnect (bool soft) {
  //~ INFOF (("### CRcHost::RequestConnect ('%s', soft=%i)", Id (), (int) soft));
  if (!soft || state != hcsStandby)
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
  if (t1 != NEVER) s2.SetF ("%s -%s", reqGid, TicksAbsToString (&s1, t1, INT_MAX, true));
  else s2.SetC (reqGid);
  Send (RequestCommand (&s1, rc, s2.Get (), '-'));
}


bool CRcHost::RemoteGetRequestSet (CResource *rc, CRcRequestSet *ret) {
  CSplitString reqStrings;
  CString s, reply;
  CRcRequest *req;
  int n;

  // Sanity and init ...
  if (!RemoteInfo (StringF (&s, "iq %s", rc->Lid ()), &reply))
    return false;
  ret->Clear ();
  reply.Strip ("\n\r" WHITESPACE);
  reqStrings.Set (reply.Get (), INT_MAX, "\n");

  // Parse returned strings ...
  //~ INFOF (("### RemoteGetRequestSet (%s)", rc->Uri ()));
  for (n = 0; n < reqStrings.Entries (); n++) {
    //~ INFOF (("###   reqStrings[%i] = '%s'", n, reqStrings.Get (n)));
    req = new CRcRequest ();
    if (!req->SetFromStr (reqStrings[n])) {
      SECURITYF (("Invalid request as a reply to an 'iq ...' message: '%s'", reqStrings.Get (n)));
      delete req;
      return false;
    }
    req->Convert (rc);
    //~ INFOF (("###   storing request '%s'", req->ToStr (&s)));
    ret->Set (req->Gid (), req);
  }

  // Success ...
  return true;
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


TTicks CRcHost::ResetTimes (bool resetAge, bool resetRetry, bool resetIdle) {
  TTicks tNow, tNext;

  tNow = TicksNowMonotonic ();

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

  if (!receiveBuf.AppendFromFile (fd, Id ())) {
    //~ INFOF(("### CRcHost: EOF (fd = %i)", fd));
    WARNINGF (("Connection lost to host '%s' - disconnecting.", Id ()));
    netThread.AddTask ((ENetOpcode) hnoDisconnnect, this); // connection seems to be closed from peer -> disconnect ourself, too
  }

  while (receiveBuf.ReadLine (&line)) {
    DEBUGF (3, ("From server %s: '%s'", Id (), line.Get ()));

    // Interpret line...
    line.Strip ();
    error = false;
    argv = NULL;
    switch (line[0]) {

      case 'h':   // h <prog name> <version>          # connection ("hello") message
        ResetAgeTime ();
        break;

      case 'd':   // d <driver>/<rcLid> <type> <rw>  # declaration of exported resource
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
          rc = CResource::Register (s.Get (), NULL);
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

      case 'v':   // v <driver>/<rcLid> ?|([~]<value>) [<timestamp>]  # value/state changed
        //~ INFOF (("### Received: '%s'", line.Get ()));
        line.Split (&argc, &argv, 4);
        if (argc < 3) { error = true; break; }
        rc = GetRemoteResource (this, argv[1]);
        if (!rc) { error = true; break; }
        vs.SetType (rc->Type ());
        if (!vs.SetFromStrFast (argv[2])) { error = true; break; }
        //~ INFOF (("### line = '%s', argv[2] = '%s', vs = %s", line.Get (), argv[2], vsToStr ()));
        rc->ReportValueState (&vs);
        rc->NotifySubscribers (rceConnected);
        //~ INFOF (("### Received: '%s'", line.Get ()));
        //~ INFOF (("#   vs = '%s'", vs.ToStr (&s)));
        ResetAgeTime ();
        break;

      case 'r':   // r <driver>/<rcLid> [<reqGid>]       # request changed
        line.Split (&argc, &argv);
        if (argc < 2 || argc > 3) { error = true; break; }
        rc = GetRemoteResource (this, argv[1]);
        if (!rc) { error = true; break; }
        rc->NotifySubscribers (rceRequestChanged, argc == 3 ? argv[2] : NULL);
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
  TTicks tNow;
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
   * 3. NetOps are sent asynchronously and may be received here in any order, even in a very weird one (e.g.: 'hnoSend'
   *    -> 'hnoDisconnet' -> 'hnoSend'). For this reason, each operation must be executed correctly independent of the
   *    current state - 'ASSERT' statements are not allowed.
   *    An exception are the 'hnoCon*' operations, which are only allowed to be received in the 'hcsConnecting' state,
   *    which can never be left without any of these two operations.
   */

  DEBUGF (3, ("CRcHost::NetRun (%s, %i), state = %i", Id (), opcode, state));

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
      DEBUGF (3, ("Sending to server '%s':\n%s", Id (), sendBuf.Get ()));
      bytesToWrite = sendBuf.Len ();
      bytesWritten = write (fd, sendBuf.Get (), bytesToWrite);
      if (bytesWritten == bytesToWrite) sendBuf.Clear ();
      else {
        // Could not write everything...
        if (bytesWritten >= 0) DEBUGF (3, ("  ... written %i out of %i bytes.", bytesWritten, bytesToWrite));
        else {
          if (errno == EAGAIN || errno == EWOULDBLOCK) DEBUGF (3, ("  ... would block."));
          else DEBUGF (3, ("  ... error: %s", strerror (errno)));
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
    tRetry = 0;
    resetRetryTime = false;
  }
  if (state != hcsConnected) {    // idle disconnects can only be initiated in this state...
    // Clear and disable the idle timer...
    tIdle = 0;
    resetIdleTime = false;
  }
  tNow = ResetTimes (false, resetRetryTime, resetIdleTime);
  // Note: Whether an action is applicable can be decided by the 't...' time variables now.

  // Check & handle age timeout ...
  if (tAge && tNow >= tAge) {
    //~ INFOF (("### Age timout on host '%s' (tNow = %i, tAge = %i)", Id (), (int) tNow, (int) tAge));
    // Acknowledge / disable age time,,,
    tAge = 0;
    UpdateTimer ();
    // Schedule 'hnoDisconnect'...
    netThread.AddTask ((ENetOpcode) hnoDisconnnect, this);
  }

  // Check & handle retry timeout ...
  if (tRetry && tNow >= tRetry) {
    //~ INFOF (("### Retry timout on host '%s' (tNow = %i, tRetry = %i)", Id (), (int) tNow, (int) tRetry));
    // Acknowledge / disable retry time,,,
    tRetry = 0;
    UpdateTimer ();
    //~ INFOF (("### Retry timeout ('%s')", Id ()));
    // Schedule a connection attempt now...
    netThread.AddTask ((ENetOpcode) hnoSend, this);
  }

  // Check & handle idle timeout ...
  if (tIdle && tNow >= tIdle) {
    //~ INFOF (("### Idle check for host '%s' (tNow = %i, tIdle = %i)", Id (), (int) tNow, (int) tIdle));
    if (CheckIfIdle ()) {     // Really idle? Yes...
      // Acknowledge / disable idle time,,,
      tIdle = 0;
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
  CString s, info;
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
  if (_state == hcsNewRetryWait && conThread->LastAttempt () == NEVER)
    // There is a special case to consider: In the construction, the state
    // is initialized with 'hcsNewRetryWait' to initiate a new connection soon.
    // Since no valid timestamp and no error string is available, we replace
    // the format string and do not show misleading time/error strings.
    stateFormat = "New, trying...\n";
  ret->AppendF (stateFormat, TicksAbsToString (&s, conThread->LastAttempt (), 0), conThread->ErrorString ());
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
  TTicks tWait;

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


void CRcHost::CheckIO (bool *canWrite, bool *canRead, TTicks timeOut) {
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
  TTicks tWait;

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


void RcReadConfig (CString *retSignals, CString *retAttrs) {
  CSplitString args;
  CString str;
  const char *fileName, *errStr;
  FILE *f;
  char buf[256], *p, *q;
  int defaultPort;
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
      for (p = buf; *p && strchr (WHITESPACE, *p); p++);  // skip leading whitespace
      if (*p) switch (toupper (*p)) {

          case 'P':   // Default port ...
            args.Set (p);
            if (args.Entries () != 2) error  = true;
            else {
              defaultPort = strtol (args[1], &p, 0);
              if (*p != '\0') error  = true;
            }
            break;

          case 'H':   // Host ...
            // Syntax: H <host id> [<port>]
            args.Set (p);
            if (args.Entries () < 2 || args.Entries () > 3) error = true;
            else errStr = AddHost (args[1], args.Entries () == 3 ? args[2] : NULL, defaultPort);
            if (errStr) error = true;
            break;

          case 'A':   // Alias ...
            // Syntax: A <name> <target> [<attrs>]
            args.Set (p, 4);
            if (args.Entries () < 3) { error = true; break; }
            str.SetC (args[2]);
            aliasMap.Set (args[1], &str);
            // Store attributes if given ...
            if (args.Entries () > 3) retAttrs->AppendF ("%s %s\n", args[1], args[3]);
            // Auto-add host ...
            if (strncmp (str.Get (), "/host/", 6) == 0) {
              p = q = (char *) str.Get () + 6;
              while (q[0] && q[0] != '/') q++;
              q[0] = '\0';
              errStr = AddHost (p, NULL, defaultPort);
              if (errStr) error = true;
            }
            break;

          case 'S':   // Signal ...
            // Syntax: S <host> <name> <type> [<attrs>]
            //~ INFOF (("### S : '%s'", localHostId.Get (), p));
            args.Set (p, 5);
            if (args.Entries () < 4) { error = true; break; }
            retSignals->AppendF ("%s %s %s\n", args[1], args[2], args[3]);    // store signals
            // Store attributes if given ...
            if (args.Entries () > 4) retAttrs->AppendF ("/host/%s/signal/%s %s\n", args[1], args[2], args[4]);
            // Auto-add host...
            errStr = AddHost (args[1], NULL, defaultPort);  // auto-add other host
            if (errStr) error = true;
            break;

          case 'D':   // Default request / attributes ...
            // Syntax: D <name> <attrs>
            args.Set (p, 3);
            if (args.Entries () < 3) { error = true; break; }
            retAttrs->AppendF ("%s %s\n", args[1], args[2]);
            break;

          default:
            error = true;
      }

      // Cleanup...
      if (error) ERRORF (("%s in file '%s': %s", errStr ? errStr : "Invalid line", fileName, buf));
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

  // Sanitize alias map ...
  PrepareAliasMap ();
}
