#include "ThaiWordBreaker.h"

#include <Logging.h>
#include <Utf8.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#include "generated/thai_word_dawg.h"

// Static member definition
std::vector<std::vector<uint16_t>> ThaiWordBreaker::userDictWords_;

namespace {

constexpr size_t MAX_USER_DICT_WORDS = 200;

struct MatchEdge {
  size_t endIndex;
};

struct Score {
  int unknownCodepoints = std::numeric_limits<int>::max();
  int unknownRuns = std::numeric_limits<int>::max();
  int segmentCount = std::numeric_limits<int>::max();
  int negativeMatchedSquareSum = std::numeric_limits<int>::max();
};

struct Decision {
  size_t nextIndex = 0;
  bool isKnownWord = false;
  bool valid = false;
  Score score;
};

struct Segment {
  size_t start = 0;
  size_t end = 0;
  bool isKnownWord = false;
};

bool isTerminalNode(const uint16_t nodeIndex) {
  return (thai_word_dawg::kNodeTerminalBits[nodeIndex / 8] >> (nodeIndex % 8)) & 0x01u;
}

bool isBetterScore(const Score& candidate, const Score& current, const size_t candidateLength,
                   const size_t currentLength) {
  if (candidate.unknownCodepoints != current.unknownCodepoints) {
    return candidate.unknownCodepoints < current.unknownCodepoints;
  }
  if (candidate.unknownRuns != current.unknownRuns) {
    return candidate.unknownRuns < current.unknownRuns;
  }
  if (candidate.segmentCount != current.segmentCount) {
    return candidate.segmentCount < current.segmentCount;
  }
  if (candidate.negativeMatchedSquareSum != current.negativeMatchedSquareSum) {
    return candidate.negativeMatchedSquareSum < current.negativeMatchedSquareSum;
  }
  return candidateLength > currentLength;
}

bool isValidThaiBoundary(const uint32_t prevCp, const uint32_t cp) {
  if (isThaiCombining(cp)) return false;
  if (isThaiFollowingVowel(cp)) return false;
  if (isThaiLeadingVowel(prevCp)) return false;
  return true;
}

std::vector<bool> buildBoundaryFlags(const std::vector<CodepointInfo>& cps) {
  std::vector<bool> boundaries(cps.size() + 1, false);
  boundaries.front() = true;
  boundaries.back() = true;

  for (size_t idx = 1; idx < cps.size(); ++idx) {
    boundaries[idx] = isValidThaiBoundary(cps[idx - 1].value, cps[idx].value);
    // Prevent breaking before a consonant followed by การันต์ (์).
    // Such consonants are silent finals belonging to the preceding syllable
    // (e.g. "ไพรซ์" must not split as "ไพร/ซ์").
    if (boundaries[idx] && isThaiConsonant(cps[idx].value) && idx + 1 < cps.size() &&
        cps[idx + 1].value == 0x0E4C) {
      boundaries[idx] = false;
    }
  }

  return boundaries;
}

size_t nextBoundaryIndex(const std::vector<bool>& boundaries, const size_t from) {
  for (size_t idx = from + 1; idx < boundaries.size(); ++idx) {
    if (boundaries[idx]) {
      return idx;
    }
  }
  return boundaries.size() - 1;
}

// Check user dictionary words starting at position `start` in the codepoint array.
// User dictionary words are stored as uint16_t codepoint sequences for compact matching.
void collectUserDictMatchEnds(const std::vector<CodepointInfo>& cps, const size_t start,
                              const std::vector<bool>& boundaries,
                              const std::vector<std::vector<uint16_t>>& userDict,
                              std::vector<MatchEdge>& outMatches) {
  if (userDict.empty()) return;

  for (const auto& dictWord : userDict) {
    const size_t end = start + dictWord.size();
    if (end > cps.size()) continue;
    if (!boundaries[end]) continue;

    bool match = true;
    for (size_t i = 0; i < dictWord.size(); ++i) {
      if (static_cast<uint16_t>(cps[start + i].value) != dictWord[i]) {
        match = false;
        break;
      }
    }

    if (match) {
      outMatches.push_back({end});
    }
  }
}

void collectDictionaryMatchEnds(const std::vector<CodepointInfo>& cps, const size_t start,
                                const std::vector<bool>& boundaries, std::vector<MatchEdge>& outMatches) {
  if (start >= cps.size() || !isThaiCharacter(cps[start].value)) {
    return;
  }

  // Check DAWG dictionary
  uint16_t nodeIndex = thai_word_dawg::kRootNode;
  size_t pos = start;

  while (pos < cps.size()) {
    const uint32_t cp = cps[pos].value;
    if (!isThaiCharacter(cp)) {
      break;
    }

    const uint8_t symbol = static_cast<uint8_t>(cp - 0x0E00u);
    bool advanced = false;

    const uint32_t firstEdge = thai_word_dawg::kNodeFirstEdge[nodeIndex];
    const uint8_t edgeCount = thai_word_dawg::kNodeEdgeCount[nodeIndex];

    for (uint32_t edgeIdx = firstEdge; edgeIdx < firstEdge + edgeCount; ++edgeIdx) {
      const auto& edge = thai_word_dawg::kEdges[edgeIdx];
      const uint8_t* label = thai_word_dawg::kLabelData + edge.labelOffset;
      if (label[0] != symbol) {
        continue;
      }

      size_t scan = pos;
      bool matched = true;
      for (uint8_t labelIdx = 0; labelIdx < edge.labelLength; ++labelIdx) {
        if (scan >= cps.size() || !isThaiCharacter(cps[scan].value) ||
            static_cast<uint8_t>(cps[scan].value - 0x0E00u) != label[labelIdx]) {
          matched = false;
          break;
        }
        ++scan;
      }

      if (!matched) {
        continue;
      }

      nodeIndex = edge.childIndex;
      pos = scan;
      advanced = true;

      if (isTerminalNode(nodeIndex) && boundaries[pos]) {
        outMatches.push_back({pos});
      }
      break;
    }

    if (!advanced) {
      break;
    }
  }

  // Also check user dictionary words (supplements the DAWG)
  collectUserDictMatchEnds(cps, start, boundaries, ThaiWordBreaker::getUserDictWords(), outMatches);
}

std::vector<Segment> buildSegments(const std::vector<Decision>& decisions, const size_t length) {
  std::vector<Segment> segments;
  size_t index = 0;

  while (index < length) {
    const auto& decision = decisions[index];
    if (!decision.valid || decision.nextIndex <= index) {
      break;
    }

    if (!segments.empty() && !decision.isKnownWord && !segments.back().isKnownWord && segments.back().end == index) {
      segments.back().end = decision.nextIndex;
    } else {
      segments.push_back({index, decision.nextIndex, decision.isKnownWord});
    }

    index = decision.nextIndex;
  }

  return segments;
}

}  // namespace

