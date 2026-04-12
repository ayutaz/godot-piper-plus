#include "piper_runtime_support.hpp"

#include <cmath>
#include <cstring>
#include <map>
#include <optional>

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include "piper_core/openjtalk_dictionary_manager.h"
#include "piper_language_support.hpp"
#include "piper_tts_paths.hpp"
#include "piper_core/phoneme_parser.hpp"

namespace godot {
namespace piper_runtime {

namespace {

String packed_or_array_to_phoneme_string(const Variant &value,
		const String &stage, RuntimeErrorInfo &error) {
	if (value.get_type() == Variant::STRING) {
		return String(value).strip_edges();
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
		set_runtime_error(error, "ERR_REQUEST_INVALID",
				"PiperTTS: request.phonemes must be a String, Array, or PackedStringArray.",
				stage);
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

Error apply_phoneme_silence_dictionary(const Dictionary &silence_map,
		const piper::Voice &voice,
		std::optional<std::map<piper::Phoneme, float>> &parsed_silence_map,
		RuntimeErrorInfo &error, const String &stage) {
	if (silence_map.is_empty()) {
		parsed_silence_map.reset();
		return OK;
	}

	std::map<piper::Phoneme, float> parsed_map;
	Array keys = silence_map.keys();
	for (int i = 0; i < keys.size(); ++i) {
		String key = String(keys[i]).strip_edges();
		if (key.is_empty()) {
			set_runtime_error(error, "ERR_REQUEST_INVALID",
					"PiperTTS: phoneme_silence_seconds keys must not be empty.",
					stage);
			return ERR_INVALID_PARAMETER;
		}

		std::vector<piper::Phoneme> phonemes = piper::parsePhonemeString(
				key.utf8().get_data(),
				static_cast<int>(voice.phonemizeConfig.phonemeType));
		if (phonemes.size() != 1) {
			set_runtime_error(error, "ERR_REQUEST_INVALID",
					String("PiperTTS: phoneme_silence_seconds key '") + key +
							"' must resolve to exactly one phoneme token.",
					stage);
			return ERR_INVALID_PARAMETER;
		}

		double silence_seconds = static_cast<double>(silence_map[key]);
		if (!std::isfinite(silence_seconds) || silence_seconds < 0.0) {
			set_runtime_error(error, "ERR_REQUEST_INVALID",
					String("PiperTTS: phoneme_silence_seconds value for '") + key +
							"' must be a finite value >= 0.0.",
					stage);
			return ERR_INVALID_PARAMETER;
		}

		parsed_map[phonemes.front()] = static_cast<float>(silence_seconds);
	}

	parsed_silence_map = std::move(parsed_map);
	return OK;
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

Array resolved_segments_to_dictionary_array(
		const std::vector<piper::ResolvedSegment> &resolved_segments) {
	Array segments;
	for (const auto &segment : resolved_segments) {
		Dictionary entry;
		entry["text"] = String::utf8(segment.text.c_str());
		entry["language_code"] = String::utf8(segment.languageCode.c_str());
		entry["language_id"] =
				segment.languageId.has_value() ? Variant(*segment.languageId)
											   : Variant(-1);
		entry["is_phoneme_input"] = segment.isPhonemeInput;
		segments.push_back(entry);
	}
	return segments;
}

String resolve_native_dictionary_contract_path(const String &configured_dictionary_path) {
	const String trimmed_dictionary_path = configured_dictionary_path.strip_edges();
	if (!trimmed_dictionary_path.is_empty()) {
		return piper_tts_paths::resolve_global_path(trimmed_dictionary_path);
	}

	const char *default_dictionary_path = get_openjtalk_dictionary_path();
	if (default_dictionary_path == nullptr || default_dictionary_path[0] == '\0') {
		return String();
	}

	return String(default_dictionary_path);
}

} // namespace

String runtime_state_to_string(RuntimeState state) {
	switch (state) {
		case RuntimeState::Uninitialized:
			return "uninitialized";
		case RuntimeState::Initializing:
			return "initializing";
		case RuntimeState::Ready:
			return "ready";
		case RuntimeState::Busy:
			return "busy";
		case RuntimeState::Stopping:
			return "stopping";
	}

	return "unknown";
}

void clear_runtime_error(RuntimeErrorInfo &error) {
	error = RuntimeErrorInfo{};
}

void set_runtime_error(RuntimeErrorInfo &error, const String &code,
		const String &message, const String &stage,
		const String &requested_language_code,
		const String &resolved_language_code, int64_t requested_language_id,
		int64_t resolved_language_id, const String &selection_mode) {
	error = RuntimeErrorInfo{};
	error.has_error = true;
	error.code = code;
	error.message = message;
	error.stage = stage;
	error.requested_language_code = requested_language_code;
	error.resolved_language_code = resolved_language_code;
	error.requested_language_id = requested_language_id;
	error.resolved_language_id = resolved_language_id;
	error.selection_mode = selection_mode;
}

Dictionary runtime_error_to_dictionary(const RuntimeErrorInfo &error) {
	Dictionary result;
	if (!error.has_error) {
		return result;
	}

	result["code"] = error.code;
	result["message"] = error.message;
	result["stage"] = error.stage;
	if (!error.requested_language_code.is_empty()) {
		result["requested_language_code"] = error.requested_language_code;
	}
	if (error.requested_language_id >= 0) {
		result["requested_language_id"] = error.requested_language_id;
	}
	if (!error.resolved_language_code.is_empty()) {
		result["resolved_language_code"] = error.resolved_language_code;
	}
	if (error.resolved_language_id >= 0) {
		result["resolved_language_id"] = error.resolved_language_id;
	}
	if (!error.selection_mode.is_empty()) {
		result["selection_mode"] = error.selection_mode;
	}
	return result;
}

bool build_effective_request(const RuntimePropertySnapshot &snapshot,
		const Dictionary &request, EffectiveRequest &effective_request,
		const String &stage, RuntimeErrorInfo &error) {
	clear_runtime_error(error);
	effective_request = EffectiveRequest{};
	effective_request.speaker_id = snapshot.speaker_id;
	effective_request.language_id = snapshot.language_id;
	effective_request.language_code = snapshot.language_code;
	effective_request.speech_rate = snapshot.speech_rate;
	effective_request.noise_scale = snapshot.noise_scale;
	effective_request.noise_w = snapshot.noise_w;
	effective_request.sentence_silence_seconds = snapshot.sentence_silence_seconds;
	effective_request.phoneme_silence_seconds = snapshot.phoneme_silence_seconds;

	if (request.has("text")) {
		effective_request.has_text = true;
		effective_request.text = String(request["text"]);
	}

	if (request.has("phoneme_string")) {
		effective_request.has_phoneme_string = true;
		effective_request.phoneme_string =
				String(request["phoneme_string"]).strip_edges();
	}

	if (request.has("phonemes")) {
		effective_request.has_phoneme_string = true;
		effective_request.phoneme_string =
				packed_or_array_to_phoneme_string(request["phonemes"], stage, error);
		if (error.has_error) {
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
		effective_request.language_code =
				String(request["language_code"]).strip_edges();
	}
	if (request.has("speech_rate")) {
		effective_request.speech_rate =
				static_cast<double>(request["speech_rate"]);
	}
	if (request.has("noise_scale")) {
		effective_request.noise_scale =
				static_cast<double>(request["noise_scale"]);
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
		effective_request.phoneme_silence_seconds =
				request["phoneme_silence_seconds"];
	} else if (request.has("phoneme_silence")) {
		effective_request.phoneme_silence_seconds =
				request["phoneme_silence"];
	}

	if (effective_request.has_text && effective_request.has_phoneme_string) {
		set_runtime_error(error, "ERR_REQUEST_INVALID",
				"PiperTTS: request cannot contain both text and phoneme input.",
				stage);
		return false;
	}

	if (!effective_request.has_text &&
			!effective_request.has_phoneme_string) {
		set_runtime_error(error, "ERR_REQUEST_INVALID",
				"PiperTTS: request must contain text, phoneme_string, or phonemes.",
				stage);
		return false;
	}

	if ((effective_request.has_text && effective_request.text.is_empty()) ||
			(effective_request.has_phoneme_string &&
					effective_request.phoneme_string.is_empty())) {
		set_runtime_error(error, "ERR_INVALID_PARAMETER",
				"PiperTTS: Request input must not be empty.", stage);
		return false;
	}

	return true;
}

Dictionary build_runtime_contract(bool web_export, const String &model_path,
		const String &config_path, const String &dictionary_path,
		const String &openjtalk_library_path,
		const String &custom_dictionary_path, int execution_provider,
		RuntimeState runtime_state) {
	Dictionary contract;
	PackedStringArray phase1_supported_text_frontends;
	PackedStringArray phase1_excluded_features;
	PackedStringArray required_japanese_text_assets;
	const String resolved_web_dictionary_path = web_export
			? piper_tts_paths::resolve_web_dictionary_source(
					dictionary_path, model_path, config_path)
			: String();
	const bool supports_japanese_text_input =
			!web_export || !resolved_web_dictionary_path.is_empty();
	if (web_export) {
		phase1_supported_text_frontends.push_back("en_text_cmu_dict");
		if (supports_japanese_text_input) {
			phase1_supported_text_frontends.push_back("ja_text_openjtalk_dict");
		}
		phase1_excluded_features.push_back("non_cpu_execution_provider");
		phase1_excluded_features.push_back("openjtalk_native");
		if (!supports_japanese_text_input) {
			phase1_excluded_features.push_back("japanese_text_input");
			phase1_excluded_features.push_back("openjtalk_dictionary_bootstrap");
		}
		required_japanese_text_assets.push_back("open_jtalk_dic_utf_8-1.11");
	}

	contract["is_web_export"] = web_export;
	contract["execution_provider_policy"] =
			web_export ? "cpu_only" : "multi_provider";
	contract["supports_non_cpu_execution_provider"] = !web_export;
	contract["supports_openjtalk_native"] = !web_export;
	contract["supports_openjtalk_library_path"] = !web_export;
	contract["resource_source_mode"] =
			web_export ? "godot_file_access" : "filesystem";
	contract["resource_path_mode"] =
			web_export ? "memory_fileaccess" : "globalize_path";
	contract["preview_support_tier"] = web_export ? "preview" : "native";
	contract["phase1_minimal_synthesize_mode"] =
			web_export
					? (supports_japanese_text_input
									? "en_text_cmu_dict_or_ja_text_openjtalk_dict_or_phoneme_string"
									: "en_text_cmu_dict_or_phoneme_string")
					: "platform_default";
	contract["phase1_supported_text_frontends"] = phase1_supported_text_frontends;
	contract["phase1_excluded_features"] = phase1_excluded_features;
	contract["supports_japanese_text_input"] = supports_japanese_text_input;
	contract["required_japanese_text_assets"] = required_japanese_text_assets;
	contract["openjtalk_dictionary_bootstrap_mode"] = web_export
			? (supports_japanese_text_input ? "staged_asset" : "missing_required_asset")
			: "filesystem";
	contract["resolved_dictionary_path"] =
			web_export ? resolved_web_dictionary_path
					   : resolve_native_dictionary_contract_path(dictionary_path);
	contract["runtime_state"] = runtime_state_to_string(runtime_state);
	contract["model_path"] = model_path;
	contract["config_path"] = config_path;
	contract["dictionary_path"] = dictionary_path;
	contract["openjtalk_library_path"] = openjtalk_library_path;
	contract["custom_dictionary_path"] = custom_dictionary_path;
	contract["execution_provider"] = execution_provider;
	return contract;
}

Error build_request_synthesis_config(const piper::Voice &voice,
		EffectiveRequest &effective_request,
		piper::SynthesisConfig &synthesis_config, const String &stage,
		RuntimeErrorInfo &error) {
	clear_runtime_error(error);
	synthesis_config = voice.synthesisConfig;
	synthesis_config.lengthScale =
			CLAMP(effective_request.speech_rate, 0.1f, 5.0f);
	synthesis_config.noiseScale =
			CLAMP(effective_request.noise_scale, 0.0f, 2.0f);
	synthesis_config.noiseW =
			CLAMP(effective_request.noise_w, 0.0f, 2.0f);
	synthesis_config.sentenceSilenceSeconds =
			MAX(effective_request.sentence_silence_seconds, 0.0f);
	synthesis_config.speakerId =
			static_cast<piper::SpeakerId>(MAX(effective_request.speaker_id, 0));

	Error silence_error = apply_phoneme_silence_dictionary(
			effective_request.phoneme_silence_seconds, voice,
			synthesis_config.phonemeSilenceSeconds, error, stage);
	if (silence_error != OK) {
		return silence_error;
	}

	Error language_error = piper_language::resolve_requested_language(voice,
			effective_request.language_id, effective_request.language_code,
			effective_request, stage, error);
	if (language_error != OK) {
		effective_request.selection_mode = "";
		effective_request.resolved_language_code = String();
		effective_request.resolved_language_id = -1;
		return language_error;
	}
	synthesis_config.languageId = effective_request.resolved_language_id >= 0
			? std::make_optional(static_cast<piper::LanguageId>(
					effective_request.resolved_language_id))
			: std::nullopt;

	Error text_language_error = piper_language::validate_text_language_support(
			voice, synthesis_config, effective_request, stage, error);
	if (text_language_error != OK) {
		return text_language_error;
	}

	if (!effective_request.has_text &&
			effective_request.selection_mode.is_empty()) {
		effective_request.selection_mode = "auto";
	}

	return OK;
}

std::vector<piper::Phoneme> parse_effective_phoneme_string(
		const piper::Voice &voice, const String &phoneme_string) {
	return piper::parsePhonemeString(phoneme_string.utf8().get_data(),
			static_cast<int>(voice.phonemizeConfig.phonemeType));
}

Dictionary synthesis_result_to_dictionary(const piper::SynthesisResult &result,
		int sample_rate) {
	Dictionary data;
	data["sample_rate"] = sample_rate;
	data["infer_seconds"] = result.inferSeconds;
	data["audio_seconds"] = result.audioSeconds;
	data["real_time_factor"] = result.realTimeFactor;
	data["has_timing_info"] = result.hasTimingInfo;
	data["phoneme_timings"] =
			phoneme_timings_to_dictionary_array(result.phonemeTimings);
	data["resolved_segments"] =
			resolved_segments_to_dictionary_array(result.resolvedSegments);
	return data;
}

Dictionary inspection_result_to_dictionary(
		const piper::InspectionResult &result, const piper::Voice &voice) {
	Dictionary data;
	Array phoneme_sentences;
	Array phoneme_id_sentences;

	for (std::size_t sentence_index = 0;
			sentence_index < result.phonemeSentences.size(); ++sentence_index) {
		const auto &sentence = result.phonemeSentences[sentence_index];
		PackedStringArray phoneme_tokens;
		phoneme_tokens.resize(static_cast<int>(sentence.size()));
		for (std::size_t phoneme_index = 0; phoneme_index < sentence.size();
				++phoneme_index) {
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
			result.resolvedLanguageId.has_value()
					? Variant(*result.resolvedLanguageId)
					: Variant(-1);
	data["resolved_language_code"] =
			piper_language::language_code_from_id(voice, result.resolvedLanguageId);
	data["resolved_segments"] =
			resolved_segments_to_dictionary_array(result.resolvedSegments);
	return data;
}

} // namespace piper_runtime
} // namespace godot
