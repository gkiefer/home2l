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


#include "system.H"



#if !ANDROID
#include "ui_screen.H"
#else
#include "ui_base.H"
#endif


static ESystemMode systemMode = smNone;





// *************************** Environment Options *****************************


ENV_PARA_BOOL ("ui.passiveBehaviour", envPassiveBehaviour, false);
  /* Main application bahaviour
   *
   * Set the general application behaviour. If set to false (default),
   * the app tries to control the display brightness, has own mechanisms for dimming the screen
   * and tries to auto-activate after some time when the user shows no activity in another app
   * (launcher-like behaviour).
   *
   * If set to true, most of these mechanisms are disabled, and the app behaves like a
   * normal app. All settings are controlled by the (Android) system.
   */
ENV_PARA_INT ("ui.standbyDelay", envStandbyDelay, 60000);
  /* Time (ms) until standby mode is entered
   */
ENV_PARA_INT ("ui.offDelay", envOffDelay, 3600000);
  /* Time (ms) until the screen is switched off
   */

ENV_PARA_BOOL ("sync2l", envSync2lEnable, false);
  /* Enable a Sync2l interface to the device's address book via a named pipe
   *
   * If enabled, a named pipe special file 'HOME2L\_ROOT/tmp/sync2l' is created
   * via which the device's address book can be accessed by the "sync2l" PIM
   * synchronisation tool. If you do not know that tool (or do not use it), you
   * should not set this parameter.
   *
   * The pipe is created automatically, and it is made user and group readable
   * and writable (mode 0660, ignoring an eventual umask). It is recommended
   * to set the SGID bit of the parent directory and let it be owned be group
   * 'home2l', so that a Debian or other chroot'ed Linux installation can
   * access the pipe.
   */





// *************************** Resources (common) ******************************


static CResource *rcModeStandby = NULL;
static CResource *rcModeActive = NULL;
static bool valModeStandby = false, valModeActive = false;  // cached mode settings in resources

static CResource *rcDispLight = NULL;
static CResource *rcLuxSensor = NULL;

static CResource *rcMute = NULL;
static bool valMute = false;      // currently cached value

static CResource *rcBluetooth = NULL;
static CResource *rcBluetoothAudio = NULL;

static CResource *rcPhoneState = NULL;


// Some forward declarations...
static void SystemModeUpdate (bool inForeground);
static void BluetoothDriveValue (CRcValueState *vs);


static void DriveActiveStandbySync (void *) {
  bool inForeground = false;

  // Bring the app to front, if the screen is to be switched off (active == standby == false) ...
  if (!valModeActive && !valModeStandby && !envPassiveBehaviour) {
    inForeground = true;
    if (systemMode != smOff) SystemGoForeground ();     // be careful with 'SystemGoForeground ()' invocations
  }

  // Update system mode...
  SystemModeUpdate (inForeground);
}


static void RcDriverFunc_ui (ERcDriverOperation op, CRcDriver *drv, CResource *rc, CRcValueState *vs) {
  //~ INFOF (("RcDriverFunc_ui (%i)", op));

  switch (op) {
    case rcdOpInit:
      rcModeStandby = drv->RegisterResource ("standby", rctBool, true);
      rcModeStandby->SetDefault (false);
        /* [RC:ui] Report and select standby mode
         *
         * If 'true', the screen is on, but eventually with reduced brightness
         * (unless \texttt{ui/active} is also set). If neither \texttt{ui/active} nor
         * \texttt{ui/standby} is 'true', the screen is switched off, and the device may
         * enter a power saving mode, depending on the OS platform.
         */
      rcModeActive = drv->RegisterResource ("active", rctBool, true);
      rcModeActive->SetDefault (false);
        /* [RC:ui] Report and select active mode
         *
         * If 'true', the screen is on at full brightness (as during interaction).
         */

      rcDispLight = drv->RegisterResource ("dispLight", rctPercent, false);
        /* [RC:ui] Display brightness
         */
      rcLuxSensor = drv->RegisterResource ("luxSensor", rctFloat, false);
        /* [RC:ui] Light sensor output in Lux
         */

      rcMute = drv->RegisterResource ("mute", rctBool, true);
      rcMute->SetDefault (false);
        /* [RC:ui] Audio muting
         *
         * If 'true', the music player is paused. This can be used to mute playing music
         * if the doorbell rings or some other event in the house occurs which requires
         * the attention of the user.
         */

      rcBluetooth = drv->RegisterResource ("bluetooth", rctBool, true);    // no default(!)
        /* [RC:ui] Report and set Bluetooth state
         */
      rcBluetoothAudio = drv->RegisterResource ("bluetoothAudio", rctBool, false);
        /* [RC:ui] Report whether an audio device is connected via Bluetooth
         */

#if WITH_PHONE
      rcPhoneState = drv->RegisterResource ("phone", rctPhoneState, false);
        /* [RC:ui] Report phone state
         */
#endif

#if ANDROID == 0
      rcBluetooth->ReportValue (false);
      rcBluetoothAudio->ReportValue (false);
#endif
      break;

    case rcdOpStop:       // after return, no threads must be running anymore, eventually open timers can be left (will be neglected)
      break;

    case rcdOpDriveValue: // according to the pending requests, a new value has to be applied
      if (rc == rcMute) {
        if (vs->IsValid ()) valMute = vs->Bool ();      // cache the value
      }
      else if (rc == rcBluetooth) {
        BluetoothDriveValue (vs);
      }
      else {  // modes (standby, active, background)...
        if (vs->IsValid ()) { // ignore all request off events
          if (rc == rcModeStandby) valModeStandby = vs->Bool ();      // cache the value
          else if (rc == rcModeActive) valModeActive = vs->Bool ();   // cache the value
          else ASSERT (false);
          MainThreadCallback (DriveActiveStandbySync);
        }
      }
      break;
  }
}





// *************************** Debian/PC-specific part *************************


#if ANDROID == 0


static inline void DebianInit () {}


//~ void SystemSetImmersiveMode () {}

