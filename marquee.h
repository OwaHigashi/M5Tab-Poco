// marquee.h
// Non-blocking multi-track horizontal scrolling marquee for the FX area.
// Nico Nico-style: up to MARQUEE_LANES concurrent tracks in horizontal bands.
//
// Rendering strategy — PRE-RENDERED PER TRACK:
//   1. A single persistent "fontCanvas" (large, holds the loaded VLW) is
//      used to measure and render text when a new track arrives.
//   2. Each track gets its own PSRAM sprite exactly sized to (textW × bandH)
//      with the finished text bitmap copied in.
//   3. Per frame we only pushSprite with clipping — no fillSprite, no
//      drawString, no VLW glyph work. Tiny trail strip is cleared each step.
//   This makes per-frame cost almost purely DMA bandwidth.
//
// Markup:
//   <r> <g> <b> <y> <c> <m> <w> <o>           color (default white)
//   <s1>/<small>  <s2>/<normal>  <s3>/<big>   size
//   <u> … </u>                                underline
//   <hl> … </hl>                              highlight box
//   </>                                       → reset COLOR only
//   </big> </small> </size>                   → reset size
//   </u> </hl>                                → close emphasis
//   </r> … </color>                           → reset color (same as </>)
// Per-track speed: pixPerStep = 12 + 6 * speedMult  (x1..x5 → 18..42).

#pragma once
#include <M5Unified.h>
#include <SD_MMC.h>
#include <vector>

static constexpr int   MARQUEE_LANES    = 9;      // overlapping bands, Nico-Nico
static constexpr int   MARQUEE_FONT_W   = 10000;  // shared font-canvas width
static constexpr int   MARQUEE_BAND_H   = 120;    // fits scale 2 (80px) + padding

struct MarqueeSeg {
  String   s;
  uint16_t color;
  uint8_t  scale;
  bool     underline;
  bool     highlight;
  int      pxWidth;
  int      pxHeight;
};

struct MarqueeTrack {
  bool      active;
  M5Canvas* sprite;        // per-track pre-rendered bitmap (textW × bandH)
  int       textW;
  int       textX;         // x on display where sprite left edge sits (decreases)
  int       pixPerStep;
  uint32_t  lastStep;
  uint32_t  startMillis;
};

struct MarqueeState {
  bool         initialized;
  bool         customFontLoaded;
  M5Canvas*    fontCanvas;     // persistent measure+render canvas with VLW loaded
  int          fontCanvasW;
  int          bandH;
  int          fxW, fxH;
  int          bandYs[MARQUEE_LANES];
  uint16_t     defaultColor;
  uint8_t      defaultScale;
  uint16_t     bgColor;
  uint16_t     highlightColor;
  uint16_t     stepIntervalMs;   // base step interval (minimum)
  uint16_t     emptyLaneWaitMs;  // extra wait added per missing lane (below ref)
  uint8_t      refLanes;         // reference active count — no wait at/above this
  MarqueeTrack tracks[MARQUEE_LANES];
};
inline MarqueeState g_marquee{};

// ----- tag parsing -----
inline bool marqueeApplyTag(const String& tagRaw,
                            uint16_t defColor, uint8_t defScale,
                            uint16_t& color, uint8_t& scale,
                            bool& underline, bool& highlight) {
  if (tagRaw.length() == 0) return false;
  String t = tagRaw; t.toLowerCase();

  // Bare </> resets color only.
  if (t == "/") { color = defColor; return true; }
  if (t.length() >= 2 && t[0] == '/') {
    String c = t.substring(1);
    if (c == "r" || c == "g" || c == "b" || c == "y" ||
        c == "c" || c == "m" || c == "w" || c == "o" ||
        c == "red" || c == "green" || c == "blue" || c == "yellow" ||
        c == "cyan" || c == "magenta" || c == "white" || c == "orange" ||
        c == "color") { color = defColor; return true; }
    if (c == "s1" || c == "s2" || c == "s3" ||
        c == "big" || c == "small" || c == "size")
      { scale = defScale; return true; }
    if (c == "u")                  { underline = false; return true; }
    if (c == "hl" || c == "mark")  { highlight = false; return true; }
    return false;
  }

  if (t == "r" || t == "red")     { color = TFT_RED;     return true; }
  if (t == "g" || t == "green")   { color = TFT_GREEN;   return true; }
  if (t == "b" || t == "blue")    { color = TFT_BLUE;    return true; }
  if (t == "y" || t == "yellow")  { color = TFT_YELLOW;  return true; }
  if (t == "c" || t == "cyan")    { color = TFT_CYAN;    return true; }
  if (t == "m" || t == "magenta") { color = TFT_MAGENTA; return true; }
  if (t == "w" || t == "white")   { color = TFT_WHITE;   return true; }
  if (t == "o" || t == "orange")  { color = TFT_ORANGE;  return true; }
  if (t == "s1" || t == "small")  { scale = 1; return true; }
  if (t == "s2" || t == "normal") { scale = 2; return true; }
  if (t == "s3" || t == "big")    { scale = 3; return true; }
  if (t == "u")                   { underline = true;  return true; }
  if (t == "hl" || t == "mark")   { highlight = true;  return true; }
  return false;
}

