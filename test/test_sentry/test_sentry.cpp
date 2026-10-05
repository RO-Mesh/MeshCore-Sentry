#include <gtest/gtest.h>

#include "NativeShim.h"
#include <Utils.h>
#include <helpers/TxtDataHelpers.h>
#include "SentryManager.h"

static mesh::GroupChannel makeHashChannel(const char* name) {
  mesh::GroupChannel c;
  memset(&c, 0, sizeof(c));
  uint8_t digest[32];
  mesh::Utils::sha256(digest, sizeof(digest), (const uint8_t*)name, strlen(name));
  memcpy(c.secret, digest, 16);
  mesh::Utils::sha256(c.hash, sizeof(c.hash), c.secret, 16);
  return c;
}

static size_t makePayload(uint8_t* out, const char* text) {
  uint32_t ts = 1234;
  memcpy(out, &ts, 4);
  out[4] = TXT_TYPE_PLAIN;
  strcpy((char*)&out[5], text);
  return 5 + strlen(text);
}

static std::string cli(SentryManager& s, const char* command) {
  char buf[180];
  char cmd[180];
  strcpy(cmd, command);
  buf[0] = 0;
  s.handleCommand(cmd, buf);
  return std::string(buf);
}

TEST(SentryManager, BurnsTokenAndRejectsReplay) {
  NativeFS fs;
  SentryManager s;
  s.begin(&fs);
  EXPECT_EQ(cli(s, "set sentry.tokens RIVER31"), "OK - 1 tokens");

  mesh::GroupChannel ch = makeHashChannel("#public");
  uint8_t payload[120];
  size_t len = makePayload(payload, "wxbot: WX: Temp 20, Hum 40, Stn# 2-RIVER31");

  EXPECT_TRUE(s.handleGroupText(PAYLOAD_TYPE_GRP_TXT, ch, payload, len, 1000));
  SentryPreset preset;
  ASSERT_TRUE(s.takePendingPreset(preset));
  EXPECT_FLOAT_EQ(preset.freq, 869.650f);
  s.clearPendingSwitch();

  EXPECT_FALSE(s.handleGroupText(PAYLOAD_TYPE_GRP_TXT, ch, payload, len, 2000));
}

TEST(SentryManager, DelayGateControlsReboot) {
  NativeFS fs;
  SentryManager s;
  s.begin(&fs);
  EXPECT_EQ(cli(s, "set sentry.delay 5"), "OK");
  EXPECT_EQ(cli(s, "set sentry.tokens FALCON84"), "OK - 1 tokens");

  mesh::GroupChannel ch = makeHashChannel("#public");
  uint8_t payload[120];
  size_t len = makePayload(payload, "op: chk 3 FALCON84 normal chatter");

  EXPECT_TRUE(s.handleGroupText(PAYLOAD_TYPE_GRP_TXT, ch, payload, len, 1000));
  EXPECT_FALSE(s.shouldReboot(5999));
  EXPECT_TRUE(s.shouldReboot(6000));
}

TEST(SentryManager, WrongChannelDoesNotBurnToken) {
  NativeFS fs;
  SentryManager s;
  s.begin(&fs);
  EXPECT_EQ(cli(s, "set sentry.tokens RIVER31"), "OK - 1 tokens");

  mesh::GroupChannel wrong = makeHashChannel("#other");
  uint8_t payload[120];
  size_t len = makePayload(payload, "wxbot: WX: Temp 20, Hum 40, Stn# 1-RIVER31");

  EXPECT_FALSE(s.handleGroupText(PAYLOAD_TYPE_GRP_TXT, wrong, payload, len, 1000));

  mesh::GroupChannel right = makeHashChannel("#public");
  EXPECT_TRUE(s.handleGroupText(PAYLOAD_TYPE_GRP_TXT, right, payload, len, 2000));
}
