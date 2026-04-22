// M5Tab-Poco
// M5Stack Tab5 remote-triggered full-color FX server
// Board: M5Stack Tab5 (ESP32-P4 + ESP32-C6 via ESP-Hosted)
// Libs : M5Unified, M5GFX, WiFi, WebServer

// Pull SD_MMC in first so M5GFX sees _SDMMC_H_ and instantiates its
// DataWrapperT<fs::SDMMCFS> specialization (needed for loadFont from SD).
#include <FS.h>
#include <SD_MMC.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <esp_system.h>

#include "config.h"
#include "webpage.h"
#include "animations.h"
#include "sounds.h"
#include "marquee.h"

// ---- Proportional fonts (smooth edges vs the built-in 8x6 scaled bitmap) ----
#define FONT_BTN    (&fonts::FreeSansBold18pt7b)
#define FONT_TITLE  (&fonts::FreeSansBold24pt7b)
#define FONT_STATUS (&fonts::FreeSans18pt7b)
#define FONT_SMALL  (&fonts::FreeSans12pt7b)
#define FONT_TINY   (&fonts::FreeSans9pt7b)

// ---- Config loaded at boot ----
static Config cfg;

// ---- Globals ----
static WebServer* server = nullptr;

struct AppState {
  volatile bool acceptRequests = true;
  uint8_t volume = 180;
  volatile int  pendingFx  = 0;
  uint32_t lastFxMillis = 0;
  uint16_t debounce_ms  = 300;
  String   wifi_ssid;
  String   marquee_font;       // optional VLW path on SD; empty = use efontJA
  int      marquee_scale = 3;  // efontJA_24_b scale (ignored when VLW loaded)
  int      marquee_wait_per_empty = 3;  // ms of extra wait per missing lane
  int      marquee_ref_lanes = 5;       // reference active count — no wait at/above
} state;

// ---- Layout ----
static int SCREEN_W = 1280;
static int SCREEN_H = 720;
static const int BAR_H = 120;
static int FX_H = 600;

struct Btn { int x, y, w, h; const char* label; uint16_t color; };
static Btn btnVolDown, btnVolUp, btnToggle;

// M5GFX's loadFont + VLW glyph rendering consumes a lot of stack; the default
// loop task stack (~8 KB) overflows inside marqueeEnsureInit on Tab5.
// Equivalent to SET_LOOP_TASK_STACK_SIZE(32*1024), written as a plain
// function definition (placed AFTER struct Btn so the Arduino IDE's
// auto-prototype generator doesn't push forward decls of drawButton above
// the struct definition).
size_t getArduinoLoopTaskStackSize(void) { return 32 * 1024; }

static void computeLayout() {
  SCREEN_W = M5.Display.width();
  SCREEN_H = M5.Display.height();
  FX_H     = SCREEN_H - BAR_H;
  int by   = SCREEN_H - BAR_H + 20;
  // Smaller ACCEPT so it doesn't overlap the volume bar; labels "ACCEPT"/"REJECT" fit.
  btnVolDown = {  20, by, 120, 80, "VOL-",   TFT_BLUE  };
  btnVolUp   = { 160, by, 120, 80, "VOL+",   TFT_BLUE  };
  btnToggle  = { 310, by, 170, 80, "ACCEPT", TFT_GREEN };
}

