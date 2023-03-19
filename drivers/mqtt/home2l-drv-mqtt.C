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


#include <mosquitto.h>
#include <errno.h>

#include "resources.H"





// *************************** Global variables ********************************


static struct mosquitto *mosq = NULL;

static CRcDriver *mqttDrv = NULL;

static CKeySet mqttRetainedTopics;
  // for shutdown: topics for which retained messages (may) have been sent, which now have to be cleared





// *************************** Environment Parameters **************************


// ***** General and Mandantory *****

ENV_PARA_STRING("mqtt.broker", envMqttBroker, "localhost");
  /* MQTT broker
   *
   * Network name or IP of the MQTT broker, optionally followed by its port number.
   * If no port is given, the default of 1883 or 8883 is used, depending on whether
   * TLS is enabled or not.
   */



// ***** Importing *****

ENV_PARA_SPECIAL("mqtt.import.<ID>", const char *, NULL);
  /* Defines a single resource to be imported from the MQTT network
   *
   * The string has the general syntax:
   * \begin{center} \small
   * \texttt{<topic>:[<reqtopic>]:[<validtopic>[=<value>]]:[<lid>]:[<type>]:[<false>:<true>]}
   * \end{center}
   * Where:
   * \begin{description}
   *   \item[\texttt{<topic>}] is the MQTT state topic to listen to.
   *   \item[\texttt{<reqtopic>}] is the MQTT command topic for manipulating the
   *       resource. A ''+'' sign in the beginning is replaced by the state topic.
   *       If empty, the resource will be read-only.
   *   \item[\texttt{<validtopic>[=<value>]}] is the topic and optionally payload string
   *       indicating whether the peer is connected. As long as the topic does not
   *       reflect the given value, the resource will be reported with state \refapic{rcsUnknown}
   *       in the \textit{Home2L} network. The value comparison is done in a case-insensitive
   *       way, and trailing and leading whitespaces are ignored. If \texttt{<value>} is
   *       not give, the payload is interpreted as a value of type \refapic{rctBool}.
   *   \item[\texttt{<lid>}] is the resource's local ID. If empty, \texttt{<ID>} is used.
   *   \item[\texttt{<type>}] is the \textit{Home2L} data type for the resource.
   *       The default is ''string'' if \texttt{<false>} and \texttt{<true>} are empty, else ''bool''.
   *   \item[\texttt{<false>:<true>}] are alternative strings for the Boolean values
   *       for ''false'' and ''true'', respectively. They are only effective if the resource type
   *       is 'rctBool'. By default, ''0'' and ''1'' are written.
   * \end{description}
   *
   * Trailing colons can be omitted.
   *
   * The \texttt{<ID>} part of the key can be chosen arbitrarily.
   */



// ***** Exporting *****

ENV_PARA_SPECIAL("mqtt.export.<ID>", const char *, NULL);
  /* Defines a single resource to be exported to the MQTT network
   *
   * The string has the general syntax:
   * \begin{center}
   * \texttt{<uri>:[<subtopic>]:[<reqsubtopic>]:[<false>:<true>]}
   * \end{center}
   * Where:
   * \begin{description}
   *   \item[\texttt{<uri>}] identifies the resource.
   *   \item[\texttt{<subtopic>}] defines the MQTT state subtopic. If empty, \texttt{<ID>} is used.
   *       The full topic will be the prefix specified by \refenv{mqtt.prefix} followed
   *       by a ''/'' and the subtopic.
   *   \item[\texttt{<reqsubtopic>}] is the optional MQTT command topic for manipulating the
   *       resource. A ''+'' sign in the beginning is replaced by the state subtopic.
   *       If empty, the resource cannot be manipulated from the MQTT network.
   *   \item[\texttt{<false>,<true>}] are alternative strings for the truth values
   *       ''false'' and ''true'', respectively. They are only effective if the resource type
   *       is 'rctBool'. By default, ''0'' and ''1'' are written to outgoing messages.
   * \end{description}
   *
   * Trailing colons can be omitted.
   *
   * The topic name may contain slashes (''/''), but they should not start with
   * ''host'' to avoid conflicts resources exported by \refenv{mqtt.exportSet}.
   *
   * The \texttt{<ID>} part of the key can be chosen arbitrarily.
   */

ENV_PARA_STRING("mqtt.exportSet", envMqttExportSet, NULL);
  /* Defines a set of resources to be exported read-only to the MQTT network
   *
   * This provides a simplified, easy-to-use way to export many resources.
   * The string may contain wildcards and multiple patterns separated by comma as
   * accepted by the \refapic{RcNewSubscriber} API call.
   *
   * The MQTT topics will be set to the prefix specified by \refenv{mqtt.prefix}
   * followed by the effective URI of the respective resource.
   *
   * For security reasons, manipulating these resources from the MQTT network
   * is not possible. To allow manipulations and allow more fine-grained settings,
   * place request by publishing MQTT messages, define
   * \refenv{mqtt.export.<ID>} parameters instead.
   */



// ***** General Options *****

ENV_PARA_INT("mqtt.qos", envMqttQoS, 0);
  /* MQTT "Quality of Service" (QoS) level for the communication with the broker
   *
   * This defines the QoS level for both subscriptions and publishing operations
   * with the broker.
   *
   * Recommendations on selecting the QoS level are given in
   * Section~\ref{sec:drvlib-mqtt-details}.
   */

ENV_PARA_INT("mqtt.keepalive", envMqttKeepalive, 60);
  /* MQTT keepalive time
   *
   * This is the number of seconds after which the broker should send a PING message
   * to the client if no other messages have been exchanged in that time.
   * Setting this to 0 disables the keepalive mechanism.
   */