void SystemSetAudioNormal () { DEBUG (1, "SystemSetAudioNormal ()"); }
void SystemSetAudioPhone () { DEBUG (1, "SystemSetAudioPhone ()"); }
//~ void SystemSetAudioRinging () { DEBUG (1, "SystemSetAudioRinging ()"); }

static inline void BluetoothDriveValue (CRcValueState *vs) {}

static inline void BackgroundForegroundDrive (void *data) {}





// *************************** Android-specific part ***************************


#else // ANDROID == 1


#include <jni.h>
#include <android/sensor.h>

#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>


ENV_PARA_STRING ("android.autostart", envAndroidAutostart, NULL);
  /* Shell script to be executed on startup of the app
   *
   * If defined, the named shell script is started and executed in the background
   * on each start of the app. The path name may either be absolute or relative to
   * HOME2L\_ROOT. If the name starts with '!', the script is started with root
   * privileges using 'su'.
   *
   * It is allowed to append command line arguments.
   */



static JavaVM *javaVM = NULL;
static JNIEnv *jniEnv = NULL;
static jclass jniClass = NULL;

static jmethodID midAboutToExit = NULL;

static jmethodID midShowMessage = NULL;
static jmethodID midShowToast = NULL;

static jmethodID midAssetLoadTextFile = NULL;
static jmethodID midAssetCopyFileToInternal = NULL;

static jmethodID midSetKeepScreenOn = NULL;
static jmethodID midSetDisplayBrightness = NULL;

static jmethodID midGoForeground = NULL;
static jmethodID midGoBackground = NULL;
static jmethodID midLaunchApp = NULL;

//~ static jmethodID midSetImmersiveMode = NULL;

static jmethodID midSetAudioNormal = NULL;    // deprecated
static jmethodID midSetAudioPhone = NULL;     // deprecated
//~ static jmethodID midSetAudioRinging = NULL;

static jmethodID midEnableSync2l = NULL;

static jmethodID midBluetoothSet = NULL;
static jmethodID midBluetoothPoll = NULL;





// ***** Exception handling ******


static void AndroidExceptionCheck () {
  // Handle exceptions not handled inside Java code,
  // should be called after each Java method invocation from native code.
  // Usually, such exceptions must not occur, so that we abort and log as much information as
  // possible here.
  if (jniEnv->ExceptionCheck ()) {
    jniEnv->ExceptionDescribe ();     // Log to stderr first (Android: 'W/System.err')
    jthrowable exc = jniEnv->ExceptionOccurred ();
    jmethodID toString = jniEnv->GetMethodID (jniEnv->FindClass("java/lang/Object"), "toString", "()Ljava/lang/String;");
    jstring s = (jstring) jniEnv->CallObjectMethod (exc, toString);
    const char *utf = jniEnv->GetStringUTFChars (s, NULL);
    ABORTF (("Unexpected Java Exception in native code: %s", utf));
    //~ jniEnv->ReleaseStringUTFChars (s, utf);
    //~ jniEnv->DeleteLocalRef (s);
    //~ jniEnv->DeleteLocalRef (exc);
  }
}




// ***** Android storage preparation *****


static bool AndroidAssetLoadTextFile (const char *relPath, CString *ret) {
  // Load a text file from asset
  jstring jRet, jRelPath = jniEnv->NewStringUTF (relPath);
  const char *buf;

  //~ INFOF (("### (before) ExceptionOccurred () = %i", (int) jniEnv->ExceptionCheck ()));
  jRet = (jstring) (jniEnv->CallStaticObjectMethod (jniClass, midAssetLoadTextFile, jRelPath));
  //~ INFOF (("### (after ) ExceptionOccurred () = %i", (int) jniEnv->ExceptionCheck ()));
  AndroidExceptionCheck ();
  jniEnv->DeleteLocalRef (jRelPath);

  if (jniEnv->IsSameObject (jRet, NULL)) {     // if 'jRet' is 'null' ...
    WARNINGF(("Failed to read asset '%s'.", relPath));
    return false;
  }

  // Success ...
  buf = jniEnv->GetStringUTFChars (jRet, NULL);
  ret->Set (buf);
  jniEnv->ReleaseStringUTFChars (jRet, buf);
  jniEnv->DeleteLocalRef (jRet);
  return true;
}


static bool AndroidAssetCopyFileToInternal (const char *relPath) {
  jstring jRelPath;
  jboolean jOk;

  //~ INFOF (("### Copying '%s'...", relPath));

  // Copy the file ...
  jRelPath = jniEnv->NewStringUTF (relPath);
  jOk = jniEnv->CallStaticBooleanMethod (jniClass, midAssetCopyFileToInternal, jRelPath);
  jniEnv->DeleteLocalRef (jRelPath);
  AndroidExceptionCheck ();

  // Done ...
  if (jOk != JNI_TRUE) {
    WARNINGF(("Failed to copy asset '%s'.", relPath));
    return false;
  }
  return true;
}


