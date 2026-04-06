#include <gtest/gtest.h>

#include "language_detector.hpp"
#include "multilingual_phonemize.hpp"

namespace {

std::string sentence_to_utf8(const std::vector<piper::Phoneme> &sentence) {
	std::string result;
	for (piper::Phoneme phoneme : sentence) {
		result += piper::phonemeToString(phoneme);
	}
	return result;
}

} // namespace

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

TEST(MultilingualPhonemizeTest, SupportMatrixReflectsExpandedLanguages) {
	EXPECT_EQ(piper::getMultilingualTextRoutingMode("ja"),
			piper::MultilingualTextRoutingMode::AutoDetect);
	EXPECT_EQ(piper::getMultilingualTextRoutingMode("en"),
			piper::MultilingualTextRoutingMode::AutoDetect);
	EXPECT_EQ(piper::getMultilingualTextRoutingMode("es"),
			piper::MultilingualTextRoutingMode::ExplicitOnly);
	EXPECT_EQ(piper::getMultilingualTextRoutingMode("fr"),
			piper::MultilingualTextRoutingMode::ExplicitOnly);
	EXPECT_EQ(piper::getMultilingualTextRoutingMode("pt"),
			piper::MultilingualTextRoutingMode::ExplicitOnly);
	EXPECT_EQ(piper::getMultilingualTextRoutingMode("zh"),
			piper::MultilingualTextRoutingMode::Unsupported);
}

TEST(MultilingualPhonemizeTest, WrapperUsesConfiguredLatinDefaultLanguage) {
	auto segments = piper::segmentMultilingualText(
			"salutこんにちは", {"ja", "en", "fr"}, "fr");

	ASSERT_EQ(segments.size(), 2u);
	EXPECT_EQ(segments[0].lang, "fr");
	EXPECT_EQ(segments[0].text, "salut");
	EXPECT_EQ(segments[1].lang, "ja");
	EXPECT_EQ(segments[1].text, "こんにちは");
}

TEST(MultilingualPhonemizeTest, SpanishRuleBasedPhonemizerProducesPhonemes) {
	std::vector<std::vector<piper::Phoneme>> phonemes;
	piper::phonemize_spanish("hola amigo", phonemes);

	ASSERT_EQ(phonemes.size(), 1u);
	EXPECT_EQ(sentence_to_utf8(phonemes[0]), "ola amigo");
}

TEST(MultilingualPhonemizeTest, FrenchRuleBasedPhonemizerProducesPhonemes) {
	std::vector<std::vector<piper::Phoneme>> phonemes;
	piper::phonemize_french("salut ami", phonemes);

	ASSERT_EQ(phonemes.size(), 1u);
	EXPECT_EQ(sentence_to_utf8(phonemes[0]), "saly ami");
}

TEST(MultilingualPhonemizeTest, PortugueseRuleBasedPhonemizerProducesPhonemes) {
	std::vector<std::vector<piper::Phoneme>> phonemes;
	piper::phonemize_portuguese("olá mundo", phonemes);

	ASSERT_EQ(phonemes.size(), 1u);
	EXPECT_EQ(sentence_to_utf8(phonemes[0]), "ola mundo");
}