ENV_PARA_STRING("mqtt.prefix", envMqttPrefix, "home2l");
  /* Prefix for MQTT topics of exported resources and ''birth-and-will'' messages
   */

ENV_PARA_STRING("mqtt.birthAndWill", envMqttBirthAndWill, "online");
  /* Subtopic stating whether the Home2L client is connected
   *
   * The driver uses the MQTT ''last will and testament'' mechanism to allow
   * others to know whether we are connected.
   *
   * The string has the general syntax:
   * \begin{center}
   * \texttt{<subtopic>[:<false>:<true>]}
   * \end{center}
   * Where:
   * \begin{description}
   *   \item[\texttt{<subtopic>}] specifies the subtopic. The full topic will
   *       be ''<\refenv{mqtt.prefix}>/<subtopic>''.
   *   \item[\texttt{<false>}] specifies the payload published as a last will and
   *       testament (LWT) message. By default, ''0'' is published.
   *   \item[\texttt{<true>}] specifies the payload published on startup of this
   *       driver as a retained message. By default, ''1'' is published.
   * \end{description}
   *
   * Trailing colons can be omitted.
   */

ENV_PARA_STRING("mqtt.busySign", envMqttBusySign, "!");
  /* Character indicating the "busy" state for outgoing messages for exported resources
   *
   * Values with state \refapic{rcsBusy} are prefixed with this. If the receiving MQTT
   * subscribers do not support the concept of ''busy'' resources, an empty string
   * may be set here.
   */

ENV_PARA_STRING("mqtt.unknownSign", envMqttUnkownSign, "?");
  /* Payload string indicating the "unkown" state for outgoing messages for exported resources
   *
   * If set to an empty string, an empty payload is published.
   */

ENV_PARA_STRING("mqtt.reqId", envMqttReqId, "mqtt");
  /* Request ID for incoming messages for exported resources
   *
   * See also: \refenv{mqtt.reqAttrs}
   */

ENV_PARA_STRING("mqtt.reqAttrs", envMqttReqAttrs, NULL);
  /* Request attributes for incoming messages for exported resources
   *
   * If an MQTT message is received for an exported writable resources (e.g. actors),
   * this will be transformed into a request reflecting the desired ''write'' operation.
   * This setting allows to specify the attributes for such requests. If no priority
   * is given here, the default priority of \refapic{rcPrioNormal} is used, which is
   * the default for automation rules.
   */



// ***** Security Options *****

ENV_PARA_STRING("mqtt.clientId", envMqttClientId, NULL);
  /* MQTT client ID [default: instance name]
   */

ENV_PARA_STRING("mqtt.username", envMqttUsername, NULL);
  /* MQTT user name to send to the broker [default: none]
   */

ENV_PARA_STRING("mqtt.password", envMqttPassword, NULL);
  /* MQTT user password to send to the broker [default: none]
   */

ENV_PARA_STRING("mqtt.interface", envMqttInterface, NULL);
  /* MQTT network interface to use
   *
   * This is the network host name or IP address of the local network interface
   * to bind to. Set this parameter to restrict MQTT network communication to a
   * particular interface. If unset, all interfaces are enabled.
   */

ENV_PARA_INT("mqtt.tls.mode", envMqttTlsMode, 0);
  /* Select TLS mode of operation
   *
   * The following values are allowed:
   * \begin{itemize}
   *   \item[0:] Do not use TLS.
   *   \item[1:] Enable certificate based SSL/TLS support. This requires
   *       \refenv{mqtt.tls.capath} to be specified. If the broker requests
   *       clients to provide a certificate, \refenv{mqtt.tls.certfile} and
   *       \refenv{mqtt.tls.keyfile} must be specified, too.
   *   \item[2:] Enable pre-shared-key (PSK) based TLS support. This requires
   *       \refenv{mqtt.tls.psk} and \refenv{mqtt.tls.identity} to be specified.
   * \end{itemize}
   */

ENV_PARA_STRING("mqtt.tls.capath", envMqttTlsCaPath, NULL);
  /* Path to a directory containing the PEM encoded trusted CA certificate files.
   */

ENV_PARA_STRING("mqtt.tls.certfile", envMqttTlsCertFile, NULL);
  /* Path to a file containing the PEM encoded certificate file for this client
   */

ENV_PARA_STRING("mqtt.tls.keyfile", envMqttTlsKeyFile, NULL);
  /* Path to a file containing the PEM encoded private key for this client
   *
   * Note: Encrypted key files which require a password to be entered at run time
   * are not supported.
   */

ENV_PARA_STRING("mqtt.tls.psk", envMqttTlsPsk, NULL);
  /* Pre-shared-key for TLS/PSK mode in hex format with no leading ''0x''
   */

ENV_PARA_STRING("mqtt.tls.identity", envMqttTlsIdentity, NULL);
  /* Identity for TLS/PSK mode [Default: Home2L instance name]
   */





// *************************** MQTT Import *************************************


// ***** CMqttImport *****


class CMqttImport {
  // MQTT topic imported as a resource.
  // Received topic messages are reported as value changes.
  // 'DriveValue()' calls let a respective MQTT state message be published.
  public:
    CMqttImport () { rc = NULL; }
    ~CMqttImport () {}

    const char *Topic () { return topic.Get (); }
    const char *ReqTopic () { return reqTopic.Get (); }
    const char *ValidTopic () { return validTopic.Get (); }

    const char *ToStr (CString *) { return CString::emptyStr; }

