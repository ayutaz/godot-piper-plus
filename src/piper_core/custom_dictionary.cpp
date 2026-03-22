#include "custom_dictionary.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

#include "json.hpp"
#include "phoneme_parser.hpp"

using json = nlohmann::json;

namespace piper {

namespace {

bool isReservedV2Key(const std::string& word) {
    return word == "version" || word == "description" || word == "metadata" || word == "entries";
}

int parsePriority(const json& value, int defaultPriority = 5) {
    if (value.is_number_integer()) {
        return value.get<int>();
    }

    if (value.is_number()) {
        return static_cast<int>(value.get<double>());
    }

    return defaultPriority;
}

void appendV2Entries(const json& entries,
                     std::unordered_map<std::string, DictionaryEntry>& result) {
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        const std::string word = it.key();
        const json& value = it.value();

        if (word.empty() || isReservedV2Key(word)) {
            continue;
        }

        if (value.is_object()) {
            auto pronIt = value.find("pronunciation");
            if (pronIt == value.end() || !pronIt->is_string()) {
                continue;
            }

            std::string pronunciation = pronIt->get<std::string>();
            if (pronunciation.empty()) {
                continue;
            }

            int priority = 5;
            auto priorityIt = value.find("priority");
            if (priorityIt != value.end()) {
                priority = parsePriority(*priorityIt);
            }

            result[word] = DictionaryEntry(pronunciation, priority);
            continue;
        }

        if (value.is_string()) {
            std::string pronunciation = value.get<std::string>();
            if (!pronunciation.empty()) {
                result[word] = DictionaryEntry(pronunciation, 5);
            }
        }
    }
}

void appendLegacyEditorEntries(const json& entries,
                               std::unordered_map<std::string, DictionaryEntry>& result) {
    for (const auto& value : entries) {
        if (!value.is_object()) {
            continue;
        }

        auto patternIt = value.find("pattern");
        auto replacementIt = value.find("replacement");
        if (patternIt == value.end() || replacementIt == value.end() ||
            !patternIt->is_string() || !replacementIt->is_string()) {
            continue;
        }

        int priority = 5;
        auto priorityIt = value.find("priority");
        if (priorityIt != value.end()) {
            priority = parsePriority(*priorityIt);
        }

        const std::string pattern = patternIt->get<std::string>();
        const std::string replacement = replacementIt->get<std::string>();
        if (pattern.empty() || replacement.empty()) {
            continue;
        }

        result[pattern] = DictionaryEntry(replacement, priority);
    }
}

std::unordered_map<std::string, DictionaryEntry> parseJsonDictionary(const std::string& content) {
    std::unordered_map<std::string, DictionaryEntry> result;
    json root = json::parse(content);

    if (!root.is_object()) {
        throw std::runtime_error("Dictionary root must be a JSON object.");
    }

    auto entriesIt = root.find("entries");
    if (entriesIt != root.end()) {
        if (entriesIt->is_object()) {
            appendV2Entries(*entriesIt, result);
            return result;
        }

        if (entriesIt->is_array()) {
            appendLegacyEditorEntries(*entriesIt, result);
            return result;
        }

        throw std::runtime_error("Dictionary entries must be an object or array.");
    }

    for (auto it = root.begin(); it != root.end(); ++it) {
        if (isReservedV2Key(it.key()) || !it.value().is_string()) {
            continue;
        }

        std::string pronunciation = it.value().get<std::string>();
        if (!pronunciation.empty()) {
            result[it.key()] = DictionaryEntry(pronunciation, 5);
        }
    }

    return result;
}

} // anonymous namespace

CustomDictionary::CustomDictionary() {
    // デフォルト辞書ディレクトリを設定
    // 実行ファイルからの相対パスで設定
    defaultDictDir_ = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() 
                      / "data" / "dictionaries";
    
    loadDefaultDictionaries();
}

CustomDictionary::CustomDictionary(const std::string& dictPath) : CustomDictionary() {
    loadDictionary(dictPath);
}

