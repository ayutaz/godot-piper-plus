// OpenJTalk wrapper - uses C API direct calls (no system()/popen())
// This replaces the previous implementation that used external binary execution.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "openjtalk_wrapper.h"
#include "openjtalk_api.h"
#include "openjtalk_dictionary_manager.h"
#include "openjtalk_error.h"
#include "openjtalk_security.h"

// Constants - Size limits
#define OPENJTALK_MAX_INPUT (1024 * 1024)  // 1MB limit
#define OPENJTALK_MAX_BUFFER 4096

// Thread-safe custom dictionary path storage
#ifdef _WIN32
static char g_custom_dict_path[1024] = {0};
static CRITICAL_SECTION g_dict_path_mutex;
static BOOL g_dict_mutex_initialized = FALSE;

static void ensure_dict_mutex_initialized(void) {
    if (!g_dict_mutex_initialized) {
        InitializeCriticalSection(&g_dict_path_mutex);
        g_dict_mutex_initialized = TRUE;
    }
}
#else
static char g_custom_dict_path[1024] = {0};
static pthread_mutex_t g_dict_path_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

// Get the effective dictionary path (custom or default)
static const char* get_effective_dictionary_path(void) {
    const char* path = NULL;

#ifdef _WIN32
    ensure_dict_mutex_initialized();
    EnterCriticalSection(&g_dict_path_mutex);
    if (g_custom_dict_path[0] != '\0') {
        path = g_custom_dict_path;
    }
    LeaveCriticalSection(&g_dict_path_mutex);
#else
    pthread_mutex_lock(&g_dict_path_mutex);
    if (g_custom_dict_path[0] != '\0') {
        path = g_custom_dict_path;
    }
    pthread_mutex_unlock(&g_dict_path_mutex);
#endif

    if (path) {
        return path;
    }

    // Fall back to default dictionary path
    return get_openjtalk_dictionary_path();
}

// Set a custom dictionary path
void openjtalk_set_dictionary_path(const char* path) {
#ifdef _WIN32
    ensure_dict_mutex_initialized();
    EnterCriticalSection(&g_dict_path_mutex);
    if (path) {
        strncpy(g_custom_dict_path, path, sizeof(g_custom_dict_path) - 1);
        g_custom_dict_path[sizeof(g_custom_dict_path) - 1] = '\0';
    } else {
        g_custom_dict_path[0] = '\0';
    }
    LeaveCriticalSection(&g_dict_path_mutex);
#else
    pthread_mutex_lock(&g_dict_path_mutex);
    if (path) {
        strncpy(g_custom_dict_path, path, sizeof(g_custom_dict_path) - 1);
        g_custom_dict_path[sizeof(g_custom_dict_path) - 1] = '\0';
    } else {
        g_custom_dict_path[0] = '\0';
    }
    pthread_mutex_unlock(&g_dict_path_mutex);
#endif
}

// Check if OpenJTalk is available (always true with static linking)
int openjtalk_is_available(void) {
    return 1;
}

// Ensure OpenJTalk dictionary is available
int openjtalk_ensure_dictionary(void) {
    return ensure_openjtalk_dictionary() == 0 ? 1 : 0;
}

