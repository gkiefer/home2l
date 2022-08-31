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


#ifndef _RC_BASE_
#define _RC_BASE_

#include "resources.H"





// *************************** Global variables ********************************


// Environment settings...
extern bool serverEnabled;  // read-only

TTicks RcNetTimeout ();

// Local host ID (HID)...
extern CString localHostId; // normalized local host ID (HID):
                            // - for servers: host name + eventually the server port number (HID under which this process can be re-connected over the net)
                            // - for non-servers: host name + progname + PID

// The following are exported here (mainly) for the directory functions in 'resources.H'

extern CDict<CRcHost> hostMap;       // keys are host names; map is read-only after initialization

extern CDict<CRcDriver> driverMap;   // keys are driver LIDs; map is read-only after initialization

extern CMutex subscriberMapMutex;
static inline void SubscriberMapLock () { subscriberMapMutex.Lock (); }
static inline void SubscriberMapUnlock () { subscriberMapMutex.Unlock (); }
extern CDictRef<CRcSubscriber> subscriberMap;

extern CDictCompact<CString> aliasMap; // values are always full URIs; cyclic references in aliases are coped with by limiting the number of indirections in 'RcPathResolvePattern'
                                    // map is read-only after initialization

extern CMutex unregisteredResourceMapMutex;
extern CDictRef<CResource> unregisteredResourceMap;  // keeps (and owns) unregistered resources





// *************************** Helper classes **********************************


class CRcSubscriberLink {
  public:
    CRcSubscriberLink (CRcSubscriber *_subscr, CRcSubscriberLink *_next) { subscr = _subscr; next = _next; isConnected = false; }

    CRcSubscriber *subscr;
    CRcSubscriberLink *next;

    // Link attributes...
    bool isConnected;
};


class CResourceLink {
  public:
    CResourceLink (CResource *_resource, CResourceLink *_next) { resource = _resource; next = _next; }

    CResource *resource;
    CResourceLink *next;
};





// *************************** URI Path Handling *******************************


#ifndef SWIG



// ***** Path Normalization and Resolution *****


const char *RcPathNormalize (CString *ret, const char *uri, const char *workDir = NULL);
  // Make a path absolute and normalize it.
  //
  // If 'workDir' is left 'NULL', the default initial path ("/alias") is used.
  // On success, the first character is '/'. However, if invalid input is passed as 'uri',
  // the first character may be different. This must be checked by the caller.
  //
  // Returns 'ret->Get ()'.

const char *RcPathResolve (CString *ret, const char *uri, const char *workDir = NULL, const char **retTarget = NULL, const char **retLocalPath = NULL);
  // Resolve URIs from the "/alias" or "/local" domain as far as possible and return the resolved path.
  //
  // The path is normalized and made absolute internally (unless 'retTarget' or 'retLocalPath' are set, see below).
  // If 'workDir' is left unset ('NULL'), the default initial path is used ("/alias").
  // This function operates symbolically. No resources need to be registered before use.
  // Only 'localHostId' and 'aliasMap' are used must be defined beforehead.
  //
  // 'retTarget' and 'retLocalPath' allow to retrieve the alias target used together with the local part of
  // 'uri' not part of the mapping. 'ret' is always exactly equal to the concatenation of '*retTarget' and '*retLocalPath'.
  // If no resolution happens, '*retTarget' is set to NULL, and '*retLocalPath' is left unchanged.
  // No dynamic data is allocated. If modified, '*retLocalPath' will always be pointed somewhere into 'uri'.
  // '*retTarget' may be pointed somewhere into 'aliasMap'. To allow this, no normalization is done internally, and
  // 'uri' must be normalized and made absolute before.
  //
  // On error, an unresolved, but eventually normalized variant of 'uri' is copied to '*ret'. No warning is logged.
  // Returns 'ret->Get ()'.



// ***** Path Analysis *****


enum ERcPathDomain {
  rcpNone = 0,    // unknown, incomplete or invalid
  rcpHost,        // host dir ("/host")
  rcpLocal,       // local dir ("/local")
  rcpAlias        // alias dir ("/alias")
};


