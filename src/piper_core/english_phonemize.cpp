#include "english_phonemize.hpp"

#include "json.hpp"
#include "utf8.h"
#include "utf8_utils.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using json = nlohmann::json;

namespace piper {
namespace {

bool loadCmuDictFromJson(const json &j,
                         std::unordered_map<std::string, std::string> &dict) {
    if (!j.is_object()) {
        return false;
    }

    dict.clear();
    dict.reserve(j.size());
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (it.value().is_string()) {
            dict[it.key()] = it.value().get<std::string>();
        }
    }
    return true;
}

constexpr char32_t IPA_TURNED_A   = 0x0251;
constexpr char32_t IPA_ASH        = 0x00E6;
constexpr char32_t IPA_TURNED_V   = 0x028C;
constexpr char32_t IPA_SCHWA      = 0x0259;
constexpr char32_t IPA_OPEN_O     = 0x0254;
constexpr char32_t IPA_EPSILON    = 0x025B;
constexpr char32_t IPA_RHOTIC_SCH = 0x025A;
constexpr char32_t IPA_REV_OPEN_E = 0x025C;
constexpr char32_t IPA_SM_CAP_I   = 0x026A;
constexpr char32_t IPA_HORSESHOE  = 0x028A;
constexpr char32_t IPA_LENGTH     = 0x02D0;

constexpr char32_t IPA_VOICED_G   = 0x0261;
constexpr char32_t IPA_ENG        = 0x014B;
constexpr char32_t IPA_ALVEOLAR_R = 0x0279;
constexpr char32_t IPA_ESH        = 0x0283;
constexpr char32_t IPA_EZH        = 0x0292;
constexpr char32_t IPA_THETA      = 0x03B8;
constexpr char32_t IPA_ETH        = 0x00F0;

constexpr char32_t IPA_PRIMARY    = 0x02C8;
constexpr char32_t IPA_SECONDARY  = 0x02CC;

static const std::unordered_map<std::string, std::vector<char32_t>> &arpaToIpa() {
    static const std::unordered_map<std::string, std::vector<char32_t>> table = {
        {"AA",  {IPA_TURNED_A}},
        {"AE",  {IPA_ASH}},
        {"AH",  {IPA_TURNED_V}},
        {"AO",  {IPA_OPEN_O, IPA_LENGTH}},
        {"AW",  {'a', IPA_HORSESHOE}},
        {"AY",  {'a', IPA_SM_CAP_I}},
        {"B",   {'b'}},
        {"CH",  {'t', IPA_ESH}},
        {"D",   {'d'}},
        {"DH",  {IPA_ETH}},
        {"EH",  {IPA_EPSILON}},
        {"ER",  {IPA_RHOTIC_SCH}},
        {"EY",  {'e', IPA_SM_CAP_I}},
        {"F",   {'f'}},
        {"G",   {IPA_VOICED_G}},
        {"HH",  {'h'}},
        {"IH",  {IPA_SM_CAP_I}},
        {"IY",  {'i', IPA_LENGTH}},
        {"JH",  {'d', IPA_EZH}},
        {"K",   {'k'}},
        {"L",   {'l'}},
        {"M",   {'m'}},
        {"N",   {'n'}},
        {"NG",  {IPA_ENG}},
        {"OW",  {'o', IPA_HORSESHOE}},
        {"OY",  {IPA_OPEN_O, IPA_SM_CAP_I}},
        {"P",   {'p'}},
        {"R",   {IPA_ALVEOLAR_R}},
        {"S",   {'s'}},
        {"SH",  {IPA_ESH}},
        {"T",   {'t'}},
        {"TH",  {IPA_THETA}},
        {"UH",  {IPA_HORSESHOE}},
        {"UW",  {'u', IPA_LENGTH}},
        {"V",   {'v'}},
        {"W",   {'w'}},
        {"Y",   {'j'}},
        {"Z",   {'z'}},
        {"ZH",  {IPA_EZH}},
    };
    return table;
}

static const std::vector<char32_t> AH_UNSTRESSED = {IPA_SCHWA};
static const std::vector<char32_t> ER_STRESSED = {IPA_REV_OPEN_E, IPA_LENGTH};
static const std::vector<char32_t> AA_R_MERGED = {IPA_TURNED_A, IPA_LENGTH, IPA_ALVEOLAR_R};

static bool isPunctuation(char32_t cp) {
    return cp == ',' || cp == '.' || cp == ';' || cp == ':' ||
           cp == '!' || cp == '?';
}

static const std::unordered_set<std::string> &functionWords() {
    static const std::unordered_set<std::string> words = {
        "a", "an", "the",
        "i", "me", "my", "mine", "myself",
        "you", "your", "yours", "yourself",
        "he", "him", "his", "himself",
        "she", "her", "hers", "herself",
        "it", "its", "itself",
        "we", "us", "our", "ours", "ourselves",
        "they", "them", "their", "theirs", "themselves",
        "am", "is", "are", "was", "were", "be", "been", "being",
        "have", "has", "had", "having",
        "do", "does", "did",
        "will", "would", "shall", "should",
        "can", "could", "may", "might", "must",
        "at", "by", "for", "from", "in", "of", "on", "to", "with",
        "about", "after", "before", "between", "into", "through", "under",
        "and", "but", "or", "nor", "so", "yet",
        "if", "that", "than", "when", "while", "as", "because", "since",
        "not", "no",
    };
    return words;
}

using utf8_util::toCodepoints;

struct Token {
    std::string text;
    bool isWord;
};

static bool isAlphaOrApostrophe(char32_t cp) {
    return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') || cp == '\'';
}

static char32_t toLowerAscii(char32_t cp) {
    return (cp >= 'A' && cp <= 'Z') ? (cp + 32) : cp;
}

static std::vector<Token> tokenize(const std::string &text) {
    auto cps = toCodepoints(text);
    std::vector<Token> tokens;
    size_t i = 0;
    while (i < cps.size()) {
        char32_t ch = cps[i];
        if (isAlphaOrApostrophe(ch)) {
            std::string word;
            while (i < cps.size() && isAlphaOrApostrophe(cps[i])) {
                utf8::unchecked::append(toLowerAscii(cps[i]), std::back_inserter(word));
                ++i;
            }
            tokens.push_back({std::move(word), true});
            continue;
        }
        if (isPunctuation(ch)) {
            std::string punct;
            utf8::unchecked::append(ch, std::back_inserter(punct));
            tokens.push_back({std::move(punct), false});
            ++i;
            continue;
        }
        ++i;
    }
    return tokens;
}

struct ArpaToken {
    std::string base;
    int stress;
};

static std::vector<ArpaToken> parseArpabet(const std::string &arpa) {
    std::vector<ArpaToken> result;
    std::istringstream iss(arpa);
    std::string tok;
    while (iss >> tok) {
        if (tok.empty()) {
            continue;
        }
        char last = tok.back();
        if (last >= '0' && last <= '2') {
            result.push_back({tok.substr(0, tok.size() - 1), last - '0'});
        } else {
            result.push_back({tok, -1});
        }
    }
    return result;
}

struct IpaPhoneme {
    std::vector<char32_t> ipa;
    int stress;
};

static std::vector<IpaPhoneme> convertWordToIpa(const std::vector<ArpaToken> &tokens) {
    std::vector<IpaPhoneme> result;
    const auto &table = arpaToIpa();
    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto &tok = tokens[i];
        if (tok.base == "AA" && i + 1 < tokens.size() &&
                tokens[i + 1].base == "R" && tokens[i + 1].stress == -1) {
            result.push_back({AA_R_MERGED, tok.stress});
            ++i;
            continue;
        }
        if (tok.base == "ER" && tok.stress == 1) {
            result.push_back({ER_STRESSED, tok.stress});
            continue;
        }
        if (tok.base == "AH" && tok.stress == 0) {
            result.push_back({AH_UNSTRESSED, tok.stress});
            continue;
        }
        auto it = table.find(tok.base);
        if (it != table.end()) {
            result.push_back({it->second, tok.stress});
        }
    }
    return result;
}

