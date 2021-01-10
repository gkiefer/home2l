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


#include "ui_widgets.H"
#include "system.H"
#include "apps.H"
#include "phone.H"
#include "resources.H"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>



#define MAX_URL 100
#define MAX_FAVNAME 30

#define INFO_HEIGHT 48
#define INFO_FONTSIZE 24

#define INPUT_HEIGHT 60
#define INPUT_FONTSIZE 48

#define PADBIG_FONTSIZE 48
#define PADSMALL_FONTSIZE 24

#define DPAD_SPACE 5
#define DPAD_X 256
#define DPAD_Y (INPUT_HEIGHT + 2 * DPAD_SPACE)
#define DPAD_W 512
#define DPAD_H (UI_RES_Y - UI_BUTTONS_HEIGHT - INFO_HEIGHT - INPUT_HEIGHT - 2 * DPAD_SPACE)

static const SDL_Rect imageArea = Rect (0, 0, UI_RES_X, UI_RES_Y - UI_BUTTONS_HEIGHT - INFO_HEIGHT);

static class CScreenPhone *scrPhone = NULL;




// *************************** Environment Options *****************************


ENV_PARA_NOVAR ("phone.enable", bool, envPhoneEnable, NULL);
  /* Enable the phone applet
   */
ENV_PARA_PATH ("phone.linphonerc", envPhoneLinphonerc, NULL);
  /* Linphone RC file (Linphone backend only)
   *
   * With the Linphone backend, all settings not directly accessible by Home2L
   * settings are made in the (custom) Linphone RC file.
   */
ENV_PARA_NOVAR ("phone.register", const char *, envPhoneRegister, NULL);
  /* Phone registration string
   */
ENV_PARA_NOVAR ("phone.secret", const char *, envPhoneSecret, NULL);
  /* Phone registration password
   */
ENV_PARA_PATH ("phone.ringFile", envPhoneRingFile, "share/sounds/phone-classic.wav");
  /* Ring tone file
   */
ENV_PARA_INT ("phone.ringGap", envPhoneRingGap, 2000);
  /* Number of milliseconds to wait between two rings.
   */

ENV_PARA_INT ("phone.rotation", envPhoneRotation, 0);
  /* Phone video rotation
   */
ENV_PARA_NOVAR ("phone.doorRegex", const char *, envPhoneDoorRegex, NULL);
  /* Regex to decide whether a caller is a door phone
   */
ENV_PARA_PATH ("phone.ringFileDoor", envPhoneRingFileDoor, "share/sounds/dingdong-classic.wav");
  /* Ring tone file for door phones calling
   */
ENV_PARA_STRING ("phone.openerDtmf", envPhoneOpenerDtmf, NULL);
  /* DTMF sequence to send if the opener button is pushed
   */
ENV_PARA_STRING ("phone.openerRc", envPhoneOpenerRc, NULL);
  /* Resource (type 'bool') to activat if the opener button is pushed
   */
ENV_PARA_INT ("phone.openerDuration", envPhoneOpenerDuration, 1000);
  /* Duration of the opener signal
   */
ENV_PARA_INT ("phone.openerHangup", envPhoneOpenerHangup, 0);
  /* Time until the phone hangs up after the opener button is pushed (0 = no auto-hangup)
   */





ENV_PARA_SPECIAL ("phone.fav<n>", const char *, NULL);
  /* Define Phonebook entry \#$n$ ($n = 0..9$)
   *
   * An entry has the form "[<display>|]<dial>", where <dial> is the number to be dialed,
   * and (optionally) <display> is the printed name.
   */






// ***************** CWidgetVideo **************************


class CWidgetVideo: public CWidget {
  public:
    CWidgetVideo () { phone = NULL; streamId = -1;  texVideo = NULL; surfInCall = NULL; }
    ~CWidgetVideo () { if (texVideo) SDL_DestroyTexture (texVideo); SurfaceFree (&surfInCall); }

    void Setup (CPhone *_phone, SDL_Rect _maxArea, int _streamId = -1);

