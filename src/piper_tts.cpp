#include "piper_tts.h"

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "piper_core/custom_dictionary.hpp"
#include "piper_core/openjtalk_dictionary_manager.h"
#include "piper_core/phoneme_parser.hpp"
#include "piper_core/piper.hpp"
#include "piper_language_support.hpp"
#include "piper_runtime_support.hpp"
#include "piper_tts_paths.hpp"

extern "C" {
	void openjtalk_set_dictionary_path(const char* path);
	void openjtalk_set_library_path(const char* path);
}

#include <cstring>
#include <functional>
#include <vector>

namespace godot {
namespace {

using RuntimeErrorInfo = piper_runtime::RuntimeErrorInfo;
using EffectiveRequest = piper_runtime::EffectiveRequest;

void set_last_error(RuntimeErrorInfo &target, const RuntimeErrorInfo &error) {
	target = error;
}

void set_last_error(RuntimeErrorInfo &target, const String &code, const String &message,
		const String &stage, const String &requested_language_code = String(),
		const String &resolved_language_code = String(), int64_t requested_language_id = -1,
		int64_t resolved_language_id = -1, const String &selection_mode = String()) {
	piper_runtime::set_runtime_error(target, code, message, stage,
			requested_language_code, resolved_language_code,
			requested_language_id, resolved_language_id, selection_mode);
}

void clear_last_error(RuntimeErrorInfo &target) {
	piper_runtime::clear_runtime_error(target);
}

bool language_code_is_japanese(const String &language_code) {
	return language_code.strip_edges().to_lower().begins_with("ja");
}

String effective_openjtalk_dictionary_path(const String &resolved_dictionary_path) {
	if (!resolved_dictionary_path.is_empty()) {
		return resolved_dictionary_path;
	}

	if (piper_tts_paths::is_web_runtime()) {
		return String();
	}

	const char *default_path = get_openjtalk_dictionary_path();
	if (default_path == nullptr || default_path[0] == '\0') {
		return String();
	}

	return String(default_path);
}

bool openjtalk_dictionary_ready(const String &resolved_dictionary_path) {
	const String effective_path =
			effective_openjtalk_dictionary_path(resolved_dictionary_path);
	if (effective_path.is_empty()) {
		return false;
	}

	const CharString utf8_path = effective_path.utf8();
	return openjtalk_dictionary_path_is_ready(utf8_path.get_data()) != 0;
}

bool validate_japanese_text_frontend(RuntimeErrorInfo &target,
		const EffectiveRequest &request, const String &resolved_dictionary_path,
		const String &stage) {
	if (!request.has_text || !language_code_is_japanese(request.resolved_language_code) ||
			openjtalk_dictionary_ready(resolved_dictionary_path)) {
		return true;
	}

	const bool web_runtime = piper_tts_paths::is_web_runtime();
	const String message = web_runtime
			? "PiperTTS: Japanese text input on Web requires a staged OpenJTalk dictionary asset."
			: "PiperTTS: Japanese text input requires an OpenJTalk dictionary.";
	set_last_error(target, "ERR_OPENJTALK_DICTIONARY_NOT_READY", message, stage,
			request.language_code, request.resolved_language_code,
			request.language_id, request.resolved_language_id,
			request.selection_mode);
	return false;
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

	// --- Property: openjtalk_library_path ---
	ClassDB::bind_method(D_METHOD("set_openjtalk_library_path", "path"), &PiperTTS::set_openjtalk_library_path);
	ClassDB::bind_method(D_METHOD("get_openjtalk_library_path"), &PiperTTS::get_openjtalk_library_path);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "openjtalk_library_path", PROPERTY_HINT_FILE, "*.dll,*.so,*.dylib"),
			"set_openjtalk_library_path", "get_openjtalk_library_path");

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
	ADD_PROPERTY(PropertyInfo(Variant::INT, "execution_provider", PROPERTY_HINT_ENUM, "CPU:0,CoreML:1,DirectML:2,NNAPI:3,Auto:4,CUDA:5"),
			"set_execution_provider", "get_execution_provider");

	// --- Property: gpu_device_id ---
	ClassDB::bind_method(D_METHOD("set_gpu_device_id", "device_id"), &PiperTTS::set_gpu_device_id);
	ClassDB::bind_method(D_METHOD("get_gpu_device_id"), &PiperTTS::get_gpu_device_id);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "gpu_device_id", PROPERTY_HINT_RANGE, "0,255,1"),
			"set_gpu_device_id", "get_gpu_device_id");

	// --- Enum constants: ExecutionProviderGD ---
	BIND_ENUM_CONSTANT(EP_CPU);
	BIND_ENUM_CONSTANT(EP_COREML);
	BIND_ENUM_CONSTANT(EP_DIRECTML);
	BIND_ENUM_CONSTANT(EP_NNAPI);
	BIND_ENUM_CONSTANT(EP_AUTO);
	BIND_ENUM_CONSTANT(EP_CUDA);

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
	ClassDB::bind_method(D_METHOD("get_language_capabilities"), &PiperTTS::get_language_capabilities);
	ClassDB::bind_method(D_METHOD("get_runtime_contract"), &PiperTTS::get_runtime_contract);
	ClassDB::bind_method(D_METHOD("get_runtime_state"), &PiperTTS::get_runtime_state);
	ClassDB::bind_method(D_METHOD("get_last_error"), &PiperTTS::get_last_error);

	// --- Methods (M3: async) ---
	ClassDB::bind_method(D_METHOD("synthesize_async", "text"), &PiperTTS::synthesize_async);
	ClassDB::bind_method(D_METHOD("synthesize_async_request", "request"), &PiperTTS::synthesize_async_request);
	ClassDB::bind_method(D_METHOD("stop"), &PiperTTS::stop);
	ClassDB::bind_method(D_METHOD("is_processing"), &PiperTTS::is_processing);

	// Internal methods for call_deferred (worker thread → main thread)
	ClassDB::bind_method(D_METHOD("_on_synthesis_raw_done", "pcm_data", "sample_rate", "generation"), &PiperTTS::_on_synthesis_raw_done);
	ClassDB::bind_method(D_METHOD("_on_synthesis_failed", "error", "generation"), &PiperTTS::_on_synthesis_failed);

	// --- Methods (M6: streaming) ---
	ClassDB::bind_method(D_METHOD("synthesize_streaming", "text", "playback"), &PiperTTS::synthesize_streaming);
	ClassDB::bind_method(D_METHOD("synthesize_streaming_request", "request", "playback"),
			&PiperTTS::synthesize_streaming_request);

	// --- Signals ---
	ADD_SIGNAL(MethodInfo("initialized", PropertyInfo(Variant::BOOL, "success")));
	ADD_SIGNAL(MethodInfo("synthesis_completed",
			PropertyInfo(Variant::OBJECT, "audio", PROPERTY_HINT_RESOURCE_TYPE, "AudioStreamWAV")));
	ADD_SIGNAL(MethodInfo("synthesis_failed",
			PropertyInfo(Variant::STRING, "error")));
	ADD_SIGNAL(MethodInfo("runtime_state_changed",
			PropertyInfo(Variant::STRING, "state")));
	ADD_SIGNAL(MethodInfo("synthesis_failed_detailed",
			PropertyInfo(Variant::DICTIONARY, "error")));

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

