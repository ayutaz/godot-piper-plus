#ifndef ENGLISH_PHONEMIZE_HPP
#define ENGLISH_PHONEMIZE_HPP

#include <string>
#include <unordered_map>
#include <vector>

#include "phoneme_parser.hpp"

namespace piper {

bool loadCmuDict(const std::string &jsonPath,
                 std::unordered_map<std::string, std::string> &dict);

bool loadCmuDictFromJsonString(const std::string &jsonText,
                               std::unordered_map<std::string, std::string> &dict);

void phonemize_english(const std::string &text,
                       std::vector<std::vector<Phoneme>> &phonemes,
                       const std::unordered_map<std::string, std::string> &cmuDict);

} // namespace piper

#endif // ENGLISH_PHONEMIZE_HPP
