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
#include "app_music.H"

#include "resources.H"
#include "streamer.H"

#include <mpd/client.h>



#define UI_SPACE          24  // space between UI groups
#define UI_DISPLAY_SPACE   4  // space around displays
#define UI_CONTROLS_SPACE 16  // space around control buttons
#define UI_DIRNAME_H      72
#define UI_SLIDER_WIDTH   48

#define COL_MAIN_BUTTONS  BROWN
#define COL_BACKGROUND    BROWN
#define COL_DISPLAY       YELLOW
#define COL_PLAY_BUTTONS  DARK_GREY
#define COL_LIST_TITLE    BROWN





// *************************** Environment options *****************************


ENV_PARA_SPECIAL ("music.<MPD>.host", const char *, NULL);
  /* Network host name and optionally port of the given MPD instance.
   *
   * This variable implicitly declares the server with its symbolic name <MPD>.
   * If no port is given, the default port is assumed.
   */

ENV_PARA_INT ("music.port", envMpdDefaultPort, 6600);
  /* Default port for MPD servers
   */

ENV_PARA_SPECIAL ("music.<MPD>.password", const char *, NULL);
  /* Password of the MPD instance (optional, NOT IMPLEMENTED YET)
   */

ENV_PARA_SPECIAL ("music.(<MPD>|any)[.<OUTPUT>].name", const char *, NULL);
  /* Define a display name for an MPD server or an output
   */

ENV_PARA_INT ("music.streamPort", envMpdDefaultStreamPort, 8000);
  /* Default port for HTTP streams coming from MPD servers
   *
   * The setting for a particular server <MPD> can be given by variable keyed
   * "music.<MPD>.streamPort".
   */

ENV_PARA_INT ("music.streamBufferDuration", envStreamerBufferDuration, 1000);
  /* Buffer length [ms] for HTTP streaming.
   */

ENV_PARA_FLOAT ("music.volumeGamma", envDefaultVolumeGamma, 1.0);
  /* Gamma value for the volume controller (default and always used for local outputs)
   *
   * The setting for a particular server <MPD> and optionally  output <OUTPUT> can be
   * given by variable keyed "music.<MPD>.[<OUTPUT>].volumeGamma".
   */

ENV_PARA_STRING ("music.streamOutPrefix", envMpdStreamOutPrefix, "stream");
  /* Name prefix for an output
   *
   * If the output name has the format "<prefix>[<port>]", it is recogized as a
   * output for HTTP streaming, which can be listened to locally. For concenience,
   * the port number can be appended to the stream prefix.
   */

ENV_PARA_STRING ("music.recordOut", envMpdRecordOut, "record");
  /* Name for a recording output
   *
   * If the output name has this name, it is recogized as an output with recording
   * functionality. Such an output is not listed and selectable by the usual output
   * functionality, but activated if and only if a streaming source is played.
   */

ENV_PARA_STRING ("music.streamDirHint", envMpdStreamDirHint, NULL);
  /* MPD directory in which radio streams can probably be found
   *
   * The "go to current" button navigates to the parent directory of the currently playing
   * song. If the song is not a local file, but a (HTTP) stream, this does not work out of the box.
   * This setting optionally defines the directory to go to, if a non-file is currently played.
   */

ENV_PARA_INT ("music.recoveryInterval", envRecoveryInterval, 2000);
  /* Retry interval time if something (presently local streaming) fails.
   */

ENV_PARA_INT ("music.recoveryMaxTime", envRecoveryMaxTime, 10000);
  /* Maximum time to retry if something (presently local streaming) fails.
   */

ENV_PARA_BOOL ("music.autoUnmute", envAutoUnmute, false);
  /* Automatically continue playing if the reason for muting is gone.
   *
   * If 'true', the music player resumes playing if the 'mute' resource
   * changes from 1 to 0. If 'false', the player stays paused. The latter may be
   * useful if there are multiple phones in the room, and the user answers with
   * a phone other than that controlling the player.
   */

ENV_PARA_NOVAR ("var.music.server", const char *, envMpdServer, NULL);
  /* MPD server to connect to first
   */



static const char *MakeEnvKeyPath (const char *base, const char *serverKey, const char *outputKey = NULL, bool forceOutput = false) {
  static CString ret;
  const char *para = base + 6;    // skip "music."

  ret.SetF ("music.%s", para);
  if (serverKey) ret.InsertF (0, "music.%s.%s:", serverKey, para);
  if (outputKey) ret.InsertF (0, "music.any.%s.%s:", outputKey, para);
  if (serverKey && outputKey) ret.InsertF (0, "music.%s.%s.%s:", serverKey, outputKey, para);
  return ret.Get ();
}




// *****************************************************************************
// *                                                                           *
// *                          Headers                                          *
// *                                                                           *
// *****************************************************************************



// ***************** Model-related classes  *********************


#define OUTPUTS_MAX 8     // maximum number of outputs per MPD server


enum EDirEntryType {
  // The ordering of types is relevant for the directory listing order:
  // * Songs mu7st be first, so that the index numbers in the dir list match the play queue.
  // * Directories should be before playlists, so that in the root list the collection comes first.
  detNone = -1,         // unknown
  detSong,              // song (MPD_ENTITY_TYPE_SONG)
  detDirectory,         // directory (MPD_ENTITY_TYPE_DIRECTORY)
  detPlaylist,          // playlist (MPD_ENTITY_TYPE_PLAYLIST)
  //~ detPlaylistRoot,      // VFS: root directory for all MPD playlists '~P/'
  //~ detCurPlaylist        // VFS: current playqueue ('~Q/')
};


class CDirEntry {
public:
  CDirEntry () { type = detNone; duration = 0; }

  EDirEntryType Type () { return type; }
  const char *Uri () { return uri.Get (); }
  const char *Title () { return title.Get (); }
  int Duration () { return duration; }      // only used for songs

protected:
  friend class CMusicPlayer;

  CDirEntry *next;

  EDirEntryType type;
  CString uri, title;
    // 'uri' is the full URI. 'title' is the title as displayed in the directory listing.
    // For local files, 'title' is the MPD tag "title".
    // For http streams, 'title' is the MPD tag 'name', the station name.
    // Information for the display (title, info line) is retrieved from the player.
  int duration;    // for songs only
};


class CMusicPlayer {
public:
  CMusicPlayer ();
  ~CMusicPlayer () { SetServer (-1); }

  static void ClassInit ();
  static void ClassDone () {}

  void SetView (class CScreenMusicMain *_view) { view = _view; }

  // Iteration...
  void Update ();     // Update song and player infos; calls 'On...Changed' callbacks as applicable

  // MPD server(s)...
  static int Servers () { return serverDict.Entries (); }
  static int ServerIdx (const char *key) { return serverDict.Find (key); }
  static const char *ServerKey (int idx) { return serverDict.GetKey (idx); }
  static const char *ServerName (int idx) { return serverDict.Get (idx)->Get (); }     // Server display name

  int Server () { return serverIdx; }
  void SetServer (int idx);
    // 'idx < 0' sets everything in save state (e.g. disconnects from current server) and is used for destruction
  bool ServerConnected () { return mpdConnection != NULL; }
    // 'true' after successful server connection

  bool GetState (CString *ret);        // return stringified state of server (queue and player)
  bool SetState (const char *state);   // apply stringified state to server (queue and player)

  bool RepeatMode () { return serverRepeatMode; }
  bool SetRepeatMode (bool on);

  // Outputs...
  int Outputs () { return outputEntries; }
  const char *OutputKey (int idx) { return outputKey[idx].Get (); }     // MPD name of an output (acts as ID and display name)
  const char *OutputName (int idx) { return outputName[idx].Get (); }   // Display name of an output
  bool OutputCanStream (int idx) { return outputStreamPort[idx] > 0; }   // Streaming output?
  int OutputStreamPort (int idx) { return outputStreamPort[idx]; }   // Streaming output?
  void SetOutput (int idx);
  int Output () { return outputIdx; }

  // Volume...
  int Volume () { return volume; }
  bool SetVolume (int _volume);        // 0 <= volume <= 100

  // Directory browsing...
  int DirEntries () { return dirEntries; }
  CDirEntry *DirEntry (int idx) { return dirList[idx]; }
  int DirPlayingIdx () { return dirPlayingIdx; }      // index of currently playing song; or -1 otherwise
  bool DirIsQueue () { return dirPath[0] == '~' && dirPath[1] == 'Q'; }
    // Does the directory view show the current play queue?

  const char *DirPath () { return dirPath.Get (); }   // path name of directory currently loaded in the browser
  const char *DirPathReadable ();                     // human-readable description of the path
  void DirClear ();
    // URIs are full paths following the MPD convention (no leading '/').
    // The following special cases are recognized:
    //   ~Q         - The current play queue
    //   ~P         - The list of MPD playlists
    //   ~R         - The virtual root directory (presently: MPD playlists + link  MPD root)
    //   ~[P|R]/<list>  - The MPD playlist named <list>
    //   ~0         - Empty directory (e.g. if there is no server connection)
  bool DirLoad (const char *uri);
    // Load a directory ('uri' must be a directory or a VFS playlist)
  bool DirLoadParent (const char *uri);
    // Load parent directory for a given song.
    // If the parent cannot be found, the current queue ("~Q") is loaded automatically.
  bool DirLoadQueue () { return DirLoad (queuePath.Get ()); }
  bool DirReload () { return DirLoad (dirPath.Get ()); }
  int DirFind (const char *uri);        // search for entry 'uri' in the directory; if not found, a value <0 is returned

  // Play queue management...
  const char *QueuePath () { return queuePath.Get (); }
  int QueueSongs () { return queueSongs; }
  bool QueueClear ();                      // clear queue
  bool QueueLoadDir (bool force = false);  // stop playing and load the current directory into the queue
  bool QueueAppendMultiple (const char *uriLines);
  bool QueueAppend (const char *uri);
  bool QueueInsert (int idx, const char *uri);   // Insert before 'idx'
  bool QueueDelete (int idx, int num = 1);

  bool QueueTryLinkDir ();
    // Check if the queue is equivalent to the currently loaded dir and if so, set 'queuePath' accordingly.
    // Return 'true', if 'queuePath' has been changed (successfully).
  void QueueUnlink () { queuePath.SetC ("~Q"); }
  bool QueueIsLinked () { return queuePath[0] != '~' || queuePath[1] != 'Q'; }
  bool QueueIsDir () { return dirPath.Compare (queuePath) == 0; }

  // Song info and navigation in queue...
  int SongIdx () { return songQueueIdx; }
  bool SetSongAndPos (int idx, int pos);     // set active song and position
  bool SetSong (int idx) { return SetSongAndPos (idx, 0); }   // set active song
  bool PlaySong (int idx);                   // set active song and start playing

  bool HaveSong () { return songQueueIdx >= 0; }
  const char *SongUri () { return songUri.Get (); }
  const char *SongTitle () { return songTitle.Get (); }
  const char *SongSubtitle () { return songSubtitle.Get (); }
  int SongDuration () { return songDuration; }

  // Player state...
  ERctPlayerState PlayerState () { return playerState; }

  bool IsPlaying () { return playerState == rcvPlayerPlaying; }
  bool IsPlayingOrShouldBe () { return playerState == rcvPlayerPlaying || playerIsMuted; }
    // Returns 'true', if the player is either actually playing or should be playing, but is
    // not for a reason not intended by the user.
    // Use this variant to decide if background mode can be quit.
  bool IsPlayingForSure (int minDb = -INT_MAX);
    // Returns 'true', if the player actually playing and is guaranteed that the user can hear something.
    // (At least this is the goal - there may be some more holes to fix, for example, the case that a readio
    // stream contains silence.)
    // Use this variant for an alarm clock to check if the user gets woken up or if some other measure has to be taken.
  bool IsPaused () { return playerState == rcvPlayerPaused; }
  bool IsStopped () { return playerState == rcvPlayerStopped; }
  bool Play ();
  bool Pause ();
  bool Stop ();

  int SongPos () { return playerSongPos; }
  bool SetSongPos (int sec) { return SetSongAndPos (songQueueIdx, sec); }

  // Misc. commands...
  bool SongNext ();
  bool SongPrev ();

  bool SkipForward ();
  bool SkipBack ();