// Convert text to phonemes using OpenJTalk C API
char* openjtalk_text_to_phonemes(const char* text) {
    OpenJTalkResult result = {OPENJTALK_SUCCESS, ""};

    // Validate input using security module
    if (!text) {
        openjtalk_set_result(&result, OPENJTALK_ERROR_NULL_INPUT, "Input text is NULL");
        fprintf(stderr, "Error: %s\n", result.message);
        return NULL;
    }

    if (strlen(text) == 0) {
        openjtalk_set_result(&result, OPENJTALK_ERROR_EMPTY_INPUT, "Input text is empty");
        fprintf(stderr, "Error: %s\n", result.message);
        return NULL;
    }

    size_t text_len = strlen(text);
    if (text_len > OPENJTALK_MAX_INPUT) {
        openjtalk_set_result(&result, OPENJTALK_ERROR_INPUT_TOO_LARGE,
                            "Input text too large: %zu bytes (max %d bytes)",
                            text_len, OPENJTALK_MAX_INPUT);
        fprintf(stderr, "Error: %s\n", result.message);
        return NULL;
    }

    // Get dictionary path
    const char* dic_path = get_effective_dictionary_path();
    if (!dic_path) {
        openjtalk_set_result(&result, OPENJTALK_ERROR_DICTIONARY_NOT_FOUND,
                            "Failed to get OpenJTalk dictionary path");
        fprintf(stderr, "Error: %s\n", result.message);
        return NULL;
    }

    // Initialize OpenJTalk with C API
    OpenJTalk* oj = openjtalk_initialize_with_dict(dic_path);
    if (!oj) {
        openjtalk_set_result(&result, OPENJTALK_ERROR_DICTIONARY_NOT_FOUND,
                            "Failed to initialize OpenJTalk (dictionary: %s)", dic_path);
        fprintf(stderr, "Error: %s\n", result.message);
        return NULL;
    }

    // Extract full context labels
    HTS_Label* label = openjtalk_extract_fullcontext(oj, text);
    if (!label) {
        fprintf(stderr, "Error: Failed to extract full context labels\n");
        openjtalk_finalize(oj);
        return NULL;
    }

    // Allocate buffer for phonemes
    size_t phoneme_buffer_size = OPENJTALK_MAX_BUFFER;
    char* phonemes = (char*)malloc(phoneme_buffer_size);
    if (!phonemes) {
        HTS_Label_clear(label);
        openjtalk_finalize(oj);
        return NULL;
    }

    phonemes[0] = '\0';
    size_t total_phoneme_len = 0;

    // Extract phonemes from full-context labels
    size_t label_size = HTS_Label_get_size(label);
    for (size_t i = 0; i < label_size; i++) {
        const char* label_str = HTS_Label_get_string(label, i);
        if (!label_str) continue;

        // Skip silence at beginning and end
        if (i == 0 || i == label_size - 1) {
            if (strstr(label_str, "-sil+")) continue;
        }

        // Extract phoneme from full-context label
        // Format: xx^xx-phoneme+xx=xx/A:...
        const char* minus_pos = strchr(label_str, '-');
        if (minus_pos) {
            const char* plus_pos = strchr(minus_pos + 1, '+');
            if (plus_pos && plus_pos > minus_pos + 1) {
                size_t phoneme_len = (size_t)(plus_pos - minus_pos - 1);
                if (phoneme_len > 0 && phoneme_len < 32) {
                    // Check buffer capacity
                    size_t space_needed = (total_phoneme_len > 0 ? 1 : 0) + phoneme_len + 1;
                    if (total_phoneme_len + space_needed > phoneme_buffer_size - 1) {
                        // Check for potential overflow
                        if (phoneme_buffer_size > ((size_t)-1) / 2) {
                            fprintf(stderr, "Error: Buffer size would overflow\n");
                            free(phonemes);
                            HTS_Label_clear(label);
                            openjtalk_finalize(oj);
                            return NULL;
                        }
                        size_t new_size = phoneme_buffer_size * 2;
                        char* new_phonemes = (char*)realloc(phonemes, new_size);
                        if (!new_phonemes) {
                            free(phonemes);
                            HTS_Label_clear(label);
                            openjtalk_finalize(oj);
                            return NULL;
                        }
                        phonemes = new_phonemes;
                        phoneme_buffer_size = new_size;
                    }

                    // Add space if not first phoneme
                    if (total_phoneme_len > 0) {
                        phonemes[total_phoneme_len++] = ' ';
                    }

                    // Copy phoneme
                    memcpy(phonemes + total_phoneme_len, minus_pos + 1, phoneme_len);
                    total_phoneme_len += phoneme_len;
                    phonemes[total_phoneme_len] = '\0';
                }
            }
        }
    }

    // Clean up
    HTS_Label_clear(label);
    openjtalk_finalize(oj);

    if (total_phoneme_len == 0) {
        free(phonemes);
        return NULL;
    }

    return phonemes;
}

// Free phoneme string
void openjtalk_free_phonemes(char* phonemes) {
    if (phonemes) {
        free(phonemes);
    }
}