/* Native implementations...
 *
 * [2019-09-02] Crashed inside:
 *
 *    ctx = SDL_RWFromFile (relPath, "r");
 *
 * ********** Crash dump: **********
 * Build fingerprint: 'samsung/espressowifixx/espressowifi:4.2.2/JDQ39/P3110XXDMH1:user/release-keys'
 * pid: 6495, tid: 6582, name: SDLThread  >>> org.home2l.app <<<
 * signal 6 (SIGABRT), code -6 (SI_TKILL), fault addr --------
 * Stack frame #00  pc 000220fc  /system/lib/libc.so (tgkill+12)
 * Stack frame #01  pc 00013153  /system/lib/libc.so (pthread_kill+50)
 * Stack frame #02  pc 0001334b  /system/lib/libc.so (raise+10)
 * Stack frame #03  pc 0001203b  /system/lib/libc.so
 * Stack frame #04  pc 000219b0  /system/lib/libc.so (abort+4)
 * Stack frame #05  pc 000471eb  /system/lib/libdvm.so (dvmAbort+78)
 * Stack frame #06  pc 0004bb57  /system/lib/libdvm.so (dvmDecodeIndirectRef(Thread*, _jobject*)+146)
 * Stack frame #07  pc 0004e6db  /system/lib/libdvm.so
 * Stack frame #08  pc 0009ec07  /data/app-lib/org.home2l.app-2/libhome2l-wallclock.so: Routine Internal_Android_JNI_FileOpen at /home/gundolf/prog/home2l/src/external/sdl2/dummy_app/jni/../../src/SDL2/src/core/android/SDL_android.c:1178
 * Stack frame #09  pc 0009f841  /data/app-lib/org.home2l.app-2/libhome2l-wallclock.so (Android_JNI_FileOpen+84): Routine Android_JNI_FileOpen at /home/gundolf/prog/home2l/src/external/sdl2/dummy_app/jni/../../src/SDL2/src/core/android/SDL_android.c:1323
 * Stack frame #10  pc 000732a1  /data/app-lib/org.home2l.app-2/libhome2l-wallclock.so (SDL_RWFromFile_REAL+116): Routine SDL_RWFromFile_REAL at /home/gundolf/prog/home2l/src/external/sdl2/dummy_app/jni/../../src/SDL2/src/file/SDL_rwops.c:548
 * Stack frame #11  pc 0006e4d7  /data/app-lib/org.home2l.app-2/libhome2l-wallclock.so (SDL_RWFromFile+10): Routine SDL_RWFromFile at /home/gundolf/prog/home2l/src/external/sdl2/dummy_app/jni/../../src/SDL2/src/dynapi/SDL_dynapi_procs.h:382
 * Stack frame #12  pc 0005f3c9  /data/app-lib/org.home2l.app-2/libhome2l-wallclock.so: Routine AndroidAssetLoadTextFile at /home/gundolf/prog/home2l/src/wallclock/system.C:269

static bool AndroidAssetLoadTextFile (const char *relPath, CString *ret) {
  // Load a text file from asset
  SDL_RWops *ctx;
  char *buf;
  int size;

  ctx = SDL_RWFromFile (relPath, "r");
  if (!ctx) ERRORF (("Failed to open asset '%s': %s", relPath, SDL_GetError ()));

  size = (int) SDL_RWsize (ctx);
  if (size < 0) ERRORF (("Failed to get size of asset '%s': %s", relPath, SDL_GetError ()));

  buf = MALLOC(char, size + 1);
  if (SDL_RWread (ctx, buf, 1, size) < size)
    ERRORF (("Failed to read asset '%s': %s", relPath, SDL_GetError ()));
  buf[size] = '\0';
  ret->SetO (buf);    // pass ownership of 'buf' to CString object

  SDL_RWclose (ctx);
}
*/


static inline void AndroidPrepareHome2lRoot () {
  CString s, installedVersion, myVersion;
  CSplitString fileList;
  struct stat fileStat;
  int n;
  const char *p;
  bool ok, newEtc;

  //~ INFO ("######### AndroidPrepareHome2lRoot #########");

  // a) Check blob ...
  ok = false;
  if (!installedVersion.ReadFile (EnvGetHome2lRootPath (&s, "VERSION")))
    INFO ("No installed blob found.");
  else {
    ASSERT (AndroidAssetLoadTextFile ("VERSION", &myVersion));
    myVersion.Strip ();
    installedVersion.Strip ();
    if (installedVersion.Compare (myVersion.Get ()) != 0) INFO ("Installed blob must be updated.");
    else {
      INFO ("Installed blob is up-to-date.");
      ok = true;
    }
  }

  // b) Check for an 'etc' dir ...
  newEtc = true;
  if (stat (EnvGetHome2lRootPath (&s, "etc"), &fileStat) == 0)  // Does 'etc' exist ...
    if (fileStat.st_mode & S_IFDIR)     // ... and is a directory? ...
      newEtc = false;                   // => Do NOT install a new one.
  if (newEtc) INFO ("No /etc directory found: Will install a default one.");

  // c) Remove and install new files ...
  if (!ok) {
    INFO (newEtc ? "-T- Installing new configuration and updating asset cache..." : "-T- Updating asset cache...");
    DEBUGF (1, ("Installing new blob %s at '%s'...",
            newEtc ? "and 'etc' template" : "",
            EnvHome2lRoot ()));

    // Remove old blob (and eventually '/etc/*') ...
    UnlinkTree (EnvHome2lRoot (), newEtc ? "/var" : "/var /etc");

    // Get and create all files from asset ...
    ASSERT (AndroidAssetLoadTextFile ("FILES", &s));

    fileList.Set (s.Get ());
    for (n = 0; n < fileList.Entries (); n++) {
      p = fileList.Get (n);
      if (newEtc || !(p[0] == 'e' && p[1] == 't' && p[2] == 'c' && p[3] == '/')) {
        DEBUGF (2, ("Installing '%s'...", p));
        AndroidAssetCopyFileToInternal (p);

        // Make files in 'bin/*' executable...
        if (p[0] == 'b' && p[1] == 'i' && p[2] == 'n' && p[3] == '/')
          chmod (EnvGetHome2lRootPath (&s, p), S_IRUSR | S_IWUSR | S_IXUSR);
      }
    }
    AndroidAssetCopyFileToInternal ("VERSION");
      // The VERSION file must be copied last for the case that the App crashes during the update.
  }

  // d) Set permissions to make 'var' and 'tmp' accessible from outside ...
  chmod (EnvHome2lRoot (), S_IRUSR | S_IWUSR | S_IXUSR | S_IXGRP | S_IXOTH);
  chmod (EnvGetHome2lRootPath (&s, "var"), S_IRUSR | S_IWUSR | S_IXUSR | S_IXGRP | S_IXOTH);
  chmod (EnvGetHome2lRootPath (&s, "tmp"), S_IRUSR | S_IWUSR | S_IXUSR | S_IXGRP | S_IXOTH);
}





// ***** Logging *****


