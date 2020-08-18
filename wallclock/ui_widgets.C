/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2015-2020 Gundolf Kiefer
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





// *****************************************************************************
// *                                                                           *
// *                         Useful widget extensions                          *
// *                                                                           *
// *****************************************************************************



// *************************** CModalWidget ************************************


/* This is a widget that can pop up on a screen, such as
 * a menu or a message box.
 */


int CModalWidget::Run (CScreen *_screen) {
  Start (_screen);
  while (IsRunning ()) UiIterate ();
  //~ INFO ("# CModalWidget::Run exits");
  return status;
}


void CModalWidget::Start (CScreen *_screen) {
  if (IsRunning ()) return;
  status = -2;
  _screen->AddWidget (this, 1);
}


bool CModalWidget::IsRunning () {
  if (!screen) return false;    // invisible
  if (screen != CScreen::ActiveScreen () || UiIsClosed ()) {
    Stop ();
    return false;
  }
  return true;
}


void CModalWidget::Stop () {
  if (screen) screen->DelWidget (this);
  if (status < 0) status = -1;
}


bool CModalWidget::HandleEvent (SDL_Event *ev) {
  int x, y;

  switch (ev->type) {
    case SDL_MOUSEBUTTONDOWN:
      GetMouseEventPos (ev, &x, &y);
      // Prevent event from being passed to background widgets...
      if (RectContains (&area, x, y)) return true;
      // Handle cancellation by touching outside the widget...
      if (!RectContains (&rNoCancel, x, y)) {
        Stop ();
        return true;
      }
      break;
    case SDL_KEYDOWN:
      if (ev->key.keysym.mod == KMOD_NONE && ev->key.keysym.sym == SDLK_ESCAPE) Stop ();
      return true;
    case SDL_KEYUP:
      return true;
  }

  return false;
}





// *************************** CCursorWidget ***********************************


void CCursorWidget::Render (SDL_Renderer *ren) {
  SDL_Rect r;

  CWidget::Render (ren);
  if (ren && cursorArea.w && cursorArea.h) {
    r = Rect (area.x + cursorArea.x, area.y + cursorArea.y, cursorArea.w, cursorArea.h);
    LocalToScreenCoords (&r.x, &r.y);
    SDL_SetRenderDrawBlendMode (ren, blendMode);
    SDL_SetRenderDrawColor (ren, cursorColor.r, cursorColor.g, cursorColor.b, cursorColor.a);
    SDL_RenderFillRect (ren, &r);
  }
}


bool CCursorWidget::HandleEvent (SDL_Event *ev) {
  if (!cbHandleEvent) return false;
  return cbHandleEvent (ev, cbHandleEventData);
}





// *****************************************************************************
// *                                                                           *
// *                     The widgets                                           *
// *                                                                           *
// *****************************************************************************



// *************************** CButton *****************************************


void CbActivateScreen (CButton *, bool, void *screen) {
  if (screen) ((CScreen *) screen)->Activate ();
  else WARNING ("Tried to activate non-existing screen");
}


void CButton::Init () {
  surfLabel = NULL;
  surfLabelIsOwned = false;
  colNorm = colDown = TRANSPARENT;
  hAlign = vAlign = 0;
  cbPushed = NULL;
  cbPushedData = NULL;
  isDown = changed = false;
  hotkey = SDLK_UNKNOWN;
}


void CButton::Done () {
  if (!surfLabelIsOwned) surfLabel = NULL;
  SurfaceFree (&surfLabel);
  SurfaceFree (&surface);
}


void CButton::Set (SDL_Rect _area, TColor _color) {
  SetArea (_area);
  SetColor (_color);
}


void CButton::Set (SDL_Rect _area, TColor _color, SDL_Surface *_icon) {
  Set (_area, _color);
  SetLabel (_icon);
}


void CButton::Set (SDL_Rect _area, TColor _color, const char *text, TColor textColor, TTF_Font *font) {
  Set (_area, _color);
  SetLabel (text, textColor, font);
}


void CButton::Set (SDL_Rect _area, TColor _color, SDL_Surface *_icon, const char *text, TColor textColor, TTF_Font *font) {
  Set (_area, _color);
  SetLabel (_icon, text, textColor, font);
}


void CButton::SetArea (SDL_Rect _area) {
  CWidget::SetArea (_area);
  ChangedSurface ();
}


void CButton::SetColor (TColor _colNorm, TColor _colDown) {
  if (_colNorm != colNorm || _colDown != colDown) {
    colNorm = _colNorm;
    colDown = _colDown;
    ChangedSurface ();
  }
}


void CButton::SetLabel (SDL_Surface *_icon, SDL_Rect *srcRect, bool takeOwnership) {

  // Clear old label...
  if (surfLabelIsOwned) SurfaceFree (surfLabel);
  surfLabel = NULL;
  surfLabelIsOwned = false;

  if (_icon) {

    // Simple case...
    if (!srcRect) {
      SurfaceSet (&surfLabel, _icon);
      surfLabelIsOwned = takeOwnership;
    }

    // Sub-image case...
    else {
      SurfaceSet (&surfLabel, CreateSurface (*srcRect));
      SurfaceBlit (_icon, srcRect, surfLabel, NULL);
      if (takeOwnership) SurfaceFree (_icon);
      surfLabelIsOwned = true;
    }
  }

  // Done...
  ChangedSurface ();
}


void CButton::SetLabel (const char *text, TColor textColor, TTF_Font *font) {
  if (!surfLabelIsOwned) surfLabel = NULL;
  if (!text) SurfaceFree (&surfLabel);
  else SurfaceSet (&surfLabel, FontRenderText (font ? font : BUTTON_DEFAULT_FONT, text, textColor));
  surfLabelIsOwned = true;
  ChangedSurface ();
}


void CButton::SetLabel (SDL_Surface *_icon, const char *text, TColor textColor, TTF_Font *font) {
  SDL_Surface *surfText;
  SDL_Rect rIcon, rText, rAll;

  // Catch special cases...
  if (!_icon) { SetLabel (text, textColor, font); return; }
  if (!text) { SetLabel (SurfaceDup (_icon), NULL, true); return; }

  // Create text surface...
  surfText = FontRenderText (font ? font : BUTTON_DEFAULT_FONT, text, textColor);

  // Calculate layout...
  rIcon = Rect (_icon);
  rText = Rect (surfText);
  rAll = Rect (0, 0, rIcon.w + rIcon.w / 4 + rText.w, MAX (rIcon.h, rText.h));
  RectAlign (&rIcon, rAll, -1, 0);    // left-justify icon
  RectAlign (&rText, rAll, 1, 0);     // right-justify text

  // Draw joint surface...
  if (!surfLabelIsOwned) surfLabel = NULL;
  SurfaceSet (&surfLabel, CreateSurface (rAll.w, rAll.h));
  SDL_FillRect (surfLabel, NULL, ToUint32 (TRANSPARENT));
  SurfaceBlit (_icon, NULL, surfLabel, &rIcon);
  SurfaceBlit (surfText, NULL, surfLabel, &rText);
  surfLabelIsOwned = true;

  // Cleanup & wrap up...
  SDL_FreeSurface (surfText);
  ChangedSurface ();
}


void CButton::OnPushed (bool longPush) {
  if (cbPushed) cbPushed (this, longPush, cbPushedData);
}


SDL_Surface *CButton::GetSurface () {
  SDL_Rect r;
  int n;

  if (changed) {
    ASSERT (area.w > 0 && area.h > 0);

    // Create surface...
    SurfaceSet (&surface, CreateSurface (area.w, area.h));

    // Draw background...
    r = Rect (surface);
    for (n = 0; n < 64; n++) {
      r.h = (n + 1) * surface->h / 64 - r.y;
      SDL_FillRect (surface, &r, ToUint32 (ColorBrighter (colNorm, isDown ? n-32 : 32-n)));
      r.y += r.h;
    }

    // Draw label...
    if (surfLabel) {
      r = Rect (surface);
      RectGrow (&r, -BUTTON_LABEL_BORDER, -BUTTON_LABEL_BORDER);
      SurfaceBlit (surfLabel, NULL, surface, &r, hAlign, vAlign, SDL_BLENDMODE_BLEND);
    }

    // Done...
    changed = false;
  }
  return surface;
}


