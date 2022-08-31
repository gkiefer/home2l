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


#include "phone.H"

#include "env.H"

#include <pjsua.h>

#include <sys/stat.h>           // for 'open'
#include <errno.h>
#include <string.h>


/* NOTE: PJSIP does not support fully separated phone objects.
 *   Presently, the creation of multiple 'CPhone' objects is possible, but they
 *   all share the same audio and video media.
 *   In the future, the creation of multiple 'CPhone' objects may be disabled,
 *   so that it is discouraged to use multiple objects in a single application.
 */

extern const char *envPhoneRotationKey;





// ********************** Platform-specific adapters ***************************


// ***** Debian *****

#if !ANDROID


#include <alsa/asoundlib.h>


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void AlsaErrorHandler (const char *file, int line, const char *function, int err, const char *fmt, ...) {
  char *buf;

  va_list ap;
  va_start (ap, fmt);
  vasprintf (&buf, fmt, ap);
  va_end (ap);

  DEBUGF (3, ("[ALSA] %s:%i:%s (): %s", file, line, function, buf));

  free (buf);
}
#pragma GCC diagnostic pop


static inline void AlsaInit () {
  snd_lib_error_set_handler (NULL); // AlsaErrorHandler);
    // Unset PJSIP's ALSA error handler to avoid problems with logging (see below).
    // Note [2017-09-02]:
    //   A level of >= 4 results in a strange "Calling pjlib from unknown/external thread." assertion inside PJLIB.
    //   This happens inside 'pjmedia-audiodev/alsa_dev.c:static void alsa_error_handler ()':
    //   PJLIB registers an ALSA log function, which may be triggered by an SDL2-internal thread and calls 'pj_log ()',
    //   which then throws the assertion.
    //   In consequence, log levels > 3 presently cannot be used out-of-the-box.
}


static inline void AndroidInit () {}




// ***** Android *****

#else // !ANDROID


/* The following code replaces the code around 'JNI_OnLoad()' in PJSIP, file 'pjsip/pjlib/src/pj/os_core_unix.c',
 * which had to be disabled by setting 'PJ_JNI_HAS_JNI_ONLOAD=0' in the prebuild script, since PJSIP is built as
 * a static library.
 */


#include <jni.h>

#include "system.H"


JavaVM *pj_jni_jvm = NULL;


static inline void AndroidInit () {
  pj_jni_jvm = AndroidGetJavaVM ();
}


static inline void AlsaInit () {}




#endif // !ANDROID






// *************************** PJSIP-related helpers ***************************


#define NO_ID_PJ PJSUA_INVALID_ID        // "NULL" value for all sorts of IDs (= -1 in PJSIP 2.11) ...


#define PJSTR_ARGS(PJSTR) (int) (PJSTR).slen, (PJSTR).ptr
  // This allows to use printf as follows:
  //   pj_str_t some_pj_str;
  //   printf ("The string reads: '%.*s'.\n", PJSTR_ARGS(some_pj_str));


//~ static char *PjStrDupToC (pj_str_t pjStr) {
  //~ char *ret = malloc (pjStr.slen + 1);
  //~ memcpy (ret, pjStr.ptr, pjStr.slen);
  //~ ret [pjStr.slen] = '\0';
  //~ return ret;
//~ }


static const char *PjStrError (pj_status_t pjStatus) {
  static char buf[256];
  pj_strerror (pjStatus, buf, sizeof (buf));
  return buf;
}


static bool AnalyseSipUri (pj_str_t uri, pj_str_t *retUser, pj_str_t *retDomain) {
  // Analyse a given URI as robustly as possible.
  // Both 'retUser' and 'retDomain' must be defined, they will refer to substrings
  // inside 'uri' or will be set to 'NULL', if no respective component has been found.
  // Returns 'false' if neither a user nor a domain component has been found.
  char *p, *q;

  // Clear return values...
  retUser->ptr = retDomain->ptr = NULL;
  retUser->slen = retDomain->slen = 0;

  // Strip away anything outside "<...>"...
  for (p = uri.ptr + uri.slen - 1; p > uri.ptr && *p != '>'; p--);
  if (*p == '>') {
    for (q = p - 1; q > uri.ptr && *q != '<'; q--);
    if (*q != '<') return false;    // no matching '<' found
    uri.ptr = q + 1;
    uri.slen = p - q - 1;
  }

  // Strip away leading (white)space...
  while (uri.slen > 0 && uri.ptr[0] == ' ') { uri.ptr++; uri.slen--; }

  // Strip away leading 'sip:'...
  if (uri.slen >= 4)
    if (uri.ptr[0] == 's' && uri.ptr[1] == 'i' && uri.ptr[2] == 'p' && uri.ptr[3] == ':') { uri.ptr += 4; uri.slen -= 4; }

  // Strip away leading and trailing (white)space...
  while (uri.slen > 0 && uri.ptr[0] == ' ') { uri.ptr++; uri.slen--; }
  while (uri.slen > 0 && uri.ptr[uri.slen - 1] == ' ') uri.slen--;

  // Search for '@' and eventually return domain part...
  for (p = uri.ptr + uri.slen - 1; p > uri.ptr && *p != '@'; p--);
  if (*p == '@') {
    if (p <= uri.ptr + uri.slen - 2) {  // domain has at least 1 char?
      retDomain->ptr = p + 1;
      retDomain->slen = uri.ptr + uri.slen - retDomain->ptr;
    }
    uri.slen = p - uri.ptr;             // strip away '@' and everything behind
  }

  // The remainder must be a user part...
  if (uri.slen > 0) {
    retUser->ptr = uri.ptr;
    retUser->slen = uri.slen;
  }

  // Done...
  return (retUser->ptr != NULL);
}


static const char *StrMediaDir (pjmedia_dir dir) {
  // Cases commented out are numerically equivalent to others.
  switch (dir) {
    case PJMEDIA_DIR_NONE:              return "NONE";
    case PJMEDIA_DIR_ENCODING:          return "ENCODING";
    // case PJMEDIA_DIR_CAPTURE:           return "CAPTURE";
    case PJMEDIA_DIR_DECODING:          return "DECODING";
    // case PJMEDIA_DIR_PLAYBACK:          return "PLAYBACK";
    // case PJMEDIA_DIR_RENDER:            return "RENDER";
    case PJMEDIA_DIR_ENCODING_DECODING: return "ENCODING_DECODING";
    // case PJMEDIA_DIR_CAPTURE_PLAYBACK:  return "CAPTURE_PLAYBACK";
    // case PJMEDIA_DIR_CAPTURE_RENDER:    return "CAPTURE_RENDER";
    default: return "UNKNOWN";
  }
}


static const char *StrCallMediaStatus (pjsua_call_media_status s) {
  return s == PJSUA_CALL_MEDIA_NONE ? "NONE" :
         s == PJSUA_CALL_MEDIA_ACTIVE ? "ACTIVE" :
         s == PJSUA_CALL_MEDIA_LOCAL_HOLD ? "LOCAL_HOLD" :
         s == PJSUA_CALL_MEDIA_REMOTE_HOLD ? "REMOTE_HOLD" : "ERROR";
}


static void DumpCallInfo (pjsua_call_id callId) {
  pjsua_call_info callInfo;
  pjsua_call_media_info *mediaInfo;
  pj_status_t pjStatus;
  int n;

  DEBUGF (1, ("DumpCallInfo (callId = %i)", callId));

  // Get call info ...
  pjStatus = pjsua_call_get_info (callId, &callInfo);
  if (pjStatus != PJ_SUCCESS) {
    DEBUGF (1, ("failed to obtain call info: %s", PjStrError (pjStatus)));
    return;
  }

  // General...
  DEBUGF (1, ("  local_info = '%.*s', local_contact = '%.*s'",
          PJSTR_ARGS (callInfo.local_info), PJSTR_ARGS (callInfo.local_contact)));
  DEBUGF (1, ("  remote_info = '%.*s', remote_contact = '%.*s'",
          PJSTR_ARGS (callInfo.remote_info), PJSTR_ARGS (callInfo.remote_contact)));
  DEBUGF (1, ("  state = '%.*s', last_status = '%.*s' (%i), media_status = %s",
          PJSTR_ARGS (callInfo.state_text), PJSTR_ARGS (callInfo.last_status_text), callInfo.last_status,
          StrCallMediaStatus (callInfo.media_status)));
  DEBUGF (1, ("  conf_slot = %i, media_cnt = %i, prov_media_cnt = %i",
          callInfo.conf_slot, callInfo.media_cnt, callInfo.prov_media_cnt));
  DEBUGF (1, ("  rem_offerer (SDP offerer?) = %i, rem_aud_cnt = %i, rem_vid_cnt = %i",
          (int) callInfo.rem_offerer, (int) callInfo.rem_aud_cnt, (int) callInfo.rem_vid_cnt));

  // Media...
  for (n = 0; n < (int) callInfo.media_cnt; n++) {
    mediaInfo = &callInfo.media[n];
    DEBUGF (1, ("  Active media %i: index = %i, type = %s, dir = %s, status = %s, stream (aud: conf_slot / vid: dev_index) = %i",
            n, (int) mediaInfo->index,
            mediaInfo->type == PJMEDIA_TYPE_NONE ? "NONE" :
            mediaInfo->type == PJMEDIA_TYPE_AUDIO ? "AUDIO" :
            mediaInfo->type == PJMEDIA_TYPE_VIDEO ? "VIDEO" :
            mediaInfo->type == PJMEDIA_TYPE_APPLICATION ? "NONE" : "UNKOWN",
            StrMediaDir (mediaInfo->dir),
            StrCallMediaStatus (mediaInfo->status),
            mediaInfo->type == PJMEDIA_TYPE_AUDIO ? mediaInfo->stream.aud.conf_slot :
            mediaInfo->type == PJMEDIA_TYPE_VIDEO ? mediaInfo->stream.vid.cap_dev : -999));
  }
  for (n = 0; n < (int) callInfo.prov_media_cnt; n++) {
    mediaInfo = &callInfo.prov_media[n];
    DEBUGF (1, ("  Provisional media %i: index = %i, type = %s, dir = %s, stream (aud: conf_slot / vid: dev_index) = %i",
            n, (int) mediaInfo->index,
            mediaInfo->type == PJMEDIA_TYPE_NONE ? "NONE" :
            mediaInfo->type == PJMEDIA_TYPE_AUDIO ? "AUDIO" :
            mediaInfo->type == PJMEDIA_TYPE_VIDEO ? "VIDEO" :
            mediaInfo->type == PJMEDIA_TYPE_APPLICATION ? "NONE" : "UNKOWN",
            StrMediaDir (mediaInfo->dir),
            mediaInfo->type == PJMEDIA_TYPE_AUDIO ? mediaInfo->stream.aud.conf_slot :
            mediaInfo->type == PJMEDIA_TYPE_VIDEO ? mediaInfo->stream.vid.cap_dev : -999));
  }
}





// *************************** Video Render Device *****************************


#define WINDOWS 2    // the two streams (primary, secondary)
#define WINDOW_MAIN 0
#define WINDOW_SIDE 1

#define VIDEO_DEFAULT_WIDTH   352
#define VIDEO_DEFAULT_HEIGHT  288
#define VIDEO_DEFAULT_FPS      25

static const char *videoDriverName = "Home2l";
static const char *videoDeviceName = "Screen"; // [VIDEO_DEVICES] = { "Main", "Side" };

static pjmedia_vid_dev_index videoDeviceIndex = -1;     // Is set once during the first invocation of 'VideoGetDeviceIndex'.


/* NOTE [2021-06-18, PJSIP 2.11]
 *    PJSIP has the habbit to pass synthetic empty frames to 'put_frame' functions, even if there is no new
 *    incoming video data see pjsip/pjmedia/src/pjmedia/vid_conf.c:791). However, the
 *    function 'pjmedia_port_put_frame()' does not call the application-provided "put frame" function,
 *    but an internal one, which may do automatic image format conversions without checking if the input
 *    frame is valid or empty. They crash with the synthetic empty frame.
 *
 *    As a workaround, we try to offer and support as many formats as possible via 'videoFormatList' here
 *    with the hope that now conversion functions are called. This is not an ideal solution. Let's hope it works.
 */