  // Error handling...
  bool InErrorRecovery () { return errorPermanent; }      // in error state (still hoping to recover)
  bool InError () { return errorPermanent; }              // in error state (non-recoverable)
  const char *GetErrorMsg () { return errorMsg.Get (); }  // Get last error message
  bool ErrorReasonIsServer () { return !ServerConnected (); }         // Error can be reset by changing the server
  bool ErrorReasonIsStreamer () { return !ErrorReasonIsServer (); }   // Error can be reset by changing the output (implementation may change in th future, presently there are only two reasons)
  bool ErrorReasonIsOther () { return false; }

protected:
  void SetErrorMsg (const char *fmt, const char *msg);
  void SetErrorState (bool _errorRecovery, bool _errorPermanent);   // only one of the two flags may be set
  void ClearErrorState () { SetErrorState (false, false); }
  void CheckAndHandleMpdError ();
  bool StreamerShouldBeOn () { return IsPlaying () && OutputCanStream (outputIdx); }
  void StreamerStartOrStop ();
  void StreamerWatchdog ();
  void ReadOutputs ();

  class CScreenMusicMain *view;

  // Change/update management...
  bool inUpdate, changedQueue;

  // Server...
  static CDictFast<CString> serverDict;
  struct mpd_connection *mpdConnection;
  bool mpdInErrorHandling;
  CString mpdHost;
  int serverIdx;          // index of currently connected server
  bool serverRepeatMode;

  // Outputs and volume...
  CString outputKey[OUTPUTS_MAX];
  CString outputName[OUTPUTS_MAX];  // display name(s)
  unsigned outputId[OUTPUTS_MAX];
  int outputStreamPort[OUTPUTS_MAX];   // 0 == no stream output port
  int outputEntries;
  int outputIdxRecorder;    // index of the recorder output (or -1 if not present)

  int outputIdx;        // currently selected output
  int volume, volumeRaw;
  float volumeGamma;

  // Browser state ('dir...')...
  CString dirPath;
  CDirEntry **dirList;
  int dirEntries;
  int dirPlayingIdx;    // index of currently active entry (or -1 if not present in this dir); to be updated on song change

  // Queue...
  CString queuePath;    // dir/palylist currently loaded into the queue
  int queueSongs;

  // Currently playing song ...
  int songQueueIdx;
  CString songUri, songTitle, songSubtitle;
  int songDuration;
  bool songIsStream;

  // Player state (inner song)...
  ERctPlayerState playerState;
  bool playerIsMuted;    // If true, playing will continue if unmuted and player in pause state
  int playerSongPos;
  int playerBitrate, playerFreq, playerChannels;

  // Streaming...
  EStreamerState streamerState;

  // Error state & recovery...
  bool errorRecovery, errorPermanent;
  TTicksMonotonic tRecoveryLast, tRecoveryNext;
  CString errorMsg;
};





// ***************** View-related classes *********************


class CListboxDirectory: public CListbox {
  public:
    CListboxDirectory () { playingSong = -1; }

    void Setup (SDL_Rect _area);

    void SetPlayingSong (int _playingSong);
    int PlayingSong () { return playingSong; }

  protected:
    virtual SDL_Surface *RenderItem (CListboxItem *item, int idx, SDL_Surface *surf);

    int playingSong;
};


class CScreenMusicMain: public CScreen, public CTimer {
  public:
    CScreenMusicMain ();
    ~CScreenMusicMain ();

    void Setup ();

    // UI Callbacks...
    void OnButtonPushed (CButton *btn, bool longPush);
    void OnListItemPushed (CListbox *, int idx, bool longPush);
    void OnPosSliderValueChanged (CSlider *slider, int val, int lastVal);
    void OnVolSliderValueChanged (CSlider *slider, int val, int lastVal);

    virtual void Activate (bool on = true);   // from 'CScreen'
    virtual void OnTime ();                   // from 'CTimer'

    // Actions ...
    void PlayerOn () { isStarting = true; ConnectServer (); player.Play (); UpdateActiveState (); }
    void PlayerOff () { player.Stop (); UpdateActiveState (); }  // for 'appOpLongPush' or long-push on "back" button
      // This should pause or stop the player in a way that it is a) silent and b) it can remain in
      // for a longer time. Executing "Stop" may be a bit uncomfortable, since unlike "Pause", the
      // song position is lost. However, it is not clear if in the paused state, the underlying MPD
      // remains active in some way, and e.g. keeps the file open and prevents the disk from
      // spinning down.
    CMusicPlayer *Player () { return &player; }

    // Callbacks for the model class (music player, streamer)...
    void OnServerChanged (int idx, bool errRecovery, bool errPermanent);  // Server or error state changed
    void OnOutputChanged (int idx);           // Selected output changed
    void OnRepeatModeChanged (bool repeatOn); // Repeat mode changed
    void OnStreamerStateChanged (EStreamerState state);
    void OnVolumeChanged (int volume);
    void OnDirChanged (int idx0, int idx1);   // The browser directory changed (size and/or any entries between the indices)
    void OnSongChanged (ERctPlayerState state, int songs, int idx, int duration);
    void OnPlayerStateChanged (ERctPlayerState state);
    void OnSongPosChanged (ERctPlayerState state, int songPos, int bitrate, int freq, int channels);

  protected:
    void ConnectServer ();
    void UpdateActiveState ();      // to be is called on screen (de-)activation or player state changes
    void UpdateBluetooth ();

    void DisplaySetup ();
    void DisplayClearAndDrawSong (ERctPlayerState state, int songs, int idx, int duration);
    void DisplayDrawSongPos (ERctPlayerState state, int songPos);
    void DisplayDrawPlayerState (ERctPlayerState state);
    void DisplayDrawInfoLine (ERctPlayerState state, int bitrate = 0, int freq = 0, int channels = 0);

    void RunServerMenu (int xPos, bool transfer);
    void RunOutputMenu (int xPos);

    // Model...
    CMusicPlayer player;
    bool isPlayingActive;   // 'true' if screen is active OR music is playing in the background
    bool isStarting;        // 'true' if the player has been started without activating the screen (is reset when 'isPlayingActive == true' or a permanent error is reached)

    // Button bar & background...
    CButton *buttonBar;
    CWidget wdgBackground;

    // Player...
    CWidget wdgDisplay;
    CSlider sliderPos, sliderVol;
    CButton btnPosBack, btnPosForward, btnVolDown, btnVolUp;
    CButton btnSongPrev, btnSongNext, btnStop, btnPlayPause;

    // Display...
    bool dispHaveSong, dispHaveServer;
    TTF_Font *dispFontSmall, *dispFontLarge, *dispFontLargeButSmaller;
    SDL_Rect dispRect, dispRectPlayerState, dispRectPlayerTime, dispRectInfo;

    // Directory...
    CButton btnDirTitle;
    SDL_Surface *surfDirTitleLabel;
    CListboxDirectory listDir;
};





// *****************************************************************************
// *                                                                           *
// *                          Implementations                                  *
// *                                                                           *
// *****************************************************************************



// *************************** CMusicPlayer ************************************


CDictFast<CString> CMusicPlayer::serverDict;


static bool MpdUriIsStream (const char *uri) {
  // Check if an URI is external (e.g. a stream) and not a file in the local database.
  return strstr (uri, ":/") != NULL;
}


CMusicPlayer::CMusicPlayer () {
  view = NULL;

  inUpdate = false;
  changedQueue = true;

  mpdConnection = NULL;

  mpdInErrorHandling = false;

  dirList = NULL;
  dirEntries = 0;
  dirPlayingIdx = -1;

  streamerState = strOff;

  errorRecovery = errorPermanent = false;

  serverIdx = -2;
  SetServer (-1);
}


void CMusicPlayer::ClassInit () {
  CSplitString splitVarName;
  CString s;
  const char *serverId, *dispName, *hostName;
  int n, idx0, idx1;

  // Discover MPD servers...
  EnvGetPrefixInterval ("music.", &idx0, &idx1);
  for (n = idx0; n < idx1; n++) {
    splitVarName.Set (EnvGetKey (n), 4, ".");
    if (splitVarName.Entries () == 3) if (strcmp (splitVarName.Get (2), "host") == 0) {
      serverId = splitVarName.Get (1);

      // Add server entry...
      dispName = EnvGet (StringF (&s, "music.%s.name", serverId));
      if (!dispName) dispName = serverId; // WARNINGF (("Missing environment setting '%s'", s.Get ()));
      hostName = EnvGet (StringF (&s, "music.%s.host", serverId));    // only for sanity checking
      if (!hostName) WARNINGF (("Missing environment setting '%s'", s.Get ()));
      if (dispName && hostName) {
        s.SetC (dispName);
        serverDict.Set (serverId, &s);
      }
    }
  }

  //~ serverDict.Dump ();
}





// ********** Helpers **********


void CMusicPlayer::SetErrorMsg (const char *fmt, const char *msg) {
  if (!fmt) errorMsg.SetC ("(No error)");
  else {
    errorMsg.SetF (fmt, msg);
    WARNING (errorMsg.Get ());
  }
}


void CMusicPlayer::SetErrorState (bool _errorRecovery, bool _errorPermanent) {
  //~ INFOF (("### SetErrorState (%i, %i)", (int) _errorRecovery, (int) _errorPermanent));
  if (_errorPermanent) _errorRecovery = false;      // permanent error superseeds recoverable one
  if (_errorRecovery != errorRecovery || _errorPermanent != errorPermanent) {
    errorRecovery = _errorRecovery;
    errorPermanent = _errorPermanent;
    if (view) view->OnServerChanged (serverIdx, _errorRecovery, _errorPermanent);
  }
}


void CMusicPlayer::CheckAndHandleMpdError () {
  const char *msg;
  enum mpd_error mpdError;

  // Avoid recursive calls...
  if (mpdInErrorHandling) return;
  mpdInErrorHandling = true;

  // Get error...
  mpdError = mpd_connection_get_error (mpdConnection);
  if (mpdError != MPD_ERROR_SUCCESS) {

    // Get and show error...
    msg = mpd_connection_get_error_message (mpdConnection);
    SetErrorMsg ("MPD: %s", msg);

    // Recover from error...
    if (!mpd_connection_clear_error (mpdConnection)) {

      // Simple clearing did not work: Try to reconnect...
      mpd_connection_free (mpdConnection);
      mpdConnection = NULL;
      SetServer (serverIdx);
      if (!mpdConnection) SetErrorState (false, true);
    }
  }

  // Done...
  mpdInErrorHandling = false;
}


void CMusicPlayer::StreamerStartOrStop () {
  if (StreamerShouldBeOn ()) {
    if (StreamerState () == strOff)
      StreamerStart (mpdHost.Get (), OutputStreamPort (outputIdx), envStreamerBufferDuration);
  }
  else StreamerStop ();
}


void CMusicPlayer::StreamerWatchdog () {
  TTicksMonotonic now;
  const char *msg;

  // Sanity...
  if (errorPermanent) return;     // we have a permanent error => helpless to try anything else
  if (!StreamerShouldBeOn ()) {   // no need for the streamer => clear recovery mode
    if (errorRecovery && ErrorReasonIsStreamer ()) ClearErrorState ();
    return;
  }

  // Get any error message if present ...
  msg = StreamerGetError ();
  if (msg[0]) SetErrorMsg ("GStreamer: %s", msg);

  // Check if there is an error ...
  //~ switch (streamerState) {
    //~ case strOn:    ok = true;          break;  // no problem
    //~ case strBusy:  ok = errorRecovery; break;  // stay alarmed
    //~ default:       ok = false;                 // we have a (new) problem
  //~ }

  // Recovery ...
  if (streamerState == strOn) {

    // Everything is ok (again)...
    if (ErrorReasonIsStreamer ()) ClearErrorState ();
  }
  else if (streamerState == strError || streamerState == strOff) {
    now = TicksMonotonicNow ();

    // Eventually start recovery mode ...
    if (!errorRecovery) {
      SetErrorState (true, false);
      tRecoveryNext = now;
      tRecoveryLast = now + envRecoveryMaxTime;
    }

    // Check if we have tried too long ...
    if (now >= tRecoveryLast) {
      Pause ();
      SetErrorState (false, true);
      if (view) if (view->IsActive ()) RunErrorBox (GetErrorMsg ());
        // Show error dialog, but only if the music screen is active
    }
    else if (now >= tRecoveryNext) {

      // Do Recovery actions...
      StreamerStop ();
      StreamerStartOrStop ();

      // Schedule next recovery try...
      tRecoveryNext = now + envRecoveryInterval;
    }
  }
}





// ********** Updating & connecting **********