bool CButton::HandleEvent (SDL_Event *ev) {
  int x, y;
  bool ret;

  ret = false;
  switch (ev->type) {
    case SDL_MOUSEBUTTONDOWN:
      GetMouseEventPos (ev, &x, &y);
      if (RectContains (&area, x, y)) {
        if (isDown && ev->button.clicks == 2) {   // long push?
          isDown = false;
          ChangedSurface ();
          OnPushed (true);
          ret = true;
        }
        else if (!isDown) {
          isDown = true;
          ChangedSurface ();
          ret = true;
        }
      }
      break;
    case SDL_MOUSEMOTION:
      GetMouseEventPos (ev, &x, &y);
      if (!RectContains (&area, x, y) && isDown) {
        isDown = false;
        ChangedSurface ();
      }
      break;
    case SDL_MOUSEBUTTONUP:
      if (isDown) {
        isDown = false;
        ChangedSurface ();
        OnPushed (false);   // This was surely a short push
        ret = true;
      }
      break;
    case SDL_KEYDOWN:
      //~ INFOF (("CButton: SDL_KEYDOWN '%s', hotkey = '%s'", SDL_GetKeyName (ev->key.keysym.sym), SDL_GetKeyName (hotkey)));
      if (ev->key.keysym.sym == hotkey && ev->key.keysym.mod == KMOD_NONE && hotkey != SDLK_UNKNOWN) {
        isDown = true;
        ChangedSurface ();
        ret = true;
      }
      break;
    case SDL_KEYUP:
      if (ev->key.keysym.sym == hotkey && ev->key.keysym.mod == KMOD_NONE && hotkey != SDLK_UNKNOWN && isDown) {
        isDown = false;
        ChangedSurface ();
        OnPushed (false);
        ret = true;
      }
      break;
  }
  return ret;
}


CButton *CreateMainButtonBar (int buttons, TButtonDescriptor *descTable, CScreen *screen) {
  CButton *ret;
  SDL_Rect *layout;
  int n, *layoutDef;

  layoutDef = MALLOC (int, buttons);
  for (n = 0; n < buttons; n++) layoutDef[n] = descTable[n].layoutWidth;
  layout = LayoutRow (UI_BUTTONS_RECT, layoutDef, buttons);
  free (layoutDef);

  ret = new CButton [buttons];
  for (n = 0; n < buttons; n++) {
    ret[n].Set (layout[n], descTable[n].color, IconGet (descTable[n].iconName, WHITE), _(descTable[n].text), WHITE);
    ret[n].SetCbPushed (descTable[n].cbPushed, screen);
    if (descTable[n].hotkey != SDLK_UNKNOWN) ret[n].SetHotkey (descTable[n].hotkey);
    if (screen) screen->AddWidget (&ret[n]);
  }

  free (layout);
  return ret;
}





// *************************** CFlatButton *************************************


SDL_Surface *CFlatButton::GetSurface () {
  SDL_Rect r;

  if (changed) {
    SurfaceSet (&surface, CreateSurface (area.w, area.h));
    SDL_FillRect (surface, NULL, ToUint32 (isDown ? colDown : colNorm));
    if (surfLabel) {
      r = Rect (surface);
      RectGrow (&r, -BUTTON_LABEL_BORDER, -BUTTON_LABEL_BORDER);
      SurfaceBlit (surfLabel, NULL, surface, &r, hAlign, vAlign, SDL_BLENDMODE_BLEND);
    }
    changed = false;
  }
  return surface;
}





// *************************** CListbox ****************************************


CListbox::CListbox () {
  itemArr = NULL;
  pool = NULL;
  poolIdx = NULL;
  items = poolSize = 0;
  selectedItem = downIdx = -1;
  noLongPush = false;
  cbPushed = NULL;
  SetMode (lmReadOnly, 0);
  SetFormat (NULL);
}


CListbox::~CListbox () {
  SetItems (0);
  if (itemArr) delete [] itemArr;
  InvalidatePool ();
  if (pool) delete [] pool;
}


void CListbox::SetMode (EListboxMode _mode, int _itemHeight, int _itemGap) {
  mode = _mode;
  itemHeight = _itemHeight;
  itemGap = _itemGap;
  SetItems (0);
}


void CListbox::SetFormat (TTF_Font *_font, int _hAlign, TColor colGrid,
                    TColor _colLabel, TColor _colBack,
                    TColor _colLabelSelected, TColor _colBackSelected,
                    TColor _colLabelSpecial, TColor _colBackSpecial) {
  SetColors (colGrid);
  font = _font;
  hAlign = _hAlign;
  colLabel = _colLabel;
  colBack = _colBack;
  colLabelSelected = _colLabelSelected;
  colBackSelected = _colBackSelected;
  colLabelSpecial = _colLabelSpecial;
  colBackSpecial = _colBackSpecial;
  ChangedSetup ();
}



// ***** Content management ****


void CListbox::SetItems (int _items) {
  CListboxItem *_itemArr;
  int n;

  // Up-sizing...
  if (_items > items) {
    _itemArr = new CListboxItem[_items];
    for (n = 0; n < items; n++) _itemArr[n] = itemArr[n];
    SETA(itemArr, _itemArr);
  }

  // Wrap up...
  items = _items;
  Changed ();

  // Invalidate pool...
  //   We must do this here, since some events may be pending and processed
  //   before the next render cycle. In this case, the event handler may access
  //   deprecated pool entries.
  InvalidatePool ();
}


void CListbox::SetItem (int idx, const char *_text, const char *_iconName, bool _isSpecial, void *data) {
  CListboxItem *item = &itemArr[idx];

  item->SetLabel (_text, _iconName);
  item->isSpecial = _isSpecial;
  item->data = data;
  ChangedItems (idx);
}


void CListbox::SetItem (int idx, const char *_text, SDL_Surface *_iconSurf, bool _isSpecial, void *data) {
  CListboxItem *item = &itemArr[idx];

  item->SetLabel (_text, _iconSurf);
  item->isSpecial = _isSpecial;
  item->data = data;
  ChangedItems (idx);
}


int CListbox::GetItemLabelWidth (int idx) {
  CListboxItem *item = &itemArr[idx];
  int width = 0;

  if (item->text) {
    width = FontGetWidth (font, item->text);
    if (item->iconSurf || item->iconName) width += itemHeight / 4;    // add space between icon and text
  }
  if (item->iconSurf) width += item->iconSurf->w;
  else if (item->iconName) width += IconGet (item->iconName)->w;
  return width;
}


SDL_Rect CListbox::GetItemRect (int idx) {
  SDL_Rect r;

  if (changed) UpdatePool ();
  if (itemHeight) {
    r.x = 0;
    r.y = idx * (itemHeight + itemGap);
    r.w = area.w;
    r.h = itemHeight;
    return r;
  }
  else {
    ASSERT (idx < poolSize && poolIdx[idx] == idx);
    return *(pool[idx]->GetArea ());
  }
}



// ***** Selection and actions *****


void CListbox::SelectItem (int idx, bool _isSelected) {
  CListboxItem *item = &itemArr[idx];

  //~ INFOF (("### CListbox::SelectItem (%i): %i -> %i", idx, item->isSelected, _isSelected));
  if (idx < 0 || idx >= items) return;
  if (item->isSelected != _isSelected) {
    //~ INFOF (("### CListbox::SelectItem (%i): %i -> %i", idx, item->isSelected, _isSelected));
    if (mode != lmSelectAny && _isSelected) SelectItem (selectedItem, false);   // unselect previously selected item
    item->isSelected = _isSelected;
    selectedItem = _isSelected ? idx : -1;
    ChangedItems (idx);
  }
}


void CListbox::SelectAll (bool _isSelected) {
  if (mode != lmSelectAny) SelectItem (selectedItem, false);   // fast track for 'SelectNone' in all single-selection modes
  else for (int n = 0; n < items; n++) SelectItem (n, _isSelected);
}



// ***** Rendering *****