inline void marqueeParseMarkup(const String& in,
                               std::vector<MarqueeSeg>& out,
                               uint16_t defColor, uint8_t defScale) {
  out.clear();
  MarqueeSeg cur{};
  cur.color = defColor; cur.scale = defScale;
  cur.underline = false; cur.highlight = false;
  size_t i = 0, n = in.length();
  while (i < n) {
    char ch = in[i];
    if (ch == '<') {
      int end = in.indexOf('>', i + 1);
      if (end > (int)i) {
        String tag = in.substring(i + 1, end);
        MarqueeSeg probe = cur;
        if (marqueeApplyTag(tag, defColor, defScale,
                            probe.color, probe.scale,
                            probe.underline, probe.highlight)) {
          if (cur.s.length() > 0) {
            out.push_back(cur);
            cur.s = String();
          }
          cur.color     = probe.color;
          cur.scale     = probe.scale;
          cur.underline = probe.underline;
          cur.highlight = probe.highlight;
          i = end + 1;
          continue;
        }
      }
    }
    cur.s += ch;
    ++i;
  }
  if (cur.s.length() > 0) out.push_back(cur);
}

// ----- init -----
inline void marqueeApplyFont(M5Canvas* c, const String& vlwPath) {
  bool ok = false;
  if (vlwPath.length() > 0 && SD_MMC.exists(vlwPath.c_str())) {
    if (c->loadFont(SD_MMC, vlwPath.c_str())) {
      ok = true;
      Serial.printf("[marquee] VLW loaded: %s\n", vlwPath.c_str());
    } else {
      Serial.printf("[marquee] VLW loadFont failed: %s\n", vlwPath.c_str());
    }
  }
  if (!ok) {
    c->setFont(&fonts::efontJA_24_b);
    Serial.println("[marquee] using efontJA_24_b");
  }
  g_marquee.customFontLoaded = ok;
}