void CMusicPlayer::Update () {
  struct mpd_status *mpdStatus;
  const struct mpd_audio_format *mpdAudioFormat;
  struct mpd_song *mpdSong;
  ERctPlayerState _playerState;
  EStreamerState _streamerState;
  const char *songArtist, *songDate, *streamTitle;
  int _volumeRaw, _queueSongs, _songIdx, _songPos;
  bool changedSong, _repeat, _songIsStream;

  // Sanity...
  if (!ServerConnected () || inUpdate) return;
  inUpdate = true;    // avoid recursive calls to this method

  // Handle unhandled errors...
  CheckAndHandleMpdError ();
  if (!ServerConnected ()) return;
  StreamerWatchdog ();

  // Handle (un)muting...
  if (SystemIsMuted ()) {
    if (!playerIsMuted && playerState == rcvPlayerPlaying) {
      //~ INFO ("### Muting...");
      Pause ();
      playerIsMuted = true;
  }
  }
  else {
    if (playerIsMuted && playerState != rcvPlayerStopped) {
      //~ INFO ("### Unmuting...");
      if (envAutoUnmute) Play ();
      playerIsMuted = false;
    }
  }

  // Update streamer state (if applicable)...
  _streamerState = StreamerIterate ();
  if (_streamerState != streamerState) {
    streamerState = _streamerState;
    if (view) view->OnStreamerStateChanged (_streamerState);
  }

  // Get the run status...
  mpdStatus = mpd_run_status (mpdConnection);
  if (!mpdStatus) {
    CheckAndHandleMpdError ();
    inUpdate = false;
    return;
  }

  // Repeat mode...
  _repeat = mpd_status_get_repeat (mpdStatus);
  if (_repeat != serverRepeatMode) {
    serverRepeatMode = _repeat;

    // Notify view...
    if (view) view->OnRepeatModeChanged (_repeat);
  }

  // Volume...
  _volumeRaw = mpd_status_get_volume (mpdStatus);
  if (_volumeRaw != volumeRaw) {
    volumeRaw = _volumeRaw;
    if (_volumeRaw < 0) volume = -1;
    else {
      volume = round (pow (((double) _volumeRaw) / 100.0, 1.0 / volumeGamma) * 100.0);
      //~ INFOF (("### CMusicPlayer::Update: volume = %i, _volumeRaw = %i, gamma = %f", volume, _volumeRaw, volumeGamma));
      if (volume < 0) volume = 0;
      if (volume > 100) volume = 100;
    }

    // Notify view...
    if (view) view->OnVolumeChanged (volume);
  }

  // Queue change...
  _queueSongs = (int) mpd_status_get_queue_length (mpdStatus);
  if (_queueSongs != queueSongs) QueueUnlink ();
    // Queue was changed from outside: We must unlink the queue path.

  // Song (or queue) change...
  _songIdx = mpd_status_get_song_pos (mpdStatus);
  if (_songIdx < 0 && playerState == rcvPlayerStopped) {
    // This is a dirty hack to get a nicer display: If the player is stopped,
    // report the first song as being active. This will be the one played anyway
    // if the "Play" buttion will be pushed.
    _songIdx = 0;
  }
  changedSong = (_queueSongs != queueSongs || _songIdx != songQueueIdx || changedQueue);
  if (changedSong) {
    queueSongs = _queueSongs;
    songQueueIdx = _songIdx;
    songDuration = 0;   // Default
    changedQueue = false;

    mpdSong = mpd_run_get_queue_song_pos (mpdConnection, _songIdx);
    //~ INFO ("### checked song...");
    if (mpdSong) {
      songUri.Set (mpd_song_get_uri (mpdSong));
      songDuration = (int) mpd_song_get_duration (mpdSong);

      // Enable/disable recording for HTTP streams ...
      _songIsStream = MpdUriIsStream (songUri.Get ());
      //~ INFOF (("### ... new (%i, stream = %i)", _songIdx, (int) songIsStream));
      if (_songIsStream != songIsStream) {
        if (outputIdxRecorder >= 0) {
          if (_songIsStream) mpd_run_enable_output (mpdConnection, outputId[outputIdxRecorder]);
          else mpd_run_disable_output (mpdConnection, outputId[outputIdxRecorder]);
        }
        songIsStream = _songIsStream;
      }

      // Set subtitle intelligently...
      if (songIsStream) {
        // HTTP stream => use station name
        songSubtitle.Set (mpd_song_get_tag (mpdSong, MPD_TAG_NAME, 0));
        if (songSubtitle.IsEmpty () && songQueueIdx >= 0) {
          if (QueueIsDir ()) songSubtitle.Set (dirList [songQueueIdx]->Title ());
        }
      }
      else {
        // no HTTP stream => assume local file, display artist and year
        songArtist = mpd_song_get_tag (mpdSong, MPD_TAG_ARTIST, 0);
        songDate = mpd_song_get_tag (mpdSong, MPD_TAG_DATE, 0);
        if (songArtist) {
          if (songDate) songSubtitle.SetF ("%s %s", songArtist, songDate);
          else songSubtitle.Set (songArtist);
        }
        else if (songDate) songSubtitle.Set (songDate);
        else songSubtitle.Clear ();
      }

      // Set main title...
      songTitle.Set (mpd_song_get_tag (mpdSong, MPD_TAG_TITLE, 0));
      if (songTitle.IsEmpty ()) songTitle.Set (songSubtitle);

      // Done with the song ...
      mpd_song_free (mpdSong);
    }
    else {
      songUri.Clear ();
      songTitle.Clear ();
      songSubtitle.Clear ();
      songIsStream = false;
      if (outputIdxRecorder >= 0) mpd_run_disable_output (mpdConnection, outputId[outputIdxRecorder]);
        // this must always happen together with 'songIsStream = false'
    }
    dirPlayingIdx = DirFind (songUri.Get ());
    //~ INFOF (("### OnSongChanged ()..."));

    // Notify view...
    if (view) view->OnSongChanged (playerState, queueSongs, songQueueIdx, songDuration);
  }

  // Player state ...
  switch (mpd_status_get_state (mpdStatus)) {
    case MPD_STATE_PLAY: _playerState = rcvPlayerPlaying; break;
    case MPD_STATE_PAUSE: _playerState = rcvPlayerPaused; break;
    case MPD_STATE_STOP: default: _playerState = rcvPlayerStopped; break;
  }
  if (_playerState != playerState) {
    playerState = _playerState;

    // Start or stop streamer...
    StreamerStartOrStop ();

    // Notify view...
    if (view) view->OnPlayerStateChanged (_playerState);
  }

  // Song position...
  _songPos = (int) mpd_status_get_elapsed_time (mpdStatus);
  if (_songPos != playerSongPos || changedSong) {

    // If we have a stream: Re-check the title, which now contains changing info...
    if (songIsStream) {
      mpdSong = mpd_run_get_queue_song_pos (mpdConnection, _songIdx);
      if (mpdSong) streamTitle = mpd_song_get_tag (mpdSong, MPD_TAG_TITLE, 0);
      else streamTitle = NULL;
      if (streamTitle) {
        if (songTitle.Compare (streamTitle) != 0) {
          songTitle.Set (streamTitle);

          // Notify view...
          if (view) view->OnSongChanged (playerState, queueSongs, songQueueIdx, songDuration);
        }
      }
      if (mpdSong) mpd_song_free (mpdSong);
    }

    // Update song position...
    playerSongPos = _songPos;
    playerBitrate = (int) mpd_status_get_kbit_rate (mpdStatus);
    mpdAudioFormat = mpd_status_get_audio_format (mpdStatus);
    if (mpdAudioFormat) {
      playerFreq = (int) mpdAudioFormat->sample_rate;
      playerChannels = (int) mpdAudioFormat->channels;
    }
    else playerFreq = playerChannels = 0;

    // Notify view...
    if (view) view->OnSongPosChanged (playerState, songIsStream ? -1 : playerSongPos, playerBitrate, playerFreq, playerChannels);
  }

  // Done...
  mpd_status_free (mpdStatus);
  inUpdate = false;
}


void CMusicPlayer::SetServer (int idx) {
  CMessageBox *popup;
  CString s;
  enum mpd_error mpdError;
  const char *mpdId, *msg;
  int mpdPort;

  //~ INFOF (("### SetServer (%i -> %i)", serverIdx, idx));

  // Check if we can ommit something...
  if (ServerConnected () && idx == serverIdx) return;

  // Clear error state...
  if (ErrorReasonIsServer ()) ClearErrorState ();

  // Stop streaming if adequate...
  if (idx != serverIdx) {
    StreamerStop ();
    if (view) view->OnStreamerStateChanged ( (streamerState = strOff) );
  }

  // Disconnect from old server if previously connected...
  if (ServerConnected ()) {
    mpd_connection_free (mpdConnection);
    mpdConnection = NULL;
  }
  if (serverIdx >= 0) DirClear ();

  // Clear all variables...
  //   NOTE: This part is also used when invoked from the constructor...
  mpdHost.Clear ();
  mpdPort = 0;
  serverIdx = -1;
  serverRepeatMode = false;

  outputEntries = 0;
  outputIdx = -1;

  volume = volumeRaw = -2;
  volumeGamma = 1.0;

  QueueUnlink ();
  queueSongs = 0;

  songQueueIdx = songDuration = -1;
  songIsStream = false;

  playerSongPos = 0;
  playerState = rcvPlayerStopped;

  playerBitrate = playerFreq = playerChannels = 0;
  playerIsMuted = false;

  errorRecovery = errorPermanent = false;

  // Connect to new one...
  if (idx >= 0) {
    serverIdx = idx;
    mpdId = serverDict.GetKey (idx);

    // Store the new server...
    EnvPut (envMpdServerKey, mpdId);

    if (!EnvGetHostAndPort (StringF (&s, "music.%s.host", mpdId), &mpdHost, &mpdPort, envMpdDefaultPort)) {
      mpdHost.SetC ("localhost");
      mpdPort = envMpdDefaultPort;
    }
    //~ TBD: ENV_PARA_SPECIAL ("music.password.<MPD>");

    // Try to connect...
    popup = NULL;
    if (view) if (view->IsActive ())  // Show message box only if the music screen is active
      popup = StartMessageBox (_("Connecting ..."), StringF (&s, "%s:%i", mpdHost.Get (), mpdPort), NULL, mbmNone);
    mpdConnection = mpd_connection_new (mpdHost.Get (), mpdPort, 3000);    // timeout = 3000ms
    ASSERT (mpdConnection != NULL);   // 'mpdConnection == NULL' only occurs when out of memory
    if (popup) StopMessageBox (popup);

    // Handle error...
    mpdError = mpd_connection_get_error (mpdConnection);
    if (mpdError != MPD_ERROR_SUCCESS) {
      msg = mpd_connection_get_error_message (mpdConnection);
      SetErrorMsg ("MPD: %s", msg);
      mpd_connection_free (mpdConnection);
      mpdConnection = NULL;
      SetErrorState (false, true);
      if (view) if (view->IsActive ()) RunErrorBox (GetErrorMsg ());
        // Show error dialog, but only if the music screen is active
    }

    // Success...
    if (ServerConnected ()) {

      // Set some constant server options as needed for us...
#if LIBMPDCLIENT_CHECK_VERSION(2, 13, 0)
      mpd_connection_set_keepalive (mpdConnection, true);
#endif
      mpd_run_random (mpdConnection, false);
      mpd_run_single (mpdConnection, false);
      mpd_run_consume (mpdConnection, false);

      // Read outputs from server...
      ReadOutputs ();
      if (outputIdxRecorder >= 0) mpd_run_disable_output (mpdConnection, outputId[outputIdxRecorder]);
        // this must always happen together with 'songIsStream = false' (reset above)
    }

    // Notifications...
    //   The following calls may recursively call this method again, particularly via
    //   the error handling mechanism.
    if (view) view->OnServerChanged (serverIdx, errorRecovery, errorPermanent);
    Update ();

    // Navigate browser to current song ...
    if (ServerConnected ()) {
      DirLoadParent (SongUri ());               // Load parent of the current song.
      if (!QueueIsLinked ()) DirLoadQueue ();   // If not identical to the play queue: Load the play queue.
    }
  }
}


