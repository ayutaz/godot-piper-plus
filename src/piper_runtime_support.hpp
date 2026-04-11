#ifndef PIPER_RUNTIME_SUPPORT_H
#define PIPER_RUNTIME_SUPPORT_H

#include <vector>

#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "piper_core/piper.hpp"

namespace godot {
namespace piper_runtime {

enum class RuntimeState {
	Uninitialized = 0,
	Initializing = 1,
	Ready = 2,
	Busy = 3,
	Stopping = 4,
};

struct RuntimePropertySnapshot {
	int speaker_id = 0;
	int language_id = -1;
	String language_code;
	float speech_rate = 1.0f;
	float noise_scale = 0.667f;
	float noise_w = 0.8f;
	float sentence_silence_seconds = 0.2f;
	Dictionary phoneme_silence_seconds;
};

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
	String resolved_language_code;
	int resolved_language_id = -1;
	String selection_mode;
};

struct RuntimeErrorInfo {
	bool has_error = false;
	String code;
	String message;
	String stage;
	String requested_language_code;
	String resolved_language_code;
	int64_t requested_language_id = -1;
	int64_t resolved_language_id = -1;
	String selection_mode;
};

String runtime_state_to_string(RuntimeState state);

void clear_runtime_error(RuntimeErrorInfo &error);
void set_runtime_error(RuntimeErrorInfo &error, const String &code, const String &message,
		const String &stage, const String &requested_language_code = String(),
		const String &resolved_language_code = String(), int64_t requested_language_id = -1,
		int64_t resolved_language_id = -1, const String &selection_mode = String());
Dictionary runtime_error_to_dictionary(const RuntimeErrorInfo &error);

bool build_effective_request(const RuntimePropertySnapshot &snapshot, const Dictionary &request,
		EffectiveRequest &effective_request, const String &stage,
		RuntimeErrorInfo &error);
Dictionary build_runtime_contract(bool web_export, const String &model_path,
		const String &config_path, const String &dictionary_path,
		const String &openjtalk_library_path, const String &custom_dictionary_path,
		int execution_provider, RuntimeState runtime_state);
Error build_request_synthesis_config(const piper::Voice &voice,
		EffectiveRequest &effective_request, piper::SynthesisConfig &synthesis_config,
		const String &stage, RuntimeErrorInfo &error);

std::vector<piper::Phoneme> parse_effective_phoneme_string(
		const piper::Voice &voice, const String &phoneme_string);
Dictionary synthesis_result_to_dictionary(const piper::SynthesisResult &result,
		int sample_rate);
Dictionary inspection_result_to_dictionary(
		const piper::InspectionResult &result, const piper::Voice &voice);

} // namespace piper_runtime
} // namespace godot

#endif // PIPER_RUNTIME_SUPPORT_H
