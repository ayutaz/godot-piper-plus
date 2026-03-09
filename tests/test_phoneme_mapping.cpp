#include <gtest/gtest.h>
#include "phoneme_parser.hpp"

class PhonemeMapping : public ::testing::Test {};

// 1. BasicPhonemeMapping - single char phonemes (a, i, u, e, o)
TEST_F(PhonemeMapping, BasicPhonemeMapping) {
    auto result = piper::parsePhonemeString("a i u e o", piper::PHONEME_TYPE_OPENJTALK);
    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], U'a');
    EXPECT_EQ(result[1], U'i');
    EXPECT_EQ(result[2], U'u');
    EXPECT_EQ(result[3], U'e');
    EXPECT_EQ(result[4], U'o');
}

// 2. MultiCharPUA - ky, ch, ts, sh mappings
TEST_F(PhonemeMapping, MultiCharPUA) {
    auto result = piper::parsePhonemeString("ky ch ts sh", piper::PHONEME_TYPE_OPENJTALK);
    ASSERT_EQ(result.size(), 4);
    EXPECT_EQ(result[0], (piper::Phoneme)0xE006); // ky
    EXPECT_EQ(result[1], (piper::Phoneme)0xE00E); // ch
    EXPECT_EQ(result[2], (piper::Phoneme)0xE00F); // ts
    EXPECT_EQ(result[3], (piper::Phoneme)0xE010); // sh
}

// 3. LongVowelPUA - a:, i:, u:, e:, o:
TEST_F(PhonemeMapping, LongVowelPUA) {
    auto result = piper::parsePhonemeString("a: i: u: e: o:", piper::PHONEME_TYPE_OPENJTALK);
    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], (piper::Phoneme)0xE000);
    EXPECT_EQ(result[1], (piper::Phoneme)0xE001);
    EXPECT_EQ(result[2], (piper::Phoneme)0xE002);
    EXPECT_EQ(result[3], (piper::Phoneme)0xE003);
    EXPECT_EQ(result[4], (piper::Phoneme)0xE004);
}

// 4. NVariantBilabial - N_m before m/b/p
TEST_F(PhonemeMapping, NVariantBilabial) {
    auto result = piper::parsePhonemeString("N_m", piper::PHONEME_TYPE_OPENJTALK);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], (piper::Phoneme)0xE019);
}

// 5. NVariantAlveolar - N_n
TEST_F(PhonemeMapping, NVariantAlveolar) {
    auto result = piper::parsePhonemeString("N_n", piper::PHONEME_TYPE_OPENJTALK);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], (piper::Phoneme)0xE01A);
}

// 6. NVariantVelar - N_ng
TEST_F(PhonemeMapping, NVariantVelar) {
    auto result = piper::parsePhonemeString("N_ng", piper::PHONEME_TYPE_OPENJTALK);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], (piper::Phoneme)0xE01B);
}

// 7. NVariantUvular - N_uvular
TEST_F(PhonemeMapping, NVariantUvular) {
    auto result = piper::parsePhonemeString("N_uvular", piper::PHONEME_TYPE_OPENJTALK);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], (piper::Phoneme)0xE01C);
}

// 8. SpecialConsonantCl - cl (sokuon)
TEST_F(PhonemeMapping, SpecialConsonantCl) {
    auto result = piper::parsePhonemeString("cl", piper::PHONEME_TYPE_OPENJTALK);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], (piper::Phoneme)0xE005);
}

// 8b. SmallTsuHandling - both cl and q should map to the same sokuon PUA
TEST_F(PhonemeMapping, SmallTsuHandling) {
    auto result = piper::parsePhonemeString("cl q", piper::PHONEME_TYPE_OPENJTALK);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], (piper::Phoneme)0xE005);
    EXPECT_EQ(result[1], (piper::Phoneme)0xE005);
}

