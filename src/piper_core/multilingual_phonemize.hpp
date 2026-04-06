#ifndef PIPER_MULTILINGUAL_PHONEMIZE_HPP
#define PIPER_MULTILINGUAL_PHONEMIZE_HPP

#include <string>
#include <vector>

#include "language_detector.hpp"
#include "piper.hpp"

namespace piper {

enum class MultilingualTextRoutingMode {
	Unsupported = 0,
	ExplicitOnly = 1,
	AutoDetect = 2,
};

MultilingualTextRoutingMode getMultilingualTextRoutingMode(
		const std::string &languageCode);
bool supportsMultilingualTextPhonemization(const std::string &languageCode);
bool supportsMultilingualAutoRouting(const std::string &languageCode);
bool isMultilingualLatinLanguage(const std::string &languageCode);
std::string getMultilingualTextSupportError(const std::string &languageCode);

void phonemize_spanish(const std::string &text,
		std::vector<std::vector<Phoneme>> &phonemes);
void phonemize_french(const std::string &text,
		std::vector<std::vector<Phoneme>> &phonemes);
void phonemize_portuguese(const std::string &text,
		std::vector<std::vector<Phoneme>> &phonemes);

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