CustomDictionary::CustomDictionary(const std::vector<std::string>& dictPaths) : CustomDictionary() {
    for (const auto& path : dictPaths) {
        loadDictionary(path);
    }
}

void CustomDictionary::loadDefaultDictionaries() {
    std::vector<std::string> defaultDicts = {
        "default_tech_dict.json",
        "default_common_dict.json",
        "additional_tech_dict.json",  // 最新トレンドの技術用語
        "user_custom_dict.json"        // ユーザーカスタム辞書（日本語発音修正用）
    };
    
    for (const auto& dictName : defaultDicts) {
        auto dictPath = defaultDictDir_ / dictName;
        if (std::filesystem::exists(dictPath)) {
            try {
                loadDictionary(dictPath.string());
            } catch (const std::exception& e) {
                std::cerr << "Warning: Failed to load default dictionary " 
                          << dictPath << ": " << e.what() << std::endl;
            }
        }
    }
}

void CustomDictionary::loadDictionary(const std::string& dictPath) {
    if (!std::filesystem::exists(dictPath)) {
        throw std::runtime_error("Dictionary file not found: " + dictPath);
    }
    
    std::ifstream file(dictPath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open dictionary file: " + dictPath);
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());

    auto entries = parseJsonDictionary(content);
    for (const auto& [word, entry] : entries) {
        addEntry(word, entry);
    }
}

void CustomDictionary::addEntry(const std::string& word, const DictionaryEntry& entry) {
    if (isMixedCase(word)) {
        // 大文字小文字が混在している場合は区別する
        caseSensitiveEntries_[word] = entry;
    } else {
        // 全て大文字または小文字の場合は正規化
        std::string normalizedWord = toLowerCase(word);
        
        // 既存エントリとの優先度比較
        auto it = entries_.find(normalizedWord);
        if (it != entries_.end()) {
            if (entry.priority <= it->second.priority) {
                return; // 既存の方が優先度が高い
            }
        }
        
        entries_[normalizedWord] = entry;
    }
}

std::string CustomDictionary::applyToText(const std::string& text) const {
    std::string result = text;
    
    // エントリを長さでソート（長い単語から処理）
    std::vector<std::pair<std::string, DictionaryEntry>> sortedCaseSensitive(
        caseSensitiveEntries_.begin(), caseSensitiveEntries_.end());
    std::sort(sortedCaseSensitive.begin(), sortedCaseSensitive.end(),
              [](const auto& a, const auto& b) { return a.first.length() > b.first.length(); });
    
    std::vector<std::pair<std::string, DictionaryEntry>> sortedEntries(
        entries_.begin(), entries_.end());
    std::sort(sortedEntries.begin(), sortedEntries.end(),
              [](const auto& a, const auto& b) { return a.first.length() > b.first.length(); });
    
    // 大文字小文字を区別するエントリを処理
    for (const auto& [word, entry] : sortedCaseSensitive) {
        std::regex pattern = getWordPattern(word, true);
        result = std::regex_replace(result, pattern, entry.pronunciation);
    }
    
    // 大文字小文字を区別しないエントリを処理
    for (const auto& [word, entry] : sortedEntries) {
        std::regex pattern = getWordPattern(word, false);
        result = std::regex_replace(result, pattern, entry.pronunciation);
    }
    
    return result;
}

void CustomDictionary::addWord(const std::string& word, const std::string& pronunciation, int priority) {
    addEntry(word, DictionaryEntry(pronunciation, priority));
    patternCache_.clear(); // キャッシュをクリア
}

bool CustomDictionary::removeWord(const std::string& word) {
    bool removed = false;
    
    if (caseSensitiveEntries_.erase(word) > 0) {
        removed = true;
    }
    
    std::string normalizedWord = toLowerCase(word);
    if (entries_.erase(normalizedWord) > 0) {
        removed = true;
    }
    
    if (removed) {
        patternCache_.clear();
    }
    
    return removed;
}

