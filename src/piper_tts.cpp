#include "piper_tts.h"

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "piper_core/piper.hpp"

extern "C" {
	void openjtalk_set_dictionary_path(const char* path);
}

#include <cstring>
#include <functional>
#include <optional>

namespace godot {

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

PiperTTS::PiperTTS() {
	piper_config = std::make_unique<piper::PiperConfig>();
	voice = std::make_unique<piper::Voice>();
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

	if (ready) {
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

	// --- Property: speaker_id ---
	ClassDB::bind_method(D_METHOD("set_speaker_id", "id"), &PiperTTS::set_speaker_id);
	ClassDB::bind_method(D_METHOD("get_speaker_id"), &PiperTTS::get_speaker_id);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "speaker_id", PROPERTY_HINT_RANGE, "0,999,1"),
			"set_speaker_id", "get_speaker_id");

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
	ClassDB::bind_method(D_METHOD("is_ready"), &PiperTTS::is_ready);

	// --- Methods (M3: async) ---
	ClassDB::bind_method(D_METHOD("synthesize_async", "text"), &PiperTTS::synthesize_async);
	ClassDB::bind_method(D_METHOD("stop"), &PiperTTS::stop);
	ClassDB::bind_method(D_METHOD("is_processing"), &PiperTTS::is_processing);

	// Internal methods for call_deferred (worker thread → main thread)
	ClassDB::bind_method(D_METHOD("_on_synthesis_done", "audio"), &PiperTTS::_on_synthesis_done);
	ClassDB::bind_method(D_METHOD("_on_synthesis_failed", "error"), &PiperTTS::_on_synthesis_failed);

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

void PiperTTS::set_speaker_id(int p_id) {
	speaker_id = p_id < 0 ? 0 : p_id;
}

int PiperTTS::get_speaker_id() const {
	return speaker_id;
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
	if (ready) {
		piper::terminate(*piper_config);
		ready = false;
	}

	if (model_path.is_empty()) {
		UtilityFunctions::push_error("PiperTTS: model_path is not set.");
		emit_signal("initialized", false);
		return ERR_UNCONFIGURED;
	}

	if (config_path.is_empty()) {
		UtilityFunctions::push_error("PiperTTS: config_path is not set.");
		emit_signal("initialized", false);
		return ERR_UNCONFIGURED;
	}

	// Resolve Godot resource paths to absolute OS paths
	String abs_model = resolve_path(model_path);
	String abs_config = resolve_path(config_path);

	// Set OpenJTalk dictionary path if configured
	if (!dictionary_path.is_empty()) {
		String abs_dict = resolve_path(dictionary_path);
		std::string dict_str = abs_dict.utf8().get_data();
		openjtalk_set_dictionary_path(dict_str.c_str());
		UtilityFunctions::print(String("PiperTTS: OpenJTalk dictionary path set to: ") + abs_dict);
	}

	std::string model_str = abs_model.utf8().get_data();
	std::string config_str = abs_config.utf8().get_data();

	try {
		piper::initialize(*piper_config);

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

		ready = true;
		UtilityFunctions::print("PiperTTS: Voice loaded successfully.");
		emit_signal("initialized", true);
		return OK;

	} catch (const std::exception &e) {
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

	// Apply current synthesis parameters to the voice config
	voice->synthesisConfig.lengthScale = speech_rate;
	voice->synthesisConfig.noiseScale = noise_scale;
	voice->synthesisConfig.noiseW = noise_w;

	// Update speaker ID if multi-speaker model
	if (speaker_id >= 0) {
		voice->synthesisConfig.speakerId = static_cast<piper::SpeakerId>(speaker_id);
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
	return create_audio_stream(audio_buffer, sample_rate);
}

bool PiperTTS::is_ready() const {
	return ready;
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

	if (processing.load()) {
		UtilityFunctions::push_error("PiperTTS: Already processing. Call stop() first or wait for completion.");
		return ERR_BUSY;
	}

	// Apply synthesis parameters on the main thread (safe - no worker is running)
	voice->synthesisConfig.lengthScale = speech_rate;
	voice->synthesisConfig.noiseScale = noise_scale;
	voice->synthesisConfig.noiseW = noise_w;

	if (speaker_id >= 0) {
		voice->synthesisConfig.speakerId = static_cast<piper::SpeakerId>(speaker_id);
	}

	// Join any previous (completed) worker thread
	_join_worker_thread();

	processing.store(true);
	stop_requested.store(false);

	std::string text_str = text.utf8().get_data();
	worker_thread = std::make_unique<std::thread>(
			&PiperTTS::_synthesis_thread_func, this, std::move(text_str));

	return OK;
}

void PiperTTS::stop() {
	bool was_processing = processing.load();
	bool was_streaming = streaming_active_.load();

	if (!was_processing && !was_streaming) {
		return;
	}

	stop_requested.store(true);

	if (was_processing) {
		_join_worker_thread();
	}

	if (was_streaming) {
		streaming_active_.store(false);
		audio_chunk_queue_.clear();
		pending_samples_.clear();
		pending_sample_offset_ = 0;
		streaming_playback_ = Ref<AudioStreamGeneratorPlayback>();
		set_process(false);
	}
}

bool PiperTTS::is_processing() const {
	return processing.load();
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

void PiperTTS::_synthesis_thread_func(std::string text_str) {
	std::vector<int16_t> audio_buffer;
	piper::SynthesisResult result;

	try {
		piper::textToAudio(*piper_config, *voice, text_str,
				audio_buffer, result, nullptr);
	} catch (const std::exception &e) {
		if (!stop_requested.load()) {
			call_deferred("_on_synthesis_failed", String(e.what()));
		}
		processing.store(false);
		return;
	}

	// Check if stop was requested during synthesis
	if (stop_requested.load()) {
		processing.store(false);
		return;
	}

	if (audio_buffer.empty()) {
		call_deferred("_on_synthesis_failed", String("Synthesis produced no audio."));
		processing.store(false);
		return;
	}

	int sample_rate = voice->synthesisConfig.sampleRate;
	Ref<AudioStreamWAV> audio = create_audio_stream(audio_buffer, sample_rate);

	call_deferred("_on_synthesis_done", audio);
	processing.store(false);
}

void PiperTTS::_on_synthesis_done(const Ref<AudioStreamWAV> &audio) {
	emit_signal("synthesis_completed", audio);
}

void PiperTTS::_on_synthesis_failed(const String &error_msg) {
	UtilityFunctions::push_error(String("PiperTTS: ") + error_msg);
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

	// Apply synthesis parameters
	voice->synthesisConfig.lengthScale = speech_rate;
	voice->synthesisConfig.noiseScale = noise_scale;
	voice->synthesisConfig.noiseW = noise_w;

	if (speaker_id >= 0) {
		voice->synthesisConfig.speakerId = static_cast<piper::SpeakerId>(speaker_id);
	}

	_join_worker_thread();

	streaming_playback_ = playback;
	audio_chunk_queue_.clear();
	pending_samples_.clear();
	pending_sample_offset_ = 0;

	processing.store(true);
	stop_requested.store(false);
	streaming_active_.store(true);
	set_process(true);

	std::string text_str = text.utf8().get_data();
	worker_thread = std::make_unique<std::thread>(
			&PiperTTS::_streaming_thread_func, this, std::move(text_str));

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

void PiperTTS::_streaming_thread_func(std::string text_str) {
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
			call_deferred("_on_synthesis_failed", String(e.what()));
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