    void SetStream (int _streamId) { Setup (phone, maxArea, _streamId); }

    void Iterate ();

    virtual SDL_Texture *GetTexture () { return texVideo; }

  protected:
    CPhone *phone;
    SDL_Rect maxArea;
    int streamId;
    SDL_Texture *texVideo;

    TTicksMonotonic missingVideoTime;
    SDL_Surface *surfInCall;
};



void CWidgetVideo::Setup (CPhone *_phone, SDL_Rect _maxArea, int _streamId) {
  phone = _phone;
  maxArea = _maxArea;
  streamId = _streamId;
  if (texVideo) {
    SDL_DestroyTexture (texVideo);
    texVideo = NULL;
  }
  if (streamId >= 0) missingVideoTime = TicksMonotonicNow ();
  else missingVideoTime = -1;
}


void CWidgetVideo::Iterate () {
  TPhoneVideoFrame *p;
  SDL_Rect r;
  int texW, texH;

  // Lock video and obtain picture...
  p = phone->VideoLockFrame (streamId);

  // Update texture from MS picture...
  if (p && p->changed) {

    // Check if the texture format is outdated...
    if (texVideo) {
      SDL_QueryTexture (texVideo, NULL, NULL, &texW, &texH);
      if (p->w != texW || p->h != texH) {
        // Format changed -> need to recreate the texture...
        SDL_DestroyTexture (texVideo);
        texVideo = NULL;
      }
    }

    // Create texture object (again) if necessary...
    if (!texVideo) {
      texVideo = SDL_CreateTexture (UiGetSdlRenderer (),
                    SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STATIC, p->w, p->h);
        // Texture parameters taken from https://forums.libsdl.org/viewtopic.php?t=9898, "SDL 2.0 and ffmpeg. How to render texture in new version."
      if (!texVideo)
        ERRORF(("'SDL_CreateTexture' failed for video texture: %s", SDL_GetError ()));
      SDL_SetTextureBlendMode (texVideo, SDL_BLENDMODE_NONE);
      DEBUGF (1, ("Received resolution of view #%i: %i x %i pixels.\n", streamId, p->w, p->h));
      texW = p->w;
      texH = p->h;

      // Calculate actual area...
      r = maxArea;
      if (r.w * texH < texW * r.h) {
        // Texture is wider than area => fit to width...
        r.h = texH * r.w / texW;
        r.y += (maxArea.h - r.h) / 2;
      }
      else {
        // Window is wider than area => fit to height...
        r.w = texW * r.h / texH;
        r.x += (maxArea.w - r.w) / 2;
      }
      SetArea (r);
    }

    // Update the texture...
    if (SDL_UpdateYUVTexture (texVideo, NULL, p->planeY, p->pitchY, p->planeU, p->pitchU, p->planeV, p->pitchV) != 0)
      ERRORF(("'SDL_UpdateYUVTexture' failed: %s", SDL_GetError ()));

    // Trigger drawing...
    Changed ();
    missingVideoTime = TicksMonotonicNow ();
  }
  else {

    // Check for time-out without picture...
    if (missingVideoTime >= 0 && TicksMonotonicNow () > missingVideoTime + 1000) {
      missingVideoTime = -1;

      // Remove texture...
      if (texVideo) {
        SDL_DestroyTexture (texVideo);
        texVideo = NULL;
        Changed ();
      }

      // Set in-call image, but only on the main display (streamId == 0)...
      if (streamId == 0) {
        if (!surfInCall)
          SurfaceSet (&surfInCall, SurfaceGetOpaqueCopy (IconGet ("phone-incall"), BLACK));
        r = Rect (surfInCall);
        RectCenter (&r, maxArea);
        SetArea (r);
        texVideo = SDL_CreateTextureFromSurface (UiGetSdlRenderer (), surfInCall);
        SDL_SetTextureBlendMode (texture, SDL_BLENDMODE_NONE);
        Changed ();
      }
    }
  }

  // Unlock picture mutex...
  phone->VideoUnlock ();
}





// ***************** CScreenPhone **************************


#define OPEN_DOOR_DTMF "#"


