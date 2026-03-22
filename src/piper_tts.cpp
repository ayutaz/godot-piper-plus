#include "piper_tts.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "piper_core/custom_dictionary.hpp"
#include "piper_core/phoneme_parser.hpp"
#include "piper_core/piper.hpp"

extern "C" {
	void openjtalk_set_dictionary_path(const char* path);
}

#include <cstring>
#include <functional>
#include <optional>
#include <vector>

namespace godot {

namespace {

struct ModelCatalogEntry {
	std::string key;
	std::vector<std::string> aliases;
};

const std::vector<ModelCatalogEntry> &get_model_catalog() {
	static const std::vector<ModelCatalogEntry> catalog = {
		{"ja_JP-test-medium", {}},
		{"tsukuyomi-chan", {"tsukuyomi", "tsukuyomi-chan-6lang-fp16", "ja_JP-tsukuyomi-chan-medium", "ja-tsukuyomi"}},
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

bool path_exists(const String &path) {
	if (path.is_empty()) {
		return false;
	}

	return std::filesystem::exists(std::filesystem::path(path.utf8().get_data()));
}

std::string normalize_language_code(const String &code) {
	String normalized = code.strip_edges().to_lower();
	std::string utf8 = normalized.utf8().get_data();
	std::replace(utf8.begin(), utf8.end(), '_', '-');
	return utf8;
}

Error apply_requested_language(
		piper::Voice &voice, int requested_language_id,
		const String &requested_language_code, String &error_message) {
	const std::string normalized_code = normalize_language_code(requested_language_code);
	if (!normalized_code.empty()) {
		if (!voice.modelConfig.languageIdMap || voice.modelConfig.languageIdMap->empty()) {
			error_message = "PiperTTS: language_code was set, but the loaded model does not expose language_id_map.";
			return ERR_INVALID_PARAMETER;
		}

		std::vector<std::string> candidates = {normalized_code};
		const std::size_t separator = normalized_code.find('-');
		if (separator != std::string::npos && separator > 0) {
			candidates.push_back(normalized_code.substr(0, separator));
		}

		for (const auto &[language_code, language_value] : *voice.modelConfig.languageIdMap) {
			const std::string normalized_map_code =
					normalize_language_code(String(language_code.c_str()));
			for (const std::string &candidate : candidates) {
				if (candidate == normalized_map_code) {
					voice.synthesisConfig.languageId = language_value;
					return OK;
				}
				const std::size_t separator = normalized_map_code.find('-');
				if (separator != std::string::npos &&
						candidate == normalized_map_code.substr(0, separator)) {
					voice.synthesisConfig.languageId = language_value;
					return OK;
				}
			}
		}

		error_message = String("PiperTTS: language_code '") +
				requested_language_code.strip_edges() +
				"' was not found in language_id_map.";
		return ERR_INVALID_PARAMETER;
	}

	if (requested_language_id >= 0) {
		if (voice.modelConfig.numLanguages > 0 &&
				requested_language_id >= voice.modelConfig.numLanguages) {
			error_message = String("PiperTTS: language_id is out of range for this model: ") +
					String::num_int64(requested_language_id);
			return ERR_INVALID_PARAMETER;
		}

		voice.synthesisConfig.languageId =
				static_cast<piper::LanguageId>(requested_language_id);
		return OK;
	}

	voice.synthesisConfig.languageId.reset();
	return OK;
}

struct EffectiveRequest {
	bool has_text = false;
	bool has_phoneme_string = false;
	String text;
	String phoneme_string;
	int speaker_id = 0;
	int language_id = -1;
	String language_code;
	float speech_rate = 1.0f;
	float noise_scale = 0.667f;
	float noise_w = 0.8f;
	float sentence_silence_seconds = 0.2f;
	Dictionary phoneme_silence_seconds;
};

class ScopedVoiceSynthesisConfig {
public:
	explicit ScopedVoiceSynthesisConfig(piper::Voice &voice)
			: voice_(voice), saved_(voice.synthesisConfig) {}

	~ScopedVoiceSynthesisConfig() {
		voice_.synthesisConfig = saved_;
	}

private:
	piper::Voice &voice_;
	piper::SynthesisConfig saved_;
};

String packed_or_array_to_phoneme_string(const Variant &value, String &error_message) {
	if (value.get_type() == Variant::STRING) {
		return value;
	}

	PackedStringArray tokens;
	if (value.get_type() == Variant::PACKED_STRING_ARRAY) {
		tokens = value;
	} else if (value.get_type() == Variant::ARRAY) {
		Array array = value;
		tokens.resize(array.size());
		for (int i = 0; i < array.size(); ++i) {
			tokens.set(i, String(array[i]));
		}
	} else {
		error_message = "PiperTTS: request.phonemes must be a String, Array, or PackedStringArray.";
		return String();
	}

	String joined;
	for (int i = 0; i < tokens.size(); ++i) {
		if (i > 0) {
			joined += " ";
		}
		joined += String(tokens[i]).strip_edges();
	}

	return joined.strip_edges();
}

String language_code_from_id(const piper::Voice &voice,
		const std::optional<piper::LanguageId> &language_id) {
	if (!language_id.has_value() || !voice.modelConfig.languageIdMap) {
		return String();
	}

	for (const auto &[code, value] : *voice.modelConfig.languageIdMap) {
		if (value == *language_id) {
			return String(code.c_str());
		}
	}

	return String();
}

Array phoneme_timings_to_dictionary_array(
		const std::vector<piper::PhonemeInfo> &phoneme_timings) {
	Array timings;
	for (const auto &timing : phoneme_timings) {
		Dictionary entry;
		entry["phoneme"] = String::utf8(timing.phoneme.c_str());
		entry["start_time"] = timing.start_time;
		entry["end_time"] = timing.end_time;
		entry["start_frame"] = timing.start_frame;
		entry["end_frame"] = timing.end_frame;
		timings.push_back(entry);
	}
	return timings;
}

Dictionary synthesis_result_to_dictionary(const piper::SynthesisResult &result, int sample_rate) {
	Dictionary data;
	data["sample_rate"] = sample_rate;
	data["infer_seconds"] = result.inferSeconds;
	data["audio_seconds"] = result.audioSeconds;
	data["real_time_factor"] = result.realTimeFactor;
	data["has_timing_info"] = result.hasTimingInfo;
	data["phoneme_timings"] = phoneme_timings_to_dictionary_array(result.phonemeTimings);
	return data;
}

Dictionary inspection_result_to_dictionary(
		const piper::InspectionResult &result, const piper::Voice &voice) {
	Dictionary data;
	Array phoneme_sentences;
	Array phoneme_id_sentences;

	for (std::size_t sentence_index = 0; sentence_index < result.phonemeSentences.size();
			++sentence_index) {
		const auto &sentence = result.phonemeSentences[sentence_index];
		PackedStringArray phoneme_tokens;
		phoneme_tokens.resize(static_cast<int>(sentence.size()));
		for (std::size_t phoneme_index = 0; phoneme_index < sentence.size(); ++phoneme_index) {
			phoneme_tokens.set(static_cast<int>(phoneme_index),
					String::utf8(
							piper::phonemeToString(sentence[phoneme_index]).c_str()));
		}
		phoneme_sentences.push_back(phoneme_tokens);

		PackedInt64Array id_tokens;
		if (sentence_index < result.phonemeIdSentences.size()) {
			const auto &ids = result.phonemeIdSentences[sentence_index];
			id_tokens.resize(static_cast<int>(ids.size()));
			for (std::size_t id_index = 0; id_index < ids.size(); ++id_index) {
				id_tokens.set(static_cast<int>(id_index), ids[id_index]);
			}
		}
		phoneme_id_sentences.push_back(id_tokens);
	}

	Dictionary missing_phonemes;
	for (const auto &[phoneme, count] : result.missingPhonemes) {
		missing_phonemes[String::utf8(piper::phonemeToString(phoneme).c_str())] =
				static_cast<int64_t>(count);
	}

	data["phoneme_sentences"] = phoneme_sentences;
	data["phoneme_id_sentences"] = phoneme_id_sentences;
	data["missing_phonemes"] = missing_phonemes;
	data["resolved_language_id"] =
			result.resolvedLanguageId.has_value() ? Variant(*result.resolvedLanguageId) : Variant(-1);
	data["resolved_language_code"] =
			language_code_from_id(voice, result.resolvedLanguageId);
	return data;
}

bool build_effective_request(
		const PiperTTS &tts, const Dictionary &request, EffectiveRequest &effective_request,
		String &error_message) {
	effective_request.speaker_id = tts.get_speaker_id();
	effective_request.language_id = tts.get_language_id();
	effective_request.language_code = tts.get_language_code();
	effective_request.speech_rate = tts.get_speech_rate();
	effective_request.noise_scale = tts.get_noise_scale();
	effective_request.noise_w = tts.get_noise_w();
	effective_request.sentence_silence_seconds = tts.get_sentence_silence_seconds();
	effective_request.phoneme_silence_seconds = tts.get_phoneme_silence_seconds();

	if (request.has("text")) {
		effective_request.has_text = true;
		effective_request.text = String(request["text"]);
	}

	if (request.has("phoneme_string")) {
		effective_request.has_phoneme_string = true;
		effective_request.phoneme_string = String(request["phoneme_string"]).strip_edges();
	}

	if (request.has("phonemes")) {
		effective_request.has_phoneme_string = true;
		effective_request.phoneme_string =
				packed_or_array_to_phoneme_string(request["phonemes"], error_message);
		if (!error_message.is_empty()) {
			return false;
		}
	}

	if (request.has("speaker_id")) {
		effective_request.speaker_id = static_cast<int>(request["speaker_id"]);
	}
	if (request.has("language_id")) {
		effective_request.language_id = static_cast<int>(request["language_id"]);
	}
	if (request.has("language_code")) {
		effective_request.language_code = String(request["language_code"]).strip_edges();
	}
	if (request.has("speech_rate")) {
		effective_request.speech_rate = static_cast<double>(request["speech_rate"]);
	}
	if (request.has("noise_scale")) {
		effective_request.noise_scale = static_cast<double>(request["noise_scale"]);
	}
	if (request.has("noise_w")) {
		effective_request.noise_w = static_cast<double>(request["noise_w"]);
	}
	if (request.has("sentence_silence_seconds")) {
		effective_request.sentence_silence_seconds =
				static_cast<double>(request["sentence_silence_seconds"]);
	} else if (request.has("sentence_silence")) {
		effective_request.sentence_silence_seconds =
				static_cast<double>(request["sentence_silence"]);
	}
	if (request.has("phoneme_silence_seconds")) {
		effective_request.phoneme_silence_seconds = request["phoneme_silence_seconds"];
	} else if (request.has("phoneme_silence")) {
		effective_request.phoneme_silence_seconds = request["phoneme_silence"];
	}

	if (effective_request.has_text && effective_request.has_phoneme_string) {
		error_message = "PiperTTS: request cannot contain both text and phoneme input.";
		return false;
	}

	if (!effective_request.has_text && !effective_request.has_phoneme_string) {
		error_message = "PiperTTS: request must contain text, phoneme_string, or phonemes.";
		return false;
	}

	return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

PiperTTS::PiperTTS() {
	set_process(false);
}

PiperTTS::~PiperTTS() {
	stop_requested.store(true);
	_join_worker_thread();

	// Clean up streaming state
	streaming_active_.store(false);
	audio_chunk_queue_.clear();
	pending_samples_.clear();
	pending_sample_offset_ = 0;

	if (ready && piper_config) {
		piper::terminate(*piper_config);
		ready = false;
	}
}

// ---------------------------------------------------------------------------
// Godot binding
// ---------------------------------------------------------------------------

void PiperTTS::_bind_methods() {
	// --- Property: model_path ---
	ClassDB::bind_method(D_METHOD("set_model_path", "path"), &PiperTTS::set_model_path);
	ClassDB::bind_method(D_METHOD("get_model_path"), &PiperTTS::get_model_path);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "model_path", PROPERTY_HINT_FILE, "*.onnx"),
			"set_model_path", "get_model_path");

	// --- Property: config_path ---
	ClassDB::bind_method(D_METHOD("set_config_path", "path"), &PiperTTS::set_config_path);
	ClassDB::bind_method(D_METHOD("get_config_path"), &PiperTTS::get_config_path);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "config_path", PROPERTY_HINT_FILE, "*.json"),
			"set_config_path", "get_config_path");

