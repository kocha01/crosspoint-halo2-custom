#include "ParsedText.h"

#include <GfxRenderer.h>
#include <Utf8.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>

#include "hyphenation/Hyphenator.h"

constexpr int MAX_COST = std::numeric_limits<int>::max();

namespace {

// Soft hyphen byte pattern used throughout EPUBs (UTF-8 for U+00AD).
constexpr char SOFT_HYPHEN_UTF8[] = "\xC2\xAD";
constexpr size_t SOFT_HYPHEN_BYTES = 2;
constexpr char ZERO_WIDTH_SPACE_UTF8[] = "\xE2\x80\x8B";

// Returns the first rendered codepoint of a word (skipping leading soft hyphens).
uint32_t firstCodepoint(const std::string& word) {
  const auto* ptr = reinterpret_cast<const unsigned char*>(word.c_str());
  while (true) {
    const uint32_t cp = utf8NextCodepoint(&ptr);
    if (cp == 0) return 0;
    if (cp != 0x00AD) return cp;  // skip soft hyphens
  }
}

// Returns the last codepoint of a word by scanning backward for the start of the last UTF-8 sequence.
uint32_t lastCodepoint(const std::string& word) {
  if (word.empty()) return 0;
  // UTF-8 continuation bytes start with 10xxxxxx; scan backward to find the leading byte.
  size_t i = word.size() - 1;
  while (i > 0 && (static_cast<uint8_t>(word[i]) & 0xC0) == 0x80) {
    --i;
  }
  const auto* ptr = reinterpret_cast<const unsigned char*>(word.c_str() + i);
  return utf8NextCodepoint(&ptr);
}

bool containsSoftHyphen(const std::string& word) { return word.find(SOFT_HYPHEN_UTF8) != std::string::npos; }

bool isZeroWidthBreakToken(const std::string& word) { return word == ZERO_WIDTH_SPACE_UTF8; }

// Returns true if the word contains at least 2 Thai codepoints (U+0E00–U+0E7F),
// indicating it is a Thai word worth auto-segmenting for DP line breaking.
bool containsThaiText(const std::string& word) {
  int thaiCount = 0;
  const auto* ptr = reinterpret_cast<const unsigned char*>(word.c_str());
  uint32_t cp;
  while ((cp = utf8NextCodepoint(&ptr))) {
    if (cp >= 0x0E00 && cp <= 0x0E7F) {
      if (++thaiCount >= 2) return true;
    }
  }
  return false;
}

bool hasZeroWidthBreakTokens(const std::vector<std::string>& words) {
  return std::any_of(words.begin(), words.end(), [](const std::string& word) { return isZeroWidthBreakToken(word); });
}

long long lineBadness(const int remainingSpace, const int lineWidth, const bool isLastLine,
                      const bool preferFullLines) {
  if (remainingSpace < 0) {
    return MAX_COST;
  }

  const long long slack = static_cast<long long>(remainingSpace);
  const long long baseCost = slack * slack;

  if (!preferFullLines) {
    return isLastLine ? 0 : baseCost;
  }

  // Thai paragraphs without visible word gaps look much better when the breaker
  // strongly prefers fuller lines. Add a cubic term so large trailing whitespace
  // becomes disproportionately expensive, while keeping the last line penalty mild.
  const long long cubicCost = (baseCost * slack) / std::max(1, lineWidth);
  long long cost = baseCost + cubicCost;

  const int usedWidth = lineWidth - remainingSpace;
  if (!isLastLine && usedWidth * 100 < lineWidth * 72) {
    cost += static_cast<long long>(lineWidth) * lineWidth;
  }

  if (isLastLine) {
    cost /= 6;
  }

  return cost;
}

// Removes every soft hyphen in-place so rendered glyphs match measured widths.
void stripSoftHyphensInPlace(std::string& word) {
  size_t pos = 0;
  while ((pos = word.find(SOFT_HYPHEN_UTF8, pos)) != std::string::npos) {
    word.erase(pos, SOFT_HYPHEN_BYTES);
  }
}

// Returns the advance width for a word while ignoring soft hyphen glyphs and optionally appending a visible hyphen.
// Uses advance width (sum of glyph advances + kerning) rather than bounding box width so that italic glyph overhangs
// don't inflate inter-word spacing.
bool requiresFitWidthMeasurement(const std::string& word) {
  const auto* ptr = reinterpret_cast<const unsigned char*>(word.c_str());
  uint32_t cp;
  while ((cp = utf8NextCodepoint(&ptr))) {
    if (cp >= 0x0E00 && cp <= 0x0E7F) {
      return true;
    }
  }
  return false;
}

uint16_t measureWordWidth(const GfxRenderer& renderer, const int fontId, const std::string& word,
                          const EpdFontFamily::Style style, const bool appendHyphen = false) {
  if (isZeroWidthBreakToken(word) && !appendHyphen) {
    return 0;
  }
  if (word.size() == 1 && word[0] == ' ' && !appendHyphen) {
    return renderer.getSpaceWidth(fontId, style);
  }
  const bool hasSoftHyphen = containsSoftHyphen(word);
  if (!hasSoftHyphen && !appendHyphen) {
    return requiresFitWidthMeasurement(word) ? renderer.getTextFitWidth(fontId, word.c_str(), style)
                                             : renderer.getTextAdvanceX(fontId, word.c_str(), style);
  }

  std::string sanitized = word;
  if (hasSoftHyphen) {
    stripSoftHyphensInPlace(sanitized);
  }
  if (appendHyphen) {
    sanitized.push_back('-');
  }
  return requiresFitWidthMeasurement(sanitized) ? renderer.getTextFitWidth(fontId, sanitized.c_str(), style)
                                                : renderer.getTextAdvanceX(fontId, sanitized.c_str(), style);
}

int boundaryAdvance(const GfxRenderer& renderer, const int fontId, const std::string& prevWord,
                    const std::string& nextWord, const EpdFontFamily::Style prevStyle, const bool nextContinues) {
  if (isZeroWidthBreakToken(prevWord) || isZeroWidthBreakToken(nextWord)) {
    return 0;
  }

  if (nextContinues) {
    return renderer.getKerning(fontId, lastCodepoint(prevWord), firstCodepoint(nextWord), prevStyle);
  }

  return renderer.getSpaceAdvance(fontId, lastCodepoint(prevWord), firstCodepoint(nextWord), prevStyle);
}

bool countsAsVisibleGap(const std::string& prevWord, const std::string& nextWord, const bool nextContinues) {
  return !nextContinues && !isZeroWidthBreakToken(prevWord) && !isZeroWidthBreakToken(nextWord);
}

size_t continuationGroupEndExclusive(const size_t start, const std::vector<bool>& continuesVec) {
  size_t end = start + 1;
  while (end < continuesVec.size() && continuesVec[end]) {
    ++end;
  }
  return end;
}

}  // namespace