enum EPhoneAction {
  paHangup, paCall,         // general
  paBack,                   // psIdle
  paAcceptMuted,            // psRinging
  paDoor,                   // psRinging / psInCall
  paMic, paCam, paTransfer  // psInCall / psDialing
};


class CScreenPhone: public CScreen {
  public:
    CScreenPhone ();
    virtual ~CScreenPhone () { Done (); }
    void Done ();

    void Setup ();

    void Iterate ();

    // Phone state...
    EPhoneState GetState () { return phone.GetState (); }

    // Image view...
    void SetImage (SDL_Surface *_surfImage, bool blinking = false);   // implies 'EnableImage (true)'
    void EnableImage (bool enable = true);

    // Callbacks for Linphone...
    void OnPhoneStateChanged (EPhoneState oldState);
    void ShowInfo (const char *msg);

    // Button actions...
    void OnActionButton (EPhoneAction action);
    void OnDialButton (char c);
    void OnFavButton (int favId);

    // High-level actions...
    bool Dial (const char *url, CScreen *_returnScreen = NULL) { returnScreen = _returnScreen; return phone.Dial (url); }

  protected:
    void SetMicOn (bool on);
    void SetCamOn (bool on);

    void UpdateInputLine ();
    void AdvanceOpenDoor ();

    // Phone...
    CPhone phone;
    CScreen *returnScreen;                // if != NULL: Screen to activate when idle

    // UI elements...
    CButton btnHangup, btnCall,           // general
            btnBack, btnBackspace,        // psIdle
            btnAcceptMuted,               // psRinging
            btnDoor,                      // psRinging / psInCall
            btnMic, btnCam, btnTransfer;  // psInCall/psDialing
    CWidget wdgImage, wdgInfo;      // general
    SDL_Surface *surfInfo;

    // Idle view...
    CButton btnsDialPad[12], btnsFavorites[10];
    CWidget wdgInput;
    SDL_Surface *surfInput;
    char input[MAX_URL+1];
    char favNames[10][MAX_FAVNAME+1], favUrls[10][MAX_URL+1];

    // Ringing view...
    SDL_Surface *surfImage;
    bool imageEnabled;
    TTicksMonotonic imageBlinkTime;

    // In-call view...
    CWidgetVideo wdgVideoMain, wdgVideoSmall;

    // Door-related...
    CRegex doorRegex;
    bool peerIsDoor;    // set, if the peer has been identified as a door phone
    bool openDoor;      // if set, the "open door" sequence is in progress
    TTicks tHangup;     // time for auto-hangup (-1 = no auto-hangup)
};



// ***** Button Callbacks *****


static void CbActionButton (class CButton *, bool, void *data) {
  scrPhone->OnActionButton ((EPhoneAction) (long) data);
}


static void CbDialButton (class CButton *, bool, void *data) {
  scrPhone->OnDialButton ((char) (long) data);
}


static void CbFavButton (class CButton *, bool, void *data) {
  scrPhone->OnFavButton ((long) data);
}



// ***** Phone Callbacks *****


static void CbPhoneStateChanged (void *, EPhoneState oldState) {
  scrPhone->OnPhoneStateChanged (oldState);
}


static void CbShowInfo (void *, const char *msg) {
  scrPhone->ShowInfo (msg);
}



// ***** Init/Done/Iterate *****


CScreenPhone::CScreenPhone () {
  returnScreen = NULL;
  surfImage = surfInfo = surfInput = NULL;
  imageBlinkTime = -1;
  peerIsDoor = openDoor = false;
  tHangup = -1;
}


void CScreenPhone::Done () {
  phone.Done ();
  wdgInfo.SetSurface (NULL);
  SurfaceFree (&surfInfo);
  wdgInput.SetSurface (NULL);
  SurfaceFree (&surfInput);
}