static uint32_t videoFormatList [] = {
  // Video formats offered ...
  PJMEDIA_FORMAT_RGBA,
  PJMEDIA_FORMAT_RGB24,
  PJMEDIA_FORMAT_BGRA,
  PJMEDIA_FORMAT_DIB,
  PJMEDIA_FORMAT_YUY2,
  PJMEDIA_FORMAT_UYVY,
  PJMEDIA_FORMAT_YVYU,
  PJMEDIA_FORMAT_I420,
  PJMEDIA_FORMAT_YV12,
  PJMEDIA_FORMAT_I420JPEG,
  PJMEDIA_FORMAT_I422JPEG
};


static EPhoneVideoFormat PhoneVideoFormatOf (uint32_t pjMediaFormat) {
  switch (pjMediaFormat) {
#if PJ_IS_BIG_ENDIAN
    case PJMEDIA_FORMAT_RGBA:     return pvfRGBA8888;
    case PJMEDIA_FORMAT_RGB24:    return pvfRGB24;
    case PJMEDIA_FORMAT_BGRA:     return pvfBGRA8888;
#else /* PJ_IS_BIG_ENDIAN */
    case PJMEDIA_FORMAT_RGBA:     return pvfABGR8888;
    case PJMEDIA_FORMAT_RGB24:    return pvfBGR24;
    case PJMEDIA_FORMAT_BGRA:     return pvfARGB8888;
#endif /* PJ_IS_BIG_ENDIAN */
    case PJMEDIA_FORMAT_DIB :     return pvfRGB24;
    case PJMEDIA_FORMAT_YUY2:     return pvfYUY2;
    case PJMEDIA_FORMAT_UYVY:     return pvfUYVY;
    case PJMEDIA_FORMAT_YVYU:     return pvfYVYU;
    case PJMEDIA_FORMAT_I420:     return pvfIYUV;
    case PJMEDIA_FORMAT_YV12:     return pvfYV12;
    case PJMEDIA_FORMAT_I420JPEG: return pvfIYUV;
    case PJMEDIA_FORMAT_I422JPEG: return pvfYV12;
    default: break;
  }
  return pvfNone;
}


struct TVideoStream {
  pjmedia_vid_dev_stream base;      // Base stream (must be first element in structure)
  pjmedia_vid_dev_param param;      // Settings

  // Output data (may be protected by 'windowsMutex', the fields above are not)...
  bool running;                     // video stream running?
  bool changed;                     // is set by 'VideoStreamPutFrame ()'.
  pjmedia_frame frame;              // frame data in native format ('buf == NULL' => invalid)
  TPhoneVideoFrame phoneVideoFrame; // points inside 'pjFrame'
};


static inline TVideoStream *CastVideoStream (pjmedia_vid_dev_stream *strm) {
  return (TVideoStream *) ((uint8_t *) strm - offsetof (TVideoStream, base));
}


static pthread_mutex_t windowsMutex = PTHREAD_MUTEX_INITIALIZER;       // Mutex for 'windows' and all output data in 'TVideoStream' objects referred to
static TVideoStream *windows[WINDOWS] = { NULL, NULL };
  // The 'TVideoStream *' pointers are the native window handles of the respective PJMEDIA streams.
  // To put some output to screen, the native window handle of a stream must be stored in this
  // array.


static int WindowOfStream (pjmedia_vid_dev_stream *strm) {
  TVideoStream *videoStream = CastVideoStream (strm);
  return  videoStream == windows[0] ? 0
        : videoStream == windows[1] ? 1
        : -1;
}


static void WindowAssign (int window, pjmedia_vid_dev_hwnd *hwnd) {
  pthread_mutex_lock (&windowsMutex);
  if (hwnd) {
    ASSERT (hwnd->type == PJMEDIA_VID_DEV_HWND_TYPE_NONE && window >= 0 && window < WINDOWS);
    windows[window] = (TVideoStream *) hwnd->info.window;
  }
  else windows[window] = NULL;
  pthread_mutex_unlock (&windowsMutex);
  //~ INFOF (("### WindowAssign (): %08x -> %i", windows[window], window));
}


static pj_status_t WindowAssignByID (int window, pjsua_vid_win_id wid) {
  pjsua_vid_win_info winInfo;
  pj_status_t pjStatus;

  //~ INFOF (("### WindowAssignByID (%i, %i)", window, wid));
  pthread_mutex_lock (&windowsMutex);
  windows[window] = NULL;
  pthread_mutex_unlock (&windowsMutex);
  if (wid < 0) return PJ_EINVAL;
  pjStatus = pjsua_vid_win_get_info (wid, &winInfo);
  if (pjStatus != PJ_SUCCESS) return pjStatus;
  //~ INFO ("###    ... have winInfo.");
  if (winInfo.rdr_dev != videoDeviceIndex) return PJ_EINVAL;
  //~ INFO ("###    ... correct device.");
  WindowAssign (window, &winInfo.hwnd);
  return PJ_SUCCESS;
}





//  ***** Stream callbacks *****


pj_status_t VideoStreamGetParam (pjmedia_vid_dev_stream *strm, pjmedia_vid_dev_param *param) {
  // Get the running parameters for the specified video stream.
  //     strm The video stream.
  //     param  Video stream parameters to be filled in by this function once it returns successfully.
  // Returns
  //     PJ_SUCCESS on successful operation or the appropriate error code.
  TVideoStream *videoStream = CastVideoStream (strm);

  //~ INFOF (("### VideoStreamGetParam (%08x, %i)", videoStream, WindowOfStream (strm)));

  *param = videoStream->param;
  return PJ_SUCCESS;
}


pj_status_t VideoStreamGetCap (pjmedia_vid_dev_stream *strm, pjmedia_vid_dev_cap cap, void *value) {
  // Get the value of a specific capability of the video stream.
  //     strm The video stream.
  //     cap  The video capability which value is to be retrieved.
  //     value  Pointer to value to be filled in by this function once it returns successfully. Please see the type of value to be supplied in the pjmedia_vid_dev_cap documentation.
  // Returns
  //     PJ_SUCCESS on successful operation or the appropriate error code.
  TVideoStream *videoStream = CastVideoStream (strm);
  pjmedia_format *retFmt = (pjmedia_format *) value;
  pjmedia_rect_size *retRectSize = (pjmedia_rect_size *) value;
  pjmedia_vid_dev_hwnd *retHwnd = (pjmedia_vid_dev_hwnd *) value;

  //~ INFOF (("### VideoStreamGetCap (%08x, %i)", videoStream, WindowOfStream (strm)));

  switch (cap) {
    case PJMEDIA_VID_DEV_CAP_FORMAT:
      *retFmt = videoStream->param.fmt;
      break;
    case PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE:
      *retRectSize = videoStream->param.fmt.det.vid.size;   // videoStream->param.disp_size;
      break;
    case PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW:
      *retHwnd = videoStream->param.window;
      break;
    default:
      return PJMEDIA_EVID_INVCAP;
  }
  return PJ_SUCCESS;
}


pj_status_t VideoStreamSetCap (pjmedia_vid_dev_stream *strm, pjmedia_vid_dev_cap cap, const void *value) {
  // Set the value of a specific capability of the video stream.
  //     strm The video stream.
  //     cap The video capability which value is to be set.
  //     value Pointer to value. Please see the type of value to be supplied in the pjmedia_vid_dev_cap documentation.
  // Returns
  //     PJ_SUCCESS on successful operation or the appropriate error code.
  pjmedia_format *fmt = (pjmedia_format *) value;
  const pjmedia_rect_size *rectSize = (pjmedia_rect_size *) value;
  TVideoStream *videoStream = CastVideoStream (strm);

  //~ INFOF (("### VideoStreamSetCap (%08x, win = %i, cap = %i)", videoStream, WindowOfStream (strm), (int) cap));

  switch (cap) {
    case PJMEDIA_VID_DEV_CAP_FORMAT:
      ASSERT (fmt->id == PJMEDIA_FORMAT_I420);
      ASSERT (fmt->type == PJMEDIA_TYPE_VIDEO);
      if (fmt->detail_type == PJMEDIA_FORMAT_DETAIL_NONE) break;    // no details => return
      ASSERT (fmt->detail_type == PJMEDIA_FORMAT_DETAIL_VIDEO);
      videoStream->param.fmt = *fmt;
      // TBD: Update dimensions in 'videoStream'?
      break;
    case PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE:
      fmt = &(videoStream->param.fmt);
      if (fmt->detail_type != PJMEDIA_FORMAT_DETAIL_VIDEO) {
        fmt->detail_type = PJMEDIA_FORMAT_DETAIL_VIDEO;
        memset (&fmt->det.vid, 0, sizeof (pjmedia_video_format_detail));
      }
      fmt->det.vid.size = *rectSize;
      // TBD: Update dimensions in 'videoStream'!
      break;
    default:
      return PJMEDIA_EVID_INVCAP;
  }
  return PJ_SUCCESS;
}


pj_status_t VideoStreamStart (pjmedia_vid_dev_stream *strm) {
  // Start the stream.
  //     strm The video stream.
  // Returns
  //     PJ_SUCCESS on successful operation or the appropriate error code.
  TVideoStream *videoStream = CastVideoStream (strm);

  //~ INFOF (("### VideoStreamStart (%08x, %i)", videoStream, WindowOfStream (strm)));

  videoStream->running = true;
  return PJ_SUCCESS;
}


pj_status_t VideoStreamPutFrame (pjmedia_vid_dev_stream *strm, const pjmedia_frame *frame) {
  // Put one frame to the stream. Application needs to call this function periodically only if
  // the stream doesn't support "active interface", i.e. the pjmedia_vid_dev_info.has_callback member
  // is PJ_FALSE.
  //     strm The video stream.
  //     frame  The video frame to put to the device.
  // Returns
  //     PJ_SUCCESS on successful operation or the appropriate error code.
  TVideoStream *videoStream = CastVideoStream (strm);

  //~ INFOF (("### VideoStreamPutFrame (videoStream=%08x, frame=%08x, window=%i)", videoStream, frame, WindowOfStream (strm)));
  //~ if (frame) INFOF (("###   frame = (win = %i, type = %i, buf = %08x, size = %i, timestamp = %i)", WindowOfStream (strm), frame->type, frame->buf, frame->size, frame->timestamp));

  // Sanity, ignore empty frames ...
  if (!videoStream->running) return PJ_SUCCESS;
  if (frame->size == 0 || frame->buf == NULL) return PJ_SUCCESS;

  // Lock windows...
  pthread_mutex_lock (&windowsMutex);

  // Check if the frame really changed ...
  //   WORKAROUND [2021-06-15, PJSIP 2.11 with VP8]:
  //     If the far end switched off video, this function keeps on being called with new time stamps,
  //     but identical images. It appears, the only way to check if the incoming video is still present is
  //     to compare the complete frame (see pjsip/pjmedia/src/pjmedia/vid_conf.c:791).
  if (videoStream->frame.size >= frame->size) {
    if (memcmp (videoStream->frame.buf, frame->buf, frame->size) == 0) {
      pthread_mutex_unlock (&windowsMutex);
      return PJ_SUCCESS;
    }
  }

  // (Re-)alloc frame...
  if ((int) videoStream->frame.size < (int) frame->size) {
    if (videoStream->frame.buf) free (videoStream->frame.buf);
    videoStream->frame.buf = malloc (frame->size);
    videoStream->frame.size = frame->size;
  }

  // Copy frame...
  //   [2021-06-18, PJSIP 2.11] No clear information could be found in the PJSIP documentation on
  //       who is owning the 'frame' object and its dynamic data. The PJSIP SDL driver copies a reference
  //       to 'frame' and uses it in another thread at an unspecified time later, but it never frees or
  //       destroys the object.
  //       For this reason, we copy the frame here to be on the safe side.
  //       TBD: Check later, if we can eliminate this copying.
  pjmedia_frame_copy (&(videoStream->frame), frame);

  // Set 'phoneVideoFrame' fields...
  videoStream->changed = true;

  videoStream->phoneVideoFrame.format = PhoneVideoFormatOf (videoStream->param.fmt.id);
  videoStream->phoneVideoFrame.w = videoStream->param.fmt.det.vid.size.w;
  videoStream->phoneVideoFrame.h = videoStream->param.fmt.det.vid.size.h;

  videoStream->phoneVideoFrame.planeY = NULL;
  videoStream->phoneVideoFrame.data = (uint8_t *) videoStream->frame.buf;
  switch (videoStream->param.fmt.id) {
    case PJMEDIA_FORMAT_YUY2:
    case PJMEDIA_FORMAT_UYVY:
    case PJMEDIA_FORMAT_YVYU:
      // Packed formats ...
      videoStream->phoneVideoFrame.pitch = videoStream->phoneVideoFrame.w * 2;
      break;
    default:
      // Normal case ...
      videoStream->phoneVideoFrame.pitch = videoStream->phoneVideoFrame.w;
  }

  // Unlock windows & done...
  pthread_mutex_unlock (&windowsMutex);
  return PJ_SUCCESS;
}


