#include "piper_tts.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "piper_core/custom_dictionary.hpp"
#include "piper_core/multilingual_phonemize.hpp"
#include "piper_core/phoneme_parser.hpp"
#include "piper_core/piper.hpp"

extern "C" {
	void openjtalk_set_dictionary_path(const char* path);
	void openjtalk_set_library_path(const char* path);
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

String language_code_from_id(const piper::Voice &voice,
		const std::optional<piper::LanguageId> &language_id);

std::string base_language_code(const std::string &language_code) {
	const std::size_t separator = language_code.find('-');
	if (separator == std::string::npos || separator == 0) {
		return language_code;
	}
	return language_code.substr(0, separator);
}

bool is_multilingual_auto_route_language(const std::string &language_code) {
	return language_code == "ja" || language_code == "en";
}

bool is_multilingual_text_language(const std::string &language_code) {
	return is_multilingual_auto_route_language(language_code) ||
			language_code == "es" || language_code == "fr" || language_code == "pt";
}

struct LanguageCodeMatch {
	piper::LanguageId language_id = -1;
	String canonical_code;
	bool exact_match = false;
};

std::optional<LanguageCodeMatch> resolve_language_code_match(
		const piper::Voice &voice, const std::string &normalized_code) {
	if (!voice.modelConfig.languageIdMap || voice.modelConfig.languageIdMap->empty()) {
		return std::nullopt;
	}

	for (const auto &[language_code, language_value] : *voice.modelConfig.languageIdMap) {
		const String map_code = String(language_code.c_str());
		if (normalize_language_code(map_code) == normalized_code) {
			return LanguageCodeMatch{language_value, map_code, true};
		}
	}

	const std::string requested_base = base_language_code(normalized_code);
	if (requested_base.empty() || requested_base == normalized_code) {
		return std::nullopt;
	}

	for (const auto &[language_code, language_value] : *voice.modelConfig.languageIdMap) {
		const String map_code = String(language_code.c_str());
		if (normalize_language_code(map_code) == requested_base) {
			return LanguageCodeMatch{language_value, map_code, false};
		}
	}

	return std::nullopt;
}

void set_last_error(Dictionary &target, const String &code, const String &message,
		const String &stage, const String &requested_language_code = String(),
		const String &resolved_language_code = String(), int64_t requested_language_id = -1,
		int64_t resolved_language_id = -1, const String &selection_mode = String()) {
	target.clear();
	target["code"] = code;
	target["message"] = message;
	target["stage"] = stage;
	if (!requested_language_code.is_empty()) {
		target["requested_language_code"] = requested_language_code;
	}
	if (requested_language_id >= 0) {
		target["requested_language_id"] = requested_language_id;
	}
	if (!resolved_language_code.is_empty()) {
		target["resolved_language_code"] = resolved_language_code;
	}
	if (resolved_language_id >= 0) {
		target["resolved_language_id"] = resolved_language_id;
	}
	if (!selection_mode.is_empty()) {
		target["selection_mode"] = selection_mode;
	}
}

void clear_last_error(Dictionary &target) {
	target.clear();
}

Dictionary build_language_capabilities(const piper::Voice &voice) {
	Dictionary capabilities;
	Array languages;
	PackedStringArray available_language_codes;
	PackedInt64Array available_language_ids;
	PackedStringArray auto_route_language_codes;
	PackedStringArray explicit_only_language_codes;
	PackedStringArray text_supported_language_codes;
	PackedStringArray phoneme_only_language_codes;
	PackedStringArray preview_language_codes;
	PackedStringArray experimental_language_codes;

	auto push_unique_string = [](PackedStringArray &array, const String &value) {
		if (!array.has(value)) {
			array.push_back(value);
		}
	};

	auto push_unique_id = [](PackedInt64Array &array, int64_t value) {
		for (int i = 0; i < array.size(); ++i) {
			if (array[i] == value) {
				return;
			}
		}
		array.push_back(value);
	};

	auto classify_language = [](const std::string &normalized_code) {
		Dictionary entry;
		entry["routing_mode"] = "phoneme_only";
		entry["support_tier"] = "phoneme_only";
		entry["frontend_backend"] = "raw_phoneme_only";
		entry["text_supported"] = false;
		entry["auto_supported"] = false;
		entry["phoneme_only"] = true;

		if (normalized_code == "ja") {
			entry["routing_mode"] = "auto";
			entry["support_tier"] = "preview";
			entry["frontend_backend"] = "openjtalk";
			entry["text_supported"] = true;
			entry["auto_supported"] = true;
			entry["phoneme_only"] = false;
		} else if (normalized_code == "en") {
			entry["routing_mode"] = "auto";
			entry["support_tier"] = "preview";
			entry["frontend_backend"] = "cmu_dict";
			entry["text_supported"] = true;
			entry["auto_supported"] = true;
			entry["phoneme_only"] = false;
		} else if (normalized_code == "es" || normalized_code == "fr" ||
				normalized_code == "pt") {
			entry["routing_mode"] = "explicit_only";
			entry["support_tier"] = "experimental";
			entry["frontend_backend"] = "rule_based";
			entry["text_supported"] = true;
			entry["auto_supported"] = false;
			entry["phoneme_only"] = false;
		}

		return entry;
	};

	auto append_language_entry = [&](const String &language_code, int64_t language_id) {
		const std::string normalized_code = normalize_language_code(language_code);
		Dictionary entry = classify_language(normalized_code);
		entry["language_code"] = language_code;
		entry["language_id"] = language_id;
		languages.push_back(entry);
		if (String(entry["support_tier"]) == "preview") {
			push_unique_string(preview_language_codes, language_code);
		} else if (String(entry["support_tier"]) == "experimental") {
			push_unique_string(experimental_language_codes, language_code);
		}
	};

	if (voice.modelConfig.languageIdMap && !voice.modelConfig.languageIdMap->empty()) {
		std::vector<std::pair<piper::LanguageId, String>> sorted_languages;
		sorted_languages.reserve(voice.modelConfig.languageIdMap->size());
		for (const auto &[language_code, language_id] : *voice.modelConfig.languageIdMap) {
			sorted_languages.emplace_back(language_id, String(language_code.c_str()));
		}
		std::sort(sorted_languages.begin(), sorted_languages.end(),
				[](const auto &lhs, const auto &rhs) {
					if (lhs.first == rhs.first) {
						return lhs.second < rhs.second;
					}
					return lhs.first < rhs.first;
				});

		for (const auto &[language_id, language_code] : sorted_languages) {
			available_language_codes.push_back(language_code);
			push_unique_id(available_language_ids, language_id);
			append_language_entry(language_code, language_id);

			const std::string normalized_code = normalize_language_code(language_code);
			if (is_multilingual_auto_route_language(normalized_code)) {
				push_unique_string(auto_route_language_codes, language_code);
				push_unique_string(text_supported_language_codes, language_code);
			} else if (is_multilingual_text_language(normalized_code)) {
				push_unique_string(explicit_only_language_codes, language_code);
				push_unique_string(text_supported_language_codes, language_code);
			} else {
				push_unique_string(phoneme_only_language_codes, language_code);
			}
		}
	} else {
		const bool has_multilingual = voice.phonemizeConfig.phonemeType == piper::MultilingualPhonemes;
		const bool has_openjtalk = voice.phonemizeConfig.phonemeType == piper::OpenJTalkPhonemes;
		const bool has_english_text = voice.phonemizeConfig.phonemeType == piper::TextPhonemes;

		if (has_multilingual) {
			for (const char *language_code : {"ja", "en"}) {
				const String code(language_code);
				available_language_codes.push_back(code);
				push_unique_id(available_language_ids, code == "ja" ? 0 : 1);
				append_language_entry(code, code == "ja" ? 0 : 1);
				push_unique_string(auto_route_language_codes, code);
				push_unique_string(text_supported_language_codes, code);
			}
		} else if (has_openjtalk) {
			available_language_codes.push_back("ja");
			push_unique_id(available_language_ids, 0);
			append_language_entry("ja", 0);
			push_unique_string(auto_route_language_codes, "ja");
			push_unique_string(text_supported_language_codes, "ja");
		} else if (has_english_text) {
			available_language_codes.push_back("en");
			push_unique_id(available_language_ids, 0);
			append_language_entry("en", 0);
			push_unique_string(auto_route_language_codes, "en");
			push_unique_string(text_supported_language_codes, "en");
		}
	}

	String configured_language_code;
	if (voice.synthesisConfig.languageId.has_value()) {
		configured_language_code = language_code_from_id(voice, voice.synthesisConfig.languageId);
	}

	String default_language_code;
	if (!configured_language_code.is_empty()) {
		default_language_code = configured_language_code;
	} else if (auto_route_language_codes.has("en")) {
		default_language_code = "en";
	} else if (!available_language_codes.is_empty()) {
		default_language_code = available_language_codes[0];
	}

	capabilities["has_language_id_map"] =
			voice.modelConfig.languageIdMap && !voice.modelConfig.languageIdMap->empty();
	capabilities["available_language_codes"] = available_language_codes;
	capabilities["available_language_ids"] = available_language_ids;
	capabilities["default_language_code"] = default_language_code;
	capabilities["configured_language_code"] = configured_language_code;
	capabilities["configured_language_id"] =
			voice.synthesisConfig.languageId.has_value() ? Variant(*voice.synthesisConfig.languageId)
														   : Variant(-1);
	capabilities["auto_route_language_codes"] = auto_route_language_codes;
	capabilities["explicit_only_language_codes"] = explicit_only_language_codes;
	capabilities["text_supported_language_codes"] = text_supported_language_codes;
	capabilities["phoneme_only_language_codes"] = phoneme_only_language_codes;
	capabilities["preview_language_codes"] = preview_language_codes;
	capabilities["experimental_language_codes"] = experimental_language_codes;
	capabilities["languages"] = languages;
	capabilities["supports_text_input"] = !text_supported_language_codes.is_empty();
	return capabilities;
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
	String resolved_language_code;
	int resolved_language_id = -1;
	String selection_mode;
};

Error resolve_requested_language(
		const piper::Voice &voice, int requested_language_id,
		const String &requested_language_code, EffectiveRequest &effective_request,
		String &error_code, String &error_message) {
	error_code = String();
	const std::string normalized_code = normalize_language_code(requested_language_code);
	if (!normalized_code.empty()) {
		if (!voice.modelConfig.languageIdMap || voice.modelConfig.languageIdMap->empty()) {
			error_code = "ERR_LANGUAGE_CAPABILITY_MISSING";
			error_message = "PiperTTS: language_code was set, but the loaded model does not expose language_id_map.";
			return ERR_INVALID_PARAMETER;
		}

		const std::optional<LanguageCodeMatch> matched_language =
				resolve_language_code_match(voice, normalized_code);
		if (!matched_language.has_value()) {
			error_code = "ERR_LANGUAGE_UNSUPPORTED_FOR_TEXT";
			error_message = String("PiperTTS: language_code '") +
					requested_language_code.strip_edges() +
					"' was not found in language_id_map.";
			return ERR_INVALID_PARAMETER;
		}

		if (requested_language_id >= 0 &&
				requested_language_id != static_cast<int>(matched_language->language_id)) {
			error_code = "ERR_LANGUAGE_SELECTOR_CONFLICT";
			error_message = String("PiperTTS: language_code '") +
					requested_language_code.strip_edges() +
					"' conflicts with language_id=" + String::num_int64(requested_language_id) +
					" for the loaded model.";
			return ERR_INVALID_PARAMETER;
		}

		effective_request.resolved_language_id = matched_language->language_id;
		effective_request.resolved_language_code = matched_language->canonical_code;
		effective_request.selection_mode =
				matched_language->exact_match ? "language_code_exact" : "language_code_base";
		return OK;
	}

	if (requested_language_id >= 0) {
		if (voice.modelConfig.numLanguages > 0 &&
				requested_language_id >= voice.modelConfig.numLanguages) {
			error_code = "ERR_LANGUAGE_ID_OUT_OF_RANGE";
			error_message = String("PiperTTS: language_id is out of range for this model: ") +
					String::num_int64(requested_language_id);
			return ERR_INVALID_PARAMETER;
		}

		effective_request.resolved_language_id = requested_language_id;
		effective_request.resolved_language_code = language_code_from_id(
				voice, static_cast<piper::LanguageId>(requested_language_id));
		effective_request.selection_mode = "language_id";
		return OK;
	}

	effective_request.resolved_language_id = -1;
	effective_request.resolved_language_code = String();
	effective_request.selection_mode = "auto";
	return OK;
}

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

Error validate_text_language_support(
		const piper::Voice &voice, const piper::SynthesisConfig &synthesis_config,
		const EffectiveRequest &effective_request, String &error_code, String &error_message) {
	error_code = String();
	if (!effective_request.has_text ||
			voice.phonemizeConfig.phonemeType != piper::MultilingualPhonemes) {
		return OK;
	}

	const std::string resolved_language_code =
			normalize_language_code(language_code_from_id(voice, synthesis_config.languageId));
	if (resolved_language_code.empty()) {
		return OK;
	}

	if (!piper::supportsMultilingualTextPhonemization(resolved_language_code)) {
		error_code = "ERR_LANGUAGE_UNSUPPORTED_FOR_TEXT";
		error_message = String("PiperTTS: ") +
				String::utf8(
						piper::getMultilingualTextSupportError(resolved_language_code)
								.c_str());
		return ERR_INVALID_PARAMETER;
	}

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
				segment.languageId.has_value() ? Variant(*segment.languageId) : Variant(-1);
		entry["is_phoneme_input"] = segment.isPhonemeInput;
		segments.push_back(entry);
	}
	return segments;
}

