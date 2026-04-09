#include <gtest/gtest.h>
#include <array>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "piper.hpp"
#include "phoneme_ids.hpp"
#include "piper_test_utils.hpp"

class PiperCore : public ::testing::Test {
protected:
	piper::PhonemeIdConfig createConfig() {
		piper::PhonemeIdConfig config;
		auto map = std::make_shared<piper::PhonemeIdMap>();
		// Simple phoneme ID map for testing
		(*map)[U'a'] = { 3 };
		(*map)[U'i'] = { 4 };
		(*map)[U'u'] = { 5 };
		(*map)[U'e'] = { 6 };
		(*map)[U'o'] = { 7 };
		(*map)[U'k'] = { 8 };
		(*map)[U'N'] = { 9 };
		(*map)[U'_'] = { 10 }; // pause
		// PUA phoneme
		(*map)[(piper::Phoneme)0xE00E] = { 11 }; // ch
		(*map)[(piper::Phoneme)0xE006] = { 12 }; // ky

		config.phonemeIdMap = map;
		config.idPad = 0;
		config.idBos = 1;
		config.idEos = 2;
		config.interspersePad = true;
		config.addBos = true;
		config.addEos = true;
		return config;
	}
};

static int readLe32(const std::array<uint8_t, 44> &header, std::size_t offset) {
	return static_cast<int>(header[offset]) |
		   (static_cast<int>(header[offset + 1]) << 8) |
		   (static_cast<int>(header[offset + 2]) << 16) |
		   (static_cast<int>(header[offset + 3]) << 24);
}

// 1. BasicPhonemeToIds
TEST_F(PiperCore, BasicPhonemeToIds) {
	auto config = createConfig();
	std::vector<piper::Phoneme> phonemes = { U'a', U'i', U'u' };
	std::vector<piper::PhonemeId> ids;
	std::map<piper::Phoneme, std::size_t> missing;

	piper::phonemes_to_ids(phonemes, config, ids, missing);

	EXPECT_TRUE(missing.empty());
	// Expected: BOS, PAD, a_id, PAD, i_id, PAD, u_id, PAD, EOS
	// = 1, 0, 3, 0, 4, 0, 5, 0, 2
	ASSERT_EQ(ids.size(), 9);
	EXPECT_EQ(ids[0], 1); // BOS
	EXPECT_EQ(ids[1], 0); // PAD
	EXPECT_EQ(ids[2], 3); // a
	EXPECT_EQ(ids[3], 0); // PAD
	EXPECT_EQ(ids[4], 4); // i
	EXPECT_EQ(ids[5], 0); // PAD
	EXPECT_EQ(ids[6], 5); // u
	EXPECT_EQ(ids[7], 0); // PAD
	EXPECT_EQ(ids[8], 2); // EOS
}

// 2. PUAPhonemeToIds - PUA codepoints should be looked up in the map
TEST_F(PiperCore, PUAPhonemeToIds) {
	auto config = createConfig();
	std::vector<piper::Phoneme> phonemes = { (piper::Phoneme)0xE00E }; // ch
	std::vector<piper::PhonemeId> ids;
	std::map<piper::Phoneme, std::size_t> missing;

	piper::phonemes_to_ids(phonemes, config, ids, missing);

	EXPECT_TRUE(missing.empty());
	// BOS, PAD, ch_id(11), PAD, EOS
	ASSERT_EQ(ids.size(), 5);
	EXPECT_EQ(ids[2], 11); // ch
}

// 3. MissingPhoneme - phoneme not in map is tracked
TEST_F(PiperCore, MissingPhoneme) {
	auto config = createConfig();
	std::vector<piper::Phoneme> phonemes = { U'z' }; // not in map
	std::vector<piper::PhonemeId> ids;
	std::map<piper::Phoneme, std::size_t> missing;

	piper::phonemes_to_ids(phonemes, config, ids, missing);

	EXPECT_EQ(missing.size(), 1);
	EXPECT_GT(missing[U'z'], 0u);
	// Should still have BOS, PAD, EOS (missing phoneme is skipped)
	ASSERT_EQ(ids.size(), 3); // BOS, PAD, EOS
}

