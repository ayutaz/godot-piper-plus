#ifndef PIPER_LANGUAGE_SUPPORT_H
#define PIPER_LANGUAGE_SUPPORT_H

#include <optional>

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include "piper_core/piper.hpp"
#include "piper_runtime_support.hpp"

namespace godot {
namespace piper_language {

Dictionary build_language_capabilities(const piper::Voice &voice);
String language_code_from_id(const piper::Voice &voice,
		const std::optional<piper::LanguageId> &language_id);

Error resolve_requested_language(const piper::Voice &voice,
		int requested_language_id, const String &requested_language_code,
		piper_runtime::EffectiveRequest &effective_request,
		const String &stage, piper_runtime::RuntimeErrorInfo &error);

Error validate_text_language_support(const piper::Voice &voice,
		const piper::SynthesisConfig &synthesis_config,
		const piper_runtime::EffectiveRequest &effective_request,
		const String &stage, piper_runtime::RuntimeErrorInfo &error);

void annotate_language_metadata(Dictionary &data,
		const piper_runtime::EffectiveRequest &request,
		const piper::SynthesisConfig &synthesis_config,
		const piper::Voice &voice);

} // namespace piper_language
} // namespace godot

#endif // PIPER_LANGUAGE_SUPPORT_H
