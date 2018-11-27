/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2015-2018 Gundolf Kiefer
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


#include "phone.H"

#include <config.H>

#include <mediastreamer2/msfactory.h>     // new in 3.2.1
#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msvideo.h>
//#include <mediastreamer2/devices.h>       // for 'ms_devices_info_add'

#include <linphone/linphonecore.h>

#include <sys/stat.h>       // for 'open'
#include <errno.h>
#include <string.h>
#include <unistd.h>




struct TPhoneData {
  struct _LinphoneCore *lpCore;
  struct _LinphoneCall *lpCall[2];
};


#define PHONE_LIBDATA(phone) (* (TPhoneData *) phone->GetLibData ())
#define LIBDATA (* (TPhoneData *) libData)




// ********************** Helpers **************************


// The following functions are implemented in 'base.C', the prototypes
// must match those in 'base.H'.


#if WITH_DEBUG == 1
extern int envDebug;            ///< Debug level (read-only; may also be mapped to a constant macro).
#else
#define envDebug 0
#endif


void LogPara (const char *_logHead, const char* _logFile, int _logLine);
void LogPrintf (const char *format, ...);


#if WITH_DEBUG == 1
#define DEBUG(LEVEL, MSG) do { if (envDebug >= LEVEL) LogPara ("DEBUG", __FILE__, __LINE__); LogPrintf (MSG); } while (0)
#define DEBUGF(LEVEL, FMT) do { if (envDebug >= LEVEL) LogPara ("DEBUG", __FILE__, __LINE__); LogPrintf FMT; } while (0)
#else
#define DEBUG(LEVEL, MSG) do {} while (0)
#define DEBUGF(LEVEL, FMT) do {} while (0)
#endif

#define INFO(MSG) { LogPara ("INFO", __FILE__, __LINE__); LogPrintf (MSG); }
#define INFOF(FMT) { LogPara ("INFO", __FILE__, __LINE__); LogPrintf FMT; }

#define WARNING(MSG) { LogPara ("WARNING", __FILE__, __LINE__); LogPrintf (MSG); }
#define WARNINGF(FMT) { LogPara ("WARNING", __FILE__, __LINE__); LogPrintf FMT; }

#define ERROR(MSG) { LogPara ("ERROR", __FILE__, __LINE__); LogPrintf (MSG); exit (3); }
#define ERRORF(FMT) { LogPara ("ERROR", __FILE__, __LINE__); LogPrintf FMT; exit (3); }

#define ASSERT(COND) { if (!(COND)) ERROR("Assertion failed") }





// ***************** Mediastreamer driver ******************

// Important note on the design (2015-05-22)
//
// It seems that SDL2 calls from different treads are not allowed if GPU acceleration
// is to be used, even if SDL2 calls are properly synchronized with a mutex.
// Unfortunately, the video images are delivered by a background thread from
// mediastreamer2 ('MSFilterDesc::process'). Hence, this module is designed as follows:
//
// 1. Only the main thread is allowed to call SDL2 functions.
//
// 2. Images are passed from the background to the main thread by copying them
//    to 'pic[]', the structures are protected by the 'picMutex'.
//
// Unfortunately, this involves one additional copy operation for each frame.
//
// The pictures are held in global variables => only one 'CPhone' is allowed
// to use video.


// Synchronized copies of up-to-date images in 2 streams...
static CPhone *picPhone = NULL;     // phone to which the following fields belong (there can be only one!)
static pthread_mutex_t picMutex = PTHREAD_MUTEX_INITIALIZER;   // Mutex for 'pic' and 'picChanged'
static MSPicture pic[2];
static bool picChanged[2];



// ***** MSPicture helpers *****


static void MSPictureInit (MSPicture *pic) {
  int n;

  pic->w = pic->h = 0;
  for (n = 0; n < 4; n++) pic->strides[n] = 0;
  for (n = 0; n < 4; n++) pic->planes[n] = NULL;
}


