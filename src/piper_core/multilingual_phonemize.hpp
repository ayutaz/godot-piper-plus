#ifndef PIPER_MULTILINGUAL_PHONEMIZE_HPP
#define PIPER_MULTILINGUAL_PHONEMIZE_HPP

#include <string>
#include <optional>
#include <vector>

#include "language_detector.hpp"
#include "piper.hpp"

namespace piper {

enum class MultilingualTextRoutingMode {
	Unsupported = 0,
	ExplicitOnly = 1,
	AutoDetect = 2,
};

struct MultilingualLanguageCapability {
	std::string languageCode;
	std::optional<LanguageId> languageId;
	MultilingualTextRoutingMode routingMode = MultilingualTextRoutingMode::Unsupported;
	bool autoRouteAllowed = false;
	bool textPhonemizerAvailable = false;
	bool phonemeOnly = true;
};

struct MultilingualRoutingPlan {
	std::vector<LangSegment> segments;
	std::optional<LanguageId> resolvedLanguageId;
	std::string resolvedLanguageCode;
	bool hasExplicitLanguageSelection = false;
};

MultilingualTextRoutingMode getMultilingualTextRoutingMode(
		const std::string &languageCode);
bool supportsMultilingualTextPhonemization(const std::string &languageCode);
bool supportsMultilingualAutoRouting(const std::string &languageCode);
bool isMultilingualLatinLanguage(const std::string &languageCode);
std::string getMultilingualTextSupportError(const std::string &languageCode);
std::vector<MultilingualLanguageCapability> getMultilingualLanguageCapabilities(
		const Voice &voice);
std::vector<std::string> getMultilingualAutoRouteLanguages(const Voice &voice);
std::vector<std::string> getMultilingualTextLanguages(const Voice &voice);
std::string getMultilingualDefaultLatinLanguage(
		const Voice &voice, const std::vector<std::string> &languages);
MultilingualRoutingPlan planMultilingualTextRouting(
		const Voice &voice,
		const std::string &text,
		const std::optional<std::string> &explicitLanguageCode = std::nullopt,
		const std::optional<LanguageId> &explicitLanguageId = std::nullopt);

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
