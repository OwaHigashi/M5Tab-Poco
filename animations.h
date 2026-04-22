// animations.h
// Full-color particle effects for M5Stack Tab5 (1280x720 MIPI-DSI).
// Draws directly into the FX area (width=W, height=H). All effects stay
// within `min(cx, cy, W-cx, H-cy) - margin` from the center so the shape
// never clips off the visible area.

#pragma once
#include <M5Unified.h>
#include <math.h>

namespace fx {

  struct Particle {
    float x, y, vx, vy;
    float life;      // 1.0 -> 0.0
    float size;
    uint16_t hue;
  };

  inline uint16_t hsv565(float h, float s, float v) {
    float r, g, b;
    float c = v * s;
    float hh = fmodf(h, 360.0f) / 60.0f;
    float x = c * (1 - fabsf(fmodf(hh, 2.0f) - 1));
    if      (hh < 1) { r = c; g = x; b = 0; }
    else if (hh < 2) { r = x; g = c; b = 0; }
    else if (hh < 3) { r = 0; g = c; b = x; }
    else if (hh < 4) { r = 0; g = x; b = c; }
    else if (hh < 5) { r = x; g = 0; b = c; }
    else             { r = c; g = 0; b = x; }
    float m = v - c;
    uint8_t R = (uint8_t)((r + m) * 255);
    uint8_t G = (uint8_t)((g + m) * 255);
    uint8_t B = (uint8_t)((b + m) * 255);
    return M5.Display.color565(R, G, B);
  }

  inline float frand(float a, float b) {
    return a + (b - a) * (random(10000) / 10000.0f);
  }

  inline int minOf4(int a, int b, int c, int d) {
    int m = a;
    if (b < m) m = b;
    if (c < m) m = c;
    if (d < m) m = d;
    return m;
  }

} // namespace fx

// ==========================================================
//  BOMB — ~1.9s: sparks -> flash -> shockwave/fireball -> smoke
//  All radii capped so the effect fits within the FX area.
// ==========================================================
inline void runBombAnimation(int W, int H) {
  using namespace fx;
  auto& d = M5.Display;
  const int cx = W / 2, cy = H / 2;
  // Largest radius that still fits on all four sides, with a small margin.
  const float Rmax = (float)(minOf4(cx, cy, W - cx, H - cy) - 10);

  // 1) Pre-flash: sparks converging inward (~150ms)
  for (int f = 0; f < 10; ++f) {
    d.fillRect(0, 0, W, H, TFT_BLACK);
    float r = Rmax * (0.80f - f * 0.07f);
    if (r < 6) r = 6;
    for (int i = 0; i < 40; ++i) {
      float ang = frand(0, 2 * PI);
      int px = cx + (int)(cosf(ang) * r);
      int py = cy + (int)(sinf(ang) * r);
      uint16_t col = hsv565(frand(20, 60), 1.0f, 1.0f);
      d.fillCircle(px, py, 3, col);
    }
    delay(14);
  }

  // 2) White flash (3 frames, ~120ms)
  d.fillRect(0, 0, W, H, TFT_WHITE); delay(40);
  d.fillRect(0, 0, W, H, 0xFFE0);    delay(40);
  d.fillRect(0, 0, W, H, TFT_WHITE); delay(40);

  // 3) Expanding shockwave + fireball (~900ms)
  const int RINGS = 16;
  for (int i = 0; i < RINGS; ++i) {
    d.fillRect(0, 0, W, H, TFT_BLACK);
    float t = (float)i / (RINGS - 1);
    int r_core   = (int)(Rmax * 0.35f * t + 24);
    int r_glow   = (int)(Rmax * 0.55f * t + 20);
    // r_shock is the outermost yellow/orange/red ring; keep its +1 edge
    // strictly inside Rmax so nothing clips at the top/bottom.
    int r_shock  = (int)(Rmax * 0.92f * t + 14);
    if (r_shock + 2 > (int)Rmax) r_shock = (int)Rmax - 2;

    d.fillCircle(cx, cy, r_glow,               hsv565(10, 1.0f, 0.25f));
    d.fillCircle(cx, cy, (int)(r_glow * 0.7f), hsv565(20, 1.0f, 0.6f));
    d.fillCircle(cx, cy, r_core,               hsv565(40, 1.0f, 1.0f));
    d.fillCircle(cx, cy, r_core / 2,           TFT_WHITE);

    d.drawCircle(cx, cy, r_shock,     TFT_YELLOW);
    d.drawCircle(cx, cy, r_shock + 1, TFT_ORANGE);
    d.drawCircle(cx, cy, r_shock - 1, TFT_RED);

    // Debris along the shockwave radius (clipped to screen)
    for (int p = 0; p < 28; ++p) {
      float ang = frand(0, 2 * PI);
      float rd  = frand(r_shock * 0.7f, r_shock * 1.05f);
      if (rd > Rmax) rd = Rmax;
      int px = cx + (int)(cosf(ang) * rd);
      int py = cy + (int)(sinf(ang) * rd);
      uint16_t col = hsv565(frand(0, 50), 1.0f, frand(0.6f, 1.0f));
      d.fillCircle(px, py, (int)frand(2, 5), col);
    }
    delay(55);
  }

  // 4) Smoke billow, fading (~400ms)
  for (int i = 0; i < 8; ++i) {
    d.fillRect(0, 0, W, H, TFT_BLACK);
    float phase = i / 8.0f;
    int r = (int)(Rmax * (0.60f + phase * 0.35f));
    if (r > (int)Rmax) r = (int)Rmax;
    uint8_t g = (uint8_t)(90 * (1.0f - phase));
    d.fillCircle(cx, cy, r, d.color565(g, g, g));
    for (int p = 0; p < 16; ++p) {
      float ang = frand(0, 2 * PI);
      float rd  = frand(r * 0.4f, r * 0.9f);
      int px = cx + (int)(cosf(ang) * rd);
      int py = cy + (int)(sinf(ang) * rd);
      uint8_t gg = (uint8_t)(frand(30, 120) * (1.0f - phase));
      d.fillCircle(px, py, (int)frand(8, 20), d.color565(gg, gg, gg));
    }
    delay(45);
  }

  d.fillRect(0, 0, W, H, TFT_BLACK);
}

