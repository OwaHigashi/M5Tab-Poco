// sounds.h
// Non-blocking-ish tone-based SFX for M5Stack Tab5 speaker (ES8388).
// Uses M5.Speaker.tone() and short noise bursts via playRaw() for realism.

#pragma once
#include <M5Unified.h>

// Short pseudo-random noise burst (white-noise-ish) generated on the fly.
inline void playNoiseBurst(uint16_t duration_ms, uint8_t amp = 255) {
  const uint32_t sr = 22050;
  const size_t n = (size_t)(sr * duration_ms / 1000);
  const size_t CHUNK = 1024;
  int16_t buf[CHUNK];
  size_t sent = 0;
  uint32_t seed = micros();
  while (sent < n) {
    size_t len = (n - sent > CHUNK) ? CHUNK : (n - sent);
    for (size_t i = 0; i < len; ++i) {
      seed = seed * 1664525u + 1013904223u;
      int16_t s = (int16_t)((int32_t)((seed >> 8) & 0xFFFF) - 32768);
      s = (int16_t)((int32_t)s * amp / 255);
      buf[i] = s;
    }
    M5.Speaker.playRaw(buf, len, sr, false, 1, 0);
    while (M5.Speaker.isPlaying(0)) { delay(1); }
    sent += len;
  }
}

// ==========================================================
//  BOMB — ~1.5s: rising fuse -> bang -> crackle -> low rumble
// ==========================================================
inline void playBombSound() {
  // Fuse sweep (~220ms)
  for (int f = 200; f <= 1400; f += 150) {
    M5.Speaker.tone(f, 20);
    delay(22);
  }
  // BANG (~260ms)
  playNoiseBurst(250, 255);
  // Mid-tail crackle (~210ms)
  for (int i = 0; i < 3; ++i) {
    playNoiseBurst(40, (uint8_t)(180 - i * 30));
    delay(30);
  }
  // Low rumble (~560ms)
  for (int f = 180; f >= 60; f -= 10) {
    M5.Speaker.tone(f, 35);
    delay(40);
  }
}

// ==========================================================
//  CLAP — ~3.2s: layered claps then a rising cheer chord
// ==========================================================
inline void playClapSound() {
  const int claps = 12;
  for (int i = 0; i < claps; ++i) {
    uint8_t amp = (uint8_t)(140 + random(80));
    playNoiseBurst(25 + random(10), amp);
    delay(90 + random(110));  // irregular human cadence
  }
  // Final cheer: rising major chord
  const int notes[] = { 523, 659, 784, 988, 1319 };  // C5 E5 G5 B5 E6
  for (int i = 0; i < 5; ++i) {
    M5.Speaker.tone(notes[i], 120);
    delay(130);
  }
}