enum ERcPathAnalysisState {
  // Analysis state of a partial or complete URI. See 'RcPathAnalyse()' for details.
  rcaNone = 0,      // unknown or invalid

  rcaRoot,          // in main root dir ("/<here>")
  rcaHost,          // host prefix found, in host ID part ("/host/<here>")
  rcaLocal,         // local prefix found, in driver ID part ("/local/<here>")
  rcaDriver,        // local prefix or own host ID found, in driver part ("/local/<here>" or "/host/<localhost>/<here>"
  rcaResource,      // local or host prefix found, in resource LID part ("/local/<driver>/.../<here>" or "/host/<host>/.../here")

  rcaAlias,         // alias prefix found ("/alias/.../<here>")
  rcaAliasResolved  // alias prefix found and the path is fully resolvable to a target in the "/host" or "/local" domain
};


typedef struct SRcPathInfo {
  ERcPathAnalysisState state;
  ERcPathDomain domain;
  const char *localPath;
  CRcHost *host;
  CRcDriver *driver;
  const char *target;       // alias target (for aliases)
  CResource *resource;
} TRcPathInfo;


static inline void RcPathInfoClear (TRcPathInfo *info) { bzero (info, sizeof (*info)); }

static inline bool RcPathIsValid (const ERcPathAnalysisState state) { return state == rcaResource || state == rcaAliasResolved; }
static inline bool RcPathIsValid (const TRcPathInfo *info) { return RcPathIsValid (info->state); }
  // Check if a path is valid and complete.

ERcPathDomain RcPathGetRootDomain (const char *uri);
  // Analyse the first path component and get the root domain ('rcpHost', 'rcpLocal', 'rcpAlias').
  // 'uri' must be an absolute path. On error, 'rcpNone' is returned.

ERcPathAnalysisState RcPathAnalyse (const char *uri, TRcPathInfo *ret, bool allowWait = false);
  // Analyse a complete or partial URI and return info about it.
  //
  // 'uri' must be an absolute path starting with '/' and must be normalized before
  //  (i.e., it must not contain any "//", "/./" or "..").
  //  Wildcards are not allowed.
  //
  // Depending on the correctness and completeness of 'uri', '*ret' is filled accordingly.
  // If set, 'ret->localPath' always points inside 'uri', and the offset from the beginning
  // of the original string can safely be computed by 'ret->localPath - uri'.
  //
  // The identified analysis state corresponds to the last component(s) of the path, which may
  // be incomplete or even empty.
  //
  // Examples ('uri' -> 'ret->state' / 'ret->localPath'):
  //    "/host/this_host"   -> rcaHost   / "this_host"
  //    "/host/this_host/"  -> rcaDriver / ""
  //    "/host/this_host/driver/resource/in/subdir"  -> rcaResource / "resource/in/subdir" + 'ret->driver' set if driver is valid
  //    "/host/other_host/driver/resource/in/subdir"  -> rcaResource / "driver/resource/in/subdir" + 'ret->host' set if host is valid
  //
  // Full specification:
  //
  //   rcpNone:     'ret->localPath' = 'NULL'
  //
  //   rcpRoot:     'ret->localPath' = start of domain name ('uri + 1')
  //
  //   rcpHost:     'ret->localPath' = start of host name
  //                'ret->host'        set if the hostname is complete and known
  //
  //   rcpDriver:   'ret->localPath' = driver name
  //                'ret->driver'      set if the driver is complete and known
  //
  //   rcpResource: 'ret->localPath' = resource LID
  //                'ret->resource'    set if name was found (= resource is registered)
  //                'ret->host' OR
  //                'ret->driver'      also set, depending on the resource location (for local resources, 'ret->host' is unspecified)
  //
  //   rcpAlias:    'ret->localPath' = everything behind "/alias/" if 'ret->target == NULL', else everything behind 'ret->target'
  //                'ret->target'    = last resolvable alias target = prefix of 'ret->localPath', potentially without a trailing "/";
  //                                   in any case, if 'ret->target' is set, the correct full target path can be determined
  //                                   by concatenating 'ret->target' with 'ret->localPath'
  //
  //   rcpAliasResolved:
  //                'ret->localPath' = everything behind "/alias/" if 'ret->target == NULL', else everything behind 'ret->target'
  //                'ret->target'    = last resolvable alias target = prefix of 'ret->localPath', potentially without a trailing "/";
  //                All other information can now be obtained by analysing 'ret->target' + 'ret->localPath'
  //
  // 'ret->target' always points into a value of 'aliasMap'.
  //
  // If the parameter 'allowWait' is set (true), the function is allowed to block a little while (at most 'envNetTimeout')
  // to query resource information over the net and obtain better results. If unset, the function is guaranteed to not
  // block.