static void MSPictureCopyFrom (MSPicture *pic, MSPicture *src) {
  // This function must only be called for 'pic' elements created by 'MSPictureInit'!
  int n, bytes[4];

  if (!src) {
    // Clear the picture ...
    if (pic->planes[0]) free (pic->planes[0]);
    MSPictureInit (pic);
  }
  else {
    if ((pic->w != src->w) || (pic->h != src->h)
        || (pic->strides[0] != src->strides[0]) || (pic->strides[1] != src->strides[1])
        || (pic->strides[2] != src->strides[2]) || (pic->strides[3] != src->strides[3])) {
      // Prepare for new format...
      MSPictureCopyFrom (pic, NULL);    // Clear picture
      pic->w = src->w;                  // Set parameters...
      pic->h = src->h;
      for (n = 0; n < 4; n++) pic->strides[n] = src->strides[n];
    }

    for (n = 0; n < 4; n++) bytes[n] = pic->h * pic->strides[n];

    if (pic->planes[0] == NULL) {
      // Allocate memory...
      pic->planes[0] = (uint8_t *) malloc (bytes[0] + bytes[1] + bytes[2] + bytes[3]);
      for (n = 1; n < 4; n++) pic->planes[n] = pic->planes[n-1] + bytes[n-1];
      //printf ("### (Re)allocating: %08x\n", (uint32_t) pic->planes[0]);
    }

    //printf ("### Copying: %08x\n", (uint32_t) pic->planes[0]);
    if (pic->h) for (n = 0; n < 4; n++) if (src->strides[n])
      memcpy (pic->planes[n], src->planes[n], pic->h / (pic->strides[0] / pic->strides[n]) * pic->strides[n]);
        // It seems that planes with a smaller (half) stride than plane #0 also have
        // a smaller y resolution. For this reason, the height is devided in the same
        // ratio. I do not know if this assumption is true. However, using 'pic->h'
        // for all planes leads to eventual segmentation faults.
  }
}


static inline void MSPictureClear (MSPicture *pic) {
  MSPictureCopyFrom (pic, NULL);
}



// ***** Display driver *****


static void MsDisplayReset () {
  int n;

  pthread_mutex_lock (&picMutex);
  for (n = 0; n < 2; n++) {
    MSPictureClear (&pic[n]);
    picChanged[n] = true;
  }
  pthread_mutex_unlock (&picMutex);
}


static void MsDisplayProcess (MSFilter *f) {
  MSPicture streamPic;
  mblk_t *inp;
  int n;

  for (n = 0; n < 2; n++) {
    if (f->inputs[n]) {
      if ( (inp = ms_queue_peek_last (f->inputs[n])) ) {
        if (ms_yuv_buf_init_from_mblk (&streamPic, inp) == 0) {
          pthread_mutex_lock (&picMutex);
          MSPictureCopyFrom (&pic[n], &streamPic);
          picChanged[n] = true;
          pthread_mutex_unlock (&picMutex);
        }
      }
      ms_queue_flush (f->inputs[n]);
    }
    else {
      pthread_mutex_lock (&picMutex);
      MSPictureClear (&pic[n]);
      picChanged[n] = true;
      pthread_mutex_unlock (&picMutex);
    }
  }
}




// ***** Init/Done *****


static MSFilterDesc msDisplayDesc;


static inline void MsInit (MSFactory *msFactory) {

  // Add own device info for P3110...
  //ms_devices_info_add (ms_factory_get_devices_info (msFactory), "samsung", "GT-P3110", "omap4", DEVICE_HAS_BUILTIN_AEC_CRAPPY, 0, 0);
      // 2017-01-04: This does not work, we had to add this information to
      //            'external/linphone/src/submodules/linphone/mediastreamer2/src/audiofilters/devices.c'
      //            and to rebuilt liblinphone!!

  // Add own video output filter...
  memset (&msDisplayDesc, 0, sizeof (msDisplayDesc));
  msDisplayDesc.id = MS_FILTER_PLUGIN_ID;
  msDisplayDesc.name = "Home2lDisplay";
  msDisplayDesc.text = "A custom video display for 'home2l' and 'phone2l'";
  msDisplayDesc.category = MS_FILTER_OTHER;
  msDisplayDesc.ninputs = 2;
  msDisplayDesc.noutputs = 0;
  msDisplayDesc.process = MsDisplayProcess;

  //~ ms_filter_register (&msDisplayDesc);      // required in 2.5.1; but obsolete and not working in 3.2.1
  ms_factory_register_filter (msFactory, &msDisplayDesc);    // required in 3.2.1

  // Init video output filter...
  MSPictureInit (&pic[0]);
  MSPictureInit (&pic[1]);
  picChanged[0] = picChanged[1] = false;
}





