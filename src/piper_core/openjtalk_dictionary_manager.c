#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#define mkdir(path, mode) _mkdir(path)
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#endif

// Dictionary directory name
#define DICTIONARY_DIR "open_jtalk_dic_utf_8-1.11"

// Helper function to create directory with parents
static int mkdir_p(const char* path) {
    char tmp[1024];
    char* p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;

    for (p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
    return 0;
}

// Get the base directory for data files
static const char* get_data_dir(void) {
    static char data_dir[1024] = {0};

    if (data_dir[0] != '\0') {
        return data_dir;
    }

    // Check environment variable first
    const char* env_dir = getenv("OPENJTALK_DATA_DIR");
    if (env_dir && access(env_dir, F_OK) == 0) {
        strncpy(data_dir, env_dir, sizeof(data_dir) - 1);
        return data_dir;
    }

#ifdef _WIN32
    // On Windows, try AppData
    const char* appdata = getenv("APPDATA");
    if (appdata) {
        snprintf(data_dir, sizeof(data_dir), "%s\\piper", appdata);
    } else {
        // Fallback to current directory
        GetCurrentDirectoryA(sizeof(data_dir) - 10, data_dir);
        strcat(data_dir, "\\data");
    }
#else
    // On Unix-like systems, use XDG_DATA_HOME or ~/.local/share
    const char* xdg_data = getenv("XDG_DATA_HOME");
    if (xdg_data) {
        snprintf(data_dir, sizeof(data_dir), "%s/piper", xdg_data);
    } else {
        const char* home = getenv("HOME");
        if (home) {
            snprintf(data_dir, sizeof(data_dir), "%s/.local/share/piper", home);
        } else {
            // Fallback to /tmp
            strcpy(data_dir, "/tmp/piper");
        }
    }
#endif

    // Create directory if it doesn't exist
    struct stat st = {0};
    if (stat(data_dir, &st) == -1) {
        mkdir_p(data_dir);
    }

    return data_dir;
}

// Get the path to the OpenJTalk dictionary
const char* get_openjtalk_dictionary_path(void) {
    static char dict_path[1024] = {0};

    if (dict_path[0] != '\0') {
        return dict_path;
    }

    // Check environment variable first
    const char* env_dict = getenv("OPENJTALK_DICTIONARY_PATH");
    if (env_dict && access(env_dict, F_OK) == 0) {
        strncpy(dict_path, env_dict, sizeof(dict_path) - 1);
        return dict_path;
    }

    // Check common system locations
    const char* system_paths[] = {
#ifdef _WIN32
        "C:\\Program Files\\open_jtalk\\dic",
        "C:\\Program Files (x86)\\open_jtalk\\dic",
#else
        "/usr/share/open_jtalk/dic",
        "/usr/local/share/open_jtalk/dic",
        "/opt/open_jtalk/dic",
#endif
        NULL
    };

    for (int i = 0; system_paths[i] != NULL; i++) {
        if (access(system_paths[i], F_OK) == 0) {
            strncpy(dict_path, system_paths[i], sizeof(dict_path) - 1);
            return dict_path;
        }
    }

    // Use local data directory
    const char* data_dir = get_data_dir();
    snprintf(dict_path, sizeof(dict_path), "%s/%s", data_dir, DICTIONARY_DIR);

    return dict_path;
}

// Ensure the OpenJTalk dictionary is available
// NOTE: Dictionary downloading is handled by the Godot editor plugin
// (model_downloader.gd). This function only checks if the dictionary
// exists at the expected path.
int ensure_openjtalk_dictionary(void) {
    const char* dict_path = get_openjtalk_dictionary_path();

    // Check if dictionary already exists
    if (access(dict_path, F_OK) == 0) {
        return 0;
    }

    // Dictionary not found - log guidance message
    fprintf(stderr,
        "OpenJTalk dictionary not found at: %s\n"
        "Please use the Godot editor plugin (PiperTTS > Model Downloader) to download it,\n"
        "or set the OPENJTALK_DICTIONARY_PATH environment variable,\n"
        "or set the dictionary_path property on PiperTTS node.\n",
        dict_path);
    return -1;
}

// Get the path to the HTS voice file
// HTS voice is not needed for phonemizer-only mode (C API direct call).
const char* get_openjtalk_voice_path(void) {
    return NULL;
}
