extends Control

const CSS10_MODEL_PATH := "res://addons/piper_plus/models/css10/css10-ja-6lang-fp16.onnx"
const CSS10_CONFIG_PATH := "res://addons/piper_plus/models/css10/config.json"
const OPENJTALK_DICT_PATH := "res://addons/piper_plus/dictionaries/open_jtalk_dic_utf_8-1.11"
const DEFAULT_JA_TEXT := "こんにちは、世界"
const DEFAULT_EN_TEXT := "Hello from Piper Plus."

@onready var tts: PiperTTS = $PiperTTS
@onready var audio_player: AudioStreamPlayer = $AudioStreamPlayer
@onready var status_label: Label = $StatusLabel

var _text_to_say := DEFAULT_EN_TEXT
var _model_kind := ""

func _ready() -> void:
	tts.initialized.connect(_on_tts_initialized)
	tts.synthesis_failed.connect(_on_synthesis_failed)
	call_deferred("_begin")

func _begin() -> void:
	_configure_demo_assets()
	if tts.model_path.is_empty():
		status_label.text = "no_model"
		return
	status_label.text = "initializing:%s" % _model_kind
	var err := tts.initialize()
	if err != OK:
		status_label.text = "init_error:%d" % err

func _resource_dir_exists(path: String) -> bool:
	return DirAccess.dir_exists_absolute(ProjectSettings.globalize_path(path))

func _configure_demo_assets() -> void:
	if FileAccess.file_exists(CSS10_MODEL_PATH) and FileAccess.file_exists(CSS10_CONFIG_PATH) and _resource_dir_exists(OPENJTALK_DICT_PATH):
		tts.model_path = CSS10_MODEL_PATH
		tts.config_path = CSS10_CONFIG_PATH
		tts.dictionary_path = OPENJTALK_DICT_PATH
		tts.language_code = "ja"
		_text_to_say = "%s。%s。%s。" % [DEFAULT_JA_TEXT, DEFAULT_JA_TEXT, DEFAULT_JA_TEXT]
		_model_kind = "css10_ja"
		status_label.text = "configured:%s" % _model_kind
		return

	if FileAccess.file_exists(CSS10_MODEL_PATH) and FileAccess.file_exists(CSS10_CONFIG_PATH):
		tts.model_path = CSS10_MODEL_PATH
		tts.config_path = CSS10_CONFIG_PATH
		tts.dictionary_path = ""
		tts.language_code = "en"
		_text_to_say = "%s %s %s %s" % [DEFAULT_EN_TEXT, DEFAULT_EN_TEXT, DEFAULT_EN_TEXT, DEFAULT_EN_TEXT]
		_model_kind = "css10_en"
		status_label.text = "configured:%s" % _model_kind
		return

	tts.model_path = ""
	tts.config_path = ""
	tts.dictionary_path = ""
	tts.language_code = ""
	_model_kind = "none"
	status_label.text = "configured:none"

func _on_tts_initialized(success: bool) -> void:
	if not success:
		status_label.text = "init_failed"
		return
	status_label.text = "ready:%s" % _model_kind
	call_deferred("_speak")

func _speak() -> void:
	var audio := tts.synthesize(_text_to_say)
	if audio == null:
		status_label.text = "synth_failed"
		return
	audio_player.stream = audio
	audio_player.play()
	status_label.text = "playing:%s" % _model_kind

func _on_synthesis_failed(error: String) -> void:
	status_label.text = "synth_failed:%s" % error
