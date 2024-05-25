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


#ifndef _UI_BASE_
#define _UI_BASE_


/** @file
 *
 * @defgroup ui UI Toolkit
 * @brief Lightweight UI toolkit based on *SDL2*.
 * @{
 *
 * @defgroup ui_base Base
 * @brief Basic types and functionalities.
 * @nosubgrouping
 * @{
 */


#include <SDL.h>
#include <SDL_ttf.h>

#include "env.H"





// *************************** Screen resolution *******************************


#define UI_RES_X 1024   ///< UI resolution in pixels (X)
#define UI_RES_Y 600    ///< UI resolution in pixels (Y)





// *************************** General *****************************************


/// @name General functions ...
/// @{


extern SDL_Renderer *uiSdlRenderer;   ///< @private Use 'UiGetSdlRenderer ()' instead.


void UiInit (const char *windowTitle = "Home2L");
void UiDone ();

static inline SDL_Renderer *UiGetSdlRenderer () { return uiSdlRenderer; }

void UiGetWindowSize (int *w, int *h);

#if ANDROID == 0

void UiSetWindowSize (int w, int h);

bool UiGetWindowFullScreen ();
void UiSetWindowFullScreen (bool _fullScreen);
void UiToggleWindowFullScreen ();

bool UiGetWindowResizable ();
void UiSetWindowResizable (bool _resizable);
void UiToggleWindowResizable ();


#endif // ANDROID == 0


/// @}





// *************************** Events ******************************************


/// @name Events ...
/// @{


enum EUserEvent {
  evMainThreadCallback = 0,
  evSystemModeChanged,    // (int) data1 = new mode, (int) data2 = previous mode
  evEND
};


void UiIterate (bool noWait = false);
  /**< Iterate the application's main event loop:
   * - Check for SDL events and call 'HandleEvent' for the active screen on new events
   * - Check timers and execute their callbacks
   * - Redraw UI if changes are pending
   * - Do some housekeeping related to window mapping, pause/resume in Android, ...
   * - On a long push, generate an SDL_MOUSEBUTTONDOWN event with 'clicks == 2'
   *
   * Execution may take a longer period of time if a) no events occur (sleep mode) and 'noWait == false'
   * or b) some event handler calls 'CScreen::Run' for a new screen.
   */
static inline void UiIterateNoWait () { UiIterate (true); }
  ///< @brief Iterate the application's main event loop; Does not sleep, but returns
  /// immediately ("yield"-functionality for longer running processes in the UI thread).


void UiQuit ();       ///< @brief Quit the application
bool UiIsClosed ();   ///< @brief Is the application quitting?

void UiPushUserEvent (EUserEvent code, void *data1 = NULL, void *data2 = NULL);

static inline void MainThreadCallback (void (*func) (void *), void *data = NULL) { UiPushUserEvent (evMainThreadCallback, (void *) func, data); }
  ///< @brief Request function 'func' to be called by main thread at next occasion


/// @}





// *************************** SDL audio ***************************************


/// @name SDL audio ...
///
/// Audio playing functionality.
/// The following functions are not thread-safe and must be called from the main thread,
/// which is the one executing timers.
///
/// @{


#define AUDIO_FOREVER -1    ///< Constant to specify an infinite number of repetitions.


void AudioStart (const char *fileName, int repetitions = 1, TTicks repetitionGap = 0);
  ///< @brief Start playing audio.
  ///
  /// The contents of the audio file are enqueued in a non-blocking way for the given number
  /// of repetitions and a playback gap of the given time (if applicable).
  /// If 'repetitions == AUDIO_FOREVER', the audio is repeated forever or until stopped.
void AudioStop ();
  ///< @brief Stop playing audio immediately.

bool AudioIsPlaying ();
  ///< @brief Query if audio is still playing.
  /// If 'false' is returned, the audio device is surely in a stopped state.


/// @}





// *************************** SDL_Color helpers *******************************


/// @name Color helpers ...
/// @{


#define SELECTED_SDL_PIXELFORMAT SDL_PIXELFORMAT_ARGB8888
  ///< @brief Selected (SDL) pixel format.
  ///
  /// We define a selected pixel format, on which all higher-level routines can rely on for
  /// efficient direct pixel manipulations. All functions in this module (i.e. 'Image*' and 'Font*')
  /// always return surfaces in the correct format. Textures are always created with that format,
  /// or the texture creation fails completely.
  ///
  /// The format must be selected carefully to match the underlying hardware architecture for
  /// performance reasons.


