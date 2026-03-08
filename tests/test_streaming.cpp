#include <gtest/gtest.h>
#include <string>
#include <vector>

// Helper: split text into sentences by Japanese/English punctuation
// This mirrors the logic used in PiperTTS::synthesize_streaming
// Uses byte-level scanning to handle UTF-8 correctly (std::regex doesn't work with multibyte)
static std::vector<std::string> splitTextIntoSentences(const std::string& text) {
    std::vector<std::string> sentences;
    if (text.empty()) return sentences;

    // Sentence-ending punctuation bytes:
    // ASCII: '.' (0x2E), '!' (0x21), '?' (0x3F)
    // UTF-8 Japanese: 。(E38082) ！(EFBC81) ？(EFBC9F)

    size_t start = 0;
    size_t i = 0;
    while (i < text.size()) {
        bool isSentenceEnd = false;
        size_t endLen = 0;

        unsigned char c = static_cast<unsigned char>(text[i]);

        // Check ASCII punctuation
        if (c == '.' || c == '!' || c == '?') {
            isSentenceEnd = true;
            endLen = 1;
        }
        // Check 3-byte UTF-8 sequences for Japanese punctuation
        else if (i + 2 < text.size()) {
            unsigned char c1 = static_cast<unsigned char>(text[i + 1]);
            unsigned char c2 = static_cast<unsigned char>(text[i + 2]);
            // 。 = E3 80 82
            if (c == 0xE3 && c1 == 0x80 && c2 == 0x82) {
                isSentenceEnd = true;
                endLen = 3;
            }
            // ！ = EF BC 81
            else if (c == 0xEF && c1 == 0xBC && c2 == 0x81) {
                isSentenceEnd = true;
                endLen = 3;
            }
            // ？ = EF BC 9F
            else if (c == 0xEF && c1 == 0xBC && c2 == 0x9F) {
                isSentenceEnd = true;
                endLen = 3;
            }
        }

        if (isSentenceEnd) {
            size_t sentenceEnd = i + endLen;
            std::string sentence = text.substr(start, sentenceEnd - start);
            if (!sentence.empty()) {
                sentences.push_back(sentence);
            }
            start = sentenceEnd;
            i = sentenceEnd;
        } else {
            i++;
        }
    }

    // Remaining text without ending punctuation
    if (start < text.size()) {
        sentences.push_back(text.substr(start));
    }

    return sentences;
}

class StreamingTest : public ::testing::Test {};

// 1. TextChunkingJapanese
TEST_F(StreamingTest, TextChunkingJapanese) {
    auto chunks = splitTextIntoSentences(u8"今日はいい天気です。明日も晴れるでしょう。");
    ASSERT_EQ(chunks.size(), 2u);
    EXPECT_EQ(chunks[0], u8"今日はいい天気です。");
    EXPECT_EQ(chunks[1], u8"明日も晴れるでしょう。");
}

// 2. EmptyTextNoChunks
TEST_F(StreamingTest, EmptyTextNoChunks) {
    auto chunks = splitTextIntoSentences("");
    EXPECT_TRUE(chunks.empty());
}

// 3. SingleSentenceOneChunk - text without punctuation
TEST_F(StreamingTest, SingleSentenceOneChunk) {
    auto chunks = splitTextIntoSentences(u8"こんにちは");
    ASSERT_EQ(chunks.size(), 1u);
    EXPECT_EQ(chunks[0], u8"こんにちは");
}

// 4. EnglishSentences
TEST_F(StreamingTest, EnglishSentences) {
    auto chunks = splitTextIntoSentences("Hello world. How are you? Fine!");
    ASSERT_EQ(chunks.size(), 3u);
    EXPECT_EQ(chunks[0], "Hello world.");
    EXPECT_EQ(chunks[1], " How are you?");
    EXPECT_EQ(chunks[2], " Fine!");
}

// 5. MixedPunctuation
TEST_F(StreamingTest, MixedPunctuation) {
    auto chunks = splitTextIntoSentences(u8"元気ですか？はい！元気です。");
    ASSERT_EQ(chunks.size(), 3u);
    EXPECT_EQ(chunks[0], u8"元気ですか？");
    EXPECT_EQ(chunks[1], u8"はい！");
    EXPECT_EQ(chunks[2], u8"元気です。");
}