void ParsedText::addWord(std::string word, const EpdFontFamily::Style fontStyle, const bool underline,
                         const bool attachToPrevious) {
  if (word.empty()) return;

  // Auto-segment Thai text to enable DP line breaking.
  // Words with ≥2 Thai codepoints are split at dictionary word boundaries
  // and ZWSP tokens inserted between segments, allowing the DP layout
  // engine to find optimal line break positions without requiring
  // pre-processed EPUB content.
  if (word.size() >= 6 && containsThaiText(word)) {
    autoSegmentThaiWord(std::move(word), fontStyle, underline, attachToPrevious);
    return;
  }

  addWordInternal(std::move(word), fontStyle, underline, attachToPrevious);
}

void ParsedText::addWordInternal(std::string word, const EpdFontFamily::Style fontStyle, const bool underline,
                                 const bool attachToPrevious) {
  words.push_back(std::move(word));
  EpdFontFamily::Style combinedStyle = fontStyle;
  if (underline) {
    combinedStyle = static_cast<EpdFontFamily::Style>(combinedStyle | EpdFontFamily::UNDERLINE);
  }
  wordStyles.push_back(combinedStyle);
  wordContinues.push_back(attachToPrevious);
}

void ParsedText::autoSegmentThaiWord(std::string word, const EpdFontFamily::Style fontStyle, const bool underline,
                                     const bool attachToPrevious) {
  // Use the hyphenation engine to find Thai dictionary word boundaries.
  // includeFallback=false ensures only dictionary-matched boundaries are used,
  // not cluster-level fallback splits.
  auto breakInfos = Hyphenator::breakOffsets(word, false);

  if (breakInfos.empty()) {
    // No break points found — add the word as a single token.
    addWordInternal(std::move(word), fontStyle, underline, attachToPrevious);
    return;
  }

  // Split word at each break offset and insert ZWSP tokens between segments.
  // ZWSP tokens enable the DP line breaker path (Mode A) and allow the layout
  // engine to distribute Thai text across lines without visible word gaps.
  size_t lastOffset = 0;
  bool isFirst = true;

  for (const auto& info : breakInfos) {
    if (info.byteOffset <= lastOffset || info.byteOffset >= word.size()) continue;

    std::string segment = word.substr(lastOffset, info.byteOffset - lastOffset);
    if (!segment.empty()) {
      if (isFirst) {
        addWordInternal(std::move(segment), fontStyle, underline, attachToPrevious);
        isFirst = false;
      } else {
        // Insert ZWSP as a zero-width break opportunity, then the segment.
        // Both are marked as continuations (attachToPrevious=true) so they
        // render without visible inter-word spacing.
        addWordInternal(std::string(ZERO_WIDTH_SPACE_UTF8), fontStyle, false, true);
        addWordInternal(std::move(segment), fontStyle, underline, true);
      }
    }
    lastOffset = info.byteOffset;
  }

  // Add the remaining part after the last break point.
  if (lastOffset < word.size()) {
    std::string remaining = word.substr(lastOffset);
    if (!remaining.empty()) {
      if (isFirst) {
        addWordInternal(std::move(remaining), fontStyle, underline, attachToPrevious);
      } else {
        addWordInternal(std::string(ZERO_WIDTH_SPACE_UTF8), fontStyle, false, true);
        addWordInternal(std::move(remaining), fontStyle, underline, true);
      }
    }
  }
}