// *************************** CPhone: Actions *********************************


static void ShowRegistrationState (LinphoneCore *lpCore) {
  LinphoneProxyConfig *lpProxyConfig = linphone_core_get_default_proxy_config (lpCore);

  DEBUGF (1, ("[Linphone] linphone_core_get_identity: %s", linphone_core_get_identity (lpCore)));
  DEBUGF (1, ("[Linphone] linphone registration state: %s", linphone_registration_state_to_string (linphone_proxy_config_get_state (lpProxyConfig))));
}


static void LpUpdateMediaSelection (CPhone *phone, unsigned mask) {
  unsigned mediaSelected;
  bool on;

  mediaSelected = phone->GetMediaSelected ();
  if (mask & pmAudioIn) {
    on = mediaSelected & pmAudioIn ? 1 : 0;
    linphone_core_enable_mic (PHONE_LIBDATA (phone).lpCore, on);
    // call? -> does not seem to be necessary
  }
  //if (mask & pmAudioOut) {  // not implemented
  //  on = mediaSelected & pmAudioOut ? 1 : 0;
  //  // core? -> not implemented
  //  // call? -> not implemented
  //}
  if (mask & pmVideoIn) {
    on = mediaSelected & pmVideoIn ? 1 : 0;
    linphone_core_enable_video_capture (PHONE_LIBDATA (phone).lpCore, on);
    linphone_core_enable_video_preview (PHONE_LIBDATA (phone).lpCore, 0); // on);
    if (PHONE_LIBDATA (phone).lpCall[0]) linphone_call_enable_camera (PHONE_LIBDATA (phone).lpCall[0], on);
  }
  if (mask & pmVideoOut) {
    on = (mediaSelected & pmVideoOut) ? 1 : 0;
    linphone_core_enable_video_display (PHONE_LIBDATA (phone).lpCore, on && picPhone == phone);
    // call: change during call not implemented yet
  }
}




// ***** General *****


bool CPhone::Dial (const char *uri) {
  ShowRegistrationState (LIBDATA.lpCore);
  if (!linphone_core_invite (LIBDATA.lpCore, uri)) {
    WARNINGF(("'linphone_core_invite' failed (URL = '%s')", uri));
    return false;
  }
  return true;
}


bool CPhone::AcceptCall () {
  // Only the primary call can be accepted
  if (!LIBDATA.lpCall[0]) {
    WARNING("'AcceptCall' invoked without a pending incoming call");
    return false;
  }
  if (linphone_core_accept_call (LIBDATA.lpCore, LIBDATA.lpCall[0]) != 0) {
    WARNING("'linphone_core_accept_call' failed");
    return false;
  }
  LpUpdateMediaSelection (this, pmAll);
  return true;
}


bool CPhone::Hangup () {
  if (LIBDATA.lpCall[0]) {
    if (linphone_core_terminate_call (LIBDATA.lpCore, LIBDATA.lpCall[0]) != 0) {
      WARNING("'linphone_core_terminate_call' failed");
      return false;
    }
  }
  else if (LIBDATA.lpCall[1]) {
      // No current, but a secondary call: Cancel a transfer action...
      linphone_core_resume_call (LIBDATA.lpCore, LIBDATA.lpCall[1]);
  }
  else {
    DEBUG(1, "'Hangup' invoked without an active call");
    return false;
  }
  return true;
}


bool CPhone::CancelAllCalls () {
  if (linphone_core_terminate_all_calls (LIBDATA.lpCore) != 0) {
    WARNING("'linphone_core_terminate_all_calls' failed");
    return false;
  }
  return true;
}



// ***** DTMF *****


bool CPhone::SendDtmf (const char *dtmfSequence) {
  return linphone_call_send_dtmfs (LIBDATA.lpCall[0], (char *) dtmfSequence) == 0;
    // TBD: is 'dtmfSquence' modified in liblinphone??
}



