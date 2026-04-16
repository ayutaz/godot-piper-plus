@tool
extends RefCounted

const DESCRIPTOR_DIR := "res://addons/piper_plus/model_descriptors"

static var _cached_descriptors: Dictionary = {}


static func get_descriptor_path(model_key: String) -> String:
	var canonical_model_key := model_key.strip_edges()
	if canonical_model_key.is_empty():
		return ""
	return DESCRIPTOR_DIR.path_join("%s.json" % canonical_model_key)


static func _load_descriptor(model_key: String) -> Dictionary:
	var canonical_model_key := model_key.strip_edges()
	if canonical_model_key.is_empty():
		return {}

	if _cached_descriptors.has(canonical_model_key):
		var cached = _cached_descriptors[canonical_model_key]
		if typeof(cached) == TYPE_DICTIONARY:
			return cached

	var descriptor_path := get_descriptor_path(canonical_model_key)
	if descriptor_path.is_empty() or not FileAccess.file_exists(descriptor_path):
		return {}

	var text := FileAccess.get_file_as_string(descriptor_path)
	if text.is_empty():
		return {}

	var parsed = JSON.parse_string(text)
	if typeof(parsed) != TYPE_DICTIONARY:
		return {}

	var descriptor := Dictionary(parsed).duplicate(true)
	_cached_descriptors[canonical_model_key] = descriptor
	return descriptor


static func get_descriptor(model_key: String) -> Dictionary:
	return _load_descriptor(model_key)


static func get_model_key(model_key: String) -> String:
	return String(_load_descriptor(model_key).get("model_key", model_key.strip_edges()))


static func get_catalog_name(model_key: String) -> String:
	return String(_load_descriptor(model_key).get("catalog_name", "multilingual-sample-text-catalog"))


static func get_default_language_code(model_key: String) -> String:
	return String(_load_descriptor(model_key).get("default_language_code", "ja"))


static func get_auto_route_language_code(model_key: String) -> String:
	return String(_load_descriptor(model_key).get("auto_route_language_code", "en"))


static func _languages(model_key: String) -> Array:
	var descriptor := _load_descriptor(model_key)
	var languages_variant: Variant = descriptor.get("languages", [])
	if typeof(languages_variant) != TYPE_ARRAY:
		return []
	return languages_variant


static func get_language_items(model_key: String) -> Array[Dictionary]:
	var items: Array[Dictionary] = []
	for language in _languages(model_key):
		if typeof(language) != TYPE_DICTIONARY:
			continue
		items.append(Dictionary(language).duplicate(true))
	return items


static func list_language_codes(model_key: String) -> PackedStringArray:
	var codes := PackedStringArray()
	for language in get_language_items(model_key):
		codes.append(String(language.get("language_code", "")))
	return codes


static func resolve_language_code(model_key: String, language_code: String) -> String:
	var default_language_code := get_default_language_code(model_key)
	var normalized := language_code.strip_edges().to_lower().replace("_", "-")
	if normalized.is_empty():
		return default_language_code

	for item in get_language_items(model_key):
		var code := String(item.get("language_code", "")).to_lower()
		if code == normalized:
			return code
		var aliases_variant: Variant = item.get("aliases", [])
		if typeof(aliases_variant) == TYPE_ARRAY:
			for alias in aliases_variant:
				var normalized_alias := String(alias).strip_edges().to_lower().replace("_", "-")
				if normalized_alias == normalized:
					return code

	var base_code := normalized.split("-", false, 1)[0]
	for item in get_language_items(model_key):
		var code := String(item.get("language_code", "")).to_lower()
		if code == base_code:
			return code
		var aliases_variant: Variant = item.get("aliases", [])
		if typeof(aliases_variant) == TYPE_ARRAY:
			for alias in aliases_variant:
				var normalized_alias := String(alias).strip_edges().to_lower().replace("_", "-")
				if normalized_alias == base_code:
					return code

	return default_language_code


static func get_language_item(model_key: String, language_code: String) -> Dictionary:
	var canonical := resolve_language_code(model_key, language_code)
	for item in get_language_items(model_key):
		if String(item.get("language_code", "")) == canonical:
			return item
	return {}


static func get_language_display_name(model_key: String, language_code: String) -> String:
	var item := get_language_item(model_key, language_code)
	if item.is_empty():
		return resolve_language_code(model_key, language_code).to_upper()
	return String(item.get("display_name", resolve_language_code(model_key, language_code).to_upper()))


static func get_language_template_text(model_key: String, language_code: String) -> String:
	return String(get_language_item(model_key, language_code).get("template_text", ""))


static func get_language_placeholder_text(model_key: String, language_code: String) -> String:
	return String(get_language_item(model_key, language_code).get("placeholder_text", ""))


static func get_language_options(model_key: String) -> Array[Dictionary]:
	var options: Array[Dictionary] = []
	for item in get_language_items(model_key):
		options.append({
			"language_code": String(item.get("language_code", "")),
			"display_name": String(item.get("display_name", "")),
			"template_text": String(item.get("template_text", "")),
			"placeholder_text": String(item.get("placeholder_text", "")),
		})
	return options


static func get_asset_requirements(model_key: String) -> Dictionary:
	var descriptor := _load_descriptor(model_key)
	var requirements: Variant = descriptor.get("asset_requirements", {})
	if typeof(requirements) != TYPE_DICTIONARY:
		return {}
	return Dictionary(requirements).duplicate(true)