// 9. AllPalatalizedConsonants - test all palatalized consonants
TEST_F(PhonemeMapping, AllPalatalizedConsonants) {
    auto result = piper::parsePhonemeString("ky kw gy gw ty dy py by zy hy ny my ry", piper::PHONEME_TYPE_OPENJTALK);
    ASSERT_EQ(result.size(), 13);
    EXPECT_EQ(result[0], (piper::Phoneme)0xE006);  // ky
    EXPECT_EQ(result[1], (piper::Phoneme)0xE007);  // kw
    EXPECT_EQ(result[2], (piper::Phoneme)0xE008);  // gy
    EXPECT_EQ(result[3], (piper::Phoneme)0xE009);  // gw
    EXPECT_EQ(result[4], (piper::Phoneme)0xE00A);  // ty
    EXPECT_EQ(result[5], (piper::Phoneme)0xE00B);  // dy
    EXPECT_EQ(result[6], (piper::Phoneme)0xE00C);  // py
    EXPECT_EQ(result[7], (piper::Phoneme)0xE00D);  // by
    EXPECT_EQ(result[8], (piper::Phoneme)0xE011);  // zy
    EXPECT_EQ(result[9], (piper::Phoneme)0xE012);  // hy
    EXPECT_EQ(result[10], (piper::Phoneme)0xE013); // ny
    EXPECT_EQ(result[11], (piper::Phoneme)0xE014); // my
    EXPECT_EQ(result[12], (piper::Phoneme)0xE015); // ry
}

// 10. QuestionMarkers
TEST_F(PhonemeMapping, QuestionMarkers) {
    auto r1 = piper::parsePhonemeString("?!", piper::PHONEME_TYPE_OPENJTALK);
    ASSERT_EQ(r1.size(), 1);
    EXPECT_EQ(r1[0], (piper::Phoneme)0xE016);

    auto r2 = piper::parsePhonemeString("?.", piper::PHONEME_TYPE_OPENJTALK);
    ASSERT_EQ(r2.size(), 1);
    EXPECT_EQ(r2[0], (piper::Phoneme)0xE017);

    auto r3 = piper::parsePhonemeString("?~", piper::PHONEME_TYPE_OPENJTALK);
    ASSERT_EQ(r3.size(), 1);
    EXPECT_EQ(r3[0], (piper::Phoneme)0xE018);
}

// 11. EmptyInput - empty string returns empty result
TEST_F(PhonemeMapping, EmptyInput) {
    auto result = piper::parsePhonemeString("", piper::PHONEME_TYPE_OPENJTALK);
    EXPECT_TRUE(result.empty());
}

// 12. MixedSingleAndMultiChar - combined phoneme string
TEST_F(PhonemeMapping, MixedSingleAndMultiChar) {
    // "k o N n i ch i w a" — こんにちは
    auto result = piper::parsePhonemeString("k o N n i ch i w a", piper::PHONEME_TYPE_OPENJTALK);
    ASSERT_EQ(result.size(), 9);
    EXPECT_EQ(result[0], U'k');
    EXPECT_EQ(result[1], U'o');
    EXPECT_EQ(result[2], U'N');
    EXPECT_EQ(result[3], U'n');
    EXPECT_EQ(result[4], U'i');
    EXPECT_EQ(result[5], (piper::Phoneme)0xE00E); // ch
    EXPECT_EQ(result[6], U'i');
    EXPECT_EQ(result[7], U'w');
    EXPECT_EQ(result[8], U'a');
}

// 13. TextPhonemeType - non-OpenJTalk mode
TEST_F(PhonemeMapping, TextPhonemeType) {
    auto result = piper::parsePhonemeString("a b c", piper::PHONEME_TYPE_TEXT);
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], U'a');
    EXPECT_EQ(result[1], U'b');
    EXPECT_EQ(result[2], U'c');
}

// 14. TextPhonemeTypePauseMarker - "pau" and "_" in text mode
TEST_F(PhonemeMapping, TextPhonemeTypePauseMarker) {
    auto result = piper::parsePhonemeString("pau _ a", piper::PHONEME_TYPE_TEXT);
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], U'_'); // pau -> _
    EXPECT_EQ(result[1], U'_');
    EXPECT_EQ(result[2], U'a');
}

// 15. InvalidUTF8 - malformed UTF-8 should be skipped safely in text mode
TEST_F(PhonemeMapping, InvalidUTF8) {
    std::string invalid = std::string("\xE3\x81", 2) + " a";
    std::vector<piper::Phoneme> result;

    EXPECT_NO_THROW(result = piper::parsePhonemeString(invalid, piper::PHONEME_TYPE_TEXT));
    ASSERT_FALSE(result.empty());
    EXPECT_EQ(result.back(), U'a');
}

// 16. BufferOverflowProtection - large inputs should be processed without truncation
TEST_F(PhonemeMapping, BufferOverflowProtection) {
    std::string input;
    input.reserve(20000);
    for (int i = 0; i < 10000; ++i) {
        input += "a ";
    }

    auto result = piper::parsePhonemeString(input, piper::PHONEME_TYPE_OPENJTALK);
    ASSERT_EQ(result.size(), 10000);
    EXPECT_EQ(result.front(), U'a');
    EXPECT_EQ(result.back(), U'a');
}
