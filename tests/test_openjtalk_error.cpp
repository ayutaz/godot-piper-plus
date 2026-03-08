#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "openjtalk_error.h"
}

class OpenJTalkError_ : public ::testing::Test {};

// 1. ErrorToStringSuccess
TEST_F(OpenJTalkError_, ErrorToStringSuccess) {
    EXPECT_STREQ(openjtalk_error_to_string(OPENJTALK_SUCCESS), "Success");
}

// 2. ErrorToStringAllCodes - verify all error codes have string representation
TEST_F(OpenJTalkError_, ErrorToStringAllCodes) {
    // Input validation errors
    EXPECT_STREQ(openjtalk_error_to_string(OPENJTALK_ERROR_NULL_INPUT), "Null input provided");
    EXPECT_STREQ(openjtalk_error_to_string(OPENJTALK_ERROR_EMPTY_INPUT), "Empty input provided");
    EXPECT_STREQ(openjtalk_error_to_string(OPENJTALK_ERROR_INPUT_TOO_LARGE), "Input size exceeds limit");
    EXPECT_STREQ(openjtalk_error_to_string(OPENJTALK_ERROR_INVALID_PATH), "Invalid path characters");

    // Resource errors
    EXPECT_STREQ(openjtalk_error_to_string(OPENJTALK_ERROR_DICTIONARY_NOT_FOUND), "Dictionary not found");
    EXPECT_STREQ(openjtalk_error_to_string(OPENJTALK_ERROR_VOICE_NOT_FOUND), "Voice file not found");
    EXPECT_STREQ(openjtalk_error_to_string(OPENJTALK_ERROR_BINARY_NOT_FOUND), "OpenJTalk binary not found");

    // Memory errors
    EXPECT_STREQ(openjtalk_error_to_string(OPENJTALK_ERROR_MEMORY), "Memory allocation failed");
    EXPECT_STREQ(openjtalk_error_to_string(OPENJTALK_ERROR_BUFFER_TOO_SMALL), "Buffer too small");

    // I/O errors
    EXPECT_STREQ(openjtalk_error_to_string(OPENJTALK_ERROR_IO_READ), "Failed to read file");
    EXPECT_STREQ(openjtalk_error_to_string(OPENJTALK_ERROR_IO_WRITE), "Failed to write file");
    EXPECT_STREQ(openjtalk_error_to_string(OPENJTALK_ERROR_TEMP_FILE), "Temporary file operation failed");

    // Execution errors
    EXPECT_STREQ(openjtalk_error_to_string(OPENJTALK_ERROR_COMMAND_FAILED), "Command execution failed");
    EXPECT_STREQ(openjtalk_error_to_string(OPENJTALK_ERROR_PARSE_OUTPUT), "Failed to parse output");

    // Security errors
    EXPECT_STREQ(openjtalk_error_to_string(OPENJTALK_ERROR_SECURITY), "Security validation failed");

    // Unknown
    EXPECT_STREQ(openjtalk_error_to_string(OPENJTALK_ERROR_UNKNOWN), "Unknown error");
}

// 3. ErrorToStringUnknownCode - invalid code should return "Unknown error"
TEST_F(OpenJTalkError_, ErrorToStringUnknownCode) {
    EXPECT_STREQ(openjtalk_error_to_string((OpenJTalkError)999), "Unknown error");
}

// 4. SetResultWithFormat
TEST_F(OpenJTalkError_, SetResultWithFormat) {
    OpenJTalkResult result;
    openjtalk_set_result(&result, OPENJTALK_ERROR_DICTIONARY_NOT_FOUND, "Dict not found at: %s", "/path/to/dict");

    EXPECT_EQ(result.code, OPENJTALK_ERROR_DICTIONARY_NOT_FOUND);
    EXPECT_STREQ(result.message, "Dict not found at: /path/to/dict");
}

// 5. SetResultWithoutFormat - NULL format uses default string
TEST_F(OpenJTalkError_, SetResultWithoutFormat) {
    OpenJTalkResult result;
    openjtalk_set_result(&result, OPENJTALK_ERROR_MEMORY, NULL);

    EXPECT_EQ(result.code, OPENJTALK_ERROR_MEMORY);
    EXPECT_STREQ(result.message, "Memory allocation failed");
}

// 6. SetResultNullResult - should not crash
TEST_F(OpenJTalkError_, SetResultNullResult) {
    // Should not crash when result is NULL
    openjtalk_set_result(nullptr, OPENJTALK_SUCCESS, "test");
    // If we get here, it didn't crash
    SUCCEED();
}

// 7. SetResultMessageTruncation - message buffer is 256 chars
TEST_F(OpenJTalkError_, SetResultMessageTruncation) {
    OpenJTalkResult result;
    // Create a very long message
    std::string longMsg(300, 'A');
    openjtalk_set_result(&result, OPENJTALK_ERROR_UNKNOWN, "%s", longMsg.c_str());

    // Message should be truncated and null-terminated
    EXPECT_EQ(result.code, OPENJTALK_ERROR_UNKNOWN);
    EXPECT_EQ(strlen(result.message), 255);  // 255 chars + null terminator
    EXPECT_EQ(result.message[255], '\0');
}

// 8. SetResultIntegerFormat
TEST_F(OpenJTalkError_, SetResultIntegerFormat) {
    OpenJTalkResult result;
    openjtalk_set_result(&result, OPENJTALK_ERROR_INPUT_TOO_LARGE, "Input size %d exceeds limit %d", 2000000, 1048576);

    EXPECT_EQ(result.code, OPENJTALK_ERROR_INPUT_TOO_LARGE);
    EXPECT_STREQ(result.message, "Input size 2000000 exceeds limit 1048576");
}