	// --- Property: dictionary_path ---
	ClassDB::bind_method(D_METHOD("set_dictionary_path", "path"), &PiperTTS::set_dictionary_path);
	ClassDB::bind_method(D_METHOD("get_dictionary_path"), &PiperTTS::get_dictionary_path);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "dictionary_path", PROPERTY_HINT_DIR),
			"set_dictionary_path", "get_dictionary_path");

	// --- Property: custom_dictionary_path ---
	ClassDB::bind_method(D_METHOD("set_custom_dictionary_path", "path"), &PiperTTS::set_custom_dictionary_path);
	ClassDB::bind_method(D_METHOD("get_custom_dictionary_path"), &PiperTTS::get_custom_dictionary_path);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "custom_dictionary_path", PROPERTY_HINT_FILE, "*.json"),
			"set_custom_dictionary_path", "get_custom_dictionary_path");

	// --- Property: speaker_id ---
	ClassDB::bind_method(D_METHOD("set_speaker_id", "id"), &PiperTTS::set_speaker_id);
	ClassDB::bind_method(D_METHOD("get_speaker_id"), &PiperTTS::get_speaker_id);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "speaker_id", PROPERTY_HINT_RANGE, "0,999,1"),
			"set_speaker_id", "get_speaker_id");

	// --- Property: language_id ---
	ClassDB::bind_method(D_METHOD("set_language_id", "id"), &PiperTTS::set_language_id);
	ClassDB::bind_method(D_METHOD("get_language_id"), &PiperTTS::get_language_id);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "language_id", PROPERTY_HINT_RANGE, "-1,999,1"),
			"set_language_id", "get_language_id");

	// --- Property: language_code ---
	ClassDB::bind_method(D_METHOD("set_language_code", "code"), &PiperTTS::set_language_code);
	ClassDB::bind_method(D_METHOD("get_language_code"), &PiperTTS::get_language_code);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "language_code"),
			"set_language_code", "get_language_code");

	// --- Property: speech_rate ---
	ClassDB::bind_method(D_METHOD("set_speech_rate", "rate"), &PiperTTS::set_speech_rate);
	ClassDB::bind_method(D_METHOD("get_speech_rate"), &PiperTTS::get_speech_rate);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speech_rate", PROPERTY_HINT_RANGE, "0.1,5.0,0.1"),
			"set_speech_rate", "get_speech_rate");

	// --- Property: noise_scale ---
	ClassDB::bind_method(D_METHOD("set_noise_scale", "scale"), &PiperTTS::set_noise_scale);
	ClassDB::bind_method(D_METHOD("get_noise_scale"), &PiperTTS::get_noise_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_scale", PROPERTY_HINT_RANGE, "0.0,2.0,0.01"),
			"set_noise_scale", "get_noise_scale");

	// --- Property: noise_w ---
	ClassDB::bind_method(D_METHOD("set_noise_w", "w"), &PiperTTS::set_noise_w);
	ClassDB::bind_method(D_METHOD("get_noise_w"), &PiperTTS::get_noise_w);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_w", PROPERTY_HINT_RANGE, "0.0,2.0,0.01"),
			"set_noise_w", "get_noise_w");

	// --- Property: sentence_silence_seconds ---
	ClassDB::bind_method(D_METHOD("set_sentence_silence_seconds", "seconds"), &PiperTTS::set_sentence_silence_seconds);
	ClassDB::bind_method(D_METHOD("get_sentence_silence_seconds"), &PiperTTS::get_sentence_silence_seconds);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sentence_silence_seconds", PROPERTY_HINT_RANGE, "0.0,5.0,0.01"),
			"set_sentence_silence_seconds", "get_sentence_silence_seconds");

	// --- Property: phoneme_silence_seconds ---
	ClassDB::bind_method(D_METHOD("set_phoneme_silence_seconds", "silence_map"), &PiperTTS::set_phoneme_silence_seconds);
	ClassDB::bind_method(D_METHOD("get_phoneme_silence_seconds"), &PiperTTS::get_phoneme_silence_seconds);
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "phoneme_silence_seconds"),
			"set_phoneme_silence_seconds", "get_phoneme_silence_seconds");

	// --- Property: execution_provider ---
	ClassDB::bind_method(D_METHOD("set_execution_provider", "provider"), &PiperTTS::set_execution_provider);
	ClassDB::bind_method(D_METHOD("get_execution_provider"), &PiperTTS::get_execution_provider);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "execution_provider", PROPERTY_HINT_ENUM, "CPU:0,CoreML:1,DirectML:2,NNAPI:3,Auto:4"),
			"set_execution_provider", "get_execution_provider");

	// --- Enum constants: ExecutionProviderGD ---
	BIND_ENUM_CONSTANT(EP_CPU);
	BIND_ENUM_CONSTANT(EP_COREML);
	BIND_ENUM_CONSTANT(EP_DIRECTML);
	BIND_ENUM_CONSTANT(EP_NNAPI);
	BIND_ENUM_CONSTANT(EP_AUTO);

	// --- Methods (M2: sync) ---
	ClassDB::bind_method(D_METHOD("initialize"), &PiperTTS::initialize);
	ClassDB::bind_method(D_METHOD("synthesize", "text"), &PiperTTS::synthesize);
	ClassDB::bind_method(D_METHOD("synthesize_request", "request"), &PiperTTS::synthesize_request);
	ClassDB::bind_method(D_METHOD("synthesize_phoneme_string", "phoneme_string"), &PiperTTS::synthesize_phoneme_string);
	ClassDB::bind_method(D_METHOD("is_ready"), &PiperTTS::is_ready);
	ClassDB::bind_method(D_METHOD("inspect_text", "text"), &PiperTTS::inspect_text);
	ClassDB::bind_method(D_METHOD("inspect_request", "request"), &PiperTTS::inspect_request);
	ClassDB::bind_method(D_METHOD("inspect_phoneme_string", "phoneme_string"), &PiperTTS::inspect_phoneme_string);
	ClassDB::bind_method(D_METHOD("get_last_synthesis_result"), &PiperTTS::get_last_synthesis_result);
	ClassDB::bind_method(D_METHOD("get_last_inspection_result"), &PiperTTS::get_last_inspection_result);

	// --- Methods (M3: async) ---
	ClassDB::bind_method(D_METHOD("synthesize_async", "text"), &PiperTTS::synthesize_async);
	ClassDB::bind_method(D_METHOD("stop"), &PiperTTS::stop);
	ClassDB::bind_method(D_METHOD("is_processing"), &PiperTTS::is_processing);

	// Internal methods for call_deferred (worker thread → main thread)
	ClassDB::bind_method(D_METHOD("_on_synthesis_raw_done", "pcm_data", "sample_rate", "generation"), &PiperTTS::_on_synthesis_raw_done);
	ClassDB::bind_method(D_METHOD("_on_synthesis_failed", "error", "generation"), &PiperTTS::_on_synthesis_failed);

	// --- Methods (M6: streaming) ---
	ClassDB::bind_method(D_METHOD("synthesize_streaming", "text", "playback"), &PiperTTS::synthesize_streaming);

	// --- Signals ---
	ADD_SIGNAL(MethodInfo("initialized", PropertyInfo(Variant::BOOL, "success")));
	ADD_SIGNAL(MethodInfo("synthesis_completed",
			PropertyInfo(Variant::OBJECT, "audio", PROPERTY_HINT_RESOURCE_TYPE, "AudioStreamWAV")));
	ADD_SIGNAL(MethodInfo("synthesis_failed",
			PropertyInfo(Variant::STRING, "error")));

	// --- Signal: streaming_ended ---
	ADD_SIGNAL(MethodInfo("streaming_ended"));
}