// ***** Transfers *****


bool CPhone::PrepareTransfer () {
  if (state != psInCall) {
    WARNING("'CPhone::PrepareTransfer' called without existing call");
    return false;
  }
  linphone_core_pause_call (LIBDATA.lpCore, LIBDATA.lpCall[0]);
  return true;
}


bool CPhone::CompleteTransfer () {
  //~ INFO ("### CPhone::Transfer");
  if (!LIBDATA.lpCall[0] || !LIBDATA.lpCall[1]) {
    WARNING("'CPhone::Transfer' called without two existing calls");
    return false;
  }
  if (state == psTransferInCall) {
    // Destination has picked up: We complete as a normal attended transfer.
    if (linphone_core_transfer_call_to_another (LIBDATA.lpCore, LIBDATA.lpCall[1], LIBDATA.lpCall[0]) != 0) {
      WARNING("'linphone_core_transfer_call_to_another' failed");
      return false;
    }
  }
  else if (state == psTransferDialing) {
    // Destination has not yet picked up: Go to the "auto-pickup" state.
    ReportState (psTransferAutoComplete);
  }

  return true;
}





// *************************** CPhone: Media selection *************************


void CPhone::SelectMedia (unsigned selected, unsigned mask) {
  unsigned changed, _mediaSelected;

  _mediaSelected = (selected & mask) | (mediaSelected & ~mask);
  changed = mediaSelected ^ _mediaSelected;
  //~ INFOF (("### SelectMedia: %x -> %x", mediaSelected, _mediaSelected));
  mediaSelected = _mediaSelected;
  LpUpdateMediaSelection (this, changed);
}





// *************************** CPhone: State retrieval *************************


// ***** Linphone callbacks *****


static void LpCbDisplayStatus (LinphoneCore *lpCore, const char *msg) {
  INFOF(("(liblinphone) %s", msg));
  ((CPhone *) linphone_core_get_user_data (lpCore))->ReportInfo (msg);
}


//~ static void LpCbDisplayWarning (LinphoneCore *lpCore, const char *msg) {
  //~ WARNINGF(("(liblinphone) %s\n", msg));
//~ }