static void AndroidShowMessage (const char *title, const char *msg) {
  jstring jTitle = jniEnv->NewStringUTF (title),
          jMsg = jniEnv->NewStringUTF (msg);
  jniEnv->CallStaticVoidMethod (jniClass, midShowMessage, jTitle, jMsg);
  AndroidExceptionCheck ();
  jniEnv->DeleteLocalRef (jTitle);
  jniEnv->DeleteLocalRef (jMsg);
}


static void AndroidShowToast (const char *msg, bool showLonger) {
  jstring jMsg = jniEnv->NewStringUTF (msg);
  jniEnv->CallStaticVoidMethod (jniClass, midShowToast, jMsg, showLonger ? JNI_TRUE : JNI_FALSE);
  AndroidExceptionCheck ();
  jniEnv->DeleteLocalRef (jMsg);
}





// ***** Pre-Init *****


extern const char *envRootDir, *envRootDirKey;     // defined in 'env.C'


static inline void AndroidPreInit () {

  // JNI data structures...
  javaVM->GetEnv ((void **) &jniEnv, JNI_VERSION_1_6);    // requires 'initNative' to be called from Android before
  ASSERT(jniEnv != NULL);
  jniClass = jniEnv->FindClass ("org/home2l/app/Home2l");
  ASSERT(jniClass != NULL);

  /*********************************************************************
   *
   *  Type Signatures [http://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/types.html]
   *
   * The JNI uses the Java VM’s representation of type signatures.
   *
   *  Z - boolean
   *  B - byte
   *  C - char
   *  S - short
   *  I - int
   *  J - long
   *  F - float
   *  D - double
   *  L <fully-qualified-class> ;   - fully-qualified-class
   *  [ type                        -type[]
   *
   *  ( arg-types ) ret-type
   *
   *  For example, the Java method:
   *
   *    long f (int n, String s, int[] arr);
   *
   *  has the following type signature:
   *
   *    (ILjava/lang/String;[I)J
   *
   *********************************************************************/

  midAboutToExit = jniEnv->GetStaticMethodID (jniClass, "aboutToExit", "()V");
  ASSERT(midAboutToExit != NULL);

  midShowMessage = jniEnv->GetStaticMethodID (jniClass, "showMessage", "(Ljava/lang/String;Ljava/lang/String;)V");
  ASSERT(midShowMessage != NULL);
  midShowToast = jniEnv->GetStaticMethodID (jniClass, "showToast", "(Ljava/lang/String;Z)V");
  ASSERT(midShowToast != NULL);

  midAssetLoadTextFile = jniEnv->GetStaticMethodID (jniClass, "assetLoadTextFile", "(Ljava/lang/String;)Ljava/lang/String;");
  ASSERT(midAssetLoadTextFile != NULL);
  midAssetCopyFileToInternal = jniEnv->GetStaticMethodID (jniClass, "assetCopyFileToInternal", "(Ljava/lang/String;)Z");;
  ASSERT(midAssetCopyFileToInternal != NULL);

  midSetKeepScreenOn = jniEnv->GetStaticMethodID (jniClass, "setKeepScreenOn", "(Z)V");
  ASSERT(midSetKeepScreenOn != NULL);
  midSetDisplayBrightness = jniEnv->GetStaticMethodID (jniClass, "setDisplayBrightness", "(F)V");
  ASSERT(midSetDisplayBrightness != NULL);

  midGoForeground = jniEnv->GetStaticMethodID (jniClass, "goForeground", "()V");
  ASSERT(midGoForeground != NULL);
  midGoBackground = jniEnv->GetStaticMethodID (jniClass, "goBackground", "()V");
  ASSERT(midGoBackground != NULL);
  midLaunchApp = jniEnv->GetStaticMethodID (jniClass, "launchApp", "(Ljava/lang/String;)V");
  ASSERT(midLaunchApp != NULL);

  //~ midSetImmersiveMode = jniEnv->GetStaticMethodID (jniClass, "setImmersiveMode", "()V");
  //~ ASSERT(midSetImmersiveMode != NULL);

  midSetAudioNormal = jniEnv->GetStaticMethodID (jniClass, "setAudioNormal", "()V");
  ASSERT(midSetAudioNormal != NULL);
  midSetAudioPhone = jniEnv->GetStaticMethodID (jniClass, "setAudioPhone", "()V");
  ASSERT(midSetAudioPhone != NULL);
  //~ midSetAudioRinging = jniEnv->GetStaticMethodID (jniClass, "setAudioRinging", "()V");
  //~ ASSERT(midSetAudioRinging != NULL);

  midEnableSync2l = jniEnv->GetStaticMethodID (jniClass, "enableSync2l", "(Ljava/lang/String;)V");
  ASSERT(midEnableSync2l != NULL);

  midBluetoothSet = jniEnv->GetStaticMethodID (jniClass, "bluetoothSet", "(Z)V");
  ASSERT(midBluetoothSet != NULL);
  midBluetoothPoll = jniEnv->GetStaticMethodID (jniClass, "bluetoothPoll", "()I");
  ASSERT(midBluetoothPoll != NULL);

  // Set log callbacks to enable dialogs and toasts ...
  LogSetCallbacks (AndroidShowMessage, AndroidShowToast);

  // Prepare HOME2l_ROOT (aka asset cache)...
  envRootDir = EnvGet (envRootDirKey);
    // This has been set by 'EnvPut' from Java ('Home2l.init()'),
    // but 'EnvInit ()' has not been called yet, so that 'EnvGetHome2lRootPath ()'
    // requires this to work!
  AndroidPrepareHome2lRoot ();
}


void *_AndroidGetJavaVM () { return javaVM; }





// ***** Mode setting *****


static int64_t lastDisplayTime = 0;   // timestamps are in nanoseconds(!)


static void AndroidSetBrightness (float brightness) {
  static float lastBrightness = -1.0;

  if (brightness != lastBrightness) {
    //~ INFO ("'AndroidSetBrightness' running");
    jniEnv->CallStaticVoidMethod (jniClass, midSetDisplayBrightness, (jfloat) brightness);
    AndroidExceptionCheck ();
    lastBrightness = brightness;
  }
}