pj_status_t VideoStreamStop (pjmedia_vid_dev_stream *strm) {
  // Stop the stream.
  //     strm The video stream.
  // Returns
  //     PJ_SUCCESS on successful operation or the appropriate error code.
  TVideoStream *videoStream = CastVideoStream (strm);

  //~ INFOF (("### VideoStreamStop (%08x, %i)", videoStream, WindowOfStream (strm)));

  videoStream->running = false;
  return PJ_SUCCESS;
}


pj_status_t VideoStreamDestroy (pjmedia_vid_dev_stream *strm) {
  // Destroy the stream.
  //     strm The video stream.
  // Returns
  //     PJ_SUCCESS on successful operation or the appropriate error code.
  TVideoStream *videoStream = CastVideoStream (strm);
  int win;

  // Unlink from windows...
  pthread_mutex_lock (&windowsMutex);
  win = WindowOfStream (strm);
  if (win >= 0) windows[win] = NULL;
  pthread_mutex_unlock (&windowsMutex);

  //~ INFOF (("### VideoStreamDestroy (%08x, %i)", videoStream, win));

  // Cleanup object...
  if (videoStream->frame.buf) free (videoStream->frame.buf);

  // Done...
  free (videoStream);
  return PJ_SUCCESS;
}


static pjmedia_vid_dev_stream_op videoStreamCallbacks {
  &VideoStreamGetParam,
  &VideoStreamGetCap,
  &VideoStreamSetCap,
  &VideoStreamStart,
  NULL,
  &VideoStreamPutFrame,
  &VideoStreamStop,
  &VideoStreamDestroy
};





//  ***** Factory callbacks *****


pj_status_t VideoFactoryInit (pjmedia_vid_dev_factory *f) {
  // Initialize the video device factory.
  //   f The video device factory

  //~ INFO ("### VideoFactoryInit ()");
  return PJ_SUCCESS;
}


pj_status_t VideoFactoryDestroy (pjmedia_vid_dev_factory *f) {
  // Close this video device factory and release all resources back to the operating system.
  //     f  The video device factory.

  //~ INFO ("### VideoFactoryDestroy ()");
  return PJ_SUCCESS;
}


unsigned VideoFactoryGetDevCount (pjmedia_vid_dev_factory *f) {
  // Get the number of video devices installed in the system.
  //     f  The video device factory.

  //~ INFO ("### VideoFactoryGetDevCount ()");
  return 1;
}


pj_status_t VideoFactoryGetDevInfo (pjmedia_vid_dev_factory *f, unsigned index, pjmedia_vid_dev_info *info) {
  // Get the video device information and capabilities.
  //     f  The video device factory.
  //     index  Device index.
  //     info   The video device information structure which will be initialized by this function once it returns successfully.
  //~ INFOF (("### VideoFactoryGetDevInfo (%i)", (int) index));
  int i;

  PJ_UNUSED_ARG (f);
  PJ_ASSERT_RETURN (index == 0, PJMEDIA_EVID_INVDEV);

  bzero(info, sizeof (*info));
  info->id = index;
  strcpy (info->name, videoDeviceName);
  strcpy (info->driver, videoDriverName);
  info->dir = PJMEDIA_DIR_RENDER;
  info->has_callback = PJ_FALSE;     // PJSIP SDL driver has 'PJ_FALSE'.
  info->caps =  PJMEDIA_VID_DEV_CAP_FORMAT | PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE | PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW;

  // Init formats ...
  info->fmt_cnt = ENTRIES (videoFormatList);
  for (i = 0; i < ENTRIES (videoFormatList); i++)
    pjmedia_format_init_video (&info->fmt[i], videoFormatList[i], VIDEO_DEFAULT_WIDTH, VIDEO_DEFAULT_HEIGHT, VIDEO_DEFAULT_FPS, 1);
    // NOTE: The format "I420" appears to be the only one possible here (with VP8, x264, ffmpeg).
    //       On the other hand, "YV12" is currently the only one supported by Home2l, and the same as delivered by liblinphone.
    //       By defintion, both only differ in the order of the U and V planes. For some reason, everything
    //       appears to be fine with selecting I420 here and assuming YV12 in CWidgetVideo::Iterate () (app_phone.C).
    //       It's magic!
  return PJ_SUCCESS;
}


pj_status_t VideoFactoryDefaultParam (pj_pool_t *pool, pjmedia_vid_dev_factory *f, unsigned index, pjmedia_vid_dev_param *param) {
  // Initialize the specified video device parameter with the default values for the specified device.
  //     f      The video device factory.
  //     index  Device index.
  //     param  The video device parameter.

  //~ INFOF (("### VideoFactoryDefaultParam (%i)", (int) index));

  PJ_UNUSED_ARG (pool);
  PJ_UNUSED_ARG (f);
  PJ_ASSERT_RETURN (index == 0, PJMEDIA_EVID_INVDEV);

  bzero (param, sizeof (*param));  // zero-out everything, optional fields are left out now below.
  param->dir = PJMEDIA_DIR_RENDER;
  param->cap_id = PJMEDIA_VID_INVALID_DEV;
  param->rend_id = index;
  param->clock_rate = PJSUA_DEFAULT_CLOCK_RATE;   // stock SDL driver uses 90000 (0 = unset does not work)
  param->flags = PJMEDIA_VID_DEV_CAP_FORMAT | PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE;

  // Format... (see comment in 'VideoFactoryDevInfo')
  pjmedia_format_init_video(&param->fmt, videoFormatList[0], VIDEO_DEFAULT_WIDTH, VIDEO_DEFAULT_HEIGHT, VIDEO_DEFAULT_FPS, 1);
  return PJ_SUCCESS;
}


pj_status_t VideoFactoryCreateStream (pjmedia_vid_dev_factory *f, pjmedia_vid_dev_param *param, const pjmedia_vid_dev_cb *cb, void *userData, pjmedia_vid_dev_stream **pVidStrm) {
  // Open the video device and create video stream. See pjmedia_vid_dev_stream_create()
  TVideoStream *videoStream;

  PJ_UNUSED_ARG (f);
  PJ_UNUSED_ARG (cb);
  PJ_UNUSED_ARG (userData);

  // Sanity...
  PJ_ASSERT_RETURN (param->dir == PJMEDIA_DIR_RENDER, PJ_EINVAL);

  // Allocate stream...
  videoStream = (TVideoStream *) malloc (sizeof (TVideoStream));

  //~ INFOF (("### VideoFactoryCreateStream () -> %08x", videoStream));

  // Fill '*videoStream'...
  bzero (videoStream, sizeof (TVideoStream));   // zero out complete object; only non-zero fields are set below...

  videoStream->base.op = &videoStreamCallbacks;

  videoStream->param = *param;
  videoStream->param.window.type = PJMEDIA_VID_DEV_HWND_TYPE_NONE;
  videoStream->param.window.info.window = (void *) videoStream;

  videoStream->frame.type = PJMEDIA_FRAME_TYPE_VIDEO;

  // Done...
  *pVidStrm = &(videoStream->base);
  return PJ_SUCCESS;
}


pj_status_t VideoFactoryRefresh (pjmedia_vid_dev_factory *f) {
  // Refresh the list of video devices installed in the system.
  //     f  The video device factory.
  //~ INFO ("### VideoFactoryRefresh ()");
  PJ_UNUSED_ARG(f);
  return PJ_SUCCESS;
}


static pjmedia_vid_dev_factory_op videoFactoryOps = {
    &VideoFactoryInit,
    &VideoFactoryDestroy,
    &VideoFactoryGetDevCount,
    &VideoFactoryGetDevInfo,
    &VideoFactoryDefaultParam,
    &VideoFactoryCreateStream,
    &VideoFactoryRefresh
};





// ********** Top-level ... **********


static pjmedia_vid_dev_factory* VideoFactoryCreateFunc (pj_pool_factory *pf) {
  static pjmedia_vid_dev_factory videoFactory;

  memset (&videoFactory, 0, sizeof (videoFactory));
  videoFactory.op = &videoFactoryOps;
  return &videoFactory;
}


static pjmedia_vid_dev_index VideoGetDeviceIndex () {
  pjmedia_vid_dev_info vdi;
  pjmedia_vid_dev_index n;

  if (videoDeviceIndex >= 0) return videoDeviceIndex;

  for (n = (int) pjsua_vid_dev_count () - 1; n >= 0; n--) {
    ASSERT (PJ_SUCCESS == pjsua_vid_dev_get_info (n, &vdi));
    if (strcmp (vdi.driver, videoDriverName) == 0) {
      videoDeviceIndex = n;
      return n;
    }
  }
  ASSERT (false);   // should never get here
  return -1;
}





// *************************** LIBDATA *****************************************


struct TMgmtCheckRec {
  bool regState, callState, mediaState;
  pjsua_call_id incomingCallId;
  pjsua_call_id callId;   // last callId ID of a "call state" event (to identify 'callStatus')
  int callStatus;         // last known call status code, -1 == unknown
  int dtmfDigit;          // -1 == unset
};


static inline void MgmtCheckRecClear (TMgmtCheckRec *rec) {
  rec->regState = rec->callState = rec->mediaState = false;
  rec->incomingCallId = rec->callId = NO_ID_PJ;
  rec->callStatus = -1;
  rec->dtmfDigit = -1;
}


struct TPhoneData {
  bool isSet, haveAccount;
  pjsua_acc_id pjAccountId;

  pjsua_call_id pjCallId[2];
  int callStatus[2];                // last known call status code, -1 == unknown
  pjsua_player_id playerId;         // audio player ID for ringback
  TTicks tLastStatusLog;            // used for regular status logs, e.g. during calls

  TMgmtCheckRec check;              // [T:any] - data potentially accessed by assynchronous callbacks;
                                    //           protected by the mutex 'mgmtMutex'
};

#define PHONE_LIBDATA(phone) (* (TPhoneData *) phone->GetLibData ())
#define LIBDATA (* (TPhoneData *) libData)

STATIC_ASSERT (sizeof (TPhoneData) <= LIBDATA_SIZE);





// *************************** Phone management ********************************


#define MAX_PHONES 1        // Maximum number of allowed phones


static pthread_mutex_t mgmtMutex = PTHREAD_MUTEX_INITIALIZER;   // Mutex for all following 'mgmt*' variables
static int mgmtPhones = 0;        // count number of phones to create/destroy 'PPJSUA' accordingly
static CPhone *mgmtPhoneList[MAX_PHONES];


static inline void MgmtLock () { pthread_mutex_lock (&mgmtMutex); }
static inline void MgmtUnlock () { pthread_mutex_unlock (&mgmtMutex); }


// All the following function assert that 'mgmtMutex' is already locked - the caller is responsible!
// Return values are probably only valid until the next 'MgmtUnlock ()' operation!


static void MgmtAddPhone (CPhone *phone) {
  ASSERT (mgmtPhones < MAX_PHONES);

  mgmtPhoneList[mgmtPhones] = phone;
  mgmtPhones++;
}