SDL_Surface *CListbox::RenderItem (CListboxItem *item, int idx, SDL_Surface *surf) {
  SDL_Surface *surfText, *surfIcon;
  SDL_Rect r, rText, rIcon, rLabel;
  TColor colItemLabel, colItemBack;

  //~ INFOF (("###   RenderItem (surf = 0x%lx)", (uint64_t) surf));

  // Sanity ...
  ASSERT (itemHeight > 0);      // for variable-height list boxes, this method must (presently) be overloaded!

  // Determine colors ...
  colItemLabel = item->isSelected ? colLabelSelected : item->isSpecial ? colLabelSpecial : colLabel;
  colItemBack = item->isSelected ? colBackSelected : item->isSpecial ? colBackSpecial : colBack;
  if (item->isSelected && item->isSpecial) {
    // Usually, the 'isSelected' flag dominates the 'isSpecial' flag. However, if both are set and the
    // "selected" color does not differ from the normal color, the "special" color is used anyway.
    // This is independent of the foreground and background color. This way, both the "selected" and the
    // "special" property can be visualized independently, e.g. by indicating the "selected" status with
    // the background and the "special" status with the foreground (label) color.
    if (colLabelSelected == colLabel) colItemLabel = colLabelSpecial;
    if (colBackSelected == colBack) colItemBack = colBackSpecial;
  }

  // Clear surface ...
  if (!surf) surf = CreateSurface (area.w, itemHeight);
  SDL_FillRect (surf, NULL, ToUint32 (colItemBack));

  // Determine text and icon surfaces (both are optional) ...
  if (item->text) surfText = FontRenderText (font, item->text, colItemLabel, colItemBack);
  else surfText = NULL;
  if (item->iconSurf) surfIcon = item->iconSurf;
  else if (item->iconName) surfIcon = IconGet (item->iconName, colItemLabel);
  else surfIcon = NULL;

  // Determine layout ...
  //   Set 'rLabel',  'rText' and 'rIcon' relative to (0,0) in 'surf'.
  if (surfIcon) rIcon = Rect (surfIcon);
  else rIcon = Rect (0, 0, 0, 0);
  if (surfText) {
    rText = Rect (surfText);
    if (surfIcon) {
      // Have both icon and text ...
      rLabel = Rect (0, 0, rIcon.w + rIcon.w / 4 + rText.w, MAX (rIcon.h, rText.h));
      RectAlign (&rIcon, rLabel, -1, 0);    // left-justify icon
      RectAlign (&rText, rLabel, 1, 0);     // right-justify text
    }
    else {
      // Have text, but no icon ...
      rLabel = rText;
    }
  }
  else {
    rText = Rect (0, 0, 0, 0);
    if (surfIcon) {
      // Have icon, but no text ...
      rLabel = rText;
    }
    else {
      // Have neither text nor an icon ...
      rLabel = Rect (0, 0, 0, 0);
    }
  }
  r = Rect (surf);                        // align 'rLabel' in 'surf' ...
  RectGrow (&r, -itemHeight / 4, 0);      //   insert some space to the left & right
  RectAlign (&rLabel, r, hAlign, 0);
  RectMove (&rText, rLabel.x, rLabel.y);  // make 'rText' relative to 'surf'
  RectMove (&rIcon, rLabel.x, rLabel.y);  // make 'rIcon' relative to 'surf'

  // Draw and free sub-surfaces ...
  if (surfText) {
    SurfaceBlit (surfText, NULL, surf, &rText);
    SurfaceFree (surfText);
  }
  if (surfIcon)
    SurfaceBlit (surfIcon, NULL, surf, &rIcon, 0, 0, SDL_BLENDMODE_BLEND);

  // Done ...
  //~ INFOF (("###   RenderItem () -> surf = 0x%lx", (uint64_t) surf));
  return surf;
}



// ***** Callbacks *****


void CListbox::Render (SDL_Renderer *ren) {
  if (changed) UpdatePool ();
  CCanvas::Render (ren);
}


bool CListbox::HandleEvent (SDL_Event *ev) {
  int x, y, wdg, idx;
  bool ret, evIsDown;

  if (CCanvas::HandleEvent (ev)) {
    Changed ();
    return true;
  }
  ret = evIsDown = false;
  if (mode != lmReadOnly) switch (ev->type) {

    case SDL_MOUSEBUTTONDOWN:
      downSelectedItem = selectedItem;        // for 'lmSelectSingle': remember the original selection
      evIsDown = true;
    case SDL_MOUSEMOTION:
      if (!evIsDown && downIdx < 0) break;    // no preceeding "down" event => ignore motion event

      // Get mouse position...
      GetMouseEventPos (ev, &x, &y);

      // In listbox area? ...
      if (!RectContains (&area, x, y)) {
        if (evIsDown) break;      // button down outside => not our event (break with 'ret == false')

        // Have dragged out of the area => cancel and restore selection ...
        switch (mode) {
          case lmReadOnly:
            break;
          case lmActivate:
            SelectNone ();
            break;
          case lmSelectSingle:
            if (downSelectedItem >= 0) SelectItem (downSelectedItem, true);
            else SelectNone ();
            break;
          case lmSelectAny:
            if (downIdx >= 0) SelectItem (downIdx, !itemArr[downIdx].isSelected);
            break;
        }
        downIdx = -1;             // cancel dragging
        ret = true;               // it remains our event
        break;   // case SDL_MOUSEMOTION:
      }

      // Search for the affected item...
      ScreenToWidgetCoords (&x, &y);
      for (wdg = 0; wdg < poolSize; wdg++) {
        idx = poolIdx[wdg];
        if (idx >= 0) if (RectContains (pool[wdg]->GetArea (), x, y)) {
          if (idx != downIdx) {
            // Dragging ...
            if (mode == lmSelectAny && downIdx >= 0)
              SelectItem (downIdx, !itemArr[downIdx].isSelected);
            SelectItem (idx, !itemArr[idx].isSelected);
            if (downIdx >= 0) noLongPush = true;
            downIdx = idx;
          }
          ret = true;
          break;   // for (wdg ...)
        }
      }

      //~ INFOF (("###   evIsDown = %i, downIdx = %i, noLongPush = %i", (int) evIsDown, downIdx, (int) noLongPush));

      // Handle long push...
      if (evIsDown && ev->button.clicks == 2 && downIdx >= 0 && !noLongPush) {
        OnPushed (downIdx, true);
        if (mode == lmActivate) SelectItem (downIdx, false);
        downIdx = -1;
        ret = true;
      }

      break;

    case SDL_MOUSEBUTTONUP:

      // Handle simple push...
      if (downIdx >= 0) {
        OnPushed (downIdx, false);
        if (mode == lmActivate) SelectItem (downIdx, false);
      }
      downIdx = -1;

      // Reset long push flag(s)...
      noLongPush = false;

      break;
  }
  return ret;
}



// ***** Change management *****


void CListbox::ChangedItems (int idx, int num) {
  if (idx < 0) { num += idx; idx = 0; }
  if (idx + num >= items) { num = items - idx; }
  while (num-- > 0) itemArr[idx++].changed = true;
  Changed ();
}



// ***** Helpers *****


// Notes on the pool management:
// * The pool contains widgets to be dynamically assigned to all visible (potentially skipping non-visible) list items.
// * The following invariants must be kept at any time:
// * For variable-height items:
//   - There is a 1:1 correspondance.
//   - A widget is added to the canvas if and only if some item(s) refers to it.
//   - If items are removed, their respective pool items are deleted from the canvas.
// * For fix-height items:
//   - Item 'idx' maps to pool item 'idx % poolSize'.
//   - 'poolSize' is (re-)calculated if 'itemHeight' or 'area' changes.
//     This is implemented by invalidating the complete pool and recalculation in 'UpdatePool'.
//   - Multiple items may refer to the same widget. Hence, in 'UpdatePool' they are all unlinked and selectively added to the canvas again.
//

void CListbox::InvalidatePool () {
  int n;

  if (poolSize <= 0) return;   // fast track for multiple invalidations
  DelAllWidgets ();
  for (n = 0; n < poolSize; n++) if (pool[n]) delete pool[n];
  FREEA(pool);
  FREEA(poolIdx);
  poolSize = 0;
}