// =====================================================
//  Web page handlers
// =====================================================
static void handleRoot() {
  server->send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

static void handleStatus() {
  String j = "{";
  j += "\"accept\":"; j += (state.acceptRequests ? "true" : "false");
  j += ",\"volume\":"; j += state.volume;
  j += ",\"wifi\":\""; j += (WiFi.status() == WL_CONNECTED ? "connected" : "disconnected");
  j += "\",\"rssi\":"; j += (WiFi.status() == WL_CONNECTED ? (int)WiFi.RSSI() : 0);
  j += "}";
  server->send(200, "application/json", j);
}

// =====================================================
//  Talk (browser mic → Tab5 speaker) — streamed chunked PCM
//  Browser posts raw Int16 LE 16kHz mono PCM to /talk.
//  Uses the WebServer raw-upload path (4-arg on()) because the plain-body
//  path stores the body via String(const char*) — truncates at first NUL,
//  which fails on binary audio.
//  M5.Speaker.playRaw does NOT copy — it holds a pointer to our buffer —
//  so we own a ring of PSRAM slots and give each POST its own slot.
// =====================================================
static constexpr int    TALK_RING_SLOTS = 8;
static constexpr size_t TALK_SLOT_BYTES = 32 * 1024;  // 1s @ 16kHz Int16 mono
static constexpr uint32_t TALK_SR       = 16000;
static constexpr int    TALK_SPK_CH     = 1;          // FX uses 0
static uint8_t* talkRing[TALK_RING_SLOTS] = {nullptr};
static uint8_t  talkRingIdx = 0;
static size_t   talkAccum   = 0;
static bool     talkSkip    = false;

static void initTalkRing() {
  for (int i = 0; i < TALK_RING_SLOTS; ++i) {
    talkRing[i] = (uint8_t*)heap_caps_malloc(TALK_SLOT_BYTES, MALLOC_CAP_SPIRAM);
    if (!talkRing[i]) Serial.printf("[talk] PSRAM alloc failed slot %d\n", i);
  }
  Serial.printf("[talk] ring ready: %d x %u bytes (PSRAM)\n",
                TALK_RING_SLOTS, (unsigned)TALK_SLOT_BYTES);
}

static void handleTalkRaw() {
  HTTPRaw& r = server->raw();
  if (r.status == RAW_START) {
    talkAccum = 0;
    talkSkip  = !state.acceptRequests || talkRing[talkRingIdx] == nullptr;
    return;
  }
  if (talkSkip) return;
  uint8_t* dst = talkRing[talkRingIdx];
  if (r.status == RAW_WRITE) {
    size_t copy = r.currentSize;
    if (talkAccum + copy > TALK_SLOT_BYTES) copy = TALK_SLOT_BYTES - talkAccum;
    if (copy > 0) {
      memcpy(dst + talkAccum, r.buf, copy);
      talkAccum += copy;
    }
  } else if (r.status == RAW_END) {
    if (talkAccum >= 2) {
      size_t bytes   = talkAccum & ~((size_t)1);
      size_t samples = bytes / 2;
      M5.Speaker.playRaw((const int16_t*)dst, samples, TALK_SR, false, 1, TALK_SPK_CH);
      talkRingIdx = (uint8_t)((talkRingIdx + 1) % TALK_RING_SLOTS);
    }
  }
}

static void handleTalk() {
  if (talkSkip) {
    server->send(503, "application/json", "{\"ok\":false,\"reason\":\"disabled\"}");
  } else {
    server->send(200, "application/json", "{\"ok\":true}");
  }
}

// =====================================================
//  Marquee — scrolling text with simple HTML-ish color markup.
//  POST /marquee        body = UTF-8 text (text/plain). Cap at 1024 bytes.
//  POST /marquee/stop   stops any running marquee.
// =====================================================
static void handleMarquee() {
  if (!state.acceptRequests) {
    server->send(503, "application/json", "{\"ok\":false,\"reason\":\"disabled\"}");
    return;
  }
  const String& body = server->arg("plain");
  if (body.length() == 0) {
    server->send(400, "application/json", "{\"ok\":false,\"reason\":\"empty\"}");
    return;
  }
  if (body.length() > 1024) {
    server->send(413, "application/json", "{\"ok\":false,\"reason\":\"too_long\"}");
    return;
  }
  int speed = 1;
  if (server->hasArg("speed")) {
    long v = server->arg("speed").toInt();
    if (v >= 1 && v <= 5) speed = (int)v;
  }
  auto r = marqueeAddTrack(body, SCREEN_W, FX_H, state.marquee_font,
                           state.marquee_scale, speed);
  switch (r) {
    case MarqueeAddResult::OK:
      server->send(200, "application/json", "{\"ok\":true}");
      return;
    case MarqueeAddResult::NORMAL_LANES_FULL:
      server->send(503, "application/json",
                   "{\"ok\":false,\"reason\":\"lanes_full\",\"hint\":\"wrap with <sp></sp> to use overflow lanes\"}");
      return;
    case MarqueeAddResult::ALL_LANES_FULL:
      server->send(503, "application/json",
                   "{\"ok\":false,\"reason\":\"all_lanes_full\"}");
      return;
    case MarqueeAddResult::EMPTY:
      server->send(400, "application/json", "{\"ok\":false,\"reason\":\"empty\"}");
      return;
    case MarqueeAddResult::INIT_FAIL:
      server->send(500, "application/json", "{\"ok\":false,\"reason\":\"init_fail\"}");
      return;
  }
}

static void handleMarqueeStop() {
  marqueeStopAll();
  server->send(200, "application/json", "{\"ok\":true}");
}

static void handleFx(int fxId) {
  if (!state.acceptRequests) {
    server->send(503, "application/json", "{\"ok\":false,\"reason\":\"disabled\"}");
    return;
  }
  uint32_t now = millis();
  if (now - state.lastFxMillis < state.debounce_ms) {
    server->send(429, "application/json", "{\"ok\":false,\"reason\":\"busy\"}");
    return;
  }
  state.lastFxMillis = now;
  state.pendingFx = fxId;
  // Stop all marquee lanes so they don't fight the FX animation for the area.
  if (marqueeActive()) marqueeStopAll();
  // Immediate visual acknowledgement: black the FX area so the user sees the
  // request was accepted even before the animation task picks it up.
  M5.Display.fillRect(0, 0, SCREEN_W, FX_H, TFT_BLACK);
  server->send(200, "application/json", "{\"ok\":true}");
}
static void handleBomb() { handleFx(1); }
static void handleClap() { handleFx(2); }

// =====================================================
//  Control bar (on-device)
// =====================================================
static void drawButton(const Btn& b, bool active) {
  uint16_t fg = active ? TFT_WHITE : TFT_LIGHTGREY;
  uint16_t bg = active ? b.color   : TFT_DARKGREY;
  M5.Display.fillRoundRect(b.x, b.y, b.w, b.h, 12, bg);
  M5.Display.drawRoundRect(b.x, b.y, b.w, b.h, 12, fg);
  M5.Display.setTextColor(fg, bg);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(FONT_BTN);
  M5.Display.drawString(b.label, b.x + b.w/2, b.y + b.h/2);
}

static void drawControlBar() {
  int barY = SCREEN_H - BAR_H;
  // Guard against any earlier setTextSize(N>1) stuck in driver state (e.g. from an FX).
  M5.Display.setTextSize(1);
  M5.Display.fillRect(0, barY, SCREEN_W, BAR_H, TFT_BLACK);
  M5.Display.drawFastHLine(0, barY, SCREEN_W, TFT_DARKGREY);

  drawButton(btnVolDown, true);
  drawButton(btnVolUp,   true);
  btnToggle.color = state.acceptRequests ? TFT_GREEN : TFT_RED;
  btnToggle.label = state.acceptRequests ? "ACCEPT" : "REJECT";
  drawButton(btnToggle, true);

  // Volume bar — placed after the (smaller) ACCEPT button with a gap.
  int vx = 510, vy = barY + 40, vw = 260, vh = 40;
  M5.Display.drawRect(vx, vy, vw, vh, TFT_WHITE);
  int fill = (int)(vw * (state.volume / 255.0f));
  if (fill > 2) M5.Display.fillRect(vx + 1, vy + 1, fill - 2, vh - 2, TFT_ORANGE);

  char buf[40];
  snprintf(buf, sizeof(buf), "VOL %3d/255", state.volume);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setFont(FONT_SMALL);
  M5.Display.setTextDatum(middle_left);
  M5.Display.drawString(buf, vx + vw + 10, vy + vh/2);

  // Right side: WiFi status line (two small lines).
  M5.Display.setFont(FONT_SMALL);
  M5.Display.setTextDatum(middle_right);
  if (WiFi.status() == WL_CONNECTED) {
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    snprintf(buf, sizeof(buf), "STA %d dBm", (int)WiFi.RSSI());
    M5.Display.drawString(buf, SCREEN_W - 20, barY + 30);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString(WiFi.localIP().toString(), SCREEN_W - 20, barY + BAR_H - 30);
  } else {
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5.Display.drawString("NO WIFI", SCREEN_W - 20, barY + BAR_H/2);
  }
}

static bool hit(const Btn& b, int x, int y) {
  return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}

static void handleTouch() {
  auto t = M5.Touch.getDetail();
  if (!t.wasPressed()) return;
  Serial.printf("[touch] press x=%d y=%d\n", t.x, t.y);

  int x = t.x, y = t.y;
  if (hit(btnVolDown, x, y)) {
    state.volume = (state.volume > 20) ? state.volume - 20 : 0;
    M5.Speaker.setVolume(state.volume);
    drawControlBar();
  } else if (hit(btnVolUp, x, y)) {
    state.volume = (state.volume < 235) ? state.volume + 20 : 255;
    M5.Speaker.setVolume(state.volume);
    drawControlBar();
  } else if (hit(btnToggle, x, y)) {
    state.acceptRequests = !state.acceptRequests;
    drawControlBar();
  }
}

// =====================================================
//  Idle screen (main FX area content when no FX running)
// =====================================================
static void drawIdleScreen() {
  M5.Display.setTextSize(1);
  M5.Display.fillRect(0, 0, SCREEN_W, FX_H, TFT_NAVY);

  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_WHITE, TFT_NAVY);
  M5.Display.setFont(FONT_TITLE);
  M5.Display.drawString("M5Tab-Poco READY", SCREEN_W / 2, FX_H / 2 - 80);

  if (WiFi.status() == WL_CONNECTED) {
    M5.Display.setFont(FONT_STATUS);
    M5.Display.setTextColor(TFT_YELLOW, TFT_NAVY);
    String url = "http://" + WiFi.localIP().toString() + "/";
    M5.Display.drawString(url, SCREEN_W / 2, FX_H / 2);

    M5.Display.setFont(FONT_SMALL);
    M5.Display.setTextColor(TFT_GREENYELLOW, TFT_NAVY);
    char line[128];
    snprintf(line, sizeof(line), "connected to  %s   (%d dBm)",
             state.wifi_ssid.c_str(), (int)WiFi.RSSI());
    M5.Display.drawString(line, SCREEN_W / 2, FX_H / 2 + 60);
  } else {
    M5.Display.setFont(FONT_STATUS);
    M5.Display.setTextColor(TFT_RED, TFT_NAVY);
    M5.Display.drawString("WiFi NOT connected", SCREEN_W / 2, FX_H / 2);

    M5.Display.setFont(FONT_SMALL);
    M5.Display.setTextColor(TFT_LIGHTGREY, TFT_NAVY);
    char line[160];
    snprintf(line, sizeof(line),
             "trying STA: %s   (check SSID/pass in /config.ini on SD)",
             state.wifi_ssid.length() ? state.wifi_ssid.c_str() : "(no SSID)");
    M5.Display.drawString(line, SCREEN_W / 2, FX_H / 2 + 60);
  }
}