static void LpCbCallStateChanged (LinphoneCore *lpCore, LinphoneCall *call, LinphoneCallState callState, const char *msg) {
  // This function does the following in this order (as applicable):
  // 1. Update 'lpCall[0]' and 'lpCall[1]', (un)ref calls etc.
  // 2. Set the new phone state
  // 3. Perform other Linphone actions. This must always be the last step, since these
  //    may recursively call this function again.
  CPhone *phone;
  LinphoneCall **phoneCalls;
  int callId;
  char *from;

  phone = (CPhone *) linphone_core_get_user_data (lpCore);
  phoneCalls = PHONE_LIBDATA (phone).lpCall;

  // Find 'call' in both slots and eventually assign it to the first empty slot...
  if (call == phoneCalls[0]) callId = 0; else if (call == phoneCalls[1]) callId = 1;
  else if (!phoneCalls[0]) callId = 0; else if (!phoneCalls[1]) callId = 1;
  else ERROR ("Already two active calls, received 'callStateChanged' for a third one");

  from = linphone_call_get_remote_address_as_string (call);

  switch (callState) {

    case LinphoneCallEnd:
    case LinphoneCallError:
      INFOF(("LpCbCallStateChanged #%i (%x/%x): LinphoneCallEnd|LinphoneCallError", callId, phoneCalls[0], phoneCalls[1]));
      if (phoneCalls[callId]) {   // ignore calls not yet stored in here (e.g. rejected incomings)
        linphone_call_unref (phoneCalls[callId]);
        phoneCalls[callId] = NULL;
        if (callId == 0 && phoneCalls[1] == NULL) {
          // Simple case: The single call ended
          //~ INFO ("### primary and only call ended");
          phone->ReportState (psIdle);
          MsDisplayReset ();
        }
        else if (callId == 1) {
          // The paused call ended
          //~ INFO ("### paused call ended");
          // We assume that we are in a transfer and want to hangup to
          // let the two partners talk alone. Hence, we terminate the
          // primary call and do not change the phone state, because this
          // will be done later...
          linphone_core_terminate_call (lpCore, phoneCalls[0]);
        }
        else {
          // The primary call ended, but there is still a paused one: Resume this one...
          //~ INFO ("### primary call ended, resuming paused one");
          linphone_core_resume_call (lpCore, phoneCalls[1]);
            // state will be set later in event 'LinphoneCallResuming'
        }
      }

      break;

    case LinphoneCallIncomingReceived:
      INFOF(("LpCbCallStateChanged #%i (%x/%x): LinphoneCallIncomingReceived", callId, phoneCalls[0], phoneCalls[1]));
      // if a call already active: reject, otherwise query desired action...
      switch ((phoneCalls[0] || phoneCalls[1]) ? psIdle : phone->GetIncomingCallAction ()) {
        case psRinging:
          phoneCalls[0] = linphone_call_ref (call);
          //~ phone->ReportInfo ("Receiving call from %s", from);
          phone->ReportState (psRinging);
          break;
        case psInCall:
          phoneCalls[0] = linphone_call_ref (call);
          phone->AcceptCall ();
          break;
        default:
          linphone_core_decline_call (lpCore, call, LinphoneReasonBusy);
          break;
      };
      break;

    case LinphoneCallOutgoingInit:
      //~ INFOF(("LpCbCallStateChanged #%i (%x/%x): LinphoneCallOutgoingInit", callId, phoneCalls[0], phoneCalls[1]));
      //~ phone->ReportInfo ("Establishing call to %s", from);
      ASSERT (phoneCalls[0] == NULL);
      phoneCalls[0] = linphone_call_ref (call);
      phone->ReportState (phoneCalls[1] ? psTransferDialing : psDialing);
      break;

    case LinphoneCallConnected:
      //~ INFOF(("LpCbCallStateChanged #%i (%x/%x): LinphoneCallConnected", callId, phoneCalls[0], phoneCalls[1]));
      //~ phone->ReportInfo ("Connected to %s.", from);
      if (phone->GetState () == psTransferAutoComplete) {
        if (linphone_core_transfer_call_to_another (lpCore, phoneCalls[1], phoneCalls[0]) != 0)
          WARNING("'linphone_core_transfer_call_to_another' failed");
      }
      else
        phone->ReportState (phoneCalls[1] ? psTransferInCall : psInCall);
      break;

    case LinphoneCallPaused:
      //~ INFOF(("LpCbCallStateChanged #%i (%x/%x): LinphoneCallPaused", callId, phoneCalls[0], phoneCalls[1]));
      //printf ("### cb_call_state_changed: Call %i with %s is now paused.\n", id, from);
      if (callId == 0) {
        ASSERT (phoneCalls[1] == NULL);
        phoneCalls[1] = phoneCalls[0];
        phoneCalls[0] = NULL;
        phone->ReportState (psTransferIdle);
      }
      break;

    case LinphoneCallResuming:
      //~ INFOF(("LpCbCallStateChanged #%i (%x/%x): LinphoneCallResuming", callId, phoneCalls[0], phoneCalls[1]));
      if (callId == 1 && phoneCalls[0] == NULL) {
        phoneCalls[0] = phoneCalls[1];
        phoneCalls[1] = NULL;
        phone->ReportState (psInCall);
      }
      break;

    /*
    case LinphoneCallStreamsRunning:
      INFOF(("(linphone) Media streams established with %s (%s).\n", from,
            linphone_call_params_video_enabled (linphone_call_get_current_params (call)) ? "video" : "audio"));
      break;

    case LinphoneCallPausedByRemote:
      printf ("### cb_call_state_changed: Call %i has been paused by %s.\n",id,from);
      break;

    case LinphoneCallOutgoingProgress:
      phone->ReportInfo ("Call to %s in progress.", from);
      break;

    case LinphoneCallOutgoingRinging:
      phone->ReportInfo ("Call to %s ringing.", from);
      break;

    case LinphoneCallOutgoingEarlyMedia:
      phone->ReportInfo ("Call to %s early media.", from);
      break;
    */

    /*
    case LinphoneCallUpdatedByRemote:
      printf ("### cb_call_state_changed: Call %i with %s updated.\n", id, from);
      cp = linphone_call_get_current_params(call);
      // TBD: auto-start camera?
      if (!linphone_call_camera_enabled (call) && linphone_call_params_video_enabled (cp)){
        printf ("Far end requests to share video.\nType 'camera on' if you agree.\n");
      }
      break;
    case LinphoneCallPausing:
      printf ("### cb_call_state_changed: Pausing call %i with %s.\n", id, from);
      break;
    case LinphoneCallPaused:
      printf ("### cb_call_state_changed: Call %i with %s is now paused.\n", id, from);
      break;
    case LinphoneCallPausedByRemote:
      printf ("### cb_call_state_changed: Call %i has been paused by %s.\n",id,from);
      break;
    case LinphoneCallResuming:
      printf ("### cb_call_state_changed: Resuming call %i with %s.\n", id, from);
      break;
    */

    default:
      break;
  }

  ms_free (from);
}


