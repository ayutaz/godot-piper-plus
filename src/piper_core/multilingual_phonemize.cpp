#include "multilingual_phonemize.hpp"

namespace piper {

std::vector<LangSegment> segmentMultilingualText(
		const std::string &utf8Text,
		const std::vector<std::string> &languages,
		const std::string &defaultLatinLang) {
	UnicodeLanguageDetector detector(languages, defaultLatinLang);
	return detector.segmentText(utf8Text);
}

std::string detectMultilingualDominantLanguage(
		const std::string &utf8Text,
		const std::vector<std::string> &languages,
		const std::string &defaultLatinLang) {
	UnicodeLanguageDetector detector(languages, defaultLatinLang);
	return detectDominantLanguage(utf8Text, detector);
}

} // namespace piper