// Consumes data to minimize memory usage
void ParsedText::layoutAndExtractLines(const GfxRenderer& renderer, const int fontId, const uint16_t viewportWidth,
                                       const std::function<void(std::shared_ptr<TextBlock>)>& processLine,
                                       const bool includeLastLine) {
  if (words.empty()) {
    return;
  }

  // Apply fixed transforms before any per-line layout work.
  applyParagraphIndent();

  const int pageWidth = viewportWidth;
  auto wordWidths = calculateWordWidths(renderer, fontId);

  std::vector<size_t> lineBreakIndices;
  // When the parser has already inserted zero-width break markers (for example
  // between Thai dictionary words), use the DP line breaker instead of the
  // greedy hyphenation loop. This produces noticeably fuller lines without
  // stretching glyph spacing, while still preserving gapless Thai rendering.
  if (hyphenationEnabled && !hasZeroWidthBreakTokens(words)) {
    // Use greedy layout that can split words mid-loop when a hyphenated prefix fits.
    lineBreakIndices = computeHyphenatedLineBreaks(renderer, fontId, pageWidth, wordWidths, wordContinues);
  } else {
    lineBreakIndices = computeLineBreaks(renderer, fontId, pageWidth, wordWidths, wordContinues);
  }
  const size_t lineCount = includeLastLine ? lineBreakIndices.size() : lineBreakIndices.size() - 1;

  for (size_t i = 0; i < lineCount; ++i) {
    extractLine(i, pageWidth, wordWidths, wordContinues, lineBreakIndices, processLine, renderer, fontId);
  }

  // Remove consumed words so size() reflects only remaining words
  if (lineCount > 0) {
    const size_t consumed = lineBreakIndices[lineCount - 1];
    words.erase(words.begin(), words.begin() + consumed);
    wordStyles.erase(wordStyles.begin(), wordStyles.begin() + consumed);
    wordContinues.erase(wordContinues.begin(), wordContinues.begin() + consumed);
  }
}

