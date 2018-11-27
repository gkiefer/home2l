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


#include "ui_screen.H"

#include "system.H"
#include "apps.H"

#include <stdarg.h>





// ***************** SDL wrappers and helpers **************


#if SDL_VERSION_ATLEAST (2, 0, 5)


static inline void SetClipRect (SDL_Renderer *ren, SDL_Rect *r) {
  SDL_RenderSetClipRect (ren, r);
}


#else


static inline void SetClipRect (SDL_Renderer *ren, SDL_Rect *r) {
  // WORKAROUND (2016-01-17):
  //   Apparently, there are bugs in SDL 2.0.3 related to clipping in
  //   the case that the logical resolution and the aspect ratio is
  //   different from the window size.
  //
  //   'SDL_RenderSetClipRect' obviously assumes that the rendering is
  //   is aligned to the lower left corner of the window, whereas the render
  //   assume it centered in the window.
  //
  //   An alternative solution based on 'SDL_RenderSetViewPort'
  //   (see commit 7415c0 from 2016-01-17) works almost fine on a PC, but
  //   produces even more layout problems and finger/mouse event location
  //   problems in Android, even if the logical and physical resolution
  //   are identical.
  //
  //   This function is a wrapper to 'SDL_RenderSetClipRect' to make it work.
  //
  //   The bug appears to be fixed in SDL 2.0.5.
  //
  SDL_Rect argR;
  int winW, winH, rX, rY;

  if (!r) SDL_RenderSetClipRect (ren, NULL);
  else {
    UiGetWindowSize (&winW, &winH);
    argR = *r;
    rX = winW * UI_RES_Y;
    rY = winH * UI_RES_X;
    if (rX > rY) argR.x += (winW * UI_RES_Y / winH - UI_RES_X) / 2;
      // window is wider than visible part => shift clip rectangle to the right
    if (rY > rX) argR.y -= (winH * UI_RES_X / winW - UI_RES_Y) / 2;
      // window is taller than visible part => shift clip rectangle up
    SDL_RenderSetClipRect (ren, &argR);
  }
}


#endif





// ***************** CWidget *******************************


CWidget::CWidget () {
  screen = NULL;
  canvas = NULL;
  next = NULL;
  surface = NULL;
  area = Rect (0, 0, UI_RES_X, UI_RES_Y);
  texture = NULL;
  sdlBlendMode = SDL_BLENDMODE_NONE;
}


void CWidget::LocalToScreenCoords (int *x, int *y) {
  if (canvas) canvas->WidgetToScreenCoords (x, y);
}


void CWidget::ScreenToLocalCoords (int *x, int *y) {
  if (canvas) canvas->ScreenToWidgetCoords (x, y);
}


void CWidget::GetMouseEventPos (SDL_Event *ev, int *x, int *y) {
  if (ev->type == SDL_MOUSEMOTION) {
    *x = ev->motion.x;
    *y = ev->motion.y;
  }
  else {
    *x = ev->button.x;
    *y = ev->button.y;
  }
  ScreenToLocalCoords (x, y);
}


SDL_Texture *CWidget::GetTexture () {
  SDL_Surface *surf;

  if (!texture) {
    surf = GetSurface ();
    if (surf) {
      texture = SDL_CreateTextureFromSurface (UiGetSdlRenderer (), surf);
      SDL_SetTextureBlendMode (texture, sdlBlendMode);
    }
  }
  return texture;
}


void CWidget::Render (SDL_Renderer *ren) {
  SDL_Texture *tex;
  SDL_Rect r;

  tex = GetTexture ();
  if (tex) {
    GetRenderArea (&r);
    SDL_RenderCopy (ren, tex, NULL, &r);
  }
}


void CWidget::Changed () {
  ClearTexture ();
  if (screen) screen->Changed ();
  if (canvas) canvas->Changed ();
  //~ if (canvas) if (canvas->IsVisible (&area)) canvas->Changed ();
}


void CWidget::ClearTexture () {
  if (texture) {
    SDL_DestroyTexture (texture);
    texture = NULL;
  }
}


