#ifndef PHONEME_PARSER_H_
#define PHONEME_PARSER_H_

#include <string>
#include <vector>
#include <regex>
#include <cstdint>

namespace piper {

// Use int for PhonemeType to avoid including piper.hpp
// Values must match piper.hpp PhonemeType enum:
// TextPhonemes = 0, OpenJTalkPhonemes = 1
typedef int PhonemeTypeInt;
const PhonemeTypeInt PHONEME_TYPE_TEXT = 0;
const PhonemeTypeInt PHONEME_TYPE_OPENJTALK = 1;

typedef char32_t Phoneme;

// Structure to hold either text or phonemes
struct TextOrPhonemes {
    bool isPhonemes;
    std::string text;
    std::vector<Phoneme> phonemes;
};

// Parse text containing [[ phonemes ]] notation
// Returns a vector of TextOrPhonemes segments
std::vector<TextOrPhonemes> parsePhonemeNotation(const std::string& input);

// Convert phoneme string to vector of Phoneme objects
// Handles both single-character and multi-character phonemes
std::vector<Phoneme> parsePhonemeString(const std::string& phonemeStr, PhonemeTypeInt phonemeType);

} // namespace piper

#endif // PHONEME_PARSER_H_