static void LpCbDtmfReceived (LinphoneCore *lpCore, LinphoneCall *call, int dtmf) {
  CPhone *phone = (CPhone *) linphone_core_get_user_data (lpCore);
  phone->OnDtmfReceived ((char) dtmf);
}



// ***** Callbacks *****


void CPhone::OnPhoneStateChanged (EPhoneState oldState) {
  if (cbPhoneStateChanged) cbPhoneStateChanged (cbPhoneStateChangedData, oldState);
}


void CPhone::OnInfo (const char *msg) {
  if (cbInfo) cbInfo (cbInfoData, msg);
}


void CPhone::OnDtmfReceived (char dtmf) {
  if (cbDtmfReceived) cbDtmfReceived (cbDtmfReceivedData, dtmf);
}



// ***** Information *****


int CPhone::GetCallDuration (int callId) {
  if (LIBDATA.lpCall[callId]) return linphone_call_get_duration (LIBDATA.lpCall[callId]);
  return 0;
}


const char *CPhone::GetPeerUrl (int callId) {
  if (LIBDATA.lpCall[callId]) return linphone_call_get_remote_address_as_string (LIBDATA.lpCall[callId]);
  return NULL;
}





// *************************** CPhone: Video stream ****************************


TPhoneVideoFrame *CPhone::VideoLockFrame (int streamId) {
  MSPicture *p;

  pthread_mutex_lock (&picMutex);

  if (streamId < 0) return NULL;
  p = &pic[streamId];
  if (!p->planes[0] || !p->w || !p->h) return NULL;

  picInfo.changed = picChanged[streamId];
  picChanged[streamId] = false;
  picInfo.w = p->w;
  picInfo.h = p->h;
  picInfo.planeY = p->planes[0];
  picInfo.pitchY = p->strides[0];
  picInfo.planeU = p->planes[1];
  picInfo.pitchU = p->strides[1];
  picInfo.planeV = p->planes[2];
  picInfo.pitchV = p->strides[2];
  return &picInfo;
}


void CPhone::VideoUnlock () {
  pthread_mutex_unlock (&picMutex);
}





// *************************** CPhone: Internal ********************************


void CPhone::ReportState (EPhoneState _state) {
  EPhoneState oldState;

  DEBUGF (1, ("CPhone::ReportState: %i -> %i", state, _state));
  if (_state != state) {
    LpUpdateMediaSelection (this, pmAll);   // TBD: Does this fix the problem that the mute state was not initialized correctly?

    oldState = state;
    state = _state;
    OnPhoneStateChanged (oldState);
  }
}


void CPhone::ReportInfo (const char *fmt, ...) {
  char *buf;

  va_list ap;
  va_start (ap, fmt);
  vasprintf (&buf, fmt, ap);
  va_end (ap);
  OnInfo (buf);
  free (buf);
}





// *************************** CPhone: Setting up ******************************


static void CbOrtpLogHandler (const char *domain, OrtpLogLevel lev, const char *fmt, va_list args) {
  char buf[200];

  vsnprintf (buf, 199, fmt, args);
  LogPara ("DEBUG", "[linphone]", lev);
  //~ LogPara ((lev & ORTP_DEBUG) ? "DEBUG" :
            //~ (lev & ORTP_MESSAGE) ? "INFO" :
            //~ (lev & ORTP_WARNING) ? "WARNING" :
            //~ "ERROR",
            //~ "[linphone]", lev);
  LogPrintf (buf);
}


