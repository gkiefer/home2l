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


#ifndef _PHONE_
#define _PHONE_


/** @file
 *
 * @addtogroup common_phone
 *
 * This module provides a wrapper for IP phone functionality, which is presently
 * used by the *Home2L* applications *WallClock* and *DoorMan*.
 * Presently, two possible backends are supported: *Linphone* and *PJSIP*.
 * Depending on the backend to be used, the following .C files have to be
 * compiled, which contain alternative implementations to @ref phone.H :
 * - *Linphone*: `phone.C` and `phone-linphone.C`
 * - *PJSIP*: `phone.C` and `phone-pjsip.C`
 *
 * Furthermore, the respective external backend libraries have to be available
 * (see directory `external/linphone` or `external/pjsip`, respectively).
 *
 * @{
 */


/* Unfortunately, 'liblinphone' and friends make some definitions (e.g. for bool)
 * that conflict with other libraries and the main program. Hence, this module also acts
 * as a demilitarized zone. The header must not include any home2l-related basic headers.
 */


#include <stdint.h>
#include <pthread.h>




// *************************** Environment settings ****************************


// (read-only; primarily needed by the phone drivers themselves)
extern const char *envPhoneLinphonerc;

extern int envPhoneSipPort;

extern const char *envPhoneAudioDriver;
extern const char *envPhoneAudioDevice;
extern const char *envPhoneAudioInDevice;
extern const char *envPhoneAudioOutDevice;
extern float envPhoneAudioInGain;
extern float envPhoneAudioOutGain;
extern const char *envPhoneAudioCodec;

extern int envPhoneEchoTail;
extern int envPhoneEchoAlgo;
extern int envPhoneEchoAggressiveness;
extern bool envPhoneEchoNoiseSuppression;

extern const char *envPhoneVideoDriver;
extern const char *envPhoneVideoDevice;
extern const char *envPhoneVideoCodec;

extern const char *envPhoneRegister;
extern const char *envPhoneSecret;

extern const char *envPhoneRingbackFile;
extern const char *envPhoneRingbackFileKey;
extern float envPhoneRingbackLevel;

extern int envPhoneRotation;





// *************************** Video driver(s) *********************************


/// @brief Video frame pixel format.
/// The values refer to the respective SDL pixel format types of a similar name.
/// (see 'SDL_PixelFormatEnum' and the 'SDL_PIXELFORMAT_*' values in the SDL2 documentation).
enum EPhoneVideoFormat {
  pvfNone = 0,
  pvfABGR8888,
  pvfBGR24,
  pvfARGB8888,
  pvfRGB24,
  pvfYUY2,
  pvfUYVY,
  pvfYVYU,
  pvfIYUV,
  pvfYV12
};


const char *StrPhoneVideoFormat (EPhoneVideoFormat x);
  ///< Get a readable string for the format.


/// @brief Data structure to pass video frames to the UI.
struct TPhoneVideoFrame {
  bool changed;              ///< Set on change, reset on fetching.

  /// @name Frame format.
  ///
  /// @{
  EPhoneVideoFormat format;  ///< Pixel format.
  int w, h;                  ///< Image dimensions.
  /// @}

  /// @name Frame data references.
  ///
  /// The following fields describe an image either by a single chunk suitable for 'SDL_UpdateTexture()'
  /// or in separate planes as accepted by 'SDL_UpdateYUVTexture()'. The plane references are
  /// used if 'data == NULL' and 'planeY != NULL'.
  /// @{
  uint8_t *data;                      ///< Data, if all planes contiguous in memory (for 'SDL_UpdateTexture()')
  int pitch;                          ///< number of bytes between rows (for 'data != NULL')

  uint8_t *planeY, *planeU, *planeV;  ///< color planes (Y, U, V) (for 'SDL_UpdateYUVTexture()')
  int pitchY, pitchU, pitchV;         ///< number of bytes between rows (for 'SDL_UpdateYUVTexture()')
  /// @}
};


static inline void PhoneVideoFrameInit (TPhoneVideoFrame *vf) {
  vf->changed = false;

  vf->format = pvfNone;
  vf->w = vf->h = 0;

  vf->data = NULL;
  vf->pitch = 0;

  vf->planeY = vf->planeU = vf->planeV = NULL;
  vf->pitchY = vf->pitchU = vf->pitchV = 0;
}