// =====================================================
//  FX runner  (sound + animation run in parallel)
// =====================================================
static TaskHandle_t s_sfx_task = nullptr;

static void sfx_task_fn(void* pv) {
  auto fn = (void(*)())pv;
  fn();
  s_sfx_task = nullptr;
  vTaskDelete(NULL);
}

static void startSfxAsync(void (*fn)()) {
  if (s_sfx_task != nullptr) return;  // a previous SFX is still finishing
  // Pin to Core 0 so it doesn't steal cycles from the animation on Core 1.
  xTaskCreatePinnedToCore(sfx_task_fn, "sfx", 8192, (void*)fn, 5, &s_sfx_task, 0);
}

static void waitSfx(uint32_t timeout_ms = 6000) {
  uint32_t t0 = millis();
  while (s_sfx_task != nullptr && millis() - t0 < timeout_ms) {
    delay(5);
  }
}

static void runPendingFx() {
  int fx = state.pendingFx;
  if (fx == 0) return;
  state.pendingFx = 0;
  Serial.printf("[fx] running fx=%d free_heap=%u\n", fx, (unsigned)ESP.getFreeHeap());

  if (fx == 1) {
    startSfxAsync(playBombSound);
    runBombAnimation(SCREEN_W, FX_H);
  } else if (fx == 2) {
    startSfxAsync(playClapSound);
    runClapAnimation(SCREEN_W, FX_H);
  }
  waitSfx();

  // Anchor debounce to FX end. Any requests that queued up in the TCP accept
  // backlog during the animation are now within debounce_ms of "now" and get
  // rejected with 429 on the next handleClient() call — so rapid repeated
  // presses during an animation do NOT stack up into back-to-back replays.
  state.lastFxMillis = millis();

  // Keep the FX area black after the animation (user preference: no
  // auto-return to the READY screen). Refresh the control bar only, so the
  // bottom status reflects the current WiFi RSSI.
  drawControlBar();
  Serial.printf("[fx] done free_heap=%u\n", (unsigned)ESP.getFreeHeap());
}

