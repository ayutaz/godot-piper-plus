#include <gtest/gtest.h>
#include "phoneme_parser.hpp"

class PhonemeParser : public ::testing::Test {};

// 1. PlainText - no [[ ]] notation
TEST_F(PhonemeParser, PlainText) {
    auto result = piper::parsePhonemeNotation("hello world");
    ASSERT_EQ(result.size(), 1);
    EXPECT_FALSE(result[0].isPhonemes);
    EXPECT_EQ(result[0].text, "hello world");
}

// 2. SingleNotation
TEST_F(PhonemeParser, SingleNotation) {
    auto result = piper::parsePhonemeNotation("test [[ a i u ]] end");
    ASSERT_EQ(result.size(), 3);
    EXPECT_FALSE(result[0].isPhonemes);
    EXPECT_EQ(result[0].text, "test ");
    EXPECT_TRUE(result[1].isPhonemes);
    EXPECT_EQ(result[1].text, "a i u");
    EXPECT_FALSE(result[2].isPhonemes);
    EXPECT_EQ(result[2].text, " end");
}

// 3. MultipleNotations
TEST_F(PhonemeParser, MultipleNotations) {
    auto result = piper::parsePhonemeNotation("before [[ a ]] middle [[ b ]] after");
    ASSERT_EQ(result.size(), 5);
    EXPECT_FALSE(result[0].isPhonemes);
    EXPECT_EQ(result[0].text, "before ");
    EXPECT_TRUE(result[1].isPhonemes);
    EXPECT_EQ(result[1].text, "a");
    EXPECT_FALSE(result[2].isPhonemes);
    EXPECT_EQ(result[2].text, " middle ");
    EXPECT_TRUE(result[3].isPhonemes);
    EXPECT_EQ(result[3].text, "b");
    EXPECT_FALSE(result[4].isPhonemes);
    EXPECT_EQ(result[4].text, " after");
}

// 4. NotationAtStart
TEST_F(PhonemeParser, NotationAtStart) {
    auto result = piper::parsePhonemeNotation("[[ a i ]] text");
    ASSERT_EQ(result.size(), 2);
    EXPECT_TRUE(result[0].isPhonemes);
    EXPECT_EQ(result[0].text, "a i");
    EXPECT_FALSE(result[1].isPhonemes);
    EXPECT_EQ(result[1].text, " text");
}

// 5. NotationAtEnd
TEST_F(PhonemeParser, NotationAtEnd) {
    auto result = piper::parsePhonemeNotation("text [[ a i ]]");
    ASSERT_EQ(result.size(), 2);
    EXPECT_FALSE(result[0].isPhonemes);
    EXPECT_EQ(result[0].text, "text ");
    EXPECT_TRUE(result[1].isPhonemes);
    EXPECT_EQ(result[1].text, "a i");
}

// 6. OnlyNotation
TEST_F(PhonemeParser, OnlyNotation) {
    auto result = piper::parsePhonemeNotation("[[ k o N n i ch i w a ]]");
    ASSERT_EQ(result.size(), 1);
    EXPECT_TRUE(result[0].isPhonemes);
    EXPECT_EQ(result[0].text, "k o N n i ch i w a");
}

// 7. EmptyInput
TEST_F(PhonemeParser, EmptyInput) {
    auto result = piper::parsePhonemeNotation("");
    EXPECT_TRUE(result.empty());
}

// 8. EmptyNotation - [[ ]] with empty content
TEST_F(PhonemeParser, EmptyNotation) {
    auto result = piper::parsePhonemeNotation("[[ ]]");
    ASSERT_EQ(result.size(), 1);
    EXPECT_TRUE(result[0].isPhonemes);
    // After trimming, the text should be empty
    EXPECT_EQ(result[0].text, "");
}

// 9. ExtraSpaces - whitespace inside [[ ]] is trimmed from trailing
TEST_F(PhonemeParser, ExtraSpaces) {
    auto result = piper::parsePhonemeNotation("[[   a  b  c   ]]");
    ASSERT_EQ(result.size(), 1);
    EXPECT_TRUE(result[0].isPhonemes);
    // The regex captures content between [[ and ]], then trailing whitespace is trimmed
    // But leading whitespace is consumed by the \s* in the regex
    // The code trims trailing whitespace with: phonemeStr.erase(phonemeStr.find_last_not_of(" \t\n\r") + 1);
    // So "  a  b  c  " after regex capture becomes "  a  b  c" after trailing trim
    // Actually the regex is: \[\[\s*([^\]]*)\s*\]\]
    // The \s* after [[ consumes leading space, so capture group gets "a  b  c   "
    // Then trailing trim gives "a  b  c"
    EXPECT_EQ(result[0].text, "a  b  c");
}

// 10. ConsecutiveNotations
TEST_F(PhonemeParser, ConsecutiveNotations) {
    auto result = piper::parsePhonemeNotation("[[ a ]][[ b ]]");
    ASSERT_EQ(result.size(), 2);
    EXPECT_TRUE(result[0].isPhonemes);
    EXPECT_EQ(result[0].text, "a");
    EXPECT_TRUE(result[1].isPhonemes);
    EXPECT_EQ(result[1].text, "b");
}
