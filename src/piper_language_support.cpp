#include "piper_language_support.hpp"

#include <algorithm>
#include <vector>

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include "piper_core/multilingual_phonemize.hpp"

namespace godot {
namespace piper_language {

namespace {

std::string normalize_language_code(const String &code) {
	String normalized = code.strip_edges().to_lower();
	std::string utf8 = normalized.utf8().get_data();
	std::replace(utf8.begin(), utf8.end(), '_', '-');
	return utf8;
}

std::string base_language_code(const std::string &language_code) {
	const std::size_t separator = language_code.find('-');
	if (separator == std::string::npos || separator == 0) {
		return language_code;
	}
	return language_code.substr(0, separator);
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

Dictionary capability_entry_to_dictionary(
		const piper::MultilingualLanguageCapability &capability, int64_t language_id) {
	Dictionary entry;
	entry["language_code"] = String(capability.languageCode.c_str());
	entry["language_id"] = language_id;
	switch (capability.routingMode) {
		case piper::MultilingualTextRoutingMode::AutoDetect:
			entry["routing_mode"] = "auto";
			break;
		case piper::MultilingualTextRoutingMode::ExplicitOnly:
			entry["routing_mode"] = "explicit_only";
			break;
		case piper::MultilingualTextRoutingMode::Unsupported:
			entry["routing_mode"] = "phoneme_only";
			break;
	}
	entry["support_tier"] = String(capability.supportTier.c_str());
	entry["frontend_backend"] = String(capability.frontendBackend.c_str());
	entry["text_supported"] = capability.textPhonemizerAvailable;
	entry["auto_supported"] = capability.autoRouteAllowed;
	entry["phoneme_only"] = capability.phonemeOnly;
	return entry;
}

void annotate_resource_metadata(Dictionary &entry, const piper::Voice &voice,
		const Dictionary &runtime_contract) {
	const String language_code = entry.get("language_code", "");
	const bool text_supported = static_cast<bool>(entry.get("text_supported", false));
	PackedStringArray required_resources;
	bool resource_ready = text_supported;
	String resource_status = text_supported ? "not_required" : "phoneme_only";

	if (language_code == "ja") {
		required_resources.push_back("openjtalk_dictionary");
		resource_ready = runtime_contract.is_empty()
				? true
				: static_cast<bool>(runtime_contract.get(
							"supports_japanese_text_input", false));
		resource_status = resource_ready ? "ready" : "missing_required_resource";
	} else if (language_code == "en") {
		required_resources.push_back("cmudict_data.json");
		resource_ready = !voice.cmuDict.empty();
		resource_status = resource_ready ? "ready" : "missing_required_resource";
	} else if (language_code == "zh") {
		required_resources.push_back("pinyin_single.json");
		required_resources.push_back("pinyin_phrases.json");
		resource_ready =
				!voice.pinyinSingleDict.empty() && !voice.pinyinPhraseDict.empty();
		resource_status = resource_ready ? "ready" : "missing_required_resource";
	}

	entry["required_resources"] = required_resources;
	entry["resource_ready"] = resource_ready;
	entry["resource_status"] = resource_status;
}

void append_language_entry(Dictionary &capabilities, Array &languages,
		PackedStringArray &available_language_codes,
		PackedInt64Array &available_language_ids,
		PackedStringArray &auto_route_language_codes,
		PackedStringArray &explicit_only_language_codes,
		PackedStringArray &text_supported_language_codes,
		PackedStringArray &resource_ready_language_codes,
		PackedStringArray &resource_missing_language_codes,
		PackedStringArray &phoneme_only_language_codes,
		PackedStringArray &preview_language_codes,
		PackedStringArray &experimental_language_codes,
		const Dictionary &entry) {
	const String language_code = entry.get("language_code", "");
	const int64_t language_id = static_cast<int64_t>(entry.get("language_id", -1));
	const String routing_mode = entry.get("routing_mode", "");
	const String support_tier = entry.get("support_tier", "");
	const bool text_supported = static_cast<bool>(entry.get("text_supported", false));
	const bool auto_supported = static_cast<bool>(entry.get("auto_supported", false));
	const bool resource_ready = static_cast<bool>(entry.get("resource_ready", false));
	const String resource_status = entry.get("resource_status", "");

	languages.push_back(entry);
	if (!available_language_codes.has(language_code)) {
		available_language_codes.push_back(language_code);
	}
	bool has_id = false;
	for (int i = 0; i < available_language_ids.size(); ++i) {
		if (available_language_ids[i] == language_id) {
			has_id = true;
			break;
		}
	}
	if (!has_id && language_id >= 0) {
		available_language_ids.push_back(language_id);
	}
	if (auto_supported) {
		auto_route_language_codes.push_back(language_code);
	}
	if (routing_mode == "explicit_only") {
		explicit_only_language_codes.push_back(language_code);
	}
	if (text_supported) {
		text_supported_language_codes.push_back(language_code);
		if (resource_ready) {
			resource_ready_language_codes.push_back(language_code);
		} else if (resource_status == "missing_required_resource") {
			resource_missing_language_codes.push_back(language_code);
		}
	}
	if (static_cast<bool>(entry.get("phoneme_only", false))) {
		phoneme_only_language_codes.push_back(language_code);
	}
	if (support_tier == "preview") {
		preview_language_codes.push_back(language_code);
	}
	if (support_tier == "experimental") {
		experimental_language_codes.push_back(language_code);
	}
}

Dictionary build_single_language_entry(const char *language_code,
		int64_t language_id, const char *routing_mode, const char *support_tier,
		const char *frontend_backend, bool text_supported, bool auto_supported) {
	Dictionary entry;
	entry["language_code"] = language_code;
	entry["language_id"] = language_id;
	entry["routing_mode"] = routing_mode;
	entry["support_tier"] = support_tier;
	entry["frontend_backend"] = frontend_backend;
	entry["text_supported"] = text_supported;
	entry["auto_supported"] = auto_supported;
	entry["phoneme_only"] = !text_supported;
	return entry;
}

} // namespace

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

Dictionary build_language_capabilities(const piper::Voice &voice,
		const Dictionary &runtime_contract) {
	Dictionary capabilities;
	Array languages;
	PackedStringArray available_language_codes;
	PackedInt64Array available_language_ids;
	PackedStringArray auto_route_language_codes;
	PackedStringArray explicit_only_language_codes;
	PackedStringArray text_supported_language_codes;
	PackedStringArray resource_ready_language_codes;
	PackedStringArray resource_missing_language_codes;
	PackedStringArray phoneme_only_language_codes;
	PackedStringArray preview_language_codes;
	PackedStringArray experimental_language_codes;

	if (voice.modelConfig.languageIdMap && !voice.modelConfig.languageIdMap->empty()) {
		for (const auto &capability : piper::getMultilingualLanguageCapabilities(voice)) {
			const int64_t language_id =
					capability.languageId.has_value() ? *capability.languageId : -1;
			Dictionary entry =
					capability_entry_to_dictionary(capability, language_id);
			annotate_resource_metadata(entry, voice, runtime_contract);
			append_language_entry(capabilities, languages, available_language_codes,
					available_language_ids, auto_route_language_codes,
					explicit_only_language_codes, text_supported_language_codes,
					resource_ready_language_codes, resource_missing_language_codes,
					phoneme_only_language_codes, preview_language_codes,
					experimental_language_codes, entry);
		}
	} else if (voice.phonemizeConfig.phonemeType == piper::OpenJTalkPhonemes) {
		Dictionary entry = build_single_language_entry(
				"ja", 0, "auto", "preview", "openjtalk", true, true);
		annotate_resource_metadata(entry, voice, runtime_contract);
		append_language_entry(capabilities, languages, available_language_codes,
				available_language_ids, auto_route_language_codes,
				explicit_only_language_codes, text_supported_language_codes,
				resource_ready_language_codes, resource_missing_language_codes,
				phoneme_only_language_codes, preview_language_codes,
				experimental_language_codes, entry);
	} else if (voice.phonemizeConfig.phonemeType == piper::TextPhonemes) {
		Dictionary entry = build_single_language_entry(
				"en", 0, "auto", "preview", "cmu_dict", true, true);
		annotate_resource_metadata(entry, voice, runtime_contract);
		append_language_entry(capabilities, languages, available_language_codes,
				available_language_ids, auto_route_language_codes,
				explicit_only_language_codes, text_supported_language_codes,
				resource_ready_language_codes, resource_missing_language_codes,
				phoneme_only_language_codes, preview_language_codes,
				experimental_language_codes, entry);
	} else {
		Dictionary ja_entry = build_single_language_entry(
				"ja", 0, "auto", "preview", "openjtalk", true, true);
		annotate_resource_metadata(ja_entry, voice, runtime_contract);
		append_language_entry(capabilities, languages, available_language_codes,
				available_language_ids, auto_route_language_codes,
				explicit_only_language_codes, text_supported_language_codes,
				resource_ready_language_codes, resource_missing_language_codes,
				phoneme_only_language_codes, preview_language_codes,
				experimental_language_codes, ja_entry);
		Dictionary en_entry = build_single_language_entry(
				"en", 1, "auto", "preview", "cmu_dict", true, true);
		annotate_resource_metadata(en_entry, voice, runtime_contract);
		append_language_entry(capabilities, languages, available_language_codes,
				available_language_ids, auto_route_language_codes,
				explicit_only_language_codes, text_supported_language_codes,
				resource_ready_language_codes, resource_missing_language_codes,
				phoneme_only_language_codes, preview_language_codes,
				experimental_language_codes, en_entry);
	}

	String configured_language_code;
	if (voice.synthesisConfig.languageId.has_value()) {
		configured_language_code =
				language_code_from_id(voice, voice.synthesisConfig.languageId);
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
			voice.synthesisConfig.languageId.has_value()
					? Variant(*voice.synthesisConfig.languageId)
					: Variant(-1);
	capabilities["auto_route_language_codes"] = auto_route_language_codes;
	capabilities["explicit_only_language_codes"] = explicit_only_language_codes;
	capabilities["text_supported_language_codes"] = text_supported_language_codes;
	capabilities["resource_ready_language_codes"] = resource_ready_language_codes;
	capabilities["resource_missing_language_codes"] =
			resource_missing_language_codes;
	capabilities["phoneme_only_language_codes"] = phoneme_only_language_codes;
	capabilities["preview_language_codes"] = preview_language_codes;
	capabilities["experimental_language_codes"] = experimental_language_codes;
	capabilities["languages"] = languages;
	capabilities["supports_text_input"] = !text_supported_language_codes.is_empty();
	return capabilities;
}

Error resolve_requested_language(const piper::Voice &voice,
		int requested_language_id, const String &requested_language_code,
		piper_runtime::EffectiveRequest &effective_request,
		const String &stage, piper_runtime::RuntimeErrorInfo &error) {
	const std::string normalized_code =
			normalize_language_code(requested_language_code);
	if (!normalized_code.empty()) {
		if (!voice.modelConfig.languageIdMap || voice.modelConfig.languageIdMap->empty()) {
			piper_runtime::set_runtime_error(error, "ERR_LANGUAGE_CAPABILITY_MISSING",
					"PiperTTS: language_code was set, but the loaded model does not expose language_id_map.",
					stage, requested_language_code);
			return ERR_INVALID_PARAMETER;
		}

		const std::optional<LanguageCodeMatch> matched_language =
				resolve_language_code_match(voice, normalized_code);
		if (!matched_language.has_value()) {
			piper_runtime::set_runtime_error(error, "ERR_LANGUAGE_UNSUPPORTED_FOR_TEXT",
					String("PiperTTS: language_code '") +
							requested_language_code.strip_edges() +
							"' was not found in language_id_map.",
					stage, requested_language_code);
			return ERR_INVALID_PARAMETER;
		}

		if (requested_language_id >= 0 &&
				requested_language_id != static_cast<int>(matched_language->language_id)) {
			piper_runtime::set_runtime_error(error, "ERR_LANGUAGE_SELECTOR_CONFLICT",
					String("PiperTTS: language_code '") +
							requested_language_code.strip_edges() +
							"' conflicts with language_id=" +
							String::num_int64(requested_language_id) +
							" for the loaded model.",
					stage, requested_language_code, matched_language->canonical_code,
					requested_language_id, matched_language->language_id,
					matched_language->exact_match ? "language_code_exact" : "language_code_base");
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
			piper_runtime::set_runtime_error(error, "ERR_LANGUAGE_ID_OUT_OF_RANGE",
					String("PiperTTS: language_id is out of range for this model: ") +
							String::num_int64(requested_language_id),
					stage, String(), String(), requested_language_id);
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

Error validate_text_language_support(const piper::Voice &voice,
		const piper::SynthesisConfig &synthesis_config,
		const piper_runtime::EffectiveRequest &effective_request,
		const String &stage, piper_runtime::RuntimeErrorInfo &error) {
	if (!effective_request.has_text ||
			voice.phonemizeConfig.phonemeType != piper::MultilingualPhonemes) {
		return OK;
	}

	const std::string resolved_language_code = normalize_language_code(
			language_code_from_id(voice, synthesis_config.languageId));
	if (resolved_language_code.empty()) {
		return OK;
	}

	if (!piper::supportsMultilingualTextPhonemization(resolved_language_code)) {
		piper_runtime::set_runtime_error(error, "ERR_LANGUAGE_UNSUPPORTED_FOR_TEXT",
				String("PiperTTS: ") +
						String::utf8(
								piper::getMultilingualTextSupportError(
										resolved_language_code)
										.c_str()),
				stage, effective_request.language_code,
				effective_request.resolved_language_code,
				effective_request.language_id,
				effective_request.resolved_language_id,
				effective_request.selection_mode);
		return ERR_INVALID_PARAMETER;
	}

	return OK;
}

void annotate_language_metadata(Dictionary &data,
		const piper_runtime::EffectiveRequest &request,
		const piper::SynthesisConfig &synthesis_config,
		const piper::Voice &voice) {
	std::optional<piper::LanguageId> resolved_language_id =
			synthesis_config.languageId;
	if (!resolved_language_id.has_value() && request.resolved_language_id >= 0) {
		resolved_language_id =
				static_cast<piper::LanguageId>(request.resolved_language_id);
	}

	String resolved_language_code =
			language_code_from_id(voice, resolved_language_id);
	if (resolved_language_code.is_empty() &&
			!request.resolved_language_code.is_empty()) {
		resolved_language_code = request.resolved_language_code;
	}

	data["requested_language_id"] = request.language_id;
	data["requested_language_code"] = request.language_code;
	data["resolved_language_id"] =
			resolved_language_id.has_value() ? Variant(*resolved_language_id)
											 : Variant(-1);
	data["resolved_language_code"] = resolved_language_code;
	data["selection_mode"] = request.selection_mode;
}

} // namespace piper_language
} // namespace godot
