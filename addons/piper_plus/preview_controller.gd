@tool
extends RefCounted

const CONTROLLED_PROPERTIES := [
	"model_path",
	"config_path",
	"dictionary_path",
	"openjtalk_library_path",
	"custom_dictionary_path",
	"speaker_id",
	"language_id",
	"language_code",
	"speech_rate",
	"noise_scale",
	"noise_w",
	"sentence_silence_seconds",
	"phoneme_silence_seconds",
	"execution_provider",
	"gpu_device_id",
]

static func build_session_config(target: Object) -> Dictionary:
	var config := {}
	if target == null:
		return config

	for property_name in CONTROLLED_PROPERTIES:
		config[property_name] = target.get(property_name)
	return config

static func apply_session_config(preview_tts: Object, config: Dictionary) -> void:
	if preview_tts == null:
		return

	for property_name in CONTROLLED_PROPERTIES:
		if config.has(property_name):
			preview_tts.set(property_name, config[property_name])

static func create_preview_session(owner: Node, target: Object) -> Dictionary:
	var result := {
		"tts": null,
		"player": null,
		"config": {},
		"error": "",
	}

	var preview_tts = ClassDB.instantiate("PiperTTS")
	if preview_tts == null:
		result["error"] = "PiperTTS クラスを生成できませんでした。GDExtension の読み込み状態を確認してください。"
		return result

	var player := AudioStreamPlayer.new()
	owner.add_child(preview_tts)
	owner.add_child(player)

	var config := build_session_config(target)
	apply_session_config(preview_tts, config)

	result["tts"] = preview_tts
	result["player"] = player
	result["config"] = config
	return result
