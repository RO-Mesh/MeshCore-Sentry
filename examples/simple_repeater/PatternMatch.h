// PatternMatch.h — the user-facing sender=/text= pattern language: the vendored
// regex engine plus top-level '|' alternation and structural validation.
// Personal fork feature; this file pair is fork-owned like the rest of the
// packet filter's code set, and the vendored engine (TinyRegex.h/.cpp) stays
// untouched — everything a user can type is decided here.
//
// Dialect (see FILTER.md for the user-facing reference):
//   - '|' at the top level splits the pattern into alternatives, which are
//     matched in order, first match wins: "Alice|Bob" matches either name.
//     '\|' is a literal pipe and '|' inside a [...] class is a class member.
//   - Anchors bind to their own alternative, standard regex style:
//     "^Alice|Bob$" is "(^Alice)|(Bob$)". For an exact match of either, write
//     "^Alice$|^Bob$".
//   - There are no groups: '(' and ')' stay ordinary characters, as they were
//     before alternation existed, so "^(Alice|Bob)$" matches a sender literally
//     named "(Alice|Bob)" — parentheses are documented as not supported.
//   - At most FILTER_PATTERN_MAX_ALTS alternatives; an empty alternative
//     ("A|", "|A", "A||B") is rejected, because an empty branch matches
//     everything.
// A pattern is stored as text and re-split on every evaluation, so the
// compiled engine state never outlives one evaluation. Cost is bounded by
// FILTER_PATTERN_MAX_ALTS x the engine's per-call step budget, and evaluation
// stops at the first alternative that exhausts that budget (fail-open).

#ifndef _PATTERN_MATCH_H
#define _PATTERN_MATCH_H

#include <stddef.h>

// Syntax-check a pattern at add time. Returns false if it is rejected.
// `err` must be a buffer of at least `err_sz` bytes (err_sz >= 1); it is always
// written and holds a short reason only for rejections made here (empty
// alternative, too many alternatives) — an empty `err` means the vendored
// engine rejected the pattern itself, so the caller should use its own
// "bad/long ... regex" fallback wording.
bool patternValid(const char* pattern, char* err, size_t err_sz);

// True if the pattern matches anywhere in `subject`. On step-budget exhaustion
// the whole evaluation gives up: no match, patternAborted() true, fail-open
// (the engine has always failed open).
bool patternMatches(const char* pattern, const char* subject);

// Non-zero if the last patternMatches() gave up on the step budget. Cleared by
// the next patternMatches() call; the caller counts these as budget aborts.
bool patternAborted(void);

#endif // _PATTERN_MATCH_H