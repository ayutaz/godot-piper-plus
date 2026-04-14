#ifndef CHINESE_PHONEMIZE_HPP
#define CHINESE_PHONEMIZE_HPP

#include <string>
#include <unordered_map>
#include <vector>

#include "phoneme_parser.hpp"

namespace piper {

bool loadPinyinDicts(const std::string &singleCharPath,
		const std::string &phrasePath,
		std::unordered_map<int, std::string> &singleCharDict,
		std::unordered_map<std::string, std::string> &phraseDict);

bool loadPinyinDictsFromJsonStrings(const std::string &singleCharJson,
		const std::string &phraseJson,
		std::unordered_map<int, std::string> &singleCharDict,
		std::unordered_map<std::string, std::string> &phraseDict);

void phonemize_chinese(const std::string &text,
		std::vector<std::vector<Phoneme>> &phonemes,
		const std::unordered_map<int, std::string> &singleCharDict,
		const std::unordered_map<std::string, std::string> &phraseDict);

} // namespace piper

#endif // CHINESE_PHONEMIZE_HPP
