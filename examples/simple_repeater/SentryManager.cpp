#include "SentryManager.h"

#include <ctype.h>
#include <helpers/TxtDataHelpers.h>
#include <Utils.h>

#define SENTRY_CFG_FILE "/sentry_cfg"
#define SENTRY_MAGIC 0x52534E54UL
#define SENTRY_VERSION 1
#define SENTRY_DEFAULT_DELAY_MS 30000UL

static int sentryHexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static int sentryDecodeHex(const char* in, uint8_t* out, size_t out_len) {
  size_t in_len = strlen(in);
  if ((in_len & 1) != 0 || in_len / 2 > out_len) return 0;
  for (size_t i = 0; i < in_len; i += 2) {
    int hi = sentryHexVal(in[i]), lo = sentryHexVal(in[i + 1]);
    if (hi < 0 || lo < 0) return 0;
    out[i / 2] = (uint8_t)((hi << 4) | lo);
  }
  return (int)(in_len / 2);
}

static void sentryToHex(char* out, const uint8_t* in, size_t len) {
  static const char* HEX_CHARS = "0123456789ABCDEF";
  for (size_t i = 0; i < len; i++) {
    out[i * 2] = HEX_CHARS[in[i] >> 4];
    out[i * 2 + 1] = HEX_CHARS[in[i] & 0x0F];
  }
  out[len * 2] = 0;
}

static bool sentryTokenChar(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '_' || c == '-';
}

static bool sentryLooksLikeSig8(const char* s) {
  for (int i = 0; i < 8; i++) {
    if (!isxdigit(s[i])) return false;
  }
  return s[8] == 0;
}

static File sentryOpenWrite(FILESYSTEM* fs, const char* filename) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  fs->remove(filename);
  return fs->open(filename, FILE_O_WRITE);
#else
  return fs->open(filename, "w", true);
#endif
}

SentryManager::SentryManager() {
  _fs = NULL;
  _pending_switch = false;
  _pending_preset = 0;
  _reboot_at = 0;
  setDefaults();
}

void SentryManager::setDefaults() {
  memset(&_cfg, 0, sizeof(_cfg));
  _cfg.magic = SENTRY_MAGIC;
  _cfg.version = SENTRY_VERSION;
  _cfg.enabled = 1;
  _cfg.apply_delay_ms = SENTRY_DEFAULT_DELAY_MS;
  _cfg.active_preset = 1;
  _cfg.presets[0] = {869.525f, 62.5f, 8, 5};
  _cfg.presets[1] = {869.650f, 62.5f, 9, 5};
  _cfg.presets[2] = {869.400f, 62.5f, 7, 5};
  configureChannel("#public");
}

void SentryManager::begin(FILESYSTEM* fs) {
  _fs = fs;
  if (!load()) {
    setDefaults();
    save();
  }
}

bool SentryManager::load() {
  if (!_fs || !_fs->exists(SENTRY_CFG_FILE)) return false;
  File file = _fs->open(SENTRY_CFG_FILE);
  if (!file) return false;
  SentryConfig tmp;
  bool ok = file.read((uint8_t*)&tmp, sizeof(tmp)) == sizeof(tmp);
  file.close();
  if (!ok || tmp.magic != SENTRY_MAGIC || tmp.version != SENTRY_VERSION) return false;
  _cfg = tmp;
  if (_cfg.apply_delay_ms < 5000UL) _cfg.apply_delay_ms = 5000UL;
  if (_cfg.active_preset < 1 || _cfg.active_preset > 3) _cfg.active_preset = 1;
  if (_cfg.token_count > SENTRY_MAX_TOKENS) _cfg.token_count = SENTRY_MAX_TOKENS;
  return true;
}

bool SentryManager::save() {
  if (!_fs) return false;
  File file = sentryOpenWrite(_fs, SENTRY_CFG_FILE);
  if (!file) return false;
  bool ok = file.write((uint8_t*)&_cfg, sizeof(_cfg)) == sizeof(_cfg);
  file.close();
  return ok;
}

bool SentryManager::configureChannel(const char* name, const char* psk_hex) {
  if (!name || name[0] == 0 || strlen(name) >= sizeof(_cfg.channel)) return false;
  memset(_cfg.channel_secret, 0, sizeof(_cfg.channel_secret));
  _cfg.channel_secret_len = 0;
  StrHelper::strzcpy(_cfg.channel, name, sizeof(_cfg.channel));

  if (strcmp(name, "Public") == 0) {
    psk_hex = "8b3387e9c5cdea6ac9e5edbaa115cd72";
  }

  if (psk_hex && psk_hex[0]) {
    int len = sentryDecodeHex(psk_hex, _cfg.channel_secret, sizeof(_cfg.channel_secret));
    if (len != 16 && len != 32) return false;
    _cfg.channel_secret_len = (uint8_t)len;
  } else {
    if (name[0] != '#') return false;
    uint8_t digest[32];
    mesh::Utils::sha256(digest, sizeof(digest), (const uint8_t*)name, strlen(name));
    memcpy(_cfg.channel_secret, digest, 16);
    _cfg.channel_secret_len = 16;
  }

  mesh::Utils::sha256(&_cfg.channel_hash, sizeof(_cfg.channel_hash), _cfg.channel_secret, _cfg.channel_secret_len);
  return true;
}

