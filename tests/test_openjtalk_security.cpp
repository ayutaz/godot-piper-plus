#include <gtest/gtest.h>

extern "C" {
#include "openjtalk_security.h"
}

class OpenJTalkSecurity : public ::testing::Test {};

// 1. SafePathValidation
TEST_F(OpenJTalkSecurity, SafePathValidation) {
    EXPECT_EQ(openjtalk_is_safe_path("/usr/local/share/dict"), 1);
    EXPECT_EQ(openjtalk_is_safe_path("/home/user/models/voice.onnx"), 1);
    EXPECT_EQ(openjtalk_is_safe_path("relative/path/to/file"), 1);
}

// 2. RejectPathTraversal
TEST_F(OpenJTalkSecurity, RejectPathTraversal) {
    EXPECT_EQ(openjtalk_is_safe_path("../../../etc/passwd"), 0);
    EXPECT_EQ(openjtalk_is_safe_path("/usr/../etc/shadow"), 0);
    EXPECT_EQ(openjtalk_is_safe_path("path/.."), 0);
}

// 3. RejectCommandInjectionInPath
TEST_F(OpenJTalkSecurity, RejectCommandInjectionInPath) {
    EXPECT_EQ(openjtalk_is_safe_path("/path; rm -rf /"), 0);
    EXPECT_EQ(openjtalk_is_safe_path("/path|cat /etc/passwd"), 0);
    EXPECT_EQ(openjtalk_is_safe_path("/path&malicious"), 0);
    EXPECT_EQ(openjtalk_is_safe_path("/path`whoami`"), 0);
    EXPECT_EQ(openjtalk_is_safe_path("/path${HOME}"), 0);
}

// 4. NullInputPath
TEST_F(OpenJTalkSecurity, NullInputPath) {
    EXPECT_EQ(openjtalk_is_safe_path(nullptr), 0);
}

// 5. EmptyStringPath - empty string is technically safe (no dangerous chars)
TEST_F(OpenJTalkSecurity, EmptyStringPath) {
    EXPECT_EQ(openjtalk_is_safe_path(""), 1);  // No dangerous chars found
}

// 6. RejectControlCharacters
TEST_F(OpenJTalkSecurity, RejectControlCharacters) {
    EXPECT_EQ(openjtalk_is_safe_path("/path/\x01with/control"), 0);
    EXPECT_EQ(openjtalk_is_safe_path("/path/\x7fwith/del"), 0);
}

// 7. AllowParentheses - for "Program Files (x86)" style paths
TEST_F(OpenJTalkSecurity, AllowParentheses) {
    EXPECT_EQ(openjtalk_is_safe_path("/Program Files (x86)/OpenJTalk"), 1);
    EXPECT_EQ(openjtalk_is_safe_path("C:/Program Files (x86)/test"), 1);
}

// 8. ValidateCommandSafe
TEST_F(OpenJTalkSecurity, ValidateCommandSafe) {
    EXPECT_EQ(openjtalk_validate_command("openjtalk"), 1);
    EXPECT_EQ(openjtalk_validate_command("openjtalk -x dict_dir"), 1);
}

// 9. ValidateCommandNull
TEST_F(OpenJTalkSecurity, ValidateCommandNull) {
    EXPECT_EQ(openjtalk_validate_command(nullptr), 0);
}

// 10. RejectCommandChaining
TEST_F(OpenJTalkSecurity, RejectCommandChaining) {
    EXPECT_EQ(openjtalk_validate_command("cmd && evil"), 0);
    EXPECT_EQ(openjtalk_validate_command("cmd || evil"), 0);
    EXPECT_EQ(openjtalk_validate_command("cmd ; evil"), 0);
    EXPECT_EQ(openjtalk_validate_command("cmd | evil"), 0);
}

// 11. RejectSubshellExecution
TEST_F(OpenJTalkSecurity, RejectSubshellExecution) {
    EXPECT_EQ(openjtalk_validate_command("cmd $(evil)"), 0);
    EXPECT_EQ(openjtalk_validate_command("cmd `evil`"), 0);
}

// 12. RejectRedirection
TEST_F(OpenJTalkSecurity, RejectRedirection) {
    EXPECT_EQ(openjtalk_validate_command("cmd > /etc/passwd"), 0);
    EXPECT_EQ(openjtalk_validate_command("cmd < /dev/null"), 0);
    EXPECT_EQ(openjtalk_validate_command("cmd >> /tmp/log"), 0);
}
