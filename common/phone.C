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





// *************************** Environment Parameters **************************


ENV_PARA_PATH ("phone.linphonerc", envPhoneLinphonerc, NULL);
  /* Linphone RC file (Linphone backend only)
   *
   * With the Linphone backend, some of the following settings are configured
   * in a separate (custom) Linphone RC file. This is specified here.
   */

ENV_PARA_INT ("phone.sip.port", envPhoneSipPort, 5060);
  /* Port to use for the SIP protocol (PJSIP backend only)
   */

ENV_PARA_STRING ("phone.audio.driver", envPhoneAudioDriver, ANDROID ? "" : "ALSA");
  /* Audio driver of the selected audio input and output devices (PJSIP backend only)
   *
   * A list of possible drivers names on the current machine is logged as info
   * messages headed "Devices and Codecs" when starting the tool.
   *
   * Usually, it is not necessary to change this parameter manually.
   */

ENV_PARA_STRING ("phone.audio.device", envPhoneAudioDevice, NULL);
  /* Audio device (PJSIP backend only)
   *
   * A list of possible device names on the current machine is logged as info
   * messages headed "Devices and Codecs" when starting the tool.
   *
   * By default, the PJSIP default device is used.
   */

ENV_PARA_STRING ("phone.audio.in.device", envPhoneAudioInDevice, NULL);
  /* Audio microphone device (PJSIP backend only)
   *
   * If set, this overrides the \refenv{phone.audio.device} setting for the
   * audio input (microphone).
   *
   * \textbf{Note:} With PJSIP~2.11, echo cancellation appearantly does not work properly
   * if the input and output devices are different.
   */

ENV_PARA_STRING ("phone.audio.out.device", envPhoneAudioOutDevice, NULL);
  /* Audio speaker device (PJSIP backend only)
   *
   * If set, this overrides the \refenv{phone.audio.device} setting for the
   * audio output (speaker).
   *
   * \textbf{Note:} With PJSIP~2.11, echo cancellation appearantly does not work properly
   * if the input and output devices are different.
   */

ENV_PARA_FLOAT ("phone.audio.in.gain", envPhoneAudioInGain, 1.0);
  /* Audio microphone amplification (PJSIP backend only)
   *
   * Amplification factor for the microphone input.
   */

ENV_PARA_FLOAT ("phone.audio.out.gain", envPhoneAudioOutGain, 1.0);
  /* Audio speaker amplification (PJSIP backend only)
   *
   * Amplification factor for the speaker output.
   */


ENV_PARA_STRING ("phone.audio.codec", envPhoneAudioCodec, NULL);
  /* Preferred audio codec (PJSIP backend only)
   *
   * A list of possible codec names on the current machine is logged as info
   * messages headed "Devices and Codecs" when starting the tool.
   *
   * By default, the PJSIP default is used.
   */

ENV_PARA_INT ("phone.echo.tail", envPhoneEchoTail, -1);
  /* Acoustic echo cancellation tail length [ms] (PJSIP backend only)
   *
   * Tail length in miliseconds for the echo cancellation algorithm.
   *
   * By default or if set <0, the PJSIP default is used.
   */

ENV_PARA_INT ("phone.echo.algo", envPhoneEchoAlgo, -1);
  /* Acoustic echo cancellation algorithm (PJSIP backend only)
   *
   * Possible values are:
   * \begin{itemize}
   *   \item[<0:] Use the PJSIP default.
   *   \item[0:] Simple echo suppressor.
   *   \item[1:] Speex AEC.
   *   \item[2:] WebRTC AEC.
   * \end{itemize}
   *
   * Details can be found in the PJSIP documentation ('enum pjmedia\_echo\_flag').
   */

ENV_PARA_INT ("phone.echo.aggressiveness", envPhoneEchoAggressiveness, -1);
  /* Acoustic echo cancellation aggressiveness (PJSIP backend only)
   *
   * Possible values are:
   * \begin{itemize}
   *   \item[<0:] Use the PJSIP default.
   *   \item[0:] Conservative.
   *   \item[1:] Moderate.
   *   \item[2:] Aggressive.
   * \end{itemize}
   *
   * Details can be found in the PJSIP documentation ('enum pjmedia\_echo\_flag').
   */

ENV_PARA_BOOL ("phone.echo.denoise", envPhoneEchoNoiseSuppression, true);
  /* Enable noise suppression with echo cancellation (PJSIP backend only)
   *
   * Details can be found in the PJSIP documentation ('enum pjmedia\_echo\_flag').
   */

ENV_PARA_STRING ("phone.video.driver", envPhoneVideoDriver, ANDROID ? "Android" : "v4l");
  /* Video driver of the selected video capture device (PJSIP backend only)
   *
   * A list of possible drivers names on the current machine is logged as info
   * messages headed "Devices and Codecs" when starting the tool.
   *
   * Usually, it is not necessary to change this parameter manually.
   */

ENV_PARA_STRING ("phone.video.device", envPhoneVideoDevice, NULL);
  /* Video capture device (PJSIP backend only)
   *
   * A list of possible device names on the current machine is logged as info
   * messages headed "Devices and Codecs" when starting the tool.
   *
   * By default, the PJSIP default device is used.
   */

ENV_PARA_STRING ("phone.video.codec", envPhoneVideoCodec, NULL);
  /* Preferred video codec (PJSIP backend only)
   *
   * A list of possible codec names on the current machine is logged as info
   * messages headed "Devices and Codecs" when starting the tool.
   *
   * By default, the PJSIP default is used.
   */


ENV_PARA_STRING ("phone.register", envPhoneRegister, NULL);
  /* Phone registration string
   */
ENV_PARA_STRING ("phone.secret", envPhoneSecret, NULL);
  /* Phone registration password
   */


ENV_PARA_PATH ("phone.ringback", envPhoneRingbackFile, "share/sounds/ringback.wav");
  /* Ringback audio file
   *
   * This is the sound to be played to the caller while ringing.
   * It must be a WAV file formatted as 16 bit PCM mono/single channel.
   */

ENV_PARA_FLOAT ("phone.ringback.level", envPhoneRingbackLevel, 1.0);
  /* Ringback level adjustment
   *
   * This allows to adjust the volume of the ringback sound.
   */

ENV_PARA_INT ("phone.rotation", envPhoneRotation, 0);
  /* Phone video camera rotation in degree clockwise
   *
   * This allows to correct the camera orientation.
   * Legal values are 0, 90, 180, and 270.
   */





// *************************** Functions ***************************************


const char *StrPhoneVideoFormat (EPhoneVideoFormat x) {
  switch (x) {
    case pvfABGR8888: return "ABGR8888";
    case pvfBGR24:    return "BGR24";
    case pvfARGB8888: return "ARGB8888";
    case pvfRGB24:    return "RGB24";
    case pvfYUY2:     return "YUY2";
    case pvfUYVY:     return "UYVY";
    case pvfYVYU:     return "YVYU";
    case pvfIYUV:     return "IYUV";
    case pvfYV12:     return "YV12";
    default: break;
  }
  return "(unknown)";
}
