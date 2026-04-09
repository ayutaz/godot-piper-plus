@tool
extends RefCounted

const PiperAssetPaths = preload("res://addons/piper_plus/piper_asset_paths.gd")
const CATALOG_PATH := "res://addons/piper_plus/download_catalog.json"

static var _cached_items: Dictionary = {}

static func _load_items() -> Dictionary:
	if not _cached_items.is_empty():
		return _cached_items

	var text := FileAccess.get_file_as_string(CATALOG_PATH)
	if text.is_empty():
		return {}

	var parsed = JSON.parse_string(text)
	if typeof(parsed) != TYPE_DICTIONARY:
		return {}

	var items := Dictionary(parsed.get("items", {}))
	_cached_items = items.duplicate(true)
	return _cached_items

static func list_item_keys() -> PackedStringArray:
	var keys := PackedStringArray()
	for key in _load_items().keys():
		keys.append(String(key))
	return keys

static func list_model_item_keys() -> PackedStringArray:
	var keys := PackedStringArray()
	for key in list_item_keys():
		if String(get_item_definition(key).get("type", "")) == "model":
			keys.append(key)
	return keys

static func get_item_definition(key: String) -> Dictionary:
	var items := _load_items()
	if not items.has(key):
		return {}

	var definition := Dictionary(items[key]).duplicate(true)
	var item_type := String(definition.get("type", ""))
	var dest_subdir := String(definition.get("dest_subdir", ""))
	definition["key"] = key
	definition["dest"] = PiperAssetPaths.project_destination(dest_subdir)
	definition["legacy_dest"] = PiperAssetPaths.legacy_destination_for_item(key, item_type)
	return definition

static func _primary_model_filename(item: Dictionary) -> String:
	var files: Array = item.get("files", [])
	for file_entry: Dictionary in files:
		var filename := String(file_entry.get("filename", ""))
		if filename.ends_with(".onnx"):
			return filename
	return ""

static func get_canonical_model_path(key: String) -> String:
	var item := get_item_definition(key)
	if item.is_empty() or String(item.get("type", "")) != "model":
		return ""

	var filename := _primary_model_filename(item)
	if filename.is_empty():
		return ""
	return String(item.get("dest", "")) + filename

static func find_installed_primary_model_path(key: String) -> String:
	var item := get_item_definition(key)
	if item.is_empty() or String(item.get("type", "")) != "model":
		return ""

	var filename := _primary_model_filename(item)
	if filename.is_empty():
		return ""

	var canonical := String(item.get("dest", "")) + filename
	if FileAccess.file_exists(canonical):
		return canonical

	var legacy := String(item.get("legacy_dest", "")) + filename
	if FileAccess.file_exists(legacy):
		return legacy

	return ""

static func get_primary_model_path(key: String) -> String:
	var installed := find_installed_primary_model_path(key)
	if not installed.is_empty():
		return installed
	return get_canonical_model_path(key)

static func _has_compiled_openjtalk_dictionary(path: String) -> bool:
	var absolute_path := ProjectSettings.globalize_path(path)
	if not DirAccess.dir_exists_absolute(absolute_path):
		return false

	for required_file in ["sys.dic", "unk.dic", "matrix.bin", "char.bin"]:
		if not FileAccess.file_exists(absolute_path.path_join(required_file)):
			return false

	return true

static func get_recommended_dictionary_path(key: String) -> String:
	var item := get_item_definition(key)
	if item.is_empty():
		return ""

	var dictionary_key := String(item.get("recommended_dictionary_key", ""))
	if dictionary_key.is_empty():
		return ""

	if dictionary_key == "naist-jdic":
		var canonical := PiperAssetPaths.openjtalk_dictionary_path()
		if _has_compiled_openjtalk_dictionary(canonical):
			return canonical
		var legacy := PiperAssetPaths.legacy_openjtalk_dictionary_path()
		if _has_compiled_openjtalk_dictionary(legacy):
			return legacy
	return ""

static func is_item_installed(key: String) -> bool:
	var item := get_item_definition(key)
	if item.is_empty():
		return false

	if String(item.get("type", "")) == "dictionary":
		var install_directory := String(item.get("install_directory", ""))
		if install_directory.is_empty():
			return false
		var canonical := String(item.get("dest", "")).path_join(install_directory)
		if _has_compiled_openjtalk_dictionary(canonical):
			return true
		var legacy := String(item.get("legacy_dest", "")).path_join(install_directory)
		return _has_compiled_openjtalk_dictionary(legacy)

	for file_entry: Dictionary in item.get("files", []):
		if bool(file_entry.get("extract", false)):
			continue
		var filename := String(file_entry.get("filename", ""))
		var canonical := String(item.get("dest", "")) + filename
		var legacy := String(item.get("legacy_dest", "")) + filename
		if not FileAccess.file_exists(canonical) and not FileAccess.file_exists(legacy):
			return false
	return true