// *************************** CPhone ******************************************

// The class 'CPhone' represents a telephone. It contains two "Call"
// objects (#0 and #1). This is to support the following scenarios:
//
// a) Simple calls: only call #0 is used, #1 remains inactive.
//
// b) Transfer calls: #0 is the new call (or none), #1 the paused one.
//    Swapping between slots #0 and #1 happens on pausing/resuming.
//
// c) Camera preview for the door phone using a second call is presently
//    unsupported.
//    Current solution for door cam: Use primary call (no second call),
//    accept with mic muted. Drawback: Visitor recognizes accept action.
//
// d) Machines with multiple logical phones (door/gate phone) may use multiple
//    'CPhone' objects.
//
// NOTE on the use of audio / camera devices by other program modules / threads
// (e.g. application-programmed ringing, music playback):
//
// 1. To avoid conflicts, other modules must ensure that during the invocation
//    of any action ('Dial', 'AcceptCall', enabling auto-acceptance),
//    such devices are closed by the other modules.
//
// 2. Devices may be used by the other modules only after a "device-permitting"
//    state has been reported again via 'ReportState ()'. The "device-permitting"
//    states are: 'psIdle', 'psRinging'.
//
// To avoid race conditions, the following contract applies:
// - After returning from 'CPhone::Dial ()', the state is already 'psDialing'
//   (i.e. 'ReportState ()' is called from 'CPhone::Dial ()').
// - 'AcceptCall ()' is only called when in state 'psRinging'.
// - If 'SetIncomingCallAction (psInCall)' is called, the application does
//   not use any device at all.


/// @brief Current phone state.
enum EPhoneState {
  psNone = 0,             ///< No state (e.g. phone not initialized).
  psIdle,                 ///< Phone is idle.
  psDialing,              ///< Phone is dialing, remote end may be ringing.
  psRinging,              ///< This phone is ringing.
  psInCall,               ///< A call is active.
  psTransferIdle,         ///< Primary call is paused, UI may query a new number to transfer to.
  psTransferDialing,      ///< Primary call is paused, a new call is in the dialing state.
  psTransferAutoComplete, ///< Same as 'psTransferDialing', but on remote pickup the transfer will complete automatically.
  psTransferInCall        ///< Primary call is paused, secondary call is active; hanging up results in completing the transfer.
};


static inline bool PhoneStateIsDevicePermitting (EPhoneState s) { return s == psIdle || s == psRinging; }
  ///< @brief Return whether the caller is allowed to use the media (audio/video) devices that may
  /// otherwise be occupied by the respective phone.


/// @brief Media mask.
enum EPhoneMedia {
  pmNone = 0,         ///< Nothing
  pmAudioIn = 1,      ///< Microphone
  pmAudioOut = 2,     ///< Speaker
  pmAudio = 3,        ///< Microphone and Speaker
  pmVideoIn = 4,      ///< Camera
  pmVideoOut = 8,     ///< Display
  pmVideo = 12,       ///< Camera and Display
  pmAll = 15          ///< Everything
};
#define pmOff pmNone
#define pmOn pmAll


/// @brief IP Telephone.
///
/// **Note:** The instantiation of multiple instances is not fully supported in some backends.
class CPhone {
  public:
    CPhone () { Init (); }
    ~CPhone () { Done (); }
    void Init ();
    void Done ();

    /// @name General ...
    /// @{

    void Iterate ();    ///< @brief Iterate the backend and update the phone state; must be called regularly.

    /// @}


    /// @name Setup ...
    /// @{

    void Setup (const char *agentName, int _mediaSelected, int withLogging,
                const char *tmpDir);
      ///< @brief Setup phone object.
      ///
      /// @param agentName is the reference to the name of the phone agent. The pointer must remain valid for
      ///        the life time of the object.
      /// @param _mediaSelected is set of initially enabled media.
      /// @param withLogging enables logging, and should be set to the 'envDebug' setting.
      /// @param tmpDir is the root temporary directory. It should be set to some path below EnvHome2lTmp().
      ///        To disallow the generation of temporary files (e.g. database for noise cancellation), NULL
      ///        may be passed here.
      ///
      /// A call to this function finally causes a "state changed" event from 'psNone' to 'psIdle'.
      /// If the callback handlers are set before, this can be caught for initialization purposes.
      ///
      /// A registration does not happen automatically here. To register, call Register() afterwards.
      ///
      /// **Note:** (2017-01-15) Persistent echo cancellation with *Linphone* requires a patched version
      ///        of *Linphone* (see `external/linphone`).
      ///