    // (Try to) initialize object ...
    bool Init (const char *key, const char *id, const char *desc) {
      CSplitString args;
      CString errStr;
      ERcType rcType;
      const char *arg, *q, *rcLid;

      // Parse description ...
      rcLid = NULL;
      rcType = rctString;
      args.Set (desc, 8, ":");
      if (args.Entries () < 1) errStr.SetC ("Missing topic");
      else {                                              // state/main topic (mandantory) ...
        arg = args[0];
        if (!arg[0]) errStr.SetC ("Missing topic");
        else if (mosquitto_pub_topic_check (arg) != MOSQ_ERR_SUCCESS) errStr.SetF ("Invalid MQTT state topic '%'", arg);
        else topic.Set (arg);
      }
      if (errStr.IsEmpty () && args.Entries () > 1) {    // request topic (optional) ...
        arg = args[1];
        if (arg[0] == '+') {
          reqTopic.SetF ("%s/%s", topic.Get (), &arg[1]);
          reqTopic.PathNormalize ();
        }
        else if (arg[0] != '\0') reqTopic.Set (arg);
        if (mosquitto_pub_topic_check (reqTopic.Get ()) != MOSQ_ERR_SUCCESS)
          errStr.SetF ("Invalid MQTT request topic '%s'", reqTopic.Get ());
      }
      if (errStr.IsEmpty () && args.Entries () > 2) {    // valid topic + value (optional) ...
        arg = args[2];
        q = strchr (arg, '=');
        if (q) {
          validTopic.Set (arg, q - arg);
          validPayload.Set (q + 1);
        }
        else validTopic.Set (arg);
        if (validTopic[0] == '+') {
          validTopic[0] = '/';
          validTopic.Insert (0, topic.Get ());
          validTopic.PathNormalize ();
        }
        if (mosquitto_pub_topic_check (validTopic.Get ()) != MOSQ_ERR_SUCCESS)
          errStr.SetF ("Invalid MQTT valid topic '%s'", validTopic.Get ());
      }
      if (errStr.IsEmpty () && args.Entries () > 3) {    // resource LID (default = 'id') ...
        arg = args[3];
        if (arg[0]) rcLid = arg;
      }
      if (errStr.IsEmpty () && args.Entries () > 4) {    // type (optional, default = string) ...
        arg = args[4];
        rcType = RcTypeGetFromName (arg);
        if (rcType == rctNone) errStr.SetF ("Invalid type '%s'", arg);
      }
      if (errStr.IsEmpty () && args.Entries () > 5) {    // truth values (optional) ...
        arg = args[5];
        if (arg[0]) {
          boolStr[0].Set (arg);
          boolStr[0].Strip ();
        }
        if (errStr.IsEmpty () && args.Entries () > 6) {
          arg = args[6];
          if (arg[0]) {
            boolStr[1].Set (arg);
            boolStr[1].Strip ();
          }
        }
      }

      // Handle error ...
      if (!errStr.IsEmpty ()) {
        WARNINGF (("Invalid setting '%s': %s", key, errStr.Get ()));
        return false;
      }

      // Register local resource ...
      if (!rcLid) rcLid = id; // topic.Get ();
      rc = mqttDrv->RegisterResource (rcLid, rcType, !reqTopic.IsEmpty ());
      rc->SetUserData (this);

      // Done ...
      //~ INFOF (("### Registered '%s' / '%s' / '%s'", topic.Get (), reqTopic.Get (), validTopic.Get ()));
      return true;
    }

    // Done ...
    void Done () {
      if (!reqTopic.IsEmpty ()) mqttRetainedTopics.Set (reqTopic.Get ());
    }

    // [T:any] To be called on receipt of an MQTT message (for 'topic' or 'validTopic'):
    // - message for the state topic: report it
    // - message for the valid topic: report the "invalid" state if invalid, else re-subscribe to get the state soon
    void OnMqttMessage (const char *_topic, const char *_payload) {
      //~ INFOF (("### CMqttImport::OnMqttMessage (%s, %s)", _topic, _payload));
      if (topic.Compare (_topic) == 0) {
        // Received a value for the state topic: report it ...
        if (_payload) rc->ReportValue (_payload);
        else rc->ReportUnknown ();
      }
      else if (validTopic.Compare (_topic) == 0) {
        // Received a value for the "valid" (alive) topic ...
        CString payload (_payload);
        int mosqErr;

        payload.Strip ();
        if (strcasecmp (payload.Get (), validPayload.Get ()) == 0) {
          // Client became alive again: Re-subscribe to state topic to (try to) get the current state value ...
          mosqErr = mosquitto_unsubscribe (mosq, NULL, topic.Get ());
          if (mosqErr != MOSQ_ERR_SUCCESS)
            WARNINGF (("MQTT: Failed to unsubscribe from topic '%s': %s", _topic, mosquitto_strerror (mosqErr)));
          mosqErr = mosquitto_subscribe (mosq, NULL, topic.Get (), envMqttQoS);
          if (mosqErr != MOSQ_ERR_SUCCESS)
            WARNINGF (("MQTT: Failed to re-subscribe to topic '%s': %s", _topic, mosquitto_strerror (mosqErr)));
        }
        else {
          // Client got lost: Invalidate resource ...
          rc->ReportUnknown ();
        }
      }
    }

