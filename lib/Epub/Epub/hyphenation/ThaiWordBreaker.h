#pragma once

#include <cstddef>
#include <vector>

#include "HyphenationCommon.h"

class ThaiWordBreaker {
 public:
  // Returns legal Thai line-break positions as codepoint indexes inside cps.
  // Breaks are emitted only at dictionary-derived word boundaries unless
  // includeFallback is enabled, in which case unknown runs additionally expose
  // safe Thai cluster boundaries as a last resort.
  static std::vector<size_t> breakIndexes(const std::vector<CodepointInfo>& cps, bool includeFallback);
};