    bool Register (const char *identity = NULL, const char *secret = NULL);
      ///< @brief Register phone with a registrar.
      ///
      /// @param identity is a string containing the user name and eventually domain of the identity,
      ///     for example: "Joe Miller <joe@example.org>".
      /// @param secret is the password.
      ///
      /// Both arguments are references and must remain valid for the lifetime of the object.
      /// If NULL is passed for the identity, the default/main account defined by 'phone.register'
      /// and 'phone.secret' is registered.

    /// @}


    /// @name Actions ...
    ///   These are actions that can be initiated by the application.
    ///   Unless a 'Get...' method is present in this section, the
    ///   effect may not immediately visible, e.g. the phone state may
    ///   change somewhat later and will be returned by the backend via
    ///   the callback mechanisms below. By default, the functions return
    ///   'true' on success (= no immediate failure), otherwise they
    ///   print a warning message and return 'false'.
    /// @{

    bool Dial (const char *url);
      ///< @brief Dial the number or URL.
      ///
      /// If PrepareTransfer() was called previously,
      /// an eventually existing call will be paused automatically and
      /// the new peer is called as a future destination.
      ///
      /// **Note:** The caller must make sure that the audio/video devices is
      ///       not opened  before calling this method, since the phone
      ///       library may use it from now.
      ///
    bool AcceptCall ();
      ///< @brief Green button on the phone: Accept an incoming call.
      ///
      /// **Note:** The caller must make sure that the audio/video devices is
      ///       not opened  before calling this method, since the phone
      ///       library may use it from now.
      ///
    bool Hangup ();
      ///< @brief Red button on the phone: Cancel a call or a dialing process.
      /// If there is a paused call, it will be resumed and become the active call again.
      ///
      /// **Note:** The caller must not open the audio/video devices before a
      ///       "device-permitting" state (e.g. 'psIdle') is reported again.
      ///
    bool CancelAllCalls ();
      ///< @brief Cancel all active calls, try to reach the idle state as soon as possible.
      ///
      /// **Note:** The caller must not open the audio/video devices before a
      ///       "device-permitting" state (e.g. 'psIdle') is reported again.
      ///

    bool PrepareTransfer ();
      ///< @brief Pause (hold) the current call and set phone into the state 'psTransferIdle'
      /// to initiate an attended transfer. For an unattended transfer, call
      /// CompleteTransfer() immediately after dialing (Dial()).
    bool CompleteTransfer ();
      ///< @brief Transfer the paused call to the active one (complete the transfer).
      /// If called when still dialing, the auto-pickup state is entered automatically.

    bool SendDtmf (const char *dtmfSequence);
      ///< @brief Send a sequence of DTMF characters.

    /// @}


    /// @name Handling incoming calls ...
    /// @{

    void SetIncomingCallAction (EPhoneState _incomingAction) { incomingAction = _incomingAction; }
      ///< @brief Set decision on what to do with an incoming call in form of a desired next state:
      /// - 'psRinging': let it ring (default mode)
      /// - 'psInCall': automatically accept (e.g. for baby phone mode or for a door phone)
      /// - others: reject call
      ///
      /// **Note:** The caller must make sure that the audio/video devices are
      ///       not opened before calling this method with argument 'psInCall'.
    EPhoneState GetIncomingCallAction () { return incomingAction; }

    void SetAutoAccept () { SetIncomingCallAction (psInCall); }
    void SetAutoReject () { SetIncomingCallAction (psIdle); }
    void SetManualAccept () { SetIncomingCallAction (psRinging); }

    /// @}


    /// @name Media selection ...
    /// @{

    void SelectMedia (unsigned selected, unsigned mask = pmAll);
    unsigned GetMediaSelected () { return mediaSelected; }

