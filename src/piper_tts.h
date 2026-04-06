#ifndef PIPER_TTS_H
#define PIPER_TTS_H

#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <godot_cpp/classes/audio_stream_generator_playback.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include "audio_queue.h"

// Forward declarations
namespace piper {
struct PiperConfig;
struct SynthesisResult;
struct Voice;
} // namespace piper

namespace godot {

class PiperTTS : public Node {
	GDCLASS(PiperTTS, Node)

public:
	enum ExecutionProviderGD {
		EP_CPU = 0,
		EP_COREML = 1,
		EP_DIRECTML = 2,
		EP_NNAPI = 3,
		EP_AUTO = 4,
		EP_CUDA = 5,
	};

private:
	// Properties
	String model_path;
	String config_path;
	String dictionary_path; // OpenJTalk dictionary directory
	String openjtalk_library_path; // Optional openjtalk-native shared library
	String custom_dictionary_path; // Runtime custom dictionary JSON
	int speaker_id = 0;
	int language_id = -1; // Auto when < 0
	String language_code;
	float speech_rate = 1.0f;   // = lengthScale
	float noise_scale = 0.667f;
	float noise_w = 0.8f;
	float sentence_silence_seconds = 0.2f;
	Dictionary phoneme_silence_seconds;
	int execution_provider = 0; // EP_CPU
	int gpu_device_id = 0;

	// Internal state
	bool ready = false;
	std::unique_ptr<piper::PiperConfig> piper_config;
	std::unique_ptr<piper::Voice> voice;
	Dictionary last_synthesis_result_;
	Dictionary last_inspection_result_;
	Dictionary last_error_;

	// Async synthesis state
	std::atomic<bool> processing{false};
	std::atomic<bool> stop_requested{false};
	std::atomic<uint32_t> synthesis_generation_{0};
	std::unique_ptr<std::thread> worker_thread;
	std::unique_ptr<piper::SynthesisResult> pending_async_result_;
	std::mutex pending_async_result_mutex_;
	Dictionary pending_async_result_metadata_;

	// Streaming state (M6)
	Ref<AudioStreamGeneratorPlayback> streaming_playback_;
	AudioChunkQueue<16> audio_chunk_queue_;
	std::atomic<bool> streaming_active_{false};
	std::vector<int16_t> pending_samples_;
	size_t pending_sample_offset_ = 0;

	// Helpers
	String resolve_path(const String &path) const;
	String resolve_model_path(const String &path) const;
	String resolve_config_path(const String &resolved_model_path) const;
	Ref<AudioStreamWAV> create_audio_stream(const std::vector<int16_t> &audio_buffer, int sample_rate) const;

	// Async internal methods (called via call_deferred from worker thread)
	void _synthesis_thread_func(std::string text_str, std::string phoneme_string,
			bool has_phoneme_string, uint32_t generation);
	void _on_synthesis_raw_done(const PackedByteArray &pcm_data, int sample_rate, uint32_t generation);
	void _on_synthesis_failed(const String &error_msg, uint32_t generation);
	void _join_worker_thread();

	// Streaming internal methods (M6)
	void _streaming_thread_func(std::string text_str, std::string phoneme_string,
			bool has_phoneme_string, uint32_t generation);
	void _push_pending_samples();

protected:
	static void _bind_methods();

public:
	PiperTTS();
	~PiperTTS();

	// Properties
	void set_model_path(const String &p_path);
	String get_model_path() const;

	void set_config_path(const String &p_path);
	String get_config_path() const;

	void set_dictionary_path(const String &p_path);
	String get_dictionary_path() const;

	void set_openjtalk_library_path(const String &p_path);
	String get_openjtalk_library_path() const;

	void set_custom_dictionary_path(const String &p_path);
	String get_custom_dictionary_path() const;

	void set_speaker_id(int p_id);
	int get_speaker_id() const;

	void set_language_id(int p_id);
	int get_language_id() const;

	void set_language_code(const String &p_code);
	String get_language_code() const;

	void set_speech_rate(float p_rate);
	float get_speech_rate() const;

	void set_noise_scale(float p_scale);
	float get_noise_scale() const;

	void set_noise_w(float p_w);
	float get_noise_w() const;

	void set_sentence_silence_seconds(float p_seconds);
	float get_sentence_silence_seconds() const;

	void set_phoneme_silence_seconds(const Dictionary &p_map);
	Dictionary get_phoneme_silence_seconds() const;

	void set_execution_provider(int p_ep);
	int get_execution_provider() const;

	void set_gpu_device_id(int p_id);
	int get_gpu_device_id() const;

	// Methods (M2: sync)
	Error initialize();
	Ref<AudioStreamWAV> synthesize(const String &text);
	Ref<AudioStreamWAV> synthesize_request(const Dictionary &request);
	Ref<AudioStreamWAV> synthesize_phoneme_string(const String &phoneme_string);
	bool is_ready() const;
	Dictionary inspect_text(const String &text);
	Dictionary inspect_request(const Dictionary &request);
	Dictionary inspect_phoneme_string(const String &phoneme_string);
	Dictionary get_last_synthesis_result() const;
	Dictionary get_last_inspection_result() const;
	Dictionary get_language_capabilities() const;
	Dictionary get_last_error() const;

	// Methods (M3: async)
	Error synthesize_async(const String &text);
	Error synthesize_async_request(const Dictionary &request);
	void stop();
	bool is_processing() const;

	// Methods (M6: streaming)
	void _process(double p_delta) override;
	Error synthesize_streaming(const String &text, const Ref<AudioStreamGeneratorPlayback> &playback);
	Error synthesize_streaming_request(
			const Dictionary &request, const Ref<AudioStreamGeneratorPlayback> &playback);

	// Signals:
	//   initialized(success: bool)
	//   synthesis_completed(audio: AudioStreamWAV)
	//   synthesis_failed(error: String)
	//   streaming_ended()
};

} // namespace godot

VARIANT_ENUM_CAST(godot::PiperTTS::ExecutionProviderGD);

#endif // PIPER_TTS_H