    // Driver's 'DriveValue()' entry:
    // - publish the value to the request topic
    void DriveValue (CRcValueState *vs) {
      CString payload;
      int mosqErr, valIdx;

      //~ INFOF (("### Driving to '%s': %s", reqTopic.Get (), vs->ToStr ()));

      ASSERT (!reqTopic.IsEmpty ());
      if (vs->IsValid ()) {
        // Defined value: publish to request topic ...
        if (vs->Type () == rctBool) {
          // Transform to user-specified truth values...
          valIdx = vs->Bool () ? 1 : 0;
          if (!boolStr[valIdx].IsEmpty ()) payload.SetC (boolStr[valIdx]);
          else payload.Set ('0' + (char) valIdx);
        }
        else
          vs->ToStr (&payload, false, false, false, INT_MAX);
        mosqErr = mosquitto_publish (mosq, NULL, reqTopic.Get (), payload.Len (), payload.Get (), envMqttQoS, true);
        if (mosqErr != MOSQ_ERR_SUCCESS)
          WARNINGF (("MQTT: Failed to publish '%s' <- '%s': %s", reqTopic.Get (), payload.Get (), mosquitto_strerror (mosqErr)));
        vs->SetToReportBusy ();
      }
      else {
        // No value: Clear (retained) message ...
        mosqErr = mosquitto_publish (mosq, NULL, reqTopic.Get (), 0, payload.Get (), envMqttQoS, true);
        if (mosqErr != MOSQ_ERR_SUCCESS)
          WARNINGF (("MQTT: Failed to clear topic '%s': %s", reqTopic.Get (), mosquitto_strerror (mosqErr)));
      }
    }

    // [T:any]
    int OnConnect (const char **mqttSub) {
      int ret = 0;

      // Publish our currently requested value ...
      if (!reqTopic.IsEmpty ()) rc->RedriveValue ();

      // Subscribe to MQTT topics ..
      if (!topic.IsEmpty ()) mqttSub[ret++] = topic.Get ();
      if (!validTopic.IsEmpty ()) mqttSub[ret++] = validTopic.Get ();
      return ret;
    }

    // [T:any]
    void OnDisconnect () {
      rc->ReportUnknown ();
    }

  protected:
    CString topic, reqTopic, validTopic, validPayload, boolStr[2];
    CResource *rc;
};





// ***** Global variables *****


static CMqttImport **mqttImportList = NULL;
static int mqttImports;

static CDictRef<CMqttImport> mqttImportLookup;
  // Dictionary to quickly identify the relevant import for an incoming message.
  // If a topic is handled by multiple export objects (a.g. a common "valid" topic, a 'NULL' is entered here.





// ***** Global functions *****


static void MqttImportAddToLookup (const char *topic, CMqttImport *imp) {
  // Add a topic/import object to the lookup table.
  // If the topic is unique, 'imp' is registered, otherwise 'NULL' is set to indicate
  // that multiple import objects have to be checked.
  int idx = mqttImportLookup.Find (topic);
  mqttImportLookup.Set (topic, idx < 0 ? imp : NULL);
}


static void MqttImportInit () {
  CString prefix;
  CMqttImport *imp;
  const char *key;
  int i, idx0, idx1, prefixLen;

  prefix.SetC ("mqtt.import.");
  prefixLen = prefix.Len ();
  EnvGetPrefixInterval (prefix.Get (), &idx0, &idx1);
  if (idx1 > idx0) mqttImportList = new CMqttImport * [idx1 - idx0];
  mqttImports = 0;
  for (i = idx0; i < idx1; i++) {
    key = EnvGetKey (i);
    imp = new CMqttImport ();
    if (imp->Init (key, key + prefixLen, EnvGetVal (i))) {
      mqttImportList[mqttImports++] = imp;
      // Register state and "valid" topic in lookup dictionary ...
      MqttImportAddToLookup (imp->Topic (), imp);
      if (imp->ValidTopic () [0] != '\0') MqttImportAddToLookup (imp->ValidTopic (), imp);
    }
  }
}


static void MqttImportDone () {
  int i;

  mqttImportLookup.Clear ();
  for (i = 0; i < mqttImports; i++) delete mqttImportList[i];
  FREEA (mqttImportList);
}


static int MqttImportOnConnect (const char **mqttSub) {
  int i, subs;

  // Notify all single exports and collect request topics for subscriptions ...
  subs = 0;
  for (i = 0; i < mqttImports; i++) {
    subs += mqttImportList[i]->OnConnect (&mqttSub[subs]);
  }

  // Done ...
  return subs;
}


static inline void MqttImportOnDisconnect () {
  for (int i = 0; i < mqttImports; i++)
    mqttImportList[i]->OnDisconnect ();
}


static bool MqttImportOnMqttMessage (const char *topic, const char *payload) {
  CMqttImport *imp;
  int i, idx;

  idx = mqttImportLookup.Find (topic);
  if (idx < 0) return false;

  imp = mqttImportLookup.Get (idx);
  if (imp) imp->OnMqttMessage (topic, payload);     // topic relevant for a single import
  else {      // topic relevant for a multiple imports: Check all ...
    for (i = 0; i < mqttImports; i++) mqttImportList[i]->OnMqttMessage (topic, payload);
  }
  return true;
}





// *************************** MQTT Export *************************************


// ***** CMqttExport *****


class CMqttExport: public CRcSubscriber {
  // Represents an exported resource or set of resources.
  // The internal resource is subscribed to, and value change events are published as MQTT messages.
  // If writable, received matching MQTT messages are transformed into a request set for the resource.
  public:
    CMqttExport () { rc = NULL; }
    ~CMqttExport () { Done (); }

    CResource *Resource () { return rc; }
    const char *Topic () { return topic.Get (); }
    const char *ReqTopic () { return reqTopic.Get (); }