// =====================================================
//  Network identity dump — writes /mac.txt on SD.
// =====================================================
static void dumpNetInfoToSD(const String& hostname, const String& ssid) {
  if (!SD_MMC.cardType()) {
    if (!SD_MMC.begin("/sdcard", false)) return;
  }
  File f = SD_MMC.open("/mac.txt", "w");
  if (!f) return;

  uint8_t sta[6] = {0}, ap[6] = {0};
  delay(300);
  if (esp_wifi_get_mac(WIFI_IF_STA, sta) != ESP_OK) WiFi.macAddress(sta);
  if (esp_wifi_get_mac(WIFI_IF_AP,  ap ) != ESP_OK) WiFi.softAPmacAddress(ap);
  char line[96];

  f.println("# M5Tab-Poco network identity  (regenerated every boot)");
  f.printf ("hostname = %s\n", hostname.c_str());
  f.printf ("sta_ssid = %s\n", ssid.c_str());
  snprintf(line, sizeof(line), "sta_mac  = %02X:%02X:%02X:%02X:%02X:%02X\n",
           sta[0], sta[1], sta[2], sta[3], sta[4], sta[5]);
  f.print(line);
  snprintf(line, sizeof(line), "ap_mac   = %02X:%02X:%02X:%02X:%02X:%02X\n",
           ap[0], ap[1], ap[2], ap[3], ap[4], ap[5]);
  f.print(line);
  if (WiFi.status() == WL_CONNECTED) {
    f.printf("mode     = STA\n");
    f.printf("ip       = %s\n",      WiFi.localIP().toString().c_str());
    f.printf("gateway  = %s\n",      WiFi.gatewayIP().toString().c_str());
    f.printf("netmask  = %s\n",      WiFi.subnetMask().toString().c_str());
    f.printf("dns      = %s\n",      WiFi.dnsIP().toString().c_str());
    f.printf("rssi_dbm = %d\n",      (int)WiFi.RSSI());
  } else {
    f.printf("mode     = STA (not connected)\n");
  }
  f.close();
  Serial.printf("[mac] STA %02X:%02X:%02X:%02X:%02X:%02X\n",
                sta[0], sta[1], sta[2], sta[3], sta[4], sta[5]);
}

