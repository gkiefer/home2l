/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2015-2024 Gundolf Kiefer
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


#include "streamer.H"

#if WITH_GSTREAMER == 1

#include <gst/gst.h>



// *****************************************************************************
// *                                                                           *
// *                          GStreamer interface                              *
// *                                                                           *
// *****************************************************************************

/*
 * Notes
 * -----
 *
 * - Command example / Pipeline description:
 *      gst-launch-1.0 playbin uri=http://lennon:8000 buffer-duration=500000000 volume=2.0
 *
 * - MPD http plugin:
 *    - problems with FLAC (MPD blocks after one gst logoff)
 *    - lame: CPU load lennon ~50%
 *    - wave: CPU load lennon ~10%; MPDroid@mobile works in O2Stu ("24 MBits/sec")
 *
 * - Good example code:
 *      https://gstreamer.freedesktop.org/documentation/tutorials/basic/streaming.html
 *      "A network-resilient example", basic-tutorial-12.c
 *
 * - Android: Audio sink is 'openslessink'
 *       https://github.com/GStreamer/gst-plugins-bad/blob/master/sys/opensles/openslessink.c
 *       https://gstreamer.freedesktop.org/documentation/tutorials/basic/platform-specific-elements.html
 */


static bool gstInitialized;

static EStreamerState stateStreamer;
static CString stateError;

static GstElement *gstPipeline = NULL;
static GstBus *gstBus = NULL;
static gulong gSignalHandler = 0;

static bool dbLevelActivated;
static int dbLevel;



static void ReportError (const char *msg) {
  WARNINGF (("GStreamer error: %s", msg));
  if (stateError.IsEmpty ()) stateError.Set (msg);      // in a sequence of errors, report the first one
  stateStreamer = strError;
}


static void ReportGError (GError **err) {
  if (*err) {
    ReportError ((*err)->message);
    g_error_free (*err);
    *err = NULL;
  }
}





// *************************** Pipeline callback *******************************

/* NOTE: As of GLib 2.50 (Debian Stretch, 2018-01-22), it appears that the signal callback
 *       is always invoked from 'g_main_context_iteration' and thus from the main UI
 *       thread via 'StreamerIterate' as requested by the interface of this module.
 *       If this does not hold any more, a mutex must be added for all variables
 *       accessed by this function.
 */


static void CbGstMessage (GstBus *bus, GstMessage *msg, void *) {
  GError *gError = NULL;
  gchar *debug;
  const gchar *name;
  gint percent;
  const GstStructure *gstStructure;
  gdouble rms_db;
  const GValue *arrayVal, *value;
  GValueArray *rmsArray;
  int n, channels;

  //~ INFO ("### CbGstMessage ()");
  switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_ERROR:
      gst_message_parse_error (msg, &gError, &debug);
      ReportGError (&gError);
      g_free (debug);

      gst_element_set_state (gstPipeline, GST_STATE_READY);
      break;

    case GST_MESSAGE_EOS:      // end-of-stream
      gst_element_set_state (gstPipeline, GST_STATE_READY);
      stateStreamer = strOff;
      break;

    case GST_MESSAGE_BUFFERING:
      gst_message_parse_buffering (msg, &percent);
      DEBUGF (3, ("[GStreamer] Buffering (%3d%%)", percent));
      if (percent < 100) {
        stateStreamer = strBusy;
        gst_element_set_state (gstPipeline, GST_STATE_PAUSED);
      }
      else {
        gst_element_set_state (gstPipeline, GST_STATE_PLAYING);
        stateStreamer = strOn;
      }
      break;

    case GST_MESSAGE_CLOCK_LOST:
      // Get a new clock...
      gst_element_set_state (gstPipeline, GST_STATE_PAUSED);
      gst_element_set_state (gstPipeline, GST_STATE_PLAYING);
      break;

    case GST_MESSAGE_ELEMENT:
      gstStructure = gst_message_get_structure (msg);
      name = gst_structure_get_name (gstStructure);
      //~ INFOF (("###   GST_MESSAGE_ELEMENT: name = '%s'", name));
      if (strcmp (name, "level") == 0) {

        /* the values are packed into GValueArrays with the value per channel */
        arrayVal = gst_structure_get_value (gstStructure, "rms");
        rmsArray = (GValueArray *) g_value_get_boxed (arrayVal);

        /* we can get the number of channels as the length of any of the value
         * arrays */
        channels = rmsArray->n_values;
        rms_db = 0.0;
        for (n = 0; n < channels; n++) {
          value = &rmsArray->values[n];
          rms_db += g_value_get_double (value);
        }
        rms_db /= channels;
        dbLevel = int (rms_db);
        //~ INFOF (("### streamer: dbLevel = %i", dbLevel));
      }
      break;

    default:
      // (Unhandled message)
      break;
  }
}





// *************************** Interface functions *****************************


/*
static void GstLog (GstDebugCategory * category,
                    GstDebugLevel      level,
                    const gchar      * file,
                    const gchar      * function,
                    gint               line,
                    GObject          * object,
                    GstDebugMessage  * message,
                    gpointer           data) {
  if (level <= gst_debug_category_get_threshold (category)) {
    DEBUGF (3, ("[Gstreamer] GST: %s,%s: %s", file, function, gst_debug_message_get (message)));
  }
}
*/