#if SELECTED_SDL_PIXELFORMAT==SDL_PIXELFORMAT_ARGB8888


struct TColor {
  Uint8 b : 8;
  Uint8 g : 8;
  Uint8 r : 8;
  Uint8 a : 8;
};  //  __attribute__((packed))__


#define COL_MASK_R 0x00FF0000     ///< @internal
#define COL_MASK_G 0x0000FF00     ///< @internal
#define COL_MASK_B 0x000000FF     ///< @internal
#define COL_MASK_A 0xFF000000     ///< @internal
#define COL_MASK_RGB (COL_MASK_R | COL_MASK_G | COL_MASK_B)     ///< @internal


static inline TColor ToColor (Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) {
  TColor ret;
  ret.r = r; ret.g = g; ret.b = b; ret.a = a;
  return ret;
}


static inline TColor ToColor (Uint32 val) {
  union { Uint32 i; TColor c; } ret;
  ret.i = val;
  return ret.c;
  //TColor ret;
  //memcpy (&ret, &val, sizeof (TColor));
  //return ret;
}


static inline Uint32 ToUint32 (TColor color) {
  union { Uint32 i; TColor c; } ret;
  ret.c = color;
  return ret.i;
}


static inline Uint32 ToUint32 (Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) {
  return ToUint32 (ToColor (r, g, b, a));
}


static inline TColor ToColor (SDL_Color sdlCol) {
  TColor ret;
  ret.r = sdlCol.r; ret.g = sdlCol.g; ret.b = sdlCol.b; ret.a = sdlCol.a;
  return ret;
}


static inline bool operator == (const TColor &col1, const TColor &col2) { return ToUint32 (col1) == ToUint32 (col2); }
static inline bool operator != (const TColor &col1, const TColor &col2) { return ToUint32 (col1) != ToUint32 (col2); }


static inline SDL_Color ToSDL_Color (TColor color) {
  SDL_Color ret = { .r = color.r, .g = color.g, .b = color.b, .a = color.a };
  return ret;
}


#endif


TColor ColorSum (TColor col1, TColor col2);
  ///< @brief Add two colors (with saturation).
TColor ColorBrighter (TColor color, int d);
  ///< @brief Increase all color components by `d` with saturation; `d` may be negative.
static inline TColor ColorDarker (TColor color, int d) { return ColorBrighter (color, -d); }
  ///< @brief Decreas all color components by `d` with saturation; `d` may be negative.
TColor ColorScale (TColor color, int factor);
  ///< @brief Multiply all components with a factor. 'factor' is a fixed point number with 8 fractional bits (1.0 == 0x100).
TColor ColorBlend (TColor color0, TColor color1, int weight1);
  ///< @brief Blend two colors; 'weight' is a fixed point number with 8 fractional bits (1.0 == 0x100) and refers to 'color1'.


/// @}


/// @name Color palette ...
/// @{

#define WHITE         ToColor (0xff, 0xff, 0xff)
#define LIGHT_GREY    ToColor (0xc0, 0xc0, 0xc0)
#define GREY          ToColor (0x80, 0x80, 0x80)
#define DARK_GREY     ToColor (0x40, 0x40, 0x40)
#define DARK_DARK_GREY ToColor (0x20, 0x20, 0x20)
#define BLACK         ToColor (0x00, 0x00, 0x00)

#define RED           ToColor (0xff, 0x00, 0x00)
#define GREEN         ToColor (0x00, 0xff, 0x00)
#define BLUE          ToColor (0x00, 0x00, 0xff)
#define CYAN          ToColor (0x00, 0xff, 0xff)
#define MAGENTA       ToColor (0xff, 0x00, 0xff)
#define YELLOW        ToColor (0xff, 0xff, 0x00)

#define DARK_RED      ToColor (0x80, 0x00, 0x00)
#define DARK_GREEN    ToColor (0x00, 0x80, 0x00)
#define DARK_BLUE     ToColor (0x00, 0x00, 0x80)
#define DARK_CYAN     ToColor (0x00, 0x80, 0x80)
#define DARK_MAGENTA  ToColor (0x80, 0x00, 0x80)
#define DARK_YELLOW   ToColor (0x80, 0x80, 0x00)