void CScreenPhone::Setup () {
  static const char padDigits[] = "123456789*0#";
  CString s, tmpDir;
  TTF_Font *fontNorm, *fontBig;
  int n, len, ix, iy, x, y, w, h;
  const char *favStr, *p;
  char key[30], digit[2];

  // Setup phone (without callbacks, they may not work at this time)...
  //   TBD: Use the 'var' dir for the echo cancelation state?
  EnvGetHome2lTmpPath (&tmpDir, EnvInstanceName ());
  //~ INFOF (("### tmpDir = '%s'", tmpDir.Get ()));
  EnvMkTmpDir (tmpDir.Get ());
  phone.Setup (
      EnvExecName (),
      pmAll,
      envDebug >= 3,
      tmpDir.Get (),
      envPhoneLinphonerc
    );
  phone.Register (EnvGet (envPhoneRegisterKey), EnvGet (envPhoneSecretKey));
  phone.SetCamRotation (envPhoneRotation);

  // Read configuration variables...
  doorRegex.SetPattern (EnvGet (envPhoneDoorRegexKey), REG_EXTENDED | REG_NOSUB);

  // Read favorites...
  for (n = 0; n < 10; n++) {
    sprintf (key, "phone.fav%i", n);
    if ( (favStr = EnvGet (key)) ) {
      if ( (p = strchr (favStr, '|')) ) {
        len = MIN(MAX_FAVNAME, p-favStr);
        memcpy (favNames[n], favStr, len);
        favNames[n][len] = '\0';
        strncpy (favUrls[n], p+1, MAX_URL);
        favUrls[n][MAX_URL] = '\0';
      }
      else {
        strncpy (favNames[n], favStr, MAX_FAVNAME);
        strncpy (favUrls[n], favStr, MAX_URL);
        favNames[n][MAX_FAVNAME] = favUrls[n][MAX_URL] = '\0';
      }
    }
    else favNames[n][0] = favUrls[n][0] = '\0';
  }

  // Init buttons...
  btnHangup.SetColor (DARK_RED);
  btnHangup.SetHotkey (SDLK_END);
  btnHangup.SetCbPushed (CbActionButton, (void *) paHangup);

  btnCall.SetColor (DARK_GREEN);
  btnCall.SetHotkey (SDLK_RETURN);
  btnCall.SetCbPushed (CbActionButton, (void *) paCall);

  btnBack.SetLabel (WHITE, "ic-back-48");
  btnBack.SetColor (DARK_GREY);
  btnBack.SetHotkey (SDLK_ESCAPE);
  btnBack.SetCbPushed (CbActionButton, (void *) paBack);

  btnBackspace.SetLabel (WHITE, "ic-backspace-48");
  btnBackspace.SetColor (DARK_GREY);
  btnBackspace.SetHotkey (SDLK_BACKSPACE);
  btnBackspace.SetCbPushed (CbDialButton, (void *) '<');

  btnAcceptMuted.SetLabel (WHITE, "ic-videocam-48");
  btnAcceptMuted.SetColor (DARK_GREEN);
  btnAcceptMuted.SetHotkey (SDLK_v);
  btnAcceptMuted.SetCbPushed (CbActionButton, (void *) paAcceptMuted);

  btnMic.SetHotkey (SDLK_m);
  btnMic.SetCbPushed (CbActionButton, (void *) paMic);
  SetMicOn (true);

  btnCam.SetHotkey (SDLK_c);
  btnCam.SetCbPushed (CbActionButton, (void *) paCam);
  SetCamOn (true);

  btnDoor.SetLabel (WHITE, "ic-key-48");
  btnDoor.SetColor (DARK_YELLOW);
  btnDoor.SetHotkey (SDLK_o);
  btnDoor.SetCbPushed (CbActionButton, (void *) paDoor);

  btnTransfer.SetLabel (WHITE, "ic-redo-48");
  btnTransfer.SetColor (DARK_GREY);
  btnTransfer.SetHotkey (SDLK_t);
  btnTransfer.SetCbPushed (CbActionButton, (void *) paTransfer);

  // Info widget...
  wdgInfo.SetArea (Rect (0, UI_RES_Y-UI_BUTTONS_HEIGHT-INFO_HEIGHT, UI_RES_X, INFO_HEIGHT));
  surfInfo = CreateSurface (*wdgInfo.GetArea ());
  ShowInfo (NULL);

  // Idle view: dial pad + input line...
  fontNorm = FontGet (fntNormal, PADSMALL_FONTSIZE);
  fontBig = FontGet (fntNormal, PADBIG_FONTSIZE);

  // ... input line ...
  wdgInput.SetArea (Rect (0, DPAD_SPACE, UI_RES_X, INPUT_HEIGHT));
  input[0] = '\0';
  UpdateInputLine ();

  // ... dial pad ...
  w = (DPAD_W - 2 * DPAD_SPACE) / 3;
  h = (DPAD_H - 3 * DPAD_SPACE) / 4;
  for (iy = 0; iy < 4; iy++) for (ix = 0; ix < 3; ix++) {
    n = iy * 3 + ix;
    digit[0] = padDigits[n];
    digit[1] = '\0';
    x = DPAD_X + (w + DPAD_SPACE) * ix;
    y = DPAD_Y + (h + DPAD_SPACE) * iy;
    btnsDialPad[n].Set (Rect (x, y, w, h), DARK_GREY, digit, WHITE, fontBig);
    btnsDialPad[n].SetCbPushed (CbDialButton, (void *) (long) digit[0]);
  }

  // ... favorite buttons ...
  h = (DPAD_H - 4 * DPAD_SPACE) / 5;
  for (iy = 0; iy < 5; iy++) for (ix = 0; ix < 2; ix++) {
    n = ix * 5 + iy;
    x = ix ? DPAD_X+DPAD_W+2*DPAD_SPACE : 0;
    w = ix ? UI_RES_X-x : DPAD_X-2*DPAD_SPACE;
    y = DPAD_Y + (h + DPAD_SPACE) * iy;
    btnsFavorites[n].Set (Rect (x, y, w, h), BLACK, favNames[n], LIGHT_GREY, fontNorm);
    btnsFavorites[n].SetCbPushed (CbFavButton, (void *) (long) n);
  }

  // Ringing view: Image widget...
  imageBlinkTime = -1;
  //SetImage (IconGet ("phone-ringing"));  // preload ringing icon

  // In-call view: Video widgets...
  wdgVideoMain.Setup (&phone, imageArea);
  wdgVideoSmall.Setup (&phone, Rect (imageArea.x+imageArea.w*3/4, imageArea.y+imageArea.h*3/4, imageArea.w/4, imageArea.h/4));

  // Set phone callbacks...
  phone.SetCbPhoneStateChanged (CbPhoneStateChanged);
  phone.SetCbInfo (CbShowInfo);

  // Draw Screen...
  OnPhoneStateChanged (psIdle);

  // DEBUG: Print setting...
  //~ phone.DumpSettings ();
}