// Convert text to phonemes with prosody features using OpenJTalk C API
OpenJTalkProsodyResult* openjtalk_text_to_phonemes_with_prosody(const char* text) {
    // Validate input
    if (!text || strlen(text) == 0) {
        fprintf(stderr, "Error: Invalid input text\n");
        return NULL;
    }

    size_t text_len = strlen(text);
    if (text_len > OPENJTALK_MAX_INPUT) {
        fprintf(stderr, "Error: Input text too large\n");
        return NULL;
    }

    // Get dictionary path
    const char* dic_path = get_effective_dictionary_path();
    if (!dic_path) {
        fprintf(stderr, "Error: Failed to get OpenJTalk dictionary path\n");
        return NULL;
    }

    // Initialize OpenJTalk with C API
    OpenJTalk* oj = openjtalk_initialize_with_dict(dic_path);
    if (!oj) {
        fprintf(stderr, "Error: Failed to initialize OpenJTalk (dictionary: %s)\n", dic_path);
        return NULL;
    }

    // Extract full context labels
    HTS_Label* label = openjtalk_extract_fullcontext(oj, text);
    if (!label) {
        fprintf(stderr, "Error: Failed to extract full context labels\n");
        openjtalk_finalize(oj);
        return NULL;
    }

    size_t label_size = HTS_Label_get_size(label);

    // Allocate result structure
    OpenJTalkProsodyResult* prosody_result =
        (OpenJTalkProsodyResult*)malloc(sizeof(OpenJTalkProsodyResult));
    if (!prosody_result) {
        HTS_Label_clear(label);
        openjtalk_finalize(oj);
        return NULL;
    }

    // Allocate arrays for prosody values
    prosody_result->phonemes = (char*)malloc(OPENJTALK_MAX_BUFFER);
    prosody_result->prosody_a1 = (int*)malloc(sizeof(int) * (label_size + 1));
    prosody_result->prosody_a2 = (int*)malloc(sizeof(int) * (label_size + 1));
    prosody_result->prosody_a3 = (int*)malloc(sizeof(int) * (label_size + 1));
    prosody_result->count = 0;

    if (!prosody_result->phonemes || !prosody_result->prosody_a1 ||
        !prosody_result->prosody_a2 || !prosody_result->prosody_a3) {
        openjtalk_free_prosody_result(prosody_result);
        HTS_Label_clear(label);
        openjtalk_finalize(oj);
        return NULL;
    }

    prosody_result->phonemes[0] = '\0';
    size_t total_phoneme_len = 0;

    // Extract phonemes and prosody from full-context labels
    for (size_t i = 0; i < label_size; i++) {
        const char* label_str = HTS_Label_get_string(label, i);
        if (!label_str || strlen(label_str) == 0) continue;

        // Extract phoneme from: xx^xx-phoneme+xx=xx/A:a1+a2+a3/B:...
        const char* minus_pos = strchr(label_str, '-');
        if (!minus_pos) continue;

        const char* plus_pos = strchr(minus_pos + 1, '+');
        if (!plus_pos || plus_pos <= minus_pos + 1) continue;

        // Extract phoneme
        size_t phoneme_len = (size_t)(plus_pos - minus_pos - 1);
        if (phoneme_len == 0 || phoneme_len >= 32) continue;

        char phoneme[32];
        memcpy(phoneme, minus_pos + 1, phoneme_len);
        phoneme[phoneme_len] = '\0';

        // Extract A1/A2/A3 from /A:a1+a2+a3/
        int a1 = 0, a2 = 0, a3 = 0;
        const char* a_marker = strstr(label_str, "/A:");
        if (a_marker) {
            const char* a1_start = a_marker + 3;
            const char* a1_end = strchr(a1_start, '+');
            if (a1_end) {
                a1 = (int)strtol(a1_start, NULL, 10);

                const char* a2_start = a1_end + 1;
                const char* a2_end = strchr(a2_start, '+');
                if (a2_end) {
                    a2 = (int)strtol(a2_start, NULL, 10);

                    const char* a3_start = a2_end + 1;
                    const char* a3_end = strchr(a3_start, '/');
                    if (a3_end) {
                        a3 = (int)strtol(a3_start, NULL, 10);
                    }
                }
            }
        }

        // Add phoneme to result
        size_t space_needed = (total_phoneme_len > 0 ? 1 : 0) + strlen(phoneme) + 1;
        if (total_phoneme_len + space_needed < OPENJTALK_MAX_BUFFER - 1) {
            if (total_phoneme_len > 0) {
                prosody_result->phonemes[total_phoneme_len] = ' ';
                total_phoneme_len++;
            }
            memcpy(prosody_result->phonemes + total_phoneme_len, phoneme, strlen(phoneme));
            total_phoneme_len += strlen(phoneme);
            prosody_result->phonemes[total_phoneme_len] = '\0';

            // Store prosody values
            int idx = prosody_result->count;
            prosody_result->prosody_a1[idx] = a1;
            prosody_result->prosody_a2[idx] = a2;
            prosody_result->prosody_a3[idx] = a3;
            prosody_result->count++;
        }
    }

    // Clean up
    HTS_Label_clear(label);
    openjtalk_finalize(oj);

    if (prosody_result->count == 0) {
        openjtalk_free_prosody_result(prosody_result);
        return NULL;
    }

    return prosody_result;
}

// Free prosody result
void openjtalk_free_prosody_result(OpenJTalkProsodyResult* result) {
    if (result) {
        if (result->phonemes) free(result->phonemes);
        if (result->prosody_a1) free(result->prosody_a1);
        if (result->prosody_a2) free(result->prosody_a2);
        if (result->prosody_a3) free(result->prosody_a3);
        free(result);
    }
}