bool CMusicPlayer::GetState (CString *ret) {
  struct mpd_entity *mpdEntity;
  const char *queueUri;
  bool mpdOk;

  // Sanity...
  if (!ServerConnected ()) return false;

  // Player state and directories...
  ret->SetF ("%s\n%s\n%i:%i:%i",
             dirPath.Get (), queuePath.Get (),
             songQueueIdx, playerSongPos, (int) playerState);

  // Play queue...
  mpdOk = mpd_send_list_queue_meta (mpdConnection);
  while (mpdOk && (mpdEntity = mpd_recv_entity (mpdConnection)) != NULL) {
    if (mpd_entity_get_type (mpdEntity) == MPD_ENTITY_TYPE_SONG) {
      queueUri = mpd_song_get_uri (mpd_entity_get_song (mpdEntity));
      //~ INFOF (("###     got '%s'", queueUri));
      if (queueUri) ret->AppendF ("\n%s", queueUri);
    }
  }
  mpdOk = mpdOk && mpd_response_finish (mpdConnection);
  if (!mpdOk) CheckAndHandleMpdError ();

  // Done...
  return mpdOk;
}


bool CMusicPlayer::SetState (const char *state) {
  CSplitString arg;
  int _songQueueIdx, _playerSongPos, _playerState;

  // Preparation & sanity...
  if (!state || !ServerConnected ()) return false;
  arg.Set (state, 4, "\n");
  if (arg.Entries () < 3) return false;

  // Parse player state and queue directory...
  if (sscanf (arg[2], "%i:%i:%i", &_songQueueIdx, &_playerSongPos, &_playerState) != 3) return false;
  queuePath.Set (arg[1]);

  // Load queue...
  QueueClear ();
  if (arg.Entries () >= 4) QueueAppendMultiple (arg[3]);

  // Resume playing...
  if (!SetSongAndPos (_songQueueIdx, _playerSongPos)) return false;
  switch ((ERctPlayerState) _playerState) {
    case rcvPlayerStopped:   Stop ();    break;
    case rcvPlayerPaused:    Pause ();   break;
    case rcvPlayerPlaying:   Play ();    break;
    default:          Stop ();    return false;
  }

  // Load directory...
  if (!DirLoad (arg[0])) DirLoadQueue ();

  // Done...
  return true;
}


bool CMusicPlayer::SetRepeatMode (bool on) {
  bool mpdOk;

  if (!ServerConnected ()) return false;
  mpdOk = mpd_run_repeat (mpdConnection, on);
  if (mpdOk) {
    serverRepeatMode = on;
    if (view) view->OnRepeatModeChanged (on);
  }
  return mpdOk;
}


void CMusicPlayer::ReadOutputs () {
  struct mpd_output *mpdOutput = NULL;
  struct mpd_status *mpdStatus;
  CString s;
  const char *name;
  int n, mpdPort, firstEnabledIdx;
  bool mpdOk;

  outputEntries = n = 0;
  firstEnabledIdx = outputIdxRecorder = -1;
  mpdOk = mpd_send_outputs (mpdConnection);
  while (mpdOk && n < OUTPUTS_MAX && (mpdOutput = mpd_recv_output (mpdConnection)) ) {

    // Key, name, and ID...
    outputKey[n].Set (mpd_output_get_name (mpdOutput));
    outputId[n] = mpd_output_get_id (mpdOutput);
    if (outputKey[n].Compare (envMpdRecordOut) == 0) {

      // Recording output...
      outputIdxRecorder = n;
    }
    else {

      // Normal output...
      name = EnvGet (
          StringF (&s, "music.%s.%s.name:music.any.%s.name",
                        ServerKey (serverIdx), outputKey[n].Get (), outputKey[n].Get ())
        );
      outputName[n].SetC (name ? name : outputKey[n].Get ());

      // First enabled output...
      if (firstEnabledIdx < 0) if (mpd_output_get_enabled (mpdOutput)) firstEnabledIdx = n;

      // Stream? ...
      //~ INFOF (("### Output #%i: %s", n, outputKey[n].Get ()));
      if (strncmp (outputKey[n].Get (), envMpdStreamOutPrefix, strlen (envMpdStreamOutPrefix)) == 0) {
        // Yes...
        outputStreamPort[n] = envMpdDefaultStreamPort;
        EnvGetInt (MakeEnvKeyPath (envMpdDefaultStreamPortKey, ServerKey (serverIdx), outputKey[n].Get ()), &outputStreamPort[n]);
        if (sscanf ("%i", outputKey[n] + strlen (envMpdStreamOutPrefix), &mpdPort) == 1)
          outputStreamPort[n] = mpdPort;
        //~ INFOF (("###    Stream output (%i): Port %i", n, outputStreamPort[n]));
      }
      else outputStreamPort[n] = 0;   // No.

      // Count...
      n++;
    }

    // Done with this MPD output...
    mpd_output_free (mpdOutput);
  }
  if (mpdOk) mpdOk = mpd_response_finish (mpdConnection);
  if (n <= 0) mpdOk = false;    // no outputs

  // Set the output ...
  if (mpdOk) {
    outputEntries = n;
    SetOutput (firstEnabledIdx < 0 ? 0 : firstEnabledIdx);

    // WORKAROUND [2018-02-04]: Reanimate a potentially dead mixer...
    //   If a USB audio stick is unplugged, MPD disables the hardware mixer and
    //   does not enable it again when the device is plugged in again. To overcome
    //   this, the output is disabled and enabled again here.
    mpdStatus = mpd_run_status (mpdConnection);
    if (mpdStatus) {
      if (mpd_status_get_volume (mpdStatus) <= 0) {
        mpd_run_disable_output (mpdConnection, outputId[outputIdx]);
        mpd_run_enable_output (mpdConnection, outputId[outputIdx]);
      }
      mpd_status_free (mpdStatus);
    }
  }
}


void CMusicPlayer::SetOutput (int idx) {
  CString s;
  int n;

  //~ INFOF (("### CMusicPlayer::SetOutput (%i)", idx));

  // Sanity...
  if (!ServerConnected ()) return;
  if (ErrorReasonIsStreamer ()) ClearErrorState ();

  // Enable the selected output and disable all others...
  //   Note: We should first enable, then disable. Otherwise MPD may stop playing.
  mpd_run_enable_output (mpdConnection, outputId[idx]);
  for (n = 0; n < outputEntries; n++) {
    if (n != idx) mpd_run_disable_output (mpdConnection, outputId[n]);
  }
  outputIdx = idx;

  // Determine new volume gamma...
  volumeGamma = envDefaultVolumeGamma;
  EnvGetFloat (MakeEnvKeyPath ("music.volumeGamma", ServerKey (serverIdx), OutputKey (outputIdx)), &volumeGamma);
  //~ INFOF (("### volumeGamma = %f", volumeGamma));
  //~ INFOF (("### keys = %s", MakeEnvKeyPath ("music.volumeGamma", ServerKey (serverIdx), OutputKey (outputIdx))));

  // Notify view & update streamer...
  StreamerStartOrStop ();
  if (view) view->OnOutputChanged (idx);
}


bool CMusicPlayer::SetVolume (int _vol) {
  bool mpdOk;
  int rawVolume;

  if (!ServerConnected ()) return false;

  if (_vol < 0) _vol = 0;
  if (_vol > 100) _vol = 100;
  rawVolume = round (pow (((double) _vol) / 100.0, volumeGamma) * 100.0);

  //~ INFOF (("### CMusicPlayer::SetVolume: volume = %i, rawVolume = %i, gamma = %f", _vol, rawVolume, volumeGamma));

  if (rawVolume < 0) rawVolume = 0;
  if (rawVolume > 100) rawVolume = 100;

  mpdOk = mpd_run_set_volume (mpdConnection, (unsigned) rawVolume);
  volume = mpdOk ? _vol : -1;
  if (view) view->OnVolumeChanged (volume);
  return mpdOk;
}





// ********** Directory browsing **********


static int PCDirEntryCompare (const void *l, const void *r) {
  CDirEntry *el = (* (CDirEntry **) l);
  CDirEntry *er = (* (CDirEntry **) r);

  if (el->Type () != er->Type ()) return el->Type () - er->Type ();
  return strcmp (el->Uri (), er->Uri ());
}


void CMusicPlayer::DirClear () {
  int n;

  dirPath.SetC ("~0");
  if (dirList) {
    for (n = dirEntries - 1; n >= 0; n--) delete dirList [n];
    delete [] dirList;
    dirList = NULL;
  }
  dirEntries = 0;
  dirPlayingIdx = -1;

  if (view) view->OnDirChanged (0, 0);
}


const char *CMusicPlayer::DirPathReadable () {
  const char *p = dirPath.Get ();

  if (p[0] == '\0') return _("* Music Collection *");      // collection main
  if (p[0] != '~') return p;                      // path
  if (p[2] == '/') return p + 3;                  // playlist name
  switch (p[1]) {
    case 'Q': return _("* Current Queue *");
    case 'P': return _("* Playlists *");
    case 'R': return _("* Main *");
    case '0': return CString::emptyStr;
    default: break;
  }
  return p;     // default/strange: Return verbosely
}


bool CMusicPlayer::DirLoad (const char *uri) {
  CString s;
  struct mpd_entity *mpdEntity;
  const struct mpd_directory *mpdDir;
  const struct mpd_playlist *mpdPlaylist;
  const struct mpd_song *mpdSong;
  enum mpd_entity_type mpdType;
  CDirEntry *first, *entry;
  int n;
  bool ok, mpdOk, lsPlaylists, virtualRoot, doSort;

  //~ INFOF (("### DirLoad ('%s')", uri));

  // Sanity...
  if (!ServerConnected ()) return false;
  if (!uri) uri = "";

  // Store and sanitize URI, clear pre-existing list...
  s.Set (uri);    // copy uri (may point to a previous directory entry)
  s.PathNormalize ();
  s.PathRemoveTrailingSlashes ();
  DirClear ();
  dirPath.SetO (s.Disown ());
  uri = dirPath.Get ();

  // Send list command...
  lsPlaylists = false;    // select directories and songs by default
    // MPD lists its own playlists together with the database root directory.
    // For these reasons, the 'lsPlaylists' setting is used to select whether
    // playlists or directorys should be listed.
  virtualRoot = false;
  doSort = ok = mpdOk = true;    // 'ok' represents a malformed path, 'mpdOk' is set to 'false' on MPD errors.

  // Special (VFS) URI...
  if (uri[0] == '~') {
    switch (uri[1]) {

      case 'R':
        // Virtual root...
        if (uri[2] == '\0') virtualRoot = true;
        // ... fall through to playlist ...

      case 'P':
        // Playlists or playlist directory...
        doSort = false;
        if (uri[2] == '\0') {
          // List of all playlists...
          lsPlaylists = true;
          mpdOk = mpd_send_list_playlists (mpdConnection);
        }
        else if (uri[2] == '/')
          // List of a particular playlist content...
          mpdOk = mpd_send_list_playlist_meta (mpdConnection, uri + 3);   // skip "~P/"
        else ok = false;
        break;

      case 'Q':
        // Current play queue...
        doSort = false;
        if (uri[2] == '\0') mpdOk = mpd_send_list_queue_meta (mpdConnection);
        else ok = false;
        break;

      case '0':
        return true;      // empty directory

      default:
        ok = false; // invalid path
    }
  }

  // Database directory...
  else {
    mpdOk = mpd_send_list_meta (mpdConnection, uri);
    doSort = true;
  }

  if (!ok) ERRORF (("Invalid MPD path '%s' - this should not happen", uri));

  // Read the entries (to a reversed linked chain first)...
  first = NULL;
  dirEntries = 0;
  if (mpdOk) {
    while ((mpdEntity = mpd_recv_entity (mpdConnection)) != NULL) {
      mpdType = mpd_entity_get_type (mpdEntity);
      if ((lsPlaylists && (mpdType == MPD_ENTITY_TYPE_PLAYLIST)) ||
          (!lsPlaylists && (mpdType == MPD_ENTITY_TYPE_DIRECTORY || mpdType == MPD_ENTITY_TYPE_SONG))) {

        // Create and link new entry...
        entry = new CDirEntry ();
        entry->next = first;
        first = entry;
        dirEntries++;

        // Fill up the entry...
        switch (mpd_entity_get_type (mpdEntity)) {
          case MPD_ENTITY_TYPE_DIRECTORY:
            mpdDir = mpd_entity_get_directory (mpdEntity);
            entry->type = detDirectory;
            entry->uri.Set (mpd_directory_get_path (mpdDir));
            entry->title.SetC (entry->uri.PathLeaf ());    // Set title to local name only
            //~ printf("%s\n", charset_from_utf8(mpd_directory_get_path(dir)));
            break;

          case MPD_ENTITY_TYPE_SONG:
            mpdSong = mpd_entity_get_song (mpdEntity);
            entry->type = detSong;
            entry->uri.Set (mpd_song_get_uri (mpdSong));
            entry->title.Set (mpd_song_get_tag (mpdSong, MPD_TAG_NAME, 0));
            if (entry->title.IsEmpty ()) entry->title.Set (mpd_song_get_tag (mpdSong, MPD_TAG_TITLE, 0));
            if (entry->title.IsEmpty ()) entry->title.SetC (entry->uri);
            entry->duration = (int) mpd_song_get_duration (mpdSong);
            //~ INFOF (("###   song.Uri () = '%s'", entry->uri.Get ()));
            break;

          case MPD_ENTITY_TYPE_PLAYLIST:
            mpdPlaylist = mpd_entity_get_playlist (mpdEntity);
            entry->type = detPlaylist;
            entry->uri.SetF ("%s/%s", uri, mpd_playlist_get_path (mpdPlaylist));
            entry->title.SetC (entry->uri.PathLeaf ());
            //~ printf("%s\n", charset_from_utf8(mpd_playlist_get_path(playlist)));
            break;

          default:
            ASSERT (false);
        }
      }
      mpd_entity_free (mpdEntity);
    }
    mpdOk = mpd_response_finish (mpdConnection);
  }

  // Virtual root (OBSOLETE)...
  if (virtualRoot) {
    // We already have the playlists. Now add an entry for the local collection...
    entry = new CDirEntry ();
    entry->next = first;
    first = entry;
    dirEntries++;

    entry->type = detDirectory;
    entry->uri.Set ("");
    entry->title.SetC (_("Music Collection"));    // TBD: remove or translate
  }

  // Create array...
  if (dirEntries > 0) {
    dirList = new CDirEntry * [dirEntries];
    for (n = dirEntries - 1, entry = first; n >= 0; n--, entry = entry->next) dirList[n] = entry;
    ASSERT (entry == NULL);
  }
  else dirList = NULL;

  // Sort if applicable...
  if (doSort && dirEntries > 0) qsort (dirList, dirEntries, sizeof (dirList[0]), PCDirEntryCompare);
  //~ for (n = 0; n < dirEntries; n++) INFOF (("###   dirList[%i] = '%s'", n, dirList[n]->Uri ()));

  // Notify view, write back new dir ...
  dirPlayingIdx = DirFind (songUri.Get ());
  //~ INFOF (("### dirPlayingIdx = %i", dirPlayingIdx));
  if (view) view->OnDirChanged (0, dirEntries);

  // Check if this is the directory currently loaded in the queue...
  QueueTryLinkDir ();

  // Complete...
  if (!mpdOk) CheckAndHandleMpdError ();
  return mpdOk;
}


