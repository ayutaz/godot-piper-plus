// OpenJTalk wrapper - uses the builtin C API by default and can optionally
// delegate phonemization to an openjtalk-native shared library when one is
// available.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__EMSCRIPTEN__)
#define PIPER_PLUS_OPENJTALK_NATIVE_UNSUPPORTED 1
#else
#include <dlfcn.h>
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
#define OPENJTALK_MAX_PATH 1024

typedef struct {
    char* phonemes;
    int* phoneme_ids;
    int phoneme_count;
    float* durations;
    float total_duration;
} OpenJTalkNativePhonemeResult;

typedef struct {
    char* phonemes;
    int* prosody_a1;
    int* prosody_a2;
    int* prosody_a3;
    int phoneme_count;
} OpenJTalkNativeProsodyResult;

#ifdef _WIN32
typedef HMODULE OpenJTalkNativeLibraryHandle;
#else
typedef void* OpenJTalkNativeLibraryHandle;
#endif

typedef struct {
    OpenJTalkNativeLibraryHandle library;
    void* (*create)(const char* dict_path);
    void (*destroy)(void* handle);
    OpenJTalkNativePhonemeResult* (*phonemize)(void* handle, const char* text);
    void (*free_result)(OpenJTalkNativePhonemeResult* result);
    OpenJTalkNativeProsodyResult* (*phonemize_with_prosody)(void* handle, const char* text);
    void (*free_prosody_result)(OpenJTalkNativeProsodyResult* result);
} OpenJTalkNativeApi;

static char g_custom_dict_path[OPENJTALK_MAX_PATH] = {0};
static char g_native_library_path[OPENJTALK_MAX_PATH] = {0};

// Thread-safe custom path storage
#ifdef _WIN32
static CRITICAL_SECTION g_config_mutex;
static BOOL g_config_mutex_initialized = FALSE;

static void ensure_config_mutex_initialized(void) {
    if (!g_config_mutex_initialized) {
        InitializeCriticalSection(&g_config_mutex);
        g_config_mutex_initialized = TRUE;
    }
}

#define CONFIG_MUTEX_LOCK()   \
    do {                      \
        ensure_config_mutex_initialized(); \
        EnterCriticalSection(&g_config_mutex); \
    } while (0)
#define CONFIG_MUTEX_UNLOCK() LeaveCriticalSection(&g_config_mutex)
#elif defined(PIPER_PLUS_OPENJTALK_NATIVE_UNSUPPORTED)
#define CONFIG_MUTEX_LOCK()   do { } while (0)
#define CONFIG_MUTEX_UNLOCK() do { } while (0)
#else
static pthread_mutex_t g_config_mutex = PTHREAD_MUTEX_INITIALIZER;

#define CONFIG_MUTEX_LOCK() pthread_mutex_lock(&g_config_mutex)
#define CONFIG_MUTEX_UNLOCK() pthread_mutex_unlock(&g_config_mutex)
#endif

static OpenJTalkNativeApi g_native_api = {0};
static int g_native_load_attempted = 0;
static int g_native_backend_available = 0;

static void clear_path_buffer(char* buffer, size_t buffer_size) {
    if (buffer && buffer_size > 0) {
        buffer[0] = '\0';
    }
}

static void copy_path_buffer(char* destination, size_t destination_size,
                             const char* source) {
    if (!destination || destination_size == 0) {
        return;
    }

    if (source) {
        strncpy(destination, source, destination_size - 1);
        destination[destination_size - 1] = '\0';
    } else {
        destination[0] = '\0';
    }
}

static void unload_native_api_locked(void) {
#if !defined(PIPER_PLUS_OPENJTALK_NATIVE_UNSUPPORTED)
    if (g_native_api.library) {
#ifdef _WIN32
        FreeLibrary(g_native_api.library);
#else
        dlclose(g_native_api.library);
#endif
    }
#endif

    memset(&g_native_api, 0, sizeof(g_native_api));
    g_native_load_attempted = 0;
    g_native_backend_available = 0;
}

static char* duplicate_c_string(const char* input);

static int load_native_symbol(OpenJTalkNativeLibraryHandle library,
                              const char* symbol_name, void** destination) {
#ifdef PIPER_PLUS_OPENJTALK_NATIVE_UNSUPPORTED
    (void)library;
    (void)symbol_name;
    (void)destination;
    return 0;
#else
#ifdef _WIN32
    FARPROC symbol = GetProcAddress(library, symbol_name);
    if (!symbol) {
        return 0;
    }
    *destination = (void*)symbol;
#else
    void* symbol = dlsym(library, symbol_name);
    if (!symbol) {
        return 0;
    }
    *destination = symbol;
#endif
    return 1;
#endif
}