// =====================================================
//  Setup / Loop
// =====================================================
static const char* resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "POWERON";
    case ESP_RST_EXT:      return "EXT";
    case ESP_RST_SW:       return "SW";
    case ESP_RST_PANIC:    return "PANIC (exception/assert)";
    case ESP_RST_INT_WDT:  return "INT_WDT (int watchdog)";
    case ESP_RST_TASK_WDT: return "TASK_WDT (task watchdog)";
    case ESP_RST_WDT:      return "WDT (other watchdog)";
    case ESP_RST_DEEPSLEEP:return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT (power dip)";
    case ESP_RST_SDIO:     return "SDIO";
    case ESP_RST_USB:      return "USB";
    case ESP_RST_JTAG:     return "JTAG";
    default:               return "UNKNOWN";
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  esp_reset_reason_t rr = esp_reset_reason();
  Serial.printf("\n\n[M5Tab-Poco] setup() entry  (reset_reason=%d %s)\n",
                (int)rr, resetReasonStr(rr));

  auto m5cfg = M5.config();
  m5cfg.output_power = true;
  m5cfg.internal_spk = true;
  m5cfg.internal_mic = false;   // unused; avoids starting the ADC half of ES8388
  M5.begin(m5cfg);
  Serial.printf("[boot] panel=%p native=%dx%d heap=%u psram=%u\n",
                (void*)M5.Display.getPanel(),
                M5.Display.width(), M5.Display.height(),
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());

  bool ok = cfg.load("/config.ini");
  Serial.printf("[boot] cfg.load=%s err='%s' keys=%u\n",
                ok ? "OK" : "FAIL", cfg.errorReason.c_str(),
                (unsigned)cfg.all().size());

  state.wifi_ssid  = cfg.getString("wifi_ssid");
  String wifi_pass = cfg.getString("wifi_pass");
  String hostname  = cfg.getString("hostname", "m5tab-poco");
  uint16_t port    = (uint16_t)cfg.getInt("http_port", 80);
  uint8_t rot      = (uint8_t) cfg.getInt("rotation", 1);
  uint8_t bright   = (uint8_t) cfg.getInt("brightness", 255);
  uint16_t wifi_timeout_ms = (uint16_t)cfg.getInt("wifi_timeout_ms", 10000);
  state.volume         = (uint8_t) cfg.getInt("startup_volume", 180);
  state.acceptRequests = cfg.getBool("accept_on_boot", true);
  state.debounce_ms    = (uint16_t)cfg.getInt("debounce_ms", 300);
  state.marquee_font   = cfg.getString("marquee_font", "/fonts/ipaexg40.vlw");
  state.marquee_scale  = (int)cfg.getInt("marquee_scale", 2);
  state.marquee_wait_per_empty = (int)cfg.getInt("marquee_wait_per_empty", 3);
  state.marquee_ref_lanes      = (int)cfg.getInt("marquee_ref_lanes", 5);

  M5.Display.setRotation(rot);
  M5.Display.setBrightness(bright);
  computeLayout();

  // ---- Boot screen ----
  M5.Display.fillScreen(TFT_NAVY);
  M5.Display.fillRect(0, 0, SCREEN_W, 80, TFT_ORANGE);
  M5.Display.setTextDatum(middle_left);
  M5.Display.setTextColor(TFT_BLACK, TFT_ORANGE);
  M5.Display.setFont(FONT_TITLE);
  M5.Display.drawString("M5Tab-Poco booting...", 20, 40);

  M5.Display.setTextColor(TFT_WHITE, TFT_NAVY);
  M5.Display.setFont(FONT_SMALL);
  M5.Display.setTextDatum(top_left);
  int ly = 110;
  char line[160];
  snprintf(line, sizeof(line), "Display : %dx%d  rot=%d  bri=%d",
           SCREEN_W, SCREEN_H, rot, bright);
  M5.Display.drawString(line, 40, ly); ly += 30;
  snprintf(line, sizeof(line), "Config  : %s",
           ok ? ("loaded (" + String(cfg.all().size()) + " keys)").c_str()
              : ("FAIL: " + cfg.errorReason).c_str());
  M5.Display.drawString(line, 40, ly); ly += 30;
  snprintf(line, sizeof(line), "WiFi STA: %s",
           state.wifi_ssid.length() ? state.wifi_ssid.c_str() : "(no SSID in config.ini!)");
  M5.Display.drawString(line, 40, ly); ly += 30;

  M5.Speaker.begin();
  M5.Speaker.setVolume(state.volume);

  // ---- Connect WiFi STA (no AP fallback — device always joins a provided WiFi) ----
  if (state.wifi_ssid.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(hostname.c_str());
    WiFi.setAutoReconnect(true);

    String s_ip = cfg.getString("ip_address");
    if (s_ip.length() > 0) {
      IPAddress ip, gw, sn, d1, d2;
      ip.fromString(s_ip);
      gw.fromString(cfg.getString("gateway", "0.0.0.0"));
      sn.fromString(cfg.getString("netmask", "255.255.255.0"));
      d1.fromString(cfg.getString("dns1", cfg.getString("gateway", "0.0.0.0").c_str()));
      d2.fromString(cfg.getString("dns2", "8.8.8.8"));
      WiFi.config(ip, gw, sn, d1, d2);
      Serial.printf("[wifi] static %s gw=%s\n", ip.toString().c_str(), gw.toString().c_str());
    } else {
      Serial.println("[wifi] DHCP");
    }

    WiFi.begin(state.wifi_ssid.c_str(), wifi_pass.c_str());
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < wifi_timeout_ms) {
      delay(200);
      Serial.print('.');
    }
    Serial.println();
  } else {
    Serial.println("[wifi] no SSID in config.ini — WiFi not started");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] CONNECTED ssid=%s ip=%s rssi=%d\n",
                  state.wifi_ssid.c_str(),
                  WiFi.localIP().toString().c_str(),
                  (int)WiFi.RSSI());
  } else if (state.wifi_ssid.length()) {
    Serial.printf("[wifi] not yet connected (status=%d) — retrying in background\n",
                  (int)WiFi.status());
  }

  dumpNetInfoToSD(hostname, state.wifi_ssid);

  // ---- WebServer (also starts when WiFi is down; will serve once IP comes up) ----
  server = new WebServer(port);
  server->on("/",       HTTP_GET,  handleRoot);
  server->on("/status", HTTP_GET,  handleStatus);
  server->on("/bomb",   HTTP_POST, handleBomb);
  server->on("/clap",   HTTP_POST, handleClap);
  server->on("/talk",   HTTP_POST, handleTalk, handleTalkRaw);
  server->on("/marquee",      HTTP_POST, handleMarquee);
  server->on("/marquee/stop", HTTP_POST, handleMarqueeStop);
  initTalkRing();
  // Pre-init marquee so the first POST doesn't block a loop tick with
  // font load + sprite alloc (TASK_WDT risk).
  marqueeEnsureInit(SCREEN_W, FX_H, state.marquee_font);
  g_marquee.emptyLaneWaitMs = (uint16_t)state.marquee_wait_per_empty;
  g_marquee.refLanes        = (uint8_t)state.marquee_ref_lanes;
  server->begin();

  // ---- Final paint ----
  M5.Display.fillScreen(TFT_NAVY);
  drawControlBar();
  drawIdleScreen();
  Serial.printf("[boot] setup() complete, heap=%u psram=%u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
}