void CPhone::Setup (const char *agentName, int _mediaSelected, int withLogging,
                    const char *tmpDir,
                    const char *lpLinphoneRcFile) {
  static LinphoneCoreVTable lpCoreVtable;
  char buf[256];
  const char *lpConfigFile;

  // Setup logging...
  linphone_core_set_log_level ((OrtpLogLevel) (withLogging ? ORTP_MESSAGE | ORTP_WARNING | ORTP_ERROR | ORTP_FATAL | ORTP_TRACE : 0));
  linphone_core_set_log_handler (CbOrtpLogHandler);

  // Reset object if it was already used before...
  if (LIBDATA.lpCore) {
    Done ();
    Init ();
  }

  // Prepare callback table...
  memset (&lpCoreVtable, 0, sizeof (lpCoreVtable));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  // As of LP 3.2.1, the fields 'display_status' and 'display_warning' are declared deprecated,
  // but no replacement appears to exist...
  lpCoreVtable.display_status = LpCbDisplayStatus;
  lpCoreVtable.display_warning = LpCbDisplayStatus;
#pragma GCC diagnostic pop
  lpCoreVtable.call_state_changed = LpCbCallStateChanged;
  lpCoreVtable.dtmf_received = LpCbDtmfReceived;

  // Create ".linphone.ecstate" file to allow persistent state of the echo cancellation...
  if (tmpDir) {
    int fd;

    // Touch ".linphone.ecstate" file...
    snprintf (buf, sizeof(buf), "%s/.linphone.ecstate", tmpDir);
    buf[sizeof(buf)-1] = '\0';
    fd = open (buf, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (fd < 0) WARNINGF (("Unable to touch '%s': %s", buf, strerror (errno)))
    else close (fd);

    // Setup config file name in 'buf' (variable config file appears to be necessary for EC)...
    snprintf (buf, sizeof(buf), "%s/.linphonerc", tmpDir);
    buf[sizeof(buf)-1] = '\0';
    lpConfigFile = buf;

    // WORKAROUND [2017-09-01]: The variable config file 'lpConfigFile' is getting longer and longer over time,
    //   and after some time, linphone appears to crash or exhaust memory when reading it. For this reason,
    //   we remove it here on initialization.
    unlink (lpConfigFile);
  }
  else lpConfigFile = NULL;

  // Create the 'lpCore' object...
  LIBDATA.lpCore = linphone_core_new (&lpCoreVtable, lpConfigFile, lpLinphoneRcFile, this);
  linphone_core_set_user_agent (LIBDATA.lpCore, agentName, buildVersion);

  // Initialize the mediastreamer part & activate own display filter...
  if (_mediaSelected & pmVideoOut) {    // video out selected...
    if (!picPhone) {
      picPhone = this;
      MsInit (linphone_core_get_ms_factory (LIBDATA.lpCore)); // TBD: Move this out of the 'if' clause!!
      linphone_core_set_video_display_filter (LIBDATA.lpCore, "Home2lDisplay");
      //~ printf ("### linphone_core_get_video_display_filter (): %s\n", linphone_core_get_video_display_filter (lpc));
      //~ printf ("### video_stream_get_default_video_renderer (): %s\n", video_stream_get_default_video_renderer ());
    }
    else {
      WARNING ("A 'CPhone' object trying a activate video output, which is already acquired by another phone - not activating!");
    }
  }

  // Set selected media...
  mediaSelected = _mediaSelected;
  LpUpdateMediaSelection (this, pmAll);

  // Set parameters...
  if (envPhoneRingbackFile) linphone_core_set_ringback (LIBDATA.lpCore, envPhoneRingbackFile);
  //~ linphone_core_set_ring_during_incoming_early_media (LIBDATA.lpCore, 0);     // no ringing during early media
  if (envPhonePlayFile) linphone_core_set_play_file (LIBDATA.lpCore, envPhonePlayFile);

  // Configure DTMF sending...
  //~ linphone_core_set_use_info_for_dtmf (LIBDATA.lpCore, 1);
  //~ linphone_core_set_use_rfc2833_for_dtmf (LIBDATA.lpCore, 1);

  // Trigger a state change...
  ReportState (psIdle);
}


bool CPhone::Register (const char *identity, const char *secret) {
  LinphoneProxyConfig *proxy_cfg;
  LinphoneAddress *from;
  LinphoneAuthInfo *info;
  const char *server_addr;

  // Create proxy config...
  proxy_cfg = linphone_proxy_config_new();

  // Parse identity & set auth info...
  from = linphone_address_new (identity);
  info = linphone_auth_info_new (linphone_address_get_username (from), NULL, secret, NULL, NULL, NULL);
  linphone_core_add_auth_info (LIBDATA.lpCore, info);
  linphone_auth_info_destroy (info);

  // Configure proxy entries...
  linphone_proxy_config_set_identity (proxy_cfg, identity); // set identity with user name and domain; may be different from 'identity' to 'linphone_auth_info_new'?
  server_addr = linphone_address_get_domain (from); // extract domain address from identity
  linphone_proxy_config_set_server_addr (proxy_cfg, server_addr); // we assume domain = proxy server address
  linphone_proxy_config_enable_register (proxy_cfg, true); // activate registration for this proxy config
  linphone_address_destroy (from); // release resource
  linphone_core_add_proxy_config (LIBDATA.lpCore, proxy_cfg); // add proxy config to linphone core
  linphone_core_set_default_proxy (LIBDATA.lpCore, proxy_cfg); // set to default proxy

  // Return...
  return true;      // not quite correct...
}


void CPhone::SetCamRotation (int rot) {
  if (rot != 0) linphone_core_set_device_rotation (LIBDATA.lpCore, rot);
}


void CPhone::DumpSettings () {
  const char **soundDevices;
  int n;

  soundDevices = linphone_core_get_sound_devices (LIBDATA.lpCore);
  for (n = 0; soundDevices[n]; n++)
    INFOF (("### sound device #%i = '%s'", n, soundDevices[n]));
  INFOF (("### ringer device = '%s'", linphone_core_get_ringer_device (LIBDATA.lpCore)));
  INFOF (("### playback device = '%s'", linphone_core_get_playback_device (LIBDATA.lpCore)));
  INFOF (("### capture device = '%s'", linphone_core_get_capture_device (LIBDATA.lpCore)));
}





// *************************** CPhone: Init/Done/Iterate ***********************


void CPhone::Init () {
  ASSERT (sizeof (libData) >= sizeof (TPhoneData));

  LIBDATA.lpCore = NULL;
  LIBDATA.lpCall[0] = LIBDATA.lpCall[1] = NULL;

  state = psNone;
  incomingAction = psRinging;

  cbPhoneStateChanged = NULL;
  cbInfo = NULL;
  cbPhoneStateChangedData = cbInfoData = NULL;
}


void CPhone::Done () {
  int n;

  if (LIBDATA.lpCore) {
    CancelAllCalls ();
    for (n = 0; n < 2; n++) if (LIBDATA.lpCall[n]) linphone_call_unref (LIBDATA.lpCall[n]);
    linphone_core_destroy (LIBDATA.lpCore);
    Init ();  // Reset everything to avoid problems when 'Done' is called multiple times
  }
}


void CPhone::Iterate () {
  linphone_core_iterate (LIBDATA.lpCore);

  //~ if (state == psInCall) INFOF (("Audio jitter buffer size = %i ms\n", linphone_core_get_audio_jittcomp (LIBDATA.lpCore)));

  //~ if (state == psInCall)
    //~ INFOF (("Echo cancelation = %i/%i;   Echo limiter = %i/%i",
            //~ linphone_call_echo_cancellation_enabled (LIBDATA.lpCall[0]), linphone_core_echo_cancellation_enabled (LIBDATA.lpCore),
            //~ linphone_call_echo_limiter_enabled (LIBDATA.lpCall[0]), linphone_core_echo_limiter_enabled (LIBDATA.lpCore)));
}