// ==========================================================
//  CLAP — ~3.6s: rainbow spotlight -> sparkle burst -> BRAVO!
//  Radii / star-cross positions clipped to stay on-screen.
// ==========================================================
inline void runClapAnimation(int W, int H) {
  using namespace fx;
  auto& d = M5.Display;
  const int cx = W / 2, cy = H / 2;
  const float Rmax = (float)(minOf4(cx, cy, W - cx, H - cy) - 10);

  // 1) Rainbow radial bands (~660ms)
  for (int f = 0; f < 18; ++f) {
    d.fillRect(0, 0, W, H, TFT_BLACK);
    for (int band = 0; band < 12; ++band) {
      int r = (int)(Rmax * (0.10f + band * 0.07f) + f * 2);
      if (r > Rmax) break;
      float hue = fmodf(band * 30 + f * 15, 360.0f);
      d.drawCircle(cx, cy, r,     hsv565(hue, 1.0f, 1.0f));
      d.drawCircle(cx, cy, r + 1, hsv565(hue, 1.0f, 0.7f));
      d.drawCircle(cx, cy, r + 2, hsv565(hue, 1.0f, 0.4f));
    }
    delay(35);
  }

  // 2) Golden sparkle burst (2 waves × 28 × 24ms ≈ 1344ms)
  const int N = 140;
  Particle ps[N];
  for (int wave = 0; wave < 2; ++wave) {
    for (int i = 0; i < N; ++i) {
      float ang = frand(0, 2 * PI);
      float spd = frand(3.0f, 11.0f);
      ps[i].x = cx; ps[i].y = cy;
      ps[i].vx = cosf(ang) * spd;
      ps[i].vy = sinf(ang) * spd;
      ps[i].life = 1.0f;
      ps[i].size = frand(3, 7);
      ps[i].hue  = (uint16_t)frand(35, 60);
    }
    for (int step = 0; step < 28; ++step) {
      d.fillRect(0, 0, W, H, TFT_BLACK);
      // Faint rainbow rim
      for (int band = 0; band < 5; ++band) {
        int r = (int)(Rmax * (0.18f + band * 0.18f));
        if (r > Rmax) break;
        float hue = fmodf(band * 50 + wave * 20 + step * 6, 360.0f);
        d.drawCircle(cx, cy, r, hsv565(hue, 1.0f, 0.35f));
      }
      for (int i = 0; i < N; ++i) {
        ps[i].x += ps[i].vx;
        ps[i].y += ps[i].vy;
        ps[i].vy += 0.25f;
        ps[i].life -= 0.040f;
        if (ps[i].life <= 0) continue;
        if (ps[i].x < 0 || ps[i].x >= W || ps[i].y < 0 || ps[i].y >= H) continue;
        uint16_t col = hsv565(ps[i].hue, 1.0f, ps[i].life);
        int sz = (int)(ps[i].size * ps[i].life) + 1;
        d.fillCircle((int)ps[i].x, (int)ps[i].y, sz, col);
        int x0 = (int)ps[i].x - sz * 2;
        int y0 = (int)ps[i].y - sz * 2;
        if (x0 < 0) x0 = 0; if (x0 + sz * 4 > W) x0 = W - sz * 4;
        if (y0 < 0) y0 = 0; if (y0 + sz * 4 > H) y0 = H - sz * 4;
        d.drawFastHLine(x0, (int)ps[i].y, sz * 4, col);
        d.drawFastVLine((int)ps[i].x, y0,       sz * 4, col);
      }
      delay(24);
    }
  }

  // 3) "BRAVO!" with expanding rim (~960ms)
  for (int i = 0; i < 12; ++i) {
    d.fillRect(0, 0, W, H, TFT_BLACK);
    for (int band = 0; band < 8; ++band) {
      int r = (int)(Rmax * (0.15f + band * 0.10f)) + i * 6;
      if (r > Rmax) break;
      float hue = fmodf(band * 45 + i * 20, 360.0f);
      d.drawCircle(cx, cy, r, hsv565(hue, 1.0f, 0.8f));
    }
    d.setTextDatum(middle_center);
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.setFont(&fonts::FreeSansBold24pt7b);
    d.drawString("BRAVO!", cx, cy);
    delay(80);
  }

  d.fillRect(0, 0, W, H, TFT_BLACK);
}