int CMusicPlayer::DirFind (const char *uri) {
  int n;

  if (dirPath.Get () [0] != '~') {
    // Not a special path: Do a quick pre-check if the current database directory is loaded...
    if (strncmp (uri, dirPath.Get (), dirPath.Len ()) != 0) return -1;
  }
  for (n = 0; n < dirEntries; n++)
    if (strcmp (uri, dirList [n]->Uri ()) == 0) return n;
  return -1;
}


bool CMusicPlayer::DirLoadParent (const char *uri) {
  CString s;
  bool success;

  // Sanity...
  if (!uri) uri = "-";   // 'PathGoUp' will make "" out of it

  // External URL? ...
  if (MpdUriIsStream (uri)) {
    success = false;
    if (envMpdStreamDirHint) success = DirLoad (envMpdStreamDirHint);
    if (!success) success = DirLoadQueue ();
  }

  // Music collection ...
  else {
    s.Set (uri);
    s.PathGoUp ();
    success = DirLoad (s.Get ());
  }

  // Complete...
  return success;
}



// ********** Queue **********


bool CMusicPlayer::QueueClear () {
  bool mpdOk = mpd_run_clear (mpdConnection);

  // Reload dir if the play queue was loaded in the browser...
  if (DirIsQueue ()) DirReload ();

  // Done...
  changedQueue = true;
  Update ();
  return mpdOk;
}


bool CMusicPlayer::QueueLoadDir (bool force) {
  int n;
  bool mpdOk;

  // Check, if the directory is already loaded...
  if (!force && QueueIsDir ()) return true;

  // Clear and add all entries in a single command list...
  mpdOk = mpd_command_list_begin (mpdConnection, false);
  if (mpdOk) mpdOk = mpd_send_clear (mpdConnection);

  queueSongs = 0;
    // We must count and set 'queueSongs' correctly here. Otherwise, 'queuePath'
    // will be reset at the next update.
  for (n = 0; n < dirEntries && mpdOk; n++) if (dirList[n]->Type () == detSong) {
    mpdOk &= mpd_send_add (mpdConnection, dirList[n]->Uri ());
    if (mpdOk) queueSongs++;
  }

  if (mpdOk) mpdOk = mpd_command_list_end (mpdConnection);
  if (mpdOk) mpdOk = mpd_response_finish (mpdConnection);

  // Show error if necessary...
  if (!mpdOk) CheckAndHandleMpdError ();

  // Reload dir if the play queue was loaded in the browser...
  if (DirIsQueue ()) DirReload ();

  // Done...
  queuePath.Set (dirPath);

  //~ INFOF (("### QueueLoadDir (): queuePath = '%s', dirPath = '%s'", queuePath.Get (), dirPath.Get ()));

  changedQueue = true;
  Update ();
  return mpdOk;
}


bool CMusicPlayer::QueueAppendMultiple (const char *uriLines) {
  CSplitString uriList;
  int n;
  bool mpdOk;

  uriList.Set (uriLines, INT_MAX, "\n");

  // Clear and add all entries in a single command list...
  mpdOk = mpd_command_list_begin (mpdConnection, false);
  for (n = 0; n < uriList.Entries () && mpdOk; n++)
    mpdOk &= mpd_send_add (mpdConnection, uriList[n]);
  if (mpdOk) mpdOk = mpd_command_list_end (mpdConnection);
  if (mpdOk) mpdOk = mpd_response_finish (mpdConnection);

  // Show error if necessary...
  if (!mpdOk) CheckAndHandleMpdError ();

  // Done...
  QueueUnlink ();
  changedQueue = true;
  Update ();
  return mpdOk;
}


bool CMusicPlayer::QueueAppend (const char *uri) {
  bool mpdOk = mpd_run_add (mpdConnection, uri);
  if (!mpdOk) CheckAndHandleMpdError ();

  // Reload dir if the play queue was loaded in the browser...
  if (DirIsQueue ()) DirReload ();

  // Done...
  QueueUnlink ();
  changedQueue = true;
  Update ();
  return mpdOk;
}


bool CMusicPlayer::QueueInsert (int idx, const char *uri) {
  struct mpd_status *mpdStatus;
  unsigned fromPos = 0;
  bool mpdOk;

  // Query current queue length...
  mpdStatus = mpd_run_status (mpdConnection);
  if (mpdStatus) {
    fromPos = mpd_status_get_queue_length (mpdStatus);
    mpd_status_free (mpdStatus);
    mpdOk = true;
  }
  else mpdOk = false;

  // Append new entry and move to selected position...
  if (mpdOk) mpdOk = mpd_run_add (mpdConnection, uri);
  if (mpdOk) mpdOk = mpd_run_move (mpdConnection, fromPos, (unsigned) idx);

  // Show error if necessary...
  if (!mpdOk) CheckAndHandleMpdError ();

  // Reload dir if the play queue was loaded in the browser...
  if (DirIsQueue ()) DirReload ();

  // Done...
  QueueUnlink ();
  changedQueue = true;
  Update ();
  return mpdOk;
}


bool CMusicPlayer::QueueDelete (int idx, int num) {
  bool mpdOk = mpd_run_delete_range (mpdConnection, (unsigned) idx, (unsigned) (idx + num));
  if (!mpdOk) CheckAndHandleMpdError ();

  // Reload dir if the play queue was loaded in the browser...
  if (DirIsQueue ()) DirReload ();

  // Done...
  QueueUnlink ();
  changedQueue = true;
  Update ();
  return mpdOk;
}


bool CMusicPlayer::QueueTryLinkDir () {
  struct mpd_entity *mpdEntity;
  const struct mpd_song *mpdSong;
  enum mpd_entity_type mpdType;
  const char *queueUri;
  int n;
  bool ok, mpdOk;

  // Sanity...
  if (QueueIsLinked ()) return true;
  if (DirIsQueue ()) return false;

  // Compare the complete directory with the queue...
  mpdOk = mpd_send_list_queue_meta (mpdConnection);
  ok = mpdOk ? true : false;
  n = 0;
  while (ok && (mpdEntity = mpd_recv_entity (mpdConnection)) != NULL) {
    //~ INFOF (("###    compare #%i", n));
    mpdType = mpd_entity_get_type (mpdEntity);
    if (mpdType == MPD_ENTITY_TYPE_SONG) {
      mpdSong = mpd_entity_get_song (mpdEntity);
      queueUri = mpdSong ? mpd_song_get_uri (mpdSong) : NULL;
      if (!queueUri) ok = false;
      if (n >= dirEntries || dirList[n]->Type () != detSong) ok = false;
      if (ok) {
        //~ INFOF (("###    compare '%s' == '%s'", queueUri, dirList[n]->Uri ()));
        if (strcmp (queueUri, dirList[n]->Uri ()) != 0) ok = false;
        n++;
      }
    }
    mpd_entity_free (mpdEntity);
  }
  mpdOk = mpd_response_finish (mpdConnection);
  if (!mpdOk) ok = false;
  if (ok) if (n < dirEntries) if (dirList[n]->Type () == detSong) ok = false;

  // Adopt 'queuePath' if equal...
  if (ok) queuePath.Set (dirPath);
  return ok;
}



// ********** Song info **********


bool CMusicPlayer::SetSongAndPos (int idx, int pos) {
  bool mpdOk;

  // Sanity...
  if (!ServerConnected () || idx < 0 || idx >= queueSongs || pos < 0) return false;     // illegal parameters => no action and fail

  // Operation...
  mpdOk = mpd_run_seek_pos (mpdConnection, (unsigned) idx, (unsigned) pos);

  // Done...
  if (idx != songQueueIdx) Update ();
  else {
    // Lightweight update without accessing the server...
    playerSongPos = pos;
    if (view) view->OnSongPosChanged (playerState, playerSongPos, playerBitrate, playerFreq, playerChannels);
  }
  return mpdOk;
}


bool CMusicPlayer::PlaySong (int idx) {
  bool mpdOk;

  // Sanity & operation...
  if (ServerConnected ()) mpdOk = mpd_run_play_pos (mpdConnection, (unsigned) idx);
  else mpdOk = false;

  // Done...
  Update ();
  return mpdOk;
}



// ********** Player state **********


bool CMusicPlayer::IsPlayingForSure (int minDb) {
  int curDb;

  if (playerIsMuted) return true;
  if (errorRecovery || errorPermanent || playerState != rcvPlayerPlaying) return false;
  if (minDb == -INT_MAX) {
    //~ INFO("### CMusicPlayer::IsPlayingForSure (): DB check disabled - reporting success");
    return true;
  }
  if (!OutputCanStream (outputIdx)) {    // cannot make DB check for non-streaming output
    //~ INFO("### CMusicPlayer::IsPlayingForSure (): Output cannot deliver DB info - reporting success");
    return true;
  }

  curDb = StreamerGetDbLevel ();
  //~ INFOF(("### CMusicPlayer::IsPlayingForSure (): cur = %i db, min = %i db", curDb, minDb));
  return curDb >= minDb;
}


bool CMusicPlayer::Play () {
  bool mpdOk;

  // Sanity & operation...
  if (!ServerConnected ()) return mpdOk = false;

  // Check mute mode...
  else if (SystemIsMuted ()) {
    playerIsMuted = true;
    mpdOk = true;
  }

  // Play...
  else mpdOk = mpd_run_play (mpdConnection);

  // Done...
  Update ();
  return mpdOk;
}


bool CMusicPlayer::Pause () {
  bool mpdOk;

  // Sanity & operation...
  if (ServerConnected ()) mpdOk = mpd_run_pause (mpdConnection, true);
  else mpdOk = false;

  // Done...
  Update ();
  return mpdOk;
}