void CListbox::UpdatePool () {
  CWidget **_pool;
  SDL_Surface *surf;
  SDL_Rect r;
  int n, idx0, idx1, wdg, y, virtH, _poolSize, *_poolIdx;

  // Extend or shrink pool if necessary ...
  _poolSize = !itemHeight ? items : (area.h / itemHeight + 3);
  //   extend...
  if (_poolSize > poolSize) {
    _pool = new CWidget * [_poolSize];
    _poolIdx = new int [_poolSize];
    for (n = 0; n < poolSize; n++) {
      _pool[n] = pool[n];
      _poolIdx[n] = poolIdx[n];
    }
    for (n = poolSize; n < _poolSize; n++) {
      _pool[n] = new CWidget ();
      _poolIdx[n] = -1;
    }
    SETA(pool, _pool);
    SETA(poolIdx, _poolIdx);
  }
  //   shrink...
  for (n = _poolSize; n < poolSize; n++) {
    DelWidget (pool[n]);
    FREEO(pool[n]);
    poolIdx[n] = -1;
  }
  //   complete...
  poolSize = _poolSize;

  // Determine visible items (primarily fixed-height case)...
  if (itemHeight) {
    idx0 = ((area.y-virtArea.y) / (itemHeight + itemGap)) - 1;
    idx1 = idx0 + poolSize;
    if (idx0 < 0) idx0 = 0;
    if (idx1 >= items) idx1 = items;
  }
  else {
    idx0 = 0;
    idx1 = items;
  }

  // Update items and assign and place widgets as necessary...
  // TBD: The following loops can be accelerated for the case of scrolling
  //      by introducing a 'changedContent' flag
  DelAllWidgets ();
  y = 0;
  for (n = idx0; n < idx1; n++) {
    wdg = n % poolSize;
    if (poolIdx[wdg] != n || itemArr[n].changed) {
      //~ INFOF(("### (Re-)Assigning item %i to widget %i ('surf' was 0x%lx)", n, wdg, (uint64_t) pool[wdg]->GetSurface ()));
      // assign widget...
      if (poolIdx[wdg] != n) {
        // widget has been used for other item before => delete surface
        surf = pool[wdg]->GetSurface ();
        if (surf) {
          SurfaceFree (&surf);
          pool[wdg]->SetSurface (NULL);
        }
        poolIdx[wdg] = n;
      }
      // update surface...
      surf = RenderItem (&itemArr[n], n, pool[wdg]->GetSurface ());
      //~ INFOF (("###   rendered surface 0x%lx", surf));
      pool[wdg]->SetSurface (surf);
      // place widget...
      r = Rect(surf);
      r.x = 0;
      if (itemHeight) r.y = n * (itemHeight + itemGap);
      else {
        r.y = y;
        y += r.h + itemGap;
      }
      pool[wdg]->SetArea (r);
      // done...
      itemArr[n].changed = false;
      //~ INFOF (("###   -> surface = 0x%lx", (uint64_t) pool[wdg]->GetSurface ()));
    }
    //~ INFOF(("### Adding widget %i (%i, %i, %i, %i)", wdg, pool[wdg]->GetArea ()->x, pool[wdg]->GetArea ()->y, pool[wdg]->GetArea ()->w, pool[wdg]->GetArea ()->h));
    pool[wdg]->SetTextureBlendMode (sdlBlendMode);
      // Inherit the blend mode from the containing widget:
      // Whole widget is drawn in a transparent way <=> All individual items are drawn transparently.
    AddWidget (pool[wdg]);
  }

  // Update 'virtArea'...
  virtH = itemHeight ? (items * (itemHeight+itemGap) - itemGap) : (y - itemGap);
  if (virtArea.h != virtH || virtArea.w != area.w) {   // changed?
    SetVirtArea (Rect (virtArea.x, virtArea.y, area.w, virtH));
    UpdatePool ();    // Repeat the process, since other items may have become visible.
  }
  //~ INFOF(("### virtArea = (%i, %i, %i, %i)", virtArea.x, virtArea.y, area.w, virtH));

  // Complete...
  changed = false;
}





// *************************** CMenu *******************************************


#define MENU_FRAME_X 16
#define MENU_FRAME_Y 8



void CMenu::Setup (SDL_Rect _rContainer, int _hAlign, int _vAlign, TColor color, TTF_Font *_font) {
  SDL_Surface *surf;
  SDL_Rect r;

  rContainer = _rContainer;
  hAlign = _hAlign;
  vAlign = _vAlign;
  if (!_font) _font = MENU_DEFAULT_FONT;
  SetMode (lmActivate, 7 * FontGetLineSkip (_font) / 4, 1);
  SetFormat (_font, -1, DARK_GREY, WHITE, BLACK, GREY, DARK_GREY);
  rNoCancel = Rect (0, 0, -1, -1);    // indicates "undefined"

  // Create frame texture...
  surf = CreateSurface (1, 64);
  for (r = Rect (0, 0, 1, 1); r.y < 64; r.y++)
    SDL_FillRect (surf, &r, ToUint32 (ColorBrighter (color, r.y - 32)));
  TextureSet (&texFrame, CreateTexture (surf));
  SDL_FreeSurface (surf);
}


void CMenu::SetItems (const char *_itemStr) {
  char *p;
  int n, items;

  itemStr.Set (_itemStr);

  // Pass 1: Count items...
  items = 1;
  for (p = (char *) itemStr.Get (); p[0]; p++) if (p[0] == '|') items++;
  CListbox::SetItems (items);

  // Pass 2: Set items...
  n = 1;
  for (p = (char *) itemStr.Get (); p[0]; p++) if (p[-1] == '|') {
    p[-1] = '\0';
    SetItem (n++, p);
  }
  SetItem (0, itemStr.Get ());    // first item
}



// ***** Running the menu *****


void CMenu::Start (CScreen *_screen) {
  int n, width, maxWidth;

  if (IsRunning ()) return;

  // Init variables...
  hadLongPush = false;

  // Determine geometry...
  //   ... find longest label ...
  maxWidth = 0;
  for (n = 0; n < GetItems (); n++) {
    width = GetItemLabelWidth (n);
    if (width > maxWidth) maxWidth = width;
  }
  maxWidth += FontGetLineSkip (font);
  //   ... determine frame rectangle ...
  rFrame = Rect (0, 0, maxWidth + itemHeight/2 + 2 * MENU_FRAME_X,
                       GetItems () * (itemHeight + 1) - 1 + 2 * MENU_FRAME_Y);
  RectAlign (&rFrame, rContainer, hAlign, vAlign);
  //   ... limit frame vertically ...
  //   (Horizontal oversize is left to the user, vertical oversize will be resolved by introducing scrollbars.)
  if (rFrame.y < rContainer.y) rFrame.y = rContainer.y;
  if (rFrame.y + rFrame.h > rContainer.y + rContainer.h) rFrame.h = rContainer.y + rContainer.h - rFrame.y;
  //   ... determine listbox area ...
  SetArea (Rect (rFrame.x + MENU_FRAME_X, rFrame.y + MENU_FRAME_Y, rFrame.w - 2 * MENU_FRAME_X, rFrame.h - 2 * MENU_FRAME_Y));
  //   ... set no-cancel area if not given explicitly before ...
  if (rNoCancel.w < 0) rNoCancel = rFrame;

  // Activate widget...
  CModalWidget::Start (_screen);
}



// ***** Callbacks *****


void CMenu::Render (SDL_Renderer *ren) {
  TextureRender (texFrame, NULL, &rFrame);
  CListbox::Render (ren);
}


bool CMenu::HandleEvent (SDL_Event *ev) {
  if (CListbox::HandleEvent (ev)) return true;
  return CModalWidget::HandleEvent (ev);
}


void CMenu::OnPushed (int idx, bool longPush) {
  SetStatus (idx);
  hadLongPush = longPush;
  Stop ();
}



// ***** High-level functions *****


int RunMenu (const char *_itemStr, SDL_Rect _rContainer, int _hAlign, int _vAlign,
             TColor color, TTF_Font *_font, bool *retLongPush) {
  CMenu menu;
  int ret;

  menu.Setup (_rContainer, _hAlign, _vAlign, color, _font);
  ret = menu.Run (CScreen::ActiveScreen (), _itemStr);
  if (retLongPush) *retLongPush = menu.GetStatusLongPush ();
  return ret;
}





// *************************** CMessageBox *************************************


#define MSGBOX_SPACE_X 32
#define MSGBOX_SPACE_Y 32
#define MSGBOX_BUTTON_MINWIDTH 160


BUTTON_TRAMPOLINE (CbMessageBoxButtonPushed, CMessageBox, OnButtonPushed);


