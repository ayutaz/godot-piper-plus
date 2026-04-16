// Chinese (Mandarin) phonemizer for Piper TTS — C++ port of chinese.py.
//
// Converts Chinese text to IPA phonemes via a pinyin intermediate representation.
// Uses pypinyin-format JSON dictionaries for character-to-pinyin conversion,
// then applies normalization, tone sandhi, and IPA mapping.

#include "chinese_phonemize.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <istream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "json.hpp"
#include "utf8.h"
#include "utf8_utils.hpp"

using json = nlohmann::json;

namespace piper {
namespace {

constexpr Phoneme PUA_PH = 0xE020;
constexpr Phoneme PUA_TH = 0xE021;
constexpr Phoneme PUA_KH = 0xE022;
constexpr Phoneme PUA_TC = 0xE023;
constexpr Phoneme PUA_TCH = 0xE024;
constexpr Phoneme PUA_TRS = 0xE025;
constexpr Phoneme PUA_TRSH = 0xE026;
constexpr Phoneme PUA_TSH = 0xE027;

constexpr Phoneme PUA_AI = 0xE028;
constexpr Phoneme PUA_EI = 0xE029;
constexpr Phoneme PUA_AU = 0xE02A;
constexpr Phoneme PUA_OU = 0xE02B;

constexpr Phoneme PUA_AN = 0xE02C;
constexpr Phoneme PUA_EN = 0xE02D;
constexpr Phoneme PUA_ANG = 0xE02E;
constexpr Phoneme PUA_ENG = 0xE02F;
constexpr Phoneme PUA_UNG = 0xE030;

constexpr Phoneme PUA_IA = 0xE031;
constexpr Phoneme PUA_IE = 0xE032;
constexpr Phoneme PUA_IOU = 0xE033;
constexpr Phoneme PUA_IAU = 0xE034;
constexpr Phoneme PUA_IEN = 0xE035;
constexpr Phoneme PUA_IN = 0xE036;
constexpr Phoneme PUA_IANG = 0xE037;
constexpr Phoneme PUA_ING = 0xE038;
constexpr Phoneme PUA_IUNG = 0xE039;

constexpr Phoneme PUA_UA = 0xE03A;
constexpr Phoneme PUA_UO = 0xE03B;
constexpr Phoneme PUA_UAI = 0xE03C;
constexpr Phoneme PUA_UEI = 0xE03D;
constexpr Phoneme PUA_UAN = 0xE03E;
constexpr Phoneme PUA_UEN = 0xE03F;
constexpr Phoneme PUA_UANG = 0xE040;
constexpr Phoneme PUA_UENG = 0xE041;

constexpr Phoneme PUA_YE = 0xE042;
constexpr Phoneme PUA_YEN = 0xE043;
constexpr Phoneme PUA_YN = 0xE044;

constexpr Phoneme PUA_RETRO = 0xE045;

constexpr Phoneme PUA_TONE1 = 0xE046;
constexpr Phoneme PUA_TONE2 = 0xE047;
constexpr Phoneme PUA_TONE3 = 0xE048;
constexpr Phoneme PUA_TONE4 = 0xE049;
constexpr Phoneme PUA_TONE5 = 0xE04A;

constexpr Phoneme IPA_ALVPAL_FRIC = 0x0255;
constexpr Phoneme IPA_RETRO_FRIC = 0x0282;
constexpr Phoneme IPA_RETRO_APPR = 0x027B;
constexpr Phoneme IPA_RSCHWA = 0x025A;
constexpr Phoneme IPA_CLOSE_BACK = 0x0264;
constexpr Phoneme IPA_BARRED_I = 0x0268;
constexpr Phoneme PUA_Y_VOWEL = 0xE01E;

Phoneme mapZhPunct(char32_t cp) {
	switch (cp) {
		case 0x3002:
			return '.';
		case 0xFF0C:
		case 0x3001:
			return ',';
		case 0xFF01:
			return '!';
		case 0xFF1F:
			return '?';
		case 0xFF1B:
			return ';';
		case 0xFF1A:
			return ':';
		case 0x2026:
			return '.';
		case 0x2014:
			return ',';
		case 0x201C:
		case 0x201D:
			return '"';
		case 0x2018:
		case 0x2019:
			return '\'';
		default:
			return 0;
	}
}

bool isZhPunctuation(char32_t cp) {
	return cp == ',' || cp == '.' || cp == ';' || cp == ':' || cp == '!' ||
			cp == '?' || cp == 0x3002 || cp == 0xFF0C || cp == 0xFF01 ||
			cp == 0xFF1F || cp == 0x3001 || cp == 0xFF1B || cp == 0xFF1A ||
			cp == 0x201C || cp == 0x201D || cp == 0x2018 || cp == 0x2019 ||
			cp == 0x2026 || cp == 0x2014;
}

bool isCJK(char32_t cp) {
	return (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF);
}

using utf8_util::cpToUtf8;
using utf8_util::cpsToUtf8;
using utf8_util::toCodepoints;

struct InitialEntry {
	const char *pinyin;
	int len;
};

const InitialEntry INITIALS_ORDER[] = {
		{"zh", 2}, {"ch", 2}, {"sh", 2}, {"b", 1}, {"p", 1}, {"m", 1},
		{"f", 1}, {"d", 1}, {"t", 1}, {"n", 1}, {"l", 1}, {"g", 1},
		{"k", 1}, {"h", 1}, {"j", 1}, {"q", 1}, {"x", 1}, {"r", 1},
		{"z", 1}, {"c", 1}, {"s", 1},
};

const int NUM_INITIALS = sizeof(INITIALS_ORDER) / sizeof(INITIALS_ORDER[0]);

const std::unordered_set<std::string> RETROFLEX_INITIALS = {
		"zh",
		"ch",
		"sh",
		"r",
};

const std::unordered_set<std::string> ALVEOLAR_INITIALS = {
		"z",
		"c",
		"s",
};

Phoneme initialToIPA(const std::string &init) {
	if (init == "b") {
		return 'p';
	}
	if (init == "p") {
		return PUA_PH;
	}
	if (init == "m") {
		return 'm';
	}
	if (init == "f") {
		return 'f';
	}
	if (init == "d") {
		return 't';
	}
	if (init == "t") {
		return PUA_TH;
	}
	if (init == "n") {
		return 'n';
	}
	if (init == "l") {
		return 'l';
	}
	if (init == "g") {
		return 'k';
	}
	if (init == "k") {
		return PUA_KH;
	}
	if (init == "h") {
		return 'x';
	}
	if (init == "j") {
		return PUA_TC;
	}
	if (init == "q") {
		return PUA_TCH;
	}
	if (init == "x") {
		return IPA_ALVPAL_FRIC;
	}
	if (init == "zh") {
		return PUA_TRS;
	}
	if (init == "ch") {
		return PUA_TRSH;
	}
	if (init == "sh") {
		return IPA_RETRO_FRIC;
	}
	if (init == "r") {
		return IPA_RETRO_APPR;
	}
	if (init == "z") {
		return 0xE00F;
	}
	if (init == "c") {
		return PUA_TSH;
	}
	if (init == "s") {
		return 's';
	}
	return 0;
}

const std::string KEY_RETRO = "-i_retroflex";
const std::string KEY_ALVE = "-i_alveolar";

Phoneme finalToIPA(const std::string &fin) {
	if (fin == "a") {
		return 'a';
	}
	if (fin == "o") {
		return 'o';
	}
	if (fin == "e") {
		return IPA_CLOSE_BACK;
	}
	if (fin == "i") {
		return 'i';
	}
	if (fin == "u") {
		return 'u';
	}
	if (fin == "\xC3\xBC" || fin == "v") {
		return PUA_Y_VOWEL;
	}
	if (fin == "ai") {
		return PUA_AI;
	}
	if (fin == "ei") {
		return PUA_EI;
	}
	if (fin == "ao") {
		return PUA_AU;
	}
	if (fin == "ou") {
		return PUA_OU;
	}
	if (fin == "an") {
		return PUA_AN;
	}
	if (fin == "en") {
		return PUA_EN;
	}
	if (fin == "ang") {
		return PUA_ANG;
	}
	if (fin == "eng") {
		return PUA_ENG;
	}
	if (fin == "ong") {
		return PUA_UNG;
	}
	if (fin == "er") {
		return IPA_RSCHWA;
	}
	if (fin == "ia") {
		return PUA_IA;
	}
	if (fin == "ie") {
		return PUA_IE;
	}
	if (fin == "iao") {
		return PUA_IAU;
	}
	if (fin == "iu" || fin == "iou") {
		return PUA_IOU;
	}
	if (fin == "ian") {
		return PUA_IEN;
	}
	if (fin == "in") {
		return PUA_IN;
	}
	if (fin == "iang") {
		return PUA_IANG;
	}
	if (fin == "ing") {
		return PUA_ING;
	}
	if (fin == "iong") {
		return PUA_IUNG;
	}
	if (fin == "ua") {
		return PUA_UA;
	}
	if (fin == "uo") {
		return PUA_UO;
	}
	if (fin == "uai") {
		return PUA_UAI;
	}
	if (fin == "ui" || fin == "uei") {
		return PUA_UEI;
	}
	if (fin == "uan") {
		return PUA_UAN;
	}
	if (fin == "un" || fin == "uen") {
		return PUA_UEN;
	}
	if (fin == "uang") {
		return PUA_UANG;
	}
	if (fin == "ueng") {
		return PUA_UENG;
	}
	if (fin == "\xC3\xBC"
			"e" || fin == "ve") {
		return PUA_YE;
	}
	if (fin == "\xC3\xBC"
			"an" || fin == "van") {
		return PUA_YEN;
	}
	if (fin == "\xC3\xBC"
			"n" || fin == "vn") {
		return PUA_YN;
	}
	if (fin == KEY_RETRO) {
		return PUA_RETRO;
	}
	if (fin == KEY_ALVE) {
		return IPA_BARRED_I;
	}
	return 0;
}

Phoneme toneToPUA(int tone) {
	switch (tone) {
		case 1:
			return PUA_TONE1;
		case 2:
			return PUA_TONE2;
		case 3:
			return PUA_TONE3;
		case 4:
			return PUA_TONE4;
		case 5:
			return PUA_TONE5;
		default:
			return 0;
	}
}

bool startsWith(const std::string &s, const std::string &prefix) {
	return s.size() >= prefix.size() &&
			s.compare(0, prefix.size(), prefix) == 0;
}

std::string normalizePinyin(const std::string &py) {
	std::string s = py;
	{
		size_t pos = 0;
		while ((pos = s.find('v', pos)) != std::string::npos) {
			s.replace(pos, 1, "\xC3\xBC");
			pos += 2;
		}
	}

	if (startsWith(s, "yu")) {
		return std::string("\xC3\xBC") + s.substr(2);
	}
	if (!s.empty() && s[0] == 'y') {
		std::string remainder = s.substr(1);
		if (startsWith(remainder, "i")) {
			return remainder;
		}
		return "i" + remainder;
	}
	if (!s.empty() && s[0] == 'w') {
		std::string remainder = s.substr(1);
		if (startsWith(remainder, "u")) {
			return remainder;
		}
		return "u" + remainder;
	}
	return s;
}

struct PinyinSplit {
	std::string initial;
	std::string final_;
};

PinyinSplit splitPinyin(const std::string &pinyin) {
	for (int i = 0; i < NUM_INITIALS; ++i) {
		const auto &entry = INITIALS_ORDER[i];
		if (pinyin.size() >= static_cast<size_t>(entry.len) &&
				pinyin.compare(0, entry.len, entry.pinyin) == 0) {
			std::string init(entry.pinyin, entry.len);
			std::string fin = pinyin.substr(entry.len);

			if (fin == "i") {
				if (RETROFLEX_INITIALS.count(init)) {
					return {init, KEY_RETRO};
				}
				if (ALVEOLAR_INITIALS.count(init)) {
					return {init, KEY_ALVE};
				}
			}

			if ((init == "j" || init == "q" || init == "x") && !fin.empty() &&
					fin[0] == 'u') {
				fin = std::string("\xC3\xBC") + fin.substr(1);
			}

			return {init, fin};
		}
	}

	return {"", pinyin};
}

std::vector<Phoneme> pinyinToIPA(const std::string &syllable, int tone) {
	auto split = splitPinyin(syllable);
	std::vector<Phoneme> tokens;

	if (!split.initial.empty()) {
		Phoneme ipa = initialToIPA(split.initial);
		if (ipa != 0) {
			tokens.push_back(ipa);
		}
	}

	if (!split.final_.empty()) {
		Phoneme ipa = finalToIPA(split.final_);
		if (ipa != 0) {
			tokens.push_back(ipa);
		} else {
			for (char ch : split.final_) {
				if (ch >= 'a' && ch <= 'z') {
					std::string single(1, ch);
					Phoneme f = finalToIPA(single);
					if (f != 0) {
						tokens.push_back(f);
					} else {
						tokens.push_back(static_cast<Phoneme>(ch));
					}
				}
			}
		}
	}

	Phoneme toneMarker = toneToPUA(tone);
	if (toneMarker != 0) {
		tokens.push_back(toneMarker);
	}
	return tokens;
}

struct SyllableTone {
	std::string syllable;
	int tone;
};

void applyToneSandhi(std::vector<SyllableTone> &syllables) {
	int n = static_cast<int>(syllables.size());
	for (int i = 0; i < n - 1; ++i) {
		const auto &syllable = syllables[i].syllable;
		int tone = syllables[i].tone;
		int nextTone = syllables[i + 1].tone;

		if (tone == 3 && nextTone == 3) {
			syllables[i].tone = 2;
			continue;
		}

		if (syllable == "i" && tone == 1) {
			if (nextTone == 4) {
				syllables[i].tone = 2;
			} else if (nextTone >= 1 && nextTone <= 3) {
				syllables[i].tone = 4;
			}
			continue;
		}

		if (syllable == "bu" && tone == 4 && nextTone == 4) {
			syllables[i].tone = 2;
		}
	}
}

int extractTone(const std::string &syllable, std::string &base) {
	if (!syllable.empty() && syllable.back() >= '1' && syllable.back() <= '5') {
		base = syllable.substr(0, syllable.size() - 1);
		return syllable.back() - '0';
	}
	base = syllable;
	return 5;
}

std::vector<std::string> splitPinyinString(const std::string &s) {
	std::vector<std::string> result;
	size_t start = 0;
	while (start < s.size()) {
		while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) {
			++start;
		}
		if (start >= s.size()) {
			break;
		}
		size_t end = s.find_first_of(" \t", start);
		if (end == std::string::npos) {
			end = s.size();
		}
		result.push_back(s.substr(start, end - start));
		start = end;
	}
	return result;
}

std::string firstAlternative(const std::string &s) {
	size_t comma = s.find(',');
	if (comma != std::string::npos) {
		return s.substr(0, comma);
	}
	return s;
}

size_t phraseMatch(const std::vector<char32_t> &cps, size_t pos,
		const std::unordered_map<std::string, std::string> &phraseDict,
		std::string &pinyinOut) {
	size_t maxLen = std::min(cps.size() - pos, static_cast<size_t>(8));
	for (size_t len = maxLen; len >= 2; --len) {
		std::string key = cpsToUtf8(cps, pos, len);
		auto it = phraseDict.find(key);
		if (it != phraseDict.end()) {
			pinyinOut = it->second;
			return len;
		}
	}
	return 0;
}

struct CharPinyin {
	char32_t codepoint;
	bool isChinese;
	std::string normalized;
	int tone;
};

std::vector<CharPinyin> textToPinyin(const std::vector<char32_t> &cps,
		const std::unordered_map<int, std::string> &singleCharDict,
		const std::unordered_map<std::string, std::string> &phraseDict) {
	std::vector<CharPinyin> result;
	size_t n = cps.size();
	size_t i = 0;

	while (i < n) {
		char32_t cp = cps[i];

		if (!isCJK(cp)) {
			result.push_back({cp, false, "", 0});
			++i;
			continue;
		}

		std::string phrasePy;
		size_t matchLen = phraseMatch(cps, i, phraseDict, phrasePy);
		if (matchLen > 0) {
			auto syllables = splitPinyinString(phrasePy);
			for (size_t j = 0; j < matchLen; ++j) {
				std::string base;
				int tone = 5;
				if (j < syllables.size()) {
					tone = extractTone(syllables[j], base);
				}
				result.push_back({cps[i + j], true, normalizePinyin(base), tone});
			}
			i += matchLen;
			continue;
		}

		int cpInt = static_cast<int>(cp);
		auto it = singleCharDict.find(cpInt);
		if (it != singleCharDict.end()) {
			std::string raw = firstAlternative(it->second);
			std::string base;
			int tone = extractTone(raw, base);
			result.push_back({cp, true, normalizePinyin(base), tone});
		} else {
			result.push_back({cp, false, "", 0});
		}
		++i;
	}

	return result;
}

void applyToneSandhiToChars(std::vector<CharPinyin> &chars) {
	int n = static_cast<int>(chars.size());
	int i = 0;

	while (i < n) {
		if (!chars[i].isChinese) {
			++i;
			continue;
		}

		int groupStart = i;
		while (i < n && chars[i].isChinese) {
			++i;
		}
		int groupEnd = i;

		if (groupEnd - groupStart < 2) {
			continue;
		}

		std::vector<SyllableTone> tones;
		tones.reserve(groupEnd - groupStart);
		for (int j = groupStart; j < groupEnd; ++j) {
			tones.push_back({chars[j].normalized, chars[j].tone});
		}

		applyToneSandhi(tones);
		for (int j = groupStart; j < groupEnd; ++j) {
			chars[j].tone = tones[j - groupStart].tone;
		}
	}
}

bool loadPinyinDictsFromStreams(std::istream &singleInput, std::istream &phraseInput,
		std::unordered_map<int, std::string> &singleCharDict,
		std::unordered_map<std::string, std::string> &phraseDict) {
	singleCharDict.clear();
	phraseDict.clear();

	{
		json root;
		try {
			singleInput >> root;
		} catch (...) {
			return false;
		}

		for (auto &[key, value] : root.items()) {
			try {
				int codepoint = std::stoi(key);
				if (value.is_string()) {
					singleCharDict[codepoint] = value.get<std::string>();
				} else if (value.is_array() && !value.empty()) {
					singleCharDict[codepoint] = value[0].get<std::string>();
				}
			} catch (...) {
				continue;
			}
		}
	}

	{
		json root;
		try {
			phraseInput >> root;
		} catch (...) {
			singleCharDict.clear();
			phraseDict.clear();
			return false;
		}

		for (auto &[key, value] : root.items()) {
			if (value.is_string()) {
				phraseDict[key] = value.get<std::string>();
			} else if (value.is_array() && !value.empty()) {
				std::string pyStr;
				for (size_t idx = 0; idx < value.size(); ++idx) {
					if (idx > 0) {
						pyStr += " ";
					}
					if (value[idx].is_array() && !value[idx].empty()) {
						pyStr += value[idx][0].get<std::string>();
					} else if (value[idx].is_string()) {
						pyStr += value[idx].get<std::string>();
					}
				}
				if (!pyStr.empty()) {
					phraseDict[key] = pyStr;
				}
			}
		}
	}

	return true;
}

} // namespace