bool RcPathGetDirectory (const char *uri, CKeySet *ret = NULL, bool *retExists = NULL, CString *retPrefix = NULL, bool allowWait = false);
  // Read a local subdirectory.
  //
  // 'uri' must be an absolute path starting with "/" without wildcards. It will be normalized internally
  // and returned via 'retPrefix' (if given).
  // All 'ret*' arguments are optional.
  // If given, the key set '*ret' is filled with relative names.
  // If given, '*retExists' is set to 'true' if 'uri' actually points to a directory. This is an efficient
  // alternative for checking for the existence of a directory if its contents are not needed.
  // If given, '*retPrefix' is set to the normalized URI.
  // Directory-like entries are appended with a '/'.
  //
  // Returns 'true' on success. On error, a warning is emitted and 'false' is returned.

bool RcPathIsDir (const char *uri, const TRcPathInfo *info = NULL, bool allowWait = false);
  // Check if analysed URI represents a directory with sub-entries.
  // This is not always trivial, since aliases may point to trees of resources, which only become
  // visible after their registration. The function tries to be as precise as possible by eventually
  // calling 'RcPathGetDirectory ()' internally.
  // If the URI has been analysed before, the analysis results should be passed as 'info'.
  // Otherwise, the URI is analysed internally.



// ***** Pattern Matching and Expansion *****


bool RcPathMatchesSingle (const char *uri, const char *exp);
  // Same as 'RcPathMatches', but slightly faster and only supports a single pattern expression 'exp',
  // which must be normalized, absolute and stripped.

bool RcPathMatches (const char *uri, const char *pattern);
  // Check if the URI matches a pattern.
  //
  // The pattern may be a comma- or whitespace-separated list of URIs (expressions).
  // Within the resource expressions, both MQTT-style and filename-style wildcards
  // can be used to select multiple resources:
  //
  //   '?' matches any single character except '/'.
  //   '*' matches 0 or more characters except '/'.
  //   '+' matches 1 or more characters except '/'.
  //   '#' matches the complete remaining string (including '/' characters) and can
  //       thus be used to select a complete subtree. If used, '#' must be the last
  //       character in the expression. Anything behind a '#' is ignored silently. },
  //
  // 'uri' is neither normalized nor resolved. In 'uri', wildcard characters are allowed and
  // treated like normal characters.

bool RcPathResolvePattern (const char *pattern, CKeySet *retResolvedPattern, CListRef<CResource> *retResources = NULL);
  // Determine all currently registered resources based on a wildcard search using 'pattern'.
  //
  // The pattern may be a comma- or whitespace-separated list of URIs. Wildcards are allowed.
  //
  // All URIs are be resolved into the "/host" domain and, unless stored in 'retResources' (see below),
  // stored in 'retResolvedPattern'. Such an URI may represent an unregistered resource or be an expression with wildcards.
  // Remote resources may be advertised by their host at some later time, so the 'retResolvedPattern' set may need to be
  // observed afterwards.
  //
  // If 'retResources' is defined, matched known resources are returned via '*retResources'.
  // URIs without wildcards that are returned via 'retResources' are not added to 'retResolvedPattern'.
  //
  // If an expression cannot be resolved into the "/host" domain, a warning is emitted, and the expression is
  // skipped.
  //
  // Returns 'true' if 'pattern' could be processed completely without error or 'false' otherwise.


