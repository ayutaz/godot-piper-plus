@tool
extends RefCounted

const MODEL_KEY := "multilingual-test-medium"
const ModelDescriptorScript = preload("res://addons/piper_plus/model_descriptor.gd")


static func get_descriptor_path() -> String:
	return ModelDescriptorScript.get_descriptor_path(MODEL_KEY)


static func get_descriptor() -> Dictionary:
	return ModelDescriptorScript.get_descriptor(MODEL_KEY)


static func get_catalog_name() -> String:
	return ModelDescriptorScript.get_catalog_name(MODEL_KEY)


static func get_model_key() -> String:
	return ModelDescriptorScript.get_model_key(MODEL_KEY)


static func get_default_language_code() -> String:
	return ModelDescriptorScript.get_default_language_code(MODEL_KEY)


static func get_asset_requirements() -> Dictionary:
	return ModelDescriptorScript.get_asset_requirements(MODEL_KEY)


static func get_language_items() -> Array[Dictionary]:
	return ModelDescriptorScript.get_language_items(MODEL_KEY)


static func list_language_codes() -> PackedStringArray:
	return ModelDescriptorScript.list_language_codes(MODEL_KEY)


static func resolve_language_code(language_code: String) -> String:
	return ModelDescriptorScript.resolve_language_code(MODEL_KEY, language_code)


static func get_language_item(language_code: String) -> Dictionary:
	return ModelDescriptorScript.get_language_item(MODEL_KEY, language_code)


static func get_language_display_name(language_code: String) -> String:
	return ModelDescriptorScript.get_language_display_name(MODEL_KEY, language_code)


static func get_language_template_text(language_code: String) -> String:
	return ModelDescriptorScript.get_language_template_text(MODEL_KEY, language_code)


static func get_language_placeholder_text(language_code: String) -> String:
	return ModelDescriptorScript.get_language_placeholder_text(MODEL_KEY, language_code)


static func get_language_options() -> Array[Dictionary]:
	return ModelDescriptorScript.get_language_options(MODEL_KEY)
