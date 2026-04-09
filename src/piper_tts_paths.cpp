#include "piper_tts_paths.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <optional>

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

namespace godot::piper_tts_paths {

namespace {

struct ModelCatalogEntry {
	std::string key;
	std::vector<std::string> aliases;
};

const std::vector<ModelCatalogEntry> &get_model_catalog() {
	static const std::vector<ModelCatalogEntry> catalog = {
		{"css10", {"css10-ja-6lang-fp16", "css10-ja-6lang"}},
		{"ja_JP-test-medium", {}},
		{"tsukuyomi-chan", {"tsukuyomi", "tsukuyomi-chan-6lang-fp16",
									  "ja_JP-tsukuyomi-chan-medium", "ja-tsukuyomi"}},
		{"en_US-ljspeech-medium", {"ljspeech", "en-ljspeech-medium"}},
	};
	return catalog;
}

bool has_suffix(const std::string &value, const char *suffix) {
	const std::string suffix_str = suffix;
	return value.size() >= suffix_str.size() &&
			value.compare(value.size() - suffix_str.size(), suffix_str.size(), suffix_str) == 0;
}

std::string strip_model_filename_suffix(const std::string &name) {
	if (has_suffix(name, ".onnx.json")) {
		return name.substr(0, name.size() - 10);
	}
	if (has_suffix(name, ".onnx")) {
		return name.substr(0, name.size() - 5);
	}
	return name;
}

std::string resolve_catalog_key(const std::string &name) {
	for (const auto &entry : get_model_catalog()) {
		if (name == entry.key) {
			return entry.key;
		}
		for (const auto &alias : entry.aliases) {
			if (name == alias) {
				return entry.key;
			}
		}
	}
	return {};
}

std::optional<std::filesystem::path> find_onnx_in_directory(
		const std::filesystem::path &directory, const std::string &preferred_stem) {
	std::error_code ec;
	if (!std::filesystem::exists(directory, ec) ||
			!std::filesystem::is_directory(directory, ec)) {
		return std::nullopt;
	}

	const std::filesystem::path preferred_file = directory / (preferred_stem + ".onnx");
	if (std::filesystem::exists(preferred_file, ec) &&
			std::filesystem::is_regular_file(preferred_file, ec)) {
		return preferred_file;
	}

	const std::filesystem::path nested_directory = directory / preferred_stem;
	if (std::filesystem::exists(nested_directory, ec) &&
			std::filesystem::is_directory(nested_directory, ec)) {
		const std::filesystem::path nested_preferred =
				nested_directory / (preferred_stem + ".onnx");
		if (std::filesystem::exists(nested_preferred, ec) &&
				std::filesystem::is_regular_file(nested_preferred, ec)) {
			return nested_preferred;
		}
	}

	std::optional<std::filesystem::path> single_onnx;
	for (const auto &entry : std::filesystem::directory_iterator(directory, ec)) {
		if (ec) {
			break;
		}
		if (!entry.is_regular_file(ec)) {
			continue;
		}
		if (entry.path().extension() != ".onnx") {
			continue;
		}
		if (single_onnx.has_value()) {
			return std::nullopt;
		}
		single_onnx = entry.path();
	}

	return single_onnx;
}

std::optional<String> find_onnx_in_resource_directory(
		const String &directory, const std::string &preferred_stem) {
	Ref<DirAccess> dir = DirAccess::open(directory);
	if (dir.is_null()) {
		return std::nullopt;
	}

	const String preferred_stem_string(preferred_stem.c_str());
	const String preferred_file = directory.path_join(preferred_stem_string + String(".onnx"));
	if (FileAccess::file_exists(preferred_file)) {
		return preferred_file;
	}

	const String nested_directory = directory.path_join(preferred_stem_string);
	if (DirAccess::open(nested_directory).is_valid()) {
		const String nested_preferred =
				nested_directory.path_join(preferred_stem_string + String(".onnx"));
		if (FileAccess::file_exists(nested_preferred)) {
			return nested_preferred;
		}
	}

	std::optional<String> single_onnx;
	const PackedStringArray files = DirAccess::get_files_at(directory);
	for (int i = 0; i < files.size(); ++i) {
		const String file_name = files[i];
		if (file_name.get_extension() != "onnx") {
			continue;
		}
		if (single_onnx.has_value()) {
			return std::nullopt;
		}
		single_onnx = directory.path_join(file_name);
	}

	return single_onnx;
}

} // namespace

String resolve_global_path(const String &path) {
	if (path.begins_with("res://") || path.begins_with("user://")) {
		return ProjectSettings::get_singleton()->globalize_path(path);
	}
	return path;
}

String resolve_model_path(const String &path, const std::vector<String> &model_roots) {
	if (path.is_empty()) {
		return String();
	}

	String resolved_path = resolve_global_path(path.strip_edges());
	if (!resolved_path.is_empty()) {
		std::filesystem::path fs_path = std::filesystem::path(resolved_path.utf8().get_data());
		std::error_code ec;
		if (std::filesystem::exists(fs_path, ec)) {
			if (std::filesystem::is_regular_file(fs_path, ec) &&
					fs_path.extension() == ".onnx") {
				return resolved_path;
			}
			if (std::filesystem::is_directory(fs_path, ec)) {
				std::optional<std::filesystem::path> maybe_file =
						find_onnx_in_directory(fs_path, fs_path.filename().string());
				if (maybe_file.has_value()) {
					std::string maybe_file_str = maybe_file->string();
					return String(maybe_file_str.c_str());
				}
			}
		}
	}

	String stripped = path.strip_edges();
	std::string input_name = stripped.get_file().utf8().get_data();
	input_name = strip_model_filename_suffix(input_name);
	const std::string canonical_key = resolve_catalog_key(input_name);
	if (canonical_key.empty()) {
		return String();
	}

	const std::vector<std::string> search_stems = canonical_key == input_name
			? std::vector<std::string>{canonical_key}
			: std::vector<std::string>{canonical_key, input_name};

	for (const String &root_path : model_roots) {
		const String resolved_root = resolve_global_path(root_path);
		if (resolved_root.is_empty()) {
			continue;
		}

		std::filesystem::path root_fs = std::filesystem::path(resolved_root.utf8().get_data());
		for (const std::string &search_stem : search_stems) {
			for (const std::filesystem::path &candidate_dir : {
						 root_fs,
						 root_fs / search_stem,
				 }) {
				std::optional<std::filesystem::path> maybe_file =
						find_onnx_in_directory(candidate_dir, search_stem);
				if (maybe_file.has_value()) {
					std::string maybe_file_str = maybe_file->string();
					return String(maybe_file_str.c_str());
				}
			}
		}
	}

	return String();
}

String resolve_config_path(const String &configured_config_path,
		const String &resolved_model_path) {
	if (!configured_config_path.is_empty()) {
		return resolve_global_path(configured_config_path);
	}

	String model_config_path = resolved_model_path + String(".json");
	if (path_exists(model_config_path)) {
		return model_config_path;
	}

	String directory_config_path = resolved_model_path.get_base_dir().path_join("config.json");
	if (path_exists(directory_config_path)) {
		return directory_config_path;
	}

	return String();
}

bool path_exists(const String &path) {
	if (path.is_empty()) {
		return false;
	}

	return std::filesystem::exists(std::filesystem::path(path.utf8().get_data()));
}

bool is_web_runtime() {
	return OS::get_singleton() && OS::get_singleton()->has_feature("web");
}

String resolve_web_model_source(const String &path) {
	if (path.is_empty()) {
		return String();
	}

	const String stripped = path.strip_edges();
	if (FileAccess::file_exists(stripped) && stripped.get_extension() == "onnx") {
		return stripped;
	}

	if (DirAccess::open(stripped).is_valid()) {
		const std::string preferred_stem =
				strip_model_filename_suffix(stripped.get_file().utf8().get_data());
		const std::optional<String> maybe_file =
				find_onnx_in_resource_directory(stripped, preferred_stem);
		if (maybe_file.has_value()) {
			return *maybe_file;
		}
	}

	std::string input_name = stripped.get_file().utf8().get_data();
	input_name = strip_model_filename_suffix(input_name);
	const std::string canonical_key = resolve_catalog_key(input_name);
	if (canonical_key.empty()) {
		return String();
	}

	const std::vector<String> model_roots = {
		"user://piper/models",
		"res://piper_plus_assets/models",
		"res://addons/piper_plus/models",
		"res://models",
	};

	const std::vector<std::string> search_stems = canonical_key == input_name
			? std::vector<std::string>{canonical_key}
			: std::vector<std::string>{canonical_key, input_name};

	for (const String &root_path : model_roots) {
		for (const std::string &search_stem : search_stems) {
			for (const String &candidate_dir : {
						 root_path,
						 root_path.path_join(String(search_stem.c_str())),
				 }) {
				const std::optional<String> maybe_file =
						find_onnx_in_resource_directory(candidate_dir, search_stem);
				if (maybe_file.has_value()) {
					return *maybe_file;
				}
			}
		}
	}

	return String();
}

String resolve_web_config_source(
		const String &configured_config_path, const String &resolved_model_path) {
	const String stripped_config = configured_config_path.strip_edges();
	if (!stripped_config.is_empty() && FileAccess::file_exists(stripped_config)) {
		return stripped_config;
	}

	const String model_config_path = resolved_model_path + String(".json");
	if (FileAccess::file_exists(model_config_path)) {
		return model_config_path;
	}

	const String directory_config_path =
			resolved_model_path.get_base_dir().path_join("config.json");
	if (FileAccess::file_exists(directory_config_path)) {
		return directory_config_path;
	}

	return String();
}

bool read_web_file_bytes(const String &source_path, std::vector<uint8_t> &data,
		String &error_message) {
	const String trimmed_source = source_path.strip_edges();
	if (trimmed_source.is_empty()) {
		error_message = "PiperTTS: web resource source path is empty.";
		return false;
	}

	if (!FileAccess::file_exists(trimmed_source)) {
		error_message = String("PiperTTS: failed to open web resource source: ") + trimmed_source;
		return false;
	}

	const PackedByteArray bytes = FileAccess::get_file_as_bytes(trimmed_source);
	if (bytes.is_empty()) {
		error_message = String("PiperTTS: web resource is empty: ") + trimmed_source;
		return false;
	}

	data.resize(bytes.size());
	std::memcpy(data.data(), bytes.ptr(), static_cast<size_t>(bytes.size()));
	return true;
}

bool read_web_file_text(const String &source_path, String &text,
		String &error_message) {
	const String trimmed_source = source_path.strip_edges();
	if (trimmed_source.is_empty()) {
		error_message = "PiperTTS: web resource source path is empty.";
		return false;
	}

	if (!FileAccess::file_exists(trimmed_source)) {
		error_message = String("PiperTTS: failed to open web resource source: ") + trimmed_source;
		return false;
	}

	text = FileAccess::get_file_as_string(trimmed_source);
	if (text.is_empty()) {
		error_message = String("PiperTTS: web resource is empty: ") + trimmed_source;
		return false;
	}
	return true;
}

String find_web_cmudict_source(const String &model_path, const String &config_path) {
	const String model_dir = model_path.get_base_dir();
	const String config_dir = config_path.get_base_dir();

	std::vector<String> candidate_dirs;
	auto push_candidate_dir = [&](const String &candidate_dir) {
		if (candidate_dir.is_empty()) {
			return;
		}
		if (std::find(candidate_dirs.begin(), candidate_dirs.end(), candidate_dir) ==
				candidate_dirs.end()) {
			candidate_dirs.push_back(candidate_dir);
		}
	};

	push_candidate_dir(model_dir);
	push_candidate_dir(config_dir);
	push_candidate_dir(model_dir.get_base_dir().path_join("dictionaries"));
	push_candidate_dir(config_dir.get_base_dir().path_join("dictionaries"));
	push_candidate_dir(model_dir.get_base_dir().get_base_dir().path_join("dictionaries"));
	push_candidate_dir(config_dir.get_base_dir().get_base_dir().path_join("dictionaries"));
	push_candidate_dir("user://piper/dictionaries");
	push_candidate_dir("res://piper_plus_assets/dictionaries");
	push_candidate_dir("res://addons/piper_plus/dictionaries");

	for (const String &candidate_dir : candidate_dirs) {
		for (const String &filename : {"cmudict_data.json", "cmudict.json"}) {
			const String candidate = candidate_dir.path_join(filename);
			if (FileAccess::file_exists(candidate)) {
				return candidate;
			}
		}
	}

	return String();
}

} // namespace godot::piper_tts_paths
