#ifndef PIPER_TTS_H
#define PIPER_TTS_H

#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>

#include <memory>
#include <string>
#include <vector>

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

	// Helper
	String resolve_path(const String &path) const;
	Ref<AudioStreamWAV> create_audio_stream(const std::vector<int16_t> &audio_buffer, int sample_rate) const;

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

	// Methods
	Error initialize();
	Ref<AudioStreamWAV> synthesize(const String &text);
	bool is_ready() const;

	// Signals: initialized(success: bool)
};

} // namespace godot

#endif // PIPER_TTS_H
