extends Control

const MODEL_PATH := "res://test/models/multilingual-test-medium.onnx"
const CONFIG_PATH := "res://test/models/multilingual-test-medium.onnx.json"
const DICT_PATH := "res://addons/piper_plus/dictionaries/open_jtalk_dic_utf_8-1.11"
const TEXT_TO_SAY := "こんにちは、世界"

@onready var tts: PiperTTS = $PiperTTS
@onready var audio_player: AudioStreamPlayer = $AudioStreamPlayer
@onready var status_label: Label = $StatusLabel

func _ready() -> void:
	tts.initialized.connect(_on_tts_initialized)
	tts.synthesis_failed.connect(_on_synthesis_failed)
	call_deferred("_begin")

func _begin() -> void:
	status_label.text = "dict_exists:%s" % [DirAccess.dir_exists_absolute(ProjectSettings.globalize_path(DICT_PATH))]
	tts.model_path = MODEL_PATH
	tts.config_path = CONFIG_PATH
	tts.dictionary_path = DICT_PATH
	tts.language_code = "ja"
	var err := tts.initialize()
	if err != OK:
		status_label.text = "init_error:%d" % err

func _on_tts_initialized(success: bool) -> void:
	if not success:
		status_label.text = "init_failed"
		return
	var audio := tts.synthesize(TEXT_TO_SAY)
	if audio == null:
		status_label.text = "synth_failed"
		return
	audio_player.stream = audio
	audio_player.play()
	status_label.text = "playing"

func _on_synthesis_failed(error: String) -> void:
	status_label.text = "synth_failed:%s" % error