static void MgmtDelPhone (CPhone *phone) {
  int id;

  for (id = 0; id < mgmtPhones; id++) if (mgmtPhoneList[id] == phone) break;
  ASSERT (id < mgmtPhones);     // phone should not be deleted twice!

  mgmtPhones--;
  mgmtPhoneList[id] = mgmtPhoneList[mgmtPhones];
}


static int MgmtPhoneIdOfAccount (pjsua_acc_id accId) {
  for (int n = 0; n < mgmtPhones; n++)
    if (PHONE_LIBDATA(mgmtPhoneList[n]).pjAccountId == accId) return n;
  return -1;
}


static CPhone *MgmtPhoneOfAccount (pjsua_acc_id accId) {
  int id = MgmtPhoneIdOfAccount (accId);
  return id >= 0 ? mgmtPhoneList[id] : NULL;
}


static int MgmtPhoneIdOfCall (pjsua_call_id callId) {
  for (int n = 0; n < mgmtPhones; n++)
    if (PHONE_LIBDATA(mgmtPhoneList[n]).pjCallId[0] == callId ||
        PHONE_LIBDATA(mgmtPhoneList[n]).pjCallId[1] == callId) return n;
  return -1;
}


static CPhone *MgmtPhoneOfCall (pjsua_call_id callId) {
  int id = MgmtPhoneIdOfCall (callId);
  return id >= 0 ? mgmtPhoneList[id] : NULL;
}





// *************************** Media management ********************************


static int mediaDevAudioIn = PJMEDIA_AUD_DEFAULT_CAPTURE_DEV;
static int mediaDevAudioOut = PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV;
static int mediaDevVideoIn = PJMEDIA_VID_DEFAULT_CAPTURE_DEV;
//~ #define mediaDevVideoOut (VideoGetDeviceIndex ())


static CPhone *mediaOwner = NULL;
  // In PJSIP, only a single core and a single set of audio/video devices are allowed at a time.
  // This variable points to the 'CPhone' object currently owning the right to use the devices.
  // This variable may only be accessed from the main thread. It is primarily maintained by
  // 'MediaUpdate ()', but call action methods should also recognize ist to avoid errors.
static unsigned mediaActivated = 0;
  // Media currently active
static bool mediaVideoTransmitting = false;
  // Video camera stream is currently transmitted
static bool mediaVideoPreview = false;
  // Video camera preview is currently active

static void MediaDumpDevicesAndCodecs () {
  /* Log a readable list of available and selected devices and codecs.
   * The output is important to inform the user about possible values
   * for the device and codec selection environment parameters.
   *
   * Selected audio and video devices are printed depending on 'mediaDev*',
   * which must have been set in advance.
   *
   * Codecs are displayed as selected if their priority is set to
   * 'PJMEDIA_CODEC_PRIO_HIGHEST'. Unwanted codecs may not be disabled
   * completely to save a bit of compatibility if the preferred codec
   * is not usable in a call.
   */
  pjmedia_aud_dev_info audDevInfo[PJMEDIA_AUD_MAX_DEVS];
  pjsua_codec_info codecInfo[MAX (PJMEDIA_CODEC_MGR_MAX_CODECS, PJMEDIA_VID_CODEC_MGR_MAX_CODECS)];
  pjmedia_vid_dev_info vidDevInfo;
  int devAudioIn, devAudioOut;
  int n, k, count;

  INFO ("\n--------------------------- Phone Devices and Codecs ---------------------------");

  // Audio...
  INFO ("\nAudio Devices:");
  count = ENTRIES (audDevInfo);
  ASSERT (PJ_SUCCESS == pjsua_enum_aud_devs (audDevInfo, (unsigned *) &count));
  ASSERT (PJ_SUCCESS == pjsua_get_snd_dev (&devAudioIn, &devAudioOut));
  if (devAudioIn < 0) devAudioIn = mediaDevAudioIn;
  if (devAudioOut < 0) devAudioOut = mediaDevAudioOut;
  for (n = 0; n < count; n++) {
    INFOF (("  %c%3i. '%s' (#in/#out = %i/%i, driver = '%s', %i format(s))",
            n == mediaDevAudioIn ? (n == mediaDevAudioOut ? '*' : 'M') : (n == mediaDevAudioOut ? 'P' : ' '),
            n, audDevInfo[n].name, audDevInfo[n].input_count, audDevInfo[n].output_count,
            audDevInfo[n].driver, audDevInfo[n].ext_fmt_cnt));
  }

  // Audio codecs...
  INFO ("\nAudio Codecs:");
  count = ENTRIES (codecInfo);
  ASSERT (PJ_SUCCESS == pjsua_enum_codecs (codecInfo, (unsigned *) &count));
  for (n = 0; n < count; n++) {
    INFOF (("  %c%3i. '%.*s' (prio = %i, desc = '%.*s')",
            codecInfo[n].priority > PJMEDIA_CODEC_PRIO_HIGHEST ? '*' : ' ', n,
            PJSTR_ARGS (codecInfo[n].codec_id), codecInfo[n].priority,
            PJSTR_ARGS (codecInfo[n].desc)));
  }

  // Video...
  //   Note [2021-05-18]: There is also a 'pjsua_vid_enum_devs()' call to get all
  //       devices at once, similar to the audio devices and codecs.
  //       However, on Android, it appearantly does not work and returns a scrambled
  //       list (alignment problem?).
  INFO ("\nVideo Devices:");
  count = pjsua_vid_dev_count ();
  for (n = 0; n < count; n++) {
    ASSERT (PJ_SUCCESS == pjsua_vid_dev_get_info (n, &vidDevInfo));
    INFOF (("  %c%3i. '%s' (driver = '%s', dir = %s, %i format(s))",
            (n == mediaDevVideoIn || n == VideoGetDeviceIndex ()) ? '*' : ' ',
            n, vidDevInfo.name, vidDevInfo.driver,
            StrMediaDir (vidDevInfo.dir),
            vidDevInfo.fmt_cnt));
    for (k = 0; k < (int) vidDevInfo.fmt_cnt; k++) {
      DEBUGF(1, ("          format %i: %ix%i, %i/%i fps, %i bps, %s", k,
            (int) vidDevInfo.fmt[k].det.vid.size.w, (int) vidDevInfo.fmt[k].det.vid.size.h,
            (int) vidDevInfo.fmt[k].det.vid.fps.num, (int) vidDevInfo.fmt[k].det.vid.fps.denum,
            (int) vidDevInfo.fmt[k].det.vid.avg_bps,
            StrPhoneVideoFormat (PhoneVideoFormatOf (vidDevInfo.fmt[k].id))));
    }
  }

  // Video codecs...
  INFO ("\nVideo Codecs:");
  count = ENTRIES (codecInfo);
  ASSERT (PJ_SUCCESS == pjsua_vid_enum_codecs (codecInfo, (unsigned *) &count));
  for (n = 0; n < count; n++) {
    INFOF (("  %c%3i. '%.*s' (prio = %i, desc = '%.*s')",
            codecInfo[n].priority >= PJMEDIA_CODEC_PRIO_HIGHEST ? '*' : ' ', n,
            PJSTR_ARGS (codecInfo[n].codec_id), codecInfo[n].priority,
            PJSTR_ARGS (codecInfo[n].desc)));
  }

  INFO ("\n------------------------ Phone Devices and Codecs (END) ------------------------\n");
}


static void MediaSetup () {
  const char *devStr;
  pj_status_t pjStatus;
  pj_str_t pjStr;

  // Audio device(s) ...
  devStr = envPhoneAudioInDevice ? envPhoneAudioInDevice : envPhoneAudioDevice;
  if (devStr) {
    if (PJ_SUCCESS != pjmedia_aud_dev_lookup (envPhoneAudioDriver, devStr, &mediaDevAudioIn)) {
      WARNINGF (("Invalid audio input device: '%s'", devStr));
      mediaDevAudioIn = PJMEDIA_AUD_DEFAULT_CAPTURE_DEV;
    }
    //~ INFOF (("### Selected audio in device %i", mediaDevAudioIn));
  }
  devStr = envPhoneAudioInDevice ? envPhoneAudioOutDevice : envPhoneAudioDevice;
  if (devStr) {
    if (PJ_SUCCESS != pjmedia_aud_dev_lookup (envPhoneAudioDriver, devStr, &mediaDevAudioOut)) {
      WARNINGF (("Invalid audio output device: '%s'", devStr));
      mediaDevAudioOut = PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV;
    }
    //~ INFOF (("### Selected audio out device %i", mediaDevAudioOut));
  }

  // Audio codec ...
  if (envPhoneAudioCodec) {
    pj_strset2 (&pjStr, (char *) envPhoneAudioCodec);
    pjStatus = pjsua_codec_set_priority (&pjStr, PJMEDIA_CODEC_PRIO_HIGHEST);
    if (pjStatus != PJ_SUCCESS)
      WARNINGF (("Failed to prioritize audio codec '%s': %s", PjStrError (pjStatus)));
  }

  // Echo cancellation ...
  ASSERT (PJ_SUCCESS == pjsua_set_ec (
    envPhoneEchoTail > 0 ? envPhoneEchoTail : PJSUA_DEFAULT_EC_TAIL_LEN,
    PJMEDIA_ECHO_USE_SW_ECHO
    | (envPhoneEchoAlgo == 0 ? PJMEDIA_ECHO_SIMPLE :
       envPhoneEchoAlgo == 1 ? PJMEDIA_ECHO_SPEEX :
       envPhoneEchoAlgo == 2 ? PJMEDIA_ECHO_WEBRTC :
       PJMEDIA_ECHO_DEFAULT)
    | (envPhoneEchoAggressiveness == 0 ? PJMEDIA_ECHO_AGGRESSIVENESS_CONSERVATIVE :
       envPhoneEchoAggressiveness == 1 ? PJMEDIA_ECHO_AGGRESSIVENESS_MODERATE :
       envPhoneEchoAggressiveness == 2 ? PJMEDIA_ECHO_AGGRESSIVENESS_AGGRESSIVE :
       PJMEDIA_ECHO_AGGRESSIVENESS_DEFAULT)
    | (envPhoneEchoNoiseSuppression ? PJMEDIA_ECHO_USE_NOISE_SUPPRESSOR : 0)
  ));

  // Video in (capture) device ...
  if (envPhoneVideoDevice) {
    if (PJ_SUCCESS != pjmedia_vid_dev_lookup (envPhoneVideoDriver, envPhoneVideoDevice, &mediaDevVideoIn)) {
      WARNINGF (("Invalid video input device: '%s'", envPhoneVideoDevice));
      mediaDevVideoIn = PJMEDIA_VID_DEFAULT_CAPTURE_DEV;
    }
  }
  //    camera orientation ...
  if (envPhoneRotation != 0) {
    /* [2021-06-16, PJSIP 2.11]
     *   On Android, the call 'pjsua_vid_dev_set_setting()' leads to a strange crash inside 'PJSUA_LOCK()' right at the
     *   entry of the function. On Debian/Linux, video cameras appearently do not always support the orientation capability,
     *   and the feature is usually not necessary, anyway.
     *   For these reasons, the feature is currently disabled (fixes are welcome).
     */
    WARNINGF (("Camera orientation correction is presently unsupported: Ignoring '%s = %i'.", envPhoneRotationKey, envPhoneRotation));
    /*
    pjmedia_vid_dev_info pjDevInfo;
    pjmedia_orient pjCamOrient;

    ASSERT (PJ_SUCCESS == pjmedia_vid_dev_get_info (mediaDevVideoIn, &pjDevInfo));
    if (!(pjDevInfo.caps & PJMEDIA_VID_DEV_CAP_ORIENTATION))
      WARNINGF (("Camera does not support orientation correction: Ignoring '%s = %i'.", envPhoneRotationKey, envPhoneRotation));
    else {
      //~ INFOF (("### camDev = %i, devInfo.caps = %x", mediaDevVideoIn, pjDevInfo.caps));
      pjCamOrient =
        envPhoneRotation ==   0 ? PJMEDIA_ORIENT_NATURAL :
        envPhoneRotation ==  90 ? PJMEDIA_ORIENT_ROTATE_90DEG :
        envPhoneRotation == 180 ? PJMEDIA_ORIENT_ROTATE_180DEG :
        envPhoneRotation == 270 ? PJMEDIA_ORIENT_ROTATE_270DEG :
        PJMEDIA_ORIENT_UNKNOWN;
      if (pjCamOrient == PJMEDIA_ORIENT_UNKNOWN)
        WARNINGF (("Unsupported camera orientation ('%s'): %i", envPhoneRotationKey, envPhoneRotation));
      else
        ASSERT_WARN (PJ_SUCCESS == pjsua_vid_dev_set_setting (
            mediaDevVideoIn, PJMEDIA_VID_DEV_CAP_ORIENTATION, &pjCamOrient, true
          ));
    }
    */
  }

  // Video codec ...
  if (envPhoneVideoCodec) {
    pj_strset2 (&pjStr, (char *) envPhoneVideoCodec);
    pjStatus = pjsua_codec_set_priority (&pjStr, PJMEDIA_CODEC_PRIO_HIGHEST);
    if (pjStatus != PJ_SUCCESS)
      WARNINGF (("Failed to prioritize video codec '%s': %s", PjStrError (pjStatus)));
  }

}


