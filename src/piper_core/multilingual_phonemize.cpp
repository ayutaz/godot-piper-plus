#include "multilingual_phonemize.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <utility>

#include "utf8.h"
#include "utf8_utils.hpp"

namespace piper {
namespace {

using utf8_util::toCodepoints;

struct LatinToken {
	std::u32string text;
	bool isWord = false;
};

bool is_whitespace(char32_t cp) {
	return cp == U' ' || cp == U'\t' || cp == U'\n' || cp == U'\r';
}

bool is_token_punctuation(char32_t cp) {
	switch (cp) {
		case U',':
		case U'.':
		case U';':
		case U':':
		case U'!':
		case U'?':
		case U'-':
		case U'\'':
		case U'(':
		case U')':
		case U'\u00A1':
		case U'\u00BF':
		case U'\u00AB':
		case U'\u00BB':
			return true;
		default:
			return false;
	}
}

bool is_opening_punctuation(char32_t cp) {
	return cp == U'(' || cp == U'\u00A1' || cp == U'\u00BF' ||
			   cp == U'\u00AB';
}

bool is_latin_letter(char32_t cp) {
	return (cp >= U'A' && cp <= U'Z') ||
			   (cp >= U'a' && cp <= U'z') ||
			   (cp >= 0x00C0 && cp <= 0x00FF) ||
			   cp == U'\u0107' || cp == U'\u010D' ||
			   cp == U'\u0119' || cp == U'\u011A' ||
			   cp == U'\u0129' || cp == U'\u00F1' ||
			   cp == U'\u0153' || cp == U'\u00D8' ||
			   cp == U'\u00F8' || cp == U'\u1EBD';
}

char32_t to_lower_latin(char32_t cp) {
	if (cp >= U'A' && cp <= U'Z') {
		return cp + 32;
	}

	switch (cp) {
		case U'\u00C0':
		case U'\u00C1':
		case U'\u00C2':
		case U'\u00C3':
		case U'\u00C4':
			return U'\u00E0' + (cp - U'\u00C0');
		case U'\u00C7':
			return U'\u00E7';
		case U'\u00C8':
		case U'\u00C9':
		case U'\u00CA':
		case U'\u00CB':
			return U'\u00E8' + (cp - U'\u00C8');
		case U'\u00CC':
		case U'\u00CD':
		case U'\u00CE':
		case U'\u00CF':
			return U'\u00EC' + (cp - U'\u00CC');
		case U'\u00D1':
			return U'\u00F1';
		case U'\u00D2':
		case U'\u00D3':
		case U'\u00D4':
		case U'\u00D5':
		case U'\u00D6':
			return U'\u00F2' + (cp - U'\u00D2');
		case U'\u00D9':
		case U'\u00DA':
		case U'\u00DB':
		case U'\u00DC':
			return U'\u00F9' + (cp - U'\u00D9');
		case U'\u0178':
			return U'\u00FF';
		default:
			return cp;
	}
}

std::u32string to_lower_word(const std::u32string &word) {
	std::u32string lowered;
	lowered.reserve(word.size());
	for (char32_t cp : word) {
		lowered.push_back(to_lower_latin(cp));
	}
	return lowered;
}

std::vector<LatinToken> tokenize_latin_text(const std::string &text) {
	std::vector<LatinToken> tokens;
	if (!utf8::is_valid(text.begin(), text.end())) {
		return tokens;
	}

	auto cps = toCodepoints(text);
	for (std::size_t index = 0; index < cps.size();) {
		char32_t cp = cps[index];
		if (is_whitespace(cp)) {
			++index;
			continue;
		}

		if (is_latin_letter(cp)) {
			LatinToken token;
			token.isWord = true;
			while (index < cps.size() && is_latin_letter(cps[index])) {
				token.text.push_back(to_lower_latin(cps[index]));
				++index;
			}
			tokens.push_back(std::move(token));
			continue;
		}

		if (is_token_punctuation(cp)) {
			tokens.push_back({std::u32string(1, cp), false});
		}
		++index;
	}

	return tokens;
}

bool is_vowel(char32_t cp) {
	switch (cp) {
		case U'a':
		case U'e':
		case U'i':
		case U'o':
		case U'u':
		case U'y':
		case U'\u00E0':
		case U'\u00E1':
		case U'\u00E2':
		case U'\u00E3':
		case U'\u00E8':
		case U'\u00E9':
		case U'\u00EA':
		case U'\u00EC':
		case U'\u00ED':
		case U'\u00EE':
		case U'\u00F2':
		case U'\u00F3':
		case U'\u00F4':
		case U'\u00F5':
		case U'\u00F9':
		case U'\u00FA':
		case U'\u00FB':
		case U'\u00FC':
		case U'\u00E6':
		case U'\u00F8':
		case U'\u0153':
		case U'\u0129':
		case U'\u1EBD':
			return true;
		default:
			return false;
	}
}

bool is_front_vowel(char32_t cp) {
	switch (cp) {
		case U'e':
		case U'i':
		case U'y':
		case U'\u00E9':
		case U'\u00EA':
		case U'\u00E8':
		case U'\u00ED':
		case U'\u00EE':
		case U'\u00EF':
			return true;
		default:
			return false;
	}
}

bool is_word_end_or_consonant(
		const std::u32string &word, std::size_t index) {
	return index + 1 >= word.size() || !is_vowel(word[index + 1]);
}

void append_chars(std::vector<char32_t> &out,
		std::initializer_list<char32_t> chars) {
	out.insert(out.end(), chars.begin(), chars.end());
}

std::vector<char32_t> transliterate_spanish_word(const std::u32string &input) {
	std::u32string word = to_lower_word(input);
	std::vector<char32_t> output;

	for (std::size_t index = 0; index < word.size(); ++index) {
		char32_t cp = word[index];
		char32_t next = index + 1 < word.size() ? word[index + 1] : 0;
		char32_t next2 = index + 2 < word.size() ? word[index + 2] : 0;

		if (cp == U'c' && next == U'h') {
			append_chars(output, {U't', U'\u0283'});
			++index;
			continue;
		}
		if (cp == U'l' && next == U'l') {
			output.push_back(U'\u028E');
			++index;
			continue;
		}
		if (cp == U'r' && next == U'r') {
			output.push_back(U'r');
			++index;
			continue;
		}
		if (cp == U'q' && next == U'u') {
			output.push_back(U'k');
			++index;
			continue;
		}
		if (cp == U'g' && next == U'u' && is_front_vowel(next2)) {
			output.push_back(U'g');
			++index;
			continue;
		}

		switch (cp) {
			case U'a':
			case U'\u00E1':
				output.push_back(U'a');
				break;
			case U'e':
			case U'\u00E9':
				output.push_back(U'e');
				break;
			case U'i':
			case U'\u00ED':
				output.push_back(U'i');
				break;
			case U'o':
			case U'\u00F3':
				output.push_back(U'o');
				break;
			case U'u':
			case U'\u00FA':
			case U'\u00FC':
				output.push_back(U'u');
				break;
			case U'b':
			case U'v':
				output.push_back(U'b');
				break;
			case U'c':
				output.push_back(is_front_vowel(next) ? U's' : U'k');
				break;
			case U'd':
				output.push_back(U'd');
				break;
			case U'f':
				output.push_back(U'f');
				break;
			case U'g':
				output.push_back(is_front_vowel(next) ? U'x' : U'g');
				break;
			case U'h':
				break;
			case U'j':
				output.push_back(U'x');
				break;
			case U'k':
				output.push_back(U'k');
				break;
			case U'l':
				output.push_back(U'l');
				break;
			case U'm':
				output.push_back(U'm');
				break;
			case U'n':
				output.push_back(U'n');
				break;
			case U'\u00F1':
				output.push_back(U'\u0272');
				break;
			case U'p':
				output.push_back(U'p');
				break;
			case U'r':
				output.push_back(index == 0 ? U'r' : U'\u027E');
				break;
			case U's':
				output.push_back(U's');
				break;
			case U't':
				output.push_back(U't');
				break;
			case U'w':
				output.push_back(U'w');
				break;
			case U'x':
				append_chars(output, {U'k', U's'});
				break;
			case U'y':
				output.push_back(index + 1 == word.size() ? U'i' : U'j');
				break;
			case U'z':
				output.push_back(U's');
				break;
			default:
				break;
		}
	}

	return output;
}

std::vector<char32_t> transliterate_french_word(const std::u32string &input) {
	std::u32string word = to_lower_word(input);
	std::vector<char32_t> output;

	for (std::size_t index = 0; index < word.size(); ++index) {
		char32_t cp = word[index];
		char32_t next = index + 1 < word.size() ? word[index + 1] : 0;
		char32_t next2 = index + 2 < word.size() ? word[index + 2] : 0;
		bool last = index + 1 == word.size();

		if (cp == U'c' && next == U'h') {
			output.push_back(U'\u0283');
			++index;
			continue;
		}
		if (cp == U'g' && next == U'n') {
			output.push_back(U'\u0272');
			++index;
			continue;
		}
		if (cp == U'o' && next == U'u') {
			output.push_back(U'u');
			++index;
			continue;
		}
		if (cp == U'o' && next == U'n' &&
				is_word_end_or_consonant(word, index + 1)) {
			output.push_back(U'\u00F5');
			++index;
			continue;
		}
		if ((cp == U'a' || cp == U'e') &&
				(next == U'n' || next == U'm') &&
				is_word_end_or_consonant(word, index + 1)) {
			output.push_back(U'\u00E3');
			++index;
			continue;
		}
		if (cp == U'a' && next == U'u') {
			output.push_back(U'o');
			++index;
			continue;
		}
		if (cp == U'e' && next == U'u') {
			output.push_back(U'\u00F8');
			++index;
			continue;
		}
		if (cp == U'q' && next == U'u') {
			output.push_back(U'k');
			++index;
			continue;
		}
		if (cp == U'o' && next == U'i') {
			append_chars(output, {U'w', U'a'});
			++index;
			continue;
		}

		switch (cp) {
			case U'a':
			case U'\u00E0':
			case U'\u00E2':
				output.push_back(U'a');
				break;
			case U'b':
				if (!last) {
					output.push_back(U'b');
				}
				break;
			case U'c':
				output.push_back(is_front_vowel(next) ? U's' : U'k');
				break;
			case U'd':
				if (!last) {
					output.push_back(U'd');
				}
				break;
			case U'e':
			case U'\u00E8':
			case U'\u00E9':
			case U'\u00EA':
			case U'\u00EB':
				if (!last) {
					output.push_back(U'e');
				}
				break;
			case U'f':
				output.push_back(U'f');
				break;
			case U'g':
				output.push_back(is_front_vowel(next) ? U'\u0292' : U'g');
				break;
			case U'h':
				break;
			case U'i':
			case U'\u00EE':
			case U'\u00EF':
			case U'y':
				output.push_back(U'i');
				break;
			case U'j':
				output.push_back(U'\u0292');
				break;
			case U'k':
				output.push_back(U'k');
				break;
			case U'l':
				output.push_back(U'l');
				break;
			case U'm':
			case U'n':
				if (!last) {
					output.push_back(cp);
				}
				break;
			case U'o':
			case U'\u00F4':
			case U'\u00F6':
				output.push_back(U'o');
				break;
			case U'p':
				if (!last) {
					output.push_back(U'p');
				}
				break;
			case U'q':
				output.push_back(U'k');
				break;
			case U'r':
				output.push_back(U'\u0281');
				break;
			case U's':
				if (last) {
					break;
				}
				if (!output.empty() && is_vowel(output.back()) && is_vowel(next)) {
					output.push_back(U'z');
				} else {
					output.push_back(U's');
				}
				break;
			case U't':
				if (!last) {
					output.push_back(U't');
				}
				break;
			case U'u':
			case U'\u00F9':
			case U'\u00FB':
			case U'\u00FC':
				output.push_back(U'y');
				break;
			case U'v':
				output.push_back(U'v');
				break;
			case U'x':
				if (!last) {
					append_chars(output, {U'k', U's'});
				}
				break;
			case U'z':
				if (!last) {
					output.push_back(U'z');
				}
				break;
			default:
				break;
		}
	}

	return output;
}

std::vector<char32_t> transliterate_portuguese_word(const std::u32string &input) {
	std::u32string word = to_lower_word(input);
	std::vector<char32_t> output;

	for (std::size_t index = 0; index < word.size(); ++index) {
		char32_t cp = word[index];
		char32_t next = index + 1 < word.size() ? word[index + 1] : 0;
		char32_t next2 = index + 2 < word.size() ? word[index + 2] : 0;

		if (cp == U'l' && next == U'h') {
			output.push_back(U'\u028E');
			++index;
			continue;
		}
		if (cp == U'n' && next == U'h') {
			output.push_back(U'\u0272');
			++index;
			continue;
		}
		if (cp == U'c' && next == U'h') {
			output.push_back(U'\u0283');
			++index;
			continue;
		}
		if (cp == U'r' && next == U'r') {
			output.push_back(U'\u0281');
			++index;
			continue;
		}
		if (cp == U's' && next == U's') {
			output.push_back(U's');
			++index;
			continue;
		}
		if (cp == U'q' && next == U'u') {
			output.push_back(U'k');
			++index;
			continue;
		}
		if (cp == U'g' && next == U'u' && is_front_vowel(next2)) {
			output.push_back(U'g');
			++index;
			continue;
		}

		switch (cp) {
			case U'a':
			case U'\u00E0':
			case U'\u00E1':
			case U'\u00E2':
				output.push_back(U'a');
				break;
			case U'\u00E3':
				output.push_back(U'\u00E3');
				break;
			case U'e':
			case U'\u00E9':
			case U'\u00EA':
				output.push_back(U'e');
				break;
			case U'i':
			case U'\u00ED':
				output.push_back(U'i');
				break;
			case U'o':
			case U'\u00F3':
			case U'\u00F4':
				output.push_back(U'o');
				break;
			case U'\u00F5':
				output.push_back(U'\u00F5');
				break;
			case U'u':
			case U'\u00FA':
				output.push_back(U'u');
				break;
			case U'b':
				output.push_back(U'b');
				break;
			case U'c':
				output.push_back(is_front_vowel(next) ? U's' : U'k');
				break;
			case U'\u00E7':
				output.push_back(U's');
				break;
			case U'd':
				output.push_back(U'd');
				break;
			case U'f':
				output.push_back(U'f');
				break;
			case U'g':
				output.push_back(is_front_vowel(next) ? U'\u0292' : U'g');
				break;
			case U'h':
				break;
			case U'j':
				output.push_back(U'\u0292');
				break;
			case U'k':
				output.push_back(U'k');
				break;
			case U'l':
				output.push_back(U'l');
				break;
			case U'm':
				output.push_back(U'm');
				break;
			case U'n':
				output.push_back(U'n');
				break;
			case U'p':
				output.push_back(U'p');
				break;
			case U'r':
				output.push_back(index == 0 ? U'\u0281' : U'\u027E');
				break;
			case U's':
				output.push_back(U's');
				break;
			case U't':
				output.push_back(U't');
				break;
			case U'v':
				output.push_back(U'v');
				break;
			case U'x':
				output.push_back(U'\u0283');
				break;
			case U'z':
				output.push_back(U'z');
				break;
			default:
				break;
		}
	}

	return output;
}

void phonemize_rule_based_latin(
		const std::string &text,
		std::vector<std::vector<Phoneme>> &phonemes,
		const std::function<std::vector<char32_t>(const std::u32string &)> &wordPhonemizer) {
	phonemes.clear();
	if (!utf8::is_valid(text.begin(), text.end())) {
		return;
	}

	std::vector<LatinToken> tokens = tokenize_latin_text(text);
	if (tokens.empty()) {
		return;
	}

	std::vector<Phoneme> sentence;
	bool needSpaceBeforeWord = false;

	for (const LatinToken &token : tokens) {
		if (token.isWord) {
			std::vector<char32_t> word = wordPhonemizer(token.text);
			if (word.empty()) {
				needSpaceBeforeWord = true;
				continue;
			}

			if (!sentence.empty() && needSpaceBeforeWord) {
				sentence.push_back(static_cast<Phoneme>(U' '));
			}
			for (char32_t cp : word) {
				sentence.push_back(static_cast<Phoneme>(cp));
			}
			needSpaceBeforeWord = true;
			continue;
		}

		for (char32_t cp : token.text) {
			sentence.push_back(static_cast<Phoneme>(cp));
			needSpaceBeforeWord = !is_opening_punctuation(cp);
		}
	}

	if (!sentence.empty()) {
		phonemes.push_back(std::move(sentence));
	}
}

} // namespace

MultilingualTextRoutingMode getMultilingualTextRoutingMode(
		const std::string &languageCode) {
	if (languageCode == "ja" || languageCode == "en") {
		return MultilingualTextRoutingMode::AutoDetect;
	}
	if (languageCode == "es" || languageCode == "fr" || languageCode == "pt") {
		return MultilingualTextRoutingMode::ExplicitOnly;
	}
	return MultilingualTextRoutingMode::Unsupported;
}

bool supportsMultilingualTextPhonemization(const std::string &languageCode) {
	return getMultilingualTextRoutingMode(languageCode) !=
			MultilingualTextRoutingMode::Unsupported;
}

bool supportsMultilingualAutoRouting(const std::string &languageCode) {
	return getMultilingualTextRoutingMode(languageCode) ==
			MultilingualTextRoutingMode::AutoDetect;
}

bool isMultilingualLatinLanguage(const std::string &languageCode) {
	return languageCode == "en" || languageCode == "es" ||
			languageCode == "fr" || languageCode == "pt";
}

std::string getMultilingualTextSupportError(const std::string &languageCode) {
	if (languageCode.empty()) {
		return "Multilingual text phonemization could not resolve a supported language for this segment. "
			   "Use language_code to select one of: ja, en, es, fr, pt.";
	}

	if (languageCode == "zh") {
		return "Multilingual text phonemization for language_code 'zh' is not implemented. "
			   "Use raw phoneme input or select a supported text language: ja, en, es, fr, pt.";
	}

	return "Multilingual text phonemization for language_code '" + languageCode +
		   "' is not implemented. Use raw phoneme input or a supported text language.";
}

std::vector<MultilingualLanguageCapability> getMultilingualLanguageCapabilities(
		const Voice &voice) {
	std::vector<MultilingualLanguageCapability> capabilities;

	auto append_capability = [&capabilities](const std::string &code,
			std::optional<LanguageId> languageId) {
		MultilingualLanguageCapability capability;
		capability.languageCode = code;
		capability.languageId = languageId;
		capability.routingMode = getMultilingualTextRoutingMode(code);
		capability.autoRouteAllowed =
				capability.routingMode == MultilingualTextRoutingMode::AutoDetect;
		capability.textPhonemizerAvailable =
				capability.routingMode != MultilingualTextRoutingMode::Unsupported;
		capability.phonemeOnly = !capability.textPhonemizerAvailable;
		capabilities.push_back(std::move(capability));
	};

	if (voice.modelConfig.languageIdMap && !voice.modelConfig.languageIdMap->empty()) {
		std::vector<std::pair<LanguageId, std::string>> sortedLanguages;
		sortedLanguages.reserve(voice.modelConfig.languageIdMap->size());
		for (const auto &[code, id] : *voice.modelConfig.languageIdMap) {
			sortedLanguages.emplace_back(id, code);
		}
		std::sort(sortedLanguages.begin(), sortedLanguages.end(),
				[](const auto &lhs, const auto &rhs) {
					return lhs.first < rhs.first;
				});

		for (const auto &[id, code] : sortedLanguages) {
			append_capability(code, id);
		}
		return capabilities;
	}

	append_capability("ja", std::nullopt);
	append_capability("en", std::nullopt);
	return capabilities;
}

std::vector<std::string> getMultilingualAutoRouteLanguages(const Voice &voice) {
	std::vector<std::string> languages;
	for (const auto &capability : getMultilingualLanguageCapabilities(voice)) {
		if (capability.autoRouteAllowed) {
			languages.push_back(capability.languageCode);
		}
	}

	if (languages.empty()) {
		languages = {"ja", "en"};
	}

	return languages;
}

std::vector<std::string> getMultilingualTextLanguages(const Voice &voice) {
	std::vector<std::string> languages;
	for (const auto &capability : getMultilingualLanguageCapabilities(voice)) {
		if (capability.textPhonemizerAvailable) {
			languages.push_back(capability.languageCode);
		}
	}

	if (languages.empty()) {
		languages = {"ja", "en"};
	}

	return languages;
}

std::string getMultilingualDefaultLatinLanguage(
		const Voice &voice, const std::vector<std::string> &languages) {
	std::string selectedLanguage;
	if (voice.synthesisConfig.languageId.has_value() &&
			voice.modelConfig.languageIdMap) {
		for (const auto &[code, id] : *voice.modelConfig.languageIdMap) {
			if (id == *voice.synthesisConfig.languageId) {
				selectedLanguage = code;
				break;
			}
		}
	}
	if (isMultilingualLatinLanguage(selectedLanguage) &&
			supportsMultilingualTextPhonemization(selectedLanguage) &&
			std::find(languages.begin(), languages.end(), selectedLanguage) != languages.end()) {
		return selectedLanguage;
	}

	for (const char *preferredLanguage : {"en", "es", "fr", "pt"}) {
		if (std::find(languages.begin(), languages.end(), preferredLanguage) != languages.end() &&
				supportsMultilingualTextPhonemization(preferredLanguage)) {
			return preferredLanguage;
		}
	}

	for (const std::string &language : languages) {
		if (isMultilingualLatinLanguage(language) &&
				supportsMultilingualTextPhonemization(language)) {
			return language;
		}
	}

	return {};
}

std::string normalizeMultilingualLanguageCode(const std::string &languageCode) {
	std::string normalized = languageCode;
	auto is_not_space = [](unsigned char ch) { return !std::isspace(ch); };
	normalized.erase(normalized.begin(),
			std::find_if(normalized.begin(), normalized.end(), is_not_space));
	normalized.erase(std::find_if(normalized.rbegin(), normalized.rend(), is_not_space).base(),
			normalized.end());
	std::transform(normalized.begin(), normalized.end(), normalized.begin(),
			[](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
	std::replace(normalized.begin(), normalized.end(), '_', '-');
	return normalized;
}

std::optional<LanguageId> resolveLanguageIdForCode(
		const Voice &voice, const std::string &languageCode) {
	if (!voice.modelConfig.languageIdMap) {
		return std::nullopt;
	}

	const std::string normalizedCode = normalizeMultilingualLanguageCode(languageCode);
	for (const auto &[code, id] : *voice.modelConfig.languageIdMap) {
		if (normalizeMultilingualLanguageCode(code) == normalizedCode) {
			return id;
		}
	}

	return std::nullopt;
}

MultilingualRoutingPlan planMultilingualTextRouting(
		const Voice &voice,
		const std::string &text,
		const std::optional<std::string> &explicitLanguageCode,
		const std::optional<LanguageId> &explicitLanguageId) {
	MultilingualRoutingPlan plan;
	plan.hasExplicitLanguageSelection =
			(explicitLanguageCode.has_value() && !explicitLanguageCode->empty()) ||
			explicitLanguageId.has_value();

	std::string resolvedExplicitLanguageCode;
	if (explicitLanguageCode.has_value() && !explicitLanguageCode->empty()) {
		resolvedExplicitLanguageCode = normalizeMultilingualLanguageCode(*explicitLanguageCode);
	} else if (explicitLanguageId.has_value() && voice.modelConfig.languageIdMap) {
		for (const auto &[code, id] : *voice.modelConfig.languageIdMap) {
			if (id == *explicitLanguageId) {
				resolvedExplicitLanguageCode = normalizeMultilingualLanguageCode(code);
				break;
			}
		}
	}

	std::vector<std::string> languages = getMultilingualAutoRouteLanguages(voice);
	if (!resolvedExplicitLanguageCode.empty() &&
			std::find(languages.begin(), languages.end(), resolvedExplicitLanguageCode) ==
					languages.end()) {
		languages.push_back(resolvedExplicitLanguageCode);
	}

	const std::string defaultLatin = getMultilingualDefaultLatinLanguage(voice, languages);
	plan.segments = segmentMultilingualText(text, languages, defaultLatin);

	if (!resolvedExplicitLanguageCode.empty()) {
		plan.resolvedLanguageCode = resolvedExplicitLanguageCode;
	} else if (!text.empty()) {
		plan.resolvedLanguageCode =
				detectMultilingualDominantLanguage(text, languages, defaultLatin);
	}

	plan.resolvedLanguageId = resolveLanguageIdForCode(voice, plan.resolvedLanguageCode);
	return plan;
}

bool containsHanWithoutKana(const std::string &utf8Text) {
	if (!utf8::is_valid(utf8Text.begin(), utf8Text.end())) {
		return false;
	}

	bool hasKana = false;
	bool hasHan = false;
	auto it = utf8Text.begin();
	while (it != utf8Text.end()) {
		uint32_t cp = utf8::unchecked::next(it);
		if ((cp >= 0x3040 && cp <= 0x309F) ||
				(cp >= 0x30A0 && cp <= 0x30FF) ||
				(cp >= 0x31F0 && cp <= 0x31FF) ||
				(cp >= 0xFF65 && cp <= 0xFF9F)) {
			hasKana = true;
		}
		if ((cp >= 0x4E00 && cp <= 0x9FFF) ||
				(cp >= 0x3400 && cp <= 0x4DBF) ||
				(cp >= 0xF900 && cp <= 0xFAFF)) {
			hasHan = true;
		}
	}

	return hasHan && !hasKana;
}

void phonemize_spanish(const std::string &text,
		std::vector<std::vector<Phoneme>> &phonemes) {
	phonemize_rule_based_latin(text, phonemes, transliterate_spanish_word);
}

void phonemize_french(const std::string &text,
		std::vector<std::vector<Phoneme>> &phonemes) {
	phonemize_rule_based_latin(text, phonemes, transliterate_french_word);
}

void phonemize_portuguese(const std::string &text,
		std::vector<std::vector<Phoneme>> &phonemes) {
	phonemize_rule_based_latin(text, phonemes, transliterate_portuguese_word);
}

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
