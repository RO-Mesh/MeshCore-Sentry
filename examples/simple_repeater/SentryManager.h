#ifndef _SENTRY_MANAGER_H
#define _SENTRY_MANAGER_H

#include <Arduino.h>
#include <Mesh.h>
#include <helpers/IdentityStore.h>

#define SENTRY_MAX_TOKENS 10
#define SENTRY_MAX_TOKEN_LEN 16
#define SENTRY_MAX_CHANNEL_LEN 24
#define SENTRY_AUTH_LEN 32

struct SentryPreset {
  float freq;
  float bw;
  uint8_t sf;
  uint8_t cr;
};

struct SentryToken {
  char value[SENTRY_MAX_TOKEN_LEN];
  uint8_t spent;
};

struct SentryConfig {
  uint32_t magic;
  uint8_t version;
  uint8_t enabled;
  char channel[SENTRY_MAX_CHANNEL_LEN];
  uint8_t channel_hash;
  uint8_t channel_secret[32];
  uint8_t channel_secret_len;
  char auth_secret[SENTRY_AUTH_LEN];
  uint32_t apply_delay_ms;
  uint8_t active_preset;
  uint8_t token_count;
  SentryPreset presets[3];
  SentryToken tokens[SENTRY_MAX_TOKENS];
};

class SentryManager {
  FILESYSTEM* _fs;
  SentryConfig _cfg;
  bool _pending_switch;
  uint8_t _pending_preset;
  unsigned long _reboot_at;

  void setDefaults();
  bool load();
  bool save();
  bool configureChannel(const char* name, const char* psk_hex = NULL);
  bool parseTrigger(const char* text, uint8_t& preset, char* token, size_t token_len,
                    char* sig, size_t sig_len) const;
  bool burnToken(const char* token);
  bool authMatches(uint8_t preset, const char* token, const char* sig) const;
  bool channelMatches(const mesh::GroupChannel& channel) const;

public:
  SentryManager();

  void begin(FILESYSTEM* fs);
  bool handleGroupText(uint8_t type, const mesh::GroupChannel& channel, const uint8_t* data,
                       size_t len, unsigned long now_ms);
  bool takePendingPreset(SentryPreset& preset);
  bool shouldReboot(unsigned long now_ms) const;
  void clearPendingSwitch();

  void handleCommand(char* command, char* reply);
  void formatStatus(char* reply, size_t reply_len) const;
};

#endif