void StreamerInit () {
  static bool streamerInitialized = false;
  GError *gError = NULL;

  DEBUGF (1, ("StreamerInit ()"));

  // Sanity...
  if (streamerInitialized) return;
  streamerInitialized = true;

  // Init variables...
  stateStreamer = strOff;

  // Initialize GStreamer...
  gstInitialized = gst_init_check (NULL, NULL, &gError);
  //~ gst_debug_set_default_threshold (GST_LEVEL_INFO /* GST_LEVEL_DEBUG */);
  //~ gst_debug_add_log_function (&GstLog, NULL);
  ReportGError (&gError);
}


void StreamerDone () {
  DEBUGF (1, ("StreamerDone ()"));
  StreamerStop ();
  gst_deinit ();
}


void StreamerStart (const char *host, int port, TTicks bufferDuration) {
  static CString launchStr;
  GstStateChangeReturn ret;
  GError *gError = NULL;

  // Sanity...
  if (!gstInitialized) return;
  StreamerStop ();

  DEBUGF (1, ("StreamerStart (%s:%i, %i)", host, port, (int) bufferDuration));

  // Report state...
  dbLevelActivated = false;
  dbLevel = STREAMER_LEVEL_UNKNOWN;
  stateStreamer = strBusy;

  // Build the pipeline ...
  launchStr.SetF ("playbin uri=http://%s:%i buffer-duration=%i000000 flags=audio+download audio-filter=level", host, port, bufferDuration);
    // Options to 'playbin' (see https://gstreamer.freedesktop.org/documentation/playback/playbin.html):
    // * flags
    //   - 'audio' enables audio (and disables anything else), also soft volume (the device buttons should be used)
    //   - 'download' enables download
  //~ INFOF (("### launchStr = '%s'", launchStr.Get ()));
  gstPipeline = gst_parse_launch (launchStr, &gError);
  ReportGError (&gError);
  if (!gstPipeline) return;

  gstBus = gst_element_get_bus (gstPipeline);
  ASSERT (gstBus != NULL);

  // Start playing ...
  ret = gst_element_set_state (gstPipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {

    // Report error...
    ReportError ("Unable to set the pipeline to the playing state");
      // like GStreamer-generated messages, this string is not translated.

  } else {

    // Connect bus & signal...
    gst_bus_add_signal_watch (gstBus);
    gSignalHandler = g_signal_connect (gstBus, "message", G_CALLBACK (CbGstMessage), NULL);
  }

  //~ StreamerGetDbLevel ();  // DEBUG
}


void StreamerStop () {
  DEBUGF (1, ("StreamerStop ()"));

  // Sanity...
  if (!gstInitialized || !gstPipeline) return;

  // Report "off" state...
  dbLevelActivated = false;
  dbLevel = STREAMER_LEVEL_UNKNOWN;
  stateStreamer = strOff;

  // Stop pipeline...
  gst_element_set_state (gstPipeline, GST_STATE_NULL);

  // Free resources ...
  g_signal_handler_disconnect (gstBus, gSignalHandler);
  gst_object_unref (gstBus);
  gstBus = NULL;
  gst_object_unref (gstPipeline);
  gstPipeline = NULL;
}


EStreamerState StreamerIterate () {
  if (StreamerStateIsActive (stateStreamer)) {
    //~ INFO (("### g_main_context_iteration ()"));
    while (g_main_context_iteration (NULL, false));
      // 'NULL' : use main/default context
      // 'false': do not block
  }
  return stateStreamer;
}


EStreamerState StreamerState () {
  return stateStreamer;
}


const char *StreamerGetError (CString *s) {
  s->SetO (stateError.Disown ());
  return s->Get ();
}


int StreamerGetDbLevel () {
  //~ GstElement *level;

  //~ // Activate level filter if not yet done...
  //~ INFOF (("StreamerGetDbLevel (): dbLevelActivated = %i, gstPipeline = 0x%08x", (int) dbLevelActivated, (unsigned) gstPipeline));
  //~ if (!dbLevelActivated && gstPipeline != NULL) {
    //~ INFO ("### make level component...");
    //~ level = gst_element_factory_make ("level", NULL);
    //~ if (!level) WARNING ("StreamerGetDbLevel (): Unable to activate GStreamer 'level' plugin");
    //~ else {
      //~ g_object_set (G_OBJECT (level), "post-messages", true, NULL);
      //~ g_object_set (G_OBJECT (level), "interval", (guint64) 200000000, NULL);

      //~ g_object_set (G_OBJECT (gstPipeline), "audio-filter", level, NULL);

    //~ }
    //~ dbLevelActivated = true;
    //~ ASSERT (level != NULL);   // DEBUG
  //~ }

  // return value...
  //~ INFOF (("### dbLevel = %i", dbLevel));
  return stateStreamer == strOn ? dbLevel : STREAMER_LEVEL_UNKNOWN;
}





// *************************** GStreamer disabled ******************************


#else // WITH_GSTREAMER == 1


static EStreamerState stateStreamer = strOff;
static CString stateError;


void StreamerInit () {}
void StreamerDone () {}


void StreamerStart (const char *host, int port, TTicks bufferDuration) {
  stateStreamer = strError;
  stateError.SetC ("Compiled without streaming support");
    // like GStreamer-generated messages, this string is not translated.
}


void StreamerStop () {
  stateStreamer = strOff;
}


EStreamerState StreamerIterate () {
  return stateStreamer;
}


EStreamerState StreamerState () {
  return stateStreamer;
}


const char *StreamerGetError (CString *s) {
  s->SetC (stateError.Get ());
  stateError.Clear ();
  return s->Get ();
}


int StreamerGetDbLevel () {
  return STREAMER_LEVEL_UNKNOWN;
}


#endif // WITH_GSTREAMER == 1