// 4. NoBosEos - test without BOS/EOS
TEST_F(PiperCore, NoBosEos) {
	auto config = createConfig();
	config.addBos = false;
	config.addEos = false;

	std::vector<piper::Phoneme> phonemes = { U'a' };
	std::vector<piper::PhonemeId> ids;
	std::map<piper::Phoneme, std::size_t> missing;

	piper::phonemes_to_ids(phonemes, config, ids, missing);

	// a_id, PAD (only intersperse PAD)
	ASSERT_EQ(ids.size(), 2);
	EXPECT_EQ(ids[0], 3); // a
	EXPECT_EQ(ids[1], 0); // PAD
}

// 5. NoPadIntersperse - test without padding between phonemes
TEST_F(PiperCore, NoPadIntersperse) {
	auto config = createConfig();
	config.interspersePad = false;

	std::vector<piper::Phoneme> phonemes = { U'a', U'i', U'u' };
	std::vector<piper::PhonemeId> ids;
	std::map<piper::Phoneme, std::size_t> missing;

	piper::phonemes_to_ids(phonemes, config, ids, missing);

	// BOS, a, i, u, EOS
	ASSERT_EQ(ids.size(), 5);
	EXPECT_EQ(ids[0], 1); // BOS
	EXPECT_EQ(ids[1], 3); // a
	EXPECT_EQ(ids[2], 4); // i
	EXPECT_EQ(ids[3], 5); // u
	EXPECT_EQ(ids[4], 2); // EOS
}

// 6. EmptyPhonemes
TEST_F(PiperCore, EmptyPhonemes) {
	auto config = createConfig();
	std::vector<piper::Phoneme> phonemes;
	std::vector<piper::PhonemeId> ids;
	std::map<piper::Phoneme, std::size_t> missing;

	piper::phonemes_to_ids(phonemes, config, ids, missing);

	// BOS, PAD, EOS
	ASSERT_EQ(ids.size(), 3);
	EXPECT_EQ(ids[0], 1); // BOS
	EXPECT_EQ(ids[1], 0); // PAD
	EXPECT_EQ(ids[2], 2); // EOS
}

// 7. ClearsOutputVector - phonemes_to_ids should clear ids before adding
TEST_F(PiperCore, ClearsOutputVector) {
	auto config = createConfig();
	std::vector<piper::Phoneme> phonemes = { U'a' };
	std::vector<piper::PhonemeId> ids = { 99, 98, 97 }; // pre-existing data
	std::map<piper::Phoneme, std::size_t> missing;

	piper::phonemes_to_ids(phonemes, config, ids, missing);

	// Should have been cleared and rebuilt
	EXPECT_EQ(ids[0], 1); // BOS, not 99
}

// 8. NullPhonemeIdMap
TEST_F(PiperCore, NullPhonemeIdMap) {
	piper::PhonemeIdConfig config;
	config.phonemeIdMap = nullptr; // null map

	std::vector<piper::Phoneme> phonemes = { U'a' };
	std::vector<piper::PhonemeId> ids;
	std::map<piper::Phoneme, std::size_t> missing;

	piper::phonemes_to_ids(phonemes, config, ids, missing);

	// Should still have BOS, PAD, EOS but no phoneme IDs added
	// (the code checks if phonemeIdMap is not null before looking up)
	ASSERT_GE(ids.size(), 2u); // At least BOS and EOS
}

// 9. MultipleIdsPerPhoneme
TEST_F(PiperCore, MultipleIdsPerPhoneme) {
	auto config = createConfig();
	(*config.phonemeIdMap)[U'a'] = {3, 30};

	std::vector<piper::Phoneme> phonemes = {U'a'};
	std::vector<piper::PhonemeId> ids;
	std::map<piper::Phoneme, std::size_t> missing;

	piper::phonemes_to_ids(phonemes, config, ids, missing);

	ASSERT_EQ(ids.size(), 7);
	EXPECT_EQ(ids[2], 3);
	EXPECT_EQ(ids[4], 30);
	EXPECT_EQ(ids[6], 2);
}