// ---------------------------------------------------------------------------
// Property accessors
// ---------------------------------------------------------------------------

void PiperTTS::set_model_path(const String &p_path) {
	model_path = p_path;
}

String PiperTTS::get_model_path() const {
	return model_path;
}

void PiperTTS::set_config_path(const String &p_path) {
	config_path = p_path;
}

String PiperTTS::get_config_path() const {
	return config_path;
}

void PiperTTS::set_dictionary_path(const String &p_path) {
	dictionary_path = p_path;
}

String PiperTTS::get_dictionary_path() const {
	return dictionary_path;
}

void PiperTTS::set_custom_dictionary_path(const String &p_path) {
	custom_dictionary_path = p_path;
}

String PiperTTS::get_custom_dictionary_path() const {
	return custom_dictionary_path;
}

void PiperTTS::set_language_code(const String &p_code) {
	language_code = p_code.strip_edges();
}

String PiperTTS::get_language_code() const {
	return language_code;
}

void PiperTTS::set_speaker_id(int p_id) {
	speaker_id = p_id < 0 ? 0 : p_id;
}

int PiperTTS::get_speaker_id() const {
	return speaker_id;
}

void PiperTTS::set_language_id(int p_id) {
	language_id = p_id < 0 ? -1 : p_id;
}