#endif // #ifndef SWIG





// *************************** Networking **************************************

// [T:x] - Method is only/may only be called from thread x


enum ENetOpcode {
  // Opcodes to be passed to 'CNetRunnable' objects...

  // General...
  noNone = 0,
  noExit         // instruct a network thread to stop finally

  // Codes 0x10..0x1f are reserved for servers.
  // Codes 0x20..0x2f are reserved for hosts.
};


class CNetRunnable {
  // Classes with methods to be executed by the net thread can be derived from this.
  public:
    virtual void NetRun (ENetOpcode opcode, void *data) = 0;
};


class CNetThread: public CThread {
  // A background thread for networking tasks.
  // Presently, there is only one such thread, and it grabs and serves all objects from 'hostMap' and 'serverMap'.
  // If desired in the future, distributing the work over multiple threads is possible by instantiating multiple
  // objects of this class and by implementing arguments to select the handled tasks.
  public:
    CNetThread () { listenFd = -1; }
    virtual ~CNetThread () { Stop (); }

    void Start ();
    void Stop ();

    void AddTask (ENetOpcode opcode, CNetRunnable *runnable = NULL, void *data = NULL);

  protected:
    virtual void *Run ();

    CSleeper sleeper;
    int listenFd;     // listening FD for server (or -1 in no-server mode)
};


void RcNetStart ();
void RcNetStop ();
  // If there are untransmitted requests, the function may wait up to 'rc.netTimeout'
  // milliseconds before actually shutting down.
  //
  // Note: 'RcNetStop' must only be called when shutting down the program.
  //   The function terminates the net thread and brings everything in a
  //   stable condition. Restarting is not possible.
  //
  //   It does *not* free all memory and does *not* close all file handles.
  //   The OS can do this much better afterwards. This is probably not the style
  //   of programming many instructors use to teach. However, the author
  //   of these lines believes that there are too many applications out
  //   there that spend seconds for exiting *after* the user hit the "quit"
  //   button (i.e. telling the program that he/she does not want the program
  //   to do anything anymore). This library aims to be efficient and snappy.





// *************************** CRcServer ***************************************

// A 'CRcServer' object represents a connection to a client for which this instance
// represents the server.

// [T:x] - Method is only (allowed to be) called from thread x


enum EServerConnectionState {
  scsNew = 0,           // Starting state, no hello message has been received yet
  scsConnected,         // Connection established
  scsDisconnected,      // Connection ended
  scsInDeletion         // a 'noDelete' task has already been submitted
};


enum EServerNetOpcode {
  snoSubscriberEvent= 0x10, // a subscriber event is pending => send out to client
  snoAliveTimer,            // alive timer asserted
  snoExecTimer,             // exec timer asserted
  snoDelete,                // delete server object
};


class CRcServer: public CNetRunnable {
  // A 'CRcServer' object represents a client connection for which we act as a server.
  public:
    CRcServer (int _fd, uint32_t _peerIp4Adr, uint16_t _peerPort, const char *_peerAdrStr);      // [T:net]
    virtual ~CRcServer ();      // [T:net]

    // Identification...
    const char *PeerAdr () { return peerAdrStr.Get (); }
    const char *HostId () { return hostId.Get (); }

    // Callbacks...
    int Fd () { return fd; }                // [T:net]
    int WritePending () { return !sendBuf.IsEmpty (); } // [T:net]
    void OnFdReadable ();                   // [T:net]
    void OnFdWritable () { SendFlush (); }  // [T:net]

    virtual void NetRun (ENetOpcode opcode, void *data);  // [T:net]

    // Misc...
    bool HasSubscribers () { return subscrDict.Entries () > 0; }    // [T:any]

    // Info...
    static void PrintInfoAll (FILE *f = stdout, int verbosity = 2);    // Info on all servers
      // verbosity == 0: only server (one line), >= 1: list subscriptions, >= 2: list resources for subscriptions

  protected:
    friend class CNetThread;

    void GetInfo (CString *ret, int verbosity = 2);   // [T:any]
      // Info on 'this'; method is protected because objects may be created or deleted any time by the net thread

