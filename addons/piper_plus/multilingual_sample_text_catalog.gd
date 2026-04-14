@tool
extends RefCounted

const CATALOG_PATH := "res://addons/piper_plus/multilingual_sample_text_catalog.json"

static var _cached_catalog: Dictionary = {}

static func _load_catalog() -> Dictionary:
	if not _cached_catalog.is_empty():
		return _cached_catalog

	var text := FileAccess.get_file_as_string(CATALOG_PATH)
	if text.is_empty():
		return {}

	var parsed = JSON.parse_string(text)
	if typeof(parsed) != TYPE_DICTIONARY:
		return {}

	_cached_catalog = Dictionary(parsed).duplicate(true)
	return _cached_catalog

static func get_catalog_name() -> String:
	return String(_load_catalog().get("catalog_name", "multilingual-sample-text-catalog"))

static func get_model_key() -> String:
	return String(_load_catalog().get("model_key", "multilingual-test-medium"))

static func get_default_language_code() -> String:
	return String(_load_catalog().get("default_language_code", "ja"))

static func _languages() -> Array:
	var catalog := _load_catalog()
	var languages_variant: Variant = catalog.get("languages", [])
	if typeof(languages_variant) != TYPE_ARRAY:
		return []
	return languages_variant

static func get_language_items() -> Array[Dictionary]:
	var items: Array[Dictionary] = []
	for language in _languages():
		if typeof(language) != TYPE_DICTIONARY:
			continue
		items.append(Dictionary(language).duplicate(true))
	return items

static func list_language_codes() -> PackedStringArray:
	var codes := PackedStringArray()
	for language in get_language_items():
		codes.append(String(language.get("language_code", "")))
	return codes

static func resolve_language_code(language_code: String) -> String:
	var normalized := language_code.strip_edges().to_lower().replace("_", "-")
	if normalized.is_empty():
		return get_default_language_code()

	for item in get_language_items():
		var code := String(item.get("language_code", "")).to_lower()
		if code == normalized:
			return code

	var base_code := normalized.split("-", false, 1)[0]
	for item in get_language_items():
		var code := String(item.get("language_code", "")).to_lower()
		if code == base_code:
			return code

	return get_default_language_code()

static func get_language_item(language_code: String) -> Dictionary:
	var canonical := resolve_language_code(language_code)
	for item in get_language_items():
		if String(item.get("language_code", "")) == canonical:
			return item
	return {}

static func get_language_display_name(language_code: String) -> String:
	var item := get_language_item(language_code)
	if item.is_empty():
		return resolve_language_code(language_code).to_upper()
	return String(item.get("display_name", resolve_language_code(language_code).to_upper()))

static func get_language_template_text(language_code: String) -> String:
	return String(get_language_item(language_code).get("template_text", ""))

static func get_language_placeholder_text(language_code: String) -> String:
	return String(get_language_item(language_code).get("placeholder_text", ""))

static func get_language_options() -> Array[Dictionary]:
	var options: Array[Dictionary] = []
	for item in get_language_items():
		options.append({
			"language_code": String(item.get("language_code", "")),
			"display_name": String(item.get("display_name", "")),
			"template_text": String(item.get("template_text", "")),
			"placeholder_text": String(item.get("placeholder_text", "")),
		})
	return options