bool SentryManager::channelMatches(const mesh::GroupChannel& channel) const {
  if (_cfg.channel_hash != channel.hash[0]) return false;
  return memcmp(_cfg.channel_secret, channel.secret, _cfg.channel_secret_len) == 0;
}

bool SentryManager::parseTrigger(const char* text, uint8_t& preset, char* token, size_t token_len,
                                 char* sig, size_t sig_len) const {
  preset = 0;
  token[0] = 0;
  sig[0] = 0;

  const char* p = strstr(text, "Stn# ");
  if (p) {
    p += 5;
    if (*p >= '1' && *p <= '3' && p[1] == '-') {
      preset = (uint8_t)(*p - '0');
      p += 2;
      size_t i = 0;
      while (sentryTokenChar(*p) && i + 1 < token_len) token[i++] = *p++;
      if (sentryTokenChar(*p)) return false;
      token[i] = 0;
      char* sep = strrchr(token, '-');
      if (sep && sentryLooksLikeSig8(sep + 1)) {
        StrHelper::strzcpy(sig, sep + 1, sig_len);
        *sep = 0;
      }
      return preset != 0 && token[0] != 0;
    }
  }

  p = strstr(text, "chk ");
  if (p) {
    p += 4;
    while (*p == ' ') p++;
    if (*p < '1' || *p > '3') return false;
    preset = (uint8_t)(*p++ - '0');
    while (*p == ' ') p++;
    size_t i = 0;
    while (sentryTokenChar(*p) && i + 1 < token_len) token[i++] = *p++;
    if (sentryTokenChar(*p)) return false;
    token[i] = 0;
    const char* sp = strstr(p, "sig:");
    if (sp) {
      sp += 4;
      size_t j = 0;
      while (isxdigit(*sp) && j + 1 < sig_len) sig[j++] = *sp++;
      sig[j] = 0;
    }
    return preset != 0 && token[0] != 0;
  }

  return false;
}

bool SentryManager::authMatches(uint8_t preset, const char* token, const char* sig) const {
  if (_cfg.auth_secret[0] == 0) return true;
  if (!sig || strlen(sig) != 8) return false;

  char msg[32];
  snprintf(msg, sizeof(msg), "%u:%s", (unsigned)preset, token);
  uint8_t digest[32];
  mesh::Utils::sha256(digest, sizeof(digest), (const uint8_t*)_cfg.auth_secret, strlen(_cfg.auth_secret),
                      (const uint8_t*)msg, strlen(msg));
  char expected[9];
  sentryToHex(expected, digest, 4);
  return strcasecmp(expected, sig) == 0;
}

bool SentryManager::burnToken(const char* token) {
  for (uint8_t i = 0; i < _cfg.token_count; i++) {
    if (_cfg.tokens[i].spent) continue;
    if (strcmp(_cfg.tokens[i].value, token) == 0) {
      _cfg.tokens[i].spent = 1;
      return save();
    }
  }
  return false;
}

bool SentryManager::handleGroupText(uint8_t type, const mesh::GroupChannel& channel, const uint8_t* data,
                                    size_t len, unsigned long now_ms) {
  if (!_cfg.enabled || type != PAYLOAD_TYPE_GRP_TXT || len < 6) return false;
  if (data[4] != TXT_TYPE_PLAIN && data[4] != TXT_TYPE_SIGNED_PLAIN) return false;
  if (!channelMatches(channel)) return false;

  char text[180];
  size_t text_len = len - 5;
  if (text_len >= sizeof(text)) text_len = sizeof(text) - 1;
  memcpy(text, &data[5], text_len);
  text[text_len] = 0;

  uint8_t preset;
  char token[SENTRY_MAX_TOKEN_LEN];
  char sig[12];
  if (!parseTrigger(text, preset, token, sizeof(token), sig, sizeof(sig))) return false;
  if (!authMatches(preset, token, sig)) return false;
  if (!burnToken(token)) return false;

  _cfg.active_preset = preset;
  save();
  _pending_preset = preset;
  _pending_switch = true;
  _reboot_at = now_ms + _cfg.apply_delay_ms;
  return true;
}

bool SentryManager::takePendingPreset(SentryPreset& preset) {
  if (!_pending_switch || _pending_preset < 1 || _pending_preset > 3) return false;
  preset = _cfg.presets[_pending_preset - 1];
  return true;
}