void CMessageBox::Setup (const char *title, int contentW, int contentH, int _buttons, CButton **_buttonArr, TColor color, int titleHAlign) {
  SDL_Surface *surfTitle;
  SDL_Rect r, rWindow, *layout;
  int n;

  // Store parameters...
  buttonArr = _buttonArr;
  buttons = _buttons;

  // Draw title...
  surfTitle = title ? FontRenderText (FontGet (fntBold, 32), title, WHITE) : NULL;

  // Determine size and set area...
  rWindow.x = rWindow.y = 0;
  rWindow.w = contentW;
  rWindow.h = contentH;
  if (surfTitle) {
    rWindow.w = MAX (rWindow.w, surfTitle->w);
    rWindow.h += MSGBOX_SPACE_Y + surfTitle->h;
  }
  if (buttons) {
    rWindow.w = MAX (rWindow.w, _buttons * (MSGBOX_BUTTON_MINWIDTH));
    rWindow.h += MSGBOX_SPACE_Y + UI_BUTTONS_HEIGHT;
  }

  RectGrow (&rWindow, 2 * MSGBOX_SPACE_X, MSGBOX_SPACE_Y);
  if (rWindow.w > UI_RES_X) rWindow.w = UI_RES_X;
  if (rWindow.h > UI_RES_Y) rWindow.h = UI_RES_Y;
  RectCenter (&rWindow, RectScreen ());
  SetArea (rWindow);

  rContent = Rect (MSGBOX_SPACE_X, MSGBOX_SPACE_Y, rWindow.w - 2 * MSGBOX_SPACE_X, rWindow.h - 2 * MSGBOX_SPACE_Y);
  RectMove (&rContent, rWindow.x, rWindow.y);
  if (surfTitle) {
    rContent.y += surfTitle->h + MSGBOX_SPACE_Y;
    rContent.h -= surfTitle->h + MSGBOX_SPACE_Y;
  }
  if (buttons)
    rContent.h -= UI_BUTTONS_HEIGHT + MSGBOX_SPACE_Y;

  // Render background...
  SurfaceSet (&surface, CreateSurface (area.w, area.h));
  r = Rect (surface);
  for (n = 0; n < 64; n++) {
    r.h = (n + 1) * surface->h / 64 - r.y;
    SDL_FillRect (surface, &r, ToUint32 (ColorBrighter (color, 32 - n)));
    r.y += r.h;
  }

  // Render title...
  if (surfTitle) {
    r = Rect (surface);
    RectGrow (&r, -MSGBOX_SPACE_X, -MSGBOX_SPACE_Y);
    SurfaceBlit (surfTitle, NULL, surface, &r, titleHAlign, -1, SDL_BLENDMODE_BLEND);
    SurfaceFree (surfTitle);
  }
  ChangedSurface ();

  // Place buttons...
  if (buttons) {
    layout = LayoutRowEqually (Rect (rWindow.x + MSGBOX_SPACE_X, rWindow.y + rWindow.h - MSGBOX_SPACE_Y - UI_BUTTONS_HEIGHT, rWindow.w - 2 * MSGBOX_SPACE_X, UI_BUTTONS_HEIGHT), buttons);
    for (n = 0; n < buttons; n++) {
      buttonArr[n]->SetArea (layout[buttons-1 - n]);
      buttonArr[n]->SetCbPushed (CbMessageBoxButtonPushed, this);
    }
    free (layout);
  }
}


CButton *CMessageBox::GetStdButton (EMessageButtonId buttonId) {
  // TRANSLATORS: Standard buttons for message boxes
  static const char *buttonText [] = { N_("Cancel"), N_("OK"), N_("No"), N_("Yes") };
  static const SDL_Keycode buttonHotkey[] = { SDLK_ESCAPE, SDLK_RETURN, SDLK_n, SDLK_j };

  stdButtons[buttonId].SetColor (buttonId == mbiCancel ? DARK_RED : DARK_GREEN);
  stdButtons[buttonId].SetLabel (_(buttonText[buttonId]));
  stdButtons[buttonId].SetHotkey (buttonHotkey[buttonId]);
  return &stdButtons[buttonId];
}


void CMessageBox::Setup (const char *title, int contentW, int contentH, int buttonMask, TColor color) {
  int n, _buttons;

  _buttons = 0;
  for (n = 0; n < mbiEND; n++) if (buttonMask & (1 << n))
    stdButtonArr[_buttons++] = GetStdButton ((EMessageButtonId) n);
  Setup (title, contentW, contentH, _buttons, stdButtonArr, color);
}


void CMessageBox::Setup (const char *title, const char *text, SDL_Surface *icon, int buttonMask, int hAlign, TTF_Font *font) {
  SDL_Surface *surfText;
  SDL_Rect r;
  int w, h, d;

  //~ INFOF (("### CMessageBox::Setup (): title = '%s', text = '%s'", title, text));

  if (!font) font = MSGBOX_DEFAULT_FONT;
  surfText = text ? TextRender (text, CTextFormat (font, WHITE, TRANSPARENT, hAlign)) : NULL;
  h = 0;
  w = 0;
  if (surfText) { w = surfText->w; h = surfText->h; }
  if (icon) { w += MSGBOX_SPACE_X + icon->w; h = MAX (h, icon->h); }

  Setup (title, w, h, buttonMask);

  r = rContent;
  RectMove (&r, -area.x, -area.y);
  //~ INFOF (("### r = (x = %i, y = %i, w = %i, h = %i)", r.x, r.y, r.w, r.h));
  if (icon) {
    if (surfText) {
      d = (rContent.w - icon->w - surfText->w) / 3;
      r.x += d;
      SurfaceBlit (icon, NULL, surface, &r, -1, 0, SDL_BLENDMODE_BLEND);
      r.x += icon->w;
      r.w -= (d + icon->w);
    }
    else
      SurfaceBlit (icon, NULL, surface, &r, 0, 0, SDL_BLENDMODE_BLEND);
  }
  if (surfText) {
    SurfaceBlit (surfText, NULL, surface, &r, 0, 0, SDL_BLENDMODE_BLEND);
    SurfaceFree (surfText);
  }
}


void CMessageBox::Start (CScreen *_screen) {
  SystemActiveLock ("_message");
  screenHasKeyboard = _screen->HasKeyboard ();
  _screen->SetKeyboard (false);
  CModalWidget::Start (_screen);
  for (int n = 0; n < buttons; n++) _screen->AddWidget (buttonArr[n], 1);
}


void CMessageBox::Stop () {
  SystemActiveUnlock ("_message");
  if (screen) {
    screen->SetKeyboard (screenHasKeyboard);
    for (int n = 0; n < buttons; n++) screen->DelWidget (buttonArr[n]);
  }
  CModalWidget::Stop ();
}


void CMessageBox::OnButtonPushed (CButton *button, bool) {
  int n;

  for (n = 0; buttonArr[n] != button; n++) ASSERT (n < buttons);
  SetStatus (n);
  Stop ();
}


int RunMessageBox (const char *title, const char *text, int buttonMask, SDL_Surface *icon, int hAlign, TTF_Font *font) {
  CMessageBox msgBox;

  msgBox.Setup (title, text, icon, buttonMask, hAlign, font);
  return msgBox.Run (CScreen::ActiveScreen ());
}


int RunInfoBox (const char *title, const char *text, SDL_Surface *icon, int hAlign, TTF_Font *font) {
  return RunMessageBox (title, text, mbmOk, icon, hAlign, font);
}


int RunInfoBox (const char *text, SDL_Surface *icon, int hAlign, TTF_Font *font) {
  return RunInfoBox (_("Information"), text, icon, hAlign, font);
}


int RunWarnBox (const char *title, const char *text, SDL_Surface *icon, int hAlign, TTF_Font *font) {
  return RunMessageBox (title, text, mbmOk, icon, hAlign, font);
}


int RunWarnBox (const char *text, SDL_Surface *icon, int hAlign, TTF_Font *font) {
  return RunWarnBox (_("Warning"), text, icon, hAlign, font);
}


int RunErrorBox (const char *title, const char *text, SDL_Surface *icon, int hAlign, TTF_Font *font) {
  return RunMessageBox (title, text, mbmOk, icon, hAlign, font);
}


int RunErrorBox (const char *text, SDL_Surface *icon, int hAlign, TTF_Font *font) {
  return RunErrorBox (_("Error"), text, icon, hAlign, font);
}


int RunSureBox (const char *title, const char *text, SDL_Surface *icon, int hAlign, TTF_Font *font) {
  return RunMessageBox (title, text, mbmOkCancel, icon, hAlign, font);
}