    void SetMicOn (bool on) { SelectMedia (on ? pmOn : pmOff, pmAudioIn); }
    bool GetMicOn () { return GetMediaSelected () & pmAudioIn ? true : false; }
    void SetCamOn (bool on) { SelectMedia (on ? pmOn : pmOff, pmVideoIn); }
    bool GetCamOn () { return GetMediaSelected () & pmVideoIn ? true : false; }

    /// @}


    /// @name State retrieval ...
    ///   These methods and callback functions are used to retrieve
    ///   the actual state of the underlying phone.
    ///   Each callback can alternatively be installed by either overloading
    ///   a virtual method ( On...() ) or by passing a C function ( SetCb...() )
    ///   together with some arbitrary user data ('data').
    /// @{

    // Phone state...
    EPhoneState GetState () { return state; }   ///< @brief Get current phone state.

    // Callbacks...
    virtual void OnPhoneStateChanged (EPhoneState oldState);
    void SetCbPhoneStateChanged (void (*_cbPhoneStateChanged) (void *, EPhoneState), void *data = NULL) { cbPhoneStateChanged = _cbPhoneStateChanged; cbPhoneStateChangedData = data; }
      ///< @brief Callback on a phone state change; the second parameter is the previous state.

    virtual void OnInfo (const char *msg);
    void SetCbInfo (void (*_cbInfo) (void *, const char *), void *data = NULL) { cbInfo = _cbInfo; cbInfoData = data; }
      ///< @brief Callback on the receipt of a new info message (should be displayed on the UI).

    virtual void OnDtmfReceived (char dtmf);
    void SetCbDtmfReceived (void (*_cbDtmfReceived) (void *, char), void *data = NULL) { cbDtmfReceived = _cbDtmfReceived; cbDtmfReceivedData = data; }
      ///< @brief Callback on the receipt of a DTMF character.

    // Information...
    int GetCallDuration (int callId = 0);
      ///< @brief Get call duration in seconds.
      /// @param callId is 0 for the (last) active call, or 1 for the currently paused one.
    const char *GetPeerUrl (int callId = 0);
      ///< @brief Get the URL of the peer.
      /// @param callId is 0 for the (last) active call, or 1 for the currently paused one.
      ///
      /// **Note:**
      /// 1. This method is not thread-safe (only call from main thread / same thread as all other methods of all other 'CPhone' objects).
      /// 2. The returned pointer is valid until the next call of this method (for any object!).

    /// @}


    /// @name Video stream retrieval ...
    ///   The backend should provide 2 video streams, referring to 'streamId' == 0/1:
    ///   - 0: Main video (to be displayed in a big frame, e.g. received video)
    ///   - 1: Small video (to be displayed in a small frame, e.g. a self-view)
    /// @{
    TPhoneVideoFrame *VideoLockFrame (int streamId);
      ///< @brief Lock the video stream and return info about the current picture.
      ///
      /// The lock must be held as long as the return value is used and must
      /// be unlocked even if 'NULL' is returned.
      /// Also resets the internal "changed" flag automatically.
    void VideoUnlock ();
      ///< @brief Unlock the video stream.
    /// @}


    /// @name  For internal use only...
    ///   The following methods must be considered 'protected' and are called by
    ///   the respective backend callbacks (Blame the author for not adding
    ///   tons of "friend" clauses instead...).
    /// @{
    void ReportState (EPhoneState _state);
    void ReportInfo (const char *fmt, ...);
    void *GetLibData () { return libData; }
    const int GetLibDataSize () { return sizeof (libData); }
    /// @}

  protected:

    // Various state fields...
    EPhoneState state;
    int mediaSelected;
    EPhoneState incomingAction;

    // Callbacks...
    void *cbPhoneStateChangedData, *cbInfoData, *cbDtmfReceivedData;
    void (*cbPhoneStateChanged) (void *, EPhoneState);
    void (*cbInfo) (void *, const char *);
    void (*cbDtmfReceived) (void *, char);

    // Video stream...
    TPhoneVideoFrame picInfo;   // contains 'updated' field

    // Library-specific data...
#define LIBDATA_SIZE 64
    uint8_t libData [LIBDATA_SIZE];
};


/// @}  // @file


#endif