    // Helpers...
    void Lock () { mutex.Lock (); }
    void Unlock () { mutex.Unlock (); }

    void Disconnect ();                 // [T:net] Cancel connection; schedule a delete operation for net thread
    void SendFlush ();                  // [T:net]
    void ResetAliveTimer ();            // [T:net] Set/reset the alive timer

    // Static data...
    int fd;
    CString peerAdrStr;
    uint32_t peerIp4Adr;
    uint16_t peerPort;

    // Dynanamic data (protected by object mutex)...
    CMutex mutex;                   // protects all fields labelled "[T:X]" with X != "net";
                                    // presently, only for 'GetInfo()' can access these fields, and it will only read
                                    // => Readers in net thread do not lock.
    CRcServer *next;                // [T:net] for managing all objects in a chained list

    EServerConnectionState state;   // [atomic]
    CString hostId;                 // [T:w=net,r=any] Host ID as sent in the "hello" message by the peer
    CDict<CRcSubscriber> subscrDict;// [T:w=net,r=any] Set of agent subscribers managed by this server; Key is the LID == GID.
                                    //         For each subscriber on the client side, one agent subscriber on the server is
                                    //         created, which represents the client subscriber and transmits all events to
                                    //         the client.

    CString receiveBuf;             // [T:net] received data is processed completely in 'OnFdReadable'
    CString sendBuf;                // [T:net]

    CTimer aliveTimer;              // [T:net] for sending regular "alive" messages

    // Shell-related data (TBD: untested, potentialy incomplete)...
    CShell *execShell;              // [T:net]
    CTimer execTimer;               // [T:net] for checking a running shell (each 100ms); TBD: eliminate, make fully event-driven
};





// *************************** CRcHost *****************************************

// [T:x] - Method is only/may only be called from thread x


enum EHostConnectionState {
  hcsNewConnecting = 0, // Object is new and like 'hcsConnecting' otherwise (see below)
  hcsNewRetryWait,      // Connection will be (re-)tried after some time (like 'hcsRetryWait', see below); Initial state
  hcsNewConnected,      // Connected, but no complete resource info has been received yet.

  hcsConnecting,        // Address lookup and connection in progress (e.g. background thread running)
  hcsRetryWait,         // Not connected, but connection will be tried after some time
  hcsConnected,         // Connection established, may be disconnected if idle
  hcsStandby            // Not connected, no connection presently required
};

static inline bool HostResourcesUnknown (EHostConnectionState s) { return s <= hcsNewConnected; }


enum EHostNetOpcode {
  hnoSend = 0x20,       // Try to flush send buffer now (implies a connection attempt if necessary)
                        //   hcsStandby, hcsRetryWait -> Try to connect now
                        //   hcsConnecting -> No action ('hnoSend' will be auto-submitted on completion)
                        //   hcsConnected -> send (on error -> hcsRetryWait)

  hnoDisconnnect,       // Close connection because of an error or an idle timeout
                        //   hcsStandby -> (keep previous state)
                        //   hcsConnecting -> hcsStandby/hcsRetryWait (must be detected by the 'hnoCon*' operations)
                        //   hcsConnected -> hcsStandby/hcsRetryWait  (+ close socket)
                        //   hcsRetryWait -> hcsStandby/hcsRetryWait

  hnoTimer,             // A timer event has occured: Check for timed condition: idle timeout? redry? age? ...?

  hnoConSuccess,        // Connection is established, the connection thread completed successfully
                        //   hcsConnecting -> hcsConnected (+ store fd, + submit or execute 'hnoSend')
                        //   (all others) -> abort (should not happen)

  hnoConFailed,         // Connection attempt failed, the connection thread completed, but without success
                        //   hcsConnecting -> hcsRetryWait
                        //   (all others) -> abort (should not happen)
};


class CConThread: public CThread {
  // Connection thread: Performs peer lookup and connects to a remote host.
  public:
    CConThread () { Clear (); }

    void Clear () { host = NULL; ip4Adr = 0; port = 0; fd = -1; errString.Clear (); tLastAttempt = NEVER; }