void CWidget::RenderList (CWidget *list, SDL_Renderer *ren) {
  if (list) {
    RenderList (list->next, ren);
    list->Render (ren);
  }
}





// ***************** CCanvas *******************************


void CCanvas::SetVirtArea (SDL_Rect r) {
  LimitVirtArea (&r);
  if (r.x != virtArea.x || r.y != virtArea.y || r.w != virtArea.w || r.h != virtArea.h) {
    virtArea = r;
    Changed ();
  }
}


void CCanvas::LimitVirtArea (SDL_Rect *r) {
  bool changed = false;

  if (!r) r = &virtArea;
  if (r->x + r->w < area.x + area.w)  { r->x = area.x + area.w - r->w; changed = true; }
  if (r->x > area.x)                  { r->x = area.x;                 changed = true; }
  if (r->y + r->h < area.y + area.h)  { r->y = area.y + area.h - r->h; changed = true; }
  if (r->y > area.y)                  { r->y = area.y;                 changed = true; }
  if (changed && r == &virtArea) Changed ();
}


void CCanvas::ScrollTo (SDL_Rect r, int hAlign, int vAlign) {
  SDL_Rect s;
  int vx, vy;

  s = Rect (0, 0, r.w, r.h);
  RectAlign (&s, area, hAlign, vAlign);
  vx = s.x - r.x;
  vy = s.y - r.y;
  if (vx != virtArea.x || vy != virtArea.y) {
    virtArea.x = vx;
    virtArea.y = vy;
    Changed ();
  }
  LimitVirtArea ();
}


void CCanvas::ScrollIn (SDL_Rect r) {
  bool changed = false;

  if (virtArea.x + r.x < area.x) { virtArea.x = area.x - r.x; changed = true; }
  if (virtArea.y + r.y < area.y) { virtArea.y = area.y - r.y; changed = true; }
  if (virtArea.x + r.x + r.w > area.x + area.w) { virtArea.x = area.x + area.w - r.x - r.w; changed = true; }
  if (virtArea.y + r.y + r.h > area.y + area.h) { virtArea.y = area.y + area.h - r.y - r.h; changed = true; }
  if (changed) Changed ();
}


bool CCanvas::IsVisible (SDL_Rect *r) {
  return (r->x + virtArea.x < area.x + area.w) && (r->x + virtArea.x + r->w > area.x) &&
         (r->y + virtArea.y < area.y + area.h) && (r->y + virtArea.y + r->h > area.y);
}


void CCanvas::WidgetToScreenCoords (int *x, int *y) {
  *x += virtArea.x;
  *y += virtArea.y;
  if (canvas) canvas->WidgetToScreenCoords (x, y);
}


void CCanvas::ScreenToWidgetCoords (int *x, int *y) {
  *x -= virtArea.x;
  *y -= virtArea.y;
  if (canvas) canvas->ScreenToWidgetCoords (x, y);
}


