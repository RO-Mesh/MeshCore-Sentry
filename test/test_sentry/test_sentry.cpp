#include <gtest/gtest.h>

#include "NativeShim.h"
#include <Utils.h>
#include <helpers/TxtDataHelpers.h>
#include "SentryManager.h"

#include <string>

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

TEST(SentryManager, RotatesSpentTokensAndPreservesLiveOnes) {
  NativeFS fs;
  SentryManager s;
  s.begin(&fs);
  EXPECT_EQ(cli(s, "sentry import tokens OLD1 KEEP1"), "OK - 2 tokens");

  mesh::GroupChannel ch = makeHashChannel("#public");
  uint8_t payload[120];
  size_t len = makePayload(payload, "op: chk 1 OLD1");
  EXPECT_TRUE(s.handleGroupText(PAYLOAD_TYPE_GRP_TXT, ch, payload, len, 1000));
  s.clearPendingSwitch();

  EXPECT_EQ(cli(s, "sentry tokens rotate NEW1 NEW2"), "OK - add=1 repl=1 skip=0");
  std::string page = cli(s, "sentry export tokens 0");
  EXPECT_NE(page.find("#0:+NEW1"), std::string::npos);
  EXPECT_NE(page.find("#1:+KEEP1"), std::string::npos);
  EXPECT_NE(page.find("#2:+NEW2"), std::string::npos);
}

TEST(SentryManager, BadTokenImportKeepsExistingTokens) {
  NativeFS fs;
  SentryManager s;
  s.begin(&fs);
  EXPECT_EQ(cli(s, "sentry import tokens KEEP1"), "OK - 1 tokens");
  EXPECT_EQ(cli(s, "sentry import tokens NEW1 bad.token"), "Err - bad token");

  std::string page = cli(s, "sentry export tokens 0");
  EXPECT_NE(page.find("#0:+KEEP1"), std::string::npos);
  EXPECT_EQ(page.find("NEW1"), std::string::npos);
}

TEST(SentryManager, ImportPresetChangesTriggeredProfile) {
  NativeFS fs;
  SentryManager s;
  s.begin(&fs);
  EXPECT_EQ(cli(s, "sentry import preset 2 869.700 125.0 10 5"), "OK");
  EXPECT_EQ(cli(s, "sentry import tokens GO2"), "OK - 1 tokens");

  mesh::GroupChannel ch = makeHashChannel("#public");
  uint8_t payload[120];
  size_t len = makePayload(payload, "op: chk 2 GO2");
  EXPECT_TRUE(s.handleGroupText(PAYLOAD_TYPE_GRP_TXT, ch, payload, len, 1000));

  SentryPreset preset;
  ASSERT_TRUE(s.takePendingPreset(preset));
  EXPECT_FLOAT_EQ(preset.freq, 869.700f);
  EXPECT_FLOAT_EQ(preset.bw, 125.0f);
  EXPECT_EQ(preset.sf, 10);
  EXPECT_EQ(preset.cr, 5);
}

TEST(SentryManager, ExportsConfigAndPresets) {
  NativeFS fs;
  SentryManager s;
  s.begin(&fs);
  EXPECT_EQ(cli(s, "sentry import channel #admin-ops"), "OK");
  EXPECT_EQ(cli(s, "sentry import delay 90"), "OK");

  std::string config = cli(s, "sentry export config");
  EXPECT_NE(config.find("channel=#admin-ops"), std::string::npos);
  EXPECT_NE(config.find("delay=90"), std::string::npos);

  std::string presets = cli(s, "sentry export presets");
  EXPECT_NE(presets.find("preset 1"), std::string::npos);
  EXPECT_NE(presets.find("869.525"), std::string::npos);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
