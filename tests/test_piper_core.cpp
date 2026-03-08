#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <vector>

#include "phoneme_ids.hpp"

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
