// config.h
// SD-backed configuration for M5Tab-Poco.
// Format: simple key=value, one per line. `#` or `;` starts a comment.
// Unknown keys are preserved so future .ini additions are readable without
// changing this file.

#pragma once
#include <Arduino.h>
#include <SD_MMC.h>
#include <FS.h>
#include <map>

class Config {
public:
  // Mount SD and load file at `path`. Returns true only if file was read
  // successfully; false means defaults are in use. On false, `errorReason`
  // holds a short tag: "no_sd", "no_file", "empty".
  bool load(const char* path = "/config.ini") {
    _loaded = false;
    _kv.clear();
    if (!SD_MMC.begin("/sdcard", false)) {
      errorReason = "no_sd";
      return false;
    }
    File f = SD_MMC.open(path, "r");
    if (!f) {
      errorReason = "no_file";
      return false;
    }
    while (f.available()) {
      String line = f.readStringUntil('\n');
      parseLine(line);
    }
    f.close();
    if (_kv.empty()) {
      errorReason = "empty";
      return false;
    }
    _loaded = true;
    return true;
  }

  bool isLoaded() const { return _loaded; }

  // Typed accessors with fallbacks.
  String getString(const char* key, const char* fallback = "") const {
    auto it = _kv.find(String(key));
    return it == _kv.end() ? String(fallback) : it->second;
  }
  int getInt(const char* key, int fallback) const {
    auto it = _kv.find(String(key));
    return it == _kv.end() ? fallback : it->second.toInt();
  }
  bool getBool(const char* key, bool fallback) const {
    auto it = _kv.find(String(key));
    if (it == _kv.end()) return fallback;
    String v = it->second; v.toLowerCase();
    return (v == "true" || v == "1" || v == "on" || v == "yes");
  }
  bool has(const char* key) const {
    return _kv.find(String(key)) != _kv.end();
  }
  const std::map<String,String>& all() const { return _kv; }

  String errorReason;

private:
  bool _loaded = false;
  std::map<String,String> _kv;

  static String trim(const String& s) {
    int a = 0, b = s.length();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b-1])) b--;
    return s.substring(a, b);
  }

  void parseLine(const String& raw) {
    String line = trim(raw);
    if (line.length() == 0) return;
    if (line[0] == '#' || line[0] == ';') return;
    int eq = line.indexOf('=');
    if (eq <= 0) return;
    String key = trim(line.substring(0, eq));
    String val = line.substring(eq + 1);
    // Strip inline comments starting with ; or #  (only if NOT inside quotes)
    bool inq = false;
    char qch = 0;
    for (int i = 0; i < (int)val.length(); ++i) {
      char c = val[i];
      if (inq) { if (c == qch) inq = false; }
      else if (c == '"' || c == '\'') { inq = true; qch = c; }
      else if (c == ';' || c == '#')  { val = val.substring(0, i); break; }
    }
    val = trim(val);
    // Strip optional surrounding quotes
    if (val.length() >= 2 &&
        ((val[0] == '"'  && val[val.length()-1] == '"') ||
         (val[0] == '\'' && val[val.length()-1] == '\''))) {
      val = val.substring(1, val.length() - 1);
    }
    if (key.length() > 0) _kv[key] = val;
  }
};
