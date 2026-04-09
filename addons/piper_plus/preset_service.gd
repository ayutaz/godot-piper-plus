@tool
extends RefCounted

const DownloadCatalog = preload("res://addons/piper_plus/download_catalog.gd")

static func list_model_presets() -> PackedStringArray:
	return DownloadCatalog.list_model_item_keys()

static func match_model_path_to_preset(current_model_path: String) -> String:
	var current_value := current_model_path.strip_edges()
	if current_value.is_empty():
		return ""

	var current_stem := current_value.get_file()
	if current_stem.ends_with(".onnx"):
		current_stem = current_stem.trim_suffix(".onnx")

	for key in list_model_presets():
		var model_path := DownloadCatalog.get_primary_model_path(key)
		var model_stem := model_path.get_file()
		if model_stem.ends_with(".onnx"):
			model_stem = model_stem.trim_suffix(".onnx")
		if current_value == key or current_value == model_path or current_stem == model_stem:
			return key
	return ""

static func resolve_preset_application(key: String) -> Dictionary:
	var model_value := key
	var installed_model_path := DownloadCatalog.find_installed_primary_model_path(key)
	if not installed_model_path.is_empty():
		model_value = installed_model_path

	return {
		"model_path": model_value,
		"config_path": "",
		"dictionary_path": DownloadCatalog.get_recommended_dictionary_path(key),
		"language_id": -1,
		"language_code": "",
	}
