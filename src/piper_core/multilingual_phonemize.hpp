#ifndef PIPER_MULTILINGUAL_PHONEMIZE_HPP
#define PIPER_MULTILINGUAL_PHONEMIZE_HPP

#include <string>
#include <vector>

#include "language_detector.hpp"

namespace piper {

std::vector<LangSegment> segmentMultilingualText(
		const std::string &utf8Text,
		const std::vector<std::string> &languages = std::vector<std::string>{"ja", "en"},
		const std::string &defaultLatinLang = "en");

std::string detectMultilingualDominantLanguage(
		const std::string &utf8Text,
		const std::vector<std::string> &languages = std::vector<std::string>{"ja", "en"},
		const std::string &defaultLatinLang = "en");

} // namespace piper

#endif // PIPER_MULTILINGUAL_PHONEMIZE_HPP