static inline void AndroidSetMode (ESystemMode mode, ESystemMode lastMode) {
  ASSERT (mode != lastMode);
  if (envPassiveBehaviour) {
    if (mode != smBackground)
      jniEnv->CallStaticVoidMethod (jniClass, midSetKeepScreenOn, mode == smActive ? JNI_TRUE : JNI_FALSE);
  }
  else {    // normal (active) mode...
    //~ INFOF (("### AndroidSetMode (normal): %i -> %i", lastMode, mode));
    if (mode != smBackground) {
      jniEnv->CallStaticVoidMethod (jniClass, midSetKeepScreenOn, mode == smOff ? JNI_FALSE : JNI_TRUE);
      lastDisplayTime = 0;    // make display brightness change immediately
    }
  }
  AndroidExceptionCheck ();
}



// ***** Entering / leaving background *****


static inline void AndroidGoBackground (const char *appStr) {
  //~ ABORT ("### AndroidGoBackground () called");
  if (!appStr) {
    jniEnv->CallStaticVoidMethod (jniClass, midGoBackground);
  }
  else {
    jstring jAppStr = jniEnv->NewStringUTF (appStr);
    jniEnv->CallStaticVoidMethod (jniClass, midLaunchApp, jAppStr);
    jniEnv->DeleteLocalRef (jAppStr);
  }
  AndroidExceptionCheck ();
}


static inline void AndroidGoForeground () {
  //~ ABORT ("### AndroidGoForeground () called");
  jniEnv->CallStaticVoidMethod (jniClass, midGoForeground);
  AndroidExceptionCheck ();
}



// ***** (Light) Sensor(s) *****


#define SENSOR_INTERVAL 128     // check interval in ms
#define DISPLAY_INTERVAL 1024   // interval to update the screen brightness



// Parameters...
ENV_PARA_FLOAT ("ui.lightSensor.minLux", envLightSensorMinLux, 7);
  /* Any Lux value below this will be rounded to this
   */
ENV_PARA_FLOAT ("ui.lightSensor.alOffset", envLightSensorAlOffset, 20);
  /* Linear part of the "apparent light" function (in Lux)
   */
ENV_PARA_FLOAT ("ui.lightSensor.alFilterWeight", envLightSensorAlFilterWeight, 0.1);
  /* Apparent light filter factor
   */
ENV_PARA_FLOAT ("ui.lightSensor.acThreshold", envLightSensorAcThreshold, 0.02);
  /* Apparent change threshold to report a change
   *
   * The app tries to detect the presence of people in the room by monitoring
   * the light sensor and waking up the app on a quick change in light.
   * This and the previous parameters can be used to tune the sensitivity
   * of this wakeup mechanism. Please refer to the source code to understand
   * the exact algorithm.
   *
   * Please do not expect too much -- typical light sensors are not well suited
   * for presence detection. If unsure, leave these settings with their defaults.
   */

ENV_PARA_FLOAT ("ui.display.minLux", envBrightnessMinLux, 10.0);
  /* Reference Lux value for the "minimum" brightness values
   */
ENV_PARA_FLOAT ("ui.display.typLux", envBrightnessTypLux, 100.0);
  /* Reference Lux value for the "typical" brightness values
   */
ENV_PARA_FLOAT ("ui.display.maxLux", envBrightnessMaxLux, 1000.0);
  /* Reference Lux value for the "maximum" brightness values
   *
   * THe display brightness is adjusted according to a two-piece
   * piece-wise linear function depending on the logarithm of the
   * Lux value. This and the previous parameters define the Lux
   * values for the three supporting points of the piece-wise
   * linear function.
   */

ENV_PARA_FLOAT ("ui.display.activeMin", envBrightnessActiveMin, 0.5);
  /* Minimum display brightness in active mode (percent)
   */
ENV_PARA_FLOAT ("ui.display.activeTyp", envBrightnessActiveTyp, 0.7);
  /* Typical display brightness in active mode (percent)
   */
ENV_PARA_FLOAT ("ui.display.activeMax", envBrightnessActiveMax, 1.0);
  /* Maximum display brightness in active mode (percent)
   */

ENV_PARA_FLOAT ("ui.display.standbyMin", envBrightnessStandbyMin, 0.25);
  /* Minimum display brightness in standby mode (percent)
   */
ENV_PARA_FLOAT ("ui.display.standbyTyp", envBrightnessStandbyTyp, 0.35);
  /* Typical display brightness in standby mode (percent)
   */
ENV_PARA_FLOAT ("ui.display.standbyMax", envBrightnessStandbyMax, 0.5);
  /* Maximum display brightness in standby mode (percent)
   */



// Data structures...
static ASensorManager *sensorManager = NULL;
static ASensorEventQueue *sensorEventQueue = NULL;
static ALooper *looper = NULL;
static const ASensor *lightSensor = NULL;

static CTimer sensorTimer;

static float alMin, alTyp, alMax;   // al values for min/typ/max Lux values (precomputed in 'SensorInit'