void CScreenPhone::Iterate () {
  int t;
  // TBD: Try different frequencies if video enabled

  // Super-class...
  phone.Iterate ();

  // Videos...
  wdgVideoMain.Iterate ();
  wdgVideoSmall.Iterate ();

  // Blinking image...
  if (imageBlinkTime >= 0) {
    t = TicksMonotonicNow ();
    if (t > imageBlinkTime + 500) {
      EnableImage (!imageEnabled);
      if (t > imageBlinkTime + 1500) imageBlinkTime = t;
      else imageBlinkTime += 500;
    }
  }

  // Auto-hangup (for door phones)...
  if (tHangup >= 0) if (TicksNow () >= tHangup) {
    DEBUG (1, "CScreenPhone: Auto-Hanging up");
    OnActionButton (paHangup);
    tHangup = -1;
  }
}



// ***** Image view *****


void CScreenPhone::SetImage (SDL_Surface *_surfImage, bool blinking) {
  SDL_Rect r;

  SurfaceSet (&surfImage, SurfaceGetOpaqueCopy (_surfImage, BLACK));
  r = Rect (surfImage);
  RectCenter (&r, imageArea);
  wdgImage.SetArea (r);
  imageEnabled = false;
  EnableImage (true);
  imageBlinkTime = blinking ? TicksMonotonicNow () : -1;
}