    // (Try to) init object for a single resource ...
    bool InitSingle (const char *key, const char *id, const char *desc) {
      CSplitString args;
      CString errStr, s;
      const char *arg;

      //~ INFOF(("### Exporting '%s' <- %s", id, desc));

      errStr = NULL;
      args.Set (desc, 6, ":");
      if (args.Entries () < 1) errStr.SetC ("Empty description");
      else {
        // Resource ...
        rc = RcGet (args[0]);
        if (!rc) errStr.SetF ("Invalid resource indicator '%s'", args[0]);
      }
      if (errStr.IsEmpty ()) {
        // State topic ...
        if (args.Entries () > 1) arg = args[1];
        else arg = CString::emptyStr;
        topic.SetF ("%s/%s", envMqttPrefix, arg[0] ? arg : id);
        if (mosquitto_pub_topic_check (topic.Get ()) != MOSQ_ERR_SUCCESS) errStr.SetF ("Invalid MQTT state topic '%'", topic.Get ());
      }
      if (errStr.IsEmpty () && args.Entries () > 2) {
        // Request topic ...
        arg = args[2];
        if (arg[0] == '+') {
          reqTopic.SetF ("%s/%s", topic.Get (), &arg[1]);
          reqTopic.PathNormalize ();
        }
        else if (arg[0] != '\0') reqTopic.SetF ("%s/%s", envMqttPrefix, arg);
        if (mosquitto_pub_topic_check (reqTopic.Get ()) != MOSQ_ERR_SUCCESS) errStr.SetF ("Invalid MQTT request topic '%'", reqTopic.Get ());
      }
      if (errStr.IsEmpty () && args.Entries () > 3) {
        // Boolean value strings ...
        arg = args[3];
        if (arg[0]) boolStr [0].Set (arg);
        if (errStr.IsEmpty () && args.Entries () > 4) {
          arg = args[4];
          if (arg[0]) boolStr [1].Set (arg);
        }
        boolStr[0].Strip ();
        boolStr[1].Strip ();
      }
      if (!errStr.IsEmpty ()) {
        WARNINGF (("Invalid setting '%s': %s", key, errStr.Get ()));
        topic.Clear ();
        reqTopic.Clear ();
        rc = NULL;
        return false;
      }
      CRcSubscriber::Register (StringF (&s, "MQTT/%s", id));
      return true;
    }

    // (Try to) init object for a set of resources in a read-only way ...
    bool InitSet (const char *pattern) {
      topic.Set (pattern);
      rc = NULL;
      CRcSubscriber::Register ("MQTT");
      return true;
    }

    // Done ...
    void Done () {
      if (rc) mqttRetainedTopics.Set (topic.Get ());
    }

    // [T:any] To be called when a connection to the broker succeeds:
    // - (re-)subscribe to all Home2L resources
    // - implicitly causes values to be reported as MQTT messages
    // - return MQTT subscriptions to be made
    int OnConnect (const char **mqttSub) {
      CRcSubscriber::Clear ();                            // just to be sure that everything will be subscribed freshly
      if (rc) {                                           // single export ...
        CRcSubscriber::AddResource (rc);
        if (reqTopic.IsEmpty ()) return 0;
        else {
          mqttSub[0] = reqTopic.Get ();   // return the request topic ...
          return 1;
        }
      }
      else {                                              // set export ...
        CRcSubscriber::AddResources (topic.Get ());
        return 0;                         // no requests accepted
      }
    }

    // [T:any] To be called when a connection to the broker is lost:
    // - unsubscribe to all Home2L resources (saves CPU time and a new subscription is necessary anyway)
    void OnDisconnect () {
      CRcSubscriber::Clear ();
    }

    // [T:any] Called on Home2L subscriber events:
    // - publish a new value/state as an MQTT message
    virtual bool OnEvent (CRcEvent *ev) {                 // from 'CRcEventProcessor'
      if (ev->Type () == rceValueStateChanged) {
        CRcValueState *vs;
        CString setTopic, payload;
        const char *_topic;
        int mosqErr, valIdx;

        // Determine topic...
        if (rc) _topic = topic.Get ();  // single export
        else {                          // set export: take URI as subtopic
          _topic = StringF (&setTopic, "%s%s", envMqttPrefix, ev->Resource ()->Uri ());
          mqttRetainedTopics.Set (_topic);    // Register topic (cannot be done in 'Done()')
        }

        // Determine payload...
        vs = ev->ValueState ();
        switch (vs->State ()) {
          case rcsBusy:
            payload.SetC (envMqttBusySign);
            // fall through ...
          case rcsValid:
            if (vs->Type () == rctBool) {
              // Transform to user-specified truth values...
              valIdx = vs->Bool () ? 1 : 0;
              if (!boolStr[valIdx].IsEmpty ()) payload.Append (boolStr[valIdx]);
              else payload.Append ('0' + valIdx);
            }
            else {
              vs->ToStr (&payload);
              if (vs->State () == rcsBusy) {
                payload.Del (0, 1);
                payload.Insert (0, envMqttBusySign);
              }
            }
            break;
          default:    // rcsUnknown
            payload.SetC (envMqttUnkownSign);
            break;
        }

        // Publish ...
        mosqErr = mosquitto_publish (mosq, NULL, _topic, payload[0] ? payload.Len () : 0, payload.Get (), envMqttQoS, true);
          // 'mid' = NULL (do not keep reference for later tracking), 'retain' = true
        if (mosqErr != MOSQ_ERR_SUCCESS)
          WARNINGF (("MQTT: Failed to publish '%s' = '%s': %s", _topic, payload.Get (), mosquitto_strerror (mosqErr)));
      }
      return true;
    }