bool CMusicPlayer::Stop () {
  bool mpdOk;

  // Sanity & operation...
  if (ServerConnected ()) mpdOk = mpd_run_stop (mpdConnection);
  else mpdOk = false;

  // Release mute mode...
  playerIsMuted = false;

  // Done...
  Update ();
  return mpdOk;
}



// ********** Misc. commands **********


bool CMusicPlayer::SongNext () {
  if (queueSongs < 1 || songQueueIdx < 0 || songQueueIdx >= queueSongs) return false;
  return SetSongAndPos ((songQueueIdx + 1) % queueSongs, 0);
}


bool CMusicPlayer::SongPrev () {
  if (queueSongs < 1 || songQueueIdx < 0 || songQueueIdx >= queueSongs) return false;
  return SetSongAndPos ((songQueueIdx + queueSongs - 1) % queueSongs, 0);
}


#define SKIP_SECONDS 10


bool CMusicPlayer::SkipForward () {
  if (songDuration <= 0) return false;
  if (playerSongPos + SKIP_SECONDS > songDuration) return SongNext ();
  else return SetSongPos (playerSongPos + SKIP_SECONDS);
}


bool CMusicPlayer::SkipBack () {
  bool ok;

  if (songDuration <= 0) return false;
  if (playerSongPos > SKIP_SECONDS) return SetSongPos (playerSongPos - SKIP_SECONDS);
  else if (playerSongPos >= 1 || songQueueIdx <= 0) return SetSongPos (0);
  else {
    ok = SetSongAndPos (songQueueIdx - 1, 0);
    if (ok) ok = SetSongPos (songDuration - SKIP_SECONDS);
    return ok;
  }
}





// *************************** CListboxDirectory *******************************


void CListboxDirectory::Setup (SDL_Rect _area) {
  TTF_Font *_font = FontGet (fntNormal, 24);
  SetMode (lmActivate, FontGetLineSkip (_font) + 8, 0);
  SetFormat (_font, -1, TRANSPARENT, WHITE, TRANSPARENT);
  SetArea (_area);
  SetTextureBlendMode (SDL_BLENDMODE_BLEND);
}


void CListboxDirectory::SetPlayingSong (int _playingSong) {
  int oldPlayingSong;

  if (_playingSong != playingSong) {
    oldPlayingSong = playingSong;
    playingSong = _playingSong;
    if (oldPlayingSong >= 0) ChangedItems (oldPlayingSong);
    if (playingSong >= 0) ChangedItems (playingSong);
  }
}


SDL_Surface *CListboxDirectory::RenderItem (CListboxItem *item, int idx, SDL_Surface *surf) {
  CDirEntry *entry = (CDirEntry *) item->data;
  SDL_Surface *surfText;
  SDL_Rect r;
  TColor col0, col1;
  char buf[256];

  ASSERT (itemHeight > 0 && entry);

  // Determine front and back color...
  if (idx != playingSong) {
    col0 = colBack;
    col1 = item->IsSelected () ? COL_DISPLAY : WHITE;
  }
  else {
    col0 = item->IsSelected () ? COL_DISPLAY : WHITE;
    col1 = colBack;
  }

  // Prepare/clear surface...
  if (!surf) surf = CreateSurface (area.w, itemHeight);
  SDL_FillRect (surf, NULL, ToUint32 (col0));

  // Draw label...
  r = Rect (surf);
  if (entry->Type () == detSong) {
    // Prepend index number...
    snprintf (buf, sizeof (buf), "%3i. %s", idx + 1, entry->Title ());
    surfText = FontRenderText (font, buf, col1);
  }
  else {
    // Prepend icon...
    SurfaceBlit (
        IconGet (entry->Type () == detPlaylist ? "ic-queue_music-24" : "ic-folder-24", col1),
        NULL, surf, &r, -1, 0, SDL_BLENDMODE_BLEND
      );
    r.x = 32;     // insert some space to the left
    r.w -= r.x;
    surfText = FontRenderText (font, entry->Title (), col1);
  }
  SurfaceBlit (surfText, NULL, surf, &r, -1, 0, SDL_BLENDMODE_BLEND);
  SurfaceFree (surfText);

  // Done...
  return surf;
}





// *************************** CScreenMusicMain *******************************


BUTTON_TRAMPOLINE(CbOnButtonPushed, CScreenMusicMain, OnButtonPushed)
LISTBOX_TRAMPOLINE(CbOnListItemPushed, CScreenMusicMain, OnListItemPushed)
SLIDER_TRAMPOLINE(CbOnPosSliderValueChanged, CScreenMusicMain, OnPosSliderValueChanged)
SLIDER_TRAMPOLINE(CbOnVolSliderValueChanged, CScreenMusicMain, OnVolSliderValueChanged)


enum EBtnIdMusicMain {
  btnIdMmBack = 0,
  btnIdMmSelServer,
  btnIdMmGoServer,
  btnIdMmSelOutput,
  btnIdMmBluetooth,
  btnIdMmRepeatMode,
  btnIdMmGoCurrent,
  btnIdMmEND
};


static TButtonDescriptor mmButtons[btnIdMmEND] = {
  { -1, COL_MAIN_BUTTONS, "ic-back-48", NULL, CbOnButtonPushed, SDLK_ESCAPE }, // btnIdMmBack
  { -3, COL_MAIN_BUTTONS, "ic-tape-48", NULL,        CbOnButtonPushed, SDLK_s },        // btnIdMmSelServer
  { -1, COL_MAIN_BUTTONS, "ic-walk-48", NULL,        CbOnButtonPushed, SDLK_g },        // btnIdMmGoServer
  { -1, COL_MAIN_BUTTONS, "ic-hearing-48", NULL,     CbOnButtonPushed, SDLK_o },        // btnIdMmSelOutput
  { -1, COL_MAIN_BUTTONS, "ic-bluetooth-48", NULL,   CbOnButtonPushed, SDLK_b },        // btnIdMmBluetooth
  { -1, COL_MAIN_BUTTONS, "ic-repeat_off-48", NULL,  CbOnButtonPushed, SDLK_r },        // btnIdMmRepeatMode  ; or 'ic-repeat', 'ic-repeat_one'
  { -1, COL_MAIN_BUTTONS, "ic-location-48", NULL,    CbOnButtonPushed, SDLK_HOME }      // btnIdMmGoCurrent
};


static void *appLaunchButton = NULL;





// ***************** General / Setup ***********************


static void SetAppLaunchLabel (bool live) {
  if (appLaunchButton) APP_SET_LABEL (appLaunchButton, "ic-audio", _("Music"), live ? COL_APP_LABEL_LIVE : COL_APP_LABEL);
}


CScreenMusicMain::CScreenMusicMain () {
  isStarting = isPlayingActive = false;
  buttonBar = NULL;
  dispFontSmall = dispFontLarge = NULL;
  dispHaveSong = dispHaveServer = false;
  surfDirTitleLabel = NULL;
}


CScreenMusicMain::~CScreenMusicMain () {
  player.SetView (NULL);
  DelAllWidgets ();   // unlink all buttons of 'buttonBar'
  FREEA (buttonBar);
  SurfaceFree (wdgDisplay.GetSurface ());
  SurfaceFree (wdgBackground.GetSurface ());
  SurfaceFree (&surfDirTitleLabel);
}


void CScreenMusicMain::Setup () {
  SDL_Surface *surf;
  SDL_Rect r, *layout, *layoutMain, *layoutPane;
  int n;

  // Main buttons...
  SETA (buttonBar, CreateMainButtonBar (btnIdMmEND, mmButtons, this));

  // Background...
  surf = CreateSurface (1, 64);
  for (r = Rect (0, 0, 1, 1); r.y < 64; r.y++)
    SDL_FillRect (surf, &r, ToUint32 (ColorBrighter (COL_BACKGROUND, /* 32 - r.y */ r.y - 64)));
  wdgBackground.SetSurface (surf);
  wdgBackground.SetArea (UI_USER_RECT);
  AddWidget (&wdgBackground);

  r = UI_USER_RECT;
  RectGrow (&r, -UI_SPACE, -UI_SPACE);
  layoutMain = LayoutRowEqually (r, 2, UI_SPACE);

  // Left pane (player) ...
  layoutPane = LayoutCol (layoutMain[0], UI_SPACE,
                          -1, UI_BUTTONS_HEIGHT, UI_BUTTONS_HEIGHT, UI_BUTTONS_HEIGHT * 3/2, 0);

  //     display...
  r = layoutPane[0];
  wdgDisplay.SetArea (r);
  DisplaySetup ();
  AddWidget (&wdgDisplay);

  //     position slider...
  layout = LayoutRow (layoutPane[1], UI_CONTROLS_SPACE,
                      UI_BUTTONS_HEIGHT, -1, UI_BUTTONS_HEIGHT, 0);

  btnPosBack.Set (layout[0], COL_PLAY_BUTTONS, IconGet ("ic-fast_rewind-48"));
  btnPosBack.SetCbPushed (CbOnButtonPushed, this);
  btnPosBack.SetHotkey (SDLK_COMMA);

  sliderPos.SetFormat (COL_PLAY_BUTTONS, DARK_DARK_GREY, BLACK, TRANSPARENT, UI_SLIDER_WIDTH);
  sliderPos.SetArea (layout[1]);
  sliderPos.SetTextureBlendMode (SDL_BLENDMODE_BLEND);
  sliderPos.SetInterval (0, 0, false);
  sliderPos.SetCbValueChanged (CbOnPosSliderValueChanged, this);

  btnPosForward.Set (layout[2], COL_PLAY_BUTTONS, IconGet ("ic-fast_forward-48"));
  btnPosForward.SetCbPushed (CbOnButtonPushed, this);
  btnPosForward.SetHotkey (SDLK_PERIOD);

  free (layout);

  //     volume slider...
  layout = LayoutRow (layoutPane[2], UI_CONTROLS_SPACE,
                      UI_BUTTONS_HEIGHT, -1, UI_BUTTONS_HEIGHT, 0);

  btnVolDown.Set (layout[0], COL_PLAY_BUTTONS, IconGet ("ic-volume_mute-48"));
  btnVolDown.SetCbPushed (CbOnButtonPushed, this);
  btnVolDown.SetHotkey (SDLK_LEFT);

  sliderVol.SetFormat (COL_PLAY_BUTTONS, DARK_DARK_GREY, BLACK, TRANSPARENT, UI_SLIDER_WIDTH);
  sliderVol.SetArea (layout[1]);
  sliderVol.SetTextureBlendMode (SDL_BLENDMODE_BLEND);
  sliderVol.SetInterval (0, 100);
  sliderVol.SetCbValueChanged (CbOnVolSliderValueChanged, this);

  btnVolUp.Set (layout[2], COL_PLAY_BUTTONS, IconGet ("ic-volume_up-48"));
  btnVolUp.SetCbPushed (CbOnButtonPushed, this);
  btnVolUp.SetHotkey (SDLK_RIGHT);

  free (layout);

  //     play buttons...
  layout = LayoutRowEqually (layoutPane[3], 4, UI_CONTROLS_SPACE);
  n = 0;

  btnSongPrev.Set (layout[n++], COL_PLAY_BUTTONS, IconGet ("ic-skip_previous-96"));
  btnSongPrev.SetCbPushed (CbOnButtonPushed, this);
  btnSongPrev.SetHotkey (SDLK_UP);
  AddWidget (&btnSongPrev);

  btnStop.Set (layout[n++], COL_PLAY_BUTTONS, IconGet ("ic-stop-96"));
  btnStop.SetCbPushed (CbOnButtonPushed, this);
  btnStop.SetHotkey (SDLK_CARET);
  AddWidget (&btnStop);

  btnPlayPause.Set (layout[n++], COL_PLAY_BUTTONS, IconGet ("ic-play-96"));
  btnPlayPause.SetCbPushed (CbOnButtonPushed, this);
  btnPlayPause.SetHotkey (SDLK_SPACE);
  AddWidget (&btnPlayPause);

  btnSongNext.Set (layout[n++], COL_PLAY_BUTTONS, IconGet ("ic-skip_next-96"));
  btnSongNext.SetCbPushed (CbOnButtonPushed, this);
  btnSongNext.SetHotkey (SDLK_DOWN);
  AddWidget (&btnSongNext);

  free (layout);

  //     done.
  free (layoutPane);

  // Right pane (directory)...
  r = layoutMain[1];
  //~ r.y += (r.h - UI_DIRNAME_H);
  r.h = UI_DIRNAME_H;
  btnDirTitle.Set (r, COL_LIST_TITLE);
  btnDirTitle.SetCbPushed (CbOnButtonPushed, this);
  SurfaceSet (&surfDirTitleLabel, CreateSurface (r));   // only reserve memory; no need to initialize the surface here
  AddWidget (&btnDirTitle);

  r.y = layoutMain[1].y + UI_DIRNAME_H + UI_SPACE;
  //~ r.y = layoutMain[1].y;
  r.h = layoutMain[1].h - UI_DIRNAME_H - UI_SPACE;
  listDir.Setup (r);
  listDir.SetCbPushed (CbOnListItemPushed, this);
  AddWidget (&listDir);

  // Layout complete...
  free (layoutMain);

  // Setup player...
  player.SetView (this);
}