void CScreenPhone::EnableImage (bool enable) {
  if (imageEnabled != enable) {
    wdgImage.SetSurface (enable ? surfImage : NULL);
    imageEnabled = enable;
  }
}



// ***** Callbacks for Linphone *****


void CScreenPhone::OnPhoneStateChanged (EPhoneState oldState) {
  SDL_Rect *layout;
  EPhoneState newState;
  int n;

  newState = phone.GetState ();

  // Unlink all widgets...
  DelAllWidgets ();
  wdgVideoMain.SetStream (-1);
  wdgVideoSmall.SetStream (-1);

  if (newState < psTransferIdle) {
    btnHangup.SetLabel (WHITE, "ic-call_end-48");
    btnCall.SetLabel (WHITE, "ic-phone-48");
  }
  else {
    btnHangup.SetLabel (WHITE, "ic-undo-48");
    btnCall.SetLabel (WHITE, "ic-redo-48");
  }

  // Check if peer is a door phone...
  if (newState == psRinging || newState == psInCall) {
    peerIsDoor = doorRegex.Match (phone.GetPeerUrl ());
  }

  // Big by-state switch...
  switch (newState) {

    case psIdle:
      SystemActiveUnlock ("_phone");
      SystemUnmute ("_phone");
      AudioStop ();

      // Setup button bar...
      layout = LayoutRow (UI_BUTTONS_RECT, UI_BUTTONS_SPACE, -2, -3, -3, -2, 0);
      btnBack.SetArea (layout[0]);    AddWidget (&btnBack);
      btnHangup.SetArea (layout[1]);  AddWidget (&btnHangup);
      btnCall.SetArea (layout[2]);    AddWidget (&btnCall);
      btnBackspace.SetArea (layout[3]);    AddWidget (&btnBackspace);
      free (layout);

      // Enable input line and dial pad...
      AddWidget (&wdgInput);
      for (n = 0; n < 12; n++) AddWidget (&btnsDialPad[n]);
      for (n = 0; n < 10; n++) AddWidget (&btnsFavorites[n]);

      // Return to last active screen if some activity was interrupted by a call...
      if (returnScreen) {
        returnScreen->Activate ();
        returnScreen = NULL;
      }
      break;

    case psTransferIdle:

      // Setup button bar...
      layout = LayoutRow (UI_BUTTONS_RECT, UI_BUTTONS_SPACE, -4, -4, -2, 0);
      btnHangup.SetArea (layout[0]);    AddWidget (&btnHangup);
      btnCall.SetArea (layout[1]);      AddWidget (&btnCall);
      btnBackspace.SetArea (layout[2]); AddWidget (&btnBackspace);
      free (layout);

      // Enable input line and dial pad...
      AddWidget (&wdgInput);
      for (n = 0; n < 12; n++) AddWidget (&btnsDialPad[n]);
      for (n = 0; n < 10; n++) AddWidget (&btnsFavorites[n]);

      break;

    case psRinging:
      SystemActiveLock ("_phone");
      SystemMute ("_phone");
      SystemSetAudioNormal ();
      SystemGoForeground ();    // bring app to front
      AudioStart (peerIsDoor ? envPhoneRingFileDoor : envPhoneRingFile, AUDIO_FOREVER, envPhoneRingGap);

      // Setup button bar...
      static const int fmtRinging [] = { -1, -1, 0 };
      static const int fmtRingingDoor [] = { -4, -1, -3, -2, 0 };
      layout = LayoutRow (UI_BUTTONS_RECT, peerIsDoor ? fmtRingingDoor : fmtRinging);
      btnHangup.SetArea (layout[0]); AddWidget (&btnHangup);
      if (!peerIsDoor) {
        btnCall.SetArea (layout[1]); AddWidget (&btnCall);
      }
      else {
        btnAcceptMuted.SetArea (layout[1]); AddWidget (&btnAcceptMuted);
        btnCall.SetArea (layout[2]); AddWidget (&btnCall);
        btnDoor.SetArea (layout[3]); AddWidget (&btnDoor);
      }
      free (layout);

      // Blinking image...
      SetImage (IconGet (peerIsDoor ? "phone-ringing-door" : "phone-ringing"), true);
      AddWidget (&wdgImage);

      // Activate screen...
      returnScreen = ActiveScreen ();
      Activate ();

      break;

    case psDialing:
    case psInCall:
    case psTransferDialing:
    case psTransferAutoComplete:
    case psTransferInCall:
      SystemActiveLock ("_phone");
      SystemMute ("_phone");
      AudioStop ();
      SystemSetAudioPhone ();

      // Layout button bar...
      static const int fmtInCall [] = { -1, -1, -6, -2, 0 };
      static const int fmtInCallDoor [] = { -2, -6, -2, 0 };
      static const int fmtTransfer [] = { -1, -1, -4, -4, 0 };

      layout = LayoutRow (UI_BUTTONS_RECT, newState >= psTransferIdle ? fmtTransfer : (peerIsDoor ? fmtInCallDoor : fmtInCall));
      n = 0;

      btnMic.SetArea (layout[n++]); AddWidget (&btnMic);
      SetMicOn (phone.GetMicOn ());

      if (!peerIsDoor) {
        btnCam.SetArea (layout[n++]); AddWidget (&btnCam);
        SetCamOn (phone.GetCamOn ());
      }
      else
        SetCamOn (false);       // We do not offer a camera image to the door

      btnHangup.SetArea (layout[n++]); AddWidget (&btnHangup);

      if (peerIsDoor) {
        btnDoor.SetArea (layout[n++]); AddWidget (&btnDoor);
      }
      else {
        btnTransfer.SetArea (layout[n++]); AddWidget (&btnTransfer);
      }
      free (layout);

      // Setup video or auto-completion...
      if (newState == psTransferAutoComplete) {
        // Set icon for auto-transfer...
        SetImage (IconGet ("ic-phone_forwarded-96"), true);
        AddWidget (&wdgImage);
      }
      else {
        // Add video widgets...
        wdgVideoMain.SetStream (0); AddWidget (&wdgVideoMain);
        wdgVideoSmall.SetStream (1); AddWidget (&wdgVideoSmall);
      }
      break;

    default: break;
  }

  // Add info widget (must be last to be on top)...
  AddWidget (&wdgInfo);

  // Advance door opening (must be last since recursive calls of this method may result)...
  AdvanceOpenDoor ();
}