int PiperTTS::get_language_id() const {
	return language_id;
}

void PiperTTS::set_speech_rate(float p_rate) {
	speech_rate = CLAMP(p_rate, 0.1f, 5.0f);
}

float PiperTTS::get_speech_rate() const {
	return speech_rate;
}

void PiperTTS::set_noise_scale(float p_scale) {
	noise_scale = CLAMP(p_scale, 0.0f, 2.0f);
}

float PiperTTS::get_noise_scale() const {
	return noise_scale;
}

void PiperTTS::set_noise_w(float p_w) {
	noise_w = CLAMP(p_w, 0.0f, 2.0f);
}

float PiperTTS::get_noise_w() const {
	return noise_w;
}

void PiperTTS::set_sentence_silence_seconds(float p_seconds) {
	sentence_silence_seconds = MAX(p_seconds, 0.0f);
}

float PiperTTS::get_sentence_silence_seconds() const {
	return sentence_silence_seconds;
}

void PiperTTS::set_phoneme_silence_seconds(const Dictionary &p_map) {
	phoneme_silence_seconds = p_map;
}

Dictionary PiperTTS::get_phoneme_silence_seconds() const {
	return phoneme_silence_seconds;
}

void PiperTTS::set_execution_provider(int p_ep) {
	execution_provider = CLAMP(p_ep, 0, 4);
}