void CScreenMusicMain::Activate (bool on) {
  CScreen::Activate (on);
  //~ INFOF (("### CScreenMusicMain::Activate (%i) -> IsActive () = %i", (int) on, (int) IsActive ()));
  UpdateActiveState ();
  if (on)
    ConnectServer ();
  else
    EnvFlush ();
}


void CScreenMusicMain::ConnectServer () {
  const char *serverKey = EnvGet (envMpdServerKey);
  //~ INFOF (("###  Activate (1): envMpdServer = '%s', idx = %i", serverKey, player.ServerIdx (serverKey)));
  if (serverKey) player.SetServer (player.ServerIdx (serverKey));
}


void CScreenMusicMain::UpdateActiveState () {
  // to be is called on screen (de-)activation or player state changes
  bool isActive = IsActive ();
  bool _isPlayingActive;

  // Screen locking...
  if (isActive && !player.IsStopped ())
    SystemActiveLock ("_music");
  else
    SystemActiveUnlock ("_music");

  // Start/stop regular timer...
  if (player.InError ()) isStarting = false;
  _isPlayingActive = (isActive || player.IsPlayingOrShouldBe () || isStarting);
  //~ INFOF (("CScreenMusicMain::UpdateActiveState (): isActive = %i, playerState = %i, isStarting = %i", (int) isActive, (int) player.PlayerState (), (int) isStarting));
  //~ INFOF (("CScreenMusicMain::UpdateActiveState (): isPlayingActive = %i -> %i", (int) isPlayingActive, (int) _isPlayingActive));
  if (_isPlayingActive != isPlayingActive) {
    if (_isPlayingActive) {
      CTimer::Set (0, 256);
      SetAppLaunchLabel (true);
    }
    else {
      player.SetServer (-1);
      CTimer::Clear ();
      SetAppLaunchLabel (false);
    }
    isPlayingActive = _isPlayingActive;
  }
}


void CScreenMusicMain::UpdateBluetooth () {
  static TColor lastBtCol = TRANSPARENT;
  TColor col;
  bool btOn, btBusy, btAudio;

  btOn = SystemBluetoothGetState (&btBusy, &btAudio);
  col = WHITE;
  if (btOn) {
    if (btBusy) col = LIGHT_RED;
    else col = btAudio ? YELLOW : LIGHT_BLUE;
  }
  if (ToUint32 (col) != ToUint32 (lastBtCol)) {
    buttonBar[btnIdMmBluetooth].SetLabel (col, mmButtons[btnIdMmBluetooth].iconName);
    lastBtCol = col;
  }
}


void CScreenMusicMain::OnTime () {
  //~ INFO ("### CScreenMusicMain::OnTime");

  // Update player...
  player.Update ();

  // Update Bluetooth button...
  UpdateBluetooth ();

  // Reset 'isStarting' flag...
  if (isStarting) {
    if (player.IsPlayingOrShouldBe () || player.InError () || player.Server () < 0) isStarting = false;
    if (!isStarting) UpdateActiveState ();
  }
}



// ***************** Menus ***********************


void CScreenMusicMain::RunServerMenu (int xPos, bool transfer) {
  CMenu menu;
  CString playerState;
  int n, idx;
  bool isCurrent, ok = false;

  // Setup and execute menu...
  menu.Setup (Rect (xPos, 0, UI_RES_X - xPos, UI_RES_Y - UI_BUTTONS_HEIGHT), -1, 1, COL_MAIN_BUTTONS, FontGet (fntNormal, 24));
  menu.SetItems (player.Servers ());
  for (n = 0; n < player.Servers (); n++) {
    isCurrent = (n == player.Server () && !player.InError ());
    menu.SetItem (n, player.ServerName (n), IconGet ("ic-tape-24"), isCurrent);
  }
  idx = menu.Run (CScreen::ActiveScreen ());
  if (idx < 0) return;   // menu was cancelled

  // Change the server...
  if (transfer) {
    ok = player.GetState (&playerState);
    if (player.PlayerState () == rcvPlayerPlaying) player.Pause ();
  }

  player.SetServer (idx);

  if (transfer && ok)
    player.SetState (playerState.Get ());
}


void CScreenMusicMain::RunOutputMenu (int xPos) {
  CMenu menu;
  int n, idx;
  bool isCurrent;

  // Setup and execute menu...
  menu.Setup (Rect (xPos, 0, UI_RES_X - xPos, UI_RES_Y - UI_BUTTONS_HEIGHT), -1, 1, COL_MAIN_BUTTONS, FontGet (fntNormal, 24));
  menu.SetItems (player.Outputs ());
  for (n = 0; n < player.Outputs (); n++) {
    isCurrent = (n == player.Output () && !player.InError ());
    menu.SetItem (n,
        player.OutputName (n),
        IconGet (player.OutputCanStream (n) ? "ic-headset-24" : "ic-speaker-24"),
        isCurrent);
  }
  idx = menu.Run (CScreen::ActiveScreen ());
  if (idx < 0) return;   // menu was cancelled

  // Change output...
  player.SetOutput (idx);
}



// ***************** UI Callbacks ****************


void CScreenMusicMain::OnButtonPushed (CButton *btn, bool longPush) {
  int idx;

  switch ((EBtnIdMusicMain) (btn - buttonBar)) {

    // Main button bar...
    case btnIdMmBack:
      if (longPush) PlayerOff ();
      AppEscape ();
      break;
    case btnIdMmSelServer:
      RunServerMenu (btn->GetArea ()->x, false);
      break;
    case btnIdMmGoServer:
      RunServerMenu (btn->GetArea ()->x, true);
      break;
    case btnIdMmSelOutput:
      RunOutputMenu (btn->GetArea ()->x);
      break;
    case btnIdMmBluetooth:
      SystemBluetoothToggle ();
      break;
    case btnIdMmRepeatMode:
      player.SetRepeatMode (!player.RepeatMode ());
      break;
    case btnIdMmGoCurrent:
      if (player.QueueIsLinked ()) player.DirLoadQueue ();   // It's easy: We know the loaded directory.
      else {
        player.DirLoadParent (player.SongUri ());     // Load parent of current song
        if (!player.QueueIsLinked () && !longPush) player.DirLoadQueue ();
          // On a normal (short) push, we want to go to the exact queue, not a target.
          // => If the parent directory of the current song was not linkable, load the (unlinked) queue.
      }
      idx = player.DirFind (player.SongUri ());
      if (idx >= 0) {
        listDir.SetPlayingSong (idx);
        listDir.ScrollTo (idx, 0);
      }
      break;
    default:

      // Special buttons in left pane...
      if (btn == &btnPosBack)           player.SkipBack ();
      else if (btn == &btnPosForward)   player.SkipForward ();
      else if (btn == &btnVolDown) {
        if (longPush) player.SetVolume (0);
        else player.SetVolume (player.Volume () - 5);
      }
      else if (btn == &btnVolUp)        player.SetVolume (player.Volume () + 5);
      else if (btn == &btnSongPrev)     player.SongPrev ();
      else if (btn == &btnSongNext)     player.SongNext ();
      else if (btn == &btnStop)         player.Stop ();
      else if (btn == &btnPlayPause) {
        if (player.IsPlaying ()) player.Pause ();
        else player.Play ();
      }

      // Dir title button...
      else if (btn == &btnDirTitle) {
        CString dirPath (player.DirPath ());
        if (dirPath[0] == '\0' || longPush) player.DirLoad ("~P");
        else {
          player.DirLoadParent (dirPath);
          idx = player.DirFind (dirPath.Get ());
          if (idx >= 0) listDir.ScrollTo (idx, 0);
        }
      }
  }
}


void CScreenMusicMain::OnListItemPushed (CListbox *, int idx, bool longPush) {
  CString path;
  CDirEntry *entry;
  int playingIdx;

  entry = player.DirEntry (idx);
  switch (entry->Type ()) {
    case detDirectory:
    case detPlaylist:
      player.DirLoad (entry->Uri ());
      playingIdx = player.DirPlayingIdx ();
      if (longPush) {
        if (player.QueueLoadDir ()) player.PlaySong (0);    // long push on directory: start playing it from the beginning
      }
      else
        listDir.ScrollTo (playingIdx < 0 ? 0 : playingIdx); // scroll to top (or playing song, if we stumble accross it)
      break;
    case detSong:
      if (player.QueueIsDir ()) player.PlaySong (idx);
      else if (longPush) {
        // long push on song/stream: (re-)load directory + start playing the song
        if (player.QueueLoadDir (true)) player.PlaySong (idx);
      }
      break;
    default:
      ASSERT_WARN (true);
      break;
  }
}


void CScreenMusicMain::OnPosSliderValueChanged (CSlider *slider, int val, int lastVal) {
  //~ INFOF (("### CScreenMusicMain::OnPosSliderValueChanged (%i -> %i)", lastVal, val));
  player.SetSongPos (val);
}


void CScreenMusicMain::OnVolSliderValueChanged (CSlider *slider, int val, int lastVal) {
  player.SetVolume (val);
}



// ***************** Player Callbacks **********************


void CScreenMusicMain::OnServerChanged (int idx, bool errorRecovery, bool errorPermanent) {
  //~ INFOF (("### CScreenMusicMain::OnServerChanged (%i)", idx));
  TColor color;

  if (idx >= player.Servers ()) idx = -1;   // sanity

  // Set label...
  color = (errorPermanent ? GREY : WHITE);
  buttonBar[btnIdMmSelServer].SetLabel (color, "ic-tape-48", idx >= 0 ? player.ServerName (idx) : NULL);

  // Propagate further updates...
  dispHaveServer = player.ServerConnected ();
  if (!dispHaveServer) {
    dispHaveSong = false;
    OnSongChanged (rcvPlayerStopped, 0, 0, 0);
  }
  OnPlayerStateChanged (player.PlayerState ());
}


void CScreenMusicMain::OnOutputChanged (int idx) {
  OnPlayerStateChanged (player.PlayerState ());
}


void CScreenMusicMain::OnStreamerStateChanged (EStreamerState state) {
  TColor col;

  switch (state) {
    case strOn:     col = YELLOW;     break;
    case strBusy:   col = LIGHT_RED;  break;
    case strError:  col = GREY;       break;
    default:        col = WHITE;
  }
  buttonBar[btnIdMmSelOutput].SetLabel (col, mmButtons[btnIdMmSelOutput].iconName);
}


void CScreenMusicMain::OnRepeatModeChanged (bool repeatOn) {
  buttonBar[btnIdMmRepeatMode].SetLabel (WHITE, repeatOn ? "ic-repeat-48" : "ic-repeat_off-48");
}


void CScreenMusicMain::OnVolumeChanged (int volume) {
  //~ INFOF (("### CScreenMusicMain::OnVolumeChanged (%i)", volume));
  if (volume < 0) {
    DelWidget (&sliderVol);
    DelWidget (&btnVolDown);
    DelWidget (&btnVolUp);
  }
  else {
    AddWidget (&sliderVol);
    AddWidget (&btnVolDown);
    AddWidget (&btnVolUp);
    sliderVol.SetValue (volume, false);
  }
}