#define LIGHT_RED     ToColor (0xff, 0x80, 0x80)
#define LIGHT_GREEN   ToColor (0x80, 0xff, 0x80)
#define LIGHT_BLUE    ToColor (0x80, 0x80, 0xff)
#define LIGHT_CYAN    ToColor (0x80, 0xff, 0xff)
#define LIGHT_MAGENTA ToColor (0xff, 0x80, 0xff)
#define LIGHT_YELLOW  ToColor (0xff, 0xff, 0x80)

#define BROWN         ToColor (0x80, 0x40, 0x00)
#define LIGHT_BROWN   ToColor (0xc0, 0x80, 0x40)

#define ORANGE        ToColor (0xc0, 0x40, 0x00)

#define TRANSPARENT   ToColor (0x00, 0x00, 0x00, 0x00)

/// @}





// *************************** SDL_Rect helpers ********************************


/// @name Geometry helpers ...
/// @{


static inline SDL_Rect Rect (int x, int y, int w, int h) {
  SDL_Rect ret = { .x = x, .y = y, .w = w, .h = h };
  return ret;
}


static inline SDL_Rect Rect (int w, int h) {
  SDL_Rect ret = { .x = 0, .y = 0, .w = w, .h = h };
  return ret;
}


static inline SDL_Rect Rect (SDL_Surface *surf) {
  SDL_Rect ret = { .x = 0, .y = 0, .w = surf->w, .h = surf->h };
  return ret;
}


static inline SDL_Rect Rect (SDL_Texture *tex) {
  SDL_Rect ret;
  ret.x = ret.y = 0;
  SDL_QueryTexture (tex, NULL, NULL, &ret.w, &ret.h);
  return ret;
}


static inline SDL_Rect RectScreen () { return Rect (0, 0, UI_RES_X, UI_RES_Y); }
  ///< @brief Rectangle representing the whole UI screen.


static inline void RectMove (SDL_Rect *rect, int dx, int dy) {
  rect->x += dx;
  rect->y += dy;
}


static inline void RectGrow (SDL_Rect *rect, int dx, int dy) {
  rect->x -= dx;
  rect->y -= dy;
  rect->w += 2 * dx;
  rect->h += 2 * dy;
}


void RectAlign (SDL_Rect *rect, SDL_Rect container, int hAlign = -1, int vAlign = -1);
  ///< @brief Align the rectangle 'rect' with an outer rectangle 'container'.
  /// @param rect Rectangle to align.
  /// @param container Outer rectangle serving as a reference.
  /// @param hAlign Horizontal alignmnent: -1 = left, 0 = center, +1 = right.
  /// @param vAlign Vertical alignmnent: -1 = upper, 0 = center, +1 = lower.


static inline void RectCenter (SDL_Rect *rect, SDL_Rect container) { RectAlign (rect, container, 0, 0); }
  ///< @brief Center the rectangle 'rect' with respect to an outer rectangle 'container'.


static inline bool RectContains (SDL_Rect *rect, int x, int y) {
  return x >= rect->x && x < rect->x+rect->w && y >= rect->y && y < rect->y+rect->h;
}


/// @}





// *************************** Surface helpers *********************************


/// @name Surface helpers ...
/// See also: CNetpbmReader
/// @{


#define ORIENT(FLIPH, ROT) (((FLIPH) ? 4 : 0) | ((ROT) & 3))
  ///< @brief Get orientation identifier based on horizontal flipping and a rotation in 90 degree steps clock-wise.
#define ORIENT_WITH_FLIPV(FLIPH, ROT) (((FLIPH) ? 0 : 4) | (((ROT) + 2) & 3))
  ///< @brief Get orientation identifier with implied vertical flipping first.

#define ORIENT_FLIPH(ORI) (((ORI) & 4) ? true : false)
  ///< @brief Get whether horizontal flipping is included in the orientation.
#define ORIENT_ROT(ORI) ((ORI) & 3)
  ///< @brief Get the number of rotations.


#if SELECTED_SDL_PIXELFORMAT==SDL_PIXELFORMAT_ARGB8888