bool SentryManager::shouldReboot(unsigned long now_ms) const {
  return _pending_switch && _reboot_at && (long)(now_ms - _reboot_at) >= 0;
}

void SentryManager::clearPendingSwitch() {
  _pending_switch = false;
  _pending_preset = 0;
  _reboot_at = 0;
}

void SentryManager::formatStatus(char* reply, size_t reply_len) const {
  uint8_t live = 0, spent = 0;
  for (uint8_t i = 0; i < _cfg.token_count; i++) {
    if (_cfg.tokens[i].spent) spent++; else live++;
  }
  snprintf(reply, reply_len, "sentry %s ch=%s active=%u delay=%lus tokens=%u/%u auth=%s%s",
           _cfg.enabled ? "on" : "off", _cfg.channel, (unsigned)_cfg.active_preset,
           (unsigned long)(_cfg.apply_delay_ms / 1000UL), (unsigned)live, (unsigned)spent,
           _cfg.auth_secret[0] ? "on" : "off", _pending_switch ? " pending" : "");
}

void SentryManager::handleCommand(char* command, char* reply) {
  if (strcmp(command, "sentry status") == 0) {
    formatStatus(reply, 160);
    return;
  }

  if (memcmp(command, "set sentry.channel ", 19) == 0) {
    char* name = command + 19;
    char* psk = strchr(name, ' ');
    if (psk) *psk++ = 0;
    if (configureChannel(name, psk)) {
      save();
      strcpy(reply, "OK");
    } else {
      strcpy(reply, "Err - channel needs #name, Public, or name PSKHEX");
    }
    return;
  }

  if (memcmp(command, "set sentry.tokens ", 18) == 0) {
    char* p = command + 18;
    memset(_cfg.tokens, 0, sizeof(_cfg.tokens));
    _cfg.token_count = 0;
    while (*p && _cfg.token_count < SENTRY_MAX_TOKENS) {
      while (*p == ' ') p++;
      if (!*p) break;
      char* start = p;
      while (*p && *p != ' ') p++;
      char old = *p;
      *p = 0;
      if (strlen(start) == 0 || strlen(start) >= SENTRY_MAX_TOKEN_LEN) {
        strcpy(reply, "Err - bad token length");
        return;
      }
      StrHelper::strzcpy(_cfg.tokens[_cfg.token_count].value, start, SENTRY_MAX_TOKEN_LEN);
      _cfg.tokens[_cfg.token_count].spent = 0;
      _cfg.token_count++;
      if (old == 0) break;
      *p++ = old;
    }
    save();
    snprintf(reply, 160, "OK - %u tokens", (unsigned)_cfg.token_count);
    return;
  }

  if (memcmp(command, "set sentry.auth ", 16) == 0) {
    const char* secret = command + 16;
    if (strcmp(secret, "off") == 0) {
      _cfg.auth_secret[0] = 0;
    } else if (strlen(secret) >= sizeof(_cfg.auth_secret)) {
      strcpy(reply, "Err - auth too long");
      return;
    } else {
      StrHelper::strzcpy(_cfg.auth_secret, secret, sizeof(_cfg.auth_secret));
    }
    save();
    strcpy(reply, "OK");
    return;
  }

  if (memcmp(command, "set sentry.delay ", 17) == 0) {
    long secs = atol(command + 17);
    if (secs < 5 || secs > 300) {
      strcpy(reply, "Err - delay 5..300 sec");
      return;
    }
    _cfg.apply_delay_ms = (uint32_t)secs * 1000UL;
    save();
    strcpy(reply, "OK");
    return;
  }

  if (memcmp(command, "set preset ", 11) == 0) {
    char* p = command + 11;
    int id = atoi(p);
    if (id < 1 || id > 3) {
      strcpy(reply, "Err - preset 1..3");
      return;
    }
    while (*p && *p != ' ') p++;
    float vals[4];
    for (int i = 0; i < 4; i++) {
      while (*p == ' ') p++;
      if (!*p) {
        strcpy(reply, "Err - set preset <id> <freq> <bw> <sf> <cr>");
        return;
      }
      vals[i] = atof(p);
      while (*p && *p != ' ') p++;
    }
    if (vals[0] < 400.0f || vals[0] > 1000.0f || vals[1] <= 0.0f ||
        vals[2] < 7.0f || vals[2] > 12.0f || vals[3] < 5.0f || vals[3] > 8.0f) {
      strcpy(reply, "Err - bad radio params");
      return;
    }
    _cfg.presets[id - 1] = {vals[0], vals[1], (uint8_t)vals[2], (uint8_t)vals[3]};
    save();
    strcpy(reply, "OK");
    return;
  }

  reply[0] = 0;
}