int RunSureBox (const char *text, SDL_Surface *icon, int hAlign, TTF_Font *font) {
  return RunSureBox (_("Sure?"), text, icon, hAlign, font);
}


int RunQueryBox (const char *title, const char *text, SDL_Surface *icon, int hAlign, TTF_Font *font) {
  return RunMessageBox (title, text, mbmYesNoCancel, icon, hAlign, font);
}


int RunQueryBox (const char *text, SDL_Surface *icon, int hAlign, TTF_Font *font) {
  return RunQueryBox (_("Question"), text, icon, hAlign, font);
}


CMessageBox *StartMessageBox (const char *title, const char *text, SDL_Surface *icon, int buttonMask, int hAlign, TTF_Font *font) {
  CMessageBox *msgBox = new CMessageBox ();
  msgBox->Setup (title, text, icon, buttonMask, hAlign, font);
  msgBox->Start (CScreen::ActiveScreen ());
  UiIterate ();
  return msgBox;

}


void StopMessageBox (CMessageBox *msgBox) {
  if (!msgBox) return;    // sanity
  msgBox->Stop ();
  delete msgBox;
}





// *************************** CInputLine **************************************


#define INPUT_SPACE_X 4     // space in the beginning and end of line
#define INPUT_SPACE_Y 2     // space at top/bottom of input line (affects cursor)
#define INPUT_CURSOR_W 4    // cursor width


class CUndoState {
  public:
    CString input;
    int mark0, markD;
    class CUndoState *next;
};


void CInputLine::Setup (int fontSize) {
  font = FontGet (fntMono, fontSize);   // presently, only mono-type fonts are supported by 'Render' and 'GetCharOfMouseEvent'
  //~ INFOF(("### FontWidth('') = %i, FontWidth('X') = %i, FontWidth('XX') = %i, FontWidth('I') = %i, FontWidth('II') = %i",
          //~ FontGetWidth (font, ""), FontGetWidth (font, "X"), FontGetWidth (font, "XX"), FontGetWidth (font, "I"), FontGetWidth (font, "II")));
  charWidth = FontGetWidth (font, "7");
    // WORKAROUND [2019-07-29]:
    //    For some strange reason, after upgrading to Debian 10 (Buster), the string "X"
    //    no longer worked as a reference character. This is strange, since a) SDL2 is
    //    still used pre-compiled and statically linked from 'external/sdl' and b)
    //    the TTF fonts were not changed either.
  wdgMain.SetCursorFormat (RED, SDL_BLENDMODE_MOD);
  SetInput (NULL);    // clear input line
  AddWidget (&wdgMain);
  ClearHistory ();
  ChangedContent ();
  ChangedMark ();
}


void CInputLine::SetInput (const char *_inputStr, int _mark0, int _markD) {
  input.SetAsIso8859 (_inputStr);
  ClearHistory ();
  ChangedInput ();
  SetMark (_mark0, _markD);
}


void CInputLine::ClearHistory () {
  ClearStateList (&undoFirst);
  ClearStateList (&redoFirst);
}


bool CInputLine::InputModified () {
  if (!undoFirst) return false;
  return undoFirst->next != NULL;
}



// ***** Editing *****


void CInputLine::SetMark (int _mark0, int _markD) {

  // Validate new mark...
  if (_mark0 < 0) _mark0 = 0;
  if (_mark0 > inputLen) _mark0 = inputLen;
  if (_mark0 + _markD < 0) _markD = -mark0;
  if (_mark0 + _markD > inputLen) _markD = inputLen - _mark0;

  // Set mark, if changed...
  if (_mark0 != mark0 || _markD != markD) {
    mark0 = _mark0;
    markD = _markD;
    ChangedMark ();
  }
}


void CInputLine::MoveMark (int _mark0) {
  int _markD = markD - _mark0 + mark0;

  // Validate new mark (only what is not done later in 'SetMark')...
  if (_mark0 < 0) _markD += _mark0;
  if (_mark0 > inputLen) _markD += (_mark0 - inputLen);

  SetMark (_mark0, _markD);
}


void CInputLine::InsChar (char c) {
  DelMarked ();
  input.Insert (mark0, c);
  mark0++;
  ChangedInput ();
}


void CInputLine::InsText (const char *txt, int chars) {
  DelMarked ();
  if (chars < 0) chars = strlen (txt);
  input.Insert (mark0, txt, chars);
  mark0 += chars;
  ChangedInput ();
  ChangedMark ();
}


void CInputLine::DelMarked () {
  if (markD != 0) {
    if (markD < 0) {
      mark0 += markD;
      markD = -markD;
    }
    input.Del (mark0, markD);
    markD = 0;
    ChangedInput ();
    ChangedMark ();
  }
}


void CInputLine::DelChar (int pos) {
  if (pos >= 0 && pos < inputLen) {
    input.Del (pos, 1);
    if (mark0 > pos) mark0--;
    ChangedInput ();
    ChangedMark ();
  }
}



// ***** Clipboard *****


void CInputLine::ClipboardCopy () {
  CString clip;
  int n0, dn;

  if (markD != 0) {
    if (markD > 0) { n0 = mark0; dn = markD; }
    else { n0 = mark0 + markD; dn = -markD; }
    clip.Set (&input, n0, dn);
    SDL_SetClipboardText (ToUtf8 (clip.Get ()));
  }
}


void CInputLine::ClipboardPaste () {
  InsText (ToIso8859 (SDL_GetClipboardText ()));
}



// ***** Undo / Redo *****


void CInputLine::ClearStateList (class CUndoState **pList) {
  CUndoState *s;

  while (*pList) {
    s = *pList;
    *pList = (*pList)->next;
    delete s;
  }
}


void CInputLine::PushInput () {
  // Push the input line contents to the undo state.
  CUndoState *s;

  s = new CUndoState ();
  s->input = input;
  s->mark0 = mark0;
  s->markD = markD;
  s->next = undoFirst;
  undoFirst = s;
  //~ INFOF (("### PushInput: '%s', %i/%i", s->input.Get (), s->mark0, s->markD));
}


void CInputLine::PushMark () {
  // Push a new mark to the undo state.
  if (!undoFirst) PushInput ();
  undoFirst->mark0 = mark0;
  undoFirst->markD = markD;
  //~ INFOF (("### PushMark: '%s', %i/%i", undoFirst->input.Get (), undoFirst->mark0, undoFirst->markD));
}


void CInputLine::SetState (class CUndoState *s) {
  // Set the widget to a previously stored undo/redo state.
  if (s) {
    //~ INFOF (("### SetState: '%s', %i/%i", s->input.Get (), s->mark0, s->markD));
    input = s->input;
    inputLen = input.Len ();
    ChangedContent ();    // no call to 'ChangedInput' here as it would mess up the undo/redo structures
    SetMark (s->mark0, s->markD);
  }
  else SetInput (NULL);
}


void CInputLine::Undo () {
  CUndoState *s;

  if (!undoFirst) return;       // Undo list is empty
  if (!undoFirst->next) return; // Undo list is empty (there is only one item in the list, which is the current state)

  // The first item in the undo list is the current state => move that to the redo list...
  s = undoFirst;
  undoFirst = s->next;
  s->next = redoFirst;
  redoFirst = s;

  // The next item is the one to switch to...
  SetState (undoFirst);
}


void CInputLine::Redo () {
  CUndoState *s;

  if (redoFirst) {
    // move the first redo item to the undo list...
    s = redoFirst;
    redoFirst = s->next;
    s->next = undoFirst;
    undoFirst = s;
    SetState (s);
  }
}



// ***** Suggestions *****


void CInputLine::SetSuggestion (int _suggPos, const char *_suggText, int _suggMark0, int _suggMarkD) {
  ASSERT (_suggPos <= mark0);
  suggPos = _suggPos;
  suggText.Set (_suggText);
  suggMark0 = _suggMark0;
  suggMarkD = _suggMarkD;
  ChangedContent ();
}


void CInputLine::ApplySuggestion () {
  markD = suggPos - mark0;
  DelMarked ();
  InsText (suggText);
  ClearSuggestion ();
}



// ***** Change management *****


void CInputLine::ChangedInput () {
  inputLen = input.Len ();
  PushInput ();
  ClearStateList (&redoFirst);
  ChangedContent ();
}

void CInputLine::ChangedMark () {
  PushMark ();
  CheckSuggestion ();
  changedMark = true;
  Changed ();
}



// ***** Callbacks *****


