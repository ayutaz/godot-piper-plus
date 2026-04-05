#include <gtest/gtest.h>

#include "language_detector.hpp"
#include "multilingual_phonemize.hpp"

TEST(LanguageDetectorTest, DetectsKana) {
	piper::UnicodeLanguageDetector detector({"ja", "en"});
	EXPECT_TRUE(detector.hasKana("こんにちは"));
	EXPECT_FALSE(detector.hasKana("hello"));
}

TEST(LanguageDetectorTest, SplitsJaEnSegments) {
	piper::UnicodeLanguageDetector detector({"ja", "en"});
	auto segments = detector.segmentText("Helloこんにちはworld");

	ASSERT_EQ(segments.size(), 3u);
	EXPECT_EQ(segments[0].lang, "en");
	EXPECT_EQ(segments[0].text, "Hello");
	EXPECT_EQ(segments[1].lang, "ja");
	EXPECT_EQ(segments[1].text, "こんにちは");
	EXPECT_EQ(segments[2].lang, "en");
	EXPECT_EQ(segments[2].text, "world");
}

TEST(LanguageDetectorTest, DominantLanguagePrefersMajority) {
	piper::UnicodeLanguageDetector detector({"ja", "en"});
	EXPECT_EQ(piper::detectDominantLanguage("Helloこんにちはworld", detector), "en");
}

TEST(MultilingualPhonemizeTest, WrapperUsesJaEnDefaults) {
	auto segments = piper::segmentMultilingualText("abcこんにちはDEF");

	ASSERT_EQ(segments.size(), 3u);
	EXPECT_EQ(segments[0].lang, "en");
	EXPECT_EQ(segments[1].lang, "ja");
	EXPECT_EQ(segments[2].lang, "en");
}