inline bool marqueeEnsureInit(int screenW, int fxH, const String& vlwPath) {
  auto& m = g_marquee;
  if (m.initialized) return true;

  m.fxW   = screenW;
  m.fxH   = fxH;
  m.bandH = MARQUEE_BAND_H;
  // Distribute bandYs across fxH so even-index bands tile without overlap
  // and odd-index bands fall exactly between them. With LANES=9, bandH=120,
  // fxH=600: step = (600-120)/8 = 60 → Y = 0,60,120,180,240,300,360,420,480.
  int span = fxH - m.bandH;
  int step = (MARQUEE_LANES > 1) ? (span / (MARQUEE_LANES - 1)) : 0;
  for (int i = 0; i < MARQUEE_LANES; ++i) {
    m.bandYs[i] = i * step;
    m.tracks[i] = MarqueeTrack{};
  }
  m.defaultColor    = TFT_WHITE;
  m.bgColor         = TFT_BLACK;
  m.highlightColor  = M5.Display.color565(40, 40, 80);
  m.stepIntervalMs  = 16;
  m.emptyLaneWaitMs = 3;
  m.refLanes        = 5;
  m.fontCanvasW     = MARQUEE_FONT_W;

  Serial.printf("[marquee] pre-init: free_heap=%u free_psram=%u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());

  if (!m.fontCanvas) m.fontCanvas = new M5Canvas(&M5.Display);
  m.fontCanvas->setColorDepth(16);
  m.fontCanvas->setPsram(true);
  if (!m.fontCanvas->createSprite(m.fontCanvasW, m.bandH)) {
    Serial.printf("[marquee] fontCanvas alloc failed %dx%d free_psram=%u\n",
                  m.fontCanvasW, m.bandH, (unsigned)ESP.getFreePsram());
    return false;
  }
  m.fontCanvas->fillSprite(m.bgColor);
  m.fontCanvas->setTextDatum(bottom_left);
  marqueeApplyFont(m.fontCanvas, vlwPath);

  m.initialized = true;
  Serial.printf("[marquee] init OK: fontCanvas=%dx%d bands=%d bandH=%d free_psram=%u\n",
                m.fontCanvasW, m.bandH, MARQUEE_LANES, m.bandH,
                (unsigned)ESP.getFreePsram());
  return true;
}

inline void marqueeFreeTrackSprite(MarqueeTrack& t) {
  if (t.sprite) {
    if (t.sprite->getBuffer()) t.sprite->deleteSprite();
    delete t.sprite;
    t.sprite = nullptr;
  }
}

inline bool marqueeActive() {
  for (int i = 0; i < MARQUEE_LANES; ++i) {
    if (g_marquee.tracks[i].active) return true;
  }
  return false;
}

inline void marqueeStopAll() {
  auto& m = g_marquee;
  for (int i = 0; i < MARQUEE_LANES; ++i) {
    m.tracks[i].active = false;
    marqueeFreeTrackSprite(m.tracks[i]);
  }
  if (m.initialized) {
    M5.Display.fillRect(0, 0, m.fxW, m.fxH, TFT_BLACK);
  }
}

// ----- addTrack result codes -----
enum class MarqueeAddResult {
  OK,
  EMPTY,               // no usable text
  INIT_FAIL,
  NORMAL_LANES_FULL,   // 5 non-overlap lanes busy AND no <sp> in payload
  ALL_LANES_FULL,      // all 9 lanes busy even with <sp>
};

// Strip <sp>/</sp> tokens (case-insensitive); returns true if any was present.
// These tokens act only as a permission flag to allow occupying overlap-bands.
inline bool marqueeStripSpTags(String& s) {
  bool found = false;
  String lower = s; lower.toLowerCase();
  if (lower.indexOf("<sp>") >= 0 || lower.indexOf("</sp>") >= 0) {
    found = true;
    // Remove all variants literally.
    s.replace("<sp>", "");  s.replace("</sp>", "");
    s.replace("<SP>", "");  s.replace("</SP>", "");
    s.replace("<Sp>", "");  s.replace("</Sp>", "");
    s.replace("<sP>", "");  s.replace("</sP>", "");
  }
  return found;
}

// ----- addTrack: parse + render into fontCanvas + copy to track sprite -----
inline MarqueeAddResult marqueeAddTrack(const String& markup,
                                        int screenW, int fxH,
                                        const String& vlwPath,
                                        int defaultScale = 2,
                                        int speedMult    = 1) {
  if (markup.length() == 0) return MarqueeAddResult::EMPTY;
  if (!marqueeEnsureInit(screenW, fxH, vlwPath))
    return MarqueeAddResult::INIT_FAIL;

  auto& m = g_marquee;
  if (defaultScale < 1) defaultScale = 1;
  if (defaultScale > 3) defaultScale = 3;
  m.defaultScale = (uint8_t)defaultScale;
  if (speedMult < 1) speedMult = 1;
  if (speedMult > 5) speedMult = 5;

  // Detect + strip <sp>…</sp> override (permits overlap-band placement).
  String cleanMarkup = markup;
  const bool allowOverflow = marqueeStripSpTags(cleanMarkup);
  if (cleanMarkup.length() == 0) return MarqueeAddResult::EMPTY;

  std::vector<MarqueeSeg> segs;
  marqueeParseMarkup(cleanMarkup, segs, m.defaultColor, m.defaultScale);

  // Measure widths via fontCanvas.
  int totalW = 0;
  for (auto& seg : segs) {
    m.fontCanvas->setTextSize(seg.scale);
    seg.pxWidth  = m.fontCanvas->textWidth(seg.s.c_str());
    seg.pxHeight = m.fontCanvas->fontHeight();
    totalW += seg.pxWidth;
  }
  if (totalW <= 0) return MarqueeAddResult::EMPTY;
  if (totalW > m.fontCanvasW) {
    Serial.printf("[marquee] text too wide: %d > %d, truncating\n",
                  totalW, m.fontCanvasW);
    totalW = m.fontCanvasW;
  }

  // Pick band with the admission policy:
  //   - default: only EVEN bands (0,2,4,6,8) — non-overlap — up to 5 slots
  //   - <sp>-tagged: also ODD bands (1,3,5,7) allowed — overlap OK — up to 9
  //   - never evict; if no eligible free slot, REJECT the message
  int freeEven[MARQUEE_LANES], freeEvenN = 0;
  int freeOdd [MARQUEE_LANES], freeOddN  = 0;
  for (int i = 0; i < MARQUEE_LANES; ++i) {
    if (m.tracks[i].active) continue;
    if ((i & 1) == 0) freeEven[freeEvenN++] = i;
    else              freeOdd [freeOddN ++] = i;
  }
  int band;
  if (freeEvenN > 0) {
    band = freeEven[random(freeEvenN)];
  } else if (allowOverflow && freeOddN > 0) {
    band = freeOdd[random(freeOddN)];
  } else {
    if (!allowOverflow) {
      Serial.printf("[marquee] reject: 5 normal lanes full (use <sp> to override)\n");
      return MarqueeAddResult::NORMAL_LANES_FULL;
    }
    Serial.println("[marquee] reject: all 9 lanes full");
    return MarqueeAddResult::ALL_LANES_FULL;
  }
  auto& t = m.tracks[band];
  marqueeFreeTrackSprite(t);  // free any leftover

  // Render into fontCanvas at x=0..totalW. Feed the WDT between segments
  // — drawString via VLW can read SD blocks and take real time.
  uint32_t renderT0 = millis();
  m.fontCanvas->fillSprite(m.bgColor);
  int x = 0;
  const int yBottom = m.bandH - 4;
  for (auto& seg : segs) {
    m.fontCanvas->setTextSize(seg.scale);
    if (seg.highlight) {
      m.fontCanvas->fillRect(x - 2, yBottom - seg.pxHeight,
                             seg.pxWidth + 4, seg.pxHeight + 2, m.highlightColor);
    }
    m.fontCanvas->setTextColor(seg.color,
                               seg.highlight ? m.highlightColor : m.bgColor);
    m.fontCanvas->drawString(seg.s.c_str(), x, yBottom);
    if (seg.underline) {
      m.fontCanvas->fillRect(x, yBottom + 1, seg.pxWidth, 3, seg.color);
    }
    x += seg.pxWidth;
    yield();
    if (x >= totalW) break;
  }
  uint32_t renderMs = millis() - renderT0;

  // Allocate per-track sprite and memcpy the rendered rows.
  t.sprite = new M5Canvas(&M5.Display);
  t.sprite->setColorDepth(16);
  t.sprite->setPsram(true);
  if (!t.sprite->createSprite(totalW, m.bandH)) {
    Serial.printf("[marquee] track sprite alloc failed (%dx%d)\n",
                  totalW, m.bandH);
    delete t.sprite; t.sprite = nullptr;
    return MarqueeAddResult::INIT_FAIL;
  }
  {
    const uint16_t* src = (const uint16_t*)m.fontCanvas->getBuffer();
    uint16_t* dst       = (uint16_t*)t.sprite->getBuffer();
    if (!src || !dst) {
      Serial.println("[marquee] null buffer on copy; aborting");
      marqueeFreeTrackSprite(t);
      return MarqueeAddResult::INIT_FAIL;
    }
    const size_t rowBytes = (size_t)totalW * sizeof(uint16_t);
    for (int y = 0; y < m.bandH; ++y) {
      memcpy(dst + (size_t)y * totalW,
             src + (size_t)y * m.fontCanvasW, rowBytes);
      if ((y & 15) == 0) delay(0);
    }
  }

  t.textW       = totalW;
  t.textX       = m.fxW;                  // start fully off-screen right
  t.pixPerStep  = 12 + 6 * speedMult;     // 18..42
  t.lastStep    = millis();
  t.startMillis = t.lastStep;
  t.active      = true;

  // Ensure the FX area is cleared at least once; then clear this band.
  static bool firstDraw = true;
  if (firstDraw) {
    M5.Display.fillRect(0, 0, m.fxW, m.fxH, m.bgColor);
    firstDraw = false;
  }
  M5.Display.fillRect(0, m.bandYs[band], m.fxW, m.bandH, m.bgColor);

  Serial.printf("[marquee] band=%d textW=%d segs=%u speed=x%d render=%ums sp=%d free_psram=%u\n",
                band, t.textW, (unsigned)segs.size(),
                speedMult, (unsigned)renderMs, (int)allowOverflow,
                (unsigned)ESP.getFreePsram());
  return MarqueeAddResult::OK;
}

// ----- per-frame step -----
inline void marqueeStep() {
  auto& m = g_marquee;
  if (!m.initialized) return;
  uint32_t now = millis();

  int activeCount = 0;
  for (int i = 0; i < MARQUEE_LANES; ++i) {
    if (m.tracks[i].active) ++activeCount;
  }
  if (activeCount == 0) return;

  // Per-tick effective interval: when fewer lanes are active the tick is
  // shorter (less render work). Insert wait to slow it down, but ONLY up to
  // `refLanes` — beyond that, accept the natural slowdown from rendering.
  // This avoids making 1-lane artificially slower than 9-lane.
  const int missing = (activeCount < m.refLanes)
                        ? (m.refLanes - activeCount) : 0;
  const uint32_t effectiveInterval =
      m.stepIntervalMs + (uint32_t)missing * m.emptyLaneWaitMs;

  static uint32_t statsT0 = 0;
  static uint32_t stepsDone = 0;
  static uint32_t renderAccMs = 0;
  static int      statsMaxActive = 0;
  const uint32_t stepT0 = millis();

  for (int i = 0; i < MARQUEE_LANES; ++i) {
    auto& t = m.tracks[i];
    if (!t.active || !t.sprite) continue;
    if (now - t.lastStep < effectiveInterval) continue;
    t.lastStep = now;
    t.textX -= t.pixPerStep;
    if (t.textX + t.textW < 0) {
      t.active = false;
      marqueeFreeTrackSprite(t);
      M5.Display.fillRect(0, m.bandYs[i], m.fxW, m.bandH, TFT_BLACK);
      continue;
    }
    // Clear the trail (the strip on the right of the new sprite position
    // that was covered last frame by the sprite's right edge).
    const int trailX0 = t.textX + t.textW;
    const int trailX1 = trailX0 + t.pixPerStep;
    const int clrX0   = trailX0 < 0 ? 0 : trailX0;
    const int clrX1   = trailX1 > m.fxW ? m.fxW : trailX1;
    if (clrX1 > clrX0) {
      M5.Display.fillRect(clrX0, m.bandYs[i], clrX1 - clrX0,
                          m.bandH, m.bgColor);
    }
    // Push the pre-rendered sprite (clipped by display bounds).
    t.sprite->pushSprite(&M5.Display, t.textX, m.bandYs[i]);
    ++stepsDone;
  }

  renderAccMs += millis() - stepT0;
  if (activeCount > statsMaxActive) statsMaxActive = activeCount;

  if (statsT0 == 0) statsT0 = now;
  if (now - statsT0 >= 1000) {
    Serial.printf("[marquee.stats] active=%d/%d(max=%d) ref=%u steps=%u "
                  "renderSum=%ums interval=%ums wait_per_empty=%ums\n",
                  activeCount, MARQUEE_LANES, statsMaxActive,
                  (unsigned)m.refLanes,
                  (unsigned)stepsDone, (unsigned)renderAccMs,
                  (unsigned)effectiveInterval, (unsigned)m.emptyLaneWaitMs);
    stepsDone      = 0;
    renderAccMs    = 0;
    statsMaxActive = activeCount;
    statsT0        = now;
  }
}