void CInputLine::Render (SDL_Renderer *ren) {
  SDL_Rect r;
  int n, w, h, lh, suggX = 0, m0, m1;

  // Handle changed content...
  if (changedContent) {

    // Determine width and height and (re-)create surface ...
    lh = area.h / 2;
    h = 2 * lh;
    w = FontGetWidth (font, input.Get ());
    if (suggPos >= 0) {
      suggX = FontGetWidth (font, input.Get (), suggPos);
      n = suggX + FontGetWidth (font, suggText.Get ());
      if (n > w) w = n;
    }
    w += 2 * INPUT_SPACE_X;    // space on left & right
    if (w < area.w) w = area.w;
    if (surfMain) {
      if (surfMain->w > w) w = surfMain->w;
      if (surfMain->w != w || surfMain->h != h) SurfaceFree (&surfMain);
    }
    if (!surfMain) surfMain = CreateSurface (w, h);

    // Render content...
    r = Rect (0, 0, w, lh);
    TextRender (ToUtf8 (input.Get ()), CTextFormat (font, BLACK, WHITE, -1, 0, INPUT_SPACE_X, 0), surfMain, &r);
    if (suggPos >= 0) {
      r.x += suggX;
      r.y = lh;
      TextRender (ToUtf8 (suggText.Get ()), CTextFormat (font, GREY, BLACK, -1, 0, INPUT_SPACE_X, 0), surfMain, &r);
    }

    // Set content...
    wdgMain.SetArea (Rect (0, 0, w, h));
    wdgMain.SetSurface (surfMain);
    SetVirtArea (Rect (virtArea.x, virtArea.y, w, h));
    changedContent = false;
  }

  // Handle changed mark...
  if (changedMark) {
    lh = area.h / 2;
    if (markD < 0) { m0 = mark0 + markD; m1 = mark0; }
    else { m0 = mark0; m1 = mark0 + markD; }
    r.x = INPUT_SPACE_X + m0 * charWidth - INPUT_CURSOR_W / 2;
    r.w = (m1 - m0) * charWidth + INPUT_CURSOR_W;
    r.y = INPUT_SPACE_Y;
    r.h = lh - 2 * INPUT_SPACE_Y;
    wdgMain.SetCursor (r);
    ScrollIn (r);
    ScrollIn (Rect (INPUT_SPACE_X + mark0 * charWidth - INPUT_CURSOR_W / 2, 0, INPUT_CURSOR_W, lh));
      // moves in actual cursor, if the whole marking does not fit into the view
    changedMark = false;
  }

  // Call super-class...
  CCanvas::Render (ren);
}


int CInputLine::GetCharOfMouseEvent (SDL_Event *ev) {
  int x, y;

  wdgMain.GetMouseEventPos (ev, &x, &y);
  if (RectContains (wdgMain.GetArea (), x, y))
    return ((x - INPUT_SPACE_X) / charWidth);
  else
    return -1;
}


bool CInputLine::HandleEvent (SDL_Event *ev) {
  SDL_Keycode key;
  int modState, _mark0, _mark1;
  bool ret;

  if (CCanvas::HandleEvent (ev)) return true;
  ret = false;
  switch (ev->type) {
    case SDL_MOUSEBUTTONDOWN:
      if ( (_mark0 = GetCharOfMouseEvent (ev)) < 0) break;
      if (ev->button.clicks == 2) {
        while (_mark0 > 0 && input[_mark0-1] != ' ') _mark0--;
        _mark1 = _mark0;
        while (_mark1 < inputLen && input[_mark1] != ' ') _mark1++;
      }
      else _mark1 = _mark0;
      SetMark (_mark0, _mark1 - _mark0);
      break;
    case SDL_MOUSEMOTION:
      if ( (_mark0 = GetCharOfMouseEvent (ev)) < 0) break;
      MoveMark (_mark0);
      break;
    case SDL_TEXTINPUT:
      InsText (ToIso8859 (ev->text.text));
      //~ INFOF (("### CInputLine: SDL_TEXTINPUT: '%s' (%02x %02x %02x %02x)", ev->text.text, (uint8_t) ev->text.text[0], (uint8_t) ev->text.text[1], (uint8_t) ev->text.text[2], (uint8_t) ev->text.text[3]));
      ret = true;
      break;
    case SDL_KEYDOWN:
      key = ev->key.keysym.sym;
      modState = ev->key.keysym.mod;
      //~ INFOF (("### CInputLine: SDL_KEYDOWN: '%s'", SDL_GetKeyName (key)));
      if (key >= SDLK_a && key <= SDLK_z && !(modState & KMOD_CTRL)) break;   // ignore letters without 'Ctrl'
      ret = true;
      switch (key) {
        case SDLK_BACKSPACE:
          if (markD != 0) DelMarked ();
          else DelChar (mark0 - 1);
          break;
        case SDLK_INSERT:
        case SDLK_v:
          if (key == SDLK_INSERT && (modState & KMOD_CTRL)) ClipboardCopy ();
          else ClipboardPaste ();
          break;
        case SDLK_DELETE:
          if (markD != 0) ClipboardCut ();
          else DelChar (mark0);
          break;
        case SDLK_c:
          ClipboardCopy ();
          break;
        case SDLK_x:
          ClipboardCut ();
          break;

        case SDLK_LEFT:
          _mark0 = mark0;
          if (modState & KMOD_CTRL) {
            while (_mark0 > 0 && input.Get () [_mark0 - 1] == ' ') _mark0--;
            while (_mark0 > 0 && input.Get () [_mark0 - 1] != ' ') _mark0--;
          }
          else _mark0--;
          if (modState & KMOD_SHIFT) MoveMark (_mark0);
          else SetMark (markD ? MIN (mark0, mark0 + markD) : _mark0);
          break;
        case SDLK_RIGHT:
          _mark0 = mark0;
          if (modState & KMOD_CTRL) {
            while (_mark0 < inputLen && input.Get () [_mark0] != ' ') _mark0++;
            while (_mark0 < inputLen && input.Get () [_mark0] == ' ') _mark0++;
          }
          else _mark0++;
          if (modState & KMOD_SHIFT) MoveMark (_mark0);
          else SetMark (markD ? MAX (mark0, mark0 + markD) : _mark0);
          break;
        case SDLK_HOME:
          if (modState & KMOD_SHIFT) MoveMark (0);
          else SetMark (0);
          break;
        case SDLK_END:
          if (modState & KMOD_SHIFT) MoveMark (inputLen);
          else SetMark (inputLen);
          break;
        case SDLK_a:
          if (modState & KMOD_CTRL) SetMark (0, inputLen);
          break;

        case SDLK_z:
          Undo ();
          break;
        case SDLK_y:
          Redo ();
          break;

        case SDLK_TAB:
          // TBD
          break;
        default:
          ret = false;
      }
      break;
    //~ case SDL_TEXTEDITING:
      //~ INFOF (("### SDL_TEXTEDITING: '%s', %i/%i", ev->edit.text, ev->edit.start, ev->edit.length));
      //~ ret = true;
      //~ break;
  }
  return ret;
}





// *************************** CInputScreen ************************************


#define INPUT_HEIGHT 96


BUTTON_TRAMPOLINE (CbInputScreenOnButtonPushed, CInputScreen, OnButtonPushed)