std::vector<uint16_t> ParsedText::calculateWordWidths(const GfxRenderer& renderer, const int fontId) {
  std::vector<uint16_t> wordWidths;
  wordWidths.reserve(words.size());

  for (size_t i = 0; i < words.size(); ++i) {
    wordWidths.push_back(measureWordWidth(renderer, fontId, words[i], wordStyles[i]));
  }

  return wordWidths;
}

std::vector<size_t> ParsedText::computeLineBreaks(const GfxRenderer& renderer, const int fontId, const int pageWidth,
                                                  std::vector<uint16_t>& wordWidths, std::vector<bool>& continuesVec) {
  if (words.empty()) {
    return {};
  }

  // Calculate first line indent (only for left/justified text).
  // Positive text-indent (paragraph indent) is suppressed when extraParagraphSpacing is on.
  // Negative text-indent (hanging indent, e.g. margin-left:3em; text-indent:-1em) always applies —
  // it is structural (positions the bullet/marker), not decorative.
  const int firstLineIndent =
      blockStyle.textIndentDefined && (blockStyle.textIndent < 0 || !extraParagraphSpacing) &&
              (blockStyle.alignment == CssTextAlign::Justify || blockStyle.alignment == CssTextAlign::Left)
          ? blockStyle.textIndent
          : 0;

  // Ensure any word that would overflow even as the first entry on a line is split using fallback hyphenation.
  for (size_t i = 0; i < wordWidths.size(); ++i) {
    // First word needs to fit in reduced width if there's an indent
    const int effectiveWidth = i == 0 ? pageWidth - firstLineIndent : pageWidth;
    while (wordWidths[i] > effectiveWidth) {
      if (!hyphenateWordAtIndex(i, effectiveWidth, renderer, fontId, wordWidths, /*allowFallbackBreaks=*/true)) {
        break;
      }
    }
  }

  const size_t totalWordCount = words.size();
  const bool preferFullLines = hasZeroWidthBreakTokens(words);

  // DP table to store the minimum badness (cost) of lines starting at index i
  std::vector<int> dp(totalWordCount);
  // 'ans[i]' stores the index 'j' of the *last word* in the optimal line starting at 'i'
  std::vector<size_t> ans(totalWordCount);

  // Base Case
  dp[totalWordCount - 1] = 0;
  ans[totalWordCount - 1] = totalWordCount - 1;

  for (int i = totalWordCount - 2; i >= 0; --i) {
    int currlen = 0;
    dp[i] = MAX_COST;

    // First line has reduced width due to text-indent
    const int effectivePageWidth = i == 0 ? pageWidth - firstLineIndent : pageWidth;

    for (size_t j = i; j < totalWordCount; ++j) {
      // Add space before word j, unless it's the first word on the line or a continuation
      int gap = 0;
      if (j > static_cast<size_t>(i)) {
        gap = boundaryAdvance(renderer, fontId, words[j - 1], words[j], wordStyles[j - 1], continuesVec[j]);
      }
      currlen += wordWidths[j] + gap;

      if (currlen > effectivePageWidth) {
        break;
      }

      // Cannot break after word j if the next word attaches to it (continuation group)
      if (j + 1 < totalWordCount && continuesVec[j + 1]) {
        continue;
      }

      const int remainingSpace = effectivePageWidth - currlen;
      const bool isLastLine = j == totalWordCount - 1;
      const long long lineCost = lineBadness(remainingSpace, effectivePageWidth, isLastLine, preferFullLines);
      const long long suffixCost = isLastLine ? 0 : dp[j + 1];
      const long long cost_ll = lineCost + suffixCost;
      const int cost = cost_ll > MAX_COST ? MAX_COST : static_cast<int>(cost_ll);

      if (cost < dp[i]) {
        dp[i] = cost;
        ans[i] = j;  // j is the index of the last word in this optimal line
      }
    }

    // Handle oversized word: if no valid configuration found, force single-word line
    // This prevents cascade failure where one oversized word breaks all preceding words.
    // Continuation groups must stay intact even when they overflow the page width.
    if (dp[i] == MAX_COST) {
      const size_t forcedGroupEnd = continuationGroupEndExclusive(static_cast<size_t>(i), continuesVec);
      ans[i] = forcedGroupEnd - 1;
      // Inherit cost from the first word after the forced group so subsequent words can still lay out normally.
      if (forcedGroupEnd < totalWordCount) {
        dp[i] = dp[forcedGroupEnd];
      } else {
        dp[i] = 0;
      }
    }
  }

  // Stores the index of the word that starts the next line (last_word_index + 1)
  std::vector<size_t> lineBreakIndices;
  size_t currentWordIndex = 0;

  while (currentWordIndex < totalWordCount) {
    size_t nextBreakIndex = ans[currentWordIndex] + 1;

    // Safety check: prevent infinite loop if nextBreakIndex doesn't advance
    if (nextBreakIndex <= currentWordIndex) {
      // Force advance by at least one word to avoid infinite loop
      nextBreakIndex = currentWordIndex + 1;
    }

    lineBreakIndices.push_back(nextBreakIndex);
    currentWordIndex = nextBreakIndex;
  }

  return lineBreakIndices;
}

