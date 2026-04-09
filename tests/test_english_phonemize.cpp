#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "english_phonemize.hpp"
#include "piper.hpp"

namespace {

std::string sentenceToUtf8(const std::vector<piper::Phoneme> &sentence) {
    std::string result;
    for (auto cp : sentence) {
        result += piper::phonemeToString(cp);
    }
    return result;
}

} // namespace

TEST(EnglishPhonemize, LoadCmuDictFromJson) {
    const auto fixturePath = std::filesystem::path(__FILE__).parent_path() / "fixtures" / "test_cmudict_data.json";

    std::unordered_map<std::string, std::string> dict;
    EXPECT_TRUE(piper::loadCmuDict(fixturePath.string(), dict));
    EXPECT_EQ(dict.at("the"), "DH AH0");
    EXPECT_EQ(dict.at("cat"), "K AE1 T");
    EXPECT_EQ(dict.at("cats"), "K AE1 T Z");
}

TEST(EnglishPhonemize, LoadCmuDictFromJsonString) {
    const std::string jsonText = R"({
        "the": "DH AH0",
        "cat": "K AE1 T"
    })";

    std::unordered_map<std::string, std::string> dict;
    EXPECT_TRUE(piper::loadCmuDictFromJsonString(jsonText, dict));
    EXPECT_EQ(dict.at("the"), "DH AH0");
    EXPECT_EQ(dict.at("cat"), "K AE1 T");
}

TEST(EnglishPhonemize, LoadCmuDictFromInvalidJsonStringFails) {
    std::unordered_map<std::string, std::string> dict = {
        {"stale", "S T EY1 L"},
    };

    EXPECT_FALSE(piper::loadCmuDictFromJsonString("{invalid", dict));
    EXPECT_TRUE(dict.empty());
}

TEST(EnglishPhonemize, PhonemizeDestressesFunctionWords) {
    std::unordered_map<std::string, std::string> dict = {
        {"the", "DH AH0"},
        {"cat", "K AE1 T"},
    };

    std::vector<std::vector<piper::Phoneme>> phonemes;
    piper::phonemize_english("the cat", phonemes, dict);

    ASSERT_EQ(phonemes.size(), 1u);
    ASSERT_EQ(phonemes[0].size(), 7u);
    EXPECT_EQ(sentenceToUtf8(phonemes[0]), u8"ðə kˈæt");
}

TEST(EnglishPhonemize, PhonemizeUsesMorphologicalFallback) {
    std::unordered_map<std::string, std::string> dict = {
        {"cat", "K AE1 T"},
    };

    std::vector<std::vector<piper::Phoneme>> phonemes;
    piper::phonemize_english("cats", phonemes, dict);

    ASSERT_EQ(phonemes.size(), 1u);
    EXPECT_EQ(sentenceToUtf8(phonemes[0]), u8"kˈætz");
}