static void MediaUpdate () {
  // The behavior of the function depends on the actually selected media,
  // but also on
  // a) the current phone state - audio devices are never accessed in
  //    "device-permitting" states (see notes in 'phone.H') - and
  // b) the presence of a video stream in the active call.
  //
  // Hence, this function must also be called on each state change by
  // which the "device-permitting" status may change or whenever a video stream
  // is established.
  //
  // Audio in/out is switched at the device level, streams are always enabled.
  // Video in/out switching may in the future involve re-invites.
  //
  pjsua_call_id callId;
  pjsua_call_info callInfo;
  pjsua_conf_port_id confId;
  pjsua_snd_dev_param sndDevParam;
  pjsua_vid_preview_param previewParam;
  pjsua_call_vid_strm_op_param opParam;
  pj_status_t pjStatus;
  unsigned mediaSelected;     // media as selected by the CPhone object
  unsigned _mediaActivated;   // media actually active (prospective value for 'mediaActivated')
  unsigned mediaToChange;     // mask to indicate media to change wrt. to the present state
  int n;

  // Determine effectively selected media...
  //   These are the media as requested by the phone with the following exceptions:
  //   - If no phone is active or the active phone is in a "device permitting" state, all media are unselected.
  callId = NO_ID_PJ;
  confId = NO_ID_PJ;
  if (!mediaOwner) {
    //~ INFOF (("### MediaUpdate (): no owner - deactivating all media"));
    mediaSelected = pmNone;
  }
  else {
    mediaSelected = mediaOwner->GetMediaSelected ();
    if (PhoneStateIsDevicePermitting (mediaOwner->GetState ())) mediaSelected = pmNone;
    //~ INFOF (("### MediaUpdate (): phone state = %i, mediaSelected = %i, mediaActivated = %i", mediaOwner->GetState (), mediaSelected, mediaActivated));

    // Try to obtain current call ID, call info & conference ID...
    callId = PHONE_LIBDATA(mediaOwner).pjCallId[0];
    if (callId != NO_ID_PJ) {
      if (PJ_SUCCESS == pjsua_call_get_info (callId, &callInfo))
        confId = pjsua_call_get_conf_port (callId);
      else callId = NO_ID_PJ;
        // This may happen due to a race. If the call has been stopped after the last run of
        // 'UpdatePhoneState()', 'PHONE_LIBDATA(mediaOwner).pjCallId[0]' may have become invalid.
    }
  }

  // Prepare new activation vector...
  _mediaActivated = mediaActivated;
  mediaToChange = mediaSelected ^ _mediaActivated;

  // Audio...
  if (mediaToChange & pmAudio) {
    if ((mediaSelected & pmAudio) == 0) {
      //~ INFO ("###    Setting  audio NULL device (disabling audio) ...");
      ASSERT_WARN (PJ_SUCCESS == pjsua_set_null_snd_dev ());
      _mediaActivated &= ~pmAudio;
    }
    else {

      // Enable device...
      //~ INFO ("###    Enabling audio device...");
      pjsua_snd_dev_param_default (&sndDevParam);
      sndDevParam.capture_dev = mediaDevAudioIn;
      sndDevParam.playback_dev = mediaDevAudioOut;
      ASSERT_WARN (PJ_SUCCESS == pjsua_set_snd_dev2 (&sndDevParam));

      // Audio in...
      if ((mediaToChange & pmAudioIn) && confId >= 0) {
        //~ INFO ("###    Setting mic level...");
        ASSERT_WARN (PJ_SUCCESS == pjsua_conf_adjust_tx_level (confId, (mediaSelected & pmAudioIn) ? envPhoneAudioInGain : 0.0));
      }

      // Audio out...
      if ((mediaToChange & pmAudioOut) && confId >= 0) {
        //~ INFO ("###    Setting speaker level...");
        ASSERT_WARN (PJ_SUCCESS == pjsua_conf_adjust_rx_level (confId, (mediaSelected & pmAudioOut) ? envPhoneAudioInGain : 0.0));
      }

      // Report success...
      _mediaActivated &= ~pmAudio;
      if (confId >= 0) _mediaActivated |= (mediaSelected & pmAudio);
    }
  }

  // Video in: Handle camera preview ...
  //   Camera preview is enabled if an only if both video in (camera) and video out (screen)
  //   are selected. This saves processing effort on tools without a display (e.g. doorman).
  if (!mediaVideoPreview && (mediaSelected & pmVideo) == pmVideo) {

    // Start local preview...
    pjsua_vid_preview_param_default (&previewParam);
    previewParam.rend_id = VideoGetDeviceIndex ();
    pjStatus = pjsua_vid_preview_start (mediaDevVideoIn, &previewParam);
    if (pjStatus == PJ_SUCCESS) {
      pjStatus = WindowAssignByID (WINDOW_SIDE, pjsua_vid_preview_get_win (mediaDevVideoIn));
      if (pjStatus != PJ_SUCCESS) pjsua_vid_preview_stop (mediaDevVideoIn);
    }
    if (pjStatus == PJ_SUCCESS)
      mediaVideoPreview = true;
    else
      WARNINGF (("Failed to start local camera preview: %s", PjStrError (pjStatus)));
  }
  else if (mediaVideoPreview && (mediaSelected & pmVideo) != pmVideo) {

    // Stop preview...
    pjStatus = pjsua_vid_preview_stop (mediaDevVideoIn);
    if (pjStatus != PJ_SUCCESS)
      WARNINGF (("Failed to stop local camera preview: %s", PjStrError (pjStatus)));
    mediaVideoPreview = false;    // mark as not running anyway
  }

  // Video in: Handle transmitted stream ...
  //~ INFOF (("###    mediaVideoTransmitting = %i, callId = %i", (int) mediaVideoTransmitting, callId));
  if (!mediaVideoTransmitting && (mediaSelected & pmVideoIn) && callId >= 0) {
    //~ INFO ("###   ... switching on video transmission.");

    // Switch on transmission...
    pjsua_call_vid_strm_op_param_default (&opParam);
    opParam.cap_dev = mediaDevVideoIn;
    pjStatus = pjsua_call_set_vid_strm (callId, PJSUA_CALL_VID_STRM_CHANGE_CAP_DEV, &opParam);
    if (pjStatus == PJ_SUCCESS)
      pjStatus = pjsua_call_set_vid_strm (callId, PJSUA_CALL_VID_STRM_START_TRANSMIT, NULL);
    if (pjStatus == PJ_SUCCESS)
      mediaVideoTransmitting = true;
    else
      WARNINGF (("Failed to start video camera transmission: %s", PjStrError (pjStatus)));
  }
  else if (mediaVideoTransmitting && !(mediaSelected & pmVideoIn)) {
    //~ INFO ("###   ... switching off video transmission.");

    // Switch off transmission ...
    //   If there is no valid callId, we assume that there is no call, and the transmission has been stopped already.
    if (callId != NO_ID_PJ) {
      pjStatus = pjsua_call_set_vid_strm (callId, PJSUA_CALL_VID_STRM_STOP_TRANSMIT, NULL);
      if (pjStatus != PJ_SUCCESS)
        WARNINGF (("Failed to stop video camera transmission: %s", PjStrError (pjStatus)));
    }
    mediaVideoTransmitting = false;    // mark as not running anyway
  }

  // Video in: Update '_mediaActivated' ...
  if (mediaVideoTransmitting || mediaVideoPreview)  _mediaActivated |= pmVideoIn;
  else                                              _mediaActivated &= ~pmVideoIn;

  // Video out (screen)...
  if (mediaToChange & pmVideoOut) {
    if ((mediaSelected & pmVideoOut) && callId >= 0) {

      // Determine window ID of incoming video...
      for (n = 0; n < (int) callInfo.media_cnt; n++) {
        if (callInfo.media[n].type == PJMEDIA_TYPE_VIDEO) { //  && (callInfo->media[n].dir == PJMEDIA_DIR_DECODING || callInfo->media[n].dir == PJMEDIA_DIR_ENCODING_DECODING)) {
          if (WindowAssignByID (WINDOW_MAIN, callInfo.media[n].stream.vid.win_in)) {
            _mediaActivated |= pmVideoOut;
            break;
          }
        }
      }
    }
    else {
      WindowAssign (WINDOW_MAIN, NULL);
      _mediaActivated &= ~pmVideoOut;
    }
  }

  // Done...
  mediaActivated = _mediaActivated;
}


static bool MediaLock (CPhone *phone) {

  // Sanity...
  if (phone == mediaOwner) return true;

  // Check availability and lock it for 'phone'...
  if (mediaOwner) return false;
  mediaOwner = phone;
  ASSERT_WARN (mediaActivated == 0);
  mediaActivated = 0;

  // Done...
  return true;
}


static void MediaUnlock (CPhone *phone) {

  // Sanity...
  if (phone != mediaOwner) {
    ASSERT_WARN (true);
    return;
  }

  // Release...
  mediaOwner = NULL;
  MediaUpdate ();
  ASSERT_WARN (mediaActivated == 0);
  mediaActivated = 0;
}





// *************************** CPhone: Media selection *************************


void CPhone::SelectMedia (unsigned selected, unsigned mask) {
  //~ INFOF (("### SelectMedia: %x -> %x", mediaSelected, (selected & mask) | (mediaSelected & ~mask)));
  mediaSelected = (selected & mask) | (mediaSelected & ~mask);
  MediaUpdate ();
}





// *************************** CPhone: Actions *********************************


// ***** General *****


bool CPhone::Dial (const char *uri) {
  pjsua_acc_info accInfo;
  pj_str_t sipUser, sipDomain, sipAccountUser, cleanUriPj;
  char *cleanUri;
  bool ok;

  // Sanity...
  if (LIBDATA.pjAccountId == NO_ID_PJ) {
    WARNING ("Unable to dial without a valid account.");
    return false;
  }
  if (LIBDATA.pjCallId[0] != NO_ID_PJ) {
    WARNING ("Unable to dial during an existing call.");
    return false;
  }

  // Cancel with error if some other phone is owning the audio/video devices...
  if (!MediaLock (this)) {
    WARNING ("CPhone::Dial () failed because some other phone is active");
    return false;
  }

  // Create clean URI...
  if (!AnalyseSipUri (pj_str ((char *) uri), &sipUser, &sipDomain)) {
    WARNINGF (("Unable to dial invalid URI: %s", uri));
    return false;
  }
  if (!sipDomain.ptr) {
    // No domain in given URI: Add the registrar's domain.
    // This is the normal case if a traditional phone number is passed.
    ASSERT (pjsua_acc_get_info (LIBDATA.pjAccountId, &accInfo) == PJ_SUCCESS);
    AnalyseSipUri (accInfo.acc_uri, &sipAccountUser, &sipDomain);
    if (!sipDomain.ptr) {
      WARNINGF (("Unable to obtain domain from account '%.*s'.", PJSTR_ARGS (accInfo.acc_uri)));
      return false;
    }
  }
  asprintf (&cleanUri, "sip:%.*s@%.*s", PJSTR_ARGS (sipUser), PJSTR_ARGS (sipDomain));

  // Dial...
  pj_cstr (&cleanUriPj, cleanUri);
  ok = (PJ_SUCCESS == pjsua_call_make_call (LIBDATA.pjAccountId, &cleanUriPj, NULL, this, NULL, NULL)); // &LIBDATA.pjCallId[0]));
    // Note (2017-08-16, PJSIP 2.6): The returned call ID (last arg #6) appears to be 0, not the one used later
    // during invitation or in the cofirmed state. In order to obtain the final call ID, we do the following:
    // - pass 'this' as 'user_data' (arg #4)
    // - in 'UpdatePhoneState': if 'user_data != NULL' and the call ID is unknown to the respective CPhone object
    //   and slot #0 is empty, the call ID is assigned to slot  #0.
  if (!ok) WARNINGF (("'pjsua_call_make_call' failed for URI '%s'", cleanUri));

  // Report state change...
  if (ok) {
    //~ INFOF (("### Inviting (%i)", LIBDATA.pjCallId[0]));
    ReportInfo (_("Inviting..."));
    ReportState (state >= psTransferIdle ? psTransferDialing : psDialing);
  }

  // Update media...
  MediaUpdate ();

  // Complete...
  free (cleanUri);
  return ok;
}