    // [T:any] To be called on receipt of an MQTT message to the request topics
    // - generate a request accordingly
    // - the state topic is not relevant (must not be subscribed to)
    void OnMqttReqMessage (const char *_payload) {
      CRcRequest *req;

      //~ INFOF (("### CMqttExport::OnMqttReqMessage ('%s', '%s')", reqTopic.Get (), _payload));
      if (!_payload) rc->DelRequest (envMqttReqId);
        // Empty payload: Remove request
      else {
        // Create and set request...
        req = NULL;
        if (rc->Type () == rctBool && !boolStr[0].IsEmpty () && !boolStr[1].IsEmpty ()) {

          // Handle special boolean strings ...
          CString s;
          s.Set (_payload);
          s.Strip ();
          if (strcasecmp (s.Get (), boolStr[0]) == 0) req = new CRcRequest (false, envMqttReqId);
          else if (strcasecmp (s.Get (), boolStr[1]) == 0) req = new CRcRequest (true, envMqttReqId);
        }
        if (!req) req = new CRcRequest (_payload, envMqttReqId);
        req->SetAttrsFromStr (envMqttReqAttrs);
        //~ INFOF (("###   setting request for %s: %s", rc->Uri (), req->ToStr ()));
        rc->SetRequest (req);
      }
    }

  protected:
    CString topic, reqTopic, boolStr[2];
    CResource *rc;  // the resource (NULL in the case of a resource set)
    // In case of a resource set:
    //   a) 'rc == NULL'
    //   b) 'topic' contains the pattern
};





// ***** Global variables *****


static CMqttExport **mqttExportList = NULL;
static int mqttExports;

static CMqttExport *mqttSetExport = NULL;

static CDictRef<CMqttExport> mqttExportLookup;
  // Dictionary to quickly identify the relevant export for an incoming message (which is for a request topic).





// ***** Global functions *****


static void MqttExportInit () {
  CString prefix;
  CMqttExport *exp;
  const char *key;
  int i, idx0, idx1, prefixLen;

  // (Try to) initialize all explicit exports ...
  prefix.SetC ("mqtt.export.");
  prefixLen = prefix.Len ();
  EnvGetPrefixInterval (prefix.Get (), &idx0, &idx1);
  if (idx1 > idx0) mqttExportList = new CMqttExport * [idx1 - idx0];
  mqttExports = 0;
  for (i = idx0; i < idx1; i++) {
    key = EnvGetKey (i);
    exp = new CMqttExport ();
    if (exp->InitSingle (key, key + prefixLen, EnvGetVal (i))) {
      mqttExportList[mqttExports] = exp;
      // Register request topic in lookup dictionary (if present) ...
      if (exp->ReqTopic () [0]) mqttExportLookup.Set (exp->ReqTopic (), exp);
      mqttExports++;
    }
  }

  // Initialize set export ...
  if (envMqttExportSet) {
    exp = new CMqttExport ();
    if (exp->InitSet (envMqttExportSet)) mqttSetExport = exp;
    else delete exp;
  }
}


static void MqttExportDone () {
  int i;

  mqttExportLookup.Clear ();
  for (i = 0; i < mqttExports; i++) delete mqttExportList[i];
  FREEA (mqttExportList);
  FREEO (mqttSetExport);
}


static int MqttExportOnConnect (const char **mqttSub) {
  int i, subs;

  // Notify all single exports and collect request topics for subscriptions ...
  subs = 0;
  for (i = 0; i < mqttExports; i++) {
    subs = mqttExportList[i]->OnConnect (mqttSub);
    mqttSub += subs;
  }

  // Notify set export (if present) ...
  if (mqttSetExport) subs += mqttSetExport->OnConnect (mqttSub);

  // Done ...
  return subs;
}


static inline void MqttExportOnDisconnect () {
  for (int i = 0; i < mqttExports; i++)
    mqttExportList[i]->OnDisconnect ();
  if (mqttSetExport) mqttSetExport->OnDisconnect ();
}


static bool MqttExportOnMqttMessage (const char *topic, const char *payload) {
  CMqttExport *exp = mqttExportLookup.Get (topic);
  //~ INFOF(("### MqttExportOnMqttMessage (%s, %s) -> %i", topic, payload, exp ? 1 : 0));
  if (!exp) return false;   // not our topic
  exp->OnMqttReqMessage (payload);
  return true;
}





// *************************** MQTT ********************************************


static CMutex mqttCallbackMutex;              // protects the following fields
static volatile int mqttCallbacksRunning = 0; // number of currently running callbacks
static CString mqttBirthAndWillTopic;         // topic and payload for birth and will messages ...
static CString mqttBirthPayload, mqttWillPayload;




// ***** Callbacks *****


static void MqttCallbackOnLog (struct mosquitto *mosq, void *, int level, const char *str) {
  const char *levelStr;
  int debugLevel;

  debugLevel = 1;     // default debug level (for anything but MOSQ_LOG_DEBUG)
  switch (level) {
    case MOSQ_LOG_ERR:      levelStr = "error";   break;
    case MOSQ_LOG_WARNING:  levelStr = "warning"; break;
    case MOSQ_LOG_NOTICE:   levelStr = "notice";  break;
    case MOSQ_LOG_INFO:     levelStr = "info";    break;
    case MOSQ_LOG_DEBUG:    levelStr = "debug";   debugLevel = 2; break;
    default:                levelStr = "other";
  }
  DEBUGF(debugLevel, ("[MOSQ:%s] %s", levelStr, str));
}