void CCanvas::Render (SDL_Renderer *ren) {
  SDL_Rect renArea, r;

  if (area.w <= 0 || area.h <= 0) return;

  // Set clipping area...
  renArea = area;
  LocalToScreenCoords (&renArea.x, &renArea.y);
  SetClipRect (ren, &renArea);

  // Fill background...
  SDL_SetRenderDrawBlendMode (ren, sdlBlendMode);
  SDL_SetRenderDrawColor (ren, backColor.r, backColor.g, backColor.b, backColor.a);
  SDL_RenderFillRect (ren, NULL);

  // Render all sub-widgets...
  RenderList (firstWidget, ren);

  // Render scroll bars...
  SDL_SetRenderDrawBlendMode (ren, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor (ren, scrollbarColor.r, scrollbarColor.g, scrollbarColor.b, scrollbarColor.a);
  if (virtArea.h > renArea.h) {    // vertical scrollbar...
    r.w = scrollbarWidth;
    r.x = renArea.x + area.w - r.w;
    r.h = area.h * area.h / virtArea.h;
    r.y = renArea.y + (area.h - r.h) * (area.y - virtArea.y) / (virtArea.h - area.h);
    SDL_RenderFillRect (ren, &r);
  }
  if (virtArea.w > renArea.w) {    // horizontal scrollbar...
    r.h = scrollbarWidth;
    r.y = renArea.y + area.h - r.h;
    r.w = area.w * area.w / virtArea.w;
    r.x = renArea.x + (area.w - r.w) * (area.x - virtArea.x) / (virtArea.w - area.w);
    SDL_RenderFillRect (ren, &r);
  }

  // Unset clipping...
  SetClipRect (ren, NULL);
}


bool CCanvas::HandleEvent (SDL_Event *ev) {
  static CCanvas *dragCanvas = NULL;
  static enum { dragMain = 0, dragVBar, dragHBar } dragType;
  static int startX, startY, startVirtX, startVirtY;
  SDL_Rect va;
  int x, y;
  bool ret;

  ret = false;
  if (!dragCanvas || dragCanvas == this) {
    switch (ev->type) {

      case SDL_MOUSEBUTTONDOWN:
        GetMouseEventPos (ev, &x, &y);
        //INFOF(("SDL_FINGERDOWN(%i/%i): %4i, %4i", (int) ev->tfinger.touchId, (int) ev->tfinger.fingerId, x, y));
        if (RectContains (&area, x, y)) {
          if (virtArea.h > area.h && x >= area.x + area.w/2) {
            dragCanvas = this;
            dragType = (x >= area.x + area.w*7/8) ? dragVBar : dragMain;
            ret = true;
          }
          else if (virtArea.w > area.w && y >= area.y + area.h/2) {
            dragCanvas = this;
            dragType = (y >= area.y + area.h*7/8) ? dragHBar : dragMain;
            ret = true;
          }
          startX = x;
          startY = y;
          startVirtX = virtArea.x;
          startVirtY = virtArea.y;
        }
        break;

      case SDL_MOUSEMOTION:
        if (dragCanvas == this) {
          GetMouseEventPos (ev, &x, &y);
          va = virtArea;
          switch (dragType) {
            case dragMain:
              if (va.w > area.w) va.x = startVirtX + x - startX;
              if (va.h > area.h) va.y = startVirtY + y - startY;
              break;
            case dragVBar:
              va.y = startVirtY - (y - startY) * (va.h - area.h) / (area.h - area.h * area.h / va.h);
              break;
            case dragHBar:
              va.x = startVirtX - (x - startX) * (va.w - area.w) / (area.w - area.w * area.w / va.w);
              break;
          }
          LimitVirtArea (&va);
          SetVirtArea (va);
          ret = true;
        }
        break;

      case SDL_MOUSEBUTTONUP:
        if (dragCanvas == this) {
          //~ LimitVirtArea ();
          dragCanvas = NULL;
          ret = true;
        }
        break;
    }
  }
  return ret;
}


void CCanvas::DelAllWidgets () {
  for (CWidget *w = firstWidget; w; w = w->next) w->canvas = NULL;
  firstWidget = NULL;
}


void CCanvas::DoAddWidget (CWidget **pFirst, CWidget *widget) {
  if (widget->canvas == this) return;     // is already added

  // Add before '*pFirst'...
  widget->screen = NULL;
  widget->canvas = this;
  widget->next = *pFirst;
  *pFirst = widget;

  // Mark as changed...
  Changed ();
}


void CCanvas::DoDelWidget (CWidget **pFirst, CWidget *widget) {
  CWidget **pCur;

  if (this != widget->canvas) return;  // widget is not member of this convas

  // Search for widget and remove it...
  pCur = pFirst;
  while (*pCur && (*pCur != widget)) pCur = &((*pCur)->next);
  if (*pCur) {
    *pCur = (*pCur)->next;
    widget->canvas = NULL;
  }

  // Mark as changed...
  Changed ();
}






// ***************** CScreen *******************************


CScreen *CScreen::activeScreen = NULL;
bool CScreen::changed = false;
bool CScreen::emulateOff = false;
bool CScreen::emulateStandby = false;
bool CScreen::keyboardOn = false;





// ***** Init/Done *****


CScreen::~CScreen () {
  DelAllWidgets ();
}





// ***** Widgets *****


void CScreen::DelAllWidgets () {
  for (CWidget *w = firstWidget; w; w = w->next) w->screen = NULL;
  firstWidget = NULL;
}


void CScreen::DoAddWidget (CWidget **pFirst, CWidget *widget) {
  if (widget->screen == this) return;     // is already added

  // Add before '*pFirst'...
  widget->screen = this;
  widget->canvas = NULL;
  widget->next = *pFirst;
  *pFirst = widget;

  Changed ();
}


void CScreen::DoDelWidget (CWidget **pFirst, CWidget *widget) {
  CWidget **pCur;

  if (this != widget->screen) return;  // widget is not member of this screen

  // Search for widget and remove it...
  pCur = pFirst;
  while (*pCur && (*pCur != widget)) pCur = &((*pCur)->next);
  if (*pCur) {
    *pCur = (*pCur)->next;
    widget->screen = NULL;
  }

  Changed ();
}





// ***** Others *****


void CScreen::Activate (bool on) {
  if (!on) {    // Deactivate...
    if (this == activeScreen) activeScreen = NULL;
  }
  else {        // Activate...
    if (this != activeScreen) {
      // Deactivate previous screen...
      if (activeScreen) activeScreen->Deactivate ();
      // Link this screen...
      activeScreen = this;
      // Show or hide on-screen keyboard as appropriate...
      SetKeyboard (withKeyboard);
    }
  }

  // Trigger redraw...
  Changed ();
}


void CScreen::Run () {
  CScreen *lastActiveScreen;

  lastActiveScreen = activeScreen;
  running = true;
  Activate ();
  while (running && activeScreen == this) {
    UiIterate ();
    if (UiIsClosed ()) return;
  }
  if (activeScreen == this) {
    if (lastActiveScreen) lastActiveScreen->Activate ();
    else Activate (false);
  }
}


bool CScreen::HandleEvent (SDL_Event *ev) {
  if (withKeyboard) {
    if (ev->type == SDL_MOUSEBUTTONDOWN) if (ev->button.y > UI_RES_Y / 2) {
      // Touch on the lower half of the screen: Assume that the screen keyboard was gone
      // and the user wants to reactivate it.
      SDL_StopTextInput ();
      SDL_StartTextInput ();
      return true;
    }
  }
  //~ INFO ("CScreen::HandleEvent");
  if (activeScreen) for (CWidget *w = activeScreen->firstWidget; w; w = w->next)
    if (w->HandleEvent (ev)) return true;
  return false;
}


void CScreen::RenderUpdate () {
  SDL_Renderer *ren;

  if (!activeScreen || emulateOff) {
    // Clear the SDL window ...
    ren = UiGetSdlRenderer ();
    SDL_SetRenderDrawBlendMode (ren, SDL_BLENDMODE_NONE);
#if ANDROID
    SDL_SetRenderDrawColor (ren, 64, 64, 64, 0xff);  // grey & opaque; Brighter on Android since the screen backlight is usually reduced as well.
#else
    SDL_SetRenderDrawColor (ren, 32, 32, 32, 0xff);  // grey & opaque
#endif
    SDL_RenderClear (ren);
    SDL_RenderPresent (ren);
  }
  else if (changed) {
    // (Re-)Draw the active screen ...
    ren = UiGetSdlRenderer ();
    SDL_SetRenderDrawBlendMode (ren, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor (ren, 0, 0, 0, 0xff);    // black & opaque
    SDL_RenderClear (ren);
    CWidget::RenderList (activeScreen->firstWidget, ren);
    if (emulateStandby) {
      SDL_SetRenderDrawBlendMode (ren, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor (ren, 0, 0, 0, 0x80);  // black & semi-transparent
      SDL_RenderFillRect (ren, NULL);
    }
    SDL_RenderPresent (ren);
    changed = false;
  }
}


void CScreen::SetKeyboard (bool on) {
  withKeyboard = on;
  if (this == activeScreen && on != keyboardOn) {
    if (on) SDL_StartTextInput ();
    else {
      SDL_StopTextInput ();
      //~ SystemSetImmersiveMode ();  // WORKAROUND: Android switches off immersive mode when keyboard is closed
    }
    keyboardOn = on;
  }
}





// ***************** Layout Iterators **********************


SDL_Rect *LayoutRow (SDL_Rect container, const int *format, int items, int space) {
  SDL_Rect *ret;
  int n, pos, numRel, wFixed, wRel = 0, itemFormat;

  // Analyse format array...
  if (items < 0) for (items = 0; format[items]; items++);
  wFixed = 0;
  if (format) {
    numRel = 0;
    for (n = 0; n < items; n++) {
      if (format[n] > 0) wFixed += format[n];
      else numRel -= format[n];
    }
  }
  else numRel = items;

  // Do the layout...
  ret = MALLOC(SDL_Rect, items);
  if (numRel > 0) wRel = (container.w - wFixed - (items-1) * space + numRel/2) / numRel;
  pos = 0;
  for (n = 0; n < items; n++) {
    itemFormat = format ? format[n] : -1;
    if (itemFormat > 0) {
      // place fixed-width item...
      ret[n] = Rect (container.x + pos, container.y, itemFormat, container.h);
      pos += (itemFormat + space);
    }
    else {
      // place relative-width item...
      ret[n] = Rect (container.x + pos, container.y, -itemFormat * wRel, container.h);
      pos += (-itemFormat * wRel + space);
    }
  }
  n = items - 1;
  itemFormat = format ? format[n] : -1;
  if (itemFormat > 0) ret[n].x = container.x + container.w - ret[n].w;  // right-justify last item
  else ret[n].w = container.x + container.w - ret[n].x;   // adapt size to match right border

  // Done...
  return ret;
}


SDL_Rect *LayoutRow (SDL_Rect container, int space, ...) {
  va_list ap;
  int format[256];    // reserve sufficient space for the format on the stack
  int n, val;

  va_start (ap, space);
  n = 0;
  do {
    val = va_arg (ap, int);
    format [n++] = val;
    ASSERT (n < (int) (sizeof (format) / sizeof (int)));
  }
  while (val != 0);
  va_end (ap);
  return LayoutRow (container, format, -1, space);
}


SDL_Rect *LayoutCol (SDL_Rect container, const int *format, int items, int space) {
  SDL_Rect *ret, tCont;
  int n;

  tCont = Rect (container.y, container.x, container.h, container.w);
  ret = LayoutRow (tCont, format, items, space);
  for (n = 0; format[n]; n++) ret[n] = Rect (ret[n].y, ret[n].x, ret[n].h, ret[n].w);
  return ret;
}


SDL_Rect *LayoutCol (SDL_Rect container, int space, ...) {
  va_list ap;
  int format[256];    // reserve sufficient space for the format on the stack
  int n, val;

  va_start (ap, space);
  n = 0;
  do {
    val = va_arg (ap, int);
    format [n++] = val;
    ASSERT (n < (int) (sizeof (format) / sizeof (int)));
  }
  while (val != 0);
  va_end (ap);
  return LayoutCol (container, format, -1, space);
}


SDL_Rect *LayoutMatrix (SDL_Rect container, const int *hFormat, const int *vFormat, int hItems, int vItems, int hSpace, int vSpace) {
  SDL_Rect *hLayout, *vLayout, *ret;
  int n, x, y;

  hLayout = LayoutRow (container, hFormat, hItems, hSpace);
  vLayout = LayoutCol (container, vFormat, vItems, vSpace);
  if (hItems < 0) for (hItems = 0; hFormat[hItems]; hItems++);
  if (vItems < 0) for (vItems = 0; vFormat[vItems]; vItems++);
  ret = MALLOC(SDL_Rect, hItems * vItems);
  for (y = 0; y < vItems; y++) for (x = 0; x < hItems; x++) {
    n = hItems * y + x;
    ret[n] = Rect (hLayout[x].x, vLayout[y].y, hLayout[x].w, vLayout[y].h);
  }
  free (hLayout);
  free (vLayout);
  return ret;
}