bool CPhone::AcceptCall () {
  // Only the primary call can be accepted.
  bool ok;

  // Sanity...
  if (LIBDATA.pjCallId[0] == NO_ID_PJ) {
    WARNING ("'AcceptCall' invoked without a pending incoming call");
    return false;
  }

  // Cancel with error if some other phone is owning the audio/video devices...
  if (!MediaLock (this)) {
    WARNING ("CPhone::AcceptCall () failed because some other phone is active");
    return false;
  }

  // Update media...
  MediaUpdate ();

  // Accept...
  ok = (PJ_SUCCESS == pjsua_call_answer (LIBDATA.pjCallId[0], 200, NULL, NULL));    // Accept - "OK"
  ASSERT_WARN (ok);
  return ok;
}


bool CPhone::Hangup () {
  bool ok;

  // Hangup primary call, if present...
  if (LIBDATA.pjCallId[0] != NO_ID_PJ) {
    ok = (PJ_SUCCESS == pjsua_call_hangup (LIBDATA.pjCallId[0], 0, NULL, NULL));      // Hangup (with defaults)
    ASSERT_WARN (ok);
  }

  // Else: unpause secondary call, if present...
  else if (LIBDATA.pjCallId[1] != NO_ID_PJ) {
    //~ INFOF (("### #%i = (%i), #%i = (%i)", 0, LIBDATA.pjCallId[0], 1, LIBDATA.pjCallId[1]));
    ok = (PJ_SUCCESS == pjsua_call_reinvite (LIBDATA.pjCallId[1], PJSUA_CALL_UNHOLD, NULL));
      // TBD: include 'PJSUA_CALL_INCLUDE_DISABLED_MEDIA' option? (see http://www.pjsip.org/docs/latest/pjsip/docs/html/group__PJSUA__LIB__CALL.htm)
    ASSERT_WARN (ok);
    if (ok) {
      LIBDATA.pjCallId[0] = LIBDATA.pjCallId[1];
      LIBDATA.pjCallId[1] = NO_ID_PJ;
      ReportInfo (_("Resuming paused call."));
      ReportState (psInCall);
        // We assert that the secondary call is still connected. If not, the state will be changed again
        // in 'UpdatePhoneState ()'. To trigger this check, we set the respective flag now.
      LIBDATA.check.callState = true;
    }
    //~ INFOF (("### #%i = (%i), #%i = (%i)", 0, LIBDATA.pjCallId[0], 1, LIBDATA.pjCallId[1]));
  }

  // Else: Nothing to do...
  else {
    WARNING ("'CPhone::Hangup' invoked without any active call");
    ok = false;
  }

  // Done...
  return ok;
}


bool CPhone::CancelAllCalls () {
  // We cannot use 'pjsua_call_hangup_all' here, since that would also cancel
  // the calls of other 'CPhone' objects!
  int n;
  bool ok;

  for (n = 0, ok = true; n < 2; n++)
    if (LIBDATA.pjCallId[n] != NO_ID_PJ)
      ok = ok && (PJ_SUCCESS == pjsua_call_hangup (LIBDATA.pjCallId[n], 0, NULL, NULL));
  ASSERT_WARN (ok);
  return ok;
}



// ***** DTMF *****


bool CPhone::SendDtmf (const char *dtmfSequence) {
  pj_str_t pjDtmfSeq;
  bool ok;

  pj_cstr (&pjDtmfSeq, dtmfSequence);
  if (LIBDATA.pjCallId[0] == NO_ID_PJ) return false;
  ok = (PJ_SUCCESS == pjsua_call_dial_dtmf (LIBDATA.pjCallId[0], &pjDtmfSeq));
  ASSERT_WARN (ok);
  return ok;
}



// ***** Transfers *****


bool CPhone::PrepareTransfer () {
  bool ok;

  // Sanity...
  if (state != psInCall) {
    WARNING("'CPhone::PrepareTransfer' called without connected call");
    return false;
  }
  ok = (LIBDATA.pjCallId[0] != NO_ID_PJ && LIBDATA.pjCallId[1] == NO_ID_PJ);
    // There may be very rare race conditions that may make this assertion fail (e.g. the 'pjCallId'
    // fields may have been changed before the 'state' variable has been updated away from 'psInCall').
  ASSERT_WARN (ok);
  if (!ok) return false;

  // Pause the current call...
  ok = (PJ_SUCCESS == pjsua_call_set_hold (LIBDATA.pjCallId[0], NULL));
  ASSERT_WARN (ok);
  if (!ok) return false;

  // State transition...
  LIBDATA.pjCallId[1] = LIBDATA.pjCallId[0];
  LIBDATA.pjCallId[0] = NO_ID_PJ;
  ReportInfo (_("Call is paused. Please dial the number to transfer to."));
  ReportState (psTransferIdle);
  return false;
}


bool CPhone::CompleteTransfer () {
  bool ok;

  //~ INFOF (("### CPhone::Transfer: #%i = (%i), #%i = (%i)", 0, LIBDATA.pjCallId[0], 1, LIBDATA.pjCallId[1]));

  // If destination has not yet picked up: Just enter the "auto-pickup" state...
  if (state == psTransferDialing) {
    ReportInfo (_("Pick up destination phone to complete the transfer."));
    ReportState (psTransferAutoComplete);
    return true;
  }

  // Sanity...
  if (state != psTransferInCall) {
    WARNING("'CPhone::Transfer' called without two ready calls");
    return false;
  }
  ok = (LIBDATA.pjCallId[0] != NO_ID_PJ && LIBDATA.pjCallId[1] != NO_ID_PJ);
    // There may be very rare race conditions that may make this assertion fail (e.g. the 'pjCallId'
    // fields may have been changed before the 'state' variable has been updated away from 'psTransferInCall').
  ASSERT_WARN (ok);
  if (!ok) return false;

  // Transfer the paused call to the new one...
  ok = (PJ_SUCCESS == pjsua_call_xfer_replaces (LIBDATA.pjCallId[1], LIBDATA.pjCallId[0], 0, NULL));
    // (From the PJSIP doc, on 3rd parameter 'options'):
    //   Application may specify PJSUA_XFER_NO_REQUIRE_REPLACES to suppress the inclusion
    //   of "Require: replaces" in the outgoing INVITE request created by the REFER request.
  ASSERT_WARN (ok);
  return ok;
}





// *************************** CPhone: State retrieval *************************


/* Note on state tracking & concurrency
 * ------------------------------------
 *
 * PJSIP works with various background threads, and the state of the phone engine is
 * only tracked imprecisely. To avoid errors due to race conditions, the following
 * statements hold or must be followed:
 *
 * 1. All calls to this wrapper module must be made from the same thread (called "main thread").
 *
 * 2. State changes (call/media/...) inside the PJSIP library are traced by polling
 *    the state from the main thread. The polling must be triggered, for example, by
 *    PJSIP's asynchronous callback functions or manually from the action method.
 *    Manual triggering *must* be placed if the respective event may not be accompanied
 *    by an actual PSJIP state change - for example, if just the primary & secondory
 *    calls are exchanged.
 *
 * 3. The callback/polling mechanism guarantees that no event is missed. However,
 *    the number of events is not tracked. Hence, between two invocations of
 *    'UpdatePhoneState ()', multiple events may have occured.
 *
 * 4. Given the restriction in 3., the 'CPhone' phone state is traced by a combination
 *    of the action methods (which "know" what should happen next) and the observations
 *    made in 'UpdatePhoneState ()' (which may or may not know what the only correct
 *    current state can be).
 *
 * 5. No strict assumptions can be made from 'CPhone' state for the underlying library
 *    due to concurrency. The 'CPhone::state' field is used to
 *    a) Guide the UI,
 *    b) Make decisions on the behaviour of some action methods.
 *
 * 6. a) The caller must be prepared that the action methods do not always exactly do
 *       what they are supposed to do (see 5b).
 *
 *    b) However, the 'Phone' state will after some time always reflect the correct
 *       state. (This must be guaranteed in this module!)
 *
 * 7. The following state transition may occur in 'UpdatePhoneState',
 *    section "Handle call state change":
 *       (any)   -> psIdle            (unique plausible state)
 *       (any)   -> psInCall          (unique plausible state)
 *       (any)   -> psTransferIdle    (unique plausible state)
 *       (any)   -> psTransferInCall  (unique plausible state)
 *       (psTransferDialing, psTransferAutoComplete)   -> psDialing
 *                                    (a bit complex case: secondary call was lost)
 *
 *    The following state transition may occur in 'UpdatePhoneState',
 *    section "Handle incoming call":
 *       psIdle -> psRinging
 *
 *    The following state transitions are performed elsewhere based on actions:
 *       psIdle            -> psDialing              ('Dial ()')
 *       psTransferIdle    -> psTransferDialing      ('Dial ()')
 *       psInCall          -> psTransferIdle         ('PrepareTransfer ()')
 *       psTransferDialing -> psTransferAutocomplete ('CompleteTransfer ()')
 *       (>= psTransferIdle) -> psInCall(*)          ('Hangup ()', (*) call state check must follow)
 *
 *    This information defines a state transition diagram, which the reader
 *    may want to draw for comprehension.
 */


