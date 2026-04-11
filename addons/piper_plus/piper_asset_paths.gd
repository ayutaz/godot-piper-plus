@tool
extends RefCounted

const PROJECT_ASSET_ROOT := "res://piper_plus_assets"
const PROJECT_MODELS_ROOT := PROJECT_ASSET_ROOT + "/models"
const PROJECT_DICTIONARIES_ROOT := PROJECT_ASSET_ROOT + "/dictionaries"

const LEGACY_MODELS_ROOT := "res://addons/piper_plus/models"
const LEGACY_DICTIONARIES_ROOT := "res://addons/piper_plus/dictionaries"

const DEFAULT_CUSTOM_DICTIONARY_NAME := "custom_dictionary.json"
const OPENJTALK_DICTIONARY_DIRNAME := "open_jtalk_dic_utf_8-1.11"

static func project_asset_root() -> String:
	return PROJECT_ASSET_ROOT

static func project_models_root() -> String:
	return PROJECT_MODELS_ROOT

static func project_dictionaries_root() -> String:
	return PROJECT_DICTIONARIES_ROOT

static func legacy_models_root() -> String:
	return LEGACY_MODELS_ROOT

static func legacy_dictionaries_root() -> String:
	return LEGACY_DICTIONARIES_ROOT

static func default_custom_dictionary_path() -> String:
	return PROJECT_DICTIONARIES_ROOT.path_join(DEFAULT_CUSTOM_DICTIONARY_NAME)

static func openjtalk_dictionary_path() -> String:
	return PROJECT_DICTIONARIES_ROOT.path_join(OPENJTALK_DICTIONARY_DIRNAME)

static func legacy_openjtalk_dictionary_path() -> String:
	return LEGACY_DICTIONARIES_ROOT.path_join(OPENJTALK_DICTIONARY_DIRNAME)

static func ensure_trailing_slash(path: String) -> String:
	return path if path.ends_with("/") else path + "/"

static func project_destination(dest_subdir: String) -> String:
	var trimmed := dest_subdir.strip_edges().trim_prefix("/")
	var base := PROJECT_ASSET_ROOT
	if not trimmed.is_empty():
		base = base.path_join(trimmed)
	return ensure_trailing_slash(base)

static func legacy_destination_for_item(key: String, item_type: String) -> String:
	if item_type == "model":
		return ensure_trailing_slash(LEGACY_MODELS_ROOT.path_join(key))
	return ensure_trailing_slash(LEGACY_DICTIONARIES_ROOT)
