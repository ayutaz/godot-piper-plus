#ifndef OPENJTALK_DICTIONARY_MANAGER_H
#define OPENJTALK_DICTIONARY_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

// Get the path to the OpenJTalk dictionary
const char* get_openjtalk_dictionary_path();

// Check whether a dictionary path points to a compiled OpenJTalk/MeCab dictionary.
int openjtalk_dictionary_path_is_ready(const char* path);

// Get the path to the HTS voice file
const char* get_openjtalk_voice_path();

// Ensure the OpenJTalk dictionary is available (download if necessary)
int ensure_openjtalk_dictionary();

#ifdef __cplusplus
}
#endif

#endif // OPENJTALK_DICTIONARY_MANAGER_H