void loop() {
  static uint32_t  lastBeat = 0;
  static uint32_t  loopCount = 0;
  static wl_status_t lastWifi = (wl_status_t)-1;
  static uint32_t  minHeap = 0xFFFFFFFFu;

  uint32_t now  = millis();
  uint32_t heap = ESP.getFreeHeap();
  if (heap < minHeap) minHeap = heap;

  loopCount++;
  if (now - lastBeat >= 2000) {
    Serial.printf("[loop] %u iter/2s heap=%u(min=%u) psram=%u wifi=%d rssi=%d pendingFx=%d\n",
                  loopCount, heap, minHeap,
                  (unsigned)ESP.getFreePsram(),
                  (int)WiFi.status(),
                  WiFi.status() == WL_CONNECTED ? (int)WiFi.RSSI() : 0,
                  state.pendingFx);
    loopCount = 0;
    lastBeat = now;
  }

  // Redraw status when WiFi state flips.
  wl_status_t cur = WiFi.status();
  if (cur != lastWifi) {
    Serial.printf("[wifi] status change %d -> %d\n", (int)lastWifi, (int)cur);
    lastWifi = cur;
    drawControlBar();
    drawIdleScreen();
  }

  M5.update();
  if (server) server->handleClient();
  handleTouch();
  marqueeStep();
  runPendingFx();
}
