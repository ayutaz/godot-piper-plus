#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "custom_dictionary.hpp"
#include "json.hpp"

using json = nlohmann::json;

class CustomDictionaryTest : public ::testing::Test {
protected:
    piper::CustomDictionary dict;
    std::string tempDir;

    std::string fixturePath(const std::string& fileName) const {
        return (std::filesystem::path(__FILE__).parent_path() / "fixtures" / fileName).string();
    }

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
    EXPECT_FALSE(dict.getPronunciation("github").has_value());
}

// 9. CaseInsensitiveLookup - all lowercase words are case-insensitive
TEST_F(CustomDictionaryTest, CaseInsensitiveLookup) {
    dict.addWord("docker", "ドッカー");  // all lowercase -> case insensitive
    auto pron = dict.getPronunciation("docker");
    ASSERT_TRUE(pron.has_value());
    EXPECT_EQ(pron.value(), "ドッカー");
    auto upperPron = dict.getPronunciation("DOCKER");
    ASSERT_TRUE(upperPron.has_value());
    EXPECT_EQ(upperPron.value(), "ドッカー");
}

// 10. LoadV1Format - simple JSON dict
TEST_F(CustomDictionaryTest, LoadV1Format) {
    dict.loadDictionary(fixturePath("test_dictionary_v1.json"));
    auto pron = dict.getPronunciation("kubernetes");
    ASSERT_TRUE(pron.has_value());
    EXPECT_EQ(pron.value(), "クーバネティス");
}

// 11. LoadV2Format - dict with version and priority
TEST_F(CustomDictionaryTest, LoadV2Format) {
    dict.loadDictionary(fixturePath("test_dictionary_v2.json"));
    auto pron = dict.getPronunciation("terraform");
    ASSERT_TRUE(pron.has_value());
    EXPECT_EQ(pron.value(), "テラフォーム");
    auto pron2 = dict.getPronunciation("ansible");
    ASSERT_TRUE(pron2.has_value());
    EXPECT_EQ(pron2.value(), "アンシブル");
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

// 15. LongestMatchFirst - longer entries should be replaced before shorter ones
TEST_F(CustomDictionaryTest, LongestMatchFirst) {
    dict.addWord("docker", "ドッカー");
    dict.addWord("docker compose", "ドッカーコンポーズ");

    std::string result = dict.applyToText("docker compose is built on docker");
    EXPECT_NE(result.find("ドッカーコンポーズ"), std::string::npos);
    EXPECT_NE(result.find("ドッカー"), std::string::npos);
    EXPECT_EQ(result.find("docker compose"), std::string::npos);
}

// 16. EmptyDictionary - applying an empty dictionary should be a no-op
TEST_F(CustomDictionaryTest, EmptyDictionary) {
    const std::string input = "plain text stays unchanged";
    EXPECT_EQ(dict.applyToText(input), input);

    auto stats = dict.getStats();
    EXPECT_EQ(stats.totalEntries, 0);
}

// 17. SpecialCharacters - entries with regex meta characters should still match
TEST_F(CustomDictionaryTest, SpecialCharacters) {
    dict.addWord("C++", "シープラスプラス");
    dict.addWord("@user", "アットユーザー");

    std::string result = dict.applyToText("C++ mentions @user in the thread");
    EXPECT_NE(result.find("シープラスプラス"), std::string::npos);
    EXPECT_NE(result.find("アットユーザー"), std::string::npos);
    EXPECT_EQ(result.find("C++"), std::string::npos);
    EXPECT_EQ(result.find("@user"), std::string::npos);
}

// 18. SaveUsesRuntimeV2Format - saved files should use the runtime V2 shape
TEST_F(CustomDictionaryTest, SaveUsesRuntimeV2Format) {
    dict.addWord("docker", "ドッカー");

    std::string outFile = tempDir + "/saved_v2_dict.json";
    dict.saveDictionary(outFile);

    std::ifstream file(outFile);
    ASSERT_TRUE(file.is_open());

    nlohmann::json root = nlohmann::json::parse(file);
    ASSERT_TRUE(root.contains("version"));
    EXPECT_EQ(root["version"].get<std::string>(), "2.0");
    ASSERT_TRUE(root.contains("entries"));
    EXPECT_TRUE(root["entries"].is_object());
    EXPECT_EQ(root["entries"]["docker"]["pronunciation"].get<std::string>(), "ドッカー");
    EXPECT_EQ(root["entries"]["docker"]["priority"].get<int>(), 5);
}

// 19. LoadLegacyEditorFormat - old Godot editor format should still load
TEST_F(CustomDictionaryTest, LoadLegacyEditorFormat) {
    dict.loadDictionary(fixturePath("test_dictionary_editor_legacy.json"));

    auto pron = dict.getPronunciation("docker");
    ASSERT_TRUE(pron.has_value());
    EXPECT_EQ(pron.value(), "ドッカー");

    auto pron2 = dict.getPronunciation("GitHub");
    ASSERT_TRUE(pron2.has_value());
    EXPECT_EQ(pron2.value(), "ギットハブ");
}

// 20. ApplyToTextSegmentsPreservesInlinePhonemes - only normal text should be replaced
TEST_F(CustomDictionaryTest, ApplyToTextSegmentsPreservesInlinePhonemes) {
    dict.addWord("docker", "ドッカー");

    std::string result = piper::applyCustomDictionaryToTextSegments(
        "docker [[ d o k a ]] docker", &dict);

    EXPECT_EQ(result, "ドッカー [[ d o k a ]] ドッカー");
}