bool loadPinyinDicts(const std::string &singleCharPath, const std::string &phrasePath,
		std::unordered_map<int, std::string> &singleCharDict,
		std::unordered_map<std::string, std::string> &phraseDict) {
	std::ifstream singleInput(singleCharPath);
	if (!singleInput.is_open()) {
		return false;
	}

	std::ifstream phraseInput(phrasePath);
	if (!phraseInput.is_open()) {
		return false;
	}

	return loadPinyinDictsFromStreams(
			singleInput, phraseInput, singleCharDict, phraseDict);
}

bool loadPinyinDictsFromJsonStrings(const std::string &singleCharJson,
		const std::string &phraseJson,
		std::unordered_map<int, std::string> &singleCharDict,
		std::unordered_map<std::string, std::string> &phraseDict) {
	std::istringstream singleInput(singleCharJson);
	std::istringstream phraseInput(phraseJson);
	return loadPinyinDictsFromStreams(
			singleInput, phraseInput, singleCharDict, phraseDict);
}

void phonemize_chinese(const std::string &text,
		std::vector<std::vector<Phoneme>> &phonemes,
		const std::unordered_map<int, std::string> &singleCharDict,
		const std::unordered_map<std::string, std::string> &phraseDict) {
	phonemes.clear();

	if (!utf8::is_valid(text.begin(), text.end())) {
		return;
	}

	auto cps = toCodepoints(text);
	if (cps.empty()) {
		return;
	}

	auto charPinyins = textToPinyin(cps, singleCharDict, phraseDict);
	applyToneSandhiToChars(charPinyins);

	std::vector<Phoneme> sentence;
	for (const auto &cp : charPinyins) {
		if (!cp.isChinese) {
			char32_t ch = cp.codepoint;
			Phoneme mapped = mapZhPunct(ch);
			if (mapped != 0) {
				sentence.push_back(mapped);
				continue;
			}
			if (isZhPunctuation(ch)) {
				sentence.push_back(ch);
				continue;
			}
			if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
				sentence.push_back(static_cast<Phoneme>(' '));
				continue;
			}
			if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') ||
					(ch >= 'a' && ch <= 'z')) {
				sentence.push_back(ch);
			}
			continue;
		}

		std::string normalized = cp.normalized;
		int tone = cp.tone;

		bool hasErhua = false;
		if (normalized.size() > 1 && normalized != "er" &&
				normalized.back() == 'r') {
			hasErhua = true;
			normalized = normalized.substr(0, normalized.size() - 1);
		}

		auto ipaTokens = pinyinToIPA(normalized, tone);
		if (hasErhua && !ipaTokens.empty()) {
			Phoneme lastToken = ipaTokens.back();
			bool lastIsTone =
					(lastToken >= PUA_TONE1 && lastToken <= PUA_TONE5);
			if (lastIsTone) {
				ipaTokens.insert(ipaTokens.end() - 1, IPA_RSCHWA);
			} else {
				ipaTokens.push_back(IPA_RSCHWA);
			}
		}

		for (Phoneme phoneme : ipaTokens) {
			sentence.push_back(phoneme);
		}
	}

	if (!sentence.empty()) {
		phonemes.push_back(std::move(sentence));
	}
}

} // namespace piper