static void SensorIterate (CTimer *, void *) {
  static int64_t lastSensorTime = 0;   // timestamps are in nanoseconds(!)
  static float lastAl = 0.0, lastBrightness = -1.0;
  ASensorEvent ev;
  float lux, rawAl, al, ac; // apparent light, apparent change
  float brightness, brMin, brTyp, brMax;
  bool trigger;
  int n;

  if (ASensorEventQueue_getEvents (sensorEventQueue, &ev, 1) < 1) return;
  if (ev.timestamp - lastSensorTime < SENSOR_INTERVAL*1000000 / 2)  return;
        // less than half the desired interval time => ignore event to avoid unexpected behavior
  lastSensorTime = ev.timestamp;

  // Check for sudden light change to trigger a wake-up...
  //   read Lux value...
  lux = ev.light < envLightSensorMinLux ? envLightSensorMinLux : ev.light;     // 'ev.light' is in Lux
  //   compute filtered "apparent light" value (~log (lux value) )...
  rawAl = logf (lux + envLightSensorAlOffset);
  if (lastAl <= 0.0) al = lastAl = rawAl;
  else al = envLightSensorAlFilterWeight * rawAl + (1.0 - envLightSensorAlFilterWeight) * lastAl;
  //   compute trigger...
  ac = al - lastAl;
  trigger = fabs (ac) >= envLightSensorAcThreshold;
  lastAl = al;

  //~ INFOF (("lux = %7.1f, rawAl = %5.3f, al = %5.3f, change = %6.3f %c", lux, rawAl, al, ac, trigger ? '*' : ' '));
  if (trigger && systemMode != smBackground) SystemWakeupStandby ();

  // Compute display brightness...
  if ((ev.timestamp - lastDisplayTime >= DISPLAY_INTERVAL * 1000000) && (systemMode != smBackground)) {
    //~ INFOF (("ev.timestamp = %llu, lastDisplayTime = %llu", ev.timestamp, lastDisplayTime));
    lastDisplayTime = ev.timestamp;

    switch (systemMode) {
      case smActive:
        brMin = envBrightnessActiveMin; brTyp = envBrightnessActiveTyp; brMax = envBrightnessActiveMax; break;
      case smStandby:
        brMin = envBrightnessStandbyMin; brTyp = envBrightnessStandbyTyp; brMax = envBrightnessStandbyMax; break;
      default:    // 'smOff' (and others on accident)
        brightness = 0.5 * envBrightnessStandbyMin;
    }
    if (systemMode >= smStandby) {
      if (al <= alMin) brightness = brMin;
      else if (al <= alTyp) brightness = brTyp + (al-alTyp) * (brMin-brTyp) / (alMin-alTyp);
      else if (al < alMax) brightness = brTyp + (al-alTyp) * (brMax-brTyp) / (alMax-alTyp);
      else brightness = brMax;
    }
    //~ INFOF (("systemMode = %i, dispBright := %.3f  (alMin = %.3f, alTyp = %.3f, alMax = %.3f, brMin = %.3f, brTyp = %.3f, brMax = %.3f)", systemMode, brightness, alMin, alTyp, alMax, brMin, brTyp, brMax));
    if (brightness < 0.0) brightness = 0.0;   // limit result, the configuration parameters may have been be invalid...
    if (brightness > 1.0) brightness = 1.0;
    AndroidSetBrightness (brightness);

    // Report light sensor and display brightness values to resources...
    rcLuxSensor->ReportValue (lux);
    rcDispLight->ReportValue (brightness * 100.0f);
  }
}