void ParsedText::applyParagraphIndent() {
  if (extraParagraphSpacing || words.empty()) {
    return;
  }

  if (blockStyle.textIndentDefined) {
    // CSS text-indent is explicitly set (even if 0) - don't use fallback EmSpace
    // The actual indent positioning is handled in extractLine()
  } else if (blockStyle.alignment == CssTextAlign::Justify || blockStyle.alignment == CssTextAlign::Left) {
    // No CSS text-indent defined - use EmSpace fallback for visual indent
    words.front().insert(0, "\xe2\x80\x83");
  }
}

// Builds break indices while opportunistically splitting the word that would overflow the current line.
std::vector<size_t> ParsedText::computeHyphenatedLineBreaks(const GfxRenderer& renderer, const int fontId,
                                                            const int pageWidth, std::vector<uint16_t>& wordWidths,
                                                            std::vector<bool>& continuesVec) {
  // Calculate first line indent (only for left/justified text).
  // Positive text-indent (paragraph indent) is suppressed when extraParagraphSpacing is on.
  // Negative text-indent (hanging indent, e.g. margin-left:3em; text-indent:-1em) always applies —
  // it is structural (positions the bullet/marker), not decorative.
  const int firstLineIndent =
      blockStyle.textIndentDefined && (blockStyle.textIndent < 0 || !extraParagraphSpacing) &&
              (blockStyle.alignment == CssTextAlign::Justify || blockStyle.alignment == CssTextAlign::Left)
          ? blockStyle.textIndent
          : 0;

  std::vector<size_t> lineBreakIndices;
  size_t currentIndex = 0;
  bool isFirstLine = true;

  while (currentIndex < wordWidths.size()) {
    const size_t lineStart = currentIndex;
    int lineWidth = 0;

    // First line has reduced width due to text-indent
    const int effectivePageWidth = isFirstLine ? pageWidth - firstLineIndent : pageWidth;

    // Consume as many words as possible for current line, splitting when prefixes fit
    while (currentIndex < wordWidths.size()) {
      const bool isFirstWord = currentIndex == lineStart;
      int spacing = 0;
      if (!isFirstWord) {
        spacing = boundaryAdvance(renderer, fontId, words[currentIndex - 1], words[currentIndex],
                                  wordStyles[currentIndex - 1], continuesVec[currentIndex]);
      }
      const int candidateWidth = spacing + wordWidths[currentIndex];

      // Word fits on current line
      if (lineWidth + candidateWidth <= effectivePageWidth) {
        lineWidth += candidateWidth;
        ++currentIndex;
        continue;
      }

      // Word would overflow — try to split based on hyphenation points
      const int availableWidth = effectivePageWidth - lineWidth - spacing;
      const bool allowFallbackBreaks = isFirstWord;  // Only for first word on line

      if (availableWidth > 0 &&
          hyphenateWordAtIndex(currentIndex, availableWidth, renderer, fontId, wordWidths, allowFallbackBreaks)) {
        // Prefix now fits; append it to this line and move to next line
        lineWidth += spacing + wordWidths[currentIndex];
        ++currentIndex;
        break;
      }

      // Could not split: force at least one word per line to avoid infinite loop
      if (currentIndex == lineStart) {
        lineWidth += candidateWidth;
        ++currentIndex;
      }
      break;
    }

    // Don't break inside a continuation group (e.g., inline-styled word fragments or non-breaking spaces).
    // If the overflowing fragment belongs to a group that started earlier on this line, move the whole group
    // to the next line. If the group itself starts the line, keep the whole group together on this line rather
    // than splitting in the middle.
    if (currentIndex < wordWidths.size() && continuesVec[currentIndex]) {
      size_t continuationStart = currentIndex;
      while (continuationStart > lineStart && continuesVec[continuationStart]) {
        --continuationStart;
      }

      if (continuationStart > lineStart) {
        currentIndex = continuationStart;
      } else {
        currentIndex = continuationGroupEndExclusive(lineStart, continuesVec);
      }
    }

    lineBreakIndices.push_back(currentIndex);
    isFirstLine = false;
  }

  return lineBreakIndices;
}