void CScreenPhone::ShowInfo (const char *msg) {
  TextRender (msg, CTextFormat (FontGet (fntNormal, INFO_FONTSIZE), LIGHT_GREY, BLACK, 0, 0), surfInfo);
  wdgInfo.SetSurface (surfInfo);
  //~ INFOF (("(app_phone) S: %s\n", msg)); fflush (stdout);
}



// ***** Button actions *****


void CScreenPhone::SetMicOn (bool on) {
  btnMic.SetLabel (WHITE, on ? "ic-mic-48" : "ic-mic_off-48");
  btnMic.SetColor (on ? DARK_GREY : BLACK);
  phone.SetMicOn (on);
}


void CScreenPhone::SetCamOn (bool on) {
  btnCam.SetLabel (WHITE, on ? "ic-videocam-48" : "ic-videocam_off-48");
  btnCam.SetColor (on ? DARK_GREY : BLACK);
  phone.SetCamOn (on);
  //~ wdgVideoSmall.SetStream (on ? 1 : -1);
}


void CScreenPhone::UpdateInputLine () {
  SDL_Rect *area = wdgInput.GetArea ();

  if (!surfInput) surfInput = CreateSurface (area->w, area->h);
  TextRender (input, CTextFormat (FontGet (fntNormal, INPUT_FONTSIZE), WHITE, input[0] ? DARK_GREY : BLACK, 0, 0), surfInput);
  wdgInput.SetSurface (surfInput);
}