void PiperTTS::set_openjtalk_library_path(const String &p_path) {
	openjtalk_library_path = p_path;
}

String PiperTTS::get_openjtalk_library_path() const {
	return openjtalk_library_path;
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
	execution_provider = CLAMP(p_ep, 0, 5);
}

int PiperTTS::get_execution_provider() const {
	return execution_provider;
}

void PiperTTS::set_gpu_device_id(int p_id) {
	gpu_device_id = MAX(p_id, 0);
}

int PiperTTS::get_gpu_device_id() const {
	return gpu_device_id;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

String PiperTTS::resolve_path(const String &path) const {
	return piper_tts_paths::resolve_global_path(path);
}

String PiperTTS::resolve_model_path(const String &path) const {
	static const std::vector<String> model_roots = {
		"user://piper/models",
		"res://piper_plus_assets/models",
		"res://addons/piper_plus/models",
	};
	return piper_tts_paths::resolve_model_path(path, model_roots);
}

String PiperTTS::resolve_config_path(const String &resolved_model_path) const {
	return piper_tts_paths::resolve_config_path(config_path, resolved_model_path);
}

piper_runtime::RuntimePropertySnapshot PiperTTS::build_runtime_property_snapshot() const {
	piper_runtime::RuntimePropertySnapshot snapshot;
	snapshot.speaker_id = speaker_id;
	snapshot.language_id = language_id;
	snapshot.language_code = language_code;
	snapshot.speech_rate = speech_rate;
	snapshot.noise_scale = noise_scale;
	snapshot.noise_w = noise_w;
	snapshot.sentence_silence_seconds = sentence_silence_seconds;
	snapshot.phoneme_silence_seconds = phoneme_silence_seconds;
	return snapshot;
}

void PiperTTS::set_runtime_state(piper_runtime::RuntimeState state) {
	if (runtime_state_.load() == state) {
		return;
	}

	runtime_state_.store(state);
	emit_signal("runtime_state_changed",
			piper_runtime::runtime_state_to_string(runtime_state_.load()));
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

// ---------------------------------------------------------------------------
// Public methods (M2: sync)
// ---------------------------------------------------------------------------

Error PiperTTS::initialize() {
	if (processing.load() || streaming_active_.load()) {
		UtilityFunctions::push_error("PiperTTS: Cannot initialize while synthesis is in progress.");
		set_last_error(last_error_, "ERR_BUSY", "PiperTTS: Cannot initialize while synthesis is in progress.",
				"initialize");
		emit_signal("initialized", false);
		return ERR_BUSY;
	}

	if (ready) {
		piper::terminate(*piper_config);
		ready = false;
	}
	last_synthesis_result_.clear();
	last_inspection_result_.clear();
	resolved_dictionary_path_ = String();
	clear_last_error(last_error_);
	set_runtime_state(piper_runtime::RuntimeState::Initializing);
	{
		std::lock_guard<std::mutex> lock(pending_async_result_mutex_);
		pending_async_result_.reset();
	}

	if (model_path.is_empty()) {
		UtilityFunctions::push_error("PiperTTS: model_path is not set.");
		set_last_error(last_error_, "ERR_UNCONFIGURED", "PiperTTS: model_path is not set.",
				"initialize");
		set_runtime_state(piper_runtime::RuntimeState::Uninitialized);
		emit_signal("initialized", false);
		return ERR_UNCONFIGURED;
	}

	const bool web_runtime = piper_tts_paths::is_web_runtime();
	String resolved_model_source =
			web_runtime ? piper_tts_paths::resolve_web_model_source(model_path)
						: resolve_model_path(model_path);
	if (resolved_model_source.is_empty()) {
		UtilityFunctions::push_error(
				web_runtime
						? "PiperTTS: model_path could not be resolved to a valid .onnx model resource."
						: "PiperTTS: model_path could not be resolved to a valid .onnx model file.");
		set_last_error(last_error_, "ERR_CANT_OPEN",
				web_runtime
						? "PiperTTS: model_path could not be resolved to a valid .onnx model resource."
						: "PiperTTS: model_path could not be resolved to a valid .onnx model file.",
				"initialize");
		set_runtime_state(piper_runtime::RuntimeState::Uninitialized);
		emit_signal("initialized", false);
		return ERR_CANT_OPEN;
	}
	String resolved_config_source = web_runtime
			? piper_tts_paths::resolve_web_config_source(config_path, resolved_model_source)
			: resolve_config_path(resolved_model_source);

	if (resolved_config_source.is_empty()) {
		UtilityFunctions::push_error(
				web_runtime
						? "PiperTTS: config_path is not set and no fallback config resource was found next to the model."
						: "PiperTTS: config_path is not set and no fallback config was found next to the model.");
		set_last_error(last_error_, "ERR_UNCONFIGURED",
				web_runtime
						? "PiperTTS: config_path is not set and no fallback config resource was found next to the model."
						: "PiperTTS: config_path is not set and no fallback config was found next to the model.",
				"initialize");
		set_runtime_state(piper_runtime::RuntimeState::Uninitialized);
		emit_signal("initialized", false);
		return ERR_UNCONFIGURED;
	}

	// Configure optional openjtalk-native backend path before dictionary setup.
	if (!openjtalk_library_path.is_empty()) {
		String abs_openjtalk_library = resolve_path(openjtalk_library_path);
		std::string library_str = abs_openjtalk_library.utf8().get_data();
		openjtalk_set_library_path(library_str.c_str());
		UtilityFunctions::print(String("PiperTTS: OpenJTalk native library path set to: ") +
				abs_openjtalk_library);
	} else {
		openjtalk_set_library_path(nullptr);
	}

	String resolved_dictionary_source;
	if (web_runtime) {
		resolved_dictionary_source = piper_tts_paths::resolve_web_dictionary_source(
				dictionary_path, resolved_model_source, resolved_config_source);
		if (!resolved_dictionary_source.is_empty()) {
			String web_dictionary_error;
			String staged_dictionary_path;
			if (!piper_tts_paths::stage_web_dictionary_to_user(
						resolved_dictionary_source, staged_dictionary_path,
						web_dictionary_error)) {
				UtilityFunctions::push_error(web_dictionary_error);
				set_last_error(last_error_, "ERR_OPENJTALK_DICTIONARY_NOT_READY",
						web_dictionary_error, "initialize");
				set_runtime_state(piper_runtime::RuntimeState::Uninitialized);
				emit_signal("initialized", false);
				return ERR_UNCONFIGURED;
			}
			resolved_dictionary_source = staged_dictionary_path;
		}
	} else if (!dictionary_path.is_empty()) {
		resolved_dictionary_source = resolve_path(dictionary_path);
	}

	if (!resolved_dictionary_source.is_empty()) {
		resolved_dictionary_path_ = resolved_dictionary_source;
		std::string dict_str = resolved_dictionary_source.utf8().get_data();
		openjtalk_set_dictionary_path(dict_str.c_str());
		UtilityFunctions::print(String("PiperTTS: OpenJTalk dictionary path set to: ") +
				resolved_dictionary_source);
	} else {
		resolved_dictionary_path_ = String();
		openjtalk_set_dictionary_path(nullptr);
	}

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

#if defined(__EMSCRIPTEN__)
		if (ep != EP_CPU) {
			const String message =
					"PiperTTS: Web export supports only EP_CPU execution_provider.";
			UtilityFunctions::push_error(message);
			set_last_error(last_error_, "ERR_UNSUPPORTED_EXECUTION_PROVIDER",
					message, "initialize");
			piper::terminate(*piper_config);
			voice->customDictionary.reset();
			set_runtime_state(piper_runtime::RuntimeState::Uninitialized);
			emit_signal("initialized", false);
			return ERR_UNAVAILABLE;
		}

		if (!openjtalk_library_path.is_empty()) {
			const String message =
					"PiperTTS: Web export does not support openjtalk-native shared libraries.";
			UtilityFunctions::push_error(message);
			set_last_error(last_error_, "ERR_OPENJTALK_NATIVE_UNSUPPORTED",
					message, "initialize");
			piper::terminate(*piper_config);
			voice->customDictionary.reset();
			set_runtime_state(piper_runtime::RuntimeState::Uninitialized);
			emit_signal("initialized", false);
			return ERR_UNAVAILABLE;
		}
#endif

		if (web_runtime) {
			std::vector<uint8_t> model_data;
			String web_error;
			if (!piper_tts_paths::read_web_file_bytes(
						resolved_model_source, model_data, web_error)) {
				throw std::runtime_error(web_error.utf8().get_data());
			}

			String config_json;
			if (!piper_tts_paths::read_web_file_text(
						resolved_config_source, config_json, web_error)) {
				throw std::runtime_error(web_error.utf8().get_data());
			}

			std::optional<std::string> cmu_dict_json;
			std::string cmu_dict_source_label;
			const String cmu_dict_source =
					piper_tts_paths::find_web_cmudict_source(
							resolved_model_source, resolved_config_source);
			if (!cmu_dict_source.is_empty()) {
				String cmu_dict_text;
				if (!piper_tts_paths::read_web_file_text(
							cmu_dict_source, cmu_dict_text, web_error)) {
					throw std::runtime_error(web_error.utf8().get_data());
				}
				cmu_dict_json = cmu_dict_text.utf8().get_data();
				cmu_dict_source_label = cmu_dict_source.utf8().get_data();
			}

			std::optional<std::string> pinyin_single_dict_json;
			std::string pinyin_single_dict_source_label;
			const String pinyin_single_dict_source =
					piper_tts_paths::find_web_pinyin_single_dict_source(
							resolved_model_source, resolved_config_source);
			if (!pinyin_single_dict_source.is_empty()) {
				String pinyin_single_dict_text;
				if (!piper_tts_paths::read_web_file_text(
							pinyin_single_dict_source, pinyin_single_dict_text, web_error)) {
					throw std::runtime_error(web_error.utf8().get_data());
				}
				pinyin_single_dict_json = pinyin_single_dict_text.utf8().get_data();
				pinyin_single_dict_source_label =
						pinyin_single_dict_source.utf8().get_data();
			}

			std::optional<std::string> pinyin_phrase_dict_json;
			std::string pinyin_phrase_dict_source_label;
			const String pinyin_phrase_dict_source =
					piper_tts_paths::find_web_pinyin_phrase_dict_source(
							resolved_model_source, resolved_config_source);
			if (!pinyin_phrase_dict_source.is_empty()) {
				String pinyin_phrase_dict_text;
				if (!piper_tts_paths::read_web_file_text(
							pinyin_phrase_dict_source, pinyin_phrase_dict_text, web_error)) {
					throw std::runtime_error(web_error.utf8().get_data());
				}
				pinyin_phrase_dict_json = pinyin_phrase_dict_text.utf8().get_data();
				pinyin_phrase_dict_source_label =
						pinyin_phrase_dict_source.utf8().get_data();
			}

			piper::loadVoice(*piper_config, std::move(model_data),
					resolved_model_source.utf8().get_data(), config_json.utf8().get_data(),
					resolved_config_source.utf8().get_data(), *voice, sid, ep, gpu_device_id,
					cmu_dict_json, cmu_dict_source_label,
					pinyin_single_dict_json, pinyin_single_dict_source_label,
					pinyin_phrase_dict_json, pinyin_phrase_dict_source_label);
		} else {
			piper::loadVoice(*piper_config, resolved_model_source.utf8().get_data(),
					resolved_config_source.utf8().get_data(), *voice, sid, ep, gpu_device_id);
		}

		EffectiveRequest language_request;
		language_request.language_id = language_id;
		language_request.language_code = language_code;
		RuntimeErrorInfo language_error;
		if (piper_language::resolve_requested_language(*voice, language_id, language_code,
					language_request, "initialize", language_error) != OK) {
			UtilityFunctions::push_error(language_error.message);
			set_last_error(last_error_, language_error);
			piper::terminate(*piper_config);
			voice->customDictionary.reset();
			set_runtime_state(piper_runtime::RuntimeState::Uninitialized);
			emit_signal("initialized", false);
			return ERR_INVALID_PARAMETER;
		}
		voice->synthesisConfig.languageId = language_request.resolved_language_id >= 0
				? std::make_optional(static_cast<piper::LanguageId>(language_request.resolved_language_id))
				: std::nullopt;
		if (language_code_is_japanese(language_request.resolved_language_code) &&
				!openjtalk_dictionary_ready(resolved_dictionary_path_)) {
			const String message = web_runtime
					? "PiperTTS: Japanese text input on Web requires a staged OpenJTalk dictionary asset. Set dictionary_path or stage res://piper_plus_assets/dictionaries/open_jtalk_dic_utf_8-1.11."
					: "PiperTTS: Japanese text input requires an OpenJTalk dictionary. Set dictionary_path to a compiled naist-jdic directory.";
			UtilityFunctions::push_error(message);
			set_last_error(last_error_, "ERR_OPENJTALK_DICTIONARY_NOT_READY", message,
					"initialize", language_request.language_code,
					language_request.resolved_language_code, language_request.language_id,
					language_request.resolved_language_id,
					language_request.selection_mode);
			piper::terminate(*piper_config);
			voice->customDictionary.reset();
			set_runtime_state(piper_runtime::RuntimeState::Uninitialized);
			emit_signal("initialized", false);
			return ERR_UNCONFIGURED;
		}

		ready = true;
		if (model_path != resolved_model_source) {
			UtilityFunctions::print(String("PiperTTS: Model resolved to: ") + resolved_model_source);
		}
		if (config_path.is_empty()) {
			UtilityFunctions::print(String("PiperTTS: Config auto-resolved to: ") + resolved_config_source);
		}
		UtilityFunctions::print("PiperTTS: Voice loaded successfully.");
		clear_last_error(last_error_);
		set_runtime_state(piper_runtime::RuntimeState::Ready);
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
		set_last_error(last_error_, "ERR_CANT_OPEN",
				String("PiperTTS: Failed to initialize -- ") + String(e.what()), "initialize");
		ready = false;
		set_runtime_state(piper_runtime::RuntimeState::Uninitialized);
		emit_signal("initialized", false);
		return ERR_CANT_OPEN;
	}
}

Ref<AudioStreamWAV> PiperTTS::synthesize(const String &text) {
	if (!ready) {
		UtilityFunctions::push_error("PiperTTS: Not initialized. Call initialize() first.");
		set_last_error(last_error_, "ERR_UNCONFIGURED",
				"PiperTTS: Not initialized. Call initialize() first.", "synthesize", String(),
				String(), -1, -1, "text");
		return Ref<AudioStreamWAV>();
	}

	if (text.is_empty()) {
		UtilityFunctions::push_warning("PiperTTS: Empty text provided.");
		set_last_error(last_error_, "ERR_INVALID_PARAMETER",
				"PiperTTS: Empty text provided.", "synthesize", String(), String(), -1, -1,
				"text");
		return Ref<AudioStreamWAV>();
	}

	if (processing.load() || streaming_active_.load()) {
		UtilityFunctions::push_error("PiperTTS: Synthesis in progress. Call stop() first.");
		set_last_error(last_error_, "ERR_BUSY", "PiperTTS: Synthesis in progress. Call stop() first.",
				"synthesize", String(), String(), -1, -1, "text");
		return Ref<AudioStreamWAV>();
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

	piper::SynthesisConfig synthesis_config;
	if (piper_runtime::build_request_synthesis_config(
				*voice, request, synthesis_config, "synthesize", last_error_) != OK) {
		UtilityFunctions::push_error(last_error_.message);
		return Ref<AudioStreamWAV>();
	}
	if (!validate_japanese_text_frontend(
				last_error_, request, resolved_dictionary_path_, "synthesize")) {
		UtilityFunctions::push_error(last_error_.message);
		return Ref<AudioStreamWAV>();
	}

	std::string text_str = text.utf8().get_data();
	std::vector<int16_t> audio_buffer;
	piper::SynthesisResult result;

	try {
		piper::textToAudio(*piper_config, *voice, text_str, synthesis_config,
				audio_buffer, result, nullptr);
	} catch (const std::exception &e) {
		UtilityFunctions::push_error(
				String("PiperTTS: Synthesis failed -- ") + String(e.what()));
		set_last_error(last_error_, "ERR_SYNTHESIS_RUNTIME",
				String("PiperTTS: Synthesis failed -- ") + String(e.what()),
				"synthesize", request.language_code, request.resolved_language_code,
				request.language_id, request.resolved_language_id, request.selection_mode);
		return Ref<AudioStreamWAV>();
	}

	if (audio_buffer.empty()) {
		UtilityFunctions::push_warning("PiperTTS: Synthesis produced no audio.");
		set_last_error(last_error_, "ERR_SYNTHESIS_RUNTIME", "PiperTTS: Synthesis produced no audio.",
				"synthesize", request.language_code, request.resolved_language_code,
				request.language_id, request.resolved_language_id, request.selection_mode);
		return Ref<AudioStreamWAV>();
	}

	int sample_rate = synthesis_config.sampleRate;
	last_synthesis_result_ = piper_runtime::synthesis_result_to_dictionary(result, sample_rate);
	last_synthesis_result_["input_mode"] = "text";
	last_synthesis_result_["sentence_silence_seconds"] = synthesis_config.sentenceSilenceSeconds;
	last_synthesis_result_["phoneme_silence_seconds"] = phoneme_silence_seconds;
	piper_language::annotate_language_metadata(
			last_synthesis_result_, request, synthesis_config, *voice);
	clear_last_error(last_error_);
	return create_audio_stream(audio_buffer, sample_rate);
}

Ref<AudioStreamWAV> PiperTTS::synthesize_request(const Dictionary &request_dictionary) {
	if (!ready) {
		UtilityFunctions::push_error("PiperTTS: Not initialized. Call initialize() first.");
		set_last_error(last_error_, "ERR_UNCONFIGURED",
				"PiperTTS: Not initialized. Call initialize() first.", "synthesize_request");
		return Ref<AudioStreamWAV>();
	}

	if (processing.load() || streaming_active_.load()) {
		UtilityFunctions::push_error("PiperTTS: Synthesis in progress. Call stop() first.");
		set_last_error(last_error_, "ERR_BUSY", "PiperTTS: Synthesis in progress. Call stop() first.",
				"synthesize_request");
		return Ref<AudioStreamWAV>();
	}
	last_synthesis_result_.clear();

	EffectiveRequest request;
	piper_runtime::RuntimeErrorInfo request_error;
	if (!piper_runtime::build_effective_request(build_runtime_property_snapshot(),
				request_dictionary, request, "synthesize_request", request_error)) {
		UtilityFunctions::push_error(request_error.message);
		set_last_error(last_error_,
				request_error.code.is_empty() ? "ERR_REQUEST_INVALID" : request_error.code,
				request_error.message, "synthesize_request");
		return Ref<AudioStreamWAV>();
	}

	piper::SynthesisConfig synthesis_config;
	if (piper_runtime::build_request_synthesis_config(
				*voice, request, synthesis_config, "synthesize_request", last_error_) != OK) {
		UtilityFunctions::push_error(last_error_.message);
		return Ref<AudioStreamWAV>();
	}
	if (!validate_japanese_text_frontend(
				last_error_, request, resolved_dictionary_path_, "synthesize_request")) {
		UtilityFunctions::push_error(last_error_.message);
		return Ref<AudioStreamWAV>();
	}

	std::vector<int16_t> audio_buffer;
	piper::SynthesisResult result;

	try {
		if (request.has_phoneme_string) {
			std::vector<piper::Phoneme> phonemes =
					piper_runtime::parse_effective_phoneme_string(*voice, request.phoneme_string);
			piper::phonemesToAudio(*piper_config, *voice, phonemes, synthesis_config,
					audio_buffer, result, nullptr);
		} else {
			piper::textToAudio(*piper_config, *voice, request.text.utf8().get_data(), synthesis_config,
					audio_buffer, result, nullptr);
		}
	} catch (const std::exception &e) {
		UtilityFunctions::push_error(
				String("PiperTTS: Synthesis failed -- ") + String(e.what()));
		set_last_error(last_error_, "ERR_SYNTHESIS_RUNTIME",
				String("PiperTTS: Synthesis failed -- ") + String(e.what()),
				"synthesize_request", request.language_code, request.resolved_language_code,
				request.language_id, request.resolved_language_id, request.selection_mode);
		return Ref<AudioStreamWAV>();
	}

	if (audio_buffer.empty()) {
		UtilityFunctions::push_warning("PiperTTS: Synthesis produced no audio.");
		set_last_error(last_error_, "ERR_SYNTHESIS_RUNTIME", "PiperTTS: Synthesis produced no audio.",
				"synthesize_request", request.language_code, request.resolved_language_code,
				request.language_id, request.resolved_language_id, request.selection_mode);
		return Ref<AudioStreamWAV>();
	}

	int sample_rate = synthesis_config.sampleRate;
	last_synthesis_result_ = piper_runtime::synthesis_result_to_dictionary(result, sample_rate);
	last_synthesis_result_["input_mode"] = request.has_phoneme_string ? "phoneme_string" : "text";
	last_synthesis_result_["sentence_silence_seconds"] = synthesis_config.sentenceSilenceSeconds;
	last_synthesis_result_["phoneme_silence_seconds"] = request.phoneme_silence_seconds;
	piper_language::annotate_language_metadata(
			last_synthesis_result_, request, synthesis_config, *voice);
	clear_last_error(last_error_);
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
		set_last_error(last_error_, "ERR_UNCONFIGURED",
				"PiperTTS: Not initialized. Call initialize() first.", "inspect_request");
		return empty_result;
	}
	if (processing.load() || streaming_active_.load()) {
		UtilityFunctions::push_error("PiperTTS: Cannot inspect while synthesis is in progress.");
		set_last_error(last_error_, "ERR_BUSY",
				"PiperTTS: Cannot inspect while synthesis is in progress.", "inspect_request");
		return empty_result;
	}
	last_inspection_result_.clear();

	EffectiveRequest request;
	piper_runtime::RuntimeErrorInfo request_error;
	if (!piper_runtime::build_effective_request(build_runtime_property_snapshot(),
				request_dictionary, request, "inspect_request", request_error)) {
		UtilityFunctions::push_error(request_error.message);
		set_last_error(last_error_,
				request_error.code.is_empty() ? "ERR_REQUEST_INVALID" : request_error.code,
				request_error.message, "inspect_request");
		return empty_result;
	}

	piper::SynthesisConfig synthesis_config;
	if (piper_runtime::build_request_synthesis_config(
				*voice, request, synthesis_config, "inspect_request", last_error_) != OK) {
		UtilityFunctions::push_error(last_error_.message);
		return empty_result;
	}
	if (!validate_japanese_text_frontend(
				last_error_, request, resolved_dictionary_path_, "inspect_request")) {
		UtilityFunctions::push_error(last_error_.message);
		return empty_result;
	}

	piper::InspectionResult inspection_result;
	try {
		if (request.has_phoneme_string) {
			std::vector<piper::Phoneme> phonemes =
					piper_runtime::parse_effective_phoneme_string(*voice, request.phoneme_string);
			piper::inspectPhonemes(*voice, phonemes, synthesis_config, inspection_result);
		} else {
			piper::inspectText(*piper_config, *voice, request.text.utf8().get_data(), synthesis_config,
					inspection_result);
		}
	} catch (const std::exception &e) {
		UtilityFunctions::push_error(
				String("PiperTTS: Inspection failed -- ") + String(e.what()));
		set_last_error(last_error_, "ERR_SYNTHESIS_RUNTIME",
				String("PiperTTS: Inspection failed -- ") + String(e.what()), "inspect_request",
				request.language_code, request.resolved_language_code, request.language_id,
				request.resolved_language_id, request.selection_mode);
		return empty_result;
	}

	last_inspection_result_ =
			piper_runtime::inspection_result_to_dictionary(inspection_result, *voice);
	last_inspection_result_["input_mode"] = request.has_phoneme_string ? "phoneme_string" : "text";
	last_inspection_result_["sentence_silence_seconds"] = synthesis_config.sentenceSilenceSeconds;
	last_inspection_result_["phoneme_silence_seconds"] = request.phoneme_silence_seconds;
	piper_language::annotate_language_metadata(
			last_inspection_result_, request, synthesis_config, *voice);
	clear_last_error(last_error_);
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

Dictionary PiperTTS::get_language_capabilities() const {
	if (!ready || !voice) {
		return Dictionary();
	}
	return piper_language::build_language_capabilities(*voice);
}

Dictionary PiperTTS::get_runtime_contract() const {
	const bool web_runtime = piper_tts_paths::is_web_runtime();
	const String resolved_model_source = web_runtime
			? piper_tts_paths::resolve_web_model_source(model_path)
			: resolve_model_path(model_path);
	const String resolved_config_source = web_runtime
			? piper_tts_paths::resolve_web_config_source(
					  config_path, resolved_model_source)
			: resolve_config_path(resolved_model_source);
	const String contract_model_path =
			resolved_model_source.is_empty() ? model_path : resolved_model_source;
	const String contract_config_path =
			resolved_config_source.is_empty() ? config_path : resolved_config_source;
	return piper_runtime::build_runtime_contract(
			web_runtime, contract_model_path, contract_config_path,
			dictionary_path, openjtalk_library_path, custom_dictionary_path,
			execution_provider, runtime_state_.load());
}

String PiperTTS::get_runtime_state() const {
	return piper_runtime::runtime_state_to_string(runtime_state_.load());
}

Dictionary PiperTTS::get_last_error() const {
	return piper_runtime::runtime_error_to_dictionary(last_error_);
}

// ---------------------------------------------------------------------------
// Public methods (M3: async)
// ---------------------------------------------------------------------------

Error PiperTTS::synthesize_async(const String &text) {
	Dictionary request;
	request["text"] = text;
	return synthesize_async_request(request);
}

Error PiperTTS::synthesize_async_request(const Dictionary &request_dictionary) {
	if (!ready) {
		UtilityFunctions::push_error("PiperTTS: Not initialized. Call initialize() first.");
		set_last_error(last_error_, "ERR_UNCONFIGURED",
				"PiperTTS: Not initialized. Call initialize() first.", "synthesize_async_request");
		return ERR_UNCONFIGURED;
	}

	if (processing.load() || streaming_active_.load()) {
		UtilityFunctions::push_error("PiperTTS: Already processing. Call stop() first or wait for completion.");
		set_last_error(last_error_, "ERR_BUSY",
				"PiperTTS: Already processing. Call stop() first or wait for completion.",
				"synthesize_async_request");
		return ERR_BUSY;
	}
	last_synthesis_result_.clear();

	EffectiveRequest request;
	piper_runtime::RuntimeErrorInfo request_error;
	if (!piper_runtime::build_effective_request(build_runtime_property_snapshot(),
				request_dictionary, request, "synthesize_async_request", request_error)) {
		UtilityFunctions::push_error(request_error.message);
		set_last_error(last_error_,
				request_error.code.is_empty() ? "ERR_REQUEST_INVALID" : request_error.code,
				request_error.message, "synthesize_async_request");
		return ERR_INVALID_PARAMETER;
	}

	String language_error;
	String language_error_code;
	piper::SynthesisConfig synthesis_config;
	if (piper_runtime::build_request_synthesis_config(
				*voice, request, synthesis_config, "synthesize_async_request", last_error_) != OK) {
		UtilityFunctions::push_error(last_error_.message);
		return ERR_INVALID_PARAMETER;
	}
	if (!validate_japanese_text_frontend(
				last_error_, request, resolved_dictionary_path_, "synthesize_async_request")) {
		UtilityFunctions::push_error(last_error_.message);
		return ERR_UNCONFIGURED;
	}

	_join_worker_thread();
	{
		std::lock_guard<std::mutex> lock(pending_async_result_mutex_);
		pending_async_result_.reset();
		pending_async_result_metadata_.clear();
		pending_async_result_metadata_["input_mode"] =
				request.has_phoneme_string ? "phoneme_string" : "text";
		pending_async_result_metadata_["sentence_silence_seconds"] =
				request.sentence_silence_seconds;
		pending_async_result_metadata_["phoneme_silence_seconds"] =
				request.phoneme_silence_seconds;
		pending_async_result_metadata_["requested_language_id"] = request.language_id;
		pending_async_result_metadata_["requested_language_code"] = request.language_code;
		pending_async_result_metadata_["resolved_language_id"] = request.resolved_language_id;
		pending_async_result_metadata_["resolved_language_code"] = request.resolved_language_code;
		pending_async_result_metadata_["selection_mode"] = request.selection_mode;
	}

	processing.store(true);
	stop_requested.store(false);
	set_runtime_state(piper_runtime::RuntimeState::Busy);
	uint32_t gen = ++synthesis_generation_;

	std::string text_str = request.has_text ? request.text.utf8().get_data() : std::string();
	std::string phoneme_str =
			request.has_phoneme_string ? request.phoneme_string.utf8().get_data() : std::string();
	worker_thread = std::make_unique<std::thread>(
			&PiperTTS::_synthesis_thread_func, this, std::move(text_str), std::move(phoneme_str),
			request.has_phoneme_string, synthesis_config, gen);

	clear_last_error(last_error_);
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
		set_runtime_state(piper_runtime::RuntimeState::Stopping);
		_join_worker_thread();
		processing.store(false);
		std::lock_guard<std::mutex> lock(pending_async_result_mutex_);
		pending_async_result_.reset();
		pending_async_result_metadata_.clear();
	}

	if (was_streaming) {
		set_runtime_state(piper_runtime::RuntimeState::Stopping);
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

	set_runtime_state(ready ? piper_runtime::RuntimeState::Ready
							 : piper_runtime::RuntimeState::Uninitialized);
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

void PiperTTS::_synthesis_thread_func(std::string text_str, std::string phoneme_string,
		bool has_phoneme_string, piper::SynthesisConfig synthesis_config,
		uint32_t generation) {
	std::vector<int16_t> audio_buffer;
	piper::SynthesisResult result;

	try {
		if (has_phoneme_string) {
			std::vector<piper::Phoneme> phonemes =
					piper::parsePhonemeString(
							phoneme_string,
							static_cast<int>(voice->phonemizeConfig.phonemeType));
			piper::phonemesToAudio(*piper_config, *voice, phonemes, synthesis_config,
					audio_buffer, result, nullptr);
		} else {
			piper::textToAudio(*piper_config, *voice, text_str, synthesis_config,
					audio_buffer, result, nullptr);
		}
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

	int sample_rate = synthesis_config.sampleRate;
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
			last_synthesis_result_ = piper_runtime::synthesis_result_to_dictionary(
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
	set_runtime_state(piper_runtime::RuntimeState::Ready);
	emit_signal("synthesis_completed", stream);
}

void PiperTTS::_on_synthesis_failed(const String &error_msg, uint32_t generation) {
	if (generation != synthesis_generation_.load()) {
		processing.store(false);
		return;
	}
	processing.store(false);
	set_runtime_state(piper_runtime::RuntimeState::Ready);
	UtilityFunctions::push_error(String("PiperTTS: ") + error_msg);
	last_synthesis_result_.clear();
	Dictionary metadata_snapshot;
	{
		std::lock_guard<std::mutex> lock(pending_async_result_mutex_);
		metadata_snapshot = pending_async_result_metadata_;
		pending_async_result_.reset();
		pending_async_result_metadata_.clear();
	}
	String requested_language_code = metadata_snapshot.get("requested_language_code", "");
	String resolved_language_code = metadata_snapshot.get("resolved_language_code", "");
	int64_t requested_language_id = metadata_snapshot.has("requested_language_id")
			? static_cast<int64_t>(metadata_snapshot["requested_language_id"])
			: -1;
	int64_t resolved_language_id = metadata_snapshot.has("resolved_language_id")
			? static_cast<int64_t>(metadata_snapshot["resolved_language_id"])
			: -1;
	String selection_mode = metadata_snapshot.get("selection_mode", "");
	set_last_error(last_error_, "ERR_SYNTHESIS_RUNTIME",
			String("PiperTTS: ") + error_msg, "worker", requested_language_code,
			resolved_language_code, requested_language_id, resolved_language_id, selection_mode);
	emit_signal("synthesis_failed", error_msg);
	emit_signal("synthesis_failed_detailed", get_last_error());
}

// ---------------------------------------------------------------------------
// Public methods (M6: streaming)
// ---------------------------------------------------------------------------

Error PiperTTS::synthesize_streaming(
		const String &text, const Ref<AudioStreamGeneratorPlayback> &playback) {
	Dictionary request;
	request["text"] = text;
	return synthesize_streaming_request(request, playback);
}

Error PiperTTS::synthesize_streaming_request(
		const Dictionary &request_dictionary,
		const Ref<AudioStreamGeneratorPlayback> &playback) {
	if (!ready) {
		UtilityFunctions::push_error("PiperTTS: Not initialized. Call initialize() first.");
		set_last_error(last_error_, "ERR_UNCONFIGURED",
				"PiperTTS: Not initialized. Call initialize() first.", "synthesize_streaming_request");
		return ERR_UNCONFIGURED;
	}

	if (!playback.is_valid()) {
		UtilityFunctions::push_error("PiperTTS: Invalid AudioStreamGeneratorPlayback.");
		set_last_error(last_error_, "ERR_INVALID_PARAMETER",
				"PiperTTS: Invalid AudioStreamGeneratorPlayback.", "synthesize_streaming_request");
		return ERR_INVALID_PARAMETER;
	}

	if (processing.load() || streaming_active_.load()) {
		UtilityFunctions::push_error("PiperTTS: Already processing. Call stop() first.");
		set_last_error(last_error_, "ERR_BUSY", "PiperTTS: Already processing. Call stop() first.",
				"synthesize_streaming_request");
		return ERR_BUSY;
	}
	last_synthesis_result_.clear();

	EffectiveRequest request;
	piper_runtime::RuntimeErrorInfo request_error;
	if (!piper_runtime::build_effective_request(build_runtime_property_snapshot(),
				request_dictionary, request, "synthesize_streaming_request", request_error)) {
		UtilityFunctions::push_error(request_error.message);
		set_last_error(last_error_,
				request_error.code.is_empty() ? "ERR_REQUEST_INVALID" : request_error.code,
				request_error.message, "synthesize_streaming_request");
		return ERR_INVALID_PARAMETER;
	}

	String language_error;
	String language_error_code;
	piper::SynthesisConfig synthesis_config;
	if (piper_runtime::build_request_synthesis_config(
				*voice, request, synthesis_config, "synthesize_streaming_request", last_error_) != OK) {
		UtilityFunctions::push_error(last_error_.message);
		return ERR_INVALID_PARAMETER;
	}
	if (!validate_japanese_text_frontend(
				last_error_, request, resolved_dictionary_path_, "synthesize_streaming_request")) {
		UtilityFunctions::push_error(last_error_.message);
		return ERR_UNCONFIGURED;
	}

	_join_worker_thread();
	{
		std::lock_guard<std::mutex> lock(pending_async_result_mutex_);
		pending_async_result_.reset();
		pending_async_result_metadata_.clear();
		pending_async_result_metadata_["input_mode"] =
				request.has_phoneme_string ? "phoneme_string" : "text";
		pending_async_result_metadata_["sentence_silence_seconds"] =
				request.sentence_silence_seconds;
		pending_async_result_metadata_["phoneme_silence_seconds"] =
				request.phoneme_silence_seconds;
		pending_async_result_metadata_["requested_language_id"] = request.language_id;
		pending_async_result_metadata_["requested_language_code"] = request.language_code;
		pending_async_result_metadata_["resolved_language_id"] = request.resolved_language_id;
		pending_async_result_metadata_["resolved_language_code"] = request.resolved_language_code;
		pending_async_result_metadata_["selection_mode"] = request.selection_mode;
		pending_async_result_metadata_["sample_rate"] = synthesis_config.sampleRate;
	}

	streaming_playback_ = playback;
	audio_chunk_queue_.clear();
	pending_samples_.clear();
	pending_sample_offset_ = 0;

	processing.store(true);
	stop_requested.store(false);
	streaming_active_.store(true);
	set_runtime_state(piper_runtime::RuntimeState::Busy);
	uint32_t gen = ++synthesis_generation_;
	set_process(true);

	std::string text_str = request.has_text ? request.text.utf8().get_data() : std::string();
	std::string phoneme_str =
			request.has_phoneme_string ? request.phoneme_string.utf8().get_data() : std::string();
	worker_thread = std::make_unique<std::thread>(
			&PiperTTS::_streaming_thread_func, this, std::move(text_str), std::move(phoneme_str),
			request.has_phoneme_string, synthesis_config, gen);

	clear_last_error(last_error_);
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
				const int sample_rate = pending_async_result_metadata_.get(
						"sample_rate", voice->synthesisConfig.sampleRate);
				last_synthesis_result_ =
						piper_runtime::synthesis_result_to_dictionary(
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
		pending_samples_.clear();
		pending_sample_offset_ = 0;
		streaming_active_.store(false);
		streaming_playback_ = Ref<AudioStreamGeneratorPlayback>();
		set_process(false);
		set_runtime_state(piper_runtime::RuntimeState::Ready);
		emit_signal("streaming_ended");
	}
}

// ---------------------------------------------------------------------------
// Streaming internal methods (M6)
// ---------------------------------------------------------------------------

void PiperTTS::_streaming_thread_func(std::string text_str, std::string phoneme_string,
		bool has_phoneme_string, piper::SynthesisConfig synthesis_config,
		uint32_t generation) {
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

		if (has_phoneme_string) {
			std::vector<piper::Phoneme> phonemes =
					piper::parsePhonemeString(
							phoneme_string,
							static_cast<int>(voice->phonemizeConfig.phonemeType));
			piper::phonemesToAudio(*piper_config, *voice, phonemes, synthesis_config,
					audioBuffer, result, callback);
		} else {
			piper::textToAudio(*piper_config, *voice, text_str, synthesis_config,
					audioBuffer, result, callback);
		}
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