void CInputScreen::Setup (const char *inputPreset, TColor color, int userBtns, CButton **userBtnList, const int *userBtnWidth) {
  SDL_Rect *layout;
  CButton *btn;
  int *format;
  int n;

  // Input line...
  SetKeyboard (true);   // enable on-screen keyboard
  wdgInput.Setup ();
  wdgInput.SetArea (Rect (0, 0, UI_RES_X, INPUT_HEIGHT));
  if (inputPreset) wdgInput.SetInput (inputPreset);
  AddWidget (&wdgInput);

  // Buttons...
  format = MALLOC(int, 7 + userBtns);
  for (n = 0; n < 7 + userBtns; n++) format[n] = -1;
  if (userBtnWidth)
    for (n = 0; n < userBtns; n++) format[n+1] = userBtnWidth[n];
  layout = LayoutRow (Rect (0, INPUT_HEIGHT + 32, UI_RES_X, UI_BUTTONS_HEIGHT), format, 7 + userBtns);
  FREEP (format);

  n = 0;
  btnBack.Set (layout[n++], GREY, IconGet ("ic-back-48"));
  btnBack.SetHotkey (SDLK_ESCAPE);
  btnBack.SetCbPushed (CbInputScreenOnButtonPushed, this);
  AddWidget (&btnBack);

  while (n < userBtns + 1) {
    btn = userBtnList[n - 1];
    btn->SetArea (layout[n]);
    btn->SetColor (color);
    btn->SetCbPushed (CbInputScreenOnButtonPushed, this);
    AddWidget (btn);
    n++;
  }

  btnUndo.Set (layout[n++], GREY, IconGet ("ic-undo-48"));
  btnUndo.SetCbPushed (CbInputScreenOnButtonPushed, this);
  AddWidget (&btnUndo);

  btnRedo.Set (layout[n++], GREY, IconGet ("ic-redo-48"));
  btnRedo.SetCbPushed (CbInputScreenOnButtonPushed, this);
  AddWidget (&btnRedo);

  btnCut.Set (layout[n++], GREY, IconGet ("ic-cut-48"));
  btnCut.SetCbPushed (CbInputScreenOnButtonPushed, this);
  AddWidget (&btnCut);

  btnCopy.Set (layout[n++], GREY, IconGet ("ic-copy-48"));
  btnCopy.SetCbPushed (CbInputScreenOnButtonPushed, this);
  AddWidget (&btnCopy);

  btnPaste.Set (layout[n++], GREY, IconGet ("ic-paste-48"));
  btnPaste.SetCbPushed (CbInputScreenOnButtonPushed, this);
  AddWidget (&btnPaste);

  btnOk.Set (layout[n++], GREY, "OK");
  btnOk.SetHotkey (SDLK_RETURN);
  btnOk.SetCbPushed (CbInputScreenOnButtonPushed, this);
  AddWidget (&btnOk);

  FREEP(layout);
}


void CInputScreen::OnButtonPushed (CButton *btn, bool longPush) {

  // Button "Back"...
  if (btn == &btnBack) {
    if (!wdgInput.InputModified ()) Return ();
    else if (RunSureBox (_("Discard changes?")) == 1) Return ();
  }

  // Buttons "Cut", "Copy" and "Paste"...
  else if (btn == &btnUndo)   wdgInput.Undo ();
  else if (btn == &btnRedo)   wdgInput.Redo ();
  else if (btn == &btnCut)    wdgInput.ClipboardCut ();
  else if (btn == &btnCopy)   wdgInput.ClipboardCopy ();
  else if (btn == &btnPaste)  wdgInput.ClipboardPaste ();

  // Button "OK" ...
  else if (btn == &btnOk) Commit ();

  // User button ...
  else OnUserButtonPushed (btn, longPush);
}





// *************************** CSlider *****************************************


CSlider::CSlider () {
  sliderW = barH = 0;
  colSlider = colBarLower = colBarUpper = colBack = WHITE;
  isDown = continuousUpdate = false;
  redraw = true;
  val0 = val1 = val = 0;
  slider0 = downX = 0;
  cbValueChanged = NULL;
  cbValueChangedData = NULL;
}


CSlider::~CSlider () {
  SurfaceFree (&surface);
}


void CSlider::SetFormat (TColor _colSlider, TColor _colBarLower, TColor _colBarUpper, TColor _colBack, int _sliderW, int _barH) {
  sliderW = _sliderW;
  barH = _barH;
  colSlider = _colSlider;
  colBarLower = _colBarLower;
  colBarUpper = _colBarUpper;
  colBack = _colBack;
  ChangedSurface ();
}


void CSlider::SetArea (SDL_Rect _area) {
  CWidget::SetArea (_area);
  SetValue (val);
  Changed ();
}


void CSlider::SetInterval (int _val0, int _val1, bool _continuousUpdate) {
  val0 = _val0;
  val1 = _val1;
  continuousUpdate = _continuousUpdate;
  SetValue (val, false);
  ChangedSurface ();
}


void CSlider::SetValue (int _val, bool callOnValueChanged) {
  int _slider0, lastVal;

  // Skip if non-continuous user interaction is in progress...
  //   Presently, there is no escape mechanism for a non-continuous user interaction.
  //   Hence, we ignore the value set here since it will be overwritten by the commited
  //   user value. If the user interaction can be canceled (e.g. by dragging away from the
  //   slider), we must change the strategy, store the value set here and restore that
  //   on cancelation.
  if (isDown) return;

  // Clip value and return on no change...
  if (_val < val0) _val = val0;
  else if (_val > val1) _val = val1;

  // Calculate new slider values...
  if (val1 == val0) _slider0 = 0;   // avoid division by zero
  else _slider0 = ((area.w - sliderW) * (_val - val0) + (val1 - val0) / 2) / (val1 - val0);
  if (_slider0 != slider0) {
    slider0 = _slider0;
    ChangedSurface ();
  }

  // Take over value & notify callback...
  if (_val != val) {
    lastVal = val;
    val = _val;
    if (callOnValueChanged) OnValueChanged (_val, lastVal);
  }
}


void CSlider::SetSlider0 (int _slider0, bool updateVal) {
  int lastVal;

  //~ INFOF (("### CSlider::SetSlider0 (_slider0 = %i, updateVal = %i)", _slider0, (int) updateVal));

  // Clip slider position and return on no change...
  if (_slider0 < 0) _slider0 = 0;
  else if (_slider0 > area.w - sliderW) _slider0 = area.w - sliderW;

  // Set slider...
  if (_slider0 != slider0) {
    slider0 = _slider0;
    ChangedSurface ();
  }

  // Calculate new value (must be consistent to 'SetValue' as much as possible)...
  if (updateVal && area.w - sliderW > 0) {
    lastVal = val;
    val = val0 + (slider0 * (val1 - val0) + (area.w - sliderW) / 2) / (area.w - sliderW);
    if (lastVal != val) OnValueChanged (val, lastVal);
    //~ INFOF (("###   val: %i -> %i", lastVal, val));
  }
}


void CSlider::OnValueChanged (int val, int lastVal) {
  if (cbValueChanged) cbValueChanged (this, val, lastVal, cbValueChangedData);
}


SDL_Surface *CSlider::GetSurface () {
  SDL_Rect r;
  int n;

  if (redraw) {
    ASSERT (area.w > 0 && area.h > 0);

    // Create surface...
    SurfaceSet (&surface, CreateSurface (area.w, area.h));

    // Clear background...
    SDL_FillRect (surface, NULL, ToUint32 (colBack));

    // Draw bars...
    r = Rect (0, (area.h - barH) / 2, slider0, barH);
    SDL_FillRect (surface, &r, ToUint32 (colBarLower));
    r.x = slider0 + sliderW;
    r.w = area.w - r.x;
    SDL_FillRect (surface, &r, ToUint32 (colBarUpper));

    // Draw slider...
    r = Rect (slider0, 0, sliderW, area.h);
    for (n = 0; n < 64; n++) {
      r.h = (n + 1) * surface->h / 64 - r.y;
      if (r.h > 0) {
        SDL_FillRect (surface, &r, ToUint32 (ColorBrighter (colSlider, isDown ? n-32 : 32-n)));
        r.y += r.h;
      }
    }

    // Done...
    redraw = false;
  }
  return surface;

}


bool CSlider::HandleEvent (SDL_Event *ev) {
  int x, y;
  bool ret;

  ret = false;
  switch (ev->type) {
    case SDL_MOUSEBUTTONDOWN:
      //~ INFOF (("CSlider::HandleEvent: SDL_MOUSEBUTTONDOWN; slider = (%i, %i)", slider0, sliderW));
      GetMouseEventPos (ev, &x, &y);
      if (!isDown && x >= area.x + slider0 && x < area.x + slider0 + sliderW && y >= area.y && y < area.y + area.h) {
        //~ INFO ("... down on slider ...");
        downX = x - slider0;
        isDown = true;
        ChangedSurface ();
        ret = true;
      }
      break;
    case SDL_MOUSEMOTION:
      if (isDown) {
        GetMouseEventPos (ev, &x, &y);
        SetSlider0 (x - downX, continuousUpdate);
        ret = true;
      }
      break;
    case SDL_MOUSEBUTTONUP:
      if (isDown) {
        GetMouseEventPos (ev, &x, &y);
        isDown = false;
        SetSlider0 (x - downX, true);
        ChangedSurface ();
        ret = true;
      }
      break;
  }
  return ret;
}