std::vector<size_t> ThaiWordBreaker::breakIndexes(const std::vector<CodepointInfo>& cps, const bool includeFallback) {
  if (cps.size() < 2) {
    return {};
  }

  const auto boundaries = buildBoundaryFlags(cps);
  std::vector<Decision> decisions(cps.size() + 1);
  decisions[cps.size()].valid = true;
  decisions[cps.size()].score = {0, 0, 0, 0};

  std::vector<MatchEdge> matches;

  for (size_t i = cps.size(); i-- > 0;) {
    if (!boundaries[i]) {
      continue;
    }

    matches.clear();
    collectDictionaryMatchEnds(cps, i, boundaries, matches);

    Decision best;

    for (auto it = matches.rbegin(); it != matches.rend(); ++it) {
      const size_t end = it->endIndex;
      const auto& suffix = decisions[end];
      if (!suffix.valid) {
        continue;
      }

      Decision candidate;
      candidate.valid = true;
      candidate.isKnownWord = true;
      candidate.nextIndex = end;
      candidate.score = suffix.score;
      candidate.score.segmentCount += 1;
      const size_t length = end - i;
      candidate.score.negativeMatchedSquareSum -= static_cast<int>(length * length);

      if (!best.valid || isBetterScore(candidate.score, best.score, candidate.nextIndex - i, best.nextIndex - i)) {
        best = candidate;
      }
    }

    const size_t fallbackEnd = nextBoundaryIndex(boundaries, i);
    if (fallbackEnd > i) {
      const auto& suffix = decisions[fallbackEnd];
      if (suffix.valid) {
        Decision candidate;
        candidate.valid = true;
        candidate.isKnownWord = false;
        candidate.nextIndex = fallbackEnd;
        candidate.score = suffix.score;
        candidate.score.unknownCodepoints += static_cast<int>(fallbackEnd - i);
        candidate.score.unknownRuns += 1;
        candidate.score.segmentCount += 1;

        if (!best.valid || isBetterScore(candidate.score, best.score, candidate.nextIndex - i, best.nextIndex - i)) {
          best = candidate;
        }
      }
    }

    decisions[i] = best;
  }

  if (!decisions[0].valid) {
    return {};
  }

  const auto segments = buildSegments(decisions, cps.size());
  if (segments.empty()) {
    return {};
  }

  std::vector<size_t> breaks;
  breaks.reserve(cps.size());

  for (size_t idx = 0; idx + 1 < segments.size(); ++idx) {
    breaks.push_back(segments[idx].end);
  }

  if (includeFallback) {
    for (const auto& segment : segments) {
      if (segment.isKnownWord || segment.end - segment.start < 2) {
        continue;
      }
      // For unknown segments (e.g. transliterated words not in the dictionary),
      // only break at positions that look like Thai syllable starts, not at
      // every valid boundary. This prevents aggressive splits like "ท/ริ/ค".
      for (size_t idx = segment.start + 1; idx < segment.end; ++idx) {
        if (!boundaries[idx]) {
          continue;
        }
        const uint32_t cp = cps[idx].value;
        // Leading vowels (เ แ โ ไ ใ) always start a new syllable
        if (isThaiLeadingVowel(cp)) {
          breaks.push_back(idx);
        } else if (isThaiConsonant(cp)) {
          // Skip consonants that form a cluster with the preceding consonant
          // (ร ล ว after another consonant, e.g. กร ปล คว)
          if (idx > segment.start && isThaiConsonant(cps[idx - 1].value) &&
              (cp == 0x0E23 || cp == 0x0E25 || cp == 0x0E27)) {
            continue;
          }
          breaks.push_back(idx);
        }
      }
    }
  }

  std::sort(breaks.begin(), breaks.end());
  breaks.erase(std::unique(breaks.begin(), breaks.end()), breaks.end());
  return breaks;
}

void ThaiWordBreaker::setUserDictionary(const std::vector<std::string>& words) {
  userDictWords_.clear();

  const size_t count = std::min(words.size(), MAX_USER_DICT_WORDS);
  userDictWords_.reserve(count);

  for (size_t i = 0; i < count; ++i) {
    const auto& word = words[i];
    if (word.empty()) continue;

    // Convert UTF-8 word to a compact uint16_t codepoint sequence.
    // Thai codepoints (U+0E00–U+0E7F) fit in 16 bits.
    std::vector<uint16_t> cps;
    cps.reserve(word.size() / 3 + 1);  // Thai chars are 3 bytes in UTF-8

    const auto* ptr = reinterpret_cast<const unsigned char*>(word.c_str());
    uint32_t cp;
    while ((cp = utf8NextCodepoint(&ptr))) {
      cps.push_back(static_cast<uint16_t>(cp));
    }

    if (cps.size() >= 2) {
      userDictWords_.push_back(std::move(cps));
    }
  }

  if (!userDictWords_.empty()) {
    LOG_INF("THAI", "Loaded %d user dictionary words", static_cast<int>(userDictWords_.size()));
  }
}

bool ThaiWordBreaker::hasUserDictionary() { return !userDictWords_.empty(); }