static inline SDL_Surface *CreateSurface (int w, int h) { return SDL_CreateRGBSurfaceWithFormat (0, w, h, 32, SELECTED_SDL_PIXELFORMAT); }
static inline SDL_Surface *CreateSurface (SDL_Rect r) { return CreateSurface (r.w, r.h); }
static inline SDL_Surface *CreateSurface (SDL_Rect *r) { return CreateSurface (r->w, r->h); }

static inline SDL_Surface *CreateSurfaceFrom (int w, int h, void *_pixels, int _pitch) { return SDL_CreateRGBSurfaceFrom (_pixels, w, h, 32, _pitch, COL_MASK_R, COL_MASK_G, COL_MASK_B, COL_MASK_A); }

#endif


static inline SDL_Surface *SurfaceSet (SDL_Surface **pSurf, SDL_Surface *newSurf) { if (*pSurf) SDL_FreeSurface (*pSurf); *pSurf = newSurf; return newSurf; }
static inline void SurfaceFree (SDL_Surface **pSurf) { SurfaceSet (pSurf, NULL); }
static inline void SurfaceFree (SDL_Surface *surf) { SDL_FreeSurface (surf); }

static inline SDL_Surface *SurfaceDup (SDL_Surface *src) { return SDL_ConvertSurface (src, src->format, 0); }

void SurfaceNormalize (SDL_Surface **pSurf);
  ///< @brief Normalize pixel format to SELECTED_SDL_PIXELFORMAT.

void SurfaceRecolor (SDL_Surface *surf, TColor color);
  ///< @brief Replace all RGB values with the passed color while leaving alpha values untouched.
SDL_Surface *SurfaceGetOpaqueCopy (SDL_Surface *surf, TColor backColor);
  ///< @brief Create an opaque copy; caller becomes owner of returned surface.
void SurfaceMakeTransparentMono (SDL_Surface *surf, Uint8 opaqueLevel = 0xff);
  ///< @brief Copy the R values to the A channel and make all RGB values white.
  ///
  /// 'opaqueLevel' determines what is "white" here. This will be the final color,
  /// and all pixels originally brighter than this will be fully opaque.

void SurfaceBlit (SDL_Surface *src, SDL_Rect *srcRect, SDL_Surface *dst, SDL_Rect *dstRect = NULL, int hAlign = 0, int vAlign = 0, SDL_BlendMode blendMode = SDL_BLENDMODE_NONE);
  ///< @brief Blit a surface; It is allowed to leave 'src == NULL', in which case nothing happens.
  /// @param src is the source surface.
  /// @param srcRect is the part of the source surface to be blit (default = complete surface).
  /// @param dst is the destination surface.
  /// @param dstRect is the part of the destination surface to be rendered into (default = complete surface).
  /// @param hAlign is the horizontal alignment, if the destination rectangle has a different size (typically larger) than the source rectangle.
  /// @param vAlign is the vertical alignment, if the destination rectangle has a different size (typically larger) than the source rectangle.
  /// @param blendMode is the SDL blend mode.
static inline void SurfaceFill (SDL_Surface *surf, TColor col) { SDL_FillRect (surf, NULL, ToUint32 (col)); }
static inline void SurfaceFillRect (SDL_Surface *surf, SDL_Rect r, TColor col) { SDL_FillRect (surf, &r, ToUint32 (col)); }
static inline void SurfaceFillRect (SDL_Surface *surf, SDL_Rect *r, TColor col) { SDL_FillRect (surf, r, ToUint32 (col)); }

SDL_Surface *SurfaceGetScaledDownCopy (SDL_Surface *surf, int factor, bool preserveThinLines = false);
  ///< @brief Get a copy of the surface, scaled down by a factor in both dimensions.
  ///
  /// The dimensions of the original surface should be multiples of 'factor'.
  /// 'preserveThinLines == true' performs a gamma-correction on the alpha channel to increase small
  /// alpha values and may be used to avoid that thin lines get averaged out when scaling down.
SDL_Surface *SurfaceGetFlippedAndRotatedCopy (SDL_Surface *surf, int orient);
  ///< @brief Get a copy of the surface, which is eventually flipped and then rotated in 90° steps clockwise
  /// 'surf' must conform to SELECTED_SDL_PIXELFORMAT (see SurfaceNormalize() ).