std::optional<std::string> CustomDictionary::getPronunciation(const std::string& word) const {
    // 大文字小文字を区別してチェック
    auto it = caseSensitiveEntries_.find(word);
    if (it != caseSensitiveEntries_.end()) {
        return it->second.pronunciation;
    }
    
    // 正規化してチェック
    std::string normalizedWord = toLowerCase(word);
    auto it2 = entries_.find(normalizedWord);
    if (it2 != entries_.end()) {
        return it2->second.pronunciation;
    }
    
    return std::nullopt;
}

void CustomDictionary::saveDictionary(const std::string& outputPath) const {
    std::ofstream file(outputPath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open output file: " + outputPath);
    }

    json root;
    root["version"] = "2.0";
    root["description"] = "Custom dictionary exported from Piper";
    root["metadata"] = {
        {"created", "auto-generated"},
        {"author", "Piper"},
        {"license", "MIT"},
    };
    root["entries"] = json::object();

    for (const auto& [word, entry] : entries_) {
        root["entries"][word] = {
            {"pronunciation", entry.pronunciation},
            {"priority", entry.priority},
        };
    }

    for (const auto& [word, entry] : caseSensitiveEntries_) {
        root["entries"][word] = {
            {"pronunciation", entry.pronunciation},
            {"priority", entry.priority},
        };
    }

    file << root.dump(2) << '\n';
}

CustomDictionary::Stats CustomDictionary::getStats() const {
    return {
        entries_.size() + caseSensitiveEntries_.size(),
        entries_.size(),
        caseSensitiveEntries_.size()
    };
}

std::string CustomDictionary::toLowerCase(const std::string& str) const {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

bool CustomDictionary::isMixedCase(const std::string& str) const {
    bool hasUpper = false;
    bool hasLower = false;
    
    for (char c : str) {
        if (std::isupper(c)) hasUpper = true;
        if (std::islower(c)) hasLower = true;
        if (hasUpper && hasLower) return true;
    }
    
    return false;
}

std::regex CustomDictionary::getWordPattern(const std::string& word, bool caseSensitive) const {
    std::string cacheKey = word + "_" + (caseSensitive ? "1" : "0");
    
    auto it = patternCache_.find(cacheKey);
    if (it != patternCache_.end()) {
        return it->second;
    }
    
    // エスケープ処理
    std::string escapedWord;
    for (char c : word) {
        if (std::string(".^$*+?{}[]|()\\").find(c) != std::string::npos) {
            escapedWord += '\\';
        }
        escapedWord += c;
    }
    
    auto isWordChar = [](unsigned char c) {
        return std::isalnum(c) || c == '_';
    };

    // 単語境界は英数字語にだけ適用する。
    // C++ や @user のような記号を含むエントリは、そのままの文字列一致で扱う。
    std::string patternStr = escapedWord;
    if (!word.empty() &&
        isWordChar(static_cast<unsigned char>(word.front())) &&
        isWordChar(static_cast<unsigned char>(word.back()))) {
        patternStr = "\\b" + escapedWord + "\\b";
    }
    
    auto flags = std::regex::ECMAScript;
    if (!caseSensitive) {
        flags |= std::regex::icase;
    }
    
    std::regex pattern(patternStr, flags);
    patternCache_[cacheKey] = pattern;
    
    return pattern;
}

// 便利な関数の実装
std::unique_ptr<CustomDictionary> createDefaultDictionary() {
    return std::make_unique<CustomDictionary>();
}

std::string applyCustomDictionaryToTextSegments(const std::string& text,
                                                const CustomDictionary* dictionary) {
    if (dictionary == nullptr || text.empty()) {
        return text;
    }

    auto segments = parsePhonemeNotation(text);
    if (segments.empty()) {
        return dictionary->applyToText(text);
    }

    std::string result;
    for (const auto& segment : segments) {
        if (segment.isPhonemes) {
            result += "[[ ";
            result += segment.text;
            result += " ]]";
        } else {
            result += dictionary->applyToText(segment.text);
        }
    }

    return result;
}

std::string applyCustomDictionary(const std::string& text, 
                                 const std::vector<std::string>& dictPaths) {
    CustomDictionary dict(dictPaths);
    return applyCustomDictionaryToTextSegments(text, &dict);
}

} // namespace piper
