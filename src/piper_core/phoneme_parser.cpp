#include "phoneme_parser.hpp"
#include <sstream>
#include <algorithm>
#include <map>
#include "spdlog/spdlog.h"
#include "utf8.h"

namespace piper {

// Japanese multi-character phoneme mappings (PUA)
// Must match Python token_mapper.py FIXED_PUA_MAPPING
static const std::map<std::string, char32_t> japanesePhonemePUA = {
    // Long vowels
    {"a:", 0xE000},
    {"i:", 0xE001},
    {"u:", 0xE002},
    {"e:", 0xE003},
    {"o:", 0xE004},
    // Special consonants
    {"cl", 0xE005},
    {"q", 0xE005},
    // Palatalized consonants
    {"ky", 0xE006},
    {"kw", 0xE007},
    {"gy", 0xE008},
    {"gw", 0xE009},
    {"ty", 0xE00A},
    {"dy", 0xE00B},
    {"py", 0xE00C},
    {"by", 0xE00D},
    // Affricates and special sounds
    {"ch", 0xE00E},
    {"ts", 0xE00F},
    {"sh", 0xE010},
    {"zy", 0xE011},
    {"hy", 0xE012},
    // Palatalized nasals/liquids
    {"ny", 0xE013},
    {"my", 0xE014},
    {"ry", 0xE015},
    // Question type markers (Issue #204)
    {"?!", 0xE016},  // Emphatic question - 強調疑問
    {"?.", 0xE017},  // Neutral/rhetorical question - 平叙疑問
    {"?~", 0xE018},  // Tag question - 確認疑問
    // N phoneme variants (Issue #207)
    {"N_m", 0xE019},      // ん before m/b/p (bilabial)
    {"N_n", 0xE01A},      // ん before n/t/d/ts/ch (alveolar)
    {"N_ng", 0xE01B},     // ん before k/g (velar)
    {"N_uvular", 0xE01C}, // ん at end or before vowels
};

namespace {

void appendUtf8Token(const std::string& token, std::vector<Phoneme>& phonemes) {
    std::string sanitized;
    utf8::replace_invalid(token.begin(), token.end(), std::back_inserter(sanitized));

    auto it = sanitized.begin();
    while (it != sanitized.end()) {
        phonemes.push_back(utf8::next(it, sanitized.end()));
    }
}

} // namespace

std::vector<TextOrPhonemes> parsePhonemeNotation(const std::string& input) {
    std::vector<TextOrPhonemes> result;
    std::regex phonemeRegex(R"(\[\[\s*([^\]]*)\s*\]\])");
    
    size_t lastPos = 0;
    auto begin = std::sregex_iterator(input.begin(), input.end(), phonemeRegex);
    auto end = std::sregex_iterator();
    
    for (std::sregex_iterator i = begin; i != end; ++i) {
        std::smatch match = *i;
        
        // Add text before the phoneme notation
        if (static_cast<size_t>(match.position()) > lastPos) {
            TextOrPhonemes textSegment;
            textSegment.isPhonemes = false;
            textSegment.text = input.substr(lastPos, match.position() - lastPos);
            result.push_back(textSegment);
        }
        
        // Add the phonemes
        TextOrPhonemes phonemeSegment;
        phonemeSegment.isPhonemes = true;
        std::string phonemeStr = match[1].str();
        // Trim trailing whitespace
        phonemeStr.erase(phonemeStr.find_last_not_of(" \t\n\r") + 1);
        phonemeSegment.text = phonemeStr; // Store trimmed phoneme string
        // Phonemes will be parsed later based on the phoneme type
        result.push_back(phonemeSegment);
        
        lastPos = match.position() + match.length();
    }
    
    // Add any remaining text
    if (lastPos < input.length()) {
        TextOrPhonemes textSegment;
        textSegment.isPhonemes = false;
        textSegment.text = input.substr(lastPos);
        result.push_back(textSegment);
    }
    
    return result;
}

std::vector<Phoneme> parsePhonemeString(const std::string& phonemeStr, PhonemeTypeInt phonemeType) {
    std::vector<Phoneme> phonemes;
    std::istringstream iss(phonemeStr);
    std::string token;
    
    // Split by whitespace
    while (iss >> token) {
        if (token.empty()) continue;
        
        if (phonemeType == PHONEME_TYPE_OPENJTALK ||
            phonemeType == PHONEME_TYPE_MULTILINGUAL) {
            // Multilingual models reuse the Japanese PUA tokens for JA segments.
            auto it = japanesePhonemePUA.find(token);
            if (it != japanesePhonemePUA.end()) {
                // Use the PUA codepoint directly
                phonemes.push_back(it->second);
            } else {
                appendUtf8Token(token, phonemes);
            }
        } else {
            // For English/text phonemes, keep UTF-8 tokens as-is.
            if (token == "pau" || token == "_") {
                // Pause marker
                phonemes.push_back(static_cast<Phoneme>('_'));
            } else {
                appendUtf8Token(token, phonemes);
            }
        }
    }
    
    spdlog::debug("Parsed {} phonemes from string: {}", phonemes.size(), phonemeStr);
    return phonemes;
}

} // namespace piper
