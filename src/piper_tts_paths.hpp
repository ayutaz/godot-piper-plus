#ifndef PIPER_TTS_PATHS_H
#define PIPER_TTS_PATHS_H

#include <godot_cpp/variant/string.hpp>

#include <vector>

namespace godot::piper_tts_paths {

String resolve_global_path(const String &path);
String resolve_model_path(const String &path, const std::vector<String> &model_roots);
String resolve_config_path(const String &configured_config_path,
		const String &resolved_model_path);

bool path_exists(const String &path);
bool is_web_runtime();
String resolve_web_model_source(const String &path);
String resolve_web_config_source(
		const String &configured_config_path, const String &resolved_model_path);
bool read_web_file_bytes(const String &source_path, std::vector<uint8_t> &data,
		String &error_message);
bool read_web_file_text(const String &source_path, String &text,
		String &error_message);
String find_web_cmudict_source(const String &model_path, const String &config_path);

} // namespace godot::piper_tts_paths

#endif // PIPER_TTS_PATHS_H