// 10. MissingPhonemeCountAccumulates
TEST_F(PiperCore, MissingPhonemeCountAccumulates) {
	auto config = createConfig();
	std::vector<piper::Phoneme> phonemes = {U'z', U'z', U'y'};
	std::vector<piper::PhonemeId> ids;
	std::map<piper::Phoneme, std::size_t> missing;

	piper::phonemes_to_ids(phonemes, config, ids, missing);

	EXPECT_EQ(missing.size(), 2u);
	EXPECT_EQ(missing[U'z'], 2u);
	EXPECT_EQ(missing[U'y'], 1u);
}

// 11. PausePhonemeMapping
TEST_F(PiperCore, PausePhonemeMapping) {
	auto config = createConfig();
	std::vector<piper::Phoneme> phonemes = {U'_'};
	std::vector<piper::PhonemeId> ids;
	std::map<piper::Phoneme, std::size_t> missing;

	piper::phonemes_to_ids(phonemes, config, ids, missing);

	EXPECT_TRUE(missing.empty());
	ASSERT_EQ(ids.size(), 5);
	EXPECT_EQ(ids[2], 10);
}

// 12. SampleRateValidation
TEST_F(PiperCore, SampleRateValidation) {
	for (int sampleRate : {16000, 22050, 24000, 44100, 48000}) {
		json config = {
			{"audio", {{"sample_rate", sampleRate}}}
		};
		piper::SynthesisConfig synthesisConfig;
		piper::parseSynthesisConfig(config, synthesisConfig);
		EXPECT_EQ(synthesisConfig.sampleRate, sampleRate);
	}
}

// 13. Int16Range
TEST_F(PiperCore, Int16Range) {
	const std::vector<float> audio = {-2.0f, -0.5f, 0.0f, 0.5f, 2.0f};
	std::vector<int16_t> pcm;

	piper::scaleAudioToInt16(audio.data(), audio.size(), pcm);

	ASSERT_EQ(pcm.size(), audio.size());
	for (int16_t sample : pcm) {
		EXPECT_GE(sample, std::numeric_limits<int16_t>::min());
		EXPECT_LE(sample, std::numeric_limits<int16_t>::max());
	}
	EXPECT_EQ(pcm.front(), -32767);
	EXPECT_EQ(pcm.back(), std::numeric_limits<int16_t>::max());
}

// 14. WAVHeaderStructure
TEST_F(PiperCore, WAVHeaderStructure) {
	auto header = piper::createWavHeader(22050, 22050, 1, 2);

	ASSERT_EQ(header.size(), 44u);
	EXPECT_EQ(std::string(reinterpret_cast<const char *>(header.data()), 4), "RIFF");
	EXPECT_EQ(std::string(reinterpret_cast<const char *>(header.data() + 8), 4), "WAVE");
	EXPECT_EQ(std::string(reinterpret_cast<const char *>(header.data() + 12), 4), "fmt ");
	EXPECT_EQ(std::string(reinterpret_cast<const char *>(header.data() + 36), 4), "data");
	EXPECT_EQ(readLe32(header, 24), 22050);
	EXPECT_EQ(readLe32(header, 40), 44100);
}

// 15. EmptyStringHandling
TEST_F(PiperCore, EmptyStringHandling) {
	std::vector<float> durations;
	std::vector<piper::PhonemeId> ids;
	piper::PhonemeIdMap idMap;

	auto timings = piper::extractTimingsFromDurations(
		durations, ids, idMap, 256, 22050, piper::TextPhonemes);

	EXPECT_TRUE(timings.empty());
}

// 16. UTF8Support
TEST_F(PiperCore, UTF8Support) {
	EXPECT_TRUE(piper::isSingleCodepoint(u8"あ"));
	EXPECT_EQ(piper::getCodepoint(u8"あ"), U'あ');

	json config = {
		{"phoneme_id_map", {
			{u8"あ", json::array({42})}
		}}
	};
	piper::PhonemizeConfig phonemizeConfig;
	piper::parsePhonemizeConfig(config, phonemizeConfig);

	ASSERT_EQ(phonemizeConfig.phonemeIdMap.size(), 1u);
	EXPECT_EQ(phonemizeConfig.phonemeIdMap[U'あ'][0], 42);
}