static void MqttCallbackOnConnect (struct mosquitto *mosq, void *, int connackCode) {
  CString topic, payload;
  const char **mqttSubList;
  int i, mqttSubs, mqttSubsMax, mosqErr;

  //~ INFO (("### MqttCallbackOnConnect ()"));
  if (connackCode != 0) {   // Failed to connect ...
    WARNINGF (("MQTT: Failed to connect to broker '%s': %s", envMqttBroker, mosquitto_connack_string (connackCode)));
  }
  else {                    // Connected successfully ...

    // Prepare array for all topics to subscribe to ...
    mqttSubsMax = mqttExports         // max. 1 per export (request topic)
                + 0                   // none for the set exports (not writable)
                + 2 * mqttImports;    // max. 2 per import (state topic + valid topic)
    mqttSubList = mqttSubsMax ? MALLOC (const char *, mqttSubsMax) : NULL;

    // Call subsystem functions ...
    mqttSubs = MqttImportOnConnect (&mqttSubList[0]);
    mqttSubs += MqttExportOnConnect (&mqttSubList[mqttSubs]);

    // (Re-)subscribe to all requested topics ...
    //   TBD: Switch to 'mosquitto_subscribe_multiple()' (not yet available in v1.5.7 / Debian Buster)
    for (i = 0; i < mqttSubs; i++) {
      //~ INFOF (("###   Subscribing to '%s'.", mqttSubList[i]));
      mosqErr = mosquitto_subscribe (mosq, NULL, mqttSubList[i], envMqttQoS);
      if (mosqErr != MOSQ_ERR_SUCCESS)
        WARNINGF (("MQTT: Failed to subscribe to '%s': %s", mqttSubList[i], mosquitto_strerror (mosqErr)));
    }

    // Cleanup ...
    FREEP (mqttSubList);

    // Publish birth ...
    if (!mqttBirthAndWillTopic.IsEmpty ()) {
      mosqErr = mosquitto_publish (mosq, NULL, mqttBirthAndWillTopic.Get (), mqttBirthPayload.Len (), mqttBirthPayload.Get (), envMqttQoS, true);
      if (mosqErr != MOSQ_ERR_SUCCESS)
        WARNINGF (("MQTT: Failed to set last will: %s", mosquitto_strerror (mosqErr)));
    }
  }
}


static void MqttCallbackOnDisconnect (struct mosquitto *mosq, void *, int reason) {

  // Log event (and reason) ...
  if (reason == 0) DEBUGF (1, ("Disconnected from broker '%s'", envMqttBroker));
  else {
    // TBD: Use the following, more verbose line ('mosquitto_reason_string()' not yet available in v1.5.7 / Debian Buster):
    //   WARNINGF (("Connection lost to broker '%s': %s", envMqttBroker, mosquitto_reason_string (reason)));
    WARNINGF (("Connection lost to broker '%s'.", envMqttBroker));
  }

  // Notify subsystems ...
  MqttImportOnDisconnect ();
  MqttExportOnDisconnect ();
}


static void MqttCallbackOnMessage (struct mosquitto *mosq, void *, const struct mosquitto_message *message) {
  const char *topic, *payload;
  CString sPayload;

  // Extract topic and payload ...
  topic = message->topic;
  if (message->payloadlen > 0) {
    sPayload.Set ((const char *) message->payload, message->payloadlen);
    payload = sPayload.Get ();
  }
  else payload = NULL;
    /* From 'mosquitto.h' (missing in Mosquitto API documentation [2020-06-29]):
     *
     *    struct mosquitto_message {
     *      int mid;
     *      char *topic;
     *      void *payload;
     *      int payloadlen;
     *      int qos;
     *      bool retain;
     *    };
     */
  //~ INFOF (("### MqttCallbackOnMessage (%s, %s)", topic, payload));

  // Pass message to subsystems ...
  if (!MqttImportOnMqttMessage (topic, payload))
    if (!MqttExportOnMqttMessage (topic, payload))
      WARNINGF (("MQTT: Received message on unsubscribed topic '%s'", topic));
}





// ***** Functions *****


static int MqttCallbackNoPassword (char *, int, int, void *) { return 0; }