Dictionary synthesis_result_to_dictionary(const piper::SynthesisResult &result, int sample_rate) {
	Dictionary data;
	data["sample_rate"] = sample_rate;
	data["infer_seconds"] = result.inferSeconds;
	data["audio_seconds"] = result.audioSeconds;
	data["real_time_factor"] = result.realTimeFactor;
	data["has_timing_info"] = result.hasTimingInfo;
	data["phoneme_timings"] = phoneme_timings_to_dictionary_array(result.phonemeTimings);
	data["resolved_segments"] = resolved_segments_to_dictionary_array(result.resolvedSegments);
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
	data["resolved_segments"] = resolved_segments_to_dictionary_array(result.resolvedSegments);
	return data;
}

void annotate_language_metadata(
		Dictionary &data, const EffectiveRequest &request,
		const piper::SynthesisConfig &synthesis_config, const piper::Voice &voice) {
	std::optional<piper::LanguageId> resolved_language_id = synthesis_config.languageId;
	if (!resolved_language_id.has_value() && request.resolved_language_id >= 0) {
		resolved_language_id = static_cast<piper::LanguageId>(request.resolved_language_id);
	}

	String resolved_language_code = language_code_from_id(voice, resolved_language_id);
	if (resolved_language_code.is_empty() && !request.resolved_language_code.is_empty()) {
		resolved_language_code = request.resolved_language_code;
	}

	data["requested_language_id"] = request.language_id;
	data["requested_language_code"] = request.language_code;
	data["resolved_language_id"] =
			resolved_language_id.has_value() ? Variant(*resolved_language_id) : Variant(-1);
	data["resolved_language_code"] = resolved_language_code;
	data["selection_mode"] = request.selection_mode;
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
		const Dictionary &silence_map, const piper::Voice &voice,
		std::optional<std::map<piper::Phoneme, float>> &parsed_silence_map,
		String &error_message) {
	if (silence_map.is_empty()) {
		parsed_silence_map.reset();
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

	parsed_silence_map = std::move(parsed_map);
	return OK;
}

Error build_request_synthesis_config(
		const piper::Voice &voice, EffectiveRequest &effective_request,
		piper::SynthesisConfig &synthesis_config, String &error_code,
		String &error_message) {
	error_code = String();
	synthesis_config = voice.synthesisConfig;
	synthesis_config.lengthScale = CLAMP(effective_request.speech_rate, 0.1f, 5.0f);
	synthesis_config.noiseScale = CLAMP(effective_request.noise_scale, 0.0f, 2.0f);
	synthesis_config.noiseW = CLAMP(effective_request.noise_w, 0.0f, 2.0f);
	synthesis_config.sentenceSilenceSeconds =
			MAX(effective_request.sentence_silence_seconds, 0.0f);
	synthesis_config.speakerId =
			static_cast<piper::SpeakerId>(MAX(effective_request.speaker_id, 0));

	Error silence_error = apply_phoneme_silence_dictionary(
			effective_request.phoneme_silence_seconds, voice,
			synthesis_config.phonemeSilenceSeconds, error_message);
	if (silence_error != OK) {
		error_code = "ERR_REQUEST_INVALID";
		return silence_error;
	}

	String language_error_code;
	Error language_error = resolve_requested_language(
			voice, effective_request.language_id, effective_request.language_code,
			effective_request, language_error_code, error_message);
	if (language_error != OK) {
		if (language_error_code.is_empty()) {
			language_error_code = "ERR_REQUEST_INVALID";
		}
		error_code = language_error_code;
		effective_request.selection_mode = "";
		effective_request.resolved_language_code = String();
		effective_request.resolved_language_id = -1;
		return language_error;
	}
	synthesis_config.languageId = effective_request.resolved_language_id >= 0
			? std::make_optional(static_cast<piper::LanguageId>(effective_request.resolved_language_id))
			: std::nullopt;

	String text_language_error_code;
	Error text_language_error = validate_text_language_support(
			voice, synthesis_config, effective_request, text_language_error_code, error_message);
	if (text_language_error != OK) {
		error_code = text_language_error_code.is_empty() ? "ERR_REQUEST_INVALID" : text_language_error_code;
		return text_language_error;
	}

	if (!effective_request.has_text && effective_request.selection_mode.is_empty()) {
		effective_request.selection_mode = "auto";
	}

	return OK;
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
	clear_last_error(last_error_);
	{
		std::lock_guard<std::mutex> lock(pending_async_result_mutex_);
		pending_async_result_.reset();
	}

	if (model_path.is_empty()) {
		UtilityFunctions::push_error("PiperTTS: model_path is not set.");
		set_last_error(last_error_, "ERR_UNCONFIGURED", "PiperTTS: model_path is not set.",
				"initialize");
		emit_signal("initialized", false);
		return ERR_UNCONFIGURED;
	}

	// Resolve Godot resource paths to absolute OS paths
	String abs_model = resolve_model_path(model_path);
	if (abs_model.is_empty()) {
		UtilityFunctions::push_error(
				"PiperTTS: model_path could not be resolved to a valid .onnx model file.");
		set_last_error(last_error_, "ERR_CANT_OPEN",
				"PiperTTS: model_path could not be resolved to a valid .onnx model file.",
				"initialize");
		emit_signal("initialized", false);
		return ERR_CANT_OPEN;
	}
	String abs_config = resolve_config_path(abs_model);

	if (abs_config.is_empty()) {
		UtilityFunctions::push_error(
				"PiperTTS: config_path is not set and no fallback config was found next to the model.");
		set_last_error(last_error_, "ERR_UNCONFIGURED",
				"PiperTTS: config_path is not set and no fallback config was found next to the model.",
				"initialize");
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

	// Set OpenJTalk dictionary path if configured
	if (!dictionary_path.is_empty()) {
		String abs_dict = resolve_path(dictionary_path);
		std::string dict_str = abs_dict.utf8().get_data();
		openjtalk_set_dictionary_path(dict_str.c_str());
		UtilityFunctions::print(String("PiperTTS: OpenJTalk dictionary path set to: ") + abs_dict);
	} else {
		openjtalk_set_dictionary_path(nullptr);
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

#if defined(__EMSCRIPTEN__)
		if (ep != EP_CPU) {
			const String message =
					"PiperTTS: Web export supports only EP_CPU execution_provider.";
			UtilityFunctions::push_error(message);
			set_last_error(last_error_, "ERR_UNSUPPORTED_EXECUTION_PROVIDER",
					message, "initialize");
			piper::terminate(*piper_config);
			voice->customDictionary.reset();
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
			emit_signal("initialized", false);
			return ERR_UNAVAILABLE;
		}
#endif

		piper::loadVoice(*piper_config, model_str, config_str,
				*voice, sid, ep, gpu_device_id);

		String language_error;
		String language_error_code;
		EffectiveRequest language_request;
		language_request.language_id = language_id;
		language_request.language_code = language_code;
		if (resolve_requested_language(*voice, language_id, language_code, language_request,
					language_error_code, language_error) != OK) {
			UtilityFunctions::push_error(language_error);
			set_last_error(last_error_,
					language_error_code.is_empty() ? "ERR_INVALID_PARAMETER" : language_error_code,
					language_error, "initialize", language_request.language_code,
					language_request.resolved_language_code, language_request.language_id,
					language_request.resolved_language_id, language_request.selection_mode);
			piper::terminate(*piper_config);
			voice->customDictionary.reset();
			emit_signal("initialized", false);
			return ERR_INVALID_PARAMETER;
		}
		voice->synthesisConfig.languageId = language_request.resolved_language_id >= 0
				? std::make_optional(static_cast<piper::LanguageId>(language_request.resolved_language_id))
				: std::nullopt;

		ready = true;
		if (model_path != abs_model) {
			UtilityFunctions::print(String("PiperTTS: Model resolved to: ") + abs_model);
		}
		if (config_path.is_empty()) {
			UtilityFunctions::print(String("PiperTTS: Config auto-resolved to: ") + abs_config);
		}
		UtilityFunctions::print("PiperTTS: Voice loaded successfully.");
		clear_last_error(last_error_);
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

	String language_error_code;
	String language_error;
	piper::SynthesisConfig synthesis_config;
	if (build_request_synthesis_config(
				*voice, request, synthesis_config, language_error_code, language_error) != OK) {
		UtilityFunctions::push_error(language_error);
		set_last_error(last_error_,
				language_error_code.is_empty() ? "ERR_INVALID_PARAMETER" : language_error_code,
				language_error, "synthesize", request.language_code,
				request.resolved_language_code, request.language_id, request.resolved_language_id,
				request.selection_mode);
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
	last_synthesis_result_ = synthesis_result_to_dictionary(result, sample_rate);
	last_synthesis_result_["input_mode"] = "text";
	last_synthesis_result_["sentence_silence_seconds"] = synthesis_config.sentenceSilenceSeconds;
	last_synthesis_result_["phoneme_silence_seconds"] = phoneme_silence_seconds;
	annotate_language_metadata(last_synthesis_result_, request, synthesis_config, *voice);
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
	String error_message;
	if (!build_effective_request(*this, request_dictionary, request, error_message)) {
		UtilityFunctions::push_error(error_message);
		set_last_error(last_error_, "ERR_REQUEST_INVALID", error_message, "synthesize_request");
		return Ref<AudioStreamWAV>();
	}

	if ((request.has_text && request.text.is_empty()) ||
			(request.has_phoneme_string && request.phoneme_string.is_empty())) {
		UtilityFunctions::push_error("PiperTTS: Request input must not be empty.");
		set_last_error(last_error_, "ERR_INVALID_PARAMETER",
				"PiperTTS: Request input must not be empty.", "synthesize_request");
		return Ref<AudioStreamWAV>();
	}

	String error_code;
	piper::SynthesisConfig synthesis_config;
	if (build_request_synthesis_config(*voice, request, synthesis_config, error_code, error_message) != OK) {
		UtilityFunctions::push_error(error_message);
		set_last_error(last_error_,
				error_code.is_empty() ? "ERR_INVALID_PARAMETER" : error_code, error_message,
				"synthesize_request", request.language_code, request.resolved_language_code,
				request.language_id, request.resolved_language_id, request.selection_mode);
		return Ref<AudioStreamWAV>();
	}

	std::vector<int16_t> audio_buffer;
	piper::SynthesisResult result;

	try {
		if (request.has_phoneme_string) {
			std::vector<piper::Phoneme> phonemes =
					parse_effective_phoneme_string(*voice, request.phoneme_string);
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
	last_synthesis_result_ = synthesis_result_to_dictionary(result, sample_rate);
	last_synthesis_result_["input_mode"] = request.has_phoneme_string ? "phoneme_string" : "text";
	last_synthesis_result_["sentence_silence_seconds"] = synthesis_config.sentenceSilenceSeconds;
	last_synthesis_result_["phoneme_silence_seconds"] = request.phoneme_silence_seconds;
	annotate_language_metadata(last_synthesis_result_, request, synthesis_config, *voice);
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
	String error_message;
	if (!build_effective_request(*this, request_dictionary, request, error_message)) {
		UtilityFunctions::push_error(error_message);
		set_last_error(last_error_, "ERR_REQUEST_INVALID", error_message, "inspect_request");
		return empty_result;
	}

	if ((request.has_text && request.text.is_empty()) ||
			(request.has_phoneme_string && request.phoneme_string.is_empty())) {
		UtilityFunctions::push_error("PiperTTS: Request input must not be empty.");
		set_last_error(last_error_, "ERR_INVALID_PARAMETER",
				"PiperTTS: Request input must not be empty.", "inspect_request");
		return empty_result;
	}

	String error_code;
	piper::SynthesisConfig synthesis_config;
	if (build_request_synthesis_config(*voice, request, synthesis_config, error_code, error_message) != OK) {
		UtilityFunctions::push_error(error_message);
		set_last_error(last_error_,
				error_code.is_empty() ? "ERR_INVALID_PARAMETER" : error_code, error_message,
				"inspect_request", request.language_code, request.resolved_language_code,
				request.language_id, request.resolved_language_id, request.selection_mode);
		return empty_result;
	}

	piper::InspectionResult inspection_result;
	try {
		if (request.has_phoneme_string) {
			std::vector<piper::Phoneme> phonemes =
					parse_effective_phoneme_string(*voice, request.phoneme_string);
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

	last_inspection_result_ = inspection_result_to_dictionary(inspection_result, *voice);
	last_inspection_result_["input_mode"] = request.has_phoneme_string ? "phoneme_string" : "text";
	last_inspection_result_["sentence_silence_seconds"] = synthesis_config.sentenceSilenceSeconds;
	last_inspection_result_["phoneme_silence_seconds"] = request.phoneme_silence_seconds;
	annotate_language_metadata(last_inspection_result_, request, synthesis_config, *voice);
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
		set_last_error(const_cast<PiperTTS *>(this)->last_error_,
				"ERR_UNCONFIGURED",
				"PiperTTS: initialize() must succeed before querying language capabilities.",
				"get_language_capabilities");
		return Dictionary();
	}
	clear_last_error(const_cast<PiperTTS *>(this)->last_error_);
	return build_language_capabilities(*voice);
}

Dictionary PiperTTS::get_runtime_contract() const {
	Dictionary contract;
	const bool is_web_export = OS::get_singleton() && OS::get_singleton()->has_feature("web");

	contract["is_web_export"] = is_web_export;
	contract["execution_provider_policy"] = is_web_export ? "cpu_only" : "multi_provider";
	contract["supports_non_cpu_execution_provider"] = !is_web_export;
	contract["supports_openjtalk_native"] = !is_web_export;
	contract["supports_openjtalk_library_path"] = !is_web_export;
	contract["resource_path_mode"] = "globalize_path";
	contract["model_path"] = model_path;
	contract["config_path"] = config_path;
	contract["dictionary_path"] = dictionary_path;
	contract["openjtalk_library_path"] = openjtalk_library_path;
	contract["custom_dictionary_path"] = custom_dictionary_path;
	contract["execution_provider"] = execution_provider;
	return contract;
}

Dictionary PiperTTS::get_last_error() const {
	return last_error_;
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
	String error_message;
	if (!build_effective_request(*this, request_dictionary, request, error_message)) {
		UtilityFunctions::push_error(error_message);
		set_last_error(last_error_, "ERR_REQUEST_INVALID", error_message,
				"synthesize_async_request");
		return ERR_INVALID_PARAMETER;
	}

	if ((request.has_text && request.text.is_empty()) ||
			(request.has_phoneme_string && request.phoneme_string.is_empty())) {
		UtilityFunctions::push_error("PiperTTS: Request input must not be empty.");
		set_last_error(last_error_, "ERR_INVALID_PARAMETER",
				"PiperTTS: Request input must not be empty.", "synthesize_async_request");
		return ERR_INVALID_PARAMETER;
	}

	String language_error;
	String language_error_code;
	piper::SynthesisConfig synthesis_config;
	if (build_request_synthesis_config(
				*voice, request, synthesis_config, language_error_code, language_error) != OK) {
		UtilityFunctions::push_error(language_error);
		set_last_error(last_error_,
				language_error_code.is_empty() ? "ERR_INVALID_PARAMETER" : language_error_code,
				language_error, "synthesize_async_request", request.language_code,
				request.resolved_language_code, request.language_id, request.resolved_language_id,
				request.selection_mode);
		return ERR_INVALID_PARAMETER;
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
	String error_message;
	if (!build_effective_request(*this, request_dictionary, request, error_message)) {
		UtilityFunctions::push_error(error_message);
		set_last_error(last_error_, "ERR_REQUEST_INVALID", error_message,
				"synthesize_streaming_request");
		return ERR_INVALID_PARAMETER;
	}

	if ((request.has_text && request.text.is_empty()) ||
			(request.has_phoneme_string && request.phoneme_string.is_empty())) {
		UtilityFunctions::push_error("PiperTTS: Request input must not be empty.");
		set_last_error(last_error_, "ERR_INVALID_PARAMETER",
				"PiperTTS: Request input must not be empty.", "synthesize_streaming_request");
		return ERR_INVALID_PARAMETER;
	}

	String language_error = error_message;
	String language_error_code;
	piper::SynthesisConfig synthesis_config;
	if (build_request_synthesis_config(
				*voice, request, synthesis_config, language_error_code, language_error) != OK) {
		UtilityFunctions::push_error(language_error);
		set_last_error(last_error_,
				language_error_code.is_empty() ? "ERR_INVALID_PARAMETER" : language_error_code,
				language_error, "synthesize_streaming_request", request.language_code,
				request.resolved_language_code, request.language_id, request.resolved_language_id,
				request.selection_mode);
		return ERR_INVALID_PARAMETER;
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
						synthesis_result_to_dictionary(*pending_async_result_, sample_rate);
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