// 17. ModelConfigParsing
TEST_F(PiperCore, ModelConfigParsing) {
	json config = {
		{"phoneme_type", "openjtalk"},
		{"phoneme_id_map", {
			{"a", json::array({3})},
			{u8"あ", json::array({4})}
		}},
		{"audio", {{"sample_rate", 24000}}},
		{"num_speakers", 2},
		{"speaker_id_map", {{"alice", 0}, {"bob", 1}}}
	};

	piper::PhonemizeConfig phonemizeConfig;
	piper::SynthesisConfig synthesisConfig;
	piper::ModelConfig modelConfig;

	piper::parsePhonemizeConfig(config, phonemizeConfig);
	piper::parseSynthesisConfig(config, synthesisConfig);
	piper::parseModelConfig(config, modelConfig);

	EXPECT_EQ(phonemizeConfig.phonemeType, piper::OpenJTalkPhonemes);
	EXPECT_FALSE(phonemizeConfig.interspersePad);
	EXPECT_EQ(synthesisConfig.sampleRate, 24000);
	EXPECT_EQ(modelConfig.numSpeakers, 2);
	ASSERT_TRUE(modelConfig.speakerIdMap.has_value());
	EXPECT_EQ(modelConfig.speakerIdMap->at("alice"), 0);
	EXPECT_EQ(modelConfig.speakerIdMap->at("bob"), 1);
}

// 18. PhonemizeConfigBilingualAlias
TEST_F(PiperCore, PhonemizeConfigBilingualAlias) {
	json config = {
		{"phoneme_type", "bilingual"}
	};

	piper::PhonemizeConfig phonemizeConfig;
	piper::parsePhonemizeConfig(config, phonemizeConfig);

	EXPECT_EQ(phonemizeConfig.phonemeType, piper::MultilingualPhonemes);
	EXPECT_TRUE(phonemizeConfig.interspersePad);
}

// 19. ModelConfigLanguageParsing
TEST_F(PiperCore, ModelConfigLanguageParsing) {
	json config = {
		{"num_languages", 3},
		{"language_id_map", {
			{"ja", 0},
			{"en", 1},
			{"zh", 2}
		}}
	};

	piper::ModelConfig modelConfig;
	piper::parseModelConfig(config, modelConfig);

	EXPECT_EQ(modelConfig.numLanguages, 3);
	ASSERT_TRUE(modelConfig.languageIdMap.has_value());
	EXPECT_EQ(modelConfig.languageIdMap->at("ja"), 0);
	EXPECT_EQ(modelConfig.languageIdMap->at("en"), 1);
	EXPECT_EQ(modelConfig.languageIdMap->at("zh"), 2);
}

// 20. ModelConfigLanguageCountInference - infer num_languages from language_id_map
TEST_F(PiperCore, ModelConfigLanguageCountInference) {
	json config = {
		{"language_id_map", {
			{"ja", 0},
			{"en", 1}
		}}
	};

	piper::ModelConfig modelConfig;
	piper::parseModelConfig(config, modelConfig);

	EXPECT_EQ(modelConfig.numLanguages, 2);
	ASSERT_TRUE(modelConfig.languageIdMap.has_value());
	EXPECT_EQ(modelConfig.languageIdMap->size(), 2u);
}

TEST_F(PiperCore, ParseJsonConfigFromStringSuccess) {
	json config;
	std::string error_message;
	const std::string json_text = R"({
		"audio": {
			"sample_rate": 24000
		},
		"num_languages": 2
	})";

	EXPECT_TRUE(piper::parseJsonConfigFromString(json_text, config, &error_message));
	EXPECT_TRUE(error_message.empty());
	EXPECT_TRUE(config.is_object());
	EXPECT_EQ(config["audio"]["sample_rate"], 24000);
	EXPECT_EQ(config["num_languages"], 2);
}

TEST_F(PiperCore, ParseJsonConfigFromStringFailure) {
	json config;
	std::string error_message;

	EXPECT_FALSE(piper::parseJsonConfigFromString("{not-json", config, &error_message));
	EXPECT_FALSE(error_message.empty());
	EXPECT_TRUE(config.is_null());
}