void CScreenPhone::AdvanceOpenDoor () {
  if (openDoor) switch (phone.GetState ()) {

    case psRinging:

      // First accept the call...
      phone.AcceptCall ();
      break;

    case psInCall:
      //~ INFO ("### AdvanceOpenDoor: psInCall");

      // Send DTMF sequence if configured to do so...
      if (envPhoneOpenerDtmf) if (envPhoneOpenerDtmf[0]) {
        DEBUGF (2, ("Sending DTMF: '%s'", envPhoneOpenerDtmf));
        phone.SendDtmf (envPhoneOpenerDtmf);
      }

      // Issue direct request to opener resource if configured so...
      if (envPhoneOpenerRc) if (envPhoneOpenerRc[0]) {
        //~ INFOF (("### Issue opener reuqest"));
        RcSetRequest (envPhoneOpenerRc, true, NULL, rcPrioNormal, 0, -envPhoneOpenerDuration);
      }

      // Initiate auto-hangup if configured so...
      if (envPhoneOpenerHangup) tHangup = TicksNow () + envPhoneOpenerHangup;

      // Done...
      openDoor = false;
      break;

    default:
      break;
  }
}


void CScreenPhone::OnActionButton (EPhoneAction action) {
  EPhoneState state = phone.GetState ();

  switch (action) {

    case paHangup:
      switch (state) {
        case psIdle:
          input[0] = '\0';
          UpdateInputLine ();
          break;
        default:
          phone.Hangup ();
      }
      break;

    case paCall:
      SetMicOn (true);
      SetCamOn (true);
      switch (state) {
        case psIdle:
        case psTransferIdle:
          phone.Dial (input);
          break;
        case psRinging:
          phone.AcceptCall ();
          break;
        case psTransferDialing:
        case psTransferInCall:
          phone.CompleteTransfer ();
          break;
        default: break;
      }
      break;

    case paBack:
      phone.Hangup ();
      AppEscape ();
      break;

    case paAcceptMuted:
      SetMicOn (false);
      SetCamOn (false);
      phone.AcceptCall ();
      break;

    case paMic:
      SetMicOn (!phone.GetMicOn ());
      break;

    case paCam:
      SetCamOn (!phone.GetCamOn ());
      break;

    case paDoor:
      INFO ("Opening door.");
      openDoor = true;
      AdvanceOpenDoor ();
      break;

    case paTransfer:
      switch (state) {
        case psInCall:
          phone.PrepareTransfer ();
          break;
        case psTransferDialing:
        case psTransferInCall:
          phone.CompleteTransfer ();
          break;
        default:
          break;
      }
      break;
  }
}


void CScreenPhone::OnDialButton (char c) {
  int len = strlen (input);

  if (c == '<') {
    if (len > 0) input[len-1] = '\0';
  }
  else if (len < MAX_URL) {
    input[len] = c;
    input[len+1] = '\0';
  }
  UpdateInputLine ();
}


void CScreenPhone::OnFavButton (int favId) {
  strncpy (input, favUrls[favId], MAX_URL);
  input[MAX_URL] = '\0';
  UpdateInputLine ();
  phone.Dial (input);
}






// ***************** Main functions ************************


CTimer iterationTimer;


static void CbIterationTimer (CTimer *, void *data) {
  CScreenPhone *scr = (CScreenPhone *) data;
  scr->Iterate ();
}


void *AppFuncPhone (int appOp, void *data) {
  switch (appOp) {

    case appOpInit:
      scrPhone = new CScreenPhone ();
      scrPhone->Setup ();
      iterationTimer.Set (0, 32, CbIterationTimer, scrPhone);
      return APP_INIT_OK;

    case appOpDone:
      iterationTimer.Clear ();
      delete scrPhone;
      break;

    case appOpLabel:
      APP_SET_LAUNCHER (data, "ic-phone", _("Phone"), SDLK_t);
      break;

    case appOpActivate:
      scrPhone->Activate ();
      break;
  }
  return NULL;
}


void AppPhoneDial (const char *url, CScreen *returnScreen) {
  if (scrPhone) {
    scrPhone->Activate ();
    scrPhone->Dial (url, returnScreen);
  }
  else WARNING ("AppPhoneDial(): No phone available.");
}
