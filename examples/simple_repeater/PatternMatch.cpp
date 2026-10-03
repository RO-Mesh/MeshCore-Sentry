// PatternMatch.cpp — top-level '|' alternation and structural validation over
// the vendored regex engine. See PatternMatch.h for the dialect.

#include "PatternMatch.h"
#include <stdio.h>
#include <string.h>

#include "PacketFilterConfig.h"
#include "TinyRegex.h"

// The split buffer must hold the longest storable pattern, and it sits on the
// stack in the packet path: size it by the wider of the two rule pattern
// storages, both of which are build-flag overridable (PacketFilterConfig.h).
#define PATTERN_SPLIT_MAX \
  ((FILTER_SENDER_PATTERN_LEN > FILTER_TEXT_PATTERN_LEN) ? FILTER_SENDER_PATTERN_LEN \
                                                        : FILTER_TEXT_PATTERN_LEN)

// budget abort flag of the last patternMatches() call
static bool g_aborted = false;

// Copy `pattern` into `buf` and terminate every top-level alternative in place,
// so the alternatives can be walked as consecutive NUL-terminated strings.
// Returns the alternative count (at least 1, empty branches included), or 0 if
// the pattern does not fit `buf`. Splitting is purely mechanical — what an
// empty or engine-rejected branch means is each caller's business. A '\'
// escape and a [...] class both hold literal bytes: '\|' and '[|]' are pipes,
// '[\]|]' is a class holding ']'.
static int splitAlternatives(const char* pattern, char* buf, size_t buf_sz) {
  size_t len = strlen(pattern);
  if (len + 1 > buf_sz) return 0;
  memcpy(buf, pattern, len + 1);

  int alts = 1;
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
    buf[i] = 0;          // terminate this alternative in place
    alts++;
  }
  return alts;
}

// Why the engine cannot evaluate an alternative predictably. re_compile()
// never fills in the `ch` of BEGIN/END/quantifier symbols, and matchone()
// compares that byte whenever such a symbol is used as an operand — so these
// shapes match whatever the previously compiled pattern left behind, and a rule
// built on one decides packets differently from one packet to the next. The
// wrapper refuses them instead, both at add time and when matching.
enum AltDefect {
  ALT_OK = 0,
  ALT_NOTHING_TO_REPEAT,   // '*', '+' or '?' with no repeatable symbol before it
  ALT_ANCHOR_MISPLACED,    // '^' or '$' that is not at an edge of the alternative
};

// Classify one alternative by scanning it the way the engine parses it: a '\'
// escape and a [...] class each hold literal bytes, and the escape is checked
// first so '[\]]' does not end the class early. Deterministic by construction —
// every byte is consumed exactly once, and the trailing '\' case the engine
// rejects is skipped rather than stepped over.
static AltDefect altDefect(const char* alt) {
  enum { NONE, EDGE, QUANTIFIED, REPEATABLE } prev = NONE;   // previous symbol
  bool in_class = false;
  for (size_t i = 0; alt[i]; i++) {
    char c = alt[i];
    if (c == '\\') {
      if (alt[i + 1]) i++;                     // the escaped byte is a literal
      if (!in_class) prev = REPEATABLE;
      continue;
    }
    if (in_class) {
      if (c == ']') { in_class = false; prev = REPEATABLE; }   // a class is repeatable
      continue;
    }
    if (c == '[') { in_class = true; continue; }
    if (c == '*' || c == '+' || c == '?') {
      if (prev != REPEATABLE) return ALT_NOTHING_TO_REPEAT;
      prev = QUANTIFIED;   // a quantifier is not itself repeatable
      continue;
    }
    if (c == '^') {
      if (prev != NONE) return ALT_ANCHOR_MISPLACED;   // only anchors at the start
      prev = EDGE;
      continue;
    }
    if (c == '$') {
      if (alt[i + 1] != 0) return ALT_ANCHOR_MISPLACED;   // only at the end
      prev = EDGE;
      continue;
    }
    prev = REPEATABLE;   // ordinary char, '.', or a ']' outside a class
  }
  return ALT_OK;
}

bool patternValid(const char* pattern, char* err, size_t err_sz) {
  err[0] = 0;
  char buf[PATTERN_SPLIT_MAX];
  int alts = splitAlternatives(pattern, buf, sizeof(buf));
  if (alts == 0) return false;                 // too long for buf: the caller's check
  if (alts > FILTER_PATTERN_MAX_ALTS) {
    // reason phrases stay field-agnostic; the caller names the field they came from
    snprintf(err, err_sz, "too many alternatives (max %d)", FILTER_PATTERN_MAX_ALTS);
    return false;
  }
  const char* alt = buf;
  for (int i = 0; i < alts; i++, alt += strlen(alt) + 1) {
    // an empty branch matches everything, so it is never an accident the user
    // meant: "A|", "|A" and "A||B" are refused rather than quietly stored
    if (alt[0] == 0) {
      snprintf(err, err_sz, "empty alternative");
      return false;
    }
    switch (altDefect(alt)) {   // say the actionable thing before "bad regex"
      case ALT_NOTHING_TO_REPEAT:
        snprintf(err, err_sz, "nothing to repeat");
        return false;
      case ALT_ANCHOR_MISPLACED:
        snprintf(err, err_sz, "^ and $ must be at the pattern edges");
        return false;
      default: break;
    }
    if (re_compile(alt) == NULL) return false;   // engine rejects it: no reason given
  }
  return true;
}

bool patternMatches(const char* pattern, const char* subject) {
  g_aborted = false;
  char buf[PATTERN_SPLIT_MAX];
  int alts = splitAlternatives(pattern, buf, sizeof(buf));
  if (alts == 0) return false;   // pattern cannot split: never matches
  const char* alt = buf;
  for (int i = 0; i < alts; i++, alt += strlen(alt) + 1) {
    // Branches that cannot match are skipped rather than failing the whole
    // pattern: an empty one would match everything, one the engine refuses is
    // broken, and one the engine would decide by luck is worse. `filter add`
    // rejects all three, so only a config stored before '|' was an alternation
    // can hold one ("A||B") — the branches around it should keep working.
    if (alt[0] == 0) continue;
    if (altDefect(alt) != ALT_OK) continue;
    re_t compiled = re_compile(alt);
    if (compiled == NULL) continue;
    int matchlength;
    int idx = re_matchp(compiled, subject, &matchlength);
    if (re_budget_exhausted()) { g_aborted = true; return false; }   // fail-open
    if (idx >= 0) return true;
  }
  return false;
}

bool patternAborted(void) {
  return g_aborted;
}