static void UpdatePhoneState (CPhone *phone) {
  // This function does the following in this order (as applicable):
  // 1. Update 'pjCallId[0]' and 'pjCallId[1]', only this function is allowed to modify them.
  // 2. Determine and report the new phone state
  // 3. Perform other phone actions.
  TPhoneData *phoneData;
  TMgmtCheckRec _check;
  pjsua_acc_info accInfo;
  pjsua_call_info callInfo;
  pjsua_conf_port_id confPort;
  pj_str_t ringbackFilePj;
  pj_status_t pjStatus;
  EPhoneState oldPhoneState, newPhoneState;
  int n, callStatus;
  bool havePrimaryCall, primaryConfirmed, haveSecondaryCall;
  bool ok;

  //~ INFO ("### UpdatePhoneState()");

  // Copy out and acknowledge check record in a thread-safe way...
  phoneData = &PHONE_LIBDATA(phone);
  MgmtLock ();
  _check = phoneData->check;    // local copy of the previous check status
  phoneData->check.regState = phoneData->check.callState = phoneData->check.mediaState = false;
  phoneData->check.incomingCallId = NO_ID_PJ;
  MgmtUnlock ();

  // Handle registration change...
  if (_check.regState) {
    ASSERT (PJ_SUCCESS == pjsua_acc_get_info (PHONE_LIBDATA(phone).pjAccountId, &accInfo));
    phone->ReportInfo (_("Registration: %.*s (%i)"), PJSTR_ARGS (accInfo.status_text), accInfo.status);
  }

  // Handle call state change...
  if (_check.callState) {
    DumpCallInfo (_check.callId);

    // Check if this a new outgoing call...
    //   See comment in 'CPhone::Dial'.
    if (phoneData->pjCallId[0] == NO_ID_PJ && phoneData->pjCallId[1] != _check.callId)
      if (pjsua_call_get_user_data (_check.callId) == phone) {
        // This is a newly dialed call for 'phone'...
        //~ INFOF (("### newly dialed (%i) -> #0", _check.callId));
        phoneData->pjCallId[0] = _check.callId;
      }

    //~ INFOF (("### #%i = (%i), #%i = (%i)", 0, phoneData->pjCallId[0], 1, phoneData->pjCallId[1]));

    // Assign call status...
    for (n = 0; n < 2; n++) if (_check.callId == phoneData->pjCallId[n] && _check.callStatus >= 0) {
      //~ INFOF (("### storing call status (%i) = %i to slot #%i", _check.callId, _check.callStatus, n));
      phoneData->callStatus[n] = _check.callStatus;
    }

    // Check existence of a paused call (existence)...
    haveSecondaryCall = (phoneData->pjCallId[1] != NO_ID_PJ);
    if (haveSecondaryCall) {
      haveSecondaryCall = (PJ_SUCCESS == pjsua_call_get_info (phoneData->pjCallId[1], &callInfo));
      if (haveSecondaryCall) {
        //~ INFOF (("### Call #1 state: %.*s", PJSTR_ARGS (callInfo.state_text)));
        if (callInfo.state == PJSIP_INV_STATE_DISCONNECTED) haveSecondaryCall = false;
      }
      if (!haveSecondaryCall) phoneData->pjCallId[1] = NO_ID_PJ;
    }

    // Check primary call (existence & confirmed)...
    havePrimaryCall = (phoneData->pjCallId[0] != NO_ID_PJ);
    primaryConfirmed = false;
    if (havePrimaryCall) {
      havePrimaryCall = (PJ_SUCCESS == pjsua_call_get_info (phoneData->pjCallId[0], &callInfo));
      if (havePrimaryCall) {
        //~ INFOF (("Call #0 state: %.*s", PJSTR_ARGS (callInfo.state_text)));
        if (callInfo.state == PJSIP_INV_STATE_DISCONNECTED) havePrimaryCall = false;
      }
      if (!havePrimaryCall) phoneData->pjCallId[0] = NO_ID_PJ;
    }
    if (havePrimaryCall && callInfo.state == PJSIP_INV_STATE_CONFIRMED) primaryConfirmed = true;

    // Perform eventual state transition...
    //   Depending on the currently queried call infos ('havePrimaryCall', 'primaryConfirmed', 'haveSecondary')
    //   and the previous phone state, we determine an eventual state change.
    oldPhoneState = newPhoneState = phone->GetState ();
    if (!haveSecondaryCall) {

      // Case 1: No secondary call...
      if (oldPhoneState >= psTransferIdle) {
        switch (oldPhoneState) {
          case psTransferIdle:
            newPhoneState = psIdle;
            break;
          case psTransferDialing:
          case psTransferAutoComplete:
            newPhoneState = psDialing;
            break;
          case psTransferInCall:
            newPhoneState = psInCall;
            break;
          default:
            break;
        }
      }
      if (!havePrimaryCall) newPhoneState = psIdle;
      else if (primaryConfirmed) newPhoneState = psInCall;
    }
    else {

      // Case 2: We have a secondary, paused call...
      if (!havePrimaryCall) newPhoneState = psTransferIdle;
      else if (primaryConfirmed) newPhoneState = psTransferInCall;
    }
    //~ INFOF (("###   phoneState: %i -> %i", oldPhoneState, newPhoneState));

    // Report info and new state...
    if (newPhoneState != oldPhoneState) {

      // Info message...
      //~ INFO ("### reporting state change...");
      if (newPhoneState < psTransferIdle && oldPhoneState >= psTransferIdle) {
        // Paused call ended: report just that...
        MgmtLock ();
        callStatus = PHONE_LIBDATA(phone).callStatus[1];
        MgmtUnlock ();
        if (callStatus >= 0)
          phone->ReportInfo (_("Paused call ended: %.*s (%i)"), PJSTR_ARGS (*pjsip_get_status_text (callStatus)), callStatus);
        else
          phone->ReportInfo (_("Paused call ended."));
      }
      else switch (newPhoneState) {
        case psIdle:
        case psTransferIdle:
          if (newPhoneState > oldPhoneState) phone->ReportInfo (_("Ready."));
          else {
            MgmtLock ();
            callStatus = PHONE_LIBDATA(phone).callStatus[0];
            MgmtUnlock ();
            if (callStatus >= 0)
              phone->ReportInfo (_("Call ended: %.*s (%i)"), PJSTR_ARGS (*pjsip_get_status_text (callStatus)), callStatus);
            else
              phone->ReportInfo (_("Call ended."));
          }
          break;
        case psInCall:
        case psTransferInCall:
          phone->ReportInfo (_("Connected."));
          break;
        default:
          ASSERT (false);
      }

      // New state ...
      //   report the state ...
      phone->ReportState (newPhoneState);
      //   update media ...
      if (PhoneStateIsDevicePermitting (oldPhoneState) != PhoneStateIsDevicePermitting (newPhoneState))
        MediaUpdate ();
      //   unclaim the audio/video devices if possible ...
      if (newPhoneState == psIdle) MediaUnlock (phone);
    }

    // Auto-complete transfer if appropriate...
    if (newPhoneState == psTransferInCall && oldPhoneState == psTransferAutoComplete) phone->CompleteTransfer ();

    // Start/stop ringback as appropriate ...
    if (newPhoneState == psDialing || newPhoneState == psTransferDialing) {
      if (phoneData->playerId == PJSUA_INVALID_ID) {

        // Start ringback sound ...
        //~ INFOF (("### Playing ringback sound '%s' ...", envPhoneRingbackFile));
        //   create player ...
        EnvGetPath (envPhoneRingbackFileKey, &envPhoneRingbackFile);
        pj_strset2 (&ringbackFilePj, (char *) envPhoneRingbackFile);
        pjStatus = pjsua_player_create (&ringbackFilePj, 0, &(phoneData->playerId));
        if (pjStatus != PJ_SUCCESS) phoneData->playerId = PJSUA_INVALID_ID;
        //   connect to conference ...
        else {
          confPort = pjsua_player_get_conf_port (phoneData->playerId);
          //~ INFOF (("###   playerId = %i, confPort = %i", LIBDATA.playerId, confPort));
          pjStatus = pjsua_conf_connect (confPort, 0);
          if (pjStatus == PJ_SUCCESS) pjStatus = pjsua_conf_adjust_rx_level (confPort, envPhoneRingbackLevel);
        }
        //   handle error ...
        if (pjStatus != PJ_SUCCESS)
          WARNINGF (("Failed to play ringback sound '%s': %s", envPhoneRingbackFile, PjStrError (pjStatus)));
      }
    }
    else {  // not dialing ...
      if (phoneData->playerId != PJSUA_INVALID_ID) {

        // Stop ringback sound ...
        //~ INFOF (("### Stopping ringback sound '%s' (%i) ...", envPhoneRingbackFile, phoneData->playerId));
        ASSERT_WARN (PJ_SUCCESS == pjsua_player_destroy (phoneData->playerId));
          // destroy player, implicitely disconnecting it from the conference
        phoneData->playerId = PJSUA_INVALID_ID;
      }
    }

  } // if (_check.callState)

  // Handle incoming call...
  if (_check.incomingCallId != NO_ID_PJ) {
    //~ INFOF (("### incoming (%i)", _check.incomingCallId));
    if (PJ_SUCCESS == pjsua_call_get_info (_check.incomingCallId, &callInfo)) {
      phone->ReportInfo (_("%.*s is calling!"), PJSTR_ARGS (callInfo.remote_info));

      newPhoneState = phone->GetIncomingCallAction ();    // desired incoming call action
      if (phone->GetState () != psIdle || phoneData->pjCallId[0] != NO_ID_PJ) newPhoneState = psIdle;     // we are busy => must reject
      if (!MediaLock (phone)) newPhoneState = psIdle;        // another phone is using the devices => must reject
      switch (newPhoneState) {
        case psRinging:
          phoneData->pjCallId[0] = _check.incomingCallId;
          ok = (PJ_SUCCESS == pjsua_call_answer (phoneData->pjCallId[0], 180, NULL, NULL));  // Provisional - "Ringing"
          ASSERT_WARN (ok);
          if (ok) phone->ReportState (psRinging);
          break;
        case psInCall:
          phoneData->pjCallId[0] = _check.incomingCallId;
          ASSERT_WARN (PJ_SUCCESS == pjsua_call_answer (phoneData->pjCallId[0], 200, NULL, NULL));  // Accept - "OK"
          break;
        default:
          ASSERT_WARN (PJ_SUCCESS == pjsua_call_hangup (phoneData->pjCallId[0], 486, NULL, NULL));  // Decline - "Busy Here"
      }
    }
  }

  // Handle call media change...
  if (_check.mediaState) {

    // Check call media...
    if (phoneData->pjCallId[0] != NO_ID_PJ) {
      if (PJ_SUCCESS == pjsua_call_get_info (phoneData->pjCallId[0], &callInfo)) {
        if (callInfo.media_status == PJSUA_CALL_MEDIA_ACTIVE) {

          // When media is active, connect call to sound device...
          pjsua_conf_connect (callInfo.conf_slot, 0);
          pjsua_conf_connect (0, callInfo.conf_slot);

          // Update media selection (media have become selectable)...
          MediaUpdate ();
        }
      }
    }
  }
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
  pjsua_call_info callInfo;

  // Sanity...
  if (LIBDATA.pjCallId[callId] == NO_ID_PJ) return 0;

  // Go ahead...
  if (PJ_SUCCESS == pjsua_call_get_info (LIBDATA.pjCallId[callId], &callInfo))
    return (int) callInfo.connect_duration.sec;
  else
    return 0;
}


const char *CPhone::GetPeerUrl (int callId) {
  static char *buf = NULL, unknown[] = "?";
  pjsua_call_info callInfo;

  // Sanity...
  if (LIBDATA.pjCallId[callId] == NO_ID_PJ) return unknown;

  // Go ahead...
  if (PJ_SUCCESS == pjsua_call_get_info (LIBDATA.pjCallId[callId], &callInfo)) {
    if (buf) free (buf);
    buf = (char *) malloc (callInfo.remote_info.slen + 1);
    memcpy (buf, callInfo.remote_info.ptr, callInfo.remote_info.slen);
    buf[callInfo.remote_info.slen] = '\0';
    return buf;
  }
  else return unknown;
}





// *************************** CPhone: Video stream ****************************




TPhoneVideoFrame *CPhone::VideoLockFrame (int streamId) {
  TVideoStream *videoStream;

  // Lock windows...
  pthread_mutex_lock (&windowsMutex);

  // Sanity...
  if (streamId < 0 || streamId >= WINDOWS) return NULL;
  videoStream = windows[streamId];
  if (!videoStream) return NULL;

  // Go ahead...
  videoStream->phoneVideoFrame.changed = videoStream->changed;
  videoStream->changed = false;
  return videoStream->frame.buf ? &videoStream->phoneVideoFrame : NULL;
  return NULL;
}


void CPhone::VideoUnlock () {
  pthread_mutex_unlock (&windowsMutex);
}





// *************************** CPhone: Internal ********************************