SDL_Surface *SurfaceReadBmp (const char *fileName);
  ///< @brief Read a .bmp file.
  /// @param fileName may either be an absolute path name or relative to HOME2L_ROOT.
  ///
  /// On error, a warning is emitted and NULL is returned.



// ********** CNetpbmReader **********


#define NETPBM_IDLE -3      ///< State code "idle" for CNetpbmReader
#define NETPBM_ERROR -2     ///< State code "error" for CNetpbmReader
#define NETPBM_SUCCESS -1   ///< State code "success" for CNetpbmReader


/** @brief Reader for *Netpbm* image files.
 *
 * This class allows to read a [*Netpbm*](https://en.wikipedia.org/wiki/Netpbm_format)
 * image file (type 2 or 3) into an *SDL_Surface* object.
 */
class CNetpbmReader {
  public:
    CNetpbmReader () { surf = NULL; state = NETPBM_IDLE; }
    ~CNetpbmReader () { Clear (); }

    /// @name Feeding the reader ...
    /// @{
    void Clear ();                  ///< Reset reader (not necessary at the first time).
    void Put (const char *line);    ///< Put a text line.
    /// @}

    /// @name Obtaining the status and result ...
    /// @{
    int State () { return state; }
    bool Running () { return state >= 0; }
    bool Error () { return state == NETPBM_ERROR; }
    bool Success () { return state == NETPBM_SUCCESS; }

    SDL_Surface *Surface () { return Success () ? surf : NULL; }
      ///< @brief Return surface on success or NULL in all other cases.
      ///
      /// For grey images (type 'P2'), the grey value is mapped to the alpha channel,
      /// whereas the RGB channels contain the color 'WHITE'. The color can be changed
      /// using 'SurfaceRecolor', eventually followed by 'SurfaceGetOpaqueCopy'.
    /// @}

    /// @name High-level readers ...
    /// @{
    bool ReadFile (const char *fileName);
    bool ReadStream (int fd);
    bool ReadShell (CShell *shell);
    /// @}

  protected:
    SDL_Surface *surf;
    int format;             // 2 or 3 (the number behind the 'P')
    int w, h;
    int state;              // Number of words read (1 => 'Pn' was just read; < 0: no reading in progress)
};


/// @}





// *************************** Texture helpers *********************************


/// @name Texture helpers ...
/// @{


static inline SDL_Texture *CreateTexture (SDL_Surface *surf) { return SDL_CreateTextureFromSurface (uiSdlRenderer, surf); }

static inline void TextureSet (SDL_Texture **pTex, SDL_Texture *newTex) { if (*pTex) SDL_DestroyTexture (*pTex); *pTex = newTex; }
static inline void TextureFree (SDL_Texture **pTex) { TextureSet (pTex, NULL); }

static inline void TextureRender (SDL_Texture *tex, SDL_Rect *srcRect = NULL, SDL_Rect *dstRect = NULL) { SDL_RenderCopy (uiSdlRenderer, tex, srcRect, dstRect); }


/// @}





// *************************** Icon handling ***********************************


/// @name Icon handling ...
/// @{


SDL_Surface *IconGet (const char *name, TColor color = WHITE, TColor bgColor = TRANSPARENT, int scaleDown = 1, int orient = 0, bool preserveThinLines = false);
  ///< @brief Get a reference to an icon from the application's '*etc/icons/*' or '*share/icons/*' folder.
  ///
  /// The name is passed without a file name suffix. The returned reference remains owned by
  /// the library, which may perform some caching if icons are used muliple times.
  ///


/// @}





// *************************** Font handling ***********************************


/// @name Font handling ...
/// @{


/// Supported font styles
enum EFontStyle {
  fntNormal = 0,      ///< Normal
  fntBold,            ///< Bold
  fntItalic,          ///< Italic
  fntBoldItalic,      ///< Bold and italic
  fntLight,           ///< Light
  fntMono,            ///< Monotype
  fntMonoBold,        ///< Monotype, bold
  fntMonoItalic,      ///< Monotype, italic
  fntMonoBoldItalic,  ///< Monotype, bold and italic
  fntEND
};


TTF_Font *FontGet (EFontStyle style, int size);
  ///< @brief Get a reference to a font of a specific style and size.