// Splits words[wordIndex] into prefix (adding a hyphen only when needed) and remainder when a legal breakpoint fits the
// available width.
bool ParsedText::hyphenateWordAtIndex(const size_t wordIndex, const int availableWidth, const GfxRenderer& renderer,
                                      const int fontId, std::vector<uint16_t>& wordWidths,
                                      const bool allowFallbackBreaks) {
  // Guard against invalid indices or zero available width before attempting to split.
  if (availableWidth <= 0 || wordIndex >= words.size()) {
    return false;
  }

  const std::string& word = words[wordIndex];
  const auto style = wordStyles[wordIndex];

  // Collect candidate breakpoints (byte offsets and hyphen requirements).
  auto breakInfos = Hyphenator::breakOffsets(word, allowFallbackBreaks);
  if (breakInfos.empty()) {
    return false;
  }

  size_t chosenOffset = 0;
  int chosenWidth = -1;
  bool chosenNeedsHyphen = true;

  // Iterate over each legal breakpoint and retain the widest prefix that still fits.
  for (const auto& info : breakInfos) {
    const size_t offset = info.byteOffset;
    if (offset == 0 || offset >= word.size()) {
      continue;
    }

    const bool needsHyphen = info.requiresInsertedHyphen;
    const int prefixWidth = measureWordWidth(renderer, fontId, word.substr(0, offset), style, needsHyphen);
    if (prefixWidth > availableWidth || prefixWidth <= chosenWidth) {
      continue;  // Skip if too wide or not an improvement
    }

    chosenWidth = prefixWidth;
    chosenOffset = offset;
    chosenNeedsHyphen = needsHyphen;
  }

  if (chosenWidth < 0) {
    // No hyphenation point produced a prefix that fits in the remaining space.
    return false;
  }

  // Split the word at the selected breakpoint and append a hyphen if required.
  std::string remainder = word.substr(chosenOffset);
  words[wordIndex].resize(chosenOffset);
  if (chosenNeedsHyphen) {
    words[wordIndex].push_back('-');
  }

  // Insert the remainder word (with matching style and continuation flag) directly after the prefix.
  words.insert(words.begin() + wordIndex + 1, remainder);
  wordStyles.insert(wordStyles.begin() + wordIndex + 1, style);

  // Continuation flag handling after splitting a word into prefix + remainder.
  //
  // The prefix keeps the original word's continuation flag so that no-break-space groups
  // stay linked. The remainder always gets continues=false because it starts on the next
  // line and is not attached to the prefix.
  //
  // Example: "200&#xA0;Quadratkilometer" produces tokens:
  //   [0] "200"               continues=false
  //   [1] " "                 continues=true
  //   [2] "Quadratkilometer"  continues=true   <-- the word being split
  //
  // After splitting "Quadratkilometer" at "Quadrat-" / "kilometer":
  //   [0] "200"         continues=false
  //   [1] " "           continues=true
  //   [2] "Quadrat-"    continues=true   (KEPT — still attached to the no-break group)
  //   [3] "kilometer"   continues=false  (NEW — starts fresh on the next line)
  //
  // This lets the backtracking loop keep the entire prefix group ("200 Quadrat-") on one
  // line, while "kilometer" moves to the next line.
  // wordContinues[wordIndex] is intentionally left unchanged — the prefix keeps its original attachment.
  wordContinues.insert(wordContinues.begin() + wordIndex + 1, false);

  // Update cached widths to reflect the new prefix/remainder pairing.
  wordWidths[wordIndex] = static_cast<uint16_t>(chosenWidth);
  const uint16_t remainderWidth = measureWordWidth(renderer, fontId, remainder, style);
  wordWidths.insert(wordWidths.begin() + wordIndex + 1, remainderWidth);
  return true;
}