void CPhone::ReportState (EPhoneState _state) {
  EPhoneState oldState;

  //~ INFOF (("### CPhone::ReportState: %i -> %i", state, _state));
  if (_state != state) {
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

  //~ INFOF (("(PJSIP-Info) %s", buf));
  OnInfo (buf);

  free (buf);
}





// *************************** CPhone: Setting up ******************************


// ***** PJSUA / PJSIP callbacks (asynchronous!) *****


static void AsyncOnLogging (int level, const char *data, int len) {
  if (level <= 3)
    DEBUGF (1, ("[PJSIP-%i] %.*s", level, len - 1, data));
  else
    DEBUGF (3, ("[PJSIP-%i] %.*s", level, len - 1, data));
}


static void AsyncOnRegState (pjsua_acc_id accId) {
  CPhone *phone;

  MgmtLock ();
  phone = MgmtPhoneOfAccount (accId);
  ASSERT_WARN (phone != NULL);
  if (phone) PHONE_LIBDATA(phone).check.regState = true;
  MgmtUnlock ();
}


static void AsyncOnIncomingCall (pjsua_acc_id accId, pjsua_call_id callId, pjsip_rx_data *) {
  CPhone *phone;

  //~ INFOF (("### AsyncOnIncomingCall (%i)", callId));

  MgmtLock ();
  phone = MgmtPhoneOfAccount (accId);
  ASSERT_WARN (phone != NULL);
  if (phone) {
    if (PHONE_LIBDATA(phone).check.incomingCallId != NO_ID_PJ)
      pjsua_call_hangup (callId, 486, NULL, NULL);
        // The last call ID was not yet polled => reject the new with 'BUSY_HERE'
    else
      PHONE_LIBDATA(phone).check.incomingCallId = callId;
  }
  MgmtUnlock ();
}


static void AsyncOnCallState (pjsua_call_id callId, pjsip_event *) {
  CPhone *phone, *identifiedPhone;
  pjsua_call_info callInfo;
  int n;
  bool ok;

  //~ INFOF (("### AsyncOnCallState (%i)", callId));

  // Determine status code...
  ok = (PJ_SUCCESS == pjsua_call_get_info (callId, &callInfo));
  ASSERT_WARN (ok);

  // Lock mgmt structures...
  MgmtLock ();

  // Try to identify phone...
  identifiedPhone = MgmtPhoneOfCall (callId);

  // Write out info to check structure...
  for (n = 0; n < mgmtPhones; n++) {
    phone = mgmtPhoneList[n];
    if (!identifiedPhone || phone == identifiedPhone) {
      PHONE_LIBDATA(phone).check.callState = true;
      PHONE_LIBDATA(phone).check.callId = callId;
      PHONE_LIBDATA(phone).check.callStatus = ok ? callInfo.last_status : -1;
    }
  }

  // Unlock mgmt structures...
  MgmtUnlock ();
}


static void AsyncOnCallMediaState (pjsua_call_id callId) {
  CPhone *phone;

  MgmtLock ();
  phone = MgmtPhoneOfCall (callId);
  ASSERT_WARN (phone != NULL);
  if (phone) PHONE_LIBDATA(phone).check.mediaState = true;
  MgmtUnlock ();
}


static void AsyncOnDtmfDigit (pjsua_call_id callId, int digit) {
  CPhone *phone;

  MgmtLock ();
  phone = MgmtPhoneOfCall (callId);
  ASSERT (phone != NULL);
  PHONE_LIBDATA(phone).check.dtmfDigit = digit;
  MgmtUnlock ();
}


static inline void PjsuaInit (const char *agentName) {
  pjsua_config pjCfg;
  pjsua_logging_config logCfg;
  pjsua_media_config mediaCfg;
  pjsua_transport_config transportCfg;

  // Create PJSUA...
  AndroidInit ();
  ASSERT (PJ_SUCCESS == pjsua_create());

  // Init PJSUA...
  pjsua_config_default (&pjCfg);
  pj_strset2 (&pjCfg.user_agent, (char *) agentName);
  pjCfg.cb.on_reg_state = &AsyncOnRegState;
  pjCfg.cb.on_incoming_call = &AsyncOnIncomingCall;
  pjCfg.cb.on_call_state = &AsyncOnCallState;
  pjCfg.cb.on_call_media_state = &AsyncOnCallMediaState;
  pjCfg.cb.on_dtmf_digit = &AsyncOnDtmfDigit;

  pjsua_logging_config_default (&logCfg);
  logCfg.level = logCfg.console_level = 6; // withLogging ? 6 : 3;
    // The 'console_level' also applies to the callback function, so both levels are set equally.
  logCfg.msg_logging = PJ_TRUE; // PJ_FALSE;    // with 'PJ_TRUE', complete protocol excerpts are printed.
  logCfg.cb = AsyncOnLogging;

  pjsua_media_config_default (&mediaCfg);

  ASSERT (pjsua_init (&pjCfg, &logCfg, &mediaCfg) == PJ_SUCCESS);

  AlsaInit ();

  // Register video driver,,,
  ASSERT (PJ_SUCCESS == pjmedia_vid_register_factory (VideoFactoryCreateFunc, NULL));
    // Video devices must be selected in the account settings.

  // Add UDP transport...
  pjsua_transport_config_default (&transportCfg);
  transportCfg.port = envPhoneSipPort;
  ASSERT (PJ_SUCCESS == pjsua_transport_create (PJSIP_TRANSPORT_UDP, &transportCfg, NULL));

  // Initialization is done, now start PJSUA...
  ASSERT (PJ_SUCCESS == pjsua_start ());

  // Init media parameters ...
  MediaSetup ();
}


static inline void PjsuaDone () {
  pjsua_destroy();
}


void CPhone::Setup (const char *agentName, int _mediaSelected, int withLogging, const char *tmpDir) {

  // Reset if already setup...
  if (LIBDATA.isSet) Done ();

  // If this is the first phone: Initialize PJSUA...
  MgmtLock ();
  if (mgmtPhones == 0) {
    MgmtAddPhone (this);
    MgmtUnlock ();
    PjsuaInit (agentName);
    MediaDumpDevicesAndCodecs ();
  }
  else MgmtUnlock ();

  // Complete...
  LIBDATA.isSet = true;
  ReportState (psIdle);

  // Set selected media...
  mediaSelected = _mediaSelected;
  MediaUpdate ();
}


bool CPhone::Register (const char *identity, const char *secret) {
  /* Example (from "Simple PJSUA"):
   *    #define SIP_DOMAIN "example.com"
   *    #define SIP_USER "alice"
   *    #define SIP_PASSWD "secret"
   *
   *    pjsua_acc_config cfg;
   *
   *    pjsua_acc_config_default(&cfg);
   *    cfg.id = pj_str("sip:" SIP_USER "@" SIP_DOMAIN);
   *    cfg.reg_uri = pj_str("sip:" SIP_DOMAIN);
   *    cfg.cred_count = 1;
   *    cfg.cred_info[0].realm = pj_str(SIP_DOMAIN);
   *    cfg.cred_info[0].scheme = pj_str("digest");
   *    cfg.cred_info[0].username = pj_str(SIP_USER);
   *    cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
   *    cfg.cred_info[0].data = pj_str(SIP_PASSWD);
   *
   *    status = pjsua_acc_add(&cfg, PJ_TRUE, &acc_id);
   *    if (status != PJ_SUCCESS) error_exit("Error adding account", status);
   */
  pjsua_acc_config pjAccCfg;
  pj_str_t identityPj, sipUser, sipDomain;
  char *bufRegUri;
  bool ok;

  // Sanity and default registration arguments ...
  if (!identity) identity = envPhoneRegister;
  if (!secret) secret = envPhoneSecret;
  if (!identity || !secret) {
    WARNING ("Missing registration information (identity or password): Not registering phone.");
    return false;
  }

  // Interpret 'identity' (extract domain & user)...
  pj_cstr (&identityPj, identity);
  ok = AnalyseSipUri (identityPj, &sipUser, &sipDomain);
  if (!sipDomain.ptr) ok = false;
  if (!ok) WARNINGF (("Malformed SIP identity: %s", identity));

  // Do the registration...
  if (ok) {
    asprintf (&bufRegUri, "sip:%.*s", PJSTR_ARGS (sipDomain));

    ReportInfo (_("Registration in progress (%.*s@%.*s) ..."), PJSTR_ARGS (sipUser), PJSTR_ARGS (sipDomain));

    // Account configuration...
    //   ... general ...
    pjsua_acc_config_default (&pjAccCfg);
    pjAccCfg.id = pj_str ((char *) identity);
    pjAccCfg.reg_uri = pj_str (bufRegUri);

    pjAccCfg.cred_count = 1;
    pjAccCfg.cred_info[0].realm = pj_str ((char *) "*");
    pjAccCfg.cred_info[0].scheme = pj_str ((char *) "digest");
    pjAccCfg.cred_info[0].username = sipUser;
    pjAccCfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    pjAccCfg.cred_info[0].data = pj_str ((char *) secret);

    //   ... video settings ...
    pjAccCfg.vid_cap_dev = PJMEDIA_VID_DEFAULT_CAPTURE_DEV;
    pjAccCfg.vid_rend_dev = VideoGetDeviceIndex ();
    pjAccCfg.vid_in_auto_show = PJ_TRUE;
    pjAccCfg.vid_out_auto_transmit = PJ_FALSE;    // disable auto transmit by default

    if (pjsua_acc_add (&pjAccCfg, PJ_TRUE, &LIBDATA.pjAccountId) != PJ_SUCCESS) {
      WARNINGF (("PJSIP: pjsua_acc_add() failed for identity '%s'.", identity));
      ok = false;
    }

    free (bufRegUri);
  }

  // Done...
  return ok;
}





// *************************** CPhone: Init/Done/Iterate ***********************


void CPhone::Init () {

  // Set fail-safe fields...
  state = psNone;
  incomingAction = psRinging;

  cbPhoneStateChanged = NULL;
  cbInfo = NULL;
  cbPhoneStateChangedData = cbInfoData = NULL;

  PhoneVideoFrameInit (&picInfo);

  LIBDATA.haveAccount = false;
  LIBDATA.pjAccountId = NO_ID_PJ;
  LIBDATA.pjCallId[0] = LIBDATA.pjCallId[1] = NO_ID_PJ;
  LIBDATA.playerId = PJSUA_INVALID_ID;
  LIBDATA.tLastStatusLog = NEVER;

  LIBDATA.isSet = false;   // will be set later in 'Setup'

  MgmtCheckRecClear (&LIBDATA.check);
}


void CPhone::Done () {
  if (LIBDATA.isSet) {
    CancelAllCalls ();
    ASSERT_WARN (PJ_SUCCESS == pjsua_acc_del (LIBDATA.pjAccountId));
    LIBDATA.isSet = false;

    // If this was the last open phone: Shutdown PJSUA...
    MgmtLock ();
    MgmtDelPhone (this);
    if (mgmtPhones == 0) {
      MgmtUnlock ();
      PjsuaDone ();
    }
    else MgmtUnlock ();
  }
}


void CPhone::Iterate () {
  TTicks tNow;
  TMgmtCheckRec *check = &LIBDATA.check;
  char dtmfChar;

  // Note: We do (cheap) read-only pre-checks here to avoid locking the mgmt structures.

  // Any kind of phone state...
  if (check->regState || check->incomingCallId != NO_ID_PJ || check->callState || check->mediaState)
    UpdatePhoneState (this);

  // DTMF received...
  if (check->dtmfDigit != -1) {
    MgmtLock ();
    dtmfChar = (char) check->dtmfDigit;
    check->dtmfDigit = -1;
    MgmtUnlock ();
    OnDtmfReceived (dtmfChar);
  }

  // Log statistics ...
  if (envDebug >= 1) {
    tNow = TicksNowMonotonic ();
    if (tNow > LIBDATA.tLastStatusLog + TICKS_FROM_SECONDS (5)) {
      pjmedia_echo_stat ecStat;
      pj_status_t pjStatus;
      char callStat[4096];
      int callId;

      callId = LIBDATA.pjCallId[0];
      if (callId >= 0) {
        pjStatus = pjsua_call_dump (LIBDATA.pjCallId[0], true, callStat, sizeof (callStat), "  ");
        if (pjStatus != PJ_SUCCESS)
          DEBUGF (1, ("Failed to retrieve call statistics: %s", PjStrError (pjStatus)));
        else
          DEBUGF (1, ("Call statistics:\n%s", callStat));

        pjStatus = pjsua_get_ec_stat (&ecStat);
        if (pjStatus != PJ_SUCCESS)
          DEBUGF (1, ("No echo canceller statistics available: %s", PjStrError (pjStatus)));
        else
          DEBUGF (1, ("Echo canceller (%s): %s\n", ecStat.name, ecStat.stat_info));
      }

      LIBDATA.tLastStatusLog = tNow;
    }
  }
}