    void Start (class CRcHost *_host) { host = _host; CThread::Start (); }
    void Cancel () { Lock (); host = NULL; Unlock (); }
      // Emulate cancellation by setting a flag to let the thread not write to any data anymore.
      // After cancellation, the thread still remains alive, but does no longer access any data (i.e. 'host') outside
      // this object. By calling 'exit' without deleting the object before, long waiting times on exit can be avoided.

    void GetAdr (uint32_t *_ip4Adr, uint16_t *_port) { Lock (); *_ip4Adr = ip4Adr; *_port = port; Unlock (); }
    int Fd () { return ATOMIC_READ (fd); }

    void Lock () { mutex.Lock (); }
    void Unlock () { mutex.Unlock (); }

    const char *AdrString () { return adrString.Get (); }     // returned pointer valid as long as 'this' is locked
    const char *ErrorString () { return errString.Get (); }   // returned pointer valid as long as 'this' is locked
    TTicks LastAttempt () { return ATOMIC_READ (tLastAttempt); }

  protected:
    void *Run ();

    class CRcHost *host;
    CMutex mutex;

    uint32_t ip4Adr;      // [T:con] peer's IPv4 adress in network order
    uint16_t port;        // [T:con] peer's port number in network order
    int fd;               // [T:con,w; any,r] file descriptor, set and unchanged after the 'hnoCon*' message was sent
    CString adrString;    // [T:any, protected by 'mutex'] peer's clear-text address and port (for info retrieval); TBD: really needed?
    CString errString;    // [T:any, protected by 'mutex'] String describing the last error (for info retrieval)
    TTicks tLastAttempt;  // [T:any, protected by 'mutex'] Time (absolute) of last connection attempt (for info retrieval)
};


class CRcHost: public CNetRunnable, public CShell {
  // Represents a remote server host; instances are managed by a global 'CDict'
  public:
    CRcHost ();
    virtual ~CRcHost ();

    void Init (const char *_id, const char *_netHost, int _netPort) { id.Set (_id); netHost.Set (_netHost); netPort = _netPort; }

    void ClearResources ();     // unregister all resources

    const char *Id () { return id.Get (); }
    const char *ToStr (CString *ret) { GetInfo (ret, 0); ret->Strip (); return *ret; }

    CResource *GetResource (const char *rcLid, bool allowWait = false);

    TTicks LastAlive () { return ATOMIC_READ (tLastAlive); }

    // Networking (for application)...
    void RemoteSubscribe (CRcSubscriber *subscr, CResource *rc);
    void RemoteUnsubscribe (CRcSubscriber *subscr, CResource *rc);
    void RemoteSetRequest (CResource *rc, CRcRequest *req);
    void RemoteDelRequest (CResource *rc, const char *reqGid, TTicks t1);

    bool RemoteGetRequestSet (CResource *rc, CRcRequestSet *ret);
      // returns all currently set requests

    bool RemoteInfoResource (CResource *rc, int verbosity, CString *retText);
      // returns info on a resource, output format equivalent to 'CResource::GetInfo ()'
    bool RemoteInfoSubscribers (int verbosity, CString *retText);
      // returns info on all subscribers, output format equivalent to 'CRcSubscriber::GetInfoAll ()'

    void RequestConnect (bool soft = false);
      // request a (re-)connection now;
      // If 'soft' is set, no connection attempt is made in state 'hcsStandby' (only in 'hcsRetryWait' an alike).

    // Networking callbacks...
    int Fd () { return fd; }      // [T:net]
    bool WritePending () { return !sendBufEmpty; }  // [T:any]
    void OnFdReadable ();         // [T:net]
    void OnFdWritable ();         // [T:net]

    virtual void NetRun (ENetOpcode opcode, void *data);  // [T:net]

    // For directory services...
    void GetInfo (CString *ret, int verbosity = 2);
      // verbosity == 0: only server (one line)
      //           >= 1: list subscriptions
      //           >= 2: list resources for subscriptions
      // 'verbosity == 0' implies that not network operations are performed.
    void PrintInfo (FILE *f = stdout, int verbosity = 2);
    static void PrintInfoAll (FILE *f = stdout, int verbosity = 2);    // Info on all hosts