static int try_load_native_library(const char* library_path) {
#ifdef PIPER_PLUS_OPENJTALK_NATIVE_UNSUPPORTED
    (void)library_path;
    return 0;
#else
    OpenJTalkNativeLibraryHandle library = NULL;

#ifdef _WIN32
    library = LoadLibraryA(library_path);
#else
    library = dlopen(library_path, RTLD_LAZY | RTLD_LOCAL);
#endif
    if (!library) {
        return 0;
    }

    OpenJTalkNativeApi candidate = {0};
    candidate.library = library;

    if (!load_native_symbol(library, "openjtalk_native_create",
                            (void**)&candidate.create) ||
        !load_native_symbol(library, "openjtalk_native_destroy",
                            (void**)&candidate.destroy) ||
        !load_native_symbol(library, "openjtalk_native_phonemize",
                            (void**)&candidate.phonemize) ||
        !load_native_symbol(library, "openjtalk_native_free_result",
                            (void**)&candidate.free_result) ||
        !load_native_symbol(library, "openjtalk_native_phonemize_with_prosody",
                            (void**)&candidate.phonemize_with_prosody) ||
        !load_native_symbol(library, "openjtalk_native_free_prosody_result",
                            (void**)&candidate.free_prosody_result)) {
#ifdef _WIN32
        FreeLibrary(library);
#else
        dlclose(library);
#endif
        return 0;
    }

    g_native_api = candidate;
    g_native_load_attempted = 1;
    g_native_backend_available = 1;
    fprintf(stderr, "OpenJTalk: using openjtalk-native backend (%s)\n",
            library_path);
    return 1;
#endif
}

static int ensure_native_backend_loaded_locked(void) {
#ifdef PIPER_PLUS_OPENJTALK_NATIVE_UNSUPPORTED
    g_native_load_attempted = 1;
    g_native_backend_available = 0;
    return 0;
#else
    char configured_path[OPENJTALK_MAX_PATH];
    char env_path[OPENJTALK_MAX_PATH];
    const char* default_candidates[] = {
#ifdef _WIN32
        "openjtalk_native.dll",
        "addons/piper_plus/bin/openjtalk_native.dll",
#elif defined(__APPLE__)
        "libopenjtalk_native.dylib",
        "addons/piper_plus/bin/libopenjtalk_native.dylib",
#else
        "libopenjtalk_native.so",
        "addons/piper_plus/bin/libopenjtalk_native.so",
#endif
        NULL,
    };

    if (g_native_load_attempted) {
        return g_native_backend_available;
    }

    copy_path_buffer(configured_path, sizeof(configured_path),
                     g_native_library_path[0] != '\0' ? g_native_library_path : NULL);
    copy_path_buffer(env_path, sizeof(env_path),
                     getenv("OPENJTALK_NATIVE_LIBRARY_PATH"));
    g_native_load_attempted = 1;

    if (configured_path[0] != '\0') {
        if (try_load_native_library(configured_path)) {
            return 1;
        }
        fprintf(stderr,
                "OpenJTalk: failed to load openjtalk-native from '%s'; using builtin backend\n",
                configured_path);
    }

    if (env_path[0] != '\0') {
        if (try_load_native_library(env_path)) {
            return 1;
        }
        fprintf(stderr,
                "OpenJTalk: failed to load OPENJTALK_NATIVE_LIBRARY_PATH='%s'; using builtin backend\n",
                env_path);
    }

    for (int index = 0; default_candidates[index] != NULL; ++index) {
        if (try_load_native_library(default_candidates[index])) {
            return 1;
        }
    }

    g_native_backend_available = 0;
    return 0;
#endif
}

static char* snapshot_effective_dictionary_path_locked(void) {
    const char* path = NULL;

    if (g_custom_dict_path[0] != '\0') {
        path = g_custom_dict_path;
    } else {
        path = get_openjtalk_dictionary_path();
    }

    return duplicate_c_string(path);
}

// Set a custom dictionary path
void openjtalk_set_dictionary_path(const char* path) {
    CONFIG_MUTEX_LOCK();
    copy_path_buffer(g_custom_dict_path, sizeof(g_custom_dict_path), path);
    CONFIG_MUTEX_UNLOCK();
}