void ParsedText::extractLine(const size_t breakIndex, const int pageWidth, const std::vector<uint16_t>& wordWidths,
                             const std::vector<bool>& continuesVec, const std::vector<size_t>& lineBreakIndices,
                             const std::function<void(std::shared_ptr<TextBlock>)>& processLine,
                             const GfxRenderer& renderer, const int fontId) {
  const size_t lineBreak = lineBreakIndices[breakIndex];
  const size_t lastBreakAt = breakIndex > 0 ? lineBreakIndices[breakIndex - 1] : 0;
  const size_t lineWordCount = lineBreak - lastBreakAt;

  // Calculate first line indent (only for left/justified text).
  // Positive text-indent (paragraph indent) is suppressed when extraParagraphSpacing is on.
  // Negative text-indent (hanging indent, e.g. margin-left:3em; text-indent:-1em) always applies —
  // it is structural (positions the bullet/marker), not decorative.
  const bool isFirstLine = breakIndex == 0;
  const int firstLineIndent =
      isFirstLine && blockStyle.textIndentDefined && (blockStyle.textIndent < 0 || !extraParagraphSpacing) &&
              (blockStyle.alignment == CssTextAlign::Justify || blockStyle.alignment == CssTextAlign::Left)
          ? blockStyle.textIndent
          : 0;

  // Calculate total word width for this line, count actual word gaps,
  // and accumulate total natural gap widths (including space kerning adjustments).
  int lineWordWidthSum = 0;
  size_t actualGapCount = 0;
  int totalNaturalGaps = 0;

  for (size_t wordIdx = 0; wordIdx < lineWordCount; wordIdx++) {
    lineWordWidthSum += wordWidths[lastBreakAt + wordIdx];
    // Count gaps: each word after the first creates a gap, unless it's a continuation
    if (wordIdx > 0) {
      const auto globalIdx = lastBreakAt + wordIdx;
      const int gap = boundaryAdvance(renderer, fontId, words[globalIdx - 1], words[globalIdx], wordStyles[globalIdx - 1],
                                      continuesVec[globalIdx]);
      totalNaturalGaps += gap;
      if (countsAsVisibleGap(words[globalIdx - 1], words[globalIdx], continuesVec[globalIdx])) {
        actualGapCount++;
      } else if (isZeroWidthBreakToken(words[globalIdx])) {
        // Thai word boundary: ZWS marker counts as one justify gap
        actualGapCount++;
      }
    }
  }

  // Calculate spacing (account for indent reducing effective page width on first line)
  const int effectivePageWidth = pageWidth - firstLineIndent;
  const bool isLastLine = breakIndex == lineBreakIndices.size() - 1;

  // For justified text, compute per-gap extra to distribute remaining space evenly
  const int spareSpace = effectivePageWidth - lineWordWidthSum - totalNaturalGaps;
  const int justifyExtra = (blockStyle.alignment == CssTextAlign::Justify && !isLastLine && actualGapCount >= 1)
                               ? spareSpace / static_cast<int>(actualGapCount)
                               : 0;

  // Calculate initial x position (first line starts at indent for left/justified text;
  // may be negative for hanging indents, e.g. margin-left:3em; text-indent:-1em).
  auto xpos = static_cast<int16_t>(firstLineIndent);
  if (blockStyle.alignment == CssTextAlign::Right) {
    xpos = effectivePageWidth - lineWordWidthSum - totalNaturalGaps;
  } else if (blockStyle.alignment == CssTextAlign::Center) {
    xpos = (effectivePageWidth - lineWordWidthSum - totalNaturalGaps) / 2;
  }

  // Pre-calculate X positions for words
  // Continuation words attach to the previous word with no space before them
  std::vector<int16_t> lineXPos;
  lineXPos.reserve(lineWordCount);

  for (size_t wordIdx = 0; wordIdx < lineWordCount; wordIdx++) {
    lineXPos.push_back(xpos);

    const bool nextIsContinuation = wordIdx + 1 < lineWordCount && continuesVec[lastBreakAt + wordIdx + 1];
    if (nextIsContinuation) {
      int advance = wordWidths[lastBreakAt + wordIdx];
      advance += boundaryAdvance(renderer, fontId, words[lastBreakAt + wordIdx], words[lastBreakAt + wordIdx + 1],
                                 wordStyles[lastBreakAt + wordIdx], true);
      xpos += advance;
    } else {
      int gap = 0;
      if (wordIdx + 1 < lineWordCount) {
        gap = boundaryAdvance(renderer, fontId, words[lastBreakAt + wordIdx], words[lastBreakAt + wordIdx + 1],
                              wordStyles[lastBreakAt + wordIdx], false);
      }
      if (blockStyle.alignment == CssTextAlign::Justify && !isLastLine) {
        if (gap > 0) {
          gap += justifyExtra;  // Latin/space gap: add to natural space
        } else if (isZeroWidthBreakToken(words[lastBreakAt + wordIdx])) {
          gap = justifyExtra;  // Thai word boundary: inject extra after ZWS pivot
        }
      }
      xpos += wordWidths[lastBreakAt + wordIdx] + gap;
    }
  }

  // Build line data by moving from the original vectors using index range
  std::vector<std::string> lineWords(std::make_move_iterator(words.begin() + lastBreakAt),
                                     std::make_move_iterator(words.begin() + lineBreak));
  std::vector<EpdFontFamily::Style> lineWordStyles(wordStyles.begin() + lastBreakAt, wordStyles.begin() + lineBreak);

  for (auto& word : lineWords) {
    if (containsSoftHyphen(word)) {
      stripSoftHyphensInPlace(word);
    }
  }

  processLine(
      std::make_shared<TextBlock>(std::move(lineWords), std::move(lineXPos), std::move(lineWordStyles), blockStyle));
}