static inline void SensorInit () {
  float fVal;
  int iVal;

  if (envPassiveBehaviour) return;

  // Init Android/NDK interface...
  sensorManager = ASensorManager_getInstance ();
  ASSERT(sensorManager != NULL);
  looper = ALooper_prepare (ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
  ASSERT(looper != NULL);
  sensorEventQueue = ASensorManager_createEventQueue (sensorManager, looper, 0, NULL, NULL);
  ASSERT(sensorEventQueue != NULL);

  lightSensor = ASensorManager_getDefaultSensor (sensorManager, ASENSOR_TYPE_LIGHT);
  ASSERT(lightSensor != NULL);
  ASensorEventQueue_enableSensor (sensorEventQueue, lightSensor);
  ASensorEventQueue_setEventRate (sensorEventQueue, lightSensor, SENSOR_INTERVAL*1000);

  // Init timer...
  sensorTimer.Set (0, SENSOR_INTERVAL, SensorIterate);

  // Light values for display brightness computations...
  alMin = logf (envBrightnessMinLux + envLightSensorAlOffset);
  alTyp = logf (envBrightnessTypLux + envLightSensorAlOffset);
  alMax = logf (envBrightnessMaxLux + envLightSensorAlOffset);
}


static inline void SensorDone () {
  if (sensorManager) ASensorManager_destroyEventQueue (sensorManager, sensorEventQueue);
  sensorTimer.Clear ();
}



//~ // ***** Immersive Mode *****
//~
//~
//~ void SystemSetImmersiveMode () {
  //~ jniEnv->CallStaticVoidMethod (jniClass, midSetImmersiveMode);
  //~ AndroidExceptionCheck ();
//~ }



// ***** Audio Manager *****


void SystemSetAudioNormal () {
  jniEnv->CallStaticVoidMethod (jniClass, midSetAudioNormal);
  AndroidExceptionCheck ();
}


void SystemSetAudioPhone () {
  jniEnv->CallStaticVoidMethod (jniClass, midSetAudioPhone);
  AndroidExceptionCheck ();
}


//~ void SystemSetAudioRinging () {
  //~ jniEnv->CallStaticVoidMethod (jniClass, midSetAudioRinging);
  //~ AndroidExceptionCheck ();
//~ }



// ***** Sync2l interface *****


bool EnableSync2l () {
  CString sync2lPipeName;
  jstring jPipeName;
  bool ok;

  // Check if we want the Sync2l adapter...
  if (!envSync2lEnable) return true;

  // Create the pipe and set its permissions...
  EnvGetHome2lTmpPath (&sync2lPipeName, "sync2l");
  EnvMkTmpDir (NULL);
  INFOF (("### sync2lPipeName = %s", sync2lPipeName.Get ()));
  unlink (sync2lPipeName.Get ());    // try to unlink previous entry first (no matter if it fails)
  ok =         ( mkfifo (sync2lPipeName.Get (), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) == 0 );                  // create the pipe
  if (ok) ok = ( chmod (sync2lPipeName.Get (), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) == 0);   // set permissions to allow read/write access to owner and group
  if (!ok) {
    WARNINGF (("Failed to create Sync2l pipe: %s", strerror (errno)));
    return false;
  }

  // Launch the Java thread serving the pipe...
  jPipeName = jniEnv->NewStringUTF (sync2lPipeName);
  jniEnv->CallStaticVoidMethod (jniClass, midEnableSync2l, jPipeName);
  AndroidExceptionCheck ();
  jniEnv->DeleteLocalRef (jPipeName);

  // Done...
  return true;
}



// ***** Bluetooth *****


#define BLUETOOTH_INTERVAL 512

static CTimer bluetoothTimer;
static bool valBluetooth = false, valBluetoothReq = true, valBluetoothDrv = false, valBluetoothAudio = false;
  // valBluetooth, valBluetoothAudio   : actual values last reported by Android ('1' == connection esablished)
  // valBluetoothReq   : Value last requested to Android
  // valBluetoothDrv   : Value last driven by resource
  //
  // 'valBluetoothReq' and 'valBluetoothDrv' are set differently to make sure Bluetooth is
  // defined on startup.



static void BluetoothIterate (CTimer *, void *) {
  jint jStatus;

  // Drive a new value if pending...
  if (valBluetoothDrv != valBluetoothReq) {
    jniEnv->CallStaticVoidMethod (jniClass, midBluetoothSet, valBluetoothDrv ? JNI_TRUE : JNI_FALSE);
    valBluetoothReq = valBluetoothDrv;
  }

  // Poll status from the Java world...
  jStatus = jniEnv->CallStaticIntMethod (jniClass, midBluetoothPoll);
    // Note: The following bit masks must match the corresponding ones in the Java method
  valBluetooth = (jStatus & 1) != 0;
  valBluetoothAudio = (jStatus & 2) != 0;

  // Exception check...
  AndroidExceptionCheck ();

  //~ INFOF (("### BluetoothIterate: BT = %i/%i/%i, BTaudio = %i", (int) valBluetoothDrv, (int) valBluetoothReq, (int) valBluetooth, (int) valBluetoothAudio));

  // Report values...
  rcBluetooth->ReportValue (valBluetoothReq, valBluetoothReq == valBluetooth ? rcsValid : rcsBusy);
  rcBluetoothAudio->ReportValue (valBluetoothAudio);
}


static void BluetoothIterateCb (void *) {
  BluetoothIterate (NULL, NULL);
}


static void BluetoothDriveValue (CRcValueState *vs) {
  //~ INFOF (("### BluetoothDriveValue: %s", vs->ToStr ()));
  if (vs->IsValid ()) {
    valBluetoothDrv = vs->Bool ();
    //~ INFOF (("###   ... valBluetoothDrv = %i", (int) valBluetoothDrv));
    MainThreadCallback (BluetoothIterateCb, NULL);
  }
  vs->SetState (rcsBusy);  // report "busy" until we know the new state
}


static void BluetoothInit () {
  bluetoothTimer.Set (0, BLUETOOTH_INTERVAL, BluetoothIterate);
}


static void BluetoothDone () {
  bluetoothTimer.Clear ();

  // Turn off Bluetooth...
  jniEnv->CallStaticVoidMethod (jniClass, midBluetoothSet, JNI_FALSE);
  AndroidExceptionCheck ();
}



// ***** Calls from Android *****


extern "C"
void Java_org_home2l_app_Home2l_initNative (JNIEnv *env, jobject thiz) {
  DEBUG (1, "C call from Java: initNative()");
  env->GetJavaVM (&javaVM);
  ASSERT(javaVM != NULL);
}


extern "C"
void Java_org_home2l_app_Home2l_putEnvNative (JNIEnv *env, jobject thiz, jstring jKey, jstring jValue) {
  const char *key, *value;

  key = env->GetStringUTFChars (jKey, NULL);
  value = env->GetStringUTFChars (jValue, NULL);
  DEBUGF (1, ("Java: Putting '%s=%s' into the environment.", key, value));
  EnvPut (key, value);
  env->ReleaseStringUTFChars (jKey, key);
  env->ReleaseStringUTFChars (jValue, value);
}


/*
extern "C"
void Java_org_home2l_app_Home2l_shellCommand (JNIEnv *env, jobject thiz, jstring jCmd) {
  const char *cmd;
  int status;

  cmd = env->GetStringUTFChars (jCmd, NULL);
  INFOF (("Java: Executing '%s'.", cmd));
  status = system (cmd);
  INFOF (("      ... Status is: %i", status));
  env->ReleaseStringUTFChars (jCmd, cmd);
}
*/





// ***** Init/Done *****


static inline void AndroidInit () {
  CSplitString args;
  const char *p;
  CString cmd, execName;
  bool withSu, withArgs;

  // Run init script ...
  if (envAndroidAutostart) {

    // Analyes parameters, resolve executable ...
    withSu = (envAndroidAutostart[0] == '!');
    args.Set (envAndroidAutostart + (withSu ? 1 : 0), 2);
    if (args.Entries () < 1) ERRORF (("Illegal setting: %s", envAndroidAutostartKey));
    withArgs = (args.Entries () > 1);
    EnvGetHome2lRootPath (&execName, args.Get (0));

    // Construct & run command line ...
    cmd.SetF (withSu ? "su -c '%s%s%s' &" : "%s%s%s &",
              execName.Get (), withArgs ? " " : "", withArgs ? args.Get (1) : "");
    //~ s.SetF ("test -x %1$s/etc/android-init.sh && su -c '%1$s/etc/android-init.sh %2$s' &",
            //~ EnvHome2lRoot (), envAndroidGateway ? envAndroidGateway : CString::emptyStr);
    INFOF (("### Running '%s'...", cmd.Get ()));
    system (cmd.Get ());
  }

  // Init subsystems ...
  SensorInit ();
  BluetoothInit ();
}


#endif // ANDROID




// *************************** Common routines *********************************


#if ANDROID
void SystemPreInit () {
  AndroidPreInit ();
}
#endif


void SystemInit () {
#if ANDROID
  AndroidInit ();
  EnableSync2l ();
#else
  DebianInit ();
#endif
  RcRegisterDriver ("ui", RcDriverFunc_ui);
  SystemWakeup ();
}


void SystemDone () {
#if ANDROID
  jniEnv->CallStaticVoidMethod (jniClass, midAboutToExit);
  AndroidExceptionCheck ();
  SensorDone ();
  BluetoothDone ();
#endif
}



// ******* System mode **********


static void SystemSetMode (ESystemMode _mode) {
  ESystemMode lastMode = systemMode, newMode = _mode;

  // Handle passive bahaviour case ...
  if (envPassiveBehaviour) {
    if (newMode > smBackground && newMode < smStandby) newMode = smStandby;   // do not allow 'smOff'
    //~ if (systemMode == smBackground) _mode = smBackground;   // do not come to front autonomously
  }

  // Physically switch mode...
  if (newMode != lastMode) {
    DEBUGF(1, ("Switching system mode: %i -> %i", (int) lastMode, (int) newMode));
#if ANDROID
    AndroidSetMode (newMode, lastMode);
#else
    CScreen::EmulateStandby (newMode == smStandby);
    CScreen::EmulateOff (newMode <= smOff);
#endif
    systemMode = newMode;
    UiPushUserEvent (evSystemModeChanged, (void *) newMode, (void *) lastMode);
  }
}


ESystemMode SystemGetMode () {
  return systemMode;
}


static void SystemModeUpdate (bool inForeground) {
  // Update the system mode after a change by the drivers for 'rcModeActive'/'rcModeStandby'
  // or when it is known that the app has entered or left background. The UI visibility presently
  // can not be determined precisely, so that heuristics are used. In general, we must ensure
  // that in case of doubt, we do NOT enter background mode ('smBackground') or leave it.
  //
  // 'inForeground' should be set if it is known that the UI is visible. If not set,
  // the state 'smBackground' is not left or entered.
  ESystemMode newMode;

  //~ INFO ("### SystemModeUpdate");

  // Determine new mode according to the 'standby', 'active' resources ...
  //   If the current state is 'smBackground', it is not changed, unless 'valModeActive' and 'valModeStandby'
  //   are both 'false'. This is considered the trigger for forcing the app to foreground again.
  newMode = valModeActive ? smActive
            : valModeStandby ? smStandby
            : smOff;
  if (!inForeground && systemMode == smBackground) newMode = smBackground;
    // If in background state: Remain there unless 'inForeground' is set.
  //~ INFOF(("###   valModeActive = %i, valModeStandby = %i, smActive = %i, smStandby = %i -> %i", (int) valModeActive, (int) valModeStandby, smActive, smStandby, newMode));
  SystemSetMode (newMode);
}


void SystemWakeupActive () {
  //~ INFO ("### SystemWakeupActive");
  rcModeActive->SetRequest (true, "_wakeup", rcPrioNormal, 0, TicksNow () + envStandbyDelay);
  SystemWakeupStandby ();   // setup standby timer to not to shut down too quickly
}


void SystemWakeupStandby () {
  rcModeStandby->SetRequest (true, "_wakeup", rcPrioNormal, 0, TicksNow () + envOffDelay);
}


void SystemActiveLock (const char *reqName, bool withWakeup) {
  //~ INFOF (("### SystemActiveLock ('%s')", reqName));
  rcModeActive->SetRequest (true, reqName, rcPrioNormal);
  if (withWakeup) SystemWakeupActive ();
}


void SystemActiveUnlock (const char *reqName, bool withWakeup) {
  if (withWakeup) {
    if (systemMode == smActive) SystemWakeupActive ();
    else SystemWakeupStandby ();               // make sure to get to standby mode for some time after removing the lock
  }
  rcModeActive->DelRequest (reqName);
}


void SystemStandbyLock (const char *reqName, bool withWakeup) {
  rcModeStandby->SetRequest (true, reqName, rcPrioNormal);
}


void SystemStandbyUnlock (const char *reqName, bool withWakeup) {
  if (withWakeup) SystemWakeupStandby ();
  rcModeActive->DelRequest (reqName);
}



// ******* Entering / leaving background **********


void SystemGoBackground (const char *appStr) {
  //~ INFO ("### SystemGoBackground ()");
#if ANDROID
  AndroidGoBackground (appStr);
#else
  if (appStr) {
    DEBUGF (1, ("SystemLaunchApp ('%s') - ignoring", appStr));
    return;
  }
#endif
  SystemSetMode (smBackground);
}


void SystemGoForeground () {
  //~ INFO ("### SystemGoForeground ()");
#if ANDROID
  AndroidGoForeground ();
#endif
  SystemModeUpdate (true);
}


void SystemReportUiVisibility (bool foreNotBack) {
  if (foreNotBack) SystemModeUpdate (true);
}



// ******* Mute flag **********


void SystemMute (const char *reqName) {
  rcMute->SetRequest (true, reqName, rcPrioNormal);
}


void SystemUnmute (const char *reqName) {
  rcMute->DelRequest (reqName);
}


bool SystemIsMuted () {
  //~ INFOF (("### SystemIsMuted (): %i", (int) valMute));
  return valMute;
}


class CResource *SystemGetMuteRc () {
  return rcMute;
}



// ******* Bluetooth *******


class CResource *SystemGetBluetoothRc () {
  return rcBluetooth;
}


class CResource *SystemGetBluetoothAudioRc () {
  return rcBluetoothAudio;
}


bool SystemBluetoothGetState (bool *retBusy, bool *retAudio) {
  CRcValueState vs, vsAudio;
  bool retOn;

  rcBluetooth->GetValueState (&vs);
  retOn = vs.ValidBool ();

  if (retBusy) *retBusy = !vs.IsValid ();

  if (retAudio) {
    if (retOn && vs.IsValid ()) *retAudio = rcBluetoothAudio->ValidBool ();
    else *retAudio = false;
  }

  //~ INFOF (("### SystemBluetoothGetState () -> %s / %i", vs.ToStr (), (int) retOn));

  return retOn;
}


void SystemBluetoothSet (bool enable) {
  rcBluetooth->SetRequest (enable, NULL, rcPrioNormal, 0, -1000);
}



// ******* Phone State *******


class CResource *SystemGetPhoneStateRc () {
  return rcPhoneState;
}


void SystemReportPhoneState (enum ERctPhoneState _phoneState) {
#if WITH_PHONE
  rcPhoneState->ReportValue (_phoneState);
#endif
}