void openjtalk_set_library_path(const char* path) {
    CONFIG_MUTEX_LOCK();
    copy_path_buffer(g_native_library_path, sizeof(g_native_library_path), path);
    unload_native_api_locked();
    CONFIG_MUTEX_UNLOCK();
}

// Check if OpenJTalk is available (builtin backend is always available)
int openjtalk_is_available(void) {
    return 1;
}

// Ensure OpenJTalk dictionary is available
int openjtalk_ensure_dictionary(void) {
    char* dic_path = NULL;
    int ready = 0;

    CONFIG_MUTEX_LOCK();
    dic_path = snapshot_effective_dictionary_path_locked();
    CONFIG_MUTEX_UNLOCK();

    if (dic_path) {
        ready = openjtalk_dictionary_path_is_ready(dic_path);
    }

    if (ready) {
        free(dic_path);
        return 1;
    }

    fprintf(stderr,
            "OpenJTalk dictionary is not ready at: %s\n"
            "Expected compiled dictionary files: sys.dic, unk.dic, matrix.bin, char.bin\n",
            dic_path ? dic_path : "(null)");
    free(dic_path);
    return 0;
}

static char* duplicate_c_string(const char* input) {
    if (!input) {
        return NULL;
    }

    size_t length = strlen(input);
    char* output = (char*)malloc(length + 1);
    if (!output) {
        return NULL;
    }

    memcpy(output, input, length + 1);
    return output;
}

static int tokenize_phoneme_string(const char* phoneme_string, char*** tokens_out,
                                   int* token_count_out, char** storage_out) {
    char* working_copy = NULL;
    char** tokens = NULL;
    int token_count = 0;
    int token_capacity = 0;
    char* cursor = NULL;

    if (!phoneme_string || !tokens_out || !token_count_out || !storage_out) {
        return 0;
    }

    *tokens_out = NULL;
    *token_count_out = 0;
    *storage_out = NULL;

    working_copy = duplicate_c_string(phoneme_string);
    if (!working_copy) {
        return 0;
    }

    cursor = working_copy;
    while (*cursor != '\0') {
        while (*cursor == ' ') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }

        if (token_count == token_capacity) {
            int next_capacity = token_capacity == 0 ? 8 : token_capacity * 2;
            char** resized_tokens =
                (char**)realloc(tokens, sizeof(char*) * next_capacity);
            if (!resized_tokens) {
                free(tokens);
                free(working_copy);
                return 0;
            }
            tokens = resized_tokens;
            token_capacity = next_capacity;
        }

        tokens[token_count++] = cursor;
        while (*cursor != '\0' && *cursor != ' ') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }

        *cursor = '\0';
        cursor++;
    }

    *tokens_out = tokens;
    *token_count_out = token_count;
    *storage_out = working_copy;
    return 1;
}

static void free_tokenized_phoneme_string(char** tokens, char* storage) {
    if (!storage) {
        return;
    }
    free(tokens);
    free(storage);
}

static int is_boundary_token(const char* token) {
    return token && ((strcmp(token, "pau") == 0) || (strcmp(token, "sil") == 0));
}

static char* normalize_native_phoneme_string(const char* phoneme_string,
                                             int** kept_indices_out,
                                             int* kept_count_out) {
    char** tokens = NULL;
    int token_count = 0;
    char* token_storage = NULL;
    int* kept_indices = NULL;
    int kept_count = 0;
    size_t output_length = 0;
    char* output = NULL;

    if (!tokenize_phoneme_string(phoneme_string, &tokens, &token_count, &token_storage)) {
        return NULL;
    }

    for (int i = 0; i < token_count; ++i) {
        if ((i == 0 || i == token_count - 1) && is_boundary_token(tokens[i])) {
            continue;
        }

        int* resized_indices =
            (int*)realloc(kept_indices, sizeof(int) * (kept_count + 1));
        if (!resized_indices) {
            free(kept_indices);
            free_tokenized_phoneme_string(tokens, token_storage);
            return NULL;
        }
        kept_indices = resized_indices;
        kept_indices[kept_count++] = i;
        output_length += strlen(tokens[i]) + 1;
    }

    if (kept_count == 0) {
        free(kept_indices);
        free_tokenized_phoneme_string(tokens, token_storage);
        return NULL;
    }

    output = (char*)malloc(output_length);
    if (!output) {
        free(kept_indices);
        free_tokenized_phoneme_string(tokens, token_storage);
        return NULL;
    }

    output[0] = '\0';
    for (int i = 0; i < kept_count; ++i) {
        if (i > 0) {
            strcat(output, " ");
        }
        strcat(output, tokens[kept_indices[i]]);
    }

    free_tokenized_phoneme_string(tokens, token_storage);

    if (kept_indices_out && kept_count_out) {
        *kept_indices_out = kept_indices;
        *kept_count_out = kept_count;
    } else {
        free(kept_indices);
    }

    return output;
}

