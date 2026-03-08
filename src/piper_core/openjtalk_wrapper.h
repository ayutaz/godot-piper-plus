#ifndef OPENJTALK_WRAPPER_H_
#define OPENJTALK_WRAPPER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

// Prosody result structure for phonemes with A1/A2/A3 values
typedef struct {
    char* phonemes;         // Space-separated phonemes
    int* prosody_a1;        // A1 values for each phoneme
    int* prosody_a2;        // A2 values for each phoneme
    int* prosody_a3;        // A3 values for each phoneme
    int count;              // Number of phonemes
} OpenJTalkProsodyResult;

// Check if OpenJTalk C API is available (always true with static linking)
int openjtalk_is_available(void);

// Ensure OpenJTalk dictionary is available
int openjtalk_ensure_dictionary(void);

// Set a custom dictionary path for OpenJTalk
// If set, this path will be used instead of the default dictionary path.
// Pass NULL to revert to default behavior.
void openjtalk_set_dictionary_path(const char* path);

// Convert text to phonemes using OpenJTalk C API (direct, no external binary)
char* openjtalk_text_to_phonemes(const char* text);

// Free phoneme string returned by openjtalk_text_to_phonemes
void openjtalk_free_phonemes(char* phonemes);

// Convert text to phonemes with prosody features (A1/A2/A3) using OpenJTalk C API
OpenJTalkProsodyResult* openjtalk_text_to_phonemes_with_prosody(const char* text);

// Free prosody result
void openjtalk_free_prosody_result(OpenJTalkProsodyResult* result);

#ifdef __cplusplus
}
#endif

#endif // OPENJTALK_WRAPPER_H_