    int LockResources () { Lock (); return resourceMap.Entries (); }
      // returns the number of presently known resources (no network querying!)
    CResource *GetResource (int n) { return resourceMap.Get (n); }
    void UnlockResources () { Unlock (); }

    // CShell functions (TBD: untested, potentialy incomplete)...
    virtual bool Start (const char *cmd, bool readStdErr = false);
    virtual bool StartRestricted (const char *name, const char *args = NULL);
    virtual bool IsRunning () { return execBusy; }
    virtual void Wait ();
    virtual void CheckIO (bool *canWrite, bool *canRead, TTicks timeOut = -1);
    virtual void WriteLine (const char *line);
    virtual void WriteClose ();
    virtual bool ReadLine (CString *str);
    virtual bool ReadClosed () { return execComplete; }

  protected:
    friend class CResource;
    friend class CConThread;

    // Helpers...
    void Lock () { mutex.Lock (); }
    void Unlock () { mutex.Unlock (); }

    void Send (const char *line);                       // always non-blocking
    void SendAL (const char *line);                     // always non-blocking

    bool RemoteInfo (const char *msg, CString *ret);    // always blocking, returns 'true' on success

    bool CheckIfIdle ();  // [T:net] Check various conditions on whether this host is idle and can be out into standby mode

    TTicks ResetTimes (bool resetAge, bool resetRetry, bool resetIdle);    // [T:net]
      // Reset (and eventually enable) the selected time(s) to "now + delay" and schedule the next 'hnoTimer' event.
      // Returns the current time.
    void ResetAgeTime () { ResetTimes (true, false, false); }
    void ResetRetryTime () { ResetTimes (false, true, false); }
    void ResetFirstRetry () { tFirstRetry = 0; }
    void ResetIdleTime () { ResetTimes (false, false, true); }
    void UpdateTimer () { ResetTimes (false, false, false); }

    // Static data...
    CString id;
    CString netHost;    // network host name
    int netPort;        // network port number

    // Dynamic data (protected by the mutex unless marked by '[T:net]')...
    CMutex mutex;
    CCond cond;                     // general condition variable, signalled on: info received, exec output received
    CDictRef<CResource> resourceMap; // Resources in the map are static (i.e., cannot be removed), but the map itself is dynamic
    EHostConnectionState state;     // [T:net]
    int fd;                         // [T:net]
    CConThread *conThread;          // [T:net (starting/joining)]
    CString receiveBuf;             // [T:net] received data is processed in 'OnFdReadable' and forwarded to other ('*Response') buffers
    CString sendBuf;
    bool sendBufEmpty;              // [T:any] (pseudo-atomic)
    TTicks tAge, tRetry, tIdle, tFirstRetry;
        // [T:net] Times (monotonic) for the next timeouts of the respective class (0 = inactive/never; special value 'NEVER' not used!)
    int retriesLeft;
    CTimer timer;                   // [T:net]
    TTicks tLastAlive;              // [atomic] time of last alive indication from server
    CString infoResponse, execResponse;       // [T:any, protected by 'mutex']
    bool infoBusy, infoComplete, execBusy, execComplete, execWriteClosed;   // [T:any!]
};





// *************************** Initialization **********************************


void RcSetupNetworking (bool enableServer);     // Setup networking (server enable flag, netmask etc.)

void RcReadConfig (CString *retSignals, CString *retAttrs);        // Read resources config file.
  // Read the 'resources.conf' file and initialize the main directories
  // ('aliasMap', 'hostMap', signals) as well as the local host ID and port.
  //
  // The signals declared in the config file for the local host are returned as a string via 'retSignals' - one signal
  // per line with the syntax "<name> <type>".
  // This allows to read the config file before the signal driver is initialized.
  //
  // Resource registration attributes are returned as a string via 'retAttrs' - one resource per line with the syntax
  // "<rc> [<attrs>]".





#endif