static char* native_text_to_phonemes(const char* text) {
    char* dic_path = NULL;
    void* handle = NULL;
    OpenJTalkNativePhonemeResult* native_result = NULL;
    char* normalized = NULL;

    if (!ensure_native_backend_loaded_locked()) {
        return NULL;
    }

    dic_path = snapshot_effective_dictionary_path_locked();
    if (!dic_path) {
        return NULL;
    }

    handle = g_native_api.create(dic_path);
    if (!handle) {
        free(dic_path);
        return NULL;
    }

    native_result = g_native_api.phonemize(handle, text);
    if (native_result && native_result->phonemes) {
        normalized = normalize_native_phoneme_string(native_result->phonemes, NULL, NULL);
    }

    if (native_result) {
        g_native_api.free_result(native_result);
    }
    g_native_api.destroy(handle);
    free(dic_path);
    return normalized;
}

static OpenJTalkProsodyResult* native_text_to_phonemes_with_prosody(const char* text) {
    char* dic_path = NULL;
    void* handle = NULL;
    OpenJTalkNativeProsodyResult* native_result = NULL;
    OpenJTalkProsodyResult* result = NULL;
    int* kept_indices = NULL;
    int kept_count = 0;

    if (!ensure_native_backend_loaded_locked()) {
        return NULL;
    }

    dic_path = snapshot_effective_dictionary_path_locked();
    if (!dic_path) {
        return NULL;
    }

    handle = g_native_api.create(dic_path);
    if (!handle) {
        free(dic_path);
        return NULL;
    }

    native_result = g_native_api.phonemize_with_prosody(handle, text);
    if (!native_result || !native_result->phonemes) {
        if (native_result) {
            g_native_api.free_prosody_result(native_result);
        }
        g_native_api.destroy(handle);
        free(dic_path);
        return NULL;
    }

    result = (OpenJTalkProsodyResult*)malloc(sizeof(OpenJTalkProsodyResult));
    if (!result) {
        g_native_api.free_prosody_result(native_result);
        g_native_api.destroy(handle);
        free(dic_path);
        return NULL;
    }
    memset(result, 0, sizeof(*result));

    result->phonemes = normalize_native_phoneme_string(native_result->phonemes,
                                                       &kept_indices, &kept_count);
    if (!result->phonemes || kept_count <= 0) {
        free(kept_indices);
        g_native_api.free_prosody_result(native_result);
        g_native_api.destroy(handle);
        openjtalk_free_prosody_result(result);
        free(dic_path);
        return NULL;
    }

    result->prosody_a1 = (int*)malloc(sizeof(int) * kept_count);
    result->prosody_a2 = (int*)malloc(sizeof(int) * kept_count);
    result->prosody_a3 = (int*)malloc(sizeof(int) * kept_count);
    if (!result->prosody_a1 || !result->prosody_a2 || !result->prosody_a3) {
        free(kept_indices);
        g_native_api.free_prosody_result(native_result);
        g_native_api.destroy(handle);
        openjtalk_free_prosody_result(result);
        free(dic_path);
        return NULL;
    }

    for (int i = 0; i < kept_count; ++i) {
        int source_index = kept_indices[i];
        if (source_index >= 0 && source_index < native_result->phoneme_count) {
            result->prosody_a1[i] = native_result->prosody_a1[source_index];
            result->prosody_a2[i] = native_result->prosody_a2[source_index];
            result->prosody_a3[i] = native_result->prosody_a3[source_index];
        } else {
            result->prosody_a1[i] = 0;
            result->prosody_a2[i] = 0;
            result->prosody_a3[i] = 0;
        }
    }
    result->count = kept_count;

    free(kept_indices);
    g_native_api.free_prosody_result(native_result);
    g_native_api.destroy(handle);
    free(dic_path);
    return result;
}