static void destress(std::vector<IpaPhoneme> &ipas) {
    for (auto &p : ipas) {
        if (p.stress >= 1) {
            p.stress = 0;
        }
    }
}

static void emitWord(const std::vector<IpaPhoneme> &ipas,
                     std::vector<Phoneme> &sentence) {
    for (const auto &p : ipas) {
        if (p.stress == 1) {
            sentence.push_back(IPA_PRIMARY);
        } else if (p.stress == 2) {
            sentence.push_back(IPA_SECONDARY);
        }
        for (char32_t ch : p.ipa) {
            sentence.push_back(ch);
        }
    }
}

static std::vector<std::string> getSourceWords(const std::vector<Token> &tokens) {
    std::vector<std::string> words;
    for (const auto &tok : tokens) {
        if (tok.isWord) {
            words.push_back(tok.text);
        }
    }
    return words;
}

static std::string tryMorphologicalFallback(
        const std::string &word,
        const std::unordered_map<std::string, std::string> &cmuDict) {
    auto tryBase = [&](const std::string &base, const char *suffixArpa) -> std::string {
        auto it = cmuDict.find(base);
        if (it != cmuDict.end()) {
            return it->second + " " + suffixArpa;
        }
        return {};
    };

    const size_t len = word.size();

    if (len > 4 && word.compare(len - 3, 3, "ing") == 0) {
        std::string base = word.substr(0, len - 3);
        auto r = tryBase(base, "IH0 NG");
        if (!r.empty()) return r;
        if (base.size() >= 2 && base.back() == base[base.size() - 2]) {
            r = tryBase(base.substr(0, base.size() - 1), "IH0 NG");
            if (!r.empty()) return r;
        }
        r = tryBase(base + "e", "IH0 NG");
        if (!r.empty()) return r;
    }

    if (len > 3 && word.compare(len - 2, 2, "ed") == 0) {
        std::string base = word.substr(0, len - 2);
        auto r = tryBase(base, "D");
        if (!r.empty()) return r;
        if (base.size() >= 2 && base.back() == base[base.size() - 2]) {
            r = tryBase(base.substr(0, base.size() - 1), "D");
            if (!r.empty()) return r;
        }
        r = tryBase(word.substr(0, len - 1), "D");
        if (!r.empty()) return r;
    }

    if (len > 2 && word.back() == 's') {
        if (len > 4 && word.compare(len - 3, 3, "ies") == 0) {
            auto r = tryBase(word.substr(0, len - 3) + "y", "Z");
            if (!r.empty()) return r;
        }
        if (len > 3 && word.compare(len - 2, 2, "es") == 0) {
            auto r = tryBase(word.substr(0, len - 2), "IH0 Z");
            if (!r.empty()) return r;
        }
        auto r = tryBase(word.substr(0, len - 1), "Z");
        if (!r.empty()) return r;
    }

    if (len > 3 && word.compare(len - 2, 2, "er") == 0) {
        std::string base = word.substr(0, len - 2);
        auto r = tryBase(base, "ER0");
        if (!r.empty()) return r;
        if (base.size() >= 2 && base.back() == base[base.size() - 2]) {
            r = tryBase(base.substr(0, base.size() - 1), "ER0");
            if (!r.empty()) return r;
        }
    }

    if (len > 3 && word.compare(len - 2, 2, "ly") == 0) {
        std::string base = word.substr(0, len - 2);
        auto r = tryBase(base, "L IY0");
        if (!r.empty()) return r;
        if (len > 4 && word[len - 3] == 'i') {
            r = tryBase(word.substr(0, len - 3) + "y", "L IY0");
            if (!r.empty()) return r;
        }
    }

    if (len > 4 && word.compare(len - 3, 3, "est") == 0) {
        auto r = tryBase(word.substr(0, len - 3), "AH0 S T");
        if (!r.empty()) return r;
    }

    return {};
}

} // namespace

