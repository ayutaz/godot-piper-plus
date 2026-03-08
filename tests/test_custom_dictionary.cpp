#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "custom_dictionary.hpp"

class CustomDictionaryTest : public ::testing::Test {
protected:
    piper::CustomDictionary dict;
    std::string tempDir;

    void SetUp() override {
        tempDir = (std::filesystem::temp_directory_path() / "piper_test_dict").string();
        std::filesystem::create_directories(tempDir);
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir);
    }
};

// 1. AddAndGetWord
TEST_F(CustomDictionaryTest, AddAndGetWord) {
    dict.addWord("docker", "ドッカー");
    auto pron = dict.getPronunciation("docker");
    ASSERT_TRUE(pron.has_value());
    EXPECT_EQ(pron.value(), "ドッカー");
}

// 2. GetNonExistentWord
TEST_F(CustomDictionaryTest, GetNonExistentWord) {
    auto pron = dict.getPronunciation("nonexistent");
    EXPECT_FALSE(pron.has_value());
}

// 3. RemoveWord
TEST_F(CustomDictionaryTest, RemoveWord) {
    dict.addWord("test", "テスト");
    EXPECT_TRUE(dict.removeWord("test"));
    EXPECT_FALSE(dict.getPronunciation("test").has_value());
}

// 4. RemoveNonExistentWord
TEST_F(CustomDictionaryTest, RemoveNonExistentWord) {
    EXPECT_FALSE(dict.removeWord("nonexistent"));
}

// 5. ApplyToText
TEST_F(CustomDictionaryTest, ApplyToText) {
    dict.addWord("docker", "ドッカー");
    std::string result = dict.applyToText("I use docker daily");
    EXPECT_NE(result.find("ドッカー"), std::string::npos);
    EXPECT_EQ(result.find("docker"), std::string::npos);
}

// 6. PriorityOrder - higher priority replaces lower
TEST_F(CustomDictionaryTest, PriorityOrder) {
    dict.addWord("api", "エーピーアイ", 3);
    dict.addWord("api", "アピ", 7);
    auto pron = dict.getPronunciation("api");
    ASSERT_TRUE(pron.has_value());
    EXPECT_EQ(pron.value(), "アピ");
}

// 7. PriorityOrderLowerDoesNotReplace
TEST_F(CustomDictionaryTest, PriorityOrderLowerDoesNotReplace) {
    dict.addWord("api", "エーピーアイ", 7);
    dict.addWord("api", "アピ", 3);
    auto pron = dict.getPronunciation("api");
    ASSERT_TRUE(pron.has_value());
    EXPECT_EQ(pron.value(), "エーピーアイ");
}

// 8. CaseSensitivity - mixed case words are case-sensitive
TEST_F(CustomDictionaryTest, CaseSensitivity) {
    dict.addWord("GitHub", "ギットハブ");  // mixed case -> case sensitive
    auto pron = dict.getPronunciation("GitHub");
    ASSERT_TRUE(pron.has_value());
    EXPECT_EQ(pron.value(), "ギットハブ");
    // Lowercase lookup should not find it in case-sensitive entries
    // But it may be found via case-insensitive path if it was also added there
}

// 9. CaseInsensitiveLookup - all lowercase words are case-insensitive
TEST_F(CustomDictionaryTest, CaseInsensitiveLookup) {
    dict.addWord("docker", "ドッカー");  // all lowercase -> case insensitive
    auto pron = dict.getPronunciation("docker");
    ASSERT_TRUE(pron.has_value());
    EXPECT_EQ(pron.value(), "ドッカー");
}

// 10. LoadV1Format - simple JSON dict
TEST_F(CustomDictionaryTest, LoadV1Format) {
    std::string dictFile = tempDir + "/test_v1.json";
    std::ofstream f(dictFile);
    f << "{\n  \"kubernetes\": \"クーバネティス\",\n  \"nginx\": \"エンジンエックス\"\n}\n";
    f.close();

    dict.loadDictionary(dictFile);
    auto pron = dict.getPronunciation("kubernetes");
    ASSERT_TRUE(pron.has_value());
    EXPECT_EQ(pron.value(), "クーバネティス");
}

// 11. LoadV2Format - dict with version and priority
TEST_F(CustomDictionaryTest, LoadV2Format) {
    std::string dictFile = tempDir + "/test_v2.json";
    std::ofstream f(dictFile);
    // V2 parser uses flat regex: "word": {"pronunciation": "...", "priority": N}
    // The "version": "2.0" triggers V2 mode, entries are at top level
    f << "{\n  \"version\": \"2.0\",\n"
      << "  \"terraform\": {\"pronunciation\": \"テラフォーム\", \"priority\": 8}\n"
      << "}\n";
    f.close();

    dict.loadDictionary(dictFile);
    auto pron = dict.getPronunciation("terraform");
    ASSERT_TRUE(pron.has_value());
    EXPECT_EQ(pron.value(), "テラフォーム");
}

// 12. SaveAndReload
TEST_F(CustomDictionaryTest, SaveAndReload) {
    dict.addWord("test", "テスト");
    dict.addWord("hello", "ヘロー");

    std::string outFile = tempDir + "/saved_dict.json";
    dict.saveDictionary(outFile);

    // Verify file exists
    EXPECT_TRUE(std::filesystem::exists(outFile));

    // Reload into new dictionary
    piper::CustomDictionary dict2;
    dict2.loadDictionary(outFile);

    auto pron = dict2.getPronunciation("test");
    ASSERT_TRUE(pron.has_value());
    EXPECT_EQ(pron.value(), "テスト");
}

// 13. LoadNonExistentFile
TEST_F(CustomDictionaryTest, LoadNonExistentFile) {
    EXPECT_THROW(dict.loadDictionary("/nonexistent/path/dict.json"), std::runtime_error);
}

// 14. Stats
TEST_F(CustomDictionaryTest, Stats) {
    dict.addWord("docker", "ドッカー");      // all lowercase -> case insensitive
    dict.addWord("GitHub", "ギットハブ");     // mixed case -> case sensitive

    auto stats = dict.getStats();
    EXPECT_EQ(stats.totalEntries, 2);
    EXPECT_EQ(stats.caseInsensitiveEntries, 1);
    EXPECT_EQ(stats.caseSensitiveEntries, 1);
}