static char* builtin_text_to_phonemes(const char* text) {
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
    char* dic_path = snapshot_effective_dictionary_path_locked();
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
        free(dic_path);
        return NULL;
    }

    // Extract full context labels
    HTS_Label* label = openjtalk_extract_fullcontext(oj, text);
    if (!label) {
        fprintf(stderr, "Error: Failed to extract full context labels\n");
        openjtalk_finalize(oj);
        free(dic_path);
        return NULL;
    }

    // Allocate buffer for phonemes
    size_t phoneme_buffer_size = OPENJTALK_MAX_BUFFER;
    char* phonemes = (char*)malloc(phoneme_buffer_size);
    if (!phonemes) {
        openjtalk_label_clear(label);
        openjtalk_finalize(oj);
        free(dic_path);
        return NULL;
    }

    phonemes[0] = '\0';
    size_t total_phoneme_len = 0;

    // Extract phonemes from full-context labels
    size_t label_size = openjtalk_label_get_size(label);
    for (size_t i = 0; i < label_size; i++) {
        const char* label_str = openjtalk_label_get_string(label, i);
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
                            openjtalk_label_clear(label);
                            openjtalk_finalize(oj);
                            free(dic_path);
                            return NULL;
                        }
                        size_t new_size = phoneme_buffer_size * 2;
                        char* new_phonemes = (char*)realloc(phonemes, new_size);
                        if (!new_phonemes) {
                            free(phonemes);
                            openjtalk_label_clear(label);
                            openjtalk_finalize(oj);
                            free(dic_path);
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
    openjtalk_label_clear(label);
    openjtalk_finalize(oj);
    free(dic_path);

    if (total_phoneme_len == 0) {
        free(phonemes);
        return NULL;
    }

    return phonemes;
}

static OpenJTalkProsodyResult* builtin_text_to_phonemes_with_prosody(const char* text) {
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
    char* dic_path = snapshot_effective_dictionary_path_locked();
    if (!dic_path) {
        fprintf(stderr, "Error: Failed to get OpenJTalk dictionary path\n");
        return NULL;
    }

    // Initialize OpenJTalk with C API
    OpenJTalk* oj = openjtalk_initialize_with_dict(dic_path);
    if (!oj) {
        fprintf(stderr, "Error: Failed to initialize OpenJTalk (dictionary: %s)\n", dic_path);
        free(dic_path);
        return NULL;
    }

    // Extract full context labels
    HTS_Label* label = openjtalk_extract_fullcontext(oj, text);
    if (!label) {
        fprintf(stderr, "Error: Failed to extract full context labels\n");
        openjtalk_finalize(oj);
        free(dic_path);
        return NULL;
    }

    size_t label_size = openjtalk_label_get_size(label);

    // Allocate result structure
    OpenJTalkProsodyResult* prosody_result =
        (OpenJTalkProsodyResult*)malloc(sizeof(OpenJTalkProsodyResult));
    if (!prosody_result) {
        openjtalk_label_clear(label);
        openjtalk_finalize(oj);
        free(dic_path);
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
        openjtalk_label_clear(label);
        openjtalk_finalize(oj);
        free(dic_path);
        return NULL;
    }

    prosody_result->phonemes[0] = '\0';
    size_t total_phoneme_len = 0;

    // Extract phonemes and prosody from full-context labels
    for (size_t i = 0; i < label_size; i++) {
        const char* label_str = openjtalk_label_get_string(label, i);
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
    openjtalk_label_clear(label);
    openjtalk_finalize(oj);
    free(dic_path);

    if (prosody_result->count == 0) {
        openjtalk_free_prosody_result(prosody_result);
        return NULL;
    }

    return prosody_result;
}

// Convert text to phonemes using OpenJTalk C API
char* openjtalk_text_to_phonemes(const char* text) {
    char* phonemes = NULL;

    CONFIG_MUTEX_LOCK();
    phonemes = native_text_to_phonemes(text);
    if (!phonemes) {
        phonemes = builtin_text_to_phonemes(text);
    }
    CONFIG_MUTEX_UNLOCK();

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
    OpenJTalkProsodyResult* result = NULL;

    CONFIG_MUTEX_LOCK();
    result = native_text_to_phonemes_with_prosody(text);
    if (!result) {
        result = builtin_text_to_phonemes_with_prosody(text);
    }
    CONFIG_MUTEX_UNLOCK();

    return result;
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