bool loadCmuDict(const std::string &jsonPath,
                 std::unordered_map<std::string, std::string> &dict) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        return false;
    }

    try {
        json j = json::parse(file);
        return loadCmuDictFromJson(j, dict);
    } catch (const json::exception &) {
        return false;
    }
}

bool loadCmuDictFromJsonString(const std::string &jsonText,
                               std::unordered_map<std::string, std::string> &dict) {
    dict.clear();
    try {
        json j = json::parse(jsonText);
        return loadCmuDictFromJson(j, dict);
    } catch (const json::exception &) {
        return false;
    }
}

void phonemize_english(const std::string &text,
                       std::vector<std::vector<Phoneme>> &phonemes,
                       const std::unordered_map<std::string, std::string> &cmuDict) {
    phonemes.clear();
    if (!utf8::is_valid(text.begin(), text.end())) {
        return;
    }

    auto tokens = tokenize(text);
    if (tokens.empty()) {
        return;
    }

    auto sourceWords = getSourceWords(tokens);
    const auto &funcWords = functionWords();

    std::vector<bool> wordIsFunction(tokens.size(), false);
    size_t srcIdx = 0;
    for (size_t ti = 0; ti < tokens.size(); ++ti) {
        if (tokens[ti].isWord && srcIdx < sourceWords.size()) {
            wordIsFunction[ti] = funcWords.count(sourceWords[srcIdx]) > 0;
            ++srcIdx;
        }
    }

    std::vector<Phoneme> sentence;
    bool needSpace = false;

    for (size_t ti = 0; ti < tokens.size(); ++ti) {
        const auto &tok = tokens[ti];
        if (!tok.isWord) {
            for (auto cp : toCodepoints(tok.text)) {
                sentence.push_back(cp);
            }
            needSpace = true;
            continue;
        }

        auto dictIt = cmuDict.find(tok.text);
        std::string morphArpa;
        if (dictIt == cmuDict.end()) {
            morphArpa = tryMorphologicalFallback(tok.text, cmuDict);
            if (morphArpa.empty()) {
                needSpace = true;
                continue;
            }
        }

        if (needSpace) {
            sentence.push_back(static_cast<Phoneme>(' '));
        }

        const std::string &arpaStr =
            (dictIt != cmuDict.end()) ? dictIt->second : morphArpa;
        auto arpaTokens = parseArpabet(arpaStr);
        auto wordIpas = convertWordToIpa(arpaTokens);
        if (wordIsFunction[ti]) {
            destress(wordIpas);
        }
        emitWord(wordIpas, sentence);
        needSpace = true;
    }

    if (!sentence.empty()) {
        phonemes.push_back(std::move(sentence));
    }
}

} // namespace piper