int PiperTTS::get_execution_provider() const {
	return execution_provider;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

String PiperTTS::resolve_path(const String &path) const {
	if (path.begins_with("res://") || path.begins_with("user://")) {
		return ProjectSettings::get_singleton()->globalize_path(path);
	}
	return path;
}

String PiperTTS::resolve_model_path(const String &path) const {
	if (path.is_empty()) {
		return String();
	}

	String resolved_path = resolve_path(path.strip_edges());
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

	const std::vector<String> model_roots = {
		"user://piper/models",
		"res://addons/piper_plus/models",
	};

	const std::vector<std::string> search_stems = canonical_key == input_name
			? std::vector<std::string>{canonical_key}
			: std::vector<std::string>{canonical_key, input_name};

	for (const String &root_path : model_roots) {
		const String resolved_root = resolve_path(root_path);
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

String PiperTTS::resolve_config_path(const String &resolved_model_path) const {
	if (!config_path.is_empty()) {
		return resolve_path(config_path);
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

Ref<AudioStreamWAV> PiperTTS::create_audio_stream(
		const std::vector<int16_t> &audio_buffer, int sample_rate) const {
	Ref<AudioStreamWAV> stream;
	stream.instantiate();
	stream->set_format(AudioStreamWAV::FORMAT_16_BITS);
	stream->set_mix_rate(sample_rate);
	stream->set_stereo(false);

	PackedByteArray data;
	data.resize(audio_buffer.size() * sizeof(int16_t));
	memcpy(data.ptrw(), audio_buffer.data(), data.size());
	stream->set_data(data);

	return stream;
}

namespace {

Error apply_phoneme_silence_dictionary(
		const Dictionary &silence_map, piper::Voice &voice, String &error_message) {
	if (silence_map.is_empty()) {
		voice.synthesisConfig.phonemeSilenceSeconds.reset();
		return OK;
	}

	std::map<piper::Phoneme, float> parsed_map;
	Array keys = silence_map.keys();
	for (int i = 0; i < keys.size(); ++i) {
		String key = String(keys[i]).strip_edges();
		if (key.is_empty()) {
			error_message = "PiperTTS: phoneme_silence_seconds keys must not be empty.";
			return ERR_INVALID_PARAMETER;
		}

		std::vector<piper::Phoneme> phonemes = piper::parsePhonemeString(
				key.utf8().get_data(), static_cast<int>(voice.phonemizeConfig.phonemeType));
		if (phonemes.size() != 1) {
			error_message = String("PiperTTS: phoneme_silence_seconds key '") + key +
					"' must resolve to exactly one phoneme token.";
			return ERR_INVALID_PARAMETER;
		}

		double silence_seconds = static_cast<double>(silence_map[key]);
		if (!std::isfinite(silence_seconds) || silence_seconds < 0.0) {
			error_message = String("PiperTTS: phoneme_silence_seconds value for '") + key +
					"' must be a finite value >= 0.0.";
			return ERR_INVALID_PARAMETER;
		}

		parsed_map[phonemes.front()] = static_cast<float>(silence_seconds);
	}

	voice.synthesisConfig.phonemeSilenceSeconds = std::move(parsed_map);
	return OK;
}

Error apply_effective_request_to_voice(
		piper::Voice &voice, const EffectiveRequest &effective_request,
		String &error_message) {
	voice.synthesisConfig.lengthScale = CLAMP(effective_request.speech_rate, 0.1f, 5.0f);
	voice.synthesisConfig.noiseScale = CLAMP(effective_request.noise_scale, 0.0f, 2.0f);
	voice.synthesisConfig.noiseW = CLAMP(effective_request.noise_w, 0.0f, 2.0f);
	voice.synthesisConfig.sentenceSilenceSeconds =
			MAX(effective_request.sentence_silence_seconds, 0.0f);
	voice.synthesisConfig.speakerId =
			static_cast<piper::SpeakerId>(MAX(effective_request.speaker_id, 0));

	Error silence_error = apply_phoneme_silence_dictionary(
			effective_request.phoneme_silence_seconds, voice, error_message);
	if (silence_error != OK) {
		return silence_error;
	}

	return apply_requested_language(
			voice, effective_request.language_id, effective_request.language_code,
			error_message);
}

std::vector<piper::Phoneme> parse_effective_phoneme_string(
		const piper::Voice &voice, const String &phoneme_string) {
	return piper::parsePhonemeString(
			phoneme_string.utf8().get_data(),
			static_cast<int>(voice.phonemizeConfig.phonemeType));
}

} // namespace

// ---------------------------------------------------------------------------
// Public methods (M2: sync)
// ---------------------------------------------------------------------------

Error PiperTTS::initialize() {
	if (processing.load() || streaming_active_.load()) {
		UtilityFunctions::push_error("PiperTTS: Cannot initialize while synthesis is in progress.");
		emit_signal("initialized", false);
		return ERR_BUSY;
	}

	if (ready) {
		piper::terminate(*piper_config);
		ready = false;
	}
	last_synthesis_result_.clear();
	last_inspection_result_.clear();
	{
		std::lock_guard<std::mutex> lock(pending_async_result_mutex_);
		pending_async_result_.reset();
	}

	if (model_path.is_empty()) {
		UtilityFunctions::push_error("PiperTTS: model_path is not set.");
		emit_signal("initialized", false);
		return ERR_UNCONFIGURED;
	}

	// Resolve Godot resource paths to absolute OS paths
	String abs_model = resolve_model_path(model_path);
	if (abs_model.is_empty()) {
		UtilityFunctions::push_error(
				"PiperTTS: model_path could not be resolved to a valid .onnx model file.");
		emit_signal("initialized", false);
		return ERR_CANT_OPEN;
	}
	String abs_config = resolve_config_path(abs_model);

	if (abs_config.is_empty()) {
		UtilityFunctions::push_error(
				"PiperTTS: config_path is not set and no fallback config was found next to the model.");
		emit_signal("initialized", false);
		return ERR_UNCONFIGURED;
	}

	// Set OpenJTalk dictionary path if configured
	if (!dictionary_path.is_empty()) {
		String abs_dict = resolve_path(dictionary_path);
		std::string dict_str = abs_dict.utf8().get_data();
		openjtalk_set_dictionary_path(dict_str.c_str());
		UtilityFunctions::print(String("PiperTTS: OpenJTalk dictionary path set to: ") + abs_dict);
	}

	std::string model_str = abs_model.utf8().get_data();
	std::string config_str = abs_config.utf8().get_data();

	bool piper_initialized = false;
	try {
		piper_config = std::make_unique<piper::PiperConfig>();
		voice = std::make_unique<piper::Voice>();
		piper::initialize(*piper_config);
		piper_initialized = true;

		voice->customDictionary.reset();
		if (!custom_dictionary_path.is_empty()) {
			String abs_custom_dictionary = resolve_path(custom_dictionary_path);
			std::string custom_dict_str = abs_custom_dictionary.utf8().get_data();
			voice->customDictionary = std::make_shared<piper::CustomDictionary>(custom_dict_str);
			UtilityFunctions::print(String("PiperTTS: Custom dictionary loaded: ") + abs_custom_dictionary);
		}

		// Load voice model
		std::optional<piper::SpeakerId> sid;
		if (speaker_id >= 0) {
			sid = static_cast<piper::SpeakerId>(speaker_id);
		}

		// Resolve EP_AUTO to platform-specific EP
		int ep = execution_provider;
		if (ep == EP_AUTO) {
#if defined(__APPLE__)
			ep = EP_COREML;
#elif defined(_WIN32) && defined(PIPER_PLUS_HAS_DIRECTML)
			ep = EP_DIRECTML;
#elif defined(__ANDROID__)
			ep = EP_NNAPI;
#else
			ep = EP_CPU;
#endif
		}

		piper::loadVoice(*piper_config, model_str, config_str,
				*voice, sid, ep);

		String language_error;
		if (apply_requested_language(*voice, language_id, language_code, language_error) != OK) {
			UtilityFunctions::push_error(language_error);
			piper::terminate(*piper_config);
			voice->customDictionary.reset();
			emit_signal("initialized", false);
			return ERR_INVALID_PARAMETER;
		}

		ready = true;
		if (model_path != abs_model) {
			UtilityFunctions::print(String("PiperTTS: Model resolved to: ") + abs_model);
		}
		if (config_path.is_empty()) {
			UtilityFunctions::print(String("PiperTTS: Config auto-resolved to: ") + abs_config);
		}
		UtilityFunctions::print("PiperTTS: Voice loaded successfully.");
		emit_signal("initialized", true);
		return OK;

	} catch (const std::exception &e) {
		if (piper_initialized && piper_config) {
			piper::terminate(*piper_config);
		}
		piper_config.reset();
		voice.reset();
		UtilityFunctions::push_error(
				String("PiperTTS: Failed to initialize -- ") + String(e.what()));
		ready = false;
		emit_signal("initialized", false);
		return ERR_CANT_OPEN;
	}
}

Ref<AudioStreamWAV> PiperTTS::synthesize(const String &text) {
	if (!ready) {
		UtilityFunctions::push_error("PiperTTS: Not initialized. Call initialize() first.");
		return Ref<AudioStreamWAV>();
	}

	if (text.is_empty()) {
		UtilityFunctions::push_warning("PiperTTS: Empty text provided.");
		return Ref<AudioStreamWAV>();
	}

	if (processing.load() || streaming_active_.load()) {
		UtilityFunctions::push_error("PiperTTS: Synthesis in progress. Call stop() first.");
		return Ref<AudioStreamWAV>();
	}
	last_synthesis_result_.clear();

	ScopedVoiceSynthesisConfig scoped_config(*voice);
	EffectiveRequest request;
	request.has_text = true;
	request.text = text;
	request.speaker_id = speaker_id;
	request.language_id = language_id;
	request.language_code = language_code;
	request.speech_rate = speech_rate;
	request.noise_scale = noise_scale;
	request.noise_w = noise_w;
	request.sentence_silence_seconds = sentence_silence_seconds;
	request.phoneme_silence_seconds = phoneme_silence_seconds;

	String language_error;
	if (apply_effective_request_to_voice(*voice, request, language_error) != OK) {
		UtilityFunctions::push_error(language_error);
		return Ref<AudioStreamWAV>();
	}

	std::string text_str = text.utf8().get_data();
	std::vector<int16_t> audio_buffer;
	piper::SynthesisResult result;

	try {
		piper::textToAudio(*piper_config, *voice, text_str,
				audio_buffer, result, nullptr);
	} catch (const std::exception &e) {
		UtilityFunctions::push_error(
				String("PiperTTS: Synthesis failed -- ") + String(e.what()));
		return Ref<AudioStreamWAV>();
	}

	if (audio_buffer.empty()) {
		UtilityFunctions::push_warning("PiperTTS: Synthesis produced no audio.");
		return Ref<AudioStreamWAV>();
	}

	int sample_rate = voice->synthesisConfig.sampleRate;
	last_synthesis_result_ = synthesis_result_to_dictionary(result, sample_rate);
	last_synthesis_result_["input_mode"] = "text";
	last_synthesis_result_["sentence_silence_seconds"] = voice->synthesisConfig.sentenceSilenceSeconds;
	last_synthesis_result_["phoneme_silence_seconds"] = phoneme_silence_seconds;
	return create_audio_stream(audio_buffer, sample_rate);
}

Ref<AudioStreamWAV> PiperTTS::synthesize_request(const Dictionary &request_dictionary) {
	if (!ready) {
		UtilityFunctions::push_error("PiperTTS: Not initialized. Call initialize() first.");
		return Ref<AudioStreamWAV>();
	}

	if (processing.load() || streaming_active_.load()) {
		UtilityFunctions::push_error("PiperTTS: Synthesis in progress. Call stop() first.");
		return Ref<AudioStreamWAV>();
	}
	last_synthesis_result_.clear();

	EffectiveRequest request;
	String error_message;
	if (!build_effective_request(*this, request_dictionary, request, error_message)) {
		UtilityFunctions::push_error(error_message);
		return Ref<AudioStreamWAV>();
	}

	if ((request.has_text && request.text.is_empty()) ||
			(request.has_phoneme_string && request.phoneme_string.is_empty())) {
		UtilityFunctions::push_error("PiperTTS: Request input must not be empty.");
		return Ref<AudioStreamWAV>();
	}

	ScopedVoiceSynthesisConfig scoped_config(*voice);
	if (apply_effective_request_to_voice(*voice, request, error_message) != OK) {
		UtilityFunctions::push_error(error_message);
		return Ref<AudioStreamWAV>();
	}

	std::vector<int16_t> audio_buffer;
	piper::SynthesisResult result;

	try {
		if (request.has_phoneme_string) {
			std::vector<piper::Phoneme> phonemes =
					parse_effective_phoneme_string(*voice, request.phoneme_string);
			piper::phonemesToAudio(*piper_config, *voice, phonemes, audio_buffer, result, nullptr);
		} else {
			piper::textToAudio(*piper_config, *voice, request.text.utf8().get_data(),
					audio_buffer, result, nullptr);
		}
	} catch (const std::exception &e) {
		UtilityFunctions::push_error(
				String("PiperTTS: Synthesis failed -- ") + String(e.what()));
		return Ref<AudioStreamWAV>();
	}

	if (audio_buffer.empty()) {
		UtilityFunctions::push_warning("PiperTTS: Synthesis produced no audio.");
		return Ref<AudioStreamWAV>();
	}

	int sample_rate = voice->synthesisConfig.sampleRate;
	last_synthesis_result_ = synthesis_result_to_dictionary(result, sample_rate);
	last_synthesis_result_["input_mode"] = request.has_phoneme_string ? "phoneme_string" : "text";
	last_synthesis_result_["sentence_silence_seconds"] = voice->synthesisConfig.sentenceSilenceSeconds;
	last_synthesis_result_["phoneme_silence_seconds"] = request.phoneme_silence_seconds;
	return create_audio_stream(audio_buffer, sample_rate);
}

Ref<AudioStreamWAV> PiperTTS::synthesize_phoneme_string(const String &phoneme_string) {
	Dictionary request;
	request["phoneme_string"] = phoneme_string;
	return synthesize_request(request);
}

bool PiperTTS::is_ready() const {
	return ready;
}

Dictionary PiperTTS::inspect_text(const String &text) {
	Dictionary request;
	request["text"] = text;
	return inspect_request(request);
}

Dictionary PiperTTS::inspect_request(const Dictionary &request_dictionary) {
	Dictionary empty_result;
	if (!ready) {
		UtilityFunctions::push_error("PiperTTS: Not initialized. Call initialize() first.");
		return empty_result;
	}
	if (processing.load() || streaming_active_.load()) {
		UtilityFunctions::push_error("PiperTTS: Cannot inspect while synthesis is in progress.");
		return empty_result;
	}
	last_inspection_result_.clear();

	EffectiveRequest request;
	String error_message;
	if (!build_effective_request(*this, request_dictionary, request, error_message)) {
		UtilityFunctions::push_error(error_message);
		return empty_result;
	}

	if ((request.has_text && request.text.is_empty()) ||
			(request.has_phoneme_string && request.phoneme_string.is_empty())) {
		UtilityFunctions::push_error("PiperTTS: Request input must not be empty.");
		return empty_result;
	}

	ScopedVoiceSynthesisConfig scoped_config(*voice);
	if (apply_effective_request_to_voice(*voice, request, error_message) != OK) {
		UtilityFunctions::push_error(error_message);
		return empty_result;
	}

	piper::InspectionResult inspection_result;
	try {
		if (request.has_phoneme_string) {
			std::vector<piper::Phoneme> phonemes =
					parse_effective_phoneme_string(*voice, request.phoneme_string);
			piper::inspectPhonemes(*voice, phonemes, inspection_result);
		} else {
			piper::inspectText(*piper_config, *voice, request.text.utf8().get_data(),
					inspection_result);
		}
	} catch (const std::exception &e) {
		UtilityFunctions::push_error(
				String("PiperTTS: Inspection failed -- ") + String(e.what()));
		return empty_result;
	}

	last_inspection_result_ = inspection_result_to_dictionary(inspection_result, *voice);
	last_inspection_result_["input_mode"] = request.has_phoneme_string ? "phoneme_string" : "text";
	last_inspection_result_["sentence_silence_seconds"] = voice->synthesisConfig.sentenceSilenceSeconds;
	last_inspection_result_["phoneme_silence_seconds"] = request.phoneme_silence_seconds;
	return last_inspection_result_;
}

Dictionary PiperTTS::inspect_phoneme_string(const String &phoneme_string) {
	Dictionary request;
	request["phoneme_string"] = phoneme_string;
	return inspect_request(request);
}

Dictionary PiperTTS::get_last_synthesis_result() const {
	return last_synthesis_result_;
}

Dictionary PiperTTS::get_last_inspection_result() const {
	return last_inspection_result_;
}

// ---------------------------------------------------------------------------
// Public methods (M3: async)
// ---------------------------------------------------------------------------

Error PiperTTS::synthesize_async(const String &text) {
	if (!ready) {
		UtilityFunctions::push_error("PiperTTS: Not initialized. Call initialize() first.");
		return ERR_UNCONFIGURED;
	}

	if (text.is_empty()) {
		UtilityFunctions::push_error("PiperTTS: Empty text provided.");
		return ERR_INVALID_PARAMETER;
	}

	if (processing.load() || streaming_active_.load()) {
		UtilityFunctions::push_error("PiperTTS: Already processing. Call stop() first or wait for completion.");
		return ERR_BUSY;
	}
	last_synthesis_result_.clear();

	EffectiveRequest request;
	request.has_text = true;
	request.text = text;
	request.speaker_id = speaker_id;
	request.language_id = language_id;
	request.language_code = language_code;
	request.speech_rate = speech_rate;
	request.noise_scale = noise_scale;
	request.noise_w = noise_w;
	request.sentence_silence_seconds = sentence_silence_seconds;
	request.phoneme_silence_seconds = phoneme_silence_seconds;

	String language_error;
	if (apply_effective_request_to_voice(*voice, request, language_error) != OK) {
		UtilityFunctions::push_error(language_error);
		return ERR_INVALID_PARAMETER;
	}

	// Join any previous (completed) worker thread
	_join_worker_thread();
	{
		std::lock_guard<std::mutex> lock(pending_async_result_mutex_);
		pending_async_result_.reset();
		pending_async_result_metadata_.clear();
		pending_async_result_metadata_["input_mode"] = "text";
		pending_async_result_metadata_["sentence_silence_seconds"] =
				request.sentence_silence_seconds;
		pending_async_result_metadata_["phoneme_silence_seconds"] =
				request.phoneme_silence_seconds;
	}

	processing.store(true);
	stop_requested.store(false);
	uint32_t gen = ++synthesis_generation_;

	std::string text_str = text.utf8().get_data();
	worker_thread = std::make_unique<std::thread>(
			&PiperTTS::_synthesis_thread_func, this, std::move(text_str), gen);

	return OK;
}

void PiperTTS::stop() {
	bool was_processing = processing.load();
	bool was_streaming = streaming_active_.load();

	if (!was_processing && !was_streaming) {
		return;
	}

	++synthesis_generation_; // Invalidate pending call_deferred
	stop_requested.store(true);

	if (was_processing) {
		_join_worker_thread();
		processing.store(false);
		std::lock_guard<std::mutex> lock(pending_async_result_mutex_);
		pending_async_result_.reset();
		pending_async_result_metadata_.clear();
	}

	if (was_streaming) {
		streaming_active_.store(false);
		audio_chunk_queue_.clear();
		pending_samples_.clear();
		pending_sample_offset_ = 0;
		streaming_playback_ = Ref<AudioStreamGeneratorPlayback>();
		std::lock_guard<std::mutex> lock(pending_async_result_mutex_);
		pending_async_result_.reset();
		pending_async_result_metadata_.clear();
		set_process(false);
	}
}

bool PiperTTS::is_processing() const {
	return processing.load() || streaming_active_.load();
}

// ---------------------------------------------------------------------------
// Async internal methods
// ---------------------------------------------------------------------------

void PiperTTS::_join_worker_thread() {
	if (worker_thread && worker_thread->joinable()) {
		worker_thread->join();
	}
	worker_thread.reset();
}

void PiperTTS::_synthesis_thread_func(std::string text_str, uint32_t generation) {
	std::vector<int16_t> audio_buffer;
	piper::SynthesisResult result;

	try {
		piper::textToAudio(*piper_config, *voice, text_str,
				audio_buffer, result, nullptr);
	} catch (const std::exception &e) {
		if (!stop_requested.load()) {
			call_deferred("_on_synthesis_failed", String(e.what()), generation);
		} else {
			processing.store(false);
		}
		return;
	}

	// Check if stop was requested during synthesis
	if (stop_requested.load()) {
		processing.store(false);
		return;
	}

	if (audio_buffer.empty()) {
		call_deferred("_on_synthesis_failed", String("Synthesis produced no audio."), generation);
		return;
	}

	int sample_rate = voice->synthesisConfig.sampleRate;
	{
		std::lock_guard<std::mutex> lock(pending_async_result_mutex_);
		pending_async_result_ = std::make_unique<piper::SynthesisResult>(result);
	}

	// Pack raw PCM data for main-thread delivery (no Godot objects on worker thread)
	PackedByteArray pcm_data;
	pcm_data.resize(audio_buffer.size() * sizeof(int16_t));
	memcpy(pcm_data.ptrw(), audio_buffer.data(), pcm_data.size());

	call_deferred("_on_synthesis_raw_done", pcm_data, sample_rate, generation);
	// processing is cleared in the deferred handler, not here
}

void PiperTTS::_on_synthesis_raw_done(const PackedByteArray &pcm_data, int sample_rate, uint32_t generation) {
	if (generation != synthesis_generation_.load()) {
		// Stale request — discard silently
		processing.store(false);
		return;
	}

	Ref<AudioStreamWAV> stream;
	stream.instantiate();
	stream->set_format(AudioStreamWAV::FORMAT_16_BITS);
	stream->set_mix_rate(sample_rate);
	stream->set_stereo(false);
	stream->set_data(pcm_data);

	{
		std::lock_guard<std::mutex> lock(pending_async_result_mutex_);
		if (pending_async_result_) {
			last_synthesis_result_ = synthesis_result_to_dictionary(
					*pending_async_result_, sample_rate);
			Array metadata_keys = pending_async_result_metadata_.keys();
			for (int i = 0; i < metadata_keys.size(); ++i) {
				Variant key = metadata_keys[i];
				last_synthesis_result_[key] = pending_async_result_metadata_[key];
			}
		}
		pending_async_result_.reset();
		pending_async_result_metadata_.clear();
	}

	processing.store(false);
	emit_signal("synthesis_completed", stream);
}

void PiperTTS::_on_synthesis_failed(const String &error_msg, uint32_t generation) {
	if (generation != synthesis_generation_.load()) {
		processing.store(false);
		return;
	}
	processing.store(false);
	UtilityFunctions::push_error(String("PiperTTS: ") + error_msg);
	last_synthesis_result_.clear();
	{
		std::lock_guard<std::mutex> lock(pending_async_result_mutex_);
		pending_async_result_.reset();
		pending_async_result_metadata_.clear();
	}
	emit_signal("synthesis_failed", error_msg);
}

// ---------------------------------------------------------------------------
// Public methods (M6: streaming)
// ---------------------------------------------------------------------------

Error PiperTTS::synthesize_streaming(
		const String &text, const Ref<AudioStreamGeneratorPlayback> &playback) {
	if (!ready) {
		UtilityFunctions::push_error("PiperTTS: Not initialized. Call initialize() first.");
		return ERR_UNCONFIGURED;
	}

	if (text.is_empty()) {
		UtilityFunctions::push_error("PiperTTS: Empty text provided.");
		return ERR_INVALID_PARAMETER;
	}

	if (!playback.is_valid()) {
		UtilityFunctions::push_error("PiperTTS: Invalid AudioStreamGeneratorPlayback.");
		return ERR_INVALID_PARAMETER;
	}

	if (processing.load() || streaming_active_.load()) {
		UtilityFunctions::push_error("PiperTTS: Already processing. Call stop() first.");
		return ERR_BUSY;
	}
	last_synthesis_result_.clear();

	EffectiveRequest request;
	request.has_text = true;
	request.text = text;
	request.speaker_id = speaker_id;
	request.language_id = language_id;
	request.language_code = language_code;
	request.speech_rate = speech_rate;
	request.noise_scale = noise_scale;
	request.noise_w = noise_w;
	request.sentence_silence_seconds = sentence_silence_seconds;
	request.phoneme_silence_seconds = phoneme_silence_seconds;

	String language_error;
	if (apply_effective_request_to_voice(*voice, request, language_error) != OK) {
		UtilityFunctions::push_error(language_error);
		return ERR_INVALID_PARAMETER;
	}

	_join_worker_thread();
	{
		std::lock_guard<std::mutex> lock(pending_async_result_mutex_);
		pending_async_result_.reset();
		pending_async_result_metadata_.clear();
		pending_async_result_metadata_["input_mode"] = "text";
		pending_async_result_metadata_["sentence_silence_seconds"] =
				request.sentence_silence_seconds;
		pending_async_result_metadata_["phoneme_silence_seconds"] =
				request.phoneme_silence_seconds;
	}

	streaming_playback_ = playback;
	audio_chunk_queue_.clear();
	pending_samples_.clear();
	pending_sample_offset_ = 0;

	processing.store(true);
	stop_requested.store(false);
	streaming_active_.store(true);
	uint32_t gen = ++synthesis_generation_;
	set_process(true);

	std::string text_str = text.utf8().get_data();
	worker_thread = std::make_unique<std::thread>(
			&PiperTTS::_streaming_thread_func, this, std::move(text_str), gen);

	return OK;
}

void PiperTTS::_process(double p_delta) {
	if (!streaming_active_.load()) {
		return;
	}

	if (!streaming_playback_.is_valid()) {
		streaming_active_.store(false);
		set_process(false);
		return;
	}

	// Pop chunks from queue into pending buffer
	std::vector<int16_t> chunk;
	while (audio_chunk_queue_.pop(chunk)) {
		pending_samples_.insert(pending_samples_.end(), chunk.begin(), chunk.end());
	}

	// Push samples to playback
	_push_pending_samples();

	// Check if streaming is complete (worker done + queue empty + all samples pushed)
	if (!processing.load() && audio_chunk_queue_.empty() &&
			pending_sample_offset_ >= pending_samples_.size()) {
		{
			std::lock_guard<std::mutex> lock(pending_async_result_mutex_);
			if (pending_async_result_) {
				last_synthesis_result_ =
						synthesis_result_to_dictionary(*pending_async_result_,
								voice->synthesisConfig.sampleRate);
				Array metadata_keys = pending_async_result_metadata_.keys();
				for (int i = 0; i < metadata_keys.size(); ++i) {
					Variant key = metadata_keys[i];
					last_synthesis_result_[key] = pending_async_result_metadata_[key];
				}
			}
			pending_async_result_.reset();
			pending_async_result_metadata_.clear();
		}
		pending_samples_.clear();
		pending_sample_offset_ = 0;
		streaming_active_.store(false);
		streaming_playback_ = Ref<AudioStreamGeneratorPlayback>();
		set_process(false);
		emit_signal("streaming_ended");
	}
}

// ---------------------------------------------------------------------------
// Streaming internal methods (M6)
// ---------------------------------------------------------------------------

void PiperTTS::_streaming_thread_func(std::string text_str, uint32_t generation) {
	std::vector<int16_t> audioBuffer;
	piper::SynthesisResult result;

	try {
		auto callback = [&audioBuffer, this]() {
			if (stop_requested.load()) {
				return;
			}
			if (!audioBuffer.empty()) {
				audio_chunk_queue_.push(std::vector<int16_t>(audioBuffer));
			}
		};

		piper::textToAudio(*piper_config, *voice, text_str,
				audioBuffer, result, callback);
	} catch (const std::exception &e) {
		if (!stop_requested.load()) {
			call_deferred("_on_synthesis_failed", String(e.what()), generation);
		}
		streaming_active_.store(false);
		processing.store(false);
		return;
	}

	// Push any remaining audio not captured by callback
	// (happens when audioCallback is provided but text has only one sentence,
	//  or if the last sentence's callback was called but audioBuffer wasn't cleared)
	if (!audioBuffer.empty() && !stop_requested.load()) {
		audio_chunk_queue_.push(std::move(audioBuffer));
	}
	if (!stop_requested.load()) {
		std::lock_guard<std::mutex> lock(pending_async_result_mutex_);
		pending_async_result_ = std::make_unique<piper::SynthesisResult>(result);
	}

	processing.store(false);
}

void PiperTTS::_push_pending_samples() {
	if (pending_sample_offset_ >= pending_samples_.size()) {
		return;
	}
	if (!streaming_playback_.is_valid()) {
		return;
	}

	int frames_available = streaming_playback_->get_frames_available();
	if (frames_available <= 0) {
		return;
	}

	int remaining = static_cast<int>(pending_samples_.size()) -
					static_cast<int>(pending_sample_offset_);
	int to_push = std::min(remaining, frames_available);

	PackedVector2Array frames;
	frames.resize(to_push);
	Vector2 *frames_ptr = frames.ptrw();
	const int16_t *samples_ptr = pending_samples_.data() + pending_sample_offset_;

	for (int i = 0; i < to_push; i++) {
		float s = static_cast<float>(samples_ptr[i]) / 32768.0f;
		frames_ptr[i] = Vector2(s, s);
	}

	streaming_playback_->push_buffer(frames);
	pending_sample_offset_ += to_push;

	// Compact when fully consumed
	if (pending_sample_offset_ >= pending_samples_.size()) {
		pending_samples_.clear();
		pending_sample_offset_ = 0;
	}
}

} // namespace godot