SDL_Surface *FontRenderText (TTF_Font *font, const char *text, TColor color);
  ///< @brief Render a text using 'blended' mode of the *SDL2_ttf* library.
  /// The returned surface is a new surface of minimal size and must later be free'd
  /// using SurfaceFree().
SDL_Surface *FontRenderText (TTF_Font *font, const char *text, TColor color, TColor bgColor);
  ///< @brief Render a text using 'shaded' mode of the *SDL2_ttf* library.
  /// This mode is considered faster than 'blended' mode.
  /// The returned surface is a new surface of minimal size and must later be free'd
  /// using SurfaceFree().

static inline int FontGetLineSkip (TTF_Font *font) { return TTF_FontLineSkip (font); }

static inline int FontGetHeight (TTF_Font *font) { return TTF_FontHeight (font); }
int FontGetWidth (TTF_Font *font, const char *text, int textLen = -1);


/// @}





// *************************** Complex text formatting *************************


/// @name Complex text formatting ...
/// See also: CTextFormat, CTextSet
/// @{


/// Text format specification.
/// All fields are public and may be arbitrarily written to in order to set the formatting.
class CTextFormat {
  public:
    CTextFormat () { font = NULL; }
    CTextFormat (TTF_Font *_font, TColor _color, TColor _bgColor = TRANSPARENT,
                 int _hAlign = -1, int _vAlign = -1,
                 int _hSpace = 0, int _vSpace = 0,
                 int _maxWidth = 0, int _maxHeigth = 0) {
      font = _font; color = _color; bgColor = _bgColor;
      hAlign = _hAlign; vAlign = _vAlign;
      hSpace = _hSpace; vSpace = _vSpace;
      maxWidth = _maxWidth;
      maxHeight = _maxHeigth;
    }

    // Parameters ...
    TTF_Font *font;           ///< Text font.
    TColor color, bgColor;    ///< Foreground and background color; 'bgColor' == 'TRANSPARENT' selects "blended mode" in font rendering.
    int hAlign, vAlign;       ///< Alignment: -1 = left/up, 0 = center, +1 = right/down.
    int vSpace, hSpace;       ///< Vertical and horizontal space in pixels.
    int maxWidth, maxHeight;  ///< Maximum width in pixels (for automatic line breaking).
};


/// Typesetter for longer formatted texts.
class CTextSet {
  public:
    CTextSet () { surface = NULL; firstItem = NULL; height = 0; }
    ~CTextSet () { Clear (); }

    void Clear ();
      ///< @brief Clear everything.
    void AddLines (const char *text, CTextFormat fmt, bool *retAbbreviated = NULL);
      ///< @brief Add text with a given format.
      /// If the 'maxHeight' parameter is set in 'fmt', the text is automatically abbreviated if it is too long.
      /// The value returned via '*retAbbreviated' indicates whether the text was abbreviated.
    int GetHeight ();
      ///< @brief Accumulated height in pixels according to all previous AddLines() invocations

    SDL_Surface *Render (SDL_Surface *dst = NULL, SDL_Rect *dstRect = NULL);
      ///< @brief Render everything and return a new surface containing the text.
      ///
      /// If 'dst' is NULL, a suitable surface is created, which needs to be free'd later.
      /// Its dimensions are set to those of 'dstRect' or, if 'dstRect' is NULL, to a minimum
      /// area required for the text.
      /// If 'dst' is not NULL, the text is painted into 'dst', and 'dst' is returned.
      /// 'dstRect' is (if not NULL) the sub-area inside 'dst' into which the text will be placed.
      /// In any case, text reflow and abbreviation is based on the settings in 'fmt' ('maxWidth', 'maxHeight').
      /// Hence, these values in 'fmt' should not be larger than implied limits passed via 'dst'/'dstRect'.

  protected:
    SDL_Surface *surface;
    class CTextItem *firstItem;
    int height;
};


// Easy-to-use function(s)...
SDL_Surface *TextRender (const char *text, CTextFormat fmt, SDL_Surface *dst = NULL, SDL_Rect *dstRect = NULL, bool *retAbbreviated = NULL);
  ///< @brief Easy-to-use function to typeset a text with a single format.


/// @}





/** @}  // ui_base
 * @}   // ui
 */


#endif
