#include <Utf8.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "lib/Epub/Epub/hyphenation/Hyphenator.h"

namespace {

struct TestCase {
  std::string annotated;
  bool includeFallback = false;
};

std::vector<size_t> expectedPositionsFromAnnotatedWord(const std::string& annotated) {
  std::vector<size_t> positions;
  const unsigned char* ptr = reinterpret_cast<const unsigned char*>(annotated.c_str());
  size_t codepointIndex = 0;

  while (*ptr != 0) {
    if (*ptr == '=') {
      positions.push_back(codepointIndex);
      ++ptr;
      continue;
    }

    utf8NextCodepoint(&ptr);
    ++codepointIndex;
  }

  return positions;
}

std::string stripMarkers(const std::string& annotated) {
  std::string word;
  word.reserve(annotated.size());
  for (char ch : annotated) {
    if (ch != '=') {
      word.push_back(ch);
    }
  }
  return word;
}

std::vector<size_t> breakOffsetsToPositions(const std::string& word, const std::vector<Hyphenator::BreakInfo>& breaks) {
  std::vector<size_t> positions;
  positions.reserve(breaks.size());

  for (const auto& info : breaks) {
    const unsigned char* ptr = reinterpret_cast<const unsigned char*>(word.c_str());
    size_t codepointIndex = 0;
    size_t byteOffset = 0;

    while (*ptr != 0 && byteOffset < info.byteOffset) {
      const unsigned char* current = ptr;
      utf8NextCodepoint(&ptr);
      byteOffset += static_cast<size_t>(ptr - current);
      ++codepointIndex;
    }

    positions.push_back(codepointIndex);
  }

  std::sort(positions.begin(), positions.end());
  positions.erase(std::unique(positions.begin(), positions.end()), positions.end());
  return positions;
}

std::string positionsToAnnotated(const std::string& word, const std::vector<size_t>& positions) {
  std::string result;
  const unsigned char* ptr = reinterpret_cast<const unsigned char*>(word.c_str());
  size_t codepointIndex = 0;
  size_t breakIndex = 0;

  while (*ptr != 0) {
    while (breakIndex < positions.size() && positions[breakIndex] == codepointIndex) {
      result.push_back('=');
      ++breakIndex;
    }

    const unsigned char* current = ptr;
    utf8NextCodepoint(&ptr);
    result.append(reinterpret_cast<const char*>(current), reinterpret_cast<const char*>(ptr));
    ++codepointIndex;
  }

  while (breakIndex < positions.size() && positions[breakIndex] == codepointIndex) {
    result.push_back('=');
    ++breakIndex;
  }

  return result;
}

}  // namespace

int main() {
  Hyphenator::setPreferredLanguage("th");

  const std::vector<TestCase> testCases = {
      {"กัน"},
      {"หนัง"},
      {"หนัง=กวาง"},
      {"ตัด"},
      {"ขึ้น"},
      {"ซึ่ง"},
      {"ไว้"},
      {"โดย=จะ=ตัด=การ=เคลื่อนที่=ไป=กลับ=ของ=ก้านสูบ=ออก=ไป"},
      {"ความคิด=ฟุ้งซ่าน=ภายใน=หัว"},
      {"ประสิทธิภาพ=ของ=มัน=ดีกว่า=เครื่องจักร=ไอน้ำ"},
      {"นี่=คือ=ผลิตภัณฑ์=ที่จะ=พลิกโฉม=เมือง"},
      {"ใน=การ=ขับเคลื่อน=แทน"},
  };

  bool success = true;

  for (const auto& testCase : testCases) {
    const std::string word = stripMarkers(testCase.annotated);
    const auto expected = expectedPositionsFromAnnotatedWord(testCase.annotated);
    const auto actual = breakOffsetsToPositions(word, Hyphenator::breakOffsets(word, testCase.includeFallback));

    if (actual != expected) {
      success = false;
      std::cerr << "Mismatch for: " << word << '\n';
      std::cerr << "  Expected: " << testCase.annotated << '\n';
      std::cerr << "  Actual:   " << positionsToAnnotated(word, actual) << '\n';
    }
  }

  if (!success) {
    return 1;
  }

  std::cout << "Thai word-break tests passed (" << testCases.size() << " cases)." << std::endl;
  return 0;
}
