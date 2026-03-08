#ifndef PIPER_TTS_H
#define PIPER_TTS_H

#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <godot_cpp/classes/audio_stream_generator_playback.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include "audio_queue.h"

// Forward declarations
namespace piper {
struct PiperConfig;
struct Voice;
} // namespace piper

namespace godot {

class PiperTTS : public Node {
	GDCLASS(PiperTTS, Node)

private:
	// Properties
	String model_path;
	String config_path;
	String dictionary_path; // OpenJTalk dictionary directory
	int speaker_id = 0;
	float speech_rate = 1.0f;   // = lengthScale
	float noise_scale = 0.667f;
	float noise_w = 0.8f;

	// Internal state
	bool ready = false;
	std::unique_ptr<piper::PiperConfig> piper_config;
	std::unique_ptr<piper::Voice> voice;

	// Async synthesis state
	std::atomic<bool> processing{false};
	std::atomic<bool> stop_requested{false};
	std::unique_ptr<std::thread> worker_thread;

	// Streaming state (M6)
	Ref<AudioStreamGeneratorPlayback> streaming_playback_;
	AudioChunkQueue<16> audio_chunk_queue_;
	std::atomic<bool> streaming_active_{false};
	std::vector<int16_t> pending_samples_;
	size_t pending_sample_offset_ = 0;

	// Helpers
	String resolve_path(const String &path) const;
	Ref<AudioStreamWAV> create_audio_stream(const std::vector<int16_t> &audio_buffer, int sample_rate) const;

	// Async internal methods (called via call_deferred from worker thread)
	void _synthesis_thread_func(std::string text_str);
	void _on_synthesis_done(const Ref<AudioStreamWAV> &audio);
	void _on_synthesis_failed(const String &error_msg);
	void _join_worker_thread();

	// Streaming internal methods (M6)
	void _streaming_thread_func(std::string text_str);
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

	void set_speaker_id(int p_id);
	int get_speaker_id() const;

	void set_speech_rate(float p_rate);
	float get_speech_rate() const;

	void set_noise_scale(float p_scale);
	float get_noise_scale() const;

	void set_noise_w(float p_w);
	float get_noise_w() const;

	// Methods (M2: sync)
	Error initialize();
	Ref<AudioStreamWAV> synthesize(const String &text);
	bool is_ready() const;

	// Methods (M3: async)
	Error synthesize_async(const String &text);
	void stop();
	bool is_processing() const;

	// Methods (M6: streaming)
	void _process(double p_delta) override;
	Error synthesize_streaming(const String &text, const Ref<AudioStreamGeneratorPlayback> &playback);

	// Signals:
	//   initialized(success: bool)
	//   synthesis_completed(audio: AudioStreamWAV)
	//   synthesis_failed(error: String)
	//   streaming_ended()
};

} // namespace godot

#endif // PIPER_TTS_H