static void MqttInit () {
  CSplitString args;
  CString netHost;
  int mosqErr, netPort;

  // Init mosquitto ...
  ASSERT (mosquitto_lib_init () == MOSQ_ERR_SUCCESS);
  mosq = mosquitto_new (EnvInstanceName (), true, NULL);
    // session ID is instance name; "clean session" mode; no user data
  if (!mosq) ERRORF (("MQTT: Failed to initialize Mosquitto: %s", strerror (errno)));

  // Set security/authentication options ...
  mosqErr = mosquitto_username_pw_set (mosq, envMqttUsername, envMqttPassword);
  if (mosqErr != MOSQ_ERR_SUCCESS)
    WARNINGF (("MQTT: Failed to define username/password: %s", mosquitto_strerror (mosqErr)));
  switch (envMqttTlsMode) {
    case 1:
      DEBUG (1, "Enabling certificate based SSL/TLS support.");
      mosqErr = mosquitto_tls_set (mosq, NULL, envMqttTlsCaPath, envMqttTlsCertFile, envMqttTlsKeyFile, MqttCallbackNoPassword);
      if (mosqErr != MOSQ_ERR_SUCCESS)
        WARNINGF (("MQTT: Failed to enable certificate based SSL/TLS support: %s", mosquitto_strerror (mosqErr)));
      break;
    case 2:
      DEBUG (1, "Enabling pre-shared-key (PSK) based TLS support.");
      mosqErr = mosquitto_tls_psk_set (mosq, envMqttTlsPsk, envMqttTlsIdentity ? envMqttTlsIdentity : EnvInstanceName (), NULL);
        // use default ciphers
      if (mosqErr != MOSQ_ERR_SUCCESS)
        WARNINGF (("MQTT: Failed to enable pre-shared-key (PSK) based TLS support: %s", mosquitto_strerror (mosqErr)));
      break;
    default:
      DEBUG (1, "Not using TLS.");
  }

  // Init import/export subsystems ...
  MqttImportInit ();
  MqttExportInit ();

  // Init callbacks ...
  mosquitto_log_callback_set (mosq, MqttCallbackOnLog);
  mosquitto_connect_callback_set (mosq, MqttCallbackOnConnect);
  mosquitto_disconnect_callback_set (mosq, MqttCallbackOnDisconnect);
  mosquitto_message_callback_set (mosq, MqttCallbackOnMessage);

  // Birth and will ...
  args.Set (envMqttBirthAndWill, 3, ":");
  mqttBirthAndWillTopic.SetF ("%s/%s", envMqttPrefix, args[0]);
  mqttBirthAndWillTopic.PathNormalize ();
  if (mosquitto_pub_topic_check (mqttBirthAndWillTopic.Get ()) != MOSQ_ERR_SUCCESS) {
    WARNINGF (("MQTT: Invalid birth-and-will topic (%s): '%s'", envMqttBirthAndWillKey, mqttBirthAndWillTopic.Get ()));
    mqttBirthAndWillTopic.Clear ();
  }
  else {
    mqttWillPayload.SetF (args.Entries () <= 1 ? "0" : args[1]);
    mosqErr = mosquitto_will_set (mosq, mqttBirthAndWillTopic.Get (), mqttWillPayload.Len () + 1, mqttWillPayload.Get (), envMqttQoS, true);
    if (mosqErr != MOSQ_ERR_SUCCESS)
      WARNINGF (("MQTT: Failed to set last will: %s", mosquitto_strerror (mosqErr)));
    mqttBirthPayload.SetF (args.Entries () <= 2 ? "1" : args[2]);
  }

  // Start background thread ...
  ASSERT (mosquitto_loop_start (mosq) == MOSQ_ERR_SUCCESS);

  // Connect ...
  if (EnvNetResolve (envMqttBroker, &netHost, &netPort, (envMqttTlsMode == 0) ? 1883 : 8883, true)) {
    if (!envMqttInterface)
      mosqErr = mosquitto_connect_async (mosq, netHost.Get (), netPort, envMqttKeepalive);
    else
      mosqErr = mosquitto_connect_bind_async (mosq, netHost.Get (), netPort, envMqttKeepalive, envMqttInterface);
    if (mosqErr != MOSQ_ERR_SUCCESS)
      WARNINGF (("MQTT: Failed to connect to broker: %s", mosqErr == MOSQ_ERR_ERRNO ? strerror (errno) : mosquitto_strerror (mosqErr)));
  }
}


static void MqttDone () {
  int i, mosqErr;

  // Disable callbacks (except log callback) ...
  mosquitto_connect_callback_set (mosq, NULL);
  mosquitto_disconnect_callback_set (mosq, NULL);
  mosquitto_message_callback_set (mosq, NULL);

  // Wait until eventually running callbacks complete ...
  mqttCallbackMutex.Lock ();
  while (mqttCallbacksRunning > 0) {
    /* We do this in a busy waiting loop, since this is only done once on shutdown,
     * and the expected waiting time is short. An alternative implementation with blocking
     * waiting here may require additional overhead (e.g. "condition signal" calls) in
     * the callbacks to be executed frequently any time.
     */
    mqttCallbackMutex.Unlock ();
    mqttCallbackMutex.Lock ();
  }
  mqttCallbackMutex.Unlock ();

  // Clean up data structures and determine retained topics to be cleared ...
  MqttExportDone ();
  MqttImportDone ();

  // Clear all retained messages ...
  for (i = 0; i < mqttRetainedTopics.Entries (); i++) {
    mosqErr = mosquitto_publish (mosq, NULL, mqttRetainedTopics[i], 0, NULL, envMqttQoS, true);
    if (mosqErr != MOSQ_ERR_SUCCESS)
      WARNINGF (("MQTT: Failed to clear retained message on topic '%s': %s", mqttRetainedTopics[i], mosquitto_strerror (mosqErr)));
  }
  mqttRetainedTopics.Clear ();

  // Publish that we are offline ...
  if (!mqttBirthAndWillTopic.IsEmpty ()) {
    mosqErr = mosquitto_publish (mosq, NULL, mqttBirthAndWillTopic.Get (), mqttWillPayload.Len () + 1, mqttWillPayload.Get (), envMqttQoS, true);
    if (mosqErr != MOSQ_ERR_SUCCESS)
      WARNINGF (("MQTT: Failed to publish '%s' <- '%s': %s", mqttBirthAndWillTopic.Get (), mqttWillPayload.Get (), mosquitto_strerror (mosqErr)));
  }

  // Disconnect ...
  mosqErr = mosquitto_disconnect (mosq);
  ASSERT (mosqErr == MOSQ_ERR_SUCCESS || mosqErr == MOSQ_ERR_NO_CONN);

  // Shutdown mosquitto ...
  ASSERT (mosquitto_loop_stop (mosq, false) == MOSQ_ERR_SUCCESS);
  mosquitto_destroy (mosq);
  mosq = NULL;
  mosquitto_lib_cleanup ();
}





// *************************** Top-Level ***************************************


HOME2L_DRIVER(mqtt) (ERcDriverOperation op, CRcDriver *drv, CResource *rc, CRcValueState *vs) {
  CMqttImport *mqttImport;

  switch (op) {

    case rcdOpInit:
      mqttDrv = drv;
      MqttInit ();
      break;

    case rcdOpStop:
      MqttDone ();
      mqttDrv = NULL;
      break;

    case rcdOpDriveValue:
      mqttImport = (CMqttImport *) rc->UserData ();
      mqttImport->DriveValue (vs);
      break;
  }
}