void CScreenMusicMain::OnDirChanged (int idx0, int idx1) {
  int entries = player.DirEntries ();
  SDL_Rect r;
  int n;

  //~ INFOF (("### OnDirChanged (%i, %i)", idx0, idx1));

  // Redraw title if indicated...
  if (idx0 <= 0 && idx1 >= entries) {
    r = Rect (surfDirTitleLabel);
    SurfaceFillRect (surfDirTitleLabel, r, TRANSPARENT);
    RectGrow (&r, -2, -2);
    TextRender (
        player.DirPathReadable (),
        CTextFormat (FontGet (fntBold, 24), WHITE, TRANSPARENT, 0, 0, 0, 0, r.w, r.h),
        surfDirTitleLabel, &r
      );
    btnDirTitle.SetLabel (surfDirTitleLabel);
  }

  // Update list...
  listDir.SetItems (entries);
  for (n = idx0; n < idx1; n++)
    listDir.SetItem (n, NULL, (const char *) NULL, false, player.DirEntry (n));

  // Scroll to playing song if applicable...
  listDir.SetPlayingSong (player.DirPlayingIdx ());
  if (listDir.PlayingSong () >= 0) listDir.ScrollTo (listDir.PlayingSong (), 0);
}


void CScreenMusicMain::OnSongChanged (ERctPlayerState state, int songs, int idx, int duration) {
  int dirIdx;

  //~ INFOF (("### CScreenMusicMain::OnSongChanged (%i/%i, dur = %i)", idx, songs, duration));

  // Update 'dispHaveSong'...
  dispHaveSong = (dispHaveServer && songs > 0 && idx >= 0 && duration >= 0);

  // Update display...
  DisplayClearAndDrawSong (state, songs, idx, duration);
  DisplayDrawPlayerState (state);

  // Update marker in directory...
  dirIdx = player.DirPlayingIdx ();   // this is the entry in the directory (which may be different from what is played)
  listDir.SetPlayingSong (dirIdx);
  if (dirIdx >= 0) listDir.ScrollTo (dirIdx, 0);

  // Set position slider interval...
  if (duration > 0) {
    sliderPos.SetInterval (0, duration, false);
    AddWidget (&sliderPos);
    AddWidget (&btnPosBack);
    AddWidget (&btnPosForward);
  }
  else {
    DelWidget (&sliderPos);
    DelWidget (&btnPosBack);
    DelWidget (&btnPosForward);
  }
}


void CScreenMusicMain::OnPlayerStateChanged (ERctPlayerState state) {
  //~ INFOF (("### CScreenMusicMain::OnPlayerStateChanged (%i)", state));

  // Lock screen if playing/pausing...
  UpdateActiveState ();

  // Set play/pause button label...
  btnPlayPause.SetLabel (WHITE, state == rcvPlayerPlaying ? "ic-pause-96" : "ic-play-96");

  // Update display...
  if (dispHaveServer) {     // display is not off?
    DisplayDrawPlayerState (state);
    DisplayDrawInfoLine (state);
  }
}


void CScreenMusicMain::OnSongPosChanged (ERctPlayerState state, int songPos, int bitrate, int freq, int channels) {

  //~ INFOF (("### CScreenMusicMain::OnSongPosChanged (%i)", songPos));

  if (!dispHaveServer) return;    // display is off

  // Update display...
  DisplayDrawSongPos (state, songPos);
  DisplayDrawInfoLine (state, bitrate, freq, channels);

  // Update position slider...
  sliderPos.SetValue (songPos, false);
}





// ***************** Drawing *******************************


void CScreenMusicMain::DisplaySetup () {
  SDL_Surface *surfDisp;

  surfDisp = CreateSurface (wdgDisplay.GetArea ());
  SurfaceFill (surfDisp, BLACK);
  wdgDisplay.SetSurface (surfDisp);

  dispFontSmall = FontGet (fntNormal, 20);
  dispFontLarge = FontGet (fntBold, 32);
  dispFontLargeButSmaller = FontGet (fntBold, 24);

  dispRect = Rect (surfDisp);
  RectGrow (&dispRect, -UI_DISPLAY_SPACE, -UI_DISPLAY_SPACE);

  dispRectPlayerState = Rect (dispRect.x + (dispRect.w - 48) / 2, 0, 48, 48);

  dispRectPlayerTime = Rect (dispRectPlayerState.x + dispRectPlayerState.w, UI_DISPLAY_SPACE, 0, 48 - 2 * UI_DISPLAY_SPACE);
  dispRectPlayerTime.w = dispRect.w - dispRectPlayerTime.x;

  dispRectInfo = Rect (dispRect.x, 0, dispRect.w, FontGetLineSkip (dispFontSmall));
  dispRectInfo.y = dispRect.y + dispRect.h - dispRectInfo.h;
}


void CScreenMusicMain::DisplayClearAndDrawSong (ERctPlayerState state, int songs, int idx, int duration) {
  SDL_Surface *surfDisp, *surf;
  SDL_Rect r, rTitle, rSepLine;
  TTF_Font *font;
  char buf[32];
  bool abbreviated;

  // Init variables / layout...
  rSepLine = Rect (dispRect.x, dispRectPlayerState.y + dispRectPlayerState.h, dispRect.w, UI_DISPLAY_SPACE);

  rTitle = Rect (dispRect.x, rSepLine.y + rSepLine.h, dispRect.w, 0);
  rTitle.h = dispRectInfo.y - UI_DISPLAY_SPACE - rTitle.y;

  // Clear display and draw statics...
  surfDisp = wdgDisplay.GetSurface ();
  if (dispHaveServer) {
    SurfaceFill (surfDisp, COL_DISPLAY);
    SurfaceFillRect (surfDisp, &rSepLine, BLACK);
  }
  else
    SurfaceFill (surfDisp, BLACK);

  // Draw song info...
  if (dispHaveSong) {

    // Song out of n...
    r = Rect (dispRect.x, dispRectPlayerTime.y, dispRectPlayerState.x - dispRect.x, dispRectPlayerTime.h);

    snprintf (buf, sizeof (buf), "%i", idx + 1);
    surf = FontRenderText (dispFontLarge, buf, BLACK, COL_DISPLAY);
    SurfaceBlit (surf, NULL, surfDisp, &r, -1, 1);
    r.x += surf->w;
    r.w -= surf->w;
    SurfaceFree (&surf);

    snprintf (buf, sizeof (buf), " / %i", songs);
    surf = FontRenderText (dispFontSmall, buf, BLACK, COL_DISPLAY);
    SurfaceBlit (surf, NULL, surfDisp, &r, -1, 1);
    SurfaceFree (&surf);

    // Song duration...
    dispRectPlayerTime.w = dispRect.x + dispRect.w - dispRectPlayerTime.x;      // widen 'dispRectPlayerTime' to all available space
    if (duration > 0) {
      snprintf (buf, sizeof (buf), " / %i:%02i", duration / 60, duration % 60);
      surf = FontRenderText (dispFontSmall, buf, BLACK, COL_DISPLAY);
      SurfaceBlit (surf, NULL, surfDisp, &dispRectPlayerTime, 1, 1);
      dispRectPlayerTime.w -= surf->w;      // reduce 'dispRectPlayerTime' by space for total time
      SurfaceFree (&surf);
    }

    // Title...
    abbreviated = true;
    for (font = dispFontLarge; font && abbreviated; font = (font == dispFontLarge ? dispFontLargeButSmaller : NULL)) {
      TextRender (
        player.SongTitle (),
        CTextFormat (font, BLACK, COL_DISPLAY, 0, 0, 0, 0, rTitle.w, rTitle.h),
        surfDisp, &rTitle, &abbreviated
      );
    }
  }

  // Done...
  wdgDisplay.SetSurface (surfDisp);
}


void CScreenMusicMain::DisplayDrawSongPos (ERctPlayerState state, int songPos) {
  SDL_Surface *surfDisp, *surf;
  char buf[32];

  if (!dispHaveServer) return;

  // Clear all...
  surfDisp = wdgDisplay.GetSurface ();
  SurfaceFillRect (surfDisp, dispRectPlayerTime, COL_DISPLAY);

  if (dispHaveSong && songPos >= 0) {

    // Draw play time...
    snprintf (buf, sizeof (buf), "%i:%02i", songPos / 60, songPos % 60);
    surf = FontRenderText (dispFontLarge, buf, BLACK, COL_DISPLAY);
    SurfaceBlit (surf, NULL, surfDisp, &dispRectPlayerTime, 1, 1);
    SurfaceFree (&surf);
  }

  // Done...
  wdgDisplay.SetSurface (surfDisp);
}


void CScreenMusicMain::DisplayDrawPlayerState (ERctPlayerState state) {
  SDL_Surface *surfDisp;

  if (!dispHaveServer) return;

  // Draw play/pause symbol...
  surfDisp = wdgDisplay.GetSurface ();
  switch (state) {
    case rcvPlayerPlaying:
    case rcvPlayerPaused:
      SurfaceBlit (
        IconGet (state == rcvPlayerPlaying ? "ic-play-48" : "ic-pause-48", BLACK, COL_DISPLAY), NULL,
        surfDisp, &dispRectPlayerState
      );
      break;
    default:
      SurfaceFillRect (surfDisp, &dispRectPlayerState, COL_DISPLAY);
      break;
  }

  // Done...
  wdgDisplay.SetSurface (surfDisp);
}


void CScreenMusicMain::DisplayDrawInfoLine (ERctPlayerState state, int bitrate, int freq, int channels) {
  SDL_Surface *surfDisp, *surf;
  SDL_Rect clipR;
  char buf[64];

  if (!dispHaveServer) return;

  // Draw info line...
  surfDisp = wdgDisplay.GetSurface ();
  SurfaceFillRect (surfDisp, dispRectInfo, COL_DISPLAY);
  surf = NULL;
  if (state == rcvPlayerPlaying && channels > 0) {
    snprintf (buf, sizeof (buf), "%i kbps, %.1f kHz, %s", bitrate, ((float) freq) / 1000.0, channels == 2 ? "Stereo" : "Mono");
    surf = FontRenderText (dispFontSmall, buf, BLACK, COL_DISPLAY);
    SurfaceBlit (surf, NULL, surfDisp, &dispRectInfo, 1, 1);
  }
  else if (dispHaveSong) {
    surf = FontRenderText (dispFontSmall, player.SongSubtitle (), BLACK, COL_DISPLAY);
    clipR = Rect (0, 0, dispRectInfo.w, dispRectInfo.h);
    SurfaceBlit (surf, &clipR, surfDisp, &dispRectInfo, -1, 1);
  }
  SurfaceFree (&surf);

  // Done...
  wdgDisplay.SetSurface (surfDisp);
}





// *****************************************************************************
// *                                                                           *
// *                             Top-Level                                     *
// *                                                                           *
// *****************************************************************************


static CScreenMusicMain *scrMusicMain = NULL;


void *AppFuncMusic (int appOp, void *data) {
  switch (appOp) {

    case appOpInit:
      CMusicPlayer::ClassInit ();
      StreamerInit ();
      scrMusicMain = new CScreenMusicMain ();
      scrMusicMain->Setup ();
      return APP_INIT_OK;

    case appOpDone:
      FREEO(scrMusicMain);
      StreamerDone ();
      CMusicPlayer::ClassDone ();
      break;

    case appOpLabel:
      appLaunchButton = data;
      SetAppLaunchLabel (false);
      APP_SET_HOTKEY (data, SDLK_m);
      break;

    case appOpActivate:
      scrMusicMain->Activate ();
      break;

    case appOpLongPush:
      if (AppMusicIsPlayingOrShouldBe ()) AppMusicPlayerOff ();
      else AppMusicPlayerOn ();
  }
  return NULL;
}





// ********** Special API **********


void AppMusicPlayerOn () {
  if (scrMusicMain) scrMusicMain->PlayerOn ();
}


void AppMusicPlayerOff () {
  if (scrMusicMain) scrMusicMain->PlayerOff ();
}


bool AppMusicSetServer (const char *id) {
  int idx = -1;

  if (scrMusicMain) {
    idx = scrMusicMain->Player ()->ServerIdx (id);
    if (idx >= 0) scrMusicMain->Player ()->SetServer (idx);
    else WARNINGF (("Unknown MPD server: '%s'", id));
  }
  return idx >= 0;
}


bool AppMusicIsPlaying () {
  if (!scrMusicMain) return false;
  return scrMusicMain->Player ()->IsPlaying ();
}


bool AppMusicIsPlayingOrShouldBe () {
  if (!scrMusicMain) return false;
  return scrMusicMain->Player ()->IsPlayingOrShouldBe ();
}


bool AppMusicIsPlayingForSure (int minDb) {
  if (!scrMusicMain) return false;
  return scrMusicMain->Player ()->IsPlayingForSure (minDb);
}
