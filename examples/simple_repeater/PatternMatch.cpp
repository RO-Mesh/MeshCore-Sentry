// PatternMatch.cpp — top-level '|' alternation and structural validation over
// the vendored regex engine. See PatternMatch.h for the dialect.

#include "PatternMatch.h"
#include <stdio.h>
#include <string.h>

#include "PacketFilterConfig.h"
#include "TinyRegex.h"

// The split buffer must hold the longest storable pattern; text patterns are
// the wider of the two rule storages, so one buffer sized by it covers both.
#define PATTERN_SPLIT_MAX FILTER_TEXT_PATTERN_LEN

// budget abort flag of the last patternMatches() call
static bool g_aborted = false;

enum SplitResult {
  SPLIT_OK = 0,
  SPLIT_EMPTY_ALT,     // "A|", "|A", "A||B": an empty branch would match everything
  SPLIT_TOO_MANY,      // more alternatives than FILTER_PATTERN_MAX_ALTS
  SPLIT_UNUSABLE,      // does not fit buf (callers reject over-long patterns first)
};

// Copy `pattern` into `buf` and terminate every top-level alternative in place,
// so the alternatives can be walked as consecutive NUL-terminated strings.
// Returns the alternative count, or 0 with *result set on rejection. A '\'
// escape and a [...] class both hold literal bytes: '\|' and '[|]' are pipes,
// '[\]|]' is a class holding ']'.
static int splitAlternatives(const char* pattern, char* buf, size_t buf_sz,
                             SplitResult* result) {
  size_t len = strlen(pattern);
  if (len + 1 > buf_sz) { *result = SPLIT_UNUSABLE; return 0; }
  memcpy(buf, pattern, len + 1);

  int alts = 1;
  size_t start = 0;       // offset where the alternative being scanned begins
  bool in_class = false;  // inside [...] — '|' there is a class member
  for (size_t i = 0; i < len; i++) {
    char c = buf[i];
    if (c == '\\') { i++; continue; }          // escaped byte
    if (in_class) {
      if (c == ']') in_class = false;
      continue;
    }
    if (c == '[') { in_class = true; continue; }
    if (c != '|') continue;

    if (i == start) { *result = SPLIT_EMPTY_ALT; return 0; }
    if (alts >= FILTER_PATTERN_MAX_ALTS) { *result = SPLIT_TOO_MANY; return 0; }
    buf[i] = 0;          // terminate this alternative in place
    start = i + 1;
    alts++;
  }
  if (start == len) { *result = SPLIT_EMPTY_ALT; return 0; }   // trailing '|'

  *result = SPLIT_OK;
  return alts;
}

bool patternValid(const char* pattern, char* err, size_t err_sz) {
  err[0] = 0;
  char buf[PATTERN_SPLIT_MAX];
  SplitResult result;
  int alts = splitAlternatives(pattern, buf, sizeof(buf), &result);
  if (alts == 0) {
    if (result == SPLIT_EMPTY_ALT) snprintf(err, err_sz, "empty alternative in regex");
    else if (result == SPLIT_TOO_MANY) {
      snprintf(err, err_sz, "too many alternatives (max %d)", FILTER_PATTERN_MAX_ALTS);
    }
    return false;
  }
  const char* alt = buf;
  for (int i = 0; i < alts; i++) {
    if (re_compile(alt) == NULL) return false;   // engine rejects it: no reason given
    alt += strlen(alt) + 1;
  }
  return true;
}

bool patternMatches(const char* pattern, const char* subject) {
  g_aborted = false;
  char buf[PATTERN_SPLIT_MAX];
  SplitResult result;
  int alts = splitAlternatives(pattern, buf, sizeof(buf), &result);
  if (alts == 0) return false;   // stored pattern cannot split: never matches
  const char* alt = buf;
  for (int i = 0; i < alts; i++) {
    re_t compiled = re_compile(alt);
    if (compiled == NULL) return false;   // stored pattern the engine rejects
    int matchlength;
    int idx = re_matchp(compiled, subject, &matchlength);
    if (re_budget_exhausted()) { g_aborted = true; return false; }   // fail-open
    if (idx >= 0) return true;
    alt += strlen(alt) + 1;
  }
  return false;
}

bool patternAborted(void) {
  return g_aborted;
}